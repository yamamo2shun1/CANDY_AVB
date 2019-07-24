/*
 * FILENAME: tcpip.h
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

#ifndef _TCPIP_H
#define  _TCPIP_H 1

/*
 * Tcp/Ip-like header, after ip options removed. First part of IPv4
 * header if overwritten with double linked list links.
 */

#ifndef IP_V6
struct ip6_addr;  /* predecl for v4 only builds */
#endif

struct ipovly
{
   struct ipovly *   ih_next;    /* list link */
   struct ipovly *   ih_prev;    /* list link */
   u_short  padword;             /* unused - for compilers that don't pad structures */
   u_short  ih_len;              /* protocol length */
   struct   in_addr  ih_src;     /* source internet address */
   struct   in_addr  ih_dst;     /* destination internet address */
};

struct tcpiphdr
{
     struct  ipovly ti_i; /* overlaid ip structure */
     struct tcphdr  ti_t; /* tcp header */
};

/* The following definition is somewhat mis-named, since it really
 * indicates the size of the overlay structure, not the IP head.
 * on 32 bit IPv4 systems this is usually the same, but on IPv6 or
 * systems with poiunters which are not 32 bits these sizes may
 * NOT be the same.
 */
#ifdef IP_V6
#define TCPIPHDRSZ (sizeof (struct tcpiphdr))
#else
#define TCPIPHDRSZ 40
#endif

#define  ti_next  ti_i.ih_next
#define  ti_prev  ti_i.ih_prev
#define  ti_x1    ti_i.ih_x1
#define  ti_pr    ti_i.ih_pr
#define  ti_len   ti_i.ih_len
#define  ti_src   ti_i.ih_src
#define  ti_dst   ti_i.ih_dst
#define  ti_sport ti_t.th_sport
#define  ti_dport ti_t.th_dport
#define  ti_seq   ti_t.th_seq
#define  ti_ack   ti_t.th_ack

/* For NetPort/InterNiche, omit bitfields and use SET_TH_OFF()
 * and GET_TH_OFF macros
 */
/* #define ti_x2 ti_t.th_x2 #define ti_off ti_t.th_off 
 */

#define  ti_flags ti_t.th_flags
#define  ti_win   ti_t.th_win
#define  ti_sum   ti_t.th_sum
#define  ti_urp   ti_t.th_urp

#endif   /* _TCPIP_H */

/* end of file tcpip.h */


