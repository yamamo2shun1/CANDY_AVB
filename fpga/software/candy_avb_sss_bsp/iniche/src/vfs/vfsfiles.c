/*
 * FILENAME: vfsfiles.c
 *
 * Copyright 1997- 2000 By InterNiche Technologies Inc. All rights reserved
 *
 *
 * MODULE: VFS
 *
 * ROUTINES: vfopen(), set_vfopen_error(), get_vfopen_error(), 
 * ROUTINES: vf_alloc_and_link_vop(), *vf_alloc_buffer(), vf_free_buffer(), 
 * ROUTINES: vfopen_locked(), vfclose_locked(), vunlink_flag_open_files(),
 * ROUTINES: vfread(), vfwrite_locked(), vfseek(), vftell(), vgetc_locked(),
 * ROUTINES: vfslookup_locked(), (), vfslookup(), strippath(), 
 * ROUTINES: isvfile_locked(), (), isvfile(), vferror(), vclearerr(), 
 *
 * PORTABLE: yes
 */

/* Additional Copyrights: */

/* vfsfiles.c 
 * Portions Copyright 1996 by NetPort Software. All rights reserved. 
 * InterNIche virtual file system. This implements the vfopen(), 
 * vfread(), etc, calls the Embedded WEB servers and similar devices 
 * to do file IO. These calls lookup names, descriptors, etc, to see
 * if the file is a "virtual" file in RAM/ROM/FLASH, or if it should 
 * fall through to the local disk system (if any) 
 * 7/24/96 - Created by John Bartas 
 * 12/10/98 - Added RWVFS hooks for writable file support -JB- 
 * 12/26/98 - Separated from WebPort sources. 
 */

#include "ipport.h"

#ifdef VFS_FILES

#include "vfsport.h"    /* per-port include */

#include "vfsfiles.h"   /* overlay vfs stuff on system file IO */

#include "profiler.h"   /* InterNiche profile utility */
 
/* if this sort of thing is needed, it should probably be in ipport.h */
#ifdef NOTDEF
#ifdef HT_LOCALFS
#include <fcntl.h>   /* Standard lib stuff for file IO */
#include <io.h>
#endif   /* HT_LOCALFS */
#endif   /* NOTDEF */

#ifdef WEBPORT
#include "httpd.h"   /* local NetPort HTML includes */
#include "cgi.h"
#endif   /* WEBPORT */

#ifdef HTML_COMPRESSION
#include "htcmptab.h"   /* tag table, for compression */
#endif   /* HTML_COMPRESSION */

/* path to file system root. This must use UNIX slashes, and should not
be NULL. A single slash is OK. */
#ifdef HT_LOCALFS
/* if there's a local FS (e.g. a disk), force vfs created files to 
 * live in a special directory. This can be changed at startup 
 * time.
 */
char     vfs_root_path[HTPATHMAX]   =  { "/vfs/" };
#else /* the vfs is the whole file system */
char     vfs_root_path[HTPATHMAX]   =  { "/" };
#endif   /* HT_LOCALFS */

#ifdef VFS_STRIPPATH
/* preserve compatibility with old-style webport path */
char *   http_root_path =  &vfs_root_path[0];
#endif   /* VFS_STRIPPATH */

#ifdef HT_EXTDEV
/* list of alternate file IO routine sets */
struct vfroutines *  vfsystems   =  NULL;
#endif   /* HT_EXTDEV */


#ifdef HT_RWVFS

/* this counts the number of bytes that have been allocated for 
 * buffers for the read/write file system so that the above maximum 
 * can be enforced
 */
unsigned long vfs_total_rw_space;

/* this counts the number of dynamically allocated files in the file
   system so that the above maximum can be enforced */
unsigned long vfs_total_dyna_files;

/* this flag gets set during successful unlink operations so that
 * vfs_sync() knows to sync the file system even though none of the 
 * files in it show that they are stale
 */
int      vfs_dir_stale;

#endif   /* HT_RWVFS */


/* this counts the number of open files in the file system so that
   the above maximum can be enforced */
unsigned long vfs_open_files;

#ifdef VFS_STRIPPATH
char * strippath(char * name);
#endif   /* VFS_STRIPPATH */

VFILE * vfiles = NULL;  /* list of open files */

/* list of files */
struct vfs_file * vfsfiles =  NULL;

#ifdef VFS_UNIT_TEST

/* this flag determines whether the VFS logs the file names that are
   passed to it */
int      vfs_log_file_name =  0;

#endif   /* VFS_UNIT_TEST */

/* vfopen_error is used as a place to store errors that occur in vfopen() */
/* the porting engineer might want to replace this with a macro that
   defines vfopen to be errno, depending on his target system */
int      vfopen_error   =  0;



/* FUNCTION: set_vfopen_error()
 * 
 * this function sets the vfopen() error indication. it is called from
 * vfopen() when errors occur
 *
 * PARAM1: int error
 *
 * RETURNS: 
 */

void set_vfopen_error(int error)
{
   vfopen_error = error;
}



/* FUNCTION: get_vfopen_error()
 * 
 * this function returns the last error that occurred in a call to 
 * vfopen(). the porting engineer might want to define errno to be 
 * equal to this function, depending on his target system 
 *
 * PARAM1: 
 *
 * RETURNS: 
 */

int get_vfopen_error()
{
   return vfopen_error;
}



/* FUNCTION: vf_alloc_and_link_vop()
 * 
 * this function allocates a VFILE structure and links it to the
 * beginning of the linked list of open files
 *
 * PARAM1: 
 *
 * RETURNS: 
 */

VFILE * vf_alloc_and_link_vop()
{
   struct vfs_open * vop;

   /* enforce maximum number of simultaneously open files */
   if (vfs_open_files >= VFS_MAX_OPEN_FILES)
   {
#ifdef VFS_VERBOSE
      dprintf("vfs_open_files too big (%ld) in vf_alloc_and_link_vop()\n",
       vfs_open_files);
#endif   /* VFS_VERBOSE */
      return NULL;
   }

   /* allocate a structure to represent the open file */
   vop = VFS_VFS_OPEN_ALLOC();

   /* if the allocation succeeded */
   if (vop)
   {
      /* add to the beginning of the list of open files */
      vop->next = vfiles;
      vfiles = vop;
      /* increment the count of open files */
      vfs_open_files++;
   }
#ifdef VFS_VERBOSE
   else
      dprintf("VFS_VFS_OPEN_ALLOC() failed in vf_alloc_and_link_vop()\n");
#endif   /* VFS_VERBOSE */

   return vop;
}

