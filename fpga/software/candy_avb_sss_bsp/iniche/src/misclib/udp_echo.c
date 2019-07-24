/*
 * FILENAME: udp_echo.c
 *
 * Copyright 1995 - 2002 By InterNiche Technologies Inc. All rights reserved
 *
 * Test program for UDP sockets API. Implements UDP echo 
 * server and client DESIGN DESCRIPTION The structure UdpClient is 
 * used to maintain per-connection information. (connection is a 
 * misnomer here. Better word can be operation/transaction).
 *
 * MODULE: MISCLIB
 *
 * ROUTINES: udp_client_add(), udp_client_del(), 
 * ROUTINES: udp_client_from_pio(), udp_cecho_init(), udp_secho_init(), 
 * ROUTINES: udp_cecho_close(), udp_secho_close(), udp_sendecho(), 
 * ROUTINES: udp_send_an_echo(), udp_echo_poll(), udp_echo_init(), 
 * ROUTINES: udp_echo_cleanup(), udp_echo_stat
 *
 * PORTABLE: yes
 */

#include "ipport.h"     /* NetPort embedded system includes */

#ifdef  UDPSTEST        /* whole file can be ifdefed out */

#include "tcpport.h"    /* NetPort embedded system includes */
#include "in_utils.h"


#define  ECHO_PORT               7  /* standard UDP/TCP echo port */

#define  UDP_IDLE                1
#define  UDP_BUSY                2
#define  UDP_IDLE_TIMEOUT        600   /* value in seconds */
                                      /* If a UDP Echo client has not been
                                         used for this long, delete it */
SOCKTYPE es_sock =              INVALID_SOCKET; /* echo server socket */

extern   int                    kbhit(void);/* from Microsquash|Borland lib*/
extern   ip_addr                 activehost; /* host for ping, echo, etc.   */
extern   char *                  prompt;

static   char *   echodata =    "UDP echo number:            ";

/* List of error codes used by this file */
#define  UDPE_BASE                  300
#define  UDPE_BAD_SOCKET            (UDPE_BASE+1)
#define  UDPE_BIND_FAILED           (UDPE_BASE+2)
#define  UDPE_CONNECT_FAILED        (UDPE_BASE+3)
#define  UDPE_ALLOC_ERROR           (UDPE_BASE+4)
#define  UDPE_SRV_BAD_SOCKET        (UDPE_BASE+5)
#define  UDPE_SRV_BIND_FAILED       (UDPE_BASE+6)
#define  UDPE_NO_CONN_FOUND         (UDPE_BASE+7)
#define  UDPE_NODE_NOT_FOUND        (UDPE_BASE+8)
#define  UDPE_SRV_CLOSE_ERR         (UDPE_BASE+9)
#define  UDPE_CLIENT_EXISTS         (UDPE_BASE+10)
#define  UDPE_ERR_SENDING_ECHO      (UDPE_BASE+11)
#define  UDPE_TIME_NOT_RIPE         (UDPE_BASE+12)

#define  SUCCESS                    0

/* Structure for holding information regarding a UDP Client connection */

struct UdpClient
{
   SOCKTYPE sock;             /* client socket   */
   void *   pio;              /* Output messages go to this device */
   ip_addr  rhost;            /* UDP Echos to be sent to this host   */
   u_long   replies;          /* Number of replies received  */
   u_long   times;            /* Number of times ECHO is to be sent  */
   u_long   send_cnt ;        /* Number of the reply to be sent(counter)  */
   int      state;            /* state of the UDP connection   */
   u_long   ticks;            /* ClockTick when next EchoPkt is to be sent */
   u_long   delay;            /* Delay between two Echo packets   */
   int      len;              /* Lenght of UDP pkt to be sent  */
   u_long   tot_sent;         /* Total echo pkts sent */
   u_long   tot_rcvd;         /* Total echo pkts received   */
   struct   UdpClient * next; /* Pointer to the next client connection  */
};

typedef struct UdpClient * UDPCLIENT ;

UDPCLIENT udpq;                  /* Start node for queue of UDP clients */

int      udp_client_add(void * pio, SOCKTYPE sock, ip_addr host);
int      udp_client_del(UDPCLIENT udpclient);
UDPCLIENT udp_client_from_pio(void * pio);
int      udp_cecho_init(void * pio);
int      udp_secho_init(void * pio);
int      udp_cecho_close(void * pio);
int      udp_secho_close (void * pio);
int      udp_sendecho(void * pio, ip_addr rhost, int len, long times);   
int      udp_send_an_echo(UDPCLIENT udpclient);
void     udp_echo_poll(void);
int      udp_echo_init(void);
void     udp_echo_cleanup(void);
int      udp_echo_stats(void * pio);

