/******************************************************************************
*                                                                             *
* License Agreement                                                           *
*                                                                             *
* Copyright (c) 2016 Altera Corporation, San Jose, California, USA.           *
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
#ifdef ALT_INICHE

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <io.h>
#include <os_cpu.h>
#include "ipport.h"

#ifdef UCOS_II  
#include <ucos_ii.h>
#endif

#include "in_utils.h"
#include "netbuf.h"
#include "net.h"
#include "q.h"
#include "ether.h"
#include "system.h"
#include "alt_types.h"

#include "altera_avalon_timer_regs.h"
#include "altera_msgdma_descriptor_regs.h"
#include "altera_avalon_tse.h"

#include "sys/alt_irq.h"
#include "sys/alt_cache.h"

#include "altera_eth_tse_regs.h"
#include "iniche/altera_eth_tse_iniche.h"
#include "iniche/ins_tse_mac.h"

#include "socket.h"

/* Task includes */
#include "ipport.h"
#include "tcpapp.h"

#ifndef ALT_INICHE
#include "osport.h"
#endif

#ifdef ALT_INICHE
#include <errno.h>
#include "alt_iniche_dev.h"
#endif

ins_tse_info tse[MAXNETS];
extern alt_tse_system_info tse_mac_device[MAXNETS];

alt_u8 number_of_tse_mac = 0;
alt_tse_iniche_dev_driver_data tse_iniche_dev_driver_data[MAXNETS];
extern alt_u8 max_mac_system;

#ifdef ALT_INICHE


/* @Function Description: TSE MAC Driver Open/Initialization routine
 * @API TYPE: Public
 * @Param p_dec     pointer to TSE device instance
 * @Return ENP_HARDWARE on error, otherwise return SUCCESS
 */

error_t altera_eth_tse_init(
    alt_iniche_dev              *p_dev)
{
    int i;
    
    alt_tse_iniche_dev_driver_data *p_driver_data = 0;
    alt_tse_system_info *psys_info = 0;

    dprintf("altera_eth_tse_init %d\n", p_dev->if_num);
       
    /* Get the pointer to the alt_tse_iniche_dev_driver_data structure from the global array */
    for(i = 0; i < number_of_tse_mac; i++) {
        if(tse_iniche_dev_driver_data[i].p_dev == p_dev) {
            p_driver_data = &tse_iniche_dev_driver_data[i];
        }
    }
    /* If pointer could not found */
    if(p_driver_data == 0) {
        return ENP_HARDWARE;
    }
    
    /* Get the pointer to the alt_tse_system_info structure from the global array */
    for(i = 0; i < max_mac_system; i++) {
        if(tse_mac_device[i].tse_mac_base == p_driver_data->hw_mac_base_addr) {
            psys_info = &tse_mac_device[i];
        }
    }
    /* If pointer could not found */
    if(psys_info == 0) {
        return ENP_HARDWARE;
    }
    
    prep_tse_mac(p_dev->if_num, psys_info + p_driver_data->hw_channel_number);
    
    return SUCCESS;
}
#endif /* ALT_INICHE */


/* @Function Description: TSE MAC Driver Open/Registration routine
 * @API TYPE: Internal
 * @Param index     index of the NET structure associated with TSE instance
 * @Param psys_info pointer to the TSE hardware info structure
 * @Return next index of NET
 */
int prep_tse_mac(int index, alt_tse_system_info *psys_info)
{
    NET ifp;
    dprintf("prep_tse_mac %d\n", index);
    {
        tse[index].sem = 0; /*Tx IDLE*/
        tse[index].tse = (void *)psys_info;

        ifp = nets[index];
        ifp->n_mib->ifAdminStatus = ALTERA_TSE_ADMIN_STATUS_DOWN; /* status = down */
        ifp->n_mib->ifOperStatus =  ALTERA_TSE_ADMIN_STATUS_DOWN;   
        ifp->n_mib->ifLastChange =  cticks * (100/TPS);
        ifp->n_mib->ifPhysAddress = (u_char*)tse[index].mac_addr;
        ifp->n_mib->ifDescr =       "Altera TSE MAC ethernet";
        ifp->n_lnh =                ETHHDR_SIZE; /* ethernet header size. was:14 */
        ifp->n_hal =                ALTERA_TSE_HAL_ADDR_LEN;  /* hardware address length */
        ifp->n_mib->ifType =        ETHERNET;   /* device type */
        ifp->n_mtu =                ALTERA_TSE_MAX_MTU_SIZE;  /* max frame size */
    
        /* install our hardware driver routines */
        ifp->n_init =       tse_mac_init;
        ifp->pkt_send =     NULL;
        ifp->raw_send =     tse_mac_raw_send;
        ifp->n_close =      tse_mac_close;
        ifp->n_stats =      (void(*)(void *, int))tse_mac_stats; 
    
    #ifdef IP_V6
        ifp->n_flags |= (NF_NBPROT | NF_IPV6);
    #else
        ifp->n_flags |= NF_NBPROT;
    #endif
    
        nets[index]->n_mib->ifPhysAddress = (u_char*)tse[index].mac_addr;   /* ptr to MAC address */
    
    #ifdef ALT_INICHE
        /* get the MAC address. */
        get_mac_addr(ifp, (unsigned char *)tse[index].mac_addr);
    #endif /* ALT_INICHE */
    
        /* set cross-pointers between iface and tse structs */
        tse[index].index = index;
        tse[index].netp = ifp;
        ifp->n_local = (void*)(&tse[index]);
    
        index++;
   }
 
   return index;
}

