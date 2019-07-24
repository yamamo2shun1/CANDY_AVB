/*
 * FILENAME: ftpclnt.c
 *
 * Copyright 1997- 2000 By InterNiche Technologies Inc. All rights reserved
 *
 *
 * MODULE: FTPCLIENT
 *
 * ROUTINES: fc_check(), fc_getreply(), fc_checklogin(), 
 * ROUTINES: fc_checkcmd(), fc_endxfer(), fc_clearf(), fc_sendmore(), 
 * ROUTINES: fc_getmore(), fc_dataconn(), fc_killsess(), fc_connect(), 
 * ROUTINES: fc_get(), fc_put(), fc_connopen(), fc_senduser(), fc_sendpass(), 
 * ROUTINES: fc_sendport(), fc_sendcmd(), fc_dir(), fc_pwd(), fc_chdir(), 
 * ROUTINES: fc_settype(), fc_quit(), fc_ready(), fc_usercmd(), 
 * ROUTINES: fc_hashmark(), fc_state(), fc_pasv(), 
 *
 * PORTABLE: yes
 */

/* ftpclnt.c Generic FTP client. This file contains the guts of the 
 * FTP client logic. There are several entry poins for user commands, 
 * include connect, send, and recv. These initiate a change in the 
 * connection's state machine which is should result in the 
 * performance of the desired task. These jobs are driven by periodic 
 * calls to fc_check, which can be made from a super loop, or a task 
 * which sleeps on ftp_clients == NULL. 
 * 1/12/97 - Created. John Bartas 
 */

#include "ftpport.h"    /* TCP/IP, sockets, system info */

#ifdef FTP_CLIENT
#include "ftpsrv.h"
#include "ftpclnt.h"

#ifdef IP_V6
#include "socket6.h"
#endif   /* IP_V6 */


/* operating parmeters which may be overwritten by application code 
 * 1. If the FTP Client sends any FTP command, and it doesn't receive 
 * a reply for fc_replytmo seconds, then the FTP Client connection is 
 * closed. 
 * 2. If the FTP Client connection has remained idle ( no 
 * commands sent ) for fc_activity_tmo seconds, then the FTP Client 
 * connection is closed 
 */
int  fc_replytmo       =  20;    /* secs of inactivity for cmd timeout */
int  fc_connect_tmo    =  300;   /* secs of inactivity for connect timeout */
int  fc_activity_tmo   =  1200;  /* ftp client's inactivity timeout (secs) */

/* Starting port number for data transfers of ftp client */
#define FTP_START_DATA_PORT  8000  /* Refered RFC1700 Assigned nums */

static unshort fc_next_port=FTP_START_DATA_PORT ;

/* The FTP client's per-port message handler */
extern   void fc_printf(ftpc *, char *, ...);   /* per-port response printer */

/* internal routines */
int   fc_connopen(ftpc * ftpconn);  /* initiate command connection */
int   fc_getreply(ftpc * ftpconn, int * code);  /* check for ftpconn reply */
int   fc_dataconn(ftpc * ftpconn);  /* initiate data connection */
int   fc_checklogin(ftpc * ftpconn);/* see if user login is complete */
int   fc_checkcmd(ftpc * ftpconn);  /* see if last cmd is complete */
int   fc_sendmore(ftpc * ftpconn);  /* send more data on a STOR command */
int   fc_getmore(ftpc * ftpconn);   /* send more data on a RETR command */
int   fc_senduser(ftpc * ftpconn);
int   fc_sendpass(ftpc * ftpconn);
int   fc_sendport(ftpc * ftpconn);
int   fc_usercmd(ftpc * ftpconn, int cmdcode, char * cmdarg);
int   fc_sendcmd(ftpc * ftpconn);
int   fc_ready(ftpc * ftpconn);     /* test session readiness for command */
void  fc_hashmark(ftpc * ftpconn, ulong before, unsigned added);
void  fc_killsess(ftpc * ftpconn);  /* clean up ftpconn resources */
void  fc_clearf(ftpc * ftpconn);    /* reset ftpconn counters */
void  fc_endxfer(ftpc * ftpconn);   /* end data xfer */

#ifdef FC_LOG
GEN_IO ftplog =NULL;       /* By default log o/p would goto std output */
int      log_flag=FALSE;   /* By default, logging is disabled */
/* Text strings for logstate and cmdstate. Update this if any change
   is done to the states */
char *   fc_str[]=   {
   "0",
   "FCL_CONNECTING", /* 1 */ 
   "FCL_CONNECTED",  /* 2 */
   "FCL_READY",      /* 3 */
   "FCL_SENTUSER",   /* 4 */
   "FCL_SENTPASS",   /* 5 */
   "FCL_LOGGEDIN",   /* 6 */
   "FCL_PENDING",    /* 7 */
   "FCL_CLOSING",    /* 8 */
   "9",
   "FCC_RECVPORT",   /* 10*/
   "FCC_RECVOK",     /* 11*/
   "FCC_RECVCONN",   /* 12*/
   "FCC_RECEIVING",  /* 13*/
   "FCC_RECVDONE",   /* 14*/
   "15",
   "16",
   "17",
   "18",
   "19",
   "FCC_SENDPORT",   /* 20*/
   "FCC_SENDOK",     /* 21*/
   "FCC_SENDCONN",   /* 22*/
   "FCC_SENDING",    /* 23*/
   "FCC_SENDDONE",   /* 24*/
   "25",
   "26",
   "27",
   "28",
   "29",
   "FCC_NLSTPORT",   /* 30*/
   "FCC_NLSTOK",     /* 31*/
   "FCC_NLSTCONN",   /* 32*/
   "FCC_NLSTING",    /* 33*/
   "FCC_NLSTDONE",   /* 34*/
   "35"
};

#define FC_CBLOG(fc) { \
                        if (log_flag==TRUE)                              \
                           log_printf(ftplog,"logstate=%s,cmdstate=%s\n",\
                              fc_str[fc->logstate],fc_str[fc->cmdstate]);                \
                     }
#else /* FC_LOG */
#define  FC_CBLOG(fc)   ;
#endif   /* FC_LOG */

/* Define FC_CBACK if FC_USECALLBACK is enabled */

#ifdef FC_USECALLBACK
void  (*fc_callback)(void * fc, int logstate, int cmdstate) = NULL ;
#define FC_CBACK(fc) {if(fc_callback) \
                         (*fc_callback)((void *)fc,fc->logstate,fc->cmdstate);}
#else
#define  FC_CBACK(fc)   ;
#endif   /* FC_USECALLBACK */

/* The user application can set a callback via fc_callback to receive 
 * information when the logstate or cmdstate changes This feature is 
 * available only when FC_USECALLBACK in enabled in ftpclnt.h If 
 * FC_LOG is enabled, then the change of state is also logged. If 
 * FC_USEBCALLBACK and FC_LOG are both disabled, FC_CALLBACK will 
 * evaluate to empty braces. 
 */
#define FC_CALLBACK(fc) {                                                   \
                           FC_CBACK(fc);                                    \
                           FC_CBLOG(fc);                                    \
                        }

/* FC_SETLOGSTATE updates the logstate of ftpcon and calls 
 * FC_CALLBACK. Hence, if FC_CALLBACK(fc) is {}, it evaluates to a 
 * simple C assignment and there is no extra code. 
 */
#define  FC_SETLOGSTATE(fc,lstate)  {  fc->logstate=lstate; FC_CALLBACK(fc);  }

struct ftpc *  ftp_clients =  NULL  ;  /* support multiple client links */

struct   queue    ftpcq;      /* contains messages from other tasks */
unsigned char ftpc_msgsize [FTPC_NUM_MSGS] = {0x10, 0x0C, 0x0C, 0x0C, 0x00, 0x20, 0x0C, 0x10, 0x0C, 0x0C, 0x0C, 0x00, 0x08};
struct ftpc_msg_stats ftpc_msg_stats;
struct ftpc_err ftpc_err;
extern struct ftpc *  ftp_get_con(void * pio);

#define  FC_MENULOG() ;


/* FUNCTION: fc_check()
 * 
 * fc_check() - poll ftp clients for work. This does not need to be 
 * called if(ftp_clients == NULL), else it should be called 
 * periodicly to move FTP transactions. 
 *
 * PARAM1: void
 *
 * RETURNS: 0 if there are not more conenctions open,
 *          1 if there are connections but no IO in progress,
 *          2 if the is at least 1 file transfer going
 */

