/*
 * FILENAME: tcp_subr.c
 *
 * Copyright 1997-2007 By InterNiche Technologies, Inc. All rights reserved.
 *
 *
 * MODULE: INET
 *
 * ROUTINES: tcp_init(), tcp_template(), tcp_respond(), 
 * ROUTINES: tcp_newtcpcb(), tcp_drop(), tcp_close(), tcp_quench(),
 * ROUTINES: tcp_getseq(), tcp_putseq(), 
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

int      tcp_ttl  =  TCP_TTL;

struct   tcpstat  tcpstat;
struct  inpcb  tcb;


#ifdef TCP_BIGCWND      /* Support for Large initial congestion window */
int      use_default_cwnd = 1;         /* Flag to use this on all sockets */
u_long   default_cwnd = (2*TCP_MSS);  /* initial cwnd value to use */
#endif   /* TCP_BIGCWND */


/* FUNCTION: tcp_init()
 * 
 * Tcp initialization
 *
 * 
 * PARAM1: 
 *
 * RETURNS: 
 */

void
tcp_init()
{
   tcp_iss = 1;      /* wrong */
   tcb.inp_next = tcb.inp_prev = &tcb;
}



/* FUNCTION: tcp_template()
 *
 * Create template to be used to send tcp packets on a connection.
 * Call after host entry created, allocates an mbuf and fills
 * in a skeletal tcp/ip header, minimizing the amount of work
 * necessary when the connection is used.
 *
 * 
 * PARAM1: struct tcpcb *tp
 *
 * RETURNS: 
 */

struct tcpiphdr * 
tcp_template(struct tcpcb * tp)
{
   struct inpcb * inp   =  tp->t_inpcb;
   struct tcpiphdr * n;

   if ((n = tp->t_template) == 0)
   {
      n = (struct tcpiphdr *)TPH_ALLOC (sizeof (*n));
      if (n == NULL)
         return (0);
   }
   n->ti_next = n->ti_prev = 0;
   n->ti_len = htons(sizeof (struct tcpiphdr) - sizeof (struct ip));
   n->ti_src = inp->inp_laddr;
   n->ti_dst = inp->inp_faddr;
   n->ti_sport = inp->inp_lport;
   n->ti_dport = inp->inp_fport;
   n->ti_seq = 0;
   n->ti_ack = 0;
   n->ti_t.th_doff = (5 << 4);   /* NetPort */
   n->ti_flags = 0;
   n->ti_win = 0;
   n->ti_sum = 0;
   n->ti_urp = 0;
   return (n);
}


/* FUNCTION: tcp_respond()
 *
 * Send a single message to the TCP at address specified by
 * the given TCP/IP header.  If flags==0, then we make a copy
 * of the tcpiphdr at ti and send directly to the addressed host.
 * This is used to force keep alive messages out using the TCP
 * template for a connection tp->t_template.  If flags are given
 * then we send a message back to the TCP which originated the
 * segment ti, and discard the mbuf containing it and any other
 * attached mbufs.
 *
 * In any case the ack and sequence number of the transmitted
 * segment are as specified by the parameters.
 *
 * 
 * PARAM1: struct tcpcb *tp
 * PARAM2: struct tcpiphdr *ti
 * PARAM3: tcp_seq ack
 * PARAM4: tcp_seq seq
 * PARAM5: int flags
 * PARAM6: struct mbuf *ti_mbuf
 *
 * RETURNS: 
 */

#define  xchg(a,b,type) {  type  t; t=a;  a=b;  b=t;  }

void
tcp_respond(struct tcpcb * tp,
   struct tcpiphdr * ti,
   tcp_seq  ack,
   tcp_seq  seq,
   int   flags,
   struct mbuf *  ti_mbuf)
{
   int      tlen;       /* tcp data len - 0 or 1 */
   int      domain;     /* AF_INET or AF_INET6 */
   int      win = 0;    /* window to use in sent packet */
   struct mbuf *  m;    /* mbuf to send */
   struct tcpiphdr * tmp_thdr;   /* scratch */

   if (tp)
      win = (int)sbspace(&tp->t_inpcb->inp_socket->so_rcv);

