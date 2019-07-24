/* tftpport.h

   Copyright 1998-2000 by InterNiche Technologies Inc.

   TFTP per-port definitions.
   
   12/26/98 - Created as part of cleanup. John Bartas
*/

#ifndef _TFTPPORT_H_
#define _TFTPPORT_H_ 1

#include "ipport.h"

/* version 1.5 VFS hooks */
#ifdef VFS_FILES
#include "vfsfiles.h"
#else
#define vfopen(n,m)     fopen(n,m)
#define vfclose(fd)     fclose(fd)
#define vferror(fd)     ferror(fd)
#define vfread(buf,i,j,fd)    fread(buf,i,j,fd)
#define vfwrite(buf,i,j,fd)   fwrite(buf,i,j,fd)
#define vfseek(fd,off,m)      fseek(fd,off,m)
#define VFILE FILE
#endif

#ifdef NOTDEF
#define ZEROCOPY_API 1     /* IP buffers can be marked in use */
#endif

#define TFTPSERVER  (void*)(-99L)   /* fake session for server */

/* map TFTP's alloc & free routines to heap for this port */
#ifndef TFC_ALLOC
#define TFC_ALLOC(size) (struct tfconn *)npalloc(sizeof(struct tfconn))
#define TFC_FREE(ptr) npfree(ptr)
#endif

#define	MAXTFTPS	   4  /* max. concurrent sessions */

/*  Constants for round trip time estimation and retry timeout.
    All calculation is done in clock ticks (at TPS rate) but
    only the initial estimate and the upper limits are specified in ticks;
    the rest of the algorithm uses dimensionless multipliers.  */

#ifndef   TF_MAXTMO
#define   TF_MAXTMO   (15*TPS)   /* upper limit on retry timeout timer */
#endif

#ifndef   TF_MINTMO
#define   TF_MINTMO   (1*TPS)    /* lower limit on retry timeout timer */
#endif

/*  Constants for limiting the number of retries.  
    Either or both of these can be zero to *not* limit the number of
    retries.  */

#ifndef   TFTPTRIES
#define   TFTPTRIES   (4)        /* # of retries on packet transmission */
#endif

#ifndef   REQTRIES
#define   REQTRIES    (5)        /* # of retries on initial request */
#endif

#endif /* _TFTPORT_H_ */


