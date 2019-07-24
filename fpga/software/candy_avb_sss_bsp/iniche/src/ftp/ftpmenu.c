/*
 * FILENAME: ftpmenu.c
 *
 * Copyright 1997- 2000 By InterNiche Technologies Inc. All rights reserved
 *
 *
 * MODULE: FTPCLIENT
 *
 * ROUTINES: ftp_get_con(), ftp_open(), ftp_get(), ftp_put(), 
 * ROUTINES: ftp_move(), ftp_asc_mode(), ftp_bin_mode(), ftp_hash(), 
 * ROUTINES: ftp_pwd(), ftp_chdir(), ftp_verbose(), ftp_quit(), 
 * ROUTINES: ftp_list(), ftp_state(), ftp_pasv(), fc_file_out(), ftp_log(), 
 *
 * PORTABLE: yes
 */

/* ftpmenu.c DOS demo menu hooks for FTP client. This is the 
 * non-portable portion of the FTP client for the DOS demo package. 
 * 1/12/97 - Created. John Bartas 
 * 1/4/99  - Edited menu text -MPG- 
 */

#include "ftpport.h"    /* TCP/IP, sockets, system info */

#ifdef FTP_CLIENT

#include "ftpsrv.h"
#include "ftpclnt.h"

#include "menu.h"

/* The FTP client's per-port message handler */
extern   void fc_printf(ftpc *, char *, ...);   /* per-port response printer */

int  ftp_open    (void * pio);
int  ftp_move    (void * pio, int direction);
int  ftp_get     (void * pio);
int  ftp_put     (void * pio);
int  ftp_asc_mode(void * pio);
int  ftp_bin_mode(void * pio);
int  ftp_hash    (void * pio);
int  ftp_pwd     (void * pio);
int  ftp_list    (void * pio);
int  ftp_chdir   (void * pio);
int  ftp_verbose (void * pio);
int  ftp_state   (void * pio);
int  ftp_pasv    (void * pio);
int  ftp_quit    (void * pio);
int  ftp_log     (void * pio);

/*
 * Altera Niche Stack Nios port modification:
 * Add braces to remove build warning
 */
struct menu_op ftpmenu[]   =  {
   { "ftpc",  stooges,     "FTP client menu" },
   { "ascii", ftp_asc_mode,"use ASCII transfer mode" },
   { "binary",ftp_bin_mode,"use Binary transfer mode" },
   { "cd",    ftp_chdir,   "change server's directory" },
   { "fquit", ftp_quit,    "quit FTP session" },
   { "fverb", ftp_verbose, "toggle verbose mode" },
   { "fpasv", ftp_pasv,    "set server to passive mode" },
   { "ftp",   ftp_open,    "open an FTP connection" },
   { "hash",  ftp_hash,    "toggle hash mark printing" },
   { "get",   ftp_get,     "GET a file" },
   { "put",   ftp_put,     "PUT a file" },
   { "pwd",   ftp_pwd,     "print working directory" },
   { "ls",    ftp_list,    "list files in server directory" },
#ifdef NET_STATS
   { "fstate",ftp_state,   "display FTP client state" },
#endif
#ifdef FC_LOG
   { "flog",  ftp_log,     "enable/disable logging" },
#endif
   { NULL },
};

#ifdef FC_LOG
#define FC_MENULOG() {                                       \
                        if ((log_flag==TRUE) && (ftplog!=NULL))                  \
                           log_printf(ftplog,"%s%s\n", \
                              prompt,((GEN_IO)pio)->inbuf);  \
                     }
#else
#define  FC_MENULOG() ;
#endif   /* FC_LOG */

static char * ousage = "usage: ftp host username [password]\n";
extern   char *   prompt;  /* system menu prompt */

/* functions that are used to send messages to the FTP client task */
int ftpc_send_msg1 (u_long type, u_long pio);

int ftpc_send_msg2 (u_long type, u_long pio, u_char * argp, u_long arglen);

int ftpc_send_msg4 (u_long type, 
                    u_long pio, 
                    u_char * arg1p, 
                    u_long arg1len,
                    u_char * arg2p, 
                    u_long arg2len,
                    u_char * arg3p, 
                    u_long arg3len);

int ftpc_send_msg5 (u_long type, 
                    u_long pio, 
                    u_char * arg1p,  /* address */
                    u_long arg1len,
                    u_char * arg2p,  /* domain */
                    u_long arg2len,
                    u_char * arg3p,  /* username */
                    u_long arg3len,
                    u_char * arg4p,  /* password */
                    u_long arg4len);


