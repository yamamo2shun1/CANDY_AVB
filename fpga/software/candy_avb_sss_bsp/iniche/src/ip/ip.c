/*
 * FILENAME: ip.c
 *
 * Copyright 1997-2002 InterNiche Technologies Inc. All rights reserved
 *
 * This contains the base IP code except the received packet demuxer.
 *
 * MODULE: INET
 *
 * ROUTINES: ip_init(), ip_bldhead(), ip_write_internal(), ip_write(), 
 * ROUTINES: ip_raw_write(), ip_fragment(), ip_mymach(), ip_dump(), 
 * ROUTINES: iproute(), add_route(), del_route(), ip_data(), ip_optlen(), 
 * ROUTINES: ip_copypkt()
 *
 * PORTABLE: yes
 */

/* Additional Copyrights: */
/* Portions Copyright 1990 by NetPort Software. */
/* Portions Copyright 1986 by Carnegie Mellon  */
/* Portions Copyright 1984 by the Massachusetts Institute of Technology  */


#include "ipport.h"
#include "libport.h"
#include "q.h"
#include "netbuf.h"
#include "net.h"
#include "arp.h"
#include "ip.h"
#include "icmp.h"
#include "ip_reasm.h"

static unsigned uid = 1;      /* base for unique IP packet ID */

struct IpMib   ip_mib;     /* IP stats for snmp */

#ifdef IP_ROUTING
RTMIB    rt_mib;
unsigned ipRoutes = RT_TABS;
#endif   /* IP_ROUTING */

#ifdef IP_MULTICAST
int   ipmc_outopt(PACKET p, u_char prot);
#endif   /* IP_MULTICAST */

#ifdef IP_FRAGMENTS
int   ip_fragment(PACKET p, ip_addr firsthop);
#endif

#ifdef IPSEC
int   IPSecInit(void);
int   ipsec_outbound(PACKET * pp);
#endif

/* function to obtain the length of the largest interrupt-safe buffer */
extern unsigned pk_get_max_intrsafe_buf_len (void);
/* compute the minimum of two quantities A and B */
#define MIN(A,B) (((A)<(B))?(A):(B))
/* number of fragments upto which we use ip_fragment_lc () for fragmentation 
 * (but only if the original IP datagram passed to ip_fragment () is in an 
 * interrupt-safe buffer) */
#define FRAG_SCHEME_SWITCH_THRESHOLD 3

/* FUNCTION: ip_init()
 *
 * ip_init() - Intialize the internet layer.
 *
 * 
 * PARAM1: void
 *
 * RETURNS: Returns SUCCESS (0) if everything went 
 * OK, else returns a non-zero error code. 
 */

int
ip_init(void)
{

   /* register IP type with link layer drivers */
   if (reg_type(IP_TYPE) != 0)
   {   
#ifdef NPDEBUG
      dprintf("IP_INIT: unable to register type with MAC driver\n");
#endif
      return(1);
   }

   /* initialize the IP mib */
   MEMSET(&ip_mib, 0, sizeof(ip_mib));
   ip_mib.ipForwarding = 2;   /* default to host, not gateway (router) */
   ip_mib.ipDefaultTTL = IP_TTL;

#ifdef IP_ROUTING
   /* alloc space for the route table */
   rt_mib = (struct RtMib*)RT_ALLOC(ipRoutes * sizeof(struct RtMib));
   if (!rt_mib)
   {
#ifdef NPDEBUG
      dprintf("IP_INIT ERROR: can't alloc route table\n");
#endif   /* NPDEBUG */
      return(ENP_NOMEM);
   }

  MEMSET(rt_mib,0, ipRoutes * sizeof(struct RtMib)) ;
  ip_mib.ipForwarding = 1;   /* override default, be gateway (router) */
#endif   /* IP_ROUTING */

   /* set IP reassembly timeout */
   ip_mib.ipReasmTimeout = IRE_TMO;

#ifdef IPSEC
   if (IPSecInit())
   {
#ifdef NPDEBUG
      dprintf("IP_INIT: IPSecInit() failed\n");
#endif   /* NPDEBUG */
      return(1);
   }
#endif   /* IPSEC */

   /* everything opened OK return 0 */
   return(SUCCESS);
}

/* FUNCTION: ip_bldhead()
 *
 * ip_bldhead() - Prepend an IP header to p->nb_prot
 *
 * NOTE: this is no longer used by ip_write(), which inlines it
 * so it can override the IP time-to-live with the multicast time-to-live
 * and only calculate the IP header checksum once.  It remains as
 * a utility function for the convenience of various pseudo-drivers
 * that generate IP datagrams to exercise the stack.
 * 
 * PARAM1: PACKET p
 * PARAM2: unsigned pid
 * PARAM3: u_char prot
 * PARAM4: unshort fragword
 *
 * RETURNS: void
 */

void
ip_bldhead(PACKET p, unsigned pid, u_char prot, unshort fragword)
{
   struct ip * pip;
   struct ip_socopts *sopts;

   /* prepend IP header to packet data */
   p->nb_prot -= sizeof(struct ip);       /* this assumes no send options! */
   p->nb_plen += sizeof(struct ip);

   pip = (struct ip*)p->nb_prot;

   pip->ip_ver_ihl = 0x45;       /* 2 nibbles; VER:4, IHL:5. */
   pip->ip_flgs_foff = fragword; /* fragment flags and offset */
   pip->ip_id = htons((unshort)pid);   /* IP datagram ID */
   pip->ip_len = htons((unshort)p->nb_plen);
   pip->ip_prot = prot;          /* install protocol ID (TCP, UDP, etc) */

   /* have IP_TOS or IP_TTL been set? */
   if ((sopts = p->soxopts))
   {
	  /* yup */
	  if (sopts->ip_ttl)
         pip->ip_time = sopts->ip_ttl;
	  else
         pip->ip_time = (u_char)IP_TTL;     /* default number of hops, really */
      pip->ip_tos = sopts->ip_tos;
   }
   else
   {
	  /* nope */
      pip->ip_time = (u_char)IP_TTL;       /* default number of hops, really */
      pip->ip_tos = IP_TOS_DEFVAL;
   }
   
   pip->ip_chksum = IPXSUM;      /* clear checksum field for summing */
   pip->ip_chksum = ~cksum(pip, 10);
}

