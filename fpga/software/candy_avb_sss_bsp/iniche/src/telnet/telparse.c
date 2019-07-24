/*
 * FILENAME: telparse.c
 *
 * Copyright 1998- 2000 By InterNiche Technologies Inc. All rights reserved
 *
 *
 * MODULE: TELNET
 *
 * ROUTINES: tel_parse(), tel_parse_cmd(), tel_parse_option(), 
 * ROUTINES: tel_start_nego(), tel_parse_subcmd(), tel_opt_search(), 
 * ROUTINES: tel_send_cmd(),  
 *
 * PORTABLE: yes
 */


/* File telparse.c Implementation file for Telnet Server 
 */
#include "telnet.h"

#ifdef TELNET_SVR       /* Then include this source file */


/* Local variables */

#ifdef TEL_SHOW_MSG

char *   tel_cmd_str[20]={
   "EOF",
   "Suspend Process",
   "Abort Process",
   "Unknown (239)",
   "Subnegotiation End",
   "NOP",
   "Data Mark",
   "Break",
   "Interrupt Process",
   "Abort Output",
   "Are You There",
   "Erase Character",
   "Erase Line",
   "Go Ahead",
   "Subnegotiate",
   "Will",
   "Won't",
   "Do",
   "Don't",
   "IAC",
};

char *   tel_opt_str[256]={      /* ascii strings for Telnet options */
   "Binary",                     /* 0 */
   "Echo",
   "Reconnection",
   "Supress Go Ahead",
   "Message Size Negotiation",
   "Status",                              /* 5 */
   "Timing Mark",
   "Remote Controlled Trans and Echo",
   "Output Line Width",
   "Output Page Size",
   "Output Carriage-Return Disposition",     /* 10 */
   "Output Horizontal Tab Stops",
   "Output Horizontal Tab Disposition",
   "Output Formfeed Disposition",
   "Output Vertical Tabstops",
   "Output Vertical Tab Disposition",        /* 15 */
   "Output Linefeed Disposition",
   "Extended ASCII",
   "Logout",
   "Byte Macro",
   "Data Entry Terminal",                    /* 20 */
   "SUPDUP",
   "SUPDUP Output",
   "Send Location",
   "Terminal Type",
   "End of Record",                       /* 25 */
   "TACACS User Identification",
   "Output Marking",
   "Terminal Location Number",
   "3270 Regime",
   "X.3 PAD",                             /* 30 */
   "Negotiate About Window Size",
   "Terminal Speed",
   "Toggle Flow Control",
   "Linemode",
   "X Display Location",                  /* 35 */
   "Environment",
   "Authentication",
   "Data Encryption",
   "39",
   "40","41","42","43","44","45","46","47","48","49",
   "50","51","52","53","54","55","56","57","58","59",
   "60","61","62","63","64","65","66","67","68","69",
   "70","71","72","73","74","75","76","77","78","79",
   "80","81","82","83","84","85","86","87","88","89",
   "90","91","92","93","94","95","96","97","98","99",
   "100","101","102","103","104","105","106","107","108","109",
   "110","111","112","113","114","115","116","117","118","119",
   "120","121","122","123","124","125","126","127","128","129",
   "130","131","132","133","134","135","136","137","138","139",
   "140","141","142","143","144","145","146","147","148","149",
   "150","151","152","153","154","155","156","157","158","159",
   "160","161","162","163","164","165","166","167","168","169",
   "170","171","172","173","174","175","176","177","178","179",
   "180","181","182","183","184","185","186","187","188","189",
   "190","191","192","193","194","195","196","197","198","199",
   "200","201","202","203","204","205","206","207","208","209",
   "210","211","212","213","214","215","216","217","218","219",
   "220","221","222","223","224","225","226","227","228","229",
   "230","231","232","233","234","235","236","237","238","239",
   "240","241","242","243","244","245","246","247","248","249",
   "250","251","252","253","254",
   "Extended Options List"                /* 255 */
};

#endif   /* TEL_SHOW_MSG */


/* FUNCTION: tel_parse()
 * 
 * Parse the input string that has been just received 
 * (Would have been copied into rxbuf of tconn)
 * All the telnet commands present in rxbuf would be
 * processed and stripped out. So at the end of this
 * routine, rxbuf will not contain any telnet commands.
 *
 * PARAM1: TEL_CONN tconn - Pointer to telnet connection (IN)
 *
 * RETURNS: SUCCESS or error number
 * later- we may have to search ahead for DM 
 */

