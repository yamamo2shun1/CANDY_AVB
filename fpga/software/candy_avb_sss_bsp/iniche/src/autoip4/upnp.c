/*
 * FILENAME: upnp.c
 *
 * Copyright  2002 By InterNiche Technologies Inc. All rights reserved
 *
 *   Universal plug and play
 *
 *   This file has all the top level UPnP managment routines.
 *   It uses: 
 *     - DHCP client for Addressing
 *     - AutoIP for Addressing 
 *     - SSDP for discovery 
 *
 *   9/15/2000 - Created - Stan Breitlow 
 *
 * MODULE: AUTOIP4
 *
 * ROUTINES: Upnp_init(), upnp_recv_dhcp_status(), upnp_tick(), 
 * ROUTINES: Play (), Upnp_tick(), upnp_set_fixed_net_vars(), 
 * ROUTINES: Upnp_GetDnsOptionsFromDhcpOptions(), 
 * ROUTINES: upnp_add_net_vars_to_database(),
 *
 * PORTABLE: yes
 */



#include "ipport.h"

#ifdef USE_UPNP

#include "q.h"
#include "netbuf.h"
#include "net.h"
#include "ip.h"
#include "dhcpclnt.h"
#include "dns.h"
#include "dhcpclnt.h"
#include "intimers.h"

#include "autoip.h"
#include "upnp.h"
#include "ds_app.h"


#ifndef USE_AUTOIP
#error USE_AUTOIP must be defined with USE_UPNP
#endif

#ifdef INCLUDE_NVPARMS
#include "nvparms.h"    /* For nvparms struct */
#endif   /* INCLUDE_NVPARMS */


/* local objects */

struct   tUPNP upnp[MAXNETS];
long  upnp_timer;
int   DHCP_WAIT_TIME = 300;   /* 30 seconds default */

#ifndef USER_DS_SUPPORT
struct ds_struct ds_structs[MAXNETS];  /* database */
#endif

/* local protos */

int upnp_recv_dhcp_status(int iface, int state);
static void upnp_set_fixed_net_vars(int iface);
static void upnp_add_net_vars_to_database(int iface);
static void Upnp_GetDnsOptionsFromDhcpOptions(void);
static void upnp_add_Fixed_vars_to_database(int iface);

extern void fixup_subnet_mask(int);

#ifdef IN_MENUS
#include "menu.h"
extern struct menu_op upnpmenu[5];
#endif   /* IN_MENUS */

/* default method of getting IP address */
eIP_METHOD addressing_method = eIP_METHOD_DHCP_AUTO_FIXED; 


/* FUNCTION: Upnp_init()
 * 
 *   Initialize the sub modules
 *
 * PARAM1: void
 *
 * RETURNS: 
 */

int Upnp_init(void)
{
   int   i;
   int   e;
   struct tUPNP * up;

   e = AutoIp_init();
   if (e)
   {
      dtrap();
   }

   for (i = 0; i < MAXNETS; i++)
   {
      up = &upnp[i];

      /* Disable Auto-IP on unset nets. If a dynamic interface wants this later, 
       * the creator will have to explicitly enable it.
       */
      if(nets[i] == NULL)
      {
         up->state = UPNP_DISABLED;
         continue;
      }

      /* Only do this on ethernets */
      if(nets[i]->n_mib->ifType != ETHERNET)
         continue;

      /* nets[] should already have info set from NV parms system or
       * by other means. Save this info to the "UPnP database items now.
       */
      upnp_add_Fixed_vars_to_database(i);    /* set fixed info */
      upnp_add_net_vars_to_database(i);      /* set tmp info */

#ifdef INCLUDE_NVPARMS
      /* Skip interfaces which are not flagged to be DHCP clients */
      if (inet_nvparms.ifs[i].client_dhcp) /* is DHCP Client enabled ? */
         nets[i]->n_flags |= NF_AUTOIP;  /* yes, then use AUTOIP too */
#endif   /* INCLUDE_NVPARMS */

      if (!(nets[i]->n_flags & NF_AUTOIP))
      {
         up->state = UPNP_DISABLED;
         continue;
      }

      up->state = UPNP_START;
      up->idle_timer = 0 ;
      up->notify_delay = 0;
      up->notify_count = 0;
      up->got_dhcp_address = FALSE;

      dhc_set_callback(i, upnp_recv_dhcp_status );

      /* Check parameter database for IP setting */
      DS_get_word(tag_IP_ADDRESS_MODE, i, &addressing_method);
      up->ip_method = addressing_method;
   }

#ifdef IN_MENUS
   install_menu(&upnpmenu[0]);
   install_version("autoip3.1");
#endif   /* IN_MENUS */

#ifdef USER_SMTP_ALERTS
   /* If "user smtp" service is supported, do the init now. */
   UserSmtp_init();
#endif

   /* start the AutoIP interval timer */
   upnp_timer = in_timerset(Upnp_tick, 100, 0L );
   if(upnp_timer == 0)
      return -1;

   return 0;
}



