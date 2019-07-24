/*
 * FILENAME: socket6.h
 *
 * Copyright 2002 By InterNiche Technologies Inc. All rights reserved
 *
 * Socket definitions for IP version 6, as defined by RFC-2133 and RFC-2292
 *
 * MODULE: IP
 *
 *
 * PORTABLE: yes
 */

#ifndef _SOCKET6_H_
#define _SOCKET6_H_ 1

#include "ip6.h"
#include "icmp6.h"
#include "ip6mibs.h"


/* Mapping of Interniche types to the UNIXy types used in the RFC.
 * Comment these out if they conflict with other headers in your system.
 */
#define uint32_t  u_long
#define size_t    unsigned



#define INET_ADDRSTRLEN    16    /* length of printfed IPv4 addr */
#define INET6_ADDRSTRLEN   46    /* max length of printed IPv6 addr */

/* map posix types form the rfc to Iniche types */

#define u_int16m_t   unshort
#define u_int32m_t   u_long


struct sockaddr_in6
{
   u_short        sin6_family;   /* AF_INET6 */
   u_short        sin6_port;     /* transport layer port # */
   u_long         sin6_flowinfo; /* IPv6 flow information */
   ip6_addr       sin6_addr;     /* IPv6 address */
};

/* Use the unspecifed address as the wildcard IP address */
#define in6addr_any ip6unspecified

/* convert printable string to binary address */
int   inet_pton(int af, const char * src, void * dst);

/* convert binary address tp printable string */
const char * inet_ntop(int af, const void * src, char * dst, size_t size);


/* get data for prepending IP header (or something) onto mbuf */
struct mbuf *  mbuf_prepend(struct mbuf * m, int size);


/* ICMPv6 setsockopt filter structure and macros (RFC-2292) */

struct icmp6_filter {
  uint32_t  icmp6_filt[8];  /* 8*32 = 256 bits */
};

#define ICMP6_FILTER_WILLPASS(type, filterp) \
    ((((filterp)->icmp6_filt[(type) >> 5]) & (1 << ((type) & 31))) != 0)
#define ICMP6_FILTER_WILLBLOCK(type, filterp) \
    ((((filterp)->icmp6_filt[(type) >> 5]) & (1 << ((type) & 31))) == 0)
#define ICMP6_FILTER_SETPASS(type, filterp) \
    ((((filterp)->icmp6_filt[(type) >> 5]) |=  (1 << ((type) & 31))))
#define ICMP6_FILTER_SETBLOCK(type, filterp) \
    ((((filterp)->icmp6_filt[(type) >> 5]) &= ~(1 << ((type) & 31))))
#define ICMP6_FILTER_SETPASSALL(filterp) \
    memset((filterp), 0xFF, sizeof(struct icmp6_filter))
#define ICMP6_FILTER_SETBLOCKALL(filterp) \
    memset((filterp), 0, sizeof(struct icmp6_filter))

/* check if this addr can go out some IF */
int ip6_addrcango(struct sockaddr_in6 * dest);


#endif   /* _SOCKET6_H_*/



