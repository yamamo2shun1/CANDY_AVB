/*
 * FILENAME: nvparms.c
 *
 * Copyright 1998- 2002 By InterNiche Technologies Inc. All rights reserved
 *
 *
 * MODULE: MISCLIB
 *
 * ROUTINES: get_file_parms(), get_nv_value(), get_nv_params(), 
 * ROUTINES: get_nvif_nets(), get_nvipdec_nets(), get_nvbool_nets(), 
 * ROUTINES: get_nvbool(), get_nvint(), get_nvunshort(), get_nvstring(), 
 * ROUTINES: get_nvipdec(), get_nvdnssrv(), nv_bool(), set_nv_params(), 
 * ROUTINES: inet_nvset(), comport_nvset(), install_nvformat(), 
 * ROUTINES: edit_nv_params(), nv_get_sec_num(), nv_read_parse(), 
 * ROUTINES: nv_add_entry(), nv_del_entry(),  nv_del_entry_byid(), 
 * PORTABLE: yes
 */

/* Additional Copyrights: */

/* nvparms.c 
 * Portions Copyright 1996 by NetPort Software Code for a generic 
 * Non-Volatile parameters system. For the basic demo, we assume nv 
 * parms can just be written to a file. On an embedded system, this 
 * probably needs to be replaced with code to write them to nvram or 
 * flash. The following routines get/set the nvram data from a file 
 * into the single global nv_parms structure. Programmers should read 
 * NV data by calling get_nv_params() and then reading the data 
 * itrems from the structure. Writing NV data is done by setting the 
 * data in the structure and calling set_nv_params(), to ensure 
 * backup to NV storage. Most routines silently return 0 if OK, else 
 * print an error message to console and return -1. 
 */

#include "ipport.h"
#include "libport.h"

#ifdef   INCLUDE_NVPARMS   /* whole file can be ifdeffed out */

#ifndef VFS_FILES
#ifndef HT_LOCALFS
#error Must have VFS to use NV parameters system
#endif   /* HT_LOCALFS */
#endif   /* VFS_FILES  */

#include "vfsfiles.h"
#include "nvparms.h"
#include "ip.h"
#include "in_utils.h"

#include "nvfsio.h"

#ifdef DNS_CLIENT
#include "dns.h"
#endif   /* DNS_CLIENT */

#ifndef MINI_IP
#include "nptcp.h"
#include "socket.h"     /* for inet46_print_addr */
#ifdef IP_V6
#include "socket6.h"    /* for inet_pton */
#endif
#endif /* !MINI_IP */

#if defined (IP_MULTICAST) && (defined (IGMP_V1) || defined (IGMP_V2))
#include "../ipmc/igmp_cmn.h"
char * igmp_bld_err_str = "Build does not support requested IGMP protocol.\n";
char * igmp_bad_prot_err_str = "Unknown IGMP protocol number.\n";
#endif /* IP_MULTICAST and (IGMPv1 or IGMPv2) */ 

char *   nvfilename  = "webport.nv";

/* the IP stack's nvparm structure */
struct inet_nvparam inet_nvparms;

#ifdef USE_COMPORT
struct comport_nvparam comport_nvparms;
#endif   /* USE_COMPORT */

void   nv_bool(char * string, int * boolptr);

/* nvram (actually, file data) get functions */
int get_nvif_nets(struct nvparm_info * nvinfo_ptr, char * parm);
int get_nvipdec_nets(struct nvparm_info * nvinfo_ptr, char * parm);
int get_nvbool_nets(struct nvparm_info * nvinfo_ptr, char* parm);
int get_nvbool(struct nvparm_info * nvinfo_ptr, char* parm);
int get_nvint(struct nvparm_info * nvinfo_ptr, char* parm);
int get_nvunshort(struct nvparm_info * nvinfo_ptr, char* parm);
int get_nvstring(struct nvparm_info * nvinfo_ptr, char* parm);
int get_nvipdec(struct nvparm_info * nvinfo_ptr, char * parm);
int get_nvipv6dec_nets(struct nvparm_info * nvinfo_ptr, char * parm);
int get_nvip46dec(struct nvparm_info * nvinfo_ptr, char * parm);
 
#ifdef DNS_CLIENT
int get_nvdnssrv(struct nvparm_info * nvinfo_ptr, char * parm);
#endif   /* DNS_CLIENT */

/* Non-volatile parameters are read from a disk file and filled into 
 * their runtime tables below. We do this with an array of "nvparm_info" 
 * structures, where each entry has text for a single parameter name, 
 * a parameter type, an optional bound, and a pointer to a variable to 
 * store the parsed data. Each Module has an array of "nvparm_info" 
 * structures which it keeps track of with the "nvparm_format" structure.
 * The "nvparm_format" structure has a pointer to the array of "nvparm_info"'s
 * a count for the size of the array, a pointer to a "set" routine to generate 
 * and initialize a default NV file, and finally a pointer to the next module's
 * "nvparm_format" structure. The get_file_parms() routine is called with the
 * name of the file to parse and a pointer to an "nvparm_format" linked list. 
 * It scans the file line by line, looks up the parameters in each module's
 * "nvparm_format" structure's "nvparm_info" array, and calls the appropriate
 * generic parse function to set the associated variable. 
 */