/* FUNCTION: ip_write_internal()
 *
 * ip_write_internal() - common core for ip_write(), ip_raw_write().
 * Routes and writes supplied packet.
 *
 * PARAM1: PACKET p; packet to send
 *
 * RETURNS: Returns 0 if sent OK, ENP_SEND_PENDING (1) if 
 * waiting for ARP, else negative error code if error detected. 
 */
int
ip_write_internal(PACKET p)
{
   ip_addr firsthop;
#ifdef IP_MULTICAST
   PACKET pkt2;
   struct in_multi * inm;
   unsigned i;
   struct ip * pip;
#endif /* IP_MULTICAST */
   u_char intrsafe_buf;
   u_long limit;
   PACKET newpkt;
   unsigned maxbuflen;

#ifdef IP_MULTICAST
   /* If destination address is multicast, process multicast options */
   if (IN_MULTICAST(ntohl(p->fhost)))
   {
      if (p->imo != NULL)
         if (p->imo->imo_multicast_netp)
            p->net = p->imo->imo_multicast_netp;
         else
            p->net = iproute(p->fhost, &firsthop);
      else
      {
         for (i = 0; i < ifNumber; i++)
            if (nets[i]->n_mcastlist)
            {
               p->net = nets[i];
               break;
            }
      }

      /* Confirm that the outgoing interface supports multicast. */
      if ((p->net == NULL) || (p->net->n_mcastlist) == NULL)
      {
#ifdef   NPDEBUG
         if (NDEBUG & (IPTRACE|PROTERR))
         {
            dprintf("ip_write_internal: pkt:%p len%u to %u.%u.%u.%u, can't route\n",
                    p, p->nb_plen, PUSH_IPADDR(p->fhost));
            dtrap();
         }
#endif
         ip_mib.ipOutNoRoutes++;
         LOCK_NET_RESOURCE(FREEQ_RESID);
         pk_free(p);
         UNLOCK_NET_RESOURCE(FREEQ_RESID);
         IN_PROFILER(PF_IP, PF_EXIT);
         return (ENP_NO_ROUTE);
      }

      /*
       * If we belong to the destination multicast group
       * on the outgoing interface, and the caller did not
       * forbid loopback, put a copy of the packet on the
       * received queue.
       */
      inm = lookup_mcast(p->fhost, p->net);
      if ((inm != NULL) &&
          ((p->imo == NULL) || p->imo->imo_multicast_loop)) 
      {
         p->type = IPTP;
         pkt2 = ip_copypkt(p);
         if (pkt2)
         {
            LOCK_NET_RESOURCE(RXQ_RESID);
            putq(&rcvdq, (q_elt)pkt2);
            UNLOCK_NET_RESOURCE(RXQ_RESID);
            SignalPktDemux();
         }
      }

      /*
       * Multicasts with a time-to-live of zero may be looped-
       * back, above, but must not be transmitted on a network.
       * Also, multicasts addressed to the loopback interface
       * are not sent -- a copy will already have been looped
       * back above if this host actually belongs to the
       * destination group on the loopback interface.
       */
      pip = (struct ip *)(p->nb_prot);
      if ((pip->ip_time == 0) || 
          ((p->fhost & htonl(0xFF000000)) == IPLBA) || 
          (p->fhost == p->net->n_ipaddr))
      {
         LOCK_NET_RESOURCE(FREEQ_RESID);
         pk_free(p);
         UNLOCK_NET_RESOURCE(FREEQ_RESID);
         IN_PROFILER(PF_IP, PF_EXIT);
         return(SUCCESS);
      }

      firsthop = p->fhost;

      goto sendit;
   }

#endif /* IP_MULTICAST */

   /* if this is a broadcast packet, use the caller-selected network */
   if (p->fhost == 0xFFFFFFFF)
   {
      firsthop = p->fhost;
   }
   else
   {
      p->net = iproute(p->fhost, &firsthop);
      if (p->net == NULL)
      {
#ifdef   NPDEBUG
         if (NDEBUG & (IPTRACE|PROTERR))
         {
            dprintf("ip_write_internal: pkt:%p len%u to %u.%u.%u.%u, can't route\n",
                    p, p->nb_plen, PUSH_IPADDR(p->fhost));
            dtrap();
         }
#endif   /* NPDEBUG */
         ip_mib.ipOutNoRoutes++;
         LOCK_NET_RESOURCE(FREEQ_RESID);
         pk_free(p);
         UNLOCK_NET_RESOURCE(FREEQ_RESID);
         IN_PROFILER(PF_IP, PF_EXIT);
         return (ENP_NO_ROUTE);
      }
   }

#ifdef   IP_MULTICAST
sendit:  /* label used for multicast packets to skip routing logic */
#endif   /* IP_MULTICAST */

#ifdef   NPDEBUG
   if (NDEBUG & IPTRACE)
   {
      dprintf("ip_write: pkt[%u] to %u.%u.%u.%u,", 
       p->nb_plen, PUSH_IPADDR(p->fhost));
      dprintf(" route %u.%u.%u.%u\n", PUSH_IPADDR(firsthop));
   }
#endif

#ifdef   IPSEC
   IN_PROFILER(PF_IPSEC, PF_ENTRY);
   ipsec_outbound(&p);
   IN_PROFILER(PF_IPSEC, PF_EXIT);
   if (p == NULL)
   {
      IN_PROFILER(PF_IP, PF_EXIT);
      return ENP_LOGIC;
   }
   if (!(p->fhost))
   {
      p->fhost = ((struct ip *)ip_head(p))->ip_dest;
      if ((p->net = iproute(p->fhost, &firsthop)) == NULL)
      {
         ip_mib.ipOutNoRoutes++;
         LOCK_NET_RESOURCE(FREEQ_RESID);
         pk_free(p);
         UNLOCK_NET_RESOURCE(FREEQ_RESID);
         IN_PROFILER(PF_IP, PF_EXIT);
         return ENP_NO_ROUTE;
      }
   }
#endif   /* IPSEC */

   /* If the packet is being sent to the same interface it will be sent
    * from, short-cut things and just put it on the received queue.
    */
   if ((p->net->n_ipaddr == p->fhost) &&
       ((p->fhost & htonl(0xff000000)) != htonl(0x7f000000)))
   {
      if (!(p->net->n_flags & NF_NBPROT))
      {
         /* We can't do this reliably for interfaces whose drivers
          * don't set nb_prot to the start of the IP header -- 
          * if they don't do that, we can't reliably fake a MAC
          * header for arbitrary drivers here.  So we drop the
          * packet here.  If you want to be able to do this, 
          * change your driver(s) to set received packets' nb_prot 
          * to just past its MAC headers (i.e. to the start of the
          * IP or ARP protocol header) and nb_type to the protocol
          * type, and set the NF_NBPROT flag in its interfaces'
          * n_flags fields.
          */
#ifdef NPDEBUG
         dtrap();
#endif
         LOCK_NET_RESOURCE(FREEQ_RESID);
         pk_free(p);
         UNLOCK_NET_RESOURCE(FREEQ_RESID);
         IN_PROFILER(PF_IP, PF_EXIT);
         return ENP_LOGIC;
      }
      p->type = IPTP;
      UNLOCK_NET_RESOURCE(NET_RESID);
      LOCK_NET_RESOURCE(RXQ_RESID);
      putq(&rcvdq, (q_elt)p);
      UNLOCK_NET_RESOURCE(RXQ_RESID);
      LOCK_NET_RESOURCE(NET_RESID);
      IN_PROFILER(PF_IP, PF_EXIT);

      SignalPktDemux();

      return SUCCESS;
   }

   /* determine if the buffer that needs to be transmitted is interrupt-safe */
   intrsafe_buf = ((p->flags & PKF_INTRUNSAFE) ? 0 : 1);
   /* obtain the length of the largest interrupt-safe buffer that can be 
    * allocated via pk_alloc () */
   maxbuflen = pk_get_max_intrsafe_buf_len ();
   /* compute the maximum amount of data that can be sent in one chunk.  If
    * the original buffer is interrupt-safe, we just need to consider the
    * MTU of the egress interface.  If the original buffer is interrupt-
    * unsafe, we compute the MIN of the largest interrupt-safe buffer that
    * we can use and the MTU of the egress interface (since we must satisfy
    * both constraints) */
   if (!intrsafe_buf)
       limit = MIN(maxbuflen,p->net->n_mtu);
   else
       limit = p->net->n_mtu;

   if ((p->nb_plen + p->net->n_lnh) > limit)
   {
#ifdef IP_FRAGMENTS
      int err;
      err = ip_fragment(p, firsthop);
      IN_PROFILER(PF_IP, PF_EXIT);
      return(err);
#else
      dtrap();    /* this should be caught by programmers during development */
      LOCK_NET_RESOURCE(FREEQ_RESID);
      pk_free(p);
      UNLOCK_NET_RESOURCE(FREEQ_RESID);
      IN_PROFILER(PF_IP, PF_EXIT);
      return ENP_LOGIC;
#endif   /* IP_FRAGMENTS */
   }
   else
   {
      /* fragmentation is not required; check to see if we need to copy out of
       * an interrupt-unsafe buffer */ 
      if (!intrsafe_buf)
      {
         LOCK_NET_RESOURCE(FREEQ_RESID);
         newpkt = pk_alloc(p->nb_plen + p->net->n_lnh);
         UNLOCK_NET_RESOURCE(FREEQ_RESID);
         if (newpkt == 0)
         {
            /* can't allocate interrupt-safe buffer, so free the packet that 
             * we are working with */
            LOCK_NET_RESOURCE(FREEQ_RESID);
            pk_free(p);
            UNLOCK_NET_RESOURCE(FREEQ_RESID);
            IN_PROFILER(PF_IP, PF_EXIT);
            return ENP_NOBUFFER;
         }
         else
         {
            /* copy from interrupt-unsafe buffer into interrupt-safe buffer */
            newpkt->nb_prot = newpkt->nb_buff + p->net->n_lnh;
            MEMCPY(newpkt->nb_prot, p->nb_prot, p->nb_plen);
            /* setup various fields in the newly allocated PACKET structure */
            newpkt->nb_plen = p->nb_plen;
            newpkt->net = p->net;
            newpkt->fhost = p->fhost;
            /* free the original packet since it is no longer needed */
            LOCK_NET_RESOURCE(FREEQ_RESID);
            pk_free(p);
            UNLOCK_NET_RESOURCE(FREEQ_RESID);
            IN_PROFILER(PF_IP, PF_EXIT);
            /* send packet to MAC layer. This will try to resolve MAC layer addressing 
             * and send packet. ip2mac() can return SUCCESS, PENDING, or error codes. 
             */
            return(ip2mac(newpkt, firsthop));
         }
      }
      else
      {
         /* since the packet is in an interrupt-safe buffer, it can be passed to 
          * ip2mac () without any additional work. */
         IN_PROFILER(PF_IP, PF_EXIT);
         return(ip2mac(p, firsthop));
      }
   }
}

