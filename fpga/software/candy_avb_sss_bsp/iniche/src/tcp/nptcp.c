/*
 * FILENAME: nptcp.c
 *
 * Copyright 1997- 2000 By InterNiche Technologies Inc. All rights reserved
 *
 * nptcp.c NetPort TCP layer's emulation of BSD routines called by 
 * the TCP code. 
 *
 *
 * MODULE: INET
 *
 * ROUTINES: pffindtype(), pffindproto(), m_getnbuf(), m_free(), 
 * ROUTINES: m_freem(), m_copy(), m_adj(), mbuf_len (), dtom(), remque (), 
 * ROUTINES: insque(), nptcp_init(), tcp_rcv(), ip_output(), in_broadcast(), 
 * ROUTINES: np_stripoptions(), tcp_stat(), tcp_bsdconnstat(), 
 * ROUTINES: tcp_bsdsendstat(), tcp_bsdrcvstat(), mbuf_stat(), mbuf_list(), 
 * ROUTINES: sock_list(), *search_mbuf_queue(), *socket_queue_name(), 
 * ROUTINES: so_icmpdu(), tcp_tick(),
 *
 * PORTABLE: yes
 *
 * NOTE: CSUM_DEMO is used in this file to make some minor performance-
 *       related code changes. These changes may or may not be needed
 *       under normal circumstances.
 */

#include "ipport.h"

#ifdef INCLUDE_TCP  /* include/exclude whole file at compile time */

#include "tcpport.h"
#include "in_pcb.h"
#include "ip.h"
#include "icmp.h"
#include "udp.h"
#include "protosw.h"
#include "tcp.h"
#include "tcp_fsm.h"
#include "tcp_seq.h"
#include "tcp_timr.h"
#include "tcp_var.h"
#include "tcpip.h"

#ifdef IP_V6
#include "socket6.h"
#endif   /* IP_V6 */

unshort  select_wait;      /* select wait bits */


extern   unsigned lilbufs;
extern   unsigned bigbufs;

extern   int   ip_write(u_char prot, PACKET pkt);

#ifdef INCLUDE_SNMP
struct tcp_mib tcpmib;     /* mib stats for TCP layer */
#else
struct TcpMib  tcpmib;  /* mib stats for TCP layer */
#endif

#ifdef DO_DELAY_ACKS
int tcp_fasttimo(void);
#endif

#ifdef TCP_MENUS
#include "menu.h"
extern struct menu_op tcpmenu[5];
#endif   /* IN_MENUS */

#ifdef TCP_WIN_SCALE
/* default value for window scaling option */
u_char default_wind_scale = 0;
#endif   /* TCP_WIN_SCALE */

int      TCPTV_MSL   = (30 * PR_SLOWHZ);  /* max seg lifetime default */

/* protocol switch table entry, for TCP: */
struct protosw tcp_protosw =  {
   SOCK_STREAM,   
   IPPROTO_TCP,
   PR_CONNREQUIRED|PR_WANTRCVD,
   tcp_input,
#ifdef CTL_INPUT
   tcp_ctlinput,
#endif   /* CTL_INPUT */
#ifdef CTL_OUTPUT
   tcp_ctloutput,
#endif
   tcp_usrreq,
   tcp_init,
#ifdef DO_DELAY_ACKS
   tcp_fasttimo,
#endif
   tcp_slowtimo,
};

#ifdef UDP_SOCKETS
struct protosw udp_protosw =  {
   SOCK_DGRAM,          /* type - datagram */
   IPPROTO_UDP,         /* number (UDP over IP) */
   PR_ATOMIC|PR_ADDR,   /* flags */
   NULL,    /* input, not for us */
#ifdef CTL_INPUT
   NULL,
#endif   /* CTL_INPUT */
#ifdef CTL_OUTPUT
   NULL,
#endif
   udp_usrreq, /* this is where all the real work happens */
   NULL,    /* state-less UDP needs no init call */
#ifdef DO_DELAY_ACKS
   NULL,
#endif
   NULL,
};
#endif   /* UDP_SOCKETS */

#ifdef IP_RAW
struct protosw rawip_protosw =  {
   SOCK_RAW,            /* type - raw */
   IPPROTO_RAW,         /* number (raw IP) */
   PR_ATOMIC|PR_ADDR,   /* flags */
   NULL,                /* input, not for us */
#ifdef CTL_INPUT
   NULL,
#endif   /* CTL_INPUT */
#ifdef CTL_OUTPUT
   NULL,
#endif
   rawip_usrreq,        /* this is where all the real work happens */
   NULL,                /* state-less UDP needs no init call */
#ifdef DO_DELAY_ACKS
   NULL,
#endif
   NULL,
};
#endif   /* IP_RAW */

#ifdef NET_STATS
char *   tcpstates[] = 
{
   "CLOSED",
   "LISTEN",
   "SYN_SENT",
   "SYN_RCVD",
   "ESTABLISHED",
   "CLOSE_WAIT",
   "FIN_WAIT_1",
   "CLOSING",
   "LAST_ACK",
   "FIN_WAIT_2",
   "TIME_WAIT",
};
#endif   /* NET_STATS */



/* FUNCTION: pffindtype()
 * 
 * PARAM1: int domain
 * PARAM2: int type
 *
 * RETURNS: 
 */

struct protosw *  
pffindtype(int domain, int type)
{

   /* check that the passed domain is vaid for the build */
   if (domain != AF_INET)
   {
#ifdef IP_V6
      if(domain != AF_INET6)
#endif
      {
         dtrap();    /* programming error */
         return NULL;
      }
   }

   if (type == SOCK_STREAM)
      return &tcp_protosw;
#ifdef UDP_SOCKETS
   else if(type == SOCK_DGRAM)
      return &udp_protosw;
#endif   /* UDP_SOCKETS */
#ifdef IP_RAW
   else if(type == SOCK_RAW)
      return &rawip_protosw;
#endif  /* IP_RAW */
   else
      return NULL;
}



/* FUNCTION: pffindproto()
 * 
 * PARAM1: int domain
 * PARAM2: int protocol
 * PARAM3: int type
 *
 * RETURNS: 
 */

