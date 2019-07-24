/******************************************************************************
*                                                                             *
* License Agreement                                                           *
*                                                                             *
* Copyright (c) 2006 Altera Corporation, San Jose, California, USA.           *
* All rights reserved.                                                        *
*                                                                             *
* Permission is hereby granted, free of charge, to any person obtaining a     *
* copy of this software and associated documentation files (the "Software"),  *
* to deal in the Software without restriction, including without limitation   *
* the rights to use, copy, modify, merge, publish, distribute, sublicense,    *
* and/or sell copies of the Software, and to permit persons to whom the       *
* Software is furnished to do so, subject to the following conditions:        *
*                                                                             *
* The above copyright notice and this permission notice shall be included in  *
* all copies or substantial portions of the Software.                         *
*                                                                             *
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR  *
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,    *
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE *
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER      *
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING     *
* FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER         *
* DEALINGS IN THE SOFTWARE.                                                   *
*                                                                             *
* This agreement shall be governed in all respects by the laws of the State   *
* of California and by the laws of the United States of America.              *
*                                                                             *
* Altera does not recommend, suggest or require that this reference design    *
* file be used in conjunction or combination with any other product.          *
*                                                                             *
* Source for the Altera InterNiche device services.                           *
*                                                                             *
* Author EPS                                                                  *
*                                                                             *
******************************************************************************/

/*******************************************************************************
 *******************************************************************************
 *
 * InterNiche device services.
 *
 *******************************************************************************
 ******************************************************************************/

/*******************************************************************************
 *
 * Imported services.
 *
 ******************************************************************************/

#include "sys/alt_llist.h"
#include "alt_iniche_dev.h"

#ifdef IP_MULTICAST
#include "ipmc/igmp_cmn.h"
extern int mcastlist(struct in_multi *);
#endif


/*******************************************************************************
 *
 * InterNiche device service globals.
 *
 ******************************************************************************/

/* List of InterNiche devices. */
ALT_LLIST_HEAD(alt_iniche_dev_list);


/*******************************************************************************
 *
 * InterNiche device service functions.
 *
 ******************************************************************************/

/*
 * iniche_devices_init
 *
 *   --> if_count               Number of interfaces before init.
 *
 *   <--                        Number of interfaces after init.
 *
 *   This function initializes the InterNiche devices.  The number of interfaces
 * before initialization is specified by if_count.  This function returns the
 * total number of interfaces after initialization.
 */

int iniche_devices_init(
    int                         if_count)
{
    alt_iniche_dev              *p_dev;
    alt_iniche_dev              *p_dev_list_end;
    NET                         p_net;
    ip_addr                     ipaddr,
                                netmask,
                                gw;
    int                         use_dhcp;

    /* Get the InterNiche device list. */
    p_dev = (alt_iniche_dev *) (alt_iniche_dev_list.next);
    p_dev_list_end = (alt_iniche_dev *) (&(alt_iniche_dev_list.next));

    /* Initialize each InterNiche device. */
    while (p_dev != p_dev_list_end)
    {
        /* Initialize the InterNiche device data record. */
        p_dev->p_driver_data = p_dev;
        p_dev->if_num = if_count;
        p_dev->p_net = nets[p_dev->if_num];

        /* Perform device specific initialization. */
        (*(p_dev->init_func))(p_dev);

        /* Get the interface IP address. */
        p_net = p_dev->p_net;
                
        if (get_ip_addr(p_dev, &ipaddr, &netmask, &gw, &use_dhcp))
        {
#ifdef DHCP_CLIENT
            /* 
             * OR in the DHCP flag, if enabled. This will allow any
             * application-specific flag setting in get_ip_addr(), such 
             * as enabling AUTOIP, to occur 
             */
            if (use_dhcp) {
                p_net->n_flags |= NF_DHCPC;
            }
#endif
            p_net->n_ipaddr = ipaddr;
            p_net->snmask = netmask;
            p_net->n_defgw = gw;
#ifdef IP_MULTICAST
	    p_net->n_mcastlist = mcastlist;
#if defined (IGMP_V1) || defined (IGMP_V2)
            p_net->igmp_oper_mode = IGMP_MODE_DEFAULT;
#endif  /* IGMPv1 or IGMPv2 */
#endif  /* IP_MULTICAST */
        }

        /* Initialize next device. */
        if_count++;
        p_dev = (alt_iniche_dev *) p_dev->llist.next;
    }

    return (if_count);
}
