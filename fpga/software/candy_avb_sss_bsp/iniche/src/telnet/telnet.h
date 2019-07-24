/*
 * FILENAME: telnet.h
 *
 * Copyright 1998- 2000 By InterNiche Technologies Inc. All rights reserved
 *
 *
 * MODULE: TELNET
 *
 *
 * PORTABLE: yes
 */

/* telnet.h Header file for Telnet Some #defines have been taken from 
 * NCSA Telnet code. 
 */
#ifndef TELNET_H
#define  TELNET_H                1


#include "telport.h"

/* the data member state of TelnetConn can have the following values */
#define  TEL_NORMAL           1  /* connection is normal    */
#define  TEL_PROC_CMD         2  /* processing some cmd     */
#define  TEL_LOGIN            3  /* accepting user login    */
#define  TEL_PASSWORD         4  /* accepting user password */


#define  TEL_RECV_BUF_SIZE    512
#define  TEL_SEND_BUF_SIZE    2048

#define  TELNET_PORT          23
#define  TEL_MAX_NEWLN        20 /* max newln in str for tel_tcp_send()*/

#define  TEL_MAX_LOGIN_TRIES  5
#define  TEL_IDLE_TIME        600 /* time in sec for which conn can be idle */


struct TelGlobals  /* Struct contaning all global params of Telnet */
{
   int tel_recv_buf_size  ; /* TEL_RECV_BUF_SIZE   */
   int tel_send_buf_size  ; /* TEL_SEND_BUF_SIZE   */
   int tel_port           ; /* TELNET_PORT         */
   int tel_max_login_tries; /* TEL_MAX_LOGIN_TRIES */
   int tel_idle_time      ; /* TEL_IDLE_TIME       */
};

extern struct TelGlobals tglob;  

/* TelnetOption is used to store information about options that are 
 * concerned with DO,DONT,WILL,WONT commands A TELNET connection is a 
 * two way communication channel. That means, options must be 
 * negotiated separately for each direction of data flow, if that is 
 * desired. (RFC 856) Hence we have l_ prefix to denote parameters 
 * pertaining to local session and r_ prefix to denote parameters 
 * pertaining to remote session. WILL command is used to negotiate 
 * options for local telnet session. DO command is used to negotiate 
 * options for remote telnet session. Conversely, when we receive a 
 * WILL command, the option is being negotiated for the remote telnet 
 * session. And when we receive a DO command, the option is being 
 * negotiated for the local telnet session. 
 */
struct TelnetOption
{
   int   option   ;     /* Option that we are refering to */
   int   config   ;     /* Whether it is configurable or not (TRUE/FALSE) */
   int   l_value  ;     /* value for local telnet-enabled/disabled (TRUE/FALSE) */
   int   r_value  ;     /* value for remote telnet-enabled/disabled (TRUE/FALSE) */
   int l_negotiate;   /* Do we negotiate for this option when a new
                         telnet connection is started ? (TRUE/FALSE) */
   int r_negotiate;   /* Do we negotiate for this option when a new
                         telnet connection is started ? (TRUE/FALSE) */
   int l_req_sent ;   /* Have we sent a telnet cmd for a change in value
                         of this option ? (TRUE/FALSE) */
   int r_req_sent ;   /* Have we sent a telnet cmd for a change in value
                         of this option ? (TRUE/FALSE) */
};

typedef struct TelnetOption * TEL_OPT;


/* This structure contains a list of all the options that we will be 
 * supporting. There will be a global variable "tel_def_options" 
 * which will contain default values for all these options. A new 
 * telnet session starts with these default values and updates them 
 * based on negotiations. To have automatic telnet processing of an 
 * option, we will have the requirement that all the members of this 
 * strucure are of type "struct TelnetOption". If support for a a new 
 * option is to be added, then the following needs to be done. 
 * 1. Add * it in this structure. 
 * 2. Add its default value in "tel_def_options" 
 * 3. Use its values whereever required. Its values will be 
 *    automatically negotiated at the start of telnet session. 
 *    And they also be dynamically changed if it is negotiated midway 
 *    through the session. If its value needs to be changed midway 
 *    through the session, then code should be added to send a telnet 
 *    command to the peer. 
 */
struct TelOptionList {
     struct TelnetOption  binary;
     struct TelnetOption  echo  ;
     struct TelnetOption  sga   ;
     struct TelnetOption  status;
};



/* Structures to maintain various statistics */

struct TelStatistics       /* statistics for telnet server */
{
   long  conn_opened;   /* Total num of connections opened till date */
   long  conn_closed;   /* Total num of connections closed till date */
};

struct TelSessStats        /* statistics for a telnet session */
{
   u_long   rcvd;       /* total bytes received    */
   u_long   cmds;       /* total commands executed */
};


