/*
 * FILENAME: tcp_var.h
 *
 * Copyright 1997-2007 By InterNiche Technologies, Inc. All rights reserved.
 *
 *
 * MODULE: TCP
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

#ifndef _TCP_VAR_H
#define  _TCP_VAR_H  1

#ifndef  IP_MAXPACKET
#define  IP_MAXPACKET   (24<<10)    /* maximum packet size */
#endif

#ifndef  SACK_BLOCKS    /* allow ipport.h to override */
#define  SACK_BLOCKS    3
#endif   /*  SACK_BLOCKS */

#ifdef TCP_WIN_SCALE
extern   u_char   default_wind_scale;  /* default scaling value */
#undef TCP_WIN_SHORT                   /* force 32-bit windows */
#endif   /* TCP_WIN_SCALE */

#ifdef TCP_WIN_SHORT
typedef  u_short  tcp_win;    /* original 1 bit window size */
#else
typedef  u_long   tcp_win;    /* scale windows up to 32 bits */
#endif   /* TCP_WIN_SCALE */


/* default window sizes from tcp_usr.c */
extern   u_long   tcp_sendspace;
extern   u_long   tcp_recvspace;

/*
 * Tcp control block, one per tcp; fields:
 */
struct tcpcb
{
   struct   tcpiphdr *  seg_next;   /* sequencing queue */
   struct   tcpiphdr *  seg_prev;
   int      t_state;             /* state of this connection */
   int      t_timer[TCPT_NTIMERS];  /* tcp timers */
   int      t_rxtshift;          /* log(2) of rexmt exp. backoff */
   int      t_rxtcur;            /* current retransmit value */
   int      t_dupacks;           /* consecutive dup acks recd */
   u_short  t_maxseg;            /* maximum segment size */
   char     t_force;             /* 1 if forcing out a byte */
   u_short  t_flags;             /* mask of the TF_ state bits below */
   struct tcpiphdr * t_template; /* skeletal packet for transmit */
   struct inpcb * t_inpcb;       /* back pointer to internet pcb */
   /*
    * The following fields are used as in the protocol specification.
    * See RFC783, Dec. 1981, page 21.
    */
   /* send sequence variables */
   tcp_seq     snd_una;    /* send unacknowledged */
   tcp_seq     snd_nxt;    /* send next */
   tcp_seq     snd_up;     /* send urgent pointer */
   tcp_seq     snd_wl1;    /* window update seg seq number */
   tcp_seq     snd_wl2;    /* window update seg ack number */
   tcp_seq     iss;        /* initial send sequence number */
   tcp_win     snd_wnd;    /* send window */
   /* receive sequence variables */
   tcp_win     rcv_wnd;    /* receive window */
   tcp_seq     rcv_nxt;    /* receive next */
   tcp_seq     rcv_up;     /* receive urgent pointer */
   tcp_seq     irs;        /* initial receive sequence number */

   /*
    * Additional variables for this implementation.
    */
   /* receive variables */
   tcp_seq     rcv_adv;    /* advertised window */

   /* retransmit variables */
   /* highest sequence number sent used to recognize retransmits */
   tcp_seq   snd_max;

   /* congestion control (for slow start, source quench, retransmit after loss) */
   tcp_win  snd_cwnd;      /* congestion-controlled window */

   /* snd_cwnd size threshhold for for slow start exponential to
    * linear switch
    */
   tcp_win  snd_ssthresh;

   /*
    * transmit timing stuff.
    * srtt and rttvar are stored as fixed point; for convenience in smoothing,
    * srtt has 3 bits to the right of the binary point, rttvar has 2.
    * "Variance" is actually smoothed difference.
    */
   int      t_idle;        /* inactivity time */
   u_long   t_rttick;      /* cticks if timing RTT, else 0 */
   tcp_seq  t_rtseq;       /* sequence number being timed */
   int      t_srtt;        /* smoothed round-trip time */
   int      t_rttvar;      /* variance in round-trip time */
   tcp_win  max_rcvd;      /* most peer has sent into window */
   tcp_win  max_sndwnd;    /* largest window peer has offered */
   /* out-of-band data */
   char     t_oobflags;    /* have some */
   char     t_iobc;        /* input character */
#define     TCPOOB_HAVEDATA   0x01
#define     TCPOOB_HADDATA    0x02

#ifdef TCP_SACK
   /* seq numbers in last SACK option sent. sack_seq[0] is last segment
    * which triggered a SACK.
    */
   tcp_seq  sack_seq[SACK_BLOCKS];
   int      sack_seqct;    /* number of valid sack_seq[] entries */

