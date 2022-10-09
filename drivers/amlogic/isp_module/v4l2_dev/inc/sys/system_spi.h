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

#ifndef __SYSTEM_SPI_H__
#define __SYSTEM_SPI_H__

#include "acamera_types.h"


#define AUTO_CPHA_CPOL 0x4000
#define AUTO_SS_MSK 0x2000
#define IE_MSK 0x1000
#define LSB_MSK 0x0800
#define TX_NEG_MSK 0x0400
#define RX_NEG_MSK 0x0200
#define GO_BUSY_MSK 0x0100
#define CHAR_LEN_MSK 0x007F


/**
 *   Initialize spi connection
 *
 *   The function is called before any uart communication is run to
 *   initialise uart connection.
 *
 *   @return  fd - on success
 *           -1 - on error
 */
int32_t system_spi_init( void );


/**
 *   Read data
 *
 *   The function reads 32 bit data from spi channel
 *
 *   @param sel_mask
 *   @param control
 *   @param data
 *   @param data_size
 *
 */
uint32_t system_spi_rw32( uint32_t sel_mask, uint32_t control, uint32_t data, uint8_t data_size );


/**
 *   Read data
 *
 *   The function reads 32 bit data from spi channel
 *
 *   @param sel_mask
 *   @param control
 *   @param data
 *   @param data_size
 *
 */
uint32_t system_spi_rw48( uint32_t sel_mask, uint32_t control, uint32_t addr, uint8_t addr_size, uint32_t data, uint8_t data_size );


#endif /* __SYSTEM_SPI_H__ */
