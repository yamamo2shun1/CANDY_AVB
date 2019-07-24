/*
 * FILENAME: icmp.c
 *
 * Copyright  2000 By InterNiche Technologies Inc. All rights reserved
 *
 *
 * MODULE: INET
 *
 * ROUTINES: icmprcv(), icmp_destun(), icmp_du(), icmp_stats(),
 *
 * PORTABLE: yes
 */

/* Additional Copyrights: */
/* Portions Copyright 1990-1994 by NetPort Software. */
/* Portions Copyright 1986 by Carnegie Mellon  */
/* Portions Copyright 1984 by the Massachusetts Institute of Technology  */


#include "ipport.h"
#include "q.h"
#include "netbuf.h"
#include "net.h"
#include "ip.h"
#include "icmp.h"
#include "pmtu.h"

/* 
 * Altera Niche Stack Nios port modification:
 * Fix build warnings 
 */
#ifdef ALT_INICHE
#ifdef IP_RAW
int   ip_raw_input(PACKET P);
#endif   /* IP_RAW */
#endif /* ALT_INICHE */

struct IcmpMib icmp_mib;   /* storage for MIB statistics */

/* the callback for application to get a peek at icmp du packets */
void (*icmpdu_hook)(PACKET p, struct destun * pdp) = NULL;


/* FUNCTION: icmprcv()
 *
 * ICMP received packet upcall handler. Returns 0 if we processed the 
 * packet, or ENP_NOT_MINE, or a negative error code. 
 *
 * 
 * PARAM1: PACKET p
 *
 * RETURNS: 
 */

#ifdef USER_PING_TSTAMP
extern u_long user_UTCtime();
struct align_stamps
{
   u_long dtstamp[3]; 
};
#endif /* USER_PING_TSTAMP */

