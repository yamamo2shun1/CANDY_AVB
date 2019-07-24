
#include "ipport.h"

#ifdef INCLUDE_INICHE_LOG

#include <time.h>

#include "iniche_log_port.h"
#include "iniche_log.h"

#include "in_utils.h"

#ifdef ENABLE_SSL
#include "../ssl/ssl_cfg.h"
#endif

#ifdef SSLDEBUG
extern int sslverbose;
#endif

#include "menu.h"

extern time_t convLocalTimeToString(char *timeBuffer);

log_data_t *p_global_log = NULL;


static char *logTypeStr[LOG_TYPE_MAX_NUM] = 
{
   "NONE", /* LOG_TYPE_NONE */
   "FATAL", /* LOG_TYPE_FATAL */
   "CRITICAL", /* LOG_TYPE_CRITICAL */
   "MAJOR", /* LOG_TYPE_MAJOR */
   "MINOR", /* LOG_TYPE_MINOR */
   "WARNING", /* LOG_TYPE_WARNING */
   "INFO", /* LOG_TYPE_INFO */
   "DEBUG", /* LOG_TYPE_DEBUG */
};

void iniche_free_log(log_data_t **p)
{
   if(*p == NULL)
   {
      return;
   }
#ifdef USE_LOGFILE
   if((*p)->log_fp)
   {
      nv_fclose((*p)->log_fp);
      (*p)->log_fp = NULL;
   }
#endif
   if((*p)->argv)
   {
      LOG_FREE((*p)->argv);
      (*p)->argv = NULL;
   }
   LOG_FREE(*p);
   *p = NULL;
}

int iniche_create_log(log_data_t **p, int log_level, char *fname, void *pio)
{
   int i;
   /* This routine assumes a NULL pointer is passed */
   if(*p == NULL)
   {
      *p = (log_data_t *) LOG_MALLOC(sizeof(log_data_t));
      if(*p == NULL)
      {
         return(-1);
      }
      memset(*p, 0, sizeof(log_data_t));
   }
   else
   {
      for(i = 0; i < LOG_TYPE_MAX_NUM; i++)
      {
         (*p)->log_counts[i] = 0;
      }
   }
   if((log_level >= LOG_TYPE_NONE) && (log_level < LOG_TYPE_MAX_NUM))
   {
      (*p)->log_level = log_level;
   }
   else
   {
      (*p)->log_level = DEFAULT_LOG_LEVEL;
   }
#ifdef USE_LOGFILE
   if(fname)
   {
      strcpy((*p)->log_fname, fname);
   }
   else
   {
      strcpy((*p)->log_fname, LOG_FILE_DEFAULT);
   }
   if((*p)->log_fp)
   {
      nv_fclose((*p)->log_fp);
      (*p)->log_fp = NULL;
   }
   (*p)->log_fp = nv_fopen((*p)->log_fname, "w");
   if((*p)->log_fp == NULL)
   {
      iniche_free_log(p);
      return(-1);
   }
#endif
   (*p)->pio = pio;
   return(0);
}

void log_with_type(log_data_t *p, logTypeEnum log_type, char *pstr, int printf_flag)
{
   char timeBuffer[100];
   if(log_type >  LOG_TYPE_NONE && log_type < LOG_TYPE_MAX_NUM)
   {
      if(p == NULL)
      {
         return;
      }
      if(log_type > p->log_level)
      {
         return;
      }
      p->log_counts[log_type]++;
      if(pstr)
      {
         convLocalTimeToString(timeBuffer);
#ifdef USE_LOGFILE
         if(p->log_fp)
         {
            nv_fprintf(p->log_fp, "%s %lu %s %s\n", logTypeStr[log_type], p->log_counts[log_type], timeBuffer, pstr);
         }
#endif
         if(printf_flag)
         {
            ns_printf(p->pio, "%s %lu %s %s\n", logTypeStr[log_type], p->log_counts[log_type], timeBuffer, pstr);
         }
      }
   }
}   /* log_with_type() */

void glog_with_type(logTypeEnum log_type, char *pstr, int printf_flag)
{
   log_data_t *p = p_global_log;
   log_with_type(p, log_type, pstr, printf_flag);
}   /* glog_with_type() */

int global_log_create(void)
{
   return(iniche_create_log(&p_global_log, DEFAULT_LOG_LEVEL, LOG_FILE_DEFAULT, NULL));
}

void global_log_free(void)
{
   iniche_free_log(&p_global_log);
}

void glog_usage(log_data_t *p)
{
   ns_printf(p->pio, "\n"); 
   ns_printf(p->pio, "usage: glog [options]\n"); 
   ns_printf(p->pio, "options:  [stop]\n"); 
   ns_printf(p->pio, "          [start]\n"); 
   ns_printf(p->pio, "          [info]\n"); 
   ns_printf(p->pio, "          [<module_name> <debug_flag>]\n"); 
   ns_printf(p->pio, "          [-l <log_level = 0, 1, .., 7>]\n");    
#ifdef USE_LOGFILE
   ns_printf(p->pio, "          [-f <file_name>]\n");    
#endif
}

