/*
 * FILENAME: dhcpclnt.c
 *
 * Copyright 1997- 2000 By InterNiche Technologies Inc. All rights reserved
 *
 * DHCP Client code for InterNiche IP stack.
 *
 * MODULE: INET
 *
 * ROUTINES: dhc_get_srv_ipaddr(), dhc_init(), 
 * ROUTINES: dhc_set_callback(), dhc_upcall(), dhc_buildheader(), 
 * ROUTINES: dhc_discover(), dhc_rx_offer(), dhc_request(), dhc_setip(), 
 * ROUTINES: dhc_resetip(), dhc_decline(), dh_getlong(), dhc_extract_opts(), 
 * ROUTINES: dhc_stats(), dhc_second(), dhc_halt(), dhc_reclaim(), 
 * ROUTINES: dhc_state_init(), dhc_alldone(), dhc_ifacedone(), 
 * ROUTINES: dhc_set_state(),
 *
 * PORTABLE: yes
 */

/* Additional Copyrights: */
/* Portions Copyright 1996 by NetPort Software. */

#include "ipport.h"

#ifdef DHCP_CLIENT   /* ifdef out whole file */

#include "libport.h"
#include "q.h"
#include "netbuf.h"
#include "net.h"
#include "ip.h"
#include "udp.h"
#include "dhcpclnt.h"

/* A lot of the following variables are named similarly to variables 
 * of similar function in the dhcp server code. n general the 
 * difference is that the client names start with "dhc_" and the 
 * server versions 
 */
UDPCONN dhc_conn = NULL;

long     xids  =  0x22334455;    /* seed for unique request IDs */

/* DHCP client statistics: */
ulong      dsc_errors    = 0; /* protocol/implementation runtime errors */
ulong      dsc_discovers = 0; /* discovers sent */
ulong      dsc_offers    = 0; /* offers recieved */
ulong      dsc_requests  = 0; /* requests sent */
ulong      dsc_acks      = 0; /* acks received */
ulong      dsc_bpreplys  = 0; /* plain bootp replys received */
ulong      dsc_declines  = 0; /* declines sent */
ulong      dsc_releases  = 0; /* releases sent */
ulong      dsc_naks      = 0; /* naks received */
ulong      dsc_renew     = 0; /* unicast requests sent to renew lease */
ulong      dsc_rebind    = 0; /* broadcast requests sent to renew lease */
ulong      dsc_rlyerrs   = 0; /* runtime errors due to DHCP Relay agents */

/* reqlist contains the list of options to be requested in a DISCOVER 
 * pkt. reqlist is used only if DHCP_REQLIST is enabled. 
 */
#ifdef DHCP_REQLIST
u_char   reqlist[]   =  {  DHOP_SNMASK,DHOP_ROUTER,   DHOP_DNSRV, DHOP_DOMAIN }  ;

/* u_char reqlist[] = { DHOP_SNMASK,DHOP_ROUTER, DHOP_DNSRV, 
 * DHOP_DOMAIN,0x2c,0x2e,0x2f }; 
 */
int      reqlist_len = sizeof(reqlist)/sizeof(u_char);
#endif   /* DHCP_REQLIST */

struct dhc_state  dhc_states[MAXNETS]; /* DHCP client state of each net. */

#ifdef NET_STATS
static char *dhc_state_str[] = 
{
   "unused",
   "init",
   "init-reboot",
   "rebooting",
   "selecting",
   "requesting",
   "bound",
   "renewing",
   "rebinding",
   "restarting",
};
#endif

#define  DHC_BCASTFLAG  0x8000            /* for the flags field of pkt */
#define  DHC_INFINITY   0xffffffff        /* That is, "-1" */
#define  DHC_MAX_TRIES  4                 /* Max num of retires to tbe done */
#define  DHC_RETRY_TMO  4                 /* Timeout(secs) for retries */
#define  DHCPDATA       ((void*)0xFFFFFFFD)  /* tag for pass to udp_open() */

/* DHCP functions used within this file */
int      dhc_upcall      (PACKET pkt, void * data); 
int      dhc_discover    (int iface);    /* send a dhcp discover packet */
int      dhc_reclaim     (int iface);    /* try to reclaim previous address */
int      dhc_setip       (int iface);    /* set iface's IP addr */
int      dhc_resetip     (int iface);    /* reset iface's IP addr */
int      dhc_request     (int iface, int xid_flag); /* Send a request */
int      dhc_rx_offer    (int iface, struct bootp * bp, unsigned bplen);
int      dhc_decline     (int iface, struct bootp * bp, unsigned bplen);
int      dhc_buildheader (int iface, struct bootp * outbp);
int  dhc_extract_opts(int iface, u_char * opts);
void     dhc_set_state   (int iface, int state);

extern   void fixup_subnet_mask(int netnum);  /* from ipnet.c */
static long dh_getlong( u_char *ptr ); 

extern char *  dhc_hostname(void);  /* returns a name for this machine */
extern char * name; /* name of product */

/* DC_DOMAINNAME - domain name to be used for this DHCP Client.
 * Used for option 81 in the DHCP Request.
 * Proper value for this macro must be defined in dhcpclnt.h or ipport.h
 * If it is not defined, then the following value will be used.
 */
#ifndef DC_DOMAINNAME
#define DC_DOMAINNAME  "iniche"
#endif

/* FUNCTION: dhc_get_srv_ipaddr()
 * 
 * PARAM1: u_char *options - the ones after "magic cookie"
 *
 * RETURNS: 
 */

ip_addr 
dhc_get_srv_ipaddr(u_char *options /* after magic cookie */) 
{
    u_char * opts;
    u_char   optlen;
   ip_addr srv_ipaddr = 0;

   if ((opts = find_opt(DHOP_SERVER, options)) != NULL) 
   {
      opts++;
      optlen = *opts;
      opts++;
      srv_ipaddr = dh_getlong(opts);
      opts += optlen;
   }

   return (srv_ipaddr);
} 


/* dhc_init() - this must set up the default fields above, read in 
 * bootptab file (if supported) and open UDP socket for listens. 
 * Returns 0 if OK, else negative error code from net.h file 
 */


/* FUNCTION: dhc_init()
 * 
 * This must set up the default fields above, read in 
 * bootptab file (if supported) and open UDP socket for listens. 
 *
 * PARAM1: void
 *
 * RETURNS: Returns 0 if OK, else negative error code from net.h file 
 */

