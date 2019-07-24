/*
 * FILENAME: socket2.c
 *
 * Copyright 1997-2007 By InterNiche Technologies, Inc. All rights reserved.
 *
 * Primitive routines for operating on sockets and socket buffers,
 * Procedures to manipulate state flags of socket
 * and do appropriate wakeups.  Normal sequence from the
 * active (originating) side is that soisconnecting() is
 * called during processing of connect() call,
 * resulting in an eventual call to soisconnected() if/when the
 * connection is established.  When the connection is torn down
 * soisdisconnecting() is called during processing of disconnect() call,
 * and soisdisconnected() is called when the connection to the peer
 * is totally severed.  The semantics of these routines are such that
 * connectionless protocols can call soisconnected() and soisdisconnected()
 * only, bypassing the in-progress calls when setting up a ``connection''
 * takes no time.
 *
 * From the passive side, a socket is created with
 * two queues of sockets: so_q0 for connections in progress
 * and so_q for connections already made and awaiting user acceptance.
 * As a protocol is preparing incoming connections, it creates a socket
 * structure queued on so_q0 by calling sonewconn().  When the connection
 * is established, soisconnected() is called, and transfers the
 * socket structure to so_q, making it available to accept().
 * 
 * If a socket is closed with sockets on either
 * so_q0 or so_q, these sockets are dropped.
 *
 * If higher level protocols are implemented in
 * the kernel, the wakeups done here will sometimes
 * cause software-interrupt process scheduling.
 *
 *
 * MODULE: INET
 *
 * ROUTINES: soisconnecting(), soisconnected(), 
 * ROUTINES: soisdisconnecting(), soisdisconnected(), sonewconn(), 
 * ROUTINES: soqinsque(), soqremque(), socantsendmore(), socantrcvmore(), 
 * ROUTINES: sbselqueue(), sbwait(), sbwakeup(), sowakeup(), soreserve(), 
 * ROUTINES: sbreserve(), sbrelease(), sbappend(), sbappendrecord(), 
 * ROUTINES: sbappendaddr(), sbcompress(), sbflush(), sbdrop(), sbdropend(), 
 * ROUTINES: sbdroprecord(),
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
#include "protosw.h"

#ifdef INCLUDE_TCP  /* include/exclude whole file at compile time */

/* Initial TCP window sizes, etc. */
extern   u_long   tcp_sendspace;
extern   u_long   tcp_recvspace;
extern   u_char   default_wind_scale;

/* FUNCTION: soisconnecting()
 *
 * Called by the protocol layers when a socket is connecting. Sets
 * the required flags and does the wakeup() call.
 *
 * PARAM1: struct socket *so
 *
 * RETURNS: 
 */

void
soisconnecting(struct socket * so)
{
   so->so_state &= ~(SS_ISCONNECTED|SS_ISDISCONNECTING);
   so->so_state |= SS_ISCONNECTING;
   tcp_wakeup ((char *)&so->so_timeo);
}



/* FUNCTION: soisconnected()
 * 
 * Called by the protocol layers when a socket has connected. Sets
 * the required flags and does the wakeup() call.
 *
 * PARAM1: struct socket *so
 *
 * RETURNS: 
 */

void
soisconnected(struct socket * so)
{
   struct socket *   head  =  so->so_head;

   if (head) 
   {
      if (soqremque(so, 0) == 0)
         panic("soisconnected");
      soqinsque(head, so, 1);
      sorwakeup(head);
      tcp_wakeup ((char *)&head->so_timeo);
   }

   so->so_state &= ~(SS_ISCONNECTING|SS_ISDISCONNECTING);
   so->so_state |= SS_ISCONNECTED;
   so->so_error = 0;
   tcp_wakeup  ((char *)&so->so_timeo);
   sorwakeup (so);
   sowwakeup (so);
}



/* FUNCTION: soisdisconnecting()
 * 
 * Called by the protocol layers when a socket is disconnecting. Sets
 * the required flags and does the wakeup() calls.
 *
 * PARAM1: struct socket *so
 *
 * RETURNS: 
 */

