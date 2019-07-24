/*
 * FILENAME: dnsclnt.c
 *
 * Copyright 1997- 2000 By InterNiche Technologies Inc. All rights reserved
 *
 * DNS Client code for NetPort IP stack
 *
 * MODULE: INET
 *
 * ROUTINES: dns_query(), dnc_sendreq(), dnc_copyout(), 
 * ROUTINES: dnc_copyin(), dnc_del(), dnc_new(), dns_check(), 
 * ROUTINES: badreply(), dns_upcall(), dnc_lookup(), dns_lookup(), 
 * ROUTINES: gethostbyname(), dns_state(), 
 *
 * PORTABLE: yes
 */


#include "ipport.h"

#ifdef DNS_CLIENT    /* ifdef out whole file */

#include <time.h>
#include "libport.h"
#include "q.h"
#include "netbuf.h"
#include "net.h"
#include "ip.h"
#include "udp.h"
#include "dns.h"
#ifndef MINI_IP
#include "nptcp.h"
#endif
#include "ns.h"

#ifdef IP_V6
#include "ip6.h"
#endif

#ifdef DNS_CLIENT_UPDT
#ifndef NS_DDNS
#define NS_DDNS
#endif
#include "nameser.h"
#endif

/*#define DEBUG_DNSCLNT_C*/

#define DEFAULT_TTL_TIME 300 /* seconds */
#define DNS_TTL_FACTOR 1 /* upscale TTL */
#define DNC_TIMEOUT_TICKS (10 * TPS)
#define DNS_QUERY_WAIT  1 /* seconds */
#define DNS_QUERY_RETRY 5

/* List for querie/database records. */
struct dns_querys * dns_qs = NULL;

unshort  dnsids   =  0x1234;  /* seed for unique request IDs */

/* DNS client statistics: */
ulong      dnsc_errors = 0;      /* protocol/implementation runtime errors */
ulong      dnsc_requests = 0;    /* requests sent */
ulong      dnsc_replies = 0;     /* replys received */
ulong      dnsc_good = 0;        /* usable replys received */
ulong      dnsc_tmos = 0;        /* timeouts */
ulong      dnsc_retry = 0;       /* total retries */

#ifdef DNS_CLIENT_UPDT
ulong     dnsc_updates = 0;     /* Dynamic DNS Updates sent */
ip_addr   dns_update_server;    /* IP addr of dynamic update server */
#endif /* DNS_CLIENT_UPDT */

/* pending requests, can be used as a flag to spin dns_check() task */
unsigned   dnsc_active = 0;

/* retry info, can be overwritten by application code */
unsigned dns_firsttry = 4; /* time to first retry, in seconds */
unsigned dns_trys = 5;     /* max number of retrys */

ip_addr dns_servers[MAXDNSSERVERS]; /* need to be set from NV parameters */

#ifdef  DNS_CLIENT_UPDT
char    soa_mname[MAXDNSNAME];

/* internal routines */
static struct hostent* getsoa(char *);
#endif  /* DNS_CLIENT_UPDT */

/* internal routines */
static int  dnc_sendreq(struct dns_querys *);
int  dnc_copyout(char * dest, char * src);
static int  dnc_copyin(char * dest, char * src, struct dns_hdr * dns);
static void dnc_del(struct dns_querys *);
static struct dns_querys * dnc_new(void);
static int  badreply(struct dns_hdr * dns, char * text);
static void dnc_set_answer(struct dns_querys *, unshort type, char * cp, int rdlen);
static int  dnc_lookup(char * name, struct dns_querys ** cacheentry);



#ifdef DEBUG_DNSCLNT_C

void dump_dns_qs(char *text)
{
   struct dns_querys *p;
   int i;
#ifdef IP_V6
   char ip6buf[40];     
#endif

   if (dns_qs == NULL)
   {
      printf("dump_dns_qs(): %s: dns_qs is NULL\n", text);
      return;
   }

   printf("dump_dns_qs(): %s\n", text);
   for (p = dns_qs; p; p = p->next)
   {
      printf("dns_names = %s send_time = %lu, expire_time = %lu\n", 
             p->dns_names, p->send_time, p->expire_time);
      printf("tries = %lu lport = %lu, id = %lu, replies = %lu \n", 
             p->tries, p->lport, p->id, p->replies);
#ifdef IP_V6
      printf("ipaddrs = %lu ip6addrs = %lu, type = %lu, err = %lu rcode = %lu\n", 
             p->ipaddrs, p->ip6addrs, p->type, p->err, p->rcode);
#else
      printf("ipaddrs = %lu type = %lu, err = %lu rcode = %lu\n", 
             p->ipaddrs, p->type, p->err, p->rcode);
#endif
      printf("auths_ip = %u.%u.%u.%u\n", PUSH_IPADDR(p->auths_ip));
#ifdef IP_V6
      printf("auths_ip6 = %s\n", print_ip6((struct in6_addr *)(p->auths_ip6), ip6buf));
#endif
      if (p->ipaddrs)
      {
         printf("IPv4 addresses: (ipaddr_list)\n");
         for (i = 0; i < p->ipaddrs; i++)
         {
            printf("\t%u.%u.%u.%u\n", PUSH_IPADDR(p->ipaddr_list[i]));
         }
      }
#ifdef IP_V6
      if (p->ip6addrs)
      {
         printf("IPv6 addresses: (ip6_list)\n");
         for (i = 0; i < p->ip6addrs; i++)
         {
            printf("\t%s\n", print_ip6((struct in6_addr *)(&p->ip6_list[i][0]), ip6buf));
         }
      }
#endif
   }
}

#endif



void reverse_string(char * s)
{
   int c, i, j;
   for (i = 0, j =strlen(s)-1; i<j; i++, j--)
   {
      c = s[i];
      s[i] = s[j];
      s[j] = c;
   }
} 



void reverse_domain_addr(char *dest, char *src)
{
   int len;
   int ti, i;
   char temp[255];

   dest[0] = '\0';
   len = strlen(src);
   ti = 0;
   for (i = len - 1; i >= 0; i--)
   {
      if (src[i] == '.')
      {
         temp[ti] = '\0';
         reverse_string(temp);
         strcat(dest, temp);
         strcat(dest, ".");
         ti = 0;
      }
      else
      {
         temp[ti] = src[i];
         ti++;
      }
   }
   temp[ti] = '\0';
   reverse_string(temp);
   strcat(dest, temp);
}



/* FUNCTION: dns_query()
 *
 * dns_query() - starts the process of sending out a DNS request for 
 * a domain name. This is hardwired for the basic question "what is 
 * the IP address of named host?". It creates the control structures 
 * and calls dnc_sendreq() to send the actual packet. Caller should 
 * poll dns_lookup for results. 
 *
 * Old version, for backward API compatibility. This is implemented
 * as a wrapper for the new function
 *
 *
 * PARAM1: char * name
 * PARAM2: ip_addr * ip
 *
 * RETURNS: Returns 0 if IP address was filled in 
 * from cache, ENP_SEND_PENDING if query was launched OK, or one of 
 * the ENP error codes. 
 */

int
dns_query(char *name, ip_addr *ip_ptr)
{
   int      err;
   struct dns_querys * dns_entry;

   /* Do query for an "A" record (DNS_TYPE_IPADDR) */
   err = dns_query_type(name, DNS_TYPE_IPADDR,
                        (ip_addr)0, (unsigned long)0, &dns_entry);

   if ((err == 0) && (ip_ptr != (ip_addr *)NULL))
      *ip_ptr = dns_entry->ipaddr_list[0];

   return err;
}



/* FUNCTION: dns_query_type()
 *
 * New (for v2.0) version of dns_query()
 * This creates the control structures and calls dnc_sendreq() to
 * send the DNS query packet. Caller should 
 * poll dns_lookup for results. 
 *
 *
 * PARAM1: char * name - IN - host name to look up
 * PARAM2: char type - IN - type of query (V4, V6, SERVER, etc.)
 * PARAM3: ip address. (for updates) 
 * PARAM4: ttl (for updates) 
 * PARAM5: dns_querys ** dnsptr - OUT - dns entry 
 *
 * RETURNS: Returns 0 if IP address was filled in 
 * from cache, ENP_SEND_PENDING if query was launched OK, or one of 
 * the ENP error codes. 
 */

int
dns_query_type(char * name, char type, ip_addr ipaddr,
               unsigned long ttl, struct dns_querys ** dns_ptr)
{
   struct dns_querys *  dns_entry;
   int   e;

   /* see if we already have an entry for this name in the cache */
   e = dnc_lookup(name, dns_ptr);
   if (e == 0) /* no error */
   {
      return 0;
   }
   else if (e == ENP_SEND_PENDING)   /* waiting for previous query */
      return e;   
   
   /* else find a free entry so we can start a query */
   dns_entry = dnc_new();
   if (dns_entry == NULL)
      return ENP_RESOURCE;

   /* prepare entry for use and copy in name. The name for the query will
    * be replaced by the reply, and other strings (alias, server name) may
    * be added to the buffer if room permits.
    */
   strncpy(dns_entry->dns_names, name, MAXDNSNAME-1);  /* copy in name */

   dns_entry->tries = 0;      /* no retries yet */
   dns_entry->id = dnsids++;  /* set ID for this transaction */
   if (dnsids == 0)              /* don't allow ID of 0 */
      dnsids++;

   dns_entry->ipaddr_list[0] = 0L;  /* non-zero when it succeeds */

   /* get UDP port for packet, keep for ID */
   dns_entry->lport = udp_socket();
   dnsc_active++;       /* maintain global dns pending count */
   dnsc_requests++;     /* count this request */

#ifdef DNS_CLIENT_UPDT
   /* load up input parameters for DNS UPDATE request */
   dns_entry->he.h_z_name = name;
   dns_entry->he.h_add_ipaddr = ipaddr;
   dns_entry->he.h_ttl = ttl;
#endif  /* DNS_CLIENT_UPDT */

   dns_entry->type = type;    /* set type of DNS query */
   *dns_ptr = dns_entry;      /* return DNS entry */

   e = dnc_sendreq(dns_entry);
   if (e == 0) /* first packet sent OK */
      return ENP_SEND_PENDING;
   else  /* some kind of error */
      return e;

   USE_ARG(ttl);
   USE_ARG(ipaddr);
}



/* FUNCTION: dnc_sendreq()
 *
 * dnc_sendreq() - Sends out a DNS request packet based on the query 
 * info in dns_entry. This is intended to be called from 
 * dns_query() or dns_check(). 
 *
 * 
 * PARAM1: int entry
 *
 * RETURNS: Returns 0 if OK, or one of the ENP error codes. 
 */

static int
dnc_sendreq(struct dns_querys * dns_entry)
{      
   PACKET pkt;
   struct dns_hdr *  dns;  /* outgoing dns header */
   char *   cp;   /* scratch pointer for building question field */
   int   server;  /* index of server to use */
   
   
   /* figure out which server to try */
   for (server = 0; server < MAXDNSSERVERS; server++)
   {
      if (dns_servers[server] == 0L)
         break;
   }
   if (server == 0)  /* no servers set? */
   {
      dprintf("No DNS servers configured\n");
      dtrap();
      return ENP_LOGIC;
   }
   server = dns_entry->tries % server;

   /* allocate packet for DNS request */
   if ((pkt = udp_alloc(MAXDNSUDP, 0)) == NULL)
      return ENP_RESOURCE;

   /* fill in DNS header. Most fields are cleared to 0s */
   dns = (struct dns_hdr *)pkt->nb_prot;
   MEMSET(dns, 0, sizeof(struct dns_hdr));
   dns->id = dns_entry->id;

#ifdef DNS_CLIENT_UPDT
   /* If this is an UPDATE packet, format the DNS packet differently */
   
   if (dns_entry->type == DNS_UPDT)
   {
	  char * domP = NULL;

      dns->qdcount = htons(1);      /* Set zone count to 1 */
      dns->nscount = htons(1);      /* Set update count to 1 */
      dns->flags = htons(0x2800);   /* Set opcode field to 5 */

      /* format zone name into UPDATE packet */
      cp = (char*)(dns + 1);      /* point at next byte past header */

      
	   /*
		* get domain name from host name.
		*/
	   if ((domP = strstr(dns_entry->he.h_z_name, ".")) == NULL)
	   {
         badreply(dns, "bad domain name.\n");
         return -1;	   
	   }	  
	  
	  cp += dnc_copyout(cp, domP + 1);

      /* finish off zone section. We write these two 16 bit words a
       * byte at a time since cp may be on an odd address and some
       * machines
       */
      *cp++ = 0;  /* high byte of type */
      *cp++ = 6;  /* type 6 = soa */
      *cp++ = 0;  /* high byte of class */
      *cp++ = 1;  /* class 1 == internet */


      /* Put in NAME and TYPE */
	  /* host name */
      cp += dnc_copyout(cp, dns_entry->he.h_z_name);
      *cp++ = 0;  /* high byte of type */
      *cp++ = 1;  /* type 1 == host address, type 6 = soa */
      *cp++ = 0;     /* high byte of class */
	  /* is it a delete? */
      if (dns_entry->he.h_ttl == 0)
         *cp++ = (char)DNS_CLASS_NONE;  /* class = NONE */
      else      
		 *cp++ = 1;     /* class 1 == internet */

      /* Put in TTL value */
      *cp++ = (unsigned char)(dns_entry->he.h_ttl >> 24);
      *cp++ = (unsigned char)(dns_entry->he.h_ttl >> 16);
      *cp++ = (unsigned char)(dns_entry->he.h_ttl >> 8);
      *cp++ = (unsigned char)dns_entry->he.h_ttl;

      /* Put in RDLENGTH which is length of ip address ie 4 */
      *cp++ = 0;  /* low byte of length */
      *cp++ = 4;
      /* Put in IP address */
      MEMCPY(cp, &dns_entry->he.h_add_ipaddr, 4);
      cp += 4;

      dnsc_updates++;
   }
   else
   {
#endif  /* DNS_CLIENT_UPDT */


      dns->qdcount = htons(1);      /* 1 question */
      dns->flags = htons(DNSF_RD);  /* Recursion Desired */

      /* format name into question field after header */
      cp = (char*)(dns + 1);  /* point at next byte past header */
      cp += dnc_copyout(cp, dns_entry->dns_names);

      /* finish off question field. We write these two 16 bit words a 
       * byte at a time since cp may be on an odd address and some 
       * machines 
       */
      *cp++ = 0;  /* high byte of type */
      *cp++ = dns_entry->type;  /* type 1 == host address, type 6 = soa */
      *cp++ = 0;  /* high byte of class */
      *cp++ = 1;  /* class 1 == internet */

#ifdef DNS_CLIENT_UPDT  
   }
   pkt->fhost = dns_update_server;

#else
   pkt->fhost = dns_servers[server];
#endif /* DNS_CLIENT_UPDT */
   pkt->nb_plen = cp - (char*)dns;     /* length of packet */
   dns_entry->send_time = cticks;   /* note time we send packet */

   return (udp_send(DNS_PORT, dns_entry->lport, pkt));
}



