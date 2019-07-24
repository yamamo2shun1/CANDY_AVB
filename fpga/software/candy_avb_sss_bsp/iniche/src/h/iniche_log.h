#ifndef _INICHE_LOG_H_
#define _INICHE_LOG_H_

#include "iniche_log_port.h"

/* Increase this as you add more options to glog menu */ 
#define GLOG_MAX_ARGC 20

typedef enum _logTypeEnum
{
   LOG_TYPE_NONE = 0,
   LOG_TYPE_FATAL,
   LOG_TYPE_CRITICAL,
   LOG_TYPE_MAJOR,
   LOG_TYPE_MINOR,
   LOG_TYPE_WARNING,
   LOG_TYPE_INFO,
   LOG_TYPE_DEBUG,
   
   LOG_TYPE_MAX_NUM

} logTypeEnum;

typedef struct _log_data_t
{
   void * pio;
#ifdef USE_LOGFILE
   char log_fname[LOGFILE_NAME_SIZE];
   NV_FILE *log_fp;
#endif
   unsigned long log_counts[LOG_TYPE_MAX_NUM];
   int log_level;    /* This is the current log level */
   int start_flag;
   int stop_flag;
   char **argv;
   int argc;

} log_data_t;

int iniche_create_log(log_data_t **p, int log_level, char *fname, void *pio);
void iniche_free_log(log_data_t **p);
void log_with_type(log_data_t *p, logTypeEnum log_type, char *pstr, int printf_flag);

void glog_with_type(logTypeEnum log_type, char *pstr, int printf_flag);
int global_log_create(void);
void global_log_free(void);

int glog_app(void * pio);

#endif   /* _INICHE_LOG_H_ */


