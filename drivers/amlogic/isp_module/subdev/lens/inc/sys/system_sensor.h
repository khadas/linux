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

#ifndef __SYSTEM_SENSOR_H__
#define __SYSTEM_SENSOR_H__

#include "acamera_types.h"

/**
 *   Reset a sensor
 *
 *   This function must reset the current sensor
 *
 *   @param mask  - a parameter which will be sent from a sensor driver
 */
void system_reset_sensor( uint32_t mask );

#endif /* __SYSTEM_SENSOR_H__ */
