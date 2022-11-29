// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * drivers/amlogic/media/enhancement/amvecm/blue_stretch/blue_str.c
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
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/amlogic/media/amvecm/amvecm.h>
#include <linux/amlogic/media/amvecm/bls_alg.h>
#include "blue_str.h"
#include "../amve.h"
#include "../reg_helper.h"
#include "../arch/vpp_regs.h"

static int bls_debug;
static int idx_r = -1, idx_g = -1, idx_b = -1;
static int bs_latch_flag;

static char bls_ver[32] = "blue str:20220713 fix 10b csc";
int bs_proc_en;
/*default parameters*/

static struct bls_par bls_param = {
	.alg_ver = bls_ver,
	.blue_stretch_en = 0,
	.blue_stretch_cr_inc = 0,
	.blue_stretch_cb_inc = 1,
	.blue_stretch_gain = 16,
	.blue_stretch_gain_cb4cr = 8,
	.blue_stretch_error_crp = 16,
	.blue_stretch_error_crp_inv = 256,
	.blue_stretch_error_crn = 16,
	.blue_stretch_error_crn_inv = 256,
	.blue_stretch_error_cbp = 8,
	.blue_stretch_error_cbp_inv = 512,
	.blue_stretch_error_cbn = 8,
	.blue_stretch_error_cbn_inv = 512,
	.blue_stretch_luma_high = 96,
	.bls_proc = NULL,
};

/*
 *static struct bls_par bls_param = {
 *	.alg_ver = bls_ver,
 *	.blue_stretch_en = 1,
 *	.blue_stretch_cr_inc = 1,
 *	.blue_stretch_cb_inc = 1,
 *	.blue_stretch_gain = 116,
 *	.blue_stretch_gain_cb4cr = 0,
 *	.blue_stretch_error_crp = 12,
 *	.blue_stretch_error_crp_inv = 256,
 *	.blue_stretch_error_crn = 12,
 *	.blue_stretch_error_crn_inv = 256,
 *	.blue_stretch_error_cbp = 12,
 *	.blue_stretch_error_cbp_inv = 512,
 *	.blue_stretch_error_cbn = 12,
 *	.blue_stretch_error_cbn_inv = 512,
 *	.blue_stretch_luma_high = 125,
 *	.bls_proc = NULL
 *};
 */

struct bls_par *get_bls_par(void)
{
	return &bls_param;
}
EXPORT_SYMBOL(get_bls_par);

void set_blue_str_parm(struct blue_str_parm_s *parm)
{
	struct bls_par *bls_param;

	bls_param = get_bls_par();

	bls_param->blue_stretch_en = parm->blue_stretch_en;
	bls_param->blue_stretch_cr_inc = parm->blue_stretch_cr_inc;
	bls_param->blue_stretch_cb_inc = parm->blue_stretch_cb_inc;
	bls_param->blue_stretch_gain = parm->blue_stretch_gain;
	bls_param->blue_stretch_gain_cb4cr = parm->blue_stretch_gain_cb4cr;
	bls_param->blue_stretch_error_crp = parm->blue_stretch_error_crp;
	bls_param->blue_stretch_error_crp_inv = parm->blue_stretch_error_crp_inv;
	bls_param->blue_stretch_error_crn = parm->blue_stretch_error_crn;
	bls_param->blue_stretch_error_crn_inv = parm->blue_stretch_error_crn_inv;
	bls_param->blue_stretch_error_cbp = parm->blue_stretch_error_cbp;
	bls_param->blue_stretch_error_cbp_inv = parm->blue_stretch_error_cbp_inv;
	bls_param->blue_stretch_error_cbn = parm->blue_stretch_error_cbn;
	bls_param->blue_stretch_error_cbn_inv = parm->blue_stretch_error_cbn_inv;
	bls_param->blue_stretch_luma_high = parm->blue_stretch_luma_high;
}

