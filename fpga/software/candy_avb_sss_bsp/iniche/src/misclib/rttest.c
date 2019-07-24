/*
 * FILENAME: rttest.c
 *
 * Copyright  2001 By InterNiche Technologies Inc. All rights reserved
 *
 * Pseudo driver for speed testing routing & routing and 
 * NAT. This works by by installing itself as an InterNiche MAC 
 * driver which allows the user to generate IP or NAT routing 
 * traffic. 
 *
 * MODULE: MISCLIB
 *
 * ROUTINES: rt_help(), rtcreate_device(), rt_init(), 
 * ROUTINES: rt_pkt_send(), rt_close(), rt_reg_type(), rt_stats(), rt_send(), 
 * ROUTINES: nt_send(), rt_setprot(), rt_statistics(), rt_startup(), rt_done(),
 *
 * PORTABLE: yes
 */


#include "ipport.h"

#ifdef ROUTE_TEST    /* Whole file can be ifdeffed */

#ifndef IP_ROUTING
#error Must define IP_ROUTING to use ROUTE_TEST
#endif   /* IP_ROUTING */

#ifndef DYNAMIC_IFACES
#error Must define DYNAMIC_IFACES to use ROUTE_TEST
#endif   /* DYNAMIC_IFACES */

#ifdef NATRT
#include "../natrt/natport.h"
#endif /* NATRT */

#include "q.h"
#include "netbuf.h"
#include "net.h"
#include "ether.h"
#include "arp.h"
#include "ip.h"
#include "udp.h"
#include "tcp.h"

#include "nvparms.h"
#include "menu.h"

NET      nt_ifp;     /* this driver (the creatyed test driver) */
int      lb_index;   /* index of loopback iface */
int      rt_index;   /* index of this test iface */

int   rtdrv_prep(int ifaces);

int   rt_init(int iface);           /* net initialization routine */
int   rt_pkt_send(struct netbuf *); /* send packet on media */
int   rt_close(int  iface);         /* net close routine */
int   rt_reg_type(unshort type, struct net*);   /* register a MAC type */
void  rt_stats(void * pio, int iface);   

struct nt_stats   {
   long  udp_pkts_sent;
   long  udp_bytes_sent;
   long  tcp_pkts_sent;
   long  tcp_bytes_sent;
   long  udp_pkts_recv;
   long  udp_bytes_recv;
   long  tcp_pkts_recv;
   long  tcp_bytes_recv;
   long  other_pkts;
};

struct nt_stats   ntstat;

int rt_send(void * pio);
int rt_startup(void * pio);
int rt_done(void * pio);
int nt_send(void * pio);
int rt_setprot(void * pio);
int rt_statistics(void * pio);
int rt_help(void * pio);

struct menu_op rt_menu[]   =
{
   "rtest",    stooges,          "Route test",      /* menu ID */
   "rstart",   rt_startup,       "Create route test device",
   "rdone",    rt_done,          "Delete route test device",
   "rsend",    rt_send,          "send packets through route code",
#ifdef NATRT
   "nsend",    nt_send,          "send packets through Nat",
#endif
   "rprot",    rt_setprot,       "set route test protocol (tcp or udp)",
   "rstat",    rt_statistics,    "show/clear route route test statistics",
   "rthelp",   rt_help,          "help with route TEST",
   NULL, 0, NULL,
};



/* FUNCTION: rt_help()
 * 
 * menu routine
 * 
 * PARAM1: void * pio
 *
 * RETURNS: 
 */

int
rt_help(void * pio)
{
   ns_printf(pio, "Use \"rstart\" to create a test iface first, then:\n");
   ns_printf(pio, "\"rsend\" to start a packet burst through router\n");
   ns_printf(pio, "\"nsend\" to start a packet burst through NAT\n");
   ns_printf(pio, "\"rstat\" to display the resulting statstics\n");
   ns_printf(pio, "\"rstat 0\" to reset statistics\n");
   ns_printf(pio, "\"rprot\" to set protocol as 6(TCP) or 17(UDP)\n");
   ns_printf(pio, "\"length\" to set length of test packets\n");
   ns_printf(pio, "\"rdone\" to delete the test iface\n");
   return 0;
}

char *   nat_mac  = "RTRTST";    /* fake MAC address */

u_char   ntprot   =  6;       /* protocol of test pkts, 6==tcp, 17==udp */
u_short  nt_sport =  1200; /* source and dest ports */
u_short  nt_dport =  80;
ip_addr   nt_srcaddr;
ip_addr   nt_destaddr;

