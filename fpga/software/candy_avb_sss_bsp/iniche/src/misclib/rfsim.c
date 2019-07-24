/*
 * FILENAME: rfsim.c
 *
 * Copyright  2001 By InterNiche Technologies Inc. All rights reserved
 *
 * Code to simulate wireless networks by adding a controlled latency and
 * loss rate to routed packets. Target startup code should call sim_init()
 * to ready this for use.
 *
 * So far this has only been used on MPC860 builds, using the FADS 860T
 * with two 10Mbs ethernets.
 *
 * MODULE: MISCLIB
 *
 * PORTABLE: In Theory
 */


#include "ipport.h"


#ifdef RF_SIMULATION

#ifndef INICHE_TIMERS
#error rfsim requires INICHE_TIMERS
#endif


#include "q.h"
#include "netbuf.h"
#include "net.h"
#include "ip.h"
#include "menu.h"

int      rfsim_routing;   /* Bool, TRUE if we're doing this */
u_long   sim_latency;      /* Ave time to hold pkt, in TPS.  */

static int  lossrate;      /* loss will be 1 in rate pkts */
static int  deviation;     /* deviation in loss rate */

u_long simpkts;
u_long simdrops;
u_long simlastloss;

/* The rfsim menu routines */
int   simstart(void *);
int   simquit(void *);
int   simtime(void *);
int   simstat(void *);
int   simloss(void *);
void  rfsim_timer(void);

struct menu_op sim_menu[] = {
   "rfsim",   stooges,    "RF simulation",      /* menu ID */
   "simstart", simstart,   "start the routing delay",
   "simquit",  simquit,    "stop the routing delay",
   "simtime",  simtime,    "set the sim time (in ms)",
   "simloss",  simloss,    "set/clear sim loss rate (1 in n)",
   "simdump",  simstat,    "dump sim stats",
   NULL, NULL, NULL
};

long  simtimer;
queue simq;

/* sim_init(0 - called to set up the rfsim system */

int
sim_init()
{
   int err;
   err = install_menu(sim_menu);
   if(err)
      return err;

   return 0;
}

/* rfsim_send() - called from IP routing to send packets when
 * the global rfsim_routing flag is TRUE.
 */

int
rfsim_send(void * vpkt, u_long firsthop)
{
   PACKET pkt;

   pkt = (PACKET)vpkt;
   
   /* set time to eventually send the pkt */
   pkt->nb_tstamp = cticks + sim_latency;

   /* hijack the fhost field to hold the IP address */
   pkt->fhost = (ip_addr)firsthop;
   putq(&simq, (qp)pkt);
   return 0;
}

void
rfsim_timer()
{
   ip_addr     hop;
   struct ip * pip;
   PACKET      pkt;

   /* send al packets which have timed out */
   while(((PACKET)(simq.q_head))->nb_tstamp < cticks)
   {
      pkt = (PACKET)getq(&simq); /* get pkt to send */
      hop = pkt->fhost;          /* get hop from host field */
      pip = ip_head(pkt);        /* get real host address */
      pkt->fhost = pip->ip_dest; /* restore host field */
      simpkts++;

      /* see if it's time to drop a packet */
      if((lossrate) && 
         ((simpkts - simlastloss) > lossrate) && 
         ((cticks & deviation) == 1))
      {
         LOCK_NET_RESOURCE(FREEQ_RESID);
         pk_free(pkt);           /* drop instead of send */
         UNLOCK_NET_RESOURCE(FREEQ_RESID);
         simlastloss = simpkts;  /* reset drop timer */
         simdrops++;             /* count drops */
      }
      else
         ip2mac(pkt, hop);          /* send pkt to hardware */
   }
   return;
}

int
simstart(void * pio)
{
   simtimer = in_timerset(rfsim_timer, 20);
   if(simtimer == 0)
      return ENP_RESOURCE;

   ns_printf(pio, "rfsim slowdown active, latency is %d ticks.\n",
      sim_latency);
   rfsim_routing = TRUE;
   simpkts = simlastloss = simdrops = 0;
   return 0;
}


int
simquit(void * pio)
{
   PACKET   pkt;
   ip_addr  hop;
   struct ip * pip;
   
   if(simtimer)
   {
      in_timerkill(simtimer);
      simtimer = 0;
   }

   /* send all queued packets */
   while(simq.q_head)
   {
      pkt = (PACKET)getq(&simq); /* get pkt to send */
      hop = pkt->fhost;          /* get hop from host field */
      pip = ip_head(pkt);        /* get real host address */
      pkt->fhost = pip->ip_dest; /* restore host field */
      ip2mac(pkt, hop);          /* send pkt to hardware */
   }
   rfsim_routing = FALSE;
   ns_printf(pio, "rfsim stopped\n");
   return 0;
}


int
simtime(void * pio)
{
   char *   arg;
   long     msecs;

   arg = nextarg(((GEN_IO)pio)->inbuf);
   msecs = atol(arg);
   if(msecs == 0)
   {
      ns_printf(pio, "To set latency to 0 just shut sim off\n");
      return 0;
   }
   sim_latency = (msecs * TPS)/1000;
   ns_printf(pio, "SIM Latency now %d ms. (%d ticks)\n", msecs, sim_latency);

   return 0;
}

int
simstat(void * pio)
{
   ns_printf(pio, "rfsim %sactive, latency is %d ticks.\n",
      rfsim_routing?"":"in", sim_latency);

   ns_printf(pio, "rfsim queue; current:%ld, max:%ld\n",
      simq.q_len, simq.q_max);

   ns_printf(pio, "pkts delayed: %ld, dropped: %ld,  drop rate: %d in %d\n",
      simpkts, simdrops, lossrate?1:0, lossrate);

   return 0;
}


int
simloss(void * pio)
{
   char *   arg;
   int      rate;

   arg = nextarg(((GEN_IO)pio)->inbuf);
   if(arg == NULL || *arg == 0)
   {
      ns_printf(pio, "lossrate is %d in %d; enter new rate (1 in 'rate') to change\n",
         lossrate?0:1, lossrate);
      return 0;
   }
   rate = atoi(arg);

   if(rate == 0 && *arg != '0')
   {
      ns_printf(pio, "Please enter a number.\n");
      return -1;
   }
   lossrate = rate;     /* set rate of loss */

   /* set deviation mask for loss */
   if(lossrate < 11)
      deviation = 0x01;
   else if(lossrate < 30)
      deviation = 0x03;
   else if(lossrate < 101)
      deviation = 0x07;
   else if(lossrate < 1000)
      deviation = 0x01f;

   ns_printf(pio, "lossrate set to 1 in %d packets\n",
      lossrate);

   return 0;
}

#endif   /* RF_SIMULATION */

