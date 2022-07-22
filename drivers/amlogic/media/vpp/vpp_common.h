/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __VPP_COMMON_H__
#define __VPP_COMMON_H__

/*Standard Linux headers*/
#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/compat.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/errno.h>
#include <linux/uaccess.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/ctype.h>
#include <linux/vmalloc.h>
#include <linux/io.h>
#include <linux/poll.h>
#include <linux/workqueue.h>
#include <linux/sched/clock.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vout/vout_notify.h>

#include "vpp_common_def.h"
#include "vpp_reg_io.h"
#include "vpp_drv.h"

/* Commom define */
#define PR_SYS    (0)
#define PR_IOC    (1)
#define PR_DEBUG  (2)

extern unsigned int pr_lvl;

#define pr_vpp(level, fmt, args ...)\
	do {\
		if ((level) & pr_lvl)\
			pr_info("vpp:" fmt, ##args);\
	} while (0)

#define PR_ERR(fmt, args ...)  pr_info("vpp_err:" fmt, ##args)
#define PR_DRV(fmt, args ...)  pr_info("vpp_drv:" fmt, ##args)

#ifndef MIN
#define MIN(a, b) \
	({typeof(a) x = (a);\
	typeof(b) y = (b);\
	(x < y) ? (x) : (y);\
	})
#endif

#ifndef MAX
#define MAX(a, b) \
	({typeof(a) x = (a);\
	typeof(b) y = (b);\
	(x > y) ? (x) : (y);\
	})
#endif

#ifndef XOR
#define XOR(a, b) \
	({typeof(a) x = (a);\
	typeof(b) y = (b);\
	(y & (~x)) + ((~y) & x);\
	})
#endif

/*Common functions*/
int vpp_check_range(int val, int down, int up);
int vpp_mask_int(int val, int start, int len);
int vpp_insert_int(int src_val, int insert_val, int start, int len);

#endif

