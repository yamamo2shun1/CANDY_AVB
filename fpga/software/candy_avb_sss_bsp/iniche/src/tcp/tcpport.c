/*
 * FILENAME: tcpport.c
 *
 * Copyright  2000 By InterNiche Technologies Inc. All rights reserved
 *
 * InterNiche TCP layer's per-port routines. The portions of 
 * the TCP stack which may need to be modified for new ports are 
 * placed in this file.
 *
 * MODULE: INET
 *
 * ROUTINES: tcpinit(), tcp_sleep(), tcp_wakeup(),
 *
 * PORTABLE: no
 */

/* Additional Copyrights: */
/* Portions Copyright 1997 by InterNice Technologies Inc. */


#include "ipport.h"
#include "tcpport.h"

#ifdef INCLUDE_TCP  /* include/exclude whole file at compile time */

#include "in_pcb.h"
#include "tcp_timr.h"
#include "ip.h"   /* the NetPort ip.h */

/* bring to window sizes from tcp_usr.c for overwriting */
extern   u_long   tcp_sendspace;
extern   u_long   tcp_recvspace;


/* FUNCTION: tcpinit()
 *
 * tcpinit() - Set up TCP to run. Code to do port specific setup is 
 * in this file. This calls code for generic NetPort but setup in 
 * nptcp.c. This assumes netport IP, packet quques, interfaces, 
 * et.al., are already up. Returns 0 if OK, else non-zero error code. 
 *
 * 
 * PARAM1: none
 *
 * RETURNS: 0 if OK, else one of the ENP_ error codes
 */

int
tcpinit(void)
{
   int   e; /* error holder */

#ifdef DOS_TSR
   /* reduce some defaults to conserve TSR resources */
   tcp_sendspace = (TCP_MSS) * 2;
   tcp_recvspace = (TCP_MSS) * 2;
   TCPTV_MSL =    (4 * PR_SLOWHZ);     /* max seg lifetime default */
#endif

   e = nptcp_init();    /* call the NetPort init in nptcp.c */
   if (e)
      return e;

   return 0;   /* good return */
}



#ifndef TCPWAKE_ALREADY

/* the tcp process sleep and wakeup system.
 * these are the superloop versions, for other versions you need 
 * to define them in you port files and set TCPWAKE_ALREADY.
 *
 * A true multitasking version of these is in ..\misclib\netmain.c
 */


void *   last_arg;   /* for debugging */

/* FUNCTION: tcp_sleep()
 * 
 * PARAM1: void * timeout
 *
 * RETURNS: void
 */

void
tcp_sleep(void * timeout)
{
   UNLOCK_NET_RESOURCE(NET_RESID);
   tk_yield(); /* let the system run a bit... */
   LOCK_NET_RESOURCE(NET_RESID);

   last_arg = timeout;
}



/* FUNCTION: tcp_wakeup()
 * 
 * PARAM1: void * wake
 *
 * RETURNS: void
 */

void
tcp_wakeup(void * wake)
{
   last_arg = wake;
}
#endif   /* TCPWAKE_ALREADY */

#endif /* INCLUDE_TCP */


/* end of file tcpport.c */