/* FUNCTION: upnp_recv_dhcp_status()
 *
 *    Callback that receives status changes from the dhcp client.
 *
 * PARAM1: int iface
 * PARAM2: int state
 *
 * RETURNS: 
 */

int upnp_recv_dhcp_status(int iface, int state)
{
    struct tUPNP * up;

   up = &upnp[iface];

   switch (state)
   {
   case DHCS_UNUSED:       /* no discovery attempted  */
   case DHCS_INIT:         /* Ready to send a DISCOVER packet  */
   case DHCS_INITREBOOT:   /* Have IP, ready to send REQUEST(skip DISCOVER) */
   case DHCS_REBOOTING:    /* rebooting/reclaiming address  */
   case DHCS_SELECTING:    /* discovery sent, but no offer yet  */
   case DHCS_REQUESTING:   /* sent request; waiting for ACK|NAK  */
   case DHCS_RENEWING:     /* Renewing the lease  */
   case DHCS_REBINDING:    /* rebinding to new server  */
   case DHCS_RESTARTING:  
      break;

      /* got a ACK we liked  */
   case DHCS_BOUND:
      up->got_dhcp_address = TRUE;
      dhc_main_ipset(iface, state);
      break;
   }

   return(0);
}


/* FUNCTION: upnp_tick()
 * 
 *   Called every 100ms to run finite state machine.
 *   Manages the Address and Discover phases of AutoIP
 *
 * PARAM1: void
 *
 * RETURNS: void
 */

u_long   last_upnptick;

