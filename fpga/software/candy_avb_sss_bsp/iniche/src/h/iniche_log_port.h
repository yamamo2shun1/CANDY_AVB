#ifndef _INICHE_LOG_PORT_H_
#define _INICHE_LOG_PORT_H_

#include "ipport.h"
#include "nvfsio.h"

/* Note: USE_LOGFILE should be defined only of there exits a file system */
#define USE_LOGFILE  1

#define DEFAULT_LOG_LEVEL LOG_TYPE_DEBUG
#define LOG_FILE_DEFAULT "./iniche.log"
#define LOGFILE_NAME_SIZE 255
#define LOG_MALLOC(size) npalloc(size)
#define LOG_FREE(p)      npfree(p)

#endif   /* _INICHE_LOG_PORT_H_ */