int tel_parse(TEL_CONN tconn)
{
   u_char * curr_pos=(u_char *)tconn->rxbuf;
   u_char * mark     =(u_char *)tconn->rxbuf+tconn->rxsize;
   int   bytes_to_skip=0;

   while ( curr_pos < mark )
   {
      /* Scan through the whole buffer for special characters */
      while ( curr_pos < mark && (*curr_pos) != TEL_IAC )
      {
         curr_pos++;
      }

      if ( curr_pos >= mark )    /* We have parsed everything we received */
         break ;

      if ( (*curr_pos) == TEL_IAC )    /* Check it again, just to be safe */
      {
         if ( (*(curr_pos+1)) == TEL_IAC )   /* padded IAC. So its data */
         {
            info_printf(("Padded IAC.\n"));
            bytes_to_skip=1;

            tel_move(curr_pos,bytes_to_skip,(mark-curr_pos));
            tconn->rxsize   -=bytes_to_skip;
            mark            -=bytes_to_skip;

            continue ;
         }
         else
         {
            bytes_to_skip= tel_parse_cmd(tconn,curr_pos);

            if ( bytes_to_skip < 0 )
            {
               dprintf("Error %d in tel_parse_cmd()\n",bytes_to_skip);
               /* Either it can be error in ExecCmd or OptionCmd. So 
                * let us assume that there was error in ExecCmd. 
                * ExecCmd is of 2 bytes.
                */
               bytes_to_skip=2; 
            }

            tel_move(curr_pos,bytes_to_skip,(mark-curr_pos));
            tconn->rxsize   -=bytes_to_skip;
            mark            -=bytes_to_skip;

            continue ;
         }
      }
   }

   return SUCCESS;
}




/* int tel_move(char int skip, int buf_size) 
 * { 
 *    MEMMOVE(curr_pos,curr_pos+skip,buf_size-skip);
 * } 
 */


/* FUNCTION: tel_parse_cmd()
 * 
 * Parse the command and take action on it.
 * If we it is a sub-cmd, then call tel_parse_subcmd()
 * to have it processed. For DO/WILL/DONT/WONT option 
 * negotiation, call tel_parse_option()
 *
 * PARAM1: TEL_CONN tconn - Pointer to telnet connection (IN)
 * PARAM2: u_char * pos - Postion of command in rxbuf (IN)
 *
 * RETURNS: Length of the command processed. Returns negative number
 * if any error occurs.
 */

int
tel_parse_cmd(TEL_CONN tconn, u_char * pos)
{

    int   retValue=2; /* By default we process 2 chars:IAC, and CMD */
   /* DO,DONT,WILL,WONT process 3*/
   /* Subnegotiation processes n chars */

   info_printf(("RECV: %s %s\r\n",TEL_CMD_STR(*(pos+1)),TEL_OPT_STR(*(pos+2))));

   if ( (*pos) == TEL_IAC )
   {
      /* Check the next byte */
      if ( (*(pos+1)) >= TEL_SE ) /* SE is the lowest numbered telnet cmd */
      {

         switch ( (*(pos+1)) )
         {
         case TEL_AYT    :  /* received a telnet Are-You-There command     */
            /* Send back some visible evidence that we got it */
            ns_printf(&(tconn->tel_io),"Got Are-You-There cmd.\n%s",
             tconn->prompt);
            break;

         case TEL_IP     :  /* received a telnet Interrupt Process command */
            /* terminate previous task on this session */
            /* For example, if pinging is going on, terminate it */
            break;

         case TEL_AO     :  /* received a telnet Abort Output command      */
            /* In a preemptive system, set the tel_io structure such 
             * that output goes to /dev/null. That is the telnet peer 
             * should not get any messages. This command does not 
             * have much use on a non-preemptive OS.
             */
            break;
         case TEL_EL     :  /* received a telnet erase line command        */
         case TEL_EC     :  /* received a telnet erase character command   */
         case TEL_BREAK  :  /* received a telnet Break command             */
         case TEL_DM     :  /* received a telnet Data Mark command         */
         case TEL_NOP    :  /* received a telnet No Operation command      */
         case TEL_SE     :  /* received a telnet Subnegotiation End command*/
         case TEL_ABORT  :  /* received a telnet Abort Process command     */
         case TEL_SUSP   :  /* received a telnet Suspend Process command   */
         case TEL_EOF    :  /* received a telnet EOF command               */
         case TEL_GOAHEAD:  /* received   telnet go ahead option           */
            dprintf("Got telnet cmd %d.\n",*(pos+1));
            break;

         case TEL_DO     :  /* received a telnet DO negotiation            */
         case TEL_DONT   :  /* received a telnet DONT option               */
         case TEL_WILL   :  /* received a telnet WILL option               */
         case TEL_WONT   :  /* received a telnet WONT option               */
            tel_parse_option(tconn,*(pos+1),*(pos+2));
            retValue=3;
            break ;
         case TEL_SB     :  /* received   telnet sub-options negotiation   */
            retValue = tel_parse_subcmd(tconn,pos) ;
            break ;
         }
      }
      else
      {
         dtrap();
         dprintf("Unknown telnet cmd %d:%d\n",*pos,*(pos+1) );
         return TEL_N_CMD_UNKNOWN;
      }
   }  
   else
   {
     /* Some other character . For eg. ESC. 
        Right now we are not processing it */
      return TEL_N_NO_IAC;
   }

   return retValue ;     
}


