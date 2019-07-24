/*
 * FILENAME: slipport.h
 *
 * Copyright 1997 - 2004 By InterNiche Technologies Inc. All rights reserved
 *
 *
 * SLIP port file for most demo builds. Maps serial line 
 * calls to Comm port calls or to Hayes modem dialer.
 *
 * MODULE: INET
 *
 *
 * PORTABLE: no
 */


#include "ipport.h"

#include UART_INCLUDE

#define  _NSLIP   1  /* number of slip lines to support */

#ifdef USE_MODEM
#include "../modem/mdmport.h"
#endif

extern   struct slip_errors   slip_error[_NSLIP];

extern   int slip_byte(int unit, int byte);  /* input from uart */

#define  FAST_SLIP   1  /* optimize SLIP en/decoding w/ memchr() */

#ifdef FAST_SLIP
#define  MEMCHR(buf, chr,  len)  memchr(buf, chr,  len)
#endif   /* FAST_SLIP */


/* map slip line driver routine macros to actual routines */

#define  SLIP_PCUART 1


#ifdef SLIP_MODEM /* do slip over Hayes dialup modem */
#define  SLIP_connect   modem_connect
#define  SLIP_hangup    modem_hangup
#define  SLIP_write     NULL
#define  SLIP_putc      modem_putc
#define  SLIP_state     modem_lstate
#define  SLIP_speed     modem_speed
#define  SLIP_init      modem_init
#endif

#ifdef SLIP_PCUART   /* do slip on raw PC uart */
#define  SLIP_connect    ln_uconnect
#define  SLIP_disconnect ln_udisconnect
#define  SLIP_write      NULL
#define  SLIP_putc       ln_uputc
#define  SLIP_state      ln_ustate
#define  SLIP_speed      ln_uspeed
#define  SLIP_init       ln_uinit
#endif

