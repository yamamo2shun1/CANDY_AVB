/*   tftputil.c
 *
 * Copyright 1998-1999 by InterNiche Technologies, Inc. All rights reserved.
 * Copyright 1995 by NetPort Software.
 * Copyright 1986 by Carnegie Mellon
 * Copyright 1984 by the Massachusetts Institute of Technology
 *
 * MODULE: TFTP
 *
 * ROUTINES: tfmkcn(), tfcleanup(), tftptmo(), tf_good(), tfdodata()
 * ROUTINES: tf_full(), tfsnderr(), tfdoerr(), tfsndack(), tf_write()
 * ROUTINES: tfsndata(), tfdoack(), tftprcv(), tfkill()
 * ROUTINES: tftp_tick(), check_state()
 *
 * TFTP Client and Server utility routines
 *
 * PORTABLE: yes
 */

#include "tftpport.h"

#include "tftp.h"

/* internal routine */
static void check_state(struct tfconn * cn);

struct tfconn * tftp_conns = NULL;   /* list of active connections */
int ntftps = 0;   /* number of connections */


/* FUNCTION: tfmkcn()
 *
 * Setup a TFTP connection block
 *
 * PARAMS: none
 *
 * RETURN: struct tfconn *    TFTP connection block
 *                            or NULL if error
 *
 * Allocates and initializes a TFTP connection block.
 */

struct tfconn * 
tfmkcn(void)
{
   struct tfconn * cn;

   cn = (struct tfconn *)TFC_ALLOC(sizeof(struct tfconn));
   if (cn == (struct tfconn *)NULL)
      return (struct tfconn *)NULL;

   ntftps++;
   cn->next = tftp_conns;   /* add this tfconn to list */
   tftp_conns = cn;

   cn->tf_srtt = cn->tf_rt = TPS/2;   /* init round trip timer with large-ish guess */
   if (cn->tf_rt > TF_MAXTMO)
      cn->tf_rt = TF_MAXTMO;
   else if (cn->tf_rt < TF_MINTMO)
      cn->tf_rt = TF_MINTMO;
   cn->tf_NR_last = 1;   /* fake last packet Number retrys */

   return cn;
}



/* FUNCTION: tfcleanup()
 *
 * TFTP cleanup routine
 *
 * PARAM1: struct tfconn *    TFTP connection block
 *
 * RETURNS: none
 *
 * Called when we are done with the connection. 
 * Unlink the connection block from the TFTP list, and free
 * any allocated resources.
 */

void
tfcleanup(struct tfconn *cn)
{
   struct tfconn *tmp, *last;

   /* unlink cn from list */
   /* traverse list, looking of cn to free */
   last = (struct tfconn *)NULL;
   for (tmp = tftp_conns; tmp; tmp = tmp->next)
   {
      if (tmp == cn)
      {
         if (last)   /* unlink from list */
            last->next = tmp->next;
         else   /* was first in list, fix list pointer */
            tftp_conns = tmp->next;
         break;
      }
      last = tmp;
   }

   if (tmp == (struct tfconn *)NULL)      /* connection not in master list? */
   {
      dtrap();   /* serious prog error..... */
   }

   if (cn->tf_fd != (VFILE *)NULL)
      vfclose(cn->tf_fd);     /* close any open file */
   if (cn->tf_outbuf.data)
      tftp_udpfree(cn);       /* free any data buffer */
   if (cn->tf_conn != 0)
      tftp_udpclose(cn->tf_conn);   /* close the connection */

   TFC_FREE(cn);              /* free the connection block */
   ntftps--;
}



/* FUNCTION: tftptmo()
 *
 * Handle a TFTP timeout event 
 *
 * PARAM1: struct tfconn *    TFTP connection block
 *
 * RETURNS: none
 *
 * TFTP timed out waiting for a reply. Try resending the
 * previous packet, unless maximum retry limit has been exceeded,
 * in which case give up and kill the connection.
 */

