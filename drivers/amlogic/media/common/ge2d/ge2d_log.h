/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _GE2D_LOG_H_
#define _GE2D_LOG_H_

#include <stdarg.h>
#include <linux/printk.h>

#define GE2D_LOG_TAG "[GE2D]"
#define GE2D_LOG_LEVEL_NULL 0
#define GE2D_LOG_LEVEL_DEBUG 1
#define GE2D_LOG_LEVEL_DEBUG2 2
#define GE2D_LOG_DUMP_CMD_QUEUE_REGS 0x100
#define GE2D_LOG_DUMP_STACK 0x200

extern unsigned int ge2d_log_level;

#undef pr_fmt
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt
#define ge2d_log_info(fmt, ...) \
	pr_info(fmt, ##__VA_ARGS__)

#define ge2d_log_err(fmt, ...) \
	pr_err(fmt, ##__VA_ARGS__)

#define ge2d_log_dbg(fmt, ...) \
	do { \
		if ((ge2d_log_level & 0xff) >= GE2D_LOG_LEVEL_DEBUG) { \
			pr_info(fmt, ##__VA_ARGS__); \
		} \
	} while (0)

#define ge2d_log_dbg2(fmt, ...) \
	do { \
		if ((ge2d_log_level & 0xff) >= GE2D_LOG_LEVEL_DEBUG2) { \
			pr_info(fmt, ##__VA_ARGS__); \
		} \
	} while (0)

#endif