void
Upnp_tick(long dummy)
{
   int   iface;
   struct tUPNP * up;
   int   timerlag;

   /* if we missed a long interval, derive a value to update the timer */
   if((cticks - last_upnptick) > (TPS/10))
   {
      /* approx. number of 100ms intervals we missed. The "|10"
       * on TPS is to prevent a divide by zero if TPS < 10 .
       */
      timerlag = (cticks - last_upnptick)/((TPS|10)/10);
   }
   else
      timerlag = 0;

   last_upnptick = cticks;

   for(iface = 0; iface < MAXNETS; iface++)
   {
      /* first blank net means we're done with loop */
      if(nets[iface] == NULL)
         return;

      up = &upnp[iface];

      if(!up)     /* iface is not doing Auot IP, skip it. */
         continue;

      /* update timers if we had a long break */
      if(timerlag)
         up->idle_timer += timerlag;

      switch (up->state)
      {

         /*-------------------------------------------------------- */
         /* Let things settle before starting */
         /*-------------------------------------------------------- */

      case UPNP_START:

         up->state = UPNP_INIT;
         break;

         /*-------------------------------------------------------- */
         /* Determine where to start the sequence. */
         /* If the device is configured to use only fixed IP */
         /* we will skip dhcp and AutoIP */
         /*-------------------------------------------------------- */

      case UPNP_INIT:

         /* Use configured IP addressing method */
         switch (up->ip_method)
         {
         default:
         case eIP_METHOD_DHCP_AUTO_FIXED:
         case eIP_METHOD_DHCP_FIXED: 

            up->state = UPNP_DHCP_SEEK; 
            break;

         case eIP_METHOD_AUTO_FIXED: 

            up->state = UPNP_AUTO_IP; 
            /* turn off dhcp, otherwise it will reset  */
            dhc_halt(iface);
            break;

         case eIP_METHOD_FIXED: 

            up->state = UPNP_FIXED_IP; 
            /* turn off dhcp, otherwise it will reset  */
            dhc_halt(iface);
            break;
         }
         up->idle_timer = 0;
         break;

         /*-------------------------------------------------------- */
         /* We are waiting for the dhcp server to give us an address. */
         /* We only wait so long before timing out. */
         /*-------------------------------------------------------- */

      case UPNP_DHCP_SEEK:

         if (up->got_dhcp_address)
         {
            up->state = UPNP_GOT_DHCP_ADDRESS; 
            break;
         }

         /* If dhcp dos not resolve after a while */
         /* try auto IP if it is allowed by the configuration */
         up->idle_timer++;
         if (up->idle_timer >= (unsigned)DHCP_WAIT_TIME)
         {
            /* What is the next phase */
            if (up->ip_method == eIP_METHOD_DHCP_AUTO_FIXED)
               up->state = UPNP_AUTO_IP;
            else
               up->state = UPNP_FIXED_IP;
            /* turn off dhcp, otherwise it will reset  */
            dhc_halt(iface);
         }
         break;

         /*-------------------------------------------------------- */
         /* We go to this state if the dhcp client gets an address */
         /* Set the active IP address to the found address */
         /*-------------------------------------------------------- */

      case UPNP_GOT_DHCP_ADDRESS:

         /* put info in the database */
         upnp_add_net_vars_to_database(iface);
         /* Start the discovery phase */
         /*      up->state = UPNP_SSDP_NOTIFY;  */
         Upnp_GetDnsOptionsFromDhcpOptions();
         up->state = UPNP_STARTUP_FINISHED; 
         break;

         /*-------------------------------------------------------- */
         /* If we did not get an address from a dhcp server */
         /* We will try to get an IP address via AutoIP,  */
         /* this involve ARP probing. */
         /*-------------------------------------------------------- */

      case UPNP_AUTO_IP:

         AutoIp_tick(iface);
         switch (AutoIp_get_state(iface))
         {
         case AUTOIP_GOT_ADDRESS:  /* put info in the database */
            upnp_add_net_vars_to_database(iface);
            /*up->state = UPNP_SSDP_NOTIFY;  */
            up->state = UPNP_STARTUP_FINISHED;
            break;
         case AUTOIP_FAILED:
            up->state = UPNP_FIXED_IP;    
            break;
         default:
            break;
         }
         break;

         /*-------------------------------------------------------- */
         /* Use a hardcoded IP address */
         /*-------------------------------------------------------- */

      case UPNP_FIXED_IP:
         /* Get the fix IP info back from the database */
         upnp_set_fixed_net_vars(iface);

         /* If the fixed IP address is zero, take down the iface (for now) */
         nets[iface]->n_mib->ifOperStatus = NI_DOWN;

         /* put info in the database */
         upnp_add_net_vars_to_database(iface);
         DS_set_byte(tag_POLLING_ENABLED, iface, 1);

#ifdef USER_SMTP_ALERTS
         /* If "user smtp" service is supported, do the init now. */
         UserSmtp_LookupStart();
#endif
         switch (up->ip_method)
         {
         case eIP_METHOD_DHCP_AUTO_FIXED:
         case eIP_METHOD_DHCP_FIXED: 
         default:
            up->state = UPNP_IDLE;
            break;

            /* This one will not look for a DHCP in the future */
         case eIP_METHOD_FIXED: 
            up->state = UPNP_IDLE_FIXED;
            break;
         }
         break;

         /*-------------------------------------------------------- */
         /* Tell the network about ourselves. */
         /* Do this several times 1 second apart. */
         /* This is recommended by the UPNP spec */
         /*-------------------------------------------------------- */

      case UPNP_SSDP_NOTIFY:

         /* not using ssdp currently */
         up->state = UPNP_STARTUP_FINISHED;

         /*ssdp_notify(iface); */
         up->notify_delay = 0;
         up->state = UPNP_SSDP_WAIT;
         break;

         /*-------------------------------------------------------- */
         /* Wait after advertising */
         /*-------------------------------------------------------- */

      case UPNP_SSDP_WAIT:

         if (++up->notify_delay > SSDP_NOTIFY_DELAY)
         {
            if (++up->notify_count > SSDP_NOTIFY_COUNT) 
               up->state = UPNP_SSDP_DISCOVER;
            else
               up->state = UPNP_SSDP_NOTIFY;
         }
         break;

         /*-------------------------------------------------------- */
         /* Look for the other devices we need */
         /*-------------------------------------------------------- */

      case UPNP_SSDP_DISCOVER:

         /* skip anything here for now */
         up->state = UPNP_STARTUP_FINISHED;
         break;

         /*-------------------------------------------------------- */
         /* The startup phase is complete */
         /* Let the system know it can "do its thing" */
         /*-------------------------------------------------------- */

      case UPNP_STARTUP_FINISHED:

         DS_set_byte(tag_POLLING_ENABLED, iface, 1);
         up->state = UPNP_IDLE;
         up->idle_timer = 0;
#ifdef USER_SMTP_ALERTS
         UserSmtp_LookupStart();
#endif 
         break;

         /*-------------------------------------------------------- */
         /* Spin here until its time to do monitoring and checkup stuff */
         /*-------------------------------------------------------- */

      case UPNP_IDLE:

         up->idle_timer++;
         if (up->idle_timer >= CHECKUP_TIME)
         {
            /* don't recheck if we have a dhcp address already */
            if (up->got_dhcp_address == FALSE)
            {
               up->state = UPNP_DHCP_CHECKUP;
            }
            /* Mark net as up so we can try again */
            nets[iface]->n_mib->ifOperStatus = NI_UP;
            up->idle_timer = 0;
         }
         break;

         /*-------------------------------------------------------- */
         /* Here we stay as fixed IP */
         /*-------------------------------------------------------- */

      case UPNP_IDLE_FIXED:
         break;

         /*-------------------------------------------------------- */
         /* Check for a dhcp that might have come on-line  */
         /*-------------------------------------------------------- */

      case UPNP_DHCP_CHECKUP:

         /* start dhcp again */
         dhc_state_init(iface, TRUE);
         dhc_set_callback(iface, upnp_recv_dhcp_status );
         up->state = UPNP_DHCP_RESEEK;
         up->idle_timer = 0;
         break;

         /*-------------------------------------------------------- */
         /* We are in AutoIP mode or DHCP Failed->Fixed mode */
         /* let's looking for a dhcp server again */
         /*-------------------------------------------------------- */

      case UPNP_DHCP_RESEEK:

         if (up->got_dhcp_address)
         {
            up->state = UPNP_GOT_DHCP_ADDRESS; 
            break;
         }

         /* If dhcp dos not resolve after a while return to idle */
         up->idle_timer++;
         if (up->idle_timer >= (unsigned)DHCP_WAIT_TIME)
         {
            /* Nothing found, back to idle */
            up->state = UPNP_IDLE;
            /* turn off dhcp, otherwise it will reset  */
            dhc_halt(iface);
         }
         break;

      case UPNP_DISABLED:
         break;

      default:
         dtrap();
         break;
      } /* switch */
   } /* for */

#ifdef USER_SMTP_ALERTS
   UserSmtp_Tick();
#endif

   USE_ARG(dummy);
}



