/* tftp.h

   Contains structures of TFTP packet headers and such. 

   Copyright 1998 by InterNiche Technologies Inc. All rights reserved.
   Copyright 1992 by NetPort Software.
   Copyright 1986 by Carnegie Mellon
   Copyright 1984 by the Massachusetts Institute of Technology
*/

#ifndef _TFTP_H_
#define _TFTP_H_ 1


#define  NORMLEN     512   /* normal length of received packet */

/* read or write request packet */
struct tfreq {
   unshort    tf_op;       /* 1 (read) or 2 (write) */
   /* this is followed by two variable-length zero-terminated
    * octet strings: one for the filename, one for the mode.
    */
};

/* data packet */
struct tfdata {
   unshort   tf_op;        /* always 3 */
   unshort   tf_block;
   char      tf_data[NORMLEN];
};

/* structure of an ack packet */
struct   tfack {
   unshort   tf_op;        /* would be 4 */
   unshort   tf_block;
};

/* error packet */
struct tferr {
   unshort   tf_op;        /* would be 5 */
   unshort    tf_code;
   char      tf_err[1]; };

/* TFTP added definitions */
#ifndef PUT
#define  GET   1
#define  PUT   2
#endif

#define  ASCII       1
#define  OCTET       2
#define  TEST        3

/* TFTP opcodes */
#define  TF_RRQ      1     /* read  request */
#define  TF_WRQ      2     /* write request */
#define  TF_DATA     3     /* data packet */
#define  TF_ACK      4     /* acknowledgement packet */
#define  TF_ERROR    5     /* error packet */

/* TFTP error codes */
#define  ERRTXT      0     /* see the enclosed text */
#define  FNOTFOUND   1     /* file not found */
#define  ACCESS      2     /* access violation */
#define  DISKFULL    3     /* don't even ask. */
#define  ILLTFTP     4     /* illegal TFTP operation */
#define  BADTID      5     /* unkown transfer ID */
#define  FEXISTS     6     /* file already exists */
#define  NOUSER      7     /* no such user */

extern char * tferrors[8]; /* text for the above errors */

/* TFTP states */
#define  DATAWAIT    1     /* GET: sent req; waiting for data */
#define  ACKWAIT     2     /* PUT: sent data; waiting for ack */
#define  DEAD        3     /* a killed connection, waiting for cleanup */
#define  TIMEOUT     4     /* connection timed out */
#define  RCVERR      5     /* connection got error from other side */
#define  RCVACK      6     /* PUT: got ack, sending data */
#define  RCVDATA     7     /* GET: sent ack; waiting for next block */
#define  RCVLASTDATA 8     /* GET: received last block of file (A block > 512 bytes) */
#define  SENTLAST    9     /* PUT: sent last packet, awaiting ack */
#define  TERMINATED  10    /* PUT: sent last packet & and got ack for it */

#define  TFTPPORT    69    /* TFTP's well known port */

struct tftpbuf {
   char * data;      /* tftpudp managed buffer, to support zero-copy */
   unsigned dlen;    /* length of valid data in buffer, 0 if none */
   void * udp_use;   /* packet handle for UDP api to use */
};

typedef struct tftpbuf * TFTPBUF;

/* The TFTP connection structure. Contains connection info, and data for
   timeout calculations. */

