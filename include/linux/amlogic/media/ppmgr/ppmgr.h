/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * include/linux/amlogic/media/ppmgr/ppmgr.h
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#ifndef _PPMGR_MAIN_H
#define  _PPMGR_MAIN_H
#include <linux/interrupt.h>
/*#include <mach/am_regs.h>*/
#include <linux/amlogic/media/canvas/canvas.h>
#include <linux/fb.h>
#include <linux/list.h>
#include <linux/uaccess.h>
#include <linux/sysfs.h>
#include  <linux/spinlock.h>
#include <linux/kthread.h>

#define PPMGR_IOC_MAGIC  'P'
#define PPMGR_IOC_2OSD0		_IOW(PPMGR_IOC_MAGIC, 0x00, unsigned int)
#define PPMGR_IOC_ENABLE_PP _IOW(PPMGR_IOC_MAGIC, 0X01, unsigned int)
#define PPMGR_IOC_CONFIG_FRAME  _IOW(PPMGR_IOC_MAGIC, 0X02, unsigned int)
#define PPMGR_IOC_GET_ANGLE  _IOR(PPMGR_IOC_MAGIC, 0X03, unsigned int)
#define PPMGR_IOC_SET_ANGLE  _IOW(PPMGR_IOC_MAGIC, 0X04, unsigned int)

struct frame_info_t {
int width;
int height;
int bpp;
int angle;
int format;
};

/*TV 3D mode*/
#define PPMGR_MODE_3D_ENABLE      0x00000001
#define PPMGR_MODE_AUTO           0x00000002
#define PPMGR_MODE_2D_TO_3D       0x00000004
#define PPMGR_MODE_LR             0x00000008
#define PPMGR_MODE_BT             0x00000010
#define PPMGR_MODE_LR_SWITCH      0x00000020
#define PPMGR_MODE_FIELD_DEPTH    0x00000040
#define PPMGR_MODE_3D_TO_2D_L     0x00000080
#define PPMGR_MODE_3D_TO_2D_R     0x00000100

#define LR_FORMAT_INDICATOR   0x00000200
#define BT_FORMAT_INDICATOR   0x00000400

#define TYPE_NONE           0
#define TYPE_2D_TO_3D       1
#define TYPE_LR             2
#define TYPE_BT             3
#define TYPE_LR_SWITCH      4
#define TYPE_FILED_DEPTH    5
#define TYPE_3D_TO_2D_L     6
#define TYPE_3D_TO_2D_R     7

enum platform_type_t {
PLATFORM_MID = 0, PLATFORM_MBX, PLATFORM_TV, PLATFORM_MID_VERTICAL
};

#endif
