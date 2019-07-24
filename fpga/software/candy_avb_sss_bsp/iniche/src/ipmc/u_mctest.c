/*
 * FILENAME: u_mctest.c
 *
 * Copyright 1997- 2000 By InterNiche Technologies Inc. All rights reserved
 *
 *
 * MODULE: IPMC
 *
 * ROUTINES: u_mctest_init(), u_mctest_run()
 *
 * PORTABLE: yes
 */

/*
 * u_mctest.c            (c) Bob Quinn           2/4/97
 *
 * Released to the public domain
 *
 * Description:
 *  Tries out BSD-compatible Multicast APIs, and sends multicast 
 *  messages in response to multicast messages received.  
 */
#include "ipport.h"

// altera changes begin (cadler@altera.com)
// IP_MULTI_UMCTEST now controlled from ipport.h
//#ifdef INCLUDE_TCP
//#define IP_MULTI_UMCTEST  /* define to enable u_mctest.c */
//#endif
// altera changes end (cadler@altera.com)

#ifdef IP_MULTI_UMCTEST
#ifdef IP_MULTICAST

#include "tcpport.h"
#include "menu.h"

#define  BUFSIZE     1024
#define  TTL_VALUE   2
#define  TEST_ADDR "234.5.6.7"
#define  TEST_ADDR1 "234.1.2.3"
#define  TEST_ADDR2 "234.4.5.6"
#define  TEST_PORT   3456
#define  LOOPMAX     3

extern   struct net  netstatic[MAXNETS];
extern   char * itoa(int, char *, int);
extern   u_long inet_addr(char far *);
int u_mctest_run(void * pio);

// altera changes begin (cadler@altera.com)
#ifdef IN_MENUS
// altera changes end (cadler@altera.com)
struct menu_op u_mctest_menu[]   =  {
   { "u_mctestc",  stooges,  "u_mctest client menu" },
   { "u_mctest", u_mctest_run, "run udp multicast test" },
   {  NULL },
};
// altera changes begin (cadler@altera.com)
#endif //IN_MENUS
// altera changes end (cadler@altera.com)


/* FUNCTION: u_mctest_init()
 * 
 * PARAM1: 
 *
 * RETURNS: 
 */

void u_mctest_init()
{
   printf("mctest init called\n");
// altera changes begin (cadler@altera.com)
#ifdef IN_MENUS
// altera changes end (cadler@altera.com)
   install_menu(u_mctest_menu);
// altera changes begin (cadler@altera.com)
#endif //IN_MENUS
// altera changes end (cadler@altera.com)
}


/* FUNCTION: u_mctest_run()
 * 
 * PARAM1: void * pio
 *
 * RETURNS: 
 */

