/*
 * FILENAME: telmenu.c
 *
 * Copyright 1998- 2000 By InterNiche Technologies Inc. All rights reserved
 *
 *
 * MODULE: TELNET
 *
 * ROUTINES: tel_menu_init(), tel_show_opt(), tel_show_stats(), 
 * ROUTINES: tel_close_sess(), 
 *
 * PORTABLE: yes
 */


/* File telmenu.c Implementation of menus for TELNET 
 * 1/4/99 - Edited menu text -MPG- 
 */
#include "telnet.h"

#ifdef TELNET_SVR       /* Then include this source file */
#ifdef TEL_MENU

#include "menu.h"

/*
 * Altera Niche Stack Nios port modification:
 * Add braces to remove build warning
 */
struct menu_op telmenu[]   =  {
   { "telnet",    stooges,       "TELNET server menu" } ,
   { "tshow",     tel_show_opt,  "show the options values for all sessions" } ,
   { "tstats",    tel_show_stats,"show the statistics of all TELNET sessions" } ,
   { "logout",    NULL,          "logout of the TELNET session" } ,
   { "exit",      NULL,          "logout of the TELNET session" } ,
   { NULL } ,
};

/* Macro to show TRUE=1 and FALSE=0 in output */
#define  TEL_BOOL(X)    (  (X==TRUE)   ?  1  :  0  )


/* FUNCTION: tel_menu_init()
 * 
 * Install the telnet menus into system menus.
 *
 * PARAM1: none()
 *
 * RETURNS: SUCCESS or error number.
 */

int
tel_menu_init()
{
   install_menu(telmenu);
   install_version("telnet3.1");
   return SUCCESS ;
}


/* FUNCTION: tel_show_opt()
 * 
 * Show the values of options of all telnet sessions.
 *
 * PARAM1: void * pio - Pointer to GenericIO structure (IN)
 *
 * RETURNS: SUCCESS or error number
 */

int tel_show_opt(void * pio)
{
   TEL_CONN tconn;

   int   opt_cnt  =  sizeof(struct  TelOptionList)/sizeof(struct  TelnetOption)  ;
   TEL_OPT opt;
   int   i;
   int   cnt=0;

   tconn=telq;

   if ( tconn == NULL )
   {
      ns_printf(pio,"There are no open telnet sessions.\n");
   }
   else
   {
      ns_printf(pio,"Showing OPTION values for each telnet session....\n");
   }

   while ( tconn != NULL )
   {
      cnt++;
      ns_printf(pio,"\nSession %d : Socket is %lu......\n",cnt,tconn->sock);

      opt = (TEL_OPT)tconn->options ;
      for (i=0 ; i < opt_cnt ; ++i )
      {
         ns_printf(pio,"[%d]%s: configurable=%d\n", 
          opt->option, 
          TEL_OPT_STR(opt->option), 
          TEL_BOOL(opt->config));
         ns_printf(pio,"   For Local Session:value=%d, req sent=%d, nego=%d\n",
          TEL_BOOL(opt->l_value), 
          TEL_BOOL(opt->l_req_sent), 
          TEL_BOOL(opt->l_negotiate) );
         ns_printf(pio,"   For Remote Session:value=%d, req sent=%d, nego=%d\n",
          TEL_BOOL(opt->r_value), 
          TEL_BOOL(opt->r_req_sent), 
          TEL_BOOL(opt->r_negotiate) );
         opt++;
      }

      tconn=tconn->next;
   }

   return SUCCESS;
}


/* FUNCTION: tel_show_stats()
 * 
 * Show telnet statistics. 
 *
 * PARAM1: void * pio - Pointer to GenericIO structure (IN)
 *
 * RETURNS: SUCCESS or error number
 */

int tel_show_stats(void * pio)
{
   TEL_CONN tconn;

   int   cnt=0;

   ns_printf(pio,"Total connections opened = %ld\n",tel_stats.conn_opened);
   ns_printf(pio,"Total connections closed = %ld\n",tel_stats.conn_closed);

   tconn=telq;

   while ( tconn != NULL )
   {
      cnt++;
      ns_printf(pio,"\nTelnet Session %d: Showing statistics for socket %lu.\n",
       cnt,tconn->sock);

      ns_printf(pio,"Bytes rcvd=%ld, Cmds rcvd = %ld\n",
       tconn->stats.rcvd, tconn->stats.cmds );

      tconn=tconn->next;
   }

   ns_printf(pio,"Number of ongoing telnet sessions = %d.\n",cnt);

   ns_printf(pio,"Global variables (current value,default value)\n");
   ns_printf(pio,"Recv buf size: %d,%d\n", tglob.tel_recv_buf_size, 
      TEL_RECV_BUF_SIZE );
   ns_printf(pio,"Send buf size: %d,%d\n", tglob.tel_send_buf_size, 
      TEL_SEND_BUF_SIZE );
   ns_printf(pio,"Listen port: %d,%d\n", tglob.tel_port, 
      TELNET_PORT       );
   ns_printf(pio,"Max login tries allowed: %d,%d\n", tglob.tel_max_login_tries,
      TEL_MAX_LOGIN_TRIES);
   ns_printf(pio,"Idle timeout: %d,%d\n", tglob.tel_idle_time,
      TEL_IDLE_TIME  );

   return SUCCESS;

}

#ifdef CLOSE_SESS    /* Right now tel_close_sess doesn't work */


/* FUNCTION: tel_close_sess()
 * 
 * Show telnet statistics. 
 *
 * PARAM1: void * pio - Pointer to GenericIO structure (IN)
 *
 * RETURNS: SUCCESS or error number
 */

int tel_close_sess(void * pio)
{
   TEL_CONN tconn;
   char *   cp;
   int   cnt=0;
   int   sess_num;

   cp = nextarg(pio->inbuf);  /* see if user put parm on cmd line */
   if (!*cp)
   {
      ns_printf(pio,"usage:tclose <Telnet Session Number>\n");
      ns_printf(pio,"Telnet Session Number can be found from tstats command.\n");
      ns_printf(pio,"Eg:tclose 1\n");
      return -1;
   }

   sess_num=atoi(cp);
   if ( sess_num <= 0 )
   {
      ns_printf(pio,"Invalid session number %d.\n",sess_num);
      return -1;
   }

   tconn=telq;

   while ( tconn != NULL )
   {
      cnt++;
      if ( cnt == sess_num )
      {
         /* Close this telnet session */
         if ( pio == &(tconn->tel_io) )
         {
            ns_printf(pio,"Session can't close itself. Use logout.\n");
            return -1;
         }
         else
         {
            tel_delete(tconn);
            ns_printf(pio,"Telnet session %d successfully closed.\n",cnt);
            return SUCCESS;
         }
      }
      else
      {
         tconn=tconn->next;
      }
   }

   ns_printf(pio,"Could not find session %d.\n",sess_num);

   return -1;
}
#endif

#endif   /* TEL_MENU */
#endif   /* TELNET_SVR */

