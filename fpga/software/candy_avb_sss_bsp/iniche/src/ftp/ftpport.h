/*
 * FILENAME: FTPPORT.H
 *
 * Copyright 1997- 2000 By InterNiche Technologies Inc. All rights reserved
 *
 *
 * MODULE: FTP
 *
 * ROUTINES:
 *
 * PORTABLE: yes
 */

/* ftpport.h FTP server's per-port definitions. 
 * 1/12/97 - Created as part of cleanup. John Bartas 
 */

#ifndef _FTPPORT_H_
#define  _FTPPORT_H_    1

#include "ipport.h"
#include "libport.h"
#include "tcpapp.h"
#include "userpass.h"

/* Implementation defines: */
#define  FTPMAXPATH     168   /* max path length, excluding file name */
#define  FTPMAXFILE     16    /* max file name length w/o path */
#define CMDBUFSIZE      256
/* #define  CMDBUFSIZE     1024  // Texas Imperial Sw's WFTD sends a BIG welcome str */


#ifndef FILEBUFSIZE     /* Allow override from ipport.h */
#define  FILEBUFSIZE    (6*1024)
#endif   /* FILEBUFSIZE */


#ifdef MAX_USERLENGTH
#define  FTPMAXUSERNAME    MAX_USERLENGTH
#define  FTPMAXUSERPASS    MAX_USERLENGTH
#else
#define  FTPMAXUSERNAME    32
#define  FTPMAXUSERPASS    24
#endif

/* default port for FTP data transfers. This can default to 20 (as 
 * some interpret the RFC as recommending) or default to 0 to let the 
 * sockets layer pick a port randomly. It could even point to a user 
 * provided routine which determines a port number alogrithmically.
 */
#define  FTP_DATAPORT      20


/* set up file system options for target system */

#ifdef UNIX_VFS
#define  FTP_SLASH   '/'   /* use UNIX style slash */
#else
#if ((!defined(AMD_NET186)) && (!defined(ACE_360)) && (!defined(ARM_PID)))
#define  DRIVE_LETTERS     1     /* track drive as well as directory */
#endif
#define  DEFAULT_DRIVE   "c:"
#define  FTP_SLASH   '\\'     /* use DOS style slash */
#endif

/* define clock tick info for DOS */
#define  FTPTPS   TPS      /* number of ftps_loop calls per second */
#define  ftpticks cticks


#ifdef MINI_TCP   /* mini-sockets */
/* Set the type of indentifier sockets will have */
#define  SO_GET_FPORT(so) (((M_SOCK)so)->fport)
#define  SO_GET_LPORT(so) (((M_SOCK)so)->lport)
#else    /* not MINI_TCP, use BSDish sockets */
unshort  SO_GET_FPORT(WP_SOCKTYPE so);
unshort  SO_GET_LPORT(WP_SOCKTYPE so);
#endif


/* Added sys_ routines for FTP support */
SOCKTYPE t_tcplisten(u_short * lport, int doamin);
SOCKTYPE t_tcpopen(ip_addr host, u_short lport, u_short fport);


/* define number of read-sends to do per ftp_check(). For best 
 * performance, this times the FILEBUFSIZE should be about equal 
 * to anicipated TCP window.
 */
#ifndef MAXSENDLOOPS    /* allow tuning from ipport.h */
#ifdef TCP_ZEROCOPY
#define  MAXSENDLOOPS   12 /* MAX number of loops */
#else
#define  MAXSENDLOOPS   1  /* MAX number of loops */
#endif   /* not TCP_ZEROCOPY */
#endif   /* no MAXSENDLOOPS */

/* Configurable limit on max number of ftp sessions.  Setting this value to 0 or
 * -1 results in no limitation on number of sessions.  The default value is 32.
 */
#ifndef MAX_FTPS_SESS   /* allow tuning from ipport.h */
#define MAX_FTPS_SESS 32
#endif  /* MAX_FTPS_SESS */

/* map ftp's timer tick count to system's */
#define  ftp_ticks   cticks

/* map FTP server's alloc and free to local mem library */
#define  FTPSALLOC(size)   npalloc(size)
#define  FTPSFREE(ptr)     npfree(ptr)

/* FTP Client related non-volatile parameters. Please see nvparms.h 
 * and nvparms.c regarding the usage of the following structure.
 */
#ifdef INCLUDE_NVPARMS
#ifdef FTP_CLIENT
struct ftpc_nvparam
{
   int  fc_replytmo    ;  /* secs of inactivity (for a cmd) for timeout*/
   int  fc_activity_tmo;  /* ftp client's inactivity timeout (secs) */
};

extern struct ftpc_nvparam ftpc_nvparms;
#endif /* FTP_CLIENT */
#endif /* INCLUDE_NVPARMS */

#endif   /* _FTPPORT_H_ */

