/*
 * FILENAME: cu_srv.c
 *
 * Copyright  2004 By InterNiche Technologies Inc. All rights reserved
 *
 * Test program for TCP sockets API. Implements Crypto Engine server
 *
 * MODULE: MISCLIB
 *
 * ROUTINES: tcp_scipher_init(), tcp_scipher_close(), tcp_cipher_cleanup(), 
 * ROUTINES: tcp_cipher_init(), tcp_cipher_recv(), tcp_cipher_poll(), 
 * ROUTINES: tcp_cipher_stats()
 *
 * PORTABLE: yes
 */

#include <string.h> 
#include "ipport.h"     /* NetPort embedded system includes */

#ifdef TCP_CIPHERTEST     /* whole file can be ifdeffed out */
#include "ce.h"
#include "cu_srv.h"

#ifndef WINSOCK
#include "tcpport.h"    /* NetPort embedded system includes */
#include "in_utils.h"
#ifndef sock_noblock
#define  sock_noblock(sock,flag) setsockopt(sock,SOL_SOCKET,SO_NBIO,NULL,0)
#endif
#endif


/* This global switch can be set prior to calling tcp_cipher_init() */
int      tcpcipher_server =  TRUE;

static int in_cipherpoll = 0;   /* re-entry flag */

static SOCKTYPE elisten_sock =         INVALID_SOCKET; /* Crypto Engine server socket -
                                                   listening socket descriptor */
static SOCKTYPE esvr_sock    =         INVALID_SOCKET; /* Crypto Engine server active socket - 
                                                   connected socket descriptor */

/* Do we need to close the server's active socket? */
static int      esvr_sock_close = FALSE;

#ifdef NPDEBUG
static void printBufferInHex(void *pio, unsigned char *buf, int len)
{
	ns_printf(pio, "tcp_cipher::printBufferInHex Input buffer = ");
	if (buf == NULL) {
		ns_printf(pio, "NULL \n");
		return;
	}
	while (len)
	{
	        printf("%02x", *buf);
		
		if (--len>0)
			buf++;
	}
	printf("\n");
	return;
}

static void printBuffer(void *pio, unsigned char *buf, int len)
{
	ns_printf(pio, "tcp_cipher::printBuffer output buffer = ");
	if (buf == NULL) {
		ns_printf(pio, "NULL \n");
		return;
	}
	while (len)
	{
	        ns_printf(pio, "%c", *buf);
		
		if (--len>0)
			buf++;
	}
	ns_printf(pio, "\n");
	return;
}
#endif

/*static void copyStr(unsigned char *key, unsigned char c, int len)
{
	int i;
	for (i=0; i<len; i++)
	{
		*key=c;
		key++;
	}
}*/

#ifdef NPDEBUG
static void binary_op(void *pio, char byte)
{
   int count=8;				/* Number of bits in a byte.	*/

   while(count--)
      {
					/* AND the high order bit (the
					 * left one) If the bit is set,
					 * print a ONE.			*/
      ns_printf(pio, "%d", ( byte & 128 ) ? 1 : 0 );

					/* Move all the bits LEFT.	*/
      byte <<= 1;
      } 

   ns_printf(pio, "\n");
}
#endif

static void cb(void *pio, void *conn, unsigned char *buf, int len, int status, int retCode)
{
   USE_ARG(retCode);
	USE_VOID(conn);
	USE_VOID(pio);
	done=1;

	if (status == CE_DONE) {
		cipherOutBuf=buf;		
		cipherOutBufLen=len;
	}

	return;
}

void tcpCipher_getCipherId(ServerInBuffer *srvInBufPtr)
{
	short cipherId;
	int   e;          /* error holder */
	cipherId=getCipherId(srvInBufPtr->encryptionAsymetricAlg,
                        srvInBufPtr->encryptionSymetricAlg,
				            srvInBufPtr->authenticationAlg);
	srvOutBuf.cipherId=htons(cipherId);
	e = send(esvr_sock, (char *)&srvOutBuf, sizeof(srvOutBuf), 0);
	if (e < 0)
	{
		 /* Print the error to console */
		e = t_errno(esvr_sock);
		ns_printf(NULL,
                 "TCP Crypto Engine server::tcpCipher_getCipherId, error %d sending reply\n", e);
		dtrap();
	}
	return;
}

void tcpCipher_freeCipher(ServerInBuffer *srvInBufPtr)
{	
	freeCipher(srvInBufPtr->cipherId);
	return;
}