int
icmprcv(PACKET p)      /* the incoming packet */
{
   unsigned len;        /* packet length, minus IP & MAC headers */
   ip_addr host;        /* the foreign host */
   struct ip * pip;
   struct ping *  e;
   unsigned short osum;
   unsigned short xsum;
#ifdef FULL_ICMP
   struct redirect * rd;
   struct destun *   pdp;
#endif   /* FULL_ICMP */
   char sav_ch = 0;
   int i;

   icmp_mib.icmpInMsgs++;                 /* received one more icmp */

   pip = ip_head(p);                      /* find IP header */
   len = p->nb_plen - (ip_hlen(pip));     /* strip IP header length */
   host = p->fhost;                       /* filled in by IP layer */

#ifdef   NPDEBUG
   if ((NDEBUG & UPCTRACE) && (NDEBUG & IPTRACE))
      dprintf("ICMP: p[%u] from %u.%u.%u.%u\n", len, PUSH_IPADDR(host));
#endif

   e = (struct ping *)ip_data(pip);       /* finally, extract ICMP header */

   osum = e->pchksum;
   e->pchksum = 0;

   if (len&1)
   {
      sav_ch = *(((char *) e) + len);
      ((char *)e)[len] = 0;
   }

   xsum = ~cksum(e, (len+1)>>1);
   if (len&1) *(((char *) e) + len) = sav_ch;
   if (xsum != osum)
   {
      e->pchksum = osum;
#ifdef   NPDEBUG
      if ((NDEBUG & UPCTRACE) && (NDEBUG & IPTRACE))
      {
         dprintf("ICMP: Bad xsum %04x should have been %04x\n",
          osum, xsum);
         if (NDEBUG & DUMP) ip_dump(p);
      }
#endif
      icmp_mib.icmpInErrors++;
      LOCK_NET_RESOURCE(FREEQ_RESID);
      pk_free(p);
      UNLOCK_NET_RESOURCE(FREEQ_RESID);
      return ENP_BAD_HEADER;
   }

   e->pchksum = osum;

   switch (e->ptype) 
   {
   case ECHOREQ:  /* got ping request, send reply */
      icmp_mib.icmpInEchos++;
#ifdef   NPDEBUG
      if ((NDEBUG & UPCTRACE) && (NDEBUG & IPTRACE))
         dprintf("ICMP: echo reply to %u.%u.%u.%u\n", PUSH_IPADDR(host));
#endif
      e->ptype = ECHOREP;
      e->pchksum = 0;
      if (len&1)  /* pad odd length packets for checksum routine */
      {
         sav_ch = *(((char *) e) + len);
         ((char *)e)[len] = 0;
      }

      e->pchksum = ~cksum(e, (len+1)>>1);
      if (len&1) *(((char *) e) + len) = sav_ch;
      /* check to see if the destination is the IPv4 broadcast address,
       * or if the destination is a multicast group address, or if the
       * destination address is the subnet-directed broadcast 
       */
      if ((pip->ip_dest == 0xffffffff) || 
#ifdef IP_MULTICAST
          (IN_MULTICAST(ntohl(pip->ip_dest))) ||
#endif
          (pip->ip_dest == (p->net->n_ipaddr | (~(p->net->snmask)))))
      {
         pip->ip_src = p->net->n_ipaddr;
      }
      else pip->ip_src = pip->ip_dest;

      pip->ip_dest = host;
      icmp_mib.icmpOutEchoReps++;
      icmp_mib.icmpOutMsgs++;
      p->fhost = host;
      p->nb_prot += ip_hlen(pip);      /* move pointer past IP to ICMP */
      p->nb_plen = len;

      i = ip_write(ICMP_PROT, p);
      if (i < 0)
      {
#ifdef   NPDEBUG
         if (NDEBUG & (UPCTRACE))
            dprintf("icmp: reply failed\n");
#endif
      }
      /* reused p will be freed by net->xxx_send() */
      return 0;
   case ECHOREP:
      icmp_mib.icmpInEchoReps++;
      
/* 
 * Altera Niche Stack Nios port modification
 */
#if defined(ALT_INICHE) && !defined(PING_APP) && defined(IP_RAW)
      return(ip_raw_input(p));
#endif

      /* The PING appplication expects the nb_prot field to point to
       * the ICMP header, not the IP as it does here, NOR to the application
       * data like a TCP or UDP app would. Adjust this here:
       */
      p->nb_prot += ip_hlen(pip);      /* move pointer past IP to ICMP */
      p->nb_plen -= ip_hlen(pip);      /* subtract IP header size */
#ifdef PING_APP
      return(pingUpcall(p));
#else    /* not a real ping app; but report ping reply if debugging... */
#ifdef NPDEBUG
      if ((NDEBUG & IPTRACE))
      {
         dprintf("Ping (icmp echo) reply from %u.%u.%u.%u, data len %d\n", 
          PUSH_IPADDR(p->fhost), p->nb_plen);
      }
#endif   /* NPDEBUG */
      LOCK_NET_RESOURCE(FREEQ_RESID);
      pk_free(p);
      UNLOCK_NET_RESOURCE(FREEQ_RESID);
      return(0);
#endif   /* PING_APP */
#ifdef FULL_ICMP
   case DESTIN:
      icmp_mib.icmpInDestUnreachs++;
      pdp = (struct destun *)e;
#ifdef   NPDEBUG
      if (NDEBUG & UPCTRACE)
      {
         dprintf("ICMP: got dest unreachable type ");
         dprintf("%u on %u.%u.%u.%u ", pdp->dcode,
          PUSH_IPADDR(pdp->dip.ip_dest));
         dprintf("from %u.%u.%u.%u\n", PUSH_IPADDR(host));
      }
#endif   /* NPDEBUG */
      icmp_du(p, pdp);
      break;
   case SOURCEQ:
      icmp_mib.icmpInSrcQuenchs++;
#ifdef   NPDEBUG
      if (NDEBUG & UPCTRACE)
      {
         dprintf("ICMP: source quench from %u.%u.%u.%u\n", PUSH_IPADDR(host));
         if (NDEBUG & DUMP) ip_dump(p);
      }
#endif   /* NPDEBUG */
      LOCK_NET_RESOURCE(FREEQ_RESID);
      pk_free(p);
      UNLOCK_NET_RESOURCE(FREEQ_RESID);
      break;
   case REDIR:          /* got an icmp redirect */
      icmp_mib.icmpInRedirects++;
      rd = (struct redirect *)e;
#ifdef   NPDEBUG
      if (NDEBUG & UPCTRACE)
      {
         dprintf("ICMP: rcvd redirect for %u.%u.%u.%u ",
          PUSH_IPADDR(rd->rdip.ip_dest));
         dprintf("to %u.%u.%u.%u\n", PUSH_IPADDR(rd->rdgw));
      }
#endif   /* NPDEBUG */
#ifdef IP_ROUTING
      /* try to add/update route table */
      add_route(rd->rdip.ip_dest, 0xFFFFFFFF, rd->rdgw, 
       net_num(p->net), IPRP_ICMP);
#endif   /* IP_ROUTING */
      LOCK_NET_RESOURCE(FREEQ_RESID);
      pk_free(p);
      UNLOCK_NET_RESOURCE(FREEQ_RESID);
      break;
   case TIMEX:
      icmp_mib.icmpInTimeExcds++;
#ifdef   NPDEBUG
      if (NDEBUG & UPCTRACE)
      {
         struct timex * pt =  (struct  timex *)e;

         dprintf("ICMP: timex msg from %u.%u.%u.%u\n",
          PUSH_IPADDR(p->fhost));
         dprintf(" about %u.%u.%u.%u\n", PUSH_IPADDR(pt->tip.ip_dest));
         if (NDEBUG & DUMP) ip_dump(p);
      }
#endif   /* NPDEBUG */
      LOCK_NET_RESOURCE(FREEQ_RESID);
      pk_free(p);
      UNLOCK_NET_RESOURCE(FREEQ_RESID);
      break;
   case PARAM:
      icmp_mib.icmpInParmProbs++;
#ifdef   NPDEBUG
      if (NDEBUG & UPCTRACE)
         dprintf("ICMP: got param problem message\n");
      if (NDEBUG & DUMP)
      {
         ip_dump(p);
      }
#endif   /* NPDEBUG */
      LOCK_NET_RESOURCE(FREEQ_RESID);
      pk_free(p);
      UNLOCK_NET_RESOURCE(FREEQ_RESID);
      break;
   case TIMEREQ:
      icmp_mib.icmpInTimestamps++;
#ifdef   NPDEBUG
      if (NDEBUG & UPCTRACE)
         dprintf("ICMP: got timestamp request\n");
#endif   /* NPDEBUG */
      e->ptype = TIMEREP;
      e->pchksum = 0;

#ifdef USER_PING_TSTAMP
      {
		  struct tstamp *tsp = (struct tstamp *)e;
          struct align_stamps *sstmp = (struct align_stamps *)&tsp->stamps;

		  /* 
		   * user_UTCtime() must return UTC time in htonl((u_long)t) format.
		   * we set the Receive Timestamp and Transmit Timestamp
		   */
		  sstmp->dtstamp[1] = sstmp->dtstamp[2] = user_UTCtime();
	  }
#endif /* USER_PING_TSTAMP */

      e->pchksum = ~cksum(e, sizeof(struct tstamp)>>1);
      pip->ip_src = pip->ip_dest;
      pip->ip_dest = host;
      icmp_mib.icmpOutMsgs++;
      icmp_mib.icmpOutTimestampReps++;
      p->nb_prot += ip_hlen(pip);      /* move pointer past IP to ICMP */
      p->nb_plen = sizeof(struct tstamp);
      p->fhost = host;
      i = ip_write(ICMP_PROT, p);
      if (i < 0)
      {
#ifdef   NPDEBUG
         if (NDEBUG & UPCTRACE)
            dprintf("icmp: can't send timestamp reply\n");
#endif   /* NPDEBUG */
      }
      /* re-used packet was pk_free()d by net->send() */
      return (0);
   case INFO:
#ifdef   NPDEBUG
      if (NDEBUG & UPCTRACE)
         dprintf("icmp: got info request\n");
#endif   /* NPDEBUG */
      LOCK_NET_RESOURCE(FREEQ_RESID);
      pk_free(p);
      UNLOCK_NET_RESOURCE(FREEQ_RESID);
      break;
#endif   /* FULL_ICMP */
   default:
#ifdef   NPDEBUG
      if (NDEBUG & UPCTRACE)
      {
         dprintf("icmp: unhandled type %u\n", e->ptype);
         if (NDEBUG & DUMP) ip_dump(p);
      }
#endif   /* NPDEBUG */
      LOCK_NET_RESOURCE(FREEQ_RESID);
      pk_free(p);
      UNLOCK_NET_RESOURCE(FREEQ_RESID);
      return ENP_NOT_MINE;
   }
#ifdef FULL_ICMP
   return ENP_NOT_MINE;
#endif   /* FULL_ICMP */
}

