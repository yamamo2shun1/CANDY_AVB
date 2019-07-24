/*
 * FILENAME: tcp_in.c
 *
 * Copyright 1997-2008 By InterNiche Technologies, Inc. All rights reserved.
 *
 *
 * MODULE: TCP
 *
 * ROUTINES: tcp_reass(), tcp_input(), tcp_dooptions(), 
 * ROUTINES: tcp_pulloutofband(), tcp_xmit_timer(), tcp_mss(), 
 * ROUTINES: in_localaddr(),
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

#include "in_pcb.h"
#include "ip.h"
#include "tcp.h"
#include "tcp_fsm.h"
#include "tcp_seq.h"
#include "tcp_timr.h"
#include "tcp_var.h"
#include "tcpip.h"

#ifdef IP_V6
#include "socket6.h"
#endif   /* IP_V6 */

#define  TRACE_RESET 1
#ifdef TRACE_RESET  /* this can be usefull for tracing tcp_input() logic: */
int      dropline;
#define  GOTO_DROP   {  dropline=__LINE__;   goto  drop; }
#define  GOTO_DROPWITHRESET   {  dropline=__LINE__;   goto  dropwithreset; }
#else /* the production flavor: */
#define  GOTO_DROP   {  goto  drop; }
#define  GOTO_DROPWITHRESET   {  goto  dropwithreset; }
#endif

char     tcpprintfs  =  0;
char     tcprexmtthresh =  3;
struct   tcpiphdr tcp_saveti;


int   tcp_reass   __P ((struct tcpcb *, struct tcpiphdr *, struct mbuf *));
void  tcp_xmit_timer(struct tcpcb * tp);
extern   void  tcp_pulloutofband __P ((struct socket *, struct tcpiphdr *, 
            struct mbuf *));
extern   void  tcp_dooptions  __P ((struct tcpcb *, struct mbuf *, 
            struct tcpiphdr *));
#ifdef IP_V6
int ip6_tcpmss(struct socket * so, NET ifp);
#endif

/* FUNCTION: tcp_reass()
 * 
 * Insert segment ti into reassembly queue of tcp with
 * control block tp.  tcp_input() does the common case inline
 * (segment is the next to be received on an established connection,
 * and the queue is empty), avoiding linkage into and removal
 * from the queue and repetition of various conversions.
 * Set DELACK for segments received in order, but ack immediately
 * when segments are out of order (so fast retransmit can work).
 *
 * PARAM1: struct tcpcb *tp
 * PARAM2: struct tcpiphdr *ti
 * PARAM3: struct mbuf *ti_mbuf
 *
 * RETURNS:  Return TH_FIN if reassembly now includes a segment with FIN.
 */

int
tcp_reass(struct tcpcb * tp, 
   struct tcpiphdr * ti,
   struct mbuf *  ti_mbuf)
{
   struct tcpiphdr * q;
   struct socket *   so =  tp->t_inpcb->inp_socket;
   struct mbuf *  m;
   int   flags;

   /*
    * Call with ti==0 after become established to
    * force pre-ESTABLISHED data up to user socket.
    */
   if (ti == 0)
      goto present;

   /*
    * Find a segment which begins after this one does.
    */
   for (q = tp->seg_next; q != (struct tcpiphdr *)tp;
       q = (struct tcpiphdr *)q->ti_next)
   {
      if (SEQ_GT(q->ti_seq, ti->ti_seq))
      break;
   }

   /*
    * If there is a preceding segment, it may provide some of
    * our data already.  If so, drop the data from the incoming
    * segment.  If it provides all of our data, drop us.
    */
   if ((struct tcpiphdr *)q->ti_prev != (struct tcpiphdr *)tp) 
   {
      long  i;
      q = (struct tcpiphdr *)q->ti_prev;
      /* conversion to int (in i) handles seq wraparound */
      i = q->ti_seq + q->ti_len - ti->ti_seq;
      if (i > 0) 
      {
         if (i >= (long)ti->ti_len) 
         {
            tcpstat.tcps_rcvduppack++;
            tcpstat.tcps_rcvdupbyte += ti->ti_len;
            GOTO_DROP;
         }
         m_adj (ti_mbuf, (int)i);
         ti->ti_len -= (short)i;
         ti->ti_seq += (tcp_seq)i;
      }
      q = (struct tcpiphdr *)(q->ti_next);
   }
   tcpstat.tcps_rcvoopack++;
   tcpstat.tcps_rcvoobyte += ti->ti_len;

   /*
    * While we overlap succeeding segments trim them or,
    * if they are completely covered, dequeue them.
    */
   while (q != (struct tcpiphdr *)tp) 
   {
      int   i  =  (int)((ti->ti_seq +  ti->ti_len) -  q->ti_seq);
      if (i <= 0)
         break;
      if (i < (int)(q->ti_len))
      {
         q->ti_seq += i;
         q->ti_len -= (u_short)i;
         m_adj (dtom(q), (int)i);
         break;
      }
      q = (struct tcpiphdr *)q->ti_next;
      m = dtom(q->ti_prev);
      remque (q->ti_prev);
      m_freem (m);
   }

   /*
    * Stick new segment in its place.
    */
   insque(ti, q->ti_prev);

present:
   /*
    * Present data to user, advancing rcv_nxt through
    * completed sequence space.
    */
   if (TCPS_HAVERCVDSYN (tp->t_state) == 0)
      return (0);
   ti = tp->seg_next;
   if (ti == (struct tcpiphdr *)tp || ti->ti_seq != tp->rcv_nxt)
      return (0);
   if (tp->t_state == TCPS_SYN_RECEIVED && ti->ti_len)
      return (0);
   do 
   {
      tp->rcv_nxt += ti->ti_len;
      flags = ti->ti_flags & TH_FIN;
      remque(ti);
      m = dtom(ti);
      ti = (struct tcpiphdr *)ti->ti_next;
      if (so->so_state & SS_CANTRCVMORE)
         m_freem (m);
      else
         sbappend (&so->so_rcv, m);
   } while (ti != (struct tcpiphdr *)tp && ti->ti_seq == tp->rcv_nxt);
      sorwakeup(so);
   return (flags);
drop:
   /**m_freem (dtom(ti));**/
   m_freem (ti_mbuf);
   return (0);
}


/* FUNCTION: tcp_input()
 *
 * TCP input routine, follows pages 65-76 of the
 * protocol specification dated September, 1981 very closely.
 *
 * 
 * PARAM1: struct mbuf * m
 * PARAM2: NET ifp - iface packet was received on
 *
 * RETURNS: void
 */

