/*
 * FILENAME: rawiptst.c
 *
 * Copyright 1995-2002 InterNiche Technologies Inc. All rights reserved.
 *
 * Test program for raw IP sockets API.  Implements raw IP 
 * testers which send and receive datagrams from raw sockets.
 *
 * MODULE: MISCLIB
 *
 * ROUTINES: raw_testerq_lock(), raw_testerq_unlock(),
 * ROUTINES: raw_tester_new(), raw_tester_del(), raw_tester_delid(),
 * ROUTINES: raw_tester_findid(), 
 * ROUTINES: raw_testerq_poll(),
 * ROUTINES: raw_ristart(), raw_ridelete(), raw_rilist(), raw_risend(),
 * ROUTINES: raw_rihalt(), raw_ribind(), raw_riconnect(), raw_richeck(),
 * ROUTINES: raw_rihdrincl(), raw_test_init()
 *
 * PORTABLE: yes
 */

#include "ipport.h"     /* NetPort embedded system includes */

#ifdef RAWIPTEST        /* whole file can be ifdef'd out */

#include "tcpport.h"    /* NetPort embedded system includes */
#include "in_utils.h"
#include "dns.h"
#include "menu.h"

/* RAWIPTEST_BUFLEN - length of buffer used for receives and sends
 *
 * Ideally, this should be large enough to hold the largest datagram
 * you expect to send or receive, including its IP header.
 *
 * You can override this definition from ipport.h if desired.
 */
#ifndef RAWIPTEST_BUFLEN
#define RAWIPTEST_BUFLEN      1536
#endif  /* RAWIPTEST_BUFLEN */

/* build up a sockets-API abstraction
 */
#ifndef SOCKTYPE
#define SOCKTYPE        long
#endif /* SOCKTYPE */

#define  socket(x,y,z)           t_socket(x,y,z)
#define  bind(s,a,al)            t_bind(s,a)
#define  connect(s,a,al)         t_connect(s,a)
#define  send(s,b,l,f)           t_send(s,b,l,f)
#define  sendto(s,b,l,f,a,al)    t_sendto(s,b,l,f,a)
#define  recvfrom(s,b,l,f,a,al)  t_recvfrom(s,b,l,f,a)
#define  socketclose(s)          t_socketclose(s)
#define  getsockopt(s,l,o,d,dl)  t_getsockopt(s,o,d)
#define  setsockopt(s,l,o,d,dl)  t_setsockopt(s,o,d)

/* raw_tester_buf[] - buffer used for receives and sends
 */
static char raw_tester_buf[RAWIPTEST_BUFLEN];

extern   ip_addr  activehost;    /* default host to send to */
extern   u_long   pingdelay;     /* time (ticks) to delay between sends */
extern   int      deflength;     /* default send data length */

static   char *   echodata =    "UDP echo number:            ";

/* List of error codes used by this file */
#define  RITE_BASE                  (-9100)
#define  RITE_ALLOC_ERROR           (RITE_BASE+1)
#define  RITE_SOCKET_FAILED         (RITE_BASE+2)
#define  RITE_SETOPT_FAILED         (RITE_BASE+3)
#define  RITE_TESTER_NOTFOUND       (RITE_BASE+4)

#define  SUCCESS                    0

/* struct raw_tester - raw IP socket tester
 */
struct raw_tester
{
   struct raw_tester * next;     /* next raw_tester in queue */
   void * pio;                   /* where to write console output */
   unsigned id;                  /* tester identification */
   SOCKTYPE sock;                /* socket for this tester */
   unsigned conn : 1;            /* flag: socket is connected */
   unsigned sending : 1;         /* flag: tester is sending */
   unsigned rxcheck : 1;         /* flag: check received data */
   unsigned long sendtoaddr;     /* IP address (network order) to send to */
   long sendcount;               /* how many sends to do */
   long sendsdone;               /* how many sends done */
   int sendlen;                  /* # bytes to send each time */
   unsigned long senddelay;      /* ticks to delay after each send */
   unsigned long lastsendtime;   /* tick count at last send */
   int senderr;                  /* socket error that stopped sending */
};

/* raw_testerq - queue of testers
 */
static struct raw_tester * raw_testerq = NULL;

/* raw_testerq_locked - flag indicating that raw_testerq is locked
 *                      for modification
 */
static volatile int raw_testerq_locked = 0;

/* raw_tester_idseq - identification sequence for raw tester
 */
static unsigned raw_tester_idseq = 0;

/* FUNCTION: raw_testerq_lock()
 *
 * Unconditionally locks the raw_testerq, blocking until locked.
 *
 * RETURNS: void
 */