struct protosw *  
pffindproto(int domain, int protocol, int type)
{
#ifdef IP_RAW
   if (type == SOCK_RAW)
      return(pffindtype(domain, type));
#endif

   switch (protocol)
   {
#ifdef BSD_SOCKETS
   case IPPROTO_TCP:
      if (type == SOCK_STREAM)
         break;
      /* IPPROTO_TCP protocol on non-SOCK_STREAM type socket */
      dtrap();
      return NULL;
   case IPPROTO_UDP:
      if (type == SOCK_DGRAM)
         break;
      /* IPPROTO_UDP protocol on non-SOCK_DGRAM type socket */
      dtrap();
      return NULL;
#endif /* BSD_SOCKETS */
   case 0:
      /* let protocol default based on socket type */
      break;
   default:
      /* unknown/unsupported protocol on socket */
      dtrap();
      return NULL;
   }
   return(pffindtype(domain, type));   /* map to findtype */
}

struct mbstats {
   u_long   allocs;
   u_long   frees;
}  mbstat;

struct queue   soq;     /* A socket queue */
struct queue   mbufq;   /* A netport queue for in-use mbufs */
struct queue   mfreeq;  /* A netport queue for free mbufs */



/* FUNCTION: m_getnbuf()
 *
 * m_getnbuf() - the sole mbuf get routine for this port. These no 
 * version with a wait option. This gets a netport style nbuf to hold 
 * the data & maps it into a structure designed to fool the BSD TCP 
 * code into thinking it's an mbuf. 
 *
 * 
 * PARAM1: int type
 * PARAM2: int len
 *
 * RETURNS: 
 */

struct mbuf *  
m_getnbuf(int type, int len)
{
   struct mbuf *  m;
   PACKET pkt = NULL;

#ifdef NPDEBUG
   if (type < MT_RXDATA || type > MT_IFADDR)
   {
      dtrap(); /* is this OK? */
   }
#endif

   /* if caller has data (len >= 0), we need to allocate 
    * a packet buffer; else all we need is the mbuf */
   if (len != 0)
   {
      LOCK_NET_RESOURCE(FREEQ_RESID);
      pkt = pk_alloc(len + HDRSLEN);

      UNLOCK_NET_RESOURCE(FREEQ_RESID);
      if (!pkt)
         return NULL;
   }

   m = (struct mbuf *)getq(&mfreeq);
   if (!m)
   {
      if (pkt) 
      {
         LOCK_NET_RESOURCE(FREEQ_RESID);
         pk_free(pkt);
         UNLOCK_NET_RESOURCE(FREEQ_RESID);
      }
      return NULL;
   }
   m->m_type = type;
   if (len == 0)
   {
      m->pkt = NULL;
      m->m_base = NULL;    /* caller better fill these in! */
      m->m_memsz = 0;
   }
   else
   {
      m->pkt = pkt;
      /* set m_data to the part where tcp data should go */
      m->m_base = m->m_data = pkt->nb_prot = pkt->nb_buff + HDRSLEN;
      m->m_memsz = pkt->nb_blen - HDRSLEN;
   }
   m->m_len = 0;
   m->m_next = m->m_act = NULL;
   mbstat.allocs++;        /* maintain local statistics */
   putq(&mbufq, (qp)m);
   return m;
}


/* FUNCTION: m_free()
 *
 * m_free() frees mbufs allocated above. Retuns pointer to "next" 
 * mbuf. 
 *
 * 
 * PARAM1: struct mbuf * m
 *
 * RETURNS: 
 */


struct mbuf *  
m_free(struct mbuf * m)
{
   struct mbuf *  nextptr;

#ifdef NPDEBUG
   if (mbufq.q_len < 1)
      panic("mfree: q_len");

   if (m->m_type < MT_RXDATA || m->m_type > MT_IFADDR)
   {
      if (m->m_type == MT_FREE)
      {
         dtrap(); /* debug double free of mbuf by tcp_in() */
         return m->m_next; /* seems harmless, though.... */
      }
      else
         panic("m_free: type");
   }
#endif   /* NPDEBUG */

   nextptr = m->m_next;    /* remember value to return */

   if (qdel(&mbufq, m) == NULL)
      panic("m_free: missing");

   m->m_type = MT_FREE;    /* this may seem silly, but helps error checking */

   if (m->pkt)
   {
      LOCK_NET_RESOURCE(FREEQ_RESID);
      pk_free(m->pkt);     /* free up the netport buffer */
      UNLOCK_NET_RESOURCE(FREEQ_RESID);
   }
   mbstat.frees++;
   putq(&mfreeq, (qp)m);
   return nextptr;
}



/* FUNCTION: m_freem()
 *
 *  m_freem() frees a list of mbufs
 *
 * 
 * PARAM1: struct mbuf * m
 *
 * RETURNS: 
 */

void
m_freem(struct mbuf * m)
{
   while (m != NULL)
      m = m_free(m);
}



/* FUNCTION: m_copy()
 *
 * m_copy() - Make a copy of an mbuf chain starting "off" bytes from 
 * the beginning, continuing for "len" bytes. If len is M_COPYALL, 
 * copy to end of mbuf. Returns pointer to the copy. 
 *
 * 
 * PARAM1: struct mbuf * m
 * PARAM2: int off
 * PARAM3: int len
 *
 * RETURNS: 
 */

struct mbuf *  
m_copy(struct mbuf * m, int off, int len)
{
   struct mbuf *  nb, * head, *  tail;
   int   tocopy;

   if (len == 0)  /* nothing to do */
      return NULL;

#ifdef NPDEBUG
   /* sanity test parms */
   if (off < 0 || (len < 0 && len != M_COPYALL))
   {
      dtrap();
      return NULL;
   }
#endif   /* NPDEBUG */

   /* move forward through mbuf q to "off" point */
   while (off > 0) 
   {
      if (!m)
      {
         dtrap();
         return NULL;
      }
      if (off < (int)m->m_len)
         break;
      off -= m->m_len;
      m = m->m_next;
   }

   head = tail = NULL;

   while (len > 0)
   {
      if (m == NULL) /* at end of queue? */
      {
         panic("m_copy: bad len");
         return NULL;
      }
      tocopy = (int)MIN(len, (int)(m->m_len - off));

      /* mbuf data is expected to be aligned according to 
       * ALIGN_TYPE, so if the offset isn't aligned, we must 
       * copy the buffer instead of cloning it.
       * Also, don't permit multiple clones; they sometimes
       * lead to corrupted data.
       */
      if ((off & (ALIGN_TYPE - 1)) ||
          (m->pkt->inuse != 1))
      {
         if ((nb = m_getwithdata (m->m_type, tocopy)) == NULL)
            goto nospace;
         MEMCPY(nb->m_data, m->m_data+off, tocopy);
         nb->m_len = tocopy;  /* set length of data we just moved into new mbuf */

         tcpstat.tcps_mcopies++;
         tcpstat.tcps_mcopiedbytes += tocopy;
      }
      else
      {
         /* Rather than memcpy every mbuf's data, "clone" the data by 
          * making a duplicate of the mbufs involved and bumping the 
          * inuse count of the actual packet structs
          */
         if ((nb = m_getwithdata (m->m_type, 0)) == NULL)
            goto nospace;

         m->pkt->inuse++;     /* bump pkt use count to clone it */

         /* set up new mbuf with pointers to cloned packet */
         nb->pkt = m->pkt;
         nb->m_base = m->m_base;
         nb->m_memsz = m->m_memsz;
         nb->m_data = m->m_data + off;
         nb->m_len = tocopy;

         tcpstat.tcps_mclones++;
         tcpstat.tcps_mclonedbytes += tocopy;
      }

      len -= tocopy;
      off = 0;
      if (tail)      /* head & tail are set by first pass thru loop */
         tail->m_next = nb;
      else
         head = nb;
      tail = nb;     /* always make new mbuf the tail */
      m = m->m_next;

   }

   return head;

nospace:
   m_freem (head);
   return NULL;
}




