/*
 * FILENAME: tk_crnos.c
 *
 * Copyright  2002 By InterNiche Technologies Inc. All rights reserved
 *
 * Wrapper and utility Functions to map NicheTask "TK_" macros to ChronOS
 *
 * MODULE: MISCLIB
 *
 * PORTABLE: yes (within ChronOS & uCOS systems)
 *
 * These wrapper functions for native ChronOS functions are dependant on the
 * the implemenation of the project/target directory osport.c file.
 *
 * Modified: 10-21-2008 (skh)
 */

#include "ipport.h"

#ifdef CHRONOS

#include "ucos_ii.h"
#include "os_cpu.h"

#include OSPORT_H

extern int num_net_tasks;
extern struct inet_taskinfo nettasks[];

 /* 
  * Q and Mutex used by tcp_sleep/wakeup
  */
extern OS_EVENT *global_wakeup_Mutex;
extern struct TCP_PendPost global_TCPwakeup_set[];
extern int global_TCPwakeup_setIndx;
extern u_long tcp_sleep_count;
extern u_long tcp_wakeup_count;

void     TK_OSTaskResume(u_char * Id);
void     TK_OSTimeDly(void);
u_char   TK_OSTaskQuery(void);



void TK_OSTimeDly(void)
{
   OSTimeDly(2);
}



void TK_OSTaskResume(u_char * Id)
{
INT8U err;

   err = OSTaskResume(*Id);
   
#ifdef NPDEBUG
   if ((err != OS_NO_ERR) && (err != OS_TASK_NOT_SUSPENDED))
   {
      dprintf("ChronOS API call failure, to Resume Suspended Task!\n");
      dtrap();
      panic("TK_OSTaskResume");      
   }
#endif
}



/* count of the number of times we timed-out waiting on a semaphore */
u_long tcp_sleep_timeout = 0;


/* FUNCTION: tck_sleep()
 *
 * Put a task to sleep until an event occurs.
 * On entry, the caller is holding the NET_RESID resource.
 *
 * PARAM1: void *             sleep event object
 *
 * RETURN: none
 */
void
tcp_sleep(void * event)
{
   int i;
   INT8U err;

   for (i = 0; i < GLOBWAKE_SZ; i++)
   {
      if (global_TCPwakeup_set[i].soc_event == NULL)
      {
         global_TCPwakeup_set[i].soc_event = event;
         global_TCPwakeup_set[i].ctick = cticks;
         if (i > global_TCPwakeup_setIndx)
            global_TCPwakeup_setIndx = i;

         tcp_sleep_count++;

         /* Give up the lock before going to sleep. This can
          * potentially cause a context switch to the task
          * signaling the event.
          */
         UNLOCK_NET_RESOURCE(NET_RESID);

         /* don't wait forever in case we miss the event */
         OSSemPend(global_TCPwakeup_set[i].semaphore, TPS, &err);
         if (err == 10)
         {
            ++tcp_sleep_timeout;

            /* clear the entry */
            global_TCPwakeup_set[i].ctick = 0;
            global_TCPwakeup_set[i].soc_event = NULL;
         }

         /* Regain the lock */
         LOCK_NET_RESOURCE(NET_RESID);
         return;
      }
   }

   /* The table is full. Try calling TK_YIELD() and hope for the best.
    * The user should increase the size of the table.
    * We'll record the max index for debugging purposes.
    */
   global_TCPwakeup_setIndx = i;

   UNLOCK_NET_RESOURCE(NET_RESID);
   TK_YIELD();
   LOCK_NET_RESOURCE(NET_RESID);
}



/* FUNCTION: tcp_wakeup()
 *
 * Wake up any tasks that might be waiting on this event.
 *
 * PARAM1: void *             wakeup event object
 *
 * RETURN: none
 */