//temporary code for msgdma hw workaround
void msgdma_reset(alt_msgdma_dev * dev)
{

   /* start prefetcher reset sequence */
   IOWR_ALT_MSGDMA_PREFETCHER_CONTROL(dev->prefetcher_base, 
   ALT_MSGDMA_PREFETCHER_CTRL_RESET_SET_MASK);
   /* wait until hw clears the bit */
   while(ALT_MSGDMA_PREFETCHER_CTRL_RESET_GET(
       IORD_ALT_MSGDMA_PREFETCHER_CONTROL(dev->prefetcher_base)));
   /*
    * This reset is intended to be used along with reset dispatcher in 
    * dispatcher core. Once the reset sequence in prefetcher core has 
    * completed, software is expected to reset the dispatcher core, 
    * and polls for dispatcher?s reset sequence to be completed.
    */

    /* Reset the registers and FIFOs of the dispatcher and master modules */
    /* set the reset bit, no need to read the control register first since 
    this write is going to clear it out */
    IOWR_ALTERA_MSGDMA_CSR_CONTROL(dev->csr_base, ALTERA_MSGDMA_CSR_RESET_MASK);
    while(0 != (IORD_ALTERA_MSGDMA_CSR_STATUS(dev->csr_base)
                   & ALTERA_MSGDMA_CSR_RESET_STATE_MASK));

}  

/* @Function Description: TSE MAC Initialization routine. This function opens the
 *                        device handle, configure the callback function and interrupts ,
 *                        for MSGDMA TX and MSGDMA RX block associated with the TSE MAC,
 *                        Initialize the MAC Registers for the RX FIFO and TX FIFO
 *                        threshold watermarks, initialize the tse device structure,
 *                        set the MAC address of the device and enable the MAC   
 * 
 * @API TYPE: Internal
 * @Param iface index of the NET structure associated with TSE instance
 * @Return 0 if ok, else -1 if error
 */