   /* Figure out of we can recycle the passed buffer or if we need a 
    * new one. Construct the easy parts of the the TCP and IP headers.
    */
   if (flags == 0)   /* sending keepalive from timer */
   {
      /* no flags == need a new buffer */
      m = m_getwithdata (MT_HEADER, 64);
      if (m == NULL)
         return;
      tlen = 1;   /* Keepalives have one byte of data */
      m->m_len = TCPIPHDRSZ + tlen;
      /*
       * Copy template contents into the mbuf and set ti to point
       * to the header structure in the mbuf.
       */
      tmp_thdr = (struct tcpiphdr *)((char *)m->m_data+sizeof(struct ip)
         - sizeof(struct ipovly));
      if ((char *)tmp_thdr < m->pkt->nb_buff)
      {
         panic("tcp_respond- packet ptr underflow\n");
      }
      MEMCPY(tmp_thdr, ti, sizeof(struct tcpiphdr));
      ti = tmp_thdr;
      flags = TH_ACK;
      domain = tp->t_inpcb->inp_socket->so_domain;
   }
   else  /* Flag was passed (e.g. reset); recycle passed mbuf */
   {
      m = ti_mbuf;   /*dtom(ti);*/
      if(m->pkt->type == IPTP)   /* IPv4 packet */
         domain = AF_INET;
      else
         domain = AF_INET6;

      m_freem(m->m_next);
      m->m_next = 0;
      tlen = 0;         /* NO data */
      m->m_len = TCPIPHDRSZ;
      xchg(ti->ti_dport, ti->ti_sport, u_short);
      if(m->pkt->type == IPTP)
         xchg(ti->ti_dst.s_addr, ti->ti_src.s_addr, u_long);
      if (flags & TH_RST)  /* count resets in MIB */
         TCP_MIB_INC(tcpOutRsts);   /* keep MIB stats */
   }

   /* finish constructing the TCP header */
   ti->ti_seq = htonl(seq);
   ti->ti_ack = htonl(ack);
   ti->ti_t.th_doff = 0x50;      /* NetPort: init data offset bits */
   ti->ti_flags = (u_char)flags;
   ti->ti_win = htons((u_short)win);
   ti->ti_urp = 0;

