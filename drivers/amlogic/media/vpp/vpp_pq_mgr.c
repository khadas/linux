// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include "vpp_common.h"
#include "vpp_pq_mgr.h"
#include "vpp_modules_inc.h"
#include "vpp_vf_proc.h"
#include "vpp_data.h"

#define RET_POINT_FAIL (-1)

static enum vpp_chip_type_e chip_type;
static struct vpp_pq_mgr_settings pq_mgr_settings;

/*For 3D LUT*/
static bool lut3d_db_initial;
static unsigned char *pkey_lut_all;

/*Internal functions*/
static int _modules_init(struct vpp_dev_s *pdev)
{
	vpp_module_vadj_init(pdev);
	vpp_module_matrix_init(pdev);
	vpp_module_gamma_init(pdev);
	vpp_module_go_init(pdev);
	vpp_module_meter_init(pdev);
	vpp_module_ve_init(pdev);
	vpp_module_dnlp_init(pdev);
	vpp_module_lc_init(pdev);
	vpp_module_cm_init(pdev);
	vpp_module_sr_init(pdev);
	vpp_vf_init(pdev);

	return 0;
}

static void _buffer_init(void)
{
	unsigned int len = 0;
	unsigned int buf_size = 0;

	len = vpp_module_pre_gamma_get_table_len();
	if (len != 0) {
		buf_size = len * sizeof(unsigned int);
		pq_mgr_settings.cur_pre_gamma_tbl.r_data =
			kmalloc(buf_size, GFP_KERNEL);
		pq_mgr_settings.cur_pre_gamma_tbl.g_data =
			kmalloc(buf_size, GFP_KERNEL);
		pq_mgr_settings.cur_pre_gamma_tbl.b_data =
			kmalloc(buf_size, GFP_KERNEL);
	}

	len = vpp_module_lcd_gamma_get_table_len();
	if (len != 0) {
		buf_size = len * sizeof(unsigned int);
		pq_mgr_settings.cur_gamma_tbl.r_data =
			kmalloc(buf_size, GFP_KERNEL);
		pq_mgr_settings.cur_gamma_tbl.g_data =
			kmalloc(buf_size, GFP_KERNEL);
		pq_mgr_settings.cur_gamma_tbl.b_data =
			kmalloc(buf_size, GFP_KERNEL);
	}
}

static void _buffer_free(void)
{
	kfree(pq_mgr_settings.cur_pre_gamma_tbl.r_data);
	kfree(pq_mgr_settings.cur_pre_gamma_tbl.g_data);
	kfree(pq_mgr_settings.cur_pre_gamma_tbl.b_data);
	kfree(pq_mgr_settings.cur_gamma_tbl.r_data);
	kfree(pq_mgr_settings.cur_gamma_tbl.g_data);
	kfree(pq_mgr_settings.cur_gamma_tbl.b_data);
}

static int _set_default_settings(void)
{
	vpp_pq_mgr_set_brightness(0);
	vpp_pq_mgr_set_contrast(0);
	vpp_pq_mgr_set_saturation(0);
	vpp_pq_mgr_set_hue(0);
	vpp_pq_mgr_set_brightness_post(0);
	vpp_pq_mgr_set_contrast_post(0);
	vpp_pq_mgr_set_saturation_post(0);
	vpp_pq_mgr_set_hue_post(0);

	/*WB*/
	vpp_module_go_set_pre_offset(0, 0);
	vpp_module_go_set_pre_offset(1, 0);
	vpp_module_go_set_pre_offset(2, 0);
	vpp_module_go_set_gain(0, 1024);
	vpp_module_go_set_gain(1, 1024);
	vpp_module_go_set_gain(2, 1024);
	vpp_module_go_set_offset(0, 0);
	vpp_module_go_set_offset(1, 0);
	vpp_module_go_set_offset(2, 0);

	/*Gamma*/
	vpp_module_pre_gamma_set_default();
	vpp_module_lcd_gamma_set_default();

	/*CM*/
	vpp_module_cm_set_default();

	return 0;
}

static int _get_mab_from_sat_hue(int sat_val, int hue_val)
{
	int i, ma, mb, mab;

	int hue_cos[] = {
		256, 256, 256, 255, 255, 254, 253, 252, 251, 250, 248, 247, 245, /*0~12*/
		243, 241, 239, 237, 234, 231, 229, 226, 223, 220, 216, 213, 209  /*13~25*/
	};

	int hue_sin[] = {
		-147, -142, -137, -132, -126, -121, -115, -109, -104, -98, /*-25~-16*/
		-92, -86, -80, -74, -68, -62, -56, -50, -44, -38, /*-15~--6*/
		-31, -25, -19, -13, -6, 0, 6, 13, 19, 25, 31, /*-5~5*/
		38, 44, 50, 56, 62, 68, 74, 80, 86, 92, /*6~15*/
		98, 104, 109, 115, 121, 126, 132, 137, 142, 147 /*16~25*/
	};

	sat_val = vpp_check_range(sat_val, (-128), 127);
	hue_val = vpp_check_range(hue_val, (-25), 25);

	i = (hue_val > 0) ? hue_val : (-hue_val);
	ma = (hue_cos[i] * (sat_val + 128)) >> 7;
	mb = (hue_sin[hue_val + 25] * (sat_val + 128)) >> 7;

	ma = vpp_check_range(ma, (-512), 511);
	mb = vpp_check_range(mb, (-512), 511);

	mab = ((ma & 0x03ff) << 16) | (mb & 0x03ff);

	return mab;
}

static void _set_pq_bypass_all(bool enable)
{
	struct vpp_pq_state_s *pstatus = &pq_mgr_settings.pq_status;

	pr_vpp(PR_DEBUG, "[%s] pq_bypass_all = %d\n", __func__, enable);

	if (enable) {
		vpp_module_vadj_en(false);
		vpp_module_vadj_post_en(false);
		vpp_module_vadj_set_param(EN_VADJ_VD1_RGBBST_EN, false);
		vpp_module_vadj_set_param(EN_VADJ_POST_RGBBST_EN, false);
		vpp_module_pre_gamma_en(false);
		vpp_module_lcd_gamma_en(false);
		vpp_module_go_en(false);
		vpp_module_dnlp_en(false);
		vpp_module_lc_en(false);
		vpp_module_ve_blkext_en(false);
		vpp_module_ve_ccoring_en(false);
		vpp_module_sr_en(EN_MODE_SR_0, false);
		vpp_module_sr_en(EN_MODE_SR_1, false);
		vpp_module_cm_en(false);
		vpp_module_lut3d_en(false);
	} else {
		vpp_module_vadj_en(pstatus->pq_cfg.vadj1_en);
		vpp_module_vadj_post_en(pstatus->pq_cfg.vadj2_en);
		vpp_module_vadj_set_param(EN_VADJ_VD1_RGBBST_EN, pstatus->pq_cfg.vd1_ctrst_en);
		vpp_module_vadj_set_param(EN_VADJ_POST_RGBBST_EN, pstatus->pq_cfg.post_ctrst_en);
		vpp_module_pre_gamma_en(pstatus->pq_cfg.pregamma_en);
		vpp_module_lcd_gamma_en(pstatus->pq_cfg.gamma_en);
		vpp_module_go_en(pstatus->pq_cfg.wb_en);
		vpp_module_dnlp_en(pstatus->pq_cfg.dnlp_en);
		vpp_module_lc_en(pstatus->pq_cfg.lc_en);
		vpp_module_ve_blkext_en(pstatus->pq_cfg.black_ext_en);
		vpp_module_ve_ccoring_en(pstatus->pq_cfg.chroma_cor_en);
		vpp_module_sr_en(EN_MODE_SR_0, pstatus->pq_cfg.sharpness0_en);
		vpp_module_sr_en(EN_MODE_SR_1, pstatus->pq_cfg.sharpness1_en);
		vpp_module_cm_en(pstatus->pq_cfg.cm_en);
		vpp_module_lut3d_en(pstatus->pq_cfg.lut3d_en);
	}
}

