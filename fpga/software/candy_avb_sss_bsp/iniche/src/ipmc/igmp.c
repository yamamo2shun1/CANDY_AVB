/*
 * FILENAME: igmp.c
 *
 * Copyright 1997- 2000 By InterNiche Technologies Inc. All rights reserved
 *
 * This file contains code that is executed for all links
 * that are configured to run IGMPv1.
 */

/*
 * MODULE: IPMC
 *
 * ROUTINES: igmpv1_input()
 * PORTABLE: yes
 */

/* Additional Copyrights: */

/* This file contains the routines to implement the Internet Group Management
 * Protocol.(IGMP).
 *
 * Copyright (c) 1988 Stephen Deering.
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. [rescinded 22 July 1999]
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
 
#include "ipport.h"

#if defined (IP_MULTICAST) && defined (IGMP_V1)

#include "tcpport.h"

#include "../tcp/in_pcb.h"
#include "../tcp/protosw.h"

#include "igmp.h"
#include "igmp_cmn.h"

/* externs */
extern ip_addr igmp_all_hosts_group;
extern u_long igmp_timers_are_running;
extern struct igmp_stats igmpstats;

/* FUNCTION: igmpv1_input ()
 * 
 * This function processes incoming packets for all physical links
 * that are configured to run IGMPv1.  It is invoked from igmp_input ()
 * after the packet has passed validation tests.  Incoming packets can 
 * either be IGMPv1 Host Membership Queries or Reports.  Note that 
 * IGMP is only used to report membership in IPv4 multicast group
 * addresses.
 *
 * INPUT: Packet to be processed thru' the IGMPv1 state machine(s).
 *
 * OUTPUT: This function returns IGMP_ERR if it is passed an "unknown"
 * packet type.  Otherwise, it returns IGMP_OK.
 */

int igmpv1_input(PACKET p)
{
   struct igmp *  igmp;
   struct ip *    pip;
   struct in_multi * inm;
   NET netp  = p->net;
   int rc;
         
   pip = ip_head (p);
   igmp = (struct igmp *) (ip_data (pip));

   switch (igmp->igmp_type) 
   {
   case IGMP_HOST_MEMBERSHIP_QUERY:
      ++igmpstats.igmpv1mode_v1_queries_rcvd;
      /*
       * Start the timers in all of our membership records for
       * the interface on which the query arrived, except those
       * that are already running and those that belong to the
       * "all-hosts" group.
       */
      for (inm = netp->mc_list; inm; inm = inm->inm_next)
      {
         /* skip all IPv6 entries - they are indicated by 
          * an IPv4 address field of 0 */
         if (inm->inm_addr == 0)
            continue;
         /* skip IPv4 multicast address of 224.0.0.1 (note that
          * the IPv4 address stored in inm_addr is in network 
          * byte order */
         if (inm->inm_addr != igmp_all_hosts_group)
         {
            if (inm->inm_timer == 0)
            {
               inm->inm_timer = (unsigned) IGMP_RANDOM_DELAY(inm->inm_addr);
               /* increment the count of running timers */
               ++igmp_timers_are_running;            
            }   
         }
      }
      rc = IGMP_OK;
      break;

   case IGMP_HOST_MEMBERSHIP_REPORT:
      ++igmpstats.igmpv1mode_v1_reports_rcvd;
      /*
       * If we belong to the group being reported and have a 
       * running timer for that group, stop our timer for that 
       * group.
       */
      inm = lookup_mcast(igmp->igmp_group, netp);
      if (inm != NULL) 
      {
         if (inm->inm_timer > 0)
         {
            inm->inm_timer = 0;
            /* decrement the count of running timers */
            --igmp_timers_are_running;
            ++igmpstats.igmpv1mode_v1_reports_rcvd_canceled_timer;
         }
      }
      rc = IGMP_OK;
      break;
      
   default:
      ++igmpstats.igmpv1mode_unknown_pkttype;
      rc = IGMP_ERR;
      break;   
   }

   /* we're done with the received packet; return packet buffer back 
    * to free pool */
   LOCK_NET_RESOURCE(FREEQ_RESID);
   pk_free(p);
   UNLOCK_NET_RESOURCE(FREEQ_RESID);
      
   return rc;
}

#endif   /* IP_MULTICAST and IGMPv1 */

