/*
 * FILENAME: et_arp.c
 *
 * Copyright 1997-2007 by InterNiche Technologies Inc. All rights reserved
 *
 * Ethernet (and similar media) arp code.
 * 
 * Following is a description of the ARP API
 * 1. Call etainit() during init time to do ARP related initializations
 * 2. Call send_via_arp() to send a packet()
 *    If ARP has an entry, et_send() is called to send the packet.
 *    If ARP doesn't have an entry, a new entry is created, send_arp()
 *       is called to send the ARP request and packet is queued.
 * 3. When a ARP reply is received, arprcv() is called.
 *    It updates the ARP table and sends pending packets 
 *       (related to the received reply).
 * 4. When a ARP request is received, then arpReply() is called
 *    to send an ARP reply.
 * 5. Call arp_stats() to display ARP table
 * 6. When DYNAMIC_IFACES is enabled, call clear_arp_entries() to
 *    clear all interface related entries in the ARP table.
 *
 *
 * MODULE: INET
 *
 * ROUTINES: etainit(), et_send(), arp_free_pending(), 
 * ROUTINES: arp_send_pending(), send_arp(), find_oldest_arp(), 
 * ROUTINES: make_arp_entry(), arpReply(), arprcv(), 
 * ROUTINES: send_via_arp(), arp_stats(), clear_arp_entries(), 
 *
 * PORTABLE: yes
 */

/* Additional Copyrights: */
/* Portions Copyright 1990 by NetPort Software  */

#include "ipport.h"
#include "q.h"
#include "netbuf.h"
#include "net.h"
#include "ether.h"
#include "arp.h"
#include "ip.h"
#include "intimers.h"

/* allow override in ipport.h_h */
#ifndef ARPENT_TMO
#define ARPENT_TMO      10    /* periodic ARP entry timeout (sec) */
#endif

#define arpsize         (ETHHDR_SIZE + sizeof(struct  arp_hdr))

static long arp_timer = 0;    /* periodic timer handle */
void cb_arpent_tmo(long);     /* timer callback function */

                              /* last ARP entry used */
struct arptabent *arpcache = (struct arptabent *)NULL;
static u_long cachetime;      /* time we created arpcache entry */
int arp_ageout = 600 * TPS;   /* APR table refresh age, in ticks */

#ifdef USE_AUTOIP
/* Auto Ip callback */
extern void AutoIp_arp_response(struct arp_hdr * hdr, NET ifp);
#endif

/* arp stats - In addition to MIB */
unsigned    arpReqsIn  = 0;   /* requests received */
unsigned    arpReqsOut = 0;   /* requests sent */
unsigned    arpRepsIn  = 0;   /* replys received */
unsigned    arpRepsOut = 0;   /* replys sent */

struct arptabent  arp_table[MAXARPS];     /* the actual table */

#ifdef IEEE_802_3
u_char snapdata[6] = {170, 170, 3, 0, 0, 0 };  /* per RFC-1042 */
#endif


/* FUNCTION: etainit()
 * 
 * PARAMS: none
 *
 * RETURNS: int               0 if OK, else nonzero
 */

int
etainit(void)
{
   /* register ARP type with the Net Driver */
   if (reg_type(ET_ARP) != 0)
   {
#ifdef NPDEBUG
      dprintf("ARP: unable to register type with MAC Driver\n");
#endif
      return (1);
   }
   return (0);
}



/* FUNCTION: et_send()
 * 
 * Send a packet with ARP information
 *
 * PARAM1: PACKET             packet to send
 * PARAM2: arptabent *        ARP entry pointer
 *
 * RETURNS: int               0 if OK, or the usual ENP_ errors 
 *
 * Fill in an outgoing ethernet-bound packet's ethernet 
 * header and send it. Header info is in arp entry passed
 * and MIB info in the packet header. 
 */

int
et_send(PACKET pkt, struct arptabent *tp)
{
   char *ethhdr;
   IFMIB etif = pkt->net->n_mib;    /* mib info for this ethernet interface */
   int err;

   tp->lasttime = cticks;
   pkt->nb_prot -= ETHHDR_SIZE;  /* prepare for prepending ethernet header */
   pkt->nb_plen += ETHHDR_SIZE;
   ethhdr = pkt->nb_prot + ETHHDR_BIAS;

#ifdef IEEE_802_3
   /* If this arp host uses only SNAP, then use SNAP */
   if ((tp->flags & (ET_ETH2|ET_SNAP)) == ET_SNAP)
   {
      struct snap_hdr * snap;
      pkt->nb_prot -= 8;      /* IEEE SNAP header occupies 8 bytes */
      pkt->nb_plen += 8;
      ethhdr = pkt->nb_prot + ETHHDR_BIAS;
      snap = (struct snap_hdr *)(pkt->nb_prot + ETHHDR_SIZE);
      MEMMOVE(snap, snapdata, 6);
      snap->type = ARPIP;
   }
#endif   /* IEEE_802_3 */

   if (ethhdr < pkt->nb_buff)   /* sanity check pointer */
      panic("et_send: prepend");

   MEMMOVE(ethhdr + ET_DSTOFF, tp->t_phy_addr, 6);  /* set pkt's MAC dst addr */
   MEMMOVE(ethhdr + ET_SRCOFF, etif->ifPhysAddress, 6);  /* MAC src */

   /* nice clean ethernet II header */
   if ((tp->flags & (ET_ETH2|ET_SNAP)) != ET_SNAP)
      ET_TYPE_SET(ethhdr, ntohs(ARPIP));

#ifdef IEEE_802_3
   if ((tp->flags & (ET_ETH2|ET_SNAP)) == ET_SNAP)
   {
      unsigned len8023;

      len8023 = pkt->nb_plen - ETHHDR_SIZE;

      /* As per RFC 1042 :
       * a.) Length should be net endian.
       * b.) Length should include 802.2 LLC and SNAP headers.
       */

      ET_TYPE_SET(ethhdr, len8023);
   }
#endif   /* IEEE_802_3 */

   /* if a packet oriented send exists, use it: */
   if (pkt->net->pkt_send)
      err = pkt->net->pkt_send(pkt);   /* send packet to media */
   else  /* else use older raw_send routine */
   {
      /* sent to media */
      err = pkt->net->raw_send(pkt->net, pkt->nb_prot, pkt->nb_plen);
      LOCK_NET_RESOURCE(FREEQ_RESID);
      pk_free(pkt);
      UNLOCK_NET_RESOURCE(FREEQ_RESID);
   }

   return (err);
}



