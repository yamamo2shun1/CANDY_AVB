/*
 * FILENAME: dhcpclnt.h
 *
 * Copyright 1997- 2000 By InterNiche Technologies Inc. All rights reserved
 *
 * dhcpclnt.h - was bootp.h, and then dhcp.h, and the IETF commitees aren't
 * through with this poor protocol yet...
 *
 * MODULE: INET
 *
 * PORTABLE: yes
 */

/* Additional Copyrights: */
/* Portions Copyright 1995-1996 by NetPort Software. */


#ifndef _DHCPCLNT_H
#define  _DHCPCLNT_H 1

/* List of enabled DHCP CLIENT options */
#define  DHCP_REQLIST   1

/* List of disabled DHCP CLIENT options */
#ifdef NOTDEF
#define  DHC_MAXDNSRVS  2 
#endif

#ifdef USE_AUTOIP /* AutoIP requires DNS option */
#define  DHC_MAXDNSRVS  2 
#endif   /* USE_AUTOIP */

#define  BOOTP_SERVER_PORT    67
#define  BOOTP_CLIENT_PORT    68

/* DHCP packet types; from options field of bootp header */
#define  DHCP_INVALIDOP 99
#define  DHCP_DISCOVER  1
#define  DHCP_OFFER     2
#define  DHCP_REQUEST   3
#define  DHCP_DECLINE   4
#define  DHCP_ACK       5
#define  DHCP_NAK       6
#define  DHCP_RELEASE   7

/* DHCP options we support: */

#define  DHOP_PAD       0     /* single char of padding, always 0 */
#define  DHOP_SNMASK    1     /* client's subnet mask */
#define  DHOP_ROUTER    3     /* set default router */
#define  DHOP_DNSRV     6     /* IP address of domain name server */
#define  DHOP_NAME      12    /* name (maybe DNS-ish) of host */
#define  DHOP_DOMAIN    15    /* name of host's domain */

#define  DHOP_CADDR     50    /* client requested IP address */
#define  DHOP_LEASE     51    /* lease time */
#define  DHOP_OVLD      52    /* options are in sname and file fields too */
#define  DHOP_TYPE      53    /* DHCP type of pkt; 1-7 above */
#define  DHOP_SERVER    54    /* server ID (IP address) */
#define  DHOP_REQLIST   55    /* client's parameter request list */
#define  DHOP_MSG       56    /* message text explaining NAK or DECLINE */
#define  DHOP_MAXSIZE   57    /* maximum DHCP message size */
#define  DHOP_RENEWAL   58    /* renewal (T1) time */
#define  DHOP_REBINDING 59    /* rebinding (T2) time */
#define  DHOP_CLIENT    61    /* client ID (i.e. hardware address) */
#define  DHOP_END       255   /* marks end of valid options */

/* DHCP Client per-net states (for dhc_state). The states are as per 
 * the description in RFC2131. DHCS_RESTARTING is a transient state. 
 * It is just used to inform the application layer(user) that DHCP 
 * Client is restarting the negotiation. Following is an example
 * 
 * 1. Client sends a REQUEST (it might be in DHCS_REBOOTING or 
 *    DHCS_REQUESTING state).
 * 2. Client doesn't receive a ACK/NAK. It retransmits
 * 3. After few retransmits (as per RFC2131), the client needs to move back
 *    to DHCS_INIT state. At this time, the user needs to be informed 
 *    that the client is restarting. 
 * 4. The Client calls the callback with state as DHCS_RESTARTING.
 * 5. The Client moves to DHCS_INIT state.
 * 
 * There is another example where DHCS_RESTARTING is used.
 * 1. Client is in DHCS_BOUND state. At the required time (when lease timer
 *    t1 expires), it will move to DHCS_RENEWING state. It sends a REQUEST
 *    to renew the lease
 * 2. No ACK/NAK is received in DHCS_RENEWING state. When lease timer t2 
 *    expires, the Client moves to DHCS_REBINDING state, and sends a
 *    REQUEST to rebind the current IP address.
 * 3. If no ACK/NAK comes and the lease expires, then the client should
 *    stop all network processing and request network reinitialization.
 *    That is, the IP address of the interface should be cleared and
 *    Client should move to DHCS_INIT state. At this time, the user needs
 *    to be informed that the client is restarting. 
 * 4. The Client calls the callback with state as DHCS_RESTARTING.
 * 5. The Client moves to DHCS_INIT state.
 * 
 */

#define  DHCS_UNUSED       0  /* no discovery attempted */
#define  DHCS_INIT         1  /* Ready to send a DISCOVER packet */
#define  DHCS_INITREBOOT   2  /* Have IP, ready to send REQUEST(skip DISCOVER)*/
#define  DHCS_REBOOTING    3  /* rebooting/reclaiming address */
#define  DHCS_SELECTING    4  /* discovery sent, but no offer yet */
#define  DHCS_REQUESTING   5  /* sent request; waiting for ACK|NAK */
#define  DHCS_BOUND        6  /* got a ACK we liked */
#define  DHCS_RENEWING     7  /* Renewing the lease */
#define  DHCS_REBINDING    8  /* rebinding to new server */
#define  DHCS_RESTARTING   9  /* Temp. state. Only to inform callback() */

/* macro for writing IP address options to the options buffer. 
Assumes the IP address is in local endian. */
#if(BYTE_ORDER == BIG_ENDIAN)
#define PUT_IP_OPT(ptr, code, ip)  {   \
   *ptr++ = code;   \
   *ptr++ = 4;      \
   *(ptr++) = *(char*)(&ip);   \
   *(ptr++) = *(((char*)(&ip))+1);   \
   *(ptr++) = *(((char*)(&ip))+2);   \
   *(ptr++) = *(((char*)(&ip))+3);   \
   }
