/*
 * FILENAME: in_utils.c
 *
 * Copyright 1997- 2000 By InterNiche Technologies Inc. All rights reserved
 *
 * Misc commands for InterNiche menu system & diagnostic suite. 
 *
 * MODULE: MISCLIB
 *
 * ROUTINES: do_trap(), nextarg(), hexdump(), print_ipad(), 
 * ROUTINES: print_uptime(), panic(), print_eth(), uslash(), ns_printf(), 
 * ROUTINES: std_out(), std_in(), con_page(),
 *
 * PORTABLE: yes
 */

/* Additional Copyrights: */
/* Portions Copyright 1993 by NetPort software */


#include "ipport.h"
#ifdef MQX
#include <fio.h>
#endif

#if defined(NATIVE_PRINTF) || defined(PRINTF_STDARG)
#include <stdarg.h>
#endif

#ifdef NATIVE_PRINTF
#include <stdio.h>
#endif

#include "q.h"
#include "netbuf.h"
#include "net.h"
#include "ip.h"
#include "icmp.h"
#include "ether.h"
#include "arp.h"
#include "menu.h"
#include "in_utils.h"



/* FUNCTION: do_trap()
 *
 * try to hook system debugger
 * 
 * PARAM1: 
 *
 * RETURNS: 0
 */

int
do_trap(void)
{
   dtrap();
   return 0;
}



/* FUNCTION: nextarg()
 *
 * nextarg() - returns a pointer to next arg in string passed. args 
 * are printable ascii sequences of chars delimited by spaces. If 
 * string ends without more args, then a pointer to the NULL 
 * terminating the string is returned. 
 *
 * 
 * PARAM1: char * argp
 *
 * RETURNS:  pointer to next arg in string 
 */

char *   
nextarg(char * argp)
{
   while (*argp > ' ')argp++; /* scan past current arg */
      while (*argp == ' ')argp++;   /* scan past spaces */
      return (argp);
}



/* FUNCTION: hexdump()
 *
 * hexdump() - does a hex dump to console of a passed buffer. The 
 * buffer is declared as void so structs can be passed with the 
 * Compiler fussing. 
 *
 * 
 * PARAM1: void * pio
 * PARAM2: void * buffer
 * PARAM3: unsigned len
 *
 * RETURNS: void
 */

#define  HEX_BYTES_PER_LINE   16

void
hexdump(void * pio, void * buffer, unsigned len)
{
   u_char * data  =  (u_char *)buffer;
   unsigned int count;
   char  c;

   while (len)
   {
      /* display data in hex */
      for (count = 0; (count < HEX_BYTES_PER_LINE) && (count < len); ++count)
         ns_printf(pio, "%02x ", *(data + count));
      /* display data in ascii */
      for (count = 0; (count < HEX_BYTES_PER_LINE) && (count < len); ++count)
      {
         c = *(data + count);
         ns_printf(pio, "%c", ((c >= 0x20) && (c < 0x7f)) ? c : '.');
      }
      ns_printf(pio,"\n");
      len -= count;
      data += count;
   }
}



/* FUNCTION: print_ipad()
 *
 * Formatted print of an IP address, deprecated in favor of PUSH_IPADDR() macro
 *
 * PARAM1: unsigned long ipaddr
 *
 * RETURNS: pointer to formatted text
 */

char     ipreturn[18];     /* buffer for return */

char *   
print_ipad(unsigned long ipaddr)
{
   struct l2b  ip;

   ip.ip.iplong = ipaddr;
   sprintf(ipreturn, "%u.%u.%u.%u", 
    ip.ip.ipchar[0],
    ip.ip.ipchar[1],
    ip.ip.ipchar[2],
    ip.ip.ipchar[3]);

   return ipreturn;
}



/* FUNCTION: print_uptime()
 *
 * format a neat time string from a SNMP TIMETICKS
 * uptime format variable.
 *
 * Note: NOT REENTRANT
 * 
 * PARAM1: unsigned long timetick
 *
 * RETURNS:  pointer to formatted text
 */

static char tistring[24];     /* buffer for return */