/* FUNCTION: m_adj()
 * 
 * slice "length" data from the head of an mbuf
 *
 * PARAM1: struct mbuf * mp
 * PARAM2: int len
 *
 * RETURNS: 
 */

void
m_adj(struct mbuf * mp, int len)
{
   struct mbuf *  m;
   int   count;

   if ((m = mp) == NULL)
      return;

   if (len >= 0) 
   {
      while (m != NULL && len > 0) 
      {
         if (m->m_len <= (unsigned)len)
         {
            len -= m->m_len;
            m->m_len = 0;
            m = m->m_next;
         }
         else
         {
            m->m_len -= len;
            m->m_data += len;
            break;
         }
      }
   } 
   else
   {
      /*
       * Trim from tail.  Scan the mbuf chain,
       * calculating its length and finding the last mbuf.
       * If the adjustment only affects this mbuf, then just
       * adjust and return.  Otherwise, rescan and truncate
       * after the remaining size.
       */
      len = -len;
      count = 0;
      for (;;) 
      {
         count += m->m_len;
         if (m->m_next == (struct mbuf *)0)
            break;
         m = m->m_next;
      }
      if (m->m_len >= (unsigned)len)
      {
         m->m_len -= len;
         return;
      }
      count -= len;
      /*
       * Correct length for chain is "count".
       * Find the mbuf with last data, adjust its length,
       * and toss data from remaining mbufs on chain.
       */
      for (m = mp; m; m = m->m_next)
      {
         if (m->m_len >= (unsigned)count)
         {
            m->m_len = count;
            break;
         }
         count -= m->m_len;
      }
      while ((m = m->m_next) != (struct mbuf *)NULL)
         m->m_len = 0;
   }
}


/* FUNCTION: mbuf_len ()
 * 
 *   mbuf_len() - Compute the length of a chain of m_bufs.
 *
 * PARAM1: struct mbuf *m
 *
 * RETURNS: 
 */

int
mbuf_len (struct mbuf * m)
{
   int   len   =  0;

   while (m)
   {
      len += m->m_len;
      m = m->m_next;
   }
   return len;
}



/* FUNCTION: dtom()
 * 
 * dtom() - given a data pointer, return the mbuf it's in.
 *
 * PARAM1: void * data
 *
 * RETURNS: 
 */

struct mbuf *  
dtom(void * data)
{
   qp qptr;
   struct mbuf *  m;

   for (qptr = mbufq.q_head; qptr; qptr = qptr->qe_next)
   {
      m = (struct mbuf *)qptr;

      if (IN_RANGE(m->m_base, m->m_memsz, (char*)data))
         return (struct mbuf *)qptr;
      else
         continue;

   }

   panic("dtom");    /* data not found in any "in use" mbuf */
   return NULL;
}


/* FUNCTION: remque ()
 *
 * remque() - Used for removing overlapping tcp segments from input 
 * queue. Adapted from BSD/Mango, but "struct queue" conflicted with 
 * NetPort struct; so it uses the a contrived BSDq structure. 
 *
 * 
 * PARAM1: void *arg
 *
 * RETURNS: 
 */

struct bsdq {
   struct bsdq *  next; /* for overlaying on ipovly, et.al. */
   struct bsdq *  prev;
};


void
remque (void * arg)
{
   struct bsdq *  old;

   old = (struct bsdq *)arg;
   if (!old->prev) return;
      old->prev->next = old->next;
   if (old->next)
      old->next->prev = old->prev;
}



/* FUNCTION: insque()
 * 
 * PARAM1: void * n
 * PARAM2: void * p
 *
 * RETURNS: 
 */

void
insque(void * n, void * p)
{
   struct bsdq *  newe, *  prev;

   newe = (struct bsdq *)n;
   prev = (struct bsdq *)p;
   newe->next = prev->next;
   newe->prev = prev;
   prev->next = newe;
   if (newe->next)
      newe->next->prev = newe;
}


/* FUNCTION: nptcp_init()
 *
 * nptcp_init() - set up TCP to run. This assumes netport IP, packet 
 * quques, interfaces, et.al., are already up. This routine is called 
 * by the port file, which should already have done port specific 
 * setup, does NetPort TCP general setup, and then calls tcp_init() 
 * to do BSD TCP generic setup. 
 *
 * 
 * PARAM1: 
 *
 * RETURNS: Returns 0 if OK, else non-zero error code. 
 */

