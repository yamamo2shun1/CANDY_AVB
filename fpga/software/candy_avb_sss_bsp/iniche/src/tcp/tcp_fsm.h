/*
 * FILENAME: tcp_fsm.h
 *
 * Copyright 1997-2007 By InterNiche Technologies, Inc. All rights reserved.
 *
 * TCP FSM state definitions.
 * Per RFC793, September, 1981.
 *
 * MODULE: TCP
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


#ifndef _TCP_FSM_H
#define  _TCP_FSM_H  1

#define     TCP_NSTATES    11

#define     TCPS_CLOSED          0  /* closed */
#define     TCPS_LISTEN          1  /* listening for connection */
#define     TCPS_SYN_SENT        2  /* active, have sent syn */
#define     TCPS_SYN_RECEIVED    3  /* have send and received syn */
/* states < TCPS_ESTABLISHED are those where connections not established */
#define     TCPS_ESTABLISHED     4  /* established */
#define     TCPS_CLOSE_WAIT      5  /* rcvd fin, waiting for close */
/* states > TCPS_CLOSE_WAIT are those where user has closed */
#define     TCPS_FIN_WAIT_1      6  /* have closed, sent fin */
#define     TCPS_CLOSING      7  /* closed xchd FIN; await FIN ACK */
#define     TCPS_LAST_ACK        8  /* had fin and close; await FIN ACK */
/* states > TCPS_CLOSE_WAIT && < TCPS_FIN_WAIT_2 await ACK of FIN */
#define     TCPS_FIN_WAIT_2      9  /* have closed, fin is acked */
#define     TCPS_TIME_WAIT       10 /* in 2*msl quiet wait after close */

#define     TCPS_HAVERCVDSYN(s)     ((s)  >= TCPS_SYN_RECEIVED)

/* This macro was changed per customer issue report: 12/23/05
Panic seen in function “dtom” after the 2MSL TCP timer expires
A FIN/ACK is received by the NetStack after the server has unilaterally 
closed the connection. NetStack has already sent a FIN towards the client. 
The FIN/ACK is processed by the TCP layer (tcp_input()) and incorrectly 
sent to the TCP reassembly queue because this should happen only during the 
TCP established state. As a result of this, the reassembly queue pointers 
indicate data is available. The socket is kept until the 2 MSL timer 
expires (about 2 minutes). At that point the clock_tick thread in the 
NetStack performs the cleanup of the socket and the associated tcp control 
block structure. When it tries to free the mbufs in the reassembly queue 
it fails because although the pointers indicate data is present, 
all the mbufs have been freed up at that point. This causes the dtom function to panic. 
 */
#define     TCPS_HAVERCVDFIN(s)     ((s)  >= TCPS_TIME_WAIT)

#ifdef   TCPOUTFLAGS
/*
 * Flags used when sending segments in tcp_output.
 * Basic flags (TH_RST,TH_ACK,TH_SYN,TH_FIN) are totally
 * determined by state, with the proviso that TH_FIN is sent only
 * if all data queued for output is included in the segment.
 */
u_char   tcp_outflags[TCP_NSTATES]  =
{
   TH_RST|TH_ACK,             /* CLOSED */
   0,                         /* LISTEN */
   TH_SYN,                    /* SYN SENT */
   TH_SYN|TH_ACK,             /* SYN RECEIVED */
   TH_ACK,                    /* ESTABLISHED */
   TH_ACK,                    /* CLOSE WAIT */
   TH_FIN|TH_ACK,             /* FIN WAIT 1 */
   TH_FIN|TH_ACK,             /* CLOSING */
   TH_FIN|TH_ACK,             /* LAST ACK */
   TH_ACK,                    /* FIN WAIT 2 */
   TH_ACK,                    /* TIME_WAIT */
};
#endif /* TCPOUTFLAGS */

#endif

/* end of file tcp_fsm.h */

