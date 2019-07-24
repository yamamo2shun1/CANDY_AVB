/*
 * FILENAME: ifmap.c
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
 * PORTABLE: YES
 *
 */


#include "ipport.h"

#include "q.h"
#include "netbuf.h"
#include "net.h"
#include "ip.h"
#include "ifmap.h"

/* the actual pairs database: */
struct iface_pair ifpairs[MAX_IFPAIRS];


/* FUNCTION: ifmap_bind()
 * 
 * bind a pair of ifaces together.
 *
 * PARAM1: NET upper
 * PARAM2: NET lower
 *
 * RETURNS: 0 if OK or ENP_RESOURCE if it can't get a free entry.
 */

int
ifmap_bind(NET upper, NET lower)
{
   int i;

   for(i = 0; i < MAX_IFPAIRS; i++)
   {
      if((ifpairs[i].lower == NULL) &&
         (ifpairs[i].upper == NULL))
      {
         ifpairs[i].lower = lower;
         ifpairs[i].upper = upper;
         return 0;
      }
   }
   return ENP_RESOURCE;      /* no free array entries */
}

/* FUNCTION: ifmap_nextupper()
 * 
 * Get the upper iface of a bound pair for the passed lower iface.
 * This returns the first match if "lower" is NULL, else the next
 * match after the passed lower.
 *
 * PARAM1: NET upper
 * PARAM2: NET lower
 *
 * RETURNS: 
 */

NET
ifmap_nextupper(NET upper, NET lower)
{
   int i;
   int matched = FALSE;

   for(i = 0; i < MAX_IFPAIRS; i++)
   {
      if(ifpairs[i].lower == lower)
      {
         if((upper == NULL) ||
           (matched))
         {
            return ifpairs[i].upper;
         }
         else if(ifpairs[i].upper == upper)
            matched = TRUE;
      }
   }
   return NULL;     /* not found, or no "next" */
}

/* FUNCTION: ifmap_nextlower()
 * 
 * Get the lower iface of a bound pair for the passed upper iface.
 *
 * PARAM1: NET upper
 * PARAM2: NET lower
 *
 * RETURNS: pointer tp lower iface, NULL if error
 */

NET
ifmap_nextlower(NET upper, NET lower)
{
   int i;
   int matched = FALSE;

   for(i = 0; i < MAX_IFPAIRS; i++)
   {
      if(ifpairs[i].upper == upper)
      {
         if((lower == NULL) ||
            (matched))
         {
            return ifpairs[i].lower;
         }
         else if(ifpairs[i].lower == lower)
            matched = TRUE;
      }
   }
   return NULL;     /* not found */
}

/* FUNCTION: ifmap_del()
 * 
 * delete all instances  of interface pairs which match the passed
 * NET parameters. NULL NET is treated as a wildcard.
 *
 * PARAM1: NET upper
 * PARAM2: NET lower
 *
 * RETURNS: 
 */
 
void
ifmap_del(NET upper, NET lower)
{
   int   i;

   /* loop through pairs */
   for(i = 0; i < MAX_IFPAIRS; i++)
   {
      if(ifpairs[i].lower == lower)
      {
         if((ifpairs[i].upper == upper) ||
            (upper == NULL))
         {
            /* lower matches, and upper is match or wildcard. */
            ifpairs[i].upper = ifpairs[i].lower = NULL;
         }
      }
      if(ifpairs[i].upper == upper)
      {
         if((ifpairs[i].lower == lower) ||
            (lower == NULL))
         {
            /* upper matches, and lower is match or wildcard. */
            ifpairs[i].upper = ifpairs[i].lower = NULL;
         }
      }
   }
}



