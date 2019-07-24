/*
 * FILENAME: nrmenus.c
 *
 * Copyright 1997- 2000 By InterNiche Technologies Inc. All rights reserved
 *
 * Software Menus for InterNiche sample/demo TTY console.
 *
 * MODULE: MISCLIB
 *
 * ROUTINES: do_dtrap(), ishexdigit(), hexnibble(), 
 * ROUTINES: atoh(), atohl(), hex2bin(), agent(), setcommstr(), dumpqueues(), 
 * ROUTINES: dumpbufs(), qname(), showver(), sendtrap(), dump_bytes(), 
 * ROUTINES: menu_exit(), udpecho(), tcpecho(), show_routes(), 
 * ROUTINES: mn_add_route(), set_ipaddr(), gethostbynametest(), gethost2test()
 * ROUTINES: ping_sethost(), ping_setdelay(), ping_setlength(),
 *
 * PORTABLE: yes
 */

/* Additional Copyrights: */
/* Portions Copyright 1990-1996 by NetPort */


#include "ipport.h"


#ifdef IN_MENUS

#include "in_utils.h"

#include "q.h"          /* need these 3 to pull in ifNumber */
#include "netbuf.h"
#include "net.h"
#include "ether.h"
#include "arp.h"
#include "ip.h"
#include "icmp.h"
#include "udp.h"
#include "menu.h"
#include "app_ping.h"
#include "pmtu.h"

#include "dns.h"

#include "nvparms.h"

#ifdef INCLUDE_INICHE_LOG
#include "iniche_log.h"
#endif

#ifdef IP_V6
#include "ip6.h"
#endif   /* IP_V6 */

#ifndef TIMEFOR1TICK
#define TIMEFOR1TICK       (1000/TPS)
#endif

#define  t_time   unsigned long

extern   int   station_state(void * pio);
extern   int   mh_stats(void * pio);
extern   int   set_upc(void * pio);
extern   int   iface_stats(void * pio);
extern   int   linkstats(void * pio);
extern   int   dumpbufs(void * pio);
extern   int   dump_buf_estats (void * pio);
#ifdef HEAPBUFS
extern   int   dump_hbuf_stats (void * pio);
#endif
extern   int   dumpqueues(void * pio);
extern   int   showver (void * pio);
extern   int   dns_state (void * pio);
extern   int   set_nv_params (void * pio);
extern   int   edit_nv_params(void * pio);

#ifdef INCLUDE_TCP
#ifndef MINI_TCP
extern   int   mbuf_stat(void * pio);
extern   int   mbuf_list(void * pio);
#endif   /* MINI_TCP */
extern   int   tcp_stat(void * pio);
extern   int   sock_list(void * pio);
extern   int   tcp_bsdconnstat(void * pio);
extern   int   tcp_bsdsendstat(void * pio);
extern   int   tcp_bsdrcvstat(void * pio);
#endif   /* INCLUDE_TCP */

#ifdef IP_ROUTING
int   show_routes    (void * pio);
int   mn_add_route   (void * pio);
extern RTMIB btreeRoot;
#endif   /* IP_ROUTING */

#if defined (IP_MULTICAST) && (defined (IGMP_V1) || defined (IGMP_V2))
extern   int igmp_print_stats (void * pio);
#endif /* IP_MULTICAST and (IGMPv1 or IGMPv2) */

extern   int   vfs_dir(void * pio);
extern   int   tk_stats(void * pio);

#ifdef UDPSTEST
extern   int   udpecho(void * pio);
extern   int   udp_secho_init(void * pio);
extern   int   udp_cecho_close(void * pio);
extern   int   udp_secho_close(void * pio);
extern   int   udp_echo_stats(void * pio);
extern   int   udp_sendecho(void * pio, ip_addr fhost, int len, long times);
#endif

#ifdef TCP_ECHOTEST
extern   int   tcpecho(void * pio);
extern   int   tcp_secho_init (void * pio);
extern   int   tcp_secho_close(void * pio);
extern   int   tcp_cecho_close(void * pio);
extern   int   tcp_echo_stats (void * pio);
extern   int   tcp_sendecho(void * pio, ip_addr fhost, long len, long times);
#endif

#ifdef TCP_CIPHERTEST
extern   int   tcp_scipher_init (void * pio);
extern   int   tcp_scipher_close(void * pio);
extern   int   tcp_cipher_stats (void * pio);
#endif

#ifdef DNS_CLIENT
int gethostbynametest(void * pio);
int nslookup_reverse(void * pio);
extern int setdnssvr(void * pio);
extern int dnc_database(void * pio);
#ifdef IP_V6
int gethost2test(void * pio);
#endif   /* IP_V6 */
#endif   /* DNS_CLIENT */

#ifdef DNS_CLIENT_UPDT
extern   int   nsupdatetest (void * pio);
extern   int   dns_update(char * soa, char * dname, ip_addr ipaddr, u_long ttl,
                           void * pio);
extern u_long dns_update_server;
static int setupdns(void * pio);

#endif /* DNS_CLIENT_UPDT */

#ifdef DNS_CLIENT_TEST_UPDT
extern int nsddnstest(void * pio);
#endif /* DNS_CLIENT_TEST_UPDT */


#ifdef DHCP_CLIENT
int   dhc_stats      (void * pio);
#endif


#ifdef NS_DDNS
void dns_dumpcache(void * pio);
#endif

int send_grat_arp(void * pio);

#ifdef  LOSSY_IO
int   setlossy(void * pio);
extern unsigned long myrandNum;
extern unsigned long myrandDenom;
#endif

#ifdef   MINI_PING
int   mini_ping(void * pio);
#endif   /* MINI_PING */

int   menu_exit (void * pio);
int   do_dtrap (void * pio);
int   defaulttype(void * pio);
int   dump_bytes(void * pio);
int   set_ipaddr(void * pio);

char * qname(PACKET pkt);  /* get name of queue packet is in */

extern   ip_addr  activehost;    /* common host parameter */
extern   u_long   pingdelay;     /* common delay parameter (1 second) */
extern   int      deflength;     /* common length parameter */

extern   int   settrapsize(void * pio);

#ifdef USE_PROFILER
extern   struct menu_op profmenu[5];
#endif

#ifdef NATRT
extern   void  nat_re_init(void);   /* refresh NAT router's parameters */
#endif

#ifdef ROUTE_TEST
extern   struct menu_op rt_menu[10];
#endif   /* ROUTE_TEST */

#ifdef CHANGE_PWD
int   change_pwd(void * pio);
#endif
int   list_users(void * pio);
int   menu_add_user(void * pio);
unsigned atoh(char * buf);
int set_cdelay(void * pio);
int set_chost(void * pio);
int set_clength(void * pio);

#ifdef PING_APP
int ping_set_parm (unsigned long int parm_type, unsigned long int value);
#endif

/*
 * Altera Niche Stack Nios port modification:
 * Add braces to remove build warning
 */
