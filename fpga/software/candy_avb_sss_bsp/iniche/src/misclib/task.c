/*
 * FILENAME: task.c
 *
 * Copyright 1999- 2000 By InterNiche Technologies Inc. All rights reserved
 *
 *
 * MODULE: MISCLIB
 *
 * ROUTINES: tk_init(), tk_new(), int    (), tk_block (), 
 * ROUTINES: tk_exit(), tk_kill(), tk_stats(), tk_del(), tk_wake(), 
 * ROUTINES: tk_sleep(), tk_ev_block(), tk_ev_wake(),
 *
 * PORTABLE: yes
 */

/* Additional Copyrights: */
/* Portions Copyright 1984 by the Massachusetts Institute of Technology */

#include   "ipport.h"

#ifdef INICHE_TASKS  /* whole file can be ifdeffed out */

#include   "task.h"
#include   "libport.h"
#include   "profiler.h"

task * tk_cur;                /* a pointer to the current task */
unsigned TDEBUG = 0;          /* DEBUG flag */
u_long   tk_wakeups  =  0;    /* count fo task switches */
u_long   tk_allocfail = 0;    /* number of resource allocation failures */

static task * tk_died = NULL;

void  tk_del(task * tk);   /* remove tasks resources */

/* FUNCTION: tk_init()
 * 
 * tk_init() - Initialize the tasking system. Create the first task, 
 * and set its stack pointer to the main program stack. The first 
 * task always uses the main system startup stack for compatability 
 * with compiler provided "startup" files. The circular list of tasks 
 * contains only the original task, so originally set its next task 
 * pointer to point to itself. This routine returns to the caller 
 * with a pointer to the first task, or error it returns NULL. 
 *
 * PARAM1: stack_t * st_base
 * PARAM2: int st_size
 *
 * RETURNS: 
 */

task *
tk_init(stack_t * st_base, int st_size)
{
   task *tk;      /* pointer to the first task */
   stack_t * sp;

   /* create the first task */
   tk = (task*)TK_ALLOC(sizeof(struct task));

   /* return immediately if task structure could not be
    * allocated.  The caller must handle this scenario.
    */
   if (!tk) 
   {
      ++tk_allocfail;
      return(tk);
   }

   tk_cur = tk;      /* It is the currently running task */
   /* Task is running thus does not need to be awakened. */
   tk->tk_flags = TF_MAIN; /* mark it as using system stack */
   tk->tk_name = "Main";
   tk->tk_next = tk;       /* task is only one in list */
   tk->tk_count = 0;
   tk->tk_size = st_size;
   tk->tk_stack = st_base;

   /* fill the system stack with the guardword */
   sp = tk_getsp();
#ifdef STK_TOPDOWN   /* Intel x86 "top down" stack */
   while (sp > st_base)
   {
      sp--; /* stop when sp == base */
      *sp = GUARDWORD;
   }
#elif STK_BOTTOMUP   /* stack moves from bottom up */
   {  stack_t * stacktop = st_base + st_size/sizeof(stack_t);
      while (sp < stacktop)
      {
         sp++;
         *sp = GUARDWORD;
      }
   }
#else
#error You must define a stack type! (STK_TOPDOWN or STK_BOTTOMUP)
#endif

   tk->tk_guard = sp;   /* set address for stack check */

#ifdef   DEBUG
   if (TDEBUG)
      dprintf("tk_init: Initial stack size = %d\n", tk->tk_size);
#endif

   return(tk);
}


/* FUNCTION: tk_new()
 *
 * Create a new task with stksiz bytes of stack, and place 
 * it in the circular list of tasks after prev_tk. Awaken it so that 
 * it will run, and set it up so that when it does run, it will start 
 * runing routine start. This routine does not affect the execution 
 * of the currently running task. Returns a pointer to the new task, 
 * or NULL if there isn't enough memory for the new task 
 *
 * 
 * PARAM1: task * prev_tk
 * PARAM2: task code routine
 * PARAM3: size of stack
 * PARAM4: name for new task
 * PARAM5: arg for task (deprecated)
 *
 * RETURNS: 
 */