/* FUNCTION: ftp_get_con()
 * 
 * ftp_get_con() - Get the FTP connection for the particular pio 
 *
 * PARAM1: void * pio
 *
 * RETURNS: Return NULL if a corresponding connection is not found. 
 */

struct ftpc *  
ftp_get_con(void * pio)
{
   struct ftpc *  tmpcon   =  NULL  ;

   tmpcon = ftp_clients ;
   while ( tmpcon )
   {
      if (tmpcon->pio == pio )
      {
         return tmpcon;
      }
      else
         tmpcon=tmpcon->next ;
   }

   /* We couldn't find a connection which uses pio */

   return NULL ;
}


/* FUNCTION: ftp_open()
 * 
 * ftp_open() - Menu system function to open a new connection for FTP 
 * commands. If a connection is already open, this function returns an 
 * error to its caller. get host, user, password info from menu system's 
 * command buffer.
 *
 * In multitasking environments, this function sends a message (with the
 * various parameters from the user's request) to the FTP client task.
 * In SUPERLOOP mode, it just directly invokes the function that uses
 * the parameters provided to initiate a connection.
 *
 * PARAM1: void * pio
 *
 * RETURNS: -1 if an error occured; 0, if it initiated the opening of
 * a session.
 */

int
ftp_open(void * pio)
{
   char *   hostname=NULL;
   char *   user;
   char *   passwd;
   char *   msg;
   u_long     domain;        /* AF_INET or AF_INET6 */
   u_char     server[16];    /* dual mode IP address */
   u_long x;

   GEN_IO io= (GEN_IO) pio ;

   FC_MENULOG();
   /* extract args from command buffer */
   if ( io != NULL )
      hostname = nextarg(io->inbuf);
   if (*hostname == 0)
   {
      ns_printf(pio,ousage);
      return -1;
   }
   user = nextarg(hostname);
   *(user-1) = 0; /* null terminate hostname */
   if (*user == 0)
   {
      ns_printf(pio,ousage);
      return -1;
   }
   passwd = nextarg(user);
   if (*passwd != 0) /* if passwrod was specd... */
      *(passwd-1) = 0;  /* ...null terminate user */



#ifdef IP_V6
   /* Code for both dual-mode and  IP_V6 only:
    * If there are colons in the host name, then assume
    * it's IPv6, else set domain for IP_V4
    */
   if(strchr(hostname, ':'))
      domain = AF_INET6;
   else
      domain = AF_INET;

   if(inet_pton(domain, hostname, &server) !=0)
      msg = pton_error;
   else
      msg = NULL;
#else /* IP_V4 only */
   {
      unsigned int my_bits;   /* tmp, for subnet bits */
      msg = parse_ipad((ip_addr*)&server, &my_bits, hostname);
      domain = AF_INET;
   }
#endif /* IP_V4 */

   if (msg)
   {
      ns_printf(pio,"FTP host address error: %s\n", msg);
      return -1;
   }

   /* Supports only one client connection for each PIO */
   if ( ftp_get_con(pio) != NULL )
   {
      ns_printf(pio,"ftp session already open\n");
      return -1;
   }

   x = strlen (passwd) + 1;

#ifdef OS_PREEMPTIVE
   ftpc_send_msg5 (FTPC_CNTRL_START_SESS,
                   (u_long) pio, 
                   server,
                   sizeof (server),
                   (u_char *) &domain,
                   sizeof (domain),
                   (u_char *) user,
                   (strlen (user) + 1),
                   (u_char *) passwd,
                   (strlen (passwd) + 1));
#else
   ftpc_process_open (pio, server, domain, user, passwd); 
#endif                   
                   
   return 0;
}


/* FUNCTION: ftp_get()
 * 
 * PARAM1: void * pio
 *
 * RETURNS: 
 */

int
ftp_get(void * pio)
{
   FC_MENULOG();
   return ftp_move(pio,1);
}


/* FUNCTION: ftp_put()
 * 
 * PARAM1: void * pio
 *
 * RETURNS: 
 */

int
ftp_put(void * pio)
{
   FC_MENULOG();
   return ftp_move(pio,0);
}


