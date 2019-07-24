/*
 * system.h - SOPC Builder system and BSP software package information
 *
 * Machine generated for CPU 'nios2_0' in SOPC Builder design 'candy_avb_test_qsys'
 * SOPC Builder design path: ../../candy_avb_test_qsys.sopcinfo
 *
 * Generated: Tue Jul 23 16:48:51 JST 2019
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

#ifndef __SYSTEM_H_
#define __SYSTEM_H_

/* Include definitions from linker script generator */
#include "linker.h"


/*
 * CPU configuration
 *
 */

#define ALT_CPU_ARCHITECTURE "altera_nios2_gen2"
#define ALT_CPU_BIG_ENDIAN 0
#define ALT_CPU_BREAK_ADDR 0x00a00820
#define ALT_CPU_CPU_ARCH_NIOS2_R1
#define ALT_CPU_CPU_FREQ 100000000u
#define ALT_CPU_CPU_ID_SIZE 1
#define ALT_CPU_CPU_ID_VALUE 0x00000000
#define ALT_CPU_CPU_IMPLEMENTATION "fast"
#define ALT_CPU_DATA_ADDR_WIDTH 0x1f
#define ALT_CPU_DCACHE_BYPASS_MASK 0x80000000
#define ALT_CPU_DCACHE_LINE_SIZE 32
#define ALT_CPU_DCACHE_LINE_SIZE_LOG2 5
#define ALT_CPU_DCACHE_SIZE 2048
#define ALT_CPU_EXCEPTION_ADDR 0x00000020
#define ALT_CPU_FLASH_ACCELERATOR_LINES 0
#define ALT_CPU_FLASH_ACCELERATOR_LINE_SIZE 0
#define ALT_CPU_FLUSHDA_SUPPORTED
#define ALT_CPU_FREQ 100000000
#define ALT_CPU_HARDWARE_DIVIDE_PRESENT 0
#define ALT_CPU_HARDWARE_MULTIPLY_PRESENT 1
#define ALT_CPU_HARDWARE_MULX_PRESENT 0
#define ALT_CPU_HAS_DEBUG_CORE 1
#define ALT_CPU_HAS_DEBUG_STUB
#define ALT_CPU_HAS_EXTRA_EXCEPTION_INFO
#define ALT_CPU_HAS_ILLEGAL_INSTRUCTION_EXCEPTION
#define ALT_CPU_HAS_JMPI_INSTRUCTION
#define ALT_CPU_ICACHE_LINE_SIZE 32
#define ALT_CPU_ICACHE_LINE_SIZE_LOG2 5
#define ALT_CPU_ICACHE_SIZE 4096
#define ALT_CPU_INITDA_SUPPORTED
#define ALT_CPU_INST_ADDR_WIDTH 0x18
#define ALT_CPU_NAME "nios2_0"
#define ALT_CPU_NUM_OF_SHADOW_REG_SETS 0
#define ALT_CPU_OCI_VERSION 1
#define ALT_CPU_RESET_ADDR 0x00900000


/*
 * CPU configuration (with legacy prefix - don't use these anymore)
 *
 */

#define NIOS2_BIG_ENDIAN 0
#define NIOS2_BREAK_ADDR 0x00a00820
#define NIOS2_CPU_ARCH_NIOS2_R1
#define NIOS2_CPU_FREQ 100000000u
#define NIOS2_CPU_ID_SIZE 1
#define NIOS2_CPU_ID_VALUE 0x00000000
#define NIOS2_CPU_IMPLEMENTATION "fast"
#define NIOS2_DATA_ADDR_WIDTH 0x1f
#define NIOS2_DCACHE_BYPASS_MASK 0x80000000
#define NIOS2_DCACHE_LINE_SIZE 32
#define NIOS2_DCACHE_LINE_SIZE_LOG2 5
#define NIOS2_DCACHE_SIZE 2048
#define NIOS2_EXCEPTION_ADDR 0x00000020
#define NIOS2_FLASH_ACCELERATOR_LINES 0
#define NIOS2_FLASH_ACCELERATOR_LINE_SIZE 0
#define NIOS2_FLUSHDA_SUPPORTED
#define NIOS2_HARDWARE_DIVIDE_PRESENT 0
#define NIOS2_HARDWARE_MULTIPLY_PRESENT 1
#define NIOS2_HARDWARE_MULX_PRESENT 0
#define NIOS2_HAS_DEBUG_CORE 1
#define NIOS2_HAS_DEBUG_STUB
#define NIOS2_HAS_EXTRA_EXCEPTION_INFO
#define NIOS2_HAS_ILLEGAL_INSTRUCTION_EXCEPTION
#define NIOS2_HAS_JMPI_INSTRUCTION
#define NIOS2_ICACHE_LINE_SIZE 32
#define NIOS2_ICACHE_LINE_SIZE_LOG2 5
#define NIOS2_ICACHE_SIZE 4096
#define NIOS2_INITDA_SUPPORTED
#define NIOS2_INST_ADDR_WIDTH 0x18
#define NIOS2_NUM_OF_SHADOW_REG_SETS 0
#define NIOS2_OCI_VERSION 1
#define NIOS2_RESET_ADDR 0x00900000


/*
 * Define for each module class mastered by the CPU
 *
 */

#define __ALTERA_AVALON_JTAG_UART
#define __ALTERA_AVALON_NEW_SDRAM_CONTROLLER
#define __ALTERA_AVALON_ONCHIP_MEMORY2
#define __ALTERA_AVALON_PIO
#define __ALTERA_AVALON_SYSID_QSYS
#define __ALTERA_AVALON_TIMER
#define __ALTERA_AVALON_UART
#define __ALTERA_ETH_TSE
#define __ALTERA_MODULAR_ADC
#define __ALTERA_MSGDMA
#define __ALTERA_NIOS2_GEN2
#define __ALTERA_ONCHIP_FLASH
#define __ALTPLL
#define __AVALON_WB


/*
 * System configuration
 *
 */

#define ALT_DEVICE_FAMILY "MAX 10"
#define ALT_ENHANCED_INTERRUPT_API_PRESENT
#define ALT_IRQ_BASE NULL
#define ALT_LOG_PORT "/dev/null"
#define ALT_LOG_PORT_BASE 0x0
#define ALT_LOG_PORT_DEV null
#define ALT_LOG_PORT_TYPE ""
#define ALT_NUM_EXTERNAL_INTERRUPT_CONTROLLERS 0
#define ALT_NUM_INTERNAL_INTERRUPT_CONTROLLERS 1
#define ALT_NUM_INTERRUPT_CONTROLLERS 1
#define ALT_STDERR "/dev/jtaguart_0"
#define ALT_STDERR_BASE 0xa01718
#define ALT_STDERR_DEV jtaguart_0
#define ALT_STDERR_IS_JTAG_UART
#define ALT_STDERR_PRESENT
#define ALT_STDERR_TYPE "altera_avalon_jtag_uart"
#define ALT_STDIN "/dev/jtaguart_0"
#define ALT_STDIN_BASE 0xa01718
#define ALT_STDIN_DEV jtaguart_0
#define ALT_STDIN_IS_JTAG_UART
#define ALT_STDIN_PRESENT
#define ALT_STDIN_TYPE "altera_avalon_jtag_uart"
#define ALT_STDOUT "/dev/jtaguart_0"
#define ALT_STDOUT_BASE 0xa01718
#define ALT_STDOUT_DEV jtaguart_0
#define ALT_STDOUT_IS_JTAG_UART
#define ALT_STDOUT_PRESENT
#define ALT_STDOUT_TYPE "altera_avalon_jtag_uart"
#define ALT_SYSTEM_NAME "candy_avb_test_qsys"


