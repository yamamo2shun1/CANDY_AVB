/*
 * FILENAME: tcp_timr.c
 *
 * Copyright 1997-2007 By InterNiche Technologies, Inc. All rights reserved.
 *
 *
 * MODULE: INET
 *
 * ROUTINES: tcp_fasttimo(), tcp_slowtimo(), tcp_canceltimers(), 
 * ROUTINES: tcp_timers(),
 *
 * PORTABLE: yes
 */

/* Additional Copyrights: */
/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. [rescinded 22 July 1999]
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "ipport.h"
#include "tcpport.h"

#ifdef INCLUDE_TCP  /* include/exclude whole file at compile time */

#include "protosw.h"
#include "in_pcb.h"
#include "ip.h"
#include "tcp.h"
#include "tcp_fsm.h"
#include "tcp_seq.h"
#include "tcp_timr.h"
#include "tcp_var.h"
#include "tcpip.h"


int      tcp_keepidle   =  TCPTV_KEEP_IDLE;
int      tcp_keepintvl  =  TCPTV_KEEPINTVL;
int      tcp_maxidle;

#ifdef DO_DELAY_ACKS

/* FUNCTION: tcp_fasttimo()
 *
 * Fast timeout routine for processing delayed acks. This should be
 * called once per ctick to get the delayed ACK timing correct.
 * 
 * PARAM1: none.
 *
 * RETURNS: none.
 */

void
tcp_fasttimo(void)
{
   struct inpcb * inp;
   struct tcpcb * tp;

   inp = tcb.inp_next;
   if (inp)
      for (; inp != &tcb; inp = inp->inp_next)
   {
      if ((tp = (struct tcpcb *)inp->inp_ppcb) &&
          (tp->t_flags & TF_DELACK)) 
      {
         if(--tp->t_delackcur <= 0)
         {
            tp->t_delackcur = tp->t_delacktime;
            tp->t_flags &= ~TF_DELACK;
            tp->t_flags |= TF_ACKNOW;
            tcpstat.tcps_delack++;
            (void) tcp_output(tp);
         }
      }
   }
}
#endif   /* DO_DELAY_ACKS */



/* FUNCTION: tcp_slowtimo()
 *
 * Tcp protocol timeout routine called every 500 ms.
 * Updates the timers in all active tcb's and
 * causes finite state machine actions if timers expire.
 *
 * 
 * PARAM1: 
 *
 * RETURNS: 
 */

void
tcp_slowtimo(void)
{
   struct inpcb * ip, * ipnxt;
   struct tcpcb * tp;
   int   i;
   struct socket * so, * sonext;
   struct sockbuf *  sb;

   tcp_maxidle = TCPTV_KEEPCNT * tcp_keepintvl;

   /* search through open sockets */
   for (so = (struct socket *)soq.q_head; so != NULL; so = sonext)
   {
      sonext = so->next;

      /* for SOCK_STREAM (TCP) sockets, we must do slow-timeout 
       * processing and (optionally) processing of pending 
       * zero-copy socket upcalls.
       */
      if (so->so_type == SOCK_STREAM)
      {
         ip = so->so_pcb;
         if (!ip)
            continue;
         ipnxt = ip->inp_next;

         tp = intotcpcb(so->so_pcb);
         if (!tp)
            continue;

         for (i = 0; i < TCPT_NTIMERS; i++) 
         {
            if (tp->t_timer[i] && --tp->t_timer[i] == 0) 
            {
               /* call usrreq to do actual work */
               so->so_req = PRU_SLOWTIMO;
               (void) tcp_usrreq(so, (struct mbuf *)0, 
                                 LONG2MBUF((long)i));

               /* If ip disappeared on us, handle it */
               if (ipnxt->inp_prev != ip)
                  goto tpgone;
            }
         }
      
#ifdef TCP_ZEROCOPY
         /* also nudge sockets which may have missed an upcall. We have
          * to test for a variety of problems, as ending up here usually
          * means the system doesn't have enough buffers to be doing this
          * well.
          */
         if (so->rx_upcall)         /* If call back set... */
         {
            u_int ready = so->so_rcv.sb_cc;

            /* If socket has data, try to deliver it to app */
            if (ready > 0)
            {
               tcp_data_upcall(so);
               if (ready != so->so_rcv.sb_cc)   /* did app accept any data? */
                  tcp_output(tp);   /* may need to push out a Window update */
            }

            /* else, no current data... */
            else
            {
               /* if the connection is shutting down, but the application
                * hasn't been informed, do so now
                */
               if ((tp->t_state > TCPS_ESTABLISHED) &&
                   ((so->so_state & SS_UPCFIN) == 0))
               {
                  so->rx_upcall(so, NULL, ESHUTDOWN);
                  so->so_state |= SS_UPCFIN;
               }
            }
         }
#endif   /* TCP_ZEROCOPY */

         tp->t_idle++;
      }

      /* wake up anyone sleeping in a select() involving this socket */
      sb = &so->so_rcv;
      if (sb->sb_flags & SB_SEL) 
      {
         select_wait = 0;
#ifndef SOCK_MAP_EVENTS
         tcp_wakeup ((char *)&select_wait);
#else
         tcp_wakeup2 (so->owner);
#endif
         sb->sb_flags &= ~SB_SEL;
      }
      sb = &so->so_snd;
      if (sb->sb_flags & SB_SEL) 
      {
         select_wait = 0;         
#ifndef SOCK_MAP_EVENTS
         tcp_wakeup ((char *)&select_wait);
#else
         tcp_wakeup2 (so->owner);
#endif
         sb->sb_flags &= ~SB_SEL;
      }

      /* wake any thread with a timer going for a connection state change */     
      tcp_wakeup((char*)&so->so_timeo);

tpgone:
      ;
   }

   tcp_iss += (unsigned)(TCP_ISSINCR/PR_SLOWHZ);      /* increment iss */

   if (tcp_iss & 0xff000000)
      tcp_iss = 0L;
}


