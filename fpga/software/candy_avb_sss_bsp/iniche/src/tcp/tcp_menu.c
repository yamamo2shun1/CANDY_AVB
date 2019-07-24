/*
 * FILENAME: tcp_menu.c
 *
 * Copyright 2001 By InterNiche Technologies Inc. All rights reserved
 *
 * TCp menu for testing exotic TCP options
 *
 * MODULE: TCP
 *
 * ROUTINES: tcp_winscale(), tcp_cwndsize(), tcp_cdump(), 
 *
 * PORTABLE: yes
 */


#include "ipport.h"

#ifdef INCLUDE_TCP  /* include/exclude whole file at compile time */
#ifdef TCP_MENUS  /* whole file can be ifdeffed out */

#ifndef IN_MENUS
#error TCP_MENUS requires IN_MENUS
#endif


#include "tcpport.h"
#include "protosw.h"
#include "in_pcb.h"
#include "tcp.h"
#include "tcp_timr.h"
#include "tcp_var.h"
#include "menu.h"

int   tcp_setwin(void * pio);
int   tcp_cwndsize(void * pio);
int   tcp_cdump(void * pio); 
int   so_setopts(void * pio); 
int   tp_setflags(void * pio); 

struct menu_op tcpmenu[] = {
   "tcp", stooges, "TCP test menus",
#ifdef TCP_BIGCWND
   "tcwnd",    tcp_cwndsize,  "set congestion window (MSS factor)",
#endif   /* TCP_BIGCWND */
   "twinset",  tcp_setwin,    "set default TCP window (Kbytes)",
#ifdef NET_STATS
   "tcdump",   tcp_cdump,     "dump connection details",
#endif   /* NET_STATS */
   "sosetopts",so_setopts,    "set default socket options",
   NULL, NULL, NULL
};


extern   u_long   tcp_recvspace;



int
tcp_setwin(void * pio)
{
   u_long   i;
   char *   cp =  nextarg( ((GEN_IO)pio)->inbuf );

   i = atol(cp);
   if(i == 0)
   {
      ns_printf(pio, "window value must be number of K-bytes, currently %uKB\n",
         (int)(tcp_recvspace/1024) );
      return -1;
   }
   tcp_recvspace = i * 1024L;
   default_wind_scale = 0;
   if(tcp_recvspace > (64L * 1024L))
   {
#ifdef TCP_WIN_SCALE
      u_long tmp;
      tmp = tcp_recvspace;
      while(tmp & 0xFFFF0000)
      {
         default_wind_scale++;
         tmp >>= 1;
      }
      ns_printf(pio, "tcp window scaling factor set to %d, ", (int)default_wind_scale);
#else
      ns_printf(pio, "Size must be < 64K, or recompile w/TCP_WIN_SCALE\n");
      tcp_recvspace = (32L * 1024L);
#endif
   }

   ns_printf(pio, "window set to %ld\n", tcp_recvspace);
   return 0;
}


#ifdef TCP_BIGCWND
int
tcp_cwndsize(void * pio)
{
   int      i;
   char *   cp =  nextarg( ((GEN_IO)pio)->inbuf );

   i = atoi(cp);
   if(i < 1)
   {
      ns_printf(pio, "c-window must be scale, 1 - 255; currently %s\n",
         default_cwnd);
      return -1;
   }
   default_cwnd = (i * TCP_MSS);
   use_default_cwnd = 1;
   ns_printf(pio, "default initial congestion window set to %d\n", default_cwnd);
   return 0;
}
#endif   /* TCP_BIGCWND */


#ifdef NET_STATS

void
dump_so_options(void * pio, unshort options)
{
   if(options & SO_DEBUG)ns_printf(pio, "SO_DEBUG ");
   if(options & SO_ACCEPTCONN)ns_printf(pio, "ACCEPTCONN ");
   if(options & SO_REUSEADDR)ns_printf(pio, "REUSEADDR ");
   if(options & SO_KEEPALIVE)ns_printf(pio, "KEEPALIVE ");
   if(options & SO_DONTROUTE)ns_printf(pio, "DONTROUTE ");
   if(options & SO_BROADCAST)ns_printf(pio, "BROADCAST ");
   if(options & SO_USELOOPBACK)ns_printf(pio, "USELOOPBACK ");
   if(options & SO_LINGER)ns_printf(pio, "LINGER ");
   if(options & SO_OOBINLINE)ns_printf(pio, "OOBINLINE ");
   if(options & SO_TCPSACK)ns_printf(pio, "TCPSACK ");
   if(options & SO_WINSCALE)ns_printf(pio, "WINSCALE ");
   if(options & SO_TIMESTAMP)ns_printf(pio, "TIMESTAMP ");
   if(options & SO_BIGCWND)ns_printf(pio, "BIGCWND ");

   return;
}

