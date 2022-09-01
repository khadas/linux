// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include "vpp_common.h"
#include "vpp_csc.h"
#include "vpp_modules_inc.h"

static enum csc_output_format_e csc_force_output;
/* 0: follow sink, 1: follow source, 2: debug, 0xff: bootup default value */
/* by default follow source to match default sdr_mode*/
static unsigned int hdr_policy;
static unsigned int cur_hdr_policy;
static int hdr_process_status[3]; /*0/1/2: off/normal/bypass*/

static bool customer_mtrx_en;
static bool customer_mtrx_used;
static bool customer_hdmi_display_en;
static bool customer_master_display_en;
static int customer_panel_lumin;

/*Internal functions*/
static void _mtrx_dot_mul(s64 (*a)[3], s64 (*b)[3],
	s64 (*out)[3], int norm)
{
	int i, j;
	s64 tmp;

	if (!a || !b || !out)
		return;

	for (i = 0; i < 3; i++) {
		for (j = 0; j < 3; j++) {
			tmp = a[i][j] * b[i][j] + (norm >> 1);
			out[i][j] = div64_s64(tmp, norm);
		}
	}
}

static void _mtrx_mul(s64 (*a)[3], s64 *b, s64 *out, int norm)
{
	int j, k;
	s64 tmp;

	if (!a || !b || !out)
		return;

	for (j = 0; j < 3; j++) {
		out[j] = 0;
		for (k = 0; k < 3; k++)
			out[j] += a[k][j] * b[k];
		tmp = out[j] + (norm >> 1);
		out[j] = div64_s64(tmp, norm);
	}
}

static void _mtrx_mul_mtrx(s64 (*a)[3], s64 (*b)[3],
	s64 (*out)[3], int norm)
{
	int i, j, k;
	s64 tmp;

	if (!a || !b || !out)
		return;

	for (i = 0; i < 3; i++) {
		for (j = 0; j < 3; j++) {
			out[i][j] = 0;
			for (k = 0; k < 3; k++)
				out[i][j] += a[k][j] * b[i][k];
			tmp = out[i][j] + (norm >> 1);
			out[i][j] = div64_s64(tmp, norm);
		}
	}
}

static void _inverse_3x3(s64 (*in)[3], s64 (*out)[3],
	int norm, int obl)
{
	int i, j;
	s64 determinant = 0;
	s64 tmp;

	if (!in || !out)
		return;

	for (i = 0; i < 3; i++)
		determinant +=
			in[0][i] * (in[1][(i + 1) % 3] * in[2][(i + 2) % 3]
			- in[1][(i + 2) % 3] * in[2][(i + 1) % 3]);
	determinant = (determinant + 1) >> 1;

	for (i = 0; i < 3; i++) {
		for (j = 0; j < 3; j++) {
			out[j][i] = (in[(i + 1) % 3][(j + 1) % 3]
				* in[(i + 2) % 3][(j + 2) % 3]);
			out[j][i] -= (in[(i + 1) % 3][(j + 2) % 3]
				* in[(i + 2) % 3][(j + 1) % 3]);
			out[j][i] = (out[j][i] * norm) << (obl - 1);
			tmp = out[j][i] + (determinant >> 1);
			out[j][i] = div64_s64(tmp, determinant);
		}
	}
}

static void _calculate_t(s64 (*prmy)[2], s64 (*tout)[3],
	int norm, int obl)
{
	int i, j;
	s64 z[4];
	s64 a[3][3];
	s64 b[3];
	s64 c[3];
	s64 d[3][3];
	s64 e[3][3];
	s64 tmp;

	if (!prmy || !tout)
		return;

	for (i = 0; i < 4; i++)
		z[i] = norm - prmy[i][0] - prmy[i][1];

	for (i = 0; i < 3; i++) {
		for (j = 0; j < 2; j++)
			a[i][j] = prmy[i][j];
		a[i][2] = z[i];
	}

	tmp = norm * prmy[3][0] * 2;
	tmp = div64_s64(tmp, prmy[3][1]);

	b[0] = (tmp + 1) >> 1;
	b[1] = norm;

	tmp = norm * z[3] * 2;
	tmp = div64_s64(tmp, prmy[3][1]);

	b[2] = (tmp + 1) >> 1;

	_inverse_3x3(a, d, norm, obl);
	_mtrx_mul(d, b, c, norm);

	for (i = 0; i < 3; i++)
		for (j = 0; j < 3; j++)
			e[i][j] = c[i];

	_mtrx_dot_mul(a, e, tout, norm);
}

