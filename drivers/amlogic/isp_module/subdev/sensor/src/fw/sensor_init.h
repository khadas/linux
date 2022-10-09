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

#ifndef __SENSOR_INIT_H__
#define __SENSOR_INIT_H__

#include "acamera_sbus_api.h"


#if SENSOR_BINARY_SEQUENCE == 1
#define sensor_load_sequence acamera_sensor_load_binary_sequence
#else
#define sensor_load_sequence acamera_sensor_load_array_sequence
#endif

typedef struct acam_reg_t {
    uint32_t address;
    uint32_t value;
    uint32_t mask;
    uint8_t len;
} acam_reg_t;

void acamera_sensor_load_binary_sequence( acamera_sbus_ptr_t p_sbus, char size, const char *sequence, int group );
void acamera_sensor_load_array_sequence( acamera_sbus_ptr_t p_sbus, char size, const acam_reg_t **sequence, int group );

#endif /* __SENSOR_INIT_H__ */
