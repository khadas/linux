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


extern void sensor_init_ov08a10( void** ctx, sensor_control_t*, void*) ;
extern void sensor_deinit_ov08a10( void *ctx ) ;
extern uint32_t get_calibrations_ov08a10( uint32_t ctx_num,void * sensor_arg,ACameraCalibrations *) ;

extern void sensor_init_imx290( void** ctx, sensor_control_t*, void*) ;
extern void sensor_deinit_imx290( void *ctx ) ;
extern uint32_t get_calibrations_imx290( uint32_t ctx_num,void * sensor_arg,ACameraCalibrations *) ;
extern uint32_t get_calibrations_imx290_lens_8mm( uint32_t ctx_num, void * sensor_arg, ACameraCalibrations *) ;
extern uint32_t get_calibrations_imx290_lens_4mm( uint32_t ctx_num, void * sensor_arg, ACameraCalibrations *) ;

extern void sensor_init_imx227( void** ctx, sensor_control_t*, void*) ;
extern void sensor_deinit_imx227( void *ctx ) ;
extern uint32_t get_calibrations_imx227( uint32_t ctx_num,void * sensor_arg,ACameraCalibrations *) ;

extern void sensor_init_imx481( void** ctx, sensor_control_t*, void*) ;
extern void sensor_deinit_imx481( void *ctx ) ;
extern uint32_t get_calibrations_imx481( uint32_t ctx_num,void * sensor_arg,ACameraCalibrations *) ;
extern void sensor_init_imx307( void** ctx, sensor_control_t*, void*) ;
extern void sensor_deinit_imx307( void *ctx ) ;
extern uint32_t get_calibrations_imx307( uint32_t ctx_num,void * sensor_arg,ACameraCalibrations *) ;


#define SENSOR_INIT_SUBDEV_FUNCTIONS_OS08A10 sensor_init_ov08a10
#define SENSOR_DEINIT_SUBDEV_FUNCTIONS_OS08A10 sensor_deinit_ov08a10
#define CALIBRATION_SUBDEV_FUNCTIONS_OS08A10 get_calibrations_ov08a10

#define SENSOR_INIT_SUBDEV_FUNCTIONS_IMX290 sensor_init_imx290
#define SENSOR_DEINIT_SUBDEV_FUNCTIONS_IMX290 sensor_deinit_imx290
#define CALIBRATION_SUBDEV_FUNCTIONS_IMX290 get_calibrations_imx290
#define CALIBRATION_SUBDEV_FUNCTIONS_IMX290_LENS_8mm  get_calibrations_imx290_lens_8mm
#define CALIBRATION_SUBDEV_FUNCTIONS_IMX290_LENS_4mm  get_calibrations_imx290_lens_4mm

#define SENSOR_INIT_SUBDEV_FUNCTIONS_IMX227 sensor_init_imx227
#define SENSOR_DEINIT_SUBDEV_FUNCTIONS_IMX227 sensor_deinit_imx227
#define CALIBRATION_SUBDEV_FUNCTIONS_IMX227 get_calibrations_imx227

#define SENSOR_INIT_SUBDEV_FUNCTIONS_IMX481 sensor_init_imx481
#define SENSOR_DEINIT_SUBDEV_FUNCTIONS_IMX481 sensor_deinit_imx481
#define CALIBRATION_SUBDEV_FUNCTIONS_IMX481 get_calibrations_imx481
#define SENSOR_INIT_SUBDEV_FUNCTIONS_IMX307 sensor_init_imx307
#define SENSOR_DEINIT_SUBDEV_FUNCTIONS_IMX307 sensor_deinit_imx307
#define CALIBRATION_SUBDEV_FUNCTIONS_IMX307 get_calibrations_imx307


