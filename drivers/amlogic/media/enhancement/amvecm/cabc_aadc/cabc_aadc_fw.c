/*
 * drivers/amlogic/media/enhancement/amvecm/cabc_aadc_fw.c
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
#include <linux/workqueue.h>
#include <linux/amlogic/media/amvecm/cabc_addc_alg.h>
#include <linux/amlogic/media/amvecm/amvecm.h>
#ifdef CONFIG_AMLOGIC_LCD
#include <linux/amlogic/media/vout/lcd/lcd_notify.h>
#endif
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
#include <linux/amlogic/media/amdolbyvision/dolby_vision.h>
#endif
#include "cabc_aadc_fw.h"
#include "../amve.h"

static int pr_cabc_aad;
module_param(pr_cabc_aad, int, 0664);
MODULE_PARM_DESC(pr_cabc_aad, "\n pr_cabc_aad\n");

#define pr_cabc_aad_dbg(fmt, args...)\
	do {\
		if (pr_cabc_aad)\
			pr_info("CACB_AAD: " fmt, ##args);\
	} while (0)

static int cabc_aad_en;
static int status_flag;
static int pre_cabc_en;
static int debug_cabc_aad;

static int lut_Y_gain[17] = {
	0, 256, 512, 768, 1024, 1280, 1536, 1792, 2048, 2304,
	2560, 2816, 3072, 3328, 3584, 3840, 4096
};

static int lut_RG_gain[17] = {
	0, 1024, 2048, 3072, 4096, 5120, 6144, 7168, 8192, 9216,
	10240, 11264, 12288, 13312, 14336, 15360, 16384
};

static int lut_BG_gain[17] = {
	0, 1024, 2048, 3072, 4096, 5120, 6144, 7168, 8192, 9216,
	10240, 11264, 12288, 13312, 14336, 15360, 16384
};

static int xyz2rgb_matrix[9] = {
	13273, -6296, -2042, -3970, 7684, 170, 228, -836, 4331
};

static int rgb2xyz_matrix[9] = {
	1689, 1465, 739, 871, 2929, 296, 79, 488, 3892
};

static int xyz2lms_matrix[9] = {
	1639, 2898, -331, -927, 4773, 187, 0, 0, 3761
};

static int lms2xyz_matrix[9] = {
	7618, -4626, 901, 1479, 2617, 0, 0, 0, 4461
};

static int rgb2lms_matrix[9] = {
	1286, 2620, 190, 636, 3104, 355, 73, 448, 3574
};

static int lms2rgb_matrix[9] = {
	22413, -19012, 695, -4609, 9392, -688, 122, -791, 4767
};

static int gain_lut[16][3] = {
	{7567, 3381, 953},
	{5153, 3885, 3075},
	{4319, 3988, 4509},
	{4846, 3989, 2950},
	{4543, 4036, 3371},
	{4096, 4096, 4096},
	{3817, 4119, 4688},
	{4954, 3879, 3719},
	{5513, 3855, 2314},
	{4104, 4095, 4079},
	{5784, 3787, 2182},
	{4096, 4096, 4096},
	{4096, 4096, 4096},
	{4096, 4096, 4096},
	{4096, 4096, 4096},
	{4096, 4096, 4096}
};

static int xy_lut[16][2] = {
	{1833, 1669},
	{1427, 1441},
	{1270, 1295},
	{1416, 1468},
	{1362, 1423},
	{1281, 1348},
	{1225, 1290},
	{1365, 1365},
	{1524, 1537},
	{1281, 1348},
	{1559, 1544},
	{1281, 1348},
	{1281, 1348},
	{1281, 1348},
	{1281, 1348},
	{1281, 1348}
};

static int hist[64] = {0};
int aad_final_gain[3] = {4096, 4096, 4096};
int cabc_final_gain[3] = {4096, 4096, 4096};
static int pre_gamma[3][65] = {
	{
		0, 16, 32, 48, 64, 80, 96, 112, 128, 144,
		160, 176, 192, 208, 224, 240, 256, 272, 288, 304,
		320, 336, 352, 368, 384, 400, 416, 432, 448, 464,
		480, 496, 512, 528, 544, 560, 576, 592, 608, 624,
		640, 656, 672, 688, 704, 720, 736, 752, 768, 784,
		800, 816, 832, 848, 864, 880, 896, 912, 928, 944,
		960, 976, 992, 1008, 1023
	},
	{
		0, 16, 32, 48, 64, 80, 96, 112, 128, 144,
		160, 176, 192, 208, 224, 240, 256, 272, 288, 304,
		320, 336, 352, 368, 384, 400, 416, 432, 448, 464,
		480, 496, 512, 528, 544, 560, 576, 592, 608, 624,
		640, 656, 672, 688, 704, 720, 736, 752, 768, 784,
		800, 816, 832, 848, 864, 880, 896, 912, 928, 944,
		960, 976, 992, 1008, 1023
	},
	{
		0, 16, 32, 48, 64, 80, 96, 112, 128, 144,
		160, 176, 192, 208, 224, 240, 256, 272, 288, 304,
		320, 336, 352, 368, 384, 400, 416, 432, 448, 464,
		480, 496, 512, 528, 544, 560, 576, 592, 608, 624,
		640, 656, 672, 688, 704, 720, 736, 752, 768, 784,
		800, 816, 832, 848, 864, 880, 896, 912, 928, 944,
		960, 976, 992, 1008, 1023
	}
};

static int o_bl_mapping[13] = {
	2048, 2048, 2048, 2048, 2048, 2662, 2867,
	3072, 3276, 3481, 3686, 3891, 4096
};

static int maxbin_bl_mapping[13] = {
	1984, 1984, 3292, 3292, 3292, 3292, 3292,
	3292, 3592, 4096, 4096, 4096, 4096
};

static struct pre_gamma_table_s pre_gam;
static int sensor_rgb[3] = {243, 256, 314};
static struct aad_param_s aad_parm = {
	.aad_sensor_mode = 1,
	.aad_mode = 2,
	.aad_xyz2rgb_matrix = xyz2rgb_matrix,
	.aad_rgb2xyz_matrix = rgb2xyz_matrix,
	.aad_xyz2lms_matrix = xyz2lms_matrix,
	.aad_lms2xyz_matrix = lms2xyz_matrix,
	.aad_rgb2lms_matrix = rgb2lms_matrix,
	.aad_lms2rgb_matrix = lms2rgb_matrix,
	.aad_LUT_Y_gain = lut_Y_gain,
	.aad_LUT_RG_gain = lut_RG_gain,
	.aad_LUT_BG_gain = lut_BG_gain,
	.aad_Y_gain_min = 64,
	.aad_Y_gain_max = 0x4000,
	.aad_RG_gain_min = 64,
	.aad_RG_gain_max = 0x4000,
	.aad_BG_gain_min = 64,
	.aad_BG_gain_max = 0x4000,
	.aad_tf_en = 1,
	.aad_tf_alpha = 0x20,
	.aad_dist_mode = 0,
	.aad_force_gain_en = 0,
	.aad_gain_lut = gain_lut,
	.aad_xy_lut = xy_lut,
	.aad_r_gain = 4096,
	.aad_g_gain = 4096,
	.aad_b_gain = 4096,
	.aad_min_dist_th = 4096,
	.sensor_input = sensor_rgb,
};

static struct cabc_param_s cabc_parm = {
	.cabc_max95_ratio = 10,
	.cabc_hist_mode = 0,
	.cabc_hist_blend_alpha = 64,
	.cabc_init_bl_min = 0x80,
	.cabc_init_bl_max = 0xfff,
	.cabc_tf_alpha = 0x20,
	.cabc_tf_en = 1,
	.cabc_sc_flag = 1,
	.cabc_sc_hist_diff_thd = 1920 * 1080 / 3,
	.cabc_sc_apl_diff_thd = 24 << (LED_BL_BW - PSTHIST_NRM),
	.cabc_en = 0,
	.cabc_bl_map_mode = 0,
	.cabc_bl_map_en = 1,
	.o_bl_cv = o_bl_mapping,
	.maxbin_bl_cv = maxbin_bl_mapping,
	.cabc_patch_bl_th = 25,
	.cabc_patch_on_alpha = 30,
	.cabc_patch_bl_off_th = 10,
	.cabc_patch_off_alpha = 15,
	.cabc_temp_proc = 1,
};

static struct pre_gam_param_s pre_gam_parm = {
	.pre_gamma_gain_ratio = 16,
};

static struct cabc_debug_param_s cabc_dbg_parm = {
	.dbg_cabc_gain = 0,
	.avg = 0,
	.max95 = 0,
	.tf_bl = 0,
};

static struct aad_debug_param_s aad_dbg_parm = {
	.y_val = 0,
	.rg_val = 0,
	.bg_val = 0,
	.cur_frm_gain = 0,
};

static char aad_ver[32] = "aad_v1_20210621";
int cur_o_gain[3] = {4096, 4096, 4096};
#ifdef CONFIG_AMLOGIC_LCD
static u32 pre_backlight;
#endif
struct aad_fw_param_s fw_aad_parm = {
	.fw_aad_en = 0,
	.fw_aad_status = 0,
	.fw_aad_ver = aad_ver,
	.aad_param = &aad_parm,
	.cur_gain = cur_o_gain,
	.aad_debug_mode = 0,
	.dbg_param = &aad_dbg_parm,
	.aad_alg = NULL,
};

static char cabc_ver[32] = "cabc_v1_20210621";
struct cabc_fw_param_s fw_cabc_parm = {
	.fw_cabc_en = 1,
	.fw_cabc_status = 0,
	.fw_cabc_ver = cabc_ver,
	.cabc_param = &cabc_parm,
	.i_hist = hist,
	.cur_bl = 0,
	.tgt_bl = 0,
	.cabc_debug_mode = 0,
	.dbg_param = &cabc_dbg_parm,
	.cabc_alg = NULL,
};

int *vf_hist_get(void)
{
	return fw_cabc_parm.i_hist;
}

struct pgm_param_s fw_pre_gma_parm = {
	.fw_pre_gamma_en = 1,
	.pre_gam_param = &pre_gam_parm,
	.aad_gain = aad_final_gain,
	.cabc_gain = cabc_final_gain,
	.pre_gamma_proc = NULL,
};

static int gain_pre_gamma_init(void)
{
	int i, j;

	for (i = 0; i < 3; i++) {
		aad_final_gain[i] = 4096;
		cabc_final_gain[i] = 4096;
	}

	for (i = 0; i < 3; i++) {
		for (j = 0; j < 64; j++)
			pre_gamma[i][j] = j << 4;
		pre_gamma[i][64] = 1023;
	}
	return 0;
}

static void pre_gamma_data_cp(struct pre_gamma_table_s *pre_gam)
{
	memcpy(pre_gam->lut_r, pre_gamma[0], sizeof(int) * 65);
	memcpy(pre_gam->lut_g, pre_gamma[1], sizeof(int) * 65);
	memcpy(pre_gam->lut_b, pre_gamma[2], sizeof(int) * 65);
}

struct aad_fw_param_s *aad_fw_param_get(void)
{
	return &fw_aad_parm;
}
EXPORT_SYMBOL(aad_fw_param_get);

struct cabc_fw_param_s *cabc_fw_param_get(void)
{
	return &fw_cabc_parm;
}
EXPORT_SYMBOL(cabc_fw_param_get);

struct pgm_param_s *pregam_fw_param_get(void)
{
	return &fw_pre_gma_parm;
}
EXPORT_SYMBOL(pregam_fw_param_get);

int fw_en_get(void)
{
	return cabc_aad_en;
}

void backlight_update_ctrl(u32 en)
{
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	if (is_amdv_on())
		return;
#endif
#ifdef CONFIG_AMLOGIC_LCD
		aml_lcd_atomic_notifier_call_chain(LCD_EVENT_BACKLIGHT_GD_SEL,
			&en);
#endif
}

static void backlight_setting_update(u32 backlight)
{
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	if (is_amdv_on())
		return;
#endif
#ifdef CONFIG_AMLOGIC_LCD
	if (pre_backlight != backlight) {
		aml_lcd_atomic_notifier_call_chain(LCD_EVENT_BACKLIGHT_GD_DIM,
			&backlight);
		pre_backlight = backlight;
	}
#endif
}

void aml_cabc_alg_process(struct work_struct *work)
{
	u32 backlight;

	if (!fw_aad_parm.aad_alg) {
		if (pr_cabc_aad & AAD_DEBUG)
			pr_cabc_aad_dbg("%s: aad alg func is NULL\n", __func__);
	} else {
		fw_aad_parm.aad_alg(&fw_aad_parm, aad_final_gain);
	}

	if (!fw_cabc_parm.cabc_alg) {
		if (pr_cabc_aad & CABC_DEBUG)
			pr_cabc_aad_dbg("%s: cabc alg func is NULL\n", __func__);
	} else {
		fw_cabc_parm.cabc_alg(&fw_cabc_parm, cabc_final_gain);
		backlight = fw_cabc_parm.tgt_bl;
		backlight_setting_update(backlight);
		if (fw_cabc_parm.cabc_param->cabc_temp_proc == 0) {
			backlight_update_ctrl(0);
			fw_cabc_parm.cabc_param->cabc_temp_proc = 1;
		}
	}

	if (!fw_pre_gma_parm.pre_gamma_proc) {
		if (pr_cabc_aad & PRE_GAM_DEBUG)
			pr_cabc_aad_dbg("%s: pre gamma proc is NULL\n", __func__);
	} else {
		pre_gam.en = fw_pre_gma_parm.fw_pre_gamma_en;
		fw_pre_gma_parm.pre_gamma_proc(&fw_pre_gma_parm, pre_gamma);
		pre_gamma_data_cp(&pre_gam);
		set_pre_gamma_reg(&pre_gam);
	}

	if (!status_flag)
		status_flag = 1;

	if (fw_pre_gma_parm.pre_gamma_proc &&
		fw_cabc_parm.cabc_alg &&
		cabc_aad_en &&
		fw_cabc_parm.cabc_param->cabc_en &&
		pre_cabc_en != fw_cabc_parm.cabc_param->cabc_en) {
		backlight_update_ctrl(1);
		pre_cabc_en = fw_cabc_parm.cabc_param->cabc_en;
	}
}

void aml_cabc_alg_bypass(struct work_struct *work)
{
	if (!cabc_aad_en && status_flag) {
		pre_gam.en = 0;
		gain_pre_gamma_init();
		pre_gamma_data_cp(&pre_gam);
		set_pre_gamma_reg(&pre_gam);
		pr_cabc_aad_dbg("%s\n", __func__);
		status_flag = 0;
		if (pre_cabc_en) {
			backlight_update_ctrl(0);
			pre_cabc_en = 0;
		}
	}
}

void cabc_aad_d_convert_str(int num, char cur_s[], int bit_chose)
{
	char buf[9] = {0};
	int i, count, cur_s_len;

	if (bit_chose == 10)
		snprintf(buf, sizeof(buf), "%d", num);
	else if (bit_chose == 16)
		snprintf(buf, sizeof(buf), "%x", num);

	count = strlen(buf);
	cur_s_len = strlen(cur_s);

	buf[count] = ' ';

	for (i = 0; i < count + 1; i++)
		cur_s[i + cur_s_len] = buf[i];
}

ssize_t debug_cabc_alg_state(char *buf)
{
	int i = 0;
	ssize_t len = 0;
	struct cabc_fw_param_s *fw_cabc_param = cabc_fw_param_get();

	if (debug_cabc_aad == 0x30) {
		return sprintf(buf, "for_tool:%d\n",
			fw_cabc_param->fw_cabc_en);
	} else if (debug_cabc_aad == 0x31) {
		return sprintf(buf, "for_tool:%d\n",
			fw_cabc_param->cabc_param->cabc_hist_mode);
	} else if (debug_cabc_aad == 0x32) {
		return sprintf(buf, "for_tool:%d\n",
			fw_cabc_param->cabc_param->cabc_tf_en);
	} else if (debug_cabc_aad == 0x33) {
		return sprintf(buf, "for_tool:%d\n",
			fw_cabc_param->cabc_param->cabc_sc_flag);
	} else if (debug_cabc_aad == 0x34) {
		return sprintf(buf, "for_tool:%d\n",
			fw_cabc_param->cabc_param->cabc_bl_map_mode);
	} else if (debug_cabc_aad == 0x35) {
		return sprintf(buf, "for_tool:%d\n",
			fw_cabc_param->cabc_param->cabc_bl_map_en);
	} else if (debug_cabc_aad == 0x36) {
		return sprintf(buf, "for_tool:%d\n",
			fw_cabc_param->cabc_param->cabc_temp_proc);
	} else if (debug_cabc_aad == 0x37) {
		return sprintf(buf, "for_tool:%d\n",
			fw_cabc_param->cabc_param->cabc_max95_ratio);
	} else if (debug_cabc_aad == 0x38) {
		return sprintf(buf, "for_tool:%d\n",
			fw_cabc_param->cabc_param->cabc_hist_blend_alpha);
	} else if (debug_cabc_aad == 0x39) {
		return sprintf(buf, "for_tool:%d\n",
			fw_cabc_param->cabc_param->cabc_init_bl_min);
	} else if (debug_cabc_aad == 0x3A) {
		return sprintf(buf, "for_tool:%d\n",
					fw_cabc_param->cabc_param->cabc_init_bl_max);
	} else if (debug_cabc_aad == 0x3B) {
		return sprintf(buf, "for_tool:%d\n",
			fw_cabc_param->cabc_param->cabc_tf_alpha);
	} else if (debug_cabc_aad == 0x3C) {
		return sprintf(buf, "for_tool:%d\n",
			fw_cabc_param->cabc_param->cabc_sc_hist_diff_thd);
	} else if (debug_cabc_aad == 0x3D) {
		return sprintf(buf, "for_tool:%d\n",
			fw_cabc_param->cabc_param->cabc_sc_apl_diff_thd);
	} else if (debug_cabc_aad == 0x3E) {
		return sprintf(buf, "for_tool:%d\n",
			fw_cabc_param->cabc_param->cabc_patch_bl_th);
	} else if (debug_cabc_aad == 0x3F) {
		return sprintf(buf, "for_tool:%d\n",
			fw_cabc_param->cabc_param->cabc_patch_on_alpha);
	} else if (debug_cabc_aad == 0x40) {
		return sprintf(buf, "for_tool:%d\n",
			fw_cabc_param->cabc_param->cabc_patch_bl_off_th);
	} else if (debug_cabc_aad == 0x41) {
		return sprintf(buf, "for_tool:%d\n",
			fw_cabc_param->cabc_param->cabc_patch_off_alpha);
	} else if (debug_cabc_aad == 0x42) {
		char *stemp = NULL;

		stemp = kmalloc(100, GFP_KERNEL);
		if (!stemp)
			return 0;
		memset(stemp, 0, 100);

		for (i = 0; i < 13; i++) {
			pr_info("o_bl_cv[%d] = %d\n", i, fw_cabc_param->cabc_param->o_bl_cv[i]);
			cabc_aad_d_convert_str(fw_cabc_param->cabc_param->o_bl_cv[i],
				stemp, 10);
		}

		len = sprintf(buf, "for_tool:%s\n", stemp);
		kfree(stemp);

		return len;
	} else if (debug_cabc_aad == 0x43) {
		char *stemp = NULL;

		stemp = kmalloc(100, GFP_KERNEL);
		if (!stemp)
			return 0;
		memset(stemp, 0, 100);

		for (i = 0; i < 13; i++) {
			pr_info("maxbin_bl_cv[%d] = %d\n",
				i, fw_cabc_param->cabc_param->maxbin_bl_cv[i]);
			cabc_aad_d_convert_str(fw_cabc_param->cabc_param->maxbin_bl_cv[i],
				stemp, 10);
		}
		len = sprintf(buf, "for_tool:%s\n", stemp);
		kfree(stemp);

		return len;
	}

	return 0;
}

int cabc_alg_state(void)
{
	int i;
	struct cabc_fw_param_s *fw_cabc_param = cabc_fw_param_get();

	pr_info("\n--------cabc alg print-------\n");
	pr_info("fw_cabc_en = %d\n", fw_cabc_param->fw_cabc_en);
	pr_info("cabc_status = %d\n", fw_cabc_param->fw_cabc_status);
	pr_info("fw_cabc_ver = %s\n", fw_cabc_param->fw_cabc_ver);

	pr_info("\n--------cabc alg parameters-------\n");
	pr_info("cabc_max95_ratio = %d\n", fw_cabc_param->cabc_param->cabc_max95_ratio);
	pr_info("cabc_hist_mode = %d\n", fw_cabc_param->cabc_param->cabc_hist_mode);
	pr_info("cabc_hist_blend_alpha = %d\n",
		fw_cabc_param->cabc_param->cabc_hist_blend_alpha);
	pr_info("cabc_init_bl_min = %d\n",
		fw_cabc_param->cabc_param->cabc_init_bl_min);
	pr_info("cabc_init_bl_max = %d\n",
		fw_cabc_param->cabc_param->cabc_init_bl_max);
	pr_info("cabc_tf_alpha = %d\n",
		fw_cabc_param->cabc_param->cabc_tf_alpha);
	pr_info("cabc_tf_en = %d\n", fw_cabc_param->cabc_param->cabc_tf_en);
	pr_info("cabc_sc_flag = %d\n", fw_cabc_param->cabc_param->cabc_sc_flag);
	pr_info("cabc_sc_hist_diff_thd = %d\n",
		fw_cabc_param->cabc_param->cabc_sc_hist_diff_thd);
	pr_info("cabc_sc_apl_diff_thd = %d\n",
		fw_cabc_param->cabc_param->cabc_sc_apl_diff_thd);
	pr_info("cabc_en = %d\n",
		fw_cabc_param->cabc_param->cabc_en);
	pr_info("cabc_bl_map_mode = %d\n",
		fw_cabc_param->cabc_param->cabc_bl_map_mode);
	pr_info("cabc_bl_map_en = %d\n",
		fw_cabc_param->cabc_param->cabc_bl_map_en);
	pr_info("cabc_patch_bl_th = %d\n",
		fw_cabc_param->cabc_param->cabc_patch_bl_th);
	pr_info("cabc_patch_on_alpha = %d\n",
		fw_cabc_param->cabc_param->cabc_patch_on_alpha);
	pr_info("cabc_patch_bl_off_th = %d\n",
		fw_cabc_param->cabc_param->cabc_patch_bl_off_th);
	pr_info("cabc_patch_off_alpha = %d\n",
		fw_cabc_param->cabc_param->cabc_patch_off_alpha);
	pr_info("cabc_temp_proc = %d\n",
		fw_cabc_param->cabc_param->cabc_temp_proc);

	for (i = 0; i < 13; i++)
		pr_info("o_bl_mapping[%d] = %d\n", i, fw_cabc_param->cabc_param->o_bl_cv[i]);

	for (i = 0; i < 13; i++)
		pr_info("maxbin_bl_mapping[%d] = %d\n",
		i, fw_cabc_param->cabc_param->maxbin_bl_cv[i]);

	pr_info("\n--------fw vpp y hist-------\n");
	if (fw_cabc_param->i_hist) {
		for (i = 0; i < 16; i++)
			pr_info("i_hist[%2d ~ %2d] = 0x%5x, 0x%5x, 0x%5x, 0x%5x\n",
				i * 4, i * 4 + 3,
				fw_cabc_param->i_hist[i * 4],
				fw_cabc_param->i_hist[i * 4 + 1],
				fw_cabc_param->i_hist[i * 4 + 2],
				fw_cabc_param->i_hist[i * 4 + 3]);
	}

	pr_info("cur_bl = %d\n", fw_cabc_param->cur_bl);
	pr_info("tgt_bl = %d\n", fw_cabc_param->tgt_bl);
	pr_info("\n--------cabc final gain-------\n");
	for (i = 0; i < 3; i++)
		pr_info("cabc_final_gain[%d] = %d\n", i, cabc_final_gain[i]);
	pr_info("cabc_debug_mode = %d\n", fw_cabc_param->cabc_debug_mode);

	pr_info("\n--------cabc debug parameters-------\n");
	pr_info("dbg_cabc_gain = %d\n", fw_cabc_param->dbg_param->dbg_cabc_gain);
	pr_info("avg = %d\n", fw_cabc_param->dbg_param->avg);
	pr_info("max95 = %d\n", fw_cabc_param->dbg_param->max95);
	pr_info("tf_bl = %d\n", fw_cabc_param->dbg_param->tf_bl);

	pr_info("cabc_alg = %p\n", fw_cabc_param->cabc_alg);

	return 0;
}

void db_cabc_param_set(struct db_cabc_param_s *db_cabc_param_data)
{
	unsigned int i = 0;
	void __user *argp;
	unsigned int mem_size;

	int db_o_bl_cv[13] = {0};
	int db_maxbin_bl_cv[13] = {0};

	if (!db_cabc_param_data) {
		pr_info("db_cabc_param_data is NULL\n");
		return;
	}

	/*db_o_bl_cv data receive*/
	{
		argp = (void __user *)db_cabc_param_data->db_o_bl_cv.cabc_aad_param_ptr;
		mem_size = sizeof(int) * db_cabc_param_data->db_o_bl_cv.length;
		pr_info("aad argp = %p	db_o_bl_cv.length=%d mem_size=%d\n",
			argp, db_cabc_param_data->db_o_bl_cv.length, mem_size);
		if (db_cabc_param_data->db_o_bl_cv.length > mem_size) {
			db_cabc_param_data->db_o_bl_cv.length = mem_size;
			pr_info("db_o_bl_cv system control length > kernel length\n");
		}
		if (copy_from_user(db_o_bl_cv,
				   argp,
				   mem_size))
			pr_info("db_o_bl_cv control db_cabc_aad_param_s fail\n");
		else
			pr_info("db_o_bl_cv control db_cabc_aad_param_s success\n");
	}
	/*db_maxbin_bl_cv data receive*/
	{
		argp = (void __user *)db_cabc_param_data->db_maxbin_bl_cv.cabc_aad_param_ptr;
		mem_size = sizeof(int) * db_cabc_param_data->db_maxbin_bl_cv.length;
		if (db_cabc_param_data->db_maxbin_bl_cv.length > mem_size) {
			db_cabc_param_data->db_maxbin_bl_cv.length = mem_size;
			pr_info("db_maxbin_bl_cv system control length > kernel length\n");
		}
		if (copy_from_user(db_maxbin_bl_cv,
				   argp,
				   mem_size))
			pr_info("db_maxbin_bl_cv control db_cabc_aad_param_s fail\n");
		else
			pr_info("db_maxbin_bl_cv control db_cabc_aad_param_s success\n");
	}

	if (pr_cabc_aad & CABC_DEBUG) {
		pr_info("data from user cabc_param_cabc_en = %d\n",
			db_cabc_param_data->cabc_param_cabc_en);
		pr_info("data from user cabc_param_hist_mode = %d\n",
			db_cabc_param_data->cabc_param_hist_mode);
		pr_info("data from user cabc_param_tf_en = %d\n",
			db_cabc_param_data->cabc_param_tf_en);
		pr_info("data from user cabc_param_sc_flag = %d\n",
			db_cabc_param_data->cabc_param_sc_flag);
		pr_info("data from user cabc_param_bl_map_mode = %d\n",
			db_cabc_param_data->cabc_param_bl_map_mode);
		pr_info("data from user cabc_param_bl_map_en = %d\n",
			db_cabc_param_data->cabc_param_bl_map_en);
		pr_info("data from user cabc_param_temp_proc = %d\n",
			db_cabc_param_data->cabc_param_temp_proc);
		pr_info("data from user cabc_param_max95_ratio = %d\n",
			db_cabc_param_data->cabc_param_max95_ratio);
		pr_info("data from user cabc_param_hist_blend_alpha = %d\n",
			db_cabc_param_data->cabc_param_hist_blend_alpha);
		pr_info("data from user cabc_param_init_bl_min = %d\n",
			db_cabc_param_data->cabc_param_init_bl_min);
		pr_info("data from user cabc_param_init_bl_max = %d\n",
			db_cabc_param_data->cabc_param_init_bl_max);
		pr_info("data from user cabc_param_tf_alpha = %d\n",
			db_cabc_param_data->cabc_param_tf_alpha);

		pr_info("data from user cabc_param_sc_hist_diff_thd = %d\n",
			db_cabc_param_data->cabc_param_sc_hist_diff_thd);
		pr_info("data from user cabc_param_sc_apl_diff_thd = %d\n",
			db_cabc_param_data->cabc_param_sc_apl_diff_thd);
		pr_info("data from user cabc_param_patch_bl_th = %d\n",
			db_cabc_param_data->cabc_param_patch_bl_th);
		pr_info("data from user cabc_param_patch_on_alpha = %d\n",
			db_cabc_param_data->cabc_param_patch_on_alpha);
		pr_info("data from user cabc_param_patch_bl_off_th = %d\n",
			db_cabc_param_data->cabc_param_patch_bl_off_th);
		pr_info("data from user cabc_param_patch_off_alpha = %d\n",
			db_cabc_param_data->cabc_param_patch_off_alpha);

		for (i = 0; i < db_cabc_param_data->db_o_bl_cv.length; i++)
			pr_info("data from user db_o_bl_cv[%d] = %d\n",
				i, db_o_bl_cv[i]);
		for (i = 0; i < db_cabc_param_data->db_maxbin_bl_cv.length; i++)
			pr_info("data from user db_o_bl_cv[%d] = %d\n",
				i, db_maxbin_bl_cv[i]);
	}

	fw_cabc_parm.fw_cabc_en = db_cabc_param_data->cabc_param_cabc_en;
	fw_cabc_parm.cabc_param->cabc_hist_mode =
		db_cabc_param_data->cabc_param_hist_mode;
	fw_cabc_parm.cabc_param->cabc_tf_en =
		db_cabc_param_data->cabc_param_tf_en;
	fw_cabc_parm.cabc_param->cabc_sc_flag =
		db_cabc_param_data->cabc_param_sc_flag;
	fw_cabc_parm.cabc_param->cabc_bl_map_mode =
		db_cabc_param_data->cabc_param_bl_map_mode;
	fw_cabc_parm.cabc_param->cabc_bl_map_en =
		db_cabc_param_data->cabc_param_bl_map_en;
	fw_cabc_parm.cabc_param->cabc_temp_proc =
		db_cabc_param_data->cabc_param_temp_proc;
	fw_cabc_parm.cabc_param->cabc_max95_ratio =
		db_cabc_param_data->cabc_param_max95_ratio;
	fw_cabc_parm.cabc_param->cabc_hist_blend_alpha =
		db_cabc_param_data->cabc_param_hist_blend_alpha;
	fw_cabc_parm.cabc_param->cabc_init_bl_min =
		db_cabc_param_data->cabc_param_init_bl_min;
	fw_cabc_parm.cabc_param->cabc_init_bl_max =
		db_cabc_param_data->cabc_param_init_bl_max;
	fw_cabc_parm.cabc_param->cabc_tf_alpha =
		db_cabc_param_data->cabc_param_tf_alpha;
	fw_cabc_parm.cabc_param->cabc_sc_hist_diff_thd =
		db_cabc_param_data->cabc_param_sc_hist_diff_thd;
	fw_cabc_parm.cabc_param->cabc_sc_apl_diff_thd =
		db_cabc_param_data->cabc_param_sc_apl_diff_thd;
	fw_cabc_parm.cabc_param->cabc_patch_bl_th =
		db_cabc_param_data->cabc_param_patch_bl_th;
	fw_cabc_parm.cabc_param->cabc_patch_on_alpha =
		db_cabc_param_data->cabc_param_patch_on_alpha;
	fw_cabc_parm.cabc_param->cabc_patch_bl_off_th =
		db_cabc_param_data->cabc_param_patch_bl_off_th;
	fw_cabc_parm.cabc_param->cabc_patch_off_alpha =
		db_cabc_param_data->cabc_param_patch_off_alpha;

	memcpy(fw_cabc_parm.cabc_param->o_bl_cv, db_o_bl_cv, sizeof(db_o_bl_cv));
	memcpy(fw_cabc_parm.cabc_param->maxbin_bl_cv, db_maxbin_bl_cv, sizeof(db_maxbin_bl_cv));
}