void
raw_testerq_lock(void)
{
   do {
      while (raw_testerq_locked)
         tk_yield();
      raw_testerq_locked++;
      if (raw_testerq_locked != 1)
         raw_testerq_locked--;
   } 
   while (raw_testerq_locked != 1);
}

/* FUNCTION: raw_testerq_unlock()
 *
 * Unlocks the raw_testerq.
 *
 * RETURNS: void
 */
void
raw_testerq_unlock(void)
{
   raw_testerq_locked--;
}

/* FUNCTION: raw_tester_new()
 *
 * Creates a new raw IP tester.  Allocates memory for a new 
 * raw_tester structure, fills it in, links it to the queue.
 */
int
raw_tester_new(void * pio, 
               int protocol)
{
   struct raw_tester * newtester;
   int e;

   /* allocate storage for the tester state */
   newtester = (struct raw_tester *) npalloc(sizeof(struct raw_tester));
   if (newtester == NULL)
   {
      ns_printf(pio, "raw_tester_new: npalloc() failed\n");
      return RITE_ALLOC_ERROR;
   }

   /* give it a new ID */
   newtester->id = raw_tester_idseq++;

   /* keep track of where to write console output */
   newtester->pio = pio;

   /* make a socket of given protocol */
   newtester->sock = socket(PF_INET, SOCK_RAW, protocol);
   if (newtester->sock == -1)
   {
      ns_printf(pio, "raw_tester_new: [%u] socket() failed\n",
                newtester->id);
      npfree(newtester);
      return RITE_SOCKET_FAILED;
   }

   /* put the socket in non-blocking mode 
    * NOTE this needs changing for non-InterNiche stacks
    */
   e = t_setsockopt(newtester->sock, SO_NBIO, NULL);
   if (e == -1)
   {
      ns_printf(pio, 
                "raw_tester_new: [%u] t_setsockopt() failed, err %d\n",
                newtester->id, t_errno(newtester->sock));
      socketclose(newtester->sock);
      npfree(newtester);
      return RITE_SETOPT_FAILED;
   }

   /* lock the tester queue */
   raw_testerq_lock();

   /* link the tester state into the queue */
   newtester->next = raw_testerq;
   raw_testerq = newtester;

   /* unlock the tester queue */
   raw_testerq_unlock();

   /* and return the ID of the new tester */
   return newtester->id;
}

/* FUNCTION: raw_tester_del()
 *
 * Deletes a raw IP tester.  Removes it from the tester queue,
 * closes its socket (if open), frees its storage.
 * Assumes tester queue is locked.
 *
 * PARAM1: struct raw_tester * tester; IN- ptr to tester to delete
 * 
 * RETURNS:
 *
 */
int
raw_tester_del(struct raw_tester * tester)
{
   struct raw_tester * qtester;     /* current entry in queue */
   struct raw_tester * pqtester;    /* previous entry in queue */

   /* locate the tester in the queue */
   for (pqtester = NULL, qtester = raw_testerq; 
        qtester != NULL;
        pqtester = qtester, qtester = qtester->next)
      if (qtester == tester)
         break;

   /* if we didn't find it, bail out */
   if (qtester == NULL)
      return RITE_TESTER_NOTFOUND;

   /* unlink it from the queue */
   if (pqtester == NULL)
      raw_testerq = qtester->next;
   else
      pqtester->next = qtester->next;

   /* if it's got a socket, close it */
   if (qtester->sock != INVALID_SOCKET)
   {
      socketclose(qtester->sock);
   }

   /* free its storage */
   npfree(qtester);

   /* and return success */
   return SUCCESS;
}

/* FUNCTION: raw_tester_delid()
 *
 * Deletes a raw IP tester.  Removes it from the tester queue,
 * closes its socket (if open), frees its storage.
 * Assumes tester queue is locked.
 *
 * PARAM1: unsigned id; IN- ID of tester to delete
 * 
 * RETURNS: integer indicating success or failure.
 */
int
raw_tester_delid(unsigned id)
{
   struct raw_tester * qtester;     /* current entry in queue */
   struct raw_tester * pqtester;    /* previous entry in queue */

   /* locate the tester in the queue */
   for (pqtester = NULL, qtester = raw_testerq; 
        qtester != NULL;
        pqtester = qtester, qtester = qtester->next)
      if (qtester->id == id)
         break;

   /* if we didn't find it, bail out */
   if (qtester == NULL)
      return RITE_TESTER_NOTFOUND;

   /* unlink it from the queue */
   if (pqtester == NULL)
      raw_testerq = qtester->next;
   else
      pqtester->next = qtester->next;

   /* if it's got a socket, close it */
   if (qtester->sock != INVALID_SOCKET)
   {
      socketclose(qtester->sock);
   }

   /* free its storage */
   npfree(qtester);

   /* and return success */
   return SUCCESS;
}

