/*
 * FILENAME: nameser.h
 *
 * Copyright  2000 By InterNiche Technologies Inc. All rights reserved
 *
 *
 * MODULE: DNS
 *
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
 *
 *
 * Portions Copyright (c) 1993 by Digital Equipment Corporation.
 * 
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies, and that
 * the name of Digital Equipment Corporation not be used in advertising or
 * publicity pertaining to distribution of the document or software without
 * specific, written prior permission.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND DIGITAL EQUIPMENT CORP. DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS.   IN NO EVENT SHALL DIGITAL EQUIPMENT
 * CORPORATION BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 * -
 * --Copyright--
 */

#ifndef __ARPA_NAMESER_H__
#define  __ARPA_NAMESER_H__
#ifdef __cplusplus
extern "C" {
#endif

/*
 *      @(#)nameser.h 8.1 (Berkeley) 6/2/93
 * $Id: nameser.h,v 1.1.2.2 2007/11/02 00:46:58 steveh Exp $
 */
/*
 * revision information.  this is the release date in YYYYMMDD format.
 * it can change every day so the right thing to do with it is use it
 * in preprocessor commands such as "#if (__BIND > 19931104)".  do not
 * compare for equality; rather, use it to determine whether your resolver
 * is new enough to contain a certain feature.
 */
#define  __BIND   19950621 /* interface version stamp */

/*
 * Define constants based on rfc883
 */
#define  PACKETSZ 512   /* maximum packet size */
#define  MAXDNAME 256   /* maximum domain name */
#define  MAXCDNAME   255   /* maximum compressed domain name */
#define  MAXLABEL 63 /* maximum length of domain label */
#define  HFIXEDSZ 12 /* #/bytes of fixed data in header */
#define  QFIXEDSZ 4  /* #/bytes of fixed data in query */
#define  RRFIXEDSZ   10 /* #/bytes of fixed data in r record */
#define  INT32SZ  4  /* for systems without 32-bit ints */
#define  INT16SZ  2  /* for systems without 16-bit ints */
#define  INADDRSZ 4  /* for sizeof(struct inaddr) != 4 */

/*
 * Internet nameserver port number
 */
#define  NAMESERVER_PORT   53

/*
 * Currently defined opcodes
 */
#define  QUERY    0x0   /* standard query */
#define  IQUERY   0x1   /* inverse query */
#define  STATUS   0x2   /* nameserver status query */
   /*   0x3     0x3 reserved */
#define  NS_NOTIFY_OP   0x4   /* notify secondary of SOA change */
   /* non standard - supports ALLOW_UPDATES stuff from Mike Schwartz */
# define UPDATEA 0x9    /* add resource record */
# define UPDATED 0xa /* delete a specific resource record */
# define UPDATEDA 0xb   /* delete all named resource record */
# define UPDATEM 0xc /* modify a specific resource record */
# define UPDATEMA 0xd   /* modify all named resource record */
# define ZONEINIT 0xe   /* initial zone transfer */
# define ZONEREF 0xf /* incremental zone referesh */

/*
 * Currently defined response codes
 */
#define  NSNOERROR   0  /* no error */
#define  FORMERR  1  /* format error */
#define  SERVFAIL 2  /* server failure */
#define  NXDOMAIN 3  /* non existent domain */
#define  NOTIMP   4  /* not implemented */
#define  REFUSED  5  /* query refused */
   /* non standard */
# define NOCHANGE 0xf   /* update failed to change db */

/* 
 * DDNS response codes
 */
#define  YXDOMAIN (6)
#define  YXRRSET (7)
#define  NXRRSET (8)
#define  NOTAUTH (9)
#define  NOTZONE (10)

#define  C_ANY    255   /* wildcard match */
#define  C_NONE (254)
#define  C_IN  1  /* the arpa internet */

/*
 * Type values for resources and queries
 */
#define  T_A   1  /* host address */
#define  T_NS  2  /* authoritative server */
#define  T_MD  3  /* mail destination */
#define  T_MF  4  /* mail forwarder */
#define  T_CNAME  5  /* canonical name */
#define  T_SOA    6  /* start of authority zone */
#define  T_MB  7  /* mailbox domain name */
#define  T_MG  8  /* mail group member */
#define  T_MR  9  /* mail rename name */
#define  T_NULL   10 /* null resource record */
#define  T_WKS    11 /* well known service */
#define  T_PTR    12 /* domain name pointer */
#define  T_HINFO  13 /* host information */
#define  T_MINFO  14 /* mailbox information */
#define  T_MX  15 /* mail routing information */
#define  T_TXT    16 /* text strings */
#define  T_RP  17 /* responsible person */
#define  T_AFSDB  18 /* AFS cell database */
#define  T_X25    19 /* X_25 calling address */
#define  T_ISDN   20 /* ISDN calling address */
#define  T_RT  21 /* router */
#define  T_NSAP   22 /* NSAP address */
#define  T_NSAP_PTR  23 /* reverse NSAP lookup (deprecated) */
#define  T_SIG    24 /* security signature */
#define  T_KEY    25 /* security key */
#define  T_PX  26 /* X.400 mail mapping */
#define  T_GPOS   27 /* geographical position (withdrawn) */
#define  T_AAAA   28 /* IP6 Address */
#define  T_LOC    29 /* Location Information */
   /* non standard */
#define  T_UINFO  100   /* user (finger) information */
#define  T_UID    101   /* user ID */
#define  T_GID    102   /* group ID */
#define  T_UNSPEC 103   /* Unspecified format (binary data) */
   /* Query type values which do not appear in resource records */
#define  T_AXFR   252   /* transfer zone of authority */
#define  T_MAILB  253   /* transfer mailbox records */
#define  T_MAILA  254   /* transfer mail agent records */
#define  T_ANY    255   /* wildcard match */

/*
 * Values for class field
 */
#define  C_IN  1  /* the arpa internet */
#define  C_CHAOS  3  /* for chaos net (MIT) */
#define  C_HS  4  /* for Hesiod name server (MIT) (XXX) */
   /* Query class values which do not appear in resource records */
#define  C_ANY    255   /* wildcard match */

/*
 * Status return codes for T_UNSPEC conversion routines
 */
#define  CONV_SUCCESS   0
#define  CONV_OVERFLOW  (-1)
#define  CONV_BADFMT (-2)
#define  CONV_BADCKSUM  (-3)
#define  CONV_BADBUFLEN (-4)

#ifndef BYTE_ORDER

#define  LITTLE_ENDIAN  1234  /* least-significant byte first (vax, pc) */
#define  BIG_ENDIAN  4321  /* most-significant byte first (IBM, net) */
#define  PDP_ENDIAN  3412  /* LSB first in word, MSW first in long (pdp)*/

#if defined(vax) || defined(ns32000) || defined(sun386) || defined(i386) || \
   defined(MIPSEL) || defined(_MIPSEL) || defined(BIT_ZERO_ON_RIGHT) || \
   defined(__alpha__) || defined(__alpha)
#define  BYTE_ORDER  LITTLE_ENDIAN
#endif

#if defined(sel) || defined(pyr) || defined(mc68000) || defined(sparc) || \
   defined(is68k) || defined(tahoe) || defined(ibm032) || defined(ibm370) || \
   defined(MIPSEB) || defined(_MIPSEB) || defined(_IBMR2) || defined(DGUX) ||\
   defined(apollo) || defined(__convex__) || defined(_CRAY) || \
   defined(__hppa) || defined(__hp9000) || \
   defined(__hp9000s300) || defined(__hp9000s700) || \
   defined (BIT_ZERO_ON_LEFT) || defined(m68k)
#define  BYTE_ORDER  BIG_ENDIAN
#endif

#endif   /* BYTE_ORDER */

#if !defined(BYTE_ORDER) || \
   (BYTE_ORDER != BIG_ENDIAN && BYTE_ORDER != LITTLE_ENDIAN && \
    BYTE_ORDER != PDP_ENDIAN)
   {
 /* you must determine what the correct bit order is for
  * your compiler - the next line is an intentional error
  * which will force your compiles to bomb until you fix
  * the above macros.
  */
      error "Undefined or invalid BYTE_ORDER";
   }
#endif

#ifdef NS_DDNS
#define MAX_NAM_LEN 256

struct ResRec
{
    struct ResRec *next;  /* pointer to next RR */
	char name[MAX_NAM_LEN];   /* name */
	unsigned short  type;      /* type */
	unsigned short  dclass;     /* class */
	unsigned long  ttl;         /* TTL */
	short  RDlength;    /* length of Rdata */
	int    flags;       /* keep track of processing within a chain */
	char *RdataP;     /* Rdata, N.b., this still points into 
	                   * the original msg! */
};

typedef struct ResRec RR;

/* RR flags */
#define RR_CH_MATCHED     (1)
#define  NS_UPDATE (5)     /* DDNS update */

struct DQDesc
{
	char zone[MAX_NAM_LEN];   /* ZONE */
	RR   *perqsP;     /* pointer to prerequisites */
	RR   *upsP;       /* pointer to updates */
	RR   *addsP;      /* pointer to addition RRs */
};

typedef struct DQDesc DQDESC;

DQDESC * dns_parse_ddqry(u_char * msg, int msglen, int buflen, int errCode);
int dns_dynup(DQDESC * DQDp);

#endif



/*
 * Structure for query header.  The order of the fields is machine- and
 * compiler-dependent, depending on the byte/bit order and the layout
 * of bit fields.  We use bit fields only in int variables, as this
 * is all ANSI requires.  This requires a somewhat confusing rearrangement.
 */
#if 1 /* We don't use bit fields they are not portable ** Narayan **/
   typedef struct 
   {
      unsigned short id;  /* query ident number  */
      unsigned char bits [2];
      unsigned short qdcount;   /* number of question entries */
      unsigned short ancount;   /* number of answer entries */
      unsigned short nscount;   /* number of authority entries */
      unsigned short arcount;   /* number of resource entries */
   } HEADER;

#define  HEADER_GET_RCODE(x)  ((x)->bits  [1]   &  0xf)
#define  HEADER_GET_PR(x)  ((x)->bits[1]  &  0x40)
#define  HEADER_GET_RA(x)  ((x)->bits  [1]   &  0x80)
#define  HEADER_GET_RD(x)  ((x)->bits  [0]   &  0x1)
#define  HEADER_GET_TC(x)  ((x)->bits  [0]   &  0x2)
#define  HEADER_GET_AA(x)  ((x)->bits  [0]   &  0x4)
#define  HEADER_GET_OPCODE(x) (((x)->bits [0]   >> 3) &  0xF)
#define  HEADER_GET_QR(x)  ((x)->bits  [0]   &  0x80)

#define  HEADER_CLR_PR(x)  ((x)->bits  [1]   &  ~0x40)
#define  HEADER_SET_PR(x)  ((x)->bits  [1]   |= 0x40)
#define  HEADER_SET_RA(x)  ((x)->bits  [1]   |= 0x80)
#define  HEADER_SET_RD(x)  ((x)->bits  [0]   |= 0x1)
#define  HEADER_CLR_RD(x)  ((x)->bits  [0]   &= ~0x1)
#define  HEADER_SET_TC(x)  ((x)->bits  [0]   |= 0x2)
#define  HEADER_CLR_TC(x)  ((x)->bits  [0]   &= ~0x2)
#define  HEADER_SET_AA(x)  ((x)->bits  [0]   |= 0x4)
#define  HEADER_CLR_AA(x)  ((x)->bits  [0]   &= ~0x4)
#define  HEADER_SET_RCODE(x,v)   {(x)->bits  [1]   |= ((v)  &  0xf);}
#define  HEADER_SET_QR(x)  ((x)->bits  [0]   |= 0x80)
#define  HEADER_CLR_QR(x)  ((x)->bits  [0]   &= ~0x80)
#define  HEADER_SET_OPCODE(h,code)  {(h)->bits  [0]   |= ((code)  << 3);}
#else /* Use BIT-Fields */
   typedef struct 
   {
      unsigned id :16;    /* query identification number */
#if BYTE_ORDER == BIG_ENDIAN
      /* fields in third byte */
      unsigned qr: 1;  /* response flag */
      unsigned opcode: 4; /* purpose of message */
      unsigned aa: 1;  /* authoritive answer */
      unsigned tc: 1;  /* truncated message */
      unsigned rd: 1;  /* recursion desired */
      /* fields in fourth byte */
      unsigned ra: 1;  /* recursion available */
      unsigned pr: 1;  /* primary server reqd (nonstd) */
      unsigned unused :2; /* unused bits (MBZ as of 4.9.3a3) */
      unsigned rcode :4;  /* response code */
#endif
#if BYTE_ORDER == LITTLE_ENDIAN || BYTE_ORDER == PDP_ENDIAN
      /* fields in third byte */
      unsigned rd :1;  /* recursion desired */
      unsigned tc :1;  /* truncated message */
      unsigned aa :1;  /* authoritive answer */
      unsigned opcode :4; /* purpose of message */
      unsigned qr :1;  /* response flag */
      /* fields in fourth byte */
      unsigned rcode :4;  /* response code */
      unsigned unused :2; /* unused bits (MBZ as of 4.9.3a3) */
      unsigned pr: 1;  /* primary server reqd (nonstd) */
      unsigned ra :1;  /* recursion available */
#endif
      /* remaining bytes */
      unsigned qdcount :16;  /* number of question entries */
      unsigned ancount :16;  /* number of answer entries */
      unsigned nscount :16;  /* number of authority entries */
      unsigned arcount :16;  /* number of resource entries */
   } HEADER;
#endif   /* 0 */

/*
 * Defines for handling compressed domain names
 */
#define  INDIR_MASK  0xc0

/*
 * Structure for passing resource records around.
 */
   struct rrec {
        int16_t  r_zone;  /* zone number */
        int16_t  r_class; /* class number */
        int16_t  r_type;  /* type number */
        u_int32_t   r_ttl;   /* time to live */
        int   r_size;  /* size of data area */
        char  *  r_data;  /* pointer to data */
   };

/*
 * Inline versions of get/put short/long.  Pointer is advanced.
 *
 * These macros demonstrate the property of C whereby it can be
 * portable or it can be elegant but rarely both.
 */
#define GETSHORT(s, cp) { \
 u_char *t_cp = (u_char *)(cp); \
 (s) = ((u_int16_t)t_cp[0] << 8) \
     | ((u_int16_t)t_cp[1]) \
     ; \
 (cp) += INT16SZ; \
}