void processInputBuffer(ServerInBuffer *srvInBufPtr)
{
	unsigned char flags;
	int ret_code;
	unsigned short cipherId;
	unsigned short bufLen;
	unsigned char *buffer=NULL;
   unsigned char priv_key_len=16;
   unsigned char iv_len=16;
	
   unsigned char priv_key[16]=
		{0x6c,0x3e,0xa0,0x47,0x76,0x30,0xce,0x21,
       0xa2,0xce,0x33,0x4a,0xa7,0x46,0xc2,0xcd};
       
   unsigned char iv[16]=
		{0xc7,0x82,0xdc,0x4c,0x09,0x8c,0x66,0xcb,
       0xd9,0xcd,0x27,0xd8,0x25,0x68,0x2c,0x81};       

   struct securityParameters secParm;
   struct inputParameters inpParm;           
   
   secParm.auth_key=NULL;
   secParm.auth_key_len=0;
   secParm.priv_key=priv_key;
   secParm.priv_key_len=priv_key_len;
   secParm.iv=iv;
   secParm.iv_len=iv_len;
   secParm.cipher_len=NULL;   
		
	if (srvInBufPtr->encryptionSymetricAlg>0)
	{
		ns_printf(NULL, "tcp_cipher::processInputBuffer encryptionSymetricAlg = %d \n",
			             srvInBufPtr->encryptionSymetricAlg);
		tcpCipher_getCipherId(srvInBufPtr);
		return;
	}

   if (srvInBufPtr->action==DELETE_CIPHER)
	{
		ns_printf(NULL, "tcp_cipher::processInputBuffer encryptionSymetricAlg = %d \n",
			             srvInBufPtr->encryptionSymetricAlg);
		tcpCipher_freeCipher(srvInBufPtr);
		return;
	}

	bufLen = ntohs(srvInBufPtr->bufLen);

    flags=srvInBufPtr->flags;

   #ifdef NPDEBUG
	   binary_op(NULL, flags);
   #endif

    cipherId=ntohs(srvInBufPtr->cipherId);
	
   #ifdef NPDEBUG
	   ns_printf(NULL, "cipher id = %x \n", cipherId);
	   ns_printf(NULL, "flags = %x \n", flags);
	   printBufferInHex(NULL, srvInBufPtr->buffer, bufLen);
	   /*ns_printf(NULL, "buffer = %s \n", srvInBufPtr->buffer);*/
	   ns_printf(NULL, "buffer length = %d \n", bufLen);
   #endif

	if (srvInBufPtr->bufLen>0)
	{
	    buffer = (unsigned char *)npalloc(bufLen);
		MEMCPY(buffer, srvInBufPtr->buffer, bufLen);
	}

   #ifdef NPDEBUG
	   ns_printf(NULL, "tcp_cipher::processInputBuffer buffer = ");
	   printBufferInHex(NULL, srvInBufPtr->buffer, bufLen);
   #endif

	/*auth_key=(unsigned char *)npalloc(key_len);
			
	copyStr(auth_key, 0xaa, auth_key_len);*/
   	
	inpParm.cid=cipherId;
   inpParm.flags=flags;
   inpParm.pConn=NULL;
   inpParm.inBuf=buffer;
   inpParm.inBufLen=bufLen;
   inpParm.outBuf=NULL;
   inpParm.cb=cb;

   ret_code=ce_crypt(NULL, 
             &inpParm,
             &secParm);
   if (ret_code == SUCCESS)
	    ce_do();
	
   #ifdef NPDEBUG
	   ns_printf(NULL, "tcp_cipher::processInputBuffer done = %d \n", done);
	   ns_printf(NULL, "tcp_cipher::processInputBuffer output buffer = \n");
	   printBuffer(NULL, cipherOutBuf, cipherOutBufLen);		 		
   #endif
	
	npfree(buffer);
	buffer=NULL;	
}

