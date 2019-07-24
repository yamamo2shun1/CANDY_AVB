/* igmp_cmn.c 
 *
 * This file contains all of the code that is common to IGMPv1 
 * and IGMPv2 protocol implementations.
 */

#include "ipport.h"

#if defined (IP_MULTICAST) && (defined (IGMP_V1) || defined (IGMP_V2))

#include "tcpport.h"

#include "../tcp/in_pcb.h"
#include "../tcp/protosw.h"

#include "igmp.h"
#include "igmp2.h"
#include "igmp_cmn.h"

/* number of running IGMP timers at any given instant */
u_long igmp_timers_are_running = 0;
/* time in ticks for the next invocation of IGMP timeout routine */
u_long igmp_cticks = 0;
/* IGMPv1 and v2 statistics data structure */
struct igmp_stats igmpstats;
/* IP address for all-hosts multicast address group (224.0.0.1) */
ip_addr igmp_all_hosts_group;
/* IP address for all-routers multicast address group (224.0.0.2) */
ip_addr igmp_all_rtrs_group;

/* prototypes */
void igmp_send (u_char type, struct in_multi * inm);
int igmp_validate (PACKET p);
int igmp_print_stats (void * pio);
void rip_input(PACKET p);

/* externs */
extern int igmpv1_input (PACKET p);
extern int igmpv2_input (PACKET p);
#ifdef IGMP_V2
extern u_char igmpv2_chk4_rtr_alert_opt (struct ip * pip);
#endif


/* FUNCTION: igmp_init ()
 * 
 * This function initializes the IGMP module.  It sets
 * up each link to operate in the appropriate mode
 * (IGMPv1 or IGMPv2).  It also initializes the count
 * of running IGMP timers.  (A IGMP timer is defined
 * as running of the inm_timer field of the owning 
 * multicast address data structure) is greater than zero.)  
 * It also sets up the IGMP timeout routine (igmp_fasttimo ())
 * to be invoked once every 200 ms.
 *
 * INPUT: None.
 *
 * OUTPUT: None.
 */

int igmp_init(void)
{
   NET ifp;

   /*
    * To avoid byte-swapping the same value over and over again.
    */
   igmp_all_hosts_group = htonl(INADDR_ALLHOSTS_GROUP);
   igmp_all_rtrs_group = htonl(INADDR_ALLRTRS_GROUP);

   /* note that the IGMP operational mode configuration for a
    * given link (i.e., whether it should run IGMPv1 or IGMPv2)
    * has already been validated, so no additional checks are 
    * required here. 
    */
   for (ifp = (NET) netlist.q_head; ifp; ifp = ifp->n_next)
   {
      if (ifp->igmp_oper_mode == IGMP_MODE_V1)
      {
         ifp->igmpv1_rtr_present = 1;
      }
      else
      {
         ifp->igmpv1_rtr_present = 0;
         /* not really required, only referred to if IGMPv1 router is 
          * "present" */
         ifp->igmpv1_query_rcvd_time = 0;
      }
   }

   /*
    * Call igmp_fasttimo PR_FASTHZ (5) times per second
    */
   igmp_cticks = cticks + TPS/PR_FASTHZ;

   /* there are no timers running initially */
   igmp_timers_are_running = 0;

   return IGMP_OK;
}


/* FUNCTION: igmp_input ()
 * 
 * This function is invoked by igmp_demux () to provide a
 * received IGMP packet to the IGMP module.  It first
 * validates the received packet via igmp_validate ().
 * If the validation succeeds, this function then hands
 * off the packet to the IGMPv1 or IGMPv2 protocol
 * modules for further processing.  The decision of which
 * protocol module to hand the packet off to is determined
 * by the operating mode that is configured for that link.
 *
 * The IGMPv1 and IGMPv2 protocol modules are responsible
 * for returning the received packet buffer back to the
 * free pool (after they have processed the packet).
 *
 * INPUT: Received packet.
 *
 * OUTPUT: This function can return any one of the following
 * four values: (1) return code from igmp_validate (), (2) 
 * return code from igmpv1_input (), (3) return code from 
 * igmpv2_input (), or (4) IGMP_ERR.  The latter value is 
 * returned if the operating mode is not correctly configured
 * to a valid IGMP operating mode.
 */