void 
tftptmo(struct tfconn *cn)
{
   TFTPBUF p;

   cn->tf_tmo++;
   /* if we have more tries... */
   if (--cn->tf_tries >= 0)
   {
      int e;

      cn->tf_rsnd++;
      cn->tf_NR++;
      /* do the actual retry. Don't use tf_write() since it will re-init 
         the round trip & retry count values. */
      p = &cn->tf_outbuf;
      e = tftp_udpsend(cn, p->data, cn->tf_lastlen);
      if (e < 0)
      {
         if (cn->callback)
         {
            if (e == ENP_NOBUFFER)
               cn->callback(TFC_BUFFER, cn, "UDP alloc failed");
            else
               cn->callback(TFC_UDPSEND, cn, "UDP send failed");
         }
         tfkill(cn);    /* UDP send error, kill tftp session */
      }
      else  /* udp retry packet sent OK */
      {
         cn->tf_rt <<= 1;      /* double round trip est. */
         if (cn->tf_rt > TF_MAXTMO)
            cn->tf_rt = TF_MAXTMO;
         cn->tf_tick = cticks + cn->tf_rt;   /* set time to do next retry */
      }
   }
   else 
   {
      cn->tf_state = TIMEOUT;
   }
}



/* FUNCTION tf_good()
 *
 * Do timing calculations after good receives
 *
 * PARAM1: struct tfconn *    TFTP connection block
 *
 * RETURNS: none
 *
 * Calculate the round-trip time and refine the connection's
 * round-trip estimate.
 */

void
tf_good(struct tfconn *cn)
{
   unsigned long trt;

   trt = cticks - cn->tf_sent;  /*  Measured round trip time  */
   /* set smoothed round trip time based on measured */
   if ((cn->tf_srtt > trt) && (cn->tf_srtt > 1))
      cn->tf_srtt--;
   else if ((cn->tf_srtt) < trt)
      cn->tf_srtt++;
   /* set the retry time: experimental algorithm: */
   trt += cn->tf_srtt + 1;
   if (trt < TF_MINTMO)
      trt = TF_MINTMO;
   else if (trt > TF_MAXTMO)
      trt = TF_MAXTMO;
   cn->tf_rt = trt;

   cn->tf_NR_last = cn->tf_NR;
   cn->tf_NR = 0;
}



/* FUNCTION: tfdodata()
 *
 * Process a received data packet
 *
 * PARAM1: struct tfconn *    TFTP connection block
 * PARAM2: TFTPBUF            received data buffer
 * PARAM3: unsigned int       data length
 *
 * RETURNS: int               TRUE if OK, FALSE if error
 *
 * Process a data packet received for TFTP connection cn.
 * Handle out of sequence blocks and check on block length.
 * If a block is way too short (len < tftp header size), send
 * back an error message and abort the transfer. If the block
 * is less than NORMLEN bytes long, shut down the transfer; we're done.
 * Otherwise, just write it to disk (if necessary).
 */

int /* BOOL */
tfdodata(struct tfconn * cn, TFTPBUF p, unsigned len)
{
   char * data;
   struct tfdata *ptfdat;

   if (len < 4) 
   {
      tfsnderr(cn, ERRTXT, "Bad len (too short)");
      if (cn->callback)
         cn->callback(TFC_BADLEN, cn, "short data from peer");
      tfkill(cn);
      return FALSE;
   }

   if (cn->tf_state != DATAWAIT)
   {
      tfsnderr(cn, ERRTXT, "Rcvd unexpected data block");
      return FALSE;
   }

   /* point to tftp header at front of packet */
   ptfdat = (struct tfdata *)p->data;
   len -= 4;   /* subtract length of header from data */

   if (htons(ptfdat->tf_block) != cn->tf_expected)
   {
      /* We got a retransmission of a packet we have already tried to ACK.
       * If we retransmit the ACK, and the old ACK finally gets through also,
       * our correspondent will resend the next data block, and we will do
       * the same thing for it, on through to the end of the file.  So we
       * shouldn't retransmit the ACK until we are convinced that the first
       * one was actually lost.  We will become convinced if our own timeout
       * waiting for the next data packet expires.
       */
      cn->tf_ous++;   
      return FALSE;
   }
   tf_good(cn);   /* adjust timer values */

   cn->tf_size += len;        /* update file statistics */
   cn->tf_flen = len;

   /* write UDP data into file */
   if (len > 0)
   {
      data = ptfdat->tf_data;
      if ((int)vfwrite(data, 1, len, cn->tf_fd) != (int)len)
      {
         tf_full(cn);
         return FALSE;
      }
   }
   
   /* Send the ack. If len < NORMLEN, we've reached end-of-file */
   tfsndack(cn);
   if (len < NORMLEN)         /* last data block */
   {
      if (cn->tf_fd)          /* close the open file */
      {
         vfclose(cn->tf_fd);
         cn->tf_fd = (VFILE *)NULL;
      }
      cn->tf_state = RCVLASTDATA;   /* get ready to close the connection */
   }

   cn->tf_expected++;
   return TRUE;
}



