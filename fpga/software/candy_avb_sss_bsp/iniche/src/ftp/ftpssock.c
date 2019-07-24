/*
 * FILENAME: ftpssock.c
 *
 * Copyright  2000 - 2002 By InterNiche Technologies Inc. All rights reserved
 *
 *    Sockets specific code for FTP. FTP Implementations on APIs 
 * other than sockets need to replace these.
 *
 * MODULE: FTP
 *
 * ROUTINES: ftp_cmdcb(), ftp_datacb(), t_tcplisten(), 
 * ROUTINES: FTP_TCPOPEN(), ftp4open(), ftp6open(), SO_GET_FPORT(), 
 * ROUTINES: SO_GET_LPORT(), ftps_v4pasv(), ftps_eprt(), ftps_epsv(), 
 *
 * PORTABLE: yes
 */

/* Additional Copyrights: */

/* ftpssock.c 
 * Portions Copyright 1996 by NetPort Software. All rights reserved. 
 * The Sockets-dependant portion of the FTP Server code. 
 * 11/24/96 - Created by John Bartas 
 */

#include "ftpport.h"    /* TCP/IP, sockets, system info */
#include "ftpsrv.h"

#ifdef IP_V6
#include "socket6.h"
#endif


#ifdef MINI_TCP

#include "msring.h"
extern struct msring ftps_msring;

#ifndef SUPERLOOP
extern TK_OBJECT(to_ftpsrv);
#endif


/* ftp_cmcb() - ftp command connection callback */
int
ftp_cmdcb(int code, M_SOCK so, void * data)
{
   int e = 0;

   switch(code)
   {
   case M_OPENOK:          /* new socket is connected */
      msring_add(&ftps_msring, so);
      break;
   case M_CLOSED:          /* TCP lost socket connection */
      break;               /* let stale conn timer catch these */
   case M_RXDATA:          /* received data packet, let recv() handle it */
   case M_TXDATA:          /* ready to send more, http_loop will do it */
      e = -1;        /* return nonzero code to indicate we don't want it */
      break;
   default:
      dtrap();             /* not a legal case */
      return 0;
   }

#ifdef SUPERLOOP
   ftps_loop();            /* give loop a spin */
#else /* multitasking */
   TK_WAKE(&to_ftpsrv);    /* wake webserver task */
#endif

   USE_VOID(data);
   return e;

}

/* ftp_datacb() - ftp data socket connection callback */

int
ftp_datacb(int code, M_SOCK so, void * data)
{
   int      e = 0;

   switch(code)
   {
   case M_OPENOK:          /* new socket is connected */
      /* if this FTP was listening for a data connection,
       * switch its listening socket for the data socket
       * and close the listening socket
       */
      if (so->app_data)
      {
         ftpsvr * ftp;
         M_SOCK old_so;

         ftp = (ftpsvr *)(so->app_data);
         old_so = ftp->datasock;
         ftp->datasock = so;
         if (old_so != SYS_SOCKETNULL)
         {
            UNLOCK_NET_RESOURCE(NET_RESID);
            m_close(old_so);
            LOCK_NET_RESOURCE(NET_RESID);
         }
      }
      UNLOCK_NET_RESOURCE(NET_RESID);
      m_ioctl(so, SO_NONBLOCK, NULL);   /* make socket non-blocking */
      LOCK_NET_RESOURCE(NET_RESID);
      break;
   case M_RXDATA:
      e = -1;              /* return nonzero code to indicate we don't want it */
      break;
   case M_TXDATA:
   case M_CLOSED:          /* lost ftp data socket connection */
      break;
   default:
      dtrap();             /* not a legal case */
      return 0;
   }

#ifdef SUPERLOOP
   ftps_loop();            /* give loop a spin */
#else /* multitasking */
   TK_WAKE(&to_ftpsrv);    /* wake webserver task */
#endif

   USE_VOID(data);
   return e;
}


#endif /* MINI_TCP */


/* FUNCTION: t_tcplisten()
 * 
 * t_tcplisten() implementation for NetPort sockets. Local port 
 * (lport) is passed in local endian. If lport is 0 (wildcard), then 
 * selected value is filled in to caller's variable. 
 *
 * PARAM1: u_short * lport
 *
 * RETURNS: returns a listening socket, or SYS_SOCKETNULL if error. 
 */


