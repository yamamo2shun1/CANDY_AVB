/*
 * FILENAME: syslog.c
 *
 * Copyright 2004 By InterNiche Technologies Inc. All rights reserved.
 *
 * Implements the syslog client for InterNiche Stack.
 * API is based on the BSD syslog specification.
 * This syslog client also allows different applications to send
 * logs to different syslog servers. 
 *
 * Usability notes.
 * Applications can use syslog as follows.
 * 1. Use syslog directly. Just call syslog() to send the log.
 * 2. Use syslog when needed.
 *    Call openlog() to open logging for a facility/application.
 *    Call syslog() to send logs.
 *    Call closelog() when done.
 * 3. Use syslog with special features (for InterNiche syslog client)
 *    We have defined a special mechanism where-in different
 *    applications can send logs to different syslog servers.
 *    To use this feature, use the following sequence.
 *    Call openlog() to open logging for a facility/application.
 *    Call openlogaddr() to set the syslog server address.
 *    Call syslog() to send logs to the specific syslog server.
 *    Call closelogfac() when done. This will close the logging
 *       session for the particular application.
 *
 * Additional information about the syslog client
 * 1. Call setlogmask() to mask the priorities of syslog messages.
 * 2. Calling closelog() closes all logging sessions, including
 *    the default. On a subsequent syslog() call, default session
 *    is recreated.
 * 3. syslog() does the following
 *    - If LOG_CONS option was set, then log to console
 *    - If LOG_FILE option was set, then log to file.
 *    - Send message/log to syslog server
 * 4. If no facility/application is specified in syslog(),
 *    then the default facility LOG_USER is used.
 * 5. Here are some of the default values
 *    Default facility - LOG_USER
 *    Default severity - LOG_NOTICE
 *    Default options  - LOG_FILE
 * 6. The test cases/functions provided at the end of the 
 *    file can be viewed for sample usage.
 * 7. sl_defparams.enable flag can be used to enable/disable syslog
 * 8. sl_defparams.enable flag can be used to enable/disable 
 *    local logging of syslog messages - not currently supported. 
 * 
 *    
 * Interoperability notes.
 * - The openlog(), syslog(), closelog(), setlogmask() functions 
 *    work as per the BSD specs (manpages).
 * - In addition to the above, specific functions like
 *    openlogaddr() and closelogfac() are provided.
 * - Support for other BSD syslog functions (like vsyslog())
 *    is not provided.
 *
 * Integration notes.
 * - At system startup, call syslog_init() to initialize the syslog client
 *   Mainly needed to install the syslog sub-menu
 * - At system shutdown, call closelog() to cleanup the syslog client
 *   - Close the open log file
 *   - Release all the resources used by syslog client
 *     (free all nodes in sl_list). 
 * - Syslog packets are sent over UDP. If SYSLOG_SOCKETS is defined,
 *   then sockets API is used for it. Otherwise lightweight API is used.
 *
 *
 * 09/02/2004 - created -AT-
 * 10/04/2004 - support for lightweight API -AT-
 * 
 * MODULE: MISCLIB
 *
 * ROUTINES: closelog(), openlog(), sl_create_default(), 
 * ROUTINES: sl_print_date(), syslog(), 
 * ROUTINES: setlogmask(), openlogaddr(), closelogfac(), 
 * ROUTINES: sl_lw_udpsend(), sl_udpsend(), sl_logger(), 
 * ROUTINES: sl_set_syslogd(), sl_enable(), sl_stats(), 
 * ROUTINES: sl_test1(), sl_test2(), sl_test3(), sl_test4(), 
 * ROUTINES: sl_testsev(), sl_testfac(), sl_testdate(), sl_testall(), 
 * ROUTINES: sl_nvset(), sl_nvinfo(), syslog_init(), prep_syslog(), 
 *
 * PORTABLE: yes
 * Port-dependent macros have been provided in syslog.h
 * and can be finetuned for the specific port.
 *
 */


#include "ipport.h"   /* InterNiche embedded system includes */

#ifdef INICHE_SYSLOG  /* whole file can be ifdefed out */

#include "syslog.h"
#include "genlist.h"  /* generic linked list */
#include "vfsfiles.h"
#include "in_utils.h"
#include "menu.h"
#include "udp.h"       /* decl of udp_alloc(), udp_send() */
#include "q.h"
#include "intimers.h"
#ifdef USE_SNTP_V4
#include "..\sntp\sntpv4.h"
#endif

#ifndef SUPERLOOP
#ifndef OSPORT_H
#error Need to define OSPORT_H
#endif
#include OSPORT_H
#endif   /* SUPERLOOP */

#ifndef SUPERLOOP
extern TK_OBJECT(to_syslog);
#endif   /* not SUPERLOOP */

/* if TCP is being used, then use sockets, else lightweight UDP API */
#ifdef INCLUDE_TCP
#define SYSLOG_SOCKETS   1  /* use sockets() */ 
#endif


/* syslog_info structure contains all parameters (facility specific) *
 * for sending a syslog message/log to a syslog server.              *
 * sl_list will maintain a dynamic linked list of syslog_info nodes. *
 * All nodes in sl_list can be indexed using the facility value.     *
 */
typedef struct syslog_info
{
   long facility;           /* application using syslog */
   char iden[MAX_NAME_LEN]; /* name of the facility */
   int  logopt;             /* options for logging */
   SOCKTYPE sock;           /* client socket   */
   struct sockaddr sa;      /* sa and sa_len are needed for sendto() */
   int sa_len;
   VFILE  *fp;              /* log file */
   char * fname;            /* name of log file */
}syslog_info;

/* sl_defval global defines the default values used by               *
 * the syslog client. Default settings can be adjusted               *
 * via sl_defval.                                                    *
 */
static syslog_info sl_defval = 
{ LOG_USER, "", LOG_FILE, INVALID_SOCKET };

/* sl_def points to the node in sl_list with default settings.       *
 * By using sl_def, we don't have to do a lookup in sl_list          *
 * for the default case.                                             *
 */
static syslog_info *sl_def = NULL;
static long sl_mask = LOG_UPTO(LOG_DEBUG);/* default - allow all severities */

/* syslog_stats contains counters for statisical needs               *
 */
struct syslog_stats
{
   long callcnt;                   /* num of times syslog() was called */
   long severity[SEVERITY_MAX];    /* a counter for each severity */
   long facility[LOG_NFACILITIES];/* a counter for each facility */
};

struct syslog_stats slstats;

/* Define sl_list. As all genlist API expect a pointer to NicheList,
 * lets define syslog_list and make sl_list point to it.
 * We will refer to sl_list in all the API.
 */
struct NicheList syslog_list;
NICHELIST sl_list = &syslog_list;

int sl_udpsend(syslog_info *sli, char * outbuf, int len);
extern u_long inet_addr(char far * str);
extern char *print46_addr(struct sockaddr *ipaddr);

/* Syslog Client related non-volatile parameters. Please see nvparms.h
 * and nvparms.c regarding the usage of the following structure.
 */
