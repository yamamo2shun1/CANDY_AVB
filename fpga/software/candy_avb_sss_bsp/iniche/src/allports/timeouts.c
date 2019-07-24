/*
 * FILENAME: timeouts.c
 *
 * Copyright  2000 By InterNiche Technologies Inc. All rights reserved
 *
 * Handles InterNiche task & application interval timers.
 *
 * MODULE: MISCLIB
 *
 * ROUTINES: inet_timer(),
 *
 * PORTABLE: yes
 */


#include "ipport.h"
#include "in_utils.h"
#include "q.h"
#include "netbuf.h"
#include "net.h"
#include "arp.h"
#include "ip.h"

#ifdef INICHE_TIMERS    /* build using interval timers? */
#include "intimers.h"
static void check_interval_timers(void);
#endif

#ifdef INCLUDE_TCP
void  tcp_tick(void);
#endif

#if defined (IP_MULTICAST) && (defined (IGMP_V1) || defined (IGMP_V2))
/* Call igmp timeout routine */
extern   unsigned long  igmp_cticks;
extern   void igmp_fasttimo(void);
#endif   /* IP_MULTICAST and (IGMPv1 or IGMPv2) */

#ifdef USE_PPP
extern   void ppp_timeisup(void);
#endif
#ifdef DHCP_CLIENT
extern   int dhc_discover(int iface);
extern   int dhc_second(void);
#endif
#ifdef DHCP_SERVER
extern   void dhcp_timeisup(void);
#endif
#ifdef FTP_SERVER
extern   void ftps_check(void);
#endif
#ifdef DNS_CLIENT
extern   void dns_check(void);
#endif
#ifdef NATRT
extern   void nat_timeisup(void);
#endif
#ifdef USE_MODEM
extern   void dial_check(void);
#endif
#ifdef USE_COMPORT
extern   void uart_check(void);
#endif
#ifdef PING_APP
void ping_check(void);
#endif
#ifdef IP_V6
void ip6_timer(void);
static unsigned long v6timer_mod = 0L;
#endif
#ifdef TCP_ECHOTEST
void tcp_echo_poll(void);
#endif
#if defined(TFTP_CLIENT) || defined(TFTP_SERVER)
extern   void  tftp_tick(void);
#endif
#ifdef UDPSTEST
extern void   udp_echo_poll(void);
#endif /* UDPSTEST */
#ifdef RIP_SUPPORT 
extern void   rip_check(void);
#endif /* RIP_SUPPORT */
#ifdef INCLUDE_SNMP
extern void snmp_check(void);
#endif 
#ifdef BTREE_ROUTES
extern void   rtbtree_tmo(void);
#endif   /* BTREE_ROUTES */
#ifdef RAWIPTEST
extern void   raw_testerq_poll(void);
#endif /* RAWIPTEST */
#ifdef WEBPORT
extern   void  http_check(void);
#endif
#ifdef TELNET_SVR
extern   void  tel_check(void);
#endif
#ifdef FTP_CLIENT
void ftpc_periodic_timer(void);
#endif
#ifdef IP_FRAGMENTS
extern u_long ire_cticks;
extern u_char ip_reasm_process_timer_tick(void);
#endif
#ifdef USE_SNTP_V4
extern int sntpv4_timeout(void);
#endif

/* if INICHE_TASKS is defined, this is done vxd_task() in task for Vxd */
#ifndef INICHE_TASKS
#ifdef USE_VXD
extern void vxd_check(void);
#endif
#endif

unsigned long nextppp = 0L;   /* tick for next call to ppp timer */

void (*port_1s_callout)(void) = NULL;

/* FUNCTION: inet_timer()
 *
 * This handles all TCP/IP related timeouts. Ideally this should be 
 * called about 10 times a second; and no less than twice a second 
 * (The minimum for TCP timeouts). Does NOT handle most
 * application timeouts. 
 *
 * 
 * PARAM1: void
 *
 * RETURNS: 
 */

