/*
 * FILENAME: slipif.c
 *
 * Copyright 1997 - 2004 By InterNiche Technologies Inc. All rights reserved
 *
 * SLIP driver for Netport demo package. Developed and 
 * tested primarily on PC UART hardware. This should be compatable to 
 * any byte-oriented UART. Frame oriented UARTS such as Motorola 
 * MC68302 may by supported by using the call to ln_write() instead 
 * of ln_putc().
 *
 * MODULE: INET
 *
 * ROUTINES: prep_slip(), slip_pkt_send(), slip_rxbyte(), 
 * ROUTINES: slip_rxframe(), slip_init(), slip_stats(), slip_reg_type(), 
 * ROUTINES: slip_close(),
 *
 * PORTABLE: yes
 */

/* Additional Copyrights: */
/* Portions Copyright 1994, 1995 by NetPort Software 6/25/97 */

#include "ipport.h"

#ifdef USE_SLIP   /* whole file can be ifdeffed out */

#include "q.h"
#include "netbuf.h"
#include "net.h"
#include "ip.h"
#include "slip.h"

#include "slipport.h"   /* slip "port" file - what usrt, etc */

#ifdef NPDEBUG
/* macro to sanity check device number */
#define check_iface(ifc) \
   if(nets[ifc]->n_mib->ifType != SLIP)   \
   {  dtrap(); }
#else
#define  check_iface(if);
#endif   /* NPDEBUG */

/* declare the SLIP iface routines */
int   slip_init(int iface);
int   slip_reg_type(unshort type, struct net * netp);
int   slip_pkt_send(PACKET netp);
int   slip_close(int iface);
void  slip_stats(void * pio,int iface);

static char slipmac[6]= {'S','L', 'I', 'P', 'I', 'F'}; 

int      slip_ifs =  1; /* default, may by overwritted before prep_slip() call */

struct slip_errors   slip_error[_NSLIP];  /* error counters */

/* Lookup tables to map iface to unit and unit to iface. */
int      slip_unit_map[_NSLIP];
int      slip_iface_map[_NSLIP];

struct com_line   slip_lines[_NSLIP];
int      slip_units  =  0; /* unit numbers */

u_char   slipoutbuf[SLIPBUFSIZ];
u_char   slipinbuf[SLIPBUFSIZ];

/* FUNCTION: prep_slip()
 *
 * prep_slip() - This prepares the slip logic for calls to 
 * slip_init() It should be called at init time before any other slip 
 * calls are make, and also before any network activity starts. 
 * Parameter passed in the index of the next available nets[] 
 * structure. 
 *
 * 
 * PARAM1: int iface
 *
 * RETURNS:  Returns index of next available iface after slip 
 * interfaces.
 */

int
prep_slip(int iface)
{
   int   i;
   int   unit;

   for (i = iface; i < (iface + _NSLIP); i++)
   {
      if (slip_units >= _NSLIP)  /* run out of line units? */
         return (i); /* that's all we can do */

      /* set up net interface routine pointers, MIB, etc, before trying to
      init the net. */
      nets[i]->n_mib->ifAdminStatus = 2;  /* status = down */
      nets[i]->n_mib->ifOperStatus = 2;   /* will be set up on connect  */
      nets[i]->n_mib->ifLastChange = cticks * (100/TPS);
      nets[i]->n_mib->ifPhysAddress = (u_char*)&slipmac[0];
      nets[i]->n_mib->ifType = SLIP;
      nets[i]->n_mib->ifDescr = (u_char*)"SLIP";

      /* set header and max frame lengths for SLIP including HDR + Bias */
      nets[i]->n_mtu = SLIPSIZ + SLIPHDR_SIZE + SLIPHDR_BIAS ;

      /* slip frame delimited size + alignment offset */
      nets[i]->n_lnh = SLIPHDR_SIZE + SLIPHDR_BIAS;
      nets[i]->n_hal = 0;                 /* hardware address length */

      nets[i]->n_flags |= NF_NBPROT;      /* we set nb_prot fields in driver */

      /* install our hardware driver routines */
      nets[i]->n_init = slip_init;
      nets[i]->pkt_send = slip_pkt_send;
      nets[i]->raw_send = NULL;
      nets[i]->n_close = slip_close;
      nets[i]->n_stats = slip_stats;

      /* set up slip line driver define in ipport.h */
      unit = slip_units++; /* bump counter for next unit */
      slip_lines[unit].ln_connect = SLIP_connect;
      slip_lines[unit].ln_disconnect = SLIP_disconnect;
      slip_lines[unit].ln_write = SLIP_write;
      slip_lines[unit].ln_putc = SLIP_putc;
      slip_lines[unit].ln_state = LN_INITIAL;
      slip_lines[unit].ln_getc = slip_rxbyte;
 
      slip_lines[unit].lower_type = SLIP;
      slip_lines[unit].upper_unit = (void *)unit;

      /* set up the unit <-> iface mappings */
      slip_unit_map[iface] = unit;
      slip_iface_map[unit] = iface;
   }
   return (i); /* return index of next iface */
}