/*
 * altera_iniche configuration
 *
 */

#define DHCP_CLIENT
#define INCLUDE_TCP
#define INICHE_DEFAULT_IF "NOT_USED"
#define IP_FRAGMENTS


/*
 * altpll_0 configuration
 *
 */

#define ALTPLL_0_BASE 0xa016f0
#define ALTPLL_0_IRQ -1
#define ALTPLL_0_IRQ_INTERRUPT_CONTROLLER_ID -1
#define ALTPLL_0_NAME "/dev/altpll_0"
#define ALTPLL_0_SPAN 16
#define ALTPLL_0_TYPE "altpll"
#define ALT_MODULE_CLASS_altpll_0 altpll


/*
 * avalon_wb configuration
 *
 */

#define ALT_MODULE_CLASS_avalon_wb avalon_wb
#define AVALON_WB_BASE 0x40000000
#define AVALON_WB_IRQ -1
#define AVALON_WB_IRQ_INTERRUPT_CONTROLLER_ID -1
#define AVALON_WB_NAME "/dev/avalon_wb"
#define AVALON_WB_SPAN 1073741824
#define AVALON_WB_TYPE "avalon_wb"


/*
 * descriptor_memory configuration
 *
 */

#define ALT_MODULE_CLASS_descriptor_memory altera_avalon_onchip_memory2
#define DESCRIPTOR_MEMORY_ALLOW_IN_SYSTEM_MEMORY_CONTENT_EDITOR 0
#define DESCRIPTOR_MEMORY_ALLOW_MRAM_SIM_CONTENTS_ONLY_FILE 0
#define DESCRIPTOR_MEMORY_BASE 0xa02000
#define DESCRIPTOR_MEMORY_CONTENTS_INFO ""
#define DESCRIPTOR_MEMORY_DUAL_PORT 0
#define DESCRIPTOR_MEMORY_GUI_RAM_BLOCK_TYPE "AUTO"
#define DESCRIPTOR_MEMORY_INIT_CONTENTS_FILE "candy_avb_test_qsys_descriptor_memory"
#define DESCRIPTOR_MEMORY_INIT_MEM_CONTENT 0
#define DESCRIPTOR_MEMORY_INSTANCE_ID "NONE"
#define DESCRIPTOR_MEMORY_IRQ -1
#define DESCRIPTOR_MEMORY_IRQ_INTERRUPT_CONTROLLER_ID -1
#define DESCRIPTOR_MEMORY_NAME "/dev/descriptor_memory"
#define DESCRIPTOR_MEMORY_NON_DEFAULT_INIT_FILE_ENABLED 0
#define DESCRIPTOR_MEMORY_RAM_BLOCK_TYPE "AUTO"
#define DESCRIPTOR_MEMORY_READ_DURING_WRITE_MODE "DONT_CARE"
#define DESCRIPTOR_MEMORY_SINGLE_CLOCK_OP 0
#define DESCRIPTOR_MEMORY_SIZE_MULTIPLE 1
#define DESCRIPTOR_MEMORY_SIZE_VALUE 8192
#define DESCRIPTOR_MEMORY_SPAN 8192
#define DESCRIPTOR_MEMORY_TYPE "altera_avalon_onchip_memory2"
#define DESCRIPTOR_MEMORY_WRITABLE 1


/*
 * eth_tse_0 configuration
 *
 */

#define ALT_MODULE_CLASS_eth_tse_0 altera_eth_tse
#define ETH_TSE_0_BASE 0xa01000
#define ETH_TSE_0_ENABLE_MACLITE 0
#define ETH_TSE_0_FIFO_WIDTH 32
#define ETH_TSE_0_IRQ -1
#define ETH_TSE_0_IRQ_INTERRUPT_CONTROLLER_ID -1
#define ETH_TSE_0_IS_MULTICHANNEL_MAC 0
#define ETH_TSE_0_MACLITE_GIGE 0
#define ETH_TSE_0_MDIO_SHARED 0
#define ETH_TSE_0_NAME "/dev/eth_tse_0"
#define ETH_TSE_0_NUMBER_OF_CHANNEL 1
#define ETH_TSE_0_NUMBER_OF_MAC_MDIO_SHARED 1
#define ETH_TSE_0_PCS 0
#define ETH_TSE_0_PCS_ID 0
#define ETH_TSE_0_PCS_SGMII 0
#define ETH_TSE_0_RECEIVE_FIFO_DEPTH 256
#define ETH_TSE_0_REGISTER_SHARED 0
#define ETH_TSE_0_RGMII 0
#define ETH_TSE_0_SPAN 1024
#define ETH_TSE_0_TRANSMIT_FIFO_DEPTH 256
#define ETH_TSE_0_TYPE "altera_eth_tse"
#define ETH_TSE_0_USE_MDIO 1


/*
 * hal configuration
 *
 */

#define ALT_INCLUDE_INSTRUCTION_RELATED_EXCEPTION_API
#define ALT_MAX_FD 32
#define ALT_SYS_CLK SYS_CLK_TIMER
#define ALT_TIMESTAMP_CLK none


/*
 * jtaguart_0 configuration
 *
 */

#define ALT_MODULE_CLASS_jtaguart_0 altera_avalon_jtag_uart
#define JTAGUART_0_BASE 0xa01718
#define JTAGUART_0_IRQ 1
#define JTAGUART_0_IRQ_INTERRUPT_CONTROLLER_ID 0
#define JTAGUART_0_NAME "/dev/jtaguart_0"
#define JTAGUART_0_READ_DEPTH 64
#define JTAGUART_0_READ_THRESHOLD 8
#define JTAGUART_0_SPAN 8
#define JTAGUART_0_TYPE "altera_avalon_jtag_uart"
#define JTAGUART_0_WRITE_DEPTH 64
#define JTAGUART_0_WRITE_THRESHOLD 8


/*
 * modular_adc_0_sample_store_csr configuration
 *
 */

