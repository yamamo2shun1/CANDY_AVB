/*
 * FILENAME: allports.c
 *
 * Copyright 1998-2008 By InterNiche Technologies Inc. All rights reserved
 *
 * Routines common to most targets. 
 *
 * MODULE: MISCLIB
 *
 * ROUTINES: netmain_init(), icmp_port_du(), station_state(), 
 * ROUTINES: ftpc_callback(), sysuptime(), packet_check(), 
 * ROUTINES: mcastlist(), prep_modules(), 
 *
 * The functions in this file are generic functions 
 * and hence used for all builds. In addition to the above functions,
 * globals "name" and "prompt" are also initialized in this file.
 *
 * This file was previously .\misclib\cmnport.c
 *
 * PORTABLE: yes
 * They are portable for all targets that InterNiche supports. In future
 * if there is need for any of them  to be made target-specific, then
 * #ifndef wrappers can be provided over here.
 */

#include "ipport.h"

#ifdef DHCP_SERVER
#include "dhcpport.h"
#endif   /* DHCP_SERVER */

#include "libport.h"
#include "q.h"
#include "netbuf.h"
#include "net.h"
#include "ether.h"
#include "arp.h"
#include "ip.h"
#include "icmp.h"
#include "udp.h"
#ifdef IP_V6
#include "ip6.h"
#endif

#ifdef INCLUDE_NVPARMS     /* system uses InterNiche NV system */
#include "nvparms.h"
#endif

#ifdef USE_PPP
#include "ppp_port.h"
#endif   /* USE_PPP */

#ifdef USE_MODEM
#include "mdmport.h"
#else
#ifdef USE_PPP
#include "../mppp/mppp.h" /* for pppcfg */
#endif
#endif   /* USE_MODEM */

#ifdef PING_APP
#include "app_ping.h"
#endif
#ifndef TIMEFOR1TICK
#define TIMEFOR1TICK       (1000/TPS)
#endif

#if defined (IP_MULTICAST) && (defined (IGMP_V1) || defined (IGMP_V2))
#include "../ipmc/igmp_cmn.h"
#endif /* IP multicast and (IGMPv1 or IGMPv2) */

#include "menu.h"
#ifdef INCLUDE_INICHE_LOG
#include "iniche_log.h"
#endif
#ifdef OSPORT_H
#include OSPORT_H
#endif

/* various network entry points */
extern   char* ip_startup(void);
extern   void  inet_timer(void);

#ifdef WEBPORT
extern   int   http_init(void);   /* start up the web server */
extern   int   prep_http(void);
#endif

#ifdef TK_STDIN_DEVICE
extern   void  kbdio(void);
#endif

#if defined(TFTP_CLIENT) || defined(TFTP_SERVER)
extern   int   tftp_init(void);
#endif

#ifdef TELNET_SVR
extern   int   tel_init(void);
extern   void  tel_cleanup(void);
extern   int   prep_telnet(void);
#endif

#ifdef DHCP_CLIENT
extern   void  dhc_setup(void);
#endif

#ifdef DHCP_SERVER
extern   int   dhcp_init(void);
extern   int   prep_dhcpsrv(void);
extern   void  dhcpsrv_cleanup(void);
#endif

#ifdef DNS_CLIENT
#include "dns.h"
#endif

#ifdef TCP_ECHOTEST
extern   int   tcp_echo_init(void);
extern   void  tcp_echo_cleanup(void);
extern   void  tcp_echo_recv(void);
#endif

#ifdef TCP_CIPHERTEST
extern   int   tcp_cipher_init(void);
extern   void  tcp_cipher_cleanup(void);
extern   void  tcp_cipher_recv(void);
#endif

#ifdef FTP_CLIENT
extern   int   fc_check(void);
extern   void  (*fc_callback)(void *fc,int logstate, int cmdstate);
extern   void  ftpc_callback(void *fc,int logstate, int cmdstate);
extern   int   prep_ftpc(void);
#endif

#ifdef VFS_FILES
extern   int   prep_vfs(void);
#endif

#ifdef SMTP_ALERTS
extern   int   smtp_init(void);
extern   int   prep_smtp(void);
#endif

#ifdef NATRT
extern   int   prep_natrt(void);
#endif

#ifdef  IP_MULTICAST
/* Add multicast test program call and a dummy definition of mcastlist */
extern   void  u_mctest_init(void);
extern   int   mcastlist(struct in_multi *);
#endif   /* IP_MULTICAST */