void
inet_timer(void)
{
#ifdef IP_FRAGMENTS
   /* run thru' the IP reassembly queue (once every second) */
   if (ire_cticks < cticks)
      ip_reasm_process_timer_tick ();
#endif

#ifdef INCLUDE_TCP
   tcp_tick();          /* run TCP timers */
#endif

#ifdef INICHE_TIMERS    /* interval timers? */
   check_interval_timers();
#endif

#if defined (IP_MULTICAST) && (defined (IGMP_V1) || defined (IGMP_V2))
   /* Call igmp timeout routine */
   if (igmp_cticks < cticks)  /* Call igmp timeout routine 5 times per sec */
      igmp_fasttimo();
#endif   /* IP_MULTICAST and (IGMPv1 or IGMPv2) */

#ifdef USE_SNTP_V4
      sntpv4_timeout ();
#endif

#ifdef USE_MODEM
   dial_check();
#endif   /* USE_MODEM */

#ifdef USE_COMPORT
   uart_check();
#endif

#ifdef UDPSTEST
   udp_echo_poll();
#endif

#ifdef PING_APP
   ping_check();  /* check for ping send/receive */
#endif

#ifdef RAWIPTEST
   raw_testerq_poll();
#endif


#ifdef SUPERLOOP
#ifdef WEBPORT
      http_check();
#endif 
#ifdef TELNET_SVR
      tel_check();
#endif 
#ifdef INCLUDE_SNMP
      snmp_check();
#endif 
#endif /* SUPERLOOP */


/* if INICHE_TASKS is defined, this is done vxd_task() in task for Vxd */
#if (defined(USE_VXD) && !defined(INICHE_TASKS))
   vxd_check();
#endif

/* !!!???  */
#ifdef IP_V6
   if ((v6timer_mod < cticks) ||  /* next call to ip6_timer is due */
       (v6timer_mod > (cticks + (10 * TPS))) )  /* for when cticks wraps */
   {
      v6timer_mod = cticks + (TPS / 5);
      ip6_timer();
   }
#endif



   /* Some timer routines only need calling once a second: */
   if ((nextppp < cticks) ||  /* next call to PPP is due */
       (nextppp > (cticks+(10*TPS))) )  /* for when cticks wraps */
   {
      nextppp = cticks + TPS;

      if (port_1s_callout != NULL)
         (*port_1s_callout)();

#ifdef USE_PPP
      ppp_timeisup();
#endif
#ifdef DHCP_CLIENT
      dhc_second();
#endif
#ifdef DHCP_SERVER
      dhcp_timeisup();
#endif
#ifdef DNS_CLIENT
      dns_check();
#endif
#ifdef NATRT
      nat_timeisup();
#endif
#ifdef RIP_SUPPORT 
      rip_check();
#endif 
#if defined(TFTP_CLIENT) || defined(TFTP_SERVER)
      tftp_tick();
#endif

#ifdef BTREE_ROUTES
      rtbtree_tmo();
#endif   /* BTREE_ROUTES */
#ifdef FTP_CLIENT
      ftpc_periodic_timer();
#endif
#ifdef IPSEC
      IPSecTimer();
#endif
   }
}


#ifdef INICHE_TIMERS

struct intimer intimers[NUM_INTIMERS];

/* FUNCTION: check_interval_timers()
 *
 * Check to see if any interval timers are ready to fire.
 *
 * RETURNS: NA
 */

static int numtimers = 0;     /* number of active timers */

static void
check_interval_timers(void)
{
   int   i;
   int   found = 0;  /* number of valid timers found */

   /* if no timers, just return */
   if (numtimers > 0)
   {
      /* loop throught the timer list looking for active timers ready to fire */
      for (i = 0; i < NUM_INTIMERS; i++)
      {
         if (intimers[i].callback)   /* is this timer active? */
	 {
            if ((intimers[i].tmo < cticks) && (!intimers[i].inuse))  /* timer ready fire? */
            {
               intimers[i].tmo = intimers[i].interval + cticks;   /* set next tmo */
               intimers[i].inuse = TRUE;
               intimers[i].callback(intimers[i].parm);      /* call user routine */
               intimers[i].inuse = FALSE;
            }
            /* If we've examined all the active timers, we're done */
            if (++found >= numtimers)
               break;
         }
      }
   }
}

/* FUNCTION: in_timerset()
 *
 * Create an interval timer
 * 
 * PARAM1: callback routine 
 * PARAM2: number of milliseconds between callback calls
 * PARAM3: parameter to pass to callbacks
 *
 * RETURNS: timer ID if OK, else if table is full.
 */

long
in_timerset(void (*callback)(long), long msecs, long parm)
{
   int   i;

   for(i = 0; i < NUM_INTIMERS; i++)
   {
      if(intimers[i].callback == NULL)
      {
         /* found empty table entry, set up new timer */
         intimers[i].callback = callback;
         intimers[i].parm = parm;
         /* set interval, in TPS (cticks) units */
         intimers[i].interval = (msecs * TPS)/1000;
         intimers[i].tmo = intimers[i].interval + cticks;   /* first tmo */
		 intimers[i].inuse = FALSE;
         numtimers++;
         return (long)&intimers[i];
      }
   }
   return 0;
}

/* FUNCTION: in_timerkill()
 *
 * Delete a timer created previously by a call to in_timerset()
 * 
 * PARAM1: long timer Address of the timer to delete.
 *
 * RETURNS: 0 if OK, ENP error if timer not in list.
 */


int
in_timerkill(long timer)
{
   int   i;

   for(i = 0; i < NUM_INTIMERS; i++)
   {
      if(timer == (long)&intimers[i])
      {
         intimers[i].callback = NULL;
         numtimers--;
         return 0;      /* OK return */
      }
   }
   dtrap();    /* timer to kill not found */
   return ENP_PARAM;
}

#endif   /* INICHE_TIMERS */