void
tcp_input(struct mbuf * m, NET ifp)
{
#ifdef IP_V4
   struct ip * pip;
   struct in_addr laddr;
#endif   /* IP_V4 */
#ifdef IP_V6
   /* Storage for received packet's IPv6 addresses on stack. */
   ip6_addr    ip6_src;
   ip6_addr    ip6_dst;
#endif   /* IP_V6 */
   struct tcpiphdr * ti;
   struct inpcb * inp;
   struct mbuf *  om =  0;
   int   len,  tlen, off;
   struct tcpcb * tp =  0;
   int   tiflags;
   struct socket *   so =  NULL;
   int   todrop,  acked,   ourfinisacked, needoutput  =  0;
   int   dropsocket  =  0;
   long  iss   =  0;
   tcp_win  rx_win;     /* scaled window from received segment */

#ifdef DO_TCPTRACE
   int   ostate;
#endif


   tcpstat.tcps_rcvtotal++;
   TCP_MIB_INC(tcpInSegs);    /* keep MIB stats */

#ifdef IP_V4
#ifdef IP_V6     /* skip test on v4  only builds */
   if(m->pkt->type == IPTP)   /* IPv4 packet */
#endif
   {
      /*
       * Get IP and TCP header together in first mbuf.
       * Note: IP leaves IP header in first mbuf.
       */
      pip = mtod(m, struct ip *);
      if (pip->ip_ver_ihl > 0x45)   /* IP v4, 5 dword hdr len */
      {
         np_stripoptions(pip, (struct mbuf *)m);
         pip = mtod(m, struct ip *);
      }
      if (m->m_len < ((sizeof (struct ip) + sizeof (struct tcphdr))))
      {
         tcpstat.tcps_rcvshort++;
         return;
      }
      tlen = pip->ip_len;     /* this was fudged by IP layer */

      /* The following is needed in the cases where the size of the
       * overlay structure is larger than the size of the ip header.
       * This can happen if the ih_next and ih_prev pointers in the
       * overlay structure are larger than 32 bit pointers.
       */
      ti = (struct tcpiphdr *)(m->m_data + sizeof(struct ip) - 
         sizeof(struct ipovly));
      if ((char *)ti < m->pkt->nb_buff)
      {
         panic("tcp_input");
      }
   }
#endif   /* IP_V4 */
#ifdef IP_V6
#ifdef IP_V4
   else        /* dual-mode requires else to avoid compiler warnings */
#endif /* IP_V4 */
   if(m->pkt->type == htons(0x86DD))   /* IPv6 packet */
   {
      struct ipv6 * pip6;
      pip6 = m->pkt->ip6_hdr;
      tlen = htons(pip6->ip_len);
      if(tlen < sizeof (struct tcphdr))
      {
         tcpstat.tcps_rcvshort++;
         return;
      }

      /* "ti" is partial overlay structure design for IPv4. It includes 
       * the TCP header as it's last 20 bytes. Since the tcp_reassm() logic
       * will overwrite the IPv6 src & dest addresses in the IP header, 
       * we save these in local storage before using "ti".
       */
      IP6CPY(&ip6_src, &pip6->ip_src);
      IP6CPY(&ip6_dst, &pip6->ip_dest);

      ti = (struct tcpiphdr *)(m->m_data - sizeof(struct ipovly));
      if ((char *)ti < m->pkt->nb_buff)
      {
         panic("tcp_input");
      }
   }
   else
   {
      dtrap();
      return;
   }
#endif   /* IP_V6 */

   /*
    * Check that TCP offset makes sense,
    * pull out TCP options and adjust length.
    */

   off = GET_TH_OFF(ti->ti_t) << 2;
   if (off < sizeof (struct tcphdr) || off > tlen) 
   {
#ifdef DO_TCPTRACE
      tcp_trace("tcp off: src %x off %d\n", ti->ti_src, off);
#endif
      tcpstat.tcps_rcvbadoff++;
      TCP_MIB_INC(tcpInErrs);   /* keep MIB stats */
      GOTO_DROP;
   }
   tlen -= (int)off;
   ti->ti_len = (u_short)tlen;
   if (off > sizeof (struct tcphdr)) 
   {
      int olen;      /* length of options field */
      u_char * op;   /* scratch option pointer */

      olen = off - sizeof (struct tcphdr);   /* get options length */
      om = m_getwithdata (MT_RXDATA, olen);  /* get mbuf for opts */
      if (om == 0)
         GOTO_DROP;
      om->m_len = olen;       /* set mbuf length */
      /* set pointer to options field at end of TCP header */
      if(m->pkt->type == htons(0x86DD))   /* IPv6 packet */
         op = (u_char*)(m->m_data + 20);  /* past TCP header */
      else
         op = (u_char*)(m->m_data + 40);  /* past IP + TCP */
      MEMCPY(om->m_data, op, olen); /* copy to new mbuf */

      /* strip options from data mbuf. This actually just cuts the first 
       * m_len bytes from the TCP header, but it leaves the mbuf members 
       * set so the adjustment below does the right thing.
       */
      m->m_data += om->m_len;
      m->m_len -= om->m_len;
   }
   tiflags = ti->ti_flags;

#if (BYTE_ORDER == LITTLE_ENDIAN)
   /* Convert TCP protocol specific fields to host format. */
   ti->ti_seq = ntohl(ti->ti_seq);
   ti->ti_ack = ntohl(ti->ti_ack);
   ti->ti_urp = ntohs(ti->ti_urp);
#endif   /* endian */


   /*
    * Locate pcb for segment.
    */
findpcb:

   switch(m->pkt->type)
   {
#ifdef IP_V4
   case  IPTP:   /* IPv4 packet */
      /* Drop TCP and IP headers; TCP options were dropped above. */
      m->m_data += 40;
      m->m_len -= 40;

      inp = in_pcblookup(&tcb, ti->ti_src.s_addr, ti->ti_sport, 
         ti->ti_dst.s_addr, ti->ti_dport, INPLOOKUP_WILDCARD);
      break;
#endif   /* IP_V4 */
#ifdef IP_V6
   case htons(0x86DD):   /* IPv6 packet */
      /* Drop TCP header; TCP options were dropped above. */
      m->m_data += 20;
      m->m_len -= 20;

      inp = ip6_pcblookup(&tcb, &ip6_src, ti->ti_sport, 
         &ip6_dst, ti->ti_dport, INPLOOKUP_WILDCARD);
      break;
#endif   /* IP_V6 */
   default:
      dtrap();
      return;
   }

   
   /*
    * If the state is CLOSED (i.e., TCB does not exist) then
    * all data in the incoming segment is discarded.
    * If the TCB exists but is in CLOSED state, it is embryonic,
    * but should either do a listen or a connect soon.
    */
   if (inp == 0)
      GOTO_DROPWITHRESET;
   tp = intotcpcb (inp);
   if (tp == 0)
      GOTO_DROPWITHRESET;
   if (tp->t_state == TCPS_CLOSED)
      GOTO_DROP;
   so = inp->inp_socket;
#ifdef DO_TCPTRACE
   if (so->so_options & SO_DEBUG) 
   {
      ostate = tp->t_state;
      tcp_saveti = *ti;
   }
#endif

   /* figure out the size of the other guy's receive window */
   rx_win = (tcp_win)(ntohs(ti->ti_win));    /* convert endian */

#ifdef TCP_WIN_SCALE
   /* Both the WINDSCLAE flag and send_wind_scale should be set if he 
    * negtitiated this at connect itme.
    */
   if((tp->t_flags & TF_WINSCALE) &&
      (tp->snd_wind_scale))
   {
      rx_win <<= tp->snd_wind_scale;         /* apply scale */
   }
#endif /* TCP_WIN_SCALE */

   if (so->so_options & SO_ACCEPTCONN) 
   {
      so = sonewconn(so);
      if (so == 0)
         GOTO_DROP;
      /*
       * This is ugly, but ....
       *
       * Mark socket as temporary until we're
       * committed to keeping it.  The code at
       * ``drop'' and ``dropwithreset'' check the
       * flag dropsocket to see if the temporary
       * socket created here should be discarded.
       * We mark the socket as discardable until
       * we're committed to it below in TCPS_LISTEN.
       */
      dropsocket++;

      inp = (struct inpcb *)so->so_pcb;
      inp->ifp = ifp;      /* save iface to peer */

      switch(so->so_domain)
      {
#ifdef IP_V4
      case AF_INET:
         inp->inp_laddr = ti->ti_dst;
#ifdef IP_PMTU
         inp->inp_pmtu = pmtucache_get(inp->inp_faddr.s_addr);
#else    /* not compiled for pathmtu, guess based on iface */
         inp->inp_pmtu = ifp->n_mtu - (ifp->n_lnh + 40);
#endif   /* IP_PMTU */
         break;
#endif   /* end v4 */

#ifdef IP_V6
      case AF_INET6:
         IP6CPY(&inp->ip6_laddr, &ip6_dst);
         inp->inp_pmtu = ip6_pmtulookup(&ip6_src, ifp);
         break;
#endif   /* end v6 */
      }

      inp->inp_lport = ti->ti_dport;
      tp = intotcpcb(inp);
      tp->t_state = TCPS_LISTEN;
   }

   /*
    * Segment received on connection.
    * Reset idle time and keep-alive timer.
    */
   tp->t_idle = 0;
   tp->t_timer[TCPT_KEEP] = tcp_keepidle;

   /*
    * Process options if not in LISTEN state,
    * else do it below (after getting remote address).
    */
   if (om && tp->t_state != TCPS_LISTEN) 
   {
      tcp_dooptions(tp, om, ti);
      om = 0;
   }

   acked = (int)(ti->ti_ack - tp->snd_una);

#ifdef TCP_SACK
   /* If we received a TCP SACK header, indicate we need output now */
   if(tp->t_flags & TF_SACKREPLY)
   {
      needoutput = 1;            /* set tcp_input local flag */
   }
   /* If new data has been acked and we have a sack hole list 
    * for the peer then see if the ack covers any sack holes
    */
   if((acked > 0) &&
      (tp->sack_hole_start[0] != 0) )
   {
      int i,j;
      for (i = 0; i < SACK_BLOCKS; i++)
      {
         /* see if the ack covers this sack hole */
         if(ti->ti_ack >= tp->sack_hole_end[i])
         {
            /* if hole is not the last one in the list, move up 
             * the trainling holes 
             */
            if((i < (SACK_BLOCKS - 1)) && 
               (tp->sack_hole_start[i + 1] != 0))
            {
               for(j = i; j < SACK_BLOCKS - 1; j++)
               {
                  tp->sack_hole_start[j] = tp->sack_hole_start[j + 1];
                  tp->sack_hole_end[j] = tp->sack_hole_end[j + 1];
               }
            }
            else  /* hole is at end of list */
            {
               /* delete the hole */
               tp->sack_hole_start[i] = tp->sack_hole_end[i] = 0;
            }
         }
      }
   }
#endif /* TCP_SACK */


   /*
    * Calculate amount of space in receive window,
    * and then do TCP input processing.
    * Receive window is amount of space in rcv queue,
    * but not less than advertised window.
    */
   { long win;

      win = (long)sbspace(&so->so_rcv);
      if (win < 0)
         win = 0;
      tp->rcv_wnd = (tcp_win)MAX((u_long)win, (tp->rcv_adv - tp->rcv_nxt));
   }


   /*
    * Header prediction: check for the two common cases
    * of a uni-directional data xfer.  If the packet has
    * no control flags, is in-sequence, the window didn't
    * change and we're not retransmitting, it's a
    * candidate.  If the length is zero and the ack moved
    * forward, we're the sender side of the xfer.  Just
    * free the data acked & wake any higher level process
    * that was blocked waiting for space.  If the length
    * is non-zero and the ack didn't move, we're the
    * receiver side.  If we're getting packets in-order
    * (the reassembly queue is empty), add the data to
    * the socket buffer and note that we need a delayed ack.
    */
   if ((tp->t_state == TCPS_ESTABLISHED) &&
       ((tiflags & (TH_SYN|TH_FIN|TH_RST|TH_URG|TH_ACK)) == TH_ACK) &&
       (ti->ti_seq == tp->rcv_nxt) &&
       (rx_win && rx_win == tp->snd_wnd) &&
       (tp->snd_nxt == tp->snd_max))
   {
      if (ti->ti_len == 0)
      {
         if (SEQ_GT(ti->ti_ack, tp->snd_una) &&
             SEQ_LEQ(ti->ti_ack, tp->snd_max) &&
             tp->snd_cwnd >= tp->snd_wnd) 
         {
            /*
             * this is a pure ack for outstanding data.
             */
            ++tcpstat.tcps_predack;
            if (tp->t_rttick && 
#ifdef TCP_TIMESTAMP
               ((tp->t_flags & TF_TIMESTAMP) == 0) && 
#endif /* TCP_TIMESTAMP */
               (SEQ_GT(ti->ti_ack, tp->t_rtseq)))
            {
               tcp_xmit_timer(tp);
            }

            tcpstat.tcps_rcvackpack++;
            tcpstat.tcps_rcvackbyte += acked;
            sbdrop(&so->so_snd, acked);
            tp->snd_una = ti->ti_ack;
            m_freem(m);

            /*
             * If all outstanding data are acked, stop
             * retransmit timer, otherwise restart timer
             * using current (possibly backed-off) value.
             * If process is waiting for space,
             * wakeup/selwakeup/signal.  If data
             * are ready to send, let tcp_output
             * decide between more output or persist.
             */
            if (tp->snd_una == tp->snd_max)
               tp->t_timer[TCPT_REXMT] = 0;
            else if (tp->t_timer[TCPT_PERSIST] == 0)
               tp->t_timer[TCPT_REXMT] = tp->t_rxtcur;

            if (so->so_snd.sb_flags & (SB_WAIT | SB_SEL))
               sowwakeup(so);

            /* If there is more data in the send buffer, and some is
             * still unsent, then call tcp_output() to try to send it
             */
            if (so->so_snd.sb_cc > (tp->snd_nxt - tp->snd_una))
               (void) tcp_output(tp);
            return;
         }
      }
      else if (ti->ti_ack == tp->snd_una &&
          tp->seg_next == (struct tcpiphdr *)tp &&
          ti->ti_len <= sbspace(&so->so_rcv))
      {
         int   adv;

#ifdef TCP_ZEROCOPY
         /* This may be a window probe sent because a zerocopy application
          * has closed the TCP window by sitting on all the bigbufs. If so,
          * we want to drop this byte and send an ACK for the pervious one.
          */
         if (so->rx_upcall && (ti->ti_len == 1) && (bigfreeq.q_len < 2))
         {
            goto drop_probe;
         }
#endif   /* TCP_ZEROCOPY */

         /* this may also be a garden-variety probe received because
          * the socket sendbuf was full.
          */
         if(tp->rcv_wnd == 0)
         {
#ifdef TCP_ZEROCOPY
drop_probe:
#endif   /* TCP_ZEROCOPY */
            /* we should probably do some elegant handling of the TCP state
             * info in this seg, but Windows NT 4.0 has a nasty bug where it
             * will hammer us mericilessly with these probes (one customer
             * reports thousands per second) so we just dump it ASAP to
             * save cycles.
             */
            tcpstat.tcps_rcvwinprobe++;
            m_freem (m);      /* free the received mbuf */
            tcp_output(tp);   /* send the ack now... */
            return;
         }

         /*
          * this is a pure, in-sequence data packet
          * with nothing on the reassembly queue and
          * we have enough buffer space to take it.
          */
         ++tcpstat.tcps_preddat;
         tp->rcv_nxt += ti->ti_len;
         tcpstat.tcps_rcvpack++;
         tcpstat.tcps_rcvbyte += ti->ti_len;
         /*
          * Add data to socket buffer.
          */
         sbappend(&so->so_rcv, m);
         sorwakeup(so);
#ifdef TCP_ZEROCOPY
         /* if it's a zerocopy socket, push data up to user */
         if (so->rx_upcall)
            tcp_data_upcall(so);
#endif   /* TCP_ZEROCOPY */

         /*
          * If this is a short packet, then ACK now - with Nagel
          *   congestion avoidance sender won't send more until
          *   he gets an ACK.
          */
         if (tiflags & TH_PUSH)
            tp->t_flags |= TF_ACKNOW;
         else
            tp->t_flags |= TF_DELACK;

         /* see if we need to send an ack */
         adv = (int)(tp->rcv_wnd - (tcp_win)(tp->rcv_adv - tp->rcv_nxt));

         if ((adv >= (int)(tp->t_maxseg * 2)) ||
             (tp->t_flags & TF_ACKNOW))
         {
#ifdef DO_DELAY_ACKS
            if(tp->t_delacktime)   /* doing delayed acks on timer */
            {
               if(tp->t_delackcur != 0)  /* ack timeout not set? */
                  tp->t_delackcur = tp->t_delacktime;  /* set it */
               tp->t_flags |= TF_DELACK;
               tp->t_flags &= ~TF_ACKNOW;
               return;
            }
#endif   /* DO_DELAY_ACKS */

            tp->t_flags |= TF_ACKNOW;
            tp->t_flags &= ~TF_DELACK;
            tcp_output(tp);   /* send the ack now... */
         }

         return;
      }
   }

   switch (tp->t_state) 
   {

   /*
    * If the state is LISTEN then ignore segment if it contains an RST.
    * If the segment contains an ACK then it is bad and send a RST.
    * If it does not contain a SYN then it is not interesting; drop it.
    * Don't bother responding if the destination was a broadcast.
    * Otherwise initialize tp->rcv_nxt, and tp->irs, select an initial
    * tp->iss, and send a segment:
    *     <SEQ=ISS><ACK=RCV_NXT><CTL=SYN,ACK>
    * Also initialize tp->snd_nxt to tp->iss+1 and tp->snd_una to tp->iss.
    * Fill in remote peer address fields if not previously specified.
    * Enter SYN_RECEIVED state, and process any other fields of this
    * segment in this state.
    */
   case TCPS_LISTEN: 
   {
         struct mbuf *  am;

         if (tiflags & TH_RST)
            GOTO_DROP;
         if (tiflags & TH_ACK)
            GOTO_DROPWITHRESET;
         if ((tiflags & TH_SYN) == 0)
            GOTO_DROP;
         if(in_broadcast(ti->ti_dst.s_addr))
            GOTO_DROP;
         am = m_getwithdata (MT_SONAME, sizeof (struct sockaddr));
         if (am == NULL)
            GOTO_DROP;

#ifdef IP_V4
         if(inp->inp_socket->so_domain == AF_INET)
         {
         struct sockaddr_in * sin;
         am->m_len = sizeof (struct sockaddr_in);
         sin = mtod(am, struct sockaddr_in *);
         sin->sin_family = AF_INET;
         sin->sin_addr = ti->ti_src;
         sin->sin_port = ti->ti_sport;
         /* Assuming pcbconnect will work, we put the sender's address in 
          * the inp_laddr (after saving a local laddr copy). If the connect
          * fails we restore the inpcb before going to drop:
          */
         laddr = inp->inp_laddr;    /* save tmp laddr */
         if (inp->inp_laddr.s_addr == INADDR_ANY)
            inp->inp_laddr = ti->ti_dst;
         if (in_pcbconnect (inp, am)) 
         {
            inp->inp_laddr = laddr;
            (void) m_free(am);
            GOTO_DROP;
         }
         
         inp->ifp = ifp;      /* set interface for conn.*/
         
         (void) m_free (am);
         }
#endif   /* end v4 */
#ifdef IP_V6
         if(inp->inp_socket->so_domain == AF_INET6)
         {
            struct sockaddr_in6 * sin;
            int      local_unspecified = FALSE;
                      
            am->m_len = sizeof (struct sockaddr_in6);
            sin = mtod(am, struct sockaddr_in6 *);
            sin->sin6_family = AF_INET6;
            IP6CPY(&sin->sin6_addr, &ip6_src);
            sin->sin6_port = ti->ti_sport;
            /* if socket has no local address, copy in pkt dest */
            if(IP6EQ(&inp->ip6_laddr, &ip6unspecified))
            {
               IP6CPY(&inp->ip6_laddr, &ip6_dst);
               local_unspecified = TRUE;  /* set flag in case connect fails */
            }
            
            inp->ifp = ifp;      /* set interface for conn
                                    for routing, maybe changed by ip6_pcbconnect */
                        
            if (ip6_pcbconnect (inp, am))
            {
               if(local_unspecified)   /* need to restore unspecified addr? */
                  IP6CPY(&inp->ip6_laddr, &ip6unspecified);
               (void) m_free(am);
               GOTO_DROP;
            }
            (void) m_free(am);
         }
#endif   /* end v6 */

         tp->t_template = tcp_template(tp);
         if (tp->t_template == 0) 
         {
            SETTP(tp, tcp_drop(tp, ENOBUFS));
            dropsocket = 0;      /* socket is already gone */
            GOTO_DROP;
         }
         if (om) 
         {
            tcp_dooptions(tp, om, ti);
            om = 0;
         }
         if (iss)
            tp->iss = iss;
         else
            tp->iss = tcp_iss;
         tcp_iss += (unsigned)(TCP_ISSINCR/2);
         tp->irs = ti->ti_seq;
         tcp_sendseqinit(tp);
         tcp_rcvseqinit(tp);
         tp->t_flags |= TF_ACKNOW;
         tp->t_state = TCPS_SYN_RECEIVED;
         tp->t_timer[TCPT_KEEP] = TCPTV_KEEP_INIT;
         dropsocket = 0;      /* committed to socket */
         tcpstat.tcps_accepts++;
         goto trimthenstep6;
      }

   /*
    * If the state is SYN_SENT:
    *   if seg contains an ACK, but not for our SYN, drop the input.
    *   if seg contains a RST, then drop the connection.
    *   if seg does not contain SYN, then drop it.
    * Otherwise this is an acceptable SYN segment
    *   initialize tp->rcv_nxt and tp->irs
    *   if seg contais ack then advance tp->snd_una
    *   if SYN has been acked change to ESTABLISHED else SYN_RCVD state
    *   arrange for segment to be acked (eventually)
    *   continue processing rest of data/controls, beginning with URG
    */
   case TCPS_SYN_SENT:
      inp->ifp = ifp;
      if ((tiflags & TH_ACK) &&
          (SEQ_LEQ(ti->ti_ack, tp->iss) ||
          SEQ_GT(ti->ti_ack, tp->snd_max)))
      {
         GOTO_DROPWITHRESET;
      }
      if (tiflags & TH_RST) 
      {
         if (tiflags & TH_ACK)
            SETTP(tp, tcp_drop(tp, ECONNREFUSED));
         GOTO_DROP;
      }
      if ((tiflags & TH_SYN) == 0)
         GOTO_DROP;
      if (tiflags & TH_ACK) 
      {
         tp->snd_una = ti->ti_ack;
         if (SEQ_LT(tp->snd_nxt, tp->snd_una))
            tp->snd_nxt = tp->snd_una;
      }
      tp->t_timer[TCPT_REXMT] = 0;
      tp->irs = ti->ti_seq;
      tcp_rcvseqinit(tp);
      if (inp->inp_laddr.s_addr != ti->ti_dst.s_addr) 
      {
         /* 
          * the IP interface may have changed address since we sent our SYN
          * (e.g. PPP brings link up as a result of said SYN and gets new
          * address via IPCP); if so we need to update the inpcb and the
          * TCP header template with the new address.
          */
         if ((m->pkt->net != NULL)
             && (m->pkt->net->n_ipaddr == ti->ti_dst.s_addr)) 
      /* send an ack */
         {
            inp->inp_laddr = ti->ti_dst;
            if (tp->t_template != NULL)
               tp->t_template->ti_src = ti->ti_dst;
         }
      }
      tp->t_flags |= TF_ACKNOW;
      if (tiflags & TH_ACK && SEQ_GT(tp->snd_una, tp->iss)) 
      {
         tcpstat.tcps_connects++;
         tp->t_state = TCPS_ESTABLISHED;
         soisconnected (so);
         tp->t_maxseg = tcp_mss(so);
         (void) tcp_reass (tp, (struct tcpiphdr *)0, m);
         /*
          * if we didn't have to retransmit the SYN,
          * use its rtt as our initial srtt & rtt var.
          */
         if (tp->t_rttick) 
         {
            tcp_xmit_timer(tp);
         }
      } else
         tp->t_state = TCPS_SYN_RECEIVED;

      trimthenstep6:
      /*
       * Advance ti->ti_seq to correspond to first data byte.
       * If data, trim to stay within window,
       * dropping FIN if necessary.
       */
      ti->ti_seq++;
      if ((tcp_win)ti->ti_len > tp->rcv_wnd) 
      {
         todrop = ti->ti_len - (u_short)tp->rcv_wnd;
         /* XXX work around 4.2 m_adj bug */
         if (m->m_len) 
         {
            m_adj(m, -todrop);
         }
         else 
         {
            /* skip tcp/ip header in first mbuf */
            m_adj(m->m_next, -todrop);
         }
         ti->ti_len = (u_short)tp->rcv_wnd;
         tiflags &= ~TH_FIN;
         tcpstat.tcps_rcvpackafterwin++;
         tcpstat.tcps_rcvbyteafterwin += todrop;
      }
      tp->snd_wl1 = ti->ti_seq - 1;
      tp->rcv_up = ti->ti_seq;
      goto step6;
   }

   /*
    * States other than LISTEN or SYN_SENT.
    * First check that at least some bytes of segment are within 
    * receive window.  If segment begins before rcv_nxt,
    * drop leading data (and SYN); if nothing left, just ack.
    */
   todrop = (int)(tp->rcv_nxt - ti->ti_seq);
   if (todrop > 0) 
   {
      if (tiflags & TH_SYN) 
      {
         tiflags &= ~TH_SYN;
         ti->ti_seq++;
         if (ti->ti_urp > 1) 
            ti->ti_urp--;
         else
            tiflags &= ~TH_URG;
         todrop--;
      }
      /*
       * Altera Niche Stack Nios port modification:
       * Add parenthesis to remove implicit order of operaton
       * & possible build warning.
       */
      if ((todrop > (int)ti->ti_len) ||
          ((todrop == (int)ti->ti_len) && 
          (tiflags&TH_FIN) == 0)) 
      {
         tcpstat.tcps_rcvduppack++;
         tcpstat.tcps_rcvdupbyte += ti->ti_len;
         /*
          * If segment is just one to the left of the window,
          * check two special cases:
          * 1. Don't toss RST in response to 4.2-style keepalive.
          * 2. If the only thing to drop is a FIN, we can drop
          *    it, but check the ACK or we will get into FIN
          *    wars if our FINs crossed (both CLOSING).
          * In either case, send ACK to resynchronize,
          * but keep on processing for RST or ACK.
          */
         if ((tiflags & TH_FIN && todrop == (int)ti->ti_len + 1) ||
            (tiflags & TH_RST && ti->ti_seq == tp->rcv_nxt - 1))
         {
            todrop = ti->ti_len;
            tiflags &= ~TH_FIN;
            tp->t_flags |= TF_ACKNOW;
         }
         else
            goto dropafterack;
      }
      else 
      {
         tcpstat.tcps_rcvpartduppack++;
         tcpstat.tcps_rcvpartdupbyte += todrop;
      }
      m_adj(m, todrop);
      ti->ti_seq += todrop;
      ti->ti_len -= (u_short)todrop;
      if (ti->ti_urp > (u_short)todrop)
         ti->ti_urp -= (u_short)todrop;
      else 
      {
         tiflags &= ~TH_URG;
         ti->ti_urp = 0;
      }
   }

   /*
    * If new data are received on a connection after the
    * user processes are gone, then RST the other end.
    */
   if ((so->so_state & SS_NOFDREF) &&
       tp->t_state > TCPS_CLOSE_WAIT && ti->ti_len) 
   {
      tp = tcp_close(tp);
      tcpstat.tcps_rcvafterclose++;
      GOTO_DROPWITHRESET;
   }

   /*
    * If segment ends after window, drop trailing data
    * (and PUSH and FIN); if nothing left, just ACK.
    */
   todrop = (int)((ti->ti_seq + (short)ti->ti_len) - (tp->rcv_nxt+tp->rcv_wnd));
   if (todrop > 0) 
   {
      tcpstat.tcps_rcvpackafterwin++;
      if (todrop >= (int)ti->ti_len) 
      {
         tcpstat.tcps_rcvbyteafterwin += ti->ti_len;
         /*
          * If a new connection request is received
          * while in TIME_WAIT, drop the old connection
          * and start over if the sequence numbers
          * are above the previous ones.
          */
         if (tiflags & TH_SYN &&
             tp->t_state == TCPS_TIME_WAIT &&
             SEQ_GT(ti->ti_seq, tp->rcv_nxt)) 
         {
            iss = (tcp_seq)(tp->rcv_nxt + (TCP_ISSINCR));
            if (iss & 0xff000000)
            {
               iss = 0L;
            }
            (void) tcp_close(tp);
            goto findpcb;
         }
         /*
          * If window is closed can only take segments at
          * window edge, and have to drop data and PUSH from
          * incoming segments.  Continue processing, but
          * remember to ack.  Otherwise, drop segment
          * and ack.
          */
         if ((tp->rcv_wnd == 0) && (ti->ti_seq == tp->rcv_nxt))
         {
            tp->t_flags |= TF_ACKNOW;
            tcpstat.tcps_rcvwinprobe++;
         } else
            goto dropafterack;
      } else
         tcpstat.tcps_rcvbyteafterwin += todrop;
      /* XXX work around m_adj bug */
      if (m->m_len) 
      {
         m_adj(m, -todrop);
      }
      else 
      {
         /* skip tcp/ip header in first mbuf */
         m_adj(m->m_next, -todrop);
      }
      ti->ti_len -= (u_short)todrop;
      tiflags &= ~(TH_PUSH|TH_FIN);
   }


   /*
    * If the RST bit is set examine the state:
    * SYN_RECEIVED STATE:
    *   If passive open, return to LISTEN state.
    *   If active open, inform user that connection was refused.
    * ESTABLISHED, FIN_WAIT_1, FIN_WAIT2, CLOSE_WAIT STATES:
    *   Inform user that connection was reset, and close tcb.
    * CLOSING, LAST_ACK, TIME_WAIT STATES
    *   Close the tcb.
    */

#ifdef DOS_RST
   /* DOS_RST - Fix for "Denial of Service (DOS) using RST"
    * An intruder can send RST packet to break on existing TCP
    * connection. It means that if a RST is received in
    * ESTABLISHED state from an intruder, then the connection gets
    * closed between the original peers. To overcome this 
    * vulnerability, it is suggested that we accept RST only when
    * the sequence numbers match. Else we send an ACK.
    */
   if ((tiflags & TH_RST) && (tp->t_state == TCPS_ESTABLISHED) &&
      (ti->ti_seq != tp->rcv_nxt))
   {
      /* RST received in established state and sequnece numbers
       * don't match.
       */
#ifdef DO_TCPTRACE
     tcp_trace("rcvd RST with seq num mismatch - ignoring RST, sending ACK.\n");
#endif
      tiflags &= ~TH_RST;  /* clear reset flag */
      goto dropafterack;   /* send an ack and drop current packet */
   }
#endif /* DOS_RST */

   if (tiflags&TH_RST) 
   {
      switch (tp->t_state) 
      {
   
      case TCPS_SYN_RECEIVED:
         so->so_error = ECONNREFUSED;
         goto close;
   
      case TCPS_ESTABLISHED:
         TCP_MIB_INC(tcpEstabResets);     /* keep MIB stats */
      case TCPS_FIN_WAIT_1:
      case TCPS_FIN_WAIT_2:
      case TCPS_CLOSE_WAIT:
         so->so_error = ECONNRESET;
         close:
         tp->t_state = TCPS_CLOSED;
         tcpstat.tcps_drops++;
         SETTP(tp, tcp_close(tp));
#ifdef TCP_ZEROCOPY
         if (so->rx_upcall)
            so->rx_upcall(so, NULL, ECONNRESET);
#endif   /* TCP_ZEROCOPY */
         GOTO_DROP;
   
      case TCPS_CLOSING:
      case TCPS_LAST_ACK:
      case TCPS_TIME_WAIT:
         SETTP(tp, tcp_close(tp));
         GOTO_DROP;
      }
   }

   /*
    * If a SYN is in the window, then this is an
    * error and we send an RST and drop the connection.
    */

#ifdef DOS_SYN
   if ((tiflags & TH_SYN) && (tp->t_state == TCPS_ESTABLISHED))
   {
      /* DOS_SYN - Fix for "Denial of Service (DOS) attack using SYN"
       * An intruder can send SYN to reset the connection.
       * Hence normal behaviour can cause a vulnerability.
       * To protect against this attack, just ignore the SYN packet
       * when we are in established state.
       * One solution is to send an ACK.
       * But our guess is that just dropping the SYN should also
       * work fine.
       */
#ifdef DO_TCPTRACE
     tcp_trace("rcvd SYN in established state - ignoring SYN.\n");
#endif
      GOTO_DROP;
   }
#else
   if (tiflags & TH_SYN) 
   {
      tp = tcp_drop(tp, ECONNRESET);
      GOTO_DROPWITHRESET;
   }
#endif /* end of else of DOS_SYN */

   /*
    * If the ACK bit is off we drop the segment and return.
    */
   if ((tiflags & TH_ACK) == 0)
      GOTO_DROP;

   /*
    * Ack processing.
    */
   switch (tp->t_state) 
   {

   /*
    * In SYN_RECEIVED state if the ack ACKs our SYN then enter
    * ESTABLISHED state and continue processing, otherwise
    * send an RST.
    */
   case TCPS_SYN_RECEIVED:
      if (SEQ_GT(tp->snd_una, ti->ti_ack) ||
          SEQ_GT(ti->ti_ack, tp->snd_max))
      {
         TCP_MIB_INC(tcpEstabResets);     /* keep MIB stats */
         GOTO_DROPWITHRESET;
      }
      tcpstat.tcps_connects++;
      tp->t_state = TCPS_ESTABLISHED;
      soisconnected(so);
      tp->t_maxseg = tcp_mss(so);
      (void) tcp_reass(tp, (struct tcpiphdr *)0, m);
      tp->snd_wl1 = ti->ti_seq - 1;
      /* fall into ... */

   /*
    * In ESTABLISHED state: drop duplicate ACKs; ACK out of range
    * ACKs.  If the ack is in the range
    *   tp->snd_una < ti->ti_ack <= tp->snd_max
    * then advance tp->snd_una to ti->ti_ack and drop
    * data from the retransmission queue.  If this ACK reflects
    * more up to date window information we update our window information.
    */
   case TCPS_ESTABLISHED:
   case TCPS_FIN_WAIT_1:
   case TCPS_FIN_WAIT_2:
   case TCPS_CLOSE_WAIT:
   case TCPS_CLOSING:
   case TCPS_LAST_ACK:
   case TCPS_TIME_WAIT:

      if (SEQ_LEQ(ti->ti_ack, tp->snd_una)) 
      {
         if (ti->ti_len == 0 && rx_win == tp->snd_wnd) 
         {
            tcpstat.tcps_rcvdupack++;
            /*
             * If we have outstanding data (not a
             * window probe), this is a completely
             * duplicate ack (ie, window info didn't
             * change), the ack is the biggest we've
             * seen and we've seen exactly our rexmt
             * threshhold of them, assume a packet
             * has been dropped and retransmit it.
             * Kludge snd_nxt & the congestion
             * window so we send only this one
             * packet.  If this packet fills the
             * only hole in the receiver's seq.
             * space, the next real ack will fully
             * open our window.  This means we
             * have to do the usual slow-start to
             * not overwhelm an intermediate gateway
             * with a burst of packets.  Leave
             * here with the congestion window set
             * to allow 2 packets on the next real
             * ack and the exp-to-linear thresh
             * set for half the current window
             * size (since we know we're losing at
             * the current window size).
             */
            if (tp->t_timer[TCPT_REXMT] == 0 ||
                ti->ti_ack != tp->snd_una)
            {
               tp->t_dupacks = 0;
            }
            else if (++tp->t_dupacks == tcprexmtthresh) 
            {
               tcp_seq onxt = tp->snd_nxt;
               u_short  win   =
               MIN(tp->snd_wnd, tp->snd_cwnd) / 2 /
               tp->t_maxseg;

               if (win < 2)
                  win = 2;
               tp->snd_ssthresh = (u_short)(win * tp->t_maxseg);

               tp->t_timer[TCPT_REXMT] = 0;
               tp->t_rttick = 0;
               tp->snd_nxt = ti->ti_ack;
               tp->snd_cwnd = tp->t_maxseg;
               (void) tcp_output(tp);

               if (SEQ_GT(onxt, tp->snd_nxt))
                  tp->snd_nxt = onxt;
               GOTO_DROP;
            }
         } else
            tp->t_dupacks = 0;
         break;
      }
      tp->t_dupacks = 0;
      if (SEQ_GT(ti->ti_ack, tp->snd_max)) 
      {
         tcpstat.tcps_rcvacktoomuch++;
         goto dropafterack;
      }
      acked = (int)(ti->ti_ack - tp->snd_una);
      tcpstat.tcps_rcvackpack++;
      tcpstat.tcps_rcvackbyte += acked;

      /*
       * If transmit timer is running and timed sequence
       * number was acked, update smoothed round trip time.
       * Since we now have an rtt measurement, cancel the
       * timer backoff (cf., Phil Karn's retransmit alg.).
       * Recompute the initial retransmit timer.
       */
      if((tp->t_rttick) && 
#ifdef TCP_TIMESTAMP
         ((tp->t_flags & TF_TIMESTAMP) == 0) && 
#endif /* TCP_TIMESTAMP */
         (SEQ_GT(ti->ti_ack, tp->t_rtseq)))
         tcp_xmit_timer(tp);

      /*
       * If all outstanding data is acked, stop retransmit
       * timer and remember to restart (more output or persist).
       * If there is more data to be acked, restart retransmit
       * timer, using current (possibly backed-off) value.
       */
      if (ti->ti_ack == tp->snd_max) 
      {
         tp->t_timer[TCPT_REXMT] = 0;
         needoutput = 1;
      } else if (tp->t_timer[TCPT_PERSIST] == 0)
         tp->t_timer[TCPT_REXMT] = tp->t_rxtcur;
      /*
       * When new data is acked, open the congestion window.
       * If the window gives us less than ssthresh packets
       * in flight, open exponentially (maxseg per packet).
       * Otherwise open linearly (maxseg per window,
       * or maxseg^2 / cwnd per packet).
       */
      {
         tcp_win  cw =  tp->snd_cwnd;
         u_short  incr  =  tp->t_maxseg;

         if (cw > tp->snd_ssthresh)
            incr = MAX( (incr * incr / cw), (ALIGN_TYPE << 2) );

         tp->snd_cwnd = MIN(cw + (u_short)incr, (IP_MAXPACKET));
      }
      if (acked > (int)so->so_snd.sb_cc) 
      {
         tp->snd_wnd -= (u_short)so->so_snd.sb_cc;
         sbdrop(&so->so_snd, (int)so->so_snd.sb_cc);
         ourfinisacked = 1;
      } 
      else 
      {
         sbdrop(&so->so_snd, acked);
         tp->snd_wnd -= (u_short)acked;
         ourfinisacked = 0;
      }

      if (so->so_snd.sb_flags & (SB_WAIT | SB_SEL))
         sowwakeup(so);

      tp->snd_una = ti->ti_ack;
      if (SEQ_LT(tp->snd_nxt, tp->snd_una))
         tp->snd_nxt = tp->snd_una;


      switch (tp->t_state) 
      {

      /*
       * In FIN_WAIT_1 STATE in addition to the processing
       * for the ESTABLISHED state if our FIN is now acknowledged
       * then enter FIN_WAIT_2.
       */
      case TCPS_FIN_WAIT_1:
         if (ourfinisacked) 
         {
            /*
             * If we can't receive any more
             * data, then closing user can proceed.
             * Starting the timer is contrary to the
             * specification, but if we don't get a FIN
             * we'll hang forever.
             */
            if (so->so_state & SS_CANTRCVMORE) 
            {
               soisdisconnected(so);
               tp->t_timer[TCPT_2MSL] = tcp_maxidle;
            }
            tp->t_state = TCPS_FIN_WAIT_2;
         }
         break;

       /*
       * In CLOSING STATE in addition to the processing for
       * the ESTABLISHED state if the ACK acknowledges our FIN
       * then enter the TIME-WAIT state, otherwise ignore
       * the segment.
       */
      case TCPS_CLOSING:
         if (ourfinisacked) 
         {
            tp->t_state = TCPS_TIME_WAIT;
            tcp_canceltimers(tp);
            tp->t_timer[TCPT_2MSL] = 2 * TCPTV_MSL;
            soisdisconnected(so);
         }
         break;

      /*
       * In LAST_ACK, we may still be waiting for data to drain
       * and/or to be acked, as well as for the ack of our FIN.
       * If our FIN is now acknowledged, delete the TCB,
       * enter the closed state and return.
       */
      case TCPS_LAST_ACK:
         if (ourfinisacked) 
         {
            SETTP(tp, tcp_close(tp));
            GOTO_DROP;
         }
         break;

      /*
       * In TIME_WAIT state the only thing that should arrive
       * is a retransmission of the remote FIN.  Acknowledge
       * it and restart the finack timer.
       */
      case TCPS_TIME_WAIT:
         tp->t_timer[TCPT_2MSL] = 2 * TCPTV_MSL;
         goto dropafterack;
      }
   }

step6:
   /*
    * Update window information.
    * Don't look at window if no ACK: TAC's send garbage on first SYN.
    */
   if ((tiflags & TH_ACK) &&
       /*
        * Altera Niche Stack Nios port modification:
        * Add parenthesis to remove implicit order of operation
        * & possible build warnings.
        */
       (SEQ_LT(tp->snd_wl1, ti->ti_seq) || (tp->snd_wl1 == ti->ti_seq &&
       (SEQ_LT(tp->snd_wl2, ti->ti_ack) ||
       ((tp->snd_wl2 == ti->ti_ack) && (rx_win > tp->snd_wnd)))))) 
   {
      /* keep track of pure window updates */
      if ((ti->ti_len == 0) &&
          (tp->snd_wl2 == ti->ti_ack) &&
          (rx_win > tp->snd_wnd))
      {
         tcpstat.tcps_rcvwinupd++;
      }
      tp->snd_wnd = rx_win;
      tp->snd_wl1 = ti->ti_seq;
      tp->snd_wl2 = ti->ti_ack;
      if (tp->snd_wnd > tp->max_sndwnd)
         tp->max_sndwnd = tp->snd_wnd;
      needoutput = 1;
   }

   /*
    * Process segments with URG.
    */
   if ((tiflags & TH_URG) && ti->ti_urp &&
       TCPS_HAVERCVDFIN(tp->t_state) == 0) 
   {
      /*
       * This is a kludge, but if we receive and accept
       * random urgent pointers, we'll crash in
       * soreceive.  It's hard to imagine someone
       * actually wanting to send this much urgent data.
       */
      if (ti->ti_urp + so->so_rcv.sb_cc > SB_MAX) 
      {
         ti->ti_urp = 0;         /* XXX */
         tiflags &= ~TH_URG;     /* XXX */
         goto dodata;         /* XXX */
      }
      /*
       * If this segment advances the known urgent pointer,
       * then mark the data stream.  This should not happen
       * in CLOSE_WAIT, CLOSING, LAST_ACK or TIME_WAIT STATES since
       * a FIN has been received from the remote side. 
       * In these states we ignore the URG.
       *
       * According to RFC961 (Assigned Protocols),
       * the urgent pointer points to the last octet
       * of urgent data.  We continue, however,
       * to consider it to indicate the first octet
       * of data past the urgent section
       * as the original spec states.
       */
      if (SEQ_GT(ti->ti_seq+ti->ti_urp, tp->rcv_up)) 
      {
         tp->rcv_up = ti->ti_seq + ti->ti_urp;
         so->so_oobmark = so->so_rcv.sb_cc +
         (tp->rcv_up - tp->rcv_nxt) - 1;
         if (so->so_oobmark == 0)
            so->so_state |= SS_RCVATMARK;
         sohasoutofband(so);
         tp->t_oobflags &= ~(TCPOOB_HAVEDATA | TCPOOB_HADDATA);
      }
      /*
       * Remove out of band data so doesn't get presented to user.
       * This can happen independent of advancing the URG pointer,
       * but if two URG's are pending at once, some out-of-band
       * data may creep in... ick.
       */
      if ( (ti->ti_urp <= ti->ti_len)
#ifdef SO_OOBINLINE
       && (so->so_options & SO_OOBINLINE) == 0
#endif
       )
      {
         tcp_pulloutofband(so, ti, m);
      }
   } else
      /*
       * If no out of band data is expected,
       * pull receive urgent pointer along
       * with the receive window.
       */
   if (SEQ_GT(tp->rcv_nxt, tp->rcv_up))
      tp->rcv_up = tp->rcv_nxt;
dodata:                       /* XXX */

   /*
    * Process the segment text, merging it into the TCP sequencing queue,
    * and arranging for acknowledgment of receipt if necessary.
    * This process logically involves adjusting tp->rcv_wnd as data
    * is presented to the user (this happens in tcp_usrreq.c,
    * case PRU_RCVD).  If a FIN has already been received on this
    * connection then we just ignore the text.
    */
   if ((ti->ti_len || (tiflags&TH_FIN)) &&
       TCPS_HAVERCVDFIN(tp->t_state) == 0) 
   {

      /* Do the common segment reassembly case inline */
      if((ti->ti_seq == tp->rcv_nxt) &&
         (tp->seg_next == (struct tcpiphdr *)(tp) ) &&
         (tp->t_state == TCPS_ESTABLISHED))
      {
#ifdef DO_DELAY_ACKS
         if (tp->t_delacktime)
         {
            if (tp->t_delackcur == 0)    /* need to set ack timer? */
               tp->t_delackcur = tp->t_delacktime;
            tp->t_flags |= TF_DELACK;
         }
         else
         {
            tp->t_flags |= TF_ACKNOW;
         }
#else    /* not DO_DELAY_ACKS */
            tp->t_flags |= TF_ACKNOW;
#endif   /* DO_DELAY_ACKS */

         tp->rcv_nxt += ti->ti_len;
         tiflags = ti->ti_flags & TH_FIN;
         tcpstat.tcps_rcvpack++;
         tcpstat.tcps_rcvbyte += ti->ti_len;
         sbappend(&so->so_rcv, (m));
         sorwakeup(so);
#ifdef TCP_SACK
         tp->sack_seqct = 0;     /* clear sack valid block count */
#endif /* TCP_SACK */
      }
      else     /* received out of sequence segment */
      {
         /* Drop it in the reassmbly queue */
         tiflags = tcp_reass(tp, ti, m);
         tp->t_flags |= TF_ACKNOW;

#ifdef TCP_SACK      
         /* If doing SACK and there's data, then save sack info. and
          * set flag to send sack header on next ack
          */
         if((tp->t_flags & TF_SACKOK) && (m->m_len > 0))
            tcp_setsackinfo(tp, ti);
#endif /* TCP_SACK */
      }


      /*
       * Note the amount of data that peer has sent into
       * our window, in order to estimate the sender's
       * buffer size.
       */
      len = (int)(so->so_rcv.sb_hiwat - (tp->rcv_adv - tp->rcv_nxt));
      if (len > (int)tp->max_rcvd)
         tp->max_rcvd = (u_short)len;
#ifdef TCP_ZEROCOPY
      if (so->rx_upcall)
      {
         tcp_data_upcall(so);
         /* if we have FIN and app has all data, do shutdown */
         if ((tiflags & TH_FIN) &&
            (so->so_rcv.sb_cc == 0))
         {
            so->rx_upcall(so, NULL, ESHUTDOWN);
            so->so_state |= SS_UPCFIN;    /* flag that upcall was FINed */
         }
      }
#endif   /* TCP_ZEROCOPY */
   } 
   else
   {
      m_freem(m);
      tiflags &= ~TH_FIN;
   }

   /*
    * If FIN is received ACK the FIN and let the user know
    * that the connection is closing.
    */
   if (tiflags & TH_FIN) 
   {
      if (TCPS_HAVERCVDFIN(tp->t_state) == 0) 
      {
         socantrcvmore(so);
         tp->t_flags |= TF_ACKNOW;
         tp->rcv_nxt++;
      }
      switch (tp->t_state) 
      {

       /*
       * In SYN_RECEIVED and ESTABLISHED STATES
       * enter the CLOSE_WAIT state.
       */
      case TCPS_SYN_RECEIVED:
      case TCPS_ESTABLISHED:
         tp->t_state = TCPS_CLOSE_WAIT;
         break;

       /*
       * If still in FIN_WAIT_1 STATE FIN has not been acked so
       * enter the CLOSING state.
       */
      case TCPS_FIN_WAIT_1:
         tp->t_state = TCPS_CLOSING;
         break;

       /*
       * In FIN_WAIT_2 state enter the TIME_WAIT state,
       * starting the time-wait timer, turning off the other 
       * standard timers.
       */
      case TCPS_FIN_WAIT_2:
         tp->t_state = TCPS_TIME_WAIT;
         tcp_canceltimers(tp);
         tp->t_timer[TCPT_2MSL] = 2 * TCPTV_MSL;
         soisdisconnected(so);
         break;

      /*
       * In TIME_WAIT state restart the 2 MSL time_wait timer.
       */
      case TCPS_TIME_WAIT:
         tp->t_timer[TCPT_2MSL] = 2 * TCPTV_MSL;
         break;
      }
   }
#ifdef DO_TCPTRACE
   if (so->so_options & SO_DEBUG)
      tcp_trace("TCP_IN: state: %d, tcpcb: %x saveti: %x", 
    ostate, tp, &tcp_saveti);
#endif
   /*
    * Return any desired output.
    */
   if (needoutput || (tp->t_flags & TF_ACKNOW))
      (void) tcp_output(tp);
   return;

dropafterack:
   /*
    * Generate an ACK dropping incoming segment if it occupies
    * sequence space, where the ACK reflects our state.
    */
   if (tiflags & TH_RST)
      GOTO_DROP;
   m_freem (m);
   tp->t_flags |= TF_ACKNOW;
   (void) tcp_output (tp);
   return;

dropwithreset:
   TCP_MIB_INC(tcpInErrs);    /* keep MIB stats */
   if (om) 
   {
      (void) m_free(om);
      om = 0;
   }

   /* Don't reset resets */
   if (tiflags & TH_RST)
      GOTO_DROP;

   /*
    * Generate a RST, dropping incoming segment.
    * Make ACK acceptable to originator of segment.
    * Don't bother to respond if destination was broadcast.
    */
#ifdef IP_V4
   if (in_broadcast(ti->ti_dst.s_addr))
      GOTO_DROP;
#endif

#ifdef IP_V6
   if(m->pkt->type == htons(0x86DD))
   {
      /* restore the received IP addresses for pass to the 
       * tcp_repond routine - they may have been corrupted. 
       */
      IP6CPY(&(m->pkt->ip6_hdr->ip_src), &ip6_src);
      IP6CPY(&(m->pkt->ip6_hdr->ip_dest), &ip6_dst);
   }
#endif   /* IP_V6 */

   if (tiflags & TH_ACK)
      tcp_respond (tp, ti, (tcp_seq)0, ti->ti_ack, TH_RST, m);
   else
   {
      if (tiflags & TH_SYN)
         ti->ti_seq++;
      tcp_respond(tp, ti, ti->ti_seq, (tcp_seq)0, TH_RST|TH_ACK, m);
   }
   /* destroy temporarily created socket */
   if (dropsocket)
      (void) soabort(so);
   return;

drop:
   if (om)
      (void) m_free(om);
   /*
    * Drop space held by incoming segment and return.
    */
#ifdef DO_TCPTRACE
   if (tp && (tp->t_inpcb->inp_socket->so_options & SO_DEBUG))
      tcp_trace("drop: state %d, tcpcb: %x, saveti: %x",
    ostate, tp, &tcp_saveti);
#endif
   m_freem(m);
   /* destroy temporarily created socket */
   if (dropsocket)
      (void) soabort(so);
   return;
}