#ifdef TESTMENU   /* after menus.h */
extern   struct menu_op testmenu[10];
#endif   /* TESTMENU */

#ifdef CSUM_DEMO
extern   int   cksum_init(void);
extern   void  csum_init(void);
#endif

#ifdef DNS_SERVER
extern   int   dns_srvr_check(void);
extern   int   dns_srvr_init(int );
extern   void  dns_srvr_timer(void);
#endif   /* DNS_SERVER */

#ifdef UDPSTEST
extern   int   udp_echo_init(void);
extern   void  udp_echo_cleanup(void);
#endif   /* UDPSTEST */

#ifdef RIP_SUPPORT 
extern   int   rip_init(void);
extern   void  rip_cleanup(void);
extern   int   prep_rip(void);
#endif   /* RIP_SUPPORT */

#ifdef INCLUDE_SNMP
extern   int   snmp_init(void);
extern   void  snmp_cleanup(void);
extern   int   prep_snmp(void);
#endif   /* INCLUDE_SNMP */

#ifdef RAWIPTEST
extern int raw_test_init(void);
#endif   /* RAWIPTEST */

#ifdef SUPERLOOP
#define task_stats(x)     ;
#else
void task_stats(void * pio);
#endif

#ifdef FULL_ICMP
void icmp_port_du(PACKET p, struct destun * pdp);
#endif

#ifdef FTP_SERVER
extern   int   ftps_init(void);
extern   void  ftps_cleanup(void);
#endif

#if defined(MEMDEV_SIZE) && defined(VFS_FILES)
extern int   init_memdev(void);
#endif

#ifdef USE_AUTOIP
extern   int   Upnp_init(void);
#endif   /* USE_AUTOIP */

#ifdef INICHE_SYSLOG 
extern   int   syslog_init(void);
extern   void  closelog(void);
extern   int   prep_syslog(void);
#endif   /* INICHE_SYSLOG */

#ifdef INCLUDE_SSLAPP
extern int sslapp_init(void);
#endif   /* INCLUDE_SSLAPP */
#ifdef SUPERLOOP
int iniche_net_ready = FALSE;
#endif

#ifdef USE_SNTP_V4
int sntpv4_init (void);
int sntpv4_app (void);
#endif

/* The following global variables are used in most ports. They are used by code
 * covered under a variety of ipport.h feature ifdefs. Experience has show that
 * it's not practical to do a complete ifdef to omit these when they are not
 * in use. It would require a huge multiple OR case, and many preprocessors
 * don't handle ifdef logic that complex.
 */

char * name = "InterNiche Portable TCP/IP, v3.1 \n";
char * prompt = "INET> ";     /* prompt for console */
ip_addr activehost  =  0L;    /* common host parameter */
u_long  pingdelay   =  TPS;   /* common delay parameter (1 second) */
int     deflength   =  64;    /* common length parameter */

/* PING_APP-related variables */
#ifdef PING_APP
extern ip_addr ping4_activehost; /* default ping host */
extern u_long ping4_delay;       /* 1 second between pings */
extern u_long ping4_deflength;   /* default ping packet data length */
#endif /* PING_APP */

#ifdef DNS_CLIENT_UPDT
extern u_long dns_update_server;
#endif   /* DNS_CLIENT_UPDT */


#ifdef USE_COMPORT
#include "comline.h"          /* include if not yet included */
/* Struct contaning all configuration global params for Comport */
struct ComPortCfg comportcfg;
#endif   /* USE_COMPORT */

/* static net structs, so we can patch in default IP address. */
extern   struct net  netstatic[STATIC_NETS];


/* FUNCTION: netmain_init()
 * 
 * Initialize all the modules that are being compiled in.
 * This function is generic and is required for all builds
 *
 * Tasks do their own initialization. Hence for modules which have
 * their own tasks, we don't do the initialization in netmain_init(). 
 * That is done by putting then under "#ifdef SUPERLOOP"
 * 
 * PARAM1: 
 *
 * RETURNS: 
 */

