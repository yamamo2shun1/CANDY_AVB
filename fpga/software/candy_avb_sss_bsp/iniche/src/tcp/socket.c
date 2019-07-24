/*
 * FILENAME: socket.c
 *
 * Copyright 1997-2007 By InterNiche Technologies, Inc. All rights reserved.
 *
 *
 * MODULE: INET
 *
 * ROUTINES: socreate (), sobind(), solisten(), sofree(), 
 * ROUTINES: soclose(), soabort(), soaccept(), soconnect(), sodisconnect(), 
 * ROUTINES: sosend(), soreceive(), soshutdown(), sorflush(), sosetopt(), 
 * ROUTINES: sogetopt(), sohasoutofband()
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
#include "ip.h"         /* for IP-layer socket options */
#include "tcp.h"        /* for TCP-layer socket options */
#include "tcp_fsm.h"
#include "tcp_seq.h"
#include "tcp_timr.h"
#include "tcp_var.h"


/* default so_options value for new sockets. This can be set by applications 
 * (user code) at runtime, but it will apply to ALL new sockets in the system.
 */

#define  SO_DEFAULTS    SO_OOBINLINE

#ifdef SO_TCPSACK
#define SO_TCPSACK_DEF SO_TCPSACK
#else
#define SO_TCPSACK_DEF  0
#endif /* SO_TCPSACK */

#ifdef SO_WINSCALE
#define SO_WINSCALE_DEF SO_WINSCALE
#else
#define SO_WINSCALE_DEF 0
#endif /* SO_WINSCALE */

#ifdef SO_TIMESTAMP
#define SO_TIMESTAMP_DEF SO_TIMESTAMP
#else
#define SO_TIMESTAMP_DEF   0
#endif /* SO_TIMESTAMP */

#ifdef SO_BIGCWND
#define SO_BIGCWND_DEF SO_BIGCWND
#else
#define SO_BIGCWND_DEF  0
#endif /* SO_BIGCWND */

/* always force inline OOB data, big in others if defined in build */
unshort socket_defaults = (SO_OOBINLINE | SO_TCPSACK_DEF |
            SO_WINSCALE_DEF | SO_TIMESTAMP_DEF | SO_BIGCWND_DEF);

/*
 * Socket operation routines.
 * These routines are called by the routines in
 * sockcall.c or from a system process, and
 * implement the semantics of socket operations by
 * switching out to the protocol specific routines.
 */

#ifdef IP_MULTICAST
int   ip_setmoptions(int optname, struct socket *so, void *arg);
int   ip_getmoptions(int optname, struct socket *so, void *arg);
int   ip_freemoptions(struct ip_moptions *imo);
#endif   /* IP_MULTICAST */

/* FUNCTION: socreate ()
 * 
 * PARAM1: int dom
 * PARAM2: int type
 * PARAM3: int proto
 *
 * RETURNS: 
 */

struct socket *   
socreate (int dom, int type, int proto)
{
   struct protosw *prp;
   struct socket *so;
   int   error;
   int rc;

   if (proto)
      prp = pffindproto(dom, proto, type);
   else
      prp = pffindtype(dom, type);
   if (prp == 0)
      return NULL;
   if (prp->pr_type != type)
      return NULL;
   if ((so = SOC_ALLOC (sizeof (*so))) == NULL)
      return NULL;
   so->next = NULL;
   putq(&soq,(qp)so);

   so->so_options = socket_defaults;
   so->so_domain = dom;
   so->so_state = 0;
   so->so_type = (char)type;
   so->so_proto = prp;

#ifdef IP_MULTICAST
   so->inp_moptions = NULL;
#endif   /* IP_MULTICAST */

   so->so_req = PRU_ATTACH;
   error = (*prp->pr_usrreq)(so,(struct mbuf *)0, LONG2MBUF((long)proto));
   if (error) goto bad;

   if (so_evtmap)
   {                       
      rc = (*so_evtmap_create) (so);
      if (rc != 0)
      {
bad:   
         so->so_state |= SS_NOFDREF;
         sofree (so);
         return NULL;   
      }
      /*
       * Altera Niche Stack Nios port modification:
       * Remove (void *) cast since -> owner is now TK_OBJECT
       * to fix build warning.
       */
      so->owner = TK_THIS;
   }

   return so;
}



/* FUNCTION: sobind()
 * 
 * PARAM1: struct socket *so
 * PARAM2: struct mbuf *nam
 *
 * RETURNS: 0 if OK, else one of the socket error codes
 */

int
sobind(struct socket * so, 
   struct mbuf *  nam)
{
   int   error;

   so->so_req = PRU_BIND;
   error = (*so->so_proto->pr_usrreq)(so, (struct mbuf *)0, nam);
   return (error);
}



