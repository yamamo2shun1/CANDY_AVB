/*
 * FILENAME: t.c
 *
 * Copyright 1998- 2000 By InterNiche Technologies Inc. All rights reserved
 *
 *
 * MODULE: MISCLIB
 *
 * ROUTINES: tel_new(), tel_addq(), tel_delq(), 
 * ROUTINES: tel_delete(), tel_loop(), tel_tcp_send(), tel_tcp_recv(), 
 * ROUTINES: tel_process(), tel_conn_from_sock(), tel_read(), 
 * ROUTINES: tel_sp_char_proc(), tel_proc_cmd(
 *
 * PORTABLE: yes
 */


/* File telnet.c Implementation file for Telnet Server 
 */
#include "telnet.h"

#ifdef TELNET_SVR       /* Then include this source file */

static int max_telnet_conn = MAX_TELNET_SESS;
static int telnet_connect  = 0;  /* Current count of telnet connections */

TEL_CONN telq=NULL;     /* Start of queue of open connections */
/* last_error is used to store the last error for functions
 * that return meaningful values (eg funcs returning char *)    
 */
long     last_error;    

struct TelStatistics tel_stats;

#ifdef TEL_USERAUTH
char *tel_login_prompt  = "\nlogin: ";
char *tel_passwd_prompt = "password: ";
#endif

/* extra space in cmdbuf kept, to account for increased space due to
 * potential CRLF substitution */
#define TEL_CRLFPAD    10 


/* FUNCTION: tel_new()
 * 
 * Allocate memory for a new TelnetConn object 
 * A TelnetConn object holds all information about a
 * telnet connection. Also it adds the new object to the
 * open connection queue (telq).
 *
 * PARAM1: None
 *
 * RETURNS: A pointer to the NEW telnet object. NULL if error.
 */

TEL_CONN tel_new()
{
   TEL_CONN tconn;

   /* Increment the count of connections and compare against configured max */
   telnet_connect++;
   if (max_telnet_conn > 0)
   {
      if (telnet_connect > max_telnet_conn)
      {                   /* connection max exceeded */
         telnet_connect--;/* decrement count of connections */ 
         return NULL;     /* do not accept connection */
      }
   }

   tconn=(TEL_CONN)TEL_ALLOC(sizeof(struct TelnetConn));
   if (tconn == NULL)
   {
      last_error = TEL_NEW_ALLOC_ERR;
      return NULL;
   }

   tconn->cmdbuf = (char *) TEL_ALLOC(tglob.tel_recv_buf_size);
   if (tconn->cmdbuf == NULL)
   {
      /* Could not Allocate space for receive buffer */
      TEL_FREE(tconn);
      last_error = TEL_NEW_CANT_ALLOC_CMD_BUF;
      return NULL;
   }

   tconn->txbuf = (char *) TEL_ALLOC(tglob.tel_send_buf_size);
   if (tconn->txbuf == NULL)
   {
      /* Could not Allocate space for send(transmit) buffer */
      TEL_FREE(tconn->cmdbuf);
      TEL_FREE(tconn);
      last_error = TEL_NEW_CANT_ALLOC_SEND_BUF;
      return NULL;
   }

   tconn->rxbuf = (char *) TEL_ALLOC(tglob.tel_recv_buf_size);
   if (tconn->rxbuf == NULL)
   {
      /* Could not Allocate space for receive buffer */
      TEL_FREE(tconn->txbuf);
      TEL_FREE(tconn->cmdbuf);
      TEL_FREE(tconn);
      last_error = TEL_NEW_CANT_ALLOC_RECV_BUF;
      return NULL;
   }

   tconn->options = (struct TelOptionList *) 
   TEL_ALLOC(sizeof(struct TelOptionList));
   if (tconn->options == NULL)
   {
      /* Could not Allocate space for options */
      TEL_FREE(tconn->rxbuf);
      TEL_FREE(tconn->txbuf);
      TEL_FREE(tconn->cmdbuf);
      TEL_FREE(tconn);
      last_error = TEL_NEW_CANT_ALLOC_OPTIONS;
      return NULL;
   }
   else
   {
      /* Copy the default options and their values */
      *(tconn->options)=tel_opt_list;
   }

   if (tel_addq(tconn) != SUCCESS)
   {
      /* Could not add to queue ! Wierd */
      TEL_FREE(tconn->options);
      TEL_FREE(tconn->rxbuf);
      TEL_FREE(tconn->txbuf);
      TEL_FREE(tconn->cmdbuf);
      TEL_FREE(tconn);
      last_error = TEL_NEW_CANT_ADD_TO_Q;
      return NULL;
   }

   /* If user authentication is enabled, then login, password need to be
      verified */
#ifdef TEL_USERAUTH
   tconn->state = TEL_LOGIN;
   tconn->prompt= tel_login_prompt;
   tconn->tries = 0;
#else
   tconn->state = TEL_NORMAL;
   tconn->prompt= prompt;
#endif

   tel_start_timer(&tconn->curr_tick);
   tconn->cmdsize=0;
   tconn->txsize=0;
   tconn->rxsize=0;
   tconn->inserver=0;

   tel_stats.conn_opened++;

   return tconn;
}