void 
netmain_init(void)
{
   int   e = 0;
   char *   msg;
#ifdef IP_V6
   ip6_addr host;
#endif

   printf("%s\n", name);
   printf("Copyright 1996-2008 by InterNiche Technologies. All rights reserved. \n");
#ifdef IN_MENUS
   install_version("allports3.1");
#endif
#ifndef SUPERLOOP
   /* call this to do pre-task setup including intialization of port_prep */
   msg = pre_task_setup();
   if (msg)
      panic(msg);
#endif

#ifdef INCLUDE_NVPARMS     /* system uses InterNiche NV system */
   e = get_nv_params();    /* get flash parameters into data structs */
   if (e)
   {
      printf("fatal error (%d) reading NV parameters.\n", e);
      panic("nv");
   }

   /* set static iface IP info up from stored parameters. These may 
   be overwritten from command line parms or DHCP later. */
   {
      int   i;

      for (i = 0; i < STATIC_NETS; i++)
      {
         netstatic[i].n_ipaddr = inet_nvparms.ifs[i].ipaddr;
         netstatic[i].snmask = inet_nvparms.ifs[i].subnet;
         netstatic[i].n_defgw = inet_nvparms.ifs[i].gateway;
#ifdef IP_MULTICAST
         /* Create a dummy entry for the Ethernet interface mcastlist */
         /* If this entry is set to NULL, multicast is not supported  */
         /* on this interface */
         netstatic[i].n_mcastlist = mcastlist;
#if defined (IGMP_V1) || defined (IGMP_V2)
         if ((inet_nvparms.ifs[i].igmp_oper_mode != IGMP_MODE_V1) &&
             (inet_nvparms.ifs[i].igmp_oper_mode != IGMP_MODE_V2))
            netstatic[i].igmp_oper_mode = IGMP_MODE_DEFAULT;
         else
            netstatic[i].igmp_oper_mode = inet_nvparms.ifs[i].igmp_oper_mode;
#endif /* IGMPv1 or IGMPv2 */
#endif /* IP_MULTICAST */
      }
   }

#ifdef DNS_CLIENT
   /* set DNS client's server list from nvparms information */
   MEMCPY(dns_servers, inet_nvparms.dns_servers, sizeof(dns_servers));

#ifdef DNS_CLIENT_UPDT
   MEMCPY(soa_mname, inet_nvparms.dns_zone_name, sizeof(soa_mname));
   MEMCPY(&dns_update_server, &inet_nvparms.dns_update_server, sizeof(dns_update_server));
#endif   /* DNS_CLIENT_UPDT */

#endif   /* DNS_CLIENT */

#ifdef USE_COMPORT
   comportcfg.comport = comport_nvparms.comport;
   comportcfg.LineProtocol = comport_nvparms.LineProtocol;
#endif   /* USE_COMPORT */
#endif   /* INCLUDE_NVPARMS */

#ifndef INCLUDE_NVPARMS
#ifdef USE_COMPORT
   comportcfg.comport = 0x01;
   comportcfg.LineProtocol = PPP;   /* Default to PPP */
#endif   /* USE_COMPORT */
#endif   /* INCLUDE_NVPARMS */

#ifdef IP_V6
         ip6_init();
#endif

#ifdef INCLUDE_INICHE_LOG
   if(global_log_create())
   {
      printf("global_log_create() failed\n");
   }
   glog_with_type(LOG_TYPE_INFO, "INICHE LOG initialized", 1);
#endif

   msg = ip_startup();
   if (msg)
   {
      printf("inet startup error: %s\n", msg);
      panic("IP");
   }

#if defined(MEMDEV_SIZE) && defined(VFS_FILES)
   init_memdev(); /* init the mem and null test devices */
#endif

#ifdef IP_MULTICAST
#ifdef INCLUDE_TCP
   /* call the IP multicast test program */
   u_mctest_init();
#endif
#endif  

   /* clear debugging flags. Port can optionally turn them
    * back on in post_task_setup();
    * NDEBUG = UPCTRACE | IPTRACE | TPTRACE ;  
    */
   NDEBUG = 0;    

   /* print IP address of the first interface - for user's benefit */
   printf("IP address of %s : %s\n" , ((NET)(netlist.q_head))->name,
      print_ipad(((NET)(netlist.q_head))->n_ipaddr));
 
#ifndef SUPERLOOP
   /* call this per-target routine after basic tasks & net are up */
   msg = post_task_setup();
   if (msg)
      panic(msg);
#endif

#ifdef PING_APP
   ping_init();
#endif   /* PING_APP */

#ifdef RAWIPTEST
   raw_test_init();
#endif   /* RAWIPTEST */

#if defined(TFTP_CLIENT) || defined(TFTP_SERVER)
   tftp_init();
#endif   /* TFTP */

#ifdef TESTMENU
   install_menu(testmenu);
#endif   /* TESTMENU */
#ifdef CSUM_DEMO
   cksum_init();
#endif

#ifdef USE_AUTOIP
   Upnp_init();      /* start Auto IP before DHCP client */
#endif   /* USE_AUTOIP */

#ifdef DHCP_CLIENT
   dhc_setup();   /* kick off any DHCP clients */
#endif   /* DHCP_CLIENT */

#ifdef DHCP_SERVER
#ifdef INCLUDE_NVPARMS
   if(dhserve_nvparms.ServeDHCP)
#endif
   {
      e = dhcp_init();
      if(e)
      {
         dprintf("Error %d starting DHCP server.\n",e);
      }
      else
      {
         exit_hook(dhcpsrv_cleanup);
         dprintf("Started DHCP server\n");
      }
   }
#endif /* DHCP_SERVER */

#ifdef IN_MENUS
   printf(prompt);
#endif

#ifdef UDPSTEST
   e=udp_echo_init();
   if ( e == SUCCESS )
   {
      exit_hook(udp_echo_cleanup);
   }
   else
      dprintf("Error %d starting UDP Echo server.\n",e);
#endif

#ifdef RIP_SUPPORT
   e=rip_init();
   if ( e == SUCCESS )
   {
      exit_hook(rip_cleanup);
   }
   else
      dprintf("Error %d starting RIP server.\n",e);
#endif

#ifdef INICHE_SYSLOG
   e =syslog_init();
   if (e == SUCCESS)
      exit_hook(closelog);
   else
      dprintf("Error %d initializing syslog client.\n",e);
#endif

#ifdef FTP_CLIENT
   fc_callback=ftpc_callback;
#endif

#ifdef INCLUDE_SSLAPP
   sslapp_init();
#endif

#ifdef USE_SNTP_V4
   sntpv4_init ();
#endif

/* The following initializations take place when SUPERLOOP is enabled.
 * Otherwise they would be done in the respective task.
 */

#ifdef SUPERLOOP

#ifdef INCLUDE_SNMP
   e = snmp_init();
   if (e == SUCCESS)
      exit_hook(snmp_cleanup);
   else
      dprintf("Error %d initializing SNMP agent.\n",e);
#endif   /* INCLUDE_SNMP */

#ifdef WEBPORT
   e = http_init(); /* start up http server */
   if (e)
      dprintf("Error %d starting HTTP server.\n",e);
#endif   /* WEBPORT */

#ifdef FTP_SERVER
   e = ftps_init();
   if ( e == SUCCESS )
   {
      exit_hook(ftps_cleanup);
   }
   else
      dprintf("Error %d starting FTP server.\n",e);
#endif   /* FTP_SERVER */

#ifdef TELNET_SVR
   e=tel_init();
   if ( e == SUCCESS )
   {
      exit_hook(tel_cleanup);
   }
   else
      dprintf("Error %d starting TELNET server.\n",e);
#endif

#ifdef TCP_ECHOTEST
   e=tcp_echo_init();
   if ( e == SUCCESS )
   {
      exit_hook(tcp_echo_cleanup);
   }
   else
      dprintf("Error %d starting TCP Echo server.\n",e);
#endif
#ifdef TCP_CIPHERTEST
   e=tcp_cipher_init();
   if ( e == SUCCESS )
   {
      exit_hook(tcp_cipher_cleanup);
   }
   else
      dprintf("Error %d starting TCP cipher server.\n",e);
#endif
#ifdef USE_CRYPTOENG
   e = ce_init();
   if(e != 0)
   {
      dprintf("ce_init() failed\n");
      panic("prep_modules");
   }
#endif

#ifdef SMTP_ALERTS
   smtp_init ();
#endif

#ifdef CSUM_DEMO
   csum_init();
#endif

#ifdef USE_SNTP_V4
   e = sntpv4_app ();
   if(e != 0)
   {
      dprintf("Failed to start time sync via SNTPv4 client\n");
      panic("prep_modules");
   }
#endif

#endif   /* SUPERLOOP */

#ifdef INCLUDE_SSLAPP
   e = sslapp_init();
   if(e != 0)
   {
      dprintf("sslapp_init() failed\n");
      panic("prep_modules");
   }
#endif
   USE_ARG(e);    /* Avoid compiler warnings */

} /* end of netmain_init() */


