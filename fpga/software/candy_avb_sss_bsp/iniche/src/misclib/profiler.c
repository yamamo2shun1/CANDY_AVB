/*
 * FILENAME: profiler.c
 *
 * Copyright  2000 By InterNiche Technologies Inc. All rights reserved
 *
 * profiler.c Profile code for portions of the InterNiche TCP/IP 
 * stack and related modules. 
 *
 *
 * MODULE: MISCLIB
 *
 * ROUTINES: pf_start(), pf_stop(), pf_stats(), pm_start(), 
 * ROUTINES: pm_stop(), pf_ticklog(),
 *
 * PORTABLE: no
 */


#include "ipport.h"

#ifdef USE_PROFILER  /* whole file can be ifdeffed */

#include "in_utils.h"
#include "menu.h"
#include "profiler.h"


static u_long   start_tick;
static u_long   stop_tick;
static u_long   last_tick;


struct pbucket * PF_IP;
struct pbucket * PF_USUM;
struct pbucket * PF_IDLE;
struct pbucket * PF_TCP;
struct pbucket * PF_TSUM;
struct pbucket * PF_WEB ;
struct pbucket * PF_FTP;
struct pbucket * PF_FS;
struct pbucket * PF_NAT;
struct pbucket * PF_IPSEC;
struct pbucket * PF_OTHER;       /* bucket for everything we don't track */
struct pbucket * PF_CRYPTOENG;

/* predefine the array of pbuckets for modules we can profile by default */

struct pbucket in_pbuckets[NUM_PBUCKETS];


#define  NEST_DEPTH     8
static struct pbucket * current;
static struct pbucket * nested[NEST_DEPTH];
static int     nestindex;
static int     pfinitialized = FALSE;
static int     pfrunning = 0;

int   pfmodules;     /* number of modules being profiled */


/* If you have trouble when enabling the profiler (especially
 * if you have added modeuls at runtime), then enable the define
 * below for some error checking. Don't leave it on for general
 * use since it slows things up quite a bit.
 */

#ifdef NOTDEF
#define PFDEBUG
#endif

/* FUNCTION: pf_start()
 * 
 * lowest level profile start primitive. This is exported for
 * applications to call
 *
 * PARAM1: N/A
 *
 * RETURNS: N/A
 */

void
pf_start()
{
   int i;

   if(pfrunning)
      return;

   if(pfinitialized == FALSE)
   {
      int module = 0;

      /* skip past any runtime added modules */
      while(in_pbuckets[module].name)
         module++;

      PF_IP = &in_pbuckets[module];
      in_pbuckets[module].name = "IP";
      in_pbuckets[module++].id = &PF_IP;

      PF_USUM = &in_pbuckets[module];
      in_pbuckets[module].name = "UDP sum";
      in_pbuckets[module++].id = &PF_USUM;

      PF_OTHER = &in_pbuckets[module];
      in_pbuckets[module].name = "other";
      in_pbuckets[module++].id = &PF_OTHER;

#ifdef INICHE_TASKS /* idle loop */
      PF_IDLE = &in_pbuckets[module];
      in_pbuckets[module].name = "idle";
      in_pbuckets[module++].id = &PF_IDLE;
#endif

#ifdef INCLUDE_TCP
      PF_TCP = &in_pbuckets[module];  /* TCP layer & sockets */
      in_pbuckets[module].name = "TCP";
      in_pbuckets[module++].id = &PF_TCP;

      PF_TSUM = &in_pbuckets[module];   /* TCP checksumming  */
      in_pbuckets[module].name = "TCP sum";
      in_pbuckets[module++].id = &PF_TSUM;
#endif
#ifdef WEBPORT
      PF_WEB = &in_pbuckets[module];
      in_pbuckets[module].name = "WebPort";
      in_pbuckets[module++].id = &PF_WEB;
#endif
#ifdef FTP_SERVER
      PF_FTP = &in_pbuckets[module];
      in_pbuckets[module].name = "FTP srv";
      in_pbuckets[module++].id = &PF_FTP;
#endif
#ifdef VFS_FILES
      PF_FS = &in_pbuckets[module];  /* VFS (may include native) */
      in_pbuckets[module].name = "VFS";
      in_pbuckets[module++].id = &PF_FS;
#endif
#ifdef NATRT
      PF_NAT = &in_pbuckets[module];  /* NAT routing */
      in_pbuckets[module].name = "NAT";
      in_pbuckets[module++].id = &PF_NAT;
#endif
#ifdef IPSEC
      PF_IPSEC = &in_pbuckets[module];  /* IPSEC */
      in_pbuckets[module].name = "IPSEC";
      in_pbuckets[module++].id = &PF_IPSEC;
#endif
#ifdef USE_CRYPTOENG
      PF_CRYPTOENG = &in_pbuckets[module];  
      in_pbuckets[module].name = "CryptoEngine";
      in_pbuckets[module++].id = &PF_CRYPTOENG;
#endif

      pfinitialized = TRUE;

      /* end of firsttime setup. */
   }
   
   pfmodules = 0;    /* init active module count */

   /* reset all the bucket times to zero */
   for (i = 0; i < NUM_PBUCKETS; i++)
   {
      in_pbuckets[i].ticks = 0;
      if(in_pbuckets[i].name)
         pfmodules++;      /* count active modules */
   }

   /* Call the routine which gets the current "profile tick" count. This
    * is generally not the same as cticks - it should be much faster.
    * This routine should be implemented by all hardware ports (BSPs)
    * which support profiling. If it shows up as unresolved during
    * link time then you need to provide it.
    */
   start_tick = get_ptick();     /* call hardware tick routine */

   last_tick = start_tick;
   pfrunning = 1;
   nestindex = 0;       /* now supended profile threads */
   current = NULL;      /* no current profile thread. */
}



