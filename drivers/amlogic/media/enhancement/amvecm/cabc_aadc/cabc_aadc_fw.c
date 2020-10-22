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
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/workqueue.h>
#include <linux/amlogic/media/amvecm/cabc_addc_alg.h>
#include <linux/amlogic/media/amvecm/amvecm.h>
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
static char update_ver[32] = "no alg";
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
	.cabc_tf_en = 0,
	.cabc_sc_flag = 1,
	.cabc_sc_hist_diff_thd = 1920 * 1080 / 3,
	.cabc_sc_apl_diff_thd = 24 << (LED_BL_BW - PSTHIST_NRM),
	.cabc_en = 1,
};

static struct pre_gam_param_s pre_gam_parm = {
	.pre_gamma_gain_ratio = 16,
};

static struct debug_param_s dbg_parm = {
	.cabc_gain = 0,
	.avg = 0,
	.max95 = 0,
	.tf_bl = 0,
	.y_val = 0,
	.rg_val = 0,
	.bg_val = 0,
	.cur_frm_gain = 0,
};

struct cabc_aad_fw_param_s fw_parm = {
	.fw_en = 0,
	.fw_aad_en = 0,
	.fw_status = 0,
	.fw_ver = 0,
	.fw_update_ver = update_ver,
	.aad_param = &aad_parm,
	.cabc_param = &cabc_parm,
	.pre_gam_param = &pre_gam_parm,
	.i_hist = hist,
	.o_pre_gamma = pre_gamma,
	.tgt_bl = 0,
	.debug_mode = 0,
	.dbg_param = &dbg_parm,
	.cabc_aad_alg = NULL,
};

static int pre_gamma_init(void)
{
	int i, j;

	for (i = 0; i < 3; i++) {
		for (j = 0; j < 64; j++)
			pre_gamma[i][j] = j << 4;
		pre_gamma[i][64] = 1023;
	}
	return 0;
}

static void pre_gamma_set(struct pre_gamma_table_s *pre_gam)
{
	memcpy(pre_gam->lut_r, pre_gamma[0], sizeof(int) * 65);
	memcpy(pre_gam->lut_g, pre_gamma[1], sizeof(int) * 65);
	memcpy(pre_gam->lut_b, pre_gamma[2], sizeof(int) * 65);
}

struct cabc_aad_fw_param_s *cabc_aad_fw_param_get(void)
{
	return &fw_parm;
}
EXPORT_SYMBOL(cabc_aad_fw_param_get);

int *vf_param_get(void)
{
	return fw_parm.i_hist;
}

int fw_en_get(void)
{
	return fw_parm.fw_en;
}

#define FW_ENABLE  1
#define FW_DISABLE  0
void aml_cabc_alg_process(struct work_struct *work)
{
	if (fw_parm.fw_en && !fw_parm.fw_status) {
		fw_parm.fw_status = FW_ENABLE;
		pre_gam.en = fw_parm.fw_en;
		pr_cabc_aad_dbg("%s: fw_en = %d\n", __func__, fw_parm.fw_en);
	}

	if (!fw_parm.cabc_aad_alg) {
		pr_cabc_aad_dbg("%s: cabc aad alg func is NULL\n", __func__);
		return;
	}

	if (fw_parm.fw_en) {
		fw_parm.cabc_aad_alg(&fw_parm);
		pre_gamma_set(&pre_gam);
		set_pre_gamma_reg(&pre_gam);
	}
}

void aml_cabc_alg_bypass(struct work_struct *work)
{
	if (!fw_parm.fw_en && fw_parm.fw_status) {
		fw_parm.fw_status = FW_DISABLE;
		pre_gam.en = fw_parm.fw_en;
		pre_gamma_init();
		pre_gamma_set(&pre_gam);
		set_pre_gamma_reg(&pre_gam);
		pr_cabc_aad_dbg("%s: fw_en = %d\n", __func__, fw_parm.fw_en);
	}
}

