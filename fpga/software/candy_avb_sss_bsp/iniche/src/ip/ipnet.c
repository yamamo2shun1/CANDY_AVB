/*
 * FILENAME: ipnet.c
 *
 * Copyright  2000 By InterNiche Technologies Inc. All rights reserved
 *
 * System code called from ip_startup(), et.al. to do low level
 * initialization of network structures, packet queues, etc.
 *
 * MODULE: INET
 *
 * ROUTINES: Netinit(), netclose(), net_stats(), pktdemux(), 
 * ROUTINES: c_older(), ip2mac(),
 *
 * PORTABLE: yes
 */

/* Additional Copyrights: */
/* Portions Copyright 1990-1994 by NetPort Software  */
/*  Copyright 1986 by Carnegie Mellon  */
/*  Copyright 1984, 1985 by the Massachusetts Institute of Technology  */


#include "ipport.h"
#include "libport.h"
#include "q.h"
#include "netbuf.h"
#include "net.h"
#include "ip.h"

#ifdef INCLUDE_ARP
#include "arp.h"
#endif

#include "ether.h"

#ifdef DYNAMIC_IFACES   /* Install dynamic iface menu */
#ifdef IN_MENUS   /* need this too */
#include "menu.h"
extern struct menu_op dynif_menu[];
#endif /* IN_MENUS */
#endif /* DYNAMIC_IFACES */

#ifdef USE_PPPOE
int   poe_rcv(PACKET pkt);
#endif   /* USE_PPPOE */

#ifdef  LOSSY_IO
/* support for an option that allows us to deliberatly loose packets */
int   in_lastloss = 0;  /* packets since last loss */
int   in_lossrate = 10; /* number of packets to punt (30 is "1 in 30") */
static unsigned long myrandVal = ((unsigned long)0x29823745);
static unsigned long myrandMods = 222;
unsigned long myrandNum  = 1;
unsigned long myrandDenom = 20;
#endif   /* LOSSY_IO */


queue    rcvdq;         /* queue of all received (but undemuxed) packets */

int      MaxLnh   =  0;       /* Length of the biggest local net header in the whole system */
int      MaxMtu   =  0;       /* length of biggest MAC packet */

unsigned NDEBUG = 0;       /* Debugging...*/

void   fixup_subnet_mask(int);


/* FUNCTION: Netinit()
 *
 * Netinit() - This routine does the basic initialization of the IP 
 * protocol stack. It assumes the clock is already initialized. It 
 * also assumes the net[] structures have been at least partially set 
 * up, with n_mtu, n_lnh and init() variables set; and that ifNumber 
 * has been set to the number of nets[]. It does (in order): - Packet 
 * buffer initialization, creation of the free queue. - Per net 
 * interface init. . 
 *
 * 
 * PARAM1: none
 *
 * RETURNS: Netinit() returns 0 if everything inited OK, else 
 * a non-zero error code
 */