#ifdef HT_RWVFS



/* FUNCTION: *vf_alloc_buffer()
 * 
 * PARAM1: unsigned long size
 *
 * RETURNS: 
 */

unsigned char * vf_alloc_buffer(unsigned long size)
{
   unsigned char *buffer;
#ifdef MUTE_WARNS
   unsigned int long_size,int_size;
#endif   /* MUTE_WARNS */

   /* make sure the requested allocation does not exceed the total
      memory space reserved for file buffers */
   if ((vfs_total_rw_space + size) > VFS_MAX_TOTAL_RW_SPACE)
      return NULL;

   /* the file sizes in the vfs_file structures are all unsigned 
    * longs, but npalloc() only can allocate sizes expressed in 
    * unsigned ints, so if your longs are bigger than your ints, you 
    * can't allocate memory 
    * any bigger than what will fit in an unsigned int 
    */
#ifdef MUTE_WARNS
   /* the idiotic hoops you got to jump through to suppress compiler
      warnings */
   long_size   =  sizeof(unsigned   long);
   int_size = sizeof(unsigned int);
   if (long_size > int_size)
#else
   if (sizeof(unsigned long) > sizeof (unsigned int))
#endif   /* MUTE_WARNS */
   {
      unsigned long mem_mask;

      /* what this does is create a mem_mask containing 0xffff0000 on 
       * most systems where this "if" expression will evaluate to 
       * true (2 byte ints, 4 byte longs). if any of those upper bits 
       * are on in your requested size, you otta luck.
       */
#ifdef MUTE_WARNS
      switch (int_size)
#else
      switch (sizeof(unsigned int))
#endif   /* MUTE_WARNS */
      {
      case 2 :
         mem_mask = 0xffff0000;
         break;
         default :
            dtrap();    /* you have a weird compiler */
         return NULL;
      }

      if (size & mem_mask)
         return NULL;
   }

   /* try to allocate a buffer of the requested size */
   buffer = (unsigned char *) npalloc((unsigned int) size);

   /* if the allocation succeeded */
   if (buffer)
   {
      /* add size to the count of total buffer space allocated */
      vfs_total_rw_space += size;
   }

   return buffer;
}



/* FUNCTION: vf_free_buffer()
 * 
 * PARAM1: unsigned char *buffer
 * PARAM2: unsigned long size
 *
 * RETURNS: 
 */

void vf_free_buffer(unsigned char * buffer, unsigned long size)
{
   /* free the buffer */
   if (buffer)
      npfree(buffer);

   /* and subtract its size from the total buffer space count */
   vfs_total_rw_space -= size;
}

#endif   /* HT_RWVFS */



/* FUNCTION: vfopen_locked()
 * 
 * PARAM1: char * name
 * PARAM2: char * mode
 *
 * RETURNS: 
 */