   /* start & end of current SACK hole(s) to fill with a send. If these are
    * both zero then there is no SACK hole specified for the send. These
    * are updated whever a sack header is received and cleared when the
    * missing segments are send by the rexmit timer.
    */
   tcp_seq  sack_hole_start[SACK_BLOCKS];
   tcp_seq  sack_hole_end[SACK_BLOCKS];

   /* The seq number in our send stream which triggered the last sack
    * option header from the other side.
    */
   tcp_seq  sack_trigger;
#endif /* TCP_SACK */

#ifdef TCP_WIN_SCALE
   /* Scales factors if TF_TIMESTAMP bit is set */
   u_char   snd_wind_scale;      /* RFC-1323 Snd.Wind.Scale (S) */
   u_char   rcv_wind_scale;      /* RFC-1323 Rcv.Wind.Scale (R) */
#endif   /* TCP_WIN_SCALE */

#ifdef TCP_TIMESTAMP
   u_long   ts_recent;           /* RFC-1323 TS.Recent (peer's timestamp) */
   tcp_seq  last_ack;            /* RFC-1323 Last.ACK (last ack to peer) */
#endif   /* TCP_TIMESTAMP */

#ifdef DO_DELAY_ACKS
   long     t_delacktime;        /* Time (in cticks) for delaying ACK */
   long     t_delackcur;         /* Time left Time for delaying this ACK */
#endif   /* DO_DELAY_ACKS */
};


/* bit values for tcpcb.t_flags field */
#define     TF_ACKNOW      0x0001     /* ack peer immediately */
#define     TF_DELACK      0x0002     /* ack, but try to delay it */
#define     TF_NODELAY     0x0004     /* don't delay packets to coalesce */
#define     TF_NOOPT       0x0008     /* don't use tcp options */
#define     TF_SENTFIN     0x0010     /* have sent FIN */
#define     TF_SACKOK      0x0020     /* negotiated SACK on this conn.*/
#define     TF_SACKNOW     0x0040     /* send SACK option packet now */
#define     TF_WINSCALE    0x0080     /* negotiated a scaled window */
#define     TF_TIMESTAMP   0x0100     /* negotiated timestamp option */
#define     TF_SACKREPLY   0x0200     /* send reply to peer's sack options */
#define     TF_MAXSEG      0x0400     /* user has set max seg */


#ifdef TCP_SACK
int      tcp_buildsack(struct tcpcb * tp, u_char * opt);
void     tcp_resendsack(u_char * opt, struct tcpcb * tp);
u_char * tcp_putseq(u_char * cp, tcp_seq seq);
u_long   tcp_getseq(u_char * cp);
void     tcp_setsackinfo(struct tcpcb * tp, struct tcpiphdr * ti);
#endif /* TCP_SACK */


#define     intotcpcb(ip)     ((struct tcpcb *)(ip)->inp_ppcb)
#define     sototcpcb(so)     (intotcpcb(sotoinpcb(so)))

/*
 * TCP statistics.
 * Many of these should be kept per connection,
 * but that's inconvenient at the moment.
 */
struct   tcpstat
{
   u_long   tcps_connattempt;    /* connections initiated */
   u_long   tcps_accepts;        /* connections accepted */
   u_long   tcps_connects;       /* connections established */
   u_long   tcps_drops;          /* connections dropped */
   u_long   tcps_conndrops;      /* embryonic connections dropped */
   u_long   tcps_closed;         /* conn. closed (includes drops) */
   u_long   tcps_segstimed;      /* segs where we tried to get rtt */
   u_long   tcps_rttupdated;     /* times we succeeded */
   u_long   tcps_delack;         /* delayed acks sent */
   u_long   tcps_timeoutdrop;    /* conn. dropped in rxmt timeout */
   u_long   tcps_rexmttimeo;     /* retransmit timeouts */
   u_long   tcps_persisttimeo;   /* persist timeouts */
   u_long   tcps_keeptimeo;      /* keepalive timeouts */
   u_long   tcps_keepprobe;      /* keepalive probes sent */
   u_long   tcps_keepdrops;      /* connections dropped in keepalive */

   u_long   tcps_sndtotal;       /* total packets sent */
   u_long   tcps_sndpack;        /* data packets sent */
   u_long   tcps_sndbyte;        /* data bytes sent */
   u_long   tcps_sndrexmitpack;  /* data packets retransmitted */
   u_long   tcps_sndrexmitbyte;  /* data bytes retransmitted */
   u_long   tcps_sndacks;        /* ack-only packets sent */
   u_long   tcps_sndprobe;       /* window probes sent */
   u_long   tcps_sndurg;         /* packets sent with URG only */
   u_long   tcps_sndwinup;       /* window update-only packets sent */
   u_long   tcps_sndctrl;        /* control (SYN|FIN|RST) packets sent */