#define ALT_MODULE_CLASS_modular_adc_0_sample_store_csr altera_modular_adc
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_BASE 0xa01400
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_CORE_VARIANT 0
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_CSD_LENGTH 2
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_CSD_SLOT_0 "CH3"
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_CSD_SLOT_1 "CH6"
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_CSD_SLOT_10 "CH0"
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_CSD_SLOT_11 "CH0"
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_CSD_SLOT_12 "CH0"
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_CSD_SLOT_13 "CH0"
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_CSD_SLOT_14 "CH0"
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_CSD_SLOT_15 "CH0"
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_CSD_SLOT_16 "CH0"
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_CSD_SLOT_17 "CH0"
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_CSD_SLOT_18 "CH0"
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_CSD_SLOT_19 "CH0"
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_CSD_SLOT_2 "CH0"
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_CSD_SLOT_20 "CH0"
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_CSD_SLOT_21 "CH0"
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_CSD_SLOT_22 "CH0"
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_CSD_SLOT_23 "CH0"
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_CSD_SLOT_24 "CH0"
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_CSD_SLOT_25 "CH0"
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_CSD_SLOT_26 "CH0"
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_CSD_SLOT_27 "CH0"
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_CSD_SLOT_28 "CH0"
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_CSD_SLOT_29 "CH0"
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_CSD_SLOT_3 "CH0"
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_CSD_SLOT_30 "CH0"
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_CSD_SLOT_31 "CH0"
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_CSD_SLOT_32 "CH0"
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_CSD_SLOT_33 "CH0"
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_CSD_SLOT_34 "CH0"
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_CSD_SLOT_35 "CH0"
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_CSD_SLOT_36 "CH0"
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_CSD_SLOT_37 "CH0"
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_CSD_SLOT_38 "CH0"
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_CSD_SLOT_39 "CH0"
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_CSD_SLOT_4 "CH0"
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_CSD_SLOT_40 "CH0"
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_CSD_SLOT_41 "CH0"
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_CSD_SLOT_42 "CH0"
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_CSD_SLOT_43 "CH0"
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_CSD_SLOT_44 "CH0"
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_CSD_SLOT_45 "CH0"
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_CSD_SLOT_46 "CH0"
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_CSD_SLOT_47 "CH0"
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_CSD_SLOT_48 "CH0"
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_CSD_SLOT_49 "CH0"
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_CSD_SLOT_5 "CH0"
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_CSD_SLOT_50 "CH0"
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_CSD_SLOT_51 "CH0"
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_CSD_SLOT_52 "CH0"
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_CSD_SLOT_53 "CH0"
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_CSD_SLOT_54 "CH0"
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_CSD_SLOT_55 "CH0"
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_CSD_SLOT_56 "CH0"
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_CSD_SLOT_57 "CH0"
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_CSD_SLOT_58 "CH0"
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_CSD_SLOT_59 "CH0"
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_CSD_SLOT_6 "CH0"
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_CSD_SLOT_60 "CH0"
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_CSD_SLOT_61 "CH0"
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_CSD_SLOT_62 "CH0"
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_CSD_SLOT_63 "CH0"
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_CSD_SLOT_7 "CH0"
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_CSD_SLOT_8 "CH0"
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_CSD_SLOT_9 "CH0"
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_DUAL_ADC_MODE 0
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_IRQ 5
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_IRQ_INTERRUPT_CONTROLLER_ID 0
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_NAME "/dev/modular_adc_0_sample_store_csr"
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_PRESCALER_CH16 0
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_PRESCALER_CH8 0
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_REFSEL "Internal VREF"
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_SPAN 512
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_TYPE "altera_modular_adc"
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_USE_CH0 1
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_USE_CH1 1
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_USE_CH10 0
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_USE_CH11 0
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_USE_CH12 0
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_USE_CH13 0
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_USE_CH14 0
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_USE_CH15 0
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_USE_CH16 0
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_USE_CH2 1
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_USE_CH3 1
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_USE_CH4 1
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_USE_CH5 1
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_USE_CH6 1
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_USE_CH7 1
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_USE_CH8 1
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_USE_CH9 0
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_USE_TSD 0
#define MODULAR_ADC_0_SAMPLE_STORE_CSR_VREF 3.3


/*
 * modular_adc_0_sequencer_csr configuration
 *
 */