VFILE *
vfopen_locked(char * name, char * mode)
{
   struct vfs_file * vfp;
   struct vfs_open * vop;

   /* clear any previous vfopen() error */
   set_vfopen_error(0);

   /* the old code used to do special handling of '?' in files for 
    * the benefit of the web server. the web server should be doing 
    * this now. this is here to make sure that its doing it 
    */
   if (strchr(name,'?'))
   {
      dtrap();
      return NULL;
   }

   /* determine if the file exists */
   /* if the directory exists, vfp will point to its directory entry
      structure else vfp will be NULL */
   vfp = vfslookup_locked(name);

   /* if the file exists */
   if (vfp)
   {

#ifdef HT_RWVFS

      /* if mode begins with 'w' we will truncate to end of file */
      /* make sure the file is writable before proceeding */
      if ((*mode == 'w') && !(vfp->flags & VF_WRITE))
      {
         set_vfopen_error(ENP_FILEIO);
#ifdef VFS_VERBOSE
         dprintf("mode w with no VF_WRITE\n");
#endif   /* VFS_VERBOSE */
         return NULL;
      }

#endif   /* HT_RWVFS */

      /* allocate a VFILE structure to represent the open file */
      vop = vf_alloc_and_link_vop();

      /* check for failure */
      if (!vop)
      {
         set_vfopen_error(ENP_NOMEM);
#ifdef VFS_VERBOSE
         dprintf("vf_alloc_and_link_vop() failed 1\n");
#endif   /* VFS_VERBOSE */
         return NULL;
      }

      /* link to the file's directory entry structure */
      vop->file = vfp;

      /* by default start at the beginning of the file */
      /* note that vfp->data could be NULL at this point since empty
         files might have no data buffer allocated to them */
      vop->cmploc = vfp->data;   /* start at beginning of file */

#ifdef HT_RWVFS

      /* if mode begins with 'a', seek to end of file */
      if (*mode == 'a')
      {
         if (vfp->data)
         {
            vop->cmploc = vfp->data + vfp->comp_size;
         }
      }

      /* if mode begins with 'w', truncate to end of file */
      if (*mode == 'w')
      {
         /* set the size of the file before compression to 0 */
         vfp->real_size = 0;
         /* set the size of the compressed data to 0 */
         vfp->comp_size = 0;
         /* note we leave the pointer to the file buffer and its length
            alone since first writes will go to it */
         /* flag that the file has been modified */
         vfp->flags |= VF_STALE;

         /* turn off the compression flag */
         vfp->flags &= ~VF_HTMLCOMPRESSED;
      }

#endif   /* HT_RWVFS */

      return vop;
   }

#ifdef HT_EXTDEV

   /* if the mode implies that the file should be created if it
      does not exist */
   if (*mode != 'r')
   {
      /* see if one of the other systems wants to create this file */
      /* if none of the below devices can open the file, continue on */
      struct vfroutines *  vfs;

      for (vfs = vfsystems; vfs; vfs = vfs->next)
      {
         if ((vop = vfs->r_fopen(name, mode)) != NULL)
         {
            return vop;
         }
      }
   }

#endif   /* HT_EXTDEV */

#ifdef HT_LOCALFS
   /* compare the passed name to a vfs_root_path. if the name does
    * not begin with it, pass it to the native file 
    * system fopen()
    */
   if (strncmp(name, vfs_root_path, strlen(vfs_root_path)))
   {
      return (VFILE *) fopen(name, mode);
   }
#endif   /* HT_LOCALFS */

#ifdef HT_RWVFS

   /* if the mode implies that the file should be created if it
      does not exist */
   if (*mode != 'r')
   {
      /* enforce maximum number of files */
      if (vfs_total_dyna_files >= VFS_MAX_DYNA_FILES)
      {
         set_vfopen_error(ENP_NOMEM);
#ifdef VFS_VERBOSE
         dprintf("vf_total_dyna_files too big in vfopen_locked()\n");
#endif   /* VFS_VERBOSE */
         return NULL;
      }

      /* make sure the file name is not too long for the VFS */
      if (strlen(name) > FILENAMEMAX)
      {
         set_vfopen_error(ENP_PARAM);
#ifdef VFS_VERBOSE
         dprintf("file name too long in vfopen_locked()\n");
#endif   /* VFS_VERBOSE */
         return NULL;
      }

      /* allocate a vfs_file structure to hold the new file entry in */
      vfp = VFS_VFS_FILE_ALLOC();

      /* check for memory allocation failure */
      if (!vfp)
      {
         set_vfopen_error(ENP_NOMEM);
#ifdef VFS_VERBOSE
         dprintf("VFS_VFS_FILE_ALLOC() failed in vfopen_locked()\n");
#endif   /* VFS_VERBOSE */
         return NULL;
      }

      /* allocate a VFILE structure to represent the open file */
      vop = vf_alloc_and_link_vop();

      /* check for memory allocation failure */
      if (!vop)
      {
         VFS_VFS_FILE_FREE(vfp); /* free the allocated vfs_file entry */
         set_vfopen_error(ENP_NOMEM);
#ifdef VFS_VERBOSE
         dprintf("vf_alloc_and_link_vop() failed 2\n");
#endif   /* VFS_VERBOSE */
         return NULL;
      }

      /* add the vfs_file structure to the head of the list */

      vfp->next = vfsfiles;
      vfsfiles = vfp;

      /* increment count of total files */
      vfs_total_dyna_files++;

      /* remove leading directory separator before storing name */
      if (*name == '/' || *name == '\\')
         name++;

      /* store the converted name in the directory entry structure */
      strcpy(vfp->name,name);

      /* set the flags */
      vfp->flags = VF_DYNAMICINFO   /* the directory entry was allocated */
      | VF_WRITE        /* the file is writable */
      | VF_NONVOLATILE  /* the file is non-volatile */
      | VF_STALE;       /* the file is stale */

      /* the rest of the vfs_file structure fields remain with the 
       * zeroes that npalloc() is guaranteed to init its data with. 
       * note that this means the data pointer contains a null 
       * because we don't allocate any buffer to hold the data 
       * in until the first write 
       */
      /* link to the file's directory entry structure */
      vop->file = vfp;

      /* the cmploc and tag fields of the vop retain their NULLs from 
       * npalloc(). cmploc contains NULL because there is no data 
       * buffer to point to yet. tag contains NULL because no 
       * decompression operation has started yet
       */
      return vop;
   }

#endif   /* HT_RWVFS */

#ifdef HT_LOCALFS

   /* pass the open to the local file system */
   return (VFILE *) fopen(name,mode);

#else

   set_vfopen_error(ENP_NOFILE);
#ifdef VFS_VERBOSE
   dprintf("fell thru to end of vfopen_locked()\n");
#endif   /* VFS_VERBOSE */
   return NULL;

#endif   /* HT_LOCALFS */
}



/* FUNCTION: vfopen()
 * 
 * PARAM1: char * name
 * PARAM2: char * mode
 *
 * RETURNS: 
 */

VFILE *
vfopen(char * name, char * mode)
{
   VFILE *vfd;

#ifdef VFS_UNIT_TEST
   if (vfs_log_file_name)
      dprintf("vfopen() passed >%s<,%s\n",name,mode);
#endif   /* VFS_UNIT_TEST */

   /* lock the VFS */
   vfs_lock();

   vfd = vfopen_locked(name,mode);

   vfs_unlock();

   return vfd;
}



/* FUNCTION: vfclose_locked()
 * 
 * PARAM1: VFILE * vfd
 *
 * RETURNS: 
 */

void 
vfclose_locked(VFILE * vfd)
{
   VFILE * vtmp;
   VFILE * vlast;

   vlast = NULL;

   /* see if vfd is in our list of open virtual files. We
      can't use isvfile() since we need a pointer to last. */
   vtmp = vfiles;
   while (vtmp)
   {
      /* if this is the one we are looking for, exist search loop */
      if (vfd == vtmp)
         break;

      /* bump the next and previous pointers along to try the next one */
      vlast = vtmp;
      vtmp = vtmp->next;
   }

   /* if the passed in handle was not in the list we maintain */
   if (vfd != vtmp)
   {

#ifdef HT_LOCALFS
      /* default to call on local system */
      fclose((FILE*)vfd);
#endif   /* HT_LOCALFS */
      return;
   }

   /* the passed in handle is in the list we maintain */

   /* this not really a forever loop. it exists so we can break easily
      and deal with all the ifdefs */
   while (1)
   {
      struct vfs_file * vfp   =  vfd->file;
#ifdef HT_RWVFS
      unsigned char *new_buffer;

      /* vfd->file will be null if somebody unlinked the file after
       * this handle was created to point to it. if the file itself 
       * is gone there is nothing left to do, so break to list 
       * deletion code at bottom of loop
       */
      if (vfp == NULL)
         break;
#endif   /* HT_RWVFS */

#ifdef HT_EXTDEV
      /* if the file was created by an external file system */
      if (vfp->method)
      {
         /* call that file system's fclose() */
         struct vfroutines *  vfs   =  (struct  vfroutines*)(vfp->method);

         vfs->r_fclose(vfd);
         break;   /* break to list deletion code after end of phoney loop */
      }
#endif   /* HT_EXTDEV */

#ifdef HT_RWVFS

      /* if the buffer containing the data was allocated dynamically, 
       * and there are VFS_CLOSE_FRAG_FLOOR bytes of unused data 
       * between the end of the file and the end of the buffer 
       */
      if ((vfp->flags & VF_DYNAMICDATA) &&
          ((vfp->buf_size - vfp->comp_size) > VFS_CLOSE_FRAG_FLOOR) &&
          vfp->data) /* this last test is a sanity check */
      {
         /* try to reclaim the unused data */

         /* allocate a new buffer just big enough for the data */
         new_buffer = vf_alloc_buffer(vfp->comp_size);

         /* if the allocation worked */
         if (new_buffer)
         {
            /* copy the old buffer to the new one */
            MEMCPY(new_buffer,vfp->data,(unsigned int) (vfp->comp_size));
            /* free the old buffer */
            vf_free_buffer(vfp->data,vfp->buf_size);
            /* update the buffer pointer and size to reflect the
               just big enough buffer */
            vfp->data = new_buffer;
            vfp->buf_size = vfp->comp_size;
         }
      }

#ifdef VFS_AUTO_SYNC
      /* if the file's flags indicate that the file is non-volatile
         and that it has been changed */
      if ((vfp->flags & (VF_NONVOLATILE | VF_STALE)) ==
          (VF_NONVOLATILE | VF_STALE))
      {
         /* store the non-volatile files to whatever backing store 
          * the system provides. note that it is up to the vfs_sync() 
          * function to clear the VF_STALE bits in whatever files it 
          * successfully stores. this way if a given attempt fails, 
          * the unsuccessfully saved files get another chance on the 
          * next call to vfs_sync(). note also that the sync function 
          * is passed the pointer to the vfs_file so that if all it 
          * does is sync the single file, then only that 
          * file's VF_STALE flag can be cleared. 
          */
         vfs_sync(vfp);
      }
#endif   /* VFS_AUTO_SYNC */

#endif   /* HT_RWVFS */

      /* break to list deletion code below */
      break;

   }

   if (vlast)  /* unlink from list of open files */
      vlast->next = vtmp->next;
   else
      vfiles = vtmp->next;

   /* free structure addressed by open handle */
   VFS_VFS_OPEN_FREE(vtmp);
   /* decrement the number of open files */
   vfs_open_files--;
   return;
}



