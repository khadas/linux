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
#define FW_LOG_NAMES {"SUBDEV_IQ"}
#define FW_LOG_REAL_TIME 1
#define ISP_FW_BUILD 0
#define ISP_WDR_DEFAULT_MODE WDR_MODE_LINEAR
#define KERNEL_MODULE 1
#define LOG2_GAIN_SHIFT 18
#define LOG_MODULE_ALL 1
#define LOG_MODULE_GENERIC 0
#define LOG_MODULE_GENERIC_MASK 1
#define LOG_MODULE_MAX 1
#define V4L2_INTERFACE_BUILD 1
#endif
