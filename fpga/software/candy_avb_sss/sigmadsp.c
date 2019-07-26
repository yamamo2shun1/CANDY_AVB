/******************************************************************************
* Copyright (c) 2006 Altera Corporation, San Jose, California, USA.           *
* All rights reserved. All use of this software and documentation is          *
* subject to the License Agreement located at the end of this file below.     *
*******************************************************************************                                                                           *
* Date - October 24, 2006                                                     *
* Module - led.c                                                              *
*                                                                             *
******************************************************************************/


/* 
 * Simple Socket Server (SSS) example. 
 * 
 * Please refer to the Altera NicheStack Tutorial documentation for details on this 
 * software example, as well as details on how to configure the NicheStack TCP/IP 
 * networking stack and MicroC/OS-II Real-Time Operating System.  
 */

/*
 * Include files:
 * 
 * <stdlib.h>: Contains C "rand()" function.
 * <stdio.h>: Contains C "printf()" function.
 * includes.h: This is the default MicroC/OS-II include file.
 * simple_socket_servert.h: Our simple socket server app's declarations.
 * altera_avalon_pio_regs.h: Defines register-access macros for PIO peripheral.
 * alt_error_handler.h: Altera Error Handler suite of development error 
 * handling functions.
 */


#include <stdlib.h> 
#include <stdio.h>  

#include "includes.h" 

#include "altera_avalon_pio_regs.h"
#include "altera_modular_adc.h"

#include "sys/alt_irq.h"
#include "sys/alt_cache.h"

#include "alt_error_handler.h"
#include "osc_server.h"

#include "SigmaStudioFW.h"

uint32_t adc[2] = {0};
volatile uint32_t adc_busy = 0;

void adc_callback(void *context)
{
	alt_adc_word_read(MODULAR_ADC_0_SAMPLE_STORE_CSR_BASE, adc, 2);
	adc_busy = 0;
}

void adc_init(void)
{
	adc_stop(MODULAR_ADC_0_SEQUENCER_CSR_BASE);
	adc_set_mode_run_once(MODULAR_ADC_0_SEQUENCER_CSR_BASE);
	alt_adc_register_callback
	(
		altera_modular_adc_open(MODULAR_ADC_0_SEQUENCER_CSR_NAME),
		(alt_adc_callback) adc_callback, NULL,
		MODULAR_ADC_0_SAMPLE_STORE_CSR_BASE
	);
}

/*
 * led_bit_toggle() sets or clears a bit in led_8_val, which corresponds
 * to 1 of 8 leds, and then writes led_8_val to a register on the Nios
 * Development Board which controls 8 LEDs, D0 - D7.
 * 
 */
 
void led_bit_toggle(OS_FLAGS bit)
{
    OS_FLAGS  led_8_val;
    INT8U error_code;
    
    led_8_val = OSFlagQuery(SigmaDSPEventFlag, &error_code);
    alt_uCOSIIErrorHandler(error_code, 0);
    if (bit & led_8_val)
    {
       led_8_val = OSFlagPost(SigmaDSPEventFlag, bit, OS_FLAG_CLR, &error_code);
       alt_uCOSIIErrorHandler(error_code, 0);
    }
    else
    {
       led_8_val = OSFlagPost(SigmaDSPEventFlag, bit, OS_FLAG_SET, &error_code);
       alt_uCOSIIErrorHandler(error_code, 0);
    }
    #ifdef LED_PIO_BASE
       IOWR_ALTERA_AVALON_PIO_DATA(LED_PIO_BASE, led_8_val);
       printf("Value for LED_PIO_BASE set to %d.\n", (INT8U)led_8_val);
    #endif
      
    return;
}
 
void SigmaDSPCommunicateTask()
{
   INT8U error_code;

   //ADAU1761 RESET
   IOWR_ALTERA_AVALON_PIO_DIRECTION(PIO_4_BASE, 1);
   IOWR_ALTERA_AVALON_PIO_DATA(PIO_4_BASE, 0);
   usleep(1000 * 20);
   IOWR_ALTERA_AVALON_PIO_DATA(PIO_4_BASE, 1);

   i2c_setup(0x00, 0xB3);

   default_download_IC_1();
   default_download_IC_2();

   adc_init();
   adc_busy = 1;
   adc_start(MODULAR_ADC_0_SEQUENCER_CSR_BASE);

   //IOWR(MM_BRIDGE_0_BASE, 0x000, 112);
   //IOWR(MM_BRIDGE_0_BASE, 0x000, 345);
   //int test = IORD(MM_BRIDGE_0_BASE, 0x000);
   printf("--------------------------\n");
   printf("test = %d\n", 0);
   printf("--------------------------\n");

   /* This is a task which does not terminate, so loop forever. */   
   while(1)
   {
      /* Wait 50 milliseconds between pattern updates, to make the pattern slow
       * enough for the human eye, and more impotantly, to give up control so
       * MicroC/OS-II can schedule other lower priority tasks. */ 
      OSTimeDlyHMSM(0,0,0,500);

      if (adc_busy == 0)
      {
    	  printf("adc = {%lu, %lu}", adc[0], adc[1]);

    	  IOWR(MM_BRIDGE_0_BASE, 0x000, adc[0]);
    	  IOWR(MM_BRIDGE_0_BASE, 0x001, adc[1]);

    	  adc_busy = 1;
    	  adc_start(MODULAR_ADC_0_SEQUENCER_CSR_BASE);
       }

      /* Check that we still have the SSSLEDLightshowSem semaphore. If we don't,
       * then wait until the LEDManagement task gives it back to us. */
      OSSemPend(SigmaDSPCommunicateSem, 0, &error_code);
      alt_uCOSIIErrorHandler(error_code, 0);
   
      /* Write a random value to 7-segment LEDs */
      

      error_code = OSSemPost(SigmaDSPCommunicateSem);
      alt_uCOSIIErrorHandler(error_code, 0);
      
   }
}

/*
 * The LedManagementTask() is a simple MicroC/OS-II task that controls Nios
 * Development Board LEDs based on commands received by the 
 * SSSSimpleSocketServerTask in the SSSLEDCommandQ. 
 * 
 * The task will read the SSSLedCommandQ for an 
 * in-coming message command from the SSSSimpleSocketServerTask. 
 */
 
void SigmaDSPManagementTask()
{
  
  INT32U sigma_dsp_command;
  BOOLEAN SigmaDSPCommunicateActive;
  INT8U error_code;
  
  SigmaDSPCommunicateActive = OS_TRUE;
  
  while(1)
  {
    sigma_dsp_command = (INT32U)OSQPend(SigmaDSPCommandQ, 0, &error_code);
   
    alt_uCOSIIErrorHandler(error_code, 0);
        
   } /* while(1) */
} 


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
* Altera does not recommend, suggest or require that this reference design    *
* file be used in conjunction or combination with any other product.          *
******************************************************************************/