/* FUNCTION: ip_write()
 *
 * ip_write() - Fill in the internet header in the packet p and send 
 * the packet through the appropriate net interface. This will 
 * involve using routing. Call with PACKET p->nb_plen & p->nb_prot 
 * fields set to start of upper layer (e.g. UDP), and p->fhost set to 
 * target IP address. 
 * 
 * PARAM1: u_char prot
 * PARAM2: PACKET p
 *
 * RETURNS: Returns 0 if sent OK, ENP_SEND_PENDING (1) if 
 * waiting for ARP, else negative error code if error detected. 
 */

int
ip_write(
   u_char   prot,
   PACKET   p)
{
   struct ip * pip;
   u_char ttl;
   struct ip_socopts *sopts;

   IN_PROFILER(PF_IP, PF_ENTRY);       /* measure time in IP */

   ip_mib.ipOutRequests++;

   /* make room for IP header, and form a pointer to it (pip) */
   p->nb_prot -= sizeof(struct ip);
   p->nb_plen += sizeof(struct ip);
   pip = (struct ip*)(p->nb_prot);

   /* build the initial IP header:
    * IP source address (ip_src) and IP destination address (ip_dest)
    * should already have been filled in by upper-layer protocol
    */
   pip->ip_ver_ihl = 0x45;       /* 2 nibbles; VER:4, IHL:5. */
   pip->ip_flgs_foff = 0;        /* clear fragmentation info field */
   pip->ip_id = htons((unshort)uid);
   uid++;
   pip->ip_len = htons((unshort)(p->nb_plen));
   pip->ip_prot = prot;
   pip->ip_chksum = IPXSUM;      /* clear checksum field */
   ttl = IP_TTL;
#ifdef IP_MULTICAST
   if ((IN_MULTICAST(ntohl(p->fhost))) && (p->imo != NULL))
      ttl = p->imo->imo_multicast_ttl;
#endif /* IP_MULTICAST */

   /* have IP_TOS or IP_TTL been set? */
   if ((sopts = p->soxopts))
   {
	  /* yup */
	  if (sopts->ip_ttl)
         pip->ip_time = sopts->ip_ttl;
	  else
         pip->ip_time = ttl;
      pip->ip_tos = sopts->ip_tos;
   }
   else
   {
	  /* nope */
      pip->ip_time = ttl;
      pip->ip_tos = IP_TOS_DEFVAL;
   }
   
   /* checksum the IP header */
   pip->ip_chksum = ~cksum(pip, 10);

   /* do the actual write */
   return (ip_write_internal(p));
}


