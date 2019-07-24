/*
 * FILENAME: ipraw.c
 *
 * Copyright 2002 InterNiche Technologies Inc. All rights reserved
 *
 * Support for "raw" access to IP.
 *
 * MODULE: IP
 *
 * ROUTINES: ip_raw_open(), ip_raw_close(), ip_raw_input(),
 * ROUTINES: ip_raw_alloc(), ip_raw_free(), ip_raw_maxalloc()
 *
 * PORTABLE: yes
 */

#include "ipport.h"
#include "q.h"
#include "netbuf.h"
#include "net.h"
#include "ip.h"

#ifdef IP_RAW

/* IEP_ALLOC(), IEP_FREE() - default macros to support allocation
 *                           of raw IP endpoints
 *
 * These macros provide a default allocator based on the InterNiche
 * npalloc() and npfree() functions.  They may be overridden from
 * the target ipport.h file.
 */
#ifndef IEP_ALLOC
#define IEP_ALLOC(size) npalloc(size)
#endif  /* IEP_ALLOC */
#ifndef IEP_FREE
#define IEP_FREE(p)     npfree(p)
#endif  /* IEP_FREE */

/* ipraw_eps: head of list of raw IP endpoints
 */
struct ipraw_ep * ipraw_eps = NULL;

/* FUNCTION: ip_raw_open()
 *
 * Opens an endpoint for reception of raw IP datagrams.
 *
 * PARAM1: u_char prot; IN - the IP protocol ID for which
 *                      datagrams are to be received, or 0
 *                      to receive traffic for all IP 
 *                      protocol IDs not otherwise handled
 *                      by the stack.
 * PARAM2: ip_addr laddr; IN - the local IP address for which
 *                        datagrams are to be received, in 
 *                        network order; or 0 to receive traffic
 *                        directed to any IP address.
 * PARAM3: ip_addr faddr; IN - the remote peer IP address from 
 *                        which datagrams are to be received, in 
 *                        network order; or 0 to receive traffic
 *                        from any peer IP address.
 * PARAM4: int (*handler)(PACKET, void *); IN - pointer to an
 *                                         upcall function to be
 *                                         called on receipt of
 *                                         matching packets
 * PARAM5: void * data; IN - opaque token to be passed to the 
 *                      upcall function; may be used to identify 
 *                      the endpoint to the upcall function
 *
 * RETURNS: a pointer to the allocated endpoint structure, or
 *          NULL if no endpoint was allocated.
 */
struct ipraw_ep *
ip_raw_open(u_char prot,
            ip_addr laddr,
            ip_addr faddr,
            int (*handler)(PACKET, void *),
            void * data)
{
   struct ipraw_ep * ep;

   LOCK_NET_RESOURCE(NET_RESID);

   /* allocate a structure for the endpoint */
   ep = (struct ipraw_ep *)IEP_ALLOC(sizeof(struct ipraw_ep));
   if (ep == NULL)
   {
#ifdef NPDEBUG
      if (NDEBUG & INFOMSG)
         dprintf("IP: Couldn't allocate ep storage.\n");
#endif
      UNLOCK_NET_RESOURCE(NET_RESID);
      return ep;
   }

   /* fill it in with the caller's requested binding */
   ep->ipr_laddr = laddr;
   ep->ipr_faddr = faddr;
   ep->ipr_prot = prot;
   ep->ipr_rcv = handler;
   ep->ipr_data = data;

   /* link it into the list 
    * (at the head, because that's simple and fast) 
    */
   ep->ipr_next = ipraw_eps;
   ipraw_eps = ep;

   /* and return the pointer to the endpoint */
   UNLOCK_NET_RESOURCE(NET_RESID);
   return ep;
}

/* FUNCTION: ip_raw_close()
 * 
 * Closes an endpoint.
 *
 * PARAM1: struct ipraw_ep * ep; IN - pointer to endpoint
 *                               that is to be closed
 * RETURNS: void
 */
void
ip_raw_close(struct ipraw_ep * ep)
{
   struct ipraw_ep * prev_ep;
   struct ipraw_ep * curr_ep;

   LOCK_NET_RESOURCE(NET_RESID);

   /* search the list of endpoints for the one we're supposed to close */
   for (prev_ep = NULL, curr_ep = ipraw_eps;
        curr_ep != NULL;
        curr_ep = curr_ep->ipr_next)
   {
      if (curr_ep == ep)
         break;
      prev_ep = curr_ep;
   }

   /* if we didn't find it, we can't close it, so just return */
   if (curr_ep == NULL)
   {
#ifdef NPDEBUG
      /* caller passed pointer to endpoint not in list 
       * -- not fatal, but may be programming error
       */
      dtrap();
#endif /* NPDEBUG */
      UNLOCK_NET_RESOURCE(NET_RESID);
      return;
   }

   /* unlink it from the list */
   if (prev_ep)
      prev_ep = curr_ep->ipr_next;
   else
      ipraw_eps = curr_ep->ipr_next;

   /* free its storage */
   IEP_FREE(curr_ep);

   /* and return */
   UNLOCK_NET_RESOURCE(NET_RESID);
}

