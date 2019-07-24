/*
 * FILENAME: userpass.h
 *
 * Copyright 1997- 2000 By InterNiche Technologies Inc. All rights reserved
 *
 * User & password defines for demo TCP/IP application. 
 * This is used by FTP, WebPort, PPP(pap), and any other network 
 * protocols which need a user name and password. 
 *
 * MODULE: TCP
 *
 *
 * PORTABLE: yes
 */


#ifndef _USERPASS_H_
#define  _USERPASS_H_   1


/* these #defines can be overridden from ipport.h */
#ifndef NUM_NETUSERS
#define  NUM_NETUSERS   12
#endif

#ifndef MAX_USERLENGTH
#define  MAX_USERLENGTH 32
#endif


/* the "app codes" for chgeck_permit() */
#define  HTTP_USER      1
#define  FTP_USER       2
#define  PPP_USER       3
#define  TELNET_USER    4

struct userpass
{
   char  username[MAX_USERLENGTH];
   char  password[MAX_USERLENGTH];
   void *   permissions;   /* for use by porting apps. */
};

extern   struct userpass   users[NUM_NETUSERS];
typedef  struct userpass   USER;

int    add_user(char * username, char * password, void *);
int    check_permit(char * username, char * password, int appcode, void *);
char * get_password(char * user);

#endif   /* _USERPASS_H_ */


