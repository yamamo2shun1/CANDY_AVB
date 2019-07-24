/*
 * FILENAME: memio.c
 *
 * Copyright 1998- 2000 By InterNiche Technologies Inc. All rights reserved
 *
 * memio.c calloc() & free() workalikes (calloc1() and mem_free()) 
 * designed for embedded systems. 8/21/98 - first use, on AMD Net186. 
 *
 * MODULE: MPC860
 *
 * ROUTINES: sizeof(), mheap_init(), calloc1(), mem_free(), mh_stats(),
 *
 * PORTABLE: yes
 */

#include "ipport.h"
#include "in_utils.h"
#include "memwrap.h"


extern unsigned memtrapsize;     /* alloc size to dtrap on */

struct mem_block
{
#ifdef NPDEBUG
   int         pattern;       /* pattern for testing overwrites */
#endif   /* NPDEBUG */
   struct mem_block *   next; /* pointer to next block */
   unsigned    size;          /* size of block (excluding this header) */
};

/* define a pattern for putting in every mem block - this is used to 
 * check for overwrites. This declaration assumes ALIGN_TYPE is the 
 * same as 
 */

#if (ALIGN_TYPE == 4)
#define  MEM_PATTERN    0x6D656D70  /* ascii for MEMP */
#elif (ALIGN_TYPE == 2)
#define  MEM_PATTERN    0x6D65      /* ascii for ME */
#else
#define  MEM_PATTERN    0x6D        /* ascii M */
#endif

#define MEMBLOCKSIZE ((sizeof(struct mem_block) & (ALIGN_TYPE - 1)) ? \
   ((sizeof(struct mem_block) + ALIGN_TYPE) & ~(ALIGN_TYPE - 1)) : \
   (sizeof(struct mem_block)))

struct mem_block *   mh_free;
struct mem_block *   mheap_base;

long     mh_startfree;  /* heap size (in bytes) at init time */
long     mh_totfree;    /* current free heap size */
long     mh_minfree;    /* minimum free heap size seen */
long     mh_failed;     /* number of times alloc failed */



/* FUNCTION: mheap_init()
 *
 * Called at system init time to set up heap for use. MUST be called
 * before any calls to calloc1(). Takes a single contiguous memory space
 * and sets it up to be used by calloc1() anbd mem_free().
 *
 * PARAM1: char * base - address for start of heap area in memory
 * PARAM2: long size  - size of heap area at address.
 *
 * RETURNS: void
 */


void
mheap_init(char * base, long size)
{
   /* make sure the heap is aligned */
   if ((long)base & (ALIGN_TYPE-1))
   {
      base = (char*)(((long)base + (ALIGN_TYPE-1)) & ~(ALIGN_TYPE-1));
      size -= (ALIGN_TYPE-1);
   }

   mheap_base = (struct mem_block *)base;
   mh_free = (struct mem_block *)base;

   /* trim heap to multiple of ALIGN_TYPE */
   size &= ~(ALIGN_TYPE-1);

   /* start with no free space (we will add to this) */
   mh_totfree = 0;

#ifdef SEG16_16
   if (size > 0x0000FFF0)  /* segment limits force multiple blocks */
   {
      unsigned seg, offset;
      struct mem_block *   tmp;

      tmp = mh_free;
      seg = _FP_SEG(mh_free);
      offset = _FP_OFF(mh_free);
      while (size >= 0)
      {
         tmp->size = ((size>0xFFF0)?0xFFF0:(unsigned)size) - MEMBLOCKSIZE;
         mh_totfree += tmp->size;
         size -= 0xFFF0;
         seg += 0x0FFF;
#ifdef NPDEBUG
         tmp->pattern = MEM_PATTERN;    /* set pattern for testing overwrites */
#endif   /* NPDEBUG */
         tmp->next = _MK_FP(seg, offset);
         if (size >= 0) /* need more entrys? */
            tmp = tmp->next;  /* prepare for next loop */
         else
            tmp->next = NULL; /* terminate list */
      } 
   }
   else  /* make one big free block */
#endif   /* SEG16_16 */
   {
      mh_free->size = (unsigned)size - MEMBLOCKSIZE;
      mh_totfree += mh_free->size;
      mh_free->next = NULL;
#ifdef NPDEBUG
      mh_free->pattern = MEM_PATTERN;     /* set pattern for testing overwrites */
#endif   /* NPDEBUG */
   }

   /* set starting and minimum free space from the just-initialized total */
   mh_startfree = mh_minfree = mh_totfree;
}



