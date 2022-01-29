// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#include <sound/soc.h>

#include "tdm_hw.h"
#include "iomap.h"
#include "tdm_gain_version.h"

#define MST_CLK_INVERT_PH0_PAD_BCLK       BIT(0)
#define MST_CLK_INVERT_PH0_PAD_FCLK       BIT(1)
#define MST_CLK_INVERT_PH1_TDMIN_BCLK     BIT(2)
#define MST_CLK_INVERT_PH1_TDMIN_FCLK     BIT(3)
#define MST_CLK_INVERT_PH2_TDMOUT_BCLK    BIT(4)
#define MST_CLK_INVERT_PH2_TDMOUT_FCLK    BIT(5)

void aml_tdmout_gain_step(int index, int enable)
{
	unsigned int reg, offset, reg1;
	int i;

	if (index >= 3) {
		reg = EE_AUDIO_TDMOUT_D_GAIN_CTRL;
	} else {
		offset = EE_AUDIO_TDMOUT_B_GAIN_CTRL - EE_AUDIO_TDMOUT_A_GAIN_CTRL;
		reg = EE_AUDIO_TDMOUT_A_GAIN_CTRL + offset * index;
	}
	audiobus_update_bits(reg, 0x1 << 31, (enable ? 1 : 0) << 31);

	if (index >= 3) {
		reg = EE_AUDIO_TDMOUT_D_GAIN0;
		reg1 = EE_AUDIO_TDMOUT_D_GAIN2;
	} else {
		offset = EE_AUDIO_TDMOUT_B_GAIN0 - EE_AUDIO_TDMOUT_A_GAIN0;
		reg = EE_AUDIO_TDMOUT_A_GAIN0 + offset * index;
		reg1 = EE_AUDIO_TDMOUT_A_GAIN2 + offset * index;
	}

	for (i = 0; i < 2; i++) {
		if (enable) {
			audiobus_write(reg + i, 0x0);
			audiobus_write(reg1 + i, 0x0);
		} else {
			audiobus_write(reg + i, 0xffffffff);
			audiobus_write(reg1 + i, 0xffffffff);
		}
	}
}

/* without audio handler, it should be improved */
void aml_tdm_enable(struct aml_audio_controller *actrl,
	int stream, int index,
	bool is_enable, bool fade_out, bool use_vadtop)
{
	unsigned int offset, reg;

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		pr_debug("tdm playback enable\n");

		if (is_enable && fade_out)
			aml_tdmout_gain_step(index, false);
		if (index >= 3) {
			reg = EE_AUDIO_TDMOUT_D_CTRL0;
		} else {
			offset = EE_AUDIO_TDMOUT_B_CTRL0
					- EE_AUDIO_TDMOUT_A_CTRL0;
			reg = EE_AUDIO_TDMOUT_A_CTRL0 + offset * index;
		}
		aml_audiobus_update_bits(actrl, reg, 1 << 31, is_enable << 31);
	} else {
		pr_debug("tdm capture enable\n");
		if (use_vadtop) {
			reg = EE_AUDIO2_TDMIN_VAD_CTRL;
			vad_top_update_bits(reg, 1 << 31 | 1 << 26, is_enable << 31 | 1 << 26);
		} else {
			if (index >= 3) {
				reg = EE_AUDIO_TDMIN_D_CTRL;
			} else {
				offset = EE_AUDIO_TDMIN_B_CTRL
						- EE_AUDIO_TDMIN_A_CTRL;
				reg = EE_AUDIO_TDMIN_A_CTRL + offset * index;
			}
			aml_audiobus_update_bits(actrl, reg, 1 << 31 | 1 << 26,
						 is_enable << 31 | 1 << 26);
		}
	}
}

void aml_tdm_arb_config(struct aml_audio_controller *actrl, bool use_arb)
{
	/* config ddr arb */
	if (use_arb)
		aml_audiobus_write(actrl, EE_AUDIO_ARB_CTRL, 1 << 31 | 0xff << 0);
}

void aml_tdm_fifo_reset(struct aml_audio_controller *actrl,
	int stream, int index, bool use_vadtop)
{
	unsigned int reg, offset;

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (index == 3) {
			reg = EE_AUDIO_TDMOUT_D_CTRL0;
		} else if (index < 3) {
			offset = EE_AUDIO_TDMOUT_B_CTRL0
					- EE_AUDIO_TDMOUT_A_CTRL0;
			reg = EE_AUDIO_TDMOUT_A_CTRL0 + offset * index;
		} else {
			reg = EE_AUDIO_TDMOUT_A_CTRL0;
		}
		/* reset afifo */
		aml_audiobus_update_bits(actrl, reg, 3 << 28, 0);
		aml_audiobus_update_bits(actrl, reg, 1 << 29, 1 << 29);
		aml_audiobus_update_bits(actrl, reg, 1 << 28, 1 << 28);
	} else {
		if (use_vadtop) {
			reg = EE_AUDIO2_TDMIN_VAD_CTRL;
			vad_top_update_bits(reg, 3 << 28, 0);
			vad_top_update_bits(reg, 1 << 29, 1 << 29);
			vad_top_update_bits(reg, 1 << 28, 1 << 28);
		} else {
			if (index == 3) {
				reg = EE_AUDIO_TDMIN_D_CTRL;
			} else if (index < 3) {
				offset = EE_AUDIO_TDMIN_B_CTRL
						- EE_AUDIO_TDMIN_A_CTRL;
				reg = EE_AUDIO_TDMIN_A_CTRL + offset * index;
			} else {
				reg = EE_AUDIO_TDMIN_A_CTRL;
			}
			/* reset afifo */
			aml_audiobus_update_bits(actrl, reg, 3 << 28, 0);
			aml_audiobus_update_bits(actrl, reg, 1 << 29, 1 << 29);
			aml_audiobus_update_bits(actrl, reg, 1 << 28, 1 << 28);
		}
	}
}

void tdm_enable(int tdm_index, int is_enable)
{
	unsigned int offset, reg;

	if (tdm_index < 3) {
		pr_info("tdmout is_enable:%d\n", is_enable);

		offset = EE_AUDIO_TDMOUT_B_CTRL0
			- EE_AUDIO_TDMOUT_A_CTRL0;
		reg = EE_AUDIO_TDMOUT_A_CTRL0 + offset * tdm_index;

		audiobus_update_bits(reg, 1 << 31, is_enable << 31);
	} else if (tdm_index < 6) {
		pr_info("tdmin is_enable:%d\n", is_enable);

		tdm_index -= 3;
		offset = EE_AUDIO_TDMIN_B_CTRL
			- EE_AUDIO_TDMIN_A_CTRL;
		reg = EE_AUDIO_TDMIN_A_CTRL + offset * tdm_index;

		audiobus_update_bits(reg, 1 << 31, is_enable << 31);
	}
}

