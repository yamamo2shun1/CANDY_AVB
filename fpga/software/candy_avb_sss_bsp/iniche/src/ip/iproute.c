/*
 * FILENAME: iproute.c
 *
 * Copyright 2001 By InterNiche Technologies Inc. All rights reserved
 *
 * 	This contains IP routing code. There are two complete sets of 
 * functions: one for BTREE_ROUTES and the other for nonhashed.
 * 
 *
 * MODULE: INET
 *
 * ROUTINES: rt_lookup(), add_route(), del_route(), 
 *
 * PORTABLE: yes
 */

#include "ipport.h"
#include "q.h"
#include "netbuf.h"
#include "net.h"
#include "ip.h"


#ifdef IP_ROUTING    /* whole file goes away without this */

#ifdef RIP_SUPPORT
#include "../rip/rip.h"
#endif

RTMIB cachedRoute = NULL;        /* last route entry we used */

u_char   rtp_priority[] =  {  /* route table entry priorities, by protocol */
   /* FOO */            0x00, /* empty entry, lowest priority */
   /* IPRP_OTHER   1 */ 0xF0,
   /* IPRP_LOCAL   2 */ 0xFF, /* hand entered = higest priority */
   /* IPRP_NETMGMT 3 */ 0xF0,
   /* IPRP_ICMP    4 */ 0xCA,
   /* IPRP_EGP     5 */ 0xA0,
   /* IPRP_GGP     6 */ 0x80,
   /* IPRP_HELLO   7 */ 0x80,
   /* IPRP_RIP     8 */ 0xB0,
};



#ifndef BTREE_ROUTES

/* FUNCTION: rt_lookup()
 *
 * Look up passed host in the routing table. If a full address match
 * is found it is returned, else a net match is returned (if one is
 * found). The caller can distinguish by testing rtp->ipRouteDest.
 *
 * PARAM1: ip_addr   host
 *
 * RETURNS: Returns a pointer to the table entry; NULL if no suitable
 * entry was found.
 */

RTMIB
rt_lookup(ip_addr host)
{
   RTMIB    rtp;
   RTMIB    netmatch;
   unsigned char max_bits_matched = 0;
   unsigned char curr_bits_matched;
   ip_addr  entry_mask;
   unsigned long int bitcount_mask;
   unsigned long int extracted_bit;

   netmatch = NULL;

   /* see if it's in the route table. */
   for (rtp = rt_mib; rtp < rt_mib + ipRoutes; rtp++)
   {
      if (rtp->ipRouteNextHop == 0L)   /* skip over empty entries */
         continue;
#ifdef RIP_SUPPORT
      /* skip RIP private entries */
      if (rtp->ipRouteFlags & RIP_PRIVATE)
         continue;
#endif
      entry_mask = rtp->ipRouteMask;
      /* check to see if we have a match in the route table */
      if ((rtp->ipRouteDest & entry_mask) == (host & entry_mask))
      {
         /* check to see if current match is better than the previous best
          * by computing the number of bits that matched */
         for (curr_bits_matched = 0, bitcount_mask = 0x80000000; bitcount_mask > 0; bitcount_mask >>= 1)
         {
            extracted_bit = (ntohl(entry_mask)) & bitcount_mask;
            if (extracted_bit) ++curr_bits_matched;
         }
         if (curr_bits_matched > max_bits_matched)
         {
            /* save a pointer to the best match */
            max_bits_matched = curr_bits_matched;
            netmatch = rtp;
            /* if all 32 bits match, stop searching the route table */
            if (max_bits_matched == 32)
               break;
         }
      }
   }

   if (netmatch)
      netmatch->ipRouteAge = cticks;   /* timestamp entry we used */

   return netmatch;
}