/* FUNCTION: raw_tester_findid()
 *
 * Deletes a raw IP tester.  Removes it from the tester queue,
 * closes its socket (if open), frees its storage.
 * Assumes tester queue is locked.
 *
 * PARAM1: unsigned id; IN- ID of tester to delete
 * 
 * RETURNS: integer indicating success or failure.
 */
struct raw_tester *
raw_tester_findid(unsigned id)
{
   struct raw_tester * qtester;     /* current entry in queue */

   /* locate the tester in the queue */
   for (qtester = raw_testerq; 
        qtester != NULL;
        qtester = qtester->next)
      if (qtester->id == id)
         break;

   /* return pointer to it (or NULL if not found) */
   return qtester;
}

/* FUNCTION: raw_testerq_poll()
 *
 * Polls all the raw testers for work
 *
 * RETURNS: void
 */
void
raw_testerq_poll(void)
{
   struct raw_tester * tester;
   struct raw_tester * nexttester;
   int len;
   struct sockaddr_in addr;
   struct ip * pip;
   unsigned addrlen;
   char * dptr;
   unsigned donedata;
   int hdrincl;
   int match;
   int e;

   /* only proceed if we can lock the tester queue immediately */
   if (raw_testerq_locked++)
   {
      raw_testerq_locked--;
      return;
   }

   /* iterate through the testers, checking for work to do */
   for (tester = raw_testerq; tester != NULL; tester = nexttester)
   {

      /* keep track of which tester is next,
       * so we know what to do next 
       * even if we have to delete the current one due to error
       */
      nexttester = tester->next;

      if (tester->sock != INVALID_SOCKET)
      {
         do 
         {
            /* find out whether the socket has the IP_HDRINCL option set
             */
            e = getsockopt(tester->sock, IPPROTO_IP, IP_HDRINCL,
                           &hdrincl, sizeof(hdrincl));
            if (e < 0)
            {
               ns_printf(tester->pio,
                         "raw_testerq_poll: [%d] getsockopt error %d\n",
                         tester->id, t_errno(tester->sock));
               tester->sending = 0;
               break;
            }

            /* check the tester for received data */
            addrlen = sizeof(addr);
            len = recvfrom(tester->sock, 
                           raw_tester_buf, sizeof(raw_tester_buf), 0,
                           (struct sockaddr *)&addr, &addrlen);
            if (len < 0)
            {
               e = t_errno(tester->sock);
               if (e != EWOULDBLOCK)
               {
                  ns_printf(tester->pio, 
                            "raw_testerq_poll: [%d] recvfrom() error %d\n",
                            tester->id, e);
                  e = raw_tester_del(tester);
               }
               break;
            }

            /* if requested, verify received data
             * this leaves match set as follows:
             *   0 if the received data is not verified
             *   1 if the received data matches what we send
             *   -1 if the received data does not match what we send
             */
            match = 0;
            if (tester->rxcheck)
            {
               dptr = raw_tester_buf;
               if (hdrincl)
               {
                  pip = (struct ip *)dptr;
                  dptr += (pip->ip_ver_ihl & 0xf) << 2;
               }
               donedata = 0;
               while (dptr < raw_tester_buf + len)
               {
                  if (*dptr != (char)(donedata & 0xff))
                  {
                     match = -1;
                     break;
                  }
                  dptr++;
                  donedata++;
               }
               if (match == 0)
                  match = 1;
            }

            ns_printf(tester->pio,
                      "raw_testerq_poll: [%d] rcvd %d bytes %sfrom %u.%u.%u.%u\n",
                      tester->id, len, 
                      (match > 0) ? "(OK) " : (match < 0) ? "(not OK) " : "",
                      PUSH_IPADDR(addr.sin_addr.s_addr));

         }
         while (len > 0);

         /* if tester is sending, and it's time to do the next send...
          */
         if ((tester->sending) &&
             ((tester->sendsdone == 0) ||
              (cticks - tester->lastsendtime > tester->senddelay)))
         {
            dptr = raw_tester_buf;

            if (hdrincl)
            {
               /* XXX fill in the IP header */
               pip = (struct ip *)dptr;
               dptr += sizeof(struct ip);
            }

            /* fill the buffer out to the send length with data:
             * sequence 0, 1, 2, ..., 255, 0, 1, 2, ..., 255 until
             * we reach the send length
             * (note that if we built the IP header above its
             * size is included in the send length!)
             */
            donedata = 0;
            while (dptr - raw_tester_buf < tester->sendlen)
            {
               *dptr++ = (char)((donedata++) & 0xff);
            }

            /* send the buffer 
             */
            if (tester->conn && (tester->sendtoaddr == 0))
            {
               e = send(tester->sock, raw_tester_buf, tester->sendlen, 0);
               if (e < 0)
               {
                  ns_printf(tester->pio,
                            "raw_testerq_poll: [%d] send error %d\n",
                            tester->id, t_errno(tester->sock));
                  tester->senderr = t_errno(tester->sock);
                  tester->sending = 0;
                  continue;
               }
            }
            else
            {
               addr.sin_family = AF_INET;
               addr.sin_port = 0;
               addr.sin_addr.s_addr = tester->sendtoaddr;
               e = sendto(tester->sock, raw_tester_buf, tester->sendlen, 0,
                          (struct sockaddr *)&addr, &addrlen);
               if (e < 0)
               {
                  ns_printf(tester->pio,
                            "raw_testerq_poll: [%d] sendto error %d (to %u.%u.%u.%u)\n",
                            tester->id, t_errno(tester->sock),
                            PUSH_IPADDR(tester->sendtoaddr));
                  tester->senderr = t_errno(tester->sock);
                  tester->sending = 0;
                  continue;
               }
            }
            tester->lastsendtime = cticks;
            tester->sendsdone++;

            /* if we're done, stop sending
             */
            if (tester->sendcount && tester->sendsdone >= tester->sendcount)
            {
               tester->sending = 0;
            }
         }

      }
      
   }

   /* unlock the receiver queue */
   raw_testerq_unlock();
}


