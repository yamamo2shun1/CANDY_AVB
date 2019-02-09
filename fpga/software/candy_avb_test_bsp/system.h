/*
 * system.h - SOPC Builder system and BSP software package information
 *
 * Machine generated for CPU 'nios2_0' in SOPC Builder design 'candy_avb_test_qsys'
 * SOPC Builder design path: ../../candy_avb_test_qsys.sopcinfo
 *
 * Generated: Fri Feb 08 08:28:13 JST 2019
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
#define ALT_CPU_BREAK_ADDR 0x41080820
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
#define ALT_CPU_EXCEPTION_ADDR 0x40800020
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
#define ALT_CPU_INST_ADDR_WIDTH 0x1f
#define ALT_CPU_NAME "nios2_0"
#define ALT_CPU_NUM_OF_SHADOW_REG_SETS 0
#define ALT_CPU_OCI_VERSION 1
#define ALT_CPU_RESET_ADDR 0x41040000


/*
 * CPU configuration (with legacy prefix - don't use these anymore)
 *
 */

#define NIOS2_BIG_ENDIAN 0
#define NIOS2_BREAK_ADDR 0x41080820
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
#define NIOS2_EXCEPTION_ADDR 0x40800020
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
#define NIOS2_INST_ADDR_WIDTH 0x1f
#define NIOS2_NUM_OF_SHADOW_REG_SETS 0
#define NIOS2_OCI_VERSION 1
#define NIOS2_RESET_ADDR 0x41040000


/*
 * Define for each module class mastered by the CPU
 *
 */

#define __ALTERA_AVALON_JTAG_UART
#define __ALTERA_AVALON_NEW_SDRAM_CONTROLLER
#define __ALTERA_AVALON_PIO
#define __ALTERA_AVALON_SYSID_QSYS
#define __ALTERA_AVALON_TIMER
#define __ALTERA_AVALON_UART
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
#define ALT_STDERR_BASE 0x410810d0
#define ALT_STDERR_DEV jtaguart_0
#define ALT_STDERR_IS_JTAG_UART
#define ALT_STDERR_PRESENT
#define ALT_STDERR_TYPE "altera_avalon_jtag_uart"
#define ALT_STDIN "/dev/jtaguart_0"
#define ALT_STDIN_BASE 0x410810d0
#define ALT_STDIN_DEV jtaguart_0
#define ALT_STDIN_IS_JTAG_UART
#define ALT_STDIN_PRESENT
#define ALT_STDIN_TYPE "altera_avalon_jtag_uart"
#define ALT_STDOUT "/dev/jtaguart_0"
#define ALT_STDOUT_BASE 0x410810d0
#define ALT_STDOUT_DEV jtaguart_0
#define ALT_STDOUT_IS_JTAG_UART
#define ALT_STDOUT_PRESENT
#define ALT_STDOUT_TYPE "altera_avalon_jtag_uart"
#define ALT_SYSTEM_NAME "candy_avb_test_qsys"


/*
 * altpll_0 configuration
 *
 */

#define ALTPLL_0_BASE 0x410810b0
#define ALTPLL_0_IRQ -1
#define ALTPLL_0_IRQ_INTERRUPT_CONTROLLER_ID -1
#define ALTPLL_0_NAME "/dev/altpll_0"
#define ALTPLL_0_SPAN 16
#define ALTPLL_0_TYPE "altpll"
#define ALT_MODULE_CLASS_altpll_0 altpll


/*
 * avalon_wb_0 configuration
 *
 */

#define ALT_MODULE_CLASS_avalon_wb_0 avalon_wb
#define AVALON_WB_0_BASE 0x0
#define AVALON_WB_0_IRQ -1
#define AVALON_WB_0_IRQ_INTERRUPT_CONTROLLER_ID -1
#define AVALON_WB_0_NAME "/dev/avalon_wb_0"
#define AVALON_WB_0_SPAN 1073741824
#define AVALON_WB_0_TYPE "avalon_wb"


/*
 * hal configuration
 *
 */

#define ALT_INCLUDE_INSTRUCTION_RELATED_EXCEPTION_API
#define ALT_MAX_FD 32
#define ALT_SYS_CLK TIMER_0
#define ALT_TIMESTAMP_CLK none