/* FUNCTION: solisten()
 * 
 * PARAM1: struct socket *so
 * PARAM2: int backlog
 *
 * RETURNS: 0 if OK, else one of the socket error codes
 */

int
solisten(struct socket * so, 
   int   backlog)
{
   int   error;

   so->so_req = PRU_LISTEN;
   error = (*so->so_proto->pr_usrreq)(so,
    (struct mbuf *)0, (struct mbuf *)0);
   if (error) 
   {
      return (error);
   }
   if (so->so_q == 0) 
   {
      so->so_q = so;
      so->so_q0 = so;
      so->so_options |= SO_ACCEPTCONN;
   }
   if (backlog < 0)
      backlog = 0;
   so->so_qlimit = (char)MIN(backlog, SOMAXCONN);
   return 0;
}



/* FUNCTION: sofree()
 * 
 * PARAM1: struct socket *so
 *
 * RETURNS: 
 */

void
sofree(struct socket * so)
{
   INET_TRACE (INETM_SOCKET|INETM_CLOSE,
    ("INET: sofree, so %lx so_pcb %lx so_state %x so_head %lx\n",
    so, so->so_pcb, so->so_state, so->so_head));

   if (so->so_pcb || (so->so_state & SS_NOFDREF) == 0)
      return;
   if (so->so_head) 
   {
      if (!soqremque(so, 0) && !soqremque(so, 1))
         panic("sofree");
      so->so_head = 0;
   }
   sbrelease(&so->so_snd);
   sorflush(so);
#ifdef SAVE_SOCK_ENDPOINTS
   if (so->so_endpoint)
      _socket_free_entry (so);
#endif   /* SAVE_SOCK_ENDPOINTS */

#ifdef IP_MULTICAST
   /* multicast opts? */
   if (so->inp_moptions)
	   ip_freemoptions(so->inp_moptions);
#endif   /* IP_MULTICAST */

   /* IP_TOS opts? */
   if (so->so_optsPack)
      SOCOPT_FREE(so->so_optsPack);
	   
   qdel(&soq, so);   /* Delete the socket entry from the queue */
   
   if (so_evtmap)  
      (*so_evtmap_delete) (so);
   
   SOC_FREE(so);
}


/* FUNCTION: soclose()
 * 
 * Close a socket on last file table reference removal.
 * Initiate disconnect if connected.
 * Free socket when disconnect complete.
 *
 * PARAM1: struct socket *so
 *
 * RETURNS: 
 */

int
soclose(struct socket * so)
{
   int   error =  0;
   struct socket *   tmpso;
   unsigned long endtime;

   /* Check whether the closing socket is in the socket queue.  If it is
    * not, return a EINVAL error code to the caller.
    */
   for ((tmpso=(struct socket *)soq.q_head);tmpso != NULL;tmpso=tmpso->next)
   {
      if (so == tmpso)
         break;
   }
   if ( tmpso == NULL)
      return EINVAL;
   INET_TRACE (INETM_SOCKET|INETM_CLOSE,
    ("INET: soclose, so %lx  so_pcb %lx so_state %x so_q %lx\n",
    so, so->so_pcb, so->so_state, so->so_q));
   if (so->so_options & SO_ACCEPTCONN)
   {
      while (so->so_q0 != so)
         (void) soabort(so->so_q0);
      while (so->so_q != so)
         (void) soabort(so->so_q);
   }
   /* for datagram-oriented sockets, dispense with further tests */
   if (so->so_type != SOCK_STREAM)
   { 
      so->so_req = PRU_DETACH;
      error = (*so->so_proto->pr_usrreq)(so,
       (struct mbuf *)0, (struct mbuf *)0);
      goto discard;
   }

   if (so->so_pcb == 0)
      goto discard;
   if (so->so_state & SS_ISCONNECTED) 
   {
      if ((so->so_state & SS_ISDISCONNECTING) == 0) 
      {
         error = sodisconnect(so);
         if (error)
            goto drop;
      }
      if (so->so_options & SO_LINGER) 
      {
         if ((so->so_state & SS_ISDISCONNECTING) &&
             (so->so_state & SS_NBIO))
         {
            goto drop;
         }
         endtime = cticks + (unsigned long)so->so_linger * TPS;         
         while ((so->so_state & SS_ISCONNECTED) && (cticks < endtime))
         {
            tcp_sleep((char *)&so->so_timeo);
         }
      }
      else  /* Linger option not set */
      {
         /* If socket still has send data just return now, leaving the 
          * socket intact so the data can be sent. Socket should be cleaned
          * up later by timers.
          */
         if(so->so_snd.sb_cc)
         {
            so->so_state |= SS_NOFDREF;   /* mark as OK to close */
            return 0;
         }
      }
   }
drop:
   if (so->so_pcb) 
   {
      int   error2;
      so->so_req = PRU_DETACH;
      error2 = (*so->so_proto->pr_usrreq)(so,
       (struct mbuf *)0, (struct mbuf *)0);
      if (error == 0)
         error = error2;
   }
discard:
   if (so->so_state & SS_NOFDREF)
   {
      /* panic("soclose");  - non-fatal - degrade to dtrap() for now */
      dtrap();
   }
   so->so_state |= SS_NOFDREF;
   sofree(so);
   return (error);
}