/* FUNCTION: arp_free_pending()
 *
 * Free any pending packets for a ARP table entry
 * 
 * PARAM1: arptabent *        ARP table entry pointer
 *
 * RETURNS: none
 *
 * Free all pending packets for the specified ARP table entry.
 * Clear the list (entry->pending) after freeing the packets
 * and mark the entry "unused".
 */

void
arp_free_pending(struct arptabent *entry)
{
   PACKET tmppkt;
   PACKET nextpkt;

   /* entry->pending has the linked list of all pending packets */
   tmppkt = entry->pending;
   entry->pending = (PACKET)NULL;

   LOCK_NET_RESOURCE(FREEQ_RESID);

   /* free all pending packets */
   while (tmppkt)
   {
      nextpkt = tmppkt->next;        /* save the next packet in list */
      tmppkt->next = (PACKET)NULL;
      pk_free(tmppkt);               /* free current packet */
      tmppkt = nextpkt;              /* process the next packet */
   }

   entry->t_pro_addr = 0;     /* mark the entry "unused" */

   UNLOCK_NET_RESOURCE(FREEQ_RESID);
}



/* FUNCTION: arp_send_pending()
 * 
 * Send any pending packets for an ARP entry
 *
 * PARAM1: arptabent *        ARP table entry
 *
 * RETURNS: none
 *
 * Send all pending packets for the specified ARP table entry.
 * Clear the list (entry->pending) after sending the packets.
 */

void
arp_send_pending(struct arptabent *entry)
{
   PACKET tmppkt = entry->pending;

   /* entry->pending has the linked list of all pending packets */

   /* send all pending packets */
   while ((tmppkt = entry->pending) != (PACKET)NULL)
   {
      entry->pending = tmppkt->next;   /* unlink the next packet */
      tmppkt->next = (PACKET)NULL;
      et_send(tmppkt, entry);          /* try send again */
   }
}



/* FUNCTION: send_arp()
 * 
 * Send an ARP request packet
 *
 * PARAM1: PACKET             packet to be send
 * PARAM2: ip_addr            destination IP address of packet
 *
 * RETURNS: Returns 0 if OK, or the usual ENP_ errors 
 *
 * Send an arp for outgoing ethernet packet pkt, which has no
 * current arp table entry. This means making a partial entry 
 * and queuing the packet at the entry. Packet will be sent when 
 * target IP host replies to the ARP we send herein. If no reply, 
 * timeout will eventually free packet. 
 */

