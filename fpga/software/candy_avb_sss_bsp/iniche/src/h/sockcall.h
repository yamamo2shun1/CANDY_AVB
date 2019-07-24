/*
 * FILENAME: sockcall.h
 *
 * Copyright 1997 - 2000 By InterNiche Technologies Inc. All rights reserved
 *
 *
 * MODULE: TCP
 *
 *
 * PORTABLE: yes
 */

/* Additional Copyrights: */
/* Portions Copyright 1996 by NetPort Software. All rights reserved. 
 * UNIX-ish Sockets library definitions and entry points. 
 */

#ifndef _SOCKCALL_H
#define  _SOCKCALL_H 1

#define  SYS_SOCKETNULL -1    /* error return from sys_socket. */
#define  INVALID_SOCKET -1    /* WINsock-ish synonym for SYS_SOCKETNULL */
#define  SOCKET_ERROR   -1    /* error return from send(), sendto(), et.al. */
#define  SOL_SOCKET     -1    /* compatability parm for set/get sockopt */

/* t_shutdown "how" parameter values */
#ifndef SHUT_RD
#define SHUT_RD         0     /* shutdown "read" half */
#define SHUT_WR         1     /* shutdown "write" half */
#define SHUT_RDWR       2     /* shutdown both "read" and "write" */
#endif


extern   long  t_socket (int, int, int);
extern   int   t_bind (long, struct sockaddr *, int);
extern   int   t_listen (long, int);
extern   long  t_accept (long, struct sockaddr *, int *);
extern   int   t_connect (long, struct sockaddr *, int);
extern   int   t_getpeername (long, struct sockaddr *, int * addrlen);
extern   int   t_getsockname (long, struct sockaddr *, int * addrlen);
extern   int   t_setsockopt (long sock, int level, int op, void * data, int dlen);
extern   int   t_getsockopt (long sock, int level, int op, void * data, int dlen);
extern   int   t_recv (long, char *, int, int);
extern   int   t_send (long, char *, int, int);
extern   int   t_recvfrom (long s, char * buf, int len, int flags, struct sockaddr *, int*);
extern   int   t_sendto (long s, char * buf, int len, int flags, struct sockaddr *, int);
extern   int   t_shutdown (long, int);
extern   int   t_socketclose (long);
extern   int   t_errno(long s);

extern   char * so_perror(int);  /* return an error string for a socket error */

/* define a macro test for validating a passed socket */

#ifndef SOC_CHECK    /* ports may replace this test */
#define  SOC_RANGE(so)  /* null - this version doesn't need range setup */
#define SOC_CHECK(so) \
   {  struct socket * tmp; \
      for(tmp = (struct socket *)(&soq); tmp; tmp = tmp->next) \
      {  if(tmp == so) break; } \
      if(tmp != so) \
      { dtrap(); \
         return SOCKET_ERROR; \
      } \
    }
#endif   /* undefined SOC_CHECK */

#ifdef SO_SELECT

/*
 * Altera Niche Stack modifcation 12-Feb-2008 (jrk):
 * Niche Stack indicates it should be built with _POSIX_SOURCE
 * defined, which prevents any collision between Niche Stack's fd_set
 * implementation below and newlib. However, this conflicts
 * with other Nios embedded source that depends on these definitions
 * in newlib. The following will override the newlib implementation
 * of fd_set for this object.
 */
#ifdef ALT_INICHE
  #include "sys/types.h"
  #undef fd_set
#endif /* ALT_INICHE */

/* the definitions to support the select() function. These are about 
 * as UNIX-like as we can make 'em on embedded code. They are also 
 * fairly compatable with WinSock's select() definitions.
 */

typedef struct   /* the select socket array manager */
{ 
   unsigned fd_count;               /* how many are SET? */
   long     fd_array[FD_SETSIZE];   /* an array of SOCKETs */
} fd_set;

/* our select call - note the traditional "width" parameter is absent */
int t_select(fd_set * in, fd_set * out, fd_set * ev, long tmo_seconds);

/* Select-related functions are calls (not macros) to save space */
#undef FD_ZERO
#undef FD_CLR
#undef FD_SET
#undef FD_ISSET

#define  FD_ZERO(set)         (((fd_set *)(set))->fd_count = 0)
#define  FD_CLR(so, set)       ifd_clr((long)(so), (fd_set *)(set))
#define  FD_SET(so, set)       ifd_set((long)(so), (fd_set *)(set))
#define  FD_ISSET(so, set)     ifd_isset((long)(so), (fd_set *)(set))
/* additional functionality */
#define  FD_GET(i, set)       ifd_get((int)(i), (fd_set *)(set))
#define  FD_COUNT(set)        (((fd_set *)(set))->fd_count)

void ifd_clr(long so, fd_set *set);
void ifd_set(long so, fd_set *set);
int  ifd_isset(long so, fd_set *set);
long ifd_get(unsigned i, fd_set *set);

#endif   /* SO_SELECT */

#endif   /* _SOCKCALL_H */

/* end of file sockcall.h */