static void _mtrx_mul_mtrx(int (*mtrx_a)[3], int (*mtrx_b)[3], int (*mtrx_out)[3])
{
	int i, j, k;

	for (i = 0; i < 3; i++) {
		for (j = 0; j < 3; j++) {
			mtrx_out[i][j] = 0;
			for (k = 0; k < 3; k++)
				mtrx_out[i][j] += mtrx_a[i][k] * mtrx_b[k][j];
		}
	}

	for (i = 0; i < 3; i++) {
		for (j = 0; j < 3; j++)
			mtrx_out[i][j] = (mtrx_out[i][j] + (1 << 9)) >> 10;
	}
}

static void _mtrx_multi(int *rgb, int *coef)
{
	int i, j, k;
	int mtrx_rgb[3][3] = {0};
	int mtrx_in[3][3] = {0};
	int mtrx_out[3][3] = {0};

	int mtrx_rgbto709l[3][3] = {
		{187, 629, 63},
		{-103, -346, 450},
		{450, -409, -41},
	};
	int mtrx_709ltorgb[3][3] = {
		{1192, 0, 1836},
		{1192, -218, -547},
		{1192, 2166, 0},
	};

	if (!rgb || !coef)
		return;

	mtrx_in[0][0] = rgb[0];
	mtrx_in[1][1] = rgb[1];
	mtrx_in[2][2] = rgb[2];

	if (mtrx_in[0][0] == 0x400 &&
		mtrx_in[1][1] == 0x400 &&
		mtrx_in[2][2] == 0x400) {
		for (i = 0; i < 3; i++) {
			for (j = 0; j < 3; j++) {
				if (i == j)
					mtrx_out[i][j] = 0x400;
				else
					mtrx_out[i][j] = 0;
			}
		}
	} else {
		_mtrx_mul_mtrx(mtrx_in, mtrx_709ltorgb, mtrx_rgb);
		_mtrx_mul_mtrx(mtrx_rgbto709l, mtrx_rgb, mtrx_out);
	}

	k = 0;
	for (i = 0; i < 3; i++) {
		for (j = 0; j < 3; j++) {
			coef[k] = mtrx_out[i][j];
			k++;
			pr_vpp(PR_DEBUG, "[%s] mtrx_out[%d][%d] = 0x%x\n",
				__func__, i, j, mtrx_out[i][j]);
		}
	}
}

static void _str_sapr_to_d(char *s, int *d, int n)
{
	int i, j, count;
	long value;
	char des[9] = {0};

	count = (strlen(s) + n - 2) / (n - 1);
	for (i = 0; i < count; i++) {
		for (j = 0; j < n - 1; j++)
			des[j] = s[j + i * (n - 1)];
		des[n - 1] = '\0';

		if (kstrtol(des, 10, &value) < 0)
			return;

		d[i] = value;
	}
}

#ifdef CONFIG_AMLOGIC_LCD
static int _lcd_gamma_notifier(struct notifier_block *nb,
	unsigned long event, void *data)
{
	if ((event & LCD_EVENT_GAMMA_UPDATE) == 0)
		return NOTIFY_DONE;

	vpp_module_gamma_set_viu_sel(*(int *)data);
	vpp_module_lcd_gamma_notify();

	return NOTIFY_OK;
}

static struct notifier_block nb_lcd_gamma = {
	.notifier_call = _lcd_gamma_notifier,
};
#endif

/*External functions*/
int vpp_pq_mgr_init(struct vpp_dev_s *pdev)
{
	int ret = 0;

	chip_type = pdev->pm_data->chip_id;

	memset(&pq_mgr_settings, 0, sizeof(struct vpp_pq_mgr_settings));

	_modules_init(pdev);
	_buffer_init();
	_set_default_settings();

	lut3d_db_initial = false;

#ifdef CONFIG_AMLOGIC_LCD
	ret = aml_lcd_notifier_register(&nb_lcd_gamma);
	if (ret)
		PR_DRV("register aml_lcd_gamma_notifier failed\n");
#endif

	pq_mgr_settings.pq_mgr_init_done = true;

	return ret;
}
EXPORT_SYMBOL(vpp_pq_mgr_init);

void vpp_pq_mgr_deinit(void)
{
#ifdef CONFIG_AMLOGIC_LCD
	aml_lcd_notifier_unregister(&nb_lcd_gamma);
#endif

	_buffer_free();

	pq_mgr_settings.pq_mgr_init_done = false;
}
EXPORT_SYMBOL(vpp_pq_mgr_deinit);

void vpp_pq_mgr_set_default_settings(void)
{
	_set_default_settings();
}
EXPORT_SYMBOL(vpp_pq_mgr_set_default_settings);

int vpp_pq_mgr_set_status(struct vpp_pq_state_s *pstatus)
{
	int ret = 0;

	if (pq_mgr_settings.bypass_top_set)
		return ret;

	if (!pstatus)
		return RET_POINT_FAIL;

	memcpy(&pq_mgr_settings.pq_status, pstatus, sizeof(struct vpp_pq_state_s));

	pr_vpp(PR_DEBUG, "[%s] pq_en = %d\n", __func__, pstatus->pq_en);

	if (!pstatus->pq_en) {
		_set_pq_bypass_all(true);
	} else {
		vpp_module_vadj_en(pstatus->pq_cfg.vadj1_en);
		vpp_module_vadj_post_en(pstatus->pq_cfg.vadj2_en);
		vpp_module_vadj_set_param(EN_VADJ_VD1_RGBBST_EN, pstatus->pq_cfg.vd1_ctrst_en);
		vpp_module_vadj_set_param(EN_VADJ_POST_RGBBST_EN, pstatus->pq_cfg.post_ctrst_en);
		vpp_module_pre_gamma_en(pstatus->pq_cfg.pregamma_en);
		vpp_module_lcd_gamma_en(pstatus->pq_cfg.gamma_en);
		vpp_module_go_en(pstatus->pq_cfg.wb_en);
		vpp_module_dnlp_en(pstatus->pq_cfg.dnlp_en);
		vpp_module_lc_en(pstatus->pq_cfg.lc_en);
		vpp_module_ve_blkext_en(pstatus->pq_cfg.black_ext_en);
		vpp_module_ve_ccoring_en(pstatus->pq_cfg.chroma_cor_en);
		vpp_module_sr_en(EN_MODE_SR_0, pstatus->pq_cfg.sharpness0_en);
		vpp_module_sr_en(EN_MODE_SR_1, pstatus->pq_cfg.sharpness1_en);
		vpp_module_cm_en(pstatus->pq_cfg.cm_en);
		vpp_module_lut3d_en(pstatus->pq_cfg.lut3d_en);
	}

	return ret;
}
EXPORT_SYMBOL(vpp_pq_mgr_set_status);

int vpp_pq_mgr_set_brightness(int val)
{
	int ret = 0;

	if (pq_mgr_settings.bypass_top_set)
		return ret;

	pq_mgr_settings.brightness = val;
	val = vpp_check_range(val, (-1024), 1023);

	if (chip_type < CHIP_G12A)
		val = val >> 1;

	pr_vpp(PR_DEBUG, "[%s] brightness = %d\n", __func__, val);

	ret = vpp_module_vadj_set_brightness(val);

	return ret;
}
EXPORT_SYMBOL(vpp_pq_mgr_set_brightness);

void vpp_pq_mgr_set_blkext_params(int *pdata, int length)
{
	int i;

	if (length != EN_BLKEXT_MAX || !pdata) {
		pr_vpp(PR_DEBUG, "[%s] length = %d\n", __func__, length);
		return;
	}

	for (i = 0; i < length; i++)
		pr_vpp(PR_DEBUG, "[%s] pdata[%d] = %d\n", __func__, i, pdata[i]);

	vpp_module_ve_set_blkext_params(pdata);
}
EXPORT_SYMBOL(vpp_pq_mgr_set_blkext_params);

