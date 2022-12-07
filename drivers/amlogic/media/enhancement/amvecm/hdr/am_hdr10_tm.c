// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * drivers/amlogic/media/enhancement/amvecm/hdr/gamut_convet.c
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
#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/amlogic/media/amvecm/amvecm.h>
#include <linux/amlogic/media/amvecm/hdr10_tmo_alg.h>
#include "am_hdr10_tm.h"
#include "../set_hdr2_v0.h"
#include "am_hdr10_tmo_fw.h"

unsigned int hdr10_tm_dbg;
module_param(hdr10_tm_dbg, uint, 0664);
MODULE_PARM_DESC(hdr10_tm_dbg, "HDR10 tone mapping dbg\n");

#define pr_hdr_tm(fmt, args...)\
	do {\
		if (hdr10_tm_dbg)\
			pr_info(fmt, ## args);\
	} while (0)

unsigned int panell = 400;
module_param(panell, uint, 0664);
MODULE_PARM_DESC(panell, "display panel luminance\n");

static unsigned int hdr10_tm_sel = 2; /*1 old algorithm, 2 hdr_tmo algorithm  default 2*/
module_param(hdr10_tm_sel, uint, 0664);
MODULE_PARM_DESC(hdr10_tm_sel, "hdr10_tm_sel\n");

#define KNEE_POINT 2
static unsigned int kp = KNEE_POINT;
static unsigned int knee_point[KNEE_POINT] = {0, 0};
module_param_array(knee_point, uint, &kp, 0664);
MODULE_PARM_DESC(knee_point, "\n knee point xy\n");

/*u32 ebz_p[MAX_BEIZER_ORDER] = {
 *	0, 1170, 1755, 2121, 2377, 2596, 2779, 2925, 3064, 3185,
 *	3320, 3514, 3799, 3957, 4095
 *};
 */

/*time domain filter, avoid flicker*/
/*scene change th: 2/3 scene diff*/
u32 sc_th = 682;
module_param(sc_th, uint, 0664);
MODULE_PARM_DESC(sc_th, "scene change th\n");

u32 hdr_tm_iir = 1;
module_param(hdr_tm_iir, uint, 0664);
MODULE_PARM_DESC(hdr_tm_iir, "HDR_TM_IIR\n");

/* blend ratio: gain = bld_ratio * bld_gain + (256 - bld_ratio) * gain*/
u32 bld_ratio = 250;
module_param(bld_ratio, uint, 0664);
MODULE_PARM_DESC(bld_ratio, "bld_ratio\n");

int is_hdr_tmo_support(void)
{
	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_SM1)
		return 1;
	return 0;
}

