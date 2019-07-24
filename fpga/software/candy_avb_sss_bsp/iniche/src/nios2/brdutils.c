/*
 * FILENAME: brdutils.c
 *
 * Copyright 2004 InterNiche Technologies Inc. All rights reserved.
 *
 * Target Board utility functions for InterNiche TCP/IP.
 * This one for the ALTERA Cyclone board with the ALTERA Nios2 Core running 
 * with the uCOS-II RTOS.
 *
 * This file for:
 *   ALTERA Cyclone Dev board with the ALTERA Nios2 Core.
 *   SMSC91C11 10/100 Ethernet Controller
 *   GNU C Compiler provided by ALTERA Quartus Toolset.
 *   Quartus HAL BSP
 *   uCOS-II RTOS Rel 2.76 as distributed by Altera/NIOS2
 *
 * MODULE  : NIOS2GCC
 * ROUTINES: dtrap(), clock_init(), clock_c(), kbhit() getch().
 * PORTABLE: no
 *
 * 06/21/2004
 * 
 */

#include "ipport.h"

#include "fcntl.h"
#include "unistd.h"
#include "sys/alt_irq.h"

#ifdef UCOS_II
#include "includes.h"  /* Standard includes for uC/OS-II */
#endif

void dtrap(void);
int  kbhit(void);
int  getch(void);
void clock_c(void);
void clock_init(void);
void cticks_hook(void);
#ifdef USE_LCD
extern void update_display(void);
#endif

int kb_last = EOF;

void irq_Mask(void);
void irq_Unmask(void);

/* dtrap() - function to trap to debugger */
void
dtrap(void)
{
   printf("dtrap - needs breakpoint\n");
}

int
kbhit()
{
   static int kbd_init = 0;
   int   kb;
   
   if (!kbd_init)
   {
      /* we really should read the flags, OR in O_NONBLOCK, and write
       * the flags back to STDIN, but the NIOS-II/HAL implementation
       * will only let us modify O_NONBLOCK and O_APPEND, so we'll
       * just write the new flag value.
       */
      if (fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK) != 0)
      {
         printf("F_SETFL failed.\n");
         dtrap();
      }
      kbd_init = 1; 
   }

   /* we have to do a read to see if there is a character available.
    * we save the character, if there was one, to be read later. */
   if (kb_last == EOF)
   {
      kb = getchar();
      if (kb < 0)       /* any error means no character present */
         return (FALSE);
         
      /* there was a character, and we read it. */
      kb_last = kb;
   }

   return (TRUE);
}

int 
getch()
{
int chr;

   if(kb_last != EOF)
   {
      chr = kb_last;
      kb_last = EOF;
   }
   else
      chr = getchar();

   return chr;
}

/*
 * Altera Niche Stack Nios port modification:
 * Add clock_init and clock_c as empty routines to support superloop
 */
#ifdef SUPERLOOP
void clock_init(void)
{
   /* null */ ;
}

void clock_c(void)
{
   /* null */ ;
}

#else /* !SUPERLOOP */

/*
 * system clock : NO OP - start clock tick counting;
 * called at init time.
 * 
 */
int OS_TPS;
int cticks_factor;
int cticks_initialized = 0;

void clock_init(void)
{
   OS_TPS = OS_TICKS_PER_SEC;
   cticks_factor = 0;
   cticks = 0;
   cticks_initialized = 1;
}

/* undo effects of clock_init (i.e. restore ISR vector) 
 * NO OP since using RTOS's timer.
 */
void clock_c(void)
{
   /* null */ ;
}


/* This function is called from the uCOS-II OSTimeTickHook() routine.
 * Use the uCOS-II/Altera HAL BSP's timer and scale cticks as per TPS.
 */

void
cticks_hook(void)
{
   if (cticks_initialized) 
   {
      cticks_factor += TPS;
      if (cticks_factor >= OS_TPS)
      {
         cticks++;
         cticks_factor -= OS_TPS;
#ifdef USE_LCD
         update_display();
#endif
      }
   }
}

#endif /* SUPERLOOP */

/* Level of Nesting in irq_Mask() and irq_Unmask() */
int irq_level = 0;

/* Latch on to Altera's NIOS2 BSP */
static alt_irq_context cpu_statusreg;

#ifdef NOT_DEF
#define IRQ_PRINTF   1
#endif

/* Disable Interrupts */

/*
 * Altera Niche Stack Nios port modification:
 * The old implementation of the irq_Mask and irq_unMask functions
 * was incorrect because it didn't implement nesting of the interrupts!
 * The InterNiche handbook specification implies that nesting is
 * something that is supported, and the code seems to need it as 
 * well.
 *
 * From Section 2.2.3.1 on the NicheStack Handbook:
 * "Note that it is not sufficient to simply disable interrupts in 
 * ENTER_CRIT_SECTION() and enable them in EXIT_CRIT_SECTION()
 * because calls to ENTER_CRIT_SECTION() can be nested."
 */
void
irq_Mask(void)
{
   alt_irq_context  local_cpu_statusreg;

   local_cpu_statusreg = alt_irq_disable_all();
	
   if (++irq_level == 1)
   {
      cpu_statusreg = local_cpu_statusreg;
   }
}


/* Re-Enable Interrupts */
void
irq_Unmask(void)
{
   if (--irq_level == 0)
   {
      alt_irq_enable_all(cpu_statusreg);
   }
}


#ifdef USE_PROFILER

/* FUNCTION: get_ptick
 * 
 * Read the hig-resolution timer
 * 
 * PARAMS: none
 * 
 * RETURN: current high-res timer value
 */
u_long
get_ptick(void)
{
   return (alt_nticks());
}

#endif  /* USE_PROFILER */