/* FUNCTION: calloc1()
 *
 * Similar to standard calloc(), except it only takes 1 size arg.
 *
 * PARAM1: unsigned size
 *
 * RETURNS: pointer to memif OK, else NULL
 */

char *   
calloc1(unsigned size)
{
   unsigned lostsize;      /* size of data block plus struct */
   struct mem_block *   bp;
   struct mem_block *   newb;
   struct mem_block *   lastb;

#if (ALIGN_TYPE > 1)
   /* increase requested size enough to ensure future alignment */
   if ((long)size & (ALIGN_TYPE-1))
   {
      size = (size + ALIGN_TYPE) & ~(ALIGN_TYPE-1);
   }
#endif

   lostsize = size + MEMBLOCKSIZE;  /* size we will take from heap */

   ENTER_CRIT_SECTION(mh_free);
   bp = mh_free;                    /* init vars for list search */
   lastb = NULL;
   while (bp)
   {
#ifdef NPDEBUG
      if (bp->pattern != MEM_PATTERN)  /* test for corruption */
      {
         dtrap();
         EXIT_CRIT_SECTION(mh_free);
         return NULL;   /* probably should be panic */
      }
#endif   /* NPDEBUG */
      if (bp->size >= size)   /* take first-fit free block */
      {
         /* Decide if the block is big enough to be worth dividing */
         if (bp->size > (size + (MEMBLOCKSIZE * 2)))
         {
            /* Divide block and return front part to caller. First
             * make a new block after the portion we will return
             */
            newb = (struct mem_block *)((char*)(bp) + lostsize);
            newb->size = bp->size - lostsize;
            newb->next = bp->next;
#ifdef NPDEBUG
            newb->pattern = MEM_PATTERN;   /* set test pattern */
#endif   /* NPDEBUG */

            /* modify bp to reflect smaller size we will return */
            bp->next = newb;
            bp->size = size;
         }
         else  /* not worth fragmenting block, return whole thing */
         {
            lostsize = bp->size + MEMBLOCKSIZE;       /* adjust lostsize */
         }
         if (lastb)   /* unlink block from queue */
            lastb->next = bp->next;
         else
            mh_free = bp->next;

         /* keep statistics */
         mh_totfree -= lostsize;
         if (mh_totfree < mh_minfree)
            mh_minfree = mh_totfree;
         bp->next = mheap_base;     /* tag next ptr with illegal value */
         EXIT_CRIT_SECTION(mh_free);
         return((char*)(bp) + MEMBLOCKSIZE);
      }
      lastb = bp;
      bp = bp->next;
   }
   EXIT_CRIT_SECTION(mh_free);
   mh_failed++;   /* count out of memory conditions */
   return NULL;   /* failure return - no memory */
}


/* FUNCTION: mem_free()
 * 
 * Find block which contains buf and insert it in free 
 * list. Maintains list in order, low to high memory. 
 *
 * PARAM1: char HUGE * buf - buffer to add to free list.
 *
 * RETURNS: void
 */

