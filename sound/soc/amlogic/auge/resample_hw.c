/*
 * sound/soc/amlogic/auge/resample_hw.c
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
#include <linux/clk.h>
#include <linux/amlogic/iomap.h>
#include <linux/math64.h>

#include "resample_hw.h"
#include "resample_hw_coeff.h"
#include "regs.h"
#include "iomap.h"

/*Cnt_ctrl = mclk/fs_out-1 ; fest 256fs */
#define RESAMPLE_CNT_CONTROL (255)
#define SINC8_FILTER_COEF_ADDR (0)
#define AA_FILTER_COEF_ADDR (129 * 32)

static u32 resample_coef_parameters_table[7][5] = {
	/*coef of 32K, fc = 9000, Q:0.55, G= 14.00, */
	{0x0146cd61, 0x0081f5a5, 0x038eadfd, 0x0081f5a5, 0x00557b5f},
	/*coef of 44.1K, fc = 14700, Q:0.55, G= 14.00, */
	{0x0106f9aa, 0x00b84366, 0x03cdcb2d, 0x00b84366, 0x0054c4d7},
	/*coef of 48K, fc = 15000, Q:0.60, G= 11.00, */
	{0x00ea25ae, 0x00afe01d, 0x03e0efb0, 0x00afe01d, 0x004b155e},
	/*coef of 88.2K, fc = 26000, Q:0.60, G= 4.00, */
	{0x009dc098, 0x000972c7, 0x000e7582, 0x00277b49, 0x000e2d97},
	/*coef of 96K, fc = 36000, Q:0.50, G= 4.00, */
	{0x0098178d, 0x008b0d0d, 0x00087862, 0x008b0d0d, 0x00208fef},
	/*coef of 192K*/
	{0x008741e5, 0x008fd7fd, 0x001ed6c9, 0x008fd7fd, 0x002618ae},
	/*no support filter now*/
	{0x00800000, 0x0, 0x0, 0x0, 0x0},
};

#ifdef AA_FILTER_DEBUG
void new_resample_set_ram_coeff_aa(enum resample_idx id, int len,
				   unsigned int *params)
{
	int i;
	unsigned int *p = params;

	new_resample_write(id, AUDIO_RSAMP_AA_COEF_ADDR, AA_FILTER_COEF_ADDR);
	for (i = 0; i < len; i++, p++)
		new_resample_write(id, AUDIO_RSAMP_AA_COEF_DATA, *p);
}
#endif

void new_resample_set_ram_coeff_sinc(enum resample_idx id, int len,
				     unsigned int *params)
{
	int i;
	unsigned int *p = params;

	new_resample_write(id, AUDIO_RSAMP_SINC_COEF_ADDR,
			   SINC8_FILTER_COEF_ADDR);
	for (i = 0; i < len; i++, p++)
		new_resample_write(id, AUDIO_RSAMP_SINC_COEF_DATA, *p);
}

void new_resample_init_param(enum resample_idx id)
{
	new_resample_write(id, AUDIO_RSAMP_CTRL0, 7);
	new_resample_write(id, AUDIO_RSAMP_CTRL0, 0);

	/*enable memory power*/
	aml_write_hiubus(HHI_AUDIO_MEM_PD_REG0, 0x0);

	/* filter tap of AA and sinc filter */
	new_resample_update_bits(id, AUDIO_RSAMP_CTRL2, 0xff << 8, 0x3f << 8);
	new_resample_update_bits(id, AUDIO_RSAMP_CTRL2, 0xff, 0x3f);
	new_resample_update_bits(id, AUDIO_RSAMP_CTRL1, 0x1 << 12, 0x1 << 12);

	if (id == RESAMPLE_A) {
		/*write resample A filter in ram*/
		new_resample_set_ram_coeff_sinc(id, SINC8_FILTER_COEF_SIZE,
						&sinc8_coef[0]);
	}
}

void new_resample_enable(enum resample_idx id, bool enable)
{
	new_resample_update_bits(id, AUDIO_RSAMP_CTRL1,
				 0x1 << 11, enable << 11);
}

bool new_resample_get_status(enum resample_idx id)
{
	unsigned int val = new_resample_read(id, AUDIO_RSAMP_CTRL1);

	return ((val >> 11) & 0x1);
}

static int new_resample_status_check(enum resample_idx id)
{
	int check_times = 20;
	unsigned int val;

	/* wait resample status to bypass, then change config, max 20us */
	while (check_times--) {
		val = new_resample_read(id, AUDIO_RSAMP_RO_STATUS);
		val = (val >> 11) & 0x7;
		if (val == 1)
			break;
		udelay(1);
	}

	if (check_times == 0) {
		pr_err("%s(), check resample status error!", __func__);
		return -1;
	}

	return 0;
}