int
nptcp_init()
{
   int   i;
   /* total netport buffers:
    * Get twice the number of packet buffers plus 3, to help
    * support UDP. if you are receiving UDP packets faster
    * than your UDP server can process them, its possible for 
    * all of your packet buffers to end up in the UDP socket's 
    * receive queue. each of these packet buffers has associated with 
    * it 2 mbufs, one for the packet buffer itself and one for the 
    * address of the remote host that the packet came from. in order 
    * for this socket queue to get emptied and the receive buffers to 
    * get freed, the socket has to be read and what's the first thing 
    * that soreceive() does to try to do this? it tries to allocate 
    * an mbuf, so if you only have twice as many mbufs as packet 
    * buffers, soreceive() can't complete and the packet buffers stay 
    * on the queue, so we allocate 3 extra mbufs in the hope that 
    * this will allow soreceive() to complete and free up the packet 
    * buffers. yes, its kind of an ugly hack and 3 is a wild guess.
    */
   unsigned bufcount = (lilbufs + bigbufs) * 2 + 3;
   struct mbuf *  m; /* scratch mbuf for mfreeq init */

   MEMSET(&soq, 0, sizeof(soq));    /* Set socket queue to NULLs */
   MEMSET(&mbufq, 0, sizeof(mbufq));
   MEMSET(&mfreeq, 0, sizeof(mfreeq));
   for (i = 0; i < (int)bufcount; i++)
   {
      m = MBU_ALLOC(sizeof(struct mbuf));
      if (!m)  /* malloc error, bail out */
         panic("tcpinit");
      m->m_type = MT_FREE;
      m->m_len = 0;
      m->m_data = NULL;
      putq(&mfreeq, (qp)m);
   }
   mfreeq.q_min = (int)bufcount;   /* this should match q_max and q_len */

#ifdef INCLUDE_SNMP                /* SNMP initialization */
   tcpmib.tcpRtoAlgorithm = 4;     /* Van Jacobson's algorithm */
   tcpmib.tcpRtoMin = TCPTV_MIN * 1000;      /* PR_SLOWHZ */
   tcpmib.tcpRtoMax = TCPTV_REXMTMAX * 1000; /* PR_SLOWHZ */
#endif

   tcp_init();    /* call the BSD init in tcp_usr.c */

#ifdef TCP_MENUS
   install_menu(&tcpmenu[0]);
#endif   /* IN_MENUS */

   return 0;   /* good return */
}


/* FUNCTION: tcp_rcv()
 *
 * tcp_rcv(PACKET pkt) - called from IP stack whenever it receives a 
 * TCP packet. data is passed in a NetPort netbuf structure. Returns 
 * 0 if packet was processed succesfully, ENP_NOT_MINE if not for me, 
 * or a negative error code if packet was badly formed. In practice, 
 * the return value is usually ignored. 
 *
 * 
 * PARAM1: PACKET pkt
 *
 * RETURNS: 
 */

#ifdef IP_V4
 
int
tcp_rcv(PACKET pkt)     /* NOTE: pkt has nb_prot pointing to IP header */
{
   struct mbuf *  m_in;
   struct ip * bip;  /* IP header, berkeley version */
   struct tcphdr *   tcpp;
   unshort  len;  /* scratch length holder */

   /* For TCP, the netport IP layer is modified to set nb_prot to the 
    * start of the IP header (not TCP). We need to do some further
    * mods which the BSD code expects:
    */
   bip = (struct ip *)pkt->nb_prot;    /* get ip header */
   len = ntohs(bip->ip_len);  /* get length in local endian */

   /* verify checksum of received packet */

   tcpp = (struct tcphdr *)ip_data(bip);
   if (tcp_cksum(bip) != tcpp->th_sum)
   {
      TCP_MIB_INC(tcpInErrs);    /* keep MIB stats */
      tcpstat.tcps_rcvbadsum++;  /* keep BSD stats */
      LOCK_NET_RESOURCE(FREEQ_RESID);
      pk_free(pkt);  /* punt packet */
      UNLOCK_NET_RESOURCE(FREEQ_RESID);
      return ENP_BAD_HEADER;
   }

   m_in = m_getnbuf(MT_RXDATA, 0);
   if (!m_in){
      LOCK_NET_RESOURCE(FREEQ_RESID);
      pk_free(pkt);
      UNLOCK_NET_RESOURCE(FREEQ_RESID);
      return ENP_RESOURCE;  
   }

   IN_PROFILER(PF_TCP, PF_ENTRY);      /* measure time in TCP */

   /* subtract IP header length from total IP packet length */
   len -= ((unshort)(bip->ip_ver_ihl & 0x0f) << 2);
   bip->ip_len = len;   /* put TCP length in struct for TCP code to use */

   /* set mbuf to point to start of IP header (not TCP) */
   m_in->pkt = pkt;
   m_in->m_data = pkt->nb_prot;
   m_in->m_len = pkt->nb_plen;
   m_in->m_base = pkt->nb_buff;     /* ??? */
   m_in->m_memsz = pkt->nb_blen;    /* ??? */

   tcp_input(m_in, pkt->net);

   IN_PROFILER(PF_TCP, PF_EXIT);      /* measure time in TCP */

   return 0;
}


/* FUNCTION: ip_output()
 *
 * ip_output() - called by tcp_output() to send a tcp segment on the 
 * IP layer. We convert the mbufs passed into a PACKET, set a few 
 * things up for NetPort IP, and pass it on down. Returns 0 if no 
 * error, negative "NP" error if lower layers fail, postive error 
 * from nptcp.h if TCP/Sockets fails. 
 *
 * 
 * PARAM1: struct mbuf * data
 *
 * RETURNS: 
 */