static int in_udpechoq = 0;   /* re-entry/queue protection flag */


/* FUNCTION: udp_client_add()
 *
 * Add a new client to the Queue. Allocates memory for a new node,
 * Adds to node to queue,  initializes data members
 *
 * PARAM1: void * pio      - GenericIO structure (IN) 
 * PARAM2: SOCKTYPE sock   - Client Socket (IN)
 * PARAM3: ip_addr host    - Host to whom echo packets are to be sent 
 *
 * RETURNS:  SUCCESS or error number 
 */

int
udp_client_add(void * pio, SOCKTYPE sock, ip_addr host)
{
   UDPCLIENT tmpclient;

   in_udpechoq++;

   /* Allocate memory */
   tmpclient = (UDPCLIENT) npalloc( sizeof(struct UdpClient) ) ;
   if ( tmpclient == NULL )
   {
      ns_printf(pio,"Allocation error.\n");
      in_udpechoq--;
      return UDPE_ALLOC_ERROR;
   }

   /* Add the new node to the queue */
   if ( udpq == NULL )
   {
      udpq = tmpclient ;
      tmpclient->next=NULL;
   }
   else
   {
      /* Insert it at the beginning */
      tmpclient->next = udpq;
      udpq=tmpclient ;
   }

   /* Initialize data members */
   tmpclient->sock      = sock;        /* UDP Echo Client's socket           */
   tmpclient->pio       = pio;      /* Client's I/O device                */
   tmpclient->rhost     = host;     /* UDP Echo Server's IP addr          */
   tmpclient->times     = 1;        /* Count of total EchoPkts to be sent */
   tmpclient->send_cnt  = 0;        /* Num of Echo Pkts sent till now     */
   tmpclient->ticks     = cticks;   /* Timetick to track sending of echos */
   tmpclient->state     = UDP_IDLE ;   /* Are we sending UDP Echos? No.      */
   tmpclient->replies   = 0 ;       /* Num of replies received so far     */
   tmpclient->len       = 64 ;      /* Default value of Echo Pkt          */
   tmpclient->tot_sent  = 0;
   tmpclient->tot_rcvd  = 0;

   in_udpechoq--;

   return SUCCESS ;
}


/* FUNCTION: udp_client_del()
 *
 * Purpose : Cleanup stuff for a UDP Echo 
 * Client Three things to be done here 1. Close the underlying socket 
 * 2. Remove node from the Q 3. Free the memory used by the node
 *
 * PARAM1: UDPCLIENT udpclient - Pointer to UDP Echo Client connection 
 *
 * RETURNS: SUCCESS or error number 
 */

