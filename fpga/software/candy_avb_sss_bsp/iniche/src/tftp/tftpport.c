/* tftpport.c
 *
 * Copyright 1998 by InterNiche Technologies Inc. All rights reserved.
 * Copyright 1995 by NetPort Software.
 * Copyright 1986 by Carnegie Mellon
 * Copyright 1984 by the Massachusetts Institute of Technology
 *
 * MODULE: TFTP
 *
 * ROUTINES: tftp_init(), tfs_done()
 *
 * TFTP per-port code
 *
 * PORTABLE: yes
 */

#include "tftpport.h"
#include "tftp.h"

#include "q.h"
#include "netbuf.h"
#include "net.h"
#include "ip.h"

#include "menu.h"

int tfs_done(int, struct tfconn *, char *);

#ifdef IN_MENUS
extern struct menu_op tftpmenu[];
#endif


/* FUNCTION: tftp_init()
 *
 * Initialize TFTP
 *
 * PARAMS: none
 *
 * RETURN: int                0 if successful, otherwise ENP_ error code
 *
 * Open a UDP port for listen on tftp port, do any prep 
 * required for client UDP use.
 */

int
tftp_init(void)
{
#ifdef TFTP_SERVER
   int e;

   e = tfsinit(NULL, tfs_done);
   if (e)
      dprintf("tftp server start err %d\n", e);
#endif   /* TFTP_SERVER */

#ifdef IN_MENUS
   if (install_menu(tftpmenu))
      dprintf("tftp client menu failure\n");
   install_version("tftp3.1");
#endif   /* IN_MENUS */

   return 0;
}



#ifdef TFTP_SERVER

/* FUNCTION: tfs_done()
 *
 * TFTP Server's "done" callback function
 *
 * PARAM1: int                completion status code
 * PARAM2: tfconn *           TFTP connection 
 * PARAM3: char *             status message
 *
 * RETURN: int                0 if successful, otherwise ENP_ error code
 *
 * Print completion status information on the Console
 */

int
tfs_done(int status, struct tfconn * cn, char * msg)
{
   ip_addr fhost = cn->tf_fhost;
   unsigned dir = cn->tf_dir;

   if (status == 0)
   {
      ns_printf(NULL, "tftp server: moved %lu bytes %s ",
                cn->tf_size, (dir==PUT) ? "to" : "from");
   }
   else  /* error of some kind */
   {
      ns_printf(NULL, "tftp server error: status %d, msg: %s, host:",
                status, (msg) ? msg : "None");
   }
   ns_printf(NULL, "%u.%u.%u.%u\n", PUSH_IPADDR(fhost));

   return 0;
}

#endif  /* TFTP_SERVER */