int
ip_output(struct mbuf * data, struct   ip_socopts * so_optsPack) /* mbuf chain with data to send */
{
   struct ip * bip;
   struct tcphdr *   tcpp;
   PACKET pkt;
   struct mbuf *  m1, * m2, * mtmp; /* temp mbuf pointers */
   int   e;    /* error holder */
   int   total;

   /* reassemble mbufs into a contiguous packet. Do this with as 
    * little copying as possible. Typically the mbufs will be either 
    * 1) a single mbuf with iptcp header info only (e.g.tcp ACK 
    * packet), or 2) iptcp header with data mbuf chained to it, or 3) 
    * #2) with a tiny option data mbuf between header and data. 
    */
   if ((data->m_next))
   {
      m1 = data;
      m2 = data->m_next;

      /* If m2 is small (e.g. options), copy it to m1 and free it */
      while (m2 && (m2->m_len < 10))
      {
         pkt = m1->pkt;
         if ((pkt->nb_buff + pkt->nb_blen) > /* make sure m2 will fit in m1 */
             (m1->m_data + m1->m_len + m2->m_len))
         {
            MEMCPY((m1->m_data + m1->m_len), m2->m_data, m2->m_len);
            m1->m_len += m2->m_len;
            m1->m_next = m2->m_next;
            m_free(m2);    /* free this m2.... */
            m2 = m1->m_next;  /* ...and thread the next one */
            tcpstat.tcps_oappends++;
         }
         else     /* if won't fit, fall to next copy */
            break;
      }

      while (m2)  /* If we still have two or more buffers, more copying: */
      {
         /* try prepending m1 to m2, first see if it fits: */
         e = m2->m_data - m2->pkt->nb_buff;  /* e is prepend space */
         if (e < MaxLnh)
         { 
#ifdef NPDEBUG
            dprintf("nptcp: MaxLnh:%d, e:%d\n", MaxLnh, e);
#endif
            panic("tcp_out:mbuf-nbuf");   /* sanity check */
         }

         if ((m1->m_len < (unsigned)(e - MaxLnh))  /* leave room for MAC */
             && ((m1->m_len & (ALIGN_TYPE - 1)) == 0)  /* and stay aligned */
             && ((m2->m_data - m2->pkt->nb_buff) == HDRSLEN))   /* be at start */
         {
            MEMCPY((m2->m_data - m1->m_len), m1->m_data, m1->m_len);
            m2->m_data -= m1->m_len;   /* fix target to reflect prepend */
            m2->m_len += m1->m_len;
            m_free(m1);    /* free head (copied) mbuf */
            data = m1 = m2;   /* move other mbufs up the chain */
            m2 = m2->m_next;  /* loop to while(m2) test */
            tcpstat.tcps_oprepends++;
         }
         else     /* if won't fit, fall to next copy */
            break;
      }

      if (m2)  /* If all else fails, brute force copy: */
      {
         total = 0;
         for (mtmp = m1; mtmp; mtmp = mtmp->m_next)
            total += mtmp->m_len;
         LOCK_NET_RESOURCE(FREEQ_RESID);
         pkt = pk_alloc(total + HDRSLEN);
         UNLOCK_NET_RESOURCE(FREEQ_RESID);
         if (!pkt)
            return ENOBUFS;
         pkt->nb_prot = pkt->nb_buff + MaxLnh;

         mtmp = m1;
         while (mtmp)
         {
            MEMCPY(pkt->nb_prot, mtmp->m_data, mtmp->m_len);
            pkt->nb_prot += mtmp->m_len;
            pkt->nb_plen += mtmp->m_len;
            m2 = mtmp;
            mtmp = mtmp->m_next;
            if (m2 != data)   /* save original head */
               m_free(m2);
            tcpstat.tcps_ocopies++;
         }
         pkt->nb_prot -= total;     /* fix data pointer */

         /* release the original mbufs packet install the new one */
         LOCK_NET_RESOURCE(FREEQ_RESID);
         pk_free(data->pkt);
         UNLOCK_NET_RESOURCE(FREEQ_RESID);
         data->pkt = pkt;
         data->m_len = pkt->nb_plen;
         data->m_next = NULL;
         data->m_data = pkt->nb_prot;
         data->m_len = total;
      }
   }

   if ((data->m_data < (data->pkt->nb_buff + MaxLnh)))
      panic("ip_output: overflow");

   pkt = data->pkt;

   /* do we have options? */
   if (so_optsPack)
	   pkt->soxopts = so_optsPack;   /* yup */
#ifdef IP6_ROUTING
   else
   {
      panic("ip_output: no so_optsPack for the IPv6 scope");     
   }
#endif

   /* fill in dest host for IP layer */
   bip = (struct ip *)data->m_data;
   pkt->fhost = bip->ip_dest;

   /* make enough IP header for cksum calculation */
   bip->ip_ver_ihl = 0x45;
   bip->ip_len = htons(bip->ip_len);   /* make net endian for calculation */
   tcpp = (struct tcphdr *)ip_data(bip);
#ifdef CSUM_DEMO
   if (!(tcpp->th_flags & TH_SYN))
   tcpp->th_flags |= TH_PUSH;     /* force the PSH flag in TCP hdr */
#endif
   tcpp->th_sum = tcp_cksum(bip);

   pkt->nb_prot = (char*)(bip + 1);    /* point past IP header */
   pkt->nb_plen = data->m_len - sizeof(struct ip);

   e = ip_write(IPPROTO_TCP, pkt);

   /* ip_write() is now responsable for data->pkt, so... */
   data->pkt = NULL;
   m_freem(data);

   if (e < 0)
   {
      /* don't report dropped sends, it causes socket applications to 
      bail when a TCP retry will fix the problem */
      if (e == SEND_DROPPED)
         return 0;
      return e;
   }
   else
      return 0;
}
#endif   /* IP_V4 */



/* FUNCTION: in_broadcast()
 *
 * Determine if the passed IP address is a braodcast or not.
 * Currently no support for subnet broadcasts.
 *
 * PARAM1: u_long ipaddr
 *
 * RETURNS: TRUE if broadcast, else FALSE
 */

int
in_broadcast(u_long ipaddr)   /* passed in net endian */
{
   if (ipaddr == 0xffffffff)
      return TRUE;

   return FALSE;
}


/* FUNCTION: np_stripoptions()
 *
 * np_stripoptions - This replaces BSD ip_stripoptions. The 
 * functionality is slightly different. Called from tcp_in.c 
 *
 * 
 * PARAM1: struct ip * ti
 * PARAM2: struct mbuf * m
 *
 * RETURNS: void
 */

void
np_stripoptions(struct ip * ti, struct mbuf * m)
{
   int   ihlen;

   /* get the IP header length in octets */
   ihlen = (ti->ip_ver_ihl & 0x0f) << 2;

   /* if it's <= 20 octets, there are no IP header options to strip */
   if (ihlen <= 20)
      return;

   /* figure out how much to strip: we want to keep the 20-octet IP header */
   ihlen -= 20;

   /* remove the stripped options from the IP datagram length */
   ti->ip_len -= ihlen;

   /* and from the IP header length (which will be 5*4 octets long) */
   ti->ip_ver_ihl = (ti->ip_ver_ihl & 0xf0) | 5;

   /* move the 20-octet IP header up against the IP payload */
   MEMMOVE( ((char*)ti) + ihlen, ti, 20);
   m->m_len -= ihlen;
   m->m_data += ihlen;
}


#ifdef NET_STATS
#ifdef BSDTCP_STATS

/* FUNCTION: tcp_stat()
 *
 * The next few routines pretty-print stats. #define-ing ns_printf 
 * away will minmize space at the expense of utility. They support
 * many ancient (but sometimes still usefull) BSD counters.
 *
 * 
 * PARAM1: void * pio
 *
 * RETURNS: 0
 */