int  
fc_check(void)
{
   ftpc * ftpconn;
   ftpc * ftpc_next;
   int   e; /* generic error holder */
   int   retval = 0;

   /* loop through active client list */
   for (ftpconn = ftp_clients; ftpconn; ftpconn = ftpc_next)
   {
      ftpc_next = ftpconn->next;
      if (ftpconn->in_use) /* make sure we're not being re-entered */
         continue;
      ftpconn->in_use++;   /* set re-entry flag */

      e = 0;   /* clear error before each connection */
      switch (ftpconn->logstate)
      {
      case FCL_CONNECTING:
      case FCL_CONNECTED:
         /* see if we timed out on non-blocking connect() */
         if ((ftpconn->last_cmd + ((unsigned long)fc_connect_tmo * TPS)) < ftp_ticks)
         {
            fc_printf(ftpconn,
             "FTP Connection timed out on non-blocking connect.\n");
            e = -1;
         }
         else
         {
            e = fc_connopen(ftpconn);  /* keep polling until state goes READY */
         }
         break;
      case FCL_SENTUSER:   /* check for reply to USER or PASS cmd */
      case FCL_SENTPASS:
         e = fc_checklogin(ftpconn);
         if (ftpconn->logstate == FCL_LOGGEDIN)
            fc_printf(ftpconn, "ftp user \"%s\" logged in.\n", ftpconn->username);
         break;
      case FCL_READY:      /* connected & ready, but not doing login */
         if ((ftpconn->last_cmd + ((unsigned long)fc_activity_tmo*TPS)) < ftp_ticks)
         {
            fc_printf(ftpconn,
             "FTP Connection timed out : No activity for %d secs\n",
             fc_activity_tmo);
            e=-1;
         }
         break;
      case FCL_LOGGEDIN:   /* nothing to do here */
         if ((ftpconn->last_cmd + ((unsigned long)fc_activity_tmo*TPS)) < ftp_ticks)
         {
            fc_printf(ftpconn,
             "FTP Connection timed out : No activity for %d secs\n",
             fc_activity_tmo);
            e=-1;
         }
         break;
      case FCL_PENDING:    /* command in progress */
         e = fc_checkcmd(ftpconn);  /* push command to next state */
         break;
      case FCL_CLOSING:    /* close FTP connection */
         e = -1;     /* mark for deletion */
         break;
      default:
         e = -1;     /* mark for deletion */
         break;
      }
      ftpconn->in_use--;   /* clear re-entry flag */

      if (ftpconn->logstate == FCL_PENDING)
         retval = 2;       /* At least 1 connection has transaction in progress */

      /* delete any terminating connections */
      if (e)
      {
#ifdef FC_LOG
         if (log_flag == TRUE)
         {
            ftpconn->in_use++;   /* set re-entry flag */
            log_printf(ftplog,"Closing FTP client connection.\n");
            ftpconn->in_use--;   /* clear re-entry flag */
         }
#endif
         ftpc_next = ftpconn->next; /* maintain pointer to next */
         fc_killsess(ftpconn);
      }
   }
   if((retval == 0) && (ftp_clients))
      retval = 1;    /* at least one connection open */
   return retval;
}




/* FUNCTION: fc_getreply()
 * 
 * fc_getreply() - check for a reply to and outstanding command. 
 * Performs state machine actions if reply is ready. Does not handle 
 * timeouts. 
 * 
 * PARAM1: ftpc * ftpconn
 * PARAM2: int * replycode
 *
 * RETURNS: Returns: 0 if we have full command, -1 if error, 1 if no 
 * reply yet. 
 */

int
fc_getreply(ftpc * ftpconn, int * replycode)
{
   int   e;    /* error holder */
   int   current; /* cuurent cmd text length */
   char *   beginLine;
   char *   endLine;
   char *   cp;
   int   rc =  1;

   current = strlen(ftpconn->cmdbuf);
   e = t_recv(ftpconn->cmd_sock, ftpconn->cmdbuf + current, 
    CMDBUFSIZE - current, 0);
   if (e < 0)  /* read error ? */
   {
      e = t_errno(ftpconn->cmd_sock);
      if (e == EWOULDBLOCK)
      {
         /* If some reply info is left in buffer from a previous read,
          * return code to this affect
          */
         if(ftpconn->cmdbuf[0] == 0)
            return 1;   /* wait some more */
      }
      else
      {
         fc_printf(ftpconn, "ftp client read error %d\n", e);
         return -1;
      }
   }
   else if (e == 0) /* socket closed by server */
   {
      fc_printf(ftpconn, "ftp connection closed by server\n");
      return -1;
   }

   /* add bytes read on last t_recv() to total bytes in buffer */
   current += e;
   /* null terminate after length of buffer */
   ftpconn->cmdbuf[current] = 0;
   beginLine = ftpconn->cmdbuf;
   for (;;)
   {
      /* find CRLF at end of command buffer */
      endLine = strstr(beginLine, "\r\n");
      /* not a complete line yet so break and return no reply yet */
      if (!endLine)
         break;

      /* scan reply code from decimal digits at beginning of line */
      cp = beginLine;
      *replycode = 0;
      while ((*cp >= '0') && (*cp <= '9'))
      {
         *replycode = (*replycode * 10) + *cp - '0';
         cp++;
      }

      /* if the character following the digits is not a dash 
       * continuation character (see section 4.2 of RFC 959) and 
       * three digits were read
       */
      if ((*cp != '-') && (cp == beginLine + 3))
      {
         if (ftpconn->options & FCO_VERB)
            fc_printf(ftpconn, "ftp reply: %s", ftpconn->cmdbuf);
         if (*replycode == 421)
         {
            fc_printf(ftpconn, "ftp closing session per code 421\n");
            rc = -1;
         }
         else
            rc = 0;  /* command is ready */
         break;
      }
      /* we didn't see a complete reply so try starting at the
         beginning of the next line */
      beginLine = endLine + 2;
   }
   return rc;
}



/* FUNCTION: fc_checklogin()
 * 
 * fc_checklogin() - Check for a reply to an outstanding USER or PASS 
 * command. Handle state changes if reply has arrived, do timeouts. 
 *
 * PARAM1: ftpc * ftpconn
 *
 * RETURNS: Returns 0 if OK (including no reply), if error returns -1. 
 */

int
fc_checklogin(ftpc * ftpconn)
{
   int   e;
   int   replycode;

   e = fc_getreply(ftpconn, &replycode);
   if (e == 1) /* reply not ready yet */
   {
      /* see if we should time out waiting for reply */
      if ((ftpconn->last_cmd + ((unsigned long)fc_replytmo*TPS)) < ftp_ticks)
      {
         fc_printf(ftpconn, "ftp login timed out\n");
         return -1;
      }
      return 0;   /* else wait some more */
   }
   if (e != 0) /* fatal connection error? */
      return -1;


   if ( replycode == 220 )
   {
      /* remove all 220 commands from buffer */
      /* We have remove all lines starting with 220 from cmd buf */
      char *   curpos, *   nextpos;

      curpos=ftpconn->cmdbuf ;
      while ( (nextpos= strstr(curpos, "\r\n")) != NULL )
      {
         replycode = atoi(curpos);
         if ( replycode == 220 )   /* keep searching */
            curpos=nextpos+2;  
         else 
            break;
      }
      /* if there were any 220 commands */
      if (curpos != ftpconn->cmdbuf)
      {
         int   reply_220_len; /* length of 220 commands */
         int   remainder_len; /* length of remainder of command buffer */

         /* compute length of 220 commands and length of remainder of
            command buffer */
         reply_220_len = curpos - ftpconn->cmdbuf;
         remainder_len = sizeof(ftpconn->cmdbuf) - reply_220_len;

         /* move the remainder of the command buffer to the front */
         MEMMOVE(ftpconn->cmdbuf,curpos,remainder_len);
         /* zero out the number of bytes removed at the tail end of buf */
         MEMSET(ftpconn->cmdbuf + remainder_len,0,reply_220_len);
      }
   }

   /* we have complete reply in ftpconn->cmdbuf */
   if (replycode > 399 || replycode < 200) 
   {
      fc_printf(ftpconn, "User login failed, error.\n");
      FC_SETLOGSTATE(ftpconn,FCL_READY);  /* not logged in */
      return -1;
   }

   if (replycode < 230) /* ignore 220, 226, et.al. */
   {
      return 0;
   }

   /* code is now in range 230 - 399 */
   switch (ftpconn->logstate)
   {
   case FCL_SENTUSER:   /* got reply to USER cmd */
      if (replycode >= 300 && replycode < 399)  /* need password */
      {
         e = fc_sendpass(ftpconn);  /* send password */
         if (e)
            return e;
      }
      else
         FC_SETLOGSTATE(ftpconn,FCL_LOGGEDIN); 
      break;      
   case FCL_SENTPASS:   /* got reply to PASS cmd */
      FC_SETLOGSTATE(ftpconn,FCL_LOGGEDIN); 
      break;
   }
   return 0;
}




/* FUNCTION: fc_checkcmd()
 * 
 * fc_checkcmd() - check for a reply to and outstanding command. 
 * Performs state machine actions if reply is ready. Also does 
 * timeouts. 
 *
 * PARAM1: ftpc * ftpconn
 *
 * RETURNS: Returns 0 if OK, if error returns -1. 
 */

