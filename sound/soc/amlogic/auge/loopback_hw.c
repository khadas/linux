// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#define DEBUG

#include "linux/kernel.h"

#include "loopback_hw.h"
#include "regs.h"
#include "iomap.h"

static unsigned int get_tdmin_id_from_lb_src(enum datalb_src lb_src)
{
	return lb_src % TDMINLB_PAD_TDMINA;
}

void tdminlb_set_clk(enum datalb_src lb_src, int mclk_sel, bool enable)
{
	unsigned int tdmin_src;

	/* config for external codec */
	if (lb_src >= TDMINLB_PAD_TDMINA) {
		unsigned int id = get_tdmin_id_from_lb_src(lb_src);
		tdmin_src = id;
	} else {
		tdmin_src = lb_src;
	}
	if (mclk_sel > 0) {
		tdmin_src = mclk_sel;
	}
	audiobus_update_bits(EE_AUDIO_CLK_TDMIN_LB_CTRL,
			     0x3 << 30 | 1 << 29 | 0xf << 24 | 0xf << 20,
			     (enable ? 0x3 : 0x0) << 30 |
			     1 << 29 | tdmin_src << 24 | tdmin_src << 20
	);
}

void tdminlb_enable(int tdm_index, int is_enable)
{
	audiobus_update_bits(EE_AUDIO_TDMIN_LB_CTRL,
			     0x1 << 31, is_enable << 31);
}

void tdminlb_set_format(int i2s_fmt)
{
	audiobus_update_bits(EE_AUDIO_TDMIN_LB_CTRL,
			     0x1 << 30,
			     !!i2s_fmt << 30 /* 0:tdm mode; 1: i2s mode; */
	);
}

void tdminlb_set_ctrl(enum datalb_src src)
{
	audiobus_update_bits(EE_AUDIO_TDMIN_LB_CTRL,
				0x1 << 25 | 0xf << 20 | 0x7 << 16 | 0x1f << 0,
				0x1 << 25 |  /* invert ws */
				src << 20 |  /* in src */
				3   << 16 |  /* skew */
				31  << 0     /* bit width */
	);
}

void tdminlb_fifo_enable(int is_enable)
{
	if (is_enable) {
		audiobus_update_bits(EE_AUDIO_TDMIN_LB_CTRL, 1 << 29, 1 << 29);
		audiobus_update_bits(EE_AUDIO_TDMIN_LB_CTRL, 1 << 28, 1 << 28);
	} else {
		audiobus_update_bits(EE_AUDIO_TDMIN_LB_CTRL, 3 << 28, 0);
	}
}

static void tdminlb_set_lane_mask(int lane, int mask)
{
	audiobus_write(EE_AUDIO_TDMIN_LB_MASK0 + lane, mask);
}

void tdminlb_set_lanemask_and_chswap(int swap, int lane_mask, unsigned int mask)
{
	unsigned int i;

	pr_debug
		("%s() lane swap:0x%x lane mask:0x%x, chmask:%#x\n",
		__func__, swap, lane_mask, mask);

	/* channel swap */
	audiobus_write(EE_AUDIO_TDMIN_LB_SWAP0, swap);

	/* lane mask */
	for (i = 0; i < 4; i++)
		if ((1 << i) & lane_mask)
			tdminlb_set_lane_mask(i, mask);
}

void tdminlb_set_src(int src)
{
	audiobus_update_bits(EE_AUDIO_TDMIN_LB_CTRL, 0xf << 20, src);
}

void lb_set_datain_src(int id, int src)
{
	int offset = EE_AUDIO_LB_B_CTRL0 - EE_AUDIO_LB_A_CTRL0;
	int reg = EE_AUDIO_LB_A_CTRL0 + offset * id;

	audiobus_update_bits(reg, 0x7 << 0, src);
}

void lb_set_mode(int id, enum lb_out_rate rate)
{
	int offset = EE_AUDIO_LB_B_CTRL0 - EE_AUDIO_LB_A_CTRL0;
	int reg = EE_AUDIO_LB_A_CTRL0 + offset * id;

	audiobus_update_bits(reg, 0x1 << 30, rate << 30);
}

void loopback_src_set(int id, struct mux_conf *conf, int version)
{
	int offset = 0;
	int reg = 0;

	if (version == 0) {
		offset = EE_AUDIO_LB_B_CTRL0 - EE_AUDIO_LB_A_CTRL0;
		reg = EE_AUDIO_LB_B_CTRL0 + offset * id;
	} else {
		offset = EE_AUDIO_LB_B_CTRL2 - EE_AUDIO_LB_A_CTRL2;
		reg = EE_AUDIO_LB_A_CTRL2 + offset * id;
	}
	audiobus_update_bits(reg, conf->mask << conf->shift, conf->val << conf->shift);
}

