/*
 * FILENAME: ip_reasm.c
 *
 * Copyright 1997- 2000 By InterNiche Technologies Inc. All rights reserved
 *
 * IP reassembly.
 *
 * MODULE: INET
 *
 * ROUTINES: ip_reassm() 
 *
 * PORTABLE: yes
 */

/* Additional Copyrights: */
/* Portions Copyright 1993 by NetPort software */

#include "ipport.h"

#ifdef IP_FRAGMENTS  /* whole file can be ifdeffed out */

#include "q.h"
#include "netbuf.h"
#include "net.h"
#include "ip.h"
#include "ip_reasm.h"
#include "icmp.h"

/* number of bytes currently used by the reassembly process - this is the sum of
 * the lengths of the buffers used in the reassembly process */
u_long ipr_curr_mem = 0;

/* IRE statistics data structure */
struct ire_stats ire_stats;

/* head pointer to IP reassembly entry (IRE) queue */
IREP h_ireq = 0;
/* next invocation of timer tick processing routine */
u_long ire_cticks = 0;

IREP ip_reasm_match_frag_with_ire (struct ip * pip);
IP_FRAGTYPE ip_reasm_determine_type_of_frag (struct ip * pip);
int ip_reasm_check_mem_useage (u_short increment);
int ip_reasm_incr_mem_useage (u_short increment);
int ip_reasm_decr_mem_useage (u_short decrement);
int ip_reasm_process_first_fragment (PACKET p);
int ip_reassm (PACKET p);
IPREASM_RC ip_reasm_compute_overlap (PACKET p, IREP irep, u_short * indexp, RFQP * last_rfqpp, u_short * frag_offsetp, u_char * hole_createdp);
int ip_reasm_process_subsequent_fragments (PACKET p, IREP irep);
u_char ip_reasm_find_ire (IREP irep);
u_char ip_reasm_copy_queued_fragments_into_reassy_buffer (PACKET reassy_pkt, IREP irep);
int ip_reasm_delete_ire (IREP irep);
int ip_reasm_mark_compact_rfq (IREP irep);
int ipr_stats(void * pio);
#ifdef FULL_ICMP
u_long ip_reasm_send_icmp_timex (IREP irep);
#endif

/* FUNCTION: ip_reasm_frag_match_frag_with_ire ()

This function attempts to locate a IRE that corresponds to the received
packet.  This requires a comparison of the source address, destination 
address, identification, and protocol fields in the packet with their
counterparts in the IRE entry.  Note that all multi-byte fields (such
as IP source address, IP destination address, and identication) are 
stored in network-byte order, so no host-network translations are 
required prior to the comparison.

INPUT: Pointer to start of IP header of received packet

OUTPUT: Pointer to the IRE entry that has identical values for the source
address, destination address, identification, and protocol fields in the
packet being processed.  If no such entry is found, this function returns 
NULL.
*/

IREP ip_reasm_match_frag_with_ire (struct ip * pip)
{
   ip_addr src;
   ip_addr dest;
   u_char prot;
   u_short id;
   IREP tmpp;
 
   src = pip->ip_src;
   dest = pip->ip_dest;
   prot = pip->ip_prot;
   id = pip->ip_id;

   /* note that multi-byte fields such as the source address, destination address,
    * and id fields are stored in the IRE structure in network byte order */
   for (tmpp = h_ireq; tmpp; tmpp = tmpp->next)
      {
      if ((tmpp->src == src) && (tmpp->dest == dest) && 
          (tmpp->prot == prot) && (tmpp->id == id))
         {
         return tmpp; /* we've found a match */
         }
      }

   /* no match found */
   return NULL;
}

/* FUNCTION: ip_reasm_determine_type_of_frag ()

This function examines various fields in the IP header to determine
if a packet is a complete packet (IP_CP), first fragment (IP_FF), 
middle fragment (IP_MF), or last fragment (IP_LF).  It uses information 
from the fields in the IP header to make that decision.

   More Fragments (MF)     Fragmentation Offset    Classification
   --------------------------------------------------------------
   0                       0                       Complete Packet
   0                       Non-zero                Last Fragment
   1                       0                       First Fragment
   1                       Non-zero                Middle Fragment

INPUT: Pointer to start of IP header for packet

OUTPUT: One of the various IP_FRAGTYPE values (IP_CP, IP_FF, IP_MF, 
or IP_LF)
*/

IP_FRAGTYPE ip_reasm_determine_type_of_frag (struct ip * pip)
{
  u_short mf;
  u_short foff;
  IP_FRAGTYPE rc;

  mf = (((ntohs(pip->ip_flgs_foff)) & IP_FLG_MF) >> 13);
  foff = ((ntohs(pip->ip_flgs_foff)) & IP_EXTRACT_FOFF);

  if (mf == 0)
     {
     if (foff == 0) {rc = IP_CP;}
     else {rc = IP_LF;}
     }
  else
     {
     if (foff == 0) {rc = IP_FF;}
     else {rc = IP_MF;}
     }

  return rc;
}

/* FUNCTION: ip_reasm_check_mem_useage ()

This function computes the sum of the existing useage (as stored in 
ipr_curr_mem) and the requested allocation (by a function in the IP 
reassembly module).  If the sum total of the two items becomes greater 
than IP_REASM_MAX_MEM, it returns ENP_RESOURCE (signifying that the 
request would fail).

The reassembly code calls this function to check to see if a memory 
request would be successful before proceeding with its work (e.g., 
allocating memory for an IRE and packet (from ip_reasm_process_first_-
fragment ()), or a RFQ and packet (e.g. from ip_reasm_process_-
subsequent_fragments ())).  If the check passes, the caller subsequently 
invokes ip_reasm_incr_mem_useage () to record the increment.

INPUT: Amount of memory increment being requested

OUTPUT: ENP_RESOURCE, if the increment request can't be allowed; 
otherwise it returns IPREASM_OK.
*/

