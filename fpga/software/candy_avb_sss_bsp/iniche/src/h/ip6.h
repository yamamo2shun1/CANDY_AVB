/*
 * FILENAME: ip6.h
 *
 * Copyright 2002 By InterNiche Technologies Inc. All rights reserved
 *
 * This file contains definitions for code IPv6.
 *
 * If this file is included in a source along with the IPv4-ish "ip.h" file
 * Then the IPv4 file should appear in the include list first. 
 *
 * MODULE: IPV6
 *
 * ROUTINES: 
 *
 * PORTABLE: yes
 */


#ifndef _IP6_H_
#define _IP6_H_ 1

/* Sanity test for #defines in ipport.h */
#ifndef LINKED_PKTS
#error IP_V6 option requires LINKED_PKTS option in ipport.h
#endif

#ifndef IP_MULTICAST
#error IP_V6 option requires IP_MULTICAST option in ipport.h
#endif


/*
 * THE IPv6 header
 */
struct ipv6
{
   u_char   ip_ver_flo[4];    /* 4 bits version (6), 8 bits class, & flow label */
   u_short  ip_len;           /* payload length */
   u_char   ip_nexthdr;       /* next header type 6 = TCP, etc */
   u_char   ip_hopcount;      /* hops left until expires */
   ip6_addr ip_src;           /* source and dest addresses */
   ip6_addr ip_dest;
};


/*
 * IP v6 extention headers
 *
 */

struct ex6_hopopts     /* IPPROTO_HOPOPTS - type 0  */
{
   u_char    nexthdr;       /* type of next header */
   u_char    extlen;		/* length of this header */
   u_char    reserved;      /* reserved field */
};

struct ex6_destopts     /* IPPROTO_DSTOPTS - type 60 */
{
   u_char    nexthdr;       /* type of next header */
   u_char    extlen;		/* length of this header */
   u_char    reserved;      /* reserved field */
};

struct ex6_route     /* IPPROTO_ROUTE - type 43  */
{
   u_char   nexthdr;    /* type of next header */
   u_char   extlen;     /* length of this header */
   u_char   type;       /* routing header type */
   u_char   segments;   /* segments left */
      /* variable amount of data follows */
};

/* Fragment header */
struct ex6_frag      /* IPPROTO_FRAGMENT - type 44 */
{
   u_char    nexthdr;       /* type of next header */
   u_char    reserved;      /* reserved field */
   unshort   offset;        /* offset and flag */
   u_long    id;            /* IP packet identification */
};

#define op6_frag ex6_frag  /* cover rename */ 
#define IPV6_MOREFRAGS  0x01  /* "more frags" flag in frag->offset */

struct ex6_auth         /* IPPROTO_AH - type 51 */
{
   u_char    nexthdr;       /* type of next header */
   u_char    reserved;      /* reserved field */


};

struct ex6_esp       /* IPPROTO_ESP - type 50 */
{
   u_char    nexthdr;       /* type of next header */
   u_char    reserved;      /* reserved field */


};




/*
 * An Interniche IP v6 address for this machine
 */

struct net;    /* predecl */

struct ip6_inaddr
{
   struct ip6_inaddr *  next;    /* for application use */
   ip6_addr             addr;    /* address value (maybe unassigned) */
   int                  prefix;  /* number of bits in prefix */
   struct net *         ifp;     /* iface associated with it (maybe NULL) */
   int                  flags;   /* mask of the IA_ bits below */
   u_long               tmo1;    /* expiration ctick if IA_LEASE flag set */
   u_long               tmo2;    /* expiration ctick of address */
   u_long               lasttm;  /* last action for dup checking, etc. */
   int                  dups;    /* count of DADNS sent */
};


/* values for in6_addr->flags */
#define IA_STATEMASK    0x03     /* low two bits have one of the following: */
#define IA_TENTATIVE    0x00     /* address is being checked for duplicate */
#define IA_PREFERRED    0x01     /* tmo1 field is end of "preferred" period */
#define IA_DEPRECATE    0x02     /* address is being deprecated */

