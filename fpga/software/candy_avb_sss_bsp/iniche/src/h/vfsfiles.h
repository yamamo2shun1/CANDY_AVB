/*
 * FILENAME: vfsfiles.h
 *
 * Copyright 1999-2008 By InterNiche Technologies Inc. All rights reserved
 *
 * NetPort "virtual" file system. This implements the vfopen(), 
 * vfread(), etc, calls the Embedded WEB servers (et.al.) makes to do 
 * file IO. These calls lookup names, descriptors, etc, to see if the 
 * file is a "virtual" file in RAM/ROM/FLASH, or if it should fall 
 * through to the local disk system (if any)
 *
 * MODULE: VFS
 *
 *
 * PORTABLE: yes
 */

/* Additional Copyrights: */
/* vfsfiles.h 
 * Portions Copyright 1996 by NetPort Software. All rights reserved. 
 */

#ifndef _VFSFILES_H
#define  _VFSFILES_H 1

/* bit flag values for vfs_file.flags mask */
#define     VF_HTMLCOMPRESSED    0x01  /* If HTML, file is tag-compressed */
#define     VF_GIFCOMPRESSED     VF_HTMLCOMPRESSED /* If GIF, file is 0-compressed */

#define     VF_AUTHBASIC   0x02  /* check Basic user auth */
#define     VF_AUTHMD5     0x04  /* check MD5 user auth */

#define     VF_MAPFILE     0x08  /* data pointer is a (struct mapfile*) */
#define     VF_CVAR        0x10  /* it's C variable disply */

#define     VF_WRITE       0x20  /* writable device */
#define     VF_DYNAMICINFO 0x40  /* file info created dynamically */
#define     VF_DYNAMICDATA 0x80  /* file data created dynamically */
#define     VF_NONVOLATILE 0x100 /* copy to non-volatile storage on,close */
#define     VF_STALE       0x200 /* contents of file have been changed */
#define     VF_PUSH        0x400 /* It's a web server "push" file */
#define     VF_NOCACHE     0x800
#define     VF_NODATA      0x8000 /* file has a size, but no data actual data */

#ifndef VFS_FILES /* set (or not) in ipport.h */

/* If there's no VFS, just map vfs routines to local */
#ifdef HT_LOCALFS

#define  VFILE FILE
#define  vfopen(n,m)    fopen(n,m)
#define  vfclose(fd)    fclose(fd)
#define  vfread(buf,i,j,fd)      fread(buf,i,j,fd)
#define  vfseek(fd,off,m)        fseek(fd,off,m)
#define  vfwrite(buf,siz,items,fd)  fwrite(buf,siz,items,fd)
#define  vftell(fd)     ftell(fd)
#define  vgetc(fd)      fgetc(fd)
#define  vferror(fd)    ferror(fd)
#define  vunlink(name)  remove(name)
#define  vclearerr(fd)  clearerr(fd)

#else /* no local file system either? */
#error Don't compile this, the defines don't make sense
#error Either VFS_FILES or HT_LOCALFS must be defined, or both.
#endif   /* HT_LOCALFS */

#else /* VFS_FILES - there IS a VFS system: */

#ifndef HT_LOCALFS   /* Build defined VFS but no local FS */
/* Map ansi routines and declarations to VFS */
#ifndef NO_STDFILE_CALLS   /* Switch requested by GHS Integrity group */
#undef  FILE
#undef  fopen
#undef  fclose
#undef  fread
#undef  fseek
#undef  fwrite
#undef  ftell
#undef  getc
#undef  ferror
#undef  remove
#undef  unlink
#undef  clearerr

#define  FILE                 VFILE
#define  fopen(n,m)           vfopen(n,m)
#define  fclose(fd)           vfclose(fd)
#define  fread(buf,i,j,fd)    vfread(buf,i,j,fd)
#define  fseek(fd,off,m)      vfseek(fd,off,m)
#define  fwrite(buf,siz,items,fd)      vfwrite(buf,siz,items,fd)
#define  ftell(fd)            vftell(fd)
#define  getc(fd)             vgetc(fd)
#define  ferror(fd)           vferror(fd)
#define  remove(name)         vunlink(name)
#define  unlink(name)         vunlink(name)
#define  clearerr(fd)         vclearerr(fd)
#endif   /* NO_STDFILE_CALLS */
#endif   /* HT_LOCALFS */

