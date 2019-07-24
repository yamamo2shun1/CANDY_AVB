/*
 * FILENAME: menulib.c
 *
 * Copyright  2000 By InterNiche Technologies Inc. All rights reserved
 *
 * Misc utility routines for Interniche menuing system
 *
 * MODULE: MISCLIB
 *
 * ROUTINES: setdebug(), set_upc(), snmp_stat(), iface_stats(), 
 * ROUTINES: ifstats(), linkstats(), settrapsize(), if_configbyname()
 * ROUTINES: if_netbytext
 *
 * PORTABLE: yes
 */

/* Additional Copyrights: */
/* Portions Copyright 1993 by NetPort software */


#include "ipport.h"
#include "in_utils.h"

#include "q.h"
#include "netbuf.h"
#include "net.h"
#include "ip.h"
#include "icmp.h"
#include "ether.h"
#include "arp.h"

#include "menu.h"
#include "nvparms.h"

int ifstats(void * pio, struct net * ifp);
char * print_uptime(unsigned long timetick);

unsigned atoh(char *);

#ifdef IP_V6
#include "ip6.h"
#endif

/* FUNCTION: setdebug()
 *
 * setdebug() - toggle NDEBUG protocol stack debugging options
 * 
 * PARAM1: void * pio
 *
 * RETURNS: 0
 */

int
setdebug(void * pio)
{
#ifdef NPDEBUG
     char *   cp;

   /* get an optional parameter */
   cp = nextarg(((GEN_IO)pio)->inbuf);

   /* if no parameter was entered */
   if (!*cp)
   {
      if (NDEBUG)
      { NDEBUG &= UPCTRACE;
         ns_printf(pio,"IP stack debug tracing off\n");
      }
      else
      { NDEBUG |= INFOMSG|IPTRACE|PROTERR|TPTRACE;
         ns_printf(pio,"IP stack debug tracing on\n");
      }
   }
   else
   {
      /* '?' means display NDEBUG value, else set NDEBUG flag to parameter value */
      if (*cp != '?')
      {
#ifdef IN_MENUS
         NDEBUG = atoh(cp);
#endif
      }
      ns_printf(pio,"NDEBUG is now 0x%x\n",NDEBUG);
   }
#else
   ns_printf(pio,"not a DEBUG compile.\n");
#endif
   return 0;
}



/* FUNCTION: set_upc()
 *
 * toggle state of upcall variable
 *
 * PARAM1: void * pio
 *
 * RETURNS: 0
 */

int
set_upc(void * pio)
{
   if (NDEBUG & UPCTRACE)
      NDEBUG &= ~UPCTRACE;
   else
      NDEBUG |= UPCTRACE;

   ns_printf(pio,"Upcall debugging %sabled\n", 
    (NDEBUG&UPCTRACE)?"en":"dis");

   return 0;
}


/* FUNCTION: if_configbyname(NET ifp)
 *
 * Configure the passed net interface from a set of NV parameters with
 * same name. This allows the NV system to specify parameters for
 * dynamic interfaces, and also frees the NV file from having to
 * specifiy the nets in the order they come up.
 *
 *
 * PARAM1: NET to configure - should have ifp->name set up.
 *
 * RETURNS: Returns 0 if OK, -1 if name not found
 */

#ifdef   INCLUDE_NVPARMS   /* only if using NV parms */
int
if_configbyname(NET ifp)
{
   int   i;

   /* search the NV parameters "nets" array for for our name */
   for(i = 0; i < MAXNETS; i++)
   {
      if(strncmp(inet_nvparms.ifs[i].name, ifp->name, IF_NAMELEN - 1) == 0)
      {
         /* found a matching name, set our IP congif from NV entry */
         ifp->n_ipaddr = inet_nvparms.ifs[i].ipaddr;
         ifp->snmask = inet_nvparms.ifs[i].subnet;
         ifp->n_defgw = inet_nvparms.ifs[i].gateway;
         return 0;
      }
   }
   return -1;
}
#endif   /* INCLUDE_NVPARMS */