extern   int      deflength;     /* use ping packet length */
extern   void     ip_bldhead(PACKET p, 
   unsigned pid, u_char prot, unshort fragword);
extern   void     nat_re_init(void);

static  int    save_nat_inet;
static  int    save_nat_localnet;

/* test driver routines */

/* FUNCTION: rtcreate_device()
 * 
 * the "ni_create()" entry to create a dynamic rttest device.
 * 
 * PARAM1: NET ifp
 * PARAM2: void * bindinfo
 *
 * RETURNS: 0 if OK or an ENP error code
 */

int
rtcreate_device(NET ifp, void * bindinfo)
{
   USE_VOID(bindinfo);

   /* set up  an InterNiche net structure for a fake ethernet interface */
   ifp->n_lnh = ETHHDR_SIZE;        /* space reserved for ethernet header */
   ifp->n_hal = 6;                  /* hardware address length */
   ifp->n_mtu = 1514;               /* max frame size */
   ifp->n_mib->ifAdminStatus = 2;   /* status = down */
   ifp->n_mib->ifOperStatus = 2;    /* will be set up in init()  */
   ifp->n_mib->ifLastChange = cticks * (100/TPS);
   ifp->n_mib->ifType = ETHERNET;
   ifp->n_mib->ifDescr = (u_char*)"Routing/NAT test driver";
   ifp->n_mib->ifPhysAddress = (u_char*)nat_mac;

   /* install our virtual driver routines */
   ifp->n_init = rt_init;
   ifp->pkt_send = rt_pkt_send;
   ifp->n_close = rt_close;
   ifp->n_stats = rt_stats;
   ifp->n_reg_type = rt_reg_type;

   /* set a custom name for ndis device */
   ifp->name[0] = 'r';
   ifp->name[1] = 't';
   ifp->name[2] = 's';

#ifdef INCLUDE_NVPARMS   /* only if using NV parms */
   /* search the NV parameters for iface setup for our name. If this
    * fails we just default to what's already in the ifp.
    */
   if_configbyname(ifp);
#endif   /* INCLUDE_NVPARMS */

   nt_ifp = ifp;     /* save our iface for later use */

   rt_index = if_netnumber(nt_ifp);    /* get numeric index */

   return(rt_init(rt_index));
}


/* interface initialization routine */



/* FUNCTION: rt_init()
 * 
 * PARAM1: int iface
 *
 * RETURNS:  0 if OK or an ENP error code
 */

int
rt_init(int iface)
{
   if (nets[iface] != nt_ifp)
   {
      dtrap(); /* serious code error */
      return ENP_LOGIC;
   }
   return 0;
}





/* FUNCTION: rt_pkt_send()
 * 
 * MAC interface send packet call for test device. In this case
 * it is called whenever a packet sent from this device has been
 * routed through the loopback driver and back to this interface.
 * All we need to do is count them and free the buffer.
 *
 * PARAM1: struct netbuf * pkt
 *
 * RETURNS:  0 if OK or an ENP error code
 */

int
rt_pkt_send(struct netbuf * pkt)
{
   struct ip * pip;
   struct IfMib * mib;

   pip = (struct ip *)(pkt->nb_prot + ETHHDR_SIZE);

   if (pip->ip_prot == 6)
   {
      ntstat.tcp_pkts_recv++;
      ntstat.tcp_bytes_recv += pkt->nb_plen;
   }
   else if(pip->ip_prot == 17)
   {
      ntstat.udp_pkts_recv++;
      ntstat.udp_bytes_recv += pkt->nb_plen;
   }
   else
   {
      ntstat.other_pkts++;
   }

   mib = nt_ifp->n_mib;
   mib->ifOutOctets += pkt->nb_plen;
   mib->ifOutUcastPkts++;

   LOCK_NET_RESOURCE(FREEQ_RESID);
   pk_free(pkt);
   UNLOCK_NET_RESOURCE(FREEQ_RESID);

   return 0;
}



/* FUNCTION: rt_close()
 * 
 * PARAM1: int  iface
 *
 * RETURNS:   0 if OK or an ENP error code
 */

int
rt_close(int  iface)
{
   USE_ARG(iface);
   return 0;
}


/* FUNCTION: rt_reg_type()
 * 
 * register a MAC type - no-op on this device.
 *
 * PARAM1: unshort type
 * PARAM2: struct net * ifp
 *
 * RETURNS:   0 if OK or an ENP error code
 */

int
rt_reg_type(unshort type, struct net * ifp)
{
   USE_ARG(type);
   USE_ARG(ifp);
   return 0;
}




