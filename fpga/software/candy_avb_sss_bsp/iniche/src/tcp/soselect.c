/*
 * FILENAME: soselect.c
 *
 * Copyright 1997- 2000 By InterNiche Technologies Inc. All rights reserved
 *
 * soselect.c Sockets library select() support. This will probably 
 * need to be modified on a per-port basis. If your sockets code does 
 * not rely on select() calls, then you can omit this file form the 
 * build. 
 *
 *
 * MODULE: INET
 *
 * ROUTINES: t_select(), sock_selscan(), sock_select(), 
 * ROUTINES: in_pcbnotify(), void (), tcp_notify(), FD_CLR(), FD_SET(), 
 * ROUTINES: FD_ISSET(),
 *
 * PORTABLE: yes
 */

#include "ipport.h"
#include "tcpport.h"

#ifdef INCLUDE_TCP  /* include/exclude whole file at compile time */
#ifdef SO_SELECT  /* whole file can be ifdeffed out */

#include "protosw.h"
#include "in_pcb.h"

/* Select routines in this file: */
int sock_selscan(fd_set * ibits, fd_set * obits);
int sock_select(long sock, int flags);

#define  SOREAD      1
#define  SOWRITE     2


/* FUNCTION: t_select()
 *
 * t_select() - implement a UNIX-like socket select call. Causes the 
 * calling process to block waiting for activity on any of a list of 
 * sockets. Arrays of socket descriptions are passed for read, write, 
 * and exception event. Any of these may be NULL if the event is not 
 * of interest. A timeout is also passed, which is given in cticks 
 * (TPS ticks per second). Returns the number of sockets which had an 
 * event occur. 
 *
 * 
 * PARAM1: fd_set *in
 * PARAM2: fd_set *out
 * PARAM3: fd_set *ex
 * PARAM4: long tv
 *
 * RETURNS: 
 */

int
t_select(fd_set * in,   /* lists of sockets to watch */
   fd_set * out,
   fd_set * ex,
   long  tv)   /* ticks to wait */
{
   fd_set obits[3], ibits [3];
   u_long   tmo;
   int   retval   =  0;

   MEMSET(&obits, 0, sizeof(obits));
   MEMSET(&ibits, 0, sizeof(ibits));

   if (in)
      MEMCPY(&ibits[0], in, sizeof(fd_set));
   if (out)
      MEMCPY(&ibits[1], out, sizeof(fd_set));
   if (ex)
      MEMCPY(&ibits[2], ex, sizeof(fd_set));
   tmo = cticks + tv;

   /* if all the fd_sets are empty, just block;  else do a real select() */
   if ((ibits[0].fd_count == 0) && (ibits[1].fd_count == 0) &&
       (ibits[2].fd_count == 0))
   {
      if (tv > 0)      /* make sure we don't block on nothing forever */
      {
#ifdef SUPERLOOP
         while (tmo > cticks)
         {
            tk_yield();
         }
#else
         TK_SLEEP(tv);
#endif
      }
   }
   else
   {
#ifdef SOC_CHECK_ALWAYS
      int i, j;
      struct socket * so;

      for (i = 0; i < 3; i++)
      {
         for (j = 0; j < ibits[i].fd_count; j++)
         {
            so = LONG2SO(ibits[i].fd_array[j]);
            SOC_CHECK(so);
         }
      }
#endif /* SOC_CHECK_ALWAYS */

      /* Lock the net semaphore before going into selscan. Upon
       * return we will either call tcp_sleep(), which unlocks the
       * semaphore, or fall into the unlock statement.
       */
      LOCK_NET_RESOURCE(NET_RESID);
      while ((retval = sock_selscan(ibits, obits)) == 0)
      {
         if (tv != -1L) 
         {
            if (tmo <= cticks)
               break;
         }
         select_wait = 1;
         tcp_sleep (&select_wait);
      }
      UNLOCK_NET_RESOURCE(NET_RESID);

   }

   if (retval >= 0)
   {
      if (in)
         MEMCPY(in, &obits[0], sizeof(fd_set));
      if (out)
         MEMCPY(out, &obits[1], sizeof(fd_set));
      if (ex)
         MEMCPY(ex, &obits[2], sizeof(fd_set));
   }
   return retval;
}


