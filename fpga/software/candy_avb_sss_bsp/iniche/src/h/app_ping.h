/*
 * FILENAME: app_ping.h
 *
 * Copyright  2000 By InterNiche Technologies Inc. All rights reserved
 *
 * Header file for  app_ping.c
 * NOTE - this file may by included even if PING_APP is not defined in the
 * system build, since UDP_ECHO and others use some of these values.
 *
 * MODULE: TCP
 *
 * PORTABLE: yes
 */

#ifndef APP_PING_H
#define  APP_PING_H     1

#include "in_utils.h"

struct PingInfo 
{
   u_long   delay;         /* delay between successive pings */
   u_long   length;        /* ping packet length */
   ip_addr  ipadd;   
   u_long   nextping;      /* cticks value to do next ping */
   long     times;         /* number of times to ping */
   int      out;
   int      in;
   GEN_IO   pio;           /* To communicate with invocator of ping */
   struct PingInfo * next; /* Next ping packet */
};

typedef struct PingInfo * PING_INFO ;

extern   ip_addr  activehost; /* default ping host */

#define  TIMEFOR1TICK      (1000/TPS)  /* Time between each tick in millisecs */


int         ping_init(void);
PING_INFO   ping_new (void);
int         ping_delete(PING_INFO p);
int         ping_addq(PING_INFO p);
int         ping_delq(PING_INFO p);
PING_INFO   ping_search(GEN_IO pio);
int         ping_start(void * pio);
int         ping_send(PING_INFO p);
int         ping_end(void * pio);
int         ping_setdelay(void * pio);
int         ping_setlength(void * pio);   /* menu routine to set default ping size*/
int         ping_sethost(void * pio);     /* set default host for pings, et.al. */
int         pingUpcall(PACKET p);
void        ping_check(void);
PING_INFO   ping_demux(ip_addr fhost);
int         ping_stats(void * pio);

#define     PING_ALLOC(size)  npalloc(size) 
#define     PING_FREE(ptr)    npfree(ptr)

/* Note that some of the error codes listed below have been
 * reused by other functions in this module.
 */
#define     PINGERRBASE       200

/* List of error codes returned by ping_delq() */
#define     PING_DELQ_BAD_OBJECT       (PINGERRBASE+11)
#define     PING_DELQ_Q_EMPTY          (PINGERRBASE+12)
#define     PING_DELQ_OBJ_NOT_FOUND    (PINGERRBASE+13)

/* List of error codes returned by ping_delete() */
#define     PING_DEL_NULL_ARGUMENT     (PINGERRBASE+21)

/* List of error codes returned by ping_start() */
#define     PING_ST_NOIP               (PINGERRBASE+31)
#define     PING_ST_BAD_ARG2           (PINGERRBASE+32)
#define     PING_ST_ALLOC_ERR          (PINGERRBASE+33)

/* List of error codes returned by ping_end() */
#define     PING_END_NO_SESSION        (PINGERRBASE+41)

/* List of error codes returned by ping_setdelay() */
#define     PING_DELAY_BAD_ARG         (PINGERRBASE+51)

/* List of error codes returned by ping_setlength() */
#define     PING_LEN_BAD_ARG           (PINGERRBASE+61)

/* types of messages sent to PING client task from the console task,
 * Telnet server task, or timer tick task.  These messages provide 
 * configuration parameters, initiate (or terminate) ping requests, 
 * and provide periodic timeout notification.
 */
#define PING_CNTRL_START_PING     0x1
#define PING_CNTRL_END_PING       0x2
#define PING_CNTRL_SET_PARM       0x3
#define PING_CNTRL_PERIODIC_TIMER 0x4
#define PING_CNTRL_LIST_STATS     0x5
#define PING_DATA_ECHO_REPLY      0x6

/* type of parameter sent in PING_CNTRL_SET_PARM message */
#define PARM_TYPE_HOST   0x1
#define PARM_TYPE_DELAY  0x2
#define PARM_TYPE_LENGTH 0x3

/* structure of message sent to PING client task.  Note that not all 
 * parameters are used in all of the messages.
 */
struct pingtask_msg
{
   struct pingtask_msg * next;
   u_char type;
   u_long parm1;
   u_long parm2;
   u_long parm3;
   u_long parm4;
   u_long parm5;
};

/* Counters used by PING client code to keep track of error conditions 
 * encountered during processing
 */
struct ping_err
{
   u_long alloc_fail;
   u_long bad_msgtype;
   u_long bad_parmtype;
   u_long empty_q;
};

/* counters for the number of messages of various types received by 
 * the PING client task
 */
struct ping_msg_stats
{
   u_long start_ping;
   u_long end_ping;
   u_long set_parm;
   u_long periodic_timer;
   u_long list_stats;   
   u_long echo_reply;
};

extern struct queue pingRcvq;     /* contains messages from other tasks */

#endif   /* APP_PING_H */

