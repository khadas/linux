// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/arm-smccc.h>
#include <linux/platform_device.h>

#include <linux/amlogic/secmon.h>
#include <linux/amlogic/tee.h>
#include <linux/amlogic/tee_demux.h>
#include <asm/cputype.h>
#include <linux/printk.h>

static void __iomem *dmx_shm_input;
static void __iomem *dmx_shm_output;

static int get_share_memory(void)
{
	if (!dmx_shm_input)
		dmx_shm_input = get_meson_sm_input_base();

	if (!dmx_shm_output)
		dmx_shm_output = get_meson_sm_output_base();

	if (dmx_shm_input && dmx_shm_output)
		return 0;

	return -1;
}

int tee_demux_set(enum tee_dmx_cmd cmd, void *data, u32 len)
{
	struct arm_smccc_res res;

	if (!data || !len || get_share_memory())
		return -1;

	meson_sm_mutex_lock();
	memcpy((void *)dmx_shm_input, (const void *)data, len);
	arm_smccc_smc(0x82000076, cmd, len, 0, 0, 0, 0, 0, &res);
	meson_sm_mutex_unlock();

	return res.a0;
}
EXPORT_SYMBOL(tee_demux_set);

int tee_demux_get(enum tee_dmx_cmd cmd,
				void *in, u32 in_len, void *out, u32 out_len)
{
	struct arm_smccc_res res;

	if (!out || !out_len || get_share_memory())
		return -1;

	meson_sm_mutex_lock();
	if (in && in_len)
		memcpy((void *)dmx_shm_input, (const void *)in, in_len);
	arm_smccc_smc(0x82000076, cmd, in_len, out_len, 0, 0, 0, 0, &res);
	if (!res.a0)
		memcpy((void *)out, (void *)dmx_shm_output, out_len);
	meson_sm_mutex_unlock();

	return res.a0;
}
EXPORT_SYMBOL(tee_demux_get);
