/*
 * FILENAME: telerr.c
 *
 * Copyright 1997- 2000 By InterNiche Technologies Inc. All rights reserved
 *
 *
 * MODULE: TELNET
 *
 * ROUTINES: tel_err_str()
 *
 * PORTABLE: yes
 */


/* File telerr.c Implementation file for Telnet Server This file 
 * contains description for all the errors. A call to tel_err_str() 
 * returns the description of the error. 
 */
#include "telnet.h"

#ifdef TELNET_SVR       /* Then include this source file */
#ifdef TEL_SHOW_MSG


struct ErrorInfo  {
    int   err;
    char *   str;
};

struct ErrorInfo  tel_err[]=  {
     {  TRUE                          , "success"                               },
     {  FALSE                         , "failure"                               },
     {  TELBASE                       , "Base value for telnet errors"          },
     {  TEL_NEW_ALLOC_ERR             , "Cant alloc mem for new conn"           },
     {  TEL_NEW_CANT_ALLOC_RECV_BUF   , "Cant alloc mem for receive buffer"     },
     {  TEL_NEW_CANT_ALLOC_SEND_BUF   , "Cant alloc mem for send buffer"        },
     {  TEL_NEW_CANT_ADD_TO_Q         , "Cant add new connectionto queue"       },
     {  TEL_NEW_CANT_ALLOC_OPTIONS    , "Cant alloc mem for options"            },
     {  TEL_NEW_CANT_ALLOC_CMD_BUF    , "Cant alloc mem for command bufer"      },
     {  TEL_DELQ_Q_EMPTY              , "Q is empty. Entry cant be removed"     },
     {  TEL_DELQ_CONN_NOT_FOUND       , "Conn not found in Q. Cant remove"      },
     {  TEL_DEL_NULL_ARGUMENT         , "Cant delete a NULL pointer"            },
     {  TEL_INIT_CANT_OPEN_SOCKET     , "Cant open socket for Telnet Server"    },
     {  TEL_INIT_BIND_FAILED          , "Cant bind socket"                      },
     {  TEL_INIT_LISTEN_FAILED        , "listen() failed on socket"             },
     {  TEL_INIT_ASYNCSELECT          , "ASyncSelect() failed on socket"        },
     {  TEL_INIT_RCVBUF               , "RCVBUF size for socket cant be set"    },
     {  TEL_INIT_SNDBUF               , "SNDBUF size for socket cant be set"    },
     {  TEL_INIT_STARTUP_ERR          , "WSAStartup() failed"                   },
     {  TEL_CHECK_NO_LISTEN_SOCK      , "Cant find listen socket"               },
     {  TEL_PROC_TCP_CLOSED           , "Underlying TCP conn was closed"        },
     {  TEL_PROC_FATAL_ERROR          , "Fatal error while doing TCP read()"    },
     {  TEL_PROC_LOGGED_OUT           , "User had logged out of telnet sess"    },
     {  TEL_PROC_AUTH_FAILED          , "User could not login(invalid password)"},
     {  TEL_PROC_TIMED_OUT            , "Connection timed out."                 },
     {  TEL_SP_BAD_SIZE               , "Cant do BS,ESC processing:bad ip size" },
     {  TEL_N_CMD_UNKNOWN             , "Received unknown Telnet command"       },
     {  TEL_N_NO_IAC                  , "Telnet cmd doesn't start with IAC"     },
     {  TEL_N_NO_SB                   , "No SB found in sub-negotiation cmd"    },
     {  TEL_N_NO_SE                   , "No SE found in sub-negotiation cmd"    },
};

/* FUNCTION: tel_err_str()
 * 
 * 
 *
 * PARAM1: int value - Error value (IN)
 *
 * RETURNS: Pointer to string , if error value is recognised, 
 * otherwise NULL.
 */

char * tel_err_str(int value)
{
   int   i;
   int   errCount=sizeof(tel_err)/sizeof(struct ErrorInfo);

   for ( i=0 ; i < errCount ; i++ )
   {
      if ( tel_err[i].err == value )
         return tel_err[i].str ;
   }
   return NULL;
}

#endif   /*  TEL_SHOW_MSG */
#endif   /* TELNET_SVR - Whole file can be ifdeffed out */