struct nvparm_info inet_nvformats[] = 
{
   {"   Net interface: %u\n",        NVIF_NETS,    NVBND_NOOP, \
    &inet_nvparms.ifs[0], NULL, },
   /* first five iterate for MAXNETS number of nets */
   {"IP address: %u.%u.%u.%u\n",     NVIPDEC_NETS, NVBND_NOOP, \
    &inet_nvparms.ifs[0], NULL, },
   {"subnet mask: %u.%u.%u.%u\n",    NVSBDEC_NETS, NVBND_NOOP, \
    &inet_nvparms.ifs[0], NULL, },
   {"gateway: %u.%u.%u.%u\n",        NVGTDEC_NETS, NVBND_NOOP, \
    &inet_nvparms.ifs[0], NULL, },
   {"DHCP Client: %s\n",             NVBOOL_NETS,  NVBND_NOOP, \
    &inet_nvparms.ifs[0], NULL, },
#if defined (IP_MULTICAST) && (defined (IGMP_V1) || defined (IGMP_V2))
   {"IGMP mode: %u\n",               NVCHAR_NETS,  NVBND_NOOP, \
    &(inet_nvparms.ifs[0]), NULL},
#endif /* IP_MULTICAST and (IGMPv1 or IGMPv2) */ 

#ifdef DNS_CLIENT
   /* hardcode number of DNS servers, should match MAXDNSSERVERS */
   {"DNS server: 1 - %u.%u.%u.%u\n", NVDNSSRV,     NVBND_NOOP, \
    &inet_nvparms.dns_servers[0], NULL, }, 
   {"DNS server: 2 - %u.%u.%u.%u\n", NVDNSSRV,     NVBND_NOOP, \
    &inet_nvparms.dns_servers[0], NULL, }, 
   {"DNS server: 3 - %u.%u.%u.%u\n", NVDNSSRV,     NVBND_NOOP, \
    &inet_nvparms.dns_servers[0], NULL, }, 
#ifdef DNS_CLIENT_UPDT
   {"DNS zone name: - %s\n",         NVSTRING,     MAX_NVSTRING, \
    &inet_nvparms.dns_zone_name[0], NULL, },
   {"DNS dynamic_update name: 1 - %u.%u.%u.%u\n",  NVDNSSRV,     NVBND_NOOP, \
    &inet_nvparms.dns_update_server, NULL, },
#endif   /* DNS_CLIENT_UPDT */
#endif   /* DNS_CLIENT */
#ifdef IP_V6
   {"IPV6 address: %s\n",        NVIPV6DEC_NETS, NVBND_NOOP, \
    &inet_nvparms.ifs[0], NULL, },
#endif
};

#define NUMINET_FORMATS  \
        (sizeof(inet_nvformats)/sizeof(struct nvparm_info))


#ifdef USE_COMPORT
/* Comport nvparms live here, since SLIP which lives in the .\net directory
 * could be using the comport nvparms.
 */
struct nvparm_info comport_nvformats[] = 
{
   {"Comm port: %c\n"    , NVINT, NVBND_NOOP, &comport_nvparms.comport, NULL, },
   {"line protocol: %s\n", NVINT, NVBND_NOOP, &comport_nvparms.LineProtocol, NULL, },
};

#define NUMCOMPORT_FORMATS  \
        (sizeof(comport_nvformats)/sizeof(struct nvparm_info))

#endif   /* USE_COMPORT */


/* per-port routine to pre-initialize new NV parameters */
void (*set_nv_defaults)(void) = NULL;

#ifdef HT_SYNCDEV
/* per-port routine to write nvfs image to flash part */
void (*nv_writeflash)(void) = NULL;
#endif

char *   IPerr = "Bad IP address format in NV file line %d\nerror: %s\n";

static int netidx = -1; /* index of nets[] to configure */

static int line;      /* line number of input file currently being processed */


char * get_nv_value(char *linebuf,struct nvparm_format *parmformat);

int inet_nvset(NV_FILE * fp);

#ifdef USE_COMPORT
int comport_nvset(NV_FILE * fp);
#endif   /* USE_COMPORT */

#ifdef USE_COMPORT
struct nvparm_format comport_format =
{
   NUMCOMPORT_FORMATS,
   &comport_nvformats[0],
   comport_nvset,
   NULL,
}; 
#endif   /* USE_COMPORT */

struct nvparm_format inet_format =
{
   NUMINET_FORMATS,
   &inet_nvformats[0],
   inet_nvset,
#ifdef USE_COMPORT
   &comport_format,
#else
   (struct nvparm_format *)NULL,
#endif   /* USE_COMPORT */
};

struct nvparm_format *nv_formats = &inet_format;


/* FUNCTION: get_file_parms()
 *
 * get_file_parms() - Open an InterNiche style parameters text file, 
 * read it a line at a time, and call the set functions in the 
 * descriptor array passed. Returns 0 if no error, else one of the 
 * ENP_ errors. 
 *
 * 
 * PARAM1: char * filename
 * PARAM2: struct nvparm_info *parminfo
 * PARAM3: int array_items
 * PARAM4: int *readline
 *
 * RETURNS: 
 */