/* FUNCTION: tf_full()
 *
 * Handle the "disk full" condition
 *
 * PARAM1: struct tfconn *    TFTP connection block
 *
 * RETURNS: none
 *
 * Handle disk full condition by sending error packet and killing
 * this connection.
 */

void
tf_full(struct tfconn *cn)
{
   tfsnderr(cn, DISKFULL, " ");
   tfkill(cn);   
   if (cn->callback)
       cn->callback(TFC_DISKFULL, cn, tferrors[DISKFULL] );
}



/*  */

char * tferrors[] = 
{
   "See text",
   "File not found",
   "Access violation",
   "Disk full",
   "Illegal TFTP operation",
   "Unknown transfer ID",
   "File already exists",
   "No such user"
};


/* FUNCTION: tfsnderr()
 *
 * Send an error packet
 *
 * PARAM1: struct tfconn *    TFTP connection block
 * PARAM2: unsigned int       error code
 * PARAM3: char *             error message text (if error code is zero)
 *
 * RETURNS: none
 *
 * Send an error packet. If the code is nonzero, use a pre-defined
 * error message. Otherwise, copy the text into the packet.
 */

void
tfsnderr(struct tfconn * cn, unsigned code, char *text)
{
   unsigned len;
   struct tferr *perr;

   /* use pre-defined error message? */
   if (code != ERRTXT)
      text = tferrors[code];

   /* length = header + string + NUL */
   len = 4 + strlen(text) + 1;

   /* allocate a buffer and fill in the fields */
   tftp_udpbuffer(cn, len);
   if (cn->tf_outbuf.data != (char *)NULL)
   {
      perr = (struct tferr *)(cn->tf_outbuf.data);
      perr->tf_op = htons(TF_ERROR);
      perr->tf_code = htons(code);
      strcpy(perr->tf_err, text);
      
      /* send the error message */
      tftp_udpsend(cn, perr, len);

      /* free now; there are no retrys on these */
      tftp_udpfree(cn);
   }
}



/* FUNCTION: tfdoerr()
 *
 * Process an incoming error packet 
 *
 * PARAM1: struct tfconn *    TFTP connection block
 * PARAM2: TFTPBUF            received data buffer
 * PARAM3: unsigned int       data length
 *
 * RETURNS: int               TRUE if OK, FALSE if error
 *
 * If the connection is still alive, kill it. Pass the 
 * error message to the app via the callback function.
 */

void
tfdoerr(struct tfconn *cn, TFTPBUF p, unsigned len)
{
   struct tferr *perr;
   int tferror;               /* tftp protocol error */
   char *errtxt;              /* text of/for error */

   /* if connection is dead, don't worry about this... */
   if ((cn->tf_state != DEAD) && (cn->tf_state != TERMINATED))
   {
      tfkill(cn);

      /* get the error code and error message */
      perr = (struct tferr *)p->data;
      tferror = htons(perr->tf_code);     /* get tftp error code (see rfc) */
      if ((tferror > 0) && (tferror < 8))
         errtxt = tferrors[tferror];      /* get pre-defined error text */
      else
      {
         /* not a known error, try to extract text from tftp packet */
         errtxt = perr->tf_err;
         if ((*errtxt <= ' ') || (*errtxt >= 0x7f) ||
             (strlen(errtxt) > 100))   /* make sure it's printable */
         {
            errtxt = "bogus tftp error text";
         }
      }

      /* notify app of error */
      if (cn->callback)
         cn->callback(TFC_FOREIGN, cn, errtxt);
   }

   USE_ARG(len);
}