void
tcp_wakeup(void *event)
{
   int i;

   OSSchedLock();

   for (i = 0; i < GLOBWAKE_SZ; i++)
   {
      if ((global_TCPwakeup_set[i].ctick != 0) &&
          (global_TCPwakeup_set[i].soc_event == event))
      {
         /* signal the event */
         OSSemPost(global_TCPwakeup_set[i].semaphore);

         /* clear the entry */
         global_TCPwakeup_set[i].ctick = 0;
         global_TCPwakeup_set[i].soc_event = NULL;

         tcp_wakeup_count++;
      }
   }

   OSSchedUnlock();
}



u_char TK_OSTaskQuery(void)
{
   OS_TCB task_data;
   INT8U err, task_prio;

   err = OSTaskQuery(OS_PRIO_SELF, &task_data);

   if (err == OS_NO_ERR)
   {
      task_prio = task_data.OSTCBPrio;
   }
   else
   {
      dprintf("ChronOS API call failure, unable to identify task!");
      panic("TK_OSTaskQuery");
      return 0;
   }
   
   return task_prio;
}



void
tk_yield(void)
{
   /* To ensure cycles to the lower priority tasks we should really
    * delay by two ticks, but that really hurts performance on some
    * long-tick targets. One tick works better overall....
    */
   OSTimeDly(1);
}


extern struct inet_taskinfo * nettask;
extern int num_net_tasks;

int
tk_stats(void * pio)
{
   int      t;    /* index into ChronOS TCB table */
   OS_TCB * tcb;  /* ChronOS Task Control Block */
   OS_STK * sp;   /* scratch stack pointer */
   int      stackuse;
   char     name[OS_TASK_NAME_SIZE+1];
   INT8U    err;
   

   ns_printf(pio, "ChronOS RTOS stats:\n");

#ifdef NO_INICHE_EXTENSIONS
   ns_printf(pio, "Context switches; Delay:  %lu\n",
      OSCtxSwCtr);
#else
   ns_printf(pio, "Context switches; Delay:  %lu, Interrupt: %lu\n",
      OSCtxSwCtr, OSCtxIntCtr);
#endif

   ns_printf(pio, "       name     prio. state    wakeups stack-size stack-use \n");

   
   for (t = 0; t <= OS_LOWEST_PRIO ; t++)
   {
      /* get pointer to TCB and see if entry is in use and not a mutex */
      tcb = OSTCBPrioTbl[t];
      if ((tcb == NULL) || (tcb == (OS_TCB *)1))
         continue;

      OSTaskNameGet(tcb->OSTCBPrio, (INT8U *)&name, &err);

#ifdef NO_INICHE_EXTENSIONS
      ns_printf(pio, "%15s %2d    0x%04x,    ---   ",
                     name, tcb->OSTCBPrio, tcb->OSTCBStat);
#else
      ns_printf(pio, "%15s %2d    0x%04x, %9ld,",
                     name, tcb->OSTCBPrio, tcb->OSTCBStat, tcb->wakeups);
#endif

      /* Find lowest non-zero value in stack so we can estimate the
       * unused portion. Subtracting this from size gives us the used
       * portion of the stack.
       */
#if OS_TASK_CREATE_EXT_EN > 0
      if(tcb->OSTCBStkBottom && tcb->OSTCBStkSize)
      {
         sp = tcb->OSTCBStkBottom + 1;
         while(*sp == 0)
            sp++;
         /* This OS traditionally keeps the size in OS_STK (int) units rather
          * than bytes, so convert back to bytes for display.
          */
         stackuse = (tcb->OSTCBStkSize - (sp - tcb->OSTCBStkBottom)) * sizeof(OS_STK);
         ns_printf(pio, "%6d,      %6d\n",
            tcb->OSTCBStkSize * sizeof(OS_STK),  stackuse);
      }
      else
#endif
      {
         ns_printf(pio, "No stack data\n");
      }
   }

   ns_printf(pio, "tcp_sleep_count = %lu, tcp_wakeup_count = %lu\n",
                  tcp_sleep_count, tcp_wakeup_count);
   ns_printf(pio, "global_TCPwakeup_setIndx = %d, tcp_sleep_timeout = %lu\n",
                  global_TCPwakeup_setIndx, tcp_sleep_timeout);

   return 0;
}


#endif /* CHRONOS */

