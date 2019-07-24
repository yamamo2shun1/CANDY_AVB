/*
 * FILENAME: pmtu.h
 *
 * Copyright 2001 InterNiche Technologies Inc.  All rights reserved.
 *
 * Path MTU cache definitions
 *
 * MODULE: INET
 *
 * PORTABLE: yes
 *
 */

#ifndef PMTU_H_
#define PMTU_H_

#ifdef  IP_PMTU

/*
 * struct pmtucacheent - a Path MTU cache entry
 */
struct pmtucacheent
{
   ip_addr dest;              /* peer IP address, in network order */
   u_long lastused;           /* time (cticks) entry was last referenced */
   u_long lastchanged;        /* time (cticks) entry was last changed */
   unsigned pmtu;             /* current path MTU estimate */
};

/*
 * pmtu_maxcache: maximum number of Path MTU cache entries
 *
 * This global contains the limit on the number of Path MTU cache 
 * entries retained by the stack.  It is intended to be set from 
 * the IP_PMTU_MAXCACHE macro (so can be set by ipport.h) or
 * overridden by the target's startup code *before startup*
 * (i.e. before pmtucache_init() is called by ip_init()).
 */
extern int pmtu_maxcache;

/*
 * pmtu_timeout: time (in seconds) that a cached entry can remain
 * without an update before it is invalidated
 *
 * This global contains the time limit after which a cached Path MTU 
 * value will be re-initialized to force re-discovery.  A value of 
 * 0 turns off this function (i.e. cached Path MTU values will be 
 * able to hang around forever).  It is intended to be set from 
 * the IP_PMTU_TIMEOUT macro (so can be set by ipport.h) or
 * overridden by the target's startup code, preferably before startup
 * (i.e. before pmtucache_init() is called by ip_init()).
 */
extern u_long pmtu_timeout;

/*
 * pmtu_llimit: lower limit for Path MTU guesses
 *
 * This global contains the lower limit for path MTU estimates made by
 * pmtu_nextguess().  It is intended to be set from the IP_PMTU_LLIMIT
 * macro (so can be set by ipport.h) or overridden by the target's
 * startup code, preferably before startup (i.e. before
 * pmtucache_init() is called by ip_init()).  
 */
extern unsigned pmtu_llimit;

/*
 * function prototypes
 */
int pmtucache_init(void);
void pmtucache_exit(void);
void pmtucache_set(ip_addr dest, unsigned pmtu);
unsigned pmtucache_get(ip_addr dest);
#ifdef  NET_STATS
int pmtu_stats(void * pio);
#endif  /* NET_STATS */

#endif  /* IP_PMTU */

#endif  /* PMTU_H_ */


