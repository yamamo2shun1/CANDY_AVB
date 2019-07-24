/*
 * FILENAME: vfsutil.c
 *
 * Copyright 1997- 2000 By InterNiche Technologies Inc. All rights reserved
 *
 *
 * MODULE: VFS
 *
 * ROUTINES: vfs_file_detail(), vfs_file_list(), 
 * ROUTINES: vfs_change_flag(), vfs_set_flag(), vfs_clear_flag(), 
 * ROUTINES: vfs_do_sync(), vfs_open_list(), vfs_dir(), 
 * ROUTINES: vfs_unit_scan_index(), vfs_unit_scan_file_name(), 
 * ROUTINES: vfs_unit_list(), vfs_unit_vfopen(), vfs_unit_vfclose(), 
 * ROUTINES: vfs_unit_vunlink(), vfs_unit_vfread(), vfs_unit_vfwrite(), 
 * ROUTINES: vfs_unit_vfseek(), vfs_unit_vftell(), vfs_unit_vgetc(), 
 * ROUTINES: vfs_unit_vferror(), vfs_unit_vclearerr(), 
 * ROUTINES: vfs_tog_log_file_name(), vfs_led_test(), vfs_init(), 
 *
 * PORTABLE: yes
 */

/* Additional Copyrights: */

/* vfsutil.c 
 * Portions Copyright 1996 by NetPort Software. All rights reserved. 
 * InterNiche virtual file system. This implements the vfopen(), 
 * vfread(), etc, calls the Embedded WEB servers and similar devices 
 * to do file IO. These calls lookup names, descriptors, etc, to see 
 * if the file is a "virtual" file in RAM/ROM/FLASH, or if it should 
 * fall through to the local disk system (if any) 
 * 12/26/98 - Separated from WebPort sources. -JB- 
 */

#include "ipport.h"

#ifdef VFS_FILES

#include "vfsport.h" /* per-port include */
#include "vfsfiles.h"   /* overlay vfs stuff on system file IO */

#include "menu.h"

#ifdef IN_MENUS

/* VFS system commands */

struct vfs_flag_map_struct
{
   char  flag_name;  /* how flag bit is identified in display */
   char  editable;   /* can flag bit be modified by user */
   unsigned short bit_mask;   /* mask representing position of bit */
};

/* the VFS commands use the following array to map vfs_file.flags field
   bits to displayable characters */
/*
 * Altera Niche Stack Nios port modification:
 * Add braces to remove build warning
 */
struct vfs_flag_map_struct vfs_flag_map[] =
{
   { 'H',  1, VF_HTMLCOMPRESSED } ,
   { 'B',  1, VF_AUTHBASIC } ,
   { '5',  1, VF_AUTHMD5 } ,
   { 'M',  1, VF_MAPFILE } ,
   { 'V',  0, VF_CVAR } ,
   { 'W',  1, VF_WRITE } ,
   { 'I',  0, VF_DYNAMICINFO } ,
   { 'D',  0, VF_DYNAMICDATA } ,
   { 'N',  1, VF_NONVOLATILE } ,
   { 'S',  0, VF_STALE } ,
#ifdef WEBPORT
   { 's',  0, 0 } , /* these last three are special cases */
   { 'c',  0, 0 } ,
#endif   /* WEBPORT */
#ifdef HT_EXTDEV
   { 'm',  0  ,0 } ,
#endif   /* HT_EXTDEV */
   { 0, 0, 0 }   /* array terminator */
};

#define  VFS_FLAG_MAP_LEN  (sizeof(vfs_flag_map)/sizeof(vfs_flag_map[0]))

/* maximum length of a buffer where a file's details (e.g., name, start
 * address, length, etc.) are created.  (The string generated is used 
 * when a file listing is generated.
 */
#define FILEDETAIL_MAXLEN 80


/* FUNCTION: vfs_file_detail()
 * 
 * this function creates a detailed file directory listing for the
 * file addressed by the vfp parameter in the buffer addressed by buf
 *
 * PARAM1: struct vfs_file *vfp
 * PARAM2: char *buf
 *
 * RETURNS: 
 */

