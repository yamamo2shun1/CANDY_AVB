/*
 * FILENAME: ftpsport.c
 *
 * Copyright  2000-2008 By InterNiche Technologies Inc. All rights reserved
 *
 *
 * MODULE: FTPSERVER
 *
 * ROUTINES: ftps_init(), ftps_check(), fs_lookupuser(), 
 * ROUTINES: ftps_cleanup(), 
 *
 * PORTABLE: no
 */

/* Additional Copyrights: */

/* ftpsport.c 
 * Portions Copyright 1996 by NetPort Software. All rights reserved. 
 * The Port-dependant portion of the FTP Server code. 11/24/96 - 
 * Created by John Bartas 
 */

#include "ftpport.h"    /* TCP/IP, sockets, system info */
#include "ftpsrv.h"

#ifndef SUPERLOOP
#ifndef OSPORT_H
#error Need to define OSPORT_H
#endif
#include OSPORT_H
#endif /* SUPERLOOP */

struct sockaddr_in   ftpssin;

#ifdef IP_V4
SOCKTYPE ftps_sock = SYS_SOCKETNULL;
#endif   /* IP_V4 */

#ifdef IP_V6
SOCKTYPE ftps_sock6 = SYS_SOCKETNULL;
#endif   /* IP_V6 */


#ifdef MINI_TCP
#include "msring.h"
#ifndef FTPS_NEWQLEN
#define FTPS_NEWQLEN    4      /* max # buffered new connections */
#endif /* ndef FTPS_NEWQLEN */
struct msring ftps_msring;
static M_SOCK ftps_msring_buf[FTPS_NEWQLEN + 1];
#endif /* MINI_TCP */


#if (defined(FTP_CLIENT) && defined(INCLUDE_NVPARMS))
#include "nvparms.h"
extern int  fc_replytmo    ;  /* secs of inactivity (for a cmd) for timeout*/
extern int  fc_activity_tmo;  /* ftp client's inactivity timeout (secs) */
#endif

#ifdef FTP_SERVER
extern int  ftps_init(void);
extern void ftps_cleanup(void);
extern void ftps_check(void);
#endif



/* FUNCTION: ftps_init()
 * 
 * ftps_init() - this is called by the ftp server demo package once 
 * at startup time. It initializes the vfs and opens a tcp socket to 
 * listen sfor web connections 
 *
 * If FTP_CLIENT and INCLUDE_NVPARMS are both enabled, then we need to
 * change a couple of FTP_CLIENT parameters to that in the NV file.
 *
 * PARAM1: 
 *
 * RETURNS: Returns 0 if OK, non-zero if error. 
 */

int
ftps_init()
{
   unshort  port;
   int      opens = 0;

   /* add default users for this port */
   /* anonymous makes you enter a password, we just don't care what it is */
   add_user("anonymous", "*", NULL);
   add_user("guest", "guest", NULL);

   port = FTP_PORT;

#ifdef IP_V4
   ftps_sock = t_tcplisten(&port, AF_INET);
   if (ftps_sock == SYS_SOCKETNULL)
      dprintf("FTP server: unable to start listen\n");
   else
      opens++;
#endif   /* IP_V4 */

#ifdef IP_V6
   ftps_sock6 = t_tcplisten(&port, AF_INET6);
   if (ftps_sock6 == SYS_SOCKETNULL)
      dprintf("FTP server: unable to start IPv6 listen\n");
   else
      opens++;
#endif   /* IP_V6 */

   if (opens == 0)
      return -1;

#ifdef MINI_TCP
   msring_init(&ftps_msring, ftps_msring_buf,
               sizeof(ftps_msring_buf) / sizeof(ftps_msring_buf[0]));
#endif /* MINI_TCP */

#if (defined(FTP_CLIENT) && defined(INCLUDE_NVPARMS))
   /* So the following code is included only when an NVFS exists. */

   if (ftpc_nvparms.fc_replytmo > 0)
      fc_replytmo = ftpc_nvparms.fc_replytmo ;

   if (ftpc_nvparms.fc_activity_tmo > 0)
      fc_activity_tmo = ftpc_nvparms.fc_activity_tmo ;

#endif /* FTP_CLIENT & INCLUDE_NVPARMS */

   return 0;
}

