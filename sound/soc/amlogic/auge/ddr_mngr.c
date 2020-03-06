/*
 * sound/soc/amlogic/auge/ddr_mngr.c
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
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
#define DEBUG
#undef pr_fmt
#define pr_fmt(fmt) "audio_ddr_mngr: " fmt

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/of_device.h>

#include <linux/notifier.h>
#include <linux/suspend.h>

#include "regs.h"
#include "ddr_mngr.h"

#include "resample.h"
#include "resample_hw.h"
#include "effects_hw.h"
#include "effects_hw_v2.h"
#include "effects_v2.h"
#include "pwrdet_hw.h"
#include "vad.h"

#define DRV_NAME "audio-ddr-manager"

static DEFINE_MUTEX(ddr_mutex);

#define DDRMAX 4
static struct frddr frddrs[DDRMAX];
static struct toddr toddrs[DDRMAX];

/* resample */
static struct toddr_attach attach_resample_a;
static struct toddr_attach attach_resample_b;
static void aml_check_resample(struct toddr *to, bool enable);

/* power detect */
static struct toddr_attach attach_pwrdet;
static void aml_check_pwrdet(bool enable);
static bool aml_check_pwrdet_module(int src);

/* VAD */
static struct toddr_attach attach_vad;
static void aml_check_vad(struct toddr *to, bool enable);


/* Audio EQ DRC */
static struct frddr_attach attach_aed;

static irqreturn_t aml_ddr_isr(int irq, void *devid)
{
	(void)devid;
	return IRQ_WAKE_THREAD;
}

/* to DDRS */
static struct toddr *register_toddr_l(struct device *dev,
	struct aml_audio_controller *actrl,
	irq_handler_t handler, void *data)
{
	struct toddr *to;
	unsigned int mask_bit;
	int i, ret;

	/* lookup unused toddr */
	for (i = 0; i < DDRMAX; i++) {
		if (!toddrs[i].in_use)
			break;
	}

	if (i >= DDRMAX)
		return NULL;

	to = &toddrs[i];

	/* irqs request */
	ret = request_threaded_irq(to->irq, aml_ddr_isr, handler,
		IRQF_SHARED, dev_name(dev), data);
	if (ret) {
		dev_err(dev, "failed to claim irq %u\n", to->irq);
		return NULL;
	}
	/* enable audio ddr arb */
	mask_bit = i;
	/*aml_audiobus_update_bits(actrl, EE_AUDIO_ARB_CTRL,*/
	/*		(1 << 31)|(1 << mask_bit),*/
	/*		(1 << 31)|(1 << mask_bit));*/

	to->dev = dev;
	to->in_use = true;
	pr_debug("toddrs[%d] registered by device %s\n", i, dev_name(dev));
	return to;
}

static int unregister_toddr_l(struct device *dev, void *data)
{
	struct toddr *to;
	struct aml_audio_controller *actrl;
	unsigned int mask_bit;
	unsigned int value;
	int i;

	if (dev == NULL)
		return -EINVAL;

	for (i = 0; i < DDRMAX; i++) {
		if ((toddrs[i].dev) == dev && toddrs[i].in_use)
			break;
	}

	if (i >= DDRMAX)
		return -EINVAL;

	to = &toddrs[i];

	/* disable audio ddr arb */
	mask_bit = i;
	actrl = to->actrl;
	/*aml_audiobus_update_bits(actrl, EE_AUDIO_ARB_CTRL,*/
	/*		1 << mask_bit, 0 << mask_bit);*/

	/* no ddr active, disable arb switch */
	value = aml_audiobus_read(actrl, EE_AUDIO_ARB_CTRL) & 0x77;
	/*if (value == 0)*/
	/*	aml_audiobus_update_bits(actrl, EE_AUDIO_ARB_CTRL,*/
	/*			1 << 31, 0 << 31);*/

	free_irq(to->irq, data);
	to->dev = NULL;
	to->in_use = false;
	pr_debug("toddrs[%d] released by device %s\n", i, dev_name(dev));

	return 0;
}

int fetch_toddr_index_by_src(int toddr_src)
{
	int i;

	for (i = 0; i < DDRMAX; i++) {
		if (toddrs[i].in_use
			&& (toddrs[i].src == toddr_src)) {
			return i;
		}
	}

	return -1;
}

struct toddr *fetch_toddr_by_src(int toddr_src)
{
	int i;

	for (i = 0; i < DDRMAX; i++) {
		if (toddrs[i].in_use
			&& (toddrs[i].src == toddr_src)) {
			return &toddrs[i];
		}
	}

	return NULL;
}

struct toddr *aml_audio_register_toddr(struct device *dev,
	struct aml_audio_controller *actrl,
	irq_handler_t handler, void *data)
{
	struct toddr *to = NULL;

	mutex_lock(&ddr_mutex);
	to = register_toddr_l(dev, actrl,
		handler, data);
	mutex_unlock(&ddr_mutex);
	return to;
}

int aml_audio_unregister_toddr(struct device *dev, void *data)
{
	int ret;

	mutex_lock(&ddr_mutex);
	ret = unregister_toddr_l(dev, data);
	mutex_unlock(&ddr_mutex);
	return ret;
}

void audio_toddr_irq_enable(struct toddr *to, bool en)
{
	if (!to || !to->in_use || to->irq < 0)
		return;

	mutex_lock(&ddr_mutex);
	if (en)
		enable_irq(to->irq);
	else
		disable_irq_nosync(to->irq);
	mutex_unlock(&ddr_mutex);
}

static inline unsigned int
	calc_toddr_address(unsigned int reg, unsigned int base)
{
	return base + reg - EE_AUDIO_TODDR_A_CTRL0;
}

int aml_toddr_set_buf(struct toddr *to, unsigned int start,
			unsigned int end)
{
	struct aml_audio_controller *actrl = to->actrl;
	unsigned int reg_base = to->reg_base;
	unsigned int reg;

	to->start_addr = start;
	to->end_addr   = end;

	reg = calc_toddr_address(EE_AUDIO_TODDR_A_START_ADDR, reg_base);
	aml_audiobus_write(actrl, reg, start);
	reg = calc_toddr_address(EE_AUDIO_TODDR_A_FINISH_ADDR, reg_base);
	aml_audiobus_write(actrl, reg, end);

	/* int address */
	if (to->chipinfo
		&& (!to->chipinfo->int_start_same_addr)) {
		reg = calc_toddr_address(EE_AUDIO_TODDR_A_INIT_ADDR, reg_base);
		aml_audiobus_write(actrl, reg, start);
	}

	return 0;
}

int aml_toddr_set_buf_startaddr(struct toddr *to, unsigned int start)
{
	struct aml_audio_controller *actrl = to->actrl;
	unsigned int reg_base = to->reg_base;
	unsigned int reg;

	to->start_addr = start;

	reg = calc_toddr_address(EE_AUDIO_TODDR_A_START_ADDR, reg_base);
	aml_audiobus_write(actrl, reg, start);

	/* int address */
	if (to->chipinfo
		&& (!to->chipinfo->int_start_same_addr)) {
		reg = calc_toddr_address(EE_AUDIO_TODDR_A_INIT_ADDR, reg_base);
		aml_audiobus_write(actrl, reg, start);
	}

	return 0;
}

int aml_toddr_set_buf_endaddr(struct toddr *to, unsigned int end)
{
	struct aml_audio_controller *actrl = to->actrl;
	unsigned int reg_base = to->reg_base;
	unsigned int reg;

	to->end_addr   = end;

	reg = calc_toddr_address(EE_AUDIO_TODDR_A_FINISH_ADDR, reg_base);
	aml_audiobus_write(actrl, reg, end);

	return 0;
}