#ifdef DNS_CLIENT_UPDT

static char DDNSZone[MAXDNSNAME];

/* FUNCTION: dns_sendDDNSReq()
 * 
 * Create and send a DDNS request
 *
 * PARAM1: char *             pointer to zone name
 * PARAM2: int                length of prerequisites
 * PARAM3: int                number of prerequisites
 * PARAM4: char *             pointer to prerequisites
 * PARAM5: int                length of updates.
 * PARAM6: int                number of updates
 * PARAM7: char *             pointer to updates
 *
 * RETURNS: int               0 if OK, or one of the ENP_ error codes. 
 *
 * Formats and sends a full DDNS req.
 * Only called by nsddnstest() at the moment.
 */

int
dns_sendDDNSReq(char *zone, int lenPreqs, int numPreqs,
                char *preqsP, int lenUps, int numUps, char *upsP)
{      
   PACKET pkt;
   struct dns_hdr *  dns;  /* outgoing dns header */
   char *   cp;   /* scratch pointer for building question field */
   int   server;  /* index of server to use */
   struct dns_querys *  dns_entry;
 
   /* else find a free entry so we can start a query */
   dns_entry = dnc_new();
   if (dns_entry == NULL)
      return -1;

   /* prepare entry for use and copy in name. The name for the query will
    * be replaced by the reply, and other strings (alias, server name) may
    * be added to the buffer if room permits.
    */
   strncpy(dns_entry->dns_names, zone, MAXDNSNAME-1);  /* copy in zone */
   strncpy(DDNSZone, zone, MAXDNSNAME-1);  /* save zone */

   dns_entry->tries = 0;      /* no retries yet */
   dns_entry->id = dnsids++;  /* set ID for this transaction */
   if (dnsids == 0)              /* don't allow ID of 0 */
      dnsids++;

   dns_entry->ipaddr_list[0] = 0L;  /* non-zero when it succeeds */

   /* get UDP port for packet, keep for ID */
   dns_entry->lport = udp_socket();
   dnsc_active++;       /* maintain global dns pending count */
   dnsc_requests++;     /* count this request */

   /* load up input parameters in case it's DNS UPDATE request */
   dns_entry->he.h_z_name = DDNSZone;
   dns_entry->he.h_add_ipaddr = 0;
   dns_entry->he.h_ttl = 0;
   dns_entry->send_time = cticks;   /* note time we send packet */

   dns_entry->type = NS_UPDATE;    /* set type of DNS query */

   /* allocate packet for DNS request */
   if ((pkt = udp_alloc(MAXDNSUDP, 0)) == NULL)
      return ENP_RESOURCE;

   /* fill in DNS header. Most fields are cleared to 0s */
   dns = (struct dns_hdr *)pkt->nb_prot;
   MEMSET(dns, 0, sizeof(struct dns_hdr));
   
   dns->qdcount = htons(1);      /* Set zone count to 1 */
   dns->ancount = htons(numPreqs);    /* Set number of prerequisites */
   dns->nscount = htons(numUps);      /* number of updates */
   dns->flags = htons(0x2800);   /* Set opcode field to 5 */

   /* format zone name into UPDATE packet */
   cp = (char *)(dns + 1);      /* point at next byte past header */	  
  
   cp += dnc_copyout(cp, zone);

   /* finish off zone section. We write these two 16 bit words a
    * byte at a time since cp may be on an odd address and some
    * machines
    */
   *cp++ = 0;  /* high byte of type */
   *cp++ = 6;  /* type 6 = soa */
   *cp++ = 0;  /* high byte of class */
   *cp++ = 1;  /* class 1 == internet */

   /* put prereqs. */
   MEMCPY(cp, preqsP, lenPreqs);
   cp += lenPreqs;

   /* put updates. */
   memcpy(cp, upsP, lenUps);
   cp += lenUps;

   pkt->fhost = dns_update_server;

   pkt->nb_plen = cp - (char *)dns;     /* length of packet */
   
   return (udp_send(DNS_PORT, dns_entry->lport, pkt));
}

#endif  /* DNS_CLIENT_UPDT */



/* FUNCTION: dnc_copyout()
 *
 * dnc_copyout() - copy a domain name from user "dot" text format to 
 * DNS header format. dest buffer is assumed to be large enough, 
 * which means at least as big as src string.
 * 
 * PARAM1: char * dest
 * PARAM2: char * src
 *
 * RETURNS:  Returns length of string copied. 
 */

int
dnc_copyout(char * dest, char * src)
{
   int   namelen; /* length of name segment */
   char *   cp;   /* pointer to dest string we're building */
   char *   fld, *   next; /* pointers into src */

   cp = dest;
   next = src; /* will be assigned to "this" on first pass */

   while (next)   /* 1 loop per field in dot-delimited string */
   {
      fld = next;
      next = strchr(fld, '.');   /* find next dot in string */
      if (next)   /* if not last field... */
      {
         namelen = next - fld;   /* length of this field */
         next++;  /* bump next pointer past dot */
      }
      else  /* no dot means end of string */
         namelen = strlen(fld);

      *cp++ = (char)(namelen & 0xFF);  /* put length in dest buffer */
      MEMCPY(cp, fld, namelen);     /* follow with string data */
      cp += namelen;    /* point past name to next length byte */
   }
   *cp++ = 0;  /* null terminate string */
   return (cp - dest);
}



/* FUNCTION: dnc_copyin()
 *
 * dnc_copyin() - the inverse of copyout above - it copies a dns 
 * format domain name to "dot" formatting. 
 *
 * 
 * PARAM1: char * dest
 * PARAM2: char * src
 * PARAM3: struct dns_hdr * dns
 *
 * RETURNS: Returns length of string copied, 0 if error. 
 */

int
dnc_copyin(char * dest, char * src, struct dns_hdr * dns)
{
   unshort   namelen; /* length of name segment */
   char *   cp;   /* pointer to dest string we're building */
   char *   fld, *   next; /* pointers into src */
   int   donelen;    /* number of bytes moved */
   unshort offset;

   cp = dest;
   next = src; /* will be assigned to "this" on first pass */
   donelen = 0;

   while (next)   /* 1 loop per field in dot-delimited string */
   {
      fld = next;
      namelen = *fld++;
      if (namelen == 0)
      break;   /* done */
      if ((namelen & 0xC0) == 0xC0)
      {
         fld--;
         offset = (unshort)*fld; /* get first byte of offset code */
         fld++;
         offset &= 0x3f;             /* mask our high two bits */
         offset <<= 8;               /* make it high byte of word */
         offset += (unshort)*fld; /* add low byte of word */
         fld = offset + (char *)dns;  /* ptr into domain name */
         namelen = *fld++;
      }
      if (namelen + donelen > MAXDNSNAME) /* check for overflow */
         return 0;   /* error */
      MEMCPY(cp, fld, namelen);
      donelen += (namelen+1); /* allow for dot/len byte */
      cp += namelen;
      *cp++ = '.';
      next = fld + namelen;
   }
   *(cp-1) = 0;   /* null terminate string (over last dot) */
   return donelen + 1;  /* include null in count */
}



/* FUNCTION: dnc_del()
 *
 * Delete a DNS entry
 *
 * PARAM1: dns_querys *       ptr to entry to delete
 *
 * RETURN: none
 */

static void
dnc_del(struct dns_querys *entry)
{
   struct dns_querys *tmp, *last;

#ifdef DEBUG_DNSCLNT_C
   dump_dns_qs("dnc_del()");
#endif

   /* find passed dns entry in list */
   last = (struct dns_querys *)NULL;

   LOCK_NET_RESOURCE(NET_RESID);
   for (tmp = dns_qs; tmp; tmp = tmp->next)
   {
      if (tmp == entry)  /* found entry in list */
      {
         if (last)          /* unlink */
            last->next = tmp->next;
         else
            dns_qs = tmp->next;
         break;
      }
      last = tmp;
   }
   UNLOCK_NET_RESOURCE(NET_RESID);

   DNC_FREE(entry);  /* free the memory */
   dnsc_active--;    /* one less active entry */
}



/* FUNCTION: dnc_ageout()
 *
 * See if we can age out any of the DNS entries.
 * 
 * PARAM1: void
 *
 * RETURNS: void
 */

static void
dnc_ageout()
{
   struct dns_querys *dns_entry, *tmpp;

   LOCK_NET_RESOURCE(NET_RESID);
   /* See if we can expire any old entries */
   for (dns_entry = dns_qs; dns_entry; dns_entry = tmpp)
   {
      tmpp = dns_entry->next;
      if ((dns_entry->ipaddrs != 0)
#ifdef IP_V6
          || (dns_entry->ip6addrs != 0)
#endif
         )
      {
         /* If entry has expired then delete it. */
         if ((cticks - dns_entry->send_time) > (dns_entry->expire_time * TPS))
         {
            dnc_del(dns_entry);
         }
      }
   }
   UNLOCK_NET_RESOURCE(NET_RESID);
}



/* FUNCTION: dnc_new()
 *
 * Create a new DNS cache entry
 *
 * PARAMS: none
 *
 * RETURN: dns_querys *       ptr to new entry or NULL
 */

static struct dns_querys *
dnc_new(void)
{
   struct dns_querys * dns_entry;

   dns_entry = DNC_ALLOC(sizeof(struct dns_querys));  /* get new entry */
   if (dns_entry)
   {
       /* set respose code to an illegal value. Response will overwrite */
       dns_entry->rcode = DNSRC_UNSET;

       /* set legacy hostent alias pointer to dns_entry alias list */
       dns_entry->he.h_aliases = &dns_entry->alist[0];

       /* Put new entry in the DNS database/request list */
       LOCK_NET_RESOURCE(NET_RESID);
       dns_entry->next = dns_qs;
       dns_qs = dns_entry;
       UNLOCK_NET_RESOURCE(NET_RESID);
   }

   return dns_entry;
}



/* FUNCTION: dns_check()
 *
 * Periodic check for DNS client activity
 *
 * PARAMS: none
 *
 * RETURN: none
 *
 * Poll once a second to drive DNS timeouts and 
 * retrys. Can be skipped when dnsc_active == 0 
 */

void
dns_check(void)
{
   struct dns_querys * dns_entry;
   int   trysec;  /* seconds to wait for next try */

   dnc_ageout();     /* age out expired entries */

   for (dns_entry = dns_qs; dns_entry; dns_entry = dns_entry->next)
   {
      if (dns_entry->id == 0) /* skip non-active request entrys */
         continue;
      
      /* If we already have a reply we like, don't send */
      if (dns_entry->rcode == DNSRC_OK)
         continue;
      
      /* If it's a name error then punt the request if it is
         over 10 seconds old */
      if ((dns_entry->rcode == DNSRC_ENAME) &&
          ((cticks - dns_entry->send_time) > DNC_TIMEOUT_TICKS))
      {
         goto timeout;
      }

      /* it is an active request; see if it's time to retry */
      trysec = (dns_firsttry << dns_entry->tries);
      if ((cticks - dns_entry->send_time) > (TPS * (unsigned long)trysec))
      {
         if (dns_entry->tries >= dns_trys)   /* retried out */
         {
timeout:
            LOCK_NET_RESOURCE(NET_RESID);
            dnc_del(dns_entry);
            UNLOCK_NET_RESOURCE(NET_RESID);
            dnsc_tmos++;      /* count timeouts */
            /* After a timeout we return because the list is now altered.
             * We'll process the rest of it on the next time tick.
             */
            return;
         }
         dnsc_retry++;           /* count total retries */
         dns_entry->tries++;  /* count entry retries */		 
		 
         dnc_sendreq(dns_entry);
      }
   }
}



/* FUNCTION: badreply()
 *
 * dns_badreply() - per-port handler for less than ideal DNS replys. 
 * 
 * 
 * PARAM1: struct dns_hdr * dns
 * PARAM2: char * text
 *
 * RETURNS: Returns ENP code if problem should kill transaction, else 0 
 */