/* FUNCTION: tel_connection()
 *
 * Handle accept of a new telnet connection - basicly a wrapper
 * for tel_new();
 *
 * PARAM1: temp_sock - new socket returned from accept
 *
 *
 * RETURNS: Pointer to new connection if OK, else NULL (e.g. alloc failed)
 */

TEL_CONN
tel_connection(SOCKTYPE temp_sock)
{
   TEL_CONN temp_conn;

   temp_conn = tel_new();
   if (temp_conn != NULL)
   {
      temp_conn->sock = temp_sock;
      temp_conn->tel_io.id = (long)temp_sock;
      temp_conn->tel_io.out = tel_tcp_send;
      /* mark the connection in use, so that it isn't 
         processed by the tk_yield() inside ns_printf(). */
      temp_conn->inserver = 1;
      tel_start_nego(temp_conn);
      ns_printf(&temp_conn->tel_io,
                "Welcome to InterNiche Telnet Server 1.0\n");
      if (temp_conn->state == TEL_NORMAL)
      {
         ns_printf(&temp_conn->tel_io,
                   "Please type ? or help for more information.\n");
      }
      ns_printf(&temp_conn->tel_io,"\n%s",temp_conn->prompt);
   }
   else
   {
      dprintf("Error #%ld opening new connection.\n",last_error);
      return NULL;
   }

   /* It's safe to allow processing now. */
   temp_conn->inserver = 0;

   return temp_conn;
}


/* FUNCTION: tel_addq()
 * 
 * Add a telnet object to global queue (of open connections)
 *
 * PARAM1: TEL_CONN tconn - Pointer to object to be added
 *
 * REMARKS: No Allocation is done. Only the queue is updated
 *
 * RETURNS: SUCCESS or error number
 */

int tel_addq(TEL_CONN tconn)
{
   if (telq == NULL)     
   {
      /* Queue is empty */
      telq=tconn;
      telq->next = NULL;
   }
   else
   {
      /* Add it to the front of the queue */
      tconn->next = telq;
      telq=tconn;
   }
   return SUCCESS;
}


/* FUNCTION: tel_delq()
 * 
 * Delete a telnet object from the  global queue 
 *
 * PARAM1: TEL_CONN tconn - Pointer to object to be deleted
 *
 * REMARKS: No Free (deallocation) is done. Only the queue is updated
 *
 * RETURNS: SUCCESS or error number
 */

int tel_delq(TEL_CONN tconn)
{
   TEL_CONN temp_conn;
   TEL_CONN prev_conn;
   int retval = TEL_DELQ_CONN_NOT_FOUND;     /* assume no connection */

   /* Check for empty queue */
   if (telq == (TEL_CONN)NULL)
   {     
      retval = TEL_DELQ_Q_EMPTY;
   }
   else
   {
      /* Find the item and delete it */
      prev_conn = (TEL_CONN)NULL;
      temp_conn = telq;

      LOCK_NET_RESOURCE(NET_RESID);

      while (temp_conn != (TEL_CONN)NULL) 
      {
         if (temp_conn == tconn)
         {
            if (prev_conn == (TEL_CONN)NULL)    /* first entry in list */
               telq = tconn->next;
            else                                /* middle of list */
               prev_conn->next = tconn->next;
            retval = SUCCESS;    /* found a connection */
            break;
         }

         /* advance to next list entry */
         prev_conn = temp_conn;
         temp_conn = temp_conn->next;
      }
   
      UNLOCK_NET_RESOURCE(NET_RESID);
   }

   return retval;
}