   /* Finish constructing IP header and send, based on IP type in use */
   switch(domain)
   {
#ifdef IP_V4
      case AF_INET:
      {
         struct ip * pip;

         pip = (struct ip *)((char*)ti+sizeof(struct ipovly)-sizeof(struct ip));

         pip->ip_len = (unshort)(TCPIPHDRSZ + tlen);

         /* If our system's max. MAC header size is geater than the size 
          * of the MAC header in the received packet then we need to 
          * adjust the IP header offset to allow for this. Since the packets 
          * are only headers they should always fit.
          */
         if(pip >= (struct ip *)(m->pkt->nb_buff + MaxLnh))
         {
            m->m_data = (char*)pip; /* headers will fit, just set pointer */
         }
         else     /* MAC may not fit, adjust pointer and move headers back */
         {
            m->m_data = m->pkt->nb_prot = m->pkt->nb_buff + MaxLnh;  /* new ptr */
            MEMMOVE(m->m_data, pip, TCPIPHDRSZ);  /* move back tcp/ip headers */
         }

         /*
          * In the case of a SYN DOS attack, many RST|ACK replies
          *   have no tp structure and need to be freed.
          */
         if (!tp)
              m_freem(m);
         else
		 {
			 if ((tp->t_inpcb) && (tp->t_inpcb->inp_socket))
				ip_output(m, tp->t_inpcb->inp_socket->so_optsPack);
			 else
				ip_output(m, (struct   ip_socopts *)NULL);
		 }

         break;
      }
#endif   /* IP_V4 */
#ifdef IP_V6
      case AF_INET6:
      {
         struct ipv6 *  pip6;
         struct mbuf *  ip_m;     /* IP header's mbuf */

         /* Get mbuf space for the IP header. mbuf m shold contain the
          * TCP header somewhere, so set m_dsata to that and try to prepend 
          * an IPv6 header.
          */
         m->m_data = (char*)&ti->ti_t;    /* TCP header */
         m->m_len = sizeof(struct tcphdr);
         ip_m = mbuf_prepend(m, sizeof(struct ipv6));
         if(!ip_m)
         {
            m_free(m);
            return;
         }

         pip6 = (struct ipv6 *)ip_m->m_data;

         /* we have to find the IPv6 addresses. If a packet was passed
          * then get them form that, otherwise get them from the passed tp.
          * we should always have one or the other.
          */
         if(ti_mbuf)
         {
            ip6_addr       tmp;
            struct ipv6 * inpip = ti_mbuf->pkt->ip6_hdr;

            /* pip6 and inpip may be the same, so swap the IP addresses
             * through a tmp variable.
             */
            IP6CPY(&tmp, &inpip->ip_src);
            IP6CPY(&pip6->ip_src, &inpip->ip_dest);
            IP6CPY(&pip6->ip_dest, &tmp);
         }
         else if(tp)
         {
            struct inpcb * inp = tp->t_inpcb;

            IP6CPY(&pip6->ip_src, &inp->ip6_laddr);
            IP6CPY(&pip6->ip_dest, &inp->ip6_faddr);
         }
         else
         {
            dtrap();
            break;
         }
         /* best effort send */
         /* send down to glue layer to IPv6 */
         /* and don't forget the so_optsPack */
         if (tp)
            if (tp->t_inpcb)
			         if (tp->t_inpcb->inp_socket)
                  tcp6_send(tp, ip_m, &ti->ti_t, 
                     sizeof(struct ipv6) + sizeof(struct tcphdr) + tlen,
                     tp->t_inpcb->inp_socket->so_optsPack);
			         else
                  tcp6_send(tp, ip_m, &ti->ti_t, 
                     sizeof(struct ipv6) + sizeof(struct tcphdr) + tlen,
                     (struct   ip_socopts *)0);

         break;
      }
#endif   /* IP_V6 */
      default:
         dtrap();
         break;
   }
   return;
}

#undef xchg


/* FUNCTION: tcp_newtcpcb()
 *
 * Create a new TCP control block, making an
 * empty reassembly queue and hooking it to the argument
 * protocol control block.
 *
 * 
 * PARAM1: struct inpcb *inp
 *
 * RETURNS: 
 */

struct tcpcb * 
tcp_newtcpcb(struct inpcb * inp)
{
   struct tcpcb * tp;
   short t_time;

   tp = TCB_ALLOC(sizeof (*tp));
   if (tp == NULL)
      return (struct tcpcb *)NULL;
   tp->seg_next = tp->seg_prev = (struct tcpiphdr *)tp;
   tp->t_maxseg = TCP_MSS;
   tp->t_flags = 0;        /* sends options! */
   tp->t_inpcb = inp;
   /*
    * Init srtt to TCPTV_SRTTBASE (0), so we can tell that we have no
    * rtt estimate.  Set rttvar so that srtt + 2 * rttvar gives
    * reasonable initial retransmit time.
    */
   tp->t_srtt = TCPTV_SRTTBASE;
   tp->t_rttvar = TCPTV_SRTTDFLT << 2;

   t_time = ((TCPTV_SRTTBASE >> 2) + (TCPTV_SRTTDFLT << 2)) >> 1;
   TCPT_RANGESET(tp->t_rxtcur, t_time, TCPTV_MIN, TCPTV_REXMTMAX);

   /* Set initial congestion window - RFC-2581, pg 4. */
   tp->snd_cwnd = 2 * TCP_MSS;

#ifdef TCP_BIGCWND
   /* See if we should set a large initial congestion window (RFC-2518) */
   if((use_default_cwnd) ||
      (inp->inp_socket->so_options & SO_BIGCWND))
   {
      tp->snd_cwnd = default_cwnd;
   }
#endif


#ifdef TCP_WIN_SCALE
   /* set TCP window scaling factor to outgoing window. The window we are
    * sending is what we are silling to receive, so rfc1323 calls this the
    * "rcv.wind.scale". We will try to negotiate this on the syn/ack
    * handshake and back off to zero if it fails.
    */
   tp->rcv_wind_scale = default_wind_scale;
#endif   /* TCP_WIN_SCALE */

#ifdef DO_DELAY_ACKS
   tp->t_delacktime = 1;
#endif   /* DO_DELAY_ACKS */

   tp->snd_ssthresh = 65535;  /* Start with high slow-start threshold */

   inp->inp_ppcb = (char *)tp;
   return (tp);
}