void tdm_fifo_enable(int tdm_index, int is_enable)
{
	unsigned int reg, offset;
	if (tdm_index < 3) {
		offset = EE_AUDIO_TDMOUT_B_CTRL0
			- EE_AUDIO_TDMOUT_A_CTRL0;
		reg = EE_AUDIO_TDMOUT_A_CTRL0 + offset * tdm_index;

		if (is_enable) {
			audiobus_update_bits(reg, 1 << 29, 1 << 29);
			audiobus_update_bits(reg, 1 << 28, 1 << 28);
		} else {
			audiobus_update_bits(reg, 3 << 28, 0);
		}

	} else if (tdm_index < 6) {
		tdm_index -= 3;
		offset = EE_AUDIO_TDMIN_B_CTRL
				- EE_AUDIO_TDMIN_A_CTRL;
		reg = EE_AUDIO_TDMIN_A_CTRL + offset * tdm_index;

		if (is_enable) {
			audiobus_update_bits(reg, 1 << 29, 1 << 29);
			audiobus_update_bits(reg, 1 << 28, 1 << 28);
		} else {
			audiobus_update_bits(reg, 3 << 28, 0);
		}
	}
}

int tdmout_get_frddr_type(int bitwidth)
{
	unsigned int frddr_type = 0;

	switch (bitwidth) {
	case 8:
		frddr_type = 0;
		break;
	case 16:
		frddr_type = 2;
		break;
	case 24:
	case 32:
		frddr_type = 4;
		break;
	default:
		pr_err("invalid bit_depth: %d\n", bitwidth);
		break;
	}

	return frddr_type;
}

void aml_tdm_fifo_ctrl(struct aml_audio_controller *actrl,
	int bitwidth, int stream,
	int index, unsigned int fifo_id)
{
	unsigned int frddr_type = tdmout_get_frddr_type(bitwidth);
	unsigned int reg, offset;

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		pr_debug("tdm prepare----playback\n");
		// from ddr, 63bit split into 2 samples
		if (index == 3) {
			reg = EE_AUDIO_TDMOUT_D_CTRL1;
		} else if (index < 3) {
			offset = EE_AUDIO_TDMOUT_B_CTRL1
					- EE_AUDIO_TDMOUT_A_CTRL1;
			reg = EE_AUDIO_TDMOUT_A_CTRL1 + offset * index;
		} else {
			reg = EE_AUDIO_TDMOUT_A_CTRL1;
		}
		aml_audiobus_update_bits(actrl, reg,
				0x3 << 24 | 0x1f << 8 | 0x7 << 4,
				fifo_id << 24 | (bitwidth - 1) << 8 | frddr_type << 4);
	} else {
		pr_debug("tdm prepare----capture\n");
	}

}

static void aml_clk_set_tdmout_by_id(struct aml_audio_controller *actrl,
		unsigned int tdm_index,
		unsigned int sclk_sel,
		unsigned int lrclk_sel,
		bool sclk_ws_inv,
		bool is_master,
		bool binv)
{
	unsigned int val_sclk_ws_inv = 0;
	unsigned int reg;

	if (tdm_index == 3)
		reg = EE_AUDIO_CLK_TDMOUT_D_CTRL;
	else if (tdm_index < 3)
		reg = EE_AUDIO_CLK_TDMOUT_A_CTRL + tdm_index;
	else
		reg = EE_AUDIO_CLK_TDMOUT_A_CTRL;
	/* This is just a copy from previous setting. WHY??? */
	val_sclk_ws_inv = sclk_ws_inv && is_master;
	if (val_sclk_ws_inv)
		aml_audiobus_update_bits(actrl, reg,
			0x3 << 30 | 1 << 28 | 0xf << 24 | 0xf << 20,
			0x3 << 30 | val_sclk_ws_inv << 28 |
			sclk_sel << 24 | lrclk_sel << 20);
	else
		aml_audiobus_update_bits(actrl, reg,
			0x3 << 30 | 1 << 29 | 0xf << 24 | 0xf << 20,
			0x3 << 30 | binv << 29 |
			sclk_sel << 24 | lrclk_sel << 20);
}

static void aml_clk_set_tdmin_by_id(struct aml_audio_controller *actrl,
		unsigned int tdm_index,
		unsigned int sclk_sel,
		unsigned int lrclk_sel,
		bool use_vadtop)
{
	unsigned int reg;
	if (use_vadtop) {
		reg = EE_AUDIO2_CLK_TDMIN_VAD_CTRL;
		vad_top_update_bits(reg,
			0xff << 20,
			sclk_sel << 24 | lrclk_sel << 20);
	} else {
		if (tdm_index == 3)
			reg = EE_AUDIO_CLK_TDMIN_D_CTRL;
		else if (tdm_index < 3)
			reg = EE_AUDIO_CLK_TDMIN_A_CTRL + tdm_index;
		else
			reg = EE_AUDIO_CLK_TDMIN_A_CTRL;

		aml_audiobus_update_bits(actrl,
			reg,
			0xff << 20,
			sclk_sel << 24 | lrclk_sel << 20);
	}
}

static void aml_tdmout_invert_lrclk(struct aml_audio_controller *actrl,
		unsigned int tdm_index,
		bool finv)
{
	unsigned int reg_out;
	unsigned int off_set = EE_AUDIO_TDMOUT_B_CTRL1 - EE_AUDIO_TDMOUT_A_CTRL1;

	if (tdm_index == 3)
		reg_out = EE_AUDIO_TDMOUT_D_CTRL1;
	else if (tdm_index < 3)
		reg_out = EE_AUDIO_TDMOUT_A_CTRL1 + off_set * tdm_index;
	else
		reg_out = EE_AUDIO_TDMOUT_A_CTRL1;
	aml_audiobus_update_bits(actrl,
		reg_out, 0x1 << 28, finv << 28);
}

static void aml_tdmout_bclk_skew(struct aml_audio_controller *actrl,
		unsigned int tdm_index,
		unsigned int bclkout_skew)
{
	unsigned int reg_out;
	unsigned int off_set = EE_AUDIO_TDMOUT_B_CTRL0 - EE_AUDIO_TDMOUT_A_CTRL0;

	if (tdm_index == 3)
		reg_out = EE_AUDIO_TDMOUT_D_CTRL0;
	else if (tdm_index < 3)
		reg_out = EE_AUDIO_TDMOUT_A_CTRL0 + off_set * tdm_index;
	else
		reg_out = EE_AUDIO_TDMOUT_A_CTRL0;
	aml_audiobus_update_bits(actrl, reg_out, 0x1f << 15, bclkout_skew << 15);
}

