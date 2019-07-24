/*
 * FILENAME: menus.c
 *
 * Copyright  1993-2007 By InterNiche Technologies Inc. All rights reserved
 *
 *
 * MODULE: MISCLIB
 *
 * ROUTINES: do_command(), stooges(), showmenu(), help(), 
 * ROUTINES: kbdio(), stcmp(), stlen(), install_menu()
 * ROUTINES: install_version()
 *
 * PORTABLE: yes
 */

/* Additional Copyrights: */
/* Portions Copyright 1990-1992 by NetPort Software */


#include "ipport.h"
#ifdef MQX
#include <fio.h>
#endif

#ifdef IN_MENUS

#include "menu.h"

extern   int  nummenus;       /* items in menus[] */
extern   char *prompt;

/* the following two routines are DOS-isms menuing requires from the port */
extern   int kbhit(void);     /* TRUE if console char (keyboard) is ready */
extern   int getch(void);     /* get next console char */



/* FUNCTION: do_command()
 *
 * do_command() - this is called when we have a complete command 
 * string in cbuf[]. It looks up the command in the menus and 
 * executes it. Prints error messages as appropriate. 
 *
 * 
 * PARAM1: void * pio   - device for ns_printf() output
 *
 * RETURNS: Returns 0 if the command passed was found in menus
 *  (or otherwise understood), returns -1 if not. 
 */

int
do_command(void * pio)
{
   struct menu_op *mp;
   struct menu_op *mp1   =  NULL;
   char *inbuf = ((GEN_IO)pio)->inbuf;
   int   i;
   int   l;

   mp1 = NULL;                /* clear menu command pointer */
   if (inbuf[0] == '?')    /* is user after help? */
   {
      help(pio);
      ns_printf(pio,"\n");
      return 0;
   }

#ifdef INCLUDE_SHELL
   if (inbuf[0] == '!') /* User want an OS shell command? */
   {
      system(&inbuf[1]);   /* call system */
      return 0;
   }
#endif   /* INCLUDE_SHELL */

   /* scan the various menus for the user command: */
   for (i = 0; i < nummenus; i++)   /* menu loop */
   {
      mp = menus[i];    /* get pointer to next menu list */
      if (!mp) break;
         mp++;          /* bump past menu ID */
      while (mp->opt)   /* scan this menu's options for cmd */
      {      
         l = stcmp(mp->opt, inbuf);
         if (l == 0 || l > stlen(inbuf))  /* got a match */
         {
            if(stlen(inbuf) == (int)strlen(mp->opt))    /* was it an exact match? */
            {
               mp1 = mp;
               goto menu_do_cmd;    /* quit searching and go do it */
            }

            if (mp1) /* we already had a partial match */
            {
               ns_printf(pio,"Ambiguous command: %s \n", inbuf);
               return -1;
            }
            mp1 = mp;      /* remember the matching entry */
         }
         mp++;
      }
   }
   if (!mp1)
   {
      ns_printf(pio,"Unknown command: %s\n", inbuf);
      return -1;
   }

menu_do_cmd:
   if ( mp1->func )  /* Has the function been specified ? */
      (mp1->func)(pio); /* call the menu command's function */

   return 0;   /* we found the command */
}



/* FUNCTION: stooges()
 *
 * Called when an unused menu routine hook is exececuted. This means
 * a serious programming error occured.
 *
 * PARAM1: void * pio   - device for ns_printf() output
 *
 * RETURNS: 0
 */

int
stooges(void * pio)
{
   return 0;
}



/* FUNCTION: showmenu()
 *
 * Loop through the passed menu struct, displaying the options
 * and descriptions.
 *
 * PARAM1: void * pio   - device for ns_printf() output
 * PARAM2: struct menu_op * menu
 *
 * RETURNS: void
 */

void
showmenu(void * pio, struct menu_op * menu)     /* menu to display */   
{
   ns_printf(pio,"SNMP Station: %s commands:\n", menu->opt);   /* 1st item is menu ID */
   menu++;
   while (menu->opt)
   {
      ns_printf(pio,"   %-8s - %s\n", menu->opt, menu->desc);
      menu++;
   }
}


/* FUNCTION: help()
 *
 * help() - printf the commands and descriptions from the app's 
 * menus. Returns no meaningfull value, declared an int to suppress 
 * compiler warnings when assigned to menus. 
 *
 * 
 * PARAM1: void * pio   - device for ns_printf() output
 *
 * RETURNS: 0
 */

int
help(void * pio)
{
   char *   argp;
   int   i,l;

   /* help command line should still be in inbuf */
   argp = nextarg(((GEN_IO)pio)->inbuf);     /* get first arg after "help" or '?' */

   /* handle simple '?' or 'help' cases first */
   if (!*argp)
   {
      showmenu(pio,menus[0]);
      ns_printf(pio,"Also try 'help [");
      for (i = 0; i < nummenus; i++)   /* menu loop */
      {
         if (!menus[i]) break;
            if (i != 0) ns_printf(pio,"|");  /* separate IDs w/vertical bars */
            ns_printf(pio,"%s", menus[i]->opt); /* print each menu ID */
      }
      ns_printf(pio,"]'\n");
      return 0;
   }
   /* else - should be a parm after help */
   for (i = 0; i < nummenus; i++)   /* menu loop */
   {
      if (!menus[i]) break;
         l = stcmp(argp, menus[i]->opt);
      if (l == 0 || l > stlen(argp))      /* found a menu matching parm */
      {
         showmenu(pio,menus[i]);
         return 0;
      }
   }
   ns_printf(pio,"unknown help parameter %s\n", argp);
   return 0;
}


