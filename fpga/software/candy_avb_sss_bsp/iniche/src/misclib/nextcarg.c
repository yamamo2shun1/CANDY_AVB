/*
 * FILENAME: nextcarg.c
 *
 * Copyright 1997- 2000 By InterNiche Technologies Inc. All rights reserved
 *
 *
 * MODULE: MISCLIB
 *
 * ROUTINES: nextcarg(),
 *
 * PORTABLE: yes
 */

/* Additional Copyrights: */
/* Portions Copyright 1996 by NetPort Software */

#include "ipport.h"
#include "in_utils.h"



/* FUNCTION: nextcarg()
 *
 * nextcarg() - return next arg from comma delimited arg list passed. 
 * Current (passed) arg is NULL terminated. If the arg points to a 
 * string enclosed in quotes, the following happens. 1. say 
 * arg="string in quotes", other_arg 2. Move the string so that first 
 * quote is overwritten. arg=string in quotes", other_arg 3. Put a 
 * NULL instead of the last quote. That is , arg=string in quotes 4. 
 * nextarg=other_arg The above logic will allow the caller to use 
 * nextcarg() seamlessly, irrespective of whether the input string is 
 * enclosed in quotes or not. Returns pointer to next arg, or NULL if 
 * no more args. 
 *
 * 
 * PARAM1: char * arg
 *
 * RETURNS: 
 */

char *   
nextcarg(char * arg)
{
   char *   cp;

   cp = strchr(arg, ',');

   /* NULL terminate passed arg weather cp is set or not: */
   if ( *arg == '\"' )
   {
      unsigned len = cp-arg-1 ;
      MEMMOVE(arg,arg+1,len);  /* Move the string so that
                                  the first quote is overwritten */
      while (*arg != '\"' )
         arg++;
   }
   else
   {
      while (*arg != ',' && *arg > ' ')
         arg++;
   }
   *arg = 0;

   if (!cp) /* no next arg; return NULL */
      return NULL;

   cp++;       /* move cp past comma */
   while (*cp <= ' ') cp++;
      return cp;
}