/* FUNCTION: if_netbytext()
 *
 * get a net pointer based on a user text string. The string may be a ones
 * based number index (eg "1", "2") or an iface name ("pp0"). Prints error
 * message to passed output device if text does not map to an interface.
 * 
 *
 * PARAM1: void * pio   - output stream for error reporting
 * PARAM2: char * iftext - text of interface.
 *
 * RETURNS: pointer to iface if OK, else NULL.
 */


NET
if_netbytext(void * pio, char * iftext)
{
   unsigned iface;
   NET      ifp;

   if((strlen(iftext) == 1) && (isdigit(*iftext)))
   {
      iface = (unsigned)(*iftext - '0') - 1;
      if((iface >= ifNumber) ||
         ((ifp = if_getbynum((int)iface)) == NULL))
      {
           ns_printf(pio,"interface number must be 1 to %d.\n", ifNumber);
           return NULL;
      }
      else
         return (ifp);
   }
   else     /* not a single digit, look up as name */
   {
      for(ifp = (NET)(netlist.q_head); ifp; ifp = ifp->n_next)
      {
         if(strcmp(iftext, ifp->name) == 0)
            return ifp;
      }
      if(!ifp)
           ns_printf(pio,"no interface named \"%s\".\n", iftext);
   }
   return NULL;
}


/* FUNCTION: iface_stats()
 * 
 * menu routine to get iface number and invoke stats
 *
 * PARAM1: void * pio - output stream
 *
 * RETURNS: 0
 */

int
iface_stats(void * pio)
{
   char *   cp;
   struct net * ifp;

   ifp = (NET)(netlist.q_head);   /* default to first net */

   cp = nextarg(((GEN_IO)pio)->inbuf);    /* get console arg */
   if(cp && *cp)     /* arg is present on command line */
   {
      ifp = if_netbytext(pio, cp);
      if(ifp == NULL)      /* error parsing iface name/number text? */
         return -1;
   }

   ifstats(pio, ifp);
   return 0;
}


/* FUNCTION: ifstats()
 *
 * per-iface portion of iface_stats()
 *
 * PARAM1: void * pio   - output stream
 * PARAM2: int ifc      - interface to dump
 *
 * RETURNS: 0
 */

int
ifstats(void * pio, struct net * ifp)
{
   ns_printf(pio, "Interface %s - %s \n", 
      ifp->name, ifp->n_mib->ifDescr);
#ifdef IP_V4
   ns_printf(pio,"IPv4 address: %s, " , print_ipad(ifp->n_ipaddr));
   ns_printf(pio,"subnet mask: %s, ", print_ipad(ifp->snmask));
   ns_printf(pio,"gateway: %s\n"    , print_ipad(ifp->n_defgw));
#endif   /* IP_V4 */
#ifdef IP_V6
   if(ifp->n_flags & NF_IPV6)
   {
      int i;
      char ip6buf[46];     /* tmp buffer for ipv6 address text */

      for(i = 0; i < MAX_V6_ADDRS; i++)
      {
         if(ifp->v6addrs[i])
         {
            ns_printf(pio,"IPv6 %6s addr: %s", v6types[i], 
               print_ip6(&(ifp->v6addrs[i]->addr), ip6buf));
            ns_printf(pio," - %s\n", 
               ip6addrstates[ifp->v6addrs[i]->flags & IA_STATEMASK]);
         }
      }
   }
   else
      ns_printf(pio,"No IPv6 addresses\n");
#endif   /* IP_V6 */
   ns_printf(pio,"Status; Admin:%s Oper:%s for: %s\n", 
      ifp->n_mib->ifAdminStatus==1?"up":"down",
      ifp->n_mib->ifOperStatus==1?"up":"down",
      print_uptime(sysuptime() - (ifp->n_mib->ifLastChange)));
   ns_printf(pio,"rcvd: errors:%lu   dropped:%lu   station:%lu   bcast:%lu   bytes:%lu\n",
      ifp->n_mib->ifInErrors, ifp->n_mib->ifInDiscards,
      ifp->n_mib->ifInUcastPkts, ifp->n_mib->ifInNUcastPkts,
      ifp->n_mib->ifInOctets);
   ns_printf(pio,"sent: errors:%lu   dropped:%lu   station:%lu   bcast:%lu   bytes:%lu\n",
      ifp->n_mib->ifOutErrors, ifp->n_mib->ifOutDiscards,
      ifp->n_mib->ifOutUcastPkts, ifp->n_mib->ifOutNUcastPkts,
      ifp->n_mib->ifOutOctets);
   ns_printf(pio,"MAC address: ");
   hexdump(pio,ifp->n_mib->ifPhysAddress, 6);
   ns_printf(pio," \n");

#ifdef IP_MULTICAST
   /* Print any multicast addresses assign to this iface */
   if(ifp->mc_list)
   {
      struct in_multi * imp;

      ns_printf(pio, "   Multicast addresses: \n");
      for (imp = ifp->mc_list; imp; imp = imp->inm_next)
      {
#ifdef IP_V6
         {
            char ip6buf[40];     /* tmp buffer for ipv6 address text */
            ns_printf(pio, "   %s\n", print_ip6(&(imp->ip6addr), ip6buf));
            continue;
         }
#endif   /* IP_V6 */
         ns_printf(pio, "   %s\n",  print_ipad(imp->inm_addr) );
      }
   }
#endif   /* IP_MULTICAST */


   return 0;
}