/* array of menu options, see menu.h */
struct menu_op mainmenu[]  = 
{
   { "general",  stooges,          "main menu" } ,      /* menu ID */
   { "help",     help,             "help with menus" } ,
   { "state",    station_state,    "show current station setup" } ,
   { "sendarp",  send_grat_arp,    "send a gratuitous ARP" },
#ifdef UDPSTEST
   { "uesend",   udpecho, "open UDP echo client, send UDP echo packet" } ,
   { "uesinit",  udp_secho_init,   "start UDP echo server" } ,
   { "uechalt",  udp_cecho_close,  "close UDP echo client" } ,
   { "ueshalt",  udp_secho_close,  "close UDP echo server" } ,
   { "uestats",  udp_echo_stats,   "UDP echo statistics" } ,
#endif
#ifdef TCP_ECHOTEST
   { "tesend",   tcpecho, "open TCP echo client, send TCP echo packet" } ,
   { "tesinit",  tcp_secho_init,   "start TCP echo server" } ,
   { "teshalt",  tcp_secho_close,  "close TCP echo server" } ,
   { "techalt",  tcp_cecho_close,  "close TCP echo client" } ,
   { "testats",  tcp_echo_stats,   "TCP echo statistics" } ,
#endif
#ifdef TCP_CIPHERTEST
   { "tesCipherInit",  tcp_scipher_init,   "start Crypto Engine server" } ,
   { "tesServerCipherhalt",  tcp_scipher_close,  "close Crypto Engine server" } ,
   { "teCipherstats",  tcp_cipher_stats,   "Crypto Engine statistics" } ,
#endif

#ifdef PING_APP
   /* delay, host, and length parameters (used by PING_APP) */
   { "p4delay",  ping_setdelay,    "set milliseconds to wait between pings" },
   { "p4host",   ping_sethost,     "set default active IP host" },
   { "p4length", ping_setlength,   "set default ping packet length" },
#endif

   /* common delay, host, and length parameters (used by various applications) */
   { "delay",    set_cdelay,       "set common delay parameter" },
   { "host",     set_chost,        "set common host parameter" },
   { "length",   set_clength,      "set common length parameter" },

   { "quit",     menu_exit,        "quit station program" } ,
#ifdef   MINI_PING
   { "ping",     mini_ping,        "send a ping" } ,
#endif   /* MINI_PING */
#ifdef INCLUDE_NVPARMS
   { "nvset",    set_nv_params,    "set non-volatile parameters" } ,
   { "nvedit",   edit_nv_params,   "edit non-volatile parameters" } ,
#endif
#ifdef DNS_CLIENT_UPDT  
   { "setupdns",    setupdns,       "set server for dynamic update" } ,
#endif /* DNS_CLIENT_UPDT */  
   { "setip",    set_ipaddr,       "set interface IP address" } ,
   { "version",  showver,          "display version information" } ,
   { "!command", NULL,             "pass command to OS shell" } ,
   { NULL } ,
};

/*
 * Altera Niche Stack Nios port modification:
 * Add braces to remove build warning
 */
/* array of menu options, see menu.h */
struct menu_op diagmenu[]  =
{
   { "diagnostic",stooges,      "diagnostic aid menu" } ,
#if defined(NET_STATS) && defined(INCLUDE_ARP)
   { "arps",     arp_stats,     "display ARP stats and table" } ,
#endif   /* INCLUDE_ARP && NET_STATS */
   { "buffers",  dumpbufs,      "display free q buffer stats" } ,
   { "bufstat",  dump_buf_estats, "display buffer error statistics" } ,
#ifdef HEAPBUFS
   { "hbufstat", dump_hbuf_stats, "display heap buffer diagnostics" } ,
#endif
   { "queues",   dumpqueues,    "dump packet buffer queues" } ,
   { "dbytes",   dump_bytes,    "dump block of memory" } ,
   { "debug",    setdebug,      "set IP stack debug tracing" } ,
   { "dtrap",    do_dtrap,      "try to hook debugger" } ,
#ifdef INCLUDE_INICHE_LOG
   { "glog",     glog_app,     "glog [options]" } ,
#endif
#ifdef VFS_FILES
   { "dir",      vfs_dir,       "directory of VFS files" } ,
#endif   /* VFS_FILES */
#ifdef  LOSSY_IO
   { "lossy",    setlossy,      "set lossy packet IO options" } ,
#endif
#if defined(NET_STATS) || defined(IP_V6)
   { "iface",    iface_stats,   "display net interface stats" } ,
#endif
   { "linkstats",linkstats,     "display link layer specific stats" } ,
#ifdef HEAP_STATS    /* statistics keeping npalloc() and npfree() */
   { "memory",   mh_stats,      "list currently alloced memory" } ,
#endif   /* HEAP_STATS */

#ifdef DNS_CLIENT
   { "nsdatabase", dnc_database,   "dump DNS client database" } ,
   { "nslookup", gethostbynametest,"do DNS gethostbyname()" } ,
   { "nslookupr", nslookup_reverse, "do reverse DNS" } ,
   { "setdnssvr", setdnssvr,       "set up DNS Servers" } ,
#ifdef IP_V6
   { "ns2lookup",   gethost2test,  "do DNS gethostbyname2( AF_INET6)" } ,
#endif   /* IP_V6 */

#ifdef  NS_DDNS 
   { "dnsdump", dns_dumpcache,  "dump DNS cache" } ,
#endif  /* NS_DDNS */

#ifdef DNS_CLIENT_TEST_UPDT
   { "nsddnstest", nsddnstest,  "do Dynamic DNS test" } ,
#endif  /* DNS_CLIENT_TEST_UPDT */ 

#ifdef  DNS_CLIENT_UPDT 
   { "nsupdate", nsupdatetest,  "do Dynamic DNS update" } ,
#endif  /* DNS_CLIENT_UPDT */
#ifdef NET_STATS
   { "dnsstats", dns_state,     "DNS setup & statistics" } ,
#endif
#endif   /* DNS_CLIENT */

#ifdef INCLUDE_TCP
#ifdef NET_STATS
#ifndef MINI_TCP
   { "mbuf",     mbuf_stat,      "display mbuf stats" } ,
   { "mlist",    mbuf_list,      "display mbufs in use" } ,
#endif   /* MINI_TCP */
   { "tcp",      tcp_stat,       "display TCP stats" } ,
   { "sockets",  sock_list,      "display socket list" } ,
   { "tbconn",   tcp_bsdconnstat,"tcp BSD connection stats" } ,
   { "tbsend",   tcp_bsdsendstat,"tcp BSD send stats" } ,
   { "tbrcv",    tcp_bsdrcvstat, "tcp BSD receive stats" } ,
#endif   /* NET_STATS */
#endif   /* INCLUDE_TCP */

#ifdef IP_ROUTING
   { "routes",   show_routes,    "display IP route table" } ,
   { "rtadd",    mn_add_route,   "manually add IP route to table" } ,
#endif   /* IP_ROUTING */

#if defined (IP_MULTICAST) && (defined (IGMP_V1) || defined (IGMP_V2))
   { "igmp",     igmp_print_stats, "Print IGMPv1/v2 statistics" } ,
#endif /* IP_MULTICAST and (IGMPv1 or IGMPv2) */

   { "allocsize",settrapsize,    "set size for alloc() breakpoint" } ,
#ifdef NET_STATS
   { "ipstat",   ip_stats,       "display IP layer stats" } ,
   { "iprstat",  ipr_stats,      "display IP reassembly stats" } ,
   { "icmpstat", icmp_stats,     "display ICMP layer stats" } ,
   { "udp",      udp_stats,      "display UDP layer stats" } ,
#endif
   { "upcall",   set_upc,        "trace received packets" } ,
#if defined(INICHE_TASKS) || defined(CHRONOS)
   { "tkstats",  tk_stats,       "tasking system status" } ,
#endif
#ifdef DHCP_CLIENT
#ifdef NET_STATS
   { "dcstats",  dhc_stats,      "DHCP Client statistics" } ,
#endif
#endif
#ifdef IP_PMTU
#ifdef NET_STATS
   { "pmtu",     pmtu_stats,     "Path MTU cache statistics" } ,
#endif
#endif
#ifdef CHANGE_PWD
   { "changepwd",change_pwd,    "change user password" } ,
#endif
#ifdef NET_STATS
   { "users",    list_users,    "list all users" } ,
   { "adduser",  menu_add_user, "add a new user" } ,
#endif
   { NULL } ,
};

