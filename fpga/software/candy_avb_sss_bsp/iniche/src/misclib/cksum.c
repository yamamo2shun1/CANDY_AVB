/* FILE: cksum.c
 *
 * Internet checksum function
 *
 */
 
#include "ipport.h"

#ifdef ALTERA_NIOS
#include "sys/alt_cache.h"
#endif

#ifndef C_CHECKSUM
unsigned short asm_cksum (void *, unsigned);
#endif
#ifdef ALT_CKSUM
unsigned short alt_cksum (void *, unsigned);
#endif
  
/* Figure out how many checksum choices we have. There is always
 * a C version. If 'C_CHECKSUM' is not defined, then we have an
 * assembly version, and there is a third choice specifically for
 * Altera NIOS-II systems.
 */
#ifdef ALT_CKSUM
#define MAX_CKSUM_SELECT  3
#else
#ifndef C_CHECKSUM
#define MAX_CKSUM_SELECT  2
#else
#define MAX_CKSUM_SELECT  1
#endif
#endif

/* checksum algorithm selection 
 * 1 = C code
 * 2 = assembly language
 * 3 = alternate checksum accelerator
 */
static int cksum_select = MAX_CKSUM_SELECT;


/* FUNCTION: ccksum.c
 *
 * C language checksum example from RFC 1071
 *
 * PARAM1: ptr          pointer to data to be checksum-ed
 * PARAM2: count        number of 16-bit words to process
 *
 * RETURN: ccksum       16-bit checksum value
 *
 * The InterNiche Stack guarentees that data is 16-bit aligned
 * and odd byte lengths are padded with zero, so processing can
 * be done in 16-bit chunks.
 */

unsigned short
ccksum (void *ptr, unsigned words)
{
   unsigned short *addr = (unsigned short *)ptr;
   unsigned long sum = 0;
   int count = (int)words;

   while (--count >= 0)
   {
      /*  This is the inner loop */
      sum += *addr++;
   }

   /*  Fold 32-bit sum to 16 bits */
   sum = (sum & 0xffff) + (sum >> 16);
   sum = (sum & 0xffff) + (sum >> 16);

   /* checksum = ~sum; *//* removed for MIT IP stack */
   return ((unsigned short)sum);
}



/* FUNCTION: cksum
 *
 * Compute checksum using the function selected by "cksum_select"
 *
 * PARAM1: addr         pointer to data to be checksum-ed
 * PARAM2: count        number of 16-bit words to process
 *
 * RETURN: ccksum       16-bit checksum value
 *
 * "cksum_select" selects which function is used to compute the checksum: 
 *    1 = C implementation
 *    2 = assembly language implementation
 *    3 = user-supplied alternate implementation
 */

unsigned short
cksum (void *ptr, unsigned count)
{
   switch (cksum_select)
   {
      case 1:
      default:
         return (ccksum(ptr, count));
 #ifndef C_CHECKSUM
      case 2:
         return (asm_cksum(ptr, count));
#endif
#ifdef ALT_CKSUM
      case 3:
#ifdef ALTERA_NIOS2
         alt_dcache_flush_all();
         return (alt_cksum((void *)((unsigned long)ptr & 0x7FFFFFFF), count));
#else
#endif
         return (alt_cksum(ptr, count));
#endif
   }
}



#ifdef IN_MENUS

#include "menu.h"
int do_cksum (void *);

/*
 * Altera Niche Stack Nios port modification:
 * add braces to remove build warning
 */
struct menu_op cksummenu[]  =  {  /* array of menu option, see menu.h */
   { "chksum",   stooges,       "checksum menu" },    /* menu ID */
   { "cksum",    do_cksum,      "set/get checksum selection" },
   { NULL,       NULL,          NULL }
};


/* FUNCTION: cksum_init()
 * 
 * Install checksum test menu
 * 
 * PARAMS: none
 * 
 * RETURN: 0 if successful, otherwise non-zero error code
 */

int
cksum_init(void)
{
   install_menu(cksummenu);
   return (0); 
}



/* FUNCTION: cksum_alg
 * 
 * Selects which checksum algorithm to use
 * 
 * PARAM1: int alg      algorithm choice
 *                        1 = C checksum
 *                        2 = assembly language checksum (optional)
 *                        3 = Altera CI checksum (optional)
 * 
 * RETURN: none
 */

void
cksum_alg(int alg)
{
  if ((1 <= alg) && (alg <= MAX_CKSUM_SELECT))
  {
#if 1
      cksum_select = alg;
#else
      cksum_select = 1;
#endif
  }
}



/* FUNCTION: which_cksum
 * 
 * Get the current checksum algorithm selection
 * 
 * PARAMS: none
 * 
 * RETURN: int            current value of cksum_select
 */

int
which_cksum(void)
{
   return (cksum_select);
}



/* FUNCTION: do_cksum()
 * 
 * PARAM1: void *             console output descriptor
 *
 * RETURNS: int               0 if successful, else error code
 */

int
do_cksum (void * pio)
{
   char *cp = nextarg(((GEN_IO)pio)->inbuf);
   int value;

   if (!*cp)   /* no arg given */
   {
      ns_printf(pio, "current checksum selection is %d\n", cksum_select);
      ns_printf(pio, "valid selection range is: 1..%d\n", MAX_CKSUM_SELECT);
      return (-1);
   }
   value = atol(cp);
   if ((1 <= value) && (value <= MAX_CKSUM_SELECT))
   {
      cksum_alg(value);
   }
   else
   {
      ns_printf(pio, "invalid csum parameter value\n");
      ns_printf(pio, "valid selection range is: 1..%d\n", MAX_CKSUM_SELECT);
      return (-1);
   }

   return (0);
}

#endif  /* IN_MENUS */