/* FUNCTION: add_route()

 * add_route(dest, mask, nexthop, metric); - Make an entry in the 
 * route table directing dest to nexthop. If route already exists, 
 * update it.
 *
 * PARAM1: ip_addr   dest
 * PARAM2: ip_addr   mask
 * PARAM3: ip_addr   nexthop
 * PARAM4: int    ifc
 * PARAM5: int    prot
 *
 * RETURNS: Returns a pointer to the table entry; so caller can 
 * process it further, (ie add metrics), Returns NULL on error.
 *
 */

RTMIB
add_route(
   ip_addr  dest,       /* ultimate destination */
   ip_addr  mask,       /* net mask, 0xFFFFFFFF if dest is host address */
   ip_addr  nexthop,    /* where to forward to */
   int      iface,      /* interface (net) for nexthop */
   int      prot)       /* how we know it: icmp, table, etc */
{
   RTMIB rtp;           /* scratch route table entrry pointer */
   RTMIB newrt;         /* best entry for new route */
   struct net * ifp;    /* interface (net) for nexthop */

   newrt = NULL;     /* may be replaced with empty or more expendable entry */

					 
					 
   /* set the route interface pointer according to the index passed. This allows 
    * the passed index to be used to access dynamic interfaces, which do not appear 
    * in the nets[] array.
    */
   ifp = if_getbynum(iface);
   if(!ifp)
      return NULL;

   if (rt_mib == NULL)
      return NULL;

   /* Don't add null masks or IP addresses - they give false positives on
    * net matches and don't belong here anyway.
    */
   if((dest == 0) || (mask == 0))
   {
      dtrap();    /* bad configuration? */
      dprintf("add_route: rejected null parm; dest: %lx, mask: %lx\n",
	      htonl(dest), htonl(mask) );
      return NULL;
   }



   /* if it's already in the route table, just update it. */
   for (rtp = rt_mib; rtp < rt_mib + ipRoutes; rtp++)
   {
      if (rtp->ipRouteDest == dest) /* found existing entry for target */
      {
         rtp->ipRouteNextHop = nexthop;      /* fix entry */
         rtp->ipRouteAge = cticks;           /* timestamp it */
         /* set the rfc1213 1-based SNMP-ish interface index */
         rtp->ipRouteIfIndex = (long)(iface) + 1;
         rtp->ipRouteProto = prot;           /* icmp, or whatever */
         rtp->ipRouteMask = mask;
         rtp->ifp = ifp;
         return(rtp);   /* just update and exit */
      }
      /* if we didn't find empty slot yet, look for good slot to recycle */
      if (!newrt || (newrt->ipRouteProto != 0))
      {
         if (!rtp->ipRouteNextHop)  /* found empty slot for use */
         {
            newrt = rtp;   /* record empty route for use */
            newrt->ipRouteProto = 0;
            continue;
         }
         /* else see if the new route has higher priority than this slot: */
         if (rtp_priority[prot] >= rtp_priority[rtp->ipRouteProto])
         {
            if (!newrt)
            {
               newrt = rtp;
               continue;
            }

            /* see if rtp is less important then newrtp */
            if (rtp_priority[rtp->ipRouteProto] < 
                rtp_priority[newrt->ipRouteProto])
            {
               newrt = rtp;   /* save lower priority entry for recycle */
            }
            else if(rtp_priority[rtp->ipRouteProto] == 
                rtp_priority[newrt->ipRouteProto])
            {
               /* if equal priority, keep the older entry for deletion */
               if (c_older(rtp->ipRouteAge, newrt->ipRouteAge) == rtp->ipRouteAge)
                  newrt = rtp;   /* got an older one */
            }
         }
      }
   }

   /* fall to here if not in table: create a new route */
   if (newrt)  /* did we find an empty or lower priority route entry? */
      rtp = newrt;   /* create new entry in lowest priority slot */
   else  /* all slots have higher priority, new entry looses */
      return NULL;

   /* set default value in new route entry, caller can modiy further. */
   rtp->ifp = ifp;
   rtp->ipRouteDest = dest;
   rtp->ipRouteIfIndex = (long)(if_netnumber(ifp)) + 1; /* set interface number */
   rtp->ipRouteMetric1 = ip_mib.ipDefaultTTL;
   rtp->ipRouteMetric2 = -1;
   rtp->ipRouteMetric3 = -1;
   rtp->ipRouteMetric4 = -1;
   rtp->ipRouteNextHop = nexthop;
   rtp->ipRouteProto = prot;                 /* icmp, or whatever */

   if ((dest & mask) == (nexthop & mask))
      rtp->ipRouteType = IPRT_DIRECT;
   else
      rtp->ipRouteType = IPRT_INDIRECT;

   rtp->ipRouteAge = cticks;        /* timestamp it */
   rtp->ipRouteMask = mask;
   rtp->ipRouteMetric5 = -1;
   return(rtp);
}

