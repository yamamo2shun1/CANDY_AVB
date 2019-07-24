/*
 * FILENAME: ftpsvfs.c
 *
 * Copyright  2000 By InterNiche Technologies Inc. All rights reserved
 *
 *
 * MODULE: FTP
 *
 * ROUTINES: fs_dodir(), lslash(), fs_dir(), fs_permit(), 
 *
 * PORTABLE: no
 */

/* Additional Copyrights: */

/* ftpsvfs.c 
 * Portions Copyright 1996 by NetPort Software. All rights reserved. 
 * FS dependent portions of FTP serer. This version for InteNiche 
 * VFS. 
 * 7/21/96 - Taken out of ftpsrv.c - John Bartas 
 */

#include "ftpport.h"    /* TCP/IP, sockets, system info */
#include "ftpsrv.h"

#include "vfsfiles.h"
#include "in_utils.h"

int ftpputs(ftpsvr * ftp, char * text);
void ftp_leave_passive_state(ftpsvr * ftp);



/* FUNCTION: fs_dodir()
 * 
 * fs_dodir() - do a "dir" (or "ls" for UNIX weenies) on the current 
 * ftp directory, write the resulting text out ftps->datasock, which 
 * is open before this is called. How you do the DIR and buffer is 
 * local to this function. Any tmp files or buffers should be cleaned 
 * up before you return. This is the VFS-only version.
 *
 * PARAM1: ftpsvr * ftp
 * PARAM2: u_long ftpcmd
 *
 * RETURNS: Returns 0 if OK, else -1 if error. 
 */

int
fs_dodir(ftpsvr * ftp, u_long ftpcmd)
{
   char *   cp;
   int   bytes_to_send;
   int   bytes_sent;
   int   rc;
   int   blocked;
   struct vfs_file * vfp;

   ftpputs(ftp, "150 Here it comes...\r\n");

   /* if we are already connected to the client because we are in
      passive mode, don't create connection to client */
   if (!(ftp->passive_state & FTPS_PASSIVE_CONNECTED))
   {
      /* create a data connection back to the client */
      ftp->datasock = FTP_TCPOPEN(ftp);
      if (ftp->datasock == SYS_SOCKETNULL)
      {
         ftpputs(ftp, "425 Can't open data connection\r\n");
         return 0;   /* not actually OK, but we handled error */
      }
   }

   /* lock the VFS */
   vfs_lock();

   /* for each file in the file list */
   for (vfp = vfsfiles; vfp; vfp = vfp->next)
   {
      /* if client asked for long version of file listing */
      if (ftpcmd == 0x4c495354)  /* "LIST" */
      {
         /* print month, day, hour and minute, as in :
         -rw-r--r--   1 jharan   jharan  11772 Jan 19 13:31 install.log */
         /* since we don't have time stamps in the VFS, we lie about 
          * the date and time. if the VF_WRITE bit is set, the file 
          * is read/write so we display the roughly analogous 
          * Unix file mask corresponding to 666 else 444 
          */
         sprintf(ftp->filebuf,
          "%s 0 root root %11ld %s %2d %02d:%02d %s",
          ((vfp->flags & VF_WRITE) ? "-rw-rw-rw-" : "-r--r--r--"),
          vfp->real_size,"Jan",1,1,1,vfp->name);
      }
      else
         /* else just give the client the file name */
      strcpy(ftp->filebuf,vfp->name);

      /* append a newline sequence to the end of the file listing */
      cp = ftp->filebuf + strlen(ftp->filebuf);
      *cp++ = '\r';
      *cp++ = '\n';
      *cp = 0;

      /* get number of bytes to transmit */
      bytes_to_send = cp - ftp->filebuf;

      blocked = 0;
      /* while there are bytes left to transmit */
      for (bytes_sent = 0; bytes_to_send > 0; )
      {
         /* try to send as much as is left to transmit */
         rc = t_send(ftp->datasock,ftp->filebuf + bytes_sent,bytes_to_send,0);

         /* this means some sort of error occurred */
         if (rc < 0)
         {
            /* get socket error. If it's s (hopefully) transient buffer shortage
             * then just wait a bit and try again, up to a limit:
             */
            rc = t_errno(ftp->datasock);
            if((rc == EWOULDBLOCK) || (rc == ENOBUFS))
            {
               if(blocked++ < 100)     /* don't loop here forever... */
               {
                  tk_yield();    /* let system spin a bit */
                  continue;
               }
            }
            ftpputs(ftp, "426 aborted, data send error\r\n");
            break;
         }

         /* socket could be non-blocking, which means t_send() might have
            sent something less than what was requested */
         bytes_to_send -= rc;
         bytes_sent += rc;

#ifndef BLOCKING_APP
         /* if the whole thing wasn't sent, it wont get any better
          * if you don't yield to receive side
          */
         if (bytes_to_send > 0)
            tk_yield();
#endif
      }

      /* if this happens, we broke in the loop above because of a
         socket error */
      if (bytes_to_send > 0)
         break;
   }

   /* unlock the VFS */
   vfs_unlock();

   /* if vfp is now NULL, then we exited the above loop without an
      error, so we can report that the transfer went ok */
   if (!vfp)
      ftpputs(ftp, "226 Transfer OK, Closing connection\r\n");

   /* close the data connection and leave passive state if we in it */
   ftp_leave_passive_state(ftp);

   return 0;   /* good return */
}




/* FUNCTION: lslash()
 * 
 * lslash() - format universal (UNIX) slashes '/' into local type. 
 * PC-DOS version. 
 *
 * PARAM1: char * path
 *
 * RETURNS: 
 */

void
lslash(char * path)
{
   char *   cp;

   for (cp = path; *cp; cp++)
      if (*cp == '\\')     /* DOS slash? */
      *cp = '/';     /* convert to normal slash */
}

/* fs_dir() - verify drive/directory exists. */



/* FUNCTION: fs_dir()
 * 
 * PARAM1: ftpsvr * ftp
 *
 * RETURNS: 
 */

int
fs_dir(ftpsvr * ftp)
{
   /* for the sake of web browsers that start off with a cd /, we
      will allow that the directory "/" exists */
   if (!strcmp(ftp->filename,"/"))
      return TRUE;
   /* otherwise the VFS is flat, so there are no directories */
   return FALSE;  /* path/name not found */
}




/* FUNCTION: fs_permit()
 * 
 * fs_permit() - check if the logged in user has permision for this 
 * file operation. Returns TRUE or FALSE. 
 *
 * PARAM1: ftpsvr * ftp
 *
 * RETURNS: 
 */

int
fs_permit(ftpsvr * ftp)    /* verify user permission */
{
   USE_ARG(ftp);

   return TRUE;
}


