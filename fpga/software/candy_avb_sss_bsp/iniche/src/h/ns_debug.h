/*
 * FILENAME: ns_debug.h
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
 * Copyright (c) 1998, Neelum Inc. Sunnyvale, CA 94086. USA.
 */

/*
 * $Id: ns_debug.h,v 1.1.2.1 2007/07/24 02:19:27 steveh Exp $
 * $Log: ns_debug.h,v $
 * Revision 1.1.2.1  2007/07/24 02:19:27  steveh
 * moved from dnssrv to h
 *
 * Revision 7.1  2003/11/11 00:44:33  Satish
 * V6_to_V7
 *
 * Revision 6.1  2002/05/31 21:48:19  frank
 * V5_to_V6
 *
 * Revision 5.1  2001/06/22 23:34:56  frank
 * V4_to_V5
 *
 * Revision 4.3  2001/06/22 20:44:40  frank
 * V_to_V
 *
 * Revision 4.2  2001/06/22 20:44:39  frank
 * V4_to_V
 *
 * Revision 4.1  2000/11/01 23:31:28  amba
 * Initial revision
 *
 * Revision 1.1  1999/11/16 17:28:43  narayan
 *  Version 1.5 Check-in
 *
 */
#define  NSM_ERR  (1<<0l)  /* Errors  */
#define  NSM_IO   (1<<1l)  /* When doing IO */
#define  NSM_SYSQUERY   (1<<2l)  /* Sysquery  */
#define  NSM_CACHE   (1<<3l)
#define  NSM_DB   (1<<4l)
#define  NSM_QUERY   (1<<5l)
#define  NSM_RESP (1<<6l)
#define  NSM_REQ  (1<<7l)
#define  NSM_NS   (1<<8l)
#define  NSM_ZONE (1<<9l)  /* Zone transfers */
#define  NSM_RR   (1<<10l)
#define  NSM_LOOKUP  (1<<11l)
#define  NSM_RETRY   (1<<12l)
#define  NSM_MISC (1<<13l)
#define  NSM_MATCH   (1<<14l) /* Name matches  */
#define  NSM_HASH (1<<15l) /* Hash matches  */
#define  NSM_INIT (1<<16l) /* Init routines */
#define  NSM_FORW (1<<17l) /* Forwarding  */

#define  FP_QUERY(level,msg)

#ifdef INTERNICHE
#define  log_msg  printf
#define  debug_prf   printf
#endif

#if defined(DEBUG) || defined(_DEBUG)
extern   long  _ns_dbg;
#define  NS_DEBUG(m,a)  if ((m)  &  _ns_dbg) debug_prf   a;
#define ASSERT(exp) if (!(exp)) {    \
 debug_prf ("ASSERT FAILED <%s> %s:%d\n",#exp, __FILE__, __LINE__);\
 panic ("assert");}
#else
#define  NS_DEBUG(m,a)
#define  ASSERT(x)
#endif