#define IA_MFLAG        0x04     /* ManagedFlag, RFC-2462 */
#define IA_OFLAG        0x08     /* OtherConfigFlag, RFC-2462 */
#define IA_LEASE        0x10     /* tmo1 field is a finite lease */



/* ipv6->nexthdr values. Names are from RFC-2292 (IPv6 raw sockets) */
#define  IPPROTO_HOPOPTS   0  /* IPv6 Hop-by-Hop options */
#define  IPPROTO_IPV6      41 /* IPv6 header */
#define  IPPROTO_ROUTING   43 /* IPv6 Routing header */
#define  IPPROTO_FRAGMENT  44 /* IPv6 fragmentation header */
#define  IPPROTO_ESP       50 /* encapsulating security payload */
#define  IPPROTO_AH        51 /* authentication header */
#define  IPPROTO_ICMPV6    58 /* ICMPv6 */
#define  IPPROTO_NONE      59 /* IPv6 no next header */
#define  IPPROTO_DSTOPTS   60 /* IPv6 Destination options */


#define  IPV6_MINMTU    1280   /* recommended path MTU for IPv6 Internet (rfc2460) */
#define  IPV6_MAXMTU    1500   /* large path MTU for IPv6  */

#define ND_DEFRALG_OLD  0     /* use oldest router */
#define ND_DEFRALG_RR   1     /* use round robin router */

#define ND_PMTU_ID   0        /* ideally comply with RFC1981 PMTU storage */
#define ND_PMTU_MIN  1        /* minimally comply with RFC1981 PMTU storage */

/* values for ND_send_RS_Flag */
#define ND_SEND_RS  0        /* send out RS at init */
#define ND_NOTSEND_RS  2     /* do not send out RS at init */

extern ip6_addr allRoutersMcast;
extern ip6_addr allNodesMcast;
extern ip6_addr ip6unspecified;
extern ip6_addr ip6loopback;
extern ip6_addr ip6llloopback;

extern NET ip6_lbnet;    /* loopback interface */

extern ip6_route * rt6_lookup(ip6_addr * dest);


/* define address testing macros (as per rfc2553) */
#define __IPV6_ADDR_MC_SCOPE(a)		((a)->addr[1] & 0x0f)
#define __IPV6_ADDR_SCOPE_NODELOCAL	0x01
#define __IPV6_ADDR_SCOPE_LINKLOCAL	0x02
#define __IPV6_ADDR_SCOPE_SITELOCAL	0x05
#define __IPV6_ADDR_SCOPE_ORGLOCAL	0x08	/* just used in this file */
#define __IPV6_ADDR_SCOPE_GLOBAL	0x0e

/*
 * Unicast Scope
 * Note that we must check topmost 10 bits only, not 16 bits (see RFC2373).
 */

/* Macro to identify IPv6 multicast addresses */
#define IN6_IS_ADDR_MULTICAST(ipaddr)  ((ipaddr)->addr[0] == 0xFF)

/* Macro to identify IPv6 multicast scope */
#define IN6_IS_ADDR_MCSCOPE(ipaddr)   ((ipaddr)->addr[1] & 0x0F)

/* Macro to identify IPv6 global address - highest 3 bits are 001 */
#define IN6_IS_ADDR_GLOBAL(ipaddr) (((ipaddr)->addr[0] & 0xE0) == 0x20)

/* Macro to identify IPv6 Link-local addresses */
#define IN6_IS_ADDR_LINKLOCAL(ipaddr) \
     (((ipaddr)->addr[0] == 0xFE) && \
     ((ipaddr)->addr[1] == 0x80))

/* Macro to identify IPv6 Site-local addresses */
#define IN6_IS_ADDR_SITELOCAL(ipaddr) \
     (((ipaddr)->addr[0] == 0xFE) && \
     ((ipaddr)->addr[1] == 0xC0))