task * 
tk_new(task * prev_tk,     /* predecessor (sp?) to the new task */
   int    (*start)(int),   /* Where the new task starts execution. */
   int      stksiz,  /* The size in bytes of the stack of the new task. */
   char   * name,    /* The task's name as a string */
   int      arg)     /* argument to the task */
{
   task * tk;     /* a pointer to the new task */
   stack_t * sp;  /* pointer to task stack area for fill */

   /* create the new task */
   tk = (task *)TK_ALLOC(sizeof(task));
   if (tk == NULL)
   {
      ++tk_allocfail;
      return NULL;        /* if no memory left, return null */
   }
   tk->tk_size = stksiz;
   tk->tk_stack = (stack_t *)STK_ALLOC(stksiz);
   if (tk->tk_stack == NULL)
   {
      ++tk_allocfail;
      TK_FREE(tk);
      return NULL;
   }

   /* set it up to run */
   tk->tk_fp = tk_frame (tk, start, arg);

   /* fill stack with guardword */
   sp = tk->tk_fp;

#ifdef STK_TOPDOWN   /* Intel x86 "top down" stack */
   sp--;
   while (sp > tk->tk_stack)
   {
      sp--;
      *sp = GUARDWORD;
   }
#else /* stack moves from bottom up */
   dtrap(); /* untested */
   x = stksiz - (sp - tk->tk_stack);   /* bytes to fill */
   x = x/(sizeof(stack_t));
   x++;
   sp++;
   while (x++ > 0)
      *sp++ = GUARDWORD;   
#endif

   tk->tk_guard = sp;
   tk->tk_flags |= TF_AWAKE;  /* Schedule the task to run. */
   tk->tk_next = prev_tk->tk_next;  /* Fit it in after prev_tk. */
   prev_tk->tk_next = tk;
   tk->tk_name = name;        /* Set its name */
   tk->tk_count = 0;          /* init the count */

   return (tk);
}



/* FUNCTION: tk_block ()
 *
 * Block the currently running task and run the next task in the 
 * circular list of tasks that is awake. Before returning, see if any 
 * cleanup has to be done for another task. 
 *
 * 
 * PARAM1: 
 *
 * RETURNS: 
 */

void
tk_block () 
{
   char msg[255];
   task * tk = tk_cur;     /* the next task to run */

   /* check if the guard word is still intact */
   if (*(tk->tk_guard) != GUARDWORD)
   {
      sprintf(msg, "tk_block stack: task name = %s", tk->tk_name);
      panic(msg);
      return;
   }

#ifdef NPDEBUG
   if (TDEBUG)
      dprintf("TASK: Task %s blocking.\n", tk->tk_name);
#endif

   IN_PROFILER(PF_IDLE, PF_ENTRY);      /* measure time in the idle loop */

   /* find the next ready to run task */
   do 
   {
      tk = tk->tk_next;
      /* see if a sleeping task is ready to wake */
      if (tk->tk_waketick && tk->tk_waketick < cticks)
      {
         tk->tk_waketick = 0; /* clear wakeup tick flag */
         break;   /* go run this task */
      }
   } while ((tk->tk_flags & TF_AWAKE) == 0);

   IN_PROFILER(PF_IDLE, PF_EXIT);

   tk->tk_flags &= ~TF_AWAKE;    /* clear wake flag before it runs */
   tk_switch (tk);      /* Run the next task. */

#ifdef NPDEBUG
   if (TDEBUG)
      dprintf("TASK: Task %s running\n", tk_cur->tk_name);
#endif

   if (tk_died) 
   {
      tk_del(tk_died);  /* delete dead task */
      tk_died = NULL;   /* clear pointer/flag */
   }
}



/* FUNCTION: tk_exit()
 *
 * tk_exit() : destroy the current task. Accomplished by setting a 
 * flag indicating that an exit has occured and then entering the 
 * scheduler loop. When tk_block() returns for some other task and 
 * finds this flag set, it deallocates the task which exited. This is 
 * necessary because we still need a stack to run on. The task 
 * removes itself from the circular list of tasks in the system so 
 * that it cannot be awoken after it has exited. Otherwise, the exit 
 * might be done in the context of the task itself, which would prove 
 * disastrous. This routine never returns. 
 *
 * 
 * PARAM1: void
 *
 * RETURNS: no
 */

void
tk_exit() 
{
   tk_kill(tk_cur);  /* kill current task, save in tk_to_die */
   tk_block();       /* this should delete tk_to_die */
   panic("tk_exit"); /* should never return from block */
}


/* FUNCTION: tk_kill()
 *
 * tk_kill(task) - kill a task. A special case is if you're killing 
 * yourself, you die the instant your task blocks. 
 *
 * 
 * PARAM1: task * tk - task to die
 *
 * RETURNS: void
 */