/* FUNCTION: soabort()
 * 
 * Must be called at splnet...
 *
 * PARAM1: struct socket *so
 *
 * RETURNS: 
 */

int
soabort(struct socket * so)
{
   so->so_req = PRU_ABORT;
   return(*so->so_proto->pr_usrreq)(so, (struct mbuf *)0, (struct mbuf *)0);
}



/* FUNCTION: soaccept()
 * 
 * PARAM1: struct socket *so
 * PARAM2: struct mbuf * nam
 *
 * RETURNS: 
 */

int
soaccept(struct socket * so, 
   struct mbuf *  nam)
{
   int   error;

   if ((so->so_state & SS_NOFDREF) == 0)
      panic("soaccept");
   so->so_state &= ~SS_NOFDREF;
   so->so_req = PRU_ACCEPT;
   error = (*so->so_proto->pr_usrreq)(so, (struct mbuf *)0, nam);

   return (error);
}



/* FUNCTION: soconnect()
 * 
 * PARAM1: struct socket *so
 * PARAM2: struct mbuf *nam
 *
 * RETURNS: 
 */

int
soconnect(struct socket * so, 
   struct mbuf *  nam)
{
   int   error;

   if (so->so_options & SO_ACCEPTCONN)
      return (EOPNOTSUPP);
   /*
    * If protocol is connection-based, can only connect once.
    * Otherwise, if connected, try to disconnect first.
    * This allows user to disconnect by connecting to, e.g.,
    * a null address.
    */
   if (so->so_state & (SS_ISCONNECTED|SS_ISCONNECTING) &&
       ((so->so_proto->pr_flags & PR_CONNREQUIRED) ||
       (sodisconnect(so) != 0)))
   {
      error = EISCONN;
   }
   else
   {
      so->so_req = PRU_CONNECT;
      error = (*so->so_proto->pr_usrreq)(so, (struct mbuf *)0, nam);
   }
   return error;
}



/* FUNCTION: sodisconnect()
 * 
 * PARAM1: struct socket *so
 *
 * RETURNS: 
 */

int
sodisconnect(struct socket * so)
{
   int   error;

   INET_TRACE (INETM_SOCKET|INETM_CLOSE,
    ("INET: sodisconnect, so %lx so_state %x\n", so, so->so_state));

   if ((so->so_state & SS_ISCONNECTED) == 0) 
   {
      error = ENOTCONN;
      goto bad;
   }
   if (so->so_state & SS_ISDISCONNECTING) 
   {
      error = EALREADY;
      goto bad;
   }
   so->so_req = PRU_DISCONNECT;
   error = (*so->so_proto->pr_usrreq)(so, (struct mbuf *)0, (struct mbuf *)0);

bad:
   return (error);
}



/* FUNCTION: sosend()
 * 
 * Send on a socket.
 * If send must go all at once and message is larger than
 * send buffering, then hard error.
 * Lock against other senders.
 * If must go all at once and not enough room now, then
 * inform user that this would block and do nothing.
 * Otherwise, if nonblocking, send as much as possible.
 *
 * PARAM1: struct socket *so
 * PARAM2: struct mbuf *nam
 * PARAM3: char * data;
 * PARAM4: int * data_length;
 * PARAM5: int flags
 *
 * RETURNS: 0 if OK or BSD socket error code
 */