/* FUNCTION: ip_write2 ()
 *
 * This function is used to fill in the IP header of an
 * outgoing packet.  The first two parameters for this
 * function are identical to ip_write (), and the
 * third parameter is intended to allow the caller to
 * specify a list of IP options to be inserted into the
 * outgoing packet.  The caller is expected to provide
 * a buffer that is large enough for the outgoing
 * packet (including the IP options).  The third
 * parameter is expected to be a pointer to a list of
 * option types (each one byte long) to be inserted into 
 * the packet, and is terminated by the EOL (0x00) option.
 *
 * INPUT: (1) Protocol value to be inserted into the
 *            IP header field.
 *        (2) Packet to be transmitted.
 *        (3) Pointer to a list of option types (each one 
 *            byte long) to be inserted into the packet, 
 *            and is terminated by the EOL (0x00) option.
 *
 * OUTPUT: This function returns the return code from 
 *         ip_write_internal ().
 */

int ip_write2 (u_char prot, PACKET p, u_char * optp)
{
   struct ip * pip;
   u_char ttl;
   struct ip_socopts * sopts;
   u_char iphlen = sizeof (struct ip);
   u_char * tmpp;
   u_char iphlen_pad;
   u_char i;

   IN_PROFILER(PF_IP, PF_ENTRY);       /* measure time in IP */

   ip_mib.ipOutRequests++;

   /* compute the total length of the options requested */
   for (tmpp = optp; (*tmpp) != EOL_OPT; ++tmpp)
   {
      /* account for options, if any (caller has already created
       * adequate space for the requested option) */
      switch (*tmpp)
      {
         case IP_RTR_ALERT_OPT:
            iphlen += IP_RTR_ALERT_OPT_SIZE;
            break;
         default:
            break;
      }
   }

   /* compute the amount of padding required, if any (to ensure
    * that the IP header (including options) ends on a dword
    * (four byte) boundary */
   if (iphlen & 0x3)
      iphlen_pad = 4 - (iphlen & 0x3);
   else iphlen_pad = 0; /* no header padding required */

   /* the packet passed to ip_write2 () has its nb_prot set to
    * point to start of the protocol's (e.g., IGMP) data, and 
    * nb_plen set to the length of that data.  locate start of 
    * the IP header (account for IP options), and form a pointer 
    * to it (pip) */
   p->nb_prot -= (iphlen + iphlen_pad);
   /* add padding length to the total length of the IP datagram */
   p->nb_plen += (iphlen + iphlen_pad);
   pip = (struct ip *) (p->nb_prot);

   /* build the initial IP header:
    * IP source address (ip_src) and IP destination address (ip_dest)
    * should already have been filled in by upper-layer protocol
    */
   pip->ip_ver_ihl = ((IP_VER << 4) | ((iphlen + iphlen_pad) >> 2));
   pip->ip_flgs_foff = 0; /* clear fragmentation info field */
   pip->ip_id = htons((unshort)uid);
   uid++;
   pip->ip_len = htons((unshort)(p->nb_plen));
   pip->ip_prot = prot;
   pip->ip_chksum = IPXSUM;      /* clear checksum field */
   ttl = IP_TTL;
#ifdef IP_MULTICAST
   if ((IN_MULTICAST(ntohl(p->fhost))) && (p->imo != NULL))
      ttl = p->imo->imo_multicast_ttl;
#endif /* IP_MULTICAST */

   /* have TOS or TTL been set (via socket options)? */
   if ((sopts = p->soxopts))
   {
      if (sopts->ip_ttl)
         pip->ip_time = sopts->ip_ttl;
      else
         pip->ip_time = ttl;
      pip->ip_tos = sopts->ip_tos;
   }
   else
   {
      pip->ip_time = ttl;
      pip->ip_tos = IP_TOS_DEFVAL;
   }

   /* point to the start of the IP options, and insert the options */
   for (tmpp = (u_char *)(p->nb_prot + sizeof(struct ip)); *optp != EOL_OPT; ++optp)
   {
      /* caller has already provided adequate space for the requested options */
      switch (*optp)
      {
         case IP_RTR_ALERT_OPT:
            *((u_long *) tmpp) = htonl (IP_RTR_ALERT_OPT_DATA);
            /* this option is 4 bytes long */
            tmpp += IP_RTR_ALERT_OPT_SIZE;
            break;
         default:
            break;
      }
   }

   /* add one (or more) one-byte long End of Option options (if required) */
   for (i = 0; i < iphlen_pad; ++i) *(tmpp + i) = 0;
   
   /* checksum the IP header */
   pip->ip_chksum = ~cksum (pip, ((iphlen + iphlen_pad)/2));

   /* do the actual write */
   return (ip_write_internal (p));
}


#ifdef IP_RAW

/* FUNCTION: ip_raw_write()
 *
 * ip_raw_write() - send the packet p through the appropriate net
 * interface.  This will involve using routing.
 *
 * PARAM1: PACKET p; packet to send
 *
 * RETURNS: Returns 0 if sent OK, ENP_SEND_PENDING (1) if 
 * waiting for ARP, else negative error code if error detected. 
 */
int
ip_raw_write(PACKET p)
{
   struct ip * pip;

   IN_PROFILER(PF_IP, PF_ENTRY);       /* measure time in IP */

   ip_mib.ipOutRequests++;

   /* form a pointer to IP header (pip) */
   pip = (struct ip*)(p->nb_prot);
   
   /* if there's no IP id, give it one */
   if (pip->ip_id == 0)
   {
      pip->ip_id = htons((unshort)uid);
      uid++;
   }

   /* checksum the IP header */
   pip->ip_chksum = IPXSUM;      /* clear checksum field */
   pip->ip_chksum = ~cksum(pip, 10);

   /* do the actual write */
   return (ip_write_internal(p));
}

