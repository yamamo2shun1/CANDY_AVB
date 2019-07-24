/*
 * FILENAME: net.h
 *
 * Copyright 1996- 2000 By InterNiche Technologies Inc. All rights reserved
 *
 * Definitions for the basic network interface object, the NET structure.
 *
 * MODULE: INET
 *
 *
 * PORTABLE: yes
 */

/* Additional Copyrights: */
/* Portions Copyright 1990-1996 by NetPort Software. 
 * Portions Copyright 1986 by Carnegie Mellon 
 * Portions Copyright 1983,1984,1985 by the Massachusetts Institute 
 * of Technology 
 */

#ifndef _NET_H_
#define  _NET_H_  1

#include "q.h"
#include "profiler.h"

#ifdef BTREE_ROUTES  /* These must be before #include "ip6mibs.h" */
/* enum values to support the route table btree software */
enum AVLSKEW {	NONE,	LEFT, RIGHT };
enum AVLRES { BT_ERROR = 0, BT_OK, BT_BALANCE };

/* types of btrees we support with rtbtree.c code */
enum AVLTYPE { IPV4_ROUTE, IPV6_ROUTE  };

typedef  unsigned long AVLDATUM;
struct avl_node {
   struct avl_node * left;       /* btree brach to higher side of tree */
   struct avl_node * right;      /* btree brach to lower side of tree */
   enum AVLSKEW skew;            /* which branch is longer */
   enum AVLTYPE treetype;        /* type of route v4 or v6 */
};
#endif   /* BTREE_ROUTES */


#ifdef IP_V6

/*
 * The basic IPv6 address; as per RFC2133
 */

struct in6_addr {
   u_char   addr[16];   /* 128 bit IPv6 address */
};

typedef struct in6_addr ip6_addr;

/*
 * Many of the IPv6 MIB groups are kept on a per-iafce basis, so it's
 * helpful to include them here
 */
#include "ip6mibs.h"

#endif /* IP_V6 */



/* some SNMP-derived net interface types, per rfc 1156, pg 14 */
/* ...these values now assigned by RFC-1700 */
#define  ETHERNET       6
#define  TOKEN802       9
#define  SLIP           28       /* SLIP, per rfc1213 */
#define  PPP            23       /* PPP, per rfc1213 */
#define  LOOPIFTYPE     24       /* loopback, per rfc1213 */

/* ifType values not covered by RFCs */
#define  EXTERN_IPX     205      /* netx.exe, not internal IPX  */
#define  PPPOE          206      /* Not in RFC1156, add unique number */
#define  IP6GIF         207      /* IPv6 Generic Interface Tunnel */
#define  IP6TO4         208      /* IPv6 "6to4" tunnel */


/* The following structure is the iface MIB from rfc1213. Most fields are
 * simple counters and are used as such by the InterNiche networking code.
 * Two exceptions are ifAdminStatus and ifOperStatus. We have tried to
 * keep their use consistent with the RFC.
 *    When an interface (NET) is created, it is marked down (i.e. the
 * ifAdminStatus is set to the "down" position) so that it can safely
 * be configured (a separate step), and then marked "up" once
 * configuration is complete. A NET with ifAdminStatus==DOWN will
 * not be used to send by the inet code.
 *    ifOperStatus should indicate the actual state of the link. For
 * ethernet this probably means UP almost all the time. For PPP it means
 * UP when connected, DOWN other wise; thus a modem in AUTOANSWER would
 * have the same ifAdminStatus and ifOperStatus (UP, DOWN) as a
 * disconnected modem.
 */

