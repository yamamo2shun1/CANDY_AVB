/*
 * FILENAME: tk_crnos.h
 *
 * Copyright  2001-2002 By InterNiche Technologies Inc. All rights reserved
 *
 * Definitions to map NicheTask "TK_" macros to ChronOS/uCOS RTOS
 *
 * MODULE: MISCLIB
 *
 * PORTABLE: yes (within uCOS systems)
 */

#ifndef TK_CRNOS_H
#define TK_CRNOS_H

#ifndef CHRONOS
#error - must define CHRONOS in ipport.h to use this file
#endif

extern void TK_OSTaskResume(u_char * Id);

/* define the default priority for all net related tasks */
#define  NET_PRIORITY   0  /* not used on ChronOS */

#ifdef NOT_DEF

/* 
 * Altera Niche Stack Nios port modification:
 * OS_PRIO_SELF defined in ucosii.h, as is OS_EVENT
 */

/* Self-ID token for pass to ChronOS primitives */
#ifdef ALT_INICHE
#include "ucos_ii.h"
#else
#define OS_PRIO_SELF 0xFF
#endif /* ALT_INICHE */

#endif

/* Define the CHRONOS object control TK_ macros...  */

/* macros for task type, entry, and name */
#define  TK_ENTRY(name)       void  name(void * parm)
#define  TK_OBJECT(name)      u_char   name
#define  TK_OBJECT_PTR(name)  u_char * name
#define  TK_OBJECT_REF(name)  TK_OBJECT(name)
#define  TK_ENTRY_PTR(name)   void(*name)(void*)

#define  TK_THIS              TK_OSTaskQuery()

#ifndef  TK_RETURN_ERROR
#define  TK_RETURN_ERROR()   return
#endif
#ifndef  TK_RETURN_OK
#define  TK_RETURN_OK()      return
#endif


/*
 * synchronization primitives
 * 
 * Need one or the other of LOCK_NET_RESOURCE/UNLOCK_NET_RESOURCE and
 * ENTER_CRIT_SECTION/EXIT_CRIT_SECTION.  These should be implementable
 * as either functions or macros, so we allow e.g. ipport.h_h to 
 * override these definitions by defining the names as macros.
 * CHRONOS targets generally use LOCK_NET_RESOURCE/UNLOCK_NET_RESOURCE
 * so we default to function declaractions for them.
 */
extern void LOCK_NET_RESOURCE(int res);
extern void UNLOCK_NET_RESOURCE(int res);
extern void irq_Mask(void);
extern void irq_Unmask(void);

#define  ENTER_CRIT_SECTION(p)      irq_Mask()
#define  EXIT_CRIT_SECTION(p)       irq_Unmask()

#ifdef NOTDEF
/* set up pointers to CHRONOS semaphores as void * so we don't
 * have to include ChronOS includes in every file in the build. In Addition
 * RXQ_RESID and FREEQ_RESID are not real semaphores, but just placeholders.
 * See the LOCK_NET_RESOURCE() code for details.
 */
#ifndef NET_RESID
extern   void *      net_task_sem_ptr;
#define  NET_RESID   (net_task_sem_ptr)
#endif
#ifndef PINGQ_RESID
extern   void *      pingq_sem_ptr;
#define  PINGQ_RESID  (pingq_sem_ptr)
#endif
#ifndef FTPCQ_RESID
extern   void *      ftpcq_sem_ptr;
#define  FTPCQ_RESID  (ftpcq_sem_ptr)
#endif
#ifndef RXQ_RESID
extern   void *      receiveq_sem_ptr;
#define  RXQ_RESID   (receiveq_sem_ptr)
#endif
#ifndef FREEQ_RESID
extern   void *      freeq_sem_ptr;
#define  FREEQ_RESID (freeq_sem_ptr)
#endif

#endif


/* declare tasks which may need to be accessed by system code */
extern   TK_OBJECT(to_pingcheck);
extern   TK_OBJECT(to_netmain);
extern   TK_OBJECT(to_emailer);
extern   TK_OBJECT(to_ftpclnt);


/* map TK_ macros to CHRONOS: */
#define  TK_BLOCK()          OSTaskSuspend(OS_PRIO_SELF)
extern OS_EVENT *rcvdq_sem_ptr;
#define  TK_NETRX_BLOCK()    { \
	                       INT8U err; \
	                       OSSemPend(rcvdq_sem_ptr, TPS, &err); \
	                       if ((err != OS_NO_ERR) && (err != OS_TIMEOUT)) \
                                  dtrap(); \
	                     }

#define  TK_SLEEP(count)     OSTimeDly(count + 1)

/* (Id) is always of type TK_OBJECT_PTR */
#define  TK_WAKE(Id)         OSTaskResume(*(u_char*)(Id))

/* (ev) is always of type TK_OBJECT_REF */
#define  TK_WAKE_EVENT(ev)   OSTaskResume((u_char)(ev))

/* Do tk_yield() as a function to avoid pointless delays */
void tk_yield(void);
#define TK_YIELD()           tk_yield()


/* For ChronOS SignalPktDemux() is a routine which manually sets the net
 * task to runnable. This is so we can call it from an ISR and not have
 * ChronOS enable interrupts on us.
 */
#define  SignalPktDemux()     OSSemPost(rcvdq_sem_ptr)
#define  SignalEmailTask()    OSTaskResume(to_emailer)
#define  SignalFtpClient()    OSTaskResume(to_ftpclnt)

#endif  /* TK_CRNOS_H */

