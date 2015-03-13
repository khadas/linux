/*
 * Amlogic GE2D driver
 *
 * Copyright (C) 2015 Amlogic, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Author: Platform-BJ <platform.bj@amlogic.com>
 *
 */

#ifndef _GE2D_LOG_H_
#define _GE2D_LOG_H_

#include <stdarg.h>
#include <linux/printk.h>

#define GE2D_LOG_TAG "[GE2D]"
#define GE2D_LOG_LEVEL_NULL 0
#define GE2D_LOG_LEVEL_DEBUG 1

extern unsigned int ge2d_log_level;
#define ge2d_log_info(fmt, ...) \
	pr_info(fmt, ##__VA_ARGS__)

#define ge2d_log_err(fmt, ...) \
	pr_err(fmt, ##__VA_ARGS__)

#define ge2d_log_dbg(fmt, ...) \
	do { \
		if (ge2d_log_level == GE2D_LOG_LEVEL_DEBUG) { \
			pr_info(fmt, ##__VA_ARGS__); \
		} \
	} while (0)

#endif