int aml_toddr_set_intrpt(struct toddr *to, unsigned int intrpt)
{
	struct aml_audio_controller *actrl = to->actrl;
	unsigned int reg_base = to->reg_base;
	unsigned int reg;

	reg = calc_toddr_address(EE_AUDIO_TODDR_A_INT_ADDR, reg_base);
	aml_audiobus_write(actrl, reg, intrpt);
	reg = calc_toddr_address(EE_AUDIO_TODDR_A_CTRL0, reg_base);
	aml_audiobus_update_bits(actrl, reg, 0xff << 16, 0x34 << 16);

	return 0;
}

unsigned int aml_toddr_get_position(struct toddr *to)
{
	return aml_toddr_read_status2(to);
}

unsigned int aml_toddr_get_addr(struct toddr *to, enum status_sel sel)
{
	struct aml_audio_controller *actrl = to->actrl;
	unsigned int reg_base = to->reg_base;
	unsigned int reg_sel, reg, addr;

	reg_sel = calc_toddr_address(EE_AUDIO_TODDR_A_CTRL1, reg_base);
	aml_audiobus_update_bits(actrl, reg_sel,
		0xf << 8,
		sel << 8);

	reg = calc_toddr_address(EE_AUDIO_TODDR_A_STATUS2, reg_base);
	addr = aml_audiobus_read(actrl, reg);

	if (sel == VAD_WAKEUP_ADDR) {
		/* clear VAD addr/cnt */
		reg = calc_toddr_address(EE_AUDIO_TODDR_A_CTRL0, reg_base);
		aml_audiobus_update_bits(actrl, reg, 0x1 << 1, 0x1 << 1);
	}

	/* reset to default, current write addr */
	aml_audiobus_update_bits(actrl, reg_sel,
		0xf << 8,
		0x2 << 8);

	return addr;
}

void aml_toddr_enable(struct toddr *to, bool enable)
{
	struct aml_audio_controller *actrl = to->actrl;
	unsigned int reg_base = to->reg_base;
	unsigned int reg;

	reg = calc_toddr_address(EE_AUDIO_TODDR_A_CTRL0, reg_base);
	aml_audiobus_update_bits(actrl,	reg, 1<<31, enable<<31);

	/* check resample */
	aml_check_resample(to, enable);

	if (to->chipinfo
		&& to->chipinfo->wakeup) {
		if (to->chipinfo->wakeup == 1) {
			/* check power detect */
			if (aml_check_pwrdet_module(to->src))
				aml_check_pwrdet(enable);
		} else if (to->chipinfo->wakeup == 2)
			/* check VAD */
			aml_check_vad(to, enable);
	}

	if (!enable)
		aml_audiobus_write(actrl, reg, 0x0);
}

void aml_toddr_select_src(struct toddr *to, enum toddr_src src)
{
	struct aml_audio_controller *actrl = to->actrl;
	unsigned int reg_base = to->reg_base;
	unsigned int reg;

	/* store to check toddr num */
	to->src = src;

	if (to->chipinfo
		&& to->chipinfo->src_sel_ctrl) {
		reg = calc_toddr_address(EE_AUDIO_TODDR_A_CTRL1, reg_base);
		aml_audiobus_update_bits(actrl, reg,
			0xf << 28,
			(src & 0xf) << 28);
	} else {
		reg = calc_toddr_address(EE_AUDIO_TODDR_A_CTRL0, reg_base);
		aml_audiobus_update_bits(actrl,	reg, 0x7, src & 0x7);
	}
}

void aml_toddr_set_fifos(struct toddr *to, unsigned int threshold)
{
	struct aml_audio_controller *actrl = to->actrl;
	unsigned int reg_base = to->reg_base;
	unsigned int reg, mask, val;

	if (threshold < FIFO_BURST) {
		pr_warn("%s, please check threshold:%d less than burst\n",
			__func__, threshold);
		threshold = FIFO_BURST;
	}

	to->threshold = threshold;

	/*
	 * the threshold in bytes, register value is:
	 * val = (threshold / burst) - 1
	 */
	threshold /= FIFO_BURST;
	reg = calc_toddr_address(EE_AUDIO_TODDR_A_CTRL1, reg_base);

	if (to->chipinfo
			&& to->chipinfo->src_sel_ctrl) {
		mask = 0xfff << 12 | 0xf << 8;
		val = (threshold - 1) << 12 | 2 << 8;
	} else {
		mask = 0xff << 16 | 0xf << 8;
		val = (threshold - 1) << 16 | 2 << 8;
	}

	aml_audiobus_update_bits(actrl, reg, mask, val);

	if (to->chipinfo && to->chipinfo->ugt) {
		reg = calc_toddr_address(EE_AUDIO_TODDR_A_CTRL0, reg_base);
		aml_audiobus_update_bits(actrl, reg, 0x1, 0x1);
	}
}

void aml_toddr_force_finish(struct toddr *to)
{
	struct aml_audio_controller *actrl = to->actrl;
	unsigned int reg_base = to->reg_base;
	unsigned int reg;

	reg = calc_toddr_address(EE_AUDIO_TODDR_A_CTRL1, reg_base);
	aml_audiobus_update_bits(actrl, reg, 1 << 25, 1 << 25);
	aml_audiobus_update_bits(actrl, reg, 1 << 25, 0 << 25);
}

void aml_toddr_set_format(struct toddr *to, struct toddr_fmt *fmt)
{
	struct aml_audio_controller *actrl = to->actrl;
	unsigned int reg_base = to->reg_base;
	unsigned int reg;

	to->bitdepth = fmt->bit_depth;
	to->channels = fmt->ch_num;
	to->rate     = fmt->rate;

	reg = calc_toddr_address(EE_AUDIO_TODDR_A_CTRL0, reg_base);
	aml_audiobus_update_bits(actrl, reg,
		0x1 << 27 | 0x7 << 24 | 0x1fff << 3,
		0x1 << 27 | fmt->endian << 24 | fmt->type << 13 |
		fmt->msb << 8 | fmt->lsb << 3);
}

unsigned int aml_toddr_get_status(struct toddr *to)
{
	struct aml_audio_controller *actrl = to->actrl;
	unsigned int reg_base = to->reg_base;
	unsigned int reg;

	reg = calc_toddr_address(EE_AUDIO_TODDR_A_STATUS1, reg_base);

	return aml_audiobus_read(actrl, reg);
}

unsigned int aml_toddr_get_fifo_cnt(struct toddr *to)
{
	return (aml_toddr_get_status(to) & TODDR_FIFO_CNT) >> 8;
}

void aml_toddr_ack_irq(struct toddr *to, int status)
{
	struct aml_audio_controller *actrl = to->actrl;
	unsigned int reg_base = to->reg_base;
	unsigned int reg;

	reg = calc_toddr_address(EE_AUDIO_TODDR_A_CTRL1, reg_base);

	aml_audiobus_update_bits(actrl, reg, MEMIF_INT_MASK, status);
	aml_audiobus_update_bits(actrl, reg, MEMIF_INT_MASK, 0);
}

void aml_toddr_insert_chanum(struct toddr *to)
{
	struct aml_audio_controller *actrl = to->actrl;
	unsigned int reg_base = to->reg_base;
	unsigned int reg;

	reg = calc_toddr_address(EE_AUDIO_TODDR_A_CTRL1, reg_base);
	aml_audiobus_update_bits(actrl, reg, 1 << 24, 1 << 24);
}