void db_aad_param_set(struct db_aad_param_s *db_aad_param_data)
{
	unsigned int i = 0, j = 0;
	void __user *argp;
	unsigned int mem_size;
	int LUT_Y_gain_cur[17]	= {0};
	int LUT_RG_gain_cur[17] = {0};
	int LUT_BG_gain_cur[17] = {0};
	int gain_lut_cur[48]	= {0};
	int xy_lut_cur[32]		= {0};

	if (!db_aad_param_data) {
		pr_info("db_aad_param_data is NULL\n");
		return;
	}

	/*db_LUT_Y_gain data receive*/
	{
		argp = (void __user *)db_aad_param_data->db_LUT_Y_gain.cabc_aad_param_ptr;
		mem_size = sizeof(int) * db_aad_param_data->db_LUT_Y_gain.length;
		if (db_aad_param_data->db_LUT_Y_gain.length > mem_size) {
			db_aad_param_data->db_LUT_Y_gain.length = mem_size;
			pr_info("db_LUT_Y_gain system control length > kernel length\n");
		}
		if (copy_from_user(LUT_Y_gain_cur,
				   argp,
				   mem_size))
			pr_info("db_LUT_RG_gain control db_cabc_aad_param_s fail\n");
		else
			pr_info("db_LUT_RG_gain control db_cabc_aad_param_s success\n");
	}
	/*db_LUT_RG_gain data receive*/
	{
		argp = (void __user *)db_aad_param_data->db_LUT_RG_gain.cabc_aad_param_ptr;
		mem_size = sizeof(int) * db_aad_param_data->db_LUT_RG_gain.length;
		if (db_aad_param_data->db_LUT_RG_gain.length > mem_size) {
			db_aad_param_data->db_LUT_RG_gain.length = mem_size;
			pr_info("db_LUT_RG_gain system control length > kernel length\n");
		}
		if (copy_from_user(LUT_RG_gain_cur,
				   argp,
				   mem_size))
			pr_info("db_LUT_RG_gain control db_cabc_aad_param_s fail\n");
		else
			pr_info("db_LUT_RG_gain control db_cabc_aad_param_s success\n");
	}
	/*db_LUT_BG_gain data receive*/
	{
		argp = (void __user *)db_aad_param_data->db_LUT_BG_gain.cabc_aad_param_ptr;
		mem_size = sizeof(int) * db_aad_param_data->db_LUT_BG_gain.length;
		if (db_aad_param_data->db_LUT_BG_gain.length > mem_size) {
			db_aad_param_data->db_LUT_BG_gain.length = mem_size;
			pr_info("db_LUT_RG_gain system control length > kernel length\n");
		}
		if (copy_from_user(LUT_BG_gain_cur,
				   argp,
				   mem_size))
			pr_info("db_LUT_RG_gain control db_cabc_aad_param_s fail\n");
		else
			pr_info("db_LUT_BG_gain control db_cabc_aad_param_s success\n");
	}
	/*db_gain_lut data receive*/
	{
		argp = (void __user *)db_aad_param_data->db_gain_lut.cabc_aad_param_ptr;
		mem_size = sizeof(int) * db_aad_param_data->db_gain_lut.length;
		if (db_aad_param_data->db_gain_lut.length > mem_size) {
			db_aad_param_data->db_gain_lut.length = mem_size;
			pr_info("db_gain_lut system control length > kernel length\n");
		}
		if (copy_from_user(gain_lut_cur,
				   argp,
				   mem_size))
			pr_info("db_gain_lut control db_cabc_aad_param_s fail\n");
		else
			pr_info("db_gain_lut control db_cabc_aad_param_s success\n");
	}
	/*db_xy_lut data receive*/
	{
		argp = (void __user *)db_aad_param_data->db_xy_lut.cabc_aad_param_ptr;
		mem_size = sizeof(int) * db_aad_param_data->db_xy_lut.length;
		if (db_aad_param_data->db_xy_lut.length > mem_size) {
			db_aad_param_data->db_xy_lut.length = mem_size;
			pr_info("db_xy_lut system control length > kernel length\n");
		}
		if (copy_from_user(xy_lut_cur,
				   argp,
				   mem_size))
			pr_info("db_xy_lut control db_cabc_aad_param_s fail\n");
		else
			pr_info("db_xy_lut control db_cabc_aad_param_s success\n");
	}

	if (pr_cabc_aad & AAD_DEBUG) {
		pr_info("data from user aad_param_cabc_aad_en = %d\n",
		db_aad_param_data->aad_param_cabc_aad_en);
		pr_info("data from user aad_param_aad_en = %d\n",
			db_aad_param_data->aad_param_aad_en);
		pr_info("data from user aad_param_tf_en = %d\n",
			db_aad_param_data->aad_param_tf_en);
		pr_info("data from user aad_param_force_gain_en = %d\n",
			db_aad_param_data->aad_param_force_gain_en);
		pr_info("data from user aad_param_sensor_mode = %d\n",
			db_aad_param_data->aad_param_sensor_mode);
		pr_info("data from user aad_param_mode = %d\n",
			db_aad_param_data->aad_param_mode);
		pr_info("data from user aad_param_dist_mode = %d\n",
			db_aad_param_data->aad_param_dist_mode);
		pr_info("data from user aad_param_tf_alpha = %d\n",
			db_aad_param_data->aad_param_tf_alpha);
		for (i = 0; i < 3; i++)
			pr_info("data from user aad_param_sensor_input[%d] = %d\n",
			i, db_aad_param_data->aad_param_sensor_input[i]);
		for (i = 0; i < db_aad_param_data->db_LUT_Y_gain.length; i++)
			pr_info("data from user db_LUT_Y_gain[%d] = %d\n",
				i, LUT_Y_gain_cur[i]);
		for (i = 0; i < db_aad_param_data->db_LUT_RG_gain.length; i++)
			pr_info("data from user db_LUT_Y_gain[%d] = %d\n",
				i, LUT_RG_gain_cur[i]);
		for (i = 0; i < db_aad_param_data->db_LUT_BG_gain.length; i++)
			pr_info("data from user db_LUT_Y_gain[%d] = %d\n",
				i, LUT_BG_gain_cur[i]);
		for (i = 0; i < db_aad_param_data->db_gain_lut.length; i++)
			pr_info("data from user db_LUT_Y_gain[%d] = %d\n",
				i, gain_lut_cur[i]);
		for (i = 0; i < db_aad_param_data->db_xy_lut.length; i++)
			pr_info("data from user db_LUT_Y_gain[%d] = %d\n",
				i, xy_lut_cur[i]);
	}

	cabc_aad_en = db_aad_param_data->aad_param_cabc_aad_en;
	fw_aad_parm.fw_aad_en = db_aad_param_data->aad_param_aad_en;
	fw_aad_parm.aad_param->aad_tf_en = db_aad_param_data->aad_param_tf_en;
	fw_aad_parm.aad_param->aad_force_gain_en = db_aad_param_data->aad_param_force_gain_en;
	fw_aad_parm.aad_param->aad_sensor_mode = db_aad_param_data->aad_param_sensor_mode;
	fw_aad_parm.aad_param->aad_mode = db_aad_param_data->aad_param_mode;
	fw_aad_parm.aad_param->aad_dist_mode = db_aad_param_data->aad_param_dist_mode;
	fw_aad_parm.aad_param->aad_tf_alpha = db_aad_param_data->aad_param_tf_alpha;

	memcpy(fw_aad_parm.aad_param->sensor_input, db_aad_param_data->aad_param_sensor_input,
		sizeof(db_aad_param_data->aad_param_sensor_input));
	memcpy(fw_aad_parm.aad_param->aad_LUT_Y_gain, LUT_Y_gain_cur, sizeof(LUT_Y_gain_cur));
	memcpy(fw_aad_parm.aad_param->aad_LUT_RG_gain, LUT_RG_gain_cur, sizeof(LUT_RG_gain_cur));
	memcpy(fw_aad_parm.aad_param->aad_LUT_BG_gain, LUT_BG_gain_cur, sizeof(LUT_BG_gain_cur));

	for (i = 0; i < 16; i++)
		for (j = 0; j < 3; j++)
			fw_aad_parm.aad_param->aad_gain_lut[i][j] =
				gain_lut_cur[i * 3 + j];

	for (i = 0; i < 16; i++)
		for (j = 0; j < 2; j++)
			fw_aad_parm.aad_param->aad_xy_lut[i][j] =
				xy_lut_cur[i * 2 + j];
}