/* FUNCTION: ftp_move()
 * 
 * ftp_move() - menu routine to start a file mvoe operation. Passed 
 * flag tells us put(0) or get(1). 
 *
 * In multitasking environments, this function sends a message (with the
 * various parameters from the user's request) to the FTP client task.
 * In SUPERLOOP mode, it just directly invokes the function that uses
 * the parameters provided to start the move.
 *
 * PARAM1: void * pio
 * PARAM2: int direction
 *
 * RETURNS: -1 if an error occured; 0, if it initiated the move request.
 */

int
ftp_move(void * pio, int direction)
{
   char *   sfile=NULL; /* name of source file to put/get */
   char *   dfile;   /* optional name */
   GEN_IO io= (GEN_IO) pio ;
   unsigned long int transfer_type;

   if ( ftp_get_con(pio) == NULL )
   {
      ns_printf(pio,"Open FTP session first\n");
      return -1;
   }

   /* extract args from command buffer */
   if ( io != NULL )
      sfile = nextarg(io->inbuf);   /* name of source file */
   if (!sfile || !*sfile)
   {
      ns_printf(pio,"usage: ftp put|get filename [destname]\n");
      return -1;
   }
   dfile = nextarg(sfile);
   if (dfile && *dfile) /* optional dest name given? */
      *(dfile-1) = 0;   /* null terminate source file name */
   else
      dfile = sfile;    /* use foreign name for both */

   if (direction) transfer_type = FTPC_GET_TRANSFER;
   else transfer_type = FTPC_PUT_TRANSFER;
   
#ifdef OS_PREEMPTIVE
   ftpc_send_msg4 (FTPC_CNTRL_MOVE_FILE, 
                   (u_long) pio, 
                   (u_char *) &transfer_type,
                   sizeof (transfer_type),
                   (u_char *) sfile,
                   (strlen (sfile) + 1),
                   (u_char *) dfile,
                   (strlen (dfile) + 1));
#else
   ftpc_process_move (pio, transfer_type, sfile, dfile);
#endif                   

   return 0;
}


/* FUNCTION: ftp_asc_mode()
 * 
 * In multitasking environments, this function sends a message (with the
 * filetype configuration (ASCII) from the user's request) to the FTP 
 * client task.  In SUPERLOOP mode, it just directly invokes the function 
 * that sets up the type of the transfer.
 * 
 * PARAM1: void * pio
 *
 * RETURNS: Return code from ftpc_send_msg2 () for non-SUPERLOOP environments,
 *          or the return code from ftpc_process_asc_mode () for SUPERLOOP
 *          environments.
 */

int
ftp_asc_mode(void * pio)
{
#ifdef OS_PREEMPTIVE
   unsigned long int transfer_mode = FTPTYPE_ASCII;

   return (ftpc_send_msg2 (FTPC_CNTRL_TRANSFER_MODE, (u_long) pio,
                           (u_char *) &transfer_mode, sizeof (transfer_mode)));
#else
   return (ftpc_process_asc_mode (pio));
#endif   
}


/* FUNCTION: ftp_bin_mode()
 * 
 * In multitasking environments, this function sends a message (with the
 * filetype configuration (image) from the user's request) to the FTP 
 * client task.  In SUPERLOOP mode, it just directly invokes the function 
 * that sets up the type of the transfer.
 * 
 * PARAM1: void * pio
 *
 * RETURNS: Return code from ftpc_send_msg2 () for non-SUPERLOOP environments,
 *          or the return code from ftpc_process_asc_mode () for SUPERLOOP
 *          environments.
 */

int
ftp_bin_mode(void * pio)
{
#ifdef OS_PREEMPTIVE
   unsigned long int transfer_mode = FTPTYPE_IMAGE;

   return (ftpc_send_msg2 (FTPC_CNTRL_TRANSFER_MODE, (u_long) pio,
                           (u_char *) &transfer_mode, sizeof (transfer_mode)));
#else
   return (ftpc_process_bin_mode (pio));
#endif   
}


/* FUNCTION: ftp_hash()
 * 
 * In multitasking environments, this function sends a message (requesting
 * that hashes be printed to display the progress of an ongoing file transfer) 
 * to the FTP client task.  In SUPERLOOP mode, it just directly invokes the 
 * function that provides the same request to the FTP client.
 * 
 * PARAM1: void * pio
 *
 * RETURNS: Return code from ftpc_send_msg1 () for non-SUPERLOOP environments,
 *          or the return code from ftpc_process_hash () for SUPERLOOP
 *          environments.
 */