int tse_mac_init(int iface)
{
   int dat;
   int speed, duplex, result, x;
   int status = SUCCESS;
   
   alt_msgdma_dev *msgdma_tx_dev;
   alt_msgdma_dev *msgdma_rx_dev;
   alt_tse_system_info* tse_hw = (alt_tse_system_info *) tse[iface].tse;
   
   dprintf("tse_mac_init %d\n", iface);

    if (tse_hw->ext_desc_mem == 1) {
        tse[iface].rxdesc[0] = (alt_msgdma_prefetcher_standard_descriptor *) tse_hw->desc_mem_base;
        tse[iface].rxdesc[1] = (alt_msgdma_prefetcher_standard_descriptor *) 
               (tse_hw->desc_mem_base + ((1+ALTERA_TSE_MSGDMA_RX_DESC_CHAIN_SIZE)*(sizeof(alt_msgdma_prefetcher_standard_descriptor))));          
        tse[iface].txdesc = (alt_msgdma_prefetcher_standard_descriptor *) 
               (tse_hw->desc_mem_base + ((1+ALTERA_TSE_MSGDMA_RX_DESC_CHAIN_SIZE+1+ALTERA_TSE_MSGDMA_RX_DESC_CHAIN_SIZE)*(sizeof(alt_msgdma_prefetcher_standard_descriptor))));          
    }
    else {
        tse[iface].rxdesc[0] = (alt_msgdma_prefetcher_standard_descriptor *)alt_uncached_malloc((1+ALTERA_TSE_MSGDMA_RX_DESC_CHAIN_SIZE)*(sizeof(alt_msgdma_prefetcher_standard_descriptor)));
        while ((((alt_u32)tse[iface].rxdesc[0]) % sizeof(alt_msgdma_prefetcher_standard_descriptor)) != 0) 
        tse[iface].rxdesc[0]++;  //boundary
          
        tse[iface].rxdesc[1] = (alt_msgdma_prefetcher_standard_descriptor *)alt_uncached_malloc((1+ALTERA_TSE_MSGDMA_RX_DESC_CHAIN_SIZE)*(sizeof(alt_msgdma_prefetcher_standard_descriptor)));
        while ((((alt_u32)tse[iface].rxdesc[1]) % sizeof(alt_msgdma_prefetcher_standard_descriptor)) != 0) 
        tse[iface].rxdesc[1]++;  //boundary  
    
        tse[iface].txdesc = (alt_msgdma_prefetcher_standard_descriptor *)alt_uncached_malloc((1+ALTERA_TSE_MSGDMA_TX_DESC_CHAIN_SIZE)*(sizeof(alt_msgdma_prefetcher_standard_descriptor)));
        while ((((alt_u32)tse[iface].txdesc) % sizeof(alt_msgdma_prefetcher_standard_descriptor)) != 0)  
          tse[iface].txdesc++;  //boundary
    }
   
    /* Get the Rx and Tx MSGDMA addresses */
    msgdma_tx_dev = alt_msgdma_open(tse_hw->tse_msgdma_tx); 
    
    if(!msgdma_tx_dev) {
      dprintf("[altera_eth_tse_init] Error opening TX MSGDMA\n");
      return ENP_RESOURCE;
    }
  
    msgdma_rx_dev = alt_msgdma_open(tse_hw->tse_msgdma_rx);
    if(!msgdma_rx_dev) {
      dprintf("[altera_eth_tse_init] Error opening RX MSGDMA\n");
      return ENP_RESOURCE;
    }

    /* Initialize mtip_mac_trans_info structure with values from <system.h>*/
    tse_mac_initTransInfo2(&tse[iface].mi, (int)tse_hw->tse_mac_base,
                                   (unsigned int)msgdma_tx_dev,            
                                   (unsigned int)msgdma_rx_dev,
                                   0);

   /* reset the PHY if necessary */   
   result = getPHYSpeed(tse[iface].mi.base);
   speed = (result >> 1) & 0x07;
   duplex = result & 0x01;
   
   /* reset the mac */ 
   IOWR_ALTERA_TSEMAC_CMD_CONFIG(tse[iface].mi.base,
                             mmac_cc_SW_RESET_mask | 
                             mmac_cc_TX_ENA_mask | 
                             mmac_cc_RX_ENA_mask);
  
   x=0;
   while(IORD_ALTERA_TSEMAC_CMD_CONFIG(tse[iface].mi.base) & 
         ALTERA_TSEMAC_CMD_SW_RESET_MSK) {
     if( x++ > 10000 ) {
       break;
     }
   }
   if(x >= 10000) {
     dprintf("TSEMAC SW reset bit never cleared!\n");
   }

   dat = IORD_ALTERA_TSEMAC_CMD_CONFIG(tse[iface].mi.base);
   if( (dat & 0x03) != 0 ) {
     dprintf("WARN: RX/TX not disabled after reset... missing PHY clock? CMD_CONFIG=0x%08x\n", dat);
   } 
   else {
     dprintf("OK, x=%d, CMD_CONFIG=0x%08x\n", x, dat);
   }
  
   /* Hack code to determine the Channel number <- Someone please fix this ugly code in the future */
   extern alt_u8 mac_group_count;
   extern alt_tse_mac_group *pmac_groups[TSE_MAX_MAC_IN_SYSTEM];
      
   if(tse_hw->use_shared_fifo == 1) {
     int channel_loop = 0;
     int mac_loop = 0;
         
     for (channel_loop = 0; channel_loop < mac_group_count; channel_loop ++) {
       for (mac_loop = 0; mac_loop < pmac_groups[channel_loop]->channel; mac_loop ++) {
         if (pmac_groups[channel_loop]->pmac_info[mac_loop]->psys_info == tse_hw) {
           tse[iface].channel = mac_loop;
         }
       }
     }
   }
   /* End of Hack code */
  
   if(tse_hw->use_shared_fifo == 1) {
      IOWR_ALTERA_MULTI_CHAN_FIFO_SEC_FULL_THRESHOLD(tse_hw->tse_shared_fifo_rx_ctrl_base,tse_hw->tse_shared_fifo_rx_depth);
      IOWR_ALTERA_MULTI_CHAN_FIFO_ALMOST_FULL_THRESHOLD(tse_hw->tse_shared_fifo_rx_ctrl_base,((tse_hw->tse_shared_fifo_rx_depth) - 140));
   }
   else {
      /* Initialize MAC registers */
      IOWR_ALTERA_TSEMAC_FRM_LENGTH(tse[iface].mi.base, ALTERA_TSE_MAC_MAX_FRAME_LENGTH); 
      IOWR_ALTERA_TSEMAC_RX_ALMOST_EMPTY(tse[iface].mi.base, 8);
      IOWR_ALTERA_TSEMAC_RX_ALMOST_FULL(tse[iface].mi.base, 8);
      IOWR_ALTERA_TSEMAC_TX_ALMOST_EMPTY(tse[iface].mi.base, 8);
      IOWR_ALTERA_TSEMAC_TX_ALMOST_FULL(tse[iface].mi.base,  3);
      IOWR_ALTERA_TSEMAC_TX_SECTION_EMPTY(tse[iface].mi.base, tse_hw->tse_tx_depth - 16); //1024/4;  
      IOWR_ALTERA_TSEMAC_TX_SECTION_FULL(tse[iface].mi.base,  0); //32/4; // start transmit when there are 48 bytes
      IOWR_ALTERA_TSEMAC_RX_SECTION_EMPTY(tse[iface].mi.base, tse_hw->tse_rx_depth - 16); //4000/4);
      IOWR_ALTERA_TSEMAC_RX_SECTION_FULL(tse[iface].mi.base,  0);
   }
  
   /* Enable TX shift 16 for removing two bytes from the start of all transmitted frames */ 
   if((ETHHDR_BIAS !=0) && (ETHHDR_BIAS !=2)) {
     dprintf("[tse_mac_init] Error: Unsupported Ethernet Header Bias Value, %d\n",ETHHDR_BIAS);
     return ENP_PARAM;
   }
 
   if(ETHHDR_BIAS == 0) {
     alt_32 temp_reg;
    
     temp_reg = IORD_ALTERA_TSEMAC_TX_CMD_STAT(tse[iface].mi.base) & (~ALTERA_TSEMAC_TX_CMD_STAT_TXSHIFT16_MSK);
     IOWR_ALTERA_TSEMAC_TX_CMD_STAT(tse[iface].mi.base,temp_reg);
 
     /* 
      * check if the MAC supports the 16-bit shift option allowing us
      * to send BIASed frames without copying. Used by the send function later. 
      */
     if(IORD_ALTERA_TSEMAC_TX_CMD_STAT(tse[iface].mi.base) & 
        ALTERA_TSEMAC_TX_CMD_STAT_TXSHIFT16_MSK) {
        tse[iface].txShift16OK = 1;
        dprintf("[tse_mac_init] Error: Incompatible %d value with TX_CMD_STAT register return TxShift16 value. \n",ETHHDR_BIAS);
       return ENP_LOGIC;
     } else {
       tse[iface].txShift16OK = 0;
     }
   
    /*Enable RX shift 16 for alignment of all received frames on 16-bit start address */   
    temp_reg = IORD_ALTERA_TSEMAC_RX_CMD_STAT(tse[iface].mi.base) & (~ALTERA_TSEMAC_RX_CMD_STAT_RXSHIFT16_MSK);
    IOWR_ALTERA_TSEMAC_RX_CMD_STAT(tse[iface].mi.base,temp_reg);
   
    /* check if the MAC supports the 16-bit shift option at the RX CMD STATUS Register  */ 
    if(IORD_ALTERA_TSEMAC_RX_CMD_STAT(tse[iface].mi.base) & ALTERA_TSEMAC_RX_CMD_STAT_RXSHIFT16_MSK)
    {
      tse[iface].rxShift16OK = 1;
      dprintf("[tse_mac_init] Error: Incompatible %d value with RX_CMD_STAT register return RxShift16 value. \n",ETHHDR_BIAS);
      return ENP_LOGIC;
    } 
    else {
      tse[iface].rxShift16OK = 0;
    }
  } /* if(ETHHDR_BIAS == 0) */
 
  if(ETHHDR_BIAS == 2) {
    IOWR_ALTERA_TSEMAC_TX_CMD_STAT(tse[iface].mi.base,ALTERA_TSEMAC_TX_CMD_STAT_TXSHIFT16_MSK);
 
    /*
     * check if the MAC supports the 16-bit shift option allowing us
     * to send BIASed frames without copying. Used by the send function later.
     */
    if(IORD_ALTERA_TSEMAC_TX_CMD_STAT(tse[iface].mi.base) &
      ALTERA_TSEMAC_TX_CMD_STAT_TXSHIFT16_MSK) {
      tse[iface].txShift16OK = 1;
    } 
    else {
      tse[iface].txShift16OK = 0;
      dprintf("[tse_mac_init] Error: Incompatible %d value with TX_CMD_STAT register return TxShift16 value. \n",ETHHDR_BIAS);
      return ENP_LOGIC;
    }
  
    /* Enable RX shift 16 for alignment of all received frames on 16-bit start address */
    IOWR_ALTERA_TSEMAC_RX_CMD_STAT(tse[iface].mi.base,ALTERA_TSEMAC_RX_CMD_STAT_RXSHIFT16_MSK);
 
    /* check if the MAC supports the 16-bit shift option at the RX CMD STATUS Register  */ 
    if(IORD_ALTERA_TSEMAC_RX_CMD_STAT(tse[iface].mi.base) & ALTERA_TSEMAC_RX_CMD_STAT_RXSHIFT16_MSK)
    {
      tse[iface].rxShift16OK = 1;
    } 
    else {
      tse[iface].rxShift16OK = 0;
      dprintf("[tse_mac_init] Error: Incompatible %d value with RX_CMD_STAT register return RxShift16 value. \n",ETHHDR_BIAS);
      return ENP_LOGIC;
    }
  } /* if(ETHHDR_BIAS == 2) */
  
  /* enable MAC */
  dat = ALTERA_TSEMAC_CMD_TX_ENA_MSK       |
        ALTERA_TSEMAC_CMD_RX_ENA_MSK       |
        mmac_cc_RX_ERR_DISCARD_mask        |
#if ENABLE_PHY_LOOPBACK
        ALTERA_TSEMAC_CMD_PROMIS_EN_MSK    |     // promiscuous mode
        ALTERA_TSEMAC_CMD_LOOPBACK_MSK     |     // promiscuous mode
#endif
        ALTERA_TSEMAC_CMD_TX_ADDR_INS_MSK  |
        ALTERA_TSEMAC_CMD_RX_ERR_DISC_MSK;  /* automatically discard frames with CRC errors */
    
  
  /* 1000 Mbps */
  if(speed == 0x01) {
    dat |= ALTERA_TSEMAC_CMD_ETH_SPEED_MSK;
    dat &= ~ALTERA_TSEMAC_CMD_ENA_10_MSK;
  }
  /* 100 Mbps */
  else if(speed == 0x02) {
    dat &= ~ALTERA_TSEMAC_CMD_ETH_SPEED_MSK;
    dat &= ~ALTERA_TSEMAC_CMD_ENA_10_MSK;
  }
  /* 10 Mbps */
  else if(speed == 0x04) {
    dat &= ~ALTERA_TSEMAC_CMD_ETH_SPEED_MSK;
    dat |= ALTERA_TSEMAC_CMD_ENA_10_MSK;
  }
  /* default to 100 Mbps if returned invalid speed */
  else {
    dat &= ~ALTERA_TSEMAC_CMD_ETH_SPEED_MSK;
    dat &= ~ALTERA_TSEMAC_CMD_ENA_10_MSK;
  }
  
  /* Half Duplex */
  if(duplex == TSE_PHY_DUPLEX_HALF) {
    dat |= ALTERA_TSEMAC_CMD_HD_ENA_MSK;
  }
  /* Full Duplex */
  else {
    dat &= ~ALTERA_TSEMAC_CMD_HD_ENA_MSK;
  }
          
  IOWR_ALTERA_TSEMAC_CMD_CONFIG(tse[iface].mi.base, dat);
  dprintf("\nMAC post-initialization: CMD_CONFIG=0x%08x\n", 
  IORD_ALTERA_TSEMAC_CMD_CONFIG(tse[iface].mi.base));
  
                          
#ifdef ALT_INICHE
   /* Set the MAC address */  
   IOWR_ALTERA_TSEMAC_MAC_0(tse[iface].mi.base,
                           ((int)((unsigned char) tse[iface].mac_addr[0]) | 
                            (int)((unsigned char) tse[iface].mac_addr[1] <<  8) |
                            (int)((unsigned char) tse[iface].mac_addr[2] << 16) | 
                            (int)((unsigned char) tse[iface].mac_addr[3] << 24)));
  
   IOWR_ALTERA_TSEMAC_MAC_1(tse[iface].mi.base, 
                           (((int)((unsigned char) tse[iface].mac_addr[4]) | 
                             (int)((unsigned char) tse[iface].mac_addr[5] <<  8)) & 0xFFFF));
   
#else /* not ALT_INICHE */

   /* Set the MAC address */  
   IOWR_ALTERA_TSEMAC_MAC_0(tse[iface].mi.base,
                           ((int)(0x00)       | 
                            (int)(0x07 <<  8) |
                            (int)(0xAB << 16) | 
                            (int)(0xF0 << 24)));

   IOWR_ALTERA_TSEMAC_MAC_1(tse[iface].mi.base, 
                           (((int)(0x0D)      | 
                             (int)(0xBA <<  8)) & 0xFFFF));


   /* Set the mac address in the tse struct */
   tse[iface].mac_addr[0] = 0x00;
   tse[iface].mac_addr[1] = 0x07;
   tse[iface].mac_addr[2] = 0xAB;
   tse[iface].mac_addr[3] = 0xF0;
   tse[iface].mac_addr[4] = 0x0D;
   tse[iface].mac_addr[5] = 0xBA;

#endif /* not ALT_INICHE */

   /* status = UP */ 
   nets[iface]->n_mib->ifAdminStatus = ALTERA_TSE_ADMIN_STATUS_UP;    
   nets[iface]->n_mib->ifOperStatus =  ALTERA_TSE_ADMIN_STATUS_UP;
   
   /* Install MSGDMA (RX) interrupt handler */
   alt_msgdma_register_callback(
        tse[iface].mi.rx_msgdma,
        (alt_msgdma_callback)&tse_msgdmaRx_isr,
        0,
        (void*)(&tse[iface]));
        
           /* Install MSGDMA (TX) interrupt handler */
    alt_msgdma_register_callback(
          tse[iface].mi.tx_msgdma,
          (alt_msgdma_callback)&tse_msgdmaTx_isr,
          0,
          (void*)(&tse[iface]));
    
  status = tse_msgdma_read_init(&tse[iface]);
  if (status == 0 ) status = tse_msgdma_write_init(&tse[iface],0,0);
  
  if (status!=0) dprintf("TSE_MAC_INIT error\n");
  
  return status;
}

