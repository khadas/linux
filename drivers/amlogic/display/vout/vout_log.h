/*
 * Amlogic VOUT Server Module
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

#ifndef _VOUT_LOG_H_
#define _VOUT_LOG_H_

#include <stdarg.h>
#include <linux/printk.h>

#define VOUT_LOG_TAG "[VOUT]"

#define vout_log_info(fmt, ...) \
	pr_info(fmt, ##__VA_ARGS__)

#define vout_log_err(fmt, ...) \
	pr_err(fmt, ##__VA_ARGS__)

#define vout_log_dbg(fmt, ...) \
	pr_warn(fmt, ##__VA_ARGS__)

#endif
