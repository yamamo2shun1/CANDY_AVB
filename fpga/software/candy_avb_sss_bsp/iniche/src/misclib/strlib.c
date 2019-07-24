/*
 * FILENAME: strlib.c
 *
 * Copyright 1998- 2000 By InterNiche Technologies Inc. All rights reserved
 *
 * Universal string  functions, provided for systems with no C library. 
 * Created for minimal C lib support. Portions of this are still only 
 * lightly tested. Caveat Emptor. -JB- 
 *
 * MODULE: MISCLIB
 *
 * ROUTINES: strlen(), strcpy(), strncpy(), strcmp(), strncmp(), 
 * ROUTINES: strcat(), strchr(), strstr(),  *
 * PORTABLE: yes
 */

/* Additional Copyrights: */

/* Portions Copyright 1993 by NetPort software. */

#include "ipport.h"
#include "in_utils.h"


#ifdef INICHE_LIBS

/* FUNCTION: strlen()
 * 
 * PARAM1: char *             source string 
 *
 * RETURNS: int               string length (excluding NUL)
 */
int
strlen(char *str)
{
   int  len = 0;

   while (*str++)
      len++;
   return (len);
}



/* FUNCTION: strcpy()
 * 
 * Copy the source string into the destination string
 *
 * PARAM1: char *             destination string
 * PARAM2: char *             source string
 *
 * RETURNS: char *            destination string
 */
char *   
strcpy(char *dest, char *src)
{
   char *ret = dest;

   while ((*dest++ = *src++) != '\0')
      /* null */ ;

   return (ret);
}



/* FUNCTION: strncpy()
 * 
 * Copy a maximum of 'maxcopy' characters from the source string to 
 * the destination string
 *
 * PARAM1: char *             destination string
 * PARAM2: char *             source string
 * PARAM3: int maxcopy        maximum number of characters to copy
 *
 * RETURNS: char *            destination string
 *
 * Characters are copied from the source string to the destination string,
 * up to a maximum of 'maxcopy' characters. If the length of the source
 * string is less than 'maxcopy', the destination is padded with NULs.
 * If the length of the source string is greater than or equal to 'maxcopy',
 * the destination string is not terminated with a NUL. 
 */
char *   
strncpy(char * dest, char * src, int maxcopy)
{
   char *ret = dest;

   while (--maxcopy >= 0)
   {   
      if ((*dest++ = *src) != '\0')
         ++src;
   }

   return (ret);
}



/* FUNCTION: strcmp()
 * 
 * Compare two strings
 *
 * PARAM1: char *             string 1
 * PARAM2: char *             string 2
 *
 * RETURNS: int                 0 if the strings are equal
 *                            < 0 if string1 is less than string2
 *                            > 0 if string1 is greater than string2
 *
 * String1 is less than string2 if string1 is shorter than string2.
 */
int
strcmp(char * str1, char * str2)
{
   while (*str1 == *str2)
   {
      ++str1;
      ++str2;
   }

   return (*str1 - *str2);
}



/* FUNCTION: strncmp()
 * 
 * Compare the first N characters of two strings
 *
 * PARAM1: char *             string1
 * PARAM2: char *             string2
 * PARAM3: int                N, maximum number of characters to compare
 *
 * RETURNS: int                 0 if the strings are equal
 *                            < 0 if string1 < string2
 *                            > 0 if string1 > string2
 *
 * Similar to strcmp() except that the comparison is performed only
 * on the first N characters of each string.
 */
int 
strncmp(char *str1, char *str2, int length)
{
   if (length <= 0)
      return 0;

   while ((--length > 0) && (*str1 == *str2))
   {
      ++str1;
      ++str2;
   }

   return (*str1 - *str2);
}



/* FUNCTION: strcat()
 *
 * Append string2 to string1
 * 
 * PARAM1: char *             string1
 * PARAM2: char *             string2
 *
 * RETURNS: char *            result string (string1)
 */
char *   
strcat(char *str1, char *str2)
{
   char *cp = str1;

   while (*cp)    /* find null at the end of str1 */
      cp++;

   do
   {
      *cp++ = *str2;
   } while (*str2++);

   return (str1);
}



/* FUNCTION: strchr()
 * 
 * Find the first occurance of a character in a string
 *
 * PARAM1: char *             string
 * PARAM2: char               character
 *
 * RETURNS: char *            position of character in string or NULL
 */
char * 
strchr(char *str, char chr)
{
   do
   {
      if (*str == chr)
         return (str);
   } while (*str++ != '\0');

   return ((char *)NULL);     /* character not found */
}



/* FUNCTION: strstr()
 *
 * Find the first occurance of string2 within string1
 *
 * PARAM1: char *             string1
 * PARAM2: char *             string2
 *
 * RETURNS: char *            position of string2 within string1 or NULL
 */
char * 
strstr(char * str1, char * str2)
{
   int len1 = strlen(str1);
   int len2 = strlen(str2);

   while (len1 >= len2)
   {
      if (*str1 == *str2)
      {
         if (strncmp(str1, str2, len2) == 0)
            return (str1);
      }
      ++str1;
      --len1;
   }

   return ((char *)NULL);
}

#endif   /* INICHE_LIBS */

