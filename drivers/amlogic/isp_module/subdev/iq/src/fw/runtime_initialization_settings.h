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
extern uint32_t get_calibrations_imx227( uint32_t ctx_num, void * sensor_arg, ACameraCalibrations *) ;
extern uint32_t get_calibrations_imx290( uint32_t ctx_num, void * sensor_arg, ACameraCalibrations *) ;
extern uint32_t get_calibrations_imx290_slt( uint32_t ctx_num, void * sensor_arg, ACameraCalibrations *) ;
extern uint32_t get_calibrations_imx290_lens_8mm( uint32_t ctx_num, void * sensor_arg, ACameraCalibrations *) ;
extern uint32_t get_calibrations_imx290_lens_4mm( uint32_t ctx_num, void * sensor_arg, ACameraCalibrations *) ;
extern uint32_t get_calibrations_os08a10_ipc( uint32_t ctx_num, void * sensor_arg, ACameraCalibrations *) ;
extern uint32_t get_calibrations_os08a10_slt( uint32_t ctx_num, void * sensor_arg, ACameraCalibrations *) ;
extern uint32_t get_calibrations_os08a10_tv( uint32_t ctx_num, void * sensor_arg, ACameraCalibrations *) ;
extern uint32_t get_calibrations_os08a10_panel( uint32_t ctx_num, void * sensor_arg, ACameraCalibrations *) ;
extern uint32_t get_calibrations_imx481( uint32_t ctx_num, void * sensor_arg, ACameraCalibrations *) ;
extern uint32_t get_calibrations_imx307_demo( uint32_t ctx_num, void * sensor_arg, ACameraCalibrations *) ;
extern uint32_t get_calibrations_imx307( uint32_t ctx_num, void * sensor_arg, ACameraCalibrations *) ;
extern uint32_t get_calibrations_imx224( uint32_t ctx_num, void * sensor_arg, ACameraCalibrations *) ;
extern uint32_t get_calibrations_imx335( uint32_t ctx_num, void * sensor_arg, ACameraCalibrations *) ;
extern uint32_t get_calibrations_imx415( uint32_t ctx_num, void * sensor_arg, ACameraCalibrations *) ;
extern uint32_t get_calibrations_ov13858( uint32_t ctx_num, void * sensor_arg, ACameraCalibrations *) ;
extern uint32_t get_calibrations_sc2232h( uint32_t ctx_num, void * sensor_arg, ACameraCalibrations *) ;
extern uint32_t get_calibrations_sc4238( uint32_t ctx_num, void * sensor_arg, ACameraCalibrations *) ;
extern uint32_t get_calibrations_sc2335( uint32_t ctx_num, void * sensor_arg, ACameraCalibrations *) ;
extern uint32_t get_calibrations_imx334( uint32_t ctx_num, void * sensor_arg, ACameraCalibrations *) ;
extern uint32_t get_calibrations_cs8328cs_tv( uint32_t ctx_num, void * sensor_arg, ACameraCalibrations *) ;
extern uint32_t get_calibrations_ov2718( uint32_t ctx_num, void * sensor_arg, ACameraCalibrations *) ;
extern uint32_t get_calibrations_os04a10_tv( uint32_t ctx_num, void * sensor_arg, ACameraCalibrations *) ;
extern uint32_t get_calibrations_ov5675( uint32_t ctx_num, void * sensor_arg, ACameraCalibrations *) ;

#define SENSOR_INIT_SUBDEV_FUNCTIONS  sensor_init_dummy
#define SENSOR_DEINIT_SUBDEV_FUNCTIONS  sensor_deinit_dummy
#define CALIBRATION_SUBDEV_FUNCTIONS_IMX227  get_calibrations_imx227
#define CALIBRATION_SUBDEV_FUNCTIONS_IMX290  get_calibrations_imx290
#define CALIBRATION_SUBDEV_FUNCTIONS_IMX290_SLT  get_calibrations_imx290_slt
#define CALIBRATION_SUBDEV_FUNCTIONS_IMX290_LENS_8mm  get_calibrations_imx290_lens_8mm
#define CALIBRATION_SUBDEV_FUNCTIONS_IMX290_LENS_4mm  get_calibrations_imx290_lens_4mm
#define CALIBRATION_SUBDEV_FUNCTIONS_OS08A10  get_calibrations_os08a10_ipc
#define CALIBRATION_SUBDEV_FUNCTIONS_OS08A10_IPC  get_calibrations_os08a10_ipc
#define CALIBRATION_SUBDEV_FUNCTIONS_OS08A10_SLT  get_calibrations_os08a10_slt
#define CALIBRATION_SUBDEV_FUNCTIONS_OS08A10_TV  get_calibrations_os08a10_tv
#define CALIBRATION_SUBDEV_FUNCTIONS_OS08A10_PANEL  get_calibrations_os08a10_panel
#define CALIBRATION_SUBDEV_FUNCTIONS_IMX481  get_calibrations_imx481
#define CALIBRATION_SUBDEV_FUNCTIONS_IMX307_DEMO get_calibrations_imx307_demo
#define CALIBRATION_SUBDEV_FUNCTIONS_IMX307  get_calibrations_imx307
#define CALIBRATION_SUBDEV_FUNCTIONS_IMX224  get_calibrations_imx224
#define CALIBRATION_SUBDEV_FUNCTIONS_IMX335  get_calibrations_imx335
#define CALIBRATION_SUBDEV_FUNCTIONS_IMX415  get_calibrations_imx415
#define CALIBRATION_SUBDEV_FUNCTIONS_OV13858  get_calibrations_ov13858
#define CALIBRATION_SUBDEV_FUNCTIONS_SC2232H  get_calibrations_sc2232h
#define CALIBRATION_SUBDEV_FUNCTIONS_SC4238 get_calibrations_sc4238
#define CALIBRATION_SUBDEV_FUNCTIONS_SC2335 get_calibrations_sc2335
#define CALIBRATION_SUBDEV_FUNCTIONS_IMX334 get_calibrations_imx334
#define CALIBRATION_SUBDEV_FUNCTIONS_SC8238CS get_calibrations_cs8328cs_tv
#define CALIBRATION_SUBDEV_FUNCTIONS_OV2718 get_calibrations_ov2718
#define CALIBRATION_SUBDEV_FUNCTIONS_OS04A10_TV  get_calibrations_os04a10_tv
#define CALIBRATION_SUBDEV_FUNCTIONS_OV5675 get_calibrations_ov5675

extern uint32_t get_calibratin_os08a10_otp( uint32_t ctx_num,void * sensor_arg,ACameraCalibrations *) ;

#define CALIBRATION_SUBDEV_FUNCTIONS_OS08A10_OTP get_calibratin_os08a10_otp
