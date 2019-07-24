/*
 * FILENAME: macloop.c
 *
 * Copyright 1997- 2000 By InterNiche Technologies Inc. All rights reserved
 *
 * macloop.c loopback driver to loop packets back at MAC layer, as 
 * opposed to IP layer loopback. This emulates and ethernet driver, 
 * even to the point of supporting ARP.
 *
 * MODULE: INET
 *
 * ROUTINES: prep_lb(), lb_raw_send(), lb_pkt_send(), lb_init(), lb_stats(), 
 * ROUTINES: lb_reg_type(), lb_close(), 
 * PORTABLE: yes
 */

/* Additional Copyrights: */
/* Portions Copyright 1996 by NetPort Software. */

#include "ipport.h"

#ifdef MAC_LOOPBACK  /* whole file can be ifdeffed away */

#include "q.h"
#include "netbuf.h"
#include "net.h"
#include "ether.h"
#include "arp.h"
#include "ip.h"

#ifdef IP_V6
#include "ip6.h"
extern int ip6_dupchecks;
#endif   /* IP_V6 */

#include "tcp.h"

#ifdef INCLUDE_NVPARMS
#include "nvparms.h"       /* for if_configbyname() prototype */
#endif

/* define LB_RAW_SEND to use raw send instead of pkt send. The raw send 
 * is slower, but it's a more realistic test. 
*/
/*
#ifdef LB_RAW_SEND
*/

/* declare the MAC loopback iface routines */
int   lb_init(int iface);
int   lb_reg_type(unshort type, struct net * netp);
int   lb_raw_send(struct net * netp, char * buffer, unsigned length);
int   lb_pkt_send(PACKET pkt);
int   lb_close(int iface);

#ifdef NET_STATS
void  lb_stats(void *, int iface);
#else
#define lb_stats NULL
#endif   /* NET_STATS */

#ifdef DYNAMIC_IFACES   /* dynamic extensions */
int   lb_setstate(struct net * ifp, int state);
int   lb_create(struct net * ifp, void * bindinfo);
#endif   /* DYNAMIC_IFACES */

/* internal */
void  lb_ifsetup(NET netp);

/* these will be set TRUE as types are registered */
int   acceptIP  =  FALSE;
int   acceptARP =  FALSE;

static char lpbhaddr[6]= {'L', 'P', 'B', 'A', 'C', 'K'}; 

static int lbnets = 0;     /* number of loopback nets set up */

/* FUNCTION: prep_lb()
 *
 * Prep routine for static MAC loopback driver. This does
 * it's work be calling lb_ifsetup(), which is shared with the
 * dynamic MAC loopback driver logic.
 * 
 * PARAM1: int iface
 *
 * RETURNS: index of static MAC iface
 */

int
prep_lb(int iface)
{
   if (iface >= STATIC_NETS)   /* Max static entries usued? */
      return(iface);    /* return count we were passed */

   lb_ifsetup(nets[iface]);

   lbnets++;
   return (iface+1); /* return index of next iface */
}

/* FUNCTION: lb_ifsetup()
 *
 * Set up net interface routine pointers, etc, before 
 * trying to init the net. Be sure the space for these is
 * already allocated.
 *
 * 
 * PARAM1: NET structure to be filled in
 *
 * RETURNS: void
 */