int
get_file_parms(char * filename,       /* file to read (w/path) */
   struct nvparm_format * parmformat, /* array of parmeter descriptors */
   int * readline)                    /* count of lines read from file */
{
   char  linebuf[NV_LINELENGTH];      /* make sure you have a big stack */
   NV_FILE * fp;                      /* file with nvram data */
   int      retval = 0;               /* value to return from this function */
   char *retstr=NULL;

   fp = nv_fopen(filename, "r");
   if (!fp)
   {
      printf("Unable to open NV Parameters file \"%s\"\n", filename);
      return ENP_NOFILE;
   }

   *readline = 0;                     /* no parameters read yet */

   /* the main file reading loop */
   for ((*readline) = 1; ; (*readline)++)
   {
      if (nv_fgets(linebuf, NV_LINELENGTH, fp) != linebuf)
      {
         retval = 0;
         break;
      }

      retstr=get_nv_value(linebuf,parmformat);

      if (retstr)
      {
         printf("%s %d, file %s\n", retstr,*readline, filename);
         printf("content: %s\n", linebuf);
         printf("Try moving %s to another directory & re-run executable\n",
          filename);
         retval=ENP_PARAM;
         break;
      }
   }
   nv_fclose(fp);
   return retval;
}

/* FUNCTION: get_nv_value()
 *
 * get_nv_value() - Process the line which contains information
 * about the nvparm. General format is <str>: <value>
 * From the <str>, identify the entry in parminfo,
 * and call the set functions in the descriptor array passed. 
 *
 * 
 * PARAM1: char *linebuf - line which is to be processed
 * PARAM2: struct nvparm_info *parminfo
 * PARAM3: int array_items
 *
 * RETURNS: Returns NULL if no error, else error string.
 */

char *
get_nv_value(char *linebuf, struct nvparm_format *parmformat)   
{
   char *   colon_pos;  /* position of colon char ':' in input line */
   char *   lf_pos;     /* position of linefeed in input line */
   int j, err;
   struct nvparm_info * curr_infoptr;

   if (linebuf[0] == '\n' || linebuf[0] == '\r')   /* blank line */
      return NULL; /* skip this line */
   if (linebuf[0] == '#')  /* see if line is commented out */
      return NULL; /* skip this line */
   lf_pos = strchr(linebuf, '\n');     /* find possible linefeed */
   if (lf_pos)       /* if linefeed exists, null it over */
      *lf_pos = '\0';
   lf_pos = strchr(linebuf, '\r');     /* find possible carriage-return */
   if (lf_pos)       /* if carriage-return exists, null it over */
      *lf_pos = '\0';
   /* make sure line has a colon and sanity check colon position */
   colon_pos = strchr(linebuf, ':');
   if (colon_pos < &linebuf[5] || colon_pos > &linebuf[50])
   {
      return "get_nv_params: Missing or displaced colon in line";
   }

   /* search nvparm_format's to figure out which parameter it is */
   while (parmformat && parmformat->info_ptr)
   {
      curr_infoptr = parmformat->info_ptr;

      for (j = 0; j < parmformat->count; j++)
      {
         if (strncmp(linebuf, curr_infoptr->pattern, colon_pos-linebuf) == 0)
         {
            switch(curr_infoptr->nvtype)
            {
            case NVBOOL:
               err = get_nvbool(curr_infoptr, colon_pos+2);
               break;
            case NVINT:
               err = get_nvint(curr_infoptr, colon_pos+2);
               break;
            case NVUNSHORT:
               err = get_nvunshort(curr_infoptr, colon_pos+2);
               break;
            case NVSTRING:
               err = get_nvstring(curr_infoptr, colon_pos+2);
               break;
            case NVIPDEC:
               err = get_nvipdec(curr_infoptr, colon_pos+2);
               break;
            case NVIP46DEC:
               err = get_nvip46dec(curr_infoptr, colon_pos+2);
               break;
#ifdef DNS_CLIENT
            case NVDNSSRV:
               err = get_nvdnssrv(curr_infoptr, colon_pos+2);
               break;
#endif
            case NVIF_NETS:
               err = get_nvif_nets(curr_infoptr, colon_pos+2);
               break;
            case NVIPDEC_NETS:
            case NVSBDEC_NETS:
            case NVGTDEC_NETS:
            case NVCHAR_NETS:
               err = get_nvipdec_nets(curr_infoptr, colon_pos+2);
               break;
#ifdef IP_V6
            case NVIPV6DEC_NETS:
               err = get_nvipv6dec_nets(curr_infoptr, colon_pos+2);
               break;
#endif
            case NVBOOL_NETS:
               err = get_nvbool_nets(curr_infoptr, colon_pos+2);
               break;
            default:    /* proprietary nvtype or is it unknown nvtype ? */
               if (curr_infoptr->nvtype >= 100 && curr_infoptr->nvtype <=200)
               {
                  if (curr_infoptr->get_nvprop_type)
                  {
                     err = curr_infoptr->get_nvprop_type(curr_infoptr, colon_pos+2);
                  }
                  else
                  {
                     printf("Expecting pointer to proprietary nvparm type ");
                     printf("parse function.\n");
                     printf("Please check configuration of the nvparm_info ");
                     printf("for the pattern = %s\n", curr_infoptr->pattern);
                     dtrap();
                     err = 1;
                  }
               }
               else
               {
                  dprintf("get_nv_value: unknown nvtype in nvparm_format %d\n", \
                          curr_infoptr->nvtype );
                  err = 1;
               }
               break;
            }

            if (err)
               return "error setting value in line";
            else
               return NULL;
         }
         curr_infoptr++;
      }
      parmformat = parmformat->next_format;
   }
   return "Unknown NV parameter string or format in line" ;
}

/* FUNCTION: get_nv_params()
 *
 * get_nv_params() - Read BV params from file or flash into NV 
 * streucture. This is usually called from main()/netmain() Returns: 
 * silently return 0 if OK, else print an error message to console 
 * and return -1. 
 *
 * 
 * PARAM1: 
 *
 * RETURNS: 
 */