/* FUNCTION: upnp_set_fixed_net_vars()
 * 
 * PARAM1: int iface
 *
 * RETURNS: 
 */

static   void
upnp_set_fixed_net_vars(int iface)
{
   NET ifp;

   ifp = nets[iface];

   /* Get the fixed IP address info out of database */
   DS_get_long(tag_NET_FIXED_IP, iface, &ifp->n_ipaddr);
   DS_get_long(tag_NET_FIXED_SUBNET, iface, &ifp->snmask);
   DS_get_long(tag_NET_FIXED_GATEWAY, iface, &ifp->n_defgw);

   if ( ifp->snmask == 0 )
   {
      fixup_subnet_mask(iface);
      dhc_states[iface].snmask = ifp->snmask; 
   }

   /* fixup broadcast addresses  */
   ifp->n_netbr    = ifp->n_ipaddr | ~ifp->snmask;
   ifp->n_netbr42  = ifp->n_ipaddr &  ifp->snmask;
   ifp->n_subnetbr = ifp->n_ipaddr | ~ifp->snmask;

}


/* FUNCTION: Upnp_GetDnsOptionsFromDhcpOptions()
 * 
 * PARAM1: void
 *
 * RETURNS: 
 */

void
Upnp_GetDnsOptionsFromDhcpOptions(void)
{
#ifdef DNS_CLIENT
   int   i;
   int   iface;

   iface = 0;
   i=0;
   while ((i < DHC_MAXDNSRVS) && (i < MAXDNSSERVERS))
   {
      dns_servers[i] = dhc_states[iface].dnsrv[i];
      i++;
   }
#endif /* DNS_CLIENT */
}


/* FUNCTION: upnp_add_net_vars_to_database()
 * 
 * PARAM1: int iface
 *
 * RETURNS: 
 */

static   void
upnp_add_net_vars_to_database(int iface)
{
   char  buf[32];
   NET   ifp;

   ifp = nets[iface];

   /* Put this info in the database */
   DS_set_long(tag_NET_IP_ADDRESS, iface, ifp->n_ipaddr);
   DS_set_long(tag_NET_SUBNET, iface, ifp->snmask);
   DS_set_long(tag_NET_GATEWAY, iface, ifp->n_defgw);

   sprintf(buf,"%u.%u.%u.%u\0", PUSH_IPADDR(ifp->n_ipaddr));
   DS_set_string(tag_IP_ADDRESS_STRING, iface, buf);
}


/* FUNCTION: upnp_add_net_vars_to_database()
 * 
 * PARAM1: int iface
 *
 * RETURNS: 
 */

static   void
upnp_add_Fixed_vars_to_database(int iface)
{
   NET   ifp;

   ifp = nets[iface];

   /* Put this info in the database */
   DS_set_long(tag_NET_FIXED_IP, iface, ifp->n_ipaddr);
   DS_set_long(tag_NET_FIXED_SUBNET, iface, ifp->snmask);
   DS_set_long(tag_NET_FIXED_GATEWAY, iface, ifp->n_defgw);
}

#endif /* USE_UPNP */