void
soisdisconnecting(struct socket * so)
{
   so->so_state &= ~SS_ISCONNECTING;
   so->so_state |= (SS_ISDISCONNECTING|SS_CANTRCVMORE|SS_CANTSENDMORE);
   tcp_wakeup  ((char *)&so->so_timeo);   
   sowwakeup (so);
   sorwakeup (so);
}



/* FUNCTION: soisdisconnected()
 * 
 * Called by the protocol layers when a socket is disconnected. Sets
 * the required flags and does the wakeup() calls.
 * 
 * PARAM1: struct socket *so
 *
 * RETURNS: 
 */

void
soisdisconnected(struct socket * so)
{
   if (so->so_state & SS_ISCONNECTING)
      so->so_state |= SS_WASCONNECTING;
   so->so_state &= ~(SS_ISCONNECTING|SS_ISCONNECTED|SS_ISDISCONNECTING);
   so->so_state |= (SS_CANTRCVMORE|SS_CANTSENDMORE);
   tcp_wakeup ((char *)&so->so_timeo);
   sowwakeup (so);
   sorwakeup (so);
}


/* FUNCTION: sonewconn()
 * 
 * When an attempt at a new connection is noted on a socket
 * which accepts connections, sonewconn is called.  If the
 * connection is possible (subject to space constraints, etc.)
 * then we allocate a new structure, properly linked into the
 * data structure of the original socket, and return this.
 *
 * PARAM1: struct socket *head
 *
 * RETURNS: 
 */

struct socket *   
sonewconn(struct socket * head)
{
   struct socket *   so;
   int rc;  

   if (head->so_qlen + head->so_q0len > 3 * head->so_qlimit / 2)
      goto bad;
   if ((so = SOC_ALLOC (sizeof (*so))) == NULL)
      goto bad;
   so->next = NULL;
   putq(&soq,(qp)so);      /* Place newly created socket in a queue */
   so->so_type = head->so_type;
   so->so_options = head->so_options &~ (unshort)SO_ACCEPTCONN;
   so->so_linger = head->so_linger;
   so->so_state = head->so_state | (unshort)SS_NOFDREF;
   so->so_proto = head->so_proto;
   so->so_timeo = head->so_timeo;
   so->so_rcv.sb_hiwat = (u_int)tcp_recvspace;
   so->so_snd.sb_hiwat = (u_int)tcp_sendspace;
   soqinsque (head, so, 0);
   so->so_req = PRU_ATTACH;
   so->so_domain = head->so_domain;

   if ((*so->so_proto->pr_usrreq)(so, (struct mbuf *)0, (struct mbuf *)0))
      goto bad2;
   if (so_evtmap)
   {
      rc = (*so_evtmap_create) (so);                       
      if (rc != 0) goto bad2;
      so->owner = head->owner;
   }
   return (so);
   
bad2:
   (void) soqremque (so, 0);
   qdel(&soq, so);   /* Delete the socket entry from the queue */
   SOC_FREE(so);  /* Free the socket structure */
bad:
   return ((struct socket *)0);
}



/* FUNCTION: soqinsque()
 * 
 * PARAM1: struct socket * head
 * PARAM2: struct socket * so
 * PARAM3: int q
 *
 * RETURNS: 
 */

void
soqinsque(struct socket * head, 
   struct socket *   so,
   int   q)
{
   so->so_head = head;
   if (q == 0) 
   {
      head->so_q0len++;
      so->so_q0 = head->so_q0;
      head->so_q0 = so;
   } else 
   {
      head->so_qlen++;
      so->so_q = head->so_q;
      head->so_q = so;
   }
}



/* FUNCTION: soqremque()
 * 
 * PARAM1: struct socket * so
 * PARAM2: int q
 *
 * RETURNS: 
 */

int
soqremque(struct socket * so, int q)
{
   struct socket *   head, *  prev, *  next;

   head = so->so_head;
   prev = head;
   for (;;) 
   {
      next = q ? prev->so_q : prev->so_q0;
      if (next == so)
         break;
      if (next == head)
         return (0);
      prev = next;
   }
   if (q == 0) 
   {
      prev->so_q0 = next->so_q0;
      head->so_q0len--;
   } else 
   {
      prev->so_q = next->so_q;
      head->so_qlen--;
   }
   next->so_q0 = next->so_q = 0;
   next->so_head = 0;
   return 1;
}