char *   
print_uptime(unsigned long timetick)
{
   unsigned seconds, minutes, hours;

   timetick = timetick/100;   /* turn timetick into seconds */
   seconds = (unsigned)(timetick%60);
   timetick = timetick/60;    /* turn timetick into minutes */
   minutes = (unsigned)(timetick%60);
   timetick = timetick/60;    /* turn timetick into hours */
   hours = (unsigned)(timetick%24);
   timetick = timetick/24;    /* turn timetick into days */

   if (timetick)  /* Is there a whole number of days? */
      sprintf(tistring, "%ld days, %dh:%dm:%ds", 
    timetick, hours, minutes, seconds);
   else if (hours)
      sprintf(tistring, "%d hours, %dm:%ds", hours, minutes, seconds);
   else
      sprintf(tistring, "%d minutes, %d sec.", minutes, seconds);
   return tistring;
}



/* FUNCTION: panic()
 *
 * Fatal system error
 *
 * PARAM1: char *             error message
 *
 * RETURN: none
 *
 * Called if Software detects fatal system error. msg is
 * a string describing problem. There is no return from this 
 * routine. What this should do varies with the implementation. In a 
 * testing or development environment it should print messages, hook 
 * debuggers, etc. In an imbedded controller, it should probably try
 * to restart (ie warmboot) the system. 
 */

/* allow to be ifdeffed out on systems which already have a panic */
#ifndef PANIC_ALREADY

void
panic(char * msg)
{
   dprintf("panic: %s\n", msg);
   dtrap();                   /* try to hook debugger */
   netexit(1);                /* try to clean up */
}
#endif  /* PANIC_ALREADY */



/* FUNCTION: print_eth()
 *
 * print_eth() - pretty-print an ethernet address. Hex chars can be 
 * separated by "spacer" character passed. Returns a pointer to a tmp 
 * buffer with the formatted address 
 *
 * 
 * PARAM1: char * addr
 * PARAM2: char spacer
 *
 * RETURNS:  pointer to formatted text
 */

char     eth_prt_buf[18];  /* buffer for return */

char *   
print_eth(char * addr, char spacer)
{
   int   i;
   char *   out   =  eth_prt_buf;

   /* loop through 6 bytes of ethernet address */
   for (i = 0; i < 6; i++)
   {
      /* high nibble */
      *out = (char)(((*addr >> 4) & 0x0f) + 0x30);
      if (*out > '9')   /* need to make it A-F? */
         (*out) += 7;
      out++;

      /* low nibble */
      *out = (char)((*addr & 0x0f) + 0x30);  /* low nibble to digit */
      if (*out > '9')   /* need to make it A-F? */
         (*out) += 7;   /* eg 0x3a -> 0x41 ('A') */
      out++;

      /* optional spacer character */
      if (spacer && i < 5)
         *out++ = spacer;
      addr++;
   }
   *out = 0;
   return eth_prt_buf;
}




/* FUNCTION: uslash()
 *
 * uslash() - turn DOS slashes("\") into UNIX slashs ("/"). That's 
 * not to imple that UNIX slashes are right or better, just to be 
 * consistent. 
 *
 * 
 * PARAM1: char * path
 *
 * RETURNS:  pointer to formatted text
 */

char *   
uslash(char * path)
{
   char *   cp;

   for (cp = path; *cp; cp++)
      if (*cp == '\\')
      *cp = '/';
   return path;
}



/* FUNCTION: ns_printf()
 *
 * A generic printf() routine. It uses information from the
 * GenericIO structure to send the output to appropriate device.
 * Uses vsprintf() to make the output string.
 * The GenericIO structure contains a pointer to the function to
 * be used for device type (e.g. file, or telnet connection)
 * and an id identifing the specific device. For example,
 * for printing to a file, the function would point to fprintf 
 * and the id would be handle to file.
 *
 * PARAM1: void * vio      - Pointer to GenericIO structure (IN)
 * PARAM2: char * format   - Format string
 * followed by parameters as in printf.
 * PARAM3: ...             - variables to format
 *
 * RETURNS: Number of bytes that were output, or a negative error
 * code if an error occured.
 */

