/*
 * FILENAME: tcp_echo.c
 *
 * Copyright  2000 By InterNiche Technologies Inc. All rights reserved
 *
 * Test program for TCP sockets API. Implements an echo 
 * server and client
 *
 * MODULE: MISCLIB
 *
 * ROUTINES: tcp_client_add(), tcp_client_del(), 
 * ROUTINES: tcp_client_from_pio(), tcp_client_from_sock(), tcp_secho_init(), 
 * ROUTINES: tcp_secho_close(), tcp_cecho_close(), tcp_echo_cleanup(), 
 * ROUTINES: tcp_echo_init(), tcp_sendecho(), tcp_send_an_echo(), 
 * ROUTINES: echo_svr_upcall(), tcp_echo_recv(), tcp_echo_poll(), 
 * ROUTINES: tcp_echo_stats(),
 *
 * PORTABLE: yes
 */

 
#include "ipport.h"     /* NetPort embedded system includes */

#ifdef TCP_ECHOTEST     /* whole file can be ifdeffed out */

#ifndef WINSOCK
#include "tcpport.h"    /* NetPort embedded system includes */
#include "in_utils.h"
#ifndef sock_noblock
#define  sock_noblock(sock,flag) setsockopt(sock,SOL_SOCKET,SO_NBIO,NULL,0)
#endif
#endif


#define  ECHO_PORT               7  /* standard UDP/TCP echo port */

/* The states of a TCP_ECHO Client. Either it is IDLE or BUSY exchaning
   TCP Echo packets */
#define  TCP_IDLE                1
#define  TCP_BUSY                2

/* TCP_IDLE_TIMEOUT is used to cleanup idle TCP Echo Client connections 
   If a TCP Echo Client has not been used for this long, delete it */
#define  TCP_IDLE_TIMEOUT        600   /* value in seconds */

/* TCP_MAX_ECHO_CONN defines the maximum simultaneous TCP Echo 
 * Clients connections allowed. This is required because for polling 
 * TCP Echo Clients we are using t_select() call. t_select() uses 
 * fd_set array, which can hold a maximum of FD_SETSIZE sockets. As 
 * one socket is needed to poll TCP Echo Server, the maximum 
 * available for TCP Echo Clients is 
 */
#define  TCP_MAX_ECHO_CONN       FD_SETSIZE-1   /* max TCP Echo Client conns*/


SOCKTYPE elisten_sock =         INVALID_SOCKET; /* echo server socket */
SOCKTYPE esvr_sock    =         INVALID_SOCKET; /* echo server active socket */

extern   int                    kbhit(void); /* from Microsquash|Borland lib*/
extern   char *                  prompt;

/* This global switch can be set prior to calling tcp_echos_init() */
int      tcpecho_server =  TRUE;

/* Do we need to close the server's active socket? */
int      esvr_sock_close = FALSE;

#ifndef  ECHOBUFSIZE
#define  ECHOBUFSIZE    (8*1024) 
#endif

/* List of error codes used by this file */

#define  TCPE_BASE                  400
#define  TCPE_ALLOC_ERROR           (TCPE_BASE+1)
#define  TCPE_NODE_NOT_FOUND        (TCPE_BASE+2)
#define  TCPE_CANT_OPEN_SOCKET      (TCPE_BASE+3)
#define  TCPE_CONNECT_FAILED        (TCPE_BASE+4)
#define  TCPE_SEND_FAILED           (TCPE_BASE+5)
#define  TCPE_BAD_SOCKET            (TCPE_BASE+6)
#define  TCPE_BIND_FAILED           (TCPE_BASE+7)
#define  TCPE_LISTEN_FAILED         (TCPE_BASE+8)
#define  TCPE_CLIENT_NOT_FOUND      (TCPE_BASE+9)
#define  TCPE_TIME_NOT_RIPE         (TCPE_BASE+10)
#define  TCPE_ALL_SOCKS_USED        (TCPE_BASE+11)
#define  TCPE_BLOCKED               (TCPE_BASE+12)

#define  SUCCESS                    0


/* Structure for holding information regarding a TCP Client connection */

