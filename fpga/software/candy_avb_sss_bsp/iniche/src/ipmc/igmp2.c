/* igmp2.c 
 *
 * This file contains code that is executed for all links
 * that are configured to run IGMPv2.
 */

#include "ipport.h"

#if defined (IP_MULTICAST) && defined (IGMP_V2)

#include "tcpport.h"

#include "../tcp/in_pcb.h"
#include "../tcp/protosw.h"

#include "igmp.h"
#include "igmp_cmn.h"
#include "igmp2.h"

/* prototypes */
int igmpv2_input (PACKET p);
int igmpv2_process_report (PACKET p);
int igmpv2_process_query (PACKET p);
void igmpv2_chk_set_timer (struct in_multi * inm, u_short max_resp_time);
#ifdef IGMPv2_CHK_RTR_ALERT_OPT
u_char igmpv2_chk4_rtr_alert_opt (struct ip * pip);
#endif

/* externs */
extern ip_addr igmp_all_hosts_group;
extern ip_addr igmp_all_rtrs_group;
extern u_long igmp_timers_are_running;
extern struct igmp_stats igmpstats;


/* FUNCTION: igmpv2_input ()
 * 
 * This function processes incoming packets for all physical links
 * that are configured to run IGMPv2.  It is invoked from igmp_input ()
 * after the packet has passed validation tests.  The received
 * packet can be any of the following types - IGMPv1 Host Membership 
 * Query, IGMPv1 Host Membership Report, IGMPv2 Membership Query,
 * IGMPv2 Membership Report, or IGMPv2 Leave Group.  Note that 
 * IGMP is only used to report membership in IPv4 multicast group
 * addresses.
 *
 * INPUT: Packet to be processed thru' the IGMPv2 state machine(s).
 *
 * OUTPUT: This function can return any of the following three values:
 * (1) return code from igmpv2_process_query (), (2) return code
 * from igmpv2_process_report (), IGMP_OK (for a received Leave Group
 * message only), or IGMP_ERR (for a message of an "unknown" type).
 */

int igmpv2_input (PACKET p)
{
   struct igmp * igmp;
   struct ip * pip;
   u_char type;
   int rc;

   pip = ip_head (p);    
   igmp = (struct igmp *) (ip_data (pip));   
   /* extract the IGMP packet type from received packet */
   type = igmp->igmp_type;

   switch (type) 
   {
      case IGMP_HOST_MEMBERSHIP_QUERY:
         rc = igmpv2_process_query (p);
         break;

      case IGMP_HOST_MEMBERSHIP_REPORT:
      case IGMPv2_MEMBERSHIP_REPORT:
         rc = igmpv2_process_report (p);
         break;
         
      case IGMPv2_LEAVE_GROUP:
         /* Leave messages are typically addressed to the all-routers 
          * multicast address group (224.0.0.2).  We do not implement 
          * multicast router functionality, and therefore, do not 
          * expect to receive such messages.  However, according to
          * RFC 2236, some implementations of an older version of the 
          * IGMPv2 specification send leave messages to the group 
          * being left.  If we do receive such a message, we will 
          * drop it. */       
         ++igmpstats.igmpv2mode_v2_leave_msgs_rcvd;
         rc = IGMP_OK;
         break;               

      default:     
         ++igmpstats.igmpv2mode_unknown_pkttype;
         rc = IGMP_ERR;         
         break;
   } /* end SWITCH */

   /* we're done processing the received packet; return packet buffer 
    * back to free pool */
   LOCK_NET_RESOURCE(FREEQ_RESID);
   pk_free(p);
   UNLOCK_NET_RESOURCE(FREEQ_RESID);
   
   return rc;
}


/* FUNCTION: igmpv2_process_report ()
 * 
 * This function processes a received packet that is either an 
 * IGMPv1 Host Membership Report or a IGMPv2 Membership Report.
 *
 * INPUT: Report packet to be processed thru' the IGMPv2 state 
 * machine for a group whose membership is being reported.
 *
 * OUTPUT: This function always returns IGMP_OK.
 */

