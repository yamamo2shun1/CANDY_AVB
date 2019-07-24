/*
 * FILENAME: in_pcb.h
 *
 * Copyright 1997-2007 By InterNiche Technologies, Inc. All rights reserved.
 *
 *
 * MODULE: INET
 *
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

#ifndef _IN_PCB_H
#define  _IN_PCB_H   1

/*
 * Common structure pcb for internet protocol implementation.
 * Here are stored pointers to local and foreign host table
 * entries, local and foreign socket numbers, and pointers
 * up (to a socket structure) and down (to a protocol-specific)
 * control block.
 */

struct inpcb
{
   struct   inpcb *  inp_next, *inp_prev;   /* list links */
   struct   inpcb *  inp_head;      /* chain of inpcb's for this protocol */

   /* The same connection PCB is used for IPv4 and IPv6. Only one of the
    * two following address pairs will be set, depending on the
    * inp->socket->domain field (AF_INET or AF_INET6).
    */
   struct   in_addr  inp_faddr;  /* foreign host table entry */
   struct   in_addr  inp_laddr;  /* local host table entry */

#ifdef IP_V6
   ip6_addr ip6_faddr;           /* foreign host address */
   ip6_addr ip6_laddr;           /* local host address (should be on ifp) */
   ip6_addr nexthop6;            /* next hop if (ifp is set) */
#endif   /* IP_V6 */

   int      inp_flags;           /* mask of INPF_XXX bits (defined below) */
   int      inp_pmtu;            /* Path MTU (max dgram size) */
   u_short  inp_fport;           /* foreign port */
   u_short  inp_lport;           /* local port */
   struct   socket * inp_socket; /* back pointer to socket */
   char *   inp_ppcb;            /* pointer to per-protocol (TCP, UDP) pcb */
   NET      ifp;                 /* interface if connected */
};

#define     INPLOOKUP_WILDCARD   1
#define     INPLOOKUP_SETLOCAL   2

#define     INPF_ROUTESET    0x01     /* nexthop and ifp are set */

#define     sotoinpcb(so)     ((struct inpcb *)(so)->so_pcb)

extern   int   in_pcballoc __P ((struct socket *, struct inpcb *));
extern   int   in_pcbbind  __P ((struct inpcb *, struct mbuf *));
extern   int   in_pcbconnect  __P ((struct inpcb *, struct mbuf *));
extern   void  in_pcbdisconnect  __P ((struct inpcb *));
extern   void  in_pcbdetach   __P ((struct inpcb *));
extern   void  in_setsockaddr __P ((struct inpcb *, struct mbuf *));
extern   void  in_setpeeraddr __P ((struct inpcb *, struct mbuf *));
extern   void  in_losing   __P ((struct inpcb *));
extern   void  in_rtchange __P ((struct inpcb *));

extern   struct inpcb * in_pcblookup   __P ((struct inpcb *, 
            u_long,  unshort, u_long,  unshort, int));
extern   void  in_pcbnotify   __P ((struct inpcb *, struct in_addr *, 
   int, void (*) (struct inpcb *)));

#ifdef IP_V6
/* IPv6 version of some of these routines */
extern   int   ip6_pcbbind  __P ((struct inpcb *, struct mbuf *));
extern   struct inpcb * ip6_pcblookup   __P ((struct inpcb *, 
            ip6_addr *,  unshort, ip6_addr *,  unshort, int));
extern   int   ip6_pcbconnect(struct inpcb * inp, struct mbuf *  nam);
extern   void  ip6_pcbdisconnect(struct inpcb * inp);
extern   void  ip6_setsockaddr(struct inpcb *, struct mbuf * nam);
extern   void  ip6_setpeeraddr(struct inpcb *,struct mbuf *  nam);
#endif   /* IP_V6 */


#endif   /* _IN_PCB_H */

/* end of file in_pcb.h */