int
ftp_hash(void * pio)
{
#ifdef OS_PREEMPTIVE
   return (ftpc_send_msg1 (FTPC_CNTRL_HASH_PRINT, (u_long) pio));
#else
   return (ftpc_process_hash (pio));
#endif
}


/* FUNCTION: ftp_pwd()
 *
 * In multitasking environments, this function sends a message (requesting
 * the name of the current working directory at the server where the client
 * is logged in) to the FTP client task.  In SUPERLOOP mode, it just directly 
 * invokes the function that provides the same request to the FTP client.
 * 
 * PARAM1: void * pio
 *
 * RETURNS: Return code from ftpc_send_msg1 () for non-SUPERLOOP environments,
 *          or the return code from ftpc_process_pwd () for SUPERLOOP
 *          environments.
 */

int
ftp_pwd(void * pio)
{
#ifdef OS_PREEMPTIVE
   return (ftpc_send_msg1 (FTPC_CNTRL_PWD, (u_long) pio));
#else
   return (ftpc_process_pwd (pio));
#endif
}


/* FUNCTION: ftp_chdir()
 *
 * In multitasking environments, this function sends a message (requesting
 * a change of the current working directory at the server where the client
 * is logged in) to the FTP client task.  In SUPERLOOP mode, it just directly 
 * invokes the function that provides the same request to the FTP client.
 * 
 * PARAM1: void * pio
 *
 * RETURNS: -1 if a path to 'cd' to is not provided; otherwise, the return 
 *          code from ftpc_send_msg2 () for non-SUPERLOOP environments,
 *          or the return code from ftpc_process_chdir () for SUPERLOOP
 *          environments.
 */

int
ftp_chdir(void * pio)
{
   char *   cparm=NULL;
   GEN_IO io= (GEN_IO) pio ;

   FC_MENULOG();
   if ( io != NULL )
      cparm = nextarg(io->inbuf);
   if (!cparm || !*cparm)
   {
      ns_printf(pio,"please specify path arg\n");
      return -1;
   }

#ifdef OS_PREEMPTIVE
   return (ftpc_send_msg2 (FTPC_CNTRL_CD, (u_long) pio, (u_char *) cparm, strlen (cparm) + 1));
#else
   return (ftpc_process_chdir (pio, cparm));
#endif
}


/* FUNCTION: ftp_verbose()
 * 
 * In multitasking environments, this function sends a message (requesting
 * that the client be verbose when displaying the state of an ongoing 
 * transaction) to the FTP client task.  In SUPERLOOP mode, it just directly 
 * invokes the function that provides the same request to the FTP client.
 * 
 * PARAM1: void * pio
 *
 * RETURNS: Return code from ftpc_send_msg1 () for non-SUPERLOOP environments,
 *          or the return code from ftpc_process_verbose () for SUPERLOOP
 *          environments.
 */

int
ftp_verbose(void * pio)
{
#ifdef OS_PREEMPTIVE
   return (ftpc_send_msg1 (FTPC_CNTRL_VERBOSE, (u_long) pio));
#else
   return (ftpc_process_verbose (pio));
#endif
}


/* FUNCTION: ftp_quit()
 * 
 * In multitasking environments, this function sends a message (requesting
 * the termination of an ongoing FTP session) to the FTP client task.  In 
 * SUPERLOOP mode, it just directly invokes the function that provides the 
 * same request to the FTP client.
 * 
 * PARAM1: void * pio
 *
 * RETURNS: Return code from ftpc_send_msg1 () for non-SUPERLOOP environments,
 *          or the return code from ftpc_process_quit_sess () for SUPERLOOP
 *          environments.
 */

int
ftp_quit(void * pio)
{
#ifdef OS_PREEMPTIVE
   return (ftpc_send_msg1 (FTPC_CNTRL_QUIT_FTP_SESS, (u_long) pio));
#else
   return (ftpc_process_quit_sess (pio));
#endif
}


/* FUNCTION: ftp_list()
 *
 * In multitasking environments, this function sends a message (requesting
 * a listing of the files in the current working directory at the server 
 * where the client is logged in) to the FTP client task.  In SUPERLOOP 
 * mode, it just directly invokes the function that provides the same 
 * request to the FTP client.
 * 
 * PARAM1: void * pio
 *
 * RETURNS: Return code from ftpc_send_msg1 () for non-SUPERLOOP environments,
 *          or the return code from ftpc_process_list () for SUPERLOOP
 *          environments.
 */

