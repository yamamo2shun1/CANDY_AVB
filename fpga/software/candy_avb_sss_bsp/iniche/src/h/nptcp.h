/*
 * FILENAME: nptcp.h
 *
 * Copyright 1997 - 2000 By InterNiche Technologies Inc. All rights reserved
 *
 * File to MAP TCP's Berserkly-isms into NetPort/InterNiche IP Stack.
 *
 * MODULE: TCP
 *
 *
 * PORTABLE: yes
 */

/* Additional Copyrights: */
/* Portions Copyright 1996 by NetPort Software. All rights reserved. */


#ifndef NPTCP_H
#define  NPTCP_H

#ifdef MINI_IP
#error This file not for Mini-IP use
#endif   /* MINI_IP */

#ifndef _IPPORT_H_
#error Include ipport.h before this file
#endif   /* missing IPPORT_H */

#define  NPPORT      1     /* flag for NetPort tcp port (not UNIX) */

extern   NET iproute(ip_addr, ip_addr*);  /* prototype netport routing */



/*
 * NetPort IP routine to get this machine's IP address relative to a 
 * destination IP address.
 */
extern   u_long ip_mymach(u_long ipaddr);

/* flag for NetPort port */
#define  NPPORT   1

/* pretend we are a UNIX kernel */
#define  KERNEL      1
#define  INET_KERNEL    1

/* Strange but true: just listing these here cuts 
 * out some compile time warnings with some compilers:
 */
struct ip;   
struct mbuf;
struct uio;

void  tcp_sleep(void * timeout);
void  tcp_wakeup(void * wake);
void  np_stripoptions(struct ip * ti, struct mbuf * m);
int   in_broadcast(u_long ipaddr);     /* TRUE if ipaddr is broadcast */

unshort  tcp_cksum(struct ip * pip);


#ifdef TCP_ZEROCOPY  /* zero-copy sockets extensions */
struct socket;
void     tcp_data_upcall(struct socket *);   /* internal (invoke upcall) */
int      tcp_xout(long s, PACKET pkt);       /* send */
PACKET   tcp_pktalloc(int datasize);         /* get send buffer */
void     tcp_pktfree(PACKET p);
#endif

/* Berkeley style "Internet address" */

struct in_addr
{
   unsigned long  s_addr;
};

#define  INADDR_ANY     0L


/* Berkeley style "Socket address" */

struct sockaddr_in
{
   short    sin_family;
   u_short  sin_port;
   struct   in_addr  sin_addr;
   char     sin_zero[8];
};

struct socket; /* predeclaration */

/* BSD sockets errors */

#ifndef SOCKERRORS_ALREADY    /* allow 3rd party override */

