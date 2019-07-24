/*
 * FILENAME: testmenu.c
 *
 * Copyright  2000 By InterNiche Technologies Inc. All rights reserved
 *
 *
 * MODULE: MISCLIB
 *
 * ROUTINES: pktwait(), setpktct(), ping_flood(), arp_flood (),
 *
 * PORTABLE: yes
 */


#include "ipport.h"

#ifdef TESTMENU   /* whole file can be ifdeffed away */

#ifndef IN_MENUS
#error TEXTNEBU requires 
#endif   /* IN_MENUS */
 
#include "q.h"
#include "netbuf.h"
#include "net.h"
#include "ether.h"
#include "arp.h"
#include "ip.h"
#include "icmp.h"
#include "udp.h"

#ifdef INCLUDE_SNMP
#include "snmpport.h"    /* lengths for nvparms SNMP strings */
#endif

#include "nvparms.h"
#include "menu.h"
#include "in_utils.h"

#ifdef DNS_CLIENT
#include "dns.h"
#endif   /* DNS_CLIENT */

#ifdef PING_APP
#include "app_ping.h"
#endif

extern   int   deflength;  /* int ..\misclib\app_ping.c */
extern   unsigned lilbufsiz;
extern   unsigned bigbufsiz;

int      ping_flood(void * pio);
int      arp_flood(void * pio);
int      setpktct(void * pio);
int      udp_flood(void * pio);
int      setfport(void * pio);
int      flood_addr(void * pio);
int      bigq_drain(void * pio);
int      pktq_drain(void * pio);
int      freeq_refill(void * pio);
int      bigq_refill(void * pio);


static   u_short  fport = 5001;  /* default UDP port */

long     pktcount =  100;

#ifdef NOTDEF  /* code for main.c: */
#ifdef TESTMENU   /* after menus.h */
extern   struct menu_op testmenu[10];
#endif   /* TESTMENU */
#ifdef TESTMENU   /* after ping_init() */
install_menu(testmenu);
#endif   /* TESTMENU */
#endif   /* NOTDEF */



struct menu_op testmenu[]  =  {  /* array of menu option, see menu.h */
   "test",  stooges, "test menu" ,    /* menu ID */
   "fping",    ping_flood,    "flood pings, don't wait for reply" ,
   "farp",     arp_flood,     "flood ARPS, don't wait for reply" ,
   "fcount",   setpktct,      "flood packet count",
   "fport",    setfport,      "flood packet port number (UDP)",
   "fudp",     udp_flood,     "send UDP flood",
   "faddr",    flood_addr,    "add/delete/list UDP flood addresses",
   "bqdrain",  bigq_drain,    "drain [arg] pkts fropm big freeq",
   "qdrain",   pktq_drain,    "drain all free pkt queue (forces pkt loss errors)",
   "bqrefill", bigq_refill,   "restore [arg] drained big queue pkts",
   "qrefill",  freeq_refill,  "restore all drained queue pkts",
   NULL,
};




/* FUNCTION: pktwait()
 *
 * pktwait() - utility routine for flood tests - waits until a packet 
 * of deflen size if free. Times out after a few seconds if we;re 
 * stuck . Returns 0 if packet is ready, else -1 if timeout 
 *
 * 
 * PARAM1: char * app
 * PARAM2: void * pio
 *
 * RETURNS: 
 */

int
pktwait(char * app, void * pio)
{
   u_long   tmo;

   tmo = cticks + (3 * TPS);  /* 3 second timeout */
   if (deflength > (int)lilbufsiz)
   {
      while (bigfreeq.q_len < 1)
      {
         tk_yield();
         if (tmo < cticks)
         {
            ns_printf(pio, "%s flood timeout\n", app);
            return -1;
         }
      }
   }
   else  /* can use a little packet */
   {
      while (lilfreeq.q_len < 1)
      {
         tk_yield();
         if (tmo < cticks)
         {
            ns_printf(pio, "%s flood timeout\n",app);
            return -1;
         }
      }
   }
   return 0;
}