/* FUNCTION: pf_stop()
 * 
 * lowest level profile stop primitive. This is exported for
 * applications to call
 *
 * PARAM1: N/A
 *
 * RETURNS: N/A
 */

void
pf_stop()
{
   if (!pfrunning)
      return;

   stop_tick = get_ptick();

   pfrunning = 0;
}


/* FUNCTION: pf_addmodule()
 * 
 * this is called to add a new module to profiling. The returned value is 
 * the module ID which should be passed to IN_PROFILER() to begin/exit
 * profiling the module. The passed name should be static text.
 *
 *
 * PARAM1: char * name of module
 *
 * RETURNS: Id of module or NULL if error .
 */


void *
pf_addmodule(char * name)
{
   int i;

   if(pfmodules >= (NUM_PBUCKETS - 2))
      return NULL;

   for (i = 0; i < NUM_PBUCKETS; i++)
   {
      if(in_pbuckets[i].name == NULL)  /* found empty bucket? */
      {
         in_pbuckets[i].name = name;
         in_pbuckets[i].ticks = 0;
         in_pbuckets[i].id = (struct pbucket**)(&(in_pbuckets[i].id));
         return ((void*)in_pbuckets[i].id);
      }
   }
   return 0;      /* no empty buckets */
}


/* FUNCTION: pf_ticklog()
 * 
 * Exported routine called by profiled modules to indicate when
 * they are being entered or exited.
 *
 * PARAM1: int id     - module ID (e.g. PF_TCP)
 * PARAM2: int opcode - PF_ENRY or PF_EXIT
 *
 * RETURNS: 
 */

void
pf_ticklog(struct pbucket * id, int opcode)
{
   u_long   now;        /* current tick */
   u_long   elapsed;    /* ticks since last call */

#ifdef PFDEBUG
   if((id->id != id) || (if->name == NULL))
   {
      dtrap();    /* invalid id parameter */
      return;
   }
   if ((opcode != PF_ENTRY) && (opcode != PF_EXIT))
   {
      dtrap();    /* bogus opcode passed */
      return;
   }
#endif /* PFDEBUG */

   /* don't do this if we're not profiling */
   if (!pfrunning)
      return;

   /* get current tick value */
   now = get_ptick();

   /* figure out elapsed tick count - check for wrap */
   if(now >= last_tick)
      elapsed = now - last_tick;
   else        /* tick wrapped */
      elapsed = now + ((u_long)0xFFFFFFFF - last_tick) + 1;

   /* see if we are entering or exiting a profile section */
   if (opcode == PF_ENTRY)
   {
      if (nestindex >= NEST_DEPTH)
      {
#ifdef PFDEBUG
         printf("pf_ticklog(): exceeded profile nesting limit\n");
#endif
/*         dtrap();*/    /* exceeded profile nesting limit */
         return;
      }

#ifdef PFDEBUG
      if(current == id)
      {
         dtrap();    /* double entry in same module */
      }
#endif /* PFDEBUG */

      /* see if we are preempting another running profile bucket */
      if (current)
      {
         current->ticks += elapsed;       /* save count for prempted module */
         nested[nestindex++] = current;   /* "push" prempted bucket */
      }
      else
         PF_OTHER->ticks += elapsed;

      current = id;     /* switch current bucket to passed ID */
   }
   else  /* opcode == PF_EXIT */
   {
      id->ticks += elapsed;

      /* see if we had preempted another running profile bucket */
      if (nestindex)
         current = nested[--nestindex];   /* restore preempted bucket */
      else
         current = NULL;
   }

   last_tick = now;
}


#ifdef IN_MENUS

static   int   pm_start(void * pio);
static   int   pm_stop(void * pio);
static   int   pf_stats(void * pio);
static   int   pm_wrap(void * pio);
extern   int   pkc_tcpsess(void * pio);

