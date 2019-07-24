/*
 * FILENAME: pcycles.c
 *
 * Copyright  2001 By InterNiche Technologies Inc. All rights reserved
 *
 * Routines to measure processing time for various types of packets.
 *
 * MODULE: MISCLIB
 *
 *
 * PORTABLE: yes
 */


#include "ipport.h"

#ifdef   PKT_CYCLES     /* whole file can be ifdeffed */
    
#ifndef USE_PROFILER
#error need USE_PROFILER to use PKT_CYCLES
#endif

#ifndef DYNAMIC_IFACES
#error need DYNAMIC_IFACES to use PKT_CYCLES
#endif

#include "in_utils.h"
#include "menu.h"
#include "profiler.h"
#include "netbuf.h"
#include "net.h"
#include "ip.h"
#include "icmp.h"
#include "udp.h"
#include "tcpport.h"
#include "tcp.h"

#ifdef NOTDEF
#undef INCLUDE_TCP
#define DEBUG_PCKTCP
#endif

extern   void     ip_bldhead(PACKET p, 
   unsigned pid, u_char prot, unshort fragword);
extern   int   deflength;

/* pseudo iface to generate traffic and measure respose times */
NET      pc_if;
static int     pcn_rx_mode = 0;     /* 0 == direct to ip_rcv(); 1 == via pktdemux */
static u_long  pcn_starttime;       /* pticks at start of current transaction */
static int     pcn_active;          /* TRUE if we're activly testing */

/* Per-session send and receive counters. They are named from 
 * the stack's perspective, i.e. a packet passed to this code
 * from the stack is a "send"
 */
static   u_long   pcn_sends;           /* echo replies in current session */
static   u_long   pcn_recvs;           /* echo replies in current session */

static   char *   cyname = "cy0";      /* iface name */
static   int      pcn_ipid = 0;        /* ID for IP packets */
static   u_char   test_prot;           /* protocol of current test */
static   u_long   calibration = 0;     /* pticks/sec */

/* TCP port numbers for connection */
static   unshort  client_port = 1024;
static   unshort  echo_port = 7;

/* sequence and ack numbers, from client's perspective */
static   u_long   tcpseq;
static   u_long   tcpack;

static   u_char   tcp_sentflags;    /* last flags passed to pkt_send() */
static   PACKET   pkc_bldtcp(int ip_length, u_char flags);


/* internal to this file */
int      pkc_rep(void * pio, u_char prot);
PACKET   pkc_bldip(u_char prot, int length);



/* MAC routines to implement to pseudo net */

/* Standard MAC net initialization routine */

int
pcn_n_init(int iface)
{
   dtrap();   /* never called??? */
   USE_ARG(iface);
   return 0;
}

/* Standard MAC PACKET send routine */

