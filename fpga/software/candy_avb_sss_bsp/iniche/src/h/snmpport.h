/*
 * FILENAME: SNMPPORT.H
 *
 * Copyright  2000 By InterNiche Technologies Inc. All rights reserved
 *
 *
 * MODULE: SNMPV1
 *
 * ROUTINES:
 *
 * PORTABLE: yes
 */

/* Additional Copyrights: */

/* snmpport.h 
 * Portions Copyright 1990, 1993 by NetPort Software. 
 */

#ifndef _SNMPPORT_H_
#define  _SNMPPORT_H_   1


/* includes for this port */
#include "ipport.h"
#include "libport.h"
#include "socket.h"             /* for decl of struct sockaddr */

/* options to trade off features for size: */
#define ENABLE_SNMP_TRAPS    1  /* this SNMP can send traps */
#define INICHE_TCPIP         1  /* SNMP will use InterNiche TCPIP stack */
#define MIB_COUNTERS         1  /* instrument TCPIP stack */
#define SNMP_32BIT_OID       1  /* Support parsing/building of 32-bit OIDs */
#define SNMP_TEST_SP_TRAP    1  /* Test enterprise specific traps */
#define SNMP_COUNTER64       1  /* support 64-bit datatypes */
#define SNMP_SYSLOG          1  /* send syslog() msg for SETs, traps */

#ifndef SNMP_SOCKETS
#define PREBIND_AGENT   1   /* hardcode SNMP port into UDP */
#endif


#ifdef NOT_USED
#endif

/* If INCLUDE_SNMP is not defined, then SNMPv1 is not enabled in the 
 * build. Hence we don't need to enable SNMPv1 Traps. This undef is 
 * done to avoid the compilation errors resulting due to reference to 
 * nvram parameters in snmpport.c 
 */
#ifndef INCLUDE_SNMP
#ifdef ENABLE_SNMP_TRAPS
#undef ENABLE_SNMP_TRAPS
#endif
#endif

#ifndef DOS_TSR
#define  SUBID_32BIT    1  /* 32 bit Object ID components */
#endif   /* else default to 16 bits */


#ifdef ENABLE_SNMP_TRAPS
#define  SNMP_UDP_TRAPS     1  /* v1/v2c over UDP, see trap_out.c */
#else   
/* If traps are not enabled, then snmp_trap() does nothing */ 
#define  snmp_trap 
#endif

#ifdef SUBID_32BIT
/* most 32 bit CPUS will want 32 bit oid components: */
#ifndef V3_OID_DEFINED
typedef u_long   oid;
#endif 
#define  MAX_SUBID   0xFFFFFFFF
#else    /* 16 bit sub IDs */
/* this is for a 16 bit processor oid component: */
typedef u_short   oid;
#define  MAX_SUBID   0xFFFF
#endif

#ifdef  SNMP_32BIT_OID 
typedef u_long oid32 ;
#endif

#ifdef SNMP_COUNTER64
/* define the datatype to hold a 64-bit counter. 
 * It can be done in a variety of ways (one 64bit value, 2 32bit values,
 * 8 bytes, etc. Hence use a union to define it.
 */

typedef union counter64
{
   struct longlong
   {
      unsigned long h,l;  /* two 32bit values h is higher, l is lower */
   }ll;
   char s[8];             /* s0 is LSB, s7 is MSB */
}Counter64;

#endif /* SNMP_COUNTER64 */


/* if you define MIB_COUNTERS, you need to provide the include file 
 * which describes the MIB-2 SNMP groups counters: 
 */
#ifndef SNMPMIB_H_FILE /* if it is not already defined */
#ifdef  INCLUDE_SNMPV3
#define SNMPMIB_H_FILE "snmp_vie.h" /* define it */
#else
#define SNMPMIB_H_FILE "rfc1213_.h" /* define it */
#endif
#endif /* SNMPMIB_H_FILE */

#ifdef MIB_COUNTERS
extern struct variable;
#include SNMPMIB_H_FILE 
extern struct snmp_mib   SnmpMib;
#endif

#define  SNMP_PORT            161
#define  SNMP_TRAP_PORT       162

/* per-port local limitations */
#define  MAX_NAME_LEN         64 /* max number of subid's in a objid */
#define  MAX_OID_LEN          64 /* max length in bytes of an encoded oid */
#define  MAX_COMMUNITY_SIZE   32 /* max chars in a community string */


#ifdef ENABLE_SNMP_TRAPS
#define  MAX_TRAP_TARGETS     3  /* max number of trap target info slots */
#define  NETPORT_TEST_TRAP    80
#endif

/* Companies porting this code should get their own number from iana.org ! */
#define  ENTERPRISE     18868    /* InterNiche Tech. enterprise number */