int
Netinit()
{
   int   i; /* general counter variable */
   int   e; /* error holder */

#ifndef MULTI_HOMED
   ifNumber = 1;     /* prevents weird behavior below... */
#endif

   /* set our largest header size and frames size */
   for (i = 0; i < (int)ifNumber; i++)
   {
      /* sanity check on mtu, et.al. We added this because earlier 
       * drivers were sloppy about setting them, but new
       * logic depends on these sizes.
       */
      if (nets[i]->n_mib->ifType == ETHERNET)   /* ethernet? */
      {
         if (nets[i]->n_mtu == 0)   /* let device code override */
            nets[i]->n_mtu = 1514;

         if (nets[i]->n_lnh == 0)
         {
#ifdef IEEE_802_3
            nets[i]->n_lnh = ETHHDR_SIZE + sizeof(struct snap_hdr);
#else
            nets[i]->n_lnh = ETHHDR_SIZE;
#endif
         }
      }
#ifdef IP_V6
      /* Ignore IPv6 tunnels for lnh and mtu values */
      if((nets[i]->n_mib->ifType == IP6GIF) ||
         (nets[i]->n_mib->ifType == IP6TO4))
      {
         continue;
      }
#endif   /* IP_V6 */

      MaxLnh = max(MaxLnh, nets[i]->n_lnh);
      MaxMtu = max(MaxMtu, nets[i]->n_mtu);
   }

   /* set up the received packet queue */
   rcvdq.q_head = rcvdq.q_tail = NULL;
   rcvdq.q_max = rcvdq.q_min = rcvdq.q_len = 0;

   /* initialize freeq */
   LOCK_NET_RESOURCE(FREEQ_RESID);
   e = pk_init();
   UNLOCK_NET_RESOURCE(FREEQ_RESID);
   if (e)   /* report error (memory ran out?) */
      return e;

   /* packet buffers in freeq are now all set. */
   exit_hook(netclose);       /* Clean up nets when we are unloaded */

   /* now do the per-IP interface initializations */
   for (i = 0; i < (int)ifNumber; i++)
   {
      if (nets[i]->n_init != NULL)  /* If iface has init routine... */
      {
         if ((e = (*nets[i]->n_init)(i)) != 0)  /* call init routine */
         {
            dprintf("init error %d on net[%d]\n", e, i);
            nets[i]->n_mib->ifOperStatus = NI_DOWN;
            continue;   /* ignore ifaces which fail */
         }
         /* If interface is ethernet, set bcast flag bit. This
          * should really be done by the init routine, but we handle it
          * here to support MAC drivers which predate the flags field.
          */
         if(nets[i]->n_mib->ifType == ETHERNET)
            nets[i]->n_flags |= NF_BCAST;

         /* set ifAdminStatus in case init() routine forgot to. IfOperStatus
          * is not nessecarily up at this point, as in the case of a modem which
          * is now in autoanswer mode.
          */
         nets[i]->n_mib->ifAdminStatus = NI_UP;

         /* assign default names to unnamed ifaces */
         if(nets[i]->name[0] == 0)     /* no name set by prep or init */
         {
            if(nets[i]->n_mib->ifType == ETHERNET)
            {
               nets[i]->name[0] = 'e';    /* "et1", "et2", etc. */
               nets[i]->name[1] = 't';
            }
            else
            {
               nets[i]->name[0] = 'i';    /* "if1", "if2", etc. */
               nets[i]->name[1] = 'f';
            }
            nets[i]->name[2] = (char)(i + '1');
            nets[i]->name[3] = '\0';
         }
      }
      /* check on subnet routing - if no mask then make one */
      fixup_subnet_mask(i);      /* make mask for this net */

      /* build broadcast addresses */
      if(nets[i]->n_ipaddr != 0)
      {
         nets[i]->n_netbr = nets[i]->n_ipaddr | ~nets[i]->snmask;
         nets[i]->n_netbr42 = nets[i]->n_ipaddr & nets[i]->snmask;
         nets[i]->n_subnetbr = nets[i]->n_ipaddr | ~nets[i]->snmask;
      }
   }

#if defined(DYNAMIC_IFACES) && defined(IN_MENUS)
   /* Install dynamic iface menu */
   install_menu(&dynif_menu[0]);
#endif /* DYNAMIC_IFACES && IN_MENUS */

   return(0);
}

/* FUNCTION: fixup_subnet_mask()
 *
 * fixup_subnet_mask() - If not subnet mask has been set by the 
 * port's init code, make adefault on form the IP address 
 *
 * 
 * PARAM1: int netnum
 *
 * RETURNS: 
 */

void
fixup_subnet_mask(int netnum)      /* which of the nets[] to do. */
{
   u_long   smask;

   if (nets[netnum]->snmask)  /* if mask is already set, don't bother */
      return;

   /* things depending on IP address class: */
   if ((nets[netnum]->n_ipaddr & AMASK) == AADDR)
      smask = 0xFF000000L;
   else if((nets[netnum]->n_ipaddr & BMASK) == BADDR)
      smask = 0xFFFF0000L;
   else if((nets[netnum]->n_ipaddr & CMASK) == CADDR)
      smask = 0xFFFFFF00L;
   else
   {
      dtrap();    /* bad logic or setup values */
      smask = 0xFFFFFF00L;
   }
   nets[netnum]->snmask = htonl(smask);
}