struct menu_op profmenu[] =
{
   "profile",  stooges,       "profiler menu",      /* menu ID */
   "pbegin",   pm_start,      "begin profiling",
   "pend",     pm_stop,       "end profiling",
   "pwrap",    pm_wrap,       "Wrap menu command inside profile start/stop",
   "pdump",    pf_stats,      "dump profile data",
#ifdef   PKT_CYCLES
   "pcmake",   pkc_start,     "Create pkt cycle pseudo driver",
   "pcdel",    pkc_del,       "Delete pkt cycle pseudo driver",
   "pcalb",    pkc_calibrate, "Calibrate profile ticks value",
   "pcrecv",   pkc_mode,      "Toggle use of rcvdq in pkt cycle tests",
   "pcping",   pkc_ping,      "Generate ping rx from pseudo driver",
   "pctcp",    pkc_tcpecho,   "Generate TCP echo packets from pseudo driver",
   "pcudp",    pkc_udpecho,   "Generate UDP echo REQ from pseudo driver",
   "pcsess",   pkc_tcpsess,   "Do entire TCP echo sessions",
   "pcdata",   pkc_data,      "Display current  pkt cycle data",
#endif   /*PKT_CYCLES  */
   NULL,
};


/* the rest of this file in Iniche Menus system interface */

/* FUNCTION: pf_percent()
 * 
 * pf_percent() - helper routine to calculate the percentage the passed
 * "ticks" is of the passed "total".
 *
 * PARAM1: total - number to take the percent from
 * PARAM2: ticks - take this percentage of total
 * PARAM3: pct - (OUT) percentage ticks is of total (0 - 99)
 * PARAM4: dec - (OUT) decmal point for pct
 *
 * RETURNS: values in pct and dec
 */

void
pf_percent(u_long total, u_long ticks, int * pct, int * dec)
{
   u_long percent;
   u_long decimal;

   /* avoid divide by zero and other wierd math */
   if((total < 1) || (ticks > total))
   {
      *pct = *dec = 0;
      return;
   }

   /* Calculate integers for percentage and decimal place.
    * for large values, avoid letting the data wrap.
    * for small values of total, avoid losing the first three
    * significant figures.
    */

   if(ticks > 10000)
   {
      percent = (int)(ticks / (total / 100) );
      decimal = (int)(ticks / (total / 1000) );
      decimal -= (percent * 10);
   }
   else     /* small value */
   {
      percent = (int)((ticks * 100) / total);
      decimal = (int)((ticks * 1000) / total);
      decimal -= (percent * 10);
   }
   *pct = (int)percent;
   *dec = (int)decimal;
}

/* FUNCTION: pf_stats()
 * 
 * Menu routine - profile info dump routine.
 
 * PARAM1: void * pio
 *
 * RETURNS: 
 */

static int
pf_stats(void * pio)
{
   int   i;
   int   pct;
   int   dec;
   u_long   total;

   if (pfrunning)
   {
      ns_printf(pio, "Profiling, please stop first.\n");
      return -1;
   }

   /* get total elapsed ticks. Check for wrap. If it wrapped more than
    * once then we are hosed:
    */
   if(stop_tick > start_tick)
      total = stop_tick - start_tick;
   else
      total = stop_tick + (0xFFFFFFFF - start_tick);

   ns_printf(pio, "total ticks:%10lu\n", total);

   if (total == 0)
      return 0;

   ns_printf(pio, "  module  ticks     (percent)\n");

   /* display the bucket times */
   for (i = 0; i< NUM_PBUCKETS; i++)
   {
      /* skip entrys which are not set */
      if (in_pbuckets[i].name == NULL)
         continue;
      pf_percent(total, in_pbuckets[i].ticks, &pct, &dec);
      ns_printf(pio, "%11s %10lu (%3u.%u)\n", in_pbuckets[i].name,
         in_pbuckets[i].ticks, pct, dec );
   }

   return 0;
}


/* FUNCTION: pm_start()
 * 
 * Menu routine - wrapper for pf_Start()
 *
 * PARAM1: void * pio
 *
 * RETURNS: 
 */

static   int
pm_start(void * pio)
{
   pf_start();
   ns_printf(pio, "Profiling started.\n");
   return 0;
}



/* FUNCTION: pm_stop()
 * 
 * Menu routine - wrapper for pf_Stop()
 *
 * PARAM1: void * pio
 *
 * RETURNS: 
 */

static   int
pm_stop(void * pio)
{
   pf_stop();
   ns_printf(pio, "Profiling stopped.\n");
   pf_stats(pio);
   return 0;
}

/* FUNCTION: pm_wrap()
 * 
 * Menu routine - wrap another menu command in a profile start/stop
 *
 * PARAM1: void * pio
 *
 * RETURNS: 
 */

static   int
pm_wrap(void * pio)
{
   char * arg;

   arg = nextarg( ((GEN_IO)pio)->inbuf );
   if(*arg == 0)
   {
      ns_printf(pio, "usage: pwrap menu command [and args]");
      return -1;
   }

   ns_printf(pio, "profiling command \"%s\"\n", arg);

   /* move args up to front of cmd buf for pass to menus */
   MEMMOVE(((GEN_IO)pio)->inbuf, arg, strlen(arg) );

   pf_start();             /* start profiling */
   do_command(pio );       /* recurse into menu system */
   pf_stop();              /* stop profiling */
   pf_stats(pio);          /* show what happend */
   return 0;
}

#endif   /* IN_MENUS */

#endif   /*  USE_PROFILER */



