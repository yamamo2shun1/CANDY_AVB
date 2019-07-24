/*
 * FILENAME: nvfsio.c
 *
 * Copyright 1998-2002 By InterNiche Technologies Inc. All rights reserved
 *
 * This file contains the generic code to read a text file which contains
 * a name/value list of non-volatile parameters.
 *
 * MODULE: MISCLIB
 *
 * ROUTINES: nv_fgets(), nv_fprintf(),
 * ROUTINES: vfs_sync_open(), vfs_sync_close(), vfs_sync_read(), 
 * ROUTINES: vfs_sync_write(), 
 *
 * PORTABLE: yes
 */

/* Additional Copyrights: */
/* Portions Copyright 1996 by NetPort Software  
 * 2/7/98 - Created from routines in nvparms.c -JB- 
 */


#include "ipport.h"


#ifdef NATIVE_PRINTF
#include <stdarg.h>
#include <stdio.h>
#endif

#include "libport.h"

/* build these only if using NVPARMS */
#ifdef INCLUDE_NVPARMS

#define  _NVFSIO_C_  1

#include "vfsfiles.h"
#include "nvfsio.h"
#include "nvparms.h"

#ifndef HT_LOCALFS
/* If there is no local file system, include code for NV parameters
 * read (nv_fgets) and write (nv_fprintf) routines
 */


/* FUNCTION: nv_fgets()
 *
 * nv_fgets(char buf, unsigned maxlen, FILE fp) - fgets for reading a 
 * line from a .nv file. This routine is provided for legacy code. 
 * FILE parameter is cast as a void* to deal with compiler 
 * crustiness. read data from the VFS file handle into the buffer 
 * until: we read an end of file condition or any sort of line end 
 * (CR or LF), or maxlen bytes of data have been read into the 
 * buffer. 
 *
 * 
 * PARAM1: char * buffer
 * PARAM2: int maxlen
 * PARAM3: NV_FILE * fd
 *
 * RETURNS: returns NULL if we encountered an EOF and no data were 
 * read, otherwise returns address of buffer.
 */

char *   
nv_fgets(char * buffer, int maxlen, NV_FILE * fd)
{
   int   bytes_read  =  0;
   int   in_char;
   int   read_eof =  FALSE;
   char *   rc =  buffer;

   while (TRUE)
   {
      /* terminate if we've read our maximum number of bytes */
      /* leaving room for the newline and null at the end */
      if (bytes_read >= (maxlen - 2))
         break;
      /* read a byte */
      in_char = vgetc((VFILE*)fd);
      /* terminate if EOF */
      if (in_char < 0)
      {
         read_eof = TRUE;
         break;
      }
      /* terminate if end of line */
      if (in_char == 0xa)
         break;
      /* if we see a return, don't copy it since the code that calls
         this appears to be expecting newline conversion to be done */
      if (in_char == 0xd)
         continue;
      /* copy the read character into the buffer */
      *(buffer + bytes_read) = (char) (in_char & 0xff);
      ++bytes_read;
   }
   /* tack a newline on the end since the caller expects to see one */
   *(buffer + bytes_read) = 0xa;
   /* null terminate input */
   *(buffer + bytes_read + 1) = 0;
   /* if no bytes were read and we read an EOF character, return NULL */
   if ((!bytes_read) && read_eof)
      rc = (char *) 0;
   return rc;
}


/* FUNCTION: nv_fprintf()
 * 
 * PARAM1: NV_FILE * fd
 * PARAM2: char * format
 * PARAM3: ...
 *
 * RETURNS: 
 */

#ifdef PRINTF_STDARG
/* different nv_fprintf() functions depending on VA support */
#include <stdarg.h>

