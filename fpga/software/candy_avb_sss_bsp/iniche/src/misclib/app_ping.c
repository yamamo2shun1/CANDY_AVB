/*
 * FILENAME: app_ping.c
 *
 * Copyright 1997- 2000 By InterNiche Technologies Inc. All rights reserved
 *
 * Generic ping command 
 * for sample utilitys. This uses the utilities in ..\inet\ping.c 
 * which comtains the fully portable but less functional ping lower 
 * layer. This is somewhat attached to the menus system, but outside 
 * of that is fairly portable; assumeing cticks & other NetPort IP 
 * system basics are present.
 *
 * MODULE: MISCLIB
 *
 * ROUTINES: ping_init(), ping_new(), ping_delete(), ping_addq(), 
 * ROUTINES: ping_delq(), ping_search(), ping_cmd_start(),  ping_send(), 
 * ROUTINES:ping_end(), pingUpcall(), ping_recv(), ping_check(),
 * ROUTINES: ping_demux(), ping_stats(),
 *
 * PORTABLE: yes
 */

/* Additional Copyrights: */
/* Portions Copyright 1993 by NetPort software */

#include "ipport.h"

#ifdef PING_APP   /* whole file can be ifdefed out */

#include "in_utils.h"

/* On superloop systems, define away task supoport */

#ifdef SUPERLOOP
#define  TK_WAKE(task)
#define  TK_BLOCK()
#endif   /* SUPERLOOP */

#include "q.h"
#include "netbuf.h"
#include "net.h"
#include "ip.h"
#include "icmp.h"

#include "menu.h"
#include "app_ping.h"

#include "dns.h"      


PING_INFO pingq = NULL;    /* Pointer to the first ping session */


struct queue    pingRcvq;     /* contains messages from other tasks */
extern char *   prompt;
ip_addr ping4_activehost = 0; /* default ping host */
u_long ping4_delay = TPS;     /* 1 second between pings */
u_long ping4_deflength = 64;     /* default ping packet data length */

int ping_cmd_end (void * pio);
int ping_cmd_start (void * pio);
int ping_cmd_list_stats (void * pio);
int ping_send_msg1 (u_char type, u_long parm1);
void ping_process_periodic_timer_msg (void);
void ping_process_rcvd_reply_pkt (PACKET pkt);
int ping_process_ping_parm (u_long type, u_long value);
int ping_process_start_ping_req (u_long dest, u_long count, void * pio);

struct ping_msg_stats ping_msg_stats;
struct ping_err ping_err;

#ifdef IN_MENUS
/* array of menu option, see menu.h */
/*
 * Altera Niche Stack Nios port modification:
 * braces to remove build warning
 */
struct menu_op settings[] =
{
   { "ping",     stooges,             "menu to set/view values for ping" },
   { "ping",     ping_cmd_start,      "Ping [host] [#times]" },
   { "endping",  ping_cmd_end,        "terminate the current ping session" },
   { "pstats",   ping_stats,          "display statistics about ping" },
   /* 'pstats' could use 'ping_cmd_list_stats' to enqueue for main loop processing */
   { NULL },
};
#endif   /* IN_MENUS */

extern   char *   pingdata;   /* default ping data in ..\net\ping.c */
#ifndef OS_PREEMPTIVE
static int in_pingcomp = 0;   /* re-entrancy guard */
#endif

/* FUNCTION: ping_init()
 * 
 * Initialize for pinging operations. Presently this function adds
 * the ping menu to MAIN menu.
 *
 * PARAM1: void
 *
 * RETURNS: 0
 */

int 
ping_init(void)
{
   pingq=NULL ;    

#ifdef IN_MENUS
   install_menu(settings);
#endif   /* IN_MENUS */
   return 0;
}


/* FUNCTION: ping_new()
 * 
 * 1. Allocate memory for a PingInfo structure Initialise elements
 *    to default values.
 * 2. Add it to the global queue, so that ping_check() can do the
 * house keeping. 
 *
 * PARAM1: none
 *
 * RETURNS: Pointer to the newly allocated structure 
 */

