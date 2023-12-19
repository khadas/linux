// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2023 Amlogic, Inc. All rights reserved.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sysfs.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/kasan.h>
#include <linux/highmem.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/printk.h>
#include <linux/string.h>
#include <linux/arm-smccc.h>
#include <linux/psci.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <uapi/linux/psci.h>
#include <linux/debugfs.h>
#include <linux/sched/signal.h>
#include <linux/dma-mapping.h>
#include <linux/of_reserved_mem.h>
#include <linux/vmalloc.h>
#include <linux/clk.h>
#include <asm/cacheflush.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include "bridge_common.h"
#include "bridge_pcm_hal.h"

static int arm_pcm_start(struct audio_pcm_function_t *audio_pcm)
{
	//TODO:Not yet implemented
	if (!audio_pcm)
		return -1;
	return 0;
}

static int arm_pcm_stop(struct audio_pcm_function_t *audio_pcm)
{
	//TODO:Not yet implemented
	if (!audio_pcm)
		return -1;
	return 0;
}

static int arm_pcm_get_status(struct audio_pcm_function_t *audio_pcm)
{
	//TODO:Not yet implemented
	if (!audio_pcm)
		return 0;
	return 0;
}

static int arm_pcm_set_hw(struct audio_pcm_function_t *audio_pcm, u8 channels, u32 rate, u32 format)
{
	//TODO:Not yet implemented
	if (!audio_pcm)
		return -1;
	return 0;
}

static int arm_pcm_create_attribute(struct audio_pcm_function_t *audio_pcm, struct kobject *kobj)
{
	//TODO:Not yet implemented
	if (!audio_pcm)
		return -1;
	return 0;
}

static void arm_pcm_destroy_attribute(struct audio_pcm_function_t *audio_pcm, struct kobject *kobj)
{
	//TODO:Not yet implemented
	if (!audio_pcm)
		return;
}

int arm_pcm_init(struct audio_pcm_function_t *audio_pcm,
			enum PCM_CARD pcm_card, enum PCM_MODE mode)
{
	//TODO:Not yet implemented
	if (!audio_pcm)
		return -1;
	audio_pcm->start = arm_pcm_start;
	audio_pcm->stop = arm_pcm_stop;
	audio_pcm->set_hw = arm_pcm_set_hw;
	audio_pcm->get_status = arm_pcm_get_status;
	audio_pcm->create_attr = arm_pcm_create_attribute;
	audio_pcm->destroy_attr = arm_pcm_destroy_attribute;

	return 0;
}

void arm_pcm_deinit(struct audio_pcm_function_t *audio_pcm)
{
	//TODO:Not yet implemented
	if (!audio_pcm)
		return;
}
