// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/version.h>
#include <linux/string.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/amlogic/media/vout/lcd/aml_ldim.h>
#include <linux/amlogic/media/vout/lcd/ldim_fw.h>
#include "ldim_drv.h"

static struct ld_reg_s nprm;
static struct fw_data_s fdata;

/*bl_matrix remap curve*/
static unsigned int bl_remap_curve[17] = {
	0, 256, 512, 768, 1024, 1280, 1536, 1792,
	2048, 2304, 2560, 2816, 3072, 3328, 3584, 3840, 4095
};

static unsigned int fw_ld_whist[16] = {
	32, 64, 96, 128, 160, 192, 224, 256,
	288, 320, 352, 384, 416, 448, 480, 512
};

static struct ldc_prm_s ldim_prm_ldc = {
	.ldc_hist_mode = 3,
	.ldc_hist_blend_mode = 1,
	.ldc_hist_blend_alpha = 0x60,
	.ldc_hist_adap_blend_max_gain = 13,
	.ldc_hist_adap_blend_diff_th1 = 256,
	.ldc_hist_adap_blend_diff_th2 = 640,
	.ldc_hist_adap_blend_th0 = 2,
	.ldc_hist_adap_blend_thn = 4,
	.ldc_hist_adap_blend_gain_0 = 0x70,
	.ldc_hist_adap_blend_gain_1 = 0x40,
	.ldc_init_bl_min = 0,
	.ldc_init_bl_max = 4095,

	.ldc_sf_mode = 2,
	.ldc_sf_gain_up = 0x20,
	.ldc_sf_gain_dn = 0x0,
	.ldc_sf_tsf_3x3 = 0x600,
	.ldc_sf_tsf_5x5 = 0xc00,

	.ldc_bs_bl_mode = 0,
	.ldc_glb_apl = 4095,
	.ldc_bs_glb_apl_gain = 0x20,
	.ldc_bs_dark_scene_bl_th = 0x200,
	.ldc_bs_gain = 0x20,
	.ldc_bs_limit_gain = 0x60,
	.ldc_bs_loc_apl_gain = 0x20,
	.ldc_bs_loc_max_min_gain = 0x20,
	.ldc_bs_loc_dark_scene_bl_th = 0x600,

	.ldc_tf_en = 1,
	.ldc_tf_sc_flag = 0,
	.ldc_tf_low_alpha = 0x20,
	.ldc_tf_high_alpha = 0x20,
	.ldc_tf_low_alpha_sc = 0x40,
	.ldc_tf_high_alpha_sc = 0x40,

	.ldc_dimming_curve_en = 1,
	.ldc_sc_hist_diff_th = (3840 * 2160) / 3,
	.ldc_sc_apl_diff_th = (24 << 4),
};

static struct fw_ctrl_s ldim_fw_ctrl = {
	.fw_ld_thsf_l = 1600,
	.fw_ld_thtf_l = 256,
	.fw_ld_thist = 0, /* 0 for default ((vnum * hnum * 5) >> 2) */
	.boost_gain = 456, /*norm 256 to 1,T960 finally use*/
	.tf_alpha = 256, /*256;*/
	.lpf_gain = 128,  /* [0~128~256], norm 128 as 1*/
	.boost_gain_neg = 3,
	.alpha_delta = 255,/* to fix flicker */

	.lpf_res = 14,    /* 1024/9*9 = 13,LPF_method=3 */
	.rgb_base = 127,

	.ov_gain = 16,

	.avg_gain = LD_DATA_MAX,

	.fw_rgb_diff_th = 32760,
	.max_luma = 4060,
	.lmh_avg_th = 200,/*for woman flicker*/
	.fw_tf_sum_th = 32760,/*20180530*/

	.lpf_method = 3,
	.ld_tf_step_th = 100,
	.tf_step_method = 3,
	.tf_fresh_bl = 8,

	.tf_blk_fresh_bl = 5,
	.side_blk_diff_th = 100,
	.bbd_th = 200,
	.bbd_detect_en = 0,
	.diff_blk_luma_en = 1,

	.sf_bypass = 0,
	.boost_light_bypass = 1,
	.lpf_bypass = 1,
	.ld_remap_bypass = 0,
	.black_frm = 0,

	.white_area_remap_en = 0,
	.white_area_th_max = 100,
	.white_area_th_min = 10,
	.white_lvl_th_max = 4095,
	.white_lvl_th_min = 2048,

	.bl_remap_curve = bl_remap_curve,
	.fw_ld_whist = fw_ld_whist,

	.nprm = &nprm,
	.prm_ldc = &ldim_prm_ldc,
};

static struct ldim_fw_s ldim_fw = {
	/* header */
	.para_ver = FW_PARA_VER,
	.para_size = sizeof(struct ldim_fw_s),
	.fw_ctrl_size = sizeof(struct fw_ctrl_s),
	.alg_ver = "not installed",
	.fw_sel = 1, /* bit0:hw/sw, bit1:no/have fw_cus */
	.valid = 0,
	.flag = 0,

	.seg_col = 1,
	.seg_row = 1,

	.bl_matrix_dbg = 0,
	.fw_hist_print = 0,
	.fw_print_frequent = 30,
	.fw_print_lv = 0,

	.ctrl = &ldim_fw_ctrl,
	.fdat = &fdata,
	.bl_matrix = NULL,

	.fw_alg_frm = NULL,
	.fw_alg_para_print = NULL,
};

static struct ldim_fw_custom_s ldim_fw_cus_pre = {
	.valid = 0,
	.seg_col = 1,
	.seg_row = 1,
	.global_hist_bin_num = 64,

	.fw_print_frequent = 200,
	.fw_print_lv = 0,

	.param = NULL,
	.bl_matrix = NULL,

	.fw_alg_frm = NULL,
	.fw_alg_para_print = NULL,
};

static struct ldim_fw_custom_s ldim_fw_cus_post = {
	.valid = 0,
	.seg_col = 1,
	.seg_row = 1,
	.global_hist_bin_num = 64,

	.fw_print_frequent = 200,
	.fw_print_lv = 0,

	.param = NULL,
	.bl_matrix = NULL,

	.fw_alg_frm = NULL,
	.fw_alg_para_print = NULL,
};

struct ldim_fw_s *aml_ldim_get_fw(void)
{
	return &ldim_fw;
}
EXPORT_SYMBOL(aml_ldim_get_fw);

struct ldim_fw_custom_s *aml_ldim_get_fw_cus_pre(void)
{
	return &ldim_fw_cus_pre;
}
EXPORT_SYMBOL(aml_ldim_get_fw_cus_pre);

struct ldim_fw_custom_s *aml_ldim_get_fw_cus_post(void)
{
	return &ldim_fw_cus_post;
}
EXPORT_SYMBOL(aml_ldim_get_fw_cus_post);


