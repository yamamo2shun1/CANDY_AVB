/*
 * FILENAME: sockcall.c
 *
 * Copyright 1997-2007 By InterNiche Technologies, Inc. All rights reserved.
 *
 *
 * MODULE: INET
 *
 * ROUTINES: t_socket(), t_bind (), t_listen(), t_accept(), 
 * ROUTINES: t_connect(), t_getpeername(), t_getsockname(), t_setsockopt(), 
 * ROUTINES: t_getsockopt(), t_recv (), t_recvfrom(), t_sendto (), 
 * ROUTINES: t_shutdown(), t_socketclose(), sockargs (), sock_recvit(), 
 * ROUTINES: sock_sendit(), t_errno()
 *
 * PORTABLE: yes
 */

/* Additional Copyrights: */
/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. [rescinded 22 July 1999]
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */


#include "ipport.h"
#include "tcpport.h"

#ifdef INCLUDE_TCP  /* include/exclude whole file at compile time */

#include "protosw.h"
#include "in_pcb.h"
#include "tcp.h"
#include "udp.h"
#include "ip.h"
#include "tcp_timr.h"
#include "tcp_var.h"

#ifdef INCLUDE_INICHE_LOG
#include "iniche_log.h"
#endif
/*#define SOCKDEBUG  1*/
/*#define TRACE_DEBUG*/

/* Internal helper functions */
static struct mbuf * sockargs(void *, int, int);
static int  t_getname(long s, struct sockaddr * addr, int * addrlen, int opcode);


/* bitmask for connection state bits which determine if send/recv is OK */
#define  SO_IO_OK (SS_ISCONNECTED|SS_ISCONNECTING|SS_ISDISCONNECTING)


/* Domain check is an option for testing validity of
 * passed sockaddr_in sizes for the socket's domain
 * (IN_ADDR for IPv6, or IP_ADDR5 for IPv6)
 */

#ifdef NPDEBUG
void
DOMAIN_CHECK(struct socket * so, int size)
{
#ifdef IP_V4
   if((so->so_domain == AF_INET) &&
      (size < sizeof(struct sockaddr_in)))
   {
      dtrap(); /* programmer passed wrong structure */
   }
#endif   /* IP_V4 */

#ifdef IP_V6
   if((so->so_domain == AF_INET6) &&
      (size != sizeof(struct sockaddr_in6)))
   {
      dtrap(); /* programmer passed wrong structure */
   }
#endif   /* IP_V6 */
}
#else    /* not debug build */
#define    DOMAIN_CHECK(so, size)   /* define away to nothing */
#endif



/* FUNCTION: t_socket()
 * 
 * PARAM1: int family
 * PARAM2: int type
 * PARAM3: int proto
 *
 * RETURNS: 
 */

long
t_socket(int family, 
   int   type, 
   int   proto)
{
   struct socket *   so;

   INET_TRACE (INETM_SOCKET, ("SOCK:sock:family %d, typ %d, proto %d\n",
    family, type, proto));
   LOCK_NET_RESOURCE(NET_RESID);
   if ((so = socreate (family, type, proto)) == NULL) 
   {  /* can't really return error info since no socket.... */
      UNLOCK_NET_RESOURCE(NET_RESID);
      return SOCKET_ERROR;
   }
   SOC_RANGE(so);
   so->so_error = 0;
   UNLOCK_NET_RESOURCE(NET_RESID);
   return SO2LONG(so);
}



/* FUNCTION: t_bind ()
 * 
 * PARAM1: long s
 * PARAM2: struct sockaddr *addr
 *
 * RETURNS: 0 if OK, else one of the socket error codes
 */

int
t_bind (long s, 
   struct sockaddr * addr,
   int addrlen)
{
   struct mbuf *     nam;
   struct sockaddr   sa;
   struct sockaddr * sap;
   struct socket *   so;
   int               err;

   so = LONG2SO(s);  /* convert long to socket */
   SOC_CHECK(so);
   DOMAIN_CHECK(so, addrlen);

   so->so_error = 0;
   if (addr == (struct sockaddr *)NULL) 
   {
      MEMSET ((void *)&sa, 0, sizeof(sa));
      addrlen = sizeof(sa);
      sa.sa_family = so->so_domain;
      sap = &sa;
   } else
      sap = addr;

   if ((nam = sockargs (sap, addrlen, MT_SONAME)) == NULL) 
   {
      so->so_error = ENOMEM;
      return SOCKET_ERROR;
   }
   LOCK_NET_RESOURCE(NET_RESID);
   err = sobind (so, nam);
   m_freem(nam);
   UNLOCK_NET_RESOURCE(NET_RESID);
   if (err) 
   {
      so->so_error = err;
      return SOCKET_ERROR;
   }
   return 0;
}



