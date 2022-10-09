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

#include "application_command_api.h"
#include "acamera_command_api.h"
#include "acamera_types.h"
#include "acamera_firmware_config.h"

extern uint32_t acamera_get_api_context( void );

uint8_t application_command( uint8_t command_type, uint8_t command, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    uint8_t ret = NOT_EXISTS;
    uint32_t ctx_id = acamera_get_api_context();

    ret = acamera_command( ctx_id, command_type, command, value, direction, ret_value );

    return ret;
}


uint8_t application_api_calibration( uint8_t type, uint8_t id, uint8_t direction, void *data, uint32_t data_size, uint32_t *ret_value )
{
    uint8_t ret = SUCCESS;

    uint32_t ctx_id = acamera_get_api_context();
    ret = acamera_api_calibration( ctx_id, type, id, direction, data, data_size, ret_value );
    return ret;
}