struct menu_op *  menus[MAX_MENUS] =   /* array of ptrs to menu groups */
{
   &mainmenu[0],  /* general menu should come 1st */
   &diagmenu[0],  /* Diagnostic menu              */
#ifdef ROUTE_TEST
   &rt_menu[0],
#endif   /* ROUTE_TEST */
#ifdef USE_PROFILER
   &profmenu[0],
#endif
   /* add new menus here */
   NULL,          /* spare slot(s) for dymanic menu additions */
   NULL,          /* Eg., RIP , NATRT, TELNET, PING install dynamic menus */
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL           /* NULL slot at end - end of menus marker */
};

int nummenus = (sizeof(menus)/sizeof(void*)); /* number of menus */

/* module version array */
char *versions[MAX_MENUS] =
{
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL
};

NET      if_netbytext(void * pio, char * iftext);
int      if_configbyname(NET);   /* configure a net based on it's name */


/* FUNCTION: do_dtrap()
 * 
 * PARAM1: void * pio
 *
 * RETURNS: 
 */

int do_dtrap(void * pio)
{
   USE_VOID(pio);
   dtrap();
   return 0;
}


#ifdef  LOSSY_IO
extern int  in_lossrate;

int
setlossy(void * pio)
{
   char *   cp =  nextarg( ((GEN_IO)pio)->inbuf );

   if(*cp == 'r')
   {
      NDEBUG |= LOSSY_RX;
   }
   else if(*cp == 't')
   {
      NDEBUG |=  LOSSY_TX;
   }
   else if(*cp == 'o')
   {
      NDEBUG &= ~(LOSSY_TX|LOSSY_RX);
   }
   else if(isdigit(*cp))
   {
	   int i,j;

      i = atoi(cp);

	  if (i <= 0)
	  {
         ns_printf(pio, "usage:  denominator[%d] <= 0.\n", i);
         ns_printf(pio, "usage:  ['rx'|'tx'|'off'|numerator denominator ]\nusage:   where numerator/denominator will be the loss rate.\n");
	  }
	  else
	  {
           cp =  nextarg(cp);

		   if(isdigit(*cp))
		   {
              j = atoi(cp);
	          if (j <= 0)
			  {
                 ns_printf(pio, "usage:  *** denominator[%d] <= 0.\n", j);
                 ns_printf(pio, "usage:  ['rx'|'tx'|'off'|numerator denominator ]\nusage:   where numerator/denominator will be the loss rate.\n");
			  }
			  else
	          if (i >= j)
			  {
                 ns_printf(pio, "usage:  *** numerator[%d] >= denominator[%d].\n", i,j);
                 ns_printf(pio, "usage:  ['rx'|'tx'|'off'|numerator denominator ]\nusage:   where numerator/denominator will be the loss rate.\n");
			  }
			  else
			  {
			     myrandNum = i;
			     myrandDenom = j;
			  }
		   }
		   else
                 ns_printf(pio, "usage:  ['rx'|'tx'|'off'|numerator denominator ]\nusage:   where numerator/denominator will be the loss rate.\n");
	  }
   }
   else
   {
      ns_printf(pio, "usage:  ['rx'|'tx'|'off'|numerator denominator ]\nusage:   where numerator/denominator will be the loss rate.\n");
   }

   if((NDEBUG & (LOSSY_RX|LOSSY_TX)) == 0)
      ns_printf(pio, "lossy data is disabled\n");
   else
   {
      ns_printf(pio, "pkt loss rate currently %d in %d ", myrandNum, myrandDenom);
      if(NDEBUG & LOSSY_RX)
         ns_printf(pio, "RX ");
      if(NDEBUG & LOSSY_TX)
         ns_printf(pio, "TX");
      ns_printf(pio, "\n");
   }
   
   return 0;
}
#endif   /* LOSSY_IO */

extern   queue    bigfreeq;   /* big free buffers */
extern   queue    lilfreeq;   /* small free buffers */
extern   unsigned    lilbufs;
extern   unsigned    bigbufs;



/* FUNCTION: dumpqueues()
 * 
 * PARAM1: void * pio
 *
 * RETURNS: 
 */

int
dumpqueues(void * pio)
{
   ns_printf(pio,"bigfreeq: head:%p, tail:%p, len:%d, min:%d, max:%d\n",
    (void far *)bigfreeq.q_head, (void far *)bigfreeq.q_tail, 
    bigfreeq.q_len, bigfreeq.q_min, bigfreeq.q_max);

   ns_printf(pio,"lilfreeq: head:%p, tail:%p, len:%d, min:%d, max:%d\n",
    (void far *)lilfreeq.q_head, (void far *)lilfreeq.q_tail, 
    lilfreeq.q_len, lilfreeq.q_min, lilfreeq.q_max);

   ns_printf(pio,"rcvdq: head:%p, tail:%p, len:%d, min:%d, max:%d\n",
    (void far *)rcvdq.q_head, (void far *)rcvdq.q_tail, 
    rcvdq.q_len, rcvdq.q_min, rcvdq.q_max);

   return 0;
}



#ifdef NPDEBUG
extern   PACKET   pktlog[MAXPACKETS];
#endif

/* FUNCTION: dumpbufs()
 *
 * dumpbufs() - dump packet free queues
 * 
 * PARAM1: void * pio
 *
 * RETURNS: 
 */

int
dumpbufs(void * pio)
{
#ifdef NPDEBUG
   int   i;
   PACKET pkt;
   int   lines =  1; /* output line count for paging */
   char *   cp;
   int   offset   =  0;

   /* optional parameter is how far into the each buffer to start display */
   cp = nextarg(((GEN_IO)pio)->inbuf);
   if (cp && *cp)
   {
      offset = atoi(cp);
   }

   ns_printf(pio,"PACKET    len  buffer    que data offset %d\n",offset);
   for (i = 0; i < (int)(lilbufs+bigbufs); i++ )
   {
      pkt = pktlog[i];
      ns_printf(pio,"%p,%4d,%p,%s:",
       pkt, pkt->nb_blen, pkt->nb_buff, qname(pkt));
      hexdump(pio,pkt->nb_buff + offset, 12);
      lines++;
      if ( con_page(pio,lines) )
         return 0 ;
   }

#else
   ns_printf(pio,"Not debug build\n");
#endif
   return 0;
}

#ifdef INCLUDE_TCP
#ifdef NET_STATS
char * socket_queue_name(PACKET pkt);
#endif   /* NET_STATS */
#endif   /* INCLUDE_TCP */


/* FUNCTION: qname()
 *
 * return 3 char name of queue PACKET is in
 * 
 * PARAM1: PACKET pkt
 *
 * RETURNS: 
 */

char * qname(PACKET pkt)
{
   qp tmp;
#ifdef INCLUDE_TCP
#ifdef NET_STATS
   char *   so_queue_name;
#endif /* NET_STATS   */
#endif /* INCLUDE_TCP */

   /* search for packet in the known queues: */
   for (tmp = lilfreeq.q_head; tmp; tmp = tmp->qe_next)
      if (tmp == (qp)pkt)
      return "lil";
   for (tmp = bigfreeq.q_head; tmp; tmp = tmp->qe_next)
      if (tmp == (qp)pkt)
      return "big";
   for (tmp = rcvdq.q_head; tmp; tmp = tmp->qe_next)
      if (tmp == (qp)pkt)
      return "rcv";

#ifdef INCLUDE_TCP
#ifdef NET_STATS
   /* check to see if the packet is in one of the socket queues */
   so_queue_name = socket_queue_name(pkt);
   if (so_queue_name)
      return so_queue_name;
#endif /* NET_STATS */
#endif /* INCLUDE_TCP */

   return "non";
}