/* FUNCTION: tel_delete()
 * 
 * Free memory for a the TelnetConn object 
 * A TelnetConn object holds all information about a
 * telnet connection. This function frees all memory that
 * was allocated for the object. It also deletes the object
 * the telq (Queue of open telnet connections)
 * Also close the underlying TCP connection.
 *
 * PARAM1: TEL_CONN tconn - A pointer to the telnet object. 
 *
 * RETURNS: SUCCESS or error number 
 */

int tel_delete(TEL_CONN tconn)
{
   int   retValue=SUCCESS;

   if (tconn == NULL)
      return TEL_DEL_NULL_ARGUMENT;

   /* Remove the connection from the queue */
   retValue = tel_delq(tconn);
   if (retValue != SUCCESS)
   {
      dtrap();
   }

   /* Close the underlying TCP connection */
   sys_closesocket(tconn->sock);

   if (tconn->options)
   {
      TEL_FREE(tconn->options);
      tconn->options=NULL;
   }

   if (tconn->rxbuf)
   {
      TEL_FREE(tconn->rxbuf);
      tconn->rxbuf=NULL;
   }
   else
   {
      /* What ? Receive buffer has already been freed !! */
      dtrap();
   }

   if (tconn->txbuf)
   {
      TEL_FREE(tconn->txbuf);
      tconn->txbuf=NULL;
   }
   else
   {
      /* What ? Send buffer has already been freed !! */
      dtrap();
   }

   if (tconn->cmdbuf)
   {
      TEL_FREE(tconn->cmdbuf);
      tconn->cmdbuf=NULL;
   }
   else
   {
      /* What ? Command buffer has already been freed !! */
      dtrap();
   }

   TEL_FREE(tconn);

   telnet_connect--;   /* Decrement count of connections */
   tel_stats.conn_closed++;

   return retValue;
}


/* FUNCTION: tel_loop()
 * 
 * Check the status of each open telnet connection.
 *
 * PARAM1: none
 *
 * RETURNS: SUCCESS , else error number
 */

int tel_loop()
{
   TEL_CONN temp_conn;
   TEL_CONN next_conn;
   int e;

   temp_conn=telq;

   while (temp_conn != NULL)
   {
      next_conn = temp_conn->next;
      if (temp_conn->inserver == 0)    /* is this connection blocked? */
      {
         temp_conn->inserver++;    /* set re-entry prevention flag */
         e=tel_process(temp_conn);
         /* There are some cases when the connection would have been
          * deleted 
          */
         if ((e == TEL_PROC_TIMED_OUT)   || (e ==  TEL_PROC_TCP_CLOSED)  ||
             (e == TEL_PROC_FATAL_ERROR) || (e ==  TEL_PROC_AUTH_FAILED) ||
             (e == TEL_PROC_LOGGED_OUT))
         {
            ; /* Do nothing. Memory pointed by temp_conn is already freed */
         }
         else
            temp_conn->inserver--;    /* reset re-entry prevention flag */
      }
      temp_conn=next_conn;
   }

   return SUCCESS;
}


/* FUNCTION: tel_tcp_send()
 * 
 * Function which outputs the contents to telnet session.
 * This function can be used with the GenericIO structure.
 *
 * PARAM1: long s - Index to the output device. In this case, it
 * will be the socket number of telnet session.(IN)
 * PARAM2: char *buf - Buffer of data (IN)
 * PARAM3: int len - Length of data in buffer (IN)
 * Length is needed so that we can output hex data also.
 *
 * REMARKS: This function provides the translation from \n to \r\n.
 * Telnet needs CRLF sequence. So all occurances of LF
 * are replaced by CRLF. 
 * Algorithm   : The algorithm used for this is as follows.
 * 1. Scan the input string and find out the position
 * of all occurances of LF. And the count (num_of_newln).
 * 2. Let us assume that the string is now having 
 * num_of_newln+1 blocks, each separated by LF.
 * 3. The size of new string would be oldlength+num_of_newln.
 * 4. Move the last block to end of new string.
 * 5. Insert a CR just before the first char of this block.
 * 6. Now move the second last block to its new position.
 * Insert a CR just before its first char.
 * 7. Repeat for all blocks.
 * We could have used a simpler (to code) algorithm where
 * we move the whole block everytime we find LF and insert
 * a CR. A typical case of time-space tradeoff.
 *
 * RETURNS: Number of bytes sent.
 */

