// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#define DEBUG
#include <sound/soc.h>

#include "iomap.h"
#include "spdif_hw.h"
#include "ddr_mngr.h"
#include "spdif.h"

#include <linux/amlogic/media/vout/hdmi_tx_ext.h>
#include <linux/amlogic/media/sound/aout_notify.h>

void aml_spdifin_chnum_en(struct aml_audio_controller *actrl,
	int index, bool is_enable)
{
	unsigned int reg;

	reg = EE_AUDIO_SPDIFIN_CTRL0;
	aml_audiobus_update_bits(actrl, reg, 1 << 26, is_enable << 26);

	pr_debug("%s spdifin ctrl0:0x%x\n",
		__func__,
		aml_audiobus_read(actrl, reg));
}

void aml_spdif_enable(struct aml_audio_controller *actrl,
	int stream,
	int index,
	bool is_enable)
{
	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		unsigned int offset, reg;

		offset = EE_AUDIO_SPDIFOUT_B_CTRL0 - EE_AUDIO_SPDIFOUT_CTRL0;
		reg = EE_AUDIO_SPDIFOUT_CTRL0 + offset * index;
		aml_audiobus_update_bits(actrl, reg, 1 << 31, is_enable << 31);
		if (!is_enable) {
			offset = EE_AUDIO_SPDIFOUT_B_CTRL1 - EE_AUDIO_SPDIFOUT_CTRL1;
			reg = EE_AUDIO_SPDIFOUT_CTRL1 + offset * index;
			aml_audiobus_update_bits(actrl, reg, 0x1 << 3, 0x0 << 3);
		}
	} else {
		aml_audiobus_update_bits(actrl,
			EE_AUDIO_SPDIFIN_CTRL0, 1 << 31, is_enable << 31);
	}
}

void aml_spdif_mute(struct aml_audio_controller *actrl,
	int stream, int index, bool is_mute)
{
	int mute_lr = 0;

	if (is_mute)
		mute_lr = 0x3;

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		unsigned int offset, reg;

		offset = EE_AUDIO_SPDIFOUT_B_CTRL0 - EE_AUDIO_SPDIFOUT_CTRL0;
		reg = EE_AUDIO_SPDIFOUT_CTRL0 + offset * index;
		if (!is_mute)
			aml_audiobus_update_bits(actrl, reg, 0x1 << 30, 0x0 << 30);
		aml_audiobus_update_bits(actrl, reg, 0x3 << 21, mute_lr << 21);
		if (is_mute) {
			aml_audiobus_update_bits(actrl, reg, 0x1 << 30, 0x1 << 30);

			offset = EE_AUDIO_SPDIFOUT_B_CTRL1 - EE_AUDIO_SPDIFOUT_CTRL1;
			reg = EE_AUDIO_SPDIFOUT_CTRL1 + offset * index;
			aml_audiobus_update_bits(actrl, reg, 0x1 << 3, 0x1 << 3);
		}
	} else {
		aml_audiobus_update_bits(actrl, EE_AUDIO_SPDIFIN_CTRL0,
					 0x3 << 6, mute_lr << 6);
	}
}

void aml_spdifout_mute_without_actrl(int index, bool is_mute)
{
	unsigned int offset, reg;
	int mute_lr = 0;

	if (is_mute)
		mute_lr = 0x3;

	offset = EE_AUDIO_SPDIFOUT_B_CTRL0 - EE_AUDIO_SPDIFOUT_CTRL0;
	reg = EE_AUDIO_SPDIFOUT_CTRL0 + offset * index;
	if (!is_mute)
		audiobus_update_bits(reg, 0x1 << 30, 0x0 << 30);
	audiobus_update_bits(reg, 0x3 << 21, mute_lr << 21);
	if (is_mute) {
		audiobus_update_bits(reg, 0x1 << 30, 0x1 << 30);

		offset = EE_AUDIO_SPDIFOUT_B_CTRL1 - EE_AUDIO_SPDIFOUT_CTRL1;
		reg = EE_AUDIO_SPDIFOUT_CTRL1 + offset * index;
		audiobus_update_bits(reg, 0x1 << 3, 0x1 << 3);
	}
}

