/*
 * FILENAME: ipstart.c
 *
 * Copyright 1996- 2000 By InterNiche Technologies Inc. All rights reserved
 *
 * Network startup file. The idea is the main() routine (or 
 * whatever) application calls ip_startup(), which does all the low
 * level work of finding interfaces, starting IP, Arp, SNMP, etc
 *
 * MODULE: INET
 *
 * ROUTINES: ip_startup(), exit_hook(), ip_exit(), 
 * ROUTINES: if_netnumber(),
 *
 * PORTABLE: yes
 */

/* Additional Copyrights: */
/* Portions Copyright 1990 - 1994 by NetPort Software */
/* Portions Copyright 1986 by Carnegie Mellon */


#include "ipport.h"
#include "q.h"
#include "netbuf.h"
#include "net.h"
#include "ip.h"
#include "icmp.h"
#include "udp.h"

#ifdef INCLUDE_ARP
#include "arp.h"
#endif

#ifdef IP_MULTICAST
#include "../ipmc/igmp.h"
#endif

#ifdef NATRT
extern   int nat_init(void);
#endif

#if (defined(USE_PPP) && defined(LB_XOVER))
extern   int ppp_lb_add_routes(void);
#endif 


#ifndef MULTI_HOMED     /* if single interface, set up everything here */
/* if your compiler complains about the next line, it means 
your ipport.h defined MAXNETS != 1 without defining MULTI_HOMED */
#if (MAXNETS != 1)
#error MULTI_HOMED must be defined if "MAXNETS" is greater than 1
#endif   /* MAXNETS != 1 */
#endif   /* MULTI_HOMED */

/* static memory for interface structs and interface mib data */
struct net  netstatic[STATIC_NETS];  /* actual net structs */

/* pointer array to all nets, to support older code */
struct net *   nets[MAXNETS];

#ifdef IP_MULTICAST
extern   ip_addr  igmp_all_hosts_group;
#endif


#ifdef INCLUDE_TCP
extern   int tcpinit(void);
extern  void evtmap_setup(void);
#endif

#ifdef USE_IPFILTER
int  ipf_init(void);
void ipf_cleanup(void);
#endif

unsigned ifNumber = 0;        /* number of net interfaces */

char * ipmcfail_str = "unable to initialize IP Multicast";


/* FUNCTION: ip_startup()
 *
 * startup() - Start up a customized IP stack as defined by ipport.h 
 * 
 * PARAM1: none
 *
 * RETURNS: returns NULL if OK, or text of an error message 
 */