void vpp_pq_mgr_set_blue_stretch_params(int *pdata, int length)
{
	int i;

	if (length != EN_BLUE_STRETCH_MAX || !pdata) {
		pr_vpp(PR_DEBUG, "[%s] length = %d\n", __func__, length);
		return;
	}

	for (i = 0; i < length; i++)
		pr_vpp(PR_DEBUG, "[%s] pdata[%d] = %d\n", __func__, i, pdata[i]);

	vpp_module_ve_set_blue_stretch_params(pdata);
}
EXPORT_SYMBOL(vpp_pq_mgr_set_blue_stretch_params);

int vpp_pq_mgr_set_brightness_post(int val)
{
	int ret = 0;

	if (pq_mgr_settings.bypass_top_set)
		return ret;

	pq_mgr_settings.brightness_post = val;
	val = vpp_check_range(val, (-1024), 1023);

	if (chip_type < CHIP_G12A)
		val = val >> 1;

	pr_vpp(PR_DEBUG, "[%s] brightness_post = %d\n", __func__, val);

	ret = vpp_module_vadj_set_brightness_post(val);

	return ret;
}
EXPORT_SYMBOL(vpp_pq_mgr_set_brightness_post);

int vpp_pq_mgr_set_contrast(int val)
{
	int ret = 0;
	int contrast_uv = 0;
	int contrast_u = 0;
	int contrast_v = 0;

	if (pq_mgr_settings.bypass_top_set)
		return ret;

	pq_mgr_settings.contrast = val;
	val = vpp_check_range(val, (-1024), 1023);

	contrast_uv = val + 1024;
	val = (val + 1024) >> 3;

	if (chip_type > CHIP_TXHD) {
		if (contrast_uv < 1024)
			contrast_u = contrast_uv;
		else
			contrast_u = (contrast_uv >> 1) + 512;

		contrast_v = contrast_uv;

		pr_vpp(PR_DEBUG,
			"[%s] contrast_u = %d, contrast_v = %d\n",
			__func__, contrast_u, contrast_v);

		/*ret = vpp_module_matrix_set_contrast_uv(contrast_u, contrast_v);*/
	}

	pr_vpp(PR_DEBUG, "[%s] contrast = %d\n", __func__, val);

	ret = vpp_module_vadj_set_contrast(val);

	return ret;
}
EXPORT_SYMBOL(vpp_pq_mgr_set_contrast);

int vpp_pq_mgr_set_contrast_post(int val)
{
	int ret = 0;

	if (pq_mgr_settings.bypass_top_set)
		return ret;

	pq_mgr_settings.contrast_post = val;
	val = vpp_check_range(val, (-1024), 1023);

	val = (val + 1024) >> 3;

	pr_vpp(PR_DEBUG, "[%s] contrast_post = %d\n", __func__, val);

	ret = vpp_module_vadj_set_contrast_post(val);

	return ret;
}
EXPORT_SYMBOL(vpp_pq_mgr_set_contrast_post);

int vpp_pq_mgr_set_saturation(int sat_val)
{
	int ret = 0;
	int val = 0;
	int hue_val = pq_mgr_settings.hue;

	if (pq_mgr_settings.bypass_top_set)
		return ret;

	pq_mgr_settings.saturation = sat_val;
	sat_val = vpp_check_range(sat_val, (-128), 127);
	hue_val = vpp_check_range(hue_val, (-25), 25);

	val = _get_mab_from_sat_hue(sat_val, hue_val);

	pr_vpp(PR_DEBUG, "[%s] sat_val/hue_val/mab = %d/%d/%d\n",
		__func__, sat_val, hue_val, val);

	ret = vpp_module_vadj_set_sat_hue(val);

	return ret;
}
EXPORT_SYMBOL(vpp_pq_mgr_set_saturation);

int vpp_pq_mgr_set_saturation_post(int sat_val)
{
	int ret = 0;
	int val = 0;
	int hue_val = pq_mgr_settings.hue_post;

	if (pq_mgr_settings.bypass_top_set)
		return ret;

	pq_mgr_settings.saturation_post = sat_val;
	sat_val = vpp_check_range(sat_val, (-128), 127);
	hue_val = vpp_check_range(hue_val, (-25), 25);

	val = _get_mab_from_sat_hue(sat_val, hue_val);

	pr_vpp(PR_DEBUG, "[%s] sat_val/hue_val/mab_post = %d/%d/%d\n",
		__func__, sat_val, hue_val, val);

	ret = vpp_module_vadj_set_sat_hue_post(val);

	return ret;
}
EXPORT_SYMBOL(vpp_pq_mgr_set_saturation_post);

int vpp_pq_mgr_set_hue(int hue_val)
{
	int ret = 0;
	int val = 0;
	int sat_val = pq_mgr_settings.saturation;

	if (pq_mgr_settings.bypass_top_set)
		return ret;

	pq_mgr_settings.hue = hue_val;
	hue_val = vpp_check_range(hue_val, (-25), 25);
	sat_val = vpp_check_range(sat_val, (-128), 127);

	val = _get_mab_from_sat_hue(sat_val, hue_val);

	pr_vpp(PR_DEBUG, "[%s] sat_val/hue_val/mab = %d/%d/%d\n",
		__func__, sat_val, hue_val, val);

	ret = vpp_module_vadj_set_sat_hue(val);

	return ret;
}
EXPORT_SYMBOL(vpp_pq_mgr_set_hue);

int vpp_pq_mgr_set_hue_post(int hue_val)
{
	int ret = 0;
	int val = 0;
	int sat_val = pq_mgr_settings.saturation_post;

	if (pq_mgr_settings.bypass_top_set)
		return ret;

	pq_mgr_settings.hue_post = hue_val;
	hue_val = vpp_check_range(hue_val, (-25), 25);
	sat_val = vpp_check_range(sat_val, (-128), 127);

	val = _get_mab_from_sat_hue(sat_val, hue_val);

	pr_vpp(PR_DEBUG, "[%s] sat_val/hue_val/mab_post = %d/%d/%d\n",
		__func__, sat_val, hue_val, val);

	ret = vpp_module_vadj_set_sat_hue_post(val);

	return ret;
}
EXPORT_SYMBOL(vpp_pq_mgr_set_hue_post);

int vpp_pq_mgr_set_sharpness(int val)
{
	if (pq_mgr_settings.bypass_top_set)
		return 0;

	pq_mgr_settings.sharpness = val;
	val = vpp_check_range(val, 0, 255);

	pr_vpp(PR_DEBUG, "[%s] sharpness = %d\n", __func__, val);

	vpp_module_sr_set_osd_gain(EN_MODE_SR_0, val, val);
	vpp_module_sr_set_osd_gain(EN_MODE_SR_1, val, val);

	return 0;
}
EXPORT_SYMBOL(vpp_pq_mgr_set_sharpness);