void aml_spdif_arb_config(struct aml_audio_controller *actrl, bool use_arb)
{
	/* config ddr arb */
	if (use_arb)
		aml_audiobus_write(actrl, EE_AUDIO_ARB_CTRL, 1 << 31 | 0xfff << 0);
}

int aml_spdifin_status_check(struct aml_audio_controller *actrl)
{
	unsigned int val;

	val = aml_audiobus_read(actrl, EE_AUDIO_SPDIFIN_STAT0);

	/* pr_info("\t--- spdif handles status0 %#x\n", val); */
	return val;
}

void aml_spdifin_clr_irq(struct aml_audio_controller *actrl,
	bool is_all_bits, int clr_bits_val)
{
	if (is_all_bits) {
		aml_audiobus_update_bits(actrl, EE_AUDIO_SPDIFIN_CTRL0,
				1 << 26, 1 << 26);
		aml_audiobus_update_bits(actrl, EE_AUDIO_SPDIFIN_CTRL0,
				1 << 26, 0);
	} else {
		aml_audiobus_update_bits(actrl, EE_AUDIO_SPDIFIN_CTRL6,
				0xff << 16, clr_bits_val << 16);
	}
}

void aml_spdif_fifo_reset(struct aml_audio_controller *actrl,
	int stream, int index)
{
	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		/* reset afifo */
		unsigned int offset, reg;

		offset = EE_AUDIO_SPDIFOUT_B_CTRL0 - EE_AUDIO_SPDIFOUT_CTRL0;
		reg = EE_AUDIO_SPDIFOUT_CTRL0 + offset * index;
		aml_audiobus_update_bits(actrl, reg, 3 << 28, 0);
		aml_audiobus_update_bits(actrl, reg, 1 << 29, 1 << 29);
		aml_audiobus_update_bits(actrl, reg, 1 << 28, 1 << 28);
	} else {
		/* reset afifo */
		aml_audiobus_update_bits
			(actrl, EE_AUDIO_SPDIFIN_CTRL0, 3 << 28, 0);
		aml_audiobus_update_bits
			(actrl, EE_AUDIO_SPDIFIN_CTRL0, 1 << 29, 1 << 29);
		aml_audiobus_update_bits
			(actrl,	EE_AUDIO_SPDIFIN_CTRL0, 1 << 28, 1 << 28);
	}
}

int spdifout_get_frddr_type(int bitwidth)
{
	unsigned int frddr_type = 0;

	switch (bitwidth) {
	case 8:
		frddr_type = 0;
		break;
	case 16:
		frddr_type = 1;
		break;
	case 24:
		frddr_type = 4;
		break;
	case 32:
		frddr_type = 3;
		break;
	default:
		pr_err("runtime format invalid bitwidth: %d\n",
			bitwidth);
		break;
	}

	return frddr_type;
}

