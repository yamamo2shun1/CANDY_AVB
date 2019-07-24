/*
 * FILENAME: intimers.c
 *
 * Copyright  2001 By InterNiche Technologies Inc. All rights reserved
 *
 * Handles InterNiche task & application interval timers.
 *
 * MODULE: MISCLIB
 *
 * PORTABLE: yes
 */

#ifndef _INTIMERS_H
#define _INTIMERS_H  1

/* interval timer structure */
struct intimer
{
   void     (*callback)(long parm);
   long     parm;
   u_long   interval;   /* fire interval, in TPS units */
   u_long   tmo;        /* ctick to fire */
   int      inuse;      /* reentrancy flag */
};

/* MAX number of interval timers in the system */
#ifndef NUM_INTIMERS       /* Allow override from ipport.h */
#define NUM_INTIMERS 5
#endif

extern struct intimer intimers[NUM_INTIMERS];   /* the timers */

/* entry points to interval timer package */
long  in_timerset(void (*callback)(long), long msecs, long parm);
int   in_timerkill(long timer);

#endif /* _INTIMERS_H */



