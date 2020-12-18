/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _TVAFE_DEBUG_H_
#define _TVAFE_DEBUG_H_

#include <stdarg.h>
#include <linux/printk.h>

#undef pr_fmt
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#define tvafe_pr_info(fmt, ...) \
	pr_info(fmt, ##__VA_ARGS__)

#define tvafe_pr_err(fmt, ...) \
	pr_err(fmt, ##__VA_ARGS__)

#define tvafe_pr_warn(fmt, ...) \
	pr_warn(fmt, ##__VA_ARGS__)

#endif
