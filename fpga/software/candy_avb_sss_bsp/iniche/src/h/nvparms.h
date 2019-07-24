/*
 * FILENAME: nvparms.h
 *
 * Copyright  2000 By InterNiche Technologies Inc. All rights reserved
 *
 *
 * MODULE: TCP
 *
 *
 * PORTABLE: yes
 */

/* Additional Copyrights: */
/* Portions Copyright 1996 by NetPort Software. */


#ifndef _NVPARMS_H
#define  _NVPARMS_H  1

#include "ipport.h"
#include "libport.h"

#ifdef INCLUDE_NVPARMS

#ifndef MAX_NVSTRING
#define  MAX_NVSTRING   128   /* MAX length of a nparms string */
#endif /* MAX_NVSTRING */

#include "q.h"
#include "netbuf.h"
#include "net.h"
#include "vfsfiles.h"
#include "nvfsio.h"

struct nvparm_info /* see explanation in nvparms.c */
{
   char * pattern; /* pattern to look for in file */
   int    nvtype;
   int    nvbound;
   void * nvdata;

   /* Optional Pointer to a function to parse proprietary nvparm types.
    * This is usually initialised to null as most modules use generic
    * nvparm data types unlike DHCP Server.
    */
   int    (*get_nvprop_type)(struct nvparm_info * nvinfo_ptr, char * parm);
};

extern struct nvparm_info ppp_nvformats[];

/* Module specific nvparm_format structure */
struct nvparm_format
{
   int count;          /* no. of elements in the info_ptr list */
   struct nvparm_info * info_ptr;
   int (*setfunc)(NV_FILE *);
   struct nvparm_format * next_format;
};

extern struct nvparm_format ppp_format;


int install_nvformat(struct nvparm_format * new_nvformat, 
                     struct nvparm_format * head_nvformat);

extern struct nvparm_format *nv_formats;

/* per-iface structure. These match up the the nets[] array */
struct ifinfo
{
   char        name[IF_NAMELEN]; /* name of interface */
   ip_addr     ipaddr;           /* IP address for this iface */
   ip_addr     subnet;           /* subnet mask for above */
   ip_addr     gateway;          /* default gateway */
   int         client_dhcp;      /* bool - Be DHCP client on this link */
#ifdef IP_V6
   ip6_addr   ipv6addr;          /* IPV6 global address */
#endif
#if defined (IP_MULTICAST) && (defined (IGMP_V1) || defined (IGMP_V2))
   u_char      igmp_oper_mode;   /* IGMP protocol (v1 or v2) that will 
                                  * be run on this link */
#endif /* IP_MULTICAST and (IGMPv1 or IGMPv2) */
};


/* Structure for NV data set/written by xxx_nv_parms() routines */
struct inet_nvparam
{
   struct ifinfo  ifs[MAXNETS];  /* per-interface IP info */

#ifdef DNS_CLIENT
#ifndef MAXDNSSERVERS
#define  MAXDNSSERVERS  3        /* MAX number of servers to try */
#endif
   ip_addr  dns_servers[MAXDNSSERVERS];   /* DNS servers */
#ifdef DNS_CLIENT_UPDT
   char     dns_zone_name[MAX_NVSTRING];  /* zone name   */
   u_long   dns_update_server;            /* dynamic update server name   */
#endif /* DNS_CLIENT_UPDT */
#endif /* DNS_CLIENT */
};

#ifdef USE_COMPORT
struct comport_nvparam
{
   int   comport;             /* PC comm port to default to */
   int   LineProtocol;        /* 1=PPP, 2=SLIP */
};

extern struct comport_nvparam comport_nvparms;
#endif /* USE_COMPORT */

/* the IP stack's nvparm structure */
extern struct inet_nvparam inet_nvparms;

#ifdef USE_COMPORT
extern   struct comport_nvparam comport_nvparms;
#endif /* USE_COMPORT */

extern   void (*set_nv_defaults)(void);

int   get_nv_params(void);
int   set_nv_params(void * pio);
int   edit_nv_params(void * pio);
int   if_configbyname(NET);


#define NVINT             20
#define NVCHAR            21
#define NVUNSHORT         22
#define NVIPDEC           23
#define NVBOOL            24
#define NVSTRING          25