/* FUNCTION: tcp_dooptions()
 * 
 * Parse received TCP options appended to the TCP header 
 *
 * PARAM1: struct tcpcb * tp
 * PARAM2: struct mbuf * om
 * PARAM3: struct tcpiphdr * ti
 *
 * RETURNS: 
 */

void
tcp_dooptions(struct tcpcb * tp, 
   struct mbuf *  om,
   struct tcpiphdr * ti)
{
   u_char * cp;   /* pointer into option buffer */
   int   opt;     /* current option code */
   int   optlen;  /* length of current option */
   int   cnt;     /* byte count left in header */
   struct socket * so = tp->t_inpcb->inp_socket;
#ifdef TCP_TIMESTAMP
   int   gotstamp = FALSE;    /* TRUE if we got a timestamp */
#endif   /* TCP_TIMESTAMP */


   cp = mtod(om, u_char *);
   cnt = om->m_len;
   for (; cnt > 0; cnt -= optlen, cp += optlen) 
   {
      opt = cp[0];
      if (opt == TCPOPT_EOL)
         break;
      if (opt == TCPOPT_NOP)
         optlen = 1;
      else 
      {
         optlen = cp[1];
         if (optlen <= 0)
            break;
      }

      switch (opt) 
      {
      case TCPOPT_MAXSEG:
      {
         u_short mssval;
         if (optlen != 4)
            continue;
         if (!(ti->ti_flags & TH_SYN))    /* MSS only on SYN */
            continue;
         mssval = *(u_short *)(cp + 2);
         mssval = ntohs(mssval);
         tp->t_maxseg = (u_short)MIN(mssval, (u_short)tcp_mss(so));
         break;
      }
#ifdef TCP_SACK
      case TCPOPT_SACKOK:
      {
         if(optlen != 2)
            continue;

         if (!(ti->ti_flags & TH_SYN))    /* only legal on SYN, SYN/ACK */
            continue;

         /* If SACK is OK with our socket and the other side then
          * set the t_flags bit which will allow it.
          */
         if(so->so_options & SO_TCPSACK)
         {
            tp->t_flags |= TF_SACKOK;
            tcpstat.tcps_sackconn++;
         }
         break;
      }
      case TCPOPT_SACKDATA:   /* got a SACK info packet */
         if(tp->t_flags & TF_SACKOK)
            tcp_resendsack(cp, tp);
         break;
#endif   /* TCP_SACK */

#ifdef TCP_WIN_SCALE
      case TCPOPT_WINSCALE:   /* window scale offer */
      {
         if (optlen != 3)
            continue;
         if (!(ti->ti_flags & TH_SYN))    /* only legal on SYN, SYN/ACK */
            continue;
         tp->snd_wind_scale = *(cp + 2);
         tp->t_flags |= TF_WINSCALE;
         /* Here's a a trick that's not in the literature. If the other side has
          * agreed to let us do scaled windows but sent set his scale as 0, 
          * then it means he's not ever going to get his window over 64K, and 
          * in practice. probably no where near that high. Some TCPs (like 
          * Microsoft's Win 2K) get really confused by disparate window sizes.
          * If we put a lid on our size too, things work a lot better.
          */
         if(tp->snd_wind_scale == 0)
            so->so_rcv.sb_hiwat = (32 * 1024);
         break;
      }
#endif /* TCP_WIN_SCALE */

#ifdef TCP_TIMESTAMP
      case TCPOPT_RTT:     /* round trip time option */
         if (optlen != 10)
            continue;

         if((so->so_options & SO_TIMESTAMP) == 0)
            continue;

         gotstamp = TRUE;     /* set flag indicating we found timestamp */

         /* Timestamp options are only legal on SYN packets or on connections
          * where we got one on the SYN packet 
          */
         if((ti->ti_flags & TH_SYN) ||
            (tp->t_flags & TF_TIMESTAMP))
         {
            /* If it's on a SYN packet, mark the connection as doing timestamps */
            if(ti->ti_flags & TH_SYN)
            {
               tp->t_flags |= TF_TIMESTAMP;
               /* 
                * Reduce our MSS for this connection to reflect the size of
                * the option we now have to add to every packet.
                */
               tp->t_maxseg -= 12;
               break;
            }

            /* See if we should save his timestamp for our replys. RFC-1323
             * goes into detail about how to do this in section 3.4.
             */
            if((ti->ti_seq <= tp->last_ack) &&
               (tp->last_ack < (ti->ti_seq + ti->ti_len)))
            {
               tp->ts_recent = tcp_getseq(cp + 2);
            }
            /* If the received segment acknowledges new data then update our
             * round trip time. Again, from RFC-1323: (3.3)
             */
            if(ti->ti_t.th_ack > tp->snd_una)
            {
               tp->t_rttick = tcp_getseq(cp + 6);  /* extract echoed tick count */
               tcp_xmit_timer(tp);     /*  pass to routine */
            }
         }
         break;
#endif   /* TCP_TIMESTAMP */

      default:
         break;
      }
   }
   (void) m_free(om);

#ifdef TCP_TIMESTAMP
   /* 
    * On syn/ack packets see if we should be timestamping or not. The
    * tp->t_flags TF_TIMESTAMP bit was already set above if we sent a
    * SYN w/RTTM and peer acked it in kind, but if we sent RTTM in
    * a SYN and this syn/ack did NOT echo it then the peer is not 
    * doing RTTM and we should not pester it with options it probably
    * doesn't understand.
    */
   if((ti->ti_flags & (TH_SYN|TH_ACK)) == (TH_SYN|TH_ACK))
   {
      if(!gotstamp)
         tp->t_flags &= ~TF_TIMESTAMP;
   }
#endif /* TCP_TIMESTAMP */

