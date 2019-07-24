/*
 * FILENAME: ipport.c
 *
 * Copyright  2000 By InterNiche Technologies Inc. All rights reserved
 *
 *
 * MODULE: INET
 *
 * ROUTINES: prep_ifaces(), netexit(),
 *
 * PORTABLE: no
 */

/* Additional Copyrights: */

/* Portions Copyright 1994 by NetPort Software  */

#include "ipport.h"
#include "q.h"
#include "netbuf.h"
#include "net.h"
#include "ip.h"

#ifdef MAC_LOOPBACK
extern   int  prep_lb(int already_found);
#endif

int(*port_prep)(int already_found) = NULL;

u_char so_evtmap;
int (*so_evtmap_create) (struct socket *);
int (*so_evtmap_delete) (struct socket *);


/* FUNCTION: prep_ifaces()
 * 
 * prep-ifaces() - prepare ifaces this stack may want to use. This is 
 * passed the nets[] index of the first interface to set up.
 *
 * PARAM1: int ifaces_found
 *
 * RETURNS:  It the net index of the last interface it set up. If no 
 * interfaces were set up, the returned value is the same as the 
 * passed value. 
 */

int
prep_ifaces(int ifaces_found)
{
   if (port_prep)
      ifaces_found = port_prep(ifaces_found);

#ifdef MAC_LOOPBACK
   ifaces_found = prep_lb(ifaces_found);
#endif   /* MAC_LOOPBACK */

   ifNumber = ifaces_found;   /* set global interface counter */

   initmsg("prepped %u interface%s, initializing...\n", 
      ifaces_found, ifaces_found==1?"":"s");

   return ifaces_found;
}


/* the the ipport.h did not define an exit func then map it to standard exit() */

#ifndef PORT_EXIT_FUNC
#define  PORT_EXIT_FUNC(code) exit(code)
#endif

int      net_system_exit   =  FALSE;



/* FUNCTION: netexit()
 * 
 * netexit() - this is called in place of system exit().
 * It undoes things like the clock tick and MAC drivers
 * hook which will hose the OS if we leave them installed. On some
 * embedded systems it may not be required. 
 *
 * PARAM1: int err
 *
 * RETURNS: SHould not return
 */

void
netexit(int err)      /* exit error level */
{
   net_system_exit = TRUE; /* set flag for shutting down */
   ip_exit();  /* do the exit_hook()ed stuff */

   PORT_EXIT_FUNC(err);    /* should not return! */
   USE_ARG(err);
}


/* FUNCTION: evtmap_setup ()
 *
 * setup event map for (UDP and TCP) socket library's events (such as 
 * those used by tcp_sleep () and tcp_wakeup ()).  These events either 
 * map into operating system primitives such as events, or into task 
 * suspend and task resume mechanisms.  In the former case, this 
 * function sets up the function pointers that will be used (by code
 * in the sockets library (socket.c and socket2.c)) to create and delete 
 * the corresponding events.  In the latter case, the function pointers 
 * are set to 0, indicating that they are not used.
 *
 * INPUT: None.
 * OUTPUT: None
 */
 
void evtmap_setup (void)
{
#ifdef SOCK_MAP_EVENTS
   so_evtmap = TRUE;
   so_evtmap_create = evtmap_create;
   so_evtmap_delete = evtmap_delete;
#else
   so_evtmap = FALSE;
   so_evtmap_create = 0;
   so_evtmap_delete = 0;
#endif   /* SOCK_MAP_EVENTS */ 

}