int
get_nv_params()
{
   int   e;
   NV_FILE * fp;

   /* Just in case Demo Applications want to parse webport.nv again */
   netidx = -1; /* reset index of nets[] to configure */

   /* invoke the generic parm reader to get TCP/IP system parameters */
   e = get_file_parms(nvfilename, nv_formats, &line);
   if (e == ENP_NOFILE)
   {
      printf("Creating sample file\n");
      fp = nv_fopen(nvfilename, "w+");
      if (fp)
      {
         if (set_nv_defaults)   /* see if app wants to init defaults */
            set_nv_defaults();
         set_nv_params(NULL);   /* write defaults to new file */

         /* now try again from the flash file we just set up */
         e = get_file_parms(nvfilename, nv_formats, &line);
      }
      else
         printf("Can't create sample file either.\n");
   }
   return(e);
}




/* FUNCTION: getnet()
 *
 * get_nvif_nets() - get number of next interface to set up
 * 
 * Networks in the .nv file can be indexed by ones based numbering, zero 
 * based numbering, or names. On the first net we figure out which, then 
 * enforce the system on the remaining nets.
 *
 * PARAM1: char * parm
 * PARAM2: struct nvparm_info * nvinfo_ptr
 *
 * RETURNS: 0 if OK, else -1
 */

static enum { 
   ZERO_BASED, ONES_BASED, NAME_BASED 
} iftype = ZERO_BASED;     /* default... */

int
get_nvif_nets(struct nvparm_info * nvinfo_ptr, char * parm)
{
   int   newnet;

   if(netidx == -1)  /* first net in nv file */
   {
      /* see if net parameter is name or number */
      if((*parm >= '0') && (*parm <= '9'))
      {
         /* see if first number is zero or one. */
         if(*parm == '0')
            iftype = ZERO_BASED;
         else if(*parm == '1')
            iftype = ONES_BASED;
         else
         {
            printf("First Net number must be 0 or 1");
            return -1;
         }
      }
      else
         iftype = NAME_BASED;
   }

   switch(iftype)
   {
   case ONES_BASED:
      newnet = atoi(parm);
      if (newnet < 1 || newnet > MAXNETS)
      {
         printf("Net numbers must be 1-%d", MAXNETS);
         return -1;
      }
      newnet--;      /* convert to zero based C array index */
      goto check_nextnet;
   case ZERO_BASED:
      newnet = atoi(parm);
      if (newnet < 0 || newnet >= MAXNETS)
      {
         printf("Net numbers must be 0-%d", MAXNETS - 1 );
         return -1;
      }
check_nextnet:
      if (newnet != (netidx + 1))
      {
         printf("next net should be %d", netidx + 1);
         return -1;
      }
      netidx = newnet;
      /* set ones based index number as textual name */

      *(((struct ifinfo *)(nvinfo_ptr->nvdata) + netidx)->name) = \
      (char)('1' + netidx);   

      break;
   case NAME_BASED:
      if(isdigit(*parm))
      {
         printf("Net must be all single digits or all names");
         printf("which don't start with a digit\n");
         return -1;
      }
      if(strlen(parm) >= IF_NAMELEN)
      {
         printf("interface names must be shorter than %d chars", IF_NAMELEN);
         return -1;
      }
      netidx++;   /* bump index to next ifs entry */
      /* copy in name */
      strncpy(((struct ifinfo *)(nvinfo_ptr->nvdata) + netidx)->name, parm, \
              IF_NAMELEN - 1);
      break;
   }
   return 0;   /* OK return */
}

/* IP address options */


/* FUNCTION: get_nvipdec_nets()
 *
 * getip - get Internet ip address for the NETS array
 *
 * 
 * PARAM1: char * parm
 *
 * RETURNS: 
 */

int 
get_nvipdec_nets(struct nvparm_info * nvinfo_ptr, char * parm)
{
   char *   cp;
   unsigned subnet;     /* dummy for passing to parse_ipad() */

   switch(nvinfo_ptr->nvtype)
   {
   case NVIPDEC_NETS:
      cp = parse_ipad((ip_addr *)&(((struct ifinfo *)(nvinfo_ptr->nvdata) + \
                      netidx)->ipaddr), &subnet, parm);
      break;
   case NVSBDEC_NETS:
      cp = parse_ipad((ip_addr *)&(((struct ifinfo *)(nvinfo_ptr->nvdata) + \
                      netidx)->subnet), &subnet, parm);
      break;
   case NVGTDEC_NETS:
      cp = parse_ipad((ip_addr *)&(((struct ifinfo *)(nvinfo_ptr->nvdata) + \
                      netidx)->gateway), &subnet, parm);
      break;
#if defined (IP_MULTICAST) && (defined (IGMP_V1) || defined (IGMP_V2))
   case NVCHAR_NETS:
      {
      struct ifinfo * ifip;
      u_char cval;

      ifip = ((struct ifinfo *) (nvinfo_ptr->nvdata)) + netidx;
      /* extract the IGMP operating mode (1 for v1, 2 for v2) from 
       * configuration file */
      cval = (u_char) atoi (parm);

      switch (cval)
      {
      case IGMP_MODE_V1:
#ifdef IGMP_V1
         cp = 0;
         ifip->igmp_oper_mode = cval;
#else
         /* return an error if the configuration file specifies
          * support for IGMPv1, but the protocol is not compiled
          * into the build.
          */
         cp = igmp_bld_err_str;
#endif
         break;
      case IGMP_MODE_V2:
#ifdef IGMP_V2
         cp = 0;
         ifip->igmp_oper_mode = cval;
#else
         /* return an error if the configuration file specifies
          * support for IGMPv2, but the protocol is not compiled
          * into the build.
          */
         cp = igmp_bld_err_str;
#endif
         break;
      default:
         /* indicate failure if mode is not one of the two expected 
          * values (1 for IGMPv1, 2 for IGMPv2) */
         cp = igmp_bad_prot_err_str;
         break;
      } /* end SWITCH */

      if (!cp) 
         printf ("interface: %s, cval: %u, netidx: %u\n", ifip->name, cval, netidx);
      }
      break;
#endif /* IP_MULTICAST and (IGMPv1 or IGMPv2) */ 
   default:   /* Bad programming ? */
      dprintf("get_nv_value: unknown nvtype in nvparm_format %d\n", \
               nvinfo_ptr->nvtype );
      return -1;
   }

   if (cp)
   {
      printf(IPerr, line, cp);
      return -1;
   }
   return 0;
}