void vfs_file_detail(struct vfs_file * vfp, char * buf)
{
   int   i;
   char  name_string[FILENAMEMAX +  1];
   char  flag_string[VFS_FLAG_MAP_LEN  +  1];

   /* make a string containing the name of the file padded to
      the max file name length */
   strcpy(name_string,vfp->name);
   i = strlen(name_string);
   while (i < FILENAMEMAX)
      name_string[i++] = ' ';
   name_string[FILENAMEMAX] = 0;

   /* make a string listing the bits of vfp->flags */
   /* note that loop below terminates when bit_mask is 0 because the 
    * last 3 chars of the flags field are not derived from bits 
    * in the flags field
    */
   for (i = 0; vfs_flag_map[i].bit_mask; ++i)
   {
      flag_string[i] =
      (char) ((vfp->flags & vfs_flag_map[i].bit_mask)
       ? vfs_flag_map[i].flag_name : '-');
   }

#ifdef WEBPORT

   /* a non-null ssi_func field is represented by a 's' */
   flag_string[i] =
(char) (vfp->ssi_func ? vfs_flag_map[i].flag_name : '-');
   i++;

   /* a non-null cgi_func field is represented by a 'c' */
   flag_string[i] =
(char) (vfp->cgi_func ? vfs_flag_map[i].flag_name : '-');
   i++;

#endif   /* WEBPORT */

#ifdef HT_EXTDEV

   /* a non-null method field is represented by a 'm' */
   flag_string[i] =
(char) (vfp->method ? vfs_flag_map[i].flag_name : '-');
   i++;

#endif   /* HT_EXTDEV */

   /* terminate flags string */
   flag_string[i] = 0;

   /* format it all into the caller supplied buffer */
   snprintf(buf, FILEDETAIL_MAXLEN, "%s %s %p %8lx %8lx %8lx",
            name_string, flag_string, vfp->data,
            vfp->real_size, vfp->comp_size, vfp->buf_size);
}



/* FUNCTION: vfs_file_list()
 * 
 * PARAM1: void *pio
 *
 * RETURNS: 
 */

int vfs_file_list(void * pio)
{
   struct vfs_file * vfp;
   int   file_count  =  0;
   char  buffer[FILEDETAIL_MAXLEN];

   /* lock the VFS */
   vfs_lock();

   /* for each file in the file list */
   for (vfp = vfsfiles; vfp; vfp = vfp->next)
   {
      /* create a line of file listing */
      vfs_file_detail(vfp,buffer);

      /* and write it out */
      ns_printf(pio,"%s\n",buffer);

      file_count++;
      if ( con_page(pio,file_count) )
         break ;
   }

   ns_printf(pio,"total files = %d\n",file_count);

#ifdef HT_RWVFS

   ns_printf(pio,"dynamically allocated files = %ld, buffer space = 0x%lx\n",
    vfs_total_dyna_files,vfs_total_rw_space);

#endif   /* HT_RWVFS */

   vfs_unlock();

   return 0;
}

#ifdef HT_RWVFS



/* FUNCTION: vfs_change_flag()
 * 
 * PARAM1: void *pio
 * PARAM2: int set
 *
 * RETURNS: 
 */