int
send_arp(PACKET pkt, ip_addr dest_ip)
{
   struct arptabent *   oldest;
   char * ethhdr;
   NET net = pkt->net;
   struct arp_hdr *  arphdr;
   IFMIB etif = pkt->net->n_mib;    /* mib info for this ethernet interface */
   PACKET arppkt;

#ifdef IP_MULTICAST
   /* The following data structure is used to map IP multicast to Ether
    * broadcast.
    */
#ifdef ETHMCAST
   union {
      u_char c[4];
      u_long l;
   } dest_ip_ptr;
#endif /* ETHMCAST */


   /* If we are broadcasting or multicasting ... */
   if ((dest_ip == 0xFFFFFFFF) ||  
      ((dest_ip & ~(net->snmask)) == (0xFFFFFFFF & ~(net->snmask)))
       || (IN_MULTICAST(ntohl(dest_ip)) ))
#else
   if ((dest_ip == 0xFFFFFFFF) ||   /* if we're broadcasting... */
      ((dest_ip & ~(net->snmask)) == (0xFFFFFFFF & ~(net->snmask))))

#endif /* IP_MULTICAST */
   {
      /* get unused or oldest entry in table */
      oldest = make_arp_entry(dest_ip, pkt->net);

      /* set MAC destination to ethernet broadcast (all FFs) */
      MEMSET(oldest->t_phy_addr, 0xFF, 6);
#ifdef IP_MULTICAST
      /* If n_mcastlist routine is defined in the net structure,
         map IP mcast to Ether multicast  */

#ifdef ETHMCAST
      if ((pkt->net->n_mcastlist) && (IN_MULTICAST(ntohl(dest_ip))))
      {
         /* If IP mcast to be mapped to Ethernet multicast */
         dest_ip_ptr.l = dest_ip;
         oldest->t_phy_addr[0] = 0x01;
         oldest->t_phy_addr[1] = 0x00;
         oldest->t_phy_addr[2] = 0x5e;
         oldest->t_phy_addr[3] = (u_char )(dest_ip_ptr.c[1] & 0x7f);
         oldest->t_phy_addr[4] = (u_char )dest_ip_ptr.c[2];
         oldest->t_phy_addr[5] = (u_char )dest_ip_ptr.c[3];
      }
#endif /* ETHMCAST */
#endif /* IP_MULTICAST */
      return (et_send(pkt, oldest));
   }

   /* If packet is addressed to this Ethernet interface, and
    * it's not a loopback address, then don't send it on the wire. 
    * Instead, free the packet and return ENP_NO_ROUTE  
    */
   if ((pkt->fhost == pkt->net->n_ipaddr) &&
      ((pkt->fhost & htonl(0xFF000000)) != htonl(0x7F000000)))
   {
      LOCK_NET_RESOURCE(FREEQ_RESID);
      pk_free(pkt);
      UNLOCK_NET_RESOURCE(FREEQ_RESID);
      return ENP_NO_ROUTE;
   }

   /* not broadcasting, so get a packet for an ARP request */
   LOCK_NET_RESOURCE(FREEQ_RESID); 
   arppkt = pk_alloc(arpsize);
   if (!arppkt)
   {
      pk_free(pkt);
      UNLOCK_NET_RESOURCE(FREEQ_RESID);
      return ENP_RESOURCE;
   }
   UNLOCK_NET_RESOURCE(FREEQ_RESID);
   arppkt->nb_prot = arppkt->nb_buff;
   arppkt->nb_plen = arpsize;
   arppkt->net = pkt->net;

   /* get unused or oldest entry in table */
   oldest = make_arp_entry(dest_ip, pkt->net);

   oldest->pending = pkt;           /* packet is "pended", not pk_free()d */

   /* build arp request packet */
   ethhdr = arppkt->nb_buff + ETHHDR_BIAS;     /* ethernet header at start of buffer */
   arphdr = (struct arp_hdr *)(arppkt->nb_buff + ETHHDR_SIZE); /* arp header follows */

#ifdef IEEE_802_3
   arphdr->ar_hd = ARP8023HW; /* net endian 802.3 arp hardware type (ethernet) */
#else
   arphdr->ar_hd = ARPHW;     /* net endian Ethernet arp hardware type (ethernet) */
#endif /* IEEE_802_3 */

   arphdr->ar_pro = ARPIP;
   arphdr->ar_hln = 6;
   arphdr->ar_pln = 4;
   arphdr->ar_op = ARREQ;
   arphdr->ar_tpa = dest_ip;        /* target's IP address */
   arphdr->ar_spa = pkt->net->n_ipaddr;   /* my IP address */
   MEMMOVE(arphdr->ar_sha, etif->ifPhysAddress, 6);
   MEMSET(ethhdr + ET_DSTOFF, 0xFF, 6);     /* destination to broadcast (all FFs) */
   MEMMOVE(ethhdr + ET_SRCOFF, etif->ifPhysAddress, 6);
   ET_TYPE_SET(ethhdr, ntohs(ET_ARP));

#ifdef NO_CC_PACKING    /* move ARP fields to proper network boundaries */
   {
      struct arp_wire * arwp  =  (struct  arp_wire *)arphdr;
      MEMMOVE(&arwp->data[AR_SHA], arphdr->ar_sha, 6);
      MEMMOVE(&arwp->data[AR_SPA], &arphdr->ar_spa, 4);
      MEMMOVE(&arwp->data[AR_THA], arphdr->ar_tha, 6);
      MEMMOVE(&arwp->data[AR_TPA], &arphdr->ar_tpa, 4);
   }
#endif   /* NO_CC_PACKING */

#ifdef IEEE_802_3
   /* If the build supports 802.3 we'll have to send an ARP with an 802.3
    * header too. Don't return on error codes since we may still be able
    * to send the real ARP request below.
    */
   {
      PACKET pkt8023;
      struct snap_hdr * snap;
      unsigned len8023;

      LOCK_NET_RESOURCE(FREEQ_RESID); 
      pkt8023 = pk_alloc(arpsize + sizeof(struct snap_hdr) );
      UNLOCK_NET_RESOURCE(FREEQ_RESID);
      if (pkt8023)
      {
         pkt8023->nb_prot = pkt8023->nb_buff;
         pkt8023->nb_plen = arpsize + sizeof(struct snap_hdr);
         pkt8023->net = pkt->net;
         ethhdr = pkt8023->nb_buff + ETHHDR_BIAS;
         MEMMOVE(ethhdr, arppkt->nb_buff + ETHHDR_BIAS, 12);
         len8023 = pkt8023->nb_plen - ETHHDR_SIZE;
         ET_TYPE_SET(ethhdr, len8023);
         snap = (struct snap_hdr *)(pkt8023->nb_buff + ETHHDR_SIZE);
         MEMMOVE(snap, snapdata, 6);
         snap->type = ET_ARP;
         MEMMOVE(pkt8023->nb_buff + ETHHDR_SIZE + 8, arphdr, sizeof(struct arp_hdr));

         /* send arp/snap request - if a packet oriented send exists, use it: */
         if (net->pkt_send)
            net->pkt_send(pkt8023);  /* driver should free pkt later */
         else  /* use old raw send */
         {
            net->raw_send(pkt8023->net, pkt8023->nb_buff, arpsize);
            LOCK_NET_RESOURCE(FREEQ_RESID);
            pk_free(pkt8023);
            UNLOCK_NET_RESOURCE(FREEQ_RESID);
         }
         arpReqsOut++;
      }
   }
#endif   /* IEEE_802_3 */

#ifndef IEEE_802_3_ONLY
   /* send arp request - if a packet oriented send exists, use it: */
   if (net->pkt_send)
      net->pkt_send(arppkt);  /* driver should free arppkt later */
   else  /* use old raw send */
   {
      net->raw_send(arppkt->net, arppkt->nb_buff, arpsize);
      LOCK_NET_RESOURCE(FREEQ_RESID);
      pk_free(arppkt);
      UNLOCK_NET_RESOURCE(FREEQ_RESID);
   }
   arpReqsOut++;
#else
   LOCK_NET_RESOURCE(FREEQ_RESID);
   pk_free(arppkt);
   UNLOCK_NET_RESOURCE(FREEQ_RESID);
#endif  /* IEEE_802_3_ONLY */
   
   return ENP_SEND_PENDING;
}