#ifdef IP_V6
/* IPV6 address options */


/* FUNCTION: get_nvipv6dec_nets()
 *
 * getip - get Internet ip address for the NETS array
 *
 * 
 * PARAM1: char * parm
 *
 * RETURNS: 
 */

int 
get_nvipv6dec_nets(struct nvparm_info * nvinfo_ptr, char * parm)
{
   int stat;

   switch(nvinfo_ptr->nvtype)
   {

   case NVIPV6DEC_NETS:
      stat = inet_pton(AF_INET6, (const char *)parm, 
                   (ip6_addr *)&((struct ifinfo *)(nvinfo_ptr->nvdata) + \
                   netidx)->ipv6addr );
      break;

   default:   /* Bad programming ? */
      dprintf("get_nv_value: unknown nvtype in nvparm_format %d\n", \
               nvinfo_ptr->nvtype );
      return -1;
   }

   if (stat)
   {
      printf(IPerr, line, parm);
      return -1;
   }
   return 0;
}

#endif   /* IP_V6 */

/* FUNCTION: get_nvbool_nets()
 *
 * Parse boolean parameters for the NETS Array.
 *
 * Right now we only use this to check if the DHCP client
 * is enabled on the iface. Can be extended for other Nets
 * boolean variables.
 * 
 * PARAM1: char* parm
 *
 * RETURNS:
 */

int
get_nvbool_nets(struct nvparm_info * nvinfo_ptr, char* parm)
{
   nv_bool(parm, &(((struct ifinfo *)(nvinfo_ptr->nvdata) + \
                   netidx)->client_dhcp));
   return 0;
}

#ifdef DNS_CLIENT

/* FUNCTION: get_nvdnssrv()
 * 
 * PARAM1: struct nvparm_info * nvinfo_ptr
 * PARAM2: char * parm
 *
 * RETURNS: 
 */

int 
get_nvdnssrv(struct nvparm_info * nvinfo_ptr, char * parm)
{
   char *   cp;
   unsigned subnet;     /* dummy for passing to parse_ipad() */
   int   svr_num;

   svr_num = atoi(parm);
   if (svr_num < 1 || svr_num > MAXDNSSERVERS)
   {
      printf("Error in line %d; DNS server number must be 1-%d\n",
       line, MAXDNSSERVERS);
      return -1;
   }
   parm += 4;  /* point to IP address field */

   cp = parse_ipad((ip_addr *)nvinfo_ptr->nvdata + (svr_num - 1), \
                   &subnet, parm);

   if (cp)
   {
      printf(IPerr, line, cp);
      return -1;
   }
   return 0;
}
#endif   /* DNS_CLIENT */

/* FUNCTION: get_nvbool()
 *
 * 
 * PARAM1: char* parm
 *
 * RETURNS: 
 */
int
get_nvbool(struct nvparm_info * nvinfo_ptr, char* parm)
{
   nv_bool(parm, (int *)(nvinfo_ptr->nvdata));
   return 0;
}


/* FUNCTION: get_nvint()
 * 
 * PARAM1: char* parm
 *
 * RETURNS: 
 */

int
get_nvint(struct nvparm_info * nvinfo_ptr, char* parm)
{
   *(int *)nvinfo_ptr->nvdata = (int)atoi(parm);
   return 0;
}


/* FUNCTION: get_nvunshort()
 * 
 * PARAM1: char* parm
 *
 * RETURNS: 
 */

int
get_nvunshort(struct nvparm_info * nvinfo_ptr, char* parm)
{
   *(unshort *)nvinfo_ptr->nvdata = (unshort)atoi(parm);
   return 0;
}

/* FUNCTION: get_nvstring()
 * 
 * PARAM1: char* parm
 *
 * RETURNS: 
 */

int
get_nvstring(struct nvparm_info * nvinfo_ptr, char* parm)
{
   strncpy((char *)(nvinfo_ptr->nvdata), parm, nvinfo_ptr->nvbound);
   return 0;
}


/* FUNCTION: get_nvipdec()
 *
 * get_nvipdec - get Internet ip address
 *
 * 
 * PARAM1: char * parm
 *
 * RETURNS: 
 */
int 
get_nvipdec(struct nvparm_info * nvinfo_ptr, char * parm)
{
   char *   cp;
   unsigned subnet;   /* dummy for passing to parse_ipad() */

   cp = parse_ipad((ip_addr *)(nvinfo_ptr->nvdata), &subnet, parm);
   if (cp)
   {
      printf(IPerr, line, cp);
      return -1;
   }
   return 0;
}