/* FUNCTION: vfclose()
 * 
 * PARAM1: VFILE *vfd
 *
 * RETURNS: 
 */

void vfclose(VFILE * vfd)
{
   vfs_lock();

   vfclose_locked(vfd);

   vfs_unlock();
}

/* FUNCTION: vfflush()
 * 
 * PARAM1: VFILE *vfd
 *
 * RETURNS: 
 */

int vfflush(VFILE * vfd)
{
   vfs_lock();

   printf("vfflush(): This function needs to be implemented\n");

   vfs_unlock();
   return(0);
}

char * vfgets(char * s, int lim, VFILE * fp) 
{
   int   c   = EOF;
   char* ret = s;

   while ( --lim > 0 && (c = vgetc(fp)) != EOF)
      if (( *ret ++ = c) == '\n')
         break;
   *ret = '\0';
   return ( c == EOF && ret == s) ? NULL : s;
}
 
/* FUNCTION: vfeof()
 * 
 * PARAM1: VFILE *vfd
 *
 * RETURNS: 1 if the file pointer is at EOF, otherwise 0
 */
 
int vfeof(VFILE * vfd)
{
   int c = 0;

#ifdef HT_LOCALFS
   return(feof((FILE*)vfd));
#endif
   if ((c = vgetc(vfd)) == EOF )
   {
      return 1;
   }   
   return 0;
}

#ifdef HT_RWVFS



/* FUNCTION: vunlink_flag_open_files()
 * 
 * fix up references to deleted file in list of currently 
 * open VFILEs 
 *
 * PARAM1: struct vfs_file *vfp
 *
 * RETURNS: 
 */

void vunlink_flag_open_files(struct vfs_file * vfp)
{
   VFILE * vtmp;

   /* for all open files */
   for (vtmp = vfiles; vtmp; vtmp = vtmp->next)
   {
      /* if the open file handle is referencing the file we are
         deleting, set that reference to NULL */
      if (vtmp->file == vfp)
         vtmp->file = NULL;
   }
}

#endif   /* HT_RWVFS */



/* FUNCTION: vunlink()
 * 
 * PARAM1: char *name
 *
 * RETURNS: 
 */

/*
 * Altera Niche Stack Nios port modification:
 * Change prototype from char * name to const char to
 * follow C library standard.
 */
int
vunlink(const char * const_name)
{
   struct vfs_file * vfp;
   int   rc =  0;
#ifdef VFS_AUTO_SYNC
   int   do_sync  =  0;
#endif
#ifdef   HT_RWVFS
   struct vfs_file * vtmp;
   struct vfs_file * vflast;
   struct vfs_file * vfnext;
#endif

/*
 * Altera Niche Stack Nios port modification:
 * Change prototype from char * name to const char to
 * follow C library standard.
 */
   char * name = malloc(strlen(const_name)+1);
   strcpy(name, const_name);

#ifdef VFS_UNIT_TEST
   if (vfs_log_file_name)
      dprintf("vunlink() passed >%s<\n",name);
#endif   /* VFS_UNIT_TEST */

   /* lock the VFS */
   vfs_lock();

   /* see if the converted name is one of the one's in our list */
   /* if it isn't */
   if ((vfp = vfslookup_locked(name)) == NULL)
   {
      vfs_unlock();

#ifdef HT_LOCALFS
      /* default to call on local system */
      return remove(name);
#else
      /* no local file system, so return error condition */
      free(name);
      return -1;
#endif   /* HT_LOCALFS */
   }

#ifdef   HT_RWVFS
   /* save the next link pointer since in one path through the code, 
    * the vfs_file structure gets freed before its unlinked from the 
    * list
    */
   vfnext = vfp->next;

   /* search list of files to determine predecessor in list */
   vflast = NULL;
   for (vtmp = vfsfiles; vtmp != NULL; vtmp = vtmp->next)
   {
      if (vtmp == vfp)
         break;
      vflast = vtmp;
   }

   /* this shouldn't happen since vfslookup_locked() already searched
      the list, but just in case */
   if (vtmp == NULL)
   {
      dtrap();
      vfs_unlock();
      free(name);
      return -1;
   }

   /* this is not really a forever loop. it exists so we can break easily
      and deal with all the ifdefs */
   while (1)
   {

#ifdef HT_EXTDEV
      /* if the file was created by an external file system */
      if (vfp->method)
      {
         /* call that file system's unlink() */
         struct vfroutines *  vfs   =  (struct  vfroutines*)   (vfp->method);

         rc = vfs->r_unlink(name);
         break;   /* break to list deletion code after end of phoney loop */
      }
#endif   /* HT_EXTDEV */

      /* if the file is not write enabled, return error condition */
      if (!(vfp->flags & VF_WRITE))
      {
         vfs_unlock();
         free(name);
         return -1;
      }

      /* if the data buffer containing the file's data was dynamically
         allocated and is not null */
      if ((vfp->flags & VF_DYNAMICDATA) && (vfp->data))
      {
         /* free the buffer */
         vf_free_buffer(vfp->data,vfp->buf_size);
      }

      /* if the vfs_file structure itself was allocated dynamically */
      if (vfp->flags & VF_DYNAMICINFO)
      {
         /* decrement count of total files */
         vfs_total_dyna_files--;

         /* free the vfs_file structure */
         VFS_VFS_FILE_FREE(vfp);
      }

      /* since we modified the list we maintain, do a sync, but after
         the vfs_file has been deleted from the list */
#ifdef VFS_AUTO_SYNC
      do_sync = 1;
#endif
      /* we were successful at our unlink */
      rc = 0;

      break;
   }

   /* delete the vfs_file structure from the list headed by vfsfiles */
   if (vflast)
      vflast->next = vfnext;
   else
      vfsfiles = vfnext;

   /* fix up references to deleted file in list of currently open VFILEs */
   vunlink_flag_open_files(vfp);

   /* flag that the directory is stale so vfs_sync() knows it has to do
      something */
   vfs_dir_stale = TRUE;

#ifdef VFS_AUTO_SYNC
   /* if we need to sync the backing store, call the function to do it */
   if (do_sync)
   {
      /* a NULL passed to vfs_sync() means that only the directory
         structure itself was changed */
      vfs_sync(NULL);
   }
#endif   /* VFS_AUTO_SYNC */

#else    /* HT_RWVFS */

   /* unlinks not allowed on read-only VFS */
   rc = -1;

#endif   /* HT_RWVFS */
   vfs_unlock();
   free(name);
   return rc;
}



