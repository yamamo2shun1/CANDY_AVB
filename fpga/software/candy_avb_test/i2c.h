/*
 * i2c.h
 *
 *  Created on: 2019/01/17
 *      Author: Shunichi Yamamoto
 */

#ifndef I2C_H_
#define I2C_H_

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>

#include "system.h"
#include "altera_avalon_pio_regs.h"

#define SCL_DELAY usleep(2)

//int i2c_send_byte(long sclk, long sdat);
//unsigned short i2c_receive_byte(long sclk, long sdat);
bool i2c_multiple_write(long sclk, long sdat, uint8_t DeviceAddr, uint16_t ControlAddr, uint8_t szData[], uint16_t len);


#endif /* I2C_H_ */
