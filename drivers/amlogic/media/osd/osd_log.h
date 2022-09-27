/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _OSD_LOG_H_
#define _OSD_LOG_H_

#include <stdarg.h>
#include <linux/printk.h>

#define OSD_LOG_LEVEL_NULL   0
#define OSD_LOG_LEVEL_DEBUG  1
#define OSD_LOG_LEVEL_DEBUG2 2
#define OSD_LOG_LEVEL_DEBUG3 3

#define MODULE_BASE   BIT(0)
#define MODULE_RENDER BIT(1)
#define MODULE_FENCE  BIT(2)
#define MODULE_BLEND  BIT(3)
#define MODULE_CURSOR BIT(4)
#define MODULE_VIU2   BIT(5)
#define MODULE_SECURE BIT(6)
#define MODULE_ENCP_STAT  BIT(7)
#define MODULE_2SLICE     BIT(8)

extern unsigned int osd_log_level;
extern unsigned int osd_log_module;

#undef pr_fmt
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#define osd_log_info(fmt, ...) \
	pr_info(fmt, ##__VA_ARGS__)

#define osd_log_err(fmt, ...) \
	pr_err(fmt, ##__VA_ARGS__)

#define osd_log_dbg(moudle, fmt, ...) \
	do { \
		if (osd_log_level >= OSD_LOG_LEVEL_DEBUG) { \
			if ((moudle) & osd_log_module) { \
				pr_info(fmt, ##__VA_ARGS__); \
			} \
		} \
	} while (0)

#define osd_log_dbg2(moudle, fmt, ...) \
	do { \
		if (osd_log_level >= OSD_LOG_LEVEL_DEBUG2) { \
			if ((moudle) & osd_log_module) { \
				pr_info(fmt, ##__VA_ARGS__); \
			} \
		} \
	} while (0)

#define osd_log_dbg3(moudle, fmt, ...) \
	do { \
		if (osd_log_level >= OSD_LOG_LEVEL_DEBUG3) { \
			if ((moudle) & osd_log_module) { \
				pr_info(fmt, ##__VA_ARGS__); \
			} \
		} \
	} while (0)
#endif