#define IN6_IS_ADDR_UNSPECIFIED(ipaddr) (IP6EQ(ipaddr, &ip6unspecified))
#define IN6_IS_ADDR_LOOPBACK(ipaddr) \
     ((IP6EQ(ipaddr, &ip6loopback)) || (IP6EQ(ipaddr, &ip6lllopback)))


/* Following macros from RFc2553 are not currently used 
int  IN6_IS_ADDR_V4MAPPED    (const struct in6_addr *);
int  IN6_IS_ADDR_V4COMPAT    (const struct in6_addr *);
*/

#define IN6_IS_SCOPE_LINKLOCAL(a)	\
	((IN6_IS_ADDR_LINKLOCAL(a)) ||	\
	 (IN6_IS_ADDR_MC_LINKLOCAL(a)))


#define IN6_IS_ADDR_MC_NODELOCAL(a)	\
	(IN6_IS_ADDR_MULTICAST(a) &&	\
	 (__IPV6_ADDR_MC_SCOPE(a) == __IPV6_ADDR_SCOPE_NODELOCAL))
#define IN6_IS_ADDR_MC_LINKLOCAL(a)	\
	(IN6_IS_ADDR_MULTICAST(a) &&	\
	 (__IPV6_ADDR_MC_SCOPE(a) == __IPV6_ADDR_SCOPE_LINKLOCAL))
#define IN6_IS_ADDR_MC_SITELOCAL(a)	\
	(IN6_IS_ADDR_MULTICAST(a) && 	\
	 (__IPV6_ADDR_MC_SCOPE(a) == __IPV6_ADDR_SCOPE_SITELOCAL))
#define IN6_IS_ADDR_MC_ORGLOCAL(a)	\
	(IN6_IS_ADDR_MULTICAST(a) &&	\
	 (__IPV6_ADDR_MC_SCOPE(a) == __IPV6_ADDR_SCOPE_ORGLOCAL))
#define IN6_IS_ADDR_MC_GLOBAL(a)	\
	(IN6_IS_ADDR_MULTICAST(a) &&	\
	 (__IPV6_ADDR_MC_SCOPE(a) == __IPV6_ADDR_SCOPE_GLOBAL))
/*
 * Macros to compare or copy IPv6 addresses. These may be replaced
 * (in ipport.h) with macros or routines which are more efficient
 * on a particular compiler.
 */
#ifndef  IP6EQ
 /* compare two IP addresses, return TRUE if the same; else FALSE */
#define IP6EQ(addrptr1, addrptr2) (!MEMCMP(addrptr1, addrptr2, sizeof(ip6_addr)))
#endif

/* Macro to copy IPv6 addresses */
#ifndef  IP6CPY
/* Copy addrptr2 into addrptr1 */
#define IP6CPY(addrptr1, addrptr2) MEMCPY(addrptr1, addrptr2, sizeof(ip6_addr))
#endif

/*
 * global IPv6 routines
 */

/* standard IPv6 to MAC send routines */
extern   int   ip6_sendeth(PACKET pkt, struct in6_addr * router);
extern   int   ip6_sendppp(PACKET pkt);

/* Set up addresses of interface for IPv6 */
int   ip6_ifaddrs(struct net * ifp);

/* per-port routine to handle sending packets on odd MAC types */
extern   int  (*ip6_portsend)(PACKET pkt, struct in6_addr * router);

/* Routine to send an IPv6 packet */
int   ip6_send(PACKET pkt, int fields);

/* ip6_send() "fields" bits: */
#define  IP6F_FLOW   0x01     /* Set Flow control field */
#define  IP6F_LEN    0x02     /* Set length field from nb_plen */
#define  IP6F_HOPS   0x04     /* Set hops */
#define  IP6F_ROUTE  0x08     /* Set nexthop and iface */