void
mem_free(char HUGE * buf)
{
   struct mem_block  HUGE *   freep;
   struct mem_block  HUGE *   tmp;
   struct mem_block  HUGE *   last;
   int   merged   =  0; /* indicates freep not merged into free list */

   /* find pointer to prepended mem_block struct */
   freep = (struct mem_block*)(buf - MEMBLOCKSIZE);
   if (freep->next != mheap_base)      /* sanity check next ptr for tag */
      panic("mem_free");

   mh_totfree += ((unsigned long)freep->size + MEMBLOCKSIZE);

   last = NULL;
   for (tmp = mh_free; tmp; tmp = tmp->next)
   {
#ifdef NPDEBUG
      if (tmp->pattern != MEM_PATTERN) /* test for corruption */
      {
         dtrap();    /* memory is corrupt, tell programmer! */
         return;     /* probably should be panic */
      }
#endif   /* NPDEBUG */
      if (freep < tmp)  /* found slot to insert freep */
      {
         /* see if we can merge with next block */
         if (((char*)freep + freep->size + MEMBLOCKSIZE) == (char*)tmp)
         {
            freep->next = tmp->next;
            freep->size += (tmp->size + MEMBLOCKSIZE);
            if (last)
               last->next = freep;
            else
               mh_free = freep;
            merged++;
         }
#ifdef MEMIO_DEBUG
         /* this and the test below check for conditions where the end of a block
          * being freed is just a few bytes away from an adjacent block. This
          * usually means the block size of off by the size of the mgt header.
          * These tests chould be enabled and run for a while after any port of
          * or surgery to this file.
          */
         else
         if (((char HUGE *)freep + freep->size + MEMBLOCKSIZE) > (char HUGE *)(tmp - 3))
         {
            dprintf("memfree: this-end: %p, next: %p\n",
               ((char*)freep + freep->size + MEMBLOCKSIZE), tmp );
            dtrap();
         }
#endif
         /* ...and see if we can merge with previous block */
         if (last && (((char*)last + last->size + MEMBLOCKSIZE) == (char*)freep))
         {
            last->size += (freep->size + MEMBLOCKSIZE);
            if (merged) /* if already merged, preserve next ptr */
            {
               last->next = freep->next;
            }
            merged++;
         }
#ifdef MEMIO_DEBUG
         else
         if (last && (((char HUGE *)last + last->size + MEMBLOCKSIZE) > (char HUGE *)(freep-3)))
         {
            dprintf("memfree: last-end: %p, this: %p\n",
               ((char*)last + last->size + MEMBLOCKSIZE), freep );

            dtrap();
         }
#endif
            
         /* if didn't merge with either adjacent block, insert into list */
         if (!merged)   
         {
            if (last)
            {
               freep->next = last->next;
               last->next = freep;
            }
            else     /* no last, put at head of list */
            {
               freep->next = mh_free;
               mh_free = freep;
            }
            mh_totfree -= MEMBLOCKSIZE;   /* we didn't get a header back */
         }
         return;
      }
      last = tmp;       /* set "last" pointer for next loop */
   }
   /* got to end of list without finding slot for freep; add to end */
   if (last)
   {
      /* See if we can merge it with last block */
      if (((char*)last + last->size + MEMBLOCKSIZE) == (char*)freep)
      {
         last->size += (freep->size + MEMBLOCKSIZE);
      }
      else
      {
         freep->next = last->next;
         last->next = freep;
      }
   }
   else     /* there was no free list */
   {
      mh_free = freep;
      freep->next = NULL;
   }
}


#ifdef HEAP_STATS

/* FUNCTION: mh_stats()
 *
 * The console routine to printf the heap stats.
 *
 * PARAM1: void * pio - output device (NULL for console)
 *
 * RETURNS: 0
 */

int
mh_stats(void * pio)
{
   int   i  =  0;
   unsigned long freeheap = 0;
   struct mem_block *   tmp;

   ns_printf(pio, "free list:\n");
   for (tmp = mh_free; tmp; tmp = tmp->next)
   {
      freeheap += tmp->size;
      ns_printf(pio, "%d: at %p, 0x%x bytes\n", i, tmp, tmp->size);
      if(tmp->next == mheap_base)   /* was tmp alloced during ns_printf? */
      {
         ns_printf(pio, "list collision, terminating dump\n");  
         break;
      }
      i++;
   }

   ns_printf(pio, "heap; start: %lu,  min: %lu,  current: %lu;  actual: %lu ",
      mh_startfree, mh_minfree, mh_totfree, freeheap);
   ns_printf(pio, "alloc failed: %lu\n", mh_failed);

#ifdef MEM_WRAPPERS
   wrap_stats(pio);     /* list optional wrapper stats */
#endif

   return 0;
}

#else
/* if HEAP_STATS is not used, provide a dummy function */

int
mh_stats(void * pio)
{
   ns_printf(pio, "no heap stats on this build\n");
   return 0;
}

#endif   /* HEAP_STATS */