static int 
badreply(struct dns_hdr *dns, char *text)
{
   dnsc_errors++;
#ifdef NPDEBUG
   dprintf("DNS error: %s; flags %x\n", text, htons(dns->flags));
#endif   /* NPDEBUG */
   /* don't kill request, let it retry with another server */

   USE_ARG(dns);
   USE_ARG(text);

   return 0;
}



static
char *
getshort(char *cp, unshort *val)
{
   unsigned char *ucp = (unsigned char *)cp;

   *val = (unshort)((*ucp << 8) | (*(ucp+1)));
   return (cp+2);
}



static
char *
getlong(char *cp, u_long *val)
{
   unsigned char *ucp = (unsigned char *)cp;

   *val = (u_long)((*(ucp)   << 24) |
                   (*(ucp+1) << 16) |
                   (*(ucp+2) <<  8) |
                   (*(ucp+3)));
   return (cp+4);
}



static char *
getoffset(char * cp, char * dns, unshort * offsetp)
{
   unshort  offset;

   /* bump past name field in answer, keeping offset to text */
   if ((*cp & 0xC0) == 0xC0)  /* is it an offset? */
   {
      offset = (unshort)(*cp++); /* get first byte of offset code */
      offset &= 0x3f;   /* mask our high two bits */
      offset <<= 8;     /* make it high byte of word */
      offset += (unshort)(*cp++);   /* add low byte of word */
   }
   else  /* text for name is right here */
   {
      offset = (unshort)(cp - dns);   /* save offset */
      while (*cp++ != 0);  /* scan past name */
   }
   *offsetp = offset;
   return cp;
}



static void
dnc_set_answer(struct dns_querys * dns_entry, unshort type, char * cp, int rdlen)
{
   int   addrx;      /* index into address lists */
   int i;

   /* save reply IP addresses in array of IP addresses so long
    * as there is room for them.
    */
   if (type == DNS_TYPE_PTR)
   {
      /* The first character usually is STX? */
      MEMCPY(dns_entry->ptr_name, cp + 1, rdlen -1);
      dns_entry->ptr_name[rdlen-1] = '\0';
      /* replace STX and ETX charachters */
      for (i = 0; i < (int)(strlen(dns_entry->ptr_name) - 1); i++)
      {
         if (dns_entry->ptr_name[i] <= 0x1F)
         {
            dns_entry->ptr_name[i] = '.';
         }
      }
      return;
   }
#ifdef IP_V4
   if (type == DNS_TYPE_IPADDR)
   {
      if (dns_entry->ipaddrs < MAXDNSADDRS)
      {
         addrx = dns_entry->ipaddrs++;      /* set index */
         MEMCPY(&dns_entry->ipaddr_list[addrx], cp, 4);
      }
      return;
   }
#endif   /* IP_V4 */
#ifdef IP_V6
   if (type == DNS_TYPE_AAAA)     /* IPv6 address */
   {
      if (dns_entry->ip6addrs < MAXDNSADDRS)
      {
         addrx = dns_entry->ip6addrs++;      /* set index */
         MEMCPY(&dns_entry->ip6_list[addrx], cp, 16);
      }
      return;
   }
#endif   /* IP_V6 */
}



/* FUNCTION: dnc_savename()
 *
 * Save a passed name in the passed dns_query structure. name is given
 * via an offset into the dns packet.
 *
 * 
 * PARAM1: dns_query structure to add name to
 * PARAM2: pointer to dns header
 * PARAM3: offset into dns header for name info
 * PARAM3: TRUE if name is an alias
 *
 * RETURNS: void
 */

void
dnc_savename(struct dns_querys *  dns_entry,
             struct dns_hdr * dns, 
             int offset,
             int aliasflag)
{
   char * cp;     /* pointer to dns_names[] name buffer */

   /* find next available space in name buffer */
   cp = dns_entry->dns_names;
   while (*cp)
   {
      if (*cp)
         cp += (strlen(cp) + 1); /* end of name (if any) */

      /* check for buffer overflow */
      if (cp >= &dns_entry->dns_names[MAXDNSNAME])
      {
         dtrap();    /* ran out of buffer space for names */
         return;
      }
      if (*cp == 0)
         break;
   }

   /* copy dns-format name into buffer as text */
   dnc_copyin(cp, offset + (char*)(dns), dns);

   /* Set pointer(s) in dns structures to new name */
   if (aliasflag)
   {
      int alias;     /* alias name index */
      /* Set the next alias pointer. First we have to find out
       * how many aliases are already in dns_entry.
       */
      for (alias = 0; alias < MAXDNSALIAS; alias++)
      {
         if (dns_entry->alist[alias] == NULL)
            break;
      }
      if (alias >= MAXDNSALIAS)
          return;
      /* set alias pointer to alias name in dns_names[] buffer */
      dns_entry->alist[alias] = cp;
   }
   else  /* set the name pointer(s) */
   {
      /* The hostent name field always points to dns_name */
      dns_entry->he.h_name = cp;
   }
   return;
}



/* FUNCTION: dns_upcall()
 *
 * dns_upcall() - called from the UDP layer whenever we receive a DNS 
 * packet (one addressed to port 53). p_nb_prot points to UDP data, 
 * which should be DNS header. Return 0 if OK, or ENP code. 
 *
 * 
 * PARAM1: PACKET pkt
 * PARAM2: unshort lport
 *
 * RETURNS: 
 */

int
dns_upcall(PACKET pkt, unshort lport)
{
   int      i;          /* record index */
   char *   cp;         /* scratch pointer for building question field */
   unshort  flags;      /* dns->flags in local endian */
   unshort  rcode;      /* response code */
   unshort  queries;    /* number of question records in reply */
   unshort  answers;    /* number of answer records in reply */
   unshort  records;    /* total records in reply */
   unshort  type;       /* various fields from the reply */
   unshort  netclass;   /* class of net (1 == inet) */
   u_long   ttl;        /* records time to live */
   unshort  rdlength;   /* length of record data */
   unshort  offset;     /* scratch offset to domain name text */
   unshort  nameoffset = 0;   /* offset to name in query */
   unshort  authoffset = 0;   /* offset to first auth server name */
   struct dns_hdr *  dns;     /* incoming dns header */
   struct dns_querys *dns_entry = NULL;

   dnsc_replies++;
   dns = (struct dns_hdr *)pkt->nb_prot;

#ifdef DEBUG_DNSCLNT_C
   dump_dns_qs("dns_upcall() 1");
#endif

   for (dns_entry = dns_qs; dns_entry; dns_entry = dns_entry->next)
   {
      if ((dns_entry->id == dns->id) &&
          (dns_entry->lport == lport))
      {
         break;
      }
   }

   /* make sure we found our entry in the table */
   if (dns_entry == NULL)
      return ENP_NOT_MINE;         /* do not free pkt here */

   dns_entry->replies++;           /* count total replies */

   /* If we already have a reply we liked then punt this one */
   if (dns_entry->rcode == DNSRC_OK)
   {
      return 0;
   }
   
   flags = htons(dns->flags);      /* extract data fromheader */
   rcode = flags & DNSF_RCMASK;    /* Response code */
   queries = htons(dns->qdcount);    /* number of questions */
   answers = htons(dns->ancount);  /* number of answers */
   records = queries + answers + htons(dns->nscount) + htons(dns->arcount);

   /* check DNS flags for good response value */
   if (!(flags & DNSF_QR))
      return (badreply(dns, "not reply"));

   /* looks like we got an answer to the query */
   dns_entry->rcode = rcode;

#ifdef DNS_CLIENT_UPDT
  {
     int opcode = (flags & DNSF_OPMASK) >> 11;   /* Op code */

     /* If UPDATE opcode set the ip address field to non zero and return */
     if (opcode == DNS_UPDT)
     {
        dns_entry->ipaddr_list[0] = 0xff;
        return 0;
     }
  }
#endif /* DNS_CLIENT_UPDT */

   if (rcode != DNSRC_OK) 
   {
      unshort ecode = (rcode & (DNSF_AUTHOR | DNSF_RECUR | DNSF_NONAME));
      
      if (!ecode)
         return (badreply(dns, "bad response code"));
   }

   if ((answers < 1) && !(rcode & DNSF_NONAME))
      return (badreply(dns, "no answers"));
   cp = (char*)(dns+1);    /* start after DNS entry */
   /* Since we're going to store new DNS info, clean out the old info */
   MEMSET(dns_entry->alist, 0, sizeof(dns_entry->alist));
   dns_entry->ipaddrs = 0;
#ifdef IP_V6
   dns_entry->ip6addrs = 0;
   MEMSET(dns_entry->ip6_list, 0, sizeof(dns_entry->ip6_list));
   MEMSET(dns_entry->auths_ip6, 0, sizeof(dns_entry->auths_ip6));
#endif   /* IP_V6 */
   /* loop through remaining records - answers, auth, and RRs */
   for (i = 0; i < records; i++)
   {
      /* Get offset to record's name */
      cp = getoffset(cp, (char *)dns, &offset);
      /* get records type and class from packet bytes */
      cp = getshort(cp, &type);
      cp = getshort(cp, &netclass);
      if (netclass != 1)
         return badreply(dns, "class not internet");
      if (i < queries)  /* just skip of echos question record */
         continue;
      /* Get the Time and data-length */
      cp = getlong(cp, &ttl);       /* 4 byte time to live field */
      cp = getshort(cp, &rdlength); /* length of record data */
      switch (type)
      {
      case DNS_TYPE_IPADDR:   /* IPv4 address for a name */
      case DNS_TYPE_PTR:  /* alias */
#ifdef IP_V6
      case DNS_TYPE_AAAA:     /* IPv6 address */
#endif   /* IP_V6 */
         if ((type == DNS_TYPE_IPADDR) && (rdlength != 4))
            return (badreply(dns, "IPADDR len not 4"));
         dnsc_good++;
         if (i < (queries + answers))   /* If record is an answer... */
         {
            if (nameoffset == 0)
            {
               /* save first answer name over request name */
               nameoffset = offset;

               /* save the name in the local DNS structure */
               dnc_savename(dns_entry, dns, offset, 0);
            }
            dnc_set_answer(dns_entry, type, cp, rdlength);    /* save the answer */
            dns_entry->expire_time = ttl * DNS_TTL_FACTOR; 
            if (dns_entry->expire_time == 0)
            {
               dns_entry->expire_time = DEFAULT_TTL_TIME * DNS_TTL_FACTOR;
            }
         }
         else  /* auth record or additional record */
         {
            if (offset == nameoffset)   /* another addr for query? */
            {
               dnc_set_answer(dns_entry, type, cp, rdlength);
            }
            else if (offset == authoffset) /* auth server IP address */
            {
               if (type == DNS_TYPE_IPADDR)
                  MEMCPY(&dns_entry->auths_ip, cp, 4);
#ifdef IP_V6
               if (type == DNS_TYPE_AAAA)
                  MEMCPY(&dns_entry->auths_ip6, cp, 16);
#endif   /* IP_V6 */
            }
         }
         break;
      case DNS_TYPE_AUTHNS: /* authoritative name server */
         /* What we really want for the name server is it's IP address,
          * however this record only contains a name, ttl, etc. We save 
          * the offset to the name, hoping that one of the additional 
          * records will have the IP address matching this name.
          */
         if (authoffset == 0)  /* only save first one */
            authoffset = cp - (char*)dns;
         break;
      case DNS_TYPE_ALIAS:  /* alias */
         /* save name in dns rec as an alias */
         dnc_savename(dns_entry, dns, offset, 1);
         break;
      default:       /* unhandled record type, ignore it. */
         break;
      }
      cp += rdlength;   /* bump past trailing data to next record */
   }
#ifdef DEBUG_DNSCLNT_C
   dump_dns_qs("dns_upcall() 2");
#endif
   return 0;
}



/* FUNCTION: dnc_lookup()
 *
 * dnc_lookup() - check cache for a pending or completed DNS client 
 * request. Passed entry index is filled in if DNS query was resolved 
 * without error, else this returns non-zero ENP_ error code. 
 *
 * 
 * PARAM1: char * name
 * PARAM2: int * cacheindex
 *
 * RETURNS: ENP_SEND_PENDING if request is still on the net, or ENP_TIMEOUT
 * id request timed out (or was never made), or 0 (SUCCESS) if passed 
 * name found in cache, or ENP_PARAM if 
 */

static  int
dnc_lookup(char * name, struct dns_querys ** cacheentry)
{
   struct dns_querys * dns_entry;
   int      alias;

#ifdef DEBUG_DNSCLNT_C
   dump_dns_qs("dnc_lookup()");
#endif
   /* look through the cache for the passed name */
   for (dns_entry = dns_qs; dns_entry; dns_entry = dns_entry->next)
   {
      if (strcmp(dns_entry->dns_names, name) == 0)
         break;

      if ((dns_entry->he.h_name) &&
          (strcmp(dns_entry->he.h_name, name) == 0))
      {
         break;
      }

      for (alias = 0; alias < MAXDNSALIAS; alias++)
      {
         if ((dns_entry->alist[alias]) &&
            (strcmp(dns_entry->alist[alias], name) == 0))
               goto found_name;
      }
   }

found_name:

   /* if not found, return error */
   if (dns_entry == NULL)
      return ENP_PARAM;

   /* else, prepare to return entry index */
   if (cacheentry != NULL)
      *cacheentry = dns_entry;

   /* if completed request, return success */
   if (dns_entry->rcode != DNSRC_UNSET)
      return 0;

   /* incomplete request -- return pending or timed-out status */
   if (dns_entry->tries < dns_trys) /* still trying? */
      return ENP_SEND_PENDING;
   else
      return ENP_TIMEOUT;
}