/* FUNCTION: vfread()
 * 
 * vfread() - fread for vfs. if vfd parm is a vfs handle, this gets 
 * data from the virtual file system, doing uncompression on the fly. 
 * If vfd is not in vfs, this can be ifdefed to fall to a native 
 * fread() call. Parameters and returns are identical to fread(); 
 *
 * PARAM1: char * buf
 * PARAM2: unsigned size
 * PARAM3: unsigned items
 * PARAM4: VFILE * vfd
 *
 * RETURNS: 
 */

int 
vfread(char * buf, unsigned size, unsigned items, VFILE * vfd)
{
   unsigned bcount;  /* number of bytes put in caller's buffer */

   IN_PROFILER(PF_FS, PF_ENTRY);

   /* lock the VFS */
   vfs_lock();

   /* if the file is in our list of open files */
   if (isvfile_locked(vfd))
   {

#ifdef HT_RWVFS
      /* the caller is trying to read a handle to a file that's been
         deleted, so he gets 0 data back */
      if (vfd->file == NULL)
      {
         vfs_unlock();
         IN_PROFILER(PF_FS, PF_EXIT);
         return 0;
      }
#endif   /* HT_RWVFS */

#ifdef HT_EXTDEV
      /* if the file was created by an external file system */
      if (vfd->file->method)
      {
         struct vfroutines *  vfs   =  (struct  vfroutines*)   (vfd->file->method);
         int   rc;

         /* call that system's fread() */
         rc = vfs->r_fread(buf,size,items,vfd);
         vfs_unlock();
         IN_PROFILER(PF_FS, PF_EXIT);
         return rc;
      }
#endif   /* HT_EXTDEV */

#ifdef HT_RWVFS
      /* the data pointer can be NULL if somebody tries to read from 
       * a freshly created file, in which case there is no data in 
       * the file, so return 0
       */
      if (!(vfd->file->data))
      {
         vfs_unlock();
         IN_PROFILER(PF_FS, PF_EXIT);
         return 0;
      }
#endif   /* HT_RWVFS */

      bcount = items * size;  /* number of bytes to transfer */

#ifdef HTML_COMPRESSION
      if (vfd->file->flags & VF_HTMLCOMPRESSED)
      {
         int   chr;  /* next char to process */

         while (bcount)
         {
            chr = vgetc_locked(vfd);
            if (chr == EOF)
            {
               vfs_unlock();
               IN_PROFILER(PF_FS, PF_EXIT);
               return(((items*size)-bcount)/size);
            }

            *buf++ = (char)chr;
            bcount--;
         }
         vfs_unlock();
         IN_PROFILER(PF_FS, PF_EXIT);
         return(items); /* filled user buffer, return # items copied */
      }
      else  /* else fall to faster non-compression code */
#endif   /* HTML_COMPRESSION */
      {  /* get here to do simple uncompressed data read */
         unsigned location = vfd->cmploc - vfd->file->data;

         if (((unsigned long)location + bcount) > vfd->file->comp_size)
            bcount = (unsigned)(vfd->file->comp_size - location);
         MEMCPY(buf, vfd->cmploc, bcount);
         vfd->cmploc += bcount;
      }
      vfs_unlock();
      IN_PROFILER(PF_FS, PF_EXIT);
      return (bcount/size);
   }

   vfs_unlock();
   IN_PROFILER(PF_FS, PF_EXIT);

#ifdef HT_LOCALFS
   /* default to call on local system */
   return(fread(buf, size, items, (FILE*)vfd));
#else
   return 0;
#endif   /* HT_LOCALFS */
}

/* VFS_WRITE_BUF_INC defines how much the size of the buffer used to
   store file system data is increased at a time */
#define  VFS_WRITE_BUF_INC 8192
#define VFS_ROUND_UP(x) \
   (((((x) - 1) / VFS_WRITE_BUF_INC) + 1) * VFS_WRITE_BUF_INC)



/* FUNCTION: vfwrite_locked()
 * 
 * PARAM1: char * buf
 * PARAM2: unsigned size
 * PARAM3: unsigned items
 * PARAM4: VFILE *vfd
 *
 * RETURNS: 
 */

