/*
 * FILENAME: ftpsrv.c
 *
 * Copyright 1997- 2000 By InterNiche Technologies Inc. All rights reserved
 *
 *
 * MODULE: FTPSERVER
 *
 * ROUTINES: ftps_connection(), ftpputs(), ftp_getcmd(), 
 * ROUTINES: ftp_flushcmd(), ftps_loop(), newftp(), delftp(), ftps_user(), 
 * ROUTINES: ftps_password(), ftp_cmdpath(), ftp_make_filename()
 * ROUTINES: ftp_leave_passive_state(), ftps_do_pasv(), ftps_cmd(),
 *
 * PORTABLE: yes
 */


#include "ftpport.h"    /* TCP/IP, sockets, system info */
#include "ftpsrv.h"     /* FTP server includes */

static u_long   max_ftps_conn = MAX_FTPS_SESS;

u_long   ftps_connects  =  0; /* TCP connections tocmd port */
u_long   ftps_sessions  =  0; /* user & password OK */
u_long   ftps_badauth   =  0; /* user or password badK */
u_long   ftps_txfiles   =  0; /* total data files sent */
u_long   ftps_rxfiles   =  0; /* total data files received */
u_long   ftps_txbytes   =  0; /* total data bytes received */
u_long   ftps_rxbytes   =  0; /* total data bytes sent */
u_long   ftps_dirs      =  0; /* total directory operations done */

/* ftp server internal routines: */
static int  ftp_getcmd(ftpsvr * ftp);
static void ftp_flushcmd(ftpsvr * ftp);

ftpsvr * newftp(void);
void  delftp(ftpsvr * ftp);
int   ftps_user(ftpsvr * ftp);
int   ftps_password(ftpsvr * ftp);
int   ftps_cmd(ftpsvr * ftp);
int   ftp_sendfile(ftpsvr * ftp);
int   ftp_getfile(ftpsvr * ftp);
void  ftp_xfercleanup(ftpsvr * ftp);

/* common FTP server reply tokens */
char * ftp_cmdok   = "200 Command OK\r\n";
char * ftp_ready   = "220 Service ready\r\n";
char * ftp_needpass= "331 User name ok, need password\r\n";
char * ftp_loggedin= "230 User logged in\r\n";
char * ftp_fileok  = "150 File status okay; about to open data connection\r\n";
char * ftp_closing = "226 Closing data connection, file transfer successful\r\n";
char * ftp_badcmd  = "500 Unsupported command\r\n";
char * ftp_noaccess= "550 Access denied\r\n";

ftpsvr * ftplist   = NULL;   /* master list of FTP connections */

int    notfatal    = 0;      /* unfatal error handling */

/* ftp server timeouts, left as globals for app overrides: */
int    ftps_iotmo  =  120;  /* Idle timeout during IO activity */
int    ftps_lotmo  =  60;   /* Idle timeout during logins */

#ifdef FTP_IDLECONN_TMO
int    ftps_conntmo  = 0;   /* connection idle timeout, 0==no timeout */
#endif


/* if x is an upper case letter, this evaluates to x,
 *  if x is a lower case letter, this evaluates to the upper case.
 */
#define  upper_case(x)  ((x)  &  ~0x20)

u_short  listcmds =  0; /* number of LIST or DIR commands */

#ifndef SUPERLOOP
extern   TK_OBJECT(to_ftpsrv);
#endif


/* FUNCTION: ftps_connection()
 * 
 * ftps_connection() - Called whenever we have accepted a connection 
 * on the FTP server listener socket. The socket passed will stay 
 * open until we close it. 
 *
 * PARAM1: WP_SOCKTYPE sock
 *
 * RETURNS: Returns ftpsvr pointer if OK, else NULL
 */

ftpsvr *
ftps_connection(SOCKTYPE sock)
{
   ftpsvr * ftp;
   int   e;

   ftps_connects++;           /* count connections */
   
   /* check if we have exceeded the maximum number of connections */
   if ((max_ftps_conn > 0) && (ftps_connects > max_ftps_conn))
   {
      ftps_connects--;
      t_socketclose(sock);
      return NULL;
   }

   /* create new FTP connection */
   if ((ftp = newftp()) == (ftpsvr *)NULL)
   {
      ftps_connects--;
      t_socketclose(sock);
      return NULL;
   }

   /* set the default data port we will connect to for data transfers 
    * to be the same as the port that the client connected with just 
    * in case we connect to a client that doesn't send PORT commands. 
    * see section 3.2 ESTABLISHING DATA CONNECTIONS in RFC 959 for a 
    * description of this, keeping in mind that what we are doing 
    * here is setting the default "user-process data port". 
    * 
    * note that t_getpeername() can in theory fail, but its not clear 
    * what we could do at this point to recover if it did and it only 
    * makes a difference if we connect to clients that don't send 
    * PORT commands anyway, so just use whatever port that we get back 
    */
   ftp->dataport = SO_GET_FPORT(sock);

   ftp->sock = sock;    /* remember client socket */
   ftp->state = FTPS_CONNECTING;
   e = t_send(ftp->sock, ftp_ready, strlen(ftp_ready), 0);
   if (e == -1)   /* did connection die already? */
   {
      dtrap();
      delftp(ftp);
      return NULL;
   }

   return ftp;
}



/* FUNCTION: ftpputs()
 * 
 * ftpputs() - put a string to an ftp command socket. 
 *
 * PARAM1: ftpsvr * ftp
 * PARAM2: char * text
 *
 * RETURNS: Retuns 0 if OK -1 if error. ftpsvr is deleted on error. 
 */

int
ftpputs(ftpsvr * ftp, char * text)
{
   int   bytes_to_send;
   int   bytes_sent;
   int   rc;

   bytes_to_send = strlen(text);
   for (bytes_sent = 0; bytes_to_send > 0; )
   {
      rc = t_send(ftp->sock, text + bytes_sent, bytes_to_send, 0);
      if (rc < 0)
      {
         rc = t_errno(ftp->sock);
         dtrap();       /* show errors to programmer */
         ftp->state = FTPS_CLOSING;
         return -1;
      }
      bytes_to_send -= rc;
      bytes_sent += rc;
      if (bytes_to_send > 0)
         tk_yield();
   }
   /* bytes_to_send should end up 0 */
   if (bytes_to_send < 0)
   {
      dtrap();
   }
   return 0;
}

#define  FTP_HASCMD     1
#define  FTP_NOCMD      2
#define  FTP_ERROR      3



/* FUNCTION: ftp_getcmd()
 * 
 * ftp_getcmd() - Get a command from the ftp command stream. Trys to 
 * read more data from a ftp client sock until a command is buffered. 
 *
 * PARAM1: ftpsvr * ftp
 *
 * RETURNS: FTP_HASCMD if a command is ready at ftp->cmdbuf, else 
 * FTP_NOCMD if a command is not ready, or FTP_ERROR if there's a 
 * serious data problem. If FTP_HASCMD is returned and the caller 
 * processes the command, he should call ftp_flushcmd() so the 
 * command doen't get processed again. 
 */

