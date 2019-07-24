/******************************************************************************
*                                                                             *
* License Agreement                                                           *
*                                                                             *
* Copyright (c) 2009 Altera Corporation, San Jose, California, USA.           *
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
******************************************************************************/

#ifndef __ALTERA_ETH_TSE_INICHE_H__
#define __ALTERA_ETH_TSE_INICHE_H__

#include "alt_iniche_dev.h"
#include "altera_eth_tse_regs.h"
#include "ins_tse_mac.h"

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */


/* System Constant Definition Used in the TSE Driver Code */
#define ALTERA_TSE_PKT_INIT_LEN                    1528
                 
#define ALTERA_TSE_ADMIN_STATUS_DOWN               2
#define ALTERA_TSE_ADMIN_STATUS_UP                 1
#define ALTERA_TSE_MAX_MTU_SIZE                    1514
#define ALTERA_TSE_MIN_MTU_SIZE                    14
#define ALTERA_TSE_HAL_ADDR_LEN                    6



/*******************************
 *
 * Public API for TSE Driver 
 *
 *******************************/


 /* @Function Description: TSE MAC Driver Open/Initialization routine
 * @API TYPE: Public
 * @Param p_dec        pointer to TSE device instance
 * @Return SUCCESS
 */

error_t altera_eth_tse_init(
    alt_iniche_dev              *p_dev);
 
 
/* @Function Description -  Closing the TSE MAC Driver Interface
 *                          
 * 
 * @API TYPE - Public
 * @param  iface    index of the NET interface associated with the TSE MAC.
 * @return SUCCESS
 */
int tse_mac_close(int iface);



/* @Function Description -  TSE transmit API to send data to the MAC
 *                          
 * 
 * @API TYPE - Public
 * @param  net  - NET structure associated with the TSE MAC instance
 * @param  data - pointer to the data payload
 * @param  data_bytes - number of bytes of the data payload to be sent to the MAC
 * @return SUCCESS if success, else SEND_DROPPED, ENP_RESOURCE if error 
 * 
 */
int tse_mac_raw_send(NET net, char * data, unsigned int data_bytes);

/* @Function Description -  TSE transmit API to send data to the MAC
 *                          
 * 
 * @API TYPE - Public
 * @param  pke  - Packet containing data to send
 * @return SUCCESS if success, else SEND_DROPPED, ENP_RESOURCE if error 
 * 
 */
int tse_mac_pkt_send(PACKET pkt);


/********************************
 *
 * Internal API for TSE Driver 
 *
 *******************************/

/** @Function Description: TSE MAC Driver Open/Registration routine
  * @API TYPE: Internal
  * @Param index     index of the NET structure associated with TSE instance
  * @Return next index of NET
  */
int prep_tse_mac(int index, alt_tse_system_info *psys_info);



/* @Function Description: TSE MAC Initialization routine. This function opens the
 *                          device handle, configure the callback function and interrupts ,
 *                        for MSGDMA TX and MSGDMA RX block associated with the TSE MAC,
 *                        Initialize the MAC Registers for the RX FIFO and TX FIFO
 *                        threshold watermarks, initialize the tse device structure,
 *                        set the MAC address of the device and enable the MAC   
 * 
 * @API TYPE: Internal
 * @Param iface    index of the NET structure associated with TSE instance
 * @Return SUCCESS if ok, else ENP_RESOURCE, ENP_PARAM, ENP_LOGIC if error
 */
int tse_mac_init(int iface);



/* @Function Description -    TSE Driver MSGDMA RX ISR callback function
 *                            
 * 
 * @API TYPE - callback
 * @param  context    - context of the TSE MAC instance
 * @param  intnum - temporary storage
 * @return SUCCESS on success else ENP_LOGIC if error
 */
void tse_msgdmaRx_isr(void * context);

/* @Function Description -    TSE Driver MSGDMA TX ISR callback function
 *                            
 * 
 * @API TYPE - callback
 * @param  context    - context of the TSE MAC instance
 * @param  intnum - temporary storage
 * @return SUCCESS on success else ENP_LOGIC if error
 */
