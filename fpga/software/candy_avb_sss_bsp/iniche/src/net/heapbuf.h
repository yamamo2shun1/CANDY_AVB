/* heapbuf.h
 *
 * Buffers allocated from the heap are called heap buffers.  The other two 
 * types of buffers, little buffers and big buffers, are referred to as 
 * regular buffers.
 */

#ifndef _HEAPBUF_H_
#define _HEAPBUF_H_ 1

/* enumerated type that is used to describe whether the heap memory
 * is "protected" (i.e., its consistency is maintained) via semaphores 
 * (HEAP_ACCESS_BLOCKING) or other mechanisms that do not cause the 
 * caller to block (such as by disabling/enabling interrupts) (HEAP_-
 * ACCESS_NONBLOCKING).
 */
enum heap_type
{
HEAP_ACCESS_BLOCKING,
HEAP_ACCESS_NONBLOCKING
};

/* Data structure that is used to contain information about heap-based
 * allocations (primarily for debugging purposes).  Since this data
 * structure is managed via the primitives in q.c/q.h, the 'next' field
 * must be the first field in the data structure.  This data structure
 * is referred to as a PHLE in the code and documentation.
 */
struct priv_hbuf_list
{
   struct priv_hbuf_list * next; /* must be first field in structure */
   struct netbuf * netbufp;      /* pointer to PACKET */
   char * databufp;              /* pointer to data buffer */
   u_long length;                /* length of data buffer, including guard band */
};

typedef struct priv_hbuf_list PHLE;
typedef struct priv_hbuf_list * PHLEP;

/* Indexes into the hbufstats [] array for various failure and success conditions 
 * encountered in the heap buffer allocation and deallocation processes */
#define TOOBIG_ALLOC_ERR             0 /* number of times allocation requested a length greater than 
                                        * MAX_ALLOC_FROM_HEAP */
#define LIMIT_EXCEEDED_ERR           1 /* number of times a request for an allocation from the heap would
                                        * have caused the max threshold (MAX_TOTAL_HEAP_ALLOC) to be exceeded */
#define NB_ALLOCFAIL_ERR             2 /* number of times couldn't allocate struct netbuf block from heap */
#define DB_ALLOCFAIL_ERR             3 /* number of times couldn't allocate data buffer block from heap */
#define PHLEB_ALLOCFAIL_ERR          4 /* number of times couldn't allocate private heapbuf list element block from heap */
#define HB_ALLOC_SUCC                5 /* number of times successfully allocated memory for struct netbuf 
                                        * and data buffer (and private heapbuf list block, if HEAPBUFS_DEBUG 
                                        * is defined) */
#define INCONSISTENT_HBUF_LEN_ERR    6 /* number of instances when either (1) packet being freed was greater 
                                        * than bigbufsiz (in 'nb_blen' field), but 'flags' did not have 
                                        * PKF_HEAPBUF flag set OR (2) when packet being freed had a length 
                                        * field in 'nb_blen' that was not consistent with what was stored 
                                        * in the corresponding entry in the private heapbuf list */
#define NO_PHLE_ERR                  7 /* number of times packet being freed did not match any entry in the
                                        * private heapbuf list */
#define HB_GUARD_BAND_VIOLATED_ERR   8 /* number of instances when pk_validate () discovered that a heap buffer's
                                        * left or right guard band was violated */
#define HBUF_NUM_STATS               9 /* number of entries in hbufstats array */

/* maximum amount of memory that can be allocated from the heap at 
 * any given time (this is the sum total of all heap-based allocations) */
#define MAX_TOTAL_HEAP_ALLOC 524288 /* 512K bytes */

/* maximum amount of memory that can allocated from the heap in one 
 * request (i.e., via one call to pk_alloc ()) */
#define MAX_INDIVIDUAL_HEAP_ALLOC 65535

#endif /* _HEAPBUF_H_ */