static int
ftp_getcmd(ftpsvr * ftp)
{
   int   readval;
   int   e;
   char *   cp;

   /* if we filled up the input buffer on the last pass and there was
      no CRLF terminating a command in any of it */
   if (ftp->cmdbuflen >= (CMDBUFSIZE - 1))
   {
      /* the input is bogus so discard it */
      ftp->cmdbuflen = 0;
   }

   /* read as much data as will fit into the command buffer, leaving 
    * room for the NULL that we are going to insert following the 
    * first CRLF, below
    */

   readval = t_recv(ftp->sock, (ftp->cmdbuf + ftp->cmdbuflen), 
    (CMDBUFSIZE - ftp->cmdbuflen - 1), 0);

   if (readval == 0)
      ftp->state = FTPS_CLOSING;

   if (readval < 0)  /* error on socket? */
   {
      e = t_errno(ftp->sock);
      if (e != EWOULDBLOCK)
      {
         /* let programmer see errors */
         dprintf("ftpsvr cmd socket error %d\n", e);
         delftp(ftp);   /* thats the end of this connection... */
         return FTP_ERROR; /* error return */
      }
      else     /* no command ready */
         return FTP_NOCMD;
   }
   ftp->cmdbuflen += readval;    /* add read data to hp */

   if (ftp->cmdbuflen == 0)   /* nothing in buffer? */
      return FTP_NOCMD;

   ftp->lasttime = ftpticks;     /* this is activity; rest timeout */

   cp = strstr(ftp->cmdbuf, "\r\n");   /* look for trailing CRLF */
   if (cp)  /* look for trailing CRLF */
   {
      char *   src;
      char *   dst;

      /* point to first byte following the CRLF */
      cp += 2;
      /* if there's not already a null there */
      if (*cp)
      {
         /* move all the characters following the CRLF up one so we got
            room to insert a null to terminate the command after the CRLF */
         /* note we do this here because some of the later code paths
            treat the command like an ASCIIZ string */
         dst = ftp->cmdbuf + ftp->cmdbuflen;
         src = dst - 1;
         while (src >= cp)
            *dst-- = *src--;
         /* increment the number of characters in the command buffer to
            account for the NULL */
         ftp->cmdbuflen++;
         /* NULL terminate the command */
         *cp = 0;
      }

      /* now, flip the characters at the beginning of the buffer that 
       * could be an FTP command from lower to upper case since 
       * the protocol's supposed to be case insensitive *
       *
       * we look at at most the first 4 bytes since no commands
       * are more than 4 bytes long 
       */

      for (dst = ftp->cmdbuf; dst < (ftp->cmdbuf + 4); ++dst)
      {
         /* upper case, leave as is */
         if ((*dst >= 'A') && (*dst <= 'Z'))
            continue;
         /* lower case gets flipped to upper */
         if ((*dst >= 'a') && (*dst <= 'z'))
         {
            *dst = (char) (*dst + (char) ('A' - 'a'));
            continue;
         }
         /* anthing else means we got to end of command so break */
         break;
      }
      return FTP_HASCMD;   /* Got command */
   }
   else
      return FTP_NOCMD;    /* NO command */
}



/* FUNCTION: ftp_flushcmd()
 * 
 * PARAM1: ftpsvr * ftp
 *
 * RETURNS: 
 */

static void
ftp_flushcmd(ftpsvr * ftp)
{
   char *   cp;
   int   old_cmd_len;
   int   rest_of_cmds_len;

   /* find command terminating CRLF */
ff_again:
   cp = strstr(ftp->cmdbuf, "\r\n");
   if (!cp)
   {
      /* might be clobbered CR at end of path (see CWD code) */
      if ( (unsigned)strlen(ftp->cmdbuf) < ftp->cmdbuflen)
      {
         ftp->cmdbuf[strlen(ftp->cmdbuf)] = '\r';     /* put back CR */
         goto ff_again;
      }
      dtrap();    /* prog error */
      return;
   }

   /* cp now points to a CRLF followed by the NULL we inserted in 
    * ftp_getcmd(), so if theres data in the buffer following the 
    * NULL, then its the beginning of the next command 
    */
   /* point to where next comamnd will be if its there */
   cp += 3;
   /* compute the length of the old command */
   old_cmd_len = cp - ftp->cmdbuf - 1;

   /* compute the length of the rest of the commands in the buffer */
   /* that's the number of bytes that had been read into the buffer, 
    * less the length of the old command less 1 for the NULL that 
    * we inserted to null terminate the old command 
    */
   rest_of_cmds_len = ftp->cmdbuflen - old_cmd_len - 1;

   /* this will happen if we didn't insert a null after the command 
    * because there was one there already, which will be the case 
    * when the old command is the only data that's been read so far, 
    * in which case the length of the rest of the commands is 0 
    */

   if (rest_of_cmds_len < 0)
      rest_of_cmds_len = 0;

   /* if there are any other commands left in the buffer */
   if (rest_of_cmds_len)
   {
      /* move them to the front of the buffer */
      MEMCPY(ftp->cmdbuf,cp,rest_of_cmds_len);
   }

   /* zero the data following rest of the commands for the length of 
    * the old command or else you risk finding command termination 
    * sequences when none have been received 
    */
   /* if there are no other commands, this zeroes the old command
      from the buffer */
   MEMSET(ftp->cmdbuf + rest_of_cmds_len,0,old_cmd_len + 1);

   ftp->cmdbuflen = rest_of_cmds_len;
}




/* FUNCTION: ftps_loop()
 * 
 * PARAM1: 
 *
 * RETURNS: 
 */

void
ftps_loop()
{
   ftpsvr * ftp;
   ftpsvr * ftpnext;
   int   cmdready;
   int   e;    /* error holder */

#ifndef MINI_TCP
   struct sockaddr   client;  /* for BSDish accept() call */
   int               clientsize;
   SOCKTYPE data_sock;        /* socket for passive accept */
#endif

   ftpnext = ftplist;   /* will be set to ftp at top of loop */

   /* loop throught connection list */
   while (ftpnext)
   {
      ftp = ftpnext;
      ftpnext = ftp->next;    /* remember next in case ftp is deleted */
      if (ftp->inuse)   /* if we are blocked in guts, quit */
         continue;
      ftp->inuse++;  /* set reentry flag */
      e = 0;         /* clear error holder */
      /* see if a command is ready */
      cmdready = ftp_getcmd(ftp);
      if (cmdready == FTP_ERROR)
         continue;

      switch (ftp->state)
      {
      case FTPS_CONNECTING:
      case FTPS_NEEDPASS:
         /* check for shorter session timeout in these states */
         if (ftp->lasttime + ((unsigned long)ftps_lotmo * FTPTPS) < ftpticks)
         {
            e = -1;  /* set flag to force deletion of ftps */
            break;
         }
         if (cmdready != FTP_HASCMD)
            break;
         if (ftp->state == FTPS_CONNECTING)
            e = ftps_user(ftp);
         else
            e = ftps_password(ftp);
         break;
      case FTPS_LOGGEDIN:
         /* connection timeouts like this are really obnoxious and should 
          * be disabled usless there is some compling reason your target 
          * should do otherwise (like dialup charges).
          */
#ifdef FTP_IDLECONN_TMO
         /* see if session has been idle for too long */
         if((ftps_conntmo > 0) &&
             (ftp->lasttime + ((unsigned long)ftps_conntmo * FTPTPS) < ftpticks))
         {
            dprintf("ftpsrv: timout of idle FTP session\n");
            delftp(ftp);
            break;
         }
#endif   /* FTP_IDLECONN_TMO */

         if (cmdready == FTP_HASCMD)
            e = ftps_cmd(ftp);
         /* if we are in passive mode and the client hasn't connected yet */
         if (ftp->passive_state == FTPS_PASSIVE_MODE)
         {
            /* check to see if the client connected */
#ifndef MINI_TCP

            /* Accept for either V4 or V6 
             */
	 data_sock = SYS_SOCKETNULL;
#ifdef IP_V6
         if (LONG2SO(ftp->datasock)->so_domain == AF_INET6)
         {
            struct sockaddr_in6 cli6;
            char ipv6WrkBuffer[60];
   
            clientsize = sizeof(struct sockaddr_in6);
            data_sock = t_accept(ftp->datasock, &cli6, &clientsize); 
         }            
#endif /* IP_V6 */
            
#ifdef IP_V4
         if (LONG2SO(ftp->datasock)->so_domain == AF_INET)
         {
            clientsize = sizeof(struct sockaddr_in);
            data_sock = t_accept(ftp->datasock, &client, &clientsize);
         }
#endif /* IP_V4 */

            /* if client didn't connect, break to continue */
            if (data_sock == SYS_SOCKETNULL)
               break;
            /* client connected, so close listening socket so we wont take
             * any more connections to it.
             */
            t_socketclose(ftp->datasock);

            /* the data socket we care about is now the actual connection
             * to the client.
             */
            ftp->datasock = data_sock;

#else    /* mini sockets */
            /* mini-sockets sets data_sock in a callback. If client
             * didn't connect yet, break to continue.
             */
            if(ftp->datasock->state & SS_ISCONNECTED == 0)
               break;
#endif
            /* change our passive state so that we know we are connected
               to the client on the data socket */
            ftp->passive_state |= FTPS_PASSIVE_CONNECTED;
            /* if we have already received our data transfer command */
            if (ftp->passive_cmd)
            {
               /* then do the command */
               IN_PROFILER(PF_FTP, PF_ENTRY);
               switch (ftp->passive_cmd)
               {
               case 0x4c495354:     /* "LIST" */
               case 0x4e4c5354:     /* "NLST" */
                  if (fs_dodir(ftp, ftp->passive_cmd))
                     ftpputs(ftp, "451 exec error\r\n");
                  break;
               case 0x52455452:     /* "RETR" */
                  ftp_sendfile(ftp);
                  break;
               case 0x53544f52:     /* "STOR" */
                  ftp_getfile(ftp);
                  break;
                  /* there is a serious logic error someplace */
               default :
                  dprintf("invalid passive_cmd\n");
                  dtrap();
               }
               IN_PROFILER(PF_FTP, PF_EXIT);
            }
         }
         break;
      case FTPS_RECEIVING:    /* task suspended while doing IO */
      case FTPS_SENDING:
         /* check for shorter session timeout in these states */
         if (ftp->lasttime + ((unsigned long)ftps_iotmo * FTPTPS) < ftpticks)
         {
            e = -1;  /* set flag to force deletion of ftps */
            break;
         }
         IN_PROFILER(PF_FTP, PF_ENTRY);
         if (ftp->state == FTPS_SENDING)
            e = ftp_sendfile(ftp);
         else     /* must be receiving */
            e = ftp_getfile(ftp);
         IN_PROFILER(PF_FTP, PF_EXIT);

#ifndef SUPERLOOP
         /* If not superloop and there is still more data to move,
          * then make sure ftp server will wake up to finish the
          * send/receive later. The the transfer finished, the ftps
          * state will have returned to LOGGEDIN.
          */
         if(ftp->state != FTPS_LOGGEDIN)
            TK_WAKE(&to_ftpsrv);     /* make sure we come back later */
#endif
         break;
      case FTPS_CLOSING:
         break;
      default:    /* bad state? */
         dtrap();
         break;
      }
      /* if fatal error or connection closed */
      /* if (e == 452) 
      {      
         ftp->state = FTPS_LOGGEDIN;
         ftp_flushcmd(ftp); 
         ftp->inuse--;
      } */
      if (e || (ftp->state == FTPS_CLOSING))
         delftp(ftp);
      else
         ftp->inuse--;     /* set reentry flag */
   }
}



