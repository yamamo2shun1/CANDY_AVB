/*
 * FILENAME: pktalloc.c
 *
 * Copyright  2000 By InterNiche Technologies Inc. All rights reserved
 *
 * Code to manage the queues of free packet buffers.
 *
 * MODULE: INET
 *
 * ROUTINES: pk_init(), pk_alloc(), pk_validate(), pk_free()
 * ROUTINES: pk_prepend(), pk_gather()
 * ROUTINES: pk_alloc_heapbuf(), pk_validate_heapbuf(), pk_free_heapbuf()
 *
 * PORTABLE: yes
 */

/* Additional Copyrights: */
/* Portions Copyright 1990-1994 by NetPort Software  */
/* Portions Copyright 1986 by Carnegie Mellon  */
/* Portions Copyright 1984, 1985 by the Massachusetts Institute of Technology  */

/*
 * Since the various allocation and deallocation functions listed in this 
 * file can be invoked from both process (or background code for SUPERLOOP-
 * type systems) and interrupt-level, they must be designed to provide 
 * mutual exclusion as appropriate (e.g., between task <-> task and between 
 * task <-> interrupt handler) to ensure the integrity of the various shared 
 * data structures.
 *
 * Shared data structures are "protected" via the {LOCK_NET_RESOURCE 
 * (FREEQ_RESID) and UNLOCK_NET_RESOURCE (FREEQ_RESID)} or {INCR_SHARED_VAR 
 * () or DECR_SHARED_VAR ()} or {ENTER_CRIT_SECTION () and EXIT_CRIT_SECTION 
 * ()} macros (depending upon the design of the target system).  Note that 
 * the INCR_SHARED_VAR () and DECR_SHARED_VAR (), in turn, use ENTER_CRIT_-
 * SECTION () and EXIT_CRIT_SECTION () to provide the mutual exclusion.
 */

#include "ipport.h"
#include "q.h"
#include "netbuf.h"
#include "net.h"
#include "ip.h"
#ifdef HEAPBUFS
#include "heapbuf.h"
#endif


/* We maintain 2 queues of free buffers, one for smallish packets
 * like ARPs, TCP acks,and PINGs; and big buffers for large datagrams
 * like FTP transfers.
 */
queue   bigfreeq;    /* big free buffers */
queue   lilfreeq;    /* small free buffers */

unsigned lilbufs = NUMLILBUFS;      /* number of small bufs to init */
unsigned lilbufsiz = LILBUFSIZE;    /* big enough for most non-full size packets */
unsigned bigbufs = NUMBIGBUFS;      /* number of big bufs to init */
unsigned bigbufsiz = BIGBUFSIZE;    /* big enough for max. ethernet packet */

#ifdef NPDEBUG
PACKET pktlog[MAXPACKETS]; /* record where the packets are */
#endif

#ifdef HEAPBUFS
#ifdef HEAPBUFS_DEBUG
/* private heapbuf list queue - contains list of all current heapbuf allocations,
 * and is used for debugging purposes */
queue phlq;
#endif /* HEAPBUFS_DEBUG */
/* variable that is used to indicate whether accesses to the heap may potentially
 * block (HEAP_ACCESS_BLOCKING) or not (HEAP_ACCESS_NONBLOCKING) */
enum heap_type heap_type;
/* data structure that keeps track of various failure and success statistics 
 * for the heap buffer allocation and deallocation processes */
u_long hbufstats [HBUF_NUM_STATS];
/* amount of memory currently allocated from the heap for heap buffers */
u_long heap_curr_mem;
/* high watermark for 'heap_curr_mem' */
u_long heap_curr_mem_hi_watermark;

/* allocate a heap buffer */
PACKET pk_alloc_heapbuf (unsigned len);
/* validate a returned heap buffer */
int pk_validate_heapbuf (PACKET pkt);
/* deallocate a heap buffer */
void pk_free_heapbuf (PACKET pkt);
/* dump heap buffer statistics */
int dump_hbuf_stats (void * pio);
#endif /* HEAPBUFS */


/* dump regular (and heap buffer, when HEAPBUFS are defined) error statistics */
int dump_buf_estats (void * pio);

/* statistics data structure that contains counters for various
 * error conditions encountered when processing "regular" (little
 * and big) and heap buffers (the latter only when HEAPBUFS is defined) */
u_long memestats [MEMERR_NUM_STATS];

#define PKTALLOC_TRUE 1
#define PKTALLOC_FALSE 0

/* FUNCTION: pk_init()
 *
 * Initialize the free queues for use by pk_alloc() and pk_free() for "regular" 
 * (little and big) buffers.  
 *
 * This function also initializes heap buffer-related data structures, such
 * as the total amount of heap memory allocated (for use in heap buffers), 
 * the high watermark of the total amount of heap memory allocated (for use
 * in heap buffers), and the heap's access type.  The last parameter is specific
 * to the target system that this code is being ported to, and care must be
 * taken to ensure that it is set correctly.
 *
 * If NPDEBUG is not defined, the user can use ALIGN_BUFS to force a
 * specific alignment for a (little or big) buffer's start address.
 * Note that the current implementation requires that ALIGN_BUFS be
 * a power of 2.
 * 
 * INPUT: none
 *
 * OUTPUT: Returns 0 if OK; in the event of an error (such as when an allocation
 * for a PACKET buffer or a data buffer fails, or if there is an inconsistency
 * between (bigbufs + lilbufs) and MAXPACKETS) it returns -1. 
 */

