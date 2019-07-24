/******************************************************************************
* Copyright (c) 2006 Altera Corporation, San Jose, California, USA.           *
* All rights reserved. All use of this software and documentation is          *
* subject to the License Agreement located at the end of this file below.     *
*******************************************************************************
* Date - October 24, 2006                                                     *
* Module - simple_socket_server.c                                             *
*                                                                             *
******************************************************************************/
 
/******************************************************************************
 * Simple Socket Server (SSS) example. 
 * 
 * This example demonstrates the use of MicroC/OS-II running on NIOS II.       
 * In addition it is to serve as a good starting point for designs using       
 * MicroC/OS-II and Altera NicheStack TCP/IP Stack - NIOS II Edition.                                          
 *                                                                             
 * -Known Issues                                                             
 *     None.   
 *      
 * Please refer to the Altera NicheStack Tutorial documentation for details on this 
 * software example, as well as details on how to configure the NicheStack TCP/IP 
 * networking stack and MicroC/OS-II Real-Time Operating System.  
 */
 
#include <stdio.h>
#include <string.h>
#include <ctype.h> 

/* MicroC/OS-II definitions */
#include "includes.h"

/* Simple Socket Server definitions */
#include "alt_error_handler.h"

/* Nichestack definitions */
#include "ipport.h"
#include "tcpport.h"

#include "altera_avalon_pio_regs.h"

#include "osc.h"
#include "osc_server.h"

/*
 * Global handles (pointers) to our MicroC/OS-II resources. All of resources 
 * beginning with "SSS" are declared and created in this file.
 */

/*
 * This SSSLEDCommandQ MicroC/OS-II message queue will be used to communicate 
 * between the simple socket server task and Nios Development Board LED control 
 * tasks.
 *
 * Handle to our MicroC/OS-II Command Queue and variable definitions related to 
 * the Q for sending commands received on the TCP-IP socket from the 
 * SSSSimpleSocketServerTask to the LEDManagementTask.
 */
OS_EVENT  *SSSLEDCommandQ;
#define SSS_LED_COMMAND_Q_SIZE  30  /* Message capacity of SSSLEDCommandQ */
void *SSSLEDCommandQTbl[SSS_LED_COMMAND_Q_SIZE]; /*Storage for SSSLEDCommandQ*/

/*
 * Handle to our MicroC/OS-II LED Event Flag.  Each flag corresponds to one of
 * the LEDs on the Nios Development board, D0 - D7. 
 */
OS_FLAG_GRP *SSSLEDEventFlag;

/*
 * Handle to our MicroC/OS-II LED Lightshow Semaphore. The semaphore is checked 
 * by the LED7SegLightshowTask each time it updates 7 segment LED displays, 
 * U8 and U9.  The LEDManagementTask grabs the semaphore away from the lightshow task to
 * toggle the lightshow off, and gives up the semaphore to turn the lightshow
 * back on.  The LEDManagementTask does this in response to the CMD_LEDS_LIGHTSHOW
 * command sent from the SSSSimpleSocketServerTask when the user sends a toggle 
 * lightshow command over the TCPIP socket.
 */
OS_EVENT *SSSLEDLightshowSem;

/* Definition of Task Stacks for tasks not invoked by TK_NEWTASK 
 * (do not use NicheStack) 
 */

OS_STK    LEDManagementTaskStk[TASK_STACKSIZE];
OS_STK    LED7SegLightshowTaskStk[TASK_STACKSIZE];

/*
 * Create our MicroC/OS-II resources. All of the resources beginning with 
 * "SSS" are declared in this file, and created in this function.
 */
void SSSCreateOSDataStructs(void)
{
  INT8U error_code;
  
  /*
  * Create the resource for our MicroC/OS-II Queue for sending commands 
  * received on the TCP/IP socket from the SSSSimpleSocketServerTask()
  * to the LEDManagementTask().
  */
  SSSLEDCommandQ = OSQCreate(&SSSLEDCommandQTbl[0], SSS_LED_COMMAND_Q_SIZE);
  if (!SSSLEDCommandQ)
  {
     alt_uCOSIIErrorHandler(EXPANDED_DIAGNOSIS_CODE, 
     "Failed to create SSSLEDCommandQ.\n");
  }
  
 /* Create our MicroC/OS-II LED Lightshow Semaphore.  The semaphore is checked 
  * by the SSSLEDLightshowTask each time it updates 7 segment LED displays, 
  * U8 and U9.  The LEDTask grabs the semaphore away from the lightshow task to
  * toggle the lightshow off, and gives up the semaphore to turn the lightshow
  * back on.  The LEDTask does this in response to the CMD_LEDS_LIGHTSHOW
  * command sent from the SSSSimpleSocketServerTask when the user sends the 
  * toggle lightshow command over the TCPIP socket.
  */
  SSSLEDLightshowSem = OSSemCreate(1);
  if (!SSSLEDLightshowSem)
  {
     alt_uCOSIIErrorHandler(EXPANDED_DIAGNOSIS_CODE, 
                            "Failed to create SSSLEDLightshowSem.\n");
  }
  
 /*
  * Create our MicroC/OS-II LED Event Flag.  Each flag corresponds to one of
  * the LEDs on the Nios Development board, D0 - D7. 
  */   
  SSSLEDEventFlag = OSFlagCreate(0, &error_code);
  if (!SSSLEDEventFlag)
  {
     alt_uCOSIIErrorHandler(error_code, 0);
  }
}