/* FUNCTION: del_route()
 *
 * del_route() - delete entries from the routing table. Mask of 
 * 0xFFFFFFFF will cause only exact dest address matches to be 
 * deleted. Mask of 0L will delete all routes for passed interface. 
 * iface is net index, else -1 is wildcard interface. 
 *
 * 
 * PARAM1: ip_addr dest
 * PARAM2: ip_addr mask
 * PARAM3: int iface
 *
 * RETURNS: Returns number of route table entries deleted. 
 */

int
del_route(ip_addr dest, ip_addr mask, int iface)
{
   RTMIB rtp;
   int   retval   =  0;
   struct net * ifp;

   /* set the route interface pointer according to the index passed. This allows 
    * the passed index to be used to access dynamic interfaces, which do not appear 
    * in the nets[] array.
    */
   if(iface == -1)
      ifp = NULL;    /* wildcard */
   else
      ifp = if_getbynum(iface);

   if (rt_mib == NULL)     /* Make sure we're up */
      return 0;

   for (rtp = rt_mib; rtp < rt_mib + ipRoutes; rtp++)
   {
      if (!rtp->ipRouteNextHop)  /* empty slot */
         continue;
      if(ifp != NULL && ifp != rtp->ifp)
         continue;   /* interface didn't match */
      if ((rtp->ipRouteDest & mask) == (dest & mask))
      {
         MEMSET(rtp, 0, sizeof(*rtp)); /* clear entry */
         retval++;
      }
      if (cachedRoute == rtp) /* clear cache if it's being deleted */
         cachedRoute = NULL;
   }
   return retval;
}


#else /**** end of non BTREE_ROUTES versions ****/

int   rtbtree_clean = 20;    /* number of seconds between cleanups */
int   rtbtree_last = 0;      /* seconds since last cleanup */
RTMIB btreeRoot = (RTMIB)0;  /* BTree parent pointer. */

/* array of all masks which are used in the routing table. This exisist to speed 
 * searchs - we only have to search for masks we know are in the tree somewhere.
 * first entry is always 0xFFFFFFFF,
 */

static ip_addr rt_masks[32] = { 0xFFFFFFFF } ;


/* FUNCTION: rtp_fillin()
 *
 * Fill in the passed RTP structure from the other parameters.
 *
 * PARAM1: RTMIB    rtp,      route entry to fill in 
 * PARAM2: ip_addr  dest,     ultimate destination 
 * PARAM3: ip_addr  mask,     net mask, 0xFFFFFFFF if dest is host address
 * PARAM4: ip_addr  nexthop,  where to forward to
 * PARAM5: struct net * ifp   interface (net) for nexthop
 * PARAM : int      prot      how we know it: icmp, table, etc
 *
 * RETURNS: NA
 */