char *   
ip_startup()
{
   int   e; /* error holder */
   int   i;

   /* thread nets[] and attach mib data to nets[] arrays */
   for (i = 0; i < STATIC_NETS; i++)
   {
      nets[i] = &netstatic[i];   /* set up array of pointers */
      nets[i]->n_mib = &nets[i]->mib;   /* set mib pointer */

      /* add static iface to end of nets list */
      putq(&netlist, nets[i]);
   }

   /* call port routine to locate and init network interfaces. */
   ifNumber = (unsigned)prep_ifaces(ifNumber);

   if (ifNumber < 1)    /* no static interfaces? */
   {
#ifdef DYNAMIC_IFACES
      /* print a debug message and hope user knows what he's doing */
      dprintf("unable to find any working interfaces");
#else /* static ifaces only */
      /* no static and no dynamic interfaces is probably a bug... */
      return("unable to find any working interfaces");
#endif   /* DYNAMIC_IFACES */
   }

   /* throw away any unused static nets */
   for (i = ifNumber; i < STATIC_NETS; i++)
   {
      qdel(&netlist, (qp)nets[i]);  /* remove from queue */
      nets[i] = NULL;               /* remove from array */
   }


   /* The sequence of events when initing the net & interface systems 
    * is very important. Be very carefull about altering the order of 
    * the following statements. 
    */
   /* once these are done, we should call ip_exit before quiting IP */
   clock_init();           /* start clock system */
   exit_hook(clock_c);

   e = Netinit();    /* start net interface(s) */
   if (e)
   {
      return("unable to initialize net");
   }

#ifdef INCLUDE_ARP
   e = etainit();          /* startup ARP layer */
   if (e)
   {
      ip_exit();
      return("unable to initialize arp");
   }
#endif

#ifdef IP_V4
   e = ip_init();       /* start up IP layer */
   if (e)
   {
      ip_exit();
      return("unable to initialize IP");
   }
#endif /* IP_V4 */

#ifdef IP_V6
   e = ip6_init();       /* start up IPv6 */
   if (e)
   {
      ip_exit();
      return("unable to initialize IPv6");
   }
#endif /* IP_V6 */


#if defined (IP_MULTICAST) && (defined (IGMP_V1) || defined (IGMP_V2))
   /* Join the All hosts group on every interface that IP multicast is
    * supported
    */
   e = igmp_init();         /* Initialize igmp */
   if (e)
   {
      ip_exit();
      return(ipmcfail_str);
   }

   for (i = 0; i < (int)ifNumber; i++)
   {
      if (nets[i]->n_mcastlist != NULL)
         if ((in_addmulti(&igmp_all_hosts_group, nets[i], 4) == NULL))
      {
         ip_exit();
         return(ipmcfail_str);
      }
   }
#endif /* IP_MULTICAST and (IGMPv1 or IGMPv2) */   

#ifdef INCLUDE_TCP
   e = tcpinit();
   if (e)
   {
      ip_exit();
      return("unable to initialize TCP");
   }
   
   /* setup event map for (UDP and TCP) socket library's events (such as 
    * those used by tcp_sleep () and tcp_wakeup ()).  These events either 
    * map into operating system primitives such as events or semaphores, 
    * or into task suspend and task resume mechanisms.
    */
   evtmap_setup ();
#endif   /* INCLUDE_TCP */

#ifdef NATRT
   nat_init();
#endif   /* NATRT */

#if (defined(USE_PPP) && defined(LB_XOVER))
   ppp_lb_add_routes();
#endif   /* USE_PPP */

#ifdef USE_IPFILTER
   e = ipf_init();
   if (e)
      return("unable to initialize IP Filter table");
   else
      exit_hook(ipf_cleanup);
#endif

   return(NULL);     /* we got through with no errors */
}


#define  NUMCLOSERS  15
static void (*closers[NUMCLOSERS])(void);
static int nclosers = 0;

/* FUNCTION: exit_hook()
 *
 * This is called to add a routine to a list of callbacks which
 * will be called if the net is gracefully shut down. Sort of like
 * "atexit()", but works on C system which don't support atecit()
 *
 * PARAM1: void (*func
 *
 * RETURNS: 
 */

void
exit_hook(void (*func)(void))
{
   if (nclosers >= (NUMCLOSERS-1))
      panic("exit_hook");

   closers[++nclosers] = func;
}


/* FUNCTION: ip_exit()
 *
 * ip_exit() - undo net hooks as part of shutting down stack.
 * 
 * PARAM1: none
 *
 * RETURNS: void
 */

void
ip_exit()
{
   int   n;

   for (n=nclosers; n; n--)
   {
#ifdef NPDEBUG
      dprintf("ip_exit: calling func %p\n", closers[n]);
#endif
      if(closers[n])
      {
         (*closers[n])();
         closers[n] = NULL;
      }
   }
}



/* FUNCTION: if_netnumber()
 *
 * Get the index given a particular NET pointer.
 * For static nets this will be and index into nets, however dynamic
 * nets will NOT have a nets entry. For these, the index is the
 * current position in the interfaces queue "netlist".
 * 
 * PARAM1: NET pointer
 *
 * RETURNS: net index for passed net pointer
 */

int
if_netnumber(NET nptr)
{
   unsigned i;
   NET ifp;

   for(ifp = (NET)(netlist.q_head), i = 0; ifp; ifp = ifp->n_next, i++)
   {
      if(ifp == nptr)
        return (int)i;
   }

   panic("bad net ptr");
   return 0;
}