void
tk_kill(task * tk_to_die)
{
   task *tk;

   if(tk_died)       /* Is another task in the process of dying? */
   {
      tk_yield();    /* this should allow it to finish */
      if(tk_died)    /* Other task still dying? */
      {
         dtrap();    /* system bug */
      }
   }
   /* hunt for the task which tk_to_die is the successor of */
   for (tk = tk_cur; tk->tk_next != tk_to_die; tk = tk->tk_next) ;

   /* now patch around tk_to_die */
   tk->tk_next = tk_to_die->tk_next;

   /* If I'm killing myself, die the next time I block */
   if (tk_cur == tk_to_die) 
   {
      tk_died = tk_cur;
   }
   else
      tk_del(tk_to_die);
}


/* FUNCTION: tk_stats()
 *
 * tk_stats() - print out tasking system status
 * 
 * PARAM1: void * pio
 *
 * RETURNS: 
 */

int
tk_stats(void * pio) 
{
#ifdef NET_STATS
   task *tk;
   char *   state;

   ns_printf(pio, "tasking status:");
   ns_printf(pio, "task wakeups: %D\n", tk_wakeups);
   ns_printf(pio, "   name          \tstate   \tstack\tused\twakes\n");

   tk = tk_cur;
   do    /* measure stack usage using guardword pattern */
   {
      stack_t * up;
      int   i; /* number of unused words in stack area */

      up = tk->tk_guard;
#ifdef STK_TOPDOWN
      for (i = tk->tk_size/sizeof(stack_t) -1; i; i--)
         if (*up++ != GUARDWORD) break;
#else /* BOTTOMUP */
      for (i = tk->tk_size/sizeof(stack_t) -1; i; i--)
         if (*up-- != GUARDWORD) break;
#endif

      if (tk == tk_cur)
         state = "running ";
      else if(tk->tk_event)
         state = "event_wt";
      else if(tk->tk_waketick)
         state = "sleeping";
      else if(tk->tk_flags & TF_AWAKE)
         state = "ready   ";
      else
         state = "blocked ";

      ns_printf(pio, "%-18s\t%s\t%u\t%u\t%lu\n", 
       tk->tk_name, state,
       tk->tk_size, i*sizeof(stack_t), tk->tk_count);
      tk = tk->tk_next;
   } while (tk != tk_cur);
#else
   USE_VOID(pio);
#endif
   return 0;
}



/* FUNCTION: tk_del()
 * 
 * PARAM1: task * tk
 *
 * RETURNS: 
 */

void
tk_del(task * tk)
{
   if (tk_cur == tk) /* don't delete running task! */
      panic("tk_del");
   /* free stack if it's not system stack */
   if ((tk->tk_flags & TF_MAIN) == 0)
      STK_FREE(tk->tk_stack);
   TK_FREE(tk);
}



/* FUNCTION: tk_wake()
 * 
 * PARAM1: task * tk
 *
 * RETURNS: 
 */

void
tk_wake(task * tk)
{
   tk->tk_flags |= TF_AWAKE;
   tk_wakeups++;
   tk->tk_count++;
}


/* FUNCTION: tk_sleep()
 *
 * tk_sleep()- put current task to sleep for passed tick count.
 *
 * 
 * PARAM1: long ticks
 *
 * RETURNS: 
 */

void
tk_sleep(long ticks)
{
   tk_cur->tk_flags &= ~TF_AWAKE;   /* put task to sleep */
   tk_cur->tk_waketick = cticks + ticks;  /* set wake time */
   tk_block();
   tk_wakeups++;
   tk_cur->tk_count++;
}


/* FUNCTION: tk_ev_block()
 *
 * tk_ev_block() - block current task until an event occurs. The 
 * event is signaled by calling tk_ev_wake() with a pointer match the 
 * one passed here. 
 *
 * 
 * PARAM1: void * event
 *
 * RETURNS: 
 */

void
tk_ev_block(void * event)
{
   tk_cur->tk_flags &= ~TF_AWAKE;   /* put task to sleep */
   tk_cur->tk_event = event;        /* set wake event */
   tk_block();                      /* wait for tk_ev_wake() */
   tk_wakeups++;
   tk_cur->tk_count++;
}


/* FUNCTION: tk_ev_wake()
 *
 * tk_ev_wake() - wake any tasks sleeping on the passed event. This 
 * does not wake up tk_cur. 
 *
 * 
 * PARAM1: void * event
 *
 * RETURNS: 
 */

void
tk_ev_wake(void * event)
{
   task * tk;
   /* look though task list - skip tk_cur */
   for (tk = tk_cur->tk_next; tk != tk_cur; tk = tk->tk_next)
   {
      if (tk->tk_event == event) /* is task wait for this event */
      {
         tk->tk_event = NULL;    /* clear the event */
         tk->tk_flags |= TF_AWAKE;  /* wake the task */
      }
   }
}

#endif   /* INICHE_TASKS -  whole file can be ifdeffed out */