int vpp_pq_mgr_set_whitebalance(struct vpp_white_balance_s *pdata)
{
	int ret = 0;

	if (pq_mgr_settings.bypass_top_set)
		return ret;

	if (!pdata)
		return RET_POINT_FAIL;

	pr_vpp(PR_DEBUG, "[%s] r_pre_offset = %d\n",
		__func__, pdata->r_pre_offset);
	pr_vpp(PR_DEBUG, "[%s] g_pre_offset = %d\n",
		__func__, pdata->g_pre_offset);
	pr_vpp(PR_DEBUG, "[%s] b_pre_offset = %d\n",
		__func__, pdata->b_pre_offset);
	pr_vpp(PR_DEBUG, "[%s] r_gain = %d\n",
		__func__, pdata->r_gain);
	pr_vpp(PR_DEBUG, "[%s] g_gain = %d\n",
		__func__, pdata->g_gain);
	pr_vpp(PR_DEBUG, "[%s] b_gain = %d\n",
		__func__, pdata->b_gain);
	pr_vpp(PR_DEBUG, "[%s] r_post_offset = %d\n",
		__func__, pdata->r_post_offset);
	pr_vpp(PR_DEBUG, "[%s] g_post_offset = %d\n",
		__func__, pdata->g_post_offset);
	pr_vpp(PR_DEBUG, "[%s] b_post_offset = %d\n",
		__func__, pdata->b_post_offset);

	memcpy(&pq_mgr_settings.video_rgb_ogo, pdata,
		sizeof(struct vpp_white_balance_s));

	vpp_module_go_set_pre_offset(0,
		vpp_check_range(pdata->r_pre_offset, (-1024), 1023));
	vpp_module_go_set_pre_offset(1,
		vpp_check_range(pdata->g_pre_offset, (-1024), 1023));
	vpp_module_go_set_pre_offset(2,
		vpp_check_range(pdata->b_pre_offset, (-1024), 1023));

	vpp_module_go_set_gain(0,
		vpp_check_range(pdata->r_gain, 0, 2047));
	vpp_module_go_set_gain(1,
		vpp_check_range(pdata->g_gain, 0, 2047));
	vpp_module_go_set_gain(2,
		vpp_check_range(pdata->b_gain, 0, 2047));

	vpp_module_go_set_offset(0,
		vpp_check_range(pdata->r_post_offset, (-1024), 1023));
	vpp_module_go_set_offset(1,
		vpp_check_range(pdata->g_post_offset, (-1024), 1023));
	vpp_module_go_set_offset(2,
		vpp_check_range(pdata->b_post_offset, (-1024), 1023));

	return ret;
}
EXPORT_SYMBOL(vpp_pq_mgr_set_whitebalance);

int vpp_pq_mgr_set_pre_gamma_table(struct vpp_gamma_table_s *pdata)
{
	int ret = 0;
	unsigned int len = 0;
	unsigned int buf_size = 0;

	if (pq_mgr_settings.bypass_top_set)
		return ret;

	if (!pdata || !pdata->r_data || !pdata->g_data || !pdata->b_data)
		return RET_POINT_FAIL;

	len = vpp_module_pre_gamma_get_table_len();
	if (len == 0)
		return RET_POINT_FAIL;

	buf_size = len * sizeof(unsigned int);

	pr_vpp(PR_DEBUG, "[%s] set data\n", __func__);

	memcpy(&pq_mgr_settings.cur_pre_gamma_tbl.r_data,
		pdata->r_data, buf_size);
	memcpy(&pq_mgr_settings.cur_pre_gamma_tbl.g_data,
		pdata->g_data, buf_size);
	memcpy(&pq_mgr_settings.cur_pre_gamma_tbl.b_data,
		pdata->b_data, buf_size);

	ret = vpp_module_pre_gamma_write(pdata->r_data,
		pdata->g_data, pdata->b_data);

	return ret;
}
EXPORT_SYMBOL(vpp_pq_mgr_set_pre_gamma_table);

int vpp_pq_mgr_set_gamma_table(struct vpp_gamma_table_s *pdata)
{
	int ret = 0;
	unsigned int len = 0;
	unsigned int buf_size = 0;

	if (pq_mgr_settings.bypass_top_set)
		return ret;

	if (!pdata || !pdata->r_data || !pdata->g_data || !pdata->b_data)
		return RET_POINT_FAIL;

	len = vpp_module_lcd_gamma_get_table_len();
	if (len == 0)
		return RET_POINT_FAIL;

	buf_size = len * sizeof(unsigned int);

	pr_vpp(PR_DEBUG, "[%s] set data\n", __func__);

	memcpy(&pq_mgr_settings.cur_gamma_tbl.r_data,
		pdata->r_data, buf_size);
	memcpy(&pq_mgr_settings.cur_gamma_tbl.g_data,
		pdata->g_data, buf_size);
	memcpy(&pq_mgr_settings.cur_gamma_tbl.b_data,
		pdata->b_data, buf_size);

	ret = vpp_module_lcd_gamma_write(pdata->r_data,
		pdata->g_data, pdata->b_data);

	return ret;
}
EXPORT_SYMBOL(vpp_pq_mgr_set_gamma_table);

int vpp_pq_mgr_set_cm_curve(enum vpp_cm_curve_type_e type, int *pdata)
{
	if (pq_mgr_settings.bypass_top_set)
		return 0;

	if (!pdata)
		return RET_POINT_FAIL;

	pr_vpp(PR_DEBUG, "[%s] set data, type = %d\n", __func__, type);

	switch (type) {
	case EN_CM_LUMA:
		vpp_module_cm_set_cm2_luma(pdata);
		break;
	case EN_CM_SAT:
		vpp_module_cm_set_cm2_sat(pdata);
		break;
	case EN_CM_HUE:
		vpp_module_cm_set_cm2_hue(pdata);
		break;
	case EN_CM_HUE_BY_HS:
		vpp_module_cm_set_cm2_hue_by_hs(pdata);
		break;
	case EN_CM_SAT_BY_L:
		vpp_module_cm_set_cm2_sat_by_l(pdata);
		break;
	case EN_CM_SAT_BY_HL:
		vpp_module_cm_set_cm2_sat_by_hl(pdata);
		break;
	case EN_CM_HUE_BY_HL:
		vpp_module_cm_set_cm2_hue_by_hl(pdata);
		break;
	}

	return 0;
}
EXPORT_SYMBOL(vpp_pq_mgr_set_cm_curve);

int vpp_pq_mgr_set_cm_offset_curve(enum vpp_cm_curve_type_e type, char *pdata)
{
	if (pq_mgr_settings.bypass_top_set)
		return 0;

	if (!pdata)
		return RET_POINT_FAIL;

	pr_vpp(PR_DEBUG, "[%s] set data, type = %d\n", __func__, type);

	switch (type) {
	case EN_CM_LUMA:
		vpp_module_cm_set_cm2_offset_luma(pdata);
		break;
	case EN_CM_SAT:
		vpp_module_cm_set_cm2_offset_sat(pdata);
		break;
	case EN_CM_HUE:
		vpp_module_cm_set_cm2_offset_hue(pdata);
		break;
	case EN_CM_HUE_BY_HS:
		vpp_module_cm_set_cm2_offset_hue_by_hs(pdata);
		break;
	default:
		break;
	}

	return 0;
}
EXPORT_SYMBOL(vpp_pq_mgr_set_cm_offset_curve);

int vpp_pq_mgr_set_matrix_param(struct vpp_mtrx_info_s *pdata)
{
	int ret = 0;
	enum matrix_mode_e mode;
	enum vpp_mtrx_type_e mtrx_sel;

	if (pq_mgr_settings.bypass_top_set)
		return ret;

	if (!pdata)
		return RET_POINT_FAIL;

	pr_vpp(PR_DEBUG, "[%s] mtrx_sel = %d\n",
		__func__, pdata->mtrx_sel);

	if (pdata->mtrx_sel < EN_MTRX_MAX)
		mtrx_sel = pdata->mtrx_sel;
	else
		mtrx_sel = EN_MTRX_VD1;

	switch (mtrx_sel) {
	case EN_MTRX_VD1:
	default:
		mode = EN_MTRX_MODE_VD1;
		break;
	case EN_MTRX_POST:
		mode = EN_MTRX_MODE_POST;
		break;
	case EN_MTRX_POST2:
		mode = EN_MTRX_MODE_POST2;
		break;
	}

	memcpy(&pq_mgr_settings.matrix_param[mtrx_sel],
		&pdata->mtrx_param, sizeof(struct vpp_mtrx_param_s));

	ret = vpp_module_matrix_rs(mode, pdata->mtrx_param.right_shift);
	ret |= vpp_module_matrix_set_pre_offset(mode, &pdata->mtrx_param.pre_offset[0]);
	ret |= vpp_module_matrix_set_offset(mode, &pdata->mtrx_param.post_offset[0]);
	ret |= vpp_module_matrix_set_coef_3x3(mode, &pdata->mtrx_param.matrix_coef[0]);

	return ret;
}
EXPORT_SYMBOL(vpp_pq_mgr_set_matrix_param);