int ip_reasm_check_mem_useage (u_short increment)
{
   /* sanity check */
   if (ipr_curr_mem > IP_REASM_MAX_MEM)
   {
      /* this should never happen */
      ++ire_stats.bad_max_mem;
   }

   /* check to see if we are already at limit OR if we may become over limit 
    * after accepting this new fragment */
   if ((ipr_curr_mem == IP_REASM_MAX_MEM) ||
       (ipr_curr_mem + increment > IP_REASM_MAX_MEM))
   {
      /* return an error indication */
      ++ire_stats.mem_check_fail;
      return ENP_RESOURCE;
   }

   /* memory limits will not be exceeded with this increment */
   return IPREASM_OK;
}

/* FUNCTION: ip_reasm_incr_mem_useage ()

This function increments the counter that is used to keep track of the amount
of memory (IRE, RFQ, and packet buffers) that is currently being used by the
IP reassembly module.

INPUT: Number of bytes "taken" by the caller

OUTPUT: Returns ENP_RESOURCE if the requested allocation will result in the 
system exceeding the maximum limit or if the system has already exceeded 
the prespecified limit for memory useage; otherwise, it returns IPREASM_OK.
*/

int ip_reasm_incr_mem_useage (u_short increment)
{
   /* sanity checks */
   if (ipr_curr_mem > IP_REASM_MAX_MEM)
   {
      /* this should never happen */
      ++ire_stats.bad_max_mem;
      return ENP_RESOURCE;
   }
   if (ipr_curr_mem + increment > IP_REASM_MAX_MEM)
   {
      /* this should never happen since caller should have 
       * checked prior to asking for the increment */
      ++ire_stats.mem_incr_fail;
      return ENP_RESOURCE;
   }

   ipr_curr_mem += increment;

   /* successfully incremented memory useage counter */
   return IPREASM_OK;
}

/* FUNCTION: ip_reasm_decr_mem_useage ()

This function decrements the counter that is used to keep track of the amount
of memory (IRE, RFQ, and packet buffers) that is currently being used by the
IP reassembly module.

INPUT: Number of bytes "released" by the caller

OUTPUT: Returns ENP_RESOURCE if the amount of memory being released is less 
than the amount of memory currently outstanding or if the system has already 
exceeded the prespecified limit for memory useage; otherwise, it returns 
IPREASM_OK.
*/

int ip_reasm_decr_mem_useage (u_short decrement)
{
   /* sanity checks */
   if (ipr_curr_mem > IP_REASM_MAX_MEM)
   {
      /* this should never happen */
      ++ire_stats.bad_max_mem;
      return ENP_RESOURCE;
   }
   if (ipr_curr_mem < decrement)
   {
      /* this should never happen since the current memory useage
       * counter must always be greater than or at least equal to
       * the allocation that is being "returned" */
      ++ire_stats.mem_decr_fail;
      return ENP_RESOURCE;
   }

   ipr_curr_mem -= decrement;

   /* successfully decremented memory useage counter */
   return IPREASM_OK;
}

/* FUNCTION: ip_reasm_process_first_fragment ()

This function processes the first fragment received for a fragment stream.
(The first fragment received may or may not be the fragment with a
Fragment Offset field of 0 (i.e., First Fragment (FF)) in the IP header.)
If the memory resource checks pass, the function allocates an IRE entry
for the new fragment stream.  The IRE entry is initialized, and linked
into the global IRE queue (pointed to by h_ireq).  The memory useage
counters are also incremented to include the new IRE entry.

INPUT: Pointer to the received fragment

OUTPUT: ENP_RESOURCE if the memory check (in ip_reasm_check_mem_useage ()) 
or the allocation for an IRE fails; otherwise, it returns IPREASM_OK.
*/

int ip_reasm_process_first_fragment (PACKET p)
{
   struct ip * pip;
   IREP irep;
   IP_FRAGTYPE ftype;
   u_char iphlen;
   u_short frag_offset;
   u_short total_len;
   int rc;

   /* this is a fragment from a hitherto unknown fragment stream; 
    * check for resource limits before accepting it */
   if ((rc = ip_reasm_check_mem_useage (p->nb_blen + (sizeof (IRE)))) != IPREASM_OK)
   {
      LOCK_NET_RESOURCE (FREEQ_RESID);
      pk_free (p);
      UNLOCK_NET_RESOURCE (FREEQ_RESID);
      ++ip_mib.ipReasmFails;
      return rc;
   }

   /* we are ok wrt memory limits; since this is the first fragment, we need 
    * to create an IRE entry */
   irep = (IREP) IPR_ALLOC(sizeof(IRE));
   if (irep == 0) 
   {
      LOCK_NET_RESOURCE (FREEQ_RESID);
      pk_free (p);
      UNLOCK_NET_RESOURCE (FREEQ_RESID);
      ++ip_mib.ipReasmFails;
      return ENP_RESOURCE;
   }

   pip = ip_head(p);
   iphlen = ip_hlen(pip);
   ftype = ip_reasm_determine_type_of_frag (pip);
   frag_offset = (((ntohs(pip->ip_flgs_foff)) & IP_EXTRACT_FOFF) << 3);
   total_len = ntohs(pip->ip_len);

   /* now setup various fields in the IRE entry (multi-byte fields stored in network 
    * byte order) */
   irep->src = pip->ip_src;
   irep->dest = pip->ip_dest;
   irep->prot = pip->ip_prot;
   irep->id = pip->ip_id;
   if (ftype == IP_LF)
      {
      irep->length = frag_offset + (total_len - iphlen);
      }
   else if (ftype == IP_FF)
      {
      /* these two fields are used during the reassembly process to copy the L2 and L3 
       * headers into the reassembled packet.  They are also used when a reassembly times 
       * out, and results in the transmission of an ICMP Time Exceeded message (with the
       * code set to "fragment reassembly time exceeded").  Please note that the 'l2_hdr' 
       * as set below may not be the start address for the L2 header (but it is adequate 
       * for our use).  If the FF is never received, these fields stay at their initial
       * value of 0. */
      irep->l2_hdr = p->nb_buff;
      irep->l3_hdr = (char *) pip;
      }

   /* note that the 'rcvd' and 'length' counters only keep track of the data part of 
    * the IP datagram */
   irep->rcvd = total_len - iphlen;
   irep->age = 0;

   /* all RFQs are compact when created */
   irep->flags |= IPR_RFQ_COMPACT;

   /* store PACKET pointer in the first location of the first RFQ */
   irep->rfq.bufp[0] = p;
   irep->rfq.frag_offset[0] = frag_offset;
   /* the next pointer in the RFQ is already 0 */

   p->nb_prot += iphlen;
   p->nb_plen -= iphlen;

   /* insert at start of global IRE list */
   irep->next = h_ireq;
   h_ireq = irep;

   /* increment the memory useage */
   ip_reasm_incr_mem_useage (p->nb_blen + (sizeof (IRE)));

   return IPREASM_OK;
}