int igmpv2_process_report (PACKET p)
{
   struct igmp * igmp;
   struct ip * pip;
   NET netp;
   struct in_multi * inm;

   netp = p->net;
   pip = ip_head (p);
   igmp = (struct igmp *) (ip_data (pip));   
   
   /* If we receive a IGMP (v1 or v2) report for a multicast group 
    * that we have a timer running for, the IGMPv2 specification 
    * requires that we stop the timer (and thereby cancel the 
    * future transmission of our report).
    
    * However, we will use the following table to guide our actions
    * instead.  Scenario #4 causes us to not cancel the timer, 
    * since we have received a IGMPv2 report, but we believe that 
    * the querier on the network is running IGMPv1, and therefore 
    * will not be able to understand the IGMPv2 report.  As a 
    * result, we let our timer run, and if it expires, we will 
    * send out a report.

    * The type of a received report can be determined by examining 
    * the first byte of the IGMP message.  This byte is 0x12 for a 
    * IGMPv1 Host Membership Report, and 0x16 for a Version 2 
    * Membership Report.
    
    * Scenario# igmpv1_rtr_present Type of rcvd report Cancel timer
    * =============================================================
    * 1         No                 IGMPv1              Yes 
    * 2         No                 IGMPv2              Yes
    * 3         Yes                IGMPv1              Yes
    * 4         Yes                IGMPv2              No

    * In scenario #1 and #2, we have a IGMPv2-capable router that 
    * can understand both IGMPv1 and IGMPv2 reports.  In scenario 
    * #3, we have a IGMPv1-capable router that can understand IGMPv1 
    * reports. In scenario #4, we have a IGMPv1-capable router that
    * cannot understand a IGMPv2 report.  It is possible that the 
    * IGMPv1-capable router in scenario #4 is also capable of 
    * processing IGMPv2 packets (it has "downgraded" itself because
    * there are IGMPv1 routers on that network); however, we do not
    * know that, and hence we don't cancel our timer (for the 
    * subsequent transmission of a IGMPv1 report).
    */
   inm = lookup_mcast(igmp->igmp_group, netp);
   if (inm != NULL) 
   {
      if (inm->inm_timer != 0)
      {
         /* we have a timer running */
         if (!(netp->igmpv1_rtr_present && 
             igmp->igmp_type == IGMPv2_MEMBERSHIP_REPORT))
         {
            /* cancel timer */
            inm->inm_timer = 0;
            /* decrement the count of running timers */
            --igmp_timers_are_running;
            /* indicate that we are not the last host to send a 
             * report for this group */
            inm->last2send_report = IGMP_FALSE;
            ++igmpstats.igmpv2mode_v12_reports_rcvd_canceled_timer;
         }
      }
      else
      {
         /* we don't have a timer running; perhaps the source
          * host has just joined the group, and has sent an
          * unsolicited report */
         ++igmpstats.igmpv2mode_v12_reports_rcvd_no_timer;   
      }
   }
   else 
   {
      /* since the Reports are sent the group address, we should
       * never receive them unless we are a member of that group
       * on that interface.  Even if imperfect filtering at the 
       * device level causes reports for unregistered groups to 
       * be passed up to the IP module, ip_rcv_phase2 () is 
       * responsible for dropping them, and so we should never
       * receive such packets. */
      ++igmpstats.igmpv2mode_v12_unknown_grp_reports_rcvd;
   }
   
   return IGMP_OK;   
}


/* FUNCTION: igmpv2_process_query ()
 * 
 * This function processes a received packet that is either an 
 * IGMPv1 Host Membership Query, or an IGMPv2 General Query,
 * or an IGMPv2 Group-Specific Query.  IGMPv1 Host Membership 
 * Query and IGMPv2 General Query require processing thru' all
 * of the IGMPv2 state machines on a link.  The IGMPv2 Group-
 * Specific Query only requires processing thru' the IGMPv2
 * state machine for the group that is being queried.
 *
 * INPUT: Query packet to be processed thru' the IGMPv2 state 
 * machine(s).
 *
 * OUTPUT: This function always returns IGMP_OK.
 */

int igmpv2_process_query (PACKET p)
{
   struct igmp * igmp;
   struct ip * pip;
   NET netp;
   u_short max_resp_time;
   u_char process_all;
   struct in_multi * inm;

   netp = p->net;
   pip = ip_head (p);
   igmp = (struct igmp *) (ip_data (pip));

   if (igmp->igmp_code == 0)
   {
      /* this is a IGMPv1 Host Membership Query */
      netp->igmpv1_rtr_present = IGMP_TRUE;
      netp->igmpv1_query_rcvd_time = cticks;      
      ++igmpstats.igmpv2mode_v1_queries_rcvd;
      /* set maximum time to respond to the equivalent of 10 
       * seconds worth of "ticks" (the timeout routine is
       * intended to be invoked PR_FASTHZ (5) times a second,
       * so each tick is equal to 200 ms) */
      max_resp_time = IGMP_MAX_HOST_REPORT_DELAY * PR_FASTHZ;
      process_all = IGMP_TRUE;
   }
   else
   {
      /* this is either a IGMPv2 General Query or 
       * a IGMPv2 Group-Specific Query */
      if (igmp->igmp_group == 0)
      {
         /* this is a IGMPv2 General Query */
         ++igmpstats.igmpv2mode_v2_general_queries_rcvd;
         process_all = IGMP_TRUE;
      }
      else
      {
         /* this is a IGMPv2 Group-Specific Query */       
         ++igmpstats.igmpv2mode_v2_grp_specific_queries_rcvd;
         process_all = IGMP_FALSE;
      }
      
      /* irrespective of whether received message is a 
       * IGMPv2 General Query or a IGMPv2 Group-Specific Query,
       * set maximum time to respond to value extracted 
       * from received message. The value in the message
       * is in tenths of a second.  max_resp_time is in
       * units of ticks (where one tick is 200 ms) */
      max_resp_time = (igmp->igmp_code * PR_FASTHZ) / 10;
   }
   
   /* process all entries in a link's multicast address linked
    * list (pointed to by mc_list) as part of the response to
    * the received IGMPv1 Host Membership Query or IGMPv2 General
    * Query message */
   if (process_all)
   {
      for (inm = netp->mc_list; inm; inm = inm->inm_next)
      {
         /* skip all IPv6 entries - they are indicated by 
          * an IPv4 address field of 0 */
         if (!(inm->inm_addr)) continue;
         /* skip IPv4 multicast address of 224.0.0.1 (note that
          * the IPv4 address stored in inm_addr is in network 
          * byte order */
         if (inm->inm_addr != igmp_all_hosts_group)
            igmpv2_chk_set_timer (inm, max_resp_time);
      } /* end FOR (iterate thru' mc_list on net) */
   } /* end IF (process all) */
   else
   {
      /* process one (for IGMPv2 Group-Specific Query) entry (the 
       * one that corresponds to the address listed in the received 
       * query) - it should be present in the link's multicast
       * address list */
      inm = lookup_mcast(igmp->igmp_group, netp);
      if (inm != NULL)
         igmpv2_chk_set_timer (inm, max_resp_time);
      else ++igmpstats.igmpv2mode_v2_unknown_grp_specific_queries_rcvd;
   } /* end ELSE (process ALL) */
   
   /* return success; caller will the received packet back to the 
    * free pool */
   return IGMP_OK;
}