int
dhc_init(void)
{
   int   i;

   /* open UDP connection to receive incoming DHCP replys */
   dhc_conn = udp_open(0L,    /* wildcard foriegn host */
      BOOTP_SERVER_PORT, BOOTP_CLIENT_PORT,
      dhc_upcall, DHCPDATA);

   if (!dhc_conn)
      return ENP_RESOURCE;

   for (i = 0; i < MAXNETS; i++)
   {
      dhc_states[i].state = DHCS_UNUSED;
      dhc_states[i].tries = 0;
   }

   return 0;
}



/* FUNCTION: dhc_set_callback()
 *
 * dhc_set_callback() : This interface allows the caller to set a 
 * calling back for a interface. When DHCP Client gets an ACK, it 
 * callings this routine. This mechanism is provider so that the 
 * caller can do some processing when the interface is up (like doing 
 * initializations or blinking LEDs , etc.). 
 *
 * 
 * PARAM1: int iface
 * PARAM2: int (*routine)(int
 * PARAM3: int
 *
 * RETURNS: 
 */

void
dhc_set_callback(int iface, int (*routine)(int,int) )
{
   dhc_states[iface].callback = routine;
}



/* FUNCTION: dhc_upcall()
 *
 * dhc_upcall() - DHCP client UDP callback. Called from stack 
 * whenever we get a bootp (or DHCP) reply. Returns 0 or error code. 
 * . 
 * 
 * PARAM1: PACKET pkt
 * PARAM2: void * data
 *
 * RETURNS: If the processing was sucessfull, the packet is freed and 0 is 
 * returned. Otherwise the packet is NOT free'ed and an error code is 
 * returned
 */

int
dhc_upcall(PACKET pkt, void * data)
{
   struct bootp * bp;
   int      len      =  pkt->nb_plen;  /* len of UDP data - the bootp/dhcp struct */
   int      dhcptype =  0;    /* DHCP type - not valid if bootp */
   int      e;
   int      iface;
   u_char * opts;          /* scratch options pointer */

   if (data != DHCPDATA)
   {
      dtrap();
      return ENP_LOGIC;    /* internal logic error */
   }

   /* punt if packet didn't come in a net we sent on */
   iface = net_num(pkt->net);
   if (dhc_states[iface].state == DHCS_UNUSED)
      return ENP_NOT_MINE;

   bp = (struct bootp *)pkt->nb_prot;

   /*   Validate various fields   */
   if ((len < (sizeof(struct bootp)-BOOTP_OPTSIZE) ) || 
       (bp->op != BOOTREPLY) ||
       (*(u_long*)(&bp->options) != RFC1084_MAGIC_COOKIE))
   {
      dtrap();
      dsc_errors++;
      return ENP_NOT_MINE;
   }

   /* punt offers or replys which are not for me */
   if(MEMCMP(bp->chaddr, pkt->net->mib.ifPhysAddress, pkt->net->n_hal))
      return ENP_NOT_MINE;    /* not an error, just ignore it */

   /* see if it's full DHCP or plain bootp by looking for dhcp type option */
   opts = find_opt(DHOP_TYPE ,&bp->options[4]);
   if (opts && *opts == DHOP_TYPE)
   {
      dhcptype = *(opts+2);
      bp->op |= ISDHCP;       /* tag packet for isdhcp() macro */
   }

   if (isdhcp(bp))
   {
      switch (dhcptype)
      {
      case DHCP_DISCOVER:
      case DHCP_REQUEST:
      case DHCP_DECLINE:
      case DHCP_RELEASE:
         dsc_errors++;     /* these should only be upcalled to a server */
         return ENP_NOT_MINE;
      }

      switch (dhc_states[iface].state)
      {
      case DHCS_INIT:
      case DHCS_INITREBOOT:
         /* How can we receive any response when we never sent one */
      case DHCS_BOUND:
         /* If there are multiple DHCP Servers, and one of them is slow
            in responding, we might get OFFER pkts when are in BOUND state */
         dsc_errors++;     /* these should only be upcalled to a server */
         return ENP_NOT_MINE;
      case DHCS_SELECTING:
         /* We will respond to the first offer packet that we receive ) */
         if ( dhcptype == DHCP_OFFER ) /* got offer back from server */
         {
            dsc_offers++;
            dhc_states[iface].srv_ipaddr = dhc_get_srv_ipaddr(&bp->options[4]);
            if (dhc_states[iface].srv_ipaddr == 0 )
            {
               dtrap(); /* didn't receive server-identifier option */
               dsc_errors++;
               dhc_states[iface].srv_ipaddr = pkt->fhost;   /* Try using fhost */
            }

            if (bp->hops)
            {
               /* OFFER is received via DHCP Relay Agent. Remember the
                * IP addr of DHCP Relay Agent, so that packets from other
                * DHCP Relay Agents can be discarded 
                */
               dhc_states[iface].rly_ipaddr = pkt->fhost;   /* Try using fhost */
            }
            else
               dhc_states[iface].rly_ipaddr = 0;

            e = dhc_rx_offer(iface,bp,pkt->nb_plen);     /* send request */
            if (e)
            {
               dsc_errors++;
               dhc_set_state(iface,DHCS_INIT);
               dtrap();
               return ENP_NOT_MINE;
            }
            else
               dhc_set_state(iface,DHCS_REQUESTING);
         }
         else
         {
            /* We can't receive any other packet in this state. 
             * Report an error and remain in SELECTING state, so that 
             * an OFFER packet from another DHCP server can be 
             * accepted. If we timeout waiting for a OFFER packet, 
             * then dhc_second() will transition to DHCS_INIT state. 
             */
            dsc_errors++;
            if ( dhcptype == DHCP_NAK ) 
               dsc_naks++;
            return ENP_NOT_MINE;
         }
         break;
      case DHCS_REQUESTING:
      case DHCS_REBINDING:
      case DHCS_RENEWING:
         /* If the ACK/NACK is not from the same server which sent 
          * the OFFER packet, then discard it. in DHCS_REBOOTING 
          * state, srv_ipaddr is 0. Hence don't check in that state 
          */
         if ( dhc_states[iface].srv_ipaddr != 
             dhc_get_srv_ipaddr(&bp->options[4]) )
         {
            dsc_errors++;
            return ENP_NOT_MINE;
         }
         if (dhc_states[iface].rly_ipaddr &&
            (dhc_states[iface].rly_ipaddr != pkt->fhost))
         {
            dsc_rlyerrs++;
            dsc_errors++;
            return ENP_NOT_MINE;
         }
      case DHCS_REBOOTING:
         if ( dhcptype == DHCP_ACK )   /* Server OKed our request */
         {
            dsc_acks++;
            dhc_extract_opts(iface,&bp->options[4]);
            if ( dhc_states[iface].lease == DHC_INFINITY )
            {
               dhc_states[iface].t1 = DHC_INFINITY ;
               dhc_states[iface].t2 = DHC_INFINITY ;
            }
            else
            {
               dhc_states[iface].t1 = dhc_states[iface].lease/2     ;
               dhc_states[iface].t2 = (dhc_states[iface].lease/8)*7 ;
            }
            dhc_states[iface].lease_start = cticks;   /* to calc lease expiry */
            dhc_states[iface].srv_ipaddr = dhc_get_srv_ipaddr(&bp->options[4]); 
            if (dhc_states[iface].srv_ipaddr == 0 )
            {
               dtrap(); /* didn't receive server-identifier option */
               dsc_errors++;
               dhc_states[iface].srv_ipaddr = pkt->fhost;   /* Try using fhost */
            }
            if (bp->hops)
            {
               /* OFFER is received via DHCP Relay Agent. Remember the
                * IP addr of DHCP Relay Agent, so that packets from other
                * DHCP Relay Agents can be discarded 
                */
               dhc_states[iface].rly_ipaddr = pkt->fhost;   /* Try using fhost */
            }
            else
               dhc_states[iface].rly_ipaddr = 0;

            dhc_setip(iface);
            dhc_set_state(iface,DHCS_BOUND);
         }
         else if ( dhcptype == DHCP_NAK ) /* Server denied our request */
         {
            dhc_set_state(iface,DHCS_INIT);
            dsc_naks++;
         }
         else
         {
            /* In REQUESTING state, we might receive a retransmitted
             * OFFER, which we should discard, but it's not an error,
             * so we log it.
             */
            if ((dhc_states[iface].state == DHCS_REQUESTING) &&
                (dhcptype == DHCP_OFFER))
            {
               dsc_offers++;
               return ENP_NOT_MINE;
            }
            /* In RENEWING, REBINDING, or REBOOTING states, we should
             * only receive ACK or NAK, and in REQUESTING state we
             * should only receive ACK or NAK or OFFER; these are
             * accounted for above, so we log whatever this is as an
             * error and discard it with no change to our state.
             */
            dsc_errors++;
            return ENP_NOT_MINE;
         }
         break;
      default:    /* bad state */
         dtrap();
         dhc_set_state(iface,DHCS_INIT);
         dsc_errors++;
         return -1;
      }
   }
   else     /* plain bootp reply */
   {
      dsc_bpreplys++;
      dhc_extract_opts(iface,&bp->options[4]);
      dhc_states[iface].ipaddr = bp->yiaddr;
      dhc_setip(iface);

      /* Set values so that DHCP State Machine remains happy */
      dhc_set_state(iface,DHCS_BOUND);
      dhc_states[iface].t1    = DHC_INFINITY ;
   }

   udp_free(pkt);
   return 0;
}