/* proprietary NVPARMS types for NicheStack */
/* Necessary Evils for NETS array parms */
#define NVIF_NETS         26    /* Can be no. or name */
#define NVIPDEC_NETS      27
#define NVSBDEC_NETS      28
#define NVGTDEC_NETS      29
#define NVBOOL_NETS       30
#define NVCHAR_NETS       35

/* Not so necessary Evils */
#define NVDNSSRV          31    /* Special type for DNS Clients' Srv No. & IP's */
#define NVLINEPROT        32
#define NVIPV6DEC_NETS    33
#define NVIP46DEC         34    /* used to read IPv4/IPv6 addresses */

#define NVBND_NOOP        -1

/* Module specific proprietary NVPARMS types that require/maintain function
 * pointers to parse functions that are aware of these new nvparm datatypes
 *
 * While adding new types use codes in the range of 100 to 200 to aid error
 * checking in the get_nv_value() routine in nvparms.c.
 *
 * Note that the values for all these macros need not be distinct, only the 
 * types that are mapped to a particular parse function need to be distinct.
 *
 * So in this example only the macros in the NVPR_DHS_XXX_NETS block need to
 * be distinct from each other and can be repeated in the NVPR_DHS_XX_POOL 
 * block since they are mapped to two different parse functions. But all the
 * values do need to be in the valid range of 100 - 200.
 */

#define NVPR_DHS_NIF_NETS 121
#define NVPR_DHS_DGW_NETS 122
#define NVPR_DHS_DNS_NETS 123
#define NVPR_DHS_DOM_NETS 124
#define NVPR_DHS_SNM_NETS 125
#define NVPR_DHS_LEA_NETS 126

#define NVPR_DHS_ID_POOL 127
#define NVPR_DHS_HI_POOL 128
#define NVPR_DHS_LW_POOL 129
#define NVPR_DHS_IF_POOL 130

#define NVPR_DHS_IDN_CLI 131
#define NVPR_DHS_HNM_CLI 132
#define NVPR_DHS_LEA_CLI 133
#define NVPR_DHS_DIP_CLI 134
#define NVPR_DHS_SNM_CLI 135
#define NVPR_DHS_DGW_CLI 136
#define NVPR_DHS_DNS_CLI 137



/* nv_sectioninfo represents information about a section in the NV file. 
 * 1. Name of the section 
 * 2. Function to be called to parse a line in that section. 
 * 3. Name of the list which contains all elements for 
 * this section. (used only by the add/del menu commands) 
 * 4. Function called to print usage of a command. 
 * (used only by the add/del menu commands) So, for providing 
 * implementation for a new section, the following needs to be done. 
 * 1. Add a function to parse a line in that section. 
 * 2. Add an entry to the v3_sections array. This would 
 * ensure that all the values from the new section will be read 
 * during init time. 
 *
 * To provide command line addition, deletion 
 * commands for a table, information about list and function 
 * displaying usage (of the command) should also be added to 
 * v3_sections array. 
 *
 * If any application needs to read table/tables from a NV file,
 * then it should do the following
 * 1. Define a nv_sectioninfo array (e.g. v3_sections[] for SNMPv3)
 * 2. Call nv_read_parse() to read and parse all the sections
 *    from fname. 
 *
 * 2/22/2 - Moved from SNMPv3 -AT-
 */

struct nv_sectioninfo
{
   char * name  ;                   /* name of section */
   int (*parse_func)(char * line);  /* Function to parse the section */
   void * list;                     /* Name of list containing all elements */
   void (*usage_func)(void * pio);  /* Function to print usage of cmd */
};

int nv_get_sec_num(char * buf, struct nv_sectioninfo *sec, int slen);
int nv_read_parse(char *fname, struct nv_sectioninfo *sec,int slen);
int nv_add_entry(void * pio, int index, struct nv_sectioninfo *sec);
int nv_del_entry(void * pio, int index, struct nv_sectioninfo *sec);
int nv_del_entry_byid(void * pio, int index, struct nv_sectioninfo *sec);


#define  NOT_A_SECTION        -1

#endif   /* INCLUDE_NVPARMS */

#endif   /* _NVPARMS_H */