void aml_tdm_set_format(struct aml_audio_controller *actrl,
	struct pcm_setting *p_config,
	unsigned int clk_sel,
	unsigned int index,
	unsigned int fmt,
	unsigned int capture_active,
	unsigned int playback_active,
	bool tdmin_src_hdmirx,
	bool use_vadtop)
{
	unsigned int binv, finv, id;
	unsigned int valb, valf;
	unsigned int reg_in, reg_out, off_set;
	int bclkin_skew, bclkout_skew;
	int master_mode;
	unsigned int clkctl = 0;

	id = index;

	binv = 0;
	finv = 0;

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		valb = SLAVE_A + clk_sel;
		valf = SLAVE_A + clk_sel;
		master_mode = 0;
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		if (use_vadtop) {
			valb = MASTER_A;
			valf = MASTER_A;
		} else {
			valb = MASTER_A + clk_sel;
			valf = MASTER_A + clk_sel;
		}
		master_mode = 1;
		break;
	default:
		return;
	}

	/*
	 * if tdmin source is hdmirx
	 * the clock is slave and from hdmirx
	 * slv_sclk_f  = HDMIRX_I2S_SCLK
	 */
	if (tdmin_src_hdmirx)
		aml_clk_set_tdmin_by_id(actrl, id, 11, 11, use_vadtop);
	else
		aml_clk_set_tdmin_by_id(actrl, id, valb, valf, use_vadtop);

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		if (p_config->sclk_ws_inv) {
			if (master_mode)
				bclkout_skew = 2;
			else
				bclkout_skew = 3;
		} else {
			bclkout_skew = 1;
		}
		bclkin_skew = 3;

		clkctl |= MST_CLK_INVERT_PH0_PAD_FCLK
			| MST_CLK_INVERT_PH2_TDMOUT_FCLK;
		finv = 1;

		if (master_mode) {
			clkctl |= MST_CLK_INVERT_PH0_PAD_BCLK;
			if (capture_active)
				binv |= 1;
		} else {
			if (playback_active)
				binv |= 1;
		}

		break;
	case SND_SOC_DAIFMT_DSP_A:
		/*
		 * Frame high, 1clk before data, one bit for frame sync,
		 * frame sync starts one serial clock cycle earlier,
		 * that is, together with the last bit of the previous
		 * data word.
		 */
		if (p_config->sclk_ws_inv) {
			if (master_mode)
				bclkout_skew = 2;
			else
				bclkout_skew = 3;
		} else {
			bclkout_skew = 1;
		}
		bclkin_skew = 3;

		if (capture_active)
			binv |= 1;
		break;
	case SND_SOC_DAIFMT_LEFT_J:
	case SND_SOC_DAIFMT_DSP_B:
		/*
		 * Frame high, one bit for frame sync,
		 * frame sync asserts with the first bit of the frame.
		 */
		if (p_config->sclk_ws_inv) {
			if (master_mode)
				bclkout_skew = 3;
			else
				bclkout_skew = 4;
		} else {
			bclkout_skew = 2;
		}
		bclkin_skew = 2;

		if (capture_active)
			binv |= 1;
		break;
	default:
		return;
	}

	p_config->pcm_mode = fmt & SND_SOC_DAIFMT_FORMAT_MASK;

	pr_debug("pad clk ctl value:%x\n", clkctl);
	/* set lrclk/bclk invertion */
	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_IB_IF:
		/* Invert both clocks */
		if (!master_mode)
			binv ^= 1;

		finv |= 1;
		clkctl ^= MST_CLK_INVERT_PH0_PAD_BCLK;
		clkctl ^= MST_CLK_INVERT_PH0_PAD_FCLK;
		break;
	case SND_SOC_DAIFMT_IB_NF:
		/* Invert bit clock */
		if (!master_mode)
			binv ^= 1;
		clkctl ^= MST_CLK_INVERT_PH0_PAD_BCLK;
		break;
	case SND_SOC_DAIFMT_NB_IF:
		/* Invert frame clock */
		finv ^= 1;
		clkctl ^= MST_CLK_INVERT_PH0_PAD_FCLK;
		break;
	case SND_SOC_DAIFMT_NB_NF:
		/* normal cases */
		break;
	default:
		return;
	}
	pr_debug("sclk_ph0 (pad) clk ctl set:%x\n", clkctl);
	/* clk ctrl: delay line and invert clk */
	/*clkctl |= 0x88880000;*/

	if (master_mode) {
		if (use_vadtop) {
			reg_out = EE_AUDIO2_MST_VAD_SCLK_CTRL1;
			vad_top_update_bits(reg_out, 0x3f, clkctl);
		} else {
			off_set = EE_AUDIO_MST_B_SCLK_CTRL1 - EE_AUDIO_MST_A_SCLK_CTRL1;
			reg_out = EE_AUDIO_MST_A_SCLK_CTRL1 + off_set * id;

			aml_audiobus_update_bits(actrl, reg_out, 0x3f, clkctl);
		}
	}

	pr_info("master_mode(%d), binv(%d), finv(%d) out_skew(%d), in_skew(%d)\n",
			master_mode, binv, finv, bclkout_skew, bclkin_skew);

	/* TDM out */
	if (playback_active) {
		aml_clk_set_tdmout_by_id(actrl,
			id, valb, valf,
			p_config->sclk_ws_inv, master_mode, binv);
		aml_tdmout_invert_lrclk(actrl, id, finv);
		aml_tdmout_bclk_skew(actrl, id, bclkout_skew);
	}

	/* TDM in */
	if (capture_active) {
		int mode;
		if (use_vadtop) {
			reg_in = EE_AUDIO2_CLK_TDMIN_VAD_CTRL;
			vad_top_update_bits(reg_in,
				0x3 << 30, 0x3 << 30);
			if (master_mode)
				vad_top_update_bits(reg_in,
					0x1 << 29, binv << 29);

			reg_in = EE_AUDIO2_TDMIN_VAD_CTRL;
			vad_top_update_bits(reg_in,
				3 << 26 | 0x7 << 16, 3 << 26 | bclkin_skew << 16);

			vad_top_update_bits(reg_in,
				0x1 << 25, finv << 25);

			mode = (p_config->pcm_mode == SND_SOC_DAIFMT_I2S) ? 0x1 : 0x0;
			vad_top_update_bits(reg_in, 0x1 << 30, mode << 30);
		} else {
			if (id == 3)
				reg_in = EE_AUDIO_CLK_TDMIN_D_CTRL;
			else if (id < 3)
				reg_in = EE_AUDIO_CLK_TDMIN_A_CTRL + id;
			else
				reg_in = EE_AUDIO_CLK_TDMIN_A_CTRL;
			aml_audiobus_update_bits(actrl, reg_in,
				0x3 << 30, 0x3 << 30);

			if (master_mode)
				aml_audiobus_update_bits(actrl, reg_in,
					0x1 << 29, binv << 29);
			if (id == 3) {
				reg_in = EE_AUDIO_TDMIN_D_CTRL;
			} else if (id < 3) {
				off_set = EE_AUDIO_TDMIN_B_CTRL - EE_AUDIO_TDMIN_A_CTRL;
				reg_in = EE_AUDIO_TDMIN_A_CTRL + off_set * id;
			} else {
				reg_in = EE_AUDIO_TDMIN_A_CTRL;
			}
			aml_audiobus_update_bits(actrl, reg_in,
				3 << 26 | 0x7 << 16, 3 << 26 | bclkin_skew << 16);

			aml_audiobus_update_bits(actrl, reg_in,
				0x1 << 25, finv << 25);

			mode = (p_config->pcm_mode == SND_SOC_DAIFMT_I2S) ? 0x1 : 0x0;
			aml_audiobus_update_bits(actrl, reg_in, 0x1 << 30, mode << 30);
		}
	}
}

