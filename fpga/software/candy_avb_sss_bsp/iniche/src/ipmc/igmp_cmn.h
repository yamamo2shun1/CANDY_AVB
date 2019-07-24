/* igmp_cmn.h
 * 
 * Companion header file to igmp_cmn.c.
 */
 
#ifndef IGMP_CMN_H
#define IGMP_CMN_H

#ifdef IP_MULTICAST
 
/* each link has its own IGMP operating mode: it can
 * choose to run either IGMPv1 or IGMPv2.
 */ 
#define IGMP_MODE_V1 0x1
#define IGMP_MODE_V2 0x2

/* If the configuration file for the TCP/IP stack does 
 * not specify a particular IGMP protocol to run on a link, 
 * the software uses the following logic to select the
 * IGMP protocol on that link.
 */
#if defined (IGMP_V1) && !defined (IGMP_V2)
#define IGMP_MODE_DEFAULT IGMP_MODE_V1
#elif !defined (IGMP_V1) && defined (IGMP_V2)
#define IGMP_MODE_DEFAULT IGMP_MODE_V2
#elif defined (IGMP_V1) && defined (IGMP_V2)
#define IGMP_MODE_DEFAULT IGMP_MODE_V2
#endif

#define IGMP_TRUE 1
#define IGMP_FALSE 0

#define IGMP_ERR -1
#define IGMP_FAIL -1
#define IGMP_OK 0

/* length of IGMP message */
#define  IGMP_MINLEN 8
 
/*
IGMPv1 and IGMPv2 statistics data structure:

(1) igmp_total_rcvd: 
Total number of IGMP (v1 or v2) messages received.

(2) igmpv1mode_v1_queries_rcvd:
Total number of IGMPv1 Host Membership Queries received
by device on a link operating in IGMPv1 mode

(3) igmpv1mode_v1_reports_rcvd:
Total number of IGMPv1 Host Membership Reports received
by device on a link operating in IGMPv1 mode.

(4) igmpv1mode_v1_reports_rcvd_canceled_timer:
Total number of IGMPv1 Host Membership Reports received
(by device on a link operating in IGMPv1 mode) for a
group for which a timer was running. 

(5) igmpv2mode_v1_queries_rcvd: 
Number of IGMPv1 Host Membership Queries received by device
on a link operating in IGMPv2 mode.

(6) igmpv2mode_v2_general_queries_rcvd:
Number of IGMPv2 General Queries received by device on a link 
operating in IGMPv2 mode.

(7) igmpv2mode_v2_grp_specific_queries_rcvd:
Number of valid IGMPv2 Group-Specific Queries received by 
device on a link operating in IGMPv2 mode.

(8) igmpv2mode_v12_reports_rcvd_canceled_timer:
Number of IGMPv1/v2 Reports received that resulted in the
cancelation of a local timer.

(9) igmpv2mode_v12_reports_rcvd_no_timer:
Number of IGMPv1/v2 Reports received that did not have a
local timer running.

(10) igmpv2mode_v2_leave_msgs_rcvd:
Number of IGMPv2 Leave Group messages received by device on a
link operating in IGMPv2 mode.

(11) igmp_badlen_rcvd:
Total number of IGMP (v1 or v2) messages that do not 
have a length of 8 bytes.

(12) igmp_badsum_rcvd:
Total number of IGMP (v1 or v2) messages that do not 
have the correct checksum.

(13) igmp_pkt_alloc_fail:
Count of failures in allocating packet buffer for outgoing 
IGMPv1/v2 packet.

(14) igmp_bad_oper_mode:
Number of instances when a received packet could not be
forwarded to either of the IGMP protocol modules (v1 or
v2) due to an incorrect configuration of the IGMP operating
mode for the ingress link.

(15) igmp_bad_queries_rcvd:
Number of IGMPv1 Host Membership Query or a IGMPv2 General 
Query messages received that are not addressed to 224.0.0.1.

(16) igmp_bad_reports_rcvd:
Number of IGMPv1/v2 reports where the IP destination address
field is not identical to the IGMP group address or the
IGMP group address is not a multicast address

(17) igmpv1mode_unknown_pkttype:
Number of packets of unknown type (e.g, not IGMPv1 Host
Membership Query or IGMPv1 Host Membership Report) received 
by device on a link opertating in v1 mode.

(18) igmpv2mode_v2_bad_grp_specific_queries_rcvd:
Number of invalid IGMPv2 Group-Specific Queries (e.g, those
having a multicast group id of 224.0.0.1 or an invalid multicast 
address) received by device on a link operating in IGMPv2 mode.

(19) igmpv2mode_v2_unknown_grp_specific_queries_rcvd:
Number of IGMPv2 Group-Specific Queries received with a group
address that is not registered for the ingress interface.

(20) igmpv2mode_v12_unknown_grp_reports_rcvd:
Number of IGMPv1/v2 Membership Reports received with a group
address that is not registered for the ingress interface.

(21) igmpv2mode_v2_rtr_alert_missing:
Number of IGMPv2 General Query, IGMPv2 Group-Specific Query,
IGMPv2 Membership Report, or IGMPv2 Leave Group messages
received without the IP Router Alert option.

(22) igmpv2mode_unknown_pkttype:
Number of packets that are of a type other than a IGMPv1/v2 
Query, IGMPv1/v2 Report, or IGMPv2 Leave Group received by 
device on a link operating in IGMPv2 mode.

(23) igmp_v1_reports_sent:
Total number of IGMPv1 Host Membership Reports sent by 
device on a link operating in IGMPv1 or IGMPv2 mode.

(24) igmpv2mode_v2_leave_msgs_sent:
Number of IGMPv2 Leave Group messages sent by device on a 
link operating in IGMPv2 mode.

(25) igmpv2mode_v2_reports_sent:
Number of IGMPv2 Membership Report messages sent by device
on a link operating in IGMPv2 mode.
*/

struct igmp_stats {
/* rx-related statistics */
u_long igmp_total_rcvd;

u_long igmpv1mode_v1_queries_rcvd;
u_long igmpv1mode_v1_reports_rcvd;
u_long igmpv1mode_v1_reports_rcvd_canceled_timer;

u_long igmpv2mode_v1_queries_rcvd;
u_long igmpv2mode_v2_general_queries_rcvd;
u_long igmpv2mode_v2_grp_specific_queries_rcvd;
u_long igmpv2mode_v12_reports_rcvd_canceled_timer;
u_long igmpv2mode_v12_reports_rcvd_no_timer;
u_long igmpv2mode_v2_leave_msgs_rcvd;

/* error-related statistics */
u_long igmp_badlen_rcvd;
u_long igmp_badsum_rcvd;
u_long igmp_pkt_alloc_fail;
u_long igmp_bad_oper_mode;
u_long igmp_bad_queries_rcvd;
u_long igmp_bad_reports_rcvd;
u_long igmpv1mode_unknown_pkttype;
u_long igmpv2mode_v2_bad_grp_specific_queries_rcvd;
u_long igmpv2mode_v2_unknown_grp_specific_queries_rcvd;
u_long igmpv2mode_v12_unknown_grp_reports_rcvd;
u_long igmpv2mode_v2_rtr_alert_missing;
u_long igmpv2mode_unknown_pkttype;

/* tx-related statistics */
u_long igmp_v1_reports_sent;
u_long igmpv2mode_v2_leave_msgs_sent;
u_long igmpv2mode_v2_reports_sent;
};

#endif /* IGMP_CMN_H */

#endif /* IP_MULTICAST */

