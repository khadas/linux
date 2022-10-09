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

#if !defined(__ACAMERA_ISP_CORE_NOMEM_SETTINGS_H__)
#define __ACAMERA_ISP_CORE_NOMEM_SETTINGS_H__
#define ISP_CORE_ID    0x58F8E91F
#define ISP_CONFIG_PING    1
#define ISP_CONFIG_PONG    0
#define ISP_CONFIG_LUT_OFFSET    0x044B0
#define ISP_CONFIG_PING_OFFSET    0x18E88
#define ISP_CONFIG_PING_SIZE    0x17FC0
#define ISP_PIPE_RAW_BITS    12
#define ISP_PIPE_RAW_WDR_BITS    20
#define ISP_PIPE_SINTER_BITS    16
#define ISP_PIPE_RGB_BITS    10
#define ISP_CONFIG_AW    16
#define ISP_CONFIG_DW    32
#define ISP_DEFECT_PIXEL_AW    12
#define ISP_DEFECT_PIXEL_CXW    13
#define ISP_DEFECT_PIXEL_CYW    12
#define ISP_SHADING_MX    6
#define ISP_SHADING_MY    6
#define ISP_SHADING_MW    8
#define ISP_SHADING_GEOM    13
#define ISP_SHADING_LUT_AW    7
#define ISP_FRONTEND_LUT0_AW    5
#define ISP_FRONTEND_LUT1_AW    8
#define ISP_BACKEND_LUT0_AW    5
#define ISP_BACKEND_LUT1_AW    8
#define ISP_GAMMA_RGB_AW    8
#define ISP_METERING_ZONES_AWB_V    33
#define ISP_METERING_ZONES_AWB_H    33
#define ISP_METERING_ZONES_MAX_V    33
#define ISP_METERING_ZONES_MAX_H    33
#define ISP_METERING_OFFSET_AE    0
#define ISP_METERING_OFFSET_AWB    2192
#define ISP_METERING_OFFSET_AF    4384
#define ISP_METERING_MEM_END    6143
#define ISP_INTERRUPT_EVENT_NONES_COUNT    16
#define ISP_INTERRUPT_EVENT_ISP_START_FRAME_START    0
#define ISP_INTERRUPT_EVENT_ISP_END_FRAME_END    1
#define ISP_INTERRUPT_EVENT_METERING_AF    4
#define ISP_INTERRUPT_EVENT_METERING_AEXP    5
#define ISP_INTERRUPT_EVENT_METERING_AWB    6
#define ISP_INTERRUPT_EVENT_ANTIFOG_HIST    8
#define ISP_INTERRUPT_EVENT_BROKEN_FRAME    3
#define ISP_INTERRUPT_EVENT_MULTICTX_ERROR    2
#define ISP_INTERRUPT_EVENT_DMA_ERROR    22
#define ISP_INTERRUPT_EVENT_WATCHDOG_EXP    19
#define ISP_INTERRUPT_EVENT_FRAME_COLLISION    20
#endif /* __ACAMERA_ISP_CORE_NOMEM_SETTINGS_H__*/