#ifdef FULL_ICMP
char * icmpdu_types[] = {
   "NET",
   "HOST",
   "PROT",
   "PORT",
   "FRAG",
   "SRC",
};

/* FUNCTION: imcp_port_du()
 *
 * PARAM1: void * pio
 *
 * RETURNS: 
 */

void
icmp_port_du(PACKET p, struct destun * pdp)
{
   dprintf("got ICMP %s UNREACHABLE from %s\n", 
      icmpdu_types[(int)(pdp->dtype)], print_ipad(p->fhost) );
   dprintf(prompt);
}
#endif   /* FULL_ICMP */



/* FUNCTION: station_state()
 *
 * state() - printf some info about the current state of the user 
 * settable station variables. 
 * 
 * PARAM1: void * pio
 *
 * RETURNS: 
 */

int
station_state(void * pio)
{
   int i;
   
#ifndef NO_INET_STACK
   NET ifp;

   for (i = 0, ifp = (NET)netlist.q_head; ifp; ifp = ifp->n_next, i++)
   {
      ns_printf(pio, "iface %d-%s IP addr:%s  ", 
       i, ifp->name, print_ipad(ifp->n_ipaddr) );
      ns_printf(pio, "subnet:%s  ", print_ipad(ifp->snmask) );
      ns_printf(pio, "gateway:%s\n", print_ipad(ifp->n_defgw) );
   }
#endif   /* NO_INET_STACK */

   ns_printf(pio, "current tick count %lu\n", cticks);

   ns_printf(pio, "common delay parameter:  %lu ticks (%lu ms).\n", pingdelay, (pingdelay * TIMEFOR1TICK));
   ns_printf(pio, "common host parameter: %s\n", print_ipad(activehost));
   ns_printf(pio, "common length parameter: %d\n", deflength);

#ifdef PING_APP
   ns_printf(pio, "ping delay:  %lu ticks (%lu ms).\n", ping4_delay, (ping4_delay * TIMEFOR1TICK));
   ns_printf(pio, "ping host: %s\n", print_ipad(ping4_activehost));
   ns_printf(pio, "ping length: %lu\n", ping4_deflength);
#endif

#ifdef USE_MODEM
   ns_printf(pio, "current dialout number is %s\n", mdm_dial_string);
#endif   /* USE_MODEM */

#ifdef USE_PPP
   ns_printf(pio, "current dial-in user name is %s\n", pppcfg.username);
   ns_printf(pio, "current dial-in password is %s\n", pppcfg.password);
#endif   /* USE_PPP */

   task_stats(pio);

   return 0;
}

