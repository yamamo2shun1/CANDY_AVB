/*
 * FILENAME: ns.h
 *
 * Copyright  2000 By InterNiche Technologies Inc. All rights reserved
 *
 *
 * MODULE: DNS
 *
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

#define  NS_USE_SENDTO_FOR_CLIENT

/* interniche port includes for all DNS source files */
#include "ipport.h"
#include "vfsfiles.h"
#include "dnsport.h"


#ifndef __P /* for ansi */
#define  __P(args)   args
#endif
#define  bcmp  MEMCMP

#ifndef bcopy
#define  bcopy(src,dst,len)   MEMCPY   (dst,src,len)
#define  bzero(buf,len)       MEMSET   (buf, 0, len)
#endif   /* bcopy */

#define  strcasecmp  stricmp
#define  strncasecmp strnicmp

#define gettime(tvp) {u_long x; x = sysuptime (); (tvp)->tv_sec = x/100l;\
 (tvp)->tv_usec = x/10000l;}

extern   unsigned long  sysuptime (void); /* From dosmain.c */

struct in_addr;   /* predecl */
extern   char *   inet_ntoa   __P ((struct in_addr));
extern   unsigned long  inet_addr   __P ((char *));

/*extern char *print_ipad (struct in_addr);*/

/*
 * Altera Niche Stack Nios port modification:
 * inet_ntoa defined to bsd sockets layer
 */
#if !(defined (ALT_INICHE) && defined (BSD_SOCKETS))
#define  inet_ntoa(x)   print_ipad  (*(long *)&x)
#endif

#ifndef USE_WINSOCK
#define  SOCKET   long
#define  GET_SOCK_ERR(x)   t_errno  (x)
#else
#define  GET_SOCK_ERR(x)   WSAGetLastError   ()
#endif

#include "webport.h"
#include "in_utils.h"
#include "nameser.h"
#include "ns_debug.h"


/*
 * Global definitions and variables for the name server.
 */
/*
 * Timeout time should be around 1 minute or so.  Using the
 * the current simplistic backoff strategy, the sequence
 * retrys after 4, 8, and 16 seconds.  With 3 servers, this
 * dies out in a little more than a minute.
 * (sequence RETRYBASE, 2*RETRYBASE, 4*RETRYBASE... for MAXRETRY)
 */
#define  MINROOTS 2     /* min number of root hints */
#define  NSMAX    16    /* max number of NS addrs to try */
#define  RETRYBASE   4  /* base time between retries */
#define  MAXRETRY 3     /* max number of retries per addr */
#define  MAXCNAMES   8  /* max # of CNAMES tried per addr */
#define  MAXQUERIES  20 /* max # of queries to be made */
/* (prevent "recursive" loops) */
#define  INIT_REFRESH   600   /* retry time for initial secondary */
/* contact (10 minutes) */

#ifdef INTERNICHE
#define  allmem   npalloc
#define  allcmem(n,sz)  npalloc(n*sz)
#define freemem(x) {if ((x) == NULL) {printf ("Free-ing null\n");}\
   else npfree(x);}
#else /* Not Interniche */
#define  allmem(sz)  getmem   (sz,  MTYPE_DNS)
#define  allcmem(n,sz)  getcmem  (n*sz,   MTYPE_DNS)
#endif

#ifdef USE_FLOATING_POINT
#define  ALPHA    0.7   /* How much to preserver of old response time */
#define  BETA  1.2      /* How much to penalize response time on failure */
#define  GAMMA    0.98  /* How much to decay unused response times */
#else /* NO FLOAT */
#define  ALPHA    70    /* How much to preserver of old response time */
#define  BETA  120      /* How much to penalize response time on failure */
#define  GAMMA    98    /* How much to decay unused response times */
#endif   /* FLOAT */

struct zoneinfo   {
   int      z_type;        /* type of zone */
   int      z_auth;        /* zone is authoritative */
   char *   z_origin;      /* root domain name of zone */
   time_t   z_time;        /* time for next refresh */
   time_t   z_lastupdate;  /* time of last refresh */
   u_long   z_refresh;     /* refresh interval */
   u_long   z_retry;       /* refresh retry interval */
   u_long   z_expire;      /* expiration time for cached info */
   u_long   z_minimum;     /* minimum TTL value */
   u_long   z_serial;      /* changes if zone modified */
   char *   z_source;      /* source location of data */
   time_t   z_ftime;       /* modification time of source file */
   int      z_addrcnt;     /* address count */
   struct in_addr z_addr[NSMAX]; /* list of master servers for zone */
   int      z_state;       /* state bits; see below */
   u_short  z_xferpid;     /* xfer child pid */
#ifdef ALLOW_UPDATES
   /* non-zero if zone has been updated
    * since last checkpoint
    */
   int hasChanged;
#endif   /*ALLOW_UPDATES*/
};

