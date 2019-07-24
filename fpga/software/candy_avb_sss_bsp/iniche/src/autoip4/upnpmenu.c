/*
 * FILENAME: upnpmenu.c
 *
 * Copyright  2002 By InterNiche Technologies Inc. All rights reserved
 *
 *   Universal plug and play menu stats & control user interface
 *
 * ROUTINES: upnp_stats(), upnp_disable(), upnp_restart(), 
 * ROUTINES: upnp_base(),
 *
 *
 * MODULE: AUTOIP4
 *
 * ROUTINES: 
 *
 * PORTABLE: yes
 */



#include "ipport.h"

#ifdef USE_UPNP
#ifdef IN_MENUS

#include "q.h"
#include "netbuf.h"
#include "net.h"
#include "menu.h"

#include "autoip.h"
#include "upnp.h"
#include "ds_app.h"

/* routine in ..\ip to get NET pointer from console text */
extern NET if_netbytext(void * pio, char * cp);

/* UPnP menu routines */
int   upnp_stats(void * pio);
int   upnp_disable(void * pio);
int   upnp_restart(void * pio);
int   upnp_base(void * pio);
int   upnp_db(void * pio);

char * upnp_states[] = /* match eUPNP_STATE enums to text */
{
   "START",
   "INIT",
   "DHCP_SEEK",
   "GOT_DHCP_ADDRESS",
   "AUTO_IP",
   "FIXED_IP",
   "SSDP_NOTIFY",
   "SSDP_WAIT",
   "SSDP_DISCOVER",
   "STARTUP_FINISHED",
   "IDLE",
   "IDLE_FIXED",
   "DHCP_CHECKUP",
   "DHCP_RESEEK",
   "DISABLED",
};


struct menu_op upnpmenu[]  = 
{
   "upnp",        stooges,          "Upnp menu",
   "upstats",     upnp_stats,       "status of UPnP Interfaces",
   "updisable",   upnp_disable,     "disable UPnP on an interface",
   "uprestart",   upnp_restart,     "restart UPnP process on interface",
   "upbase",      upnp_base,        "Set new base for UPnP address pool",
   "updbase",     upnp_db,          "Dump UPnP database for interface",
   NULL,
};



/* FUNCTION: upnp_stats()
 * 
 * Basic status dump menu routine 
 *
 * PARAM1: void * pio
 *
 * RETURNS: 0 if OK, Else ENP_ error code
 */

int
upnp_stats(void * pio)
{
   int   i;

   ns_printf(pio, "UPnP stats:\n");
   
   for(i = 0; i < MAXNETS; i++)
   {
      if(nets[i] == NULL)
         break;
      ns_printf(pio, "iface %s; state:%s; current IP:%u.%u.%u.%u\n",
         nets[i]->name, upnp_states[upnp[i].state],
         PUSH_IPADDR(nets[i]->n_ipaddr));
   }

   return 0;
}


/* FUNCTION: upnp_disable()
 * 
 * Menu routine to disable UPnP on a specified interface
 * 
 * PARAM1: void * pio
 *
 * RETURNS: 0 if OK, Else ENP_ error code
 */

int
upnp_disable(void * pio)
{
   char *   cp;
   NET      ifp;
   int      iface;

   cp =  nextarg( ((GEN_IO)pio)->inbuf );
   if(!*cp)
   {
      ns_printf(pio, "please enter iface to disable UPnP\n");
      return -1;
   }
   ifp = if_netbytext(pio, cp);
   if(ifp == NULL)      /* error parsing iface name/number text? */
      return -1;
   iface = if_netnumber(ifp);
   upnp[iface].state = UPNP_DISABLED;
   ns_printf(pio, "Disabled UPnP in iface %s\n", ifp->name);
   return 0;
}


/* FUNCTION: upnp_restart()
 * 
 * Menu routine to restart the Auto-Ip process on the specified interface
 * 
 * PARAM1: void * pio
 *
 * RETURNS:  0 if OK, Else ENP_ error code
 */

