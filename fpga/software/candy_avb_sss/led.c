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


/* <stdlib.h>: Contains C "rand()" function. */
#include <stdlib.h> 

/* <stdio.h>: Contains C "printf()" function. */
#include <stdio.h>  

/* MicroC/OS-II definitions */
#include "includes.h" 

/* Device driver accessor macros for peripherial I/O component 
 * (used for leds).) */
#include "altera_avalon_pio_regs.h"

/* Simple Socket Server definitions */
#include "alt_error_handler.h"
#include "osc_server.h"


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
    
    led_8_val = OSFlagQuery(SSSLEDEventFlag, &error_code);
    alt_uCOSIIErrorHandler(error_code, 0);
    if (bit & led_8_val)
    {
       led_8_val = OSFlagPost(SSSLEDEventFlag, bit, OS_FLAG_CLR, &error_code);
       alt_uCOSIIErrorHandler(error_code, 0);
    }
    else
    {
       led_8_val = OSFlagPost(SSSLEDEventFlag, bit, OS_FLAG_SET, &error_code);
       alt_uCOSIIErrorHandler(error_code, 0);
    }
    #ifdef LED_PIO_BASE
       IOWR_ALTERA_AVALON_PIO_DATA(LED_PIO_BASE, led_8_val);
       printf("Value for LED_PIO_BASE set to %d.\n", (INT8U)led_8_val);
    #endif
      
    return;
}
      
/*
 *    LED7SegLightshowTask displays an LED Lightshow by writing random values 
 * to the 7 segment LEDs, U8 and U9.  The show is started and stopped in 
 * response to a command from the SSSSimpleSocketServerTask, via 
 * SSSLEDLightshowSem semaphore controlled in LEDManagementTask.
 * 
 */ 
 
void LED7SegLightshowTask()
{
   INT8U error_code;
   INT16U  __attribute__ ((unused))  led_7_seg_val;  /* Attribute suppresses "unused variable" warning. */

   static INT8U led_toggle = 0;

   /* This is a task which does not terminate, so loop forever. */   
   while(1)
   {
    
      /* Wait 50 milliseconds between pattern updates, to make the pattern slow
       * enough for the human eye, and more impotantly, to give up control so
       * MicroC/OS-II can schedule other lower priority tasks. */ 
      OSTimeDlyHMSM(0,0,0,500);
      
      /* Check that we still have the SSSLEDLightshowSem semaphore. If we don't,
       * then wait until the LEDManagement task gives it back to us. */
      OSSemPend(SSSLEDLightshowSem, 0, &error_code);
      alt_uCOSIIErrorHandler(error_code, 0);
   
      /* Write a random value to 7-segment LEDs */
      #if SEVEN_SEG_PIO_BASE
         led_7_seg_val = rand();
         IOWR_ALTERA_AVALON_PIO_DATA(SEVEN_SEG_PIO_BASE, led_7_seg_val);
      #endif
      
#if 0
      switch (led_toggle)
      {
      case 0:
    	  IOWR_ALTERA_AVALON_PIO_DATA(PIO_0_BASE, 0x01);
    	  led_toggle = 1;
    	  break;
      case 1:
    	  IOWR_ALTERA_AVALON_PIO_DATA(PIO_0_BASE, 0x02);
    	  led_toggle = 0;
    	  break;
      }
#endif

      error_code = OSSemPost(SSSLEDLightshowSem);
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
 
void LEDManagementTask()
{
  
  INT32U led_command; 
  BOOLEAN SSSLEDLightshowActive;
  INT8U error_code;
  
  SSSLEDLightshowActive = OS_TRUE;
  
  while(1)
  {
    led_command = (INT32U)OSQPend(SSSLEDCommandQ, 0, &error_code);
   
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