void
tcp_cipher_recv(void)
{
   int   len;        /* length of recv data */
   int   e;          /* error holder */
   unsigned i;       /* generic index */
   int   count;      /* select return */
   fd_set fd_recv;   /* fd for recv */
   fd_set fd_accept; /* fd for accept */
   struct sockaddr_in client;
   SOCKTYPE tmpsock; /* scratch socket */  
   ServerInBuffer srvInBuf;

   if (elisten_sock == INVALID_SOCKET)
      return;  /* Crypto Engine Server not set up, don't bother */

#ifdef USE_FDS
   /* initialize FD arrays */
   FD_ZERO(&fd_recv);
   FD_ZERO(&fd_accept);
#endif

   i = 0;
   count = 0;
   
   if (tcpcipher_server)
   {
      if (esvr_sock != INVALID_SOCKET)
      {
#ifdef USE_FDS
         FD_SET(esvr_sock, &fd_recv); 
         ++i;
#else
         fd_recv.fd_array[i++] = esvr_sock;
#endif
         ns_printf(NULL, "added esvr_sock to fd_recv \n");
      }
   }

#ifndef USE_FDS
   fd_recv.fd_count = i;
#endif

   /* make this a short timeout since elisten may be created soon */
   if (elisten_sock != INVALID_SOCKET)
   {
#ifdef USE_FDS
      FD_SET(elisten_sock, &fd_accept);
#else
      fd_accept.fd_array[0] = elisten_sock;
      fd_accept.fd_count = 1;	  

#endif
      count = t_select(&fd_recv, (fd_set *)NULL, &fd_accept, 1);	  
   }
   else 
   {
      if (i)   /* if no fd_set sockets filled in, don't bother */
         count = t_select(&fd_recv, (fd_set *)NULL, (fd_set *)NULL, 1);
   }

   /* While the t_select() was executing, commands can be 
    * executed from cmd-prompt and sockets can be cleaned up. 
    * Check for that. 
    */
   if (elisten_sock == INVALID_SOCKET)
      return;  /* Crypto Engine Server not set up, don't bother */

   for (i = 0; i < fd_recv.fd_count; i++)
   {
#ifdef USE_FDS
      tmpsock = FD_GET(i, &fd_recv);
      if (tmpsock == INVALID_SOCKET)
         continue;
#else
      tmpsock = fd_recv.fd_array[i];
#endif
      MEMSET((char *)&srvInBuf, 0, sizeof(srvInBuf));
      len = recv(tmpsock, (char *)&srvInBuf, sizeof(srvInBuf), 0);

      if (len > 0)
      {
         processInputBuffer(&srvInBuf);
      }

      if (len < 0)
      {
         e = t_errno(tmpsock);
         if (e != EWOULDBLOCK)
         {
         ns_printf(NULL,"TCP Crypto Engine Server recv error %d\n", e);
         }
      }
      else if (len == 0)
      {
         ns_printf(NULL,"TCPCipher:socket closed by other side\n");
         socketclose(esvr_sock);
         esvr_sock = INVALID_SOCKET;
      }
   }

   if (done)
   {
#ifdef NPDEBUG
      printBuffer(NULL, cipherOutBuf, cipherOutBufLen);
#endif
      e = send(esvr_sock, (char *)cipherOutBuf, cipherOutBufLen, 0);
      if (e < 0)
      {
/* Print the error to console */
         e = t_errno(esvr_sock);
         ns_printf(NULL,
               "TCP Crypto Engine Server server, error %d sending reply\n", e);
      }
      npfree(cipherOutBuf);
      cipherOutBuf=NULL;
      done=0;
   }

   /* if no server listen to poll, return now */
   if (elisten_sock == INVALID_SOCKET)
      return;

#ifdef NOTDEF
   MEMSET(&client, 0, sizeof(client));
   client.sin_family = AF_INET;
   client.sin_addr.s_addr = INADDR_ANY;
   client.sin_port = htons(CIPHER_PORT);
#endif

   /* check for received client connection on server */
   len = sizeof(client);
   tmpsock = accept(elisten_sock, (struct sockaddr*)&client, &len );
   if (tmpsock != INVALID_SOCKET)
   {
      if (esvr_sock == INVALID_SOCKET)
      {
         esvr_sock = tmpsock;
      }
      else  /* we already have a connection */
      {
         dprintf("tcpCipher: rejected extra connection\n");
         socketclose(tmpsock);   /* refuse to serve another */
      }
   }
}



/* FUNCTION: tcp_cipher_poll()
 * 
 * Do routine processing for  Crypto Engine Server
 * 1. Check if client packets have been received for TCP Crypto Engine 
 *    Server. If yes, send a reply.
 * 2. Check if Crypto Engine replys have been recieved for TCP Crypto Engine Clients.
 *    If yes, update data structures, display messages.
 *
 * PARAM1: void
 *
 * RETURNS: 
 */

void
tcp_cipher_poll(void)
{
   if (elisten_sock == INVALID_SOCKET)
      return;  /* Crypto Engine Server not set up, don't bother */

   in_cipherpoll++;    /* don't re-enter from tk_yield() */
   if (in_cipherpoll != 1)
   {
      in_cipherpoll--;
      return;
   }

   tcp_cipher_recv();

   in_cipherpoll--;
}

/* FUNCTION: tcp_scipher_init()
 * 
 * Initialize the TCP Crypto Engine Server. If the flag tcpcipher_server is false,
 * then Server Init is not done.
 * 
 * PARAM1: void * pio - device for console output
 *
 * RETURNS: 0 on SUCCESS or error number. 
 */