/* FUNCTION: socantsendmore()
 * 
 * Socantsendmore indicates that no more data will be sent on the
 * socket; it would normally be applied to a socket when the user
 * informs the system that no more data is to be sent, by the protocol
 * code (in case PRU_SHUTDOWN).  Socantrcvmore indicates that no more data
 * will be received, and will normally be applied to the socket by a
 * protocol when it detects that the peer will send no more data.
 * Data queued for reading in the socket may yet be read.
 *
 * PARAM1: struct socket *so
 *
 * RETURNS: 
 */

void
socantsendmore(struct socket * so)
{
   so->so_state |= SS_CANTSENDMORE;
   sowwakeup(so);
}



/* FUNCTION: socantrcvmore()
 * 
 * PARAM1: struct socket * so
 *
 * RETURNS: 
 */

void
socantrcvmore(struct socket * so)
{
   so->so_state |= SS_CANTRCVMORE;
   sorwakeup(so);
}

/*
 * Socket select/wakeup routines.
 */


/* FUNCTION: sbselqueue()
 * 
 * Queue a process for a select on a socket buffer. 
 *
 * PARAM1: struct sockbuf * sb
 *
 * RETURNS: 
 */

void
sbselqueue(struct sockbuf * sb)
{
   sb->sb_flags |= SB_SEL;
}


/* FUNCTION: sbwait()
 * 
 * Wait for data to arrive at/drain from a socket buffer.
 *
 * PARAM1: struct sockbuf *sb
 *
 * RETURNS: 
 */

void
sbwait(struct sockbuf * sb)
{
   sb->sb_flags |= SB_WAIT;
   tcp_sleep ((char *)&sb->sb_cc);
   sb->sb_flags &= ~SB_WAIT;
}



/* FUNCTION: sbwakeup()
 * 
 *
 * Wakeup processes waiting on a socket buffer.
 *
 * PARAM1: struct sockbuf *sb
 *
 * RETURNS: 
 */

void
sbwakeup(struct socket * so, struct sockbuf * sb)
{
   if (sb->sb_flags & SB_SEL) 
   {
      select_wait = 0;
#ifndef SOCK_MAP_EVENTS
      tcp_wakeup ((char *)&select_wait);
#else
      tcp_wakeup2 (so->owner);
#endif      
      sb->sb_flags &= ~SB_SEL;
   }

#ifdef SOCK_WAKEALWAYS  /* Always wake the socket? */
   /* Systems Like Green Hills Integrity RTOS, need to process socket
      input even if looks like no one is blocked on the socket */
   tcp_wakeup ((char *)&sb->sb_cc); /* signal wake on socket */
#else /* older BSD style code - only call tcp_wakeup if blocked */
   if (sb->sb_flags & SB_WAIT)   /* is sockbuf's WAIT flag set? */
   {
      tcp_wakeup ((char *)&sb->sb_cc);   /* call port wakeup routine */
   }
#endif   /* SOCK_WAKEALWAYS */
}


/*
 * Socket buffer (struct sockbuf) utility routines.
 *
 * Each socket contains two socket buffers: one for sending data and
 * one for receiving data.  Each buffer contains a queue of mbufs,
 * information about the number of mbufs and amount of data in the
 * queue, and other fields allowing select() statements and notification
 * on data availability to be implemented.
 *
 * Data stored in a socket buffer is maintained as a list of records.
 * Each record is a list of mbufs chained together with the m_next
 * field.  Records are chained together with the m_act field. The upper
 * level routine soreceive() expects the following conventions to be
 * observed when placing information in the receive buffer:
 *
 * 1. If the protocol requires each message be preceded by the sender's
 *   name, then a record containing that name must be present before
 *   any associated data (mbuf's must be of type MT_SONAME).
 * 2. If the protocol supports the exchange of ``access rights'' (really
 *   just additional data associated with the message), and there are
 *   ``rights'' to be received, then a record containing this data
 *   should be present (mbuf's must be of type MT_RIGHTS).
 * 3. If a name or rights record exists, then it must be followed by
 *   a data record, perhaps of zero length.
 *
 * Before using a new socket structure it is first necessary to reserve
 * buffer space to the socket, by calling sbreserve().  This should commit
 * some of the available buffer space in the system buffer pool for the
 * socket (currently, it does nothing but enforce limits).  The space
 * should be released by calling sbrelease() when the socket is destroyed.
 */


