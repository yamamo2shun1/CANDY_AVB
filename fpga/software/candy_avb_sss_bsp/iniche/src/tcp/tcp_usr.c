/*
 * FILENAME: tcp_usr.c
 *
 * Copyright 1997-2007 By InterNiche Technologies, Inc. All rights reserved.
 *
 *
 * MODULE: INET
 *
 * ROUTINES: tcp_usrreq(), tcp_attach(), tcp_disconnect(), 
 * ROUTINES: tcp_usrclosed(),
 *
 * PORTABLE: yes
 */

/* Additional Copyrights: */
/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. [rescinded 22 July 1999]
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "ipport.h"
#include "tcpport.h"

#ifdef INCLUDE_TCP  /* include/exclude whole file at compile time */

#include "protosw.h"
#include "in_pcb.h"
#include "ip.h"
#include "tcp.h"
#include "tcp_fsm.h"
#include "tcp_seq.h"
#include "tcp_timr.h"
#include "tcp_var.h"
#include "tcpip.h"

/*
 * TCP protocol interface to socket abstraction.
 */
extern   char *   tcpstates[];
struct   tcpcb *  tcp_newtcpcb   __P ((struct inpcb *));
extern   int   tcp_attach  __P ((struct socket *));
extern   struct tcpcb * tcp_disconnect __P ((struct tcpcb *));
extern   struct tcpcb * tcp_usrclosed  __P ((struct tcpcb *));
extern   struct tcpcb * tcp_timers  __P ((struct tcpcb *, int));



/* FUNCTION: tcp_usrreq()
 *
 * Process a TCP user request for TCP tb.  If this is a send request
 * then m is the mbuf chain of send data.  If this is a timer expiration
 * (called from the software clock routine), then timertype tells which timer.
 * 
 * PARAM1: struct socket *so
 * PARAM2: struct mbuf *m
 * PARAM3: struct mbuf *nam
 *
 * RETURNS: 0 if OK, or nonzero socket error code.
 */

int
tcp_usrreq(struct socket * so, 
   struct mbuf *  m,
   struct mbuf *  nam)
{
   struct inpcb * inp;
   struct tcpcb * tp;
   int   error =  0;
   int   req;

#ifdef DO_TCPTRACE
   int   ostate;
#endif

   req = so->so_req;    /* get request from socket struct */
   inp = sotoinpcb(so);
   /*
    * When a TCP is attached to a socket, then there will be
    * a (struct inpcb) pointed at by the socket, and this
    * structure will point at a subsidary (struct tcpcb).
    */
   if (inp == 0 && req != PRU_ATTACH) 
   {
      return (EINVAL);
   }

   if (inp)
      tp = intotcpcb(inp);
   else  /* inp and tp not set, make sure this is OK: */
   { 
      if (req == PRU_ATTACH)
         tp = NULL;  /* stifle compiler warnings about using unassigned tp*/
      else
      {
         dtrap(); /* programming error? */
         return EINVAL;
      }
   }