int igmp_input (PACKET p)
{
   u_char mode;
   int rc;
     
   ++igmpstats.igmp_total_rcvd;
   
   /* validate the received packet; if validation fails,
    * drop the packet and return */
   if ((rc = igmp_validate (p)) != IGMP_OK) goto end;

   /* determine the operating mode for IGMP on the ingress link */
   mode = p->net->igmp_oper_mode;
   
   /* feed packet to IGMPv1 or IGMPv2 code based on the operating
    * mode of the ingress link */
   switch (mode)
   {
#ifdef IGMP_V1   
      case IGMP_MODE_V1:   
         return (igmpv1_input (p)); 
#endif
#ifdef IGMP_V2         
      case IGMP_MODE_V2:       
         return (igmpv2_input (p));
#endif
      default:
         ++igmpstats.igmp_bad_oper_mode;     
         rc = IGMP_ERR;
         break;
   }
   
end:   
   /* return packet buffer back to free pool */
   LOCK_NET_RESOURCE(FREEQ_RESID);
   pk_free(p);
   UNLOCK_NET_RESOURCE(FREEQ_RESID);
      
   return rc;
}


/* FUNCTION: igmp_fasttimo ()
 * 
 * This function is invoked by inet_timer () to periodically 
 * service the IGMPv1 and IGMPv2 modules.  It traverses thru'
 * all of the interfaces in a system, and decrements all
 * running timers.  (A IGMP timer is defined as running if 
 * the inm_timer field of the owning multicast address data 
 * structure) is greater than zero.)  If the decremented timer 
 * reaches zero, this function sends out a IGMP report for 
 * the relevant multicast group.
 * 
 * If the link is running in IGMPv2 mode and also has a 
 * v1 router "present", this function checks to determine
 * if it has heard from the router in the last IGMPv1_RTR_PRESENT_TMO 
 * seconds.  If it hasn't, the v1 router is marked "absent".
 * Subsequent query requests from the v1 router will again
 * lead the v1 router to be marked as present on the link.
 *
 * Once a timer has expired, this function decrements the
 * count of running timers by one.  This function also sets
 * up the tick counter for its next invocation (by inet_timer ())
 * (which is 200 ms into the future). 
 *
 * INPUT: None.
 *
 * OUTPUT: None.
 */

void igmp_fasttimo (void)
{
   struct in_multi * inm;
   NET ifp;
     
   LOCK_NET_RESOURCE (NET_RESID);
   
   /*
    * Quick check to see if any work needs to be done, in order
    * to minimize the overhead of fasttimo processing.
    */
   if (!igmp_timers_are_running)
   {
      UNLOCK_NET_RESOURCE (NET_RESID);
      return;
   }

   for (ifp = (NET)(netlist.q_head); ifp; ifp = ifp->n_next)
   {
      for (inm = ifp->mc_list; inm; inm = inm->inm_next)
      {
         /* skip IPv6 entries */
         if (inm->inm_addr == 0) 
               continue;

         if (inm->inm_timer == 0)   /* timer not set */
         {
            /* do nothing */
         }
         else if (--inm->inm_timer == 0)  /* timer expired */
         {
            /* send membership report in appropriate format */
            if (ifp->igmpv1_rtr_present)
            {
               /* always true for IGMPv1, may be true for IGMPv2 */
               igmp_send (IGMP_HOST_MEMBERSHIP_REPORT, inm);
            }
            else
            {
               igmp_send (IGMPv2_MEMBERSHIP_REPORT, inm);
            }

            /* for IGMPv2, indicate that we were the last to send 
             * a Report for this multicast group (relevant for 
             * IGMPv2 only).  also check to see if we should mark 
             * the IGMPv1 router as "absent". */
            if (ifp->igmp_oper_mode == IGMP_MODE_V2)
            {
               inm->last2send_report = IGMP_TRUE;
               
               if (ifp->igmpv1_rtr_present)
               {
                  if (cticks > (ifp->igmpv1_query_rcvd_time + (IGMPv1_RTR_PRESENT_TMO * TPS)))
                  {
                     /* we haven't heard from the IGMPv1 router for a duration
                      * greater than or equal to Version 1 Router Present Timeout 
                      * (400 seconds), and will now update the igmpv1_rtr_present 
                      * variable to reflect that.
                      */
                     ifp->igmpv1_rtr_present = IGMP_FALSE;
                     ifp->igmpv1_query_rcvd_time = 0;
                  }
               }  
            }

            /* decrement the count of running IGMP timers */
            --igmp_timers_are_running;
         }
         else
         {
            /* timer is still counting down */
         }
      }     
   }

   /* Setup time for the next call into igmp_fasttimo ()
    * (200 ms later). */
   igmp_cticks = cticks + TPS/PR_FASTHZ;

   UNLOCK_NET_RESOURCE (NET_RESID);
   
   return;
}

