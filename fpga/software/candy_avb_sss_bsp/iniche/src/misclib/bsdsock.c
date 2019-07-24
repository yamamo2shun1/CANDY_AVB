/* FILENAME: bsdsock.c
 *
 * Copyright 2000 InterNiche Technologies Inc.  All rights reserved.
 *
 * BSD sockets porting aid
 *
 * MODULE: misclib
 * ROUTINES: bsd_accept(), bsd_getpeername(), bsd_getsockname(),
 *           bsd_getsockopt(), bsd_inet_aton(), bsd_inet_ntoa(),
 *           bsd_recvfrom(), bsd_select(), bsd_setsockopt(), 
 *           bsd_i_sockoptlen()
 * PORTABLE: yes
 */

#include "ipport.h"

#ifdef BSD_SOCKETS

#include <stdarg.h>

#include "netbuf.h"
#include "net.h"

#ifdef IP_MULTICAST
#include "ip.h"
#endif

#include "bsdsock.h"

#include "sockvar.h"


/* FUNCTION: bsd_accept()
 *
 * Accept a connection on a socket
 *
 * PARAM1: s; IN - socket descriptor for the listening socket
 * PARAM2: addr; OUT - ptr to buffer for return of accepted peer's address;
 *               may be NULL if return of address is not requested
 * PARAM3: addrlen; IN/OUT - ptr to int; on entry, specifies the length
 *                  in bytes of addr; on successful return, specifies the
 *                  length of the returned address; may be NULL if 
 *                  addr is also NULL
 * RETURNS: A socket descriptor if successful, -1 if an error occurred.  
 *          The error is available via bsd_errno(s).
 */
BSD_SOCKET
bsd_accept(BSD_SOCKET s,
           struct sockaddr * addr, int * addrlen)
{
   struct socket * so;
   struct sockaddr laddr;
   long lret;

   so = LONG2SO(s);
   SOC_CHECK(so);

   /* if we were given a buffer for the peer's address, also get the
    * buffer's length 
    */
   if (addr != NULL)
   {
      if (addrlen == 0)
      {
         so->so_error = EFAULT;
         return -1;
      }
   }

   lret = t_accept(s, &laddr, addrlen);

   /* if we were successful, and we were given a buffer for the peer's
    * address: copy the peer's address back into the buffer, but limit
    * the copy to the lesser of the buffer's length and sizeof(struct
    * sockaddr_in), which is all that t_accept() can return as a peer
    * address.  
    */
   if ((lret != -1) && (addr != NULL))
   {
      if (*addrlen > sizeof(struct sockaddr_in))
         *addrlen = sizeof(struct sockaddr_in);
      MEMCPY(addr, &laddr, *addrlen);
   }

   return lret;
}

/* FUNCTION: bsd_getpeername()
 *
 * Get the name of the connected peer
 *
 * PARAM1: s; IN - socket descriptor 
 * PARAM2: name; OUT - ptr to buffer for return of peer's address
 * PARAM3: addrlen; IN/OUT - ptr to int; on entry, specifies the length
 *                  in bytes of name; on successful return, specifies the
 *                  length of the returned name
 * RETURNS: 0 if successful, -1 if an error occurred.  The error is
 *          available via bsd_errno(s).
 */
int
bsd_getpeername(BSD_SOCKET s,
                struct sockaddr * name, int * namelen)
{
   struct socket * so;
   struct sockaddr lname;
   int lnamelen;
   int lret;

   so = LONG2SO(s);
   SOC_CHECK(so);

   /* if the buffer length is bogus, fail */
   if (namelen == NULL)
   {
      so->so_error = EFAULT;
      return -1;
   }
   lnamelen = *namelen;

   lret = t_getpeername(s, &lname, &lnamelen);