/* FUNCTION: t_listen()
 * 
 * PARAM1: long s
 * PARAM2: int backlog
 *
 * RETURNS: 
 */

int
t_listen(long s, 
   int   backlog)
{
   struct socket *   so;
   int   err;

   so = LONG2SO(s);  /* convert long to socket */
   SOC_CHECK(so);
   so->so_error = 0;
   INET_TRACE (INETM_SOCKET, ("SOCK:listen:qlen %d\n", backlog));

   LOCK_NET_RESOURCE(NET_RESID);
   err = solisten (so, backlog);
   UNLOCK_NET_RESOURCE(NET_RESID);

   if (err != 0) 
   {
      so->so_error = err;
      return SOCKET_ERROR;
   }
   return 0;
}



/* FUNCTION: t_accept()
 * 
 * PARAM1: long s
 * PARAM2: struct sockaddr * addr
 *
 * RETURNS: 
 */

long
t_accept(long s, 
   struct sockaddr * addr,
   int * addrlen)
{
#ifdef SOCKDEBUG
   char logbuf[10];
#endif
   struct socket *   so;
   struct mbuf *  nam;

   so = LONG2SO(s);
   SOC_CHECK(so);
   DOMAIN_CHECK(so, *addrlen);

   so->so_error = 0;
   INET_TRACE (INETM_SOCKET,
      ("INET:accept:so %x so_qlen %d so_state %x\n", so, so->so_qlen, so->so_state));
   if ((so->so_options & SO_ACCEPTCONN) == 0)
   {
      so->so_error = EINVAL;
#ifdef SOCKDEBUG
      sprintf(logbuf, "t_accept[%d]: %d", __LINE__, so->so_error);
      glog_with_type(LOG_TYPE_DEBUG, logbuf, 1);
#endif
      return SOCKET_ERROR;
   }
   if ((so->so_state & SS_NBIO) && so->so_qlen == 0)
   {
      so->so_error = EWOULDBLOCK;
#ifdef SOCKDEBUG
      sprintf(logbuf, "t_accept[%d]: %d", __LINE__, so->so_error);
      glog_with_type(LOG_TYPE_DEBUG, logbuf, 1);
#endif
      return SOCKET_ERROR;
   }
   LOCK_NET_RESOURCE(NET_RESID);
   while (so->so_qlen == 0 && so->so_error == 0)
   {
      if (so->so_state & SS_CANTRCVMORE)
      {
         so->so_error = ECONNABORTED;
         UNLOCK_NET_RESOURCE(NET_RESID);
         return SOCKET_ERROR;
      }
      tcp_sleep ((char *)&so->so_timeo);
   }
   if (so->so_error)
   {
#ifdef SOCKDEBUG
      sprintf(logbuf, "t_accept[%d]: %d", __LINE__, so->so_error);
      glog_with_type(LOG_TYPE_DEBUG, logbuf, 1);
#endif
      UNLOCK_NET_RESOURCE(NET_RESID);
      return SOCKET_ERROR;
   }
   nam = m_getwithdata (MT_SONAME, sizeof (struct sockaddr));
   if (nam == NULL) 
   {
      UNLOCK_NET_RESOURCE(NET_RESID);
      so->so_error = ENOMEM;
#ifdef SOCKDEBUG
      sprintf(logbuf, "t_accept[%d]: %d", __LINE__, so->so_error);
      glog_with_type(LOG_TYPE_DEBUG, logbuf, 1);
#endif
      return SOCKET_ERROR;
   }
   { 
      struct socket *aso = so->so_q;
      if (soqremque (aso, 1) == 0)
         panic("accept");
      so = aso;
   }
   (void)soaccept (so, nam);
#ifdef TRACE_DEBUG
   { struct sockaddr_in *sin;
      sin = mtod(nam, struct sockaddr_in *);
      INET_TRACE (INETM_SOCKET, ("INET:accept:done so %lx port %d addr %lx\n",
       so, sin->sin_port, sin->sin_addr.s_addr));
   }
#endif   /* TRACE_INET */
   /* return the addressing info in the passed structure */
   if (addr != NULL)
      MEMCPY(addr, nam->m_data, *addrlen);
   m_freem (nam);
   UNLOCK_NET_RESOURCE(NET_RESID);
   SOC_RANGE(so);
   return SO2LONG(so);
}

