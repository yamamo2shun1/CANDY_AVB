/*
 * FILENAME: iface.c
 *
 * Copyright  2000 By InterNiche Technologies Inc. All rights reserved
 *
 *  Generic interface support routines, including dynamic interface API.
 *
 * MODULE: INET
 *
 * ROUTINES: ni_create(), ni_delete(), ni_get_config(), ni_set_config()
 * ROUTINES: new_iface(), del_iface(), if_listifs(), if_config(),
 * ROUTINES: if_getbyname(), if_enable(), if_disable(), if_killsocks(),
 * ROUTINES: ni_set_state(), if_getbynum(), isbcast(), reg_type(),
 *
 * PORTABLE: yes
 */

/* Additional Copyrights: */
/* Portions Copyright 1994 by NetPort Software */

#include "ipport.h"
#include "q.h"
#include "netbuf.h"
#include "net.h"
#include "ip.h"

#ifdef INCLUDE_ARP
#include "arp.h"
#endif

#ifdef INCLUDE_TCP
#ifdef MINI_TCP
#include "mtcp.h"
#else
#include "tcpport.h"
#include "../tcp/in_pcb.h"
#endif /* MINI_TCP */
#endif   /* INCLUDE_TCP */

#ifdef IN_MENUS   /* need this for dynamic iface test menu */
#include "menu.h"
NET   if_getbypio(void * pio);   /* export for other modules */
#endif


queue netlist;          /* master list of nets, static & dynamic */

#ifdef DYNAMIC_IFACES   /* create and delete can be ifdeffed out */

#ifndef PLLISTLEN    /* allow override from ipport.h */
#define PLLISTLEN 4  /* numer of protocols MACs can support */
#endif

/* master list of all MAC level protocols (IP, ARP, etc) */
unshort  protlist[PLLISTLEN]; /* zero means unused entry */


#ifndef NULLIP
#define  NULLIP   0L
#endif

void if_killsocks(NET ifp);

/* FUNCTION: ni_create()
 *
 * This creates a new dynamic interface structure. Once the memory struct
 * and linked list are set up, the passed callback is called to set up
 * hardware specific parameters like MTU size and callbacks - similar to
 * what the "prep_" routine does for a static device. The various
 * MAC protocols used by the system are registered with the device via
 * the reg_typew and the device is then configured as UP.
 *
 * PARAM1: NET * newifp
 * PARAM2: int (*create_device)(NET newifp, void * bindinfo)
 * PARAM3: char * name
 * PARAM4: void * bindinfo
 *
 * RETURNS: Returns 0 if OK, or an ENP_ error code. The address of the
 * new structure is returned in the passed "newifp" parameter
 */

int
ni_create(NET * newifp,    /* OUT - created handle */
          int (*create_device)(NET newifp, void * bindinfo),   /* IN */
          char * name,     /* IN */
          void * bindinfo) /* IN */
{
   int   i;
   int   err;
   int   iface;      /* index into nets[], for legacy support */
   struct net *   ifp;
   struct IfMib * mib;

   iface = netlist.q_len;  /* index to new interface */

   /* Make sure we have spare slots in various net arrays */
   if(iface >= MAXNETS)
      return ENP_RESOURCE;

   /* first allocate the memory */
   ifp = (struct net *)NET_ALLOC(sizeof(struct net));
   if (!ifp)
      return ENP_NOMEM;
   mib = &ifp->mib;   /* set MIB pointer for backward compatability */

   /* set up pointers - add net to list */
   putq(&netlist, ifp);
   ifp->n_mib = mib;

   /* support the old style nets[] array */
   nets[iface] = ifp;

   ifp->n_flags = NF_DYNIF;      /* set flags field */
   if (name != NULL)
   {
      MEMCPY(ifp->name, name, IF_NAMELEN-1);
      ifp->name[IF_NAMELEN-1] = 0;
   }
   err = (*create_device)(ifp, bindinfo);
   if (err)
      goto bad_create;

   /* don't allow the max header or MTU size to grow. If this is
    * a problem then set the MaxXxx ints at init time *BEFORE* 
    * calling NetInit() or ip_startup().
    */
   if ((ifp->n_lnh > MaxLnh) ||
       (ifp->n_mtu > MaxMtu))
   {
      dtrap();    /* programming, see note above */
      err = ENP_LOGIC;
      goto bad_create;
   }

   if (ifp->n_reg_type)    /* do we need to register types? */
   {
      for (i = 0; i < PLLISTLEN; i++)
      {
         if (protlist[i] == 0)
            break;
         ifp->n_reg_type(protlist[i], ifp);
      }
   }

   err = ni_set_state(ifp, NI_UP);
   if (err)
      goto bad_create;

   /* interface is created, no error */
   ifNumber = netlist.q_len;
   *newifp = ifp;
   return 0;   /* OK return */

bad_create:
   /* user create routine returned an error. */
   qdel(&netlist, (void*)ifp);      /* delete from list */
   NET_FREE(ifp);
   nets[iface] = NULL;
   return err;
}


