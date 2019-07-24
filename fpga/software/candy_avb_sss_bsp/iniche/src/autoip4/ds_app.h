/*
 * FILENAME: ds_app.h
 *
 * Copyright 2002 By InterNiche Technologies Inc. All rights reserved
 *
 *   Definitions for sample AutoIP database.
 *
*/

#ifndef _DS_APP_H_
#define _DS_APP_H_   1

/* If application programmer (user) has defined his own database then
 * include his files inside the file "user_ds.h"; otherwise support
 * our simple default system.
 */
#ifdef USER_DS_SUPPORT
#include "user_ds.h"
#else    /* use simple local system */


/* AutoIP database - one for each iface entry. */

struct ds_struct
{
   /* tag_IP_ADDRESS_MODE is one of the eIP_METHOD enums.
    * If you don't want to use the default (auto IP), then
    * set this value to some other enum prior to calling
    * Upnp_init()
    */
   unshort  tag_IP_ADDRESS_MODE;

   /* the rest of these are set during the autoIP process. */
   u_char   tag_POLLING_ENABLED;    /* boolean */
   u_long   tag_NET_FIXED_IP;
   u_long   tag_NET_FIXED_SUBNET;
   u_long   tag_NET_FIXED_GATEWAY;
   u_long   tag_NET_IP_ADDRESS;
   u_long   tag_NET_SUBNET;
   u_long   tag_NET_GATEWAY;
   char     tag_IP_ADDRESS_STRING[50];
};

/* The database: */
extern struct ds_struct ds_structs[MAXNETS];


/* Access macros */

/* get routines set a value form the database. If the database
 * entry is zero then the value is enchanged
 */

#define  DS_get_word(tag, iface, data) \
   { *(unshort*)(data) = (unshort)ds_structs[iface].tag; }

#define  DS_get_long(tag, iface, data) \
   { *(u_long*)(data) = ds_structs[iface].tag; }


/* Set routines set a value in the database */

#define  DS_set_long(tag, iface, data) \
   ds_structs[iface].tag = data;

#define  DS_set_byte(tag, iface, data) \
   ds_structs[iface].tag = data;

#define  DS_set_string(tag, iface, data) \
   { strncpy(ds_structs[iface].tag, data, sizeof(ds_structs[iface].tag));}

#endif /* USER_DS_SUPPORT */
#endif /* _DS_APP_H_ */