static void _apply_scale_factor(s64 (*in)[3], unsigned int factor, int *rs)
{
	int i, j;
	int right_shift;

	if (!in || !rs)
		return;

	if (factor > 2 * 2048)
		right_shift = -2;
	else if (factor > 2048)
		right_shift = -1;
	else
		right_shift = 0;

	for (i = 0; i < 3; i++) {
		for (j = 0; j < 3; j++) {
			in[i][j] *= factor;
			in[i][j] >>= 11 - right_shift;
		}
	}

	right_shift += 1;
	if (right_shift < 0)
		*rs = 8 + right_shift;
	else
		*rs = right_shift;
}

static void _n2c(s64 (*in)[3], int ibl, int obl)
{
	int i, j;

	if (!in)
		return;

	for (i = 0; i < 3; i++) {
		for (j = 0; j < 3; j++) {
			in[i][j] = (in[i][j] + (1 << (ibl - 12))) >> (ibl - 11);

			if (in[i][j] < 0)
				in[i][j] += 1 << obl;
		}
	}
}

/*eo clip calculate according to luminance*/
static unsigned int _eo_clip_maxl(unsigned int max_luma,
	int *ptbl_prmy_maxl, int *ptbl_margin_maxl)
{
	int i;
	unsigned int clip_max_luma = 0;

	for (i = 0; i < 7; i++) {
		if (max_luma <= ptbl_prmy_maxl[i]) {
			clip_max_luma = max_luma + ((max_luma * ptbl_margin_maxl[i]) >> 10);
			return clip_max_luma;
		}
	}

	return clip_max_luma;
}

/*External functions*/
int vpp_csc_init(struct vpp_dev_s *pdev)
{
	enum vpp_chip_type_e chip_id;
	int i;

	chip_id = pdev->pm_data->chip_id;

	csc_force_output = EN_UNKNOWN;
	hdr_policy = 0xff;
	cur_hdr_policy = 0xff;

	customer_mtrx_en = true;

	if (chip_id > CHIP_TXHD)
		customer_mtrx_used = false;
	else
		customer_mtrx_used = true;

	customer_hdmi_display_en = false;

	for (i = 0; i < 3; i++)
		hdr_process_status[i] = 2;

	customer_panel_lumin = 380;

	return 0;
}

enum csc_output_format_e vpp_csc_get_force_output(void)
{
	return csc_force_output;
}

void vpp_csc_set_force_output(enum csc_output_format_e format)
{
	csc_force_output = format;
}

int vpp_csc_get_hdr_policy(void)
{
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	int dv_policy = 0;
	int dv_mode = 0;

	if (is_amdv_enable()) {
		/*sync hdr_policy with dolby_vision_policy*/
		/*get current dolby_vision_mode*/
		dv_policy = get_amdv_policy();
		dv_mode = get_amdv_target_mode();

		if (dv_policy != AMDV_FORCE_OUTPUT_MODE ||
			dv_mode != AMDV_OUTPUT_MODE_BYPASS)
			return dv_policy; /*use dv policy when not force bypass*/
	}
#endif

	return hdr_policy;
}

void vpp_csc_set_hdr_policy(int val)
{
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	if (is_amdv_enable()) {
		set_amdv_policy(val);
		return;
	}
#endif

	hdr_policy = val;
}

