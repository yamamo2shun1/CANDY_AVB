/*
 * FILENAME: autoip.c
 *
 * Copyright  2002 By InterNiche Technologies Inc. All rights reserved
 *
 *   AutoIP ARPs random IP addresses in a private, non-routed range.
 *   range: 169.254.x.x
 *   If nothing replys it assigns itself the address.
 *
 *   9/15/2000 - Created - Stan Breitlow
 *
 * MODULE: AUTOIP4
 *
 * ROUTINES: AutoIp_init(), AutoIp_tick(), 
 * ROUTINES: AutoIp_arp_response(), AutoIp_send_arp_probe(), 
 * ROUTINES: AutoIp_pick_address(), AutoIp_get_state(), 
 * ROUTINES: AutoIp_set_net_vars(),
 *
 * PORTABLE: yes
 */


#include "ipport.h"

#ifdef USE_AUTOIP
#ifndef DHCP_CLIENT
#error DHCP_CLIENT must be defined with USE_AUTOIP
#endif

#include "q.h"
#include "netbuf.h"
#include "net.h"
#include "ip.h"
#include "ether.h"
#include "arp.h"
#include "dhcpclnt.h"

#include "autoip.h"
#include "upnp.h"

/* Set defaults for Base & range on AUtoIP address pool */
u_long   dBASE_AUTO_IP_ADDRESS =    0xA9FE0100;    /* 169.254.1.0 */
u_long   dMAX_AUTO_IP_ADDRESS  =    0xA9FEFEFF;    /* 169.254.254.255 */

#define  dRESPONSE_TIMEOUT       2000  /* 2  seconds */
#define  dVERIFY_WAIT_TIME       1000  /* 1  second */
#define  dNUMBER_OF_VERIFIES     3


/* local protos */
static ip_addr AutoIp_pick_address(void);
static void AutoIp_set_net_vars(int iface);

extern   void fixup_subnet_mask(int netnum); /* in ipnet.c */

unshort rand_seed;   /* pseudo-random number */

struct autoIP autoIPs[MAXNETS];

/* FUNCTION: AutoIp_init()
 * 
 *   Initialize the sub modules
 *
 * PARAM1: void
 *
 * RETURNS: 0
 */

int
AutoIp_init()
{
   int      iface;
   void *   stack_garbage = &iface;    /* sort of random... */

   /* look for an ethernet to seed the random numbers */
   for(iface = 0; iface < MAXNETS; iface++)
   {
      if(nets[iface] == NULL)
         break;

      if(nets[iface]->n_mib->ifType == ETHERNET)
      {
         autoIPs[iface].state = AUTOIP_START;
         autoIPs[iface].response_timer = 0;
         autoIPs[iface].arp_attempts = 0;

         /* seed with unique part of mac address  */
         if(!rand_seed)
         {
            rand_seed = (unshort)(nets[iface]->mib.ifPhysAddress[3] + 
               (nets[iface]->mib.ifPhysAddress[4] << 4) + 
               (nets[iface]->mib.ifPhysAddress[5] << 8));
         }
      }
      else
         upnp[iface].state = UPNP_DISABLED;  /* Disable Non-ethernets */
   }
   /* XOR the random numbers from whatever garbage is on the stack.
    * This may generate a compiler warning, but it's doing the right thing.
    */
   rand_seed ^= (unshort)(u_long)(stack_garbage);

   return 0;
}

#ifndef HAL_TIMER_ALREADY
u_long HAL_TIMER_get_ms_tick()
{
   return (cticks * (1000/TPS));
}
#endif

/* FUNCTION: AutoIp_tick()
 * 
 * Called from Upnp_tick() every 100ms to run finite state machine
 *
 * PARAM1: void
 *
 * RETURNS: void
 */

void
AutoIp_tick(int iface)
{
   switch (autoIPs[iface].state)
   {
   case AUTOIP_START:
      autoIPs[iface].state = AUTOIP_ARP_PROBE;
      break;

   case AUTOIP_ARP_PROBE:

      autoIPs[iface].try_address = AutoIp_pick_address();
      AutoIp_send_arp_probe(iface);
      autoIPs[iface].arp_attempts++;
      autoIPs[iface].response_timer = HAL_TIMER_get_ms_tick() + dRESPONSE_TIMEOUT;
      autoIPs[iface].state = AUTOIP_ARP_RESPONSE_WAIT;
      break;

   case AUTOIP_ARP_RESPONSE_WAIT:   

      if (HAL_TIMER_get_ms_tick() > autoIPs[iface].response_timer)
      {
         /* No arp response within the time so  */
         /* we will verify the address  */
         autoIPs[iface].state = AUTOIP_ARP_VERIFY_PROBE;
         autoIPs[iface].verify_attempts = 0;
      }
      break;

   case AUTOIP_ARP_ADDRESS_USED:

      if (autoIPs[iface].arp_attempts < 10) 
      {
         /* try again */
         autoIPs[iface].state = AUTOIP_ARP_PROBE;
      }
      else
      {
         autoIPs[iface].state = AUTOIP_FAILED;
      }
      break;

      /* The address appears to be free,  */
      /* verify still not used */
   case AUTOIP_ARP_VERIFY_PROBE:

      AutoIp_send_arp_probe(iface);
      autoIPs[iface].verify_attempts++;
      autoIPs[iface].response_timer = HAL_TIMER_get_ms_tick() + dVERIFY_WAIT_TIME;
      autoIPs[iface].state = AUTOIP_ARP_VERIFY_WAIT;
      break;

   case AUTOIP_ARP_VERIFY_WAIT:   

      if (HAL_TIMER_get_ms_tick() > autoIPs[iface].response_timer)
      {
         /* If we have verified to our satisfaction */
         /* take the address */
         if (autoIPs[iface].verify_attempts >= dNUMBER_OF_VERIFIES)
         {
            autoIPs[iface].state = AUTOIP_GOT_ADDRESS;
            /* Lets use this address */
            AutoIp_set_net_vars(iface);
         }
         else
         {
            /* verify again */
            autoIPs[iface].state = AUTOIP_ARP_VERIFY_PROBE;
         }
      }
      break;

   case AUTOIP_FAILED:
      break;

   default:
      break;
   }
}



