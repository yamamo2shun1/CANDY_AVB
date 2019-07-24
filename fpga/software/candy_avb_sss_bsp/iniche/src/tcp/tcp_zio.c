/*
 * FILENAME: tcp_zio.c
 *
 * Copyright 1999- 2000 By InterNiche Technologies Inc. All rights reserved
 *
 * tcp_zio.x Socket zero-copy extensions for InterNIche TCP/IP 
 *
 * MODULE: INET
 *
 * ROUTINES: tcp_xout(), tcp_pktalloc(), tcp_pktfree(), 
 * ROUTINES: tcp_data_upcall(),
 *
 * PORTABLE: yes
 */


#include "ipport.h"

#ifdef INCLUDE_TCP  /* include/exclude whole file at compile time */
#ifdef TCP_ZEROCOPY  /* whole file can be ifdeffed out */

#include "tcpport.h"
#include "protosw.h"
#include "in_pcb.h"
#include "tcp.h"
#include "tcp_timr.h"
#include "tcp_var.h"


/* bitmask for connection state bits which determine if send/recv is OK */
#define  SO_IO_OK (SS_ISCONNECTED|SS_ISCONNECTING|SS_ISDISCONNECTING)



/* FUNCTION: tcp_xout()
 *
 * tcp_xout(so, pkt) - send a packet (previously allocated by a call 
 * to tcp_pktalloc()) on a socket. Packet is freed by tcp_xout()if 
 * OK, not freed if there's an error. Note: pkt->n_prot points to 
 * data to be sent. The TCP/IP layers will prepend headers to this 
 * data. Returns 0 if OK or one of the ENP error codes. If the error 
 * code is ENP_SEND_PENDING then the socket errno is set with a BSD 
 * error code, but the packet is queued on the socket's send buffer 
 * and so must not be freed by the application. 
 *
 * 
 * PARAM1: long s
 * PARAM2: PACKET pkt
 *
 * RETURNS: 
 */

int
tcp_xout(long s, PACKET pkt)  /* socket & packet to send */
{
   struct socket *   so;
   struct mbuf *  m;
   int   err;

   so = LONG2SO(s);  /* convert long to socket */

   if (!(so->so_state & SS_INUPCALL))
   {
      LOCK_NET_RESOURCE(NET_RESID);
   }

   if ((so->so_state & SO_IO_OK) != SS_ISCONNECTED)
   {
      err = ENP_BAD_STATE;   /* socket is not connected! */
      goto errexit;
   }

   /* make sure space in sockbuf is available */
   if ((so->so_snd.sb_cc + pkt->nb_plen) > so->so_snd.sb_hiwat)
   {
      /*
       * If there's received packets, try processing them. They may ACK some
       * data in the socket
       *
       * NOTE if your target needs to poll a network device
       * driver to fetch received packets, you can #define POLLED_RX in
       * ipport.h so that this will let the driver have some cycles 
       * to fetch the incoming packets and put them on the rcvdq.
       */
#ifndef POLLED_RX
      if (rcvdq.q_len)
#endif
      {
         UNLOCK_NET_RESOURCE(NET_RESID);
         tk_yield();    /* let the system spin once */
         LOCK_NET_RESOURCE(NET_RESID);
      }

      if ((so->so_state & SO_IO_OK) != SS_ISCONNECTED)
      {
         err = ENP_BAD_STATE;    /* socket was disconnected in tk_yield() */
         goto errexit;
      }

      /* still choked? */
      if ((so->so_snd.sb_cc + pkt->nb_plen) > so->so_snd.sb_hiwat)
      {
         err = ENP_RESOURCE;     /* not allowed, use t_send() */
         goto errexit;
      }
      tcpstat.tcps_zioacks++;   
   }
   IN_PROFILER(PF_TCP, PF_ENTRY);      /* measure time in TCP */

   m = m_getnbuf(MT_TXDATA, 0);
   if (m == (struct mbuf *)NULL)
   {
      err = ENP_NOBUFFER;     /* is this reasonable? */
      goto errexit;
   }

   m->pkt = pkt;
   m->m_base = m->m_data = pkt->nb_prot;
   m->m_memsz = pkt->nb_blen - HDRSLEN;
   m->m_len = pkt->nb_plen;

   sbappend(&so->so_snd, m);
   err = tcp_output((struct tcpcb *)(so->so_pcb->inp_ppcb));

   if (!(so->so_state & SS_INUPCALL))
   {
      UNLOCK_NET_RESOURCE(NET_RESID);
   }

   IN_PROFILER(PF_TCP, PF_EXIT);      /* measure time in TCP */

   if (err)
   {
      so->so_error =  err;
      return ENP_SEND_PENDING;
   }
   return 0;

errexit:
   /* return error (as return, not as socket error) */
   if (!(so->so_state & SS_INUPCALL))
   {
      UNLOCK_NET_RESOURCE(NET_RESID);
   }
   return err;
}