#ifdef FTP_CLIENT
/* If FC_USECALLBACK is enabled in ftpclnt.h, and if fc_callback 
 * is defined, then FTP Client will call fc_callback when 
 * its state changes. FC_USECALLBACK is enabled by default. 
 */

void ftpc_callback(void *fc,int logstate, int cmdstate)
{
   /*
   dprintf("logstate=%d, cmdstate=%d\n",logstate,cmdstate);
   */

   USE_VOID(fc);
   USE_ARG(logstate);
   USE_ARG(cmdstate);
}

#endif 


#ifndef NO_INET_STACK   /* these functions are pretty stack-centric */


/* FUNCTION: sysuptime()
 * 
 * sysuptime() - return MIB-2 sys group compatable "sysUptime" value
 *
 * PARAM1: 
 *
 * RETURNS: 
 */

unsigned long
sysuptime()
{
   return ((cticks/TPS)*100);    /* 100ths of a sec since boot time */
}


/* FUNCTION: packet_check() - check for incoming packet 
 * Process incoming packets.
 *
 * inside_pktdemux is nonzero while processing a received packet. 
 * It is used for reentrancy protection. 
 *
 * PARAM1: 
 *
 * RETURNS: 
 */


static int inside_pktdemux = 0; 

void
packet_check(void)
{
   if(inside_pktdemux != 0)   /* check re-entrancy flag */
      return;           /* do not re-enter pktdemux(), packet will wait... */
   inside_pktdemux++;   /* set re-entrany flag */
   pktdemux();          /* process low level packet input */
   inside_pktdemux--;   /* clear re-entrany flag */
}

#ifdef IP_MULTICAST
/* This is a dummy routine that is replaced by the porting engineer with
 * a routine by the same name in the Ethernet driver.  The purpose of this
 * routine is to convert IP multicast addresses to their Ethernet multicast
 * addresses and program the chip with the appropriate Ethernet multicast
 * filters.  In the case of PPP, this dummy routine can be used.
 */


/* FUNCTION: mcastlist()
 * 
 * PARAM1: struct in_multi *multi_ptr
 *
 * RETURNS: 
 */

int
mcastlist(struct in_multi * multi_ptr)
{
   USE_ARG(multi_ptr);

   return 0;
}
#endif   /* IP_MULTICAST */