int 
udp_client_del(UDPCLIENT udpclient)
{
   int   e;
   UDPCLIENT tmpclient,prevclient;

   in_udpechoq++;

   if ( udpclient->sock != INVALID_SOCKET )
   {
      /* Close the underlying socket */
      e = socketclose(udpclient->sock);
      if (e)
      {
         /* socketclose() must have free'd the socket. Hence we
          * can't do
          * e = t_errno(udpclient->sock);
          */
         ns_printf(udpclient->pio,"udp echo: close error %d\n", e);
      }
   }

   /* Remove from the q */
   if ( udpq == udpclient )
   {
      /* It is the first node */
      udpq = udpq->next;
   }
   else
   {
      prevclient=udpq;
      tmpclient=udpq->next;
      while ( tmpclient )
      {
         if ( tmpclient == udpclient )
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
      if (tmpclient == NULL )
      {
         /* Node not found in Q !! */
         dtrap();
         in_udpechoq--;
         return UDPE_NODE_NOT_FOUND ;
      }
   }

   npfree(udpclient);

   in_udpechoq--;   
   return SUCCESS;
}


/* FUNCTION: udp_client_from_pio()
 *
 * Return the UDP Echo Client connect for a particular session
 *
 * PARAM1: void * pio
 *
 * RETURNS: Pointer to UDPCLIENT; or NULL if connection not found
 */

UDPCLIENT
udp_client_from_pio(void * pio)
{
   UDPCLIENT tmpclient=udpq;

   while ( tmpclient )
   {
      if ( tmpclient->pio == pio )
         break ;
      else
         tmpclient=tmpclient->next;
   }

   return tmpclient;
}



/* FUNCTION: udp_cecho_init()
 *
 *  Do the initialization for a 
 * UDP Echo Client. (for the session identified by pio)
 *
 * PARAM1: void * pio
 *
 * RETURNS: SUCCESS  or error number 
 */

int 
udp_cecho_init(void * pio)
{
   int   e;    /* error holder */
   struct sockaddr_in   me;   /* my IP info, for bind() */
   struct sockaddr_in   him;  /* server's IP info, for client connect() */
   SOCKTYPE sock;

   UDPCLIENT udpclient;

   udpclient=udp_client_from_pio(pio);
   if (udpclient != NULL )
   {
      ns_printf(pio,"UDP Echo Client has already been started.\n");
      return UDPE_CLIENT_EXISTS ;
   }

   ns_printf(pio,"udp echo client is starting.\n");

   /* open UDP socket */
   sock = socket(AF_INET, SOCK_DGRAM, 0);
   if (sock == INVALID_SOCKET)
   {
      ns_printf(pio,"udp_echo: bad socket: %d\n", sock);
      return UDPE_BAD_SOCKET ;
   }

   me.sin_family = AF_INET;
   me.sin_addr.s_addr = INADDR_ANY;
   me.sin_port = 0;  /* let UDP pick a client port */

   e = bind(sock, (struct sockaddr*)&me, sizeof(me));
   if (e != 0)
   {
      e = t_errno(sock);
      ns_printf(pio,"udp_echo: bind error: %d\n", e);
      socketclose(sock);
      return UDPE_BIND_FAILED;
   }

   /* make client socket a connected socket */
   him.sin_family = AF_INET;
   if (activehost)
      him.sin_addr.s_addr = activehost;
   else
      him.sin_addr.s_addr = 0x0100007f;   /* for testing */

   him.sin_port = htons(ECHO_PORT);
   e = connect(sock, (struct sockaddr *)&him, sizeof(him));
   if (e != 0)
   {
      e = t_errno(sock);
      ns_printf(pio,"udp_echo: client connect error: %d\n", e);
      return UDPE_CONNECT_FAILED;
   }

   /* put socket into non-blocking mode */
   setsockopt(sock, SOL_SOCKET, SO_NBIO, NULL, 0);

   udp_client_add(pio,sock,activehost);

   return SUCCESS;
}



/* FUNCTION: udp_secho_init()
 *
 * Initialize the UDP Echo server ( to listen to UDP port 7)
 *
 * PARAM1: void * pio
 *
 * RETURNS: SUCCESS or error number
 */

int 
udp_secho_init(void * pio)
{
   int   e;    /* error holder */
   struct sockaddr_in   me;   /* my IP info, for bind() */
   SOCKTYPE sock;

   if ( es_sock != INVALID_SOCKET )
   {
      ns_printf(pio,"udp echo server is already running.\n" );
      return SUCCESS;
   }
   ns_printf(pio,"udp echo server is starting.\n" );

   /* open UDP socket */
   sock = socket(AF_INET, SOCK_DGRAM, 0);
   if (sock == INVALID_SOCKET)
   {
      ns_printf(pio,"udp_echo: bad socket: %d\n", sock);
      return UDPE_SRV_BAD_SOCKET;
   }

   me.sin_family = AF_INET;
   me.sin_addr.s_addr = INADDR_ANY;
   me.sin_port = htons(ECHO_PORT);

   e = bind(sock, (struct sockaddr*)&me, sizeof(me));
   if (e != 0)
   {
      e = t_errno(sock);
      ns_printf(pio,"udp_echo: bind error: %d\n", e);
      socketclose(sock);
      return UDPE_SRV_BIND_FAILED;
   }

   es_sock = sock;

   /* put socket into non-blocking mode */
   setsockopt(sock, SOL_SOCKET, SO_NBIO, NULL, 0);

   return SUCCESS;
}


/* FUNCTION: udp_cecho_close()
 *
 * Close the UDP Echo Client for a particular session.
 * This function in turn calls udp_client_del().
 *
 * PARAM1: void * pio
 *
 * RETURNS: SUCCESS or error number 
 */

int
udp_cecho_close(void * pio)
{
   UDPCLIENT udpclient;

   udpclient=udp_client_from_pio(pio);
   if (udpclient == NULL )
   {
      ns_printf(pio,"echo socket not open\n");
      return UDPE_NO_CONN_FOUND;
   }

   ns_printf(pio,"udp echo - closing client socket\n");

   udp_client_del(udpclient);

   return SUCCESS;
}


/* FUNCTION: udp_secho_close()
 *
 * Cleanup the UDP Echo Server
 *
 * PARAM1: void * pio
 *
 * RETURNS: SUCCESS or error number 
 */

int
udp_secho_close(void * pio)
{
   int   e=0;
   SOCKTYPE sock;

   sock = es_sock;
   es_sock = INVALID_SOCKET;

   if ( sock != INVALID_SOCKET )
   {
      ns_printf(pio,"udp echo - closing server socket\n");
      e = socketclose(sock);
      if (e)
      {
         e = t_errno(sock);
         ns_printf(pio,"udp echo: close error %d\n", e);
      }
   }

   if ( e )
      return UDPE_SRV_CLOSE_ERR ;
   else
      return SUCCESS ;
}


/* FUNCTION: udp_sendecho()
 *
 * Send UDP Echo packets to a remote host. When a UDP Echo Client is 
 * initialized for a session, rhost field is set to the global 
 * variable activehost. If this function is called for a different 
 * rhost, then 1. The previous client connection is closed. 2. A new 
 * client connection is started for this new rhost.
 *
 * PARAM1: void * pio
 * PARAM2: ip_addr rhost   - IP Address of remote host
 * PARAM3: int len         - Length of packet to be sent
 * PARAM4: long times      - Number of times packet is to be sent
 *
 * RETURNS: SUCCESS or error number 
 */

int
udp_sendecho(void * pio,
   ip_addr rhost,    /* already in net endian */
   int   len,        /* length to send */
   long  times)      /* packet length, number of times to send */
{
   int   e;
   UDPCLIENT udpclient;

   udpclient=udp_client_from_pio(pio);

   if (udpclient == NULL || rhost != udpclient->rhost)
   {
      if ( udpclient == NULL )
      {
         ns_printf(pio,"echo socket not open. Opening....\n");
      }
      else
      {
         ns_printf(pio,"host changed, restarting client socket \n");
         udp_client_del(udpclient);
      }

      activehost = rhost;
      e = udp_cecho_init(pio);
      if (e) 
         return e;

      udpclient=udp_client_from_pio(pio);
   }

   udpclient->replies   = 0;
   udpclient->times     = times;
   udpclient->delay     = pingdelay;
   udpclient->ticks     = cticks;
   udpclient->state     = UDP_BUSY ;
   udpclient->len       = len ;
   udpclient->send_cnt  = 0 ;

   return udp_send_an_echo(udpclient);
}


/* FUNCTION: udp_send_an_echo()
 *
 * Send an Echo packet for a UDP Echo Client.
 *
 * PARAM1: UDPCLIENT udpclient
 *
 * RETURNS: 0 if SUCCESS or error number
 */

int
udp_send_an_echo(UDPCLIENT udpclient)
{
   int   e;

   if ( udpclient->ticks > cticks )  
      return UDPE_TIME_NOT_RIPE ;

   udpclient->ticks += udpclient->delay;  /* Set time for sending next packet*/

   ns_printf(udpclient->pio,"sending UDP echo %ld to %s\n",
    udpclient->send_cnt,print_ipad(udpclient->rhost));

   sprintf(echodata + 17, "%-9lu", udpclient->send_cnt );
   e = t_send(udpclient->sock, echodata, udpclient->len, 0);
   if (e != udpclient->len)
   {
      e = t_errno(udpclient->sock);
      ns_printf(udpclient->pio,"error %d sending UDP echo number %ld\n", e, 
       udpclient->send_cnt );
      ns_printf(udpclient->pio,"UDP Echo Server at %s is possibly shut down\n",
         print_ipad(udpclient->rhost));
      udp_client_del(udpclient);
      return UDPE_ERR_SENDING_ECHO ;
   }

   udpclient->send_cnt ++;
   udpclient->tot_sent ++;

   return SUCCESS;
}


/* FUNCTION: udp_echo_poll()
 * 
 * Poll all the UDP Echo related activities. That includes a UDP
 * Echo Server and UDP Echo Clients for each I/O session.
 *
 * PARAM1: 
 *
 * RETURNS: void
 */

static char inbuf[TCP_MSS];

void
udp_echo_poll()
{
   struct sockaddr_in   him;  /* IP info of current rhost */
   int   len;
   int   e;
   UDPCLIENT tmpclient,nextclient;

   in_udpechoq++;    /* don't re-entry from tk_yield() */
   if (in_udpechoq != 1)
   {
      in_udpechoq--;
      return;
   }

   /* check for received echo packet */
   if (es_sock != INVALID_SOCKET)
   {
      int   sa_size = sizeof(him);
      len = recvfrom(es_sock, inbuf, TCP_MSS, 0, (struct sockaddr *)&him, &sa_size);
      if (len < 0)
      {
         e = t_errno(es_sock);
         if (e != EWOULDBLOCK)
            dprintf("UDP echo server socket error %d\n", e);
      }
      else if(len == 0)
      {
         dtrap(); /* socket closed? */
         udp_secho_close(NULL);
      }
      else  /* if(len > 0) */
         sendto(es_sock, inbuf, len, 0, (struct sockaddr *)&him, sizeof(him));
   }

   for (tmpclient = udpq; tmpclient; tmpclient = nextclient)
   {
      nextclient=tmpclient->next;

      /* check for received echo reply */
      len = t_recv(tmpclient->sock, inbuf, TCP_MSS, 0);
      if (len < 0)
      {
         e = t_errno(tmpclient->sock);
         if (e != EWOULDBLOCK)
            ns_printf(tmpclient->pio,"UDP echo client socket error %d\n", e);
      }
      else if(len == 0)
      {
         dtrap(); /* socket closed? */
         tmpclient->sock=INVALID_SOCKET;
         udp_client_del(tmpclient);
         continue;
      }
      else  /* if(len > 0) - got some echo data */
      {
         ns_printf(tmpclient->pio,"UDP echo reply; len:%d, reply:%lu", 
          len, tmpclient->replies);
         tmpclient->replies++;
         tmpclient->tot_rcvd++;
         /* see if it's one of ours, print number if so */
         if (strncmp(inbuf, echodata, 16) == 0)
            ns_printf(tmpclient->pio,", Our send#:%ld",atol((&inbuf[17])));
         ns_printf(tmpclient->pio,"\n%s",prompt);
      }

      if ( tmpclient->state == UDP_BUSY )
      {
         if ( tmpclient->send_cnt  >= tmpclient->times )
         {
            /* Previous "udecho" command has completed */
            tmpclient->state = UDP_IDLE ;
         }
         else
         {
            if ( udp_send_an_echo(tmpclient) == SUCCESS )
               ns_printf(tmpclient->pio,"%s",prompt);
         }
      }
      if ( tmpclient->ticks + (UDP_IDLE_TIMEOUT*TPS) < cticks )
      {
         /* This client has been lying around ldle for a long time */
         ns_printf(tmpclient->pio,"Deleting idle UDP Echo Client.\n%s",prompt);
         udp_client_del(tmpclient);
      }
   }

   in_udpechoq--;
}



/* FUNCTION: udp_echo_init()
 * 
 * Do the initialization for UDP Echo - start UDP Echo Server
 *
 * PARAM1: void
 *
 * RETURNS: 0 on SUCCESS or error number 
 */

int
udp_echo_init(void)
{
   return udp_secho_init(NULL);
}



/* FUNCTION: udp_echo_cleanup()
 * 
 * Clean up UDP Echo Server and UDP Echo Clients.
 *
 * PARAM1: void
 *
 * RETURNS: 0 on SUCCESS or error number 
 */

void
udp_echo_cleanup(void)
{
   UDPCLIENT tmpclient=udpq;
   UDPCLIENT nextclient;

   /* Close the Echo Server sockets */
   udp_secho_close(NULL);

   /* Do the cleanup for each client connection */
   while ( tmpclient )
   {
      nextclient=tmpclient->next;
      udp_client_del(tmpclient);
      tmpclient=nextclient;
   }
}



/* FUNCTION: udp_echo_stats()
 * 
 *  Show statistics about all UDP Echo Clients and Servers
 *
 * PARAM1: void * pio
 *
 * RETURNS: 0 on SUCCESS or error number
 */

int
udp_echo_stats(void * pio)
{
   ns_printf(pio,"Showing UDP Echo statistics.\n");
   if ( es_sock == INVALID_SOCKET )
   {
      ns_printf(pio,"   There are no Server connections.\n");
   }
   else
   {
      ns_printf(pio,"   There is one Server connection.\n");
   }

   if ( udpq == NULL )
      ns_printf(pio,"   There are no Client connections.\n");
   else
   {
      UDPCLIENT tmpclient=udpq;
      int   cnt=0;
      while (tmpclient)
      {
         cnt++;
         ns_printf(pio,"   Total pkts for Client %d: sent=%ld,rcvd=%ld\n",
          cnt,tmpclient->tot_sent,tmpclient->tot_rcvd);
         tmpclient=tmpclient->next;
      }
      ns_printf(pio,"   Total Client connections=%d.\n",cnt);
   }
   return SUCCESS;
}

#endif      /* UDPSTEST */