int pk_init (void)
{
   PACKET packet;
   unsigned i;
   unsigned numpkts = bigbufs + lilbufs;
   u_char align_req;
   
#ifdef ALIGN_BUFS
   align_req = ALIGN_BUFS;
#else
   align_req = 0;
#endif

   for (i = 0; i < numpkts; i++)
   {
      packet = (PACKET)NB_ALLOC(sizeof(struct netbuf));
      if (packet == NULL)
         goto no_pkt_buf;

#ifdef NPDEBUG
      if (i >= MAXPACKETS)
      {
         dprintf("pk_init: bad define\n");
         return -1;
      }
      pktlog[i] = packet;     /* save for debugging */
#endif

      packet->nb_tstamp = 0L;

      if (i < bigbufs)
      {
#ifdef NPDEBUG
         {
            int j;

            /* for DEBUG compiles, bracket the data area with special chars */
            packet->nb_buff = (char *)BB_ALLOC(bigbufsiz+ALIGN_TYPE+1);
            if (!(packet->nb_buff))
               goto no_pkt_buf;

            /* Add memory markers for sanity check */
            for(j = 0; j < ALIGN_TYPE; j++)
               *(packet->nb_buff + j) = 'M'; /* MMs at start of buf */

            *(packet->nb_buff + bigbufsiz + ALIGN_TYPE) = 'M';
            packet->nb_buff += ALIGN_TYPE;   /* bump buf past MMs */
         }
#else
         packet->nb_buff = (char *)BB_ALLOC(bigbufsiz + align_req);
#ifdef ALIGN_BUFS
         /* align start of buffer pointer to desired offset */
         packet->nb_buff += (ALIGN_BUFS - (((u_long) packet->nb_buff) & (ALIGN_BUFS - 1)));
#endif
#endif
         if (!(packet->nb_buff))
            goto no_pkt_buf;
         packet->nb_blen = bigbufsiz;
         q_add(&bigfreeq, packet);        /* save it in big pkt free queue */
      }
      else     /* get a small buffer */
      {
#ifdef NPDEBUG
         {
            int j;

            /* for DEBUG compiles, bracket the data area with special chars */
            packet->nb_buff = (char *)LB_ALLOC(lilbufsiz+ALIGN_TYPE+1);
            if (!(packet->nb_buff))
               goto no_pkt_buf;

            /* Add memory markers for sanity check */
            for(j = 0; j < ALIGN_TYPE; j++)
               *(packet->nb_buff + j) = 'M'; /* MMs at start of buf */

            *(packet->nb_buff + lilbufsiz + ALIGN_TYPE) = 'M';
            packet->nb_buff += ALIGN_TYPE;
         }
#else
         packet->nb_buff = (char *)LB_ALLOC(lilbufsiz + align_req);
#ifdef ALIGN_BUFS
         /* align start of buffer pointer to desired offset */
         packet->nb_buff += (ALIGN_BUFS - (((u_long) packet->nb_buff) & (ALIGN_BUFS - 1)));
#endif
#endif
         if (!(packet->nb_buff))
            goto no_pkt_buf;
         packet->nb_blen = lilbufsiz;
         q_add(&lilfreeq, packet);        /* save it in little free queue */
      }
   }
   bigfreeq.q_min = bigbufs;
   lilfreeq.q_min = lilbufs;

   USE_ARG(align_req);

#ifdef HEAPBUFS
   /* initialize the counters that keep track of the total amount of memory 
    * allocated from the heap and the corresponding high watermark */
   heap_curr_mem = 0;
   heap_curr_mem_hi_watermark = 0;
   /* set the heap's access type to blocking */
   heap_type = HEAP_ACCESS_BLOCKING;
#endif

   return 0;

no_pkt_buf:
#ifdef NPDEBUG
   dprintf("Netinit: calloc failed getting buffer %d\n", i);
#endif
   return(-1);
}

/* FUNCTION: pk_alloc ()
 *
 * This function is invoked to allocate memory.  If the requested allocation
 * is greater than bigbufsiz, this function will return 0 if HEAPBUFS is 
 * not defined; however, if HEAPBUFS is defined, it will attempt to allocate
 * a buffer from the heap via pk_alloc_heapbuf ().  After obtaining a regular
 * little or big) or heap buffer that meets the caller's requirements, this
 * function initializes various fields in the struct netbuf structure that 
 * corresponds to the just allocated data buffer.  Note that all struct 
 * netbuf fields are not initialized in this function; the remaining fields 
 * are initialized in either pk_init () (for regular buffers) or in 
 * pk_alloc_heapbuf () (for heap buffers).
 * 
 * INPUT: Length of requested allocation
 *
 * OUTPUT: 0 if the request cannot be satisfied, or a pointer to the struct
 * netbuf structure that corresponds to the just allocated data buffer.
 */