struct tfconn {
   struct tfconn * next;      /* queue link */
   void*       tf_conn;       /* NetPort UDPCONN handle or socket */
   VFILE *     tf_fd;         /* file descriptor for xfer */
   struct tftpbuf   tf_inbuf; /* Buffer for file data rcv */
   struct tftpbuf   tf_outbuf;/* Buffer for file data xmit */
   unsigned    tf_lastlen;    /* length of last sent pkt */
   unsigned    tf_flen;       /* bytes to send in next data send */
   unshort     tf_expected;   /* most recent full processed block ( ack & all) */
   ip_addr     tf_fhost;      /* foreign host */
   unshort     tf_fport;      /* foreign port */
   unshort     tf_lport;      /* local port */
   unsigned    tf_state;      /* state of connection */
   int         tf_tries;      /* # of retries already done */
   unsigned    tf_mode;       /* mode = OCTET, [net]ASCII, ... */
   unsigned    tf_dir;        /* direction of the transfer */
   long        tf_size;       /* # of bytes transferred */
   unsigned    tf_rcv;        /* # of packets received */
   unsigned    tf_snt;        /* # of packets sent */
   unsigned    tf_ous;        /* # of out of sequence packets */
   unsigned    tf_tmo;        /* # of timeouts */
   unsigned    tf_rsnd;       /* # of resends */
   u_long      tf_start;      /* start time of transfer */
   u_long      tf_srtt;       /* average measured round trip time */
   u_long      tf_rt;         /* current timeout */
   u_long      tf_sent;       /* time that pkt was sent */
   u_long      tf_tick;       /* ctick for next event timeout */
   int         tf_NR;         /* number rexmissions of this pkt */
   int         tf_NR_last;    /* "   "   " of prev pkt */
   /* callback at end of transfer */
   int (* callback)(int status, struct tfconn *, char * msg);
};

extern struct tfconn * tftp_conns;   /* list of active connections */

#define TFC_SUCCESS  0
#define TFC_ZEROFILE -1    /* tried to send 0 length file */
#define TFC_DISKFULL -2    /* local disk full (or simlar fault) */
#define TFC_FOREIGN  -3    /* error from foreign host */
#define TFC_FILEREAD -4    /* file read error */
#define TFC_FILEOPEN -5    /* file open error */
#define TFC_BUFFER   -6    /* buffer alloc error */
#define TFC_TIMEOUT  -7    /* timed out waiting for peer reply */
#define TFC_UDPSEND  -8    /* error returned by tftp_udpsend() */
#define TFC_BADLEN   -9    /* bad data length received from peer */
#define TFC_BADSTATE -10   /* TFTP state machine in unrecognized state */

#define   dos_eof   26   /* control Z */

/* TFTP API entry point to initiate client transfers */
char *    tftpuse(ip_addr fhost, char *fname, char *rmfile, 
      unsigned dir, unsigned mode,
      int (*callback)(int, struct tfconn*, char*) );   /* callback */

struct tfconn * tfmkcn(void);   /* alloc and enqueue tfconn struct */

void   tftprcv(struct tfconn *cn, unshort port);   /* tftp client's packet receive routine */
void   tftptmo (struct tfconn * cn);   /*   timeout handler   */
void   tfcleanup(struct tfconn * cn);   /* disconnect, free & dequeue tfconn & members */
int    tfsndreq(struct tfconn * cn, char * fname);
void   tfsnderr(struct tfconn * cn, unsigned code, char *text);
int    tfsndata(struct tfconn * cn);
void   tf_good (struct tfconn * cn);   /* fixed tmo info (srtt,etc) when OK packet is received */
int    tf_write(struct tfconn * cn, unsigned len);   /* write next file block to net */
void   tfkill  (struct tfconn * cn);
int    tfsndack(struct tfconn * cn);
void   tf_full (struct tfconn * cn);
void   tfdoack (struct tfconn * cn);

extern char *tftplog;

/* server API */
int   tfsinit(int(*alert)(ip_addr,char*,unshort), 
         int (*done)(int,struct tfconn *,char*));
void  tfs_on(void);     /* toggle tftp server ON */
void  tfs_off(void);    /* toggle tftp server OFF */
int   tfshnd(ip_addr host, unshort fport, char * udata);


/* API to UDP layer - this can map to sockets or other API */
int   tftp_init(void);
void* tftp_udplisten(ip_addr fhost, unshort fport, unshort * lport, void *);
void  tftp_udpbuffer(struct tfconn * cn, int size);
void  tftp_udpfree(struct tfconn * cn);
int   tftp_udpsend(struct tfconn *, void * outbuf, int outlen);
int   tftp_udpclose(void * conn);   /* close socket or UDPCONN */

#endif /*  _TFTP_H_ */