/*
 *static void rgb2ycbcrpc_fr(int *y, int *u, int *v, int r, int g, int b, int bitdepth)
 *{
 *	int ny = 0;
 *	int ncb = 0;
 *	int ncr = 0;

 *	ny = 218 * r + 732 * g + 74 * b;
 *	ncb = -118 * r - 394 * g + 512 * b;
 *	ncr = 512 * r - 465 * g - 47 * b;

 *	*y = ny >> bitdepth;
 *	*u = (ncb >> 10) + (1 << (bitdepth - 1));
 *	*v = (ncr >> 10) + (1 << (bitdepth - 1));
 *}
 */

/*
 *static void yuv2ycbcr_fr(int *y, int *u, int *v, int y_in, int u_in, int v_in, int bitdepth)
 *{
 *	int ny = 0;
 *	int ncb = 0;
 *	int ncr = 0;
 *	int yin = 0;
 *	int uin = 0;
 *	int vin = 0;

 *	yin = y_in - (1 << (bitdepth - 4));
 *	uin = u_in - (1 << (bitdepth - 1));
 *	vin = v_in - (1 << (bitdepth - 1));

 *	ny = 1196 * yin;
 *	ncb = 1169 * uin;
 *	ncr = 1169 * vin;

 *	*y = ny >> bitdepth;
 *	*u = (ncb >> 10) + (1 << (bitdepth - 1));
 *	*v = (ncr >> 10) + (1 << (bitdepth - 1));
 *}
 */

static void ycbcr2rgbpc_nb(int *r, int *g, int *b, int y, int u, int v, int bitdepth)
{
	int ny = 0;
	int cb = 0;
	int cr = 0;
	int max = (1 << bitdepth) - 1;

	ny = y;
	cb = u - (1 << (bitdepth - 1));
	cr = v - (1 << (bitdepth - 1));

	*r = (1024 * ny + 1613 * cr) >> 10;
	*g = (1024 * ny - 191 * cb - 479 * cr) >> 10;
	*b = (1024 * ny + 1901 * cb) >> 10;

	if (*r > max)
		*r = max;
	if (*g > max)
		*g = max;
	if (*b > max)
		*b = max;

	if (*r < 0)
		*r = 0;
	if (*g < 0)
		*g = 0;
	if (*b < 0)
		*b = 0;
}

void bls_set(void)
{
	int d0, d1, d2;
	int ori_in[3] = {0}, rgb_o[3] = {0};
	int index;
	int yuv_in[3] = {0}, yuv_o[3] = {0};
	int bitdepth = BITDEPTH;
	struct bls_par *bls_param;

	bls_param = get_bls_par();
	if (!bls_param->bls_proc) {
		pr_info("blue str alg is NULL\n");
		//return;
	}

	vpp_lut3d_table_init(-1, -1, -1);

	for (d0 = 0; d0 < 17; d0++) {
		for (d1 = 0; d1 < 17; d1++) {
			for (d2 = 0; d2 < 17; d2++) {
				index = d0 * 289 + d1 * 17 + d2;
				ori_in[0] = plut3d[index * 3 + 0];
				ori_in[1] = plut3d[index * 3 + 1];
				ori_in[2] = plut3d[index * 3 + 2];
				//rgb2ycbcrpc_fr(&yuv_in[0], &yuv_in[1], &yuv_in[2],
				//	rgb_in[0], rgb_in[1], rgb_in[2], bitdepth);
				//yuv2ycbcr_fr(&yuv_in[0], &yuv_in[1], &yuv_in[2],
				//	ori_in[0], ori_in[1], ori_in[2], bitdepth);
				if (bls_param->bls_proc) {
					bls_param->bls_proc(yuv_o, ori_in, bls_param);
				} else {
					yuv_o[0] = ori_in[0];
					yuv_o[1] = ori_in[1];
					yuv_o[2] = ori_in[2];
				}
				ycbcr2rgbpc_nb(&rgb_o[0], &rgb_o[1], &rgb_o[2],
					yuv_o[0], yuv_o[1], yuv_o[2], bitdepth);
				plut3d[index * 3 + 0] = rgb_o[0];
				plut3d[index * 3 + 1] = rgb_o[1];
				plut3d[index * 3 + 2] = rgb_o[2];
				if (bls_debug && d0 == idx_r && d1 == idx_g) {
					if (idx_b < 0)
						/*print 17 point*/
						pr_info("d0 = %d, d1 = %d, d2 = %d. ori_in[3]: %d, %d, %d. yuv_in[3]: %d, %d, %d. yuv_o[3] = %d, %d, %d. rgb_o[3]: %d, %d, %d\n",
							d0, d1, d2,
							ori_in[0], ori_in[1], ori_in[2],
							yuv_in[0], yuv_in[1], yuv_in[2],
							yuv_o[0], yuv_o[1], yuv_o[2],
							rgb_o[0], rgb_o[1], rgb_o[2]);
					else if (idx_b == d2)
						pr_info("d0 = %d, d1 = %d, d2 = %d. ori_in[3]: %d, %d, %d. yuv_in[3]: %d, %d, %d. yuv_o[3] = %d, %d, %d. rgb_o[3]: %d, %d, %d\n",
							d0, d1, d2,
							ori_in[0], ori_in[1], ori_in[2],
							yuv_in[0], yuv_in[1], yuv_in[2],
							yuv_o[0], yuv_o[1], yuv_o[2],
							rgb_o[0], rgb_o[1], rgb_o[2]);
				}
			}
		}
	}

	//vpp_set_lut3d(0, 0, 0, 0);
}