/* FUNCTION: kbdio()
 *
 * Called by the system whenever a console character is ready. Calls
 * kbhit() to test, so it can be used for polling. If an enter ASCII
 * (13 or 10) is received, it calls do_command() with the currently
 * buffered input characters. 
 *
 * This does not block.
 * 
 * PARAM1: none
 *
 * RETURNS: void
 */

 
/* command line buffer. Directly referenced only by kbdio() */
char     cbuf[CBUFLEN];     

/* re-entry prevention flag for kbdio */
static int in_docmd = 0;

/* pointer to next available byte in cbuf */
char *   nextcb   =  &cbuf[0];

/* Generic IO structure that do_command() will be called with */
struct GenericIO  std_io   =  {  cbuf, std_out, 0, std_in   }  ;

void
kbdio(void)
{
   int   key;
#ifdef MQX
   _mqx_uint avl;
   _mqx_int rc;
#endif

   if(in_docmd)   /* don't poll while doing command */
      return;

#ifdef MQX
   rc = ioctl (stdin, IO_IOCTL_CHAR_AVAIL, &avl);
   if (!avl) return;
   key = fgetc (stdin);  
#else
   if(!kbhit())   /* if no key waiting, just return */
      return;
   key = getch();        /* get keystroke */
#endif

   if(key == 13 || key == 10)
   {
      if(nextcb == cbuf)   /* no data in cbuf */
      {
#ifndef KBDIO_NO_ECHO      
         putchar('\n');
#endif
         printf("%s", prompt);         
         FLUSH_STDOUT;
         return;
      }

      /* else there's data in cbuf */
      *nextcb   =  0;    /* null terminate string */
#ifndef KBDIO_NO_ECHO      
      putchar('\n');
#endif      
      in_docmd++; /* set anti-polling flag while doing command */
      do_command(&std_io); /* lookup and execute command */
      nextcb   =  cbuf;
      in_docmd--;
      printf(prompt);
      FLUSH_STDOUT;
   }

   if(key  >= ' ' && key < 127)  /* got printable char */
   {
      if(nextcb >= &cbuf[CBUFLEN-1])
         return;  /* buffer full */
      *nextcb++ = (char)key;
#ifndef KBDIO_NO_ECHO      
      putchar(key);
#endif      
      FLUSH_STDOUT;
   }
   else if(key == 8)    /* backspace */
   {
      if(nextcb == cbuf)
         return;     /* don't backspace if no chars in buf */
      *nextcb   =  0; /* clear last char from buffer */
      nextcb--;
#ifndef KBDIO_NO_ECHO      
      putchar(8);
      putchar(' ');
      putchar(8);
#endif      
      FLUSH_STDOUT;
   }
   else if(key == 27)   /* escape */
   {
      while(nextcb > cbuf) /* reset buffer */
      {
         nextcb--;
         putchar(8);
         putchar(' ');
         putchar(8);
      }
      cbuf[0]  =  27;   /* leave ESC in first byte */
      cbuf[1]  =  0;    /* clean up string */
      FLUSH_STDOUT;
      /* next char will overwrite ESC */
   }
   else if(key == 3) /* ctrl-C */
   {
      /* should we quit() here? */
   }
   return;
}



/* FUNCTION: stcmp()
 *
 * stcmp() - special string compare routine. returns 0 if strings 
 * match, else returns char where 1st mismatch occured. ie: 
 * stcmp("help", "helper") returns a 5. stcmp("help", "nohelp") 
 * returns a 1. Also: treats space as end of string. 
 *
 * 
 * PARAM1: char * string1
 * PARAM2: char * string2
 *
 * RETURNS: 0 if strings match, else returns char where 1st mismatch occured.
 */

int
stcmp(char * s1, char * s2)
{
   int   ct =  1;    /* byte being checked */

   while(*s1 && *s1 != ' ')  /* read to 1st space or end */
   {
      if(*s1++ != *s2++) return(ct);
         ct++;
   }
   if(*s2) return(ct);  /* s2 was longer than s1 */
      return(0);
}


/* FUNCTION: stlen()
 *
 * stlen() - return length of string passed. space char counts as end 
 * of string. 
 *
 * 
 * PARAM1: char * string
 *
 * RETURNS: return length of string passed
 */

int
stlen(char * st1)
{
   int   retval   =  0;

   while(*st1 && *st1 != ' ')   /* find first NULL or space */
   {
      st1++;
      retval++;
   }
   return(retval);
}


/* FUNCTION: install_menu()
 *
 * Add new menu to master list. 
 * 
 * PARAM1: menu_op *          pointer to new menu struct
 *
 * RETURNS: int               0 if OK, -1 if no more slots 
 */

int
install_menu(struct menu_op * newmenu)
{
   int   i;

   for (i = 0; i < MAX_MENUS; i++)
   {
      if (menus[i] == newmenu)      /* duplicate */
         return (0);

      if (menus[i] == (struct menu_op *)NULL)
      {
         menus[i] =  newmenu;
         return (0);          /* installed OK */
      }         
   }

   dtrap();                   /* bad programming? */
   return (-1);               /* no room in master list */
}



/* FUNCTION: install_version()
 *
 * register a module version string
 *
 * PARAM1: char *             version string
 *
 * RETURN: int                0 if successful, -1 if table is full
 */
int
install_version(char *version)
{
   int i;

   for (i = 0; i < MAX_MENUS; i++)
   {
      if (versions[i] == NULL)   /* empty slot */
      {
         versions[i] = version;
         return 0;
      }
   }

   /* table is full; notify caller */
   dtrap();
   return -1;
}

#endif   /* IN_MENUS */