struct IfMib
{
   u_long   ifIndex;          /*  1 - entry in nets[] */
   char    *ifDescr;          /*  2 - string describing interface */
   u_long   ifType;           /*  3 - Ether, token, etc */
   u_long   ifMtu;            /*  4 - Max Transmission Unit */
   u_long   ifSpeed;          /*  5 - Theoretical max in bits/sec */
   u_char * ifPhysAddress;    /*  6 - node address */
   u_long   ifAdminStatus;    /*  7 - up=1, down=2, testing=3 */
   u_long   ifOperStatus;     /*  8 - up=1, down=2, testing=3 */
   u_long   ifLastChange;     /*  9 - time of current ifOperState */
   u_long   ifInOctets;       /* 10 - bytes received */
   u_long   ifInUcastPkts;    /* 11 - DELIVERED unicast packets */
   u_long   ifInNUcastPkts;   /* 12 - non-unicast delivered packets */
   u_long   ifInDiscards;     /* 13 - dropped, ie no buffers */
   u_long   ifInErrors;       /* 14 - bad received packets */
   u_long   ifInUnknownProtos;/* 15 - unknown  protocols */
   u_long   ifOutOctets;      /* 16 - bytes sent */
   u_long   ifOutUcastPkts;   /* 17 - unicasts sent */
   u_long   ifOutNUcastPkts;  /* 18 - non-unicast packets sent */
   u_long   ifOutDiscards;    /* 19 - send dropped - non-error */
   u_long   ifOutErrors;      /* 20 - send dropped - errors */
   u_long   ifOutQLen;        /* 21 - packets queued for send */
};

typedef struct IfMib * IFMIB;

extern   IFMIB ifmibs[];      /* preallocated interface MIB structures */

#ifndef IF_NAMELEN
#define IF_NAMELEN   4        /* default max length for iface names */
#endif

#ifdef IP_MULTICAST
struct in_multi;     /* predecl */
#endif   /* IP_MULTICAST */


#ifndef MAX_V6_ADDRS          /* allow override from ipport.h */
#define MAX_V6_ADDRS    5     /* link, site, global and Mcast, defgw */
#endif

#ifdef IP_V6
struct net;
/* prefix list */
struct prefixList
{
   struct prefixList * next;         /* list link */
   struct prefixList * prev;         /* list link */
   ip6_addr prefix;        /* prefix */
   struct net      * net;           /* IF pointer */
   int      flags;         /* flags */
   int      prefixLen;     /* prefix length */
   u_long      pmtu;          /* PMTU for this link */
   u_long   invalid_time;  /* time at which invalid, in ticks */
   u_long   life_time;     /* Preferred Lifetime */
   u_long saved_valid_lifetime;    /* lifetime, saved from the RA */
};

typedef struct prefixList RTR_PRFLIST;

/* flags for prefixList */
#define PREFX_TOO_BIG       (1 << 2)  /* indicates that an ICMP toobig */
                                      /* has tried to set PMTU < 1280. */
                                      /* even tho the PMTU is still >= 1280, */
                                      /* all pkts must contain a frag hdr. */							                
struct routerConfigEntry
{
	int MaxRtrAdvInterval;  /* The maximum time allowed between sending
                            * unsolicited multicast Router Advertisements from
                            * the interface, in seconds.
                            */
	int MinRtrAdvInterval;  /* The minimum time allowed between sending
                            * unsolicited multicast Router Advertisements from
                            * the interface, in seconds.
                            */
        u_long  nextRtrAdSend;  /* ctick time for next Rtr Ad. */
        u_long  lastRtrAdSend;  /* ctick time for last Rtr Ad. */
        int     initlRtrAds;    /* number of initial RA's sent */
	u_long AdvLinkMTU;         /* The value to be placed in MTU options sent by the
                            * router.
                            */
	int AdvReachableTime;   /* The value to be placed in the Reachable Time field
                            * in the Router Advertisement messages sent by the
                            * router.
                            */
	int AdvRetransTimer;    /* The value to be placed in the Retrans Timer field
                            * in the Router Advertisement messages sent by the
                            * router. 
                            */
	int AdvCurHopLimit;     /* The default value to be placed in the Cur Hop Limit
                           * field in the Router Advertisement messages sent by
                           * the router.
                           */
	int AdvDefaultLifetime; /* The value to be placed in the Router Lifetime field
                            * of Router Advertisements sent from the interface,
                            * in seconds.
                            */
  struct net * ifp;               /* pointer to nets entry */
  unsigned int flags;     /* see values below */
	RTR_PRFLIST *prefxEntries;  	 /* list of prefixes */
};
typedef struct routerConfigEntry RTR_CNFN;