void bs_reg_set(void)
{
	vpp_set_lut3d(0, 0, 0, 0);
}

ssize_t bls_par_show(char *buf)
{
	struct bls_par *bls_param;
	ssize_t len = 0;

	bls_param = get_bls_par();

	if (bs_latch_flag & BLUE_STR_EN) {
		len += sprintf(buf + len, "blue_stretch_en=%d\n",
			bls_param->blue_stretch_en);
		bs_latch_flag &= ~BLUE_STR_EN;
	}

	if (bs_latch_flag & BLUE_STR_CR_INC) {
		len += sprintf(buf + len, "blue_stretch_cr_inc=%d\n",
			bls_param->blue_stretch_cr_inc);
		bs_latch_flag &= ~BLUE_STR_CR_INC;
	}

	if (bs_latch_flag & BLUE_STR_CB_INC) {
		len += sprintf(buf + len, "blue_stretch_cb_inc=%d\n",
			bls_param->blue_stretch_cb_inc);
		bs_latch_flag &= ~BLUE_STR_CB_INC;
	}

	if (bs_latch_flag & BLUE_STR_GAIN) {
		len += sprintf(buf + len, "blue_stretch_gain=%d\n",
			bls_param->blue_stretch_gain);
		bs_latch_flag &= ~BLUE_STR_GAIN;
	}

	if (bs_latch_flag & BLUE_STR_GAIN_CB4CR) {
		len += sprintf(buf + len, "blue_stretch_gain_cb4cr=%d\n",
			bls_param->blue_stretch_gain_cb4cr);
		bs_latch_flag &= ~BLUE_STR_GAIN_CB4CR;
	}

	if (bs_latch_flag & BLUE_STR_GAIN_ERR_CRP) {
		len += sprintf(buf + len, "blue_stretch_error_crp=%d\n",
			bls_param->blue_stretch_error_crp);
		bs_latch_flag &= ~BLUE_STR_GAIN_ERR_CRP;
	}

	if (bs_latch_flag & BLUE_STR_GAIN_ERR_CRP_INV) {
		len += sprintf(buf + len, "blue_stretch_error_crp_inv=%d\n",
			bls_param->blue_stretch_error_crp_inv);
		bs_latch_flag &= ~BLUE_STR_GAIN_ERR_CRP_INV;
	}

	if (bs_latch_flag & BLUE_STR_GAIN_ERR_CRN) {
		len += sprintf(buf + len, "blue_stretch_error_crn=%d\n",
			bls_param->blue_stretch_error_crn);
		bs_latch_flag &= ~BLUE_STR_GAIN_ERR_CRN;
	}

	if (bs_latch_flag & BLUE_STR_GAIN_ERR_CRN_INV) {
		len += sprintf(buf + len, "blue_stretch_error_crn_inv=%d\n",
			bls_param->blue_stretch_error_crn_inv);
		bs_latch_flag &= ~BLUE_STR_GAIN_ERR_CRN_INV;
	}

	if (bs_latch_flag & BLUE_STR_GAIN_ERR_CBP) {
		len += sprintf(buf + len, "blue_stretch_error_cbp=%d\n",
			bls_param->blue_stretch_error_cbp);
		bs_latch_flag &= ~BLUE_STR_GAIN_ERR_CBP;
	}

	if (bs_latch_flag & BLUE_STR_GAIN_ERR_CBP_INV) {
		len += sprintf(buf + len, "blue_stretch_error_cbp_inv=%d\n",
			bls_param->blue_stretch_error_cbp_inv);
		bs_latch_flag &= ~BLUE_STR_GAIN_ERR_CBP_INV;
	}

	if (bs_latch_flag & BLUE_STR_GAIN_ERR_CBN) {
		len += sprintf(buf + len, "blue_stretch_error_cbn=%d\n",
			bls_param->blue_stretch_error_cbn);
		bs_latch_flag &= ~BLUE_STR_GAIN_ERR_CBN;
	}

	if (bs_latch_flag & BLUE_STR_GAIN_ERR_CBN_INV) {
		len += sprintf(buf + len, "blue_stretch_error_cbn_inv=%d\n",
			bls_param->blue_stretch_error_cbn_inv);
		bs_latch_flag &= ~BLUE_STR_GAIN_ERR_CBN_INV;
	}

	if (bs_latch_flag & BLUE_STR_GAIN_LUMA_HIGH) {
		len += sprintf(buf + len, "blue_stretch_luma_high=%d\n",
			bls_param->blue_stretch_luma_high);
		bs_latch_flag &= ~BLUE_STR_GAIN_LUMA_HIGH;
	}
	return len;
}