void SSSCreateTasks(void)
{
   INT8U error_code;
  
   error_code = OSTaskCreateExt(LED7SegLightshowTask,
                             NULL,
                             (void *)&LED7SegLightshowTaskStk[TASK_STACKSIZE-1],
                             LED_7SEG_LIGHTSHOW_TASK_PRIORITY,
                             LED_7SEG_LIGHTSHOW_TASK_PRIORITY,
                             LED7SegLightshowTaskStk,
                             TASK_STACKSIZE,
                             NULL,
                             0);
   
   alt_uCOSIIErrorHandler(error_code, 0);
  
   error_code = OSTaskCreateExt(LEDManagementTask,
                              NULL,
                              (void *)&LEDManagementTaskStk[TASK_STACKSIZE-1],
                              LED_MANAGEMENT_TASK_PRIORITY,
                              LED_MANAGEMENT_TASK_PRIORITY,
                              LEDManagementTaskStk,
                              TASK_STACKSIZE,
                              NULL,
                              0);

   alt_uCOSIIErrorHandler(error_code, 0);

}

void SSSSimpleSocketServerTask()
{
  struct sockaddr_in addr;

  int socket;
  struct sockaddr_in from_addr;
  int from_len = sizeof(from_addr);//100;
  char rcv_buffer[1024];
  int recv_count = 0;

  IOWR_ALTERA_AVALON_PIO_DATA(PIO_0_BASE, 0x00);

  //if ((fd_listen = socket(AF_INET, SOCK_STREAM, 0)) < 0)
  if ((socket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
  {
    alt_NetworkErrorHandler(EXPANDED_DIAGNOSIS_CODE,"[sss_task] Socket creation failed");
  }
  
  addr.sin_family = AF_INET;
  addr.sin_port = htons(getLocalPort());
  addr.sin_addr.s_addr = INADDR_ANY;
  
  if (bind(socket, (struct sockaddr *)&addr, sizeof(addr)) < 0)
  {
    alt_NetworkErrorHandler(EXPANDED_DIAGNOSIS_CODE,"[sss_task] Bind failed");
  }

  printf("[sss_task] Open Sound Control listening on port %d\n", getLocalPort());
   
  while(1)
  {
    recv_count = recvfrom(socket, rcv_buffer, 256, 0, (struct sockaddr *)&from_addr, &from_len);
    //printf("test... %d\n", recv_count);

    if (recv_count > 0)
    {
    	getOSCPacket(rcv_buffer, recv_count);

    	processOSCPacket();
    	processOSCPacket();
    	processOSCPacket();
    	processOSCPacket();
    	processOSCPacket();

    	if (compareOSCPrefix("/candy"))
    	{
    		if (compareOSCAddress("/onboard/led"))
    		{
    			switch (getIntArgumentAtIndex(0))
    			{
    			case 0:
    				if (!strcmp(getStringArgumentAtIndex(1), "on"))
    				{
    					IOWR_ALTERA_AVALON_PIO_DATA(PIO_0_BASE, 0x01 | (IORD_ALTERA_AVALON_PIO_DATA(PIO_0_BASE) & 0x02));
    				}
    				else if (!strcmp(getStringArgumentAtIndex(1), "off"))
    				{
    					IOWR_ALTERA_AVALON_PIO_DATA(PIO_0_BASE, 0x00 | (IORD_ALTERA_AVALON_PIO_DATA(PIO_0_BASE) & 0x02));
    				}
    				break;
    			case 1:
    				if (!strcmp(getStringArgumentAtIndex(1), "on"))
    				{
    				    IOWR_ALTERA_AVALON_PIO_DATA(PIO_0_BASE, 0x02 | (IORD_ALTERA_AVALON_PIO_DATA(PIO_0_BASE) & 0x01));
    				}
    				else if (!strcmp(getStringArgumentAtIndex(1), "off"))
    				{
    				    IOWR_ALTERA_AVALON_PIO_DATA(PIO_0_BASE, 0x00 | (IORD_ALTERA_AVALON_PIO_DATA(PIO_0_BASE) & 0x01));
    				}
    				break;
    			}

    		}
    	}
    }
  } /* while(1) */
}

