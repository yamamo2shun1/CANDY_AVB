/*
 * FILENAME: in_utils.h
 *
 * Copyright 1997- 2000 By InterNiche Technologies Inc. All rights reserved
 *
 *
 * MODULE: TCP
 *
 *
 * PORTABLE: yes
 */

/* in_utils.h 03/11/97 - Created for Borland C cleanup. John Bartas 
 */
#ifndef IN_UTILS_H
#define  IN_UTILS_H     1

#include "ipport.h"
#include "libport.h"

#ifndef MAXIOSIZE
#define MAXIOSIZE       156
#endif

struct GenericIO
{
   char *   inbuf;      /* Pointer to command line  */

   /* Function to send the output string  */
   int (* out)(long id, char * outbuf, int len);  

   /* Identifier for the IO device. For TCP it would represent a SOCKET */
   long id;

   /* Get a character input from the I/O device This is needed to
    * show scrollable items
    */
   int (*getch)(long id);
};

typedef struct GenericIO * GEN_IO ;
extern   unsigned long  pingdelay;



#ifdef SEG16_16
/* macros for 16 bit seg:offset pointer manipulation */
#ifndef _MK_FP
#define _MK_FP(seg, offs) (void far *)(((unsigned long)seg << 16) \
    + (unsigned long)(unsigned)offs)
#endif
#ifndef _FP_SEG
#define  _FP_SEG(ptr)   (*((unsigned   far *)&(ptr)+1))
#endif
#ifndef _FP_OFF
#define  _FP_OFF(ptr)   (*((unsigned   far *)&(ptr)))
#endif
#else /* not Intel 16-bit mode */
#define  far   /* define to nothing */
#endif   /* SEG16_16 */

/* misc prototypes for TCP/IP demo system.*/
extern   void  hexdump(void * pio, void * buffer, unsigned len);
extern   char* nextcarg(char * args);
extern   int   std_out(long s, char * buf, int len);
extern   int   std_in(long s);
extern   int   con_page(void * pio, int line);

#ifdef MINI_IP
/* MINI stack does not support gracefull net shutdown. If the port
 * has not defined this call in ipport.h, then map it to exit ().
 */
#ifndef netexit
#define netexit(err) exit(err);
#endif /* netexit */
 
#else  /* not MINI_IP */
extern   void  netexit(int err);
#endif

/* Sometimes ns_printf can be #defined to dprintf, etc. In that case
 * skip the standard declaration.
 */

#ifndef ns_printf
extern   int   ns_printf(void * pio, char * format, ...);
#endif

/* InterNiche alloc/free entry points */
int   mh_stats(void * IO);
void  mheap_init(char * base, long size);
char* calloc1(unsigned size);
void  mem_free(char HUGE * buf);


#ifdef INICHE_LIBS
/* prototypes for routines in strlib.c file: */
int   strlen(char * str);
char* strcpy(char * str1, char * str2);
char* strncpy(char * str1, char * str2, int max);
int   strcmp(char * str1, char * str2);
char* strcat(char * str1, char * str2);
char* strchr(char * str, char chr);
int   strncmp(char * str1, char * str2, int length);
char * strstr(char * str1, char * str2);

int   atoi(char *);
#else /* not INICHE_LIBS, use compilers include files */
#include "string.h"
#endif   /* INICHE_LIBS */

/* prototypes for routines in strilib.c file: */
#ifdef INICHE_STRICMP
int   stricmp(const char * s1, const char * s2);
#endif   /* INICHE_STRICMP */
#ifdef INICHE_STRNICMP
int   strnicmp(const char * s1, const char * s2, int len);
#endif   /* INICHE_STRNICMP */

#ifdef INICHE_STRISTR
char* stristr(char * str1, char * str2);
#endif

#ifdef __linux__
#define FLUSH_STDOUT fflush(stdout);
#else
#define FLUSH_STDOUT
#endif

#endif   /* IN_UTILS_H */