/* FUNCTION: raw_ristart()
 *
 * Starts (creates) a new raw IP tester.
 *
 * PARAM1: void * pio; IN- ptr to GEN_IO object for command-line output
 *
 * RETURNS: an integer indicating success or failure; 
 *          zero indicates success; values <0 indicate failure
 */
int
raw_ristart(void * pio)
{
   char * arg2;
   int prot;
   unsigned id;

   arg2 = nextarg(((GEN_IO)pio)->inbuf);
   if (!arg2 || !*arg2)
   {
      ns_printf(pio, "requires IP protocol ID\n");
      return -1;
   }
   prot = atoi(arg2);
   if ((prot < 0) || (prot > 255))
   {
      ns_printf(pio, "IP protocol ID must be between 0 and 255 inclusive\n");
      return -1;
   }

   id = raw_tester_new(pio, prot);
   if (id < 0)
   {
      ns_printf(pio, "unable to create new tester: error %d\n", id);
      return -1;
   }
   ns_printf(pio, "started new tester [%d]\n", id);
   return 0;
}

/* FUNCTION: raw_ridelete()
 *
 * Deletes a raw IP tester.
 *
 * PARAM1: void * pio; IN- ptr to GEN_IO object for command-line output
 *
 * RETURNS: an integer indicating success or failure; 
 *          zero indicates success; values <0 indicate failure
 */
int
raw_ridelete(void * pio)
{
   char * arg2;
   long lid;
   unsigned id;
   int e;

   arg2 = nextarg(((GEN_IO)pio)->inbuf);
   if (!arg2 || !*arg2)
   {
      ns_printf(pio, "requires raw IP tester ID\n");
      return -1;
   }
   lid = atol(arg2);
   id = (unsigned)lid;
   if (((long)id) != lid)
   {
      ns_printf(pio, "invalid ID %ld\n", lid);
      return -1;
   }

   raw_testerq_lock();
   e = raw_tester_delid(id);
   raw_testerq_unlock();
   if (e < 0)
   {
      ns_printf(pio, "unable to delete tester [%d]: error %d\n", id, e);
      return -1;
   }
   ns_printf(pio, "deleted tester [%d]\n", id);
   return 0;
}

/* FUNCTION: raw_rilist()
 *
 * Displays a list of raw IP testers with state information for each.
 *
 * PARAM1: void * pio; IN- ptr to GEN_IO object for command-line output
 *
 * RETURNS: an integer indicating success or failure; 
 *          zero indicates success; values <0 indicate failure
 */
