/*
 * FILENAME: strilib.c
 *
 * Copyright 1998-2003 By InterNiche Technologies Inc. All rights reserved
 *
 * The file contains source for Common but not quite 
 * universal C library string functions. These are used by the http 
 * server, emailer, and perhaps others. This file has implementation 
 * for three functions, each of which can be enabled at compile time 
 * with a switch. INICHE_STRICMP enables stricmp() INICHE_STRNICMP 
 * enables strnicmp() INICHE_STRISTR enables stristr() 
 * Created for minimal C lib support.
 *
 * MODULE: MISCLIB
 *
 * ROUTINES: stricmp(), strnicmp(), stristr(),
 *
 * PORTABLE: yes
 */

/* Additional Copyrights: */
/* Portions Copyright 1993 by NetPort software John Bartas 
 */

#include "in_utils.h"

/* our version of toupper() - convert lower case to upper case */
#define INICHE_TOUPPER(c) (((c >= 'a') && (c <= 'z')) ? (c & ~0x20) : c)


#ifdef INICHE_STRICMP   /* bring in stricmp() */

int strnicmp(const char * s1, const char * s2, int len); /* forward decl. */


/* FUNCTION: stricmp()
 *
 * stricmp() - string compare which ignores case. Returns 0 if 
 * strings match. 
 *
 * 
 * PARAM1: char * s1
 * PARAM2: char * s2
 *
 * RETURNS: 
 */

int
stricmp(const char * s1, const char * s2)
{
   int   l1,   l2;

   l1 = strlen(s1);  /* get both lengths */
   l2 = strlen(s2);

   /* call strnicmp with maximum length */
   return( strnicmp(s1, s2, ( l1 > l2 )?l1:l2 ));
}

#endif   /* INICHE_STRICMP */

#ifdef INICHE_STRNICMP  /* bring in strnicmp() */

/* FUNCTION: strnicmp()
 *
 * strnicmp() - string compare which ignores case but limits compare 
 * size. Returns 0 if strings match. 
 *
 * 
 * PARAM1: char * s1
 * PARAM2: char * s2
 * PARAM3: int len
 *
 * RETURNS: 
 */

int
strnicmp(const char * s1, const char * s2, int len)
{
   char  c1,   c2;

   while (len--)
   {
      c1 = (char)(*s1++);
      c1 = INICHE_TOUPPER(c1);
      c2 = (char)(*s2++);
      c2 = INICHE_TOUPPER(c2);
      if (c1 == c2)
      {
         if (c1 == 0)   /* end of both strings */
            return 0;   /* they match */
         continue;   /* check next char */
      }
      else if(c1 < c2)
         return -1;
      else  /* c1 > c2 */
         return 1;
   }
   return 0;
}

#endif   /* INICHE_STRNICMP */

#ifdef INICHE_STRISTR   /* bring in stricmp() */



/* FUNCTION: stristr()
 *
 * stristr(str1, str2) - look for str2 (case insensitive) inside str1
 * 
 * PARAM1: char * str1
 * PARAM2: char * str2
 *
 * RETURNS: 
 */

char * 
stristr(char * str1, char * str2)
{
   char  c1,   c2;
   c1 = (char)(*str2); /* 1st char of sub-string in upper case */
   c1 = INICHE_TOUPPER(c1);

   while (*str1)
   {
      c2 = (char)(*str1); /* c1=toupper(*str2) */
      c2 = INICHE_TOUPPER(c2);
      if (c1 == c2)  /* found first char of str2 in str1 */
      {
         if (strnicmp(str1, str2, strlen(str2)) == 0)
         {
            return str1;   /* found str2 at str1 */
         }
      }
      str1++;
   }
   return (char*)0;
}

#endif   /* INICHE_STRISTR */