int
upnp_restart(void * pio)
{
   char *   cp;
   NET      ifp;
   int      iface;

   cp =  nextarg( ((GEN_IO)pio)->inbuf );
   if(!*cp)
   {
      ns_printf(pio, "please enter iface to restart UPnP\n");
      return -1;
   }
   ifp = if_netbytext(pio, cp);
   if(ifp == NULL)      /* error parsing iface name/number text? */
      return -1;
   iface = if_netnumber(ifp);

   /* skip the DHCP step and go right to autoIP */
   upnp[iface].state = UPNP_START;
   upnp[iface].ip_method = eIP_METHOD_AUTO_FIXED;
   autoIPs[iface].state = AUTOIP_START;
   autoIPs[iface].response_timer = 0;
   autoIPs[iface].arp_attempts = 0;

   ns_printf(pio, "(re)started UPnP in iface %s\n", ifp->name);
   return 0;
}


/* FUNCTION: upnp_base()
 * 
 * Menu routine to change the address pool used by Auto IP
 * 
 * PARAM1: void * pio
 *
 * RETURNS:  0 if OK, Else ENP_ error code
 */

int
upnp_base(void * pio)
{
   char *   cp;
   u_long   newbase;
   u_long   oldrange;
   unsigned mask;

   cp =  nextarg( ((GEN_IO)pio)->inbuf );
   if(!*cp)
   {
      ns_printf(pio, "please enter IP address for new base\n");
      return -1;
   }

   cp = parse_ipad(&newbase, &mask, cp);
   if(cp)
   {
      ns_printf(pio, "Bad IP address: %s\n", cp);
      return -1;
   }

   oldrange = dAUTO_IP_RANGE;
   dBASE_AUTO_IP_ADDRESS = htonl(newbase);   /* store in local endian */
   dMAX_AUTO_IP_ADDRESS = htonl(newbase + oldrange);

   ns_printf(pio, "Changed base of Auto-IP address pool to %u.%u.%u.%u\n",
      PUSH_IPADDR(newbase) );

   return 0;
}

int
upnp_db(void * pio)
{
   char *   cp;
   NET      ifp;
   int      iface;
   ip_addr  addr;
   ip_addr  mask;
   ip_addr  gateway;

   cp =  nextarg( ((GEN_IO)pio)->inbuf );
   if(!*cp)
   {
      ns_printf(pio, "please enter iface for database dump\n");
      return -1;
   }
   ifp = if_netbytext(pio, cp);
   if(ifp == NULL)      /* error parsing iface name/number text? */
      return -1;
   iface = if_netnumber(ifp);

   /* Get the fixed IP info from database into tmp vars */
   DS_get_long(tag_NET_FIXED_IP, iface, &addr);
   DS_get_long(tag_NET_FIXED_SUBNET, iface, &mask);
   DS_get_long(tag_NET_FIXED_GATEWAY, iface, &gateway);

   ns_printf(pio, "iface:%s; ", ifp->name);
   ns_printf(pio, " Fixed IP: %u.%u.%u.%u", PUSH_IPADDR(addr) );
   ns_printf(pio, "  mask:  %u.%u.%u.%u", PUSH_IPADDR(mask) );
   ns_printf(pio, "  gateway:  %u.%u.%u.%u\n", PUSH_IPADDR(gateway) );

   /* Get the current IP info from database into tmp vars */
   DS_get_long(tag_NET_IP_ADDRESS, iface, &addr);
   DS_get_long(tag_NET_SUBNET, iface, &mask);
   DS_get_long(tag_NET_GATEWAY, iface, &gateway);

   ns_printf(pio, "Current ip: %u.%u.%u.%u", PUSH_IPADDR(addr));
   ns_printf(pio, "  mask:  %u.%u.%u.%u", PUSH_IPADDR(mask));
   ns_printf(pio, "  gateway:  %u.%u.%u.%u\n", PUSH_IPADDR(gateway));

   return 0;
}


#endif   /* IN_MENUS */
#endif   /* USE_UPNP */


