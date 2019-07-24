/*
 * FILENAME: vfssync.c
 *
 * Copyright 1999- 2000 By InterNiche Technologies Inc. All rights reserved
 *
 *
 * MODULE: VFS
 *
 * ROUTINES: vfs_sync(), vfs_restore()
 *
 * PORTABLE: yes
 */

/* target system independent implementation of vfs_sync() and 
 * vfs_restore() 
 */
#include "ipport.h"
#include "vfsfiles.h"

#if defined(HT_SYNCDEV) && defined(HT_RWVFS)

#define  VFS_BACKING_STORE_TAG "VFS"

/* these functions store and retrieve a VFS to and from a backing store */

/* the format of the binary backing store file is shown below.
 * all numeric fields are in local endian format.
 *
 * <tag> - the 4 byte null terminated string "VFS"
 * <dir len> - 4 byte unsigned long contain the number of entries in
 *             the linked list of vfs_file structures.
 * the remaining entries are repeated <dir len> times
 * <file name len> - 2 byte unsigned short containing the length of
 *             the following file name.
 * <file name> - <file name len> bytes that constitute the file name.
 * <flags> - 2 byte unsigned short containing the vfs_file struct
 *           flags field.
 * <real size> - 4 byte unsigned long containing the vfs_file struct
 *           real_size field (file size before compression).
 * <comp size> - 4 byte unsigned long containing the vfs_file struct
 *           comp_size field (size of actual file)
 * <file data> - <comp size> bytes of binary data that constitute the
 *           file's contents (what vfs_file.data will point to)
 */


/* FUNCTION: vfs_sync()
 * 
 * PARAM1: struct vfs_file *vfp
 *
 * RETURNS: 
 */

void vfs_sync(struct vfs_file * vfp)
{
   void *   fd;
   unsigned long file_count = 0;    /* # of files we are going to sync */
   unsigned long stale_count = 0;   /* # of files that we found stale */
   unsigned short file_name_len; /* working file name length */
   unsigned long bytes_written;
   unsigned int bytes_to_write;
   int   rc;

   /* count the numbers of non-volatile and stale files */
   /* I know we are using a function parameter is a working pointer 
    * here, and that would normally be a sleezy thing to do, but this 
    * implementation of vfs_sync() doesn't use its parameter as such 
    * and doing this is an easy way to avoid the argument not 
    * used compiler warnings 
    */
   for (vfp = vfsfiles; vfp; vfp = vfp->next)
   {
      /* skip files that aren't non-volatile */
      if (!(vfp->flags & VF_NONVOLATILE))
         continue;

#ifdef HT_EXTDEV
      /* if the file is handled by an external file system, its up to 
       * it to deal with the file, so skip files 
       * that are handled by an external fiel system
       */
      if (vfp->method)
         continue;
#endif   /* HT_EXTDEV */

      /* we are going to sync all non-volatile files */
      file_count++;
      /* count the non-volatile stale ones too */
      if (vfp->flags & VF_STALE)
         stale_count++;
   }

   /* if it is not true that there are any non-volatile, stale files 
    * or the directory is stale, then there is nothing to do so return 
    */
   if (!(stale_count || vfs_dir_stale))
      return;

   /* open and truncate an existing backing store, or create a new one if it
      does not exist */
   fd = vfs_sync_open(1,&rc);
   if (!fd)
   {
      dprintf("vfs_sync_open() in vfs_sync() failed, error = %d\n",rc);
      return;
   }

   /* write the tag used to identify file as a backing store file */
   rc = vfs_sync_write(fd,VFS_BACKING_STORE_TAG,
    strlen(VFS_BACKING_STORE_TAG) + 1);
   if (rc)
      return;

   /* write the number of files to be synced */
   if (vfs_sync_write(fd,&file_count,sizeof(file_count)))
      return;

   /* for each file in the VFS */
   for (vfp = vfsfiles; vfp; vfp = vfp->next)
   {
      /* skip files that aren't non-volatile */
      if (!(vfp->flags & VF_NONVOLATILE))
         continue;
#ifdef HT_EXTDEV
      /* if the file is handled by an external file system, its up to 
       * it to deal with the file, so skip files 
       * that are handled by an external file system 
       */
      if (vfp->method)
         continue;
#endif   /* HT_EXTDEV */

      /* write the length of the file name */
      file_name_len = (unsigned short) strlen(vfp->name);
      if (vfs_sync_write(fd,&file_name_len,sizeof(file_name_len)))
         return;

      /* write the file name (note no null termination) */
      if (vfs_sync_write(fd,vfp->name,file_name_len))
         return;

      /* write the file flags */
      if (vfs_sync_write(fd,&(vfp->flags),sizeof(vfp->flags)))
         return;

      /* write the file real_size */
      if (vfs_sync_write(fd,&(vfp->real_size),sizeof(vfp->real_size)))
         return;

      /* write the file comp_size */
      if (vfs_sync_write(fd,&(vfp->comp_size),sizeof(vfp->comp_size)))
         return;

      /* write the file contents */
      for (bytes_written = 0;
          bytes_written < vfp->comp_size; bytes_written += bytes_to_write)
      {
         bytes_to_write = (unsigned int) (vfp->comp_size - bytes_written);
         if (bytes_to_write > VFS_MAX_FILE_IO_SIZE)
            bytes_to_write = VFS_MAX_FILE_IO_SIZE;
         if (vfs_sync_write(fd,vfp->data + bytes_written,bytes_to_write))
            return;
      }

      /* the file is no longer stale */
      vfp->flags &= ~VF_STALE;

      /* easy sanity check */
      file_count--;
   }

   /* close the backing store file */
   rc = vfs_sync_close(fd);
   if (rc)
      dprintf("vfs_sync_close() failed in vfs_sync(), error = %d\n",rc);

   /* reset the flag that indicates that the directory is stale */
   vfs_dir_stale = FALSE;

   /* this should never happen if it does, either the above logic is 
    * screwed up or somebody went and messed with the VFS while 
    * we had it locked 
    */
   if (file_count)
      dprintf("file_count remains %ld in vfs_sync()\n",file_count);
}