/* popular combinations */
#define  IP6F_NOROUTE  (IP6F_FLOW | IP6F_LEN | IP6F_HOPS)
#define  IP6F_ALL    0xFF     /* Set all fields */

/* 
 * default value for defualt life time of prefix 
 * after a redirect to a router without 
 * an RA.
 */
#define IP6_DEFAULT_LIFETIME (TPS * 5)

#define IP6_2HOURSSECS (2 * 60 * 60)

/* Routine to demux packet to upper protocols */
int   ip6_demux(PACKET pkt, u_char type);

/* IPv6 routing table lookup */
struct ip6_inaddr * ip6_lookup(ip6_addr * findaddr, NET ifp);

/* logical prep for a net to do IPv6 (reg type, etc) */
int   ip6_device(struct net * ifp);

/* build ipv6 mcast addr based on MAC addr */
int   ip6_setmcast(struct net * ifp);

/* build the struct containing a new IPv6 address */
struct ip6_inaddr *  ip6_mkaddr(struct net * ifp, int type, ip6_addr * addr);

/* routine to add in the IPv6 pseudo header cksum to a passed cksum */
unshort   ip6_pseudosum(struct ipv6 *, u_long len, u_char nexthdr);

/* send icmp6 parameter message. */
void ip6_badparam(PACKET pip, int type, int code, u_long disp);

/* routines to process IPv6 extension headers */
int   ip6_routehdr(PACKET pkt, int hdr_disp);
int   ip6_tvlopt(PACKET pkt, int z, int hdr_disp);


/* fragmentation and reassembly */
int   ip6_fragment(PACKET pkt, int fields);
int   ip6_reasm(PACKET *pkt, int z, int hdr_disp);


/* upper protocol handlers for received v6 pkts */
int   udp6_rcv(PACKET pkt);
int   tcp6_rcv(PACKET pkt);

/* get path MTU to a IP6 host */
int   ip6_pmtulookup(ip6_addr * faddr, NET ifp);

/* get address of ours of matching type */
struct ip6_inaddr * ip6_mataddr(ip6_addr * addr, NET reqifp);

/* Formatting routines ip6addr <-> text */
char *   print_ip6( /* IN */ ip6_addr * addr, /* OUT */ char * str);

extern char * pton_error;     /* explanation of last inet_pton failure */

/* RFC 2462 RetransTimer, in cticks */
extern u_long ip6RetransTimer;

/* format string tables for printf()ing IPv6 info */
extern char * v6types[5];
extern char * ip6addrstates[4];  /* index by "IA_TENTATIVE", etc */

#ifndef IP6_ALLOC
#define IP6_ALLOC(size)    (struct ip6_inaddr *)npalloc(size)
#define IP6_FREE(ptr)      npfree(ptr)
#endif /* IP6_ALLOC */

unshort  cksum(void * buf, unsigned words16);   /* basic IP cksum */


/* IPv6 Generic tunnel configuration info. These should be set up for
 * each non-6to4 tunnel. If NV_PARMS is used they should be
 * set from that, otherwise the system init code should fill
 * these in before calling the prep_ routine. The IPv6 addresses
 * in this struct are assigned by your tunnel provider (or your
 * sys admin people if you ARE a tunnel provider). 
 */

/* Number of IPv6 tunnels (INCLUDING 6TO4) to be supported */
#ifndef MAXTUNNELS            /* can override from ipport.h_h */
#define MAXTUNNELS MAXNETS    /* max number of tunnels */
#endif   /* MAXTUNNELS */


struct tn_configinfo
{
   char *      ifname;     /* name of tunnel to set up (eg "tu0") */
   ip6_addr    lhost6;     /* local v6 address (assigned by tunnel provider) */
   int         lpflen;     /* local v6 addr prefix length */
   ip_addr     fhost4;     /* IPv4 address of tunnel provider */
};