int vfs_change_flag(void * pio, int set)
{
   char *   arg1;
   char *   arg2  =  NULL;
   char *   cp;
   struct vfs_file * vfp;
   VFILE *vfd;
   struct vfs_flag_map_struct *  pmap;
   int   bit_changed =  FALSE;
   char  flag_name;
   char  file_name[FILENAMEMAX   +  1];

   /* scan out first and second args to command */
   arg1 = nextarg(((GEN_IO) pio)->inbuf);
   if (arg1)
      arg2 = nextarg(arg1);

   /* if two args are not present, tell the user how to do the command */
   if (!arg2 || !*arg2)
   {
      ns_printf(pio,"usage:%s <file name> <bit>\n",
       set ? "vfssetflag" : "vfsclearflag");
      return 1;
   }

   /* get pointer to space at end of first arg */
   cp = strchr(arg1,' ');
   /* it had better be non-NULL */
   if (!cp)
   {
      dtrap();
      return 2;
   }

   /* copy and null terminate the file name */
   MEMCPY(file_name,arg1,cp - arg1);
   file_name[cp - arg1] = 0;

   /* get the name of the flag from the second arg */
   flag_name = *arg2;

   /* search the flag map array for the specified flag name */
   for (pmap = vfs_flag_map;
       pmap < (vfs_flag_map + VFS_FLAG_MAP_LEN); ++pmap)
   {
      if (pmap->flag_name == flag_name)
         break;
   }

   /* if the search failed, tell the user */
   if (pmap >= vfs_flag_map + VFS_FLAG_MAP_LEN)
   {
      ns_printf(pio,"%c is not a valid bit identifier\n",flag_name);
      return 3;
   }

   /* if the flag is not editable, tell the user */
   if (!(pmap->editable))
   {
      ns_printf(pio,"Flag %c may not be modified\n",flag_name);
      return 4;
   }

   /* lock the VFS */
   vfs_lock();

   /* see if the file exists */
   vfp = vfslookup_locked(file_name);
   if (!vfp)
   {
      vfs_unlock();
      ns_printf(pio,"file name %s is not in the VFS\n",file_name);
      return 5;
   }

   /* open the file for read access */
   vfd = vfopen_locked(file_name,"r");

   /* since we already verified that the file exists above and we've 
    * locked the file system to prevent others from unlinking it, the 
    * above open should work. 
    * If it doesn't work, something is very wrong
    */
   if (!vfd)
   {
      vfs_unlock();
      dtrap();
      return 6;
   }

   /* if we are setting the bit, set it */
   if (set)
   {
      /* if its not set now */
      if (!(vfp->flags & pmap->bit_mask))
      {
         /* set it and flag that we changed it */
         vfp->flags |= pmap->bit_mask;
         bit_changed = TRUE;
      }
   }
   else  /* else reset it */
   {
      /* if its set now */
      if (vfp->flags & pmap->bit_mask)
      {
         /* reset it and flag that we changed it */
         vfp->flags &= ~(pmap->bit_mask);
         bit_changed = TRUE;
      }
   }

#ifdef HTML_COMPRESSION

   /* if its the compression bit we are fidding and we actually 
    * changed something, then we want to set the real_size field 
    * accordingly 
    */
   if ((pmap->bit_mask == VF_HTMLCOMPRESSED) && bit_changed)
   {
      /* if the compression bit is being turned off, then the "size 
       * before compression" is the same as its actual 
       * compressed size
       */
      if (!set)
         vfp->real_size = vfp->comp_size;
      else
      /* we are turning the compression bit on, so we need to run the 
       * decompression algorithm on the file to determine 
       * how big it was before it was compressed 
       */
      {
         unsigned long count;

         count = 0;
         while (vgetc_locked(vfd) != EOF)
            ++count;
         vfp->real_size = count;
      }
   }

#endif   /* HTML_COMPRESSION */

   /* if we actually changed something,
      set the stale bit so when we close the file, it will get synced */
   if (bit_changed)
      vfp->flags |= VF_STALE;

   vfclose_locked(vfd);

   vfs_unlock();

   return 0;   /* success */
}



/* FUNCTION: vfs_set_flag()
 * 
 * PARAM1: void *pio
 *
 * RETURNS: 
 */

int vfs_set_flag(void * pio)
{
   return vfs_change_flag(pio,TRUE);
}



/* FUNCTION: vfs_clear_flag()
 * 
 * PARAM1: void *pio
 *
 * RETURNS: 
 */

int vfs_clear_flag(void * pio)
{
   return vfs_change_flag(pio,FALSE);
}

#ifdef HT_SYNCDEV    /* Do we support sync to flash device? */



/* FUNCTION: vfs_do_sync()
 * 
 * PARAM1: void *pio
 *
 * RETURNS: 
 */