int
tcp_stat(void * pio)
{
   /* MIB-2 tcp group counters */ 
   ns_printf(pio,"tcpRtoAlgorithm %lu,\ttcpRtoMin %lu\n",
    tcpmib.tcpRtoAlgorithm, tcpmib.tcpRtoMin);
   ns_printf(pio,"tcpRtoMax %lu,\ttcpMaxConn %lu\n",
    tcpmib.tcpRtoMax, tcpmib.tcpMaxConn);
   ns_printf(pio,"tcpActiveOpens %lu,\ttcpPassiveOpens %lu\n",
    tcpmib.tcpActiveOpens, tcpmib.tcpPassiveOpens);
   ns_printf(pio,"tcpAttemptFails %lu,\ttcpEstabResets %lu\n",
    tcpmib.tcpAttemptFails, tcpmib.tcpEstabResets);
   ns_printf(pio,"tcpCurrEstab %lu,\ttcpInSegs %lu\n",
    tcpmib.tcpCurrEstab, tcpmib.tcpInSegs);
   ns_printf(pio,"tcpOutSegs %lu,\ttcpRetransSegs %lu\n",
    tcpmib.tcpOutSegs, tcpmib.tcpRetransSegs);
   ns_printf(pio,"tcpInErrs %lu,\ttcpOutRsts %lu\n",
    tcpmib.tcpInErrs, tcpmib.tcpOutRsts);

   return 0;
}



/* FUNCTION: tcp_bsdconnstat()
 *
 * BSD connection statistics
 *
 * 
 * PARAM1: void * pio
 *
 * RETURNS: 0
 */

int
tcp_bsdconnstat(void * pio)
{
   ns_printf(pio,"connections initiated: %lu,\tconnections accepted: %lu\n",
    tcpstat.tcps_connattempt, tcpstat.tcps_accepts);
   ns_printf(pio,"connections established: %lu,\tconnections dropped: %lu\n",
    tcpstat.tcps_connects, tcpstat.tcps_drops);
   ns_printf(pio,"embryonic connections dropped: %lu,\tconn. closed (includes drops): %lu\n",
    tcpstat.tcps_conndrops, tcpstat.tcps_closed);
   ns_printf(pio,"segs where we tried to get rtt: %lu,\ttimes we succeeded: %lu\n",
    tcpstat.tcps_segstimed, tcpstat.tcps_rttupdated);
   ns_printf(pio,"delayed acks sent: %lu,\tconn. dropped in rxmt timeout: %lu\n",
    tcpstat.tcps_delack, tcpstat.tcps_timeoutdrop);
   ns_printf(pio,"retransmit timeouts: %lu,\tpersist timeouts: %lu\n",
    tcpstat.tcps_rexmttimeo, tcpstat.tcps_persisttimeo);
   ns_printf(pio,"keepalive timeouts: %lu,\tkeepalive probes sent: %lu\n",
    tcpstat.tcps_keeptimeo, tcpstat.tcps_keepprobe);
#ifdef TCP_SACK
   ns_printf(pio,"sack connections: %lu,\tconns dropped in keepalive: %lu\n",
      tcpstat.tcps_sackconn, tcpstat.tcps_keepdrops);
#else
   ns_printf(pio,"conns dropped in keepalive: %lu\n",
      tcpstat.tcps_keepdrops);
#endif   /* TCP_SACK */

   return 0;
}




/* FUNCTION: tcp_bsdsendstat()
 * 
 * BSD TCP send stats
 *
 * PARAM1: void * pio
 *
 * RETURNS: 0
 */

int
tcp_bsdsendstat(void * pio)
{

   ns_printf(pio,"total packets sent: %lu,\tdata packets sent: %lu\n",
    tcpstat.tcps_sndtotal, tcpstat.tcps_sndpack);
   ns_printf(pio,"data bytes sent: %lu,\tdata packets retransmitted: %lu\n",
    tcpstat.tcps_sndbyte, tcpstat.tcps_sndrexmitpack);
   ns_printf(pio,"data bytes retransmitted: %lu,\tack-only packets sent: %lu\n",
    tcpstat.tcps_sndrexmitbyte, tcpstat.tcps_sndacks);
   ns_printf(pio,"window probes sent: %lu,\tpackets sent with URG only: %lu\n",
    tcpstat.tcps_sndprobe, tcpstat.tcps_sndurg);
   ns_printf(pio,"window update-only packets sent: %lu,\tcontrol (SYN|FIN|RST) packets sent: %lu\n",
    tcpstat.tcps_sndwinup, tcpstat.tcps_sndctrl);
#ifdef TCP_SACK
   ns_printf(pio,"sack hdrs sent: %lu,\tsack based data resends: %lu\n",
    tcpstat.tcps_sacksent, tcpstat.tcps_sackresend);
#endif   /* TCP_SACK */

   return 0;
}



/* FUNCTION: tcp_bsdrcvstat()
 *
 *  BSD TCP receive stats
 *
 * 
 * PARAM1: void * pio
 *
 * RETURNS: 0
 */

int
tcp_bsdrcvstat(void * pio)
{
   ns_printf(pio,"total packets received: %lu,\tpackets received in sequence: %lu\n",
    tcpstat.tcps_rcvtotal, tcpstat.tcps_rcvpack);
   ns_printf(pio,"bytes received in sequence: %lu,\tpackets received with ccksum errs: %lu\n",
    tcpstat.tcps_rcvbyte, tcpstat.tcps_rcvbadsum);
   ns_printf(pio,"packets received with bad offset: %lu,\tpackets received too short: %lu\n",
    tcpstat.tcps_rcvbadoff, tcpstat.tcps_rcvshort);
   ns_printf(pio,"duplicate-only packets received: %lu,\tduplicate-only bytes received: %lu\n",
    tcpstat.tcps_rcvduppack, tcpstat.tcps_rcvdupbyte);
   ns_printf(pio,"packets with some duplicate data: %lu,\tdup. bytes in part-dup. packets: %lu\n",
    tcpstat.tcps_rcvpartduppack, tcpstat.tcps_rcvpartdupbyte);
   ns_printf(pio,"out-of-order packets received: %lu,\tout-of-order bytes received: %lu\n",
    tcpstat.tcps_rcvoopack, tcpstat.tcps_rcvoobyte);
   ns_printf(pio,"packets with data after window: %lu,\tbytes rcvd after window: %lu\n",
    tcpstat.tcps_rcvpackafterwin, tcpstat.tcps_rcvbyteafterwin);
   ns_printf(pio,"packets rcvd after close: %lu,\trcvd window probe packets: %lu\n",
    tcpstat.tcps_rcvafterclose, tcpstat.tcps_rcvwinprobe);
   ns_printf(pio,"rcvd duplicate acks: %lu,\trcvd acks for unsent data: %lu\n",
    tcpstat.tcps_rcvdupack, tcpstat.tcps_rcvacktoomuch);
   ns_printf(pio,"rcvd ack packets: %lu,\tbytes acked by rcvd acks: %lu\n",
    tcpstat.tcps_rcvackpack, tcpstat.tcps_rcvackbyte);
   ns_printf(pio,"rcvd window update packets: %lu\n", tcpstat.tcps_rcvwinupd);
   ns_printf(pio,"rcvd predictive header hits; acks:%lu, data:%lu\n",
    tcpstat.tcps_predack, tcpstat.tcps_preddat);
#ifdef TCP_SACK
   ns_printf(pio,"sack hdrs recv: %lu,\tmax sack blocks in hdr: %lu\n",
    tcpstat.tcps_sackrcvd, tcpstat.tcps_sackmaxblk);
#endif   /* TCP_SACK */

   return 0;
}