/* The actual configs. Note that these are pointers. NULL is OK, but app
 * must provide the static memory for any tunnel if wants to configure.
 * These are matched to tunnel interfaces via the ifname field. Lower
 * interface to use may be set by if_bind() call, and defaults to first
 * Ethernet if not set.
 *
 * 6to4 does not need one of these.
 */
extern struct tn_configinfo * tunnelcfg[MAXTUNNELS];


/*
 * Various ipv6 timers. ip6_timer is invoked by the system once a second, and
 * it handles the others
 */
extern void ip6_timer(void);
extern void nd_timer(void);
extern void ping6_timer(void);
extern void ip6_fragcheck(void);


/* Routing functions. */

/* get local address relative to a remote address */
struct ip6_inaddr * ip6_myaddr(ip6_addr * dest, NET ifp);
/* get our source addr to mtach dest addr */
struct ip6_inaddr * ip6_matchmyaddr(ip6_addr * dest, NET ifp);

/* get iface, nexthop, & source. Main external entry to routing code. */
int   ip6_destif( PACKET pkt,
            ip6_addr * dest,
            ip6_addr ** source);


ip6_route * rt6_route(ip6_addr * dest);   /* basic route select */

ip6_route *rt6_addroute(
   ip6_addr *  dest,       /* ultimate destination */
   int         prefixlen,  /* 128 for exact route */
   NET         ifp);
   
 
int rt6_addprefix(ip6_addr * a1,  int prefixlen, NET ifp,    
   u_long   pmtu,          /* PMTU for this link */
   u_long   invalid_time,  /* time at which invalid, in ticks */
   u_long   life_time,     /* Preferred Lifetime */
   int  flags
   );

int   rt6_delroute(ip6_addr * dest, int pflen, NET ifp);

/* IPv6 logging macros */
#ifdef IPV6LOGING
extern char ipv6WrkBuffer[50];
#define IPV6TIMEZ    { \
	  IPV6LOG(("+ cticks %ld\n", cticks)); \
	}
#define IPV6DUMP(name, a,b) {dprintf(name); hexdump6(a,b); dprintf("+ EOD\n");}
#define IPV6LOG(text) dprintf ## text
#define IPV6CDUMP    nd_dump()
#else
#define IPV6LOG(text)
#define IPV6DUMP(name, a,b)
#define IPV6TIMEZ
#define IPV6CDUMP
#endif

/* List of constants defined in rfc2461 - use later */
/* Router constants: */
#define MAX_INITIAL_RTR_ADVERT_INTERVAL  16   /* seconds */
#define MAX_INITIAL_RTR_ADVERTISEMENTS    3   /* transmissions */
#define MAX_FINAL_RTR_ADVERTISEMENTS      3   /* transmissions */
#define MIN_DELAY_BETWEEN_RAS             3   /* seconds */
#define MAX_RA_DELAY_TIME                 .5  /* seconds */
/* Host constants: */
#define MAX_RTR_SOLICITATION_DELAY        1   /* seconds */
#define RTR_SOLICITATION_INTERVAL         4   /* seconds */
#define MAX_RTR_SOLICITATIONS             3   /* transmissions */
#define RTR_DEFAULT_LIFE                  (60 * 60 * 18)   /* seconds */
/* Node constants: */
#define MAX_MULTICAST_SOLICIT             3   /* transmissions */
#define MAX_UNICAST_SOLICIT               3   /* transmissions */
#define MAX_ANYCAST_DELAY_TIME            1   /* seconds */
#define MAX_NEIGHBOR_ADVERTISEMENT        3   /* transmissions */
#define REACHABLE_TIME               30000   /* milliseconds */
#define RETRANS_TIMER                 1000   /* milliseconds */
#define DELAY_FIRST_PROBE_TIME            5   /* seconds */
#define MIN_RANDOM_FACTOR                 (1 / 2)
#define MAX_RANDOM_FACTOR                 (3 / 2)
#define REASM_TIMEOUT                     (TPS * 60)


#endif   /* _IP6_H_ - whole file ifdef */