unsigned int aml_toddr_read(struct toddr *to)
{
	struct aml_audio_controller *actrl = to->actrl;
	unsigned int reg_base = to->reg_base;
	unsigned int reg;

	reg = calc_toddr_address(EE_AUDIO_TODDR_A_CTRL0, reg_base);

	return aml_audiobus_read(actrl, reg);
}

void aml_toddr_write(struct toddr *to, unsigned int val)
{
	struct aml_audio_controller *actrl = to->actrl;
	unsigned int reg_base = to->reg_base;
	unsigned int reg;

	reg = calc_toddr_address(EE_AUDIO_TODDR_A_CTRL0, reg_base);

	aml_audiobus_write(actrl, reg, val);
}

unsigned int aml_toddr_read1(struct toddr *to)
{
	struct aml_audio_controller *actrl = to->actrl;
	unsigned int reg_base = to->reg_base;
	unsigned int reg;

	reg = calc_toddr_address(EE_AUDIO_TODDR_A_CTRL1, reg_base);

	return aml_audiobus_read(actrl, reg);
}

void aml_toddr_write1(struct toddr *to, unsigned int val)
{
	struct aml_audio_controller *actrl = to->actrl;
	unsigned int reg_base = to->reg_base;
	unsigned int reg;

	reg = calc_toddr_address(EE_AUDIO_TODDR_A_CTRL1, reg_base);

	aml_audiobus_write(actrl, reg, val);
}

unsigned int aml_toddr_read_status2(struct toddr *to)
{
	struct aml_audio_controller *actrl = to->actrl;
	unsigned int reg_base = to->reg_base;
	unsigned int reg;

	reg = calc_toddr_address(EE_AUDIO_TODDR_A_STATUS2, reg_base);

	return aml_audiobus_read(actrl, reg);
}

bool aml_toddr_burst_finished(struct toddr *to)
{
	unsigned int addr_request, addr_reply, i = 0;
	struct aml_audio_controller *actrl = to->actrl;
	unsigned int reg_base = to->reg_base;
	unsigned int reg;
	bool fifo_stop = false;

	/* This is a SW workaround.
	 * If not wait until the fifo stops,
	 * DDR will stuck and could not recover unless reboot.
	 */
	for (i = 0; i < 10; i++) {
		unsigned int cnt0, cnt1, cnt2;

		cnt0 = aml_toddr_get_fifo_cnt(to);
		udelay(10);
		cnt1 = aml_toddr_get_fifo_cnt(to);
		udelay(10);
		cnt2 = aml_toddr_get_fifo_cnt(to);
		pr_debug("i: %d, fifo cnt:[%d] cnt1:[%d] cnt2:[%d]\n",
			i, cnt0, cnt1, cnt2);

		/* fifo stopped */
		if ((cnt0 == cnt1) && (cnt0 == cnt2) && (cnt0 < (0x40 - 2))) {
			pr_debug("%s(), i (%d) cnt(%d) break out\n",
				__func__, i, cnt2);
			fifo_stop = true;
			break;
		}
	}

	/* max 200us delay */
	for (i = 0; i < 200; i++) {
		reg = calc_toddr_address(EE_AUDIO_TODDR_A_CTRL1, reg_base);
		aml_audiobus_update_bits(actrl,	reg, 0xf << 8, 0x0 << 8);
		addr_request = aml_toddr_get_position(to);

		reg = calc_toddr_address(EE_AUDIO_TODDR_A_CTRL1, reg_base);
		aml_audiobus_update_bits(actrl,	reg, 0xf << 8, 0x2 << 8);
		addr_reply = aml_toddr_get_position(to);

		if (addr_request == addr_reply) {
			pr_debug("%s(), fifo_stop %d\n", __func__, fifo_stop);
			return true;
		}

		udelay(1);
		if ((i % 20) == 0)
			pr_info("delay:[%dus]; FRDDR_STATUS2: [0x%x] [0x%x]\n",
				i, addr_request, addr_reply);
	}
	pr_err("Error: 200us time out, TODDR_STATUS2: [0x%x] [0x%x]\n",
				addr_request, addr_reply);
	return false;
}

/* not for tl1 */
static void aml_toddr_set_resample(struct toddr *to, bool enable)
{
	struct aml_audio_controller *actrl = to->actrl;
	unsigned int reg_base = to->reg_base;
	unsigned int reg;

	reg = calc_toddr_address(EE_AUDIO_TODDR_A_CTRL0, reg_base);
	aml_audiobus_update_bits(actrl,	reg, 1<<30, !!enable<<30);
}

/* tl1 after */
static void aml_toddr_set_resample_ab(struct toddr *to,
		enum resample_idx index, bool enable)
{
	struct aml_audio_controller *actrl = to->actrl;
	unsigned int reg_base = to->reg_base;
	unsigned int reg;

	reg = calc_toddr_address(EE_AUDIO_TODDR_A_CTRL1, reg_base);
	if (index == RESAMPLE_A)
		aml_audiobus_update_bits(actrl,	reg, 1 << 27, !!enable << 27);

}

static void aml_resample_enable(
	struct toddr *to,
	struct toddr_attach *p_attach_resample,
	bool enable)
{
	if (!to || !p_attach_resample) {
		pr_err("%s(), NULL pointer.", __func__);
		return;
	}

	pr_info("toddr %d selects data to %s resample_%c for module:%s\n",
		to->fifo_id,
		enable ? "enable" : "disable",
		(p_attach_resample->id == RESAMPLE_A) ? 'a' : 'b',
		toddr_src_get_str(p_attach_resample->attach_module)
		);

	if (enable) {
		int bitwidth = to->bitdepth;
		/* channels and bit depth for resample */

		if (to->chipinfo
			&& to->chipinfo->asrc_only_left_j
			/*&& (to->src == SPDIFIN)*/
			&& (bitwidth == 32)) {
			struct aml_audio_controller *actrl = to->actrl;
			unsigned int reg_base = to->reg_base;
			unsigned int reg;
			unsigned int endian, toddr_type;

			/* TODO: fixed me */
			pr_info("Warning: Not support 32bit sample rate for axg chipset\n");
			bitwidth = 24;
			endian = 5;
			toddr_type = 4;

			/* FIX ME */
			reg = calc_toddr_address(EE_AUDIO_TODDR_A_CTRL0,
				reg_base);
			aml_audiobus_update_bits(actrl, reg,
				0x7 << 24 | 0x7 << 13,
				endian << 24 | toddr_type << 13);
		}

		if (p_attach_resample->resample_version == 1) {
			if (p_attach_resample->id == RESAMPLE_A) {
				new_resampleA_set_format(
						p_attach_resample->id,
						to->channels, bitwidth);
			}
			new_resample_src_select(p_attach_resample->id,
						to->fifo_id);
		} else if (p_attach_resample->resample_version == 0) {
			/* toddr index for resample */
			if (to->chipinfo &&
			    to->chipinfo->asrc_src_sel_ctrl) {
				resample_src_select_ab(p_attach_resample->id,
						       to->fifo_id);
			} else {
				resample_src_select(to->fifo_id);
			}
			resample_format_set(p_attach_resample->id,
					    to->channels, bitwidth);
		}
	}

	/* select reample data */
	if (to->chipinfo
			&& to->chipinfo->asrc_src_sel_ctrl)
		aml_toddr_set_resample_ab(to, p_attach_resample->id, enable);
	else
		aml_toddr_set_resample(to, enable);

	/* resample enable or disable */
	if (p_attach_resample->resample_version == 1)
		new_resample_enable(p_attach_resample->id, enable);
	else if (p_attach_resample->resample_version == 0)
		resample_enable(p_attach_resample->id, enable);
}