#endif   /* BSDTCP_STATS */



/* FUNCTION: mbuf_stat()
 * 
 * PARAM1: void * pio
 *
 * RETURNS: 0
 */

int
mbuf_stat(void * pio)
{
   ns_printf(pio,"mfreeq: head:%p, tail:%p, len:%d, min:%d, max:%d\n",
    (void*)mfreeq.q_head, (void*)mfreeq.q_tail, 
    mfreeq.q_len, mfreeq.q_min, mfreeq.q_max);

   ns_printf(pio,"mbufq: head:%p, tail:%p, len:%d, min:%d, max:%d\n",
    (void*)mbufq.q_head, (void*)mbufq.q_tail, 
    mbufq.q_len, mbufq.q_min, mbufq.q_max);

   ns_printf(pio,"mbuf allocs: %lu, frees: %lu\n", mbstat.allocs, mbstat.frees);

   ns_printf(pio,"m_copy copies: %lu, copied bytes: %lu\n", 
    tcpstat.tcps_mcopies, tcpstat.tcps_mcopiedbytes);
   ns_printf(pio,"m_copy clones: %lu, cloned bytes: %lu\n",
    tcpstat.tcps_mclones, tcpstat.tcps_mclonedbytes);

   ns_printf(pio,"ip_output appends: %lu, prepends: %lu, copies: %lu\n",
    tcpstat.tcps_oappends, tcpstat.tcps_oprepends, 
    tcpstat.tcps_ocopies);

   return 0;
}



/* FUNCTION: mbuf_list()
 * 
 * PARAM1: void * pio
 *
 * RETURNS: 
 */

int
mbuf_list(void * pio)
{
   struct mbuf *  mb;

   ns_printf(pio,"mbufs in use: \n");
   for (mb = (struct mbuf *)mbufq.q_head; mb; mb = mb->next)
      ns_printf(pio,"type %d, pkt:%lx, data:%lx, len:%d\n",
    mb->m_type, mb->pkt, mb->m_data, mb->m_len);
   return 0;
}



extern int udpsock_list(void * pio);

/* FUNCTION: sock_list()
 * 
 * Display information about all the existing TCP and UDP sockets.
 *
 * PARAM1: void * pio
 *
 * RETURNS:  0 if successfull, else error code.
 */

int
sock_list(void * pio)
{
   struct inpcb * inp;
   struct socket *   so;
   struct tcpcb * tp;

   if (tcb.inp_next == NULL)
   {
      ns_printf(pio,"No TCP sockets\n");
      return 0;
   }

   ns_printf(pio,
    "TCP sock, fhost,     ports,    opts, rxbytes, txbytes, snd_una, snd_nxt, state:\n");
   for (inp = tcb.inp_next; inp != &tcb; inp = inp->inp_next) 
   {
      tp = intotcpcb(inp);
      so = inp->inp_socket;
      if (!so)
      {
         ns_printf(pio,"No socket\n");
         continue;
      }

#ifdef IP_V4
      if(so->so_domain == AF_INET)
      ns_printf(pio,"%lx,  %u.%u.%u.%u", so, PUSH_IPADDR(inp->inp_faddr.s_addr));
#endif
#ifdef IP_V6
      if(so->so_domain == AF_INET6)
      {
         char  ip6buf[46];
         ns_printf(pio,"%lx,  %s(v6), ", so, print_ip6(&inp->ip6_faddr, &ip6buf[0]));
      }
#endif   /* IP_V6 */

      ns_printf(pio,", %u->%u, ", htons(inp->inp_lport), htons(inp->inp_fport));
      ns_printf(pio,"0x%x, %u, %u, %ld, %ld, ", (unsigned)so->so_options,
       (unsigned)so->so_rcv.sb_cc,(unsigned)so->so_snd.sb_cc,
       tp->snd_una, tp->snd_nxt);
      if ((tp->t_state < 0) || 
          (tp->t_state >= sizeof(tcpstates)/sizeof(tcpstates[0])))
         ns_printf(pio, "???\n");
      else
         ns_printf(pio, "%s\n", tcpstates[tp->t_state]);
   }

   return udpsock_list(pio);
}



/* FUNCTION: *search_mbuf_queue()
 *
 * this function searches the mbuf queue headed by mbuf for an mbuf 
 * containing the PACKET structure addressed by pkt. 
 *
 * 
 * PARAM1: struct mbuf *mbuf
 * PARAM2: PACKET pkt
 *
 * RETURNS: 
 */

struct mbuf * search_mbuf_queue(struct mbuf * mbuf, PACKET pkt)
{
   for ( ; mbuf; mbuf = mbuf->next)
   {
      if (mbuf->pkt == pkt)
         return mbuf;
   }
   return 0;
}


/* FUNCTION: *socket_queue_name()
 *
 * this function checks to see if the passed in PACKET structure is 
 * in one of the socket queues. returns a pointer to name describing 
 * what queue the PACKET structure 
 *
 * 
 * PARAM1: PACKET pkt
 *
 * RETURNS: 
 */

char * socket_queue_name(PACKET pkt)
{
   struct inpcb * inp;
   struct socket *   so;

   /* no sockets to search thru */
   if (tcb.inp_next == NULL)
   {
      return 0;
   }

   /* for each control block */
   for (inp = tcb.inp_next; inp != &tcb; inp = inp->inp_next) 
   {
      /* get the associated socket */
      so = inp->inp_socket;
      /* if there is an associate socket */
      if (so)
      {
         /* check to see if the pkt is in the rcv mbuf queue */
         if (search_mbuf_queue(so->so_rcv.sb_mb,pkt))
            return "sor";
         /* check to see if the pkt is in the send mbuf queue */
         if (search_mbuf_queue(so->so_snd.sb_mb,pkt))
            return "snd";
      }
   }
   /* not found in any socket queue */
   return 0;
}