int vfs_do_sync(void * pio)
{
   /* this implementation will not be suitable for target systems in 
    * which vfs_sync() determines what it should do based on the 
    * parameter that is passed to it. this implementation only makes 
    * sense if vfs_sync() does the same thing, that is syncs the 
    * entire file system to backing store, irrespective of the 
    * parameter that is passed to it. if the target system's 
    * implementation of vfs_sync() DOES function differently based on 
    * the parameter passed to it, this command should be modified to 
    * allow the appropriate parameters to be passed to it
    */
   ns_printf(pio,"VFS sync initiated\n");
   vfs_lock();
   vfs_sync(NULL);
   vfs_unlock();
   ns_printf(pio,"VFS sync completed\n");

   return 0;
}
#endif   /* HT_SYNCDEV */

#endif   /* HT_RWVFS */



/* FUNCTION: vfs_open_list()
 * 
 * PARAM1: void *pio
 *
 * RETURNS: 
 */

int vfs_open_list(void * pio)
{
   VFILE * vfd;
   unsigned long count = 0;
   unsigned long orphans = 0;

   vfs_lock();

   for (vfd = vfiles; vfd; vfd = vfd->next)
   {
      /* if the file that the handle is pointing at still exists */
      if (vfd->file)
      {
         /* display its name */
         ns_printf(pio,"%s\n",vfd->file->name);
      }
      else  /* else the file was deleted since the open */
      {
         /* so just increment the count of orphans */
         orphans++;
      }
      count++;
   }

   vfs_unlock();
   ns_printf(pio,"total files open = %ld\n",count);
   if (orphans)
      ns_printf(pio,"total orphans = %ld\n",orphans);
   return 0;
}





/* FUNCTION: vfs_dir()
 * 
 * vfs_dir() - a vfs dir command for the menus
 *
 * PARAM1: void * pio
 *
 * RETURNS: 
 */

int
vfs_dir(void * pio)
{
   return vfs_file_list(pio);
}

#ifdef VFS_UNIT_TEST

/* code in here is used to unit test the VFS */
/* it was thrown together quick, so pardon the spare comments */

#define  VFS_NUM_UNIT_TEST_FDS   10

VFILE *vfs_unit_test_fds[VFS_NUM_UNIT_TEST_FDS];

unsigned int atoh(char *buf);
unsigned long atohl(char *buf);



/* FUNCTION: vfs_unit_scan_index()
 * 
 * PARAM1: void *pio
 * PARAM2: char *arg
 * PARAM3: unsigned int *pindex
 *
 * RETURNS: 
 */

int vfs_unit_scan_index(void * pio, char * arg, unsigned int * pindex)
{
   *pindex = atoh(arg);
   if (*pindex >= VFS_NUM_UNIT_TEST_FDS)
   {
      ns_printf(pio,"bad index 0x%x\n",*pindex);
      return 1;
   }
   return 0;
}



/* FUNCTION: vfs_unit_scan_file_name()
 * 
 * PARAM1: void *pio
 * PARAM2: char *arg
 * PARAM3: char *file_name
 *
 * RETURNS: 
 */

int vfs_unit_scan_file_name(void * pio, char * arg, char * file_name)
{
   char *   cp;

   /* get pointer to space at end of first arg */
   cp = strchr(arg,' ');

   /* if no space, make cp point to null and end of string */
   if (!cp)
      cp = arg + strlen(arg);

   /* check for too long */
   if ((cp - arg) > FILENAMEMAX)
   {
      ns_printf(pio,"bad file name\n");
      return 1;
   }

   /* copy and null terminate the file name */
   MEMCPY(file_name,arg,cp - arg);
   file_name[cp - arg] = 0;

   return 0;
}



/* FUNCTION: vfs_unit_list()
 * 
 * PARAM1: void *pio
 *
 * RETURNS: 
 */

int vfs_unit_list(void * pio)
{
   int   i;

   for (i = 0; i < VFS_NUM_UNIT_TEST_FDS; i++)
      ns_printf(pio,"%d %p\n",i,vfs_unit_test_fds[i]);
   return 0;
}



/* FUNCTION: vfs_unit_vfopen()
 * 
 * PARAM1: void *pio
 *
 * RETURNS: 
 */