/* FUNCTION: tcp_canceltimers()
 *
 * Cancel all timers for TCP tp.
 * 
 * PARAM1: struct tcpcb *tp
 *
 * RETURNS: 
 */

void
tcp_canceltimers(struct tcpcb * tp)
{
   int   i;

   for (i = 0; i < TCPT_NTIMERS; i++)
      tp->t_timer[i] = 0;
}

unsigned char tcp_backoff [TCP_MAXRXTSHIFT + 1] =
{ 1, 2, 4, 8, 16, 32, 64, 64, 64, 64, 64, 64, 64 };



/* FUNCTION: tcp_timers()
 *
 * TCP timer processing.
 * 
 * PARAM1: struct tcpcb *tp
 * PARAM2: int timer
 *
 * RETURNS: 
 */

struct tcpcb * 
tcp_timers(struct tcpcb * tp, int timer)
{
   int   rexmt;

   switch (timer) 
   {

   /*
    * 2 MSL timeout in shutdown went off.  If we're closed but
    * still waiting for peer to close and connection has been idle
    * too long, or if 2MSL time is up from TIME_WAIT, delete connection
    * control block.  Otherwise, check again in a bit.
    */
   case TCPT_2MSL:
      if (tp->t_state != TCPS_TIME_WAIT &&
          tp->t_idle <= tcp_maxidle)
      {
         tp->t_timer[TCPT_2MSL] = (short)tcp_keepintvl;
      }
      else
         tp = tcp_close(tp);
      break;

   /*
    * Retransmission timer went off.  Message has not
    * been acked within retransmit interval.  Back off
    * to a longer retransmit interval and retransmit one segment.
    */
   case TCPT_REXMT:
      TCP_MIB_INC(tcpRetransSegs);     /* keep MIB stats */
      if (++tp->t_rxtshift > TCP_MAXRXTSHIFT) 
      {
         tp->t_rxtshift = TCP_MAXRXTSHIFT;
         tcpstat.tcps_timeoutdrop++;
         tp = tcp_drop(tp, ETIMEDOUT);
         break;
      }
      tcpstat.tcps_rexmttimeo++;
      rexmt = ((tp->t_srtt >> 2) + tp->t_rttvar) >> 1;
      rexmt *= tcp_backoff[tp->t_rxtshift];
      TCPT_RANGESET(tp->t_rxtcur, rexmt, TCPTV_MIN, TCPTV_REXMTMAX);
      tp->t_timer[TCPT_REXMT] = tp->t_rxtcur;
      /*
       * If losing, let the lower level know and try for
       * a better route.  Also, if we backed off this far,
       * our srtt estimate is probably bogus.  Clobber it
       * so we'll take the next rtt measurement as our srtt;
       * move the current srtt into rttvar to keep the current
       * retransmit times until then. Don't clobber with rtt
       * if we got it from a timestamp option.
       */
      if((tp->t_rxtshift > TCP_MAXRXTSHIFT / 4) &&
         ((tp->t_flags & TF_TIMESTAMP) == 0))
      {
         tp->t_rttvar += (tp->t_srtt >> 2);
         tp->t_srtt = 0;
      }
      tp->snd_nxt = tp->snd_una;
      /*
       * If timing a segment in this window, stop the timer.
       */
      tp->t_rttick = 0;
      /*
       * Close the congestion window down to one segment
       * (we'll open it by one segment for each ack we get).
       * Since we probably have a window's worth of unacked
       * data accumulated, this "slow start" keeps us from
       * dumping all that data as back-to-back packets (which
       * might overwhelm an intermediate gateway).
       *
       * There are two phases to the opening: Initially we
       * open by one mss on each ack.  This makes the window
       * size increase exponentially with time.  If the
       * window is larger than the path can handle, this
       * exponential growth results in dropped packet(s)
       * almost immediately.  To get more time between 
       * drops but still "push" the network to take advantage
       * of improving conditions, we switch from exponential
       * to linear window opening at some threshhold size.
       * For a threshhold, we use half the current window
       * size, truncated to a multiple of the mss.
       *
       * (the minimum cwnd that will give us exponential
       * growth is 2 mss.  We don't allow the threshhold
       * to go below this.)
       *
       * Vers 1.9 - Skip slow start if the SO_NOSLOWSTART socket option
       * is set.
       */
      if((tp->t_inpcb->inp_socket->so_options & SO_NOSLOWSTART) == 0)
      {
         u_int win = MIN(tp->snd_wnd, tp->snd_cwnd);
         win = win / 2 / tp->t_maxseg;
         if (win < 2)
            win = 2;
         tp->snd_cwnd = tp->t_maxseg;
         tp->snd_ssthresh = (u_short)win * tp->t_maxseg;
      }
      (void) tcp_output(tp);
      break;

   /*
    * Persistance timer into zero window.
    * Force a byte to be output, if possible.
    */
   case TCPT_PERSIST:
      tcpstat.tcps_persisttimeo++;
      tcp_setpersist(tp);
      tp->t_force = 1;
      (void) tcp_output(tp);
      tp->t_force = 0;
      break;

   /*
    * Keep-alive timer went off; send something
    * or drop connection if idle for too long.
    */
   case TCPT_KEEP:
      tcpstat.tcps_keeptimeo++;
      if (tp->t_state < TCPS_ESTABLISHED)
         goto dropit;
      if (tp->t_inpcb->inp_socket->so_options & SO_KEEPALIVE &&
          tp->t_state <= TCPS_CLOSE_WAIT) 
      {
         if (tp->t_idle >= tcp_keepidle + tcp_maxidle)
            goto dropit;
         /*
          * Send a packet designed to force a response
          * if the peer is up and reachable:
          * either an ACK if the connection is still alive,
          * or an RST if the peer has closed the connection
          * due to timeout or reboot.
          * Using sequence number tp->snd_una-1
          * causes the transmitted zero-length segment
          * to lie outside the receive window;
          * by the protocol spec, this requires the
          * correspondent TCP to respond.
          */
         tcpstat.tcps_keepprobe++;

         /*
          * The keepalive packet must have nonzero length
          * to get a 4.2 host to respond.
          */
         tcp_respond(tp, tp->t_template, tp->rcv_nxt - 1,
            tp->snd_una - 1, 0, (struct mbuf *)NULL);

         tp->t_timer[TCPT_KEEP] = (short)tcp_keepintvl;
      }
      else
         tp->t_timer[TCPT_KEEP] = (short)tcp_keepidle;
      break;
      dropit:
      tcpstat.tcps_keepdrops++;
      tp = tcp_drop (tp, ETIMEDOUT);
      break;
   }
   return tp;
}
tcp_seq tcp_iss;

#endif /* INCLUDE_TCP */


/* end of file tcp_timr.c */

