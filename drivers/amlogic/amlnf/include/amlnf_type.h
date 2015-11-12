/*
 * drivers/amlogic/amlnf/include/amlnf_type.h
 *
 * Copyright (C) 2015 Amlogic, Inc. All rights reserved.
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

#ifndef __AML_NF_TYPE_H__
#define __AML_NF_TYPE_H__
#include "amlnf_cfg.h"

#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/clk.h>
#include <linux/slab.h>
#include<linux/delay.h>
#include <linux/cdev.h>
#include <linux/sched.h>
/*
#include <linux/earlysuspend.h>
#include <mach/pinmux.h>
*/
#include <linux/err.h>
/*#include <linux/io.h>*/
#include <linux/bitops.h>
#include <linux/crc32.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/reboot.h>
#include <asm/div64.h>
/*#include <mach/clock.h>*/
#include <linux/list.h>
#include <linux/sizes.h>
/*#include <mach/am_regs.h>*/
#include <linux/kthread.h>
#include <linux/kmod.h>
#include <linux/blkdev.h>
#include <linux/blkpg.h>
#include <linux/freezer.h>
#include <linux/spinlock.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/mutex.h>

#ifdef AML_NAND_RB_IRQ
/*#include <mach/irqs.h>*/
#include <linux/interrupt.h>
#endif

#ifdef AML_NAND_DMA_POLLING
#ifdef CONFIG_HIGH_RES_TIMERS
#include <linux/ktime.h>
#include <linux/hrtimer.h>
#endif
#endif

#undef pr_fmt
#define pr_fmt(fmt) "nandphy: " fmt


#ifdef AML_NAND_DBG

#define aml_nand_dbg(fmt, ...) pr_info("%s: line:%d " fmt "\n", \
		__func__, __LINE__, ##__VA_ARGS__)

#define aml_nand_msg(fmt, ...) pr_info("%s: line:%d " fmt "\n", \
		__func__, __LINE__, ##__VA_ARGS__)
#define nprint(fmt, ...) pr_info(fmt , \
		##__VA_ARGS__)
#else
#define aml_nand_dbg(fmt, ...)
#define aml_nand_msg(fmt, ...) pr_info(fmt "\n",  ##__VA_ARGS__)
#define nprint(fmt, ...) pr_info(fmt,  ##__VA_ARGS__)
#endif


#endif


