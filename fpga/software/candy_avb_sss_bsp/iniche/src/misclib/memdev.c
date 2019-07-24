/*
 * FILENAME: memdev.c
 *
 * Copyright 1998- 2000 By InterNiche Technologies Inc. All rights reserved
 *
 * Memory device for VFS on diskless target systems. 
 * Currently only supports ftp read of memory image, and read/ write 
 * to "null" device. 
 *
 *
 * MODULE: MISCLIB
 *
 * ROUTINES: init_memdev(), md_fopen(), md_fclose(), md_fread(), 
 * ROUTINES: md_fwrite(), md_fseek(), md_ftell(), md_fgetc(), md_unlink(), 
 *
 * PORTABLE: yes
 */


#include "ipport.h"

#ifdef MEMDEV_SIZE   /* whole file can be ifdeffed */

#ifndef VFS_FILES /* These devices require VFS */
#error MEMDEV devices require VFS system
#endif
#ifndef HT_EXTDEV /* ...also VFS external devices feature... */
#error MEMDEV devices require VFS external devices
#endif

#include "vfsfiles.h"

/* The memory device IO routines */
VFILE* md_fopen(char * name, char * mode);
void   md_fclose(VFILE * vfd);
int    md_fread(char * buf, unsigned size, unsigned items, VFILE * vfd);
int    md_fwrite(char * buf, unsigned size, unsigned items, VFILE * vfd);
int    md_fseek(VFILE * vfd, long offset, int mode);
long   md_ftell(VFILE * vfd);
int    md_fgetc(VFILE * vfd);
int    md_unlink(char*);

/* The memory device VFS call structure. This is registered with the 
 * VFS system at init time. Thereafter calls to the memory device 
 * file 
 */

struct vfroutines mdio  =
{
   NULL,       /* list link */
   md_fopen, 
   md_fclose, 
   md_fread, 
   md_fwrite, 
   md_fseek,
   md_ftell, 
   md_fgetc, 
   md_unlink, 
};



#ifndef MEMDEV_BASE
#error memdev needs a base address defined in ipport.h
/* examples:
...Ace360:
#define  MEMDEV_BASE    0x400000
#define  MEMDEV_SIZE    0x200000
#else
...PowerPC MBX board:
#define  MEMDEV_BASE    0x000000
#define  MEMDEV_SIZE    0x200000
*/
#endif   /* not MEMDEV_BASE */


struct vfs_file   mdlist[] = 
{
   {  NULL,          /* list link */
      "mem2M",       /* name of file */
      VF_NODATA,     /* flags (compression, etc) */
      NULL,          /* name of data array */
      MEMDEV_SIZE,   /* length of original file data */
      MEMDEV_SIZE,   /* length of compressed file data */
      0,             /* block size */
#ifdef WEBPORT
      NULL,          /* SSI data routine */
      NULL,          /* CGI routine */
#ifdef SERVER_PUSH
      NULL,          /* push callback */
#endif
#endif
      &mdio,
   },
   {  &mdlist[0],    /* list link */
      "mem1M",       /* name of file */
      VF_NODATA,     /* flags (compression, etc) */
      NULL,          /* name of data array */
      MEMDEV_SIZE/2,   /* length of original file data */
      MEMDEV_SIZE/2,   /* length of compressed file data */
      0,             /* block size */
#ifdef WEBPORT
      NULL,          /* SSI data routine */
      NULL,          /* CGI routine */
#ifdef SERVER_PUSH
      NULL,          /* push callback */
#endif
#endif
      &mdio,
   },
   {  &mdlist[1],    /* list link */
      "mem512K",     /* name of file */
      VF_NODATA,     /* flags (compression, etc) */
      NULL,          /* name of data array */
      MEMDEV_SIZE/4,   /* length of original file data */
      MEMDEV_SIZE/4,   /* length of compressed file data */
      0,             /* block size */
#ifdef WEBPORT
      NULL,          /* SSI data routine */
      NULL,          /* CGI routine */
#ifdef SERVER_PUSH
      NULL,          /* push callback */
#endif
#endif
      &mdio,
   },
   {  &mdlist[2],    /* list link */
      "null",        /* name of file */
      VF_WRITE|VF_NODATA,      /* flags (compression, etc) */
      NULL,          /* name of data array */
      MEMDEV_SIZE,   /* length of original file data */
      MEMDEV_SIZE,   /* length of compressed file data */
      0,             /* block size */
#ifdef WEBPORT
      NULL,          /* SSI data routine */
      NULL,          /* CGI routine */
#ifdef SERVER_PUSH
      NULL,          /* push callback */
#endif
#endif
      &mdio,
   },
};

int mdlist_size = sizeof(mdlist)/sizeof(struct vfs_file);


/* FUNCTION: init_memdev()
 * 
 * PARAM1: 
 *
 * RETURNS: 
 */

int
init_memdev(void)
{
   /* add our IO pointer to master list */
   mdio.next = vfsystems;
   vfsystems = &mdio;

   /* add the memory device files to vfs list */
   mdlist[0].next = vfsfiles;
   vfsfiles = &mdlist[3];

   return 0;
}


/* FUNCTION: md_fopen()
 * 
 * PARAM1: char * name
 * PARAM2: char * mode
 *
 * RETURNS: 
 */

VFILE* 
md_fopen(char * name, char * mode)
{
   USE_ARG(mode);
   USE_ARG(name);
   return NULL;
}