PING_INFO 
ping_new(void)
{
   PING_INFO p;

   p=(PING_INFO)PING_ALLOC(sizeof(struct PingInfo)) ;
   if ( p == NULL )
   {
      /* Memory allocation failed. */
      dtrap();
   }
   else
   {
      if ( ping_addq(p) != SUCCESS )
      {
         /* Can't add to queue */
         dtrap();
         ping_delete(p);
         p=NULL;
      }
      else
      {
         p->delay      =ping4_delay;
         p->length     =ping4_deflength;
         p->ipadd      =ping4_activehost;
         p->nextping   =0;
         p->times      =1L;
         p->pio        =NULL;
         p->out        =0;             /* reset counters */
         p->in         =0;
      }
   }

   return p;
}


/* FUNCTION: ping_delete()
 * 
 * 1. Remove the PingInfo structure from the global queue.
 * 2. Free memory allocated for it.
 *
 * PARAM1: PING_INFO p - Pointer to PingInfo structure (IN)
 *
 * RETURNS: 0 on SUCCESS or ENP_ error code.
 */

int 
ping_delete(PING_INFO p)
{
   int   retValue=SUCCESS;

   if ( p == NULL )
      return PING_DEL_NULL_ARGUMENT ;

   /* Remove the connection from the queue */
   retValue = ping_delq(p) ;
   if ( retValue != SUCCESS )
   {
      dtrap();
   }

   PING_FREE(p);
   p=NULL;

   return retValue;
}



/* FUNCTION: ping_addq()
 * 
 * Add a PingInfo object to global queue (of open connections)
 *
 * PARAM1: PING_INFO p - Pointer to object to be added
 *
 * RETURNS: 0 on SUCCESS or ENP_ error code.
 */

int 
ping_addq(PING_INFO p)
{
   if ( pingq == NULL )     
   {
      /* Queue is empty */
      pingq=p;
      p->next = NULL ;
   }
   else
   {
      /* Add it to the front of the queue */
      p->next = pingq ;
      pingq=p;
   }
   return SUCCESS ;
}



/* FUNCTION: ping_delq()
 * 
 * Delete a PingInfo object from the global queue
 * No Free (deallocation) is done. Only the queue is updated 
 *
 * PARAM1: PING_INFO p - Pointer to object to be deleted
 *
 * RETURNS: 0 on SUCCESS or ENP_ error code.
 */

int 
ping_delq(PING_INFO p)
{
   PING_INFO temp_obj;
   PING_INFO prev_obj;

   if ( p == NULL )
   {
      return PING_DELQ_BAD_OBJECT ;
   }

   if ( pingq == NULL )     
   {
      /* Queue is empty */
      return PING_DELQ_Q_EMPTY ;
   }
   else
   {
      /* Find the item and delete it */

      /* Check if it is the first item */
      if ( pingq == p )
      {
         /* Yes it is the first in the queue */
         pingq=p->next ;
         return SUCCESS ;
      }

      /* Find in the rest of the queue */
      prev_obj = pingq ;
      temp_obj = pingq->next ;

      while ( temp_obj !=NULL ) 
      {
         if ( temp_obj == p )
         {
            prev_obj->next = temp_obj->next ;
            return SUCCESS ;
         }
         prev_obj = temp_obj ;
         temp_obj = temp_obj->next ;
      }

      if ( temp_obj == NULL )
         return PING_DELQ_OBJ_NOT_FOUND ;

   }
   return SUCCESS;
}


/* FUNCTION: ping_search()
 * 
 * Ping can be done by multiple 
 * mechnisms (standard menu, telnet etc.). We will have only one ping 
 * session per mechanism. It is assumed that each mechanism (eg a 
 * telnet session) will use a unique GEN_IO structure. This function 
 * searches the list of PingInfo structures to find a match. Returns 
 * a pointer to PingInfo struct on success. Otherwise returns NULL. 
 * Remarks : This function can be used for finding out if we already 
 * have ping session or not (for a particular telnet session). It 
 * will be used when 1. User wants to do a new ping 2. User wants to 
 * terminate an earlier ping.
 *
 * PARAM1: GEN_IO pio - device for console output
 *
 * RETURNS: Pointer to PingInfo structure (if found). Otherwise NULL. 
 */