static void
rtp_fillin(RTMIB rtp, ip_addr  dest, ip_addr  mask,
   ip_addr  nexthop, struct net * ifp, int prot)
{
   rtp->ipRouteDest = dest;            /* fill in entry */
   rtp->ipRouteNextHop = nexthop;
   rtp->ipRouteAge = cticks;           /* timestamp it */
   rtp->ipRouteProto = prot;           /* icmp, or whatever */
   rtp->ipRouteMask = mask;
   rtp->ifp = ifp;

   /* set the rfc1213 1-based SNMP-ish interface index */
   rtp->ipRouteIfIndex = (long)(if_netnumber(ifp)) + 1;

   if ((dest & mask) == (nexthop & mask))
      rtp->ipRouteType = IPRT_DIRECT;
   else
      rtp->ipRouteType = IPRT_INDIRECT;

   rtp->ipRouteMetric1 = ip_mib.ipDefaultTTL;
   rtp->ipRouteMetric2 = -1;
   rtp->ipRouteMetric3 = -1;
   rtp->ipRouteMetric4 = -1;
   rtp->ipRouteMetric5 = -1;
}


/* FUNCTION: rttmo()
 *
 * Helper (callback) function. This is passed to avlremove() by rtbtree_tmo()
 * and is called back once for every route table entry.
 *
 * PARAM1: struct RtMib * n - The route table entry to examine.
 * PARAM2: long param       - Unused
 * PARAM3: int depth        - Unused
 *
 * RETURNS: NA
 */

static u_long rtb_delctick;

static void
rttmo(struct avl_node * node, long param, int depth)
{
   RTMIB rt = (RTMIB)node;

   /* only time out the routes with type "other". Entries set by RIP,
    * SNMP, users, etc., should have their own mechanisms for cleanup.
    */
   if(rt->ipRouteProto != IPRP_OTHER)
      return;

   if (!btreeRoot)
	   return;

   /* Check timestamp to see when this was last used */
   if((rt->ipRouteAge < rtb_delctick) ||
      (rt->ipRouteAge > cticks))     /* catch cticks wrapping */
   {
      ip_addr key;      /* for pass to avlremove() */

       /* If the cached route is the one being deleted then clear the cache */
      if (rt == cachedRoute)
         cachedRoute = NULL;
    
	  ((RTMIB)node)->ipRouteNextHop = 0; /* clear route */
      key = htonl(((RTMIB)node)->ipRouteDest);     /* pass key in machine endian */
	  avlremove((struct avl_node **)&btreeRoot, (void*)&key);
   }
   USE_ARG(depth);
   USE_ARG(param);
}

/* FUNCTION: rtbtree_tmo()
 *
 * Called once a second from misclib\timeouts.c to clear out any stale route
 * table entries. Doing this every second is usually overkill, so there is
 * a user settable throttle (rtbtree_clean) for the number of seconds
 * between cleanups.
 *
 * PARAM1: NA
 *
 * RETURNS: NA
 */

void
rtbtree_tmo()
{
   if(rtbtree_last++ < rtbtree_clean)
      return;

   if (!btreeRoot)
	   return;

   rtbtree_last = 0;
   rtb_delctick = cticks - (rtbtree_clean * TPS);

   avldepthfirst((struct avl_node *)btreeRoot, rttmo, 0, 0);
}

RTMIB rt_lookup_masked(ip_addr host, ip_addr mask);  /* predecl */

/* The next few functions are identical to the ones of the same names
 * defined in the non btree code above. Their descriptions are not
 * duplicated hereunder to avoid confusion.
 */

RTMIB
rt_lookup(ip_addr host)
{
   int      i;    /* mask array index */
   ip_addr  mask;
   RTMIB    rtp;

   for(i = 0; i < 32; i++)
   {
      if(rt_masks[i] == 0)    /* no more masks? */
         return NULL;
      mask = rt_masks[i];
      rtp = rt_lookup_masked(host & mask, mask);
      if(rtp)
         return rtp;
   }
   return NULL;
}