/* FUNCTION: igmp_send ()
 * 
 * This function is used by the IGMPv1 and IGMPv2 protocol modules
 * to create and transmit a packet of a particular type.  IGMPv2
 * Membership Reports and Leave Group messages have the IP Router
 * Alert option appended to them.
 *
 * IGMPv2 Leave Group messages are addressed to the all-routers
 * multicast group; all other messages are addressed to the
 * relevant group itself.  The egress interface for the message
 * is the interface on which the multicast group address data
 * structure resides.
 *
 * When sending a packet that requires options in the IP header,
 * this function invokes ip_write2 () to transmit the outgoing
 * packet.  The third parameter to ip_write2 () is a list of
 * IP options.  The list is terminated by the EOL option.  If
 * the list of IP options only contains the EOL option, the
 * ip_write2 () function does not include any options in the
 * outgoing packet.  (Note that the stack does not "fully" 
 * support IP options.)
 *
 * INPUT: (1) Type of IGMP packet to be transmitted
 *        (2) Pointer to multicast group address structure for
 *            which IGMP message is being sent
 *
 * OUTPUT: None.
 */

void igmp_send (u_char type, struct in_multi * inm)
{
   PACKET p;
   struct igmp * igmp;
   struct ip_moptions * imop;
   struct ip_moptions simo;
   struct ip * pip;
   u_char * tmpp;
   u_char opts [2] = {IP_RTR_ALERT_OPT, EOL_OPT};
   u_char * optp;
   u_char reqd_len;

   /* compute length of buffer required for outgoing packet.
    * also account for the length of the IP Router Alert 
    * option, if required. */   
   reqd_len = MaxLnh + sizeof (struct ip) + sizeof (struct igmp);
   if ((type == IGMPv2_LEAVE_GROUP) || 
       (type == IGMPv2_MEMBERSHIP_REPORT))
   {
      reqd_len += IP_RTR_ALERT_OPT_SIZE;
   }

   /* obtain a packet to send the IGMP message */
   LOCK_NET_RESOURCE (FREEQ_RESID);
   p = pk_alloc (reqd_len);
   UNLOCK_NET_RESOURCE (FREEQ_RESID);
   
   /* log an error and return if the allocation fails */   
   if (!p)
   {
      ++igmpstats.igmp_pkt_alloc_fail;
      return;
   }

   /* Need to fill in the source and destination ip addresses */
   pip = (struct ip *) p->nb_prot;
   pip->ip_src = inm->inm_netp->n_ipaddr;
   /* Leave Group messages are sent to the all-routers multicast group */
   if (type == IGMPv2_LEAVE_GROUP)
   {
      /* igmp_all_rtrs_group is already in network byte order */
      pip->ip_dest = igmp_all_rtrs_group;
   }
   else
      pip->ip_dest = inm->inm_addr;
   
   p->fhost = pip->ip_dest;

   tmpp = (((u_char *) p->nb_prot) + sizeof (struct ip));

   /* when transmitting an IGMP packet, our IGMP module will insert
    * data for the Router Alert option in the following types of
    * packets: Version 2 Membership Report (0x16) and Leave Group 
    * (0x17) */
   if ((type == IGMPv2_LEAVE_GROUP) || 
       (type == IGMPv2_MEMBERSHIP_REPORT))
   {
      /* provide space for ip_write2 () to write option-related data */
      tmpp += IP_RTR_ALERT_OPT_SIZE;
      optp = &(opts [0]); /* one option (IP Router Alert) */
   }
   /* outgoing packet does not require any options */
   else 
      optp = &(opts [1]);
   /* point to the start of the IGMP header */
   igmp = (struct igmp *) tmpp;
   
   igmp->igmp_type = type;
   igmp->igmp_code = 0;
   
   /* all messages (Report or Leave) have Group Address field 
    * set to the group being reported or left */
   igmp->igmp_group = inm->inm_addr;
   igmp->igmp_cksum = 0;
   igmp->igmp_cksum = ~cksum((void*)igmp, IGMP_MINLEN>>1);

   imop = &simo;
   MEMSET(imop, 0, sizeof(simo));
   imop->imo_multicast_netp = inm->inm_netp;
   imop->imo_multicast_ttl = 1;
   /* we do not want our own reports to be looped back */
   imop->imo_multicast_loop = 0;

   /* set nb_prot to point to the beginning of the IGMP data,
    * and nb_plen to the length of the IGMP data, and attach
    * the multicast options structure to the outgoing packet */
   p->nb_prot = (char *) tmpp;
   p->nb_plen = sizeof(struct igmp);
   p->imo = imop;
   
   ip_write2 (IGMP_PROT, p, optp);

   if (type == IGMPv2_LEAVE_GROUP)
      ++igmpstats.igmpv2mode_v2_leave_msgs_sent;
   else if (type == IGMPv2_MEMBERSHIP_REPORT)
      ++igmpstats.igmpv2mode_v2_reports_sent;
   else if (type == IGMP_HOST_MEMBERSHIP_REPORT)
      ++igmpstats.igmp_v1_reports_sent;
}