int
sosend(struct socket *so, 
       struct mbuf *nam,      /* sockaddr, if UDP socket, NULL if TCP */
       char  *data,           /* data to send */
       int   *data_length,    /* IN/OUT  length of (remaining) data */
       int   flags)
{
   struct mbuf *head = (struct mbuf *)NULL;
   struct mbuf *m;
   int   space;
   int   resid;
   int   len;
   int   error = 0;
   int   dontroute;
   int   first = 1;

   resid = *data_length;

   /*
    * In theory resid should be unsigned.
    * However, space must be signed, as it might be less than 0
    * if we over-committed, and we must use a signed comparison
    * of space and resid.  On the other hand, a negative resid
    * causes us to loop sending 0-length segments to the protocol.
    */
   if (resid < 0)
      return (EINVAL);

   INET_TRACE (INETM_IO, ("INET:sosend: so %lx resid %d sb_hiwat %d so_state %x\n",
               so, resid, so->so_snd.sb_hiwat, so->so_state));

   if (sosendallatonce(so) && (resid > (int)so->so_snd.sb_hiwat))
      return (EMSGSIZE);

   dontroute = (flags & MSG_DONTROUTE) &&
               ((so->so_options & SO_DONTROUTE) == 0) &&
               (so->so_proto->pr_flags & PR_ATOMIC);

#define     snderr(errno)     {  error =  errno;   goto  release; }

restart:
   sblock(&so->so_snd);
   do 
   {
      if (so->so_error) 
      {
         error = so->so_error;
         so->so_error = 0;          /* ??? */
         goto release;
      }
      if (so->so_state & SS_CANTSENDMORE)
         snderr(EPIPE);
      if ((so->so_state & SS_ISCONNECTED) == 0) 
      {
         if (so->so_proto->pr_flags & PR_CONNREQUIRED)
            snderr(ENOTCONN);
         if (nam == 0)
            snderr(EDESTADDRREQ);
      }
      if (flags & MSG_OOB)
         space = 1024;
      else 
      {
         space = (int)sbspace(&so->so_snd);
         if ((sosendallatonce(so) && (space < resid)) ||
             ((resid >= CLBYTES) && (space < CLBYTES) &&
              (so->so_snd.sb_cc >= CLBYTES) &&
              ((so->so_state & SS_NBIO) == 0) &&
              ((flags & MSG_DONTWAIT) == 0)))
         {
            if ((so->so_state & SS_NBIO) || (flags & MSG_DONTWAIT))
            {
               if (first)
                  error = EWOULDBLOCK;
               goto release;
            }
            sbunlock(&so->so_snd);
            sbwait(&so->so_snd);
            goto restart;
         }
      }
      if ( space <= 0 ) 
      {
         /* no space in socket send buffer - see if we can wait */
         if ((so->so_state & SS_NBIO) || (flags & MSG_DONTWAIT))
         {
            if (first)     /* report first error */
               error = EWOULDBLOCK;
            goto release;
         }
         /* If blocking socket, let someone else run */
         sbunlock(&so->so_snd);
         sbwait(&so->so_snd);
         goto restart;
      }

      while (space > 0) 
      {
         len = resid;
         if ( so->so_type == SOCK_STREAM )
         {
            m = m_getwithdata(MT_TXDATA, len);
            if (!m)   
               snderr(ENOBUFS);
            MEMCPY(m->m_data, data, len);
            so->so_snd.sb_flags |= SB_MBCOMP;   /* allow compression */
         }
         else
         {
            m = m_get (M_WAIT, MT_TXDATA);
            m->m_data = data;
         }
         INET_TRACE (INETM_IO,
          ("sosend:got %d bytes so %lx mlen %d, off %d mtod %x\n",
             len, so, m->m_len, m->m_off, mtod (m, caddr_t)));

         *data_length -= len;
         resid -= len;
         data += len;
         m->m_len = len;
         if (head == (struct mbuf *)NULL)
            head = m;
         if (error)
            goto release;
         if (*data_length <= 0)
            break;
      }

      if (dontroute)
         so->so_options |= SO_DONTROUTE;

      so->so_req = (flags & MSG_OOB) ? PRU_SENDOOB : PRU_SEND;
      error = (*so->so_proto->pr_usrreq)(so, head, nam);

      if (dontroute)
         so->so_options &= ~SO_DONTROUTE;

      head = (struct mbuf *)NULL;
      first = 0;
   } while ((resid != 0) && (error == 0));

release:
   sbunlock(&so->so_snd);  
   if (head)
      m_freem(head);
   return error;
}



/* FUNCTION: soreceive()
 * 
 * Implement receive operations on a socket.
 * We depend on the way that records are added to the sockbuf
 * by sbappend*.  In particular, each record (mbufs linked through m_next)
 * must begin with an address if the protocol so specifies,
 * followed by an optional mbuf containing access rights if supported
 * by the protocol, and then zero or more mbufs of data.
 * In order to avoid blocking network interrupts for the entire time here,
 * we splx() while doing the actual copy to user space.
 * Although the sockbuf is locked, new data may still be appended,
 * and thus we must maintain consistency of the sockbuf during that time.
 *
 * PARAM1: struct socket *so
 * PARAM2: struct mbuf **aname
 * PARAM3: char * data
 * PARAM4: int * datalen;
 * PARAM5: int flags
 *
 * RETURNS: 
 */