/********* For Superloop AND Netmain builds *********************/

/* FUNCTION: prep_modules()
 * 
 * Call the preparation functions for all modules.
 * Each module can have a preparation function wherein it
 * intializes the menu, nvparms, etc.
 *
 * PARAM1: 
 *
 * RETURNS:  0 on SUCCESS.
 *
 */


#ifdef USE_PPP
extern   int   ppp_setup(void);
#endif   /* USE_PPP */

#ifdef USE_MODEM
extern   int   prep_modem(void);
#endif   /* USE_MODEM */

int prep_modules(void)
{
#ifdef IP_V6
   ip6_addr host;
   int i;
#endif

int e = 0;


#ifdef IP_V6
   for (i = 0; i < STATIC_NETS; i++)
   {
		/* zero out addresses */
		{
			int ifIndx;

			for (ifIndx = 0; ifIndx < MAX_V6_ADDRS; ifIndx++)
				netstatic[i].v6addrs[ifIndx] = (struct ip6_inaddr *)0;
			for (ifIndx = 0; ifIndx < MAX_V6_ADDRS; ifIndx++)
				netstatic[i].v6addrsExtd[ifIndx] = (struct ip6_inaddr *)0;
		}
   }
   ip6_init();
#endif

#ifdef PING_APP
   ping_init();
#endif   /* PING_APP */


#ifdef USE_CRYPTOENG
   e = ce_init();
   if(e != 0)
   {
      dprintf("ce_init() failed\n");
      panic("prep_modules");
   }
#endif
#ifdef USE_PPP
   e = ppp_setup();
   if (e != 0)
   {
      dprintf("PPP Module setup failed\n");
      panic("prep_modules");
   }
#endif   /* USE_PPP */

#ifdef USE_MODEM
   e = prep_modem();
   if (e != 0)
   {
      dprintf("Modem Module prep failed\n");
      panic("prep_modules");
   }
#endif   /* USE_MODEM */

#ifdef TELNET_SVR
   e = prep_telnet();
   if (e != 0)
   {
      dprintf("Telnet Module prep failed\n");
      panic("prep_modules");
   }
#endif   /* TELNET_SVR */

#ifdef DHCP_SERVER
   e = prep_dhcpsrv();
   if (e != 0)
   {
      dprintf("DHCP Server Module prep failed\n");
      panic("prep_modules");
   }
#endif   /* DHCP_SERVER */

#ifdef NATRT
   e = prep_natrt();
   if (e != 0)
   {
      dprintf("Nat Router Module prep failed\n");
      panic("prep_modules");
   }
#endif   /* NATRT */

#ifdef RIP_SUPPORT
   e = prep_rip();
   if (e != 0)
   {
      dprintf("Rip Module prep failed\n");
      panic("prep_modules");
   }
#endif   /* RIP_SUPPORT */

#ifdef INCLUDE_SNMP
   e = prep_snmp();
   if (e != 0)
   {
      dprintf("SNMP Module prep failed\n");
      panic("prep_modules");
   }
#endif   /* INCLUDE_SNMP */

#ifdef INICHE_SYSLOG
   e = prep_syslog();
   if (e != 0)
   {
      dprintf("Syslog Client prep failed\n");
      panic("prep_modules");
   }
#endif   /* INICHE_SYSLOG */

#ifdef SMTP_ALERTS
   e = prep_smtp();
   if (e != 0)
   {
      dprintf("SMTP Module prep failed\n");
      panic("prep_modules");
   }
#endif   /* SMTP_ALERTS */

#ifdef VFS_FILES
   e = prep_vfs();
   if (e != 0)
   {
      dprintf("VFS Module prep failed\n");
      panic("prep_modules");
   }
#endif   /* VFS_FILES */


#ifdef FTP_CLIENT
   e = prep_ftpc();
   if (e != 0)
   {
      dprintf("FTP Client Module prep failed\n");
      panic("prep_modules");
   }
#endif   /* FTP_CLIENT */

#ifdef WEBPORT
   e = prep_http();
   if (e != 0)
   {
      dprintf("Web Server Module prep failed\n");
      panic("prep_modules");
   }
#endif   /* WEBPORT */

#ifdef INCLUDE_SSLAPP
   e = sslapp_init();
   if(e != 0)
   {
      dprintf("sslapp_init() failed\n");
      panic("prep_modules");
   }
#endif
   return 0;
}

#endif   /* NO_INET_STACK */

