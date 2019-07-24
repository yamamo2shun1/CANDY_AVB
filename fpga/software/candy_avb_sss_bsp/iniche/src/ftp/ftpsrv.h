/*
 * FILENAME: FTPSRV.H
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

/* ftpsrv.h FTP server for WebPort HTTP server. 
 * 1/12/97 - Created. John Bartas 
 */
#ifndef FTPSVR_H
#define  FTPSVR_H 1


#ifdef VFS_FILES     /* from inet\ipport.h */
#include "vfsfiles.h"
#else /* map "vf" vcalls to native file system */
#ifndef vfopen /* allow override in ftpport.h */
#define  vfopen(file,   mode) fopen(file, mode)
#define  vfread(buf, ct,   size, fp)   fread(buf,  ct,   size, fp)
#define  vfwrite(buf,   ct,   size, fp)   fwrite(buf, ct,   size, fp)
#define  vgetc(fp)   fgetc(fp)
#define  vfclose(fp) fclose(fp)
#define  vunlink(fp) remove(fp)
#define  VFILE FILE
#endif   /* vfsopen */ 
#endif   /* VFS_FILES */

#define  FTP_PORT 21 /* standard FTP command port */

struct userstruct {
   char  username[FTPMAXUSERNAME];
   char  password[FTPMAXUSERPASS];
   char  home[FTPMAXPATH+2];  /* user's "home" directory */
   void *   group;   /* for use by port */
};
typedef struct userstruct userinfo;

#ifndef ip_addr
#define  ip_addr     u_long
#endif

struct ftpsvrs {
   struct ftpsvrs *  next; /* list link */
   int      inuse;      /* re-entry semaphore */
   SOCKTYPE sock;    /* client command socket */
   SOCKTYPE datasock;/* client   data  socket */
   int      state;      /* one of FTPS_ defines below */
   u_long   lasttime;   /* ftptick when last action occured */
   userinfo user;
   int      logtries;   /* retry count of logins */
   int      type;       /* ASCII or BINARY */
   VFILE *  filep;      /* pointer to open file during IO */
#ifdef IP_V6
   ip6_addr ip6_host;      /* host if domain == AF_INET6 */
#endif   /*  IP_V6 */
   ip_addr  host;       /* FTP client */
   u_short  dataport;   /* client data TCP port */
   char *   lastreply;  /* last reply string sent, for debugging */
   unsigned int   passive_state; /* state info for PASV command */
   u_long   passive_cmd;   /* file XFER command in passive state */
   u_short  server_dataport;  /* data port we listen on in passive mode */
#ifdef DRIVE_LETTERS
   char  drive[4];      /* usually "c:\0" */
#endif   /* DRIVE_LETTERS */
   char  cwd[FTPMAXPATH+1];   /* current directory, e.g. "/" or "/tmp/foo/" */
   char  filename[FTPMAXPATH+FTPMAXFILE];
   char  cmdbuf[CMDBUFSIZE];  /* buffer for comamnds from client */
   unsigned    cmdbuflen;     /* number of bytes currently receieved in cmdbuf */
   char  filebuf[FILEBUFSIZE];   /* file buffer for data socket & file IO */
   unsigned    filebuflen;    /* amount of data actually in filebuf */
#ifdef TCP_ZEROCOPY
   PACKET   pkt;           /* packet currently queued for send */
   queue ftpsq;            /* queue of received PACKETs */
   int   rxcode;           /* last receive code */
   int   iosize;           /* optimal IO size (TCP MSS) */
#endif   /* TCP_ZEROCOPY */
   int   wFlag;            /* flags for write blocked, et. al. */
   int   domain;           /* AF_INET or AF_INET6 */
};
typedef struct ftpsvrs ftpsvr;

extern   ftpsvr * ftplist; /* master list of FTP connections */

extern   u_long   ftps_connects;
extern   u_long   ftps_txfiles;
extern   u_long   ftps_rxfiles;
extern   u_long   ftps_txbytes;
extern   u_long   ftps_rxbytes;
extern   u_long   ftps_dirs;


/* ftpsvr.states: */
#define  FTPS_CONNECTING   1     /* connected, no USER info yet */
#define  FTPS_NEEDPASS     2     /* user OK, need password */
#define  FTPS_LOGGEDIN     3     /* ready to rock */
#define  FTPS_SENDING      4     /* sending a data file in progress */
#define  FTPS_RECEIVING    5     /* receiveing a data file in progress */
#define  FTPS_CLOSING      9     /* closing */

#define  FTPTYPE_ASCII     1
#define  FTPTYPE_IMAGE     2

/* ftpsvr.passive_state bits */
#define  FTPS_PASSIVE_MODE 0x01  /* session is in passive mode */
#define  FTPS_PASSIVE_CONNECTED  0x02  /* client has connected to data port */

/* FTP server internal commands */
char *   ftp_cmdpath(ftpsvr * ftp); /* extract path from cmd text */
char *   uslash(char * path);       /* make path into UNIX slashes */

/* required OS dependant routines */
int      fs_dodir(ftpsvr * ftp, u_long ftpcmd);
void     lslash(char * path);       /* make path into local slashes */
int      fs_dir(ftpsvr * ftp);      /* verify drive:/path exists */
int      fs_permit(ftpsvr * ftp);   /* verify user permission */
int      fs_lookupuser(ftpsvr * ftp, char * username);

/* macro to insert optional drive letter in sprintfs */
#ifdef DRIVE_LETTERS
#define  DRIVE_PTR(ftp) ftp->drive
#else /* drive not supported, insert pointer to null */
#define  DRIVE_PTR(ftp) ""
#endif

/* prototype server exported routines */
ftpsvr * ftps_connection(SOCKTYPE);   /* new connection */
void     ftp_leave_passive_state(ftpsvr * ftp);
void     ftps_loop(void);    /* periodic loop (tick) */
int      ftps_v4pasv(ftpsvr * ftp);
int      ftps_v6pasv(ftpsvr * ftp);
void     ftps_eprt(ftpsvr * ftp);
void     ftps_epsv(ftpsvr * ftp);

/* define the macro/routine FTP_TCPOPEN() based on version ifdefs */

#ifdef IP_V4   /* begin version mess */
#ifndef IP_V6  /* IP_V4 only */
SOCKTYPE ftp4open(ftpsvr * ftp);
#define FTP_TCPOPEN(ftp) ftp4open(ftp)
#else          /* IP_V4 and IP_V6 */
SOCKTYPE FTP_TCPOPEN(ftpsvr * ftp);   /* dual mode uses routine */
#endif  /* IP_V4 */
#else   /* IP_V6 only */
SOCKTYPE ftp6open(ftpsvr * ftp);
#define FTP_TCPOPEN(ftp) ftp6open(ftp)
#endif  /* IP_V6 only */

#endif   /* FTPSVR_H */

/* end of file ftpsrv.h */