struct TcpClient
{
   SOCKTYPE sock;                /* client socket */
   void *   pio;                 /* Output messages go to this device */
   ip_addr  rhost;               /* TCP Echos to be sent to this host */
   u_long   replies;             /* Number of burst replies received */
   u_long   times;               /* Number of bursts is to be sent */
   u_long   send_cnt ;           /* Number of the reply to be sent(counter) */
   int      state;               /* state of the TCP connection */
   u_long   ticks;               /* ClockTick when next burst is to be sent */
   u_long   delay;               /* Delay between two Echo bursts */
   u_long   len;                 /* Length of TCP each burst to be sent */
   u_long   tot_sent;            /* Total echo bytes sent */
   u_long   tot_rcvd;            /* Total echo bytes received */
   struct   TcpClient * next;
   char     inbuf[ECHOBUFSIZE];  /* per-client data buffer */
   int      sending;             /* non-zero while client is in send loop */
};

char *   srv_inbuf   =  NULL;

typedef struct TcpClient * TCPCLIENT ;
TCPCLIENT tcpq;                  /* Start node for queue of TCP clients */


int         tcp_client_add(void *  pio);
int         tcp_client_del(TCPCLIENT tcpclient);
TCPCLIENT tcp_client_from_pio (void *  pio);
TCPCLIENT tcp_client_from_sock(SOCKTYPE  sock);
int         tcp_secho_init (void * pio);
int         tcp_secho_close(void * pio);   
int         tcp_cecho_close(void * pio);
int         tcp_send_an_echo(TCPCLIENT tcpclient);
void        tcp_echo_poll(void);
int         tcp_echo_init(void);
void        tcp_echo_cleanup(void);
int         tcp_echo_stats(void * pio);
int         tcp_sendecho(void * pio, ip_addr fhost, long len, long times);


/* FUNCTION: tcp_client_add()
 *
 * Add a new client to the Queue
 * 1. Allocates memory for a new node
 * 2. Adds to queue
 * 3. Initialized data members
 *
 * 
 * PARAM1: void * pio - device for console output
 *
 * RETURNS: 0 on SUCCESS or error number 
 */

int
tcp_client_add(void * pio)
{
   TCPCLIENT tmpclient;
   int   num_of_conn=0;

   tmpclient=tcpq;
   while (tmpclient)
   {
      num_of_conn++;
      tmpclient=tmpclient->next;
   }

   if (num_of_conn >= TCP_MAX_ECHO_CONN)
   {
      return TCPE_ALL_SOCKS_USED;
   }

   /* Allocate memory */
   tmpclient = (TCPCLIENT)npalloc(sizeof(struct TcpClient)) ;
   if (tmpclient == NULL)
   {
      ns_printf(pio,"Allocation error.\n");
      return TCPE_ALLOC_ERROR;
   }

   /* Add the new node to the head of the queue */
   tmpclient->next = tcpq;
   tcpq=tmpclient;

   /* Initialize data members */
   tmpclient->sock      = INVALID_SOCKET;
   tmpclient->pio       = pio;
   tmpclient->rhost     = 0;
   tmpclient->times     = 1;        /* Count of total EchoPkts to be sent */
   tmpclient->send_cnt  = 0;        /* Num of Echo Pkts sent till now */
   tmpclient->ticks     = cticks;   /* Timetick to track sending of echos */
   tmpclient->state     = TCP_IDLE ;   /* Are we sending TCP Echos? No. */
   tmpclient->replies   = 0;        /* Num of replies received so far */
   tmpclient->len       = 64;       /* Default value of Echo Pkt */
   tmpclient->tot_sent  = 0;
   tmpclient->tot_rcvd  = 0;

   return SUCCESS;
}


/* FUNCTION: tcp_client_del()
 * 
 * Cleanup stuff for a TCP Echo Client Three things to be done here
 * 1. Close the underlying socket 
 * 2. Remove node from the Q
 * 3. Free the memory used by the node
 *
 * PARAM1: TCPCLIENT tcpclient - Pointer to TCP Echo Client connection
 *
 * RETURNS: 0 on SUCCESS or error number 
 */