   return;
}



/* FUNCTION: tcp_pulloutofband()
 *
 * Pull out of band byte out of a segment so
 * it doesn't appear in the user's data queue.
 * It is still reflected in the segment length for
 * sequencing purposes.
 *
 * 
 * PARAM1: struct socket *so
 * PARAM2: struct tcpiphdr *ti
 * PARAM3: struct mbuf *m
 *
 * RETURNS: 
 */

void
tcp_pulloutofband(struct socket * so, 
   struct tcpiphdr * ti,
   struct mbuf *  m)
{
   int   cnt   =  ti->ti_urp  -  1;

   /**m = dtom(ti);**/
   while (cnt >= 0) 
   {
      if (m->m_len > (unsigned)cnt) 
      {
         char *   cp =  mtod(m,  char *) +  cnt;
         struct tcpcb * tp =  sototcpcb(so);

         tp->t_iobc = *cp;
         tp->t_oobflags |= TCPOOB_HAVEDATA;
         MEMCPY(cp, cp+1, (unsigned)(m->m_len - cnt - 1));
         m->m_len--;
         return;
      }
      cnt -= m->m_len;
      m = m->m_next;
      if (m == 0)
         break;
   }
   panic("tcp_pulloutofband");
}




/* FUNCTION: tcp_xmit_timer()
 * 
 * PARAM1: struct tcpcb * tp
 *
 * RETURNS: 
 */