/* FUNCTION: ni_get_config()
 *
 * get IP address and subnet for a particular device.
 *
 * PARAM1: NET ifp
 * PARAM2: struct niconfig_0 * cfg
 *
 * RETURNS: 0
 */

int
ni_get_config(NET ifp, struct niconfig_0 * cfg)
{
   cfg->n_ipaddr = ifp->n_ipaddr;
   cfg->snmask = ifp->snmask;

   return 0;      /* OK return */
}


/* FUNCTION: ni_set_config()
 * 
 * set IP address and subnet for a particular device.
 *
 * PARAM1: NET ifp
 * PARAM2: struct niconfig_0 * cfg
 *
 * RETURNS: 0
 */

int
ni_set_config(NET ifp, struct niconfig_0 * cfg)
{
   int   err;

   err = ni_set_state(ifp, NI_DOWN);   /* shut down iface around this...*/
   if(err)
      return err;

   ifp->n_ipaddr = cfg->n_ipaddr;      /* set passed info */
   ifp->snmask = cfg->snmask;

   /* build broadcast addresses */
   if(ifp->n_ipaddr != 0)
   {
      ifp->n_netbr = ifp->n_ipaddr | ~(ifp->snmask);
      ifp->n_netbr42 = ifp->n_ipaddr & ifp->snmask;
      ifp->n_subnetbr = ifp->n_ipaddr | ~(ifp->snmask);
   }

   err = ni_set_state(ifp, NI_UP);     /* restart iface */

   return err;
}


/* FUNCTION: ni_delete()
 * 
 * PARAM1: NET ifp
 *
 * RETURNS: 
 */

int
ni_delete(NET ifp)
{
   int iface;

   if ((ifp->n_flags & NF_DYNIF) == 0) /* not a dynamic iface */
      return ENP_PARAM;

   ni_set_state(ifp, NI_DOWN);            /* mark iface as down */

   if (qdel(&netlist, (void*)ifp)==NULL)  /* delete from list */
      return ENP_PARAM;          /* interface not in queue */

   /* remove ifp from nets[], filling it's hole */
   for(iface = 0; iface < MAXNETS; iface++)
   {
      if(nets[iface] == ifp)     /* found ifp to delete */
      {
         /* move following nets[] entrys up 1 slot to cover deleted net */
         for( ;  iface < (MAXNETS - 1); iface++)
            nets[iface] = nets[iface + 1];
         break;
      }
   }

   ifNumber = netlist.q_len;     /* fix global if c   ount */
   NET_FREE(ifp);

   return 0;   /* no way this can fail? */
}

#ifdef IN_MENUS   /* need this for dynamic iface test menu */

int   new_iface(void * pio);
int   del_iface(void * pio);
int   if_enable(void * pio);
int   if_disable(void * pio);
int   if_listifs(void * pio);
int   if_config(void * pio);

