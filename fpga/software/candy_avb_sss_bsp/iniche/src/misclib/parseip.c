/*
 * FILENAME: parseip.c
 *
 * Copyright 2000 By InterNiche Technologies Inc. All rights reserved
 *
 * parse_ipad() function and wrapper function inet_addr() for IP applications
 *
 * MODULE: MISCLIB
 *
 * ROUTINES: parse_ipad(), inet_addr(), print46_addr(), 
 * ROUTINES: inet46_addr(), inet_setport(), 
 * ROUTINES: hextoa(), inet_pton(), inet_ntop(), print_ip6(), 
 *
 * PORTABLE: yes
 */

/* Additional Copyrights: */
/* Portions Copyright 1993 NetPort Software, all rights reserved. */


#include "ipport.h"
#include "in_utils.h"
#include "q.h"
#include "netbuf.h"
#include "net.h"
#include "nvparms.h"
#include "ip.h"
#ifndef MINI_IP
#include "nptcp.h"
#include "socket.h"
#ifdef IP_V6
#include "socket6.h"
#endif
#endif /* !MINI_IP */

extern unsigned atoh(const char *);
#ifdef IP_V6
char *print_ip6(ip6_addr *, char *);
#endif


/* FUNCTION: parse_ipad()
 *
 * parse_ipad(long_out, string_in); Looks for an IP address in 
 * string_in buffer, makes an IP address (in big endian) in long_out. 
 * returns NULL if OK, else returns a pointer to a string describing 
 * problem. 
 *
 * 
 * PARAM1: ip_addr * ipout
 * PARAM2: unsigned * sbits
 * PARAM3: char * stringin
 *
 * RETURNS: 
 */

char *   
parse_ipad(ip_addr * ipout,   /* pointer to IP address to set */
   unsigned *  sbits,      /* default subnet bit number */
   char *   stringin)      /* buffer with ascii to parse */
{
   char *   cp;
   int   dots  =  0; /* periods imbedded in input string */
   int   number;
   union   
   {
      u_char   c[4];
      u_long   l;
   } retval;
   char *   toobig   = "each number must be less than 255";

   cp = stringin;
   while (*cp)
   {
      if (*cp > '9' || *cp < '.' || *cp == '/')
         return("all chars must be digits (0-9) or dots (.)");
      if (*cp == '.')dots++;
         cp++;
   }

   if ( dots < 1 || dots > 3 )
      return("string must contain 1 - 3 dots (.)");

   cp = stringin;
   if ((number = atoi(cp)) > 255)   /* set net number */
      return(toobig);

   retval.c[0] = (u_char)number;

   while (*cp != '.')cp++; /* find dot (end of number) */
      cp++;             /* point past dot */

   if (dots == 1 || dots == 2) retval.c[1] = 0;
      else
   {
      number = atoi(cp);
      while (*cp != '.')cp++; /* find dot (end of number) */
         cp++;             /* point past dot */
      if (number > 255) return(toobig);
         retval.c[1] = (u_char)number;
   }

   if (dots == 1) retval.c[2] = 0;
      else
   {
      number = atoi(cp);
      while (*cp != '.')cp++; /* find dot (end of number) */
         cp++;             /* point past dot */
      if (number > 255) return(toobig);
         retval.c[2] = (u_char)number;
   }

   if ((number = atoi(cp)) > 255)
      return(toobig);
   retval.c[3] = (u_char)number;

   if (retval.c[0] < 128) *sbits = 8;
      else if(retval.c[0] < 192) *sbits = 16;
      else *sbits = 24;

      *ipout = retval.l;      /* everything went OK, return number */
   return(NULL);        /* return OK code (no error string) */
}


/* FUNCTION: inet_addr()
 * 
 * PARAM1: char far *str
 *
 * RETURNS: u_long ipaddr
 */