PING_INFO 
ping_search(GEN_IO pio)
{
   PING_INFO p;

   p=pingq;
   while ( p )
   {
      if ( p->pio == pio )
         break ;
      else
         p = p->next ;
   }

   return p;
}


/* FUNCTION: ping_cmd_start()
 * 
 * Called from menus to do a ping. 
 * Will handle an optional hosts name to ping and number of times to 
 * ping on the command line. Defaults to 1 ping to the active host. 0 
 * times will ping forever.
 *
 * PARAM1: GEN_IO pio - device for console output
 *
 * RETURNS: 0 on success, or one of the PING error codes
 */

int
ping_cmd_start (void * pio)
{
   char *   arg2;
   char *   arg3;
   char *   name;
   ip_addr ipadd;
   long  ping_times=1L;
#ifdef OS_PREEMPTIVE
   struct pingtask_msg * msgp;
#endif   
   int   e;

   ipadd = 0;
   arg2  = nextarg(((GEN_IO)pio)->inbuf);       /* get 1 or 2 args from command line */
   arg3  = nextarg(arg2);
   name  = arg2 ;

   if (*arg3)  /* If arg3 is present, null terminate arg2 */
   {
      while (*arg2 > ' ') arg2++ ;  /* scan to end of arg2 */
         *arg2 = 0;  /* null terminate string */
      arg2 = name;   /* Restore arg2 to start of string */
   }

   /* The second argument is either hostname(IP/dnsname) or number of times */
   if (*arg2)  /* first arg may be number or host */
   {
      /* if it's all digits, assume number */
      while (*arg2 > ' ')
      {
         if (*arg2 > '9' || *arg2 < '0')
         {
            /* not a digit; assume arg2 is a name */
            e = in_reshost(name, &ipadd, RH_VERBOSE | RH_BLOCK);
            if (e)
            {
               ns_printf(pio,"Unable to set up ping host \"%s\"\n", name);
               return -1;
            }
            break;   /* with arg2 pointer at non-digit && non-space */
         }
         arg2++;     /* check next char of arg */
      }
      if (*arg2 <= ' ') /* 2nd arg looks like a count */
      {
         ping_times = atol(name);
         if ((ping_times == 0) && (*name !='0'))
            ping_times=-1;    /* So that the error below can be displayed */ 
      }
   }
   if (*arg3)  /* if user specified 2 args, 2nd ard is ping_times */
   {
      ping_times = atol(arg3);
      if ((ping_times == 0) && (*arg3 !='0'))
         ping_times=-1;    /* So that the error below can be displayed */ 
   }

   if (ping_times < 0)
   {
      ns_printf(pio,"last arg must be number of times to ping (1-n)\n");
      ns_printf(pio,"or use 0 to ping forever. Default is 1\n");
      return PING_ST_BAD_ARG2;
   }

   if (!ipadd)
   {
      ns_printf(pio,"No IP address specified, will use default active host if one exists\n");
   }

   if (ping_times != 1)
      ns_printf(pio,"Use 'endping' to stop pinging, and 'icmpstat' and/or 'pstats' to examine the results...\n");

#ifndef OS_PREEMPTIVE
   return (ping_process_start_ping_req (ipadd, ping_times, pio));
#else
   msgp = (struct pingtask_msg *) PING_ALLOC (sizeof (struct pingtask_msg));
   if (!msgp)
   {
      ++ping_err.alloc_fail;
      return PING_ST_ALLOC_ERR;
   }
   
   /* send message for PING client task */
   msgp->type = PING_CNTRL_START_PING;
   msgp->parm1 = (u_long) ipadd;
   msgp->parm2 = ping_times;
   msgp->parm3 = (u_long) pio;

   LOCK_NET_RESOURCE (PINGQ_RESID);
   putq(&pingRcvq, (q_elt)msgp);
   UNLOCK_NET_RESOURCE (PINGQ_RESID);

   post_app_sem (PING_SEMID);
#endif

   return 0;
}


/* FUNCTION: ping_send()
 * 
 * Send a single ping packet based on static parms.
 *  This should be called only after start_ping() has 
 * set things up. It's designed to be called from a timed polling 
 * routine until all the pings are sent.
 *
 * PARAM1: PING_INFO p
 *
 * RETURNS: 0 on success, or -1 if error
 */