struct menu_op dynif_menu[8]   =  {
   "dynif",  stooges, "Dynamic Interface menu" ,    /* menu ID */
   "nicreate",    new_iface,     "Create new network interface",
   "nidelete",    del_iface,     "Delete network interface",
   "niup",        if_enable,     "Enable net interface",
   "nidown",      if_disable,    "Disable net interface",
   "niconfig",    if_config,     "Configure interface IP info",
   "nilist",      if_listifs,    "List current interfaces",
   NULL,
};

#ifdef MAC_LOOPBACK
/* MAC loopback create routine (in macloop.c) */
extern   int   lb_create(struct net * ifp, void * bindinfo);
#endif   /* MAC_LOOPBACK */

#ifdef USE_PPP
/* ppp interface create routine */
extern   int   ppp_create(struct net * ifp, void * bindinfo);
#endif   /* USE_PPP */

/* FUNCTION: new_iface()
 * 
 * PARAM1: void * pio
 *
 * RETURNS: 
 */

#define IF_MAXNAMES  12
static char ifnames[IF_MAXNAMES][IF_NAMELEN];

int
new_iface(void * pio)
{
   int   i;
   int   err;
   NET   net;
   char *   name;
   char *   type;
   char *   cp =  nextarg(((GEN_IO)pio)->inbuf);

   if (!*cp)   /* no arg given */
   {
      ns_printf(pio, "please enter a name & type for new loopback iface\n");
      ns_printf(pio, "valid types include:\n");
#ifdef MAC_LOOPBACK
      ns_printf(pio, "   lo\n");
#endif
#ifdef USE_PPP
      ns_printf(pio, "   ppp\n");
#endif
      return -1;
   }

   /* get name buffer for ni_create to use */
   name = NULL;
   for(i = 0; i < IF_MAXNAMES; i++)
   {
      if(ifnames[i][0] == 0)  /* unused entry */
      {
         name = &ifnames[i][0];
         break;
      }
   }
   if (!name)
      return -1;

   strcpy(name, cp);       /* copy name arg into new buffer */
   type = nextarg(cp);
   cp = strchr(name, ' '); /* get end of first arg (name) */
   if(cp)
      *cp = '\0';          /* null terminate first arg */

   err = -1;               /* assume error */

#ifdef MUTE_WARNS
   /* some compilers give a bogus warning below if we don't assign a
    * gratuitous default value to net.
    */
    net = (NET)(netlist.q_head);
#endif   /* MUTE_WARNS */
#ifdef MAC_LOOPBACK
   if(strnicmp(type, "lo", 2) == 0)
   {
      err = ni_create(&net, lb_create, name, NULL);
   }
#endif   /* MAC_LOOPBACK */
#ifdef USE_PPP
   if(strnicmp(type, "ppp", 3) == 0)
   {
      err = ni_create(&net, ppp_create, name, NULL);
   }
#endif   /* USE_PPP */

   if (err)
   {
      ns_printf(pio, "Error %d creating iface name '%s' of type '%s'\n", 
         err, name, type);
      *name = 0;     /* free name buffer (mark as unused) */
   }
   else
      ns_printf(pio, "Created iface %s, - %s\n", name, net->n_mib->ifDescr);

   return err;
}


/* FUNCTION: del_iface()
 * 
 * PARAM1: void * pio
 *
 * RETURNS: 
 */

int
del_iface(void * pio)
{
   int   i;
   int   err;
   struct net *   net;
   char *   cp =  nextarg(((GEN_IO)pio)->inbuf);
   char *   netname;

   if (!*cp)   /* no arg given */
   {
      ns_printf(pio, "enter a net name to delete\n");
      return -1;
   }
   for (net = (struct net *)(netlist.q_head); net; net = net->n_next)
   {
      if (strcmp((char*)net->name, cp) == 0)
         break;
   }
   if (!net)
   {
      ns_printf(pio, "didn't find interface \"%s\"\n", cp);
      return -1;
   }
   netname = net->name;  /* save name to free if iface frees OK */
   err = ni_delete(net);
   /* since we found the iface in the list, assume error return means 
    * it's not a static iface.
    */
   if (err)
      ns_printf(pio, "%s is not a dynamic interface\n", cp);
   else
   {
      /* if the name of the deleted iface is a string in our local static
       * name buffer table, free the entry:
       */
      for(i = 0; i < IF_MAXNAMES; i++)
      {
         if(netname == &ifnames[i][0])    /* found name in table */
         {
            ifnames[i][0] = 0;      /* clear entry */
            break;
         }
      }
      ns_printf(pio, "deleted iface %s\n", cp);
   }

   return err;
}