int
raw_rilist(void * pio)
{
   char * arg2;
   long lid;
   unsigned id;
   unsigned listall;
   struct raw_tester * tester;

   /* figure out whether we're supposed to list a specified ID
    * or all IDs: all IDs is indicated by the absence of the ID
    * argument
    */
   arg2 = nextarg(((GEN_IO)pio)->inbuf);
   if (!arg2 || !*arg2)
   {
      listall = 1;
#ifdef MUTE_WARNS
      id = 0;
#endif /* MUTE_WARNS */
   }
   else
   {
      listall = 0;
      lid = atol(arg2);
      id = (unsigned)lid;
      if (((long)id) != lid)
      {
         ns_printf(pio, "invalid ID %ld\n", lid);
         return -1;
      }
   }

   /* at this point, either listall should be 1 (meaning all IDs
    * are to be listed) or listall should be 0 and id should be the
    * ID to be listed
    */

   /* lock the queue */
   raw_testerq_lock();

   /* if no testers, say as much; else list appropriately */
   if (raw_testerq == NULL)
   {
      ns_printf(pio, "no raw testers\n");
   }
   else
   {
      for (tester = raw_testerq; tester != NULL; tester = tester->next)
      {
         if (listall || tester->id == id)
         {
            ns_printf(pio, "ID %u%c,%ssending (%ld/%ld, err %d)\n",
                      tester->id, 
                      ((pio == tester->pio) ? '*' : ' '),
                      ((tester->sending) ? " " : " not "),
                      tester->sendsdone,
                      tester->sendcount,
                      tester->senderr);
         }
      }
   }

   /* unlock the queue */
   raw_testerq_unlock();

   /* and return success */
   return 0;
}

/* FUNCTION: raw_risend()
 *
 * Tells a tester to begin sending.
 *
 * PARAM1: void * pio; IN- ptr to GEN_IO object for command-line output
 *
 * RETURNS: an integer indicating success or failure; 
 *          zero indicates success; values <0 indicate failure
 */
int
raw_risend(void * pio)
{
   char * arg2;
   char * arg3;
   char * arg4;
   long lid;
   unsigned id;
   struct raw_tester * tester;
   unsigned long hostaddr;
   unsigned hostaddrfound;
   long sendcount;
   int e;

   /* get first argument: tester ID (required) 
    */
   arg2 = nextarg(((GEN_IO)pio)->inbuf);
   if (!arg2 || !*arg2)
   {
      ns_printf(pio, "requires raw IP tester ID\n");
      return -1;
   }
   lid = atol(arg2);
   id = (unsigned)lid;
   if (((long)id) != lid)
   {
      ns_printf(pio, "invalid ID %ld\n", lid);
      return -1;
   }

   /* get optional second and third arguments:
    *   host address to which to send 
    *     defaults to activehost for un-connected testers
    *     defaults to connected address for connected testers
    *   send count
    *     defaults to 1
    *     0 means send forever (or until otherwise stopped)
    */
   hostaddrfound = 0;
   hostaddr = activehost;
   sendcount = 1;
   arg3 = nextarg(arg2);
   arg4 = nextarg(arg3);
   if (*arg4)
   {
      /* there's a third argument, so make sure the second argument
       * (presumed at this point to be a host address) is null-terminated
       */
      arg2 = arg3;
      while (*arg2 > ' ') 
         arg2++;
      *arg2 = 0;
   }

   /* the second argument (arg3) is either a host address (name or 
    * IP address) or it's a send count
    * so we look at it to see if it's all digits -- 
    * if it's not, then we guess it's a host address
    * if it is, then we guess it's a send count
    */
   if (arg3 && *arg3)
   {
      arg2 = arg3;
      while (*arg2 > ' ')
      {
         if (*arg2 > '9' || *arg2 < '0')
         {
            e = in_reshost(arg3, &hostaddr, RH_VERBOSE | RH_BLOCK);
            if (e)
            {
               ns_printf(pio, "Unable to resolve host name \"%s\"\n", arg3);
               return -1;
            }
            hostaddrfound = 1;
            break;
         }
         arg2++;
      }
      if (*arg2 <= ' ')
      {
         sendcount = atol(arg3);
         if ((sendcount == 0) && (*arg3 != '0'))
            sendcount = -1;
      }
   }

   /* if there's a third argument,
    * assume it's a send count
    */
   if (arg4 && *arg4)
   {
      sendcount = atol(arg4);
      if ((sendcount == 0) && (*arg4 != '0'))
         sendcount = -1;
   }

   /* make sure we the host address makes sense
    */
   if (hostaddrfound && (hostaddr == 0))
   {
      ns_printf(pio, 
                "specify valid host address, or use default host address\n");
      return -1;
   }

   /* make sure the send count makes sense
    */
   if (sendcount < 0)
   {
      ns_printf(pio, 
                "last argument must be send count (>= 1)\n");
      ns_printf(pio,
                "or 0 to send without limit; default is 1.\n");
      return -1;
   }

   /* lock the queue */
   raw_testerq_lock();

   /* look for the tester */
   tester = raw_tester_findid(id);
   if (tester == NULL)
   {
      ns_printf(pio, "ID [%d] not found\n", id);
      raw_testerq_unlock();
      return -1;
   }

   /* if the tester is already sending, we will stop this action
    */
   if (tester->sending)
   {
      ns_printf(pio, "Terminating previous send on ID [%d]\n", id);
      tester->sending = 0;
   }

   /* set the tester up to send:
    * take sendcount from argument
    * take sendlen and senddelay from globals deflength and pingdelay
    *   (same as used by ping)
    * if hostaddr argument present, use it as sendtoaddr;
    * else let sendtoaddr default:
    *   if the socket is connected, to the connected address;
    *   else to global activehost (same as used by ping)
    */
   tester->sendcount = sendcount;
   tester->sendsdone = 0;
   tester->senderr = 0;
   if (deflength > sizeof(raw_tester_buf))
   {
      ns_printf(pio, "Send length limited to %d bytes\n",
                id, sizeof(raw_tester_buf));
      tester->sendlen = sizeof(raw_tester_buf);
   }
   else
      tester->sendlen = deflength;
   tester->senddelay = pingdelay;
   if (hostaddrfound)
      tester->sendtoaddr = hostaddr;
   else
   {
      if (tester->conn)
         tester->sendtoaddr = 0;
      else
         tester->sendtoaddr = activehost;
   }
   tester->sending = 1;

   /* unlock the queue */
   raw_testerq_unlock();

   /* poll once to get things going */
   raw_testerq_poll();

   /* and return success */
   return 0;
}