/* FUNCTION: rt_stats()
 * 
 * PARAM1: void * pio
 * PARAM2: int iface
 *
 * RETURNS: void
 */

void
rt_stats(void * pio, int iface)
{
   rt_statistics(pio);
   USE_ARG(iface);
}


/* NAT test menu routines */

#define  TESTIP      1
#define  TESTNAT     2

static int rttest = TESTNAT;



/* FUNCTION: rt_send()
 *
 * Menu routine to generate the stream of test packets into the IP code. This is
 * just a wrapper which sets a flag and call nt_send().
 *
 * PARAM1: void * pio
 *
 * RETURNS: 0 if OK or an ENP error code
 */

int
rt_send(void * pio)
{
   int   err;

#ifdef NATRT
   /* Since test defaults to NAT, we have to unNAT it here. */
   /* stuff the indexes into nvparms structure */
   natrt_nvparms.nat_inet = save_nat_inet;
   natrt_nvparms.nat_localnet = save_nat_localnet;
   nat_re_init();       /* restart NAT with original ifaces */
#endif   /* NATRT */

   rttest = TESTIP;     /* set for IP */
   err = nt_send(pio);
   rttest = TESTNAT;

#ifdef NATRT
   /* stuff the indexes into nvparms structure */
   natrt_nvparms.nat_inet = lb_index;
   natrt_nvparms.nat_localnet = rt_index;
   nat_re_init();       /* restart NAT with original ifaces */
#endif   /* NATRT */

   return err;
}




/* FUNCTION: nt_send()
 * 
 * Menu routine to generate the stream of test packets into the IP or
 * NAT code.
 *
 * PARAM1: void * pio
 *
 * RETURNS: 0 if OK or an ENP error code
 */

