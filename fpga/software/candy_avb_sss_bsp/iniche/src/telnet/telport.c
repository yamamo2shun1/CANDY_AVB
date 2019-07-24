/*
 * FILENAME: telport.c
 *
 * Copyright 1997- 2002 By InterNiche Technologies Inc. All rights reserved
 *
 *
 * MODULE: TELNET
 *
 * ROUTINES: tel_init(), tel_start_timer(), tel_check_timeout(), 
 * ROUTINES: tel_cleanup(), tel_check(), tel_proc_subcmd(), sys_send(), 
 * ROUTINES: sys_closesocket(), SocketMsg(), 
 *
 * PORTABLE: Within interniche systems
 */

/* 
 * Define USE_FDS to enable using FD_xxx() functions, rather than
 * ad hoc code. 
 *
 * TODO: Remove old code after testing.
 *
 * #define USE_FDS      1        // use FD_xxx() functions
 */

/* File telport.c File containing porting dependent stuff 
 */
#include "telnet.h"

#ifdef TELNET_SVR       /* Then include this source file */

#ifndef SUPERLOOP
#ifndef OSPORT_H
#error Need to define OSPORT_H
#endif
#include OSPORT_H
#endif   /* SUPERLOOP */

#ifdef INCLUDE_NVPARMS
#include "nvparms.h"
struct telnet_nvparam telnet_nvparms;
#endif   /* INCLUDE_NVPARMS */

#ifdef IP_V4
struct sockaddr_in   tel_sin;
SOCKTYPE tel_listen = INVALID_SOCKET;
#endif /* IPv4 */

#ifdef IP_V6
struct sockaddr_in6  tel_sin6;
SOCKTYPE tel_listen6 = INVALID_SOCKET;
#endif /* IP_V6 */

#ifdef MINI_TCP
#include "msring.h"
/*
 * struct sockaddr_in   tel_sin;
 * SOCKTYPE tel_listen = INVALID_SOCKET;
 */
#ifndef TEL_NEWQLEN
#define TEL_NEWQLEN     4      /* max # buffered new sessions */
#endif

static struct msring tel_msring;
static M_SOCK tel_msring_buf[TEL_NEWQLEN + 1];
#endif   /* MINI_TCP */

#ifndef TEL_INICHE_IP
struct WSAData WSData;
int   StartupError=-1;
HWND ViewhWnd=0;
char *   prompt= "INET> " ;
#endif   /* ndef TEL_INICHE_IP */

/* Initialize the global variables */

struct TelGlobals tglob =
{
   TEL_RECV_BUF_SIZE  ,
   TEL_SEND_BUF_SIZE  ,
   TELNET_PORT        ,
   TEL_MAX_LOGIN_TRIES,
   TEL_IDLE_TIME      ,
};

/* later - read these from NVRAM also */

/* Default values for various options of a telnet connection */

struct TelOptionList tel_opt_list   =  {
     {  TEL_BINARY  ,  TRUE, FALSE,   FALSE,   FALSE,   FALSE,   FALSE,   FALSE },
     {  TEL_ECHO    ,  TRUE, FALSE,   FALSE,   TRUE,    TRUE,    FALSE,   FALSE },
     {  TEL_SGA     ,  TRUE, FALSE,   FALSE,   TRUE,    TRUE,    FALSE,   FALSE },
     {  TEL_STATUS  ,  TRUE, FALSE,   FALSE,   TRUE,    TRUE,    FALSE,   FALSE },
};

#ifdef MINI_TCP
/* telnet command connection callback */
#ifndef SUPERLOOP
extern TK_OBJECT(to_telnetsrv);
#endif   /* not SUPERLOOP */
int
tel_cmdcb(int code, M_SOCK so, void * data)
{
   int e = 0;

   switch(code)
   {
   case M_OPENOK:          /* new socket is connected */
      msring_add(&tel_msring, so);
      break;
   case M_CLOSED:          /* TCP lost socket connection */
      break;               /* let stale conn timer catch these */
   case M_RXDATA:          /* received data packet, let recv() handle it */
   case M_TXDATA:          /* ready to send more, loop will do it */
      e = -1;        /* return nonzero code to indicate we don't want it */
      break;
   default:
      dtrap();             /* not a legal case */
      return 0;
   }

#ifdef SUPERLOOP
#ifndef NINI_TCP
   tel_check();            /* give loop a spin */
#endif
#else /* multitasking */
   TK_WAKE(&to_telnetsrv);    /* wake server task */
#endif

   USE_VOID(data);
   return e;
}
#endif /* MINI_TCP */