/* FUNCTION: dns_lookup()
 *
 * dns_lookup() - check state of a DNS client request previously 
 * launched with dns_query(). Passed IP address is filled in (in net 
 * endian) if DNS query was resolved without error, else this clears 
 * passed Ip address and returns non-zero ENP_ code. 
 *
 * 
 * PARAM1: ip_addr * ip
 * PARAM2: char * name
 *
 * RETURNS: ENP_SEND_PENDING if request is still on the net, or ENP_TIMEOUT
 * if request timed out (or was never made), or 0 (SUCCESS) if passed IP 
 * is filled in and everything is peachy, or other ENP errors if 
 * detected. 
 */

int   
dns_lookup(ip_addr * ip, char * name)
{
   struct dns_querys * dns_entry;
   int      err;

   *ip = 0L;
   err = dnc_lookup(name, &dns_entry);
   if (err)
      return err;
   *ip = dns_entry->ipaddr_list[0];    /* return IP address */
   return 0;
}



/* static hostent structure for gethostbyname to return when it's
 * been passed a "name" which is just a dot notation IP address. */
static struct hostent dnc_hostent;
static ip_addr dnc_addr;         /* actual output in hostent */
static char *dnc_addrptrs[2];    /* pointer array for hostent */
#define DNC_NAME_SIZE      31
static char dnc_name[DNC_NAME_SIZE+1];        /* IP address string */


/* FUNCTION: dns_query_do()
 *
 * Perform a DNS query
 *
 * PARAM1: char *             host name being queried
 * PARAM2: char               query type
 * PARAM3: dns_querys **      address of ptr to query result
 *
 * RETURNS: dns_querys *      ptr to query result or NULL
 *
 * Issue a DNS query and wait for completion or timeout. If successful,
 * store the query results in 'p_entry'. Return the result of the query
 * or NULL if the query failed.
 */

struct dns_querys *
dns_query_do(char *name, char type, struct dns_querys **p_entry)
{
   int err = 0;
   int loop_cnt = 0;

   /* we will call dnc_lookup() until it either succeeds, fails
    * or we time out trying */
   err = dns_query_type(name, type, (ip_addr)0, (unsigned long)0, p_entry);
   do
   {
      err = dnc_lookup(name, p_entry);
      if (err != ENP_SEND_PENDING)
         break;
#ifndef SUPERLOOP
      TK_SLEEP(DNS_QUERY_WAIT * TPS);
#else
      {
         u_long uTicks = cticks;

         /* don't TK_SLEEP - hang around */
         while ((cticks - uTicks) < DNS_QUERY_WAIT * TPS)
            tk_yield();				
      }
#endif
      loop_cnt++;
   } while (loop_cnt < DNS_QUERY_RETRY);  

   if (err)
   {
      printf("dns_query_do(): err = %d, loop_cnt = %d\n", err, loop_cnt);
   }

   return (*p_entry);
}



/* FUNCTION: gethostbyname()
 *
 * Get host information for named host
 *
 * PARAM1: char *             host name
 *
 * RETURNS: hostent *         ptr to host entry structure or NULL
 *
 * Implements a "standard" Unix version of gethostbyname().
 * Returns a pointer to a hostent structure if successful, NULL
 * if not successful. The returned structure should NOT be 
 * freed by the caller.
 */

struct hostent *
gethostbyname(char *name)
{
   int      i;
   char *cp;
   unsigned int snbits;
   struct dns_querys *dns_entry = NULL;
   struct hostent *ho;
   
#ifdef DEBUG_DNSCLNT_C
   dump_dns_qs("gethostbyname()");
#endif

   /* if the name is a dot notation IP address, just return the address */
   cp = parse_ipad(&dnc_addr, &snbits, name);
   if (!cp)    
   {
      /* fill in the static hostent structure and return it */
      strncpy(dnc_name, name, (size_t)DNC_NAME_SIZE);
      dnc_name[DNC_NAME_SIZE] = '\0';
      dnc_hostent.h_name = &dnc_name[0];
      dnc_hostent.h_aliases = NULL;  /* we don't do the aliases */
      dnc_hostent.h_addrtype = AF_INET; /* AF_INET */
      dnc_hostent.h_length = 4;   /* length of IP address */
      dnc_hostent.h_addr_list = &dnc_addrptrs[0];
      dnc_addrptrs[0] = (char*)&dnc_addr;
      dnc_addrptrs[1] = NULL;

      return (&dnc_hostent);
   }

   /* get the name to IP address mapping information */
   dns_entry = dns_query_do(name, DNS_TYPE_IPADDR, &dns_entry);
   if (dns_entry == NULL)
   {
      printf("gethostbyname(): NULL entry\n");
      return (NULL);
   }
   if (dns_entry->ipaddrs == 0)  /* no IPv4 address? */
   {
      printf("gethostbyname(): no IPv4 addresses\n");
      return (NULL);
   }
   /* Set up the hostent fields for IPv4. */
   ho = &dns_entry->he;
   ho->h_addrtype = AF_INET;
   ho->h_length = 4;   /* length of IP address */
   ho->h_addr_list = &dns_entry->addrptrs[0];

   /* fill in the address pointer list in hostent structure */
   for (i = 0; i < dns_entry->ipaddrs; i++)
   {
      dns_entry->addrptrs[i] = (char *)&dns_entry->ipaddr_list[i];
#ifdef NPDEBUG
      dprintf("+++ gethostbyname addr = %lx\n", *((ulong *)dns_entry->addrptrs[i]));
#endif
   }
   dns_entry->addrptrs[i] = (char *)NULL;    /* last one gets a NULL */
   
   return (ho);
}
 


int prep_reverse_name(char *name, char *append_str, char *mod_str)
{
   int name_len, append_len;

   if ((name == NULL) || (append_str == NULL) || (mod_str == NULL))
   {
      return (-1);
   }
   name_len = strlen(name);
   append_len = strlen(append_str);
   if ((name_len == 0) || (append_len == 0))
   {
      return (-1);
   }

   reverse_domain_addr(mod_str, name);
#if 0
   sprintf(mod_str, "%s.%s", mod_str, append_str);
#else
   strcat(mod_str, ".");
   strcat(mod_str, append_str);
#endif

   return (0);
}



int 
nslookup(char * name, char type, struct dns_querys **dns_entry)
{
   char modified_name[255];

#ifdef DEBUG_DNSCLNT_C
   dump_dns_qs("nslookup()");
#endif

   switch (type)
   {
   case DNS_TYPE_PTR:
      if (prep_reverse_name(name, "in-addr.arpa", modified_name) == 0)
      {
         *dns_entry = dns_query_do(modified_name, type, dns_entry);
         if (*dns_entry == NULL)
         {
#ifdef DEBUG_DNSCLNT_C
            printf("nslookup(): NULL DNS entry\n");
#endif
            return (-1);
         }
      }
      break;
   default:
#ifdef DEBUG_DNSCLNT_C
      printf("nslookup(): type %d not supported\n", type);
#endif
      return (-1);
      break;
   }

   return (0);
}



#ifdef IP_V6

struct hostent *
gethostbyname2(char * name, int af)
{
   int i;
   struct hostent *ho;
   struct dns_querys *dns_entry = NULL;

#ifdef DEBUG_DNSCLNT_C
   dump_dns_qs("gethostbyname2()");
#endif

   if (af == AF_INET)
   {
      ho = gethostbyname(name);
      return (ho);
   }
   else if (af == AF_INET6)
   {
      dns_entry = dns_query_do(name, DNS_TYPE_AAAA, &dns_entry);
      if (dns_entry == NULL)
      {
         printf("gethostbyname2(): NULL entry\n");
         for (dns_entry = dns_qs; dns_entry; dns_entry = dns_entry->next)
         {
            if (strcmp(dns_entry->dns_names, name))
            {
               printf("gethostbyname2(): %s is dns_ds\n", name);
            }
         }
         return (NULL);
      }
   }
   else
   {
      return (NULL);
   }

   if (dns_entry->ip6addrs == 0)     /* no IPv6 address? */
      return (NULL);

   /* Rework the hostent fields for IPv6 */
   ho = &dns_entry->he;
   ho->h_addrtype = AF_INET6;
   ho->h_length = 16;   /* length of IP address */
   /* fill in the address pointer list in hostent structure */
   for (i = 0; i < dns_entry->ip6addrs; i++)
      dns_entry->addrptrs[i] = &dns_entry->ip6_list[i][0];
   dns_entry->addrptrs[i] = NULL;     /* last one gets a NULL */
   ho->h_addr_list = &dns_entry->addrptrs[0];

   return (ho);
}

#endif /* IP_V6 */



#ifdef DNS_CLIENT_UPDT

/* FUNCTION: dns_update()
 *
 * dns_update- sends a DNS UPDATE packet to the authoritative server
 * of the specified domain name. Send DNS_TYPE_AUTHNS to get hostent. It then sends
 * the DNS_UPDT packet to the authoritative server.
 *
 * 
 * PARAM1: char * soa_mname     domain name
 * PARAM2: char * hname         host name
 * PARAM3: ip_addr ipaddress   ip address
 * PARAM4: unsigned long ttl   ttl value
 * PARAM5: IO handle for output (NULL is OK)
 *
 * RETURNS: 0 if successful
 *          negative ENP error if internal error occurs (eg timeout)
 *          else one of the DNSRC_ errors (all positive).
 */

int
dns_update(char * soa_mname, char * hname, ip_addr ipaddr,
            unsigned long ttl, void *pio)
{
   int      err;
   u_long   tmo;
   struct dns_querys *dns_entry;
   unsigned char *ucp;
   ip_addr save_dns_srvr;
   char *domP;  
   
#ifdef DEBUG_DNSCLNT_C
   dump_dns_qs("dns_update()");
#endif

   /*
    * get domain name from host name.
    */
   if ((domP = strstr(hname, ".")) == NULL)
   {
         dprintf("bad domain name.\n");
         return -1;
   }

   /* get authoritative name server for specified domain. We will
    * call dns_query_type() until it either succeeds, fails or
    * we time out trying.
    */
   tmo = cticks + (10 * TPS);
   err = dns_query_type(domP + 1, DNS_TYPE_AUTHNS, (ip_addr)0,
            (unsigned long)0, &dns_entry);

   do
   {
      if ((err = dnc_lookup(domP + 1, &dns_entry)) == 0)
         break;
      tk_yield();
   } while  (cticks < tmo);
   if (err)
   {      
       /* kill the entry */
      dnc_del(dns_entry);
      return err;
   }

   /* Get here if we received a hostent structure. Send the update packet
    * to the received IP address (first in list) by swapping the address
    * into the list of DNS servers.
    */

   if (dns_entry->he.h_addr_list)
   {
      ucp = (unsigned char *) *(dns_entry->he.h_addr_list);
      MEMCPY(&save_dns_srvr, dns_servers, 4);   /* save 1st DNS server */
      MEMCPY(dns_servers, ucp, 4);              /* swap in AUTHS address */

      ns_printf(pio," sending update packet to %u.%u.%u.%u\n",
         *ucp, *(ucp + 1), *(ucp + 2), *(ucp + 3));
   }

   /* 
    * kill the entry, 
    */
   dnc_del(dns_entry);

   /*
    * if this is a delete, we first kill any
    * exiting entry.
    */
   if (ttl == 0L)
   {
      int found = 0;
      int alias;

      /* yup - search for it */
      /* look through the cache for the passed name */
      for (dns_entry = dns_qs; dns_entry; dns_entry = dns_entry->next)
      {
         if (strcmp(dns_entry->dns_names, hname) == 0)
         {
            found = 1;
            break;
         }
   
         if ((dns_entry->he.h_name) &&
              (strcmp(dns_entry->he.h_name, hname) == 0))
         {
             found = 1;
             break;
         }

         for (alias = 0; alias < MAXDNSALIAS; alias++)
         {
            if ((dns_entry->alist[alias]) &&
                (strcmp(dns_entry->alist[alias], hname) == 0))
            {
               found = 1;
               break;
            }
         }
      }
   
      if (found)
      {
         /* kill the entry */
         dnc_del(dns_entry);
      }
   }

   tmo = cticks + (10 * TPS);
   err = dns_query_type(hname, DNS_UPDT, ipaddr, ttl, &dns_entry);
 
   while (cticks < tmo)
   {
      if ((err = dnc_lookup(hname, &dns_entry)) == 0)
         break;
      tk_yield();   
   }

   MEMCPY(&dns_servers[0], &save_dns_srvr, 4);

   if (dns_entry->rcode == DNSRC_NOTAUTH)
   {
      ns_printf(pio," DNS server replies with - not authoritative.\n");
      dns_entry->rcode = DNSRC_UNSET;
      dns_entry->id = 0;
      dns_entry->dns_names[0] = 0;
      dns_entry->he.h_name = 0;
      return 0;
   }
   dns_entry->ipaddrs = ipaddr;

   if (!err)
      err = dns_entry->rcode;

   /* if delete then kill entry */
   if (ttl == 0L)
   {
      /* kill the entry */
      dnc_del(dns_entry);
   }
   return err;
}

#endif  /* DNS_CLIENT_UPDT */



#ifdef NET_STATS

/* FUNCTION: dns_state()
 * 
 * PARAM1: void * pio
 *
 * RETURNS: 
 */