int
pcn_pkt_send(struct netbuf * pkt)
{
   pcn_sends++;         /* echo replies in current session */

   if(test_prot == IPPROTO_TCP)
   {
      struct ip * pip;
      struct tcphdr * ptcp;
      int datalen;

      /* extract and save TCP fields needed to interact with stack */
      pip = (struct ip *)(pkt->nb_prot);
      ptcp = (struct tcphdr *)ip_data(pip);
      datalen = (int)htons(pip->ip_len);
      datalen -= (sizeof(struct ip) + ((ptcp->th_doff & 0xf0) >> 2));
      tcp_sentflags = ptcp->th_flags;

      tcpseq = htonl(ptcp->th_ack);    /* next SEQ is after his ACK */
      tcpack = htonl(ptcp->th_seq)  + datalen;  /* ack data */

#ifdef DEBUG_PCKTCP
      dprintf("send (tcp) pkt:%p flags: %4x, seq:%lx ack %lx pkt-len %d\n", 
        pkt, tcp_sentflags, tcpseq, tcpack, pkt->nb_plen);
#endif /* DEBUG_PCKTCP */

      /* if he sent RESET bit then break the test loop */
      if(tcp_sentflags & TH_RST)
      {
         pcn_active = FALSE;
         return 0;
      }

      /* if he sent a SYN or FIN we need to add it to data sequence */
      if(tcp_sentflags & (TH_SYN|TH_FIN))
      {
         tcpack++;
      }

      /* if this is a packet with no data then the stack has not saved 
       * a pointer to it for a later retry. This means we must free
       * it now:
       */
      if(htons(pip->ip_len) ==
         (((pip->ip_ver_ihl & 0x0F) + GET_TH_OFF((*ptcp))) << 2))
      {
         LOCK_NET_RESOURCE(FREEQ_RESID);
         pk_free(pkt);
         UNLOCK_NET_RESOURCE(FREEQ_RESID);
      }

      /* If the test loop is done then this is probably a straggling
       * FIN pkt or a retry of some sort. Ack it now. We deliver the
       * packet via the rcvdq to avoid potential intinite recursion
       * issues.
       */
      if(!pcn_active)
      {
         pkt = pkc_bldtcp(40, TH_ACK);   /* build ack of FIN */
         if(pkt)
         {
            putq(&rcvdq, (qp)pkt);     /* queue ACK of FIN for stack.  */
            SignalPktDemux();          /* make sure it spins */
         }
      }
   }

   return 0;
}

/* Standard MAC close routine */

int
pcn_n_close(int iface)
{
   NET ifp = if_getbynum(iface);
   if(ifp == NULL)
      return ENP_PARAM;

   ifp->mib.ifAdminStatus = ifp->mib.ifOperStatus = NI_DOWN;
   return 0;
}



/* callback routine for cycle net creation */
int
pcn_init(NET ifp, void * bindinfo)
{
   int   iface;
   
   ifp->n_init = pcn_n_init;           /* set normal callback routines */
   ifp->pkt_send = pcn_pkt_send;
   ifp->n_close = pcn_n_close;
   ifp->n_lnh = 0;                     /* No mac header */
   ifp->mib.ifPhysAddress = (u_char*)"";  /* null MAC address */
   ifp->mib.ifType = SLIP;             /* fake SLIP - simple and headerless */
   ifp->n_lnh = 0;                     /* net's local net header  size */
   ifp->n_mtu = 1500;                  /* largest legal buffer size */
   ifp->mib.ifType = LOOPIFTYPE;
   ifp->mib.ifDescr = (u_char*)"Packet timing test device";
   ifp->n_flags = NF_NBPROT;

   /* set a default local network address */
   ifp->n_ipaddr = htonl(0xC0800101);
   ifp->snmask = htonl(0xFFFFFF00);

   /* add a locally configured route to this iface */
   iface = if_netnumber(ifp);
   if(add_route(ifp->n_ipaddr, ifp->snmask, ifp->n_ipaddr, 
            iface, IPRP_LOCAL) == NULL)
   {
      return ENP_RESOURCE;
   }

   USE_VOID(bindinfo);
   return 0;
}


int
pkc_start(void * pio)
{
   int      err;        /* error holder */

   err = ni_create(&pc_if, pcn_init, cyname, NULL);
   if(err)
   {
      ns_printf(pio, "error %d creating cycle measure device\n", err);
      return (err);
   }

   ns_printf(pio, "Created cycle measure device %s\n", cyname);
   return 0;
}

   /* calibrate pticks to real time */

int
pkc_calibrate(void * pio)
{
   u_long   ptime;      /* scratch ptick holder */
   u_long   ctime;      /* end ctick for ptick calibration */
   int      seconds;    /* number of seconds to run */
   char *   arg   =  nextarg( ((GEN_IO)pio)->inbuf );

   seconds = atoi(arg);
   if(seconds <= 0)
   {
      ns_printf(pio, "usage: calibrate X (X is number of seconds)\n");
      return -1;
   }
   ns_printf(pio, "calibrating pticks...");

   ctime = cticks;
   while(cticks == ctime)     /* wait for next ctick to start */
      tk_yield();

   ptime = get_ptick();       /* get current value of fast hardware timer */
   ctime = cticks + (TPS * (u_long)seconds); 
                              /* get slow real time stop value */

   while(ctime != cticks)     /* wait desired number of seconds */
      tk_yield();

   ptime = get_ptick() - ptime;    /* get elapsed pticks */

   calibration = ptime/seconds;
   ns_printf(pio, "done, %lu pticks/sec\n", calibration);
   
   return 0;
}