/* FUNCTION: ip_reassm ()

This is the main entry point into the IP reassembly module, and is called
when the stack has received a fragment that is destined for itself.  It
first checks to see if the received fragment matches an existing IRE; if
it doesn't, it invokes ip_reasm_process_first_fragment () which attempts
to allocate and initialize a new IRE structure (to store this and 
subsequently received fragments).  If a matching IRE already exists, this 
function calls ip_reasm_process_subsequent_fragments () to process the 
just received fragment.

INPUT: Pointer to the received fragment

OUTPUT: If either of the fragment processing functions fails, this function
merely passes their return code back to its caller.  Otherwise, it returns
IPREASM_OK indicating that the fragment was successfully processed.
*/

int ip_reassm (PACKET p)
{
   IREP irep;
   struct ip * pip;
   int rc1, rc2;

   pip = ip_head(p);

   /* we have just received a fragment, so let's start processing it.  First
    * check for a matching IRE entry. */
   irep = ip_reasm_match_frag_with_ire (pip);

   if (!irep)
   {
      /* this is the first packet for a "new" fragment stream */
      if ((rc1 = ip_reasm_process_first_fragment (p)) != IPREASM_OK)
      {
         /* ip_reasm_process_first_fragment () will free the packet */
         return rc1;
      }
   }
   else
   {
      /* a matching IRE already exists for this fragment */
      if ((rc2 = ip_reasm_process_subsequent_fragments (p, irep)) != IPREASM_OK)
      {
         /* ip_reasm_process_subsequent_fragments () has already deleted the IRE entry */
         return rc2;
      }
   }

   return IPREASM_OK;
}

/* FUNCTION: ip_reasm_compute_ovelap ()

This function is invoked for all fragments received after the first fragment 
(i.e., from ip_reasm_process_subsequent_fragments ()).  It "compares" the
just received fragment with each of the fragments currently queued in that
IRE to determine if there is any overlap in the payload portion.  Please note
that fragments stored in an IRE only contain "unique" data (i.e., there is 
no duplication).  Overlaps are handled by either dropping the just received
or queued fragment (the one that is currently being compared with the just
received fragment), or by modifying the start pointer and/or length of the 
just received fragment.

There are a total of 11 cases that arise when processing overlapping 
fragments.  (The cases are numbered as A1, A2, ..., F2.  The table below 
uses the following abbreviations.

C   = fragment currently being examined (while traversing IRE queue)
CS  = currstart (start offset of fragment currently being examined
CE  = currend (end offset of fragment currently being examined
JR  = just received fragment
JRS = jrstart (start offset of just received fragment)
JRE = jrend (end offset of just received fragment)

A1: CS < JRS CE < JRE CE <  JRS C is to the left of JR with zero overlap.
A2: CS < JRS CE < JRE CE >= JRS JR partially overlaps C on the latter's right.
B1: CS > JRS CE > JRE CS >  JRE C is to the right of JR with zero overlap.
B2: CS > JRS CE > JRE CS <= JRE JR partially overlaps C on the latter's left.
C : CS = JRS CE = JRE           Both (i.e., C and JR) are identical.
D1: CS = JRS CE > JRE           C is a superset of JR.
D2: CS = JRS CE < JRE           C is a subset of JR.
E1: CS < JRS CE = JRE           C is a superset of JR.
E2: CS < JRS CE > JRE           C is a superset of JR.
F1: CS > JRS CE = JRE           C is a subset of JR.
F2: CS > JRS CE < JRE           C is a subset of JR.

INPUTS: (1) Pointer to the received fragment
        (2) Pointer to the IRE entry that corresponds to the just received 
            fragment
        (3) Pointer to store index into bufp (in RFQ)
        (4) Pointer to store pointer to RFQ.
            Parameters (3) and (4) are used together in the following manner.
            (a) *indexp = INVALID_FRAG_INDEX, *last_rfqpp = non-zero: no empty 
                slot found in RFQ(s) traversed, and *last_rfqpp points to the last 
                RFQ that was processed.  This will result in a new RFQ being
                allocated (and linked into *last_rfqpp), and the just received 
                fragment will be stored in the first slot of the newly allocated
                RFQ.
            (b) *indexp != INVALID_FRAG_INDEX, *last_rfqpp = non-zero: empty 
                slot found in RFQ(s) traversed.  This slot will be used to
                store the just received fragment.
        (5) Pointer to storage for the start offset of the just received
            fragment (the start offset may be modified if an overlap is
            detected)
        (6) Pointer to storage for boolean variable that indicates if a
            hole was created during the processing of the just received
            fragment.  (A hole is created in a RFQ if the fragment 
            currently being examined ('C') is dropped.)

OUTPUT: (1) IPREASM_DROP_FRAG_BAD_PARAM, if the IRE pointer can't be validated, or
        (2) IPREASM_DROP_FRAG_DUPLICATE, if the just received fragment is dropped
            (because it doesn't contain any unique data), or
        (3) IPREASM_ACCEPT_FRAG, if the just received fragment is "acceptable"
            (i.e., contains "unique" data).
*/

