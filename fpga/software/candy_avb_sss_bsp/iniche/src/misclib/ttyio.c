/*
 * FILENAME: ttyio.c
 * 
 * Copyright 1997- 2006 By InterNiche Technologies Inc. All rights reserved
 *
 * Portions Copyright 1986 by Carnegie Mellon
 * Portions Copyright 1984 by the Massachusetts Institute of Technology
 *
 * Copyright (c) 1982, 1986, 1988 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation and other 
 * materials related to such distribution and use acknowledge that 
 * the software was developed by the University of California, Berkeley.
 * The name of the University may not be used to endorse or promote 
 * products derived from this software without specific prior written 
 * permission. THIS SOFTWARE IS PROVIDED "AS IS" AND WITHOUT ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 *
 * Rights, responsibilities and use of this software are controlled by
 * the agreement found in the "LICENSE.H" file distributed with this
 * source code.  "LICENSE.H" may not be removed from this distribution,
 * modified, enhanced nor references to it omitted.
 *
 * This printf currently understands: %d, %x, %u, %ld, %lx, %s, %c, %p.
 * %lu is treated as %ld; This only works for values up to 2147483647.
 *
 * This code is is NOT reentrant or sharable. It 
 * is designed for DOS TSRs, device drivers, etc. not for a system 
 * library. If you are working on an embedded system which already 
 * supports a printf, you should try to use that. Field width support 
 * is switchable via FIELDWIDTH below.
 *
 * Numerics are not trucated to fit field widths.
 *
 * MODULE: MISCLIB
 *
 * ROUTINES: dprintf(), printf(), sprintf(), snprintf(), vsnprintf(),
 * ROUTINES: doprint(), hexword(), hexbyte(), declong(),
 * ROUTINES: setnum(), fplen()
 *
 * PORTABLE: yes
 */

#define  _IN_TTYIO_  1  /* switch to help out ipport.h */

#include "ipport.h"

/* if target should not use target build environment's printf */
#ifndef NATIVE_PRINTF

#ifdef PRINTF_STDARG
#include <stdarg.h>
#endif

#define  FIELDWIDTH     1        /* flag to switch in Field Width code */
#define  LINESIZE       144      /* max # chars per field */

int      maxfieldlen = LINESIZE; /* this can be set to control precision */

#ifdef FIELDWIDTH
int   setnum(char **);           /* fetch fieldwidth value from string */
int   fplen(CONST char *, void *);     /* get field length of var */
#endif   /* FIELDWIDTH */

char     digits[] =  {"0123456789ABCDEF"};

#ifdef PRINTF_STDARG
int doprint(char *target, unsigned tlen, CONST char *sp, va_list va);
#else
int doprint(char *target, unsigned tlen, char *sp, int *vp);
#endif

extern void  dputchar(int chr);         /* port-defined putchar substitue */

#ifdef dprintf
#undef dprintf
#endif

#ifdef printf
#undef printf
#endif

#define doputchar(c) {outctxp->outlen++;if(outctxp->target){if(outctxp->outlen < outctxp->tlen) *(outctxp->target++) = c;}else dputchar(c);}

#ifdef PRINTF_STDARG

/* FUNCTION: dprintf()
 * 
 * PARAM1: char * fmt
 * PARAM2: ...
 *
 * RETURNS: int n           number of characters output
 */

int 
dprintf(CONST char * fmt, ...)
{
   va_list a;
   int n;

   va_start(a, fmt);
   n = doprint(NULL, 32767, fmt, a);
   va_end(a);

   return (n);
}


/* FUNCTION: printf()
 * 
 * PARAM1: char * fmt
 * PARAM2: ...
 *
 * RETURNS: 
 */
int
printf(CONST char * fmt, ...)
{
   va_list a;
   int n;

   va_start(a, fmt);
   n = doprint(NULL, 32767, fmt, a);
   va_end(a);

   return (n);
}

