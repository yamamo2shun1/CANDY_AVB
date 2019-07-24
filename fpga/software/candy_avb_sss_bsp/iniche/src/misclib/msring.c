/* FILENAME: msring.c
 *
 * Copyright 2002 InterNiche Technologies.  All rights reserved.
 * 
 * PORTABLE: YES
 *
 * MODULE: MISCLIB
 *
 * This file provides support for ring buffers of NicheLite M_SOCKs.
 * These are useful with NicheLite TCP server applications; the server
 * can use a ring buffer in its callback function to buffer newly-
 * received connections until it can safely add them to its work queue
 * from its normal thread context.
 *
 * The application is expected to provide two storage areas:
 * a struct msring, and an array of M_SOCKs.  The array will be 
 * used to buffer the new M_SOCKs, and the struct msring will be
 * used to manage the buffer.
 *
 * The application should call msring_init() to initialize the ring,
 * call msring_add() from its callback function to add the new M_SOCK
 * to its buffer, then call msring_del() from its normal thread 
 * context to test for the presence of a buffered M_SOCK and remove
 * it from the buffer (returning it to the application).
 *
 * Note that the array of M_SOCKs should be sized to hold one more than
 * the maximum number of expected M_SOCKs.  If the ring is full when
 * the callback function calls msring_add(), the new M_SOCK will be
 * dropped!
 *
 * Implementation details:
 *
 * msring.in is the index to where the next M_SOCK added should be stored.
 * msring.out is the index from where the next M_SOCK deleted should be
 * deleted.
 * Hence, msring.in == msring.out is the "ring empty" state, and
 * (msring.in + 1) % msring.buflen == msring.out is the "ring full"
 * state.
 *
 */

#include "ipport.h"
#include "tcpapp.h"

#ifdef MINI_TCP

#include "msring.h"

/* FUNCTION: msring_init()
 *
 * Initializes an M_SOCK ring to an empty state.
 * 
 * PARAM1: struct msring * ring; a pointer to the struct msring that
 *                               is to be initialized
 * PARAM2: M_SOCK * buf; a pointer to an array of M_SOCKs that is
 *                       to be used to store the M_SOCKs in the ring
 * PARAM3: unsigned bufsize; the size (in M_SOCKs) of buf
 *
 * RETURNS: void
 */
void
msring_init(struct msring * ring,
            M_SOCK * buf,
            unsigned buflen)
{
   ring->in = ring->out = 0;
   ring->buf = buf;
   ring->buflen = buflen;
}

/* FUNCTION: msring_add()
 *
 * Adds an M_SOCK to an M_SOCK ring.
 * 
 * PARAM1: struct msring * ring; a pointer to the struct msring to which
 *                               the M_SOCK is to be added
 * PARAM2: M_SOCK so; the M_SOCK that is to be added to the ring
 *
 * RETURNS: void
 */
void
msring_add(struct msring * ring,
           M_SOCK so)
{
   if (((ring->in + 1) & ring->buflen) == ring->out)
   {
      /* the ring is full, and we are going to drop this socket */
      dtrap();
      return;
   }

   /* add the socket to the ring */
   ring->buf[ring->in] = so;
   if (ring->in + 1 < ring->buflen)
      ring->in = ring->in + 1;
   else
      ring->in = 0;
}

/* FUNCTION: msring_del()
 *
 * Removes an M_SOCK from an M_SOCK ring, returning it to the caller.
 * 
 * PARAM1: struct msring * ring; a pointer to the struct msring to which
 *                               the M_SOCK is to be added
 * PARAM2: M_SOCK * so; a pointer to an M_SOCK where the removed M_SOCK
 *                      can be returned to the caller
 *
 * RETURNS: an integer indicating the success or failure of the call.
 *          0 indicates success, with an M_SOCK returned in so.
 *          -1 indicates failure, typically that there are no more
 *          M_SOCKs in the ring.
 */
int
msring_del(struct msring * ring,
           M_SOCK * so)
{
   /* if ring empty, return failure */
   if (ring->in == ring->out)
      return -1;
   
   /* get one socket from the ring and return success */
   *so = ring->buf[ring->out];
   if (ring->out + 1 < ring->buflen)
      ring->out = ring->out + 1;
   else
      ring->out = 0;
   return 0;
}

#endif  /* MINI_TCP */
