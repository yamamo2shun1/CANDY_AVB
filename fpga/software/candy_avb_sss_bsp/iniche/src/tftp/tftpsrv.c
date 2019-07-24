/*   tftpsrv.c
 *
 * Copyright 1998 by InterNiche Technologies Inc. All rights reserved.
 * Copyright 1995 by NetPort Software.
 * Copyright 1986 by Carnegie Mellon
 * Copyright 1984 by the Massachusetts Institute of Technology
 *
 * MODULE: TFTP
 *
 * ROUTINES: tfsinit(), tfs_on(), tfs_off(), tfshnd()
 *
 *  TFTP server code - uses common code in tftputil.c and udp
 *  interface in tftpudp.c
 *
 *  PORTABLE: yes
 */

#include "tftpport.h"

int tftp_server_on = FALSE;   /* TFTP server enable/disable state */

#ifdef TFTP_SERVER

#include "tftp.h"

/* user-supplied callback functions (see tfsinit()) */
static int (*tfs_alert)(ip_addr, char *, unshort);
static int (*tfs_done)(int status, struct tfconn *, char *);

extern int ntftps;
u_long refusedt = 0;      /*  time of most recent transfer refusal  */
void * tftpsconn = NULL;   /* server UDPCONN or socket */


/* FUNCTION: tfsinit()
 *
 * Initialize the TFTP server
 *
 * PARAM1: int (*)()          alert function pointer
 * PARAM2: int (*)()          done function pointer
 *
 * RETURNS: int               0 if successful, otherwise error code
 *
 * This opens a UDP connection but does not turn on the server.
 * That needs to done by an explicit call to tfs_on().
 *
 * alert() is a function which the server will call whenever it receives
 * a request for a transfer. This function will be called in the following
 * way:
 *     alert(ip_addr, file_name, direction)
 *
 * alert() should return TRUE if it wishes to allow the transfer and
 * FALSE otherwise.  
 *
 * done() is a function that the server will call to inform the invoker
 * that this file transfer is complete or aborted.
 */

int
tfsinit(int (*alert)(ip_addr, char *, unshort),    /* notification callback */
        int (*done)(int, struct tfconn *, char *)) /* xfer complete callback */
{
   unshort lport = TFTPPORT;  /* for pass to tftp_udplisten() */

   /* open server tftp connection to receive incoming requests
   with wildcard foriegn host & wildcard foriegn port */
   tftpsconn = tftp_udplisten(0L, 0, &lport, TFTPSERVER);

   if (!tftpsconn)
      return ENP_RESOURCE;

   tfs_done = done;
   tfs_alert = alert;

   return 0;
}

#endif  /* TFTP_SERVER */



/*
 * The following functions are required to be defined, even if 
 * there is no TFTP Server configured. If there is no TFTP Server,
 * the functions are simply "no-ops".
 */

/* FUNCTION: tfs_on()
 *
 * Enable the TFTP server
 *
 * PARAMS: none
 *
 * RETURNS: none
 */

void
tfs_on() 
{
#ifdef TFTP_SERVER
   tftp_server_on = TRUE;
#endif
}



/* FUNCTION: tfs_off()
 *
 * Disable the TFTP server
 *
 * PARAMS: none
 *
 * RETURNS: none
 */

void
tfs_off() 
{
#ifdef TFTP_SERVER
   tftp_server_on = FALSE;
#endif
}



/* FUNCTION: tfshnd()
 *
 * Handle an initial incoming TFTP packet
 *
 * PARAM1: ip_addr            IP address of sender
 * PARAM2: unshort            Sender's port number
 * PARAM3: char *             Pointer to sender's data
 *
 * RETURNS: int               0 if successful, otherwise ENP_ error code
 *
 * Opening a UDP connection (immediately so that we can report errors).
 * If the server is OFF then the tftp will be refused; otherwise more
 * checking will be done.  Call the alert function and verify that the
 * "user" wishes to allow the TFTP. Report an error if not. Finally,
 * spawn a session to oversee the TFTP and cleanup when it's done.  
 */

