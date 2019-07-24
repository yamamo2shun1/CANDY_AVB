/*
 * FILENAME: tcp_out.c
 *
 * Copyright 1997-2000 By InterNiche Technologies, Inc. All rights reserved.
 *
 *
 * MODULE: INET
 *
 * ROUTINES: tcp_output(), tcp_setpersist()
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

#define     TCPOUTFLAGS

#include "ipport.h"
#include "tcpport.h"

#ifdef INCLUDE_TCP  /* include/exclude whole file at compile time */

#include "in_pcb.h"
#include "ip.h"
#include "pmtu.h"
#include "tcp.h"
#include "tcp_fsm.h"
#include "tcp_seq.h"
#include "tcp_timr.h"
#include "tcp_var.h"
#include "tcpip.h"


/*
 * Initial options. We always Use TCP MSS. In cases where we use other
 * options we make the options buffer large enough for full sized option
 * data header so that we can use it for option building later. We start
 * by defining all the header sizes.
 */

#define  MSSOPT_LEN     4

#ifdef TCP_SACK
#define  SACKOPT_LEN    ((SACK_BLOCKS * 8) + 2)
#else
#define  SACKOPT_LEN    0
#endif

#ifdef TCP_WIN_SCALE
#define  SWINOPT_LEN    4  /* actually 3, but round upwards */
#else
#define  SWINOPT_LEN    0
#endif

#ifdef TCP_TIMESTAMP
#define  STAMPOPT_LEN   10
#else
#define  STAMPOPT_LEN   0
#endif


/* Add up all the option sizes for the worst case option buffer size */
#define  OPTBUF_LEN (MSSOPT_LEN + SACKOPT_LEN + SWINOPT_LEN + STAMPOPT_LEN)

/* create the buffer and fill in the MSS option */
u_char   tcp_optionbuf[OPTBUF_LEN];

static int   bld_options(struct tcpcb *, u_char *, int, struct socket *);


/*
 * TCP Headers and IPv6
 *
 *   The tcp_output() routine gets it's send data from an mbuf 
 * queue in the socket structure. These mbufs contain only the 
 * data to send, no headers. The tcp_output() code prepends an 
 * IP/TCP header to this data for sending. In the old v4 code 
 * this header was placed in a space in the front of the PACKET 
 * containing the data mbuf. This  space had previously been  
 * reserved for this purpose by getnbuf(). If there is no mbuf 
 * data to send, then an mbuf large enough for the TCP/IP header
 * structure "ti" is allocated.
 * 
 *   "ti" is a BSD invention. It contains an "overlay" IP header followed by
 * a real TCP header. The overlay IP header has IP addresses at the end of 
 * structure where a real IP header would, however the beginning of the 
 * structure contains pointers which are used by the tcp_input() socket
 * code to link the received TCP reassembly queue. When we are sending 
 * packets (below) the v4 IP header is constructed in the "ti" structure,
 * and the complete IP/TCP header is passed down to the IP layer in a single
 * contiguous buffer.
 * 
 *   For IPv6, the IP header is 40 bytes instead of 20, and IPv6 is far more 
 * likely than IPv4 to insert a field between the IP and TCP headers. For 
 * these reasons, it's not as beneficial to keep the IP and TCP headers in a 
 * contiguous buffer. So we don't. Instead we require that the system support 
 * a scatter/gather version of the PACKET structures on which the mbufs are 
 * mapped. The IPv6 code allocates a seperate mbuf for the IP header, and 
 * builds the IP header seperatly from the TCP header. 
 * 
 *    Since we also want to support a dual-mode stack, we still maintain 
 * the old "ti" strucuture for use by the the v4 packets, as well as 
 * seperate IPv6 header mbuf used by the v6 code path. Thus the mbufs 
 * allocated for TCP headers are still (about) 40 bytes in size.
 */


/* FUNCTION: tcp_output()
 *
 * Tcp output routine: figure out what should be sent and send it.
 *
 * PARAM1: struct tcpcb * tp 
 *
 * RETURNS: 0 if OK, else a sockets error code.
 */