#ifdef MINI_TCP
SOCKTYPE
t_tcplisten(u_short * lport, int domain)
{
   int   e;
   SOCKTYPE sock;
   struct sockaddr_in   ftpsin;

   ftpsin.sin_addr.s_addr = INADDR_ANY;
   ftpsin.sin_port = htons(*lport);

   if (*lport == FTP_PORT)
      sock = m_listen(&ftpsin, ftp_cmdcb, &e);
   else
      sock = m_listen(&ftpsin, ftp_datacb, &e);

   if (sock == INVALID_SOCKET)
   {
listenerr:
      dprintf("error %d starting listen on ftp server\n", e);
      return SYS_SOCKETNULL;
   }

   /* put socket in non-blocking mode */
   e = m_ioctl(sock, SO_NBIO, NULL);
   if (e != 0)
   {
      m_close(sock);
      goto listenerr;
   }

   /* if wildcard port requested, return assigned port */
   if (*lport == 0)
      *lport = htons(sock->lport);

   return sock;
}

#else /* full-sockets TCP */

SOCKTYPE
t_tcplisten(u_short * lport, int domain)
{
   int   e;
   SOCKTYPE sock;

   sock = t_socket(domain, SOCK_STREAM, 0);
   if (sock == SYS_SOCKETNULL)
      return sock;

   switch(domain)
   {
#ifdef IP_V4
      case AF_INET:
      {
         struct sockaddr_in   ftpsin;
         int addrlen = sizeof(ftpsin);

         ftpsin.sin_family = AF_INET;
         ftpsin.sin_addr.s_addr = INADDR_ANY;
         ftpsin.sin_port = htons(*lport);
         e = t_bind(sock, (struct sockaddr*)&ftpsin, addrlen);
         if (e != 0)
         {
            e = t_errno(sock);
            dtrap();
            dprintf("error %d binding tcp listen on port %d\n",
             e, htons(*lport));
            return SYS_SOCKETNULL;
         }
         if(*lport == 0)   /* was it wildcard port? */
            *lport = htons(ftpsin.sin_port); /* return it to caller */
      }
      break;
#endif   /* IP_V4 */
#ifdef IP_V6
      case AF_INET6:
      {
         struct sockaddr_in6   ftpsin6;
         int addrlen = sizeof(ftpsin6);

         IP6CPY(&ftpsin6.sin6_addr, &in6addr_any);
         ftpsin6.sin6_port = htons(*lport);
         ftpsin6.sin6_family = AF_INET6;
         e = t_bind(sock, (struct sockaddr *)&ftpsin6, addrlen);
         if (e != 0)
         {
            e = t_errno(sock);
            dtrap();
            dprintf("error %d binding ftp6 listen on port %d\n",
             e, htons(*lport));
            return SYS_SOCKETNULL;
         }
         if (*lport == 0)  /* was it wildcard port? */
            *lport = htons(ftpsin6.sin6_port); /* return it to caller */
      }
      break;
#endif   /* IP_V6 */
      default:
         dtrap();    /* bad domain parameter */
         return SYS_SOCKETNULL;
   } /* end switch(domain) */

   /* For FTP, put socket in non-block mode */
   t_setsockopt(sock, SOL_SOCKET, SO_NBIO, NULL, 0);
   
   e = t_listen(sock, 5);
   if (e != 0)
   {
      e = t_errno(sock);
      dprintf("error %d starting listen on ftp server\n", e);
      return SYS_SOCKETNULL;
   }

   return sock;   /* return listen sock to caller */
}
#endif   /* notdef MINI_TCP */




/* FUNCTION: ftp4open()
 * FUNCTION: ftp6open()
 * FUNCTION: FTP_TCPOPEN() (may be macro)
 * 
 * FTP's TCP active open routine(s). Provided for FTP server to
 * can open an active TCP connection, such as FTP data. Versions
 * are provided for IPv4 and IPv6. The routine is intended to
 * be mapped to the macro FTP_TCPOPEN(), which is also provided 
 * as a dual-mode function below.
 *
 * PARAM1: ftpsrv * ftp
 *
 * RETURNS: Returns conencted socket if OK, 
 * SYS_SOCKETNULL on error 
 */

#ifndef FTP_TCPOPEN     /* see if it's defined */

/* dual-mode version */
SOCKTYPE 
FTP_TCPOPEN(ftpsvr * ftp)
{
#ifdef IP_V4
   if(ftp->domain == AF_INET)
      return(ftp4open(ftp));
#endif   /* IP_V4 */
#ifdef IP_V6
   if(ftp->domain == AF_INET6)
      return(ftp6open(ftp));
#endif   /* IP_V6 */

   dtrap(); /* bad domain setting */
   return SYS_SOCKETNULL;
}
#endif   /* FTP_TCPOPEN */