int
pkc_del(void * pio)
{
   int err;
   
   /* Since this is not real hardware we don't really need a gracefull
    * shutdown. Just delete the dynamic iface
    */
   if(pc_if == NULL)
      err = -1;
   else
   {
      err = ni_delete(pc_if);
      pc_if = NULL;
   }

   ns_printf(pio, "Deleting cycle iface, code\n");
   return(err);
}

/* toggle pcn_rx_mode. */

int
pkc_mode(void * pio)
{
   pcn_rx_mode = !pcn_rx_mode;

   ns_printf(pio, "Pkt Cycle tests set to use %s.\n",
      pcn_rx_mode?"rcvdq and pktdemux":"direct ip_rcv call");

   return 0;
}



/* pkc_bldip() - build the fake received packet IP layer. Building the
 * transport header is handled in the various routine below. IP header
 * is built at pkt->nb_prot and nb_plen is adjusted.
 */

PACKET
pkc_bldip(u_char prot, int length)
{
   PACKET      pkt;
   struct ip * pip;

   LOCK_NET_RESOURCE(FREEQ_RESID);
   pkt = pk_alloc(length + MaxLnh);
   UNLOCK_NET_RESOURCE(FREEQ_RESID);
   if(!pkt)
      return NULL;
   pkt->net = pc_if;
   pkt->nb_plen = length;
   pkt->nb_prot = pkt->nb_buff + MaxLnh + sizeof(struct ip);
   pkt->type = IP_TYPE;

   pip = (struct ip *)pkt->nb_prot;

   ip_bldhead(pkt, pcn_ipid++, prot, 0);
   pip = (struct ip *)pkt->nb_prot;
   pip->ip_len = htons((unshort)length);

   pip->ip_src = pip->ip_dest = pc_if->n_ipaddr;

   pip->ip_chksum = 0;
   pip->ip_chksum = ~cksum(pip, 10);

   return pkt;
}


void
pck_xchgpkt(PACKET pkt)
{
   if(pcn_rx_mode == 0)    /* direct feed to IP */
      ip_rcv(pkt);
   else     /* send it up through the rcvdq */
   {
      putq(&rcvdq, (qp)pkt);  /* enqueue pkt */
      SignalPktDemux();       /* wake demuxer task */
      tk_yield();             /* let demux task spin */
   }
}

#ifdef TCP_ECHOTEST  /* next few routines needed only for TCP */

extern SOCKTYPE esvr_sock;    /* Echo server socket in tcp_echo.c */

/* pkc_bldtcp() - build a tcp packet based on passed parameters. This
 * allocates it's own packet buffer and returns it.
 *
 * Returns NULL if error
 */

static PACKET
pkc_bldtcp(int ip_length, u_char flags)
{
   PACKET pkt;
   struct tcphdr * ptcp;

   pkt = pkc_bldip(IPPROTO_TCP, ip_length);
   if(!pkt)
      return NULL;

   ptcp = (struct tcphdr *)(pkt->nb_prot + 20);

   ptcp->th_sport = htons(client_port);
   ptcp->th_dport = htons(echo_port);
   ptcp->th_seq = htonl(tcpseq);
   ptcp->th_ack = htonl(tcpack);
   ptcp->th_doff = 0x50;
   ptcp->th_win = htons(0x2000);
   ptcp->th_urp = 0;
   ptcp->th_sum = 0;
   ptcp->th_flags = flags;
   ptcp->th_sum = tcp_cksum((struct ip *)(pkt->nb_prot));

#ifdef DEBUG_PCKTCP
   dprintf("bldpkt (tcp) flags: %4x, seq:%lx ack %lx, ip-len:%d\n", 
        flags, tcpseq, tcpack, ip_length);
#endif   /* DEBUG_PCKTCP */

   return pkt;
}