int 
vfwrite_locked(char * buf, unsigned size, unsigned items, VFILE * vfd)
{
#ifndef HT_RWVFS
#ifndef HT_LOCALFS
#ifndef HT_EXTDEV

   /* if the file is in our list of files */
   if (isvfile_locked(vfd))
   {
      vfd->error = ENP_LOGIC;
      return 0;
   }

   /* I know this is ugly but its the simplist way I can figure to 
    * suppress those annoying "parameter not used" compiler warnings. 
    * its a damned shame given that it results in the 
    * generation of superfluous code
    */
   USE_ARG(buf);
   USE_ARG(size);
   USE_ARG(items);

   return EBADF;

#endif   /* HT_EXTDEV */
#endif   /* HT_LOCALFS */
#endif   /* HT_RWVFS */

#ifdef HT_RWVFS
   unsigned int bcount; /* number of bytes to write */
   unsigned long current_offset;
   struct vfs_file * vfp;
#endif   /* HT_RWVFS */

   /* if the file is not in our list of files */
   if (!isvfile_locked(vfd))
   {
#ifdef HT_LOCALFS
      /* default to call on local system */
      return(fwrite(buf, size, items, (FILE*)vfd));
#else
      return EBADF;
#endif   /* HT_LOCALFS */

   }
   /* this file is in our list of files */

#ifdef HT_EXTDEV
   /* if the file was created by an external file system */
   if (vfd->file->method)
   {
      struct vfroutines *  vfs   =  (struct  vfroutines*)   (vfd->file->method);
      int   rc;

      /* call that system's fwrite() */
      rc = vfs->r_fwrite(buf,size,items,vfd);
      return rc;
   }
#endif

   /* if we don't support writes, set an error */
#ifndef HT_RWVFS

   vfd->error = ENP_LOGIC;
   return 0;

#else    /* HT_RWVFS */

   /* the caller is trying to write to a file that's been deleted,
      so he writes 0 data */
   if (vfd->file == NULL)
   {
      return 0;
   }

   vfp = vfd->file;

   /* if the file is not writable, return error condition */
   if (!(vfp->flags & VF_WRITE))
   {
      vfd->error = ENP_FILEIO;
      return 0;
   }

   /* compute number of bytes to write */
   bcount = size * items;

   /* get rid of this degenerate case up front */
   if (bcount == 0)
   {
      return 0;
   }

   /* if the file currently has no data buffer */
   if (vfp->data == NULL)
   {
      /* compute the size of the buffer to be created */
      /* we round up the size of the data to be written so we dont have
         the overhead of a memory allocation on every write */
      unsigned long buf_size = VFS_ROUND_UP((unsigned long)bcount);

      /* allocate a buffer of that size */
      vfp->data = vf_alloc_buffer(buf_size);

      /* if the allocation failed */
      if (!(vfp->data))
      {
         vfd->error = ENP_NOMEM;
         return 0;
      }

      /* store the size of the allocated buffer */
      vfp->buf_size = buf_size;

      /* store the number of bytes written in the real and compressed
         file sizes */
      vfp->real_size = bcount;
      vfp->comp_size = bcount;

      /* set the file pointer to the first byte following the last
         byte written */
      vfd->cmploc = vfp->data + bcount;

      /* turn off the compression flag */
      vfp->flags &= ~VF_HTMLCOMPRESSED;

      /* the data in the file is stale */
      vfp->flags |= VF_STALE;

      /* the buffer data was dynamically allocated */
      vfp->flags |= VF_DYNAMICDATA;

      /* copy the data to be written to the file buffer */
      MEMCPY(vfp->data,buf,bcount);

      /* return the number of "items" written */
      return items;
   }

   /* compute the current offset into the file */
   current_offset = vfd->cmploc - vfp->data;

   /* if the data to be written wont fit into the file buffer */
   if ((current_offset + bcount) > vfp->buf_size)
   {
      /* compute the size of a new buffer to hold the data */
      unsigned long new_buf_size = VFS_ROUND_UP(current_offset + bcount);
      unsigned char *new_buffer;

      /* allocate a new buffer */
      new_buffer = vf_alloc_buffer(new_buf_size);

      /* check for allocation failure */
      if (!new_buffer)
      {
         vfd->error = ENP_NOMEM;
         return 0;
      }

      /* copy the old buffer contents to the new buffer */
      MEMCPY(new_buffer,vfp->data,(unsigned int) (vfp->comp_size));

      /* if the old buffer had been dynamically allocated */
      if (vfp->flags & VF_DYNAMICDATA)
      {
         /* free it */
         vf_free_buffer(vfp->data,vfp->buf_size);
      }

      /* store the new buffer in the file structure */
      vfp->data = new_buffer;

      /* store the new buffer size in the file structure */
      vfp->buf_size = new_buf_size;

      /* the new buffer was dynamically allocated */
      vfp->flags |= VF_DYNAMICDATA;

      /* set the current file pointer to point into the new data 
       * buffer note that this means the VFS does not support 
       * simultaneous opens of the same file, since for that to work, 
       * all the other cmploc's that point to this file would 
       * have to be updated also 
       */
      vfd->cmploc = vfp->data + current_offset;
   }

   /* copy the data to the current file pointer */
   MEMCPY(vfd->cmploc,buf,bcount);

   /* update the current file pointer */
   vfd->cmploc += bcount;

   /* if the resulting current offset is greater than the file size */
   if (current_offset + bcount > vfp->comp_size)
   {
      /* update the "compressed" file size */
      vfp->comp_size = current_offset + bcount;
   }

   /* since we turn off the compression bit below, the "real" size and
      the compressed size must be the same */
   vfp->real_size = vfp->comp_size;

   /* turn off the compression flag */
   vfp->flags &= ~VF_HTMLCOMPRESSED;

   /* the data in the file is stale */
   vfp->flags |= VF_STALE;

   /* return the number of "items" written */
   return items;

#endif   /* HT_RWVFS */
}



/* FUNCTION: vfwrite()
 * 
 * vfwrite() - This is included to support integration with native 
 * file systems. Parameters and returns are identical to fwrite(); 
 * except that on some error conditions EBADF is returned instead of 
 * 0 
 *
 * PARAM1: char * buf
 * PARAM2: unsigned size
 * PARAM3: unsigned items
 * PARAM4: VFILE *vfd
 *
 * RETURNS: 
 */

int
vfwrite(char * buf, unsigned size, unsigned items, VFILE * vfd)
{
   int   rc;

   IN_PROFILER(PF_FS, PF_ENTRY);

   /* lock the VFS */
   vfs_lock();

   /* do the write */
   rc = vfwrite_locked(buf, size, items, vfd);

   /* unlock the VFS */
   vfs_unlock();

   IN_PROFILER(PF_FS, PF_EXIT);

   return rc;
}