/* @Function Description -  Init and setup MSGDMA TX Descriptor chain
 *                          
 * 
 * @API TYPE - Internal
 * @return SUCCESS on success 
 */
int tse_msgdma_write_init(ins_tse_info* tse_ptr,unsigned int * ActualData,unsigned int len)
{     
  alt_u32 control = 0;
  int desc_index;
  int rc;
  
  tse_ptr->txdesc_list = NULL;
  
  for(desc_index = 0; desc_index < (ALTERA_TSE_MSGDMA_TX_DESC_CHAIN_SIZE); desc_index++)
  { 
        
    /* trigger interrupt when transfer complete */
    control =  ALTERA_MSGDMA_DESCRIPTOR_CONTROL_GENERATE_SOP_MASK | ALTERA_MSGDMA_DESCRIPTOR_CONTROL_GENERATE_EOP_MASK; 
            
    if (desc_index >= ( ALTERA_TSE_MSGDMA_TX_DESC_CHAIN_SIZE - 2)) control |= ALTERA_MSGDMA_DESCRIPTOR_CONTROL_TRANSFER_COMPLETE_IRQ_MASK;
    else control  |= ALTERA_MSGDMA_DESCRIPTOR_CONTROL_EARLY_DONE_ENABLE_MASK;
            
    rc=alt_msgdma_construct_prefetcher_standard_mm_to_st_descriptor(
            tse_ptr->mi.tx_msgdma,
            (alt_msgdma_prefetcher_standard_descriptor *) &tse_ptr->txdesc[desc_index],  
            (int)ActualData,   
            len,
            control);
    if (rc!=0) return -1;
    
    if (desc_index==0) tse_ptr->txdesc_list = NULL; 
            
    rc=alt_msgdma_prefetcher_add_standard_desc_to_list(
               &tse_ptr->txdesc_list,
               &tse_ptr->txdesc[desc_index] );        
    if (rc!=0) return -1;

  } 
  
  return 0;
}