int
soreceive(struct socket * so, 
   struct mbuf **aname,
   char * data,
   int * datalen,
   int   flags)
{
   struct mbuf *  m;
   int   len;
   int   error =  0;
   int   offset;
   struct protosw *  pr =  so->so_proto;
   struct mbuf *  nextrecord;
   int   moff;
   int   lflags;

   if (aname)
      *aname = 0;
   if (flags & MSG_OOB) 
   {
      m = m_get (M_WAIT, MT_RXDATA);
      if (m == NULL)
         return ENOBUFS;
      lflags = flags & MSG_PEEK;

      so->so_req = PRU_RCVOOB;
      error = (*pr->pr_usrreq)(so, m, LONG2MBUF((long)lflags));
      if (error == 0)
      {
         do 
         {
            len = *datalen;
            if (len > (int)m->m_len)
               len = m->m_len;

            MEMCPY(data, mtod(m, char*), len);
            data += len;
            *datalen = len;
            m = m_free(m);
         } while (*datalen && (error == 0) && m);
      }

      if (m)
         m_freem(m);
      return (error);
   }

restart:
   sblock (&so->so_rcv);
   INET_TRACE (INETM_IO,
    ("INET:soreceive sbcc %d soerror %d so_state %d *datalen %d\n",
    so->so_rcv.sb_cc, so->so_error, so->so_state, *datalen));

   /* If no data is ready, see if we should wait or return */
   if (so->so_rcv.sb_cc == 0) 
   {
      if (so->so_error) 
      {
         error = so->so_error;
         so->so_error = 0;
         goto release;
      }
      if (so->so_state & SS_CANTRCVMORE)
         goto release;
      if ((so->so_state & SS_ISCONNECTED) == 0 &&
          (so->so_proto->pr_flags & PR_CONNREQUIRED)) 
      {
         error = ENOTCONN;
         goto release;
      }
      if (*datalen == 0)
         goto release;
      if ((so->so_state & SS_NBIO) || (flags & MSG_DONTWAIT)) 
      {
         error = EWOULDBLOCK;
         goto release;
      }
      sbunlock(&so->so_rcv);
      sbwait(&so->so_rcv);
      goto restart;
   }
   m = so->so_rcv.sb_mb;
   if (m == 0)
      panic("sorecv 1");
   nextrecord = m->m_act;
   if (pr->pr_flags & PR_ADDR) 
   {
      if (m->m_type != MT_SONAME) 
      {
         dprintf ("sorecv:type %d not nam", m->m_type);
         panic("sorecv 2");
      }
      if (flags & MSG_PEEK) 
      {
         if (aname)
            *aname = m_copy (m, 0, m->m_len);
         m = m->m_next;
      } else 
      {
         sbfree (&so->so_rcv, m);
         if (aname) 
         {
            *aname = m;
            m = m->m_next;
            (*aname)->m_next = 0;
            so->so_rcv.sb_mb = m;
         } else 
         {
            MFREE(m, so->so_rcv.sb_mb);
            m = so->so_rcv.sb_mb;
         }
         if (m)
            m->m_act = nextrecord;
      }
   }
   moff = 0;
   offset = 0;
   while (m && (*datalen > 0) && (error == 0))
   {
      if (m->m_type != MT_RXDATA && m->m_type != MT_HEADER)
         panic("sorecv 3");
      len = *datalen;
      so->so_state &= ~SS_RCVATMARK;
      if (so->so_oobmark && (len > (int)(so->so_oobmark - offset)))
         len = (int)(so->so_oobmark - offset);
      if (len > (int)(m->m_len - moff))
         len = m->m_len - moff;

      INET_TRACE (INETM_IO, ("INET: soreceive, so %lx %d bytes, flags %x\n",
                  so, len, flags));

      /*
       * Copy mbufs info user buffer, then free them.
       * Sockbuf must be consistent here (points to current mbuf,
       * it points to next record) when we drop priority;
       * we must note any additions to the sockbuf when we
       * block interrupts again.
       */

      MEMCPY(data, (mtod(m, char *) + moff), len);
      data += len;
      *datalen -= len;

      if (len == (int)(m->m_len - moff))
      {
         if (flags & MSG_PEEK) 
         {
            m = m->m_next;
            moff = 0;
         } else 
         {
            nextrecord = m->m_act;
            sbfree(&so->so_rcv, m);
            {
               MFREE(m, so->so_rcv.sb_mb);
               m = so->so_rcv.sb_mb;
            }
            if (m)
               m->m_act = nextrecord;
         }
      } else 
      {
         if (flags & MSG_PEEK)
            moff += len;
         else 
         {
            m->m_data += len;
            m->m_len -= len;
            so->so_rcv.sb_cc -= len;
         }
      }
      if (so->so_oobmark) 
      {
         if ((flags & MSG_PEEK) == 0) 
         {
            so->so_oobmark -= len;
            if (so->so_oobmark == 0) 
            {
               so->so_state |= SS_RCVATMARK;
               break;
            }
         } else
            offset += len;
      }
   }

   if ((flags & MSG_PEEK) == 0) 
   {
      if (m == 0)
         so->so_rcv.sb_mb = nextrecord;
      else if (pr->pr_flags & PR_ATOMIC)
         (void) sbdroprecord(&so->so_rcv);
      if (pr->pr_flags & PR_WANTRCVD && so->so_pcb)
      {
         so->so_req = PRU_RCVD;
         (*pr->pr_usrreq)(so, (struct mbuf *)0,
          (struct mbuf *)0);
      }
   }
release:
   sbunlock(&so->so_rcv);
   return (error);
}


