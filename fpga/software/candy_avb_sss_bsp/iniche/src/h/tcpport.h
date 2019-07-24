/*
 * FILENAME: tcpport.h
 *
 * Copyright 1997 - 2000 By InterNiche Technologies Inc. All rights reserved
 *
 * TCP port dependent definitions. 
 *
 * MODULE: INET
 *
 *
 * PORTABLE: usually
 */

/* Additional Copyrights: */
/* Portions Copyright 1996 by NetPort Software. All rights reserved. */


#ifndef _TCPPORT_H
#define  _TCPPORT_H  1

#ifndef _IPPORT_H_
#error You must first include ipport.h
#endif   /*  _IPPORT_H_ */

#ifdef MINI_TCP
#error This file only for use with full-size TCP
#endif

#include "libport.h"
#include "q.h"
#include "netbuf.h"
#include "net.h"
#include "ip.h"


#ifdef IP_V6
#include "ip6.h"
#include "socket6.h"
#endif   /* IP_V6 */


/* Define a default TCP MSS *Max. Segment Size) which will allow us to send
 * efficiently over our interface(s) without IP fragmenting. We default to
 * regular ethernet, and reduce the size if we have to deal with slower (PPP) or
 * badly designed legacy (802.3) links.
 */

#ifndef  TCP_MSS           /* allow override in ipport.h */
#ifdef   IPSEC
#define  TCP_MSS  0x04da   /* reduce to allow room for IPSEC encapsulation */
#elif  defined(IEEE_802_3)
#define  TCP_MSS  0x05ac   /* reduce for extra overhead of SNAP */
#elif  defined(USE_PPP)
#ifdef PPP_VJC
#define  TCP_MSS  0x0530   /* reduce by another 128 (VJC_HDR_OFFSET) on PPP */
#else
#define  TCP_MSS  0x05b0   /* reduce to avoid fragments on PPP */
#endif   /* PPP_VJC */
#else
#define  TCP_MSS  0x05b4   /* assume normal ethernet */
#endif   /* media type */
#endif   /* TCP_MSS */


/* TCP code options - these need to preceed the nptcp.h include */
#ifndef OMIT_IPV4
#define  UDP_SOCKETS    1  /* support UDP (DGRAM) under sockets API */
#endif

#define  SO_SELECT      1  /* support select() call */
#define  BSDTCP_STATS   1  /* printfs for extensive BSD statistics */

#ifdef NOTDEF
TCP options not used in this build:
#define  DO_TCPTRACE    1  /* copius debug tracing of TCP logic */
#endif

#include "nptcp.h"      /* InterNIche<->BSD mappings */
#include "mbuf.h"       /* BSD-ish Sockets includes */

#ifdef INCLUDE_SNMP
struct variable;
#include "snmpport.h"
extern   struct tcp_mib tcpmib;
#else    /* no SNMP, support tcp_mib locally for tcp_stats() */
struct TcpMib
{
   long     tcpRtoAlgorithm;
   long     tcpRtoMin;
   long     tcpRtoMax;
   long     tcpMaxConn;
   u_long   tcpActiveOpens;
   u_long   tcpPassiveOpens;
   u_long   tcpAttemptFails;
   u_long   tcpEstabResets;
   u_long   tcpCurrEstab;
   u_long   tcpInSegs;
   u_long   tcpOutSegs;
   u_long   tcpRetransSegs;
   void *   tcpConnTable;  /*32 bit ptr */
   u_long   tcpInErrs;
   u_long   tcpOutRsts;
};
extern   struct TcpMib  tcpmib; 
#endif   /* INCLUDE_SNMP */

#define  TCP_MIB_INC(varname) {tcpmib.varname++;}

#ifdef CRUDE_ALLOCS

/* prototype TCP ALLOC and FREE routines  */
struct inpcb * INP_ALLOC(int size);       /* alloc internet control block "inpcb" */
void  INP_FREE(struct inpcb *);           /* free internet control block */
struct mbuf * MBU_ALLOC(int size);        /* alloc mbuf struct */
struct socket * SOC_ALLOC(int size);      /* alloc a socket structure */
void  SOC_FREE(struct socket *);          /* free a socket structure */
struct tcpiphdr * TPH_ALLOC(int size);    /* alloc a TCP/IP header buffer */ 
void  TPH_FREE(struct tcpiphdr *);        /* free a TCP/IP header buffer */ 
struct tcpcb * TCB_ALLOC(int size);       /* ... tcp connection structure */
void  TCB_FREE(struct tcpcb *);
struct ip_socopts * SOCOPT_ALLOC(int size); /* socket options (IP_TOS, IP_TTL_OPT) */
void  SOCOPT_FREE(struct ip_socopts *);