/* FUNCTION: AutoIp_arp_response()
 * 
 *  If an ARP Probe gets a response, the TCP/IP Stack upcalls this function.
 *  We will have to try another address.
 *
 * PARAM1: PACKET pkt
 *
 * RETURNS: void
 */

void
AutoIp_arp_response(struct arp_hdr * arphdr, NET ifp)
{
   int iface;

   iface = if_netnumber(ifp);

   /* Check that we are in a mode that cares about all this */
   if (autoIPs[iface].state == AUTOIP_ARP_RESPONSE_WAIT)
   {
      /* Check for someone else probing this as a target address, IE their
       * target (probed) address is that same as the one we are trying
       */
      if (arphdr->ar_tpa == autoIPs[iface].try_address)
      {
         autoIPs[iface].state = AUTOIP_ARP_ADDRESS_USED;
         return;
      }

      /* Also check for a response from the probed address.
       * This means some already has the address.
       */
      if (arphdr->ar_spa == autoIPs[iface].try_address)
      {
         autoIPs[iface].state = AUTOIP_ARP_ADDRESS_USED;
         return;
      }
   }
}



/* FUNCTION: AutoIp_send_arp_probe()
 * 
 * PARAM1: interface index
 *
 * RETURNS: void
 */

void
AutoIp_send_arp_probe(int iface)
{
   PACKET pkt;

   LOCK_NET_RESOURCE(FREEQ_RESID);
   pkt = pk_alloc(4);  /* dummy packet */
   UNLOCK_NET_RESOURCE(FREEQ_RESID);

   if (pkt == NULL)
      return;

   nets[iface]->n_ipaddr = 0;    /* our IP address needs to be 0 */
   pkt->net = nets[iface];
   pkt->fhost = autoIPs[iface].try_address;
   send_arp(pkt, autoIPs[iface].try_address);
}



/* FUNCTION: AutoIp_pick_address()
 * 
 *   Pick a random auto IP address. Returns the address in network
 * endian (big), which is how the IP, ARP & DHCP modules expect them.
 *
 * PARAM1: void
 *
 * RETURNS: random IP address
 */

ip_addr
AutoIp_pick_address(void)
{
   unshort  newrand = rand_seed--;  /* bump this down since cticks goes up */

   newrand ^= (unshort)(cticks + 1);
   while(newrand > dAUTO_IP_RANGE)
      newrand >>= 1;
   return(htonl(dBASE_AUTO_IP_ADDRESS + newrand));
}


/* FUNCTION: AutoIp_get_state()
 * 
 * PARAM1: void
 *
 * RETURNS: 
 */

int
AutoIp_get_state(int iface)
{
   return(autoIPs[iface].state);
}


/* FUNCTION: AutoIp_set_net_vars()
 * 
 * PARAM1: int iface
 *
 * RETURNS: 
 */

void
AutoIp_set_net_vars(int iface)
{
   nets[iface]->n_ipaddr = autoIPs[iface].try_address;
   nets[iface]->snmask   = htonl(0xFFFF0000);
   nets[iface]->n_defgw  = 0x00000000;

   if ( nets[iface]->snmask == 0 )
   {
      fixup_subnet_mask(iface);
      dhc_states[iface].snmask = nets[iface]->snmask; 
   }

   /* fixup broadcast addresses  */
   nets[iface]->n_netbr    = nets[iface]->n_ipaddr | ~nets[iface]->snmask;
   nets[iface]->n_netbr42  = nets[iface]->n_ipaddr &  nets[iface]->snmask;
   nets[iface]->n_subnetbr = nets[iface]->n_ipaddr | ~nets[iface]->snmask;

   /* print IP address acquired through AutoIP - for user's benefit */
   printf("Acquired IP address via AutoIPv4 for interface: %s\n", nets[iface]->name); 
   printf("IP address : %s\n", print_ipad(nets[iface]->n_ipaddr));
   printf("Subnet Mask: %s\n", print_ipad(nets[iface]->snmask));
   printf("Gateway    : %s\n", print_ipad(nets[iface]->n_defgw));
}



/* FUNCTION: dhc_hostname()
 *
 * Called from the DHCP client to get a valid hostname for this machine. This
 * is a default routine which returns a dummy name. The porting engineer is
 * expected to define GETHOSTNAME_ALREADY and replace this with a system
 * routine.
 *
 * Note - if MUST always return a string.
 *
 * PARAM1: none
 *
 * RETURNS: pointer to ascii hostname string
 */

#ifndef DHCHOSTNAME_ALREADY

/* globally overridable hostname */
char * system_dhcp_hostname = "dummy";
 
u_char *
dhc_hostname()
{
   char * hostname;

#ifdef ALWAYS_HAVE_NAME
   hostname = get_device_name();
   if(strcmp(hostname,"") == 0)
   {
      hostname = get_device_always_name(); 
   }
#else
   hostname = system_dhcp_hostname;
#endif

   return (u_char*)hostname;
}

#endif   /* DHCHOSTNAME_ALREADY */

#endif   /* USE_AUTOIP */

