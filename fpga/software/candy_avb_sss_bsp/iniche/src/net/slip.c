/*
 * FILENAME: slip.c
 *
 * Copyright 1997 - 2004 By InterNiche Technologies Inc. All rights reserved
 *
 * Slip support for 
 * frame oriented UARTS such as Motorola MC68302 UART. This should 
 * work on any frame-oriented UART system with only minor 
 * modifications.
 *
 * MODULE: INET
 *
 * ROUTINES: slipstrip(), slipstuff(), 
 *
 * PORTABLE: yes
 */

/* Additional Copyrights: */
/* Portions Copyright 1994, 1995 by NetPort Software */

#include "ipport.h"

#ifdef USE_SLIP   /* whole file can be ifdeffed out */

#include "slip.h"
#include "slipport.h"

u_char   slipbuf[SLIPBUFSIZ];



/* FUNCTION: slipstrip()
 *
 * slipstrip() - Take a received block of bytes (a frame) and strip 
 * out the slip escape characters. This is for UARTs which can be 
 * programmed to interupt only on receipt of a pre-framed block of 
 * bytes. This logic assumes all frames are a single slip frame. This 
 * routine scans the passed buffer and does the reverse byte-stuffing 
 * to convert the SLIP frame into a normal IP frame, ready to be 
 * passed to the IP entry point - ip_rcv() on the NetPort stack. 
 * Caller may want to verify the first byte in the buffer is the 
 * first byte of a valid IP packet before passing to IP. Returns a 
 * pointer to start of stripped frame, usually 1 byte after the 
 * pointer passed. also returns the new length in the length variable 
 * passed. 
 *
 * 
 * PARAM1: u_char * bytes
 * PARAM2: unsigned * buflen
 *
 * RETURNS: Returns NULL if a problem with the frame was detected. 
 */

char *   
slipstrip(u_char * bytes, unsigned * buflen)
{
   char *   newhead; /* new start of packet */
   u_char * cp;      /* scratch pointer */
   unsigned length = *buflen; /* scratch length */

   cp = bytes;
   newhead = (char*)cp;

   /* this tests for an RFC recommended enhancement */
   if (*cp == SL_END)   /* 1st char used to clear buffer? */
   {
      length--;
      newhead++;  /* point past leading frame marker */
      cp++;
   }

#ifdef FAST_SLIP
   /* if there are no slip ESCAPE chars, take fast path: */
   if (MEMCHR(cp, SL_ESC, length) == NULL)
   {
      *buflen = length - 1;   /* truncate END byte */
      return(newhead);
   }
#endif   /* FAST_SLIP */

   /* scan frame byte by byte for SLIP special chars */
   while (length--)
   {
      switch (*cp)
      {
      case SL_END:   /* end of frame */
         *buflen = cp - (u_char*)newhead;
         return newhead;
      case SL_ESC:   /* got a "stuffed" byte */
         if (*(cp+1) == SL_ESC_ESC) /* stuffed an esc */
         {
            *cp = SL_ESC;
            MEMMOVE(cp+1, cp+2, length);  /* move up rest of data */
         }
         else if(*(cp+1) == SL_ESC_END)   /* stuffed an end */
         {
            *cp = SL_END;
            MEMMOVE(cp+1, cp+2, length);  /* move up rest of data */
         }
      break;
         default:
      break;
      }
      cp++;
   }
   return NULL;   /* got through whole length without an SL_END char */
}

/* FUNCTION: slipstuff()
 *
 * slipstuff(buf, len) - the inverse of slipstrip(). This accepts a 
 * raw IP packet bound for the SLIP port and does the required byte 
 * stuffing. It checks that the buffer passed is big enough for the 
 * result, which will be several bytes larger than the frame passed. 
 * If the FAST_SLIP compile option is used, it further assumes the 
 * buffer passed has a byte prepended for an SL_END char. Returns 
 * pointer to new packet if OK, usually 1 byte less than the buffer 
 * passed. datalen has new number of bytes in the packet if OK. 
 *  
 * 
 * PARAM1: u_char * buf
 * PARAM2: unsigned * datalen
 * PARAM3: unsigned buflen
 *
 * RETURNS: Returns NULL if the passed buffer was not big enough.
 */

char *   
slipstuff(u_char * buf, unsigned * datalen, unsigned buflen)
{
   u_char * inbuf;
   u_char * outbuf;
   unsigned newlength;
   unsigned dlength;    /* scratch length holder */

   dlength = *datalen;

#ifdef FAST_SLIP
   /* if there are no slip ESCAPE chars, take fast path: */
   if (MEMCHR(buf, SL_ESC, dlength) == NULL)
   {
      if (MEMCHR(buf, SL_END, dlength) == NULL)
      {
         *(buf + dlength) = SL_END; /* new END char at back */
         buf--;
         *buf = SL_END; /* prepend END byte at front */
         *datalen += 2; /* adjust return length for 2 added bytes */
         return (char*)buf;
      }
   }
#endif   /* FAST_SLIP */

   MEMCPY(slipbuf, buf, dlength);      /* make a copy of buffer */
   inbuf = slipbuf;
   *buf = SL_END;    /* start packet with an END char, per RFC 1055 */
   outbuf = buf+1;
   newlength = 1 + dlength;

   while (dlength--)
   {
      switch (*inbuf)
      {
      case SL_END:   /* replace "END" chars with ESC_ENDs */
         *outbuf++ = SL_ESC;
         *outbuf++ = SL_ESC_END;
         newlength += 1;
      break;
      case SL_ESC:   /* replace imbedded escapes with ESC_ESCs */
         *outbuf++ = SL_ESC;
         *outbuf++ = SL_ESC_ESC;
         newlength += 1;
      break;
         default:    /* not a special char, just move it. */
         *outbuf++ = *inbuf;  /* inbuf is bumped below */
      }
      inbuf++;

      /* make sure we don't overwrite end of buffer */
      if (newlength > (buflen - (SLIPHDR_SIZE + SLIPHDR_BIAS + 1)))
      {
#ifdef NPDEBUG
         dprintf("slipstuff overflow: %d\r\n", newlength);
#endif
         return NULL;
      }
   }
   /* now add the END char to end */
   *outbuf = SL_END;
   newlength++;

   *datalen = newlength;   /* return length of "stuffed" packet */
   return (char*)buf;      /* return pointer to packet */
}

#endif   /* USE_SLIP */
/* end of file slip.c */