/* FUNCTION: igmp_joingroup ()
 * 
 * This function is used to send the first IGMP Membership Report
 * (IGMPv1 or IGMPv2, as appropriate) for a group on the device.
 * Membership in the 224.0.0.1 group is not reported.  After
 * sending the report, this function also starts a timer that will
 * trigger a second transmission of the report (unless it is
 * supressed by a received report).
 *
 * INPUT: (1) Pointer to multicast group address structure for
 *            which IGMP Membership Report message is being sent
 *
 * OUTPUT: None.
 */
 
void igmp_joingroup(struct in_multi * inm)
{
   NET ifp;

   /* extract the network interface to which this multicast
    * address is "attached" */
   ifp = inm->inm_netp;

   if (inm->inm_addr == igmp_all_hosts_group)
   {
      inm->inm_timer = 0;
   }
   else 
   {
      /* send unsolicited membership report in appropriate format */
      if (ifp->igmpv1_rtr_present)
      {
         /* always true for IGMPv1, may be true for IGMPv2 */
         igmp_send (IGMP_HOST_MEMBERSHIP_REPORT, inm);
         /* set a delay timer (with a duration of 
          * IGMP_MAX_HOST_REPORT_DELAY) for a second unsolicited report */
         inm->inm_timer = (unsigned) IGMP_RANDOM_DELAY(inm->inm_addr);
      }
      else
      {
         igmp_send (IGMPv2_MEMBERSHIP_REPORT, inm);
         /* the delay time duration is the Unsolicited Report Interval */
         inm->inm_timer = (unsigned) IGMPv2_RANDOM_DELAY ((UNSOLIC_RPT_INTERVAL * PR_FASTHZ), inm->inm_addr);
      }
      
      /* for IGMPv2, indicate that we were the last to send 
       * a Report for this multicast group (relevant for 
       * IGMPv2 only). */
      if (ifp->igmp_oper_mode == IGMP_MODE_V2)
      {
         inm->last2send_report = IGMP_TRUE;
      }

      ++igmp_timers_are_running;
   }
   
   return;
}     


/* FUNCTION: igmp_leavegroup ()
 * 
 * This function is used to send the IGMPv2 Leave Group message.
 * for a group on the device.  This occurs when the reference
 * count for a group's membership becomes zero.  This function
 * will only send a Leave Group message when the link (on which
 * the multicast address group structure resides) is configured 
 * for IGMPv2 operation and has not been downgraded to IGMPv1
 * (because of the presence of a IGMPv1 router on the link).
 *
 * It is possible that we skip sending a Leave Group message 
 * even though we are the only member of a group.  Consider
 * the case where the host that last responded to a query has since
 * then left the group.  If we decide to leave immediately after
 * that, we will skip sending the leave message.  In such a
 * scenario, the Group Membership Interval timer in the IGMP
 * router will help the router determine when there are no
 * members of the group on that link.
 *
 * INPUT: (1) Pointer to multicast group address structure for
 *            which IGMPv2 Leave Group message is being sent.
 *
 * OUTPUT: None.
 */