/* FUNCTION: find_oldest_arp()
 *
 * Find an ARP table entry or oldest unused table entry
 *
 * PARAM1: ip_addr            destination IP address
 *
 * RETURNS: arptabent *       LRU or first free entry in arp table
 * 
 * Return LRU or first free entry in arp table - preperatory to 
 * making a new arp entry out of it. The IP address passed is that
 * of new entry so we can recycle a previous entry for that IP,
 * if it exists. 
 *
 * Old entries are removed from the table.
 */

struct arptabent * 
find_oldest_arp(ip_addr dest_ip)
{
   struct arptabent *tp;
   struct arptabent *exact  = (struct arptabent *)NULL;
   struct arptabent *oldest = (struct arptabent *)NULL;
   struct arptabent *empty  = (struct arptabent *)NULL;
   unsigned long lticks = cticks;

   /* find lru (or free) entry */
   for (tp = &arp_table[0]; tp < &arp_table[MAXARPS]; tp++)
   {
      /* age out old, pending entries */
      if (tp->pending)
      {
         /* purge if pending for more than one second */
         if ((lticks - tp->createtime) > TPS)
         {
            arp_free_pending(tp);   /* free pending packets */
            tp->t_pro_addr = 0;     /* mark entry as "unused" */
         }
      }
      else if ((tp->t_pro_addr != 0) &&
               ((int)(lticks - tp->createtime) >= arp_ageout) &&
               ((int)(lticks - tp->lasttime)   >= TPS))
      {
         /* entry has "expired" and has not been reference in 1 sec. */
         tp->t_pro_addr = 0;     /* mark entry as "unused" */
      }

      if (tp->t_pro_addr == dest_ip)   /* ip addr already has entry */
      {
         exact = tp;
      }
      else if (tp->t_pro_addr != 0)
      {
         if (!oldest || (tp->lasttime < oldest->lasttime))
            oldest = tp;
      }
      else if (!empty)
         empty = tp;          /* grab first empty slot */
   }

   return ((exact) ? exact : ((empty) ? empty : oldest));
}



/* FUNCTION: make_arp_entry()
 *
 * Create a new ARP table entry
 *
 * PARAM1: ip_addr            Remote IP address
 * PARAM2: NET                Net interface
 *
 * RETURNS: arptabent *       pointer to arp table entry
 *
 * Find the first unused (or the oldest) arp table entry, and make
 * a new entry to prepare it for an arp reply. If the IP address
 * already has an entry, the entry is returned with only the
 * timestamp modified. The ARP timer is started if it isn't already
 * active.
 */

struct arptabent *   
make_arp_entry(ip_addr dest_ip, NET net)
{
   struct arptabent *oldest;
   unsigned long lticks = cticks;

   /* find usable (or existing) ARP table entry */
   oldest = find_oldest_arp(dest_ip);

   /* If recycling entry, don't leak packets which may be stuck here */
   if (oldest->pending && (oldest->t_pro_addr != dest_ip))
   {
      arp_free_pending(oldest);
   }

   /* partially fill in arp entry */
   oldest->t_pro_addr = dest_ip;
   oldest->net = net;
   oldest->flags = 0;
   MEMSET(oldest->t_phy_addr, '\0', 6);   /* clear mac address */
   oldest->createtime = oldest->lasttime = lticks;

   /* start a ARP timer if there isn't one already */
   /* update the timeout value if there is a timer */
   /* time is specified in milliseconds */
   if (arp_timer == 0)
   {
      arp_timer = in_timerset(&cb_arpent_tmo, ARPENT_TMO * 1000, 0);
   }
   else
   {
      ((struct intimer *)arp_timer)->tmo =
              ((struct intimer *)arp_timer)->interval + lticks;
   }

   return oldest;
}



/* FUNCTION: arpReply()
 *
 * Send an ARP reply
 * 
 * PARAM1: PACKET             ARP request packet
 *
 * RETURNS: none
 *
 * do arp reply to the passed arp request packet. packet 
 * must be freed (or reused) herein. 
 */