int vpp_pq_mgr_set_dnlp_param(struct vpp_dnlp_curve_param_s *pdata)
{
	if (pq_mgr_settings.bypass_top_set)
		return 0;

	if (!pdata)
		return RET_POINT_FAIL;

	pr_vpp(PR_DEBUG, "[%s] set data\n", __func__);

	memcpy(&pq_mgr_settings.dnlp_param, pdata,
		sizeof(struct vpp_dnlp_curve_param_s));

	vpp_module_dnlp_set_param(pdata);

	return 0;
}
EXPORT_SYMBOL(vpp_pq_mgr_set_dnlp_param);

int vpp_pq_mgr_set_lc_curve(struct vpp_lc_curve_s *pdata)
{
	if (pq_mgr_settings.bypass_top_set)
		return 0;

	if (!pdata)
		return RET_POINT_FAIL;

	if (vpp_module_lc_get_support()) {
		pr_vpp(PR_DEBUG, "[%s] set data\n", __func__);

		vpp_module_lc_write_lut(EN_LC_SAT, &pdata->lc_saturation[0]);
		vpp_module_lc_write_lut(EN_LC_YMIN_LMT, &pdata->lc_yminval_lmt[0]);
		vpp_module_lc_write_lut(EN_LC_YPKBV_YMAX_LMT, &pdata->lc_ypkbv_ymaxval_lmt[0]);
		vpp_module_lc_write_lut(EN_LC_YMAX_LMT, &pdata->lc_ymaxval_lmt[0]);
		vpp_module_lc_write_lut(EN_LC_YPKBV_LMT, &pdata->lc_ypkbv_lmt[0]);
		vpp_module_lc_write_lut(EN_LC_YPKBV_RAT, &pdata->lc_ypkbv_ratio[0]);
	} else {
		pr_vpp(PR_DEBUG, "[%s] not support\n", __func__);
	}

	return 0;
}
EXPORT_SYMBOL(vpp_pq_mgr_set_lc_curve);

int vpp_pq_mgr_set_lc_param(struct vpp_lc_param_s *pdata)
{
	if (pq_mgr_settings.bypass_top_set)
		return 0;

	if (!pdata)
		return RET_POINT_FAIL;

	if (vpp_module_lc_get_support()) {
		pr_vpp(PR_DEBUG, "[%s] set data\n", __func__);

		vpp_data_updata_reg_lc(pdata);
	} else {
		pr_vpp(PR_DEBUG, "[%s] not support\n", __func__);
	}

	return 0;
}
EXPORT_SYMBOL(vpp_pq_mgr_set_lc_param);

int vpp_pq_mgr_set_module_status(enum vpp_module_e module, bool enable)
{
	if (pq_mgr_settings.bypass_top_set)
		return 0;

	pr_vpp(PR_DEBUG, "[%s] module = %d, enable = %d\n",
		__func__, module, enable);

	switch (module) {
	case EN_MODULE_VADJ1:
		vpp_module_vadj_en(enable);
		pq_mgr_settings.pq_status.pq_cfg.vadj1_en = enable;
		break;
	case EN_MODULE_VADJ2:
		vpp_module_vadj_post_en(enable);
		pq_mgr_settings.pq_status.pq_cfg.vadj2_en = enable;
		break;
	case EN_MODULE_PREGAMMA:
		vpp_module_pre_gamma_en(enable);
		pq_mgr_settings.pq_status.pq_cfg.pregamma_en = enable;
		break;
	case EN_MODULE_GAMMA:
		vpp_module_lcd_gamma_en(enable);
		pq_mgr_settings.pq_status.pq_cfg.gamma_en = enable;
		break;
	case EN_MODULE_WB:
		vpp_module_go_en(enable);
		pq_mgr_settings.pq_status.pq_cfg.wb_en = enable;
		break;
	case EN_MODULE_DNLP:
		vpp_module_dnlp_en(enable);
		pq_mgr_settings.pq_status.pq_cfg.dnlp_en = enable;
		break;
	case EN_MODULE_CCORING:
		vpp_module_ve_ccoring_en(enable);
		pq_mgr_settings.pq_status.pq_cfg.chroma_cor_en = enable;
		break;
	case EN_MODULE_SR0:
		vpp_module_sr_en(EN_MODE_SR_0, enable);
		pq_mgr_settings.pq_status.pq_cfg.sharpness0_en = enable;
		break;
	case EN_MODULE_SR1:
		vpp_module_sr_en(EN_MODE_SR_1, enable);
		pq_mgr_settings.pq_status.pq_cfg.sharpness1_en = enable;
		break;
	case EN_MODULE_LC:
		vpp_module_lc_en(enable);
		pq_mgr_settings.pq_status.pq_cfg.lc_en = enable;
		break;
	case EN_MODULE_CM:
		vpp_module_cm_en(enable);
		pq_mgr_settings.pq_status.pq_cfg.cm_en = enable;
		break;
	case EN_MODULE_BLE:
		vpp_module_ve_blkext_en(enable);
		pq_mgr_settings.pq_status.pq_cfg.black_ext_en = enable;
		break;
	case EN_MODULE_BLS:
		vpp_module_ve_blue_stretch_en(enable);
		pq_mgr_settings.pq_status.pq_cfg.blue_stretch_en = enable;
		break;
	case EN_MODULE_LUT3D:
		vpp_module_lut3d_en(enable);
		pq_mgr_settings.pq_status.pq_cfg.lut3d_en = enable;
		break;
	case EN_MODULE_DEJAGGY_SR0:
		vpp_module_sr_sub_module_en(EN_MODE_SR_0,
			EN_SUB_MD_DEJAGGY, enable);
		pq_mgr_settings.pq_status.pq_cfg.dejaggy_sr0_en = enable;
		break;
	case EN_MODULE_DEJAGGY_SR1:
		vpp_module_sr_sub_module_en(EN_MODE_SR_1,
			EN_SUB_MD_DEJAGGY, enable);
		pq_mgr_settings.pq_status.pq_cfg.dejaggy_sr1_en = enable;
		break;
	case EN_MODULE_DERING_SR0:
		vpp_module_sr_sub_module_en(EN_MODE_SR_0,
			EN_SUB_MD_DERING, enable);
		pq_mgr_settings.pq_status.pq_cfg.dering_sr0_en = enable;
		break;
	case EN_MODULE_DERING_SR1:
		vpp_module_sr_sub_module_en(EN_MODE_SR_1,
			EN_SUB_MD_DERING, enable);
		pq_mgr_settings.pq_status.pq_cfg.dering_sr1_en = enable;
		break;
	case EN_MODULE_ALL:
		_set_pq_bypass_all(!enable);
		break;
	default:
		break;
	}

	return 0;
}
EXPORT_SYMBOL(vpp_pq_mgr_set_module_status);

int vpp_pq_mgr_set_pc_mode(int val)
{
	if (pq_mgr_settings.bypass_top_set)
		return 0;

	pr_vpp(PR_DEBUG, "[%s] pc_mode = %d\n", __func__, val);

	pq_mgr_settings.pc_mode = val;

	vpp_vf_set_pc_mode(val);

	return 0;
}
EXPORT_SYMBOL(vpp_pq_mgr_set_pc_mode);

int vpp_pq_mgr_set_color_primary_status(int val)
{
	return 0;
}
EXPORT_SYMBOL(vpp_pq_mgr_set_color_primary_status);

int vpp_pq_mgr_set_color_primary(struct vpp_color_primary_s *pdata)
{
	if (!pdata)
		return 0;

	return 0;
}
EXPORT_SYMBOL(vpp_pq_mgr_set_color_primary);

int vpp_pq_mgr_set_csc_type(int val)
{
	return 0;
}
EXPORT_SYMBOL(vpp_pq_mgr_set_csc_type);