   /* if we were successful: copy the peer's address back into the
    * buffer, but limit the copy to the lesser of the buffer's length
    * and sizeof(struct sockaddr_in), which is all that
    * t_getpeername() can return as a peer address, and pass the
    * copied length back to the caller.  
    * For IPV6 addresses, or for dual IPV4/IPV6 stack, 
    * the max size is sizeof(struct sockaddr_in6)
    */
   if (lret != -1)
   {
#ifndef IP_V6
      if (lnamelen > sizeof(struct sockaddr_in))
         lnamelen = sizeof(struct sockaddr_in);
#else
      if (lnamelen > sizeof(struct sockaddr_in6))
         lnamelen = sizeof(struct sockaddr_in6);

#endif
      MEMCPY(name, &lname, lnamelen);
      *namelen = lnamelen;
   }

   return lret;
}

/* FUNCTION: bsd_getsockname()
 *
 * Gets the name of the socket.
 *
 * PARAM1: s; IN - socket descriptor 
 * PARAM2: name; OUT - ptr to buffer for return of peer's address
 * PARAM3: addrlen; IN/OUT - ptr to int; on entry, specifies the length
 *                  in bytes of name; on successful return, specifies the
 *                  length of the returned name
 * RETURNS: 0 if successful, -1 if an error occurred.  The error is
 *          available via bsd_errno(s).
 */
int
bsd_getsockname(BSD_SOCKET s,
                struct sockaddr * name, int * namelen)
{
   struct socket * so;
   struct sockaddr lname;
   int lnamelen;
   int lret;

   so = LONG2SO(s);
   SOC_CHECK(so);

   /* if the buffer length is bogus, fail */
   if (namelen == NULL)
   {
      so->so_error = EFAULT;
      return -1;
   }
   lnamelen = *namelen;

   lret = t_getsockname(s, &lname, &lnamelen);

   /* if we were successful: copy the peer's address back into the
    * buffer, but limit the copy to the lesser of the buffer's length
    * and sizeof(struct sockaddr_in), which is all that
    * t_getpeername() can return as a peer address, and pass the
    * copied length back to the caller.  
    * For IPV6 addresses, or for a dual IPV4/IPV6 stack, 
    * the max size copied is sizeof(struct sockaddr_in6)
    *
    */
   if (lret != -1)
   {
#ifndef IP_V6
      if (lnamelen > sizeof(struct sockaddr_in))
         lnamelen = sizeof(struct sockaddr_in);
#else
      if (lnamelen > sizeof(struct sockaddr_in6))
         lnamelen = sizeof(struct sockaddr_in6);
#endif
      MEMCPY(name, &lname, lnamelen);
      *namelen = lnamelen;
   }

   return lret;
}

/* FUNCTION: bsd_i_sockoptlen()
 *
 * Get the minimum length of a named socket option
 *
 * PARAM1: level; IN - the level of the option
 * PARAM2: name; IN - the name of the option
 * RETURNS: minimum length of the named socket option, in bytes
 */
int 
bsd_i_sockoptlen(int level,
                 int name)
{
   USE_ARG(level);

   switch (name)
   {
   case SO_BIO:
   case SO_NBIO:
      /* these don't use an option value */
      return 0;
   case SO_LINGER:
      /* this option is a struct linger */
      return sizeof(struct linger);
   case SO_RCVTIMEO:
   case SO_SNDTIMEO:
      /* these options are type short */
      return sizeof(short);
   case SO_KEEPALIVE:
   case SO_DONTROUTE:
   case SO_BROADCAST:
   case SO_REUSEADDR:
   case SO_OOBINLINE:
   case SO_SNDBUF:
   case SO_RCVBUF:
   case SO_NONBLOCK:
   case SO_ERROR:
   case SO_TYPE:
      /* these options are type int */
      return sizeof(int);
#ifdef TCP_ZEROCOPY
   case SO_CALLBACK:
      /* this option is a pointer to a function returning int */
      return sizeof(int (*)());
#endif /* TCP_ZEROCOPY */
#ifdef IP_MULTICAST
   case IP_MULTICAST_IF:
      /* this option is type ip_addr */
      return sizeof(ip_addr);
   case IP_MULTICAST_TTL:
   case IP_MULTICAST_LOOP:
      /* these options are type u_char */
      return sizeof(u_char);
   case IP_ADD_MEMBERSHIP:
   case IP_DROP_MEMBERSHIP:
      /* these options are struct ip_mreq */
      return sizeof(struct ip_mreq);
#endif /* IP_MULTICAST */
   case IP_TOS:
/*
 * Altera Niche Stack Nios port modification:
 * IP_TTL_OPT == SOREUSEADDR. This causes a build erro
 * due to duplicate cases. Removing this one. They both
 * return the same value (sizeof(int)).
 */
  // case IP_TTL_OPT:
      return sizeof(int);
   default:
      /* we don't know what type these options are */
      return 0;
   }
   
}