/* @Function Description -  TSE transmit API to send data to the MAC
 *                          
 * 
 * @API TYPE - Public
 * @param  net  - NET structure associated with the TSE MAC instance
 * @param  data - pointer to the data payload
 * @param  data_bytes - number of bytes of the data payload to be sent to the MAC
 * @return SUCCESS if success, else a negative value
 */
int tse_mac_raw_send(NET net, char * data, unsigned int data_bytes)
{
   unsigned int len = data_bytes;
   int rc;

   ins_tse_info* tse_ptr = (ins_tse_info*) net->n_local;
   tse_mac_trans_info *mi;
   unsigned int* ActualData;
   int cpu_sr;
   
   OS_ENTER_CRITICAL();
   mi = &tse_ptr->mi;
   
   if(tse_ptr->sem!=0) /* Tx is busy*/
   {
      dprintf("raw_send CALLED AGAIN!!!\n");
      OS_EXIT_CRITICAL();
      return ENP_RESOURCE;
   }
 
   tse_ptr->sem = 1;  
   
   // clear bit-31 before passing it to MSGDMA Driver
   ActualData = (unsigned int*)alt_remap_cached ((volatile void*) data, 4);

   #if (0)
   rc= tse_msgdma_write_init(tse_ptr,ActualData,len);
   if (rc<0) 
   {
     dprintf("tse_msgdma_write_init bad return\n");
     OS_EXIT_CRITICAL();
     return -1;
   }
   #else
     tse_ptr->txdesc[0].read_address = (alt_u32)ActualData;
     tse_ptr->txdesc[0].transfer_length = len;
     tse_ptr->txdesc[0].control = (tse_ptr->txdesc[0].control
            & ALT_MSGDMA_PREFETCHER_DESCRIPTOR_CTRL_OWN_BY_HW_CLR_MASK) 
            | ALTERA_MSGDMA_DESCRIPTOR_CONTROL_GO_MASK;
   #endif  
        
   alt_dcache_flush(ActualData,len);   
   rc = tse_mac_aTxWrite(mi,tse_ptr->txdesc);
   if(rc < 0)   /* MSGDMA not available */
   {
      dprintf("raw_send() MSGDMA not available, ret=%d, len=%d\n",rc, len);
      net->n_mib->ifOutDiscards++;
      tse_ptr->sem = 0;

      OS_EXIT_CRITICAL();
      return SEND_DROPPED;   /* ENP_RESOURCE and SEND_DROPPED have the same value! */
   }
   else   /* = 0, success */
   {
      net->n_mib->ifOutOctets += data_bytes;
      /* we dont know whether it was unicast or not, we count both in <ifOutUcastPkts> */
      net->n_mib->ifOutUcastPkts++;
      tse_ptr->sem = 0;

      OS_EXIT_CRITICAL();
      return SUCCESS;  /*success */
   }
}