int
dns_state(void * pio)
{
   struct dns_querys * dns_entry;
   int   i;

#ifdef DEBUG_DNSCLNT_C
   dump_dns_qs("dns_state()");
#endif   

   ns_printf(pio,"DNS servers:");
   for (i = 0; i < MAXDNSSERVERS; i++)
      ns_printf(pio,"%u.%u.%u.%u ", PUSH_IPADDR(dns_servers[i]));

   ns_printf(pio,"\nDNS cache:\n");
   for (dns_entry = dns_qs; dns_entry; dns_entry = dns_entry->next)
   {
#ifdef IP_V6
      if (dns_entry->he.h_addrtype == AF_INET6)
      {
         ns_printf(pio, "name: %s\n", dns_entry->he.h_name);
         if (!(dns_entry->he.h_addr_list))
            ns_printf(pio,"\tno addresses\n");
         else
         {
            char ** cpp;
            for (cpp = dns_entry->he.h_addr_list; *cpp; cpp++)
            {
               char ip6buf[40];     /* tmp buffer for ipv6 address text */
               ns_printf(pio,"\t%s\n",  print_ip6((struct in6_addr *)(*cpp), ip6buf));
            }
         }
         ns_printf(pio,"retry:%d, ID:%d, rcode:%d, err:%d\n",
          dns_entry->tries, dns_entry->id, dns_entry->rcode, dns_entry->err);
      }
      else 
#endif
      if (dns_entry->he.h_addrtype == AF_INET)
      {
         ns_printf(pio,"name: %s\n", dns_entry->he.h_name);
         if (!(dns_entry->he.h_addr_list))
            ns_printf(pio,"\tno addresses\n");
         else
         {
            char ** cpp;
            u_char * ucp;
            for (cpp = dns_entry->he.h_addr_list; *cpp; cpp++)
            {
               ucp = (unsigned char *) (*cpp);
               ns_printf(pio,"\t%d.%d.%d.%d\n", *ucp, *(ucp + 1), *(ucp + 2), *(ucp + 3));
            }
         }
         ns_printf(pio,"retry:%d, ID:%d, rcode:%d, err:%d\n",
          dns_entry->tries, dns_entry->id, dns_entry->rcode, dns_entry->err);
      }
   }
   ns_printf(pio,"protocol/implementation runtime errors:%lu\n", dnsc_errors);
   ns_printf(pio,"requests sent:%lu\n", dnsc_requests);
#ifdef DNS_CLIENT_UPDT
   ns_printf(pio, "Updates sent:%lu\n", dnsc_updates);
#endif /* DNS_CLIENT_UPDT */
   ns_printf(pio,"replies received:%lu\n", dnsc_replies);
   ns_printf(pio,"usable replies:%lu\n", dnsc_good);
   ns_printf(pio,"total retries:%lu\n", dnsc_retry);
   ns_printf(pio,"timeouts:%lu\n", dnsc_tmos);

   return 0;
}



int
dnc_database(void * pio)
{
   struct dns_querys *dns_entry;
   struct hostent *p;
   char **cpp;
   int i;

   if (dns_qs == NULL)
   {
      ns_printf(pio, "No DNS entries.\n");
      return 0;
   }

#ifdef DEBUG_DNSCLNT_C
   dump_dns_qs("dnc_database()");
#endif

   /* look through the cache for the passed name */
   for (dns_entry = dns_qs; dns_entry; dns_entry = dns_entry->next)
   {
      if (dns_entry->rcode != DNSRC_OK)
      {
         ns_printf(pio, "Query for %s: ", dns_entry->dns_names);
         if (dns_entry->rcode == DNSRC_UNSET)
            ns_printf(pio, "no reply\n");
         else
            ns_printf(pio, "bad reply code was %d.\n", dns_entry->rcode);
         continue;
      }
      if (dns_entry->he.h_name)
      {
         ns_printf(pio, "name: %s, ", dns_entry->he.h_name);
      }
      p = &dns_entry->he;
      if (*p->h_aliases)
      {
         ns_printf(pio, "\n  Aliases: ");
         for (cpp = p->h_aliases; *cpp; cpp++)
         {
            ns_printf(pio, "%s, ", *cpp);
         }
      }
#ifdef IP_V4
      if (dns_entry->ipaddrs)
      {
         if (p->h_addrtype == AF_INET)
         {
            ns_printf(pio, "\n  IPv4 addrs: ");
            for (i = 0; i < dns_entry->ipaddrs; i++)
            {
               ns_printf(pio, "%u.%u.%u.%u, ", 
                         PUSH_IPADDR(dns_entry->ipaddr_list[i]));
            }
         }
      }
#endif   /* IP_V4 */
#ifdef IP_V6
      if (dns_entry->ip6addrs)
      {
         if (p->h_addrtype == AF_INET6)
         {
            ns_printf(pio, "\n  IPv6 addrs: ");
            for (i = 0; i < dns_entry->ip6addrs; i++)
            {
               char ip6buf[40];     /* tmp buffer for ipv6 address text */
               ns_printf(pio, "%s ", 
                  print_ip6((struct in6_addr *)(&dns_entry->ip6_list[i][0]), ip6buf));
            }
         }
      }
#endif   /* IP_V6 */
      ns_printf(pio, "\n  Age (in seconds): %lu, Expires in %lu seconds (ttl)", 
         (cticks - dns_entry->send_time)/TPS, dns_entry->expire_time);

      ns_printf(pio, "\n");
   }

   return (0);
}

#endif   /* NET_STATS */


#ifdef DNS_CLIENT_TEST_UPDT

#define MAX_DDNS_STR_LEN 256

typedef struct ResRec RR;

struct ddnsReq
{
	char *zone;
	RR   *preqs;
	RR   *ups;
};

typedef struct ddnsReq DDNSREQS;

static DDNSREQS DDNSReqs;
static int nsddnsLineNo;

static char *DDNSTestFile = "..\\dnssrv\\ddnstest.txt";
/* static char DDNSzone[MAX_DDNS_STR_LEN] = {0}; */

static char *  nsddnsZone(void * pio, FILE *fp, char *inWrk);
static char * nsddnsPreqs(void * pio, FILE *fp, char *inWrk);
static char * nsddnsUps(void * pio, FILE *fp, char *inWrk);
static int nsddnsMsgs(void * pio, FILE *fp, char *inWrk);
extern int dns_sendDDNSReq(char *zone, int lenPreqs, int numPreqs,
                           char *preqsP, int lenUps, int numUps, char *upsP);


/* FUNCTION: nsddnstest()
 * 
 * Read command file and send test DDNS reqs.
 *
 * Uses recursive descent to parse file.
 *
 * PARAM1: void *pio
 *
 * RETURNS: 
 */

int 
nsddnstest(void * pio)
{
   FILE * fp;
   char *wrk;
   char inBuf[MAX_DDNS_STR_LEN];
   int err = 0;
   RR   *rrP;
   RR   *rrP2;
   char *preqsP;
   char *upsP;
   int preqsLen;
   int preqsCount;
   int upsLen;
   int upsCount;
   unsigned long delTicks;

   nsddnsLineNo = 0;
   MEMSET(&DDNSReqs, 0, sizeof(DDNSReqs));

   fp = fopen(DDNSTestFile, "r");

   if (!fp)
   {
      ns_printf(pio, "nsddnstest: can't open input file %s\n", DDNSTestFile);
      fclose(fp);
      return -1;
   }

   while ((wrk = fgets(inBuf, sizeof(inBuf), fp)))
   {
      nsddnsLineNo++;
      if ((inBuf[0] == ';') || (inBuf[0] == '\n') || (inBuf[0] == '\t') || (inBuf[0] == ' '))
         continue;

      if ((wrk = strstr(wrk, "msg")))
      {
         err = nsddnsMsgs(pio, fp, wrk);
         if (err)
         {
            dprintf("*** nsddnstest, bad msg\n");
            break;
         }
      }

      if (!DDNSReqs.zone)
      {
         ns_printf(pio, "nsddnstest: no ZONE\n");
         goto FREEIT;
      }

      if (!DDNSReqs.ups)
      {
         ns_printf(pio, "nsddnstest: no UPDATEs\n");
         goto FREEIT;
      }

      /* create DDNS msg and send it */

      /* got preqs? */
      if (DDNSReqs.preqs)
      {
         char *preqsWrkP;

         /* yup get length and count */
         preqsCount = preqsLen = 0;
         rrP = DDNSReqs.preqs;
         while (rrP)
         {
            preqsCount++;
            preqsLen += rrP->RDlength;
            rrP = rrP->next;
         }

         preqsWrkP = preqsP = (char *)npalloc(preqsLen);

         rrP = DDNSReqs.preqs;
         while (rrP)
         {
            MEMCPY(preqsWrkP, rrP->RdataP, rrP->RDlength);
            preqsWrkP += rrP->RDlength;
            rrP = rrP->next;
         }
      }

      /* package updates */
      upsCount = upsLen = 0;
      rrP = DDNSReqs.ups;
      while (rrP)
      {
         upsCount++;
         upsLen += rrP->RDlength;
         rrP = rrP->next;
      }

      wrk = upsP = (char *)npalloc(upsLen);

      rrP = DDNSReqs.ups;
      while (rrP)
      {
         MEMCPY(wrk, rrP->RdataP, rrP->RDlength);
         wrk += rrP->RDlength;
         rrP = rrP->next;
      }

      /* send the pkt to server */
      dns_sendDDNSReq(DDNSReqs.zone, preqsLen, preqsCount,
                      preqsP, upsLen, upsCount, upsP);

FREEIT:

      if (DDNSReqs.zone)
         npfree(DDNSReqs.zone);

      rrP = DDNSReqs.preqs;
      while (rrP)
      {
         rrP2 = rrP->next;
         if (rrP->RdataP)
            npfree(rrP->RdataP);	   
         npfree(rrP);
         rrP = rrP2;
      }

      rrP = DDNSReqs.ups;
      while (rrP)
      {
         rrP2 = rrP->next;
         if (rrP->RdataP)
            npfree(rrP->RdataP);	   
         npfree(rrP);
         rrP = rrP2;
      }

      DDNSReqs.zone = NULL;
      DDNSReqs.preqs = NULL;
      DDNSReqs.ups = NULL;


      /* give time for processing between UDP pkts */
      delTicks = cticks;
      while ((cticks - delTicks) < (TPS * 3))
         tk_yield();
   }  /* while */

   fclose(fp);
   return err;
}



/* FUNCTION: nsddnsMsgs()
 * 
 * Process a "msg" command
 *
 * PARAM1: void *pio
 *
 * RETURNS: 
 */

static int  
nsddnsMsgs(void * pio, FILE *fp, char *inWrk)
{
   char *wrk = inWrk;
   char *wrk2;
   char inBuf[MAX_DDNS_STR_LEN];
   int fbrace = 0;

   nsddnsLineNo--;
   inBuf[0] = 0;

   /* got "msg" find "{" ... "}"*/
   do
   {
      nsddnsLineNo++;
      if (inBuf[0] == ';')
         continue;

      if (!fbrace)
      {
         if ((wrk = strstr(wrk, "{")) == NULL)
            continue;
         else
            wrk++;
      }

      fbrace = 1;	

RELOOP:		
      if ((*wrk == '\n') || (*wrk == ';'))
         continue;

      wrk2 = wrk;
		/* got "zone"? */
      if ((wrk = strstr(wrk2, "zone")))
      {
         /* yup - process "zone" */
         if (!(wrk = nsddnsZone(pio, fp, wrk)))
            return -1;
         else
         {
            wrk = strcpy(inBuf, wrk);
            goto RELOOP; /* GOTO! */
         }
      }
      else
      {
         /* got "preqs"? */
         if ((wrk = strstr(wrk2, "preqs")))
         {
            /* yup - process "preqs" */
            if (!(wrk = nsddnsPreqs(pio, fp, wrk)))
               return -1;
            else
            {
               wrk = strcpy(inBuf, wrk);
               goto RELOOP; /* GOTO! */
            }
         }
         else
            /* got "ups"? */
            if ((wrk = strstr(wrk2, "ups")))
            {
               /* yup - process "ups" */
               if (!(wrk = nsddnsUps(pio, fp, wrk)))
                  return -1;
               else
               {
                  wrk = strcpy(inBuf, wrk);
                  goto RELOOP; /* GOTO! */
               }
            }
      }

      /* got "}"? */
      if ((wrk = strstr(wrk2, "}")))
      return 0;
   }
   while ((wrk = fgets(inBuf, sizeof(inBuf), fp)));

   /* was EOF? */
   if (!wrk)
   {
      ns_printf(pio, "msg has no body, line [%d]\n", nsddnsLineNo);
   }

   return -1;
}



static char * nsddnsRrexistsvi(void * pio, FILE *fp, char *inWrk);
static char * nsddnsRrexistsvd(void * pio, FILE *fp, char *inWrk);
static char * nsddnsRrnexist(void * pio, FILE *fp, char *inWrk);
static char * nsddnsRrninus(void * pio, FILE *fp, char *inWrk);
static char * nsddnsRrnNinus(void * pio, FILE *fp, char *inWrk);
static char * nsddnsRr(void * pio, FILE *fp, char *inWrk);
static char * nsddnsdRrs(void * pio, FILE *fp, char *inWrk);
static char * nsddnsdaRr(void * pio, FILE *fp, char *inWrk);
static char * nsddnsdRr(void * pio, FILE *fp, char *inWrk);