#ifndef MINI_TCP

/* Accept a new FTP command connection
 *
 * Returns 0 if OK, or ENP error code 
 */

int
ftp_accept(int domain, SOCKTYPE ftps_sock, int clientlen)
{
   SOCKTYPE socktmp;
   struct sockaddr client;
   int   err;
   ftpsvr * ftps;

   socktmp = t_accept(ftps_sock, &client, &clientlen);
   if (socktmp != SYS_SOCKETNULL)
   {
      ftps = ftps_connection(socktmp);
      if (ftps == NULL)
         return ENP_NOMEM;    /* most likely problem */
      ftps->domain = domain;
   }
   else
   {
      err = t_errno(ftps_sock);
      if (err != EWOULDBLOCK)
      {
         return err;
      }
   }
   return 0;
}
#endif   /* ndef MINI_TCP */


/* FUNCTION: ftps_check()
 * 
 * ftp server task loop. For the PC DOS demo, this is called once 
 * every main task loop. 
 *
 * PARAM1: 
 *
 * RETURNS: 
 */

static int in_ftps_check = 0;    /* reentry guard */

void
ftps_check()
{
#ifdef MINI_TCP
   SOCKTYPE  socktmp;
#endif

   in_ftps_check++;
   if (in_ftps_check != 1)
   {
      in_ftps_check--;
      return;
   }

#ifdef MINI_TCP

   while (msring_del(&ftps_msring, &socktmp) == 0)
   {
      ftpsvr *e;
      
      e = ftps_connection(socktmp);
      if (e == (ftpsvr *)NULL)
      {
         dprintf("ftp connection error\n");
         continue;
      }
      m_ioctl(socktmp, SO_NONBLOCK, NULL);  /* make socket non-blocking */
   }

#ifndef SUPERLOOP
   {
      struct sockaddr_in   client;
      int      block = TRUE;    /* flag to block FTP server task */
      ftpsvr * ftp;

      /* loop through ftp structs to see if it's OK to block task */
      for (ftp = ftplist; ftp; ftp = ftp->next)
      {
         /* The ftp data sending and receiving states can hang if we  
          * block the thread. If any session is in this state then 
          * clear the block flag to avoid this.
          */
         if ((ftp->state == FTPS_SENDING) ||
            (ftp->state == FTPS_RECEIVING))
         {
            block = FALSE;    /* don't block server task */
            break;            /* no need to check any more sessions */
         }
      }
      if (block)         /* flag should now be set */
         TK_BLOCK();    /* sleep until a callback wakes us */
   }
#endif   /* SUPERLOOP */
   /* nothing to do in the MINI_TCP & SUPERLOOP case since connects
    * are callback driven and the superloop will poll us anyway.
    */
#elif defined(BLOCKING_APPS)  /* use blocking select to drive server app */
   {
      fd_set ftp_fdrecv;
      fd_set ftp_fdsend;
      ftpsvr *ftp;
      int events;             /* return from select() */
#ifndef USE_FDS
      int recvs = 0;          /* number of entries in recv FD_SET array */
      int sends = 0;
#endif
#ifdef FTP_IDLECONN_TMO
      extern int    ftps_iotmo;     /* idle time in BLOCKING_APPS mode */
#endif /* FTP_IDLECONN_TMO */

#ifdef USE_FDS
      FD_ZERO(&ftp_fdrecv);
      FD_ZERO(&ftp_fdsend);
#else
      MEMSET(&ftp_fdrecv, 0, sizeof(fd_set));
      MEMSET(&ftp_fdsend, 0, sizeof(fd_set));
#endif  /* USE_FDS */

      /* use the recv array to detect new connections */

#ifdef IP_V4
      if (ftps_sock != SYS_SOCKETNULL)
      {
         FD_SET(ftps_sock, &ftp_fdrecv);
#ifndef USE_FDS
         recvs++;
#endif
      }
#endif
#ifdef IP_V6
      if (ftps_sock6 != SYS_SOCKETNULL)
      {
         FD_SET(ftps_sock6, &ftp_fdrecv);
#ifndef USE_FDS
         recvs++;
#endif
      }
#endif

      /* If no listening sockets, don't bother */
#ifdef USE_FDS
      if (FD_COUNT(&ftp_fdrecv) == 0)
         return;
#else
      if (recvs == 0)
         return;
#endif  /* USE_FDS */

      /* loop through ftp structs building read/write arrays for select */
      for (ftp = ftplist; ftp; ftp = ftp->next)
      {
      /* add ftp server's open sockets to the FD lists based on
       * their state. The server thread will block until one of
       * these has activity.
       */
#ifdef USE_FDS
         /* error checking is done in FD_SET */
#else
         if ((recvs > FD_SETSIZE) || (sends > FD_SETSIZE))
         {
            dtrap();    /* port needs bigger FD_SETSIZE */
            break;      /* no point in looping any more */
         }
#endif  /* USE_FDS */

         FD_SET(ftp->sock, &ftp_fdrecv);
#ifndef USE_FDS
         recvs++;
#endif
         if (ftp->datasock && (ftp->datasock != SYS_SOCKETNULL))
         {
            /* always add the data socket to the receive FD_SET */
            FD_SET(ftp->datasock, &ftp_fdrecv);
#ifndef USE_FDS
            recvs++;
#endif
            /* only add the send socket if we are actively sending */
            if (ftp->state == FTPS_SENDING)
            {
               FD_SET(ftp->datasock, &ftp_fdsend);
#ifndef USE_FDS
               sends++;
#endif
            }
         }
      }  /* end of for(ftplist) loop */

      /* block until one of the sockets has activity */
#ifndef FTP_IDLECONN_TMO
      events = t_select(&ftp_fdrecv, &ftp_fdsend, (fd_set *)NULL, -1);
#else /* do not block idle time, enable inactivity timeout */
      events = t_select(&ftp_fdrecv, &ftp_fdsend, (fd_set *)NULL,
                        (unsigned long)ftps_iotmo * FTPTPS); 
#endif /* FTP_IDLECONN_TMO */

#ifdef IP_V4
      if (FD_ISSET(ftps_sock, &ftp_fdrecv))  /* got a connect to server listen? */
      {
         ftp_accept(AF_INET, ftps_sock, sizeof(struct sockaddr_in));
         events--;
      }
#endif   /* IP_V4 */

#ifdef IP_V6
      if (FD_ISSET(ftps_sock6, &ftp_fdrecv))  /* got a connect v6 listen? */
      {
         ftp_accept(AF_INET6, ftps_sock6, sizeof(struct sockaddr_in6));
         events--;
      }
#endif   /* IP_V6 */

      if (events <= 0)  /* connect was only socket */
      {
         in_ftps_check--;
         return;
      }
   }
#else /* not BLOCKING_APPS, poll non-blocking sockets */

#ifdef IP_V4
   if (ftps_sock != SYS_SOCKETNULL)
      ftp_accept(AF_INET, ftps_sock, sizeof(struct sockaddr_in));
#endif   /* IP_V4 */

#ifdef IP_V6
   if (ftps_sock6 != SYS_SOCKETNULL)
      ftp_accept(AF_INET6, ftps_sock6, sizeof(struct sockaddr_in6));
#endif   /* IP_V6 */

#endif   /* BLOCKING_APPS */

   /* work on existing conections - data or command */
   ftps_loop();

   in_ftps_check--;
   return;
}




