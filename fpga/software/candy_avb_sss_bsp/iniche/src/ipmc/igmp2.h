/* igmp2.h
 *
 * Companion header file for igmp2.c.
 */

#ifndef IGMP2_H
#define IGMP2_H

#ifdef IP_MULTICAST

/* IGMPv2 message types */
#define  IGMPv2_MEMBERSHIP_REPORT      0x16
#define  IGMPv2_LEAVE_GROUP            0x17

/* compute a random delay; the first parameter is the Maximum
 * Response Time (expressed in units of 200 ms ticks) extracted 
 * from the received Query packet 
 */
#define IGMPv2_RANDOM_DELAY(mrt,multiaddr) \
   ( \
    (ip_mib.ipInReceives + ntohl(nets[0]->n_ipaddr) + ntohl((multiaddr))) \
    % mrt + 1 \
   )

/* Version 1 Router Present Timeout (400 seconds) */
#define IGMPv1_RTR_PRESENT_TMO 400

/* all-routers multicast group address (224.0.0.2) */
#define INADDR_ALLRTRS_GROUP 0xe0000002

/* maximum delay when setting a timer for a subsequent 
 * transmission of an unsolicited IGMPv2 Membership
 * Report. */
#define UNSOLIC_RPT_INTERVAL 10 /* 10 seconds */

#endif /* IGMP2_H */

#endif /* IP_MULTICAST */