/* FUNCTION: raw_rihalt()
 *
 * Tells a tester to stop sending.
 *
 * PARAM1: void * pio; IN- ptr to GEN_IO object for command-line output
 *
 * RETURNS: an integer indicating success or failure; 
 *          zero indicates success; values <0 indicate failure
 */
int
raw_rihalt(void * pio)
{
   char * arg2;
   long lid;
   unsigned id;
   struct raw_tester * tester;

   /* get argument: tester ID (required) 
    */
   arg2 = nextarg(((GEN_IO)pio)->inbuf);
   if (!arg2 || !*arg2)
   {
      ns_printf(pio, "requires raw IP tester ID\n");
      return -1;
   }
   lid = atol(arg2);
   id = (unsigned)lid;
   if (((long)id) != lid)
   {
      ns_printf(pio, "invalid ID %ld\n", lid);
      return -1;
   }

   /* lock the queue */
   raw_testerq_lock();

   /* look for the tester */
   tester = raw_tester_findid(id);
   if (tester == NULL)
   {
      ns_printf(pio, "ID [%d] not found\n", id);
      raw_testerq_unlock();
      return -1;
   }

   /* if the tester is sending, stop it
    */
   if (tester->sending)
   {
      ns_printf(pio, "Terminating previous send on ID [%d]\n", id);
      tester->sending = 0;
   }
   else
   {
      ns_printf(pio, "ID [%d] not sending\n", id);
   }

   /* unlock the queue */
   raw_testerq_unlock();

   /* and return success */
   return 0;
}

/* FUNCTION: raw_ribind()
 *
 * Tells a tester to bind to a given IP address.
 *
 * PARAM1: void * pio; IN- ptr to GEN_IO object for command-line output
 *
 * RETURNS: an integer indicating success or failure; 
 *          zero indicates success; values <0 indicate failure
 */