int
fc_checkcmd(ftpc * ftpconn)
{
   int   e;
   int   replycode;
   char* cp;

   /* if waiting for connect or doing data IO, just keep doing it */
   switch (ftpconn->cmdstate)
   {
   case FCC_SENDING:
      e = fc_sendmore(ftpconn);
      return e;
   case FCC_RECEIVING:
   case FCC_NLSTING:
      e = fc_getmore(ftpconn);
      return e;
   case FCC_SENDCONN:   /* we are trying to connect... */
   case FCC_RECVCONN:
   case FCC_NLSTCONN:
      e = fc_dataconn(ftpconn);  /* poll data connect */
      if (e == 1) /* data port connected? */
      {
         ftpconn->cmdstate++; /* go listen for result */
         FC_CALLBACK(ftpconn);
         e = 0;   /* definitely not an error */
         /* If cmdstate is FCC_NLSTING or FCC_RECEIVING, 
          * or FCC_SENDING display the message
          */
         if (ftpconn->cmdstate == FCC_SENDING)
            fc_printf(ftpconn, "sending file... cticks = %lu\n", cticks);
         else   /* FCC_NLSTING or FCC_RECEIVING */
            fc_printf(ftpconn, "receiving file...\n");
      }
      else
      {
         /* see if we should time out waiting for connect */
         if ((ftpconn->last_cmd + ((unsigned long)fc_replytmo*TPS)) < ftp_ticks)
         {
            fc_printf(ftpconn, "ftp command timed out\n");
            return -1;
         }
      }
      return e;
   }

   /* fall to here if expecting a command reply */
   e = fc_getreply(ftpconn, &replycode);
   if (e == 1) /* reply not ready yet */
   {
      /* see if we should time out waiting for reply */
      if ((ftpconn->last_cmd + ((unsigned long)fc_replytmo*TPS)) < ftp_ticks)
      {
         fc_printf(ftpconn, "ftp command timed out\n");
         return -1;
      }
      else
      {
         return 0;   /* else wait some more */
      }
   }
   else if (e != 0)  /* fatal connection error? */
   {
      return -1;
   }
   else     /* e=0 */
   {
      if (strstr(ftpconn->cmdbuf, "\r\n") == NULL)
         return 0;
   }


   /* fall to here if we got whole reply */
   e = 0;      /* clear error holder prior to case statement */
   switch (ftpconn->cmdstate)
   {
   case FCC_SENDPORT:
   case FCC_RECVPORT:
   case FCC_NLSTPORT:
      if (replycode < 200 || replycode > 299)   /* OK code? */
      {
         fc_printf(ftpconn, "ftp port command error %s\n", ftpconn->cmdbuf);
         FC_SETLOGSTATE(ftpconn,FCL_LOGGEDIN); 
         break;   /* this is not fatal to command connection */
      }
      ftpconn->cmdstate++; /* bump to actual IO command */
      FC_CALLBACK(ftpconn);
      e = fc_usercmd(ftpconn, ftpconn->cmdstate, NULL);  /* send cmd */
      break;
   case FCC_SENDOK:
   case FCC_RECVOK:
   case FCC_NLSTOK:
      if ((replycode == 150) || (replycode == 125))
      {
         ftpconn->cmdstate++; /* next state is handle actual connect */
         FC_CALLBACK(ftpconn);
      }
      else  /* not a 150 code, clean up data conn */
      {
         fc_endxfer(ftpconn); /* kill data connection */
         FC_SETLOGSTATE(ftpconn,FCL_LOGGEDIN);  /* back to base state */
         e = 0;   /* don't kill command connection */
      }
      break;
   case FCC_RECVDONE:   /* closing receive data connection */
   case FCC_NLSTDONE:
      fc_endxfer(ftpconn); /* cleanup resources */
      /* fall to logstate logic */
   case FCC_SENDDONE:   /* done with file send, awaiting reply */
      FC_SETLOGSTATE(ftpconn,FCL_LOGGEDIN);  /* back to base state */
      break;
   case FCC_TYPE:
      if (replycode == 200)
         ftpconn->mode = ftpconn->newmode;
      else
         fc_printf(ftpconn, "type cmd failed\n");
      FC_SETLOGSTATE(ftpconn,FCL_LOGGEDIN);  /* go back to idle state */
      break;
   case FCC_CWD:  /* get reply to Change Working Directory cmd */
   case FCC_PWD:  /* get reply to Print Working Directory cmd */
      if (replycode < 200 || replycode > 299)   /* check reply code */
         fc_printf(ftpconn, "path error: %s", &ftpconn->cmdbuf[5]);
      else     /* good reply, display to user */
         fc_printf(ftpconn, "ftp: %s", &ftpconn->cmdbuf[4]);
      FC_SETLOGSTATE(ftpconn,FCL_LOGGEDIN);  /* go back to idle state */
      break;
   case FCC_QUIT: /* get reply to QUIT command */
      if (replycode < 200 || replycode > 299)   /* check reply code */
         fc_printf(ftpconn, "quit error: %s", &ftpconn->cmdbuf[5]);
      else     /* good reply, display to user */
         fc_printf(ftpconn, "ftp: %s", &ftpconn->cmdbuf[4]);
      FC_SETLOGSTATE(ftpconn,FCL_CLOSING);   /* go ahead and close conn */
      break;
      default:
      dtrap(); /* bad case? */
      e = -1;
   }

   /* Flush current command from cmdbuf */
   cp = strstr(ftpconn->cmdbuf, "\r\n");    /* find end of command */
   if((cp == NULL) || (*(cp + 2) == 0))
   {
      ftpconn->cmdbuf[0] = 0;    /* invalidate whole buffer */
   }
   else  /* there was another command in the buffer */
   {
      cp += 2;    /* point to next cmd (past CR/LF) */
      MEMMOVE(ftpconn->cmdbuf, cp, strlen(cp) + 1);   /* move up next cmd */
   }
   return e;
}

/* FUNCTION: fc_showspeed()
 * 
 * show speed parameters of current transfer. 
 *
 * PARAM1: ftpc * ftpconn
 *
 * RETURNS: 
 */

void
fc_showspeed(ftpc * ftpconn)
{
   unsigned secs;
   u_long   ticks;
   char *   oper;

   switch (ftpconn->cmdstate)    /* set operation string */
   {
      case FCC_SENDING:
      case FCC_SENDDONE:
         oper = "Sent";     
         break;
      case FCC_RECEIVING:
      case FCC_RECVDONE:
         oper = "Received"; 
         break;
      case FCC_NLSTING:
      case FCC_NLSTDONE:
         oper = "Listed";   
         break;
      default:
         return;
   }

   ticks = ftp_ticks - ftpconn->last_data;
   secs = (unsigned)(ticks/TPS);
   fc_printf(ftpconn, "%s %lu bytes in %d seconds (cticks = %lu)\n",
      oper, ftpconn->datadone, secs, cticks);
   /* show speed if samples are big enough */
   if (secs > 0 && ftpconn->datadone > 1024)
   {
      fc_printf(ftpconn, "speed: %ld.%ld KB/s\n", 
         (ftpconn->datadone/secs)/1024L,
         (ftpconn->datadone/secs)%1024L );
   }
}

/* FUNCTION: fc_endxfer()
 * 
 * fc_endxfer(ftpconn) - housekepping routine to clean up after a 
 * data transfer. 
 *
 * PARAM1: ftpc * ftpconn
 *
 * RETURNS: 
 */

void
fc_endxfer(ftpc * ftpconn)
{
   /* close file if open */
   if (ftpconn->fp)
      vfclose(ftpconn->fp);
   ftpconn->fp = NULL;

   /* close data socket if open */
   if (ftpconn->data_sock && ftpconn->data_sock != SYS_SOCKETNULL)
      t_socketclose(ftpconn->data_sock);
   ftpconn->data_sock = SYS_SOCKETNULL;

   /* printf messages as required */
   if (ftpconn->options & FCO_VERB)
   {
      fc_showspeed(ftpconn);
   }

   FC_SETLOGSTATE(ftpconn,FCL_LOGGEDIN); 
   /* reset data transfer state variables */
   fc_clearf(ftpconn);
}



/* FUNCTION: fc_clearf()
 * 
 * fc_clearf(ftpconn) - housekepping routine to set all file transfer 
 * counters to zero. 
 *
 * PARAM1: ftpc * ftpconn
 *
 * RETURNS: 
 */

void
fc_clearf(ftpc * ftpconn)
{
   /* clean up ftpconn counters, etc. */
   ftpconn->datact = 0; /* clear count */
   ftpconn->offset = 0; /* clear offset */
   ftpconn->datadone = 0L;
   ftpconn->filesize = 0L;
   ftpconn->last_cmd = ftp_ticks; /* update last_cmd */
}



/* FUNCTION: fc_sendmore()
 * 
 * fc_XXXXmore() - called to drive file transfers. 
 *
 * PARAM1: ftpc * ftpconn
 *
 * RETURNS: Both return 0 if OK, if error returns -1. 
 */

int
fc_sendmore(ftpc * ftpconn)
{
   int   e;

   /* see if we need to read in buffer */
   if (ftpconn->datact == 0)
   {
      ftpconn->offset = 0; /* reset databuf index */
      ftpconn->datact = vfread(ftpconn->databuf, 1, FILEBUFSIZE, ftpconn->fp);
      if (ftpconn->datact <= 0)  /* end of file? */
      {
         ftpconn->cmdstate = FCC_SENDDONE;   /* move to await reply? */
         FC_CALLBACK(ftpconn);
         fc_endxfer(ftpconn);    /* close data connection , etc. */
         /* At this point we expect to receive the cmd "226 Transfer 
          * complete at connection for <filename>" from the peer. 
          * Hence the logstate should be FCL_PENDING. 
          */
         FC_SETLOGSTATE(ftpconn,FCL_PENDING); 
         return 0;
      }
   }
   e = t_send(ftpconn->data_sock, &ftpconn->databuf[ftpconn->offset],
    ftpconn->datact, 0);
   if (e <= 0)
   {
      e = t_errno(ftpconn->data_sock);
      if((e == EWOULDBLOCK) ||
         (e == ENOBUFS))
      {
         return 0;   /* Transient error, try again later */
      }
      else
      {
         fc_printf(ftpconn, "ftp data send error %d\n", e);
         return -1;
      }
   }

   /* fall to here if we sent data. Consider this command activity */
   ftpconn->last_cmd = ftp_ticks;

   /* do hash mark printfing */
   if (ftpconn->options & FCO_HASH)
      fc_hashmark(ftpconn, ftpconn->datadone, (unsigned)e);

   ftpconn->offset += e;
   ftpconn->datadone += (long)e;
   ftpconn->datact -= e;
   return 0;   
}

#define  FTPC_MAX_PRINTF_LEN  80



/* FUNCTION: fc_getmore()
 * 
 * PARAM1: ftpc * ftpconn
 *
 * RETURNS: 
 */