#ifndef MINI_TCP

/* FUNCTION: telnet_listen()
 * 
 * Opens a listen socket on TELNET port. Works for IPv4 or IPv6
 *
 * PARAM1: domain - AF_INET or AF_INET6
 *
 * RETURNS: listen socket if OK, else INVALID_SOCKET
 */

SOCKTYPE
telnet_listen(int domain, struct sockaddr * sin, int sin_size)
{
   int      e;
   SOCKTYPE sock;

   /* start up an TELNET port listen */
   sock = t_socket(domain, SOCK_STREAM, 0);
   if (sock == INVALID_SOCKET)
      return INVALID_SOCKET;

#ifdef IP_V4
   if(domain == AF_INET)
      ((struct sockaddr_in *)sin)->sin_family = domain;
#endif

#ifdef IP_V6
   if(domain == AF_INET6)
      ((struct sockaddr_in6 *)sin)->sin6_family = domain;
#endif

   e = t_bind(sock, (struct sockaddr*)sin, sin_size);
   if (e)
   {
      e = t_errno(sock);
      dprintf("error %d binding telnet server\n", e);
      return INVALID_SOCKET;
   }

   e = t_listen(sock, 5);
   if (e)
   {
      e=t_errno(sock);
      dprintf("error %d starting listen on telnet server\n", e);
      return INVALID_SOCKET;
   }
   return sock;
}
#endif   /* not MINI_TCP */



/* FUNCTION: tel_init()
 * 
 * Does the initialization for telnet. Opens a listen socket on TELNET port.
 *
 * PARAM1: none
 *
 * RETURNS: SUCCESS , else error number
 */

int tel_init()
{

#ifdef INCLUDE_NVPARMS  
   tel_nvram_init();
#endif /* INCLUDE_NVPARMS  */


#ifdef TEL_INICHE_IP

#ifdef MINI_TCP
   {
      int e;
      msring_init(&tel_msring, tel_msring_buf,
               sizeof(tel_msring_buf) / sizeof(tel_msring_buf[0]));
      tel_sin.sin_addr.s_addr = htonl(INADDR_ANY);
      tel_sin.sin_port        = htons(tglob.tel_port);   /* telnet port */
      tel_listen = m_listen(&tel_sin, tel_cmdcb, &e);
      if (tel_listen == INVALID_SOCKET)
      {
         dprintf("error %d starting listen on telnet server\n", e);
      }
   }
#else /* BSD-ish sockets */

#ifdef IP_V4
   tel_sin.sin_addr.s_addr = htonl(INADDR_ANY);
   tel_sin.sin_port        = htons(tglob.tel_port);   /* telnet port */
   tel_listen = telnet_listen(AF_INET, 
            (struct sockaddr *)(&tel_sin), sizeof(tel_sin) );
   if(tel_listen == INVALID_SOCKET)
      return TEL_INIT_CANT_OPEN_SOCKET;

   t_setsockopt(tel_listen, 0, SO_NBIO, NULL, 0);     /* make non-blocking */
#endif   /* IP_V4 */

#ifdef IP_V6
   IP6CPY(&tel_sin6.sin6_addr, &ip6unspecified) ;
   tel_sin6.sin6_port        = htons(tglob.tel_port);   /* telnet port */
   tel_listen6 = telnet_listen(AF_INET6,
            (struct sockaddr *)(&tel_sin6), sizeof(tel_sin6) );
   if(tel_listen6 == INVALID_SOCKET)
      return TEL_INIT_CANT_OPEN_SOCKET;

   t_setsockopt(tel_listen6, 0, SO_NBIO, NULL, 0);    /* make non-blocking */
#endif   /* IP_V6 */

#endif   /* MINI_TCP or not */

 
   /* Install telnet menu into system menus */
   tel_menu_init();

#else    /* Demo using Winsock stack */

   StartupError = WSAStartup(0x0101,&WSData);
   if ( StartupError )
   {
      return TEL_INIT_STARTUP_ERR ;
   }

   e = WSAAsyncSelect(tel_listen,ViewhWnd,WM_TELNET_MSG,FD_ACCEPT);
   if ( e != 0 )
   {
      e = WSAGetLastError();
      dprintf("error %s starting listen on telnet server.\n",e);
      return TEL_INIT_ASYNCSELECT ;
   }

   e=setsockopt(tel_listen,SOL_SOCKET,SO_RCVBUF,
      (char far *)&tglob.tel_recv_buf_size, sizeof(int));
   if ( e != 0 )
   {
      e = WSAGetLastError();
      dprintf("error %s setting RCVBUF on telnet socket \n",e);
      return TEL_INIT_RCVBUF ;
   }

   e=setsockopt(tel_listen,SOL_SOCKET,SO_SNDBUF,
      (char far *)&tglob.tel_send_buf_size, sizeof(int));
   if ( e != 0 )
   {
      e = WSAGetLastError();
      dprintf("error %s setting SNDBUF on telnet socket \n",e);
      return TEL_INIT_SNDBUF ;
   }
#endif   /* else of ifdef TEL_INICHE_IP */

   tel_stats.conn_opened=0;
   tel_stats.conn_closed=0;

#ifdef TEL_USERAUTH
   /* If user authentication is enabled, then add user,password info */
   TEL_ADD_USER("guest", "guest", NULL);
   TEL_ADD_USER("root",  "root",  NULL);
#endif

   return SUCCESS ;
}