/* FUNCTION: fs_lookupuser()
 * 
 * fs_lookupuser() lookup a user based on the name. Fill in user 
 * struct in ftp, including password. If no password required, fill 
 * in null string. Filled in data is in unencrypted form. This 
 * particular port is for the user database in ..\misclib\userpass.c 
 * 
 * PARAM1: ftpsvr * ftp
 * PARAM2: char * username
 *
 * RETURNS: Returns 0 if user found, else -1 if user invalid. 
 */

int
fs_lookupuser(ftpsvr * ftp, char * username)
{
   int   i;

   for (i = 0; i < NUM_NETUSERS; i++)
   {
      if (users[i].username[0] == 0)
         continue;
      if (strcmp(users[i].username, username) == 0)
         break;
   }

   if (i >= NUM_NETUSERS)  /* username not found? */
      return -1;

   /* extract username from command  */
   strncpy(ftp->user.username, username, FTPMAXUSERNAME);
   strncpy(ftp->user.password, users[i].password, FTPMAXUSERPASS);
   strcpy(ftp->user.home, "c:\\");  /* default for DOS port */
   return 0;
}

void  delftp(ftpsvr * ftp);


/* FUNCTION: ftps_cleanup()
 * 
 * Close down all the connections to FTP Server. Then close the
 * socket used for FTP Server listen 
 *
 * PARAM1: void
 *
 * RETURNS: 
 */