#else
#define PUT_IP_OPT(ptr, code, ip)  {   \
   *ptr++ = code;   \
   *ptr++ = 4;      \
   *(ptr+3) = *(char*)(&ip);   \
   *(ptr+2) = *(((char*)(&ip))+1);   \
   *(ptr+1) = *(((char*)(&ip))+2);   \
   *(ptr) = *(((char*)(&ip))+3);   \
   ptr += 4;   \
   }
#endif

/* macro for writing string options to the options buffer */
#define PUT_STRING_OPT(ptr, code, string)  {   int len;\
   *ptr++ = code;      \
   len = strlen(string);   \
   *ptr++ = (u_char)len;      \
   strncpy((char*)ptr, (char*)string, len);   \
   ptr += len;   \
   }

/* macro to extract IP option. extracted address is in NET endian */
#define EXTRACT_IP_OPT(ptr, ip) { \
   ptr++;   \
   (ip) = *(long*)(ptr);   \
   }

#define  BOOTREQUEST    1     /* packet op codes */
#define  BOOTREPLY      2

/* DHCP hardware typres, from ARP section of RFC1700 */
#define  ETHHWTYPE      1     /* ethernet hardware type */
#define  LINEHWTYPE     20    /* Serial line hardware type */

#define  RFC1084_MAGIC_COOKIE htonl(0x63825363)
#define  RFC1084_PAD          ((unsigned  char) 0)
#define  RFC1084_SUBNET_MASK  ((unsigned  char) 1)
#define  RFC1084_GATEWAY      ((unsigned  char) 3)
#define  RFC1084_END          ((unsigned  char) 255)

#define  WHITESPACE  "\n\t "

#define  ADDR_ERR_STRING      "Invalid ethernet address"

#define  xtoi(x)     ((x>='a')&&(x<='f')?(x-'a'+10):((x>='A')&&(x<=  'F')?(x-'A'+10):(x-'0')))    

#define  OP       0
#define  HTYPE    1
#define  HLEN     2
#define  HOPS     3
#define  CIADDR   12
#define  YIADDR   16                  
#define  SIADDR   20                  
#define  CHADDR   28

#define  BOOTP_OPTSIZE     64 /* older value */
#define  DHCP_OPTSIZE   312   /* newer value */

/* bit we mask into the op field of received packets internally. 
 * This is set if the packet is DHCP (as opposed to plain bootp)
 */
#define  ISDHCP      0x04
#define  isdhcp(bp)  (bp->op  &  ISDHCP)


/* The structure of a bootp/dhcp packet. This is the
 * UDP data area of a bootp or dhcp packet.
 */

struct bootp   {
   unsigned char     op;
   unsigned char     htype;
   unsigned char     hlen;
   unsigned char     hops;
   unsigned long     xid;
   unsigned short    secs;
   unsigned short    flags;
   unsigned long     ciaddr;
   unsigned long     yiaddr;
   unsigned long     siaddr;
   unsigned long     giaddr;
   unsigned char     chaddr[16];
   unsigned char     sname[64];
   unsigned char     file[128];
   unsigned char     options[BOOTP_OPTSIZE]; /* was "vend" for bootp */
};

/* define sizes for both old bootppackets and newer dhcop packets. 
 * Need to be aware the Windows95 and other broken systems send DHCP 
 * packets in the bootp size, so you need to check both size and 
 * options to 
 */
#define  BOOTPSIZE   (sizeof(struct bootp))
#define  DHCPSIZE    (BOOTPSIZE  +  (DHCP_OPTSIZE-BOOTP_OPTSIZE))

/* dhcp utility routines */
u_char * find_opt(u_char opcode, u_char * opts);

/* DHCP client per-interface state: */
struct dhc_state  {
   unsigned state;         /* one of the "DHCS" states below */
   int      tries;         /* retry count of current state */
   u_long   xid;           /* last xid sent */
   u_short  secs;          /* seconds since client came up */
   u_long   last_tick;     /* time of last DHCP packet */
   u_long   lease;         /* lease; only valid if state == DHCS_BOUND */
   u_long   t1 ;           /* lease related - renew timer */
   u_long   t2 ;           /* lease related - rebind timer */
   u_long   lease_start;   /* time  in cticks   of when  lease started */

   /* Configuration  options  of outstanding request */
   ip_addr  ipaddr;        /* IP address */
   ip_addr  snmask;        /* subnet mask */
   ip_addr  defgw;         /* default gateway (router) */

   ip_addr  rly_ipaddr;    /* IP addr of our RELAY agent (if any) */

   /* IP Address of the DHCP Server.
    * Needed   to send  unicast  when  renewing IP Addr
    */
   ip_addr  srv_ipaddr;

#if defined(DHC_MAXDNSRVS) && (DHC_MAXDNSRVS > 0)
   ip_addr  dnsrv[DHC_MAXDNSRVS];   /* domain name server addresses */
#endif   /* DHC_MAXDNSRVS */

   int   (*callback)(int iface, int state);  /* callback when IPaddress set */
};

extern   struct dhc_state  dhc_states[MAXNETS]; /* DHCP client state of each net*/

/* DHCP client entry points */
int      dhc_init        (void);
int      dhc_second      (void);
int      dhc_alldone     (void);
int      dhc_ifacedone   (int iface);
void     dhc_state_init  (int iface, int init_flag);
void     dhc_halt        (int iface);
void     dhc_set_callback(int iface, int (*routine)(int,int) );
int      dhc_stats       (void *pio);


#endif   /* _DHCPCLNT_H */