int
ping_send(PING_INFO p)
{
   int   e;          /* error holder */

#ifdef PING_REQ_OUTDEV
   /* We can have a variety of output devices thru pio mechanism. Eg. 
    * console, telnet client, etc. So consider the following scenario 
    * 1. InterNiche s/w with TELNET Server is running on 10.22 2. 
    * User (TELNET Client) does a TELNET connection from 10.20 3. 
    * User issues a ping-forever command. All ping output is being 
    * displayed in user's TELNET window. 4. User closes the TELNET 
    * client before terminating ping session. Now when PING 
    * application tries to display its output, it fails. In this case 
    * we close the ping session. 
    */
   if (ns_printf(p->pio,"ping %d to %s: ",p->out,print_ipad(p->ipadd)) == 0) 
   {
      /* Output device is no longer available for display */
      ping_delete(p);   /* shut off ping */
      return -1;
   }
#endif

   e = icmpEcho(p->ipadd, NULL, p->length, (unshort)p->out);

   p->times--;    /* count another one done */

   if (e == 0)
   {
      p->out++;
#ifdef PING_REQ_OUTDEV
      ns_printf(p->pio,"ping %d sent...\n",p->out);
#endif
   }
   else if(e == ARP_WAITING)
   {
#ifdef PING_REQ_OUTDEV
      ns_printf(p->pio,"Arping for host...\n");
#endif
      p->out++;
   }
   else
   {
#ifdef PING_REQ_OUTDEV
      ns_printf(p->pio,"Ping send failure %d\n",e);
#endif
      ping_end(p->pio);    /* shut off ping */
      return -1;
   }
   p->nextping = cticks + p->delay;    /* set time for next ping */

   return 0;
}



/* FUNCTION: ping_end()
 *
 * End an ongoing ping session. Calls ping_delete() to free 
 * resources used for ping session.
 *
 * PARAM1: GEN_IO pio - device for console output
 *
 * RETURNS: SUCCESS or PING_END_NO_SESSION
 */

int
ping_end(void * pio)
{
   PING_INFO p;

   p = ping_search((GEN_IO)pio);
   if ( p == NULL )
   {
      ns_printf(pio,"No ongoing ping session found.\n");
      return PING_END_NO_SESSION;
   }

#ifdef PING_REQ_OUTDEV
   if (p->out > 1)   /* print complete msg after multiple pings */
   {
      ns_printf(pio,"ping complete; sent %d, received %d\n",p->out,p->in);
   }
#endif

   ping_delete(p);

   return SUCCESS;
}


/* FUNCTION: ping_demux()
 * 
 * Find out the PingInfo structure 
 * corresponding to the received packet. This is tricky ,because we 
 * just know the source of the ping reply. So we need to go through 
 * the ping session list and find out the session that pinged to this 
 * IP address. Pitfalls : Say if two telnet sessions ping to the same 
 * host at the same time, and the destination replies, output of ping 
 * will always go to the first telnet session !!!
 *
 * PARAM1: ip_addr fhost - IP addres of machine that sent the ping reply (IN)
 *
 * RETURNS: Pointer to PingInfo structure or NULL 
 */

PING_INFO 
ping_demux(ip_addr fhost)
{
   PING_INFO p=pingq;

   while ( p )
   {
      if ( p->ipadd == fhost )
         break;
      else
         p=p->next;
   }

   return p;
}


/* FUNCTION: ping_recv()
 * 
 * This function is only used in non-preemptive mode.  It retrieves received
 * ICMP Echo Reply packets (from where pingUpcall () had deposited them)
 * and processes them.
 *
 * PARAM1: none
 *
 * RETURNS: void
 */
#ifndef OS_PREEMPTIVE 
void
ping_recv(void)
{
   PACKET pkt;   

   /* Process all the replies that have come */
   while (pingRcvq.q_len)
   {
      pkt = (PACKET)getq(&pingRcvq);
      if (!pkt)
         panic("ping_check: no pkt");
      ping_process_rcvd_reply_pkt (pkt);
   }
}
#endif

