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

#ifndef __ALT_INICHE_DEV_H__
#define __ALT_INICHE_DEV_H__

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

#include <errno.h>

#include "ipport.h"
#include "tcpport.h"
#include "sys/alt_llist.h"


/*******************************************************************************
 *
 * Structure typedefs.
 *
 ******************************************************************************/

typedef struct alt_iniche_dev_struct    alt_iniche_dev;


/*******************************************************************************
 *
 * Typedefs.
 *
 ******************************************************************************/

/*
 * alt_iniche_dev_init_func
 *
 *   --> p_dev                  Device to initialize.
 *
 *   Functions of this type initialize the device specified by p_dev.
 */

typedef error_t (*alt_iniche_dev_init_func)(
    alt_iniche_dev              *p_dev);


/*******************************************************************************
 *
 * Structure defs.
 *
 ******************************************************************************/

/*
 * InterNiche network interface device structure
 *
 *   llist                      Linked list data record.
 *   name                       Name of device.
 *   init_func                  Device initialization function.
 *   p_driver_data              Driver data.
 *   if_num                     Device interface number.
 *   p_net                      InterNiche network interface data record.
 *
 *   This structure contains fields used for InterNiche network interface
 * device drivers.
 *   All device data records are maintained in a list using the field llist.
 *   The name of the device is maintained in name.
 *   The function used to initialize the device is specified by init_func.
 *   The device driver may maintain a pointer to its own data in p_driver_data.
 *   Each device has an associated interface number maintained in if_num.
 *   The InterNiche network interface data record is maintained in p_net.
 */

struct alt_iniche_dev_struct
{
    alt_llist                   llist;
    char                        *name;
    alt_iniche_dev_init_func    init_func;
    void                        *p_driver_data;
    int                         if_num;
    NET                         p_net;
};


/*******************************************************************************
 *
 * Macros.
 *
 ******************************************************************************/

/*
 * alt_iniche_dev_reg
 *
 *   --> p_dev                  Device to register.
 *
 *   This macro registers the InterNiche device specified by p_dev with the
 * InterNiche device services.
 */

extern alt_llist                alt_iniche_dev_list;

#define alt_iniche_dev_reg(p_dev)                                              \
    alt_llist_insert(&alt_iniche_dev_list, &((p_dev)->llist))


/*******************************************************************************
 *
 * InterNiche device services prototypes.
 *
 ******************************************************************************/

int iniche_devices_init(
    int                         ifacesFound);


/*******************************************************************************
 *******************************************************************************
 *
 * External InterNiche device services.
 *
 *******************************************************************************
 ******************************************************************************/

/*******************************************************************************
 *
 * External InterNiche device services prototypes.
 *
 *   These functions are provided by the external system to the InterNiche
 * device services.
 *
 ******************************************************************************/

/*
 * get_mac_addr
 *
 *   --> net                    Network interface for which to get MAC address.
 *   <-- mac_addr               MAC address.
 *
 *   This function returns in mac_addr a MAC address to be used with the network
 * interface specified by net.
 */

int get_mac_addr(
    NET                         net,
    unsigned char               mac_addr[6]);


/*
 * get_ip_addr
 *
 *   --> p_dev                  Device for which to get IP address.
 *   <-- p_addr                 IP address for device.
 *   <-- p_netmask              IP netmask for device.
 *   <-- p_gw_addr              IP gateway address for device.
 *   <-- p_use_dhcp             TRUE if DHCP should be used to obtain an IP
 *                              address.
 *
 *   This function provides IP address information for the InterNiche network
 * interface device specified by p_dev.  If a static IP address is to be used,
 * the IP address, netmask, and default gateway address are returned in p_addr,
 * p_netmask, and p_gw_addr, and FALSE is returned in p_use_dhcp.  If the DHCP
 * protocol is to be used to obtain an IP address, TRUE is returned in
 * p_use_dhcp.
 */

int get_ip_addr(
    alt_iniche_dev              *p_dev,
    ip_addr                     *p_addr,
    ip_addr                     *p_netmask,
    ip_addr                     *p_gw_addr,
    int                         *p_use_dhcp);


#endif /* __ALT_INICHE_DEV_H__ */