struct sl_nvparam
{
   char   logfile[MAX_NVSTRING];  /* name of log file */
   unshort year;                  /* device start date (year, month, day) */
   unshort month;
   unshort day;
   unshort enable;                /* 1/0 - enable/disable syslog clnt */
   unshort locallog;            /* 1/0 - enable/disable local log */
   unshort pad;                   /* pad to make syslogd 32-bit aligned */
   /* syslogd array holds IP addresses of default syslog servers/daemons */
   struct sockaddr syslogd[LOG_MAXSVRS] ; 
};

struct sl_nvparam sl_defparms = 
{ 
   _PATH_LOG, LOG_YEAR, LOG_MONTH, LOG_DAY, 1, 0,
};

/* structure to encapsulate all time-related fields */
struct sl_time
{
   u_short year;
   u_char month;
   u_char day;
   u_char hours;
   u_char minutes;
   u_char seconds;
};

int syslog_init_flag = 0;
#ifdef USE_SYSLOG_QUEUE
typedef struct syslog_item_st
{
   int priority;
   int buf_size;
   char *outbuf;
}  syslog_item_st;

#ifndef USE_SYSLOG_TASK
long syslog_timer = 0;
#endif
void syslog_handler(long foo);
queue syslog_queue;
#endif

/* FUNCTION: closelog()
 *
 * Release all the resources used by syslog client
 * - Close the open log files
 * - Close the open sockets 
 * - Free all nodes in sl_list 
 *
 * PARAM1: -
 *
 * RETURNS: -
 */

void 
closelog(void)
{
   /* Release all resourses used by syslog client */
   int len,i;
   syslog_info * tmp;

   len = niche_list_len(sl_list);

   /* close the socket and file used by each facility */
   for (i=0; i < len; i++)
   {
      tmp = (syslog_info *) niche_list_getat(sl_list,i);
#ifdef SYSLOG_SOCKETS
      if (tmp->sock != INVALID_SOCKET)
      {
         socketclose(tmp->sock);
         tmp->sock = INVALID_SOCKET;
      }
#endif
      if (tmp->fp != NULL)
      {
         vfclose(tmp->fp);
         tmp->fp = NULL;
      }
   }

   /* Use niche_list_destructor() delete all elements from the list. */
   niche_list_destructor(sl_list);

   sl_def = NULL;  
}

/* FUNCTION: openlog()
 *
 * Note the iden and logopt values for the facility.
 * And use them for future syslog() calls for this facility.
 * 
 * This function creates a new node for this facility in sl_list
 * and puts all the information in the node. syslog() refers
 * to this information.
 *
 * PARAM1: -
 *
 * RETURNS: -
 */

void 
openlog(const char * iden, int logopt, int facility)
{
   syslog_info *tmp;
   tmp = (syslog_info *)niche_lookup_id(sl_list,facility);

   if (tmp)
   {
      /* we already have an entry for this facility - update it */
   }
   else
   {
      /* add an entry for this facility */
      if (niche_add_id_and_name(sl_list,facility,(char *)iden) != SUCCESS)
         return;
      tmp = (syslog_info *)niche_lookup_id(sl_list,facility);
      tmp->fp = NULL;
      tmp->sock = INVALID_SOCKET;
      tmp->sa_len = 0;
   }

   strcpy(tmp->iden,iden);
   tmp->logopt = logopt;
}

/* FUNCTION: sl_create_default()
 *
 * Create an entry for default syslog messages.
 * And make sl_def point to it.
 *
 * PARAM1: -
 *
 * RETURNS: -
 */

void
sl_create_default(void)
{
   if (!sl_def)
   {      
      /* default entry does not exist - create it */
      openlog(sl_defval.iden, sl_defval.logopt, sl_defval.facility);
      sl_def = (syslog_info *)niche_lookup_id(sl_list,sl_defval.facility);
      if (!sl_def)
         return;     /* can't add new node - out of memory ?? */

      openlogaddr(sl_defval.facility, &sl_defparms.syslogd[0], 
         sizeof(struct sockaddr_in),  _PATH_LOG);
   }
}

/* FUNCTION: sl_print_date_sl()
 *
 * Print today's date and time in the passed buffer.
 *
 * InterNiche stack has a concept of timeticks (ticks since the
 * system was up). Thats it. It starts with 0 and gets updated
 * at evertime time tick.
 *
 * For the syslog message, we need to print the date.
 * This is done using the following algorithm.
 * 1. Year, month, day fields of sl_defparms denote the system startup date.
 *    (LOG_YEAR, LOG_MONTH, LOG_DAY are default values for date)
 * 2. Using timetick, we can find out number of days since the system was up.
 * 3. We add the number of days to start date to get today's date.
 *
 * PARAM1: char * outbuf              - buffer to print date on(OUT)
 * PARAM2: int priority               - priority of the message (IN)
 * PARAM3: unsigned long timetick     - timeticks since system was up (IN)
 *
 * RETURNS: -
 */