IPREASM_RC ip_reasm_compute_overlap (PACKET p, IREP irep, u_short * indexp, 
 RFQP * last_rfqpp, u_short * frag_offsetp, u_char * hole_createdp)
{
   RFQP rfqp;
   PACKET currpkt;
   struct ip * jrip;
   u_short jrstart;
   u_short jrend;
   u_short currstart;
   u_short currend;
   u_short drop_len;
   u_char iphlen;
   u_short i;

   if (ip_reasm_find_ire (irep) != IPREASM_TRUE)
      {
      ++ire_stats.bad_irep;
      LOCK_NET_RESOURCE (FREEQ_RESID);
      pk_free (p);
      UNLOCK_NET_RESOURCE (FREEQ_RESID);
      return IPREASM_DROP_FRAG_BAD_PARAM;
      }

   jrip = ip_head(p);
   iphlen = ip_hlen (jrip);
   jrstart = (((ntohs(jrip->ip_flgs_foff)) & IP_EXTRACT_FOFF) << 3);
   jrend =   jrstart + ((ntohs(jrip->ip_len)) - iphlen) - 1;
   /* skip past the IP header of the just received fragment as we get ready 
    * to compare for overlap between the just received fragment and the 
    * fragments that are currently queued in the IRE entry for this fragment 
    * chain */
   p->nb_prot += iphlen;
   p->nb_plen -= iphlen;

   *frag_offsetp = jrstart;
   *indexp = INVALID_FRAG_INDEX;
   *last_rfqpp = 0;
   *hole_createdp = IPREASM_FALSE;

   rfqp = &(irep->rfq);

   while (rfqp)
      {
      for (i = 0; i < IPR_MAX_FRAGS; ++i)
         {
         if ((currpkt = rfqp->bufp[i]) != 0)
            {
            currstart = rfqp->frag_offset[i];
            currend = currstart + currpkt->nb_plen - 1;

            if (currstart < jrstart)
               {
               if (currend < jrend) /* cases A1 and A2 */
                  {
                  if (currend < jrstart) /* A1 */
                     {
                     /* current fragment is to the left of the just received 
                      * fragment, and there is zero overlap.  Move on to the 
                      * next fragment in the RFQ. */
                     }
                  else /* A2 */
                     {
                     /* the just received fragment partially overlaps current 
                      * fragment on the latter's right; increment start 
                      * pointer and decrement length of just received fragment.
                      * Move on to the next fragment in the RFQ. */
                     drop_len = (currend - jrstart) + 1;
                     p->nb_prot += drop_len;
                     p->nb_plen -= drop_len;
                     jrstart += drop_len;
                     jrend = jrstart + p->nb_plen - 1;
                     }
                  }
               else
                  {
                  /* currend is greater than or equal to jrend (cases E1 and E2).
                   * The current fragment is a superset of the just received 
                   * fragment.  Drop the just received fragment (and we're done). */
                  LOCK_NET_RESOURCE (FREEQ_RESID);
                  pk_free (p);
                  UNLOCK_NET_RESOURCE (FREEQ_RESID);
                  return IPREASM_DROP_FRAG_DUPLICATE;
                  }
               }
            else if (currstart > jrstart)
               {
               if (currend > jrend) /* cases B1 and B2 */
                  {
                  if (currstart > jrend) /* B1 */
                     {
                     /* current fragment is to the right of the just received 
                      * fragment, and there is zero overlap.  Move on to the 
                      * next fragment in the RFQ. */
                     }
                  else /* B2 */
                     {
                     /* just received fragment partially overlaps current 
                      * fragment on the latter's left; decrement length of 
                      * just received fragment.  Move on to the next fragment 
                      * in the RFQ. */
                     p->nb_plen = currstart - jrstart;
                     /* start offset remains unchanged, but jrend must be updated */
                     jrend = jrstart + p->nb_plen - 1;
                     }
                  }
               else if (currend == jrend) /* F1 */
                  {
                  /* current fragment is a subset of the just received fragment.  
                   * Decrement the length of the just received fragment.  Move
                   * on to the next fragment in the RFQ. */
                  p->nb_plen = currstart - jrstart;
                  jrend = jrstart + p->nb_plen - 1;
                  }
               else
                  {
                  /* currend is less than jrend (case F2).  current fragment is 
                   * a subset of the just received fragment.  Drop current
                   * fragment.  Move on to the next fragment in the RFQ.  Decrement
                   * the amount of memory currently in use. */
                  ip_reasm_decr_mem_useage (currpkt->nb_blen);
                  irep->rcvd -= currpkt->nb_plen;
                  LOCK_NET_RESOURCE (FREEQ_RESID);
                  pk_free (currpkt);
                  UNLOCK_NET_RESOURCE (FREEQ_RESID);
                  rfqp->bufp[i] = 0; /* mark slot as unused */
                  /* save the location of the empty slot */
                  if ((*indexp) == INVALID_FRAG_INDEX) 
                     {
                     *indexp = i;
                     *last_rfqpp = rfqp;
                     }
                  /* we've just created a hole; the array may no longer be "compact" */
                  *hole_createdp = IPREASM_TRUE;
                  }
               }
            else
               {
               /* currstart and jrstart are equal */
               if (currend == jrend) /* case C */
                  {
                  /* the current fragment is identical to the just received 
                   * fragment.  Drop the just received fragment (and we're done). */
                  LOCK_NET_RESOURCE (FREEQ_RESID);
                  pk_free (p);
                  UNLOCK_NET_RESOURCE (FREEQ_RESID);
                  return IPREASM_DROP_FRAG_DUPLICATE;
                  }
               else if (currend > jrend) /* case D1 */
                  {
                  /* current fragment is a superset of just received 
                   * fragment.  Drop the just received fragment (and we're done). */
                  LOCK_NET_RESOURCE (FREEQ_RESID);
                  pk_free (p);
                  UNLOCK_NET_RESOURCE (FREEQ_RESID);
                  return IPREASM_DROP_FRAG_DUPLICATE;
                  }
               else
                  {
                  /* currend is less than jrend (case D2).  current fragment 
                   * is a subset of the just received fragment.  Increment 
                   * the start pointer and decrement the length of the just
                   * received fragment.  Move on to the next fragment in 
                   * the RFQ. */
                  drop_len = (currend - jrstart) + 1;
                  p->nb_prot += drop_len;
                  p->nb_plen -= drop_len;
                  jrstart += drop_len;
                  jrend = jrstart + p->nb_plen - 1;
                  }
               }
            } /* end if (PACKET buffer exists at this slot) */
         else
            {
            if ((*indexp) == INVALID_FRAG_INDEX)
               {
               *indexp = i;
               *last_rfqpp = rfqp;               
               }
            if (irep->flags & IPR_RFQ_COMPACT)
               {
               break;
               }
            }
         } /* end FOR (0...(IPR_MAX_FRAGS - 1)) */

      /* skip saving RFQ pointer if we've already found an empty slot */
      if ((*indexp) == INVALID_FRAG_INDEX) 
         {
         *last_rfqpp = rfqp;
         }
      rfqp = rfqp->next;
      } /* end while (rfqp) */

   /* if no empty slots were found, (*last_rfqpp) will contain a pointer 
    * to the last RFQ that we were working with */

   *frag_offsetp = jrstart; /* stored in host byte order */

   return IPREASM_ACCEPT_FRAG;
}