int 
tcp_client_del(TCPCLIENT tcpclient)
{
   int   e;
   TCPCLIENT tmpclient, prevclient;

   /* Close the underlying socket */
   if (tcpclient->sock != INVALID_SOCKET)
   {  
      e = socketclose(tcpclient->sock);
      if (e)
      {
         e = t_errno(tcpclient->sock);
         ns_printf(tcpclient->pio,"tcp echo: close error %d\n", e);
      }
   }

   /* Remove from the q */
   if (tcpq == tcpclient)
   {
      /* It is the first node */
      tcpq = tcpq->next;
   }
   else
   {
      prevclient=tcpq;
      tmpclient=tcpq->next;
      while (tmpclient)
      {
         if (tmpclient == tcpclient)
         {
            /* Found the node in the list */
            prevclient->next=tmpclient->next ;
            break;
         }
         else
         {
            prevclient=tmpclient;
            tmpclient=tmpclient->next ;
         }
      }

      /* Was the node found in Q ? */
      if (tmpclient == NULL)
      {
         /* Node not found in Q !! */
         dtrap();
         return TCPE_NODE_NOT_FOUND ;
      }
   }

   npfree(tcpclient);

   return SUCCESS;
}


/* FUNCTION: tcp_client_from_pio()
 * 
 * Return the TCP Echo Client connect for a particular session.
 *
 * PARAM1: void * pio - device for console output
 *
 * RETURNS: Pointer to TCPCLIENT. NULL if connection not found.  
 */

TCPCLIENT
tcp_client_from_pio(void * pio)
{
   TCPCLIENT tmpclient=tcpq;

   while (tmpclient)
   {
      if (tmpclient->pio == pio)
         break ;
      else
         tmpclient=tmpclient->next;
   }

   return tmpclient;
}



/* FUNCTION: tcp_client_from_sock()
 * 
 * Return the TCP Echo Client connection for a particular session.
 *
 * PARAM1: SOCKTYPE sock
 *
 * RETURNS: Pointer to TCPCLIENT. NULL if connection not found. 
 */

TCPCLIENT
tcp_client_from_sock(SOCKTYPE sock)
{
   TCPCLIENT tmpclient=tcpq;

   while (tmpclient)
   {
      if (tmpclient->sock == sock)
         break ;
      else
         tmpclient=tmpclient->next;
   }

   return tmpclient;
}



/* FUNCTION: tcp_secho_init()
 * 
 * Initialize the TCP Echo Server. If the flag tcpecho_server is false,
 * then Server Init is not done.
 * 
 * PARAM1: void * pio - device for console output
 *
 * RETURNS: 0 on SUCCESS or error number. 
 */