int u_mctest_run(void * pio)
{
    struct sockaddr_in   stLocal, stTo, stFrom;
    char  achIn[BUFSIZE];
    char  achOut[] = "Message number:              ";
    long  s;
    int   i, iRet, iTmp;
    struct ip_mreq stMreq;
    u_char cTmp;
    unsigned long endtime;

   /* get a datagram socket */
   s = t_socket(AF_INET,SOCK_DGRAM, 0);

   if (s == INVALID_SOCKET) 
   {
      ns_printf (pio,"t_socket() failed, Err: %d\n", t_errno(s));
      exit(1);
   }

   /* avoid EADDRINUSE error on bind() */ 
   iTmp = 1;
   iRet = t_setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *)&iTmp, sizeof(iTmp));
   if (iRet == SOCKET_ERROR) 
   {
      ns_printf (pio,"t_setsockopt() SO_REUSEADDR failed, Err: %d\n",
       t_errno(s));
   }

   /* name the socket */
   stLocal.sin_family =   AF_INET;
   stLocal.sin_addr.s_addr = htonl(INADDR_ANY);
   stLocal.sin_port =   htons(TEST_PORT);
   iRet = t_bind(s, (struct sockaddr*) &stLocal, sizeof(struct sockaddr_in));
   if (iRet == SOCKET_ERROR) 
   {
      printf ("t_bind() failed, Err: %d\n",
       t_errno(s));
   }

   /* join the multicast group. TEST_ADDR */

   ns_printf (pio,"Joining multicast group: %s\n", TEST_ADDR);
   stMreq.imr_multiaddr = inet_addr(TEST_ADDR);
   stMreq.imr_interface = netstatic[0].n_ipaddr;
   iRet = t_setsockopt(s, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                       (char *)&stMreq, sizeof(stMreq));
   if (iRet == SOCKET_ERROR) 
   {
      ns_printf (pio,"t_setsockopt() IP_ADD_MEMBERSHIP failed, Err: %d\n",
                     t_errno(s));
   } 
   tk_yield();

   /* join the multicast group. TEST_ADDR1 */

   ns_printf (pio,"Joining multicast group: %s\n", TEST_ADDR1);
   stMreq.imr_multiaddr = inet_addr(TEST_ADDR1);
   stMreq.imr_interface = netstatic[0].n_ipaddr;
   iRet = t_setsockopt(s, IPPROTO_IP, IP_ADD_MEMBERSHIP, 
                       (char *)&stMreq, sizeof(stMreq));
   if (iRet == SOCKET_ERROR) 
   {
      ns_printf (pio,"t_setsockopt() IP_ADD_MEMBERSHIP failed, Err: %d\n",
                     t_errno(s));
   }
   tk_yield();

   /* join the multicast group. TEST_ADDR2 */

   ns_printf (pio,"Joining multicast group: %s\n", TEST_ADDR2);
   stMreq.imr_multiaddr = inet_addr(TEST_ADDR2);
   stMreq.imr_interface = netstatic[0].n_ipaddr;
   iRet = t_setsockopt(s, IPPROTO_IP, IP_ADD_MEMBERSHIP, 
                       (char *)&stMreq, sizeof(stMreq));
   if (iRet == SOCKET_ERROR) 
   {
      ns_printf (pio,"t_setsockopt() IP_ADD_MEMBERSHIP failed, Err: %d\n",
                     t_errno(s));
   }
   tk_yield();

   /* set TTL to traverse up to multiple routers */
   cTmp = TTL_VALUE;
   iRet = t_setsockopt(s, IPPROTO_IP, IP_MULTICAST_TTL, (char *)&cTmp, sizeof(cTmp));
   if (iRet == SOCKET_ERROR) 
   {
      ns_printf (pio,"t_setsockopt() IP_MULTICAST_TTL failed, Err: %d\n",
                     t_errno(s));
   }

   /* enable loopback */
   cTmp = 1;
   iRet = t_setsockopt(s, IPPROTO_IP, IP_MULTICAST_LOOP, (char *)&cTmp, sizeof(cTmp));
   if (iRet == SOCKET_ERROR) 
   {
      ns_printf (pio,"t_setsockopt() IP_MULTICAST_LOOP failed, Err: %d\n",
                     t_errno(s));
   }

   /* assign our destination address */
   stTo.sin_family =      AF_INET;
   stTo.sin_addr.s_addr = inet_addr(TEST_ADDR1);
   stTo.sin_port =        htons(TEST_PORT);
   ns_printf (pio,"Now sending to (and receiving from) multicast group: %s\n",
                  TEST_ADDR1);

   for (i = 0; i < LOOPMAX; i++)
   {
      static int iCounter = 1;

      /* send to the multicast address */
      sprintf(&achOut[16], "%d", iCounter++);
      iRet = t_sendto(s, achOut, sizeof(achOut), 0, (struct sockaddr*)&stTo,
                      sizeof(struct sockaddr_in));
      if (iRet < 0) 
      {
         /*      perror("sendto() failed\n"); */
         ns_printf (pio,"t_sendto() failed, Error: %d\n", t_errno(s));
         /*     exit(1); */
         goto exitloop;
      }

      /* make the socket non-blocking */
      iTmp = 1;
      iRet = t_setsockopt(s, SOL_SOCKET, SO_NONBLOCK, (char *)&iTmp, sizeof(iTmp));
      if (iRet == SOCKET_ERROR) 
      {
         ns_printf (pio,"t_setsockopt() SO_NONBLOCK (1) failed, Err: %d\n",
                        t_errno(s));
      }

      /* loop around for 5 seconds waiting to receive datagrams */
      endtime = cticks + (TPS * 5);

      while (cticks < endtime)
      {
         int sa_size = sizeof(struct sockaddr_in);
         tk_yield();

         iRet = t_recvfrom(s, achIn, BUFSIZE, 0, (struct sockaddr*)&stFrom, &sa_size);
         if (iRet < 0) 
         {
            if (t_errno(s) != EWOULDBLOCK)
            {
               /*      perror("recvfrom() failed\n"); */
               ns_printf (pio,"t_recvfrom() failed, Error: %d\n", t_errno(s));
               /*     exit(1); */
               goto exitloop;
            }
         }
         if (iRet > 0)
         {
            ns_printf(pio,"From host:%s port:%d, %s\n",
             print_ipad(stFrom.sin_addr.s_addr),
             ntohs(stFrom.sin_port), achIn);
         }
      }   /* end while (cticks...) */

      /* make the socket blocking */
      cTmp = 0;
      iRet = t_setsockopt(s, SOL_SOCKET, SO_NONBLOCK, (char *)&cTmp, sizeof(cTmp));
      if (iRet == SOCKET_ERROR) 
      {
         ns_printf (pio,"t_setsockopt() SO_NONBLOCK (0) failed, Err: %d\n",
                        t_errno(s));
      }
   }   /* end for(;;) */

exitloop:
   /* delete the multicast group. */
   stMreq.imr_multiaddr = inet_addr(TEST_ADDR);
   stMreq.imr_interface = netstatic[0].n_ipaddr;
   iRet = t_setsockopt(s, IPPROTO_IP, IP_DROP_MEMBERSHIP, (char *)&stMreq, sizeof(stMreq));
   if (iRet == SOCKET_ERROR) 
   {
      ns_printf (pio,"t_setsockopt() IP_DROP_MEMBERSHIP failed, Err: %d\n",
                     t_errno(s));
   }

#ifdef NOT_USED
#error t_shutdown is not used with UDP sockets
   iRet = t_shutdown(s, 1);
   if (iRet == SOCKET_ERROR) 
   {
      ns_printf (pio,"t_shutdown failed.  Err: %d\n", t_errno(s));
   }
#endif

   iRet = t_socketclose(s);
   if (iRet == SOCKET_ERROR) 
   {
      ns_printf (pio,"t_socketclose() failed.  Err: %d\n", t_errno(s));
   }

   return(0);
}  /* end main() */  

#endif   /* IP_MULTI_UMCTEST */
#endif   /* IP_MULTICAST */