/* FUNCTION: t_connect()
 * 
 * PARAM1: long s
 * PARAM2: struct sockaddr *addr
 *
 * RETURNS: 
 */

int
t_connect(long s, 
   struct sockaddr * addr,
   int   addrlen)
{
   struct socket *   so;
   struct mbuf *  nam;

   so = LONG2SO(s);
   SOC_CHECK(so);
   DOMAIN_CHECK(so, addrlen);

#ifdef NB_CONNECT
   /* need to test non blocking connect bits in case this is a 
      poll of a previous request */
   if (so->so_state & SS_NBIO)
   {
      if (so->so_state & SS_ISCONNECTING) /* still trying */
      {
         so->so_error = EINPROGRESS;
         return SOCKET_ERROR;
      }
      if (so->so_state & SS_ISCONNECTED)  /* connected OK */
      {
         so->so_error = 0;
         return 0;
      }
      if (so->so_state & SS_WASCONNECTING)
      {
         so->so_state &= ~SS_WASCONNECTING;
         if (so->so_error) /* connect error - maybe timeout */
            return SOCKET_ERROR;
      }
   }
#endif   /*  NB_CONNECT */

   so->so_error = 0;

   if ((nam = sockargs (addr, addrlen, MT_SONAME))
       == NULL)
   {
      so->so_error = ENOMEM;
      return SOCKET_ERROR;
   }

#ifdef TRACE_DEBUG
   { struct sockaddr_in *sin = (struct sockaddr_in *)uap->sap;
      INET_TRACE (INETM_SOCKET, ("INET: connect, port %d addr %lx\n",
       sin->sin_port, sin->sin_addr.s_addr));
   }
#endif   /* TRACE_DEBUG */

   LOCK_NET_RESOURCE(NET_RESID);
   if ((so->so_error = soconnect (so, nam)) != 0)
      goto bad;

#ifdef NB_CONNECT
   /* need to test non blocking connect bits after soconnect() call */
   if ((so->so_state & SS_NBIO)&& (so->so_state & SS_ISCONNECTING))
   {
      so->so_error = EINPROGRESS;
      goto bad;
   }
#endif   /*  NB_CONNECT */
   INET_TRACE (INETM_SOCKET, ("INET: connect, so %x so_state %x so_error %d\n",
    so, so->so_state, so->so_error));

   while ((so->so_state & SS_ISCONNECTING) && so->so_error == 0) 
   {
      tcp_sleep ((char *)&so->so_timeo);
   }
bad:
   if (so->so_error != EINPROGRESS)
      so->so_state &= ~(SS_ISCONNECTING|SS_WASCONNECTING);
   m_freem (nam);

   UNLOCK_NET_RESOURCE(NET_RESID);
   if (so->so_error)
   {
/*      printf("t_connect(): so_error = %d\n", so->so_error);*/
      return SOCKET_ERROR;

   }
      return 0;
}



/* FUNCTION: t_getpeername()
 * 
 * PARAM1: long s
 * PARAM2: struct sockaddr * addr
 * PARAM3: pointer to int with sockaddr length (v4 vs v6)
 *
 * RETURNS: 
 */

int
t_getpeername(long s, struct sockaddr * addr, int * addrlen)
{
   return(t_getname(s, addr, addrlen, PRU_PEERADDR));
}


/* FUNCTION: t_getsockname()
 * 
 * PARAM1: long s
 * PARAM2: struct sockaddr *addr
 * PARAM3: pointer to int with sockaddr length (v4 vs v6)
 *
 * RETURNS: 
 */

int 
t_getsockname(long s, struct sockaddr * addr, int * addrlen)
{
   return(t_getname(s, addr, addrlen, PRU_SOCKADDR));
}


/* FUNCTION: t_getname()
 * 
 * Worker function for getpeername and getsockname.
 *
 * PARAM1: long s
 * PARAM2: struct sockaddr *addr
 * PARAM3: pointer to int with sockaddr length (v4 vs v6)
 * PARAM4: opcode for call - peername or sockname
 *
 * RETURNS: 
 */