int 
tcp_secho_init(void * pio)
{
   int   e;    /* error holder */
   struct sockaddr_in   me;   /* my IP info, for bind() */
   int   opt;

   if (tcpecho_server)
   {
      ns_printf(pio,"tcp echo srv - starting.\n");

      /* open TCP socket */
      elisten_sock = socket(AF_INET, SOCK_STREAM, 0);
      if (elisten_sock == INVALID_SOCKET)
      {
         ns_printf(pio,"TCP echo: bad socket: %d\n", elisten_sock);
         return TCPE_BAD_SOCKET ;
      }

      opt = 1;
      e = t_setsockopt(elisten_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
      if (e != 0)
      {
         e = t_errno(elisten_sock);
         dtrap();
         dprintf("error %d setting SO_REUSEADDR on port %d\n", e, ECHO_PORT);
         return SYS_SOCKETNULL;
      }

      me.sin_family = AF_INET;
      me.sin_addr.s_addr = INADDR_ANY;
      me.sin_port = htons(ECHO_PORT);

      e = bind(elisten_sock, (struct sockaddr*)&me, sizeof(me));
      if (e != 0)
      {
         e = t_errno(elisten_sock);
         ns_printf(pio,"tcp_echo: bad socket bind: %d, %s\n", e, so_perror(e));
         socketclose(elisten_sock);
         elisten_sock = INVALID_SOCKET;
         return TCPE_BIND_FAILED ;
      }
      e = listen(elisten_sock, 3);
      if (e != 0)
      {
         e = t_errno(elisten_sock);
         ns_printf(pio,"tcp_echo: bad socket listen: %d %s\n", e, so_perror(e));
         socketclose(elisten_sock);
         elisten_sock = INVALID_SOCKET;
         return TCPE_LISTEN_FAILED;
      }
      /* for listen socket into Non-blocking mode so we can poll accept */
      sock_noblock(elisten_sock, TRUE);
   }
   else
      ns_printf(pio,"tcp echo server not enabled\n");

   srv_inbuf = (char *)npalloc(ECHOBUFSIZE);
   if (srv_inbuf == NULL)
   {
      ns_printf(pio, "tcp server: alloc failed\n");
      socketclose(elisten_sock);
      elisten_sock = INVALID_SOCKET;
      return TCPE_LISTEN_FAILED;
   }

   return SUCCESS;
}


/* FUNCTION: tcp_secho_close()
 * 
 * Close the TCP Echo Server.
 *
 * PARAM1: void * pio - device for console output
 *
 * RETURNS: 0 on SUCCESS or error number. 
 */

int
tcp_secho_close(void * pio)
{
   int   e  =  0;       /* scratch error holder */
   int   retval   =  0; /* return last non-zero error */

   if (esvr_sock != INVALID_SOCKET)
   {
      e = socketclose(esvr_sock);
      if (e)
      {
         retval = e = t_errno(esvr_sock);
         ns_printf(pio,"tcp echo server: close error %d %s\n", e, so_perror(e));
      }
      else
      {
         ns_printf(pio,"tcp echo srv - closing.\n");
         esvr_sock = INVALID_SOCKET;
      }
   }

   if (srv_inbuf)
   {
      npfree(srv_inbuf);
      srv_inbuf=NULL;
   }
   if (elisten_sock == INVALID_SOCKET)
      return e;

   e = socketclose(elisten_sock);
   if (e)
   {
      retval = e = t_errno(elisten_sock);
      ns_printf(pio,"tcp echo: server close error %d %s\n", e, so_perror(e));
   }
   elisten_sock = INVALID_SOCKET;

   return retval;
}


/* FUNCTION: tcp_cecho_close()
 *
 * Close the TCP Echo Client.
 *
 * PARAM1: void * pio - device for console output
 *
 * RETURNS: 0 on SUCCESS or error number. 
 */

int
tcp_cecho_close(void * pio)
{
   TCPCLIENT  tcpclient;

   tcpclient=tcp_client_from_pio(pio);
   if (tcpclient == NULL)
   {
      ns_printf(pio,"No TCP Echo Client found for this session.\n");   
      return TCPE_CLIENT_NOT_FOUND ;
   }
   else
   {
      ns_printf(pio,"Closing TCP Echo Client.\n");   
      return tcp_client_del(tcpclient);
   }

}


/* FUNCTION: tcp_echo_cleanup()
 *
 * Cleanup all TCP Echo related data structures.
 *
 * PARAM1: void
 *
 * RETURNS: 
 */

void
tcp_echo_cleanup(void)
{
   TCPCLIENT  tcpclient=tcpq;
   TCPCLIENT  nextclient;

   /* Close all the open clients */
   while (tcpclient)
   {
      nextclient=tcpclient->next; 
      tcp_client_del(tcpclient);
      tcpclient=nextclient;
   }

   tcp_secho_close(NULL);
}


/* FUNCTION: tcp_echo_init()
 * 
 * Initialize TCP Echo Server
 * Remarks : This function has been defined so that a clean interface 
 * is provided for TCP Echo.
 *
 * PARAM1: void * pio - device for console output
 *
 * RETURNS: 0 on SUCCESS or error number. 
 */

int
tcp_echo_init(void)
{
   return tcp_secho_init(NULL);
}
 

/* FUNCTION: tcp_sendecho()
 * 
 * Send TCP Echo packets to a TCP Echo Server. If a TCP Echo Client
 * has not be created, then it is done (one per GenericIO device).
 * Also, if the TCP Echo Server IP address is different from the
 * one being used earlier, then socket reinitialization is done.
 *
 * PARAM1: void * pio      - device for console output
 * PARAM2: ip_addr fhost   - IP Address of TCP Echo Server (IN)
 * PARAM3: long len        - Length of TCP Echo pkt to be sent (IN)
 * PARAM4: long times)     - Number of TCP Echo pkts to be sent (IN)
 *
 * RETURNS: 0 on SUCCESS or error number. 
 */

int
tcp_sendecho(void * pio,
   ip_addr  fhost,      /* already in net endian */
   long     len,
   long     times)
{
   struct sockaddr_in   sa;
   int   e;
   SOCKTYPE tmp;
   TCPCLIENT tcpclient;

   /* If client socket isn't open or host has changed, open it */
   tcpclient = tcp_client_from_pio(pio);
   if (tcpclient == NULL)   /* not found */
   {
      if ((e=tcp_client_add(pio)) != SUCCESS)
      {
         if (e == TCPE_ALL_SOCKS_USED)
         {
            ns_printf(pio,"All TCP Echo Client connections are in use.\n");
            ns_printf(pio,"Please try at a later time.\n");
         }
         return e;
      }
      tcpclient = tcp_client_from_pio(pio);
   }

   if (tcpclient->sock == INVALID_SOCKET || fhost != tcpclient->rhost)
   {
      int opt;

      if (tcpclient->sock != INVALID_SOCKET) /* host changed */
      {
         socketclose(tcpclient->sock);
         tcpclient->sock=INVALID_SOCKET;
      }

      /* (re)open new socket to client for echo */
      tmp = socket(AF_INET, SOCK_STREAM, 0);
      if (tmp == INVALID_SOCKET)
      {
         ns_printf(pio,"tcp echo: can't open socket\n");
         tcp_client_del(tcpclient);
         return TCPE_CANT_OPEN_SOCKET ;
      }

      e = t_setsockopt(tmp, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt));

      sa.sin_family = AF_INET;
      /* host is already in network endian */
      sa.sin_addr.s_addr = tcpclient->rhost = fhost;
      sa.sin_port = htons(ECHO_PORT);

      e = connect(tmp, (struct sockaddr*)&sa, sizeof(sa));
      if (e != 0)
      {
         e = t_errno(tmp);
         ns_printf(pio,"tcp_echo: bad socket connect: %d %s\n", e, so_perror(e));
         tcp_client_del(tcpclient);
         return TCPE_CONNECT_FAILED ;
      }
      /* Put in non-block mode */
      sock_noblock(tmp, TRUE);      
      tcpclient->sock = tmp;
   }


   tcpclient->replies   = 0;
   tcpclient->times     = times;
   tcpclient->delay     = pingdelay;
   tcpclient->ticks     = cticks;
   tcpclient->state     = TCP_BUSY ;
   tcpclient->len       = len ;
   tcpclient->send_cnt  = 0 ;

   return tcp_send_an_echo(tcpclient);
}

 