/* FUNCTION: dhc_buildheader()
 *
 * dhc_buildheader() - build BOOTP request header 
 *
 * 
 * PARAM1: int iface
 * PARAM2: struct bootp * outbp
 *
 * RETURNS: Returns 0 on success, else an ENP_ error code. 
 */

int
dhc_buildheader(int iface, struct bootp * outbp)
{
   int   addrlen;    /* length of hardware address */

   MEMSET(outbp, 0, sizeof(struct bootp));   /* most of this is 0 anyway */
   outbp->op = BOOTREQUEST;

   /* map SNMPish hardware types into bootp types */
   switch (nets[iface]->n_mib->ifType)
   {
   case ETHERNET:       /* ETHERNET defined in net.h */
      outbp->htype = ETHHWTYPE;  /* defined in dhcp.h */
   break;
   case PPP:
   case SLIP:
      outbp->htype = LINEHWTYPE;    /* line type for PPP or SLIP */
   break;
      default:
      dtrap();
      return ENP_LOGIC;             /* this shouldn't happen */
   }

   addrlen = min(16, nets[iface]->n_hal);
   outbp->hlen = (u_char)addrlen;
   outbp->hops = 0;
   if(dhc_states[iface].state == DHCS_RENEWING) 
      outbp->flags = 0; /* Renewing needs unicast */
   else
      outbp->flags = htons(DHC_BCASTFLAG); /* Othwise broadcast */
   outbp->xid = dhc_states[iface].xid;
   outbp->secs = dhc_states[iface].secs;
#ifdef NPDEBUG
   /* make sure net[] has a MAC address, even if length is zero */
   if(nets[iface]->mib.ifPhysAddress == NULL)
   {
      dtrap();
      return ENP_LOGIC;
   }
#endif
   MEMCPY(outbp->chaddr, nets[iface]->mib.ifPhysAddress, addrlen);

   /* return success */
   return 0;
}

unsigned long sysuptime(void);


/* FUNCTION: dhc_discover()
 *
 * dhc_discover() - Send initial DHCP discovery packet for the passed 
 * interface. 
 *
 * 
 * PARAM1: int iface
 *
 * RETURNS: Returns 0 if ok, else non-zero ENP_ error. 
 */