int bls_par_dbg(char **parm)
{
	long val;
	struct bls_par *bls_param;
	int en;

	bls_param = get_bls_par();
	if (!strcmp(parm[0], "bls_en")) {
		if (!parm[1]) {
			bs_latch_flag |= BLUE_STR_EN;
			goto for_read;
		}
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto failed;
		bls_param->blue_stretch_en = (int)val;
		bs_ct_latch();
	} else if (!strcmp(parm[0], "bls_cr_inc")) {
		if (!parm[1]) {
			bs_latch_flag |= BLUE_STR_CR_INC;
			goto for_read;
		}
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto failed;
		bls_param->blue_stretch_cr_inc = (int)val;
		bs_ct_latch();
	} else if (!strcmp(parm[0], "bls_cb_inc")) {
		if (!parm[1]) {
			bs_latch_flag |= BLUE_STR_CB_INC;
			goto for_read;
		}
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto failed;
		bls_param->blue_stretch_cb_inc = (int)val;
		bs_ct_latch();
	} else if (!strcmp(parm[0], "bls_gain")) {
		if (!parm[1]) {
			bs_latch_flag |= BLUE_STR_GAIN;
			goto for_read;
		}
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto failed;
		bls_param->blue_stretch_gain = (int)val;
		bs_ct_latch();
	} else if (!strcmp(parm[0], "bls_gain_cb4cr")) {
		if (!parm[1]) {
			bs_latch_flag |= BLUE_STR_GAIN_CB4CR;
			goto for_read;
		}
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto failed;
		bls_param->blue_stretch_gain_cb4cr = (int)val;
		bs_ct_latch();
	} else if (!strcmp(parm[0], "bls_error_crp")) {
		if (!parm[1]) {
			bs_latch_flag |= BLUE_STR_GAIN_ERR_CRP;
			goto for_read;
		}
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto failed;
		bls_param->blue_stretch_error_crp = (int)val;
		bs_ct_latch();
	} else if (!strcmp(parm[0], "bls_error_crp_inv")) {
		if (!parm[1]) {
			bs_latch_flag |= BLUE_STR_GAIN_ERR_CRP_INV;
			goto for_read;
		}
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto failed;
		bls_param->blue_stretch_error_crp_inv = (int)val;
		bs_ct_latch();
	} else if (!strcmp(parm[0], "bls_error_crn")) {
		if (!parm[1]) {
			bs_latch_flag |= BLUE_STR_GAIN_ERR_CRN;
			goto for_read;
		}
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto failed;
		bls_param->blue_stretch_error_crn = (int)val;
		bs_ct_latch();
	} else if (!strcmp(parm[0], "bls_error_crn_inv")) {
		if (!parm[1]) {
			bs_latch_flag |= BLUE_STR_GAIN_ERR_CRN_INV;
			goto for_read;
		}
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto failed;
		bls_param->blue_stretch_error_crn_inv = (int)val;
		bs_ct_latch();
	} else if (!strcmp(parm[0], "bls_error_cbp")) {
		if (!parm[1]) {
			bs_latch_flag |= BLUE_STR_GAIN_ERR_CBP;
			goto for_read;
		}
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto failed;
		bls_param->blue_stretch_error_cbp = (int)val;
		bs_ct_latch();
	} else if (!strcmp(parm[0], "bls_error_cbp_inv")) {
		if (!parm[1]) {
			bs_latch_flag |= BLUE_STR_GAIN_ERR_CBP_INV;
			goto for_read;
		}
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto failed;
		bls_param->blue_stretch_error_cbp_inv = (int)val;
		bs_ct_latch();
	} else if (!strcmp(parm[0], "bls_error_cbn")) {
		if (!parm[1]) {
			bs_latch_flag |= BLUE_STR_GAIN_ERR_CBN;
			goto for_read;
		}
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto failed;
		bls_param->blue_stretch_error_cbn = (int)val;
		bs_ct_latch();
	} else if (!strcmp(parm[0], "bls_error_cbn_inv")) {
		if (!parm[1]) {
			bs_latch_flag |= BLUE_STR_GAIN_ERR_CBN_INV;
			goto for_read;
		}
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto failed;
		bls_param->blue_stretch_error_cbn_inv = (int)val;
		bs_ct_latch();
	} else if (!strcmp(parm[0], "bls_luma_high")) {
		if (!parm[1]) {
			bs_latch_flag |= BLUE_STR_GAIN_LUMA_HIGH;
			goto for_read;
		}
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto failed;
		bls_param->blue_stretch_luma_high = (int)val;
		bs_ct_latch();
	} else if (!strcmp(parm[0], "read_all")) {
		bs_latch_flag |= BLUE_STR_EN;
		bs_latch_flag |= BLUE_STR_CR_INC;
		bs_latch_flag |= BLUE_STR_CB_INC;
		bs_latch_flag |= BLUE_STR_GAIN;
		bs_latch_flag |= BLUE_STR_GAIN_CB4CR;
		bs_latch_flag |= BLUE_STR_GAIN_ERR_CRP;
		bs_latch_flag |= BLUE_STR_GAIN_ERR_CRP_INV;
		bs_latch_flag |= BLUE_STR_GAIN_ERR_CRN;
		bs_latch_flag |= BLUE_STR_GAIN_ERR_CRN_INV;
		bs_latch_flag |= BLUE_STR_GAIN_ERR_CBP;
		bs_latch_flag |= BLUE_STR_GAIN_ERR_CBP_INV;
		bs_latch_flag |= BLUE_STR_GAIN_ERR_CBN;
		bs_latch_flag |= BLUE_STR_GAIN_ERR_CBN_INV;
		bs_latch_flag |= BLUE_STR_GAIN_LUMA_HIGH;
	} else if (!strcmp(parm[0], "bls_set")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto failed;
		en = (int)val;
		bls_set();
		bs_proc_en = en;
		pr_info("en = %d\n", en);
	} else if (!strcmp(parm[0], "bls_debug")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto failed;
		bls_debug = (int)val;
		pr_info("bls_debug = %d\n", bls_debug);
	} else if (!strcmp(parm[0], "3dlut_idx")) {
		if (!parm[3]) {
			pr_info("miss param\n");
			goto failed;
		}
		if (kstrtol(parm[1], 10, &val) < 0)
			goto failed;
		idx_r = (int)val;
		if (kstrtol(parm[2], 10, &val) < 0)
			goto failed;
		idx_g = (int)val;
		if (kstrtol(parm[3], 10, &val) < 0)
			goto failed;
		idx_b = (int)val;
		pr_info("idx_r = %d, idx_g = %d, idx_b = %d\n",
			idx_r, idx_g, idx_b);
	}

	return 0;

failed:
	return -EINVAL;

for_read:
	return 0;
}
