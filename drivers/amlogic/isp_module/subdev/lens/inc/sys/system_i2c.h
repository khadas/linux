/*
*
* SPDX-License-Identifier: GPL-2.0
*
* Copyright (C) 2011-2018 ARM or its affiliates
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; version 2.
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
* or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
* for more details.
* You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*
*/

#ifndef __SYSTEM_I2C_H__
#define __SYSTEM_I2C_H__

#include "acamera_types.h"

#define I2C_NOCONNECT ( -1 )
#define I2C_OK ( 0 )
#define I2C_ACK ( 0 )
#define I2C_NOACK ( 1 )
#define I2C_ABITRATION_LOST ( 2 )

/**
 *   Initialize i2c channel
 *
 *   This function is called by firmware to establish i2c communication.
 *
 *   @return none
 */
void system_i2c_init( uint32_t bus );


/**
 *   Close i2c connections
 *
 *   This function is used by firmware to close i2c connection
 *
 *   @return none
 */
void system_i2c_deinit( uint32_t bus );


/**
 *   Write a data through i2c channel
 *
 *   This function is used by firmware to write specified amount of data
 *   to a specific device.
 *
 *   @param address - i2c address
 *          data - pointer to the start of the memory block
 *          size - amount of bytes to write
 *
 *   @return I2C_NOCONNECT - no connection
 *           I2C_OK - success
 *           I2C_NOACK - no acknowledge from a device
 */
uint8_t system_i2c_write( uint32_t bus, uint32_t address, uint8_t *data, uint32_t size );


/**
 *   Read data from i2c channel
 *
 *   This function is used by firmware to read specified amount of data
 *   from a specific device.
 *
 *   @param address - i2c address
 *          data - pointer to the start of the memory block
 *          size - amount of bytes to write
 *
 *   @return I2C_NOCONNECT - no connection
 *           I2C_OK - success
 *           I2C_NOACK - no acknowledge from a device
 */
uint8_t system_i2c_read( uint32_t bus, uint32_t address, uint8_t *data, uint32_t size );


#endif /* __SYSTEM_I2C_H__ */