/* FUNCTION: tcp_send_an_echo()
 * 
 * Send a TCP Echo packet from our client to remote server
 *
 * PARAM1: TCPCLIENT tcpclient - Pointer to client's connection (IN)
 *
 * RETURNS: 0 on SUCCESS or error number 
 */

int
tcp_send_an_echo(TCPCLIENT tcpclient)
{
   int   e;
   u_long   total;
   u_long   sendsize;
   u_long   starttime;
   int      block;
   u_long   words;      /* send data word index */
   u_long   dataval;
   u_long   sent  =  0; /* sent in this loop */

   if (tcpclient->ticks > cticks)  
      return TCPE_TIME_NOT_RIPE ;
   if (tcpclient->sending)    /* guard against re-entry */
      return TCPE_BLOCKED;
   tcpclient->sending++;

   ns_printf(tcpclient->pio,"sending TCP echo %ld to %s\n", 
    tcpclient->send_cnt, print_ipad(tcpclient->rhost));

   total = tcpclient->len;
   block = 0;
   starttime = cticks;
   while (total > sent)
   {
      tcpclient->ticks = cticks + tcpclient->delay;   /* back off time for next send */
      sendsize = total - sent;
      if (sendsize > ECHOBUFSIZE)
         sendsize = ECHOBUFSIZE;

      /* write pattern into full-size data for integrity test */
      dataval = (tcpclient->tot_sent + sent)/4;    /* data for first word of buffer */
      for (words = 0; words < (ECHOBUFSIZE/4); words++)
         *((u_long*)tcpclient->inbuf + words) = dataval++;

      e = send(tcpclient->sock, tcpclient->inbuf, (int)sendsize, 0);
      if (e < 0)
      {
         e = t_errno(tcpclient->sock);
         if (e == EWOULDBLOCK)
         {
            tk_yield();
            continue;
         }
         ns_printf(tcpclient->pio,"error %d sending TCP echo number %ld\n", 
            e, tcpclient->send_cnt);
         ns_printf(tcpclient->pio,"   on byte %ld\n", e);

         tcpclient->sending--;
         return TCPE_SEND_FAILED;
      }
      sent += e;
      if ((block += e) >= ECHOBUFSIZE)
      {
         ns_printf(tcpclient->pio, ".");
         block -= ECHOBUFSIZE;
      }
      tk_yield();    /* let other tasks run */
   }

   tcpclient->send_cnt++;  /* keep counters current */
   tcpclient->tot_sent += sent;
   ns_printf(tcpclient->pio, "\ntcpecho: echoed %ld bytes", sent);

   if (total > ECHOBUFSIZE)   /* see if we should print performance numbers */
   {
      u_long   endticks;
      u_long   bps;
      int   fraction;

      /* wait for all data to come back */
      endticks = cticks + (5*TPS);        /* use endticks for timeout */
      while ((tcpclient->tot_rcvd < tcpclient->tot_sent) &&
          (cticks > endticks))
      {
         tk_yield();
      }

      endticks = cticks - starttime;
      if (endticks > 0) /* avoid divide by 0 */
      {
         bps = sent/endticks;    /* bytes per tick */
         bps *= TPS;             /* convert to bytes/Sec */
         bps /= 1024;            /* KBytes/Sec */
         fraction = (int)endticks % TPS;
         endticks /= TPS;
         ns_printf(tcpclient->pio, " in %d and %d/%d seconds.\n%ld KBytes/sec", 
          (int)endticks, fraction, TPS, bps);
      }
   }
   ns_printf(tcpclient->pio, "\n");

   tcpclient->sending--;   /* clear reentry flag */
   return SUCCESS;
}