void aml_set_resample(enum resample_idx id,
		bool enable, enum toddr_src resample_module)
{
	struct toddr_attach *p_attach_resample;
	struct toddr *to;

	if (id == RESAMPLE_A)
		p_attach_resample = &attach_resample_a;
	else
		p_attach_resample = &attach_resample_b;

	p_attach_resample->enable        = enable;
	p_attach_resample->id            = id;
	p_attach_resample->attach_module = resample_module;
	p_attach_resample->resample_version = get_resample_version_id(id);

	mutex_lock(&ddr_mutex);
	to = fetch_toddr_by_src(
		p_attach_resample->attach_module);
	if (to == NULL) {
		pr_info("%s(), toddr NULL\n", __func__);
		goto exit;
	}

	if (p_attach_resample->status == RUNNING)
		aml_resample_enable(to, p_attach_resample, enable);

exit:
	mutex_unlock(&ddr_mutex);
}

/*
 * when try to enable resample, if toddr is not in used,
 * set resample status as ready
 */
static void aml_check_resample(struct toddr *to, bool enable)
{
	struct toddr_attach *p_attach_resample;
	int i;

	p_attach_resample = &attach_resample_a;

	for (i = 0; i < get_resample_module_num(); i++) {
		if (to->src == p_attach_resample->attach_module) {
			/* save toddr status */
			if (enable)
				p_attach_resample->status = RUNNING;
			else
				p_attach_resample->status = DISABLED;

			/*if disable toddr, disable attached resampler*/
			if (p_attach_resample->enable)
				aml_resample_enable(to, p_attach_resample,
						    enable);
		}
		p_attach_resample = &attach_resample_b;
	}
}

static void aml_set_pwrdet(struct toddr *to,
	bool enable)
{
	if (enable) {
		struct aml_audio_controller *actrl = to->actrl;
		unsigned int reg_base = to->reg_base;
		unsigned int reg, val;
		unsigned int toddr_type, msb, lsb;

		reg = calc_toddr_address(EE_AUDIO_TODDR_A_CTRL0, reg_base);
		val = aml_audiobus_read(actrl, reg);
		toddr_type = (val >> 13) & 0x7;
		msb = (val >> 8) & 0x1f;
		lsb = (val >> 3) & 0x1f;

		aml_pwrdet_format_set(toddr_type, msb, lsb);
	}
	pwrdet_src_select(enable, to->src);
}

void aml_pwrdet_enable(bool enable, int pwrdet_module)
{
	attach_pwrdet.enable = enable;
	attach_pwrdet.attach_module = pwrdet_module;
	if (enable) {
		if ((attach_pwrdet.status == DISABLED)
			|| (attach_pwrdet.status == READY)) {
			struct toddr *to = fetch_toddr_by_src(pwrdet_module);

			if (!to) {
				attach_pwrdet.status = READY;
			} else {
				attach_pwrdet.status = RUNNING;
				aml_set_pwrdet(to, enable);
				pr_info("Capture with power detect\n");
			}
		}
	} else {
		if (attach_pwrdet.status == RUNNING) {
			struct toddr *to = fetch_toddr_by_src(pwrdet_module);

			if (to)
				aml_set_pwrdet(to, enable);
		}
		attach_pwrdet.status = DISABLED;
	}
}

static bool aml_check_pwrdet_module(int src)
{
	bool is_module_pwrdet = false;

	if (attach_pwrdet.enable
		&& (src == attach_pwrdet.attach_module))
		is_module_pwrdet = true;

	return is_module_pwrdet;
}

static void aml_check_pwrdet(bool enable)
{
	/* power detect in enable */
	if (attach_pwrdet.enable) {
		if (enable) {
			/* check whether ready ? */
			if (attach_pwrdet.status == READY)
				aml_pwrdet_enable(true,
					attach_pwrdet.attach_module);
		} else {
			if (attach_pwrdet.status == RUNNING)
				attach_pwrdet.status = READY;
		}
	}
}

static void aml_vad_enable(
	struct toddr_attach *p_attach_vad,
	bool enable)
{
	struct toddr *to = fetch_toddr_by_src(p_attach_vad->attach_module);

	if (!to)
		return;

	vad_set_toddr_info(enable ? to : NULL);

	/* vad enable or not */
	vad_enable(enable);
}

void aml_set_vad(bool enable, int module)
{
	struct toddr_attach *p_attach_vad = &attach_vad;
	bool update_running = false;

	p_attach_vad->enable        = enable;
	p_attach_vad->attach_module = module;

	if (enable) {
		if ((p_attach_vad->status == DISABLED)
			|| (p_attach_vad->status == READY)) {
			struct toddr *to = fetch_toddr_by_src(
				p_attach_vad->attach_module);

			if (!to) {
				p_attach_vad->status = READY;
			} else {
				p_attach_vad->status = RUNNING;
				update_running = true;
				pr_info("Capture with VAD\n");
			}
		}
	} else {
		if (p_attach_vad->status == RUNNING)
			update_running = true;

		p_attach_vad->status = DISABLED;
	}

	if (update_running)
		aml_vad_enable(p_attach_vad, enable);
}

/*
 * when try to enable vad, if toddr is not in used,
 * set vad status as ready
 */
static void aml_check_vad(struct toddr *to, bool enable)
{
	struct toddr_attach *p_attach_vad = &attach_vad;
	bool is_vad = false;

	if (p_attach_vad->enable
		&& (to->src == p_attach_vad->attach_module))
		is_vad = true;

	/* vad in enable */
	if (is_vad) {
		if (enable)
			p_attach_vad->status = RUNNING;
		else
			p_attach_vad->status = DISABLED;

		aml_vad_enable(p_attach_vad, enable);
	}
}


/* from DDRS */
static struct frddr *register_frddr_l(struct device *dev,
	struct aml_audio_controller *actrl,
	irq_handler_t handler, void *data, bool rvd_dst)
{
	struct frddr *from;
	unsigned int mask_bit;
	int i, ret;

	for (i = 0; i < DDRMAX; i++) {
		/* lookup reserved frddr */
		if (frddrs[i].in_use == false &&
			frddrs[i].reserved == true &&
			rvd_dst == true)
			break;
		/* lookup unused frddr */
		if (frddrs[i].in_use == false &&
			frddrs[i].reserved == false &&
			rvd_dst == false)
			break;
	}

	if (i >= DDRMAX)
		return NULL;

	from = &frddrs[i];

	/* enable audio ddr arb */
	mask_bit = i + 4;
	/*aml_audiobus_update_bits(actrl, EE_AUDIO_ARB_CTRL,*/
	/*		(1 << 31)|(1 << mask_bit),*/
	/*		(1 << 31)|(1 << mask_bit));*/

	/* irqs request */
	ret = request_threaded_irq(from->irq, aml_ddr_isr, handler,
		IRQF_SHARED, dev_name(dev), data);
	if (ret) {
		dev_err(dev, "failed to claim irq %u\n", from->irq);
		return NULL;
	}
	from->dev = dev;
	from->in_use = true;
	pr_debug("frddrs[%d] registered by device %s\n", i, dev_name(dev));
	return from;
}