/* FUNCTION: tel_parse_option()
 * 
 * Parse a DO/DONT/WILL/WONT negotiation option and
 * take appropriate action. Appropriate action include 
 * setting values for the option and sending a reply to
 * the peer.
 *
 * PARAM1: TEL_CONN tconn - Pointer to TelnetConn (IN)
 * PARAM2: int cmd - Option Cmd. It can be either of DO/DONT/WILL/WONT (IN)
 * PARAM3: int new_opt - Option that we are refering to (IN)
 *
 * REMARKS: NCSA telnet implementation has got a SWITCH->CASE 
 * for each option. So if a new option is to be added,
 * 1. The parameter needs to be added to telnet structure,
 * 2. Additions to be done to SWITCH for the new option.
 * 3. New set of similar code. More chances of bug. Bulkier
 * code. Advantage would be minimal reduction in processing
 * time. 
 * Hence we have a generic structure TelnetOption to hold
 * values for an option. There is a global structure tel_opt_list
 * which is a collection of all the supported options.
 * When a new telnet session is created, it starts with
 * a copy of the default options. This function is 
 * a generic mechanism to manipulate values for these options.
 * So if this function works perfect, it works so for
 * all the options.
 *
 * RETURNS: Nothing.
 * Algorithm   : The algorithm used here is as follows.
 * 1. For each telnet session, there is an structure containing
 * all the options supported.
 * 2. If we can't find "new_opt"  for this session, then
 *    a. Give negative reply for received DO/WILL cmd
 *    b. Do nothing for received DONT/WILL. This is a rare
 *    condition. Because
 *    (i) A DONT/WONT is either a reply
 *    (ii) Or DONT/WONT is done halfway thru a session
 *    to disable a previously agreed option.
 * 3. If a DO OPTION is received
 *    a. If the value is already set, do nothing.
 *    b. Check if the option is configurable. Otherwise
 *    reply with a WONT.
 *    c. Check if we had sent a WILL request for this option.
 *    If yes, enable the option. If no, enable the option
 *    and reply with a WILL.
 * 4. If a DONT OPTION is received,
 *    a. If the option is already disabled, do nothing.
 *    b. Otherwise check if we had sent a request.
 *    If yes, then disable this option. 
 *    If no,  then diable this option and reply with WONT.
 * 5. WILL, WONT will have similar explanations. 
 * 6. Each option will have two set of values. One for the
 * local TELNET session & other for the REMOTE telnet session.
 *    a. DO,DONT manipulate values  for the LOCAL telnet session
 *    b. WILL,WONT manipulate values for the REMOTE 
 *    telnet session
 */