/* FUNCTION: newftp()
 * 
 * newftp() - create a new ftpsvr structure. Put in master queue. 
 *
 * PARAM1: 
 *
 * RETURNS: 
 */

ftpsvr *
newftp()
{
   ftpsvr * ftp;

   ftp = (ftpsvr *)FTPSALLOC(sizeof(ftpsvr));
   if (!ftp)
      return NULL;
   ftp->next = ftplist;
   ftp->lasttime = ftpticks;
   ftplist = ftp;

   /* make sure we have a valid domain */
#if defined(IP_V4) || defined(MINI_IP)
   ftp->domain = AF_INET;  /* default to IPv4 */
#else
   ftp->domain = AF_INET6;
#endif
   return(ftp);
}



/* FUNCTION: delftp()
 * 
 * PARAM1: ftpsvr * ftp
 *
 * RETURNS: 
 */

void
delftp(ftpsvr *ftp)
{
   ftpsvr *list;
   ftpsvr *last;

   last = NULL;
   for (list = ftplist; list; list = list->next)
   {
      if (list == ftp)
      {
         /* found server to kill, unlink: */
         if (last)
            last->next = list->next;
         else
            ftplist = list->next;

         if (ftp->sock)
         {
            t_socketclose(ftp->sock);
            ftp->sock = 0;
         }
         if (ftp->datasock)
         {
            t_socketclose(ftp->datasock);
            ftp->datasock = 0;
         }

#ifdef   TCP_ZEROCOPY
         /* don't orphan any packets we may have stored in ftpsq */
         while (ftp->ftpsq.q_len > 0)
            tcp_pktfree ((PACKET)getq(&ftp->ftpsq) );
#endif   /* TCP_ZEROCOPY or not */

         FTPSFREE(ftp);
         ftps_connects--;     /* decrement connection count */
         break;
      }
      last = list;
   }
}



/* the ftps_ server command handler routines. These are called when a 
 * command is received in the ftp->cmdbuf. Which one is called 
 * depends in the session state. These all process a command, maybe 
 * change the state, (or kill the session) and flush the command. All 
 * return 0 if OK (which may mean pending work) or negative error 
 * code if a fatal error was detected. 
 */



/* FUNCTION: ftps_user()
 * 
 * ftps_user() - called when we get a command in the initial state 
 *
 * PARAM1: ftpsvr * ftp
 *
 * RETURNS: 
 */

int
ftps_user(ftpsvr * ftp)
{
   char *   cp;
   int   e;

   /* make sure client's telling me about a user */
   if (MEMCMP(ftp->cmdbuf, "USER", 4) != 0)
   {
      ftpputs(ftp, ftp_badcmd);
      return -1;  /* signal main loop to kill session */
   }
   cp = strstr(ftp->cmdbuf, "\r\n");
   if (!cp)
   {
      dtrap();
      return -1;  /* signal main loop to kill session */
   }
   *cp = 0;    /* NULL terminate user name */

   /* search user list */
   e = fs_lookupuser(ftp, &ftp->cmdbuf[5]);
   *cp = '\r';    /* put back buffer char we clobbered */
   if (e)
   {
      ftp->state = FTPS_CONNECTING;
      ftpputs(ftp, "530 Invalid user\r\n");
      ftp_flushcmd(ftp);
      return 0;   /* user not valid */
   }

   ftp->cwd[0] = FTP_SLASH;
   ftp->cwd[1] = 0;
#ifdef DRIVE_LETTERS
   strcpy(ftp->drive, DEFAULT_DRIVE);
#endif   /* DRIVE_LETTERS */
   ftp->type = FTPTYPE_ASCII; /* RFC says make text the defaul type */


   if (ftp->user.password[0] == '\0')  /* no password required? */
   {
      ftpputs(ftp, ftp_loggedin);
      ftp->state = FTPS_LOGGEDIN;
      ftps_sessions++;
   }
   else  /* require a password */
   {
      ftpputs(ftp, ftp_needpass);   /* message to client */
      ftp->state = FTPS_NEEDPASS;   /* set proper state */
   }
   ftp_flushcmd(ftp);
   return 0;
}



/* FUNCTION: ftps_password()
 * 
 * ftps_password() - called when we get a command in the 
 * FTPS_NEEDPASS state. 
 *
 * PARAM1: ftpsvr * ftp
 *
 * RETURNS: Returns 0 if OK (which may mean pending work) 
 * or negative error code if a fatal error was detected. 
 */

int
ftps_password(ftpsvr * ftp)
{
   char *   password;
   char *   cp;

   if (MEMCMP(ftp->cmdbuf, "PASS", 4) != 0)
   {
      ftpputs(ftp, ftp_badcmd);
      return -1;  /* signal main loop to kill session */
   }
   password = &ftp->cmdbuf[5];
   cp = strstr(password, "\r\n");   /* find end of command */
   if (!cp) /* require whole command to be in buffer */
      return -1;  /* signal main loop to kill session */

   *cp = 0;    /* NULL terminate */
   /* password '*' means we accept any password, so don't even compare */
   if (ftp->user.password[0] != '*')
   {
      if (strcmp(password, ftp->user.password) != 0)
      {
         ftp->state = FTPS_CONNECTING;
         ftpputs(ftp, "530 Invalid password\r\n");

         if (ftp->logtries++ > 2)
            return -1;  /* too many failed logins, kill session */
         else
         {
            ftp_flushcmd(ftp);
            return 0;   /* wait for another user/pass try */
         }
      }
   }
   ftpputs(ftp, ftp_loggedin);   /* login OK, set up session */
   ftp->state = FTPS_LOGGEDIN;
   ftps_sessions++;
   ftp_flushcmd(ftp);
   return 0;
}