#define GETLONG(l, cp) { \
 u_char *t_cp = (u_char *)(cp); \
 (l) = ((u_int32_t)t_cp[0] << 24) \
     | ((u_int32_t)t_cp[1] << 16) \
     | ((u_int32_t)t_cp[2] << 8) \
     | ((u_int32_t)t_cp[3]) \
     ; \
 (cp) += INT32SZ; \
}

#define PUTSHORT(s, cp) { \
 u_int16_t t_s = (u_int16_t)(s); \
 u_char *t_cp = (u_char *)(cp); \
 *t_cp++ = (u_char)(t_s >> 8); \
 *t_cp   = (u_char)t_s; \
 (cp) += INT16SZ; \
}

#define PUTLONG(l, cp) { \
 u_int32_t t_l = (u_int32_t)(l); \
 u_char *t_cp = (u_char *)(cp); \
 *t_cp++ = (u_char)(t_l >> 24); \
 *t_cp++ = (u_char)(t_l >> 16); \
 *t_cp++ = (u_char)(t_l >> 8); \
 *t_cp   = (u_char)t_l; \
 (cp) += INT32SZ; \
}

#define  syslog(l,fmt)
#define  syslog1(l,fmt,a)
#define  syslog2(l,fmt,a,b)
#define  syslog3(l,fmt,a,b,c)
#ifdef __cplusplus
}
#endif
#endif   /* !__ARPA__NAMESER_H__ */