/* FUNCTION: bsd_getsockopt()
 *
 * Get a socket option value
 *
 * PARAM1: s; IN - socket descriptor 
 * PARAM2: level; IN - the level of the option to get
 * PARAM3: name; IN - the name of the option to get
 * PARAM4: opt; OUT - ptr to buffer for return of option value; may be
 *              NULL
 * PARAM5: optlen; IN/OUT - ptr to int; on entry, specifies the length
 *                 in bytes of opt; on successful return, specifies the
 *                 length of the returned option; may be NULL if opt is
 *                 also NULL
 * RETURNS: 0 if successful, -1 if an error occurred.  The error is
 *          available via bsd_errno(s).
 */
int
bsd_getsockopt(BSD_SOCKET s,
               int level,
               int name,
               void * opt, int * optlen)
{
   struct socket * so;
   int loptlen;
   int e;

   so = LONG2SO(s);
   SOC_CHECK(so);

   /* make sure supplied option value is big enough for the 
    * named option, else fail w/error EFAULT
    */
   loptlen = bsd_i_sockoptlen(level, name);
   if ((optlen == NULL) || (*optlen < loptlen))
   {
      so->so_error = EFAULT;
      return -1;
   }

   e = t_getsockopt(s, level, name, opt, loptlen);

   /* if it worked, copy the option length back for the caller's use */
   if (e == 0)
   {
      *optlen = loptlen;
   }

   return e;
   
}

/*
 * FUNCTION: bsd_ioctl()
 *
 * Perform a control operation on a socket
 *
 * PARAM1: s; IN - socket descriptor 
 * PARAM2: request; IN - requested control operation
 * PARAM3: arg; IN/OUT - arguments as required for control operation
 * RETURNS: 0 if successful, -1 if an error occurred.  The error is
 *          available via bsd_errno(s).
 */
int
bsd_ioctl(BSD_SOCKET s, 
          unsigned long request, ...)
{
   struct socket * so;
   va_list argptr;
   int iarg;

   so = LONG2SO(s);
   SOC_CHECK(so);

   va_start(argptr, request);

   switch (request)
   {
   case FIONBIO:
      iarg = va_arg(argptr, int);
      va_end(argptr);
      return t_setsockopt(s, SOL_SOCKET, SO_NONBLOCK, &iarg, sizeof(iarg));
   default:
      so->so_error = EINVAL;
      return -1;
   }
}


#ifdef NOT_USED
#error use inet_addr() in misclib/parseip.c
/*
 * FUNCTION: bsd_inet_addr()
 *
 * Parses a dotted-decimal Internet address
 *
 * PARAM1: cp; IN - dotted-decimal Internet address 
 * RETURNS: The parsed Internet address, in network order
 */
BSD_IN_ADDR_T
bsd_inet_addr(char * cp)
{
   BSD_IN_ADDR_T ret;
   ip_addr addr;
   unsigned sbits;
   char * err;

   err = parse_ipad(&addr, &sbits, cp);
   if (err == NULL)
   {
      /* return the address as success */
      ret = addr;
   }
   else
   {
      /* return failure */
      ret = 0;
   }
   return ret;
}


