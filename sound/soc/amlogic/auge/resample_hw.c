// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#include <linux/clk.h>
#include <linux/amlogic/iomap.h>
#include <linux/math64.h>

#include "resample.h"
#include "resample_hw.h"
#include "resample_hw_coeff.h"
#include "regs.h"
#include "iomap.h"

/*Cnt_ctrl = mclk/fs_out-1 ; fest 256fs */
#define RESAMPLE_CNT_CONTROL (255)

static u32 resample_coef_parameters_table[9][5] = {
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
	/*no support coef of 176K*/
	{0x00800000, 0x0, 0x0, 0x0, 0x0},
	/*coef of 192K*/
	{0x008741e5, 0x008fd7fd, 0x001ed6c9, 0x008fd7fd, 0x002618ae},
	/*no support coef of 16K*/
	{0x00800000, 0x0, 0x0, 0x0, 0x0},
	/*no support filter now*/
	{0x00800000, 0x0, 0x0, 0x0, 0x0},
};

#ifdef AA_FILTER_DEBUG
/* only can read sam coeff on tm2_revB */
void check_ram_coeff_aa(enum resample_idx id, int len,
			unsigned int *params)
{
	int i;
	unsigned int *p = params;
	unsigned int val = 0;

	new_resample_write(id, AUDIO_RSAMP_SINC_COEF_ADDR,
			   SINC8_FILTER_COEF_ADDR);

	for (i = 0; i < len; i++, p++) {
		val = new_resample_read(id, AUDIO_RSAMP_AA_COEF_DATA);
		if (val != *p)
			pr_err("error ram coeff read[%d]:0x%08x, write:0x%08x\n",
			       i, val, *p);
	}
}

/* only can read sam coeff on tm2_revB */
void check_ram_coeff_sinc(enum resample_idx id, int len,
			  unsigned int *params)
{
	int i;
	unsigned int *p = params;
	unsigned int val = 0;

	new_resample_write(id, AUDIO_RSAMP_SINC_COEF_ADDR,
			   SINC8_FILTER_COEF_ADDR);

	for (i = 0; i < len; i++, p++) {
		val = new_resample_read(id, AUDIO_RSAMP_SINC_COEF_DATA);
		if (val != *p)
			pr_err("error ram coeff read[%d]:0x%08x, write:0x%08x\n",
			       i, val, *p);
	}
}

#endif

void new_resample_set_ram_coeff_aa(enum resample_idx id, int len,
				   unsigned int *params)
{
	int i;
	unsigned int *p = params;

	new_resample_write(id, AUDIO_RSAMP_AA_COEF_ADDR, AA_FILTER_COEF_ADDR);
	for (i = 0; i < len; i++, p++)
		new_resample_write(id, AUDIO_RSAMP_AA_COEF_DATA, *p);

	/* check_ram_coeff_aa(id, len, params); */
}

void new_resample_set_ram_coeff_sinc(enum resample_idx id, int len,
				     unsigned int *params)
{
	int i;
	unsigned int *p = params;

	new_resample_write(id, AUDIO_RSAMP_SINC_COEF_ADDR,
			   SINC8_FILTER_COEF_ADDR);
	for (i = 0; i < len; i++, p++)
		new_resample_write(id, AUDIO_RSAMP_SINC_COEF_DATA, *p);

#ifdef AA_FILTER_DEBUG
	check_ram_coeff_sinc(id, len, params);
#endif
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
	/* resample clk free run, doesn't gate */
	new_resample_update_bits(id, AUDIO_RSAMP_CTRL1, 0x3f << 18, 0x2a << 18);

	if (id == RESAMPLE_A) {
		/*write resample A filter in ram*/
		new_resample_set_ram_coeff_sinc(id, SINC8_FILTER_COEF_SIZE,
						&sinc8_coef[0]);
	}
}

void new_resample_enable_watchdog(enum resample_idx id, bool enable)
{
	/* set threshold of watchdog: 1ms */
	new_resample_write(id, AUDIO_RSAMP_WATCHDOG_THRD, 0x1c593);
	/* enable watch dog to check and auto reset adj */
	new_resample_update_bits(id, AUDIO_RSAMP_CTRL1,
				 0x1 << 26, enable << 26);
}

void new_resample_enable(enum resample_idx id, bool enable, int channel)
{
	/* if resample bypass, bypass chxsync too */
	if (get_resample_enable_chnum_sync(id)) {
		bool chsync_enable = enable;

		if (get_resample_source(id) == EARCRX_DMAC &&
		    channel > 2) {
			chsync_enable = false;
		}
		aml_resample_chsync_enable(id, chsync_enable);
	}

	/* 1: bypass, 0: enable*/
	new_resample_update_bits(id, AUDIO_RSAMP_CTRL1,
				 0x1 << 24, (!enable) << 24);
}

