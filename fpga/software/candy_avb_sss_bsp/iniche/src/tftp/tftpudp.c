/* FILE: tftpudp.c
 *
 * Copyright 1998 by InterNiche Technologies, Inc. All rights reserved.
 * Copyright 1996 by NetPort Software.
 *
 * MODULE: TFTP
 *
 * ROUTINES: tftp_udplisten(), tftp_udpsend(), tftp_upcall(),
 * ROUTINES: tftp_udpbuffer(), tftp_udpfree(), tftp_udpclose()
 *
 * UDP API (Sockets or lightweight) dependant portion of the TFTP code.
 *
 * PORTABLE: yes
 */

#include "tftpport.h"
#include "tftp.h"

#include "q.h"         /* get netport IP stack defines */
#include "netbuf.h"
#include "net.h"
#include "ip.h"
#include "udp.h"


UDPCONN tftp_conn;

static int tftp_upcall(PACKET pkt, void * data);


/* FUNCTION: tftp_udplisten()
 *
 * Start a UDP listen for TFTP 
 *
 * PARAM1: ip_addr            IP address of foreign host
 * PARAM2: unshort            foreign port
 * PARAM3: unshort            local port or zero
 * PARAM4: void *             callback data parameter value
 *
 * RETURNS: void *            connection ID (socket or UDPCONN)
 *                            or NULL.
 *
 * Start a UDP listen on fhost & ports passed. This is used by the
 * TFTP code to establish a receive endpoint prior to sending UDP
 * datagrams. If lport is 0, a useable value is obtained from the UDP layer.
 *
 * The "ptr" parameter is callback data. It will be the tfconn ptr for 
 * a client, TFTPSERVER for a server.
 *
 * Returns connection ID (socket or UDPCONN) if successful, else 
 * returns NULL if there is an error.
 */

void *
tftp_udplisten(ip_addr fhost, unshort fport, unshort * lport, void *ptr)
{
   UDPCONN u;
   unshort tmpport;  /* tmp holder for local port value */

   /* first, get a local port for use on this connection */
   if (*lport == 0)   /* caller wants us to allocate one */
      tmpport = udp_socket();
   else  /* use port passed */
      tmpport = *lport;

   if (ptr == NULL)
      ptr = TFTPSERVER;  /* indicate server */

   u = udp_open(fhost, fport, tmpport, tftp_upcall, (void *)ptr);

   if (u)
   {
      *lport = tmpport;       /* return local port to caller */
      return ((void *)u);     /* success */
   }
   else
      return (void *)NULL;    /* error */
}



/* FUNCTION: tftp_udpsend()
 *
 * Send a UDP datagram
 *
 * PARAM1: struct tfconn *    TFTP connection block
 * PARAM2: void *             UDP data buffer
 * PARAM3: int                UDP data buffer length
 *
 * RETURNS: int               0 if success, else error code
 */

int
tftp_udpsend(struct tfconn *conn, void *outbuf, int outlen)
{
   PACKET pkt;
   int e;   /* error holder */

   pkt = (PACKET)conn->tf_outbuf.udp_use;

   /* sanity check buffer pointer */
   if ((char *)outbuf != pkt->nb_prot)
   {
      /* this is OK on retrys, but not initial sends */
      if (((char *)outbuf <= pkt->nb_buff) ||  /* null outbuf == bad */
         (conn->tf_tmo == 0)) /* this is not a retry */
      {
         dtrap();
         return ENP_LOGIC;
      }
   }

   pkt->nb_prot = (char*)outbuf;
   pkt->nb_plen = outlen;
   pkt->fhost = conn->tf_fhost;
   pkt->net = NULL;  /* force it to route */

#ifdef ZEROCOPY_API  /* packet can be marked in use, not freed */
   pkt->inuse = 2;   /* clone it so it won't pk_free() on send */
#else    /* need to copy data in case we have to retry */
   {
      PACKET pkt2;   /* packet to send & free */

      pkt2 = udp_alloc(outlen, 0);
      if (!pkt2)
         return ENP_NOBUFFER;

      pkt2->nb_plen = pkt->nb_plen;
      pkt2->fhost = pkt->fhost;
      pkt2->net = NULL;
      MEMCPY(pkt2->nb_prot, pkt->nb_prot, outlen);
      pkt = pkt2; /* send duplicate pkt, keep original in tf_conn */
   }
#endif

   e = udp_send(conn->tf_fport, conn->tf_lport, pkt);

   if (e < 0)
   {
#ifdef NPDEBUG
      dprintf("tftp_udpsend(): udp_send() error %d\n", e);
#endif
      return e;
   }
   else
      return 0;
}