static int
t_getname(long s, struct sockaddr * addr, int * addrlen, int opcode)
{
   struct socket *   so;
   struct mbuf *  m;
   int   err;

   so = LONG2SO(s);
   SOC_CHECK(so);

   so->so_error = 0;
   INET_TRACE (INETM_SOCKET, ("INET:get[sock|peer]name so %x\n", so));
   if((opcode == PRU_PEERADDR) && (so->so_state & SS_ISCONNECTED) == 0)
   {
      so->so_error = ENOTCONN;
      return SOCKET_ERROR;
   }
   LOCK_NET_RESOURCE(NET_RESID);
   m = m_getwithdata (MT_SONAME, sizeof (struct sockaddr));
   if (m == NULL) 
   {
      so->so_error = ENOMEM;
      UNLOCK_NET_RESOURCE(NET_RESID);
      return SOCKET_ERROR;
   }
   so->so_req = opcode;
   if ((err = (*so->so_proto->pr_usrreq)(so, 0, m)) != 0)
      goto bad;

#ifdef IP_V4
   if(so->so_domain == AF_INET)
   {
      if(*addrlen < sizeof(struct sockaddr_in))
      {
         dtrap();    /* programming error */
         m_freem(m);
         UNLOCK_NET_RESOURCE(NET_RESID);
         return EINVAL;
      }
      MEMCPY(addr, m->m_data, sizeof(struct sockaddr_in));
      *addrlen = sizeof(struct sockaddr_in);
   }
#endif   /* IP_V4 */
#ifdef IP_V6
   if(so->so_domain == AF_INET6)
   {
      if(*addrlen < sizeof(struct sockaddr_in6))
      {
         dtrap();    /* programming error */
         m_freem(m);
         UNLOCK_NET_RESOURCE(NET_RESID);
         return EINVAL;
      }
      MEMCPY(addr, m->m_data, sizeof(struct sockaddr_in6));
      *addrlen = sizeof(struct sockaddr_in6);
   }
#endif   /* IP_V6 */


bad:
   m_freem(m);
   UNLOCK_NET_RESOURCE(NET_RESID);
   if (err) 
   {
      so->so_error = err;
      return SOCKET_ERROR;
   }
   return 0;
}



/* FUNCTION: t_setsockopt()
 * 
 * PARAM1: long s
 * PARAM2: int name
 * PARAM3: void *arg
 *
 * RETURNS: 
 */

int
t_setsockopt(long s,
   int   level,
   int   name,
   void * arg,
   int arglen)
{
   struct socket *   so;
   int   err;

   so = LONG2SO(s);
   SOC_CHECK(so);
   USE_ARG(arglen);

   LOCK_NET_RESOURCE (NET_RESID);

   so->so_error = 0;
   INET_TRACE (INETM_SOCKET,
    ("INET: setsockopt: name %x val %x valsize %d\n",
    name, val));

   /* is it a level IP_OPTIONS call? */
   if (level != IP_OPTIONS)
   {
      if ((err = sosetopt (so, name, arg)) != 0) 
      {
         so->so_error = err;
         UNLOCK_NET_RESOURCE (NET_RESID);
         return SOCKET_ERROR;
      }
   }
   else
   {
   /* level 1 options are for the IP packet level.
    * the info is carried in the socket CB, then put 
    * into the PACKET.
    */
      if (!so->so_optsPack)
      {
         so->so_optsPack = (struct ip_socopts *) SOCOPT_ALLOC (sizeof(struct ip_socopts *));
         if (!so->so_optsPack) 
         {
            so->so_error = ENOMEM;
            UNLOCK_NET_RESOURCE (NET_RESID);
            return SOCKET_ERROR;
         }
      }
      
      if (name == IP_TTL_OPT)
         so->so_optsPack->ip_ttl = (u_char)(*(int *)arg);
      else
      if (name == IP_TOS)
         so->so_optsPack->ip_tos = (u_char)(*(int *)arg);
	   else
	   if (name == IP_SCOPEID)
            so->so_optsPack->ip_scopeid = (u_char)(*(u_int *)arg);
      else
      {
         UNLOCK_NET_RESOURCE (NET_RESID);
         return SOCKET_ERROR;
      }   
   }

   UNLOCK_NET_RESOURCE (NET_RESID);
   return 0;
}



/* FUNCTION: t_getsockopt()
 * 
 * PARAM1: long s
 * PARAM2: int name
 * PARAM3: void *arg
 *
 * RETURNS: 
 */