#ifndef NATIVE_SPRINTF

/* FUNCTION: sprintf()
 * 
 * PARAM1: char * buf
 * PARAM2: char * fmt
 * PARAM3: ...
 *
 * RETURNS: 
 */

int
sprintf(char * buf, CONST char *   fmt, ...)
{
   va_list a;
   int n;

   va_start(a, fmt);
   n = doprint(buf, 32767, fmt, a);
   va_end(a);

   return (n);
}
#endif   /* NATIVE_SPRINTF */

#ifndef NATIVE_SNPRINTF

/* FUNCTION: snprintf()
 * 
 * PARAM1: char * buf
 * PARAM2: unsigned buflen
 * PARAM3: char * fmt
 * PARAM4: ...
 *
 * RETURNS: 
 */

int
snprintf(char * buf, unsigned buflen, CONST char * fmt, ...)
{
   va_list a;
   int len;

   va_start(a, fmt);
   len = doprint(buf, buflen, fmt, a);
   va_end(a);
   return len;
}

/* FUNCTION: vsnprintf()
 * 
 * PARAM1: char * buf
 * PARAM2: unsigned buflen
 * PARAM3: char * fmt
 * PARAM4: va_list a
 *
 * RETURNS: 
 */
int
vsnprintf(char * buf, unsigned buflen, CONST char * fmt, va_list a)
{
   return doprint(buf, buflen, fmt, a);
}

int
vsprintf(char * buf, CONST char * fmt, va_list a)
{
   return(vsnprintf(buf, 32767, fmt, a));
}

#endif   /* NATIVE_SNPRINTF */

#else    /* PRINTF_STDARG */

/* FUNCTION: dprintf()
 * 
 * PARAM1: char *sp
 * PARAM2: int var
 *
 * RETURNS: 
 */

void
dprintf(char * sp,      /* pointer to format string */
   int   var)  /* 1st of n variables on stack */
{
   doprint(NULL, 32767, sp, &var);   /* printf to stdio */
}



/* FUNCTION: printf()
 * 
 * PARAM1: char *sp
 * PARAM2: int var
 *
 * RETURNS: 
 */

void
printf(char * sp, /* pointer to format string */
   int   var)  /* 1st of n variables on stack */
{
   doprint(NULL, 32767, sp, &var);   /* printf to stdio */
}

#ifndef NATIVE_SPRINTF

/* FUNCTION: sprintf()
 * 
 * PARAM1: char * buf
 * PARAM2: char * fmt
 * PARAM3: int arg1
 *
 * RETURNS: 
 */

void
sprintf(char * buf, char * fmt, int arg1)
{
   doprint(buf, 32767, fmt, &arg1);
}
#endif   /* NATIVE_SPRINTF */


#ifndef NATIVE_SNPRINTF

/* FUNCTION: snprintf()
 * 
 * PARAM1: char * buf
 * PARAM2: unsigned buflen
 * PARAM3: char * fmt
 * PARAM4: int arg1
 *
 * RETURNS: 
 */

int
snprintf(char * buf, unsigned buflen, char * fmt, int arg1)
{
   return doprint(buf, buflen, fmt, &arg1);
}
#endif   /* NATIVE_SNPRINTF */

#endif   /* PRINTF_STDARG */


/* struct output_ctx - output context, used by doprint() and
 * its helper functions
 */
struct output_ctx {
   char * target;  
   unsigned tlen;
   unsigned outlen;
};

/* local doprint helper routines. All take target as in doprint() */
static void hexbyte(struct output_ctx * outctxp,
                    unsigned x);
static void hexword(struct output_ctx * outctxp,
                    unsigned x);
static void declong(struct output_ctx * outctxp,
                    long lg);

