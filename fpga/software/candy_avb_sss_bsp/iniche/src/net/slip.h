/*
 * FILENAME: slip.h
 *
 * Copyright 1997 - 2004 By InterNiche Technologies Inc. All rights reserved
 *
 * Slip support
 *
 * MODULE: INET
 *
 *
 * PORTABLE: yes
 */

/* Additional Copyrights: */

/* Portions Copyright 1994, 1995 by NetPort Software */

#ifndef _SLIP_H_
#define  _SLIP_H_

#include "comline.h"

/* standard defines. These are decimal versions of the 
values given in octal in the slip RFC. */
#define  SL_ESC         219
#define  SL_END         192
#define  SL_ESC_END     220
#define  SL_ESC_ESC     221

/* MAX size of SLIP packet. See RFC before modifying */
#define  SLIPSIZ        1006

/* MAX size of SLIP Buffer. This has to accomodate 1006 bytes excluding
 * the ESCAPE characters. So assuming the 1006 frame required by the RFC
 * is full of escape characters then the min Buffer Size to guarantee
 * reception and transmission would be 2012 bytes. However just for the
 * sake of rounding off 2048 has been selected.
 */
#define  SLIPBUFSIZ     2048

extern   u_char   slipbuf[SLIPBUFSIZ];
extern   u_char   slipoutbuf[SLIPBUFSIZ];
extern   u_char   slipinbuf[SLIPBUFSIZ];

#define  SLIPHDR_SIZE   1

/* Set this is to 3 to force alignment of pkt->nb_prot on a 4 byte
 * boundary so that IP and TCP headers meet alignment requriements
 * of architectures such as ARM, NEC, etc.
 *
 * To force alignment on ARM, NEC, etc. set SLIPHDR_BIAS to 3.
 */
#ifndef SLIPHDR_BIAS
#define SLIPHDR_BIAS    3
#endif

struct slip_errors
{
   unsigned    serr_noends;         /* didn't find SL_END where expected */
   unsigned    serr_overflow;       /* frame overflowed buffer */
   unsigned    serr_connect;        /* loer level connect failed */
   unsigned    serr_nobuff;         /* buffer alloc failed */
   unsigned    serr_noframe;        /* got char outside of SLIP frame */
   unsigned    serr_iphdr;          /* SLIP data was not IP packet */
   unsigned    serr_c0noise;        /* Line noise or Peer Bug introdues spurious 0xc0 */
};

/* slip internal utilites */
char *   slipstrip(u_char * bytes, unsigned * length);
char *   slipstuff(u_char * buf, unsigned * pkt_len, unsigned buf_len);

/* slip byte & frame receive entry points */
int   slip_rxframe(int line, u_char * buffer, unsigned length);
int   slip_rxbyte(struct com_line * line, int byte);

#define  SLIP_TEST_HEADER     1  /* this tests for an RFC recommended enhancement */

#endif   /* _SLIP_H_ */


