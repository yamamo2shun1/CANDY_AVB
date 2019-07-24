/*
 * FILENAME: ifmap.h
 *
 * Copyright 2001 - 2002 By InterNiche Technologies Inc. All rights reserved
 *
 * "Database" of iface to iface mappings. These are designed as an
 * administrate and configuration tool - application code can
 * set up the mappings, and the lower-level driver code will use
 * it to build IP tunnels, PPPoE links, and other constructions
 * that involve more than one interface. A lower
 * iface can be paired with more than one upper (eg PPPoE) and
 * Visa-versa.
 *
 * ROUTINES: ifmap_bind(), ifmap_nextupper(), ifmap_nextlower(), ifmap_del(),
 *
 * MODULE: PPP
 *
 */

#ifndef _IFMAP__H_
#define _IFMAP__H_ 1

#ifndef MAX_IFPAIRS
#define MAX_IFPAIRS MAXNETS
#endif   /* MAX_IFPAIRS */


struct iface_pair
{
   NET upper;
   NET lower;
};

extern struct iface_pair ifpairs[MAX_IFPAIRS];


NET   ifmap_nextupper(NET upper, NET lower);
NET   ifmap_nextlower(NET upper, NET lower);
int   ifmap_bind(NET upper, NET lower);
void  ifmap_del(NET upper, NET lower);

#endif /* _IFMAP__H_ */