#ifndef oidcpy
#define  oidcpy(dest,src,len) MEMCPY((char*)dest,(char*)src,((int)(len)*sizeof(oid)))
#endif

#ifndef ntohs
#define  ntohl(l) lswap(l) /* for intel 8086 systems */
#define  ntohs(w) bswap(w)
#endif

#define  MIB1  1, 3, 6, 1, 2, 1     /* standard mib Object Id, from mib.h */
#define  MIB2  1, 3, 6, 1, 2, 1     /* standard mib-II Object Id, from mib.h */

#define  SYS_STRING_MAX    256      /* max length of sys group strings */

#ifdef DOS_TSR
#define  SNMPSIZ     500   /* MAX size of an snmp packet */
#else
#define  SNMPSIZ     1400  /* MAX size of an snmp packet */
#endif

extern   u_char   snmpbuf[SNMPSIZ]; /* global buffer for packet building */

/* map Berkeley-ish bcopy call to Iniche macro: */
#ifndef bcopy
#define  bcopy(src,  dest, len)     MEMCPY(dest,   src,  len)
#endif

/* omit error handler from DOS TSR */
#ifdef DOS_TSR
#define  SNMPERROR(msg);   ;  /* map to no-op */
#else
int    SNMPERROR(char*);
extern   char *   snmp_err_text;    /* Set to non-null by ERROR() */
#endif


/* SNMP needs a mechanism to find the agent's IP address. ip_mymach() 
 * (MYMACHine) should return the IP address of the agent relative to 
 * a passed IP address. For single homed agents, this will be the IP 
 * address of the sole interface. For multi-home, some coding may be
 * required.
 */
#ifdef MINI_IP
#include "netbuf.h"           /* needed for net.h */
#include "net.h"              /* needed for defn of "struct net" */
extern struct net * m_netp;   /* needed for m_netp */
#define ip_mymach(addr)  (m_netp->n_ipaddr)
#else
u_long  ip_mymach(u_long);    /* return 0L if IPX trap */
#endif



extern   char  sys_descr[SYS_STRING_MAX]; /* define in application */


/* The SNMP_ENTRY macro is used to compile the external SNMP entry 
points with special code needs. */
/* #define SNMP_ENTRY loadds */
/* #define SNMP_ENTRY */


#define  far   /* define away intel segment/model support */

/* macro to force SNMP entry points to do correct segment setup. This 
 * was implemented to handle builds on intel x86 CPUs where we want 
 * to use a data segment other then the default for SNMP data due to 
 * the 64K limit and potentially large size of variables[] 
 */
/* Intel flavor */
/* #define SNMP_ENTRY __far __loadds    */
/* default */
#define  SNMP_ENTRY

/* SNMP Agent related non-volatile parameters. Please see nvparms.h
 * and nvparms.c regarding the usage of the following structure.
 */
#ifdef INCLUDE_NVPARMS
#ifdef INCLUDE_SNMP
struct snmp_nvparam
{
   /* SNMP agent stuff: */
   char     getcomm[MAX_NVSTRING];     /* get community string for our agent */
   char     setcomm[MAX_NVSTRING];     /* set community string for our agent */
   char     sysContact[MAX_NVSTRING];  /* setable mib2 sysGroup strings */
   char     sysName[MAX_NVSTRING];     /* MIB-2 sysName */
   char     sysLocation[MAX_NVSTRING]; /* MIB-2 sysLocation */
   struct sockaddr trapTarget1;        /* IP address to send SNMP traps to. */
   char     trapcomm1[MAX_NVSTRING];   /* community for trapTarget1 */
};

extern struct snmp_nvparam snmp_nvparms;
#endif /* INCLUDE_SNMP */
#endif /* INCLUDE_NVPARMS */

extern char* print46_addr(struct sockaddr *ipaddr);
ip_addr ip_myaddr(struct sockaddr *addr);
extern int snmp_send_trap(u_char * data, int len, struct sockaddr * addr);


/* Define SNMP_LOGOID and SNMP_LOGTRAP - funcs used to send syslog msgs.
 * If SNMP_SYSLOG is enabled, map them to the corresponding functions
 * Else make them no-ops.
 */
#ifdef SNMP_SYSLOG
int snmp_logtrap(int type, char * msg);
int snmp_logoid(char *community, oid * var_name, unsigned var_name_len, 
   long err, u_char * var_val, u_char valuetype);
#define SNMP_LOGOID     snmp_logoid
#define SNMP_LOGTRAP    snmp_logtrap
#else
#define SNMP_LOGOID
#define SNMP_LOGTRAP
#endif


#endif   /* _SNMPPORT_H_ */