int
t_getsockopt(long s,
   int   level,
   int   name,
   void *   arg,
   int   arglen)

{
   struct socket *   so;
   int   err;

   so = LONG2SO(s);
   SOC_CHECK(so);
   USE_ARG(level);
   USE_ARG(arglen);

   LOCK_NET_RESOURCE (NET_RESID);

   INET_TRACE (INETM_SOCKET,
    ("INET: getsockopt: name %x val %x valsize %d\n",
    name, val));

   /* is it a level IP_OPTIONS call? */
   if (level != IP_OPTIONS)
   {
      if ((err = sogetopt (so, name, arg)) != 0) 
      {
         so->so_error = err;
         UNLOCK_NET_RESOURCE (NET_RESID);
         return SOCKET_ERROR;
      }
   }
   else
   {
      /* level 1 options are for the IP packet level.
       * the info is carried in the socket CB, then put 
       * into the PACKET.
       */
      if (name == IP_TTL_OPT)
      {
         if (!so->so_optsPack) *(int *)arg = IP_TTL;
         else *(int *)arg = (int)so->so_optsPack->ip_ttl;
      }
      else if (name == IP_TOS)
      {
         if (!so->so_optsPack) *(int *)arg = IP_TOS_DEFVAL;
         else *(int *)arg = (int)so->so_optsPack->ip_tos;
      }
      else
      {
         UNLOCK_NET_RESOURCE (NET_RESID);
         return SOCKET_ERROR;
      }
   }   
   so->so_error = 0;

   UNLOCK_NET_RESOURCE (NET_RESID);
   return 0;
}



/* FUNCTION: t_recv ()
 * 
 * PARAM1: long s
 * PARAM2: char *buf
 * PARAM3: int len
 * PARAM4: int flag
 *
 * RETURNS: 
 */

int
t_recv (long s, 
   char *   buf,
   int   len, 
   int   flag)
{
#ifdef SOCKDEBUG
   char logbuf[10];
#endif
   struct socket *   so;
   int   err;
   int   sendlen = len;

   so = LONG2SO(s);
#ifdef SOC_CHECK_ALWAYS
   SOC_CHECK(so);
#endif
   if ((so->so_state & SO_IO_OK) != SS_ISCONNECTED)
   {
      so->so_error = EPIPE;
#ifdef SOCKDEBUG
      sprintf(logbuf, "t_recv: %d", so->so_error);
      glog_with_type(LOG_TYPE_DEBUG, logbuf, 1);
#endif
      return SOCKET_ERROR;
   }
   so->so_error = 0;

   LOCK_NET_RESOURCE(NET_RESID);
   IN_PROFILER(PF_TCP, PF_ENTRY);        /* measure time in TCP */
   INET_TRACE (INETM_IO, ("INET:recv: so %x, len %d\n", so, len));
   err = soreceive(so, NULL, buf, &len, flag);
   IN_PROFILER(PF_TCP, PF_EXIT);        /* measure time in TCP */
   UNLOCK_NET_RESOURCE(NET_RESID);

   if(err)
   {
      so->so_error = err;
#ifdef SOCKDEBUG
      sprintf(logbuf, "t_recv: %d", so->so_error);
      glog_with_type(LOG_TYPE_DEBUG, logbuf, 1);
#endif
      return SOCKET_ERROR;
   }

   /* return bytes we sent - the amount we wanted to send minus
    * the amount left in the buffer.
    */
   return (sendlen - len);
}



/* FUNCTION: t_recvfrom()
 * 
 * PARAM1: long s
 * PARAM2: char *buf
 * PARAM3: int len
 * PARAM4: int flags
 * PARAM5: struct sockaddr *from
 *
 * RETURNS: 
 */

int
t_recvfrom(long s, 
   char *   buf,
   int   len, 
   int   flags,
   struct sockaddr * from,
   int * fromlen)
{
   struct socket *   so;
   struct mbuf *     sender = NULL;
   int   err;
   int   sendlen = len;

   so = LONG2SO(s);
   SOC_CHECK(so);
   so->so_error = 0;

   LOCK_NET_RESOURCE(NET_RESID);

   err = soreceive(so, &sender, buf, &len, flags);

   /* copy sender info from mbuf to sockaddr */
   if (sender)
   {
      MEMCPY(from, (mtod(sender, struct sockaddr *)), *fromlen );
      m_freem (sender);
   }

   UNLOCK_NET_RESOURCE(NET_RESID);

   if(err)
   {
      so->so_error = err;
      return SOCKET_ERROR;
   }

   /* OK return: amount of data actually sent */
   return (sendlen - len);
}