#ifdef FULL_ICMP

#ifdef NPDEBUG
static char *dsts[] = 
{
   "net", "host", "protocol", "port", "fragmentation needed",
   "source route failed" };
#endif   /* NPDEBUG */



/* FUNCTION: icmp_destun()
 *
 * icmp_destun() - Send an ICMP destination unreachable packet.
 * 
 * PARAM1: ip_addr host
 * PARAM2: ip_addr src_addr (source address for outgoing ICMP/IP packet header)
 * PARAM3: struct ip *ip
 * PARAM4: unsigned typecode     - type and code fields
 * PARAM5: net packet came from 
 *
 * If the type field is 0, then type is assumed to be DESTIN.
 *
 * RETURNS: void
 */

void
icmp_destun(ip_addr host,  /* host to complain to */
   ip_addr src_addr, /* source address for outgoing ICMP/IP packet header */
   struct ip * ip,   /* IP header of offending packet */
   unsigned typecode,    /* type & code of DU to send (PROT, PORT, HOST) */
   NET   net)        /* interface that this packet came in on */
{
   PACKET p;
   struct destun *   d;
   struct ip * pip;
   int   i;

#ifdef NPDEBUG
   if (NDEBUG & PROTERR)
      dprintf("icmp: sending %s dest unreachable to %u.%u.%u.%u\n",
      dsts[typecode & 0xFF], PUSH_IPADDR(host));
#endif   /* NPDEBUG */

   LOCK_NET_RESOURCE(FREEQ_RESID);
   p = pk_alloc(512 + IPHSIZ);   /* get packet to send icmp dest unreachable */
   UNLOCK_NET_RESOURCE(FREEQ_RESID);

   if (p == NULL)
   {
#ifdef NPDEBUG
      if (NDEBUG & IPTRACE)
         dprintf("icmp: can't alloc pkt\n");
#endif   /* NPDEBUG */
      icmp_mib.icmpOutErrors++;
      return;
   }

   /* build the addresses in the IP header */
   pip = (struct ip *)p->nb_prot;
   pip->ip_src = src_addr;
   pip->ip_dest = host;

   /* allow space for icmp header */
   p->nb_prot += sizeof(struct ip);
   p->nb_plen -= sizeof(struct ip);
   p->net = net;     /* Put in the interface that this packet came in on */

   d = (struct destun *)p->nb_prot;

   if (typecode & 0xFF00)               /* if the type was sent */
      d->dtype = (char)(typecode >>8);  /* then use it */
   else                                 /* else use default */
      d->dtype = DESTIN;
   d->dcode = (char)(typecode & 0xFF);
   d->dno1 = d->dno2 = 0;
#ifndef ICMP_SUPPRESS_PMTU
   if ((typecode & 0xFF) == DSTFRAG)
      d->dno2 = htons(net->n_mtu - net->n_lnh);
#endif    /* ICMP_SUPPRESS_PMTU */
   MEMCPY(&d->dip, ip, (sizeof(struct ip) + ICMPDUDATA));

   d->dchksum = 0;
   d->dchksum = ~cksum(d, sizeof(struct destun)>>1);

   p->nb_plen =  sizeof(struct destun);
   p->fhost = host;
   i = ip_write(ICMP_PROT, p);
   if (i < 0)
   {
      icmp_mib.icmpOutErrors++;
#ifdef   NPDEBUG
      if (NDEBUG & (IPTRACE|NETERR|PROTERR))
         dprintf("ICMP: Can't send dest unreachable\n");
#endif   /* NPDEBUG  */
      return;
   }
   icmp_mib.icmpOutMsgs++;
   icmp_mib.icmpOutDestUnreachs++;
   return;
}