/* First use #defines to set memory & output for this system: */

#if 0
#ifndef npalloc
extern   char * npalloc(unsigned);
#endif   /* npalloc */
#ifndef npfree
void           npfree (void *);
#endif   /* npfree */

#ifndef NATIVE_PRINTF
#ifdef PRINTF_STDARG
int doprint(char *target, unsigned tlen, CONST char *sp, va_list va);
#else
int doprint(char *target, unsigned tlen, CONST char *sp, int FAR * vp);
#endif   /* PRINTF_STDARG */
#endif   /* NATIVE_PRINTF */
#endif

/* ns_printf's output can be directed to goto /dev/null. In such
 * cases, we don't want the following implementation and can omit
 * it with this ifdef:
 */
#ifndef ns_printf

int 
ns_printf(void * vio, char * format, ...)
{
   char *   outbuf=NULL;
   int   ret_value   ;
   int   buf_size =  MAXIOSIZE   ;
   GEN_IO pio = (GEN_IO)vio;  /* convert void* to our IO device type */

#if defined(NATIVE_PRINTF) || defined(PRINTF_STDARG)
   va_list argList;
#else /* NATIVE_PRINTF || PRINTF_STRING */
   int * next_arg=(int *)  &format;
   next_arg +=  sizeof(char *)/sizeof(int) ;
#endif   /* NATIVE_PRINTF || PRINTF_STRING */

   /* a NULL pio means just dump the output to stdout */
   if (pio == NULL)
   {
#ifdef NATIVE_PRINTF
      /* use the target system's ANSI routines */
      va_start(argList,format);
      ret_value = vprintf(format,argList);
      va_end(argList);
      return ret_value;
#else /* not NATIVE_PRINTF */
      /* use our homegrown versions */
#ifdef PRINTF_STDARG
      va_start(argList,format);
      doprint(NULL,0,format,argList);
      va_end(argList);
#else    /* not PRINTF_STDARG */
      doprint(NULL,0,format,next_arg);
#endif   /* PRINTF_STDARG */
      /* The return value here is not accurate-later */
      return strlen(format);
#endif   /* NATIVE_PRINTF */
   }

   /* Check if the output function is set */
   if (pio->out == NULL)
   {
      /* Programming mistake. Output function not set. */
      return -1;
   }

   /* Allocate memory for the output string 
    * If the format string is greater than MAXIOSIZE, then
    * we surely need to allocate a bigger block
    */
   ret_value = strlen(format); 
   if (ret_value >= MAXIOSIZE)
   {
      buf_size += ret_value ;
   }

   outbuf=(char *)npalloc(buf_size); 

   if (outbuf == NULL)
   {
      return -2;
   }

   /* Now populate the output string */

#ifdef NATIVE_PRINTF
   /* use the target system's ANSI routines */
   va_start(argList,format);
   ret_value = vsprintf(outbuf,format,argList);
   va_end(argList);
#else /* NATIVE_PRINTF */
   /* use the homegrown routines */
#ifdef PRINTF_STDARG
   va_start(argList,format);
   doprint(outbuf,buf_size,format,argList);
   va_end(argList);
#else    /* not PRINTF_STDARG */
   doprint(outbuf,buf_size,format,next_arg);
#endif   /* PRINTF_STDARG */
#endif   /* NATIVE_PRINTF */

#ifdef NATIVE_PRINTF
   /* Check if we have overwritten the output buffer */
   if ((int)strlen(outbuf) > buf_size)
   {
      /* Might want a more robust determination of whether outbuf 
       * got overflowed, like putting a magic number at the end of it 
       * before vsprintf call and verifying its still there afterwards
       */
      /* Yes , we have overwritten. Truncate the output string.
       * Some memory in the heap has been corrupted, but it is too
       * late to rectify.
       */
      panic("ns_printf:Buffer overflow");
      outbuf[buf_size-1]=0;   /* Null terminate the string */
   }
#endif

   ret_value =(pio->out)(pio->id,outbuf,strlen(outbuf)) ;

   /* Free memory for the output string */
   npfree(outbuf); 

   /* since ns_printf() can get called repeatedly down in the bowels 
    * of a single command interpretting function, spin tk_yield() so 
    * that some packets get a chance to get received 
    */
   tk_yield();

   return ret_value ;
}
#endif      /* ns_printf */



