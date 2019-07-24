/*
 * FILENAME: vfsport.c
 *
 * Copyright 2002 By InterNiche Technologies Inc. All rights reserved
 *
 *
 * MODULE: VFS
 *
 * ROUTINES: vfs_nvset(), prep_vfs().
 *
 * PORTABLE: NO
 */

#include "ipport.h"
#include "libport.h"
#include "in_utils.h"

#ifdef VFS_FILES

#include "vfsport.h"

#ifdef INCLUDE_NVPARMS
#include "nvparms.h"
#endif   /* INCLUDE_NVPARMS */

#ifdef IN_MENUS
#include "menu.h"
#endif   /* IN_MENUS */

#ifdef IN_MENUS
extern struct menu_op vfs_menu[];
#endif   /* IN_MENUS */


#ifdef INCLUDE_NVPARMS
/* Please see nvparms.h and nvparms.c regarding the usage of
 * the following datatypes and functions.
 */

struct vfs_nvparam vfs_nvparms;

extern char vfs_root_path[MAX_NVSTRING];  /* root path of http vfs files */

struct nvparm_info vfs_nvformats[] = 
{
   {"http root: %s\n", NVSTRING, MAX_NVSTRING, &vfs_nvparms.httppath, NULL, }, 
};

#define NUMVFS_FORMATS  \
        (sizeof(vfs_nvformats)/sizeof(struct nvparm_info))

#endif /* INCLUDE_NVPARMS */


#ifdef INCLUDE_NVPARMS

/* FUNCTION: vfs_nvset()
 * 
 * PARAM1: NV_FILE * fp
 *
 * RETURNS: Silent return of 0 for OK
 */
int vfs_nvset(NV_FILE * fp)
{
int i = 0;

   nv_fprintf(fp, vfs_nvformats[i++].pattern, vfs_root_path);
   return 0;
}

struct nvparm_format vfs_format = 
{
   NUMVFS_FORMATS,
   &vfs_nvformats[0],
   vfs_nvset,
   NULL
};

#endif   /* INCLUDE_NVPARMS */


/* FUNCTION: prep_vfs()
 *
 * PARAMS: NONE
 *
 * RETURNS: Error Code or 0 for OK
 */
int prep_vfs(void)
{
int e = 0;

#ifdef IN_MENUS
   /* install the VFS commands */
   e = install_menu(&vfs_menu[0]);
#endif   /* IN_MENUS */

#ifdef INCLUDE_NVPARMS
   e = install_nvformat(&vfs_format, nv_formats);
   if(e)
   {
      dprintf("unable to install VFS NVPARMS, reconfigure nv_formats[]\n");
      dtrap();
   }
#endif   /* INCLUDE_NVPARMS */
   return e;
}

#endif /* VFS_FILES */