/* FUNCTION: ftp_cmdpath()
 * 
 * ftp_getpath() - extract path from an FTP command in C string form. 
 * The returned pointer is to the ftp->cmdbuf area. 
 *
 * PARAM1: ftpsvr * ftp
 *
 * RETURNS: Returns NULL on any error after sending complain 
 * string to client. 
 */

char *   
ftp_cmdpath(ftpsvr * ftp)
{
   char *   cp;

   cp = strchr(&ftp->cmdbuf[4], '\r');
   if (!cp)
   {
      ftpputs(ftp, "501 garbled path\r\n");
      return NULL;
   }
   *cp = 0;    /* null terminate path in buffer */
   cp = &ftp->cmdbuf[4];
   while (*cp == ' ') cp++;   /* bump past spaces */
      if (strlen(cp) > FTPMAXPATH)
   {
      ftpputs(ftp, "553 path too long\r\n");
      return NULL;
   }
   return cp;
}



/* FUNCTION: ftp_make_filename()
 * 
 * create a complete file name and path from the file specification 
 * in the command buffer and the cwd associated with the current ftp 
 * session. allow_empty_filespec specifies whether a file 
 * specification is required (as is the case with RETR and STOR) or 
 * whether it is optional (as is the case with NLST and LIST). 
 *
 * PARAM1: ftpsvr *ftp
 * PARAM2: int allow_empty_filespec
 *
 * RETURNS: TRUE if success, FALSE if some error occurred 
 */

int ftp_make_filename(ftpsvr *ftp,int allow_empty_filespec)
{
   char *cp;
   char *cp1;
   int relative_path;

   cp = ftp_cmdpath(ftp);
   if(!cp)   return FALSE;   /* ftp_cmdpath() already sent error */

   /* if there is no file spec in the command buffer and one is
      required, xmit error response and fail function */
   if(!(*cp) && !allow_empty_filespec)
   {
      ftpputs(ftp, "501 bad path\r\n");
      return FALSE;
   }

   /* if the file spec is too long, xmit error response and fail */
   if(strlen(cp) > FTPMAXPATH)
   {
      ftpputs(ftp, "552 Path/name too long\r\n");
      return FALSE;
   }
   lslash(cp);

   /* assume the path specified is not a relative path */
   relative_path = FALSE;

   /* working pointer to file name */
   cp1 = ftp->filename;

/* if this target system deals with DOS drive letters */
#ifdef DRIVE_LETTERS
   /* if the client specified path contains a colon in the right place */
   if (*cp && (*(cp + 1) == ':'))
   {
      /* copy the drive: to the beginning of the filename */
      *cp1++ = (char) upper_case(*cp);
      *cp1++ = ':';

      /* if the specified drive is the same as the current drive
         and the client specified path does not start at the root */
      if ((upper_case(*cp) == upper_case(ftp->drive[0])) &&
         (*(cp + 2) != FTP_SLASH))
      {
         /* then its a relative path */
         relative_path = TRUE;
      }

      /* bump past drive and colon in client path */
      cp += 2;
   }
   /* client path does not contain a drive spec */
   else
   {
      /* so use the current drive */
      *cp1++ = ftp->drive[0];
      *cp1++ = ':';
      /* if client path does not start at the root */
      if (*cp != FTP_SLASH)
      {
         /* its a relative path */
         relative_path = TRUE;
      }
   }
#else /* DRIVE_LETTERS */
   /* target system is fortunate enough to not have to deal
      with DOS drive letters */

   /* if client path does not start at root */
   if (*cp != FTP_SLASH)
   {
      /* else its a relative path */
      relative_path = TRUE;
   }
#endif /* DRIVE_LETTERS */

   /* if the client specified path is not relative */
   if (!relative_path)
   {
      /* in this case, cp now points to past any drive info in
         the path provided by the client and cp1 now points to past
         any drive info in the constructed file name */
      /* if the path provided by the client isn't absolute, append
         a slash to the constructed file name. note this can happen
         if the client specified a drive other than the default */
      if (*cp != FTP_SLASH)
         *cp1++ = FTP_SLASH;
      /* append client path to our constructed file name */
      strcpy(cp1,cp);
   }
   /* the client specified path was relative */
   else
   {
      /* copy current working directory to file name (following
         any drive letter stuff that might have been added above) */
      strcpy(cp1, ftp->cwd);
      cp1 = &ftp->filename[strlen(ftp->filename)-1]; /* point to end */

      /* if ftp->cwd is not terminated with a slash and the file spec
         is not empty, append a slash */
      if ((*cp1 != FTP_SLASH) && *cp)
      {
         ++cp1;  /* increment ptr past last character to NULL */
         *cp1++ = FTP_SLASH;
         *cp1 = 0;
      }

      /* make sure the concatenation of the specified file name to
         the current working directory wont be too big for the
         file name field */
      if ((strlen(ftp->filename) + strlen(cp)) >= sizeof(ftp->filename))
      {
         ftpputs(ftp,"501 file name too long\r\n");
         return FALSE;
      }

      /* concatenate the file spec from the command line to the
         current working directory */
      strcat(ftp->filename, cp);
   }

   return TRUE; /* function succeeded */
}


/* FUNCTION: ftp_leave_passive_state()
 * 
 * this function is called to make the session leave passive state
 *
 * PARAM1: ftpsvr *ftp
 *
 * RETURNS: 
 */

void ftp_leave_passive_state(ftpsvr * ftp)
{
   /* there's a little confusion about whether this field is 0 or
    *  -1 when the socket is unactive, so check for both
    */
   if ((ftp->datasock != SYS_SOCKETNULL) && (ftp->datasock != 0))
      t_socketclose(ftp->datasock);
   ftp->datasock = 0;
   /* we aren't in passive mode anymore */
   ftp->passive_state = 0;
   /* no data transfer command received while in passive state */
   ftp->passive_cmd = 0;
   ftp->server_dataport = 0;
}



/* FUNCTION: ftps_do_pasv()
 *
 * handle IPv4 PASV command
 *
 * Called when the client requests a transfer in "passive" mode.
 *
 * PARAM1: ftpsvr *ftp
 *
 * RETURNS: 
 */

#ifdef IP_V4
void ftps_do_pasv(ftpsvr * ftp)
{
   /* do_pasv() may re-enter if the client sends us a PASV while we 
    * are transfering a file. This is an error on the client's part. 
    * Its not clear that this can happen given the way the 
    * main state machine works, but check for it just in case
    */
   if (ftp->passive_state & FTPS_PASSIVE_CONNECTED)
   {
      ftpputs(ftp,"425 Data transfer already in progress\r\n");
      return;
   }

   /* This will happen if the client had sent us a PASV and then sent 
    * us another one without an intervening data transfer command. 
    */
   if (ftp->passive_state & FTPS_PASSIVE_MODE)
   {
      ftp_leave_passive_state(ftp);
   }

   /* call sockets routine to do passive open */
   ftps_v4pasv(ftp);

   /* we are now in passive mode, but the client hasn't connected yet */
   ftp->passive_state = FTPS_PASSIVE_MODE;

   /* we haven't received a data transfer command from the client yet */
   ftp->passive_cmd = 0;
}
#endif   /* IP_V4 */



/* FUNCTION: ftps_cmd()
 * 
 * called when we get a command in the FTPS_LOGGEDIN state
 *
 * PARAM1: ftpsvr * ftp
 *
 * RETURNS:  Returns 0 if OK, else ftp error. 
 */