#ifdef MCF5235_PLATFORM
#define	EPERM 1		/* Not super-user */
#define	ENOENT 2	/* No such file or directory */
#define	ESRCH 3		/* No such process */
#define	EINTR 4		/* Interrupted system call */
#define	EIO 5		/* I/O error */
#define	ENXIO 6		/* No such device or address */
#define	E2BIG 7		/* Arg list too long */
#define	ENOEXEC 8	/* Exec format error */
#define	EBADF 9		/* Bad file number */
#define	ECHILD 10	/* No children */
#define	EAGAIN 11	/* No more processes */
#define	ENOMEM 12	/* Not enough core */
#define	EACCES 13	/* Permission denied */
#define	EFAULT 14	/* Bad address */
#define	ENOTBLK 15	/* Block device required */
#define	EBUSY 16	/* Mount device busy */
#define	EEXIST 17	/* File exists */
#define	EXDEV 18	/* Cross-device link */
#define	ENODEV 19	/* No such device */
#define	ENOTDIR 20	/* Not a directory */
#define	EISDIR 21	/* Is a directory */
#define	EINVAL 22	/* Invalid argument */
#define	ENFILE 23	/* Too many open files in system */
#define	EMFILE 24	/* Too many open files */
#define	ENOTTY 25	/* Not a typewriter */
#define	ETXTBSY 26	/* Text file busy */
#define	EFBIG 27	/* File too large */
#define	ENOSPC 28	/* No space left on device */
#define	ESPIPE 29	/* Illegal seek */
#define	EROFS 30	/* Read only file system */
#define	EMLINK 31	/* Too many links */
#define	EPIPE 32	/* Broken pipe */
#define	EDOM 33		/* Math arg out of domain of func */
#define	ERANGE 34	/* Math result not representable */
#define	ENOMSG 35	/* No message of desired type */
#define	EIDRM 36	/* Identifier removed */
#define	ECHRNG 37	/* Channel number out of range */
#define	EL2NSYNC 38	/* Level 2 not synchronized */
#define	EL3HLT 39	/* Level 3 halted */
#define	EL3RST 40	/* Level 3 reset */
#define	ELNRNG 41	/* Link number out of range */
#define	EUNATCH 42	/* Protocol driver not attached */
#define	ENOCSI 43	/* No CSI structure available */
#define	EL2HLT 44	/* Level 2 halted */
#define	EDEADLK 45	/* Deadlock condition */
#define	ENOLCK 46	/* No record locks available */
#define EBADE 50	/* Invalid exchange */
#define EBADR 51	/* Invalid request descriptor */
#define EXFULL 52	/* Exchange full */
#define ENOANO 53	/* No anode */
#define EBADRQC 54	/* Invalid request code */
#define EBADSLT 55	/* Invalid slot */
#define EDEADLOCK 56	/* File locking deadlock error */
#define EBFONT 57	/* Bad font file fmt */
#define ENOSTR 60	/* Device not a stream */
#define ENODATA 61	/* No data (for no delay io) */
#define ETIME 62	/* Timer expired */
#define ENOSR 63	/* Out of streams resources */
#define ENONET 64	/* Machine is not on the network */
#define ENOPKG 65	/* Package not installed */
#define EREMOTE 66	/* The object is remote */
#define ENOLINK 67	/* The link has been severed */
#define EADV 68		/* Advertise error */
#define ESRMNT 69	/* Srmount error */
#define	ECOMM 70	/* Communication error on send */
#define EPROTO 71	/* Protocol error */
#define	EMULTIHOP 74	/* Multihop attempted */
#define	ELBIN 75	/* Inode is remote (not really error) */
#define	EDOTDOT 76	/* Cross mount point (not really error) */
#define EBADMSG 77	/* Trying to read unreadable message */
#define EFTYPE 79	/* Inappropriate file type or format */
#define ENOTUNIQ 80	/* Given log. name not unique */
#define EBADFD 81	/* f.d. invalid for this operation */
#define EREMCHG 82	/* Remote address changed */
#define ELIBACC 83	/* Can't access a needed shared lib */
#define ELIBBAD 84	/* Accessing a corrupted shared lib */
#define ELIBSCN 85	/* .lib section in a.out corrupted */
#define ELIBMAX 86	/* Attempting to link in too many libs */
#define ELIBEXEC 87	/* Attempting to exec a shared library */
#define ENOSYS 88	/* Function not implemented */
#define ENMFILE 89      /* No more files */
#define ENOTEMPTY 90	/* Directory not empty */
#define ENAMETOOLONG 91	/* File or path name too long */
#define ELOOP 92	/* Too many symbolic links */
#define EOPNOTSUPP 95	/* Operation not supported on transport endpoint */
#define EPFNOSUPPORT 96 /* Protocol family not supported */
#define ECONNRESET 104  /* Connection reset by peer */
#define ENOBUFS 105	/* No buffer space available */
#define EAFNOSUPPORT 106 /* Address family not supported by protocol family */
#define EPROTOTYPE 107	/* Protocol wrong type for socket */
#define ENOTSOCK 108	/* Socket operation on non-socket */
#define ENOPROTOOPT 109	/* Protocol not available */
#define ESHUTDOWN 110	/* Can't send after socket shutdown */
#define ECONNREFUSED 111	/* Connection refused */
#define EADDRINUSE 112		/* Address already in use */
#define ECONNABORTED 113	/* Connection aborted */
#define ENETUNREACH 114		/* Network is unreachable */
#define ENETDOWN 115		/* Network interface is not configured */
#define ETIMEDOUT 116		/* Connection timed out */
#define EHOSTDOWN 117		/* Host is down */
#define EHOSTUNREACH 118	/* Host is unreachable */
#define EINPROGRESS 119		/* Connection already in progress */
#define EALREADY 120		/* Socket already connected */
#define EDESTADDRREQ 121	/* Destination address required */
#define EMSGSIZE 122		/* Message too long */
#define EPROTONOSUPPORT 123	/* Unknown protocol */
#define ESOCKTNOSUPPORT 124	/* Socket type not supported */
#define EADDRNOTAVAIL 125	/* Address not available */
#define ENETRESET 126
#define EISCONN 127		/* Socket is already connected */
#define ENOTCONN 128		/* Socket is not connected */
#define ETOOMANYREFS 129
#define EPROCLIM 130
#define EUSERS 131
#define EDQUOT 132
#define ESTALE 133
#define ENOTSUP 134		/* Not supported */
#define ENOMEDIUM 135   /* No medium (in tape drive) */
#define ENOSHARE 136    /* No such host or network path */
#define ECASECLASH 137  /* Filename exists with different case */
#define EILSEQ 138
#define EOVERFLOW 139	/* Value too large for defined data type */