void igmp_leavegroup (struct in_multi * inm)
{
   NET ifp;

   ifp = inm->inm_netp;

   if ((ifp->igmp_oper_mode == IGMP_MODE_V2) && 
       !ifp->igmpv1_rtr_present)
   {
      if (inm->last2send_report == IGMP_TRUE)
         igmp_send (IGMPv2_LEAVE_GROUP, inm);
   }
   
   return;
}


/* FUNCTION: igmp_validate ()
 * 
 * This function is used to validate the contents of a 
 * received packet.  It performs the following tests on
 * a received message.
 *
 * (a) validate the length of IGMP message
 * (b) validate the checksum
 * (c) validate that an IGMPv1 Host Membership Query or an  
 *     IGMPv2 General Query is sent to the all hosts-address
 * (d) validate that IGMPv2 Group-Specific Query carries
 *     a valid multicast group address, and is not addressed
 *     to 224.0.0.1 multicast group
 * (e) validate that the Group Address field in a received 
 *     IGMP Membership Report is identical to the destination
 *     address field in the IP header, and that the group
 *     address is a valid multicast group address
 * (f) validate that a received IGMPv2 General Query, IGMPv2 
 *     Group-Specific Query, IGMPv2 Membership Report, and 
 *     IGMPv2 Leave Group message includes the IP Router
 *     Alert option.
 *
 * Note.  This function does not free the packet even if
 * the validation fails.
 *
 * INPUT: (1) Received IGMP packet.
 *
 * OUTPUT: This function returns ENP_BAD_HEADER if the
 *         validation fails; otherwise, it returns
 *         IGMP_OK.
 */