int
ftps_cmd(ftpsvr * ftp)
{
   int   i;    /* scratch for command processing */
   char *   cp;
   char *   cp1;
   u_long   lparm;   /* scratch, for parameter extraction */
   u_short  sparm;   /* "" */
   u_long   ftpcmd;  /* 4 char command as number for fast switching */
   int   relative_path;

   ftpcmd = 0L;
   /* copy 4 bytes of ftp cmd text into local long value, replacing 
   unprintable chars with blanks */
   for (i = 0; i < 4; i++)
   {
      if (ftp->cmdbuf[i] >= ' ')
         ftpcmd = (ftpcmd << 8) | ftp->cmdbuf[i];
      else     /* space over unprintable characters */
         ftpcmd = (ftpcmd << 8) | ' ';
   }

   /* switch on command */
   switch (ftpcmd)
   {
   case 0x53595354:     /* "SYST" */
      /* see what Netscape does with this */
      ftpputs(ftp, "215 UNIX system type\r\n");
      break;
   case 0x54595045:     /* "TYPE" */
      if ((ftp->cmdbuf[5] == 'A') || (ftp->cmdbuf[5] == 'a'))
         ftp->type = FTPTYPE_ASCII;
      else  /* we default all other types to binary */
         ftp->type = FTPTYPE_IMAGE;
      ftpputs(ftp, ftp_cmdok);
      break;
   case 0x50574420:     /* "PWD " */
      sprintf(ftp->filebuf, "257 \"%s\"\r\n", uslash(ftp->cwd));
      lslash(ftp->cwd);
      ftpputs(ftp, ftp->filebuf);
      break;
   case 0x58505744:     /* "XPWD" */
      sprintf(ftp->filebuf, "257 \"%s%s\"\r\n", DRIVE_PTR(ftp), ftp->cwd);
      ftpputs(ftp, ftp->filebuf);
      break;
   case 0x55534552:     /* "USER" */
      return(ftps_user(ftp));
   case 0x504f5254:     /* PORT */
      cp = &ftp->cmdbuf[5];   /* point to IP address text */
      lparm = 0L;
      for (i = 3; i >= 0; i--)   /* extract 4 digit IP address */
      {
         lparm |= (((u_long)atoi(cp)) << (i*8));
         cp = strchr(cp, ',');   /* bump through number to comma */
         if (!cp)    /* must be comma delimited */
         {
            ftpputs(ftp,"501 invalid PORT command\r\n");
            break;
         }
         cp++;    /* point to next digit */
      }
      /* the C break key word really needs a parameter so constructs 
       * like this aren't necessary, anyway, if this is true, its 
       * because we broke out of the above for on an error and we 
       */
      if (!cp)
         break;
      sparm = (u_short)atoi(cp) << 8;
      while (*cp >= '0')cp++; /* bump through number */
         if (*cp != ',')   /* must be comma delimited */
      {
         ftpputs(ftp,"501 invalid PORT command\r\n");
         break;
      }
      cp++;    /* point to next digit */
      sparm |= atoi(cp);

      /* this will happen if the client sends us a PORT while we are 
       * transfering a file. this is an error on the client's part, 
       */
      /* actually, its not clear that this can happen given the way the
         main state machine works, but check for it just in case */
      if (ftp->passive_state & FTPS_PASSIVE_CONNECTED)
      {
         ftpputs(ftp,"425 Data transfer already in progress\r\n");
         break;
      }
      /* this will happen if the client had sent us a PASV and then 
       * sent us a PORT without an intervening data transfer command. 
       */
      if (ftp->passive_state & FTPS_PASSIVE_MODE)
      {
         ftp_leave_passive_state(ftp);
      }

      ftp->host = lparm;
      ftp->dataport = sparm;
      ftpputs(ftp, ftp_cmdok);
      break;
   case 0x51554954:     /* QUIT" */
      /* if we don't have a file transfer going, kill sess now */
      if ((ftp->state != FTPS_SENDING) && (ftp->state != FTPS_RECEIVING))
      {
         ftpputs(ftp, "221 Bye\r\n");     /* session terminating */
         ftp->state = FTPS_CLOSING;
         /*         delftp(ftp);  */
         return -1;  /* signal main loop to kill session */
      }
      else
         return 0;   /* return without flushing QUIT */
   case 0x43574420:  /* "CWD " */
      /* note the intent here is to end up with the client supplied 
       * drive string (as in "c:") in the drive field, and the client 
       * supplied current working directory, without any drive spec, 
       * in the cwd field. to this end we construct the fully 
       * qualified path, including the drive in the filename field 
       * which will 
       */
      cp = ftp_cmdpath(ftp);
      if (!cp) break;
         lslash(cp);    /* convert slashes to local */

      /* assume the path specified is not a relative path */
      relative_path = FALSE;

      /* point to beginning of file name */
      cp1 = ftp->filename;

      /* if this target system deals with DOS drive letters */
#ifdef DRIVE_LETTERS
      /* if the client specified path contains a colon in the right place */
      if (*cp && (*(cp + 1) == ':'))
      {
         /* copy the drive: to the beginning of the filename */
         *cp1++ = (char) upper_case(*cp);
         *cp1++ = ':';

         /* if the specified drive is the same as the current drive
            and the client specified path does not start at the root */
         if ((upper_case(*cp) == upper_case(ftp->drive[0])) &&
             (*(cp + 2) != FTP_SLASH))
         {
            /* then its a relative path */
            relative_path = TRUE;
         }

         /* bump past drive and colon in client path */
         cp += 2;
      }
      /* client path does not contain a drive spec */
      else
      {
         /* so use the current drive */
         *cp1++ = ftp->drive[0];
         *cp1++ = ':';
         /* if client path does not start at the root */
         if (*cp != FTP_SLASH)
         {
            /* its a relative path */
            relative_path = TRUE;
         }
      }
#else /* DRIVE_LETTERS */
      /* target system is fortunate enough to not have to deal
         with DOS drive letters */
      /* if client path does not start at root */
      if (*cp != FTP_SLASH)
      {
         /* else its a relative path */
         relative_path = TRUE;
      }
#endif   /* DRIVE_LETTERS */

      /* if the client specified path is not relative */
      if (!relative_path)
      {
         /* in this case, cp now points to past any drive info in the 
          * path provided by the client and cp1 now points to past 
          *
          * if the path provided by the client isn't absolute, append 
          * a slash to the constructed file name. note this can 
          * happen 
          */
         if (*cp != FTP_SLASH)
            *cp1++ = FTP_SLASH;
         strcpy(cp1,cp);
      }
      /* the client specified path was relative */
      else
      {
         /* copy current working directory to file name (following
            any drive letter stuff that might have been added above) */
          strcpy(cp1, ftp->cwd);     /* copy cwd for change */
          cp1 = ftp->filename + strlen(ftp->filename); /* start at end */
          while (*cp)
             {
             if (*cp == '.' && *(cp+1) == '.')   /* double dot? */
                {  /* back up 1 level */
                if (strlen(ftp->cwd) < 2)  /* make sure we have room */
                   {
                      ftpputs(ftp, "550 Bad path\r\n");
                      ftp_flushcmd(ftp);
                      return 0;   /* not a fatal error */
                   }
                   /* null out last directory level */
                while (*cp1 != FTP_SLASH && cp1 > ftp->filename)
                   *cp1-- = 0;
#ifdef DRIVE_LETTERS
               if (cp1 > (ftp->filename + 2))
#else /* DRIVE_LETTERS */
               if(cp1 > ftp->filename)   /* if not at root... */
#endif /* DRIVE_LETTERS */
                  *cp1 = 0;   /* null over trailing slash */
               cp += 2; /* bump past double dot */
            }
            else if(*cp == FTP_SLASH) /* embedded slash */
               cp++;    /* just skip past it */
            else  /* got a dir name, append to new path */
            {
               if(*(cp1-1) != FTP_SLASH) /* if not at top... */
                  *cp1++ = FTP_SLASH;    /* add the slash to new path */
               while(*cp && *cp != FTP_SLASH)  /* copy directory name */
               {
                  *cp1++ = *cp++;
                  if(cp1 >= &ftp->filename[FTPMAXPATH+2]) /* check length */
                  {
                     ftpputs(ftp, "550 Path too long\r\n");
                     ftp_flushcmd(ftp);
                     return 0;   /* not a fatal error */
                  }
               }
            }
         } /* end of 'while(*cp)' loop */
#ifdef DRIVE_LETTERS
         if(cp1 == ftp->filename + 2)   /* if at root... */
#else /* DRIVE_LETTERS */
         if(cp1 == ftp->filename)   /* if at root... */
#endif /* DRIVE_LETTERS */
            cp1++;   /* bump past slash */
         *cp1 = 0;   /* null terminate new path */
      }

      /* new drive and/or directory is now in ftp->filename */
      /* verify path exists */
      if (!fs_dir(ftp))
      {
         sprintf(ftp->filebuf, "550 Unable to find %s\r\n",ftp->filename);
         ftpputs(ftp, ftp->filebuf);
         break;
      }

#ifdef DRIVE_LETTERS

      /* store the drive into the filename in the drive field */
      ftp->drive[0] = ftp->filename[0];
      ftp->drive[1] = ':';
      ftp->drive[2] = 0;
      /* store the file name sans drive info in the cwd field */
      strcpy(ftp->cwd,ftp->filename + 2);

#else /* DRIVE_LETTERS */

      /* store the file name in the cwd field */
      strcpy(ftp->cwd, ftp->filename);

#endif /* DRIVE_LETTERS */

      sprintf(ftp->filebuf, 
         "200 directory changed to %s%s\r\n", DRIVE_PTR(ftp), ftp->cwd);
      ftpputs(ftp, ftp->filebuf);      /* send reply to client */
      break;
   case 0x4c495354:   /* "LIST" */
   case 0x4e4c5354:   /* "NLST" */
      listcmds++;
      /* attempt to create a complete path from the current working
         directory and the file spec in the command buffer. */
      if (!ftp_make_filename(ftp,TRUE))
         break;

      /* if we are in passive mode but the client hasn't connected to
         data socket yet, just store the command so it will get executed
         when the client connects */
      if (ftp->passive_state == FTPS_PASSIVE_MODE)
      {
         ftp->passive_cmd = ftpcmd;
         break;
      }


      /* generate the listing, if the function fails,
         send an error message back to the client */
      if (fs_dodir(ftp, ftpcmd))
         ftpputs(ftp, "451 exec error\r\n");
      break;

#ifdef IP_V4
   case 0x50415356:   /* "PASV" */
      ftps_do_pasv(ftp);
      break;
#endif /* IP_V4 */

#ifdef IP_V6
   case 0x45505356:   /* "EPSV" - IPv6 extended PASV */
      ftps_epsv(ftp);
      break;
   case 0x45505254:   /* "EPRT"  - IPv6 extended PORT */
      ftps_eprt(ftp);
      break;
#endif /* IP_V6 */

   /* some commands we know about and just don't do: */
   case 0x4d414342:   /* "MACB" - ??? Netscape 3.0 for Win95 sends this */
   case 0x53495a45:   /* "SIZE" - Netscape again. */
   case 0x4f505453:   /* "OPTS" - IE 5.50 */
      ftpputs(ftp, ftp_badcmd);
      break;
   case 0x52455452:   /* "RETR" */
   case 0x53544f52:   /* "STOR" */
      /* attempt to create a complete path from the current working
         directory and the file spec in the command buffer. */
      if (!ftp_make_filename(ftp,FALSE))
         break;

      /* ftp->filename now has drive:path/name of file to try for */

      /* check for user permission */
      if(fs_permit(ftp) == FALSE)
      {
         ftpputs(ftp, ftp_noaccess);
         ftp_xfercleanup(ftp);
         break;
      }

      /* verify that the name of the file we are trying to put or
         get does not exist as a directory */
      if(fs_dir(ftp))
      {
         ftpputs(ftp, "501 bad path\r\n");
         ftp_xfercleanup(ftp);
         break;
      }

      /* if we are in passive mode but the client hasn't connected to
         data socket yet, just store the command so it will get executed
         when the client connects */
      if (ftp->passive_state == FTPS_PASSIVE_MODE)
      {
         ftp->passive_cmd = ftpcmd;
         break;
      }

      IN_PROFILER(PF_FTP, PF_ENTRY);
      if(ftpcmd == 0x52455452)   /* RETR */
         ftp_sendfile(ftp);
      else   /* must be STOR */
         ftp_getfile(ftp);
      IN_PROFILER(PF_FTP, PF_EXIT);

      break;
   case 0x44454c45:   /* "DELE" */
      /* attempt to create a complete path from the current working
         directory and the file spec in the command buffer. */
      if (!ftp_make_filename(ftp,FALSE))
      {
         sprintf(ftp->filebuf, "550 Unable to parse filename  %s\r\n", \
                 ftp->filename);

         ftpputs(ftp, ftp->filebuf);
         break;
      }

      lslash(ftp->filename);

      /* ftp->filename now has drive:path/name of file to try for */

      /* check if the file that the client wants to delete exists */
      if (ftp->type == FTPTYPE_ASCII)
         ftp->filep = vfopen(ftp->filename, "r");  /* ANSI translated mode */
      else
         ftp->filep = vfopen(ftp->filename, "rb"); /* ANSI binary mode */

      if (ftp->filep == NULL)
      {
         /* if we appended VFS path to our constructed file name dont say so */
         if (*(ftp->filename) == FTP_SLASH)
            cp = ftp->filename + 1;
         else
            cp = ftp->filename;

         sprintf(ftp->filebuf, "550 No such file %s\r\n", cp);
         ftpputs(ftp, ftp->filebuf);
         break;
      }
      else
      {
         vfclose(ftp->filep);
      }

      /* check for user permission */
      if(fs_permit(ftp) == FALSE)
      {
         ftpputs(ftp, ftp_noaccess);
         break;
      }

      if (vunlink(ftp->filename) != 0)
      {
         /* if we appended VFS path to our constructed file name dont say so */
         if (*(ftp->filename) == FTP_SLASH)
            cp = ftp->filename + 1;
         else
            cp = ftp->filename;

         sprintf(ftp->filebuf, "550 Unable to delete file %s\r\n", cp);
         ftpputs(ftp, ftp->filebuf);
         break;
      }
      else
      {
         ftpputs(ftp, "250 DELE command successful\r\n");
         break;
      }
   case 0x4e4f4f50:   /* "NOOP" */
      ftpputs(ftp, ftp_cmdok);
      break;
   default:
      sprintf(ftp->filebuf, "500 Unknown cmd %s", ftp->cmdbuf);
      ftpputs(ftp, ftp->filebuf);
    }
   ftp_flushcmd(ftp);
   return 0;
}