#ifdef IP_V4
SOCKTYPE 
ftp4open(ftpsvr * ftp)
{
   int   e; /* error holder */
   SOCKTYPE sock;
   struct sockaddr_in   ftpsin;

   sock = t_socket(AF_INET, SOCK_STREAM, 0);
   if (sock == SYS_SOCKETNULL)
      return sock;

   /* Change the socket options to allow address re-use. A customer 
    * requested this to ease implementing an FTP client with multiple 
    * connections.
    */
   if (ftp->server_dataport)
   {
#ifndef MINI_TCP
      int  opt = 1;	/* boolean option value holder */

      e = t_setsockopt(sock, 0, SO_REUSEADDR, &opt, sizeof(opt));
      if (e != 0)
      {
         e = t_errno(sock);
         dtrap();
         dprintf("error %d setting SO_REUSEADDR on port %d\n",
               e, ftp->server_dataport);
         return SYS_SOCKETNULL;
      }

      /* Bind local port to the socket we just created */
      ftpsin.sin_family = AF_INET;
      ftpsin.sin_addr.s_addr = INADDR_ANY;
      ftpsin.sin_port = htons(ftp->server_dataport);

      e = t_bind(sock, (struct sockaddr*)&ftpsin, sizeof(ftpsin));
      if (e != 0)
      {
         e = t_errno(sock);
         dtrap();
         dprintf("error %d binding tcp listen on port %d\n",
            e, ftp->server_dataport);
         return SYS_SOCKETNULL;
      }
#else
      sock->lport = htons(ftp->server_dataport);
#endif   /* MINI_TCP */
   }

   ftpsin.sin_addr.s_addr = htonl(ftp->host);
   ftpsin.sin_port = htons(ftp->dataport);

#ifdef MINI_TCP
   sock->app_data = NULL;
   e = m_connect(sock, &ftpsin, ftp_datacb);
#else
   ftpsin.sin_family = AF_INET;
   e = t_connect(sock, (struct sockaddr*)&ftpsin, sizeof(ftpsin));
#endif
   if (e != 0)
   {
      dtrap();
#ifndef MINI_TCP
      t_errno(sock); /* so debugger can see error */
#endif
      return SYS_SOCKETNULL;
   }

   /* FTP data socket can be in blocking or non-blocking mode */
#ifdef MINI_TCP
   m_ioctl(sock, SO_NBIO, NULL);
#else
   t_setsockopt(sock, 0, SO_NBIO, NULL, 0);
#endif
   
   return sock;
}
#endif   /* IP_V4 */

/* FUNCTION: ftp6open()
 * 
 * v6 version of FTP4open()
 *
 * PARAM1: ftpsvr * ftp
 *
 * RETURNS: Returns conencted socket if OK, SYS_SOCKETNULL on error 
 */

#ifdef IP_V6
SOCKTYPE 
ftp6open(ftpsvr * ftp)
{
   int         e; /* error holder */
   SOCKTYPE    sock;
   struct sockaddr_in6  ftpsin;
   unshort     lport;

   lport = ftp->server_dataport;

   sock = t_socket(AF_INET6, SOCK_STREAM, 0);
   if (sock == SYS_SOCKETNULL)
      return sock;

   /* Change the socket options to allow address re-use. A customer 
    * requested this to ease implementing an FTP client with multiple 
    * connections.
    */
   if (lport)
   {
      int opt = 1;

      e = t_setsockopt(sock, 0, SO_REUSEADDR, &opt, sizeof(opt));
      if (e != 0)
      {
         e = t_errno(sock);
         dtrap();
         dprintf("error %d setting SO_REUSEADDR on port %d\n", e, lport);
         return SYS_SOCKETNULL;
      }

      /* Bind local port to the socket we just created */
      ftpsin.sin6_family = AF_INET6;
      IP6CPY(&ftpsin.sin6_addr, &ip6unspecified);
      ftpsin.sin6_port = htons(lport);

      e = t_bind(sock, (struct sockaddr*)&ftpsin, sizeof(ftpsin));
      if (e != 0)
      {
         e = t_errno(sock);
         dtrap();
         dprintf("error %d binding tcp listen on port %d\n", e, lport);
         return SYS_SOCKETNULL;
      }
   }

   IP6CPY(&ftpsin.sin6_addr, &ftp->ip6_host);
   ftpsin.sin6_port = htons(ftp->dataport);

   ftpsin.sin6_family = AF_INET6;
   e = t_connect(sock, (struct sockaddr*)&ftpsin, sizeof(ftpsin));
   if (e != 0)
   {
      dtrap();
      t_errno(sock); /* so debugger can see error */
      return SYS_SOCKETNULL;
   }

   /* FTP data socket can be in blocking or non-blocking mode */
   t_setsockopt(sock, SOL_SOCKET, SO_NBIO, NULL, 0);
   
   return sock;
}
#endif   /* IP_V6 */