void
tcp_xmit_timer(struct tcpcb * tp)
{
   int delta;
   int rtt;

#ifdef NPDEBUG
   if(tp->t_rttick == 0){ dtrap(); return; }
#endif

   tcpstat.tcps_rttupdated++;

   /* get  this rtt. Convert from cticks to TCP slow ticks */
   rtt = (int)((cticks - tp->t_rttick) / (TPS/2));
   if(tp->t_srtt != 0)
   {
      if(rtt == 0)      /* fast path for small round trip */
      {
         /* if either the rtt or varience is over 1, reduce it. */
         if(tp->t_srtt > 1)
            tp->t_srtt--;
         if(tp->t_rttvar > 1)
            tp->t_rttvar--;
      }
      else
      {
      /*
       * srtt is stored as fixed point with 3 bits
       * after the binary point (i.e., scaled by 8).
       * The following magic is equivalent
       * to the smoothing algorithm in rfc793
       * with an alpha of .875
       * (srtt = rtt/8 + srtt*7/8 in fixed point).
       */
      delta = ((rtt - 1) << 2) - (int)(tp->t_srtt >> 3);
      if ((tp->t_srtt += delta) <= 0)
         tp->t_srtt = 1;
      /*
       * We accumulate a smoothed rtt variance
       * (actually, a smoothed mean difference),
       * then set the retransmit timer to smoothed
       * rtt + 2 times the smoothed variance.
       * rttvar is stored as fixed point
       * with 2 bits after the binary point
       * (scaled by 4).  The following is equivalent
       * to rfc793 smoothing with an alpha of .75
       * (rttvar = rttvar*3/4 + |delta| / 4).
       * This replaces rfc793's wired-in beta.
       */
      if (delta < 0)
         delta = -delta;
      delta -= (short)(tp->t_rttvar >> 1);
      if ((tp->t_rttvar += delta) <= 0)
         tp->t_rttvar = 1;
      }
   }
   else  /* srtt is zero, nedd to initialize it */
   {
      /* 
       * No rtt measurement yet - use the
       * unsmoothed rtt.  Set the variance
       * to half the rtt (so our first
       * retransmit happens at 2*rtt)
       */
      if(rtt < 1)
         rtt = 1;
      tp->t_srtt = rtt << 3;
      tp->t_rttvar = rtt << 1;
   }
   tp->t_rttick = 0;       /* clear RT timer */
   tp->t_rxtshift = 0;
   TCPT_RANGESET(tp->t_rxtcur, 
    ((tp->t_srtt >> 2) + tp->t_rttvar) >> 1,
    TCPTV_MIN, TCPTV_REXMTMAX);
}