int
ftp_list(void * pio)
{
#ifdef OS_PREEMPTIVE
   return (ftpc_send_msg1 (FTPC_CNTRL_LIST, (u_long) pio));
#else
   return (ftpc_process_list (pio));
#endif
}

#ifdef NET_STATS


/* FUNCTION: ftp_state()
 *
 * In multitasking environments, this function sends a message (requesting
 * a display of the latest status of the currently open FTP session (if any)) 
 * to the FTP client task.  In SUPERLOOP mode, it just directly invokes the 
 * function that provides the same request to the FTP client.
 * 
 * PARAM1: void * pio
 *
 * RETURNS: Return code from ftpc_send_msg1 () for non-SUPERLOOP environments,
 *          or the return code from ftpc_process_print_state () for SUPERLOOP
 *          environments.
 */

int
ftp_state(void * pio)
{
#ifdef OS_PREEMPTIVE
   return (ftpc_send_msg1 (FPTC_CNTRL_PRINT_STATE, (u_long) pio));
#else
   return (ftpc_process_print_state (pio));
#endif
}
#endif   /* NET_STATS */



/* FUNCTION: ftp_pasv()
 * 
 * PARAM1: void * pio
 *
 * RETURNS: 
 */

int
ftp_pasv(void * pio)
{
   int   e;
   struct ftpc *  tmpcon   ;

   FC_MENULOG();
   if ( (tmpcon=ftp_get_con(pio)) == NULL )
   {
      ns_printf(pio,"FTP client not open\n");
      return 0;
   }

   e = fc_pasv(tmpcon);
   return e;
}

#ifdef FC_LOG
/* Use of ftp_log needs some explanation.
 * Each call to ftp_log toggles logging.  That is,
 *    If logging is enabled, it is disabled.
 *    If logging is disabled, it is enabled.
 * If logging is disabled, and ftp_log is called, then 
 *    If no arguments are passed, logging to stdio is enabled.
 *    Else the argument is treated as filename and logging to file is enabled.
 * If logging is enabled, and ftp_log is called, then the argument are not used.
 * 
 * If FC_LOGFILE is disabled, then "logging to FILE" feature is disabled.
 * 
 * EXAMPLE
 * The flog command from menu prompt will invoke ftp_log().
 * Assume that logging is disabled and FC_LOGFILE is enabled.
 * 
 * flog          - enables logging to stdio
 * flog          - disables logging 
 * flog ftp.log  - enables logging to file ftp.log
 * flog          - disables logging 
 * 
 */
#ifdef FC_LOGFILE
char     fc_buf[FTPMAXFILE];


/* FUNCTION: fc_file_out()
 * 
 * PARAM1: long s
 * PARAM2: char *buf
 * PARAM3: int len
 *
 * RETURNS: 
 */

int fc_file_out(long s, char * buf, int len)
{
   VFILE *fp = (VFILE *)s;

   vfwrite(buf,len,1,fp);
   return len;
}

struct GenericIO  fc_file_io  =  {  fc_buf,  fc_file_out,   0, NULL  }  ;
#endif   /*FC_LOGFILE */



/* FUNCTION: ftp_log()
 * 
 * PARAM1: void * pio
 *
 * RETURNS: 
 */

int
ftp_log(void * pio)
{

   FC_MENULOG();
   if (log_flag==TRUE)
   {
      /* Logging is enabled. Disable it */
      log_flag=FALSE;
#ifdef FC_LOGFILE
      if (ftplog)
      {
         ns_printf(pio, "Logging to file disabled\n");
         vfclose((VFILE *)fc_file_io.id);
      }
      else
#endif   /*FC_LOGFILE */
      ns_printf(pio, "Logging to STDIO disabled\n");
   }
   else
   {
      /* Logging is disabled. Enable it. If an argument is passed, 
       * then it is name of logfile. Hence enable logging to file. If 
       * no argument is passed, then enable logging to stdio. 
       */
#ifdef FC_LOGFILE
      char *   filename=NULL;
      GEN_IO   io= (GEN_IO) pio ;
      VFILE *  fp;
#endif   /*FC_LOGFILE */

      log_flag=TRUE;
      ftplog   =NULL;   /* By default log to stdio */     

#ifdef FC_LOGFILE
      /* extract args from command buffer */
      if ( io != NULL )
      {
         filename = nextarg(io->inbuf);
         if(*filename != 0)   /* user passed file name */
         {
            fp=vfopen(filename, "w");
            if (fp==NULL)
            {
               ns_printf(pio, "Can't open file %s.", filename);
            }
            else
            {
               fc_file_io.id  = (long)fp;
               ftplog   =  &fc_file_io;
            }
         }
      }

      if (ftplog)
         ns_printf(pio, "Logging to file %s enabled\n", filename);
      else
#endif   /*FC_LOGFILE */
      ns_printf(pio, "Logging to STDIO enabled\n");
   }

   return   0;
}
#endif   /* FC_LOG */



