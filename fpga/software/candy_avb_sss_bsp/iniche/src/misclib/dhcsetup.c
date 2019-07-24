/*
 * FILENAME: dhcsetup.c
 *
 * Copyright 1997- 2000 By InterNiche Technologies Inc. All rights reserved
 *
 *  This file is a place holder for functionality on top of 
 * DHCP CLIENT. Presently it contains the DHCP CLIENT's setup 
 * function. It initializes the DHCP Client and waits untill the DHCP 
 * Client gets the IP Address.
 *
 * MODULE: MISCLIB
 *
 * ROUTINES: dhc_setup(), dhc_main_ipset()
 *
 * PORTABLE: yes
 */


#include "ipport.h"

#ifdef DHCP_CLIENT
#include "dhcpclnt.h"
#include "nvparms.h"    /* For nvparms struct */
#include "netbuf.h"     /* To remove warnings for net.h */
#include "net.h"        /* For nets[] */
#include "in_utils.h"   /* For netexit() */


int dhc_main_ipset(int iface, int state);



/* FUNCTION: dhc_setup()
 *
 * dhc_setup() : The setup function for DHCP Client. This function 
 * interfaces with the DHCP Client.
 * 1. It initializes the DHCP Client 
 * 2. For each interface,
 *    A. It checks if DHCP Client is needed to get the IP Address
 *    B. If yes, it initializes the DHCP Client for 
 *       that interface Also sets the callback.
 *    C. Then it calls dhc_second() once, so that DHCP Client's state
 *       machine gets a chance to start.
 * 3. Waits till DHCP Client activity completes on 
 *    all interfaces. Then it returns. If it doesn't conclude in 30 
 *    seconds, it uses the old IP addresses and returns. 
 *
 * 
 * PARAM1: void
 *
 * RETURNS: void
 */

void
dhc_setup(void)
{  
   int      iface;
   ulong    dhcp_started;
   ip_addr  dhcp_saveaddr[STATIC_NETS];
   int      e;
   int      dhcnets = 0;   /* number of nets doing DHCP */

   e = dhc_init();

   if (e)
   {
      printf("Error starting DHCP client code.\n");
      netexit(1);
   }

   dhcp_started = cticks;  /* init timeout */

   for (iface = 0; iface < STATIC_NETS; iface++)
   {
      if (!nets[iface])
         continue; /* no valid iface for this network */

#ifdef INCLUDE_NVPARMS
      if (inet_nvparms.ifs[iface].client_dhcp)
         nets[iface]->n_flags |= NF_DHCPC ; /* use DHCP Client on this iface */
#endif   /* INCLUDE_NVPARMS */

      if (!(nets[iface]->n_flags & NF_DHCPC))
         continue;

      /* If callback is not already in use (by AutoIP) grab it for
       * our printf routine.
       */
      if(dhc_states[iface].callback == NULL)
      {
         dhc_set_callback(iface, dhc_main_ipset);
      }

      /* start DHCP on the iface - first save the default address */
      dhcp_saveaddr[iface] = nets[iface]->n_ipaddr;

      if (dhcp_saveaddr[iface] == 0L)  /* see if there is a default */
         dhc_state_init(iface, TRUE);  /* Put DHCPClient in INIT state */
      else
         dhc_state_init(iface, FALSE); /* Put DHCPClient in INIT-REBOOT state */

      e=dhc_second();                  /* To send the DISCOVER/REQUEST pkt */
      if (e)
      {
         printf("Error sending DHCP packet on iface %d.\n", iface);
         netexit(1);
      }

#ifndef USE_AUTOIP
      /* If we are not using Auto IP then we want to block below waiting
       * for DHCP completion. If we are using Auto IP then we want to
       * return and let the Auto IP code handle the DHCP completion.
       * "dhcnets" is a flag which allows this.
       */
      dhcnets++;
#endif
   }

   if(dhcnets == 0)  /* no nets doing DHCP? */
      return;
      
   /* wait for DHCP activity to conclude */
   /* 
    * Altera Niche Stack Nios port modification:
    * Increase DHCP timeout to > 2 minutes 
    */
   while (((cticks - dhcp_started) < (130*TPS)) &&
      (dhc_alldone() == FALSE))
   {
      /* let other tasks spin. This is required, since some systems
       * increment cticks in tasks, or use a polling task to receive
       * packets. Without this activity this loop will never exit.
       */
      tk_yield();
      pktdemux();
#ifdef SUPERLOOP
      dhc_second ();
#endif
   }

   if (dhc_alldone() == FALSE)   /* dhcp timed out? */
   {
      dprintf("DHCP timed out, going back to default IP address(es)\n");
      /* go back to defaults */
      for (iface = 0; iface < STATIC_NETS; iface++)
      {
         if (dhc_ifacedone(iface) == FALSE)
         {
            dhc_halt(iface);
            nets[iface]->n_ipaddr = dhcp_saveaddr[iface];
         }
      }
   }
}


/* FUNCTION: dhc_main_ipset()
 * 
 * callback when DHCP completes OK
 *
 * PARAM1: int iface
 * PARAM2: int state
 *
 * RETURNS: 
 */

int
dhc_main_ipset(int iface, int state)
{
   if ( state == DHCS_BOUND )
   {
      /* print IP address acquired through DHCP Client - for user's benefit */
      printf("Acquired IP address via DHCP client for interface: %s\n",
              nets[iface]->name);

      printf("IP address : %s\n", print_ipad(nets[iface]->n_ipaddr));
      printf("Subnet Mask: %s\n", print_ipad(nets[iface]->snmask));
      printf("Gateway    : %s\n", print_ipad(nets[iface]->n_defgw));
   }
   return 0;
}

#endif   /* DHCP_CLIENT */