/* FUNCTION: netclose()
 *
 * netclose() - This routine is called to shut down the nets. It 
 * calls the net_close routine associated with each network interface 
 * which should release the adapter and any other resources which it 
 * might have been using. 
 *
 * 
 * PARAM1: 
 *
 * RETURNS: 
 */

void
netclose()
{
   NET ifp;
   int index = 0;

#ifdef NPDEBUG
   if (NDEBUG & INFOMSG)   dprintf("netclose() called\n");
#endif

   for (ifp = (NET)netlist.q_head; ifp; ifp = ifp->n_next)
   {
      if (ifp->n_close)
      {
         dprintf("netclose: closing iface %s\n", ifp->n_mib->ifDescr);
         (*(ifp->n_close))(index++);
      }
      else
      {
#ifdef NPDEBUG
         if (NDEBUG & INFOMSG) dprintf("net %s: no close routine!\n", ifp->name);
#endif
         index++;
      }
   }
}


#ifdef SHARED_IPADDRS
int   add_share_route(PACKET pkt);

/* FUNCTION: add_share_route()
 *
 * Add a route table entry which forces Ip replies to the sender out the
 * same interface the packet came in on. 
 * 
 * PARAM1: IP pkt, with np_prot pointingto header.
 *
 * RETURNS: 0 if oK, else ENP_ error code
 */

int
add_share_route(PACKET pkt)
{
   int      iface;
   ip_addr  nexthop;
   RTMIB    rt;
   struct ip * pip;
   NET      ifp;

   pip = (struct ip * )pkt->nb_prot;
   rt = rt_lookup(pip->ip_src);
   ifp = pkt->net;               /* net we received packet on */
   iface = if_netnumber(ifp);    /* We need net index too.... */

   /* if we already have a route, just patch the net & return. */
   if(rt && (rt->ipRouteDest == pip->ip_src))
   {
      rt->ifp = ifp;
      return 0;
   }

   /* Figure out what to use for a "next hop". If it appears
    * to be a local IP address, then use the IP address of the
    * sender; else if there is a route get the GW from that, else if
    * the iface has one use that. As a last resort, fall back to using
    * the sender's address as a next hop.
    */
   if((ifp->n_ipaddr & ifp->snmask)  ==
      (pip->ip_src & ifp->snmask))
   {
         nexthop = pip->ip_src;
   }
   else if(rt)
      nexthop = rt->ipRouteNextHop;
   else if(pkt->net->n_defgw)
      nexthop = ifp->n_defgw;
   else
      nexthop = pip->ip_src;


   /* Now make an explicit route back to the sender */
   rt = add_route(pip->ip_src, 0xFFFFFFFF, nexthop, iface, IPRP_OTHER);

   if(rt)
      return 0;
   else
      return ENP_RESOURCE;
}
#endif /* SHARED_IPADDRS */


/* FUNCTION: pktdemux()
 *
 * pktdemux() - The received packet demuxing task. We try to run this 
 * whenever there are unprocessed packets in rcvdq. It dequeues the 
 * packet and sends it to a protocol stack based on type. 
 *
 * 
 * PARAM1: none
 *
 * RETURNS: void
 */