/* FUNCTION: nsddnsPreqs
 * 
 * Process a "preqs" command
 *
 * PARAM1: void *pio
 *
 * RETURNS: 
 */

static char *  
nsddnsPreqs(void *pio, FILE *fp, char *inWrk)
{
   char *wrk = inWrk;
   char *wrk2;
   char inBuf[MAX_DDNS_STR_LEN];
   int fbrace = 0;

   nsddnsLineNo--;
   inBuf[0] = 0;

   do
   {
      nsddnsLineNo++;
      if (inBuf[0] == ';')
         continue;

      if (!fbrace)
      {
         if ((wrk = strstr(wrk, "{")) == NULL)
            continue;
         else
            wrk++;
      }

      fbrace = 1;

      /* found "{" - get prerequisites */

RELOOP:		
      if ((*wrk == '\n') || (*wrk == ';'))
         continue;

      wrk2 = wrk;
      wrk = strtok(wrk, "{ \t\n}");

      if (wrk)
      {
         wrk2 = wrk + strlen(wrk) + 1;

         /* RRset exists (value independent) */
         if (strcmp(wrk, "rrexistsvi") == 0)
         {
            if (!(wrk = nsddnsRrexistsvi(pio, fp, wrk2)))
               return -1;
            else
            {
               wrk = strcpy(inBuf, wrk);
               goto RELOOP; /* GOTO! */
            }
         }
         else
         /* RRset exists (value dependent) */
            if (strcmp(wrk, "rrexistsvd") == 0)
            {
               if (!(wrk = nsddnsRrexistsvd(pio, fp, wrk2)))
                  return -1;
               else
               {
                  wrk = strcpy(inBuf, wrk);
                  goto RELOOP; /* GOTO! */
               }
            }
            else
               /* RRset does not exist */
               if (strcmp(wrk, "rrnexist") == 0)
               {
                  if (!(wrk = nsddnsRrnexist(pio, fp, wrk2)))
                     return -1;
                  else
                  {
                     wrk = strcpy(inBuf, wrk);
                     goto RELOOP; /* GOTO! */
                  }
               }
               else
                  /* name in use */
                  if (strcmp(wrk, "rrninus") == 0)
                  {
                     if (!(wrk = nsddnsRrninus(pio, fp, wrk2)))
                        return -1;
                     else
                     {
                        wrk = strcpy(inBuf, wrk);
                        goto RELOOP; /* GOTO! */
                     }
                  }
                  else
                     /* name not in use */
                     if (strcmp(wrk, "rrnninus") == 0)
                     {
                        if (!(wrk = nsddnsRrnNinus(pio, fp, wrk2)))
                           return -1;
                        else
                        {
                           wrk = strcpy(inBuf, wrk);
                           goto RELOOP; /* GOTO! */
                        }
                     }
                     else
                     {
                        ns_printf(pio, "preqs bad command, line [%d]\n", nsddnsLineNo);
                        return NULL;
                     }
      }
      else
         wrk = wrk2;

      /* got "}"? */
      if ((wrk = strstr(wrk, "}")))
      {
         nsddnsLineNo++;
         return fgets(inBuf, sizeof(inBuf), fp);
      }
   }
   while ((wrk = fgets(inBuf, sizeof(inBuf), fp)));

   /* was EOF? */
   if (!wrk2)
   {
      ns_printf(pio, "preqs has no body, line [%d]\n", nsddnsLineNo);
   }

   return NULL;
}



#define TYPE_RDLENGTH_SIZE (2 + 2 + 4 + 2) /* TYPE, CLASS, TTL, and RDLENGTH */

/* FUNCTION: nsddnsRrexistsvi()
 *
 * Process a "rrexistsvi" command
 *
 *  CLASS    TYPE     RDATA    Meaning
 *  ------------------------------------------------------------
 *   ANY      rrset    empty    RRset exists (value independent)
 *
 * RRset exists (value independent).  At least one RR with a
 *        specified NAME and TYPE (in the zone and class specified by
 *        the Zone Section) must exist.
 * 
 * PARAM1: void *pio
 *
 * RETURNS: 
 */

extern int dnc_copyout(char * dest, char * src);

static char *  
nsddnsRrexistsvi(void * pio, FILE *fp, char *inWrk)
{
   char *wrk = inWrk;
   char *wrk2;
   char inBuf[MAX_DDNS_STR_LEN];
   RR *RRp;
   int len;

   if ((wrk = strstr(wrk, "{")) == NULL)
   {
      ns_printf(pio, "rrexistsvi expects '{', line [%d]\n", nsddnsLineNo);
      return NULL;
   }

   /* got "rrexistsvi" find "{" ... "}"*/
   wrk2 = strtok(wrk, "{, \t}");

   RRp = (RR*) npalloc(sizeof(RR));
   MEMSET(RRp, 0, sizeof(RR));

   len = dnc_copyout(RRp->name, wrk2);

   wrk2 = strtok(NULL, "{, \t}");

   RRp->type = atoi(wrk2);
   RRp->dclass = C_ANY;

   /* alloc memory for full RR */
   RRp->RdataP = wrk = (char *) npalloc(len + TYPE_RDLENGTH_SIZE);
   MEMSET(wrk, 0, len + TYPE_RDLENGTH_SIZE);

   MEMCPY(wrk, RRp->name, len);
   wrk += len;

   PUTSHORT(RRp->type, wrk);

   PUTSHORT(RRp->dclass, wrk);

   RRp->RDlength = len + TYPE_RDLENGTH_SIZE;
	

   RRp->next = DDNSReqs.preqs;
   DDNSReqs.preqs = RRp;
   wrk = wrk2 + strlen(wrk2) + 1;
	   
   /* got "}"? */
   if ((wrk = strstr(wrk, "}")))
   {
      nsddnsLineNo++;
      return fgets(inBuf, sizeof(inBuf), fp);
   }

   ns_printf(pio, "rrexistsvi expects '}', line [%d]\n", nsddnsLineNo);
   return NULL;
}



/* FUNCTION: nsddnsRrexistsvd()
 *
 *
 *  CLASS    TYPE     RDATA    Meaning
 *  ------------------------------------------------------------
 *   zone     rrset    rr       RRset exists (value dependent)
 *
 * RRset exists (value dependent).  A set of RRs with a
 *        specified NAME and TYPE exists and has the same members
 *        with the same RDATAs as the RRset specified here in this
 *        Section.
 *
 * Process a "rrexistsvdd" command
 *
 * PARAM1: void *pio
 *
 * RETURNS: 
 */
static char *  
nsddnsRrexistsvd(void * pio, FILE *fp, char *inWrk)
{
   char *wrk = inWrk;
   char *wrk2;
   char inBuf[MAX_DDNS_STR_LEN];
   RR *RRp;
   short len;
   short len2;
   unsigned long addr;
   unsigned sbitz = 0;

   if ((wrk = strstr(wrk, "{")) == NULL)
   {
      ns_printf(pio, "rrexistsvi expects '{', line [%d]\n", nsddnsLineNo);
      return NULL;
   }

   /* found "{" - get exists (value dependent) */
   wrk2 = strtok(wrk, "{, \t}");

   RRp = (RR*) npalloc(sizeof(RR));
   MEMSET(RRp, 0, sizeof(RR));

   len = dnc_copyout(RRp->name, wrk2);

   wrk2 = strtok(NULL, "{, \t}");

   RRp->type = atoi(wrk2);

   wrk2 = strtok(NULL, "{, \t}");
   RRp->dclass = C_IN;
  
   /* SOA */
   if (RRp->type == T_SOA)
   {

      /*
       * 3.3.13. SOA RDATA format
       *
       *    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
       *    /                     MNAME                     /
       *    /                                               /
       *    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
       *    /                     RNAME                     /
       *    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
       *    |                    SERIAL                     |
       *    |                                               |
       *    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
       *    |                    REFRESH                    |
       *    |                                               |
       *    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
       *    |                     RETRY                     |
       *    |                                               |
       *    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
       *    |                    EXPIRE                     |
       *    |                                               |
       *    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
       *    |                    MINIMUM                    |
       *    |                                               |
       *    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
       */

      char zmail[MAX_DDNS_STR_LEN];
      int mlen;
      int SOAlen;
      int domLen;
      unsigned long serial;
      u_long zserial;
      u_long zrefresh;
      u_long zretry;
      u_long zexpire;
      u_long zminimum;

      zserial = atol(wrk2);  /* serial */

      wrk2 = strtok(NULL, "{, \t}");
      zrefresh = atol(wrk2);  

      wrk2 = strtok(NULL, "{, \t}");
      zretry = atol(wrk2); 

      wrk2 = strtok(NULL, "{, \t}");
      zexpire = atol(wrk2); 

      wrk2 = strtok(NULL, "{, \t}");
      zminimum = atol(wrk2); 

      mlen = dnc_copyout(zmail, "z.z"); 

      /* alloc memory for full RR */
      RRp->RdataP = wrk = (char *) npalloc(len + TYPE_RDLENGTH_SIZE + 
		   (SOAlen = len + /* comp len of SOA */
		   mlen + /* comp len of mail addr */
		   5 * 4 /* 5 longs */));

      MEMSET(wrk, 0, len + TYPE_RDLENGTH_SIZE + SOAlen);
      MEMCPY(wrk, RRp->name, len);

      RRp->RDlength = len + TYPE_RDLENGTH_SIZE + SOAlen;

      wrk += len;
      domLen = len;

      /* PUTSHORT() is equivalent to an htons() */
      PUTSHORT(RRp->type, wrk);
      PUTSHORT(RRp->dclass, wrk);
      wrk += 4; /* TTL = 0 */
     
      /* length */
      len2 = len = SOAlen;
      PUTSHORT(len, wrk);

      /* SOA */
      MEMCPY(wrk, RRp->name, domLen);
      wrk += domLen;
      MEMCPY(wrk, zmail, mlen);
      wrk += mlen;

      PUTLONG(zserial, wrk);
      PUTLONG(zrefresh, wrk);
      PUTLONG(zretry, wrk);
      PUTLONG(zexpire, wrk);
      PUTLONG(zminimum, wrk);
   }
   else
   {
      /* CNAME */
      if ((RRp->type == T_CNAME) || (RRp->type == T_NS))
      {
         char zname[MAX_DDNS_STR_LEN];
         int cnameLen;

         cnameLen = dnc_copyout(zname, wrk2); 

         /* alloc memory for full RR */
         RRp->RdataP = wrk = (char *) npalloc(len + TYPE_RDLENGTH_SIZE + cnameLen);
         RRp->RDlength = len + TYPE_RDLENGTH_SIZE + cnameLen;
         MEMSET(wrk, 0, len + TYPE_RDLENGTH_SIZE + cnameLen);
         MEMCPY(wrk, RRp->name, len);

         wrk += len;

         /* PUTSHORT() is equivalent to an htons() */
         PUTSHORT(RRp->type, wrk);
         PUTSHORT(RRp->dclass, wrk);
         wrk += 4; /* TTL = 0 */

         PUTSHORT(cnameLen, wrk);
         MEMCPY(wrk, zname, cnameLen);
      }
      else
      {

         /* alloc memory for full RR */
         RRp->RdataP = wrk = (char *) npalloc(len + TYPE_RDLENGTH_SIZE + sizeof(addr));
         MEMSET(wrk, 0, len + TYPE_RDLENGTH_SIZE + sizeof(addr));

         MEMCPY(wrk, RRp->name, len);
         RRp->RDlength = len + TYPE_RDLENGTH_SIZE + sizeof(addr);

         wrk += len;
         PUTSHORT(RRp->type, wrk);
         PUTSHORT(RRp->dclass, wrk);

         wrk += 4; /* TTL = 0 */

         if (parse_ipad(&addr,  &sbitz, wrk2) != NULL)
         {
            ns_printf(pio, "bad IP addr, line [%d]\n", nsddnsLineNo);
            return NULL;
         }

         len2 = len = sizeof(addr);
         PUTSHORT(len, wrk);
         MEMCPY(wrk, &addr, len2);
      }
   }

   RRp->next = DDNSReqs.preqs;
   DDNSReqs.preqs = RRp;
   wrk = wrk2 + strlen(wrk2) + 1;
   
   /* got "}"? */
   if ((wrk = strstr(wrk, "}")))
   {
      nsddnsLineNo++;
      return fgets(inBuf, sizeof(inBuf), fp);
   }

   ns_printf(pio, "rrexistsvdd expects '}', line [%d]\n", nsddnsLineNo);
   return NULL;
}



/* FUNCTION: nsddnsRrnexist()
 * 
 * Process a "rrnexist" command
 *
 *
 *  CLASS    TYPE     RDATA    Meaning
 *  ------------------------------------------------------------
 *   NONE     rrset    empty    RRset does not exist
 *
 * RRset does not exist.  No RRs with a specified NAME and TYPE
 *      (in the zone and class denoted by the Zone Section) can exist.
 *
 *
 * PARAM1: void *pio
 *
 * RETURNS: 
 */