/* FUNCTION: ping_process_start_ping_req ()
 * 
 * This function is common to all operating environments.  It is invoked
 * after the user's ping request has been (largely) validated.  It does 
 * a few additional checks, and then creates a ping structure for the new 
 * session, and sends out the first ping request.
 *
 * INPUTS: (1) Destination of ping request, 
 *         (2) number of ping requests to send, and 
 *         (3) pointer to generic IO structure (from where the request
 *             was initiated).
 *
 * OUTPUT: 0, or one of the ping error codes.
 */
int ping_process_start_ping_req (u_long dest, u_long count, void * pio)
{
   PING_INFO p;
   int   e;
   
   /* check to make sure that we have a valid host */
   if (dest == 0)
   {
      if (ping4_activehost == 0) return PING_ST_NOIP;
      else dest = ping4_activehost;
   }

   /* Does the invoker of this command already has another ping going ? */
   p = ping_search((GEN_IO)pio);
   if ( p != NULL )
   {
      /* End the previous session */
      ns_printf(pio,"Terminating earlier ping operation.\n");
      ping_end(pio);
   }

   p = ping_new();
   if ( p == NULL )
   {
      dtrap();
      ns_printf(pio,"Error allocating memory for new ping.\n");
      return PING_ST_ALLOC_ERR;
   }

   /* Now we are ready to use p */
   p->ipadd = (ip_addr) dest;
   p->times = count;
   p->pio   = (GEN_IO)pio;

   /* if it is ping-forever, then delay should be atleaast 2 timeticks */
   if ((p->times == 0) && (p->delay < 2))
   {
      p->delay=2;
      ns_printf(pio,"Ping delay is too low for continuous pinging.\n");
      ns_printf(pio,"Hence delay for this session is %ld ms.\n", 
       p->delay*TIMEFOR1TICK);
   }

   e = ping_send(p); /* send 1st ping */

   return e;
}


/* FUNCTION: ping_cmd_end ()
 * 
 * This function is invoked to request the termination of a previously
 * started ping session.  For OS_PREEMPTIVE builds, this function
 * sends a message to the PING client task requesting the termination.
 * For other builds, this function executes in the context of the
 * task processing the user request.
 *
 * INPUT: (1) pointer to generic IO structure (from where the termination 
 *            request was initiated).
 *
 * OUTPUT: Return code from ping_end () for non-OS_PREEMPTIVE environment, and
 *         return code from ping_send_msg1 () for OS_PREEMPTIVE environments.
 */

int ping_cmd_end (void * pio)
{
#ifdef OS_PREEMPTIVE
   return (ping_send_msg1 (PING_CNTRL_END_PING, ((u_long) pio)));
#else
   return (ping_end (pio));
#endif

   return 0;
}


/* FUNCTION: ping_process_ping_parm
 * 
 * This function sets the destination host, delay, and length fields for
 * subsequent ping requests.
 * 
 * INPUT: (1) Type of parameter being sent (e.g., PARM_TYPE_HOST)
 *        (2) Value for parameter being sent
 *
 * OUTPUT: Return code from ping_process_ping_parm () for non-OS_PREEMPTIVE
 *         environments; 0 if successful and PING_ST_ALLOC_ERR if 
 *         couldn't allocate memory for message to PING client task
 *         (for multitasking environments).
 */
 
int ping_set_parm (u_long parm_type, u_long value)
{
#ifdef OS_PREEMPTIVE
   struct pingtask_msg * msgp;

   msgp = (struct pingtask_msg *) PING_ALLOC (sizeof (struct pingtask_msg));
   if (!msgp)
   {
      ++ping_err.alloc_fail;
      return PING_ST_ALLOC_ERR;
   }
   
   /* send message to PING client task */
   msgp->type = PING_CNTRL_SET_PARM;
   msgp->parm1 = parm_type;
   msgp->parm2 = value;

   LOCK_NET_RESOURCE (PINGQ_RESID);
   putq(&pingRcvq, (q_elt)msgp);
   UNLOCK_NET_RESOURCE (PINGQ_RESID);

   post_app_sem (PING_SEMID);
#else
   return (ping_process_ping_parm (parm_type, value));
#endif   

   return 0;
}


