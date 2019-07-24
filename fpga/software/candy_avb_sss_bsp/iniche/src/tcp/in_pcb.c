/*
 * FILENAME: in_pcb.c
 *
 * Copyright 1997-2007 By InterNiche Technologies Inc. All rights reserved.
 *
 *
 * MODULE: INET
 *
 * ROUTINES: in_pcballoc(), in_pcbbind(), ip6_pcbbind(), in_pcbconnect(), 
 * ROUTINES: in_pcbdisconnect(), in_pcbdetach(), in_setsockaddr(), 
 * ROUTINES: in_setpeeraddr(), in_pcblookup(), 
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
#include "protosw.h"
#include "in_pcb.h"

#ifdef INCLUDE_TCP  /* include/exclude whole file at compile time */

/* 2/9/00 - modified in_pcblookup() put the matched pcb at the head 
 * of the list to speed future searches. The stats below are to gauge 
 * the success of this.
 */
long     inpcb_cachehits   =  0;
long     inpcb_cachemiss   =  0;


/* FUNCTION: in_pcballoc()
 *
 * Allocate a protocol control block and prepare it for use.
 *
 * PARAM1: struct socket *so
 * PARAM2: struct inpcb *head
 *
 * RETURNS: PCB if OK, NULL if out of memory.
 */

int
in_pcballoc(struct socket * so, 
   struct inpcb * head)
{
   struct inpcb * inp;

   inp = INP_ALLOC (sizeof (*inp));
   if (inp == 0)
      return ENOMEM;
   inp->inp_head = head;
   inp->inp_socket = so;

   /* Set Path MTU to a very small default. It should get expanded 
    * later by v4 or v6 specific SYN code. We don't want it zero 
    * in case it doesn't get expanded promptly.
    */
   inp->inp_pmtu = 512;
   insque(inp, head);
   so->so_pcb = inp;
   return 0;
}


/* FUNCTION: in_pcbdetach()
 * 
 * PARAM1: struct inpcb *inp
 *
 * RETURNS: 
 */

void
in_pcbdetach(struct inpcb * inp)
{
   struct socket *   so =  inp->inp_socket;

   so->so_pcb = 0;
   sofree(so);
   remque(inp);
   INP_FREE (inp);
}



#ifdef IP_V4   /* The rest of this file are specific to v4 */

/* FUNCTION: in_pcbbind()
 * 
 * Guts of socket "bind" call for TCP.
 *
 * PARAM1: struct inpcb *inp
 * PARAM2: struct mbuf *nam
 *
 * RETURNS: 0 if OK, else one of the socket error codes
 */


int
in_pcbbind(
   struct inpcb * inp,
   struct mbuf *  nam)
{
   struct socket *   so =  inp->inp_socket;
   struct inpcb * head  =  inp->inp_head;
   struct sockaddr_in * sin;
   u_short  lport =  0;


   if (inp->inp_lport || inp->inp_laddr.s_addr != INADDR_ANY)
      return (EINVAL);
   if (nam == 0)
      goto noname;
   sin = mtod(nam, struct sockaddr_in *);

   /*
    * removed test here for "if (nam->m_len != sizeof (*sin))"
    * since it really complicatges supporting dual IPv4/v6, and 
    * the 2.0 stack now checks this in t_bind(). -JB-
    */
   if (sin->sin_addr.s_addr != INADDR_ANY) 
   {
      if (ip_mymach(sin->sin_addr.s_addr) != sin->sin_addr.s_addr)
      return (EADDRNOTAVAIL);
   }
   lport = sin->sin_port;
   if (lport) 
   {
      int   wild  =  0;

      /* even GROSSER, but this is the Internet */
      if ((so->so_options & SO_REUSEADDR) == 0 &&
          ((so->so_proto->pr_flags & PR_CONNREQUIRED) == 0 ||
          (so->so_options & SO_ACCEPTCONN) == 0))
      {
         wild = INPLOOKUP_WILDCARD;
      }
      if (in_pcblookup(head,
          0L, 0, sin->sin_addr.s_addr, lport, wild))
      {
         return (EADDRINUSE);
      }
   }
   inp->inp_laddr = sin->sin_addr;
noname:
   if (lport == 0)
   {
      do 
      {
         if (head->inp_lport++ < IPPORT_RESERVED ||
             head->inp_lport > IPPORT_USERRESERVED)
         {
            head->inp_lport = IPPORT_RESERVED;
         }
         lport = htons(head->inp_lport);
      } while(in_pcblookup(head, 0L, 0, inp->inp_laddr.s_addr, lport, 0));
   }
   inp->inp_lport = lport;
   return (0);
}