#ifdef OS_PREEMPTIVE
/* utility functions to send messages with zero or more parameters to the FTP client task */

/* FUNCTION: ftpc_send_msg1 ()
 *
 * This function sends a zero- or one-parameter message to the FTP client task.  
 * The list of messages that use this function include:
 *
 * FTPC_CNTRL_QUIT_FTP_SESS
 * FTPC_CNTRL_VERBOSE
 * FTPC_CNTRL_HASH_PRINT
 * FTPC_CNTRL_PWD
 * FTPC_CNTRL_LIST
 * FPTC_CNTRL_PRINT_STATE
 * FTPC_CNTRL_PERIODIC_TIMER (zero parameters; 'pio' arg is ignored)
 *
 * INPUT: (1) Type of message to be sent (FTPC_CNTRL_QUIT_FTP_SESS, etc.)
 *        (2) Pointer to generic IO structure from where request originated
 *
 * OUTPUT: -1, if storage for the message could not be allocated; 0, otherwise.
 */

int ftpc_send_msg1 (u_long type, u_long pio)
{
   struct ftpctask_msg * msgp;

   msgp = (struct ftpctask_msg *) FTPC_ALLOC (sizeof (struct ftpctask_msg));
   if (!msgp)
   {
      ++ftpc_err.alloc_fail;
      return -1;
   }
   
   msgp->type = type;
   msgp->pio = pio;

   /* send message to FTP client task */
   LOCK_NET_RESOURCE (FTPCQ_RESID);
   putq(&ftpcq, (q_elt)msgp);
   UNLOCK_NET_RESOURCE (FTPCQ_RESID);

   post_app_sem (FTPC_SEMID);

   return 0;
}


/* FUNCTION: ftpc_send_msg2 ()
 *
 * This function sends a two-parameter message to the FTP client task.  
 * The list of messages that use this function include:
 *
 * FTPC_CNTRL_TRANSFER_MODE
 * FTPC_CNTRL_CD
 *
 * INPUT: (1) Type of message to be sent (FTPC_CNTRL_TRANSFER_MODE, etc.)
 *        (2) Pointer to generic IO structure from where request originated
 *        (3) Pointer to second parameter (in the message for the FTP client task)
 *        (4) Length of second parameter (starting at 'argp')
 *
 * OUTPUT: -1, if storage for the message could not be allocated; 0, otherwise.
 */

int ftpc_send_msg2 (u_long type, u_long pio, u_char * argp, u_long arglen)
{
   struct ftpctask_msg * msgp;

   msgp = (struct ftpctask_msg *) FTPC_ALLOC (sizeof (struct ftpctask_msg) + arglen);
   if (!msgp)
   {
      ++ftpc_err.alloc_fail;
      return -1;
   }
   
   msgp->type = type;
   msgp->pio = pio;
   memcpy (msgp->parms, argp, arglen);

   /* send message to FTP client task */
   LOCK_NET_RESOURCE (FTPCQ_RESID);
   putq(&ftpcq, (q_elt)msgp);
   UNLOCK_NET_RESOURCE (FTPCQ_RESID);

   post_app_sem (FTPC_SEMID);

   return 0;
}


/* FUNCTION: ftpc_send_msg4 ()
 *
 * This function sends a four-parameter message to the FTP client task.  
 * The list of messages that use this function include:
 *
 * FTPC_CNTRL_MOVE_FILE
 *
 * INPUT: (1) Type of message to be sent (FTPC_CNTRL_MOVE_FILE)
 *        (2) Pointer to generic IO structure from where request originated
 *        (3) Pointer to second parameter (in the message for the FTP client task)
 *        (4) Length of second parameter (starting at 'arg1p')
 *        (5) Pointer to third parameter (in the message for the FTP client task)
 *        (6) Length of third parameter (starting at 'arg2p')
 *        (7) Pointer to fourth parameter (in the message for the FTP client task)
 *        (8) Length of fourth parameter (starting at 'arg3p')
 *
 * OUTPUT: -1, if storage for the message could not be allocated; 0, otherwise.
 */