int
raw_ribind(void * pio)
{
   char * arg2;
   char * arg3;
   long lid;
   unsigned id;
   unsigned long bindaddr;
   struct raw_tester * tester;
   struct sockaddr_in addr;
   int e;

   /* get argument: tester ID (required) 
    */
   arg2 = nextarg(((GEN_IO)pio)->inbuf);
   if (!arg2 || !*arg2)
   {
      ns_printf(pio, "requires raw IP tester ID\n");
      return -1;
   }
   lid = atol(arg2);
   id = (unsigned)lid;
   if (((long)id) != lid)
   {
      ns_printf(pio, "invalid ID %ld\n", lid);
      return -1;
   }

   /* get second argument: host address to bind to (required)
    */
   arg3 = nextarg(arg2);
   if (!arg3 || !*arg3)
   {
      ns_printf(pio, "requires host address to bind to\n");
      return -1;
   }

   e = in_reshost(arg3, &bindaddr, RH_VERBOSE | RH_BLOCK);
   if (e)
   {
      ns_printf(pio, "Unable to resolve host name \"%s\"\n", arg3);
      return -1;
   }

   /* lock the queue */
   raw_testerq_lock();

   /* look for the tester */
   tester = raw_tester_findid(id);
   if (tester == NULL)
   {
      ns_printf(pio, "ID [%d] not found\n", id);
      raw_testerq_unlock();
      return -1;
   }

   /* bind the tester's socket to the given address
    */
   addr.sin_family = AF_INET;
   addr.sin_port = 0;
   addr.sin_addr.s_addr = bindaddr;
   e = bind(tester->sock, (struct sockaddr *)&addr, sizeof(addr));
   if (e < 0)
   {
      ns_printf(pio, "ID [%d] bind error %d to address %u.%u.%u.%u\n", 
                id, t_errno(tester->sock), PUSH_IPADDR(bindaddr));
   }
   else
   {
      ns_printf(pio, "ID [%d] bound to address %u.%u.%u.%u\n", 
                id, PUSH_IPADDR(bindaddr));
      e = 0;
   }

   /* unlock the queue */
   raw_testerq_unlock();

   /* and return success/failure */
   return e;
}

/* FUNCTION: raw_riconnect()
 *
 * Tells a tester to connect to a given IP address.
 *
 * PARAM1: void * pio; IN- ptr to GEN_IO object for command-line output
 *
 * RETURNS: an integer indicating success or failure; 
 *          zero indicates success; values <0 indicate failure
 */
int
raw_riconnect(void * pio)
{
   char * arg2;
   char * arg3;
   long lid;
   unsigned id;
   unsigned long connaddr;
   struct raw_tester * tester;
   struct sockaddr_in addr;
   int e;

   /* get argument: tester ID (required) 
    */
   arg2 = nextarg(((GEN_IO)pio)->inbuf);
   if (!arg2 || !*arg2)
   {
      ns_printf(pio, "requires raw IP tester ID\n");
      return -1;
   }
   lid = atol(arg2);
   id = (unsigned)lid;
   if (((long)id) != lid)
   {
      ns_printf(pio, "invalid ID %ld\n", lid);
      return -1;
   }

   /* get second argument: host address to connect to (required)
    */
   arg3 = nextarg(arg2);
   if (!arg3 || !*arg3)
   {
      ns_printf(pio, "requires host address to connect to\n");
      return -1;
   }

   e = in_reshost(arg3, &connaddr, RH_VERBOSE | RH_BLOCK);
   if (e)
   {
      ns_printf(pio, "Unable to resolve host name \"%s\"\n", arg3);
      return -1;
   }

   /* lock the queue */
   raw_testerq_lock();

   /* look for the tester */
   tester = raw_tester_findid(id);
   if (tester == NULL)
   {
      ns_printf(pio, "ID [%d] not found\n", id);
      raw_testerq_unlock();
      return -1;
   }

   /* connect the tester's socket to the given address
    */
   addr.sin_family = AF_INET;
   addr.sin_port = 0;
   addr.sin_addr.s_addr = connaddr;
   e = connect(tester->sock, (struct sockaddr *)&addr, sizeof(addr));
   if (e < 0)
   {
      ns_printf(pio, "ID [%d] connect error %d to address %u.%u.%u.%u\n", 
                id, t_errno(tester->sock), PUSH_IPADDR(connaddr));
   }
   else
   {
      if (connaddr == 0)
      {
         ns_printf(pio, "ID [%d] disconnected\n", id);
         tester->conn = 0;
      }
      else
      {
         ns_printf(pio, "ID [%d] connected to address %u.%u.%u.%u\n", 
                   id, PUSH_IPADDR(connaddr));
         tester->conn = 1;
      }
      e = 0;
   }

   /* unlock the queue */
   raw_testerq_unlock();

   /* and return success/failure */
   return e;
}

/* FUNCTION: raw_richeck()
 *
 * Tells a tester to toggle its "verify received data" setting
 *
 * PARAM1: void * pio; IN- ptr to GEN_IO object for command-line output
 *
 * RETURNS: an integer indicating success or failure; 
 *          zero indicates success; values <0 indicate failure
 */