void tse_msgdmaTx_isr(void * context);


/* @Function Description -    Init and setup MSGDMA RX Descriptor chain
 *                            
 * 
 * @API TYPE - Internal
 * @return SUCCESS on success else ENP_NOBUFFER if error
 */
int tse_msgdma_read_init(ins_tse_info* tse_ptr);


/* @Function Description -    Init and setup MSGDMA TX Descriptor chain
 *                            
 * 
 * @API TYPE - Internal
 * @return SUCCESS on success else ENP_NOBUFFER if error
 */
int tse_msgdma_write_init(ins_tse_info* tse_ptr,unsigned int * ActualData,unsigned int len);


/* @Function Description -    TSE Driver MSGDMA RX ISR callback function
 *                            
 * 
 * @API TYPE        - callback internal function
 * @return SUCCESS on success else ENP_NORESOURCE, ENP_NOBUFFER if error
 */
void tse_mac_rcv(ins_tse_info* tse_ptr);

/* @Function Description -    allocate rx descriptor chain memory
 *                            
 * 
 * @API TYPE        - callback internal function
 * @return SUCCESS on success else 1 if error
 */
int allocate_rx_descriptor_chain(ins_tse_info* tse_ptr);


int tse_mac_stats(void * pio, int iface);


/**********************************
 *
 * TSE Driver Structure Definition
 *
 **********************************/

typedef struct 
{
    alt_iniche_dev              dev;
} altera_eth_tse_if;

typedef struct
{
    alt_iniche_dev              *p_dev;
    alt_u32                     hw_mac_base_addr;
    alt_u8                      hw_channel_number;
} alt_tse_iniche_dev_driver_data;

#define ALTERA_ETH_TSE_INSTANCE(inst_name, dev_inst)                                                                 \
altera_eth_tse_if dev_inst##_if[8];                                                                                  \
char *dev_inst##_name = inst_name##_NAME;

#define ALTERA_ETH_TSE_INIT(inst_name, dev_inst)                                                                     \
{                                                                                                                           \
     extern alt_u8 number_of_tse_mac;                                                                                       \
     extern alt_tse_iniche_dev_driver_data tse_iniche_dev_driver_data[MAXNETS];                                             \
                                                                                                                            \
     int dev_inst##_loop_control = 0;                                                                                       \
     int dev_inst##_number_of_channel = inst_name##_NUMBER_OF_CHANNEL;                                                      \
     if(dev_inst##_number_of_channel < 1) {                                                                                 \
        dev_inst##_number_of_channel = 1;                                                                                   \
     }                                                                                                                      \
                                                                                                                            \
     for(dev_inst##_loop_control = 0; dev_inst##_loop_control < dev_inst##_number_of_channel; dev_inst##_loop_control++) {  \
        dev_inst##_if[dev_inst##_loop_control].dev.llist.next     = 0;                                                      \
        dev_inst##_if[dev_inst##_loop_control].dev.llist.previous = 0;                                                      \
        dev_inst##_if[dev_inst##_loop_control].dev.name           = dev_inst##_name;                                        \
        dev_inst##_if[dev_inst##_loop_control].dev.init_func      = altera_eth_tse_init;                             \
                                                                                                                            \
        alt_iniche_dev_reg(&(dev_inst##_if[dev_inst##_loop_control].dev));                                                  \
        tse_iniche_dev_driver_data[number_of_tse_mac].p_dev = &(dev_inst##_if[dev_inst##_loop_control].dev);                \
        tse_iniche_dev_driver_data[number_of_tse_mac].hw_mac_base_addr = inst_name##_BASE;                                  \
        tse_iniche_dev_driver_data[number_of_tse_mac].hw_channel_number = dev_inst##_loop_control;                          \
        number_of_tse_mac++;                                                                                                \
     }                                                                                                                      \
}
     
error_t altera_eth_tse_init(
    alt_iniche_dev              *p_dev);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __ALTERA_ETH_TSE_INICHE_H__ */
