/* 
 * File name : osport.h
 *
 * Map InterNiche tasking primitives to ChronOS or set up NicheTask.
 *
 * This file for:
 *   ALTERA Cyclone Dev board with the ALTERA Nios2 Core.
 *   SMSC91C11 10/100 Ethernet Controller
 *   GNU C Compiler provided by ALTERA Quartus Toolset.
 *   Quartus HAL BSP
 *   uCOS-II RTOS Rel 2.76 as distributed by Altera/NIOS2
 *
 * 06/21/2004
 * 
 */

/* We don't guard osport.h with OSPORT_H on purpose -AK- */

#ifndef osport_h
#define osport_h

#include "ipport.h"

#ifdef UCOS_II
#include "includes.h"
#endif /* UCOS_II */

#include "q.h"
#include "netbuf.h"
#include "net.h"
#include "arp.h"
#include "ip.h"
#include "icmp.h"
#include "udp.h"
#include "nvparms.h"
#include "nvfsio.h"
#include "menu.h"
#include "app_ping.h"

#include "task.h"

// These definitions came in the standard port of the InterNiche sources
// They are not used in the Nios II port
#if 0

#define  DEMO_STACK_SIZE      4096

#define  NETHEAP_SIZE         0x30000

/* task stack sizes */
#define  NET_STACK_SIZE       4096
#define  APP_STACK_SIZE       6144 /* 4096 */     /* default for applications */
#define  CLOCK_STACK_SIZE     4096

#define  IO_STACK_SIZE        2048
#define  WEB_STACK_SIZE       APP_STACK_SIZE
#define  FTP_STACK_SIZE       APP_STACK_SIZE
#define  PING_STACK_SIZE      4096
#define  TN_STACK_SIZE        APP_STACK_SIZE
#define  TCP_ECHO_STACK_SIZE  APP_STACK_SIZE

#endif

// Note: These definitions below have moved into the ipport.h file
#if 0

#define  DEMO_STACK_SIZE      (4096+8192)

#define  NETHEAP_SIZE         0x30000

/* task stack sizes */
#define  NET_STACK_SIZE       (4096+8192)
#define  APP_STACK_SIZE       (6144+8192) /* 4096 */     /* default for applications */
#define  CLOCK_STACK_SIZE     (4096+8192)

#define  IO_STACK_SIZE        (2048+8192)
#define  WEB_STACK_SIZE       (APP_STACK_SIZE+8192)
#define  FTP_STACK_SIZE       APP_STACK_SIZE
#define  PING_STACK_SIZE      (4096+8192)
#define  TN_STACK_SIZE        APP_STACK_SIZE
#define  TCP_ECHO_STACK_SIZE  APP_STACK_SIZE

#endif

#ifdef INICHE_TASKS
#define TK_ENTRY(name)       int name(int parm)
#define TK_ENTRY_PTR(name)   int(*name)(int)
#define TK_OBJECT(name)      task * name
#define TK_OBJECT_PTR(name)  task ** name
#define TK_RETURN_ERROR()    return (-1)
#define TK_RETURN_OK()       parm++; return (0)
#define NET_PRIORITY         0     /* not used on tasks */
#define MAIN_TASK_IS_NET     1     /* compiler main() is converted to net task */
#define TK_NETMAINPARM       0     /* parameter to main/net task */
#endif

/* table with an entry for each internet task/thread. This is filled
in the netmain.c, so it should have the same values in the same order 
in all ports */

struct inet_taskinfo {
   TK_OBJECT_PTR(tk_ptr);  /* pointer to static task object */
   char * name;            /* name of task */
   TK_ENTRY_PTR(entry);    /* pointer to code to start task at */
   int   priority;         /* priority of task */
   int   stacksize;        /* size (bytes) of task's stack */
   char* stackbase;        /* base of task's stack */
};

#ifdef ALT_INICHE
extern void alt_iniche_init(void);
#endif /* ALT_INICHE */

int TK_NEWTASK(struct inet_taskinfo * nettask);

extern char * pre_task_setup(void);
extern char * post_task_setup(void);

extern int netmain(void);
extern TK_OBJECT(to_netmain);

extern int num_net_tasks;
extern struct inet_taskinfo nettasks[];

extern void dtrap(void);

/* UCOS_II pend/post */
#undef TK_CRON_DIAGS
#define  GLOBWAKE_SZ     20
#define  GLOBWAKE_PURGE_DELT     (TPS * 2)
 
struct wake_event
{
   void *wake_sem;
   void *soc_event;        
   char *name;
};

struct TCP_PendPost
{
   u_long   ctick;           /* time entry was added */
   void     *soc_event;      /* wakeup event */	
   OS_EVENT *semaphore;      /* semaphore to wait on */
};
 


#ifdef MINI_IP
#define net_system_exit (FALSE)
#else
extern int net_system_exit;   /* TRUE if system is shutting down */
#endif /* MINI_IP */

extern int iniche_net_ready;

#endif /* osport_h */