/* Structure to maintain TCP/IP sockets */

struct TelnetConn
{
   struct TelnetConn *  next;    /* Pointer to the next socket  */
   int            state;         /* state of the protocol.  */
   u_long         curr_tick;     /*   time-tick   of last  data  receive   */
   SOCKTYPE       sock;          /* the open socket we are using  */
   char *         rxbuf;         /* input data from the client */
   char *         txbuf;         /* Output data for the client   */
   char *         cmdbuf;        /* Buffer which contains data from user*/
   int            rxsize;        /* size of valid data in rxbuf  */
   int            txsize;        /* size of unsent data in txbuf   */
   int            cmdsize;       /* size of cmdbuf   */
   int            inserver;      /* telnet server's renetry guard flag  */
   char *         prompt;           /* current prompt for this session  */
   struct TelOptionList *  options; /* Options for a telnet session   */
   struct GenericIO     tel_io;     /* I/O structure used for ns_printf  */
   struct TelSessStats  stats;      /* Statistics for this session  */
#ifdef TEL_USERAUTH
   /* If we support authentication, We just need to remember the name. If session needs
    * password, permissions, etc., it can access the user database
    */
   char   name[MAX_USERLENGTH];
   int                  tries;   /* Num of times user has tried to login*/
#endif

};

typedef struct TelnetConn * TEL_CONN ;



#define  TEL_CMD_LOW             236

/* Telnet Command codes. The description of each of them is in 
 * tel_cmd_str[]. So, SE-TEL_CMD_LOW would give the index into 
 * tel_cmd_str[]. That is tel_cmd_str[SE-TEL_CMD_LOW] is the 
 * description for SE 
 */
/* The following constants would be part of a packet. Packet would be 
 * put in a char[]. If we don't put the (char) before these constant 
 * values, then we face problems with Microsoft compiler in 
 * situations like "if ( == IAC ) ". Here even if is IAC, the 
 * comparison would fail. Because the compiler would have generated 
 * an extra command CBW for IAC. CBW (ConvertByteToWord) makes a 
 * signed extension and hence for constant values greater that 127, 
 * that would become "FF XX" where XX is the hex representation of 
 * the constant. 
 */
#define  TEL_EOF                 236
#define  TEL_SUSP                237
#define  TEL_ABORT               238

#define  TEL_SE                  240
#define  TEL_NOP                 241
#define  TEL_DM                  242
#define  TEL_BREAK               243
#define  TEL_IP                  244
#define  TEL_AO                  245
#define  TEL_AYT                 246
#define  TEL_EC                  247
#define  TEL_EL                  248
#define  TEL_GOAHEAD             249
#define  TEL_SB                  250
#define  TEL_WILL                251
#define  TEL_WONT                252
#define  TEL_DO                  253
#define  TEL_DONT                254
#define  TEL_IAC                 255

/* Assigned Telnet Options. Description is in tel_opt_str[] */

#define  TEL_BINARY              0
#define  TEL_ECHO                1
#define  TEL_RECONNECT           2
#define  TEL_SGA                 3
#define  TEL_AMSN                4
#define  TEL_STATUS              5
#define  TEL_TIMING              6
#define  TEL_RCTAN               7
#define  TEL_OLW                 8

#define  TEL_CR                  0x0d  /* Ascii Carriage Return */
#define  TEL_LF                  0x0a  /* Ascii LineFeed */
#define  TEL_CRNUL               0x00  /* NULL that follows CR in telnet*/
#define  TEL_BACKSP              0x08  /* Ascii BackSpace key */
#define  TEL_ESC                 0x1B  /* Ascii ESCAPE key */
#define  TEL_SPACE               0x20  /* Ascii SPACE key */


/* The following macros are used to display description for various codes.
   Description is included only if option is set */
#ifdef TEL_SHOW_MSG

#define  TEL_CMD_STR_SIZE  20
#define  TEL_OPT_STR_SIZE  256
extern   char *   tel_cmd_str [TEL_CMD_STR_SIZE];
extern   char *   tel_opt_str [TEL_OPT_STR_SIZE];
#define  TEL_CMD_STR(X)    ((X<TEL_CMD_LOW)  ? "" : tel_cmd_str[X-TEL_CMD_LOW])
#define  TEL_OPT_STR(X)    tel_opt_str[X]
extern   char *   tel_err_str (int value);   /* telerr.c */

#else

#define  TEL_CMD_STR(X)  ""
#define  TEL_OPT_STR(X)  ""

#endif   /* TEL_SHOW_MSG */


/* List of error values used by telnet */

#define  TELBASE                                2000

