/*
 * FILENAME: pmtu.c
 *
 * Copyright 2001 InterNiche Technologies Inc.  All rights reserved.
 *
 * Path MTU cache supporting functions
 *
 * MODULE: INET
 *
 * ROUTINES: pmtucache_init(), pmtucache_exit(), pmtu_firstguess(),
 * ROUTINES: pmtu_nextguess(), pmtucache_set(), pmtucache_get(),
 * ROUTINES: pmtu_stats()
 *
 * PORTABLE: yes
 *
 */

#include "ipport.h"

#ifdef  IP_PMTU

#include "netbuf.h"
#include "net.h"
#include "ip.h"
#include "pmtu.h"

/*
 * IP_PMTU_MAXCACHE: maximum number of Path MTU cache entries
 * 
 * This macro is used to set the initial/default limit on the number
 * of Path MTU cache entries retained by the stack.  It is intended to
 * be overrideable from ipport.h.
 */
#ifndef IP_PMTU_MAXCACHE
#define IP_PMTU_MAXCACHE      4
#endif

/*
 * pmtu_maxcache: maximum number of Path MTU cache entries
 *
 * This global contains the limit on the number of Path MTU cache 
 * entries retained by the stack.  It is intended to be set from 
 * the IP_PMTU_MAXCACHE macro (so can be set by ipport.h) or
 * overridden by the target's startup code *before startup*
 * (i.e. before pmtucache_init() is called by ip_init()).
 */
int pmtu_maxcache = IP_PMTU_MAXCACHE;

/*
 * pmtucache: the Path MTU cache
 */
struct pmtucacheent * pmtucache = NULL;

/*
 * pmtucache_mru: ptr to the most-recently-used cached entry
 */
struct pmtucacheent * pmtucache_mru = NULL;

/*
 * IP_PMTU_TIMEOUT: time (in seconds) that a cached entry can remain
 * without an update before it is invalidated
 * 
 * This macro is used to set the the time limit after which a cached
 * Path MTU value will be re-initialized to force re-discovery.  A
 * value of 0 turns off this function (i.e. cached Path MTU values
 * will be able to hang around forever).  It is intended to be
 * overrideable from ipport.h.
 */
#ifndef IP_PMTU_TIMEOUT
#define IP_PMTU_TIMEOUT       3600
#endif

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
u_long pmtu_timeout = IP_PMTU_TIMEOUT;

/*
 * IP_PMTU_LLIMIT: lower limit for Path MTU guesses
 * 
 * This macro is used to set a lower limit below which pmtu_nextguess()
 * will not make lower guesses for the path MTU estimate.  It is 
 * intended to be overrideable from ipport.h.
 */
#ifndef IP_PMTU_LLIMIT
#define IP_PMTU_LLIMIT        256
#endif

/*
 * pmtu_llimit: lower limit for Path MTU guesses
 *
 * This global contains the lower limit below which pmtu_nextguess()
 * will not make lower guesses for the path MTU estimate.  It is
 * intended to be set from the IP_PMTU_LLIMIT macro (so can be set by
 * ipport.h) or overridden by the target's startup code, preferably
 * before startup (i.e. before pmtucache_init() is called by
 * ip_init()).  
 */
unsigned pmtu_llimit = IP_PMTU_LLIMIT;

/*
 * PMTU_EXPIRED() - macro to determine if a timestamp has expired
 *
 * This is implemented as a macro to inline it yet keep the 
 * implementation consistent across uses.
 */
#ifndef PMTU_EXPIRED
#define PMTU_EXPIRED(timestamp) (cticks > (timestamp) + pmtu_timeout * TPS)
#endif  /* PMTU_EXPIRED */

/*
 * FUNCTION: pmtucache_init()
 *
 * Initializes the path MTU cache.  Memory is allocated for the path
 * MTU cache and all cache entries are marked as unused.  pmtucache_exit()
 * is installed as an exit hook function to free the cache when the stack
 * terminates.
 *
 * RETURNS: SUCCESS (0) if the path MTU cache was successfully
 *          initialized; ENP_NOMEM if the memory could not be 
 *          allocated.
 */