void
arpReply(PACKET pkt)
{
   PACKET outpkt;
   struct arp_hdr *in;
   struct arp_hdr *out;
   char *ethout;
   char *ethin;

   LOCK_NET_RESOURCE(FREEQ_RESID);
   outpkt = pk_alloc(arpsize);
   UNLOCK_NET_RESOURCE(FREEQ_RESID);

   if (!outpkt)
   {
      dtrap();
      return;
   }

   outpkt->net = pkt->net;    /* send back out the iface it came from */

   ethin = pkt->nb_prot - (ETHHDR_SIZE - ETHHDR_BIAS);
   ethout = outpkt->nb_buff + ETHHDR_BIAS;

#ifdef IEEE_802_3
   if (MEMCMP(snapdata, pkt->nb_prot - 8, 6) == 0)
   {
      /* it's an IEEE 802.3 SNAP header */
      struct snap_hdr * snap;
      unsigned len8023;
      ethin -= 8;
      in = (struct arp_hdr *)(pkt->nb_prot);
      out = (struct arp_hdr *)(outpkt->nb_buff + ETHHDR_SIZE + 8);
      snap = (struct snap_hdr *)(outpkt->nb_buff + ETHHDR_SIZE);
      outpkt->nb_plen = arpsize + 8;
      len8023 = outpkt->nb_plen - ETHHDR_SIZE;
      ET_TYPE_SET(ethout, len8023);  /* IEEE wants length in type field */
      MEMMOVE(snap, snapdata, 6);
      snap->type = ET_ARP;
   }
   else
#endif   /* IEEE_802_3 */
   {
      ET_TYPE_SET(ethout, ntohs(ET_ARP));   /* 0x0806 - ARP type on ethernet */
      in = (struct arp_hdr *)(pkt->nb_prot);
      out = (struct arp_hdr *)(outpkt->nb_buff + ETHHDR_SIZE);
      outpkt->nb_plen = arpsize;
   }

   /* prepare outgoing arp packet */
#ifdef IEEE_802_3
   out->ar_hd = ARP8023HW; /* net endian 802.3 arp hardware type (ethernet) */
#else
   out->ar_hd = ARPHW;     /* net endian Ethernet arp hardware type (ethernet) */
#endif /* IEEE_802_3 */

   out->ar_pro = ARPIP;
   out->ar_hln = 6;
   out->ar_pln = 4;
   out->ar_op = ARREP;
   out->ar_tpa = in->ar_spa;     /* swap IP addresses */
   out->ar_spa = in->ar_tpa;
   MEMMOVE(out->ar_tha, in->ar_sha, 6);    /* move his MAC address */
   MEMMOVE(out->ar_sha, outpkt->net->n_mib->ifPhysAddress, 6);  /* fill in our mac address */

   /* prepend ethernet unicast header to arp reply */
   MEMMOVE(ethout + ET_DSTOFF, ethin + ET_SRCOFF, 6);
   MEMMOVE(ethout + ET_SRCOFF, outpkt->net->n_mib->ifPhysAddress, 6);

#ifdef NO_CC_PACKING    /* move ARP fields to proper network boundaries */
   {
      struct arp_wire * arwp  =  (struct  arp_wire *)out;
      MEMMOVE(&arwp->data[AR_SHA], out->ar_sha, 6);
      MEMMOVE(&arwp->data[AR_SPA], &out->ar_spa, 4);
      MEMMOVE(&arwp->data[AR_THA], out->ar_tha, 6);
      MEMMOVE(&arwp->data[AR_TPA], &out->ar_tpa, 4);
   }
#endif   /* NO_CC_PACKING */

   /* if a packet oriented send exists, use it: */
   if (outpkt->net->pkt_send)
   {
      outpkt->nb_prot = outpkt->nb_buff;
      outpkt->net->pkt_send(outpkt);
   }
   else
   {
      outpkt->net->raw_send(pkt->net, outpkt->nb_buff, outpkt->nb_plen);
      LOCK_NET_RESOURCE(FREEQ_RESID);
      pk_free(outpkt);
      UNLOCK_NET_RESOURCE(FREEQ_RESID);
   }
   /* input 'pkt' will be freed by caller */
   arpRepsOut++;
}



/* FUNCTION: arprcv()
 *
 * Process an incoming arp packet. 
 * 
 * PARAM1: PACKET             incoming packet
 *
 * RETURNS: int               0 if it was for us
 *                            ENP_NOT_MINE if packet isn't for this IP address
 *                            else a negative error code. 
 */