/* FUNCTION: get_nvip46dec()
 *
 * get_nvip46dec - get Internet IPv4/IPv6 address
 * 
 * PARAM1: char * parm
 *
 * RETURNS: 
 */


int 
get_nvip46dec(struct nvparm_info * nvinfo_ptr, char * parm)
{
#ifndef MINI_IP
   return inet46_addr(parm, (struct sockaddr *)nvinfo_ptr->nvdata);
#else
   return -1;
#endif /* !MINI_IP */
}


/* FUNCTION: set_nv_params()
 * 
 * PARAM1: void * pio
 *
 * RETURNS: 
 */

int
set_nv_params(void * pio)
{
   NV_FILE * fp;        /* file with nvram data */
   struct nvparm_format * curr_format = nv_formats;

   fp = nv_fopen(nvfilename, "w");
   if (!fp)
   {
      ns_printf(pio, "Unable to open NV Parameters file \"%s\"\n", nvfilename);
      return -1;
   }

   while (curr_format)
   {
      curr_format->setfunc(fp);
      curr_format = curr_format->next_format;
   }

   nv_fclose(fp);

#ifdef HT_SYNCDEV
   if (nv_writeflash)   /* make sure pointer is set */
      nv_writeflash();  /* call optional per-port flash write routine */
#endif

   return 0;
}

/* FUNCTION: inet_nvset()
 * 
 * PARAM1: NV_FILE * fp
 *
 * RETURNS: Silent return of 0 for OK
 */
int inet_nvset(NV_FILE * fp)
{
int j=0;
struct l2b ip;  /* structure for IP address conversions */
int iface;      /* nets[] index */

   /* fill in nvparms from set active values */

   /* IP addressing parameters and IGMP mode section: */

   for (iface = 0; iface < MAXNETS; iface++)
   {
      j = 0;    /* inet_nvformats element currently being witten */
      nv_fprintf(fp, inet_nvformats[j++].pattern, iface);

      ip.ip.iplong = inet_nvparms.ifs[iface].ipaddr;
      nv_fprintf(fp, inet_nvformats[j++].pattern,  ip.ip.ipchar[0], 
                 ip.ip.ipchar[1], ip.ip.ipchar[2], ip.ip.ipchar[3]);

      ip.ip.iplong = inet_nvparms.ifs[iface].subnet;
      nv_fprintf(fp, inet_nvformats[j++].pattern,  ip.ip.ipchar[0], 
                 ip.ip.ipchar[1], ip.ip.ipchar[2], ip.ip.ipchar[3]);

      ip.ip.iplong = inet_nvparms.ifs[iface].gateway;
      nv_fprintf(fp, inet_nvformats[j++].pattern,  ip.ip.ipchar[0], 
                 ip.ip.ipchar[1], ip.ip.ipchar[2], ip.ip.ipchar[3]);

      nv_fprintf(fp, inet_nvformats[j++].pattern, 
                 (inet_nvparms.ifs[iface].client_dhcp)?"YES":"NO");

#if defined (IP_MULTICAST) && (defined (IGMP_V1) || defined (IGMP_V2))
      nv_fprintf(fp, inet_nvformats[j++].pattern, 
                 inet_nvparms.ifs[iface].igmp_oper_mode);
#endif /* IP_MULTICAST and (IGMPv1 or IGMPv2) */
   }

#ifdef DNS_CLIENT
   {
      int   i;
      for (i = 0; i < MAXDNSSERVERS; i++)
      {
         ip.ip.iplong = inet_nvparms.dns_servers[i];
         nv_fprintf(fp, inet_nvformats[j++].pattern,  ip.ip.ipchar[0], 
                    ip.ip.ipchar[1], ip.ip.ipchar[2], ip.ip.ipchar[3]);
      }
   }

#ifdef DNS_CLIENT_UPDT
   nv_fprintf(fp, inet_nvformats[j++].pattern, inet_nvparms.dns_zone_name);
#endif   /* DNS_CLIENT_UPDT */

#endif   /* DNS_CLIENT */

   return 0;
}

#ifdef USE_COMPORT
/* FUNCTION: comport_nvset()
 * 
 * PARAM1: NV_FILE * fp
 *
 * RETURNS: Silent return of 0 for OK
 */
int comport_nvset(NV_FILE * fp)
{
int i = 0;

   if (comport_nvparms.comport != '1'  && comport_nvparms.comport !='2' )
      comport_nvparms.comport='1';

   nv_fprintf(fp, comport_nvformats[i++].pattern, comport_nvparms.comport);

   nv_fprintf(fp, comport_nvformats[i++].pattern, 
              (comport_nvparms.LineProtocol == PPP) ? "PPP":"SLIP");

   return 0;
}
#endif   /* USE_COMPORT */

/* FUNCTION: nv_bool()
 *
 * set a passed boolean variable from a passed string
 * 
 * PARAM1: char * string
 * PARAM2: int * boolptr
 *
 * RETURNS: 
 */

void
nv_bool(char * string, int * boolptr)
{
   if (stricmp(string, "yes") == 0)
      *boolptr = TRUE;
   else if(stricmp(string, "true") == 0)
      *boolptr = TRUE;
   else
      *boolptr = FALSE;
}

