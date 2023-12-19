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
#ifdef CONFIG_AMLOGIC_BRIDGE_DSP
#include "bridge_dsp_pcm.h"
#endif
#ifdef CONFIG_AMLOGIC_BRIDGE_UAC
#include "bridge_uac_pcm.h"
#endif
#include "bridge_arm_pcm.h"
#include "bridge_pcm_hal.h"

struct aml_bridge_desc {
	int id;
	char *name;
};

const struct aml_bridge_desc dev_desc[] = {
	{PCM_DEV_TDMA, "TDMA"},
	{PCM_DEV_TDMB, "TMDB"},
	{PCM_DEV_SPDIFIN, "SPDIF"},
	{PCM_DEV_LOOPBACK, "LOOPBACK"},
	{PCM_DEV_PDMIN, "PDM"},
	{PCM_DEV_NONE, "NONE"},
};

const struct aml_bridge_desc mode_desc[] = {
	{PCM_CAPTURE, "IN"},
	{PCM_PLAYBACK, "OUT"},
};

const struct aml_bridge_desc core_desc[] = {
	{PCM_DSP, "DSP"},
	{PCM_ARM, "ARM"},
	{PCM_UAC, "UAC"},
};

char *find_card_desc(enum PCM_CARD dev)
{
	int i;

	for (i = 0; i < sizeof(dev_desc) / sizeof(struct aml_bridge_desc); i++) {
		if (dev_desc[i].id == dev)
			return dev_desc[i].name;
	}
	return NULL;
}

char *find_mode_desc(enum PCM_MODE dev)
{
	int i;

	for (i = 0; i < sizeof(mode_desc) / sizeof(struct aml_bridge_desc); i++) {
		if (mode_desc[i].id == dev)
			return mode_desc[i].name;
	}
	return NULL;
}

char *find_core_desc(enum PCM_CORE dev)
{
	int i;

	for (i = 0; i < sizeof(core_desc) / sizeof(struct aml_bridge_desc); i++) {
		if (core_desc[i].id == dev)
			return core_desc[i].name;
	}
	return NULL;
}

struct audio_pcm_function_t *audio_pcm_alloc(struct device *dev, enum PCM_CORE pcm_core,
				enum PCM_CARD pcm_card, enum PCM_MODE mode)
{
	int ret = 0;
	struct audio_pcm_function_t *audio_pcm = vzalloc(sizeof(*audio_pcm));

	if (!audio_pcm)
		return NULL;
	audio_pcm->dev = dev;
	audio_pcm->coreid = pcm_core;
	audio_pcm->cardid = pcm_card;
	audio_pcm->modeid = mode;
	switch (pcm_core)

	{
		case PCM_DSP:
#ifdef CONFIG_AMLOGIC_BRIDGE_DSP
			ret = dsp_pcm_init(audio_pcm, pcm_card, mode);
#else
			pr_err("bridge dsp core unsupport!");
			ret = -1;
#endif
			break;
		case PCM_ARM:
			//TODO:Not yet implemented
			ret = arm_pcm_init(audio_pcm, pcm_card, mode);
			break;
		case PCM_UAC:
#ifdef CONFIG_AMLOGIC_BRIDGE_UAC
			ret = uac_pcm_init(audio_pcm, mode);
#else
			pr_err("bridge uac core unsupport!");
			ret = -1;
#endif

			break;
		default:
			pr_err("unsupport this core:%d!", pcm_core);
			ret = -1;
			break;
	}
	if (ret < 0) {
		vfree(audio_pcm);
		return NULL;
	}

	return audio_pcm;
}

void audio_pcm_free(struct audio_pcm_function_t *audio_pcm)
{
	switch (audio_pcm->coreid)

	{
		case PCM_DSP:
#ifdef CONFIG_AMLOGIC_BRIDGE_DSP
			dsp_pcm_deinit(audio_pcm);
#endif
			break;
		case PCM_ARM:
			arm_pcm_deinit(audio_pcm);
			break;
		case PCM_UAC:
#ifdef CONFIG_AMLOGIC_BRIDGE_UAC
			uac_pcm_deinit(audio_pcm);
#endif
			break;
		default:
			break;
	}
	vfree(audio_pcm);
}