int
fc_getmore(ftpc * ftpconn)
{
   int   e;
   int   read_size;

   /* we will usually try to read a whole buffer full */
   read_size = FILEBUFSIZE;
   /* but if its for a list command, we read 1 byte less so that we 
    * got room to null terminate what we've read so we can pass it to 
    * fc_printf()
    */
   if (ftpconn->cmdstate == FCC_NLSTING)
      read_size--;

   e = t_recv(ftpconn->data_sock, ftpconn->databuf, read_size, 0);
   if (e < 0)  /* socket error */
   {
      e = t_errno(ftpconn->data_sock);
      if (e == EWOULDBLOCK)
         return 0;   /* no data ready try again later */
      fc_printf(ftpconn, "ftp data socket error %d\n", e);
      return -1;
   }
   else if (e == 0)   /* server closed connection */
   {
      /* this is a normal transfer termination */
      if (ftpconn->options & FCO_VERB)
         fc_printf(ftpconn, "ftp server closed data connection\n");

#if 0
      /* close data socket if open */
      if (ftpconn->data_sock && ftpconn->data_sock != SYS_SOCKETNULL)
         t_socketclose(ftpconn->data_sock);
      ftpconn->data_sock = SYS_SOCKETNULL;
#endif

      ftpconn->cmdstate++; /* move to shutdown state */
      FC_CALLBACK(ftpconn);
      return 0;
   }
   else  /* e is positive byte count */
   {
      /* got data - consider this a command activity */
      ftpconn->last_cmd = ftp_ticks;
      ftpconn->offset = 0;       /* update data pointer & count */
      ftpconn->datact = e;
   }

   if (ftpconn->cmdstate == FCC_NLSTING)
   {
      /* our homegrown version of printf() limits field sizes to 
       * something like 132 bytes. this loop feeds the LIST output 
       * to printf() 80 chars at a time so as not to exceed this limit
       */
      char *   cp;
      char  c;
      int   len;

      /* null terminate the string so printf will handle it right */
      ftpconn->databuf[e] = 0;
      cp = ftpconn->databuf;
      len = e;

      /* while there's more than 80 chars left to printf */
      while (len > FTPC_MAX_PRINTF_LEN)
      {
         /* save the 80th char in so we can restore it below */
         c = *(cp + FTPC_MAX_PRINTF_LEN);
         /* overwrite the 80th char with a null so printf will do
            the right thing */
         *(cp + FTPC_MAX_PRINTF_LEN) = 0;

         /* print the 80 bytes */
         fc_printf(ftpconn, "%s", cp);

         /* put the overwritten byte back */
         *(cp + FTPC_MAX_PRINTF_LEN) = c;
         len -= FTPC_MAX_PRINTF_LEN;
         cp += FTPC_MAX_PRINTF_LEN;
      }

      fc_printf(ftpconn, "%s", cp);
      ftpconn->datadone += ftpconn->datact;
      return 0;
   }

   e = vfwrite(ftpconn->databuf, 1, ftpconn->datact, ftpconn->fp);
   if (e != (int)ftpconn->datact)
   {
      fc_printf(ftpconn, "ftp file write error\n");
      return -1;
   }
   /* do hash mark printfing */
   if (ftpconn->options & FCO_HASH)
      fc_hashmark(ftpconn, ftpconn->datadone, ftpconn->datact);
   ftpconn->datadone += (long)ftpconn->datact;
   ftpconn->datact = 0;;
   return 0;
}




/* FUNCTION: fc_dataconn()
 * 
 * fc_dataconn() - Initiate or check open of a data connection. If 
 * ftpconn->data_sock is nonzero, this creates a socket and starts 
 * the non-blocking connect, else it calls connect to see if 
 * connection has completed. If connection completes 
 * ftpconn->cmdstate is incremented. This handles both active and 
 * passive opens, based on the FCP_PASV bit int ftpconn->options. 
 *
 * PARAM1: ftpc * ftpconn
 *
 * RETURNS: Returns -1 if error detected, 1 if we're now connected, 
 * 0 if still * waiting. 
 */

int
fc_dataconn(ftpc * ftpconn)
{
   int   e;
   SOCKTYPE sock = SYS_SOCKETNULL;
   struct sockaddr   ftpsin;  /* generic, will be cast to v4 or v6 format */
   int      sinsize;          /* size of ftpsin after domain selection */

   switch (ftpconn->domain)
   {
#ifdef IP_V4
      case AF_INET:
      {
         struct sockaddr_in * ftpsin4;
         /* fill in connect parameters for v4 connection */
         sinsize = sizeof(struct sockaddr_in);
         ftpsin4 = (struct sockaddr_in *)&ftpsin;
         ftpsin4->sin_family = AF_INET;
         ftpsin4->sin_addr.s_addr = ftpconn->fhost;
         break;
      }
#endif   /* IP_V4 */
#ifdef IP_V6
      case AF_INET6:
      {
         struct sockaddr_in6 * ftpsin6;
         /* fill in connect parameters for v6 connection */
         sinsize = sizeof(struct sockaddr_in6);
         ftpsin6 = (struct sockaddr_in6 *)&ftpsin;
         ftpsin6->sin6_family = AF_INET;
         IP6CPY(&ftpsin6->sin6_addr, &ftpconn->ip6_fhost);
         break;
      }
#endif   /* IP_V4 */
      default:
         dtrap();    /* no domain setting */
         return -1;
   }

   /* see if there is already a connection in progress */
   if (ftpconn->data_sock && ftpconn->data_sock != SYS_SOCKETNULL)
   {
      /* see if we are listening (passive) or connecting (active) */
      if ((ftpconn->options & FCO_PASV)==0)  /* server not passive, try accept */
      {
         sock = t_accept(ftpconn->data_sock, &ftpsin, &sinsize);
         if (sock == SYS_SOCKETNULL)
         {
            e = t_errno(ftpconn->data_sock);
            if (e == EWOULDBLOCK)
               return 0;   /* normal return for waiting opens */
            fc_printf(ftpconn, "ftp: error %d on listening socket\n", e);
            return -1;
         }
         else  /* accept worked, we are connected */
         {
            t_socketclose(ftpconn->data_sock);  /* close listen socket */
            ftpconn->data_sock = sock; /* install accepted data socket */

            return 1;   /* return "connected" code */
         }
      }
      else  /* server passive, we must poll active connect */
      {
         e = t_connect(sock, &ftpsin, sinsize);
         if (e == 0)    /* connection completed OK */
         {
            ftpconn->cmdstate++; /* bump command to next state */
            FC_CALLBACK(ftpconn);
#ifdef IP_V4
            if(ftpconn->domain == AF_INET)
               ftpconn->dport = htons(((struct sockaddr_in *)(&ftpsin))->sin_port);
#endif   /* IP_V4 */
#ifdef IP_V6
            if(ftpconn->domain == AF_INET6)
               ftpconn->dport = htons(((struct sockaddr_in6*)(&ftpsin))->sin6_port);
#endif   /* IP_V6 */
            return 1;
         }
         e = t_errno(sock);   /* get socket error */
         if (e == EINPROGRESS)   /* still connecting */
            return 0;   /* normal return for PASV SENDCONN & RECVCONN */
         fc_printf(ftpconn, "ftp: error %d open data connection\n", e);
         return -1;
      }
   }

   /* see if we should be listening or connecting */
   if ((ftpconn->options & FCO_PASV)==0)  /* server not passive, do listen */
   {
      /* start TCP listen on data port, passing parms in local endian */

      ftpconn->dport= fc_next_port;    /* Use a unique data port */

      fc_next_port++;
      if ( fc_next_port < FTP_START_DATA_PORT ) /* wraparound occurd ? */
         fc_next_port = FTP_START_DATA_PORT ;

      sock = t_tcplisten((u_short*)&ftpconn->dport, ftpconn->domain);

      if (sock == SYS_SOCKETNULL)
      {
         e = t_errno(ftpconn->data_sock);
         fc_printf(ftpconn, "ftp client data port list failed, error %d\n", e);
         return -1;
      }
   }
   else  /* server is passive, initiate connection */
   {
      sock = t_socket(ftpconn->domain, SOCK_STREAM, 0);
      if (sock == SYS_SOCKETNULL)
         return -1;  /* fatal, kill command connection */

      /* make non-blocking connect call */
      t_setsockopt(sock, SOL_SOCKET, SO_NBIO, NULL, 0);
      e = t_connect(sock, &ftpsin, sinsize);
      if (e != 0)
      {
         e = t_errno(sock);   /* get socket error */
         if (e != EINPROGRESS)   /* still connecting */
         {
            fc_printf(ftpconn, "ftp: error %d open data connection\n", e);
            return -1;
         }
      }
   }
   /* update state variables */
   ftpconn->last_cmd = ftp_ticks;
   ftpconn->last_data = ftp_ticks;
   ftpconn->data_sock = sock;
   ftpconn->cmdstate++;
   FC_CALLBACK(ftpconn);
   return 0;
}




/* FUNCTION: fc_killsess()
 * 
 * fc_killsess() - the "destructor" for an ftp client session struct
 *
 * PARAM1: ftpc * ftpconn
 *
 * RETURNS: 
 */

void
fc_killsess(ftpc * ftpconn)
{
   ftpc * ftpnext;
   ftpc * ftplast;

   if (ftpconn->cmd_sock && ftpconn->cmd_sock != INVALID_SOCKET)
      t_socketclose(ftpconn->cmd_sock);
   ftpconn->cmd_sock = INVALID_SOCKET;
   if (ftpconn->data_sock && ftpconn->data_sock != INVALID_SOCKET)
      t_socketclose(ftpconn->data_sock);
   ftpconn->data_sock = INVALID_SOCKET;
   if (ftpconn->fp)
      vfclose(ftpconn->fp);

   /* search ftp client list for ftpconn so we can unlink it */
   ftpnext = ftp_clients;
   ftplast = NULL;
   while (ftpnext)
   {
      if (ftpnext == ftpconn) /* found connection to delete */
      {
         if (ftplast)   /* not at head of queue */
            ftplast->next = ftpconn->next;   /* unlink */
         else  /* it's head of queue */
            ftp_clients = ftpconn->next;
         break;
      }
      ftplast = ftpnext;
      ftpnext = ftpnext->next;
   }
#ifdef NPDEBUG
   if (!ftpnext)  /* not found in list ? */
   {  dtrap(); }
#endif
   FTPC_FREE(ftpconn);  /* free structure memory */
}



