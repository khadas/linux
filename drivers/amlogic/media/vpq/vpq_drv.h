/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __VPQ_DRV_H__
#define __VPQ_DRV_H__

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
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/ctype.h>
#include <linux/vmalloc.h>
#include <linux/io.h>
#include <linux/poll.h>
#include <linux/workqueue.h>
#include <linux/sched/clock.h>

#define VPQ_DEVNO       (1)
#define VPQ_NAME        "vpq"
#define VPQ_CLS_NAME    "vpq"

enum vpq_chip_type_e {
	ID_NULL = 0,
	VPQ_CHIP_T5W,
	VPQ_CHIP_T3,
};

struct vpq_match_data_s {
	enum vpq_chip_type_e chip_id;
};

struct vpq_dev_s {
	struct device *dev;
	struct cdev vpq_cdev;
	dev_t devno;
	struct class *clsp;
	struct platform_device *pdev;
	struct vpq_match_data_s *pm_data;

	unsigned int event_info; //0:NONE 1:sig_info_change
	struct delayed_work event_dwork;
	wait_queue_head_t queue;
};

struct vpq_dev_s *get_vpq_dev(void);

#endif //__VPQ_DRV_H__