PACKET pk_alloc(unsigned len)
{
   PACKET p;

   if (len > bigbufsiz) /* caller wants oversize buffer? */
   {
#ifdef HEAPBUFS
      if ((p = pk_alloc_heapbuf (len)) == NULL)
         return NULL;
#else
      return(NULL);
#endif
   }
   else
   {
      if ((len > lilbufsiz) || (lilfreeq.q_len == 0)) /* must use a big buffer */
         p = (PACKET)getq(&bigfreeq);
      else
         p = (PACKET)getq(&lilfreeq);

      if (!p)
         return NULL;
   }

   p->nb_prot = p->nb_buff + MaxLnh;   /* point past biggest mac header */
   p->nb_plen = 0;   /* no protocol data there yet */
   p->net = NULL;

#ifdef LINKED_PKTS
   p->pk_next = p->pk_prev = NULL;
#ifdef IP_V6
   p->ip6_hdr = NULL;      /* No "Current" IPv6 header */
   p->nexthop = NULL;      /* no next hop  */
   p->nb_pmtu = 1240;      /* Set minimum IPv6 Path MTU */
#endif   /* IP_V6 */
#endif /* LINKED_PKTS */

   p->inuse = 1;  /* initially buffer in use by 1 user */

   /* note that 'type' and 'fhost' fields are not set in pk_alloc () */
   return(p);
}

/* FUNCTION: pk_validate ()
 *
 * This function is invoked to validate the regular or heap buffer that 
 * is being returned.  If the buffer being returned is a heap buffer,
 * this function invokes pk_validate_heapbuf () to perform most of the
 * validation (of the heap buffer); otherwise, it does the validation 
 * itself.
 * 
 * One check that is common to both regular and heap buffers is a check
 * for the consistency of the next pointer and inuse count.  Two other
 * checks performed by this function for regular buffers include examining 
 * the little and big buffer free queues to determine if the packet has 
 * already been freed, and an examination of the guard bands (at the 
 * beginning and end of the data buffer).
 * 
 * INPUT: Pointer to the struct netbuf structure that corresponds to the
 * data buffer that is being freed.
 *
 * OUTPUT: 0 if the buffer being freed was successfully validated, or
 * -1 if the validation failed.
 */

int pk_validate(PACKET pkt)   /* check if pk_free() can free the pkt */
{
   PACKET   p;
#ifdef NPDEBUG
   int      j;
#endif

   /* If packet link is non-zero, then this packet is
    * part of a chain and deleted this packet would break
    * the chain and cause memory leak for subsequent pkts.
    * Note that heapbufs do not use the 'next' field at all.
    */
   if ((pkt->next) && (pkt->inuse >= 1))
   {
      INCR_SHARED_VAR (memestats, INCONSISTENT_LOCATION_ERR, 1);   
      return -1;
   }

#ifdef HEAPBUFS
   if (pkt->flags & PKF_HEAPBUF) /* check private heapbuf list queue */
   {
      return (pk_validate_heapbuf (pkt));
   }
   else  
#endif /* HEAPBUFS */
   {
      /* check if the packet is already in a freeq */
      if (pkt->nb_blen == bigbufsiz)  /* check in bigfreeq */
      {
         ENTER_CRIT_SECTION(&bigfreeq);
         for (p=(PACKET)bigfreeq.q_head; p; p = p->next)
            if (p == pkt)
            {
               dprintf("pk_free: buffer %p already in bigfreeq\n", pkt);
               EXIT_CRIT_SECTION(&bigfreeq);
               INCR_SHARED_VAR (memestats, MULTIPLE_FREE_ERR, 1);
               return -1;
            }
         EXIT_CRIT_SECTION(&bigfreeq);
      }
      else if (pkt->nb_blen == lilbufsiz)  /* check in lilfreeq */
      {
         ENTER_CRIT_SECTION(&lilfreeq);
         for (p=(PACKET)lilfreeq.q_head; p; p = p->next)
            if (p == pkt)
         {
            dprintf("pk_free: buffer %p already in lilfreeq\n", pkt);
            EXIT_CRIT_SECTION(&lilfreeq);
            INCR_SHARED_VAR (memestats, MULTIPLE_FREE_ERR, 1);
            return -1;
         }
         EXIT_CRIT_SECTION(&lilfreeq);
      }
      else
      {
         /* log an error */
         INCR_SHARED_VAR (memestats, BAD_REGULAR_BUF_LEN_ERR, 1);
         return -1;
      }
   }

#ifdef NPDEBUG
   /* check for corruption of memory markers (the guard bands are only
    * present when NPDEBUG is defined) */
   for (j = ALIGN_TYPE; j > 0; j--)
   {
      if (*(pkt->nb_buff - j) != 'M')
      {
         INCR_SHARED_VAR (memestats, GUARD_BAND_VIOLATED_ERR, 1);
         return -1;
      }
   }
   if (*(pkt->nb_buff + pkt->nb_blen) != 'M')
   {
      INCR_SHARED_VAR (memestats, GUARD_BAND_VIOLATED_ERR, 1);
      return -1;
   }
#endif /* NPDEBUG */

   return 0;
}