/* FUNCTION: showver()
 * 
 * PARAM1: void * pio
 *
 * RETURNS: 
 */

int
showver(void * pio)
{
   int i;
   extern   char *name;

   /* NicheStack version */
   ns_printf(pio,"%s\n", name);

   /* module versions */
   for (i = 0; i < MAX_MENUS; i++)
   {
      if (versions[i] != (char *)NULL)
         ns_printf(pio, "%s,", versions[i]);
   }
   ns_printf(pio, "\n");

   return 0;
}


extern   char *   prompt;
char *   oldprompt   = "OOPS> ";

/* if we have to deal with Intel's 16 bit segmented architecture */
#ifdef SEG16_16

/* FUNCTION: dump_bytes()
 *
 * segment:offset version of dump_bytes()
 * 
 * PARAM1: void * pio
 *
 * RETURNS: 
 */

int
dump_bytes(void * pio)
{
   char *   cp;
   unshort  seg,  offset;
   int   length;

   cp = nextarg(((GEN_IO)pio)->inbuf);    /* see if user put addr on cmd line */
   if (!*cp)
      goto db_prompt;
   seg = (unshort)atoh(cp);
cp = strchr(cp, ':');   /* find colon in string */
   if (!cp)
      goto db_prompt;
   cp++;
   offset = (unshort)atoh(cp);
   cp = nextarg(cp);    /* see if user specified length to dump */
   if (*cp)    /* cp should point to end of args or length */
      length = (int)atoh(cp);
   else     /* no length on command line, set a reasonable default */
      length = 16;

   hexdump(pio,_MK_FP(seg, offset), length);
   return 0;

db_prompt:     /* prompt user for correct format */
   ns_printf(pio,"enter memory location in hex seg:offset form, followed by optional length.\n");
   return -1;
}

#else    /* SEG16_16 */

/* FUNCTION: dump_bytes()
 *
 * flat memory version of dump_bytes()
 * 
 * PARAM1: void * pio
 *
 * RETURNS: 
 */

int
dump_bytes(void * pio)
{
   char *   cp;
   unsigned int address;
   int   length;

   cp = nextarg(((GEN_IO)pio)->inbuf);    /* see if user put addr on cmd line */
   if (!*cp)
      goto db_prompt;
   address = atoh(cp);
   cp = nextarg(cp);    /* see if user specified length to dump */
   if (*cp)    /* cp should point to end of args or length */
      length = (int)atoh(cp);
   else     /* no length on command line, set a reasonable default */
      length = 16;

   hexdump(pio,(void *) address, length);
   return 0;

db_prompt:     /* prompt user for correct format */
   ns_printf(pio,"enter memory location in hex, followed by optional length.\n");
   return -1;
}

#endif      /* SEG16_16 */


/* FUNCTION: menu_exit()
 * 
 * PARAM1: void * pio
 *
 * RETURNS: 
 */

int
menu_exit(void * pio)
{
   USE_VOID(pio);
   netexit(0);    /* shut down hardware & exit to system */
   return 0;   /* keeps compiler from whining */
}


#ifdef UDPSTEST

/* FUNCTION: udpecho()
 * 
 * PARAM1: void * pio
 *
 * RETURNS: 
 */

int udpecho(void * pio)
{
   long  times =  1;
   char *   arg2;

   arg2 = nextarg(((GEN_IO)pio)->inbuf);  /* get iteration arg from cmd line */
   if (arg2 && *arg2)
      times = atol(arg2);

   if (times < 1)
   {
      ns_printf(pio,"command line arg must be number of echos to send\n");
      return -1;
   }
   if (activehost == 0L)
   {
      ns_printf(pio,"IP host to echo to must be set with \"host\" command\n");
      return -1;
   }

   /* send echo - this prints it's own errors */
   udp_sendecho(pio,activehost, deflength, times);

   return 0;
}
#endif   /* UDPSTEST */

#ifdef TCP_ECHOTEST

/* FUNCTION: tcpecho()
 * 
 * PARAM1: void * pio
 *
 * RETURNS: 
 */

int tcpecho(void * pio)
{
   long  times =  1;
   char *   arg2;

   arg2 = nextarg(((GEN_IO)pio)->inbuf);  /* get iteration arg from cmd line */
   if (arg2 && *arg2)
      times = atol(arg2);

   if (times < 1)
   {
      ns_printf(pio,"command line arg must be number of echos to send\n");
      return -1;
   }
   if (activehost == 0L)
   {
      ns_printf(pio,"IP host to echo to must be set with \"host\" command\n");
      return -1;
   }

   /* send echo - this prints it's own errors */
   tcp_sendecho(pio,activehost, (long)deflength, times);

   return 0;
}
#endif   /* TCP_ECHOTEST */

#ifdef IP_ROUTING

char *   route_prots[] =
{
   "foo",
   "OTHER",
   "LOCAL",
   "NETMGT",
   "ICMP",
   "EGP",
   "GGP",
   "HELLO",
   "RIP",
};


#ifdef BTREE_ROUTES

/* FUNCTION: btree_routes()
 * 
 * Helper function for displaying routes in the binary tree. A pointer to
 * this is passed to avldepthfirst();
 *
 * PARAM1: current RtMib
 * PARAM2: pointer to pio
 * PARAM3: depth in tree
 *
 * RETURNS: 
 */

static int brtcount;

void
btree_routes(struct avl_node * node, long param, int depth)
{
   struct RtMib * rtp = (struct RtMib *)node;
   void * pio = (void*)param;

   if (!rtp->ipRouteNextHop)  /* empty slot */
      return;

   brtcount++;

   /* if this is first entry then display heading */
   if(brtcount == 1)
      ns_printf(pio,"..IPaddr.......mask.........nexthop...iface..type\n");

   while(depth--)
      ns_printf(pio, " ");

   ns_printf(pio,"%u.%u.%u.%u  %u.%u.%u.%u  %u.%u.%u.%u  %d   %s\n",
      PUSH_IPADDR(rtp->ipRouteDest),
      PUSH_IPADDR(rtp->ipRouteMask),
      PUSH_IPADDR(rtp->ipRouteNextHop),
      (int)rtp->ipRouteIfIndex,
      route_prots[(int)rtp->ipRouteProto]);
}
#endif   /* BTREE_ROUTES */


/* FUNCTION: show_routes()
 * 
 * PARAM1: void * pio
 *
 * RETURNS: 
 */

int 
show_routes(void * pio)
{
   int   rtcount  =  0;

#ifdef BTREE_ROUTES
   brtcount = 0;
   avldepthfirst((struct avl_node *)btreeRoot, btree_routes, (long)pio, 0);
   rtcount = brtcount;
#else /* not BTREE_ROUTES */
   RTMIB rtp;

   if(!rt_mib)    /* system not fully up yet */
      return -1;

   for (rtp = rt_mib; rtp < rt_mib + ipRoutes; rtp++)
   {
      if (!rtp->ipRouteNextHop)  /* empty slot */
         continue;

      rtcount++;
      if (rtcount == 1)
         ns_printf(pio,"..IPaddr.......mask.........nexthop...iface..type\n");
      ns_printf(pio,"%u.%u.%u.%u  %u.%u.%u.%u  %u.%u.%u.%u  %d   %s\n",
       PUSH_IPADDR(rtp->ipRouteDest),
       PUSH_IPADDR(rtp->ipRouteMask),
       PUSH_IPADDR(rtp->ipRouteNextHop),
       (int)rtp->ipRouteIfIndex,
       route_prots[(int)rtp->ipRouteProto]);
   }
#endif   /* BTREE_ROUTES */

   if (rtcount == 0)
      ns_printf(pio,"no IP routes set\n");

   return 0;
}


