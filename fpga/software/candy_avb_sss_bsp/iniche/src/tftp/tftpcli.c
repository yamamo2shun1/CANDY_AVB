/* tftpcli.c
 *
 * Copyright 1998 by InterNiche Technologies Inc. All rights reserved.
 * Copyright 1995 by NetPort Software.
 * Copyright 1986 by Carnegie Mellon
 * Copyright 1984 by the Massachusetts Institute of Technology
 *
 * MODULE: TFTP
 *
 * ROUTINES: tftpuse(), tfsndreq()
 *
 * TFTP client code - uses common code in tftputil.c and udp
 * interface in tftpudp.c
 *
 * PORTABLE: yes
 *
*/

#include "tftpport.h"

#ifdef TFTP_CLIENT

#include "tftp.h"


/* FUNCTION: tftpuse()
 *
 * Entry point for tftp transfers.
 *
 * PARAM1: ip_addr            IP address of foreign host
 * PARAM2: char *             name of local file
 * PARAM3: char *             name of remote file
 * PARAM4: unsigned           transfer direction: PUT or GET
 * PARAM5: unsigned           transfer mode: ASCII or OCTET
 * PARAM6: int (*)()          transfer "done" callback function
 *
 * RETURN: char *             NULL if successful, else error message
 *
 * Called from app to initiate a tftp transfer. A user-supplied 
 * callback function is invoked when the transfer is complete.
 *
 * Retuns NULL if transer started without errors, else returns
 * a pointer to text describing the error.
 */

char * 
tftpuse(ip_addr fhost, char *fname, char *rmfile, unsigned dir,
        unsigned mode, int (*callback)(int, struct tfconn*, char*))
{
   struct tfconn * cn;

   cn = tfmkcn();
   if (cn == (struct tfconn *)NULL)
      return ("alloc failed");

   cn->tf_mode = mode;
   cn->tf_start = cticks;
   cn->tf_fhost = fhost;
   cn->tf_dir = dir;
   cn->tf_mode = mode;
   cn->tf_fport = TFTPPORT;
   cn->tf_lport = 0;          /* let tftp_udplisten() assign port */
   cn->tf_fhost = fhost;
   cn->callback = callback;

   /* Open client tftp connection. Note that foreign port is wildcard
      since server will select it's own when it replys */
   cn->tf_conn = tftp_udplisten(cn->tf_fhost, 0, &cn->tf_lport, (void*)cn);
   if (cn->tf_conn == (void *)NULL)
      return ("Couldn't open UDP connection");

   if (dir == GET) 
   {
      if (mode == ASCII)
         cn->tf_fd = vfopen(fname, "w");
      else if (mode == OCTET)
         cn->tf_fd = vfopen(fname, "wb");
      else 
         return ("Invalid mode");

      if (cn->tf_fd == (VFILE *)NULL)
      {
         cn->tf_state = DEAD;
         return ("Couldn't open local file");
      }
      if (tfsndreq(cn, rmfile) != 0)
         return "Initial request failed";
      cn->tf_expected = 1;
      cn->tf_state = DATAWAIT;
   }
   else if (dir == PUT)
   {
      if (mode == ASCII)
         cn->tf_fd = vfopen(fname, "r");
      else if (mode == OCTET)
         cn->tf_fd = vfopen(fname, "rb");
      else 
         return ("Invalid mode");

      if (cn->tf_fd == (VFILE *)NULL)
         return ("Couldn't open file to send");

      if (tfsndreq(cn, rmfile) != 0)
         return "Initial request failed";
      cn->tf_expected = 0;
   }
   else   /* not tftp PUT or GET? */
   {
      dtrap();   /* prog error */
      return ("bad direction parm");
   }

   return ((char *)NULL);
}



/* FUNCTION: tfsndreq()
 *
 * Send an initial TFTP client transfer request
 *
 * PARAM1: tfconn *           TFTP connection
 * PARAM2: char *             remote file name
 *
 * RETURN: int                0 if successful, otherwise ENP_ error code
 *
 * Build a TFTP read or write request packet and send it.
 */

int
tfsndreq(struct tfconn * cn, char * fname)
{
   int err;
   int reqlen;    /* length of tftp request data */
   TFTPBUF p;
   struct tfreq *ptreq;
   char *cp;

   /* calculate request length: 
    *   size of request header
    *   + length of filename in octets + NUL
    *   + length of mode string + NUL
    */
   reqlen = sizeof(struct tfreq) + strlen(fname) + 1;
   if (cn->tf_mode == OCTET)
      reqlen += 6;   /* for "octet\0" */
   else if (cn->tf_mode == ASCII)
      reqlen += 9;   /* for "netascii\0" */
   else
   {
      dtrap();   /* bad mode; prog error */
      return FALSE;
   }

   tftp_udpbuffer(cn, reqlen);
   if (cn->tf_outbuf.data == NULL)
      return ENP_NOBUFFER;
   p = &cn->tf_outbuf;
   ptreq = (struct tfreq *)p->data;

   if (cn->tf_dir == GET) 
      ptreq->tf_op = TF_RRQ;
   else if (cn->tf_dir == PUT) 
      ptreq->tf_op = TF_WRQ;
   else 
   {
      dtrap();
      return ENP_PARAM;
   }

   /* point past request header for filename, and copy filename
    * and transfer mode strings into request */
   cp = (char*)ptreq + sizeof(ptreq->tf_op);
   strcpy(cp, fname);
   cp += strlen(cp) + 1;
   strcpy(cp, (cn->tf_mode == ASCII) ? "netascii" : "octet");

   err = tf_write(cn, reqlen);
   if (err == 0)
      cn->tf_state = ACKWAIT;
   else
   {
      dtrap();   /* Initial request failed? */
      tfkill(cn);
   }

   return err;
}

#endif   /* TFTP_CLIENT */