/*
 * jtaguart_0 configuration
 *
 */

#define ALT_MODULE_CLASS_jtaguart_0 altera_avalon_jtag_uart
#define JTAGUART_0_BASE 0x410810d0
#define JTAGUART_0_IRQ 0
#define JTAGUART_0_IRQ_INTERRUPT_CONTROLLER_ID 0
#define JTAGUART_0_NAME "/dev/jtaguart_0"
#define JTAGUART_0_READ_DEPTH 64
#define JTAGUART_0_READ_THRESHOLD 8
#define JTAGUART_0_SPAN 8
#define JTAGUART_0_TYPE "altera_avalon_jtag_uart"
#define JTAGUART_0_WRITE_DEPTH 64
#define JTAGUART_0_WRITE_THRESHOLD 8


/*
 * new_sdram_controller_0 configuration
 *
 */

#define ALT_MODULE_CLASS_new_sdram_controller_0 altera_avalon_new_sdram_controller
#define NEW_SDRAM_CONTROLLER_0_BASE 0x40800000
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
#define ONCHIP_FLASH_0_CSR_BASE 0x410810c0
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
#define ONCHIP_FLASH_0_CSR_SECTOR4_ENABLED 0
#define ONCHIP_FLASH_0_CSR_SECTOR4_END_ADDR 0xffffffff
#define ONCHIP_FLASH_0_CSR_SECTOR4_START_ADDR 0xffffffff
#define ONCHIP_FLASH_0_CSR_SECTOR5_ENABLED 0
#define ONCHIP_FLASH_0_CSR_SECTOR5_END_ADDR 0xffffffff
#define ONCHIP_FLASH_0_CSR_SECTOR5_START_ADDR 0xffffffff
#define ONCHIP_FLASH_0_CSR_SPAN 8
#define ONCHIP_FLASH_0_CSR_TYPE "altera_onchip_flash"


/*
 * onchip_flash_0_data configuration
 *
 */

#define ALT_MODULE_CLASS_onchip_flash_0_data altera_onchip_flash
#define ONCHIP_FLASH_0_DATA_BASE 0x41040000
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
#define ONCHIP_FLASH_0_DATA_SECTOR4_ENABLED 0
#define ONCHIP_FLASH_0_DATA_SECTOR4_END_ADDR 0xffffffff
#define ONCHIP_FLASH_0_DATA_SECTOR4_START_ADDR 0xffffffff
#define ONCHIP_FLASH_0_DATA_SECTOR5_ENABLED 0
#define ONCHIP_FLASH_0_DATA_SECTOR5_END_ADDR 0xffffffff
#define ONCHIP_FLASH_0_DATA_SECTOR5_START_ADDR 0xffffffff
#define ONCHIP_FLASH_0_DATA_SPAN 188416
#define ONCHIP_FLASH_0_DATA_TYPE "altera_onchip_flash"


/*
 * pio_0 configuration
 *
 */

#define ALT_MODULE_CLASS_pio_0 altera_avalon_pio
#define PIO_0_BASE 0x410810a0
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
#define PIO_1_BASE 0x41081090
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
#define PIO_4_BASE 0x41081080
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
 * sysid_qsys_0 configuration
 *
 */

#define ALT_MODULE_CLASS_sysid_qsys_0 altera_avalon_sysid_qsys
#define SYSID_QSYS_0_BASE 0x410810c8
#define SYSID_QSYS_0_ID 0
#define SYSID_QSYS_0_IRQ -1
#define SYSID_QSYS_0_IRQ_INTERRUPT_CONTROLLER_ID -1
#define SYSID_QSYS_0_NAME "/dev/sysid_qsys_0"
#define SYSID_QSYS_0_SPAN 8
#define SYSID_QSYS_0_TIMESTAMP 1549581623
#define SYSID_QSYS_0_TYPE "altera_avalon_sysid_qsys"


/*
 * timer_0 configuration
 *
 */