static void new_resample_adjust_enable(enum resample_idx id,
				       int mclk_ratio, int enable)
{
	/* enable auto adjust module */
	new_resample_update_bits(id, AUDIO_RSAMP_CTRL0, 0x1 << 2,
				 0x1 << 2);
	new_resample_update_bits(id, AUDIO_RSAMP_ADJ_CTRL1, 0xffff << 16,
				 (mclk_ratio) << 16);
	new_resample_write(id, AUDIO_RSAMP_ADJ_SFT, 0x1f020604);
	new_resample_write(id, AUDIO_RSAMP_ADJ_IDET_LEN, 0x22710);
	new_resample_write(id, AUDIO_RSAMP_ADJ_FORCE, 0x0);
	new_resample_write(id, AUDIO_RSAMP_ADJ_CTRL0, enable);
	new_resample_update_bits(id, AUDIO_RSAMP_CTRL0, 0x1 << 2, 0x0 << 2);
}

void new_resampleA_set_format(enum resample_idx id, int channel, int bits)
{
	int reg_val = bits - 1;
	int mclk_ratio = 256 / channel;

	if (channel > 8 || channel == 0 || bits < 16)
		return;

	new_resample_enable(id, false);
	new_resample_status_check(id);

	new_resample_write(id, AUDIO_RSAMP_PHSINIT, 0x0);
	/* channel num */
	new_resample_update_bits(id, AUDIO_RSAMP_CTRL2, 0x3f << 24,
				 channel << 24);
	/* bit width */
	new_resample_update_bits(id, AUDIO_RSAMP_CTRL1, 0x1f << 13,
				 reg_val << 13);

	new_resample_adjust_enable(id, mclk_ratio, 1);

	new_resample_enable(id, true);

	pr_info("%s(), channel = %d, bits = %d", __func__, channel, bits);
}

void new_resampleB_set_format(enum resample_idx id, int output_sr)
{
    /*MCLK/output_sr/channel_num*/
	int mclk_ratio = 6144000 / output_sr;

	/*write resample B filter in ram*/
	if (output_sr == DEFAULT_MIC_SAMPLERATE) {
		new_resample_set_ram_coeff_sinc(id, SINC8_FILTER_COEF_SIZE,
						&sinc_coef[0]);
	} else {
		new_resample_set_ram_coeff_sinc(id, SINC8_FILTER_COEF_SIZE,
						&sinc8_coef[0]);
	}

	new_resample_enable(id, false);
	new_resample_status_check(id);

	new_resample_write(id, AUDIO_RSAMP_PHSINIT, 0x0);
	/* channel num */
	new_resample_update_bits(id, AUDIO_RSAMP_CTRL2, 0x3f << 24,
				 2 << 24); /* always two channel for loopback */
	/* bit width */
	new_resample_update_bits(id, AUDIO_RSAMP_CTRL1, 0x1f << 13,
				 31 << 13); /* tdmin_lb is always 32bit */

	new_resample_adjust_enable(id, mclk_ratio, 1);

	new_resample_enable(id, true);
}

void new_resample_src_select(enum resample_idx id, enum resample_src src)
{
    /* resample B is always for loopbackA */
	if (id == RESAMPLE_B)
		src = ASRC_LOOPBACK_A;

	new_resample_enable(id, false);
	new_resample_status_check(id);

	/* resample source select */
	new_resample_update_bits(id, AUDIO_RSAMP_CTRL1, 0xf, src);

	new_resample_enable(id, true);
}

void new_resample_set_ratio(enum resample_idx id, int input_sr, int output_sr)
{
	u32 input_sample_rate = (u32)input_sr;
	u32 output_sample_rate = (u32)output_sr;
	u64 phase_step = (u64)input_sample_rate;

	new_resample_enable(id, false);
	new_resample_status_check(id);

	phase_step *= (1 << 28);
	phase_step = div_u64(phase_step, output_sample_rate);

	new_resample_write(id, AUDIO_RSAMP_PHSSTEP, (u32)phase_step);

	new_resample_enable(id, true);

	pr_info("%s(), id = %d, phase_step = 0x%x, input_sr = %d, output_sr = %d",
		__func__, id, (u32)phase_step, input_sr, output_sr);
}

void resample_enable(enum resample_idx id, bool enable)
{
	int offset = EE_AUDIO_RESAMPLEB_CTRL0 - EE_AUDIO_RESAMPLEA_CTRL0;
	int reg = EE_AUDIO_RESAMPLEA_CTRL0 + offset * id;

	if (enable == 1) {
		/*don't change this flow*/
		audiobus_update_bits(
			reg, 0x1 << 31 | 0x1 << 28,
			0 << 31 | 0x0 << 28);

		audiobus_update_bits(reg, 0x1 << 31, 1 << 31);

		audiobus_update_bits(reg, 0x1 << 31, 0 << 31);
	}

	audiobus_update_bits(reg, 0x1 << 28, enable << 28);
}