/* FUNCTION: setpktct()
 * 
 * PARAM1: void * pio
 *
 * RETURNS: 
 */

int
setpktct(void * pio)
{
   char *   cp =  nextarg(((GEN_IO)pio)->inbuf);

   if (!*cp)   /* no arg given */
   {
      ns_printf(pio,"flood packet count is %ld\n", pktcount);
      ns_printf(pio,"To change it, put new number on command line\n");
      return -1;
   }
   pktcount = atol(cp);

   return 0;
}



/* FUNCTION: ping_flood()
 * 
 * PARAM1: void * pio
 *
 * RETURNS: 
 */

int
ping_flood(void * pio)
{
   long  i,e;

   ns_printf(pio, "sending ping flood of %ld pkts to %u.%u.%u.%u..", 
    pktcount, PUSH_IPADDR(activehost) );

   for (i = 0; i < pktcount; i++)
   {
      if (pktwait("ping", pio))
         return -1;

      e = icmpEcho(activehost, NULL, deflength, (unshort)i);
      if (e < 0)
      {
         ns_printf(pio, "ping flood send error %d on pkt %ld\n",e,i);
         return -1;
      }
      if ((i & 0x0f) == 0x0f)
         ns_printf(pio, ".");
   }
   return 0;
}

#define  arpsize  (ETHHDR_SIZE   +  sizeof(struct  arp_hdr))




/* FUNCTION: arp_flood ()
 * 
 * PARAM1: void * pio
 *
 * RETURNS: 
 */

int
arp_flood (void * pio)
{
   PACKET arppkt;
   struct ethhdr *   ethhdr;
   struct arp_hdr *  arphdr;
   NET net;
   long  i;
   int   e;
   ip_addr ipaddr;

#ifdef MULTI_HOMED
   ip_addr phost;    /* phoney host for pass to iproute */
   net = iproute(activehost, &phost);
#else
   net = nets[0];
#endif

   if (!net)
   {
      ns_printf(pio, "ARP flood: no route");
      return -1;
   }

   ns_printf(pio, "sending ARP flood of %ld pkts to %u.%u.%u.%u..", 
    pktcount, PUSH_IPADDR(activehost) );


   for (i = 0; i < pktcount; i++)
   {
      if (pktwait("ARP", pio))
         return -1;

      /******** code cribbed from et_arp.c: ********/
      LOCK_NET_RESOURCE(FREEQ_RESID);
      arppkt = pk_alloc(arpsize);
      UNLOCK_NET_RESOURCE(FREEQ_RESID);
      if (!arppkt)
         return ENP_RESOURCE;
      arppkt->nb_prot = arppkt->nb_buff;
      arppkt->nb_plen = arpsize;
      arppkt->net = net;

      /* build arp request packet */
      ethhdr = (struct ethhdr *)(arppkt->nb_buff + ETHHDR_BIAS);     /* ethernet header at start of buffer */
      arphdr = (struct arp_hdr *)(arppkt->nb_buff + ETHHDR_SIZE); /* arp header follows */
      arphdr->ar_hd = ARPHW;     /* net endian arp hardware type (ethernet) */
      arphdr->ar_pro = ARPIP;
      arphdr->ar_hln = 6;
      arphdr->ar_pln = 4;
      arphdr->ar_op = ARREQ;
      arphdr->ar_tpa = activehost;        /* target's IP address */

      /* FLOOD TEST MOD: just for grins, rotate our IP address so we 
       * flood everybody's arp tables. Remember that we store IP 
       * addresses 
       */
      ipaddr = i & (0x00FFFFFE & htonl(~net->snmask));   /* make host portion */
      arphdr->ar_spa = (net->n_ipaddr | htonl(ipaddr));  /* add net portion */

      MEMCPY(arphdr->ar_sha, net->n_mib->ifPhysAddress, 6);
      MEMSET(&(ethhdr->e_dst[0]), 0xFF, 6);     /* destination to broadcast (all FFs) */
      MEMCPY(ethhdr->e_src, net->n_mib->ifPhysAddress, 6);
      ethhdr->e_type = ET_ARP;   /* 0x0806 - ARP type on ethernet */

#ifdef NO_CC_PACKING    /* move ARP fields to proper network boundaries */
      {
         struct arp_wire * arwp  =  (struct  arp_wire *)arphdr;
         MEMMOVE(&arwp->data[AR_SHA], arphdr->ar_sha, 6);
         MEMMOVE(&arwp->data[AR_SPA], &arphdr->ar_spa, 4);
         MEMMOVE(&arwp->data[AR_THA], arphdr->ar_tha, 6);
         MEMMOVE(&arwp->data[AR_TPA], &arphdr->ar_tpa, 4);
      }
#endif   /* NO_CC_PACKING */

      /* send arp request - if a packet oriented send exists, use it: */
      if (net->pkt_send)
         e = net->pkt_send(arppkt);    /* driver should free arppkt later */
      else  /* use old raw send */
      {
         e = net->raw_send(arppkt->net, arppkt->nb_buff, arpsize);
         LOCK_NET_RESOURCE(FREEQ_RESID);
         pk_free(arppkt);
         UNLOCK_NET_RESOURCE(FREEQ_RESID);
      }

      arpReqsOut++;
      /******** end of code cribbed from et_arp.c: ********/

      if (e < 0)
      {
         ns_printf(pio, "ARP flood send error %d on pkt %ld\n",e,i);
         return -1;
      }
      if ((i & 0x0f) == 0x0f)
         ns_printf(pio, ".");
   }

   return 0;
}

