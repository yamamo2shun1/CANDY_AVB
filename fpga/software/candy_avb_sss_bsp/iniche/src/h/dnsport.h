/*
 * FILENAME: dnsport.h
 *
 * Copyright  2000 By InterNiche Technologies Inc. All rights reserved
 *
 *
 * MODULE: DNS
 *
 *
 * PORTABLE: no
 */

/* dnsport.h
 *
 * Per-port DNS definition. Most of these are pretty much the same on all
 * systems, but may require tuning for weird cases 
 */

#ifndef _DNSPORT_H_
#define _DNSPORT_H_     1

#define  UDPPORT_DNS 53 /* UDP port to use */

typedef short int16_t;
typedef unsigned short u_int16_t;
typedef unsigned long u_int32_t;


#ifndef NO_TIME_T
typedef unsigned long time_t;

/* a unix-y timeval structure */
struct timeval {
     unsigned long  tv_sec;
     unsigned long  tv_usec;
};
#endif

#endif  /* _DNSPORT_H_ */