/* flags for RTR_CNFN */
#define RTRCF_SND_ADS        1         /* router will send ads. */
#define RTRCF_MANGD          (1 << 1)  /* managed configuration */  
#define RTRCF_OTHR           (1 << 2)  /* other means of configuration */  
#define RTRCF_INITRA         (1 << 3)  /* sending inital RA's */  
#define RTRCF_READY2SEND     (1 << 4)  /* nextRtrAdSend has been set */  
#define RTRCF_FWD            (1 << 5)  /* forward pkts - act as router. */  
#endif  /* IP_V6 */


/* The NET struct has all the actual interface characteristics which 
 * are visible at the internet level and has pointers to the 
 * interface handling routines. 
 */

struct net
{
   struct net * n_next;    /* pointer next net */
   char  name[IF_NAMELEN]; /* device ID name */
   int   (*n_init)(int);   /* net initialization routine */
   /* MAC drivers can set one of the next two for sending; other
    * should be left as NULL:
    */
   int   (*raw_send)(struct net *, char*, unsigned);   
                           /* put raw data on media */
   int   (*pkt_send)(struct netbuf *);    /* send packet on media */
   int   (*n_close)(int);  /* net close routine */
   int   (*n_reg_type)(unshort, struct net*); 
                           /* register a MAC type, ie 0x0800 for IP */
   void  (*n_stats)(void * pio, int iface);   
                           /* per device-type (ODI, pktdrv) statistics dump */
   int   n_lnh;            /* net's local net header  size */
   int   n_mtu;            /* net's largest legal buffer size size */
   ip_addr  n_ipaddr;      /* interface's internet address */
   int      n_snbits ;     /* number of subnet bits */
   ip_addr  snmask;        /* interface's subnet mask */
   ip_addr  n_defgw;       /* the default gateway for this net */
   ip_addr  n_netbr;       /* our network broadcast address */
   ip_addr  n_netbr42;     /* our (4.2BSD) network broadcast  */
   ip_addr  n_subnetbr;    /* our subnetwork broadcast address */
   unsigned n_hal;         /* Hardware address length */
   struct IfMib mib;       /* the actual iface MIB image */
   IFMIB    n_mib;         /* pointer to mib for backward compatability */
   void *   n_local;       /* pointer to custom info, null if unused */
   void *   n_local2;      /* used only for MQX (contains pointer to MQX 
                            * Ethernet configuration structure, 
                            * ENET_CFG_STRUCT_PTR) or WinPcap 4.0-based
                            * driver (contains pointer to struct wpcap_dev) */
   int      n_flags;       /* mask of the NF_ bits below */

#ifdef DYNAMIC_IFACES
   int   (*n_setstate)(struct net*, int);
                           /* set link state up/down */
#endif   /* DYNAMIC_IFACES */

#ifdef IP_V6
   /* A V6 iface has a whole slew of IP addresses: */
   struct   ip6_inaddr *  v6addrs[MAX_V6_ADDRS];
   struct   ip6_inaddr *  v6addrsExtd[MAX_V6_ADDRS];   /* this allows for 2 IPv6 addrs, of each type, per IF. */
   int      DupAddrDetectTransmits;       /* per RFC-2463 */
   struct   ipv6If_mib     ip6_ifmib;     /* IPv6 MIb - per iface group */
   struct   icmp6_mib      icmp6_ifmib;   /* and another for ICMP */
   struct   ip6_inaddr     *ip6_lookupcache;   /* last router addr. */
   int                     IPv6IFUp;      /* IPv6 IF up. */
   void                    *lastDefRouter;  /* ndcache of last router. */
   int                     ip6_dupchecks;   /* count of dupe NS checks to be done */
   int                     ND_send_RS;   /* flag to send RS */
   int                     cntRtrSols;  /* count of RS */
   int                     lasttmRtrSols;  /* last time RS */
   int                     gotRouter;     /* got a router */
   RTR_CNFN             *  rtrConfig;    /* pointer to router config */
#endif /* IP_V6 */

#ifdef  IP_MULTICAST
   /* register a multicast list */
   int      (*n_mcastlist)(struct in_multi *);
   struct in_multi * mc_list;
#if defined (IGMP_V1) || defined (IGMP_V2)
   /* boolean variable that indicates our belief in whether the 
    * network currently "has" a  v1 router.  This variable is 
    * set to false (0) at startup for IGMPv2, and is always true
    * for IGMPv1.
    */
   u_char igmpv1_rtr_present; /* 1 (IGMP_TRUE) or 0 (IGMP_FALSE) */
   /* this variable keeps track of when (in ticks) the last IGMPv1
    * Host Membership Query message was received (the contents of
    * this variable are only valid when we believe that the network
    * has a v1 router).  This is only valid for a link that is
    * configured in IGMPv2 mode.
    */
   u_long igmpv1_query_rcvd_time;
   /* operating mode for IGMP on this link - IGMPv1 or IGMPv2 */
   u_char igmp_oper_mode;   
#endif /* IGMPv1 or IGMPv2 */
#endif /* IP_MULTICAST */
};