   switch (req) 
   {
   /*
    * TCP attaches to socket via PRU_ATTACH, reserving space,
    * and an internet control block.
    */
   case PRU_ATTACH:
      if (inp) 
      {
         error = EISCONN;
         break;
      }
      error = tcp_attach(so);
      if (error)
         break;
      if ((so->so_options & SO_LINGER) && so->so_linger == 0)
         so->so_linger = TCP_LINGERTIME;
#ifdef   DO_TCPTRACE
      SETTP(tp, sototcpcb(so));
#endif
      break;

   /*
    * PRU_DETACH detaches the TCP protocol from the socket.
    * If the protocol state is non-embryonic, then can't
    * do this directly: have to initiate a PRU_DISCONNECT,
    * which may finish later; embryonic TCB's can just
    * be discarded here.
    */
   case PRU_DETACH:
      if (tp->t_state > TCPS_LISTEN)
         SETTP(tp, tcp_disconnect(tp));
      else
         SETTP(tp, tcp_close(tp));
      break;

   /*
    * Give the socket an address.
    */
   case PRU_BIND:

      /* bind is quite different for IPv4 and v6, so we use two 
       * seperate pcbbind routines. so_domain was checked for 
       * validity way up in t_bind()
       */
#ifdef IP_V4
      if(inp->inp_socket->so_domain == AF_INET)
      {
         error = in_pcbbind(inp, nam);
         break;
      }
#endif /* IP_V4 */
#ifdef IP_V6
      if(inp->inp_socket->so_domain == AF_INET6)
      {
         error = ip6_pcbbind(inp, nam);
         break;
      }
#endif /* IP_V6 */
      dtrap();    /* not v4 or v6? */
      error = EINVAL;
      break;
   /*
    * Prepare to accept connections.
    */
   case PRU_LISTEN:
      if (inp->inp_lport == 0)
         error = in_pcbbind(inp, (struct mbuf *)0);
      if (error == 0)
         tp->t_state = TCPS_LISTEN;
      break;

   /*
    * Initiate connection to peer.
    * Create a template for use in transmissions on this connection.
    * Enter SYN_SENT state, and mark socket as connecting.
    * Start keep-alive timer, and seed output sequence space.
    * Send initial segment on connection.
    */
   case PRU_CONNECT:
      if (inp->inp_lport == 0) 
      {

#ifdef IP_V4
#ifndef IP_V6  /* v4 only */
      error = in_pcbbind(inp, (struct mbuf *)0);
#else    /* dual mode */
      if(so->so_domain == AF_INET)
         error = in_pcbbind(inp, (struct mbuf *)0);
      else
         error = ip6_pcbbind(inp, (struct mbuf *)0);
#endif   /* end dual mode code */
#else    /* no v4, v6 only */
      error = ip6_pcbbind(inp, (struct mbuf *)0);
#endif   /* end v6 only */

         if (error)
            break;
      }

#ifdef IP_V4
#ifndef IP_V6  /* v4 only */
      error = in_pcbconnect(inp, nam);
#else    /* dual mode */
      if(so->so_domain == AF_INET)
         error = in_pcbconnect(inp, nam);
      else
         error = ip6_pcbconnect(inp, nam);
#endif   /* end dual mode code */
#else    /* no v4, v6 only */
      error = ip6_pcbconnect(inp, nam);
#endif   /* end v6 only */

      if (error)
         break;
      tp->t_template = tcp_template(tp);
      if (tp->t_template == 0) 
      {

#ifdef IP_V4
#ifndef IP_V6  /* v4 only */
         in_pcbdisconnect(inp);
#else    /* dual mode */
         if(so->so_domain == AF_INET)
            in_pcbdisconnect(inp);
         else
            ip6_pcbdisconnect(inp);
#endif   /* end dual mode code */
#else    /* no v4, v6 only */
         ip6_pcbdisconnect(inp);
#endif   /* end v6 only */

         error = ENOBUFS;
         break;
      }

      soisconnecting(so);
      tcpstat.tcps_connattempt++;
      tp->t_state = TCPS_SYN_SENT;
      tp->t_timer[TCPT_KEEP] = TCPTV_KEEP_INIT;
      tp->iss = tcp_iss; 
      tcp_iss += (tcp_seq)(TCP_ISSINCR/2);
      tcp_sendseqinit(tp);
      error = tcp_output(tp);
      if (!error)
         TCP_MIB_INC(tcpActiveOpens);     /* keep MIB stats */
      break;

   /*
    * Create a TCP connection between two sockets.
    */
   case PRU_CONNECT2:
      error = EOPNOTSUPP;
      break;

   /*
    * Initiate disconnect from peer.
    * If connection never passed embryonic stage, just drop;
    * else if don't need to let data drain, then can just drop anyways,
    * else have to begin TCP shutdown process: mark socket disconnecting,
    * drain unread data, state switch to reflect user close, and
    * send segment (e.g. FIN) to peer.  Socket will be really disconnected
    * when peer sends FIN and acks ours.
    *
    * SHOULD IMPLEMENT LATER PRU_CONNECT VIA REALLOC TCPCB.
    */
   case PRU_DISCONNECT:
      SETTP(tp, tcp_disconnect(tp));
      break;

   /*
    * Accept a connection.  Essentially all the work is
    * done at higher levels; just return the address
    * of the peer, storing through addr.
    */
   case PRU_ACCEPT: 
   {
         struct sockaddr_in * sin   =  mtod(nam,   struct sockaddr_in *);
#ifdef IP_V6
         struct sockaddr_in6 * sin6 = mtod(nam,   struct sockaddr_in6 *);
#endif

#ifdef IP_V6
         if (so->so_domain == AF_INET6)
         {
            nam->m_len = sizeof (struct sockaddr_in6);
            sin6->sin6_port = inp->inp_fport;
            sin6->sin6_family = AF_INET6;
            IP6CPY(&sin6->sin6_addr, &inp->ip6_faddr);
         }
#endif

#ifdef IP_V4
         if (so->so_domain == AF_INET)
         {
            nam->m_len = sizeof (struct sockaddr_in);
            sin->sin_family = AF_INET;
            sin->sin_port = inp->inp_fport;
            sin->sin_addr = inp->inp_faddr;
         }
#endif
         if ( !(so->so_domain == AF_INET) &&
              !(so->so_domain == AF_INET6)
             )
         {
            dprintf("*** PRU_ACCEPT bad domain = %d\n", so->so_domain);
            dtrap();
         } 
         TCP_MIB_INC(tcpPassiveOpens);    /* keep MIB stats */
         break;
      }

   /*
    * Mark the connection as being incapable of further output.
    */
   case PRU_SHUTDOWN:
      socantsendmore(so);
      tp = tcp_usrclosed(tp);
      if (tp)
         error = tcp_output(tp);
      break;

   /*
    * After a receive, possibly send window update to peer.
    */
   case PRU_RCVD:
      (void) tcp_output(tp);
      break;

   /*
    * Do a send by putting data in output queue and updating urgent
    * marker if URG set.  Possibly send more data.
    */
   case PRU_SEND:
      if (so->so_pcb == NULL)
      {                    /* Return EPIPE error if socket is not connected */
         error = EPIPE;
         break;
      }
      sbappend(&so->so_snd, m);
      error = tcp_output(tp);
      if (error == ENOBUFS)
         sbdropend(&so->so_snd,m);  /* Remove data from socket buffer */
      break;

   /*
    * Abort the TCP.
    */
   case PRU_ABORT:
      SETTP(tp, tcp_drop(tp, ECONNABORTED));
      break;

   case PRU_SENSE:
      /*      ((struct stat *) m)->st_blksize = so->so_snd.sb_hiwat; */
      dtrap();    /* does this ever happen? */
      return (0);

   case PRU_RCVOOB:
      if ((so->so_oobmark == 0 &&
          (so->so_state & SS_RCVATMARK) == 0) ||
#ifdef SO_OOBINLINE
       so->so_options & SO_OOBINLINE ||
#endif
       tp->t_oobflags & TCPOOB_HADDATA) 
       {
         error = EINVAL;
         break;
      }
      if ((tp->t_oobflags & TCPOOB_HAVEDATA) == 0) 
      {
         error = EWOULDBLOCK;
         break;
      }
      m->m_len = 1;
      *mtod(m, char *) = tp->t_iobc;
      if ((MBUF2LONG(nam) & MSG_PEEK) == 0)
         tp->t_oobflags ^= (TCPOOB_HAVEDATA | TCPOOB_HADDATA);
      break;

   case PRU_SENDOOB:
      if (so->so_pcb == NULL)
      {                    /* Return EPIPE error if socket is not connected */
         error = EPIPE;
         break;
      }
      if (sbspace(&so->so_snd) == 0) 
      {
         m_freem(m);
         error = ENOBUFS;
         break;
      }
      /*
       * According to RFC961 (Assigned Protocols),
       * the urgent pointer points to the last octet
       * of urgent data.  We continue, however,
       * to consider it to indicate the first octet
       * of data past the urgent section.
       * Otherwise, snd_up should be one lower.
       */
      sbappend(&so->so_snd, m);
      tp->snd_up = tp->snd_una + so->so_snd.sb_cc;
      tp->t_force = 1;
      error = tcp_output(tp);
      if (error == ENOBUFS)
         sbdropend(&so->so_snd,m);  /* Remove data from socket buffer */
      tp->t_force = 0;
      break;

   case PRU_SOCKADDR:

   /* sockaddr and peeraddr have to switch based on IP type */
#ifdef IP_V4
#ifndef IP_V6  /* v4 only */
      in_setsockaddr(inp, nam);
#else /* dual mode */
      if(so->so_domain == AF_INET6)
         ip6_setsockaddr(inp, nam);
      else
         in_setsockaddr(inp, nam);
#endif   /* dual mode */
#else    /* IP_V6 */
         ip6_setsockaddr(inp, nam);
#endif
      break;         

   case PRU_PEERADDR:
#ifdef IP_V4
#ifndef IP_V6  /* v4 only */
      in_setpeeraddr(inp, nam);
#else /* dual mode */
      if(so->so_domain == AF_INET6)
         ip6_setpeeraddr(inp, nam);
      else
         in_setpeeraddr(inp, nam);
#endif   /* dual mode */
#else    /* IP_V6 */
         ip6_setpeeraddr(inp, nam);
#endif
      break;

   case PRU_SLOWTIMO:
      SETTP(tp, tcp_timers(tp, (int)MBUF2LONG(nam)));
#ifdef DO_TCPTRACE
      req |= (long)nam << 8;        /* for debug's sake */
#endif
      break;

      default:
      panic("tcp_usrreq");
   }
#ifdef DO_TCPTRACE
   if (tp && (so->so_options & SO_DEBUG))
      tcp_trace("usrreq: state: %d, tcpcb: %x, req: %d",
    ostate, tp, req);
#endif
   return (error);
}