#ifndef MINI_TCP

/* FUNCTION: SO_GET_FPORT()
 * 
 * Return the foreign port of a socket. No error checking is done. It's
 * up to the caller to make sure this socket is connected before calling.
 *
 * PARAM1: the socket
 *
 * RETURNS: Returns the foreign port of the passed socket. 
 */

unshort
SO_GET_FPORT(SOCKTYPE sock)
{
   struct sockaddr   client;
   int      clientsize;
   unshort port;

   clientsize = sizeof(client);
   t_getpeername(sock, &client, &clientsize);

#ifdef IP_V4
#ifndef IP_V6  /* v4 only case: */
   port = ((struct sockaddr_in *)(&client))->sin_port;
#else    /* dual mode */
   if(clientsize == sizeof(struct sockaddr_in))
      port = ((struct sockaddr_in *)(&client))->sin_port;
   else
      port = ((struct sockaddr_in6 *)(&client))->sin6_port;
#endif   /* end dual mode code */
#else    /* no v4, v6 only */
   port = ((struct sockaddr_in6 *)(&client))->sin6_port;
#endif   /* v6 only */

      return (ntohs(port));
}

/* FUNCTION: SO_GET_LPORT()
 * 
 * Return the foreign port of a socket. No error checking is done. It's
 * up to the caller to make sure this socket is connected before calling.
 *
 * PARAM1: the socket
 *
 * RETURNS: Returns the foreign port of the passed socket. 
 */

unshort
SO_GET_LPORT(WP_SOCKTYPE sock)
{
   struct sockaddr_in   client;
   int      clientsize;
   unshort port;

   clientsize = sizeof(client);
   t_getsockname(sock, (struct sockaddr *) &client, &clientsize);

#ifdef IP_V4
#ifndef IP_V6  /* v4 only case: */
   port = ((struct sockaddr_in *)(&client))->sin_port;
#else    /* dual mode */
   if(clientsize == sizeof(struct sockaddr_in))
      port = ((struct sockaddr_in *)(&client))->sin_port;
   else
      port = ((struct sockaddr_in6 *)(&client))->sin6_port;
#endif   /* end dual mode code */
#else    /* no v4, v6 only */
   port = ((struct sockaddr_in6 *)(&client))->sin6_port;
#endif   /* v6 only */

      return (ntohs(port));
}

#endif   /* MINI_TCP */


/* error reporting mechanism for open sessions. "text" should start
 * with an FTP code (e.g. "425 " since it will be sent to client
 * on the command connection.
 */
extern int ftpputs(ftpsvr * ftp, char * text);

static  char *   err   = "425 Can't open data connection\r\n";

#ifdef IP_V4
int
ftps_v4pasv(ftpsvr * ftp)
{
   SOCKTYPE sock;
   u_short  port;
   unsigned long addr;
   char  responseBuf[80];

   /* create a TCP socket to listen on, it will be the data socket.
    * First set port to 0 so sockets will pick one for us
    */
   port = 0;
   sock = t_tcplisten(&port, AF_INET); /* call API to start listen */
   if (sock == SYS_SOCKETNULL)   /* if socket creation failed */
   {
      ftpputs(ftp, err);
      return EIEIO;
   }

   /* get our address and data port so we can tell the client
    * what address to connect to.
    */

#ifdef MINI_TCP
   sock->app_data = (void *)ftp;     /* ptr back to FTP server state */
   addr = ntohl(ftp->sock->lhost);   /* address from connected cmd sock */
   port = ntohs(sock->lport);        /* port from listening data sock */
#else    /* BSDish sockets */
{
   struct sockaddr_in   our_addr;
   int   sa_len = sizeof(our_addr);

   if (t_getsockname(ftp->sock,(struct sockaddr *) &our_addr, &sa_len))
   {
      /* tell client pasv failed */
      ftpputs(ftp,err);
      return t_errno(sock);
   }

   /* extract and convert to local endian our command socket address */
   addr = ntohl(our_addr.sin_addr.s_addr);

   /* get our port on the data socket */
   if (t_getsockname(sock,(struct sockaddr *) &our_addr, &sa_len))
   {
      /* close the socket we just opened */
      t_socketclose(sock);
      /* tell client pasv failed */
      ftpputs(ftp,err);
      return t_errno(sock);
   }

   /* extract and convert to local endian our data socket port */
   port = ntohs(our_addr.sin_port);
}
#endif   /* MINI_TCP or BSD sockets */

   /* create our response which tells the client what address and
    * port to connect to
    */
   sprintf(responseBuf,
    "227 Entering Passive Mode (%d,%d,%d,%d,%d,%d)\r\n",
    (int) (addr >> 24), (int) ((addr >> 16) & 0xff),
    (int) ((addr >> 8) & 0xff), (int) (addr & 0xff),
    (int) (port >> 8),(int) (port & 0xff));
   ftpputs(ftp,responseBuf);

   ftp->server_dataport = port;
   ftp->datasock = sock;

   return 0;
}
#endif /* IP_V4 */