/* FUNCTION: ip_raw_input()
 *
 * Handles upcalls for raw IP endpoints, calling the endpoints'
 * upcall functions when a matching packet is received.
 *
 * PARAM1: PACKET p; IN - the received packet
 *
 * RETURNS: 0 if the received packet was accepted (and presumably
 *          freed); else an error code indicating that the
 *          received packet was not accepted/processed
 */
int
ip_raw_input(PACKET p)
{
   struct ip * pip;              /* the internet header */
   struct ipraw_ep * ep;
   struct ipraw_ep * next_ep;
   struct ipraw_ep * matched_ep = NULL;
   int err;
   int delivered;

   /* start out expecting to not deliver the packet */
   delivered = 0;

   /* get a pointer to the received packet's IP header */
   pip = (struct ip *)(p->nb_prot);

   /* search the list of raw-IP endpoints for matches */
   for (ep = ipraw_eps; ep != NULL; ep = next_ep)
   {
      /* keep track of next endpoint -- defense against upcall
       * function closing its own endpoint
       */
      next_ep = ep->ipr_next;

      /* if this packet doesn't match the endpoint's filters (IP
       * protocol ID, locally-bound address, connected-peer address)
       * then skip ahead to next endpoint
       */
      if (ep->ipr_prot && ep->ipr_prot != pip->ip_prot)
         continue;
      if (ep->ipr_laddr && ep->ipr_laddr != pip->ip_dest)
         continue;
      if (ep->ipr_faddr && ep->ipr_faddr != pip->ip_src)
         continue;

      /* if the endpoint has a receive upcall function, 
       * keep track of the endpoint
       */
      if (ep->ipr_rcv != NULL)
      {
         /* if we're already keeping track of a previously matched 
          * endpoint,
          * copy the packet into a new buffer,
          * and pass the new copy to the previously matched 
          * endpoint's upcall function 
          * before we forget the previous endpoint
          */
         if ((matched_ep != NULL) && (matched_ep->ipr_rcv != NULL))
         {
            PACKET p2;

            p2 = ip_copypkt(p);
            if (p2)
            {
               UNLOCK_NET_RESOURCE(NET_RESID);
               err = ((*matched_ep->ipr_rcv)(p2, matched_ep->ipr_data));
               LOCK_NET_RESOURCE(NET_RESID);
               if (err)
               {
                  LOCK_NET_RESOURCE(FREEQ_RESID);
                  pk_free(p2);
                  UNLOCK_NET_RESOURCE(FREEQ_RESID);
               }
               else
                  delivered = 1;
            }
         }
         matched_ep = ep;
      }
   }

   /* if we matched an endpoint, 
    * pass the packet to its upcall function
    * otherwise, return ENP_PARAM to indicate that the
    * packet was not processed and freed
    */
   if ((matched_ep != NULL) && (matched_ep->ipr_rcv != NULL))
   {
      UNLOCK_NET_RESOURCE(NET_RESID);
      err = ((*matched_ep->ipr_rcv)(p, matched_ep->ipr_data));
      LOCK_NET_RESOURCE(NET_RESID);
      if (err == 0)
         delivered = 1;
   }
   else
   {
      err = ENP_PARAM;
      ip_mib.ipUnknownProtos++;
   }

   if (!delivered)
      ip_mib.ipInDelivers--;

   return err;
}

/* FUNCTION: ip_raw_alloc()
 *
 * Allocates a packet buffer for a raw IP datagram
 *
 * PARAM1: int reqlen; IN- requested buffer length, in bytes (octets).
 * PARAM2: int hdrincl; IN- flag indicating whether application expects
 *                      to build its own IP header; if so the length
 *                      of the IP header must be included in reqlen
 *
 * RETURNS: a pointer to a packet buffer, or NULL if no buffer could
 *          be allocated.  The returned buffer's nb_prot is set to point 
 *          to the first location where the application should start
 *          writing: to where the IP header should be written if 
 *          inclhdr is non-zero, or to just past the IP header if 
 *          inclhdr is zero.
 */

PACKET
ip_raw_alloc(int reqlen, int hdrincl)
{
   int len;
   PACKET p;

   len = (reqlen + 1) & ~1;
   if (!hdrincl)
      len += IPHSIZ;
   LOCK_NET_RESOURCE(FREEQ_RESID);
   p = pk_alloc(len + MaxLnh);
   UNLOCK_NET_RESOURCE(FREEQ_RESID);
   if (p)
   {
      if (!hdrincl)
      {
         p->nb_prot += IPHSIZ;
         p->nb_plen -= IPHSIZ;
      }
   }
   return p;
}

/* FUNCTION: ip_raw_free()
 * 
 * Frees an allocated packet buffer
 *
 * PARAM1: PACKET p; IN- packet buffer to be freed
 *
 * RETURNS: void
 */

void
ip_raw_free(PACKET p)
{
   LOCK_NET_RESOURCE(FREEQ_RESID);
   pk_free(p);
   UNLOCK_NET_RESOURCE(FREEQ_RESID);
}

/* FUNCTION: ip_raw_maxalloc()
 * 
 * Returns the maximum supportable packet buffer allocation
 *
 * RETURNS: an integer whose value may be taken as a practical maximum
 *          for 
 */

int
ip_raw_maxalloc(int hdrincl)
{
   int len;

   len = bigbufsiz - MaxLnh;
   if (!hdrincl)
      len -= IPHSIZ;
   return len;
}

#endif /* IP_RAW */