/* pck_tcpconn() - make the tcp echo connection. 
 */

int
pck_tcpconn(void * pio)
{
   PACKET pkt;
   u_long conn_tmo;

   client_port++;          /* use a different port each time */
   tcpseq = 0x00100000;    /* reset sequence number */

    /* make and send a syn packet */
   pkt = pkc_bldtcp(40, TH_SYN);
   if(!pkt)
   {
      ns_printf(pio, "pck_tcpconn: no pkt\n");
      return -1;
   }
   pck_xchgpkt(pkt);

   /* Make sure that stack responded properly */
   if(tcp_sentflags != (TH_SYN|TH_ACK))
   {
      ns_printf(pio, "pck_tcpconn: no syn-ack\n");
      return -1;
   }
   
   /* make and send the ACK packet */
   pkt = pkc_bldtcp(40, TH_ACK);
   if(!pkt)
   {
      ns_printf(pio, "pck_tcpconn: no pkt\n");
      return -1;
   }
   pck_xchgpkt(pkt);

   /* Give server a chance to accept the connection */
   conn_tmo = cticks + TPS;
   while(conn_tmo > cticks)
   {
      if(esvr_sock != INVALID_SOCKET)
         break;
      tk_yield();    /* spin while TCP server socket is invalid */
   }

   return 0;
}


/* close tcp connection */

int
pck_tcpfin(void * pio)
{
   PACKET   pkt;
   u_long   tmpseq;

   /* make and send a FIN packet */
   pkt = pkc_bldtcp(40, TH_FIN|TH_ACK);
   if(!pkt)
   {
      ns_printf(pio, "pck_tcpfin: no pkt\n");
      return -1;
   }
   tmpseq = tcpseq;     /* save pre-fin sequence number */
   pck_xchgpkt(pkt);

   /* see if the stack responded with a FIN bit */
   if((tcp_sentflags & TH_FIN) == 0)
   {
      /* NicheStack will ACK our FIN before sending his own FIN.
       * If this happens then return without error. His FIN 
       * will be acked by the pkt_send() routine.
       */
      if(tcpseq == (tmpseq + 1))
         return 0;
      ns_printf(pio, "pck_tcpfin: no ACK of FIN\n");
      return -1;
   }

   /* ACK his FIN to complete the close */
   pkt = pkc_bldtcp(40, TH_FIN);
   if(!pkt)
   {
      ns_printf(pio, "pck_tcpfin: no pkt\n");
      return -1;
   }
   pck_xchgpkt(pkt);
   return 0;
}

/* tcp_hdrfixup() - prepare tcp header for next itteration */

static void
tcp_hdrfixup(char * pbuf)
{
   struct   tcphdr * ptcp;
   u_long   seq_change;
   u_long   ack_change;
   u_long   tcpsum;

   /* Change the sequence and ACK numbers to reflect the data sent and
    * received. We get this info from tcpseq and tcpack values set in
    * pck_pkt_send. Since we change the TCP header we also need to fix
    * the checksum.
    */
   ptcp = (struct tcphdr *)(pbuf + sizeof (struct ip));
   seq_change = tcpseq - htonl(ptcp->th_seq);
   seq_change = htonl(seq_change);
   ack_change = tcpack - htonl(ptcp->th_ack);
   ack_change = htonl(ack_change);

   tcpsum = ((~(u_long)ptcp->th_sum) & 0x0000ffff);
   tcpsum += (u_short)(seq_change >> 16);
   tcpsum += (u_short)(seq_change & 0x0000ffff);
   tcpsum += (u_short)(ack_change >> 16);
   tcpsum += (u_short)(ack_change & 0x0000ffff);

   while(tcpsum  & 0xffff0000)   /* if overflow, add high bits back in */
      tcpsum = (tcpsum & 0x0000ffff) + (tcpsum >> 16);

   ptcp->th_seq = htonl(tcpseq);    /* set the new seq number */
   ptcp->th_ack = htonl(tcpack);    /* set the new ACK number */
   ptcp->th_sum = ~(unshort)tcpsum; /* set new checksum */
}
#endif /* TCP_ECHOTEST */