void aml_spdif_fifo_ctrl(struct aml_audio_controller *actrl,
	int bitwidth, int stream,
	int index, unsigned int fifo_id)
{
	unsigned int toddr_type;
	unsigned int frddr_type = spdifout_get_frddr_type(bitwidth);
	unsigned int out_lane_mask = 0;

	switch (bitwidth) {
	case 8:
		toddr_type = 0;
		break;
	case 16:
		toddr_type = 1;
		break;
	case 24:
		toddr_type = 4;
		break;
	case 32:
		toddr_type = 3;
		break;
	default:
		pr_err("runtime format invalid bitwidth: %d\n",
			bitwidth);
		return;
	}

	pr_debug("%s, bit depth:%d, frddr type:%d, toddr:type:%d\n",
		__func__, bitwidth, frddr_type, toddr_type);

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		unsigned int offset, reg;

		/* mask lane 0 L/R channels */
		offset = EE_AUDIO_SPDIFOUT_B_CTRL0 - EE_AUDIO_SPDIFOUT_CTRL0;
		reg = EE_AUDIO_SPDIFOUT_CTRL0 + offset * index;
		aml_audiobus_update_bits(actrl,
			reg,
			0x1 << 29 | 0x1 << 28 | 0x1 << 20 | 0x1 << 19,
			0x1 << 29 | 0x1 << 28 | 0 << 20 | 0 << 19);

		out_lane_mask = spdifout_get_lane_mask_version(index);
		if (out_lane_mask == SPDIFOUT_LANE_MASK_V2)
			aml_audiobus_update_bits(actrl,
						 reg,
						 0xffff,
						 0x3);
		else
			aml_audiobus_update_bits(actrl,
						 reg,
						 0xff << 4,
						 0x3 << 4);

		offset = EE_AUDIO_SPDIFOUT_B_CTRL1 - EE_AUDIO_SPDIFOUT_CTRL1;
		reg = EE_AUDIO_SPDIFOUT_CTRL1 + offset * index;
		aml_audiobus_update_bits(actrl,
			reg,
			0x3 << 24 | 0x1f << 8 | 0x7 << 4,
			fifo_id << 24 | (bitwidth - 1) << 8 | frddr_type << 4);

		offset = EE_AUDIO_SPDIFOUT_B_SWAP - EE_AUDIO_SPDIFOUT_SWAP;
		reg = EE_AUDIO_SPDIFOUT_SWAP + offset * index;
		aml_audiobus_write(actrl, reg, 1 << 4);
	} else {
		unsigned int spdifin_clk = 500000000;

		/* sysclk/rate/32(bit)/2(ch)/2(bmc) */
		unsigned int counter_32k  = (spdifin_clk / (32000  * 64));
		unsigned int counter_44k  = (spdifin_clk / (44100  * 64));
		unsigned int counter_48k  = (spdifin_clk / (48000  * 64));
		unsigned int counter_88k  = (spdifin_clk / (88200  * 64));
		unsigned int counter_96k  = (spdifin_clk / (96000  * 64));
		unsigned int counter_176k = (spdifin_clk / (176400 * 64));
		unsigned int counter_192k = (spdifin_clk / (192000 * 64));
		unsigned int mode0_th = 3 * (counter_32k + counter_44k) >> 1;
		unsigned int mode1_th = 3 * (counter_44k + counter_48k) >> 1;
		unsigned int mode2_th = 3 * (counter_48k + counter_88k) >> 1;
		unsigned int mode3_th = 3 * (counter_88k + counter_96k) >> 1;
		unsigned int mode4_th = 3 * (counter_96k + counter_176k) >> 1;
		unsigned int mode5_th = 3 * (counter_176k + counter_192k) >> 1;
		unsigned int mode0_timer = counter_32k >> 1;
		unsigned int mode1_timer = counter_44k >> 1;
		unsigned int mode2_timer = counter_48k >> 1;
		unsigned int mode3_timer = counter_88k >> 1;
		unsigned int mode4_timer = counter_96k >> 1;
		unsigned int mode5_timer = (counter_176k >> 1);
		unsigned int mode6_timer = (counter_192k >> 1);

		aml_audiobus_write(actrl, EE_AUDIO_SPDIFIN_CTRL1,
			0xff << 20 | (spdifin_clk / 10000) << 0);

		aml_audiobus_write(actrl, EE_AUDIO_SPDIFIN_CTRL2,
			mode0_th << 20 |
			mode1_th << 10 |
			mode2_th << 0);

		aml_audiobus_write(actrl, EE_AUDIO_SPDIFIN_CTRL3,
			mode3_th << 20 |
			mode4_th << 10 |
			mode5_th << 0);

		aml_audiobus_write(actrl, EE_AUDIO_SPDIFIN_CTRL4,
			(mode0_timer << 24) |
			(mode1_timer << 16) |
			(mode2_timer << 8)  |
			(mode3_timer << 0));

		aml_audiobus_write(actrl, EE_AUDIO_SPDIFIN_CTRL5,
			(mode4_timer << 24) |
			(mode5_timer << 16) |
			(mode6_timer << 8));

		aml_audiobus_update_bits(actrl, EE_AUDIO_SPDIFIN_CTRL0,
			0x1 << 25 | 0x1 << 24 | 0xfff << 12,
			0x1 << 25 | 0x0 << 24 | 0xff << 12);
	}

}

