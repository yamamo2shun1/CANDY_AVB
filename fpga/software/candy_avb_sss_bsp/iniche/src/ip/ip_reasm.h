/* ip_reasm.h 
 *
 * Header file for IP reassembly module (ip_reasm.c).
 */
#ifndef _IPREASM_H_
#define _IPREASM_H_ 1

/* upper limit on the amount of memory that can be consumed by the IP 
 * reassembly module.  This counts memory consumed by the received
 * fragments, IRE, and RFQ data structures. */
#define IP_REASM_MAX_MEM 393216 /* 384K */

/* return codes from functions in the IP reassembly module */
#define IPREASM_OK 0     /* success */
#define IPREASM_FAIL 1   /* failure */
#define IPREASM_ERROR -1 /* failure */
#define IPREASM_TRUE 1   /* true */
#define IPREASM_FALSE 0  /* false */

/* enumeration for return code from ip_reasm_compute_overlap () */
enum ipreasm_rc
{
IPREASM_DROP_FRAG_DUPLICATE, /* drop just received fragment because it does not contain any unique 
                              * data (i.e., data that is not present in one (or more) of the 
                              * already queued fragments) */
IPREASM_DROP_FRAG_BAD_PARAM, /* drop just received fragment because IRE pointer failed validation */
IPREASM_ACCEPT_FRAG          /* accept just received fragment */
};

typedef enum ipreasm_rc IPREASM_RC;

/* statistics data structure for the IP reassembly module */
struct ire_stats
{
   u_long bad_irep;       /* bad IRE pointer supplied to function */
   u_long ire_timed_out;  /* number of IREs that timed out while waiting 
                           * for reassembly to complete */
   u_long bad_max_mem;    /* number of instances when the IRE reassembly 
                           * memory useage counter was greater than its max limit
                           * (when checked in the ip_reasm_xxx_mem_useage () 
                           * calls) */
   u_long mem_check_fail; /* number of instances when a check in ip_reasm_-
                           * check_mem_useage () indicated that the requested 
                           * increment would cause the memory useage counter 
                           * to go over limit */
   u_long mem_incr_fail;  /* number of instances when a check in ip_reasm_-
                           * incr_mem_useage () indicated that the requested 
                           * increment would cause the memory useage counter 
                           * to go over limit */
   u_long mem_decr_fail;  /* number of instances when the memory counter was
                           * smaller than the amount being decremented from it */
};

/* maximum lifetime of an IRE entry */
#define IRE_TMO 120 /* 120 seconds */

/* maximum number of fragments that can be stored in a RFQ (irrespective
 * of whether it is statically or dynamically allocated).  See below for
 * more information on RFQ. */
#define IPR_MAX_FRAGS 16

#define INVALID_FRAG_INDEX IPR_MAX_FRAGS /* legal values 0...15 */

/* RFQ - this data structure contains pointers to queued fragments (PACKET)
 * while they are awaiting reassembly.  One instance of this structure is 
 * statically allocated inside a IRE; other instances can be dynamically 
 * allocated if required.
 */
struct reasm_frag_queue
   {
   struct reasm_frag_queue * next;       /* pointer to the next RFQ */
   struct netbuf * bufp [IPR_MAX_FRAGS]; /* array of slots containing PACKET pointers to received fragments */
   u_short frag_offset [IPR_MAX_FRAGS];  /* starting offset of data in received fragments */
   };

typedef struct reasm_frag_queue RFQ;
typedef struct reasm_frag_queue * RFQP;

/* An IRE (or RFQ) is considered compact if there are no holes in the 
 * RFQs, i.e., if all PACKET pointers (for the received fragments) are 
 * clumped together (without any empty slots between them).  Holes are 
 * created by the code in ip_reasm_compute_overlap () if it determines 
 * that there is an overlap (between a just received fragment and an 
 * already queued fragment) that requires it to drop the already queued 
 * fragment.
 *
 * A compact RFQ allows us to stop (traversing the bufp array) when we 
 * encounter the first empty slot; however, if the RFQ is not compact, 
 * we must continue traversing until we reach a zero next pointer 
 * before we can stop.
 */
#define IPR_RFQ_COMPACT 0x1 /* if bit is set, RFQ is compact, i.e., there are no holes in it */

/* IRE - base data structure used for processing fragmented IP packets
 * thru' reassembly.  Sibling fragments can be identified as having identical 
 * values for the four-tuple of source IP address, destination IP 
 * address, protocol, and identification fields.
 */
struct ip_reasm_entry
   {
   struct ip_reasm_entry * next; /* pointer to next entry in IRE list */
   ip_addr src;    /* source IP address; stored in net endian format */
   ip_addr dest;   /* destination IP address; stored in net endian format */
   u_short id;     /* Identification field; stored in net endian format */
   u_char prot;    /* Protocol field */
   u_short length; /* length of original, unfragmented packet (in bytes) (only counts payload part of IP datagram) */
   u_short rcvd;   /* number of bytes (of packet) received (only counts payload part of IP datagram) */
   u_long age;     /* age - starts off at 0, and can go upto a max of the reassembly timeout (IRE_TMO) */
   RFQ rfq;        /* queue of fragments waiting to be copied (delayed copy scheme) */
   char * l2_hdr;  /* points to start of buffer of first fragment (FF); however, the actual
                    * L2 header may start at an offset from the start of the buffer */
   char * l3_hdr;  /* points to start of IP header of first fragment (FF) */
   unsigned char flags; /* currently only bit 0 is used to indicate if RFQ is "compact" */
   };

typedef struct ip_reasm_entry IRE;
typedef struct ip_reasm_entry * IREP;

enum ip_frag_type
{
IP_CP = 0, /* 000; complete packet (i.e., not a fragment) */
IP_FF = 1, /* 001; first fragment (i.e., one with Fragment Offset of 0) */
IP_MF = 3, /* 011; middle fragment */
IP_LF = 5  /* 101; last fragment */
};

typedef enum ip_frag_type IP_FRAGTYPE;

#endif /* _IPREASM_H_ */