#ifdef IP_V4
int
ip4_tcpmss(struct socket * so)
{
   NET ifp;

   ifp = so->so_pcb->ifp;
   return(ifp->n_mtu - (40 + ifp->n_lnh));
}
#endif   /* IP_V4 */

#ifdef IP_V6
int
ip6_tcpmss(struct socket * so, NET ifp)
{
   int pmtu;
   
   pmtu = ip6_pmtulookup(&so->so_pcb->ip6_faddr, ifp);
   return pmtu - 20;
}
#endif   /* IP_V6 */


/* FUNCTION: tcp_mss()
 *
 * Find TCP_MSS for socket passed.
 *
 * PARAM1: socket
 *
 * RETURNS: 
 */


int
tcp_mss(struct socket * so)
{
   int   mss;     /* MSS value to return */
   struct inpcb * inp;
   struct tcpcb * tp;
#ifdef IP_V6
   NET  ifp = 0;
#endif

   if ((so == NULL) ||
       (so->so_pcb == NULL) ||
       (so->so_pcb->ifp == NULL))
   {
      if (so->so_domain == AF_INET)  /* IPv4 */
         return TCP_MSS;            /* user defined */
#ifdef IP_V6
      else                          /* IPv6 */
      {
           /* no ifp */
           return 1220;               /* RFC2460 defined, less IPv6/TCP hdrs */
      }
#endif   /* IP_V6 */
   }
#ifdef IP_V6
   else
   	if (so && so->so_pcb && so->so_pcb->ifp)
   		  ifp = so->so_pcb->ifp;
   	else
        return 1220;               /* RFC2460 defined, less IPv6/TCP hdrs */
#endif   /* IP_V6 */


#ifdef IP_V4   /* Begin messy domain defines */
#ifndef IP_V6  /* V4 only version */
   mss = ip4_tcpmss(so);
#else          /* dual mode */
   if (so->so_domain == AF_INET)              /* IPv4 */
      mss = ip4_tcpmss(so);
   else                                      /* IPv6 */
      mss = ip6_tcpmss(so, ifp);
#endif         /* end of dual mode */
#else          /* no IP_v4, assume V6 only */
      mss = ip6_tcpmss(so, ifp);
#endif         /* end messy domain defines */

   if (mss > TCP_MSS)    /* check upper limit from compile */
      mss = TCP_MSS;

   /* check upper limit which may be set by setsockopt() */
   inp = (struct inpcb *)so->so_pcb;         /* Map socket to IP cb */
   tp = (struct tcpcb *)inp->inp_ppcb;       /* Map IP to TCP cb */
   
   /* has user set max seg? */
   if (tp->t_flags & TF_MAXSEG)
      return tp->t_maxseg;    /* yup */
      
   if (tp->t_maxseg && (mss > tp->t_maxseg))  /* check tcp's mss */
   {
      mss = tp->t_maxseg;        /* limit new MSS to set MSS */
   }

   return mss;
}



#endif /* INCLUDE_TCP */

/* end of file tcp_in.c */