/* FUNCTION: ping_process_ping_parm
 * 
 * This function sets the destination host, delay, and length fields for
 * subsequent ping requests.
 * 
 * INPUT: (1) Type of parameter being sent (e.g., PARM_TYPE_HOST)
 *        (2) Value for parameter being sent
 *
 * OUTPUT: 0 if successful, or PING_ST_BAD_ARG2 if an unrecognized type
 *         is encountered.
 */

int ping_process_ping_parm (u_long type, u_long value)
{
   switch (type)
   {
      case PARM_TYPE_HOST:
         ping4_activehost = (ip_addr) value;
         break;
      case PARM_TYPE_DELAY:
         ping4_delay = value;
         break;
      case PARM_TYPE_LENGTH:
         ping4_deflength = value;
         break;
      default:
         ++ping_err.bad_parmtype;
         return PING_ST_BAD_ARG2;
   }

   return 0;
}


/* FUNCTION: ping_check ()
 *
 * This function is invoked from inet_timer.  For multitasking systems,
 * it deposits a PING_CNTRL_PERIODIC_TIMER message into the message
 * queue for the PING client task (the receipt of which triggers the
 * invocation of the ping_process_periodic_timer_msg () function).  For 
 * non-preemptive systems, it invokes the ping_process_periodic_timer_msg () 
 * function directly to perform periodic processing (e.g. sending 
 * additional ping requests for ongoing sessions).
 *
 * INPUT: None.
 *
 * OUTPUT: None.
 */
void
ping_check(void)
{
#ifndef OS_PREEMPTIVE
   if (in_pingcomp)  /* guard against re-entry */
      return;
   in_pingcomp++;    /* set re-entry flag */

   ping_recv();   /* check receives here */

   ping_process_periodic_timer_msg ();

   in_pingcomp--;    /* clear re-entry flag */
#else
   /* second parameter to ping_send_msg1 () is irrelevant */
   ping_send_msg1 (PING_CNTRL_PERIODIC_TIMER, 0);
#endif

   return;      
}


/* FUNCTION: ping_cmd_list_stats ()
 * 
 * This function is invoked from the menu system to request the ping
 * configuration and latest ping statistics.
 *
 * INPUT: Pointer to generic IO structure from where the request originated.
 *
 * OUTPUT: Return code from ping_stats (for non-preemptive).  For 
 *         preemptive environments, return code from ping_send_msg1 ().
 */
 
void
ping_process_periodic_timer_msg (void)
{
   PING_INFO p = pingq;
   PING_INFO nextp;
   GEN_IO pio;

   /* Send more ping packets, if so configured. */
   while (p)
   {
      nextp = p->next ;
      if ((p->nextping != 0) && (p->nextping <= cticks))
      {
         if (p->times != 0)
         {
            ping_send(p);
         }
         else
         {
            pio = p->pio;
#ifdef PING_REQ_OUTDEV
            ns_printf(pio, prompt); /* print prompt before p->pio is freed */
#endif
            ping_end(pio);            
         }
      }
      p = nextp;
   }

}


/* ping_cmd_list_stats ()
 * 
 * This function is invoked from the menu system to request the ping
 * configuration and latest ping statistics.
 *
 * INPUT: Pointer to generic IO structure from where the request originated.
 *
 * OUTPUT: Return code from ping_stats (for SUPERLOOP).  For other
 *         operating environments, return code from ping_send_msg1 ().
 */

int 
ping_cmd_list_stats (void * pio)
{
#ifdef OS_PREEMPTIVE
   return (ping_send_msg1 (PING_CNTRL_LIST_STATS, ((u_long) pio)));
#else
   return (ping_stats (pio));
#endif

}


/* FUNCTION: ping_stats()
 * 
 * Display statistics about ping sessions.
 *
 * PARAM1: GEN_IO pio - device for console output
 *
 * RETURNS: SUCCESS.
 */