/* FUNCTION: fc_connect()
 *
 * fc_connect() - initiate connect to an FTP server. If user name is 
 * not passed, connection will be made, but no login will occur. If 
 * username in given, password must be passed also if the server will 
 * require it. Note that many BSD derived servers ask for the 
 * password even if the UNIX user does not have one in his passwd 
 * file.
 *
 *    this first parameter is a pointer to a binary IP address. This
 * is 4 bytes long if the domain parameter is AF_INET, and 16 bytes
 * long if AF_INET6.
 *
 * PARAM1: ip_addr fhost   -  ftp server to contact
 * PARAM2: char * user     - user & passwd to log in with
 * PARAM3: char * passwd
 * PARAM4: void * pio
 * PARAM5: int domain      - AF_INET or AF_INET6
 *
 * RETURNS: Returns NULL if error, else an ftpc structure. 
 */

ftpc *
fc_connect(void * ipaddr,
   char *   user,       /* user name */
   char *   passwd,     /* optional password, may be NULL or "" */
   void *   pio,        /* Handle to output device (for messages) */
   int      domain)     /* AF_INET or AF_INET6 */
{
   ftpc * ftpconn;
   int   e;

   /* create connection structure */   
   ftpconn = FTPC_ALLOC(sizeof(ftpc));
   if (ftpconn == NULL)
   {
      fc_printf(NULL, "alloc failed\n");
      return NULL;
   }

   /* fill in connection structure */
   FC_SETLOGSTATE(ftpconn,0); 
   ftpconn->mode = FTPTYPE_ASCII;
   ftpconn->domain = domain;

   /* make a copy of the passed FTP server IP address */
   switch(domain)
   {
#ifdef IP_V4
      case AF_INET:
         /* IPv4 Host is passed in net endian */
         ftpconn->fhost = *(ip_addr *)ipaddr;
         break;
#endif   /* IP_V4 */
#ifdef IP_V6
      case AF_INET6:
         IP6CPY(&ftpconn->ip6_fhost, (ip6_addr *)ipaddr);
         break;
#endif   /* IP_V6 */
      default:
         dtrap();
         return NULL;
   }

   /* if user & pw passed, copy them; else leave as 0s */
   if (user)
      MEMCPY(ftpconn->username, user, FTPMAXUSERNAME);
   if (passwd)
      MEMCPY(ftpconn->password, passwd, FTPMAXUSERPASS);
   ftpconn->next = ftp_clients;     /* link at front of list */
   ftp_clients = ftpconn;

#ifdef NPDEBUG
   ftpconn->options |= FCO_VERB; /* turn on verbose for debug builds */
#endif

   ftpconn->pio = pio;

   /* start the connection request */
   e = fc_connopen(ftpconn);
   if (e)
   {
      fc_printf(NULL, "connect error %d\n", e);
      fc_killsess(ftpconn);
      return NULL;
   }
   SignalFtpClient();       /* wake client maintainance task */
   return ftpconn;
}



/* FUNCTION: fc_get()
 * 
 * fc_get() - Start the RETR of a file from the ftp server to the 
 * local file system. 
 *
 * PARAM1: ftpc * fc
 * PARAM2: char * sname
 * PARAM3: char * dname
 *
 * RETURNS: Returns 0 if OK, else negtive error code 
 */

int 
fc_get(ftpc * fc, char * sname, char * dname)
{
   int   e;
   char *   mode;

   /* make sure session is conencted and ready */
   if (fc->logstate != FCL_LOGGEDIN)
      return -1;

   if (fc->mode == FTPTYPE_ASCII)
      mode = "wt";   /* ANSI translated mode */
   else
      mode = "wb";   /* ANSI binary mode */

   /* open emtpy file for receive */
   fc->fp = vfopen(dname, mode);    /* check mode later */
   if (fc->fp == NULL)
   {
      fc_printf(fc, "Unable to open local file %s\n", dname);
      return -1;
   }

   MEMCPY(fc->ffilename, sname, sizeof(fc->ffilename));
   FC_SETLOGSTATE(fc,FCL_PENDING); 

   if ((fc->options & FCO_PASV)==0) /* server not passive */
   {
      e = fc_dataconn(fc);    /* this will start our listen */
      if (e) return e;
         }

   fc->cmdstate = FCC_RECVPORT;
   FC_SETLOGSTATE(fc,FCL_PENDING);  /* Maybe this is redundant */
   e = fc_sendport(fc);
   return e;
}



/* FUNCTION: fc_put()
 * 
 * fc_put() - Start the STOR of a local file to the ftp server. 
 *
 * PARAM1: ftpc * fc
 * PARAM2: char * sname
 * PARAM3: char * dname
 *
 * RETURNS: Returns 0 if OK, else negtive error code 
 */

int
fc_put(ftpc * fc, char * sname, char * dname)
{
   int   e;
   char *   mode;

   /* make sure session is connected and ready */
   if (fc->logstate != FCL_LOGGEDIN)
      return -1;

   /* open local file for send */
   if (fc->mode == FTPTYPE_ASCII)
      mode = "rt";   /* ANSI translated mode */
   else
      mode = "rb";   /* ANSI binary mode */

   fc->fp = vfopen(sname, mode);
   if (fc->fp == NULL)
   {
      fc_printf(fc, "Unable to open local file %s\n", sname);
      return -1;
   }

   MEMCPY(fc->ffilename, dname, sizeof(fc->ffilename));
   FC_SETLOGSTATE(fc,FCL_PENDING); 

   if ((fc->options & FCO_PASV)==0) /* server not passive */
   {
      e = fc_dataconn(fc);    /* this will start our listen */
      if (e) return e;
   }

   fc->cmdstate = FCC_SENDPORT;
   FC_SETLOGSTATE(fc,FCL_PENDING); 
   e = fc_sendport(fc);
   return e;
}




/* FUNCTION: fc_connopen()
 * 
 * fc_connopen() - TCP active open routine. Provided so FTP client 
 * et.al. can open an active TCP connection without blocking in TCP. 
 * All parameters are passed in network endian. When calling for the 
 * first time, ftpconn->fhost and ftpconn->dport should be set and 
 * ftpconn->cmdstate should be 0. To poll for connection ready, 
 * ftpconn->cmdstate is set to the value returned from the first call 
 * and all other settings are ignored. 
 *
 * PARAM1: ftpc * ftpconn
 *
 * RETURNS: Returns 0 if OK, -1 if error detected. 
 */

int 
fc_connopen(ftpc * ftpconn)
{
   int      e;
   SOCKTYPE sock;
   struct sockaddr ftpsin;    /* cast as sockaddr_in or sockaddr_in6 */
   int      sinsize;          /* size of ftpsin cast */
   int      replycode;

   if (ftpconn->logstate == FCL_CONNECTED)   /* connected but not ready */
   {
      e = fc_getreply(ftpconn, &replycode);
      if (e == -1) return -1; /* return if error */
         if (e == 1) return 0;   /* return if not ready yet */
         /* fall to here if we have an FTP message after socket open */
      if (replycode == 220)   /* got "ready" code? */
      {
         FC_SETLOGSTATE(ftpconn,FCL_READY);  /* ready to send USER cmd */
         if (ftpconn->username[0])
            return(fc_senduser(ftpconn));
         else
            return 0;
      }
      else  /* got code other than "ready" - this is bad. */
      {
         fc_printf(ftpconn, "connect failed; %s\n", ftpconn->cmdbuf);
         return -1;     /* kill off client session */
      }
   }

   switch (ftpconn->domain)
   {
#ifdef IP_V4
      case AF_INET:
      {
         struct sockaddr_in * ftpsin4;
         /* fill in connect parameters for v4 connection */
         sinsize = sizeof(struct sockaddr_in);
         ftpsin4 = (struct sockaddr_in *)&ftpsin;
         ftpsin4->sin_family = AF_INET;
         ftpsin4->sin_addr.s_addr = ftpconn->fhost;
         ftpsin4->sin_port = htons(FTP_PORT);
         break;
      }
#endif   /* IP_V4 */
#ifdef IP_V6
      case AF_INET6:
      {
         struct sockaddr_in6 * ftpsin6;
         /* fill in connect parameters for v6 connection */
         sinsize = sizeof(struct sockaddr_in6);
         ftpsin6 = (struct sockaddr_in6 *)&ftpsin;
         ftpsin6->sin6_family = AF_INET6;
         ftpsin6->sin6_port = htons(FTP_PORT);
         IP6CPY(&ftpsin6->sin6_addr, &ftpconn->ip6_fhost);
         break;
      }
#endif   /* IP_V6 */
      default:
         dtrap();    /* no domain setting */
         return -1;
   }


   /* if this client session already has a socket, see if it's connected */
   if (ftpconn->logstate == FCL_CONNECTING)
   {
      /* poll t_connect() for complete */
      e = t_connect(ftpconn->cmd_sock, &ftpsin, sinsize);
      if (e)
         e = t_errno(ftpconn->cmd_sock);
      if (e == 0 || e == EISCONN)   /* socket connected to server OK */
      {
         FC_SETLOGSTATE(ftpconn,FCL_CONNECTED); 
         return 0;
      }
      /* else handle connect error */
      if (e == EINPROGRESS)   /* connect still in progress? */
         return 0;   /* wait some more */
      fc_printf(ftpconn, "ftp connect error %d\n", e);
      return -1;
   }

   /* fall to here on first connect. Get a new socket: */
   sock = t_socket(ftpconn->domain, SOCK_STREAM, 0);
   if (sock == SYS_SOCKETNULL)
      return -1;

   ftpconn->cmd_sock = sock;

   t_setsockopt(sock, SOL_SOCKET, SO_NBIO, NULL, 0);

   /* make initial non-blocking connect call */
   e = t_connect(sock, &ftpsin, sinsize);
   if (e != 0)
   {
      e = t_errno(sock);
      if (e != EINPROGRESS)   /* still connecting */
      {
         return -1;
      }
   }
   ftpconn->last_cmd = ftp_ticks;
   FC_SETLOGSTATE(ftpconn,FCL_CONNECTING); 
   return 0;
}