/* FUNCTION: mn_add_route()
 *
 * menu routine to manually add a route. format is target.ip 
 * target.mask next.hop iface. 
 *
 * 
 * PARAM1: void * pio
 *
 * RETURNS: 
 */

int mn_add_route(void * pio)
{
   char *   cp;      /* text for interface index */
   int      ifindex; /* value of interface index */
   unsigned snbits;
   int      i;
   char *   errmsg;
   RTMIB    rtp;
   NET      ifp;
   struct ip_args
   {  /* for reading in IP addresses from console buf */
      char *   text;
      ip_addr  ipaddr;
   }  ipargs[4];

   cp = nextarg(((GEN_IO)pio)->inbuf);    /* start of command line args */
   for (i = 0; i <= 3; i++)   /* read in 4 args */
   {
      ipargs[i].text = cp;

      if (i == 3) /* last arg is not an IP address, so we're done */
         break;

      /* null terminate IP address arg */
      cp = strchr(ipargs[i].text, ' ');
      if (cp)
      {
         *cp++ = '\0';  /* terminate this arg */
         while (*cp == ' ')cp++; /* find next arg */
            }
      else  /* no space after arg == bogus command line */
      {
         ns_printf(pio,"usage: target.ip target.mask next.hop iface \n");
         ns_printf(pio," where 1st 3 parms are in IP dot notation, last is digit 1-%d\n",
          ifNumber);
         return -1;
      }
      /* call parse_ipad to fill in IP address from text */
      errmsg = parse_ipad(&ipargs[i].ipaddr, &snbits, ipargs[i].text);
      if (errmsg)
      {
         ns_printf(pio,"bad IP format \"%s\" in arg %d, \n problem: %s\n", 
          ipargs[i].text, i, errmsg);
         return -1;
      }
   }

   ifp = if_netbytext(pio, ipargs[i].text);
   if (ifp == NULL)
      return -1;
   else
      ifindex = if_netnumber(ifp);
   
   rtp = add_route(ipargs[0].ipaddr, ipargs[1].ipaddr, ipargs[2].ipaddr,
      ifindex, IPRP_LOCAL);

   if (!rtp)
   {
      ns_printf(pio,"add_route failed, table full?\n");
      return -1;
   }
   return 0;
}

#endif   /* IP_ROUTING */

#ifdef DNS_CLIENT_UPDT  
/* FUNCTION: setupdns()
 *
 * Allows console user to set the IP address of the server to be used for 
 * dynamic update.
 *
 * PARAM1: void * pio
 *
 * RETURNS: 
 */

int
setupdns(void * pio)
{
   char *   cp;
   char *   iptext;
   unsigned int snbits;
   
   iptext = nextarg(((GEN_IO)pio)->inbuf);    /* see if user put addr on cmd line */
   if (!*iptext)
   {
      ns_printf(pio, "usage: setupdns X.X.X.X\n");
      return -1;
   }

   for (cp = iptext; *cp > ' '; cp++)  /* find end of IP text */
      ;

   *cp = 0;    /* null terminate iptext */ 
   cp = parse_ipad(&dns_update_server, &snbits, iptext);
   if (cp)
   {
      ns_printf(pio, "IP address error: %s\n", cp);
      return -1;
   }

   ns_printf(pio, "DNS dynamic update server is set to %s \n", iptext);
}
#endif /* DNS_CLIENT_UPDT */


/* FUNCTION: set_ipaddr()
 *
 * Allows console user to set an IP address of an interface on the fly. Usefull
 * for testing, but dangerous in field.
 *
 * PARAM1: void * pio
 *
 * RETURNS: 
 */

int
set_ipaddr(void * pio)
{
   char *   cp;
   char *   iptext;
   ip_addr  ipaddr;
   ip_addr  mask;
   unsigned int snbits;
   struct net * ifp;
   int      iface = 0;

   ns_printf(pio, "WARNING: 'setip' will kill all current net connections!!!!\n");
   cp = nextarg(((GEN_IO)pio)->inbuf);    /* see if user put addr on cmd line */
   if (!*cp)
   {
      ns_printf(pio, "usage: setip X.X.X.X [iface (name or 1-n)]\n");
      return -1;
   }
   iptext = cp;   /* save pointer to IP spec */

   cp = nextarg(cp); /* see if user specified interface number */
   if (*cp)
   {
      ifp = if_netbytext(pio, cp);
      if(ifp == NULL)
         return -1;
      iface = if_netnumber(ifp);
   }
   else  /* no iface number given, default to first static */
   {
      ifp = nets[0];
   }

   for (cp = iptext; *cp > ' '; cp++)  /* find end of IP text */
      ;

   *cp = 0;    /* null terminate iptext */ 
   cp = parse_ipad(&ipaddr, &snbits, iptext);
   if (cp)
   {
      ns_printf(pio, "IP address error: %s\n", cp);
      return -1;
   }

   ns_printf(pio, "replacing net %s IP address ", ifp->name);
   ns_printf(pio, "%u.%u.%u.%u", PUSH_IPADDR(ifp->n_ipaddr) );

   /* replace IP adress in nets[] structure */
   ifp->n_ipaddr = ipaddr;

   /* make default subnet mask */
   mask = (ip_addr) ~0;    /* all 1s */
   while (snbits--)
      mask = mask >> 1;

   ifp->snmask = htonl(~mask);

   ns_printf(pio, " with %u.%u.%u.%u in RAM variables\n", 
    PUSH_IPADDR(ifp->n_ipaddr) );

#ifdef INCLUDE_NVPARMS
   inet_nvparms.ifs[iface].ipaddr = ipaddr;
   inet_nvparms.ifs[iface].subnet = htonl(~mask);

   ns_printf(pio, "Use \"nvset\" to back up to flash or disk\n");
#else
   USE_ARG(iface);
#endif

#ifdef NATRT
   nat_re_init();    /* refresh NAT router's parameters */
#endif

   return 0;
}

#ifdef DNS_CLIENT

#ifdef IP_V6

/* FUNCTION: gethost2test()
 *
 * Resolve a host name to IP address via a call to gethostbyname2()
 *
 * PARAM1: void *pio
 *
 * RETURNS: 
 */

int
gethost2test(void * pio)
{
   char *   cp;
   struct hostent *  p;

   /* see if user put name on cmd line */
   cp = nextarg(((GEN_IO)pio)->inbuf);
   if (!cp || !*cp)
   {
      ns_printf(pio, "usage: ns2lookup host_name\n");
      return -1;
   }

   /* call gethostbyname() to resolve the passed name */
   p = gethostbyname2(cp, AF_INET6);

   if (!p)
   {
      ns_printf(pio,"gethostbyname2() returned NULL\n");
      return 0;
   }

   ns_printf(pio,"gethostbyname2() succeeded\n");
   ns_printf(pio," h_name is %s\n",p->h_name ? p->h_name : "NULL");

   ns_printf(pio," h_addrtype = %d\n",p->h_addrtype);
   ns_printf(pio," h_length = %d\n",p->h_length);
   ns_printf(pio," h_addr_list are\n");
   if (!(p->h_addr_list))
      ns_printf(pio,"\tno addresses\n");
   else
   {
      char ** cpp;
      for(cpp = p->h_addr_list; *cpp; cpp++)
      {
         char ip6buf[40];     /* tmp buffer for ipv6 address text */
         ns_printf(pio,"\t%s\n",  print_ip6((struct in6_addr *)(*cpp), ip6buf));
      }
   }

   return 0;
}
#endif   /* IP_V6 */

/* FUNCTION: gethostbynametest()
 *
 * Resolve a host name to IP address via a call to gethostbyname()
 *
 * PARAM1: void *pio
 *
 * RETURNS: 
 */