/* @Function Description -  TSE Driver MSGDMA RX ISR callback function
 *                          
 * 
 * @API TYPE - callback
 * @param  context  - context of the TSE MAC instance
 * @param  intnum - temporary storage
 */
void tse_msgdmaRx_isr(void * context)
{
    ins_tse_info* tse_ptr = (ins_tse_info *) context; 
    alt_u32 msgdma_status;
    alt_u32 i,control;
    
    /* Capture current rcv queue length */
    int initial_rcvdq_len = rcvdq.q_len;

    /* reenable global interrupts so we don't miss one that occurs during the
       processing of this ISR */
    IOWR_ALT_MSGDMA_PREFETCHER_CONTROL(tse_ptr->mi.rx_msgdma->prefetcher_base,
      			IORD_ALT_MSGDMA_PREFETCHER_CONTROL(tse_ptr->mi.rx_msgdma->prefetcher_base)
	  			| ALT_MSGDMA_PREFETCHER_CTRL_GLOBAL_INTR_EN_SET_MASK);
       
    msgdma_status = IORD_ALTERA_MSGDMA_CSR_STATUS(tse_ptr->mi.rx_msgdma->csr_base);
       
    if ((msgdma_status & ALTERA_MSGDMA_CSR_STOPPED_ON_ERROR_MASK)==0)
    {   
       /* Handle received packet */
        tse_mac_rcv(tse_ptr); 
        
        /* read the control field of the last descriptor in the chain */
        control = IORD_32DIRECT(&tse_ptr->rxdesc[tse_ptr->rx_chain][ALTERA_TSE_MSGDMA_RX_DESC_CHAIN_SIZE-2],0x1c);
                          
        //if the chain is completed then start a new chain
        if ((control & ALT_MSGDMA_PREFETCHER_DESCRIPTOR_CTRL_OWN_BY_HW_SET_MASK)==0)            
        {
            /* process any unprocessed descriptors */      
            for (i=(tse_ptr->rx_descriptor_index);i<(ALTERA_TSE_MSGDMA_RX_DESC_CHAIN_SIZE-1);i++)
            {
                tse_mac_rcv(tse_ptr); 
            }
            
            /* cancel any pending ints */
            /* the chain could have been completed and int generated during the processing of this ISR */
            /* But we are handling that in this ISR, so cancel any pending interrupt */
            IOWR_ALT_MSGDMA_PREFETCHER_STATUS(tse_ptr->mi.rx_msgdma->prefetcher_base,1);
          
            /* switch chains */
            tse_ptr->rx_descriptor_index = 0;
            if (tse->rx_chain == 0) tse->rx_chain=1; else tse->rx_chain=0; 
                
            /* start new chain */            
            tse_mac_aRxRead(&tse_ptr->mi, tse_ptr->rxdesc_list[tse->rx_chain]);
            
            /* allocate storage for the non active chain */
            allocate_rx_descriptor_chain(tse_ptr); 
        }    
        
        /* Wake up Niche stack if there are new packets are on queue */
        if ((rcvdq.q_len) > initial_rcvdq_len) {
            SignalPktDemux();
        }       
    } /* if (no error) */
    else {  dprintf("RX ERROR\n");  }
    
}