/* FUNCTION: t_sendto ()
 * 
 * PARAM1: long s
 * PARAM2: char *buf
 * PARAM3: int len
 * PARAM4: int flags
 * PARAM5: struct sockaddr *to
 *
 * RETURNS: 
 */

int
t_sendto (long s, 
   char *   buf,
   int   len, 
   int   flags,
   struct sockaddr * to,
   int   tolen)
{
   struct socket *   so;
   int   sendlen;
   int   err;
   struct mbuf *     name;

   so = LONG2SO(s);
   SOC_CHECK(so);
   so->so_error = 0;

   switch (so->so_type)
   {
   case SOCK_STREAM:
      /* this is a stream socket, so pass this request through
       * t_send() for its large-send support.
       */
      return t_send(s, buf, len, flags);
      /*NOTREACHED*/
   case SOCK_DGRAM:
      /* datagram (UDP) socket -- prepare to check length */
      sendlen = udp_maxalloc();
      break;
#ifdef IP_RAW
   case SOCK_RAW:
      /* raw socket -- prepare to check length */
      sendlen = ip_raw_maxalloc(so->so_options & SO_HDRINCL);
      break;
#endif /* IP_RAW */
   default:
      /* socket has unknown type */
      dtrap();
      so->so_error = EFAULT;
      return SOCKET_ERROR;
      /*NOTREACHED*/
   }

   /* fall through for non-stream sockets: SOCK_DGRAM (UDP) and
    * SOCK_RAW (raw IP)
    */

   /* check length against underlying stack's maximum */
   if (len > sendlen)
   {
      so->so_error = EMSGSIZE;
      return SOCKET_ERROR;
   }

   /* if a sockaddr was passed, wrap it in an mbuf and pas it into the
    * bowels of the BSD code; else assume this is a bound UDP socket
    * and this call came from t_send() below.
    */

   if (to)  /* sockaddr was passed */
   {
      name = sockargs(to, tolen, MT_SONAME);
      if(name == NULL)
      {
         so->so_error = ENOMEM;
         return SOCKET_ERROR;
      }
   }
   else     /* hope user called bind() first... */
      name = NULL;
   
   sendlen = len;

   LOCK_NET_RESOURCE(NET_RESID);

   err = sosend (so, name, buf, &sendlen, flags);

   if (name)
      m_freem(name);

   UNLOCK_NET_RESOURCE(NET_RESID);

   if (err != 0)
   {
      so->so_error = err;
      return SOCKET_ERROR;
   }

   return (len - sendlen);
}




/* FUNCTION: t_send()
 * 
 * PARAM1: long s
 * PARAM2: char *buf
 * PARAM3: int len
 * PARAM4: int flags
 *
 * RETURNS: 
 */

int
t_send(long s, 
   char *   buf,
   int      len, 
   int      flags)
{
   struct socket *   so;
   int   e;       /* error holder */
   int   total_sent  =  0;
   int   maxpkt;
   int   sendlen;
   int   sent;

   so = LONG2SO(s);
#ifdef SOC_CHECK_ALWAYS
   SOC_CHECK(so);
#endif
   if ((so->so_state & SO_IO_OK) != SS_ISCONNECTED)
   {
      so->so_error = EPIPE;
      return SOCKET_ERROR;
   }
   so->so_error = 0;

   /* If this is not a stream socket, assume it is bound and pass to
    * t_sendto() with a null sockaddr
    */
   if (so->so_type != SOCK_STREAM)
      return(t_sendto(s, buf, len, flags, NULL, 0));

   maxpkt = TCP_MSS;
   if(so->so_pcb)
   { 
      struct tcpcb * tp;
      tp = intotcpcb(so->so_pcb);   /* get tcp structure with mss */
      if(tp->t_maxseg)              /* Make sure it's set */
         maxpkt = tp->t_maxseg;
   }

   IN_PROFILER(PF_TCP, PF_ENTRY);       /* measure time in TCP */

   while (len)
   {
      if (len > maxpkt)
         sendlen = maxpkt;  /* take biggest block we can */
      else
         sendlen = len;
      sent = sendlen;

      LOCK_NET_RESOURCE(NET_RESID);
      e = sosend (so, NULL, buf, &sendlen, flags);
      UNLOCK_NET_RESOURCE(NET_RESID);
 
      if (e != 0)  /* sock_sendit failed? */
      {
         /* if we simply ran out of bufs, report back to caller. */
         if ((e == ENOBUFS) || (e == EWOULDBLOCK))
         {
            /* if we actually sent something before running out
             * of buffers, report what we sent; 
             * else, report the error and let the application 
             * retry the call later
             */
            if (total_sent != 0)
            {
               so->so_error = 0;
               break;      /* break out of while(len) loop */
            }
         }
         so->so_error = e;
         return SOCKET_ERROR;
      }
      /* if we can't send anymore, return now */
      if (sendlen != 0)
         break;         /* break out of while(len) loop */

      /* adjust numbers & pointers, and go do next send loop */
      sent -= sendlen;        /* subtract anything that didn't get sent */
      buf += sent;
      len -= sent;
      total_sent += sent;
   }

   IN_PROFILER(PF_TCP, PF_EXIT);        /* measure time in TCP */
   return total_sent;
}