#endif /* IP_RAW */

#ifdef IP_FRAGMENTS

/* FUNCTION: ip_fragment_lc()
 *
 * Fragment an outgoing IP datagram that is too big for it's intended MAC.
 * This function is invoked from ip_fragment ().
 *
 * If ip_fragment () computes the total number of fragments that will be
 * created from the original IP datagram to be less than or equal to FRAG_-
 * SCHEME_SWITCH_THRESHOLD and the original IP datagram is interrupt-safe, 
 * then it invokes use ip_fragment_lc () for creating the child fragments.  
 * This decreases the amount of copying that needs to be done in those 
 * cases (as compared to ip_fragment ()); however, as the size of the
 * original IP datagram increases, ip_fragment () becomes comparatively 
 * more efficient.
 * 
 * PARAM1: PACKET p
 * PARAM2: ip_addr firsthop
 *
 * RETURNS: (1) ENP_LOGIC, if the MTU is deemed "inadequate"
 *          (2) ENP_RESOURCE, if the buffer for a generated fragment 
 *              cannot be allocated
 *          (3) Return code from the invocation of ip2mac () for any 
 *              generated fragment, if less than zero
 *          (4) Return code (greater than or equal to 0) from the 
 *              invocation of ip2mac () for the last generated fragment
 */

int
ip_fragment_lc(PACKET p, ip_addr firsthop)
{
   unsigned fragsize;   /* size for each fragment to send */
   unsigned maxipsize;  /* Max size of IP data on this media */
   unsigned numfrags;   /* number of fragmentswe will send */
   unsigned foffset;    /* offset of current fragment, in bytes */
   unshort  fraginfo;   /* IP header flags/fragment field */
   int      left;       /* bytes left to send */
   PACKET   pkt2;         /* packet for next fragment */
   int      e;
   struct ip * pip;
   int      iphlen;     /* IP header length */
   unshort  tmp_fraginfo   ;

   ip_mib.ipFragOKs++;     /* count packets we fragmented */
   pip = (struct ip *)(p->nb_prot);          /* get ptr to IP header */
   iphlen = (pip->ip_ver_ihl & 0xf) << 2;    /* and its length */
   left = (int)p->nb_plen - iphlen; /* bytes left to send in datagram */

   /* Calculate a good size for fragments. 
    * First, get biggest size an IP datagram can be, 
    * then divide our outbound packet's length by that to get 
    * the number of fragments, 
    * then divide our outbound packet's length by the number 
    * of fragments to get a fragment length that will result
    * in similarly-sized segments (remembering to round this
    * length up to a multiple of 8 because that's how the IP 
    * Fragment Offset field is scaled).
    */
   maxipsize = p->net->n_mtu - (iphlen + p->net->n_lnh + 8);   /* 8 == for rounding up */
   if (maxipsize < 64)  /* to small to use */
   {
      dtrap();    /* probably bad programming */
      return ENP_LOGIC;
   }
   numfrags = (p->nb_plen/maxipsize) + 1;    /* number of fragments */
   fragsize = (((p->nb_plen + (numfrags - 1)) / numfrags) + 7) & ~7; 
   foffset = 0;

   /* Now update from any previous info */
   tmp_fraginfo = ntohs(pip->ip_flgs_foff);     /* get current frag info*/
   if ( tmp_fraginfo & (~IP_FLG_MASK) )
   {
      /* When fragmenting an already fragmented packet, we need to
         add the initial offset */
      foffset = (tmp_fraginfo & (~IP_FLG_MASK))  ;
      foffset <<= 3;    /* Multiply by 8 to get "offset in num of bytes" */
   }

   /* Loop through IP data area, sending it as fragments */
   pkt2 = p;  /* init these to avoid compiler warnings */
   e = 0;
   while (left > 1)  /* more data left to send? */
   {
      p = pkt2;  /* move next fragment up */
      pip = (struct ip *)(p->nb_prot);
      p->nb_plen = min((int)fragsize, left) + iphlen; /* set size of fragment */
      left -= fragsize; /* decrement count of bytes left to send */

      /* set the IP datagram length */
      pip->ip_len = htons((unshort)p->nb_plen);

      /* build 16bit IP header field value for fragment flags & offset */
      fraginfo = (unshort)(foffset >> 3);    /* offset, in 8 byte chunks */
      fraginfo |= ((left>1)?IP_FLG_MF:0);    /* OR in MoreFrags flag */

      if ( tmp_fraginfo & IP_FLG_MF )             
      {
         /* As flag is set in main packet, it should be set in all
          * fragmented packets also 
          */
         fraginfo |= IP_FLG_MF;
      }

      pip->ip_flgs_foff = htons(fraginfo);   /* do htons macro on separate line */
      /* set up & save next fragment (pkt2) since ip2mac() will delete p */
      if (left > 1)
      {
         LOCK_NET_RESOURCE(FREEQ_RESID);
         pkt2 = pk_alloc(left + iphlen + MaxLnh);
         UNLOCK_NET_RESOURCE(FREEQ_RESID);
         if (!pkt2)
         {
            dtrap();
            LOCK_NET_RESOURCE(FREEQ_RESID);
            pk_free(p);
            UNLOCK_NET_RESOURCE(FREEQ_RESID);
            ip_mib.ipFragFails++;
            return ENP_RESOURCE;
         }

         pkt2->net = p->net;       /* copy critical parms to new packet */
         pkt2->fhost = p->fhost;
         /* Duplicate the IPHeader */
         MEMCPY(pkt2->nb_prot, p->nb_prot, iphlen);
         /* copy data for NEXT fragment from p to pkt2 */
         MEMCPY(pkt2->nb_prot + iphlen, p->nb_prot + iphlen + fragsize, left);
      }

      /* we've changed the IP header, so recalculate the checksum */
      pip->ip_chksum = IPXSUM;
      pip->ip_chksum = ~cksum(pip, 10);

      /* ip_dump(p); */

      e = ip2mac(p, firsthop);   /* send fragment in p */
      ip_mib.ipFragCreates++;
      if (e < 0)
      {
         if (left > 1) 
         {
            LOCK_NET_RESOURCE(FREEQ_RESID);
            pk_free(pkt2);
            UNLOCK_NET_RESOURCE(FREEQ_RESID);
         }
         return e;
      }
      foffset += fragsize;    /* offset for next fragment */
   }
   return e;
}