int ftpc_send_msg4 (u_long type, 
                    u_long pio, 
                    u_char * arg1p, 
                    u_long arg1len,
                    u_char * arg2p, 
                    u_long arg2len,
                    u_char * arg3p, 
                    u_long arg3len)
{
   struct ftpctask_msg * msgp;
   unsigned char * startp;

   msgp = (struct ftpctask_msg *) FTPC_ALLOC (sizeof (struct ftpctask_msg) + arg1len + arg2len + arg3len);
   if (!msgp)
   {
      ++ftpc_err.alloc_fail;
      return -1;
   }
   
   msgp->type = type;
   msgp->pio = pio;
   startp = &(msgp->parms[0]);
   memcpy (startp, arg1p, arg1len);
   memcpy (startp + arg1len, arg2p, arg2len);
   memcpy (startp + arg1len + arg2len, arg3p, arg3len);

   /* send message to FTP client task */
   LOCK_NET_RESOURCE (FTPCQ_RESID);
   putq(&ftpcq, (q_elt)msgp);
   UNLOCK_NET_RESOURCE (FTPCQ_RESID);

   post_app_sem (FTPC_SEMID);

   return 0;
}


/* FUNCTION: ftpc_send_msg5 ()
 * 
 * This function is used to send a five-parameter message to the FTP client task.
 *
 * FTPC_CNTRL_START_SESS
 *
 * INPUT: (1) Type of message to be sent (FTPC_CNTRL_MOVE_FILE)
 *        (2) Pointer to generic IO structure from where request originated
 *        (3) Pointer to second parameter (in the message for the FTP client task)
 *        (4) Length of second parameter (starting at 'arg1p')
 *        (5) Pointer to third parameter (in the message for the FTP client task)
 *        (6) Length of third parameter (starting at 'arg2p')
 *        (7) Pointer to fourth parameter (in the message for the FTP client task)
 *        (8) Length of fourth parameter (starting at 'arg3p')
 *        (9) Pointer to fifth parameter (in the message for the FTP client task)
 *        (A) Length of fifth parameter (starting at 'arg4p')
 *
 * OUTPUT: -1, if storage for the message could not be allocated; 0, otherwise.
 */

int ftpc_send_msg5 (u_long type, 
                    u_long pio, 
                    u_char * arg1p,  /* address */
                    u_long arg1len,
                    u_char * arg2p,  /* domain */
                    u_long arg2len,
                    u_char * arg3p,  /* username */
                    u_long arg3len,
                    u_char * arg4p,  /* password */
                    u_long arg4len)
{
   struct ftpctask_msg * msgp;
   unsigned char * startp;

   msgp = (struct ftpctask_msg *) FTPC_ALLOC (sizeof (struct ftpctask_msg) + arg1len + arg2len + arg3len + arg4len);
   if (!msgp)
   {
      ++ftpc_err.alloc_fail;
      return -1;
   }
   
   msgp->type = type;
   msgp->pio = pio;
   startp = &(msgp->parms[0]);
   memcpy (startp, arg1p, arg1len);
   memcpy (startp + arg1len, arg2p, arg2len);
   memcpy (startp + arg1len + arg2len, arg3p, arg3len);
   memcpy (startp + arg1len + arg2len + arg3len, arg4p, arg4len);

   /* send message to FTP client task */
   LOCK_NET_RESOURCE (FTPCQ_RESID);
   putq(&ftpcq, (q_elt)msgp);
   UNLOCK_NET_RESOURCE (FTPCQ_RESID);

   post_app_sem (FTPC_SEMID);

   return 0;
}
#endif /* OS_PREEMPTIVE */


/* FUNCTION: ftpc_periodic_timer ()
 *
 * This function is invoked from timer task (tk_nettick), and is responsible for
 * moving the file transfer to completion.
 *
 * INPUT: None.
 * OUTPUT: None.
 */

void ftpc_periodic_timer (void)
{
#ifdef OS_PREEMPTIVE
   /* the second ('pio') parameter is irrelevant */
   ftpc_send_msg1(FTPC_CNTRL_PERIODIC_TIMER, 0);
#else
   fc_check();
#endif   
}

#endif /* FTP_CLIENT */