/* FUNCTION: pk_free ()
 *
 * This function is invoked to return a previously allocated buffer
 * or a chain of previously allocated buffers (the latter only if 
 * LINKED_PKTS is defined).
 * 
 * This function first invokes pk_validate () to validate the buffer
 * being returned.  If the validation fails, this function either
 * returns immediately (if it was processing the return of only one
 * buffer) or continues with the next buffer in the chain (if it was
 * processing a chain of buffers).
 * 
 * Each invocation of pk_free () will decrement a buffer's inuse count 
 * by 1; when the count reaches 0, the buffer is ready to be returned
 * back (to its free queue or the heap).
 *
 * After the validation is successful, this function invokes pk_free_-
 * heapbuf () if the buffer is a heap buffer.  However, if the buffer
 * being returned is a regular buffer, this function just adds the 
 * buffer back to the appropriate (little or big) buffer free queue.
 * 
 * INPUT: Pointer to the struct netbuf structure that corresponds to the
 * data buffer that is being freed.
 *
 * OUTPUT: None.
 */

void pk_free(PACKET pkt)   /* PACKET to place in free queue */
{
   int e;

#ifdef LINKED_PKTS
   /* If we support scatter/gather, then we have to loop through the
    * whole chain of packets that were passed.
    */
   while(pkt)
   {
      PACKET pknext;
      pknext = pkt->pk_next;
#endif /* LINKED_PKTS */

      /* validate the pkt before freeing */
      e = pk_validate(pkt);
      if (e)
      {
      /* pk_validate () has already recorded the type of error that has occured */
#ifdef LINKED_PKTS
         if (e == -1)
         {
            pkt = pknext;
            continue; /* skip this pkt, examine the next pkt */
         }
#endif
         return;
      }
      if (pkt->inuse-- > 1)   /* more than 1 owner? */
         return;  /* packet was cloned, don't delete yet */

#ifdef HEAPBUFS
      if (pkt->flags & PKF_HEAPBUF)
      {
         pk_free_heapbuf (pkt);
      }
      else 
#endif /* HEAPBUFS */
      {
         if (pkt->nb_blen == bigbufsiz)
            q_add(&bigfreeq, (qp)pkt);
         else if (pkt->nb_blen == lilbufsiz)
            q_add(&lilfreeq, (qp)pkt);
      }
#ifdef LINKED_PKTS
      pkt = pknext;
   }
#endif 

}

#ifdef LINKED_PKTS

PACKET
pk_prepend(PACKET pkt, int bigger)
{
   PACKET newpkt;

   LOCK_NET_RESOURCE(FREEQ_RESID);
   newpkt = pk_alloc(bigger);
   UNLOCK_NET_RESOURCE(FREEQ_RESID);
   if(!newpkt)
   {
      LOCK_NET_RESOURCE(FREEQ_RESID);
      pk_free(pkt);
      UNLOCK_NET_RESOURCE(FREEQ_RESID);
      return NULL;
   }
   newpkt->nb_prot = newpkt->nb_buff;  /* no MAC prepend */

   /* set lengths of this buffer - no data yet */
   newpkt->nb_plen = 0;       /* nothing in this buffer */
   newpkt->nb_tlen = pkt->nb_tlen;  /* total chain length unchanged */

   newpkt->pk_next = pkt;
   newpkt->pk_prev = NULL;
   /* ensure that the newly allocated buffer's PKF_HEAPBUF and 
    * PKF_INTRUNSAFE flags aren't overwritten */
   newpkt->flags |= (pkt->flags & (~(PKF_HEAPBUF | PKF_INTRUNSAFE)));
   newpkt->net = pkt->net;
   pkt->pk_prev = newpkt;     /* link new pkt */
   return(newpkt);
}

PACKET
pk_gather(PACKET pkt, int headerlen)
{
   PACKET   newpkt;
   PACKET   tmppkt;
   int      oldlen, newlen;
   char *   cp;

   oldlen = pkt->nb_tlen;

   LOCK_NET_RESOURCE(FREEQ_RESID);
   newpkt = pk_alloc(oldlen + headerlen);
   UNLOCK_NET_RESOURCE(FREEQ_RESID);
   if(!newpkt)
      return NULL;

   newpkt->nb_prot = newpkt->nb_buff + headerlen;
   /* ensure that the newly allocated buffer's PKF_HEAPBUF and 
    * PKF_INTRUNSAFE flags aren't overwritten */
   newpkt->flags |= (pkt->flags & (~(PKF_HEAPBUF | PKF_INTRUNSAFE)));
   newpkt->net = pkt->net;
   newpkt->pk_prev = newpkt->pk_next = NULL;

   tmppkt = pkt;        /* save packet for pk_free call below */
   newlen = 0;
   cp = newpkt->nb_prot;
   while(pkt)
   {
      MEMCPY(cp, pkt->nb_prot, pkt->nb_plen);
      newlen += pkt->nb_plen;
      cp += pkt->nb_plen;  /* bump pointer to data */
      pkt = pkt->pk_next;  /* next packet in chain */
   }
   LOCK_NET_RESOURCE(FREEQ_RESID);
   pk_free(tmppkt);     /* free last packet in chain */
   UNLOCK_NET_RESOURCE(FREEQ_RESID);
   if(newlen != oldlen)    /* make sure length was right */
      panic("pk_gather");

   newpkt->nb_plen = newlen;
   return newpkt;
}