/* compiled defaults for TCP buffers, may be overwritten at init time */

u_long   tcp_sendspace  =  1024*8;
u_long   tcp_recvspace  =  1024*8;


/* FUNCTION: tcp_attach()
 *
 * Attach TCP protocol to socket, allocating
 * internet protocol control block, tcp control block,
 * bufer space, and entering LISTEN state if to accept connections.
 * 
 * PARAM1: struct socket *so
 *
 * RETURNS: 0 if OK, or nonzero error code.
 */

int
tcp_attach(struct socket * so)
{
   struct tcpcb * tp;
   struct inpcb * inp;
   int   error;

   if (so->so_snd.sb_hiwat == 0 || so->so_rcv.sb_hiwat == 0) 
   {
      error = soreserve(so, tcp_sendspace, tcp_recvspace);
      if (error)
         return (error);
   }
   error = in_pcballoc(so, &tcb);
   if (error)
      return (error);
   inp = sotoinpcb(so);
   tp = tcp_newtcpcb(inp);
   if (tp == 0) 
   {
      int   nofd  =  so->so_state   &  SS_NOFDREF; /* XXX */

      so->so_state &= ~SS_NOFDREF;     /* don't free the socket yet */
      in_pcbdetach(inp);
      so->so_state |= nofd;
      return (ENOBUFS);
   }
   tp->t_state = TCPS_CLOSED;
   return (0);
}