int
arprcv(PACKET pkt)
{
   struct arp_hdr *arphdr;
   struct arptabent *tp;
   char *eth;
#ifdef IEEE_802_3
   int      ieee = FALSE;     /* TRUE if received packet is 802.3 */
#endif

   eth = pkt->nb_prot - (ETHHDR_SIZE - ETHHDR_BIAS);
   arphdr = (struct arp_hdr *)(pkt->nb_prot);

#ifdef IEEE_802_3
   /* if SNAP header is present, then we have an 802 packet */
   if (MEMCMP(snapdata, pkt->nb_prot - 8, 6) == 0)
   {
      ieee = TRUE;
      eth -= 8;
   }
#ifdef IEEE_802_3_ONLY
   else
      goto drop;
#endif   /* IEEE_802_3_ONLY */
#endif   /* IEEE_802_3 */

#ifdef NO_CC_PACKING    /* force ARP fields to local CPU valid boundaries */
   {
      struct arp_wire * arwp  =  (struct  arp_wire *)arphdr;
      MEMMOVE(&arphdr->ar_tpa, &arwp->data[AR_TPA], 4);
      MEMMOVE(arphdr->ar_tha, &arwp->data[AR_THA], 6);
      MEMMOVE(&arphdr->ar_spa, &arwp->data[AR_SPA], 4);
      MEMMOVE(arphdr->ar_sha, &arwp->data[AR_SHA], 6);
   }
#endif   /* NO_CC_PACKING */

#ifdef USE_AUTOIP
   /* If we're doing Auto Ip, show ARPs to the Auto Ip code. Filter out
    * our own ARPs which echoed from the hardware (NDIS & some real 
    * hardware does this). 
    */
   {
      if (MEMCMP(pkt->net->n_mib->ifPhysAddress, eth + ET_SRCOFF, 6) != 0)
         AutoIp_arp_response(arphdr, pkt->net);
   }
#endif
   USE_ARG(eth);

   /* check ARP's target IP against our net's: */
#ifdef IP_MULTICAST
   if ((arphdr->ar_tpa != pkt->net->n_ipaddr) &&   /* if it's not for me.... */
     (!IN_MULTICAST(ntohl(arphdr->ar_tpa))))
#else
   if (arphdr->ar_tpa != pkt->net->n_ipaddr)
#endif /* IP_MULTICAST */
   {
      LOCK_NET_RESOURCE(FREEQ_RESID);
      pk_free(pkt);     /* not for us, dump & ret (proxy here later?) */
      UNLOCK_NET_RESOURCE(FREEQ_RESID);
      return (ENP_NOT_MINE);
   }

   if (arphdr->ar_op == ARREQ)   /* is it an arp request? */
   {
      arpReqsIn++;   /* count these */
      arpReply(pkt); /* send arp reply */
      /* make partial ARP table entry */
      make_arp_entry(arphdr->ar_spa, pkt->net);
      /* fall thru to arp reply logic to finish our table entry */
   }
   else     /* ARP reply, count and fall thru to logic to update table */
   {
      arpRepsIn++;
   }

   /* scan table for matching entry */
   /* check this for default gateway situations later, JB */
   for (tp = &arp_table[0]; tp < &arp_table[MAXARPS]; tp++)
   {
      if (tp->t_pro_addr == arphdr->ar_spa)     /* we found IP address, update entry */
      {
#ifdef IEEE_802_3
         /* If it's an IEEE SNAP (802.3) set flag in arp table entry */
         if (ieee)
            tp->flags |= ET_SNAP;
         else
            tp->flags |= ET_ETH2;      /* else it's ethernet II */
#endif   /* IEEE_802_3 */

         MEMMOVE(tp->t_phy_addr, arphdr->ar_sha, 6);   /* update MAC adddress */
         tp->lasttime = cticks;
         if (tp->pending)     /* packet waiting for this IP entry? */
         {
            arp_send_pending(tp);
         }
         LOCK_NET_RESOURCE(FREEQ_RESID);
         pk_free(pkt);
         UNLOCK_NET_RESOURCE(FREEQ_RESID);

         return (0);
      }
   }

#ifdef IEEE_802_3_ONLY
drop:
#endif /* IEEE_802_3_ONLY */
   /* fall to here if packet is not in table */
   LOCK_NET_RESOURCE(FREEQ_RESID);
   pk_free(pkt);
   UNLOCK_NET_RESOURCE(FREEQ_RESID);

   return ENP_NOT_MINE;
}



/* FUNCTION: send_via_arp()
 *
 * Send an IP packet on a media which supports ARP
 * 
 * PARAM1: PACKET             packet pointer
 * PARAM2: ip_addr            destination IP address
 *
 * RETURNS: int               0 if packet went to mac sender
 *                            ENP_SEND_PENDING if awaiting arp reply
 *                            SEND_FAILED if error 
 *
 * Send a packet to the target IP address. The IP address may be the
 * packet's dest_ip or a gateway/router. We check the ARP cache (and
 * scan the ARP table if required) for a MAC address matching the
 * passed dest_ip address. If the MAC address is not already known,
 * we broadcast an arp request for the missing IP address and attach
 * the packet to the "pending" pointer. The packet will be sent when
 * the ARP reply comes in, or freed if we time out. 
 *
 * We flush the cache every second to force the regular freeing of any
 * "pending" packets. We flush every entry on the ageout interval so
 * bogus ARP addresses won't get permanently wedged in the table.
 * This happens when someone replaces a PC's MAC adapter but does not
 * change the PC's IP address. 
 */

int
send_via_arp(PACKET pkt, ip_addr dest_ip)
{
   struct arptabent *tp;
   unsigned long lticks = cticks;
   int err;

   /* don't allow zero dest */
   if (dest_ip == 0)
   {
      LOCK_NET_RESOURCE(FREEQ_RESID);
      pk_free(pkt);
      UNLOCK_NET_RESOURCE(FREEQ_RESID);
      return SEND_DROPPED; 
   }

   /* Force refresh of cache once a second */
   if ((lticks - cachetime) > TPS)
      arpcache = (struct arptabent *)NULL;

   /* look at the last ARP entry used. Good chance it's ours: */
   if (arpcache && (arpcache->t_pro_addr == dest_ip))
      tp = arpcache;
   else
   {
      /* scan arp table for an existing entry */
      tp = find_oldest_arp(dest_ip);
   }

   if (tp->t_pro_addr == dest_ip)   /* we found our entry */
   {
      if (tp->pending)  /* arp already pending for this IP? */
      {
         if (lilfreeq.q_len < 2)
         {
            /* system is depleted of resources - free the
             * pkt instead of queueing it - so that we are in a
             * position to receive an arp reply 
             */
            LOCK_NET_RESOURCE(FREEQ_RESID);
            pk_free(pkt);  /* sorry, we have to dump this one.. */
            UNLOCK_NET_RESOURCE(FREEQ_RESID);
            err = SEND_DROPPED;    /* pkts already waiting for this IP entry */
         }
         else
         {
            PACKET tmppkt=tp->pending;

            /* queue the packet in pending list */
            while (tmppkt->next)     /* traverse to the last packet */
               tmppkt = tmppkt->next;
            tmppkt->next = pkt;      /* add new pkt to end of list */
            if (pkt->next)
            {
               dtrap();              /* chain of pkts to be sent ??? */
            }
            err = ENP_SEND_PENDING; /* packet queued pending ARP reply */
         }
      }
      else  /* just send it */
      {
         arpcache = tp;       /* cache this entry */
         cachetime = lticks;  /* mark time we cached */
         err = et_send(pkt, tp);
      }
   }
   else
      /* start the ARP process for this IP address */
      err = send_arp(pkt, dest_ip);

   return (err);
}