void glog_info(log_data_t *p)
{
   ns_printf(p->pio, "\n"); 
   ns_printf(p->pio, "glog info:\n"); 
   ns_printf(p->pio, "glog level = %d [%s] \n", p->log_level, logTypeStr[p->log_level]);
#ifdef USE_LOGFILE
   ns_printf(p->pio, "glog filename = %s\n", p->log_fname);
#endif
#ifdef SSLDEBUG
   ns_printf(p->pio, "module: ssl: flags = %x\n", sslverbose);
#endif
}

int glog_process_args(log_data_t *p)
{
   if(p->argc < 2)
   {
      glog_usage(p);
      return(0);
   }
   else if(p->argc == 2)
   {
      /* In this case we have a default server on the passed port as a parameter */
      if(strcmp(p->argv[1], "?") == 0)
      {
         glog_usage(p);
         return(0);
      }
      else if(strcmp(p->argv[1], "info") == 0) 
	  {
         glog_info(p);
         return(0);
      }
   }
   if(p->argc >= 2)
   {
      int i = 1; 
      while(i < p->argc)
      {
         if(strcmp(p->argv[i], "start") == 0) 
	     {
            p->start_flag = 1;
            i += 1;
         }
         else if(strcmp(p->argv[i], "stop") == 0) 
	     {
            p->stop_flag = 1;
            i += 1;
         }
         else if(strcmp(p->argv[i], "-l") == 0) 
	     {
            p->log_level = atoi(p->argv[i+1]);
            i += 2;
         }
#ifdef USE_LOGFILE
         else if(strcmp(p->argv[i], "-f") == 0) 
	     {
            strcpy(p->log_fname, p->argv[i+1]);
            i += 2;
         }
#endif
#ifdef SSLDEBUG
         else if(strcmp(p->argv[i], "ssl") == 0) 
	     {
            sslverbose = atoi(p->argv[i+1]);
            i += 2;
         }
#endif
         else
         {
            ns_printf(p->pio, "invalid option: %s\n", p->argv[i]);
            return(-1);
         }
      }
   }   
   return(0);
}

int glog_app(void * pio)
{
   int i;
   char *buf;
   buf = (char *) ((GEN_IO)pio)->inbuf;
   ns_printf(pio,"%s\n", buf);
   if(p_global_log == NULL)
   {
      p_global_log = (log_data_t *) LOG_MALLOC(sizeof(log_data_t));
      if(p_global_log == NULL)
      {
         ns_printf(pio,"glog_app: LOG_MALLOC() failed\n");
         return(-1);
      }
      memset(p_global_log, 0, sizeof(log_data_t));      
      p_global_log->log_level = DEFAULT_LOG_LEVEL;
#ifdef USE_LOGFILE
      strcpy(p_global_log->log_fname, LOG_FILE_DEFAULT);
#endif
   }
   if(p_global_log->argv)
   {
      LOG_FREE(p_global_log->argv);
      p_global_log->argv = NULL;
   }
   p_global_log->argv = parse_args(buf, GLOG_MAX_ARGC, &(p_global_log->argc));
   if(p_global_log->argv == NULL)
   {
      ns_printf(pio, "parse_args() failed for %s\n", buf);
      return(-1);
   }   
#if 0
   for(i = 0; i < p_global_log->argc; i++)
   {
      ns_printf(pio, "%s ", p_global_log->argv[i]);
   }
   ns_printf(pio,"\n");
#endif
   if(glog_process_args(p_global_log))
   {
      ns_printf(pio, "glog_process_args() failed\n");
      return(-1);
   }
   /* Here now we can configure our log app */
   if(p_global_log->stop_flag)
   {
      glog_with_type(LOG_TYPE_INFO, "INICHE LOG stopped", 1);
      iniche_free_log(&p_global_log);
      return(0);
   }
   
   if(p_global_log->start_flag)
   {
#ifdef USE_LOGFILE
      if(iniche_create_log(&p_global_log, p_global_log->log_level, p_global_log->log_fname, pio))
#else
      if(iniche_create_log(&p_global_log, p_global_log->log_level, NULL, pio))
#endif
      {
         ns_printf(pio,"iniche_create_log() failed\n");
      }
      else
      {
         ns_printf(pio,"starting glog:\n");
         glog_with_type(LOG_TYPE_INFO, "INICHE LOG initialized", 1);
      }
      p_global_log->start_flag = 0;
   }
   return 0;
}

#endif /* INCLUDE_INICHE_LOG */

