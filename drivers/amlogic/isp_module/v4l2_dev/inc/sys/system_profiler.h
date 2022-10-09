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

#ifndef __SYSTEM_PROFILER_H__
#define __SYSTEM_PROFILER_H__

#include "acamera_types.h"

/**
 *   Return system CPU frequency
 *
 *   @return  frequency - on success
 *           -1 - on error
 */
int32_t cpu_get_freq( void );


/**
 *   Initialize system clock
 *
 *
 *   @return  none
 */
void cpu_start_clocks( void );


/**
 *   Stop system clock
 *
 *   The function returns the time difference between the current time and
 *   previous call of cpu_start_clocks
 *
 *
 *   @return  time - on success
 *           -1 - on error
 */
uint64_t cpu_stop_clocks( void );


/**
 *   Initialize profiler
 *
 *   @return  none
 */
void cpu_init_profiler( void );

uint32_t acamera_isp_io_get_counter( void );

#endif /* __SYSTEM_PROFILER_H__ */