/* FUNCTION: fc_senduser()
 * 
 * state drivers - each of these is called from the state machine 
 * switch table to push the connection from one state to another. 
 * 
 *
 * PARAM1: ftpc * ftpconn
 *
 * RETURNS: All return 0 if OK, else non-zero error code 
 */

int
fc_senduser(ftpc * ftpconn)
{
   /* try to log in with username */
   sprintf(ftpconn->cmdbuf, "USER %s\r\n", ftpconn->username);
   FC_SETLOGSTATE(ftpconn,FCL_SENTUSER); 
   return (fc_sendcmd(ftpconn));
}



/* FUNCTION: fc_sendpass()
 * 
 * PARAM1: ftpc * ftpconn
 *
 * RETURNS: 
 */

int
fc_sendpass(ftpc * ftpconn)
{
   sprintf(ftpconn->cmdbuf, "PASS %s\r\n", ftpconn->password);
   FC_SETLOGSTATE(ftpconn,FCL_SENTPASS); 
   return (fc_sendcmd(ftpconn));
}



/* FUNCTION: fc_sendport()
 * 
 * PARAM1: ftpc * ftpconn
 *
 * RETURNS: 
 */

int
fc_sendport(ftpc * ftpconn)
{

#ifdef IP_V4
   if(ftpconn->domain == AF_INET)
   {
      ulong lp;
      unshort  sp;

      /* get my IP addr for this connection, set to local endian */
      lp = htonl(ip_mymach(ftpconn->fhost));
      sp = (unshort)ftpconn->dport;

      /* format the port command from the fhost and dport info */
      sprintf(ftpconn->cmdbuf, "PORT %u,%u,%u,%u,%u,%u\r\n",
         (unsigned)(lp >> 24), (unsigned)(lp >> 16)&0xff, 
         (unsigned)(lp >> 8)&0xff, (unsigned)(lp&0xff), 
         (unsigned)(sp >> 8), (unsigned)(sp & 0xff));
   }
#endif   /* IPV4 */
#ifdef IP_V6
   if(ftpconn->domain == AF_INET6)
   {
      struct ip6_inaddr * myaddr;
      char addrbuf[46];

      /* get my IP addr for this connection & format into text */
      myaddr = ip6_myaddr(&ftpconn->ip6_fhost, NULL);
      if(myaddr == NULL)
      {
         dtrap();
         return ENP_LOGIC;
      }
      inet_ntop(AF_INET6, &myaddr->addr, addrbuf, sizeof(addrbuf) );

      /* format the port command from the fhost and dport info */
      sprintf(ftpconn->cmdbuf, "EPRT |2|%s|%u|\r\n", addrbuf, ftpconn->dport);
   }
#endif   /* IP_V6 */

   fc_clearf(ftpconn);  /* clear file xfer counters */
   return (fc_sendcmd(ftpconn));
}




/* FUNCTION: fc_sendcmd()
 * 
 * fc_sendcmd(ftpc ftpconn) - send the command in the 
 * ftpconn->cmdbuf. This is suitable for calling from the state 
 * driver switch statement in fc_check() 
 *
 * PARAM1: ftpc * ftpconn
 *
 * RETURNS: Returns 0 if OK, else non-zero error code 
 */

int
fc_sendcmd(ftpc * ftpconn)
{
   int   e;
   int   send_len;

   /*
    * Altera Niche Stack Nios port modification:
    * print cmdstate as %d rather than %s to remove build warning.
    */
   if(ftpconn->cmd_sock == INVALID_SOCKET)
   {
      dprintf("fc_sendcmd: bad socket; logstate: %d, cmdstate:%d \n",
            ftpconn->logstate, ftpconn->cmdstate);
      dtrap();
      return -1;
   }

   /* do the printf first, else we run into state machine problems 
   on speedy systems if command finishes while we're printfing */
   if (ftpconn->options & FCO_VERB)
      fc_printf(ftpconn, "ftp send: %s", ftpconn->cmdbuf);

   send_len = strlen(ftpconn->cmdbuf);
   /* these are short commands and should send all at once: */
   e = t_send(ftpconn->cmd_sock, ftpconn->cmdbuf, send_len, 0);
   if (e < 0)
   {
      e = t_errno(ftpconn->cmd_sock);
      fc_printf(ftpconn, "send error %d on cmd %s", e, ftpconn->cmdbuf);
      return -1;
   }
   /* verify that the whole buffer was sent */
   if (e != send_len)
   {
      fc_printf(ftpconn,"partial send %d bytes on cmd %s",e,ftpconn->cmdbuf);
      return -1;
   }
   ftpconn->last_cmd = ftp_ticks;
   ftpconn->cmdbuf[0] = 0;    /* clear command buffer for reply */
   return 0;
}



/* FUNCTION: fc_dir()
 * 
 * fc_dir() - start the ftp command to do a DIR ("ls" for you UNIX 
 * folks) of the current directory of the server. Problems or results 
 * are reported via fc_printf(). 
 *
 * PARAM1: ftpc * ftpconn
 *
 * RETURNS: Returns 0 if OK, else ENP_ error code. 
 */

int
fc_dir(ftpc * ftpconn)
{
   int   e;

   if ((e = fc_ready(ftpconn)) != 0)
      return e;
   if ((ftpconn->options & FCO_PASV)==0)  /* server not passive */
   {
      e = fc_dataconn(ftpconn);     /* this will start our listen */
      if (e)   /* if error, kill session and exit */
      {
         fc_killsess(ftpconn);
         return e;
      }
   }
   ftpconn->cmdstate = FCC_NLSTPORT;   /* command state: sending list port */
   FC_SETLOGSTATE(ftpconn,FCL_PENDING);   /* login state: cmd in progress */
   e = fc_sendport(ftpconn);  /* kick off the port command */
   return e;
}



/* FUNCTION: fc_pwd()
 * 
 * fc_pwd() - start the ftp command to return the Current Working 
 * Directory. Problems or results are reported via fc_printf(). 
 * 
 * PARAM1: ftpc * ftpconn
 *
 * RETURNS: Returns 0 if OK, else ENP_ error code. 
 */

int
fc_pwd(ftpc * ftpconn)
{
   int   e;

   if ((e = fc_ready(ftpconn)) != 0)
      return e;
   return(fc_usercmd(ftpconn, FCC_PWD, NULL));
}



/* FUNCTION: fc_chdir()
 * 
 * fc_chdir() - start the ftp command to return the Current Working 
 * Directory. Problems or results are reported via fc_printf(). 
 * 
 * PARAM1: ftpc * ftpconn
 * PARAM2: char * dirparm
 *
 * RETURNS: Returns 0 if OK, else ENP_ error code. 
 */

int
fc_chdir(ftpc * ftpconn, char * dirparm)
{
   int   e;

   if ((e = fc_ready(ftpconn)) != 0)
      return e;
   return(fc_usercmd(ftpconn, FCC_CWD, dirparm));
}



/* FUNCTION: fc_settype()
 * 
 * fc_chdir() - start the ftp command to set the data type Problems 
 * or results are reported via fc_printf(). 
 *
 * PARAM1: ftpc * ftpconn
 * PARAM2: int typecode
 *
 * RETURNS: Returns 0 if OK, else ENP_ error code. 
 */

int
fc_settype(ftpc * ftpconn, int typecode)
{
   char *   typestring;
   int   e;

   if ((e = fc_ready(ftpconn)) != 0)
      return e;

   if (typecode == FTPTYPE_ASCII)
      typestring = "A";
   else if(typecode == FTPTYPE_IMAGE)
      typestring = "I";
   else
      return ENP_PARAM; /* unsupported typecode */

   ftpconn->newmode = typecode;  /* save type in case command works */
   return(fc_usercmd(ftpconn, FCC_TYPE, typestring)); /* roll the bones */
}



/* FUNCTION: fc_quit()
 * 
 * fc_quit() - start the ftp command to end the ftp connection.
 * Problems or results are reported via fc_printf(). 
 * 
 * PARAM1: ftpc * ftpconn
 *
 * RETURNS: Returns 0 if OK, else ENP_ error code. 
 */

int
fc_quit(ftpc * ftpconn)
{
   int   e;

   if(ftpconn == NULL)
   {
      fc_printf(ftpconn, "ftp client not open\n");
      return ENP_PARAM;
   }

   if(ftpconn->logstate != FCL_LOGGEDIN)
   {
      /* not logged in yet, just close the cmd socket */
      if (ftpconn->cmd_sock && ftpconn->cmd_sock != INVALID_SOCKET)
         t_socketclose(ftpconn->cmd_sock);
      ftpconn->cmd_sock = INVALID_SOCKET;
   }
   if ((e = fc_ready(ftpconn)) != 0)
      return e;
   return(fc_usercmd(ftpconn, FCC_QUIT, NULL));
}



/* FUNCTION: fc_ready()
 * 
 * fc_ready() - Check to see if the passed session is ready to 
 * initiate a new command. 
 *
 * PARAM1: ftpc * ftpconn
 *
 * RETURNS: Returns 0 if ready, else ENP_ error code. 
 */