/* FUNCTION: soreserve()
 * 
 * PARAM1: struct socket *so
 * PARAM2: u_long sndcc
 * PARAM3: u_long rcvcc
 *
 * RETURNS: 
 */

int
soreserve(struct socket * so, 
   u_long   sndcc, 
   u_long   rcvcc)
{
   if (sbreserve(&so->so_snd, sndcc) == 0)
      goto bad;
   if (sbreserve(&so->so_rcv, rcvcc) == 0)
      goto bad2;
   return (0);
bad2:
   sbrelease(&so->so_snd);
bad:
   return (ENOBUFS);
}


/* FUNCTION: sbreserve()
 * 
 *
 * Allot mbufs to a sockbuf.
 * Attempt to scale cc so that mbcnt doesn't become limiting
 * if buffering efficiency is near the normal case.
 *
 * PARAM1: struct sockbuf *sb
 * PARAM2: u_long cc
 *
 * RETURNS: 
 */

int
sbreserve(struct sockbuf * sb, u_long cc)
{
#ifdef COMPILER_32BIT
   if (cc > (u_long)SB_MAX * CLBYTES / (2 * MSIZE + CLBYTES))
      return (0);
#endif
   sb->sb_hiwat = cc;
   sb->sb_mbmax = MIN(cc * 2, SB_MAX);
   return (1);
}


/* FUNCTION: sbrelease()
 *
 * Free mbufs held by a socket, and reserved mbuf space.
 *
 * 
 * PARAM1: struct sockbuf *sb
 *
 * RETURNS: 
 */

void
sbrelease(struct sockbuf * sb)
{
   sbflush(sb);
   sb->sb_hiwat = sb->sb_mbmax = 0;
}

/*
 * Routines to add and remove
 * data from an mbuf queue.
 *
 * The routines sbappend() or sbappendrecord() are normally called to
 * append new mbufs to a socket buffer, after checking that adequate
 * space is available, comparing the function sbspace() with the amount
 * of data to be added.  sbappendrecord() differs from sbappend() in
 * that data supplied is treated as the beginning of a new record.
 * To place a sender's address, optional access rights, and data in a
 * socket receive buffer, sbappendaddr() should be used.  To place
 * access rights and data in a socket receive buffer, sbappendrights()
 * should be used.  In either case, the new data begins a new record.
 * Note that unlike sbappend() and sbappendrecord(), these routines check
 * for the caller that there will be enough space to store the data.
 * Each fails if there is not enough space, or if it cannot find mbufs
 * to store additional information in.
 *
 * Reliable protocols may use the socket send buffer to hold data
 * awaiting acknowledgement.  Data is normally copied from a socket
 * send buffer in a protocol with m_copy for output to a peer,
 * and then removing the data from the socket buffer with sbdrop()
 * or sbdroprecord() when the data is acknowledged by the peer.
 */


/* FUNCTION: sbappend()
 * 
 * Append mbuf chain m to the last record in the
 * socket buffer sb.  The additional space associated
 * the mbuf chain is recorded in sb.  Empty mbufs are
 * discarded and mbufs are compacted where possible.
 *
 * PARAM1: struct sockbuf *sb
 * PARAM2: struct mbuf *m
 *
 * RETURNS: 
 */

void
sbappend(struct sockbuf * sb, struct mbuf * m)
{
   struct mbuf *  n;

   if (m == 0)
      return;
   ENTER_CRIT_SECTION(sb);
   if ((n = sb->sb_mb) != NULL) 
   {
      while (n->m_act)
         n = n->m_act;
      while (n->m_next)
         n = n->m_next;
   }
   sbcompress(sb, m, n);
   EXIT_CRIT_SECTION(sb);
}