void aml_update_tdmin_skew(struct aml_audio_controller *actrl,
	 int idx, int skew, bool use_vadtop)
{
	unsigned int reg_in, off_set;
	if (use_vadtop) {
		reg_in = EE_AUDIO2_TDMIN_VAD_CTRL;
		vad_top_update_bits(reg_in,
			0x7 << 16, skew << 16);
	} else {
		if (idx == 3) {
			reg_in = EE_AUDIO_TDMIN_D_CTRL;
		} else if (idx < 3) {
			off_set = EE_AUDIO_TDMIN_B_CTRL - EE_AUDIO_TDMIN_A_CTRL;
			reg_in = EE_AUDIO_TDMIN_A_CTRL + off_set * idx;
		} else {
			reg_in = EE_AUDIO_TDMIN_A_CTRL;
		}
		aml_audiobus_update_bits(actrl, reg_in,
			0x7 << 16, skew << 16);
	}
}

void aml_update_tdmin_rev_ws(struct aml_audio_controller *actrl,
	 int idx, int is_rev, bool use_vadtop)
{
	unsigned int reg_in, off_set;
	if (use_vadtop) {
		reg_in = EE_AUDIO2_TDMIN_VAD_CTRL;
		vad_top_update_bits(reg_in, 0x1 << 25, is_rev << 25);
	} else {
		if (idx == 3) {
			reg_in = EE_AUDIO_TDMIN_D_CTRL;
		} else if (idx < 3) {
			off_set = EE_AUDIO_TDMIN_B_CTRL - EE_AUDIO_TDMIN_A_CTRL;
			reg_in = EE_AUDIO_TDMIN_A_CTRL + off_set * idx;
		} else {
			reg_in = EE_AUDIO_TDMIN_A_CTRL;
		}
		aml_audiobus_update_bits(actrl, reg_in, 0x1 << 25, is_rev << 25);
	}
}

void aml_tdm_set_oe_v1(struct aml_audio_controller *actrl,
	int index, int force_oe, int oe_val)
{
	if (force_oe) {
		unsigned int reg, offset;
		if (index == 3) {
			reg = EE_AUDIO_TDMOUT_D_CTRL0;
		} else if (index < 3) {
			offset = EE_AUDIO_TDMOUT_B_CTRL0 - EE_AUDIO_TDMOUT_A_CTRL0;
			reg = EE_AUDIO_TDMOUT_A_CTRL0 + offset * index;
		} else {
			reg = EE_AUDIO_TDMOUT_A_CTRL0;
		}
		aml_audiobus_update_bits(actrl, reg, 0xf << 24, force_oe << 24);

		/* force oe val, in or out */
		if (index == 3)
			reg = EE_AUDIO_TDMOUT_D_CTRL1;
		else if (index < 3)
			reg = EE_AUDIO_TDMOUT_A_CTRL1 + offset * index;
		else
			reg = EE_AUDIO_TDMOUT_A_CTRL1;
		aml_audiobus_update_bits(actrl, reg, 0xf, oe_val);
	}
}

void aml_tdm_set_oe_v2(struct aml_audio_controller *actrl,
	int index, int force_oe, int oe_val)
{
	if (force_oe) {
		unsigned int reg, offset;
		if (index == 3) {
			reg = EE_AUDIO_TDMOUT_D_CTRL2;
		} else if (index < 3) {
			offset = EE_AUDIO_TDMOUT_B_CTRL2 - EE_AUDIO_TDMOUT_A_CTRL2;
			reg = EE_AUDIO_TDMOUT_A_CTRL2 + offset * index;
		} else {
			reg = EE_AUDIO_TDMOUT_A_CTRL2;
		}
		aml_audiobus_update_bits(actrl, reg, 0xff << 8, force_oe << 8);
		/* force oe val, in or out */
		if (oe_val) {
			aml_audiobus_update_bits
				(actrl, reg, 0xff << 16, oe_val << 16);
		}
	}
}

void aml_tdm_set_slot_out(struct aml_audio_controller *actrl,
	int index, int slots, int slot_width)
{
	unsigned int reg, offset;
	if (index == 3) {
		reg = EE_AUDIO_TDMOUT_D_CTRL0;
	} else if (index < 3) {
		offset = EE_AUDIO_TDMOUT_B_CTRL0 - EE_AUDIO_TDMOUT_A_CTRL0;
		reg = EE_AUDIO_TDMOUT_A_CTRL0 + offset * index;
	} else {
		reg = EE_AUDIO_TDMOUT_A_CTRL0;
	}
	aml_audiobus_update_bits
		(actrl, reg, 0x3ff, ((slots - 1) << 5) | (slot_width - 1));
}

void aml_tdm_set_slot_in(struct aml_audio_controller *actrl,
	int index, int in_src, int slot_width, bool use_vadtop)
{
	unsigned int reg, offset;
	if (use_vadtop) {
		reg = EE_AUDIO2_TDMIN_VAD_CTRL;
		vad_top_update_bits(reg,
			0xf << 20 | 0x1f, 0 << 20 | (slot_width - 1));
	} else {
		if (index == 3) {
			reg = EE_AUDIO_TDMIN_D_CTRL;
		} else if (index < 3) {
			offset = EE_AUDIO_TDMIN_B_CTRL - EE_AUDIO_TDMIN_A_CTRL;
			reg = EE_AUDIO_TDMIN_A_CTRL + offset * index;
		} else {
			reg = EE_AUDIO_TDMIN_A_CTRL;
		}
		aml_audiobus_update_bits(actrl, reg,
			0xf << 20 | 0x1f, in_src << 20 | (slot_width - 1));
	}
}