#define  FILENAMEMAX    16 /* max chars in a file name */

#define  HTPATHMAX   128   /* max chars in a file name */

extern   char  vfs_root_path[HTPATHMAX];  /* path to file system */

#ifdef WEBPORT
/* preserve compatibility with old-style webport path */
extern   char *   http_root_path;
/* predeclare structures to avoid compiler complaints */
struct httpd;
struct httpform;


#ifdef SERVER_PUSH
struct vfs_file;      /* predecl */
typedef int   (*pushcall)(struct vfs_file * vp, struct httpd * hp);
int   htpush_init(char * name, pushcall callbk);
#endif   /* SERVER_PUSH */
#endif   /* WEBPORT */

/* The per-file structure for Web server virtual files. These are 
 * kept in a linked list which may be build dynamically or 
 * pre-compiled into the system image:
 */

struct vfs_file
{
   struct vfs_file * next;
   char  name[FILENAMEMAX  +  1];   /* name of file under "path" */
   unsigned short flags;
   unsigned char *   data;    /* pointer to file data, NULL if none */
   unsigned long  real_size;  /* size in bytes of file before compression */
   unsigned long  comp_size;  /* size in bytes of file compressed */
   unsigned long  buf_size;   /* size in bytes of current memory buffer */

#ifdef WEBPORT
   /* routine to call on GET, POST, or special SSIs; NULL if none */
   int (*ssi_func)(struct vfs_file *, struct httpd *, char * args);
   /* routine to call if file is treated as CGI executable */
   int (*cgi_func)(struct httpd *, struct httpform *, char ** text);
#ifdef SERVER_PUSH
   pushcall pushfunc;
#endif   /* SERVER_PUSH */
#endif   /* WEBPORT */

   void *   method;        /* pointer depends on flags */
};


#ifndef VFS_VFS_FILE_ALLOC    /* allow override form ipport.h */
#define VFS_VFS_FILE_ALLOC() \
   (struct vfs_file *) npalloc(sizeof(struct vfs_file))
#define  VFS_VFS_FILE_FREE(x) npfree(x)
#endif   /* VFS_VFS_FILE_ALLOC */


extern   struct vfs_file * vfsfiles;   /* linked list of all vfiles */

/* structures created for and returned from each vfopen() call. These
 * are cleaned up by vfclose() calls.
 */
 
struct vfs_open
{
   struct vfs_open * next;
   struct vfs_file * file;
   unsigned char * cmploc;       /* current position in data buf */
   unsigned char * tag;          /* current position in compressed tag, if any */
   int   error;                  /* last error, if any */
};

typedef struct vfs_open VFILE;

#define VFS_VFS_OPEN_ALLOC() \
   (struct vfs_open *) npalloc(sizeof(struct vfs_open))
#define  VFS_VFS_OPEN_FREE(x) npfree(x)

extern   VFILE *  vfiles;  /* list of open files */

/* for ".map" files, the data pointer actually points to a linked 
list of these structures: */
struct mapitem {
   struct mapitem *  next;    /* list link */
   char  name[FILENAMEMAX  +  1];   /* name of html file to GET */
   int   x_min,   x_max;      /* X coordinate range */
   int   y_min,   y_max;      /* Y coordinate range */
};

/* routines which map to standard buffered file routines: */
VFILE*   vfopen(char * name, char * mode);
int      vfflush(VFILE * vfd);
void     vfclose(VFILE * vfd);
int      vfread(char * buf, unsigned size, unsigned items, VFILE * vfd);
int      vfwrite(char * buf, unsigned size, unsigned items, VFILE * vfd);
int      vfseek(VFILE * vfd, long offset, int mode);
long     vftell(VFILE * vfd);
int      vgetc(VFILE * vfd);
char     *vfgets(char * s, int lim, VFILE * fp);
int      vfeof(VFILE * vfd);
int      vferror(VFILE * vfd);
/*
 * Altera Niche Stack Nios port modification:
 * Change prototype from char * name to const char to
 * follow C library standard.
 */
int      vunlink(const char * name);
void     vclearerr(VFILE * vfd);