int vpp_pq_mgr_load_3dlut_data(struct vpp_lut3d_path_s *pdata)
{
	int key_len, tmp;
	struct file *fp;
	mm_segment_t fs;
	loff_t pos;

	if (!pdata || !pdata->ppath ||
		pdata->data_type == EN_LUT3D_INPUT_PARAM) {
		lut3d_db_initial = false;
		return 0;
	}

	pr_vpp(PR_DEBUG, "[%s] set data\n", __func__);

	key_len = 17 * 17 * 17 * 3 * pdata->data_count;
	pkey_lut_all = kmalloc(key_len, GFP_KERNEL);
	if (!pkey_lut_all) {
		lut3d_db_initial = false;
		return 1;
	}

	if (pdata->data_type == EN_LUT3D_UNIFY_KEY) {
#ifdef CONFIG_AMLOGIC_LCD
		tmp = lcd_unifykey_get_size(pdata->ppath, &key_len);
		if (tmp < 0) {
			kfree(pkey_lut_all);
			lut3d_db_initial = false;
			return 1;
		}
		tmp = lcd_unifykey_get_no_header(pdata->ppath,
			(unsigned char *)pkey_lut_all, key_len);

		if (tmp < 0) {
			kfree(pkey_lut_all);
			lut3d_db_initial = false;
			return 1;
		}
#endif
	} else if (pdata->data_type == EN_LUT3D_BIN_FILE) {
		fp = filp_open(pdata->ppath, O_RDONLY, 0);
		if (IS_ERR(fp)) {
			kfree(pkey_lut_all);
			lut3d_db_initial = false;
			return 1;
		}

		fs = get_fs();
		set_fs(KERNEL_DS);
		pos = 0;
		tmp = vfs_read(fp, pkey_lut_all, key_len, &pos);
		key_len = tmp;

		if (key_len == 0) {
			kfree(pkey_lut_all);
			lut3d_db_initial = false;
			return 1;
		}

		filp_close(fp, NULL);
		set_fs(fs);
	}

	return 0;
}
EXPORT_SYMBOL(vpp_pq_mgr_load_3dlut_data);

int vpp_pq_mgr_set_3dlut_data(struct vpp_lut3d_table_s *ptable)
{
	if (pq_mgr_settings.bypass_top_set)
		return 0;

	if (!ptable)
		return RET_POINT_FAIL;

	pr_vpp(PR_DEBUG, "[%s] data_type = %d\n", __func__,
		ptable->data_type);

	switch (ptable->data_type) {
	case EN_LUT3D_INPUT_PARAM:
		vpp_module_lut3d_set_data(ptable->pdata, 0);
		break;
	case EN_LUT3D_UNIFY_KEY:
		break;
	case EN_LUT3D_BIN_FILE:
		break;
	default:
		break;
	}

	return 0;
}
EXPORT_SYMBOL(vpp_pq_mgr_set_3dlut_data);

int vpp_pq_mgr_set_hdr_cgain_curve(struct vpp_hdr_lut_s *pdata)
{
	enum hdr_module_type_e type = EN_MODULE_TYPE_VDIN0;
	enum hdr_sub_module_e sub_module = EN_SUB_MODULE_CGAIN;
	enum hdr_vpp_type_e vpp_sel = EN_TYPE_VPP0;

	if (pq_mgr_settings.bypass_top_set)
		return 0;

	if (!pdata)
		return RET_POINT_FAIL;

	pr_vpp(PR_DEBUG, "[%s] set data\n", __func__);

	vpp_module_hdr_set_lut(type, sub_module,
		(int *)pdata->lut_data, vpp_sel);

	return 0;
}
EXPORT_SYMBOL(vpp_pq_mgr_set_hdr_cgain_curve);

int vpp_pq_mgr_set_hdr_oetf_curve(struct vpp_hdr_lut_s *pdata)
{
	enum hdr_module_type_e type = EN_MODULE_TYPE_VDIN0;
	enum hdr_sub_module_e sub_module = EN_SUB_MODULE_OETF;
	enum hdr_vpp_type_e vpp_sel = EN_TYPE_VPP0;

	if (pq_mgr_settings.bypass_top_set)
		return 0;

	if (!pdata)
		return RET_POINT_FAIL;

	pr_vpp(PR_DEBUG, "[%s] set data\n", __func__);

	vpp_module_hdr_set_lut(type, sub_module,
		(int *)pdata->lut_data, vpp_sel);

	return 0;
}
EXPORT_SYMBOL(vpp_pq_mgr_set_hdr_oetf_curve);

int vpp_pq_mgr_set_hdr_eotf_curve(struct vpp_hdr_lut_s *pdata)
{
	enum hdr_module_type_e type = EN_MODULE_TYPE_VDIN0;
	enum hdr_sub_module_e sub_module = EN_SUB_MODULE_EOTF;
	enum hdr_vpp_type_e vpp_sel = EN_TYPE_VPP0;

	if (pq_mgr_settings.bypass_top_set)
		return 0;

	if (!pdata)
		return RET_POINT_FAIL;

	pr_vpp(PR_DEBUG, "[%s] set data\n", __func__);

	vpp_module_hdr_set_lut(type, sub_module,
		(int *)pdata->lut_data, vpp_sel);

	return 0;
}
EXPORT_SYMBOL(vpp_pq_mgr_set_hdr_eotf_curve);

int vpp_pq_mgr_set_hdr_tmo_curve(struct vpp_hdr_lut_s *pdata)
{
	enum hdr_module_type_e type = EN_MODULE_TYPE_VDIN0;
	enum hdr_sub_module_e sub_module = EN_SUB_MODULE_OGAIN;
	enum hdr_vpp_type_e vpp_sel = EN_TYPE_VPP0;

	if (pq_mgr_settings.bypass_top_set)
		return 0;

	if (!pdata)
		return RET_POINT_FAIL;

	pr_vpp(PR_DEBUG, "[%s] set data\n", __func__);

	vpp_module_hdr_set_lut(type, sub_module,
		(int *)pdata->lut_data, vpp_sel);

	return 0;
}
EXPORT_SYMBOL(vpp_pq_mgr_set_hdr_tmo_curve);

int vpp_pq_mgr_set_hdr_tmo_param(struct vpp_tmo_param_s *pdata)
{
	return 0;
}
EXPORT_SYMBOL(vpp_pq_mgr_set_hdr_tmo_param);

int vpp_pq_mgr_set_cabc_param(struct vpp_cabc_param_s *pdata)
{
	return 0;
}
EXPORT_SYMBOL(vpp_pq_mgr_set_cabc_param);

int vpp_pq_mgr_set_aad_param(struct vpp_aad_param_s *pdata)
{
	return 0;
}
EXPORT_SYMBOL(vpp_pq_mgr_set_aad_param);

int vpp_pq_mgr_set_eye_protect(struct vpp_eye_protect_s *pdata)
{
	int ret = 0;
	int coef[9] = {0};
	int pre_offset[3] = {-64, -512, -512};
	int offset[3] = {64, 512, 512};

	if (pq_mgr_settings.bypass_top_set)
		return ret;

	if (!pdata)/* || vpp_vf_get_vinfo_lcd_support())*/
		return ret;

	pr_vpp(PR_DEBUG, "[%s] enable = %d\n",
		__func__, pdata->enable);
	pr_vpp(PR_DEBUG, "[%s] r_gain = %d\n",
		__func__, pdata->rgb[0]);
	pr_vpp(PR_DEBUG, "[%s] g_gain = %d\n",
		__func__, pdata->rgb[1]);
	pr_vpp(PR_DEBUG, "[%s] b_gain = %d\n",
		__func__, pdata->rgb[2]);

	if (pdata->enable) {
		_mtrx_multi(&pdata->rgb[0], &coef[0]);
		ret = vpp_module_matrix_set_pre_offset(EN_MTRX_MODE_POST, &pre_offset[0]);
		ret |= vpp_module_matrix_set_offset(EN_MTRX_MODE_POST, &offset[0]);
		ret |= vpp_module_matrix_set_coef_3x3(EN_MTRX_MODE_POST, &coef[0]);
	}

	ret |= vpp_module_matrix_en(EN_MTRX_MODE_POST, pdata->enable);

	return ret;
}
EXPORT_SYMBOL(vpp_pq_mgr_set_eye_protect);