/* From cygwin32.  */
#define EWOULDBLOCK EAGAIN	/* Operation would block */
#define __ELASTERROR 2000	/* Users can add values starting here */

#define     EHAVEOOB       2001
#define     EIEIO          2002 /* bad input/output on Old Macdonald's farm :-) */

#else  /* not MCF5235_PLATFORM */

#define     ENOBUFS        1
#define     ETIMEDOUT      2
#define     EISCONN        3
#define     EOPNOTSUPP     4
#define     ECONNABORTED   5
#define     EWOULDBLOCK    6
#define     ECONNREFUSED   7
#define     ECONNRESET     8
#define     ENOTCONN       9
#define     EALREADY       10
#define     EINVAL         11
#define     EMSGSIZE       12
#define     EPIPE          13
#define     EDESTADDRREQ   14
#define     ESHUTDOWN      15
#define     ENOPROTOOPT    16
#define     EHAVEOOB       17
#define     ENOMEM         18
#define     EADDRNOTAVAIL  19
#define     EADDRINUSE     20
#define     EAFNOSUPPORT   21
#define     EINPROGRESS    22
#define     ELOWER         23    /* lower layer (IP) error */
#define     ENOTSOCK       24    /* Includes sockets which closed while blocked */
#define     EIEIO 27 /* bad input/output on Old Macdonald's farm :-) */

#ifdef   IP_MULTICAST
#define     ETOOMANYREFS   28
#endif   /* IP_MULTICAST */

#define     EFAULT         29
#define     ENETUNREACH    30
#endif

#endif   /* SOCKERRORS_ALREADY - end 3rd party override */

#ifndef NLONG_ALREADY 
typedef u_long n_long;
#endif
#ifndef NTIME_ALREADY 
typedef u_long n_time;
#endif
#ifndef UINT_ALREADY 
typedef u_long u_int;
#endif


#ifndef MIN
#define  MIN(a,b)    (a>b?b:a)
#define  MAX(a,b)    (a>b?a:b)
#endif

/* this can be used to supress arg checking on broken compilers */
#ifndef __P
#define  __P(x)   x
#endif

#define     IFNETHDR_SIZE     MaxLnh
#define     MAXOPTLEN      256   /* size of alloc for TCP options */

#define     PR_SLOWHZ   2  /* TCP ticks per second */
#define     PR_FASTHZ   5  /* 5 fast timeouts per second */

/* IPPROTO_... definitions, from BSD <netinet/in.h> */
#define     IPPROTO_IP     0  /* added for IP multicasting changes */
#define     IPPROTO_IGMP   2  /* added for IP multicasting changes */
#define     IPPROTO_TCP    6
#define     IPPROTO_UDP    17
#define     IPPROTO_RAW    255

#define     IPPORT_RESERVED      1024
#define     IPPORT_USERRESERVED     5000  /* more BSDness */

extern   struct inpcb * GetTcpcbList(void);

/* keep the INET_TRACE feature for later, define away for now. -JB- */
#define  INET_TRACE(flags, text);

/* fake IP route structure */
struct route
{
   unshort  unused;  /* placeholder */
};

/* ip send routine for TCP layers */
extern   int   ip_output   __P ((struct mbuf *, struct   ip_socopts * so_optsPack));

/* mbuf checksumming routine */
extern   u_short  in_cksum __P ((struct mbuf *, int));


#ifdef DO_TCPTRACE
#define  SETTP(tp,action)  tp=(action)
#else
#define  SETTP(tp,action)  (action)
#endif

extern   unshort  select_wait;

int      nptcp_init(void);       /* called from tcpinit(), calls tcp_init() */
int      tcp_stat(void * pio);   /* printf TCP MIB statistics */
int      mbuf_stat(void * pio);  /* printf mbuf pool statistics */
int      sock_list(void * pio);  /* list existing sockets */
void     tcp_tick(void);         /* TCP clock tick - for timers, etc */
int      udp_usrreq(struct socket * so, struct mbuf * m, struct mbuf * nam);

#ifdef IP_RAW
int      rawip_usrreq(struct socket * so, struct mbuf * m, struct mbuf * nam);
#endif /* IP_RAW */

int      tcp_bsdconnstat(void * pio);  /* BSD-ish TCP connection stats */
int      tcp_bsdsendstat(void * pio);  /* BSD-ish TCP send stats */
int      tcp_bsdrcvstat (void * pio);  /* BSD-ish TCP receive stats */

/* end of file nptcp.h */

#endif   /* NPTCP_H */