/* FUNCTION: slip_pkt_send()
 *
 * slip_pkt_send() - this routine sends a passed IP frame on the slip 
 * link indicated by netp. It's the struct net pkt_send routine for 
 * slip. 
 *
 * 
 * PARAM1: PACKET pkt
 *
 * RETURNS: 0 if no error detected, else ENP_ error code
 */

int
slip_pkt_send(PACKET pkt)
{
   IFMIB mib;
   char *   buffer;
   unsigned length;
   int   iface,   unit, e=0;  /* make sure e is set */

   iface = net_num(pkt->net);
   check_iface(iface);
   unit = slip_unit_map[iface];

   mib = nets[iface]->n_mib;
   if (slip_lines[unit].ln_state != LN_CONNECTED)
   {
      mib->ifAdminStatus = 1;

      e = slip_lines[unit].ln_connect(&slip_lines[unit]);
      if (e)
      {
         dprintf("slip connect failed; %s error\n",
          e==1?"temporary":"permanant");
         slip_error[unit].serr_connect++; /* count slip errors */
         return ENP_BAD_STATE;
      }
      mib->ifOperStatus = 1;
   }
   length = pkt->nb_plen;

   /* copy pkt->nb_prot data into SLIP out buffer, so that we don't run over
    * pkt buffer while stuffing escape characters.
    */
   MEMCPY(&slipoutbuf[SLIPHDR_SIZE + SLIPHDR_BIAS], pkt->nb_prot, pkt->nb_plen);

   buffer = slipstuff((u_char*)&slipoutbuf[SLIPHDR_SIZE + SLIPHDR_BIAS], &length, SLIPBUFSIZ);
   if (buffer == NULL)
   {
      slip_error[unit].serr_overflow++;   /* count slip errors */
      LOCK_NET_RESOURCE(FREEQ_RESID);
      pk_free(pkt);
      UNLOCK_NET_RESOURCE(FREEQ_RESID);
      return ENP_NOBUFFER;
   }

   /* if line driver has a "write" command use it, else send bytes */
   if (slip_lines[unit].ln_write)
      e = slip_lines[unit].ln_write(&slip_lines[unit], pkt);    /* to frame-oriented UART hardware */
   else
   {
      while (length--)
         if ((e = slip_lines[unit].ln_putc(&slip_lines[unit], (int)(*buffer++))) != 0)
      break;
   }

   /* maintain mib xmit stats */
   if (e)   /* line send failed */
      mib->ifOutErrors++;
   else
   {
      mib->ifOutNUcastPkts++;
      mib->ifOutOctets += length;
   }
   LOCK_NET_RESOURCE(FREEQ_RESID);
   pk_free(pkt);
   UNLOCK_NET_RESOURCE(FREEQ_RESID);
   return 0;
}