int 
tcp_scipher_init(void * pio)
{
   int   e;    /* error holder */
   struct sockaddr_in   me;   /* my IP info, for bind() */
   int   opt;

   if (tcpcipher_server)
   {
      ns_printf(pio,"tcp Crypto Engine srv - starting.\n");

      /* open TCP socket */
      elisten_sock = socket(AF_INET, SOCK_STREAM, 0);
      if (elisten_sock == INVALID_SOCKET)
      {
         ns_printf(pio,"TCP Crypto Engine: bad socket: %d\n", elisten_sock);
         return TCPE_BAD_SOCKET ;
      }

      opt = 1;
      e = t_setsockopt(elisten_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
      if (e != 0)
      {
         e = t_errno(elisten_sock);
         dtrap();
         dprintf("error %d setting SO_REUSEADDR on port %d\n", e, CIPHER_PORT);
         return SYS_SOCKETNULL;
      }

      me.sin_family = AF_INET;
      me.sin_addr.s_addr = INADDR_ANY;
      me.sin_port = htons(CIPHER_PORT);

      e = bind(elisten_sock, (struct sockaddr*)&me, sizeof(me));
      if (e != 0)
      {
         e = t_errno(elisten_sock);
         ns_printf(pio,"tcp_Cipher: bad socket bind: %d, %s\n", e, so_perror(e) );
         socketclose(elisten_sock);
         elisten_sock = INVALID_SOCKET;
         return TCPE_BIND_FAILED ;
      }
      e = listen(elisten_sock, 3);
      if (e != 0)
      {
         e = t_errno(elisten_sock);
         ns_printf(pio,"tcp_Cipher: bad socket listen: %d %s\n", e, so_perror(e));
         socketclose(elisten_sock);
         elisten_sock = INVALID_SOCKET;
         return TCPE_LISTEN_FAILED;
      }
      /* for listen socket into Non-blocking mode so we can poll accept */
      sock_noblock(elisten_sock, TRUE);
   }
   else
      ns_printf(pio,"tcp Crypto Engine server not enabled\n");

   return SUCCESS;
}

/* FUNCTION: tcp_cipher_init()
 * 
 * Initialize TCP Crypto Engine Server
 * Remarks : This function has been defined so that a clean interface 
 * is provided for TCP Crypto Engine Server.
 *
 * PARAM1: void * pio - device for console output
 *
 * RETURNS: 0 on SUCCESS or error number. 
 */

int
tcp_cipher_init(void)
{
   return tcp_scipher_init(NULL);
}

/* FUNCTION: tcp_scipher_close()
 * 
 * Close the TCP Crypto Engine Server.
 *
 * PARAM1: void * pio - device for console output
 *
 * RETURNS: 0 on SUCCESS or error number. 
 */

int
tcp_scipher_close(void * pio)
{
   int   e = 0;               /* scratch error holder */
   int   retval = 0;          /* return last non-zero error */

   if (esvr_sock != INVALID_SOCKET)
   {
      e = socketclose(esvr_sock);
      if (e)
      {
         retval = e = t_errno(esvr_sock);
         ns_printf(pio,"tcp Crypto Engine server: close error %d %s\n",
                       e, so_perror(e));
      }
      else
      {
         ns_printf(pio,"tcp Crypto Engine srv - closing.\n");
         esvr_sock = INVALID_SOCKET;
      }
   }

   if (elisten_sock == INVALID_SOCKET)
      return e;

   e = socketclose(elisten_sock);
   if (e)
   {
      retval = e = t_errno(elisten_sock);
      ns_printf(pio,"tcp Crypto Engine: server close error %d %s\n",
                    e, so_perror(e) );
   }
   elisten_sock = INVALID_SOCKET;

   return retval;
}



/* FUNCTION: tcp_cipher_cleanup()
 *
 * Cleanup all TCP Crypto Engine related data structures.
 *
 * PARAM1: void
 *
 * RETURNS: 
 */

void
tcp_cipher_cleanup(void)
{
   tcp_scipher_close(NULL);
}

/* FUNCTION: tcp_cipher_stats()
 * 
 * Show statistics about all TCP Crypto Engine Clients and Servers
 *
 * PARAM1: void * pio - device for console output
 *
 * RETURNS: 0 on SUCCESS or error number. 
 */

int
tcp_cipher_stats(void * pio)
{
   ns_printf(
	   pio,"Showing TCP Crypto Engine statistics.\n");
   if ( esvr_sock == INVALID_SOCKET )
   {
      ns_printf(pio,"   There are no Server connections.\n");
   }
   else
   {
      ns_printf(pio,"   There is one Server connection.\n");
   }

   return SUCCESS;
}

#endif   /* TCP_CIPHERTEST */