ssize_t debug_pre_gamma_alg_state(char *buf)
{
	return 0;
}

ssize_t debug_aad_alg_state(char *buf)
{
	int i, j;
	int len = 0;
	struct aad_fw_param_s *fw_aad_param = aad_fw_param_get();

	if (debug_cabc_aad == 0x01) {
		return sprintf(buf, "for_tool:%d\n", cabc_aad_en);
	} else if (debug_cabc_aad == 0x02) {
		return sprintf(buf, "for_tool:%d\n", fw_aad_param->fw_aad_en);
	} else if (debug_cabc_aad == 0x03) {
		return sprintf(buf, "for_tool:%d\n", fw_aad_param->aad_param->aad_tf_en);
	} else if (debug_cabc_aad == 0x04) {
		return sprintf(buf, "for_tool:%d\n", fw_aad_param->aad_param->aad_force_gain_en);
	} else if (debug_cabc_aad == 0x05) {
		return sprintf(buf, "for_tool:%d\n", fw_aad_param->aad_param->aad_sensor_mode);
	} else if (debug_cabc_aad == 0x06) {
		return sprintf(buf, "for_tool:%d\n", fw_aad_param->aad_param->aad_mode);
	} else if (debug_cabc_aad == 0x07) {
		return sprintf(buf, "for_tool:%d\n", fw_aad_param->aad_param->aad_dist_mode);
	} else if (debug_cabc_aad == 0x08) {
		return sprintf(buf, "for_tool:%d\n", fw_aad_param->aad_param->aad_tf_alpha);
	} else if (debug_cabc_aad == 0x09) {
		char *stemp = NULL;

		stemp = kmalloc(100, GFP_KERNEL);
		if (!stemp)
			return 0;
		memset(stemp, 0, 100);

		for (i = 0; i < 3; i++)
			cabc_aad_d_convert_str(fw_aad_param->aad_param->sensor_input[i],
				stemp, 10);

		len = sprintf(buf, "for_tool:%s\n", stemp);
		kfree(stemp);

		return len;
	} else if (debug_cabc_aad == 0x0A) {
		char *stemp = NULL;

		stemp = kmalloc(100, GFP_KERNEL);
		if (!stemp)
			return 0;
		memset(stemp, 0, 100);

		for (i = 0; i < 17; i++)
			cabc_aad_d_convert_str(fw_aad_param->aad_param->aad_LUT_Y_gain[i],
				stemp, 10);

		len = sprintf(buf, "for_tool:%s\n", stemp);
		kfree(stemp);

		return len;
	} else if (debug_cabc_aad == 0x0B) {
		char *stemp = NULL;

		stemp = kmalloc(100, GFP_KERNEL);
		if (!stemp)
			return 0;
		memset(stemp, 0, 100);
		for (i = 0; i < 17; i++)
			cabc_aad_d_convert_str(fw_aad_param->aad_param->aad_LUT_RG_gain[i],
				stemp, 10);
		len = sprintf(buf, "for_tool:%s\n", stemp);
		kfree(stemp);

		return len;
	} else if (debug_cabc_aad == 0x0C) {
		char *stemp = NULL;

		stemp = kmalloc(100, GFP_KERNEL);
		if (!stemp)
			return 0;
		memset(stemp, 0, 100);
		for (i = 0; i < 17; i++)
			cabc_aad_d_convert_str(fw_aad_param->aad_param->aad_LUT_BG_gain[i],
				stemp, 10);
		len = sprintf(buf, "for_tool:%s\n", stemp);
		kfree(stemp);
		return len;
	} else if (debug_cabc_aad == 0x0D) {
		int temp[48] = {0};
		char *stemp = NULL;

		stemp = kmalloc(300, GFP_KERNEL);
		if (!stemp)
			return 0;
		memset(stemp, 0, 300);
		for (i = 0; i < 16; i++) {
			for (j = 0; j < 3; j++) {
				temp[i * 3 + j] = fw_aad_param->aad_param->aad_gain_lut[i][j];
				cabc_aad_d_convert_str(temp[i * 3 + j],
					stemp, 10);
			}
		}
		len = sprintf(buf, "for_tool:%s\n", stemp);
		kfree(stemp);
		return len;
	} else if (debug_cabc_aad == 0x0E) {
		int temp[32] = {0};
		char *stemp = NULL;

		stemp = kmalloc(300, GFP_KERNEL);
		if (!stemp)
			return 0;
		memset(stemp, 0, 300);
		for (i = 0; i < 16; i++) {
			for (j = 0; j < 2; j++) {
				temp[i * 2 + j] = fw_aad_param->aad_param->aad_xy_lut[i][j];
				cabc_aad_d_convert_str(temp[i * 2 + j],
					stemp, 10);
			}
		}
		len = sprintf(buf, "for_tool:%s\n", stemp);
		kfree(stemp);
		return len;
	}

	return 0;
}

