/*
 * FILENAME: ether.h
 *
 * Copyright  2000 By InterNiche Technologies Inc. All rights reserved
 *
 * information common to all ethernet drivers
 *
 * MODULE: INET
 *
 *
 * PORTABLE: yes
 */

/* Additional Copyrights: */
/* Portions Copyright 1990, 1993 by NetPort Software. */
/* Portions Copyright 1986 by Carnegie Mellon */
/* Portions Copyright 1983, 1985 by the Massachusetts Institute of Technology */

#ifndef _ETHER_H_
#define  _ETHER_H_   1


/* initialization modes: 0 = rcv normal + broadcast packets */
#define     LOOPBACK    0x01  /* send packets in loopback mode */
#define     ALLPACK     0x02  /* receive all packets: promiscuous */
#define     MULTI       0x04  /* receive multicast packets */

/* ethernet packet header */

START_PACKED_STRUCT(ethhdr)
u_char   e_dst[6];
u_char   e_src[6];
unshort  e_type;
END_PACKED_STRUCT(ethhdr)

/* ETHHDR_SIZE - size of packet header structure for allocation 
 * purposes Note this is a default -- it should be overridden in 
 * ipport.h if the need arises. 
 */

#ifndef ETHHDR_SIZE
#define  ETHHDR_SIZE (sizeof(struct ethhdr))
#endif   /* ETHHDR_SIZE */

/* ETHHDR_BIAS - where to locate the struct ethhdr within the 
 * allocated space, as an offset from the start in bytes Note this is 
 * a default -- it should be overridden in ipport.h if the need 
 * arises. 
 */

#ifndef  ETHHDR_BIAS
#define  ETHHDR_BIAS 0
#endif   /* ETHHDR_BIAS */

/* ET_DSTOFF - offset of destination address within Ethernet header
 */
#define ET_DSTOFF       (0)

/* ET_SRCOFF - offset of source address within Ethernet header
 */
#define ET_SRCOFF       (6)

/* ET_TYPEOFF - offset of Ethernet type within Ethernet header
 */
#define ET_TYPEOFF      (12)

/* ET_TYPE_GET(e) - get Ethernet type from Ethernet header pointed to
 *                  by char * e
 * Note returned Ethernet type is in host order!
 */
#define ET_TYPE_GET(e)  \
        (((unsigned)(*((e) + ET_TYPEOFF)) << 8) + \
         (*((e) + ET_TYPEOFF + 1) & 0xff))

/* ET_TYPE_SET(e, type) - set Ethernet type in Ethernet header pointed to
 *                        by char * e to value (type)
 * Note Ethernet type is value is expected to be in host order!
 */
#define ET_TYPE_SET(e, type) \
        *((e) + ET_TYPEOFF) = (unsigned char)(((type) >> 8) & 0xff); \
        *((e) + ET_TYPEOFF + 1) = (unsigned char)((type) & 0xff);

/* minimum & maximun length legal ethernet packet sizes */
#define     ET_MINLEN   60
#define     ET_MAXLEN   1514
#define     MINTU       60
#define     MTU         1514

extern   unsigned char  ETBROADCAST[];

#endif   /* _ETHER_H_ */