/* FUNCTION: ip_reasm_process_subsequent_fragments ()

This function processes all fragments received after the first fragment 
for a fragment stream.  If the memory resource check passes, this
function first invokes ip_reasm_compute_overlap () to determine if the
fragment has any unique data.  If it does, the function then queues the
fragment into an available slot (in a RFQ).  If a slot isn't available,
a new RFQ is allocated (but only if the memory resource check passes).
If ip_reasm_compute_overlap () created one or more holes during its
operation, this function invokes ip_reasm_mark_compact_rfq () to update
the compact status of the IRE.

After the initial processing listed above is complete, this function
then checks to see if it has received all of the payload for the IP
datagram being reassembled.  If it has, it attempts to allocate a
reassembly buffer, and if successful, copies all of the received data
(from the various fragments) into the reassembly buffer (via ip_reasm_-
copy_queued_fragments_into_reassy_buffer ()).  After the copying is
done, the IRE is no longer required and is deleted.  The reassembled 
IP datagram is then passed back to the stack for further processing.

INPUTS: (1) Pointer to the received fragment
        (2) Pointer to IRE entry corresponding to just received fragment

OUTPUT: (1) ENP_PARAM, if the IRE pointer can't be validated
        (2) ENP_RESOURCE, if the memory check (in ip_reasm_check_mem_useage ()) 
            or the allocation for a RFQ fails, or
        (3) ENP_NOBUFFER, if the allocation for a buffer in which the
            fragments will be reassembled in fails, or
        (4) IPREASM_OK, if the received fragment completely overlaps already
            queued fragments (and hence has been dropped), or if the fragment
            has been processed successfully.
*/

int ip_reasm_process_subsequent_fragments (PACKET p, IREP irep)
{
   PACKET reassy_pkt;
   struct ip * pip;
   RFQP new_rfqp = 0;
   int rc;
   IP_FRAGTYPE ftype;
   IPREASM_RC rc2;
   u_short index;
   RFQP rfqp;
   u_short frag_offset;
   u_char hole_created;

   if (ip_reasm_find_ire (irep) != IPREASM_TRUE)
      {
      ++ire_stats.bad_irep;
      LOCK_NET_RESOURCE (FREEQ_RESID);
      pk_free (p);
      UNLOCK_NET_RESOURCE (FREEQ_RESID);
      ++ip_mib.ipReasmFails;
      return ENP_PARAM;
      }

   pip = ip_head(p);
   ftype = ip_reasm_determine_type_of_frag (pip);
   /* this fragment is a destined for an already queued fragment stream.
    * we update the 'length' field for LFs here because ip_reasm_compute_overlap ()
    * modifies the received fragment's nb_prot pointer (thereby making its IP header 
    * inaccessible) */
   if (ftype == IP_LF)
      {
      irep->length = (((ntohs(pip->ip_flgs_foff)) & IP_EXTRACT_FOFF) << 3) + ((ntohs(pip->ip_len)) - ip_hlen(pip));
      }
   else if (ftype == IP_FF)
      {
      irep->l2_hdr = p->nb_buff;
      irep->l3_hdr = (char *) pip;
      }

   /* Check for overlap, and determine the increase in memory resource 
    * requirements from the newly arrived fragment */
   if ((rc2 = ip_reasm_compute_overlap (p, irep, &index, &rfqp, &frag_offset, &hole_created)) != IPREASM_ACCEPT_FRAG)
      {
      switch (rc2)
         {
         case IPREASM_DROP_FRAG_DUPLICATE:
            /* this isn't really an error, since it indicates that the received 
             * fragment's information is a duplicate of what is present in the 
             * already queued fragments */
            return IPREASM_OK;
         case IPREASM_DROP_FRAG_BAD_PARAM:
            /* this is a real error */
            return ENP_PARAM;
         /*
          * Altera Niche Stack Nios port modification:
          * Handle IPREASM_ACCEPT_FRAG case (will never
          * get here per if() above) to remove build warning.
          */
         case IPREASM_ACCEPT_FRAG:
            while(0);
         }
      }

   /* this is a fragment from an existing fragment stream; 
    * check for resource limits before accepting it.  For now
    * assume that an extra RFQ is not required. */
   if ((rc = ip_reasm_check_mem_useage (p->nb_blen)) != IPREASM_OK)
      {
      LOCK_NET_RESOURCE (FREEQ_RESID);
      pk_free (p);
      UNLOCK_NET_RESOURCE (FREEQ_RESID);
      ip_reasm_delete_ire (irep);
      ++ip_mib.ipReasmFails;
      return rc;
      }

   /* the two combinations of index and rfqp are as follows:
    * (1) index = INVALID_FRAG_INDEX, rfqp = non-zero: no empty slot found in RFQ(s),
    *     and rfqp points to the last RFQ that was processed
    * (2) index != INVALID_FRAG_INDEX, rfqp = non-zero: empty slot found in RFQ(s) */
   if (index != INVALID_FRAG_INDEX)
      {
      /* we have an empty slot in the PACKET array into which we can place 
       * the just received fragment */
      rfqp->bufp[index] = p;
      rfqp->frag_offset[index] = frag_offset;
      }
   else
      {
      /* since we need to allocate an extra RFQ, check for memory useage again... */
      if ((rc = ip_reasm_check_mem_useage (p->nb_blen + (sizeof (RFQ)))) != IPREASM_OK)
         {
         LOCK_NET_RESOURCE (FREEQ_RESID);
         pk_free (p);
         UNLOCK_NET_RESOURCE (FREEQ_RESID);
         ip_reasm_delete_ire (irep);
         ++ip_mib.ipReasmFails;
         return rc;
         }

      /* no slots are available in the one (or more) existing RFQs; we 
       * need to create a new RFQ to store the just received fragment */
      new_rfqp = (RFQP) IPR_ALLOC (sizeof(RFQ));
      if (new_rfqp == 0)
         {
         /* can't store the existing fragment, so we'll drop it */
         LOCK_NET_RESOURCE (FREEQ_RESID);
         pk_free (p);
         UNLOCK_NET_RESOURCE (FREEQ_RESID);
         ip_reasm_delete_ire (irep);
         ++ip_mib.ipReasmFails;
         return ENP_RESOURCE;
         }
      else
         {
         new_rfqp->bufp[0] = p;
         new_rfqp->frag_offset[0] = frag_offset;
         rfqp->next = new_rfqp;
         /* newly allocated RFQ already has its next pointer set to 0 */
         }
      }
      
      /* check to see if the RFQ is still compact, but only if hole(s) were created 
       * in ip_reasm_compute_overlap () */
      if (hole_created)
         {
         if ((rc = ip_reasm_mark_compact_rfq (irep)) != IPREASM_OK)
            {
            /* an error return is due to a bad IRE pointer, which ip_reasm_mark_compact_rfq () logs */
            LOCK_NET_RESOURCE (FREEQ_RESID);
            pk_free (p);
            UNLOCK_NET_RESOURCE (FREEQ_RESID);
            ++ip_mib.ipReasmFails;
            return rc;
            }
         }

   /* compute the total number of bytes queued in this IRE.  Note 
    * that the RHS has been updated to reflect the amount of "unique"
    * data in the just received fragment (the IP header has been dropped,
    * and perhaps additional duplicate data too (if present)) */
   irep->rcvd += (u_short) (p->nb_plen);
   ip_reasm_incr_mem_useage (p->nb_blen + ((new_rfqp == 0) ? 0 : (sizeof(RFQ))));

   if (irep->length != 0)
      {
      /* we know the total length of the original unfragmented datagram; 
       * let's check to see if we have all of the bytes... */
      if (irep->rcvd == irep->length)
         {
         /* yes, we do; let's reassemble the entire datagram in one data buffer.
          * Ensure that we allocate space for the data link header, IP header, 
          * and the payload of the original, unfragmented datagram.  We pick 
          * the data link layer and IP headers from the First Fragment (FF), 
          * but we need to adjust some of the fields in the IP header after 
          * reassembly is complete. */
         pip = (struct ip *) irep->l3_hdr;
         LOCK_NET_RESOURCE (FREEQ_RESID);
         reassy_pkt = pk_alloc (irep->length + (irep->l3_hdr - irep->l2_hdr) + ip_hlen (pip));
         UNLOCK_NET_RESOURCE (FREEQ_RESID);
         if (reassy_pkt == 0)
            {
            /* the following call will delete all queued fragments, including
             * the currently received one */
            ip_reasm_delete_ire (irep);
            ++ip_mib.ipReasmFails;
            return ENP_NOBUFFER;
            }
         else
            {
            ip_reasm_copy_queued_fragments_into_reassy_buffer (reassy_pkt, irep);
            /* free the IRE structure (and its constituent elements) */
            ip_reasm_delete_ire (irep);
            /* at this point, 'irep' is no longer a valid pointer, and so should 
             * not be referenced.  We're done, and we now pass the packet for 
             * demux'ing to the appropriate entity (e.g., UDP, TCP, etc.).  Note
             * that ip_demux () expects 'nb_prot' to point to the beginning of 
             * the IP header. */
            ++ip_mib.ipReasmOKs;
            ip_demux (reassy_pkt);
            }
         }
      else
         {
         /* we haven't received all of the data yet, and so reassembly can't complete */
         }
      }
   else
      {
      /* we haven't received the LF (so we don't know the total amount of data in 
       * the original, unfragmented datagram), and therefore reassembly can't complete yet */
      }

   return IPREASM_OK;
}

