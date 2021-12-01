// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/spinlock.h>

#include "pdm_hw.h"
#include "regs.h"
#include "iomap.h"
#include "pdm_hw_coeff.h"
#include "pdm.h"

static DEFINE_SPINLOCK(pdm_lock);
static unsigned long pdm_enable_cnt;
void pdm_enable(int is_enable, int id)
{
	unsigned long flags;

	spin_lock_irqsave(&pdm_lock, flags);
	if (is_enable) {
		if (pdm_enable_cnt == 0)
			aml_pdm_update_bits(id, PDM_CTRL,
				0x1 << 31,
				is_enable << 31);
		pdm_enable_cnt++;
	} else {
		if (WARN_ON(pdm_enable_cnt == 0))
			goto exit;
		if (--pdm_enable_cnt == 0)
			aml_pdm_update_bits(id, PDM_CTRL,
				0x1 << 31 | 0x1 << 16,
				0 << 31 | 0 << 16);
	}

exit:
	spin_unlock_irqrestore(&pdm_lock, flags);
}

void pdm_fifo_reset(int id)
{
	/* PDM Asynchronous FIFO soft reset.
	 * write 1 to soft reset AFIFO
	 */
	aml_pdm_update_bits(id, PDM_CTRL,
		0x1 << 16,
		0 << 16);

	aml_pdm_update_bits(id, PDM_CTRL,
		0x1 << 16,
		0x1 << 16);
}

void pdm_force_sysclk_to_oscin(bool force, int id, bool vad_top)
{
	if (vad_top) {
		vad_top_update_bits(EE_AUDIO2_CLK_PDMIN_CTRL1, 0x1 << 30, force << 30);
	} else {
		if (id == 0)
			audiobus_update_bits(EE_AUDIO_CLK_PDMIN_CTRL1, 0x1 << 30, force << 30);
		else if (id == 1)
			audiobus_update_bits(EE_AUDIO_CLK_PDMIN_CTRL3, 0x1 << 30, force << 30);
	}
}

void pdm_force_dclk_to_oscin(int id, bool vad_top)
{
	if (vad_top) {
		vad_top_update_bits(EE_AUDIO2_CLK_PDMIN_CTRL0,
				    0x7 << 24 | 0xffff << 0,
				    0x7 << 24 | 0x7 << 0);
	} else {
		if (id == 0)
			audiobus_update_bits(EE_AUDIO_CLK_PDMIN_CTRL0,
					     0x7 << 24 | 0xffff << 0,
					     0x7 << 24 | 0x7 << 0);
		else if (id == 1)
			audiobus_update_bits(EE_AUDIO_CLK_PDMIN_CTRL2,
					    0x7 << 24 | 0xffff << 0,
					    0x7 << 24 | 0x7 << 0);
	}
}

void pdm_set_channel_ctrl(int sample_count, int id)
{
	int left_sample_count = sample_count, right_sample_count = sample_count;
	int train_version = pdm_get_train_version();

	/* only for sc2, left and right sample count are different */
	/* sysclk / dclk / 2 */
	/* 133 / 3.072 / 2 = 22 */
	if (train_version == PDM_TRAIN_VERSION_V2)
		right_sample_count += 22;

	aml_pdm_write(id, PDM_CHAN_CTRL, ((right_sample_count << 24) |
					(left_sample_count << 16) |
					(right_sample_count << 8) |
					(left_sample_count << 0)
		));
	aml_pdm_write(id, PDM_CHAN_CTRL1, ((right_sample_count << 24) |
					(left_sample_count << 16) |
					(right_sample_count << 8) |
					(left_sample_count << 0)
		));
}

