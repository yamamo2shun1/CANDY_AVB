/*
 * FILENAME: tk_misc.c
 *
 * Copyright 2002 By InterNiche Technologies Inc. All rights reserved
 *
 * Routines to create Network Tasks for the Generic Multitasking systems ports
 * of InterNiche TCP/IP NetOS package. Also contains other miscellaneous 
 * routines for Multitasking ports.
 *
 * MODULE: ALLPORTS
 *
 * ROUTINES: create_apptasks(), tk_keyboard()
 * ROUTINES: task_stats()
 *
 * PORTABLE: yes
 */

#include "ipport.h"
#include "libport.h"

/* Multitasking systems should use this file, however superloop
 *systems need to be able to ifdef it away
 */

#ifndef SUPERLOOP /* whole file can be ifdeffed out */

/* include project header. Define this in ipport.h to point to a file
 * in your project directory, i.e. "..\******\osport.h"
 */
 
#ifndef OSPORT_H
#error Need to define OSPORT_H
#endif
#include OSPORT_H

#ifdef WEBPORT
extern struct inet_taskinfo httptask;
extern long webport_wakes;
#endif
#ifdef FTP_SERVER
extern struct inet_taskinfo ftpstask;
extern long ftpsrv_wakes;
#endif
#ifdef FTP_CLIENT
extern struct inet_taskinfo ftpctask;
#endif
#ifdef SMTP_ALERTS
extern struct inet_taskinfo smtptask;
extern long emailer_wakes;
#endif
#ifdef TELNET_SVR
extern struct inet_taskinfo telnettask;
extern long telnetsrv_wakes;
#endif
#ifdef USE_CRYPTOENG
extern struct inet_taskinfo cuexecutetask;
extern long cuexecute_wakes;
#endif
#ifdef USE_SYSLOG_TASK
extern struct inet_taskinfo syslog_task;
extern long syslog_wakes;
#endif
#ifdef INCLUDE_SNMP
extern struct inet_taskinfo snmptask;
extern long snmp_wakes;
#endif
#ifdef DNS_SERVER
extern struct inet_taskinfo dnssrvtask;
#endif
#ifdef NICHVIEW
extern struct inet_taskinfo browtask;
extern long browtask_wakes;
#endif
#ifdef TK_STDIN_DEVICE
extern struct inet_taskinfo keyboardtask;
extern long keyboard_wakes;
extern void kbdio(void);
#endif
#ifdef USE_SNTP_V4
extern struct inet_taskinfo sntpv4apptask;
#endif
#ifndef NO_INET_STACK
extern long netmain_wakes;
#endif   /* NO_INET_STACK */
#ifndef NO_INET_TICK
extern long nettick_wakes;
#endif   /* NO_INET_TICK */
#ifdef PING_APP
extern long pingcheck_wakes;
#endif   /* #ifdef PING_APP */
#ifdef TCP_ECHOTEST
extern long echotest_wakes;
#endif   /* TCP_ECHOTEST */
#ifdef TCP_CIPHERTEST
extern long ciphertest_wakes;
#endif   /* TCP_CIPHERTEST */
#ifdef IKE
extern struct inet_taskinfo iketask;
extern long ike_wakes;
#endif
#ifdef INCLUDE_SSLAPP
extern struct inet_taskinfo sslapp_task;
extern long sslapp_wakes;
#endif
/* per-application thread definitions */

int
create_apptasks(void)
{
int e = 0;
#ifdef USE_CRYPTOENG
   e = TK_NEWTASK(&cuexecutetask);
   if (e != 0)
   {
      dprintf("cuexecutetask create error\n");
      panic("create_apptasks");
      return -1;  /* compiler warnings */
   }
#endif
#ifdef WEBPORT
   e = TK_NEWTASK(&httptask);
   if (e != 0)
   {
      dprintf("httptask create error\n");
      panic("create_apptasks");
      return -1;  /* compiler warnings */
   }
#endif
#ifdef FTP_SERVER
   e = TK_NEWTASK(&ftpstask);
   if (e != 0)
   {
      dprintf("ftpstask create error\n");
      panic("create_apptasks");
      return -1;  /* compiler warnings */
   }
#endif
#ifdef FTP_CLIENT
   e = TK_NEWTASK(&ftpctask);
   if (e != 0)
   {
      dprintf("ftpctask create error\n");
      panic("create_apptasks");
      return -1;  /* compiler warnings */
   }
#endif
#ifdef SMTP_ALERTS
   e = TK_NEWTASK(&smtptask);
   if (e != 0)
   {
      dprintf("smtptask create error\n");
      panic("create_apptasks");
      return -1;  /* compiler warnings */
   }
#endif
#ifdef TELNET_SVR
   e = TK_NEWTASK(&telnettask);
   if (e != 0)
   {
      dprintf("telnettask create error\n");
      panic("create_apptasks");
      return -1;  /* compiler warnings */
   }
#endif
#ifdef INCLUDE_SNMP
   e = TK_NEWTASK(&snmptask);
   if (e != 0)
   {
      dprintf("snmptask create error\n");
      panic("create_apptasks");
      return -1;  /* compiler warnings */
   }
#endif
#ifdef DNS_SERVER
   e = TK_NEWTASK(&dnssrvtask);
   if (e != 0)
   {
      dprintf("dnssrvtask create error\n");
      panic("create_apptasks");
      return -1;  /* compiler warnings */
   }
#endif
#ifdef NICHVIEW
   e = TK_NEWTASK(&browtask);
   if (e != 0)
   {
      dprintf("browtask create error\n");
      panic("create_apptasks");
      return -1;  /* compiler warnings */
   }
#endif
#ifdef TK_STDIN_DEVICE
   e = TK_NEWTASK(&keyboardtask);
   if (e != 0)
   {
      dprintf("keyboardtask create error\n");
      panic("create_apptasks");
      return -1;  /* compiler warnings */
   }
#endif
#ifdef IKE
   e = TK_NEWTASK(&iketask);
   if (e != 0)
   {
      dprintf("IKE create error\n");
      panic("create_apptasks");
      return -1;  /* compiler warnings */
   }
#endif
#ifdef USE_SYSLOG_TASK
   e = TK_NEWTASK(&syslog_task);
   if(e != 0)
   {
      dprintf("syslog_task create error\n");
      panic("create_apptasks");
      return -1;  /* compiler warnings */
   }
#endif
#ifdef INCLUDE_SSLAPP
   e = TK_NEWTASK(&sslapp_task);
   if (e != 0)
   {
      dprintf("sslapp_task create error\n");
      panic("create_apptasks");
      return -1;  /* compiler warnings */
   }
#endif
#ifdef USE_SNTP_V4
   e = TK_NEWTASK(&sntpv4apptask);
   if(e != 0)
   {
      dprintf("SNTPv4 application task create error\n");
      panic("create_apptasks");
      return -1;
   }
#endif
/* 
 * Altera Niche Stack Nios port modification:
 * return error code, if any 
 */
   return e;
}