/* FUNCTION: ip_reasm_frag_find_ire ()

This function searches thru' the IRE list (pointed to by h_ireq) to see if
an IRE is on that list.  It is typically used to validate the IRE passed in
to a function.

INPUT: Pointer to IRE

OUTPUT: This function returns a IPREASM_TRUE if the IRE exists in the master 
IRE list; otherwise, it returns a IPREASM_FALSE.
*/

u_char ip_reasm_find_ire (IREP irep)
{
   IREP tmpp;

   /* check to see if the IRE exists in the IREQ linked list */
   for (tmpp = h_ireq; tmpp; tmpp = tmpp->next)
      {
      if (tmpp == irep) 
         {
         return IPREASM_TRUE;
         }
      }

   return IPREASM_FALSE;
}

/* FUNCTION: ip_reasm_copy_queued_fragments_into_reassy_buffer ()

This function copies all fragments queued in an IRE into a reassembly
buffer.  After the copying is complete, it sets the various fields in
the PACKET data structure and the IP header for the reassembled IP
datagram.

INPUT: (1) Pointer to struct netbuf data structure that will be used
           to the reassembled IP datagram
       (2) Pointer to IRE that contains the fragments being reassembled

OUTPUT: This function always returns IPREASM_OK.
*/

u_char ip_reasm_copy_queued_fragments_into_reassy_buffer (PACKET reassy_pkt, IREP irep)
{
   struct ip * pip;
   u_char offset;
   RFQP rfqp;
   u_short i;
   PACKET p;
   u_char iphlen;
   char * writep;
   PACKET sav_pkt = 0;

   pip = (struct ip *) irep->l3_hdr;
   iphlen = ip_hlen (pip);

   /* copy the data link and IP layer headers into place.  These headers are 
    * from the First Fragment (FF). */
   offset = (u_char) ((irep->l3_hdr - irep->l2_hdr) + iphlen);
   MEMCPY(reassy_pkt->nb_buff, irep->l2_hdr, offset);

   rfqp = &(irep->rfq);
   writep = reassy_pkt->nb_buff + offset;

   while (rfqp)
      {
      for (i = 0; i < IPR_MAX_FRAGS; ++i)
         {
         if ((p = rfqp->bufp [i]) != 0)
            {
            /* note that rfqp->frag_offset[i] is the true value of the offset field, 
             * and does not require any scaling (like the Fragment Offset field in 
             * the IP header) */
            MEMCPY((writep + rfqp->frag_offset[i]), p->nb_prot, p->nb_plen);
            /* free fragment after we've copied data out from it (unless we need to
             * save it so that we can copy some fields from it later (after exiting 
             * from the loop)) */
            if (!sav_pkt) sav_pkt = p;
            else
               {
               ip_reasm_decr_mem_useage (p->nb_blen);
               LOCK_NET_RESOURCE (FREEQ_RESID);
               pk_free (p);
               UNLOCK_NET_RESOURCE (FREEQ_RESID);
               }
            /* since we're done with the packet, mark slot as unused */
            rfqp->bufp [i] = 0;
            }
         else
            {
            if (irep->flags & IPR_RFQ_COMPACT)
               {
               break;
               }
            }
          }

      rfqp = rfqp->next;
      }

   /* update various fields in the reassembled packet.  Some fields will be 
    * picked from one of the constituent fragments (sav_pkt) which hasn't
    * been deleted yet. */
   reassy_pkt->nb_prot = reassy_pkt->nb_buff + (irep->l3_hdr - irep->l2_hdr);
   reassy_pkt->nb_plen = irep->length + iphlen;
   reassy_pkt->nb_tstamp = cticks;
   reassy_pkt->flags |= ((sav_pkt->flags) & (PKF_BCAST | PKF_MCAST));
   reassy_pkt->net = sav_pkt->net;
   reassy_pkt->type = sav_pkt->type;
   /* fhost is set by ip_demux () before handing off to the packet to its 
    * intended destination (protocol) */

   /* free the saved packet too, since we don't need it anymore */
   ip_reasm_decr_mem_useage (sav_pkt->nb_blen);
   LOCK_NET_RESOURCE (FREEQ_RESID);
   pk_free (sav_pkt);
   UNLOCK_NET_RESOURCE (FREEQ_RESID);

   /* after the reassembly is complete, we update the following fields in
    * the IP header: Total Length, Flags/Fragment Offset, header checksum */
   pip = ip_head(reassy_pkt);
   pip->ip_len = htons(reassy_pkt->nb_plen);
   /* turn off More Fragments (MF) bit; the DF bit stays unchanged */
   pip->ip_flgs_foff &= htons(~((u_short) IP_FLG_MF));
   /* clear the Fragment Offset bits */
   pip->ip_flgs_foff &= htons(~((u_short) IP_EXTRACT_FOFF)); 
   /* the following isn't really required, and can be removed */
   pip->ip_chksum = IPXSUM;
   pip->ip_chksum = ~cksum (pip, (iphlen/2));

   return IPREASM_OK;
}