/* FUNCTION: ip_fragment ()
 *
 * This function fragments an IP datagram.  It can be invoked if either
 * of the following two conditions are met:
 *
 * (a) if the outgoing IP datagram is larger than the MTU of the egress 
 *     interface
 *
 * (b) if the outgoing IP datagram resides in a interrupt-unsafe buffer,
 *     and is larger than the minimum of the largest interrupt-safe 
 *     buffer and the MTU of the egress interface.
 *
 * In the latter case, the original datagram must be copied out into 
 * multiple interrupt-safe buffers (which are then provided to ip2mac 
 * () for transmission).
 *
 * If ip_fragment () computes the total number of fragments that will be
 * created from the original IP datagram to be less than or equal to FRAG_-
 * SCHEME_SWITCH_THRESHOLD and the original IP datagram is interrupt-safe, 
 * then it invokes ip_fragment_lc () for creating the child fragments.  
 * This decreases the amount of copying that needs to be done in those 
 * cases (as compared to ip_fragment ()); however, as the size of the
 * original IP datagram increases, ip_fragment () becomes comparatively 
 * more efficient.
 *
 * PARAM1: Pointer to packet to be transmitted
 * PARAM2: IP address of the next hop
 *
 * RETURNS: (1) 0, if the outgoing IP datagram has the DF bit set, or
 *          (2) Return code from ip_fragment_lc () if the conditions for
 *              the invocation of that function are met, or
 *          (3) ENP_NOBUFFER, if the buffer for a generated fragment 
 *              cannot be allocated
 *          (4) Return code from the invocation of ip2mac () for a 
 *              generated fragment, if less than zero
 *          (5) 0, if everything went okay
 */

int ip_fragment(PACKET p, ip_addr firsthop)
{
   unsigned maxbuflen;
   struct ip * pip;
   u_char iphlen;
   u_char l2hdr_len;
   u_short useable_payload_len;
   u_short total_payload_len;
   u_short last_payload_len;
   u_short maxl3_len;
   u_short num_frags;
   u_short i;
   u_short amt_to_copy;
   u_short parent_frag_offset;
   u_short parent_mf;
   PACKET newpkt;
   struct ip * newpip;
   int e;

   pip = ip_head(p);
   if ((ntohs(pip->ip_flgs_foff)) & IP_FLG_DF)
   {
      /* can't fragment a packet with the DF bit set */
      LOCK_NET_RESOURCE(FREEQ_RESID);
      pk_free(p);
      UNLOCK_NET_RESOURCE(FREEQ_RESID);
      ip_mib.ipFragFails++;
#ifdef FULL_ICMP
      icmp_destun(pip->ip_src, p->net->n_ipaddr, pip, DSTFRAG, p->net);
#endif   /* FULL_ICMP */
      return 0;
   }

   maxbuflen = pk_get_max_intrsafe_buf_len ();

   /* the maximum amount of data that will be sent to ip2mac ().  Please note that
    * our definition of MTU is different from the traditional definition of that
    * parameter (which excludes the data link layer header). */
   maxl3_len = (unsigned short int) (MIN(maxbuflen, ((unsigned) p->net->n_mtu)));

   /* get the length of the IP header of the original, unfragmented datagram */
   iphlen = ip_hlen(pip);

   /* obtain the useable payload length in an IP datagram (after accounting for the 
    * length of the IP and data link layer headers) */
   l2hdr_len = (u_char) p->net->n_lnh;
   useable_payload_len = maxl3_len - iphlen - l2hdr_len;

   /* IP fragments must always have a length that is a multiple of 8 bytes, so we
    * need to round 'payload_len' down to the nearest multiple of 8 bytes. */
   useable_payload_len &= (~((unsigned short int) 0x07));

   /* compute the number of packets that we need to send.  First compute the total
    * payload length of the "original" datagram (which could itself be a fragment). 
    */
   total_payload_len = (ntohs(pip->ip_len)) - iphlen;
   num_frags = (total_payload_len / useable_payload_len);
   if ((last_payload_len = (total_payload_len % useable_payload_len)) != 0)
   {
      /* we will have one additional (also last) fragment that is smaller than the 
       * other fragments */
      ++num_frags;
   }

   /* if we have a small number of fragments and the "original" IP datagram (i.e.,
    * the one passed to this function) is interrupt-safe, then we use 
    * ip_fragment_lc () for creating the child fragments.  This decreases the 
    * amount of copying that needs to be done in those cases (as compared to this 
    * function); however, for larger packets, ip_fragment () becomes more efficient 
    * (than ip_fragment_lc ()). */
   if ((num_frags <= FRAG_SCHEME_SWITCH_THRESHOLD) && (!(p->flags & PKF_INTRUNSAFE)))
   {
      return (ip_fragment_lc (p, firsthop));
   }

   parent_frag_offset = (((ntohs(pip->ip_flgs_foff)) & IP_EXTRACT_FOFF) << 3);
   parent_mf = ((ntohs(pip->ip_flgs_foff)) & IP_FLG_MF);

   /* irrespective of whether the parent buffer is interrupt-safe or not, we attempt 
    * to allocate new buffers for all of the fragments.  Iterate thru' the original 
    * datagram, copying fragments into the newly allocated data buffers.  If we want
    * to send the fragments in reverse order, we just need to "reverse" the FOR loop.
    */
   for (i = 0; i < num_frags; ++i)
   {
      LOCK_NET_RESOURCE(FREEQ_RESID);
      newpkt = pk_alloc (useable_payload_len + iphlen + l2hdr_len); 
      UNLOCK_NET_RESOURCE(FREEQ_RESID);

      if (newpkt == 0)
      {
         /* free the packet that we are working with */
         LOCK_NET_RESOURCE(FREEQ_RESID);
         pk_free(p);
         UNLOCK_NET_RESOURCE(FREEQ_RESID);
         ip_mib.ipFragFails++;
         return ENP_NOBUFFER;
      }
      else
      {
         /* copy the IP header from the original datagram into the fragment */
         MEMCPY((newpkt->nb_buff + l2hdr_len), p->nb_prot, iphlen);

         /* compute the amount of payload that needs to be copied into each 
          * child fragment */
         if (i < (num_frags - 1))
         {
            amt_to_copy = useable_payload_len;
         }
         else
         {
            amt_to_copy = ((last_payload_len == 0) ? useable_payload_len : last_payload_len);
         }
         MEMCPY((newpkt->nb_buff + l2hdr_len + iphlen), p->nb_prot + iphlen + (useable_payload_len * i), amt_to_copy);

         /* set up the various netbuf fields for the fragment */
         newpkt->nb_prot = newpkt->nb_buff + l2hdr_len;
         newpkt->nb_plen = iphlen + amt_to_copy;
         newpkt->net = p->net;
         newpkt->fhost = p->fhost;
         /* type and nb_tstamp fields are not used in the egress direction, but we set 'type' anyway */
         newpkt->type = htons(IPTP);

         /* now set the Total Length, Fragment Offset, and More Fragments fields */
         newpip = ip_head(newpkt);
         newpip->ip_len = htons(newpkt->nb_plen);
         /* the following statement will reset DF and MF bits */
         newpip->ip_flgs_foff = htons((parent_frag_offset + (useable_payload_len * i)) >> 3);

         /* set More Fragments flag appropriately as shown below.
          * (1) Parent fragment offset = 0, MF = 0: original unfragmented datagram (all 
          *     child fragments but the last will have the MF bit set)
          * (2) Parent fragment offset = x, MF = 1: first or middle fragment (FF or MF)
          *     (all child fragments will have the MF bit set)
          * (3) Parent fragment offset > 0, MF = 0: last fragment (LF) (all child fragments
          *     except the last will have the MF bit set) */
         if (i < (num_frags - 1))
         {
            newpip->ip_flgs_foff |= htons(IP_FLG_MF);
         }
         else
         {
            if (parent_mf)
            {
               newpip->ip_flgs_foff |= htons(IP_FLG_MF);
            }
         }

         /* finally, update the checksum */
         newpip->ip_chksum = IPXSUM;
         newpip->ip_chksum = ~cksum(newpip, (iphlen/2));

         ip_mib.ipFragCreates++;

         /* we're done; hand the packet off to ip2mac () */
         e = ip2mac (newpkt, firsthop);
         if (e < 0)
         {
            LOCK_NET_RESOURCE(FREEQ_RESID);
            pk_free(p);
            UNLOCK_NET_RESOURCE(FREEQ_RESID);
            ip_mib.ipFragFails++;
            return e;
         }
      } /* end if (successfully allocated PACKET for child fragment) */

   } /* end FOR (all child fragments) */

   /* free the parent buffer since all of the data from it have been copied 
    * out into the child fragments */
   LOCK_NET_RESOURCE(FREEQ_RESID);
   pk_free(p);
   UNLOCK_NET_RESOURCE(FREEQ_RESID);

   ip_mib.ipFragOKs++;     /* count packets we fragmented */

   return 0;
}