int aad_alg_state(void)
{
	int i;
	struct aad_fw_param_s *fw_aad_param = aad_fw_param_get();

	pr_info("\n--------aad alg print-------\n");
	pr_info("fw_aad_en = %d\n", fw_aad_param->fw_aad_en);
	pr_info("fw_aad_status = %d\n", fw_aad_param->fw_aad_status);
	pr_info("fw_aad_ver = %s\n", fw_aad_param->fw_aad_ver);
	pr_info("\n--------aadc alg parameters-------\n");
	pr_info("aad_sensor_mode = %d\n", fw_aad_param->aad_param->aad_sensor_mode);
	pr_info("aad_mode = %d\n", fw_aad_param->aad_param->aad_mode);
	for (i = 0; i < 3; i++)
		pr_info("xyz2rgb_matrix[%d] = %d, %d, %d\n",
			i,
			fw_aad_param->aad_param->aad_xyz2rgb_matrix[i * 3],
			fw_aad_param->aad_param->aad_xyz2rgb_matrix[i * 3 + 1],
			fw_aad_param->aad_param->aad_xyz2rgb_matrix[i * 3 + 2]);
	for (i = 0; i < 3; i++)
		pr_info("rgb2xyz_matrix[%d] = %d, %d, %d\n",
			i,
			fw_aad_param->aad_param->aad_rgb2xyz_matrix[i * 3],
			fw_aad_param->aad_param->aad_rgb2xyz_matrix[i * 3 + 1],
			fw_aad_param->aad_param->aad_rgb2xyz_matrix[i * 3 + 2]);
	for (i = 0; i < 3; i++)
		pr_info("xyz2lms_matrix[%d] = %d, %d, %d\n",
			i,
			fw_aad_param->aad_param->aad_xyz2lms_matrix[i * 3],
			fw_aad_param->aad_param->aad_xyz2lms_matrix[i * 3 + 1],
			fw_aad_param->aad_param->aad_xyz2lms_matrix[i * 3 + 2]);
	for (i = 0; i < 3; i++)
		pr_info("lms2xyz_matrix[%d] = %d, %d, %d\n",
			i,
			fw_aad_param->aad_param->aad_lms2xyz_matrix[i * 3],
			fw_aad_param->aad_param->aad_lms2xyz_matrix[i * 3 + 1],
			fw_aad_param->aad_param->aad_lms2xyz_matrix[i * 3 + 2]);
	for (i = 0; i < 3; i++)
		pr_info("rgb2lms_matrix[%d] = %d, %d, %d\n",
			i,
			fw_aad_param->aad_param->aad_rgb2lms_matrix[i * 3],
			fw_aad_param->aad_param->aad_rgb2lms_matrix[i * 3 + 1],
			fw_aad_param->aad_param->aad_rgb2lms_matrix[i * 3 + 2]);
	for (i = 0; i < 3; i++)
		pr_info("lms2rgb_matrix[%d] = %d, %d, %d\n",
			i,
			fw_aad_param->aad_param->aad_lms2rgb_matrix[i * 3],
			fw_aad_param->aad_param->aad_lms2rgb_matrix[i * 3 + 1],
			fw_aad_param->aad_param->aad_lms2rgb_matrix[i * 3 + 2]);
	for (i = 0; i < 17; i++)
		pr_info("aad_LUT_Y_gain[%d] = %d\n",
			i, fw_aad_param->aad_param->aad_LUT_Y_gain[i]);
	for (i = 0; i < 17; i++)
		pr_info("aad_LUT_RG_gain[%d] = %d\n",
			i, fw_aad_param->aad_param->aad_LUT_RG_gain[i]);
	for (i = 0; i < 17; i++)
		pr_info("aad_LUT_BG_gain[%d] = %d\n",
			i, fw_aad_param->aad_param->aad_LUT_BG_gain[i]);
	pr_info("aad_Y_gain_min = %d\n", fw_aad_param->aad_param->aad_Y_gain_min);
	pr_info("aad_Y_gain_max = %d\n", fw_aad_param->aad_param->aad_Y_gain_max);
	pr_info("aad_RG_gain_min = %d\n", fw_aad_param->aad_param->aad_RG_gain_min);
	pr_info("aad_RG_gain_max = %d\n", fw_aad_param->aad_param->aad_RG_gain_max);
	pr_info("aad_BG_gain_min = %d\n", fw_aad_param->aad_param->aad_BG_gain_min);
	pr_info("aad_BG_gain_max = %d\n", fw_aad_param->aad_param->aad_BG_gain_max);
	pr_info("aad_tf_en = %d\n", fw_aad_param->aad_param->aad_tf_en);
	pr_info("aad_tf_alpha = %d\n", fw_aad_param->aad_param->aad_tf_alpha);
	pr_info("aad_dist_mode = %d\n", fw_aad_param->aad_param->aad_dist_mode);
	pr_info("aad_force_gain_en = %d\n",
		fw_aad_param->aad_param->aad_force_gain_en);
	for (i = 0; i < 16; i++)
		pr_info("aad_gain_lut[%d] = %d, %d, %d\n",
		i,
		fw_aad_param->aad_param->aad_gain_lut[i][0],
		fw_aad_param->aad_param->aad_gain_lut[i][1],
		fw_aad_param->aad_param->aad_gain_lut[i][2]);
	for (i = 0; i < 16; i++)
		pr_info("aad_xy_lut[%d] = %d, %d\n",
		i,
		fw_aad_param->aad_param->aad_xy_lut[i][0],
		fw_aad_param->aad_param->aad_xy_lut[i][1]);

	pr_info("aad_r_gain = %d\n", fw_aad_param->aad_param->aad_r_gain);
	pr_info("aad_g_gain = %d\n", fw_aad_param->aad_param->aad_g_gain);
	pr_info("aad_b_gain = %d\n", fw_aad_param->aad_param->aad_b_gain);
	pr_info("aad_min_dist_th = %d\n", fw_aad_param->aad_param->aad_min_dist_th);
	pr_info("sensor_input[0] = %d\n", fw_aad_param->aad_param->sensor_input[0]);
	pr_info("sensor_input[1] = %d\n", fw_aad_param->aad_param->sensor_input[1]);
	pr_info("sensor_input[2] = %d\n", fw_aad_param->aad_param->sensor_input[2]);

	pr_info("\n-----cur aad gain, before temporal filter----\n");
	pr_info("cur_gain[0] = %d\n", fw_aad_param->cur_gain[0]);
	pr_info("cur_gain[1] = %d\n", fw_aad_param->cur_gain[1]);
	pr_info("cur_gain[2] = %d\n", fw_aad_param->cur_gain[2]);

	pr_info("\n--------aad final gain-------\n");
	pr_info("aad_final_gain[0] = %d\n", aad_final_gain[0]);
	pr_info("aad_final_gain[1] = %d\n", aad_final_gain[1]);
	pr_info("aad_final_gain[2] = %d\n", aad_final_gain[2]);

	pr_info("\n aad_debug_mode = %d\n", fw_aad_param->aad_debug_mode);
	pr_info("aad_debug_mode = %d\n", fw_aad_param->aad_debug_mode);

	pr_info("\n--------aad dbg parameters-------\n");
	pr_info("aad_debug_mode = %d\n", fw_aad_param->aad_debug_mode);
	pr_info("y_val = %d\n", fw_aad_param->dbg_param->y_val);
	pr_info("rg_val = %d\n", fw_aad_param->dbg_param->rg_val);
	pr_info("bg_val = %d\n", fw_aad_param->dbg_param->bg_val);
	pr_info("cur_frm_gain = %d\n", fw_aad_param->dbg_param->cur_frm_gain);

	pr_info("aad_alg = %p\n", fw_aad_param->aad_alg);

	return 0;
}