int time_iir(u32 *maxl)
{
	int i;
	static u32 hist_diff[3];
	static u32 max_lum[3];
	u32 diff;
	u32 sum = 0;
	int sumshft;
	int norm14;
	u32 bld_coef;
	u32 dtl_lum = 0;
	/*ret=1: scn change; ret=2: blend*/
	int ret;

	for (i = 0; i < 2; i++) {
		hist_diff[i] = hist_diff[i + 1];
		max_lum[i] = max_lum[i + 1];
	}
	hist_diff[2] = 0;
	max_lum[2] = *maxl;

	for (i = 0; i < 128; i++) {
		sum += hdr_hist[15][i];
		diff = (hdr_hist[15][i] > hdr_hist[14][i]) ?
			(hdr_hist[15][i] - hdr_hist[14][i]) :
			(hdr_hist[14][i] - hdr_hist[15][i]);
		hist_diff[2] += diff;
	}

	if (sum == 0)
		return 0;

	sumshft =
		(sum >= (1 << 24)) ? 8 :
		(sum >= (1 << 22)) ? 6 :
		(sum >= (1 << 20)) ? 4 :
		(sum >= (1 << 18)) ? 2 :
		(sum >= (1 << 16)) ? 0 :
		(sum >= (1 << 14)) ? -2 :
		(sum >= (1 << 12)) ? -4 :
		(sum >= (1 << 10)) ? -6 :
		(sum >= (1 << 8)) ? -8 :
		(sum >= (1 << 6)) ? -10 :
		(sum >= (1 << 4)) ? -12 : -16;

	if (sumshft >= 0)
		norm14 = (1 << 30) / (sum >> sumshft);
	else if (sumshft > -16)
		norm14 = (1 << (30 + sumshft)) / sum;
	else
		norm14 = 1 << 14;

	if (sumshft >= 0) {
		hist_diff[2] = ((hist_diff[2] >> sumshft) * norm14 +
			(1 << 13)) >> 14;
	} else {
		hist_diff[2] = (((hist_diff[2] << (-sumshft)) * norm14) +
			(1 << 13)) >> 14;
	}

	/*normalize to 10bit*/
	hist_diff[2] >>= 6;
	if (hist_diff[2] > sc_th || (*maxl <= panell)) {
		ret = 1;
	} else {
		bld_coef = hist_diff[2];
		dtl_lum =
			(max_lum[2] > max_lum[1]) ?
			(max_lum[2] - max_lum[1]) :
			(max_lum[1] - max_lum[2]);

		if (dtl_lum < (1 << 3))
			max_lum[2] =
				(max_lum[2] > max_lum[1]) ?
				(max_lum[1] + (dtl_lum >> 1)) :
				(max_lum[1] - (dtl_lum >> 1));
		else if (dtl_lum < (1 << 6))
			max_lum[2] =
				(max_lum[2] > max_lum[1]) ?
				(max_lum[1] + (dtl_lum >> 3)) :
				(max_lum[1] - (dtl_lum >> 3));
		else
			max_lum[2] =
				(max_lum[2] > max_lum[1]) ?
				(max_lum[1] + (dtl_lum >> 6)) :
				(max_lum[1] - (dtl_lum >> 6));

		*maxl = max_lum[2];
		ret = 2;
	}

	pr_hdr_tm("ret = %d, dtl_lum = %d, max_lum[1] = %d, max_lum[2] = %d\n",
		  ret, dtl_lum, max_lum[1], max_lum[2]);

	return ret;
}

/*hdr10 tone mapping alg */
int gen_oe_x_val(u64 *oe_x_val)
{
	unsigned int i = 0, j = 0;
	int bin_num = 0;
	int temp1 = 0, temp2 = 0;

	oe_x_val[0] = 0;

	for (i = 1; i < 8; i++) {
		bin_num++;
		temp1 = 1 << ((i - 1) + 4);
		oe_x_val[i] = (u64)temp1;
	}
	for (j = 11; j < 20; j++) {
		for (i = 0; i <  4; i++) {
			bin_num++;
			temp1 = 1 << (j - 2);
			temp2 = 1 << j;
			oe_x_val[bin_num] = (u64)temp1 * i + (u64)temp2;
		}
	}
	for (j = 20; j < 31; j++) {
		for (i = 0; i < 8; i++) {
			bin_num++;
			temp1 = 1 << (j - 3);
			temp2 = 1 << j;
			oe_x_val[bin_num] = (u64)temp1 * i + (u64)temp2;
		}
	}
	for (i = 0; i < 16; i++) {
		bin_num++;
		oe_x_val[bin_num] = (1 << (31 - 4)) * i + (1 << 31);
	}
	oe_x_val[OE_X - 1] = MAX_32;

	return 0;
}

void decasteliau_alg(u64 *ebzcur, u64 step, u64 range, u64 *p, u32 order)
{
	u64 point[MAX_BEIZER_ORDER + 1];
	int i, j;

	for (i = 0; i < order; i++)
		point[i] = p[i];

	for (i = 1; i < order; i++) {
		for (j = 0; j < order - i; j++) {
			point[j] = (range - step) * point[j] +
				step * point[j + 1] + (range >> 1);
			point[j] = div64_u64(point[j], range);
		}
	}
	ebzcur[1] = point[0];
}