int
dhc_discover(int iface)
{
   PACKET pkt;
   struct bootp * outbp;   /* pointer to bootp block in buffer to build */
   u_char * opts;       /* scratch pointer to DHCP options field */
   long     leasetime;
   int      e;

   /* get a UDP packet buffer for DHCP sending */
   pkt = udp_alloc(sizeof(struct bootp), 0);
   if (!pkt) 
      return ENP_NOMEM;
   pkt->nb_plen = sizeof(struct bootp);

   /* start a new DHCP transaction */
   dhc_states[iface].xid = xids++;
   dhc_states[iface].secs = (unsigned short)(sysuptime()/100L);

   /* set up DHCP/BOOTP header in buffer */
   outbp = (struct bootp *)pkt->nb_prot;     /* overlay bootp struct on buffer */
   e = dhc_buildheader(iface,outbp);
   if (e)
      return e;

   /* and turn it into a DHCP DISCOVER packet */
   *(long*)(&outbp->options) = RFC1084_MAGIC_COOKIE; 
   opts = &outbp->options[4];    /* encode options after cookie */
   *opts++ = DHOP_TYPE;
   *opts++ = 1;   /* length of option field */
   *opts++ = DHCP_DISCOVER;
   leasetime = -1L ;    /* ask for infinite lease */
   PUT_IP_OPT(opts, DHOP_LEASE, leasetime);

   /* if we already have an IP address, try to get it from the server */
   if (nets[iface]->n_ipaddr != 0)
   {
      ip_addr my_ip = htonl(nets[iface]->n_ipaddr);
      PUT_IP_OPT(opts, DHOP_CADDR, my_ip);
   }

   /* If there is a list of options to be requested from server, include it*/
#ifdef DHCP_REQLIST
   if ( reqlist_len > 0 )
   {
      int   i;
      *opts++ = DHOP_REQLIST ;
      *opts++ = (u_char)reqlist_len ;

      for (i=0 ; i < reqlist_len ; i++ )
         *opts++ = reqlist[i];
   }
#endif   /* DHCP_REQLIST */

   *opts++ = DHOP_END;

   /* last_tick needs to be set in case we are doing a retry. It 
    * prevents dhc_second from calling us to do another retry while 
    * we are stuck 
    */
   dhc_states[iface].last_tick = cticks;

   pkt->fhost = 0xFFFFFFFF;   /* broadcast discovery request */
   pkt->net = nets[iface];    /* send out caller spec'ed net */

   /* we need to change the DHCP state before sending to avoid a 
    * race condition with the expected reply 
    */
   if (dhc_states[iface].state != DHCS_SELECTING)
      dhc_set_state(iface, DHCS_SELECTING);

   udp_send(BOOTP_SERVER_PORT, BOOTP_CLIENT_PORT, pkt);
   dsc_discovers++;

   /* state info is the same even if udp_send() failed */
   dhc_states[iface].last_tick = cticks;     /* set this again, post udp_send */
   dhc_states[iface].tries++;

   return 0;
}


/* FUNCTION: dhc_rx_offer()
 *
 * dhc_rx_offer() - process the received DHCPOFFER Process the 
 * received DHCP-OFFER and populate dhc_states[]. Then call 
 * dhc_request() to send a request packet. 
 *
 * 
 * PARAM1: int iface
 * PARAM2: struct bootp * bp
 * PARAM3: unsigned bplen
 *
 * RETURNS: 0 if OK, else ENP_ error
 */

int
dhc_rx_offer(int iface, struct bootp * bp, unsigned bplen)
{
   u_char * opts;
   int   e;

   if (dhc_states[iface].xid != bp->xid)
      return ENP_NOT_MINE;

   opts = &bp->options[4];    /* examine options after cookie */
   e = dhc_extract_opts(iface,opts);
   if (e)   /* parse error? */
   {
      dtrap();
      return e;
   }
   if (!bp->yiaddr)  /* require an IP address */
   {
      dhc_decline(iface,bp, bplen);
      return ENP_NOT_MINE;
   }
   dhc_states[iface].ipaddr = bp->yiaddr;

   /* if we got here, we must like the offer -- send a DHCP REQUEST */
   return (dhc_request(iface,FALSE));
}


/* FUNCTION: dhc_request()
 * 
 * dhc_request() - called when 
 * 1. we have received a DHCP offer (via dhc_rx_offer() ). 
 * 2. we have to renew a lease with a DHCP server. ( via dhc_reclaim() ). 
 * 3. we have to rebind to a different DHCP server. 
 * 4. we are in INIT-REBOOT state (via dhc_reclaim() ). 
 * This should format & send a request. Values are picked from 
 * dhc_states[]. It handles the specific nuances of a REQUEST 
 * depending on the state of DHCP Client. It implements the following 
 * table of RFC2131 (sec 4.3.6). 
 * --------------------------------------------------------------------- 
 * |              |INIT-REBOOT |SELECTING |RENEWING   |REBINDING | 
 * --------------------------------------------------------------------- 
 * |broad/unicast |broadcast   |broadcast |unicast    |broadcast | 
 * |server-ip     |MUST NOT    |MUST      |MUST NOT   |MUST NOT  | 
 * |requested-ip  |MUST        |MUST      |MUST NOT   |MUST NOT  | 
 * |ciaddr        |zero        |zero      |IP address |IP address| 
 * --------------------------------------------------------------------- 
 * In a DISCOVER->OFFER->REQUEST scenario, we need to use the same 
 * XID, SECS field as in OFFER packet. This is controlled via 
 * xid_flag. If xid_flag is TRUE, then use a new XID. Else use the 
 * old one.
 *
 * PARAM1: int iface
 * PARAM2: int xid_flag
 *
 * RETURNS:  Returns 0 if ok, else non-zero ENP_ error. 
 */