#endif   /* IP_FRAGMENTS */


/* FUNCTION: ip_mymach()
 *
 * Return the address of our machine relative to a certain foreign host. 
 *
 * 
 * PARAM1: ip_addr host
 *
 * RETURNS: The address of our machine relative to a 
 * certain foreign host. 
 */

ip_addr
ip_mymach(ip_addr host)
{
#ifdef MULTI_HOMED
   ip_addr temp;
   NET tnet;
#endif

#ifdef   IP_LOOPBACK
   /* if we're sending to a loopback address, give a loopback address */
   if ((host & htonl(0xFF000000)) == IPLBA)
      return host;
#endif   /* IP_LOOPBACK */

#ifndef MULTI_HOMED /* single static interface */
   USE_ARG(host);
   return(nets[0]->n_ipaddr);   /* always use address from only net */
#else   /* MULTI_HOMED */
   tnet = iproute(host, &temp);
   if (tnet == 0)
   {
#ifdef   NPDEBUG
      if (NDEBUG & (PROTERR|INFOMSG))
         dprintf("IP: Couldn't route to %u.%u.%u.%u\n", PUSH_IPADDR(host));
#endif   /* NPDEBUG */
      return 0L;
   }
   return tnet->n_ipaddr;
#endif /* MULTI_HOMED */
}


#ifdef NPDEBUG

/* FUNCTION: ip_dump()
 *
 * dumps an IP header to the default output.
 * 
 * PARAM1: PACKET p
 *
 * RETURNS: void
 */

void
ip_dump(PACKET p)
{
   struct ip * pip;
   unsigned char * cp;
   unsigned short xsum, osum;

   pip = ip_head(p);
   osum = pip->ip_chksum;
   pip->ip_chksum = 0;
   xsum = ~cksum(pip, ip_hlen(pip) >> 1);

   if (osum != xsum)          /* trap here if checksum is wrong */
   {
      dtrap();
   }

#ifdef NOTDEF
   /* dtrap() is fatal in the default Windows implementation, so
    * we comment it out */
   dtrap();       /* use debugger to view variables & packet */
#endif

   ns_printf(NULL ,"IP packet header:\n");
   cp = (unsigned char *)pip;    /* make char pointer for bitmasks */
   ns_printf(NULL ,"ver/hlen: %02x, TOS: %02x, len: %04x, id: %04x\n",
    *cp, *(cp+1), htons(pip->ip_len), htons(pip->ip_id));
   ns_printf(NULL ,"flags/offs: %04x, TTL %02x, protocol: %02x, cksum: %04x (%s)\n",
    htons(*(unshort*)(cp+6)), pip->ip_time, pip->ip_prot, 
    htons(osum), (osum==xsum)?"ok":"bad");
   ns_printf(NULL ,"src: %u.%u.%u.%u  ", PUSH_IPADDR(pip->ip_src));
   ns_printf(NULL ,"dest: %u.%u.%u.%u\n", PUSH_IPADDR(pip->ip_dest));

   pip->ip_chksum = osum;     /* fix what we clobbered */
}
#endif   /* NPDEBUG */



/* FUNCTION: iproute()
 *
 * iproute() - Route a packet. Takes the internet address that we 
 * want to send a packet to and returns the net interface to send it 
 * through. 
 *
 * 
 * PARAM1: ip_addr host
 * PARAM2: ip_addr * hop1
 *
 * RETURNS: Returns NULL when unable to route, else returns a NET pointer.
 */