int
setfport(void * pio)
{
   char *   cp =  nextarg(((GEN_IO)pio)->inbuf);

   if (!*cp)   /* no arg given */
   {
      ns_printf(pio,"flood port number is %ld\n", fport);
      ns_printf(pio,"To change it, put new number on command line\n");
      return -1;
   }
   fport = (u_short)atoi(cp);
   return 0;
}

#define MAXFLOODTARGETS 20
ip_addr floodtargs[MAXFLOODTARGETS];

int
udp_flood(void * pio)
{
   int      i;          /* index */
   int      e;          /* tmp error holder */
   int      send_err;   /* total send error count */
   int      no_pkt;     /* times packet get failed */
   u_short  lport;      /* local port for send (random) */
   PACKET   p;
   int      addr_index; /* index into floodtargs[] */

   ns_printf(pio, "sending UDP flood of %ld pkts to %u.%u.%u.%u\n", 
      pktcount, PUSH_IPADDR(activehost) );

   lport = udp_socket();
   e = send_err = no_pkt = 0;

   addr_index = 0;
   for (i = 0; i < pktcount; i++)
   {
      p = udp_alloc((unsigned)deflength, 0);        /* get packet for sending */
      if(!p)
      {
/*         ns_printf(pio, "pkt alloc failed on send #%d\n", i); */
         no_pkt++;
         continue;
      }
      p->nb_plen = deflength;
      /* if floodtargs array has any entries rotate through them, else send all
       * of the packets to activehost
       */
      if(floodtargs[0] == 0)
         p->fhost = activehost;
      else
      {
         p->fhost = floodtargs[addr_index++];   /* get next host */
         if(floodtargs[addr_index++] == 0)      /* wrap if at end of list */
            addr_index = 0;
      }
      e = udp_send(lport, fport, p);
      if(e)
      {
         send_err++;
/*          ns_printf(pio, "UDP send error %d on %d\n", e, i); */
         continue;
      }
   }
   ns_printf(pio, "sent %d pkts, %d errors, %d no-pkt, last return code: %d\n", 
      i - no_pkt, send_err, no_pkt, e);
   return e;
}