void aml_pdm_ctrl(struct pdm_info *info, int id)
{
	int mode, i, ch_mask = 0;
	int pdm_chs, lane_chs = 0;

	if (!info)
		return;

	if (info->bitdepth == 32)
		mode = 0;
	else
		mode = 1;

	/* update pdm channels for loopback */
	pdm_chs = info->channels;
	if (info->channels > PDM_CHANNELS_MAX)
		pdm_chs = PDM_CHANNELS_MAX;

	if (pdm_chs > info->lane_masks * 2)
		pr_warn("capturing channels more than lanes carried\n");

	/* each lanes carries two channels */
	for (i = 0; i < PDM_LANE_MAX; i++)
		if ((1 << i) & info->lane_masks) {
			ch_mask |= (1 << (2 * i));
			lane_chs += 1;
			if (lane_chs >= info->channels)
				break;
			ch_mask |= (1 << (2 * i + 1));
			lane_chs += 1;
			if (lane_chs >= info->channels)
				break;
		}

	pr_info("%s, lane mask:0x%x, channels:%d, channels mask:0x%x, bypass:%d\n",
		__func__,
		info->lane_masks,
		info->channels,
		ch_mask,
		info->bypass);

	aml_pdm_write(id, PDM_CLKG_CTRL, 1);

	/* must be sure that clk and pdm is enable */
	aml_pdm_update_bits(id, PDM_CTRL,
				(0x7 << 28 | 0xff << 8 | 0xff << 0),
				/* invert the PDM_DCLK or not */
				(0 << 30) |
				/* output mode:  1: 24bits. 0: 32 bits */
				(mode << 29) |
				/* bypass mode.
				 * 1: bypass all filter. 0: normal mode.
				 */
				(info->bypass << 28) |
				/* PDM channel reset. */
				(ch_mask << 8) |
				/* PDM channel enable */
				(ch_mask << 0)
				);

	pdm_set_channel_ctrl(info->sample_count, id);
}

void aml_pdm_arb_config(struct aml_audio_controller *actrl, bool use_arb)
{
	/* config ddr arb */
	if (use_arb)
		aml_audiobus_write(actrl, EE_AUDIO_ARB_CTRL, 1 << 31 | 0xff << 0);
}

/* config for hcic, lpf1,2,3, hpf */
static void aml_pdm_filters_config(int pdm_gain_index, int osr,
	int lpf1_len, int lpf2_len, int lpf3_len, int id)
{
	s32 hcic_dn_rate;
	s32 hcic_tap_num;
	s32 hcic_gain;
	s32 f1_tap_num;
	s32 f2_tap_num;
	s32 f3_tap_num;
	s32 f1_rnd_mode;
	s32 f2_rnd_mode;
	s32 f3_rnd_mode;
	s32 f1_ds_rate;
	s32 f2_ds_rate;
	s32 f3_ds_rate;
	s32 hpf_en;
	s32 hpf_shift_step;
	s32 hpf_out_factor;
	s32 pdm_out_mode;

	switch (osr) {
	case 32:
		hcic_dn_rate = 0x4;
		hcic_gain	 = pdm_gain_table[pdm_gain_index][PDM_OSR_32];
		break;
	case 40:
		hcic_dn_rate = 0x5;
		hcic_gain	 = pdm_gain_table[pdm_gain_index][PDM_OSR_40];
		break;
	case 48:
		hcic_dn_rate = 0x6;
		hcic_gain	 = pdm_gain_table[pdm_gain_index][PDM_OSR_48];
		break;
	case 56:
		hcic_dn_rate = 0x7;
		hcic_gain	 = pdm_gain_table[pdm_gain_index][PDM_OSR_56];
		break;
	case 64:
		hcic_dn_rate = 0x0008;
		hcic_gain	 = pdm_gain_table[pdm_gain_index][PDM_OSR_64];
		break;
	case 96:
		hcic_dn_rate = 0x000c;
		hcic_gain	 = pdm_gain_table[pdm_gain_index][PDM_OSR_96];
		break;
	case 128:
		hcic_dn_rate = 0x0010;
		hcic_gain	 = pdm_gain_table[pdm_gain_index][PDM_OSR_128];
		break;
	case 192:
		hcic_dn_rate = 0x0018;
		hcic_gain	 = pdm_gain_table[pdm_gain_index][PDM_OSR_192];
		break;
	case 384:
		hcic_dn_rate = 0x0030;
		hcic_gain	 = pdm_gain_table[pdm_gain_index][PDM_OSR_384];
		break;
	default:
		pr_info("Not support osr:%d, translate to :192\n", osr);
		hcic_dn_rate = 0x0018;
		hcic_gain	 = pdm_gain_table[pdm_gain_index][PDM_OSR_192];
		break;
	}

	hcic_tap_num = 0x0007;
	f1_tap_num	 = lpf1_len;
	f2_tap_num	 = lpf2_len;
	f3_tap_num	 = lpf3_len;
	hpf_shift_step = 0xd;
	hpf_en		 = 1;
	pdm_out_mode	 = 0;
	hpf_out_factor = 0x8000;
	f1_rnd_mode  = 1;
	f2_rnd_mode  = 0;
	f3_rnd_mode  = 1;
	f1_ds_rate	 = 2;
	f2_ds_rate	 = 2;
	f3_ds_rate	 = 2;

	/* hcic */
	aml_pdm_write(id, PDM_HCIC_CTRL1,
		(0x80000000 |
		hcic_tap_num |
		(hcic_dn_rate << 4) |
		(hcic_gain << 16))
		);

	/* lpf */
	aml_pdm_write(id, PDM_F1_CTRL,
		(0x80000000 |
		f1_tap_num |
		(2 << 12) |
		(f1_rnd_mode << 16))
		);
	aml_pdm_write(id, PDM_F2_CTRL,
		(0x80000000 |
		f2_tap_num |
		(2 << 12) |
		(f2_rnd_mode << 16))
		);
	aml_pdm_write(id, PDM_F3_CTRL,
		(0x80000000 |
		f3_tap_num |
		(2 << 12) |
		(f3_rnd_mode << 16))
		);

	/* hpf */
	aml_pdm_write(id, PDM_HPF_CTRL,
		(hpf_out_factor |
		(hpf_shift_step << 16) |
		(hpf_en << 31))
		);

}