/* FUNCTION: edit_nv_params()
 * Edit the value of the param in the nvram structure.
 * This function allows any (well almost) nvram parameter to be
 * configured from the command prompt. 
 * Usage:
 *    nvedit <whole line as it appears in webport.nv file>
 *    nvedit <str>: <value>
 * Example:
 *    nvedit PPP Console Logging: YES
 * 
 * PARAM1: void * pio
 *
 * RETURNS: 
 */

extern   char *   nextarg(char*); /* get next arg from a string */

int
edit_nv_params(void * pio)
{
   char *retstr=NULL;
   char *   cp;

   /* see if user put name on cmd line */
   cp = nextarg(((GEN_IO)pio)->inbuf);
   if (!cp || !*cp)
   {
      ns_printf(pio, "usage: nvedit <name of variable>: <value>\n");
      return -1;
   }

   retstr = get_nv_value(cp, nv_formats);
   if (retstr)
   {
      ns_printf(pio, retstr);
      return -1;
   }

   return 0;
}

/* FUNCTION: install_nvformat()
 *
 * install_nvformat() - Link a new nvparm_format structure to the end 
 * of a list of nvparm_format structures.
 *
 * PARAM1: struct nvparm_format * new_nvformat
 * PARAM2: struct nvparm_format * head_nvformat
 *
 * RETURNS: 0 for OK, -1 for Error.
 */

int
install_nvformat(struct nvparm_format * new_nvformat, 
                 struct nvparm_format * head_nvformat)
{
struct nvparm_format * curr_format = head_nvformat;

   if (!curr_format || !new_nvformat)
   {
      dprintf("Bad Call to install_nvformat\n");
      return -1;
   }

   while (curr_format->next_format)
      curr_format = curr_format->next_format;

   curr_format->next_format = new_nvformat;
   return 0;
}


/* Include genlist.h for defn of NICHE_DUP_ENTRY, addition, deletion
 * of entries in the generic list. */

#include "genlist.h" 


/* FUNCTION: nv_get_sec_num()
 * 
 * Get the index for the corresponding section.
 *
 * PARAM1: char *buf    - buf points to a section name. 
 * PARAM2: struct nv_sectioninfo *sec - array contain info about sections
 * PARAM3: int slen     - len of sec[]
 *
 * RETURNS: Index to section array or NOT_A_SECTION 
 */

int
nv_get_sec_num(char * buf, struct nv_sectioninfo *sec, int slen)
{
   int   index,sec_index=NOT_A_SECTION;

   for (index=0; index < slen ; index++ )
   {
      if (strncmp(buf, sec[index].name, strlen(sec[index].name)) == 0)
      {
         sec_index = index ;
         break;
      }
   }
   if ( index == slen )  /* Match not found */
   {
      /* There is some unknown/disabled section. Ignore it */
      sec_index = NOT_A_SECTION ;
   }
   return sec_index ;
}

/* FUNCTION: nv_read_parse()
 * 
 * Read in the non-volative parameters from a disk or flash file.
 *
 * PARAM1: char *fname  - name of file to be read
 * PARAM2: struct nv_sectioninfo *sec - list of sections and their info.
 * PARAM3: int slen  - len of sec[] array
 *
 * RETURNS: SUCCESS or error number

 * ALGORITHM : Following algo. will be used for parse the whole NV file.
 *
 * 1. Read the whole file line by line.
 * 2. If there is any error, then return with error code
 * 3. For each line read
 *    a. skip if it is blank line or a comment line
 *    b. If it starts with "[", then update sec_index. "sec_index" is
 *       an index into the sec[] array. Initially it is set to
 *       NOT_A_SECTION. When a match occurs, it will index the respective
 *       section in sec array. As in C language, indices for 
 *       sec[] start with 0.
 *    c. If it doen't start with "[", check sec_index
 *       a. If sec_index is NOT_A_SECTION, then continue
 *       b. Otherwise call the parse_routine for the section indexed
 *          by sec_index.
 * 4. Close the file and return.
 *
 */

/* to be able to read long lines of ipfilter.nv, defined NV_LONGLINE */
#define NV_LONGLINE  (NV_LINELENGTH+20)

int nv_read_parse(char *fname, struct nv_sectioninfo *sec,int slen)
{
   char * cp;                    /* scratch */
   FILE * fp;                    /* input file */
   unsigned line;                 /* current line in input file */
   int sec_index= NOT_A_SECTION; /* init to not in a list */
   int ret_code = SUCCESS;
   static char linebuf[NV_LONGLINE]; /* scratch local buffer for file reading */

   fp = (FILE *)nv_fopen(fname, "r");
   if (!fp)
   {
      return ENP_NOFILE;
   }

   line = 0;
   while (nv_fgets(linebuf, NV_LONGLINE, fp) == linebuf)
   {
      line++;      
      if ((linebuf[0] == '#') || /* see if line is commented out */
          (linebuf[0] == ' ') ||  /* or starts with whitespace */
          (linebuf[0] == '\t') || /* or starts with whitespace */
          (linebuf[0] == '\n') || /* or is empty */
          (linebuf[0] == '\r'))   /* or is empty */
      {
         continue;
      }
      cp = strchr(linebuf, '\n');   /* find possible linefeed */
      if (cp)     /* if linefeed exists, null it over */
         *cp = '\0';

      cp = strchr(linebuf, '#');    /* find possible in-line comment */
      if (cp)     /* if comment char exists, null it over */
         *cp = '\0';

      if (linebuf[0] == '[')
      {
         sec_index = nv_get_sec_num(&linebuf[1],sec,slen);
         continue;
      }

      /* fall to here if linebuf should contain a record */
      if (sec_index == NOT_A_SECTION)
         continue;
      else 
      {
         ret_code = sec[sec_index].parse_func(linebuf);
         if (ret_code != SUCCESS )
         {
            dprintf("Error #%d (line %d of %s)\n",ret_code,line,fname);
            if ( ret_code == NICHE_DUP_ENTRY )
               continue;
            else
            {
               nv_fclose(fp);
               return ret_code;
            } 
         }
      }
   }

   if (line < 1)
   {
      ret_code  = ENP_PARAM ;
   }

   nv_fclose(fp);
   return ret_code;
}