int
fc_ready(ftpc * ftpconn)
{
   if(ftpconn == NULL)
   {
      fc_printf(ftpconn, "ftp client not open\n");
      return ENP_PARAM;
   }

   if (ftpconn->logstate != FCL_LOGGEDIN)
   {
      if (ftpconn->logstate == FCL_PENDING)
         fc_printf(ftpconn, "ftp client is busy, state %d\n", ftpconn->logstate);
      else
         fc_printf(ftpconn, "ftp client not logged in\n");
      return ENP_BAD_STATE;
   }
   return 0;   /* ready for new command */
}



/* FUNCTION: fc_usercmd()
 * 
 * fc_usertcmd() - The guts of several simple ftp client commands. 
 * Formats & send a command based on the passed code and an optional 
 * text parameter. Problems or results are reported via fc_printf(). 
 * 
 * PARAM1: ftpc * ftpconn
 * PARAM2: int cmdcode
 * PARAM3: char * cmdarg
 *
 * RETURNS: Returns 0 if OK, else ENP_ error code. 
 */

int
fc_usercmd(ftpc * ftpconn, int cmdcode, char * cmdarg)
{

   ftpconn->cmdstate = cmdcode;     /* set command state */
   FC_SETLOGSTATE(ftpconn,FCL_PENDING);   /* mark login state as busy */
   switch (cmdcode)
   {
   case FCC_PWD:
      strcpy(ftpconn->cmdbuf, "XPWD\r\n");
      break;
   case FCC_CWD:
      sprintf(ftpconn->cmdbuf, "CWD %s\r\n", cmdarg);
      break;
   case FCC_TYPE:
      sprintf(ftpconn->cmdbuf, "TYPE %s\r\n", cmdarg);
      break;
   case FCC_NLSTOK:
      sprintf(ftpconn->cmdbuf, "NLST\r\n");
      break;
   case FCC_RECVOK:
      sprintf(ftpconn->cmdbuf, "RETR %s\r\n", ftpconn->ffilename);
      break;
   case FCC_SENDOK:
      sprintf(ftpconn->cmdbuf, "STOR %s\r\n", ftpconn->ffilename);
      break;
   case FCC_QUIT:
      sprintf(ftpconn->cmdbuf, "QUIT\r\n");
      break;
      default:    /* bad parameter */
      dtrap();
      return ENP_PARAM;
   }
   return(fc_sendcmd(ftpconn));     /* send to server */
}

unsigned marksize = 4096;  /* units for hash marking */


/* FUNCTION: fc_hashmark()
 * 
 * fc_hashmark() - called after each sucessfull network data block 
 * transfer to handle the printing of hashmarks. 
 * 
 * PARAM1: ftpc * ftpconn
 * PARAM2: ulong before
 * PARAM3: unsigned added
 *
 * RETURNS: 
 */

void
fc_hashmark(ftpc * ftpconn, 
   ulong before,     /* data already moved before this transfer */
   unsigned added)   /* additional data moved in this transfer */
{
   /* print a hash mark for each "marksize" boundary */
   while (before/marksize < (before+added)/marksize)
   {
      fc_printf(ftpconn, "#");   /* call per-port printf */
      if (added > marksize)   /* don't let "added" wrap */
         added -= marksize;
      else
         added = 0;
   }
}



/* fc_state() - display state info for passed connection */

#ifdef NET_STATS

char *   fc_statestr[]={
   "started connect",
   "connected but no \"220 ready\" msg",
   "ready, but not logged in",
   "sent user, waiting reply",
   "user OK, sent password",
   "cmd port open, no activity",
   "command in progress",
   "closing command connection",
};



/* FUNCTION: fc_state()
 * 
 * PARAM1: ftpc * fc
 *
 * RETURNS: 
 */

int
fc_state(ftpc * fc)
{
   ns_printf(fc->pio,"state: %s, cmdstate: %d, mode:%s idle:%ld\n", 
      fc_statestr[fc->logstate-1],
      fc->cmdstate,
      fc->mode == FTPTYPE_ASCII?"ascii":"binary", 
      (ftp_ticks - fc->last_cmd)/TPS);
   ns_printf(fc->pio,"server: %u.%u.%u.%u, data port:%d\n", 
      PUSH_IPADDR(fc->fhost), fc->dport );
   ns_printf(fc->pio,"Hashing: %s, passive: %s\n", 
      (fc->options & FCO_HASH)?"ON":"OFF",
      (fc->options & FCO_PASV)?"enabled":"off");

   /* printf what's left of the last command. We do this by starting 
    * with the second char, sice we may have nulled-out the first to 
    * invalidate the buffer.
    */
   if(fc->cmdbuf[1])    /* don't bother if no text is set */
      ns_printf(fc->pio, "last msg: %s", &fc->cmdbuf[3]);
   /* if command in progess, show speed */
   if(fc->logstate == 7)
      fc_showspeed(fc);
   return 0;
}
#endif   /* NET_STATS */



/* FUNCTION: fc_pasv()
 * 
 * fc_pasv(ftpc ftpconn) - Start attempt to set FTP command session 
 * to passive mode. This will cause the server to wait for client 
 * (that's us) to initiate data transfers. This is usefull if we are 
 * behind a firewall. Problems or results are reported via 
 * fc_printf(). 
 *
 * PARAM1: ftpc * ftpconn
 *
 * RETURNS: Returns 0 if OK, else ENP_ error code. 
 */

int
fc_pasv(ftpc * ftpconn)
{
   int   e;

   if ((e = fc_ready(ftpconn)) != 0)
      return e;

   if (ftpconn->options & FCO_PASV) /* already passive? */
      return ENP_LOGIC;

   /* send PASV command to server */
   ftpconn->cmdstate = FCC_PASV;    /* command state: sending pasv */
   FC_SETLOGSTATE(ftpconn,FCL_LOGGEDIN);  /* login state: cmd in progress */

#ifdef INWORKS
   !!!!!!!!!

   /* post-pasv operations */

   ftpconn->options |= FCO_PASV;    /* set connection bit */

#endif


   return 0;
}


#ifdef OS_PREEMPTIVE
/* FUNCTION: ftpc_process_rcvd_msgs ()
 *
 * It processes messages received from other tasks (such as console 
 * task, Telnet server task, and timer task).  These messages provide 
 * configuration parameters, initiate (or terminate) FTP transfer 
 * requests, and provide periodic timeout notification.  This function 
 * is invoked after the FTP client task returns from its wait for the 
 * FTP client semaphore.
 *
 * This function is only used in multitasking environments.
 *
 * INPUT: None.
 *
 * OUTPUT: None.
 */

void ftpc_process_rcvd_msgs (void)
{
   struct ftpctask_msg * msgp;

   while (ftpcq.q_len > 0)
   {
      LOCK_NET_RESOURCE (FTPCQ_RESID);
      msgp = getq (&ftpcq);
      UNLOCK_NET_RESOURCE (FTPCQ_RESID);
   
      if (!msgp) 
      {
         ++ftpc_err.empty_q;
         return;
      }

      switch (msgp->type)
      {
         case FTPC_CNTRL_TRANSFER_MODE:
         {
            u_long transfer_mode;

            ++ftpc_msg_stats.transfer_mode;
            /* extract new transfer mode from rcvd message */
            transfer_mode = *((u_long *)(&(msgp->parms [0])));
            if (transfer_mode == FTPTYPE_ASCII)        
               ftpc_process_asc_mode ((void *) (msgp->pio));
            else
               ftpc_process_bin_mode ((void *) (msgp->pio));
            break;
         }
         case FTPC_CNTRL_CD:
            ++ftpc_msg_stats.cd;
            ftpc_process_chdir (((void *) (msgp->pio)), (char *) &(msgp->parms [0]));
            break;
         case FTPC_CNTRL_QUIT_FTP_SESS:
            ++ftpc_msg_stats.quit_sess;
            ftpc_process_quit_sess ((void *) (msgp->pio));
            break;
         case FTPC_CNTRL_VERBOSE:
            ++ftpc_msg_stats.verbose;
            ftpc_process_verbose ((void *) (msgp->pio));
            break;
         case FTPC_CNTRL_PASV:
            ++ftpc_err.not_implemented;
            break;
         case FTPC_CNTRL_START_SESS:
         {
            u_long domain;
            char * userp;
            char * passwdp;

            ++ftpc_msg_stats.start_sess;
            domain = *((u_long *)((&(msgp->parms [0])) + 16));
            userp = (char *) (((&(msgp->parms [0])) + 16) + sizeof (domain));
            passwdp = userp + strlen (userp) + 1;
            ftpc_process_open (((void *) (msgp->pio)), (&(msgp->parms [0])), domain, userp, passwdp);
            break;
         }
         case FTPC_CNTRL_HASH_PRINT:
            ++ftpc_msg_stats.hash_print;
            ftpc_process_hash ((void *) (msgp->pio));
            break;
         case FTPC_CNTRL_MOVE_FILE:
         {
            u_long transfer_type;
            char * sfilep;
            char * dfilep;

            ++ftpc_msg_stats.move_file;
            transfer_type = *((u_long *)(&(msgp->parms [0])));
            sfilep = (char *) (&(msgp->parms [0]) + sizeof (transfer_type));
            dfilep = sfilep + strlen (sfilep) + 1;
            ftpc_process_move (((void *) (msgp->pio)), transfer_type, sfilep, dfilep);
            break;
         }
         case FTPC_CNTRL_PWD:
            ++ftpc_msg_stats.pwd;
            ftpc_process_pwd ((void *) (msgp->pio));
            break;
         case FTPC_CNTRL_LIST:
            ++ftpc_msg_stats.list;
            ftpc_process_list ((void *) (msgp->pio));
            break;
         case FPTC_CNTRL_PRINT_STATE:
            ++ftpc_msg_stats.print_state;
            ftpc_process_print_state ((void *) (msgp->pio));
            break;
         case FTPC_CNTRL_LOG:
            ++ftpc_err.not_implemented;
            break;
         case FTPC_CNTRL_PERIODIC_TIMER:
            ++ftpc_msg_stats.periodic_timer;
            fc_check ();
            break;
         default:
            /* increment error counter */
            ++ftpc_err.bad_msgtype;
            break;
      }

      /* free the message structure */
      FTPC_FREE (msgp);
   }
}
#endif /* OS_PREEMPTIVE */