int igmp_validate (PACKET p)
{
   struct ip * pip;
   int igmplen;
   struct igmp * igmp;
   u_short osum;
   u_short xsum;  
   u_char type;
   ip_addr mcgrp_addr;
   u_char resp_time;

   pip = ip_head (p);

   /* compute length of IGMP packet (after accounting for IP header, 
    * including the IP Router Alert option (if present)) */
   igmplen = p->nb_plen - ip_hlen (pip);

   /* validate length (IGMP_MINLEN is 8 bytes) */
   if (igmplen != IGMP_MINLEN) 
   {
      ++igmpstats.igmp_badlen_rcvd;
      return ENP_BAD_HEADER;
   }

   /* validate checksum */
   igmp = (struct igmp *) (ip_data (pip));
   osum = igmp->igmp_cksum;
   igmp->igmp_cksum = 0;
   xsum = ~cksum(igmp, igmplen>>1);
   if (xsum != osum)
   {
      igmp->igmp_cksum = osum;
      ++igmpstats.igmp_badsum_rcvd;
      return ENP_BAD_HEADER;
   }
   
   /* extract the IGMP packet type, Group Address, and Max Response Time 
    * (unused for IGMPv1) fields from received packet */
   type = igmp->igmp_type;
   mcgrp_addr = ntohl(igmp->igmp_group); 
   resp_time = igmp->igmp_code;
      
   if (type == IGMP_HOST_MEMBERSHIP_QUERY)
   {
      if ((resp_time == 0) || /* IGMPv1 Query */
          ((resp_time > 0) && (mcgrp_addr == 0))) /* IGMPv2 General Query */     
      {
         /* if this is a IGMPv1 Host Membership Query or a IGMPv2 
          * General Query, it must be addressed to the all-hosts 
          * group */
         if (pip->ip_dest != igmp_all_hosts_group) 
         {
            ++igmpstats.igmp_bad_queries_rcvd;
            return ENP_BAD_HEADER;
         }     
      }
      
      if ((resp_time > 0) && (mcgrp_addr != 0))
      {
         /* this is a IGMPv2 Group-Specific Query. */
         if (p->net->igmp_oper_mode == IGMP_MODE_V1)
         {
            /* IGMPv1 code does not understand a IGMPv2 Group-
             * Specific Query */
            return ENP_BAD_HEADER; 
         }
         
         /* check to make sure that the group address field carries
          * a valid multicast address; if it doesn't, we
          * drop the packet.  Also drop packets that
          * carry the multicast address for the all-hosts
          * group. */
         if ((!IN_MULTICAST(mcgrp_addr)) ||
             /* igmp_all_hosts_group is already in network byte order */
             (igmp->igmp_group == igmp_all_hosts_group))
         {
            ++igmpstats.igmpv2mode_v2_bad_grp_specific_queries_rcvd;
            /* caller will free received packet */
            return ENP_BAD_HEADER;
         }   
      }      
   }         
   
   /* check to ensure that a received IGMPv1 or v2 Report has the 
    * same IP host group address in its IP destination field and 
    * its IGMP group address field, and that the group address is
    * a valid multicast address */
   if ((type == IGMP_HOST_MEMBERSHIP_REPORT) ||
       (type == IGMPv2_MEMBERSHIP_REPORT))
   {
      if ((igmp->igmp_group != pip->ip_dest) ||
          (!IN_MULTICAST(mcgrp_addr)))
      {
         ++igmpstats.igmp_bad_reports_rcvd;
         return ENP_BAD_HEADER;    
      }
   }      

   /*
    * After receiving an IGMP packet, our IGMP module will check for the
    * presence of the Router Alert option in the following types of
    * packets.  Packets that do not have the option will be discarded.

    * (a) Version 2 General Query (0x11) (differentiated from a Version 1
          Host Membership Query by having a Max Response Time > 0)
    * (b) Version 2 Group-Specific Query (0x11)
    * (c) Version 2 Membership Report (0x16)
    * (d) Version 2 Leave Group (0x17)

    * Version 1 Host Membership Reports and Version 1 Host Membership Query
    * packets will not be checked for the IP Router Alert option.
    */
#ifdef IGMP_V2    
   if ((type == IGMPv2_LEAVE_GROUP) || 
       (type == IGMPv2_MEMBERSHIP_REPORT) ||
       ((type == IGMP_HOST_MEMBERSHIP_QUERY) && (igmp->igmp_code > 0)))
       
   {
      if (!igmpv2_chk4_rtr_alert_opt (pip))
      { 
         ++igmpstats.igmpv2mode_v2_rtr_alert_missing;
         return ENP_BAD_HEADER;
      }
   }
#endif   

   /* validation successful */
   return IGMP_OK;
}


/* FUNCTION: igmp_print_stats ()
 * 
 * This function is invoked by the CLI command parser in response
 * to the user's "igmp" command.  It prints out various statistics
 * related to the IGMPv1 and IGMPv2 modules.  Note that these
 * statistics are for the protocol module (and are across all
 * links).  The statistics are grouped into three categories:
 * receive-related, transmit-related, and error-related.
 *
 * INPUT: Pointer to I/O device where the output will be directed
 *        to.
 *
 * OUTPUT: This function always returns IGMP_OK.
 */
   
