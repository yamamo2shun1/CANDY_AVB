/*
 * FILENAME: dhcputil.c
 *
 * Copyright 1997- 2000 By InterNiche Technologies Inc. All rights reserved
 *
 * DHCP utilities which apply to both client and server
 *
 * MODULE: INET
 *
 * ROUTINES: find_opt(),  *
 * PORTABLE: yes
 */

/* Additional Copyrights: */
/* Portions Copyright 1996 by NetPort Software. */


#include "ipport.h"


#ifdef DHCP_CLIENT   /* find_opt() is used only by DHCP client */

#include "dhcpclnt.h"


/* FUNCTION: find_opt()
 *
 * find_opt() - Search an options string for a particular option 
 * code. 
 *
 * 
 * PARAM1: u_char opcode
 * PARAM2: u_char * opts
 *
 * RETURNS:  Return pointer to that code if found, NULL if not found.
 */

u_char * 
find_opt(u_char opcode, u_char * opts)
{
   u_char * end   =  opts  +  DHCP_OPTSIZE;  /* limit scope of search */

   while (opts < end)
   {
      if (*opts == opcode) /* found it */
         return opts;
      if (*opts == DHOP_END)  /* end of options; opcode not found */
         return NULL;
      if (*opts == DHOP_PAD)  /* PAD has only 1 byte */
         opts++;
      else     /* all other options should have a length field */
         opts += (*(opts+1))+2;
   }
   /* no DHOP_END option?? */
   return NULL;
}

#endif   /* DHCP_CLIENT */