/* FUNCTION: in_pcbconnect()
 *
 * Connect from a socket to a specified address.
 * Both address and port must be specified in argument sin.
 * If don't have a local address for this socket yet,
 * then pick one.
 * 
 * PARAM1: struct inpcb *inp
 * PARAM2: struct mbuf *nam
 *
 * RETURNS: 
 */

int
in_pcbconnect(struct inpcb * inp, 
   struct mbuf *  nam)
{
   unsigned long ifaddr;
   struct sockaddr_in * sin   =  mtod(nam,   struct sockaddr_in *);

   if (nam->m_len < sizeof (*sin))
      return (EINVAL);
   if (sin->sin_family != AF_INET)
      return (EAFNOSUPPORT);
   if (sin->sin_port == 0)
      return (EADDRNOTAVAIL);
   /*
    * If the destination address is INADDR_ANY,
    * use the primary local address.
    * If the supplied address is INADDR_BROADCAST,
    * and the primary interface supports broadcast,
    * choose the broadcast address for that interface.
    */
   if (sin->sin_addr.s_addr == INADDR_ANY)
   {
      if (inp && inp->ifp)
         sin->sin_addr.s_addr = inp->ifp->n_ipaddr;
      else
         return (EADDRNOTAVAIL);
   }
   else if (sin->sin_addr.s_addr == INADDR_BROADCAST)
      return (EADDRNOTAVAIL);


   if (inp->inp_laddr.s_addr == INADDR_ANY) 
   {
#ifdef MULTI_HOMED
      ip_addr hop1;     /* dummy for pass to iproute() */
      NET npnet;     /* the netport iface we can send on */
      /* call netport stack's IP routing */
      npnet = iproute(sin->sin_addr.s_addr, &hop1);
      if (!npnet)
         return EADDRNOTAVAIL;
      ifaddr = npnet->n_ipaddr;  /* local address for this host */
#else    /* not netport MULTI_HOMED, use 0th (only) iface */
      ifaddr = nets[0]->n_ipaddr;
#endif   /* MULTI_HOMED */
   }
   else  /* inp->inp_laddr.s_addr != INADDR_ANY */
      ifaddr = inp->inp_laddr.s_addr;  /* use address passed */

   if (in_pcblookup(inp->inp_head,
       sin->sin_addr.s_addr,
       sin->sin_port,
       ifaddr,
       inp->inp_lport,
       0))
   {
      return (EADDRINUSE);
   }
   if (inp->inp_laddr.s_addr == INADDR_ANY) 
   {
      if (inp->inp_lport == 0)
         (void)in_pcbbind(inp, (struct mbuf *)0);
      inp->inp_laddr.s_addr = ifaddr;
   }
   inp->inp_faddr = sin->sin_addr;
   inp->inp_fport = sin->sin_port;
   return 0;
}



/* FUNCTION: in_pcbdisconnect()
 * 
 * PARAM1: struct inpcb *inp
 *
 * RETURNS: 
 */

void
in_pcbdisconnect(struct inpcb * inp)
{

   inp->inp_faddr.s_addr = INADDR_ANY;
   inp->inp_fport = 0;
   if (inp->inp_socket->so_state & SS_NOFDREF)
      in_pcbdetach (inp);
}

