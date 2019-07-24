/*
 * FILENAME: vfsport.h
 *
 * Copyright 1997- 2000 By InterNiche Technologies Inc. All rights reserved
 *
 *
 * MODULE: VFS
 *
 *
 * PORTABLE: no
 */

/* Additional Copyrights: */

/* vfsport.h 
 * Portions Copyright 1996 by NetPort Software. All rights reserved. 
 */

#ifndef _VFSPORT_H_
#define  _VFSPORT_H_    1

#include "ipport.h"
#include "libport.h"
#include "vfsfiles.h"

/* the web server still makes heaviest use of the vfs. Includes it's
vfs setup here:*/
#ifdef WEBPORT
#include "webport.h"
#endif

/* Virtual File System related non-volatile parameters. Please see nvparms.h 
 * and nvparms.c regarding the usage of the following structure.
 */
#ifdef INCLUDE_NVPARMS
struct vfs_nvparam
{
   char     httppath[HTPATHMAX]; /* root path of http vfs files */
};

extern struct vfs_nvparam vfs_nvparms;
#endif   /* INCLUDE_NVPARMS */

#endif   /* _VFSPORT_H_ */

