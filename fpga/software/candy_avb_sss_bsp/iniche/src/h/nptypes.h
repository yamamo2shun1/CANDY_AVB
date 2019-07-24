/*
 * FILENAME: nptypes.h
 *
 * Copyright  2000 By InterNiche Technologies Inc. All rights reserved
 *
 *
 * MODULE: INET
 *
 *
 * PORTABLE: yes
 */

/* Additional Copyrights: */
/* Portions Copyright 1990-1997 by NetPort Software. */
/* Portions Copyright 1986 by Carnegie Mellon */
/* Portions Copyright 1983 by the Massachusetts Institute of Technology  */


#ifndef NPTYPES_H
#define  NPTYPES_H   1


typedef  unsigned short unshort;
typedef  unsigned long ulong;


/* now do it the way the CMU snmp code likes it: */
typedef   unsigned char u_char;
typedef   unsigned short u_short;
typedef   unsigned long u_long;

typedef  unsigned long ip_addr;


#ifdef NOT_USED

/* general stuff C code */
#ifndef  TRUE
#define  TRUE  -1
#endif

#ifndef  FALSE
#define  FALSE    0
#endif

#ifndef  NULL
#define  NULL  ((void*)0)
#endif

#endif   /* NOT_USED */


/* usefull macros: */

/* Altera Niche Stack Nios port modification:
 * Keep these macros defined.
 */
#ifndef min
#define  min(x,y)          ((x) <= (y) ? (x) : (y))
#endif
#ifndef max
#define  max(x,y)          ((x) >= (y) ? (x) : (y))
#endif

/*
 * Altera Niche Stack Nios port modification;
 * Provide routines to extract individual octets of 
 * an IP address. This is used by Altera example software
 */
#ifdef ALT_INICHE
#define ip4_addr1(ipaddr) ((unsigned int)(ntohl(ipaddr) >> 24) & 0xff)
#define ip4_addr2(ipaddr) ((unsigned int)(ntohl(ipaddr) >> 16) & 0xff)
#define ip4_addr3(ipaddr) ((unsigned int)(ntohl(ipaddr) >> 8) & 0xff)
#define ip4_addr4(ipaddr) ((unsigned int)(ntohl(ipaddr)) & 0xff)
#endif /* ALT_INICHE */

#endif   /* NPTYPES_H */