void aml_update_tdmin_src(struct aml_audio_controller *actrl,
	int index, int in_src, bool use_vadtop)
{
	unsigned int reg, offset;
	if (use_vadtop) {
		reg = EE_AUDIO2_TDMIN_VAD_CTRL;
		vad_top_update_bits(reg, 0xf << 20, 0 << 20);
	} else {
		if (index == 3) {
			reg = EE_AUDIO_TDMIN_D_CTRL;
		} else if (index < 3) {
			offset = EE_AUDIO_TDMIN_B_CTRL - EE_AUDIO_TDMIN_A_CTRL;
			reg = EE_AUDIO_TDMIN_A_CTRL + offset * index;
		} else {
			reg = EE_AUDIO_TDMIN_A_CTRL;
		}
		aml_audiobus_update_bits(actrl, reg, 0xf << 20, in_src << 20);
	}
}

void tdmin_set_chnum_en(struct aml_audio_controller *actrl,
	int index, bool enable, bool use_vadtop)
{
	unsigned int reg, offset;
	if (use_vadtop) {
		reg = EE_AUDIO2_TDMIN_VAD_CTRL;
		vad_top_update_bits(reg, 0x1 << 6, enable << 6);
	} else {
		if (index == 3) {
			reg = EE_AUDIO_TDMIN_D_CTRL;
		} else if (index < 3) {
			offset = EE_AUDIO_TDMIN_B_CTRL - EE_AUDIO_TDMIN_A_CTRL;
			reg = EE_AUDIO_TDMIN_A_CTRL + offset * index;
		} else {
			reg = EE_AUDIO_TDMIN_A_CTRL;
		}
		aml_audiobus_update_bits(actrl, reg,
			0x1 << 6, enable << 6);
	}
}

void aml_tdm_set_channel_mask(struct aml_audio_controller *actrl,
	int stream, int index, int lane, int mask, bool use_vadtop)
{
	unsigned int offset, reg;

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (lane >= LANE_MAX1) {
			if (index == 3) {
				reg = EE_AUDIO_TDMOUT_D_MASK4;
			} else if (index < 3) {
				offset = EE_AUDIO_TDMOUT_B_MASK4
					- EE_AUDIO_TDMOUT_A_MASK4;
				reg = EE_AUDIO_TDMOUT_A_MASK4 + offset * index;
			} else {
				reg = EE_AUDIO_TDMOUT_A_MASK4;
			}
			lane -= LANE_MAX1;
		} else {
			if (index == 3) {
				reg = EE_AUDIO_TDMOUT_D_MASK0;
			} else if (index < 3) {
				offset = EE_AUDIO_TDMOUT_B_MASK0
					- EE_AUDIO_TDMOUT_A_MASK0;
				reg = EE_AUDIO_TDMOUT_A_MASK0 + offset * index;
			} else {
				reg = EE_AUDIO_TDMOUT_A_MASK0;
			}
		}
	} else {
		if (use_vadtop) {
			if (lane >= LANE_MAX1) {
				reg = EE_AUDIO2_TDMIN_VAD_MASK4;
				lane -= LANE_MAX1;
			} else {
				reg = EE_AUDIO2_TDMIN_VAD_MASK0;
			}
			vad_top_write(reg + lane, mask);
			return;
		} else {
			if (lane >= LANE_MAX1) {
				if (index == 3) {
					reg = EE_AUDIO_TDMIN_D_MASK4;
				} else if (index < 3) {
					offset = EE_AUDIO_TDMIN_B_MASK4
						- EE_AUDIO_TDMIN_A_MASK4;
					reg = EE_AUDIO_TDMIN_A_MASK4 + offset * index;
				} else {
					reg = EE_AUDIO_TDMIN_A_MASK4;
				}
				lane -= LANE_MAX1;
			} else {
				if (index == 3) {
					reg = EE_AUDIO_TDMIN_D_MASK0;
				} else if (index < 3) {
					offset = EE_AUDIO_TDMIN_B_MASK0
						- EE_AUDIO_TDMIN_A_MASK0;
					reg = EE_AUDIO_TDMIN_A_MASK0 + offset * index;
				} else {
					reg = EE_AUDIO_TDMIN_A_MASK0;
				}
			}
		}
	}
	aml_audiobus_write(actrl, reg + lane, mask);
}

void aml_tdm_set_lane_channel_swap(struct aml_audio_controller *actrl,
	int stream, int index, int swap0, int swap1, bool use_vadtop)
{
	unsigned int offset, reg;

	pr_debug("\t %s swap0 = %#x, swap1 = %#x\n",
		(stream == SNDRV_PCM_STREAM_PLAYBACK) ? "tdmout" : "tdmin",
		swap0, swap1);

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (index == 3) {
			reg = EE_AUDIO_TDMOUT_D_SWAP0;
		} else if (index < 3) {
			offset = EE_AUDIO_TDMOUT_B_SWAP0
				- EE_AUDIO_TDMOUT_A_SWAP0;
			reg = EE_AUDIO_TDMOUT_A_SWAP0 + offset * index;
		} else {
			reg = EE_AUDIO_TDMOUT_A_SWAP0;
		}
		aml_audiobus_write(actrl, reg, swap0);

		if (swap1) {
			if (index == 3) {
				reg = EE_AUDIO_TDMOUT_D_SWAP1;
			} else if (index < 3) {
				offset = EE_AUDIO_TDMOUT_B_SWAP1
					- EE_AUDIO_TDMOUT_A_SWAP1;
				reg = EE_AUDIO_TDMOUT_A_SWAP1 + offset * index;
			} else {
				reg = EE_AUDIO_TDMOUT_A_SWAP1;
			}
			aml_audiobus_write(actrl, reg, swap1);
		}
	} else {
		if (use_vadtop) {
			reg = EE_AUDIO2_TDMIN_VAD_SWAP0;
			vad_top_write(reg, swap0);
			if (swap1) {
				reg = EE_AUDIO2_TDMIN_VAD_SWAP1;
				vad_top_write(reg, swap1);
			}
			return;
		} else {
			if (index == 3) {
				reg = EE_AUDIO_TDMIN_D_SWAP0;
			} else if (index < 3) {
				offset = EE_AUDIO_TDMIN_B_SWAP0
					- EE_AUDIO_TDMIN_A_SWAP0;
				reg = EE_AUDIO_TDMIN_A_SWAP0 + offset * index;
			} else {
				reg = EE_AUDIO_TDMIN_A_SWAP0;
			}
			aml_audiobus_write(actrl, reg, swap0);

			if (swap1) {
				if (index == 3) {
					reg = EE_AUDIO_TDMIN_D_SWAP1;
				} else if (index < 3) {
					offset = EE_AUDIO_TDMIN_B_SWAP1
						- EE_AUDIO_TDMIN_A_SWAP1;
					reg = EE_AUDIO_TDMIN_A_SWAP1 + offset * index;
				} else {
					reg = EE_AUDIO_TDMIN_A_SWAP1;
				}
				aml_audiobus_write(actrl, reg, swap1);
			}
		}
	}
}