static int unregister_frddr_l(struct device *dev, void *data)
{
	struct frddr *from;
	struct aml_audio_controller *actrl;
	unsigned int mask_bit;
	unsigned int value;
	int i;

	if (dev == NULL)
		return -EINVAL;

	for (i = 0; i < DDRMAX; i++) {
		if ((frddrs[i].dev) == dev && frddrs[i].in_use)
			break;
	}

	if (i >= DDRMAX)
		return -EINVAL;

	from = &frddrs[i];

	/* disable audio ddr arb */
	mask_bit = i + 4;
	actrl = from->actrl;
	/*aml_audiobus_update_bits(actrl, EE_AUDIO_ARB_CTRL,*/
	/*		1 << mask_bit, 0 << mask_bit);*/

	/* no ddr active, disable arb switch */
	value = aml_audiobus_read(actrl, EE_AUDIO_ARB_CTRL) & 0x77;
	/*if (value == 0)*/
	/*	aml_audiobus_update_bits(actrl, EE_AUDIO_ARB_CTRL,*/
	/*			1 << 31, 0 << 31);*/

	free_irq(from->irq, data);
	from->dev = NULL;
	from->in_use = false;
	pr_debug("frddrs[%d] released by device %s\n", i, dev_name(dev));
	return 0;
}

int fetch_frddr_index_by_src(int frddr_src)
{
	int i;

	for (i = 0; i < DDRMAX; i++) {
		if (frddrs[i].in_use
			&& (frddrs[i].dest == frddr_src)) {
			return i;
		}
	}

	return -1;
}

struct frddr *fetch_frddr_by_src(int frddr_src)
{
	int i;

	for (i = 0; i < DDRMAX; i++) {
		if (frddrs[i].in_use
			&& (frddrs[i].dest == frddr_src)) {
			return &frddrs[i];
		}
	}

	return NULL;
}

struct frddr *aml_audio_register_frddr(struct device *dev,
	struct aml_audio_controller *actrl,
	irq_handler_t handler, void *data, bool rvd_dst)
{
	struct frddr *fr = NULL;

	mutex_lock(&ddr_mutex);
	fr = register_frddr_l(dev, actrl, handler, data, rvd_dst);
	mutex_unlock(&ddr_mutex);
	return fr;
}

int aml_audio_unregister_frddr(struct device *dev, void *data)
{
	int ret;

	mutex_lock(&ddr_mutex);
	ret = unregister_frddr_l(dev, data);
	mutex_unlock(&ddr_mutex);
	return ret;
}

static inline unsigned int
	calc_frddr_address(unsigned int reg, unsigned int base)
{
	return base + reg - EE_AUDIO_FRDDR_A_CTRL0;
}

/*
 * check frddr_src is used by other frddr for sharebuffer
 * if used, disabled the other share frddr src, the module would
 * for current frddr, and the checked frddr
 */
int aml_check_sharebuffer_valid(struct frddr *fr, int ss_sel)
{
	int current_fifo_id = fr->fifo_id;
	unsigned int i;
	int ret = 1;

	for (i = 0; i < DDRMAX; i++) {
		if (frddrs[i].in_use
			&& (frddrs[i].fifo_id != current_fifo_id)
			&& (frddrs[i].dest == ss_sel)) {
			ret = 0;
			break;
		}
	}

	return ret;
}

/* select dst for same source
 * sel: share buffer req_sel 1~2
 * sel 0 is aleardy used for reg_frddr_src_sel1
 * sel 1 is for reg_frddr_src_sel2
 * sel 2 is for reg_frddr_src_sel3
 */
static void frddr_set_sharebuffer_enable(
	struct frddr *fr,  int dst, int sel, bool enable)
{
	struct aml_audio_controller *actrl = fr->actrl;
	unsigned int reg_base = fr->reg_base;
	unsigned int reg;
	int s_v = 0, s_m = 0;

	if (fr->chipinfo
		&& fr->chipinfo->src_sel_ctrl) {
		reg = calc_frddr_address(EE_AUDIO_FRDDR_A_CTRL2,
				reg_base);

		switch (sel) {
		case 1:
			s_m = 0x17 << 8;
			s_v = enable ?
				(dst << 8 | 1 << 12) : 0 << 8;
			break;
		case 2:
			s_m = 0x17 << 16;
			s_v = enable ?
				(dst << 16 | 1 << 20) : 0 << 16;
			break;
		default:
			pr_warn_once("sel :%d is not supported for same source\n",
				sel);
			break;
		}
	} else {
		reg = calc_frddr_address(EE_AUDIO_FRDDR_A_CTRL0,
				reg_base);

		switch (sel) {
		case 1:
			s_m = 0xf << 4;
			s_v = enable ?
				(dst << 4 | 1 << 7) : 0 << 4;
			break;
		case 2:
			s_m = 0xf << 8;
			s_v = enable ?
				(dst << 8 | 1 << 11) : 0 << 8;
			break;
		default:
			pr_warn_once("sel :%d is not supported for same source\n",
				sel);
			break;
		}
	}
	pr_debug("%s sel:%d, dst_src:%d\n",
		__func__, sel, dst);
	fr->ss_dest = enable ? dst : 0;
	fr->ss_en = enable;

	aml_audiobus_update_bits(actrl, reg, s_m, s_v);
}

/*
 * check frddr_src is used by other frddr for sharebuffer
 * if used for share frddr src, release from sharebuffer
 * and used for new frddr
 */
static int aml_check_and_release_sharebuffer(struct frddr *fr, int ss_sel)
{
	int current_fifo_id = fr->fifo_id;
	unsigned int i;
	int ret = 1;

	for (i = 0; i < DDRMAX; i++) {
		struct frddr *from = &frddrs[i];

		if (from->in_use
			&& (from->fifo_id != current_fifo_id)
			&& from->ss_en
			&& (from->ss_dest == ss_sel)) {

			frddr_set_sharebuffer_enable(from,
				ss_sel,
				1,
				false);

			pr_debug("%s, ss_sel:%d release from share buffer, use for new playback\n",
				__func__,
				ss_sel);
			ret = 0;
			break;
		}
	}

	return ret;
}

int aml_frddr_set_buf(struct frddr *fr, unsigned int start,
			unsigned int end)
{
	struct aml_audio_controller *actrl = fr->actrl;
	unsigned int reg_base = fr->reg_base;
	unsigned int reg;

	reg = calc_frddr_address(EE_AUDIO_FRDDR_A_START_ADDR, reg_base);
	aml_audiobus_write(actrl, reg, start);
	reg = calc_frddr_address(EE_AUDIO_FRDDR_A_FINISH_ADDR, reg_base);
	aml_audiobus_write(actrl, reg, end);

	/* int address */
	if (fr->chipinfo
		&& (!fr->chipinfo->int_start_same_addr)) {
		reg = calc_frddr_address(EE_AUDIO_FRDDR_A_INIT_ADDR, reg_base);
		aml_audiobus_write(actrl, reg, start);
	}

	return 0;
}

int aml_frddr_set_intrpt(struct frddr *fr, unsigned int intrpt)
{
	struct aml_audio_controller *actrl = fr->actrl;
	unsigned int reg_base = fr->reg_base;
	unsigned int reg;

	reg = calc_frddr_address(EE_AUDIO_FRDDR_A_INT_ADDR, reg_base);
	aml_audiobus_write(actrl, reg, intrpt);
	reg = calc_frddr_address(EE_AUDIO_FRDDR_A_CTRL0, reg_base);
	aml_audiobus_update_bits(actrl, reg, 0xff<<16, 4<<16);

	return 0;
}

unsigned int aml_frddr_get_position(struct frddr *fr)
{
	struct aml_audio_controller *actrl = fr->actrl;
	unsigned int reg_base = fr->reg_base;
	unsigned int reg;

	reg = calc_frddr_address(EE_AUDIO_FRDDR_A_STATUS2, reg_base);
	return aml_audiobus_read(actrl, reg);
}