/* FUNCTION: if_listifs()
 * 
 * list the current interfaces 
 *
 * PARAM1: void * pio
 *
 * RETURNS: 0
 */

int
if_listifs(void * pio)
{
   NET   ifp;
   IFMIB mib;
   int   i = 1;   /* Ones based iface index */

   for(ifp = (NET)netlist.q_head; ifp; ifp = ifp->n_next)
   {
      mib = ifp->n_mib;
      ns_printf(pio, "%d - %s - %s ",
         i++, ifp->name, mib->ifDescr);

      ns_printf(pio, "(Admin:%s, Oper:%s)\n",
         (mib->ifAdminStatus==NI_UP)?"up":"down" ,
         (mib->ifOperStatus==NI_UP)?"up":"down" );
   }
   return 0;
}


/* FUNCTION: if_config()
 * 
 * PARAM1: void * pio
 *
 * RETURNS: 
 */

int
if_config(void * pio)
{
   char *   name;
   char *   addr = NULL;
   char *   mask = NULL;
   unsigned bits;
   NET ifp;
   struct niconfig_0 cfg;

   name = nextarg(((GEN_IO)pio)->inbuf);
   if (*name)
   {
      addr = nextarg(name);
   }
   if (addr && *addr)
   {
      mask = nextarg(addr);
      *(addr-1) = 0;    /* null terminate name text */
      if (mask && *mask)
      {
         *(mask-1) = 0; /* null terminate addr text */
      }
   }
   if (!(*name) || !(*addr))
   {
      ns_printf(pio, "specify if's name, ip_addr, [mask]\n");
      return -1;
   }
   for (ifp = (NET)(netlist.q_head); ifp; ifp = ifp->n_next)
      if (strncmp(ifp->name, name, strlen(ifp->name)) == 0)
      break;

   if (!ifp)
   {
      ns_printf(pio, "Can't find iface %s\n", name);
      return -1;
   }
   name = parse_ipad(&(cfg.n_ipaddr), &bits, addr);
   if (name)
   {
      ns_printf(pio, "IP address parse error: %s\n", name);
      return -1;
   }
   if (*mask)
   {
      name = parse_ipad(&(cfg.snmask), &bits, mask);
      if (name)
      {
         ns_printf(pio, "subnet mask parse error: %s\n", name);
         return -1;
      }
   }
   else
   {
      cfg.snmask = 0L;
      while (bits--)
         cfg.snmask = (cfg.snmask << 1) | 1;
   }
   ni_set_config(ifp, &cfg);
   return 0;
}


/* FUNCTION: if_enable()
 * 
 * PARAM1: void * pio
 *
 * RETURNS: 
 */

int
if_enable(void * pio)
{
   int   err;
   NET   ifp;

   ifp = if_getbypio(pio);
   if(ifp == NULL)
      return -1;

   err = ni_set_state(ifp, NI_UP);
   if(err)
      ns_printf(pio, "Error %d enabling %s\n", err, ifp->name);
   else
      ns_printf(pio, "Enabled %s\n", ifp->name);

   return err;
}



/* FUNCTION: if_disable()
 *
 * API call to disable an interface
 *
 * PARAM1: void * pio
 *
 * RETURNS: 
 */

int
if_disable(void * pio)
{
   int   err;
   NET   ifp;

   ifp = if_getbypio(pio);
   if(ifp == NULL)
      return -1;

   err = ni_set_state(ifp, NI_DOWN);
   if(err)
      ns_printf(pio, "Error %d shuting down %s\n", err, ifp->name);
   else
      ns_printf(pio, "Shut down %s\n", ifp->name);

   return (err);
}