#ifdef NET_STATS

/* FUNCTION: arp_stats()
 * 
 * Print ARP statistics to output stream
 *
 * PARAM1: void *             PIO structure
 *
 * RETURNS: int               0 if successful, else error code
 */

int
arp_stats(void *pio)
{
   struct arptabent *atp;
   int   i;
   int   arp_entrys = 0;

   ns_printf(pio, "arp Requests In: %u,   out: %u\n", arpReqsIn, arpReqsOut);
   ns_printf(pio, "arp Replys   In: %u,   out: %u\n", arpRepsIn, arpRepsOut);

   /* count number of arp entrys in use: */
   for (i = 0; i < MAXARPS; i++)
   {
      if (arp_table[i].t_pro_addr != 0)
         arp_entrys++;
   }

   if (arp_entrys > 0)
   {
      ns_printf(pio, "X)  MAC Address   iface pend flags   IP      ctime  ltime\n");
      for (i = 0; i < MAXARPS; i++)
      {
         atp = &arp_table[i];
         if (atp->t_pro_addr != 0)
         {
            ns_printf(pio, "%d)  ", i);

            ns_printf(pio, "%02x%02x%02x-%02x%02x%02x ",
                           atp->t_phy_addr[0],
                           atp->t_phy_addr[1],
                           atp->t_phy_addr[2],
                           atp->t_phy_addr[3],
                           atp->t_phy_addr[4],
                           atp->t_phy_addr[5]);

            ns_printf(pio, "  %d   %s    0x%02x %u.%u.%u.%u   %lu  %lu\n",
                           if_netnumber(atp->net)+1,
                           atp->pending?"Y":"N",
                           atp->flags,
                           PUSH_IPADDR(atp->t_pro_addr),
                           (long)atp->createtime,
                           (long)atp->lasttime);
         }
      }
   }
   else
   {
      ns_printf(pio, "Currently no arp table entrys.\n");
   }

   return 0;
}
#endif   /* NET_STATS */



#ifdef DYNAMIC_IFACES 

/* FUNCTION: clear_arp_entries()
 *
 * Clear selected ARP table entries
 *
 * PARAM1: ip_addr            IP address of entry to clear
 * PARAM2: NET                net interface to clear
 *
 * RETURNS: int               number of entries cleared
 * 
 * Clear any ARP table entries which match the passed IP address
 * or NET. This is for use when a dynamic interface is removed or
 * shut down - we don't want ARP entries persisting.
 */

int
clear_arp_entries(ip_addr dest_ip, NET ifp)
{
   struct arptabent *tp;
   int   deleted = 0;

   /* find and free matching entries */
   for (tp = &arp_table[0]; tp < &arp_table[MAXARPS]; tp++)
   {
      if ((dest_ip && (tp->t_pro_addr == dest_ip)) ||
          (ifp && (ifp == tp->net)))
      {
         if (tp->pending)   /* clear any pending sends */
         {
            arp_free_pending(tp);
         }
         tp->t_pro_addr = 0;  /* clear table entry */
         deleted++;
      }
   }

   return (deleted);
}
#endif /* DYNAMIC_IFACES  */



/* FUNCTION: cb_arpent_tmo()
 *
 * ARP timer callback function
 *
 * PARAM1: long               callback parameter (not used)
 *
 * RETURNS: none
 *
 * Check the ARP table for unresolved entries that have been
 * pending too long. Assume these entries will never be resolved,
 * and free their resources.
 *
 * If there are no more unresolved entries, cancel the timer.
 */
void
cb_arpent_tmo(long arg)
{
   struct arptabent *tp;
   int arp_count = 0;
   unsigned long lticks = cticks;

   for (tp = &arp_table[0]; tp < &arp_table[MAXARPS]; tp++)
   {
      if (tp->t_pro_addr != 0)
      {
         /* age out old, pending entries */
         if (tp->pending && ((lticks - tp->createtime) > TPS))
         {
            /* purge if pending for more than one second */
            arp_free_pending(tp);   /* free pending packets */
            tp->t_pro_addr = 0;     /* mark entry as "unused" */
         }
         else if (((int)(lticks - tp->createtime) >= arp_ageout) &&
                  ((int)(lticks - tp->lasttime)   >= TPS))
         {
            /* entry has "expired" and has not been reference in 1 sec. */
            tp->t_pro_addr = 0;     /* mark entry as "unused" */
         }
         else
            arp_count++;
      }
   }

   /* if there are no more "pending" entries, kill the timer */
   if (arp_count == 0)
   {
      in_timerkill(arp_timer);
      arp_timer = 0;
   }
      
   USE_ARG(arg);
}

/* FUNCTION: grat_arp()
 *
 * grat_arp() - send a gratuitous ARP. 
 *
 * PARAM1 - NET
 * PARAM2 - flag: 0 => ARP, 1 => ARP reply
 *
 * RETURNS: Returns 0 if OK, or the usual ENP_ errors 
 */

