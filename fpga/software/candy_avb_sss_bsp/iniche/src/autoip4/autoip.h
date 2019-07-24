/*  AutoIP.h

   AutoIP, determines IP address by ARPing a trial address
   and waiting for responses.

   9/15/2000 - Created - Stan Breitlow

*/

#ifndef AUTOIP_H
#define AUTOIP_H

typedef enum 
{
  AUTOIP_START,
  AUTOIP_GOT_ADDRESS,
  AUTOIP_ARP_PROBE,
  AUTOIP_ARP_RESPONSE_WAIT,
  AUTOIP_ARP_ADDRESS_USED,
  AUTOIP_ARP_VERIFY_WAIT,
  AUTOIP_ARP_VERIFY_PROBE,
  AUTOIP_FAILED
} eAUTO_IP_STATE;

/* "methods", or vairous modes of operation for the AutoIP code */

typedef enum
{
   eIP_METHOD_DHCP_AUTO_FIXED,   /* Try DHCP then AuoIP */
   eIP_METHOD_DHCP_FIXED,        /* Try DHCP */
   eIP_METHOD_AUTO_FIXED,        /* Try AutoIP */
   eIP_METHOD_FIXED              /* Used fixed (NV) address */
} eIP_METHOD;


extern u_long  dBASE_AUTO_IP_ADDRESS; /* base of auto-address pool */
extern u_long  dMAX_AUTO_IP_ADDRESS;
#define        dAUTO_IP_RANGE    (dMAX_AUTO_IP_ADDRESS - dBASE_AUTO_IP_ADDRESS)

struct autoIP
{
   eAUTO_IP_STATE state;
   u_long      response_timer;
   ip_addr     try_address;
   unshort     arp_attempts;
   unshort     verify_attempts;
};

extern struct autoIP autoIPs[MAXNETS];

int AutoIp_init(void);
void AutoIp_tick(int iface);
int AutoIp_get_state(int iface);
void AutoIp_send_arp_probe(int iface);
 
#endif /* AUTOIP_H */