#endif /* LINKED_PKTS */

/* FUNCTION: pk_get_max_intrsafe_buf_len()
 *
 * This function is invoked to obtain the length of the largest interrupt-safe
 * buffer that can be allocated in the system.
 * 
 * INPUT: None.
 *
 * OUTPUT: This function always returns the length of a big buffer (bigbufsiz).
 */

unsigned pk_get_max_intrsafe_buf_len(void)
{
   return bigbufsiz;
}

/* FUNCTION: pk_alloc_heapbuf ()
 *
 * This function is invoked by pk_alloc () to allocate a buffer from the heap.
 * Note that all struct netbuf fields are not initialized in this function; 
 * the remaining fields are initialized in pk_alloc ().
 *
 * This function first checks to make sure that the current allocation will
 * not cause either the max individual heap allocation limit (MAX_INDIVIDUAL_-
 * HEAP_ALLOC) or the total heap allocation limit (MAX_TOTAL_HEAP_ALLOC) to 
 * be exceeded.  If both checks pass, it then attempts to allocate storage 
 * for a struct netbuf, data buffer, and if the optional HEAPBUFS_DEBUG flag 
 * is defined, for a private heapbuf list element (PHLE) entry (which is used 
 * only for debug purposes).  If either of these allocations fail, this 
 * function will return 0 (signifying a failure).
 *
 * If all of the constituent allocations are successful, this function then
 * initializes various fields in the struct netbuf data structure (and the
 * PHLE entry, if HEAPBUFS_DEBUG is defined), and returns a pointer to the 
 * struct netbuf structure to its caller (which, in turn, passes it back to 
 * its caller).
 * 
 * INPUT: Length of requested allocation
 *
 * OUTPUT: 0 if the request cannot be satisfied, or a pointer to the struct
 * netbuf structure that corresponds to the just allocated data buffer.
 */