int spdifin_get_mode(void)
{
	int mode_val = audiobus_read(EE_AUDIO_SPDIFIN_STAT0);

	mode_val >>= 28;
	mode_val &= 0x7;

	return mode_val;
}

int spdif_get_channel_status(int reg)
{
	return audiobus_read(reg);
}

void spdifin_set_channel_status(int ch, int bits)
{
	int ch_status_sel = (ch << 3 | bits) & 0xf;

	/*which channel status would be got*/
	audiobus_update_bits(EE_AUDIO_SPDIFIN_CTRL0,
			     0xf << 8, ch_status_sel << 8);
}

void aml_spdifout_select_aed(bool enable, int spdifout_id)
{
	unsigned int offset, reg;

	/* select eq_drc output */
	offset = EE_AUDIO_SPDIFOUT_B_CTRL1 - EE_AUDIO_SPDIFOUT_CTRL1;
	reg = EE_AUDIO_SPDIFOUT_CTRL1 + offset * spdifout_id;
	audiobus_update_bits(reg, 0x1 << 31, enable << 31);
}

void aml_spdifout_get_aed_info(int spdifout_id,	int *bitwidth, int *frddrtype)
{
	unsigned int reg, offset, val;

	offset = EE_AUDIO_SPDIFOUT_B_CTRL1
			- EE_AUDIO_SPDIFOUT_CTRL1;
	reg = EE_AUDIO_SPDIFOUT_CTRL1 + offset * spdifout_id;

	val = audiobus_read(reg);
	if (bitwidth)
		*bitwidth = (val >> 8) & 0x1f;
	if (frddrtype)
		*frddrtype = (val >> 4) & 0x7;
}

void enable_spdif_to_hdmitx_dat(bool enable)
{
	int val = !!enable;

	audiobus_update_bits(EE_AUDIO_TOHDMITX_CTRL0,
			     1 << 31 | 1 << 3 | 1 << 2,
			     val << 31 | 1 << 3);
}

void enable_spdif_to_hdmitx_clk(bool enable)
{
	int val = !!enable;

	audiobus_update_bits(EE_AUDIO_TOHDMITX_CTRL0,
			0x1 << 30, val << 30);
}

void enable_spdifout_to_hdmitx(int separated)
{
	/* if tohdmitx_en is separated, need do:
	 * step1: enable/disable clk
	 * step2: enable/disable dat
	 */
	if (separated) {
		audiobus_update_bits(EE_AUDIO_TOHDMITX_CTRL0,
				0x1 << 30, 0x1 << 30);
	}

	audiobus_update_bits(EE_AUDIO_TOHDMITX_CTRL0,
			     1 << 31 | 1 << 3 | 1 << 2,
			     1 << 31 | 1 << 3);
}