void aml_tdm_set_bclk_ratio(struct aml_audio_controller *actrl,
	int clk_sel, int lrclk_hi, int bclk_ratio, bool use_vadtop)
{
	unsigned int reg, reg_step = 2;
	if (use_vadtop) {
		reg = EE_AUDIO2_MST_VAD_SCLK_CTRL0;
		vad_top_update_bits(reg,
					(3 << 30) | 0x3ff << 10 | 0x3ff,
					(3 << 30) | lrclk_hi << 10 | bclk_ratio);
	} else {
		reg = EE_AUDIO_MST_A_SCLK_CTRL0 + reg_step * clk_sel;

		aml_audiobus_update_bits(actrl, reg,
					(3 << 30) | 0x3ff << 10 | 0x3ff,
					(3 << 30) | lrclk_hi << 10 | bclk_ratio);
	}
}

void aml_tdm_set_lrclkdiv(struct aml_audio_controller *actrl,
	int clk_sel, int ratio, bool use_vadtop)
{
	unsigned int reg, reg_step = 2;

	pr_debug("aml_dai_set_clkdiv, clksel(%d), ratio(%d)\n",
			clk_sel, ratio);
	if (use_vadtop) {
		reg = EE_AUDIO2_MST_VAD_SCLK_CTRL0;
		vad_top_update_bits(reg,
			(3 << 30) | (0x3ff << 20),
			(3 << 30) | (ratio << 20));
	} else {
		reg = EE_AUDIO_MST_A_SCLK_CTRL0 + reg_step * clk_sel;

		aml_audiobus_update_bits(actrl, reg,
			(3 << 30) | (0x3ff << 20),
			(3 << 30) | (ratio << 20));
	}
}

void aml_tdmout_select_aed(bool enable, int tdmout_id)
{
	unsigned int reg, offset;
	if (tdmout_id == 3) {
		reg = EE_AUDIO_TDMOUT_D_CTRL1;
	} else if (tdmout_id < 3) {
		/* select eq_drc output */
		offset = EE_AUDIO_TDMOUT_B_CTRL1
				- EE_AUDIO_TDMOUT_A_CTRL1;
		reg = EE_AUDIO_TDMOUT_A_CTRL1 + offset * tdmout_id;
	} else {
		reg = EE_AUDIO_TDMOUT_A_CTRL1;
	}
	audiobus_update_bits(reg, 0x1 << 31, enable << 31);
}

void aml_tdmout_get_aed_info(int tdmout_id,
	int *bitwidth, int *frddrtype)
{
	unsigned int reg, offset, val;
	if (tdmout_id == 3) {
		reg = EE_AUDIO_TDMOUT_D_CTRL1;
	} else if (tdmout_id < 3) {
		offset = EE_AUDIO_TDMOUT_B_CTRL1
				- EE_AUDIO_TDMOUT_A_CTRL1;
		reg = EE_AUDIO_TDMOUT_A_CTRL1 + offset * tdmout_id;
	} else {
		reg = EE_AUDIO_TDMOUT_A_CTRL1;
	}
	val = audiobus_read(reg);
	if (bitwidth)
		*bitwidth = (val >> 8) & 0x1f;
	if (frddrtype)
		*frddrtype = (val >> 4) & 0x7;
}

void aml_tdmout_enable_gain(int tdmout_id, int en, int gain_ver)
{
	unsigned int reg, offset;

	switch (gain_ver) {
	case GAIN_VER1:
		offset = EE_AUDIO_TDMOUT_B_CTRL1
				- EE_AUDIO_TDMOUT_A_CTRL1;
		reg = EE_AUDIO_TDMOUT_A_CTRL1 + offset * tdmout_id;
		audiobus_update_bits(reg, 0x1 << 26, !!en << 26);
		break;
	case GAIN_VER2:
		offset = EE_AUDIO_TDMOUT_B_CTRL1
				- EE_AUDIO_TDMOUT_A_CTRL1;
		reg = EE_AUDIO_TDMOUT_A_CTRL1 + offset * tdmout_id;
		audiobus_update_bits(reg, 0x1 << 7, !!en << 7);
		break;
	case GAIN_VER3:
		if (tdmout_id == 3) {
			reg = EE_AUDIO_TDMOUT_D_GAIN_EN;
		} else {
			offset = EE_AUDIO_TDMOUT_B_GAIN_EN - EE_AUDIO_TDMOUT_A_GAIN_EN;
			reg = EE_AUDIO_TDMOUT_A_GAIN_EN + offset * tdmout_id;
		}
		if (en)
			audiobus_update_bits(reg, 0xFF << 0, 0xFF << 0);
		else
			audiobus_update_bits(reg, 0xFF << 0, 0x0 << 0);

		break;
	}
}

void aml_tdm_mclk_pad_select(struct aml_audio_controller *actrl,
			     int mpad, int mpad_offset, int mclk_sel)
{
	unsigned int reg, mask_offset, val_offset;

	switch (mpad) {
	case 0:
		mask_offset = 0x7 << 0;
		val_offset = mclk_sel << 0;
		break;
	case 1:
		mask_offset = 0x7 << 4;
		val_offset = mclk_sel << 4;
		break;
	default:
		mask_offset = 0x7 << 4;
		val_offset = 0;
		pr_info("unknown tdm mpad:%d\n", mpad);
		break;
	}

	reg = EE_AUDIO_MST_PAD_CTRL0(mpad_offset);
	if (actrl)
		aml_audiobus_update_bits(actrl, reg,
					 mask_offset, val_offset);
	else
		audiobus_update_bits(reg,
				     mask_offset, val_offset);
}

void aml_tdm_sclk_pad_select(struct aml_audio_controller *actrl,
			     int mpad_offset, int tdm_index, int clk_sel)
{
	unsigned int reg, mask_offset, val_offset;

	reg = EE_AUDIO_MST_PAD_CTRL1(mpad_offset);
	switch (tdm_index) {
	case 0:
		mask_offset = 0x7 << 16 | 0x7 << 0;
		val_offset = clk_sel << 16 | clk_sel << 0;
		break;
	case 1:
		mask_offset = 0x7 << 20 | 0x7 << 4;
		val_offset = clk_sel << 20 | clk_sel << 4;
		break;
	case 2:
		mask_offset = 0x7 << 24 | 0x7 << 8;
		val_offset = clk_sel << 24 | clk_sel << 8;
		break;
	default:
		pr_err("unknown mclk pad, tdm index:%d\n", tdm_index);
		return;
	}
	if (actrl)
		aml_audiobus_update_bits(actrl, reg, mask_offset, val_offset);
	else
		audiobus_update_bits(reg, mask_offset, val_offset);
}