u_long 
inet_addr(char FAR * str)
{
   u_long   ipaddr;
   unsigned bits ;    
   /* parse_ipad expects a "char and we are passing "char far Hence 
    * we need to make the conversion. Usually this function will be 
    * used for debugging, so I think we can bear the STRCPY overhead.
    */
   static char nearBuf[30];

   strcpy((char FAR *)nearBuf,str);
   if ( parse_ipad(&ipaddr,&bits,nearBuf) == NULL )
   {
      return ipaddr ;
   }
   else
   {
      return (u_long)NULL ;
   }
}

/* FUNCTION: hextoa()
 *
 * convert a single nibble to a hex char for print
 *
 * PARAM1: value to print in low 4 bits of unshort
 *
 * RETURNS: character 0-9 or A-F
 */

char
hextoa(int val)
{
   val &= 0x0f;
   if(val < 10)
      return (char)(val + '0');
   else
      return (char)(val + 55);   /* converts 10-15 -> "A-F" */
}


/* FUNCTION: inet_pton()
 *
 * inet_addr for v6 - parse the passed string into an IPv6 address
 *
 * This is part of the RFC-2133 IPv6 sockets spec. We've added the feature that
 * pton_error points to an explanatory string if we return a 1.
 * 
 * PARAM1: IN - type of address AF_INET for v4 or AF_INET6 for v6 
 * PARAM2: IN - string containing v4/v6 address
 * PARAM3: OUT - resulting v4/v6 address
 *
 * RETURNS: 0 if OK, 1 if bad addr format, -1 if bad af arg.
 */

char * pton_error = "";

int
inet_pton(int af, const char * src, void * dst)
{
  /* 
   * Altera modification:
   * Conditionally declare variables to remove build warning
   */
#if defined(IP_V6)
   const char *   cp;      /* char after previous colon */
   unshort *      dest;    /* word pointer to dst */
   int            colons;  /* number of colons in src */
   int            words;   /* count of words written to dest */
#endif

#if defined(IP_V4) || defined(MINI_IP)
   /* RFC 2133 wants us to support both types of address */
   if(af == AF_INET)    /* wants a v4 address */
   {
      u_long ip4addr;
      unsigned sbits;
      char * err;
      
      err = parse_ipad(&ip4addr, &sbits, (char *) src);
      if(err == NULL)
      {
         /* copy the parsed address into caller's buffer, and 
          * return success
          */
         MEMCPY(dst, &ip4addr, sizeof (u_long));
         return 0;
      }
      else
      {
         /* return failure */
         pton_error = "IPv4 address parse failure";
         return 1;
      }
   }
#endif /* IP_V4 */

#ifdef IP_V6

   if(af != AF_INET6)
   {
      pton_error = "bad domain";
      return -1;
   }

   /* count the number of colons in the address */
   cp = src;
   colons = 0;
   while(*cp)
      if(*cp++ == ':') colons++;

   if(colons < 2 || colons > 7)
   {
      pton_error = "must have 2-7 colons";
      return 1;
   }

   /* loop through address text, parseing 16-bit chunks */
   cp = src;
   dest = dst;
   words = 0;

   if(*cp == ':') /* leading colon has implied zero, e.g. "::1" */
   {
      *dest++ = 0;
      words++;
      cp++;       /* bump past leading colon, eg ":!" */
   }

   while(*cp > ' ')
   {
      if(words >= 8)
      {
	              /* !!!??? */
	              dprintf("***  inet_pton: logic error?\n");
         dtrap();    /* logic error? */
         pton_error = "internal";
         return 1;
      }
      if(*cp == ':')   /* found a double-colon? */
      {
         int i;

         for(i = (8 - colons); i > 0; i--)
         {
            *dest++ = 0;   /* add zeros to dest address */
            words++;       /* count total words */
         }
         cp++;             /* bump past double colon */
         if(*cp <= ' ')    /* last colon was last char? */
         {
            *dest++ = 0;   /* add one final zero */
            words++;       /* count total words */
         }
      }
      else
      {
         unshort wordval;

         wordval = htons(atoh(cp));    /* get next 16 bit word */
         if((wordval == 0) && (*cp != '0'))  /* check format */
         {
            pton_error = "must be hex numbers or colons";
            return 1;
         }
         *dest++ = wordval;
         words++;       /* count total words set in dest */
         cp = strchr((char *)cp, ':');   /* find next colon */
         if(cp)                  /* bump past colon */
            cp++;
         else                 /* no more colons? */
            break;            /* done with parsing */
      }
   }
   if(words != 8)
   {
      pton_error = "too short - missing colon?";
      return 1;
   }
   
#endif /* IP_V6 */   
   return 0;
}


