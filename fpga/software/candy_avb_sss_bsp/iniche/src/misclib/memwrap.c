/* memwrap.c

   Wrapper routines for heap managment routines. These routines bracket
 the comventional heap functions like alloc and free, and collect
 diagnostic information. Markers to detect buffer overwrites are installed
 before and after the allocated blocks, and checked on frees. A table
 is also kept to track how many blocks of each size are allocated and
 freed. This information can be used to spot memory leaks and optimize
 heap usage by the applications.


*/

#include "ipport.h"
#include "in_utils.h"
#include "memwrap.h"    /* include this AFTER ALIGN_TYPE */


#ifdef MEM_WRAPPERS     /* whole file can be ifdeffed */

extern int memtrapsize;     /* alloc size to dtrap on */

struct memman
{
#if (ALIGN_TYPE > 1)
   char  status[ALIGN_TYPE];  /* Marker flag, free flag */
#else
   char  status[2];           /* need at least two bytes */
#endif   /* ALIGN_TYPE */
   unsigned length;           /* size of alloced block */
   char * data;               /* pointer to alloced data */
};


/* memory call counters */
struct WrapMemStats
{
   unsigned long brallocs;    /* alloc call counter */
   unsigned long brfrees;     /* free call counter */
   unsigned long brallocb;    /* alloced byte counter */
   unsigned long brfreeb;     /* freed byte counter */
   unsigned long brfailed;    /* alloc routine failures */
   unsigned long brmaxmem;    /* max. memory alloced at one time */
   unsigned brmaxsize;        /* largest single alloc */
   char * br_hiaddr;          /* highest address alloced */
   char * br_loaddr;          /* lowest address alloced */
} brmem;

#ifndef MAXHIT
#define MAXHIT 30
#endif   /* MAXHIT */

/* struct to track how many of each block size we alloc */
struct memhit
{
   unsigned size;             /* zero until entry is used */
   unsigned long blocks;      /* total alloc calls for this size */
   unsigned long curr;        /* current bytes alloced for this size */
} hitct[MAXHIT];

int
wrap_stats(void * pio)
{
   int   i;                   /* hitct index */
   unsigned long cblocks;     /* current blocks outstanding */
   char  outbuf[50];

   ns_printf(pio, "wrappers: allocs: %lu,  frees: %lu,  allocbytes: %lu   freebytes: %lu\n",
      brmem.brallocs, brmem.brfrees, brmem.brallocb, brmem.brfreeb );

   ns_printf(pio, "alloced: current bytes: %lu  max bytes: %lu\n",
      brmem.brallocb - brmem.brfreeb, brmem.brmaxmem );

   ns_printf(pio, "biggest block: %u,  allocs failed: %lu\n",
      brmem.brmaxsize, brmem.brfailed );

   ns_printf(pio, "Block sizes, in size[alloc - free = cur] format:");
   for(i = 0; i < MAXHIT; i++)
   {
      if(hitct[i].size == 0) break;
      if(i%3 == 0)
         ns_printf(pio, "\n");

      cblocks = hitct[i].curr/hitct[i].size; /* current blocks of this size */
      sprintf(outbuf, "%5u[%lu-%lu=%lu]",
         hitct[i].size,
         hitct[i].blocks,              /* allocated blocks of this size */
         hitct[i].blocks - cblocks,    /* - freed blocks of this size */
         cblocks);                     /* = current blocks of this size */
      if(strlen(outbuf) > 50) panic("wrap");
      ns_printf(pio, "%-26s", outbuf);
   }
   ns_printf(pio, "\n");
   return 0;
}


char *
wrap_alloc(unsigned size, char *(*alloc_rtn)(unsigned))
{
   int      i;
   int      free;
   int      wrapsize;      /* size to alloc including wrap info */
   char *   cp;
   struct memman * manp;

   if(size == memtrapsize)
   {
      dtrap();
   }

   /* allocate block big enough for caller plus warpping struct */
   wrapsize = size + sizeof(struct memman) + ALIGN_TYPE;
   cp = (*alloc_rtn)(wrapsize);

   if(!cp)  /* alloc couldn't get memory */
   {
      brmem.brfailed++;
      return NULL;
   }

   manp = (struct memman *)cp;      /* set mgt structure */
   cp += sizeof(struct memman);     /* set pointer to return */

   /* check for newest hi/low address */
   if(cp > brmem.br_hiaddr)
      brmem.br_hiaddr = cp;
   if((cp < brmem.br_loaddr) || (brmem.br_loaddr == NULL))
      brmem.br_loaddr = cp;

   /* fill in wrapping structure for later wrap_free() checking */
   manp->status[0] = 'M';     /* add memory marker */
   manp->status[1] = 'M';     /* mark as NOT free */
   manp->data = cp;
   manp->length = size;
   *(cp + size) = 'M';     /* end memory marker */

   /* keep track of hits on each block size */
   free = -1;  /* use -1 to indicate no free hitct[] entry */
   for(i = 0; i < MAXHIT; i++)
   {
      if(hitct[i].size == size)  /* found entry for this size */
      {
         hitct[i].curr += size;
         hitct[i].blocks++;
         break;
      }
      if(hitct[i].size == 0)  /* size not in array */
      {
         free = i;   /* remember first free slot */
         break;      /* make new entry */
      }
   }
   if(free != -1)  /* see if we should start an entry for this size */
   {
      hitct[i].curr = hitct[i].size = size;
      hitct[i].blocks = 1;
   }

   brmem.brallocs++;
   brmem.brallocb += size;
   if(brmem.brmaxsize < size) /* new record for a single alloc? */
      brmem.brmaxsize = size;
   if(brmem.brmaxmem < (brmem.brallocb - brmem.brfreeb)) /* new record for allocation? */
      brmem.brmaxmem = brmem.brallocb - brmem.brfreeb;

   return (cp);
}

void
wrap_free(char * ptr, void(*free_rtn)(char *))
{
   int      i;
   char *   cp;
   struct memman * manp;

   /* make sure the pointer is within the previously allocated range */
   if((ptr > brmem.br_hiaddr) ||
      (ptr < brmem.br_loaddr))
   {
      dtrap(); /* maybe this should be a panic.... */
      return;
   }

   /* back up from pointer to get memory manager struct */
   manp = (struct memman *)ptr;
   manp--;
   cp = manp->data;
   if(manp->status[0] != 'M')    /* Make sure marker is present */
   {
      dtrap(); /* bad pointer or corrupt memory */
      return;  /* don't confuse system free() */
   }
   if(manp->status[1] == 'F')    /* Double free ? */
   {
      dtrap(); /* bad pointer or corrupt memory */
      return;  /* don't confuse system free() */
   }
   manp->status[1] = 'F';     /* mark as free */
   if(*(cp + manp->length) != 'M')
   {
      dtrap();    /* should be panic */
      return;
   }
   brmem.brfrees++;
   brmem.brfreeb += manp->length;
   for(i = 0; i < MAXHIT; i++)
   {
      if(hitct[i].size == manp->length)
      {
         hitct[i].curr -= manp->length;
         break;
      }
   }

   cp = (char*)manp;
   
   free_rtn(cp);   /* memory came through wrap_alloc(), free it */
}

#endif   /* MEM_WRAPPERS */

