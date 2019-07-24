/*
 * FILENAME: socket.h
 *
 * Copyright 1997 - 2000 By InterNiche Technologies Inc. All rights reserved
 *
 *
 * MODULE: TCP
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

#ifndef SOCKET_H
#define  SOCKET_H 1

/*
 * Types
 */
#define     SOCK_STREAM    1     /* stream socket */
#define     SOCK_DGRAM     2     /* datagram socket */
#define     SOCK_RAW       3     /* raw-protocol interface */

#ifndef NPPORT
#define     SOCK_RDM       4     /* reliably-delivered message */
#define     SOCK_SEQPACKET 5     /* sequenced packet stream */
#endif   /* NPPORT*/

/*
 * Option flags per-socket.
 */
#define     SO_DEBUG       0x00001     /* turn on debugging info recording */
#define     SO_ACCEPTCONN  0x00002     /* socket has had listen() */
#define     SO_REUSEADDR   0x00004     /* allow local address reuse */
#define     SO_KEEPALIVE   0x00008     /* keep connections alive */
#define     SO_DONTROUTE   0x00010     /* just use interface addresses */
#define     SO_BROADCAST   0x00020     /* permit sending of broadcast msgs */
#define     SO_USELOOPBACK 0x00040     /* bypass hardware when possible */
#define     SO_LINGER      0x00080     /* linger on close if data present */
#define     SO_OOBINLINE   0x00100     /* leave received OOB data in line */
#define     SO_TCPSACK     0x00200     /* Allow TCP SACK (Selective acknowledgment) */
#define     SO_WINSCALE    0x00400     /* Set scaling window option */ 
#define     SO_TIMESTAMP   0x00800     /* Set TCP timestamp option */ 
#define     SO_BIGCWND     0x01000     /* Large initial TCP Congenstion window */ 
#define     SO_HDRINCL     0x02000     /* user access to IP hdr for SOCK_RAW */
#define     SO_NOSLOWSTART 0x04000     /* suppress slowstart on this socket */
#define     SO_FULLMSS     0x08000     /* force packets to all be MAX size */

extern   unshort socket_defaults;      /* default so_options fo new sockets */

/* for compatability with second-rate stacks: */
#define SO_EXPEDITE     SO_NOSLOWSTART
#define SO_THROUGHPUT   SO_FULLMSS

/*
 * Additional options, not kept in so_options.
 */
#define  SO_SNDBUF      0x1001      /* send buffer size */
#define  SO_RCVBUF      0x1002      /* receive buffer size */
#define  SO_SNDLOWAT    0x1003      /* send low-water mark */
#define  SO_RCVLOWAT    0x1004      /* receive low-water mark */
#define  SO_SNDTIMEO    0x1005      /* send timeout */
#define  SO_RCVTIMEO    0x1006      /* receive timeout */
#define  SO_ERROR       0x1007      /* get error status and clear */
#define  SO_TYPE        0x1008      /* get socket type */
#define  SO_HOPCNT      0x1009      /* Hop count to get to dst   */
#define  SO_MAXMSG      0x1010      /* get TCP_MSS (max segment size) */

/* ...And some netport additions to setsockopt: */
#define  SO_RXDATA      0x1011      /* get count of bytes in sb_rcv */
#define  SO_TXDATA      0x1012      /* get count of bytes in sb_snd */
#define  SO_MYADDR      0x1013      /* return my IP address */
#define  SO_NBIO        0x1014      /* set socket into NON-blocking mode */
#define  SO_BIO         0x1015      /* set socket into blocking mode */
#define  SO_NONBLOCK    0x1016      /* set/get blocking mode via optval param */
#define  SO_CALLBACK    0x1017      /* set/get zero_copy callback routine */

/*
 * TCP User-settable options (used with setsockopt).
 * TCP-specific socket options use the 0x2000 number space.
 */

#define  TCP_ACKDELAYTIME 0x2001    /* Set time for delayed acks */
#define  TCP_NOACKDELAY   0x2002    /* suppress delayed ACKs */
#define  TCP_MAXSEG       0x2003    /* set maximum segment size */
#define  TCP_NODELAY      0x2004    /* Disable Nagle Algorithm */


/*
 * Structure used for manipulating linger option.
 */
struct   linger
{
   int   l_onoff;    /* option on/off */
   int   l_linger;   /* linger time */
};


/*
 * Structure used by kernel to store most
 * addresses.
 */
struct sockaddr
{
   u_short     sa_family;     /* address family */
#ifdef IP_V6   /* V6 only or dual stacks */
   char     sa_data[32];      /* big enough for unpacked sockaddr_in6 */
#else          /* Ancient IPv4 only version */
   char     sa_data[14];      /* up to 14 bytes of direct address */
#endif
};

/*
 * Structure used by kernel to pass protocol
 * information in raw sockets.
 */
struct sockproto
{
   u_short     sp_family;     /* address family */
   u_short     sp_protocol;   /* protocol */
};

/* Support for Large initial congestion window */
#ifdef TCP_BIGCWND
extern   int      use_default_cwnd;    /* Flag to use this on all sockets */
extern   u_long   default_cwnd;        /* initial cwnd value to use */
#endif   /* TCP_BIGCWND */

/*
 * Protocol families, same as address families for now.
 */
#define  PF_UNSPEC   AF_UNSPEC
#define  PF_UNIX     AF_UNIX
#define  PF_INET     AF_INET
#define  PF_IMPLINK  AF_IMPLINK
#define  PF_PUP      AF_PUP
#define  PF_CHAOS    AF_CHAOS
#define  PF_NS       AF_NS
#define  PF_NBS      AF_NBS
#define  PF_ECMA     AF_ECMA
#define  PF_DATAKIT  AF_DATAKIT
#define  PF_CCITT    AF_CCITT
#define  PF_SNA      AF_SNA
#define  PF_DECnet   AF_DECnet
#define  PF_DLI      AF_DLI
#define  PF_LAT      AF_LAT
#define  PF_HYLINK   AF_HYLINK
#define  PF_APPLETALK   AF_APPLETALK


/*
 * Maximum queue length specifiable by listen.
 */
#define     SOMAXCONN   5

#define     MSG_OOB        0x1      /* process out-of-band data */
#define     MSG_PEEK       0x2      /* peek at incoming message */
#define     MSG_DONTROUTE  0x4      /* send without using routing tables */
#define     MSG_NEWPIPE    0x8      /* New pipe for recvfrom call   */
#define     MSG_EOR        0x10     /* data completes record */
#define     MSG_DONTWAIT   0x20     /* this message should be nonblocking */

/* utility functions defined in misclib\parseip.c */
extern int inet46_addr(char *str, struct sockaddr *address); 
extern void inet_setport(struct sockaddr *addr,int port);

#endif   /* SOCKET_H */

/* end of file socket.h */