void
lb_ifsetup(NET netp)
{
   IFMIB mib;

   netp->n_ipaddr = htonl(0x7f000001);     /* 127.0.0.1 */
   netp->n_defgw  = htonl(0x00000000);
   netp->snmask   = htonl(0xFF000000);
   netp->n_lnh = ETHHDR_SIZE;   /* space reserved for ethernet header */
#ifdef IEEE_802_3
   netp->n_lnh += sizeof(struct snap_hdr);   /* The bloated version */
#endif
   netp->n_hal = 6;    /* hardware address length */
   netp->n_mib->ifType = LOOPIFTYPE;

   /*
    * Yes! In Interniche Stack Lingo, MTU for Ethernet II and IEEE 802.3 is
    * identical. Since the n_mtu field indicates the total frame size sent on
    * the wire, and not MTU in the usual sense. If things are correctly set up
    * then (n_mtu - n_lnh) should give us the actual MTU (or the MTU in the 
    * usual sense of the term.
    */
#ifdef IEEE_802_3
   netp->n_mtu = 1514; /* max frame size */
#else
   netp->n_mtu = 1514; /* max frame size */
#endif

   if(netp->name[0] == 0)
   {
      netp->name[0] = 'l';
      netp->name[1] = 'o';
      netp->name[2] = (char)('0' + lbnets);
   }
   netp->n_flags |= NF_NBPROT;

#ifdef IP_V6
   {
   int i;
   int keepDup;
   char * cp;

   netp->n_flags |= NF_IPV6;  /* set flag for v6 compatability */

   /* set up the standard v6 loopback addresses */
   for(i = IPA_LINK; i <= IPA_GLOBAL; i++)
   {

	   							      IPV6LOG(("+ lb_ifsetup   ip6_dupchecks = %d, i = %d\n", ip6_dupchecks, i));


      keepDup = ip6_dupchecks;  /* loopback can't affect DAD */
      netp->v6addrs[i] = ip6_mkaddr(netp, i, &ip6loopback);
	  ip6_dupchecks = keepDup;
      netp->v6addrs[i]->prefix = 128;
      netp->v6addrs[i]->flags = IA_PREFERRED;

							      IPV6LOG(("+ lb_ifsetup  2 ip6_dupchecks = %d, i = %d\n", ip6_dupchecks, i));


      cp = (char *)&netp->v6addrs[i]->addr;
      switch(i)
        {
         case IPA_LINK:
            *cp++ = (char)0xFE;     /* paste in the link-local prefix */
            *cp   = (char)0x80;
            break;
         case IPA_SITE:
            *cp++ = (char)0xFE;     /* paste in the site-local prefix */
            *cp   = (char)0xC0;
            break;
         case IPA_GLOBAL:     /* leave it all zeros */
            break;
         default:
            dtrap();
         }
	  				              IPV6DUMP("+ lb_ifsetup addr = \n", (char *)&netp->v6addrs[i]->addr, 16);
							      IPV6LOG(("+ lb_ifsetup   netp->v6addrs[i]->flags = %d\n", netp->v6addrs[i]->flags));
   }
   }
#endif   /* IP_V6 */

   /* search the NV parameters for iface setup for our name. If this
    * fails we just default to what's already in the ifp.
    */
#ifdef INCLUDE_NVPARMS
   if_configbyname(netp);
#endif
#ifdef DYNAMIC_IFACES   /* dynamic extensions */
   netp->n_setstate = lb_setstate;
#endif  /* DYNAMIC_IFACES */

   mib = netp->n_mib;
   mib->ifAdminStatus = 2;  /* status = down */
   mib->ifOperStatus = 2;   /* will be set up in init()  */
   mib->ifLastChange = cticks * (100 / TPS);
   mib->ifPhysAddress = (u_char *)&lpbhaddr[0];
   mib->ifDescr = (u_char *)"loopback (pseudo ethernet)";
   mib->ifIndex = if_netnumber(netp);

#ifdef IP_V6
   /* IPv6 has very specific requirements for loopback behaviour */
   mib->ifType = LOOPIFTYPE;
#else
   /* For IPV4 it's convenient to masquerade as ethernet, so we can
    * test ARP, DHCP, etc., in loopback.
    */
   mib->ifType = ETHERNET;
#endif

   mib->ifSpeed = 10000000;
   mib->ifMtu = 1500;         /* ethernetish default */

   /* install our hardware driver routines */
   netp->n_init = lb_init;
#ifdef LB_RAW_SEND
   netp->raw_send = lb_raw_send;
#else /* use packet send */
   netp->pkt_send = lb_pkt_send;
#endif   /* LB_RAW_SEND */
   netp->n_close = lb_close;
   netp->n_stats = lb_stats;
}


#ifdef LB_RAW_SEND
/* FUNCTION: lb_raw_send()
 *
 * lb_raw_send() - this loopback send routine goes through the 
 * motions as though it's going to send a packet on an ethernet 
 * device, then just drops it in the received packet queue. 
 *
 * 
 * PARAM1: struct net * netp
 * PARAM2: char * buffer
 * PARAM3: unsigned length
 *
 * RETURNS: 0 if OK, else ENP_ error code
 */