/* FUNCTION: ftpc_process_asc_mode ()
 *
 * This function sets the file transfer type for an ongoing FTP session 
 * to ASCII.  it is either invoked directly from the FTP menu functions
 * that process user input (for SUPERLOOP-type systems), or from the 
 * message handler (ftpc_process_rcvd_msgs () (for multitasking 
 * environments)).
 *
 * INPUT: Pointer to generic IO structure where request originated.
 *
 * OUTPUT: -1, if an FTP session doesn't exist for the originating console;
 *         or return code from fc_settype ().
 */

int
ftpc_process_asc_mode(void * pio)
{
   struct ftpc * tmpcon;
   int e;

   FC_MENULOG();
   if ((tmpcon = ftp_get_con (pio)) == NULL)
   {
      ns_printf(pio,"Open FTP session first\n");
      return -1;
   }

   e = fc_settype (tmpcon, FTPTYPE_ASCII);
   if (e)
      ns_printf (pio,"ftp error %d setting type.\n", e);

   return e;
}


/* FUNCTION: ftpc_process_bin_mode ()
 *
 * This function sets the file transfer type for an ongoing FTP session 
 * to image (binary).
 *
 * INPUT: Pointer to generic IO structure where request originated.
 *
 * OUTPUT: -1, if an FTP session doesn't exist for the originating console;
 *         or return code from fc_settype ().
 */

int
ftpc_process_bin_mode(void * pio)
{
   struct ftpc * tmpcon;
   int e;

   FC_MENULOG();
   if ((tmpcon = ftp_get_con (pio)) == NULL)
   {
      ns_printf (pio,"Open FTP session first\n");
      return -1;
   }

   e = fc_settype (tmpcon, FTPTYPE_IMAGE);
   if (e)
      ns_printf (pio,"ftp error %d setting type.\n", e);

   return e;
}


/* FUNCTION: ftpc_process_chdir ()
 *
 * This function requests a change to a new working directory.
 *
 * INPUT: (1) Pointer to generic IO structure where request originated.
 *        (2) Name of new working directory
 *
 * OUTPUT: -1, if an FTP session doesn't exist for the originating console;
 *         or return code from fc_chdir ().
 */

int
ftpc_process_chdir(void * pio, char * dirstr)
{
   struct ftpc * tmpcon;
   
   if ((tmpcon = ftp_get_con (pio)) == NULL)
   {
      ns_printf (pio,"Open FTP session first\n");
      return -1;
   }

   return (fc_chdir (tmpcon, dirstr));
}


/* FUNCTION: ftpc_process_quit_sess ()
 *
 * This function requests a termination of the FTP session.
 *
 * INPUT: Pointer to generic IO structure where request originated.
 *
 * OUTPUT: -1, if an FTP session doesn't exist for the originating console;
 *         or return code from fc_quit ().
 */

int
ftpc_process_quit_sess (void * pio)
{
   struct ftpc * tmpcon;

   FC_MENULOG();
   if ((tmpcon = ftp_get_con (pio)) == NULL)
   {
      ns_printf(pio,"Open FTP session first\n");
      return -1;
   }

   return (fc_quit (tmpcon));
}


/* FUNCTION: ftpc_process_verbose ()
 *
 * This function sets the verbose flag for the FTP session.
 *
 * INPUT: Pointer to generic IO structure where request originated.
 *
 * OUTPUT: -1, if an FTP session doesn't exist for the originating console;
 *         0, otherwise.
 */

int
ftpc_process_verbose(void * pio)
{
   struct ftpc * tmpcon;
   
   FC_MENULOG();
   if ((tmpcon = ftp_get_con (pio)) == NULL)
   {
      ns_printf(pio,"Open FTP session first\n");
      return -1;
   }

   if (tmpcon->options & FCO_VERB)
      tmpcon->options &= ~FCO_VERB;
   else
      tmpcon->options |= FCO_VERB;

   ns_printf(pio,"ftp verbose mode %s\n", 
    (tmpcon->options & FCO_VERB) ? "on":"off");

   return 0;
}


/* FUNCTION: ftpc_process_open ()
 *
 * This function opens a new session to the specified FTP server.
 *
 * INPUT: (1) Pointer to generic IO structure where request originated.
 *        (2) IP address (IPv4 or IPv6 address)
 *        (3) domain (AF_INET or AF_INET6)
 *        (4) username
 *        (5) password
 *
 * OUTPUT: -1, if fc_connect () returns 0; otherwise, 0.
 */

int
ftpc_process_open(void * pio, u_char * srvp, u_long domain, char * user, char * passwd)
{
   /* make the connection; this will add a connection to the list
    * starting from "ftp_clients"
    */
   if (fc_connect (srvp, user, passwd, pio, domain) == NULL)
      return -1;
   else
      return 0;
}


/* FUNCTION: ftpc_process_hash ()
 *
 * This function sets the hash print flag for the FTP session.
 *
 * INPUT: Pointer to generic IO structure where request originated.
 *
 * OUTPUT: -1, if an FTP session doesn't exist for the originating console;
 *         0, otherwise.
 */

int
ftpc_process_hash(void * pio)
{
   struct ftpc * tmpcon;
   
   FC_MENULOG();
   if ((tmpcon = ftp_get_con (pio)) == NULL)
   {
      ns_printf(pio,"Open FTP session first\n");
      return -1;
   }

   if (tmpcon->options & FCO_HASH)
      tmpcon->options &= ~FCO_HASH;
   else
      tmpcon->options |= FCO_HASH;

   ns_printf(pio,"FTP hash mark printing turned %s\n", 
    tmpcon->options & FCO_HASH?"on":"off");
    
   return 0;
}


/* FUNCTION: ftpc_process_move ()
 *
 * This function gets (or puts) a file from (or to) the FTP server.
 *
 * INPUT: (1) Pointer to generic IO structure where request originated.
 *        (2) Direction of transfer (e.g., FTPC_GET_TRANSFER, etc.)
 *        (3) source filename
 *        (4) destination filename
 *
 * OUTPUT: -1, if an FTP session doesn't exist for the originating console;
 *         or, the return code from fc_get () or fc_put ().
 */

int
ftpc_process_move(void * pio, int direction, char * sfile, char * dfile)
{
   struct ftpc * tmpcon;

   if ((tmpcon = ftp_get_con (pio)) == NULL)
   {
      ns_printf (pio,"Open FTP session first\n");
      return -1;
   }

   if (direction == FTPC_GET_TRANSFER) 
      return (fc_get (tmpcon, sfile, dfile));
   else
      return (fc_put (tmpcon, sfile, dfile));
}


/* FUNCTION: ftpc_process_pwd ()
 *
 * This function sends a request to obtain the current working directory.
 *
 * INPUT: Pointer to generic IO structure where request originated.
 *
 * OUTPUT: -1, if an FTP session doesn't exist for the originating console;
 *         or, the return code from fc_pwd ().
 */

int
ftpc_process_pwd (void * pio)
{
   struct ftpc * tmpcon;

   FC_MENULOG();
   if ((tmpcon = ftp_get_con (pio)) == NULL)
   {
      ns_printf(pio,"Open FTP session first\n");
      return -1;
   }

   return (fc_pwd (ftp_get_con(pio)));
}


/* FUNCTION: ftpc_process_list ()
 *
 * This function sends a request to obtain a listing of the current working 
 * directory.
 *
 * INPUT: Pointer to generic IO structure where request originated.
 *
 * OUTPUT: -1, if an FTP session doesn't exist for the originating console;
 *         or, the return code from fc_dir ().
 */

int
ftpc_process_list (void * pio)
{
   struct ftpc * tmpcon;

   FC_MENULOG();
   if ((tmpcon = ftp_get_con (pio)) == NULL)
   {
      ns_printf(pio,"ftp command connection not open\n");
      return 1;
   }

   return (fc_dir (tmpcon));
}


/* FUNCTION: ftpc_process_print_state ()
 *
 * This function prints state information associated with the current FTP
 * session.
 *
 * INPUT: Pointer to generic IO structure where request originated.
 *
 * OUTPUT: 0.
 */

int
ftpc_process_print_state (void * pio)
{
   struct ftpc *  tmpcon   ;

   if ((tmpcon = ftp_get_con(pio)) == NULL)
   {
      ns_printf(pio,"FTP client not open\n");
   }
/*
 * Altera Niche Stack Nios port modification
 * conditionally call fc_state; it may not be available
 */
#ifdef NET_STATS
   else
   {
      fc_state(tmpcon);
   }
#endif

#ifdef FC_LOG
   if (log_flag==TRUE)
   {
#ifdef FC_LOGFILE
      if (ftplog)
      {
         ns_printf(pio,"Logging to file is enabled\n");
      }
      else
#endif   /*FC_LOGFILE */
      ns_printf(pio,"Logging to STDIO is enabled\n");
   }
   else
      ns_printf(pio,"Logging is disabled\n");

#endif   /* FC_LOG */

   return 0;
}

#endif /* FTP_CLIENT */