typedef  struct net * NET;

extern   unsigned    ifNumber;   /* how many links (number of nets[] used) */

/*
 * These serve as IPv6 address types AND indexes into the v6addrs
 * table for key addresses.
 */
#define IPA_LINK     0     /* Link local IP v6 address */
#define IPA_SITE     1     /* Site local IP v6 address  */
#define IPA_GLOBAL   2     /* Global IP v6 address  */
#define IPA_MCAST    3     /* Mcast, for Neighbor discovery */
#define IPA_DEFGW    4     /* default gateway for IPv6 */


/* bit definitions for net.n_flags */

#define  NF_BCAST    0x0001   /* device supports broadcasts */
#define  NF_MCAST    0x0002   /* device supports multicast */
#define  NF_DYNIF    0x0004   /* Iface is a dynamic interface */
#define  NF_NBPROT   0x0008   /* driver will maintain pkt->nb_prot field */
#define  NF_GATHER   0x0010   /* driver supports scatter gather */
#define  NF_IEEE48   0x0020   /* hardware has an IEEE 48 bit address */
#define  NF_IEEE64   0x0040   /* hardware has an IEEE 64 bit address */
#define  NF_IPV6     0x0080   /* Iface supports IPv6 */
#define  NF_DHCPC    0x0100   /* Iface uses DHCP Client to obtain IP addr */
#define  NF_AUTOIP   0x0200   /* Iface uses AutoIP to obtain IP addr */
#define  NF_6TO4     0x0400   /* Iface is RFC-3056 "6to4" tunnel */

#ifdef IP_MULTICAST
/* prototype multicast routines in ipmc.c: */
struct in_multi *    lookup_mcast(ip_addr addr, NET netp);
struct in_multi *    in_addmulti(ip_addr *ap, struct net * netp, int type);
void                 in_delmulti(struct in_multi * inm);
NET *                mcaddr_net(ip_addr);
#ifdef IP_V6
struct in_multi *    v6_lookup_mcast(ip6_addr * addr, NET netp);
NET *                v6mcaddr_net(ip_addr);
#endif   /* IP_V6 */
#endif   /* IP_MULTICAST */


/* parameters to ni_set_state - match RFC-1213 ifXxxxStatus */
#define  NI_UP     1
#define  NI_DOWN   2

#ifdef DYNAMIC_IFACES

struct niconfig_0    /* struct for iface configuration info */
{
   ip_addr  n_ipaddr;
   ip_addr  snmask;
};

int      ni_create(NET * ifp,    /* OUT - created handle */
                   int (*create_device)(NET newifp, void * bindinfo),  /* IN */
                   char *name,         /* IN - short text name of device */
                   void *blindinfo);
int      ni_get_config(NET, struct niconfig_0 *);
int      ni_set_config(NET, struct niconfig_0 *);
int      ni_set_state(NET, int opcode);
int      ni_delete(NET ifp);
#endif  /* DYNAMIC_IFACES */

extern unsigned   ifNumber;      /* how many ifaces in system */

/* In non-dynamic interface systems STATIC_NETS will be the same as 
 * MAXNETS, so don't force all the ipport.h_h files to define it.
 */
#ifndef STATIC_NETS
#define STATIC_NETS  MAXNETS
#endif

extern   NET   nets[MAXNETS];    /* pointers to the static network structs */
extern   queue netlist;          /* master list of nets, static & dynamic */


