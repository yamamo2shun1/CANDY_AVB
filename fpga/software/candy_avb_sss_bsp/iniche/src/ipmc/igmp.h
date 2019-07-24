/*
 * FILENAME: igmp.h
 *
 * Copyright 1997- 2000 By InterNiche Technologies Inc. All rights reserved
 *
 *
 * MODULE: IPMC
 *
 *
 * PORTABLE: yes
 */

/* Additional Copyrights: */

/*
 * FILENAME: igmp.h
 *
 *
 * Internet Group Management Protocol (IGMP) definitions.
 *
 * Copyright (c) 1988 Stephen Deering.
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. [rescinded 22 July 1999]
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * @(#)igmp.h 8.1 (Berkeley) 6/10/93
 */

#ifndef IGMP_H
#define IGMP_H

/* Internet Group Management Protocol (IGMP) definitions. */

#ifdef IP_MULTICAST

/* protocol identifier for IGMP in IP header */
#define  IGMP_PROT 2

/* IGMP message format.*/
struct igmp {
   u_char   igmp_type;  /* version & type of IGMP message  */
   u_char   igmp_code;  /* unused, should be zero          */
   u_short  igmp_cksum; /* IP-style checksum               */
   ip_addr  igmp_group; /* group address being reported    */
};                      /* (zero for queries)              */

/* message types, incl. version number */
#define  IGMP_HOST_MEMBERSHIP_QUERY 0x11
#define  IGMP_HOST_MEMBERSHIP_REPORT 0x12

/* max delay for response to Query message (10 seconds)  */
#define  IGMP_MAX_HOST_REPORT_DELAY 10
/* all-hosts multicast address group (224.0.0.1) */ 
#define  INADDR_ALLHOSTS_GROUP   0xe0000001

/*
 * Internet Group Management Protocol (IGMP),
 * implementation-specific definitions.
 *
 * Written by Steve Deering, Stanford, May 1988.
 *
 * MULTICAST 1.1
 */
 
/*
 * Macro to compute a random timer value between 1 and (IGMP_MAX_REPORTING_
 * DELAY * countdown frequency).  We generate a "random" number by adding
 * the total number of IP packets received, our primary IP address, and the
 * multicast address being timed-out.  The 4.3 random() routine really
 * ought to be available in the kernel!
 */
#define IGMP_RANDOM_DELAY(multiaddr) \
      /* struct in_addr multiaddr; */ \
      ( (ip_mib.ipInReceives + \
      ntohl(nets[0]->n_ipaddr) + \
      ntohl((multiaddr)) \
      ) \
      % (IGMP_MAX_HOST_REPORT_DELAY * PR_FASTHZ) + 1 \
      )

int igmp_init (void);
int igmp_input (PACKET p);
void igmp_joingroup (struct in_multi *);
void igmp_leavegroup (struct in_multi *);
void igmp_fasttimo (void);

extern ip_addr igmp_all_hosts_group;
extern u_long igmp_cticks;

#endif   /* IP_MULTICAST */

#endif   /* IGMP_H */