/* FUNCTION: ip_reasm_process_timer_tick ()

This function processes the IRE list once every second.  It is invoked 
from inet_timer ().  All IREs start off with an age of 0, and are 
deleted when they reach an age of IRE_TMO.

This function obtains the NET_RESID semaphore before proceeding with 
its work.  This protects the consistency of shared data structures 
(such as the IRE, etc.) between the timer and the packet processing 
receive tasks.

INPUT: None.

OUTPUT: This function always returns IPREASM_OK.
*/

u_char ip_reasm_process_timer_tick (void)
{
   IREP tmpp;
   IREP nxt_tmpp;

   LOCK_NET_RESOURCE (NET_RESID);
 
   for (tmpp = h_ireq; tmpp; tmpp = nxt_tmpp)
      {
      /* save the next pointer for the IRE that may be deleted */
      nxt_tmpp = tmpp->next;
      ++tmpp->age;
      /* check to see if this entry has reached its max age (expired)? */
      if (tmpp->age == IRE_TMO)
         {
         /* it has...and therefore must be deleted. */
         ++ire_stats.ire_timed_out;
#ifdef FULL_ICMP
         /* send ICMP Time Exceeded message with code 1 ("fragment reassembly time exceeded") */
         ip_reasm_send_icmp_timex (tmpp);
#endif
         ip_reasm_delete_ire (tmpp);
         ++ip_mib.ipReasmFails;
         }
      }

   /* set the time for the next invocation of this routine (one second later) */
   ire_cticks = cticks + TPS;

   UNLOCK_NET_RESOURCE (NET_RESID);

   return IPREASM_OK;
}

/* FUNCTION: ip_reasm_delete_ire ()

This function deletes an IRE.  When deleting an IRE, it also deletes any
queued PACKETs (and RFQs, if any were allocated).  This code can be invoked
in various scenarios:

(1) when the IRE times out,
(2) when all bytes (associated with a fragment stream) have been processed
    (in ip_reasm_process_subsequent_fragments ()), or
(4) in the event of an error (e.g., when memory resource consumption limits 
    would be exceeded if a fragment was "accepted", or if storage cannot be
    allocated for the reassembled packet).

INPUT: Pointer to IRE

OUTPUT: This function returns a ENP_PARAM if the IRE does not exist in the 
master IRE list; otherwise, it returns a IPREASM_OK.
*/

int ip_reasm_delete_ire (IREP irep)
{
   RFQP rfqp, first_rfqp, prev_rfqp;
   u_short i;
   PACKET p;
   IREP tmpp;
   IREP prev_tmpp = 0;

   /* check to see if the IRE exists in the master table; if it does,
    * remove it and also update the pointers in that list */
   for (tmpp = h_ireq; tmpp; tmpp = tmpp->next)
      {
      if (tmpp == irep) 
         {
         /* update the head pointer to the list */
         if (tmpp == h_ireq) h_ireq = irep->next;
         else prev_tmpp->next = irep->next;
         break;
         }
      prev_tmpp = tmpp;
      }

   /* if the IRE entry does not exist in the master table, return an error 
    * to the caller */
   if (!tmpp)
      {
      ++ire_stats.bad_irep;
      return ENP_PARAM;
      }

   /* free any queued packets (there may be none if the IRE is being deleted
    * because the packet has been successfully reassembled) */
   rfqp = first_rfqp = &(irep->rfq);

   while (rfqp)
      {
      for (i = 0; i < IPR_MAX_FRAGS; ++i)
         {
         if ((p = rfqp->bufp [i]) != 0)
            {
            ip_reasm_decr_mem_useage (p->nb_blen);
            LOCK_NET_RESOURCE (FREEQ_RESID);
            pk_free (p);
            UNLOCK_NET_RESOURCE (FREEQ_RESID);               
            }
         else
            {
            if (irep->flags & IPR_RFQ_COMPACT)
               {
               break;
               }
            }
          }

       prev_rfqp = rfqp;
       rfqp = rfqp->next;
       /* the first RFQ is statically allocated, and cannot be freed */
       if (prev_rfqp != first_rfqp)
          {
          ip_reasm_decr_mem_useage (sizeof(RFQ));
          IPR_FREE (prev_rfqp);
          }
      }

   /* now free the parent entity */
   ip_reasm_decr_mem_useage (sizeof(IRE));
   IPR_FREE (irep);

   return IPREASM_OK;
}

