/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __VPQ_PRINTK_H__
#define __VPQ_PRINTK_H__

#include <linux/kernel.h>

extern unsigned int log_lev;

#define PR_IOCTL	(1)
#define PR_TABLE	(2)
#define PR_MODULE	(3)
#define PR_VFM		(4)
#define PR_PROC		(5)

#define VPQ_PR_INFO(lev, fmt, args...)\
	do {\
		if ((lev) == log_lev)\
			pr_info("vpq_info:" fmt, ## args);\
	} while (0)

#define VPQ_PR_DRV(fmt, args ...)   pr_info("vpq_drv:" fmt, ##args)
#define VPQ_PR_ERR(fmt, args ...)   pr_err("vpq_err:" fmt, ##args)

#endif //__VPQ_PRINTK_H__