void
pc_printstat(void * pio, u_long itterations, u_long pticks, char * typetext)
{
   u_long mult;      /* multiplier to avoid data over flow */
   u_long pt_pkts;
   u_long msecs;
   u_long decimal;

   ns_printf(pio, "Ran %lu reps in %lu ticks; ", itterations, pticks);
   if(calibration && pticks)
   {
      if(pticks/itterations < 1)
         mult = 10000000;
      else if (pticks/itterations < 1000)
         mult = 10000;
      else if (pticks/itterations < 1000000)
         mult = 10;
      else
         mult = 1;

      if(mult <= 10)
         pt_pkts = (pticks * mult) / itterations;  /* pticks/packet (scaled) */
      else
         pt_pkts = pticks * (mult / itterations);  /* pticks/packet (scaled) */

      /* figure microseconds per pkt. First check for 32 bit overflow... */
      if((calibration / (10000000 / mult)) > 0)
      {

         msecs = pt_pkts / (calibration / (10000000 / mult));
         decimal = msecs;
         msecs /= 10;
         decimal -= (msecs * 10);

         ns_printf(pio, "%lu.%lu u_secs per %s", 
            msecs, decimal, typetext );
      }
   }
   ns_printf(pio, "\n");
   return;
}

/* mock up an incoming ping packet and pass it up to IP layer. */

int
pkc_ping(void * pio)
{
   return(pkc_rep(pio, ICMP_PROT));
}
   