void
sl_print_date_sl(char * outbuf, int priority, unsigned long timetick)
{
   /* first print the priority, date, time */
   unsigned seconds, minutes, hours;
   int year = sl_defparms.year;
   int month = sl_defparms.month;
   int day = sl_defparms.day;
   int days[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

   timetick = timetick/100;   /* turn timetick into seconds */
   seconds = (unsigned)(timetick%60);
   timetick = timetick/60;    /* turn timetick into minutes */
   minutes = (unsigned)(timetick%60);
   timetick = timetick/60;    /* turn timetick into hours */
   hours = (unsigned)(timetick%24);
   timetick = timetick/24;    /* turn timetick into days */

   /* add the days (since system was up) */
   while (timetick)
   {
      /* add days, 1 month at a time */
      if ((days[month-1] - day) >= (int)timetick)
      {
         day+= timetick;
         timetick = 0;
      }
      else
      {
         /* advance by 1 month */
         timetick -= days[month-1] - day +1 ;
         day=1;  /* start of next month */
         month++; /* advance to next month */
         if (month >12)
         {
            /* advance to next year */
            month=1;
            year++;
            /* reset the number of days in FEB for this year */
            if ((((year %4) == 0) && (year % 100 !=0)) || ((year % 400) == 0))
               days[1] =29;
            else
               days[1] =28;
         }
      }
   }

   sprintf(outbuf,"<%d>%04d-%02d-%02d %02d:%02d:%02d ",
      priority, year, month, day, hours, minutes, seconds);
}

#ifdef USE_SNTP_V4
void
sl_conv_ntpts2ymdhms (struct ntp_timestamp * ntptsp, struct sl_time * sltp)
{
   USE_ARG(ntptsp);

   sltp->year = 1970;
   sltp->month = 1;
   sltp->day = 1;
   sltp->hours = 0;
   sltp->minutes = 0;
   sltp->seconds = 0;
}

void
sl_print_date_ntp(char * outbuf, int priority, unsigned long timetick)
{
   struct ntp_timestamp ntpts;
   int rc;
   struct sl_time slt;

   if ((rc = sntpv4_get_time (&ntpts)) == SNTPV4_OK)
   {
      /* convert from NTP timestamp format to year/month/day/hour/minute/second format */
      sl_conv_ntpts2ymdhms (&ntpts, &slt);
      sprintf(outbuf,"<%d>%04d-%02d-%02d %02d:%02d:%02d ",
         priority, slt.year, slt.month, slt.day, slt.hours, slt.minutes, slt.seconds);
   }
   else
   {
      sl_print_date_sl (outbuf, priority, timetick);
   }   
}
#endif /* USE_SNTP_V4 */

void
sl_print_date(char * outbuf, int priority, unsigned long timetick)
{
#ifndef USE_SNTP_V4
   sl_print_date_sl (outbuf, priority, timetick);
#else
   sl_print_date_ntp (outbuf, priority, timetick);
#endif /* USE_SNTP_V4 */
}

/* FUNCTION: syslog()
 *
 * PARAM1: int priority
 * PARAM2: const char * msg
 * PARAM3: variable argument list (like printf)
 *
 * RETURNS: -
 */

#ifndef NATIVE_PRINTF
#ifdef PRINTF_STDARG
int doprint(char * target, unsigned tlen, CONST char * sp, va_list va);
#else
int doprint(char * target, unsigned tlen, CONST char * sp, int FAR * vp);
#endif   /* PRINTF_STDARG */
#endif   /* NATIVE_PRINTF */

#if 0
void 
syslog(int priority, const char * msg, ...)
{
   syslog_info *tmp;

   char * outbuf=NULL;
   char * outbuf2;
   int   ret_value   ;
   int   buf_size =  MAXIOSIZE   ;
#if defined(NATIVE_PRINTF) || defined(PRINTF_STDARG)
   va_list argList;
#else /* NATIVE_PRINTF || PRINTF_STRING */
   int * next_arg=(int *)&msg;
   next_arg +=  sizeof(char *)/sizeof(int) ;
#endif   /* NATIVE_PRINTF || PRINTF_STRING */

   /* priority = (facility | severity) */
   int severity = LOG_PRI(priority);
   int facility = priority & LOG_FACMASK;

   /* check if syslog client is enabled or not */
   if (!sl_defparms.enable)
      return;  /* do nothing - because syslog client is disabled */

   /* ignore severites disabled via sl_mask */
   if (!((1 << severity) & sl_mask))
      return;

   /* If we have specific info for this facility, use it.
    * Else use the default information.
    */
   tmp = (syslog_info *)niche_lookup_id(sl_list,facility);
   if (!tmp || !tmp->sa_len)
   {
      if (!sl_def)
         sl_create_default();

      if (!tmp)
         tmp = sl_def;   /* use the default settings */
   }

   /* update counters */
   slstats.callcnt++;
   slstats.severity[severity]++;
   slstats.facility[facility>>3]++;

   /* Allocate memory for the output string 
    * If the format string is greater than MAXIOSIZE, then
    * we surely need to allocate a bigger block
    */
   ret_value = strlen(msg); 
   if ( ret_value >= MAXIOSIZE )
   {
      buf_size += ret_value ;
   }

   outbuf=(char *)npalloc(buf_size); 

   if ( outbuf == NULL )
   {
      return ;
   }

   /* Now populate the output string */
#ifdef LOG_PRINTDATE
   sl_print_date(outbuf, priority, sysuptime());
#else
   sprintf(outbuf,"<%d> ", priority);
#endif

   strcat(outbuf, tmp->iden);
   strcat(outbuf, ":");
   outbuf2 = outbuf + strlen(outbuf);

#ifdef NATIVE_PRINTF
   /* use the target system's ANSI routines */
   va_start(argList,msg);
   ret_value = vsprintf(outbuf2,msg,argList);
   va_end(argList);
#else /* NATIVE_PRINTF */
   /* use the homegrown routines */
#ifdef PRINTF_STDARG
   va_start(argList,msg);
   doprint(outbuf2,buf_size,(char *)msg,argList);
   va_end(argList);
#else    /* not PRINTF_STDARG */
   doprint(outbuf2,buf_size,msg,next_arg);
#endif   /* PRINTF_STDARG */
#endif   /* NATIVE_PRINTF */

#ifdef NATIVE_PRINTF
   /* Check if we have overwritten the output buffer */
   if ( (int)strlen(outbuf) > buf_size )
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

   /* log to console */
   if (tmp->logopt & LOG_CONS)
      ns_printf(NULL,"%s\n",outbuf);

   /* log to file */
   if ((tmp->logopt & LOG_FILE) && tmp->fp)
   {
      vfwrite(outbuf,1,strlen(outbuf),tmp->fp);
      vfwrite(LOG_CRLF,1,2,tmp->fp);
   }
   
   ret_value = sl_udpsend(tmp, outbuf, strlen(outbuf));

   /* Free memory for the output string */
   npfree(outbuf); 

   /* since syslog() can get called repeatedly down in the bowels 
    * of a single command interpreting function, spin tk_yield() so 
    * that some packets get a chance to get received 
    */
   tk_yield();

   return ;
}
#endif

/* FUNCTION: setlogmask()
 *
 * An application has a log priority mask that determines which 
 * calls to syslog(3) may be logged. All other calls will be ignored. 
 * Logging is enabled for the priorities that have the corresponding bit 
 * set in mask. The initial mask is such that logging is enabled for 
 * all priorities. The setlogmask() function sets this logmask for 
 * the current process, and returns the previous mask. 
 * If the mask argument is 0, the current logmask is not modified. 
 *
 * PARAM1: int maskpri
 *
 * RETURNS: -
 */

int  
setlogmask (int maskpri)
{
   int tmp = (int) sl_mask;

   /* if maskpri is 0, don't do anything */
   if (maskpri)
   {
      sl_mask = maskpri;
   }
   return tmp;   /* return the old mask */
}

/* FUNCTION: openlogaddr()
 *
 * PARAM1: int facility
 * PARAM2: struct sockaddr *sa
 * PARAM3: int sa_len
 * PARAM4: char * fname          - name of facility specific logfile
 *
 * RETURNS: -
 */

void 
openlogaddr(int facility, struct sockaddr *sa, int sa_len, char *fname)
{
   syslog_info *tmp;
   int e=0;

   tmp = (syslog_info *)niche_lookup_id(sl_list,facility);
   if (!tmp)
   {
      /* entry doesn't exist for this facility - create one */
      openlog(sl_defval.iden, sl_defval.logopt, facility);
      tmp = (syslog_info *)niche_lookup_id(sl_list,facility);
      if (!tmp)
         return;     /* can't add new node - out of memory ?? */
   }

#ifdef SYSLOG_SOCKETS
   /* open UDP socket */
   tmp->sock = socket(sa->sa_family, SOCK_DGRAM, 0);
   if (tmp->sock == INVALID_SOCKET)
   {
      dprintf("syslog: can't create socket\n");
      return ;  /* can't create socket */
   }

   /* make client socket a connected socket */
   e = connect(tmp->sock, sa, sa_len);
   if (e != 0)
   {
      e = t_errno(tmp->sock);
      dprintf("syslog: client connect error: %d\n", e);
      tmp->sock = INVALID_SOCKET;
      return;
   }

   /* put socket into non-blocking mode */
   setsockopt(tmp->sock, SOL_SOCKET, SO_NBIO, NULL, 0);
#else
   USE_ARG(e);
#endif /* SYSLOG_SOCKETS */

   /* save the destination information */
   tmp->sa = *sa;
   tmp->sa_len = sa_len;

   if (fname)  
   {
      tmp->fp = vfopen(fname,"wb");
      if (!tmp->fp)
      {
         dprintf("syslog: client can't open file %s\n", fname);
         return;
      }
      tmp->fname = fname;
   }
}

/* FUNCTION: closelogfac()
 *
 * Cleanup the entry for this facility/application.
 *
 * PARAM1: int facility
 *
 * RETURNS: -
 */

void 
closelogfac(int facility)
{
   syslog_info *tmp;
   tmp = (syslog_info *)niche_lookup_id(sl_list,facility);
   if (tmp)
   {
#ifdef SYSLOG_SOCKETS
      if (tmp->sock != INVALID_SOCKET)
      {
         socketclose(tmp->sock);
         tmp->sock = INVALID_SOCKET;
      }
#endif
      if (tmp->fp != NULL)
      {
         vfclose(tmp->fp);
         tmp->fp = NULL;
      }
      niche_del(sl_list, (GEN_STRUCT)tmp);
      /* reset sl_def, if this was the default facility */
      if (sl_def == tmp)
         sl_def = NULL;
   }
}

/* FUNCTION: sl_lw_udpsend()
 *
 * Send data using Lightweight UDP API
 * Support both IPv4 and IPv6 addresses.
 *
 * PARAM1: struct sockaddr  *sa
 * PARAM2: char * outbuf
 * PARAM3: int len
 *
 * RETURNS: 
 *    - ENP_NOMEM if out of memory
 *    - returns -1 is address family is not supported
 *    - return value given by udp_send()
 */


#ifndef SYSLOG_SOCKETS

int
sl_lw_udpsend(char * data, int len, struct sockaddr *addr)
{ 
   PACKET pkt;

   if (addr->sa_family == AF_INET)
   {
      struct sockaddr_in *sin;
      sin = (struct sockaddr_in *)addr;
   
      pkt = udp6_alloc(len,0);
      if (!pkt)
         return ENP_NOMEM;
      pkt->nb_plen = len;
      
      MEMCPY(pkt->nb_prot, data, len);
      pkt->fhost = sin->sin_addr.s_addr;
   
      /* underlying stack will free the packet once its sent */
      return udp_send(sin->sin_port, LOG_PORT, pkt);
   }
#ifdef IP_V6
   else if (addr->sa_family == AF_INET6)
   {
      struct sockaddr_in6 *sin;
      sin = (struct sockaddr_in6 *)addr;
   
      pkt = udp6_alloc(len,0);
      if (!pkt)
         return ENP_NOMEM;
      pkt->nb_plen = len;
      MEMCPY(pkt->nb_prot, data, len);
   
      /* underlying stack will free the packet once its sent */
      return udp6_send(&sin->sin6_addr, sin->sin6_port, LOG_PORT, pkt);
   }
#endif
   else
   {
      return -1;  /* address family not supported */
   }
   
}

#endif


/* FUNCTION: sl_udpsend()
 *
 * Send the syslog message via UDP.
 * Send it using sockets or lightweight API.
 * If SYSLOG_SOCKETS is defined, use sockets. Else use Lightweight API.
 *
 * PARAM1: syslog_info *sli
 * PARAM2: char * outbuf
 * PARAM3: int len
 *
 * RETURNS: -
 */

int 
sl_udpsend(syslog_info *sli, char * outbuf, int len)
{
   int ret_value=0;

#ifdef SYSLOG_SOCKETS
   /* If sli doesn't have a socket of its own, use default socket.
    * This can happen if application calls openlog(), syslog().
    */

   if (sli->sock == INVALID_SOCKET)
      sli= sl_def;


   /* send log to syslogd using sockets */
   if (sli->sock)
   {
      ret_value = sendto(sli->sock, outbuf, len, 0, &sli->sa, sli->sa_len);
   }

#else
   /* send log to syslogd using lightweight API */
   if (!sli->sa_len)
      sli= sl_def;
  
   ret_value = sl_lw_udpsend(outbuf, len, &sli->sa);

   /* If multiple servers (syslog daemons) are present, then
    * send log to all of them.
    * Do it only for messages sent to default syslogd.
    */
   if ((LOG_MAXSVRS > 1) && (sli == sl_def))
   {
      int i;
      for (i=1; i < LOG_MAXSVRS; i++)
      {
         ret_value = sl_lw_udpsend(outbuf, len, &sl_defparms.syslogd[i]);
      }
   }
#endif

   return ret_value;
}


#ifdef IN_MENUS

char *severitynames[SEVERITY_MAX] = {
   "emerg",     /* index 0 */
   "alert",     /* index 1 */
   "crit",      /* index 2 */
   "err",       /* index 3 */
   "warning",   /* index 4 */
   "notice",    /* index 5 */
   "info",      /* index 6 */
   "debug",     /* index 7 */
};

char * facnames[LOG_NFACILITIES] = {
   "kern",      /* index 0 */
   "user",      /* index 1 */
   "mail",      /* index 2 */
   "daemon",    /* index 3 */
   "auth",      /* index 4 */
   "syslog",    /* index 5 */
   "lpr",       /* index 6 */
   "news",      /* index 7 */
   "uucp",      /* index 8 */
   "cron",      /* index 9 */
   "authpriv",  /* index 10 */
   "ftp",       /* index 11 */
   "ntp",       /* index 12 */
   "security",  /* index 13 */
   "console",   /* index 14 */
   "notused",   /* index 15 */
   "local0",    /* index 16 */
   "local1",    /* index 17 */
   "local2",    /* index 18 */
   "local3",    /* index 19 */
   "local4",    /* index 20 */
   "local5",    /* index 21 */
   "local6",    /* index 22 */
   "local7",    /* index 23 */
};


/* FUNCTION: sl_logger()
 * 
 * Send logs from command-line similar to BSD "logger" utility.
 *
 * PARAM1: void * pio
 *
 * RETURNS: 
 */

int
sl_logger(void * pio)
{
   ns_printf(pio,"Yet to be implemented.\n");
   return 0;
}

/* FUNCTION: sl_set_syslogd()
 * 
 * Set the IP address of default syslog servers.
 * Usage  : slsetd <num> <addr>
 * Example: slsetd 0 10.0.0.20
 *          slsetd 1 10.0.0.90
 *          slsetd 2 fe80::2a0:ccff:fee0:ac23
 *
 * LOG_MAXSVRS defines the max. number of syslog servers supported.
 *
 * PARAM1: void * pio
 *
 * RETURNS: 
 */

int
sl_set_syslogd(void * pio)
{
   int index;
   char * cp = nextarg(((GEN_IO)pio)->inbuf);    
   char * arg;
   if (!*cp) /* see if user put addr on cmd line */
   {
      ns_printf(pio, "Usage  : slsetd <num> <addr>\n");
      ns_printf(pio, "Example: slsetd 0 10.0.0.20\n");
      ns_printf(pio, "         slsetd 1 10.0.0.90\n");
      ns_printf(pio, "         slsetd 2 fe80::2a0:ccff:fee0:ac23\n");
      return -1;
   }

   index = atoi(cp);
   if (index < 0 || index >= LOG_MAXSVRS)
   {
      ns_printf(pio, "Usage  : slsetd <num> <addr>\n");
      ns_printf(pio, "Number should be between 0 and %d\n",LOG_MAXSVRS);
      return -1;
   }

   arg  = nextarg(cp);
   if (!arg)
   {
      ns_printf(pio, "Usage  : slsetd <num> <addr>\n");
      ns_printf(pio, "addr should be a valid IPv4/IPv6 address.\n");
      return -1;
   }

   /* Read the IPv4/IPv6 address */
   inet46_addr(arg,&sl_defparms.syslogd[index]);

   /* cleanup the prevailing entry for default servers */
   closelogfac(sl_defval.facility);

   /* The next call to syslog() will use the new default address. 
    * It will find that sl_def is NULL and so it will call sl_create_default()
    * to create an entry for the default facility.
    */

   return 0;
}

/* FUNCTION: sl_enable()
 * 
 * Enable/disable syslog.
 * Usage  : slenable <flag>
 * Example: slsetd 0 
 *          slsetd 1
 *
 * PARAM1: void * pio
 *
 * RETURNS: 
 */

int
sl_enable(void * pio)
{
   int value;
   char * cp = nextarg(((GEN_IO)pio)->inbuf);    
   if (!*cp) /* see if user put addr on cmd line */
   {
      ns_printf(pio, "Current syslog client setting : %s\n",
         sl_defparms.enable ? "enabled" : "disabled" ); 
      ns_printf(pio, "Usage  : slsetd <flag>\n");
      ns_printf(pio, "         flag = 1/0 (enable/disable)");
      ns_printf(pio, "Example: slsetd 0\n");
      ns_printf(pio, "         slsetd 1\n");
      return -1;
   }

   value = atoi(cp);
   if (value < 0 || value > 1)
   {
      ns_printf(pio, "Usage  : slsetd <flag>\n");
      ns_printf(pio, "         flag = 1/0 (enable/disable)");
      return -1;
   }

   sl_defparms.enable = value;
   ns_printf(pio, "New syslog client setting : %s\n",
      sl_defparms.enable ? "enabled" : "disabled" ); 

   return 0;
}

/* FUNCTION: sl_stats()
 * 
 * Show statistics for syslog client.
 *
 * PARAM1: void * pio
 *
 * RETURNS: 
 */
int
sl_stats(void * pio)
{
   int i,len;
   ns_printf(pio, "Current syslog client setting : %s\n",
      sl_defparms.enable ? "enabled" : "disabled" ); 
   ns_printf(pio, "Statistics for syslog client.\n");
   ns_printf(pio, " num of syslog() calls = %ld\n", slstats.callcnt);
   for (i=0 ; i < SEVERITY_MAX; i++)
   {
      if (i && (i%4 == 0))
         ns_printf(pio,"\n");
      ns_printf(pio, " %s = %ld", severitynames[i], slstats.severity[i]);
   }
   for (i=0 ; i < LOG_NFACILITIES; i++)
   {
      if (i%4 == 0)
         ns_printf(pio,"\n");
      ns_printf(pio, " %s = %ld", facnames[i], slstats.facility[i]);
   }
   ns_printf(pio, "\nDefault values: logopt = %d, facility = %s(%d)\n",
      sl_defval.logopt, facnames[sl_defval.facility>>3], 
      sl_defval.facility);
   for (i=0; i < LOG_MAXSVRS; i++)
   {
      ns_printf(pio, " syslogd%d = %s, Port = %d\n", i+1,
         print46_addr(&sl_defparms.syslogd[i]), LOG_PORT);
   }

   len = niche_list_len(sl_list);
   if (len)
   {
      syslog_info *tmp;
      struct sockaddr_in *sin;
      ns_printf(pio,"Individual settings for each facility:\n");
      for (i=0; i < len; i++)
      {
         tmp = (syslog_info *)niche_list_getat(sl_list,i);
         if (!tmp)
         {
            dtrap();
            continue;  /* can this happen ? */
         }
         ns_printf(pio, " facility = %s(%d), iden = %s, logopt = %d\n",
            facnames[tmp->facility>>3], tmp->facility, tmp->iden, tmp->logopt);
         sin = (struct sockaddr_in *) &tmp->sa;
         ns_printf(pio, " IP Addr = %s, Log filename = %s\n",
            INET_NTOA(sin->sin_addr.s_addr), tmp->fname);
      }
   }
#ifdef SYSLOG_SOCKETS
   ns_printf(pio,"Syslog client uses sockets API for network I/O.\n");
#else
   ns_printf(pio,"Syslog client uses lightweight API for network I/O.\n");
#endif
   return 0;
}

/* FUNCTION: sl_test1()
 * 
 * Call syslog() to send logs from command-line.
 *
 * PARAM1: void * pio
 *
 * RETURNS: 
 */

int
sl_test1(void * pio)
{
   syslog(LOG_INFO|LOG_SYSLOG, "Running syslog command slt1");
   ns_printf(pio,"Log message sent to default syslog server.\n");

   return 0;
}

/* FUNCTION: sl_test2()
 * 
 * Call openlog(), syslog() to send logs from command-line.
 *
 * PARAM1: void * pio
 *
 * RETURNS: 
 */

int
sl_test2(void * pio)
{
   openlog("InicheLog", (LOG_CONS|LOG_FILE), LOG_SYSLOG);
   syslog(LOG_INFO|LOG_SYSLOG, "Running syslog command slt2");

   ns_printf(pio,"Log message sent to default syslog server.\n");
   ns_printf(pio,"Also displayed log message on console - pl. verify.\n");

   USE_VOID(pio);
   return 0;
}

/* FUNCTION: sl_test3()
 * 
 * Call openlog(), syslog(), closelog() to send logs from command-line.
 *
 * PARAM1: void * pio
 *
 * RETURNS: 
 */

int
sl_test3(void * pio)
{
   openlog("InicheLog", (LOG_CONS|LOG_FILE), LOG_SYSLOG);
   syslog(LOG_INFO|LOG_SYSLOG, "Running syslog command slt3");
   closelog();

   ns_printf(pio,"Log message sent to default syslog server.\n");
   ns_printf(pio,"Also displayed log message on console - pl. verify.\n");
   ns_printf(pio,"closelog() must have cleared settings. Verify via slstats.\n");

   USE_VOID(pio);
   return 0;
}

/* FUNCTION: sl_test4()
 * 
 * Call openlog(), openlogaddr(), syslog(), closelogfac() to send 
 * to a specific syslog server. 
 *
 * PARAM1: void * pio
 *
 * RETURNS: 
 */

#define SL_L0FILENAME   "syslocal.log"

int
sl_test4(void * pio)
{
   char *   arg2;
   struct sockaddr_in sa;
   int fac = LOG_LOCAL0;
  
   arg2 = nextarg(((GEN_IO)pio)->inbuf);
   if ( (arg2 == NULL) || arg2[0] == 0 )
   {
      ns_printf(pio,"Run syslog test 4.\n");
      ns_printf(pio,"Usage:slt3 <syslogd IPaddress in dotted form>\n");
      return SUCCESS ;
   }

   ns_printf(pio, "Starting test4. Facility used = %s(%d). Log filename = %s\n", 
      facnames[fac>>3], fac, SL_L0FILENAME );
   openlog("InicheLog", (LOG_CONS|LOG_FILE), fac);
   sa.sin_family = AF_INET;
   sa.sin_addr.s_addr = inet_addr(arg2);
   sa.sin_port = htons(LOG_PORT);  
   openlogaddr(fac, (struct sockaddr *)&sa, 
      sizeof(struct sockaddr_in),  SL_L0FILENAME);
   syslog(LOG_INFO|fac, "Running syslog command slt4");
   closelogfac(fac);

   ns_printf(pio,"Log message sent to default syslog server.\n");
   ns_printf(pio,"Also displayed log message on console - pl. verify.\n");
   ns_printf(pio,"closelog() must have cleared settings. Verify via slstats.\n");

   USE_VOID(pio);
   return SUCCESS;
}

/* FUNCTION: sl_testsev()
 * 
 * Call syslog() to send logs from command-line.
 * Send one log for each severity.
 *
 * PARAM1: void * pio
 *
 * RETURNS: 
 */

int
sl_testsev(void * pio)
{
   int i;
   for (i=0; i < SEVERITY_MAX; i++)
      syslog(i|LOG_SYSLOG, "syslog clnt - testsev %s", severitynames[i]);

   ns_printf(pio,"Log message sent to default syslog server.\n");

   return 0;
}

/* FUNCTION: sl_testfac()
 * 
 * Call syslog() to send logs from command-line.
 * Send one log for each facility.
 *
 * PARAM1: void * pio
 *
 * RETURNS: 
 */

int
sl_testfac(void * pio)
{
   int i;
   for (i=0; i < LOG_NFACILITIES; i++)
      syslog(LOG_INFO|(i<<3), "syslog clnt - testfac %s", facnames[i]);
   ns_printf(pio,"Log message sent to default syslog server.\n");
   return 0;
}

/* FUNCTION: sl_testdate()
 * 
 * Test the function sl_print_date()
 *
 * PARAM1: void * pio
 *
 * RETURNS: 
 */

int
sl_testdate(void * pio)
{
   char buf[40];

   sl_print_date(buf,0,sysuptime());
   ns_printf(pio,"Today's date:        %s\n", buf);

   sl_print_date(buf,0,360000*24*3); /* advance 3 days */
   ns_printf(pio,"3 days from start:   %s\n", buf);

   sl_print_date(buf,0,360000*24*30); /* advance 30 days */
   ns_printf(pio,"30 days from start:  %s\n", buf);

   sl_print_date(buf,0,(unsigned long)360000*24*365); /* advance 365 days */
   ns_printf(pio,"365 days from start: %s\n", buf);

   sl_print_date(buf,0,(unsigned long)360000*24*395); /* advance 395 years */
   ns_printf(pio,"395 days from start: %s\n", buf);

   return 0;
}

/* FUNCTION: sl_testall()
 * 
 * Run all the individual syslog client tests.
 *
 * PARAM1: void * pio
 *
 * RETURNS: 
 */

int
sl_testall(void * pio)
{
   sl_test1(pio);
   sl_test2(pio);
   sl_test3(pio);
   /* slt4 test needs an argument */
   strcpy(((GEN_IO)pio)->inbuf, "slt4 ");
   strcat(((GEN_IO)pio)->inbuf, LOG_IPADDR1);
   sl_test4(pio);
   sl_testsev(pio);
   sl_testfac(pio);
   return 0;
}

int sl_nvinfo(void * pio);

struct menu_op slogmenu[] = {
 {  "syslog",   stooges,     "syslog client commands"        },
 {  "logger",   sl_logger,   "send logs to syslog server"    },
 {  "slstat",   sl_stats,    "display syslog statistics",    },
 {  "slsetd",   sl_set_syslogd,"set default syslogd addr",   },
 {  "slenable", sl_enable,   "enable/disable syslog client", },
 {  "slt1",     sl_test1,    "test syslog()",                },
 {  "slt2",     sl_test2,    "test openlog(), syslog()",     },
 {  "slt3",     sl_test3,    "test open, send, closelog()",  },
 {  "slt4",     sl_test4,    "test sending to diff syslogd", },
 {  "sltsev",   sl_testsev,  "test all severities"           },
 {  "sltfac",   sl_testfac,  "test all facilities"           },
 {  "sltall",   sl_testall,  "run all the above tests"       },
 {  "sltdate",  sl_testdate ,"test the date calculation func"},
#ifdef INCLUDE_NVPARMS
 {  "slnvinfo", sl_nvinfo   ,"info. read from nv file"       },
#endif
 {  NULL,       NULL,        NULL,                           },
};


#endif /* IN_MENUS */


#ifdef INCLUDE_NVPARMS

#include "nvparms.h"
#include "ip.h"

/* Please see nvparms.h and nvparms.c regarding the usage of
 * the following datatypes and functions.
 */
struct nvparm_info sl_formats[] = 
{
   {"syslogd1: %s\n"           , NVIP46DEC , NVBND_NOOP  , \
    &sl_defparms.syslogd[0]    , NULL, }, 
   {"syslogd2: %s\n"           , NVIP46DEC , NVBND_NOOP  , \
    &sl_defparms.syslogd[1]    , NULL, }, 
   {"syslog file: %s\n"        , NVSTRING, MAX_NVSTRING  , \
    &sl_defparms.logfile[0]    , NULL, }, 
   {"syslog year: %u\n"        , NVUNSHORT, NVBND_NOOP   , \
    &sl_defparms.year          , NULL, }, 
   {"syslog month: %u\n"       , NVUNSHORT, NVBND_NOOP   , \
    &sl_defparms.month         , NULL, }, 
   {"syslog day: %u\n"         , NVUNSHORT, NVBND_NOOP   , \
    &sl_defparms.day           , NULL, }, 
   {"syslog enable: %u\n"      , NVUNSHORT, NVBND_NOOP   , \
    &sl_defparms.enable        , NULL, }, 
   {"syslog locallog: %u\n"    , NVUNSHORT, NVBND_NOOP   , \
    &sl_defparms.locallog      , NULL, }, 
};

#define NUMSL_FORMATS (sizeof(sl_formats)/sizeof(struct nvparm_info))

/* FUNCTION: sl_nvset()
 * 
 * PARAM1: NV_FILE * fp
 *
 * RETURNS: Silent return of 0 for OK
 */
int sl_nvset(NV_FILE * fp)
{
   int i = 0;
   /* syslog client setup: */
   nv_fprintf(fp, sl_formats[i++].pattern, print46_addr(&sl_defparms.syslogd[0]));
   nv_fprintf(fp, sl_formats[i++].pattern, print46_addr(&sl_defparms.syslogd[1]));
   nv_fprintf(fp, sl_formats[i++].pattern, sl_defparms.logfile);
   nv_fprintf(fp, sl_formats[i++].pattern, sl_defparms.year);
   nv_fprintf(fp, sl_formats[i++].pattern, sl_defparms.month);
   nv_fprintf(fp, sl_formats[i++].pattern, sl_defparms.day);
   nv_fprintf(fp, sl_formats[i++].pattern, sl_defparms.enable);
   nv_fprintf(fp, sl_formats[i++].pattern, sl_defparms.locallog);
   return 0;
}

struct nvparm_format sl_format = 
{
   NUMSL_FORMATS,
   &sl_formats[0],
   sl_nvset,
   NULL
};

#ifdef IN_MENUS
/* FUNCTION: sl_nvinfo()
 * 
 * PARAM1: void * pio - output stream
 *
 * RETURNS: 
 */
int sl_nvinfo(void * pio)
{
   int i = 0;
   /* print syslog client info read from nv file */
   ns_printf(pio, sl_formats[i++].pattern, print46_addr(&sl_defparms.syslogd[0]));
   ns_printf(pio, sl_formats[i++].pattern, print46_addr(&sl_defparms.syslogd[1]));
   ns_printf(pio, sl_formats[i++].pattern, sl_defparms.logfile);
   ns_printf(pio, sl_formats[i++].pattern, sl_defparms.year);
   ns_printf(pio, sl_formats[i++].pattern, sl_defparms.month);
   ns_printf(pio, sl_formats[i++].pattern, sl_defparms.day);
   ns_printf(pio, sl_formats[i++].pattern, sl_defparms.enable);
   ns_printf(pio, sl_formats[i++].pattern, sl_defparms.locallog);
   return 0;
}
#endif /* IN_MENUS */
#endif /* INCLUDE_NVPARMS */

/* FUNCTION: syslog_init()
 * 
 * Initialize the syslog client.
 * - Initialize sl_list 
 * - Initialize default values (from those read from nv file)
 *
 * PARAM1: -
 *
 * RETURNS: SUCCESS / error code
 * Returns 0 (SUCCESS) on success.
 */

int syslog_init(void)
{
   syslog(LOG_INFO|LOG_SYSLOG, "syslog client started");
   return SUCCESS;
}


/* FUNCTION: prep_syslog()
 * 
 * Prepare the syslog client to work with rest of the system.
 * - Install syslog client's sub-menu
 * - Install syslog client's nvparms
 *
 * PARAM1: -
 *
 * RETURNS: SUCCESS / error code
 * Returns 0 (SUCCESS) on success.
 */
int prep_syslog(void)
{
   int e = 0;
   if(syslog_init_flag)
   {
      /* already initialized */
      return(0);
   }
   niche_list_constructor(sl_list, sizeof(syslog_info));
#ifdef USE_SYSLOG_QUEUE
   /* create and initialize syslog queue */
   memset(&syslog_queue, 0, sizeof(queue));
#endif
   /* create syslog timer */
#ifndef USE_SYSLOG_TASK
   syslog_timer = in_timerset(syslog_handler, 1000, 0L);
   if(syslog_timer == 0)
   {
      dprintf("failed to create syslog timer\n");      
      return(-1);
   }
#endif
#ifdef IN_MENUS
   e = install_menu(&slogmenu[0]);
#endif
#ifdef INCLUDE_NVPARMS
   e = install_nvformat(&sl_format, nv_formats);
   if(e)
      dprintf("unable to install syslog NVPARMS, reconfigure nv_formats[]\n");
   if(LOG_MAXSVRS < 2)
   {
      /* with nvparms, LOG_MAXSVRS should be 2 or more */
      dtrap();
   }
#endif   /* INCLUDE_NVPARMS */
#ifdef LOG_IPADDR1
   inet46_addr(LOG_IPADDR1, &sl_defparms.syslogd[0]);
   inet_setport(&sl_defparms.syslogd[0], LOG_PORT);
#endif
#ifdef LOG_IPADDR2
   inet46_addr(LOG_IPADDR2, &sl_defparms.syslogd[1]);
   inet_setport(&sl_defparms.syslogd[1], LOG_PORT);
#endif
   syslog_init_flag = 1;
   return e;
}

#ifdef USE_SYSLOG_QUEUE
void syslog_handler(long foo)
{
   int ret_value;
   int facility;
   syslog_info *tmp;
   syslog_item_st *sl_item = NULL;
   if(syslog_init_flag == 0)
   {
      return;
   }
   while(1)
   {
      /* May be the best approach would be to send upto like n syslogs and then return */
      sl_item = (syslog_item_st *) getq(&syslog_queue);  /* de-queue item from head of queue */
      if(sl_item == NULL)
      {
         break;
      }
      facility = sl_item->priority & LOG_FACMASK;
      /* If we have specific info for this facility, use it.
       * Else use the default information.
       */
      tmp = (syslog_info *)niche_lookup_id(sl_list, facility);
      if(!tmp || !tmp->sa_len)
      {
         if (!sl_def)
            sl_create_default();
         if (!tmp)
            tmp = sl_def;   /* use the default settings */
      }
      /* log to console */
      if (tmp->logopt & LOG_CONS)
         ns_printf(NULL, "%s\n", sl_item->outbuf);
      /* log to file */
      if((tmp->logopt & LOG_FILE) && tmp->fp)
      {
         vfwrite(sl_item->outbuf, 1, strlen(sl_item->outbuf), tmp->fp);
         vfwrite(LOG_CRLF, 1, 2, tmp->fp);
      }   
      ret_value = sl_udpsend(tmp, sl_item->outbuf, strlen(sl_item->outbuf));
      npfree(sl_item->outbuf); 
      npfree(sl_item); 
   }
   USE_ARG(foo);
}

void syslog(int priority, const char * msg, ...)
{
   syslog_item_st *sl_item = NULL;
   syslog_info *tmp;
   char *outbuf2;
   int ret_value;
#if defined(NATIVE_PRINTF) || defined(PRINTF_STDARG)
   va_list argList;
#else /* NATIVE_PRINTF || PRINTF_STRING */
   int * next_arg = (int *) &msg;
#endif   /* NATIVE_PRINTF || PRINTF_STRING */
   /* priority = (facility | severity) */
   int severity = LOG_PRI(priority);
   int facility = priority & LOG_FACMASK;

#if !defined(NATIVE_PRINTF) && !defined(PRINTF_STDARG)
   next_arg +=  sizeof(char *)/sizeof(int) ;
#endif

   if(syslog_init_flag == 0)
   {
      return;
   }
   /* check if syslog client is enabled or not */
   if (!sl_defparms.enable)
      return;  /* do nothing - because syslog client is disabled */
   /* ignore severites disabled via sl_mask */
   if (!((1 << severity) & sl_mask))
      return;
   
   sl_item = (syslog_item_st *) npalloc(sizeof(syslog_item_st)); 
   if(sl_item == NULL)
   {
      return;
   }
   sl_item->priority = priority;
   sl_item->buf_size = MAXIOSIZE;

   /* Allocate memory for the output string 
    * If the format string is greater than MAXIOSIZE, then
    * we surely need to allocate a bigger block
    */
   ret_value = strlen(msg); 
   if ( ret_value >= MAXIOSIZE )
   {
      sl_item->buf_size += ret_value ;
   }
   sl_item->outbuf = (char *)npalloc(sl_item->buf_size); 
   if(sl_item->outbuf == NULL)
   {
      npfree(sl_item);
      return;
   }
   /* If we have specific info for this facility, use it.
    * Else use the default information.
    */
   tmp = (syslog_info *)niche_lookup_id(sl_list, facility);
   if(!tmp || !tmp->sa_len)
   {
      if (!sl_def)
         sl_create_default();
      if (!tmp)
         tmp = sl_def;   /* use the default settings */
   }
   /* update counters */
   slstats.callcnt++;
   slstats.severity[severity]++;
   slstats.facility[facility>>3]++;
   /* Now populate the output string */
   sl_print_date(sl_item->outbuf, priority, sysuptime());
   strcat(sl_item->outbuf, tmp->iden);
   strcat(sl_item->outbuf, ":");
   outbuf2 = sl_item->outbuf + strlen(sl_item->outbuf);
#ifdef NATIVE_PRINTF
   /* use the target system's ANSI routines */
   va_start(argList, msg);
   ret_value = vsprintf(outbuf2, msg, argList);
   va_end(argList);
#else /* NATIVE_PRINTF */
   /* use the homegrown routines */
#ifdef PRINTF_STDARG
   va_start(argList,msg);
   doprint(outbuf2, sl_item->buf_size, (char *)msg, argList);
   va_end(argList);
#else    /* not PRINTF_STDARG */
   doprint(outbuf2, sl_item->buf_size, msg, next_arg);
#endif   /* PRINTF_STDARG */
#endif   /* NATIVE_PRINTF */
#ifdef NATIVE_PRINTF
   /* Check if we have overwritten the output buffer */
   if((int)strlen(sl_item->outbuf) > sl_item->buf_size)
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
      sl_item->outbuf[sl_item->buf_size-1] = 0;   /* Null terminate the string */
   }
#endif
   /* we are done here: add it to queue */
   putq(&syslog_queue, (void*) sl_item);  /* add item to the tail of syslog queue */
}
#else
void syslog(int priority, const char * msg, ...)
{
   syslog_info *tmp;
   char *outbuf = NULL;
   char *outbuf2;
   int ret_value;
   int buf_size =  MAXIOSIZE   ;
#if defined(NATIVE_PRINTF) || defined(PRINTF_STDARG)
   va_list argList;
#else /* NATIVE_PRINTF || PRINTF_STRING */
   int * next_arg = (int *) &msg;
   next_arg +=  sizeof(char *)/sizeof(int) ;
#endif   /* NATIVE_PRINTF || PRINTF_STRING */
   /* priority = (facility | severity) */
   int severity = LOG_PRI(priority);
   int facility = priority & LOG_FACMASK;
   /* check if syslog client is enabled or not */
   if (!sl_defparms.enable)
      return;  /* do nothing - because syslog client is disabled */
   /* ignore severites disabled via sl_mask */
   if (!((1 << severity) & sl_mask))
      return;
   /* If we have specific info for this facility, use it.
    * Else use the default information.
    */
   tmp = (syslog_info *)niche_lookup_id(sl_list, facility);
   if(!tmp || !tmp->sa_len)
   {
      if (!sl_def)
         sl_create_default();
      if (!tmp)
         tmp = sl_def;   /* use the default settings */
   }
   /* update counters */
   slstats.callcnt++;
   slstats.severity[severity]++;
   slstats.facility[facility>>3]++;
   /* Allocate memory for the output string 
    * If the format string is greater than MAXIOSIZE, then
    * we surely need to allocate a bigger block
    */
   ret_value = strlen(msg); 
   if ( ret_value >= MAXIOSIZE )
   {
      buf_size += ret_value ;
   }
   outbuf = (char *)npalloc(buf_size); 
   if(outbuf == NULL)
   {
      return ;
   }
   /* Now populate the output string */
   sl_print_date(outbuf, priority, sysuptime());
   strcat(outbuf, tmp->iden);
   strcat(outbuf, ":");
   outbuf2 = outbuf + strlen(outbuf);
#ifdef NATIVE_PRINTF
   /* use the target system's ANSI routines */
   va_start(argList, msg);
   ret_value = vsprintf(outbuf2, msg, argList);
   va_end(argList);
#else /* NATIVE_PRINTF */
   /* use the homegrown routines */
#ifdef PRINTF_STDARG
   va_start(argList,msg);
   doprint(outbuf2,buf_size,(char *)msg,argList);
   va_end(argList);
#else    /* not PRINTF_STDARG */
   doprint(outbuf2,buf_size,msg,next_arg);
#endif   /* PRINTF_STDARG */
#endif   /* NATIVE_PRINTF */
#ifdef NATIVE_PRINTF
   /* Check if we have overwritten the output buffer */
   if ( (int)strlen(outbuf) > buf_size )
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
   /* log to console */
   if (tmp->logopt & LOG_CONS)
      ns_printf(NULL,"%s\n",outbuf);
   /* log to file */
   if ((tmp->logopt & LOG_FILE) && tmp->fp)
   {
      vfwrite(outbuf,1,strlen(outbuf),tmp->fp);
      vfwrite(LOG_CRLF,1,2,tmp->fp);
   }   
   ret_value = sl_udpsend(tmp, outbuf, strlen(outbuf));

   /* Free memory for the output string */
   npfree(outbuf); 

   /* since syslog() can get called repeatedly down in the bowels 
    * of a single command interpreting function, spin tk_yield() so 
    * that some packets get a chance to get received 
    */
   tk_yield();

   return ;
}
#endif