#ifdef TCP_ZEROCOPY


/* FUNCTION: echo_svr_upcall()
 *
 * echo_svr_upcall() - TCP ZEROCOPY callback for echo server socket.
 *
 * 
 * PARAM1: SOCKTYPE esrv_sock
 * PARAM2: PACKET inpkt
 * PARAM3: int err
 *
 * RETURNS: 
 */

int 
echo_svr_upcall(SOCKTYPE sock, PACKET inpkt, int err)
{
   int   e;

   if (sock != esvr_sock)
   {
      dtrap();
   }

   if (err)
   {
      if (inpkt)
         tcp_pktfree(inpkt);
      /* If this is a graceful close from the client then start our 
       * close too, else just print a message to console.
       */
      if (err == ESHUTDOWN)
      {
         /* tell tcp_echo_recv() to close the socket */
         esvr_sock_close = TRUE;
      }
      else
         dprintf("echo server callback error %d\n", err);
      e = 0;   /* should this be an error??? later */
   }
   else     /* return echo */
   {
      e = tcp_xout(sock, inpkt);
      if (e > 0)  /* ignore codes like ENP_SEND_PENDING */
         e = 0;
   }
   return e;
}
#endif   /* TCP_ZEROCOPY */




/* FUNCTION: tcp_echo_recv()
 *
 * tcp_echo_recv() - check for and process received echo data
 * 
 * PARAM1: none
 *
 * RETURNS: void
 */


