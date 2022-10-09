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

#ifndef __ACAMERADEFAULT_H__
#define __ACAMERADEFAULT_H__

//#include <stdint.h>
#include "acamera_sbus_api.h"

typedef struct _sensor_ACameraDefault_iface_t {
    acamera_sbus_t mipi_i2c_bus;
} sensor_ACameraDefault_iface_t;
typedef sensor_ACameraDefault_iface_t *sensor_ACameraDefault_iface_ptr_t;

void reset_sensor_interface( sensor_ACameraDefault_iface_ptr_t p_iface );
void load_sensor_interface( sensor_ACameraDefault_iface_ptr_t p_iface, uint8_t mode );
void mipi_auto_tune( sensor_ACameraDefault_iface_ptr_t p_iface, uint8_t mode, uint32_t refw, uint32_t refh );

#endif /* __ACAMERADEFAULT_H__ */
