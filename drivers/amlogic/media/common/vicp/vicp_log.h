/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * drivers/amlogic/media/video_processor/common/vicp/vicp_log.h
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

#ifndef _VICP_LOG_H_
#define _VICP_LOG_H_

#include <stdarg.h>
#include <linux/printk.h>

#define VICP_ERROR		    0X0
#define VICP_INFO		    0X1
#define VICP_AFBCD		    0X2
#define VICP_RDMIF		    0X4
#define VICP_SCALER		    0X8
#define VICP_HDR2		    0X10
#define VICP_CROP		    0X20
#define VICP_SHRINK		    0X40
#define VICP_WRMIF		    0X80
#define VICP_AFBCE		    0X100
#define VICP_RDMA		    0X200

int vicp_print(int debug_flag, const char *fmt, ...);
#endif