/* FUNCTION: ftp_xfercleanup()
 * 
 * Called after a file transfer to clean up session structure and 
 * handle replys.
 *
 * PARAM1: ftpsvr * ftp
 *
 * RETURNS: void
 */

void
ftp_xfercleanup(ftpsvr * ftp)
{
   /* close any open file */
   if(ftp->filep)
   {
      vfclose(ftp->filep);
      ftp->filep = NULL;
   }
   /* close any open socket */
   if (ftp->datasock != SYS_SOCKETNULL)
   {
      t_socketclose(ftp->datasock);
      ftp->datasock = SYS_SOCKETNULL;
   }
   ftp->state = FTPS_LOGGEDIN;

#ifdef   TCP_ZEROCOPY
   /* don't orphan any packets we may have stored in ftpsq */
   while (ftp->ftpsq.q_len > 0)
      tcp_pktfree ((PACKET)getq(&ftp->ftpsq) );
#endif   /* TCP_ZEROCOPY or not */

   /* we aren't in passive mode anymore */
   ftp_leave_passive_state(ftp);
}



#ifdef TCP_ZEROCOPY  /* use InterNiche Zero-copy TCP socket extensions */
extern   PACKET tcp_pktalloc(int);
extern   void tcp_pktfree(PACKET);
int ftps_rxupcall(struct socket * so, PACKET pkt, int code);
#endif   /* TCP_ZEROCOPY */


/* FUNCTION: ftp_sendfile()
 *
 * Send a file. Filename, Port, type, and IP address are all 
 * set in ftp structure. Returns 0 if OK, else ftp error. 
 *
 * 
 * PARAM1: ftpsvr * ftp
 *
 * RETURNS:  Returns 0 if OK, else ftp error. 
 */