#ifdef USE_GENLIST

/* FUNCTION: nv_add_entry()
 * 
 * Add an entry to a table. The table is found from
 * the index'th entry in sec[].  
 *
 * Here is the generic idea
 * - Same parsing function is used to parse an entry from NV file
 *   and parse the user command on cmdline. 
 * - Hence the same nv_sectioninfo struct is used to reference the
 *   parsing function.
 * - When user enters a cmd (on cmdline) to add an entry to table,
 *   nv_add_entry() can be used to parse the info and add it to the table.
 * - Similarly, user enters a cmd (on cmdline) to del an entry in table,
 *   nv_del_entry() can be used to delete the entry in the table.
 *
 * PARAM1: void *pio - Pointer to GenericIO structure
 * PARAM2: int index - Index in sec[] array.
 * PARAM3: struct nv_sectioninfo * sec
 *
 * RETURNS: SUCCESS or error code.
 */

int 
nv_add_entry(void * pio, int index, struct nv_sectioninfo *sec)
{
   char *   cp;
   int   err;

   if ( index == NOT_A_SECTION )
   {
      ns_printf(pio,"Could not find info about table. Can't process cmd.\n");
      return -1;
   }

   cp = nextarg( ((GEN_IO)pio)->inbuf );  /* see if user put parm on cmd line */
   if (!*cp)
   {
      if ( sec[index].usage_func )
         sec[index].usage_func(pio);
      return -1;
   }

   err=sec[index].parse_func(cp);

   if ( err == SUCCESS )
      return SUCCESS;
   else
   {
      ns_printf(pio,"Error #%d\n",err);
      return -1;
   }
}

/* FUNCTION: nv_del_entry()
 * 
 * Delete an entry (row) from a table. The information
 * about the table is given by the entry in section table.
 *
 * The information on the cmdline, is the index. So if its "5", then
 * index is 5 (0 based index) and hence the 6th entry/row in the table
 * is to be deleted.
 *
 * Example :
 * Say for deleting an entry in SNMPv3 group table, user enters the
 * following command on the command line
 *    "v3delgroup 3"
 * v3_del_group() gets called with inbuf member of pio pointing to the cmdline.
 * It calls nv_del_entry(). It passes a ptr to sec[] and corresponding
 * index into this array. Hence sec[index] contains information about
 * the table (SNMpv3 group table in this case) to be modified. 
 * nv_del_entry() parses the cmdline, that is pio->inbuf, and then 
 * calls niche_del() to delete the 4th entry/row in the SNMPv3 group table.
 *
 * PARAM1: void *pio
 * PARAM2: int index
 * PARAM3: struct nv_sectioninfo * sec
 * 1. Pointer to GenericIO structure
 * 2. Index in sec[] array.
 * 3. Pointer to sec[] array
 *
 * RETURNS: SUCCESS or error code.
 */

int 
nv_del_entry(void * pio, int index, struct nv_sectioninfo *sec)
{
   GEN_STRUCT entry;
   char *   cp;

   if ( index == NOT_A_SECTION )
   {
      ns_printf(pio,"Could not find info about table. Can't process cmd.\n");
      return -1;
   }

   cp = nextarg( ((GEN_IO)pio)->inbuf );  /* see if user put parm on cmd line */
   if (!*cp)
   {
      if ( sec[index].usage_func )
         sec[index].usage_func(pio);
      return -1;
   }

   entry = niche_list_getat((NICHELIST)sec[index].list,atoi(cp));
   if ( entry )
   {
      niche_del((NICHELIST)sec[index].list,entry);
      return SUCCESS;
   }
   else
      return -1;
}

/* FUNCTION: nv_del_entry_byid()
 * 
 * Delete an entry (row) from a table. The information
 * about the table is given by the entry in section table.
 *
 * The information on the cmdline is "id" of entry.
 * Hence the entry in the table with similar "id" needs to be deleted.
 *
 * PARAM1: void *pio
 * PARAM2: int index
 * PARAM3: struct nv_sectioninfo * sec
 * 1. Pointer to GenericIO structure
 * 2. Index in sec[] array.
 * 3. Pointer to sec[] array
 *
 * RETURNS: SUCCESS or error code.
 */

int 
nv_del_entry_byid(void * pio, int index, struct nv_sectioninfo *sec)
{
   char *   cp;

   if ( index == NOT_A_SECTION )
   {
      ns_printf(pio,"Could not find info about table. Can't process cmd.\n");
      return -1;
   }

   cp = nextarg( ((GEN_IO)pio)->inbuf );  /* see if user put parm on cmd line */
   if (!*cp)
   {
      if ( sec[index].usage_func )
         sec[index].usage_func(pio);
      return -1;
   }

   return niche_del_id((NICHELIST)sec[index].list,atoi(cp));
}


#endif /* USE_GENLIST */

#endif   /* INCLUDE_NVPARMS */