/* FUNCTION: igmpv2_chk_set_timer ()
 * 
 * This function checks to see if a timer needs to be
 * (re)started in response to a received Query message.
 * Unlike IGMPv1, IGMPv2 requires that a running timer be
 * restarted if the remaining time on the timer is greater
 * than the Max Response Time specified in the IGMPv2
 * General Query/Group-Specific Query message or 10
 * seconds for a IGMPv1 Host Membership Query message.
 *
 * INPUT: (1) Multicast address data structure that is being
 *            processed in igmpv2_process_query ().
 *        (2) Max Response Time (either extracted from the
 *            received IGMPv2 Query message, or 10 seconds
 *            for an IGMPv1 Query message), expressed in
 *            200 ms tick units.
 *
 * OUTPUT: None.
 */

void igmpv2_chk_set_timer (struct in_multi * inm, u_short max_resp_time)
{  
   
   /* restart a timer if this entry already has a timer running
    * but the scheduled expiry is later than what the router
    * router has indicated in the just received Query message.
    * Or, if a timer is not running at all, we start one.

    * Otherwise, the current timer for this group is scheduled 
    * to expire within the duration indicated in the Query 
    * message, so we let it continue. 
    */
   if ((inm->inm_timer > max_resp_time) ||
       (inm->inm_timer == 0))
   {
      if (inm->inm_timer == 0) ++igmp_timers_are_running;
      inm->inm_timer = (unsigned) IGMPv2_RANDOM_DELAY (max_resp_time, inm->inm_addr);                     
   }

   return;
}


/* FUNCTION: igmpv2_chk4_rtr_alert_opt ()
 * 
 * This function checks for the presence of the IP Router
 * Alert option in the received IGMPv2 packet.  It is
 * invoked from igmp_validate () (which, in turn, is
 * invoked from igmp_input ()) to validate the received
 * packet before it is fed into the IGMP state machine(s).
 *
 * INPUT: (1) Pointer to the start of the IP header of
 *            the received packet.
 *
 * OUTPUT: This function returns IGMP_TRUE if it finds
 *         the IP Router Alert option in the received
 *         packet, and IGMP_FALSE otherwise.
 */

u_char igmpv2_chk4_rtr_alert_opt (struct ip * pip)
{
   u_char * optp;
   u_long * rtr_alert_optp;
   u_char total_optlen;
   u_char optlen;
   u_char optval;

   total_optlen = ip_hlen (pip) - sizeof (struct ip);

   if (total_optlen > 0)
   {
      /* point to just past the end of the IP header */
      optp = (u_char *) (pip + 1);
  
      while (total_optlen > 0)
      {
         /* only the lowermost 5 bits are significant */    
         optval = (*optp) & IPOPT_TYPE_MASK;
         switch (optval)
         {
            case EOL_OPT:
               /* we've encountered the End of Option List option, 
                * and so setting optlen isn't necessary */
               optlen = 1;
               /* we're done - we couldn't locate the IP Router Alert 
                * option in this IP header */
               return IGMP_FALSE;
         
            case NOOP_OPT:
               /* skip past the one byte of the No Operation option */
               optlen = 1;
               break;
         
            case IP_RTR_ALERT_OPT:
               rtr_alert_optp = (u_long *) optp; 
               if ((ntohl (*rtr_alert_optp)) == IP_RTR_ALERT_OPT_DATA)
                  /* found the option, return success */
                  return IGMP_TRUE;
               else return IGMP_FALSE;
          
            default:
               /* extract the length of the current option, and compute
                * the total length of this option */
               optlen = (*(optp + 1)) + 2;
               break;
         }
         
         /* skip past the bytes associated with the current option to 
          * point to the next option. */
         optp += optlen;
         total_optlen -= optlen;
      } /* end WHILE */
   }

   /* didn't find IP Alert option in IP header of rcvd packet */
   return IGMP_FALSE;
}

#endif /* IP_MULTICAST and IGMPv2 */