void
pktdemux()
{
   PACKET   pkt;
   NET      ifc;                /* interface packet came from */
   IFMIB    mib;
   int      pkts;
   char *   eth;

   pkts = 0;   /* packets per loop */

   while (rcvdq.q_len)
   {
      /* If we are low on free packets, don't hog CPU cycles */
      if (pkts++ > bigfreeq.q_len)
      {
#ifdef SUPERLOOP
         return;        /* don't hog stack on superloop */
#else    /* SUPERLOOP */
         tk_yield(); /* let application tasks process received packets */
         pkts = 0;   /* reset counter */
#endif   /* SUPERLOOP else */
      }

      /* If we get receive interupt from the net during this
      lock, the MAC driver needs to wait or reschedule */
      LOCK_NET_RESOURCE(RXQ_RESID);
      pkt = (PACKET)q_deq(&rcvdq);
      UNLOCK_NET_RESOURCE(RXQ_RESID);
      if (!pkt) panic("pktdemux: got null pkt");
      ifc = pkt->net;

      mib = ifc->n_mib;
      /* maintain mib stats for unicast and broadcast */
      if (isbcast(ifc, (u_char*)pkt->nb_buff + ETHHDR_BIAS))
         mib->ifInNUcastPkts++;
      else
         mib->ifInUcastPkts++;

      if(mib->ifAdminStatus == NI_DOWN)
      {
         LOCK_NET_RESOURCE(FREEQ_RESID);
         pk_free(pkt);  /* dump packet from downed interface */
         UNLOCK_NET_RESOURCE(FREEQ_RESID);
         mib->ifInDiscards++;
         continue;      /* next packet */
      }

#ifdef NPDEBUG
      if (*(pkt->nb_buff - ALIGN_TYPE) != 'M' ||
          *(pkt->nb_buff + pkt->nb_blen) != 'M')
      {
         dtrap();
         panic("pktdemux: corrupt pkt");
      }
#endif   /* NPDEBUG */

#ifdef  LOSSY_IO
      if(NDEBUG & LOSSY_RX)
      {
         if(myloss())  
         {
            LOCK_NET_RESOURCE(FREEQ_RESID);
            pk_free(pkt);        /* punt packet */
            UNLOCK_NET_RESOURCE(FREEQ_RESID);
            in_lastloss = (int)(cticks & 0x07) - 4;  /* pseudo random reset */
            continue;            /* act as if we sent OK */
         }
      }
#endif   /* LOSSY_IO */

      /* see if driver set pkt->nb_prot and pkt->type */
      if((ifc->n_flags & NF_NBPROT) == 0)
      {
         /* Set pkt->type and pkt->nb_prot based based on media type.
          * Some device drivers pass nb_plen as the total length of the
          * packet, while others subtract the MAC header. The latter is
          * probably the right thing to do, but because of this historic
          * inconsistency we don't try to fix it here - the longer size
          * turns out to be harmless since the IP layer fixes the size
          * based on the IP header length field.
          */
         switch(ifc->n_mib->ifType)
         {
         case ETHERNET:
            /* get pointer to ethernet header */
            eth = (pkt->nb_buff + ETHHDR_BIAS);
#ifdef IEEE_802_3
            /* see if it's got snap baggage */
            if (ET_TYPE_GET(eth) <= 0x0600)
            {
               struct snap_hdr *snap;
               snap = (struct snap_hdr *)(pkt->nb_buff + ETHHDR_SIZE);
               pkt->type = (unshort)(snap->type);
               pkt->nb_prot = pkt->nb_buff + pkt->net->n_lnh;
            }
            else
            {
               pkt->type = htons((unshort)ET_TYPE_GET(eth));
               pkt->nb_prot = pkt->nb_buff + ETHHDR_SIZE;
            }
#else
            pkt->type = htons((unshort)ET_TYPE_GET(eth));
            pkt->nb_prot = pkt->nb_buff + pkt->net->n_lnh;
#endif   /* IEEE_802_3 */
            break;
#if defined(USE_PPP) || defined(USE_SLIP) || defined(PROTOCOL_LIBS)
         case PPP:   /* PPP or SLIP over a UART */
         case SLIP:
            pkt->nb_prot = pkt->nb_buff + MaxLnh;
            pkt->type = IPTP;    /* only type our PPP supports */
            break;
#endif  /* USE_PPP || USE_SLIP */
#ifdef USE_PPPOE
         case PPPOE:
            /* do not change type yet, for PPPoE */
            break;
#endif   /* USE_PPPOE */
         default:    /* driver bug? */
            dprintf("pktdemux: bad Iface type %ld\n",ifc->n_mib->ifType);
            LOCK_NET_RESOURCE(FREEQ_RESID);
            pk_free(pkt);
            UNLOCK_NET_RESOURCE(FREEQ_RESID);
            continue;
         }
      }

      /* pkt->nb_prot and pkt->type are now set. pass pkt to upper layer */
      switch(pkt->type)
      {
      case IPTP:     /* IP type */
         LOCK_NET_RESOURCE(NET_RESID);
#ifdef SHARED_IPADDRS
         add_share_route(pkt);
#endif /* SHARED_IPADDRS */
#ifdef IP_V4
         ip_rcv(pkt);
#else
            /* don't care, it's IPv4 */
            LOCK_NET_RESOURCE(FREEQ_RESID);
            pk_free(pkt);
            UNLOCK_NET_RESOURCE(FREEQ_RESID);
#endif
		UNLOCK_NET_RESOURCE(NET_RESID);
         break;
#ifdef INCLUDE_ARP
      case ARPTP:       /* ARP type */
         LOCK_NET_RESOURCE(NET_RESID);
         arprcv(pkt);
         UNLOCK_NET_RESOURCE(NET_RESID);
         break;
#endif   /* INCLUDE_ARP */
#ifdef USE_PPPOE
      case  htons(0x8863):
      case  htons(0x8864):
         LOCK_NET_RESOURCE(NET_RESID);
         poe_rcv(pkt);
         UNLOCK_NET_RESOURCE(NET_RESID);
         break;
#endif   /* USE_PPPOE */
#ifdef IP_V6
      case  htons(0x86DD):
         /* Each received v6 pkt goes thru here exactly once, so set the
          * outer (first, and usually only) ipv6 header pointer. Tunneled headers
          * may exist further into the packet.
          */
         pkt->ip6_hdr = (struct ipv6 *)pkt->nb_prot;
         LOCK_NET_RESOURCE(NET_RESID);
         ip6_rcv(pkt);
         UNLOCK_NET_RESOURCE(NET_RESID);
         break;
#endif
      default:
#ifdef NPDEBUG
         if (NDEBUG & UPCTRACE)
            dprintf("pktdemux: bad pkt type 0x%04x\n", ntohs(pkt->type));
#endif   /* NPDEBUG */
         ifc->n_mib->ifInUnknownProtos++;
         LOCK_NET_RESOURCE(FREEQ_RESID);
         pk_free(pkt);           /* return to free buffer */
         UNLOCK_NET_RESOURCE(FREEQ_RESID);
         break;
      }
      continue;
   }
}