int
dhc_request(int iface,int xid_flag)
{
   struct  bootp *   outbp;
   PACKET  pkt;         /* Outgoing packet */
   u_char  *   opts; /* scratch pointer to DHCP options field */
   ip_addr opt_ip;      /* IP address temporary */
   int      e;       /* error holder */

   /* get a UDP packet buffer for sending DHCP request */
   pkt = udp_alloc(sizeof(struct bootp) + DHCP_OPTSIZE - BOOTP_OPTSIZE, 0);
   if (!pkt) 
      return ENP_NOMEM;
   pkt->nb_plen = sizeof(struct bootp) - BOOTP_OPTSIZE;

   if ( xid_flag == TRUE )
   {
      dhc_states[iface].xid  = xids++;
      dhc_states[iface].secs = 0;
   }

   /* build a BOOTP request header */
   outbp = (struct bootp *)pkt->nb_prot;
   e     = dhc_buildheader(iface,outbp);
   if (e)
      return e;

   /* turn it into a DHCP REQUEST packet */
   *(long*)(&outbp->options) = RFC1084_MAGIC_COOKIE; 
   opts    = &outbp->options[4];    /* encode options after cookie */
   *opts++ = DHOP_TYPE;
   *opts++ = 1;   /* length of option field */
   *opts++ = DHCP_REQUEST;

   /* append the options that we want to request */
   if ((dhc_states[iface].state == DHCS_SELECTING) ||
       (dhc_states[iface].state == DHCS_REQUESTING) ||
       (dhc_states[iface].state == DHCS_REBOOTING) ||
       (dhc_states[iface].state == DHCS_INITREBOOT))
   {
      opt_ip = ntohl(dhc_states[iface].ipaddr);
      PUT_IP_OPT(opts, DHOP_CADDR, opt_ip);
   }
   if (dhc_states[iface].snmask)
   {
      opt_ip = ntohl(dhc_states[iface].snmask);
      PUT_IP_OPT(opts, DHOP_SNMASK, opt_ip);
   }
   if (dhc_states[iface].defgw)
   {
      opt_ip = ntohl(dhc_states[iface].defgw);
      PUT_IP_OPT(opts, DHOP_ROUTER, opt_ip);
   }
#if defined(DHC_MAXDNSRVS) && (DHC_MAXDNSRVS > 0)
   {
      int   i;
      u_char * optlen=NULL;
      for (i = 0; i < DHC_MAXDNSRVS; i++)
      {
         if (dhc_states[iface].dnsrv[i])
         {
            if (i == 0)
            {
               *opts++ = DHOP_DNSRV;
               optlen  = opts;
               *opts++ = 0;
            }
            /* Copy the IP address in nw byte order */
            opt_ip = dhc_states[iface].dnsrv[i];
            MEMCPY((char *)opts, (char *)&opt_ip,4);
            opts+=4;
            *optlen += 4;
         }
      }
   }
#endif

   if (dhc_states[iface].lease)
   {
      PUT_IP_OPT(opts, DHOP_LEASE, dhc_states[iface].lease);
   }

   /* If there is a list of options to be requested from server, include it*/
#ifdef DHCP_REQLIST
   if ( reqlist_len > 0 )
   {
      int   i;
      *opts++ = DHOP_REQLIST ;
      *opts++ = (u_char)reqlist_len ;

      for (i=0 ; i < reqlist_len ; i++ )
         *opts++ = reqlist[i];
   }
#endif   /* DHCP_REQLIST */

   /* only set client IP address (ours) when renewing or rebinding */
   if ((dhc_states[iface].state == DHCS_RENEWING)
       || (dhc_states[iface].state == DHCS_REBINDING))
   {
      outbp->ciaddr = nets[iface]->n_ipaddr;
   }

   /* RFC 2131, section 4.3.5 ref to server-ip is option 54 0x36, not 
    * siaddr: p. 16 number 3: "The client broadcasts a DHCPREQUEST 
    * message that MUST include the 'server identifier' option to 
    * indicate which server it has selected. . . ." p. 31 bullet 1: 
    * "Client inserts the address of the selected server in 'server 
    * identifier'. . . ." RFC 951, p. 4 definition of 'siaddr' is 
    * "server IP address; returned in bootreply by server." 
    */
   /* Only include server identifier option when selecting a server. */
   if ((dhc_states[iface].state == DHCS_SELECTING) ||
       (dhc_states[iface].state == DHCS_REQUESTING))
   {
      opt_ip = ntohl(dhc_states[iface].srv_ipaddr);
      PUT_IP_OPT(opts, DHOP_SERVER, opt_ip);
   }

#ifdef USE_AUTOIP
   /* add hostname (code 12) */
   PUT_STRING_OPT(opts, 12, dhc_hostname()); 
#endif /* USE_AUTOIP */

   /* Client Fully Qualified Domain Name */
   PUT_STRING_OPT(opts, 81, DC_DOMAINNAME); 
   /* Vendor Class Identifier */
   PUT_STRING_OPT(opts, 60, name); 

   *opts++ = DHOP_END;  /* Mark the end of options */

   /* figure out whether to send via unicast or broadcast */
   if (dhc_states[iface].state == DHCS_RENEWING)
   {
      pkt->fhost = dhc_states[iface].srv_ipaddr;
   }
   else
   {
      pkt->fhost = 0xFFFFFFFF;   /* broadcast request */
   }

   pkt->net = nets[iface];    /* send out caller spec'ed net */
   pkt->nb_plen = (char *)opts - (char *)outbp;
   udp_send(BOOTP_SERVER_PORT, BOOTP_CLIENT_PORT, pkt);
   dsc_requests++;

   dhc_states[iface].last_tick = cticks;
   dhc_states[iface].tries++;

   return 0;   /* return OK code */
}


#ifdef MINI_IP

/* FUNCTION: fixup_subnet_mask()
 *
 * This is a workalike to the function of the same name in the ip directory.
 * If we are using MINI_IP then dhcp needs it's own copy of this.
 * 
 * PARAM1: int netnum
 *
 * RETURNS: 
 */

void
fixup_subnet_mask(int netnum)      /* which of the nets[] to do. */
{
   u_long   smask;

   if (m_netp->snmask)  /* if mask is already set, don't bother */
      return;

   /* things depending on IP address class: */
   if ((m_netp->n_ipaddr & AMASK) == AADDR)
      smask = 0xFF000000L;
   else if((m_netp->n_ipaddr & BMASK) == BADDR)
      smask = 0xFFFF0000L;
   else if((m_netp->n_ipaddr & CMASK) == CADDR)
      smask = 0xFFFFFF00L;
   else
   {
      dtrap();    /* bad logic or setup values */
      smask = 0xFFFFFF00L;
   }
   m_netp->snmask = htonl(smask);
}
#endif /* MINI_IP */


/* FUNCTION: dhc_setip()
 *
 * dhc_setip() : Set the IP address (& other paras) for the interface 
 * It is called when we have received an ACK. That is, we have the IP 
 * address and other information needed for this interface. If the 
 * subnet mask has not been specified by the DHCP Server, then we 
 * will calculate one from the IP Address using fixup_subnet_mask() 
 * 
 * PARAM1: int iface
 *
 * RETURNS: Returns 0 if ok, else non-zero ENP error.
 */

int
dhc_setip(int iface)
{
   nets[iface]->n_ipaddr = dhc_states[iface].ipaddr;
   nets[iface]->snmask   = dhc_states[iface].snmask;
   nets[iface]->n_defgw  = dhc_states[iface].defgw;

   if ( nets[iface]->snmask == 0 )
   {
      fixup_subnet_mask(iface);
      dhc_states[iface].snmask = nets[iface]->snmask; 
   }

   /* fixup broadcast addresses */
   nets[iface]->n_netbr    = nets[iface]->n_ipaddr | ~nets[iface]->snmask;
   nets[iface]->n_netbr42  = nets[iface]->n_ipaddr &  nets[iface]->snmask;
   nets[iface]->n_subnetbr = nets[iface]->n_ipaddr | ~nets[iface]->snmask;

   return 0;   /* return OK code */
}