RTMIB
rt_lookup_masked(ip_addr host, ip_addr mask)
{
   RTMIB    rtp;
   ip_addr  hostval;       /* local endian value of host */
   ip_addr  tmp;

   /* convert search IP address to local endian so the "<" and ">"
    * tests work corrently 
    */
   hostval = htonl(host);     

   rtp = btreeRoot;       /* start at parent of tree */

   /* binary tree search loop. This should be FAST. */
   while(rtp)
   {
      tmp = htonl(rtp->ipRouteDest);
#ifdef RIP_SUPPORT 
      /* skip RIP private entries */
      if ( rtp->ipRouteFlags & RIP_PRIVATE )
         goto nextroute;
#endif

      /* if exact match of both mask and entry then we are done */
      if((mask == rtp->ipRouteMask) && (hostval == tmp))
      {
         rtp->ipRouteAge = cticks;   /* timestamp entry we used */
         return rtp;    /* return route entry or NULL */
      }


#ifdef RIP_SUPPORT
nextroute:
#endif
      /* search the correct branch of the binary tree */
      if(hostval < tmp)
         rtp = (RTMIB)rtp->left;
      else                    /* hostval > tmp */
         rtp = (RTMIB)rtp->right;
   }

   return NULL;      /* didn't find a match */
}

/* FUNCTION: rt_lookup_unmasked()
 *
 * Look up passed host in the routing table, using exact host match
 *
 * PARAM1: ip_addr   host
 *
 * RETURNS: Returns a pointer to the table entry; NULL if no 
 * entry was found.
 */

RTMIB
rt_lookup_unmasked(ip_addr host)
{
   RTMIB    rtp;
   ip_addr  hostval;       /* local endian value of host */
   ip_addr  tmp;

   /* convert search IP address to local endian so the "<" and ">"
    * tests work corrently 
    */
   hostval = htonl(host);     

   rtp = btreeRoot;       /* start at parent of tree */

   /* binary tree search loop. This should be FAST. */
   while(rtp)
   {
      tmp = htonl(rtp->ipRouteDest);

      /* if exact match of entry then we are done */
      if(hostval == tmp)
      {
         rtp->ipRouteAge = cticks;   /* timestamp entry we used */
         return rtp;    /* return route entry or NULL */
      }

      /* search the correct branch of the binary tree */
      if(hostval < tmp)
         rtp = (RTMIB)rtp->left;
      else                    /* hostval > tmp */
         rtp = (RTMIB)rtp->right;
   }

   return NULL;      /* didn't find a match */
}

RTMIB
add_route(
   ip_addr  dest,       /* ultimate destination */
   ip_addr  mask,       /* net mask, 0xFFFFFFFF if dest is host address */
   ip_addr  nexthop,    /* where to forward to */
   int      iface,      /* interface (net) for nexthop */
   int      prot)       /* how we know it: icmp, table, etc */
{
   RTMIB rtp;           /* scratch route table entry pointer */
   struct net * ifp;    /* interface (net) for nexthop */
   int      i;          /* masks array index */

   ifp = if_getbynum(iface);
   if(!ifp)
      return NULL;

   if (!rt_mib)
	   return NULL;

   /* Don't add null masks or IP addresses - they give false positives on
    * net matches and don't belong here anyway.
    */
   if((dest == 0) || (mask == 0))
   {
      dtrap();    /* bad configuration? */
      return NULL;
   }

   /* Check that host portion of dest and host portion of mask don't overlap */
   if((~mask & dest) != 0)
   {
      dtrap();    /* programming error or bad user input */
      return NULL;
   }

   /* add mask to array */
   for(i = 0; i < 32; i++)
   {
      if(rt_masks[i] == mask)    /* Mask already in table? */
         break;                  /* don't add it again */

      /* This array is ordered, highest value first. See if new mask goes here */
      if(rt_masks[i] < mask)     /* mask gets inserted here */
      {
         int j;      /* secondary index */
         /* move subsequent masks back one slot */
         for(j = 31; j > i; j--)
            rt_masks[j] = rt_masks[j - 1];
         rt_masks[i] = mask;     /* put new mask in it's slot */
         break;                  /* done inserting mask */
      }
   }   

   /* this is just to find a particular entry, 
    * so exact mask.
	*/
   rtp = rt_lookup_unmasked(dest); 
   if(rtp && (rtp->ipRouteDest == dest))  /* found existing entry */
   {
      /* just update and exit */
      rtp_fillin(rtp, dest, mask, nexthop, ifp, prot);
      return(rtp);
   }

   /* fall to here if no entry found. Make new route entry */
   /* find an empty entry */
   for (rtp = rt_mib; rtp < rt_mib + ipRoutes; rtp++)
      if (!rtp->ipRouteNextHop)  /* empty slot */
         break;

   if (rtp >= (rt_mib + ipRoutes))
	   return NULL;

   /* set default values in new route entry */
   rtp_fillin(rtp, dest, mask, nexthop, ifp, prot);

#ifdef RIP_TRIGGERED_UPDATES
   rtp->ipRouteFlags |= RIP_TRIGGER ; 
   rtp->ipRouteFlags |= RIP_VALID ;
#endif   /* RIP_TRIGGERED_UPDATES */

   /* add route to tree. */
  if(avlinsert((struct avl_node **)&btreeRoot, (struct avl_node *)rtp) == BT_ERROR)
  {
	dprintf("*** add_route - avlinsert, BT_ERROR\n");
	dtrap();
	return NULL;
  }

   return(rtp);
}

