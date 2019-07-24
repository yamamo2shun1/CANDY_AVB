/*
 * FILENAME: rawsock.c
 *
 * Copyright 2002 InterNiche Technologies Inc. All rights reserved.
 *
 * Raw IP access for sockets -- provides sockets-API access to the 
 * InterNiche lightweight API's raw IP endpoints.
 *
 * MODULE: TCP
 *
 * ROUTINES: rawip_lookup(), rawip_soinput(), rawip_usrreq()
 *
 * PORTABLE: yes
 */

#include "ipport.h"
#include "tcpport.h"

#ifdef INCLUDE_TCP  /* include/exclude whole file at compile time */
#ifdef IP_RAW        /* can ifdef away whole file */

#include "mbuf.h"    /* sockets/BSDish includes */
#include "socket.h"
#include "sockvar.h"
#include "sockcall.h"
#include "protosw.h"

#include "ip.h"

/* list of active raw-IP endpoints */
extern struct ipraw_ep * ipraw_eps;

/* FUNCTION: rawip_lookup()
 * 
 * Locate raw IP endpoint for a socket.
 *
 * PARAM1: struct socket * so; IN- socket for which raw IP endpoint
 *                             is to be located.
 *
 * RETURNS: pointer to the matching raw IP endpoint,
 *          or NULL if no matching raw IP endpoint is found.
 */

struct ipraw_ep *
rawip_lookup(struct socket * so)
{
   struct ipraw_ep * tmp;

   for (tmp = ipraw_eps; tmp; tmp = tmp->ipr_next)
      if (tmp->ipr_data == (void*)so)
      return (tmp);

   return NULL;   /* didn't find it */
}

/* FUNCTION: rawip_soinput()
 *
 * Raw IP upcall handler for Raw IP endpoints with corresponding sockets. 
 * Return 0 if OK, else nonzero error code. 
 * 
 * PARAM1: PACKET pkt
 * PARAM2: void * so_ptr
 *
 * RETURNS: 0 if successful, else a non-zero error code which 
 *          indicates that the packet has not been accepted.
 */

int
rawip_soinput(PACKET pkt, void * so_ptr)
{
   struct mbuf *  m_in;    /* packet/data mbuf */
   struct socket *   so =  (struct  socket *)so_ptr;
   struct sockaddr_in   sin;

   LOCK_NET_RESOURCE(NET_RESID); 

   /* make sure we're not flooding input buffers */
   if ((so->so_rcv.sb_cc + pkt->nb_plen) >= so->so_rcv.sb_hiwat)
   {
      UNLOCK_NET_RESOURCE(NET_RESID);
      return ENOBUFS;
   }

   /* alloc mbuf for received data */
   m_in = m_getnbuf(MT_RXDATA, 0);
   if (!m_in)
   {
      UNLOCK_NET_RESOURCE(NET_RESID);
      return ENOBUFS;
   }

   /* set data mbuf to point to start of IP header */
   m_in->pkt = pkt;
   m_in->m_base = pkt->nb_buff;
   m_in->m_memsz = pkt->nb_blen;
   m_in->m_data = pkt->nb_prot;
   m_in->m_len = pkt->nb_plen;

   /* if this socket doesn't have IP_HDRINCL set, adjust the
    * mbuf to skip past the IP header
    */
   if (!(so->so_options & SO_HDRINCL))
   {
      unsigned int ihl = 
         (((struct ip *)(pkt->nb_prot))->ip_ver_ihl & 0x0f) << 2;
      m_in->m_data += ihl;
      m_in->m_len -= ihl;
   }

   /* fill in net address info for pass to socket append()ers */
   sin.sin_addr.s_addr = pkt->fhost;
   sin.sin_port = 0;
   sin.sin_family = AF_INET;

   /* attempt to append address information to mbuf */
   if (!sbappendaddr(&so->so_rcv, (struct sockaddr *)&sin, m_in))
   {
      /* set the pkt field in the mbuf to NULL so m_free() below wont 
       * free the packet buffer, because that is left to the 
       * underlying stack
       */
      m_in->pkt = NULL;
      /* free only the mbuf itself */
      m_free(m_in);
      /* return error condition so caller can free the packet buffer */
      UNLOCK_NET_RESOURCE(NET_RESID);
      return ENOBUFS;
   }

   tcp_wakeup(&so->so_rcv);   /* wake anyone waiting for this */

   sorwakeup(so);    /* wake up selects too */

   UNLOCK_NET_RESOURCE(NET_RESID);
   return 0;
}


/* FUNCTION: rawip_usrreq()
 *
 * proto_tab's usr_req call function for raw IP protocol. Maps the 
 * sockets requests to NicheStack raw IP interface. Returns 0 
 * if no error, negative "NP" error if lower layers fail, postive 
 * error from nptcp.h if TCP/Sockets fails. 
 * 
 * PARAM1: struct socket * so
 * PARAM2: struct mbuf * m
 * PARAM3: struct mbuf * nam
 *
 * RETURNS: 
 */

