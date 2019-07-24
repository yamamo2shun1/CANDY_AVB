/*
 * FILENAME: dns.h
 *
 * Copyright 1997- 2000 By InterNiche Technologies Inc. All rights reserved
 *
 * DNS defines for NetPort IP stack.
 *
 * MODULE: INET
 *
 *
 * PORTABLE: yes
 */


#ifndef _DNS_H_
#define  _DNS_H_  1

/* set defaults of DNS variables, may override in ipport.h */

#ifndef MAXDNSNAME
#define  MAXDNSNAME     256   /* max length of name including domain */
#endif

#ifndef MAXDNSUDP
#define  MAXDNSUDP      512   /* MAX allowable UDP size */
#endif

#ifndef MAXDNSSERVERS
#define  MAXDNSSERVERS  3     /* MAX number of servers to try */
#endif

#ifndef MAXDNSADDRS
#define  MAXDNSADDRS    10    /* MAX IP addresses to return via gethostbyname() */
#endif

#ifndef MAXDNSALIAS
#define  MAXDNSALIAS    2     /* MAX number of alias names */
#endif

#define  DNS_QUERY       0    /* Op code for query */
#define  DNS_UPDT        5    /* Op code for UPDATE request  */

/* DNS Record types */
#define  DNS_TYPE_QUERY    0     /* Type value for question record */
#define  DNS_TYPE_IPADDR   1     /* Type value for IPv4 address */
#define  DNS_TYPE_AUTHNS   2     /* Authoritative name server */
#define  DNS_TYPE_ALIAS    5     /* Alias for queried name */
#define  DNS_TYPE_PTR      12     /* Domain Name pointer */
#define  DNS_TYPE_AAAA     0x1c  /* IPv6 address ("AAAA" record) */

/* Description of data base entry for a single host.  */
struct hostent
{
   char *   h_name;        /* Official name of host. */
   char **h_aliases;       /* Alias list.  */
   int   h_addrtype;       /* Host address type.  */
   int   h_length;         /* Length of address.  */
   char **h_addr_list;     /* List of addresses from name server.  */
#define  h_addr   h_addr_list[0] /* Address, for backward compatibility.  */

#ifdef DNS_CLIENT_UPDT
   /* Extra variables passed in to Dynamic DNS updates. */
   char *   h_z_name;      /* IN- zone name for UPDATE packet */
   ip_addr  h_add_ipaddr;  /* IN- add this ip address for host name in zone */
   u_long   h_ttl;         /* IN- time-to-live field for UPDATE packet */
#endif /* DNS_CLIENT_UPDT */
};


/* basic internal structure of a client DNS entry. This one structure does double
 * duty as a request manager and database entry.
 */

struct dns_querys
{
   struct dns_querys * next;
   u_long   send_time;     /* ctick when last request was sent/received */
   u_long   expire_time;   /* second (local) when this data expires */
   unshort  tries;         /* retry count */
   unshort  lport;         /* local (client) UDP port, for verification */
   unshort  id;            /* ID of request, 0 == unused entry. */
   int      replies;       /* number of replys to current request */
   int      ipaddrs;       /* count of entries in ipaddr_list[] */
   ip_addr  ipaddr_list[MAXDNSADDRS];  /* IP addresses (net endian) */
   char *   addrptrs[MAXDNSADDRS];  /* pointers, for hostent.h_addr_list */
   int      err;                    /* last ENP_ error if, if any */
   int      rcode;                  /* last response code if replys > 1 */
   char     dns_names[MAXDNSNAME];  /* buffer of names (usually w/domain) */
   char     ptr_name[MAXDNSNAME];  /* The result of PTR query is stored here (just one for now) */
   ip_addr  auths_ip;               /* IPv4 addresses of 1st auth server */
   char *   alist[MAXDNSALIAS+1];   /* alias list, points into dns_names */

   /* Most DNS queries need a hostent structure to return the data to
    * the calling application; so we embed the hostent structure inside
    * the query structure - one less alloced buffer to keep track of.
    */
   struct hostent he;               /* for return from gethostbyname() */
   char     type;                   /* type of original query */

#ifdef IP_V6
   int      ip6addrs;                  /* count of entries in ip6_list */
   char     ip6_list[MAXDNSADDRS][16]; /* IPv6 address from answers */
   char     auths_ip6[16];             /* IPv6 addresses of 1st auth server */
#endif   /* IP_V6 */
};


/* List of DNS objects */
extern   struct dns_querys * dns_qs;

/* Macros to allocate & free DNS objects */
#ifndef DNC_ALLOC
#define DNC_ALLOC(size) (struct dns_querys *)npalloc(size)
#define DNC_FREE(ptr)   npfree((void *)ptr)
#endif