#define ALT_MODULE_CLASS_modular_adc_0_sequencer_csr altera_modular_adc
#define MODULAR_ADC_0_SEQUENCER_CSR_BASE 0xa01700
#define MODULAR_ADC_0_SEQUENCER_CSR_CORE_VARIANT 0
#define MODULAR_ADC_0_SEQUENCER_CSR_CSD_LENGTH 2
#define MODULAR_ADC_0_SEQUENCER_CSR_CSD_SLOT_0 "CH3"
#define MODULAR_ADC_0_SEQUENCER_CSR_CSD_SLOT_1 "CH6"
#define MODULAR_ADC_0_SEQUENCER_CSR_CSD_SLOT_10 "CH0"
#define MODULAR_ADC_0_SEQUENCER_CSR_CSD_SLOT_11 "CH0"
#define MODULAR_ADC_0_SEQUENCER_CSR_CSD_SLOT_12 "CH0"
#define MODULAR_ADC_0_SEQUENCER_CSR_CSD_SLOT_13 "CH0"
#define MODULAR_ADC_0_SEQUENCER_CSR_CSD_SLOT_14 "CH0"
#define MODULAR_ADC_0_SEQUENCER_CSR_CSD_SLOT_15 "CH0"
#define MODULAR_ADC_0_SEQUENCER_CSR_CSD_SLOT_16 "CH0"
#define MODULAR_ADC_0_SEQUENCER_CSR_CSD_SLOT_17 "CH0"
#define MODULAR_ADC_0_SEQUENCER_CSR_CSD_SLOT_18 "CH0"
#define MODULAR_ADC_0_SEQUENCER_CSR_CSD_SLOT_19 "CH0"
#define MODULAR_ADC_0_SEQUENCER_CSR_CSD_SLOT_2 "CH0"
#define MODULAR_ADC_0_SEQUENCER_CSR_CSD_SLOT_20 "CH0"
#define MODULAR_ADC_0_SEQUENCER_CSR_CSD_SLOT_21 "CH0"
#define MODULAR_ADC_0_SEQUENCER_CSR_CSD_SLOT_22 "CH0"
#define MODULAR_ADC_0_SEQUENCER_CSR_CSD_SLOT_23 "CH0"
#define MODULAR_ADC_0_SEQUENCER_CSR_CSD_SLOT_24 "CH0"
#define MODULAR_ADC_0_SEQUENCER_CSR_CSD_SLOT_25 "CH0"
#define MODULAR_ADC_0_SEQUENCER_CSR_CSD_SLOT_26 "CH0"
#define MODULAR_ADC_0_SEQUENCER_CSR_CSD_SLOT_27 "CH0"
#define MODULAR_ADC_0_SEQUENCER_CSR_CSD_SLOT_28 "CH0"
#define MODULAR_ADC_0_SEQUENCER_CSR_CSD_SLOT_29 "CH0"
#define MODULAR_ADC_0_SEQUENCER_CSR_CSD_SLOT_3 "CH0"
#define MODULAR_ADC_0_SEQUENCER_CSR_CSD_SLOT_30 "CH0"
#define MODULAR_ADC_0_SEQUENCER_CSR_CSD_SLOT_31 "CH0"
#define MODULAR_ADC_0_SEQUENCER_CSR_CSD_SLOT_32 "CH0"
#define MODULAR_ADC_0_SEQUENCER_CSR_CSD_SLOT_33 "CH0"
#define MODULAR_ADC_0_SEQUENCER_CSR_CSD_SLOT_34 "CH0"
#define MODULAR_ADC_0_SEQUENCER_CSR_CSD_SLOT_35 "CH0"
#define MODULAR_ADC_0_SEQUENCER_CSR_CSD_SLOT_36 "CH0"
#define MODULAR_ADC_0_SEQUENCER_CSR_CSD_SLOT_37 "CH0"
#define MODULAR_ADC_0_SEQUENCER_CSR_CSD_SLOT_38 "CH0"
#define MODULAR_ADC_0_SEQUENCER_CSR_CSD_SLOT_39 "CH0"
#define MODULAR_ADC_0_SEQUENCER_CSR_CSD_SLOT_4 "CH0"
#define MODULAR_ADC_0_SEQUENCER_CSR_CSD_SLOT_40 "CH0"
#define MODULAR_ADC_0_SEQUENCER_CSR_CSD_SLOT_41 "CH0"
#define MODULAR_ADC_0_SEQUENCER_CSR_CSD_SLOT_42 "CH0"
#define MODULAR_ADC_0_SEQUENCER_CSR_CSD_SLOT_43 "CH0"
#define MODULAR_ADC_0_SEQUENCER_CSR_CSD_SLOT_44 "CH0"
#define MODULAR_ADC_0_SEQUENCER_CSR_CSD_SLOT_45 "CH0"
#define MODULAR_ADC_0_SEQUENCER_CSR_CSD_SLOT_46 "CH0"
#define MODULAR_ADC_0_SEQUENCER_CSR_CSD_SLOT_47 "CH0"
#define MODULAR_ADC_0_SEQUENCER_CSR_CSD_SLOT_48 "CH0"
#define MODULAR_ADC_0_SEQUENCER_CSR_CSD_SLOT_49 "CH0"
#define MODULAR_ADC_0_SEQUENCER_CSR_CSD_SLOT_5 "CH0"
#define MODULAR_ADC_0_SEQUENCER_CSR_CSD_SLOT_50 "CH0"
#define MODULAR_ADC_0_SEQUENCER_CSR_CSD_SLOT_51 "CH0"
#define MODULAR_ADC_0_SEQUENCER_CSR_CSD_SLOT_52 "CH0"
#define MODULAR_ADC_0_SEQUENCER_CSR_CSD_SLOT_53 "CH0"
#define MODULAR_ADC_0_SEQUENCER_CSR_CSD_SLOT_54 "CH0"
#define MODULAR_ADC_0_SEQUENCER_CSR_CSD_SLOT_55 "CH0"
#define MODULAR_ADC_0_SEQUENCER_CSR_CSD_SLOT_56 "CH0"
#define MODULAR_ADC_0_SEQUENCER_CSR_CSD_SLOT_57 "CH0"
#define MODULAR_ADC_0_SEQUENCER_CSR_CSD_SLOT_58 "CH0"
#define MODULAR_ADC_0_SEQUENCER_CSR_CSD_SLOT_59 "CH0"
#define MODULAR_ADC_0_SEQUENCER_CSR_CSD_SLOT_6 "CH0"
#define MODULAR_ADC_0_SEQUENCER_CSR_CSD_SLOT_60 "CH0"
#define MODULAR_ADC_0_SEQUENCER_CSR_CSD_SLOT_61 "CH0"
#define MODULAR_ADC_0_SEQUENCER_CSR_CSD_SLOT_62 "CH0"
#define MODULAR_ADC_0_SEQUENCER_CSR_CSD_SLOT_63 "CH0"
#define MODULAR_ADC_0_SEQUENCER_CSR_CSD_SLOT_7 "CH0"
#define MODULAR_ADC_0_SEQUENCER_CSR_CSD_SLOT_8 "CH0"
#define MODULAR_ADC_0_SEQUENCER_CSR_CSD_SLOT_9 "CH0"
#define MODULAR_ADC_0_SEQUENCER_CSR_DUAL_ADC_MODE 0
#define MODULAR_ADC_0_SEQUENCER_CSR_IRQ -1
#define MODULAR_ADC_0_SEQUENCER_CSR_IRQ_INTERRUPT_CONTROLLER_ID -1
#define MODULAR_ADC_0_SEQUENCER_CSR_NAME "/dev/modular_adc_0_sequencer_csr"
#define MODULAR_ADC_0_SEQUENCER_CSR_PRESCALER_CH16 0
#define MODULAR_ADC_0_SEQUENCER_CSR_PRESCALER_CH8 0
#define MODULAR_ADC_0_SEQUENCER_CSR_REFSEL "Internal VREF"
#define MODULAR_ADC_0_SEQUENCER_CSR_SPAN 8
#define MODULAR_ADC_0_SEQUENCER_CSR_TYPE "altera_modular_adc"
#define MODULAR_ADC_0_SEQUENCER_CSR_USE_CH0 1
#define MODULAR_ADC_0_SEQUENCER_CSR_USE_CH1 1
#define MODULAR_ADC_0_SEQUENCER_CSR_USE_CH10 0
#define MODULAR_ADC_0_SEQUENCER_CSR_USE_CH11 0
#define MODULAR_ADC_0_SEQUENCER_CSR_USE_CH12 0
#define MODULAR_ADC_0_SEQUENCER_CSR_USE_CH13 0
#define MODULAR_ADC_0_SEQUENCER_CSR_USE_CH14 0
#define MODULAR_ADC_0_SEQUENCER_CSR_USE_CH15 0
#define MODULAR_ADC_0_SEQUENCER_CSR_USE_CH16 0
#define MODULAR_ADC_0_SEQUENCER_CSR_USE_CH2 1
#define MODULAR_ADC_0_SEQUENCER_CSR_USE_CH3 1
#define MODULAR_ADC_0_SEQUENCER_CSR_USE_CH4 1
#define MODULAR_ADC_0_SEQUENCER_CSR_USE_CH5 1
#define MODULAR_ADC_0_SEQUENCER_CSR_USE_CH6 1
#define MODULAR_ADC_0_SEQUENCER_CSR_USE_CH7 1
#define MODULAR_ADC_0_SEQUENCER_CSR_USE_CH8 1
#define MODULAR_ADC_0_SEQUENCER_CSR_USE_CH9 0
#define MODULAR_ADC_0_SEQUENCER_CSR_USE_TSD 0
#define MODULAR_ADC_0_SEQUENCER_CSR_VREF 3.3


/*
 * msgdma_rx_csr configuration
 *
 */