/* FUNCTION: vfseek()
 * 
 * vfseek : Returns 0 on success. Otherwise returns a nonzero value.
 *
 * PARAM1: VFILE * vfd
 * PARAM2: long offset
 * PARAM3: int mode
 *
 * RETURNS: 
 */

int 
vfseek(VFILE * vfd, long offset, int mode)
{
   /* lock the VFS */
   vfs_lock();

   if (isvfile_locked(vfd))
   {
#ifdef HT_RWVFS
      /* the caller is trying to seek a file that's been deleted,
         so return an error indication */
      if (vfd->file == NULL)
      {
         vfs_unlock();
         return -1;
      }
#endif   /* HT_RWVFS */

#ifdef HT_EXTDEV
      if (vfd->file->method)
      {
         struct vfroutines *vfp = (struct vfroutines*)(vfd->file->method);
         int   rc;

         rc = vfp->r_fseek(vfd, offset, mode);
         vfs_unlock();
         return rc;
      }
#endif   /* HT_EXTDEV */
      /* this vfseek() currently only supports seek to exact
         end or begining of file */
      switch (mode)
      {
      case SEEK_SET:
         vfd->cmploc = vfd->file->data + offset;
         break;
      case SEEK_CUR:
         /* If the file is compressed, then the following
          * adjustment is inaccurate. Currently we don't have
          * any scenario where this happens. - handle later */
         vfd->cmploc += offset; 
         break;
      case SEEK_END:
         vfd->cmploc = vfd->file->data + vfd->file->comp_size + offset;
         break;
      }
      vfs_unlock();
      return(0);
   }

   vfs_unlock();

#ifdef HT_LOCALFS
   /* default to call on local system */
   return(fseek((FILE*)vfd, offset, mode));
#else
   return -1;
#endif   /* HT_LOCALFS */
}



/* FUNCTION: vftell()
 * 
 * PARAM1: VFILE * vfd
 *
 * RETURNS: 
 */

long
vftell(VFILE * vfd)
{
   /* lock the VFS */
   vfs_lock();

   if (isvfile_locked(vfd))
   {
#ifdef HT_RWVFS
      /* the caller is trying to ftell a deleted file,
         so return an error condition */
      if (vfd->file == NULL)
      {
         vfs_unlock();
         return -1;
      }
#endif   /* HT_RWVFS */

#ifdef HT_EXTDEV
      if (vfd->file->method)
      {
         struct vfroutines *  vfp   =  (struct  vfroutines*)   (vfd->file->method);
         long  rc;

         rc = vfp->r_ftell(vfd);
         vfs_unlock();
         return rc;
      }
#endif   /* HT_EXTDEV */
      /* if file has been fseeked to end, return uncompressed size.
         else return current location in compression stream */
      if (vfd->cmploc == vfd->file->data + vfd->file->comp_size)
      {
         vfs_unlock();

         return vfd->file->comp_size;
      }
      else
      {
         vfs_unlock();

         return (vfd->cmploc - vfd->file->data);
      }
   }

   vfs_unlock();

#ifdef HT_LOCALFS
   /* default to call on local system */
   return(ftell((FILE*)vfd));
#else
   return EBADF;
#endif   /* HT_LOCALFS */
}



/* FUNCTION: vgetc_locked()
 * 
 * PARAM1: VFILE * vfd
 *
 * RETURNS: 
 */

int
vgetc_locked(VFILE * vfd)
{
   int   chr;

   if (isvfile_locked(vfd))
   {
#ifdef HT_RWVFS
      /* the caller is trying to read a file that's been deleted,
         so return an error condition */
      if (vfd->file == NULL)
      {
         return EOF;
      }
#endif   /* HT_RWVFS */

#ifdef HT_EXTDEV
      if (vfd->file->method)
      {
         struct vfroutines *  vfp   =  (struct  vfroutines*)   (vfd->file->method);
         return (vfp->r_fgetc(vfd));
      }
#endif   /* HT_EXTDEV */

#ifdef HT_RWVFS
      /* a freshly created file might not have a data buffer associated
         with it yet */
      if (vfd->file->data == NULL)
         return EOF;

#endif   /* HT_RWVFS */

#ifdef HTML_COMPRESSION
      if (vfd->file->flags & VF_HTMLCOMPRESSED)
      {
         if (vfd->tag)  /* last read ended inside a tag? */
         {
            chr = *vfd->tag++;
            if (chr == '\0')  /* at end of tag? */
            {
               vfd->tag = NULL;  /* clear tag flag */
               if (vfd->cmploc >= (vfd->file->data + vfd->file->comp_size))
                  return EOF;
               else
                  chr = *vfd->cmploc++;   /* get next char from file */
            }
            else  /* got valid char from inside tag */
               return (chr);
         }
         else  /* not in a tag; get next char in file */
         {
            if (vfd->cmploc >= (vfd->file->data + vfd->file->comp_size))
               return EOF;
            else
               chr = *vfd->cmploc++;
         }
#ifdef FOREIGN_LANG_SUPPORT
         if (chr == NP_ESCAPE_CHAR )      /* escape char ? */
         {
            /* If the current char is an ESCAPE char, then
             * we are interested in the next char 
             * Then next char has HIGH-BIT-SET, and it is
             * not a HTML TAG */
            chr = *vfd->cmploc++;
         }
         else
#endif
         if (chr & 0x80)   /* compressed HTML tag? */
         {
            /* find tag and insert it as source */
            vfd->tag = (u_char*) htmltags[chr & 0x7f];
            chr = *vfd->tag++;
         }
      }
      else  /* HTML compression flag not set */
      {
#endif   /* HTML_COMPRESSION */
         /* Check to see if read has advanced to end of file */
         if (vfd->cmploc >= (vfd->file->data + vfd->file->comp_size))
            chr = EOF;
         else  /* else just get next char to return */
            chr = *(vfd->cmploc++);
#ifdef HTML_COMPRESSION
      }  /* need to close brace form if...else; */
#endif   /* HTML_COMPRESSION */

      return chr;
   }

#ifdef HT_LOCALFS
   /* default to call on local system */
   return(getc((FILE*)vfd));
#else
   dtrap(); /* can this happen? */
   return EOF;
#endif   /* HT_LOCALFS */
}