int
gethostbynametest(void * pio)
{
   char *   cp;
   struct hostent *  p;

   /* see if user put name on cmd line */
   cp = nextarg(((GEN_IO)pio)->inbuf);
   if (!cp || !*cp)
   {
      ns_printf(pio, "usage: nslookup host_name\n");
      return -1;
   }

   /* call gethostbyname() to resolve the passed name */
   p = gethostbyname(cp);

   if (!p)
   {
      ns_printf(pio,"gethostbyname() returned NULL\n");
      return 0;
   }

   ns_printf(pio,"gethostbyname() succeeded\n");
   ns_printf(pio,"h_name is %s\n",p->h_name ? p->h_name : "NULL");

   ns_printf(pio,"h_addrtype = %d\n",p->h_addrtype);
   ns_printf(pio,"h_length = %d\n",p->h_length);
   ns_printf(pio,"h_addr_list are\n");
   if (!(p->h_addr_list) || !(*p->h_addr_list))
      ns_printf(pio,"\tno addresses\n");
   else
   {
      char ** cpp;
      u_char * ucp;

      for(cpp = p->h_addr_list; *cpp; cpp++)
      {
         ucp = (unsigned char *) (*cpp);
         ns_printf(pio,"\t%d.%d.%d.%d\n",
          *ucp, *(ucp + 1), *(ucp + 2), *(ucp + 3));
      }
   }

   return 0;
}

int
nslookup_reverse(void * pio)
{
   int status = -1;
   char *cp = NULL;
   struct dns_querys * dns_entry = NULL;
   /* see if user put name on cmd line */
   cp = nextarg(((GEN_IO)pio)->inbuf);
   if(!cp || !*cp)
   {
      ns_printf(pio, "usage: nslookupr domainname\n");
   }
   else
   {
      if(nslookup(cp, DNS_TYPE_PTR, &dns_entry))
      {
         ns_printf(pio,"nslookup() failed\n");
      }
      else
      {
         if(dns_entry)
         {
            ns_printf(pio,"%s    name = %s\n",dns_entry->dns_names, dns_entry->ptr_name);
            status = 0;
         }
         else
         {
            ns_printf(pio,"nslookup() dns_entry is NULL\n");
         }
      }
   }
   return(status);
}

/* FUNCTION: setdnssvr()
 * 
 * Allows console user to set a DNS Server's IP address. Useful for testing 
 * the DNS Client.
 *
 * PARAM1: void * pio
 *
 * RETURNS: 0 on success, or an error code of -1.
 */

int setdnssvr(void * pio)
{
   char * cp;
   char * iptext;
   unsigned int subnet;     /* dummy for passing to parse_ipad() */
   int   svr_num;

   cp = nextarg(((GEN_IO)pio)->inbuf);    /* see if user put addr on cmd line */
   if (!*cp)
   {
      ns_printf(pio, "usage: setdnssvr X.X.X.X [Server No. (1-%d)]\n", MAXDNSSERVERS);
      return -1;
   }
   iptext = cp;   /* save pointer to IP spec */

   cp = nextarg(cp); /* see if user specified server number */
   if (*cp)
   {
      svr_num = atoi(cp);
      if (svr_num < 1 || svr_num > MAXDNSSERVERS)
      {
         ns_printf(pio, "DNS server number must be 1-%d\n", MAXDNSSERVERS);
         return -1;
      }
   }
   else  /* no server number given, default to first server */
   {
      svr_num = 1;
   }

   for (cp = iptext; *cp > ' '; cp++)  /* find end of IP text */
      ;

   *cp = 0;    /* null terminate iptext */ 

   cp = parse_ipad(&dns_servers[svr_num-1], &subnet, iptext);

   if (cp)
   {
      ns_printf(pio, "IP address error: %s\n", cp);
      return -1;
   }

#ifdef INCLUDE_NVPARMS
   MEMCPY(inet_nvparms.dns_servers, dns_servers, sizeof(dns_servers));
   ns_printf(pio, "Use \"nvset\" to back up to flash or disk\n");
#endif

   return 0;
}
#ifdef DNS_CLIENT_UPDT

/* FUNCTION: nsupdatetest()
 * 
 * Allows the user to add the ip address of a domain name using the
 * Dynamic DNS update protocol as specified in rfc 2136.
 *
 * PARAM1: void *pio
 *
 * RETURNS: 
 */

static int 
nsupdatetest(void * pio)
{
   char *   cp;
   char * ucp;
   char dnbuf[1025];         /* buffer for domain name */
   unsigned snbits;          /* output of parse_ipaddr- subnet value */
   ip_addr ipout;            /* output of parse_ipaddr- ip address */
   unsigned long r_ttl;
   int p;                   /* Return code from sending update packet */
   unsigned short del_flag = 0;

   /* see if user put ADD on cmd line */
   cp = nextarg(((GEN_IO)pio)->inbuf);
 
   if (!strncmp(cp, "add",3) && (!strncmp(cp, "delete",6)))
   {
      ns_printf(pio,"usage: nsupdate add <domain name> <ttl> A <ip addr>\n");
      ns_printf(pio,"usage: nsupdate delete <domain name>\n");
      return -1;
   }
   if (strncmp(cp, "delete",6) == 0)
      del_flag = 1;

   /* get next parameter.  This must be the domain name.*/
   cp = nextarg(cp);
   if (!*cp)
   {
      ns_printf(pio, "usage: nsupdate add <domain name> <ttl> A <ip addr>\n");
      ns_printf(pio,"usage: nsupdate delete <domain name>\n");
      return -1;
   }

   strcpy(dnbuf,cp);
   ucp = &dnbuf[0];
   while (*ucp != ' ') ucp++;
   *ucp = '\0';

   /* If this is a delete operation, call dns_update with zero values for */
   /* ttl and ip_addr                                                     */
   if (del_flag == 1)
   {
      r_ttl = 0; /* ttl is 0 */
      ipout = 0; /* ip address is 0 */

      /* Final parameter is an ip address */
      cp = nextarg(cp);
      if (parse_ipad(&ipout, &snbits, cp))
	  {
         ns_printf(pio, "usage: nsupdate add <domain name> <ttl> A <ip addr>\n");
         ns_printf(pio,"usage: nsupdate delete <domain name>\n");
         return -1;
	  }

      p = dns_update(soa_mname, dnbuf, ipout, r_ttl, pio);
      if (p >= 0)
         ns_printf(pio, "Update function returned the following response: %d\n",p);
      else
         ns_printf(pio, "Authoritative name server not found\n");
      return 0;
   }
      
   /* Next parameter must be ttl */
   cp = nextarg(cp);
   if (!(isdigit(*cp)))
   {
      ns_printf(pio, "usage: nsupdate add <domain name> <ttl> A <ip addr>\n");
      ns_printf(pio,"usage: nsupdate delete <domain name>\n");
      return -1;
   }
   else
      r_ttl = atol(cp);

   /* Next parameter must be class- in this case an A */
   cp = nextarg(cp);
   if (*cp != 'A')
   {
      ns_printf(pio, "usage: nsupdate add <domain name> <ttl> A <ip addr>\n");
      ns_printf(pio,"usage: nsupdate delete <domain name>\n");
      return -1;
   }

   /* Final parameter is an ip address */
   cp = nextarg(cp);
   if (parse_ipad(&ipout, &snbits, cp))
   {
      ns_printf(pio, "usage: nsupdate add <domain name> <ttl> A <ip addr>\n");
      ns_printf(pio,"usage: nsupdate delete <domain name>\n");
      return -1;
   }
   p = dns_update(soa_mname, dnbuf, ipout, r_ttl, pio);
   if (p >= 0)
   {
      ns_printf(pio, "Update function returned the following error: %d\n",p);
   }
   else
      ns_printf(pio, "Authoritative name server not found\n");
   return 0;
}
#endif   /* DNS_CLIENT_UPDT */
#endif   /* DNS_CLIENT */