int
ftp_sendfile(ftpsvr * ftp)
{
   int   e;
   int   reterr = 0;
   u_long   put_timer;  /* timer for max time to loop in this routine */

#ifndef TCP_ZEROCOPY
   int   bytes = 0;
#endif

   /* See if this is start of send */
   if (ftp->state != FTPS_SENDING)
   {
      lslash(ftp->filename);
      if (ftp->type == FTPTYPE_ASCII)
         ftp->filep = vfopen(ftp->filename, "r");
      else
         ftp->filep = vfopen(ftp->filename, "rb");
      if (!ftp->filep)
      {
         ftpputs(ftp, "451 aborted, can't open file\r\n");

         /* if we are already connected to the client because we were 
          * in passive mode, close the connection to client and exit 
          */
         if (ftp->passive_state & FTPS_PASSIVE_MODE)
            ftp_leave_passive_state(ftp);
         return 451;
      }
      ftpputs(ftp, "150 Here it comes...\r\n");
      /* if we are not already connected from a previous PASV */
      if (!(ftp->passive_state & FTPS_PASSIVE_CONNECTED))
      {
         /* connect to client */
         ftp->datasock = FTP_TCPOPEN(ftp);
         if (ftp->datasock == SYS_SOCKETNULL)
         {
            ftpputs(ftp, "425 Can't open data connection\r\n");
            reterr = 425;
            goto ftsnd_exit;
         }
      }

#ifdef TCP_ZEROCOPY
      ftp->iosize = tcp_mss(LONG2SO(ftp->datasock));
#endif /* TCP_ZEROCOPY */

      ftp->state = FTPS_SENDING;
      ftp->filebuflen = 0;
   }

   /*
    * loop below while sending, quit when we reach MAX number of 
    * ftpticks we're allowed. The ftps_loop() routine will call us
    * again later
    */
   put_timer = (ftpticks + FTPTPS);    /* set timeout tick */

#ifdef TCP_ZEROCOPY
   /* different loops for ZEROCOPY and plain sockets */
   for (;;)
   {
      PACKET pkt;
      if (ftp->pkt == NULL)   /* need to read more file data */
      {
         pkt = tcp_pktalloc(ftp->iosize);
         if (!pkt)   /* if no buffer, try again later */
            return 0;
         e = vfread(pkt->nb_prot, 1, ftp->iosize, ftp->filep);
         if (e < 0)
         {
            ftpputs(ftp, "451 aborted, file read error\r\n");
            reterr = 451;
            break;
         }
         pkt->nb_plen = (unsigned)e;
      }
      else  /* retry old packet saved in session structure */
      {
         pkt = ftp->pkt;
      }

      if (pkt->nb_plen)
      {
         e = tcp_xout(ftp->datasock, pkt);
         if (e == ENP_RESOURCE)  /* blocked */
         {
            ftp->pkt = pkt;   /* save pkt for later */
            tk_yield();       /* spin system */
            return 0;         /* wait for socket to clear */
         }
         else if(e < 0)
         {
            ftpputs(ftp, "426 aborted, data send error\r\n");
            reterr = 426;
            if (pkt) 
            {
               tcp_pktfree(pkt);
            }
            break;
         }
         else  /* sent without error */
         {
            /* tcp layer will free pkt when data is acked, just 
            clear to ftps pointer for now */
            ftp->pkt = NULL;
         }
         ftp->lasttime = ftpticks;     /* reset timeout */
         /*
          * force return to let other FTP sessions run if we have had CPU
          * continuously for a longish while
          */
         if (ftpticks > put_timer)
            return 0;
      }
      else  /* end of file & all bytes sent */
      {
         tcp_pktfree(pkt);
         ftp->pkt = NULL;
         break;   /* fall to send termination logic */
      }
   }
#else /* not TCP_ZEROCOPY */
   for (;;)
   {
      if (ftp->filebuflen == 0)  /* need to read more file data */
      {
         /* if its an ASCII type transfer */
         if (ftp->type == FTPTYPE_ASCII)
         {
            /* then we need to insert a CR before any LF that is not
             * already preceeded by an LF.
             * Since the last character we read before filling up the 
             * file transfer buffer could be a lonely LF and in that 
             * case we'd have no room to insert the CR before it and 
             * it would be a righteous pain to keep track of this one 
             * boundary condition in the state machine, we will 
             * terminate the loop when there is still 1 byte left 
             */
            while (ftp->filebuflen < FILEBUFSIZE - 1)
            {
               int   next_char;
               int   prev_char   =  0;

               /* read next character from file */
               next_char = vgetc(ftp->filep);
               /* break on end of file */
               if (next_char < 0)
                  break;
               /* if we read an LF */
               if (next_char == '\n')
               {
                  /* and the previous char wasn't a CR */
                  if (prev_char != '\r')
                  {
                     /* insert a CR ahead of the LF */
                     ftp->filebuf[ftp->filebuflen] = '\r';
                     ftp->filebuflen++;
                  }
               }
               ftp->filebuf[ftp->filebuflen] = (char) next_char;
               ftp->filebuflen++;
               /* if we just read a LF, break. why? you ask. well 
                * what happens if the last byte we read before 
                * filling up the transfer buffer is a CR. when we 
                * come back in here again and read the LF, that LF 
                * looks like a lonely LF, so we'd end up inserting 
                * another CR, which wouldn't be right. so to protect 
                * against that and allow us to avoid storing the last 
                * character read in the ftpsrv structure in order to 
                * support this archaic feature, we just terminate the 
                * read when we get to the end of a line on the 
                * assumption that theres not going to be too many 
                * people moving text 
                */
               if (next_char == '\n')
                  break;
               prev_char = next_char;
            }
         }
         else  /* its a binary transfer so just read the data */
         {
            e = vfread(ftp->filebuf, 1, FILEBUFSIZE, ftp->filep);
            if (e < 0)
            {
               ftpputs(ftp, "451 aborted, file read error\r\n");
               reterr = 451;
               break;
            }
            ftp->filebuflen = (unsigned)e;
         }
      }
      bytes = (int)ftp->filebuflen;
      if (bytes)
      {
         e = t_send(ftp->datasock, ftp->filebuf, bytes, 0);
         if (e < 0)
         {
            /* See what kind of error it is. If we're out of sockbuf
             * space or buffers then 
             * return 0 to try again later. If its anything else then 
             * it's serious and we should abort with an error 
             */
            e = t_errno(ftp->datasock);
            if((e == EWOULDBLOCK) || (e == ENOBUFS))
            {
               return 0;   /* out of socket space, try layer */
            }
            ftpputs(ftp, "426 aborted, data send error\r\n");
            reterr = 426;
            break;
         }
         else  /* no send error */
         {
#ifdef NPDEBUG /* sanity test socket return values */
            if (e > FILEBUFSIZE)
            {  dtrap(); /* serious logic problem here */
               return 0;
            }
#endif   /* NPDEBUG */
            ftp->filebuflen -= e;
#ifdef NPDEBUG
            if ((int)ftp->filebuflen < 0)
            {  dtrap(); 
               return 0;
            }
#endif
            if (e != bytes)   /* partial send on NBIO socket */
            {
               if (e != 0) /* sent some data, but not all - move buffer */
               {
                  MEMMOVE(ftp->filebuf, ftp->filebuf+e, ftp->filebuflen);
               }
               return 0;      /* try again later */
            }
         }

         ftp->lasttime = ftpticks;     /* reset timeout */

         /*
          * force return to let other FTP sessions run if we have had CPU
          * continuously for a longish while
          */
         if (ftpticks > put_timer)
         {
            return 0;
         }
      }
      else  /* end of file & all bytes sent */
         break;   /* fall to send termination logic */
   }
#endif   /* TCP_ZEROCOPY */

ftsnd_exit:

   /* get here if EOF or fatal error */

#ifdef NPDEBUG
   if (reterr == 0 && ftp->filebuflen != 0)  /* buffer should be empty */
   {   dtrap();   }
#endif

   /* first reply to user if xfer was OK */
   if (!reterr)
      ftpputs(ftp, "226 Transfer OK, Closing connection\r\n");

   ftp_xfercleanup(ftp);
   return reterr;
}


