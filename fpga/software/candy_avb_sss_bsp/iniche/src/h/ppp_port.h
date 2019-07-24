/*
 * Filename: ppp_port.h
 *
 * Copyright 2004 by InterNiche Technologies Inc. All rights reserved.
 *
 * NetPort PPP layer's per-port definitions. The portions of the PPP 
 * stack which may need to be modified for new ports are placed in 
 * this file.
 *
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

#ifndef _PPP_PORT_H
#define _PPP_PORT_H 1

#include "ipport.h"  /* NetPort IP Stack definitions */

#include "libport.h"    /* misclib option switches */
#include "in_utils.h"   /* for resolving references to GEN_IO */

#ifdef IN_MENUS
#include "menu.h"
#endif   /* IN_MENUS */

#ifdef USE_PPP       /* whole file can be ifdeffed out */

/* PPP options included in this port: */
#define LOCAL_RAND   1  /* use random number generator in magic.c */

#ifdef NOTDEF
Unused PPP options listed here:
#endif

/* debug switches */
#define DEBUGFSM     1  /* ConPrintf from the FSM layer  */
#define DEBUGLCP     1  /* ConPrintf from the LCP layer  */
#define DEBUGIPCP    1  /* ConPrintf from the IPCP layer */
#define DEBUGCHAP    1  /* ConPrintf from the CHAP layer */
#define DEBUGUPAP    1  /* ConPrintf from the UPAP layer */

#define PPP_CONSOLE_LOG_STAT    0  /* disable console logging */
#define PPP_BRIEF_CONSOLE_LOG   1  /* show brief msg on snd/rcv of PPPpkt */

/* number of PPP interfaces in this port */
#ifdef DYNAMIC_IFACES
#define _NPPP 4  /* Allow creation of runtime ppp loopback iface */
#elif defined(LB_XOVER)
#define _NPPP	2  /* 2 loopback */
#else
#define _NPPP	1  /* 1 modem */
#endif

#ifdef LB_XOVER   /* define which two units loopback connects */
#define LBUNIT1   0
#define LBUNIT2   1
#endif

#define MAXNAMELEN   32   /* max length of hostname or name for auth */
#define MAXSECRETLEN 32   /* max length of password or secret */
#define MAX_DUMP_BYTES   1536 /* MAX bytes when ConPrintf-dumping pkt */

#ifdef __BORLANDC__
/* for borland port, bring in FP_SEG() macro */
/* #include <dos.h> *************/
/* #define _FP_SEG(u) FP_SEG(u) */
#endif /* __BORLANDC__ */

#define GetTickCount()	(cticks)

extern int  do_dhcp_client;   /* TRUE if we should do DHCP over PPP */
extern char * hostname; /* name of this system, for CHAP auth. */
extern int  dhc_discover(int iface);   /* send DHCP discovery */

extern unsigned NDEBUG;	/* debug printing bit mask in ..\inet */

extern int PPPDEBUG;	/* Bitmask of PPP debug options */

#define	PPPD_FILE		0x01	/* Send debug printfs to log file */
#define	PPPD_CONS		0x02	/* send debug printfs to console */

void	ppp_output(int unit,	u_char * data,	int len);

/* set system wide defaults for datagram sizes */
#define PPP_MRU 1500    /* received frames */
#define PPP_MTU 1500    /* transmitted frames */

#ifndef _PPP_PORT_C
void  ConPrintf(const char * format, ...);
#else
/*
void  ConPrintf();
*/
#endif

/* the PPP alloc routines */
#define MPPP_ALLOC(size) (M_PPP)npalloc(size)
#define MPPP_FREE(ptr) npfree(ptr)

extern int ppp_port_init(int iface);   /* per-port PPP init */

#ifdef INCLUDE_NVPARMS
struct ppp_nvparam
{
   /* PPP options: */
   int      ppp_ConsoleLog;   /* bool - Log Modem & PPP events to console */
   int      ppp_FileLog;      /* bool - Log Modem & PPP events to file */
   unshort  ppp_keepalive;    /* seconds between PPP echos, 0=disable */
   unshort  ppp_client_tmo;   /* timeout for connects as client */
#ifdef PPP_VJC
   int      ppp_request_vj;   /* bool - request that the other side do VJ compression */
#endif   /* PPP_VJC */
#ifdef   CHAP_SUPPORT   /* from ppp_port.h */
   char     secret[MAX_NVSTRING];   /* secret for CHAP/MD5 hashing */
   int      require_chap;     /* bool - require CHAP from PPP connections */
#endif   /* CHAP_SUPPORT */
#ifdef PAP_SUPPORT
   int      require_pap;
#endif   /* PAP_SUPPORT */
   char     username[MAX_NVSTRING];
   char     password[MAX_NVSTRING];
   unshort  line_tmo;   /* idle modem line timeout, 0=infinite */
   char     loginfile[MAX_NVSTRING];   /* path/name of file with login script */
   char     logservefile[MAX_NVSTRING];   /* login server script */
};

extern struct ppp_nvparam ppp_nvparms;
#endif   /* INCLUDE_NVPARMS */

#ifdef IN_MENUS
extern struct menu_op pppmenu[];
#endif   /* IN_MENUS */

#ifdef INCLUDE_NVPARMS 
extern struct nvparm_info ppp_nvformats[];
extern struct nvparm_format ppp_format;
#endif   /* INCLUDE_NVPARMS */

#endif /* USE_PPP */
#endif   /* _PPP_PORT_H */
/* end of file ppp_port.h */