int
ping_stats(void * pio)
{
   PING_INFO p=pingq;
   u_long cnt=0;

   ns_printf(pio,"Default ping delay time: %lu ms\n", ping4_delay*TIMEFOR1TICK);
   ns_printf(pio,"Default ping host: %s\n", print_ipad(ping4_activehost));
   ns_printf(pio,"Default ping data length: %lu bytes\n", ping4_deflength);

   while (p)
   {
      cnt++;
      ns_printf(pio,"Statistics about pinging %s\n",
       print_ipad(p->ipadd) );
      ns_printf(pio,"Times=%ld, Length=%d, Delay=%ld\n",
       p->times, p->length, p->delay*TIMEFOR1TICK );
      ns_printf(pio,"Packets : sent=%d, received=%d\n\n",
       p->out, p->in);
      p=p->next;
   }

   ns_printf(pio,"There are %lu ongoing ping sessions.\n",cnt);

   return SUCCESS;
}


/* FUNCTION: pingUpcall()
 *
 * called from ICMP layer when we receive an ICMP Echo Reply.
 * The received packet is queued in pingRcvq for the SUPERLOOP
 * operating environment.  For multitasking environments, it 
 * is encapsulated inside a message structure (of type 
 * PING_DATA_ECHO_REPLY) that is then sent to the PING client 
 * task.
 *
 * PARAM1: PACKET p -  Pointer to the received packet (IN)
 *
 * RETURNS: 0 for SUPERLOOP; return code from ping_send_msg1 ()
 *          for other operating environments. 
 */

int 
pingUpcall(PACKET p)
{
#ifndef OS_PREEMPTIVE
   putq(&pingRcvq, (q_elt)p);
#else   
   return (ping_send_msg1 (PING_DATA_ECHO_REPLY, ((u_long) p)));
#endif
   
   return 0;
}


/* FUNCTION: ping_process_rcvd_reply_pkt ()
 * 
 * This function is invoked to process a received ICMP Echo Reply
 * packet.  It validates the received reply, and then prints out
 * status information for the relevant ping session.  This function
 * is common to all operating environments.
 *
 * INPUT: Pointer to received packet.
 *
 * OUTPUT: None.
 */
void
ping_process_rcvd_reply_pkt (PACKET pkt)
{
   struct ping *  pp;
   u_char * pdata;
   int   i, dataend, pdlen;
   int   data_len;      /* length of DATA in recived ping */
   int   save_seq;
   ip_addr save_host;
   PING_INFO p;

   pp = (struct ping *)pkt->nb_prot;
   /* the upcalled packet included the ICMP header. Strip this off
    * for the purposes of reporting data size
    */
   data_len = pkt->nb_plen -= sizeof(struct ping);
   save_seq = (unsigned)pp->pseq;
   save_host = pkt->fhost;

   /* If the ping data fields looks like one of our defaults, verify
    * the contents. This relies on the code in ..\inet\ping.c
    */
   pdata = (u_char*)(pp+1);   /* pointer to ping data */
   pdlen = strlen(pingdata);
   if (strncmp(pingdata, (char*)pdata, pdlen) == 0)
   {
      pdata += pdlen;      /* point past verified text */
      dataend = pkt->nb_plen - pdlen;
      for (i = pdlen; i < dataend; i++)
      {
         /*
          * Altera Niche Stack Nios port modification:
          * cast arg to remove build warning
          */
         if (*pdata++ != (u_char)(i & 0xFF))
         {
            dprintf("ping data mismatch at byte %u\n",
            (unsigned int)(i + pdlen + sizeof(struct ping)) );
            break;
         }
      }
   }
   LOCK_NET_RESOURCE(FREEQ_RESID);
   pk_free(pkt);
   UNLOCK_NET_RESOURCE(FREEQ_RESID);

   /* Output the result at the caller's output device */
   /* For that, first find out the sender of this ping */

   p = ping_demux(save_host) ;
   if ( p != NULL )
   {
      p->in++;
#ifdef PING_REQ_OUTDEV
      if (ns_printf(p->pio,"got ping reply; data len %d seq %d from %s\n",
                    data_len, save_seq, print_ipad(save_host)) == 0)
      {
         /* Output device is no longer available for display */
         ping_delete(p);   /* shut off ping */
      }
      else if (ns_printf(p->pio,prompt) == 0)
      {
         /* Output device is no longer available for display */
         ping_delete(p);   /* shut off ping */
      }
#endif
         
   }
#ifndef TESTMENU  /* don't flood console if testing */
   else
   {
      dprintf("Could not find invoker of ping %s\n",print_ipad(pkt->fhost));
   }
#endif
}