/* FUNCTION: vfs_restore()
 * 
 * PARAM1: 
 *
 * RETURNS: 
 */

void vfs_restore()
{
   void *   fd;
   struct vfs_file * vfp;
   char  tag_string[sizeof(VFS_BACKING_STORE_TAG)];
   unsigned long file_count;  /* # of files we are going to restore */
   /* working file contents variables */
   unsigned short file_name_len;    /* working file name length */
   char  file_name[FILENAMEMAX   +  1];
   unsigned short flags;
   unsigned long real_size;
   unsigned long comp_size;
   unsigned char *buffer;
   unsigned long bytes_read;
   unsigned int bytes_to_read;
   int   rc;

   /* open existing backing store, fail if it does not exist */
   fd = vfs_sync_open(0,&rc);

   /* if the open fails, we will assume here its because it does not 
    * exist, which is ok since that just means that we haven't had 
    * anything to sync yet
    */
   if (!fd)
   {
      return;
   }

   /* read the tag at the beginning of the file */
   if (vfs_sync_read(fd,tag_string,sizeof(tag_string)))
      return;

   /* if its not our tag, tell user and bail */
   if (strcmp(tag_string,VFS_BACKING_STORE_TAG))
   {
      vfs_sync_close(fd);
#ifdef NPDEBUG
      dprintf("backing store tag is not present\n");
#endif   /* NPDEBUG */
      return;
   }

   /* read the number of file entries */
   if (vfs_sync_read(fd,&file_count,sizeof(file_count)))
      return;

   /* for each file entry */
   for ( ; file_count; file_count--)
   {
      /* read the length of the file name */
      if (vfs_sync_read(fd,&file_name_len,sizeof(file_name_len)))
         return;
      if (file_name_len > FILENAMEMAX)
      {
#ifdef NPDEBUG
         dprintf("file_name_len = %d\n",file_name_len);
#endif   /* NPDEBUG */
         break;
      }

      /* note that we read the file information into locals rather 
       * than directly into a linked vfs_file structure because it 
       * makes the clean up after a read error or memory allocation 
       * failure a lot simpler
       */
      /* read the file name */
      if (vfs_sync_read(fd,file_name,file_name_len))
         return;

      /* null terminate the file name */
      file_name[file_name_len] = 0;

      /* read the flags field */
      if (vfs_sync_read(fd,&flags,sizeof(flags)))
         return;

      /* read the real (uncompressed) file size */
      if (vfs_sync_read(fd,&real_size,sizeof(real_size)))
         return;

      /* read the actual (compressed) file size */
      if (vfs_sync_read(fd,&comp_size,sizeof(comp_size)))
         return;

      /* if the file is not empty */
      if (comp_size)
      {
         /* allocate a buffer to hold its contents */
         buffer = vf_alloc_buffer(comp_size);
         /* if the allocation failed */
         if (!buffer)
         {
            dprintf("buffer allocation failed for %s\n",file_name);
            break;
         }

         /* read the file contents into the buffer */
         for (bytes_read = 0;
             bytes_read < comp_size; bytes_read += bytes_to_read)
         {
            bytes_to_read = (unsigned int) (comp_size - bytes_read);
            if (bytes_to_read > VFS_MAX_FILE_IO_SIZE)
               bytes_to_read = VFS_MAX_FILE_IO_SIZE;
            if (vfs_sync_read(fd,buffer + bytes_read,bytes_to_read))
            {
               vf_free_buffer(buffer,comp_size);
               return;
            }
         }
      }
      else
      {
         /* file is empty, so associated file contents buffer is NULL */
         buffer = NULL;
      }

      /* at this point we have read all the file contents into locals 
       * and the allocated buffer, so the backing store file pointer 
       * is now pointing to the data for the next VFS file, which 
       * means its safe to "continue" below and still have to 
       * loop work right 
       */
      /* see if its already in the VFS that came with the executable */
      vfp = vfslookup_locked(file_name);

      /* if its in the VFS that came with the executable */
      if (vfp)
      {
         /* if its not write enabled */
         if (!(vfp->flags & VF_WRITE))
         {
            /* free the allocated buffer and continue. note that this 
             * could happen if in an older version of the executable 
             * the file was writable and somebody had written to it, 
             * but then in the current executable the file is not 
             * writable. in this case I think the executable should 
             * take precedence 
             */
            vf_free_buffer(buffer,comp_size);
            continue;
         }

#ifdef HT_EXTDEV
         /* the VFS that came with the executable is saying this file 
          * belongs to another file system, yet its in our list of 
          * files. believe the executable and toss the file contents 
          */
         if (vfp->method)
         {
            vf_free_buffer(buffer,comp_size);
            continue;
         }

#endif   /* HT_EXTDEV */

         /* the following is a sanity check, it should never happen 
          * so long as this function is only called once at startup 
          * since it would imply that a file or its contents got 
          * added dynamically before this function was called 
          */
         if (vfp->flags & (VF_DYNAMICDATA | VF_DYNAMICINFO))
         {
#ifdef NPDEBUG
            dprintf("existing vfp is dynamic in vfs_restore %s,%s,0x%x\n",
             file_name,vfp->name,vfp->flags);
#endif   /* NPDEBUG */
            dtrap();
         }
      }
      else  /* its not in the VFS that came with the executable */
      {
         /* enforce maximum number of files */
         if (vfs_total_dyna_files >= VFS_MAX_DYNA_FILES)
         {
#ifdef NPDEBUG
            dprintf("vfs_total_dyna_files exceeded for %s\n",file_name);
#endif   /* NPDEBUG */
            /* note that vf_free_buffer() checks for buffer == NULL,
               so calling this without checking is not a bug */
            vf_free_buffer(buffer,comp_size);
            break;
         }

         /* allocate a vfs_file structure to hold the new file entry in */
         vfp = VFS_VFS_FILE_ALLOC();

         /* check for memory allocation failure */
         if (!vfp)
         {
#ifdef NPDEBUG
            dprintf("vfs_file allocation for %s failed\n",file_name);
#endif   /* NPDEBUG */
            vf_free_buffer(buffer,comp_size);
            break;
         }

         /* add the vfs_file structure to the head of the list */

         vfp->next = vfsfiles;
         vfsfiles = vfp;

         /* increment count of total files */
         vfs_total_dyna_files++;

         /* store the passed in name in the directory entry structure */
         strcpy(vfp->name,file_name);
      }

      /* we now have a valid vfp and all its contents in locals, all
         that is left is to load it up */
      /* the file might have been stored with the stale bit on, but
         it aint stale now, so turn it off */
      vfp->flags = flags & ~VF_STALE;
      vfp->data = buffer;
      vfp->real_size = real_size;
      vfp->comp_size = comp_size;
      /* since we allocated a buffer just big enough to hold the contents,
         buf_size gets comp_size */
      vfp->buf_size = comp_size;
   }

   /* close the backing store file */
   rc = vfs_sync_close(fd);
   if (rc)
      dprintf("vfs_sync_close() failed in vfs_restore(), error = %d\n",rc);

   /* this should never happen if it does, either the above logic is 
    * screwed up or somebody went and messed with the VFS while 
    * we had it locked 
    */
   if (file_count)
      dprintf("file_count remains %ld in vfs_restore()\n",file_count);
}

#endif   /* HT_SYNCDEV && HT_RWVFS */


