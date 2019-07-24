/*
 * FILENAME: tcpapp.h
 *
 * Copyright 2001 By InterNiche Technologies Inc. All rights reserved
 *
 * Single include file for INterNicher TCP applications. This defines everything
 * the app needs to call sockets. Works for Full-Sized stack or mini stack.
 *
 * MODULE: TCP
 *
 * PORTABLE: yes
 */

/* Additional Copyrights: */
/* Portions Copyright 1996 by NetPort Software. All rights reserved. */


#ifndef _TCPAPP_H
#define  _TCPAPP_H  1

#include "ipport.h"

#include "q.h"
#include "netbuf.h"
#include "net.h"


#ifdef MINI_TCP
#include "msock.h"
#else /* not MINI_TCP */
#include "tcpport.h"   /* NetPort embedded system includes */
#endif   /* MINI_TCP */

#endif	/* _TCPAPP_H */


