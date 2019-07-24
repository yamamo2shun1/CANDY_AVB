/*
 * FILENAME: ftpcprn.c
 *
 * Copyright  2000 By InterNiche Technologies Inc. All rights reserved
 *
 *
 * MODULE: FTP
 *
 * ROUTINES: fc_printf(), 
 *
 * PORTABLE: yes
 */

/* The FTP client's message handler for this port. This is off by 
 * itself without it's prototype included to please the incredibly 
 * fussy Borland compiler again.... 
 */
#include "ftpport.h"    /* TCP/IP, sockets, system info */

#ifdef FTP_CLIENT

#include "ftpsrv.h"
#include "ftpclnt.h"

struct ftpc;
extern   struct ftpc *  ftp_clients;   /* support multiple client links */
extern   char *   prompt;


/* The member  in_use of a FTP connection is turned ON when fc_printf() is 
 * being done. FTP Client, unlike FTP Server has different entry points. 
 * For example,
 * -  When the user enters FTP commands from the command line, the FTP client
 *    code gets executed. 
 * -  fc_check() is called periodically to process FTP client connections. 
 * Consider the following scenario.
 * 1. The user executes PUT command, and the FTP client code
 *    initiating the PUT uses fc_printf() immediately after sending the
 *    PORT command using fc->cmdbuf. It would then check whether the command 
 *    was properly sent or not. 
 * 2. Now before this check can be done, fc_printf() calls tk_yield(), which
 *    would call fc_check(). fc_check() would execute and do network I/O for
 *    this connection and update the fc->cmdbuf based on the response for
 *    PORT command. 
 * 3. Now after fc_printf() is done, the check for PORT command would fail
 *    because the fc->cmdbuf is modified.
 *
 * On a multitasking system, FTP client would be more susceptible to 
 * these kind of problems. Hence a provision is provided wherein fc_check() 
 * doesn't execute when fc_printf() is being done.
*/



/* FUNCTION: fc_printf()
 * 
 * fc_printf() - per-port function to handle messages from the FTP 
 * client library. The messages may be sent to a console if your 
 * system has one, logged, or just punted. For DOS demo .exe, we 
 * print to screen and restore the user prompt. 
 *
 * PARAM1: struct ftpc * fc
 * PARAM2: char * format
 * PARAM3: long p1
 * PARAM4: long p2
 * PARAM5: long p3
 * PARAM6: long p4
 * PARAM7: long p5
 *
 * RETURNS: 
 */

#ifdef PRINTF_STDARG
/* different fc_printf() functions depending on VA support */

void
fc_printf(struct ftpc * fc, char * format, ...)
{
   va_list a;
   char linebuf[FTPMAXPATH];

#else /* the non-STDARG version */

void
fc_printf(struct ftpc * fc, char * format, 
   long  p1,   long  p2,   long  p3,   long  p4,   long  p5)
{
#endif /* PRINTF_STDARG */

   char *   cp;
   void *   pio=NULL;

   /* make sure caller passed our sole supported fc. note: fc may be NULL 
    * during fc_connect() call - this is OK. 
    */
   if (fc != NULL )
   {
      pio=(void *)fc->pio;
      /* set re-entry flag. So that fc_check() doesn't process this 
       * connection. If fc_check()->...->fc_printf() is called, then 
       * in_use is already set, and the increment/decrement 
       * below is harmless.
       */
      fc->in_use++;    
   }
   

#ifdef PRINTF_STDARG
   va_start(a,format);
   vsprintf(linebuf,format,a);
   va_end(a);
   ns_printf(pio,linebuf);
#else /* the non-STDARG version */
   ns_printf(pio,format, p1, p2, p3, p4, p5);
#endif /* PRINTF_STDARG */

#ifdef FC_LOG
   /* If logging is enabled, log it. Don't log to stdio. 
    */
   if ((log_flag == TRUE) && (ftplog!=NULL) )
#ifdef PRINTF_STDARG
      log_printf(ftplog,linebuf);
#else /* the non-STDARG version */
      log_printf(ftplog,format, p1, p2, p3, p4, p5);
#endif /* PRINTF_STDARG */
#endif

   /* see if we should regenerate prompt */
   cp = (format + (strlen(format)-1));
   if (*cp == '\n' || *cp == '\r')  /* look for newline */
      ns_printf(pio,prompt);

   if(fc)
      fc->in_use--;     /* clear re-entry flag */
}

#endif /* FTP_CLIENT */