/* FUNCTION: tfsndack()
 *
 * ACK the latest block
 *
 * PARAM1: struct tfconn *    TFTP connection block
 *
 * RETURNS: int               0 if success, else error code
 *
 * Build and send ack for latest block
 */

int
tfsndack(struct tfconn *cn)
{
   struct tfack *pack;
   int e = ENP_NOBUFFER;

   /* allocate buffer for ACK */
   tftp_udpbuffer(cn, sizeof(struct tfack));
   if (cn->tf_outbuf.data)
   {
      pack = (struct tfack *)(cn->tf_outbuf.data);

      cn->tf_lastlen = sizeof(struct tfack);
      pack->tf_op = TF_ACK;   /* in local endian, tf_write will fix */
      pack->tf_block = cn->tf_expected;

      /* send the ACK */
      e = tf_write(cn, sizeof(struct tfack));
   }

   return e;
}



/* FUNCTION: tf_write()
 *
 * Send a TFTP packet
 *
 * PARAM1: struct tfconn *    TFTP connection block
 * PARAM2: unsigned int       packet length
 *
 * Write (send) a tftp packet to the other host
 */

int
tf_write(struct tfconn *cn, unsigned len)
{
   struct tfack * packet;

   packet = (struct tfack *)(cn->tf_outbuf.data);
   if (packet == NULL)
   {
      dtrap(); /* prog error */
      return ENP_NOBUFFER;
   }

   if ((packet->tf_op != TF_RRQ) && (packet->tf_op != TF_WRQ))
   {
      packet->tf_block = htons(packet->tf_block);
      cn->tf_tries = TFTPTRIES;
   }      
   else 
      cn->tf_tries = REQTRIES;

   packet->tf_op = htons(packet->tf_op);   /* convert op to net format */

   cn->tf_lastlen = len;
   cn->tf_snt++;
   cn->tf_sent = cticks;
   cn->tf_tick = cticks + cn->tf_rt;   /* set time to retry */
   cn->tf_NR = 1;

   return (tftp_udpsend(cn, cn->tf_outbuf.data, len));
}



/* FUNCTION: tfsndata()
 *
 * Send a TFTP data block
 *
 * PARAM1: struct tfconn *    TFTP connection block
 *
 * RETURNS: int               0 if success, else error code
 *
 * Allocate a TFTP data buffer (packet). Read the next chunk
 * of file data, and send it. If this is a retry, use the 
 * previous chunk of file data.
 */

int
tfsndata(struct tfconn *cn)
{
   struct tfdata *tfdata;
   int bytes = 0;   /* bytes read from file */
   int err;

   tftp_udpbuffer(cn, sizeof(struct tfdata));
   if (cn->tf_outbuf.data == NULL)
      return ENP_NOBUFFER;

   tfdata = (struct tfdata *) (cn->tf_outbuf.data);
   tfdata->tf_op = TF_DATA;
   tfdata->tf_block = cn->tf_expected;

   /* load file data into tftp buffer */
   if (!cn->tf_NR)   /* if this is NOT a retry, read in new data */
   {
      /* read next block from the file */
      bytes = vfread(tfdata->tf_data, 1, NORMLEN, cn->tf_fd);
      if (bytes < NORMLEN)      /* end of file? */
      {
         if (vferror(cn->tf_fd))   /* see if it's an error */
         {
            if (cn->callback)
                cn->callback(TFC_FILEREAD, cn, "file read error");
            tfcleanup(cn);    /* free resources */
            return FALSE;
         }
         /* else at End Of File; fall through to do last send */
      }

      cn->tf_flen = bytes;   /* bytes in last packet sent */
      cn->tf_size += bytes;   /* total bytes sent so far */
   }
   else
   {
      dtrap();   /* can this happen? */
   }

   /* send the data block */
   err = tf_write(cn, sizeof(struct tfdata)-NORMLEN+bytes);
   if (err == 0)   /* if sent OK, wait for reply */
   {
      /* update tftp state */
      cn->tf_state = (cn->tf_flen == NORMLEN) ? ACKWAIT : SENTLAST;
   }
   else   /* else kill connection */
   {
      if (cn->callback)
      {
         if (err == ENP_NOBUFFER)
            cn->callback(TFC_BUFFER, cn, "UDP alloc failed");
         else
            cn->callback(TFC_UDPSEND, cn, "UDP send failed");
      }
      tfkill(cn);
   }

   return err;
}



