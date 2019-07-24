/*
 * FILENAME: soperror.c
 *
 * Copyright  2000 By InterNiche Technologies Inc. All rights reserved
 *
 * soperror.c Implement a "perror()" like function for InterNiche 
 * sockets errors 
 *
 *
 * MODULE: MISCLIB
 *
 * ROUTINES: so_perror(), 
 *
 * PORTABLE: yes
 */


#include "ipport.h"     /* NetPort embedded system includes */

#ifdef INCLUDE_TCP
#ifndef MINI_TCP        /* not for NicheLite */
 
#include "tcpport.h"

char *   so_emessages[] =
{
   "",               /*  0 */
   "ENOBUFS",        /*  1 */
   "ETIMEDOUT",      /*  2 */
   "EISCONN",        /*  3 */
   "EOPNOTSUPP",     /*  4 */
   "ECONNABORTED",   /*  5 */
   "EWOULDBLOCK",    /*  6 */
   "ECONNREFUSED",   /*  7 */
   "ECONNRESET",     /*  8 */
   "ENOTCONN",       /*  9 */
   "EALREADY",       /* 10 */
   "EINVAL",         /* 11 */
   "EMSGSIZE",       /* 12 */
   "EPIPE",          /* 13 */
   "EDESTADDRREQ",   /* 14 */
   "ESHUTDOWN",      /* 15 */
   "ENOPROTOOPT",    /* 16 */
   "EHAVEOOB",       /* 17 */
   "ENOMEM",         /* 18 */
   "EADDRNOTAVAIL",  /* 19 */
   "EADDRINUSE",     /* 20 */
   "EAFNOSUPPORT",   /* 21 */
   "EINPROGRESS",    /* 22 */
   "ELOWER",         /* 23  lower layer (IP) error */
};


/* FUNCTION: so_perror()
 *
 * so_perror() - return a pointer to a static string which is a short 
 * mnemonic for the socket error message. 
 *
 * 
 * PARAM1: int ecode
 *
 * RETURNS: error text string
 */

char *   
so_perror(int ecode)
{
   if (ecode < (sizeof(so_emessages)/sizeof(char*)))
      return(so_emessages[ecode]);
   else
      return "";
}

#endif   /*  MINI_TCP */
#endif   /* INCLUDE_TCP */