int
lb_raw_send(struct net * netp, char * buffer, unsigned length)
{
   struct ethhdr *   eth;
   IFMIB mib;
   PACKET pkt;

#ifdef NPDEBUG
   /* Sanity check interface pointer */
   if(netp->raw_send != lb_raw_send)
   {
      dprintf("macloop: bad net\n");
      dtrap();
   }
#endif

   /* Don't send if iface is logically down */
   if(netp->n_mib->ifAdminStatus != NI_UP)
   {
      netp->n_mib->ifOutDiscards++; /* bump mib counter for these */
      return ENP_LOGIC;    /* right thing to do? */
   }

   /* maintain mib xmit stats */
   mib = netp->n_mib;
   if (*buffer & 0x01)  /* see if multicast bit is on */
      mib->ifOutNUcastPkts++;
   else
      mib->ifOutUcastPkts++;
   mib->ifOutOctets += length;

   /* at this point we make the logical switch from sending to receiving */

   /* fill in a packet for the "received" buffer */
   LOCK_NET_RESOURCE(FREEQ_RESID);
   pkt = pk_alloc(length);
   UNLOCK_NET_RESOURCE(FREEQ_RESID);
   if (!pkt)
   {
      mib->ifInDiscards++;
      return ENP_RESOURCE;
   }
   MEMCPY(pkt->nb_buff, buffer, length);
   pkt->nb_prot = pkt->nb_buff + ETHHDR_SIZE;   /* point to IP header */
   pkt->nb_plen = length - ETHHDR_SIZE;   /* IP length */
   pkt->net = netp;
   eth = (struct ethhdr *)(pkt->nb_buff + ETHHDR_BIAS);
   MEMCPY(eth->e_dst, (void *)lpbhaddr, 6);
   MEMCPY(eth->e_src, (void *)lpbhaddr, 6);
   pkt->type = eth->e_type;

   mib->ifInOctets += length;

   /* queue the packet in rcvdq */
   putq(&rcvdq, (q_elt)pkt);

   /* Most ports should now wake the packet demuxer task */
   SignalPktDemux();

   return 0;   /* OK return */
}
#else
/* FUNCTION: lb_raw_send()
 *
 * lb_raw_send() - this loopback send routine goes through the 
 * motions as though it's going to send a packet on an ethernet 
 * device, then just drops it in the received packet queue. 
 *
 * 
 * PARAM1: struct net * netp
 * PARAM2: char * buffer
 * PARAM3: unsigned length
 *
 * RETURNS: 0 if OK, else ENP_ error code
 */

int
lb_pkt_send(PACKET pkt)
{
   NET      netp;
   IFMIB    mib;
   int      length;
   struct ethhdr *   eth;
#ifdef ROUTE_TEST
   struct ip *       pip;  /* headers for packet alterations */
   struct tcphdr *   ptcp;
#endif /* ROUTE_TEST */

   netp = pkt->net;

#ifdef NPDEBUG
   /* Sanity check interface pointer */
   if(netp->pkt_send != lb_pkt_send)
   {
      dprintf("macloop: bad net\n");
      dtrap();
   }
#endif

   length= pkt->nb_plen;
   mib = netp->n_mib;

   /* Don't send if iface is logically down */
   if(mib->ifAdminStatus != NI_UP)
   {
      mib->ifOutDiscards++; /* bump mib counter for these */
      return ENP_LOGIC;    /* right thing to do? */
   }

   /* maintain mib xmit stats */
   if (*(pkt->nb_prot + ETHHDR_BIAS) & 0x01)  /* see if multicast bit is on */
      mib->ifOutNUcastPkts++;
   else
      mib->ifOutUcastPkts++;
   mib->ifOutOctets += length;

   /* at this point we make the logical switch from sending to receiving. 
    * nb_prot, nb_plen and type should be set to the MAC (ethernet) header
    * by the send_via_arp() code.
    */
   eth = (struct ethhdr *)(pkt->nb_prot + ETHHDR_BIAS);
   pkt->type = eth->e_type;
   pkt->nb_prot += ETHHDR_SIZE;
   pkt->nb_plen -= ETHHDR_SIZE;
   mib->ifInOctets += length;

#ifdef IEEE_802_3
   /* See if sender is legacy IEEE machine. If we were not using the 
    * NF_NBPROT bit in this device we could skip this step and the packet
    * would get trapped by the logic in pktdemux.c
    */
   if(pkt->type >= 0x0600)
   {
      struct snap_hdr * snap;

      /* set up type and nb_prot for SNAP packet */
      snap = (struct snap_hdr *)(((char*)eth) + ETHHDR_SIZE - ETHHDR_BIAS);

      pkt->type = snap->type;
      pkt->nb_prot += sizeof(struct snap_hdr);
      pkt->nb_plen -= sizeof(struct snap_hdr);
   }
#endif

#ifdef ROUTE_TEST
   pip = (struct ip *)pkt->nb_prot;    /* hunt for IP header */

   /* If the two IP addreses are not both 127.1, then we may be doing a 
    * routing simulation. Try swapping the IP addresses so the "received"
    * packet doesn't keep looping back to this driver 
    */
   if((eth->e_type == htons(0x0800)) &&   /* make sure we found IP packet */
      (pip->ip_ver_ihl == 0x45) &&        /* and that we found IP header */
      (pip->ip_src != pip->ip_dest))
   {
      ip_addr tmp;      /* tmp vars for swapping */
      unshort port;

      tmp = pip->ip_src;         /* swap IP addresses */
      pip->ip_src = pip->ip_dest;
      pip->ip_dest = tmp;

      /* If it's UDP or TCP, swap the ports too. This lets us do NAT 
       * simulations.
       */
      if((pip->ip_prot == 6) ||     /* 6 - TCP */
         (pip->ip_prot == 17))      /* 17 - UDP */
      {
         /* use the tcp header struct. The UDP ports are in the same 
          * locations, so this works for UDP too.
          */
         ptcp = (struct tcphdr *)(pip + 1);
         port = ptcp->th_sport;
         ptcp->th_sport = ptcp->th_dport;
         ptcp->th_dport = port;
      }
   }
#endif   /* ROUTE_TEST */

   /* queue the packet in rcvdq */
   putq(&rcvdq, (q_elt)pkt);

   /* Most ports should now wake the packet demuxer task */
   SignalPktDemux();

   return 0;   /* OK return */
}