/* FUNCTION: dhc_resetip()
 *
 * dhc_resetip() : Reset the IP address (& other paras) for the 
 * interface It is called when we didn't receive a ACK/NAK for the 
 * interface when RENEWING/REBINDING. 
 * 
 * PARAM1: int iface
 *
 * RETURNS: Returns 0 if ok, else non-zero error. 
 */

int
dhc_resetip(int iface)
{
   /* reset the ipaddress */
   nets[iface]->n_ipaddr = 0;
   nets[iface]->snmask   = 0;
   nets[iface]->n_defgw  = 0;

   /* reset the broadcast addresses */
   nets[iface]->n_netbr    = 0;
   nets[iface]->n_netbr42  = 0;
   nets[iface]->n_subnetbr = 0;

   return 0;   /* return OK code */
}



/* FUNCTION: dhc_decline()
 * 
 * dhc_decline() - send a decline packet to server. This is usually 
 * because he didn't send us an IP address. 
 *
 * PARAM1: int iface
 * PARAM2: struct bootp * bp
 * PARAM3: unsigned bplen
 *
 * RETURNS: Returns 0 if ok, else non-zero ENP_ error. 
 */

int
dhc_decline(int iface,struct bootp * bp, unsigned bplen)
{
   struct bootp * outbp;
   PACKET pkt;
   u_char * opts;    /* scratch pointer to DHCP options field */

   /* get a UDP packet buffer for sending DHCP */
   pkt = udp_alloc(bplen, 0);
   if (!pkt) 
      return ENP_NOMEM;
   pkt->nb_plen = bplen;

   outbp = (struct bootp *)pkt->nb_prot;
   MEMCPY(outbp, bp, bplen);
   outbp->op = BOOTREQUEST;

   /* find DHCP TYPE option so we can overwrite it */   
   opts = find_opt(DHOP_TYPE, &outbp->options[4]);
   opts += 2;     /* point to actual op code */
   *opts = DHCP_DECLINE;   /* overwrite op code */

   pkt->fhost = 0xFFFFFFFF;   /* broadcast decline pkt */
   pkt->net = nets[iface];    /* send out caller speced net */
   udp_send(BOOTP_SERVER_PORT, BOOTP_CLIENT_PORT, pkt);
   dsc_declines++;   /* count declines sent */
   return 0;
}



/* FUNCTION: dh_getlong()
 *
 * dh_getlong() - routine to extract a 'long' from an arbitrary 
 * address NB: value is expected to remain in network byte order! 
 *
 * 
 * PARAM1:  u_char *ptr 
 *
 * RETURNS: the extracted 32 bit value
 */

static   long dh_getlong( u_char *ptr )
{
     long  v;
     u_char * p2 =  (u_char *)&v;

   *p2++ = *ptr++;
   *p2++ = *ptr++;
   *p2++ = *ptr++;
   *p2++ = *ptr++;

   return v;
}


/* FUNCTION: dhc_extract_opts()
 * 
 * dhc_extract_opts() - extract certain interesting options from the 
 * options string passed. These options are filled in to the fields 
 * passed. 
 *
 * PARAM1: int iface
 * PARAM2: u_char * opts
 *
 * RETURNS: Returns 0 if opts string was parsed OK, else non-zero if 
 * there was a parse error. 0 return doesn't mean all (or even any) 
 * of the options passed were filled in with good values. 
 */

int
dhc_extract_opts(int iface, u_char *opts)
{
   u_char *end = opts + DHCP_OPTSIZE;  /* limit scope of search */
   u_char optlen;

   /* first, clear the options */
   dhc_states[iface].snmask = 0; 
   dhc_states[iface].defgw = 0; 
   dhc_states[iface].lease = 0; 
#if defined(DHC_MAXDNSRVS) && (DHC_MAXDNSRVS > 0)
   MEMSET(dhc_states[iface].dnsrv, 0, sizeof(dhc_states[iface].dnsrv));
#endif   /* DHC_MAXDNSRVS */

   /* then fill them in from the DHCP data */
   while (opts <= end)
   {
      switch (*opts++)
      {
      case DHOP_PAD:
         break;
      case DHOP_END:
         return 0;   /* only good exit point */
      case DHOP_SNMASK:
         opts++;
         dhc_states[iface].snmask = dh_getlong(opts);
         opts += 4;
         break;
      case DHOP_ROUTER:
         optlen = *opts++;
         if (optlen >= 4)
            dhc_states[iface].defgw = dh_getlong(opts);
         opts += optlen;
         break;
      case DHOP_LEASE:
         opts++;
         dhc_states[iface].lease = htonl(dh_getlong(opts));
         opts += 4;
         break;
      case DHOP_DNSRV:
         optlen = *opts++;
#if defined(DHC_MAXDNSRVS) && (DHC_MAXDNSRVS > 0)
         {
            int i = 0;

            while ((optlen >= 4) && (i < DHC_MAXDNSRVS))
            {
               dhc_states[iface].dnsrv[i] = dh_getlong(opts);
               optlen -= 4;
               opts += 4;
               i++;
            }
         }
#endif   /* DHC_MAXDNSRVS */
         opts += optlen;
         break;
      default:
         opts += ((*opts) + 1);
         break;
      }
   }
   dtrap();
   return -1;
}

/* dhc_stats() - print dhcp client statistics. Returns 0 if ok, else 
 * non-zero error. 
 */
#ifdef NET_STATS


/* FUNCTION: dhc_stats()
 * 
 * PARAM1: void *pio
 *
 * RETURNS: 
 */