void
dump_tp_tpflags(void * pio, unshort flags)
{
   if(flags & TF_ACKNOW)ns_printf(pio, "ACKNOW ");
   if(flags & TF_DELACK)ns_printf(pio, "DELACK ");
   if(flags & TF_NODELAY)ns_printf(pio, "NODELAY ");
   if(flags & TF_NOOPT)ns_printf(pio, "NOOPT ");
   if(flags & TF_SENTFIN)ns_printf(pio, "SENTFIN ");
   if(flags & TF_SACKOK)ns_printf(pio, "SACKOK ");
   if(flags & TF_SACKNOW)ns_printf(pio, "SACKNOW ");
   if(flags & TF_WINSCALE)ns_printf(pio, "WINSCALE ");
   if(flags & TF_TIMESTAMP)ns_printf(pio, "TIMESTAMP ");

   return;
}

extern char *   tcpstates[11];   /* in nptcp.c */

int
tcp_cdump(void * pio)
{
   unsigned fport;
   unsigned lport;
   struct socket * so;
   struct tcpcb * tp;
   struct inpcb * inp;
   char *   cp =  nextarg( ((GEN_IO)pio)->inbuf );

#ifdef MUTE_WARNS
   tp = NULL;  /* stifle idiot compiler warnings */
   inp = NULL;
#endif   /* MUTE_WARNS */
   
   fport = atoi(cp);
   if((fport == 0) && (*cp != '0'))
   {
      ns_printf(pio, "need port number(s), fport[,lport]\n");
      return -1;      
   }
   cp = strchr(cp, ',');   /* see if there's a comma */
   if(cp)
      lport = (unsigned)atoi((cp+1));
   else
      lport = 0;     /* wildcard */

   /* look for matching connection */
   for(so = (struct socket *)soq.q_head; so; so = so->next)
   {
      inp = (struct inpcb *)so->so_pcb;
      if(!inp)
         continue;
      tp = (struct tcpcb *)inp->inp_ppcb;
      if(!tp)
         continue;
      if(inp->inp_fport != htons(fport))
         continue;
      if((inp->inp_lport == htons(lport)) ||
         (lport == 0))
            break;
   }
   if(!so)
   {
      ns_printf(pio, "no socket matching ports %u,%u\n", fport,lport);
      return -1;            
   }

   /* if we get here then so, inp and tp are all set */
   ns_printf(pio,"IP: %u.%u.%u.%u, %u->%u, ", 
      PUSH_IPADDR(inp->inp_faddr.s_addr),
      htons(inp->inp_lport), htons(inp->inp_fport));

   ns_printf(pio,"options: 0x%x, so_rcv.cc:%u, so_snd.cc:%u\n",
      (unsigned)so->so_options,
      (unsigned)so->so_rcv.sb_cc,(unsigned)so->so_snd.sb_cc);

   ns_printf(pio, "snd_wnd:%d, rcv_wnd:%d, snd_cwnd:%d\n", 
      tp->snd_wnd, tp->rcv_wnd, tp->snd_cwnd);

#ifdef TCP_WIN_SCALE
   ns_printf(pio, "snd_wnd_scale:%d, rcv_wnd_scale:%d\n",
      tp->snd_wind_scale, tp->rcv_wind_scale);
#endif

   ns_printf(pio, "%s, snd_una:0x%lx, snd_nxt:0x%lx\n",
      tcpstates[tp->t_state], tp->snd_una, tp->snd_nxt);

   ns_printf(pio, "so_options: ");
   dump_so_options(pio, so->so_options);
   ns_printf(pio, "\nTCP flags: ");
   dump_tp_tpflags(pio, tp->t_flags);
   ns_printf(pio, "rtt:%d srtt:%d, \n", tp->t_rttick, tp->t_srtt);

   return 0;
}
#endif   /* NET_STATS */

extern int hex2bin(char * inbuf, unsigned char * outbuf, int bcount);

int
so_setopts(void * pio)
{
   int      err;
   unshort  newopts;
   char *   cp =  nextarg( ((GEN_IO)pio)->inbuf );

   if(!cp || !*cp)
   {
      ns_printf(pio, "current default socket options are 0x%x\n",
         socket_defaults);      
#ifdef NET_STATS
      dump_so_options(pio, socket_defaults);      
      ns_printf(pio, "\n");      
#endif   /* NET_STATS */
      ns_printf(pio, "Enter new hex value to change them.\n");      
      return 0;
   }
   err = hex2bin(cp, (u_char*)(&newopts), 2);
   if(err)
   {
      ns_printf(pio, "Can't parse hex %s\n", cp);
      return -1;
   }
   socket_defaults = htons(newopts);
   ns_printf(pio, "default socket options set to 0x%x\n", socket_defaults);
#ifdef NET_STATS
   dump_so_options(pio, socket_defaults);      
   ns_printf(pio, "\n");      
#endif   /* NET_STATS */

   return 0;
}


#endif   /* IN_MENUS - whole file can be ifdeffed out */
#endif /* INCLUDE_TCP */


