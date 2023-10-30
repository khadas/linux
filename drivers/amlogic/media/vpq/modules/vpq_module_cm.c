// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include "vpq_module_cm.h"
#include "../vpq_printk.h"

#define NUM_MATRIX_PARAM    (7)
#define NUM_SMOOTH_PARAM    (11)

static unsigned int lpf_coef[NUM_MATRIX_PARAM] = {
	0, 16, 32, 32, 32, 16, 0,
};

static unsigned int key_pts_9[EN_CAM9_MAX] = {
	5, 9, 12, 15, 17, 19, 23, 25, 29,
};

//static unsigned int pts_start_9[EN_CAM9_MAX] = {
//	1, 7, 11, 14, 16, 18, 23, 25, 29,
//};

//static unsigned int pts_end_9[EN_CAM9_MAX] = {
//	6, 10, 13, 15, 17, 22, 24, 28, 32,
//};

static unsigned int key_pts_14[EN_CAM14_MAX] = {
	2, 4, 7, 9, 11, 13, 14, 16, 18, 20, 22, 25, 28, 30,
};

//static unsigned int pts_start_14[EN_CAM14_MAX] = {
//	1, 3, 6, 8, 10, 12, 14, 15, 17, 20, 22, 25, 28, 30,
//};

//static unsigned int pts_end_14[EN_CAM14_MAX] = {
//	2, 5, 7, 9, 11, 13, 14, 16, 19, 21, 24, 27, 29, 32,
//};

static unsigned int smooth_coef_hue[NUM_SMOOTH_PARAM] = {
	0, 20, 40, 80, 110, 128, 110, 80, 40, 20, 0,
};

static unsigned int smooth_coef_sat[NUM_SMOOTH_PARAM] = {
	40, 60, 85, 105, 115, 120, 115, 105, 85, 60, 30,
};

static unsigned int smooth_coef_luma[NUM_SMOOTH_PARAM] = {
	40, 100, 105, 110, 115, 120, 115, 110, 85, 60, 40,
};

/*
 * [color_adj_by_pts calculate lut table]
 * @param  inp_key_pts [0~32]
 * @param  inp_val     [-128~127]
 * @param  lpf_en      [0~1]
 * @param  lpf_coef    [tab]
 * @param  smooth_coef [tab]
 * @param  out_lut     [output lut]
 */
static void color_adj_by_pts(int inp_key_pts, int inp_val, int lpf_en,
	int *lpf_coef, int *smooth_coef, int *out_lut)
{
	int i = 0;
	int j = 0;
	int tmp = 0;
	int idx = 0;
	int smooth_val[NUM_SMOOTH_PARAM] = {0};

	if (!smooth_coef || !out_lut ||
		inp_key_pts < 0 || inp_key_pts > 32)
		return;

	tmp = max(-128, min(127, inp_val));

	for (i = 0; i < NUM_SMOOTH_PARAM; i++) {
		smooth_val[i] =
			smooth_coef[i] * tmp * 100 / 128;

		if (tmp > 0) {
			smooth_val[i] =
				(smooth_val[i] + 50) / 100;
		} else if (tmp < 0) {
			smooth_val[i] =
				(smooth_val[i] - 50) / 100;
		} else {
			smooth_val[i] = 0;
		}
	}

	for (i = 0; i < NUM_SMOOTH_PARAM; i++) {
		idx = inp_key_pts + i - NUM_SMOOTH_PARAM / 2;
		if (idx < 0)
			idx = 32 - abs(idx);
		else if (idx > 31)
			idx -= 32;

		out_lut[idx] = smooth_val[i];
	}

	if (lpf_en && lpf_coef) {
		for (i = 0; i < 32; i++) {
			tmp = 0;

			for (j = 0; j < NUM_MATRIX_PARAM; j++) {
				idx = i + j - NUM_MATRIX_PARAM / 2;
				if (idx < 0)
					idx = 32 - abs(idx);
				else if (idx > 31)
					idx -= 32;

				tmp += lpf_coef[j] * out_lut[idx];
			}

			out_lut[i] = tmp / 128;
		}
	}
}

void color_adj_by_mode(struct color_adj_param_s param, int *curve_io)
{
	int i = 0;
	int key_pts = 0;
	int *smooth_coef = NULL;

	if (!curve_io)
		return;

	VPQ_PR_INFO(PR_MODULE, "%s param:%d %d %d %d %d %d", __func__,
		param.curve_type, param.color_type, param.mode_9,
		param.mode_14, param.value, param.lpf_en);

	if (param.color_type == EN_COLOR_ADJ_TYPE_9)
		key_pts = key_pts_9[param.mode_9];
	else
		key_pts = key_pts_14[param.mode_14];

	switch (param.curve_type) {
	case EN_COLOR_ADJ_CURVE_HUE_HS:
	case EN_COLOR_ADJ_CURVE_HUE:
		smooth_coef = &smooth_coef_hue[0];
		break;
	case EN_COLOR_ADJ_CURVE_SAT:
		smooth_coef = &smooth_coef_sat[0];
		break;
	case EN_COLOR_ADJ_CURVE_LUMA:
		smooth_coef = &smooth_coef_luma[0];
		break;
	default:
		return;
	}

	color_adj_by_pts(key_pts, param.value, param.lpf_en,
		&lpf_coef[0], smooth_coef, curve_io);

	for (i = 0; i < 32; i++)
		VPQ_PR_INFO(PR_MODULE, "%s curve_io[%d]:%d\n", __func__,
			i, curve_io[i]);
}
