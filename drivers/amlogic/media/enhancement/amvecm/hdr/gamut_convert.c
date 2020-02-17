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
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/amvecm/amvecm.h>
#include "gamut_convert.h"

unsigned int gmt_print;
module_param(gmt_print, uint, 0664);
MODULE_PARM_DESC(gmt_print, "print gamut");

/* gamma=2.200000 lumin=500 boost=0.075000 */
static unsigned int scale_factor =
	(unsigned int)((((1.000000) * 4096.0) + 1) / 2);

#define pr_gmt(fmt, args...)\
	do {\
		if (gmt_print)\
			pr_info("gmt_convert: " fmt, ## args);\
	} while (0)\

static void xyz_inv(s64 (*in)[3], s64 (*out)[3],
		    s32 norm, s32 obl)
{
	int i, j;
	s64 determinant = 0;
	s64 tmp;

	determinant =
		in[0][0] * (in[1][1] * in[2][2] - in[1][2] * in[2][1]) -
		in[1][0] * (in[0][1] * in[2][2] - in[0][2] * in[2][1]) +
		in[2][0] * (in[0][1] * in[1][2] - in[0][2] * in[1][1]);

	tmp = determinant + (norm >> 1);
	determinant = div64_s64(tmp, norm);

	out[0][0] = (in[1][1] * in[2][2] - in[1][2] * in[2][1]);
	out[0][1] = (in[0][2] * in[2][1] - in[0][1] * in[2][2]);
	out[0][2] = (in[0][1] * in[1][2] - in[0][2] * in[1][1]);

	out[1][0] = (in[1][2] * in[2][0] - in[1][0] * in[2][2]);
	out[1][1] = (in[0][0] * in[2][2] - in[0][2] * in[2][0]);
	out[1][2] = (in[1][0] * in[0][2] - in[0][0] * in[1][2]);

	out[2][0] = (in[1][0] * in[2][1] - in[1][1] * in[2][0]);
	out[2][1] = (in[0][1] * in[2][0] - in[0][0] * in[2][1]);
	out[2][2] = (in[0][0] * in[1][1] - in[1][0] * in[0][1]);

	for (i = 0; i < 3; i++) {
		for (j = 0; j < 3; j++) {
			out[i][j] = out[i][j] << obl;
			tmp = out[i][j] + (determinant >> 1);
			out[i][j] = div64_s64(tmp, determinant);
		}
	}
}

static void rgb_xyz(s64 (*prmy)[2], s64 (*tout)[3],
		    s32 norm, s32 obl)
{
	int i, j;
	s64 mtx_prmy[4][2];
	s64 xr, yr, zr, xg, yg, zg, xb, yb, zb, xw, yw, zw;
	s64 mtx_temp[3][3];
	s64 mtx_temp_inv[3][3];
	s64 m_w[3];
	s64 m_s[3] = {0};
	s64 mtx_s[3][3];
	s64 tmp;

	for (i = 0; i < 4; i++) {
		for (j = 0; j < 2; j++)
			mtx_prmy[i][j] = prmy[i][j];
	}

	tmp = mtx_prmy[0][0] * norm;
	xr = div64_s64(tmp, mtx_prmy[0][1]);
	yr = norm;
	tmp = (norm - mtx_prmy[0][0] - mtx_prmy[0][1]) * norm;
	zr = div64_s64(tmp, mtx_prmy[0][1]);

	tmp = mtx_prmy[1][0] * norm;
	xg = div64_s64(tmp, mtx_prmy[1][1]);
	yg = norm;
	tmp = (norm - mtx_prmy[1][0] - mtx_prmy[1][1]) * norm;
	zg = div64_s64(tmp, mtx_prmy[1][1]);

	tmp = mtx_prmy[2][0] * norm;
	xb = div64_s64(tmp, mtx_prmy[2][1]);
	yb = norm;
	tmp = (norm - mtx_prmy[2][0] - mtx_prmy[2][1]) * norm;
	zb = div64_s64(tmp, mtx_prmy[2][1]);

	tmp = mtx_prmy[3][0] * norm;
	xw = div64_s64(tmp, mtx_prmy[3][1]);
	yw = norm;
	tmp = (norm - mtx_prmy[3][0] - mtx_prmy[3][1]) * norm;
	zw = div64_s64(tmp, mtx_prmy[3][1]);

	mtx_temp[0][0] = xr;
	mtx_temp[0][1] = xg;
	mtx_temp[0][2] = xb;

	mtx_temp[1][0] = yr;
	mtx_temp[1][1] = yg;
	mtx_temp[1][2] = yb;

	mtx_temp[2][0] = zr;
	mtx_temp[2][1] = zg;
	mtx_temp[2][2] = zb;

	m_w[0] = xw;
	m_w[1] = yw;
	m_w[2] = zw;

	xyz_inv(mtx_temp, mtx_temp_inv, norm, obl);

	for (i = 0; i < 3; i++) {
		for (j = 0; j < 3; j++)
			m_s[i] += mtx_temp_inv[i][j] * m_w[j];

		tmp = m_s[i] + (norm >> 1);
		m_s[i] = div64_s64(tmp, norm);
	}

	for (i = 0; i < 3; i++)
		for (j = 0; j < 3; j++)
			mtx_s[i][j] = m_s[j];

	for (i = 0; i < 3; i++)
		for (j = 0; j < 3; j++) {
			tout[i][j] = mtx_temp[i][j] * mtx_s[i][j];
			tmp = tout[i][j] + (norm >> 1);
			tout[i][j] = div64_s64(tmp, norm);
			pr_gmt("Tout[%d][%d] = 0x%llx\n", i, j, tout[i][j]);
		}
}

static void cal_mtx_out(s64 (*mtx_src)[3],
			s64 (*mtx_dest_inv)[3],
			s64 (*out)[3],
			s32 norm)
{
	int i, j, k;
	s64 tmp;

	pr_gmt("%s\n", __func__);
	for (i = 0; i < 3; i++)
		for (j = 0; j < 3; j++) {
			out[i][j] = 0;
			for (k = 0; k < 3; k++)
				out[i][j] += mtx_dest_inv[i][k] * mtx_src[k][j];

			tmp = out[i][j] + (norm >> 1);
			out[i][j] = div64_s64(tmp, norm);
			pr_gmt("out[%d][%d] = 0x%llx\n", i, j, out[i][j]);
		}
}

static void gamut_proc(s64 (*src_prmy)[2], s64 (*dst_prmy)[2],
		       s64 (*tout)[3], s32 norm, s32 obl)
{
	s64 tsrc[3][3];
	s64 tdst[3][3];
	s64 out[3][3];

	rgb_xyz(src_prmy, tsrc, norm, obl);
	rgb_xyz(dst_prmy, tdst, norm, obl);
	xyz_inv(tdst, out, 1 << obl, obl);
	cal_mtx_out(tsrc, out, tout, 1 << obl);
}

static void apply_scale_factor(s64 (*in)[3], s32 *rs)
{
	int i, j;
	s32 right_shift;

	pr_gmt("%s\n", __func__);

	if (scale_factor > 2 * 2048)
		right_shift = -2;
	else if (scale_factor > 2048)
		right_shift = -1;
	else
		right_shift = 0;
	for (i = 0; i < 3; i++)
		for (j = 0; j < 3; j++) {
			in[i][j] *= scale_factor;
			in[i][j] >>= 11 - right_shift;
		}
	right_shift += 1;
	if (right_shift < 0)
		*rs = 8 + right_shift;
	else
		*rs = right_shift;

	for (i = 0; i < 3; i++) {
		for (j = 0; j < 3; j++)
			pr_gmt("in[%d][%d] = %llx,\n", i, j, in[i][j]);
		pr_gmt("\n");
	}
}

static void N2C(s64 (*in)[3],
		s32 ibl,
		s32 obl,
		int mtx_depth)
{
	int i, j;

	for (i = 0; i < 3; i++)
		for (j = 0; j < 3; j++) {
			in[i][j] =
				(in[i][j] + (1 << (ibl - (mtx_depth + 1)))) >>
				(ibl - mtx_depth);
			if (in[i][j] < 0)
				in[i][j] += 1 << obl;
		}
}

static void cal_mtx_seting(s64 (*in)[3],
			   s32 ibl, s32 obl,
			   struct matrix_s *m,
			   int mtx_depth)
{
	int i, j;
	s32 right_shift = 0;

	apply_scale_factor(in, &right_shift);
	m->right_shift = right_shift;
	N2C(in, ibl, obl, mtx_depth);
	pr_gmt("\tHDR color convert matrix:\n");
	for (i = 0; i < 3; i++) {
		m->pre_offset[i] = 0;
		for (j = 0; j < 3; j++)
			m->matrix[i][j] = in[i][j];
		m->offset[i] = 0;
		pr_gmt("\t\t%04x %04x %04x\n",
		       (int)(m->matrix[i][0] & 0xffff),
		       (int)(m->matrix[i][1] & 0xffff),
		       (int)(m->matrix[i][2] & 0xffff));
	}
}

unsigned int gmt_mtx[3][3];
#define NORM 50000
#define BL		16
static u32 std_bt2020_prmy[3][2] = {
	{0.17 * NORM + 0.5, 0.797 * NORM + 0.5},	/* G */
	{0.131 * NORM + 0.5, 0.046 * NORM + 0.5},	/* B */
	{0.708 * NORM + 0.5, 0.292 * NORM + 0.5},	/* R */
};

static u32 std_bt2020_white_point[2] = {
	0.3127 * NORM + 0.5, 0.3290 * NORM + 0.5
};

static u32 std_bt709_prmy[3][2] = {
	{0.30 * NORM + 0.5, 0.60 * NORM + 0.5},	/* G */
	{0.15 * NORM + 0.5, 0.06 * NORM + 0.5},	/* B */
	{0.64 * NORM + 0.5, 0.33 * NORM + 0.5},	/* R */
};

static u32 std_bt709_white_point[2] = {
	0.3127 * NORM + 0.5, 0.3290 * NORM + 0.5
};

int gamut_convert_process(struct vinfo_s *vinfo,
			  enum hdr_type_e *source_type,
			  enum vd_path_e vd_path,
			  struct matrix_s *mtx,
			  int mtx_depth)
{
	int i, j;
	s64 out[3][3];
	s64 src_prmy[4][2];
	s64 dest_prmy[4][2];
	struct master_display_info_s *p = NULL;

	/*default 11bit*/
	if (mtx_depth == 0)
		mtx_depth = 11;

	if (source_type[vd_path] == HDRTYPE_SDR) {
		for (i = 0; i < 3; i++)
			for (j = 0; j < 2; j++) {
				src_prmy[i][j] = std_bt709_prmy[(i + 2) % 3][j];
				src_prmy[3][j] = std_bt709_white_point[j];
			}
	} else if ((source_type[vd_path] == HDRTYPE_HDR10) ||
		(source_type[vd_path] == HDRTYPE_HLG) ||
		(source_type[vd_path] == HDRTYPE_HDR10PLUS)) {
		for (i = 0; i < 3; i++)
			for (j = 0; j < 2; j++) {
				src_prmy[i][j] =
					std_bt2020_prmy[(i + 2) % 3][j];
				src_prmy[3][j] = std_bt2020_white_point[j];
			}
	}

	if (vinfo->master_display_info.present_flag) {
		p = &vinfo->master_display_info;
		for (i = 0; i < 3; i++)
			for (j = 0; j < 2; j++) {
				dest_prmy[i][j] = p->primaries[(i + 2) % 3][j];
				dest_prmy[3][j] = p->white_point[j];
			}
	} else {
		for (i = 0; i < 3; i++)
			for (j = 0; j < 2; j++) {
				dest_prmy[i][j] =
					std_bt709_prmy[(i + 2) % 3][j];
				dest_prmy[3][j] = std_bt709_white_point[j];
			}
	}

	gamut_proc(src_prmy, dest_prmy, out, NORM, BL);
	cal_mtx_seting(out, BL, BL, mtx, mtx_depth);

	return 0;
}