/* FUNCTION: sock_selscan()
 *
 * sock_selscan() - internal non-blocking routine under t_select(). 
 * This scans the two FD lists passed for activity by calling the 
 * subroutine sock_select() on a per-socket basis. 
 *
 * 
 * PARAM1: fd_set * ibits
 * PARAM2: fd_set * obits
 *
 * RETURNS: 
 */

int
sock_selscan(fd_set * ibits, fd_set * obits)
{
   fd_set *in, *out;
   int   which;
   int   sock;
   int   flag  =  0;
   int   num_sel  =  0;

   for (which = 0; which < 3; which++)
   {
      switch (which)
      {
      case 0:
         flag = SOREAD; break;

      case 1:
         flag = SOWRITE; break;

      case 2:
         flag = 0; break;
      }
      in = &ibits [which];
      out = &obits [which];
      for (sock = 0; sock < (int)in->fd_count; sock++)
      {
         if (sock_select (in->fd_array[sock], flag))
         {
            FD_SET(in->fd_array[sock], out);
            num_sel++;
         }
      }
   }
   return num_sel;
}


/* FUNCTION: sock_select()
 *
 * sock_select - handle the select logic for a single event on a 
 * single socket. This is called multiple times from sock_selscan 
 * above. Retuns 1 if socket is ready for event, 0 if not. 
 *
 * 
 * PARAM1: long sock
 * PARAM2: int flag
 *
 * RETURNS: 
 */

int
sock_select(long sock, int flag)
{
   struct socket *   so;
   int   ready =  0;

   so = LONG2SO(sock);

   switch (flag) 
   {
   case SOREAD:
      /* can we read something from so? */
      if (so->so_rcv.sb_cc)
      {
         ready = 1;
         break;
      }
      if (so->so_state & SS_CANTRCVMORE)
      {  ready = 1;
         break;
      }
      if (so->so_qlen)  /* attach is ready */
      {
         ready = 1;
         break;
      }

#ifdef TCP_ZEROCOPY
      /* zerocopy sockets with a callback set should wbe awakened
      if there is pending data which has been upcalled */
      if ((so->rx_upcall) && (so->so_state & SS_UPCALLED))
      {
         so->so_state &= ~SS_UPCALLED; /* clear flag */
         {
            ready = 1;
            break;
         }
      }
#endif   /* TCP_ZEROCOPY */

      /* fall to here if so is not ready to read */
      so->so_rcv.sb_flags |= SB_SEL;   /* set flag for select wakeup */
      break;

   case SOWRITE:
      if ((sbspace(&(so)->so_snd) > 0) && 
          ((((so)->so_state&SS_ISCONNECTED) || 
            ((so)->so_proto->pr_flags&PR_CONNREQUIRED)==0) || 
           ((so)->so_state & SS_CANTSENDMORE)))
      {
         ready = 1;
         break;
      }
      sbselqueue (&so->so_snd);
      break;

   case 0:
      if (so->so_oobmark || (so->so_state & SS_RCVATMARK))
      {
         ready = 1;
         break;
      }
      if (so->so_error &&
          (so->so_error != EINPROGRESS) &&
          (so->so_error != EWOULDBLOCK))
      {
         ready = 1;
         break;
      }
      sbselqueue(&so->so_rcv);
      break;
   }

   return ready;
}


/* FUNCTION: in_pcbnotify()
 *
 * Pass some notification to all connections of a protocol
 * associated with address dst.  Call the protocol specific
 * routine (if any) to handle each connection.
 *
 * 
 * PARAM1: struct inpcb *head
 * PARAM2: struct in_addr *dst
 * PARAM3: int errnum
 * PARAM4: void (*notify
 *
 * RETURNS: 
 */

