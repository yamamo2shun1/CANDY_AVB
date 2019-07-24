/*
 * FILENAME: sockvar.h
 *
 * Copyright 1997-2003 by InterNiche Technologies Inc. All rights reserved.
 *
 * Kernel structure per socket.
 * Contains send and receive buffer queues,
 * handle on protocol and pointer to protocol
 * private data and error information.
 *
 * MODULE: H
 *
 * PORTABLE: yes
 */

/* Additional Copyrights: */
/*
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

#ifndef SOCKVAR_H
#define  SOCKVAR_H   1

/*
 * Variables for socket buffering.
 */
struct   sockbuf
{
   u_int    sb_cc;      /* actual chars in buffer */
   u_int    sb_hiwat;   /* max actual char count */
   u_int    sb_mbcnt;   /* chars of mbufs used */
   u_int    sb_mbmax;   /* max chars of mbufs to use */
   u_int    sb_lowat;   /* low water mark (not used yet)*/
   u_int    sb_sel;     /* support select later? -JB- */
   struct mbuf * sb_mb; /* the   mbuf  chain */
   short    sb_flags;   /* flags, see below */
   short    sb_timeo;   /* Snd/Rcv timeout */
};

struct socket
{
   struct   socket * next; /* ptr to next socket */
   struct   inpcb * so_pcb;      /* protocol control block */
   struct   protosw * so_proto;  /* protocol handle */
#ifdef TCP_ZEROCOPY
   int   (*rx_upcall) __P ((struct socket * so, void * pkt, int error));
#endif
#ifdef IP_MULTICAST
   struct ip_moptions * inp_moptions;
#endif   /* IP_MULTICAST */
   u_long   so_options;    /* from socket call, see socket.h*/
   int      so_domain;     /* AF_INET or AF_INET6 */
   int      so_error;      /* error affecting connection */
   int      so_req;        /* request for pass to tcp_usrreq() */
   short    so_linger;     /* time to linger while closing */
   short    so_state;      /* internal state flags SS_*, below */
   short    so_timeo;      /* connection timeout */
   char     so_type;       /* generic type, see socket.h   */
   char     so_hopcnt;     /* Number of hops to dst   */

   /*
    * Variables for socket buffering.
    */
   struct   sockbuf  so_rcv,  so_snd;
   u_int    so_oobmark;       /* chars to oob mark */

   /*
    * Variables for connection queueing.
    * Socket where accepts occur is so_head in all subsidiary sockets.
    * If so_head is 0, socket is not related to an accept.
    * For head socket so_q0 queues partially completed connections,
    * while so_q is a queue of connections ready to be accepted.
    * If a connection is aborted and it has so_head set, then
    * it has to be pulled out of either so_q0 or so_q.
    * We allow connections to queue up based on current queue lengths
    * and limit on number of queued connections for this socket.
    */
   struct   socket * so_head; /* back pointer to accept socket */
   struct   socket * so_q0;   /* queue of partial connections */
   struct   socket * so_q;    /* queue of incoming connections */
   char     so_q0len;         /* partials on so_q0 */
   char     so_qlen;          /* number of connections on so_q */
   char     so_qlimit;        /* max number queued connections */
   struct   ip_socopts * so_optsPack; /* pointer to options, to be given to PACKET */
   /*
    * Altera Niche Stack Nios port modification:
    * Declare 'owner' as TK_OBJECT_REF as it is a subroutine
    * instead of simply a void * -- to fix build warning
    */
   TK_OBJECT_REF(owner);      /* owning task id for tasking-based systems
                               * (used only if SOCK_MAP_EVENTS is defined) */      
};

extern   queue soq;  /* Socket queue */

/* sockbuf defines */
#define  SB_MAX      (16*1024)   /* max chars in sockbuf */
#define  SB_LOCK     0x01     /* lock on data queue (so_rcv only) */
#define  SB_WANT     0x02     /* someone is waiting to lock */
#define  SB_WAIT     0x04     /* someone is waiting for data/space */
#define  SB_SEL      0x08     /* buffer is selected */
#define  SB_COLL     0x10     /* collision selecting */
#define  SB_NOINTR   0x40     /* operations not interruptible */
#define  SB_MBCOMP   0x80     /* can compress mbufs */

/*
 * Socket state bits.
 */
#define  SS_NOFDREF           0x0001   /* no file table ref any more */
#define  SS_ISCONNECTED       0x0002   /* socket connected to a peer */
#define  SS_ISCONNECTING      0x0004   /* in process of connecting to peer */
#define  SS_ISDISCONNECTING   0x0008   /*  in process  of disconnecting */
#define  SS_CANTSENDMORE      0x0010   /* can't send more data to peer */
#define  SS_CANTRCVMORE       0x0020   /* can't receive more data from peer */
#define  SS_RCVATMARK         0x0040   /* at mark on input */
#define  SS_PRIV              0x0080   /* privileged for broadcast, raw... */
#define  SS_NBIO              0x0100   /* non-blocking ops */
#define  SS_ASYNC             0x0200   /* async i/o notify */
#define  SS_UPCALLED          0x0400   /* zerocopy data has been upcalled (for select) */
#define  SS_INUPCALL          0x0800   /* inside zerocopy upcall (reentry guard) */
#define  SS_UPCFIN            0x1000   /* inside zerocopy upcall (reentry guard) */
#define  SS_WASCONNECTING     0x2000   /* SS_ISCONNECTING w/possible pending error */