/* FUNCTION: tel_nvram_init()
 * 
 * This function reads the values from Non-volatile RAM (nvram)
 * and initializes global variables with those values.
 * Most of the global variables involved can't have a value
 * of 0. So if the nvram value is 0, don't use it.
 *
 * PARAM1: none
 *
 * REMARKS: For InterNiche TCP/IP stack, nvram parameters are
 * stored in a file ( for eg. webport.nv ). During initialization,
 * values would have been read into the nvram structure(nvparms.h).
 * So we just have to use those values.
 * Note       : This function is called during tel_init() after all other
 * tel_*_init() calls, so that nvram values override the
 * default values.
 *
 * RETURNS: SUCCESS or error code.
 */

#ifdef INCLUDE_NVPARMS  
int 
tel_nvram_init(void)
{
   /* So the following code is included only when an NVFS exists. */

   if ( telnet_nvparms.tel_recv_buf_size > 0 )
      tglob.tel_recv_buf_size = telnet_nvparms.tel_recv_buf_size ;

   if ( telnet_nvparms.tel_send_buf_size > 0 )
      tglob.tel_send_buf_size = telnet_nvparms.tel_send_buf_size ;

   if ( telnet_nvparms.tel_port > 0 )
      tglob.tel_port = telnet_nvparms.tel_port ;

   if ( telnet_nvparms.tel_max_login_tries > 0 )
      tglob.tel_max_login_tries = telnet_nvparms.tel_max_login_tries ;

   if ( telnet_nvparms.tel_idle_time > 0 )
      tglob.tel_idle_time = telnet_nvparms.tel_idle_time ;

   return SUCCESS ;
}
#endif /* INCLUDE_NVPARMS */


/* FUNCTION: tel_start_timer()
 * 
 * PARAM1: unsigned long *timer - Pointer to value to hold timer ( OUT )
 *
 * REMARKS: We will have this function for portability reasons.
 *
 * RETURNS: none
 */

void
tel_start_timer(unsigned long * timer)
{
#ifdef TEL_INICHE_IP
   u_long   current_clock_tick   =  cticks   ;
#else
   u_long   current_clock_tick   =  (u_long)clock()   ;
#endif

   *timer=current_clock_tick;
}


/* FUNCTION: tel_check_timeout()
 * 
 * Compares the CURRENT TICKS with the passed values
 *
 * PARAM1: unsigned long timer - Timer ( In )
 * PARAM2: u_long interval - Interval in seconds
 *
 * REMARKS: We will have this function for portability reasons.
 *
 * RETURNS: SUCCESS if timeout, error FAILURE
 */

#ifdef  TEL_SESS_TIMEOUT

int
tel_check_timeout(unsigned long timer, u_long interval)
{
#ifdef TEL_INICHE_IP
   int   ticks_per_sec  =  TPS   ;
   u_long   current_clock_tick   =  cticks   ;
#else
   int   ticks_per_sec  =  CLOCKS_PER_SEC ;
   u_long   current_clock_tick   =  (u_long)clock()   ;
#endif

   if ( interval * ticks_per_sec + timer <= current_clock_tick )
   {
      return SUCCESS ;
   }

   return FAILURE ;
}
#endif /* TEL_SESS_TIMEOUT */