#ifdef TK_STDIN_DEVICE
extern   void  kbdio(void);
#endif

#ifdef TK_STDIN_DEVICE
TK_OBJECT(to_keyboard);
TK_ENTRY(tk_keyboard);
long     keyboard_wakes =  0;
#endif   /* TK_STDIN_DEVICE */

#ifdef TK_STDIN_DEVICE
struct inet_taskinfo keyboardtask = {
      &to_keyboard,
      "console",
      tk_keyboard,
      TK_KEYBOARD_TPRIO,
      IO_STACK_SIZE,
};
#endif


/* FUNCTION: TK_ENTRY()
 *
 * Task to hand keystrokes to InerNiche menu system
 * 
 * PARAM1: n/a
 *
 * RETURNS: n/a
 */

#ifdef TK_STDIN_DEVICE
TK_ENTRY(tk_keyboard)
{
   for (;;)
   {
      TK_SLEEP(1);   /* make keyboard yield some time */
      kbdio(); /* let Iniche menu routines poll for char */
      keyboard_wakes++; /* count wakeups */

      if (net_system_exit)
         break;
   }
   TK_RETURN_OK();
}
#endif   /* TK_STDIN_DEVICE */



/* FUNCTION: task_stats()
 * 
 * Print the "wake" statistics of all tasks.
 *
 * PARAM1: void * pio
 *
 * RETURNS: 
 */


void
task_stats(void * pio)
{
   ns_printf(pio, "Task wakeups:");

#ifndef NO_INET_STACK
   ns_printf(pio, "netmain: %lu\n", netmain_wakes);
#endif
#ifndef NO_INET_TICK
   ns_printf(pio, "nettick: %lu\n", nettick_wakes);
#endif
#ifdef TK_STDIN_DEVICE
   ns_printf(pio, "keyboard: %lu\n", keyboard_wakes);
#endif
#ifdef WEBPORT
   ns_printf(pio, "webport: %lu  ", webport_wakes);
#endif
#ifdef FTP_SERVER
   ns_printf(pio, "ftpsrv: %lu  ", ftpsrv_wakes);
#endif
#ifdef PING_APP
   ns_printf(pio, "pingcheck: %lu  ", pingcheck_wakes);
#endif
#ifdef TELNET_SVR
   ns_printf(pio, "telnetsrv: %lu  ", telnetsrv_wakes);
#endif
#ifdef USE_CRYPTOENG
   ns_printf(pio, "USE_CRYPTOENG: %lu  ", cuexecute_wakes);
#endif
#ifdef USE_SYSLOG_TASK
   ns_printf(pio, "USE_SYSLOG_TASK: %lu  ", syslog_wakes);
#endif
#ifdef SMTP_ALERTS
   ns_printf(pio, "smtpclient: %lu  ", emailer_wakes);
#endif
#ifdef SNMP_SOCKETS
   ns_printf(pio, "snmpsock: %lu  ", snmp_wakes);
#endif
#ifdef TCP_ECHOTEST
   ns_printf(pio, "echotest: %lu  ", echotest_wakes);
#endif
#ifdef TCP_CIPHERTEST
   ns_printf(pio, "ciphertest: %lu  ", ciphertest_wakes);
#endif
#ifdef USE_BROWSER
   ns_printf(pio, "browtask: %lu  ", browtask_wakes);
#endif
#ifdef INCLUDE_SSLAPP
   ns_printf(pio, "INCLUDE_SSLAPP: %lu  ", sslapp_wakes);
#endif
   ns_printf(pio, "\n");
}


#endif   /* SUPERLOOP - whole file can be ifdeffed out */