#ifdef PING_APP
/* FUNCTION: ping_setdelay()
 *
 * Set the default delay between successive ping requests.
 * This value affects all sessions.
 *
 * PARAM1: GEN_IO pio - device for console output
 *
 * RETURNS: -1 if 'delay' parameter is missing from command 
 * line or if the string->numeric conversion failed; otherwise,
 * return code from ping_set_parm ().
 */

int
ping_setdelay(void * pio)
{
   u_long newdelay;
   char * arg = nextarg(((GEN_IO)pio)->inbuf);
   char * termp;

   if (arg && (*arg))
   {
      newdelay = strtoul (arg, &termp, 10);
      if ((*termp == '\0') && ((*arg) != '-'))
      {
         ns_printf(pio,"Set inter-ping delay to %lu ms.\n", newdelay);
         /* delay parameter is specified in number of ticks */
         return (ping_set_parm (PARM_TYPE_DELAY, (newdelay/TIMEFOR1TICK)));
      }
      else
      {
         return -1;
      }
   }

   return -1;
}

/* FUNCTION: ping_setlength()
 *
 * Set the default value of length of ping packets.
 * This value affects all sessions.
 *
 * PARAM1: GEN_IO pio - device for console output
 *
 * RETURNS: -1 if 'length' parameter is missing from command 
 * line or if the string->numeric conversion failed; otherwise,
 * return code from ping_set_parm ().
 */

int
ping_setlength(void * pio)    /* menu routine to set default ping size */
{
   u_long newlen;
   char * arg = nextarg(((GEN_IO)pio)->inbuf);
   char * termp;

   if (arg && (*arg))
   {
      newlen = strtoul (arg, &termp, 10);
      if ((*termp == '\0') && ((*arg) != '-'))
      {
         ns_printf(pio,"Set ping length to %lu bytes.\n", newlen); 
         return (ping_set_parm (PARM_TYPE_LENGTH, newlen));
      }
      else
      {
         return -1;
      }
   }

   return -1;
}

/* FUNCTION: ping_sethost()
 * 
 * Set the default host to be pinged.
 * This value affects all sessions.
 *
 * PARAM1: GEN_IO pio - device for console output
 *
 * RETURNS: Return code from in_reshost () (if it returns an
 * error), or the return code from ping_set_parm () (if 
 * in_reshost () succeeded), or -1 if 'host' parameter is missing 
 * from command line.
 */

int
ping_sethost(void * pio)   /* set default host for pings, et.al. */
{
   int e;
   char * arg =  nextarg(((GEN_IO)pio)->inbuf);
   ip_addr newhost;

   if (arg && (*arg))
   {
      e = in_reshost(arg, &newhost, RH_VERBOSE | RH_BLOCK);
      if (!e)
      {
         printf ("Set ping host to %u.%u.%u.%u.\n", PUSH_IPADDR (newhost));
         return (ping_set_parm (PARM_TYPE_HOST, ((u_long) newhost)));
      }
      else
      {
         return e;
      }
   }

   return -1; 
}
#endif /* PING_APP */

#ifdef   MINI_PING
int
mini_ping(void * pio)
{
   int err;

   if(!activehost)
   {
      ns_printf(pio,"set host first\n");
      return 1;
   }

   err = icmpEcho(activehost, NULL, deflength, 88);
   if(err == 0)
      ns_printf(pio,"ping sent, check icmp for reply\n");
   else
      ns_printf(pio,"ping send error %d\n", err);

   return err;   
}
#endif   /* MINI_PING */

/* FUNCTION: set_cdelay()
 *
 * Set the common default delay.
 *
 * PARAM1: GEN_IO pio - device for console output
 *
 * RETURNS: -1 if 'delay' parameter is missing from command 
 * line or if the string->numeric conversion failed; otherwise,
 * this function returns SUCCESS.
 */

int
set_cdelay(void * pio)
{
   u_long newdelay;
   char * arg = nextarg(((GEN_IO)pio)->inbuf);
   char * termp;

   if (arg && (*arg))
   {
      newdelay = strtoul (arg, &termp, 10);
      if ((*termp == '\0') && ((*arg) != '-'))
      {
         ns_printf(pio,"Set common delay parameter to %lu ms.\n", newdelay);
         pingdelay = newdelay/TIMEFOR1TICK;
         return SUCCESS;
      }
      else
      {
         return -1;
      }
   }

   return -1;
}

/* FUNCTION: set_clength()
 *
 * Set the common length parameter.
 *
 * PARAM1: GEN_IO pio - device for console output
 *
 * RETURNS: -1 if 'length' parameter is missing from command 
 * line or if the string->numeric conversion failed; otherwise,
 * this function returns SUCCESS.
 */

int
set_clength(void * pio)    /* menu routine to set default ping size */
{
   u_long newlen;
   char * arg = nextarg(((GEN_IO)pio)->inbuf);
   char * termp;

   if (arg && (*arg))
   {
      newlen = strtoul (arg, &termp, 10);
      if ((*termp == '\0') && ((*arg) != '-'))
      {
         ns_printf(pio,"Set common length parameter to %lu bytes.\n", newlen); 
         deflength = newlen;
         return SUCCESS;
      }
      else
      {
         return -1;
      }
   }

   return -1;
}

/* FUNCTION: set_chost()
 * 
 * Set the common host parameter.
 *
 * PARAM1: GEN_IO pio - device for console output
 *
 * RETURNS: -1 if 'host' parameter is missing from command 
 * line, or return code from in_reshost () (if it returns an
 * error), or SUCCESS (0).
 */

int
set_chost(void * pio)   /* set default host for pings, et.al. */
{
   int   e;
   char * arg =  nextarg(((GEN_IO)pio)->inbuf);
   ip_addr newhost;

   if (arg && (*arg))
   {
      e = in_reshost(arg, &newhost, RH_VERBOSE | RH_BLOCK);
      if (!e)
      {
         printf ("Set common host parameter to %u.%u.%u.%u.\n", PUSH_IPADDR (newhost));
         activehost = newhost;
         return SUCCESS;
      }
      else
      {
         return e;
      }
   }

   return -1;
}

#ifdef CHANGE_PWD /* allow user to change password */

/* FUNCTION: get1ch()
 * 
 * Get 1 char from input device of pio.
 *
 * PARAM1: void * vio - device for console input/output
 *
 * RETURNS: received char, else -1.
 */

#include "userpass.h"

int get1ch(void *vio)
{
   int ch=-1;
   GEN_IO pio = (GEN_IO)vio;  /* convert void* to our IO device type */

   if ( pio && pio->getch )   /*if i/p func is supplied*/
   {
      do 
      {
         ch = (pio->getch)(pio->id);
         if ( ch == 0 )
            tk_yield();    /* Give timeslice to other processes */
      } while ( ch == 0 ) ;
   }
   return ch;
}

/* FUNCTION: change_pwd()
 * 
 * Change the password of a user.
 *
 * PARAM1: void * vio - device for console input/output
 *
 * RETURNS: 0 on success, else -1.
 */