/* FUNCTION: sbappendrecord()
 *
 * As above, except the mbuf chain begins a new record.
 * 
 * PARAM1: struct sockbuf *sb
 * PARAM2: struct mbuf *m0
 *
 * RETURNS: 
 */

void
sbappendrecord(struct sockbuf * sb, 
   struct mbuf *  m0)
{
   struct mbuf *  m;

   if (m0 == 0)
      return;
   ENTER_CRIT_SECTION(sb);    /* protect so_rcv operations */
   if ((m = sb->sb_mb) != NULL)
      while (m->m_act)
      m = m->m_act;
   /*
    * Put the first mbuf on the queue.
    * Note this permits zero length records.
    */
   sballoc(sb, m0);
   if (m)
      m->m_act = m0;
   else
      sb->sb_mb = m0;
   m = m0->m_next;
   m0->m_next = 0;
   sbcompress(sb, m, m0);
   EXIT_CRIT_SECTION(sb);
}



/* FUNCTION: sbappendaddr()
 *
 * Append address and data, and optionally, rights
 * to the receive queue of a socket.  Return 0 if
 * no space in sockbuf or insufficient mbufs.
 *
 * 
 * PARAM1: struct sockbuf *sb
 * PARAM2: struct sockaddr *asa
 * PARAM3: struct mbuf *m0
 *
 * RETURNS: 
 */

int
sbappendaddr(struct sockbuf * sb, 
   struct sockaddr * asa,
   struct mbuf *  m0)
{
   struct mbuf *  m, *  n;
   int   space =  sizeof   (*asa);

   ENTER_CRIT_SECTION(sb);
   for (m = m0; m; m = m->m_next)
      space += m->m_len;
   if (space > (int)sbspace(sb))
   {
      EXIT_CRIT_SECTION(sb);
      return (0);
   }
   if ((m = m_getwithdata (MT_SONAME, sizeof (struct sockaddr))) == NULL)
   {
      EXIT_CRIT_SECTION(sb);
      return 0;
   }
   *mtod(m, struct sockaddr *) = *asa;
   m->m_len = sizeof (*asa);
   sballoc (sb, m);
   if ((n = sb->sb_mb) != NULL) 
   {
      while (n->m_act)
         n = n->m_act;
      n->m_act = m;
   } else
      sb->sb_mb = m;
   if (m->m_next)
      m = m->m_next;
   if (m0)
      sbcompress(sb, m0, m);

   EXIT_CRIT_SECTION(sb);
   return (1);
}



/* FUNCTION: sbcompress()
 *
 * Compress mbuf chain m into the socket
 * buffer sb following mbuf n.  If n
 * is null, the buffer is presumed empty.
 * 
 * PARAM1: struct sockbuf *sb
 * PARAM2: struct mbuf *m
 * PARAM3: struct mbuf *n
 *
 * RETURNS: 
 */

void
sbcompress(struct sockbuf * sb, 
   struct mbuf *  m,
   struct mbuf *  n)
{

   while (m) 
   {
      if (m->m_len == 0) 
      {
         m = m_free(m);
         continue;
      }
      if (m->m_type != MT_RXDATA && 
          m->m_type != MT_TXDATA && 
          m->m_type != MT_SONAME) 
      {
         dprintf ("sbcomp:bad type %d\n", m->m_type);
         panic ("sbcomp:bad");
      }
      /* If there is room for all the data in M in N, then
       * just copy the data to N.  Note that sbdrop will 
       * increment the n->m_data pointer, so that we must
       * correct n->m_memsz.
       */
      if ( n && (sb->sb_flags & SB_MBCOMP) &&
          ((n->m_len + m->m_len) <
          n->m_memsz - (n->m_data - n->m_base) ) ) 
      {
         MEMCPY(n->m_data+n->m_len, m->m_data, m->m_len);
         sballoc(sb, m);
         n->m_len += m->m_len;
         m = m_free(m);
         continue;
      }
      sballoc(sb, m);
      if (n)
         n->m_next = m;
      else
         sb->sb_mb = m;
      n = m;
      m = m->m_next;
      n->m_next = 0;
   }
}