#ifdef HEAPBUFS
PACKET pk_alloc_heapbuf (unsigned len)
{
   u_long increment;
   u_char limit_exceeded = PKTALLOC_FALSE;
   PACKET p;
   u_char num_guard_bytes;
#ifdef HEAPBUFS_DEBUG
   PHLEP phlep;
#endif
#ifdef NPDEBUG
   u_char i;
   char * bufp;
#endif

   /* check to see if the caller is requesting more than the maximum 
    * allowed individual allocation */
   if (len > MAX_INDIVIDUAL_HEAP_ALLOC)
   {
      INCR_SHARED_VAR (hbufstats, TOOBIG_ALLOC_ERR, 1);
      return(NULL);
   }

#ifdef NPDEBUG
   num_guard_bytes = ALIGN_TYPE + 1;
#else
   num_guard_bytes = 0;
#endif

   /* check to make sure that this allocation will not cause us to
    * exceed the maximum total allocation allowed from the heap.  First
    * compute the increment. */
   increment = sizeof (struct netbuf) + (len + num_guard_bytes);
#ifdef HEAPBUFS_DEBUG
   /* also account for the size of the debug structure if HEAPBUFS_DEBUG
    * is enabled */
   increment += sizeof (PHLE);
#endif
   ENTER_CRIT_SECTION(&heap_curr_mem);
   heap_curr_mem += increment;
   if (heap_curr_mem > MAX_TOTAL_HEAP_ALLOC)
   {
      limit_exceeded = PKTALLOC_TRUE;
   }
   EXIT_CRIT_SECTION(&heap_curr_mem);
   if (limit_exceeded)
   {
      INCR_SHARED_VAR (hbufstats, LIMIT_EXCEEDED_ERR, 1);
      DECR_SHARED_VAR (heap_curr_mem, increment);
      return(NULL);
   }

   if (heap_type == HEAP_ACCESS_BLOCKING) UNLOCK_NET_RESOURCE (FREEQ_RESID);
      
   /* attempt to allocate a buffer for struct netbuf from the heap */
   if ((p = ((struct netbuf *) HB_ALLOC (sizeof (struct netbuf)))) == 0)
   {
      /* restore state that existed prior to call into pk_alloc () */
      if (heap_type == HEAP_ACCESS_BLOCKING) LOCK_NET_RESOURCE (FREEQ_RESID);
      INCR_SHARED_VAR (hbufstats, NB_ALLOCFAIL_ERR, 1);
      DECR_SHARED_VAR (heap_curr_mem, increment);
      return(NULL);
   }
   /* attempt to allocate data buffer from heap */
   if ((p->nb_buff = HB_ALLOC (len + num_guard_bytes)) == 0)
   {
      HB_FREE (p);
      if (heap_type == HEAP_ACCESS_BLOCKING) LOCK_NET_RESOURCE (FREEQ_RESID);
      INCR_SHARED_VAR (hbufstats, DB_ALLOCFAIL_ERR, 1);
      DECR_SHARED_VAR (heap_curr_mem, increment);
      return(NULL);
   }
#ifdef HEAPBUFS_DEBUG
   /* obtain storage for private heapbuf list element to help keep track of the heapbuf allocation */
   if ((phlep = ((PHLEP) HB_ALLOC (sizeof(PHLE)))) == 0)
   {
      HB_FREE (p->nb_buff);
      HB_FREE (p);
      if (heap_type == HEAP_ACCESS_BLOCKING) LOCK_NET_RESOURCE (FREEQ_RESID);
      INCR_SHARED_VAR (hbufstats, PHLEB_ALLOCFAIL_ERR, 1);
      DECR_SHARED_VAR (heap_curr_mem, increment);
      return(NULL);
   }
   else
   {
      phlep->netbufp = p;
      phlep->databufp = p->nb_buff;
      phlep->length = len + num_guard_bytes;
   }
#endif

   p->next = 0;
   p->nb_tstamp = 0L;
   /* mark buffer as being from heap and not interrupt-safe */
   p->flags = (PKF_HEAPBUF | PKF_INTRUNSAFE);
#ifdef NPDEBUG
   /* Add memory markers at start and end of block (to help detect memory corruption) */
   bufp = p->nb_buff;
   for (i = 0; i < ALIGN_TYPE; i++)
       *(bufp + i) = 'M';
   *(bufp + len + ALIGN_TYPE) = 'M';
   p->nb_buff += ALIGN_TYPE;   /* increment buffer's start pointer past guard band */
#endif
   p->nb_blen = len;

   if (heap_type == HEAP_ACCESS_BLOCKING) LOCK_NET_RESOURCE (FREEQ_RESID);
#ifdef HEAPBUFS_DEBUG
   /* add element describing current allocation into the private heapbuf list.  This
    * manipulation is already protected via ENTER_CRIT_SECTION () and EXIT_CRIT_SECTION
    * macros. */
   q_add(&phlq, (qp)phlep);
#endif
   /* increment the count of successfull allocations */
   INCR_SHARED_VAR (hbufstats, HB_ALLOC_SUCC, 1);

   /* update the high watermark if appropriate */
   ENTER_CRIT_SECTION(&heap_curr_mem);
   if (heap_curr_mem > heap_curr_mem_hi_watermark)
   {
      heap_curr_mem_hi_watermark = heap_curr_mem;
   }
   EXIT_CRIT_SECTION(&heap_curr_mem);

   return p;
}

/* FUNCTION: pk_validate_heapbuf ()
 *
 * This function is invoked to validate the heap buffer that is being 
 * returned.  It performs a few basic validations such as 
 *
 * - verifying that the length of the buffer is indeed greater than 
 *   bigbufsiz (since in the current design, a request for a buffer 
 *   less than bigbufsiz will always be allocated from the little or 
 *   big buffer queues),
 *
 * - checking to see if a corresponding entry exists in the private
 *   heapbuf list queue and if it does, that the length field in the
 *   PHLE entry is consistent with the buffer's own length field (note
 *   that the PHLE only exists if HEAPBUFS_DEBUG is defined), and
 *
 * - checking to ensure that the guard bands have not been overwritten.
 * 
 * INPUT: Pointer to the struct netbuf structure that corresponds to the
 * data buffer that is being validated (prior to being freed).
 *
 * OUTPUT: 0 if the buffer being freed was successfully validated, or
 * -1 if the validation failed.
 */