/* zone types (z_type) */
#define  Z_PRIMARY   1
#define  Z_SECONDARY 2
#define  Z_CACHE  3

/* zone state bits */
#define  Z_AUTH   0x01        /* should replace z_auth */
#define  Z_NEED_XFER 0x02     /* waiting to do xfer */
#define  Z_XFER_RUNNING 0x04  /* asynch. xfer is running */
#define  Z_NEED_RELOAD  0x08  /* waiting to do reload */
#define  Z_SYSLOGGED 0x10     /* have logged timeout */
#define  Z_CHANGED   0x20     /* should replace hasChanged */
#define  Z_FOUND  0x40        /* found in boot file when reloading */
#define  Z_INCLUDE   0x80     /* set if include used in file */
#define  Z_DB_BAD 0x100       /* errors when loading file */
#define  Z_TMP_FILE  0x200    /* backup file for xfer is temporary */
#ifdef ALLOW_UPDATES
#define  Z_DYNAMIC   0x400       /* allow dynamic updates */
#define  Z_DYNADDONLY   0x800    /* dynamic mode: add new data only */
#endif   /*ALLOW_UPDATES*/

/* xfer exit codes */
#define  XFER_UPTODATE  0  /* zone is up-to-date */
#define  XFER_SUCCESS   1  /* performed transfer successfully */
#define  XFER_TIMEOUT   2  /* no server reachable/xfer timeout */
#define  XFER_FAIL   3     /* other failure, has been logged */

/*
 * Structure for recording info on forwarded queries.
 */
struct ns_qinfo   {
     u_short  q_id;     /* id of query */
     u_short  q_nsid;   /* id of forwarded query */
#ifdef NS_USE_SENDTO_FOR_CLIENT
     SOCKET   q_dfd;    /* UDP file descriptor */
#endif
     struct sockaddr_in   q_from;  /* requestor's address */
     char *   q_msg;    /* the message */
     int   q_msglen;    /* len of message */
     int   q_naddr;     /* number of addr's in q_addr */
     int   q_curaddr;   /* last addr sent to */
     struct fwdinfo *  q_fwd;   /* last forwarder used */
     time_t   q_time;   /* time to retry */
     struct ns_qinfo * q_next;  /* rexmit list (sorted by time) */
     struct ns_qinfo * q_link;  /* storage list (random order) */
     struct  qserv  {
       struct sockaddr_in   ns_addr; /* addresses of NS's */
       struct  databuf * ns;        /* databuf for NS record */
       struct  databuf * nsdata;    /* databuf for server address */
       struct  timeval   stime;     /* time first query started */
       int   nretry;                /* # of times addr retried */
     }  q_addr   [NSMAX]; /* addresses of NS's */
     struct databuf *  q_usedns[NSMAX];  /* databuf for NS that we've tried */
     int   q_nusedns;
     int   q_cname;        /* # of cnames found */
     int   q_nqueries;     /* # of queries required */
     char *   q_cmsg;      /* the cname message */
     int   q_cmsglen;      /* len of cname message */
     struct qstream *  q_stream;   /* TCP stream, null if UDP */
     int   q_system;   /* boolean, system query */
};

#define Q_NEXTADDR(qp,n) \
 (((qp)->q_fwd == (struct fwdinfo *)0) ? \
  &(qp)->q_addr[n].ns_addr : &(qp)->q_fwd->fwdaddr)

#define  PRIMING_CACHE  42
#define  QINFO_NULL  ((struct ns_qinfo *)0)

#ifndef XFER
extern   struct ns_qinfo * qfindid(void);
extern   struct ns_qinfo * qnew(void);
extern   struct ns_qinfo * retryqp; /* next query to retry */
#endif   /* XFER */

/*
 * Return codes from ns_forw:
 */
#define  FW_OK    0
#define  FW_DUP   1
#define  FW_NOSERVER 2
#define  FW_SERVFAIL 3