int
tel_tcp_send(long desc, char *buf, int len)
{
   int   i;
   int   newln_pos[TEL_MAX_NEWLN];
   int   num_of_newln=0;
   int   start,end;
   TEL_CONN t;
   SOCKTYPE s;

   s = (SOCKTYPE)desc;     /* set socket from genericIO descriptor */

   t = tel_conn_from_sock(s);

   if (t == NULL)
   {
      /* Whoops !! No telnet connection corresponding to this sock ? 
       * One scenario when this happens for InterNiche stack is as 
       * follows. 
       * 1. User Opens TELNET session. 
       * 2. User does TCP Echo (via TCP Echo Client) 
       * 3. User closes TELNET connection (w/o closing TCP Echo Client conn) 
       * 4. After a while tcp_echo_poll() realises that the TCP 
       *    Echo Client has been idle for a long time and so it 
       *    a. Sends a message informtion intention to clean up TCPEchoClient. 
       *       (Thats when tel_tcp_send() hits this dtrap()) 
       *    b. Closes the TCP Echo Client. 
       */
      dtrap();
      return 0;
   }
   else
   {
      if (t->options->binary.l_value == FALSE)
      {
         /* Substitue LF by CRLF if we have NON-BINARY mode for peer */
         /* First find all newline(LF) posltions */

         for (i=0; i < len; i++)
         {
            if (buf[i] == '\n')
            {
               if (num_of_newln >= TEL_MAX_NEWLN)
               {
                  /* There are more that TEL_MAX_NEWLN newline chars in this str*/
                  dtrap();
               }
               else
               {
                  newln_pos[num_of_newln++]=i;        
               }
            }
         }

         /* Now insert CR before all LF */
         end=len;
         for (i= num_of_newln-1; i >= 0; i--)
         {
            start=newln_pos[i];
            MEMMOVE(buf+start+i+1,buf+start, end-start);
            buf[start+i]  ='\r';   /* insert CR */
            buf[start+i+1]='\n';   /* insert LF. Somehow MEMMOVE was making it 0 */
            end=start;
         }
         buf[len+num_of_newln] = 0;   /* NULL terminate */
      }

      /* Some data will be written. Update the timer */
      tel_start_timer(&(t->curr_tick));

      return sys_send(s, buf, len+num_of_newln, 0);
   }
}


/* FUNCTION: tel_tcp_recv()
 * 
 * Function which inputs from a telnet session.
 * This function can be used with the GenericIO structure.
 *
 * PARAM1: SOCKTYPE s
 * 1. Socket form which input is to be done (IN)
 *
 * RETURNS: The first byte that has been received.
 * Value -1 is returned if some error occured.
 * Value 0 is returned if nothing was received
 * TELNET uses GenericIO mechanism to do I/O. Normal TELNET input 
 * (for example commands, options) are done in TEL_NORMAL state.
 * So, when a command is received in TEL_NORMAL state, TELNET calls
 * the corresponding routines to execute it. The routine uses ns_printf
 * to send output to the TELNET client. Sometimes, this routine might
 * need prompting from the TELNET clinet (For example, while displaying
 * a big buffer, prompting of "Press any key to continue" can be done
 * after showing a screenfull of information). So in such a case, 
 * via the GenericIO structure, tel_tcp_recv() would be called, and
 * the connection will be in TEL_PROC_CMD state. When the command
 * processing is done, the connection should return to TEL_NORMAL state.
 * This is achieved by doing the state change when the user has pressed
 * a key.
 * In "Press any key to continue" kindof scenarios, this works perfectly.
 * There can be some other scenarios. Lets take UDP ECHO (udecho command).
 * The user type the command to do 10 echos. The corresponding routine
 * starts processing, and keeps checking for user input. If any key has
 * been pressed, it stops doing UDP ECHOs. So this routine would periodically
 * call tel_tcp_recv() to see is any data has arrived. For cases like
 * 10 echos, the user may not press any key. So, we should have some
 * mechanism to return to normal state. This is done by doing a check
 * when the execution of the routine is over. It is made sure that TELNET
 * connection is returns to normal state.
 */