/* pending requests, can be used as a flag to spin dnsc_check() task */
extern   unsigned    dnsc_active;

/* DNS client statistics: */
extern   ulong    dnsc_errors;   /* protocol/implementation runtime errors */
extern   ulong    dnsc_requests; /* requests sent */
extern   ulong    dnsc_OK;       /* OK replys received */
extern   ulong    dnsc_unks;     /* error (unknown) replys received */
extern   ulong    dnsc_tmos;     /* timeouts */


/* DNS operational parameters which can be set at run time */
extern   ip_addr  dns_servers[MAXDNSSERVERS];

#ifdef   DNS_CLIENT_UPDT
extern   char     soa_mname[MAXDNSNAME];
#endif    /* DNS_CLIENT_UPDT */

extern   unsigned dns_firsttry;  /* time to first retry, in seconds */
extern   unsigned dns_trys;      /* max number of retrys */



/* header format of a DNS packet over UDP */
START_PACKED_STRUCT(dns_hdr)
   unshort  id;         /* 16 bit unique query ID */
   unshort  flags;      /* various bit fields, see below */
   unshort  qdcount;    /* entries in the question field */
   unshort  ancount;    /* resource records in the answer field */
   unshort  nscount;    /* name server resource records */
   unshort  arcount;    /* resource records in the additional records */
END_PACKED_STRUCT(dns_hdr)

#define  DNS_PORT    53    /* DNS reserved port on UDP */

/* DNS header flags field defines */
#define  DNSF_QR     0x8000   /* query (0), or response (1) */
#define  DNSF_OPMASK 0x7800   /* 4 bit opcode kinds of query, 0==standard */
#define  DNSF_AUTHOR 0x0400   /* authoritative */
#define  DNSF_RECUR  0x0100   /* recursion */
#define  DNSF_NONAME 0x0003   /* No such name. */ 
#define  DNSF_AA     0x0400   /* set if Authoritive Answers */
#define  DNSF_TC     0x0200   /* set if truncated message */
#define  DNSF_RD     0x0100   /* Recursion Desired bit */
#define  DNSF_RA     0x0080   /* Recursion Allowed bit */
#define  DNSF_Z      0x0070   /* 3 reserved bits, must be zero */
#define  DNSF_RCMASK 0x000F   /* Response Code mask */

/* Reponse Code values: */
#define  DNSRC_OK       0     /* good response */
#define  DNSRC_EFORMAT  1     /* Format error */
#define  DNSRC_ESERVER  2     /* Server Error */
#define  DNSRC_ENAME    3     /* Name error */
#define  DNSRC_EIMP     4     /* Not Implemented on server */
#define  DNSRC_EREFUSE  5     /* Server refused operation */
#define  DNSRC_UNSET    0xFF  /* No reponse yet (used only in dns_querys struct) */

#ifdef DNS_CLIENT_UPDT

/* Error codes used within Dynamic DNS Update operation */
#define  DNSRC_EDOMAIN  6     /* Some name that ought not to exist, exists */
#define  DNSRC_ERRSET   7     /* Some RRset that ought not to exist, exists*/
#define  DNSRC_ENRRSET  8     /* Some RRset that ought to exist, does not */
#define  DNSRC_NOTAUTH  9     /* Server is not authoritative for the zone */
                              /* named in the zone section */
#define  DNSRC_NOTZONE  10    /* A name used within the Update section */
                              /* is not within the zone denoted        */
#define  DNS_CLASS_NONE    254    /* CLASS NONE */

#endif /* DNS_CLIENT_UPDT */

/* DNS client external entry points: */
int   dns_query(char * name, ip_addr * ip);     /* start a DNS query */
int   dns_lookup(ip_addr * ip, char * name);    /* check query status */
void  dnsc_check(void); /* spin once a second to drive retrys & timeouts */

/* New (for v2.0) version of dns_query(), which takes a record type
 * argument and fills in a pointer to the dns_query structure.
 */
int   dns_query_type(char * name, char type, ip_addr ipaddr,
            unsigned long ttl, struct dns_querys ** dns_ptr);

/* flags for in_reshost(); */
#define  RH_VERBOSE     0x01  /* do informational printfs   */
#define  RH_BLOCK       0x02  /* block with call to tk_yield()  */
int   in_reshost(char * host, ip_addr * address, int flags);


/* Return entry from host data base for host with NAME. This could be 
 * in sockcall.h but is here because a non-TCP used may want DNS. 
 */
struct hostent * gethostbyname(char * name);
int nslookup(char * name, char type, struct dns_querys **dns_entry);

/* ....And the crufty v6 version (RFC2133).... */
struct hostent *  gethostbyname2(char * name, int af);

#endif   /* _DNS_H_ */