int igmp_print_stats (void * pio)
{  
   NET ifp;
  
   for (ifp = (NET)(netlist.q_head); ifp; ifp = ifp->n_next)
   {
      ns_printf (pio, "%s: mode: %u [%s] ", 
                 ifp->name, 
                 ifp->igmp_oper_mode, 
                 ((ifp->igmp_oper_mode == IGMP_MODE_V1)? "v1":"v2"));
      /* if a link has been configured for IGMPv2, display the status
       * of the v1 router-related variables too */
      if (ifp->igmp_oper_mode == IGMP_MODE_V2)
      {           
         ns_printf (pio, "v1 rtr: %u [%s] v1 last query: %x [now %x]\n",
                    ifp->igmpv1_rtr_present,
                    ((ifp->igmpv1_rtr_present == IGMP_TRUE)? "present" : "absent"),
                    ifp->igmpv1_query_rcvd_time,
                    cticks);
      }
      else ns_printf (pio, "\n");
   }   

   /* rx and timer statistics */
   ns_printf (pio, "[Rx ] IGMP messages rcvd: %lu, timers running: %lu\n", igmpstats.igmp_total_rcvd, igmp_timers_are_running);
   
   ns_printf (pio, "[Rx ] IGMPv1 Host Membership Queries rcvd (by v1-mode links): %lu\n", igmpstats.igmpv1mode_v1_queries_rcvd);
   ns_printf (pio, "[Rx ] IGMPv1 Host Membership Reports rcvd: %lu\n", igmpstats.igmpv1mode_v1_reports_rcvd);
   ns_printf (pio, "[Rx ] IGMP Host Membership Reports rcvd causing timer cancellation: %lu\n", igmpstats.igmpv1mode_v1_reports_rcvd_canceled_timer);
      
   ns_printf (pio, "[Rx ] IGMPv1 Host Membership Queries rcvd (by v2-mode links): %lu\n", igmpstats.igmpv2mode_v1_queries_rcvd);
   ns_printf (pio, "[Rx ] IGMPv2 General Queries rcvd: %lu, Group-Specific Queries rcvd: %lu\n", igmpstats.igmpv2mode_v2_general_queries_rcvd, igmpstats.igmpv2mode_v2_grp_specific_queries_rcvd);
   ns_printf (pio, "[Rx ] IGMP Host Membership Reports rcvd causing timer cancellation: %lu\n", igmpstats.igmpv2mode_v12_reports_rcvd_canceled_timer);   
   ns_printf (pio, "[Rx ] IGMP Host Membership Reports rcvd with no local timer: %lu\n", igmpstats.igmpv2mode_v12_reports_rcvd_no_timer);
   ns_printf (pio, "[Rx ] IGMPv2 Leave Group messages rcvd: %lu\n", igmpstats.igmpv2mode_v2_leave_msgs_rcvd);
   
   /* tx statistics */
   ns_printf (pio, "[Tx ] IGMPv2 Leave Group messages sent: %lu, Membership Reports sent: %lu\n", igmpstats.igmpv2mode_v2_leave_msgs_sent, igmpstats.igmpv2mode_v2_reports_sent);
   ns_printf (pio, "[Tx ] IGMPv1 Host Membership Reports sent: %lu\n", igmpstats.igmp_v1_reports_sent);

   /* error statistics */
   ns_printf (pio, "[Err] IGMP packets rcvd with bad length: %lu\n", igmpstats.igmp_badlen_rcvd);
   ns_printf (pio, "[Err] IGMP packets rcvd with bad checksum: %lu\n", igmpstats.igmp_badsum_rcvd);
   ns_printf (pio, "[Err] Packet buffer allocation failures: %lu, Bad IGMP Oper Mode config: %lu\n",igmpstats.igmp_pkt_alloc_fail, igmpstats.igmp_bad_oper_mode);
   ns_printf (pio, "[Err] Bad IGMP Queries rcvd: %lu, Bad IGMP Reports rcvd: %lu\n", igmpstats.igmp_bad_queries_rcvd, igmpstats.igmp_bad_reports_rcvd); 
   ns_printf (pio, "[Err] Bad IGMPv2 Group-Specific Queries rcvd: %lu\n", igmpstats.igmpv2mode_v2_bad_grp_specific_queries_rcvd);
   ns_printf (pio, "[Err] IGMPv2 Group-Specific Queries rcvd with unknown Group Address: %lu\n", igmpstats.igmpv2mode_v2_unknown_grp_specific_queries_rcvd);
   ns_printf (pio, "[Err] IGMP Membership Reports rcvd with unknown Group Address: %lu\n", igmpstats.igmpv2mode_v12_unknown_grp_reports_rcvd);
   ns_printf (pio, "[Err] Number of IGMPv2 messages rcvd without Router Alert option: %lu\n", igmpstats.igmpv2mode_v2_rtr_alert_missing); 
   ns_printf (pio, "[Err] IGMP packets of unknown type rcvd by v1-mode links: %lu\n", igmpstats.igmpv1mode_unknown_pkttype);
   ns_printf (pio, "[Err] IGMP packets of unknown type rcvd by v2-mode links: %lu\n", igmpstats.igmpv2mode_unknown_pkttype);
   
   return IGMP_OK;
}

#endif /* IP_MULTICAST and (IGMPv1 or IGMPv2) */