bool new_resample_get_status(enum resample_idx id)
{
	unsigned int val = new_resample_read(id, AUDIO_RSAMP_CTRL1);

	return !((val >> 24) & 0x1);
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
	new_resample_update_bits(id, AUDIO_RSAMP_ADJ_CTRL1, 0xffff, 0);
	new_resample_write(id, AUDIO_RSAMP_ADJ_SFT, 0x1f020604);
	new_resample_write(id, AUDIO_RSAMP_ADJ_IDET_LEN, 0x100);
	new_resample_write(id, AUDIO_RSAMP_ADJ_FORCE, 0x0);
	new_resample_write(id, AUDIO_RSAMP_ADJ_CTRL0, enable);
	new_resample_update_bits(id, AUDIO_RSAMP_CTRL0, 0x1 << 2, 0x0 << 2);
}

static void resample_set_bits(enum resample_idx id, int msb, int lsb)
{
	int mask = 0x1f;

	msb &= mask;
	lsb &= mask;
	new_resample_update_bits(id, AUDIO_RSAMP_CTRL1,
			 mask << 13, msb << 13);
	new_resample_update_bits(id, AUDIO_RSAMP_CTRL1,
			 mask << 27, lsb << 27);
}

void new_resampleA_set_format(enum resample_idx id, int channel, int bits)
{
	int msb = bits - 1, lsb = 0;
	int mclk_ratio = 256 / channel;
	enum toddr_src src = get_resample_source(id);

	if (channel > 8 || channel == 0 || bits < 16)
		return;

	new_resample_update_bits(id, AUDIO_RSAMP_CTRL1,
				 0x1 << 11, 0 << 11);

	new_resample_status_check(id);

	new_resample_write(id, AUDIO_RSAMP_PHSINIT, 0x0);
	/* channel num */
	new_resample_update_bits(id, AUDIO_RSAMP_CTRL2, 0x3f << 24,
				 channel << 24);

	if (get_resample_enable_chnum_sync(id)) {
		if (src == EARCRX_DMAC && channel > 2) {
			aml_resample_chsync_enable(id, false);
		} else {
			aml_resample_chsync_set(id, channel);
			aml_resample_chsync_enable(id, true);
		}
	}

	/* it should always send 24bit audio to resample module to improve SNR */
	if (get_resample_version() >= T5_RESAMPLE)
		get_toddr_bits_config(src, bits, &msb, &lsb);
	resample_set_bits(id, msb, lsb);

	new_resample_adjust_enable(id, mclk_ratio, 1);

	new_resample_update_bits(id, AUDIO_RSAMP_CTRL1,
				 0x1 << 11, 1 << 11);

	pr_info("%s(), channel = %d, bits = %d", __func__, channel, bits);
}

void new_resampleB_set_format(enum resample_idx id, int output_sr, int channel)
{
	/*MCLK/output_sr/channel_num*/
	int mclk_ratio = 12288000 / output_sr / channel;

	/*write resample B filter in ram*/
	if (output_sr == DEFAULT_MIC_SAMPLERATE) {
		new_resample_set_ram_coeff_sinc(id, SINC8_FILTER_COEF_SIZE,
						&sinc_coef[0]);
	} else {
		new_resample_set_ram_coeff_sinc(id, SINC8_FILTER_COEF_SIZE,
						&sinc8_coef[0]);
	}

	new_resample_set_ram_coeff_aa(id, AA_FILTER_COEF_SIZE, &aa_coef_a_half[0]);

	new_resample_update_bits(id, AUDIO_RSAMP_CTRL1,
				 0x1 << 11, 0 << 11);

	new_resample_status_check(id);

	new_resample_write(id, AUDIO_RSAMP_PHSINIT, 0x0);
	/* channel num */
	new_resample_update_bits(id, AUDIO_RSAMP_CTRL2, 0x3f << 24,
				 channel << 24);

	if (get_resample_enable_chnum_sync(id))
		aml_resample_chsync_set(id, channel);

	/* bit width */
	new_resample_update_bits(id, AUDIO_RSAMP_CTRL1, 0x1f << 13,
				 31 << 13); /* tdmin_lb is always 32bit */

	new_resample_adjust_enable(id, mclk_ratio, 1);

	new_resample_update_bits(id, AUDIO_RSAMP_CTRL1,
				 0x1 << 11, 1 << 11);
}

void new_resample_src_select(enum resample_idx id, enum resample_src src)
{
    /* resample B is always for loopbackA */
	if (id == RESAMPLE_B)
		src = ASRC_LOOPBACK_A;

	new_resample_update_bits(id, AUDIO_RSAMP_CTRL1,
				 0x1 << 11, 0 << 11);
	new_resample_status_check(id);

	/* resample source select */
	new_resample_update_bits(id, AUDIO_RSAMP_CTRL1, 0xf, src);

	new_resample_update_bits(id, AUDIO_RSAMP_CTRL1,
				 0x1 << 11, 1 << 11);
}