#else /* not so CRUDE_ALLOCS */

/* MAP ALLOC and FREE calls to port's memory manager */
#define  INP_ALLOC(size)      (struct inpcb *)npalloc(size)
#define  INP_FREE(ptr)        npfree((void*)ptr)
#define  MBU_ALLOC(size)      (struct mbuf *)npalloc(size)
#define  SOC_ALLOC(size)      (struct socket *)npalloc(size)
#define  SOC_FREE(ptr)        npfree((void*)ptr)
#define  TPH_ALLOC(size)      (struct tcpiphdr *)npalloc(size)
#define  TPH_FREE(ptr)        npfree((void*)ptr)
#define  TCB_ALLOC(size)      (struct tcpcb *)npalloc(size)
#define  TCB_FREE(ptr)        npfree((void*)ptr)
#define  SOCOPT_ALLOC(size)   (struct ip_socopts *)npalloc(size)
#define  SOCOPT_FREE(ptr)     npfree((void*)ptr)

#endif   /* alloc macros */

#ifdef WEBPORT
extern   int http_init(void);
#endif

#ifdef SO_SELECT
/* define the size of the sockets arrays passed to select(). On UNIX 
 * and winsock this is usually 64, but most embedded systems don't 
 * need more than 1 or 2, and can't always afford to waste the space. 
 * NOTE: These determine the size of set_fd structs, which are often 
 */
#ifndef FD_SETSIZE   /* let h_h files override */
#define  FD_SETSIZE     12
#endif   /* FD_SETSIZE */
#endif   /* SO_SELECT */

#ifdef DO_TCPTRACE
/* void tcp_trace(char *, ...); */
#define  tcp_trace   printf
#endif


/* Calculate mbuf reserved space for MAC + IP + TCP + OPTIONS.
 * This is based on ethernet, 802.3 and and PPPoE since they are the
 * largest of our default MAC header sizes.
 */
 
#ifndef ETHHDR_BIAS
#define ETHHDR_BIAS 0
#endif

/* If we are using RFC-1323 TCP timestamp feature allow space for the
 * option in each packet
 */
#ifdef TCP_TIMESTAMP
#define  RTTMOPT_SIZE   12    /* Actual size plus alignment pad */
#else
#define  RTTMOPT_SIZE   0
#endif /* TCP_TIMESTAMP */


/* Figure out a minimum value */
#ifdef USE_PPPOE
#define  HDRSMINLEN  (64 + ETHHDR_BIAS + RTTMOPT_SIZE + IPSECOPT_SIZE)
#elif  defined(IEEE_802_3)
#define  HDRSMINLEN  (62 + ETHHDR_BIAS + RTTMOPT_SIZE + IPSECOPT_SIZE)
#else
#define  HDRSMINLEN  (54 + ETHHDR_BIAS + RTTMOPT_SIZE + IPSECOPT_SIZE)
#endif

/* Figure out the actual value: minimum value padded out to 
 * a multiple of ALIGN_TYPE
 */
#if ((HDRSMINLEN & (ALIGN_TYPE - 1)) == 0)
#define  HDRSLEN  HDRSMINLEN
#else
#define  HDRSLEN  (HDRSMINLEN + ALIGN_TYPE - (HDRSMINLEN & (ALIGN_TYPE - 1)))
#endif


/* macros to convert between 32-bit long socket descriptors and the 
 * actual socket structure pointers. On most systems they just cast 
 * the values. They are provided to support 48 bit "large model" 
 * 386/pentium builds, which 
 */

#ifndef  LONG2SO
#define  LONG2SO(ln)    ((struct socket *)ln)
#define  SO2LONG(so)    ((long)so)
#define  LONG2MBUF(ln)  ((struct mbuf *)ln)
#define  MBUF2LONG(mb)  ((long)mb)
#endif

/*
 * Altera Niche Stack modification:
 * Ensure that socket FD's are always positive and greater 
 * than ALT_MAX_FD.
 */
#ifdef ALT_INICHE

#undef LONG2SO
#undef SO2LONG
#define LONG2SO(ln) ((struct socket *) ((ln - ALT_MAX_FD) << 2))
#define SO2LONG(so) ((long) ((((unsigned long)so) >> 2) + ALT_MAX_FD))

#endif /* ALT_INICHE */

/* Now that TCP options are set up, include the sockets definitions */

#include "socket.h"
#include "sockvar.h"
#include "sockcall.h"
/*
 * Altera Niche Stack Nios port modification
 */