#ifdef HT_EXTDEV
struct vfroutines {
   struct vfroutines *  next; /* keep these in a list */
   VFILE* (* r_fopen)(char * name, char * mode);
   void   (* r_fclose)(VFILE * vfd);
   int    (* r_fread)(char * buf, unsigned size, unsigned items, VFILE * vfd);
   int    (* r_fwrite)(char * buf, unsigned size, unsigned items, VFILE * vfd);
   int    (* r_fseek)(VFILE * vfd, long offset, int mode);
   long   (* r_ftell)(VFILE * vfd);
   int    (* r_fgetc)(VFILE * vfd);
   int    (* r_unlink)(char*);
};
extern   struct vfroutines *  vfsystems;
#endif

void vfs_init(void);
void vfs_file_detail(struct vfs_file * vfp, char * buf);
int vfs_file_list(void * pio);

#ifdef HT_RWVFS

/* this constant defines the maximum amount of space that will be
 * allocated to contain data buffers for the read/write file system
 */

#ifndef VFS_MAX_TOTAL_RW_SPACE   /* allow override */
#define  VFS_MAX_TOTAL_RW_SPACE  0xffff
#endif


/* this counts the number of bytes that have been allocated for 
 * buffers for the read/write file system so that the above maximum 
 * can be enforced
 */
extern   unsigned long  vfs_total_rw_space;

/* this constant defines the maximum number of dynamically allocated
   files that can exist in the file system */
#define  VFS_MAX_DYNA_FILES   0xff

/* this counts the number of dynamically allocated files in the file
   system so that the above maximum can be enforced */
extern   unsigned long  vfs_total_dyna_files;

#define  VFS_CLOSE_FRAG_FLOOR 255

unsigned char *vf_alloc_buffer(unsigned long size);
void vf_free_buffer(unsigned char * buffer, unsigned long size);

void vfs_sync(struct vfs_file * vfp);
void vfs_restore(void);

extern   int   vfs_dir_stale;

#ifndef VFS_MAX_FILE_IO_SIZE
#define VFS_MAX_FILE_IO_SIZE (16 * 1024)
#endif

/* opens backing store media, clears it if clear !=0, returns handle 
 * to media if successful, returns null and puts 
 * error code (like errno) at perror if unsuccessful 
 */
void * vfs_sync_open(int clear, int * p_error);

/* closes the backing store media. returns 0 if successful */
int vfs_sync_close(void * fd);

/* read len bytes into buf from the backing store referenced by fd at 
 * the current backing store offset. increment offset by len. 
 * returns 0 if successful 
 */
int vfs_sync_read(void * fd, void * buf, unsigned int len);

/* write len bytes from buf to the backing store referenced by fd at 
 * the current backing store offset. increment offset by len. 
 * returns 0 if successful, error if not 
 */
int vfs_sync_write(void * fd, void * buf, unsigned int len);

#endif   /* HT_RWVFS */

/* this constant defines the maximum number of files that can be
   concurrently open in the file system */
#define  VFS_MAX_OPEN_FILES   0xff

/* this counts the number of open files in the file system so that
   the above maximum can be enforced */
extern   unsigned long  vfs_open_files;

extern   int   vfopen_error;

#ifndef EOF
#define  EOF   (-1)
#endif   /* EOF */

#ifndef EBADF
#define  EBADF (-2)
#endif

/* internal support routines */
struct vfs_file * vfslookup(char * name);    /* lookup vfs_file by file name */
int    isvfile(VFILE*);    /* bool - is the VFILE virtual? */
char * uslash(char * path);   /* make DOS slashes into UNIX */
void set_vfopen_error(int error);
int get_vfopen_error(void);
VFILE * vf_alloc_and_link_vop(void);
VFILE *vfopen_locked(char * name, char * mode);
void vfclose_locked(VFILE * vfd);
void vfunlink_flag_open_files(struct vfs_file * vfp);
int vgetc_locked(VFILE * vfd);
struct vfs_file * vfslookup_locked(char * name);
int isvfile_locked(VFILE * vfp);
int vfwrite_locked(char * buf, unsigned size, unsigned items, VFILE * vfd);

/* fseek constants, if not in C library headers */
#ifndef SEEK_CUR
#define  SEEK_CUR    1
#endif
#ifndef SEEK_END
#define  SEEK_END    2
#endif
#ifndef SEEK_SET
#define  SEEK_SET    0
#endif

#endif  /* VFS_FILES */

#endif  /* _VFSFILES_H */