int
flood_addr(void * pio)
{
   char *   cp =  nextarg(((GEN_IO)pio)->inbuf);
   char     arg;     /* first char of second arg - a, d, or l. */
   char *   hosttext;
   ip_addr  host;
   unsigned i;

   if (!*cp)   /* no arg given */
      goto fa_usage;

   arg = ((*cp) | 0x40);   /* force arg to lowercase */

   if(arg == 'l')     /* user wants list */
   {
      ns_printf(pio, "Current flood target list:\n");
      for(i = 0; i < MAXFLOODTARGETS; i++)
      {
         if (floodtargs[i] == 0)
            break;
         ns_printf(pio, "%u.%u.%u.%u\n", PUSH_IPADDR(floodtargs[i]));
      }
      return 0;
   }
   /* make sure arg is 'a' or 'd' ('l' is handled above */
   if((arg != 'a') && (arg != 'd'))
   {
      ns_printf(pio, "Arg must begin with 'a' or 'd'\n");
      goto fa_usage;
   }
   hosttext = nextarg(cp);
   if(!(*hosttext))
      goto fa_usage;

   cp = parse_ipad(&host, &i, hosttext);
   if(cp)
   {
      ns_printf(pio, "%s\n", cp);
      goto fa_usage;
   }

   for(i = 0; i < MAXFLOODTARGETS; i++)
   {
      if((arg == 'a') && (floodtargs[i] == 0))
      {
         floodtargs[i] = host;
         ns_printf(pio, "Added host %u.%u.%u.%u\n", PUSH_IPADDR(host));
         return 0;
      }
      if((arg == 'd') && (floodtargs[i] == host))
      {
         floodtargs[i] = 0;
         ns_printf(pio, "Deleted host %u.%u.%u.%u\n", PUSH_IPADDR(host));
         for( ; i < (MAXFLOODTARGETS - 1); i++)    /* move up rest of list */
            floodtargs[i] = floodtargs[i + 1];
         floodtargs[i] = 0;      /* clear last entry */
         return 0;
      }
   }
   ns_printf(pio, "Sorry, %s\n", arg=='a'?"flood target table full":"Host not in list");
   return -1;

fa_usage:
   ns_printf(pio, "Usage: faddr <add|delete|list> X.X.X.X\n");
   return -1;
}

/* tmp storage for queues */
struct queue bigqback;
struct queue lilqback;

/* q_drain() - utility routine to drain qsrc into qdest */

static void
q_drain(struct queue * qsrc, struct queue * qdest)
{
   while(qsrc->q_len)
   {
      putq(qdest, getq(qsrc));
   }
}

int
bigq_drain(void * pio)
{
   int count;

   count = atoi(nextarg(((GEN_IO)pio)->inbuf)); /* get count from arg */
   if(count == 0)    /* if no count given, move whole queue */
      count = bigfreeq.q_len;

   ns_printf(pio, "draining %d pkts from bigfreeq into tmp que\n", count);
   while((count--) && (bigfreeq.q_len))
      putq(&bigqback, getq(&bigfreeq));
   return 0;
}

int
pktq_drain(void * pio)
{
   ns_printf(pio, "draining both free queue into tmp que\n");
   q_drain(&bigfreeq, &bigqback);
   q_drain(&lilfreeq, &lilqback);
   return 0;
}

int
freeq_refill(void * pio)
{
   ns_printf(pio, "restoring free pkts queues\n");
   q_drain(&bigqback, &bigfreeq);
   q_drain(&lilqback, &lilfreeq);
   return 0;
}

int
bigq_refill(void * pio)
{
   int count;

   count = atoi(nextarg(((GEN_IO)pio)->inbuf)); /* get count from arg */
   if(count == 0)    /* if no count given, move whole queue */
      count = bigfreeq.q_len;

   ns_printf(pio, "restoring %d pkts to bigfreeq from tmp que\n", count);
   while((count--) && (bigqback.q_len))
      putq(&bigfreeq, getq(&bigqback));
   return 0;
}

#endif   /* TESTMENU - whole file can be ifdeffed away */