int
tcp_output(struct tcpcb * tp)
{
   struct socket *   so =  tp->t_inpcb->inp_socket;
   int   len;
   long  win;
   int   off,  flags,   error;
   struct mbuf *  m;
   struct tcpiphdr * ti;
   unsigned optlen = 0;
   int   idle, sendalot;
   struct mbuf *  sendm;   /* mbuf which contains data to send */
   struct mbuf * tcp_mbuf; /* mbuf containing TCP header */
   int   bufoff;           /* offset of data in sendm->m_data */

#ifdef TCP_SACK
   int   sack_resend;
   int   sack_hole = 0;    /* next sack hole to fill */

   if(tp->t_flags & TF_SACKREPLY)
   {
      /* we are resending based on a received SACK header */
      sack_resend = TRUE;
      tp->t_flags &= ~TF_SACKREPLY;    /* clear flag */
   }
   else
      sack_resend = FALSE;
#endif /* TCP_SACK */
   
   /*
    * Determine length of data that should be transmitted,
    * and flags that will be used.
    * If there is some data or critical controls (SYN, RST)
    * to send, then transmit; otherwise, investigate further.
    */
   idle = (tp->snd_max == tp->snd_una);

again:
   sendalot = 0;
   off = (int)(tp->snd_nxt - tp->snd_una);
   win = (long)tp->snd_wnd;   /* set basic send window */
   if (win > (long)tp->snd_cwnd) /* see if we need congestion control */
   {
      win = (int)(tp->snd_cwnd & ~(ALIGN_TYPE-1)); /* keep data aligned */
   }

   /*
    * If in persist timeout with window of 0, send 1 byte.
    * Otherwise, if window is small but nonzero
    * and timer expired, we will send what we can
    * and go to transmit state.
    */
   if (tp->t_force) 
   {
      if (win == 0)
         win = 1;
      else 
      {
         tp->t_timer[TCPT_PERSIST] = 0;
         tp->t_rxtshift = 0;
      }
   }

#ifdef TCP_SACK
   /* See if we need to adjust the offset for a sack resend */
   if(sack_resend)
   {
      off = (int)(tp->sack_hole_start[sack_hole] - tp->snd_una);
      /* if this hole's already been acked then punt and move to next hole */
      if(off < 0)
      {
         /* clear out the acked hole */
         tp->sack_hole_start[sack_hole] = tp->sack_hole_end[sack_hole] = 0;
         /* see if we're done with SACK hole list (2 tests) */
         if(++sack_hole >= SACK_BLOCKS)
            return 0;
         if(tp->sack_hole_start[sack_hole] == tp->sack_hole_end[sack_hole])
            return 0;
         goto again;
      }
      tp->snd_nxt = tp->sack_hole_start[sack_hole];
      len = (int)(tp->sack_hole_end[sack_hole] - tp->sack_hole_start[sack_hole]);
      len = (int)MIN(len, (int)win);
   }
   else
#endif /* TCP_SACK */
   {
      /* set length of packets which are not sack resends */
      len = (int)MIN(so->so_snd.sb_cc, (unsigned)win) - off;
   }

   flags = tcp_outflags[tp->t_state];


   /* See if we need to build TCP options field. This test should be fast. */

#if (defined(TCP_TIMESTAMP) | defined(TCP_SACK))	   
   if((flags & TH_SYN) ||
/*   !!!???   (so->so_options & SO_TIMESTAMP) ||  */
	  (tp->t_flags & TF_SACKNOW)
	 )
   {
      optlen = bld_options(tp, &tcp_optionbuf[optlen], flags, so);
   }
#else
   /* If other options not defined this build then don't bother to call bld_options() except 
    * on SYN packets
    */
   if(flags & TH_SYN)
   {
      optlen = bld_options(tp, &tcp_optionbuf[optlen], flags, so);
   }
#endif

   if (len < 0)
   {
      /*
       * If FIN has been sent but not acked,
       * but we haven't been called to retransmit,
       * len will be -1.  Otherwise, window shrank
       * after we sent into it.  If window shrank to 0,
       * cancel pending retransmit and pull snd_nxt
       * back to (closed) window.  We will enter persist
       * state below.  If the window didn't close completely,
       * just wait for an ACK.
       */
      len = 0;
      if (win == 0) 
      {
         tp->t_timer[TCPT_REXMT] = 0;
         tp->snd_nxt = tp->snd_una;
      }
   }

   if (len > (int)tp->t_maxseg)
   {
      len = tp->t_maxseg;
      sendalot = 1;
   }

#ifdef IP_V4
#ifdef IP_PMTU
   {
      int pmtu = tp->t_inpcb->inp_pmtu - 40;

      if (len > pmtu)
      {
         len = pmtu - 40;
         sendalot = 1;
      }
   }
#endif /* IP_PMTU */
   /* We don't need a pmtu test for IPv6. V6 code limits t_maxseg to
    * the Path MTU, so the test above the v4 ifdef above covers us.
    */
#endif /* IP_V4 */

   if (SEQ_LT(tp->snd_nxt + len, tp->snd_una + so->so_snd.sb_cc))
      flags &= ~TH_FIN;
   win = (long)(sbspace(&so->so_rcv));

   /*
    * If our state indicates that FIN should be sent
    * and we have not yet done so, or we're retransmitting the FIN,
    * then we need to send.
    */
   if ((flags & TH_FIN) &&
       (so->so_snd.sb_cc == 0) &&
       ((tp->t_flags & TF_SENTFIN) == 0 || tp->snd_nxt == tp->snd_una))
   {
      goto send;
   }
   /*
    * Send if we owe peer an ACK.
    */
   if (tp->t_flags & TF_ACKNOW)
      goto send;
   if (flags & (TH_SYN|TH_RST))
      goto send;
   if (SEQ_GT(tp->snd_up, tp->snd_una))
      goto send;

   /*
    * Sender silly window avoidance.  If connection is idle
    * and can send all data, a maximum segment,
    * at least a maximum default-size segment do it,
    * or are forced, do it; otherwise don't bother.
    * If peer's buffer is tiny, then send
    * when window is at least half open.
    * If retransmitting (possibly after persist timer forced us
    * to send into a small window), then must resend.
    */
   if (len)
   {
      if (len == (int)tp->t_maxseg)
         goto send;
      if ((idle || tp->t_flags & TF_NODELAY) &&
          len + off >= (int)so->so_snd.sb_cc)
      {
         goto send;
      }
      if (tp->t_force)
         goto send;
      if (len >= (int)(tp->max_sndwnd / 2))
         goto send;
      if (SEQ_LT(tp->snd_nxt, tp->snd_max))
         goto send;
   }

   /*
    * Compare available window to amount of window
    * known to peer (as advertised window less
    * next expected input).  If the difference is at least two
    * max size segments or at least 35% of the maximum possible
    * window, then want to send a window update to peer.
    */
   if (win > 0)
   {
      int   adv   =  (int)win -  (int)(tp->rcv_adv -  tp->rcv_nxt);

      if (so->so_rcv.sb_cc == 0 && adv >= (int)(tp->t_maxseg * 2))
         goto send;
      if (100 * (u_int)adv / so->so_rcv.sb_hiwat >= 35)
         goto send;
   }

   /*
    * TCP window updates are not reliable, rather a polling protocol
    * using ``persist'' packets is used to insure receipt of window
    * updates.  The three ``states'' for the output side are:
    *   idle         not doing retransmits or persists
    *   persisting      to move a small or zero window
    *   (re)transmitting   and thereby not persisting
    *
    * tp->t_timer[TCPT_PERSIST]
    *   is set when we are in persist state.
    * tp->t_force
    *   is set when we are called to send a persist packet.
    * tp->t_timer[TCPT_REXMT]
    *   is set when we are retransmitting
    * The output side is idle when both timers are zero.
    *
    * If send window is too small, there is data to transmit, and no
    * retransmit or persist is pending, then go to persist state.
    * If nothing happens soon, send when timer expires:
    * if window is nonzero, transmit what we can,
    * otherwise force out a byte.
    */
   if (so->so_snd.sb_cc && tp->t_timer[TCPT_REXMT] == 0 &&
       tp->t_timer[TCPT_PERSIST] == 0) 
   {
      tp->t_rxtshift = 0;
      tcp_setpersist(tp);
   }

   /*
    * No reason to send a segment, just return.
    */
   return (0);

send:
   ENTER_CRIT_SECTION(tp);

   /* Limit send length to the current buffer so as to
    * avoid doing the "mbuf shuffle" in m_copy().
    */
   bufoff = off;
   sendm = so->so_snd.sb_mb;
   if (len)
   {
      /* find mbuf containing data to send (at "off") */
      while (sendm)  /* loop through socket send list */
      {
         bufoff -= sendm->m_len;
         if (bufoff < 0)   /* if off is in this buffer, break */
            break;
         sendm = sendm->m_next;
      }
      if (!sendm) { dtrap();  /* shouldn't happen */ }
      bufoff += sendm->m_len; /* index to next data to send in msend */

      /* if socket has multiple unsent mbufs, set flag for send to loop */
      if ((sendm->m_next) && (len > (int)sendm->m_len))
      {
         flags &= ~TH_FIN; /* don't FIN on segment prior to last */
         sendalot = 1;     /* set to send more segments */
      }
      if((flags & TH_FIN) && (so->so_snd.sb_cc > (unsigned)len))
      {
         /* This can happen on slow links (PPP) which retry the last 
          * segment - the one with the FIN bit attached to data.
          */
         flags &= ~TH_FIN; /* don't FIN on segment prior to last */
      }

      /* only send the rest of msend */
      len = min(len, (int)sendm->m_len);

      /* if we're not sending starting at sendm->m_data (in which 
       * case bufoff != 0), then we will copy the data; else we would 
       * write IP/TCP headers over sent but un-ack'ed data in sendm. 
       * Similarly, if sendm->m_data is not aligned with respect to 
       * sendm->m_base and ALIGN_TYPE, we will copy the data to 
       * ensure that it (and the then-prepended IP/TCP headers) will 
       * be aligned according to ALIGN_TYPE. 
       */
      if ((bufoff != 0) ||       /* data not front aligned in send mbuf? */
          (((sendm->m_data - sendm->m_base) & (ALIGN_TYPE - 1)) != 0))
      {
         len = min(len, (int)(sendm->m_len - bufoff));   /* limit len again */

         /* One more test - if this data is not aligned with the front
          * of the m_data buffer then we can't use it in place, else we
          * might write the IP/TCP header over data that has not yet
          * been acked. In this case we must make sure our send
          * fits into a little buffer and send what we can.
          */
         if ((len > (int)(lilbufsiz - HDRSLEN)) && /* length is bigger the small buffer? */
             (bigfreeq.q_len < 2))      /* and we are low on big buffers */
         {
            len = lilbufsiz - HDRSLEN;
         }
      }
   }

   /* if send data is sufficiently aligned in packet, prepend TCP/IP header
    * in the space provided. 
    */
   if (len && (bufoff == 0) && 
       (sendm->pkt->inuse == 1) &&
       (((sendm->m_data - sendm->m_base) & (ALIGN_TYPE - 1)) == 0) && 
       (optlen == 0))
   {
      /* get an empty mbuf to "clone" the data */
      m = m_getnbuf(MT_TXDATA, 0);
      if (!m)
      {
         EXIT_CRIT_SECTION(tp);
         return (ENOBUFS);
      }
      m->pkt = sendm->pkt; /* copy packet location in new mbuf */
      m->pkt->inuse++;     /* bump packet's use count */
      m->m_base = sendm->m_base; /* clone mbuf members */
      m->m_memsz = sendm->m_memsz;
      m->m_len = len + TCPIPHDRSZ;  /* adjust clone for header */
      m->m_data = sendm->m_data - TCPIPHDRSZ;
   }
   else  /* either no data or data is not front aligned in mbuf */
   {
      /* Grab a header mbuf, attaching a copy of data to be 
       * transmitted, and initialize the header from 
       * the template for sends on this connection.
       */
      m = m_getwithdata (MT_HEADER, IFNETHDR_SIZE + TCPIPHDRSZ);
      if (m ==(struct mbuf *)NULL)
      {
         EXIT_CRIT_SECTION(tp);
         return ENOBUFS;
      }

      m->m_len = TCPIPHDRSZ;
      m->m_data += IFNETHDR_SIZE;/* Move this to sizeof tcpip hdr leave*/
      /* 14 bytes for ethernet header      */

      if (len) /* attach any data to send */
      {
         m->m_next = m_copy(so->so_snd.sb_mb, off, (int) len);
         if (m->m_next == 0)
         {
            m_freem(m);
            EXIT_CRIT_SECTION(tp);
            return ENOBUFS;
         }
      }
   }
   EXIT_CRIT_SECTION(tp);

   if (len) 
   {
      if (tp->t_force && len == 1)
         tcpstat.tcps_sndprobe++;
      else if (SEQ_LT(tp->snd_nxt, tp->snd_max)) 
      {
         tcpstat.tcps_sndrexmitpack++;
         tcpstat.tcps_sndrexmitbyte += len;
#ifdef TCP_SACK
      if(sack_resend)
         tcpstat.tcps_sackresend++;
#endif
      } 
      else 
      {
         tcpstat.tcps_sndpack++;
         tcpstat.tcps_sndbyte += len;
      }
   }
   else if (tp->t_flags & TF_ACKNOW)
   {
      tcpstat.tcps_sndacks++;
   }
   else if (flags & (TH_SYN|TH_FIN|TH_RST))
      tcpstat.tcps_sndctrl++;
   else if (SEQ_GT(tp->snd_up, tp->snd_una))
      tcpstat.tcps_sndurg++;
   else
      tcpstat.tcps_sndwinup++;

   ti = (struct tcpiphdr *)(m->m_data+sizeof(struct ip)-sizeof(struct ipovly));
   if ((char *)ti < m->pkt->nb_buff)
   {
      panic("tcp_out- packet ptr underflow\n");
   }
   tcp_mbuf = m;        /* flag TCP header mbuf */

#ifdef IP_V6  /* Dual mode code */
   if(so->so_domain == AF_INET6)
   {
      m = mbuf_prepend(m, sizeof(struct ipv6));
      if(m == NULL)
      {
         /* this can happen when we run out of mbufs or pkt buffers
          * That is, mfreeq is empty or (lilfreeq, bigfreeq) are empty.
          * One solution is to find out which one is getting full and
          * then increase them.
          */
         dtrap();             /* This is really rare... */
         m_freem(tcp_mbuf);   /* Free TCP/data chain */
         return ENOBUFS;
      }

      /* strip overlay from front of TCP header */
      tcp_mbuf->m_data += sizeof(struct ipovly);
      tcp_mbuf->m_len -= sizeof(struct ipovly);
   }
#endif   /* end IP_V6 */

   if (tp->t_template == 0)
      panic("tcp_output");

   MEMCPY((char*)ti, (char*)tp->t_template, sizeof(struct tcpiphdr));

   /*
    * Fill in fields, remembering maximum advertised
    * window for use in delaying messages about window sizes.
    * If resending a FIN, be sure not to use a new sequence number.
    */
   if (flags & TH_FIN && tp->t_flags & TF_SENTFIN && 
       tp->snd_nxt == tp->snd_max)
   {
      tp->snd_nxt--;
   }

   ti->ti_seq = htonl(tp->snd_nxt);
   ti->ti_ack = htonl(tp->rcv_nxt);

   /*
    * If we're sending a SYN, check the IP address of the interface
    * that we will (likely) use to send the IP datagram -- if it's
    * changed from what is in the template (as it might if this is
    * a retransmission, and the original SYN caused PPP to start
    * bringing the interface up, and PPP has got a new IP address
    * via IPCP), update the template and the inpcb with the new 
    * address.
    */
   if (flags & TH_SYN)
   {
      struct inpcb * inp;
      inp = (struct inpcb *)so->so_pcb;

      switch(so->so_domain)
      {
#ifdef IP_V4
      case AF_INET:
      {
         ip_addr src;

#ifdef INCLUDE_PPP

         if(((flags & TH_ACK) == 0) && /* SYN only, not SYN/ACK */
            (inp->ifp) &&              /* Make sure we have iface */
            (inp->ifp->mib.ifType == PPP))   /* only PPP type */
         {
            dtrap(); /* remove after confirmed to work in PPP */ 
            src = ip_mymach(ti->ti_dst.s_addr);

         if (src != ti->ti_src.s_addr)
         {
            ti->ti_src.s_addr = src;
            tp->t_template->ti_src.s_addr = src;
            tp->t_inpcb->inp_laddr.s_addr = src;
         }
         }
#endif   /* INCLUDE_PPP */

         /* If this is a SYN (not a SYN/ACK) then set the pmtu */
         if((flags & TH_ACK) == 0)
         {
#ifdef IP_PMTU
            inp->inp_pmtu = pmtucache_get(inp->inp_faddr.s_addr);
#else    /* not compiled for pathmtu, guess based on iface */
            {
               NET ifp;
               /* find iface for route. Pass "src" as nexthop return */
               ifp = iproute(ti->ti_dst.s_addr, &src);
               if(ifp)
                  inp->inp_pmtu = ifp->n_mtu - (ifp->n_lnh + 40);
               else
                  inp->inp_pmtu = 580;  /* Ugh. */
            }
#endif   /* IP_PMTU */
         }
         break;
      }
#endif   /* IP_V4 */

#ifdef IP_V6
      case AF_INET6:
      {
         struct ip6_inaddr * local;
         
         local = ip6_myaddr(&tp->t_inpcb->ip6_faddr, inp->ifp);

         /* If we got a local address & it's not the one in the pcb, then
          * we assume it changed at the iface and fix it in the pcb. Unlike 
          * v4, we don't have an IP header yet, not do we have a template 
          * to worry about.
          */
         if((local) && 
            (!IP6EQ(&local->addr, &tp->t_inpcb->ip6_laddr)))
         {
            IP6CPY(&tp->t_inpcb->ip6_laddr, &local->addr);
         }
         /* If this is a SYN (not a SYN/ACK) then set the pmtu */
         if((flags & TH_ACK) == 0)
         {
            inp->inp_pmtu = ip6_pmtulookup(&inp->ip6_laddr, inp->ifp);
         }
         break;
      }
#endif   /* IP_V6 */
      default:
         dtrap();    /* bad domain setting */
      }
   }

   /* fill in options if any are set */
   if (optlen)
   {
      struct mbuf * mopt;

      mopt = m_getwithdata(MT_TXDATA, MAXOPTLEN);
      if (mopt == NULL) 
      {
         m_freem(m);
         return (ENOBUFS);
      }

      /* insert options mbuf after after tmp_mbuf */
      mopt->m_next = tcp_mbuf->m_next;
      tcp_mbuf->m_next = mopt;

      /* extend options to aligned address */
      while(optlen & 0x03)
         tcp_optionbuf[optlen++] = TCPOPT_EOL;

      MEMCPY(mtod(mopt, char *), tcp_optionbuf, optlen);
      mopt->m_len = optlen;
      /* use portable macro to set tcp data offset bits */
      SET_TH_OFF(ti->ti_t, ((sizeof (struct tcphdr) + optlen) >> 2));
   }

   ti->ti_flags = (u_char)flags;
   /*
    * Calculate receive window. Don't shrink window,
    * but avoid silly window syndrome.
    */
   if (win < (long)(so->so_rcv.sb_hiwat / 4) && win < (long)tp->t_maxseg)
      win = 0;
   if (win < (long)(tp->rcv_adv - tp->rcv_nxt))
      win = (long)(tp->rcv_adv - tp->rcv_nxt);

   /* do check for Iniche buffer limits -JB- */
   if (bigfreeq.q_len == 0)   /* If queue length is 0, set window to 0 */
   {
      win = 0;
   }
   else if(win > (((long)bigfreeq.q_len - 1) * (long)bigbufsiz))
   {
      win = ((long)bigfreeq.q_len - 1) * bigbufsiz;
   }

#ifdef TCP_WIN_SCALE
   if(tp->t_flags & TF_WINSCALE)
   {
      ti->ti_win = htons((u_short)(win >> tp->rcv_wind_scale)); /* apply scale */
   }
   else
#endif /* TCP_WIN_SCALE */
   {
      ti->ti_win = htons((u_short)win);
   }

   if (SEQ_GT(tp->snd_up, tp->snd_nxt)) 
   {
      ti->ti_urp = htons((u_short)(tp->snd_up - tp->snd_nxt));
      ti->ti_flags |= TH_URG;
   }
   else
   {
      /*
       * If no urgent pointer to send, then we pull
       * the urgent pointer to the left edge of the send window
       * so that it doesn't drift into the send window on sequence
       * number wraparound.
       */
      tp->snd_up = tp->snd_una;        /* drag it along */
   }
   /*
    * If anything to send and we can send it all, set PUSH.
    * (This will keep happy those implementations which only
    * give data to the user when a buffer fills or a PUSH comes in.)
    */
   if (len && off+len == (int)so->so_snd.sb_cc)
      ti->ti_flags |= TH_PUSH;

   /*
    * In transmit state, time the transmission and arrange for
    * the retransmit.  In persist state, just set snd_max.
    */
   if (tp->t_force == 0 || tp->t_timer[TCPT_PERSIST] == 0) 
   {
      tcp_seq startseq = tp->snd_nxt;

      /*
       * Advance snd_nxt over sequence space of this segment.
       */
      if (flags & TH_SYN)
         tp->snd_nxt++;

      if (flags & TH_FIN)
      {
         tp->snd_nxt++;
         tp->t_flags |= TF_SENTFIN;
      }
      tp->snd_nxt += len;
      if (SEQ_GT(tp->snd_nxt, tp->snd_max)) 
      {
         tp->snd_max = tp->snd_nxt;
         /*
          * Time this transmission if not a retransmission and
          * not currently timing anything.
          */
         if (tp->t_rttick == 0) 
         {
            tp->t_rttick = cticks;
            tp->t_rtseq = startseq;
            tcpstat.tcps_segstimed++;
         }
      }

      /*
       * Set retransmit timer if not currently set,
       * and not doing an ack or a keep-alive probe.
       * Initial value for retransmit timer is smoothed
       * round-trip time + 2 * round-trip time variance.
       * Initialize shift counter which is used for backoff
       * of retransmit time.
       */
      if (tp->t_timer[TCPT_REXMT] == 0 &&
          tp->snd_nxt != tp->snd_una) 
      {
         tp->t_timer[TCPT_REXMT] = tp->t_rxtcur;
         if (tp->t_timer[TCPT_PERSIST]) 
         {
            tp->t_timer[TCPT_PERSIST] = 0;
            tp->t_rxtshift = 0;
         }
      }
   }
   else
   {
      if (SEQ_GT(tp->snd_nxt + len, tp->snd_max))
         tp->snd_max = tp->snd_nxt + len;
   }

#ifdef DO_TCPTRACE
   tcp_trace("tcp_output: sending, state %d, tcpcb: %x",
    tp->t_state, tp );
#endif

#ifdef MUTE_WARNS
   error = 0;
#endif

#ifdef IP_V4
#ifdef IP_V6     /* Test domain on v4/v6 builds */
   if(so->so_domain != AF_INET6)
#endif   /* IP_V6 */
   {
      struct ip * pip;
   
      pip = mtod(m, struct ip *);
      /* Fill in IP length and send to IP level. */
      pip->ip_len = (u_short)(TCPIPHDRSZ + optlen + len);
      error = ip_output(m, so->so_optsPack);
   }
#endif   /* IP_V4 */
#ifdef IP_V6
   if(so->so_domain == AF_INET6)
   {
      struct ipv6 * pip6;

      /* fill in ipv6 header src & dest from socket info */
      pip6 = (struct ipv6 *)m->m_data;
      IP6CPY(&pip6->ip_src, &tp->t_inpcb->ip6_laddr);
      IP6CPY(&pip6->ip_dest, &tp->t_inpcb->ip6_faddr);

      /* send down to glue layer to IPv6 */
      /* and don't forget the so_optsPack */
      if (tp)
         if (tp->t_inpcb)
			      if (tp->t_inpcb->inp_socket)
                  error = tcp6_send(tp, m, &ti->ti_t,
                     (sizeof(struct ipv6) + sizeof(struct tcphdr) + optlen + len),
                     tp->t_inpcb->inp_socket->so_optsPack);
      			else
                  error = tcp6_send(tp, m, &ti->ti_t,
                    (sizeof(struct ipv6) + sizeof(struct tcphdr) + optlen + len),
                    (struct   ip_socopts *)0);          
   }
#endif   /* IP_V6 */

   if (error)
   {
      if (error == ENOBUFS)   /* ip_output needed a copy buffer it couldn't get */
      {
         if (m->m_type == MT_FREE)  /* ip_output() probably freed first mbuf */
            m = m->m_next;
         m_freem(m); /* free the mbuf chain */
         tcp_quench(tp->t_inpcb);
         return (error);
      }
      return (error);
   }

   /*
    * Data sent (as far as we can tell).
    */

   TCP_MIB_INC(tcpOutSegs);   /* keep MIB stats */
   tcpstat.tcps_sndtotal++;

#ifdef TCP_SACK
   /* If we're doing a sack driven resend then update the sack info.*/
   if(sack_resend)
   {
      /* snd_nxt has been maintined by the above code to reflect 
       * the amount of data space covered by the send, so we use 
       * it to update sack_start.
       */
      if((tp->sack_hole_start[sack_hole] + len) != tp->snd_nxt)
      {
         dtrap(); /* tmp debug ... can this happen? */
      }
      tp->sack_hole_start[sack_hole] = tp->snd_nxt;

      /* If the sack hole is filled, move to next hole */
      if(tp->sack_hole_start[sack_hole] >= tp->sack_hole_end[sack_hole])
      {
         /* set next byte to send to end of hole */
         tp->snd_nxt = tp->sack_hole_end[sack_hole];
         /* Done with hole, flag this by clearing out values */
         tp->sack_hole_start[sack_hole] = tp->sack_hole_end[sack_hole] = 0;
         sack_hole++;      /* move on to next hole */

         /* If next hole has no set data then we are done */
         if(tp->sack_hole_start[sack_hole] == tp->sack_hole_end[sack_hole])
         {
            sendalot = 0;     /* done with sack-based resent */
         }
      }
   }
#endif /* TCP_SACK */

#ifdef TCP_TIMESTAMP
   /* save last ACK to peer for timestamp features */
   if(tp->t_flags & TF_TIMESTAMP)
      tp->last_ack = htonl(ti->ti_ack);
#endif   /* TCP_TIMESTAMP */

   /*
    * If this advertises a larger window than any other segment,
    * then remember the size of the advertised window.
    * Any pending ACK has now been sent.
    */
   if (win > 0 && SEQ_GT(tp->rcv_nxt+win, tp->rcv_adv))
      tp->rcv_adv = tp->rcv_nxt + (unsigned)win;
   tp->t_flags &= ~(TF_ACKNOW|TF_SACKNOW|TF_DELACK);
   if (sendalot)
      goto again;
   return (0);
}