#define ALT_MODULE_CLASS_msgdma_rx_csr altera_msgdma
#define MSGDMA_RX_CSR_BASE 0xa01680
#define MSGDMA_RX_CSR_BURST_ENABLE 0
#define MSGDMA_RX_CSR_BURST_WRAPPING_SUPPORT 0
#define MSGDMA_RX_CSR_CHANNEL_ENABLE 0
#define MSGDMA_RX_CSR_CHANNEL_ENABLE_DERIVED 0
#define MSGDMA_RX_CSR_CHANNEL_WIDTH 8
#define MSGDMA_RX_CSR_DATA_FIFO_DEPTH 256
#define MSGDMA_RX_CSR_DATA_WIDTH 32
#define MSGDMA_RX_CSR_DESCRIPTOR_FIFO_DEPTH 128
#define MSGDMA_RX_CSR_DMA_MODE 2
#define MSGDMA_RX_CSR_ENHANCED_FEATURES 0
#define MSGDMA_RX_CSR_ERROR_ENABLE 1
#define MSGDMA_RX_CSR_ERROR_ENABLE_DERIVED 1
#define MSGDMA_RX_CSR_ERROR_WIDTH 6
#define MSGDMA_RX_CSR_IRQ -1
#define MSGDMA_RX_CSR_IRQ_INTERRUPT_CONTROLLER_ID -1
#define MSGDMA_RX_CSR_MAX_BURST_COUNT 16
#define MSGDMA_RX_CSR_MAX_BYTE 1024
#define MSGDMA_RX_CSR_MAX_STRIDE 1
#define MSGDMA_RX_CSR_NAME "/dev/msgdma_rx_csr"
#define MSGDMA_RX_CSR_PACKET_ENABLE 1
#define MSGDMA_RX_CSR_PACKET_ENABLE_DERIVED 1
#define MSGDMA_RX_CSR_PREFETCHER_ENABLE 1
#define MSGDMA_RX_CSR_PROGRAMMABLE_BURST_ENABLE 0
#define MSGDMA_RX_CSR_RESPONSE_PORT 2
#define MSGDMA_RX_CSR_SPAN 32
#define MSGDMA_RX_CSR_STRIDE_ENABLE 0
#define MSGDMA_RX_CSR_STRIDE_ENABLE_DERIVED 0
#define MSGDMA_RX_CSR_TRANSFER_TYPE "Aligned Accesses"
#define MSGDMA_RX_CSR_TYPE "altera_msgdma"


/*
 * msgdma_rx_prefetcher_csr configuration
 *
 */

#define ALT_MODULE_CLASS_msgdma_rx_prefetcher_csr altera_msgdma
#define MSGDMA_RX_PREFETCHER_CSR_BASE 0xa01660
#define MSGDMA_RX_PREFETCHER_CSR_BURST_ENABLE 0
#define MSGDMA_RX_PREFETCHER_CSR_BURST_WRAPPING_SUPPORT 0
#define MSGDMA_RX_PREFETCHER_CSR_CHANNEL_ENABLE 0
#define MSGDMA_RX_PREFETCHER_CSR_CHANNEL_ENABLE_DERIVED 0
#define MSGDMA_RX_PREFETCHER_CSR_CHANNEL_WIDTH 8
#define MSGDMA_RX_PREFETCHER_CSR_DATA_FIFO_DEPTH 256
#define MSGDMA_RX_PREFETCHER_CSR_DATA_WIDTH 32
#define MSGDMA_RX_PREFETCHER_CSR_DESCRIPTOR_FIFO_DEPTH 128
#define MSGDMA_RX_PREFETCHER_CSR_DMA_MODE 2
#define MSGDMA_RX_PREFETCHER_CSR_ENHANCED_FEATURES 0
#define MSGDMA_RX_PREFETCHER_CSR_ERROR_ENABLE 1
#define MSGDMA_RX_PREFETCHER_CSR_ERROR_ENABLE_DERIVED 1
#define MSGDMA_RX_PREFETCHER_CSR_ERROR_WIDTH 6
#define MSGDMA_RX_PREFETCHER_CSR_IRQ 2
#define MSGDMA_RX_PREFETCHER_CSR_IRQ_INTERRUPT_CONTROLLER_ID 0
#define MSGDMA_RX_PREFETCHER_CSR_MAX_BURST_COUNT 16
#define MSGDMA_RX_PREFETCHER_CSR_MAX_BYTE 1024
#define MSGDMA_RX_PREFETCHER_CSR_MAX_STRIDE 1
#define MSGDMA_RX_PREFETCHER_CSR_NAME "/dev/msgdma_rx_prefetcher_csr"
#define MSGDMA_RX_PREFETCHER_CSR_PACKET_ENABLE 1
#define MSGDMA_RX_PREFETCHER_CSR_PACKET_ENABLE_DERIVED 1
#define MSGDMA_RX_PREFETCHER_CSR_PREFETCHER_ENABLE 1
#define MSGDMA_RX_PREFETCHER_CSR_PROGRAMMABLE_BURST_ENABLE 0
#define MSGDMA_RX_PREFETCHER_CSR_RESPONSE_PORT 2
#define MSGDMA_RX_PREFETCHER_CSR_SPAN 32
#define MSGDMA_RX_PREFETCHER_CSR_STRIDE_ENABLE 0
#define MSGDMA_RX_PREFETCHER_CSR_STRIDE_ENABLE_DERIVED 0
#define MSGDMA_RX_PREFETCHER_CSR_TRANSFER_TYPE "Aligned Accesses"
#define MSGDMA_RX_PREFETCHER_CSR_TYPE "altera_msgdma"


/*
 * msgdma_tx_csr configuration
 *
 */

#define ALT_MODULE_CLASS_msgdma_tx_csr altera_msgdma
#define MSGDMA_TX_CSR_BASE 0xa016a0
#define MSGDMA_TX_CSR_BURST_ENABLE 0
#define MSGDMA_TX_CSR_BURST_WRAPPING_SUPPORT 0
#define MSGDMA_TX_CSR_CHANNEL_ENABLE 0
#define MSGDMA_TX_CSR_CHANNEL_ENABLE_DERIVED 0
#define MSGDMA_TX_CSR_CHANNEL_WIDTH 8
#define MSGDMA_TX_CSR_DATA_FIFO_DEPTH 256
#define MSGDMA_TX_CSR_DATA_WIDTH 32
#define MSGDMA_TX_CSR_DESCRIPTOR_FIFO_DEPTH 128
#define MSGDMA_TX_CSR_DMA_MODE 1
#define MSGDMA_TX_CSR_ENHANCED_FEATURES 0
#define MSGDMA_TX_CSR_ERROR_ENABLE 1
#define MSGDMA_TX_CSR_ERROR_ENABLE_DERIVED 1
#define MSGDMA_TX_CSR_ERROR_WIDTH 1
#define MSGDMA_TX_CSR_IRQ -1
#define MSGDMA_TX_CSR_IRQ_INTERRUPT_CONTROLLER_ID -1
#define MSGDMA_TX_CSR_MAX_BURST_COUNT 16
#define MSGDMA_TX_CSR_MAX_BYTE 1024
#define MSGDMA_TX_CSR_MAX_STRIDE 1
#define MSGDMA_TX_CSR_NAME "/dev/msgdma_tx_csr"
#define MSGDMA_TX_CSR_PACKET_ENABLE 1
#define MSGDMA_TX_CSR_PACKET_ENABLE_DERIVED 1
#define MSGDMA_TX_CSR_PREFETCHER_ENABLE 1
#define MSGDMA_TX_CSR_PROGRAMMABLE_BURST_ENABLE 0
#define MSGDMA_TX_CSR_RESPONSE_PORT 2
#define MSGDMA_TX_CSR_SPAN 32
#define MSGDMA_TX_CSR_STRIDE_ENABLE 0
#define MSGDMA_TX_CSR_STRIDE_ENABLE_DERIVED 0
#define MSGDMA_TX_CSR_TRANSFER_TYPE "Aligned Accesses"
#define MSGDMA_TX_CSR_TYPE "altera_msgdma"