int
tel_tcp_recv(long iodesc)
{
   TEL_CONN t;
   SOCKTYPE s = (SOCKET)iodesc;

   t = tel_conn_from_sock(s);
   if (t == NULL)
   {
      /* Whoops !! No telnet connection corresponding to this sock ? */
      dtrap();
      return -1;
   }
   else
   {
      t->state = TEL_PROC_CMD;
      if (t->rxsize == 0 )
      {
         if (tel_read(t) != SUCCESS)
            return -1;     /* some fatal error on this connection */
      }

      if (t->rxsize == 0 )
      {
         return 0;   /* No data was received */
      }
      else
      {
         t->state = TEL_NORMAL;
         t->rxsize= 0;  /* discard all new input */
         return ((int)(t->rxbuf[0]));  /* Return the first char received */
      }
   }
}


/* FUNCTION: tel_process()
 * 
 * Do the processing for the open connection. If some date
 * needs to be sent, send it, and if some data has been
 * received, process it.
 * 1. If the lower TCP layer has closed, then cleanup up
 * the telnet session.
 *
 * PARAM1: TEL_CONN tconn - Pointer to telnet connection
 *
 * RETURNS: SUCCESS , else error number
 * later-if connection is open for a long time, close the connection
 */

int tel_process(TEL_CONN tconn)
{

   int   e;
   int   old_cmdsize;

#ifdef  TEL_SESS_TIMEOUT
   /* Terminate the session if it has been idling around long enough */
   if (tel_check_timeout(tconn->curr_tick,(u_long)tglob.tel_idle_time) == SUCCESS)
   {
      tel_delete(tconn);
      return TEL_PROC_TIMED_OUT;
   }
#endif

   if (tconn->state == TEL_PROC_CMD)
      return SUCCESS;

   if ((e=tel_read(tconn)) != SUCCESS)
      return e;

   if (tconn->rxsize > 0)
   {
      /* Process telnet commands and strip them out of rxbuf */
      if (tel_parse(tconn) != SUCCESS)
      {
         /* Error in parsing the received string */
         dtrap();
      }

      /* Now see if we have some new DATA */
      if (tconn->rxsize > 0)
      {
         /* Add it to cmdbuf, so that it contains all the unprocessed
            command till now */

         /* do a sanity check, so that cmdbuf is not exceeded */
         if (tconn->cmdsize+tconn->rxsize > 
            (tglob.tel_recv_buf_size- TEL_CRLFPAD))
         {
            /* received more data than can be held by cmdbuf.
             * ignore it for now - in future/later, increase sizeof cmdbuf
             */

            /* find out how much of received data can be used. Keep
             * extra buffer of 10 for CRLF processing in tel_sp_char_proc() */
            int tobeused=tglob.tel_recv_buf_size - tconn->cmdsize - TEL_CRLFPAD;

            old_cmdsize=tconn->cmdsize;
            if (tobeused > 0)
            {
               MEMCPY(tconn->cmdbuf+tconn->cmdsize,tconn->rxbuf,tobeused);
               tconn->cmdsize+=tobeused;
            }
            tconn->rxsize=0;
            /* put CRLF at the end of it, so that cmdline is processed */
            tconn->cmdbuf[tglob.tel_recv_buf_size-TEL_CRLFPAD-2]= TEL_CR;
            tconn->cmdbuf[tglob.tel_recv_buf_size-TEL_CRLFPAD-1]= TEL_LF;
            ns_printf(&tconn->tel_io, "Cmd too big (%d bytes), truncating \n",
               tglob.tel_recv_buf_size-TEL_CRLFPAD);
         }
         else
         {
            old_cmdsize=tconn->cmdsize;
            MEMCPY(tconn->cmdbuf+tconn->cmdsize,tconn->rxbuf,tconn->rxsize);
            tconn->cmdsize+=tconn->rxsize;
            tconn->rxsize=0;
         }

         /* Do processing for special characters like BackSpace, ESC
            and prepare output buffer to be ECHOed */
         tel_sp_char_proc(tconn->cmdbuf,&(tconn->cmdsize),
          tconn->txbuf,&(tconn->txsize), old_cmdsize);

         if (tconn->options->echo.l_value == TRUE)
         {
            if (tconn->txsize >  0 && tconn->state !=TEL_PASSWORD)
            {
               /* Echo the data back to the terminal */
               sys_send(tconn->sock,tconn->txbuf,tconn->txsize,0);
            }
         }
         tconn->txsize=0;     /* Discard the output buffer */

         /* Check if we have received a CRLF. If yes, then we have 
          * received a whole command and can execute it. Do this only 
          * if the received buffer has atleast 2 bytes 
          */
         if (tconn->cmdsize > 1)
         {
            /* Do menu-command processing in non-binary mode */

            if (tconn->options->binary.r_value == FALSE)
            {
               /* process all commands in cmdbuf*/
               e=tel_proc_cmd(tconn);
               if (e != SUCCESS) 
               {
                  /* This can happen if the user logs out */
                  return e;
               }
            }
            else
            {
               /* Binary mode . Do nothing */
               tconn->cmdsize=0;
            }
         }
         else
         {
            /* Do nothing. */
         }

      }
      else
      {
         /* do some sanity checking */
         if (tconn->rxsize < 0)
         {
            dtrap();
            tconn->rxsize=0;
         }
      }

   }
   else
   {
      /* do some sanity checking */
      if (tconn->rxsize < 0)
      {
         dtrap();
         tconn->rxsize=0;
      }
   }

   return SUCCESS;   /* OK return */
}