static void spdifout_fifo_ctrl(int spdif_id, int fifo_id, int bitwidth,
			       int channels, int lane_i2s)
{
	unsigned int frddr_type = spdifout_get_frddr_type(bitwidth);
	unsigned int offset, reg, i, chmask = 0;
	unsigned int swap_masks = 0;

	for (i = 0; i < channels; i++)
		chmask |= (1 << i);

	swap_masks = (2 * lane_i2s) |
		(2 * lane_i2s + 1) << 4;
	pr_debug("spdif_%s fifo ctrl, frddr:%d type:%d, %d bits, chmask %#x, swap %#x\n",
		(spdif_id == 0) ? "a" : "b",
		fifo_id,
		frddr_type,
		bitwidth,
		chmask,
		swap_masks);

	/* mask lane 0 L/R channels */
	offset = EE_AUDIO_SPDIFOUT_B_CTRL0 - EE_AUDIO_SPDIFOUT_CTRL0;
	reg = EE_AUDIO_SPDIFOUT_CTRL0 + offset * spdif_id;
	audiobus_update_bits(reg,
		0x1 << 20 | 0x1 << 19,
		0 << 20 | 0 << 19);

	if (spdifout_get_lane_mask_version(spdif_id) == SPDIFOUT_LANE_MASK_V2)
		audiobus_update_bits(reg,
				     0xffff,
				     chmask);
	else
		audiobus_update_bits(reg,
				     0xff << 4,
				     chmask << 4);


	offset = EE_AUDIO_SPDIFOUT_B_CTRL1 - EE_AUDIO_SPDIFOUT_CTRL1;
	reg = EE_AUDIO_SPDIFOUT_CTRL1 + offset * spdif_id;
	audiobus_update_bits(reg, 0x3 << 24 | 0x1f << 8 | 0x7 << 4,
		fifo_id << 24 | (bitwidth - 1) << 8 | frddr_type << 4);

	offset = EE_AUDIO_SPDIFOUT_B_SWAP - EE_AUDIO_SPDIFOUT_SWAP;
	reg = EE_AUDIO_SPDIFOUT_SWAP + offset * spdif_id;
	audiobus_write(reg, swap_masks);
}

static bool spdifout_is_enable(int spdif_id)
{
	unsigned int offset, reg, val;

	offset = EE_AUDIO_SPDIFOUT_B_CTRL0 - EE_AUDIO_SPDIFOUT_CTRL0;
	reg = EE_AUDIO_SPDIFOUT_CTRL0 + offset * spdif_id;
	val = audiobus_read(reg);

	return ((val >> 31) == 1);
}

void spdifout_enable(int spdif_id, bool is_enable, bool reenable)
{
	unsigned int offset, reg;

	pr_debug("spdif_%s is set to %s\n",
		(spdif_id == 0) ? "a" : "b",
		is_enable ? "enable" : "disable");

	if (!is_enable) {
		/*
		 * must set ctrl1 bit 3 = 0 when disable
		 * share buffer, spdif should be active, so mute it
		 * audiobus_update_bits(reg, 0x3 << 21, 0x3 << 21);
		 */
		return;
	}
	offset = EE_AUDIO_SPDIFOUT_B_CTRL0 - EE_AUDIO_SPDIFOUT_CTRL0;
	reg = EE_AUDIO_SPDIFOUT_CTRL0 + offset * spdif_id;

	/* disable then for reset, to correct channel map */
	if (reenable)
		audiobus_update_bits(reg, 1 << 31, 0);

	/* reset afifo */
	audiobus_update_bits(reg, 3 << 28, 0);
	audiobus_update_bits(reg, 1 << 29, 1 << 29);
	audiobus_update_bits(reg, 1 << 28, 1 << 28);

	audiobus_update_bits(reg, 1 << 31, is_enable << 31);
}

void spdifout_samesource_set(int spdif_index, int fifo_id,
			     int bitwidth, int channels,
			     bool is_enable, int lane_i2s)
{
	int spdif_id;

	if (spdif_index == 1)
		spdif_id = 1;
	else
		spdif_id = 0;

	if (is_enable)
		spdifout_fifo_ctrl(spdif_id, fifo_id, bitwidth, channels, lane_i2s);
}

int spdifin_get_sample_rate(void)
{
	unsigned int val;
	/*EE_AUDIO_SPDIFIN_STAT0*/
	/*r_width_max bit17:8 (the max width of two edge;)*/
	unsigned int max_width = 0;

	val = audiobus_read(EE_AUDIO_SPDIFIN_STAT0);

	/* NA when check min width of two edges */
	if (((val >> 18) & 0x3ff) == 0x3ff)
		return 7;

	/*check the max width of two edge when spdifin sr=32kHz*/
	/*if max_width is more than 0x2f0(magic number),*/
	/*sr(32kHz) is unavailable*/
	max_width = ((val >> 8) & 0x3ff);

	if ((((val >> 28) & 0x7) == 0) && max_width == 0x3ff)
		return 7;

	return (val >> 28) & 0x7;
}

