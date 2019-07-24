/*
 * FILENAME: syslog.h
 *
 * Copyright 2004 By InterNiche Technologies Inc. All rights reserved
 *
 * Definitions syslog client
 *
 * MODULE: MISCLIB
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

#ifndef _SYS_SYSLOG_H_
#define _SYS_SYSLOG_H_

#include "tcpapp.h"   /* for definition for sockaddr */

#define _PATH_LOG     "sys.log"     /* name of log file */
#define LOG_PORT      514           /* port of syslog server */
#define LOG_IPADDR1   "10.0.0.20"   /* ipaddress of syslog server */
#define LOG_IPADDR2   "fe80::20c:6eff:fe6c:8e46"  /* syslogd IP address */
#define LOG_CRLF      "\r\n"        /* CRLF for the file system */
#define LOG_YEAR      2004          /* start date(year) for the stack */
#define LOG_MONTH     9             /* start date(month) for the stack */
#define LOG_DAY       2             /* start date(day) for the stack */
#define LOG_MAXSVRS   3            /* max number of default syslogds */

/*
 * priorities/facilities are encoded into a single 32-bit quantity, where the
 * bottom 3 bits are the priority (0-7) and the top 28 bits are the facility
 * (0-big number).  Both the priorities and the facilities map roughly
 * one-to-one to strings in the syslogd(8) source code.  This mapping is
 * included in this file.
 *
 * priorities (these are ordered)
 */
#define LOG_EMERG     0     /* system is unusable */
#define LOG_ALERT     1     /* action must be taken immediately */
#define LOG_CRIT      2     /* critical conditions */
#define LOG_ERR       3     /* error conditions */
#define LOG_WARNING   4     /* warning conditions */
#define LOG_NOTICE    5     /* normal but significant condition */
#define LOG_INFO      6     /* informational */
#define LOG_DEBUG     7     /* debug-level messages */
#define SEVERITY_MAX  8     /* total number of severities */

#define LOG_PRIMASK   0x07  /* mask to extract priority part (internal) */
/* extract priority */
#define LOG_PRI(p)            ((p) & LOG_PRIMASK)
#define LOG_MAKEPRI(fac, pri) ((fac) | (pri))


/* facility codes */
/* Facility #10 clashes in DEC UNIX, where                */
/* it's defined as LOG_MEGASAFE for AdvFS  event logging. */
#define LOG_KERN     (0<<3)   /* kernel messages */
#define LOG_USER     (1<<3)   /* random user-level messages */
#define LOG_MAIL     (2<<3)   /* mail system */
#define LOG_DAEMON   (3<<3)   /* system daemons */
#define LOG_AUTH     (4<<3)   /* authorization messages */
#define LOG_SYSLOG   (5<<3)   /* messages generated internally by syslogd */
#define LOG_LPR      (6<<3)   /* line printer subsystem */
#define LOG_NEWS     (7<<3)   /* network news subsystem */
#define LOG_UUCP     (8<<3)   /* UUCP subsystem */
#define LOG_CRON     (9<<3)   /* clock daemon */
#define LOG_AUTHPRIV (10<<3)  /* authorization messages (private) */
#define LOG_FTP      (11<<3)  /* ftp daemon */
#define LOG_NTP      (12<<3)  /* NTP subsystem */
#define LOG_SECURITY (13<<3)  /* security subsystems (firewalling, etc.) */
#define LOG_CONSOLE  (14<<3)  /* /dev/console output */
/* other codes through 15 reserved for system use */
#define LOG_LOCAL0   (16<<3)  /* reserved for local use */
#define LOG_LOCAL1   (17<<3)  /* reserved for local use */
#define LOG_LOCAL2   (18<<3)  /* reserved for local use */
#define LOG_LOCAL3   (19<<3)  /* reserved for local use */
#define LOG_LOCAL4   (20<<3)  /* reserved for local use */
#define LOG_LOCAL5   (21<<3)  /* reserved for local use */
#define LOG_LOCAL6   (22<<3)  /* reserved for local use */
#define LOG_LOCAL7   (23<<3)  /* reserved for local use */

#define LOG_NFACILITIES   24  /* current number of facilities */
#define LOG_FACMASK   0x03f8  /* mask to extract facility part */
/* facility of pri */
#define LOG_FAC(p)   (((p) & LOG_FACMASK) >> 3)


/*
 * arguments to setlogmask.
 */
#define LOG_MASK(pri) (1 << (pri))           /* mask for one priority */
#define LOG_UPTO(pri) ((1 << ((pri)+1)) - 1) /* all priorities through pri */

/*
 * Option flags for openlog.
 *
 * LOG_ODELAY no longer does anything.
 * LOG_NDELAY is the inverse of what it used to be.
 */
#define LOG_PID      0x01   /* log the pid with each message */
#define LOG_CONS     0x02   /* log on the console if errors in sending */
#define LOG_ODELAY   0x04   /* delay open until first syslog() (default) */
#define LOG_NDELAY   0x08   /* don't delay open */
#define LOG_NOWAIT   0x10   /* don't wait for console forks: DEPRECATED */
#define LOG_PERROR   0x20   /* log to stderr as well */
#define LOG_FILE     0x40   /* Iniche specific - log to file */


#ifdef INICHE_SYSLOG
int  syslog_init(void);
void closelog   (void);
void openlog    (const char * iden, int logopt, int facility);
void syslog     (int priority, const char * msg, ...);
int  setlogmask (int maskpri);
void openlogaddr(int facility, struct sockaddr *sa, int sa_len, char *fname);
void closelogfac(int facility);
#else
/* INICHE_SYSLOG is not defined. Make all calls as "noop". */
#define openlog
#define setlogmask
#define syslog
#define openlogaddr
#define closelogfac
#endif /* else of INICHE_SYSLOG */

/* vsyslog is not supported
void vsyslog    (int priority, const char * msg, va_list args);
*/

#define  INET_NTOA         print_ipad

#endif  /* _SYS_SYSLOG_H_ */