static int rtdeletions;    /* count routes deleted for wildcard */

static void
rtdel(struct avl_node * node, long param, int depth)
{
   struct RtMib * n = (struct RtMib *)node;
   ip_addr key;

   if (!btreeRoot)
	   return;

   if(n->ifp == (struct net *)param)
   {
      /* If the cached route is the one being deleted then clear the cache */
      if (n == cachedRoute)
         cachedRoute = NULL;
      /* delete the entry n. */
	  n->ipRouteNextHop = 0; /* clear route */
      key = htonl(n->ipRouteDest);     /* pass key in machine endian */
      avlremove((struct avl_node **)&btreeRoot, (void*)&key);
      rtdeletions++;
   }
   USE_ARG(depth);
}
  
int
del_route(ip_addr dest, ip_addr mask, int iface)
{
   struct net * ifp;
   ip_addr key;

   if(!rt_mib)    /* no route yet? */
      return 0;

   /* set the route interface pointer according to the index passed. This allows 
    * the passed index to be used to access dynamic interfaces, which do not appear 
    * in the nets[] array.
    */
   if(iface == -1)
      ifp = NULL;    /* wildcard */
   else
      ifp = if_getbynum(iface);

   /* If an iface was specified and a mask was given then just delete
    * the passed IP address entry and return.
    */
   if(mask == 0xFFFFFFFF)
   {
      struct avl_node * delnode;    /* node to delete */

       /* If the cached route is the one being deleted then clear the cache */
      if (cachedRoute->ipRouteDest == dest)
         cachedRoute = NULL;

      /* call btree route to remove the actual entry */
      delnode = avlv4_access((struct avl_node *)btreeRoot, (ntohl(dest)));
      if(!delnode)
      {
         dtrap();    /* node not in tree??? */
         return 0;   /* deleted 0 entries */
      }
	  ((RTMIB)delnode)->ipRouteNextHop = 0; /* clear route */
      key = htonl((((RTMIB)delnode)->ipRouteDest));
      avlremove((struct avl_node **)&btreeRoot, &key);
      return 1;      /* done, deleted one route entry */
   }

   /* fall to here if we are doing a wildcard delete. To do this we
    * traverse the whole tree, using a callback function to delete any
    * entry with the passed net. This is not really reentrant since
    * we use a static variable to count deletions, but the only ill
    * effect of reentering will be a bogus return count..
    */

   if(ifp == NULL)
   {
      dtrap();    /* illegal parameter combo */
      return ENP_PARAM;
   }

   rtdeletions = 0;     /* init number of routes deleted */
   avldepthfirst((struct avl_node *)btreeRoot, rtdel, (long)ifp, 0);
   return(rtdeletions);
}

#endif /* BTREE_ROUTES or not */

#endif   /* IP_ROUTING  whole file */