/* FUNCTION: inet_ntop()
 *
 * Format an IP address for printing.
 *
 * PARAM1: int                AF_INET for v4, AF_INET6 for v6
 * PARAM2: void *             ptr to v4 or v6 address to format
 * PARAM3: char *             ptr to output buffer
 * PARAM4: size_t             size of output buffer (bytes)
 *
 * RETURNS: char *            ptr to string if OK, else NULL.
 *
 * This is part of the RFC-2133 IPv6 sockets spec.
 * The passed buffer should be big enough for
 * address output (40 bytes).
 */

const char *
inet_ntop(int af, const void *addr, char *str, size_t size)
{
   char *cp;

#if defined(IP_V4) || defined(MINI_IP)
   if (af == AF_INET)
   {
      u_long   ip4addr;

      ip4addr = *(u_long*)addr;
      cp = print_ipad(ip4addr);
      if (strlen(cp) < size)
      {
         strcpy(str, cp);
         return (str);
      }
   }
#endif   /* IP_V4 */

#ifdef IP_V6
   if (af == AF_INET6)
   {
      char ip6buf[48];

      cp = print_ip6((ip6_addr *)addr, ip6buf);
      if (strlen(cp) < size)
      {
         strcpy(str, ip6buf);
         return (str);
      }
   }
#endif

   return ((const char *)NULL);
}


#ifdef IP_V6

/* FUNCTION: print_ip6()
 *
 * Format an IPv6 address for printing. Passed buffer MUST be big
 * enough for address output (40 bytes).
 * 
 * PARAM1: IN - v6 address to format
 * PARAM2: OUT - string ready to print
 *
 * RETURNS: Pointer to string.
 */

char *
print_ip6(ip6_addr * addr, char * str)
{
   int         i;
   unshort *   up;
   char *      cp;
   unshort     word;
   int         skip = 0;   /* skipping zeros flag */

   if(addr == NULL)     /* trap NULL pointers */
      return NULL;

   up = (unshort*)addr;
   cp = str;

   for(i = 0; i < 8; i++)  /* parse 8 16-bit words */
   {
      word = htons(*up);
      up++;

      /* If word has a zero value, see if we can skip it */
      if(word == 0)
      {
         /* if we haven't already skipped a zero string... */
         if(skip < 2)
         {
            /* if we aren't already skipping one, start */
            if(!skip)
            {
               skip++;
               if (i == 0)
                  *cp++ = ':';
            }
            continue;
         }
      }
      else
      {
         if(skip == 1)  /* If we were skipping zeros... */
         {
            skip++;        /* ...stop now */
            *cp++ = ':';   /* make an extra colon */
         }
      }

      if(word & 0xF000)
         *cp++ = hextoa(word >> 12);
      if(word & 0xFF00)
         *cp++ = hextoa((word & 0x0F00) >> 8);
      if(word & 0xFFF0)
         *cp++ = hextoa((word & 0x00F0) >> 4);
      *cp++ = hextoa(word & 0x000F);
      *cp++ = ':';
   }
   if(skip == 1)  /* were we skipping training zeros? */
   {
      *cp++ = ':';
      *cp = 0;
   }
   else
      *--cp = 0;  /* turn trailing colon into null */
   return str;
}

