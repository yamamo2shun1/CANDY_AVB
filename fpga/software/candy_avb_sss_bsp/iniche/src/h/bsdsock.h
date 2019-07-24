/* FILENAME: bsdsock.h
 *
 * Copyright 2000 InterNiche Technologies Inc.  All rights reserved.
 *
 * BSD sockets porting aid
 *
 * #define BSD_SOCKETS in your ipport.h_h, and #include this in your
 * application source to get the BSD-flavored sockets API.
 *
 * Then you get:
 *   BSD_SOCKET as a retargetable "socket" type
 *   BSD_SIZE_T as a retargetable "size" type
 *   BSD_TIMEVAL_T as a retargetable "struct timeval" type
 *   bsd_...() retargetable functions and macros for the BSD sockets API
 *
 * ...so you can write your code using these, and then re-target 
 * them to various sockets-like platforms (we supply the InterNiche
 * target, but these should be straightforward for modern Un*x 
 * and mostly so for Winsock).
 *
 * MODULE: misclib
 * ROUTINES: bsd_bind(), bsd_close(), bsd_connect(), bsd_errno(), 
 *           bsd_listen(), bsd_recv(), bsd_send(), bsd_sendto(), 
 *           bsd_shutdown(), bsd_socket()
 * PORTABLE: yes
 *
 */

#ifndef BSDSOCK_H
#define BSDSOCK_H

#include "tcpport.h"
#include "socket.h"
#include "sockcall.h"
#include "nptcp.h"

/*
 * BSD_SOCKET - the type of a socket.  For InterNiche Release 1.x (x
 * <= 7) sockets this is "long", so that is the default.  For BSD it
 * would be "int".  For Winsock it would be "SOCKET".  Don't change
 * this, override this default with your own macro prior to including
 * this file.  
 */
#ifndef BSD_SOCKET
#define BSD_SOCKET long
#endif

/*
 * BSD_SIZE_T - the "size" type.  For InterNiche Release 1.x (x <= 7)
 * this is "int".  
 */
#ifndef BSD_SIZE_T
#define BSD_SIZE_T int
#endif

/*
 * BSD_IN_ADDR_T - the "Internet Address" type.  Must be able to 
 * hold a 32-bit IPv4 address.
 */
#ifndef BSD_IN_ADDR_T
#define BSD_IN_ADDR_T unsigned long
#endif

/*
 * BSD_TIMEVAL_T - the "struct timeval" type.  bsd_select() needs a
 * structure with these fields for its timeout parameter.
 */
#ifndef BSD_TIMEVAL_T
struct timeval
{
   long tv_sec;     /* seconds */
   long tv_usec;    /* and microseconds */
};
#define BSD_TIMEVAL_T struct timeval
#endif

/*
 * ioctl() request definitions
 *
 * The only ioctl() request presently supported is FIONBIO, so 
 * rather than define the whole _IOC macro family we just 
 * define FIONBIO.
 */
#define FIONBIO SO_NONBLOCK

/*
 * Some BSD sockets calls can be mapped to InterNiche sockets calls
 * with straightforward macro definitions, so we do.
 */
#define bsd_bind(s, addr, addrlen)     t_bind((s), (addr), addrlen)
#define bsd_close(s)                   t_socketclose((s))
#define bsd_connect(s, addr, addrlen)  t_connect((s), (addr), addrlen)
#define bsd_listen(s, backlog)         t_listen((s), (backlog))
#define bsd_recv(s, buf, len, flags)   t_recv((s), (buf), (len), (flags))
#define bsd_send(s, buf, len, flags)   t_send((s), (buf), (len), (flags))
#define bsd_sendto(s, msg, len, flags, to, tolen) \
            t_sendto((s), (msg), (len), (flags), (to), tolen)
#define bsd_shutdown(s, how)           t_shutdown((s), (how))
/* 
 * Altera Niche Stack Nios port modification:
 * fix build warning
 */
#ifdef ALT_INICHE
#define bsd_socket(dom, type, proto)   t_socket((dom), (type), (proto))
#else
#define bsd_socket(dom, type, proto)   t_socket((dom), (type), 0)
#endif
/*
 * BSD errno isn't the best choice for a multithreaded environment.
 * The InterNiche sockets API provides a per-socket error which can be
 * read via t_errno(s).  We suppose this could be re-targeted to errno
 * for a Unix-like environment (or perhaps better, the SO_ERROR socket
 * option), or WSAGetLastError() for a Winsock environment.  
 */
#define bsd_errno(s)                   t_errno((s))

/*
 * Some other BSD sockets calls require functions to do the mapping
 * to InterNiche sockets calls; we provide the prototypes for those
 * functions here, and the implementations in misclib/bsdsock.c.
 */
BSD_SOCKET bsd_accept(BSD_SOCKET s, 
                      struct sockaddr * addr, int * addrlen);
int bsd_getpeername(BSD_SOCKET s,
                    struct sockaddr * addr, int * addrlen);
int bsd_getsockname(BSD_SOCKET s,
                    struct sockaddr * addr, int * addrlen);
int bsd_getsockopt(BSD_SOCKET s,
                   int level,
                   int name,
                   void * opt, int * optlen);
BSD_IN_ADDR_T bsd_inet_addr(char * cp);
int bsd_inet_aton(char * cp, struct in_addr * pin);
char * bsd_inet_ntoa(struct in_addr in);
int bsd_ioctl(BSD_SOCKET s, unsigned long request, ...);
int bsd_recvfrom(BSD_SOCKET s,
                 void * buf,
                 BSD_SIZE_T len,
                 int flags,
                 struct sockaddr * from, int * fromlen);
#ifdef SO_SELECT
int bsd_select(int nfds,
               fd_set * readfds,
               fd_set * writefds,
               fd_set * exceptfds,
               struct timeval * timeout);
#endif /* SO_SELECT */
int bsd_setsockopt(BSD_SOCKET s,
                   int level,
                   int name,
                   void * opt, int optlen);

#endif  /* BSDSOCK_H */

