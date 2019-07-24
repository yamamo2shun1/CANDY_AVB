/*
 * FILENAME: FTPCLNT.H
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

/* ftpclnt.h Definitions for generic FTP client. 
 * 1/12/97 - Created. John Bartas 
 */
#ifndef FTPCLNT_H
#define  FTPCLNT_H

/* Define the options enabled for FTP Client */
#define  FC_USECALLBACK  1  /* Call callbacks when FTPClient changes state */
#define  FC_LOG          1  /* Log FTP output to stdio/file */

#ifdef NOTDEF  /* List of options disabled */
#endif   /* NOTDEF */

#ifdef SUPERLOOP
#define SignalFtpClient()  /* define to nothing */
#endif

/* Define the structure used for each FTP connection */
typedef struct ftpc 
{
   struct ftpc *  next; /* list link */
   int   logstate;      /* loggin (FCL_) state */
   int   cmdstate;      /* command (FCC_) state */
   int   mode;          /* FTPTYPE_ASCII or FTPTYPE_IMAGE */
   int   newmode;       /* mode to set if TYPE cmd succeeds */
   int   in_use;        /* reentry flag */
   int   domain;        /* AF_INET or AF_INET6 */
   ip_addr fhost;       /* host, if connected (in net endian) */
#ifdef IP_V6
   ip6_addr ip6_fhost;  /* V6 host, if connected */
#endif   /* IP_V4 */
   unsigned options;    /* hash, pasv, etc */
   unshort     dport;   /* data port */
   SOCKTYPE cmd_sock;   /* command socket descriptor */
   SOCKTYPE data_sock;  /* data socket descriptor */
   char  username[FTPMAXUSERNAME];  /* name of connection user for login */
   char  password[FTPMAXUSERPASS];  /* password for login */
   char  ffilename[FTPMAXPATH+FTPMAXFILE];   /* name for foriegn file */
   char  cmdbuf[CMDBUFSIZE];  /* command buffer */
   char  databuf[FILEBUFSIZE];   /* buffer for FTP data */
   VFILE * fp;    /* file in progress */
   unsigned datact;  /* unprocessed data in databuf */
   unsigned offset;  /* offset to data in databuf */
   ulong    datadone;   /* data already moved in current transfer */
   ulong    filesize;   /* file size of send/receive file */
   ulong    last_cmd;   /* time of last cmdstate change */
   ulong    last_data;  /* time of last data transfer activity */
   void *   pio;     /* To support Generic I/O for client */
} ftpc;

extern   ftpc *   ftp_clients;   /* support multiple client links */

extern   unsigned marksize;   /* units for hash marking */

/* bits for ftpc options field: */
#define  FCO_HASH    0x0001   /* hash mark printing */
#define  FCO_PASV    0x0002   /* in passive mode */
#define  FCO_VERB    0x0004   /* verbose mode */

/* ftp client loggin states; If any change is done, also update fc_str[]. */
#define  FCL_CONNECTING    1  /* started connect */
#define  FCL_CONNECTED     2  /* connected but no "220 ready" msg */
#define  FCL_READY         3  /* ready, but not logged in */
#define  FCL_SENTUSER      4  /* sent user, waiting reply */
#define  FCL_SENTPASS      5  /* user OK, sent password */
#define  FCL_LOGGEDIN      6  /* cmd port open, no activity */
#define  FCL_PENDING       7  /* command in progress (see FCC_* codes) */
#define  FCL_CLOSING       8  /* closing command connection */

/* ftp client command states; grouped by command type
   If any change is done, also update fc_str[]. */
#define  FCC_RECVPORT      10 /* sent port command, awaiting reply */
#define  FCC_RECVOK        11 /* sent RETR command, awaiting reply */
#define  FCC_RECVCONN      12 /* connecting for file receive */
#define  FCC_RECEIVING     13 /* file receive in progress */
#define  FCC_RECVDONE      14 /* done with RETR, awaiting final msg */

#define  FCC_SENDPORT      20 /* sent port command, awaiting reply */
#define  FCC_SENDOK        21 /* sent STOR command, awaiting reply */
#define  FCC_SENDCONN      22 /* connecting for file send */
#define  FCC_SENDING       23 /* data file send in progress */
#define  FCC_SENDDONE      24 /* done with STOR, awaiting final msg */

#define  FCC_NLSTPORT      30 /* sent port for NLST command */
#define  FCC_NLSTOK        31 /* sent NLST for NLST command */
#define  FCC_NLSTCONN      32 /* waiting for connect on data port */
#define  FCC_NLSTING       33 /* waiting for connect on data port */
#define  FCC_NLSTDONE      34 /* done with NLST, awaiting final msg */


/* the non-data port commands: */

#define  FCC_CWD           70 /* sent cwd command, awaiting reply */
#define  FCC_PWD           80 /* sent pwd command, awaiting reply */
#define  FCC_TYPE          90 /* sent TYPE command, awaiting reply */
#define  FCC_PASV       100   /* sent PASV, awaiting reply */
#define  FCC_QUIT       110   /* sent QUIT, awaiting reply (before closing) */

/* Map ftp memory alloc/free routines */
#define  FTPC_ALLOC(size)  (ftpc*)npalloc(size)
#define  FTPC_FREE(buf) npfree(buf)


