/*
 * Filename: libport.h
 *
 * Copyright 2004 by InterNiche Technologies.
 *
 * All the platform specific defines for each port go here.
 * All these should be re-examined with each new port of the code.
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

#ifndef _LIBPORT_H_
#define _LIBPORT_H_   1

#ifndef INICHE_LIBS   /* use Standard C libs; strlib (strcat, strcpy, etc) */
#include <stdlib.h>   /* for atoi(), exit() */
#include <string.h>
#endif
#ifdef PRINTF_STDARG
#include <stdarg.h>
#endif 

/* options specific to the utilities library */

/* enabled options */
/*
 * Altera Niche Stack modifcation 12-Feb-2008 (jrk):
 * These are provided in newlib. Removing.
 */
//#define INICHE_STRICMP  1     /* no stricmp in C library */
//#define INICHE_STRNICMP 1     /* no strnicmp in C library */
//#define INICHE_STRISTR  1     /* no stristr in C library */

#ifdef NOTDEF
#define INCLUDE_FLASHFS 1     /* NV params read from flash */
#endif

#ifdef INCLUDE_FLASHFS
#define FLASHBASE 0x70000000  /* memory address to build FFS on net186 */
#define FLASHFIRM 0xD0000000  /* base of flash to store FFS net186 */
#define NUMNVFILES 8          /* numnber of flash files allowed */
#define NVFSIZE   (1024*4)    /* reserved size for each flash file */
#endif

/* misc prototypes for TCP/IP misclib port */
void	panic(char * msg);
#ifdef PRINTF_STDARG
void  doprint(char * target, char * sp, va_list va);
#else  /* PRINTF_STDARG */
void  doprint(char * target, char * sp, int * vp);
#endif /* PRINTF_STDARG */

/* map memory routines to standard lib */
#define MEMCPY(dst,src,size)  memcpy((void *)(dst),(void *)(src),(size))
#define MEMMOVE(dst,src,size) memmove((void *)(dst),(void *)(src),(size))
#define MEMSET(ptr,val,size)  memset((void *)(ptr),(val),(size))
#define MEMCMP(p1,p2,size)    memcmp((void *)(p1),(void *)(p2),(size))

#ifndef isdigit
#define isdigit(_c)  ((_c >= '0' && _c <= '9') ? 1:0)
#endif

#endif /* _LIBPORT_H_ */

