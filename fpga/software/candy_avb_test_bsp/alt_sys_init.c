/*
 * alt_sys_init.c - HAL initialization source
 *
 * Machine generated for CPU 'nios2_0' in SOPC Builder design 'candy_avb_test_qsys'
 * SOPC Builder design path: ../../candy_avb_test_qsys.sopcinfo
 *
 * Generated: Thu Jan 17 08:47:53 JST 2019
 */

/*
 * DO NOT MODIFY THIS FILE
 *
 * Changing this file will have subtle consequences
 * which will almost certainly lead to a nonfunctioning
 * system. If you do modify this file, be aware that your
 * changes will be overwritten and lost when this file
 * is generated again.
 *
 * DO NOT MODIFY THIS FILE
 */

/*
 * License Agreement
 *
 * Copyright (c) 2008
 * Altera Corporation, San Jose, California, USA.
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * This agreement shall be governed in all respects by the laws of the State
 * of California and by the laws of the United States of America.
 */

#include "system.h"
#include "sys/alt_irq.h"
#include "sys/alt_sys_init.h"

#include <stddef.h>

/*
 * Device headers
 */

#include "altera_nios2_gen2_irq.h"
#include "altera_avalon_jtag_uart.h"
#include "altera_avalon_sysid_qsys.h"
#include "altera_avalon_timer.h"
#include "altera_avalon_uart.h"
#include "altera_eth_tse.h"
#include "altera_msgdma.h"
#include "altera_onchip_flash.h"

/*
 * Allocate the device storage
 */

ALTERA_NIOS2_GEN2_IRQ_INSTANCE ( NIOS2_0, nios2_0);
ALTERA_AVALON_JTAG_UART_INSTANCE ( JTAGUART_0, jtaguart_0);
ALTERA_AVALON_SYSID_QSYS_INSTANCE ( SYSID_QSYS_0, sysid_qsys_0);
ALTERA_AVALON_TIMER_INSTANCE ( TIMER_0, timer_0);
ALTERA_AVALON_TIMER_INSTANCE ( TIMER_1, timer_1);
ALTERA_AVALON_TIMER_INSTANCE ( TIMER_2, timer_2);
ALTERA_AVALON_UART_INSTANCE ( UART_0, uart_0);
ALTERA_ETH_TSE_INSTANCE ( TSE_0_TSE, tse_0_tse);
ALTERA_MSGDMA_CSR_PREFETCHER_CSR_INSTANCE ( TSE_0_DMA_RX, TSE_0_DMA_RX_CSR, TSE_0_DMA_RX_PREFETCHER_CSR, tse_0_dma_rx);
ALTERA_MSGDMA_CSR_PREFETCHER_CSR_INSTANCE ( TSE_0_DMA_TX, TSE_0_DMA_TX_CSR, TSE_0_DMA_TX_PREFETCHER_CSR, tse_0_dma_tx);
ALTERA_ONCHIP_FLASH_DATA_CSR_INSTANCE ( ONCHIP_FLASH_0, ONCHIP_FLASH_0_DATA, ONCHIP_FLASH_0_CSR, onchip_flash_0);

/*
 * Initialize the interrupt controller devices
 * and then enable interrupts in the CPU.
 * Called before alt_sys_init().
 * The "base" parameter is ignored and only
 * present for backwards-compatibility.
 */

void alt_irq_init ( const void* base )
{
    ALTERA_NIOS2_GEN2_IRQ_INIT ( NIOS2_0, nios2_0);
    alt_irq_cpu_enable_interrupts();
}

/*
 * Initialize the non-interrupt controller devices.
 * Called after alt_irq_init().
 */

void alt_sys_init( void )
{
    ALTERA_AVALON_TIMER_INIT ( TIMER_0, timer_0);
    ALTERA_AVALON_TIMER_INIT ( TIMER_1, timer_1);
    ALTERA_AVALON_TIMER_INIT ( TIMER_2, timer_2);
    ALTERA_AVALON_JTAG_UART_INIT ( JTAGUART_0, jtaguart_0);
    ALTERA_AVALON_SYSID_QSYS_INIT ( SYSID_QSYS_0, sysid_qsys_0);
    ALTERA_AVALON_UART_INIT ( UART_0, uart_0);
    ALTERA_ETH_TSE_INIT ( TSE_0_TSE, tse_0_tse);
    ALTERA_MSGDMA_INIT ( TSE_0_DMA_RX, tse_0_dma_rx);
    ALTERA_MSGDMA_INIT ( TSE_0_DMA_TX, tse_0_dma_tx);
    ALTERA_ONCHIP_FLASH_INIT ( ONCHIP_FLASH_0, onchip_flash_0);
}