static char *  
nsddnsRrnexist(void * pio, FILE *fp, char *inWrk)
{
   char *wrk = inWrk;
   char *wrk2;
   char inBuf[MAX_DDNS_STR_LEN];
   RR *RRp;
   short len;

   /* got "rrnexist" find "{" ... "}"*/
   if ((wrk = strstr(wrk, "{")) == NULL)
   {
      ns_printf(pio, "rrexistsvi expects '{', line [%d]\n", nsddnsLineNo);
      return NULL;
   }

   /* found "{" - get exists (value independent) */
   wrk2 = strtok(wrk, "{, \t}");

   RRp = (RR*) npalloc(sizeof(RR));
   MEMSET(RRp, 0, sizeof(RR));

   len = dnc_copyout(RRp->name, wrk2);

   wrk2 = strtok(NULL, "{, \t}");

   RRp->type = atoi(wrk2);
	
   RRp->next = DDNSReqs.preqs;
   DDNSReqs.preqs = RRp; 

   RRp->dclass = C_NONE;

   /* alloc memory for full RR */
   RRp->RdataP = wrk = (char *) npalloc(len + TYPE_RDLENGTH_SIZE);
   MEMSET(wrk, 0, len + TYPE_RDLENGTH_SIZE);

   MEMCPY(wrk, RRp->name, len);
   RRp->RDlength = len + TYPE_RDLENGTH_SIZE;

   wrk += len;
   PUTSHORT(RRp->type, wrk);
   PUTSHORT(RRp->dclass, wrk);
   wrk += 4; /* TTL = 0 */
   PUTSHORT(0, wrk);

   wrk = wrk2 + strlen(wrk2) + 1;
   
   /* got "}"? */
   if ((wrk = strstr(wrk, "}")))
   {
      nsddnsLineNo++;
      return fgets(inBuf, sizeof(inBuf), fp);
   }

   ns_printf(pio, "rrnexist expects '}', line [%d]\n", nsddnsLineNo);
   return NULL;
}



/* FUNCTION: nsddnsRrninus()
 * 
 * Process a "rrninus" command
 *
 *  CLASS    TYPE     RDATA    Meaning
 *  ------------------------------------------------------------
 *   ANY      ANY      empty    Name is in use
 *
 * Name is in use.  At least one RR with a specified NAME (in
 *        the zone and class specified by the Zone Section) must exist.
 *       Note that this prerequisite is NOT satisfied by empty
 *       nonterminals.
 *
 * PARAM1: void *pio
 *
 * RETURNS: 
 */

static char *  
nsddnsRrninus(void *pio, FILE *fp, char *inWrk)
{
   char *wrk = inWrk;
   char *wrk2;
   char inBuf[MAX_DDNS_STR_LEN];
   RR *RRp;
   short len;

   if ((wrk = strstr(wrk, "{")) == NULL)
   {
      ns_printf(pio, "rrexistsvi expects '{', line [%d]\n", nsddnsLineNo);
      return NULL;
   }

   /* found "{" - get exists (value independent) */
   wrk2 = strtok(wrk, "{, \t}");

   RRp = (RR*) npalloc(sizeof(RR));
   MEMSET(RRp, 0, sizeof(RR));

   len = dnc_copyout(RRp->name, wrk2);
	
   RRp->next = DDNSReqs.preqs;
   DDNSReqs.preqs = RRp;
   RRp->dclass = C_ANY;
   RRp->type = T_ANY;

   /* alloc memory for full RR */
   RRp->RdataP = wrk = (char *) npalloc(len + TYPE_RDLENGTH_SIZE);
   RRp->RDlength = len + TYPE_RDLENGTH_SIZE;
   MEMSET(wrk, 0, len + TYPE_RDLENGTH_SIZE);

   MEMCPY(wrk, RRp->name, len);

   wrk += len;
   PUTSHORT(RRp->type, wrk);
   PUTSHORT(RRp->dclass, wrk);

   wrk = wrk2 + strlen(wrk2) + 1;
   
   /* got "}"? */
   if ((wrk = strstr(wrk, "}")))
   {
      nsddnsLineNo++;
      return fgets(inBuf, sizeof(inBuf), fp);
   }

   ns_printf(pio, "rrninus expects '}', line [%d]\n", nsddnsLineNo);
   return NULL;
}



/* FUNCTION: nsddnsRrnNinus()
 *
 * Process a "rrnninus" command
 *
 *
 *  CLASS    TYPE     RDATA    Meaning
 *  ------------------------------------------------------------
 *   NONE     ANY      empty    Name is not in use
 *
 * Name is not in use.  No RR of any type is owned by a
 *    specified NAME.  Note that this prerequisite IS satisfied by
 *       empty nonterminals.
 *
 *
 * PARAM1: void *pio
 *
 * RETURNS: 
 */

static char *  
nsddnsRrnNinus(void *pio, FILE *fp, char *inWrk)
{
   char *wrk = inWrk;
   char *wrk2;
   char inBuf[MAX_DDNS_STR_LEN];
   RR *RRp;
   short len;

   if ((wrk = strstr(wrk, "{")) == NULL)
   {
      ns_printf(pio, "expects '{', line [%d]\n", nsddnsLineNo);
      return NULL;
   }

   /* got "rrnninus" find "{" ... "}"*/

   /* found "{" - get exists (value independent) */
   wrk2 = strtok(wrk, "{, \t}");

   RRp = (RR*) npalloc(sizeof(RR));
   MEMSET(RRp, 0, sizeof(RR));

   len = dnc_copyout(RRp->name, wrk2);
	
   RRp->next = DDNSReqs.preqs;
   DDNSReqs.preqs = RRp;
   RRp->dclass = C_NONE;
   RRp->type = C_ANY;

   /* alloc memory for full RR */
   RRp->RdataP = wrk = (char *) npalloc(len + TYPE_RDLENGTH_SIZE);
   MEMSET(wrk, 0, len + TYPE_RDLENGTH_SIZE);

   MEMCPY(wrk, RRp->name, len);
   RRp->RDlength = len + TYPE_RDLENGTH_SIZE;

   wrk += len;
   PUTSHORT(RRp->type, wrk);
   PUTSHORT(RRp->dclass, wrk);
   wrk += 4; /* TTL = 0 */
   PUTSHORT(0, wrk);

   wrk = wrk2 + strlen(wrk2) + 1;
   
   /* got "}"? */
   if ((wrk = strstr(wrk, "}")))
   {
      nsddnsLineNo++;
      return fgets(inBuf, sizeof(inBuf), fp);
   }

   ns_printf(pio, "rrnninus expects '}', line [%d]\n", nsddnsLineNo);
   return NULL;
}



/* FUNCTION: nsddnsUps()
 * 
 * Process a "ups" command
 *
 * PARAM1: void *pio
 *
 * RETURNS: 
 */

static char *  
nsddnsUps(void *pio, FILE *fp, char *inWrk)
{
	char *wrk = inWrk;
	char *wrk2;
	char inBuf[MAX_DDNS_STR_LEN];
	int fbrace = 0;

	nsddnsLineNo--;
    inBuf[0] = 0;

   /* got "ups" find "{" ... "}"*/

   do
   {
      nsddnsLineNo++;
      if (inBuf[0] == ';')
         continue;

      if (!fbrace)
      {
         if ((wrk = strstr(wrk, "{")) == NULL)
            continue;
         else
            wrk++;
      }

      fbrace = 1;

      if (*wrk == '\n')
         continue;

RELOOP:
      if ((*wrk == '\n') || (*wrk == ';'))
         continue;

      /* found "{" - get prerequisites */
      wrk2 = wrk;
      wrk = strtok(wrk, "{ \t\n}");

      if (wrk)
      {
         wrk2 = wrk + strlen(wrk) + 1;

         /* add RRs to an RRset */
         if (strcmp(wrk, "rr") == 0)
         {
            if (!(wrk = nsddnsRr(pio, fp, wrk2)))
               return NULL;
            else
            {
               wrk = strcpy(inBuf, wrk);
               goto RELOOP; /* GOTO! */
            }
         }
         else
         /* delete an RRset */
            if (strcmp(wrk, "drrs") == 0)
            {
               if (!(wrk = nsddnsdRrs(pio, fp, wrk2)))
                  return NULL;
               else
               {
                  wrk = strcpy(inBuf, wrk);
                  goto RELOOP; /* GOTO! */
               }
            }
            else
               /* delete all RRsets from a name */
               if (strcmp(wrk, "darr") == 0)
               {
                  if (!(wrk = nsddnsdaRr(pio, fp, wrk2)))
                     return NULL;
                  else
                  {
                     wrk = strcpy(inBuf, wrk);
                     goto RELOOP; /* GOTO! */
                  }
               }
               else
                  /* delete an R from an RRset */
                  if (strcmp(wrk, "drr") == 0)
                  {
                     if (!(wrk = nsddnsdRr(pio, fp, wrk2)))
                        return NULL;
                     else
                     {
                        wrk = strcpy(inBuf, wrk);
                        goto RELOOP; /* GOTO! */
                     }
                  }
      }
      else 
         wrk = wrk2;
 
      /* got "}"? */
      if ((wrk = strstr(wrk, "}")))
      {
         nsddnsLineNo++;
         return fgets(inBuf, sizeof(inBuf), fp);
      }
   }
   while ((wrk = fgets(inBuf, sizeof(inBuf), fp)));

   ns_printf(pio, "ups has no body, line [%d]\n", nsddnsLineNo);
   return NULL;
}



/* FUNCTION: nsddnsRr()
 * 
 * Process a "rr" command, add RR to RRset
 *
 * PARAM1: void *pio
 *
 * RETURNS: 
 */

static char * 
nsddnsRr(void *pio, FILE *fp, char *inWrk)
{
   char *wrk = inWrk;
   char *wrk2;
   char inBuf[MAX_DDNS_STR_LEN];
   RR *RRp;
   short len;
   short len2;
   short shortWork;
   unsigned long addr;
   unsigned sbitz = 0;

   /* got "rr" find "{" ... "}"*/

   if ((wrk = strstr(wrk, "{")) == NULL)
   {
      ns_printf(pio, "rr expects '{', line [%d]\n", nsddnsLineNo);
      return NULL;
   } 

   /* found "{" - get name, type , data */
   wrk2 = strtok(wrk, "{, \t}");

   RRp = (RR*) npalloc(sizeof(RR));
   MEMSET(RRp, 0, sizeof(RR));

   len = dnc_copyout(RRp->name, wrk2); 

   /* get type  */
   wrk2 = strtok(NULL, "{, \t}");
   RRp->type = atoi(wrk2);

   /* get data  */
   wrk2 = strtok(NULL, "{, \t}");
   RRp->dclass = C_IN;

   /* SOA */
   if (RRp->type == T_SOA)
   {
      /*
       * 3.3.13. SOA RDATA format
       *
       *    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
       *    /                     MNAME                     /
       *    /                                               /
       *    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
       *    /                     RNAME                     /
       *    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
       *    |                    SERIAL                     |
       *    |                                               |
       *    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
       *    |                    REFRESH                    |
       *    |                                               |
       *    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
       *    |                     RETRY                     |
       *    |                                               |
       *    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
       *    |                    EXPIRE                     |
       *    |                                               |
       *    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
       *    |                    MINIMUM                    |
       *    |                                               |
       *    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
       */

      char zmail[MAX_DDNS_STR_LEN];
      int mlen;
      int SOAlen;
      int domLen;
      unsigned long serial;
      u_long zserial;
      u_long zrefresh;
      u_long zretry;
      u_long zexpire;
      u_long zminimum;

      zserial = atol(wrk2);  /* serial */

      wrk2 = strtok(NULL, "{, \t}");
      zrefresh = atol(wrk2);  

      wrk2 = strtok(NULL, "{, \t}");
      zretry = atol(wrk2); 

      wrk2 = strtok(NULL, "{, \t}");
      zexpire = atol(wrk2); 

      wrk2 = strtok(NULL, "{, \t}");
      zminimum = atol(wrk2); 

       mlen = dnc_copyout(zmail, "z.z"); 

      /* alloc memory for full RR */
      RRp->RdataP = wrk = (char *) npalloc(len + TYPE_RDLENGTH_SIZE + 
		   (SOAlen = len + /* comp len of SOA */
		   mlen + /* comp len of mail addr */
		   5 * 4 /* 5 longs */)
		   );
      MEMSET(wrk, 0, len + TYPE_RDLENGTH_SIZE + 
		   SOAlen);

      MEMCPY(wrk, RRp->name, len);

      RRp->RDlength = len + TYPE_RDLENGTH_SIZE + 
		   SOAlen;

      wrk += len;
      domLen = len;

      /* PUTSHORT() is equivalent to an htons() */
      PUTSHORT(RRp->type, wrk);
      PUTSHORT(RRp->dclass, wrk);
      wrk += 4; /* TTL = 0 */
     
      /* length */
      len2 = len = SOAlen;
      PUTSHORT(len, wrk);

      /* SOA */
      MEMCPY(wrk, RRp->name, domLen);
      wrk += domLen;
      MEMCPY(wrk, zmail, mlen);
      wrk += mlen;

      PUTLONG(zserial, wrk);
      PUTLONG(zrefresh, wrk);
      PUTLONG(zretry, wrk);
      PUTLONG(zexpire, wrk);
      PUTLONG(zminimum, wrk);
   }
   else
   {
      /* CNAME */
      if ((RRp->type == T_CNAME) || (RRp->type == T_NS))
      {
         char zname[MAX_DDNS_STR_LEN];
         int cnameLen;

         cnameLen = dnc_copyout(zname, wrk2); 

         /* alloc memory for full RR */
         RRp->RdataP = wrk = (char *) npalloc(len + TYPE_RDLENGTH_SIZE + cnameLen);
         RRp->RDlength = len + TYPE_RDLENGTH_SIZE + cnameLen;
         MEMSET(wrk, 0, len + TYPE_RDLENGTH_SIZE + cnameLen);
         MEMCPY(wrk, RRp->name, len);

         wrk += len;

         /* PUTSHORT() is equivalent to an htons() */
         PUTSHORT(RRp->type, wrk);
         PUTSHORT(RRp->dclass, wrk);
         wrk += 4; /* TTL = 0 */

         PUTSHORT(cnameLen, wrk);
         MEMCPY(wrk, zname, cnameLen);
      }
      else
      {
         /* alloc memory for full RR */
         RRp->RdataP = wrk = (char *) npalloc(len + TYPE_RDLENGTH_SIZE + sizeof(addr));
         MEMSET(wrk, 0, len + TYPE_RDLENGTH_SIZE + sizeof(addr));

         MEMCPY(wrk, RRp->name, len);
         RRp->RDlength = len + TYPE_RDLENGTH_SIZE + sizeof(addr);

         wrk += len;

         /* PUTSHORT() is equivalent to an htons() */
         PUTSHORT(RRp->type, wrk);
         PUTSHORT(RRp->dclass, wrk);
         wrk += 4; /* TTL = 0 */
     
         if (parse_ipad(&addr,  &sbitz, wrk2) != NULL)
         {
            ns_printf(pio, "bad IP addr, line [%d]\n", nsddnsLineNo);
            return NULL;
         }

         len2 = len = sizeof(addr);
         PUTSHORT(len, wrk);
         MEMCPY(wrk, &addr, len2);
      }
   } 

   RRp->next = DDNSReqs.ups;
   DDNSReqs.ups = RRp;

   wrk = wrk2 + strlen(wrk2) + 1;
		   
   /* got "}"? */
   if (strstr(wrk, "}"))
   {
      nsddnsLineNo++;
      return fgets(inBuf, sizeof(inBuf), fp);
   }

   ns_printf(pio, "rr expects '}', line [%d]\n", nsddnsLineNo);
   return NULL;
}



