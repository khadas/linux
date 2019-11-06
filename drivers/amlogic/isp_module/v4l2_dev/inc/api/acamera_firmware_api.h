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

#ifndef __ACAMERA_FIRMWARE_API_H__
#define __ACAMERA_FIRMWARE_API_H__

#include "acamera_types.h"
#include "acamera_sensor_api.h"
#include "acamera_lens_api.h"
#include "acamera_command_api.h"
#include "acamera_firmware_settings.h"

/**
 *   Initialize one instance of firmware context
 *
 *   The firmware can control several context at the same time. Each context must be initialized with its own
 *   set of setting and independently from all other contexts. A pointer will be returned on valid context on
 *   successful initialization.
 *
 *   @param  settings - a structure with setting for context to be initialized with
 *   @param  ctx_num - number of contexts to be initialized
 *
 *   @return 0 - success
 *          -1 - fail.
 */
int32_t acamera_init( acamera_settings *settings, uint32_t ctx_num );


/**
 *   Terminate the firmware
 *
 *   The function will close the firmware and free all previously allocated resources
 *
 *   @return 0 - success
 *          -1 - fail.
 */
int32_t acamera_terminate( void );


/**
 *   Process one instance of firmware context
 *
 *   The firmware can control several context at the same time. Each context must be given a CPU time to fulfil
 *   all tasks it has at the moment. This function has to be called for all contexts as frequent as possible to avoid
 *   delays.
 *
 *   @param  ctx - context pointer which was returned by acamera_init
 *
 *   @return 0 - success
 *          -1 - fail.
 */
int32_t acamera_process( void );


/**
 *   Process interrupts
 *
 *   This function must be called when any of interrupts happen. It will process interrupts properly to guarantee
 *   the correct firmware behaviour.
 *
 *   @return 0 - success
 *          -1 - fail.
 */
int32_t acamera_interrupt_handler( void );


#endif // __ACAMERA_FIRMWARE_API_H__