int
grat_arp(NET net, int flag)
{
   char * ethhdr;
   struct arp_hdr *  arphdr;
   IFMIB etif = net->n_mib;    /* mib info for this ethernet interface */
   PACKET arppkt;

   /* get a packet for an ARP request */
   LOCK_NET_RESOURCE(FREEQ_RESID); 
   arppkt = pk_alloc(arpsize);
   if (!arppkt)
   {
      UNLOCK_NET_RESOURCE(FREEQ_RESID);
      return ENP_RESOURCE;
   }
   UNLOCK_NET_RESOURCE(FREEQ_RESID);
   arppkt->nb_prot = arppkt->nb_buff;
   arppkt->nb_plen = arpsize;
   arppkt->net = net;

   /* build arp request packet */
   ethhdr = arppkt->nb_buff + ETHHDR_BIAS;     /* ethernet header at start of buffer */
   arphdr = (struct arp_hdr *)(arppkt->nb_buff + ETHHDR_SIZE); /* arp header follows */

#ifdef IEEE_802_3
   arphdr->ar_hd = ARP8023HW; /* net endian 802.3 arp hardware type (ethernet) */
#else
   arphdr->ar_hd = ARPHW;     /* net endian Ethernet arp hardware type (ethernet) */
#endif /* IEEE_802_3 */

   arphdr->ar_pro = ARPIP;
   arphdr->ar_hln = 6;
   arphdr->ar_pln = 4;
   
   /* ARP req? */
   if (flag == 0)
      /* yup */
      arphdr->ar_op = ARREQ;
   else
     /* nope */
      arphdr->ar_op = ARREP;
      
   arphdr->ar_tpa = net->n_ipaddr;        /* target's IP address */
   arphdr->ar_spa = net->n_ipaddr;   /* my IP address */
   MEMMOVE(arphdr->ar_sha, etif->ifPhysAddress, 6);
   MEMSET(ethhdr + ET_DSTOFF, 0xFF, 6);     /* destination to broadcast (all FFs) */
   MEMMOVE(ethhdr + ET_SRCOFF, etif->ifPhysAddress, 6);
   ET_TYPE_SET(ethhdr, ntohs(ET_ARP));

#ifdef NO_CC_PACKING    /* move ARP fields to proper network boundaries */
   {
      struct arp_wire * arwp  =  (struct  arp_wire *)arphdr;
      MEMMOVE(&arwp->data[AR_SHA], arphdr->ar_sha, 6);
      MEMMOVE(&arwp->data[AR_SPA], &arphdr->ar_spa, 4);

   /* ARP req? */
   if (flag == 0)
      /* yup */      
      MEMMOVE(&arwp->data[AR_THA], arphdr->ar_tha, 6);
   else
      /* nope */
      MEMMOVE(&arwp->data[AR_THA], arphdr->ar_sha, 6);
      
      MEMMOVE(&arwp->data[AR_TPA], &arphdr->ar_tpa, 4);
   }
#endif   /* NO_CC_PACKING */

#ifdef IEEE_802_3
   /* If the build supports 802.3 we'll have to send an ARP with an 802.3
    * header too. Don't return on error codes since we may still be able
    * to send the real ARP request below.
    */
   {
      PACKET pkt8023;
      struct snap_hdr * snap;
      unsigned len8023;

      LOCK_NET_RESOURCE(FREEQ_RESID); 
      pkt8023 = pk_alloc(arpsize + sizeof(struct snap_hdr) );
      UNLOCK_NET_RESOURCE(FREEQ_RESID);
      if(pkt8023)
      {
         pkt8023->nb_prot = pkt8023->nb_buff;
         pkt8023->nb_plen = arpsize + sizeof(struct snap_hdr);
         pkt8023->net = arppkt->net;
         ethhdr = pkt8023->nb_buff + ETHHDR_BIAS;
         MEMMOVE(ethhdr, arppkt->nb_buff + ETHHDR_BIAS, 12);
         len8023 = pkt8023->nb_plen - ETHHDR_SIZE;
         ET_TYPE_SET(ethhdr, len8023);
         snap = (struct snap_hdr *)(pkt8023->nb_buff + ETHHDR_SIZE);
         MEMMOVE(snap, snapdata, 6);
         snap->type = ET_ARP;
         MEMMOVE(pkt8023->nb_buff + ETHHDR_SIZE + 8, arphdr, sizeof(struct arp_hdr));

         /* send arp/snap request - if a packet oriented send exists, use it: */
         if (net->pkt_send)
            net->pkt_send(pkt8023);  /* driver should free pkt later */
         else  /* use old raw send */
         {
            net->raw_send(pkt8023->net, pkt8023->nb_buff, arpsize);
            LOCK_NET_RESOURCE(FREEQ_RESID);
            pk_free(pkt8023);
            UNLOCK_NET_RESOURCE(FREEQ_RESID);
         }
         arpReqsOut++;
      }
   }
#endif   /* IEEE_802_3 */

#ifndef IEEE_802_3_ONLY
   /* send arp request - if a packet oriented send exists, use it: */
   if (net->pkt_send)
      net->pkt_send(arppkt);  /* driver should free arppkt later */
   else  /* use old raw send */
   {
      net->raw_send(arppkt->net, arppkt->nb_buff, arpsize);
      LOCK_NET_RESOURCE(FREEQ_RESID);
      pk_free(arppkt);
      UNLOCK_NET_RESOURCE(FREEQ_RESID);
   }
   arpReqsOut++;
#else
   LOCK_NET_RESOURCE(FREEQ_RESID);
   pk_free(arppkt);
   UNLOCK_NET_RESOURCE(FREEQ_RESID);
#endif  /* IEEE_802_3_ONLY */
   
   return ENP_SEND_PENDING;
}



  /* end of file et_arp.c */