#ifdef USE_SYSLOG_QUEUE
#ifdef USE_SYSLOG_TASK
#ifndef SUPERLOOP

TK_OBJECT(to_syslog);
TK_ENTRY(tk_syslog);
long syslog_wakes = 0;

/*
 * Altera Niche Stack Nios port modification:
 * Create task entries for all optional add-on apps
 */
struct inet_taskinfo syslog_task = 
{
   &to_syslog,
   "syslog task",
   tk_syslog,
   TK_SYSLOG_TPRIO,
   TK_SYSLOG_SSIZE,
};

void syslog_cleanup(void)
{
   return;
}

/* The application thread works on a "controlled polling" basis: 
 * it wakes up periodically and polls for work.
 *
 * The FTP task could aternativly be set up to use blocking sockets,
 * in which case the loops below would only call the "xxx_check()"
 * routines - suspending would be handled by the TCP code.
 */

/* FUNCTION: tk_syslog
 * 
 * PARAM1: n/a
 *
 * RETURNS: n/a
 */

TK_ENTRY(tk_syslog)
{
   int err;
   while (!iniche_net_ready)
      TK_SLEEP(1);
   err = syslog_init();
   if( err == SUCCESS )
   {
      exit_hook(syslog_cleanup);
   }
   else
   {
      dtrap();  
   }
   for (;;)
   {
      syslog_handler(0);         /* will block on select */
      tk_yield();          /* give up CPU in case it didn't block */
      syslog_wakes++;   /* count wakeups */
      if (net_system_exit)
         break;
   }
   TK_RETURN_OK();
}

#endif /* not SUPERLOOP */
#endif /* USE_SYSLOG_TASK */
#endif /* USE_SYSLOG_QUEUE */

#endif /* INICHE_SYSLOG */
