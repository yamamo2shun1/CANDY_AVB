/*
 * FILENAME: icmp.h
 *
 * Copyright  2000 By InterNiche Technologies Inc. All rights reserved
 *
 * ICMP message handling
 *
 * MODULE: INET
 *
 * PORTABLE: yes
 */

/* Additional Copyrights: */

/* Portions Copyright 1990,1993 by NetPort Software. */
/* Portions Copyright 1986 by Carnegie Mellon */
/* Portions Copyright 1983 by the Massachusetts Institute of Technology */

#ifndef _ICMP_H_
#define  _ICMP_H_ 1


/* Define some ICMP messages */

/* ICMP dest unreachable types */
#define  DSTNET      0
#define  DSTHOST     1
#define  DSTPROT     2
#define  DSTPORT     3
#define  DSTFRAG     4
#define  DSTSRC      5


#define  ECHOREP     0     /* ICMP Echo reply */
#define  DESTIN      3     /* Destination Unreachable */
#define  SOURCEQ     4     /* Source quench */
#define  REDIR       5     /* Redirect */
#define  ECHOREQ     8     /* ICMP Echo request */
#define  TIMEX       11    /* Time exceeded */
#define  PARAM       12    /* Parameter problem */
#define  TIMEREQ     13    /* Timestamp request */
#define  TIMEREP     14
#define  INFO        15    /* Information request */

/* ICMP Time Exceeded codes */
#define TIMEX_REASSY_FAILED 1 /* code 1 ("fragment reassembly time exceeded") */

#define  ICMP_PROT   1     /* ICMP Protocol number on IP */
#define  ICMPSIZE    sizeof(struct  ping) /* default size for ICMP packet */

struct ping          /* ICMP Echo request/reply header */
{
   char     ptype;
   char     pcode;
   unshort  pchksum;
   unshort  pid;
   unshort  pseq;
};

/* structure of an icmp destination unreachable packet */

#define  ICMPDUDATA     8  /* size of extra data */

struct destun
{
   char     dtype;
   char     dcode;
   unshort  dchksum;
   unshort  dno1;
   unshort  dno2;
   struct ip   dip;    /* the offending IP packet */
   char     ddata[ICMPDUDATA];
};

/* structure of an icmp time exceeded packet */

/* length of "user" data (just past the "inner" IP header).  This is 
 * from the packet that the ICMP Time Exceeded message is being sent 
 * in response to. */
#define ICMPTIMEX_PAYLOAD_DATA_LEN 8
#define ICMPTIMEX_HDR_LEN 8 /* length of the ICMP header */

struct timex
{
   char     ttype;
   char     tcode;
   unshort  tchksum;
   unshort  tno1;
   unshort  tno2;
   struct ip   tip;
   char     tdata[8];
};

/* structure of a timestamp reply */

START_PACKED_STRUCT(tstamp)
   char     ttype;
   char     tcode;
   unshort txsum;
   unshort tid;
   unshort tseq;
   char     stamps[3 * sizeof(u_long)];  /* some compilers do bad aligns */
  /* long     dtstamp[3]; */ /* was "tstamp", use of structname upsets compilers */
END_PACKED_STRUCT(tstamp)

/* structure of an icmp redirect */

struct redirect
{
   char     rdtype;
   char     rdcode;
   unshort  rdchksum;
   ip_addr  rdgw;
   struct ip   rdip;
   char     rddata[8];
};


/* the snmp icmp variables */

struct IcmpMib {
   u_long   icmpInMsgs;          /* 1 - received icmp packets, including errors */
   u_long   icmpInErrors;        /* 2 - bad sums, bad len, etc. */
   u_long   icmpInDestUnreachs;  /* 3 - no comments for self explanitorys */
   u_long   icmpInTimeExcds;     /* 4 */
   u_long   icmpInParmProbs;     /* 5 */
   u_long   icmpInSrcQuenchs;    /* 6 */
   u_long   icmpInRedirects;     /* 7 */
   u_long   icmpInEchos;         /* 8 */
   u_long   icmpInEchoReps;      /* 9 */
   u_long   icmpInTimestamps;    /* 10 */
   u_long   icmpInTimestampReps; /* 11 */
   u_long   icmpInAddrMasks;     /* 12 */
   u_long   icmpInAddrMaskReps;  /* 13 */
   u_long   icmpOutMsgs;         /* 14 - total sent icmps, including errors */
   u_long   icmpOutErrors;       /* 15 - ICMP Layer errors ONLY (see rfc #1156) */
   u_long   icmpOutDestUnreachs; /* 16 */
   u_long   icmpOutTimeExcds;    /* 17 */
   u_long   icmpOutParmProbs;    /* 18 */
   u_long   icmpOutSrcQuenchs;   /* 19 */
   u_long   icmpOutRedirects;    /* 20 */
   u_long   icmpOutEchos;        /* 21 */
   u_long   icmpOutEchoReps;     /* 22 */
   u_long   icmpOutTimestamps;   /* 23 */
   u_long   icmpOutTimestampReps;/* 24 */
   u_long   icmpOutAddrMasks;    /* 25 */
   u_long   icmpOutAddrMaskReps; /* 26 */
};

extern   struct IcmpMib icmp_mib;

typedef struct IcmpMib * ICMPMIB;

/* hook in icmp.c for linked application to peek at ICMP DU packets */
extern   void (*icmpdu_hook)(PACKET p, struct destun * pdp);

int   icmp_init(void);
int   icmp_stats(void * pio);
#ifndef MINI_IP
void  icmp_destun(ip_addr, ip_addr, struct ip *, unsigned, NET);
#else
void  icmp_destun(ip_addr, struct ip *, unsigned, NET);
#endif
int   icmprcv(PACKET);
void  icmp_du(PACKET p, struct destun * pdp);
void  icmp_timex (struct ip * ip, char code);

/* in ping.c: routines to send & receive icmp pings */
int   icmpEcho(ip_addr, char*, unsigned, unshort);    /* send ping */

/* pingUpcall() must be implemented in the app if PING_APP is definied */
int   pingUpcall(PACKET);     /* upcall for ALL incoming ping packets */

#ifdef INCLUDE_TCP   /* sockets ICMPDU callback */
extern   void so_icmpdu(PACKET p, struct destun * pdp);
#endif   /* INCLUDE_TCP */

#ifndef _IPPORT_H_
#error You must first include ipport.h
#endif   /*  _IPPORT_H_ */

#ifndef ETHHDR_BIAS
#define ETHHDR_BIAS 0
#endif

/* Figure out a minimum value */
#ifdef USE_PPPOE
#define PINGHDRSMINLEN (44 + 8 + ETHHDR_BIAS + IPSECOPT_SIZE)
#elif  defined(IEEE_802_3)
#define PINGHDRSMINLEN (42 + 8 + ETHHDR_BIAS + IPSECOPT_SIZE)
#else
#define PINGHDRSMINLEN (34 + 8 + ETHHDR_BIAS + IPSECOPT_SIZE)
#endif

/* Figure out the actual value: minimum value padded out to 
 * a multiple of ALIGN_TYPE
 */
#if ((PINGHDRSMINLEN & (ALIGN_TYPE - 1)) == 0)
#define  PINGHDRSLEN  PINGHDRSMINLEN
#else
#define  PINGHDRSLEN  (PINGHDRSMINLEN + ALIGN_TYPE - (PINGHDRSMINLEN & (ALIGN_TYPE - 1)))
#endif

#endif   /* _ICMP_H_ */