/* FUNCTION: in_setsockaddr()
 * 
 * PARAM1: struct inpcb *inp
 * PARAM2: struct mbuf *nam
 *
 * RETURNS: 
 */

void
in_setsockaddr(struct inpcb * inp, 
   struct mbuf *  nam)
{
   struct sockaddr_in * sin;

   nam->m_len = sizeof (*sin);
   sin = mtod(nam, struct sockaddr_in *);
   MEMSET(sin, 0, sizeof (*sin));
   sin->sin_family = AF_INET;
   sin->sin_port = inp->inp_lport;
   sin->sin_addr = inp->inp_laddr;
}



/* FUNCTION: in_setpeeraddr()
 * 
 * PARAM1: struct inpcb *inp
 * PARAM2: struct mbuf *nam
 *
 * RETURNS: 
 */

void
in_setpeeraddr(
   struct inpcb * inp,
   struct mbuf *  nam)
{
   struct sockaddr_in * sin;

   nam->m_len = sizeof (*sin);
   sin = mtod(nam, struct sockaddr_in *);
   MEMSET(sin, 0, sizeof (*sin));
   sin->sin_family = AF_INET;
   sin->sin_port = inp->inp_fport;
   sin->sin_addr = inp->inp_faddr;
}



/* FUNCTION: in_pcblookup()
 *
 * Find a TCP connection in the passed list which matches the
 * parameters passed.
 *
 * PARAM1: struct inpcb *head
 * PARAM2: u_long faddr
 * PARAM3: unshort xfport
 * PARAM4: u_long laddr
 * PARAM5: unshort xlport
 * PARAM6: int flags
 *
 * RETURNS: NULL if not found, else entry in inpcb list.
 */


struct inpcb * 
in_pcblookup(struct inpcb * head, 
   u_long   faddr, 
   unshort  xfport,
   u_long   laddr,
   unshort  xlport,
   int   flags)
{
   struct inpcb * inp, *   match =  0;
   unshort  fport =  xfport; 
   unshort  lport =  xlport;
   int   matchwild   =  3;
   int   wildcard;

   for (inp = head->inp_next; inp != head; inp = inp->inp_next) 
   {
      if (inp->inp_lport != lport)
         continue;

      /* Skip non IPv4 sockets */
      if(inp->inp_socket->so_domain != AF_INET)
         continue;

      wildcard = 0;
      if (inp->inp_laddr.s_addr != INADDR_ANY) 
      {
         if (laddr == INADDR_ANY)
            wildcard++;
         else if (inp->inp_laddr.s_addr != laddr)
            continue;
      }
      else 
      {
         if (laddr != INADDR_ANY)
            wildcard++;
      }
      if (inp->inp_faddr.s_addr != INADDR_ANY) 
      {
         if (faddr == INADDR_ANY)
            wildcard++;
         else if (inp->inp_faddr.s_addr != faddr ||
             inp->inp_fport != fport)
         {
            continue;
         }
      } else 
      {
         if (faddr != INADDR_ANY)
            wildcard++;
      }
      if (wildcard && (flags & INPLOOKUP_WILDCARD) == 0)
         continue;
      if (wildcard < matchwild) 
      {
         match = inp;
         matchwild = wildcard;
         if (matchwild == 0)
            break;
      }
   }
   if (match == NULL)
      return match;

   if (head->inp_next == match)  /* got cache hit? */
   {
      inpcb_cachehits++;
   }
   else
   {
      inpcb_cachemiss++;
      /* "cache" the match to be first checked next time. */
      match->inp_next->inp_prev = match->inp_prev; /*unlink match */
      match->inp_prev->inp_next = match->inp_next;

      /* relink match as head->inp_next */
      match->inp_next = head->inp_next;
      head->inp_next = match;
      match->inp_prev = head;
      match->inp_next->inp_prev = match;
   }
   return (match);
}
#endif /* IP_V4 */

#endif /* INCLUDE_TCP */