/* FUNCTION: ftp_getfile()
 * 
 * Get a file from client. We open a connection to the client
 * and he will send it to us. Filename, Port, type, and IP 
 * address are all set in ftp structure. 
 *
 * PARAM1: ftpsvr * ftp
 *
 * RETURNS: Returns 0 if OK, else ftp error. 
 */

int
ftp_getfile(ftpsvr * ftp)
{
   int   bytes;
   int   e;
   int   reterr   =  0;
   u_long   get_timer;  /* ctick to force a return */

   /* See if this is start of receive operation */
   if (ftp->state != FTPS_RECEIVING)
   {
      lslash(ftp->filename);

      if (ftp->type == FTPTYPE_ASCII)
         ftp->filep = vfopen(ftp->filename, "w");
      else
         ftp->filep = vfopen(ftp->filename, "wb");
      if (!ftp->filep)
      {
         ftpputs(ftp, "451 aborted, can't open file\r\n");

         /* if we are already connected to the client because we were 
          * in passive mode, close the connection to client and exit 
          */
         if (ftp->passive_state & FTPS_PASSIVE_MODE)
            ftp_leave_passive_state(ftp);

         return 550;
      }
      /* if we are not already connected from a previous PASV */
      if (!(ftp->passive_state & FTPS_PASSIVE_CONNECTED))
      {
         ftp->datasock = FTP_TCPOPEN(ftp);
         if (ftp->datasock == SYS_SOCKETNULL)
         {
            ftpputs(ftp, "425 Can't open data connection\r\n");
            reterr = 425;
            goto ftget_exit;
         }
      }
      ftpputs(ftp, "150 Connecting for STOR\r\n");
      ftp->state = FTPS_RECEIVING;
#ifdef TCP_ZEROCOPY
      ftp->rxcode = 0;
      t_setsockopt(ftp->datasock, 0, SO_CALLBACK, (void*)ftps_rxupcall, 0);
#endif   /* TCP_ZEROCOPY */
   }

   get_timer = ftpticks + FTPTPS;   /* set tick to timeout this loop */

#ifdef TCP_ZEROCOPY
   /* different data transfer loops for zerocopy option */
   for (;;)
   {
      PACKET pkt;

      /* handle any packets the upcall has queued for us */
      while (ftp->ftpsq.q_len > 0)
      {
         LOCK_NET_RESOURCE(NET_RESID);       /* syncronize ftpsq with upcall */
         pkt = (PACKET)getq(&(ftp->ftpsq));
         UNLOCK_NET_RESOURCE(NET_RESID);
         if (ftp->rxcode && ftp->rxcode != ESHUTDOWN) /* network error? */
         {
            tcp_pktfree(pkt);
            continue;
         }
         bytes = pkt->nb_plen;
         e = vfwrite(pkt->nb_prot, 1, bytes, ftp->filep);
         tcp_pktfree(pkt);
                     
         if (e == 0) /* memory allocation failed, not enough space to write a file on the server */
         {            
            ftpputs(ftp, "452 Insufficient storage space, file write error\r\n");
            reterr = 0; /* not a fatal error to abort */
            notfatal = 1;
            goto ftget_exit;
         }
         if (e != bytes)
         {
            dtrap();
            ftpputs(ftp, "450 File unavailable, file write error\r\n");
            reterr = 0; /* not a fatal error to abort */
            notfatal = 1; 
            goto ftget_exit;
         }
         /* force return to let other FTP sessions run if we have had CPU
          * continuously for a while.
          */
         if (ftpticks > get_timer)
            break;

#if !defined(CHRONOS) && !defined(SUPERLOOP)
         /* the yield here not only lets other tasks run, but if we have
          * incoming data in the rcvdq it gives pktdemux a chance to get it
          * into our queue via the upcall. This is not an Issue on Chronos
          * since it runs the Net task at a higher priority, and it's
          * dangerous on superloops.
          */
         tk_yield();    /* let other tasks run */
#endif   /* neither CHRONOS nor SUPERLOOP */

      }
      if (ftp->rxcode)  /* network error? */
      {
         if (ftp->rxcode == ESHUTDOWN) /* file receive done */
            reterr = 0;    /* no error */
         else
         {
            reterr = 426;
            ftpputs(ftp, "426 aborted, data recv error\r\n");
         }
         break;   /* break out of for loop */
      }
      return 0;      /* done for now, let other things run */
   }
#else /* not TCP_ZEROCOPY */
   for (;;)
   {
      bytes = t_recv(ftp->datasock, ftp->filebuf, 1024, 0);
      if (bytes > 0)
      {
         e = vfwrite(ftp->filebuf, 1, bytes, ftp->filep);
         if (e == 0) /* memory allocation failed, not enough space to write a file on the server */
         {            
            ftpputs(ftp, "452 Insufficient storage space, file write error\r\n");
            reterr = 0; /* not a fatal error to abort */
            notfatal = 1;
            break;
         }
         if (e != bytes)
         {
            dtrap();
            ftpputs(ftp, "450 File unavailable, file write error\r\n");
            reterr = 0; /* not a fatal error to abort */
            notfatal = 1;
            break;
         }
         ftp->lasttime = ftpticks;     /* reset timeout */
      }
      if (bytes < 0) /* error, no data (EWOULDBLOCK) or finished */
      {
         e = t_errno(ftp->datasock);
         if (e == EWOULDBLOCK)
            return 0;      /* no work right now, let other things run */
         else  /* probably socket cloesed due to end of file */
         {
            bytes = 0;
            break;   /* break our of read loop to exit code */
         }
      }
      else if(bytes == 0)     /* another form of broken? */
      {
         /*         bytes = -1; */
         ftp->state = FTPS_CLOSING;
         break;
      }
      /*
       * force return to let other FTP sessions run if we have had CPU
       * continuously for a longish while.
       */
      if (ftpticks > get_timer)
         return 0;
   }     /* end of forever loop */

   if (bytes < 0)
   {
      ftpputs(ftp, "426 aborted, data recv error\r\n");
      reterr = 426;
   }
#endif   /* TCP_ZEROCOPY or not */

ftget_exit:

   /* first reply to user if xfer was OK */
   if (!reterr && !notfatal)
      ftpputs(ftp, "226 Transfer OK, Closing connection\r\n");

   ftp_xfercleanup(ftp);
   return reterr;
}


#ifdef TCP_ZEROCOPY

/* FUNCTION: ftps_rxupcall()
 *
 * Upcall handler for TCP_ZEROCOPY data processing. This is passed
 * packets directly  from the sockets layer for fast, copyless processing
 *
 * PARAM1: struct socket * so
 * PARAM2: PACKET pkt
 * PARAM3: int code
 *
 * RETURNS: 0 if packet accepted, -1 if not.
 */

int
ftps_rxupcall(struct socket * so, PACKET pkt, int code)
{
   ftpsvr * ftps;

   for (ftps = ftplist; ftps; ftps = ftps->next)
      if ((long)ftps->datasock == SO2LONG(so))
         break;

   if (!ftps)     /* can't find socket? */
   {
      /* if no pkt and code is set, this may be a reset (or
       * other cleanup) of a stale socket for which we have
       * already cleared out the ftps. In this case just return,
       * else dtrap & return error code.
       */
      if (pkt || (code == 0))
      {
         dtrap();    /* If pkt or no code, tel programmer */
         return -1;  /* return error code */
      }
      return 0;      /* not an error */
   }
   ftps->rxcode = code;    /* save code */

   if (pkt)    /* save pkt, if any */
      putq( &(ftps->ftpsq), pkt );

#ifndef SUPERLOOP
   /* If the queue is filling, spin server task */
   if(ftps->ftpsq.q_len > 10)
   {
      TK_WAKE(&to_ftpsrv);    /* make sure it's awake */
      tk_yield();             /* let FTP server task spin */
   }
#endif   /* SUPERLOOP */

   ftps->lasttime = ftpticks;    /* reset timeout */

   return 0;
}
#endif   /* TCP_ZEROCOPY or not */