void aml_frddr_enable(struct frddr *fr, bool enable)
{
	struct aml_audio_controller *actrl = fr->actrl;
	unsigned int reg_base = fr->reg_base;
	unsigned int reg;

	reg = calc_frddr_address(EE_AUDIO_FRDDR_A_CTRL0, reg_base);
	/* ensure disable before enable frddr */
	aml_audiobus_update_bits(actrl,	reg, 1<<31, enable<<31);

	if (!enable) {
		aml_audiobus_write(actrl, reg, 0x0);

		/* clr src sel and its en */
		if (fr->chipinfo
			&& fr->chipinfo->src_sel_ctrl) {
			reg = calc_frddr_address(EE_AUDIO_FRDDR_A_CTRL2,
				reg_base);
			aml_audiobus_write(actrl, reg, 0x0);
		}
	}
}

void aml_frddr_select_dst(struct frddr *fr, enum frddr_dest dst)
{
	struct aml_audio_controller *actrl = fr->actrl;
	unsigned int reg_base = fr->reg_base;
	unsigned int reg, src_sel_en;

	fr->dest = dst;

	if (fr->chipinfo
		&& fr->chipinfo->src_sel_ctrl) {
		reg = calc_frddr_address(EE_AUDIO_FRDDR_A_CTRL2, reg_base);
		src_sel_en = 4;
		/*update frddr channel*/
		aml_audiobus_update_bits(actrl, reg,
			0xff << 24, (fr->channels - 1) << 24);
	} else {
		reg = calc_frddr_address(EE_AUDIO_FRDDR_A_CTRL0, reg_base);
		src_sel_en = 3;
	}

	/* if sharebuffer in use, release it */
	if (fr->chipinfo
		&& fr->chipinfo->same_src_fn)
		aml_check_and_release_sharebuffer(fr, dst);

	aml_audiobus_update_bits(actrl, reg, 0x7, dst & 0x7);

	/* same source en */
	if (fr->chipinfo
		&& fr->chipinfo->same_src_fn) {
		aml_audiobus_update_bits(actrl, reg,
			1 << src_sel_en, 1 << src_sel_en);
	}
}

/* select dst for same source
 * sel: share buffer req_sel 1~2
 * sel 0 is aleardy used for reg_frddr_src_sel1
 * sel 1 is for reg_frddr_src_sel2
 * sel 2 is for reg_frddr_src_sel3
 */
void aml_frddr_select_dst_ss(struct frddr *fr,
	enum frddr_dest dst, int sel, bool enable)
{
	unsigned int ss_valid = aml_check_sharebuffer_valid(fr, dst);

	/* same source en */
	if (fr->chipinfo
		&& fr->chipinfo->same_src_fn
		&& ss_valid) {
		frddr_set_sharebuffer_enable(fr,
			dst, sel, enable);
	}
}

void aml_frddr_set_fifos(struct frddr *fr,
		unsigned int depth, unsigned int threshold)
{
	struct aml_audio_controller *actrl = fr->actrl;
	unsigned int reg_base = fr->reg_base;
	unsigned int reg;

	if (depth < FIFO_BURST) {
		pr_warn("%s, please check depth:%d less than burst\n",
			__func__, depth);
		depth = FIFO_BURST;
	}
	if (threshold < FIFO_BURST) {
		pr_warn("%s, please check threshold:%d less than burst\n",
			__func__, threshold);
		threshold = FIFO_BURST;
	}

	depth /= FIFO_BURST;
	threshold /= FIFO_BURST;

	reg = calc_frddr_address(EE_AUDIO_FRDDR_A_CTRL1, reg_base);
	aml_audiobus_update_bits(actrl,	reg,
			0xffff << 16 | 0xf << 8,
			(depth - 1) << 24 | (threshold - 1) << 16 | 2 << 8);

	if (fr->chipinfo && fr->chipinfo->ugt) {
		reg = calc_frddr_address(EE_AUDIO_FRDDR_A_CTRL0, reg_base);
		aml_audiobus_update_bits(actrl, reg, 0x1, 0x1);
	}
}

unsigned int aml_frddr_get_fifo_id(struct frddr *fr)
{
	return fr->fifo_id;
}

void aml_frddr_set_format(struct frddr *fr,
	unsigned int chnum,
	unsigned int msb,
	unsigned int frddr_type)
{
	fr->channels = chnum;
	fr->msb  = msb;
	fr->type = frddr_type;
}

static void aml_aed_enable(struct frddr_attach *p_attach_aed, bool enable)
{
	struct frddr *fr = fetch_frddr_by_src(p_attach_aed->attach_module);
	int aed_version = check_aed_version();

	if (aed_version == VERSION2 || aed_version == VERSION3) {
		struct aml_audio_controller *actrl = fr->actrl;
		unsigned int reg_base = fr->reg_base;
		unsigned int reg;

		reg = calc_frddr_address(EE_AUDIO_FRDDR_A_CTRL2, reg_base);
		if (enable) {
			aml_audiobus_update_bits(actrl,
				reg, 0x1 << 3, enable << 3);
			if (aed_version == VERSION3) {
				aed_set_ctrl(enable, 0,
					p_attach_aed->attach_module, 1);
				aed_set_format(fr->msb,
					fr->type, fr->fifo_id, 1);
			} else {
				aed_set_ctrl(enable, 0,
					p_attach_aed->attach_module, 0);
				aed_set_format(fr->msb,
					fr->type, fr->fifo_id, 0);
			}
			aed_enable(enable);
		} else {
			aed_enable(enable);
			if (aed_version == VERSION3) {
				aed_set_ctrl(enable, 0,
					p_attach_aed->attach_module, 1);
			} else {
				aed_set_ctrl(enable, 0,
					p_attach_aed->attach_module, 0);
			}
			aml_audiobus_update_bits(actrl,
				reg, 0x1 << 3, enable << 3);
		}
	} else if (aed_version == VERSION1) {
		if (enable) {
			/* frddr type and bit depth for AED */
			aml_aed_format_set(fr->dest);
		}
		aed_src_select(enable, fr->dest, fr->fifo_id);
	}
}

static bool aml_check_aed_module(int dst)
{
	bool is_module_aed = false;

	if (attach_aed.enable
		&& (dst == attach_aed.attach_module))
		is_module_aed = true;

	return is_module_aed;
}

void aml_set_aed(bool enable, int aed_module)
{
	attach_aed.enable = enable;
	attach_aed.attach_module = aed_module;
}

void aml_aed_top_enable(struct frddr *fr, bool enable)
{
	if (aml_check_aed_module(fr->dest))
		aml_aed_enable(&attach_aed, enable);
}

void aml_aed_set_frddr_reserved(void)
{
	frddrs[DDR_A].reserved = true;
}

void aml_frddr_check(struct frddr *fr)
{
	unsigned int tmp, tmp1, i = 0;
	struct aml_audio_controller *actrl = fr->actrl;
	unsigned int reg_base = fr->reg_base;
	unsigned int reg;

	/*max 200us delay*/
	for (i = 0; i < 200; i++) {
		reg = calc_frddr_address(EE_AUDIO_FRDDR_A_CTRL1, reg_base);
		aml_audiobus_update_bits(actrl,	reg, 0xf << 8, 0x0 << 8);
		reg = calc_frddr_address(EE_AUDIO_FRDDR_A_STATUS2, reg_base);
		tmp = aml_audiobus_read(actrl, reg);

		reg = calc_frddr_address(EE_AUDIO_FRDDR_A_CTRL1, reg_base);
		aml_audiobus_update_bits(actrl,	reg, 0xf << 8, 0x2 << 8);
		reg = calc_frddr_address(EE_AUDIO_FRDDR_A_STATUS2, reg_base);
		tmp1 = aml_audiobus_read(actrl, reg);

		if (tmp == tmp1)
			return;

		udelay(1);
		pr_debug("delay:[%dus]; FRDDR_STATUS2: [0x%x] [0x%x]\n",
			i, tmp, tmp1);
	}
	pr_err("Error: 200us time out, FRDDR_STATUS2: [0x%x] [0x%x]\n",
				tmp, tmp1);
	return;
}