/* @Function Description -  TSE Driver MSGDMA TX ISR callback function
 *                          
 * 
 * @API TYPE - callback
 * @param  context  - context of the TSE MAC instance
 */
void tse_msgdmaTx_isr(void * context)
{
  ins_tse_info* tse_ptr = (ins_tse_info *) context; 
  int msgdma_status;

  /* 
   * The MSGDMA interrupt source was cleared in the MSGDMA ISR entry, 
   * which called this routine. New interrupt sources will cause the 
   * IRQ to fire again once this routine returns.
   */        
  
  /* 
   * Grab MSGDMA status to validate interrupt cause. 
   * 
   * IO read to peripheral that generated the IRQ is done after IO write
   * to negate the interrupt request. This ensures at the IO write reaches 
   * the peripheral (through any high-latency hardware in the system)
   * before the ISR exits.
   */   
  msgdma_status = IORD_ALTERA_MSGDMA_CSR_STATUS(tse_ptr->mi.tx_msgdma->csr_base);
  
  if ((msgdma_status & ALTERA_MSGDMA_CSR_STOPPED_ON_ERROR_MASK)!=0)
      dprintf("TX STOPPED\n");
   
}


/* @Function Description -  Init and setup MSGDMA Descriptor chain
 *                          
 * 
 * @API TYPE - Internal
 * @return SUCCESS on success 
 */
int tse_msgdma_read_init(ins_tse_info* tse_ptr)
{     
  alt_u32 *uncached_packet_payload;
  alt_u32 control = 0;
  int desc_index;
  int chain_index;
  int rc;
  int max_transfer_size=0xffff;
  
  if  (tse_ptr->mi.rx_msgdma->max_byte < max_transfer_size) { max_transfer_size = tse_ptr->mi.rx_msgdma->max_byte; }
   
  for (chain_index=0;chain_index<2;chain_index++)
  { 
    tse_ptr->rxdesc_list[chain_index] = NULL;
  
    for(desc_index = 0; desc_index < ALTERA_TSE_MSGDMA_RX_DESC_CHAIN_SIZE; desc_index++)
    { 
      uncached_packet_payload = NULL;
      
      if ((desc_index < (ALTERA_TSE_MSGDMA_RX_DESC_CHAIN_SIZE-1))) {  
        tse_ptr->pkt_array_rx[chain_index][desc_index] = pk_alloc(ALTERA_TSE_PKT_INIT_LEN);
      
        if (!tse_ptr->pkt_array_rx[chain_index][desc_index])   /* couldn't get a free buffer for rx */
        {
          dprintf("[tse_msgdma_read_init] Fatal error: No free packet buffers for RX\n");
          tse_ptr->netp->n_mib->ifInDiscards++;
        
          return ENP_NOBUFFER;
        }
      
        // ensure bit-31 of tse_ptr->pkt_array_rx[desc_index]->nb_buff is clear before passing
        // to MSGDMA Driver
        uncached_packet_payload = (alt_u32 *)alt_remap_cached ((volatile void*) tse_ptr->pkt_array_rx[chain_index][desc_index]->nb_buff, 4);
        alt_dcache_flush((void *) uncached_packet_payload, ALTERA_TSE_PKT_INIT_LEN);
      }
   
      /* trigger interrupt when transfer complete */
      control =  ALTERA_MSGDMA_DESCRIPTOR_CONTROL_TRANSFER_COMPLETE_IRQ_MASK |
            ALTERA_MSGDMA_DESCRIPTOR_CONTROL_ERROR_IRQ_MASK | ALTERA_MSGDMA_DESCRIPTOR_CONTROL_END_ON_EOP_MASK; 
            
      rc=alt_msgdma_construct_prefetcher_standard_st_to_mm_descriptor(
            tse_ptr->mi.rx_msgdma,
            (alt_msgdma_prefetcher_standard_descriptor *) &tse_ptr->rxdesc[chain_index][desc_index],  
            (alt_u32)uncached_packet_payload,   
            max_transfer_size,
            control);
      if (rc!=0) return -1;
      
      if (desc_index==0) tse_ptr->rxdesc_list[chain_index] = NULL;  
            
      rc=alt_msgdma_prefetcher_add_standard_desc_to_list(
               &tse_ptr->rxdesc_list[chain_index],
               &tse_ptr->rxdesc[chain_index][desc_index] );        
      if (rc!=0) return -1;
    }

  } 

  dprintf("[tse_msgdma_read_init] RX descriptor chain desc (%d depth) created\n",  desc_index);
   
  tse_ptr->rx_descriptor_index=0;   //for processing completed rx descriptors
  tse_ptr->rx_chain=0;
  tse_mac_aRxRead( &tse_ptr->mi, tse_ptr->rxdesc_list[tse_ptr->rx_chain]);
  
  return SUCCESS;
}