/* coefficent for LPF1,2,3 */
static void aml_pdm_LPF_coeff(int lpf1_len, const int *lpf1_coeff,
	int lpf2_len, const int *lpf2_coeff,
	int lpf3_len, const int *lpf3_coeff, int id)
{
	int i;
	s32 data;
	s32 data_tmp;

	aml_pdm_write(id, PDM_COEFF_ADDR, 0);
	for (i = 0;
		i < lpf1_len;
		i++)
		aml_pdm_write(id, PDM_COEFF_DATA, lpf1_coeff[i]);
	for (i = 0;
		i < lpf2_len;
		i++)
		aml_pdm_write(id, PDM_COEFF_DATA, lpf2_coeff[i]);
	for (i = 0;
		i < lpf3_len;
		i++)
		aml_pdm_write(id, PDM_COEFF_DATA, lpf3_coeff[i]);

	aml_pdm_write(id, PDM_COEFF_ADDR, 0);
	for (i = 0; i < lpf1_len; i++) {
		data = aml_pdm_read(id, PDM_COEFF_DATA);
		data_tmp = lpf1_coeff[i];
		if (data != data_tmp) {
			pr_info("FAILED coeff data verified wrong!\n");
			pr_info("Coeff = %x\n", data);
			pr_info("DDR COEFF = %x\n", data_tmp);
		}
	}
	for (i = 0; i < lpf2_len; i++) {
		data = aml_pdm_read(id, PDM_COEFF_DATA);
		data_tmp = lpf2_coeff[i];
		if (data != data_tmp) {
			pr_info("FAILED coeff data verified wrong!\n");
			pr_info("Coeff = %x\n", data);
			pr_info("DDR COEFF = %x\n", data_tmp);
		}
	}
	for (i = 0; i < lpf3_len; i++) {
		data = aml_pdm_read(id, PDM_COEFF_DATA);
		data_tmp = lpf3_coeff[i];
		if (data != data_tmp) {
			pr_info("FAILED coeff data verified wrong!\n");
			pr_info("Coeff = %x\n", data);
			pr_info("DDR COEFF = %x\n", data_tmp);
		}
	}

}