/* FUNCTION: md_fclose()
 * 
 * PARAM1: VFILE * vfd
 *
 * RETURNS: 
 */

void   
md_fclose(VFILE * vfd)
{
   USE_ARG(vfd);
}



/* FUNCTION: md_fread()
 * 
 * PARAM1: char * buf
 * PARAM2: unsigned size
 * PARAM3: unsigned items
 * PARAM4: VFILE * vfd
 *
 * RETURNS: 
 */

int    
md_fread(char * buf, unsigned size, unsigned items, VFILE * vfd)
{
   u_long   bcount;     /* number of bytes put in caller's buffer */
   u_long   location;   /* current offset into file */
   unsigned long file_size = MEMDEV_SIZE;
   if(vfd && vfd->file)
   {
      file_size = vfd->file->real_size; 
/*      printf("md_fread(): name = %s, file_size = %lu\n", vfd->file->name, file_size);*/
   }
/*
   else
   {
      printf("md_fread(): No name: file_size = %lu\n", vfd->file->name, file_size);
   }*/
#ifdef SEG16_16   /* 16-bit x86 must include segment. */
   if(vfd->cmploc == (u_char*)0xFFFFFFFF)   /* at EOF */
      return 0;
   location = (u_long)(((char huge *)vfd->cmploc) - ((char huge *)vfd->file->data));
#else
   location = (u_long)(vfd->cmploc - vfd->file->data);
#endif   /* SEG16_16 */

   bcount = (items * (u_long)size);     /* number of bytes to transfer */

   /* if near end of memory, trim read count accordingly */
   if ((location + bcount) > file_size)
      bcount = ((u_long)file_size - location);

   /* trap bogus size items and end-of-x86 memory conditions */
   if((location >= file_size) ||
      (bcount  & 0xFFFF0000) ||
      (bcount == 0))
   {
      return 0;
   }


   /* Use VF_NODATA if memory devices have a size, but no
      data. This can be used to measure file read speed
      without introducing an undefined data copy. */
   /* VF_NODATA is defined in ../h/vfsfiles.h */
   
   if (!(vfd->file->flags & VF_NODATA))
   {
      if (vfd->file->name[0] == 'm')   /* memory device */
         MEMCPY(buf, vfd->cmploc + MEMDEV_BASE, (unsigned)bcount);
   }


   /* else for null device or files without data,
      use whatever garbage is in the buffer */

   /* before returning, adjust file position ptr for next read */
#ifdef SEG16_16   /* 16-bit x86 must update seg too. */
   {
      char huge * cp;
      cp = (char huge *)vfd->cmploc;
      cp += bcount;  /* adjust location */
      if(cp > (char huge*)vfd->cmploc)    /* make sure memory didn't wrap */
         vfd->cmploc = (u_char *)cp;
      else     /* read wrapped memory, set pointer to EOF value */
         vfd->cmploc = (u_char*)(0xFFFFFFFF);  /* EOF */
   }
#else
   vfd->cmploc += bcount;  /* adjust location */
#endif
   
   return ((int)bcount/size);
}



/* FUNCTION: md_fwrite()
 * 
 * PARAM1: char * buf
 * PARAM2: unsigned size
 * PARAM3: unsigned items
 * PARAM4: VFILE * vfd
 *
 * RETURNS: 
 */

int    
md_fwrite(char * buf, unsigned size, unsigned items, VFILE * vfd)
{
   if (vfd->file->name[0] == 'm')   /* memory device */
      return 0;   /* not writable device */

   vfd->cmploc += (items * size);   /* adjust location */

   USE_ARG(buf);     /* supress compiler warnings */

   return (items);
}



/* FUNCTION: md_fseek()
 * 
 * PARAM1: VFILE * vfd
 * PARAM2: long offset
 * PARAM3: int mode
 *
 * RETURNS: 
 */

int
md_fseek(VFILE * vfd, long offset, int mode)
{
   USE_ARG(vfd);     /* supress compiler warnings */
   USE_ARG(offset);
   USE_ARG(mode);
   return 0;
}



/* FUNCTION: md_ftell()
 * 
 * PARAM1: VFILE * vfd
 *
 * RETURNS: 
 */

long   
md_ftell(VFILE * vfd)
{
   USE_ARG(vfd);     /* supress compiler warnings */
   return MEMDEV_SIZE;
}



/* FUNCTION: md_fgetc()
 * 
 * PARAM1: VFILE * vfd
 *
 * RETURNS: 
 */

int    
md_fgetc(VFILE * vfd)
{
   unsigned location;   /* current offset infile */
   int   retval   =  0;

   location = vfd->cmploc - vfd->file->data;
   if (location >= vfd->file->real_size)     /* at end of file? */
      return EOF;

   if (!(vfd->file->flags & VF_NODATA))
   {
      if (vfd->file->name[0] == 'm')   /* memory device */
         retval = (int)(*vfd->cmploc) & 0xFF ;
   }

   /* else for null device or files without data, 
      use whatever is in retval */

   vfd->cmploc++;    /* adjust location */
   return retval;
}



/* FUNCTION: md_unlink()
 * 
 * PARAM1: char * filename
 *
 * RETURNS: 
 */

int    
md_unlink(char * filename)
{
   USE_ARG(filename);     /* supress compiler warnings */
   return 0;
}

#endif   /* MEMDEV_SIZE - whole file can be ifdeffed out */