/* FUNCTION: icmp_timex ()
 *
 * icmp_timex () - Send an ICMP Time Exceeded message.
 * 
 * PARAM1: struct ip * ip - Pointer to IP header of packet in response to which 
 *         an ICMP Time Exceeded message is being sent
 * PARAM2: char code - code of ICMP Time Exceeded message
 *
 * RETURNS: void
 */

void icmp_timex (struct ip * ip, char code) 
{
   PACKET p;
   struct timex * tx;
   struct ip * pip;
   int i;
   u_char icmp_pkt_len;

#ifdef NPDEBUG
   if (NDEBUG & PROTERR)
      dprintf("icmp: sending ICMP Time Exceeded with code %u to %u.%u.%u.%u\n",
       code, PUSH_IPADDR((ntohs(ip->ip_src))));
#endif   /* NPDEBUG */

   icmp_pkt_len = ICMPTIMEX_HDR_LEN + ip_hlen(ip) + ICMPTIMEX_PAYLOAD_DATA_LEN;
   LOCK_NET_RESOURCE(FREEQ_RESID);
   /* get packet to send ICMP message.  The total length of such a packet is the 
    * length of the "outer" IP header, length of the ICMP header (ICMPTIMEX_HDR_LEN,
    * 8 bytes), length of the "inner" IP header, and length of "user" data 
    * (ICMPTIMEX_PAYLOAD_DATA_LEN, 8 bytes) (just past the "inner" IP header).  The 
    * latter two items are from the packet that the ICMP Time Exceeded message is 
    * being sent in response to. */
   p = pk_alloc(MaxLnh + IPHSIZ + icmp_pkt_len);
   UNLOCK_NET_RESOURCE(FREEQ_RESID);

   if (p == NULL)
   {
#ifdef NPDEBUG
      if (NDEBUG & IPTRACE)
         dprintf("icmp: can't alloc pkt\n");
#endif   /* NPDEBUG */
      icmp_mib.icmpOutErrors++;
      return;
   }

   /* build the addresses in the IP header */
   pip = (struct ip *)p->nb_prot;
   /* source IP address of packet is our address (i.e., destination IP address 
    * in the datagram whose reassembly timed out) */
   pip->ip_src = ip->ip_dest;
   /* the destination address is the same as the source IP address of the 
    * datagram whose reassembly timed out */
   pip->ip_dest = ip->ip_src;

   /* move past space for IP header to get to start of ICMP header */
   p->nb_prot += sizeof(struct ip);

   tx = (struct timex *) p->nb_prot;

   tx->ttype = TIMEX;
   tx->tcode = code;
   tx->tno1 = tx->tno2 = 0;
   MEMCPY(&tx->tip, ip, (ip_hlen(ip) + ICMPTIMEX_PAYLOAD_DATA_LEN));

   tx->tchksum = 0;
   tx->tchksum = ~cksum(tx, (icmp_pkt_len>>1));

   p->nb_plen = icmp_pkt_len;
   /* p->fhost is expected to be in network byte order */
   p->fhost = pip->ip_dest;
   i = ip_write(ICMP_PROT, p);
   if (i < 0)
   {
      icmp_mib.icmpOutErrors++;
#ifdef   NPDEBUG
      if (NDEBUG & (IPTRACE|NETERR|PROTERR))
         dprintf("ICMP: Can't send Time Exceeded\n");
#endif   /* NPDEBUG  */
      return;
   }
   icmp_mib.icmpOutMsgs++;
   icmp_mib.icmpOutTimeExcds++;
   return;
}