int
raw_richeck(void * pio)
{
   char * arg2;
   long lid;
   unsigned id;
   struct raw_tester * tester;

   /* get argument: tester ID (required) 
    */
   arg2 = nextarg(((GEN_IO)pio)->inbuf);
   if (!arg2 || !*arg2)
   {
      ns_printf(pio, "requires raw IP tester ID\n");
      return -1;
   }
   lid = atol(arg2);
   id = (unsigned)lid;
   if (((long)id) != lid)
   {
      ns_printf(pio, "invalid ID %ld\n", lid);
      return -1;
   }

   /* lock the queue */
   raw_testerq_lock();

   /* look for the tester */
   tester = raw_tester_findid(id);
   if (tester == NULL)
   {
      ns_printf(pio, "ID [%d] not found\n", id);
      raw_testerq_unlock();
      return -1;
   }

   /* if the tester is verifying received data, stop it; else start
    */
   ns_printf(pio, "Turning receive verification %s for ID [%d]\n", 
             (tester->rxcheck) ? "off" : "on",
             id);
   tester->rxcheck = (tester->rxcheck) ? 0 : 1;

   /* unlock the queue */
   raw_testerq_unlock();

   /* and return success */
   return 0;
}

/* FUNCTION: raw_rihdrincl()
 *
 * Tells a tester to toggle its "include IP header" setting
 *
 * PARAM1: void * pio; IN- ptr to GEN_IO object for command-line output
 *
 * RETURNS: an integer indicating success or failure; 
 *          zero indicates success; values <0 indicate failure
 */
int
raw_rihdrincl(void * pio)
{
   char * arg2;
   long lid;
   unsigned id;
   struct raw_tester * tester;
   int hdrincl;
   int optlen;
   int e;

   /* get argument: tester ID (required) 
    */
   arg2 = nextarg(((GEN_IO)pio)->inbuf);
   if (!arg2 || !*arg2)
   {
      ns_printf(pio, "requires raw IP tester ID\n");
      return -1;
   }
   lid = atol(arg2);
   id = (unsigned)lid;
   if (((long)id) != lid)
   {
      ns_printf(pio, "invalid ID %ld\n", lid);
      return -1;
   }

   /* lock the queue */
   raw_testerq_lock();

   /* look for the tester */
   tester = raw_tester_findid(id);
   if (tester == NULL)
   {
      ns_printf(pio, "ID [%d] not found\n", id);
      raw_testerq_unlock();
      return -1;
   }

   /* find out whether the socket has the IP_HDRINCL option set
    */
   optlen = sizeof(hdrincl);
   e = getsockopt(tester->sock, IPPROTO_IP, IP_HDRINCL,
                  &hdrincl, &optlen);
   if (e < 0)
   {
      ns_printf(pio,
                "getsockopt error %d on ID [%d]\n",
                t_errno(tester->sock), tester->id);
      raw_testerq_unlock();
      return -1;
   }

   /* if the tester is verifying received data, stop it; else start
    */
   ns_printf(pio, "Turning IP_HDRINCL %s for ID [%d]\n", 
             (hdrincl) ? "off" : "on", id);
   hdrincl = (hdrincl) ? 0 : 1;
   e = setsockopt(tester->sock, IPPROTO_IP, IP_HDRINCL,
                  &hdrincl, sizeof(hdrincl));
   if (e < 0)
   {
      ns_printf(pio,
                "setsockopt error %d on ID [%d]\n",
                t_errno(tester->sock), tester->id);
      raw_testerq_unlock();
      return -1;
   }

   /* unlock the queue */
   raw_testerq_unlock();

   /* and return success */
   return 0;
}

#ifdef IN_MENUS
/* raw_tester_menu[] - NicheTool command menu for raw IP testing
 */
struct menu_op raw_tester_menu[] =
{
   "rawiptest",   stooges,       "menu to control raw IP testing",
   "ristart",     raw_ristart,   "start a raw IP tester",
   "ridelete",    raw_ridelete,  "delete a raw IP tester",
   "rilist",      raw_rilist,    "list raw IP testers",
   "risend",      raw_risend,    "tell a raw IP tester to start sending",
   "rihalt",      raw_rihalt,    "tell a raw IP tester to stop sending",
   "ribind",      raw_ribind,    "bind a raw IP tester to a host address",
   "riconnect",   raw_riconnect, "connect a raw IP tester to a host address",
   "richeck",     raw_richeck,   "toggle receive data verification",
   "rihdrincl",   raw_rihdrincl, "toggle IP_HDRINCL socket option",
   NULL,
};
#endif /* IN_MENUS */

/* FUNCTION: raw_test_init()
 * 
 * Initialize for raw IP testing operations. Presently this function adds
 * the raw IP testing menu.
 *
 * PARAM1: void
 *
 * RETURNS: 0
 */
int
raw_test_init(void)
{
#ifdef IN_MENUS
   install_menu(raw_tester_menu);
#endif /* IN_MENUS */
   return 0;
}

#endif      /* RAWIPTEST */