/*
 * msgdma_tx_prefetcher_csr configuration
 *
 */

#define ALT_MODULE_CLASS_msgdma_tx_prefetcher_csr altera_msgdma
#define MSGDMA_TX_PREFETCHER_CSR_BASE 0xa01640
#define MSGDMA_TX_PREFETCHER_CSR_BURST_ENABLE 0
#define MSGDMA_TX_PREFETCHER_CSR_BURST_WRAPPING_SUPPORT 0
#define MSGDMA_TX_PREFETCHER_CSR_CHANNEL_ENABLE 0
#define MSGDMA_TX_PREFETCHER_CSR_CHANNEL_ENABLE_DERIVED 0
#define MSGDMA_TX_PREFETCHER_CSR_CHANNEL_WIDTH 8
#define MSGDMA_TX_PREFETCHER_CSR_DATA_FIFO_DEPTH 256
#define MSGDMA_TX_PREFETCHER_CSR_DATA_WIDTH 32
#define MSGDMA_TX_PREFETCHER_CSR_DESCRIPTOR_FIFO_DEPTH 128
#define MSGDMA_TX_PREFETCHER_CSR_DMA_MODE 1
#define MSGDMA_TX_PREFETCHER_CSR_ENHANCED_FEATURES 0
#define MSGDMA_TX_PREFETCHER_CSR_ERROR_ENABLE 1
#define MSGDMA_TX_PREFETCHER_CSR_ERROR_ENABLE_DERIVED 1
#define MSGDMA_TX_PREFETCHER_CSR_ERROR_WIDTH 1
#define MSGDMA_TX_PREFETCHER_CSR_IRQ 3
#define MSGDMA_TX_PREFETCHER_CSR_IRQ_INTERRUPT_CONTROLLER_ID 0
#define MSGDMA_TX_PREFETCHER_CSR_MAX_BURST_COUNT 16
#define MSGDMA_TX_PREFETCHER_CSR_MAX_BYTE 1024
#define MSGDMA_TX_PREFETCHER_CSR_MAX_STRIDE 1
#define MSGDMA_TX_PREFETCHER_CSR_NAME "/dev/msgdma_tx_prefetcher_csr"
#define MSGDMA_TX_PREFETCHER_CSR_PACKET_ENABLE 1
#define MSGDMA_TX_PREFETCHER_CSR_PACKET_ENABLE_DERIVED 1
#define MSGDMA_TX_PREFETCHER_CSR_PREFETCHER_ENABLE 1
#define MSGDMA_TX_PREFETCHER_CSR_PROGRAMMABLE_BURST_ENABLE 0
#define MSGDMA_TX_PREFETCHER_CSR_RESPONSE_PORT 2
#define MSGDMA_TX_PREFETCHER_CSR_SPAN 32
#define MSGDMA_TX_PREFETCHER_CSR_STRIDE_ENABLE 0
#define MSGDMA_TX_PREFETCHER_CSR_STRIDE_ENABLE_DERIVED 0
#define MSGDMA_TX_PREFETCHER_CSR_TRANSFER_TYPE "Aligned Accesses"
#define MSGDMA_TX_PREFETCHER_CSR_TYPE "altera_msgdma"


/*
 * new_sdram_controller_0 configuration
 *
 */

#define ALT_MODULE_CLASS_new_sdram_controller_0 altera_avalon_new_sdram_controller
#define NEW_SDRAM_CONTROLLER_0_BASE 0x0
#define NEW_SDRAM_CONTROLLER_0_CAS_LATENCY 3
#define NEW_SDRAM_CONTROLLER_0_CONTENTS_INFO
#define NEW_SDRAM_CONTROLLER_0_INIT_NOP_DELAY 0.0
#define NEW_SDRAM_CONTROLLER_0_INIT_REFRESH_COMMANDS 2
#define NEW_SDRAM_CONTROLLER_0_IRQ -1
#define NEW_SDRAM_CONTROLLER_0_IRQ_INTERRUPT_CONTROLLER_ID -1
#define NEW_SDRAM_CONTROLLER_0_IS_INITIALIZED 1
#define NEW_SDRAM_CONTROLLER_0_NAME "/dev/new_sdram_controller_0"
#define NEW_SDRAM_CONTROLLER_0_POWERUP_DELAY 200.0
#define NEW_SDRAM_CONTROLLER_0_REFRESH_PERIOD 15.6
#define NEW_SDRAM_CONTROLLER_0_REGISTER_DATA_IN 1
#define NEW_SDRAM_CONTROLLER_0_SDRAM_ADDR_WIDTH 0x16
#define NEW_SDRAM_CONTROLLER_0_SDRAM_BANK_WIDTH 2
#define NEW_SDRAM_CONTROLLER_0_SDRAM_COL_WIDTH 8
#define NEW_SDRAM_CONTROLLER_0_SDRAM_DATA_WIDTH 16
#define NEW_SDRAM_CONTROLLER_0_SDRAM_NUM_BANKS 4
#define NEW_SDRAM_CONTROLLER_0_SDRAM_NUM_CHIPSELECTS 1
#define NEW_SDRAM_CONTROLLER_0_SDRAM_ROW_WIDTH 12
#define NEW_SDRAM_CONTROLLER_0_SHARED_DATA 0
#define NEW_SDRAM_CONTROLLER_0_SIM_MODEL_BASE 0
#define NEW_SDRAM_CONTROLLER_0_SPAN 8388608
#define NEW_SDRAM_CONTROLLER_0_STARVATION_INDICATOR 0
#define NEW_SDRAM_CONTROLLER_0_TRISTATE_BRIDGE_SLAVE ""
#define NEW_SDRAM_CONTROLLER_0_TYPE "altera_avalon_new_sdram_controller"
#define NEW_SDRAM_CONTROLLER_0_T_AC 5.4
#define NEW_SDRAM_CONTROLLER_0_T_MRD 2
#define NEW_SDRAM_CONTROLLER_0_T_RCD 21.0
#define NEW_SDRAM_CONTROLLER_0_T_RFC 63.0
#define NEW_SDRAM_CONTROLLER_0_T_RP 42.0
#define NEW_SDRAM_CONTROLLER_0_T_WR 2.0


/*
 * onchip_flash_0_csr configuration
 *
 */