/* FUNCTION: ip_reasm_mark_compact_rfq ()

This function iterates thru' the RFQs inside an IRE to determine the
compact/non-compact status of an IRE.

INPUT: Pointer to IRE

OUTPUT: This function returns a ENP_PARAM if the IRE does not exist in 
the master IRE list; otherwise, it returns a IPREASM_OK.
*/

int ip_reasm_mark_compact_rfq (IREP irep)
{
   RFQP rfqp;
   u_short i;
   u_char empty_slot_discovered = IPREASM_FALSE;
   u_char compact = IPREASM_TRUE;

   if (ip_reasm_find_ire (irep) != IPREASM_TRUE)
   {
      ++ire_stats.bad_irep;
      return ENP_PARAM;
   }

   rfqp = &(irep->rfq);

   while (rfqp)
      {
      for (i = 0; i < IPR_MAX_FRAGS; ++i)
         {
         if (rfqp->bufp [i] == 0)
            {            
            empty_slot_discovered = IPREASM_TRUE;
            }
         else
            {
            if (empty_slot_discovered) 
               {
               compact = IPREASM_FALSE;
               break;
               }
            }
          }
       rfqp = rfqp->next;
      }

   if (compact) 
      {
      irep->flags |= IPR_RFQ_COMPACT;
      }
   else 
      {
      irep->flags &= ~IPR_RFQ_COMPACT;
      }

   return IPREASM_OK;
}

/* FUNCTION: ipr_stats ()

This function prints information related to the operation of the IP reassembly 
module.

INPUT: Pointer to I/O device

OUTPUT: This function always returns IPREASM_OK.
*/

int ipr_stats(void * pio)
{
   unsigned long ticks_elapsed = cticks;
   u_short size_ire = sizeof(IRE);
   u_short size_rfq = sizeof(RFQ);
   IREP tmpp;
   u_short count = 0;
   RFQP rfqp;
   u_short frag_count = 0;
   u_short i;

   ns_printf(pio,"IP reassembly statistics:\n");
   ns_printf(pio,"Current memory useage %lu, ticks %lu, secs %lu, IRE %u, RFQ %u\n",ipr_curr_mem,ticks_elapsed,(ticks_elapsed/TPS),size_ire,size_rfq);
   ns_printf(pio,"[ERR] IRE T/O %lu, IRE ptr %lu, max mem %lu, mem chk %lu, mem inc %lu, mem dec %lu\n",\
    ire_stats.ire_timed_out,ire_stats.bad_irep,ire_stats.bad_max_mem,ire_stats.mem_check_fail,ire_stats.mem_incr_fail,ire_stats.mem_decr_fail);

   ns_printf(pio,"Head of IRE queue %p\n",h_ireq); 
   for (tmpp = h_ireq; tmpp; tmpp = tmpp->next)
      {
      ++count;
      /* compute the total number of fragments queued awaiting reassembly for this IRE */
      rfqp = &(tmpp->rfq);
      while (rfqp)
         {
         for (i = 0; i < IPR_MAX_FRAGS; ++i)
            {
            if (rfqp->bufp [i] != 0)
               {
               ++frag_count;            
               }
            else
               {
               if (tmpp->flags & IPR_RFQ_COMPACT)
                  {
                  break;
                  }
               }
            }
         rfqp = rfqp->next;
         } /* end WHILE (fragments queued) */
      ns_printf(pio,"IRE %p [Frags queued %u] SA 0x%lx DA 0x%lx Prot %u Id %u Len %u Rcvd %u Age %lu L2H %p L3H %p Compact %u]\n",tmpp,frag_count,(ntohl(tmpp->src)),(ntohl(tmpp->dest)),tmpp->prot,(ntohs(tmpp->id)),tmpp->length,tmpp->rcvd,tmpp->age,tmpp->l2_hdr,tmpp->l3_hdr,((tmpp->flags) & IPR_RFQ_COMPACT));
      } /* end FOR (IRE linked list) */

   ns_printf(pio,"Found a total of %u IRE entries\n",count);

   return IPREASM_OK;
}

/* FUNCTION: ip_reasm_send_icmp_timex ()

This function sends the ICMP Time Exceeded message (with code 1 
("fragment reassembly time exceeded") when an IP reassembly times
out (i.e., was not completed in the stipulated time).  This message 
can only be sent if we have received the First Fragment (FF) of 
that fragment stream; if we haven't, this function returns without 
sending any ICMP message.

INPUT: Pointer to IRE

OUTPUT: This function always returns IPREASM_OK.
*/

#ifdef FULL_ICMP
u_long ip_reasm_send_icmp_timex (IREP irep)
{
   struct ip * pip;

   /* we can only send an ICMP Time Exceeded message with code 1 ("fragment reassembly 
    * time exceeded") if we have received the First Fragment (FF) of the original, 
    * unfragmented datagram.  This is indicated by a non-zero value for the l2_hdr and
    * l3_hdr fields in the IRE data structure. */
   if (irep->l2_hdr)
      {
      pip = (struct ip *) irep->l3_hdr;
      icmp_timex (pip, TIMEX_REASSY_FAILED);
      }

   return IPREASM_OK;
}
#endif

#endif   /* IP_FRAGMENTS - whole file can be ifdeffed out */

/* end of file ip_reasm.c */

