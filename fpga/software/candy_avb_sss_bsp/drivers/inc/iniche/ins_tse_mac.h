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

#ifndef _ins_tse_regs_h_
#define _ins_tse_regs_h_

#include "altera_msgdma.h"
#include "altera_msgdma_descriptor_regs.h"
#include "altera_msgdma_prefetcher_regs.h"
#include "altera_msgdma_csr_regs.h"
#include "altera_avalon_tse.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef ALT_INICHE
#include "ipport.h"
#include "tcpport.h"
#endif

/* Base-Structure for all library functions */
typedef struct{
    /* index number */
    int     index;
    
    tse_mac_trans_info mi; /* MAC base driver data. */

    /* driver specific */
    char     mac_addr[8];   /* use 8 to word align */
    NET      netp;          /* pointer to Interniche NET struct */
    int      txShift16OK;   /* TX supports Shift16 */
    int      rxShift16OK;   /* RX supports Shift16 */
/* Temporary variable to "protect" our transmit function - should be protected otherwise though */
    int     sem;    
/* Variable to store Channel number for Share Fifo System */
    int     channel;
    
    // Location for the MSGDMA Descriptors
    alt_msgdma_prefetcher_standard_descriptor *txdesc_list;        
    alt_msgdma_prefetcher_standard_descriptor *txdesc;   
    //The two lists are used to prepare one list while the other is executing    
    alt_msgdma_prefetcher_standard_descriptor *rxdesc_list[2];    
    alt_msgdma_prefetcher_standard_descriptor *rxdesc[2];
    int     rx_chain;
    int     rx_descriptor_index;
    int     tx_descriptor_index;    
    
    // Pre-allocation of packet buffers for each RX descriptor
    PACKET pkt_array_rx[2][ALTERA_TSE_MSGDMA_RX_DESC_CHAIN_SIZE];
   
    // Hardware location
    void *tse;
    
} ins_tse_info;

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _ins_tse_regs_h_*/