/* FUNCTION: sbflush()
 *
 * Free all mbufs in a sockbuf.
 *
 * 
 * PARAM1: struct sockbuf *sb
 *
 * RETURNS: 
 */

void
sbflush(struct sockbuf * sb)
{
   ENTER_CRIT_SECTION(sb);
   if (sb->sb_flags & SB_LOCK)
      panic("sbflush");
   while ((sb->sb_mbcnt) || (sb->sb_cc))
      sbdrop (sb, (int)sb->sb_cc);
   EXIT_CRIT_SECTION(sb);
}



/* FUNCTION: sbdrop()
 *
 * Drop data from (the front of) a sockbuf.
 * 
 * PARAM1: struct sockbuf *sb
 * PARAM2: int len
 *
 * RETURNS: 
 */

void
sbdrop(struct sockbuf * sb, int len)
{
   struct mbuf *  m, *  mn;
   struct mbuf *  next;

   ENTER_CRIT_SECTION(sb);
   if ((m = sb->sb_mb) != NULL)
      next = m->m_act;
   else
      next = NULL;
   while (len > 0) 
   {
      if (m == 0) 
      {
         if (next == 0)
            panic("sbdrop");
         m = next;
         next = m->m_act;
         continue;
      }
      if (m->m_len > (unsigned)len) 
      {
         m->m_len -= len;
         m->m_data += len;
         sb->sb_cc -= len;
         break;
      }
      len -= m->m_len;
      sbfree (sb, m);
      MFREE(m, mn);
      m = mn;
   }
   while (m && m->m_len == 0) 
   {
      sbfree(sb, m);
      MFREE(m, mn);
      m = mn;
   }
   if (m) 
   {
      sb->sb_mb = m;
      m->m_act = next;
   } else
      sb->sb_mb = next;
   EXIT_CRIT_SECTION(sb);
}



/* FUNCTION: sbdropend()
 *
 * Drop data from the end of a socket buffer
 * 
 * PARAM1: struct sockbuf *sb
 * PARAM2: struct mbuf *m
 *
 * RETURNS: 
 */

void
sbdropend(struct sockbuf * sb, struct mbuf * m)
{
   struct mbuf *  nmb, *   pmb;
   int   len;
   ENTER_CRIT_SECTION(sb);
   len = mbuf_len(m);
   if (len > 0)
      m_adj(sb->sb_mb, -len); /* Adjust the lengths of the mbuf chain */
   nmb = sb->sb_mb;
   pmb = NULL;
   if (sb->sb_mb->m_len == 0)
      sb->sb_mb = NULL;
   while (nmb && (nmb->m_len !=0))  /* Release mbufs that have a 0 len */
   {
      pmb = nmb;  /* Remember previous */
      nmb = nmb->m_next;    
   }
   if (nmb && (nmb->m_len == 0)) /* Assume once 0 len found, all the rest */
   {                             /* are zeroes */ 
      if (pmb != NULL)
         pmb->m_next = NULL;
      m_freem(nmb);            
   }
   sb->sb_cc -= len;       /* Do a sbfree using the len */
   EXIT_CRIT_SECTION(sb);
}


/* FUNCTION: sbdroprecord()
 *
 * Drop a record off the front of a sockbuf
 * and move the next record to the front.
 *
 * 
 * PARAM1: struct sockbuf *sb
 *
 * RETURNS: 
 */

void
sbdroprecord(struct sockbuf * sb)
{
   struct mbuf *  m, *  mn;

   ENTER_CRIT_SECTION(sb);
   m = sb->sb_mb;
   if (m)
   {  sb->sb_mb = m->m_act;
      do 
      {  sbfree(sb, m);
         MFREE(m, mn);
      } while ((m = mn) != NULL);
         }
   EXIT_CRIT_SECTION(sb);
}


#endif /* INCLUDE_TCP */

/* end of file socket2.c */