int pre_gamma_alg_state(void)
{
	int i;
	struct pgm_param_s *fw_pregm_param = pregam_fw_param_get();

	pr_info("\n--------pre gamma print-------\n");
	pr_info("fw_pre_gamma_en = %d\n", fw_pregm_param->fw_pre_gamma_en);
	pr_info("pre_gamma_gain_ratio = %d\n",
		fw_pregm_param->pre_gam_param->pre_gamma_gain_ratio);
	for (i = 0; i < 3; i++)
		pr_info("aad_gain[%d] = %d\n", i, fw_pregm_param->aad_gain[i]);
	for (i = 0; i < 3; i++)
		pr_info("cabc_gain[%d] = %d\n", i, fw_pregm_param->cabc_gain[i]);

	pr_info("\n--------out pre_gamma-------\n");
	pr_info("\n--------pre_gamma R-------\n");
	for (i = 0; i < 17; i++) {
		if (i < 16)
			pr_info("pre_gamma R[%2d ~ %2d] = %4d, %4d, %4d, %4d\n",
				i * 4, i * 4 + 3,
				pre_gamma[0][i * 4],
				pre_gamma[0][i * 4 + 1],
				pre_gamma[0][i * 4 + 2],
				pre_gamma[0][i * 4 + 3]);
		else
			pr_info("pre_gamma R[%2d] = %4d\n",
				i * 4, pre_gamma[0][i * 4]);
	}
	pr_info("\n--------pre_gamma G-------\n");
	for (i = 0; i < 17; i++) {
		if (i < 16)
			pr_info("pre_gamma G[%2d ~ %2d] = %4d, %4d, %4d, %4d\n",
				i * 4, i * 4 + 3,
				pre_gamma[1][i * 4],
				pre_gamma[1][i * 4 + 1],
				pre_gamma[1][i * 4 + 2],
				pre_gamma[1][i * 4 + 3]);
		else
			pr_info("pre_gamma G[%2d] = %4d\n",
				i * 4, pre_gamma[1][i * 4]);
	}
	pr_info("\n--------pre_gamma B-------\n");
	for (i = 0; i < 17; i++) {
		if (i < 16)
			pr_info("pre_gamma B[%2d ~ %2d] = %4d, %4d, %4d, %4d\n",
				i * 4, i * 4 + 3,
				pre_gamma[2][i * 4],
				pre_gamma[2][i * 4 + 1],
				pre_gamma[2][i * 4 + 2],
				pre_gamma[2][i * 4 + 3]);
		else
			pr_info("pre_gamma B[%2d] = %4d\n",
				i * 4, pre_gamma[2][i * 4]);
	}
	pr_info("pre_gamma_proc = %p\n", fw_pregm_param->pre_gamma_proc);

	return 0;
}

ssize_t cabc_aad_print(char *buf)
{
	ssize_t len = 0;

	if (debug_cabc_aad == 0x00) {
		cabc_alg_state();
		aad_alg_state();
		pre_gamma_alg_state();
	} else if (debug_cabc_aad >= 0x01 && debug_cabc_aad <= 0x1D) {
		len = debug_aad_alg_state(buf);
	} else if (debug_cabc_aad >= 0x30 && debug_cabc_aad <= 0x4F) {
		len = debug_cabc_alg_state(buf);
	} else if (debug_cabc_aad >= 0x50 && debug_cabc_aad <= 0x52) {
		len = debug_pre_gamma_alg_state(buf);
	}
	debug_cabc_aad = 0;

	return len;
}

static void str_sapr_conv(const char *s, unsigned int size, int *dest, int num)
{
	int i, j;
	char *s1;
	const char *end;
	unsigned int len;
	long value;

	if (size <= 0 || !s)
		return;

	s1 = kmalloc(size + 1, GFP_KERNEL);
	//len = sizeof(s);
	len = size * num;
	end = s;

	j = 0;
	while (len >= size) {
		for (i = 0; i < size; i++)
			s1[i] = end[i];
		s1[size] = '\0';
		if (kstrtoul(s1, 10, &value) < 0)
			break;
		*dest++ = value;
		end = end + size;
		len -= size;
		j++;
		if (j >= num)
			break;
	}
	kfree(s1);
}

