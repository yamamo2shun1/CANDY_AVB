/*
 * FILENAME: profiler.h
 *
 * Copyright  2001 By InterNiche Technologies Inc. All rights reserved
 *
 * MODULE: MISCLIB
 *
 *
 * PORTABLE: no
 */

/* profiler.h Profile code for portions of the InterNiche TCP/IP 
 * stack and related modules. 
 */
#ifndef _PROFILER_H_
#define  _PROFILER_H_   1


/* one of these structures is defined for every module to be profiled.
 * They kept in an array and referenced by their "id" field.
 * Additional pbuckets can be declared and added to the array
 * at init time.
 */
struct pbucket
{
   char *   name;             /* text name of module */
   u_long   ticks;
   struct pbucket ** id;      /* self reference for error checking */
};

/* Declare the maximum number of profiling buckets */
#ifndef NUM_PBUCKETS       /* allow override from ipport.h */
#define  NUM_PBUCKETS   15
#endif

/* The actual array of profiling buckets */
extern   struct pbucket in_pbuckets[NUM_PBUCKETS];

extern   int   pfmodules;     /* current number of active pbuckets */


/* Some predefined struct pbucket IDs for InterNiche modules. The IDs
 * are addresses of the modules' "bucket" structures.
 */

extern struct pbucket * PF_IP;
extern struct pbucket * PF_USUM;
extern struct pbucket * PF_IDLE;
extern struct pbucket * PF_TCP;
extern struct pbucket * PF_TSUM;
extern struct pbucket * PF_WEB ;
extern struct pbucket * PF_FTP;
extern struct pbucket * PF_FS;
extern struct pbucket * PF_NAT;
extern struct pbucket * PF_IPSEC;
extern struct pbucket * PF_CRYPTOENG;

/* opcodes for pf_ticklog() */
#define  PF_ENTRY    1
#define  PF_EXIT     2


/* profiler system start & stop routines. This should be called either
 * by application code or fome the profiler menu to bracket in time
 * the software events to be profiled. NOTE: These do not accumulate -
 * calling pf_start() wipes out all previous profile data.
 */
void  pf_start(void);
void  pf_stop(void);

/* This is called by the monitored code modules whenever they are
 * entered or exited. See IN_PROFILER() macro.
 */
void  pf_ticklog(struct pbucket * id, int opcode);

/* this is called to add a new module to profiling. The returned value is 
 * the module ID which should be passed to IN_PROFILER() to begin/exit
 * profiling the module. The passed name should be static text.
 */

void *   pf_addmodule(char * name);


/* This port-provided routine should return a fast tick for profiling.
 * Million ticks a sec or so is good. cticks is too slow to be suitable
 * for this.
 */
extern   u_long   get_ptick(void);


/* helper utility for calculating ticks percentages */
void  pf_percent(u_long total, u_long ticks, int * pct, int * dec);


/* To use the CPU cycles per packet (or connection) functions below, you
 * must set "#define PKT_CYCLES 1" in ipport.h_h
 */
extern   int   pkc_start(void * pio);
extern   int   pkc_del(void * pio);
extern   int   pkc_ping(void * pio);
extern   int   pkc_tcpecho(void * pio);
extern   int   pkc_udpecho(void * pio);
extern   int   pkc_calibrate(void * pio);
extern   int   pkc_mode(void * pio);
extern   int   pkc_data(void * pio);

/* Macro so net sources can use profiler (or not) without littering the
 * source code with ifdefs:
 */
#ifdef USE_PROFILER
#define  IN_PROFILER(id, opcode)  pf_ticklog((struct pbucket *)id, opcode)
#else
#define  IN_PROFILER(id,  opcode)  /* define away */
#endif   /* USE_PROFILER */

#endif   /*  _PROFILER_H_ */


