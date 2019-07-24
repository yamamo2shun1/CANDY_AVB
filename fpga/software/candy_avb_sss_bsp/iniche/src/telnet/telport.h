/*
 * FILENAME: telport.h
 *
 * Copyright 1997- 2000 By InterNiche Technologies Inc. All rights reserved
 *
 *
 * MODULE: TELNET
 *
 *
 * PORTABLE: no
 */

/* telport.h - Telnet Server port definitions 
 */
#ifndef TELPORT_H
#define TELPORT_H                1


/* List of hash defines which control what the code does */

#define TEL_USERAUTH             1
#define TEL_INICHE_IP            1 

#ifdef NOTDEF
#define TEL_SHOW_MSG             1
#define TEL_SESS_TIMEOUT         1
#endif /* NOTDEF */

#ifdef TEL_INICHE_IP
#define TEL_MENU                 1  /* enabled for InterNiche stack */
#endif

/* Meanings of various Hash defines 
 *
 * TEL_INICHE_IP: If this is defined, then TELNET implementation is 
 * tightly bound to InterNiche's TCP/IP * stack. This affects 
 * routing tables and various other things. 
 * 
 * TEL_SHOW_MSG: If this is defined, then description will be shown 
 * for error messages and for various options which were negotiated. 
 * This option can be disabled if running short of memory. 
 * 
 * TEL_USERAUTH: If login-password scenario is desired, then this 
 * option should be enabled. If this option is disabled, user 
 * directly starts working at the prompt 
 *
 * TEL_SESS_TIMEOUT: If this is defined,then TELNET Server will 
 * close a TELNET session if there 
 * has not been any activity for TEL_IDLE_TIME seconds. 
 *
 * TEL_MENU: Use this to enable the menu commands. The default 
 * implementation provides use of menu commands on InterNiche's
 * command prompt. It is done in tel_menu_init(). If menus are to be
 * used with some other architecture, then customization can be done
 * in tel_menu_init().
 */


#ifdef TEL_INICHE_IP    /* if we are using Local stack */

#include "ipport.h"
#include "tcpapp.h"
#include "nvparms.h"
#include "menu.h"


char *    print_ipad(unsigned long);
#define     INET_NTOA         print_ipad
#define     stat_printf       printf
#define     info_printf(x)

#define     INADDR_NONE       0xffffffff  /* usually part of winsock.h */
#define     FAILURE                 -1

#define     TEL_ALLOC(size)   npalloc(size) 
#define     TEL_FREE(ptr)     npfree(ptr)

#define     tel_exec_cmd      do_command

#define     SOCKET            SOCKTYPE

#else

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <winsock.h>
#include <time.h>
#include <string.h>
#include "in_utils.h"

#define  dtrap();                   _asm  {  int   3  }  ;
#define  INET_NTOA(X)               inet_ntoa(*(struct   in_addr*)&X)
#define  WM_TELNET_MSG              WM_USER+102
#define  USE_ARG(X)                 (X++)

extern   int dprintf(const char * format, ...);
#define  stat_printf dprintf
/*#define info_printf dprintf */

#define  info_printf

/* struct in_addr is a union of 3 fields, third being u_long. Hence 
 * in the above #define, we are casting from u_long to pointer to 
 * u_long to pointer to struct in_addr to contents of struct in_addr 
 * I don't like doing it, but have to save 1 extra function call
 */
/* now do it the way the CMU snmp code likes it: */
typedef   unsigned long  ip_addr;

/* map memory routines to standard lib */
#define  MEMCPY(dst,src,size)    memcpy(dst,src,size)
#define  MEMMOVE(dst,src,size)   memmove(dst,src,size)
#define  MEMSET(ptr,val,size)    memset(ptr,val,size)
#define  MEMCMP(p1,p2,size)      memcmp(p1,p2,size)

#define  TEL_ALLOC(size)   calloc(1,size)   
#define  TEL_FREE(ptr)     free(ptr)

#define  SUCCESS                 0
#define  FAILURE                 -1

#ifndef TRUE
#define  TRUE                    -1
#endif

#ifndef FALSE
#define  FALSE                   0
#endif

#ifndef   NULL
#define     NULL  ((void*)0)
#endif   /* NULL */

#define     tel_exec_cmd      do_command

#endif      /* else of TEL_INICHE_IP */


#define  TEL_NV_FILE   "telnet.nv"


#ifdef TEL_INICHE_IP

u_long inet_addr(char far * str);
extern   int    udp_send(unshort /*fport*/, unshort   /*lport*/, PACKET);
extern   PACKET udp_alloc(int, int);
extern   void   pk_free(PACKET);
extern   char * nextcarg(char * arg);  /* next comma-delimited arg */

#include "menu.h"

#else    /* Not Iniche stack, assume Winsock */

/* Most Winsock recv() implementations are slightly goofy, so... */
/* winsock functions we intercept: */

int      sys_send        (SOCKET s, char FAR * buf, int len, int flags);
int      sys_closesocket (SOCKET s);
#define  sys_errno(X)      WSAGetLastError()
#define  sys_accept        accept
#define  sys_recv          recv
#define  sys_bind          bind
#define  sys_socket        socket
#define  sys_listen        listen

#define  SYS_EWOULDBLOCK   WSAEWOULDBLOCK

extern   HWND  ViewhWnd;
void        SocketMsg(UINT socket, LONG params);
typedef unsigned char u_char;


#endif   /* TEL_INICHE_IP */



#ifdef  TEL_USERAUTH
#include "userpass.h"

#ifndef MAX_USERLENGTH
#define  MAX_USERLENGTH 32
#endif

#ifndef TELNET_USER
#define  TELNET_USER    4
#endif 

#define  TEL_CHECK_PERMIT(X,Y)   check_permit(X,Y,TELNET_USER,NULL)
#define  TEL_ADD_USER            add_user

/* Configurable limit on max number of telnet sessions.  Setting this value to 0 or
 * -1 results in no limitation on number of sessions.  The default setting is 32.
 */
#ifndef  MAX_TELNET_SESS
#define  MAX_TELNET_SESS 32
#endif   /* MAX_TELNET_SESS */
#endif   /* TEL_USERAUTH */

/* Telnet Server related non-volatile parameters. Please see nvparms.h 
 * and nvparms.c regarding the usage of the following structure.
 */
#ifdef TEL_INICHE_IP
#ifdef INCLUDE_NVPARMS
struct telnet_nvparam
{
   int tel_recv_buf_size  ; 
   int tel_send_buf_size  ; 
   int tel_port           ; 
   int tel_max_login_tries; 
   int tel_idle_time      ; 
};
#endif   /* INCLUDE_NVPARMS */
#endif   /* TEL_INICHE_IP */

#endif   /* TELPORT_H  */



