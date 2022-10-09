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

#if !defined( __ACAMERA_AUTOCAPTURE_H__ )
#define __ACAMERA_AUTOCAPTURE_H__

#define OUTPUT_FORMAT_DISABLE      0
#define OUTPUT_FORMAT_RGB32        1
#define OUTPUT_FORMAT_A2R10G10B10  2
#define OUTPUT_FORMAT_RGB565       3
#define OUTPUT_FORMAT_RGB24        4
#define OUTPUT_FORMAT_GEN32        5
#define OUTPUT_FORMAT_RAW16        6
#define OUTPUT_FORMAT_RAW12        7
#define OUTPUT_FORMAT_AYUV         8
#define OUTPUT_FORMAT_Y410         9
#define OUTPUT_FORMAT_YUY2         10
#define OUTPUT_FORMAT_UYVY         11
#define OUTPUT_FORMAT_Y210         12
#define OUTPUT_FORMAT_NV12_Y       13
#define OUTPUT_FORMAT_NV12_UV      (1<<6|13)
#define OUTPUT_FORMAT_NV12_VU      (2<<6|13)
#define OUTPUT_FORMAT_YV12_Y       14
#define OUTPUT_FORMAT_YV12_U       (1<<6|14)
#define OUTPUT_FORMAT_YV12_V       (2<<6|14)

#define GET_FR  	0
#define GET_DS1 	1
#define GET_SC0 	2

#define IOCTL_FREE_MEM         0x101
#define IOCTL_GET_FR_INFO      0x102
#define IOCTL_GET_DS1_INFO     0x103
#define IOCTL_GET_SC0_INFO     0x104
#define IOCTL_STOP_AUTO        0x105
#define IOCTL_STOP_AUTO_FR     0x106
#define IOCTL_STOP_AUTO_DS1    0x107
#define IOCTL_STOP_AUTO_SC0    0x108
#define IOCTL_DBGPATH_INFO     0x109
#define IOCTL_GET_FRAME_INFO   0x110
#define IOCTL_GET_FR_RTL_INFO   0x111
#define IOCTL_GET_DS1_RTL_INFO  0x112
#define IOCTL_GET_SC0_RTL_INFO  0x113

struct frame_info {
          uint32_t type;
          uint32_t index;
          uint32_t framesize;
          uint32_t phy_addr;
};

struct autocap_image {
          uint32_t format;
          uint32_t fps;
          uint32_t imagesize;
          uint32_t imagebufferstride;
          uint32_t count;
          uint32_t memory_size;
          uint32_t realcount;
          uint32_t endtime;
          uint32_t s_address;
          uint32_t n_address;
};

#endif /* __ACAMERA_AUTOCAPTURE_H__ */