#endif   /* IN_MENUS */


/* FUNCTION: ni_set_state()
 * 
 * The NI API entry point to turn a network interface on or off.
 * This is passed the NET and a code - NI_UP or NI_DOWN, from net.h
 * NET is enabled or disabled, and the MIB ifAdminStatus flags set.
 * Any sockets using the net are brutally terminated. 
 * NET interfaces should toggle-able back and forth with this call.
 * No check is made of the net's prior state, it is simple pushed 
 * toward the desired state.
 *
 * PARAM1: NET ifp
 * PARAM2: int opcode
 *
 * RETURNS: 0 if OK or ENP_ error from net's n_setstate routine.
 */

int
ni_set_state(NET ifp, int opcode)
{
   int   err = 0;

   if ((opcode != NI_DOWN) &&
       (opcode != NI_UP))
   {
      return ENP_PARAM;
   }

   LOCK_NET_RESOURCE(NET_RESID);

   if (opcode == NI_DOWN)  /* clean up system tables */
   {
#ifdef INCLUDE_TCP
      if_killsocks(ifp);      /* kill this iface's sockets */
#endif   /* INCLUDE_TCP */
      del_route(0, 0, if_netnumber(ifp));    /* delete any IP routes */
      clear_arp_entries(NULLIP, ifp);        /* delete any ARP entries */
   }

   /* force if-mib AdminStatus flag to correct setting. This will
    * keep devices without n_setstate() calls from receiving any
    * packet to send from ip2mac().
    */
   if (opcode == NI_UP)
      ifp->n_mib->ifAdminStatus = NI_UP;
   else
      ifp->n_mib->ifAdminStatus = NI_DOWN;

   ifp->n_mib->ifLastChange = cticks * (100/TPS);
   if (ifp->n_setstate) /* call state routine if any */
      err = ifp->n_setstate(ifp, opcode);
   else
      err = 0; /* no routine == no error */

   if(!err)
   {
      /* Get here if setstate was OK - set ifOperStatus */
      if (opcode == NI_UP)
         ifp->n_mib->ifOperStatus = NI_UP;
      else
         ifp->n_mib->ifOperStatus = NI_DOWN;
   }

   UNLOCK_NET_RESOURCE(NET_RESID);

   return err;
}

#endif   /* DYNAMIC_IFACES */


/* FUNCTION: if_getbynum()
 *
 * Gets an interface pointer from the index number. This is the
 * inverse of net_num() in ipnet.c - these two routines interconvert
 * interface indexs<->pointers. Note that this handles dynamic
 * interfaces as well as static.
 *
 * PARAM1: int ifnum - 0 based index to interface
 *
 * RETURNS: Returns NET pointer, or NULL if out of range
 */

NET
if_getbynum(int ifnum)
{
   NET ifp;
   for (ifp = (NET)(netlist.q_head); ifp; ifp = ifp->n_next)
   {
      if(ifnum-- == 0)
         return ifp;
   }
   dtrap();
   return NULL;   /* list is not long enough */
}


/* FUNCTION: isbcast()
 *
 * isbcast(ifc, addr) - See if the address pointed to by addr is a 
 * broadcast for the link layer interface ifc.
 *
 * 
 * PARAM1: NET ifc
 * PARAM2: unsigned char * addr
 *
 * RETURNS:  Returns TRUE if broadcast, else false. 
 */