/* FUNCTION: tel_conn_from_sock()
 * 
 * Find the TelnetConn for a particular socket.
 *
 * PARAM1: SOCKET s - Socket  (IN)
 *
 * RETURNS: Pointer to TelnetConn structure or NULL
 */

TEL_CONN 
tel_conn_from_sock(SOCKET s)
{
   TEL_CONN t;

   for (t = telq; t != (TEL_CONN)NULL; t = t->next)
   {
      if (t->sock == s)
         break;
   }

   return t;
}



/* FUNCTION: tel_read()
 * 
 * Read data from a open telnet session
 *
 * PARAM1: TEL_CONN t - Pointer to telnet connection (IN)
 *
 * RETURNS: 1. SUCCESS if 0 or more bytes received
 * 2. error code if the underlying TCP connection was broken
 */

int
tel_read(TEL_CONN t)
{

   int   readval,e;
   readval= sys_recv(t->sock, t->rxbuf, tglob.tel_recv_buf_size , 0);

   if (readval == 0)   
   {
      /* The underlying TCP connection has been closed */
      tel_delete(t);
      return TEL_PROC_TCP_CLOSED;
   }
   else if (readval < 0) /* error on socket? */
   {
      e = sys_errno(t->sock);
      if (e == SYS_EWOULDBLOCK)
      {
         readval=0;
      }
      else
      {
         /*
          * Altera Niche Stack Nios port modification:
          * Change %p to %d to fix build warning
          */
         dprintf("telnet: error %d on socket %d\n", e, (unsigned int)t->sock);
         tel_delete(t);    /* thats the end of this connection... */
         return TEL_PROC_FATAL_ERROR; /* error return */
      }
   }
   else
   {
      /* Some data was read. Update the timer */
      tel_start_timer(&(t->curr_tick));
   }

   t->rxsize=readval;
   t->stats.rcvd+=readval;


   return SUCCESS;
}


/* FUNCTION: tel_sp_char_proc()
 * 
 * Process special characters like BackSpace, ESC(erase line).
 * This function modifies both the input buffer and the
 * output buffer. The erased chars, lines are removed from
 * input buffer and output is generated to be sent
 * to the telnet peer.
 *
 * PARAM1: char *buf - Pointer to buffer having unprocessed string (IN/OUT)
 * PARAM2: int *in_size - Pointer to size of input buffer (IN/OUT)
 * PARAM3: char *obuf - Pointer to output buffer (OUT)
 * PARAM4: int *obuf_size - Pointer to size of output buffer (OUT)
 * PARAM5: int old_size - Previous size of the bufer (IN)
 *
 * RETURNS: SUCCESS or error number.
 */