int
pkc_rep(void * pio, u_char prot)
{
   u_long   itterations;   /* number of pings (from cmd line) */
   struct   ping * png;    /* ping (ICMP) header to pass to stack */
   struct   udp * pup;     /* UCP header to pass to stack */
   u_long   pticks;        /* elpased profiler (fast) ticks */
   int      ip_len;        /* length of IP packet */
   int      err;           /* return error holder */
   PACKET   pkt;           /* packet to send */
   char     pbuf[40];      /* packet header buffer */
   int      headerslen;    /* total length of headers */
   char *   typetext;      /* printable pkt type */
   char *   arg   =  nextarg( ((GEN_IO)pio)->inbuf );
#ifdef DEBUG_PCKTCP
   struct tcphdr * ptcp;
#endif

   if(pc_if == NULL)
   {
      ns_printf(pio, "Open test device with pcmake first.\n");
      return -1;
   }

   if(*arg != 0)
      itterations = (u_long)atol(arg);
   else
      itterations = 1;     /* default to one */

   test_prot = prot;       /* save prot for pcn_pkt_send() call */
   pcn_active = TRUE;      /* now officially testing */

   switch (prot)           /* more per-protocol setup */
   {
   case ICMP_PROT:
      {
      int pingsize = sizeof(struct ping) + deflength;
      headerslen = sizeof(struct ip) + sizeof(struct ping);
      ip_len = deflength + headerslen;    /* allow for IP and ICMP headers */
      typetext = "ping";
      pkt = pkc_bldip(prot, ip_len);   /* get basic IP pkt */
      /* build icmp header after IP */
      png = (struct ping *)(pkt->nb_prot + sizeof(struct ip));
      png->ptype = ECHOREQ;
      png->pseq = 1;
      png->pchksum = 0;
      if (ip_len & 1)  /* pad odd length packets  for checksum routine */
         *(pkt->nb_prot + sizeof(struct ip) + pingsize) = 0;
      png->pchksum = ~cksum(png, (pingsize + 1) >> 1);
      break;
      }
   case UDP_PROT:
      {
      unshort udplen;
      headerslen = sizeof(struct ip) + sizeof(struct udp);
      ip_len = deflength + headerslen;    /* allow for IP and ICMP headers */
      udplen= ip_len - sizeof(struct ip);
      typetext = "UDP echo";
      pkt = pkc_bldip(prot, ip_len);   /* get basic IP pkt */
      /* build UDP header after IP */
      pup = (struct udp *)(pkt->nb_prot + sizeof(struct ip));
      pup->ud_dstp = htons(7);            /* dest port is echo */
      pup->ud_srcp = htons(udp_socket()); /* get a free port for source */
      pup->ud_len = htons(udplen);        /* IP length minus IP header */
      pup->ud_cksum = 0;                  /* no cksum, leave that for TCP */
      break;
      }
#ifdef TCP_ECHOTEST
   case IPPROTO_TCP:
      err = pck_tcpconn(pio);        /* make the tcp echo connection */
      if(err)
      {
         ns_printf(pio, "TCP setup error\n");
         return -1;
      }
      headerslen = sizeof(struct ip) + sizeof(struct tcphdr);
      ip_len = deflength + headerslen;    /* allow for IP and transport headers */
      typetext = "TCP echo";
      pkt = pkc_bldtcp(ip_len, TH_ACK);
      break;
#endif /* TCP_ECHOTEST */
   default:
      dtrap();    /* bad prot arg */
      return -1;
   }

   MEMCPY(pbuf, pkt->nb_prot, headerslen);   /* save copy of headers */

   ns_printf(pio, "Sending %d %ss into stack...\n", itterations, typetext);
   
   pcn_starttime = get_ptick();
   pcn_sends = pcn_recvs = 0;       /* clear counters */
   err = 0;

   /* main loop for sending packets. pcn_sends is incremented by the 
    * pck_pkt_send routine, which should be called before pck_xchgpkt()
    * returns. Since pck_pkt_send() does not free the packet, just keep
    * sending the same one. This saves a lot of allocating packets and 
    * copying.
    */
   while(pcn_recvs++ < itterations)
   {
      pck_xchgpkt(pkt);
      if(pcn_sends < pcn_recvs)    /* make sure we got response */
      {
         err = -1;
         ns_printf(pio, "Lost pkt #%lu, aborting.\n", pcn_recvs);
         break;
      }
      if(!pcn_active)      /* see if we got reset or something */
      {
         err = -1;
         ns_printf(pio, "reset, aborting.\n");
         break;
      }
      pc_if->mib.ifInNUcastPkts++;
      pc_if->mib.ifInOctets += ip_len;

      /* modify the TCP header for the next pass */
      if((prot == IPPROTO_TCP) && 
         (pcn_recvs < itterations))    /* skip last pass */
      {
         tcp_hdrfixup(&pbuf[0]);
      }
      MEMCPY(pkt->nb_prot, pbuf, headerslen);   /* restore headers from copy */

#ifdef DEBUG_PCKTCP
      /* Double-check the checksum modification */
      if((prot == IPPROTO_TCP) &&
         (pcn_recvs < itterations))    /* skip last pass */
      {
         u_short  tcpsum;

         ptcp = (struct tcphdr *)(pkt->nb_prot + sizeof (struct ip));
         tcpsum = ptcp->th_sum;     /* save sum to check */
         ptcp->th_sum = 0;          /* zero pkt buffer sum for computation */
         ptcp->th_sum = tcp_cksum((struct ip *)(pkt->nb_prot));
         if(ptcp->th_sum != tcpsum)
         {
            dtrap();
            break;
         }
      }
#endif   /* DEBUG_PCKTCP */
   }

   /* If there was no error in the main loop, print results */
   if(!err)
   {
      pticks = get_ptick() - pcn_starttime;

      if(prot == IPPROTO_TCP)
         pck_tcpfin(pio);    /* close tcp connection before we printf */

      pc_printstat(pio, itterations, pticks, typetext);
   }
   pcn_active = FALSE;               /* Officially done testing */

   /* If we got an error the packet is probably freed, else free it here: */
   if(err == 0){
      LOCK_NET_RESOURCE(FREEQ_RESID);
      pk_free(pkt);
      UNLOCK_NET_RESOURCE(FREEQ_RESID);
   }
   return err;
}