int
dhc_stats(void *pio)
{
   int   i;
   ns_printf(pio ,"dhcp client stats:\n");
   ns_printf(pio ,"all errors:      %lu\n", dsc_errors);
   ns_printf(pio ,"discover sent:   %lu\n", dsc_discovers);
   ns_printf(pio ,"offers rcvd:     %lu\n", dsc_offers);
   ns_printf(pio ,"requests sent:   %lu\n", dsc_requests);
   ns_printf(pio ,"acks received:   %lu\n", dsc_acks);
   ns_printf(pio ,"bootp replys:    %lu\n", dsc_bpreplys);
   ns_printf(pio ,"declines sent:   %lu\n", dsc_declines);
   ns_printf(pio ,"releases sent:   %lu\n", dsc_releases);
   ns_printf(pio ,"naks received:   %lu\n", dsc_naks);
   ns_printf(pio ,"renew req sent:  %lu\n", dsc_renew);
   ns_printf(pio ,"rebind req sent: %lu\n", dsc_rebind);
   ns_printf(pio ,"relay agent errs:%lu\n", dsc_rlyerrs);

   for ( i=0 ; i < MAXNETS ; i++ )
   {
      ns_printf(pio ,"Interface %d state = %s\n",i+1,
       dhc_state_str[ dhc_states[i].state ] );
      if ( dhc_states[i].state == DHCS_UNUSED )
         continue;
      else
      {
         ns_printf(pio ,"   tries=%d, xid=%lu, secs=%d\n", 
          dhc_states[i].tries,dhc_states[i].xid,dhc_states[i].secs);
         ns_printf(pio ,"   lease=%lu, t1=%lu, t2=%lu\n", 
          dhc_states[i].lease,dhc_states[i].t1,dhc_states[i].t2);
         ns_printf(pio ,
          "   ip=%u.%u.%u.%u, snmask=%u.%u.%u.%u, gw=%u.%u.%u.%u\n", 
          PUSH_IPADDR(dhc_states[i].ipaddr),
          PUSH_IPADDR(dhc_states[i].snmask),
          PUSH_IPADDR(dhc_states[i].defgw)  );
         ns_printf(pio , "   serverip=%u.%u.%u.%u\n", 
          PUSH_IPADDR(dhc_states[i].srv_ipaddr) );
#if defined(DHC_MAXDNSRVS) && (DHC_MAXDNSRVS > 0)
         {
            int   dnsindex;
            for (dnsindex=0; dnsindex < DHC_MAXDNSRVS ; dnsindex++ )
            {
               ns_printf(pio , "   DNS%d=%u.%u.%u.%u\n", dnsindex+1,
                  PUSH_IPADDR(dhc_states[i].dnsrv[dnsindex]) );
            }
         }
#endif
      }
   }
   return 0;
}
#endif   /* NET_STATS */


/* FUNCTION: dhc_second()
 *
 * dhc_second() - dhcp client timer. system should call this once a 
 * second for retries, lease renewal, etc. 
 *
 * 
 * PARAM1: void
 *
 * RETURNS: Returns 0 or ENP_ error code 
 */

int
dhc_second(void)
{
   int   iface;
   int   tries;
   int   e;
   u_long   half_time;

   for (iface = 0; iface < MAXNETS; iface++)
   {
      switch (dhc_states[iface].state)
      {
      case DHCS_INIT:         /* Send a discover packet */
         e = dhc_discover(iface);
         /* Error while sending a discover packet */
         if (e)
         {
            dtrap();
            return e;
         }
         dhc_set_state(iface,DHCS_SELECTING);
         break;
      case DHCS_INITREBOOT:   /* Send a request packet */
         e = dhc_reclaim(iface);
         if (e)
         {
            dtrap();
            return e;
         }
         dhc_set_state(iface,DHCS_REBOOTING);
         break;
      case DHCS_SELECTING:
         /* Send discover packet on timeout */
      case DHCS_REBOOTING:
      case DHCS_REQUESTING:
         /* Discovery timeout = DHC_RETRY_TMO secs * (2 ** retries), max 64 */

         tries = dhc_states[iface].tries ;

         /* Set the exponential count */
         if ( tries >= DHC_MAX_TRIES) 
            tries= DHC_MAX_TRIES;
         if ( cticks > (dhc_states[iface].last_tick + 
             (((u_long) (DHC_RETRY_TMO*TPS)) << tries ) ) )
         {
            /* Timeout while waiting for a OFFER/ACK/NAK. Retransmit */
            switch(dhc_states[iface].state)
            {
            case DHCS_SELECTING:
               dhc_discover(iface);
               break;
            case DHCS_REQUESTING:
               dhc_request(iface,FALSE);
               break;
            case DHCS_REBOOTING:
               dhc_reclaim(iface);
               break;
            default:
               dtrap(); /* bogus state */
               break;
            }
         }
         if ( tries == DHC_MAX_TRIES && 
             (dhc_states[iface].state !=DHCS_SELECTING) )
         {
            /* We have tried enough. Restart from INIT state */
            dhc_set_state(iface,DHCS_RESTARTING);
            dhc_resetip(iface);
            dhc_set_state(iface,DHCS_INIT);
         }
         break;
      case DHCS_REBINDING:
         /* Check for timeout. Retry if we didn't get a ACK/NAK response. */

         if ( (dhc_states[iface].lease*TPS+dhc_states[iface].lease_start) > cticks )
         {
            /* See if we need to retransmit. If we have waiting for 
             * half the time between last transmit and lease, then we 
             * need to retransmit. Also the minimum retransmit 
             * interval is 60 secs. 
             */
            half_time = (dhc_states[iface].lease_start + 
             dhc_states[iface].lease*TPS - 
             dhc_states[iface].last_tick)/2;

            if ( half_time < 60*TPS )
               half_time = 60*TPS;
            if ( dhc_states[iface].last_tick + half_time < cticks )
            {
               dhc_request(iface,FALSE);
            }
         }
         else
         {
            /* Lease has expired. We didn't receive a ACK/NAK. Hence restart*/
            dhc_set_state(iface,DHCS_RESTARTING);
            dhc_resetip(iface);
            dhc_set_state(iface,DHCS_INIT);
         }
         break;

      case DHCS_BOUND:
         /* Test for lease expiry. The RENEW timer. */
         if ( (dhc_states[iface].t1 != DHC_INFINITY) &&
             (((dhc_states[iface].t1*TPS)+dhc_states[iface].lease_start) < cticks ) )
         {
            /* Time to renew. Send a UNICAST to the DHCP server */
            dhc_set_state(iface,DHCS_RENEWING);
            e = dhc_reclaim(iface); /* unicast */ 
            if (e)
            {
               dtrap();
               return e;
            }
            dsc_renew++;
         }
         break;
      case DHCS_RENEWING:
         /* Test for lease expiry. The REBIND timer. */
         if ( (dhc_states[iface].t2*TPS+dhc_states[iface].lease_start) > cticks )
         {
            /* See if we need to retransmit. If we have waiting for 
             * half the time between last transmit and t2, then we 
             * need to retransmit. Also the minimum retransmit 
             * interval is 60 secs. 
             */
            half_time = (dhc_states[iface].lease_start +
             dhc_states[iface].t2*TPS - 
             dhc_states[iface].last_tick)/2;

            if ( half_time < 60*TPS )
               half_time = 60*TPS;
            if ( dhc_states[iface].last_tick + half_time < cticks )
            {
               dhc_request(iface,FALSE);
            }
         }
         else
         {
            /* No Response has come from the Server that assigned our 
             * IP. Hence send a broadcast packet to see if we can 
             * lease this IP from some other server 
             */
            dhc_set_state(iface,DHCS_REBINDING);
            e = dhc_request(iface,TRUE);  /* broadcast */
            if (e)
            {
               dtrap();
               return e;
            }
            dsc_rebind++;
         }
         break;
      case DHCS_UNUSED:
      default:
         continue;
      }
   }
   return 0;
}