/* FUNCTION: slip_rxbyte()
 *
 * slip_rxbyte() - pass received UART bytes to slip logic. It is 
 * generally OK to call this from the UART ISR, although you may need 
 * to add code somewhere to wake to packet demux task. Returns 0 if 
 * ok, or -1 if buffer overrun. 
 *
 * 
 * PARAM1: int unit
 * PARAM2: int byte
 *
 * RETURNS: 
 */

static unsigned sb_index = 0;
static c0_noise = FALSE;

int
slip_rxbyte(struct com_line * line, int byte)
{
int unit;

   unit = (int)line->upper_unit;

   /* Try to check if c0 has us, caught in a trap ! */
   if (sb_index == 0 && (c0_noise == TRUE)  && byte == SL_END)
   {
      c0_noise = FALSE;
      return -1;
   }

   /* if buffer is empty, wait for flush */
   if (sb_index == 0 && byte != SL_END)
   {
      slip_error[unit].serr_noframe++; /* count slip errors */
      return 0;
   }

   /* check for buffer overflow */
   if (sb_index >= SLIPBUFSIZ)
   {
      sb_index = 0;  /* flush buffer */
      slip_rxframe(unit, slipinbuf, sb_index); /* try to recover frame */
      slip_error[unit].serr_overflow++;   /* count slip errors */
      return -1;
   }

   slipinbuf[sb_index++] = (u_char)byte;    /* put byte in buffer */

   /* check for end of frame */
   if ((byte == SL_END) && /* char is end marker? */
       (sb_index > 1))      /* and data's in buffer? */
   {
      slip_rxframe(unit, slipinbuf, sb_index); /* pass frame to receiver */
      sb_index = 0;  /* clear buffer */
   }
   return 0;
}



/* FUNCTION: slip_rxframe()
 *
 * slip_rxframe() - Process received UART frames. It is generally OK 
 * to call this from the UART ISR, although you may need to add code 
 * somewhere to wake to packet demux task. Returns 0 if ok, negative 
 * error code 
 *
 * 
 * PARAM1: int unit
 * PARAM2: u_char * buffer
 * PARAM3: unsigned length
 *
 * RETURNS: 
 */

int
slip_rxframe(int unit, u_char * buffer, unsigned length)
{
   PACKET pkt; /* inet packet for incoming frame */
   int   iface;
   IFMIB mib;

   iface = slip_iface_map[unit];
   check_iface(iface);

   /* get slip escape characters out of buffer */
   buffer = (u_char*)slipstrip(buffer, &length);
   if (buffer == NULL)
   {
      slip_error[unit].serr_noends++;  /* count slip errors */
      return -1;
   }

   /* Try to detect a spurious 0xc0 byte that has been either incorrectly
    * sent by a buggy PEER or has been caused by line noise. Such a spurious
    * byte should result in a length 0 IP packet on the next frame.
    */
   if(length == 0)
   {
      c0_noise = TRUE;
      slip_error[unit].serr_c0noise++;   /* count slip errors */
      return -1;
   }

   /* test for IP packet type;  0x45 | 0x46 | 0x47 are OK */
   if (*buffer < 0x045 || *buffer > 0x047)
   {
      slip_error[unit].serr_iphdr++;   /* count slip errors */
      return -1;
   }

   mib = nets[iface]->n_mib;

   /* fill in a packet for the received buffer */
   LOCK_NET_RESOURCE(FREEQ_RESID);
   pkt = pk_alloc(length);
   UNLOCK_NET_RESOURCE(FREEQ_RESID);
   if (!pkt)
   {
      slip_error[unit].serr_nobuff++;  /* count slip errors */
      mib->ifInDiscards++;
      return ENP_RESOURCE;
   }

   /* fill in packet fields and give packet to upper layer */

   /* point to start of IP header */
   pkt->nb_prot = pkt->nb_buff + SLIPHDR_SIZE + SLIPHDR_BIAS;

   MEMCPY(pkt->nb_prot, buffer, length);  /* copy in SLIP packet */
   *pkt->nb_buff = 0;                     /* so broadcast stats are OK */
   pkt->nb_plen = length;
   pkt->net = nets[iface];
   pkt->type = IPTP;                      /* packet verified as IP above */
   pkt->nb_tstamp = cticks;

   mib->ifInOctets += length;

   /* queue the packet in rcvdq */
   LOCK_NET_RESOURCE(RXQ_RESID);
   putq(&rcvdq, (q_elt)pkt);
   UNLOCK_NET_RESOURCE(RXQ_RESID);

   /* Most ports should now wake the packet demuxer task */
   SignalPktDemux();

   return 0;   /* OK return */
}