struct qstream {
   SOCKET   s_rfd;            /* stream file descriptor */
   int      s_size;           /* expected amount of data to recive */
   int      s_bufsize;        /* amount of data recived in s_buf */
   char *   s_buf;            /* buffer of recived data */
   char *   s_bufp;           /* pointer into s_buf of recived data */
   struct qstream *  s_next;  /* next stream */
   struct sockaddr_in s_from; /* address query came from */
   u_long   s_time;           /* time stamp of last transaction */
   int      s_refcnt;         /* number of outstanding queries */
   u_short  s_tempsize;       /* temporary for size from net */
};

#define  QSTREAM_NULL   ((struct qstream *)0)
extern   struct qstream *  streamq; /* stream queue */

struct qdatagram  {
   SOCKET   dq_dfd;              /* datagram file descriptor */
   struct qdatagram * dq_next;   /* next datagram */
   struct in_addr    dq_addr;    /* address of interface */
};

#define  QDATAGRAM_NULL ((struct qdatagram *)0)
extern   struct qdatagram *   datagramq;  /* datagram queue */

struct netinfo {
     struct netinfo *  next;
     u_long   net;
     u_long   mask;
     struct in_addr my_addr;
};

struct fwdinfo {
     struct fwdinfo *  next;
     struct sockaddr_in   fwdaddr;
};

struct nets {
     char *   name;
     long  net;
     struct nets *  next;
}; 

/*
 *  Statistics Defines
 */

struct DnsStats  {
   u_long dns_dupqueries;   /* Duplicate queries */
   u_long dns_outpkts;      /* Outbound DNS packets */
   u_long dns_queries;      /* DNS queries */
   u_long dns_inv_queries;  /* DNS inverse queries */
   u_long dns_respok;       /* Good DNS responses */
   u_long dns_respfail;     /* DNS responses failures */
   u_long dns_respfmterr;   /* DNS responses format errors */
   u_long dns_responses;    /* DNS responses */
   u_long dns_dupresp;      /* DNS dup responses */
   u_long dns_badresponses; /* DNS bad responses from servers */
   u_long dns_martians;     /* DNS responses from unexpected sources */
   u_long dns_prime_cache;  /* DNS number of times cache primed */
   u_long dns_sysqueries;   /* DNS number of sysqueries sent */
   u_long dns_check_ns;     /* DNS number of check_ns calls */
};

extern struct DnsStats dns_stats;

#ifdef DNS_STATS
#define  STATS_INCR(ind)   dns_stats   [ind]++
#else
#define  STATS_INCR(ind)
#endif

/* gross count of UDP packets in and out */
#define  S_INPKTS 0
#define  S_OUTPKTS   1
/* gross count of queries and inverse queries received */
#define  S_QUERIES   2
#define  S_IQUERIES  3
#define  S_DUPQUERIES   4
#define  S_RESPONSES 5
#define  S_DUPRESP   6
#define  S_RESPOK 7
#define  S_RESPFAIL  8
#define  S_RESPFORMERR  9
#define  S_SYSQUERIES   10
#define  S_PRIMECACHE   11
#define  S_CHECKNS   12
#define  S_BADRESPONSES 13
#define  S_MARTIANS  14

#define  S_NSTATS 15 /* Careful! */
#ifdef DNS_STATS
extern   struct stats   stats[S_NSTATS];
extern   unsigned long  typestats[T_ANY+1];
#endif



#ifdef DEBUG
extern   int   debug;   /* debug flag */
extern   FILE *   ddt;  /* debug file discriptor */
#endif
#ifndef XFER
extern   struct qdatagram *   dqp;

extern   struct itimerval  ival;    /* maintenance interval */
extern   struct zoneinfo * zones;   /* zone information */
extern   int   nzones;              /* number of zones in use */

extern   int   forward_only;  /* true on slave server */
#endif   /* XFER */

#define  MAX_XFER_TIME     60 *  60 *  2  /* max seconds for an xfer */
#define  XFER_TIME_FUDGE   10             /* MAX_XFER_TIME fudge */

#ifndef XFER
extern   int   xfer_running_cnt;          /* number of xfers running */
extern   int   xfer_deferred_cnt;         /* number of deferred xfers */
#define  MAX_XFERS_RUNNING       4        /* max value of xfer_running_cnt */
#endif   /* XFER */

#define  dn_skipname __dn_skipname

#ifndef FULL_DNS
#define  net_islocal(x) (0)
#endif

#define  NS_ROOT_CACHE_FILE "../dnssrv/root.dns"