/* FUNCTION: tcp_setpersist()
 * 
 * PARAM1: struct tcpcb *tp
 *
 * RETURNS: NA
 */

void
tcp_setpersist(struct tcpcb * tp)
{
   int   t;

   t = ((tp->t_srtt >> 2) + tp->t_rttvar) >> 1;

   if (tp->t_timer[TCPT_REXMT])
      panic("tcp_output REXMT");
   /*
    * Start/restart persistance timer.
    */
   TCPT_RANGESET(tp->t_timer[TCPT_PERSIST],
    t * tcp_backoff[tp->t_rxtshift],
    TCPTV_PERSMIN, TCPTV_PERSMAX);
   if (tp->t_rxtshift < TCP_MAXRXTSHIFT)
      tp->t_rxtshift++;

}


/* FUNCTION: bld_options()
 *
 * Build tcp options for tp passed in buffer passed. No length checking
 * is done on the buffer.
 *
 * PARAM1: struct tcpcb * tp
 * PARAM1: char * cp         - buffer for building options
 * PARAM3: int flags         - option flags of pkt to send
 *
 * RETURNS: length of option data added to buffer
 */

static int
bld_options(struct tcpcb * tp, u_char * cp, int flags, struct socket * so)
{
   int      len;
   u_short  mss;

   if(tp->t_flags & TF_NOOPT)    /* no options allowed? */
      return 0;

   /* Alway put MSS option on SYN packets */
   if (flags & TH_SYN)
   {
      mss   =  (u_short)tcp_mss(so);

      /* always send MSS option on SYN, fill in MSS parm */
      *(cp + 0) = TCPOPT_MAXSEG;
      *(cp + 1) = MSSOPT_LEN;               /* length byte */
      *(cp + 2)  = (u_char) ((mss & 0xff00) >> 8);
      *(cp + 3)  = (u_char) (mss & 0xff);
      len = 4;
      cp += 4;
   }
   else
      len = 0;

#ifdef TCP_SACK

	IPV6LOG(("+  bld_options SACK tp->t_flags = %x\n", (unsigned int)tp->t_flags));   
	IPV6LOG(("+ bld_options SACK (tp->t_flags & (TF_SACKNOW|TF_SACKOK)) = %x\n", 
		(unsigned int)(tp->t_flags & (TF_SACKNOW|TF_SACKOK))));   
   
   
   /* on SYN and SYN/ACK pkts attach SACK OK option */
   if((flags & TH_SYN) &&
      (so->so_options & SO_TCPSACK))
   {
      *cp++ = TCPOPT_SACKOK;
      *cp++ = 2;  /* length */
      len += 2;
   }

   /* If we should be sending a SACK data option packet, prepare it */
   if((tp->t_flags & (TF_SACKNOW|TF_SACKOK)) == (TF_SACKNOW|TF_SACKOK))
   {
      int tmp;
      tmp = tcp_buildsack(tp, cp);
      len += tmp;
      cp += tmp;
   }
#endif /* TCP_SACK */

#ifdef TCP_WIN_SCALE
   /* TCP Window Scale options only go on packets with SYN bit set */
   if((flags & TH_SYN) &&
      (so->so_options & SO_WINSCALE))
   {
      if(flags & TH_ACK)   /* sending a SYN/ACK */
      {
         if(tp->t_flags & TF_WINSCALE)    /* did he offer to scale? */
         {
            /* On SYN/ACKs, reply to scale offer */
            *cp++ = TCPOPT_WINSCALE ;  /* window scale option */
            *cp++ = 3;                 /* option length */
            *cp++ = tp->rcv_wind_scale;
            len += 3;
         }
      }
      else     /* sending a SYN */
      {
         /* send scale offer with plain SYNs */
         *cp++ = TCPOPT_WINSCALE ;  /* window scale option */
         *cp++ = 3;                 /* option length */
         *cp++ = tp->rcv_wind_scale;
         *cp++ = TCPOPT_NOP;        /* Pad out to 4 bytes. */
         len += 4;
      }
   }
#endif   /* TCP_WIN_SCALE */

#ifdef TCP_TIMESTAMP
   /* 
    * Only add timestamp (RTTM) option IF option flag is set AND
    * it's a SYN packet OR 
    * we've already agreed to use RTTM.
    */
   if((so->so_options & SO_TIMESTAMP) &&
      (((flags & (TH_SYN|TH_ACK)) == TH_SYN)|| 
       (tp->t_flags & TF_TIMESTAMP)))
   {
      *cp = TCPOPT_RTT;     /* window scale option */
      *(cp + 1) = 10;       /* option length */

      /* set set our send time for echo */
      tcp_putseq(cp + 2, cticks);

      /* echo the last time we got from peer */
      tcp_putseq(cp + 6, tp->ts_recent);
      cp += 10;
      len += 10;
   }
#endif   /* TCP_TIMESTAMP */

   USE_ARG(so);
   return len;
}



#endif /* INCLUDE_TCP */


/* end of file tcp_out.c */