void i2s_to_hdmitx_ctrl(int i2s_tohdmitxen_separated, int tdm_index)
{
	audiobus_write(EE_AUDIO_TOHDMITX_CTRL0,
		tdm_index << 12 /* dat_sel */
		| tdm_index << 8 /* lrclk_sel */
		| 1 << 7 /* Bclk_cap_inv */
		| 0 << 6 /* Bclk_o_inv */
		| tdm_index << 4 /* Bclk_sel */
	);

	if (i2s_tohdmitxen_separated) {
		/* if tohdmitx_en is separated, need do:
		 * step1: enable/disable clk
		 * step2: enable/disable dat
		 */
		audiobus_update_bits(EE_AUDIO_TOHDMITX_CTRL0,
				     0x1 << 28, 0x1 << 28);
		audiobus_update_bits(EE_AUDIO_TOHDMITX_CTRL0,
				     0x1 << 29, 0x1 << 29);
	} else {
		audiobus_update_bits(EE_AUDIO_TOHDMITX_CTRL0,
				     0x1 << 31, 0x1 << 31);
	}
}

void aml_tdm_mute_playback(struct aml_audio_controller *actrl,
		int tdm_index, bool mute, int lane_cnt)
{
	unsigned int offset, reg;
	unsigned int mute_mask = 0xffffffff;
	unsigned int mute_val = 0;
	int i = 0;

	if (mute)
		mute_val = 0xffffffff;
	if (tdm_index == 3) {
		reg = EE_AUDIO_TDMOUT_D_MUTE0;
	} else if (tdm_index < 3) {
		offset = EE_AUDIO_TDMOUT_B_MUTE0
				- EE_AUDIO_TDMOUT_A_MUTE0;
		reg = EE_AUDIO_TDMOUT_A_MUTE0 + offset * tdm_index;
	} else {
		reg = EE_AUDIO_TDMOUT_A_MUTE0;
	}
	for (i = 0; i < LANE_MAX1; i++)
		aml_audiobus_update_bits(actrl, reg + i, mute_mask, mute_val);

	if (lane_cnt > LANE_MAX1) {
		if (tdm_index == 3) {
			reg = EE_AUDIO_TDMOUT_D_MUTE4;
		} else if (tdm_index < 3) {
			offset = EE_AUDIO_TDMOUT_B_MUTE4
					- EE_AUDIO_TDMOUT_A_MUTE4;
			reg = EE_AUDIO_TDMOUT_A_MUTE4 + offset * tdm_index;
		} else {
			reg = EE_AUDIO_TDMOUT_A_MUTE4;
		}
		for (i = 0; i < LANE_MAX1; i++)
			aml_audiobus_update_bits(actrl, reg + i,
					mute_mask, mute_val);
	}
}

void aml_tdm_mute_capture(struct aml_audio_controller *actrl,
		int tdm_index, bool mute, int lane_cnt, bool use_vadtop)
{
	unsigned int offset, reg;
	unsigned int mute_mask = 0xffffffff;
	unsigned int mute_val = 0;
	int i = 0;

	if (mute)
		mute_val = 0xffffffff;
	if (use_vadtop) {
		reg = EE_AUDIO2_TDMIN_VAD_MUTE0;
		for (i = 0; i < LANE_MAX1; i++)
			vad_top_update_bits(reg + i,
					mute_mask, mute_val);
	} else {
		if (tdm_index == 3) {
			reg = EE_AUDIO_TDMIN_D_MUTE0;
		} else if (tdm_index < 3) {
			offset = EE_AUDIO_TDMIN_B_MUTE0
					- EE_AUDIO_TDMIN_A_MUTE0;
			reg = EE_AUDIO_TDMIN_A_MUTE0 + offset * tdm_index;
		} else {
			reg = EE_AUDIO_TDMIN_A_MUTE0;
		}
		for (i = 0; i < LANE_MAX1; i++)
			aml_audiobus_update_bits(actrl, reg + i,
					mute_mask, mute_val);
	}
	if (lane_cnt > LANE_MAX1) {
		if (use_vadtop) {
			reg = EE_AUDIO2_TDMIN_VAD_MUTE4;
			for (i = 0; i < LANE_MAX1; i++)
				vad_top_update_bits(reg + i,
						mute_mask, mute_val);
		} else {
			if (tdm_index == 3) {
				reg = EE_AUDIO_TDMIN_D_MUTE4;
			} else if (tdm_index < 3) {
				offset = EE_AUDIO_TDMIN_B_MUTE4
						- EE_AUDIO_TDMIN_A_MUTE4;
				reg = EE_AUDIO_TDMIN_A_MUTE4 + offset * tdm_index;
			} else {
				reg = EE_AUDIO_TDMIN_A_MUTE4;
			}
			for (i = 0; i < LANE_MAX1; i++)
				aml_audiobus_update_bits(actrl, reg + i,
					mute_mask, mute_val);
		}
	}
}

void aml_tdm_out_reset(unsigned int tdm_id, int offset)
{
	unsigned int reg = 0, val = 0;

	if (offset && offset != 1) {
		pr_err("%s(), invalid offset = %d\n", __func__, offset);
		return;
	}

	if (tdm_id == 0) {
		reg = EE_AUDIO_SW_RESET0(offset);
		val = REG_BIT_RESET_TDMOUTA;
	} else if (tdm_id == 1) {
		reg = EE_AUDIO_SW_RESET0(offset);
		val = REG_BIT_RESET_TDMOUTB;
	} else if (tdm_id == 2) {
		reg = EE_AUDIO_SW_RESET0(offset);
		val = REG_BIT_RESET_TDMOUTC;
	} else if (tdm_id == 3) {
		reg = EE_AUDIO_SW_RESET1(offset);
		val = REG_BIT_RESET_TDMOUTD;
	} else {
		pr_err("invalid tdmout id %d\n", tdm_id);
		return;
	}
	audiobus_update_bits(reg, val, val);
	audiobus_update_bits(reg, val, 0);
}