#ifdef OS_PREEMPTIVE
/* FUNCTION: ping_send_msg1 ()
 *
 * This function is used to send a message with zero or one parameters
 * to the PING client task.
 * 
 * INPUT: (1) Type of message being sent (e.g., PING_CNTRL_START_PING)
 *        (2) Parameter #1 of message being sent (optional; not used
 *            for messages that do not require a parameter (such as
 *            the PING_CNTRL_PERIODIC_TIMER message).
 *
 * OUTPUT: 0 if successful, or PING_ST_ALLOC_ERR if the message structure
 *         cannot be allocated.
 * 
 */
 
int 
ping_send_msg1 (u_char type, u_long parm1)
{
   struct pingtask_msg * msgp;

   msgp = (struct pingtask_msg *) PING_ALLOC (sizeof (struct pingtask_msg));
   if (!msgp)
   {
      ++ping_err.alloc_fail;
      return PING_ST_ALLOC_ERR;
   }
   
   /* send message to PING client task */
   msgp->type = type;
   msgp->parm1 = parm1;

   LOCK_NET_RESOURCE (PINGQ_RESID);
   putq(&pingRcvq, (q_elt)msgp);
   UNLOCK_NET_RESOURCE (PINGQ_RESID);

   post_app_sem (PING_SEMID);

   return 0;
}


/* FUNCTION: ping_process_rcvd_msgs ()
 * 
 * This function is used in multitasking environments.  It processes
 * messages received from other tasks (such as console task, Telnet
 * server task, and timer task).  These messages provide configuration
 * parameters, initiate (or terminate) ping requests, and provide
 * periodic timeout notification.  This function is invoked after the
 * PING client task returns from its wait for the PING client
 * semaphore.
 *
 * INPUT: None.
 *
 * OUTPUT: None.
 */

void 
ping_process_rcvd_msgs (void)
{
   struct pingtask_msg * msgp;

   LOCK_NET_RESOURCE (PINGQ_RESID);
   msgp = getq (&pingRcvq);
   UNLOCK_NET_RESOURCE (PINGQ_RESID);
   
   if (!msgp) 
   {
      ++ping_err.empty_q;
      return;
   }

   switch (msgp->type)
   {
      case PING_CNTRL_START_PING:
         /* message parameters: destination (IP address), number of ping requests, 
          * pointer to GEN_IO structure */
         ++ping_msg_stats.start_ping;
         ping_process_start_ping_req (msgp->parm1, msgp->parm2, ((void *) msgp->parm3));
         break;
      case PING_CNTRL_END_PING:
         /* parameter: pointer to GEN_IO structure */
         ++ping_msg_stats.end_ping;
         ping_end ((void *) msgp->parm1);
         break;
      case PING_CNTRL_SET_PARM:
         /* parameters: type of parameter (PARM_TYPE_HOST, PARM_TYPE_DELAY, or 
          * PARM_TYPE_LENGTH), value for parameter */
         ++ping_msg_stats.set_parm;
         ping_process_ping_parm (msgp->parm1, msgp->parm2);
         break;
      case PING_CNTRL_PERIODIC_TIMER:
         ++ping_msg_stats.periodic_timer;
         ping_process_periodic_timer_msg ();
         break;
      case PING_CNTRL_LIST_STATS:
         /* parameter: pointer to GEN_IO structure */
         ++ping_msg_stats.list_stats;
         ping_stats ((void *) msgp->parm1);
         break;         
      case PING_DATA_ECHO_REPLY:
         ++ping_msg_stats.echo_reply;
         /* parameter: pointer to received packet containing ICMP Echo Reply */
         ping_process_rcvd_reply_pkt ((PACKET) msgp->parm1);
         break;
      default:
         /* increment error counter */
         ++ping_err.bad_msgtype;
         break;
   }

   /* free the data structure containing the just processed message */
   PING_FREE (msgp);

   return;
}
#endif /* OS_PREEMPTIVE */

#endif   /* PING_APP */

