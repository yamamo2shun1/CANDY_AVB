/* tftpmenu.c
 *
 * Copyright 1998 by InterNiche Technologies Inc.
 *
 * MODULE: TFTP
 *
 * ROUTINES: tftp_srv(), tftp_get(), tftp_put(),
 * ROUTINES: tftpio(), tftp_state(), tfc_done()
 *
 * menu hooks for TFTP 
 *
 * PORTABLE: yes
 *
 * 1/1/98 - Created. John Bartas
 */

#include "tftpport.h"   /* TCP/IP, sockets, system info */
#include "tftp.h"

#include "q.h"
#include "netbuf.h"
#include "net.h"
#include "ip.h"
#include "menu.h"

int tftp_srv(void *pio);
int tftp_get(void *pio);
int tftp_put(void *pio);
int tftp_state(void *pio);

/*
 * Altera Niche Stack Nios port modification:
 * Add braces to remove build warning
 */
#ifdef IN_MENUS
struct menu_op tftpmenu[] = {
#ifdef TFTP_CLIENT
   { "tftpc",    stooges,     "tftp client menu" } ,
   { "tfget",    tftp_get,     "tftp GET a file" } ,
   { "tfput",    tftp_put,     "tftp PUT a file" } ,
#endif /* TFTP_CLIENT */
#ifdef TFTP_SERVER
#if defined(TFTP_SERVER) && !defined(TFTP_CLIENT)
   { "tftps",    stooges,      "tftp server menu" } ,
   { "tfsrv",    tftp_srv,     "Toggle tftp server on/off" } ,
#else
   { "tfsrv",    tftp_srv,     "Toggle tftp server on/off" } ,
#endif
#endif /* TFTP_SERVER */
#ifdef NET_STATS
   { "tfstate",  tftp_state,  "Display tftp stats" } ,
#endif
   { NULL },
};
#endif /* IN_MENUS */

static char * usage = "usage: tf%s host sourcefile [destfile]\n";

extern char * prompt;   /* system menu prompt */

#ifdef TFTP_SERVER
extern int tftp_server_on;
#endif /* TFTP_SERVER */

/* internal routines */
static int tftpio(int direction, void *pio);
#ifdef TFTP_CLIENT
static int tfc_done(int status, struct tfconn *cn, char *msg);
#endif

void *last_pio;  /* lame attempt at supporting pio */


#ifdef TFTP_SERVER

/* FUNCITON: tftp_srv()
 *
 * Toggle TFTP Server on/off
 *
 * PARAM1: void *             PIO struct
 *
 * RETURN: int                0 if successful, otherwise ENP_ error code
 */

int
tftp_srv(void *pio)
{
   if (tftp_server_on == TRUE)
      tfs_off();
   else
      tfs_on();

   ns_printf(pio, "tftp server %s\n", tftp_server_on==TRUE?"ON":"OFF");

   return 0;
}

#endif /* TFTP_SERVER */



/* FUNCTION: tftp_get()
 *
 * Initiate TFTP Client "get" operation
 *
 * PARAM1: void *             PIO struct
 *
 * RETURN: int                0 if successful, otherwise ENP_ error code
 *
 * Call TFTP client get/put routine to "get" a file.
 */

int
tftp_get(void * pio)
{
   return tftpio(GET, pio);
}



/* FUNCTION: tftp_put()
 *
 * Initiate TFTP Client "put" operation
 *
 * PARAM1: void *             PIO struct
 *
 * RETURN: int                0 if successful, otherwise ENP_ error code
 *
 * Call TFTP client get/put routine to "put" a file.
 */

int
tftp_put(void * pio)
{
   return tftpio(PUT, pio);
}



/* FUNCTION: tftpio()
 *
 * Common TFTP Client "get"/"put" processing
 *
 * PARAM1: void *             PIO struct
 *
 * RETURN: int                0 if successful, otherwise ENP_ error code
 *
 * Initialte a file transfer operation for a TFTP client.
 */

static int 
tftpio(int dir, void * pio)
{
   ip_addr fhost;
   char *dfile;      /* source & dest file names */
   char *sfile;
   char *hostname = NULL;
   char *msg;
   int  e;
   GEN_IO io = (GEN_IO)pio;

   /* extract args from command buffer */
   if (io != (GEN_IO)NULL)
      hostname = nextarg(io->inbuf);
   if ((hostname == (char *)NULL) || (*hostname == '\0'))
   {
      ns_printf(pio, usage, (dir==PUT) ? "put" : "get");
      return -1;
   }
   sfile = nextarg(hostname);
   *(sfile-1) = '\0';         /* null terminate hostname */
   if (*sfile == '\0')
   {
      ns_printf(pio, usage, (dir==PUT) ? "put" : "get");
      return -1;
   }
   dfile = nextarg(sfile);
   if (*dfile != '\0')        /* if dfile was specified... */
      *(dfile-1) = '\0';      /* ...null terminate sfile */
   else                       /* if not specified default to sfile */
      dfile = sfile;

   msg = parse_ipad(&fhost, (unsigned *)&e, hostname);
   if (msg)
   {
      ns_printf(pio,"TFTP host address error: %s\n", msg);
      return -1;
   }
   /* if we're getting the file, swap src,dest file names */
   if (dir == GET)
   {
      msg = sfile;
      sfile = dfile;
      dfile = msg;
   }

   last_pio = pio;   /* save pio for callback */

   /* call tftp client code to create the session and start the transfer */
#ifdef TFTP_CLIENT
   msg = tftpuse(fhost, sfile, dfile, dir, OCTET, tfc_done);
#endif
   if (msg)
      ns_printf(pio, "TFTP client error: %s\n", msg);

   return 0;
}



/* FUNCTION: tftp_state()
 *
 * Display TFTP status on the PIO output stream
 *
 * PARAM1: void *             PIO struct
 *
 * RETURN: int                0 if successful, otherwise ENP_ error code
 */

int
tftp_state(void *pio)
{
   struct tfconn * cn;
   int nconns = 0;

#ifdef TFTP_SERVER
   ns_printf(pio, "tftp server state %s\n", tftp_server_on?"ON":"OFF");
#endif /* TFTP_SERVER */

   for (cn = tftp_conns; cn; cn = cn->next)
   {
      ns_printf(pio, "connection: %s %u.%u.%u.%u, state:%d, bytes moved: %ld \n",
                cn->tf_dir==PUT?"put to":"get from", 
                PUSH_IPADDR(cn->tf_fhost), cn->tf_state, cn->tf_size);
      nconns++;
   }
   ns_printf(pio, "%d connections open\n", nconns);

   return 0;
}


#ifdef TFTP_CLIENT

/* FUNCTION: tfc_done()
 *
 * TFTP Client "done" callback function
 *
 * PARAM1: int                completion status code
 * PARAM2: tfconn *           TFTP connection
 * PARAM3: char *             completion status message
 *
 * RETURN: int                0 if successful, otherwise ENP_ error code
 *
 * Print transfer completion statistics on the "current" output stream.
 */

int
tfc_done(int status, struct tfconn *cn, char *msg)
{
   ip_addr fhost = cn->tf_fhost;
   unsigned dir = cn->tf_dir;

   ns_printf(last_pio, "tftp %s %u.%u.%u.%u done; ",
             (dir == PUT) ? "to" : "from", PUSH_IPADDR(fhost));

   ns_printf(last_pio, "msg: %s, status:%s(%d)\n",
             msg ? msg : "None", (status == 0) ? "ok" : "error", status);

   last_pio = NULL;  /* clear this - default back to console */

   return 0;
}

#endif /* TFTP_CLIENT */

