/*
 * FILENAME: task.h
 *
 * Copyright 1999- 2000 By InterNiche Technologies Inc. All rights reserved
 *
 * This file contains definitions for the multi-tasking package.
 *
 * MODULE: TCP
 *
 *
 * PORTABLE: yes
 */

/* Additional Copyrights: */
/* Portions Copyright 1983 by the Massachusetts Institute of Technology */


#ifndef _TASK_H_
#define  _TASK_H_    1


/* Pattern written into the stack area */
#ifdef   SEG16_16
#define  GUARDWORD   0x5354      /* hex for ST */
#else
#define  GUARDWORD   0x53544143  /* hex for STAC */
#endif

typedef  int   stack_t;       /* type of data in task stacks */

/* NOTE: if you change this, be sure to change all the assembly which 
 * depends on it too!!!
 */
struct task
{ 
   struct task *  tk_next;    /* pointer to next task */
   stack_t *   tk_fp;         /* task's current frame ptr */
   char *      tk_name;       /* the task's name */
   int         tk_flags;      /* flag set if task is scheduled */
   unsigned long      tk_count;      /* number of wakeups */
   stack_t *   tk_guard;      /* pointer to lowest guardword */
   unsigned    tk_size;       /* stack size */
   stack_t *   tk_stack;      /* base of task's stack */
   void *      tk_event;      /* event to wake blocked task */
   unsigned long      tk_waketick;   /* tick to wake up sleeping task */
};

typedef struct task task;

/* tk_flags bits */
#define  TF_AWAKE       0x0001   /* task is ready to run */
#define  TF_MAIN        0x0002   /* task is using system stack */

extern   struct task *  tk_cur;  /* currently running task */

extern   unsigned TDEBUG;        /* tasking debugging */
extern   unsigned long   tk_wakeups;

/* entry points to tasking system */
task *   tk_init(stack_t * base, int st_size);
task *   tk_new(task*, int(*)(int), int, char*, int);    /* fork a new task */
void     tk_block(void);            /* switch to next runnable task */
void     tk_exit(void);             /* kill & delete current task */ 
void     tk_kill(task * tk_to_die); /* mark any task for death */
void     tk_wake(task * tk);        /* mark a task to run */
void     tk_sleep(long ticks);      /* sleep for number of ticks */
void     tk_ev_block(void * event); /* block until event occurs */
void     tk_ev_wake(void * event);  /* wake tasks waiting for event */


/* non-portable task utility routines */
stack_t * tk_frame(task *, int(*)(int), unsigned);
void      tk_switch(task *);        /* run the next task */
stack_t * tk_getsp(void);     /* get current stack pointer */

/* the yield() macro */
#define  tk_next()      {  tk_wake(tk_cur);  tk_block(); }

#endif   /* _TASK_H_ */


