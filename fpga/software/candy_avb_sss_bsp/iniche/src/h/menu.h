/*
 * FILENAME: menu.h
 *
 * Copyright  2000 By InterNiche Technologies Inc. All rights reserved
 *
 * Structure for simple application menu system.
 *
 * MODULE: TCP
 *
 *
 * PORTABLE: yes
 */

/* Additional Copyrights: */
/* Portions Copyright 1990,1993 by NetPort Software */


#ifndef MENU_H
#define  MENU_H   1

#include "in_utils.h"

/* a menu can be defined as an array of these structures. Selecting a 
 * key can invoke the corrosponding function. 
 */

struct menu_op                /* structure to describe each menu item */
{
   char *   opt;              /* the option name */
   int (*func)(void * pio);   /* routine name to execute the option */
   char *   desc;             /* description of the option */
};

#define  CBUFLEN     128            /* size of cbuf */
extern   char        cbuf[CBUFLEN]; /* user command line buffer */

#define  MAX_MENUS   25
extern   struct menu_op *menus[MAX_MENUS];      /* master menu list */
extern   char *versions[MAX_MENUS];             /* master version list */

/* menu routine prototypes: */
extern   int      install_menu(struct menu_op * newmenu);   /* add new menu */
extern   int      install_version(char * version);       /* add new version */
extern   char *   getcmd(void);           /* get cmdline into cbuf while spinning tasks */
extern   void     showmenu(void * pio, struct menu_op *);
extern   int      help(void * pio);       /* display menu prompt text */
extern   int      stooges(void * pio);    /* called when system breaks */
extern   int      do_command(void * pio); /* parse & perform command */
extern   char *   nextarg(char*);         /* get nexxt arg from a string */
extern   int      stlen(char*);           /* like strlen, but not exactly */
extern   int      stcmp(char*, char*);    /* like strcmp, but not exactly */
extern   int      setdebug (void * pio);
extern   int      arp_stats(void * pio);
extern   u_long   sysuptime(void);

char **parse_args(char *buf, int argc, int *pargc_index);

#endif   /* MENU_H */