void vpp_csc_calculate_gamut_mtrx(s64 (*src_prmy)[2], s64 (*dst_prmy)[2],
	s64 (*tout)[3], int norm, int obl)
{
	s64 tsrc[3][3] = {0};
	s64 tdst[3][3] = {0};
	s64 out[3][3] = {0};

	_calculate_t(src_prmy, tsrc, norm, obl);
	_calculate_t(dst_prmy, tdst, norm, obl);
	_inverse_3x3(tdst, out, 1 << obl, obl);
	_mtrx_mul_mtrx(out, tsrc, tout, 1 << obl);
}

void vpp_csc_calculate_mtrx_setting(s64 (*in)[3],
	int ibl, int obl, struct csc_mtrx_param_s *m)
{
	int i, j;
	unsigned int factor = 0;
	int right_shift = 0;

	if (!in || !m)
		return;

	_apply_scale_factor(in, factor, &right_shift);
	m->right_shift = right_shift;

	_n2c(in, ibl, obl);

	for (i = 0; i < 3; i++) {
		m->pre_offset[i] = 0;
		m->offset[i] = 0;

		for (j = 0; j < 3; j++)
			m->coef[i][j] = in[j][i];
	}
}

void vpp_csc_set_customer_mtrx_en(bool val)
{
	customer_mtrx_en = val;
}

bool vpp_csc_get_customer_mtrx_en(void)
{
	return customer_mtrx_en;
}

bool vpp_csc_get_customer_mtrx_used(void)
{
	return customer_mtrx_used;
}

void vpp_csc_set_customer_hdmi_display_en(bool val)
{
	customer_hdmi_display_en = val;
}

bool vpp_csc_get_customer_hdmi_display_en(void)
{
	return customer_hdmi_display_en;
}

void vpp_csc_set_customer_master_display_en(bool val)
{
	customer_master_display_en = val;
}

bool vpp_csc_get_customer_master_display_en(void)
{
	return customer_master_display_en;
}

void vpp_csc_set_customer_panel_lumin(int val)
{
	customer_panel_lumin = val;
}

int vpp_csc_get_customer_panel_lumin(void)
{
	return customer_panel_lumin;
}

void vpp_csc_set_hdr_process_status(int vd_path, int val)
{
	if (vd_path < 3)
		hdr_process_status[vd_path] = val;
}

int vpp_csc_get_hdr_process_status(int vd_path)
{
	if (vd_path < 3)
		return hdr_process_status[vd_path];

	return 0;
}

void vpp_csc_eo_clip_proc(unsigned int max_luma,
	unsigned int present_flag, int *ptbl_prmy_maxl, int *ptbl_margin_maxl,
	int *peo_y_lut_hdr_10000, int *peo_y_lut_src, int *peo_y_lut_dest)
{
	unsigned int clip_index = HDR_EOTF_LUT_SIZE - 1;
	unsigned int ret_luma;
	int i;

	if (max_luma < 100) /*invalid luminance*/
		max_luma = 0;
	else if (max_luma > 10000)
		max_luma /= 10000;

	ret_luma = _eo_clip_maxl(max_luma,
		ptbl_prmy_maxl, ptbl_margin_maxl);

	if (ret_luma > 10000)
		ret_luma = 10000;

	for (i = HDR_EOTF_LUT_SIZE - 1; i >= 0; i--) {
		if (max_luma == 0 || present_flag == 0) {
			if (peo_y_lut_hdr_10000[i] < 1200) {/*default 1000 luminance*/
				clip_index = i + 1;
				break;
			}
		}

		if (peo_y_lut_hdr_10000[i] < ret_luma) {
			clip_index = i + 1;
			break;
		}
	}

	if (clip_index > HDR_EOTF_LUT_SIZE - 1)
		clip_index = HDR_EOTF_LUT_SIZE - 1;

	for (i = 0; i < HDR_EOTF_LUT_SIZE; i++) {
		if (i < clip_index)
			peo_y_lut_dest[i] = peo_y_lut_src[i];
		else
			peo_y_lut_dest[i] = peo_y_lut_src[clip_index];
	}
}