/*
 * FUNCTION: bsd_inet_aton()
 *
 * Parses a dotted-decimal Internet address
 *
 * PARAM1: cp; IN - dotted-decimal Internet address 
 * PARAM2: pin; OUT - pointer to buffer where parsed address is to be
 *                    returned
 * RETURNS: 1 if parsing was successful,
 *          0 if parsing failed.
 */
int
bsd_inet_aton(char * cp, struct in_addr * pin)
{
   ip_addr addr;
   unsigned sbits;
   char * err;

   err = parse_ipad(&addr, &sbits, cp);
   if (err == NULL)
   {
      /* if the caller gave us a buffer, copy the returned address
       * into it
       */
      if (pin != NULL)
         pin->s_addr = addr;
      /* return success */
      return 1;
   }
   else
   {
      /* return failure */
      return 0;
   }
}
#endif



/*
 * FUNCTION: bsd_inet_ntoa()
 *
 * Formats an Internet address into its printable dotted-decimal
 * representation.
 *
 * PARAM1: in; IN - Internet Address (in network order)
 * RETURNS: pointer to null-terminated string containing dotted-decimal
 *          printable representation of in
 */
char *
bsd_inet_ntoa(struct in_addr in)
{
   return print_ipad(in.s_addr);
}

/* FUNCTION: bsd_recvfrom()
 *
 * Receive data from a socket
 *
 * PARAM1: s; IN - socket descriptor 
 * PARAM2: buf; OUT - ptr to buffer for return of received data
 * PARAM3: len; IN - length (in bytes) of buf
 * PARAM4: flags; IN - flags to be applied to the recvfrom() operation
 * PARAM5: from; OUT - ptr to buffer for return of sending peer's name;
 *               may be NULL if return of name is not requested
 * PARAM6: fromlen; IN/OUT - ptr to int; on entry, specifies the length
 *                  in bytes of from; on successful return, specifies the
 *                  length of the returned name; may be NULL if 
 *                  from is also NULL
 * RETURNS: the number of bytes received (>= 0), or -1 if an error
 *          occurred.  The error is available via bsd_errno(s).
 */
int
bsd_recvfrom(BSD_SOCKET s,
             void * buf,
             BSD_SIZE_T len,
             int flags,
             struct sockaddr * from, int * fromlen)
{
   struct socket * so;
   struct sockaddr lfrom;
   int lfromlen = 0;
   int lret;

   so = LONG2SO(s);
   SOC_CHECK(so);

   /* if we were given a buffer for the peer's address, also get the
    * buffer's length 
    */
   if (from != NULL)
   {
      if (fromlen == NULL)
      {
         so->so_error = EFAULT;
         return -1;
      }
      lfromlen = *fromlen;
   }

   lret = t_recvfrom(s, (char *)buf, len, flags, &lfrom, &lfromlen );

   /* if we were successful, and we were given a buffer for the peer's
    * name: copy the peer's name back into the buffer, but limit
    * the copy to the lesser of the buffer's length and sizeof(struct
    * sockaddr_in), which is all that t_recvfrom() can return as a peer
    * name.
    * For IPV6 addresses or dual IPV4/IPV6 stack, the max size copied
    * is sizeof(struct sockaddr_in6)
    */
   if ((lret != -1) && (from != NULL))
   {
#ifndef IP_V6
      if (lfromlen > sizeof(struct sockaddr_in))
         lfromlen = sizeof(struct sockaddr_in);
#else
      if (lfromlen > sizeof(struct sockaddr_in6))
         lfromlen = sizeof(struct sockaddr_in6);

#endif
      MEMCPY(from, &lfrom, lfromlen);
      *fromlen = lfromlen;
   }

   return lret;
}