static u64 curvex[OE_X], curvey[OE_X];
static u32 oo_gain[OE_X];
static u32 bld_gain[OE_X];

int gen_knee_anchor(u32 maxl, u32 panell, u64 *sx, u64 *sy, u64 *anchor)
{
	int i;
	int knee_bypass = 1229;
	u64 anchor_linear[MAX_BEIZER_ORDER];
	u64 norm, blendcoef;
	//int tgtl = 10000;
	struct basisootf_params basisootf_param;
	u64 sx1, sy1;
	u64 anchor0;

	sx1 = *sx;
	sy1 = *sy;

	basisootf_params_init(&basisootf_param);

	for (i = 0; i < MAX_BEIZER_ORDER - 1; i++)
		anchor_linear[i] = (i + 1) *
			(1 << MAX12_BIT) / MAX_BEIZER_ORDER;

	pr_hdr_tm("knee anchor p1: %lld, %lld, %lld, %lld, %lld, %lld, %lld, %lld, %lld, %lld %lld\n",
		  sx1, sy1,
		  anchor_linear[0], anchor_linear[1],
		  anchor_linear[2], anchor_linear[3],
		  anchor_linear[4], anchor_linear[5],
		  anchor_linear[6], anchor_linear[7],
		  anchor_linear[8]);

	if (maxl <= panell) {
		sx1 = knee_bypass;
		sy1 = knee_bypass;
		for (i = 0; i < MAX_BEIZER_ORDER - 1; i++)
			anchor[i] = anchor_linear[i];
	} else {
		norm = maxl;
		blendcoef = panell;
		sy1 = (blendcoef * knee_bypass +
			(norm - blendcoef) * sy1 + (norm >> 1));
		sy1 = div64_u64(sy1, norm);
		sx1 = sy1 * panell;
		sx1 = div64_u64(sx1, maxl);
		for (i = 0; i < MAX_BEIZER_ORDER - 1; i++) {
			anchor[i] =
			(blendcoef * anchor_linear[i] +
			(norm - blendcoef) * anchor[i] +
			(norm >> 1));

			anchor[i] = div64_u64(anchor[i], norm);
		}

		anchor0 = calcp1((u32)sx1, (u32)sy1,
				 panell, maxl, &basisootf_param, NULL);

		for (i = 1; i < MAX_BEIZER_ORDER - 1; i++)
			anchor[i] = min(anchor[i], (i + 1) * anchor0);

		if (anchor0 < anchor[1])
			anchor[0] = anchor0;
	}
	*sx = sx1;
	*sy = sy1;

	pr_hdr_tm("knee anchor p2: %lld, %lld, %lld, %lld, %lld, %lld, %lld, %lld, %lld, %lld %lld\n",
		  sx1, sy1,
		  anchor[0], anchor[1],
		  anchor[2], anchor[3],
		  anchor[4], anchor[5],
		  anchor[6], anchor[7],
		  anchor[8]);

	return 0;
}