void 
ftps_cleanup(void)
{
   ftpsvr * ftp;
   ftpsvr * ftpnext;

   ftpnext = ftplist;   /* will be set to ftp at top of loop */

   /* loop throught connection list */
   while (ftpnext)
   {
      ftp = ftpnext;
      ftpnext = ftp->next;   
      delftp(ftp);   /* kill the connection */
   }

#ifdef IP_V4
   if ( ftps_sock != INVALID_SOCKET )
   {
      sys_closesocket(ftps_sock);
      ftps_sock = INVALID_SOCKET ;     
   }
#endif

#ifdef IP_V6
   if ( ftps_sock6 != INVALID_SOCKET )
   {
      sys_closesocket(ftps_sock6);
      ftps_sock6 = INVALID_SOCKET ;     
   }
#endif
}


#ifndef SUPERLOOP

#ifdef FTP_SERVER

TK_OBJECT(to_ftpsrv);
TK_ENTRY(tk_ftpsrv);
long ftpsrv_wakes   =  0;

/*
 * Altera Niche Stack Nios port modification:
 * Use task priority and stack size values from ipport.h
 */
struct inet_taskinfo ftpstask = {
      &to_ftpsrv,
      "FTP server",
      tk_ftpsrv,
      TK_FTPSRVR_TPRIO,
      TK_FTPSRVR_SSIZE,
};


/* The application thread works on a "controlled polling" basis:
 * it wakes up periodically and polls for work. If there is outstanding
 * work, the next wake is accellerated to give better performance under
 * heavy loads.
 *
 * The FTP task could aternativly be set up to use blocking sockets,
 * in which case the loops below would only call the "xxx_check()"
 * routines - suspending would be handled by the TCP code.
 */

/* FUNCTION: tk_ftpsrv()
 * 
 * PARAM1: n/a
 *
 * RETURNS: n/a
 */

TK_ENTRY(tk_ftpsrv)
{
   int e;

   while (!iniche_net_ready)
      TK_SLEEP(1);

   e = ftps_init();
   if (e)
      TK_RETURN_ERROR();
   exit_hook(ftps_cleanup);

   for (;;)
   {
      ftps_check();     /* may block on select */
#ifndef BLOCKING_APPS
      tk_yield();       /* give up CPU if it didn't block */
#else
#ifdef MINI_TCP
      tk_yield();       /* give up CPU, MINI_TCP doesn't select() */
#endif   /* MINI_TCP */
#endif   /* BLOCKING_APPS */
      ftpsrv_wakes++;   /* count wakeups */
      if (net_system_exit)
         break;
   }
   TK_RETURN_OK();
}

#endif   /* FTP_SERVER */

#endif /* SUPERLOOP */

