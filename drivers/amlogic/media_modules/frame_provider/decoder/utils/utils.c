/*
 * drivers/amlogic/media/frame_provider/decoder/utils/utils.c
 *
 * Copyright (C) 2016 Amlogic, Inc. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <linux/semaphore.h>
#include <linux/sched/rt.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/slab.h>

#include "vdec.h"
#include "vdec_input.h"
#include <linux/amlogic/media/utils/vdec_reg.h>
#include "amvdec.h"
#include "decoder_mmu_box.h"
#include "decoder_bmmu_box.h"
#include "vdec_profile.h"

static int __init decoder_common_init(void)
{
	/*vdec init.*/
	vdec_module_init();

	/*amvdec init.*/
	amvdec_init();

	/*mmu box init.*/
	decoder_mmu_box_init();/*exit?*/
	decoder_bmmu_box_init();

	vdec_profile_init_debugfs();

	return 0;
}

static void __exit decoder_common_exit(void)
{
	/*vdec exit.*/
	vdec_module_exit();

	/*amvdec exit.*/
	amvdec_exit();

	decoder_mmu_box_exit();
	decoder_bmmu_box_exit();

	vdec_profile_exit_debugfs();
}

module_init(decoder_common_init);
module_exit(decoder_common_exit);