int
tel_sp_char_proc(char * buf, int * in_size, char * obuf, int * obuf_size, 
   int   old_size)
{
   int   i=old_size;
   int   out_size=0;
   int   remaining=*in_size-old_size;
   int   prev_crlf;
   int   chars_in_cmd;

   if (remaining <= 0)
      return TEL_SP_BAD_SIZE;

   /* Telnet RFC says that EC (EraseCharacter), EL (EraceLine) 
    * commands are always preceded by an IAC. But we don't receive it 
    * that way via telnet client. Maybe it is true when two processes 
    * communicate via telnet ! Hence provide the hook here also.
    */
   while (remaining > 0) 
   {
      if (buf[i] == TEL_BACKSP)
      {
         /* update the input/command buffer */

         remaining -= 2;

         if (*in_size > 1)     /* if there is a char to erase */
         {
            (*in_size) -= 2;   /* reduce size of input buffer */

            i--;

            /* Update the output buffer */
            obuf[out_size++] = TEL_BACKSP;
            obuf[out_size++] = TEL_SPACE;
            obuf[out_size++] = TEL_BACKSP;
         }
         else 
         {
            (*in_size) = '\0';    /* reduce size of input buffer */
         }

         /* If we are at end of string. no need for MEMMOVE*/
         if (remaining > 0) 
         {
            MEMMOVE(buf+i, buf+i+2, remaining);
         }
      }
      else if (buf[i] == TEL_ESC)
      {
         /* find the start of previous command */
         prev_crlf = i-1;
         while (prev_crlf >= 0)
         {
            if ((buf[prev_crlf]  ==TEL_CR) && 
                ((buf[prev_crlf+1]==TEL_LF) || (buf[prev_crlf+1]==TEL_CRNUL)))
            {
               break;
            }
            else
            {
               prev_crlf--;
            }
         }
         if (prev_crlf < 0)    /* No CRLF found */
         {
            prev_crlf = 0;
            chars_in_cmd = i+1;
         }
         else
         {
            prev_crlf+=2;     /* Move past CRLF */
            chars_in_cmd = i+1-prev_crlf;
         }

         /* chars_in_cmd contains the number of chars including ESC
            key which are to be erased */
         i=prev_crlf;
         remaining--;
         (*in_size)-=chars_in_cmd;    /* reduce size of input buffer */

         if (remaining > 0)
         {
            MEMMOVE(buf+prev_crlf,buf+prev_crlf+chars_in_cmd,remaining);
         }

         /* Now update the output buffer */
         while (--chars_in_cmd > 0)
         {
            obuf[out_size++] = TEL_BACKSP;
            obuf[out_size++] = TEL_SPACE;
            obuf[out_size++] = TEL_BACKSP;
         }
      }
      else if (buf[i] == TEL_CR &&  buf[i+1] == TEL_CRNUL)
      {
         /* For UNIX systems, we receive CR,CRNULL when the <Enter> 
          * key is pressed. But to make the cursor to goto start of 
          * next line, UNIX systems (like everyone else) need CR,LF. 
          * Hence make the substitution in output over here.
          */
         obuf[out_size++] = buf[i];
         obuf[out_size++] = TEL_LF;
         i+=2;
         remaining-=2;
      }
      else
      {
         obuf[out_size++] = buf[i];
         remaining--;
         i++;
      }
   }

   *obuf_size = out_size;
   return SUCCESS;
}


/* FUNCTION: tel_proc_cmd()
 * 
 * Process the data that we have received till now.
 * If there any commands in it, call tel_exec_cmd() to execute them.
 *
 * PARAM1: TEL_CONN tconn - Pointer to telnet connection. (IN)
 *
 * REMARKS: The members cmdbuf, cmdsize of tconn are modified in this
 * function.
 *
 * RETURNS: SUCCESS or error number 
 */