/* allocate the storage for the non active rx descriptor chain 
   update the write pointers in each descriptor to point
   to the allocated storage. */
int allocate_rx_descriptor_chain(ins_tse_info* tse_ptr)
{
    PACKET replacement_pkt;
    alt_u32 *uncached_packet_payload;
    alt_msgdma_prefetcher_standard_descriptor *rxDesc;
    int i;
    
    for (i=0;i<(ALTERA_TSE_MSGDMA_RX_DESC_CHAIN_SIZE-1);i++)
    {
        replacement_pkt = pk_alloc(ALTERA_TSE_PKT_INIT_LEN);
        if (!replacement_pkt) { /* couldn't get a free buffer for rx */
          dprintf("No free buffers for rx\n");
          return 1;
        }
        else
        {
            rxDesc = &tse_ptr->rxdesc[!tse_ptr->rx_chain][i];
            tse_ptr->pkt_array_rx[!tse_ptr->rx_chain][i] = replacement_pkt;
            uncached_packet_payload = (alt_u32 *)alt_remap_cached(tse_ptr->pkt_array_rx[!tse_ptr->rx_chain][i]->nb_buff, 4);
            alt_dcache_flush((void *) uncached_packet_payload, ALTERA_TSE_PKT_INIT_LEN);
            rxDesc->write_address = (alt_u32)(uncached_packet_payload);
        }
    } 
    
    return 0;
}

/* @Function Description -  TSE Driver MSGDMA RX ISR callback function
 *                          
 * 
 * @API TYPE        - callback internal function
 * @return SUCCESS on success
 */

void tse_mac_rcv(ins_tse_info* tse_ptr)
{     
    struct ethhdr * eth;
    int pklen;
    PACKET rx_packet;
        
    /* Correct frame length to actual (this is different from TX side) */
    pklen = IORD_32DIRECT(&tse_ptr->rxdesc[tse_ptr->rx_chain][tse_ptr->rx_descriptor_index].bytes_transfered,0) - 2;  
    
    tse_ptr->netp->n_mib->ifInOctets += (u_long)pklen;
    
    rx_packet = tse_ptr->pkt_array_rx[tse_ptr->rx_chain][tse_ptr->rx_descriptor_index];   
    rx_packet->nb_prot = rx_packet->nb_buff + ETHHDR_SIZE;
    rx_packet->nb_plen = pklen - 14;
    rx_packet->nb_tstamp = cticks;
    rx_packet->net = tse_ptr->netp;
    
    // set packet type for demux routine
    eth = (struct ethhdr *)(rx_packet->nb_buff + ETHHDR_BIAS);
    rx_packet->type = eth->e_type;
    
    putq(&rcvdq, rx_packet);
              
    tse_ptr->rx_descriptor_index++; 
} 

int tse_mac_stats(void * pio, int iface)
{
    ns_printf(pio, "tse_mac_stats(), stats will be added later!\n");
    return SUCCESS;
}

/* @Function Description -  Closing the TSE MAC Driver Interface
 *                          
 * 
 * @API TYPE - Public
 * @param  iface    index of the NET interface associated with the TSE MAC.
 * @return SUCCESS
 */
int tse_mac_close(int iface)
{
    int state;
     
    /* status = down */
    nets[iface]->n_mib->ifAdminStatus = ALTERA_TSE_ADMIN_STATUS_DOWN;    
    
    /* disable the interrupt in the OS*/
    alt_msgdma_register_callback(tse[iface].mi.rx_msgdma, 0, 0, 0);
     
    /* Disable Receive path on the device*/
    state = IORD_ALTERA_TSEMAC_CMD_CONFIG(tse[iface].mi.base);
    IOWR_ALTERA_TSEMAC_CMD_CONFIG(tse[iface].mi.base,state & ~ALTERA_TSEMAC_CMD_RX_ENA_MSK); 
    
    /* status = down */                                     
    nets[iface]->n_mib->ifOperStatus = ALTERA_TSE_ADMIN_STATUS_DOWN;     
    
    return SUCCESS;
}
#endif /* ALT_INICHE */
