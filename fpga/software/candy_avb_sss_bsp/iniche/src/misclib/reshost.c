/*
 * FILENAME: reshost.c
 *
 * Copyright 1997- 2000 By InterNiche Technologies Inc. All rights reserved
 *
 * Mid level API for DNS resolutions.
 *
 * MODULE: MISCLIB
 *
 * ROUTINES: in_reshost(),
 *
 * PORTABLE: yes
 */

/* Additional Copyrights: */
/* Portions Copyright 1993 by NetPort software */


#include "ipport.h"
#include "netbuf.h"
#include "net.h"

#include "in_utils.h"

#include "dns.h"      




/* FUNCTION: in_reshost()
 *
 * in_reshost(char addr, ip_addr result, int flags) Resolve an IP 
 * address text string into an actual IP address. Calls DNS if 
 * supported. flags word is a bit mask of the RH_ values in dns.h 
 * Returns 0 if address was set, else one of the ENP_ error codes 
 *
 * 
 * PARAM1: char * host     - IN - textual IP address or host name
 * PARAM2: ip_addr * address - OUT - address if successful
 * PARAM3: int    flags    - IN - RH_VERBOSE, RH_BLOCK
 *
 * RETURNS: 
 */

int
in_reshost(char * host, /* IN - textual IP address or host name */
   ip_addr *   address, /* OUT - address if successful */
   int   flags)         /* IN - RH_VERBOSE, RH_BLOCK */
{
   char *   cp;      /* error holder */
   unsigned snbits;  /* for pass to parse_ipad() */
#ifdef DNS_CLIENT
   int   e;          /* Net error code */
   u_long   tmo;     /* timeout for blocking calls */
   int   blocking =  flags &  RH_BLOCK;
#endif   /* DNS_CLIENT */
   int   verbose  =  flags &  RH_VERBOSE;

   cp = parse_ipad(address, &snbits, host);

   if (!cp) /* worked, string was parsable dot notation */
      return 0;

#ifndef DNS_CLIENT
   if (verbose)
      dprintf("Unable to parse IP host %s.\nerror: %s\n", host, (char*)cp);
   return ENP_PARAM;
#else /* DNS_CLIENT enabled in build */

   if (verbose)
      dprintf("can't parse %s (%s), trying DNS\n", host, cp);

   tmo = cticks + (5 * TPS);  /* set timeout value */

   if (dns_servers[0])  /* dont bother if no servers are set */
   {
      if (verbose)
         dprintf("trying DNS lookup...\n");
      e = dns_query(host, address);
      if (e == ENP_SEND_PENDING)
      {
         if (blocking)
         {
            while (tmo > cticks)
            {
               tk_yield();
               e = dns_query(host, address);
               if (e == 0)
                  goto rh_got_dns;
            }
         }
         if (verbose)
            dprintf("DNS inquiry sent\n");
         return 0;
      }
      else if(e == 0)
      {
         rh_got_dns:
         /* DNS resolution worked */
         if (verbose)
            dprintf("active host found via DNS (%u.%u.%u.%u)\n", 
          PUSH_IPADDR(*address));
         return 0;
      }
      else if(e == ENP_TIMEOUT)  /* timeout? */
      {
         if (verbose)
            dprintf("DNS timeout");
      }
      else
      {
         if (verbose)
            dprintf("DNS error %d", e);
      }
      if (verbose)
         dprintf(", host not set\n");
      return e;
   }

   if (verbose)
      dprintf("DNS/host-parse failed.\n");
   return ENP_PARAM;
#endif   /* DNS_CLIENT */
}