/* FUNCTION: tcp_drop()
 *
 * Drop a TCP connection, reporting
 * the specified error.  If connection is synchronized,
 * then send a RST to peer.
 *
 * 
 * PARAM1: struct tcpcb *tp
 * PARAM2: int err
 *
 * RETURNS: 
 */

struct tcpcb * 
tcp_drop(struct tcpcb * tp, int err)
{
   struct socket *   so =  tp->t_inpcb->inp_socket;

   if (TCPS_HAVERCVDSYN(tp->t_state)) 
   {
      tp->t_state = TCPS_CLOSED;
      (void) tcp_output(tp);
      tcpstat.tcps_drops++;
   }
   else
      tcpstat.tcps_conndrops++;
   so->so_error = err;
#ifdef TCP_ZEROCOPY
   if (so->rx_upcall)
      so->rx_upcall(so, NULL, err);
#endif   /* TCP_ZEROCOPY */
   return (tcp_close(tp));
}


/* FUNCTION: tcp_close()
 *
 * Close a TCP control block:
 *   discard all space held by the tcp
 *   discard internet protocol block
 *   wake up any sleepers
 *
 * 
 * PARAM1: struct tcpcb *tp
 *
 * RETURNS: 
 */

struct tcpcb * 
tcp_close(struct tcpcb * tp)
{
   struct tcpiphdr * t;
   struct inpcb * inp   =  tp->t_inpcb;
   struct socket *   so =  inp->inp_socket;
   struct mbuf *  m;

   t = tp->seg_next;
   while (t != (struct tcpiphdr *)tp) 
   {
      t = (struct tcpiphdr *)t->ti_next;
      m = dtom(t->ti_prev);
      remque(t->ti_prev);
      m_freem (m);
   }
   if (tp->t_template)
      TPH_FREE (tp->t_template);
   TCB_FREE (tp);
   inp->inp_ppcb = 0;
   soisdisconnected(so);
   in_pcbdetach(inp);
   tcpstat.tcps_closed++;
   return ((struct tcpcb *)0);
}


/* FUNCTION: tcp_quench()
 *
 * When a source quench is received, close congestion window
 * to one segment.  We will gradually open it again as we proceed.
 *
 * 
 * PARAM1: struct inpcb *inp
 *
 * RETURNS: 
 */

void
tcp_quench(struct inpcb * inp)
{
   struct tcpcb * tp =  intotcpcb(inp);

   if (tp)
      tp->snd_cwnd = tp->t_maxseg;
}


/* FUNCTION: tcp_putseq()
 *
 * put tcp seq in options header
 *
 * PARAM1: char * cp - pointer to buffer to insert number
 * PARAM2: 32 bit number to insert
 *
 * RETURNS: pointer to end of buffer (cp + 4)
 */

/* tcp_putseq()  */

u_char *
tcp_putseq(u_char * cp, tcp_seq seq)
{
   int   i;

   cp += 3;                /* do low byte first */
   for(i = 0; i< 4; i++)   /* put 4 bytes into buffer */
   {
      *cp-- = (u_char)(seq & 0xFF);  /* back through buffer */
      seq >>= 8;
   }
   return (cp + 5);
}

/* FUNCTION: tcp_getseq()
 *
 * SACK utility to extract a long (tcp seq) from a character pointer. It
 * does this bytewise so that the long does not have to be on a 32 bit
 * boundary, as is the case in SACK options headers.
 * 
 * PARAM1: char * cp - pointer to 32 bit net-endian number
 *
 * RETURNS: a long in local endian
 */

u_long
tcp_getseq(u_char * cp)
{
   int i;
   ulong seq = 0;

   for(i = 0; i < 4; i++)
   {
      seq <<= 8;
      seq += (u_long)*(cp++);
   }
   return seq;
}

#endif /* INCLUDE_TCP */