int
change_pwd(void * pio)
{
   char * user;
   char * cp =  nextarg(((GEN_IO)pio)->inbuf);
   char oldpwd[MAX_USERLENGTH];
   char newpwd[MAX_USERLENGTH];
   int i;
   int ch;

   if (!*cp)   /* no arg given */
   {
      ns_printf(pio,"Error - User name not specfied\n");
      ns_printf(pio,"Usage: changepwd <username>\n");
      return -1;
   }

   user = cp; /* point to start of user name */
   while ((*cp != ' ') && (*cp != '\n') && (*cp != '\r') && (*cp != '\t'))
      cp++;
   *cp = 0; /* null terminate user name */

   /* get the current password */
   ns_printf(pio,"Enter old password> ");
   i=0;
   do
   {
      ch = get1ch(pio);
      if ((ch != ' ') && (ch != '\n') && (ch != '\r') && (ch != '\t'))
      {
         oldpwd[i++] = ch;
         ns_printf(pio,"*");
      }
      else
      {
         oldpwd[i]=0; /* overwrite linefeed with null, to null-terminate */
         ns_printf(pio,"\n");
         break;
      }
   } while (ch != -1);

   ns_printf(pio,"Enter new password> ");
   i=0;
   do
   {
      ch = get1ch(pio);
      if ((ch != ' ') && (ch != '\n') && (ch != '\r') && (ch != '\t'))
      {
         newpwd[i++] = ch;
         ns_printf(pio,"*");
      }
      else
      {
         newpwd[i]=0; /* overwrite linefeed with null, to null-terminate */
         ns_printf(pio,"\n");
         break;
      }
   } while (ch != -1);

   if (add_user(user,oldpwd,newpwd) == TRUE)
      ns_printf(pio,"Password successfully changed.\n");
   else
   {
      ns_printf(pio,"Error - couldn't change password.\n");
      return -1;
   }

   return 0;
}
#endif /* CHANGE_PWD */

#endif   /* IN_MENUS */



/* FUNCTION: ishexdigit()
 *
 * some hex digit manipulation routines:
 * 
 * PARAM1: char digit
 *
 * RETURNS: 
 */

int   /* boolean */
ishexdigit(char digit)  /* return TRUE is char is hex digit */
{
   if (isdigit(digit))
      return TRUE;

   digit |= 0x20;       /* mask letters to lowercase */
   if ((digit >= 'a') && (digit <= 'f'))
      return TRUE;
   else
      return FALSE;
}



/* FUNCTION: hexnibble()
 * 
 * PARAM1: char digit
 *
 * RETURNS: 
 */

unsigned hexnibble(char digit)
{
   if (digit <= '9')
      return (digit-'0'    );

   digit &= ~0x20;   /* letter make uppercase */
   return (digit-'A')+10 ;
}



/* FUNCTION: atoh()
 *
 * atoh() - convert hex ascii string to unsigned. number may be preceeded
 * by whitspace or "0x". Halts at first non hex digit character.
 *
 * 
 * PARAM1: char * buf
 *
 * RETURNS: 
 */

unsigned
atoh(char * buf)
{
   unsigned retval = 0;
   char *   cp;
   char  digit;

   cp = buf;

   /* skip past spaces and tabs */
   while (*cp == ' ' || *cp == 9)
      cp++;

   /* while we see digits and the optional 'x' */
   while (ishexdigit(digit = *cp++) || (digit == 'x'))
   {
      /* its questionable what we should do with input like '1x234', 
       * or for that matter '1x2x3', what this does is ignore all 
       */
      if (digit == 'x')
         retval = 0;
      else
         retval = (retval << 4) + hexnibble(digit);
   }

   return retval;
}


/* FUNCTION: atohl()
 *
 * atohl() - same as atoh() above, except returns a long
 * 
 * PARAM1: char * buf
 *
 * RETURNS: 
 */

unsigned long
atohl(char * buf)
{
   unsigned long retval = 0;
   char *   cp;
   char  digit;

   cp = buf;

   /* skip past spaces and tabs */
   while (*cp == ' ' || *cp == 9)
      cp++;

   /* while we see digits and the optional 'x' */
   while (ishexdigit(digit = *cp++) || (digit == 'x'))
   {
      /* its questionable what we should do with input like '1x234', 
       * or for that matter '1x2x3', what this does is ignore all 
       */
      if (digit == 'x')
         retval = 0;
      else
         retval = (retval << 4) + hexnibble(digit);
   }

   return retval;
}


/* FUNCTION: hex2bin()
 *
 * hex2bin() - convert hexadecimal text into binary. Returns 0 if OK 
 * else error code in invalid hex char encountered in input string. 
 *
 * 
 * PARAM1: char * inbuf
 * PARAM2: unsigned char * outbuf
 * PARAM3: int bcount
 *
 * RETURNS: 
 */

int
hex2bin(char * inbuf, unsigned char * outbuf, int bcount)
{
   int   i;
   char *   cp =  inbuf;

   for (i = 0; i < bcount; i++)
   {
      if (ishexdigit(*cp))
         outbuf[i] = (unsigned char)(hexnibble(*cp) << 4);
      else
         return -1;
      cp++;
      if (ishexdigit(*cp))
         outbuf[i] |= (unsigned char)hexnibble(*cp);
      else
         return -1;
      cp++;
   }
   return 0;
}


/* 
 * Altera Niche Stack Nios port modification:
 * Wrap these routines with IN_MENUS so that they only
 * build if the globals above they depend on (also wrapped with
 * IN_MENUS) build
 */
#ifdef IN_MENUS

#ifdef PING_APP   /* only if app ping is there  */
#include "app_ping.h"

int ping_process_ping_parm (u_long, u_long);
struct ping_err ping_err;


/* FUNCTION: ping_set_parm
 * 
 * This function sets the destination host, delay, and length fields for
 * subsequent ping requests.
 * 
 * INPUT: (1) Type of parameter being sent (e.g., PARM_TYPE_HOST)
 *        (2) Value for parameter being sent
 *
 * OUTPUT: Return code from ping_process_ping_parm () for non-OS_PREEMPTIVE
 *         environments; 0 if successful and PING_ST_ALLOC_ERR if 
 *         couldn't allocate memory for message to PING client task
 *         (for multitasking environments).
 */
 
int ping_set_parm (u_long parm_type, u_long value)
{
#ifdef OS_PREEMPTIVE
   struct pingtask_msg * msgp;

   msgp = (struct pingtask_msg *) PING_ALLOC (sizeof (struct pingtask_msg));
   if (!msgp)
   {
      ++ping_err.alloc_fail;
      return PING_ST_ALLOC_ERR;
   }
   
   /* send message to PING client task */
   msgp->type = PING_CNTRL_SET_PARM;
   msgp->parm1 = parm_type;
   msgp->parm2 = value;

   LOCK_NET_RESOURCE (PINGQ_RESID);
   putq(&pingRcvq, (q_elt)msgp);
   UNLOCK_NET_RESOURCE (PINGQ_RESID);

   post_app_sem (PING_SEMID);
#else
   return (ping_process_ping_parm (parm_type, value));
#endif   

   return 0;
}


/* FUNCTION: ping_process_ping_parm
 * 
 * This function sets the destination host, delay, and length fields for
 * subsequent ping requests.
 * 
 * INPUT: (1) Type of parameter being sent (e.g., PARM_TYPE_HOST)
 *        (2) Value for parameter being sent
 *
 * OUTPUT: 0 if successful, or PING_ST_BAD_ARG2 if an unrecognized type
 *         is encountered.
 */

int ping_process_ping_parm (u_long type, u_long value)
{
   switch (type)
   {
      case PARM_TYPE_HOST:
         activehost = (ip_addr) value;
         break;
      case PARM_TYPE_DELAY:
         pingdelay = value;
         break;
      case PARM_TYPE_LENGTH:
         deflength = value;
         break;
      default:
         ++ping_err.bad_parmtype;
         return PING_ST_BAD_ARG2;
   }

   return 0;
}

#endif  /* PING_APP */

#endif /* Altera modification: IN_MENUS */

/* end of file nrmenus.c */