int
nt_send(void * pio)
{
   int      count;      /* number of test packets to generate */
   u_long   timer;      /* speed timer for statstics */
   u_long   pkts;       /* local counters for speed test */
   u_long   bytes;
   u_long   timeout;    /* timeout for stuck systems */
   PACKET   master;
   PACKET   tmp;
   struct ethhdr *   eth;
   struct ip *       pip;
   struct tcphdr *   ptcp;
   int      ipid  =  0; /* IP header ID for sent packets */
   char *   arg   =  nextarg( ((GEN_IO)pio)->inbuf );
   struct arptabent *   arp;

   count = atol(arg);
   if (count < 1)
   {
      ns_printf(pio, "usage: rtsend|ntsend count (count is number >1)\n");
      return -1;
   }

   if (deflength > 1506)
   {
      ns_printf(pio, "oversize length (%d) - max test length is 1506\n",
       deflength);
      return -1;
   }
   if (nt_ifp == NULL)
   {
      ns_printf(pio, "Create device with rstart first.\n");
      return -1;
   }

   /* make ARP table entries for both target IP addresses - the one 
    * on the loopback iface, and this one. We test for arp creation,
    * but don't waste message on it since it's fantisticly unlikely.
    */
   arp = make_arp_entry(nt_destaddr, nets[lb_index]);
   if (arp == NULL)
      return ENP_RESOURCE;
   MEMCPY(arp->t_phy_addr, "LPBACK", 6);
   arp = make_arp_entry(nt_srcaddr, nt_ifp);
   if (arp == NULL)
      return ENP_RESOURCE;
   MEMCPY(arp->t_phy_addr, nat_mac, 6);

   ns_printf(pio, "%s test: starting send of %ld pkts, %d bytes each...",
    (rttest == TESTIP)?"Route":"NAT", count, deflength);

   /* build template send packet */
   LOCK_NET_RESOURCE(FREEQ_RESID);
   master = pk_alloc(deflength);
   UNLOCK_NET_RESOURCE(FREEQ_RESID);
   if (master == NULL)
   {
      dprintf("Can't get a buffer for %d bytes\n", deflength);
      return -1;
   }

   eth = (struct ethhdr*)(master->nb_buff + ETHHDR_BIAS);
   eth->e_type = htons(0x0800);     /* put IP packet type in header & pkt */

   master->nb_prot = master->nb_buff + sizeof(struct ip) + ETHHDR_SIZE;
   master->nb_plen = deflength - (sizeof(struct ip) + ETHHDR_SIZE);
   ip_bldhead(master, ipid, ntprot, 0);
   pip = (struct ip *)master->nb_prot;

   pip->ip_src = nt_srcaddr;
   pip->ip_dest = nt_destaddr;

   /* set the port numbers as though pkt is TCP. This will also work 
    * for UDP since UDPs port numbers happen to be at the same offsets.
    */
   ptcp = (struct tcphdr *)(pip + 1);
   ptcp->th_dport = htons(80);
   ptcp->th_sport = htons(1200);

   master->nb_prot += ETHHDR_SIZE;  /* ignore ether header */

   if (rttest != TESTIP)
   {
   }

   /* record starting counts in the iface MIB */
   pkts = nt_ifp->n_mib->ifOutUcastPkts;
   bytes = nt_ifp->n_mib->ifOutOctets;

   timer = cticks;      /* note start time */
   while (count--)
   {
      timeout = cticks + TPS;
      while (bigfreeq.q_len < (bigfreeq.q_max/4))
      {
         tk_yield();
         if (timeout < cticks)
         {
            ns_printf(pio, "timeout waiting for buffers, Aborting...\n");
            break;
         }
      }

      LOCK_NET_RESOURCE(FREEQ_RESID);
      tmp = pk_alloc(deflength);
      UNLOCK_NET_RESOURCE(FREEQ_RESID);
      if (tmp == NULL)
      {
         ns_printf(pio, "error - ran out of buffers!!! Aborting...\n");
         break;
      }

      MEMCPY(tmp->nb_buff, master->nb_buff, 54);

      tmp->type = 0x0800;
      tmp->nb_prot = tmp->nb_buff + ETHHDR_SIZE;
      tmp->nb_plen = master->nb_plen;
      pip = (struct ip *)tmp->nb_prot;
      pip->ip_id = ipid++;
      pip->ip_chksum = 0;
      pip->ip_chksum = ~cksum(pip, ip_hlen(pip) >> 1);

      /* packet is ready. We want it to look like it came from a 
       * private net and is to be NAT routed to the modified MAC 
       * loopback driver, which will swap the IP addersses and port
       * and send it back for a return trip to us. To do this we
       * simple insert it in the reced queue
       */
      tmp->net = nt_ifp;
      putq(&rcvdq, (qp)tmp);
      SignalPktDemux();
   }

   /* conver start time and count to delta time and counts */
   timer = cticks - timer;
   pkts = nt_ifp->n_mib->ifOutUcastPkts - pkts;
   bytes = nt_ifp->n_mib->ifOutOctets - bytes;

   /* pretty-print results */
   ns_printf(pio, "\nDone, %ld and %ld/%d seconds.\n",
                  timer/TPS, timer % TPS, TPS);

   if (timer > 0) /* avoid divide by zero, CPUs hate that... */
   {
      ns_printf(pio, "speed %ld pkts/sec round trip, %ld bytes/sec\n", 
                     (pkts/timer) * TPS, (bytes/timer) * TPS);
      ns_printf(pio, "or %ld pkts/sec one way.\n", 
                     (pkts/timer) * TPS * 2);
   }

   /* don't forget to free the 'master' packet */
   LOCK_NET_RESOURCE(FREEQ_RESID);
   pk_free(master);
   UNLOCK_NET_RESOURCE(FREEQ_RESID);

   return 0;
}



/* FUNCTION: rt_setprot()
 * 
 * Menu routine to set type type of test packet - TCP or UDP.
 * 
 * PARAM1: void * pio
 *
 * RETURNS: 0 if OK or an ENP error code
 */

int
rt_setprot(void * pio)
{
   int   newprot;
   char *   arg   =  nextarg(cbuf);

   newprot = atoi(arg);

   if (newprot == 6)
      ns_printf(pio, "setting Nat test prot to TCP\n");
   else if(newprot == 17)
      ns_printf(pio, "setting Nat test prot to UDP\n");
   else
   {
      ns_printf(pio, "protocol must be 6 (TCP) or 17 (UDP)\n");
      return -1;
   }

   ntprot = (u_char)newprot;
   return 0;
}



/* FUNCTION: rt_statistics()
 * 
 * Menu routine to dump or clear stats.
 * 
 * PARAM1: void * pio
 *
 * RETURNS: 0 if OK or an ENP error code
 */