/* used by tel_new() */
#define  TEL_NEW_ALLOC_ERR                      (TELBASE+11)
#define  TEL_NEW_CANT_ALLOC_RECV_BUF            (TELBASE+12)
#define  TEL_NEW_CANT_ALLOC_SEND_BUF            (TELBASE+13)
#define  TEL_NEW_CANT_ADD_TO_Q                  (TELBASE+14)
#define  TEL_NEW_CANT_ALLOC_OPTIONS             (TELBASE+15)
#define  TEL_NEW_CANT_ALLOC_CMD_BUF             (TELBASE+16)

/* used by tel_delq() */
#define  TEL_DELQ_Q_EMPTY                       (TELBASE+21)
#define  TEL_DELQ_CONN_NOT_FOUND                (TELBASE+22)

/* used by tel_delete() */
#define  TEL_DEL_NULL_ARGUMENT                  (TELBASE+31)

/* used by tel_init() */
#define  TEL_INIT_CANT_OPEN_SOCKET              (TELBASE+41)
#define  TEL_INIT_BIND_FAILED                   (TELBASE+42)
#define  TEL_INIT_LISTEN_FAILED                 (TELBASE+43)
#define  TEL_INIT_ASYNCSELECT                   (TELBASE+44)
#define  TEL_INIT_RCVBUF                        (TELBASE+45)
#define  TEL_INIT_SNDBUF                        (TELBASE+46)
#define  TEL_INIT_STARTUP_ERR                   (TELBASE+47)

/* used by tel_check() */
#define  TEL_CHECK_NO_LISTEN_SOCK               (TELBASE+51)

/* used by tel_process() */
#define  TEL_PROC_TCP_CLOSED                    (TELBASE+61)
#define  TEL_PROC_FATAL_ERROR                   (TELBASE+62)
#define  TEL_PROC_LOGGED_OUT                    (TELBASE+63)
#define  TEL_PROC_AUTH_FAILED                   (TELBASE+64)
#define  TEL_PROC_TIMED_OUT                     (TELBASE+65)

/* used by tel_sp_char_proc() */

#define  TEL_SP_BAD_SIZE                        (TELBASE+71)


/* Some peculiar error values. All error codes starting with TEL_N 
 * are compulsorily NEGATATIVE values. These would be used at places 
 * where positive values have some meaning.
 */
#define  TEL_N_CMD_UNKNOWN                      -1
#define  TEL_N_NO_IAC                           -2
#define  TEL_N_NO_SB                            -3 
#define  TEL_N_NO_SE                            -4


/* List of global variables used in various files */

extern   struct TelOptionList    tel_opt_list;
extern   struct TelStatistics    tel_stats;  /* stats for all telnet sessions */

extern   TEL_CONN telq;          /* Start of queue of open connections*/
extern   long     last_error;    /* last error that occured */
extern   char *   prompt;        /* For InterNiche stack is defined in main.c */



/* X=start position, Y=number of bytes to skip, Z=current size of buffer */
#define  tel_move(X,Y,Z)   MEMMOVE(X,X+Y,Z-Y)


/* List of functions in telnet.c */

TEL_CONN tel_new(void);
int      tel_delete(TEL_CONN tconn);
int      tel_addq(TEL_CONN tconn);
int      tel_delq(TEL_CONN tconn);
int      tel_loop(void);
int      tel_tcp_send(long s, char * buf, int len);
int      tel_process(TEL_CONN tcon);
TEL_CONN tel_conn_from_sock(SOCKTYPE s);
int      tel_read(TEL_CONN tconn);
int      tel_sp_char_proc(char * buf, int * in_size, char * obuf, 
            int * obuf_size,  int   old_size);
int      tel_proc_cmd(TEL_CONN tconn);
TEL_CONN tel_connection(SOCKTYPE temp_sock);


/* List of functions in telport.c */

int      tel_init(void);
void     tel_cleanup(void);
void     tel_check(void);
void     tel_start_timer(unsigned long * timer);
int      tel_check_timeout(unsigned long timer, u_long interval);
int      tel_proc_subcmd(TEL_CONN tconn, u_char * cmd, int cmd_len);
int      tel_tcp_recv(long iodesc);
int      tel_nvram_init(void);


/* List of functions in telparse.c */

int      tel_parse(TEL_CONN tconn);
int      tel_parse_cmd(TEL_CONN tconn, u_char * pos);
void     tel_parse_option (TEL_CONN tconn, int cmd, int new_opt);
void     tel_start_nego(TEL_CONN tconn);
int      tel_parse_subcmd(TEL_CONN tconn, u_char * cmd);
int      tel_send_cmd(SOCKTYPE sock, int cmd, int option);
TEL_OPT  tel_opt_search(struct TelOptionList *list,int find);


/* List of functions in telmenu.c */

int      tel_menu_init(void);
int      tel_show_opt(void * pio);
int      tel_show_stats(void * pio);
int      tel_close_sess(void * pio);

#endif   /* TELNET_H */