void lb_set_datain_cfg(int id, struct data_cfg *datain_cfg)
{
	int offset = EE_AUDIO_LB_B_CTRL0 - EE_AUDIO_LB_A_CTRL0;
	int reg = EE_AUDIO_LB_A_CTRL0 + offset * id;
	if (!datain_cfg)
		return;

	if (datain_cfg->ch_ctrl_switch) {
		audiobus_update_bits(reg,
			1 << 29 | 0x7 << 13 | 0x1f << 8 |
			0x1f << 3 | 0x7 << 0,
			datain_cfg->ext_signed << 29 |
			datain_cfg->type       << 13 |
			datain_cfg->m          << 8  |
			datain_cfg->n          << 3  |
			datain_cfg->src        << 0
		);

		/* channel and mask */
		offset = EE_AUDIO_LB_B_CTRL2 - EE_AUDIO_LB_A_CTRL2;
		reg = EE_AUDIO_LB_A_CTRL2 + offset * id;
		audiobus_write(reg,
			(datain_cfg->chnum - 1) << 16 |
			datain_cfg->chmask      << 0
		);
	} else {
		audiobus_update_bits(reg,
			1 << 29 | 0x7 << 24 | 0xff << 16 |
			0x7 << 13 | 0x1f << 8 |
			0x1f << 3 | 0x7 << 0,
			datain_cfg->ext_signed  << 29 |
			(datain_cfg->chnum - 1) << 24 |
			datain_cfg->chmask      << 16 |
			datain_cfg->type        << 13 |
			datain_cfg->m           << 8  |
			datain_cfg->n           << 3  |
			datain_cfg->src         << 0
		);
	}
}

void lb_set_datalb_cfg(int id, struct data_cfg *datalb_cfg, bool multi_bits_lbsrcs,
		       bool use_resamplea)
{
	int offset = EE_AUDIO_LB_B_CTRL1 - EE_AUDIO_LB_A_CTRL1;
	int reg = EE_AUDIO_LB_A_CTRL1 + offset * id;

	if (!datalb_cfg)
		return;

	if (datalb_cfg->ch_ctrl_switch) {
		audiobus_update_bits(reg,
			0x1 << 29 | 0x7 << 13 |
			0x1f << 8 | 0x1f << 3,
			datalb_cfg->ext_signed << 29 |
			datalb_cfg->type       << 13 |
			datalb_cfg->m          << 8  |
			datalb_cfg->n          << 3);
		if (!multi_bits_lbsrcs)
			audiobus_update_bits(reg,
				0x3 << 30 | 0x1 << 0,
				datalb_cfg->resample_enable << 30 | datalb_cfg->loopback_src << 0);

		/* channel and mask */
		offset = EE_AUDIO_LB_B_CTRL3 - EE_AUDIO_LB_A_CTRL3;
		reg = EE_AUDIO_LB_A_CTRL3 + offset * id;
		audiobus_write(reg,
			(datalb_cfg->chnum - 1) << 16 |
			datalb_cfg->chmask	<< 0);
		if (multi_bits_lbsrcs) {
			/* from t5 chip, loopback src changed if resample for loopback */
			int loopback_src;

			if (datalb_cfg->resample_enable) {
				if (use_resamplea)
					loopback_src = RESAMPLEA;
				else
					loopback_src = RESAMPLEB;
			} else {
				loopback_src = TDMIN_LB;
			}
			audiobus_update_bits(reg, 0x1f << 20, loopback_src << 20);
		}
	} else {
		audiobus_write(reg,
			datalb_cfg->ext_signed   << 29 |
			(datalb_cfg->chnum - 1)  << 24 |
			datalb_cfg->chmask       << 16 |
			datalb_cfg->type         << 13 |
			datalb_cfg->m            << 8  |
			datalb_cfg->n            << 3
		);
	}
}

void lb_enable(int id, bool enable, bool chnum_en)
{
	int offset = EE_AUDIO_LB_B_CTRL0 - EE_AUDIO_LB_A_CTRL0;
	int reg = EE_AUDIO_LB_A_CTRL0 + offset * id;

	audiobus_update_bits(reg, 0x1 << 31, enable << 31);
	lb_set_chnum_en(id, enable, chnum_en);
}

void lb_set_chnum_en(int id, bool en, bool chnum_en)
{
	if (chnum_en) {
		int offset = EE_AUDIO_LB_B_CTRL0 - EE_AUDIO_LB_A_CTRL0;
		int reg = EE_AUDIO_LB_A_CTRL0 + offset * id;

		audiobus_update_bits(reg, 0x1 << 27, en << 27);
	}
}