/* FUNCTION: c_older()
 *
 * c_older(ct1, ct2) - return the older of two tick counts. Assumes 
 * both were set set by "ctX = cticks;" or other refernce to cticks. 
 * Also assumes the two times are within a few days of each other. 
 *
 * 
 * PARAM1: u_long ct1
 * PARAM2: u_long ct2
 *
 * RETURNS: the older of the two passed tick counts
 */

u_long
c_older(u_long ct1, u_long ct2)
{

   if (!(cticks & 0x80000000) || /* cticks has not wrapped recently, or */
       (ct1 <= cticks && ct2 <= cticks) || /* both are below cticks or */
       (ct1 >= cticks && ct2 >= cticks))   /* both are above cticks */
   {
      if (ct1 < ct2)
         return(ct1);      /* then smaller is oldest */
      else 
         return(ct2);
   }

   /* else one is less than cticks, and one is greater.
   the larger value is then the oldest */
   if (ct1 >= ct2)
      return(ct1);
   else
      return(ct2);
}



/* FUNCTION: ip2mac()
 *
 * Takes as input an outgoing IP packet with no MAC info 
 * and trys to send it on the right MAC layer. For serial links (PPP) 
 * just sends it to the iface driver. 
 * 
 * 
 * PARAM1: PACKET pkt
 * PARAM2: ip_addr dest_ip
 *
 * RETURNS: Returns 0 if packet went to mac sender; ENP_SEND_PENDING if 
 * awaiting arp reply; ENP_NO_ROUTE if iface is down, or SENDERR if other 
 * error.
 */