void
tcp_echo_recv(void)
{
   int   len;        /* length of recv data */
   int   e;          /* error holder */
   unsigned i;       /* generic index */
   int   count;      /* select return */
   fd_set fd_recv;   /* fd for recv */
   fd_set fd_accept; /* fd for accept */
   TCPCLIENT tmpclient = tcpq;
   struct sockaddr_in client;
   SOCKTYPE tmpsock; /* scratch socket */

   if (elisten_sock == INVALID_SOCKET && tcpq == NULL)
      return;  /* Echo not set up, don't bother */

#ifdef USE_FDS
   FD_ZERO(&fd_recv);
   FD_ZERO(&fd_accept);
#endif

   /* select on all open data sockets */
   i = 0;
   count = 0;
#ifdef USE_FDS
   while (tmpclient)
   {
      if (tmpclient->sock != INVALID_SOCKET)
      {
         FD_SET(tmpclient->sock, &fd_recv);
         i++;
      }
      tmpclient=tmpclient->next;
   }
#else
   while (tmpclient)
   {
      if (tmpclient->sock != INVALID_SOCKET)
         fd_recv.fd_array[i++] = tmpclient->sock ;
      tmpclient=tmpclient->next;
   }
#endif  /* USE_FDS */

#ifndef TCP_ZEROCOPY
   /* if we need to listen for server receives too */
#ifdef USE_FDS
   if (tcpecho_server && (esvr_sock != INVALID_SOCKET))
   {
      FD_SET(esvr_sock, &fd_recv);
   }
#else
   if (tcpecho_server)
   {
      if (esvr_sock != INVALID_SOCKET)
         fd_recv.fd_array[i++] = esvr_sock;
   }
#endif  /* USE_FDS */
#else
   /* if we need to close the server's active socket */
   if (esvr_sock_close != FALSE)
   {
      if (esvr_sock != INVALID_SOCKET)
      {
         socketclose(esvr_sock);
         esvr_sock = INVALID_SOCKET;
      }
      esvr_sock_close = FALSE;
   }
#endif   /* TCP_ZEROCOPY */

#ifndef USE_FDS
   fd_recv.fd_count = i;
#endif

   /* make this a short timeout since elisten may create soon */
   if (elisten_sock != INVALID_SOCKET)
   {
#ifdef USE_FDS
      FD_SET(elisten_sock, &fd_accept);
#else
      fd_accept.fd_array[0] = elisten_sock;
      fd_accept.fd_count = 1;
#endif  /* USE_FDS */
      count = t_select(&fd_recv, NULL, &fd_accept, 1);
   }
   else 
   {
      if (i)   /* if no fd_set sockets filled in, don't bother */
         count = t_select(&fd_recv, NULL, NULL, 1);
   }

   /* While the t_select() was executing, commands can be 
    * executed from cmd-prompt and sockets can be cleaned up. 
    * Check for that. 
    */
   if (elisten_sock == INVALID_SOCKET && tcpq == NULL)
      return;  /* Echo not set up, don't bother */

   for (i = 0; i < fd_recv.fd_count; i++)
   {
#ifdef USE_FDS
      tmpsock = FD_GET(i, &fd_recv);
      if (tmpsock == INVALID_SOCKET)
         continue;
#else
      tmpsock = fd_recv.fd_array[i];
#endif  /* USE_FDS */

      /* Find out the client connection corresponding to this socket */
      tmpclient = tcp_client_from_sock(tmpsock);

      /* try a receive. Pick buffer according to client or server */
      if (tmpclient)    /* found a client for this one */
         len = recv(tmpsock, tmpclient->inbuf, ECHOBUFSIZE, 0);
#ifndef TCP_ZEROCOPY
      else if (tmpsock == esvr_sock)
         len = recv(tmpsock, srv_inbuf, ECHOBUFSIZE, 0);
#endif   /* TCP_ZEROCOPY */
      else
      {
         continue;
      }

      if (len < 0)
      {
         e = t_errno(tmpsock);
         if (e != EWOULDBLOCK)
         {
            if (tmpsock != esvr_sock)
               ns_printf(tmpclient->pio,"TCP echo recv error %d\n", e);
            else
               ns_printf(NULL,"TCP echo recv error %d\n", e);
         }
      }
      else if (len == 0)
      {
         ns_printf(NULL,"TCPECHO:socket closed by other side\n");
         if (tmpsock == esvr_sock)
         {
            socketclose (tmpsock);
            esvr_sock = INVALID_SOCKET;
         }
         else
         {
            if (tmpclient == NULL)
            {
               dtrap();
            }          
            else
            {
               tmpclient->sock = INVALID_SOCKET ;
               tcp_client_del(tmpclient);
            }
         }
      }
      else  /* if (len > 0) - got some echo data */
      {
#ifndef TCP_ZEROCOPY
         if (tmpsock == esvr_sock)
         {
            /* we must be server, send echo reply */
            if (tcpecho_server)
            {
               e = send(esvr_sock, srv_inbuf, len, 0);
               if (e < 0)
               {
                  /* Print the error to console */
                  e = t_errno(esvr_sock);
                  ns_printf(NULL,
                   "TCP echo server, error %d sending reply\n", e);
               }
            }
         }
         else     /* not the server socket, must be client */
#endif   /* TCP_ZEROCOPY */
         {
            /* If not a bulk test, print info */
            if (tmpclient->len <= ECHOBUFSIZE)
            {
               ns_printf(tmpclient->pio,"TCP echo reply from:%s, len:%d, reply:%lu", 
                print_ipad(tmpclient->rhost), len, tmpclient->replies);
               ns_printf(tmpclient->pio,"\n%s",prompt);
            }
            else
            {
               u_long   dataval;
               u_long * rxbuf =  (u_long*)(tmpclient->inbuf);
               u_long * rxend =  (u_long*)(tmpclient->inbuf +  (len  &  ~3));

               dataval = tmpclient->tot_rcvd/4;

               /* adjust for odd sized previous receives */
               if (tmpclient->tot_rcvd & 3)
               {
                  MEMMOVE(rxbuf, tmpclient->inbuf + (len & 3), len);
                  rxend--;
                  dataval++;     /* ignore sliced word */
               }

               while (rxbuf < rxend)
               {
                  if (*rxbuf != dataval)
                  {
                     ns_printf(tmpclient->pio,
                      "tcp_echo data error; got %lu, expected %lu\n",
                      *rxbuf, dataval);
                  }
                  rxbuf++;
                  dataval++;
               }
            }
            tmpclient->replies++;
            tmpclient->tot_rcvd += len;
         }
      }
   }

   /* if no server listen to poll, return now */
   if (elisten_sock == INVALID_SOCKET)
      return;

#ifdef NOTDEF
   MEMSET(&client, 0, sizeof(client));
   client.sin_family = AF_INET;
   client.sin_addr.s_addr = INADDR_ANY;
   client.sin_port = htons(ECHO_PORT);
#endif

   /* check for received echo connection on server */
   len = sizeof(client);
   tmpsock = accept(elisten_sock, (struct sockaddr*)&client, &len);
   if (tmpsock != INVALID_SOCKET)
   {
      if (esvr_sock == INVALID_SOCKET)
      {
         esvr_sock = tmpsock;
#ifdef TCP_ZEROCOPY
         t_setsockopt(esvr_sock, SOL_SOCKET, SO_CALLBACK, (void*)echo_svr_upcall, 0);
#endif   /* TCP_ZEROCOPY */
      }
      else  /* we already have a connection */
      {
         dprintf("tcpecho: rejected extra connection\n");
         socketclose(tmpsock);   /* refuse to serve another */
      }
   }
}