int
nv_fprintf(NV_FILE * fd, char * format, 
   ...)
{
   int   len,  err;
   char  linebuf[NV_LINELENGTH];
   va_list a;

   va_start(a, format);
   err = vsprintf(linebuf, format, a);
   va_end(a);

#else /* the non-STDARG version */

int
nv_fprintf(NV_FILE * fd, char * format, int arg1)
{
   int len, err;
   char linebuf[NV_LINELENGTH];

   doprint(linebuf, NV_LINELENGTH, format, &arg1);

#endif   /* PRINTF_STDARG */

   /* output string now in linebuf, rest of function is the same */

   /* check for buffer overflow */
   len = strlen(linebuf);
   if (len > NV_LINELENGTH) panic("nv_fprintf");

      /* write formatted buffer to VF device */
   err = vfwrite(linebuf, len, 1, fd);
   return err;
}

#endif   /* HT_LOCALFS */


#ifdef   HT_SYNCDEV

/* default routine to convert Linear address to seg/offset. This 
 * default just handles the long to pointer cast and will work on 
 * most 32 bit linear 
 */

#ifndef LinToSeg
#define  LinToSeg(addr) ((void*)(addr))
#endif

/* this variable is used to keep track of the current location in flash
 * where reads and writes are occuring. it contains the physical address
 * of the location in flash and must be converted to a long pointer
 * before being deferenced 
 */
unsigned long vfs_flash_address;

/* FUNCTION: *vfs_sync_open()
 *
 * PARAM1: int clear
 * PARAM2: int *p_error
 *
 * RETURNS: VFS file handle of open device, NULL if error.
 */

void *
vfs_sync_open(int clear, int * p_error)
{
   unsigned short sector;
   int   rc;

   /* clear != 0 => clear backing store */
   if (clear)
   {
      /* so erase the flash */
      for (sector = VFS_BK_STORE_FIRST_SECTOR;
          sector <= VFS_BK_STORE_LAST_SECTOR; ++sector)
      {
         rc = EraseFlash(sector);
         if (rc != TRUE)
         {
            /* the AMD flash driver doesn't tell you why a failure
               occured, so this is the best we can do */
            *p_error = 1;
            dprintf("EraseFlash(%d) failed\n",sector);
            return NULL;
         }
      }
   }

   /* put the "file" pointer at the beginning of the flash backing store */
   vfs_flash_address = VFS_BK_STORE_BASE_ADDRESS;

   /* return the address of the file pointer */
   return (void *) &vfs_flash_address;
}



/* FUNCTION: vfs_sync_close()
 * 
 * PARAM1: void *xfd
 *
 * RETURNS: 0
 */

int
vfs_sync_close(void * xfd)
{
   /* more compiler warning suppression */
   unsigned long *fd = xfd;

   USE_ARG(fd);
   /* this close can't fail, so return 0 */
   return 0;
}


/* FUNCTION: vfs_sync_read()
 *
 * read len bytes of data into by buf from fd.
 * 
 * PARAM1: void *xfd
 * PARAM2: void *buf
 * PARAM3: unsigned int len
 *
 * RETURNS: 0 if successful or the non-zero reason for the error if not
 */

int
vfs_sync_read(void * xfd, void * buf, unsigned int len)
{
   unsigned long *fd = xfd;

   /* if caller is trying to read past end of backing store */
   if ((*fd + len) >
       (VFS_BK_STORE_BASE_ADDRESS + VFS_BK_STORE_SIZE))
   {
      dprintf("vfs_sync_read() tried to read 0x%x bytes at 0x%lx\n",
       len,*fd);
      vfs_sync_close(fd);
      return 1;   /* its the only error that can occur */
   }

   /* copy the data in flash to the caller buffer */
   MEMCPY(buf,LinToSeg(*fd),len);

   /* increase the "file" pointer by the amount of data read */
   *fd += len;
   return 0;
}


/* FUNCTION: vfs_sync_write()
 * 
 * writes len bytes of data pointed to by buf to fd.
 *
 * PARAM1: void *xfd
 * PARAM2: void *buf
 * PARAM3: unsigned int len
 *
 * RETURNS: 0 if successful or the non-zero reason for the error if not
 */

 
int
vfs_sync_write(void * xfd, void * buf, unsigned int len)
{
   unsigned int bytes_to_write;
   unsigned long *fd = xfd;
   int   rc;

   /* if caller is trying to write past end of backing store */
   if ((*fd + len) > (VFS_BK_STORE_BASE_ADDRESS + VFS_BK_STORE_SIZE))
   {
      dprintf("vfs_sync_write() tried to write 0x%x bytes at 0x%lx\n",
       len,*fd);
      vfs_sync_close(fd);
      return 1;
   }

   /* while there's data left to write */
   while (len)
   {
      /* chose a reasonable chunk to write at a time */
      bytes_to_write =
      (len > VFS_MAX_FILE_IO_SIZE) ? VFS_MAX_FILE_IO_SIZE : len;
      /* write the chunk to the flash driver */
      rc = ProgramFlash(*fd, buf, (int) bytes_to_write);
      /* the flash driver returns TRUE when successful */
      if (rc != TRUE)
      {
         dprintf("ProgramFlash(0x%lx,buf,0x%x) failed\n",*fd,len);
         vfs_sync_close(fd);
         return 2;
      }
      /* increase the "file pointer" by the amount written */
      *fd += bytes_to_write;
      /* decrease whats remaining to write by the amount written */
      len -= bytes_to_write;
   }
   return 0;   /* success */
}
#endif   /* HT_SYNCDEV */

#endif   /* INCLUDE_NVPARMS */