int pk_validate_heapbuf (PACKET pkt)
{
#ifdef HEAPBUFS_DEBUG
   PHLEP phlep;
#endif
   u_char start_offset;
   u_char num_guard_bytes;
#ifdef NPDEBUG
   int j;
#endif

#ifdef NPDEBUG
   start_offset = ALIGN_TYPE;
   num_guard_bytes = ALIGN_TYPE + 1;
#else
   start_offset = num_guard_bytes = 0;
#endif

   /* check for consistency with the nb_blen field */
   if (pkt->nb_blen <= bigbufsiz)  
   {
      INCR_SHARED_VAR (hbufstats, INCONSISTENT_HBUF_LEN_ERR, 1);
      return -1;
   }

#ifdef HEAPBUFS_DEBUG
   ENTER_CRIT_SECTION(&phlq);
   for (phlep=(PHLEP)phlq.q_head; phlep; phlep = phlep->next)
   {
       if ((phlep->netbufp == pkt) && (phlep->databufp == (pkt->nb_buff - start_offset)))
       {
          /* found a matching entry; perform consistency check */
          if (phlep->length != (pkt->nb_blen + num_guard_bytes))
          {
             EXIT_CRIT_SECTION(&phlq);
             INCR_SHARED_VAR (hbufstats, INCONSISTENT_HBUF_LEN_ERR, 1);
             return -1;
          }
          else
             break;
       }
   }
   EXIT_CRIT_SECTION(&phlq);

   if (phlep == 0)
   {
      /* since we don't have a record of this allocation in the private heapbuf, 
       * list return an error */
      INCR_SHARED_VAR (hbufstats, NO_PHLE_ERR, 1);
      return -1;
   }
#endif /* HEAPBUFS_DEBUG */

#ifdef NPDEBUG
   /* check for corruption of memory markers (the guard bands are only
    * present when NPDEBUG is defined) */
   for (j = ALIGN_TYPE; j > 0; j--)
   {
      if (*(pkt->nb_buff - j) != 'M')
      {
         INCR_SHARED_VAR (hbufstats, HB_GUARD_BAND_VIOLATED_ERR, 1);
         return -1;
      }
   }
   if (*(pkt->nb_buff + pkt->nb_blen) != 'M')
   {
      INCR_SHARED_VAR (hbufstats, HB_GUARD_BAND_VIOLATED_ERR, 1);
      return -1;
   }
#endif /* NPDEBUG */

   /* packet has passed the validation checks */
   return 0;
}

/* FUNCTION: pk_free_heapbuf ()
 *
 * This function is invoked to return a previously allocated heap 
 * buffer.  The buffer being returned has already been validated
 * (via pk_validate_heapbuf ()).
 * 
 * If HEAPBUFS_DEBUG is defined, this function first attempts to
 * locate and delete the matching PHLE entry from the PHL queue.
 * This function then returns the storage for the struct netbuf,
 * data buffer, and (if HEAPBUFS_DEBUG is defined) PHLE back to
 * the heap.  After the deallocation is complete, it updates the
 * counter that keep track of the total heap memory useage.
 * 
 * INPUT: Pointer to the struct netbuf structure that corresponds 
 * to the data buffer that is being freed.
 *
 * OUTPUT: None.  Even if this function fails to find a PHLE entry 
 * that corresponds to the buffer that is being freed, it returns 
 * silently after incrementing the corresponding error counter.
 */

void pk_free_heapbuf (PACKET pkt)
{
   u_char start_offset;
   u_char num_guard_bytes;
   char * bufp;
#ifdef HEAPBUFS_DEBUG
   PHLEP phlep;
#endif
   u_long len;
   u_long decrement;

#ifdef NPDEBUG
   start_offset = ALIGN_TYPE;
   num_guard_bytes = ALIGN_TYPE + 1;
#else
   start_offset = num_guard_bytes = 0;
#endif

   bufp = pkt->nb_buff - start_offset;
   len = pkt->nb_blen + num_guard_bytes;
#ifdef HEAPBUFS_DEBUG
   /* update private heapbuf list - remove element */
   ENTER_CRIT_SECTION(&phlq);
   for (phlep=(PHLEP)phlq.q_head; phlep; phlep = phlep->next)
   { 
      if ((phlep->netbufp == pkt) && (phlep->databufp == bufp))
      {
         /* found a matching entry; remove it... */
         break;
      }
   }
   EXIT_CRIT_SECTION(&phlq);

   /* did we find the private heapbuf list entry corresponding to this allocation? */
   if (phlep == 0)
   {
      /* No; we were able to validate this PACKET earlier, 
       * but the corresponding PHL entry has now disappeared! */
      INCR_SHARED_VAR (hbufstats, NO_PHLE_ERR, 1);
      return;
   }
   else
   {
      /* delete PHLE entry from its queue.  This deletion is protected via 
       * ENTER_CRIT_SECTION () and EXIT_CRIT_SECTION () macros. */
      qdel(&phlq, phlep);
   }
#endif /* HEAPBUFS_DEBUG */

   if (heap_type == HEAP_ACCESS_BLOCKING) UNLOCK_NET_RESOURCE (FREEQ_RESID);
   
   HB_FREE (pkt);
   HB_FREE (bufp);
#ifdef HEAPBUFS_DEBUG
   HB_FREE (phlep);
#endif /* HEAPBUFS_DEBUG */
   
   if (heap_type == HEAP_ACCESS_BLOCKING) LOCK_NET_RESOURCE (FREEQ_RESID);
   /* decrement the global counter that is used to keep track of the total
    * heap allocation */
   decrement = sizeof (struct netbuf) + len;
#ifdef HEAPBUFS_DEBUG
   /* also account for the size of the debug structure if HEAPBUFS_DEBUG
    * is enabled */
   decrement += sizeof (PHLE);
#endif
   DECR_SHARED_VAR (heap_curr_mem, decrement);

   return;
}

