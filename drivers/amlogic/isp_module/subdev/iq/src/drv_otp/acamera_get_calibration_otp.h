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

#define GOT_LINEAR_IQT    0x10
#define GOT_FS_LIN_IQT    0x20
#define GOT_LINEAR_OTP    0x11
#define GOT_FS_LIN_OTP    0x22
#define OPT_UNFINISH      0x03

extern int otp_enable;

extern int32_t acamera_calibration_os08a10_otp( ACameraCalibrations *c );
extern uint32_t get_calibratin_os08a10_otp( uint32_t ctx_num,void * sensor_arg,ACameraCalibrations *) ;
#define CALIBRATION_SUBDEV_FUNCTIONS_OS08A10_OTP get_calibratin_os08a10_otp

extern int32_t acamera_calibration_imx227_otp( ACameraCalibrations *c );
extern uint32_t get_calibratin_imx227_otp( uint32_t ctx_num,void * sensor_arg,ACameraCalibrations *) ;
#define CALIBRATION_SUBDEV_FUNCTIONS_IMX227_OTP get_calibratin_imx227_otp

extern int32_t acamera_calibration_ov5675_otp( ACameraCalibrations *c );
extern uint32_t get_calibratin_ov5675_otp( uint32_t ctx_num,void * sensor_arg,ACameraCalibrations *) ;
#define CALIBRATION_SUBDEV_FUNCTIONS_OV5675_OTP get_calibratin_ov5675_otp


