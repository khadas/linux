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

#ifndef __SYSTEM_UART_H__
#define __SYSTEM_UART_H__

#include "acamera_types.h"


/**
 *   Initialize uart connection
 *
 *   The function is called before any uart communication is run to
 *   initialise uart connection.
 *
 *   @return  fd - on success
 *           -1 - on error
 */
int32_t system_uart_init( void );


/**
 *   Write data with uart connection
 *
 *   The function writes specified amount of data
 *   to a given uart descriptor fd
 *
 *   @param   fd  - uard descriptor which was returned by system_uart_init
 *            p   - pointer to a data to be written
 *            len - amount of bytes to write
 *
 *   @return  >=0 - amount of written bytes on success
 *           -1 - on error
 */
int32_t system_uart_write( int32_t fd, const uint8_t *p, int32_t len );


/**
 *   Read data from uart connection
 *
 *   The function reads specified amount of data
 *   from a given uart descriptor fd
 *
 *   @param   fd  - uard descriptor which was returned by system_uart_init
 *            p   - pointer to a memory block to save input data
 *            len - amount of bytes to read
 *
 *   @return  >=0 - amount of read bytes on success
 *           -1 - on error
 */
int32_t system_uart_read( int32_t fd, uint8_t *p, int32_t len );

#endif // __SYSTEM_UART_H__