static int spdifin_get_channel_status(int sel)
{
	unsigned int val;

	/* set ch_status_sel to channel status */
	audiobus_update_bits(EE_AUDIO_SPDIFIN_CTRL0, 0xf << 8, sel << 8);

	val = audiobus_read(EE_AUDIO_SPDIFIN_STAT1);

	return val;
}

int spdifin_get_ch_status0to31(void)
{
	return spdifin_get_channel_status(0x0);
}

int spdifin_get_audio_type(void)
{
	unsigned int val;

	/* set ch_status_sel to read Pc */
	val = spdifin_get_channel_status(0x6);

	return (val >> 16) & 0xff;
}

void spdifin_set_src(int src)
{
	audiobus_update_bits(EE_AUDIO_SPDIFIN_CTRL0, 0x3 << 4, src << 4);
}

unsigned int spdif_get_channel_status0(int spdif_id)
{
	unsigned int offset, reg;

	offset = EE_AUDIO_SPDIFOUT_B_CHSTS0 - EE_AUDIO_SPDIFOUT_CHSTS0;
	reg = EE_AUDIO_SPDIFOUT_CHSTS0 + offset * spdif_id;

	return audiobus_read(reg);
}

void spdif_set_channel_status0(int spdif_id, unsigned int status0)
{
	unsigned int offset, reg;

	/* channel status a */
	offset = EE_AUDIO_SPDIFOUT_B_CHSTS0 - EE_AUDIO_SPDIFOUT_CHSTS0;
	reg = EE_AUDIO_SPDIFOUT_CHSTS0 + offset * spdif_id;
	audiobus_write(reg, status0);

	/* channel status b */
	offset = EE_AUDIO_SPDIFOUT_B_CHSTS6 - EE_AUDIO_SPDIFOUT_CHSTS6;
	reg = EE_AUDIO_SPDIFOUT_CHSTS6 + offset * spdif_id;
	audiobus_write(reg, status0);
}

void spdif_set_validity(bool en, int spdif_id)
{
	unsigned int offset, reg;

	offset = EE_AUDIO_SPDIFOUT_B_CTRL0 - EE_AUDIO_SPDIFOUT_CTRL0;
	reg = EE_AUDIO_SPDIFOUT_CTRL0 + offset * spdif_id;
	audiobus_update_bits(reg, 0x3 << 17, !!en << 17);
}

void spdif_set_channel_status_info(struct iec958_chsts *chsts, int spdif_id)
{
	unsigned int offset, reg;

	/* "ch status" = reg_chsts0~B */
	offset = EE_AUDIO_SPDIFOUT_B_CTRL0 - EE_AUDIO_SPDIFOUT_CTRL0;
	reg = EE_AUDIO_SPDIFOUT_CTRL0 + offset * spdif_id;
	audiobus_update_bits(reg, 0x1 << 24, 0x0 << 24);
	audiobus_update_bits(reg, 0x3 << 17, 1 << 17);

	/* channel status a */
	offset = EE_AUDIO_SPDIFOUT_B_CHSTS0 - EE_AUDIO_SPDIFOUT_CHSTS0;
	reg = EE_AUDIO_SPDIFOUT_CHSTS0 + offset * spdif_id;
	audiobus_write(reg, chsts->chstat1_l << 16 | chsts->chstat0_l);
	reg = EE_AUDIO_SPDIFOUT_CHSTS1 + offset * spdif_id;
	audiobus_write(reg, chsts->chstat2_l);

	/* channel status b */
	offset = EE_AUDIO_SPDIFOUT_B_CHSTS6 - EE_AUDIO_SPDIFOUT_CHSTS6;
	reg = EE_AUDIO_SPDIFOUT_CHSTS6 + offset * spdif_id;
	audiobus_write(reg, chsts->chstat1_r << 16 | chsts->chstat0_r);
	reg = EE_AUDIO_SPDIFOUT_CHSTS7 + offset * spdif_id;
	audiobus_write(reg, chsts->chstat2_r);
}