int vfs_unit_vfopen(void * pio)
{
   char *   arg1;
   char *   arg2  =  NULL;
   char *   arg3  =  NULL;
   unsigned int index;
   VFILE *vfd;
   char  file_name[FILENAMEMAX   +  1];
   char  mode[FILENAMEMAX  +  1];

   /* scan out the index, file name and mode to pass to vfopen() */
   arg1 = nextarg(((GEN_IO) pio)->inbuf);
   if (arg1)
   {
      arg2 = nextarg(arg1);
      if (arg2)
         arg3 = nextarg(arg2);
   }

   /* if three args are not present, tell the user how to do the command */
   if (!arg3 || !*arg3)
   {
      ns_printf(pio,"usage:vfsvfopen <index> <file name> <mode>\n");
      return 1;
   }

   /* get and verify index */
   if (vfs_unit_scan_index(pio,arg1,&index))
      return 1;

   /* get and verify file_name */
   if (vfs_unit_scan_file_name(pio,arg2,file_name))
      return 1;

   /* get and verify mode */
   if (vfs_unit_scan_file_name(pio,arg3,mode))
      return 1;

   ns_printf(pio,"calling vfopen(%s,%s) index 0x%x\n",file_name,mode,index);

   vfd = vfopen(file_name,mode);

   ns_printf(pio,"vfopen() returned %p\n",vfd);
   ns_printf(pio,"get_vfopen_error() returned %d\n",get_vfopen_error());

   vfs_unit_test_fds[index] = vfd;
   return 0;
}



/* FUNCTION: vfs_unit_vfclose()
 * 
 * PARAM1: void *pio
 *
 * RETURNS: 
 */

int vfs_unit_vfclose(void * pio)
{
   char *   arg1;
   unsigned int index;

   /* scan out the index */
   arg1 = nextarg(((GEN_IO) pio)->inbuf);

   /* if arg not present, tell the user how to do the command */
   if (!arg1 || !*arg1)
   {
      ns_printf(pio,"usage:vfsvfclose <index>\n");
      return 1;
   }

   /* get and verify index */
   if (vfs_unit_scan_index(pio,arg1,&index))
      return 1;

   ns_printf(pio,"calling vfclose(%p) index 0x%x\n",
    vfs_unit_test_fds[index],index);

   vfclose(vfs_unit_test_fds[index]);

   vfs_unit_test_fds[index] = 0;
   return 0;
}



/* FUNCTION: vfs_unit_vunlink()
 * 
 * PARAM1: void *pio
 *
 * RETURNS: 
 */

int vfs_unit_vunlink(void * pio)
{
   char *   arg1;
   int   rc;
   char  file_name[FILENAMEMAX   +  1];

   /* scan out the file name */
   arg1 = nextarg(((GEN_IO) pio)->inbuf);

   /* if arg not present, tell the user how to do the command */
   if (!arg1 || !*arg1)
   {
      ns_printf(pio,"usage:vfsvunlink <file name>\n");
      return 1;
   }

   /* get and verify file_name */
   if (vfs_unit_scan_file_name(pio,arg1,file_name))
      return 1;

   ns_printf(pio,"calling vunlink(%s)\n",file_name);

   rc = vunlink(file_name);

   ns_printf(pio,"vunlink() returned %d\n",rc);

   return 0;
}



/* FUNCTION: vfs_unit_vfread()
 * 
 * PARAM1: void *pio
 *
 * RETURNS: 
 */

int vfs_unit_vfread(void * pio)
{
   char *   arg1;
   char *   arg2  =  NULL;
   unsigned int index;
   unsigned int count;
   int   rc;
   char  in_buf[256];

   /* scan out the index and count */
   arg1 = nextarg(((GEN_IO) pio)->inbuf);
   if (arg1)
   {
      arg2 = nextarg(arg1);
   }

   /* if two args are not present, tell the user how to do the command */
   if (!arg2 || !*arg2)
   {
      ns_printf(pio,"usage:vfsvfread <index> <count>\n");
      return 1;
   }

   /* get and verify index */
   if (vfs_unit_scan_index(pio,arg1,&index))
      return 1;

   /* get byte count to read */
   count = atoh(arg2);
   if (count > sizeof(in_buf))
   {
      ns_printf(pio,"bad count 0x%x\n",count);
      return 1;
   }

   ns_printf(pio,"calling vfread() of 0x%x bytes fd %p\n",
    count,vfs_unit_test_fds[index]);

   rc = vfread(in_buf,1,count,vfs_unit_test_fds[index]);

   ns_printf(pio,"vfread() returned %d\n",rc);
   hexdump(pio,in_buf,count);

   return 0;
}



