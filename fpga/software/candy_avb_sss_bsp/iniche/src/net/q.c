/*
 * FILENAME: q.c
 *
 * Copyright  2000 By InterNiche Technologies Inc. All rights reserved
 *
 * Simple queuing system for internal use by the stack.
 * All other general-purpose queue manipulation routines are macros 
 * defined in "q.h". 10/18/90 - Added interrupt protection to all 
 * routines -JB- 9/19/93 - Modified for Browser -JB- 
 *
 * 
 * MODULE: INET
 *
 * ROUTINES: getq(), putq(), qdel(),  *
 * PORTABLE: yes
 */

/* Additional Copyrights: */
/* Portions Copyright 1993 by NetPort Software  */
/*  Copyright 1986 by Carnegie Mellon  */
/*  Copyright 1984 by the Massachusetts Institute of Technology  */


#include "ipport.h"
#include "q.h"

/* If the build is not using LOCKNET checking then define the macro away */

#ifndef LOCKNET_CHECKING
#define LOCKNET_CHECK(q)      /* define away */
#endif

/* Optional macro to dtrap if queue is corrupted. It tests that the queue 
 * head & item count are either both set to zero(NULL), or neither is zero.
 * It also checks the length of the queue with the number of elements
 * actually present.
 */

#ifdef QUEUE_CHECKING
extern void panic(char * msg);

#define QUEUE_CHECK(q) \
   {                                                    \
      q_elt temp=q->q_head;                             \
      int len=0;                                        \
      if(((q->q_head != NULL) && (q->q_len == 0)) ||    \
         ((q->q_head == NULL) && (q->q_len != 0)))      \
            panic("q");                                 \
      while (temp)                                      \
      {                                                 \
         len++;                                         \
         temp=temp->qe_next;                            \
      }                                                 \
      if (len != q->q_len)                              \
      {                                                 \
         dtrap();                                       \
      }                                                 \
   }

#else
#define QUEUE_CHECK(q)      /* define away */
#endif

/* FUNCTION: getq()
 *
 * Dequeue and return the first element of the specified queue.  
 *
 * PARAM1: queue * q
 *
 * RETURNS: pointer to the first element if any, or 0 if the queue is empty.
 */

void*
getq(queue * q)
{
   q_elt   temp;        /* temp for result */


   ENTER_CRIT_SECTION(q);     /* shut off ints, save old state */   

   LOCKNET_CHECK(q);          /* make sure queue is protected */

   if ((temp = q->q_head) == 0)  /* queue empty? */
   {
      EXIT_CRIT_SECTION(q);
      return (0);             /* yes, show none */
   }

   q->q_head = temp->qe_next; /* else unlink */
   temp->qe_next = 0;         /* avoid dangling pointers */
   if (q->q_head == 0)        /* queue empty? */
      q->q_tail = 0;          /* yes, update tail pointer too */
   q->q_len--;                /* update queue length */
   if (q->q_len < q->q_min)
      q->q_min = q->q_len;

   QUEUE_CHECK(q);         /* make sure queue is not corrupted */

   EXIT_CRIT_SECTION(q);   /* restore caller's int state */

   return ((void*)temp);
}


/* FUNCTION: putq()
 *
 * q_add(que, item) - Adds an item to tail of queue. 
 *
 * 
 * PARAM1: queue * q - queue to add item to
 * PARAM2: void * elt - item to add
 *
 * RETURNS: void
 */

void
putq(
   queue   *   q,       /* the queue */
   void *   elt)        /* element to delete */
{
   ENTER_CRIT_SECTION(q);
   LOCKNET_CHECK(q);       /* make sure queue is protected */
   q_addt(q, (qp)elt);     /* use macro to do work */
   QUEUE_CHECK(q);         /* make sure queue is not corrupted */
   EXIT_CRIT_SECTION(q);   /* restore int state */
}

/* FUNCTION: qdel()
 *
 * delete an item from the midst of a queue. Fix the head & tail and 
 * all counters. 
 *
 * 
 * PARAM1: queue * q
 * PARAM2: void * elt
 *
 * RETURNS: Return pointer to queue member if found, else NULL. 
 */

qp
qdel(queue * q, void * elt)
{
   qp qptr;
   qp qlast;

   /* search queue for element passed */
   ENTER_CRIT_SECTION(q);
   qptr = q->q_head;
   qlast = NULL;
   while (qptr)
   {
      if (qptr == (qp)elt)
      {
         /* found our item; dequeue it */
         if (qlast)
            qlast->qe_next = qptr->qe_next;
         else     /* item was at head of queqe */
            q->q_head = qptr->qe_next;

         /* fix queue tail pointer if needed */
         if (q->q_tail == (qp)elt)
            q->q_tail = qlast;

         /* fix queue counters */
         q->q_len--;
         if (q->q_len < q->q_min)
            q->q_min = q->q_len;
         EXIT_CRIT_SECTION(q);   /* restore int state */
         return (qp)elt;   /* success exit point */
      }
      qlast = qptr;
      qptr = qptr->qe_next;
   }
   EXIT_CRIT_SECTION(q);   /* restore int state */
   return NULL;   /* item not found in queue */
}