int
pmtucache_init(void)
{
   int i;

   pmtucache = (struct pmtucacheent *) 
      PMTU_ALLOC(sizeof(struct pmtucacheent) * pmtu_maxcache);

   if (pmtucache == NULL)
      return ENP_NOMEM;

   for (i = 0; i < pmtu_maxcache; i++)
      MEMSET(&pmtucache[i], 0, sizeof(struct pmtucacheent));

   pmtucache_mru = NULL;

   exit_hook(&pmtucache_exit);

   return 0;
}

/*
 * FUNCTION: pmtucache_exit()
 *
 * Frees resources allocated for the path MTU cache in preparation
 * for the termination of the stack. 
 */
void
pmtucache_exit(void)
{
   /* if we allocated memory for the cache, free it */
   if (pmtucache != NULL)
   {
      PMTU_FREE(pmtucache);
      pmtucache = NULL;
   }

   pmtucache_mru = NULL;
}

/*
 * FUNCTION: pmtu_firstguess()
 *
 * Returns a "first guess" path MTU for a given peer host IP address.
 *
 * PARAM1: ip_addr dest; the peer host IP address in network order.
 * 
 * RETURNS: an initial guess at the path MTU for the given peer 
 *          host IP address.
 */
static
unsigned
pmtu_firstguess(ip_addr dest)
{
   ip_addr firsthop;
   NET out_net;

   /* On what net will IP try to send to this host?  
    * If we don't know, our first guess for the path MTU is
    * the largest IP datagram we can fit in a big packet buffer.
    * If we do know, then our first guess for the path MTU is 
    * the largest IP datagram we can send on that network.
    */
   out_net = iproute(dest, &firsthop);
   if (out_net == NULL)
      return (bigbufsiz - MaxLnh);
   else
      return (out_net->n_mtu - out_net->n_lnh);
}

/*
 * FUNCTION: pmtu_nextguess()
 *
 * Returns a "next guess" path MTU for a given previous estimate.
 *
 * PARAM1: unsigned estimate; a previous path MTU estimate which is
 *         now supposed to be too large.
 * 
 * RETURNS: a next guess at the path MTU.
 */
static
unsigned
pmtu_nextguess(unsigned estimate)
{
   /* If the previous estimate is equal to or below the lower 
    * limit, keep it.
    */
   if (estimate <= pmtu_llimit)
      return estimate;

   /* Else, make a new estimate that is half of the previous 
    * estimate, or the lower limit, whichever is higher.
    */
   return max(estimate >> 1, pmtu_llimit);
}

/*
 * FUNCTION: pmtucache_set()
 *
 * Sets the path MTU cache entry for the given peer host IP address
 * to contain a new estimate.
 *
 * PARAM1: ip_addr dest; the IP address of the peer (destination) 
 *         host, in network order.
 * PARAM2: unsigned estimate; the new path MTU estimate for the
 *         peer.  May be 0, in which case pmtucache_set() will guess
 *         a path MTU estimate.
 */
void
pmtucache_set(ip_addr dest,
              unsigned estimate)
{
   int i;
   struct pmtucacheent * lru = NULL;

   /* search through the cache for matching dest, and keep
    * track of which entry is least-recently-used in lru 
    */
   for (i = 0; i < pmtu_maxcache; i++)
   {
      if (pmtucache[i].dest == dest)
      {
         /* found it! make it the most-recently-used entry */
         pmtucache_mru = &pmtucache[i];
set:
         /* estimate may have come from an ICMP destination-unreachable
          * (datagram too big) message, and may therefore be 0, so we
          * may have to guess.
          */
          if (estimate == 0)
             pmtucache_mru->pmtu = pmtu_nextguess(pmtucache_mru->pmtu);
          else
             pmtucache_mru->pmtu = estimate;
         pmtucache_mru->lastused = pmtucache_mru->lastchanged = cticks;
         return;
      }
      if ((lru == NULL) ||
          (pmtucache[i].lastused < lru->lastused) ||
          (pmtucache[i].dest == 0))
         lru = &pmtucache[i];
   }

   /* if we didn't find an entry, recycle the least-recently-used
    * entry to make one
    */ 
   if ((i >= pmtu_maxcache) && (lru != NULL))
   {
      pmtucache_mru = lru;
      pmtucache_mru->dest = dest;
      if (estimate)
         pmtucache_mru->pmtu = estimate;
      else
         pmtucache_mru->pmtu = pmtu_firstguess(dest);
      goto set;
   }
}