/* FUNCTION: soshutdown()
 *
 * PARAM1: struct socket *    socket structure
 * PARAM2: int                shutdown action
 *                               0 = shutdown read half of connection
 *                               1 = shutdown write half of connection
 *                               2 = shutdown both halves of connection
 *
 * RETURNS: int               0 if successful, else error code
 */
int
soshutdown(struct socket *so, int how)
{
   how++;   /* convert 0,1,2 into 1,2,3 */
   if (how & 1)   /* caller wanted READ or BOTH */
      sorflush(so);

   if (how & 2)   /* caller wanted WRITE or BOTH */
   {
      sbflush(&so->so_snd); /* flush the socket send queue */
      so->so_req = PRU_SHUTDOWN;
      return ((*so->so_proto->pr_usrreq)(so, (struct mbuf *)0, (struct mbuf *)0));
   }

   return 0;
}



/* FUNCTION: sorflush()
 * 
 * PARAM1: struct socket *    socket structure
 *
 * RETURNS: none
 *
 * Closes the "read" half of the socket connection. No more data
 * can be received on the socket, and any data currently in the 
 * socket receive buffer is discarded. Wakeup any processes waiting
 * on the socket.
 */
void
sorflush(struct socket * so)
{
   struct sockbuf *sb =  &so->so_rcv;
   int   s;

   sblock(sb);
   socantrcvmore(so);
   sbunlock(sb);
   sbrelease(sb);
   MEMSET((char *)sb, 0, sizeof (*sb));
   s = so->so_error;
   so->so_error = ESHUTDOWN;
   sorwakeup(so);
   so->so_error = s;
}



/* FUNCTION: sosetopt()
 * 
 * PARAM1: struct socket *so
 * PARAM2: int optname
 * PARAM3: void *arg
 *
 * RETURNS: 
 */