/* FUNCTION: nsddnsdRrs()
 * 
 * Process a "drrs" command, delete RR set
 *
 * PARAM1: void *pio
 *
 * RETURNS: 
 */

static char * 
nsddnsdRrs(void *pio, FILE *fp, char *inWrk)
{
   char *wrk = inWrk;
   char *wrk2;
   char inBuf[MAX_DDNS_STR_LEN];
   RR *RRp;
   short len;

   /* got "drrs" find "{" ... "}"*/

   if ((wrk = strstr(wrk, "{")) == NULL)
   {
      ns_printf(pio, "rr expects '{', line [%d]\n", nsddnsLineNo);
      return NULL;
   } 

   /* found "{" - get name, type , data */
   wrk2 = strtok(wrk, "{, \t}");

   RRp = (RR*) npalloc(sizeof(RR));
   MEMSET(RRp, 0, sizeof(RR));

   len = dnc_copyout(RRp->name, wrk2);

   wrk2 = strtok(NULL, "{, \t}");

   RRp->type = atoi(wrk2);

   RRp->dclass = C_ANY;

   /* alloc memory for full RR */
   RRp->RdataP = wrk = (char *) npalloc(len + TYPE_RDLENGTH_SIZE);
   RRp->RDlength = len + TYPE_RDLENGTH_SIZE;
   MEMSET(wrk, 0, len + TYPE_RDLENGTH_SIZE);

   MEMCPY(wrk, RRp->name, len);

   wrk += len;

   PUTSHORT(RRp->type, wrk);
   PUTSHORT(RRp->dclass, wrk);
   wrk += 4; /* TTL = 0 */
   PUTSHORT(0, wrk);

   RRp->next = DDNSReqs.ups;
   DDNSReqs.ups = RRp;

   wrk = wrk2 + strlen(wrk2) + 1;
		   
   /* got "}"? */
   if (strstr(wrk, "}"))
   {
      nsddnsLineNo++;
      return fgets(inBuf, sizeof(inBuf), fp);
   }

  ns_printf(pio, "drrs expects '}', line [%d]\n", nsddnsLineNo);
  return NULL;
}


/* FUNCTION: nsddnsdaRr()
 * 
 * Process a "darr" command, delete all RR from a set
 *
 * PARAM1: void *pio
 *
 * RETURNS: 
 */

static char * 
nsddnsdaRr(void *pio, FILE *fp, char *inWrk)
{
   char *wrk = inWrk;
   char *wrk2;
   char inBuf[MAX_DDNS_STR_LEN];
   RR *RRp;
   short len;

   /* got "darr" find "{" ... "}"*/

   if ((wrk = strstr(wrk, "{")) == NULL)
   {
      ns_printf(pio, "rr expects '{', line [%d]\n", nsddnsLineNo);
      return NULL;
   } 

   /* found "{" - get name */
   wrk2 = strtok(wrk, "{ \t}");

   RRp = (RR*) npalloc(sizeof(RR));
   MEMSET(RRp, 0, sizeof(RR));

   len = dnc_copyout(RRp->name, wrk2);

   RRp->type = T_ANY;
   RRp->dclass = C_ANY;

   /* alloc memory for full RR */
   RRp->RdataP = wrk = (char *) npalloc(len + TYPE_RDLENGTH_SIZE);
   MEMSET(wrk, 0, len + TYPE_RDLENGTH_SIZE);

   MEMCPY(wrk, RRp->name, len);
   wrk += len;
   RRp->RDlength = len + TYPE_RDLENGTH_SIZE;

   PUTSHORT(RRp->type, wrk);
   PUTSHORT(RRp->dclass, wrk);
   wrk += 4; /* TTL = 0 */
   PUTSHORT(0, wrk);

   RRp->next = DDNSReqs.ups;
   DDNSReqs.ups = RRp;

   wrk = wrk2 + strlen(wrk2) + 1;
		   
   /* got "}"? */
   if (strstr(wrk, "}"))
   {
      nsddnsLineNo++;
      return fgets(inBuf, sizeof(inBuf), fp);
   }

  ns_printf(pio, "darr expects '}', line [%d]\n", nsddnsLineNo);
  return NULL;
}



/* FUNCTION: nsddnsdRr()
 * 
 * Process a "drr" command, delete an RR from an RR set
 *
 * PARAM1: void *pio
 *
 * RETURNS: 
 */

static char * 
nsddnsdRr(void *pio, FILE *fp, char *inWrk)
{
   char *wrk = inWrk;
   char *wrk2;
   char inBuf[MAX_DDNS_STR_LEN];
   RR *RRp;
   short len;
   short len2;
   unsigned long addr;
   unsigned sbitz = 0;

   /* got "drr" find "{" ... "}"*/

   if ((wrk = strstr(wrk, "{")) == NULL)
   {
      ns_printf(pio, "rr expects '{', line [%d]\n", nsddnsLineNo);
      return NULL;
   } 

   /* found "{" - get name, type , data */
   wrk2 = strtok(wrk, "{, \t}");

   RRp = (RR*) npalloc(sizeof(RR));
   MEMSET(RRp, 0, sizeof(RR));

   len = dnc_copyout(RRp->name, wrk2);

   wrk2 = strtok(NULL, "{, \t}");

   RRp->type = atoi(wrk2);
   RRp->dclass = C_NONE;

   wrk2 = strtok(NULL, "{, \t}");
   RRp->dclass = C_NONE;

   /* SOA */
   if (RRp->type == T_SOA)
   {
      /*
       * 3.3.13. SOA RDATA format
       *
       *    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
       *    /                     MNAME                     /
       *    /                                               /
       *    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
       *    /                     RNAME                     /
       *    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
       *    |                    SERIAL                     |
       *    |                                               |
       *    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
       *    |                    REFRESH                    |
       *    |                                               |
       *    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
       *    |                     RETRY                     |
       *    |                                               |
       *    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
       *    |                    EXPIRE                     |
       *    |                                               |
       *    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
       *    |                    MINIMUM                    |
       *    |                                               |
       *    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
       */

      char zmail[MAX_DDNS_STR_LEN];
      int mlen;
      int SOAlen;
      int domLen;
      u_long zserial;
      u_long zrefresh;
      u_long zretry;
      u_long zexpire;
      u_long zminimum;

      zserial = atol(wrk2);  /* serial */

      wrk2 = strtok(NULL, "{, \t}");
      zrefresh = atol(wrk2);  

      wrk2 = strtok(NULL, "{, \t}");
      zretry = atol(wrk2); 

      wrk2 = strtok(NULL, "{, \t}");
      zexpire = atol(wrk2); 

      wrk2 = strtok(NULL, "{, \t}");
      zminimum = atol(wrk2); 

       mlen = dnc_copyout(zmail, "z.z"); 

       /* alloc memory for full RR */
       RRp->RdataP = wrk = (char *) npalloc(len + TYPE_RDLENGTH_SIZE + 
		   (SOAlen = len + /* comp len of SOA */
		   mlen + /* comp len of mail addr */
		   5 * 4 /* 5 longs */));

       MEMSET(wrk, 0, len + TYPE_RDLENGTH_SIZE + SOAlen);
       MEMCPY(wrk, RRp->name, len);

       RRp->RDlength = len + TYPE_RDLENGTH_SIZE + SOAlen;

       wrk += len;
       domLen = len;

       /* PUTSHORT() is equivalent to an htons() */
       PUTSHORT(RRp->type, wrk);
       PUTSHORT(RRp->dclass, wrk);
       wrk += 4; /* TTL = 0 */
     
       /* length */
       len2 = len = SOAlen;
       PUTSHORT(len, wrk);

       /* SOA */
       MEMCPY(wrk, RRp->name, domLen);
       wrk += domLen;
       MEMCPY(wrk, zmail, mlen);
       wrk += mlen;

       PUTLONG(zserial, wrk);
       PUTLONG(zrefresh, wrk);
       PUTLONG(zretry, wrk);
       PUTLONG(zexpire, wrk);
       PUTLONG(zminimum, wrk);
   }
   else
   {
      /* CNAME */
      if ((RRp->type == T_CNAME) || (RRp->type == T_NS))
      {
         char zname[MAX_DDNS_STR_LEN];
         int cnameLen;

         cnameLen = dnc_copyout(zname, wrk2); 

         /* alloc memory for full RR */
         RRp->RdataP = wrk = (char *) npalloc(len + TYPE_RDLENGTH_SIZE + cnameLen);
         RRp->RDlength = len + TYPE_RDLENGTH_SIZE + cnameLen;
         MEMSET(wrk, 0, len + TYPE_RDLENGTH_SIZE + cnameLen);
         MEMCPY(wrk, RRp->name, len);

         wrk += len;

         /* PUTSHORT() is equivalent to an htons() */
         PUTSHORT(RRp->type, wrk);
         PUTSHORT(RRp->dclass, wrk);
         wrk += 4; /* TTL = 0 */

         PUTSHORT(cnameLen, wrk);
         MEMCPY(wrk, zname, cnameLen);
      }   
      else
      {
         /* alloc memory for full RR */
         RRp->RdataP = wrk = (char *) npalloc(len + TYPE_RDLENGTH_SIZE + sizeof(addr));
         MEMSET(wrk, 0, len + TYPE_RDLENGTH_SIZE + sizeof(addr));

         MEMCPY(wrk, RRp->name, len);
         RRp->RDlength = len + TYPE_RDLENGTH_SIZE + sizeof(addr);

         wrk += len;
         PUTSHORT(RRp->type, wrk);
         PUTSHORT(RRp->dclass, wrk);
         wrk += 4; /* TTL = 0 */

         if (parse_ipad(&addr, &sbitz, wrk2) != NULL)
         {
            ns_printf(pio, "bad IP addr, line [%d]\n", nsddnsLineNo);
            return NULL;
         }

         len2 = len = sizeof(addr);
         PUTSHORT(len, wrk);

         MEMCPY(wrk, &addr, len2);
      }
   }

   RRp->next = DDNSReqs.ups;
   DDNSReqs.ups = RRp;

   wrk = wrk2 + strlen(wrk2) + 1;
		   
   /* got "}"? */
   if (strstr(wrk, "}"))
   {
      nsddnsLineNo++;
      return fgets(inBuf, sizeof(inBuf), fp);
   }

  ns_printf(pio, "drr expects '}', line [%d]\n", nsddnsLineNo);
  return NULL;
}



/* FUNCTION: nsddnsZone()
 * 
 * Process a "zone" command
 *
 * PARAM1: void *pio
 *
 * RETURNS: 
 */

static char *  
nsddnsZone(void * pio, FILE *fp, char *inWrk)
{
   char *wrk2;
   char *wrk = inWrk;
   char inBuf[MAX_DDNS_STR_LEN];

   if (DDNSReqs.zone)
   {
      ns_printf(pio,"only one ZONE is allowed, line [%d]\n", nsddnsLineNo);
      return NULL;
   }

   /* got "zone" find "{" ... "}"*/

   if ((wrk = strstr(wrk, "{")) == NULL)
   {
      ns_printf(pio, "zone expects '{', line [%d]\n", nsddnsLineNo);
      return NULL;
   } 

   wrk2 = strtok(wrk, "{ \t}");

   DDNSReqs.zone = (char *)npalloc(strlen(wrk2) + 1);
   strcpy(DDNSReqs.zone, wrk2);

   wrk = wrk2 + strlen(wrk2) + 1;

   /* got "}"? */
   if (strstr(wrk, "}"))
   {
      nsddnsLineNo++;
      return fgets(inBuf, sizeof(inBuf), fp);
   }

   ns_printf(pio, "zone expects '}', line [%d]\n", nsddnsLineNo);
   return NULL;
}

#endif /* DNS_CLIENT_UPDT */

#endif   /* DNS_CLIENT */

/* end of file dnsclnt.c */