void 
tel_parse_option(TEL_CONN tconn, int cmd, int new_opt)
{
   TEL_OPT opt;

   opt=tel_opt_search(tconn->options,new_opt);
   if ( opt == NULL )
   {
      info_printf(("[%d] not found in option list.\n",new_opt));
   }

   switch ( cmd )
   {
   case TEL_DO  :   /* received a telnet DO negotiation            */
      /* First check if we have new_opt */
      if ( opt == NULL )
      {
         /* We don't support new_opt */
         tel_send_cmd(tconn->sock,TEL_WONT,new_opt);
      }
      else
      {
         if ( opt->l_value == TRUE )  /* it is already set */
         {
            /* Do Nothing */
         }      
         else
         {
            if ( opt->config == FALSE ) 
            {
               /* Not configurable via telnet */
               tel_send_cmd(tconn->sock,TEL_WONT,new_opt);
            }
            else
            {
               /* Have we asked for this option ? */
               if ( opt->l_req_sent == FALSE )
               {
                  /* Tell peer that we accept the offer*/
                  tel_send_cmd(tconn->sock,TEL_WILL,new_opt );
               }
               opt->l_value = TRUE;
            }
         }

         if ( opt->l_req_sent == TRUE )
         {
            /* Yes, we had requested this option & got a response */
            opt->l_req_sent = FALSE;
         }
      }
      break ;

   case TEL_DONT:   /* received a telnet DONT option               */
      /* First check if we have new_opt */
      if ( opt == NULL )
      {
         /* We don't support new_opt */
      }
      else
      {
         /* We receive DONT for an OPTION, under two conditions 1. We 
          * had sent a "WILL Option" command 2. Previously we had 
          * agreed for this parameter via (a) Local machine : WILL 
          * ... , reply DO (b) Remote machine : DO ... , reply WILL 
          * And now halfway thru the session, the PEER wants to 
          * disable this option. This can be done only if the option 
          * is configurable. Hence we don't need to verify it again 
          * over here. Similar argument is valid for WONT also. 
          */
         if ( opt->l_value == TRUE )  /* it is set */
         {
            /* Have we asked for this option ? */
            if ( opt->l_req_sent == FALSE )
            {
               /* Tell peer that we agree */
               tel_send_cmd(tconn->sock,TEL_WONT,new_opt);
            }
            opt->l_value = FALSE;
         }
         else    /* Option is already disabled */
         {  
            /* do nothing */
         }      

         if ( opt->l_req_sent == TRUE )
         {
            /* Yes, we had requested this option & got a response */
            opt->l_req_sent = FALSE;
         }
      }
      break ;

   case TEL_WILL:   /* received a telnet WILL option               */
      /* First check if we have new_opt */
      if ( opt == NULL )
      {
         /* We don't support new_opt */
         tel_send_cmd(tconn->sock,TEL_DONT,new_opt);
      }
      else
      {
         if ( opt->r_value == TRUE )  /* it is already set */
         {
            /* do nothing */
            info_printf(("WILL %s rcvd. .\n", TEL_CMD_STR(new_opt) ));
         }      
         else
         {
            if ( opt->config == FALSE ) 
            {
               /* Not configurable via telnet */
               tel_send_cmd(tconn->sock,TEL_DONT,new_opt);
            }
            else
            {
               /* Have we asked for this option ? */
               if ( opt->r_req_sent == FALSE )
               {
                  /* Tell peer that we accept the offer*/
                  tel_send_cmd(tconn->sock,TEL_DO,new_opt);
               }
               opt->r_value = TRUE;
            }
         }

         if ( opt->r_req_sent == TRUE )
         {
            /* Yes, we had requested this option & got a response */
            opt->r_req_sent = FALSE;
         }
      }

      break ;

   case TEL_WONT:   /* received a telnet WONT option               */
      /* First check if we have new_opt */
      if ( opt == NULL )
      {
         /* We don't support new_opt */
      }
      else
      {
         if ( opt->r_value == TRUE )  /* it is set */
         {
            /* Have we asked for this option ? */
            if ( opt->r_req_sent == FALSE )
            {
               /* Tell peer that we accept the offer*/
               tel_send_cmd(tconn->sock,TEL_DONT,new_opt);
            }
            opt->r_value = FALSE;
         }
         else    /* Option is already disabled */
         {  
            /* do nothing */
         }      

         if ( opt->r_req_sent == TRUE )
         {
            /* Yes, we had requested this option & got a response */
            opt->r_req_sent = FALSE;
         }
      }

      break ;
      default :
         dtrap();    /* This should't happen !!!! */
   }
}


/* FUNCTION: tel_start_nego()
 * 
 * Search the TelOptionList and start negotiations for
 * marked options. That is if l_negotiate or r_negotiate
 * field for an option is TRUE, then send the WILl or DO
 * command for that option. And set l_req_sent or l_req_sent
 * to TRUE. When the reply comes from the peer, then
 * tel_parse_option() will update the values of the option.
 *
 * PARAM1: TEL_CONN tconn
 * Pointer to  TelnetConn for which options are to be
 * negotiated (IN)
 *
 * RETURNS: void
 */

