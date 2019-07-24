/*
 * FILENAME: ftpcport.c
 *
 * Copyright 2002 By InterNiche Technologies Inc. All rights reserved
 *
 *
 * MODULE: FTP Client
 *
 * ROUTINES: ftpc_nvset(), prep_ftpc(), tk_ftpclnt()
 *
 * PORTABLE: NO
 */

#include "ftpport.h"    /* TCP/IP, sockets, system info */

#ifdef FTP_CLIENT

#include "ftpsrv.h"
#include "ftpclnt.h"

#ifndef SUPERLOOP
#ifndef OSPORT_H
#error Need to define OSPORT_H
#endif
#include OSPORT_H
#endif /* SUPERLOOP */

#ifdef INCLUDE_NVPARMS
#include "nvparms.h"
#endif /* INCLUDE_NVPARMS */

#ifdef IN_MENUS
#include "menu.h"
#endif /* IN_MENUS */

#ifdef IN_MENUS
extern struct menu_op ftpmenu[];
#endif /* IN_MENUS */

#ifdef INCLUDE_NVPARMS
/* Please see nvparms.h and nvparms.c regarding the usage of
 * the following datatypes and functions.
 */

int ftpc_nvset(NV_FILE * fp);

struct ftpc_nvparam ftpc_nvparms;

struct nvparm_info ftpc_nvformats[] = 
{
   {"ftpc reply timout: %u\n"   , NVINT, NVBND_NOOP, \
    &ftpc_nvparms.fc_replytmo    , NULL, }, 
   {"ftpc activity timout: %u\n", NVINT, NVBND_NOOP, \
    &ftpc_nvparms.fc_activity_tmo, NULL, }, 
};

#define NUMFTPC_FORMATS  \
        (sizeof(ftpc_nvformats)/sizeof(struct nvparm_info))

#endif /* INCLUDE_NVPARMS */

#ifdef INCLUDE_NVPARMS

/* FUNCTION: ftpc_nvset()
 * 
 * PARAM1: NV_FILE * fp
 *
 * RETURNS: Silent return of 0 for OK
 */
int ftpc_nvset(NV_FILE * fp)
{
int i = 0;

   nv_fprintf(fp, ftpc_nvformats[i++].pattern, ftpc_nvparms.fc_replytmo);
   nv_fprintf(fp, ftpc_nvformats[i++].pattern, ftpc_nvparms.fc_activity_tmo);
   return 0;
}

struct nvparm_format ftpc_format = 
{
   NUMFTPC_FORMATS,
   &ftpc_nvformats[0],
   ftpc_nvset,
   NULL
};

#endif /* INCLUDE_NVPARMS */


/* FUNCTION: prep_ftpc()
 * 
 * PARAMS: NONE
 *
 * RETURNS: Error Code or 0 for OK
 */
int prep_ftpc(void)
{
int e = 0;
#ifdef IN_MENUS
   /* install the FTP Client commands */
   e = install_menu(&ftpmenu[0]);
#endif /* IN_MENUS */

#ifdef INCLUDE_NVPARMS
   e = install_nvformat(&ftpc_format, nv_formats);
   if(e)
   {
      dprintf("unable to install FTPClient NVPARMS reconfigure nv_formats[]\n");
      dtrap();
   }
#endif /* INCLUDE_NVPARMS */
   return e;
}


#ifndef SUPERLOOP

#ifdef FTP_CLIENT
TK_OBJECT(to_ftpclnt);
TK_ENTRY(tk_ftpclnt);
long ftpclnt_wakes = 0;
#endif

/*
 * Altera Niche Stack Nios port modification:
 * Use task priority and stack size values from ipport.h
 */
#ifdef FTP_CLIENT
struct inet_taskinfo ftpctask = {
      &to_ftpclnt,
      "FTP client",
      tk_ftpclnt,
      TK_FTPCLNT_TPRIO,
      TK_FTPCLNT_SSIZE,
};
#endif

/* The FTP client task waits for the FTP semaphore to be signaled.
 * The semaphore is signaled by other tasks such as console task, Telnet
 * server task, and timer task after they have deposited a message into
 * the FTP client task's message queue.  These messages provide 
 * configuration parameters, initiate (or terminate) FTP transfer requests, 
 * and provide periodic timeout notification.  The latter is currently
 * where the bulk of the work (wrt transfers) is done.
 */

/* FUNCTION: tk_ftpclnt()
 * 
 * PARAM1: n/a
 *
 * RETURNS: n/a
 */
 
#ifdef FTP_CLIENT

TK_ENTRY(tk_ftpclnt)
{
   while (!iniche_net_ready)
      TK_SLEEP(1);

#ifndef OS_PREEMPTIVE
   for (;;)
   {
      int work;

      work = fc_check();   /* Let client spin, get level of load */
      switch(work)         /* base task action on work level */
      {
      case 2:
         tk_yield();       /* quick give up of CPU in case it didn't block */
         break;
      case 1:
         TK_SLEEP(1);      /* pass a time tick before checking again */
         break;
      case 0:
         TK_BLOCK();       /* all done, go back to sleep */
         break;
      default:
         dtrap();          /* bad return value */
         break;
      }
      ftpclnt_wakes++;   /* count wakeups */
      if (net_system_exit)
         break;
   }
#else
   for (;;)
   {
      wait_app_sem (FTPC_SEMID);
      ftpclnt_wakes++; /* count wakeups */
      ftpc_process_rcvd_msgs (); /* process messages received from other tasks */
      if (net_system_exit)
         break;
   }
#endif

   TK_RETURN_OK();
}

#endif /* FTP_CLIENT */

#endif /* SUPERLOOP */

#endif /* FTP_CLIENT */