/* FUNCTION: vfs_unit_vfwrite()
 * 
 * PARAM1: void *pio
 *
 * RETURNS: 
 */

int vfs_unit_vfwrite(void * pio)
{
   char *   arg1;
   char *   arg2  =  NULL;
   unsigned int index;
   int   count;
   unsigned int line_len;
   unsigned long byte_count = 0;
   int   i;
   int   len;
   int   rc;
   char  out_buf[30];

   /* scan out the index and count */
   arg1 = nextarg(((GEN_IO) pio)->inbuf);
   if (arg1)
   {
      arg2 = nextarg(arg1);
   }

   /* if two args are not present, tell the user how to do the command */
   if (!arg2 || !*arg2)
   {
      ns_printf(pio,"usage:vfsvfwrite <index> <count>\n");
      return 1;
   }

   /* get and verify index */
   if (vfs_unit_scan_index(pio,arg1,&index))
      return 1;

   /* get line count to write */
   count = (int) atoh(arg2);

   ns_printf(pio,"calling vfwrite() of 0x%x lines fd %p\n",
    count,vfs_unit_test_fds[index]);

   for (i = 0; i < count; i++)
   {
      sprintf(out_buf,"line number = %x ",i);
      len = strlen(out_buf);
      /* pad out buffer to constant length */
      for (; len < 24; len++)
      {
         out_buf[len] = (char) ('A' + ((i + len) % 26));
      }
      out_buf[len] = 0;
      line_len = strlen(out_buf);
      rc = vfwrite(out_buf,1,line_len,vfs_unit_test_fds[index]);
      if (rc != (int) line_len)
      {
         ns_printf(pio,"vfwrite() returned %d\n",rc);
         break;
      }
      byte_count += line_len;
   }

   ns_printf(pio,"%ld (0x%lx) bytes written\n",byte_count,byte_count);
   return 0;
}



/* FUNCTION: vfs_unit_vfseek()
 * 
 * PARAM1: void *pio
 *
 * RETURNS: 
 */

int vfs_unit_vfseek(void * pio)
{
   char *   arg1;
   char *   arg2  =  NULL;
   char *   arg3  =  NULL;
   unsigned int index;
   unsigned long offset;
   unsigned int mode;
   int   rc;

   /* scan out the index, offset and mode */
   arg1 = nextarg(((GEN_IO) pio)->inbuf);
   if (arg1)
   {
      arg2 = nextarg(arg1);
      if (arg2)
         arg3 = nextarg(arg2);
   }

   /* if three args are not present, tell the user how to do the command */
   if (!arg3 || !*arg3)
   {
      ns_printf(pio,"usage:vfsfseek <index> <offset> <mode>\n");
      return 1;
   }

   /* get and verify index */
   if (vfs_unit_scan_index(pio,arg1,&index))
      return 1;

   /* get offset and mode */
   offset = atohl(arg2);
   mode = atoh(arg3);

   ns_printf(pio,"calling vfseek(%p,0x%lx,0x%x)\n",
    vfs_unit_test_fds[index],offset,mode);

   rc = vfseek(vfs_unit_test_fds[index],offset,mode);

   ns_printf(pio,"vfseek() returned %d\n",rc);
   return 0;
}



/* FUNCTION: vfs_unit_vftell()
 * 
 * PARAM1: void *pio
 *
 * RETURNS: 
 */