int cabc_aad_alg_state(void)
{
	int i;
	struct cabc_aad_fw_param_s *fw_param = cabc_aad_fw_param_get();

	pr_info("fw_en = %d\n", fw_param->fw_en);
	pr_info("fw_aad_en = %d\n", fw_param->fw_aad_en);
	pr_info("fw_status = %d\n", fw_param->fw_status);
	pr_info("fw_ver = %d\n", fw_param->fw_ver);
	pr_info("fw_ver = %s\n", fw_param->fw_update_ver);

	pr_info("\n--------fw aad parameters-------\n");
	pr_info("aad_sensor_mode = %d\n", fw_param->aad_param->aad_sensor_mode);
	pr_info("aad_mode = %d\n", fw_param->aad_param->aad_mode);
	for (i = 0; i < 3; i++)
		pr_info("xyz2rgb_matrix[%d] = %d, %d, %d\n",
			i,
			fw_param->aad_param->aad_xyz2rgb_matrix[i * 3],
			fw_param->aad_param->aad_xyz2rgb_matrix[i * 3 + 1],
			fw_param->aad_param->aad_xyz2rgb_matrix[i * 3 + 2]);
	for (i = 0; i < 3; i++)
		pr_info("rgb2xyz_matrix[%d] = %d, %d, %d\n",
			i,
			fw_param->aad_param->aad_rgb2xyz_matrix[i * 3],
			fw_param->aad_param->aad_rgb2xyz_matrix[i * 3 + 1],
			fw_param->aad_param->aad_rgb2xyz_matrix[i * 3 + 2]);
	for (i = 0; i < 3; i++)
		pr_info("xyz2lms_matrix[%d] = %d, %d, %d\n",
			i,
			fw_param->aad_param->aad_xyz2lms_matrix[i * 3],
			fw_param->aad_param->aad_xyz2lms_matrix[i * 3 + 1],
			fw_param->aad_param->aad_xyz2lms_matrix[i * 3 + 2]);
	for (i = 0; i < 3; i++)
		pr_info("lms2xyz_matrix[%d] = %d, %d, %d\n",
			i,
			fw_param->aad_param->aad_lms2xyz_matrix[i * 3],
			fw_param->aad_param->aad_lms2xyz_matrix[i * 3 + 1],
			fw_param->aad_param->aad_lms2xyz_matrix[i * 3 + 2]);
	for (i = 0; i < 3; i++)
		pr_info("rgb2lms_matrix[%d] = %d, %d, %d\n",
			i,
			fw_param->aad_param->aad_rgb2lms_matrix[i * 3],
			fw_param->aad_param->aad_rgb2lms_matrix[i * 3 + 1],
			fw_param->aad_param->aad_rgb2lms_matrix[i * 3 + 2]);
	for (i = 0; i < 3; i++)
		pr_info("lms2rgb_matrix[%d] = %d, %d, %d\n",
			i,
			fw_param->aad_param->aad_lms2rgb_matrix[i * 3],
			fw_param->aad_param->aad_lms2rgb_matrix[i * 3 + 1],
			fw_param->aad_param->aad_lms2rgb_matrix[i * 3 + 2]);
	for (i = 0; i < 17; i++)
		pr_info("aad_LUT_Y_gain[%d] = %d\n",
			i, fw_param->aad_param->aad_LUT_Y_gain[i]);
	for (i = 0; i < 17; i++)
		pr_info("aad_LUT_RG_gain[%d] = %d\n",
			i, fw_param->aad_param->aad_LUT_RG_gain[i]);
	for (i = 0; i < 17; i++)
		pr_info("aad_LUT_BG_gain[%d] = %d\n",
			i, fw_param->aad_param->aad_LUT_BG_gain[i]);
	pr_info("aad_Y_gain_min = %d\n", fw_param->aad_param->aad_Y_gain_min);
	pr_info("aad_Y_gain_max = %d\n", fw_param->aad_param->aad_Y_gain_max);
	pr_info("aad_RG_gain_min = %d\n", fw_param->aad_param->aad_RG_gain_min);
	pr_info("aad_RG_gain_max = %d\n", fw_param->aad_param->aad_RG_gain_max);
	pr_info("aad_BG_gain_min = %d\n", fw_param->aad_param->aad_BG_gain_min);
	pr_info("aad_BG_gain_max = %d\n", fw_param->aad_param->aad_BG_gain_max);
	pr_info("aad_tf_en = %d\n", fw_param->aad_param->aad_tf_en);
	pr_info("aad_tf_alpha = %d\n", fw_param->aad_param->aad_tf_alpha);
	pr_info("aad_dist_mode = %d\n", fw_param->aad_param->aad_dist_mode);
	pr_info("aad_force_gain_en = %d\n",
		fw_param->aad_param->aad_force_gain_en);
	for (i = 0; i < 16; i++)
		pr_info("aad_gain_lut[%d] = %d, %d, %d\n",
			i,
		fw_param->aad_param->aad_gain_lut[i][0],
		fw_param->aad_param->aad_gain_lut[i][1],
		fw_param->aad_param->aad_gain_lut[i][2]);
	for (i = 0; i < 16; i++)
		pr_info("aad_xy_lut[%d] = %d, %d\n",
			i,
		fw_param->aad_param->aad_xy_lut[i][0],
		fw_param->aad_param->aad_xy_lut[i][1]);
	pr_info("aad_r_gain = %d\n", fw_param->aad_param->aad_r_gain);
	pr_info("aad_g_gain = %d\n", fw_param->aad_param->aad_g_gain);
	pr_info("aad_b_gain = %d\n", fw_param->aad_param->aad_b_gain);
	pr_info("aad_min_dist_th = %d\n", fw_param->aad_param->aad_min_dist_th);
	pr_info("sensor_input[0] = %d\n", fw_param->aad_param->sensor_input[0]);
	pr_info("sensor_input[1] = %d\n", fw_param->aad_param->sensor_input[1]);
	pr_info("sensor_input[2] = %d\n", fw_param->aad_param->sensor_input[2]);

	pr_info("\n--------fw cabc parameters-------\n");
	pr_info("cabc_max95_ratio = %d\n", fw_param->cabc_param->cabc_max95_ratio);
	pr_info("cabc_hist_mode = %d\n", fw_param->cabc_param->cabc_hist_mode);
	pr_info("cabc_hist_blend_alpha = %d\n",
		fw_param->cabc_param->cabc_hist_blend_alpha);
	pr_info("cabc_init_bl_min = %d\n",
		fw_param->cabc_param->cabc_init_bl_min);
	pr_info("cabc_init_bl_max = %d\n",
		fw_param->cabc_param->cabc_init_bl_max);
	pr_info("cabc_tf_alpha = %d\n",
		fw_param->cabc_param->cabc_tf_alpha);
	pr_info("cabc_tf_en = %d\n", fw_param->cabc_param->cabc_tf_en);
	pr_info("cabc_sc_flag = %d\n", fw_param->cabc_param->cabc_sc_flag);
	pr_info("cabc_sc_hist_diff_thd = %d\n",
		fw_param->cabc_param->cabc_sc_hist_diff_thd);
	pr_info("cabc_sc_apl_diff_thd = %d\n",
		fw_param->cabc_param->cabc_sc_apl_diff_thd);
	pr_info("cabc_en = %d\n",
		fw_param->cabc_param->cabc_en);
	pr_info("\n--------fw pre gamma parameters-------\n");
	pr_info("pre_gma_ratio = %d\n",
		fw_param->pre_gam_param->pre_gamma_gain_ratio);

	pr_info("\n--------fw vpp y hist-------\n");
	if (fw_param->i_hist) {
		for (i = 0; i < 16; i++)
			pr_info("i_hist[%2d ~ %2d] = 0x%5x, 0x%5x, 0x%5x, 0x%5x\n",
				i * 4, i * 4 + 3,
				fw_param->i_hist[i * 4],
				fw_param->i_hist[i * 4 + 1],
				fw_param->i_hist[i * 4 + 2],
				fw_param->i_hist[i * 4 + 3]);
	}

	pr_info("\n--------out pre_gamma-------\n");
	if (fw_param->o_pre_gamma) {
		pr_info("\n--------pre_gamma R-------\n");
		for (i = 0; i < 17; i++) {
			if (i < 16)
				pr_info("pre_gamma R[%2d ~ %2d] = %4d, %4d, %4d, %4d\n",
					i * 4, i * 4 + 3,
					fw_param->o_pre_gamma[0][i * 4],
					fw_param->o_pre_gamma[0][i * 4 + 1],
					fw_param->o_pre_gamma[0][i * 4 + 2],
					fw_param->o_pre_gamma[0][i * 4 + 3]);
			else
				pr_info("pre_gamma R[%2d] = %4d\n",
					i * 4, fw_param->o_pre_gamma[0][i * 4]);
		}
		pr_info("\n--------pre_gamma G-------\n");
		for (i = 0; i < 17; i++) {
			if (i < 16)
				pr_info("pre_gamma G[%2d ~ %2d] = %4d, %4d, %4d, %4d\n",
					i * 4, i * 4 + 3,
					fw_param->o_pre_gamma[1][i * 4],
					fw_param->o_pre_gamma[1][i * 4 + 1],
					fw_param->o_pre_gamma[1][i * 4 + 2],
					fw_param->o_pre_gamma[1][i * 4 + 3]);
			else
				pr_info("pre_gamma G[%2d] = %4d\n",
					i * 4, fw_param->o_pre_gamma[1][i * 4]);
		}
		pr_info("\n--------pre_gamma B-------\n");
		for (i = 0; i < 17; i++) {
			if (i < 16)
				pr_info("pre_gamma B[%2d ~ %2d] = %4d, %4d, %4d, %4d\n",
					i * 4, i * 4 + 3,
					fw_param->o_pre_gamma[2][i * 4],
					fw_param->o_pre_gamma[2][i * 4 + 1],
					fw_param->o_pre_gamma[2][i * 4 + 2],
					fw_param->o_pre_gamma[2][i * 4 + 3]);
			else
				pr_info("pre_gamma B[%2d] = %4d\n",
					i * 4, fw_param->o_pre_gamma[2][i * 4]);
		}
	}

	pr_info("\n--------target backlight-------\n");
	pr_info("tgt_bl = %d\n", fw_param->tgt_bl);

	pr_info("\n--------debug parameters-------\n");
	pr_info("debug_mode = %d\n", fw_param->debug_mode);
	pr_info("cabc_gain = %d\n", fw_param->dbg_param->cabc_gain);
	pr_info("avg = %d\n", fw_param->dbg_param->avg);
	pr_info("max95 = %d\n", fw_param->dbg_param->max95);
	pr_info("tf_bl = %d\n", fw_param->dbg_param->tf_bl);
	pr_info("aad_gain = %d\n", fw_param->dbg_param->aad_gain);
	pr_info("y_val = %d\n", fw_param->dbg_param->y_val);
	pr_info("rg_val = %d\n", fw_param->dbg_param->rg_val);
	pr_info("bg_val = %d\n", fw_param->dbg_param->bg_val);
	pr_info("cur_frm_gain = %d\n", fw_param->dbg_param->cur_frm_gain);

	return 0;
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
	len = sizeof(s);
	end = s;

	j = 0;
	while (len >= size) {
		for (i = 0; i < size; i++)
			s1[i] = end[i];
		s1[size] = '\0';
		if (kstrtoul(s1, 10, &value) < 0)
			break;
		*dest++ = value;
		end = s + size;
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
	struct cabc_aad_fw_param_s *fw_param = cabc_aad_fw_param_get();
	int lut[48] = {0};
	int i;

	if (!param)
		return -1;

	if (!strcmp(param[0], "enable")) {
		fw_param->fw_en = 1;
		pr_info("enable cabc aad\n");
	} else if (!strcmp(param[0], "disable")) {
		fw_param->fw_en = 0;
		pr_info("disable cabc aad\n");
	} else if (!strcmp(param[0], "aad_enable")) {
		fw_param->fw_aad_en = 1;
		pr_info("enable aad\n");
	} else if (!strcmp(param[0], "aad_disable")) {
		fw_param->fw_aad_en = 0;
		pr_info("disable aad\n");
	} else if (!strcmp(param[0], "status")) {
		if (!strcmp(param[1], "enable"))
			fw_param->fw_status = 1;
		else if (!strcmp(param[1], "disable"))
			fw_param->fw_status = 0;
		pr_info("fw_status = %d\n", fw_param->fw_status);
	} else if (!strcmp(param[0], "aad_sensor_mode")) {/*debug aad*/
		if (!fw_param->aad_param)
			goto error;
		if (kstrtoul(param[1], 10, &val) < 0)
			goto error;
		fw_param->aad_param->aad_sensor_mode = val;
		pr_info("aad_sensor_mode = %d\n", (int)val);
	} else if (!strcmp(param[0], "aad_mode")) {
		if (!fw_param->aad_param)
			goto error;
		if (kstrtoul(param[1], 10, &val) < 0)
			goto error;
		fw_param->aad_param->aad_mode = val;
		pr_info("aad_mode = %d\n", (int)val);
	} else if (!strcmp(param[0], "aad_LUT_Y_gain")) {
		if (!fw_param->aad_param)
			goto error;
		if (!param[1])
			goto error;
		str_sapr_conv(param[1], 5,
			      fw_param->aad_param->aad_LUT_Y_gain,
			      17);
		pr_info("set aad_LUT_Y_gain\n");
	} else if (!strcmp(param[0], "aad_LUT_RG_gain")) {
		if (!fw_param->aad_param)
			goto error;
		if (!param[1])
			goto error;
		str_sapr_conv(param[1], 5,
			      fw_param->aad_param->aad_LUT_RG_gain,
			      17);
		pr_info("set aad_LUT_RG_gain\n");
	} else if (!strcmp(param[0], "aad_LUT_BG_gain")) {
		if (!fw_param->aad_param)
			goto error;
		if (!param[1])
			goto error;
		str_sapr_conv(param[1], 5,
			      fw_param->aad_param->aad_LUT_BG_gain,
			      17);
		pr_info("set aad_LUT_BG_gain\n");
	} else if (!strcmp(param[0], "aad_Y_gain_min")) {
		if (!fw_param->aad_param)
			goto error;
		if (kstrtoul(param[1], 10, &val) < 0)
			goto error;
		fw_param->aad_param->aad_Y_gain_min = val;
		pr_info("aad_Y_gain_min = %d\n", (int)val);
	} else if (!strcmp(param[0], "aad_Y_gain_max")) {
		if (!fw_param->aad_param)
			goto error;
		if (kstrtoul(param[1], 10, &val) < 0)
			goto error;
		fw_param->aad_param->aad_Y_gain_max = val;
		pr_info("aad_Y_gain_max = %d\n", (int)val);
	} else if (!strcmp(param[0], "aad_RG_gain_min")) {
		if (!fw_param->aad_param)
			goto error;
		if (kstrtoul(param[1], 10, &val) < 0)
			goto error;
		fw_param->aad_param->aad_RG_gain_min = val;
		pr_info("aad_RG_gain_min = %d\n", (int)val);
	} else if (!strcmp(param[0], "aad_RG_gain_max")) {
		if (!fw_param->aad_param)
			goto error;
		if (kstrtoul(param[1], 10, &val) < 0)
			goto error;
		fw_param->aad_param->aad_RG_gain_max = val;
		pr_info("aad_RG_gain_max = %d\n", (int)val);
	} else if (!strcmp(param[0], "aad_BG_gain_min")) {
		if (!fw_param->aad_param)
			goto error;
		if (kstrtoul(param[1], 10, &val) < 0)
			goto error;
		fw_param->aad_param->aad_BG_gain_min = val;
		pr_info("aad_BG_gain_min = %d\n", (int)val);
	} else if (!strcmp(param[0], "aad_BG_gain_max")) {
		if (!fw_param->aad_param)
			goto error;
		if (kstrtoul(param[1], 10, &val) < 0)
			goto error;
		fw_param->aad_param->aad_BG_gain_max = val;
		pr_info("aad_BG_gain_max = %d\n", (int)val);
	} else if (!strcmp(param[0], "aad_tf_en")) {
		if (!fw_param->aad_param)
			goto error;
		if (kstrtoul(param[1], 10, &val) < 0)
			goto error;
		fw_param->aad_param->aad_tf_en = val;
		pr_info("aad_tf_en = %d\n", (int)val);
	} else if (!strcmp(param[0], "aad_tf_alpha")) {
		if (!fw_param->aad_param)
			goto error;
		if (kstrtoul(param[1], 10, &val) < 0)
			goto error;
		fw_param->aad_param->aad_tf_alpha = val;
		pr_info("aad_tf_alpha = %d\n", (int)val);
	} else if (!strcmp(param[0], "aad_dist_mode")) {
		if (!fw_param->aad_param)
			goto error;
		if (kstrtoul(param[1], 10, &val) < 0)
			goto error;
		fw_param->aad_param->aad_dist_mode = val;
		pr_info("aad_dist_mode = %d\n", (int)val);
	} else if (!strcmp(param[0], "aad_force_gain_en")) {
		if (!fw_param->aad_param)
			goto error;
		if (kstrtoul(param[1], 10, &val) < 0)
			goto error;
		fw_param->aad_param->aad_force_gain_en = val;
		pr_info("aad_force_gain_en = %d\n", (int)val);
	} else if (!strcmp(param[0], "aad_rgb_gain")) {
		if (!fw_param->aad_param)
			goto error;
		if (param[3]) {
			if (kstrtoul(param[1], 10, &val) < 0)
				goto error;
			fw_param->aad_param->aad_r_gain = val;
			if (kstrtoul(param[2], 10, &val) < 0)
				goto error;
			fw_param->aad_param->aad_g_gain = val;
			if (kstrtoul(param[3], 10, &val) < 0)
				goto error;
			fw_param->aad_param->aad_b_gain = val;
			pr_info("aad_rgb_gain = %d, %d, %d\n",
				fw_param->aad_param->aad_r_gain,
				fw_param->aad_param->aad_g_gain,
				fw_param->aad_param->aad_b_gain);
		}
	} else if (!strcmp(param[0], "aad_gain_lut")) {
		if (!fw_param->aad_param)
			goto error;
		if (!param[1])
			goto error;
		str_sapr_conv(param[1], 4,
			      lut,
			      33);
		for (i = 0; i < 11; i++) {
			fw_param->aad_param->aad_gain_lut[i][0] = lut[i * 3];
			fw_param->aad_param->aad_gain_lut[i][1] = lut[i * 3 + 1];
			fw_param->aad_param->aad_gain_lut[i][2] = lut[i * 3 + 2];
		}
		pr_info("set aad_gain_lut\n");
	} else if (!strcmp(param[0], "gain_lut")) {
		if (!fw_param->aad_param)
			goto error;
		if (param[4]) {
			if (kstrtoul(param[1], 10, &val) < 0)
				goto error;
			i = val;
			if (kstrtoul(param[2], 10, &val) < 0)
				goto error;
			fw_param->aad_param->aad_gain_lut[i][0] = val;
			if (kstrtoul(param[3], 10, &val) < 0)
				goto error;
			fw_param->aad_param->aad_gain_lut[i][1] = val;
			if (kstrtoul(param[4], 10, &val) < 0)
				goto error;
			fw_param->aad_param->aad_gain_lut[i][2] = val;
			pr_info("gain_lut[%d] = %d, %d, %d\n",
				i,
				fw_param->aad_param->aad_gain_lut[i][0],
				fw_param->aad_param->aad_gain_lut[i][1],
				fw_param->aad_param->aad_gain_lut[i][2]);
		}
	} else if (!strcmp(param[0], "aad_xy_lut")) {
		if (!fw_param->aad_param)
			goto error;
		if (!param[1])
			goto error;
		str_sapr_conv(param[1], 4,
			      lut,
			      22);
		for (i = 0; i < 11; i++) {
			fw_param->aad_param->aad_xy_lut[i][0] = lut[i * 2];
			fw_param->aad_param->aad_xy_lut[i][1] = lut[i * 2 + 1];
		}
		pr_info("set aad_xy_lut\n");
	} else if (!strcmp(param[0], "xy_lut")) {
		if (!fw_param->aad_param)
			goto error;
		if (param[3]) {
			if (kstrtoul(param[1], 10, &val) < 0)
				goto error;
			i = val;
			if (kstrtoul(param[2], 10, &val) < 0)
				goto error;
			fw_param->aad_param->aad_xy_lut[i][0] = val;
			if (kstrtoul(param[3], 10, &val) < 0)
				goto error;
			fw_param->aad_param->aad_xy_lut[i][1] = val;
			pr_info("gain_lut[%d] = %d, %d\n",
				i,
				fw_param->aad_param->aad_xy_lut[i][0],
				fw_param->aad_param->aad_xy_lut[i][1]);
		}
	} else if (!strcmp(param[0], "aad_min_dist_th")) {
		if (!fw_param->aad_param)
			goto error;
		if (kstrtoul(param[1], 10, &val) < 0)
			goto error;
		fw_param->aad_param->aad_min_dist_th = val;
		pr_info("aad_min_dist_th = %d\n", (int)val);
	} else if (!strcmp(param[0], "sensor_input")) {
		if (!fw_param->aad_param)
			goto error;
		if (param[3]) {
			if (kstrtoul(param[1], 10, &val) < 0)
				goto error;
			fw_param->aad_param->sensor_input[0] = val;
			if (kstrtoul(param[2], 10, &val) < 0)
				goto error;
			fw_param->aad_param->sensor_input[1] = val;
			if (kstrtoul(param[3], 10, &val) < 0)
				goto error;
			fw_param->aad_param->sensor_input[2] = val;
			pr_info("sensor_input = %d, %d, %d\n",
				fw_param->aad_param->sensor_input[0],
				fw_param->aad_param->sensor_input[1],
				fw_param->aad_param->sensor_input[2]);
		}
	} else if (!strcmp(param[0], "cabc_max95_ratio")) {/*debug cabc*/
		if (!fw_param->cabc_param)
			goto error;
		if (kstrtoul(param[1], 10, &val) < 0)
			goto error;
		fw_param->cabc_param->cabc_max95_ratio = val;
		pr_info("cabc_max95_ratio = %d\n", (int)val);
	} else if (!strcmp(param[0], "cabc_hist_mode")) {
		if (!fw_param->cabc_param)
			goto error;
		if (kstrtoul(param[1], 10, &val) < 0)
			goto error;
		fw_param->cabc_param->cabc_hist_mode = val;
		pr_info("cabc_hist_mode = %d\n", (int)val);
	} else if (!strcmp(param[0], "cabc_hist_blend_alpha")) {
		if (!fw_param->cabc_param)
			goto error;
		if (kstrtoul(param[1], 10, &val) < 0)
			goto error;
		fw_param->cabc_param->cabc_hist_blend_alpha = val;
		pr_info("cabc_hist_blend_alpha = %d\n", (int)val);
	} else if (!strcmp(param[0], "cabc_init_bl_min")) {
		if (!fw_param->cabc_param)
			goto error;
		if (kstrtoul(param[1], 10, &val) < 0)
			goto error;
		fw_param->cabc_param->cabc_init_bl_min = val;
		pr_info("cabc_init_bl_min = %d\n", (int)val);
	} else if (!strcmp(param[0], "cabc_init_bl_max")) {
		if (!fw_param->cabc_param)
			goto error;
		if (kstrtoul(param[1], 10, &val) < 0)
			goto error;
		fw_param->cabc_param->cabc_init_bl_max = val;
		pr_info("cabc_init_bl_max = %d\n", (int)val);
	} else if (!strcmp(param[0], "cabc_tf_alpha")) {
		if (!fw_param->cabc_param)
			goto error;
		if (kstrtoul(param[1], 10, &val) < 0)
			goto error;
		fw_param->cabc_param->cabc_tf_alpha = val;
		pr_info("cabc_tf_alpha = %d\n", (int)val);
	} else if (!strcmp(param[0], "cabc_tf_en")) {
		if (!fw_param->cabc_param)
			goto error;
		if (kstrtoul(param[1], 10, &val) < 0)
			goto error;
		fw_param->cabc_param->cabc_tf_en = val;
		pr_info("cabc_tf_en = %d\n", (int)val);
	} else if (!strcmp(param[0], "cabc_sc_flag")) {
		if (!fw_param->cabc_param)
			goto error;
		if (kstrtoul(param[1], 10, &val) < 0)
			goto error;
		fw_param->cabc_param->cabc_sc_flag = val;
		pr_info("cabc_sc_flag = %d\n", (int)val);
	} else if (!strcmp(param[0], "cabc_sc_hist_diff_thd")) {
		if (!fw_param->cabc_param)
			goto error;
		if (kstrtoul(param[1], 10, &val) < 0)
			goto error;
		fw_param->cabc_param->cabc_sc_hist_diff_thd = val;
		pr_info("cabc_sc_hist_diff_thd = %d\n", (int)val);
	} else if (!strcmp(param[0], "cabc_sc_apl_diff_thd")) {
		if (!fw_param->cabc_param)
			goto error;
		if (kstrtoul(param[1], 10, &val) < 0)
			goto error;
		fw_param->cabc_param->cabc_sc_apl_diff_thd = val;
		pr_info("cabc_sc_apl_diff_thd = %d\n", (int)val);
	} else if (!strcmp(param[0], "cabc_en")) {
		if (!fw_param->cabc_param)
			goto error;
		if (kstrtoul(param[1], 10, &val) < 0)
			goto error;
		fw_param->cabc_param->cabc_en = val;
		pr_info("cabc_en = %d\n", (int)val);
	} else if (!strcmp(param[0], "pre_gamma_gain_ratio")) {/*debug pre gma*/
		if (!fw_param->pre_gam_param)
			goto error;
		if (kstrtoul(param[1], 10, &val) < 0)
			goto error;
		fw_param->pre_gam_param->pre_gamma_gain_ratio = val;
		pr_info("pre_gamma_gain_ratio = %d\n", (int)val);
	} else if (!strcmp(param[0], "debug_mode")) {/*debug mode*/
		if (kstrtoul(param[1], 10, &val) < 0)
			goto error;
		fw_param->debug_mode = val;
		pr_info("debug_mode = %d\n", (int)val);
	}

	return 0;
error:
	return -1;
}
