/*  UPNP.h

   Universal plug and play

   9/15/2000 - Created - Stan Breitlow

*/

#ifndef UPNP_H
#define UPNP_H

/* How long to wait for a DHCP address */
extern int DHCP_WAIT_TIME;


#define  SSDP_NOTIFY_DELAY    10       /* 1  second */
#define  SSDP_NOTIFY_COUNT    3        /* 3  times  */
#define  SECONDS_PER_MINUTE   60
#define  TICKS_PER_SECOND     10
#define  CHECKUP_TIME         5*SECONDS_PER_MINUTE*TICKS_PER_SECOND  /* 5  minutes  */

typedef enum
{
   UPNP_START,
   UPNP_INIT,
   UPNP_DHCP_SEEK,
   UPNP_GOT_DHCP_ADDRESS,
   UPNP_AUTO_IP,
   UPNP_FIXED_IP,
   UPNP_SSDP_NOTIFY,
   UPNP_SSDP_WAIT,
   UPNP_SSDP_DISCOVER,
   UPNP_STARTUP_FINISHED,
   UPNP_IDLE,
   UPNP_IDLE_FIXED,
   UPNP_DHCP_CHECKUP,
   UPNP_DHCP_RESEEK,
   UPNP_DISABLED
} eUPNP_STATE;


struct tUPNP 
{
    eUPNP_STATE state;
    eIP_METHOD  ip_method;
    int     got_dhcp_address;   /* boolean */
    u_long  idle_timer;
    u_long  notify_delay;
    u_long  notify_count;
    int     local_iface;
};

extern struct tUPNP upnp[MAXNETS];


int   Upnp_init(void);
void  Upnp_tick(long);
 
#endif /* UPNP_H */