/* FUNCTION: tcp_disconnect()
 *
 * Initiate (or continue) disconnect.
 * If embryonic state, just send reset (once).
 * If in ``let data drain'' option and linger null, just drop.
 * Otherwise (hard), mark socket disconnecting and drop
 * current input data; switch states based on user close, and
 * send segment to peer (with FIN).
 *
 * 
 * PARAM1: struct tcpcb *tp
 *
 * RETURNS: 
 */

struct tcpcb * 
tcp_disconnect(struct tcpcb * tp)
{
   struct socket *   so =  tp->t_inpcb->inp_socket;

   if (tp->t_state < TCPS_ESTABLISHED)
      tp = tcp_close(tp);
   else if ((so->so_options & SO_LINGER) && so->so_linger == 0)
      tp = tcp_drop(tp, 0);
   else 
   {
      soisdisconnecting(so);
      sbflush(&so->so_rcv);
      tp = tcp_usrclosed(tp);
      if (tp)
         (void) tcp_output(tp);
   }
   return (tp);
}



/* FUNCTION: tcp_usrclosed()
 *
 * User issued close, and wish to trail through shutdown states:
 * if never received SYN, just forget it.  If got a SYN from peer,
 * but haven't sent FIN, then go to FIN_WAIT_1 state to send peer a FIN.
 * If already got a FIN from peer, then almost done; go to LAST_ACK
 * state.  In all other cases, have already sent FIN to peer (e.g.
 * after PRU_SHUTDOWN), and just have to play tedious game waiting
 * for peer to send FIN or not respond to keep-alives, etc.
 * We can let the user exit from the close as soon as the FIN is acked.
 * 
 * PARAM1: struct tcpcb *tp
 *
 * RETURNS: 
 */

struct tcpcb * 
tcp_usrclosed(struct tcpcb * tp)
{

   switch (tp->t_state) 
   {
   case TCPS_CLOSED:
   case TCPS_LISTEN:
   case TCPS_SYN_SENT:
      tp->t_state = TCPS_CLOSED;
      tp = tcp_close(tp);
      break;

   case TCPS_SYN_RECEIVED:
   case TCPS_ESTABLISHED:
      tp->t_state = TCPS_FIN_WAIT_1;
      break;

   case TCPS_CLOSE_WAIT:
      tp->t_state = TCPS_LAST_ACK;
      break;
   }
   if (tp && tp->t_state >= TCPS_FIN_WAIT_2)
      soisdisconnected(tp->t_inpcb->inp_socket);
   return (tp);
}

#endif /* INCLUDE_TCP */


/* end of file tcp_usr.c */