/* FUNCTION: dhc_halt()
 *
 * dhc_halt() - stop dhcp all client activity for the passed iface.
 *
 * 
 * PARAM1: int iface
 *
 * RETURNS: void
 */

void
dhc_halt(int iface)
{
   if (iface < 0 || iface > MAXNETS)
   {
      dtrap();
      return;
   }
   /* clear dhc_states entry - (kills retrys) */
   MEMSET(&dhc_states[iface], 0, sizeof(struct dhc_state));
   dhc_states[iface].state = DHCS_UNUSED;
}


/* FUNCTION: dhc_reclaim()
 *
 * dhc_reclaim() - send a "request" for the address currently in the 
 * passed interface. This is an alternative to dhc_discover() for 
 * machines which have previously been assigned an address via DHCP, 
 * saved it in flash, been through a power cycle, and now want to try 
 * to reclaim previous address from the server. This works by setting 
 * parameters of dhc_states[] and calling dhc_request() 
 *
 * 
 * PARAM1: int iface
 *
 * RETURNS: Returns 0 if DHCP request was sent OK, else non-zero error. 
 */

int   
dhc_reclaim(int iface)
{
   /* punt if IP address is not set */
   if (nets[iface]->n_ipaddr == 0L)
   {
      dtrap();    /* programming bug? */
      return ENP_LOGIC;
   }

   dhc_states[iface].ipaddr = nets[iface]->n_ipaddr;
   dhc_states[iface].snmask = nets[iface]->snmask;
   dhc_states[iface].defgw  = nets[iface]->n_defgw;

#ifdef IP_ROUTING
   /* If the DHCP Server is on other network, route the request
    * from the same DHCP relay agent. To do that, add a route.
    */
   if (dhc_states[iface].rly_ipaddr)
   {
      if (dhc_states[iface].srv_ipaddr)
      {
         /* yes, earlier negotiation was done via a relay agent */
         if ( !add_route(dhc_states[iface].srv_ipaddr, 0xFFFFFFFF,
             dhc_states[iface].rly_ipaddr, iface, IPRP_LOCAL))
         {
            /* route was not added. check this case */
            dtrap(); 
         }
      }
      else
      {
         /* DHCP relay IP address is set, but DHCP Server IP address is
          * not set ! How can this happen ?
          */
         dtrap();
      }
   }
#endif  /* IP_ROUTING */

   /* send the request */
   return(dhc_request(iface,TRUE));  
}


/* FUNCTION: dhc_state_init()
 * 
 * dhc_state_init() : Initiate the DHCP Client for a interface into a 
 * starting state. Either INIT or INITREBOOT. If the DHCP Client is 
 * in INIT state, it would start by sending a DISCOVER packet. If it 
 * is in INITREBOOT state, it would try to use the earlier IP address 
 * by sending a REQUEST packet. Returns nothing. 
 *
 * PARAM1: int iface
 * PARAM2: int init_flag
 *
 * RETURNS: void
 */

void 
dhc_state_init(int iface, int init_flag)
{
   int state = (init_flag == TRUE) ? DHCS_INIT : DHCS_INITREBOOT;
   
   dhc_set_state(iface, state);
}


/* FUNCTION: dhc_alldone()
 * 
 * dhc_alldone() : Check if DHCP activities on all interfaces is done 
 * or not. 
 *
 * PARAM1: void
 *
 * RETURNS: Return TRUE if all valid interfaces are done. Return FALSE 
 * otherwise. 
 */

int 
dhc_alldone(void)
{
   int   i;
   for ( i=0 ; i < MAXNETS ; i++ )
   {
      if ( ( dhc_states[i].state == DHCS_UNUSED ) || 
          ( dhc_states[i].state == DHCS_BOUND  )  )
      {
         continue ;
      }
      else
      {
         return FALSE ;
      }
   }
   return TRUE ;
}


/* FUNCTION: dhc_ifacedone()
 *
 * dhc_ifacedone() : Check if DHCP activities on a interface is done 
 * or not. Return TRUE if done (or if DHCP Client is not used on the 
 * interface). Return FALSE otherwise. 
 *
 * 
 * PARAM1: int iface
 *
 * RETURNS: 
 */

int 
dhc_ifacedone(int iface)
{
   if ( ( dhc_states[iface].state == DHCS_UNUSED ) || 
       ( dhc_states[iface].state == DHCS_BOUND  )  )
   {
      return TRUE ;
   }
   else
   {
      return FALSE ;
   }
}


/* FUNCTION: dhc_set_state()
 *
 * dhc_set_state() : Set the new state for the interface and call the 
 * callback routine. That way applications can track about the state 
 * of DHCP Client 
 *
 * 
 * PARAM1: int iface
 * PARAM2: int state
 *
 * RETURNS: 
 */

void dhc_set_state(int iface, int state)
{
   dhc_states[iface].state = state; /* Set the new state */
   dhc_states[iface].tries = 0;     /* Reset the number of tries */

   /* If callback is set, call it */
   if (dhc_states[iface].callback)
      dhc_states[iface].callback(iface,state);
}

#endif   /* DHCP_CLIENT - ifdef out whole file */