int dynamic_hdr_sdr_ootf(u32 maxl, u32 panell, u64 sx, u64 sy, u64 *anchor)
{
	int i;
	u32 tgtl = 10000;
	static u64 oe_x[OE_X] = {0};
	int order = MAX_BEIZER_ORDER + 1;
	u64 step;
	u64 kx, ky;
	u64 ebzcur[2];
	u64 range_ebz_x, range_ebz_y;
	u64 finalanchor[MAX_BEIZER_ORDER + 1];
	u64 temp;
	u64 sx_primy;
	u64 kx_primy;

	gen_oe_x_val(oe_x);

	gen_knee_anchor(maxl, panell, &sx, &sy, anchor);

	pr_hdr_tm("d1: %d, %d, %lld, %lld, %lld, %lld, %lld, %lld, %lld, %lld, %lld, %lld %lld\n",
		  maxl, panell,
		  sx, sy,
		  anchor[0], anchor[1],
		  anchor[2], anchor[3],
		  anchor[4], anchor[5],
		  anchor[6], anchor[7],
		  anchor[8]);

	for (i = 0; i < order - 2; i++)
		finalanchor[i + 1] = anchor[i] << (MAX32_BIT - MAX12_BIT);

	finalanchor[0] = 0;
	finalanchor[10] = MAX_32;

	if (tgtl <= maxl)
		maxl = tgtl - 1;

	kx = sx << (MAX32_BIT - MAX12_BIT);
	ky = sy << (MAX32_BIT - MAX12_BIT);

	kx = kx * maxl;
	kx = div64_u64(kx, tgtl);

	if (maxl <= panell) {
		sx_primy = (u64)(1 << MAX12_BIT) * panell +  (tgtl >> 1);
		sx_primy = div64_u64(sx_primy, tgtl);
	} else {
		sx_primy = (u64)(1 << MAX12_BIT) * maxl +  (tgtl >> 1);
		sx_primy = div64_u64(sx_primy, tgtl);
	}
	kx_primy = sx_primy << (MAX32_BIT - MAX12_BIT);

	range_ebz_x = kx_primy - kx;
	range_ebz_y = MAX_32 - ky;

	if (maxl <= panell) {
		for (i = 0; i < OE_X; i++) {
			if (oe_x[i] <= kx_primy) {
				temp = tgtl << TM_GAIN_BIT;
				oo_gain[i] = div64_u64(temp, panell);
			} else {
				curvey[i] = oe_x[148] << TM_GAIN_BIT;
				curvex[i] = oe_x[i];
				oo_gain[i] = div64_u64(curvey[i], curvex[i]);
			}
			if (oo_gain[i] < (1 << TM_GAIN_BIT))
				oo_gain[i] = 1 << TM_GAIN_BIT;
			if (oo_gain[i] > (1 << MAX12_BIT) - 1)
				oo_gain[i] = (1 << MAX12_BIT) - 1;
		}
	} else {
		for (i = 0; i < OE_X; i++) {
			if (oe_x[i] <= kx) {
				temp = tgtl << TM_GAIN_BIT;
				oo_gain[i] = div64_u64(temp, panell);
			} else if (oe_x[i] <=  kx_primy) {
				step = oe_x[i] - kx;
				decasteliau_alg(&ebzcur[0], step,
						range_ebz_x,
						finalanchor, order);
				curvey[i] = ky +
					((range_ebz_y * ebzcur[1] + MAX_32 / 2)
					>> MAX32_BIT);
				temp = curvey[i] << TM_GAIN_BIT;
				curvex[i] = kx + step;
				oo_gain[i] = div64_u64(temp, curvex[i]);
			}  else {
				curvey[i] = oe_x[148] << TM_GAIN_BIT;
				curvex[i] = oe_x[i];
				oo_gain[i] = div64_u64(curvey[i], curvex[i]);
			}
			if (oo_gain[i] < (1 << TM_GAIN_BIT))
				oo_gain[i] = 1 << TM_GAIN_BIT;
			if (oo_gain[i] > (1 << MAX12_BIT) - 1)
				oo_gain[i] = (1 << MAX12_BIT) - 1;
		}
	}

	for (i = 0; i < OE_X; i++) {
		if ((i + 1) % 10 == 0)
			pr_hdr_tm("%d, %d, %d, %d, %d, %d, %d, %d, %d, %d\n",
				  oo_gain[i - 9], oo_gain[i - 8],
				  oo_gain[i - 7], oo_gain[i - 6],
				  oo_gain[i - 5], oo_gain[i - 4],
				  oo_gain[i - 3], oo_gain[i - 2],
				  oo_gain[i - 1], oo_gain[i]);
		if (i == 148)
			pr_hdr_tm("%d, %d, %d, %d, %d, %d, %d, %d, %d\n",
				  oo_gain[i - 8],
				  oo_gain[i - 7], oo_gain[i - 6],
				  oo_gain[i - 5], oo_gain[i - 4],
				  oo_gain[i - 3], oo_gain[i - 2],
				  oo_gain[i - 1], oo_gain[i]);
	}
	pr_hdr_tm("\n");

	return 0;
}