int
tfshnd(ip_addr host, unshort fport, char *udata)
{
#ifdef TFTP_SERVER
   struct tfreq *ptreq;
   char *file, *smode, *tmp;
   struct tfconn *cn = NULL;
   unsigned mode;
   int e = 0;  /* error value to return */

   /* Do we have room to do this transfer?  */
   if (ntftps >= MAXTFTPS) 
   {
      dprintf("TFTP Serve:  Ignoring req, too many connections\n");
      return ENP_RESOURCE;
   }

   ptreq = (struct tfreq *)udata;
   ptreq->tf_op = htons(ptreq->tf_op);
   if ((ptreq->tf_op != TF_RRQ) && (ptreq->tf_op != TF_WRQ))
   {
      dprintf("TFTP Server: bad init opcode %u\n", ptreq->tf_op);
      return ENP_LOGIC;
   }

   /* make conn now, in case we send errors */
   cn = tfmkcn();
   if (cn == (struct tfconn *)NULL) 
   {
      dprintf("TFTP Server: session Alloc failed\n");
      return ENP_RESOURCE;
   }
   cn->tf_start = cticks;
   cn->tf_fhost = host;
   cn->tf_fport = fport;
   cn->tf_lport = TFTPPORT;   /* errors come from std port for now */

   if (tftp_server_on != TRUE) 
   {
      dprintf("TFTP Server: got req while off\n");
      tfsnderr(cn, ERRTXT, "Transfers currently disabled.");
      tfcleanup(cn);
      return 0;   /* this is not an error */
   }

   /* get a pointer to the file name */
   file = (char*)ptreq + sizeof(ptreq->tf_op);

   if (tfs_alert)             /* call user's security function */
   {
      int code;
      unshort xmode = (unshort)(ptreq->tf_op == TF_RRQ ? PUT : GET);

      code = (*tfs_alert)(host, file, xmode);
      if (code == 0)
      {
         tfsnderr(cn, ERRTXT, "Transfer refused.");
         refusedt = cticks;
         tfcleanup(cn);
         return 0;
      }
   }

   /* determine the transfer mode. 
    * convert the transfer mode to lower case for easier comparison.
    */
   smode = file + strlen(file)+1;
   for (tmp = smode; *tmp; tmp++)
      if ((*tmp >= 'A') && (*tmp <= 'Z'))
         *tmp += ('a' - 'A');

   if ((strcmp(smode, "image") == 0) || (strcmp(smode, "octet") == 0))
      mode = OCTET;
   else if (strcmp(smode, "netascii") == 0)
      mode = ASCII;
   else 
   {
      dprintf("TFTP Server:  Bad mode %s in req\n", smode);
      tfsnderr(cn, ERRTXT, "Bad mode");
      tfcleanup(cn);
      return ENP_LOGIC;
   }

   /* open a UDP connection for data transfer with this host */
   cn->tf_lport = 0;          /* let UDP select local port */
   cn->tf_conn = tftp_udplisten(host, fport, &cn->tf_lport, (void*)cn);
   if (cn->tf_conn == (void *)NULL) 
   {
      dprintf("TFTP Server: UDP listen error\n");
      tfcleanup(cn);
      return ENP_RESOURCE;
   }
   cn->tf_mode = mode;
   cn->callback = tfs_done;

   /* is it a read request or a write request? */
   if (ptreq->tf_op == TF_RRQ) 
   {
      cn->tf_dir = PUT;
      if (cn->tf_mode == ASCII)   /* let file sys handle netascii */
         cn->tf_fd = vfopen(file, "r");
      else
         cn->tf_fd = vfopen(file, "rb");

      if (cn->tf_fd == (VFILE *)NULL) 
      {
         dprintf("TFTP server: couldn't open file\n");
         tfsnderr(cn, FNOTFOUND, " ");
         if (tfs_done)
            (*tfs_done)(TFC_FILEOPEN, cn, file);
         tfcleanup(cn);
         return ENP_FILEIO;
      }
      cn->tf_expected = 1;    /* start with block number 1 */
      cn->tf_flen = NORMLEN;
      cn->tf_state = RCVACK;  /* consider request to first ack */
   }
   else  /* ptreq->tf_op == TF_WRQ */
   {
      cn->tf_dir = GET;
      if (cn->tf_mode == ASCII)   /* let file sys handle netascii */
         cn->tf_fd = vfopen(file, "w");
      else
         cn->tf_fd = vfopen(file, "wb");

      if (cn->tf_fd == (VFILE *)NULL) 
      {
         dprintf("TFTP Server: couldn't open file\n");
         tfsnderr(cn, ACCESS, " ");
         if (tfs_done)
            (*tfs_done)(TFC_FILEOPEN, cn, file);
         tfcleanup(cn);
         return ENP_FILEIO;
      }
      cn->tf_expected = 0;    /* ack block number 0 */
      e = tfsndack(cn);
      cn->tf_expected = 1;    /* start with block number 1 */
      cn->tf_state = DATAWAIT;
   }

   return e;   /* OK return */
#else
   return 0;
#endif  /* TFTP_SERVER */
}

