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

#ifndef __SYSTEM_TIMER_H__
#define __SYSTEM_TIMER_H__

#include "acamera_types.h"


/**
 *   Initialize system timer
 *
 *   The function system_timer_init() can be called at application level to
 *   initialise the timestamp facility.
 *
 *   @return  0 - on success
 *           -1 - on error
 */
void system_timer_init( void );


/**
 *   Return the current timer value
 *
 *   The function must return the current timer value.
 *
 *   @return  current timestamp
 */
uint32_t system_timer_timestamp( void );


/**
 *   Return the system timer frequency
 *
 *   This function must return a number of ticks per second for a system timer
 *   For example in the case if the timer uses microseconds this function should return
 *   1000000
 *
 *   @return positive number - timer frequency
 *           0 - on error
 */
uint32_t system_timer_frequency( void );


/**
 *   Usleep implementation for the current platform
 *
 *   This function must suspend execution of the calling thread
 *   by specified number of microseconds. The implementation of this function
 *   depends on the current platfrom.
 *
 *   @param   usec - time to sleep in microseconds
 *
 *   @return  0 - on success
 *           -1 - on error
 */
int32_t system_timer_usleep( uint32_t usec );


#endif /* __SYSTEM_TIMER_H__ */