#endif


#ifndef MINI_IP

/* FUNCTION: print46_addr()
 * 
 * Print the address into a string and return a pointer to it.
 *
 * PARAM1: struct v3_host *host
 *
 * RETURNS: Pointer to string with the address in readable format.
 */

char * print46_addr(struct sockaddr *ipaddr)
{
   if (ipaddr->sa_family == AF_INET)
   {
      struct sockaddr_in * addr = (struct sockaddr_in *)ipaddr;
      return print_ipad(addr->sin_addr.s_addr);
   }
#ifdef IP_V6
   else
   {
      struct sockaddr_in6 * addr = (struct sockaddr_in6 *)ipaddr;
      static char namebuf[46];  /* max len of IPv6 addr */
      return (char *)inet_ntop(AF_INET6,&addr->sin6_addr, namebuf, sizeof(namebuf));
   }
#endif

   return NULL;
}

/* FUNCTION: inet46_addr()
 * 
 * PARAM1: char far *str
 * PARAM2: struct sockaddr *addr
 *
 * inet_pton() is available only if IP_V6 is defined.
 * Also, it expects us to pass the family !!
 * So, it can't be used for more generic needs.
 *
 * inet46_addr() 
 * 1. Detects the address family 
 * 2. Appropriately parses the IPv4/IPv6 address.
 * 3. If IP_V6 is disabled, it doesn't parse IPv6 adddresses.
 *
 * RETURNS: Parsed IP address (IPv4/IPv6) in "address".
 * Returns 0 on success, else error.
 *
 */

int 
inet46_addr(char * str, struct sockaddr *address)
{
   /* Read the IPv4/IPv6 address */
   address->sa_family = AF_INET; /* assume IPv4 address by default */

   if ((str[1] == '.') || (str[2] == '.') || (str[3] == '.'))
   {
      struct sockaddr_in *addr = (struct sockaddr_in *)address;
      addr->sin_addr.s_addr = inet_addr(str);
      addr->sin_family = AF_INET;
   }
#ifdef IP_V6
   else
   {
      struct sockaddr_in6 *addr = (struct sockaddr_in6 *)address;
      inet_pton(AF_INET6, str, &addr->sin6_addr);
      addr->sin6_family = AF_INET6;
   }
#endif

   return 0;
}


/* FUNCTION: inet_setport()
 *
 * Utility function to set the destination port in the sockaddr structure.
 * Sets the port for both IPv4 and IPv6 addresses.
 *
 * PARAM1: struct sockaddr *addr
 * PARAM2: int port
 *
 * RETURNS: -
 */

void inet_setport(struct sockaddr *addr,int port) 
{
   if (addr->sa_family == AF_INET)
   {
      struct sockaddr_in *si = (struct sockaddr_in *)addr;
      si->sin_port = htons(port);  
   }
#ifdef IP_V6
   else
   {
      /* addr->sa_family = AF_INET6 */
      struct sockaddr_in6 *si = (struct sockaddr_in6 *)addr;
      si->sin6_port = htons(port);
   }
#endif

}

/*
 *Function Name:convert_ip()
 *
 *Parameters:
 *
 *Description:
 *
 *Returns:
 *
 */

unsigned long convert_ip(const char *p)
{
   const char *cp = p;
   unsigned long dw;
   unsigned char *lpb = (unsigned char *) &dw;
   int n = 0;
   unsigned short v = 0;
   dw = 0;
   while(*cp)
   {
      if( *cp == '.')
      {
         lpb[n] = (unsigned char) v;
         v = 0;
         n++;
         if(n > 3)
         {
            return dw;
         }
      }
      else if(((*cp >= '0') && (*cp <= '9')))
      {
         v = (v * 10) + (*cp - '0');
      }
      cp++;
   }
   lpb[n] = (unsigned char) v;
   return dw;
}  /* convert_ip() */

#endif /* !MINI_IP */