/* FUNCTION: linkstats()
 * 
 * printf stats for link interface. default to iface 1
 *
 * PARAM1: void * pio   - output stream
 *
 * RETURNS: 0 if OK, else -1 if arg is bad.
 */

int
linkstats(void * pio)
{
   int      iface =  1;
   char *   cp;

   cp = nextarg(((GEN_IO)pio)->inbuf);

   if (*cp)
   {
      iface = atoi(cp);
      if (iface < 1 || iface > (int)ifNumber)
      {
         ns_printf(pio,"interface number must be 1 to %d.\n", ifNumber);
         return -1;
      }
   }
   iface--;

   if (nets[iface]->n_stats)
      nets[iface]->n_stats(pio,iface);  /* actually call the stats */
   else
      ns_printf(pio,"no stats routine for interface %d\n", iface+1);
   return 0;
}


/* FUNCTION: settrapsize()
 *
 * settrapsize() - This sets a value which will cause the memory 
 * managment monitoring code to trap to a debugger if a malloc is 
 * attempted for this size. This is usefull when a memory leak is 
 * occuring of memory buffers on an unusual size, as often happens 
 * with networking code. It has nothing to do with SNMP traps. 
 * Returns no meaningfull value; declared as int to make compiler 
 * happy. 
 *
 * 
 * PARAM1: void * pio   - output stream
 *
 * RETURNS: 0
 */

extern   int   memtrapsize;   /* in memman.c or user heap code */

int
settrapsize(void * pio) 
{
   char *   cp;

   cp = nextarg(((GEN_IO)pio)->inbuf);

   memtrapsize = atoi(cp);
   ns_printf(pio,"malloc trap size set to %d\n", memtrapsize);
   return 0;
}

/* FUNCTION: send_grat_arp()
 * 
 * menu routine to get iface number and send gratuitouts ARP
 *
 * PARAM1: void * pio - output stream
 *
 * RETURNS: 0
 */

int grat_arp(NET net, int flag);

int
send_grat_arp(void * pio)
{
   char *   cp;
   struct net * ifp;
   int  flag;

   ifp = (NET)(netlist.q_head);   /* default to first net */

   cp = nextarg(((GEN_IO)pio)->inbuf);    /* get console arg */
   if(cp && *cp)     /* arg is present on command line */
   {
      if (*cp == 'a')
         flag = 0;
      else
      if (*cp == 'r')
         flag = 1;
      else
      {
          ns_printf(pio,"USAGE: sendarp (a|r) [net]\n");
          return 0;         
      }      
   }

   cp = nextarg(cp);    /* get console arg */
   if(cp && *cp)     /* arg is present on command line */
   {
      ifp = if_netbytext(pio, cp);
      if(ifp == NULL)      /* error parsing iface name/number text? */
         return -1;
   }
   
   grat_arp(ifp, flag);

   USE_ARG(flag);   

   return 0;
}