int
ip2mac(PACKET pkt,         /* the packet itself, all set but for dest MAC address */
   ip_addr  dest_ip)    /* the IP host or gateway to get MAC addr for */
{
   IFMIB ifmib = pkt->net->n_mib;   /* mib info for this interface */

#ifdef  LOSSY_IO
   if(NDEBUG & LOSSY_TX)
   {
      if(myloss())
      {		  
		  LOCK_NET_RESOURCE(FREEQ_RESID);
         pk_free(pkt);                 /* punt packet */
         UNLOCK_NET_RESOURCE(FREEQ_RESID);
         in_lastloss = (int)(cticks & 0x07) - 4;  /* pseudo random reset */
         return 0;                     /* act as if we sent OK */
      }
   }
#endif   /* LOSSY_IO */

   /* Always punt if iface ifAdminStatus is DOWN. ifOperStatus may 
    * be down too, but our packet may be the event required to bring 
    * it up - so don't worry about ifOperStatus here.
    */
   if(ifmib->ifAdminStatus == NI_DOWN)
   {
      LOCK_NET_RESOURCE(FREEQ_RESID);
      pk_free(pkt);
      UNLOCK_NET_RESOURCE(FREEQ_RESID);
      return(ENP_NO_ROUTE);
   }

#ifdef LINKED_PKTS
   /* If packet gather is not supported by the driver, then collect the
    * fragments into one big buffer. Caller should have checked that buffer
    * is smaller than driver's MTU.
    */
   if (((pkt->net->n_flags & NF_GATHER) == 0) &&   /* no gather flag, and... */
       (pkt->pk_next))                             /* ...packet is scattered. */
   {
      pkt = pk_gather(pkt, pkt->net->n_lnh);
      if (!pkt)
         return ENP_NOBUFFER;
   }
#endif   /* LINKED_PKTS */

   /* some interfaces (ie SLIP) just get the raw IP frame - no ARP needed */
   if ((pkt->net->n_lnh == 0) ||    /* no MAC header */
       (ifmib->ifType == PPP) ||     /* or PPP or SLIP... */
       (ifmib->ifType == SLIP))
   {
      ifmib->ifOutUcastPkts++;   /* maintain MIB counters */
      ifmib->ifOutOctets += pkt->nb_plen;

      /* send packet on media */
      if (pkt->net->pkt_send) /* favor using packet send */
         pkt->net->pkt_send(pkt);   /* pkt will be freed by MAC code */
      else  /* no packet send; try raw send */
      {
         pkt->net->raw_send(pkt->net, pkt->nb_prot, pkt->nb_plen);
         LOCK_NET_RESOURCE(FREEQ_RESID);
         pk_free(pkt);
         UNLOCK_NET_RESOURCE(FREEQ_RESID);
      }
      return(SUCCESS);
   }

   /* don't allow unicast sends if NIC iface has no IP address. This
    * is to prevent DHCP clients from sending prior to assignment.
    */
   if (pkt->net->n_ipaddr == 0L)
   {
      if (pkt->fhost != 0xFFFFFFFF) /* check for broadcast packet */
      {
         LOCK_NET_RESOURCE(FREEQ_RESID);
         pk_free(pkt);
         UNLOCK_NET_RESOURCE(FREEQ_RESID);
         return ENP_SENDERR;
      }
   }

#ifdef INCLUDE_ARP   /* must be ethernet or token ring */
   return(send_via_arp(pkt, dest_ip));
#else
   dtrap();    /* Bad option combination? */
   return ENP_NO_IFACE; /* sent to unknown interface type */
#endif   /* INCLUDE_ARP */
}

#ifdef  LOSSY_IO
/* 
 * FUNCTION myrand(void)
 * returns: unsigned int, pseudo random integer between 0 and 255.
 */

unsigned int
myrand(void)
{
	unsigned int ret;
	int i;
	union
	{
		unsigned long L;
		unsigned char C[4];
	} U;

		U.L = myrandVal;		
		U.L += U.L % myrandMods;
		myrandVal = U.L;
		for (i = 0, ret = 0; i < 4; i++)
			ret += U.C[i];
		ret &= 0xff;
		myrandMods = ret % 256 + 128 * myrandMods;

	return ret;
}

/* 
 * FUNCTION myloss(void)
 * returns: int, 1 occurs ~ (myrandNum / myrandDenom) of the time.
 *               0 occurs ~ ( 1 - (myrandNum / myrandDenom)) of the time.
 */

int
myloss(void)
{
	if (myrand() < (myrandNum * 255 ) / myrandDenom)
	   return 1;

	return 0;
}
#endif
