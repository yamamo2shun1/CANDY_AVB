/*
 * FILENAME: ping.c
 *
 * Copyright 1996- 2000 By InterNiche Technologies Inc. All rights reserved
 *
 * API call for simple ICMP echo ("ping").
 *
 * MODULE: INET
 *
 * ROUTINES: icmpEcho(),
 *
 * PORTABLE: yes
 */

/* Additional Copyrights: */
/* Portions Copyright 1994 by NetPort software. */

#include "ipport.h"
#include "q.h"
#include "netbuf.h"
#include "net.h"
#include "ip.h"
#include "icmp.h"

char *   pingdata =  "Ping from NetPort IP stack";  /* default ping data */



/* FUNCTION: icmpEcho()
 *
 * icmpEcho() - ICMP Echo Request (the guts of "ping") Callable from 
 * Applications. Sends a single "ping" (ICMP echo request) to the 
 * specified host. Replys are upcalled via Queue event so we can send 
 * the application's window a Message as we get a reply or timeout. 
 * 
 * 
 * PARAM1: ip_addr host
 * PARAM2: char * data
 * PARAM3: unsigned length of data
 * PARAM4: unshort pingseq
 *
 * RETURNS: Returns 0 if started OK, else negative error message. 
 */

int
icmpEcho(ip_addr host,  /* host to ping - 32 bit, network-endian */
   char *   data,       /* ping data, NULL if don't care */
   unsigned datalen,     /* length of data to attach to ping request */
   unshort  pingseq)    /* ping sequence number */
{
   PACKET   p;
   int      ip_err;
   struct ping *  e;
   struct ip *    pip;

   LOCK_NET_RESOURCE(FREEQ_RESID);
   p = pk_alloc(PINGHDRSLEN + datalen);
   UNLOCK_NET_RESOURCE(FREEQ_RESID);
   if (!p)
   {
#ifdef   NPDEBUG
      if (NDEBUG & IPTRACE)
         dprintf("icmp: can't alloc packet\n");
#endif
      return(ENP_NOBUFFER);
   }

   p->nb_prot = p->nb_buff + PINGHDRSLEN;
   p->nb_plen = datalen;
   p->fhost = host;

   if(host == 0xFFFFFFFF)  /* broadcast? */
      p->net = nets[0];    /* then use first iface */

   /* copy in data field */
   if (data)
   {
      MEMCPY(p->nb_prot, data, datalen);
   }
   else  /* caller didn't specify data */
   {
      unsigned   donedata;
      strcpy(p->nb_prot, pingdata);
      donedata = (unsigned)strlen(pingdata);
      while (donedata < datalen)
      {
         *(p->nb_prot + donedata) = (char)((donedata) & 0x00FF);
         donedata++;
      }
   }

   /* adjust packet pointers to icmp ping header */
   p->nb_prot -= sizeof(struct ping);
   p->nb_plen += sizeof(struct ping);

   /* fill in icmp ping header */
   e = (struct ping *)p->nb_prot;
   e->ptype = ECHOREQ;
   e->pcode = 0;
   e->pid = 0;
   e->pseq = pingseq;

   /* Calculate the checksum */
   e->pchksum = 0;
   if (datalen & 1)  /* if data size is odd, pad with a zero */
      *((char*)(e+1) + datalen) = 0;

   e->pchksum = ~cksum(e, (ICMPSIZE+datalen+1)>>1);

   /* need to fill in IP addresses at this layer too */
   pip = (struct ip *)(p->nb_prot - sizeof(struct ip));
   pip->ip_src = ip_mymach(host);
   pip->ip_dest = host;

   LOCK_NET_RESOURCE(NET_RESID);
   ip_err = ip_write(ICMP_PROT, p);    /* send down to IP layer */
   UNLOCK_NET_RESOURCE(NET_RESID);

   /* Errors are negative. A zero means send was OK. a positive number
    * usually means we had to ARP. Assume this will work and count a send.
    */
   if(ip_err < 0)
   {
#ifdef   NPDEBUG
      if (NDEBUG & NETERR)
         dprintf("icmp: can't send echo request\n");
#endif
      /* rfc 1156 seems to say not to count these. (pg 48) -JB- */
      /* LOCK_NET_RESOURCE(FREEQ_RESID); */
      /* pk_free(p); */
      /* UNLOCK_NET_RESOURCE(FREEQ_RESID); */
      return(ip_err);
   }
   /* fall to here if we sent echo request OK */
   icmp_mib.icmpOutMsgs++;
   icmp_mib.icmpOutEchos++;

   return(0);
}