int
sosetopt(struct socket * so, 
   int   optname,
   void *   arg)
{
   int   error =  0;

   switch (optname) 
   {
   case SO_LINGER:
      so->so_linger = (short)((struct linger *)arg)->l_linger;
      /* fall thru... */

   case SO_KEEPALIVE:
   case SO_DONTROUTE:
   case SO_BROADCAST:
   case SO_REUSEADDR:
   case SO_OOBINLINE:
   case SO_TCPSACK:
   case SO_NOSLOWSTART:
#ifdef SUPPORT_SO_FULLMSS
   case SO_FULLMSS:
#endif
      if (*(int *)arg) 
         so->so_options |= optname;
      else
         so->so_options &= ~optname;
      break;

#ifdef TCP_BIGCWND
   case SO_BIGCWND:
      /* Set Large initial congestion window on this socket. This should
       * only be done on TCP sockets, and only before they are opened.
       */
      if(so->so_type != SOCK_STREAM)
         return EINVAL;
      if (*(int *)arg)
      {
         so->so_options |= optname;
         default_cwnd = TCP_MSS * (*(int *)arg);
      }
      else
         so->so_options &= ~optname;    
      break;
#endif /* TCP_BIGCWND */

   case SO_SNDBUF:
   case SO_RCVBUF:
      if (sbreserve(optname == SO_SNDBUF ?
          &so->so_snd : &so->so_rcv,
          (u_long) * (int *)arg) == 0) 
      {
         error = ENOBUFS;
         goto bad;
      }
      break;

   case SO_RCVTIMEO:
      so->so_rcv.sb_timeo = *(short *)arg;
      break;

   case SO_NBIO:     /* set socket into NON-blocking mode */
      so->so_state |= SS_NBIO;
      break;

   case SO_BIO:   /* set socket into blocking mode */
      so->so_state &= ~SS_NBIO;
      break;

   case SO_NONBLOCK:    /* set blocking mode according to arg */
      /* sanity check the arg parameter */
      if (!arg)
      {
         error = ENP_PARAM;
         break;
      }
      /* if contents of integer addressed by arg are non-zero */
      if (*(int *) arg)
         so->so_state |= SS_NBIO;   /* set non-blocking mode */
      else
         so->so_state &= ~SS_NBIO;  /* set blocking mode */
      break;

#ifdef IP_MULTICAST

   case IP_MULTICAST_IF:
   case IP_MULTICAST_TTL:
   case IP_MULTICAST_LOOP:
   case IP_ADD_MEMBERSHIP:
   case IP_DROP_MEMBERSHIP:
      error = ip_setmoptions(optname, so, arg);
      break;

#endif   /* IP_MULTICAST */

#ifdef IP_RAW

   case IP_HDRINCL:
      /* try to make sure that the argument pointer is valid */
      if (arg == NULL)
      {
         error = ENP_PARAM;
         break;
      }
      /* set the socket option flag based on the pointed-to argument */
      if (*(int *)arg)
         so->so_options |= SO_HDRINCL;
      else
         so->so_options &= ~SO_HDRINCL;
      break;

#endif   /* IP_RAW */

#ifdef DO_DELAY_ACKS
   case TCP_ACKDELAYTIME:
   case TCP_NOACKDELAY:
   {
      struct inpcb * inp;
      struct tcpcb * tp;

      if(so->so_type != SOCK_STREAM)
      {
         error = EINVAL;
         break;
      }
      inp = (struct inpcb *)(so->so_pcb);
      tp = intotcpcb(inp);
      if(!tp)
      {
         error = ENOTCONN;
         break;
      }
      if(optname == TCP_ACKDELAYTIME)
         /* Convert MS to TPS */
         tp->t_delacktime = (((*(int*)(arg))*TPS)/1000);
      else
         tp->t_delacktime = 0;

      break;
   }
#endif   /* DO_DELAY_ACKS */

   case TCP_NODELAY:
   {
      struct inpcb * inp;
      struct tcpcb * tp;

      if(so->so_type != SOCK_STREAM)
      {
         error = EINVAL;
         break;
      }
      inp = (struct inpcb *)(so->so_pcb);
      tp = intotcpcb(inp);
      if(!tp)
      {
         error = ENOTCONN;
         break;
      }
      /* try to make sure that the argument pointer is valid */
      if (arg == NULL)
      {
         error = ENP_PARAM;
         break;
      }
      /* if contents of integer addressed by arg are non-zero */
      if (*(int *) arg)
         tp->t_flags |= TF_NODELAY;   /* Disable Nagle Algorithm */
      else
         tp->t_flags &= ~TF_NODELAY;  /* Enable Nagle Algorithm */

      break;
   }

#ifdef TCP_ZEROCOPY
   case SO_CALLBACK:    /* install ZERO_COPY callback routine */
      /*
       * This generates warnings on many compilers, even when it's 
       * doing the right thing. Just make sure the "void * arg" data
       * pointer converts properly to a function pointer.
       */
      so->rx_upcall = (int (*)(struct socket *, void*, int))arg;
      break;
#endif   /* TCP_ZEROCOPY */

   case SO_MAXMSG:
   case TCP_MAXSEG:
   {
      struct inpcb * inp;
      struct tcpcb * tp;

      if(so->so_type != SOCK_STREAM)
      {
         error = EINVAL;
         break;
      }
      inp = (struct inpcb *)(so->so_pcb);
      tp = intotcpcb(inp);
      if(!tp)
      {
         error = ENOTCONN;
         break;
      }
      if (tp->t_state != TCPS_CLOSED)
      {
         error = EINVAL;
         break;
      }
      tp->t_maxseg = *(int*)(arg);    /* set TCP MSS */
      tp->t_flags |= TF_MAXSEG;   /* mark as user set max seg */
      break;
   }
   default:
      error = ENOPROTOOPT;
      break;
   }
bad:
   return (error);
}



/* FUNCTION: sogetopt()
 * 
 * PARAM1: struct socket *so
 * PARAM2: int optname
 * PARAM3: void *val
 *
 * RETURNS: 
 */