/*
 * FUNCTION: pmtucache_get()
 *
 * Gets the current path MTU estimate for a peer host IP address 
 * from the path MTU cache.
 *
 * PARAM1: ip_addr dest; the IP address of the peer (destination) 
 *         host, in network order.
 *
 * RETURNS: the current path MTU estimate for the given host.
 */
unsigned
pmtucache_get(ip_addr dest)
{
   int i;
   struct pmtucacheent * lru = NULL;

   /* do a quick check of the most-recently-used entry */
   if ((pmtucache_mru != NULL) &&
       (pmtucache_mru->dest == dest))
   {
get:
      /* if the cached estimate has timed out, restart discovery
       * with a first-guess path MTU estimate */
      if (PMTU_EXPIRED(pmtucache_mru->lastchanged))
      {
         pmtucache_mru->lastused = pmtucache_mru->lastchanged = cticks;
         pmtucache_mru->pmtu = pmtu_firstguess(dest);
      }
      /* update the last-used time and return the cached estimate */
      pmtucache_mru->lastused = cticks;
      return pmtucache_mru->pmtu;
   }

   /* search through the cache for matching dest, and keep
    * track of which entry is least-recently-used in lru 
    */
   for (i = 0; i < pmtu_maxcache; i++)
   {
      if (pmtucache[i].dest == dest)
      {
         /* found it! make it the most-recently-used entry and go return it */
         pmtucache_mru = &pmtucache[i];
         goto get;
      }
      if ((lru == NULL) ||
          (pmtucache[i].lastused < lru->lastused) ||
          (pmtucache[i].dest == 0))
         lru = &pmtucache[i];
   }

   /* if we didn't find an entry, recycle the least-recently-used
    * entry to make one, and start it with a first-guess estimate
    */ 
   if ((i >= pmtu_maxcache) && (lru != NULL))
   {
      pmtucache_mru = lru;
      pmtucache_mru->dest = dest;
      pmtucache_mru->pmtu = pmtu_firstguess(dest);
      pmtucache_mru->lastchanged = cticks;
      return pmtucache_mru->pmtu;
   }

   /* if we didn't find a least-recently-used entry, something's
    * probably not right with the cache, so return our first-guess
    * estimate and hope for the best
    */
   return pmtu_firstguess(dest);
}

#ifdef NET_STATS
/*
 * FUNCTION: pmtu_stats()
 *
 * Prints the path MTU cache and other statistical information.
 *
 * PARAM1: void * pio;
 *
 * RETURNS: SUCCESS (0).
 */
int
pmtu_stats(void * pio)
{
   struct pmtucacheent * mru = pmtucache_mru;
   int i;

   ns_printf(pio, "path MTU cache timeout (ticks) = %lu\n", pmtu_timeout);
   ns_printf(pio, "path MTU lower limit = %u\n", pmtu_llimit);

   if (pmtucache == NULL)
   {
      ns_printf(pio, "no path MTU cache established\n");
      return 0;
   }

   ns_printf(pio, "current time (ticks) = %lu\n", cticks);

   ns_printf(pio, "host,       mtu,  lastused, lastchanged\n");

   for (i = 0; i < pmtu_maxcache; i++)
   {
      if (pmtucache[i].dest != 0)
      {
         ns_printf(pio, "%u.%u.%u.%u   %d   %lu   %lu   %s%s\n",
                   PUSH_IPADDR(pmtucache[i].dest),
                   pmtucache[i].pmtu,
                   pmtucache[i].lastused,
                   pmtucache[i].lastchanged,
                   PMTU_EXPIRED(pmtucache[i].lastchanged) ? "EXP " : "",
                   (&pmtucache[i] == mru) ? "MRU" : "");
      }
   }

   return 0;
}

#endif  /* NET_STATS */

#endif  /* IP_PMTU */
