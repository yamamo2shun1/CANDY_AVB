/*
 * FILENAME: mbuf.h
 *
 * Copyright 1997 - 2000 By InterNiche Technologies Inc. All rights reserved
 *
 * This is the definitions file for NetPort mbufs. These 
 * are sytacticly used like BSD mbufs in the BSD derived C code, 
 * however the actual buffer space managed is mapped inthe a NetPort 
 * style PACKET structure. Concepts and Portions of this file are 
 * adapted from the BSD sources.
 *
 * MODULE: INET
 *
 *
 * PORTABLE: yes
 */

/* Additional Copyrights: */
/* Portions Copyright 1996 by NetPort Software. All rights reserved. */


#ifndef _MBUF_H
#define  _MBUF_H  1

/* mbuf struct - just a wrapper for nb_buff */

struct mbuf
{
   struct mbuf *  next;    /* queue link */
   PACKET   pkt;           /* the nb_buff, w/actual contiguous buffer */
   unsigned m_len;         /* length of m_data */
   char *   m_data;        /* pointer to next data */
   char *   m_base;        /* base of buffer, <= m_data */
   unsigned m_memsz;       /* size of buffer at m_base */
   struct mbuf *  m_next;  /* next mbuf in record (TCP) */
   struct mbuf *  m_act;   /* start of next record (UDP) */
   int      m_type;        /* as in UNIX; 0==free */
};

/* mbuf types */
#define  MT_FREE     0     /* should be on free list */
#define  MT_RXDATA   1     /* dynamic (data) allocation */
#define  MT_TXDATA   2     /* dynamic (data) allocation */
#define  MT_HEADER   3     /* packet header */
#define  MT_SOCKET   4     /* socket structure */
#define  MT_PCB      5     /* protocol control block */
#define  MT_RTABLE   6     /* routing tables */
#define  MT_HTABLE   7     /* IMP host tables */
#define  MT_ATABLE   8     /* address resolution tables */
#define  MT_SONAME   9     /* socket name */
#define  MT_SOOPTS   10    /* socket options */
#define  MT_FTABLE   11    /* fragment reassembly header */
#define  MT_RIGHTS   12    /* access rights */
#define  MT_IFADDR   13    /* interface address */

/* flags to m_get */
#define  M_DONTWAIT  0
#define  M_WAIT      1

#define  CLBYTES     1400
#define  M_COPYALL   -1

extern   struct mbuf *     m_getnbuf   __P ((int type, int len));

void     m_adj(struct mbuf * mp, int len);
int      mbuf_len (struct mbuf * m);

/* map all the Mango & BSD mbuf "get()" calls in the nbuf wrapper */
#define  m_getwithdata(type,  len)  m_getnbuf(type,   len)
#define  m_get(waitflag,   type) m_getnbuf(type,   0)
#define  MFREE(m, n)    ((n)  =  m_free(m))
#define  mtod(mb, cast)    (cast)(mb->m_data)

extern   struct mbuf *     m_copy   __P ((struct mbuf *, int, int));
extern   struct mbuf *  m_free   __P ((struct mbuf *));
extern   void  m_freem  __P ((struct mbuf *));
extern   struct mbuf *  dtom  __P ((void *));

#endif   /* _MBUF_H */

/* end of file mbuf.h */