/* FUNCTION: icmp_du()
 * 
 *  process an incoming ICMP destination unreachable packet.
 *
 * PARAM1: PACKET p
 * PARAM2: struct destun * pdp
 *
 * RETURNS: void
 */

void  
icmp_du(PACKET p, struct destun * pdp)
{
   /* see if user app wants notification first */
   if (icmpdu_hook)
      icmpdu_hook(p, pdp);

#ifdef INCLUDE_TCP
   /* Tell the sockets layer so it can correct the problem. */
   so_icmpdu(p, pdp);   /* this call should free packet p */
#else

#ifndef IP_PMTU
   /* if it's a datagram-too-big message, ignore it -- As the
    * build isn't using PMTU Discovery this packet is most 
    * probably a Denial of Service Attack.
    */
    if(pdp->dcode == DSTFRAG)
    {
       goto done;
    }
#else  /* IP_PMTU */
   /* if this is a datagram-too-big message, update the Path MTU cache */
   if (pdp->dcode == DSTFRAG)
      pmtucache_set(pdp->dip.ip_dest, htons(pdp->dno2));
#endif

done:
   LOCK_NET_RESOURCE(FREEQ_RESID);
   pk_free(p); /* else just free packet */
   UNLOCK_NET_RESOURCE(FREEQ_RESID);
#endif   /* INCLUDE_TCP */
}