#ifdef SO_SELECT
/* FUNCTION: bsd_select()
 *
 * Synchronous I/O multiplexing (for sockets)
 *
 * PARAM1: nfds; IN - the number of fds to be checked in each set (readfds,
 *               writefds, exceptfds): not used in this release
 * PARAM2: readfds; IN/OUT - fd set containing descriptors to be examined
 *                  for readiness to read (i.e. received data is present
 *                  and can be retreived by recv() or recvfrom() without
 *                  blocking); on successful return, will contain 
 *                  only those descriptors that are ready to read
 * PARAM3: writefds; IN/OUT - fd set containing descriptors to be examined
 *                   for readiness to write (e.g. send() or sendto() could
 *                   succeed without blocking, or socket has connect()ed);
 *                   on successful return, will contain only those 
 *                   descriptors that are ready to write
 * PARAM4: exceptfds; IN/OUT - fd set containing descriptors to be examined
 *                    for exceptional conditions (i.e. out-of-band data
 *                    pending on socket); on successful return
 * PARAM5: timeout; IN - ptr to a struct timeval, which should be set to
 *                  indicate the maximum interval to wait for any 
 *                  descriptor(s) to become ready; may be NULL, indicating 
 *                  that select() can wait indefinitely for any descriptor(s) 
 *                  to become ready
 * RETURNS: if successful, the number of ready file descriptors in all
 *          the fd sets (may be 0 if the timeout expired), or -1 if an
 *          error occured.  The error would be available via bsd_errno(s).
 */
int 
bsd_select(int nfds,
           fd_set * readfds,
           fd_set * writefds,
           fd_set * exceptfds,
           BSD_TIMEVAL_T * timeout)
{
   long ltv;    /* timeout expressed in ticks */
   long tps;    /* local copy of TPS */

   USE_ARG(nfds);

   if (timeout != NULL)
   {
      /* This turning of a struct timeval into a tick-count timeout
       * assumes that TPS (ticks-per-second) is a reasonably small
       * integer, and that tv_usec may be anywhere from 0 to one
       * million (i.e. any number of microseconds up to one second).
       * So we scale tv_usec from microseconds to something reasonable
       * based on TPS, multiply it by TPS, then scale it the rest of
       * the way to ticks-per-second.
       */
      tps = TPS;
      if (tps >= 1000)
      {
         ltv = (((timeout->tv_usec + 50) / 100) * tps) / 10000;
      }
      else if (tps >= 100)
      {
         ltv = (((timeout->tv_usec + 500) / 1000) * tps) / 1000;
      }
      else if (tps >= 10)
      {
         ltv = (((timeout->tv_usec + 5000) / 10000) * tps) / 100;
      }
      else
      {
         ltv = (((timeout->tv_usec + 50000) / 100000) * tps) / 10;
      }
      ltv += (timeout->tv_sec * TPS);
   }
   else {
      /*
       * NULL timeout: wait indefinitely in t_select()
       */
      ltv = -1;
   }

   return (t_select(readfds, writefds, exceptfds, ltv));
}
#endif /* SO_SELECT */

/* FUNCTION: bsd_setsockopt()
 *
 * Set a socket option value
 *
 * PARAM1: s; IN - socket descriptor 
 * PARAM2: level; IN - the level of the option to set
 * PARAM3: name; IN - the name of the option to set
 * PARAM4: opt; OUT - ptr to buffer containing option value; may be
 *              NULL
 * PARAM5: optlen; IN - specifies the length in bytes of the option 
 *                 value in opt
 * RETURNS: 0 if successful, -1 if an error occurred.  The error is
 *          available via bsd_errno(s).
 */
int 
bsd_setsockopt(BSD_SOCKET s,
               int level,
               int name,
               void * opt, int optlen)
{
   struct socket * so;

   so = LONG2SO(s);
   SOC_CHECK(so);

   /* make sure supplied option value is big enough for the 
    * named option, else fail w/error EFAULT
    */
   if (optlen < bsd_i_sockoptlen(level, name))
   {
      so->so_error = EFAULT;
      return -1;
   }

   return t_setsockopt(s, level, name, opt, optlen);
}

#endif /* BSD_SOCKETS */