/* FUNCTION: vgetc()
 * 
 * PARAM1: VFILE *vfd
 *
 * RETURNS: 
 */

int
vgetc(VFILE * vfd)
{
   int   rc;

   /* lock the VFS */
   vfs_lock();

   /* get the character */
   rc = vgetc_locked(vfd);

   /* unlock the VFS */
   vfs_unlock();
   return rc;
}




/* FUNCTION: vfslookup_locked()
 * 
 * vfn_lookup() - lookup a vfs_file entry by name. Pass file name may 
 * have path prepended and/or cgi parameters appended. Return pointer 
 * to vfs_file struct if it exists, else NULL. 
 * 
 * vfslookup_locked() should be called by functions internal to the
 * VFS that have already locked the VFS
 *
 * PARAM1: char * name
 *
 * RETURNS: 
 */

struct vfs_file * 
vfslookup_locked(char * name)
{
   struct vfs_file * vp;

#ifdef VFS_STRIPPATH
   char *   cp;

   /* If root path is prepended to name, skip past it */
   if (*name == '/' || *name == '\\')
   {
      cp = strippath(name);

      if (!cp) /* strippath coundn't match our path */
      {
         /* Files like "/hub47.gif" need to be taken care of */
         if (*name == '/' || *name == '\\')
            name++;
      }
      else
         name = cp ;
   }
#endif   /* VFS_STRIPPATH */

   /* see if there is a question mark in the file name */
   if (strchr(name,'?'))
   {
      dtrap(); /* is this still allowed? */
      return NULL;
   }

   for (vp = vfsfiles; vp; vp = vp->next)    /* search vfs list for name */
   {
      if (strcmp(name, vp->name) == 0)
         return vp;
   }

   return NULL;   /* fall to here if not found in for loop */
}



/* FUNCTION: vfslookup()
 * 
 * vfslookup() should be called by code that is external to the VFS
 * (like the web server) that has not locked the VFS
 *
 * PARAM1: char * name
 *
 * RETURNS: 
 */

struct vfs_file * 
vfslookup(char * name) 
{
   struct vfs_file * vp;

#ifdef VFS_UNIT_TEST
   if (vfs_log_file_name)
      dprintf("vfslookup() passed >%s<\n",name);
#endif   /* VFS_UNIT_TEST */

   /* lock the VFS */
   vfs_lock();

   /* do the lookup */
   vp = vfslookup_locked(name);

   /* unlock the VFS */
   vfs_unlock();
   return vp;
}

#ifdef VFS_STRIPPATH



/* FUNCTION: strippath()
 * 
 * strippath() - Compares the path prepended to a passed file 
 * path/name to the http_root_path variable. If the passed file 
 * path/name matches http_root_path, it strips http_root_path from 
 * the path/name and returns the remaining path/name portion for 
 * lookup in the cfs table. 
 *
 * PARAM1: char * name
 *
 * RETURNS: Returns NULL if passed path/name does NOT 
 * have the http_root_path prepended; or on any error. 
 */

char * 
strippath(char * name)
{
   char *   path; /* pointer into system path */
   char *   ptmp; /* another pointer into path */
   char *   ntmp; /* pointer into name text */
   int   dirlen;

   ntmp = uslash(name);      /* uslash() is defined in misclib\in_utils.c */
   path = http_root_path; /* The servers root path, at least one UNIX slash */
   while (*path && *ntmp)
   {
      while (*path == '/') path++;   /* strip leading slash */
         if (*path == 0)
         break;
      /* find number of chars in this directory layer's name */
      ptmp = strchr(path, '/');   /* location of next slash in path */
      if (ptmp)
         dirlen = ptmp - path;
      else
         dirlen = strlen(path);

      while (*ntmp == '/') ntmp++;   /* strip leading slash */
         if (strncmp(ntmp, path, dirlen) == 0)
      {
         path += dirlen;
         ntmp += dirlen;
      }
      else
         return NULL;   /* didn't match */
   }
   if (*path == '\0')
   {
      while (*ntmp == '\\' || *ntmp == '/')
         ntmp++;
      return ntmp;
   }
   else
      return NULL;
}

#endif   /* VFS_STRIPPATH */



/* FUNCTION: isvfile_locked()
 * 
 * isvfile_locked() should be called by functions internal to the VFS
 * that have already locked it
 *
 * PARAM1: VFILE *vfp
 *
 * RETURNS: 
 */

int
isvfile_locked(VFILE * vfp)
{
   VFILE * vtmp;

   for (vtmp = vfiles; vtmp; vtmp = vtmp->next)
      if (vtmp == vfp)
      return TRUE;

   return FALSE;  /* passed pointer not found in list */
}



/* FUNCTION: isvfile()
 * 
 * isvfile() should be called by code that is external to the VFS
 * (like the web server) that has not locked the VFS
 *
 * PARAM1: VFILE *vfp
 *
 * RETURNS: 
 */

int
isvfile(VFILE * vfp)
{
   int   rc;

   /* lock the VFS */
   vfs_lock();

   /* do the lookup */
   rc = isvfile_locked(vfp);

   /* unlock the VFS */
   vfs_unlock();
   return rc;
}



/* FUNCTION: vferror()
 * 
 * vferror() - get last error (if any) for passed file. The type of 
 * error will vary with implementation, however 0 should always be 
 * "no error". 
 *
 * PARAM1: VFILE * vfd
 *
 * RETURNS: 
 */

int
vferror(VFILE * vfd)
{
   /* lock the VFS */
   vfs_lock();

   if (isvfile_locked(vfd))
   {
      vfs_unlock();

      return vfd->error;
   }

   vfs_unlock();

#ifdef HT_LOCALFS
   return(ferror((FILE*)vfd));
#else /* not a VFILE, and no local FS */
   return -1;  /* should this be an error? */
#endif   /* HT_LOCALFS */
}



/* FUNCTION: vclearerr()
 * 
 * PARAM1: VFILE *vfd
 *
 * RETURNS: 
 */

void vclearerr(VFILE * vfd)
{
   /* lock the VFS */
   vfs_lock();

   if (isvfile_locked(vfd))
   {
      vfs_unlock();

      vfd->error = 0;
      return;
   }

   vfs_unlock();

#ifdef HT_LOCALFS
   clearerr((FILE *) vfd);
#endif   /* HT_LOCALFS */
}

#endif   /* VFS_FILES */