/* FUNCTION: doprint()
 * 
 * PARAM1: char * target; NULL for output to stdio, else buffer pointer
 * PARAM2: int tlen; target buffer length in bytes (ignored for stdio)
 * PARAM3: char * sp; printf()-style format string
 * PARAM4: va_list va; pointer to variables
 *
 * RETURNS: number of characters that would have been printed in a 
 *          target buffer of infinite length, not including the
 *          trailing '\0'
 */

int
doprint(char *target,
        unsigned tlen,
        CONST char *sp,
#ifdef PRINTF_STDARG
        va_list va)
#else
        int *vp)         /* pointer to variables for tools w/o <stdarg.h> */ 
#endif   /* PRINTF_STDARG */
{
   char *cp;
   unsigned prefill, postfill, fieldlen;     /* varibles for field len padding */
   int   swap;       /* flag and temp holder for prefill-postfill swap */
   unsigned char fillchar;
   unsigned minfieldlen;
   int   i  =  0;
   unsigned tmp = 0;
   unsigned long lng = 0;
   struct output_ctx outctx;
   struct output_ctx * outctxp = &outctx;
#ifdef PRINTF_STDARG
   unsigned w0 = 0;
#ifdef SEG16_16
   unsigned w1;
#endif
   int   i0 = 0;
   unsigned char c = 0;
   char *   cap = 0;
   void *   varp;
#endif   /* PRINTF_STDARG */

   outctx.target = target;
   outctx.tlen = tlen;
   outctx.outlen = 0;

   while (*sp)
   {
      if (*sp != '%')
      {
         doputchar(*sp++);
         continue;
      }

      /* fall to here if sp ==> '%' */
      sp++;       /* point past '%' */

      /* see if any field width control stuff is present */
      cp = (char *)sp;       /* save pointer to filed width data in cp */

      while (*sp == '-' || (*sp >= '0' && *sp <= '9') || *sp == '.')
         sp++;       /* scan past field control goodies */

#ifdef PRINTF_STDARG
      switch (*sp)
      {
      case 'p':      /* '%p' - pointer */
#ifdef SEG16_16   /* do seg:offset variety */
         w0 = va_arg(va, unsigned);
         w1 = va_arg(va, unsigned);
#else /* 32 bit flat */
         lng = va_arg(va, unsigned long);
#endif /* SEG16_16 */   /* else fall to 32 bit code */
         break;
      case 'x':      /* '%x' - this always does 0 prefill */
         tmp = va_arg(va, unsigned);
         break;
      case 'd':      /* '%d' */
         i0 = va_arg(va, int);
         break;
      case 'u':      /* '%u' */
         w0 = va_arg(va, unsigned);
         break;
      case 'c':      /* '%c' */
         c = (unsigned char)va_arg(va, unsigned);
         break;
      case 's':      /* '%s' */
         cap = va_arg(va, char *);
         break;
      case 'l':
         if (sp[1] == 'x' || sp[1] == 'd' || sp[1] == 'u')  /* '%lx', '%ld', or '%lu' */
         {
            lng = va_arg(va, unsigned long);
         }
         /*  else   '%l?', ignore it */
         break;
      default:       /* %?, ignore it */
         break;
      }  /* end switch *sp */
#endif   /* PRINTF_STDARG */

#ifdef FIELDWIDTH
      prefill = postfill = 0;       /* default to no filling */
      fillchar = ' ';               /* ...but fill with spaces, if filling */
      swap = TRUE;                  /* ...and swap prefill & postfill */
      if (sp != cp)  /* if there's field control stuff... */
      {
         if (*cp == '-')      /* field is to be left adjusted */
         {
            swap = FALSE;     /* leave pXXfill unswaped */
            cp++;
         }
         else swap = TRUE;    /* we will swap prefill & postfill later */

            /* set prefill, postfill for left adjustment */
         if (*cp == '0')   /* fill char is '0', not space default */
         {
            cp++;
            fillchar = '0';
         }
         else fillchar = ' ';

            minfieldlen = setnum(&cp);    /* get number, advance cp */
#ifdef PRINTF_STDARG
         switch (*sp)
         {
         case 's':
            varp = (void *)&cap;
            break;
         case 'd':
            varp = (void *)&i0;
            break;
         case 'u':
            varp = (void *)&w0;
            break;
         case 'l':
            varp = (void *)&lng;
            break;
            default:
            varp = NULL;
            break;
         }
         fieldlen = fplen(sp, varp);
#else
         fieldlen = fplen(sp, vp);        /* figger display size of this var */
#endif   /* PRINTF_STDARG */
         if (*cp == '.')   /* do we have a max field size? */
         {
            cp++;       /* point to number past '.' */
            maxfieldlen = setnum(&cp);
         }
         else
         {
            maxfieldlen = LINESIZE;
         }

         if (maxfieldlen < (int)fieldlen)
         {
            fieldlen = maxfieldlen;
         }
         if (minfieldlen > fieldlen)
         {
            postfill = minfieldlen - fieldlen;
         }
         else
         {
            postfill = 0;
         }
         if ((postfill + fieldlen) > LINESIZE)
         {
            postfill = 0;   /* sanity check*/
         }
      }

      if (swap)      /* caller wanted right adjustment, swap prefill/postfill */
      {
         swap = (int)prefill;
         prefill = postfill;
         postfill = (unsigned)swap;
      }

      while (prefill--)
      {
         doputchar(fillchar); /* do any pre-field padding */
      }
#endif   /* FIELDWIDTH */


#ifdef PRINTF_STDARG
      switch (*sp)
      {
      case 'p':      /* '%p' - pointer */
#ifdef SEG16_16   /* do seg:offset variety */
         hexword(outctxp, w1);   /* display segment word */
         doputchar(':');
         hexword(outctxp, w0);   /* display offset word */
#else /* 32 bit flat */
         hexword(outctxp, (unsigned)(lng >> 16));
         hexword(outctxp, (unsigned)(lng & 0x0000FFFF));
#endif /* SEG16_16 */   /* else fall to 32 bit code */
         break;
      case 'x':      /* '%x' - this always does 0 prefill */
         if (tmp > 255)
            hexword(outctxp, tmp);
         else
            hexbyte(outctxp, tmp);
         break;
      case 'd':      /* '%d' */
         declong(outctxp, (long) i0);
         break;
      case 'u':      /* '%u' */
         declong(outctxp, (unsigned long) w0);
         break;
      case 'c':      /* '%c' */
         doputchar(c);
         break;
      case 's':      /* '%s' */
         i = 0;
         while (cap  && (*cap) && (i++ < maxfieldlen))
            doputchar(*cap++);
         break;
      case 'l':
         sp++;
         if (*sp == 'x')      /* '%lx' */
         {
            hexword(outctxp, (unsigned)(lng >> 16));
            hexword(outctxp, (unsigned)(lng & 0x0000FFFF));
         }
         else if(*sp == 'd' || *sp == 'u')   /* '%ld' or '%lu' */
         {
            declong(outctxp, lng);    /* we treat %lu as %ld. */
         }
         /*  else   '%l?', ignore it */
         break;
      default:       /* %?, ignore it */
         break;
      }  /* end switch *sp */
#else /* PRINTF_STDARG */
      switch (*sp)
      {
      case 'p':      /* '%p' - pointer */
#ifdef SEG16_16   /* do seg:offset variety */
         /* display segment word */
         hexword(outctxp, (unsigned)*(vp+1));
         doputchar(':');
         /* display offset word */
         hexword(outctxp, (unsigned)*vp);
         vp += 2;          /* bump var pointer past two words */
#else /* 32 bit flat */
         lng = *vp;
         hexword(outctxp, (unsigned)(lng >> 16));
         hexword(outctxp, (unsigned)(lng & 0x0000FFFF));
         vp += 4/sizeof(*vp); /* 2 or 1 */ 
#endif /* SEG16_16 */   /* else fall to 32 bit code */
         break;
      case 'x':      /* '%x' - this always does 0 prefill */
         tmp = *(unsigned *)vp++;
         if (tmp > 255)
            hexword(outctxp, tmp);
         else
            hexbyte(outctxp, tmp);
         break;
      case 'd':      /* '%d' */
         declong(outctxp, (long)*vp++);
         break;
      case 'u':      /* '%u' */
         declong(outctxp, (unsigned long)*(unsigned *)vp);
         vp++;
         break;
      case 'c':      /* '%c' */
         doputchar((unsigned char)(*vp++));
         break;
      case 's':      /* '%s' */
         cp = *(char **)vp;
         vp += sizeof(char*)/sizeof(int);   
         i = 0;
         while (*cp && i++ < maxfieldlen)
            doputchar(*cp++);
         break;
      case 'l':
         sp++;
         if (*sp == 'x')      /* '%lx' */
         {
            /*          lng = *vp;      */
            lng = *((unsigned long *) vp);
            hexword(outctxp, (unsigned)(lng >> 16));
            hexword(outctxp, (unsigned)(lng & 0x0000FFFF));
            vp += 4/sizeof(*vp); /* 2 or 1 */ 
         }
         else if(*sp == 'd' || *sp == 'u')   /* '%ld' or '%lu' */
         {
            /* we treat %lu as %ld. */
            declong(outctxp, *(long  *)vp);
            vp += 2;
         }
         /*  else   '%l?', ignore it */
         break;
     default:       /* %?, ignore it */
         break;
      }  /* end switch *sp */
#endif   /* PRINTF_STDARG */

#ifdef   FIELDWIDTH
      while (postfill--)
      {
         doputchar(fillchar);   /* do any post-field padding */
      }
#endif   /* FIELDWIDTH */

      sp++;       /* point past '%?' */

   }  /* end while *sp */

   if (outctxp->target)
      *(outctxp->target) = '\0';   /* Null terminate the string */

   return (int)(outctxp->outlen);
}