int vpp_pq_mgr_set_aipq_offset_table(char *pdata_str,
	unsigned int height, unsigned int width)
{
	int i = 0;
	int *ptable = NULL;
	unsigned int table_len = 0;
	unsigned int size = 0;

	if (pq_mgr_settings.bypass_top_set)
		return 0;

	if (!pdata_str)
		return RET_POINT_FAIL;

	table_len = height * width * sizeof(int);
	ptable = kmalloc(table_len, GFP_KERNEL);
	if (!ptable)
		return -EFAULT;

	_str_sapr_to_d(pdata_str, ptable, 4);

	size = width * sizeof(int);
	for (i = 0; i < height; i++)
		memcpy(vpp_pq_data[i], ptable + (i * width), size);

	kfree(ptable);
	return 0;
}
EXPORT_SYMBOL(vpp_pq_mgr_set_aipq_offset_table);

int vpp_pq_mgr_set_aipq_data(struct vpp_aipq_table_s *pdata)
{
	int i = 0;
	unsigned int *ptable = NULL;
	unsigned int size = 0;

	if (pq_mgr_settings.bypass_top_set)
		return 0;

	if (!pdata)
		return RET_POINT_FAIL;

	ptable = (unsigned int *)pdata->table_ptr;
	size = pdata->width * sizeof(int);
	for (i = 0; i < pdata->height; i++)
		memcpy(vpp_pq_data[i], ptable + (i * pdata->width), size);

	return 0;
}
EXPORT_SYMBOL(vpp_pq_mgr_set_aipq_data);

int vpp_pq_mgr_set_sr_param(struct vpp_sr0_param_s *sr0_data,
	struct vpp_sr1_param_s *sr1_data)
{
	if (pq_mgr_settings.bypass_top_set)
		return 0;

	if (!sr0_data || !sr1_data)
		return RET_POINT_FAIL;

	return 0;
}
EXPORT_SYMBOL(vpp_pq_mgr_set_sr_param);

int vpp_pq_mgr_set_aisr_param(struct vpp_aisr_param_s *sr_data,
	struct vpp_aisr_nn_param_s *nn_data)
{
	if (pq_mgr_settings.bypass_top_set)
		return 0;

	if (!sr_data || !nn_data)
		return RET_POINT_FAIL;

	vpp_module_sr_set_aisr_param(sr_data, nn_data);

	return 0;
}
EXPORT_SYMBOL(vpp_pq_mgr_set_aisr_param);

int vpp_pq_mgr_set_overscan_table(unsigned int length,
	struct vpp_overscan_table_s *load_table)
{
	if (pq_mgr_settings.bypass_top_set)
		return 0;

	if (load_table)
		vpp_vf_set_overscan_table(length, load_table);

	return 0;
}
EXPORT_SYMBOL(vpp_pq_mgr_set_overscan_table);

void vpp_pq_mgr_set_lc_isr(void)
{
	vpp_module_lc_set_isr();
}
EXPORT_SYMBOL(vpp_pq_mgr_set_lc_isr);

void vpp_pq_mgr_get_status(struct vpp_pq_state_s *pstatus)
{
	if (pstatus)
		memcpy(pstatus, &pq_mgr_settings.pq_status,
			sizeof(struct vpp_pq_state_s));
}
EXPORT_SYMBOL(vpp_pq_mgr_get_status);

void vpp_pq_mgr_get_brightness(int *pval)
{
	if (pval)
		*pval = pq_mgr_settings.brightness;
}
EXPORT_SYMBOL(vpp_pq_mgr_get_brightness);

void vpp_pq_mgr_get_brightness_post(int *pval)
{
	if (pval)
		*pval = pq_mgr_settings.brightness_post;
}
EXPORT_SYMBOL(vpp_pq_mgr_get_brightness_post);

void vpp_pq_mgr_get_contrast(int *pval)
{
	if (pval)
		*pval = pq_mgr_settings.contrast;
}
EXPORT_SYMBOL(vpp_pq_mgr_get_contrast);

void vpp_pq_mgr_get_contrast_post(int *pval)
{
	if (pval)
		*pval = pq_mgr_settings.contrast_post;
}
EXPORT_SYMBOL(vpp_pq_mgr_get_contrast_post);

void vpp_pq_mgr_get_saturation(int *pval)
{
	if (pval)
		*pval = pq_mgr_settings.saturation;
}
EXPORT_SYMBOL(vpp_pq_mgr_get_saturation);

void vpp_pq_mgr_get_saturation_post(int *pval)
{
	if (pval)
		*pval = pq_mgr_settings.saturation_post;
}
EXPORT_SYMBOL(vpp_pq_mgr_get_saturation_post);

void vpp_pq_mgr_get_hue(int *pval)
{
	if (pval)
		*pval = pq_mgr_settings.hue;
}
EXPORT_SYMBOL(vpp_pq_mgr_get_hue);

void vpp_pq_mgr_get_hue_post(int *pval)
{
	if (pval)
		*pval = pq_mgr_settings.hue_post;
}
EXPORT_SYMBOL(vpp_pq_mgr_get_hue_post);

void vpp_pq_mgr_get_sharpness(int *pval)
{
	if (pval)
		*pval = pq_mgr_settings.sharpness;
}
EXPORT_SYMBOL(vpp_pq_mgr_get_sharpness);

void vpp_pq_mgr_get_whitebalance(struct vpp_white_balance_s *pdata)
{
	if (pdata)
		memcpy(pdata, &pq_mgr_settings.video_rgb_ogo,
			sizeof(struct vpp_white_balance_s));
}
EXPORT_SYMBOL(vpp_pq_mgr_get_whitebalance);

struct vpp_gamma_table_s *vpp_pq_mgr_get_pre_gamma_table(void)
{
	return &pq_mgr_settings.cur_pre_gamma_tbl;
}
EXPORT_SYMBOL(vpp_pq_mgr_get_pre_gamma_table);

struct vpp_gamma_table_s *vpp_pq_mgr_get_gamma_table(void)
{
	return &pq_mgr_settings.cur_gamma_tbl;
}
EXPORT_SYMBOL(vpp_pq_mgr_get_gamma_table);

void vpp_pq_mgr_get_matrix_param(struct vpp_mtrx_info_s *pdata)
{
	enum vpp_mtrx_type_e mtrx_sel;

	if (pdata->mtrx_sel < EN_MTRX_MAX)
		mtrx_sel = pdata->mtrx_sel;
	else
		mtrx_sel = EN_MTRX_VD1;

	memcpy(&pdata->mtrx_param, &pq_mgr_settings.matrix_param[mtrx_sel],
		sizeof(struct vpp_mtrx_param_s));
}
EXPORT_SYMBOL(vpp_pq_mgr_get_matrix_param);

void vpp_pq_mgr_get_dnlp_param(struct vpp_dnlp_curve_param_s *pdata)
{
	if (pdata)
		memcpy(pdata, &pq_mgr_settings.dnlp_param,
			sizeof(struct vpp_dnlp_curve_param_s));
}
EXPORT_SYMBOL(vpp_pq_mgr_get_dnlp_param);

