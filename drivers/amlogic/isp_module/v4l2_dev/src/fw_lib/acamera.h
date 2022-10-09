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

#ifndef __ACAMERA_H__
#define __ACAMERA_H__


#include "acamera_types.h"
#include "acamera_firmware_config.h"
#include "acamera_firmware_api.h"
#include "acamera_logger.h"
#include "system_interrupts.h"
#include "system_semaphore.h"
#include "acamera_math.h"


typedef void ( *buffer_callback_t )( void *ctx_param, tframe_t *tframe, const metadata_t *metadata );

int32_t acamera_get_context_number( void );

void acamera_update_cur_settings_to_isp( int port );

void acamera_reset_ping_pong_port(void);

#endif /* __ACAMERA_H__ */