void aml_pdm_filter_ctrl(int pdm_gain_index, int osr, int mode, int id)
{
	int lpf1_len, lpf2_len, lpf3_len;
	const int *lpf1_coeff, *lpf2_coeff, *lpf3_coeff;

	/* select LPF coefficent
	 * For filter 1 and filter 3,
	 * it's only relative with coefficent mode
	 * For filter 2,
	 * it's only relative with osr and hcic stage number
	 */

	/* mode and its latency
	 *	mode	|	latency
	 *	0		|	47.7
	 *	1		|	38.7
	 *	2		|	26
	 *	3		|	20.4
	 *	4		|	15
	 */

	switch (osr) {
	case 32:
	case 64:
		lpf2_coeff = lpf2_osr64;
		break;
	case 96:
		lpf2_coeff = lpf2_osr96;
		break;
	case 128:
		lpf2_coeff = lpf2_osr128;
		break;
	case 192:
		lpf2_coeff = lpf2_osr192;
		break;
	case 256:
		lpf2_coeff = lpf2_osr256;
		break;
	case 384:
		lpf2_coeff = lpf2_osr384;
		break;
	default:
		pr_info("osr :%d , lpf2 uses default parameters with osr64\n",
			osr);
		lpf2_coeff = lpf2_osr64;
		break;
	}

	switch (mode) {
	case 0:
		lpf1_len = 110;
		lpf2_len = 33;
		lpf3_len = 147;
		lpf1_coeff = lpf1_mode0;
		lpf3_coeff = lpf3_mode0;
		break;
	case 1:
		lpf1_len = 87;
		lpf2_len = 33;
		lpf3_len = 117;
		lpf1_coeff = lpf1_mode1;
		lpf3_coeff = lpf3_mode1;
		break;
	case 2:
		lpf1_len = 87;
		lpf2_len = 33;
		lpf3_len = 66;
		lpf1_coeff = lpf1_mode2;
		lpf3_coeff = lpf3_mode2;
		break;
	case 3:
		lpf1_len = 65;
		lpf2_len = 33;
		lpf3_len = 49;
		lpf1_coeff = lpf1_mode3;
		lpf3_coeff = lpf3_mode3;
		break;
	case 4:
		lpf1_len = 43;
		lpf2_len = 33;
		lpf3_len = 32;
		lpf1_coeff = lpf1_mode4;
		lpf3_coeff = lpf3_mode4;
		break;
	default:
		pr_info("default mode 1, osr 64\n");
		lpf1_len = 87;
		lpf2_len = 33;
		lpf3_len = 117;
		lpf1_coeff = lpf1_mode1;
		lpf3_coeff = lpf3_mode1;
		break;
	}

	/* config filter */
	aml_pdm_filters_config(pdm_gain_index,
		osr,
		lpf1_len,
		lpf2_len,
		lpf3_len, id);

	aml_pdm_LPF_coeff(lpf1_len, lpf1_coeff,
		lpf2_len, lpf2_coeff,
		lpf3_len, lpf3_coeff,
		id);

}

int pdm_get_mute_value(int id)
{
	return aml_pdm_read(id, PDM_MUTE_VALUE);
}

void pdm_set_mute_value(int val, int id)
{
	aml_pdm_write(id, PDM_MUTE_VALUE, val);
}

int pdm_get_mute_channel(int id)
{
	int val = aml_pdm_read(id, PDM_CTRL);

	return (val & (0xff << 20));
}

void pdm_set_mute_channel(int mute_chmask, int id)
{
	int mute_en = 0;

	if (mute_chmask)
		mute_en = 1;

	aml_pdm_update_bits(id, PDM_CTRL,
		(0xff << 20 | 0x1 << 17),
		(mute_chmask << 20 | mute_en << 17));
}

void pdm_set_bypass_data(bool bypass, int id)
{
	aml_pdm_update_bits(id, PDM_CTRL, 0x1 << 28, bypass << 28);
}