/* FUNCTION: std_out()
 *
 * Function which outputs the contents to standard output.
 * This function can be used with the GenericIO structure.
 *
 * PARAM1: long s    - Index to the output device. As there is only one
 *         output device for standard output, this value
 *         will not be meaningful to this function. (IN)
 * PARAM2: char *buf - Buffer of data (IN)
 * PARAM3: int len   - Length of data in buffer (IN)
 *         Length is needed so that we can output hex data also.
 *
 * RETURNS: Number of bytes send to standard output. 
 */

int std_out(long s, char * buf, int len)
{
   /* puts(buf); - This does newline expansion return 
    * write(0,buf,len); - This doesn't printf(buf); - This has 
    * problems when printf format strings (eg %s) is part of data. 
    */
   printf("%s",buf);
   USE_ARG(s);
   return len;
}

#ifdef IN_MENUS


/* FUNCTION: std_in()
 *
 * Returns the value of input char. Returns 0 if no char ready.
 *
 * PARAM1: long s - may someday be a device descriptor.
 *
 * RETURNS: Returns the input char, or 0 if no char ready.
 */

extern   int kbhit(void);
extern   int getch(void);

int 
std_in(long s)
{
   USE_ARG(s);

#ifdef MQX
   return (getchar ());
#else
   if (!kbhit())
   {
      return 0;
   }
   else
   {
      return (getch());
   }
#endif
}

#endif   /* IN_MENUS */



/* FUNCTION: con_page()
 *
 *  implement simple 'page' mechanism.  Blocks respectfully (other
 * tasks not blocked) until some input arrives.
 *
 * PARAM1: void * vio   - Pointer to GEN_IO structure (IN)
 * PARAM2: int lines    - Current line count (IN)
 *
 * RETURNS: 1 if we got a break, 0 to keep printing
 */

int
con_page(void * vio, int lines)
{
   int   ch;
   GEN_IO pio = (GEN_IO)vio;  /* convert void* to our IO device type */

   if (lines % 20 == 0)   /* Time to get user input */
   {
      if (pio && pio->getch)   /*if i/p func is supplied*/
      {
         ns_printf(pio,"....press any key for more (ESC to break)....");

         do 
         {
            ch = (pio->getch)(pio->id);
            if (ch == 0)
               tk_yield();    /* Give timeslice to other processes */
         } while (ch == 0) ;

            /* if there is fatal error, we don't want to do any I/O */
         if (ch == -1)   /* fatal error */
            return 1 ;

         ns_printf(pio,"\n");
         if (ch == 27)   /* ESC key pressed */
            return 1 ;
      }
   }
   return  0;
}

char **parse_args(char *buf, int argc, int *pargc_index)
{
   /* This routine assumes buf is a null terminated string */
   int i;
   int len;
   char *bp = buf;
   char **pargv = NULL;
   *pargc_index = 0;
   if (buf == NULL)
   {
      return (NULL);
   }
   len = strlen(buf);
   if (len <= 0)
   {
      return (NULL);
   }
   pargv = (char **) npalloc(argc * sizeof(char *));
   if (pargv == NULL)
   {
      return (NULL);
   }
   /* skip the initial blanks if any */
   while (*bp == ' ')
   {
      bp++;
   }
   while ((*bp != '\0') && ((*pargc_index) < argc))
   {
      pargv[(*pargc_index)] = bp;
      (*pargc_index)++;
      while (*bp != ' ' && *bp != '\0')
      {
         bp++; 
      }
      while (*bp == ' ' && *bp != '\0')
      {
         bp++; 
      }
   }
   for (i = 0; i < len; i++)
   {
      if (buf[i] == ' ')
         buf[i] = '\0';
   }

#if 0
   for (i = 0; i < *pargc_index; i++)
   {
      printf("pargv[%d] = %s\n", i, pargv[i]);
   }
#endif

   return (pargv);
}

