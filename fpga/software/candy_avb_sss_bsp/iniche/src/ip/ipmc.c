/*
 * FILENAME: ipmc.c
 *
 * Copyright 1997- 2002 By InterNiche Technologies Inc. All rights reserved
 *
 * This file contains the code to handle IP multicast addreses. Works
 * for both IPv4 and IPv6.
 *
 * MODULE: IP
 *
 * ROUTINES: in_addmulti(), in_delmulti(), 
 * ROUTINES: mcaddr_net(), lookup_mcast(), 
 *
 * PORTABLE: yes
 */


#include "ipport.h"

#ifdef IP_MULTICAST

#include "netbuf.h"
#include "net.h"
#include "ip.h"

#include "../ipmc/igmp.h"

#ifdef IP_V6
#include "ip6.h"
#endif   /* IP_V6 */


/* FUNCTION: in_addmulti()
 *
 * Add an address to the list of IP multicast addresses for a given interface.
 *
 * PARAM1: ip_addr * ap
 * PARAM2: struct net * netp
 * PARAM3: addrtype 6 if ip_addr is IPv6, else addr is v4
 *
 * RETURNS: 
 */

struct in_multi * 
in_addmulti(ip_addr *ap, struct net *netp, int addrtype)
{
   struct in_multi *inm = (struct in_multi *)NULL;
   int error;

   /* check for good addr. */
   if ((ap == (ip_addr *)NULL) || (*ap == 0))
      return ((struct in_multi *)NULL);  

   ENTER_CRIT_SECTION(netp);

   /* See if address already in list. */
#ifdef IP_V6
   if(addrtype == 6)
      inm = v6_lookup_mcast((ip6_addr*)ap, netp);
#endif
#ifdef IP_V4
   if(addrtype != 6)
      inm = lookup_mcast(*ap, netp);
#endif

   if (inm != (struct in_multi *)NULL) 
   {
      /* Found it; just increment the reference count. */
      ++inm->inm_refcount;
   }
   else
   {
      /*
       * New address; allocate a new multicast record
       * and link it into the interface's multicast list.
       */
      inm = (struct in_multi *)INM_ALLOC(sizeof(*inm));

      if (inm == (struct in_multi *)NULL) 
      {
         EXIT_CRIT_SECTION(netp);
         return ((struct in_multi *)NULL);
      }
#ifdef IP_V6
      if(addrtype == 6)
         IP6CPY(&inm->ip6addr, (struct in6_addr *)ap);
#endif
#ifdef IP_V4
      if(addrtype != 6)
         inm->inm_addr = *ap;
#endif
      inm->inm_netp = netp;
      inm->inm_refcount = 1;
      inm->inm_next = netp->mc_list;
      netp->mc_list = inm;

      /*
       * If net has a multicast address registration routine then ask
       * the network driver to update its multicast reception
       * filter appropriately for the new address.
       */
      if(netp->n_mcastlist)
         error = netp->n_mcastlist(inm);
      else
         error = 0;
#if defined (IGMP_V1) || defined (IGMP_V2)
      /*
       * Let IGMP know that we have joined a new IP multicast group.
       */
      if (inm->inm_addr) igmp_joingroup(inm);
#endif      
   }

   EXIT_CRIT_SECTION(netp);
   USE_ARG(error);

   return (inm);
}



/* FUNCTION: in_delmulti()
 *
 * Delete a multicast address record.
 *
 * PARAM1: struct in_multi *inm
 *
 * RETURNS: 
 */

void
in_delmulti(struct in_multi * inm)
{
   struct in_multi * p;
   NET         netp = inm->inm_netp;
   int error;

   ENTER_CRIT_SECTION(inm);
   if (--inm->inm_refcount == 0) 
   {
      /* Unlink from list.  */
      for (p = netp->mc_list; p; p = p->inm_next)
      {
         if(p == inm)   /* inm is first in mc_list */
         {
            netp->mc_list = p->inm_next;  /* unlink */
            break;
         }
         else if(p->inm_next == inm)   /* inm is next */
         {
            p->inm_next = inm->inm_next;  /* unlink */
            break;
         }
      }

      /*
       * If net has a multicast address registration routine then ask
       * the network driver to update its multicast reception
       * filter appropriately for the deleted address.
       */
      if(netp->n_mcastlist)
         error = netp->n_mcastlist(inm);
      else
         error = 0;
#if defined (IGMP_V2)
      /*
       * No remaining claims to this record; let IGMP know that
       * we are leaving the multicast group.
       */
      if (inm->inm_addr) igmp_leavegroup(inm);
#endif      

      IM_FREE(inm);
   }

   EXIT_CRIT_SECTION(inm);
   USE_ARG(error);
}



#ifdef IP_V4

/* FUNCTION: lookup_mcast()
 *
 * Look for the passed IPv4 address in the iface poassed.
 *
 * PARAM1: ip_addr addr
 * PARAM2: NET netp
 *
 * RETURNS: pointer to mcast addr structure, or NULL if not found.
 */

struct in_multi *
lookup_mcast(ip_addr addr, NET netp)
{
   struct in_multi * imp;

   for (imp = netp->mc_list; imp; imp = imp->inm_next)
   {
      if(imp->inm_addr == addr)
         return imp;
   }
   return NULL;   /* addr not found in mcast list */
}
#endif   /* IP_V4 */


#ifdef IP_V6

/* FUNCTION: v6_lookup_multi()
 *
 * Look for the passed IPv6 address in the iface poassed.
 *
 * PARAM1: ip6_addr * - address to match
 * PARAM2: NET netp - interface to search
 *
 * RETURNS: pointer to mcast addr structure, or NULL if not found.
 */

struct in_multi *
v6_lookup_mcast(ip6_addr * ipp, NET netp)
{
   struct in_multi * imp;

   for (imp = netp->mc_list; imp; imp = imp->inm_next)
   {
      if(IP6EQ(&imp->ip6addr, ipp))
         return imp;
   }
   return NULL;   /* addr not found in mcast list */

}
#endif   /* IP_V6 */

#endif   /* IP_MULTICAST */