/* FUNCTION: t_shutdown()
 * 
 * PARAM1: long               socket handle
 * PARAM2: int                shutdown action
 *
 * RETURNS: 
 */

int
t_shutdown(long s, int   how)
{
   struct socket *so;
   int   err;

   so = LONG2SO(s);
   SOC_CHECK(so);
   so->so_error = 0;
   INET_TRACE (INETM_SOCKET, ("INET:shutdown so %x how %d\n", so, how));

   LOCK_NET_RESOURCE(NET_RESID);
   err = soshutdown(so, how);
   UNLOCK_NET_RESOURCE(NET_RESID);

   if (err != 0)
   {
      so->so_error = err;
      return SOCKET_ERROR;
   }
   return 0;
}



/* FUNCTION: t_socketclose()
 * 
 * PARAM1: long s
 *
 * RETURNS: 
 */

int
t_socketclose(long s)
{
   struct socket *   so;
   int   err;

   so = LONG2SO(s);
   SOC_CHECK(so);
   so->so_error = 0;
   INET_TRACE ((INETM_CLOSE|INETM_SOCKET), ("INET:close, so %lx\n",so));

   LOCK_NET_RESOURCE(NET_RESID);
   err = soclose(so);
   UNLOCK_NET_RESOURCE(NET_RESID);

   if (err != 0) 
   {
      /* do not do the following assignment since the socket structure
         addressed by so has been freed by this point, jharan 12-10-98 */
      /*      so->so_error = err;   */
      return SOCKET_ERROR;
   }
   return 0;
}



/* FUNCTION: sockargs ()
 * 
 * PARAM1: void *arg
 * PARAM2: int arglen
 * PARAM3: int type
 *
 * RETURNS: 
 */

static struct mbuf  * 
sockargs (void * arg, 
   int   arglen, 
   int   type)
{
   struct mbuf *  m;

   LOCK_NET_RESOURCE(NET_RESID);    /* protect mfreeq */
   m = m_getwithdata (type, arglen);
   UNLOCK_NET_RESOURCE(NET_RESID);
   if (m == NULL)
      return NULL;
   m->m_len = arglen;
   MEMCPY(mtod (m, char *), arg, arglen);
   return m;
}




/* the rest are NetPort additions and won't be covered in generic 
 * socket documentation:
 */


/* FUNCTION: t_errno()
 *
 * t_errno() - return last error on a socket. Return value is 
 * undefined if the socket has not previously reported an error. 
 *
 * 
 * PARAM1: long s
 *
 * RETURNS: int               socket error code or
 *                            ENOTSOCK if socket not found
 */

int
t_errno(long s)
{
   struct socket *so = LONG2SO(s);
   struct socket *tmp;
   int errcode = ENOTSOCK;

   LOCK_NET_RESOURCE(NET_RESID);    /* protect soq */

   /* search socket queue for passed socket. This routine should
    * not use SOC_CHECK since it can be ifdeffed out, and we must
    * be ready to return EPIPE if the socket does not exist.
    */
   for (tmp = (struct socket *)(&soq); tmp; tmp = tmp->next)
   {
      if (tmp == so)  /* found socket, return error */
      {
         errcode = so->so_error;
         break;
      }
   }

   UNLOCK_NET_RESOURCE(NET_RESID);

   return errcode;
}




#endif /* INCLUDE_TCP */

