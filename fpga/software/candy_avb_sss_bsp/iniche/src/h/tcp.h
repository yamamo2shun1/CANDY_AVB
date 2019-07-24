/*
 * FILENAME: tcp.h
 *
 * Copyright 1997- 2000 By InterNiche Technologies Inc. All rights reserved
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

#ifndef _TCP_H
#define  _TCP_H   1

#ifndef BYTE_ORDER
Error - BYTE_ORDER must be defined, usually in ..\inet\ipport.h
#endif


typedef   u_long   tcp_seq;

/*
 * TCP header.
 * Per RFC 793, September, 1981.
 */

struct tcphdr
{
   u_short  th_sport;      /* source port */
   u_short  th_dport;      /* destination port */
   tcp_seq  th_seq;        /* sequence number */
   tcp_seq  th_ack;        /* acknowledgement number */

   u_char   th_doff;       /* data offset: high 4 bits only */

   /* these macros get/set the raw value, usually 5 */
#define  GET_TH_OFF(th) (th.th_doff >> 4)
#define  SET_TH_OFF(th, off)  (th.th_doff =  (u_char)(off   << 4))

   u_char   th_flags;
#define     TH_FIN   0x01
#define     TH_SYN   0x02
#define     TH_RST   0x04
#define     TH_PUSH     0x08
#define     TH_ACK   0x10
#define     TH_URG   0x20
   u_short     th_win;     /* window */
   u_short     th_sum;     /* checksum */
   u_short     th_urp;     /* urgent pointer */
};

#define  TCPOPT_EOL        0
#define  TCPOPT_NOP        1
#define  TCPOPT_MAXSEG     2
#define  TCPOPT_WINSCALE   3
#define  TCPOPT_SACKOK     4
#define  TCPOPT_SACKDATA   5
#define  TCPOPT_RTT        8

#endif   /* _TCP_H */

/* end of file tcp.h */


