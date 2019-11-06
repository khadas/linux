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

#include "acamera_types.h"
#include "acamera_firmware_config.h"



extern void sensor_init_dummy( void** ctx, sensor_control_t*) ;
extern void sensor_deinit_dummy( void *ctx ) ;
extern uint32_t get_calibrations_dummy( uint32_t ctx_num,void * sensor_arg,ACameraCalibrations *) ;

extern int32_t lens_init( void** ctx, lens_control_t* ctrl ) ;
extern void lens_deinit( void * ctx) ;
#define SENSOR_INIT_SUBDEV_FUNCTIONS {sensor_init_dummy,}
#define SENSOR_DEINIT_SUBDEV_FUNCTIONS {sensor_deinit_dummy,}
#define CALIBRATION_SUBDEV_FUNCTIONS {get_calibrations_dummy,}