#ifdef IP_V6

void
ftps_eprt(ftpsvr * ftp)
{
   char *   cp;         /* scratch command buffer pointer */
   char     delimit;    /* delimiter, from string */
   int      domain;     /* domain from command */
   unshort  port;       /* port number form command */
   char     addr[16];   /* local address buffer (binary) */
   char     responseBuf[80];

   /* point into command buffer after "EPSV " string */
   cp = &ftp->cmdbuf[5];
   delimit = *cp++;     /* read delimiter */

   if(*cp == '1')
      domain = AF_INET;
   else if(*cp == '2')
      domain = AF_INET6;
   else
   {
      ftpputs(ftp,"501 Unsupported EPRT domain\r\n");
      return;
   }
   if(domain != ftp->domain)
   {
      dtrap();    /* in theory this is possible.... */
      ftpputs(ftp,"501 Mismatched EPRT domain\r\n");
      return;
   }

   cp = strchr(cp, delimit);     /* advance to address */
   if(!cp) 
      goto eprt_error;
   cp++;
   if(inet_pton(domain, cp, &addr[0]))
   {
      /* Send detailed parse error message to console. */
      dprintf("FTP EPRT addr error: %s\n", pton_error);
      goto eprt_error;
   }

   cp = strchr(cp, delimit);     /* advance to port */
   if(!cp) 
      goto eprt_error;
   cp++;
   
   port = (unshort)atoi(cp);
   if(port == 0)
      goto eprt_error;

   /* got everything, fill in the ftps and ack the command */
   ftp->dataport = port;

   if(domain == AF_INET6)
      IP6CPY(&ftp->ip6_host, (ip6_addr*)(&addr));
   else
      ftp->host = *(ip_addr*)(&addr[0]);

   sprintf(responseBuf, "229 EPRT OK on port %u\r\n", port);
   ftpputs(ftp,responseBuf);
   return;

eprt_error:
   ftpputs(ftp,"501 Bad EPRT command\r\n");
   return;
}

void
ftps_epsv(ftpsvr * ftp)
{
   SOCKTYPE    sock;
   u_short     port;
   char        responseBuf[80];
   struct sockaddr_in6   our_addr;     /* v6 type socket */
   int         sa_len;

   /* Run the same tests as the v4 ftps_do_pasv() */
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

   /* create a TCP socket to listen on, it will be the data socket.
    * First set port to 0 so sockets will pick one for us
    */
   port = 0;
   sock = t_tcplisten(&port, ftp->domain); /* call API to start listen */
   if (sock == SYS_SOCKETNULL)   /* if socket creation failed */
   {
      ftpputs(ftp, err);
      return;
   }

   /* get our address and data port so we can tell the client
    * what address to connect to.
    */
   sa_len = sizeof(our_addr);
   if (t_getsockname(sock, (struct sockaddr *)&our_addr, &sa_len))
   {
      /* close the socket we just opened */
      t_socketclose(sock);
      /* tell client pasv failed */
      ftpputs(ftp,err);
      return;
   }

   /* extract and convert to local endian our data socket port */
   port = ntohs(our_addr.sin6_port);

   /* respons to the EPSV command. */
   sprintf(responseBuf,
    "229 Entering Extended Passive Mode (|||%d|)\r\n", port );
   ftpputs(ftp,responseBuf);

   /* we are now in passive mode, but the client hasn't connected yet */
   ftp->passive_state = FTPS_PASSIVE_MODE;

   /* we haven't received a data transfer command from the client yet */
   ftp->passive_cmd = 0;

   ftp->server_dataport = port;
   ftp->datasock = sock;

   return;
}
#endif /* IP_V6 */


