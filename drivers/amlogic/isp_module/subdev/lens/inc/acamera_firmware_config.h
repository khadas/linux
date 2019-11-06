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

#ifndef __ACAMERA_FIRMWARE_CONFIG_H__
#define __ACAMERA_FIRMWARE_CONFIG_H__


#define FIRMWARE_CONTEXT_NUMBER 1
#define FSM_NAMES {"GENERIC",}
#define FW_LOG_FROM_ISR 0
#define FW_LOG_HAS_SRC 1
#define FW_LOG_LEVEL LOG_NOTHING
#define FW_LOG_MASK (1<<(FW_LOG_MODULE_MAX-1))
#define FW_LOG_MODULE_MAX 1
#define FW_LOG_NAMES {"SUBDEV_SENSOR"}
#define FW_LOG_REAL_TIME 1
#define ISP_FW_BUILD 0
#define ISP_HAS_AF 1
#define ISP_MAX_SENSOR_MODES 8
#define ISP_SENSOR_DRIVER_AD5821 0
#define ISP_SENSOR_DRIVER_AN41908A 0
#define ISP_SENSOR_DRIVER_BU64748 0
#define ISP_SENSOR_DRIVER_DONGWOON 0
#define ISP_SENSOR_DRIVER_DW9800 0
#define ISP_SENSOR_DRIVER_FP5510A 0
#define ISP_SENSOR_DRIVER_LC898201 0
#define ISP_SENSOR_DRIVER_MODEL 0
#define ISP_SENSOR_DRIVER_NULL 1
#define ISP_SENSOR_DRIVER_ROHM 0
#define ISP_SENSOR_DRIVER_V4L2 0
#define JUNO_DIRECT_DDR_ACCESS 0
#define KERNEL_MODULE 1
#define LOG2_GAIN_SHIFT 18
#define LOG_MODULE_ALL 1
#define LOG_MODULE_GENERIC 0
#define LOG_MODULE_GENERIC_MASK 1
#define LOG_MODULE_MAX 1
#define SENSOR_BINARY_SEQUENCE 0
#define SENSOR_DEFAULT_PRESET_MODE 0
#define SENSOR_HW_INTERFACE ACameraDefault
#define V4L2_FRAME_ID_SYNC 1
#define V4L2_INTERFACE_BUILD 1
#define V4L2_RUNNING_ON_JUNO 0
#endif