/* FUNCTION: tel_cleanup()
 * 
 * Does the cleanup for telnet. Closes all open sockets,
 * and other stuff.
 *
 * PARAM1: none
 *
 * RETURNS: -
 */

void 
tel_cleanup()
{
   TEL_CONN temp_conn;
   TEL_CONN next_conn;

#ifdef IP_V4
   if(tel_listen)
      sys_closesocket(tel_listen);
#endif
#ifdef IP_V6
   if(tel_listen6)
      sys_closesocket(tel_listen6);
#endif

   /* Close all the open telnet sessions */
   temp_conn=telq;

   while ( temp_conn != NULL )
   {
      next_conn=temp_conn->next;

      /* Do cleanup at telnet layer. Closing socket,
         removing from queues, freeing memory */
      tel_delete(temp_conn);

      temp_conn=next_conn;
   }

#ifndef TEL_INICHE_IP
   WSACleanup();
#endif

}

/* FUNCTION: tel_accept()
 * 
 * Accept on the blocking telnet socket. Caller should already 
 * have determined that accept will succedd via a call to select(). 
 * This routine is designed to support eigther v4 or v6 sockets.
 *
 * PARAM1: socket to accept on
 * PARAM2: size of sockaddr (v4 or v6)
 *
 * RETURNS: -
 */

#ifndef MINI_TCP
void
tel_accept(SOCKTYPE listensock, int size_sockaddr)
{
   SOCKTYPE temp_sock;
   struct   sockaddr client;  /* generic v4/v6 sockaddr */
   int      sizep;
   int      err;

   /* got a connect to server listen */
   sizep = size_sockaddr;
   temp_sock = t_accept(listensock, (struct sockaddr *)&client, &sizep);

   if ( temp_sock != INVALID_SOCKET )
   {
      tel_connection(temp_sock);
   }
   else
   {
      err = t_errno(listensock);
      if(err != EWOULDBLOCK)
         dprintf("telnet accept err %d\n", err);
   }
   return;
}
#endif   /* MINI_TCP */

/* tel_check() re-entrancy protection */
int      inside_tel_check  =  0;


/* FUNCTION: tel_check()
 * 
 * See if we have a new connection pending. And do processing
 * for each of the open connections.
 *
 * PARAM1: void
 *
 * RETURNS: -
 */

#ifdef MINI_TCP

/* connect is handled by callback, so tel_check is minimal here */
void tel_check(void)
{
   M_SOCK so;

   if ( tel_listen == INVALID_SOCKET )
      return ;

   while(msring_del(&tel_msring, &so) == 0)
   {
      if(tel_connection(so) == 0)
      {
         dprintf("telnet connection error\n");
         continue;
      }
      m_ioctl(so, SO_NONBLOCK, NULL);   /* make socket non-blocking */
   }

   tel_loop();
}

#else   /* not MINI_TCP */