void aml_frddr_reset(struct frddr *fr, int offset)
{
	unsigned int reg = 0, val = 0;

	if (fr == NULL) {
		pr_err("%s(), frddr NULL pointer\n", __func__);
		return;
	}

	if ((offset != 0) && (offset != 1)) {
		pr_err("%s(), invalid offset = %d\n", __func__, offset);
		return;
	}

	if (fr->fifo_id == 0) {
		reg = EE_AUDIO_SW_RESET0(offset);
		val = REG_BIT_RESET_FRDDRA;
	} else if (fr->fifo_id == 1) {
		reg = EE_AUDIO_SW_RESET0(offset);
		val = REG_BIT_RESET_FRDDRB;
	} else if (fr->fifo_id == 2) {
		reg = EE_AUDIO_SW_RESET0(offset);
		val = REG_BIT_RESET_FRDDRC;
	} else if (fr->fifo_id == 3) {
		reg = EE_AUDIO_SW_RESET1;
		val = REG_BIT_RESET_FRDDRD;
	} else {
		pr_err("invalid frddr id %d\n", fr->fifo_id);
		return;
	}

	audiobus_update_bits(reg, val, val);
	audiobus_update_bits(reg, val, 0);
}

void frddr_init_without_mngr(unsigned int frddr_index, unsigned int src0_sel)
{
	unsigned int offset, reg;
	unsigned int start_addr, end_addr, int_addr;
	static int buf[256];

	memset(buf, 0x0, sizeof(buf));
	start_addr = virt_to_phys(buf);
	end_addr = start_addr + sizeof(buf) - 1;
	int_addr = sizeof(buf) / 64;

	offset = EE_AUDIO_FRDDR_B_START_ADDR - EE_AUDIO_FRDDR_A_START_ADDR;
	reg = EE_AUDIO_FRDDR_A_START_ADDR + offset * frddr_index;
	audiobus_write(reg, start_addr);

	offset = EE_AUDIO_FRDDR_B_INIT_ADDR - EE_AUDIO_FRDDR_A_INIT_ADDR;
	reg = EE_AUDIO_FRDDR_A_INIT_ADDR + offset * frddr_index;
	audiobus_write(reg, start_addr);

	offset = EE_AUDIO_FRDDR_B_FINISH_ADDR - EE_AUDIO_FRDDR_A_FINISH_ADDR;
	reg = EE_AUDIO_FRDDR_A_FINISH_ADDR + offset * frddr_index;
	audiobus_write(reg, end_addr);

	offset = EE_AUDIO_FRDDR_B_INT_ADDR - EE_AUDIO_FRDDR_A_INT_ADDR;
	reg = EE_AUDIO_FRDDR_A_INT_ADDR + offset * frddr_index;
	audiobus_write(reg, int_addr);

	offset = EE_AUDIO_FRDDR_B_CTRL1 - EE_AUDIO_FRDDR_A_CTRL1;
	reg = EE_AUDIO_FRDDR_A_CTRL1 + offset * frddr_index;
	audiobus_write(reg,
		(0x40 - 1) << 24 | (0x20 - 1) << 16 | 2 << 8 | 0 << 0);

	offset = EE_AUDIO_FRDDR_B_CTRL0 - EE_AUDIO_FRDDR_A_CTRL0;
	reg = EE_AUDIO_FRDDR_A_CTRL0 + offset * frddr_index;
	audiobus_write(reg,
		1 << 31
		| 0 << 24
		| 4 << 16
		| 1 << 3 /* src0 enable */
		| src0_sel << 0 /* src0 sel */
	);
}

void frddr_deinit_without_mngr(unsigned int frddr_index)
{
	unsigned int offset, reg;

	offset = EE_AUDIO_FRDDR_B_CTRL0 - EE_AUDIO_FRDDR_A_CTRL0;
	reg = EE_AUDIO_FRDDR_A_CTRL0 + offset * frddr_index;
	audiobus_write(reg, 0x0);
}

static enum toddr_src toddr_src_idx = TODDR_INVAL;

static const char *const toddr_src_sel_texts[] = {
	"TDMIN_A", "TDMIN_B", "TDMIN_C", "SPDIFIN",
	"PDMIN", "FRATV", "TDMIN_LB", "LOOPBACK_A",
	"FRHDMIRX", "LOOPBACK_B", "SPDIFIN_LB",
	"EARCRX_DMAC", "RESERVED_0", "RESERVED_1", "RESERVED_2",
	"VAD"
};

static const struct soc_enum toddr_input_source_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0, ARRAY_SIZE(toddr_src_sel_texts),
		toddr_src_sel_texts);

enum toddr_src toddr_src_get(void)
{
	return toddr_src_idx;
}

const char *toddr_src_get_str(enum toddr_src idx)
{
	if (idx < TDMIN_A || idx > VAD)
		return NULL;

	return toddr_src_sel_texts[idx];
}

static int toddr_src_enum_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.enumerated.item[0] = toddr_src_idx;

	return 0;
}

static int toddr_src_enum_set(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	toddr_src_idx = ucontrol->value.enumerated.item[0];
	/* also update to resample src */
	//set_resample_source(toddr_src_idx);

	return 0;
}

static int frddr_src_idx = -1;

static const char *const frddr_src_sel_texts[] = {
	"TDMOUT_A", "TDMOUT_B", "TDMOUT_C",
	"SPDIFOUT_A", "SPDIFOUT_B", "EARCTX_DMAC"
};

static const struct soc_enum frddr_output_source_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0, ARRAY_SIZE(frddr_src_sel_texts),
		frddr_src_sel_texts);

int frddr_src_get(void)
{
	return frddr_src_idx;
}

const char *frddr_src_get_str(int idx)
{
	if (idx < 0 || idx >= FRDDR_MAX)
		return NULL;

	return frddr_src_sel_texts[idx];
}

static int frddr_src_enum_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.enumerated.item[0] = frddr_src_idx;

	return 0;
}

static int frddr_src_enum_set(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	frddr_src_idx = ucontrol->value.enumerated.item[0];

	return 0;
}

static const struct snd_kcontrol_new snd_ddr_controls[] = {
	SOC_ENUM_EXT("Audio In Source",
		toddr_input_source_enum,
		toddr_src_enum_get,
		toddr_src_enum_set),
	SOC_ENUM_EXT("Audio Out Sink",
		toddr_input_source_enum,
		frddr_src_enum_get,
		frddr_src_enum_set),
};

int card_add_ddr_kcontrols(struct snd_soc_card *card)
{
	unsigned int idx;
	int err;

	for (idx = 0; idx < ARRAY_SIZE(snd_ddr_controls); idx++) {
		err = snd_ctl_add(card->snd_card,
				snd_ctl_new1(&snd_ddr_controls[idx],
				NULL));
		if (err < 0)
			return err;
	}

	return 0;
}

static const struct fifo_info ddr_2k_and_1k[] = {
	{DDR_A, FIFO_DEPTH_2K},
	{DDR_B, FIFO_DEPTH_1K},
	{DDR_C, FIFO_DEPTH_1K},
	{DDR_D, FIFO_DEPTH_1K},
};

static const struct fifo_info ddr_32k_and_1k[] = {
	{DDR_A, FIFO_DEPTH_32K},
	{DDR_B, FIFO_DEPTH_1K},
	{DDR_C, FIFO_DEPTH_1K},
	{DDR_D, FIFO_DEPTH_1K},
	{DDR_E, FIFO_DEPTH_1K},
};