void
tel_start_nego(TEL_CONN tconn)
{

   int   cnt   =  sizeof(struct  TelOptionList)/sizeof(struct  TelnetOption)  ;
   TEL_OPT opt;
   int   i;

   opt = (TEL_OPT)tconn->options ;

   for ( i=0 ; i < cnt ; i++ )
   {
      if ( opt->l_negotiate == TRUE )
      {
         tel_send_cmd(tconn->sock,TEL_WILL,opt->option);
         opt->l_req_sent=TRUE;
         info_printf(("WILL %s\n",TEL_OPT_STR(opt->option)));
      }
      if ( opt->r_negotiate == TRUE )
      {
         tel_send_cmd(tconn->sock,TEL_DO,opt->option);
         opt->r_req_sent=TRUE;
         info_printf(("DO %s\n",TEL_OPT_STR(opt->option)));
      }

      opt++;
   }
}


/* FUNCTION: tel_parse_subcmd()
 * 
 * Parse the sub-command and take action on it.
 * Sub command start with SB and end with SE.
 *
 * PARAM1: TEL_CONN tconn - Pointer to telnet connection (IN)
 * PARAM2: u_char *cmd - Postion of command in rxbuf (IN)
 *
 * RETURNS: 
 */

int
tel_parse_subcmd(TEL_CONN tconn, u_char * cmd)
{
   /* Do some meaningful stuff here later */

   int   maxlen=255; /* If subcommand doesn't get completed in 255 chars, stop*/
   int   cmd_len=0;
   u_char * curr_pos;

   if ( (*cmd+1) != TEL_SB )
      return TEL_N_NO_SB;

   curr_pos=cmd+2;      /* go past IAC, SB */

   while ( !( (*curr_pos)==TEL_IAC && (*curr_pos+1)==TEL_SE ) )
   {
      curr_pos++;
      cmd_len++;

      if ( curr_pos - cmd > maxlen )
         return TEL_N_NO_SE ;
   }

   tel_proc_subcmd(tconn,cmd+2,cmd_len);

   return cmd_len+4 ;
}


/* FUNCTION: tel_opt_search()
 * 
 * Search the TelOptionList. If the option to be searched
 * is found, then return a pointer to that TelnetOption element.
 * If search fails, return NULL. TelOptionList is a structure
 * will all members of type "TelnetOption". ASKJB to see 
 * if it is portable.
 *
 * PARAM1: struct TelOptionList *list - Ptr to TelOptionList to be searched(IN)
 * PARAM2: int find - Option to be searched (IN)
 *
 * RETURNS: A pointer to the found option element. If search fails,
 * it returns NULL.
 */

TEL_OPT
tel_opt_search(struct TelOptionList * list, int find)
{
   int   cnt   =  sizeof(struct  TelOptionList)/sizeof(struct  TelnetOption)  ;
   TEL_OPT opt;
   int   i;

   if ( list == NULL )  /* sanity check */
      return NULL ;  

   opt = (TEL_OPT)list ;

   for ( i=0 ; i < cnt ; i++ )
   {
      if ( opt->option == find )
         return (opt);
      opt++;
   }

   return NULL ;
}



/* FUNCTION: tel_send_cmd()
 * 
 * Send a telnet command. It can be a command for negotiation
 * also. This function will handle only exec commands
 * (Abort Output, IP, etc.) , and option commands 
 * (DO,DONT,WILL,WONT). It will not handle sub-negotiation,
 * as it requires random number of bytes to be sent. So for
 * sub-negotiation, we will first fill the buffer and
 * then use the send command. ExecCmd is of 2 bytes,
 * and OptionCmd is of 3 bytes. For ExecCmd, the third
 * parameter is ignored.
 *
 * PARAM1: SOCKET sock - Socket on which the command has to be sent (IN)
 * PARAM2: int cmd - Command (IN)
 * PARAM3: int option - Option  (IN)
 *
 * RETURNS: Whatever is returned by SEND.
 */

int     
tel_send_cmd(SOCKET sock, int cmd, int option)
{
   unsigned char small_buf[3];
   small_buf[0]=(u_char)TEL_IAC;
   small_buf[1]=(u_char)cmd;
   small_buf[2]=(u_char)option;

   if ( cmd < TEL_WILL )   /* then it is a ExecCmd */
      return sys_send(sock,(char *) small_buf,2,0);
   else                    /* OptionCmd- DO/DONT/WILL/WONT */
      return sys_send(sock,(char *) small_buf,3,0);
}



#endif   /* TELNET_SVR  */ 