void tel_check(void)
{

#ifdef IP_V4
#ifndef IP_V6     /* V4 only */
   if(tel_listen == INVALID_SOCKET )
      return ;
#else             /* V4/V6 Dual mode version */
   if((tel_listen == INVALID_SOCKET ) && 
      (tel_listen6 == INVALID_SOCKET ))
   {
      return ;
   }
#endif   /* IP_V6 */
#endif   /* IP_V4 */
#ifdef IP_V6      /* V6 only */
   if(tel_listen6 == INVALID_SOCKET )
      return ;
#endif   /* IP_V6 */

   /* don't recurse into this function */
   if (inside_tel_check)
      return;

   inside_tel_check++;

#ifdef BLOCKING_APPS  /* use blocking select to drive server app */
   {
      fd_set tl_fdrecv;    /* fd_set arrays */
      TEL_CONN tl;
      int   events;        /* return from select() */
      int   recvs = 0;     /* number of entries in each array */

#ifdef USE_FDS
      FD_ZERO(&tl_fdrecv);
#else
      MEMSET(&tl_fdrecv, 0, sizeof(fd_set));
#endif  /* USE_FDS */

#ifdef IP_V4
      /* use the recv array to detect new connections */
      FD_SET(tel_listen, &tl_fdrecv);
      recvs++;
#endif   /* IP_V4 */
#ifdef IP_V6
      /* use the recv array to detect new connections */
      FD_SET(tel_listen6, &tl_fdrecv);
      recvs++;
#endif   /* IP_V6 */

      /* loop through TEL_CONN structs building read & write 
       * arrays for select 
       */
      for (tl = telq; tl; tl = tl->next)
      {
#ifdef USE_FDS
         /* error checking is done in FD_SET() */
         FD_SET(tl->sock, &tl_fdrecv);
         recvs++;
#else
         if (recvs >= FD_SETSIZE) /* too many open connections for select */
         {
            dtrap(); /* out of resource, tell engineer? */
            break;
         }
         else
         {
            FD_SET(tl->sock, &tl_fdrecv);
            recvs++;
         }
#endif  /* USE_FDS */
      }
       /* block until one of the sockets has activity. */
#ifndef  TEL_SESS_TIMEOUT    
      events = t_select(&tl_fdrecv, (fd_set *)NULL, (fd_set *)NULL, -1);
#else    /* session is closed with timeout */
      events = t_select(&tl_fdrecv, (fd_set *)NULL, (fd_set *)NULL,
                        TEL_IDLE_TIME);  /* enable inactivity timeout */
#endif   /* TEL_SESS_TIMEOUT */

#ifdef IP_V4
      if (FD_ISSET(tel_listen, &tl_fdrecv))
      {
         tel_accept(tel_listen, sizeof(struct sockaddr_in));
         events--;
      }
#endif   /* IP_V4 */
#ifdef IP_V6
      if (FD_ISSET(tel_listen6, &tl_fdrecv))
      {
         tel_accept(tel_listen6, sizeof(struct sockaddr_in6));
         events--;
      }
#endif   /* IP_V6 */

      if (events <= 0)   /* connects were only sockets */
      {
         inside_tel_check--;
         return;
      }
   }


#else /* not BLOCKING_APPS, poll non-blocking sockets */

#ifdef IP_V4
   tel_accept(tel_listen, sizeof(struct sockaddr_in));
#endif   /* IP_V4 */
#ifdef IP_V6
   tel_accept(tel_listen6, sizeof(struct sockaddr_in6));
#endif   /* IP_V6 */

#endif      /* BLOCKING_APPS */

   inside_tel_check--;
   tel_loop();
}
#endif   /* MINI_TCP */




/* FUNCTION: tel_proc_subcmd()
 * 
 * Process the sub-negotiation command
 * The command starts from the next byte following IAC-SB.
 * Length does not include IAC-SB and IAC-SE chars.
 *
 * PARAM1: TEL_CONN tconn - Pointer to telnet connection (IN)
 * PARAM2: u_char *cmd - Pointer to command (IN)
 * PARAM3: int cmd_len - Length of command (IN)
 *
 * REMARKS: Support for subnegotiation of options can be added here.
 *
 * RETURNS: SUCCESS or error number
 */

int
tel_proc_subcmd(TEL_CONN tconn, u_char * cmd, int cmd_len)
{
   USE_ARG(tconn);
   USE_ARG(cmd);
   USE_ARG(cmd_len);
   return SUCCESS ;
}


#ifndef TEL_INICHE_IP


/* FUNCTION: sys_send()
 * 
 * Do a TCP send() on WinSock.
 *
 * PARAM1: SOCKTYPE s
 * PARAM2: char FAR * buf
 * PARAM3: int len
 * PARAM4: int flags
 *
 * RETURNS: 
 */

int 
sys_send(SOCKTYPE s, char FAR * buf, int len, int flags)
{
   int   e;
   int   counter  =  0;
   int   sent  =  0;
#define  WIN_LOOPCOUNT  100

   if (len == 0)
   {
      dtrap();
      return 0;
   }

   e = send(s, buf, len, flags);
   if (e == len) /* If we sent everything, just return */
      return e;

   /* check if we need to do winmsgloop here - later !! */
   /* Check what to do when partial send occurs */
   return e;
}


/* FUNCTION: sys_closesocket()
 * 
 * Call the corresponding socket routine to close the
 * underlying TCP socket.
 *
 * PARAM1: SOCKTYPE s - Socket which to be closed (IN)
 *
 * REMARKS: This could easily be changed to a #define 
 *
 * RETURNS: as per standard
 */

int
sys_closesocket(SOCKTYPE s)
{
   return(closesocket(s));
}


/* FUNCTION: SocketMsg()
 * 
 * Upcall routine from Windows application when some data
 * has arrived on a particular socket.
 *
 * PARAM1: UINT socket - Socket on which data has arrived (IN)
 * PARAM2: LONG params - More info. (IN)
 *
 * RETURNS: Nothing
 */

void
SocketMsg(UINT socket, LONG params)
{
   tel_check();
}