/* FUNCTION: tftp_upcall()
 *
 * UDP callback function
 *
 * PARAM1: PKT                received UDP datagram packet
 * PARAM2: void *             caller-supplied data
 *
 * RETURNS: int               0 if success, else error code
 */

int
tftp_upcall(PACKET pkt, void * data)
{
   struct tfconn * cn;
   struct udp * pup;

   /* get pointer to UDP header */
   pup = (struct udp *)pkt->nb_prot;
   pup -= 1;

   /* Find tftp session this packet is for. */
   if (data == TFTPSERVER)    /* tftp server connection */
   {
#ifdef TFTP_SERVER
      int err;
      /* call server for new session here */
      err = tfshnd(pkt->fhost, pup->ud_srcp, pkt->nb_prot);
#endif /* TFTP_SERVER */
      udp_free(pkt);
     /* do not return an error to the udp layer */
   }
   else  /* TFTP server child connection */
   {
      cn = (struct tfconn *)data;
      if (cn->tf_fhost != pkt->fhost)   /* sanity check */
      {
         dtrap();
         return ENP_NOT_MINE;
      }

      if (cn->tf_inbuf.udp_use)
      {
         dtrap(); /* ever happen? */
      }

      cn->tf_inbuf.data = pkt->nb_prot;
      cn->tf_inbuf.dlen = pkt->nb_plen;
      cn->tf_inbuf.udp_use = (void*)pkt;

      tftprcv(cn, pup->ud_srcp);    /* process the packet */

      if (cn->tf_inbuf.udp_use)     /* pkt may be freed in upcall */
      {
         udp_free(pkt);
         cn->tf_inbuf.udp_use = (PACKET)NULL;
      }
   }

   return 0;
}



/* FUNCTION: tftp_udpbuffer()
 *
 * Obtain a packet buffer for UDP output
 *
 * PARAM1: tfconn *           TFTP connection block
 * PARAM2: int                UDP data length
 *
 * RETURNS: none
 *
 * Called by tftp to ask UDP layer for a packet buffer. If a buffer
 * is obtained, the data is filled into the passed cn->tf_outbuf.
 * Otherwise, everything in tf_outbuf is cleared.
 *
 * The lightweight API grabs a buffer from the UDP layer to avoid doing a 
 * data copy. Sockets will probably need to calloc.....
 */

void
tftp_udpbuffer(struct tfconn *cn, int size)
{
   PACKET pkt;

   if (cn->tf_outbuf.udp_use)
   {
      pkt = (PACKET)cn->tf_outbuf.udp_use;
      if (pkt->nb_plen >= (unsigned)size)
      {
         /* recycle existing buffer */
         pkt->nb_prot = (char *)cn->tf_outbuf.data;
         return;
      }
      else
      {
         udp_free(pkt);       /* old packet was too small */
      }
   }

   pkt = udp_alloc(size, 0);
   if (!pkt) /* alloc failed, clear outbuf pointers */
   {
      cn->tf_outbuf.data = NULL;
      cn->tf_outbuf.dlen = 0;
      cn->tf_outbuf.udp_use = NULL;
   }
   else  /* return buffer pointer */
   {
      cn->tf_outbuf.data = pkt->nb_prot;
      cn->tf_outbuf.dlen =  size;
      cn->tf_outbuf.udp_use = (void *)pkt;   /* so we can find struct later */
   }
}



/* FUNCTION: tftp_udpfree()
 *
 * Free a UDP buffer that has been ACKed
 *
 * PARAM1: tfconn *           TFTP connection block
 *
 * RETURNS: none
 *
 * Called to free the udp buffer after it has been acked. 
 */

void
tftp_udpfree(struct tfconn *cn)
{
   PACKET pkt;

   pkt = (PACKET)cn->tf_outbuf.udp_use;
#ifdef ZEROCOPY_API
   pkt->inuse = 1;   /* free this copy - "unclone" it */
#endif
   udp_free(pkt);
   cn->tf_outbuf.udp_use = NULL;
   cn->tf_outbuf.data = NULL;
}



/* FUNCTION: tftp_udpclose()
 *
 * Close the TFTP connection
 *
 * PARAM1: void *             TFTP connection handle
 *
 * RETURNS: int               0 if success, else error code
 *
 * Call udp_close() to close the connection. 
 */

int
tftp_udpclose(void * conn)
{
   UDPCONN cn = (UDPCONN)conn;

   udp_close(cn);
   return 0;   /* no ability to detect error on this API */
}

