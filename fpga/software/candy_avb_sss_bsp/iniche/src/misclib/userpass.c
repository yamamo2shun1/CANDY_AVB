/*
 * FILENAME: userpass.c
 *
 * Copyright 1997- 2000 By InterNiche Technologies Inc. All rights reserved
 *
 * userpass.c user & password management for demo TCP/IP application. 
 * This is used by FTP, WebPort, PPP(pap), and any other network 
 * protocols which need a user name and password.
 *
 * MODULE: MISCLIB
 *
 * ROUTINES: add_user(), check_permit(), get_password()
 * ROUTINES: list_users(), menu_add_user(), 
 *
 * PORTABLE: yes
 */


#include "ipport.h"
#include "in_utils.h"
#include "userpass.h"
#include "menu.h"

struct userpass users[NUM_NETUSERS];


/* FUNCTION: add_user()
 *
 * Add a username & password to the user database.
 *
 * PARAM1: char *             username string
 * PARAM2: char *             password string or NULL
 * PARAM3: void *             permissions
 *
 * RETURNS: int               TRUE if user/password combo accepted, else FALSE 
 *
 * The 'permissions' parameter is not used by the demo package,
 * but could be used for extra checking for the application.
 * Duplicate user/password combinations are accepted in case two
 * different apps (e.g. FTP and PPP try to register the same user.
 * A second entry is not created for duplicate calls. 
 *
 * If the username and password match with an existing entry, then
 * we check if "permissions" is present or not. If "permissions" is
 * present, then it is used as a new password. Hence the add_user()
 * call can be used to change the password for an existing user.
 * Refer change_pwd() for sample use.
 * 
 */

int
add_user(char *username, char *password, void *permissions)
{
   int   i;

   if (username == (void *)NULL)
      return FALSE;  /* don't allow null user */

   if (password == (void *)NULL)
      password = ""; /* allow null password */

   /* validate parameters */
   if ((strlen(username) >= MAX_USERLENGTH) ||
       (strlen(password) >= MAX_USERLENGTH) ||
       (permissions && (strlen(permissions) >= MAX_USERLENGTH)))
   {
      return FALSE;
   }

   /* look for an empty slot or a matching user/passwd entry */
   for (i = 0; i < NUM_NETUSERS; i++)
   {
      if (users[i].username[0] == 0)      /* found empty slot */
      {
         strncpy(users[i].username, username, MAX_USERLENGTH);
         strncpy(users[i].password, password, MAX_USERLENGTH);
         users[i].permissions = permissions;
         return TRUE;
      }
      else if ((strcmp(users[i].username, username) == 0) &&
               (strcmp(users[i].password, password) == 0))
      {
         if (permissions) 
         {
               /* assume that in this case permissions points to
                * new password */
            strncpy(users[i].password, (void *)permissions, MAX_USERLENGTH);
         }
         return TRUE;
      }
   }

   return FALSE;
}



/* FUNCTION: check_permit()
 *
 * check_permit() - This is called from applciations which want to 
 * verify a user/password combination. Two extra parameters are 
 * provided for the application's use. These are: - appcode is one of 
 * the defines in userpass.h. this will identify the calling module. 
 * - permissions. This may be used by apps as needed. 
 *
 * 
 * PARAM1: char *    username
 * PARAM2: char *    password
 * PARAM3: int       appcode
 * PARAM4: void *    permissions
 *
 * RETURNS: TRUE if use is allowed, FALSE if not.
 */

int
check_permit(char * username, 
   char *   password, 
   int      appcode,       /* application which called us */
   void *   permissions)   /* NULL if not used */
{
   int   i;

   for (i = 0; i < NUM_NETUSERS; i++)
   {
      if (strcmp(users[i].username, username) == 0)
         break;
   }

   if (i >= NUM_NETUSERS)
      return FALSE;


   if (strcmp(users[i].password, password) != 0)
      return FALSE;

   /* add any further checking based on appcode & permissions here: */
   USE_ARG(appcode);
   USE_VOID(permissions); 

   return TRUE;   /* this user is allowed in */
}


/* FUNCTION: get_password()
 *
 * get_password() - This is called from applications which want to 
 * find out the password for a particular user.
 * 
 * PARAM1: char *    user
 *
 * RETURNS: valid password (if user is found) or NULL.
 */

char *
get_password(char * user)
{
   int   i;

   for (i = 0; i < NUM_NETUSERS; i++)
   {
      if (strcmp(users[i].username, user) == 0)
         break;
   }

   if (i >= NUM_NETUSERS)
      return NULL;
   else
      return users[i].password;
}

#ifdef NET_STATS

/* FUNCTION: list_users()
 *
 * add_user() - List all users (username & password) int the database. 
 * 
 * PARAM1: void * pio
 *
 * RETURNS: Error Code or 0 for OK
 */

int
list_users(void * pio) 
{
   int i;
   int cnt=0;

   ns_printf(pio,"List of users and their passwords.\n");
   for (i = 0; i < NUM_NETUSERS; i++)
   {
      if (users[i].username[0] == 0)
         continue;   /* empty entry */

       ns_printf(pio, "%s, %s\n", 
          users[i].username, users[i].password);
       cnt++;
   }

   ns_printf(pio,"Total number of users : %d\n",cnt);
   return 0;
}

/* FUNCTION: menu_add_user()
 *
 * menu_add_user() - Add user from CLI.
 * 
 * PARAM1: void * pio
 *
 * RETURNS: Error Code or 0 for OK
 */

int
menu_add_user(void * pio) 
{
   char * arg =  nextarg(((GEN_IO)pio)->inbuf);
   char * next;

   if (!arg || !*arg)   /* no arg given */
   {
      ns_printf(pio,"Error - User name not specfied\n");
      ns_printf(pio,"Usage: adduser username, password\n");
      return -1;
   }

   next = nextcarg(arg);
   if (!next)
   {
      ns_printf(pio,"Error - Password not specfied\n");
      ns_printf(pio,"Usage: adduser username, password\n");
      return -1;
   }
   add_user(arg,next,NULL);

   return 0;
}

#endif