#endif   /* NET_STATS */


/* FUNCTION: so_icmpdu()
 *
 * so_icmpdu() - Called from NetPort UDP layer whenever it gets a 
 * ICMP destination unreachable packet (and is linked with this 
 * sockets layer) Tries to find the offending socket, notify caller, 
 * and shut it down. 
 *
 * 
 * PARAM1: PACKET p
 * PARAM2: struct destun * pdp
 *
 * RETURNS: 
 */

void
so_icmpdu(PACKET p, struct destun * pdp)
{
   ip_addr lhost;    /* IP address of originator (our iface) */
   ip_addr fhost;    /* IP address we sent to */
   unshort  fport;   /* TCP/UDP port we sent to */
   unshort  lport;   /* TCP/UDP port we sent from */
   struct inpcb * inp;
   struct socket *   so;
   struct tcpcb * tp;

   /* extract information about packet which generated DU */
   fhost = htonl(pdp->dip.ip_dest);
   lhost = htonl(pdp->dip.ip_src);
   lport = htons(*(unshort*)(&pdp->ddata[0]));
   fport = htons(*(unshort*)(&pdp->ddata[2]));

#ifndef IP_PMTU
   /* if it's a datagram-too-big message, ignore it -- As the
    * build isn't using PMTU Discovery this packet is most 
    * probably a Denial of Service Attack.
    */
    if(pdp->dcode == DSTFRAG)
    {
       goto done;
    }
#endif   /* IP_PMTU */

   /* if it's a TCP connection, clean it up */
   if (pdp->dip.ip_prot == TCPTP)
   {
      /* find associated data structs and socket */
      inp = in_pcblookup(&tcb, fhost, fport, lhost, lport, INPLOOKUP_WILDCARD);
      if (inp == 0)
         goto done;
      so = inp->inp_socket;
      if (so == 0)
         goto done;
      tp = intotcpcb(inp);
      if (tp)
      {
         if (tp->t_state <= TCPS_LISTEN)
         {
            goto done;
         }

#ifdef ICMP_TCP_DOS
         {
         struct ip * pip;
         struct tcpiphdr * ti;

         pip = ip_head(p);  /* find IP header */
         ti = (struct tcpiphdr *)p->nb_prot;

         if(!((tp->snd_una <=  ti->ti_seq) && (ti->ti_seq <= tp->snd_nxt)))
            goto done;

         /* If we get an ICMP Type 3 (Destination Unreachable) - Code 2
          * (Protocol Unreachable) message and during the life of a TCP
          * connection, then its most probably a Denial of Service Attack.
          * As the only other interpretation would be that the support for
          * the transport protocol has been removed from the host sending
          * the error message during the life of the corresponding 
          * connection. As in common practice this is higly unlikely in most
          * cases, we will treat this message as a DOS attack.
          */
         if(pdp->dcode == DSTPROT)
         {
            if((tp->t_state >= TCPS_ESTABLISHED) && (tp->t_state <= TCPS_TIME_WAIT))
               goto done;
         }

        /* Note some ICMP error messages generated by intermediate routers,
         * include more than the recommended 64 bits of the IP Data. If the
         * TCP ACK number happens to be present then use it in detecting a
         * Denial of Service attack.
         *
         * This way we can ensure that the TCP Acknowledgement number should
         * correspond to data that have already been acknowledged. This way
         * we can further reduce the possiblity of considering a spoofed ICMP
         * packet by a factor of 2.
         */
        if(pip->ip_len >= 32)
        {
            if(!(ti->ti_seq <= tp->rcv_nxt))
               goto done;
        }
   }
#endif

         tcp_close(tp);
      }
      so->so_error = ECONNREFUSED;  /* set error for socket owner */
   }   
#ifdef UDP_SOCKETS   /* this sockets layer supports UDP too */
   else if(pdp->dip.ip_prot == UDP_PROT)
   {
      UDPCONN tmp;
      /* search udp table (which keeps hosts in net endian) */
      for (tmp = firstudp; tmp; tmp = tmp->u_next)
         if ((tmp->u_fport == fport || tmp->u_fport == 0) &&
             (tmp->u_fhost == htonl(fhost)) &&
             (tmp->u_lport == lport))
         {
            break;   /* found our UDP table entry */
         }
      if (!tmp) 
         goto done;
      so = (struct socket *)tmp->u_data;
      /* May be non-socket (lightweight) UDP connection. */
      if (so->so_type != SOCK_DGRAM)
         goto done;
      so->so_error = ECONNREFUSED;  /* set error for socket owner */
      /* do a select() notify on socket here */
      sorwakeup(so);
      sowwakeup(so);
   }
#endif   /* UDP_SOCKETS */
   else
      goto done;

#ifdef IP_PMTU
   /* if this is a datagram-too-big message, update the Path MTU cache */
   if (pdp->dcode == DSTFRAG)
      pmtucache_set(pdp->dip.ip_dest, htons(pdp->dno2));
#endif   /* IP_PMTU */

done:
   LOCK_NET_RESOURCE(FREEQ_RESID);
   pk_free(p); /* done with original packet */
   UNLOCK_NET_RESOURCE(FREEQ_RESID);
   return;
}


/* FUNCTION: tcp_tick()
 *
 * tcp_tick() - Run the tcp timers. This should be called at least 
 * TPS times per second. 
 *
 * 
 * PARAM1: 
 *
 * RETURNS: 
 */

unsigned long nextslow = 0L;     /* next slow tcp timer time */
static int in_tcptick = 0;       /* reentry gaurd */

void
tcp_tick()
{
   /* guard against re-entry */
   if (in_tcptick)
      return;
   in_tcptick++;

   LOCK_NET_RESOURCE(NET_RESID);

   if (cticks >= nextslow) /* time to do it again */
   {
      tcp_slowtimo();      /* call routine in BSD tcp_timr.c */
#ifdef CSUM_DEMO
      nextslow = cticks + (TPS/5);  /* another 200 ms */
#else
      nextslow = cticks + (TPS/2);  /* another 500 ms */
#endif
   }

#ifdef DO_DELAY_ACKS
   tcp_fasttimo();
#endif   /* DO_DELAY_ACKS */

   UNLOCK_NET_RESOURCE(NET_RESID);

   in_tcptick--;
}


#endif /* INCLUDE_TCP */