void vpp_pq_mgr_get_module_status(enum vpp_module_e module,
	unsigned char *penable)
{
	unsigned char enable;

	if (!penable)
		return;

	switch (module) {
	case EN_MODULE_VADJ1:
		enable = pq_mgr_settings.pq_status.pq_cfg.vadj1_en;
		break;
	case EN_MODULE_VADJ2:
		enable = pq_mgr_settings.pq_status.pq_cfg.vadj2_en;
		break;
	case EN_MODULE_PREGAMMA:
		enable = pq_mgr_settings.pq_status.pq_cfg.pregamma_en;
		break;
	case EN_MODULE_GAMMA:
		enable = pq_mgr_settings.pq_status.pq_cfg.gamma_en;
		break;
	case EN_MODULE_WB:
		enable = pq_mgr_settings.pq_status.pq_cfg.wb_en;
		break;
	case EN_MODULE_DNLP:
		enable = pq_mgr_settings.pq_status.pq_cfg.dnlp_en;
		break;
	case EN_MODULE_CCORING:
		enable = pq_mgr_settings.pq_status.pq_cfg.chroma_cor_en;
		break;
	case EN_MODULE_SR0:
		enable = pq_mgr_settings.pq_status.pq_cfg.sharpness0_en;
		break;
	case EN_MODULE_SR1:
		enable = pq_mgr_settings.pq_status.pq_cfg.sharpness1_en;
		break;
	case EN_MODULE_LC:
		enable = pq_mgr_settings.pq_status.pq_cfg.lc_en;
		break;
	case EN_MODULE_CM:
		enable = pq_mgr_settings.pq_status.pq_cfg.cm_en;
		break;
	case EN_MODULE_BLE:
		enable = pq_mgr_settings.pq_status.pq_cfg.black_ext_en;
		break;
	case EN_MODULE_BLS:
		enable = pq_mgr_settings.pq_status.pq_cfg.blue_stretch_en;
		break;
	case EN_MODULE_LUT3D:
		enable = pq_mgr_settings.pq_status.pq_cfg.lut3d_en;
		break;
	case EN_MODULE_DEJAGGY_SR0:
		enable = pq_mgr_settings.pq_status.pq_cfg.dejaggy_sr0_en;
		break;
	case EN_MODULE_DEJAGGY_SR1:
		enable = pq_mgr_settings.pq_status.pq_cfg.dejaggy_sr1_en;
		break;
	case EN_MODULE_DERING_SR0:
		enable = pq_mgr_settings.pq_status.pq_cfg.dering_sr0_en;
		break;
	case EN_MODULE_DERING_SR1:
		enable = pq_mgr_settings.pq_status.pq_cfg.dering_sr1_en;
		break;
	case EN_MODULE_ALL:
	default:
		enable = pq_mgr_settings.pq_status.pq_en;
		break;
	}

	*penable = enable;
}
EXPORT_SYMBOL(vpp_pq_mgr_get_module_status);

void vpp_pq_mgr_get_hdr_tmo_param(struct vpp_tmo_param_s *pdata)
{
}
EXPORT_SYMBOL(vpp_pq_mgr_get_hdr_tmo_param);

void vpp_pq_mgr_get_hdr_metadata(struct vpp_hdr_metadata_s *pdata)
{
	struct vpp_hdr_metadata_s *pmetadata;

	if (!pdata)
		return;

	pmetadata = vpp_vf_get_hdr_metadata();
	memcpy(pdata, pmetadata, sizeof(struct vpp_hdr_metadata_s));

	pr_vpp(PR_DEBUG, "[%s] white_point_0 = %d, %d\n", __func__,
		pmetadata->primaries[0][0], pmetadata->primaries[0][1]);
	pr_vpp(PR_DEBUG, "[%s] white_point_1 = %d, %d\n", __func__,
		pmetadata->primaries[1][0], pmetadata->primaries[1][1]);
	pr_vpp(PR_DEBUG, "[%s] white_point_2 = %d, %d\n", __func__,
		pmetadata->primaries[2][0], pmetadata->primaries[2][1]);
	pr_vpp(PR_DEBUG, "[%s] white_point = %d, %d\n", __func__,
		pmetadata->white_point[0], pmetadata->white_point[1]);
	pr_vpp(PR_DEBUG, "[%s] luminance = %d, %d\n", __func__,
		pmetadata->luminance[0], pmetadata->luminance[1]);
}
EXPORT_SYMBOL(vpp_pq_mgr_get_hdr_metadata);

void vpp_pq_mgr_get_hdr_histogram(struct vpp_hdr_histgm_param_s *pdata)
{
	struct hdr_hist_report_s *preport = NULL;
	int i = 0;

	if (!pdata)
		return;

	preport = vpp_module_hdr_get_hist_report();

	for (i = 0; i < VPP_HDR_HIST_BIN_COUNT; i++)
		pdata->data_rgb_max[i] =
			preport->hist_buff[HDR_HIST_BACKUP_COUNT - 1][i];
}
EXPORT_SYMBOL(vpp_pq_mgr_get_hdr_histogram);

void vpp_pq_mgr_get_histogram_ave(struct vpp_histgm_ave_s *pdata)
{
	int ave_luma;
	struct vpp_hist_report_s *phist_report;

	if (!pdata)
		return;

	phist_report = vpp_module_meter_get_hist_report();

	pdata->sum = phist_report->luma_sum;
	pdata->width = phist_report->width;
	pdata->height = phist_report->height;
	ave_luma = phist_report->luma_sum /
		(phist_report->height * phist_report->width);

	/*This type needs to modify*/
	if (chip_type > CHIP_G12A) {
		ave_luma = (ave_luma - 16) < 0 ? 0 : (ave_luma - 16);
		ave_luma = ave_luma * 255 / (235 - 16);
		if (ave_luma > 255)
			ave_luma = 255;
	}

	pdata->ave = ave_luma;
}
EXPORT_SYMBOL(vpp_pq_mgr_get_histogram_ave);

void vpp_pq_mgr_get_histogram(struct vpp_histgm_param_s *pdata)
{
	int i = 0;
	struct vpp_hist_report_s *phist_report;

	if (!pdata)
		return;

	phist_report = vpp_module_meter_get_hist_report();

	pdata->hist_pow = phist_report->hist_pow;
	pdata->luma_sum = phist_report->luma_sum;
	pdata->pixel_sum = phist_report->pixel_sum;

	for (i = 0; i < VPP_HIST_BIN_COUNT; i++)
		pdata->histgm[i] = phist_report->gamma[i];

	for (i = 0; i < VPP_COLOR_HIST_BIN_COUNT; i++) {
		pdata->hue_histgm[i] = phist_report->hue_gamma[i];
		pdata->sat_histgm[i] = phist_report->sat_gamma[i];
	}
}
EXPORT_SYMBOL(vpp_pq_mgr_get_histogram);

void vpp_pq_mgr_set_ccoring_params(unsigned int *pdata, int length)
{
	int i;

	if (length != EN_CCORING_MAX || !pdata) {
		pr_vpp(PR_DEBUG, "[%s] length = %d\n", __func__, length);
		return;
	}

	for (i = 0; i < length; i++)
		pr_vpp(PR_DEBUG, "[%s] pdata[%d] = %d\n", __func__, i, pdata[i]);

	vpp_module_ve_set_ccoring_params(pdata);
}
EXPORT_SYMBOL(vpp_pq_mgr_set_ccoring_params);

int vpp_pq_mgr_get_pc_mode(void)
{
	return pq_mgr_settings.pc_mode;
}
EXPORT_SYMBOL(vpp_pq_mgr_get_pc_mode);

enum vpp_csc_type_e vpp_pq_mgr_get_csc_type(void)
{
	return vpp_vf_get_csc_type(EN_VD1_PATH);
}
EXPORT_SYMBOL(vpp_pq_mgr_get_csc_type);

enum vpp_hdr_type_e vpp_pq_mgr_get_hdr_type(void)
{
	return vpp_vf_get_hdr_type();
}
EXPORT_SYMBOL(vpp_pq_mgr_get_hdr_type);

enum vpp_color_primary_e vpp_pq_mgr_get_color_primary(void)
{
	return vpp_vf_get_color_primary();
}
EXPORT_SYMBOL(vpp_pq_mgr_get_color_primary);

int vpp_pq_mgr_get_pre_gamma_table_len(void)
{
	return vpp_module_pre_gamma_get_table_len();
}
EXPORT_SYMBOL(vpp_pq_mgr_get_pre_gamma_table_len);

int vpp_pq_mgr_get_gamma_table_len(void)
{
	return vpp_module_lcd_gamma_get_table_len();
}
EXPORT_SYMBOL(vpp_pq_mgr_get_gamma_table_len);

struct vpp_pq_mgr_settings *vpp_pq_mgr_get_settings(void)
{
	return &pq_mgr_settings;
}
EXPORT_SYMBOL(vpp_pq_mgr_get_settings);