/* FUNCTION: tfdoack()
 *
 * Handle an incoming ack
 *
 * PARAM1: struct tfconn *    TFTP connection block
 *
 * RETURNS: none
 */

void
tfdoack(struct tfconn *cn)
{
   struct tfack *ack;
   TFTPBUF p = &cn->tf_inbuf;

   ack = (struct tfack *)(p->data);

   if (htons(ack->tf_block) != cn->tf_expected) 
   {
      /* We have received an ACK,
         but not for the data block we sent.  It must be for
         a duplicate, since we wouldn't have sent
         the current data block if we hadn't gotten an ACK for
         the previous one.  This duplicate ACK means either
         that the network resent a packet that it wasn't sure
         got through, or else the other end resent the ACK
         because our current data block is lost or late.
         In either case, we can safely ignore this extra ACK,
         and if the ACK we want doesn't come our own timer will
         get us started again.  It isn't safe to resend the current
         data block now unless we are absolutely certain that the
         other end won't reack it if the earlier send was just delayed.
       */
      cn->tf_ous++;
   }
   else 
   {
      tf_good(cn);
      /* If ack was for last packet, we are done with this connection */
      if (cn->tf_state == SENTLAST)
      {
         if (cn->tf_fd)       /* close the file */
         {
            vfclose(cn->tf_fd);
            cn->tf_fd = (VFILE *)NULL;
         }
         cn->tf_state = TERMINATED;
      }
      else
         cn->tf_state = RCVACK;

      tftp_udpfree(cn);    /* free acked data buffer */
      cn->tf_expected++;   /* ready to do next block */
   }
}



/* FUNCTION: tftprcv()
 *
 * Handle an incoming TFTP packet
 *
 * PARAM1: struct tfconn *    TFTP connection block
 * PARAM2: unshort            port number
 *
 * RETURN: none
 *
 * This is upcalled from the tftpudp.c code which handles UDP semantics. 
 * Note that contents of cn->tf_inbuf are only good for the duration of
 * this upcall.
 */

void
tftprcv(struct tfconn *cn, unshort fport)
{
   TFTPBUF p;
   struct tfdata *pdata;
   unshort op;
   unsigned len;

   p = &cn->tf_inbuf;   /* data is in connection's buffer */
   len = p->dlen;      /* size of received data in buffer */

   /* If tf_fport is still not set, this should be ack for first block */
   if (cn->tf_fport == TFTPPORT)
   {
      struct tfack * ack = (struct tfack *)(p->data);

      if (htons(ack->tf_block) <= 1)   /* only do this on first block */
         cn->tf_fport = fport;   /* set port from upcalled value */
   }
   cn->tf_rcv++;
   pdata = (struct tfdata *) (p->data);
   op = htons(pdata->tf_op);
   pdata->tf_op = op;

   switch (op)
   {
   case TF_RRQ:   /* retry of original req? */
   case TF_WRQ:
      break;      /* silently ignore... */
   case TF_DATA:
      tfdodata(cn, p, len);
      break;
   case TF_ACK:
      tfdoack(cn);
      break;
   case TF_ERROR:
      tfdoerr(cn, p, len);
      break;
   default:
      dtrap(); /* printf("TFTPRCV: Got bad opcode %u.\n", op); */
      tfsnderr(cn, ILLTFTP, " ");
      break;
   }
   check_state(cn);   /* see if we need to send something */
}



