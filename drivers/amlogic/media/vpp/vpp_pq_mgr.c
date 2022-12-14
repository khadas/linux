// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include "vpp_common.h"
#include "vpp_pq_mgr.h"
#include "vpp_modules_inc.h"
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

	return 0;
}

static int _set_default_settings(void)
{
	pq_mgr_settings.brightness = 0;
	pq_mgr_settings.contrast = 0;
	pq_mgr_settings.hue = 0;
	pq_mgr_settings.saturation = 0;
	pq_mgr_settings.brightness_post = 0;
	pq_mgr_settings.contrast_post = 0;
	pq_mgr_settings.hue_post = 0;
	pq_mgr_settings.saturation_post = 0;

	pq_mgr_settings.video_rgb_ogo.r_pre_offset = 0;
	pq_mgr_settings.video_rgb_ogo.g_pre_offset = 0;
	pq_mgr_settings.video_rgb_ogo.b_pre_offset = 0;
	pq_mgr_settings.video_rgb_ogo.r_gain = 1024;
	pq_mgr_settings.video_rgb_ogo.g_gain = 1024;
	pq_mgr_settings.video_rgb_ogo.b_gain = 1024;
	pq_mgr_settings.video_rgb_ogo.r_post_offset = 0;
	pq_mgr_settings.video_rgb_ogo.g_post_offset = 0;
	pq_mgr_settings.video_rgb_ogo.b_post_offset = 0;

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

/*External functions*/
int vpp_pq_mgr_init(struct vpp_dev_s *pdev)
{
	chip_type = pdev->pm_data->chip_id;

	memset(&pq_mgr_settings, 0, sizeof(struct vpp_pq_mgr_settings));

	_modules_init(pdev);
	_set_default_settings();

	lut3d_db_initial = false;

	return 0;
}
EXPORT_SYMBOL(vpp_pq_mgr_init);

int vpp_pq_mgr_set_status(struct vpp_pq_state_s *pstatus)
{
	return 0;
}
EXPORT_SYMBOL(vpp_pq_mgr_set_status);

int vpp_pq_mgr_set_brightness(int val)
{
	int ret = 0;

	pq_mgr_settings.brightness = val;
	val = vpp_check_range(val, (-1024), 1023);

	if (chip_type < CHIP_G12A)
		val = val >> 1;

	pr_vpp(PR_DEBUG, "[%s] brightness = %d\n", __func__, val);

	ret = vpp_module_vadj_set_brightness(val);

	return ret;
}
EXPORT_SYMBOL(vpp_pq_mgr_set_brightness);

int vpp_pq_mgr_set_brightness_post(int val)
{
	int ret = 0;

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

int vpp_pq_mgr_set_pre_gamma_table(struct vpp_pre_gamma_table_s *pdata)
{
	int ret = 0;

	if (!pdata)
		return RET_POINT_FAIL;

	pr_vpp(PR_DEBUG, "[%s] set data\n", __func__);

	memcpy(&pq_mgr_settings.cur_pre_gamma_tbl, pdata,
		sizeof(struct vpp_pre_gamma_table_s));

	ret = vpp_module_pre_gamma_write(&pdata->r_data[0],
		&pdata->g_data[0], &pdata->b_data[0]);

	return ret;
}
EXPORT_SYMBOL(vpp_pq_mgr_set_pre_gamma_table);

int vpp_pq_mgr_set_gamma_table(struct vpp_gamma_table_s *pdata)
{
	int ret = 0;

	if (!pdata)
		return RET_POINT_FAIL;

	pr_vpp(PR_DEBUG, "[%s] set data\n", __func__);

	memcpy(&pq_mgr_settings.cur_gamma_tbl, pdata,
		sizeof(struct vpp_gamma_table_s));

	ret = vpp_module_lcd_gamma_write(&pdata->r_data[0],
		&pdata->g_data[0], &pdata->b_data[0]);

	return ret;
}
EXPORT_SYMBOL(vpp_pq_mgr_set_gamma_table);

int vpp_pq_mgr_set_matrix_param(struct vpp_mtrx_info_s *pdata)
{
	int ret = 0;
	enum matrix_mode_e mode;
	enum vpp_mtrx_type_e mtrx_sel;

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

	ret = vpp_module_matrix_rs(mode,
		pdata->mtrx_param.right_shift);
	ret = vpp_module_matrix_set_pre_offset(mode, 0,
		pdata->mtrx_param.pre_offset[0]);
	ret = vpp_module_matrix_set_pre_offset(mode, 1,
		pdata->mtrx_param.pre_offset[1]);
	ret = vpp_module_matrix_set_pre_offset(mode, 2,
		pdata->mtrx_param.pre_offset[2]);
	ret = vpp_module_matrix_set_offset(mode, 0,
		pdata->mtrx_param.post_offset[0]);
	ret = vpp_module_matrix_set_offset(mode, 1,
		pdata->mtrx_param.post_offset[1]);
	ret = vpp_module_matrix_set_offset(mode, 2,
		pdata->mtrx_param.post_offset[2]);
	ret = vpp_module_matrix_set_coef_3x3(mode,
		&pdata->mtrx_param.matrix_coef[0]);

	return ret;
}
EXPORT_SYMBOL(vpp_pq_mgr_set_matrix_param);

int vpp_pq_mgr_set_dnlp_param(struct vpp_dnlp_curve_param_s *pdata)
{
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
	if (!pdata)
		return RET_POINT_FAIL;

	if (vpp_module_lc_get_support()) {
		vpp_module_lc_write_lut(EN_LC_SAT, &pdata->lc_saturation[0]);
		vpp_module_lc_write_lut(EN_LC_YMIN_LMT, &pdata->lc_yminval_lmt[0]);
		vpp_module_lc_write_lut(EN_LC_YPKBV_YMAX_LMT, &pdata->lc_ypkbv_ymaxval_lmt[0]);
		vpp_module_lc_write_lut(EN_LC_YMAX_LMT, &pdata->lc_ymaxval_lmt[0]);
		vpp_module_lc_write_lut(EN_LC_YPKBV_LMT, &pdata->lc_ypkbv_lmt[0]);
		vpp_module_lc_write_lut(EN_LC_YPKBV_RAT, &pdata->lc_ypkbv_ratio[0]);
	}

	return 0;
}
EXPORT_SYMBOL(vpp_pq_mgr_set_lc_curve);

int vpp_pq_mgr_set_lc_param(struct vpp_lc_param_s *pdata)
{
	if (!pdata)
		return RET_POINT_FAIL;

	if (vpp_module_lc_get_support())
		vpp_data_updata_reg_lc(pdata);

	return 0;
}
EXPORT_SYMBOL(vpp_pq_mgr_set_lc_param);

int vpp_pq_mgr_set_module_status(enum vpp_module_e module, bool enable)
{
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
	case EN_MODULE_LC:
		vpp_module_lc_en(enable);
		pq_mgr_settings.pq_status.pq_cfg.lc_en = enable;
		break;
	case EN_MODULE_LUT3D:
		vpp_module_lut3d_en(enable);
		pq_mgr_settings.pq_status.pq_cfg.lut3d_en = enable;
		break;
	default:
		break;
	}

	return 0;
}
EXPORT_SYMBOL(vpp_pq_mgr_set_module_status);

int vpp_pq_mgr_set_pc_mode(int val)
{
	return 0;
}
EXPORT_SYMBOL(vpp_pq_mgr_set_pc_mode);

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

	key_len = 17 * 17 * 17 * 3 * pdata->data_count;
	pkey_lut_all = kmalloc(key_len, GFP_KERNEL);
	if (!pkey_lut_all) {
		lut3d_db_initial = false;
		return 1;
	}

	if (pdata->data_type == EN_LUT3D_UNIFY_KEY) {
#ifdef CONFIG_AMLOGIC_LCD
		tmp = lcd_unifykey_get_no_header(pdata->ppath,
			(unsigned char *)pkey_lut_all, &key_len);

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
	if (!ptable)
		return RET_POINT_FAIL;

	switch (ptable->data_type) {
	case EN_LUT3D_INPUT_PARAM:
		vpp_module_lut3d_set_data(ptable->pdata);
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

	if (!pdata)
		return RET_POINT_FAIL;

	vpp_module_hdr_set_lut(type, sub_module, (int *)pdata->tm_lut);

	return 0;
}
EXPORT_SYMBOL(vpp_pq_mgr_set_hdr_cgain_curve);

int vpp_pq_mgr_set_hdr_oetf_curve(struct vpp_hdr_lut_s *pdata)
{
	enum hdr_module_type_e type = EN_MODULE_TYPE_VDIN0;
	enum hdr_sub_module_e sub_module = EN_SUB_MODULE_OETF;

	if (!pdata)
		return RET_POINT_FAIL;

	vpp_module_hdr_set_lut(type, sub_module, (int *)pdata->tm_lut);

	return 0;
}
EXPORT_SYMBOL(vpp_pq_mgr_set_hdr_oetf_curve);

int vpp_pq_mgr_set_hdr_eotf_curve(struct vpp_hdr_lut_s *pdata)
{
	enum hdr_module_type_e type = EN_MODULE_TYPE_VDIN0;
	enum hdr_sub_module_e sub_module = EN_SUB_MODULE_EOTF;

	if (!pdata)
		return RET_POINT_FAIL;

	vpp_module_hdr_set_lut(type, sub_module, (int *)pdata->tm_lut);

	return 0;
}
EXPORT_SYMBOL(vpp_pq_mgr_set_hdr_eotf_curve);

int vpp_pq_mgr_set_hdr_tmo_curve(struct vpp_hdr_lut_s *pdata)
{
	enum hdr_module_type_e type = EN_MODULE_TYPE_VDIN0;
	enum hdr_sub_module_e sub_module = EN_SUB_MODULE_OGAIN;

	if (!pdata)
		return RET_POINT_FAIL;

	vpp_module_hdr_set_lut(type, sub_module, (int *)pdata->tm_lut);

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

void vpp_pq_mgr_get_status(struct vpp_pq_state_s *pstatus)
{
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
}
EXPORT_SYMBOL(vpp_pq_mgr_get_sharpness);

void vpp_pq_mgr_get_whitebalance(struct vpp_white_balance_s *pdata)
{
	if (pdata)
		memcpy(pdata, &pq_mgr_settings.video_rgb_ogo,
			sizeof(struct vpp_white_balance_s));
}
EXPORT_SYMBOL(vpp_pq_mgr_get_whitebalance);

void vpp_pq_mgr_get_pre_gamma_table(struct vpp_pre_gamma_table_s *pdata)
{
	if (pdata)
		memcpy(pdata, &pq_mgr_settings.cur_pre_gamma_tbl,
			sizeof(struct vpp_pre_gamma_table_s));
}
EXPORT_SYMBOL(vpp_pq_mgr_get_pre_gamma_table);

void vpp_pq_mgr_get_gamma_table(struct vpp_gamma_table_s *pdata)
{
	if (pdata)
		memcpy(pdata, &pq_mgr_settings.cur_gamma_tbl,
			sizeof(struct vpp_gamma_table_s));
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

void vpp_pq_mgr_get_module_status(enum vpp_module_e module, bool *penable)
{
}
EXPORT_SYMBOL(vpp_pq_mgr_get_module_status);

void vpp_pq_mgr_get_pc_mode(int *pval)
{
}
EXPORT_SYMBOL(vpp_pq_mgr_get_pc_mode);

void vpp_pq_mgr_get_csc_type(int *pval)
{
}
EXPORT_SYMBOL(vpp_pq_mgr_get_csc_type);

void vpp_pq_mgr_get_hdr_tmo_param(struct vpp_tmo_param_s *pdata)
{
}
EXPORT_SYMBOL(vpp_pq_mgr_get_hdr_tmo_param);

void vpp_pq_mgr_get_hdr_metadata(struct vpp_hdr_metadata_s *pdata)
{
}
EXPORT_SYMBOL(vpp_pq_mgr_get_hdr_metadata);

void vpp_pq_mgr_get_hdr_type(enum vpp_hdr_type_e *pval)
{
}
EXPORT_SYMBOL(vpp_pq_mgr_get_hdr_type);

void vpp_pq_mgr_get_color_primary(enum vpp_color_primary_e *pval)
{
}
EXPORT_SYMBOL(vpp_pq_mgr_get_color_primary);

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

struct vpp_pq_mgr_settings *vpp_pq_mgr_get_settings(void)
{
	return &pq_mgr_settings;
}
EXPORT_SYMBOL(vpp_pq_mgr_get_settings);