int
rawip_usrreq(struct socket * so, 
   struct mbuf *  m,
   struct mbuf *  nam)
{
   int   e;    /* error from IP stack */
   PACKET pkt; /* packet for sending */
   struct sockaddr_in * sin;
   struct ipraw_ep * ep;
   ip_addr fhost; /* host to send to/recv from (network byte order) */
   ip_addr lhost; /* local IP address to bind to (network byte order) */
   u_char prot;
   struct ip * pip;
   int   req;
   NET   ifp;     /* ptr to network interface structure */

   req = so->so_req;    /* get request from socket struct */

   switch (req) 
   {
   case PRU_ATTACH:
      /* fake small windows so sockets asks us to move data */
      so->so_rcv.sb_hiwat = so->so_snd.sb_hiwat = 
         ip_raw_maxalloc(so->so_options & SO_HDRINCL);
      /* make a raw IP endpoint */
      prot = (u_char)(MBUF2LONG(nam));
      /* unlock the net resource; IP will immediatly re-lock it */
      UNLOCK_NET_RESOURCE(NET_RESID);
      ep = ip_raw_open(prot, 0L, 0L, rawip_soinput, so);
      LOCK_NET_RESOURCE(NET_RESID);
      if (!ep)
         return(EINVAL);
      return 0;
   case PRU_DETACH:
      /* delete the raw IP endpoint */
      ep = rawip_lookup(so);
      if (!ep)
         return(EINVAL);
      /* unlock the net resource; IP will immediatly re-lock it */
      UNLOCK_NET_RESOURCE(NET_RESID);
      ip_raw_close(ep);
      LOCK_NET_RESOURCE(NET_RESID);
      return 0;
   case PRU_CONNECT:
      /* "connect" the raw IP endpoint to a peer IP address:
       * this sets a filter for received IP datagrams and sets
       * a default address for sending
       */
      /* fall through to shared bind logic */
   case PRU_BIND:
      /* do bind parameters lookups and tests */
      if (nam == NULL)
         return(EINVAL);
      sin = mtod(nam, struct sockaddr_in *);
      if (sin == NULL)
         return(EINVAL);
      if (nam->m_len != sizeof (*sin))
         return(EINVAL);
      ep = rawip_lookup(so);
      if (!ep)
         return(EINVAL);
      if (req == PRU_BIND)
      {
         /* bind the socket to a local interface IP address.
          *
          * if the caller-supplied address is INADDR_ANY,
          * don't bind to a specific address; else, 
          * make sure the caller-supplied address is
          * an interface IP address and if so, bind to that
          */
         if (sin->sin_addr.s_addr == INADDR_ANY)
         {
            lhost = 0L;
         }
         else
         {
            lhost = sin->sin_addr.s_addr;
            /* verify that lhost is a local interface address */
            for (ifp = (NET)(netlist.q_head); ifp; ifp = ifp->n_next)
               if (ifp->n_ipaddr == lhost)
                  break;
            if (ifp == NULL)
               return(EADDRNOTAVAIL);
         }

         /* bind the endpoint */
         ep->ipr_laddr = lhost;
      }
      else /* PRU_CONNECT */
      {
         /* connect the socket to a remote IP address.
          *
          * if the caller-supplied address is INADDR_ANY,
          * use the wildcard address; else, use the caller-
          * supplied address
          */
         if (sin->sin_addr.s_addr == INADDR_ANY)
            fhost = 0L;
         else
            fhost = sin->sin_addr.s_addr;
         /* connect the IP endpoint */
         ep->ipr_faddr = fhost;
         /* mark the socket as connected or disconnected, as appropriate */
         if (fhost != 0L) {
            so->so_state &= ~(SS_ISCONNECTING|SS_ISDISCONNECTING);
            so->so_state |= SS_ISCONNECTED;
         }
         else
         {
            so->so_state &= ~SS_ISCONNECTED;
         }
         /* since socket was in listen state, packets may be queued */
         sbflush(&so->so_rcv);   /* dump these now */
      }
      return 0;
   case PRU_SEND:
      /* do parameter lookups and tests */
      if (!m)  /* no data passed? */
         return(EINVAL);

      ep = rawip_lookup(so);
      if (!ep)
      {
         m_free(m);
         /* may be bogus socket, but more likely the connection may 
         have closed due to ICMP dest unreachable from other side. */
         return(ECONNREFUSED);
      }

      if (nam == NULL)  /* no sendto() info passed, must be send() */
      {
         if (!(so->so_state & SS_ISCONNECTED))
            return (ENOTCONN);
         fhost = ep->ipr_faddr;
      }
      else 
      {
         if (so->so_state & SS_ISCONNECTED)
            return (EISCONN);
         if (nam->m_len != sizeof (*sin))
         {
            dtrap();
            return (EINVAL);
         }
         sin = mtod(nam, struct sockaddr_in *);
         fhost = sin->sin_addr.s_addr;
      }

      /* since our pkt->nb_buff size is tied to max packet size, we 
       * assume our raw IP datagrams are always in one mbuf and that the 
       * mbuf -- but check anyway
       */
      if (m->m_len > (unsigned)ip_raw_maxalloc(so->so_options & SO_HDRINCL))
      {
         dtrap(); /* should never happen */
         return EMSGSIZE;  /* try to recover */
      }
      /* get a packet buffer for send */
      pkt = ip_raw_alloc(m->m_len, so->so_options & SO_HDRINCL);
      if (!pkt)
      {
         m_free(m);
         return ENOBUFS;   /* report buffer shortages */
      }
      MEMCPY(pkt->nb_prot, m->m_data, m->m_len);
      pkt->nb_plen = m->m_len;
      /* finished with mbuf, free it now */
      m_free(m);
      pkt->fhost = fhost;

      /* if we're being asked to send to 255.255.255.255 (a local-net
       * broadcast), figure out which interface to send the broadcast
       * on, based on the IP address that the socket is bound to: if
       * it has been bound to an interface address, we should send the
       * broadcast on that interface; else, we look for the first
       * interface that can support broadcasts and is up; if we still
       * don't have an interface we look for the first interface that
       * is up; if (after all that) we don't have an interface then we
       * fail with error EADDRNOTAVAIL; and finally, if we're built
       * for a single-homed configuration where there's only one
       * interface, we might as well use it, so we do.  
       */
      if (fhost == 0xffffffff)
      {
#ifdef MULTI_HOMED
         if (ep->ipr_laddr != 0L)
         {
            for (ifp = (NET)(netlist.q_head); ifp; ifp = ifp->n_next)
               if (ifp->n_ipaddr == ep->ipr_laddr)
                  break;
         }
         else {
            for (ifp = (NET)(netlist.q_head); ifp; ifp = ifp->n_next)
               if ((ifp->n_flags & NF_BCAST) &&
                   (ifp->n_mib) && (ifp->n_mib->ifAdminStatus == NI_UP))
                  break;
         }
         if (ifp == NULL)
         {
            for (ifp = (NET)(netlist.q_head); ifp; ifp = ifp->n_next)
               if ((ifp->n_mib) && (ifp->n_mib->ifAdminStatus == NI_UP))
                  break;
            if (ifp == NULL)
               return(EADDRNOTAVAIL);
         }
         pkt->net = ifp;
#else  /* single-homed */
         pkt->net = (NET)(netlist.q_head);
#endif /* MULTI_HOMED */
      }

#ifdef IP_MULTICAST

      /* If the socket has an IP moptions structure for multicast options,
       * place a pointer to this structure in the PACKET structure.
       */
      if (so->inp_moptions)
         pkt->imo = so->inp_moptions;

#endif   /* IP_MULTICAST */

      if (so->so_options & SO_HDRINCL)
      {
         UNLOCK_NET_RESOURCE(NET_RESID);
         e = ip_raw_write(pkt);
         LOCK_NET_RESOURCE(NET_RESID);
      }
      else
      {
         pip = (struct ip *)(pkt->nb_prot - IPHSIZ);
         if (ep->ipr_laddr)
            pip->ip_src = ep->ipr_laddr;
         else
         {
            if (fhost == 0xffffffff)
               pip->ip_src = pkt->net->n_ipaddr;
            else
               pip->ip_src = ip_mymach(fhost);
         }
         pip->ip_dest = fhost;
         UNLOCK_NET_RESOURCE(NET_RESID);
         e = ip_write(ep->ipr_prot, pkt);
         LOCK_NET_RESOURCE(NET_RESID);
      }         
      if (e < 0) 
         return(e);
      return 0;
   case PRU_SOCKADDR:
      /* fall through to share PRU_PEERADDR prefix */
   case PRU_PEERADDR:
      if (nam == NULL)
         return(EINVAL);
      sin = mtod(nam, struct sockaddr_in *);
      if (sin == NULL)
         return(EINVAL);
      ep = rawip_lookup(so);
      if (!ep)
         return(EINVAL);
      sin->sin_port = 0;
      nam->m_len = sizeof(*sin);
      if (req == PRU_SOCKADDR)
      {
         sin->sin_addr.s_addr = ep->ipr_laddr;
      }
      else /* PRU_PEERADDR */
      {
         sin->sin_addr.s_addr = ep->ipr_faddr;
      }
      return 0;
   case PRU_DISCONNECT:
   case PRU_RCVD:
      dtrap();
      return 0;
   case PRU_LISTEN:     /* don't support these for raw IP */
   case PRU_ACCEPT:
   default:
      return EOPNOTSUPP;
   }
}

#endif   /* IP_RAW */
#endif /* INCLUDE_TCP */