/* FUNCTION: hexword()
 *
 * hexword(x) - print 16 bit value as hexadecimal
 * 
 * PARAM1: char ** targ
 * PARAM2: unsigned x
 *
 * RETURNS: 
 */

static void
hexword(struct output_ctx * outctxp, unsigned x)
{
   doputchar(digits[(x >> 12) & 0x0f]);
   doputchar(digits[(x >>  8) & 0x0f]);
   doputchar(digits[(x >>  4) & 0x0f]);
   doputchar(digits[x & 0x0f]);
}



/* FUNCTION: hexbyte()
 * 
 * PARAM1: char ** targ
 * PARAM2: unsigned  x
 *
 * RETURNS: 
 */

static void
hexbyte(struct output_ctx * outctxp, unsigned  x)
{
   doputchar(digits[(x >> 4) & 0x0f]);
   doputchar(digits[x & 0x0f]);
}
 

/* FUNCTION: declong()
 *
 * declong(char*, long) - print a long to target as a decimal number. 
 * Assumes signed, prepends '-' if negative. 
 *
 * 
 * PARAM1: char ** targ
 * PARAM2: long lg
 *
 * RETURNS: 
 */

/* This should be local to declong, but Microsoft C 5.1 
 * breaks if it is. They use implicit lib calls to do long integer 
 * math, and the calls pass pointers to the longs, which assumes the 
 * longs are in local data space. Sheeeesh. -JB- 
 */