#endif /* LB_RAW_SEND cs packet send */



/* FUNCTION: lb_init()
 *
 * This is the statis iface init routine.
 *
 * PARAM1: int iface
 *
 * RETURNS: 
 */

int
lb_init(int iface)
{
   IFMIB mib;

   mib = nets[iface]->n_mib;
   mib->ifAdminStatus = 1;    /* set to UP */
   mib->ifOperStatus = 1;
   mib->ifLastChange = cticks * (100/TPS);

   return 0;
}

#ifdef NET_STATS


/* FUNCTION: lb_stats()
 *
 * lb_stats() - the nets[] stats routine for the MAC loopback driver
 *
 * 
 * PARAM1: void * pio
 * PARAM2: int iface
 *
 * RETURNS: 
 */

void
lb_stats(void * pio, int iface)
{
   IFMIB mib;

   mib = nets[iface]->n_mib;
   ns_printf(pio,"status for MAC level looback driver\n");
   ns_printf(pio,"Types registered: ARP:%s, IP:%s\n", 
    acceptARP?"YES":"NO", acceptIP?"YES":"NO" );
   ns_printf(pio,"status: ADMIN:%s OPERATIONAL:%s\n",
    (mib->ifAdminStatus==1)?"UP":"DOWN",
    (mib->ifOperStatus==1)?"UP":"DOWN");
}

#endif   /* NET_STATS */



/* FUNCTION: lb_reg_type()
 * 
 * PARAM1: unshort type
 * PARAM2: struct net * netp
 *
 * RETURNS: 
 */

int
lb_reg_type(unshort type, struct net * netp)
{
   if (type == htons(0x0800))
      acceptIP = TRUE;
   else if(type == htons(0x0806))
      acceptIP = TRUE;
   else
      return -1;  /* this is an error - we only do ARP and IP */

   USE_ARG(netp);
   return 0;
}



/* FUNCTION: lb_close()
 * 
 * PARAM1: int iface
 *
 * RETURNS: 
 */

int
lb_close(int iface)
{
   IFMIB mib;

   mib = nets[iface]->n_mib;
   acceptIP = FALSE;
   acceptARP = FALSE;
   mib->ifAdminStatus = 2; /* set both SNMP MIB statuses to disabled */
   mib->ifOperStatus = 2;
   return 0;
}

#ifdef DYNAMIC_IFACES   /* dynamic extensions */

int
lb_setstate(struct net * ifp, int state)
{
   /* no physical work to do on loopback device. Just set state */
   if(state == NI_UP)
   {
      ifp->n_mib->ifAdminStatus = 1; /* set both SNMP MIB statuses to enabled */
      ifp->n_mib->ifOperStatus = 1;
   }
   else
   {
      ifp->n_mib->ifAdminStatus = 2; /* set both SNMP MIB statuses to disabled */
      ifp->n_mib->ifOperStatus = 2;
   }
   return 0;
}

int
lb_create(struct net * ifp, void * bindinfo)
{
   USE_VOID(bindinfo);

   lb_ifsetup(ifp);
   ifp->name[2] = '0' + ifNumber;   /* tag name with device index */
   return 0;
}

#endif   /* DYNAMIC_IFACES */

#endif   /* MAC_LOOPBACK - whole file can be ifdeffed away */