#endif   /* FULL_ICMP */

#ifdef NET_STATS



/* FUNCTION: icmp_stats()
 * 
 * icmp_stats() - Printf info about icmp MIB statistics.
 *
 * PARAM1: void * pio
 *
 * RETURNS: 
 */

int
icmp_stats(void * pio)
{
   ns_printf(pio,"ICMP layer stats:\n");
   ns_printf(pio,"icmpInMsgs %lu    icmpInErrors %lu\n",
    icmp_mib.icmpInMsgs, icmp_mib.icmpInErrors);
   ns_printf(pio,"In counters: DestUnreach %lu   TimeExceed %lu   ParmProb %lu\n",
    icmp_mib.icmpInDestUnreachs, icmp_mib.icmpInTimeExcds,
    icmp_mib.icmpInParmProbs);
   ns_printf(pio,"SrcQuench %lu   Redirect %lu   Echo(ping) %lu   EchoReps %lu\n",
    icmp_mib.icmpInSrcQuenchs, icmp_mib.icmpInRedirects,
    icmp_mib.icmpInEchos, icmp_mib.icmpInEchoReps);
   ns_printf(pio,"Timestmp  %lu   TStmpRep %lu    AddrMasks %lu   AddrMaskRep %lu\n",
    icmp_mib.icmpInTimestamps, icmp_mib.icmpInTimestampReps,
    icmp_mib.icmpInAddrMasks, icmp_mib.icmpInAddrMaskReps);

   ns_printf(pio,"icmpOutMsgs %lu    icmpOutErrors %lu\n",
    icmp_mib.icmpOutMsgs, icmp_mib.icmpOutErrors);
   ns_printf(pio,"Out counts:  DestUnreach %lu   TimeExceed %lu   ParmProb %lu\n",
    icmp_mib.icmpOutDestUnreachs, icmp_mib.icmpOutTimeExcds,
    icmp_mib.icmpOutParmProbs);
   ns_printf(pio,"SrcQuench %lu   Redirect %lu   Echo(ping) %lu   EchoReps %lu\n",
    icmp_mib.icmpOutSrcQuenchs, icmp_mib.icmpOutRedirects,
    icmp_mib.icmpOutEchos, icmp_mib.icmpOutEchoReps);
   ns_printf(pio,"Timestmp  %lu   TStmpRep %lu    AddrMasks %lu   AddrMaskRep %lu\n",
    icmp_mib.icmpOutTimestamps, icmp_mib.icmpOutTimestampReps,
    icmp_mib.icmpOutAddrMasks, icmp_mib.icmpOutAddrMaskReps);

   return 0;
}

#endif   /* NET_STATS */

/* end of file icmp.c */