long tens;

static void
declong(struct output_ctx * outctxp, long  lg)
{
   int   digit;

   if (lg == 0)
   {
      doputchar('0');
      return;
   }
   else if(lg < 0L)
   {
      doputchar('-');
      lg = -lg;
   }

   /* make "tens" the highest power of 10 smaller than lg */
   tens = 1;
   while ((lg/10) >= tens) 
      tens *= 10;

   while (tens)
   {
      digit = (int)(lg/tens);    /* get highest digit in lg */

      doputchar(digits[digit]);

      lg -= ((long)digit * tens);
      tens /= 10;
   }
}


#ifdef FIELDWIDTH

/* FUNCTION: setnum()
 *
 * returns the value of fieldwidth digits from a 
 * printf format string. fptr should be a pointer to a pointer to one 
 * or more fieldwidth digits. On return, is advanced past the digits 
 * and the value of the digits is returned as an int. 
 *
 * 
 * PARAM1: char ** nptr
 *
 * RETURNS: returns the value of fieldwidth digits from a 
 * printf format string
 */

int      snval;   /* return value, breaks if on stack */

int
setnum(char ** nptr)
{
   snval = 0;

   while (**nptr >= '0' && **nptr <= '9') /* scan through digits */
   {
      snval *= 10;      /* calculate return value */
      snval += **nptr - '0';
      /* bump pointer (not pointer to pointer) past valid digits */   
      (*nptr)++;
   }
   return(snval);
}