#define ALT_MODULE_CLASS_onchip_flash_0_csr altera_onchip_flash
#define ONCHIP_FLASH_0_CSR_BASE 0xa01708
#define ONCHIP_FLASH_0_CSR_BYTES_PER_PAGE 4096
#define ONCHIP_FLASH_0_CSR_IRQ -1
#define ONCHIP_FLASH_0_CSR_IRQ_INTERRUPT_CONTROLLER_ID -1
#define ONCHIP_FLASH_0_CSR_NAME "/dev/onchip_flash_0_csr"
#define ONCHIP_FLASH_0_CSR_READ_ONLY_MODE 0
#define ONCHIP_FLASH_0_CSR_SECTOR1_ENABLED 1
#define ONCHIP_FLASH_0_CSR_SECTOR1_END_ADDR 0x3fff
#define ONCHIP_FLASH_0_CSR_SECTOR1_START_ADDR 0
#define ONCHIP_FLASH_0_CSR_SECTOR2_ENABLED 1
#define ONCHIP_FLASH_0_CSR_SECTOR2_END_ADDR 0x7fff
#define ONCHIP_FLASH_0_CSR_SECTOR2_START_ADDR 0x4000
#define ONCHIP_FLASH_0_CSR_SECTOR3_ENABLED 1
#define ONCHIP_FLASH_0_CSR_SECTOR3_END_ADDR 0x2dfff
#define ONCHIP_FLASH_0_CSR_SECTOR3_START_ADDR 0x8000
#define ONCHIP_FLASH_0_CSR_SECTOR4_ENABLED 1
#define ONCHIP_FLASH_0_CSR_SECTOR4_END_ADDR 0x49fff
#define ONCHIP_FLASH_0_CSR_SECTOR4_START_ADDR 0x2e000
#define ONCHIP_FLASH_0_CSR_SECTOR5_ENABLED 1
#define ONCHIP_FLASH_0_CSR_SECTOR5_END_ADDR 0x8bfff
#define ONCHIP_FLASH_0_CSR_SECTOR5_START_ADDR 0x4a000
#define ONCHIP_FLASH_0_CSR_SPAN 8
#define ONCHIP_FLASH_0_CSR_TYPE "altera_onchip_flash"


/*
 * onchip_flash_0_data configuration
 *
 */

#define ALT_MODULE_CLASS_onchip_flash_0_data altera_onchip_flash
#define ONCHIP_FLASH_0_DATA_BASE 0x900000
#define ONCHIP_FLASH_0_DATA_BYTES_PER_PAGE 4096
#define ONCHIP_FLASH_0_DATA_IRQ -1
#define ONCHIP_FLASH_0_DATA_IRQ_INTERRUPT_CONTROLLER_ID -1
#define ONCHIP_FLASH_0_DATA_NAME "/dev/onchip_flash_0_data"
#define ONCHIP_FLASH_0_DATA_READ_ONLY_MODE 0
#define ONCHIP_FLASH_0_DATA_SECTOR1_ENABLED 1
#define ONCHIP_FLASH_0_DATA_SECTOR1_END_ADDR 0x3fff
#define ONCHIP_FLASH_0_DATA_SECTOR1_START_ADDR 0
#define ONCHIP_FLASH_0_DATA_SECTOR2_ENABLED 1
#define ONCHIP_FLASH_0_DATA_SECTOR2_END_ADDR 0x7fff
#define ONCHIP_FLASH_0_DATA_SECTOR2_START_ADDR 0x4000
#define ONCHIP_FLASH_0_DATA_SECTOR3_ENABLED 1
#define ONCHIP_FLASH_0_DATA_SECTOR3_END_ADDR 0x2dfff
#define ONCHIP_FLASH_0_DATA_SECTOR3_START_ADDR 0x8000
#define ONCHIP_FLASH_0_DATA_SECTOR4_ENABLED 1
#define ONCHIP_FLASH_0_DATA_SECTOR4_END_ADDR 0x49fff
#define ONCHIP_FLASH_0_DATA_SECTOR4_START_ADDR 0x2e000
#define ONCHIP_FLASH_0_DATA_SECTOR5_ENABLED 1
#define ONCHIP_FLASH_0_DATA_SECTOR5_END_ADDR 0x8bfff
#define ONCHIP_FLASH_0_DATA_SECTOR5_START_ADDR 0x4a000
#define ONCHIP_FLASH_0_DATA_SPAN 573440
#define ONCHIP_FLASH_0_DATA_TYPE "altera_onchip_flash"


/*
 * pio_0 configuration
 *
 */

#define ALT_MODULE_CLASS_pio_0 altera_avalon_pio
#define PIO_0_BASE 0xa016e0
#define PIO_0_BIT_CLEARING_EDGE_REGISTER 0
#define PIO_0_BIT_MODIFYING_OUTPUT_REGISTER 0
#define PIO_0_CAPTURE 0
#define PIO_0_DATA_WIDTH 2
#define PIO_0_DO_TEST_BENCH_WIRING 0
#define PIO_0_DRIVEN_SIM_VALUE 0
#define PIO_0_EDGE_TYPE "NONE"
#define PIO_0_FREQ 100000000
#define PIO_0_HAS_IN 0
#define PIO_0_HAS_OUT 1
#define PIO_0_HAS_TRI 0
#define PIO_0_IRQ -1
#define PIO_0_IRQ_INTERRUPT_CONTROLLER_ID -1
#define PIO_0_IRQ_TYPE "NONE"
#define PIO_0_NAME "/dev/pio_0"
#define PIO_0_RESET_VALUE 0
#define PIO_0_SPAN 16
#define PIO_0_TYPE "altera_avalon_pio"


/*
 * pio_1 configuration
 *
 */

#define ALT_MODULE_CLASS_pio_1 altera_avalon_pio
#define PIO_1_BASE 0xa016c0
#define PIO_1_BIT_CLEARING_EDGE_REGISTER 0
#define PIO_1_BIT_MODIFYING_OUTPUT_REGISTER 0
#define PIO_1_CAPTURE 0
#define PIO_1_DATA_WIDTH 1
#define PIO_1_DO_TEST_BENCH_WIRING 0
#define PIO_1_DRIVEN_SIM_VALUE 0
#define PIO_1_EDGE_TYPE "NONE"
#define PIO_1_FREQ 100000000
#define PIO_1_HAS_IN 0
#define PIO_1_HAS_OUT 0
#define PIO_1_HAS_TRI 1
#define PIO_1_IRQ -1
#define PIO_1_IRQ_INTERRUPT_CONTROLLER_ID -1
#define PIO_1_IRQ_TYPE "NONE"
#define PIO_1_NAME "/dev/pio_1"
#define PIO_1_RESET_VALUE 1
#define PIO_1_SPAN 16
#define PIO_1_TYPE "altera_avalon_pio"


/*
 * pio_4 configuration
 *
 */

#define ALT_MODULE_CLASS_pio_4 altera_avalon_pio
#define PIO_4_BASE 0xa016d0
#define PIO_4_BIT_CLEARING_EDGE_REGISTER 0
#define PIO_4_BIT_MODIFYING_OUTPUT_REGISTER 0
#define PIO_4_CAPTURE 0
#define PIO_4_DATA_WIDTH 1
#define PIO_4_DO_TEST_BENCH_WIRING 0
#define PIO_4_DRIVEN_SIM_VALUE 0
#define PIO_4_EDGE_TYPE "NONE"
#define PIO_4_FREQ 100000000
#define PIO_4_HAS_IN 0
#define PIO_4_HAS_OUT 1
#define PIO_4_HAS_TRI 0
#define PIO_4_IRQ -1
#define PIO_4_IRQ_INTERRUPT_CONTROLLER_ID -1
#define PIO_4_IRQ_TYPE "NONE"
#define PIO_4_NAME "/dev/pio_4"
#define PIO_4_RESET_VALUE 1
#define PIO_4_SPAN 16
#define PIO_4_TYPE "altera_avalon_pio"