int
sogetopt(struct socket * so, 
   int   optname,
   void *   val)
{
   int   error =  0;

   /* sanity check the val parameter */
   if (!val)
   {
      return ENP_PARAM;
   }

   switch (optname) 
   {
   case SO_MYADDR:
      /* Get my IP address. */
      if (so->so_state & SS_ISCONNECTED)
      {
         *(u_long *)val = so->so_pcb->ifp->n_ipaddr;
      }
      else  /* not connected, use first iface */
         *(u_long *)val = nets[0]->n_ipaddr;
      break;
   case SO_LINGER:
      {
         struct linger *   l  =  (struct  linger *)val;
         l->l_onoff = so->so_options & SO_LINGER;
         l->l_linger = so->so_linger;
      }
      break;

   case SO_KEEPALIVE:
   case SO_OOBINLINE:
   case SO_DONTROUTE:
   case SO_REUSEADDR:
   case SO_BROADCAST:
   case SO_TCPSACK:
      *(int *)val = so->so_options & optname;
      break;

   case SO_SNDLOWAT:
      *(int *)val = (int)so->so_snd.sb_lowat;
      break;

   case SO_RCVLOWAT:
      *(int *)val = (int)so->so_rcv.sb_lowat;
      break;

   case SO_SNDBUF:
      *(int *)val = (int)so->so_snd.sb_hiwat;
      break;

   case SO_RCVBUF:
      *(int *)val = (int)so->so_rcv.sb_hiwat;
      break;

   case SO_RXDATA:   /* added, JB */
      *(int *)val = (int)so->so_rcv.sb_cc;
      break;

   case SO_TXDATA:   /* added for rel 1.8 */
      *(int *)val = (int)so->so_snd.sb_cc;
      break;

   case SO_TYPE:
      *(int *)val = so->so_type;
      break;

   case SO_ERROR:
      *(int *)val = so->so_error;
      so->so_error = 0;
      break;

   case SO_MAXMSG:
   case TCP_MAXSEG:
   {
      struct inpcb * inp;
      struct tcpcb * tp;

      if(so->so_type != SOCK_STREAM)
      {
         error = EINVAL;
         break;
      }
      inp = (struct inpcb *)(so->so_pcb);
      tp = intotcpcb(inp);
      if(!tp)
      {
         error = ENOTCONN;
         break;
      }
      *(int *)val = tp->t_maxseg;     /* Fill in TCP MSS for current socket */
      break;
   }
 
   case SO_SNDTIMEO:
      *(short*)val = so->so_snd.sb_timeo;
      break;

   case SO_RCVTIMEO:
      *(short*)val = so->so_rcv.sb_timeo;
      break;

   case SO_HOPCNT:
      *(int *)val = so->so_hopcnt;
      break;

   case SO_NONBLOCK:    /* get blocking mode according to val */
      /* if the non-blocking I/O bit is set in the state */
      if (so->so_state & SS_NBIO)
         *(int *)val = 1;   /* return 1 in val */
      else
         *(int *)val = 0;     /* return 0 in val */
      break;

#ifdef IP_MULTICAST

   case IP_MULTICAST_IF:
   case IP_MULTICAST_TTL:
   case IP_MULTICAST_LOOP:
      error = ip_getmoptions(optname, so, val);
      break;

#endif   /* IP_MULTICAST */

#ifdef IP_RAW

   case IP_HDRINCL:
      /* indicate based on header-include flag in socket state */
      if (so->so_options & SO_HDRINCL)
         *(int *)val = 1;
      else
         *(int *)val = 0;
      break;

#endif   /* IP_RAW */

#ifdef DO_DELAY_ACKS
   case TCP_ACKDELAYTIME:
   case TCP_NOACKDELAY:
   {
      struct inpcb * inp;
      struct tcpcb * tp;

      if(so->so_type != SOCK_STREAM)
      {
         error = EINVAL;
         break;
      }
      inp = (struct inpcb *)(so->so_pcb);
      tp = intotcpcb(inp);
      if (!tp)
      {
         error = ENOTCONN;
         break;
      }
      if (optname == TCP_ACKDELAYTIME)
      {
         /* Convert TPS to MS */
         *(int*)(val) = (tp->t_delacktime * 1000)/TPS;
      }
      else
      {
         if (tp->t_delacktime == 0)
            *(int *)val = 1;  /* NO ACK DELAY is Enabled */
         else
            *(int *)val = 0;  /* NO ACK DELAY is NOT Enabled */		 
      }

      break;
   }
#endif   /* DO_DELAY_ACKS */
      
   case TCP_NODELAY:
   {
      struct inpcb * inp;
      struct tcpcb * tp;

      if(so->so_type != SOCK_STREAM)
      {
         error = EINVAL;
         break;
      }
      inp = (struct inpcb *)(so->so_pcb);
      tp = intotcpcb(inp);
      if (!tp)
      {
         error = ENOTCONN;
         break;
      }
      /* try to make sure that the argument pointer is valid */
      if (val == NULL)
      {
         error = ENP_PARAM;
         break;
      }
      /* if contents of integer addressed by arg are non-zero */
      if (tp->t_flags & TF_NODELAY)
         *(int *)val = 1;  /* Nagle Algorithm is Enabled */
      else
         *(int *)val = 0;  /* Nagle Algorithm is NOT Enabled */

      break;
   }

   default:
      return ENOPROTOOPT;
   }
   return error;     /* no error */
}



/* FUNCTION: sohasoutofband()
 * 
 * PARAM1: struct socket *so
 *
 * RETURNS: 
 */

void
sohasoutofband(struct socket * so)
{
   so->so_error = EHAVEOOB;   /* WILL be picked up by the socket */
   sorwakeup (so);
}



#endif /* INCLUDE_TCP */

/* end of file socket.c */