/* FUNCTION: tcp_pktalloc()
 *
 * tcp_alloc(int datasize) - request a data buffer for sending tcp 
 * data to tcp_xout(). Generally datasize must be smaller than the 
 * tcp data size of smallest device, usually about 1440 bytes. 
 * Packets allocated with this should be freed either implicitly by a 
 * successful send call to tcp_xout(), or explicitly by a call to 
 * tcp_pktfree(). Returns a PACKET structure if OK, else NULL. 
 *
 * 
 * PARAM1: int datasize
 *
 * RETURNS: 
 */

PACKET
tcp_pktalloc(int datasize)
{
   PACKET pkt;

   LOCK_NET_RESOURCE(FREEQ_RESID);
   pkt = pk_alloc(datasize + HDRSLEN);
   UNLOCK_NET_RESOURCE(FREEQ_RESID);
   if (!pkt)
      return NULL;

   /* save space for tcp/ip/MAC headers */
   pkt->nb_prot = pkt->nb_buff + HDRSLEN;
   pkt->nb_plen = pkt->nb_blen;
   return pkt;
}



/* FUNCTION: tcp_pktfree()
 *
 * tcp_pktfree(PACKET p) - free a packet allocated by (presumably) 
 * tcp_pktalloc(). This is a simple wrapper around pk_free() to lock 
 * and unlock the free-queue resource. 
 *
 * 
 * PARAM1: PACKET p
 *
 * RETURNS: 
 */

void
tcp_pktfree(PACKET p)
{
   LOCK_NET_RESOURCE(FREEQ_RESID);
   pk_free(p);
   UNLOCK_NET_RESOURCE(FREEQ_RESID);
}


/* FUNCTION: tcp_data_upcall()
 *
 * tcp_do_upcall(so) - called by tcp_input() when data is received 
 * for a socket with an upcall handler set. The upcall handler is a 
 * socket structure member, set by the app via a call to 
 * setsockopt(SO_CALLBACK, ptr). The upcall routine description is as 
 * follows: int rx_upcall(struct socket so, PACKET pkt, int error); 
 * where: so - socket which got data. pkt - pkt containing recieved 
 * data, or NULL if error. error - 0 if good pkt, else BSD socket 
 * error The upcall() returns 0 if it accepted data, non-zero if not. 
 * End of file is signaled to the upcall by ESHUTDOWN error code. If 
 * LOCK_NET_RESOURCE() is used, the resource is already locked when 
 * the upcall is called. The upcall will NOT be called from inside a 
 * CRIT_SECTION macro pairing. 
 *
 * 
 * PARAM1: struct socket * so
 *
 * RETURNS: 
 */

void
tcp_data_upcall(struct socket * so)
{
   struct mbuf *  m;
   PACKET pkt;
   int   e;

   /* don't upcall application if there's no data */
   if (so->so_rcv.sb_cc == 0)
      return;

   /* don't re-enter the upcall routine */
   if (so->so_state & SS_INUPCALL)
      return;

   /* Set flags. SS_UPCALLED is used by select() logic to wake sockets blocked
   on receive, SS_INUPCALL is the re-entry guard. */
   so->so_state |= (SS_UPCALLED|SS_INUPCALL);

   m = so->so_rcv.sb_mb;
   while (m)
   {
      /* TCP code left pkt pointers at IP header, need to fix: */
      pkt = m->pkt;        /* get pkt from mbuf */
      pkt->nb_prot = m->m_data;
      pkt->nb_plen = m->m_len;

      /* pass buffer to the app */
      e = so->rx_upcall(so, (void*)m->pkt, 0);

      if (e)   /* if app returned error, quit */
         break;

      /* dequeue the mbuf data the application just accepted */      
      m->pkt = NULL;    /* pkt is now the appp's responsability */
      sbfree(&so->so_rcv, m);
      MFREE(m, so->so_rcv.sb_mb);
      m = so->so_rcv.sb_mb;
   }
   so->so_state &= ~SS_INUPCALL;    /* clear re-entry flag */
   return;
}

#endif   /* TCP_ZEROCOPY - whole file can be ifdeffed out */
#endif /* INCLUDE_TCP */