int
pkc_tcpecho(void * pio)
{
#ifdef TCP_ECHOTEST
   return(pkc_rep(pio, IPPROTO_TCP));
#else
   ns_printf(pio, "No TCP_ECHOTEST in this build\n");
   return -1;
#endif
}

int
pkc_udpecho(void * pio)
{
   return(pkc_rep(pio, UDP_PROT));
}


int
pkc_tcpsess(void * pio)
{
#ifdef TCP_ECHOTEST
   u_long   itterations;   /* number of sessions (from cmd line) */
   u_long   sessions;      /* number already done */
   u_long   pticks;
   int      err;
   int      ip_len;
   char *   arg   =  nextarg( ((GEN_IO)pio)->inbuf );
   PACKET   pkt;

   if(pc_if == NULL)
   {
      ns_printf(pio, "Open test device with pcmake first.\n");
      return -1;
   }

   if(*arg != 0)
      itterations = (u_long)atol(arg);
   else
      itterations = 1;     /* default to one */

   test_prot = 6;          /* save prot for pcn_pkt_send() call */
   pcn_active = TRUE;      /* now officially testing */
   ns_printf(pio, "Timing %d TCP echo sessions...\n", itterations);

   sessions = err = 0;
   ip_len = deflength + sizeof(struct ip) + sizeof(struct tcphdr);

   pcn_starttime = get_ptick();

   while(sessions++ < itterations)
   {
      err = pck_tcpconn(pio);        /* make the tcp echo connection */
      if(err)
      {
         ns_printf(pio, "TCP setup error\n");
         break;
      }

      pcn_sends = pcn_recvs = 0;    /* clear counters */
      pkt = pkc_bldtcp(ip_len, TH_ACK);
      pck_xchgpkt(pkt);             /* send a single echo packet */
      if(pcn_sends < pcn_recvs)     /* make sure we got response */
      {
         err = -1;
         ns_printf(pio, "Lost data pkt in loop #%lu, aborting.\n", sessions);
         break;
      }
      if(!pcn_active)      /* see if we got reset or something */
      {
         err = -1;
         ns_printf(pio, "reset, aborting.\n");
         break;
      }
      pc_if->mib.ifInNUcastPkts++;
      pc_if->mib.ifInOctets += ip_len;

   }

   pticks = get_ptick() - pcn_starttime;

   /* If there was no error in the main loop, print results */
   if(!err)
   {
      pticks = get_ptick() - pcn_starttime;
      pck_tcpfin(pio);    /* close tcp connection before we printf */
      pc_printstat(pio, itterations, pticks, "TCP echo session");
   }
   pcn_active = FALSE;               /* Officially done testing */

   return err;

#else /* TCP_ECHOTEST */
   ns_printf(pio, "No TCP in this build\n");
   return -1;
#endif
}

int
pkc_data(void * pio)
{
   ns_printf(pio, "pkt Cycle measurement status:\n");
   ns_printf(pio, "  Device is %s\n", pc_if?"open":"closed");
   if(calibration == 0)
      ns_printf(pio, "  Profile ticks not calibrated\n");
   else
      ns_printf(pio, "  Profile ticks calibrated at %d ticks/sec\n", calibration);
   if(pcn_rx_mode)
      ns_printf(pio, "  receives use rcvdq and a task switch\n");
   else
      ns_printf(pio, "  receives passed direct to ip_rcv()\n");

   return 0;
}

#endif   /* PKT_CYCLES  */