#endif   /* ndef TEL_INICHE_IP */


#ifndef SUPERLOOP

TK_OBJECT(to_telnetsrv);
TK_ENTRY(tk_telnetsrv);
long telnetsrv_wakes = 0;

/*
 * Altera Niche Stack Nios port modification:
 * Create task entries for all optional add-on apps
 */
struct inet_taskinfo telnettask = {
      &to_telnetsrv,
      "telnet server",
      tk_telnetsrv,
      TK_TELNETSRV_TPRIO,
      TK_TELNETSRV_SSIZE,
};

/* The application thread works on a "controlled polling" basis: 
 * it wakes up periodically and polls for work.
 *
 * The FTP task could aternativly be set up to use blocking sockets,
 * in which case the loops below would only call the "xxx_check()"
 * routines - suspending would be handled by the TCP code.
 */

/* FUNCTION: tk_telnetsrv
 * 
 * PARAM1: n/a
 *
 * RETURNS: n/a
 */

TK_ENTRY(tk_telnetsrv)
{
   int err;

   while (!iniche_net_ready)
      TK_SLEEP(1);

   err = tel_init();
   if( err == SUCCESS )
   {
      exit_hook(tel_cleanup);
   }
   else
   {
      dtrap();    /* tel_init()  shouldn't ever fail */
   }

   for (;;)
   {
      tel_check();         /* will block on select */
      tk_yield();          /* give up CPU in case it didn't block */
      telnetsrv_wakes++;   /* count wakeups */

      if (net_system_exit)
         break;
   }
   TK_RETURN_OK();
}

#endif /* not SUPERLOOP */


#ifdef INCLUDE_NVPARMS
/* Please see nvparms.h and nvparms.c regarding the usage of 
 * the following datatypes and functions.
 */

struct nvparm_info telnet_nvformats[] = 
{
   {"telnet recv buf: %u\n"       , NVINT, NVBND_NOOP, \
    &telnet_nvparms.tel_recv_buf_size  , NULL, }, 

   {"telnet send buf: %u\n"       , NVINT, NVBND_NOOP, \
    &telnet_nvparms.tel_send_buf_size  , NULL, }, 

   {"telnet port: %u\n"           , NVINT, NVBND_NOOP, \
    &telnet_nvparms.tel_port           , NULL, }, 

   {"telnet max login tries: %u\n", NVINT, NVBND_NOOP, \
    &telnet_nvparms.tel_max_login_tries, NULL, }, 

   {"telnet idle time: %u\n"      , NVINT, NVBND_NOOP, \
    &telnet_nvparms.tel_idle_time      , NULL, }, 
};

#define NUMTELNET_FORMATS  \
        (sizeof(telnet_nvformats)/sizeof(struct nvparm_info))

/* FUNCTION: telnet_nvset()
 * 
 * PARAM1: NV_FILE * fp
 *
 * RETURNS: Silent return of 0 for OK
 */
int telnet_nvset(NV_FILE * fp)
{
int i = 0;

   nv_fprintf(fp, telnet_nvformats[i++].pattern, \
              telnet_nvparms.tel_recv_buf_size  );

   nv_fprintf(fp, telnet_nvformats[i++].pattern, \
              telnet_nvparms.tel_send_buf_size  );

   nv_fprintf(fp, telnet_nvformats[i++].pattern, \
              telnet_nvparms.tel_port           );

   nv_fprintf(fp, telnet_nvformats[i++].pattern, \
              telnet_nvparms.tel_max_login_tries);

   nv_fprintf(fp, telnet_nvformats[i++].pattern, \
              telnet_nvparms.tel_idle_time      );

   return 0;
} 

struct nvparm_format telnet_format = 
{
   NUMTELNET_FORMATS,
   &telnet_nvformats[0],
   telnet_nvset,
   NULL,
};

#endif   /* INCLUDE_NVPARMS */


/* FUNCTION: prep_telnet()
 *
 * PARAMS: NONE
 *
 * RETURNS: Error Code or 0 for OK
 */
int prep_telnet(void)
{
int e = 0;

#ifdef INCLUDE_NVPARMS
   e = install_nvformat(&telnet_format, nv_formats);

   if(e)
   {
      dprintf("unable to install Telnet NVPARMS, reconfigure nv_formats[]\n");
      dtrap();
   }
#endif   /* INCLUDE_NVPARMS */
   return e;
}

#endif   /* #ifdef TELNET_SVR */