int
tel_proc_cmd(TEL_CONN tconn)
{
   int   i;
   int   nextpos=0;
   static char cmd[TEL_RECV_BUF_SIZE];

   for (i=0; i < tconn->cmdsize; i++)
   {
      /* Treat CR or CR+LF or CR+CRNUL as command terminators     
       * When RETURN/ENTER key is pressed ,
       *    BSD2.6 client returns CR,CRNUL, 
       *    Win9x/Win2000 returns CR,LF 
       *    Windows XP returns CR 
       */ 
      if ((tconn->cmdbuf[i] == TEL_CR))
      {
         /* Yes we got one command. Execute it */
         if (i==nextpos)    /* User has just pressed <CRLF> */
         {
#ifdef TEL_USERAUTH
            if (tconn->state == TEL_PASSWORD)
            {
               /* User has entered the password */
               if (TEL_CHECK_PERMIT(tconn->name,"") == TRUE)
               {
                  /* Valid user. So goto NORMAL state */
                  tconn->state =TEL_NORMAL;
                  tconn->prompt=prompt;
                  ns_printf(&(tconn->tel_io),"\nWelcome %s\n",tconn->name);
               }
               else
               {
                  /* Invalid password. Try again */
                  if (tconn-> tries >= tglob.tel_max_login_tries)
                  {
                     /* We have given enough attempts. Terminate connection */
                     tel_delete(tconn);
                     return TEL_PROC_AUTH_FAILED;
                  }
                  tconn->state=TEL_LOGIN;
                  tconn->prompt=tel_login_prompt;
               }
            }
#endif
         }
         else
         {
            tconn->stats.cmds++;

            MEMCPY(cmd,tconn->cmdbuf+nextpos,i-nextpos);
            cmd[i]=0;   /* Null terminate */

            if (tconn->state == TEL_NORMAL)
            {
               /* Handle the special case of logging out */
               if (stricmp(cmd,"logout") == 0 ||
                   stricmp(cmd,"exit") == 0)
               {
                  tel_delete(tconn);
                  return TEL_PROC_LOGGED_OUT;
               }
               tconn->tel_io.inbuf=cmd;
               tconn->tel_io.getch=tel_tcp_recv;
               if (tel_exec_cmd(&(tconn->tel_io)) == 0)
               {
                  /* Command was properly executed. Return to
                   * normal state. 
                   * Consider the following calling sequence.
                   * tel_exec_cmd()->do_command()->dumpbuf()->
                   *   con_page()->tel_tcp_recv()
                   * If the TELNET client closed connection
                   * then tel_tcp_recv() would cleanup the telnet connection.
                   * But do_command() & hence tel_exec_cmd() would still return 0.  
                   * In that case, tel_exec_cmd() can't detect  it. 
                   * Hence check if tconn is valid.
                   */
                  TEL_CONN tmp=telq;
                  while (tmp && tmp != tconn)  /* search all connections */
                     tmp=tmp->next;
                  if (!tmp)  /* couldn't find tconn */
                     return TEL_PROC_LOGGED_OUT;
            
                  if (tconn->state == TEL_PROC_CMD)
                     tconn->state = TEL_NORMAL;
               }
            }
#ifdef TEL_USERAUTH
            else if (tconn->state == TEL_LOGIN)
            {
               /* User has entered the login name */
               strncpy(tconn->name,cmd,MAX_USERLENGTH);
               tconn->prompt=tel_passwd_prompt;
               tconn->state =TEL_PASSWORD;
               tconn->tries++;
            }
            else if (tconn->state == TEL_PASSWORD)
            {
               /* User has entered the password */
               if (TEL_CHECK_PERMIT(tconn->name,cmd) == TRUE)
               {
                  /* Valid user. So goto NORMAL state */
                  tconn->state =TEL_NORMAL;
                  tconn->prompt=prompt;
                  ns_printf(&(tconn->tel_io),"\nWelcome %s\n",tconn->name);
               }
               else
               {
                  /* Invalid password. Try again */
                  if (tconn-> tries >= tglob.tel_max_login_tries)
                  {
                     /* We have given enough attempts. Terminate connection */
                     tel_delete(tconn);
                     return TEL_PROC_AUTH_FAILED;
                  }
                  tconn->state=TEL_LOGIN;
                  tconn->prompt=tel_login_prompt;
               }
            }
#endif   /* TEL_USERAUTH */
         }

         /* Show the prompt */
         ns_printf(&tconn->tel_io,tconn->prompt);
         nextpos=i+1;   /* Position of start of new command */
         if (nextpos < tconn->cmdsize)
         {
            /* Discard the extra linefeed related char sent by BSD/Win9x */
            if ((tconn->cmdbuf[i+1]==TEL_LF) || (tconn->cmdbuf[i+1]==TEL_CRNUL))
               nextpos++;
         }

         /* Send the GA command if it is not suppressed */
         if (tconn->options->sga.l_value == FALSE)
         {
            tel_send_cmd(tconn->sock,TEL_GOAHEAD,0);
         }
      }              

   }

   /* The commands has been processed. So remove them from cmdbuf*/
   if (nextpos == 0)  /* No cmds were found */
   {
      /* Do nothing */
   }
   else if (nextpos == tconn->cmdsize)
   {
      /* Whole string in cmdbuf has been processed */
      tconn->cmdsize=0;
   }
   else if (nextpos < tconn->cmdsize)
   {
      /* We need to remove the processed string from cmdbuf,
         so that cmdbuf contains only the unprocessed input */
      tconn->cmdsize-=nextpos;
      MEMMOVE(tconn->cmdbuf,tconn->cmdbuf+nextpos,tconn->cmdsize);
   }
   else     /* nextpos > tconn->rxsize */
   {
      /* What? We processed ahead of the buffer !! */
      dtrap();
   }

   return SUCCESS;
}


#endif   /* #ifdef TELNET_SVR */