/* FUNCTION: tcp_echo_poll()
 * 
 * Do routine processing for TCP  Echos.
 * 1. Check is Echo packets has been received for TCP Echo 
 *    Server. If yes, send a reply.
 * 2. Check if Echo replys have been recieved for TCP Echo Clients.
 *    If yes, update data structures, display messages.
 *
 * PARAM1: void
 *
 * RETURNS: 
 */

static int in_echopoll = 0;   /* re-entry flag */

void
tcp_echo_poll(void)
{
   TCPCLIENT tmpclient = tcpq;

   if (elisten_sock == INVALID_SOCKET && tcpq == NULL)
      return;  /* Echo not set up, don't bother */

   in_echopoll++;    /* don't re-enter from tk_yield() */
   if (in_echopoll != 1)
   {
      in_echopoll--;
      return;
   }
   tcp_echo_recv();

   /* Check if we need to send TCP Echo packets */
   tmpclient=tcpq;
   while (tmpclient)
   {
      if (tmpclient->state == TCP_BUSY)
      {
         if (tmpclient->send_cnt  >= tmpclient->times)
         {
            /* Previous "techo" command has completed */
            tmpclient->state = TCP_IDLE ;
         }
         else
         {
            if (tmpclient->replies)
            {
               if (tcp_send_an_echo(tmpclient) == SUCCESS)
                  ns_printf(tmpclient->pio,"%s",prompt);
            }
         }
      }

      if (tmpclient->ticks + (TCP_IDLE_TIMEOUT*TPS) < cticks)
      {
         /* This client has been lying around ldle for a long time */
         ns_printf(tmpclient->pio,"Deleting idle TCP Echo Client.\n%s",prompt);
         tcp_client_del(tmpclient);
      }


      tmpclient=tmpclient->next;
   }

   in_echopoll--;
}


/* FUNCTION: tcp_echo_stats()
 * 
 * Show statistics about all TCP Echo Clients and Servers
 *
 * PARAM1: void * pio - device for console output
 *
 * RETURNS: 0 on SUCCESS or error number. 
 */

int
tcp_echo_stats(void * pio)
{
   ns_printf(pio,"Showing TCP Echo statistics.\n");
   if (esvr_sock == INVALID_SOCKET)
   {
      ns_printf(pio,"   There are no Server connections.\n");
   }
   else
   {
      ns_printf(pio,"   There is one Server connection.\n");
   }

   if (tcpq == NULL)
      ns_printf(pio,"   There are no Client connections.\n");
   else
   {
      TCPCLIENT tmpclient=tcpq;
      int   cnt=0;
      while (tmpclient)
      {
         cnt++;
         ns_printf(pio,"   Total bytes for Client %d: sent=%ld,rcvd=%ld\n",
          cnt, tmpclient->tot_sent, tmpclient->tot_rcvd);
         tmpclient=tmpclient->next;
      }
      ns_printf(pio,"   Total Client connections=%d.\n",cnt);
   }
   return SUCCESS;
}


#endif   /* TCP_ECHOTEST */