NET
iproute(ip_addr host, ip_addr * hop1)
{
   int      i;
   NET      ifp;
#ifdef IP_ROUTING
   RTMIB    rtp;
#endif   /* IP_ROUTING */

   if (host == 0L)      /* Sanity check parameter. */
      return NULL;

#ifdef IP_ROUTING
#ifndef BTREE_ROUTES
   /* its possible for this function to get called before the whole 
    * stack has been initialized (tk_yield() gets called out of the 
    * dialer code as part of PPP initialization), one symptom of 
    * which is the routing table not being present yet. if this 
    * happens, quit. 
    */
   if (rt_mib == NULL)
      return NULL;
#endif   /* BTREE_ROUTING */

   /* see if the host matches the cached route */
   if (cachedRoute)     /* don't test this if route is null  */
   {
      if (cachedRoute->ipRouteDest == host)    /* exact match */
      {
         *hop1 = cachedRoute->ipRouteNextHop;   /* fill in nexthop IP addr */
         cachedRoute->ipRouteAge = cticks;      /* timestamp route entry */
         return(cachedRoute->ifp); /* net to send on */
      }
   }

   rtp = rt_lookup(host);
   if(rtp)
   {
      cachedRoute = rtp;
      *hop1 = rtp->ipRouteNextHop;  /* fill in IP dest (next hop) */
      return(rtp->ifp);             /* return pointer to net */
   }
#endif   /* IP_ROUTING */

   /* Check if it is on one of my nets 
    *
    * A note about style: for single-homed builds, we don't need to
    * iterate through netlist, just look at the one-and-only net on
    * the list.  So we always set up for the first net (which is also
    * the initial state for the for loop that iterates through the
    * list), but only build the iterator for multi-homed systems
    * because single-homed systems are often memory-limited systems as
    * well.  
    */
   ifp = (NET)(netlist.q_head);
   i = 0;
#ifdef MULTI_HOMED
   for(; ifp; ifp = ifp->n_next, i++)
#endif /* MULTI_HOMED */
   {
      if((ifp->snmask != 0) &&      /* skip ifaces with no IP or subnet mask set */
         (ifp->n_ipaddr != 0) && 
         ((ifp->n_ipaddr & ifp->snmask) == (host & ifp->snmask)))
      {
#ifdef IP_ROUTING
         /* make a cached Route entry for next time */
         cachedRoute = add_route(host, 0xFFFFFFFF, host, i, IPRP_OTHER);
#ifdef NPDEBUG
         if (cachedRoute == NULL)
            dtrap();
#endif   /* NPDEBUG */
#endif   /* IP_ROUTING */
         *hop1 = host;
         return ifp;
      }
   }

#ifdef IP_LOOPBACK
   /* If it's a loopback packet (dest 127.x) and we got here, then 
    * there's no loopback device. Route the packet to nets[0] and let 
    */
   if ((host & htonl(0xFF000000)) == IPLBA)
   {
      *hop1 = host;
      return nets[0];
   }
#endif   /* IP_LOOPBACK */

   /* The host isn't on a net I'm on, so send it to the default 
    * gateway on the first net which has one. 
    */
   ifp = (NET)(netlist.q_head);
#ifdef MULTI_HOMED
   for(; ifp; ifp = ifp->n_next)
#endif   /* MULTI_HOMED */
   {
      /* Check if this net has a gateway */
      if(ifp->n_defgw)
      {
         *hop1 = ifp->n_defgw;
         return ifp;
      }
   }

#ifdef STRICT_SUBNETTING
   return NULL ;
#else
   /* if no gateway is set, then change the first hop address to the 
    * host we're trying to route to. this is just a kluge to make 
    * this work with arp routing. otherwise, we would try to return 
    * some sort of error indication. 
    */
   *hop1 = host;
   return((NET)(netlist.q_head));
#endif   /* STRICT_SUBNETTING */
}


#ifdef NO_IP_MACROS     /* Replace some macros with functions */

/* FUNCTION: ip_data()
 *
 * Return pointer to IP data area which starts after the IP header. This accounts
 * for any IP options which might be appended to the packet.
 *
 * PARAM1: struct ip * pip
 *
 * RETURNS: 
 */

unsigned char *   
ip_data(struct ip * pip)
{
   u_char   ihl;

   ihl = pip->ver_ihl_tos[IP_VER_IHL];    /* extract ip header len */
   ihl &= 0x0f;   /* mask out version bits, ihl normally == 5 */
   return((char *)(pip) + (ihl << 2)); /* convert to byte count & pointer */
}


/* FUNCTION: ip_optlen()
 * 
 * PARAM1: struct ip * pip
 *
 * RETURNS: 
 */

unsigned
ip_optlen(struct ip * pip)
{
   u_char   ihl;

   ihl = pip->ver_ihl_tos[IP_VER_IHL];    /* extract ip header len */
   ihl &= 0x0f;   /* mask out version bits, ihl normally == 5 */
   return((ihl - IP_IHL) << 2);
}

#endif   /* NO_IP_MACROS */


/* FUNCTION: ip_copypkt()
 * 
 * Makes a copy of a packet in a new packet buffer.
 *
 * PARAM1: PACKET p
 *
 * RETURNS: a pointer to the new copy of the packet,
 *          or NULL if no packet buffer could be allocated
 */
PACKET
ip_copypkt(PACKET p)
{
   PACKET np;
   int len;

   /* figure out how much we need to copy from the packet, 
    * and allocate a new buffer to hold it 
    */
   len = p->nb_plen + (p->nb_prot - p->nb_buff);
   LOCK_NET_RESOURCE(FREEQ_RESID);
   np = pk_alloc(len);
   UNLOCK_NET_RESOURCE(FREEQ_RESID);
   if (np == NULL)
   {
#ifdef NPDEBUG
      dprintf("ip_copypkt(): unable to obtain packet (len %d)\n", len);
#endif
      return NULL;
   }

   /* copy packet data into new buffer */
   MEMCPY(np->nb_buff, p->nb_buff, len);

   /* copy relevant packet fields */
   np->nb_prot = np->nb_buff + (p->nb_prot - p->nb_buff);
   np->nb_plen = p->nb_plen;
   np->net = p->net;
   np->fhost = p->fhost;
   np->type = p->type;
   np->nb_tstamp = p->nb_tstamp;

   /* return pointer to the copy */
   return np;
}