int vfs_unit_vftell(void * pio)
{
   char *   arg1;
   unsigned int index;
   long  rc;

   /* scan out the index, offset and mode */
   arg1 = nextarg(((GEN_IO) pio)->inbuf);

   /* if arg not present, tell the user how to do the command */
   if (!arg1 || !*arg1)
   {
      ns_printf(pio,"usage:vfsftell <index>\n");
      return 1;
   }

   /* get and verify index */
   if (vfs_unit_scan_index(pio,arg1,&index))
      return 1;

   ns_printf(pio,"calling vftell(%p)\n",vfs_unit_test_fds[index]);

   rc = vftell(vfs_unit_test_fds[index]);

   ns_printf(pio,"vftell() returned %ld\n",rc);
   return 0;
}



/* FUNCTION: vfs_unit_vgetc()
 * 
 * PARAM1: void *pio
 *
 * RETURNS: 
 */

int vfs_unit_vgetc(void * pio)
{
   char *   arg1;
   char *   arg2  =  NULL;
   unsigned int index;
   unsigned int count;
   unsigned int i;
   int   rc;
   char  in_buf[256];

   /* scan out the index and count */
   arg1 = nextarg(((GEN_IO) pio)->inbuf);
   if (arg1)
   {
      arg2 = nextarg(arg1);
   }

   /* if two args are not present, tell the user how to do the command */
   if (!arg2 || !*arg2)
   {
      ns_printf(pio,"usage:vfsvgetc <index> <count>\n");
      return 1;
   }

   /* get and verify index */
   if (vfs_unit_scan_index(pio,arg1,&index))
      return 1;

   /* get byte count to read */
   count = atoh(arg2);
   if (count > sizeof(in_buf))
   {
      ns_printf(pio,"bad count 0x%x\n",count);
      return 1;
   }

   ns_printf(pio,"calling vgetc() of 0x%x times on fd %p\n",
    count,vfs_unit_test_fds[index]);

   for (i = 0; i < count; ++i)
   {
      rc = vgetc(vfs_unit_test_fds[index]);
      if (rc == EOF)
         break;
      in_buf[i] = (char) rc;
   }

   ns_printf(pio,"%d (0x%x) bytes read\n",i,i);
   hexdump(pio,in_buf,i);

   return 0;
}



/* FUNCTION: vfs_unit_vferror()
 * 
 * PARAM1: void *pio
 *
 * RETURNS: 
 */

int vfs_unit_vferror(void * pio)
{
   char *   arg1;
   unsigned int index;
   int   rc;

   /* scan out the index, offset and mode */
   arg1 = nextarg(((GEN_IO) pio)->inbuf);

   /* if arg not present, tell the user how to do the command */
   if (!arg1 || !*arg1)
   {
      ns_printf(pio,"usage:vfsferror <index>\n");
      return 1;
   }

   /* get and verify index */
   if (vfs_unit_scan_index(pio,arg1,&index))
      return 1;

   ns_printf(pio,"calling vferror(%p)\n",vfs_unit_test_fds[index]);

   rc = vferror(vfs_unit_test_fds[index]);

   ns_printf(pio,"vferror() returned %d\n",rc);
   return 0;
}



/* FUNCTION: vfs_unit_vclearerr()
 * 
 * PARAM1: void *pio
 *
 * RETURNS: 
 */

int vfs_unit_vclearerr(void * pio)
{
   char *   arg1;
   unsigned int index;

   /* scan out the index, offset and mode */
   arg1 = nextarg(((GEN_IO) pio)->inbuf);

   /* if arg not present, tell the user how to do the command */
   if (!arg1 || !*arg1)
   {
      ns_printf(pio,"usage:vfsclearerr <index>\n");
      return 1;
   }

   /* get and verify index */
   if (vfs_unit_scan_index(pio,arg1,&index))
      return 1;

   ns_printf(pio,"calling vclearerr(%p)\n",vfs_unit_test_fds[index]);

   vclearerr(vfs_unit_test_fds[index]);
   return 0;
}

extern   int   vfs_log_file_name;



/* FUNCTION: vfs_tog_log_file_name()
 * 
 * PARAM1: void *pio
 *
 * RETURNS: 
 */