   u_long   tcps_rcvtotal;       /* total packets received */
   u_long   tcps_rcvpack;        /* packets received in sequence */
   u_long   tcps_rcvbyte;        /* bytes received in sequence */
   u_long   tcps_rcvbadsum;      /* packets received with ccksum errs */
   u_long   tcps_rcvbadoff;      /* packets received with bad offset */
   u_long   tcps_rcvshort;       /* packets received too short */
   u_long   tcps_rcvduppack;     /* duplicate-only packets received */
   u_long   tcps_rcvdupbyte;     /* duplicate-only bytes received */
   u_long   tcps_rcvpartduppack; /* packets with some duplicate data */
   u_long   tcps_rcvpartdupbyte; /* dup. bytes in part-dup. packets */
   u_long   tcps_rcvoopack;      /* out-of-order packets received */
   u_long   tcps_rcvoobyte;      /* out-of-order bytes received */
   u_long   tcps_rcvpackafterwin;   /* packets with data after window */
   u_long   tcps_rcvbyteafterwin;   /* bytes rcvd after window */
   u_long   tcps_rcvafterclose;  /* packets rcvd after "close" */
   u_long   tcps_rcvwinprobe;    /* rcvd window probe packets */
   u_long   tcps_rcvdupack;      /* rcvd duplicate acks */
   u_long   tcps_rcvacktoomuch;  /* rcvd acks for unsent data */
   u_long   tcps_rcvackpack;     /* rcvd ack packets */
   u_long   tcps_rcvackbyte;     /* bytes acked by rcvd acks */
   u_long   tcps_rcvwinupd;      /* rcvd window update packets */

   u_long   tcps_mcopies;        /* m_copy() actual copies */
   u_long   tcps_mclones;        /* m_copy() clones */
   u_long   tcps_mcopiedbytes;   /* m_copy() # bytes copied */
   u_long   tcps_mclonedbytes;   /* m_copy() # bytes cloned */

   u_long   tcps_oprepends;      /* ip_output() prepends of header to data */
   u_long   tcps_oappends;       /* ip_output() appends of data to header */
   u_long   tcps_ocopies;        /* ip_output() copies */
   u_long   tcps_predack;        /* VJ predicted-header acks */
   u_long   tcps_preddat;        /* VJ predicted-header data packets */
   u_long   tcps_zioacks;        /* acks recvd during zio sends */
#ifdef TCP_SACK
   u_long   tcps_sackconn;       /* connections which negotiated SACK */
   u_long   tcps_sacksent;       /* SACK option headers sent */
   u_long   tcps_sackresend;     /* segs resent because of recv SACKs */
   u_long   tcps_sackrcvd;       /* SACK option headers received */
   u_long   tcps_sackmaxblk;     /* most SACK blocks in a received option field */
#endif /* TCP_SACK */
};

extern   struct inpcb tcb; /* head of queue of active tcpcb's */
extern   struct tcpstat    tcpstat; /* tcp statistics */
extern   struct tcpcb *    tcp_timers  __P ((struct tcpcb *, int));
extern   struct tcpcb *    tcp_disconnect __P ((struct tcpcb *));
extern   struct tcpcb *    tcp_usrclosed  __P ((struct tcpcb *));
extern   struct tcpcb *    tcp_close   __P ((struct tcpcb *));
extern   struct tcpcb *    tcp_drop __P ((struct tcpcb *, int));
extern   struct tcpcb *    tcp_newtcpcb   __P ((struct inpcb *));

extern   int   tcp_usrreq __P ((struct socket *, struct mbuf *, struct mbuf *));
extern   int   tcp_output __P ((struct tcpcb *));
extern   int   tcp_ctloutput __P ((int, struct socket *, int, int, void *));
extern   void  tcp_input __P ((struct mbuf *, NET ifp));
extern   void  tcp_slowtimo   __P ((void));
extern   void  tcp_canceltimers  __P ((struct tcpcb *));
extern   void  tcp_init __P ((void));
extern   void  tcp_setpersist __P ((struct tcpcb *));
extern   void  tcp_notify __P ((struct inpcb *));
extern   void  tcp_quench __P ((struct inpcb *));
extern   void  tcp_respond __P ((struct tcpcb *, struct tcpiphdr *, u_long,
                 u_long, int, struct mbuf *));
extern   struct tcpiphdr * tcp_template __P ((struct tcpcb *));

/* IPv6 TCP send (to IP layer) routine */
extern   int   tcp6_send(struct tcpcb * tp, struct mbuf * m,
                  struct tcphdr *, int totallen, struct   ip_socopts * so_optsPack);

#endif

/* end of file tcp_var.h */