/*
 * Macros for sockets and socket buffering.
 */

/* how much space is there in a socket buffer (so->so_snd or so->so_rcv) */
#define sbspace(sb) ((((int)(sb)->sb_hiwat - (int)(sb)->sb_cc) >= 0) \
     ? ((sb)->sb_hiwat -(sb)->sb_cc): 0 )

/* do we have to send all at once on a socket? */
#define   sosendallatonce(so) \
    ((so)->so_proto->pr_flags & PR_ATOMIC)

/* adjust counters in sb reflecting allocation of m */
#define   sballoc(sb, m) { \
   (sb)->sb_cc += (m)->m_len; \
}
/* adjust counters in sb reflecting freeing of m */
#define   sbfree(sb, m) { \
   (sb)->sb_cc -= (m)->m_len; \
}
/* set lock on sockbuf sb */
#define sblock(sb) { \
   while ((sb)->sb_flags & SB_LOCK) {          \
      tcp_sleep ((char *)&(sb)->sb_flags);       \
   }                      \
   (sb)->sb_flags |= SB_LOCK;             \
}
/* release lock on sockbuf sb */
#define   sbunlock(sb) {                   \
   (sb)->sb_flags &= ~SB_LOCK;             \
   tcp_wakeup((char *)&(sb)->sb_flags);       \
}
#define  sorwakeup(so)     sowakeup((so), &(so)->so_rcv)
#define  sowwakeup(so)     sowakeup((so), &(so)->so_snd)
#define  sowakeup(so,sb)   sbwakeup ((so), sb)

/*
 * Function proto-types for ANSI C
 */
extern   void  soisconnecting __P ((struct socket *));
extern   void  soisconnected  __P ((struct socket *));
extern   void  soisdisconnecting __P ((struct socket *));
extern   void  soisdisconnected  __P ((struct socket *));
extern   struct socket *   sonewconn   __P ((struct socket *));
extern   void  soqinsque   __P ((struct socket *, struct socket *, int));
extern   int   soqremque   __P ((struct socket *, int));
extern   void  socantsendmore __P ((struct socket *));
extern   void  socantrcvmore  __P ((struct socket *));
extern   int   sbreserve   __P ((struct sockbuf *, unsigned long));
extern   void  sbwakeup __P ((struct socket *, struct sockbuf *));
extern   void  sbrelease   __P ((struct sockbuf *));
extern   void  sbselqueue  __P ((struct sockbuf *));
extern   void  sbappend __P ((struct sockbuf * sb, struct mbuf *));
extern   void  sbappendrecord __P ((struct sockbuf * sb, struct mbuf *));
extern   int   sbappendaddr   __P ((struct sockbuf *, struct sockaddr *, struct mbuf *));
extern   void  sbflush  __P ((struct sockbuf *));
extern   void  sbdrop   __P ((struct sockbuf *, int));
extern   void  sbdropend   __P ((struct sockbuf *, struct mbuf *));
extern   void  sbdroprecord   __P ((struct sockbuf *));
#ifndef sowakeup
extern   void  sowakeup __P ((struct socket *, struct sockbuf *));
#endif
extern   void  sbwait   __P ((struct sockbuf *));

extern   void  insque   __P ((void *, void *));
extern   void  remque   __P ((void *));

/*
 * socket.c
 */
extern   struct socket *   socreate __P ((int, int, int));
extern   int   sobind   __P ((struct socket *, struct mbuf *));
extern   int   solisten __P ((struct socket *, int));
extern   void  sofree   __P ((struct socket *));
extern   int   soabort  __P ((struct socket *));
extern   int   soaccept __P ((struct socket *, struct mbuf *));
extern   int   soconnect   __P ((struct socket *, struct mbuf *));
extern   int   sodisconnect   __P ((struct socket *));
extern   void  sorflush __P ((struct socket *));
extern   int   sosetopt __P ((struct socket *, int, void *));
extern   int   sogetopt __P ((struct socket *, int, void *));
extern   void  sohasoutofband __P ((struct socket *));


/* more socket.c and socket2.c */

int   soreserve(struct socket * so, u_long sndcc, u_long rcvcc);
int   sosend(struct socket * so, struct mbuf * nam, 
            char * data, int * datalen, int flags);
int   soreceive(struct socket * so, struct mbuf ** nam, 
            char * data, int * datalen, int   flags);
int   soshutdown(struct socket * so, int how);
int   soclose(struct socket * so);
void  sbcompress(struct sockbuf *, struct mbuf *, struct mbuf *);

#endif   /* SOCKVAR_H */

/* end of file sockvar.h */