void new_resample_src_select_v2(enum resample_idx id, unsigned int src)
{
	/* from t5 chip, resample src changed
	 * resampleB is set for tdmin_lb
	 */
	if (id == RESAMPLE_B)
		src = TDMIN_LB;

	new_resample_update_bits(id, AUDIO_RSAMP_CTRL1,
				 0x1 << 11, 0 << 11);
	new_resample_status_check(id);

	/* resample source select */
	new_resample_update_bits(id, AUDIO_RSAMP_CTRL1, 0x1f, src);

	new_resample_update_bits(id, AUDIO_RSAMP_CTRL1,
				 0x1 << 11, 1 << 11);
}

void new_resample_set_ratio(enum resample_idx id, int input_sr, int output_sr)
{
	u32 input_sample_rate = (u32)input_sr;
	u32 output_sample_rate = (u32)output_sr;
	u64 phase_step = (u64)input_sample_rate;

	new_resample_update_bits(id, AUDIO_RSAMP_CTRL1,
				 0x1 << 11, 0 << 11);
	new_resample_status_check(id);

	/* max downsample ratio is 8, if more than 8, enable AA filter */
	if (div_u64(input_sample_rate, output_sample_rate) > 8) {
		new_resample_update_bits(id, AUDIO_RSAMP_CTRL2, 0x3 << 16, 0x1 << 16);
		new_resample_update_bits(id, AUDIO_RSAMP_CTRL1, 0x1 << 10, 0x1 << 10);
		phase_step *= (1 << 27);
	} else {
		new_resample_update_bits(id, AUDIO_RSAMP_CTRL2, 0x3 << 16, 0x0 << 16);
		new_resample_update_bits(id, AUDIO_RSAMP_CTRL1, 0x1 << 10, 0x0 << 10);
		phase_step *= (1 << 28);
	}

	phase_step = div_u64(phase_step, output_sample_rate);

	new_resample_write(id, AUDIO_RSAMP_PHSSTEP, (u32)phase_step);

	new_resample_update_bits(id, AUDIO_RSAMP_CTRL1,
				 0x1 << 11, 1 << 11);

	pr_info("%s(), id = %d, phase_step = 0x%x, input_sr = %d, output_sr = %d",
		__func__, id, (u32)phase_step, input_sr, output_sr);
}

void resample_enable(enum resample_idx id, bool enable)
{
	int offset = EE_AUDIO_RESAMPLEB_CTRL0 - EE_AUDIO_RESAMPLEA_CTRL0;
	int reg = EE_AUDIO_RESAMPLEA_CTRL0 + offset * id;

	if (enable == 1) {
		/*don't change this flow*/
		audiobus_update_bits(reg, 0x1 << 31 | 0x1 << 28,
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
	u16 avg_cnt_init = 0;
	unsigned int clk_rate = 167000000;//clk81;
	int offset = EE_AUDIO_RESAMPLEB_CTRL0 - EE_AUDIO_RESAMPLEA_CTRL0;
	int reg = EE_AUDIO_RESAMPLEA_CTRL0 + offset * id;

	if (input_sr)
		avg_cnt_init = (u16)(clk_rate * 4 / input_sr);
	else
		pr_err("unsupport input sample rate:%d\n", input_sr);

	pr_debug("resample id:%c, clk_rate = %u, input_sr = %d, avg_cnt_init = %u\n",
		 (id == 0) ? 'a' : 'b',
		 clk_rate,
		 input_sr,
		avg_cnt_init);

	audiobus_update_bits(reg,
		0x3 << 26 | 0x3ff << 16 | 0xffff,
		0x0 << 26 | /* method0 */
		RESAMPLE_CNT_CONTROL << 16 |
		avg_cnt_init);

	return 0;
}

int resample_set_hw_param(enum resample_idx id,
		enum samplerate_index rate_index)
{
	int i, reg, offset;

	if (rate_index < RATE_32K) {
		pr_err("%s(), inval index %d", __func__, rate_index);
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

void aml_resample_chsync_enable(enum resample_idx id, bool enable)
{
	int offset =
		EE_AUDIO_RSAMP_B_CHSYNC_CTRL - EE_AUDIO_RSAMP_A_CHSYNC_CTRL;
	int reg = EE_AUDIO_RSAMP_A_CHSYNC_CTRL + offset * id;

	/* bit 31: enable */
	audiobus_update_bits(reg,
			     0x1 << 31,
			     enable << 31);
}

void aml_resample_chsync_set(enum resample_idx id, int channel)
{
	int offset =
		EE_AUDIO_RSAMP_B_CHSYNC_CTRL - EE_AUDIO_RSAMP_A_CHSYNC_CTRL;
	int reg = EE_AUDIO_RSAMP_A_CHSYNC_CTRL + offset * id;

	/* bit 0-7: chnum_max */
	audiobus_update_bits(reg,
			     0x7F << 0,
			     (channel - 1) << 0);
}