/* ftp client extern entry points. The commands (all but fc_check) 
 * initiate the transactions and return without blocking. The system 
 * or caller must regularly call fc_check() to drive the transaction. 
 * Completion is detected by checking fc_logstate.
 */
ftpc *   fc_connect(void * fhost, char * user, char * passwd,void * pio, int domain);
int   fc_put(ftpc * fc, char * fname, char * lname);
int   fc_get(ftpc * fc, char * fname, char * lname);
int   fc_chdir(ftpc * ftpconn, char * dirparm);
int   fc_pwd(ftpc * ftpconn);
int   fc_dir(ftpc * ftpconn);
int   fc_settype(ftpc * ftpconn, int type);
int   fc_state(ftpc * ftpconn);
int   fc_pasv(ftpc * ftpconn);
int   fc_quit(ftpc * ftpconn);

int   fc_check(void);   /* ftp client "task" entry point */

/* log_printf is used in ftpclnt.c and ftpcprn.c */
#ifdef FC_LOG
#include "in_utils.h"            /* for GEN_IO */
extern   GEN_IO         ftplog;  /* ftplog is "ptr to IO device" */
#define  log_printf     ns_printf   /* Use genericIO to send log to stdio/file */
extern   int            log_flag;   /* to dynamically enable/disable logging */
#define  FC_LOGFILE     1        /* log to file */

#else
#define  log_printf     ;        /* Do nothing */
#endif   /* FC_LOG */

/* The user application can set a callback via fc_callback to receive 
 * information when the logstate or cmdstate changes. Sometimes, the 
 * callback might just be interested in the states, if it is using 
 * only one FTP connection. In that case, it need not know about 
 * ftpc. Hence the first argument is a "void instead of "ftpc If the 
 * callback needs information about ftp connection, it can cast fc to 
 * "ftpc *"
 */
#ifdef FC_USECALLBACK
extern   void (*fc_callback)(void * fc, int logstate, int cmdstate);
#endif

/* types of messages sent to FTP client task from the console task,
 * Telnet server task, or timer tick task.  These messages are
 * deposited into the 'ftpcq' queue, and provide configuration 
 * parameters, initiate (or terminate) FTP transfer requests, and 
 * provide periodic timeout notification.
 */
#define FTPC_CNTRL_TRANSFER_MODE  0x0 /* ascii or binary */
#define FTPC_CNTRL_CD             0x1
#define FTPC_CNTRL_QUIT_FTP_SESS  0x2
#define FTPC_CNTRL_VERBOSE        0x3
#define FTPC_CNTRL_PASV           0x4
#define FTPC_CNTRL_START_SESS     0x5
#define FTPC_CNTRL_HASH_PRINT     0x6
#define FTPC_CNTRL_MOVE_FILE      0x7 /* get or put */
#define FTPC_CNTRL_PWD            0x8
#define FTPC_CNTRL_LIST           0x9
#define FPTC_CNTRL_PRINT_STATE    0xA
#define FTPC_CNTRL_LOG            0xB
#define FTPC_CNTRL_PERIODIC_TIMER 0xC

#define FTPC_NUM_MSGS             0xD

/* type of FTP transfer requested from client: GET or PUT */
#define FTPC_GET_TRANSFER 0x1 
#define FTPC_PUT_TRANSFER 0x2

/* base structure for all messages sent to FTP client task */
struct ftpctask_msg
{
   struct ftpctask_msg * next;
   u_long type;
   u_long pio;
   u_char parms [1];
};

/* statistics on errors encountered by the FTP client task */
struct ftpc_err
{
   u_long alloc_fail;
   u_long empty_q;
   u_long not_implemented;
   u_long bad_msgtype;
};

/* statistics on the types of messages received by the FTP client task */
struct ftpc_msg_stats
{
   u_long transfer_mode;
   u_long cd;
   u_long quit_sess;
   u_long verbose;
   u_long start_sess;
   u_long hash_print;
   u_long move_file;
   u_long pwd;
   u_long list;
   u_long print_state;
   u_long periodic_timer;
};

/* FTP client task message queue (contains messages from other tasks) */
extern struct queue ftpcq;
/* data structure to keep track of number of various types of messages
 * received by the FTP client task.
 */
extern struct ftpc_msg_stats ftpc_msg_stats;
/* data structure to keep track of errors encountered by the FTP client
 * task during its operation.
 */
extern struct ftpc_err ftpc_err;

void ftpc_process_rcvd_msgs (void);
int ftpc_process_asc_mode(void * pio);
int ftpc_process_bin_mode(void * pio);
int ftpc_process_chdir(void * pio, char * dirstr);
int ftpc_process_quit_sess (void * pio);
int ftpc_process_verbose(void * pio);
int ftpc_process_open(void * pio, u_char * srvp, u_long domain, char * user, char * passwd);
int ftpc_process_hash (void * pio);
int ftpc_process_move (void * pio, int direction, char * sfile, char * dfile);
int ftpc_process_pwd (void * pio);
int ftpc_process_list (void * pio);
int ftpc_process_print_state (void * pio);

#endif   /* FTPCLNT_H */