/* FUNCTION: slip_init()
 * 
 * PARAM1: int iface
 *
 * RETURNS: 0 if OK, else ENP error code.
 */

int
slip_init(int iface)
{
IFMIB mib;
int   unit, e = 0;  /* make sure e is set */

   check_iface(iface);
   unit = slip_unit_map[iface];

   mib = nets[iface]->n_mib;

   if (slip_lines[unit].ln_state != LN_CONNECTED)
   {
      mib->ifAdminStatus = 1;

      e = slip_lines[unit].ln_connect(&slip_lines[unit]);
      if (e)
      {
         dprintf("slip connect failed; %s error\n",
          e==1?"temporary":"permanant");
         slip_error[unit].serr_connect++; /* count slip errors */
         return ENP_BAD_STATE;
      }
      mib->ifOperStatus = 1;
   }

   return 0;
}

#ifdef NET_STATS

/* FUNCTION: slip_stats()
 *
 * slip_stats) - the nets[] stats routine for SLIP
 *
 * 
 * PARAM1: void * pio
 * PARAM2: int iface
 *
 * RETURNS: void
 */

void
slip_stats(void * pio,int iface)
{
   int   unit  =  slip_unit_map[iface];

   ns_printf(pio,"SLIP interface: \n");
   ns_printf(pio,"last baud: %ld \n", slip_lines[unit].ln_speed );

   ns_printf(pio,"state: line %s, Admin %s, Oper %s\n", 
    (slip_lines[unit].ln_state == LN_CONNECTED)?"UP":"DOWN",
    (nets[iface]->n_mib->ifAdminStatus == 1)?"UP":"DOWN",
    (nets[iface]->n_mib->ifOperStatus == 1)?"UP":"DOWN");

   ns_printf(pio,"missing END byte:   %u\n", slip_error[unit].serr_noends);
   ns_printf(pio,"overflowed buffer:  %u\n", slip_error[unit].serr_overflow);
   ns_printf(pio,"connects failed:    %u\n", slip_error[unit].serr_connect);
   ns_printf(pio,"buffer alloc fail:  %u\n", slip_error[unit].serr_nobuff);
   ns_printf(pio,"char outside frame: %u\n", slip_error[unit].serr_noframe);
   ns_printf(pio,"data not IP packet: %u\n", slip_error[unit].serr_iphdr);
   ns_printf(pio,"spurious c0 bytes : %u\n", slip_error[unit].serr_c0noise);

   uart_stats(pio,unit);      /* dump UART stats */
}

#endif   /* NET_STATS */



/* FUNCTION: slip_reg_type()
 * 
 * PARAM1: unshort type
 * PARAM2: struct net * netp
 *
 * RETURNS: 
 */

int
slip_reg_type(unshort type, struct net * netp)
{
   USE_ARG(type);
   USE_ARG(netp);
   return 0;
}



/* FUNCTION: slip_close()
 * 
 * PARAM1: int iface
 *
 * RETURNS: 
 */

int
slip_close(int iface)
{
   IFMIB mib;
   int   unit  =  slip_unit_map[iface];

   slip_lines[unit].ln_disconnect(&slip_lines[unit]);

   mib = nets[iface]->n_mib;
   mib->ifAdminStatus = 2; /* set both SNMP MIB statuses to disabled */
   mib->ifOperStatus = 2;

   return 0;
}

#endif   /* USE_SLIP */

/* end of file slipif.c */


