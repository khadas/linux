/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __SYSTEM_LOG_H__
#define __SYSTEM_LOG_H__

//changeable logs
#include <linux/kernel.h>
extern int gdc_log_level;

enum log_level_e {
	LOG_CRIT = 0,
	LOG_ERR = 0,
	LOG_INFO = 0,
	LOG_WARNING,
	LOG_DEBUG,
	LOG_MAX
};

#define gdc_log(level, fmt, ...)                                   \
	do {                                                         \
		if ((level) <= gdc_log_level)                        \
			pr_info("gdc: %s: " fmt, __func__, ##__VA_ARGS__); \
	} while (0)

#endif // __SYSTEM_LOG_H__