/* FUNCTION: tfkill()
 *
 * Kill a connection
 *
 * PARAM1: struct tfconn *    TFTP connection block
 *
 * RETURNS: none
 *
 * Mark the connection "dead", and set the timer to clean up later.
 * Close any open files NOW.
 */

void
tfkill(struct tfconn *cn)
{
   if (cn->tf_fd)
   {
      vfclose(cn->tf_fd);
      cn->tf_fd = (VFILE *)NULL;
   }
   cn->tf_state = DEAD;             /* will get cleaned up later */
   cn->tf_tick = cticks + TPS;      /* clean up after one second */
}



static int in_tftp_tick = 0;  /* re-entry guard */

/* FUNCTION: tftp_tick()
 *
 * TFTP task/superlook handler
 *
 * PARAMS: none
 *
 * RETURNS: none
 *
 * This function scans all tftp connections to see if any states have
 * timed out. This should be called on every received tftp packet and
 * 1 second timer. 
 */

void
tftp_tick(void)
{
   struct tfconn * cn, * nextcn;

   if (in_tftp_tick)   /* avoid re-entring */
      return;

   in_tftp_tick++;

   for (cn = tftp_conns; cn; cn = nextcn)
   {
      nextcn = cn->next;   /* in case cn is deleted in check_state */
      if (cn->tf_tick > cticks)   /* time for state check? */
         continue;
      check_state(cn);
   }

   in_tftp_tick--;
}



/* FUNCTION: check_state()
 *
 * Check for activity on a TFTP connection
 *
 * PARAM1: struct tfconn *    TFTP connection block
 *
 * RETURNS: none
 *
 * Check the current state of a TFTP connection, and take the
 * appropriate action.
 */

static void
check_state(struct tfconn * cn)
{
   char *msg;

   /* prevent clock ticks from re-entering. Do not exit
   this routine without decrementing this counter! */
   in_tftp_tick++;

   switch (cn->tf_state)
   {
   case DATAWAIT:    /* waiting for data? */
   case ACKWAIT:     /* waiting for ack? */
   case SENTLAST:    /* sent last data block? */
      if (cn->tf_tick <= cticks)   /* timeout now? */
         tftptmo(cn);
      break;
   case TIMEOUT:
      tfsnderr(cn, ERRTXT, "Retry limit exceeded, giving up");
      if (cn->callback)
         cn->callback(TFC_TIMEOUT, cn, "retry limit exceeded");
      tfkill(cn);
      break;
   case RCVLASTDATA:   /* receive last incoming block of file */
   case TERMINATED:   /* sent last outgoing block of file */
      if (cn->callback)
      {
         if (cn->tf_size == 0L)
            cn->callback(TFC_ZEROFILE, cn, "zero length file");
         else
         {
            unsigned secs, tenths, elapsed;

            msg = cn->tf_inbuf.data;
            /* calculate elapsed ticks for transfer */
            elapsed = (unsigned)(cticks - cn->tf_start);
            /* convert to seconds and 10ths of a second */
            secs = elapsed/TPS;
            tenths = ((unsigned)(cticks - cn->tf_start) - (secs*TPS))/2;

            sprintf(msg, "Transferred %lu bytes in %u.%u seconds", 
                    cn->tf_size, secs, tenths);
            cn->callback(TFC_SUCCESS, cn, msg);
         }
      }
      cn->tf_state = DEAD;
      break;
   case RCVACK:   /* got an ack */
      tfsndata(cn);      /* send more data */
      break;
   case RCVDATA:
      tfsndack(cn);
      break;
   case DEAD:
      tfcleanup(cn);
      break;
   default:
      dtrap();   /* bad state */
      if (cn->callback)
         cn->callback(TFC_BADSTATE, cn, " ");
      tfkill(cn);
      break;
   }
   
   in_tftp_tick--;
}

