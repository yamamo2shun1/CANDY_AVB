/*
 * FILENAME: udp_open.c
 *
 * Copyright  2000-2007 By InterNiche Technologies Inc. All rights reserved
 *
 * UDP layer's open, close and support routines.
 *
 * MODULE: INET
 *
 * ROUTINES: udp_open(), udp_close(), 
 *
 * PORTABLE: yes
 */

/* Additional Copyrights: */
/* Portions Copyright 1990 by NetPort Software  */
/*  Copyright 1986 by Carnegie Mellon  */
/*  Copyright 1984,1985 by the Massachusetts Institute of Technology  */


#include "ipport.h"
#include "q.h"
#include "netbuf.h"
#include "net.h"
#include "ip.h"
#include "udp.h"


UDPCONN firstudp = NULL;

/* FUNCTION: udp_open()
 *
 * udp_open() - Create a UDP connection and enter it in the demux 
 * table. 
 *
 * 
 * PARAM1: ip_addr fhost
 * PARAM2: unshort fsock
 * PARAM3: unshort lsock
 * PARAM4: int (*handler)(PACKET, void * ); - callback
 * PARAM5: void * data
 *
 * RETURNS:  Returns a pointer to the connections structure for use a a 
 * handle if Success, else returns NULL.
 */

UDPCONN udp_open(
   ip_addr  fhost,      /* foreign host, 0L for any */
   unshort  fsock,      /* foreign socket, 0 for any */
   unshort  lsock,      /* local socket */
   int (*handler)(PACKET, void*),   /* rcv upcall */
   void *   data)       /* random data, returned on upcalls to aid demuxing */
{
   UDPCONN con;
   UDPCONN ocon;

/*
 * Altera Niche Stack Nios port modification:
 * cast 'data' to remove build warning
 */
#ifdef   NPDEBUG
   if (NDEBUG & INFOMSG)
      dprintf("udp_open: host %u.%u.%u.%u, lsock %u, fsock %u, foo %04x\n",
    PUSH_IPADDR(fhost),lsock, fsock, (unsigned int)data);
#endif

   LOCK_NET_RESOURCE(NET_RESID);
   ocon = NULL;
   for (con = firstudp; con; con = con->u_next)
   {
      ocon = con;       /* remember last con in list */

#ifdef IP_V6
      /* we only want to check UDP-over-IPv4 connections */
      if (!(con->u_flags & UDPCF_V4))
         continue;
#endif

      if (con->u_lport == lsock && con->u_fport == fsock &&
          con->u_lhost == 0 && con->u_fhost == fhost)
      {
#ifdef   NPDEBUG
         if (NDEBUG & (INFOMSG|PROTERR))
            dprintf("UDP: Connection already exists.\n");
#endif
         UNLOCK_NET_RESOURCE(NET_RESID);
         return(NULL);
      }
   }

   con = (UDPCONN)UC_ALLOC(sizeof(struct udp_conn));
   if (con == 0)
   {
#ifdef   NPDEBUG
      if (NDEBUG & INFOMSG)
         dprintf("UDP: Couldn't allocate conn storage.\n");
#endif
      UNLOCK_NET_RESOURCE(NET_RESID);
      return(NULL);
   }

   if (ocon)   /* ocon is end of list */
      ocon->u_next = con;  /* add new connection to end */
   else  /* no list, start one */
      firstudp = con;

   con->u_next = 0;

   con->u_lport = lsock;      /* fill in connection info */
   con->u_fport = fsock;
   con->u_lhost = 0;
   con->u_fhost = fhost;
   con->u_rcv   = handler;
   con->u_data  = data;
   con->u_flags = UDPCF_V4;

   UNLOCK_NET_RESOURCE(NET_RESID);
   return(con);
}


/* FUNCTION: udp_close()
 *
 * udp_close(UDPCONN) - close a udp connection - remove the 
 * connection from udp's list of connections and deallocate it. 
 *
 * 
 * PARAM1: UDPCONN con
 *
 * RETURNS: void
 */

void
udp_close(UDPCONN con)
{
   UDPCONN pcon;
   UDPCONN lcon;

#ifdef NPDEBUG
   if ((con == NULL) || (firstudp == NULL))
   {
      dtrap(); /* bad programming! */
      return;
   }
#endif   /* NPDEBUG */

   LOCK_NET_RESOURCE(NET_RESID);
   /* find connection in list and unlink it */
   lcon = NULL;   /* clear ptr to last connection */
   for (pcon = firstudp; pcon; pcon = pcon->u_next)
   {
      if (pcon == con)  /* found connection to delete */
      break;
      lcon = pcon;   /* remember last connection */
   }

   if (!pcon)
   {
      dtrap(); /* prog error - connenction not in list */
      UNLOCK_NET_RESOURCE(NET_RESID);
      return;
   }

   if (lcon)   /* in con is not head of list */
      lcon->u_next = con->u_next;   /* unlink */
   else
      firstudp = con->u_next; /* remove from head */

   UC_FREE(con);  /* free memory for structure */
   UNLOCK_NET_RESOURCE(NET_RESID);
}
/* end of file udp_open.c */