#ifndef MINI_IP   /* Declare things defined away in the mini version */
/* define a way to convert NET pointers to index numbers */
int      if_netnumber(NET);   /* get index into nets list for NET */
NET      if_getbynum(int);    /* get static/dynamic NET from index */
int      isbcast(NET ifc, u_char * addr); /* is MAC address broadcast? */
extern   int   reg_type(unshort);   /* register prot type with all net drivers */

extern   int   MaxLnh;        /* Largest local net header size */
extern   int   MaxMtu;        /* Largest net MTU (packet) size */
#endif   /* MINI_IP */


/* alias macro to support older code */
#define  net_num(net)   if_netnumber(net)

extern   queue rcvdq;


/* Macro for passing IP addresses to printf octet at a time. usage: 
 * printf("IP is %u.%u.%u.%u\n", PUSH_IPADDR(ip)); Since we store IP 
 * addresses in net endian, it's endian sensitive. 
 */
#if (BYTE_ORDER==LITTLE_ENDIAN)
#define  PUSH_IPADDR(ip)\
   (unsigned)(ip&0xff),\
   (unsigned)((ip>>8)&0xff),\
   (unsigned)((ip>>16)&0xff),\
   (unsigned)(ip>>24)
#else
#define  PUSH_IPADDR(ip)\
   (unsigned)(ip>>24),\
   (unsigned)((ip>>16)&0xff),\
   (unsigned)((ip>>8)&0xff),\
   (unsigned)(ip&0xff)
#endif

/* Here are the debugging things. DEBUG is a global variable whose 
 * value determines what sort of debugging messages will be displayed 
 * by the system. 
 */
extern   unsigned NDEBUG;  /* Debugging options */


/* BUGHALT on a gross applications level error
 * that is detected in the network code
 */
#define  BUGHALT     0x01  /* deprecated */

/* Works in conjuction with other options: BUGHALT: 
 * Dump all arriving packets PROTERR: Dump header for level of error 
 * NETTRACE, etc.: Dump headers at trace 
 */
#define DUMP 0x02


#define  INFOMSG     0x04     /* Print general informational messages */
#define  NETERR      0x08     /* Display net interface error messages */
#define  PROTERR     0x10     /* Display protocol level error messages */
#define  NETRACE     0x20     /* Trace packet in link level net layer */
#define  TMO         0x40     /* Print message on timeout */
#define  APTRACE     0x80     /* Trace packet through application */
#define  TPTRACE     0x100    /* Transport protocol (UDP/TCP/RVD) trace */
#define  IPTRACE     0x200    /* Trace packet in internet layer  */
#define  UPCTRACE    0x400    /* trace upcall progress - DANGEROUS! */
#define  LOSSY_RX    0x800    /* do lossey receive */
#define  LOSSY_TX    0x1000   /* do lossey transmit */


#ifdef  LOSSY_IO
/* support for an option that allows us to deliberatly loose packets */
extern int   in_lastloss;  /* packets since last loss */
extern int   in_lossrate; /* number of packets to punt (3 is "1 in 3") */
#endif   /* LOSSY_IO */

/* add prototypes -JB- */
int   Netinit(void);
void  netclose(void);
void  net_stats(void);


/* C language misc prototypes for IP stack
 * these are not strictly NET related, but have to go somewhere
 */

u_long   c_older(u_long ct1, u_long ct2);    /* return older ctick */
void     panic(char * msg);         /* call on fatal errors, like UNIX panic() */
unsigned long   getipadd(char *);   /* resolve hostname to IP address */
char *   parse_ipad(unsigned long *, unsigned *, char *);
char *   print_ipad(unsigned long); /* format IP addr for print */
char *   startup(void);    /* master network "start" cmd, in startup.c */
#ifndef MINI_IP
void     exit_hook(void (*func)(void));   /* like atexit() */
void     ip_exit(void);             /* call exit_hook funcs */
#endif  /* MINI_IP */

/* ip2mac() Trys to put IP packet on ethernet, resolving ip address via arp. */
int      ip2mac(PACKET, ip_addr);
void     pktdemux(void);            /* process received packets */

#endif   /* _NET_H_ */