#define ALT_MODULE_CLASS_timer_0 altera_avalon_timer
#define TIMER_0_ALWAYS_RUN 0
#define TIMER_0_BASE 0x41081020
#define TIMER_0_COUNTER_SIZE 32
#define TIMER_0_FIXED_PERIOD 0
#define TIMER_0_FREQ 100000000
#define TIMER_0_IRQ 1
#define TIMER_0_IRQ_INTERRUPT_CONTROLLER_ID 0
#define TIMER_0_LOAD_VALUE 99999
#define TIMER_0_MULT 0.001
#define TIMER_0_NAME "/dev/timer_0"
#define TIMER_0_PERIOD 1
#define TIMER_0_PERIOD_UNITS "ms"
#define TIMER_0_RESET_OUTPUT 0
#define TIMER_0_SNAPSHOT 1
#define TIMER_0_SPAN 32
#define TIMER_0_TICKS_PER_SEC 1000
#define TIMER_0_TIMEOUT_PULSE_OUTPUT 0
#define TIMER_0_TYPE "altera_avalon_timer"


/*
 * timer_1 configuration
 *
 */

#define ALT_MODULE_CLASS_timer_1 altera_avalon_timer
#define TIMER_1_ALWAYS_RUN 0
#define TIMER_1_BASE 0x41081040
#define TIMER_1_COUNTER_SIZE 32
#define TIMER_1_FIXED_PERIOD 0
#define TIMER_1_FREQ 100000000
#define TIMER_1_IRQ 2
#define TIMER_1_IRQ_INTERRUPT_CONTROLLER_ID 0
#define TIMER_1_LOAD_VALUE 99999
#define TIMER_1_MULT 0.001
#define TIMER_1_NAME "/dev/timer_1"
#define TIMER_1_PERIOD 1
#define TIMER_1_PERIOD_UNITS "ms"
#define TIMER_1_RESET_OUTPUT 0
#define TIMER_1_SNAPSHOT 1
#define TIMER_1_SPAN 32
#define TIMER_1_TICKS_PER_SEC 1000
#define TIMER_1_TIMEOUT_PULSE_OUTPUT 0
#define TIMER_1_TYPE "altera_avalon_timer"


/*
 * timer_2 configuration
 *
 */

#define ALT_MODULE_CLASS_timer_2 altera_avalon_timer
#define TIMER_2_ALWAYS_RUN 0
#define TIMER_2_BASE 0x41081000
#define TIMER_2_COUNTER_SIZE 32
#define TIMER_2_FIXED_PERIOD 0
#define TIMER_2_FREQ 100000000
#define TIMER_2_IRQ 3
#define TIMER_2_IRQ_INTERRUPT_CONTROLLER_ID 0
#define TIMER_2_LOAD_VALUE 99999
#define TIMER_2_MULT 0.001
#define TIMER_2_NAME "/dev/timer_2"
#define TIMER_2_PERIOD 1
#define TIMER_2_PERIOD_UNITS "ms"
#define TIMER_2_RESET_OUTPUT 0
#define TIMER_2_SNAPSHOT 1
#define TIMER_2_SPAN 32
#define TIMER_2_TICKS_PER_SEC 1000
#define TIMER_2_TIMEOUT_PULSE_OUTPUT 0
#define TIMER_2_TYPE "altera_avalon_timer"


/*
 * uart_0 configuration
 *
 */

#define ALT_MODULE_CLASS_uart_0 altera_avalon_uart
#define UART_0_BASE 0x41081060
#define UART_0_BAUD 115200
#define UART_0_DATA_BITS 8
#define UART_0_FIXED_BAUD 1
#define UART_0_FREQ 100000000
#define UART_0_IRQ 4
#define UART_0_IRQ_INTERRUPT_CONTROLLER_ID 0
#define UART_0_NAME "/dev/uart_0"
#define UART_0_PARITY 'N'
#define UART_0_SIM_CHAR_STREAM ""
#define UART_0_SIM_TRUE_BAUD 0
#define UART_0_SPAN 32
#define UART_0_STOP_BITS 1
#define UART_0_SYNC_REG_DEPTH 2
#define UART_0_TYPE "altera_avalon_uart"
#define UART_0_USE_CTS_RTS 1
#define UART_0_USE_EOP_REGISTER 0

#endif /* __SYSTEM_H_ */