void spdifout_play_with_zerodata(unsigned int spdif_id, bool reenable, int separated)
{
	pr_debug("%s, spdif id:%d enable:%d\n", __func__,
		spdif_id, spdifout_is_enable(spdif_id));

	if (!spdifout_is_enable(spdif_id)) {
		unsigned int frddr_index = 0;
		unsigned int bitwidth = 32;
		unsigned int sample_rate = 48000;
		unsigned int src0_sel = 4; /* spdif b */
		struct iec958_chsts chsts;
		struct snd_pcm_substream substream;
		struct snd_pcm_runtime runtime;
		struct aud_para aud_param;

		memset(&aud_param, 0, sizeof(aud_param));

		substream.runtime = &runtime;
		runtime.rate = sample_rate;
		runtime.format = SNDRV_PCM_FORMAT_S16_LE;
		runtime.channels = 2;
		runtime.sample_bits = 16;

		aud_param.rate = runtime.rate;
		aud_param.size = runtime.sample_bits;
		aud_param.chs  = runtime.channels;

		/* check whether fix to spdif a */
		if (spdif_id == 0)
			src0_sel = 3;

		/* spdif clk */
		//spdifout_clk_ctrl(spdif_id, true);
		/* spdif to hdmitx */
		//spdifout_to_hdmitx_ctrl(separated, spdif_id);
		set_spdif_to_hdmitx_id(spdif_id);
		enable_spdifout_to_hdmitx(separated);

		/* spdif ctrl */
		spdifout_fifo_ctrl(spdif_id,
			frddr_index, bitwidth, runtime.channels, 0);

		/* channel status info */
		iec_get_channel_status_info(&chsts,
					    AUD_CODEC_TYPE_STEREO_PCM,
					    sample_rate, bitwidth, 0);
		spdif_set_channel_status_info(&chsts, spdif_id);
		spdif_set_validity(0, spdif_id);

		/* notify hdmitx audio */
		if (get_spdif_to_hdmitx_id() == spdif_id)
			aout_notifier_call_chain(AOUT_EVENT_IEC_60958_PCM, &aud_param);
		/* init frddr to output zero data. */
		frddr_init_without_mngr(frddr_index, src0_sel);

		/* spdif enable */
		spdifout_enable(spdif_id, true, reenable);
	}
}

void spdifout_play_with_zerodata_free(unsigned int spdif_id)
{
	pr_debug("%s, spdif id:%d\n",
		__func__,
		spdif_id);

	/* free frddr, then frddr in mngr */
	frddr_deinit_without_mngr(spdif_id);
}

void aml_spdif_out_reset(unsigned int spdif_id, int offset)
{
	unsigned int reg = 0, val = 0;

	if (offset && offset != 1) {
		pr_err("%s(), invalid offset = %d\n", __func__, offset);
		return;
	}

	if (spdif_id == 0) {
		reg = EE_AUDIO_SW_RESET0(offset);
		val = REG_BIT_RESET_SPDIFOUTA;
	} else if (spdif_id == 1) {
		reg = EE_AUDIO_SW_RESET0(offset);
		val = REG_BIT_RESET_SPDIFOUTB;
	} else {
		pr_err("invalid spdif id %d\n", spdif_id);
		return;
	}

	audiobus_update_bits(reg, val, val);
	audiobus_update_bits(reg, val, 0);
}

void aml_spdifin_sample_mode_filter_en(void)
{
	audiobus_update_bits(EE_AUDIO_SPDIFIN_CTRL6, 0x1 << 12, 0x1 << 12);
}

int get_spdif_to_hdmitx_id(void)
{
	int val = audiobus_read(EE_AUDIO_TOHDMITX_CTRL0) & 0x3;
	int ret = 0;

	if (val == 3)
		ret = 1;
	else if (val == 0)
		ret = 0;
	else
		pr_err("%s(), inval config\n", __func__);

	return ret;
}

void set_spdif_to_hdmitx_id(int spdif_id)
{
	audiobus_update_bits(EE_AUDIO_TOHDMITX_CTRL0,
			     0x3, spdif_id << 1 | spdif_id);
}