/* FUNCTION: dump_hbuf_stats ()
 *
 * This function prints out various error and non-error statistics
 * related to heap buffers onto the system console.
 * 
 * INPUT: Pointer to I/O structure for console.
 *
 * OUTPUT: This function always returns 0.
 */

int dump_hbuf_stats (void * pio)
{
   u_long hb_local [HBUF_NUM_STATS];
   u_long heap_curr_mem_local;
   u_long heap_curr_mem_hi_watermark_local;
#ifdef HEAPBUFS_DEBUG
   PHLEP phlep;
   u_short count = 0;
   u_long max_alloc = 0;
   u_long min_alloc = 0xFFFFFFFF;
   u_long total_alloc = 0;
#endif

   LOCK_NET_RESOURCE(FREEQ_RESID);

   ENTER_CRIT_SECTION(&hbufstats);
   MEMCPY (&hb_local, &hbufstats, sizeof(hbufstats));
   EXIT_CRIT_SECTION(&hbufstats);
   
   ENTER_CRIT_SECTION(&heap_curr_mem);
   heap_curr_mem_local = heap_curr_mem;
   heap_curr_mem_hi_watermark_local = heap_curr_mem_hi_watermark;
   EXIT_CRIT_SECTION(&heap_curr_mem);

   UNLOCK_NET_RESOURCE(FREEQ_RESID);

   ns_printf(pio, "Heap buffer error and other statistics:\n");
   ns_printf(pio, "Current total allocation %lu, high watermark %lu\n",heap_curr_mem_local,heap_curr_mem_hi_watermark_local);
   ns_printf(pio, "Successful allocations %lu, size netbuf %u, size PHLE %u\n",hb_local[HB_ALLOC_SUCC],sizeof(struct netbuf),sizeof(PHLE));
   ns_printf(pio, "Bad request size failures %lu, Max heap allocation exceeded failures %lu\n",hb_local[TOOBIG_ALLOC_ERR],hb_local[LIMIT_EXCEEDED_ERR]);
   ns_printf(pio, "netbuf allocation failures %lu, data buffer allocation failures %lu\n",hb_local[NB_ALLOCFAIL_ERR],hb_local[DB_ALLOCFAIL_ERR]);
   ns_printf(pio, "Inconsistent fields %lu, Guard band violations %lu\n",hb_local[INCONSISTENT_HBUF_LEN_ERR],hb_local[HB_GUARD_BAND_VIOLATED_ERR]);
#ifdef HEAPBUFS_DEBUG
   ns_printf(pio, "Private heapbuf list element allocation failures %lu, missing private heapbuf list element entry %lu\n",hb_local[PHLEB_ALLOCFAIL_ERR],hb_local[NO_PHLE_ERR]);
   
   LOCK_NET_RESOURCE(FREEQ_RESID);
   ENTER_CRIT_SECTION(&phlq);
   for (phlep=(PHLEP)phlq.q_head; phlep; phlep = phlep->next)
   {
      if (max_alloc < phlep->length) max_alloc = phlep->length;
      if (min_alloc > phlep->length) min_alloc = phlep->length;

      total_alloc += phlep->length;
      ++count; 
   }
   EXIT_CRIT_SECTION(&phlq);
   UNLOCK_NET_RESOURCE(FREEQ_RESID);

   if (count == 0)
      ns_printf (pio, "No heap buffers currently allocated\n");
   else
      ns_printf(pio, "Number of heap buffers currently allocated %lu, min length %lu, max length %lu, total allocation %lu\n", \
       count, min_alloc, max_alloc, total_alloc);
#endif /* HEAPBUFS_DEBUG */

   return 0;
}
#endif /* HEAPBUFS */

/* FUNCTION: dump_buf_estats ()
 *
 * This function prints out various error statistics related to regular 
 * and heap buffers onto the system console.  The first three error 
 * counters (bad_regular_buf_len, guard_band_violated, and multiple_-
 * free_err) are for regular buffers only.  The 'inconsistent_location' 
 * field is a counter for both regular and heap buffers.
 * 
 * INPUT: Pointer to I/O structure for console.
 *
 * OUTPUT: This function always returns 0.
 */

int dump_buf_estats (void * pio)
{
   u_long mlocal [MEMERR_NUM_STATS];

   LOCK_NET_RESOURCE(FREEQ_RESID);
   ENTER_CRIT_SECTION(&memestats);
   MEMCPY (&mlocal, &memestats, sizeof(memestats));
   EXIT_CRIT_SECTION(&memestats);
   UNLOCK_NET_RESOURCE(FREEQ_RESID);

   ns_printf(pio, "Regular buffer error statistics:\n");
   ns_printf(pio, "Bad buffer length %lu, Guard band violations %lu\n",mlocal[BAD_REGULAR_BUF_LEN_ERR],mlocal[GUARD_BAND_VIOLATED_ERR]);
   ns_printf(pio, "Multiple frees %lu, Inconsistent location %lu\n",mlocal[MULTIPLE_FREE_ERR],mlocal[INCONSISTENT_LOCATION_ERR]);

   return 0;
}


/* end of file pktalloc.c */