bool resample_get_status(enum resample_idx id)
{
	int offset = EE_AUDIO_RESAMPLEB_CTRL0 - EE_AUDIO_RESAMPLEA_CTRL0;
	int reg = EE_AUDIO_RESAMPLEA_CTRL0 + offset * id;
	bool enable = (audiobus_read(reg) >> 28) & 0x1;

	return enable;
}

int resample_init(enum resample_idx id, int input_sr)
{
	u16 Avg_cnt_init = 0;
	unsigned int clk_rate = 167000000;//clk81;
	int offset = EE_AUDIO_RESAMPLEB_CTRL0 - EE_AUDIO_RESAMPLEA_CTRL0;
	int reg = EE_AUDIO_RESAMPLEA_CTRL0 + offset * id;

	if (input_sr)
		Avg_cnt_init = (u16)(clk_rate * 4 / input_sr);
	else
		pr_err("unsupport input sample rate:%d\n", input_sr);

	pr_info("resample id:%c, clk_rate = %u, input_sr = %d, Avg_cnt_init = %u\n",
		(id == 0) ? 'a' : 'b',
		clk_rate,
		input_sr,
		Avg_cnt_init);

	audiobus_update_bits(reg,
		0x3 << 26 | 0x3ff << 16 | 0xffff,
		0x0 << 26 | /* method0 */
		RESAMPLE_CNT_CONTROL << 16 |
		Avg_cnt_init);

	return 0;
}

int resample_set_hw_param(enum resample_idx id,
		enum samplerate_index rate_index)
{
	int i, reg, offset;

	if (rate_index < RATE_32K) {
		pr_info("%s(), inval index %d", __func__, rate_index);
		return -EINVAL;
	}

	offset = EE_AUDIO_RESAMPLEB_COEF0 - EE_AUDIO_RESAMPLEA_COEF0;
	reg = EE_AUDIO_RESAMPLEA_COEF0 + offset * id;

	for (i = 0; i < 5; i++) {
		audiobus_write((reg + i),
			resample_coef_parameters_table[rate_index - 1][i]);
	}

	offset = EE_AUDIO_RESAMPLEB_CTRL2 - EE_AUDIO_RESAMPLEA_CTRL2;
	reg = EE_AUDIO_RESAMPLEA_CTRL2 + offset * id;

	audiobus_update_bits(reg, 3 << 26 | 1 << 25, 3 << 26 | 1 << 25);
	resample_set_hw_pause_thd(id, 128);

	return 0;
}

/* not avail for tl1 */
void resample_src_select(int src)
{
	audiobus_update_bits(EE_AUDIO_RESAMPLEA_CTRL0,
		0x3 << 29,
		src << 29);
}

/* for tl1 and after */
void resample_src_select_ab(enum resample_idx id, enum resample_src src)
{
	int offset = EE_AUDIO_RESAMPLEB_CTRL3 - EE_AUDIO_RESAMPLEA_CTRL3;
	int reg = EE_AUDIO_RESAMPLEA_CTRL3 + offset * id;

	audiobus_update_bits(reg,
		0x7 << 16,
		src << 16);
}

void resample_format_set(enum resample_idx id, int ch_num, int bits)
{
	int offset = EE_AUDIO_RESAMPLEB_CTRL3 - EE_AUDIO_RESAMPLEA_CTRL3;
	int reg = EE_AUDIO_RESAMPLEA_CTRL3 + offset * id;

	audiobus_write(reg,
		ch_num << 8 | (bits - 1) << 0);
}

int resample_ctrl_read(enum resample_idx id)
{
	int offset = EE_AUDIO_RESAMPLEB_CTRL0 - EE_AUDIO_RESAMPLEA_CTRL0;
	int reg = EE_AUDIO_RESAMPLEA_CTRL0 + offset * id;

	return audiobus_read(reg);
}

void resample_ctrl_write(enum resample_idx id, int value)
{
	int offset = EE_AUDIO_RESAMPLEB_CTRL0 - EE_AUDIO_RESAMPLEA_CTRL0;
	int reg = EE_AUDIO_RESAMPLEA_CTRL0 + offset * id;

	audiobus_write(reg, value);
}

int resample_set_hw_pause_thd(enum resample_idx id, unsigned int thd)
{
	int offset = EE_AUDIO_RESAMPLEB_CTRL2 - EE_AUDIO_RESAMPLEA_CTRL2;
	int reg = EE_AUDIO_RESAMPLEA_CTRL2 + offset * id;

	audiobus_update_bits(reg, 1 << 24 | 0x1fff << 11,
			1 << 24 | thd << 11);

	return 0;
}