/* FUNCTION: fplen()
 *
 * fplen(sp, varp) - returns the number of chars required to print 
 * the value pointed to by varp when formatted according to the string 
 * pointed to by sp. 
 *
 * 
 * PARAM1: char * sp
 * PARAM2: void * varp
 *
 * RETURNS: 
 */

long     lng1, lng2;    /* scratch value for longs */

int
fplen(CONST char * sp, void * varp)
{
   char *   cp;

   /* define the maximum digits needed to display a long value in 
    * decimal. Say , sizeof(long) = 4. So it has 8*4=32 bits. 
    * 2^10=1024. So for every 10 bits, we can show 3 digits. So for 
    * long, this value would be 32*3/10=96/10=9. And then we add one 
    * more digit for the roundoff. max_long_dig is used to prevent 
    * long overflow in lng1. There are better ways to prevent this, 
    * but very difficult to verify whether we made the perfect fix. 
    * Hence for now, this should do. 
    */
   static int max_long_dig = (8 * sizeof(long) * 3)/10+1;



   snval = 0;     /* use this for return value */
   lng1 = 1;   /* for figuring lengths of decimal numbers */

   switch (*sp)      /* switch on conversion char */
   {
   case 's':
      cp = *(char**)varp;
      while   (*cp++) snval++;
         return(snval);
   case 'c':
      return(1);
   case 'x':
      return(4);
   case 'p':
      return(9);
   case 'd':
   case 'u':
      lng2 = *(long *)varp;
      if (lng2 == 0) return(1);
         if (lng2 < 0)        /* if variable value is negative... */
      {   if (*sp == 'd')  /* format is signded decimal */ 
         {   snval++;   /* add space for '-' sign */
            lng2 = -lng2;
         }
         else     /* *sp == 'u' - format is unsigned */
            lng2 &= 0xffff;
      }
      while (lng1 <= lng2)
      {
         lng1 *= 10;
         snval++;
         if ( snval >= max_long_dig )
         {
            /* If we don't stop now, there lng1 will have long overflow */
            break;
         }
      }
      return(snval);
   case 'l':
      switch (*(sp+1))
      {
      case 'x':   /* '%lx' always 8 bytes */
         return(8);
      case 'u':      /* treat %lu like %ld */
      case 'd':
         lng2 = (*(long *)(varp));
         if (lng2 == 0) return(1);
            if (lng2 < 0)
         {
            snval++;       /* add space for '-' sign */
            lng2 = -lng2;
         }
         while (lng1 <= lng2)
         {
            lng1 *= 10;
            snval++;
            if ( snval >= max_long_dig )
            {
               /* If we don't stop now, there lng1 will have long overflow */
               break;
            }
         }
         return(snval);
         default:
         return(0);
      }         
      default:
      return(0);
   }
}
#endif   /* FIELDWIDTH */

#endif   /* NATIVE_PRINTF */