int
isbcast(NET ifc, unsigned char * addr)
{
#ifndef MULTI_HOMED
   if (ifc != nets[0])
      panic("isbcast: bad net");
#endif

#if (ALIGN_TYPE > 2)
   /* On systems with 32bit alignment requirements we have to make
    * sure our tests are aligned. Specifically, this results in "data
    * abort" errors on the Samsung/ARM port. 
    */
   if((u_long)addr & (ALIGN_TYPE - 1))
   {
      /* check first two bytes */
      if ((u_short)*(u_short*)(addr) != 0xFFFF)
         return(FALSE);
      if ((u_long)(*(u_long*)(addr + 2)) != 0xFFFFFFFF)
         return FALSE;
   }
   else
#endif /* ALIGN_TYPE > 4 */
   {
      /* check first four bytes for all ones. Since this is the fastest
       * test, do it first
       */
      if ((u_long)(*(u_long*)addr) != 0xFFFFFFFF)
         return FALSE;

      /* check last two bytes */
      if ((u_short)*(u_short*)(addr+4) != 0xFFFF)
         return(FALSE);
   }
   
   /* now reject any line type packets which don't support broadcast */
   if ((ifc->n_mib->ifType == PPP) ||
       (ifc->n_mib->ifType == SLIP))
   {
      return FALSE;
   }

   /* passed all tests, must be broadcast */
   return(TRUE);
}


/* FUNCTION: reg_type()
 *
 * reg_type() - register prot type with a net drivers 
 *
 * 
 * PARAM1: unshort type
 *
 * RETURNS: Returns 0 if OK, else non-zero error code. 
 */

int
reg_type(unshort type)
{
   NET ifp;
   int   e;

#ifdef DYNAMIC_IFACES
   unsigned i;
   
   /* add to master protocol list */
   for (i = 0; i < PLLISTLEN; i++)
   {
      if (protlist[i] == type)   /* type already in list */
         break;
      if (protlist[i] == 0)      /* Found empty slot */
      {
         protlist[i] = type;     /* add new protocol type */
         break;
      }
   }
   if (i >= PLLISTLEN)
      return ENP_RESOURCE;
#endif   /* DYNAMIC_IFACES */

   /* loop thru list of nets, making them all look at new type */
   for (ifp = (NET)(netlist.q_head); ifp; ifp = ifp->n_next)
   {
      if (ifp->n_reg_type)    /* make sure call exists */
      {
         e = (ifp->n_reg_type)(type, ifp);
         if (e)
            return e;   /* bails out if error */
      }
   }
   return 0;   /* OK code */
}

#ifdef INCLUDE_TCP

/* FUNCTION: if_killsocks()
 * 
 * This should find all the sockets associateds with a passed NET and 
 * shut them down, ideally with a TCP reset for TCP sockets.
 * Called by ni_set_state() whenever it is bringing down an interface.
 *
 * PARAM1: NET ifp - net that is going down
 *
 * RETURNS: void - this is a "best effort" routine.
 */

      /* kill this NETs sockets */
void
if_killsocks(NET ifp)
{
   struct socket * so;
   struct socket * next;
   NET      so_ifp;     /* interface of sockets in list */

   /* reset any sockets with this iface IP address */
   so = (struct socket *)(soq.q_head);
   while(so)
   {
      if(so->so_pcb)
         so_ifp = so->so_pcb->ifp;
      else
         so_ifp = NULL;
      next = (struct socket *)so->next;
      if (so_ifp == ifp)
      {
         /* this is a direct heavy-handed close. A reset is sent
          * and all data is lost. The user should really have closed
          * all the sockets gracfully first.... 
          */
         soabort(so);
      }
      so = next;
   }
}

#endif   /* INCLUDE_TCP */


#ifdef IN_MENUS

/* FUNCTION: if_getbyname()
 *
 * Gets an interface pointer for the interface matching passed name.
 *
 * PARAM1: char * name;
 *
 * RETURNS: Returns NET pointer, or NULL if name no found
 */

NET
if_getbyname(char * name)
{
   NET ifp;

   for (ifp = (NET)(netlist.q_head); ifp; ifp = ifp->n_next)
      if (strncmp(ifp->name, name, strlen(ifp->name)) == 0)
         break;

   return ifp;
}



/* FUNCTION: if_getbypio()
 *
 * Gets an interface pointer matching the name in the passed PIO
 * inbuf buffer 
 *
 * PARAM1: void * pio
 *
 * RETURNS: Returns NET pointer, or NULL if name no found
 */

NET
if_getbypio(void * pio)
{
   char * name;

   name = nextarg(((GEN_IO)pio)->inbuf);
   return(if_getbyname(name));
}
#endif