void pdm_init_truncate_data(int freq, int id)
{
	int mask_val;

	/* assume mask 1.05ms */
	mask_val = ((freq / 1000) * 1050 * 8) / 1000 - 1;
	aml_pdm_write(id, PDM_MASK_NUM, mask_val);
}

void pdm_train_en(bool en, int id)
{
	aml_pdm_update_bits(id, PDM_CTRL,
		0x1 << 19,
		en << 19);
}

void pdm_train_clr(int id)
{
	aml_pdm_update_bits(id, PDM_CTRL,
		0x1 << 18,
		0x1 << 18);
}

int pdm_train_sts(int id)
{
	int val = aml_pdm_read(id, PDM_STS);

	return ((val >> 4) & 0xff);
}

int pdm_dclkidx2rate(int idx)
{
	int rate;

	if (idx == 2)
		rate = 768000;
	else if (idx == 1)
		rate = 1024000;
	else
		rate = 3072000;

	return rate;
}

int pdm_get_sample_count(int islowpower, int dclk_idx)
{
	int count = 0;

	if (islowpower) {
		count = 0;
	} else if (dclk_idx == 1) {
		count = 38;
	} else if (dclk_idx  == 2) {
		count = 48;
	} else {
		count = pdm_get_train_sample_count_from_dts();
		if (count < 0)
			count = 18;
	}

	return count;
}

int pdm_get_ors(int dclk_idx, int sample_rate)
{
	int osr = 0;

	if (dclk_idx == 1) {
		if (sample_rate == 16000)
			osr = 64;
		else if (sample_rate == 8000)
			osr = 128;
		else
			pr_err("%s, Not support rate:%d\n",
				__func__, sample_rate);
	} else if (dclk_idx == 2) {
		if (sample_rate == 16000)
			osr = 48;
		else if (sample_rate == 8000)
			osr = 96;
		else
			pr_err("%s, Not support rate:%d\n",
				__func__, sample_rate);
	} else {
		if (sample_rate == 96000)
			osr = 32;
		else if (sample_rate == 48000)
			osr = 64;
		else if (sample_rate == 32000)
			osr = 96;
		else if (sample_rate == 24000)
			osr = 128;
		else if (sample_rate == 16000)
			osr = 192;
		else if (sample_rate == 8000)
			osr = 384;
		else
			pr_err("%s, Not support rate:%d\n",
				__func__, sample_rate);
	}

	return osr;
}

int pdm_auto_train_algorithm(int pdm_id, int enable)
{
	int sample_count = 0;
	int find = 0;
	int val;
	int count = 0;
	int sample_count1, sample_count2;

	aml_pdm_update_bits(pdm_id, PDM_CTRL, 1 << 19, enable << 19);
	if (enable == 0)
		return 0;
	pdm_set_channel_ctrl(sample_count, pdm_id);
	while (1) {
		aml_pdm_update_bits(pdm_id, PDM_CTRL, 1 << 18, 1 << 18);
		aml_pdm_update_bits(pdm_id, PDM_CTRL, 1 << 18, 0 << 18);
		usleep_range(8, 10);
		val = (aml_pdm_read(pdm_id, PDM_STS) >> 4) & 0xff;
		if (val == 0) {
			count++;
			/*try to 10*/
			if (count < 10)
				continue;
			else
				count = 0;
		} else {
			/*must be find two sample count for max and min range*/
			if (find == 1) {
				sample_count2 = sample_count;
				if (abs(sample_count2 - sample_count1) > 10) {
					pr_info("sample range:%d~:%d\n",
					(sample_count1 < sample_count2 ?
					sample_count1 : sample_count2),
					(sample_count1 < sample_count2 ?
					sample_count2 : sample_count1));
					break;
				}
			} else {
				sample_count1 = sample_count;
				find = 1;
			}
		}
		pdm_set_channel_ctrl(sample_count++, 0);
		if (sample_count > 100) {
			pr_info("not find the sample range\n");
			break;
		}
	}
	return (sample_count1 + sample_count2) / 2;
}