void aml_tdmout_auto_gain_enable(unsigned int tdm_id)
{
	unsigned int reg, offset;

	if (tdm_id >= 3) {
		reg = EE_AUDIO_TDMOUT_D_GAIN_EN;
	} else {
		offset = EE_AUDIO_TDMOUT_B_GAIN_EN - EE_AUDIO_TDMOUT_A_GAIN_EN;
		reg = EE_AUDIO_TDMOUT_A_GAIN_EN + offset * tdm_id;
	}
	/* 0 - 8 channel */
	audiobus_update_bits(reg, 0xFF << 0, 0xFF << 0);
	if (tdm_id >= 3) {
		reg = EE_AUDIO_TDMOUT_D_GAIN_CTRL;
	} else {
		offset = EE_AUDIO_TDMOUT_B_GAIN_CTRL - EE_AUDIO_TDMOUT_A_GAIN_CTRL;
		reg = EE_AUDIO_TDMOUT_A_GAIN_CTRL + offset * tdm_id;
	}
	/*
	 * bit 31: step by step change gain
	 * bit 16 - 23: gain_step
	 * bit 0 - 15: the period of each change, unit is FS
	 */
	audiobus_update_bits(reg,
			     0x1 << 31 | 0xFF << 16 | 0xFFFF << 0,
			     0x1 << 31 | 0x05 << 16 | 0x000a << 0);
}

void aml_tdmout_set_gain(int tdmout_id, int value)
{
	unsigned int reg, offset;
	int i;
	if (tdmout_id == 3) {
		reg = EE_AUDIO_TDMOUT_D_GAIN0;
	} else if (tdmout_id < 3) {
		offset = EE_AUDIO_TDMOUT_B_GAIN0 - EE_AUDIO_TDMOUT_A_GAIN0;
		reg = EE_AUDIO_TDMOUT_A_GAIN0 + offset * tdmout_id;
	} else {
		reg = EE_AUDIO_TDMOUT_A_GAIN0;
	}
	/* channel 0 - channel 8 */
	for (i = 0; i < 2; i++)
		audiobus_write(reg + i,
			       value << 24
			       | value << 16
			       | value << 8
			       | value << 0);
}

int aml_tdmout_get_gain(int tdmout_id)
{
	unsigned int reg, offset;
	int value;
	if (tdmout_id == 3) {
		reg = EE_AUDIO_TDMOUT_D_GAIN0;
	} else if (tdmout_id < 3) {
		offset = EE_AUDIO_TDMOUT_B_GAIN0 - EE_AUDIO_TDMOUT_A_GAIN0;
		reg = EE_AUDIO_TDMOUT_A_GAIN0 + offset * tdmout_id;
	} else {
		reg = EE_AUDIO_TDMOUT_A_GAIN0;
	}
	value = audiobus_read(reg);
	return value & 0xFF;
}

void aml_tdmout_set_mute(int tdmout_id, int mute)
{
	unsigned int reg, offset, value;
	int i;

	if (mute)
		value = 0xFFFFFFFF;
	else
		value = 0x0;
	if (tdmout_id == 3) {
		reg = EE_AUDIO_TDMOUT_D_MUTE0;
	} else if (tdmout_id < 3) {
		offset = EE_AUDIO_TDMOUT_B_MUTE0 - EE_AUDIO_TDMOUT_A_MUTE0;
		reg = EE_AUDIO_TDMOUT_A_MUTE0 + offset * tdmout_id;
	} else {
		reg = EE_AUDIO_TDMOUT_A_MUTE0;
	}
	/* lane0 - lane3 */
	for (i = 0; i < 4; i++)
		audiobus_write(reg + i, value);
	if (tdmout_id == 3) {
		reg = EE_AUDIO_TDMOUT_D_MUTE4;
	} else if (tdmout_id < 3) {
		offset = EE_AUDIO_TDMOUT_B_MUTE4 - EE_AUDIO_TDMOUT_A_MUTE4;
		reg = EE_AUDIO_TDMOUT_A_MUTE4 + offset * tdmout_id;
	} else {
		reg = EE_AUDIO_TDMOUT_A_MUTE4;
	}
	/* lane4 - lane7 */
	for (i = 0; i < 4; i++)
		audiobus_write(reg + i, value);
}

int aml_tdmout_get_mute(int tdmout_id)
{
	unsigned int reg, offset;
	int value;
	if (tdmout_id == 3) {
		reg = EE_AUDIO_TDMOUT_D_MUTE0;
	} else if (tdmout_id < 3) {
		offset = EE_AUDIO_TDMOUT_B_MUTE0 - EE_AUDIO_TDMOUT_A_MUTE0;
		reg = EE_AUDIO_TDMOUT_A_MUTE0 + offset * tdmout_id;
	} else {
		reg = EE_AUDIO_TDMOUT_A_MUTE0;
	}
	value = audiobus_read(reg);

	return value & 0x1;
}

int aml_tdmin_get_status(int tdm_id, bool use_vadtop)
{
	unsigned int reg, offset;

	if (use_vadtop) {
		reg = EE_AUDIO2_TDMIN_VAD_STAT;
		return vad_top_read(reg);
	} else {
		if (tdm_id == 3) {
			reg = EE_AUDIO_TDMIN_D_STAT;
		} else if (tdm_id < 3) {
			offset = EE_AUDIO_TDMIN_B_STAT - EE_AUDIO_TDMIN_A_STAT;
			reg = EE_AUDIO_TDMIN_A_STAT + offset * tdm_id;
		} else {
			reg = EE_AUDIO_TDMIN_A_STAT;
		}
		return audiobus_read(reg);
	}
}

void aml_tdmin_set_slot_num(struct aml_audio_controller *actrl,
			    int index, int slot_num, bool use_vadtop)
{
	unsigned int reg, offset;
	if (use_vadtop) {
		reg = EE_AUDIO2_TDMIN_VAD_CTRL;
		vad_top_update_bits(reg, 0x1f << 8, (slot_num - 1) << 8);
	} else {
		if (index == 3) {
			reg = EE_AUDIO_TDMIN_D_CTRL;
		} else if (index < 3) {
			offset = EE_AUDIO_TDMIN_B_CTRL - EE_AUDIO_TDMIN_A_CTRL;
			reg = EE_AUDIO_TDMIN_A_CTRL + offset * index;
		} else {
			reg = EE_AUDIO_TDMIN_A_CTRL;
		}
		aml_audiobus_update_bits(actrl, reg, 0x1f << 8, (slot_num - 1) << 8);
	}
}

void aml_tdmout_mute_speaker(int tdmout_id, bool mute)
{
	unsigned int reg, offset;

	offset = EE_AUDIO_TDMOUT_B_MUTE0 - EE_AUDIO_TDMOUT_A_MUTE0;
	reg = EE_AUDIO_TDMOUT_A_MUTE0 + offset * tdmout_id;

	if (mute)
		audiobus_write(reg, 0xffff);
	else
		audiobus_write(reg, 0);
}