/*
 * sys_clk_timer configuration
 *
 */

#define ALT_MODULE_CLASS_sys_clk_timer altera_avalon_timer
#define SYS_CLK_TIMER_ALWAYS_RUN 0
#define SYS_CLK_TIMER_BASE 0xa01600
#define SYS_CLK_TIMER_COUNTER_SIZE 32
#define SYS_CLK_TIMER_FIXED_PERIOD 1
#define SYS_CLK_TIMER_FREQ 100000000
#define SYS_CLK_TIMER_IRQ 0
#define SYS_CLK_TIMER_IRQ_INTERRUPT_CONTROLLER_ID 0
#define SYS_CLK_TIMER_LOAD_VALUE 99999
#define SYS_CLK_TIMER_MULT 0.001
#define SYS_CLK_TIMER_NAME "/dev/sys_clk_timer"
#define SYS_CLK_TIMER_PERIOD 1
#define SYS_CLK_TIMER_PERIOD_UNITS "ms"
#define SYS_CLK_TIMER_RESET_OUTPUT 0
#define SYS_CLK_TIMER_SNAPSHOT 1
#define SYS_CLK_TIMER_SPAN 32
#define SYS_CLK_TIMER_TICKS_PER_SEC 1000
#define SYS_CLK_TIMER_TIMEOUT_PULSE_OUTPUT 0
#define SYS_CLK_TIMER_TYPE "altera_avalon_timer"


/*
 * sysid_qsys_0 configuration
 *
 */

#define ALT_MODULE_CLASS_sysid_qsys_0 altera_avalon_sysid_qsys
#define SYSID_QSYS_0_BASE 0xa01710
#define SYSID_QSYS_0_ID 0
#define SYSID_QSYS_0_IRQ -1
#define SYSID_QSYS_0_IRQ_INTERRUPT_CONTROLLER_ID -1
#define SYSID_QSYS_0_NAME "/dev/sysid_qsys_0"
#define SYSID_QSYS_0_SPAN 8
#define SYSID_QSYS_0_TIMESTAMP 1563866610
#define SYSID_QSYS_0_TYPE "altera_avalon_sysid_qsys"


/*
 * uart configuration
 *
 */

#define ALT_MODULE_CLASS_uart altera_avalon_uart
#define UART_BASE 0xa01620
#define UART_BAUD 115200
#define UART_DATA_BITS 8
#define UART_FIXED_BAUD 1
#define UART_FREQ 100000000
#define UART_IRQ 4
#define UART_IRQ_INTERRUPT_CONTROLLER_ID 0
#define UART_NAME "/dev/uart"
#define UART_PARITY 'N'
#define UART_SIM_CHAR_STREAM ""
#define UART_SIM_TRUE_BAUD 0
#define UART_SPAN 32
#define UART_STOP_BITS 1
#define UART_SYNC_REG_DEPTH 2
#define UART_TYPE "altera_avalon_uart"
#define UART_USE_CTS_RTS 1
#define UART_USE_EOP_REGISTER 0


/*
 * ucosii configuration
 *
 */

#define OS_ARG_CHK_EN 1
#define OS_CPU_HOOKS_EN 1
#define OS_DEBUG_EN 1
#define OS_EVENT_NAME_SIZE 32
#define OS_FLAGS_NBITS 16
#define OS_FLAG_ACCEPT_EN 1
#define OS_FLAG_DEL_EN 1
#define OS_FLAG_EN 1
#define OS_FLAG_NAME_SIZE 32
#define OS_FLAG_QUERY_EN 1
#define OS_FLAG_WAIT_CLR_EN 1
#define OS_LOWEST_PRIO 20
#define OS_MAX_EVENTS 60
#define OS_MAX_FLAGS 20
#define OS_MAX_MEM_PART 60
#define OS_MAX_QS 20
#define OS_MAX_TASKS 10
#define OS_MBOX_ACCEPT_EN 1
#define OS_MBOX_DEL_EN 1
#define OS_MBOX_EN 1
#define OS_MBOX_POST_EN 1
#define OS_MBOX_POST_OPT_EN 1
#define OS_MBOX_QUERY_EN 1
#define OS_MEM_EN 1
#define OS_MEM_NAME_SIZE 32
#define OS_MEM_QUERY_EN 1
#define OS_MUTEX_ACCEPT_EN 1
#define OS_MUTEX_DEL_EN 1
#define OS_MUTEX_EN 1
#define OS_MUTEX_QUERY_EN 1
#define OS_Q_ACCEPT_EN 1
#define OS_Q_DEL_EN 1
#define OS_Q_EN 1
#define OS_Q_FLUSH_EN 1
#define OS_Q_POST_EN 1
#define OS_Q_POST_FRONT_EN 1
#define OS_Q_POST_OPT_EN 1
#define OS_Q_QUERY_EN 1
#define OS_SCHED_LOCK_EN 1
#define OS_SEM_ACCEPT_EN 1
#define OS_SEM_DEL_EN 1
#define OS_SEM_EN 1
#define OS_SEM_QUERY_EN 1
#define OS_SEM_SET_EN 1
#define OS_TASK_CHANGE_PRIO_EN 1
#define OS_TASK_CREATE_EN 1
#define OS_TASK_CREATE_EXT_EN 1
#define OS_TASK_DEL_EN 1
#define OS_TASK_IDLE_STK_SIZE 512
#define OS_TASK_NAME_SIZE 32
#define OS_TASK_PROFILE_EN 1
#define OS_TASK_QUERY_EN 1
#define OS_TASK_STAT_EN 1
#define OS_TASK_STAT_STK_CHK_EN 1
#define OS_TASK_STAT_STK_SIZE 512
#define OS_TASK_SUSPEND_EN 1
#define OS_TASK_SW_HOOK_EN 1
#define OS_TASK_TMR_PRIO 0
#define OS_TASK_TMR_STK_SIZE 512
#define OS_THREAD_SAFE_NEWLIB 1
#define OS_TICKS_PER_SEC SYS_CLK_TIMER_TICKS_PER_SEC
#define OS_TICK_STEP_EN 1
#define OS_TIME_DLY_HMSM_EN 1
#define OS_TIME_DLY_RESUME_EN 1
#define OS_TIME_GET_SET_EN 1
#define OS_TIME_TICK_HOOK_EN 1
#define OS_TMR_CFG_MAX 16
#define OS_TMR_CFG_NAME_SIZE 16
#define OS_TMR_CFG_TICKS_PER_SEC 10
#define OS_TMR_CFG_WHEEL_SIZE 2
#define OS_TMR_EN 0

#endif /* __SYSTEM_H_ */