#if defined (ALT_INICHE) && defined (BSD_SOCKETS)
#include "bsdsock.h"
#endif
/* get a smart TCP_MSS for a socket. If socket is null then the default value
 * frome the TCP_MSS defrinitions above is returned.
 */
int   tcp_mss (struct socket *);

/* map protable "sys_" calls to InterNiche Sockets library */
#ifdef INTEGRITY  /* Intergrity has it's own legacy mapping */
#include "../ghsintl/isockmap.h"

#else    /* InterNiche full-size stack BSD-ish sockets calls */

#ifndef SYSMACRO_ALREADY   /* Allow override from ipport.h */
/* Map ancient NetPort "sys_" socket calls to the newer (version 2.0)
 * "standard" socket calls used in 2.0
 */
#define  sys_closesocket(sock)   t_socketclose(sock)
#define  sys_send(sock, buf,  len,  flags)   t_send(sock,   buf,  len,  flags)
#define  sys_recv(sock, buf,  len,  flags)   t_recv(sock,   buf,  len,  flags)
#define  sys_accept(X,Y,L)       t_accept(X,Y,L)
#define  sys_bind(X,Y,L)         t_bind(X,Y,L)
#define  sys_socket              t_socket
#define  sys_listen              t_listen
#define  sys_errno(sock)         t_errno(sock)
#define  sys_connect(X,Y,L)      t_connect(X,Y,L)
#define  sys_shutdown            t_shutdown

#define SYS_EWOULDBLOCK EWOULDBLOCK

/* Map plain BSD socket routine names to Interniche t_" names. As always,
 * allow this to be overridden by ipport.h_h
 */
#ifndef USER_SOCKETNAMES

/*
 * Altera Niche Stack Nios port modification:
 * BSD sockets support 
 */
#if defined (ALT_INICHE) && defined (BSD_SOCKETS)

#define  socket(p1, p2, p3)                 bsd_socket(p1, p2, p3)
#define  bind(p1, p2, p3)                   bsd_bind(p1, p2, p3)
#define  connect(p1, p2, p3)                bsd_connect(p1, p2, p3)
#define  listen(p1, p2)                     bsd_listen(p1, p2)
#define  send(p1, p2, p3, p4)               bsd_send(p1, p2, p3, p4)
#define  recv(p1, p2, p3, p4)               bsd_recv(p1, p2, p3, p4)
#define  accept(p1, p2, p3)                 bsd_accept(p1, p2, p3)
#define  sendto(p1, p2, p3, p4, p5, p6)     bsd_sendto(p1, p2, p3, p4, p5, p6)
#define  recvfrom(p1, p2, p3, p4, p5, p6)   bsd_recvfrom(p1, p2, p3, p4, p5, p6)
#define  setsockopt(p1, p2, p3, p4, p5)     bsd_setsockopt(p1, p2, p3, p4, p5)
#define  select(p1, p2, p3, p4, p5)         bsd_select(p1, p2, p3, p4, p5)
#define  inet_aton(p1, p2)                  bsd_inet_aton(p1, p2)
#define  inet_ntoa(p1)                      bsd_inet_ntoa(p1)
#define  getsockname(p1, p2, p3)            bsd_getsockname(p1, p2, p3)
// There is no BSD close mapping; certain Niche stack code
// calls socketclose() directly. In the Nios port, we map
// close() to either the altera device-specific close routine,
// or to t_socketclose() anyway.
#define  socketclose(s)                     t_socketclose(s)
#else /* not (ALT_INICHE && BSD_SOCKETS) */

#define  socket(x,y,z)           t_socket(x,y,z)
#define  bind(s,a,l)             t_bind(s,a,l)
#define  connect(s,a,l)          t_connect(s,a,l)
#define  listen(s,c)             t_listen(s,c)
#define  send(s, b, l, f)        t_send(s, b, l, f)
#define  recv(s, b, l, f)        t_recv(s, b, l, f)
#define  accept(s,a,l)           t_accept(s, a, l)
#define  sendto(s,b,l,f,a,x)     t_sendto(s,b,l,f,a,x)
#define  recvfrom(s,b,l,f,a,x)   t_recvfrom(s,b,l,f,a,x)
#define  socketclose(s)          t_socketclose(s)
#define  setsockopt(s,l,o,d,x)   t_setsockopt(s,l,o,d,x)
#endif

#endif /* not (ALT_INICHE & BSD_SOCKETS */

#endif   /* SYSMACRO_ALREADY */

#endif   /* sockets mapping */


#define  WP_SOCKTYPE long     /* WebPort socket type */
#define  SOCKTYPE    long     /* preferred generic socket type */



#endif   /* _TCPPORT_H */
/* end of file tcpport.h */