void
in_pcbnotify(struct inpcb * head, 
   struct in_addr *  dst,
   int   errnum,
   void (*notify) __P ((struct inpcb *)))
{
   struct inpcb * inp, *   oinp;

   for (inp = head->inp_next; inp != head;) 
   {
      if (inp->inp_faddr.s_addr != dst->s_addr ||
          inp->inp_socket == 0) 
      {
         inp = inp->inp_next;
         continue;
      }
      if (errnum) 
         inp->inp_socket->so_error = errnum;
      oinp = inp;
      inp = inp->inp_next;
      if (notify)
         (*notify)(oinp);
   }
}



/* FUNCTION: tcp_notify()
 * 
 * Notify a tcp user of an asynchronous error;
 * just wake up so that he can collect error status.
 *
 * 
 * PARAM1: struct inpcb *inp
 *
 * RETURNS: 
 */

void
tcp_notify(struct inpcb * inp)
{
   tcp_wakeup(&inp->inp_socket->so_timeo);
   sorwakeup(inp->inp_socket);
   sowwakeup(inp->inp_socket);
}


/* the next three routines are derived from FD_XXX() macros, which is how they 
are traditionally implemented. */


/* FUNCTION: ifd_clr()
 * 
 * PARAM1: long               socket
 * PARAM2: fd_set *           file descriptor set
 *
 * RETURNS: none
 *
 * Removes the socket from the file descriptor set, and
 * compacts the fd_set.
 */

void
ifd_clr(long sock, fd_set *set)
{
   u_int i;

   for (i = 0; i < set->fd_count ; i++) 
   {
      if (set->fd_array[i] == sock)
      {
         while (i + 1 < set->fd_count)
         {
            set->fd_array[i] = set->fd_array[i + 1];
            i++;
         }
         set->fd_count--;
         return;
      }
   }

#ifdef NPDEBUG
   dtrap(); /* socket wasn't found in array */
#endif
}



/* FUNCTION: ifd_set()
 * 
 * PARAM1: long               socket
 * PARAM2: fd_set *           file descriptor set
 *
 * RETURNS: none
 *
 * Adds the socket to the file descriptor set. dtrap() is
 * called if the fd_set structure is already full.
 */

void
ifd_set(long sock, fd_set *set)
{
   if (set->fd_count < FD_SETSIZE)
      set->fd_array[set->fd_count++] = sock;
#ifdef NPDEBUG
   else
      dtrap();
#endif
}



/* FUNCTION: ifd_isset()
 * 
 * PARAM1: long               socket
 * PARAM2: fd_set *           file descriptor set
 *
 * RETURNS: int               TRUE if socket is a member of the fd_set
 *
 * Tests if a socket is a member of a file descriptor set.
 */

int   /* actually, boolean */
ifd_isset(long sock, fd_set *set)
{
   u_int   i;

   for (i = 0; i < set->fd_count ; i++)
   {
      if (set->fd_array[i] == sock)
         return TRUE;
   }
   return FALSE;
}



/* FUNCTION: ifd_get()
 * 
 * PARAM1: unsigned int       file descriptor set index
 * PARAM2: fd_set *           file descriptor set
 *
 * RETURNS: long              socket or INVALID_SOCKET
 *
 * If index is less than the number of elements in the fd_set array,
 * returns the index_th element, otherwise returns INVALID_SOCKET.
 *
 * NOTE: This is not part of the original FD_XXX() functionality.
 */

long
ifd_get(unsigned i, fd_set *set)
{
   if (i < set->fd_count)
      return set->fd_array[i];
   else
   {
#ifdef NPDEBUG
      dtrap();
#endif
      return INVALID_SOCKET;
   }
}

#endif   /* SO_SELECT */
#endif /* INCLUDE_TCP */

/* end of file socket.c */