static const struct fifo_info ddr_512[] = {
	{DDR_A, FIFO_DEPTH_512},
	{DDR_B, FIFO_DEPTH_512},
};

static struct ddr_chipinfo axg_ddr_chipinfo = {
	.int_start_same_addr   = true,
	.asrc_only_left_j      = true,
	.wakeup                = 1,
	.fifo_num              = 3,
	.toddr_info            = ddr_2k_and_1k,
	.frddr_info            = ddr_2k_and_1k,
};

static struct ddr_chipinfo g12a_ddr_chipinfo = {
	.same_src_fn           = true,
	.asrc_only_left_j      = true,
	.wakeup                = 1,
	.fifo_num              = 3,
	.toddr_info            = ddr_2k_and_1k,
	.frddr_info            = ddr_2k_and_1k,
};

static struct ddr_chipinfo tl1_ddr_chipinfo = {
	.same_src_fn           = true,
	.ugt                   = true,
	.src_sel_ctrl          = true,
	.asrc_src_sel_ctrl     = true,
	.wakeup                = 2,
	.fifo_num              = 4,
	.toddr_info            = ddr_32k_and_1k,
	.frddr_info            = ddr_2k_and_1k,
};

static struct ddr_chipinfo sm1_ddr_chipinfo = {
	.same_src_fn           = true,
	.ugt                   = true,
	.src_sel_ctrl          = true,
	.asrc_src_sel_ctrl     = true,
	.wakeup                = 2,
	.fifo_num              = 4,
	.toddr_info            = ddr_32k_and_1k,
	.frddr_info            = ddr_2k_and_1k,
};

static const struct of_device_id aml_ddr_mngr_device_id[] = {
	{
		.compatible = "amlogic, axg-audio-ddr-manager",
		.data       = &axg_ddr_chipinfo,
	},
	{
		.compatible = "amlogic, g12a-audio-ddr-manager",
		.data       = &g12a_ddr_chipinfo,
	},
	{
		.compatible = "amlogic, tl1-audio-ddr-manager",
		.data       = &tl1_ddr_chipinfo,
	},
	{
		.compatible = "amlogic, sm1-audio-ddr-manager",
		.data       = &sm1_ddr_chipinfo,
	},
	{},
};
MODULE_DEVICE_TABLE(of, aml_ddr_mngr_device_id);

static bool pm_audio_in_suspend;

void pm_audio_set_suspend(bool is_suspend)
{
	pm_audio_in_suspend = is_suspend;
}

bool pm_audio_is_suspend(void)
{
	return pm_audio_in_suspend;
}

/* Detects a suspend and resume event */
static int ddr_pm_event(struct notifier_block *notifier,
	unsigned long pm_event, void *unused)
{
	pr_info("%s, pm_event:%lu\n", __func__, pm_event);

	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		pm_audio_set_suspend(true);
		break;
	case PM_POST_SUSPEND:
		break;
	default:
		break;
	}
	return NOTIFY_DONE;
}

static struct notifier_block ddr_pm_notifier_block = {
	.notifier_call = ddr_pm_event,
};

/* table Must in order */
static struct ddr_info ddr_info[] = {
	{EE_AUDIO_TODDR_A_CTRL0, EE_AUDIO_FRDDR_A_CTRL0, "toddr_a", "frddr_a"},
	{EE_AUDIO_TODDR_B_CTRL0, EE_AUDIO_FRDDR_B_CTRL0, "toddr_b", "frddr_b"},
	{EE_AUDIO_TODDR_C_CTRL0, EE_AUDIO_FRDDR_C_CTRL0, "toddr_c", "frddr_c"},
	{EE_AUDIO_TODDR_D_CTRL0, EE_AUDIO_FRDDR_D_CTRL0, "toddr_d", "frddr_d"},
};

static int ddr_get_toddr_base_addr_by_idx(int idx)
{
	return ddr_info[idx].toddr_addr;
}

static int ddr_get_frddr_base_addr_by_idx(int idx)
{
	return ddr_info[idx].frddr_addr;
}

static char *ddr_get_toddr_name_by_idx(int idx)
{
	return ddr_info[idx].toddr_name;
}

static char *ddr_get_frddr_name_by_idx(int idx)
{
	return ddr_info[idx].frddr_name;
}

static int aml_ddr_mngr_platform_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct device_node *node_prt = NULL;
	struct platform_device *pdev_parent;
	struct aml_audio_controller *actrl = NULL;
	struct ddr_chipinfo *p_ddr_chipinfo;
	int ddr_num = 3; /* early chipset support max 3 ddr num */
	int i, ret;

	/* get audio controller */
	node_prt = of_get_parent(node);
	if (!node_prt)
		return -ENXIO;

	pdev_parent = of_find_device_by_node(node_prt);
	of_node_put(node_prt);
	actrl = (struct aml_audio_controller *)
				platform_get_drvdata(pdev_parent);

	p_ddr_chipinfo = (struct ddr_chipinfo *)
		of_device_get_match_data(&pdev->dev);
	if (!p_ddr_chipinfo) {
		dev_err(&pdev->dev,
			"check to update ddr_mngr chipinfo\n");
		return -EINVAL;
	}

	if (p_ddr_chipinfo->fifo_num == 2)
		ddr_num = p_ddr_chipinfo->fifo_num;
	else if (p_ddr_chipinfo->fifo_num == 4)
		ddr_num = p_ddr_chipinfo->fifo_num;

	for (i = 0; i < ddr_num; i++) {
		toddrs[i].irq =
			platform_get_irq_byname(pdev,
						ddr_get_toddr_name_by_idx(i));
		toddrs[i].reg_base   = ddr_get_toddr_base_addr_by_idx(i);
		toddrs[i].fifo_id    = i;
		toddrs[i].fifo_depth = p_ddr_chipinfo->toddr_info[i].depth;
		toddrs[i].chipinfo   = p_ddr_chipinfo;
		toddrs[i].actrl      = actrl;

		frddrs[i].irq        =
			platform_get_irq_byname(pdev,
						ddr_get_frddr_name_by_idx(i));
		frddrs[i].reg_base   = ddr_get_frddr_base_addr_by_idx(i);
		frddrs[i].fifo_id    = i;
		frddrs[i].fifo_depth = p_ddr_chipinfo->frddr_info[i].depth;
		frddrs[i].chipinfo   = p_ddr_chipinfo;
		frddrs[i].actrl      = actrl;

		dev_info(&pdev->dev, "%d, irqs toddr %d, frddr %d\n",
			 i, toddrs[i].irq, frddrs[i].irq);

		if (toddrs[i].irq <= 0 || frddrs[i].irq <= 0) {
			dev_err(&pdev->dev, "%s, get irq failed\n", __func__);
			return -ENXIO;
		}
	}

	ret = register_pm_notifier(&ddr_pm_notifier_block);
	if (ret)
		pr_warn("[%s] failed to register PM notifier %d\n",
				__func__, ret);

	return 0;
}

struct platform_driver aml_audio_ddr_manager = {
	.driver = {
		.name = DRV_NAME,
		.of_match_table = aml_ddr_mngr_device_id,
	},
	.probe   = aml_ddr_mngr_platform_probe,
};
module_platform_driver(aml_audio_ddr_manager);

/* Module information */
MODULE_AUTHOR("Amlogic, Inc.");
MODULE_DESCRIPTION("ALSA Soc Aml Audio DDR Manager");
MODULE_LICENSE("GPL v2");