int hdr10_tm_dynamic_proc(struct vframe_master_display_colour_s *p)
{
	int i;
	u32 maxl;
	u32 primary_maxl;
	u32 panel_luma;
	u64 sx, sy;
	u64 anchor[MAX_BEIZER_ORDER - 1];
	u32 P_init[MAX_BEIZER_ORDER - 1] = {
		181, 406, 607, 796, 834, 863, 890, 917, 938
	};
	int scn_chang_flag = 1;
	struct aml_tmo_reg_sw *pre_tmo_reg;

	if (p->luminance[0] > 10000)
		p->luminance[0] /= 10000;
	/*no luminance*/
	if (p->luminance[0] == 0)
		p->luminance[0] = 1200;

	primary_maxl = p->luminance[0];

	/*use 95% maxl because of high percert flicker*/
	maxl = (percentile[8] > primary_maxl) ? primary_maxl : percentile[8];
	pr_hdr_tm("maxl = %d, percentile[8] = %d, primary_maxl =%d\n",
		  maxl, percentile[8], primary_maxl);

	if (hdr_tm_iir)
		scn_chang_flag = time_iir(&maxl);

	if (scn_chang_flag == 0)
		pr_hdr_tm("invalid hist\n");
	else if (scn_chang_flag == 1)
		pr_hdr_tm("scn change\n");
	else if (scn_chang_flag == 2)
		pr_hdr_tm("maxl blend: maxl = %d\n", maxl);

	panel_luma = panell;

	sx = knee_point[0];
	sy = knee_point[1];

	/*invalid case*/
	if (maxl == 0) {
		maxl = 1200;
		pr_hdr_tm("invalid maxl\n");
	}

	for (i = 0; i < MAX_BEIZER_ORDER - 1; i++)
		anchor[i] = P_init[i] << 2;

	pre_tmo_reg = tmo_fw_param_get();

	if (!is_hdr_tmo_support() || !pre_tmo_reg->pre_hdr10_tmo_alg) {
		/*used old hdr alg*/
		pr_hdr_tm("used old hdr alg - ");
		if (!pre_tmo_reg->pre_hdr10_tmo_alg)
			pr_hdr_tm("hdr alg ko insmod failed.\n");
		else
			pr_hdr_tm("the chip is not support.\n");
		hdr10_tm_sel = 1;
	}

	if (hdr10_tm_sel == 1) {
		dynamic_hdr_sdr_ootf(maxl, panel_luma, sx, sy, anchor);
		if (scn_chang_flag == 0 || scn_chang_flag == 1) {
			memcpy(oo_y_lut_hdr_sdr, oo_gain, sizeof(u32) * OE_X);
			memcpy(bld_gain, oo_gain, sizeof(u32) * OE_X);
		} else {
			for (i = 0; i < OE_X; i++) {
				oo_y_lut_hdr_sdr[i] = (bld_gain[i] * bld_ratio +
					((1 << 8) - bld_ratio) * oo_gain[i]) >> 8;
			}
			memcpy(bld_gain, oo_y_lut_hdr_sdr, sizeof(u32) * OE_X);
		}
	} else if (hdr10_tm_sel == 2) {
		hdr10_tmo_gen(oo_gain);
		memcpy(oo_y_lut_hdr_sdr, oo_gain, sizeof(u32) * OE_X);
	} else {
		memcpy(oo_y_lut_hdr_sdr, oo_y_lut_hdr_sdr_def,
			sizeof(u32) * OE_X);
	}

	if (hdr10_tm_dbg > 0)
		hdr10_tm_dbg--;

	return 0;
}