int cabc_aad_debug(char **param)
{
	long val;
	struct aad_fw_param_s *fw_aad_param = aad_fw_param_get();
	struct cabc_fw_param_s *fw_cabc_param = cabc_fw_param_get();
	struct pgm_param_s *fw_pregm_param = pregam_fw_param_get();
	int lut[48] = {0};
	int i, j;

	if (!param)
		return -1;

	if (!strcmp(param[0], "enable")) {/*module en/disable*/
		cabc_aad_en = 1;
		backlight_update_ctrl(cabc_aad_en);
		pr_info("cabc_aad_en val:%d\n", cabc_aad_en);
	} else if (!strcmp(param[0], "disable")) {
		cabc_aad_en = 0;
		pr_info("disable aad\n");
	} else if (!strcmp(param[0], "aad_enable")) {/*debug aad*/
		fw_aad_param->fw_aad_en = 1;
		pr_info("enable aad\n");
	} else if (!strcmp(param[0], "aad_disable")) {
		fw_aad_param->fw_aad_en = 0;
		pr_info("disable aad\n");
	} else if (!strcmp(param[0], "cabc_aad_en")) {
		if (!strcmp(param[1], "w")) {
			if (kstrtoul(param[2], 10, &val) < 0)
				goto error;
			cabc_aad_en = val;
			backlight_update_ctrl(cabc_aad_en);
			pr_info("cabc_aad_en val:%d\n", cabc_aad_en);
		} else if (!strcmp(param[1], "r")) {
			debug_cabc_aad = 0x01;
		}
	} else if (!strcmp(param[0], "aad_en")) {
		if (!strcmp(param[1], "w")) {
			if (kstrtoul(param[2], 10, &val) < 0)
				goto error;
			fw_aad_param->fw_aad_en = val;
			pr_info("enable aad\n");
		} else if (!strcmp(param[1], "r")) {
			debug_cabc_aad = 0x02;
		}
	} else if (!strcmp(param[0], "aad_status")) {
		if (!strcmp(param[1], "enable"))
			fw_aad_param->fw_aad_status = 1;
		else if (!strcmp(param[1], "disable"))
			fw_aad_param->fw_aad_status = 0;
		pr_info("aad_status = %d\n", fw_aad_param->fw_aad_status);
	} else if (!strcmp(param[0], "aad_sensor_mode")) {
		if (!strcmp(param[1], "w")) {
			if (!fw_aad_param->aad_param)
				goto error;
			if (kstrtoul(param[2], 10, &val) < 0)
				goto error;
			fw_aad_param->aad_param->aad_sensor_mode = val;
			pr_info("aad_sensor_mode = %d\n", (int)val);
		} else if (!strcmp(param[1], "r")) {
			debug_cabc_aad = 0x05;
		} else {
			if (!fw_aad_param->aad_param)
				goto error;
			if (kstrtoul(param[1], 10, &val) < 0)
				goto error;
			fw_aad_param->aad_param->aad_sensor_mode = val;
			pr_info("aad_sensor_mode = %d\n", (int)val);
		}
	} else if (!strcmp(param[0], "aad_mode")) {
		if (!strcmp(param[1], "w")) {
			if (!fw_aad_param->aad_param)
				goto error;
			if (kstrtoul(param[2], 10, &val) < 0)
				goto error;
			fw_aad_param->aad_param->aad_mode = val;
			pr_info("aad_mode = %d\n", (int)val);
		} else if (!strcmp(param[1], "r")) {
			debug_cabc_aad = 0x06;
		} else {
			if (!fw_aad_param->aad_param)
				goto error;
			if (kstrtoul(param[1], 10, &val) < 0)
				goto error;
			fw_aad_param->aad_param->aad_mode = val;
			pr_info("aad_mode = %d\n", (int)val);
		}
	} else if (!strcmp(param[0], "aad_LUT_Y_gain")) {
		if (!fw_aad_param->aad_param)
			goto error;
		if (!param[1])
			goto error;
		str_sapr_conv(param[1], 5,
			fw_aad_param->aad_param->aad_LUT_Y_gain,
			17);
		pr_info("set aad_LUT_Y_gain\n");
	} else if (!strcmp(param[0], "aad_LUT_RG_gain")) {
		if (!fw_aad_param->aad_param)
			goto error;
		if (!param[1])
			goto error;
		str_sapr_conv(param[1], 5,
			fw_aad_param->aad_param->aad_LUT_RG_gain,
			17);
		pr_info("set aad_LUT_RG_gain\n");
	} else if (!strcmp(param[0], "aad_LUT_BG_gain")) {
		if (!fw_aad_param->aad_param)
			goto error;
		if (!param[1])
			goto error;
		str_sapr_conv(param[1], 5,
			fw_aad_param->aad_param->aad_LUT_BG_gain,
			17);
		pr_info("set aad_LUT_BG_gain\n");
	} else if (!strcmp(param[0], "aad_Y_gain_min")) {
		if (!fw_aad_param->aad_param)
			goto error;
		if (kstrtoul(param[1], 10, &val) < 0)
			goto error;
		fw_aad_param->aad_param->aad_Y_gain_min = val;
		pr_info("aad_Y_gain_min = %d\n", (int)val);
	} else if (!strcmp(param[0], "aad_Y_gain_max")) {
		if (!fw_aad_param->aad_param)
			goto error;
		if (kstrtoul(param[1], 10, &val) < 0)
			goto error;
		fw_aad_param->aad_param->aad_Y_gain_max = val;
		pr_info("aad_Y_gain_max = %d\n", (int)val);
	} else if (!strcmp(param[0], "aad_RG_gain_min")) {
		if (!fw_aad_param->aad_param)
			goto error;
		if (kstrtoul(param[1], 10, &val) < 0)
			goto error;
		fw_aad_param->aad_param->aad_RG_gain_min = val;
		pr_info("aad_RG_gain_min = %d\n", (int)val);
	} else if (!strcmp(param[0], "aad_RG_gain_max")) {
		if (!fw_aad_param->aad_param)
			goto error;
		if (kstrtoul(param[1], 10, &val) < 0)
			goto error;
		fw_aad_param->aad_param->aad_RG_gain_max = val;
		pr_info("aad_RG_gain_max = %d\n", (int)val);
	} else if (!strcmp(param[0], "aad_BG_gain_min")) {
		if (!fw_aad_param->aad_param)
			goto error;
		if (kstrtoul(param[1], 10, &val) < 0)
			goto error;
		fw_aad_param->aad_param->aad_BG_gain_min = val;
		pr_info("aad_BG_gain_min = %d\n", (int)val);
	} else if (!strcmp(param[0], "aad_BG_gain_max")) {
		if (!fw_aad_param->aad_param)
			goto error;
		if (kstrtoul(param[1], 10, &val) < 0)
			goto error;
		fw_aad_param->aad_param->aad_BG_gain_max = val;
		pr_info("aad_BG_gain_max = %d\n", (int)val);
	} else if (!strcmp(param[0], "aad_tf_en")) {
		if (!strcmp(param[1], "w")) {
			if (!fw_aad_param->aad_param)
				goto error;
			if (kstrtoul(param[2], 10, &val) < 0)
				goto error;
			fw_aad_param->aad_param->aad_tf_en = val;
			pr_info("aad_tf_en = %d\n", (int)val);
		} else if (!strcmp(param[1], "r")) {
			debug_cabc_aad = 0x03;
		} else {
			if (!fw_aad_param->aad_param)
				goto error;
			if (kstrtoul(param[1], 10, &val) < 0)
				goto error;
			fw_aad_param->aad_param->aad_tf_en = val;
			pr_info("aad_tf_en = %d\n", (int)val);
		}
	} else if (!strcmp(param[0], "aad_tf_alpha")) {
		if (!strcmp(param[1], "w")) {
			if (!fw_aad_param->aad_param)
				goto error;
			if (kstrtoul(param[2], 10, &val) < 0)
				goto error;
			fw_aad_param->aad_param->aad_tf_alpha = val;
			pr_info("aad_tf_alpha = %d\n", (int)val);
		} else if (!strcmp(param[1], "r")) {
			debug_cabc_aad = 0x08;
		} else {
			if (!fw_aad_param->aad_param)
				goto error;
			if (kstrtoul(param[1], 10, &val) < 0)
				goto error;
			fw_aad_param->aad_param->aad_tf_alpha = val;
			pr_info("aad_tf_alpha = %d\n", (int)val);
		}
	} else if (!strcmp(param[0], "aad_dist_mode")) {
		if (!strcmp(param[1], "w")) {
			if (!fw_aad_param->aad_param)
				goto error;
			if (kstrtoul(param[2], 10, &val) < 0)
				goto error;
			fw_aad_param->aad_param->aad_dist_mode = val;
			pr_info("aad_dist_mode = %d\n", (int)val);
		} else if (!strcmp(param[1], "r")) {
			debug_cabc_aad = 0x07;
		} else {
			if (!fw_aad_param->aad_param)
				goto error;
			if (kstrtoul(param[1], 10, &val) < 0)
				goto error;
			fw_aad_param->aad_param->aad_dist_mode = val;
			pr_info("aad_dist_mode = %d\n", (int)val);
		}
	} else if (!strcmp(param[0], "sensor_rgb")) {
		if (!strcmp(param[1], "w")) {
			if (!fw_aad_param->aad_param)
				goto error;
			if (!param[1])
				goto error;
			str_sapr_conv(param[2], 5,
				fw_aad_param->aad_param->sensor_input,
				3);
			pr_info("set sensor_rgb\n");
		} else if (!strcmp(param[1], "r")) {
			debug_cabc_aad = 0x09;
		} else {
			if (!fw_aad_param->aad_param)
				goto error;
			if (!param[1])
				goto error;
			str_sapr_conv(param[1], 5,
				fw_aad_param->aad_param->sensor_input,
				3);
			pr_info("set sensor_rgb\n");
		}
	} else if (!strcmp(param[0], "aad_force_gain_en")) {
		if (!strcmp(param[1], "w")) {
			if (!fw_aad_param->aad_param)
				goto error;
			if (kstrtoul(param[2], 10, &val) < 0)
				goto error;
			fw_aad_param->aad_param->aad_force_gain_en = val;
			pr_info("aad_force_gain_en = %d\n", (int)val);
		} else if (!strcmp(param[1], "r")) {
			debug_cabc_aad = 0x04;
		} else {
			if (!fw_aad_param->aad_param)
				goto error;
			if (kstrtoul(param[1], 10, &val) < 0)
				goto error;
			fw_aad_param->aad_param->aad_force_gain_en = val;
			pr_info("aad_force_gain_en = %d\n", (int)val);
		}
	} else if (!strcmp(param[0], "aad_rgb_gain")) {
		if (!fw_aad_param->aad_param)
			goto error;
		if (param[3]) {
			if (kstrtoul(param[1], 10, &val) < 0)
				goto error;
			fw_aad_param->aad_param->aad_r_gain = val;
			if (kstrtoul(param[2], 10, &val) < 0)
				goto error;
			fw_aad_param->aad_param->aad_g_gain = val;
			if (kstrtoul(param[3], 10, &val) < 0)
				goto error;
			fw_aad_param->aad_param->aad_b_gain = val;
			pr_info("aad_rgb_gain = %d, %d, %d\n",
				fw_aad_param->aad_param->aad_r_gain,
				fw_aad_param->aad_param->aad_g_gain,
				fw_aad_param->aad_param->aad_b_gain);
		}
	} else if (!strcmp(param[0], "aad_gain_lut")) {
		if (!fw_aad_param->aad_param)
			goto error;
		if (!param[1])
			goto error;
		str_sapr_conv(param[1], 4,
			lut,
			33);
		for (i = 0; i < 11; i++) {
			fw_aad_param->aad_param->aad_gain_lut[i][0] = lut[i * 3];
			fw_aad_param->aad_param->aad_gain_lut[i][1] = lut[i * 3 + 1];
			fw_aad_param->aad_param->aad_gain_lut[i][2] = lut[i * 3 + 2];
		}
		pr_info("set aad_gain_lut\n");
	} else if (!strcmp(param[0], "gain_lut")) {
		if (!fw_aad_param->aad_param)
			goto error;
		if (param[4]) {
			if (kstrtoul(param[1], 10, &val) < 0)
				goto error;
			i = val;
			if (kstrtoul(param[2], 10, &val) < 0)
				goto error;
			fw_aad_param->aad_param->aad_gain_lut[i][0] = val;
			if (kstrtoul(param[3], 10, &val) < 0)
				goto error;
			fw_aad_param->aad_param->aad_gain_lut[i][1] = val;
			if (kstrtoul(param[4], 10, &val) < 0)
				goto error;
			fw_aad_param->aad_param->aad_gain_lut[i][2] = val;
			pr_info("gain_lut[%d] = %d, %d, %d\n",
				i,
				fw_aad_param->aad_param->aad_gain_lut[i][0],
				fw_aad_param->aad_param->aad_gain_lut[i][1],
				fw_aad_param->aad_param->aad_gain_lut[i][2]);
		}
	} else if (!strcmp(param[0], "aad_xy_lut")) {
		if (!fw_aad_param->aad_param)
			goto error;
		if (!param[1])
			goto error;
		str_sapr_conv(param[1], 4,
			lut,
			22);
		for (i = 0; i < 11; i++) {
			fw_aad_param->aad_param->aad_xy_lut[i][0] = lut[i * 2];
			fw_aad_param->aad_param->aad_xy_lut[i][1] = lut[i * 2 + 1];
		}
		pr_info("set aad_xy_lut\n");
	} else if (!strcmp(param[0], "xy_lut")) {
		if (!fw_aad_param->aad_param)
			goto error;
		if (param[3]) {
			if (kstrtoul(param[1], 10, &val) < 0)
				goto error;
			i = val;
			if (kstrtoul(param[2], 10, &val) < 0)
				goto error;
			fw_aad_param->aad_param->aad_xy_lut[i][0] = val;
			if (kstrtoul(param[3], 10, &val) < 0)
				goto error;
			fw_aad_param->aad_param->aad_xy_lut[i][1] = val;
			pr_info("gain_lut[%d] = %d, %d\n",
				i,
				fw_aad_param->aad_param->aad_xy_lut[i][0],
				fw_aad_param->aad_param->aad_xy_lut[i][1]);
		}
	} else if (!strcmp(param[0], "aad_min_dist_th")) {
		if (!fw_aad_param->aad_param)
			goto error;
		if (kstrtoul(param[1], 10, &val) < 0)
			goto error;
		fw_aad_param->aad_param->aad_min_dist_th = val;
		pr_info("aad_min_dist_th = %d\n", (int)val);
	} else if (!strcmp(param[0], "sensor_input")) {
		if (!fw_aad_param->aad_param)
			goto error;
		if (param[3]) {
			if (kstrtoul(param[1], 10, &val) < 0)
				goto error;
			fw_aad_param->aad_param->sensor_input[0] = val;
			if (kstrtoul(param[2], 10, &val) < 0)
				goto error;
			fw_aad_param->aad_param->sensor_input[1] = val;
			if (kstrtoul(param[3], 10, &val) < 0)
				goto error;
			fw_aad_param->aad_param->sensor_input[2] = val;
			pr_info("sensor_input = %d, %d, %d\n",
				fw_aad_param->aad_param->sensor_input[0],
				fw_aad_param->aad_param->sensor_input[1],
				fw_aad_param->aad_param->sensor_input[2]);
		}
	} else if (!strcmp(param[0], "aad_debug_mode")) {
		if (kstrtoul(param[1], 10, &val) < 0)
			goto error;
		fw_aad_param->aad_debug_mode = val;
		pr_info("aad_debug_mode = %d\n", (int)val);
	} else if (!strcmp(param[0], "lut_Y_gain_w_val_str")) {
		if (!strcmp(param[1], "w") || !param[1]) {
			if (!fw_aad_param->aad_param)
				goto error;
			if (!param[1])
				goto error;
			str_sapr_conv(param[2], 5,
				fw_aad_param->aad_param->aad_LUT_Y_gain,
				17);
			pr_info("set lut_Y_gain_wr_val_str\n");
		} else if (!strcmp(param[1], "r")) {
			debug_cabc_aad = 0x0A;
		}
	} else if (!strcmp(param[0], "lut_RG_gain_w_val_str")) {
		if (!strcmp(param[1], "w")) {
			if (!fw_aad_param->aad_param)
				goto error;
			if (!param[2])
				goto error;
			str_sapr_conv(param[2], 5,
				fw_aad_param->aad_param->aad_LUT_RG_gain,
				17);
			pr_info("set lut_RG_gain_w_val_str\n");
		}  else if (!strcmp(param[1], "r")) {
			debug_cabc_aad = 0x0B;
		}
	} else if (!strcmp(param[0], "lut_BG_gain_w_val_str")) {
		if (!strcmp(param[1], "w")) {
			if (!fw_aad_param->aad_param)
				goto error;
			if (!param[2])
				goto error;
			str_sapr_conv(param[2], 5,
				fw_aad_param->aad_param->aad_LUT_BG_gain,
				17);
			pr_info("set lut_BG_gain_w_val_str\n");
		}  else if (!strcmp(param[1], "r")) {
			debug_cabc_aad = 0x0C;
		}
	} else if (!strcmp(param[0], "gain_lut_w_val_str")) {
		int temp[48] = {0};

		if (!strcmp(param[1], "w")) {
			if (!fw_aad_param->aad_param)
				goto error;
			if (!param[2])
				goto error;
			str_sapr_conv(param[2], 5,
				temp,
				48);
			for (i = 0; i < 16; i++)
				for (j = 0; j < 3; j++) {
					fw_aad_param->aad_param->aad_gain_lut[i][j] =
						temp[i * 3 + j];
				}
		}  else if (!strcmp(param[1], "r")) {
			debug_cabc_aad = 0x0D;
		}
	} else if (!strcmp(param[0], "xy_lut_w_val_str")) {
		int temp[32] = {0};

		if (!strcmp(param[1], "w")) {
			if (!fw_aad_param->aad_param)
				goto error;
			if (!param[2])
				goto error;
			str_sapr_conv(param[2], 5,
				temp,
				32);
			for (i = 0; i < 16; i++) {
				for (j = 0; j < 2; j++)
					fw_aad_param->aad_param->aad_xy_lut[i][j] = temp[i * 2 + j];
			}
		}  else if (!strcmp(param[1], "r")) {
			debug_cabc_aad = 0x0E;
		}
	} else if (!strcmp(param[0], "fw_cabc_en")) {/*debug cabc*/
		if (kstrtoul(param[1], 10, &val) < 0)
			goto error;
		fw_cabc_param->fw_cabc_en = val;
		pr_info("fw_cabc_en = %d\n", (int)val);
	} else if (!strcmp(param[0], "cabc_status")) {
		if (!strcmp(param[1], "enable"))
			fw_cabc_param->fw_cabc_status = 1;
		else if (!strcmp(param[1], "disable"))
			fw_cabc_param->fw_cabc_status = 0;
		pr_info("cabc_status = %d\n", fw_cabc_param->fw_cabc_status);
	} else if (!strcmp(param[0], "cabc_max95_ratio")) {
		if (!strcmp(param[1], "w")) {
			if (!fw_cabc_param->cabc_param)
				goto error;
			if (kstrtoul(param[2], 10, &val) < 0)
				goto error;
			fw_cabc_param->cabc_param->cabc_max95_ratio = val;
			pr_info("cabc_max95_ratio = %d\n", (int)val);
		} else if (!strcmp(param[1], "r")) {
			debug_cabc_aad = 0x37;
		} else {
			if (!fw_cabc_param->cabc_param)
				goto error;
			if (kstrtoul(param[1], 10, &val) < 0)
				goto error;
			fw_cabc_param->cabc_param->cabc_max95_ratio = val;
			pr_info("cabc_max95_ratio = %d\n", (int)val);
		}
	} else if (!strcmp(param[0], "cabc_hist_mode")) {
		if (!strcmp(param[1], "w")) {
			if (!fw_cabc_param->cabc_param)
				goto error;
			if (kstrtoul(param[2], 10, &val) < 0)
				goto error;
			fw_cabc_param->cabc_param->cabc_hist_mode = val;
			pr_info("cabc_hist_mode = %d\n", (int)val);
		} else if (!strcmp(param[1], "r")) {
			debug_cabc_aad = 0x31;
		} else {
			if (!fw_cabc_param->cabc_param)
				goto error;
			if (kstrtoul(param[1], 10, &val) < 0)
				goto error;
			fw_cabc_param->cabc_param->cabc_hist_mode = val;
			pr_info("cabc_hist_mode = %d\n", (int)val);
		}
	} else if (!strcmp(param[0], "cabc_hist_blend_alpha")) {
		if (!strcmp(param[1], "w")) {
			if (!fw_cabc_param->cabc_param)
				goto error;
			if (kstrtoul(param[2], 10, &val) < 0)
				goto error;
			fw_cabc_param->cabc_param->cabc_hist_blend_alpha = val;
			pr_info("cabc_hist_blend_alpha = %d\n", (int)val);
		} else if (!strcmp(param[1], "r")) {
			debug_cabc_aad = 0x38;
		} else {
			if (!fw_cabc_param->cabc_param)
				goto error;
			if (kstrtoul(param[1], 10, &val) < 0)
				goto error;
			fw_cabc_param->cabc_param->cabc_hist_blend_alpha = val;
			pr_info("cabc_hist_blend_alpha = %d\n", (int)val);
		}
	} else if (!strcmp(param[0], "cabc_init_bl_min")) {
		if (!strcmp(param[1], "w")) {
			if (!fw_cabc_param->cabc_param)
				goto error;
			if (kstrtoul(param[2], 10, &val) < 0)
				goto error;
			fw_cabc_param->cabc_param->cabc_init_bl_min = val;
			pr_info("cabc_init_bl_min = %d\n", (int)val);
		} else if (!strcmp(param[1], "r")) {
			debug_cabc_aad = 0x39;
		} else {
			if (!fw_cabc_param->cabc_param)
				goto error;
			if (kstrtoul(param[1], 10, &val) < 0)
				goto error;
			fw_cabc_param->cabc_param->cabc_init_bl_min = val;
			pr_info("cabc_init_bl_min = %d\n", (int)val);
		}
	} else if (!strcmp(param[0], "cabc_init_bl_max")) {
		if (!strcmp(param[1], "w")) {
			if (!fw_cabc_param->cabc_param)
				goto error;
			if (kstrtoul(param[2], 10, &val) < 0)
				goto error;
			fw_cabc_param->cabc_param->cabc_init_bl_max = val;
			pr_info("cabc_init_bl_max = %d\n", (int)val);
		} else if (!strcmp(param[1], "r")) {
			debug_cabc_aad = 0x3A;
		} else {
			if (!fw_cabc_param->cabc_param)
				goto error;
			if (kstrtoul(param[1], 10, &val) < 0)
				goto error;
			fw_cabc_param->cabc_param->cabc_init_bl_max = val;
			pr_info("cabc_init_bl_max = %d\n", (int)val);
		}
	} else if (!strcmp(param[0], "cabc_tf_alpha")) {
		if (!strcmp(param[1], "w")) {
			if (!fw_cabc_param->cabc_param)
				goto error;
			if (kstrtoul(param[2], 10, &val) < 0)
				goto error;
			fw_cabc_param->cabc_param->cabc_tf_alpha = val;
			pr_info("cabc_tf_alpha = %d\n", (int)val);
		} else if (!strcmp(param[1], "r")) {
			debug_cabc_aad = 0x3B;
		} else {
			if (!fw_cabc_param->cabc_param)
				goto error;
			if (kstrtoul(param[1], 10, &val) < 0)
				goto error;
			fw_cabc_param->cabc_param->cabc_tf_alpha = val;
			pr_info("cabc_tf_alpha = %d\n", (int)val);
		}
	} else if (!strcmp(param[0], "cabc_tf_en")) {
		if (!strcmp(param[1], "w")) {
			if (!fw_cabc_param->cabc_param)
				goto error;
			if (kstrtoul(param[2], 10, &val) < 0)
				goto error;
			fw_cabc_param->cabc_param->cabc_tf_en = val;
			pr_info("cabc_tf_en = %d\n", (int)val);
		} else if (!strcmp(param[1], "r")) {
			debug_cabc_aad = 0x32;
		} else {
			if (!fw_cabc_param->cabc_param)
				goto error;
			if (kstrtoul(param[1], 10, &val) < 0)
				goto error;
			fw_cabc_param->cabc_param->cabc_tf_en = val;
			pr_info("cabc_tf_en = %d\n", (int)val);
		}
	} else if (!strcmp(param[0], "cabc_sc_flag")) {
		if (!strcmp(param[1], "w")) {
			if (!fw_cabc_param->cabc_param)
				goto error;
			if (kstrtoul(param[2], 10, &val) < 0)
				goto error;
			fw_cabc_param->cabc_param->cabc_sc_flag = val;
			pr_info("cabc_sc_flag = %d\n", (int)val);
		} else if (!strcmp(param[1], "r")) {
			debug_cabc_aad = 0x33;
		} else {
			if (!fw_cabc_param->cabc_param)
				goto error;
			if (kstrtoul(param[1], 10, &val) < 0)
				goto error;
			fw_cabc_param->cabc_param->cabc_sc_flag = val;
			pr_info("cabc_sc_flag = %d\n", (int)val);
		}
	} else if (!strcmp(param[0], "cabc_sc_hist_diff_thd")) {
		if (!strcmp(param[1], "w")) {
			if (!fw_cabc_param->cabc_param)
				goto error;
			if (kstrtoul(param[2], 10, &val) < 0)
				goto error;
			fw_cabc_param->cabc_param->cabc_sc_hist_diff_thd = val;
			pr_info("cabc_sc_hist_diff_thd = %d\n", (int)val);
		} else if (!strcmp(param[1], "r")) {
			debug_cabc_aad = 0x3C;
		} else {
			if (!fw_cabc_param->cabc_param)
				goto error;
			if (kstrtoul(param[1], 10, &val) < 0)
				goto error;
			fw_cabc_param->cabc_param->cabc_sc_flag = val;
			pr_info("cabc_sc_flag = %d\n", (int)val);
		}
	} else if (!strcmp(param[0], "cabc_sc_apl_diff_thd")) {
		if (!strcmp(param[1], "w")) {
			if (!fw_cabc_param->cabc_param)
				goto error;
			if (kstrtoul(param[2], 10, &val) < 0)
				goto error;
			fw_cabc_param->cabc_param->cabc_sc_apl_diff_thd = val;
			pr_info("cabc_sc_apl_diff_thd = %d\n", (int)val);
		} else if (!strcmp(param[1], "r")) {
			debug_cabc_aad = 0x3D;
		} else {
			if (!fw_cabc_param->cabc_param)
				goto error;
			if (kstrtoul(param[1], 10, &val) < 0)
				goto error;
			fw_cabc_param->cabc_param->cabc_sc_apl_diff_thd = val;
			pr_info("cabc_sc_apl_diff_thd = %d\n", (int)val);
		}
	} else if (!strcmp(param[0], "cabc_en")) {
		if ((!strcmp(param[1], "w"))) {
			if (!fw_cabc_param->cabc_param)
				goto error;
			if (kstrtoul(param[2], 10, &val) < 0)
				goto error;
			fw_cabc_param->cabc_param->cabc_en = val;
			pr_info("cabc_en = %d\n", (int)val);
		} else if (!strcmp(param[1], "r")) {
			debug_cabc_aad = 0x30;
		} else {
			if (!fw_cabc_param->cabc_param)
				goto error;
			if (kstrtoul(param[1], 10, &val) < 0)
				goto error;
			fw_cabc_param->cabc_param->cabc_en = val;
			pr_info("cabc_en = %d\n", (int)val);
		}
	} else if (!strcmp(param[0], "cabc_bl_map_mode")) {
		if (!strcmp(param[1], "w")) {
			if (!fw_cabc_param->cabc_param)
				goto error;
			if (kstrtoul(param[2], 10, &val) < 0)
				goto error;
			fw_cabc_param->cabc_param->cabc_bl_map_mode = val;
			pr_info("cabc_bl_map_mode = %d\n", (int)val);
		} else if (!strcmp(param[1], "r")) {
			debug_cabc_aad = 0x34;
		} else {
			if (!fw_cabc_param->cabc_param)
				goto error;
			if (kstrtoul(param[1], 10, &val) < 0)
				goto error;
			fw_cabc_param->cabc_param->cabc_bl_map_mode = val;
			pr_info("cabc_bl_map_mode = %d\n", (int)val);
		}
	} else if (!strcmp(param[0], "cabc_bl_map_en")) {
		if (!strcmp(param[1], "w")) {
			if (!fw_cabc_param->cabc_param)
				goto error;
			if (kstrtoul(param[2], 10, &val) < 0)
				goto error;
			fw_cabc_param->cabc_param->cabc_bl_map_en = val;
			pr_info("cabc_bl_map_en = %d\n", (int)val);
		} else if (!strcmp(param[1], "r")) {
			debug_cabc_aad = 0x35;
		} else {
			if (!fw_cabc_param->cabc_param)
				goto error;
			if (kstrtoul(param[1], 10, &val) < 0)
				goto error;
			fw_cabc_param->cabc_param->cabc_bl_map_en = val;
			pr_info("cabc_bl_map_en = %d\n", (int)val);
		}
	} else if (!strcmp(param[0], "o_bl_cv")) {
		if (!fw_cabc_param->cabc_param)
			goto error;
		if (param[2]) {
			if (kstrtoul(param[1], 10, &val) < 0)
				goto error;
			i = (int)val;
			if (i >= 13)
				goto error;
			if (kstrtoul(param[2], 10, &val) < 0)
				goto error;
			fw_cabc_param->cabc_param->o_bl_cv[i] = (int)val;
			pr_info("o_bl_cv[%d] = %d\n",
				i, fw_cabc_param->cabc_param->o_bl_cv[i]);
		}
	} else if (!strcmp(param[0], "maxbin_bl_cv")) {
		if (!fw_cabc_param->cabc_param)
			goto error;
		if (param[2]) {
			if (kstrtoul(param[1], 10, &val) < 0)
				goto error;
			i = (int)val;
			if (i >= 13)
				goto error;
			if (kstrtoul(param[2], 10, &val) < 0)
				goto error;
			fw_cabc_param->cabc_param->maxbin_bl_cv[i] = (int)val;
			pr_info("maxbin_bl_cv[%d] = %d\n",
				i, fw_cabc_param->cabc_param->maxbin_bl_cv[i]);
		}
	} else if (!strcmp(param[0], "o_bl_cv_w_val_str")) {
		if (!strcmp(param[1], "w")) {
			if (!fw_cabc_param->cabc_param)
				goto error;
			if (!param[2])
				goto error;
			str_sapr_conv(param[2], 5,
				fw_cabc_param->cabc_param->o_bl_cv,
				13);
			pr_info("o_bl_cv_w_val_str\n");
		} else if (!strcmp(param[1], "r")) {
			debug_cabc_aad = 0x42;
		}
	} else if (!strcmp(param[0], "maxbin_bl_cv_w_val_str")) {
		if (!strcmp(param[1], "w")) {
			if (!fw_cabc_param->cabc_param)
				goto error;
			if (!param[2])
				goto error;
			str_sapr_conv(param[2], 5,
				fw_cabc_param->cabc_param->maxbin_bl_cv,
				13);
			pr_info("maxbin_bl_cv_w_val_str\n");
		} else if (!strcmp(param[1], "r")) {
			debug_cabc_aad = 0x43;
		}
	} else if (!strcmp(param[0], "cabc_patch_bl_th")) {
		if (!strcmp(param[1], "w")) {
			if (!fw_cabc_param->cabc_param)
				goto error;
			if (kstrtoul(param[2], 10, &val) < 0)
				goto error;
			fw_cabc_param->cabc_param->cabc_patch_bl_th = val;
			pr_info("cabc_patch_bl_th = %d\n", (int)val);
		} else if (!strcmp(param[1], "r")) {
			debug_cabc_aad = 0x3E;
		} else {
			if (!fw_cabc_param->cabc_param)
				goto error;
			if (kstrtoul(param[1], 10, &val) < 0)
				goto error;
			fw_cabc_param->cabc_param->cabc_patch_bl_th = val;
			pr_info("cabc_patch_bl_th = %d\n", (int)val);
		}
	} else if (!strcmp(param[0], "cabc_patch_on_alpha")) {
		if (!strcmp(param[1], "w")) {
			if (!fw_cabc_param->cabc_param)
				goto error;
			if (kstrtoul(param[2], 10, &val) < 0)
				goto error;
			fw_cabc_param->cabc_param->cabc_patch_on_alpha = val;
			pr_info("cabc_patch_on_alpha = %d\n", (int)val);
		} else if (!strcmp(param[1], "r")) {
			debug_cabc_aad = 0x3F;
		} else {
			if (!fw_cabc_param->cabc_param)
				goto error;
			if (kstrtoul(param[1], 10, &val) < 0)
				goto error;
			fw_cabc_param->cabc_param->cabc_patch_on_alpha = val;
			pr_info("cabc_patch_on_alpha = %d\n", (int)val);
		}
	} else if (!strcmp(param[0], "cabc_patch_bl_off_th")) {
		if (!strcmp(param[1], "w")) {
			if (!fw_cabc_param->cabc_param)
				goto error;
			if (kstrtoul(param[2], 10, &val) < 0)
				goto error;
			fw_cabc_param->cabc_param->cabc_patch_bl_off_th = val;
			pr_info("cabc_patch_bl_off_th = %d\n", (int)val);
		} else if (!strcmp(param[1], "r")) {
			debug_cabc_aad = 0x40;
		} else {
			if (!fw_cabc_param->cabc_param)
				goto error;
			if (kstrtoul(param[1], 10, &val) < 0)
				goto error;
			fw_cabc_param->cabc_param->cabc_patch_bl_off_th = val;
			pr_info("cabc_patch_bl_off_th = %d\n", (int)val);
		}
	} else if (!strcmp(param[0], "cabc_patch_off_alpha")) {
		if (!strcmp(param[1], "w")) {
			if (!fw_cabc_param->cabc_param)
				goto error;
			if (kstrtoul(param[2], 10, &val) < 0)
				goto error;
			fw_cabc_param->cabc_param->cabc_patch_off_alpha = val;
			pr_info("cabc_patch_off_alpha = %d\n", (int)val);
		} else if (!strcmp(param[1], "r")) {
			debug_cabc_aad = 0x41;
			pr_info("store debug_cabc_aad = 0x41\n");
		} else {
			if (!fw_cabc_param->cabc_param)
				goto error;
			if (kstrtoul(param[1], 10, &val) < 0)
				goto error;
			fw_cabc_param->cabc_param->cabc_patch_off_alpha = val;
			pr_info("cabc_patch_off_alpha = %d\n", (int)val);
		}
	} else if (!strcmp(param[0], "cabc_temp_proc")) {
		if (!strcmp(param[1], "w")) {
			if (!fw_cabc_param->cabc_param)
				goto error;
			if (kstrtoul(param[2], 10, &val) < 0)
				goto error;
			fw_cabc_param->cabc_param->cabc_temp_proc = val;
			pr_info("cabc_temp_proc = %d\n", (int)val);
		} else if (!strcmp(param[1], "r")) {
			debug_cabc_aad = 0x36;
		} else {
			if (!fw_cabc_param->cabc_param)
				goto error;
			if (kstrtoul(param[1], 10, &val) < 0)
				goto error;
			fw_cabc_param->cabc_param->cabc_temp_proc = val;
			pr_info("cabc_temp_proc = %d\n", (int)val);
		}
	} else if (!strcmp(param[0], "cabc_debug_mode")) {
		if (kstrtoul(param[1], 10, &val) < 0)
			goto error;
		fw_cabc_param->cabc_debug_mode = val;
		pr_info("cabc_debug_mode = %d\n", (int)val);
	} else if (!strcmp(param[0], "pre_gamma_en")) {/*debug pre gma*/
		if (kstrtoul(param[1], 10, &val) < 0)
			goto error;
		fw_pregm_param->fw_pre_gamma_en = val;
		pr_info("fw_pre_gamma_en = %d\n", (int)val);
	} else if (!strcmp(param[0], "pre_gamma_gain_ratio")) {
		if (!fw_pregm_param->pre_gam_param)
			goto error;
		if (kstrtoul(param[1], 10, &val) < 0)
			goto error;
		fw_pregm_param->pre_gam_param->pre_gamma_gain_ratio = val;
		pr_info("pre_gamma_gain_ratio = %d\n", (int)val);
	} else if (!strcmp(param[0], "aad_print")) {/*print state*/
		aad_alg_state();
	} else if (!strcmp(param[0], "cabc_print")) {
		cabc_alg_state();
	} else if (!strcmp(param[0], "pre_gamma_print")) {
		pre_gamma_alg_state();
	}

	return 0;
error:
	return -1;
}