int
rt_statistics(void * pio)
{
   char *   arg   =  nextarg(cbuf);

   if (arg && (*arg == '0'))
   {
      ns_printf(pio, "Clearing NAT test stats to zero\n");
      ntstat.udp_pkts_sent = 0;
      ntstat.udp_bytes_sent = 0;
      ntstat.tcp_pkts_sent = 0;
      ntstat.tcp_bytes_sent = 0;
      ntstat.udp_pkts_recv = 0;
      ntstat.udp_bytes_recv = 0;
      ntstat.tcp_pkts_recv = 0;
      ntstat.tcp_bytes_recv = 0;
      ntstat.other_pkts = 0;
      return 0;
   }

   ns_printf(pio, "TCP sent; pkts: %ld bytes: %ld\n",
    ntstat.tcp_pkts_sent,
    ntstat.tcp_bytes_sent);
   ns_printf(pio, "TCP recv; pkts: %ld bytes: %ld\n",
    ntstat.tcp_pkts_recv,
    ntstat.tcp_bytes_recv);
   ns_printf(pio, "UDP sent; pkts: %ld bytes: %ld\n",
    ntstat.udp_pkts_sent,
    ntstat.udp_bytes_sent);
   ns_printf(pio, "UDP recv; pkts: %ld bytes: %ld\n",
    ntstat.udp_pkts_recv,
    ntstat.udp_bytes_recv);

   ns_printf(pio, "Other pkts received %ld\n", ntstat.other_pkts);


   return 0;
}



/* FUNCTION: rt_startup()
 * 
 * Menu routine to create the dynamic device which generates the IP or
 * NAT test traffic.
 * 
 * PARAM1: void * pio
 *
 * RETURNS: 0 if OK or an ENP error code
 */

int
rt_startup(void * pio)
{
   int      err;
   NET      tmp;
   ip_addr  tmpip;
   struct niconfig_0 cfg;

   if (nt_ifp != NULL)
   {
      ns_printf(pio, "rt test aaalready has device\n");
      return 0;
   }

   err = ni_create(&tmp, rtcreate_device, "rt0", NULL);
   if (err || (tmp != nt_ifp))
   {
      ns_printf(pio, "rt test:error %d creating test device\n", err);
      return -1;
   }

   /* set up the new ifaces IP address and subnet mask */
   cfg.n_ipaddr = htonl(0xc0a40001);
   cfg.snmask = htonl(0xFFFF0000);
   err = ni_set_config(tmp, &cfg);
   if (err)
   {
      ns_printf(pio, "rt test device config error %d\n", err);
      return -1;
   }

   /* make a source address on the local net */
   tmpip = htonl(nt_ifp->n_ipaddr);       /* start with out local address */
   tmpip++;          /* add 1 - make it not our address */
   nt_srcaddr = htonl(tmpip);

   nt_destaddr = htonl(0x7f000002);       /* set dest addr */

   /* verify there is a loopback iface & keep it's index */
   for (lb_index = 0; lb_index < (int)ifNumber; lb_index++)
   {
      if (nets[lb_index]->n_ipaddr == htonl(0x7f000001))
         break;
   }
   if (lb_index >= (int)ifNumber)
   {
      dprintf("NAT test: Error; need loopback interface\n");
      return ENP_LOGIC;
   }

#ifdef NATRT
   /*
    * Set NAT up with this device as the private net and the
    * loopback device as a public net.
    */
   save_nat_inet = natrt_nvparms.nat_inet;      /* Save nvparms entries */
   save_nat_localnet = natrt_nvparms.nat_localnet;
   /* stuff the indexes into nvparms structure */
   natrt_nvparms.nat_inet = lb_index;
   natrt_nvparms.nat_localnet = rt_index;
   nat_re_init();       /* restart NAT with test ifaces */
#endif   /* NATRT */

   ns_printf(pio, "rt test device ready.\n   device IP %u.%u.%u.%u.\n",
    PUSH_IPADDR(nt_ifp->n_ipaddr) );
   ns_printf(pio, "   Pkt source address: %u.%u.%u.%u\n",
    PUSH_IPADDR(nt_srcaddr));
   ns_printf(pio, "   dest address: %u.%u.%u.%u\n",
    PUSH_IPADDR(nt_destaddr));

   return 0;
}



/* FUNCTION: rt_done()
 * 
 * Menu routine to delete the dynamic device which generates the IP or
 * NAT test traffic.
 * 
 * PARAM1: void * pio
 *
 * RETURNS: 0 if OK or an ENP error code
 */

int
rt_done(void * pio)
{
   int   err;

#ifdef NATRT
   /* Restore the original NAT device proior ot installing this test */
   /* stuff the indexes into nvparms structure */
   natrt_nvparms.nat_inet = lb_index;
   natrt_nvparms.nat_localnet = rt_index;
   nat_re_init();       /* restart NAT with original ifaces */
#endif   /* NATRT */

   err = ni_delete(nt_ifp);
   if (err)
   {
      ns_printf(pio, "rt test device delete error %d\n", err);
      return -1;
   }
   nt_ifp = NULL;
   return 0;
}

#endif   /* ROUTE_TEST */



