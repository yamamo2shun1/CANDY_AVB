/*
 * FILENAME: comline.h
 *
 * Copyright  2000 By InterNiche Technologies Inc. All rights reserved
 *
 * Definitions for comm lines to be used under PPP and SLIP.
 *
 * MODULE: INET
 *
 * PORTABLE: yes
 */

/* Additional Copyrights: */
/* Portions Copyright 1994, 1995 by NetPort Software. */


#ifndef _COMLINE_H_
#define  _COMLINE_H_

#include "netbuf.h"

/* Values for Line State ln_state. */

typedef enum  {
   LN_INITIAL,       /* not set up yet */
   LN_DISCONNECTED,  /* line not connected (idle) */
   LN_AUTOANS,       /* line ready (e.g. mode in autoanswer mode) */
   LN_CONNECTING,    /* line is connecting (e.g. modem dialing) */
   LN_CONNECTED,
   LN_DISCONNECTING, /* e.g. modem in the process of hanging up */
   LN_BROKEN,        /* hardware is missing, misconfigured, or broken */
   LN_RESETTING      /* line is being reset - ignore incoming chars */
} ln_states;


/* API structure for PPP and SLIP line IO. One of these structures is
 * created for each instance where a line layer, (UART, modem, slip, PPP,
 * etc.) interfaces with another line layer. Example: PPP over AT modem
 * over RS232 would have two of these, one between PPP and modem and
 * another between modem and RS232 UART.
 */

struct com_line
{
   /* bring/check line up */
   int   (*ln_connect)(struct com_line * lineptr);

   /* disconnect the line */
   int   (*ln_disconnect)(struct com_line *);

   /* one of the send routines (the next two) may be NULL */
   int   (*ln_putc)(struct com_line *, int byte);   /* send single char */
   int   (*ln_write)(struct com_line *, PACKET pkt);

   /* speed and state of the lower module */
   long        ln_speed;      /* most recent detected speed */
   ln_states   ln_state;

   int   (*ln_getc)(struct com_line *, int byte);   /* receive single char */

   /* types for the layers above and below this interface  */
   int   upper_type;
   int   lower_type;

   void * upper_unit;         /* depends on upper_type, usually M_PPP */
   int   lower_unit;          /* legacy ID for lower (UART level) drivers */
};

typedef struct com_line * LINEP;

/* Values for upper_type and lower_type. Note that a modem could
 * be both the lower layer to PPP code and upper layer to a
 * UART driver.
 */

#define  LN_PPP      1     /* upper layer is PPP */
#define  LN_SLIP     2     /* upper layer is SLIP */
#define  LN_UART     3     /* lower layer is a UART */
#define  LN_ATMODEM  4     /* upper/lower layer is a modem */
#define  LN_PPPOE    5     /* lower layer is PPPOE */
#define  LN_LBXOVER  6     /* lower is loopback crossover (for test) */
#define  LN_PORTSET  7     /* (init) lower will be set by callback */


/* UARTs under this line struct should support this API: */

/* logical line operations on UARTs: */
extern   int   ln_uinit(LINEP line);        /* assigned unit, sets up structs */
extern   int   ln_uconnect(LINEP line);      /* move to connected state */
extern   int   ln_udisconnect(LINEP line);   /* move to disconnected state */
extern   int   ln_uputc(LINEP line, int byte);   /* send a single byte */


/* lowest level UART API */
extern   int   uart_init(int unit);   /* sets up hardware */
extern   int   uart_putc(int unit, u_char);   /* send character to comm port */
extern   int   uart_getc(int unit);    /* read next buffered char from com port */
extern   int   uart_ready(int unit);   /* returns TRUE if uart is ready to send a char */
extern   void  uart_close(int unit);   /* undoes uart_init */
extern   int   uart_stats(void * pio, int unit);

/* Struct contaning all configuration global params for Comport */

#ifdef USE_COMPORT
struct ComPortCfg
{
   int   comport;             /* PC comm port to default to */
   int   LineProtocol;        /* 1=PPP, 2=SLIP */
};

extern struct ComPortCfg comportcfg;
#endif   /* USE_COMPORT */

#endif   /* _COMLINE_H_ */


