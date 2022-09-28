/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __AML_LOOPBACK_HW_H__
#define __AML_LOOPBACK_HW_H__

#include <linux/types.h>
#include "loopback.h"
#include "resample.h"

struct mux_conf {
	char name[32];
	unsigned int val;
	unsigned int reg;
	unsigned int shift;
	unsigned int mask;
};

#define AUDIO_SRC_CONFIG(_name, _val, _reg, _shift, _mask) \
{	.name = (_name), .val = (_val), .reg = (_reg),\
	.shift = (_shift), .mask = (_mask)}

struct data_cfg {
	/*
	 * 0: extend bits as "0"
	 * 1: extend bits as "msb"
	 */
	unsigned int ext_signed;
	/* channel number */
	unsigned int chnum;
	/* channel selected */
	unsigned int chmask;
	/* combined data */
	unsigned int type;
	/* the msb position in data */
	unsigned int m;
	/* the lsb position in data */
	unsigned int n;

	/* loopback datalb src */
	unsigned int src;

	unsigned int loopback_src;

	/* channel and mask in new ctrol register */
	bool ch_ctrl_switch;

	/* enable resample B for loopback*/
	unsigned int resample_enable;
	/* srcs from chipinfo */
	struct mux_conf *srcs;
	struct mux_conf *tdmin_lb_srcs;
};

void tdminlb_set_clk(enum datalb_src lb_src, int mclk_sel, bool enable);
void tdminlb_set_format(int i2s_fmt);
void tdminlb_set_ctrl(enum datalb_src src);
void tdminlb_enable(int tdm_index, int in_enable);
void tdminlb_fifo_enable(int is_enable);
void tdminlb_set_format(int i2s_fmt);
void tdminlb_set_lanemask_and_chswap
	(int swap, int lane_mask, unsigned int mask);

void tdminlb_set_src(int src);
void lb_set_datain_src(int id, int src);
void lb_set_datain_cfg(int id, struct data_cfg *datain_cfg);
void lb_set_datalb_cfg(int id, struct data_cfg *datalb_cfg, bool multi_bits_lbsrcs,
		       bool use_resamplea);
void lb_enable(int id, bool enable, bool chnum_en);
void lb_set_chnum_en(int id, bool en, bool chnum_en);
void loopback_src_set(int id, struct mux_conf *conf, int version);

enum lb_out_rate {
	MIC_RATE,
	LB_RATE,
};

void lb_set_mode(int id, enum lb_out_rate rate);
#endif