int vfs_tog_log_file_name(void * pio)
{
   vfs_log_file_name = !vfs_log_file_name;
   ns_printf(pio,"file name logging is %s\n",
    vfs_log_file_name ? "on" : "off");
   return 0;
}

#ifdef AMD_NET186

/* defined in net186 flash.c */
void PostLEDs(unsigned char value);



/* FUNCTION: vfs_led_test()
 * 
 * PARAM1: void *pio
 *
 * RETURNS: 
 */

int vfs_led_test(void * pio)
{
   char *   arg1;
   unsigned int value;

   /* scan out the value */
   arg1 = nextarg(((GEN_IO) pio)->inbuf);

   /* if arg not present, tell the user how to do the command */
   if (!arg1 || !*arg1)
   {
      ns_printf(pio,"usage:vfsled <LED value>\n");
      return 1;
   }

   /* get line count to write */
   value = atoh(arg1);

   ns_printf(pio,"calling PostLEDs(0x%x)\n",value);

   PostLEDs((unsigned char) (value & 0xff));
   return 0;
}

#endif   /* AMD_NET186 */

#endif   /* VFS_UNIT_TEST */

/*
 * Altera Niche Stack Nios port modification:
 * Add braces to remove build warning
 */
struct menu_op vfs_menu[]  =
{
   { "vfs", stooges, "VFS commands" } ,
   { "vfsfilelist", vfs_file_list, "display vfs_file structure info" } ,
#ifdef HT_RWVFS
   { "vfssetflag", vfs_set_flag, "set bit in vfs_file flags field" } ,
   { "vfsclearflag", vfs_clear_flag, "clear bit in vfs_file flags field" } ,
#ifdef HT_SYNCDEV
   { "vfssync", vfs_do_sync, "sync the VFS to backing store" } ,
#endif
#endif   /* HT_RWVFS */
   { "vfsopenlist", vfs_open_list, "list currently open VFS files" } ,
#ifdef VFS_UNIT_TEST
   { "vfsunitlist", vfs_unit_list, "list unit test VFILE array" } ,
   { "vfsvfopen", vfs_unit_vfopen, "vfsvfopen <index> <file name> <mode>" } ,
   { "vfsvfclose", vfs_unit_vfclose, "vfsvfclose <index>" } ,
   { "vfsvunlink", vfs_unit_vunlink, "vfsvunlink <file name>" } ,
   { "vfsvfread", vfs_unit_vfread, "vfsvfread <index> <count>" } ,
   { "vfsvfwrite", vfs_unit_vfwrite, "vfsvfwrite <index> <count>" } ,
   { "vfsvfseek", vfs_unit_vfseek, "vfsvfseek <index> <offset> <mode>" } ,
   { "vfsvftell", vfs_unit_vftell, "vfsvftell <index>" } ,
   { "vfsvgetc", vfs_unit_vgetc, "vfsvgetc <index> <count>" } ,
   { "vfsvferror", vfs_unit_vferror, "vfsferror <index>" } ,
   { "vfsvclearerr", vfs_unit_vclearerr, "vfsclearerr <index>" } ,
   { "vfslogname", vfs_tog_log_file_name, "toggle display of passed file names" } ,
#ifdef AMD_NET186
   { "vfsled", vfs_led_test, "vfsled <led value>" } ,
#endif   /* AMD_NET186 */
#endif   /* VFS_UNIT_TEST */
   { NULL } 
};
#endif   /* IN_MENUS */



/* FUNCTION: vfs_init()
 * 
 * PARAM1: 
 *
 * RETURNS: 
 */

void vfs_init()
{
#ifdef HT_SYNCDEV    /* Do we support sync to flash device? */
#ifdef HT_RWVFS
   vfs_lock();
   /* restore the RAM resident VFS from whatever backing store has
      been provided */
   vfs_restore();
   vfs_unlock();
#endif   /* HT_RWVFS */
#endif   /* HT_SYNCDEV */

#ifdef INCLUDE_NVPARMS
   strncpy(vfs_root_path, vfs_nvparms.httppath, HTPATHMAX);
#endif   /* INCLUDE_NVPARMS */

}

#endif   /* VFS_FILES */


