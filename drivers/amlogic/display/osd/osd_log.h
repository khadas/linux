/*
 * Amlogic Frame buffer driver
 *
 * Copyright (C) 2015 Amlogic, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Author: Amlogic Platform-BJ <platform.bj@amlogic.com>
 *
 */

#ifndef _OSD_LOG_H_
#define _OSD_LOG_H_

#include <stdarg.h>
#include <linux/printk.h>

#define OSD_LOG_LEVEL_INFO 0
#define OSD_LOG_LEVEL_DEBUG 1
#define OSD_LOG_LEVEL_ERROR 2

extern unsigned int osd_log_level;

#undef pr_fmt
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#define osd_log_info(fmt, ...) \
	pr_info(fmt, ##__VA_ARGS__)

#define osd_log_err(fmt, ...) \
	pr_err(fmt, ##__VA_ARGS__)

#define osd_log_dbg(fmt, ...) \
	do { \
		if (osd_log_level == OSD_LOG_LEVEL_ERROR) { \
			pr_err(fmt, ##__VA_ARGS__); \
		} else if (osd_log_level == OSD_LOG_LEVEL_DEBUG) { \
			pr_warn(fmt, ##__VA_ARGS__); \
		} else if (osd_log_level == OSD_LOG_LEVEL_INFO) { \
			pr_info(fmt, ##__VA_ARGS__); \
		} \
	} while (0)

#endif
