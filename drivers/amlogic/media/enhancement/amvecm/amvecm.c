// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * drivers/amlogic/media/enhancement/amvecm/amvecm.c
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

/* Standard Linux headers */
#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/errno.h>
#include <linux/uaccess.h>
#include <linux/compat.h>
#include <linux/of.h>
#include <linux/of_device.h>
/* #include <linux/amlogic/aml_common.h> */
#include <linux/ctype.h>/* for parse_para_pq */
#include <linux/vmalloc.h>
#include <linux/clk.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/amvecm/amvecm.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#ifdef CONFIG_AMLOGIC_PIXEL_PROBE
#include <linux/amlogic/pixel_probe.h>
#endif
#include <linux/io.h>
#include <linux/poll.h>
#include <linux/workqueue.h>
#include <linux/sched/clock.h>

#ifdef CONFIG_AMLOGIC_LCD
#include <linux/amlogic/media/vout/lcd/lcd_notify.h>
#endif

#include <linux/amlogic/media/amvecm/color_tune.h>
#include "arch/vpp_regs.h"
#include "arch/ve_regs.h"
#include "arch/cm_regs.h"
#include "arch/vpp_hdr_regs.h"
#include "arch/vpp_a4_regs.h"
#ifdef CONFIG_AMLOGIC_MEDIA_TVIN
#include "../../vin/tvin/tvin_global.h"
#endif

#include "amve.h"
#include "amcm.h"
#include "amcsc.h"
#include "keystone_correction.h"
#include "bitdepth.h"
#include "cm2_adj.h"
#include "pattern_detection.h"
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
#include <linux/amlogic/media/amdolbyvision/dolby_vision.h>
#endif
#include "dnlp_cal.h"
#include "vlock.h"
#include "hdr/am_hdr10_plus.h"
#include "local_contrast.h"
#include "set_hdr2_v0.h"
#include "s5_set_hdr2_v0.h"
#include "ai_pq/ai_pq.h"
#include "reg_helper.h"
#include "reg_default_setting.h"
#include "util/enc_dec.h"
#include "cabc_aadc/cabc_aadc_fw.h"
#include "hdr/am_hdr10_tmo_fw.h"
#include "hdr/am_cuva_hdr_tm.h"
#include "hdr/gamut_convert.h"
#include "../../video_sink/vpp_pq.h"
#include "frame_lock_policy.h"
#include "amve_v2.h"
#include "color/ai_color.h"
#include "am_lut3d.h"
#ifdef CONFIG_AMLOGIC_MEDIA_VPP
#include <linux/amlogic/media/vpp/vpp_common_def.h>
#include <linux/amlogic/media/vpp/vpp_drv.h>
//#include "../../vpp/vpp_vf_proc.h"
#endif
#include "hdr/am_hdr_sbtm.h"

#define pr_amvecm_dbg(fmt, args...)\
	do {\
		if (debug_amvecm)\
			pr_info("AMVECM: " fmt, ## args);\
	} while (0)
#define pr_amvecm_error(fmt, args...)\
	pr_error("AMVECM: " fmt, ## args)

#define AMVECM_NAME               "amvecm"
#define AMVECM_DRIVER_NAME        "amvecm"
#define AMVECM_MODULE_NAME        "amvecm"
#define AMVECM_DEVICE_NAME        "amvecm"
#define AMVECM_CLASS_NAME         "amvecm"
#define AMVECM_VER				"Ref.2018/11/20"

struct amvecm_dev_s {
	dev_t                       devt;
	struct cdev                 cdev;
	dev_t                       devno;
	struct device               *dev;
	struct class                *clsp;
	wait_queue_head_t           hdr_queue;
	/*hdr*/
	struct hdr_data_t	hdr_d;
	struct gamma_data_s gm_data;
	/*vlock cts_vid_lock_clk*/
	struct clk *vlock_clk;
};

#ifdef CONFIG_AMLOGIC_LCD
struct work_struct aml_lcd_vlock_param_work;
#endif

static struct amvecm_dev_s amvecm_dev;
static struct resource *res_viu2_vsync_irq;
static struct resource *res_lc_curve_irq;
static struct workqueue_struct *aml_cabc_queue;
static struct work_struct cabc_proc_work;
static struct work_struct cabc_bypass_work;

enum chip_type chip_type_id;
enum chip_cls_e chip_cls_id;

struct demo_data_s demo_data;

/*gamma loading protect*/
spinlock_t vpp_lcd_gamma_lock;
/*for 3dlut protect*/
spinlock_t vpp_3dlut_lock;

/*3dlut loading protect*/
struct mutex vpp_lut3d_lock;

signed int vd1_brightness = 0, vd1_contrast;

static int hue_pre;  /*-25~25*/
static int saturation_pre;  /*-128~127*/
static int hue_post;  /*-25~25*/
static int saturation_post;  /*-128~127*/

static s16 saturation_ma;
static s16 saturation_mb;
static s16 saturation_ma_shift;
static s16 saturation_mb_shift;

enum ecm_color_type cm_cur_work_color_md = cm_14_color;
int cm2_debug;

static int cm2_hue_array[cm_14_ecm2colormode_max][3];
static int cm2_luma_array[cm_14_ecm2colormode_max][3];
static int cm2_sat_array[cm_14_ecm2colormode_max][3];
static int cm2_hue_by_hs_array[cm_14_ecm2colormode_max][3];

unsigned int sr1_reg_val[101];
unsigned int sr1_ret_val[101];
struct vpp_hist_param_s vpp_hist_param;
static unsigned int pre_hist_height, pre_hist_width;
static unsigned int pc_mode = 0xff;
static unsigned int pc_mode_last = 0xff;
static struct hdr_metadata_info_s vpp_hdr_metadata_s;
unsigned int atv_source_flg;

#define VDJ_FLAG_BRIGHTNESS		BIT(0)
#define VDJ_FLAG_BRIGHTNESS2		BIT(1)
#define VDJ_FLAG_SAT_HUE		BIT(2)
#define VDJ_FLAG_SAT_HUE_POST		BIT(3)
#define VDJ_FLAG_CONTRAST		BIT(4)
#define VDJ_FLAG_CONTRAST2		BIT(5)
#define VDJ_FLAG_VADJ_EN        BIT(6)

static int vdj_mode_flg;
struct am_vdj_mode_s vdj_mode_s;
struct pq_ctrl_s pq_cfg;
struct pq_ctrl_s dv_cfg_bypass;

struct pq_ctrl_s pq_cfg_init[PQ_CFG_MAX] = {
	/*for tv enable pq module*/
	{
		.sharpness0_en = 1,
		.sharpness1_en = 1,
		.dnlp_en = 1,
		.cm_en = 1,
		.vadj1_en = 1,
		.vd1_ctrst_en = 0,
		.vadj2_en = 0,
		.post_ctrst_en = 0,
		.wb_en = 1,
		.gamma_en = 1,
		.lc_en = 1,
		.black_ext_en = 1,
		.chroma_cor_en = 0,
		.reserved = 0,
	},
	/*for box disable all pq module*/
	{
		.sharpness0_en = 0,
		.sharpness1_en = 0,
		.dnlp_en = 0,
		.cm_en = 0,
		.vadj1_en = 0,
		.vd1_ctrst_en = 0,
		.vadj2_en = 0,
		.post_ctrst_en = 0,
		.wb_en = 0,
		.gamma_en = 0,
		.lc_en = 0,
		.black_ext_en = 0,
		.chroma_cor_en = 0,
		.reserved = 0,
	},
	/*for tv dv bypass pq module*/
	{
		.sharpness0_en = 0,
		.sharpness1_en = 0,
		.dnlp_en = 0,
		.cm_en = 0,
		.vadj1_en = 0,
		.vd1_ctrst_en = 0,
		.vadj2_en = 0,
		.post_ctrst_en = 0,
		.wb_en = 1,
		.gamma_en = 1,
		.lc_en = 0,
		.black_ext_en = 0,
		.chroma_cor_en = 0,
		.reserved = 0,
	},
	/*for ott dv bypass pq module*/
	{
		.sharpness0_en = 0,
		.sharpness1_en = 0,
		.dnlp_en = 0,
		.cm_en = 0,
		.vadj1_en = 0,
		.vd1_ctrst_en = 0,
		.vadj2_en = 0,
		.post_ctrst_en = 0,
		.wb_en = 0,
		.gamma_en = 0,
		.lc_en = 0,
		.black_ext_en = 0,
		.chroma_cor_en = 0,
		.reserved = 0,
	}
};

/*void __iomem *amvecm_hiu_reg_base;*//* = *ioremap(0xc883c000, 0x2000); */

static int debug_amvecm;
module_param(debug_amvecm, int, 0664);
MODULE_PARM_DESC(debug_amvecm, "\n debug_amvecm\n");

unsigned int vecm_latch_flag;
module_param(vecm_latch_flag, uint, 0664);
MODULE_PARM_DESC(vecm_latch_flag, "\n vecm_latch_flag\n");

unsigned int vecm_latch_flag2;
module_param(vecm_latch_flag2, uint, 0664);
MODULE_PARM_DESC(vecm_latch_flag2, "\n vecm_latch_flag2\n");

unsigned int pq_load_en = 1;/* load pq table enable/disable */
module_param(pq_load_en, uint, 0664);
MODULE_PARM_DESC(pq_load_en, "\n pq_load_en\n");

bool gamma_en;  /* wb_gamma_en enable/disable */
module_param(gamma_en, bool, 0664);
MODULE_PARM_DESC(gamma_en, "\n gamma_en\n");

bool wb_en;  /* wb_en enable/disable */
module_param(wb_en, bool, 0664);
MODULE_PARM_DESC(wb_en, "\n wb_en\n");

unsigned int lut3d_long_sec_en = 1;  /* lut3d_long_sec_en enable/disable */
unsigned int lut3d_compress = 1;  /* lut3d_compress enable/disable */
unsigned int lut3d_write_from_file = 1;  /* lut3d_write from a file */
unsigned int lut3d_data_source = 2;/* read fron bin */

unsigned int probe_ok;/* probe ok or not */
module_param(probe_ok, uint, 0664);
MODULE_PARM_DESC(probe_ok, "\n probe_ok\n");

static unsigned int sr1_index;/* for sr1 read */
module_param(sr1_index, uint, 0664);
MODULE_PARM_DESC(sr1_index, "\n sr1_index\n");

static int mtx_sel_dbg;/* for mtx debug */
module_param(mtx_sel_dbg, uint, 0664);
MODULE_PARM_DESC(mtx_sel_dbg, "\n mtx_sel_dbg\n");

unsigned int pq_user_latch_flag;
module_param(pq_user_latch_flag, uint, 0664);
MODULE_PARM_DESC(pq_user_latch_flag, "\n pq_user_latch_flag\n");

unsigned int fmeter_debug = 255;/* for fmeter debug */
module_param(fmeter_debug, uint, 0664);
MODULE_PARM_DESC(fmeter_debug, "\n fmeter_debug\n");

unsigned int fmeter_count = 2;/* for fmeter count */
module_param(fmeter_count, uint, 0664);
MODULE_PARM_DESC(fmeter_count, "\n fmeter_count\n");

/*0: 709/601	1: bt2020*/
int tx_op_color_primary;
module_param(tx_op_color_primary, int, 0664);
MODULE_PARM_DESC(tx_op_color_primary,
		 "tx output color_primary");

unsigned int debug_game_mode_1;
module_param(debug_game_mode_1, uint, 0664);
MODULE_PARM_DESC(debug_game_mode_1, "\n debug_game_mode_1\n");
int freerun_en = GAME_MODE;/* 0:game mode;1:freerun mode */
module_param(freerun_en, int, 0664);
MODULE_PARM_DESC(freerun_en, "\n enable or disable freerun\n");

/*blue stretch function with 3dlut*/
int bs_3dlut_en = 1;
module_param(bs_3dlut_en, int, 0664);
MODULE_PARM_DESC(bs_3dlut_en, "\n bs_3dlut_en\n");

unsigned int ct_en;
module_param(ct_en, uint, 0664);
MODULE_PARM_DESC(ct_en, "\n color tune\n");

unsigned int ai_color_enable;
module_param(ai_color_enable, uint, 0664);
MODULE_PARM_DESC(ai_color_enable, "\n ai_color_enable\n");

int rd_vencl;
module_param(rd_vencl, int, 0664);
MODULE_PARM_DESC(rd_vencl, "\n rd_vencl\n");

unsigned int pq_user_value;
enum hdr_type_e hdr_source_type = HDRTYPE_NONE;

unsigned int pd_detect_en;
int pd_weak_fix_lvl = PD_LOW_LVL;
int pd_fix_lvl = PD_HIG_LVL;

unsigned int pd_det;

unsigned int gmv_weak_th = 4;
unsigned int gmv_th = 17;

int pre_fmeter_level = 0, cur_fmeter_level = 0, fmeter_flag = 0;
int cur_sr_level = 5;
int sat_hue_offset_val;

static int wb_init_bypass_coef[24] = {
	0, 0, 0, /* pre offset */
	1024,	0,	0,
	0,	1024,	0,
	0,	0,	1024,
	0, 0, 0, /* 10'/11'/12' */
	0, 0, 0, /* 20'/21'/22' */
	0, 0, 0, /* offset */
	0, 0, 0  /* mode, right_shift, clip_en */
};

static int sr0_gain_val[11] = {
	128, 128, 120, 112, 104, 96, 88, 80, 72, 64, 64
};

static int sr0_shoot_val[11] = {
	30, 28, 26, 24, 22, 20, 18, 16, 14, 12, 10
};

static int sr1_gain_val[11] = {
	128, 128, 120, 112, 104, 96, 88, 80, 72, 64, 64
};

static int sr1_shoot_val[11] = {
	40, 38, 36, 34, 32, 30, 28, 26, 24, 22, 20
};

static int sr0_gain_lmt[11] = {
	5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 60
};

static int sr1_gain_lmt[11] = {
	20, 25, 28, 30, 35, 38, 40, 45, 50, 55, 60
};

static int nn_coring[11] = {
	10, 8, 6, 4, 3, 2, 1, 1, 1, 1, 1
};

#define AIPQ_SCENE_MAX 25
#define AIPQ_FUNC_MAX 10
#define AIPQ_SINGL_DATA_LEN 3

static struct vpp_mtx_info_s mtx_info = {
	MTX_NULL,
	{
		{0, 0, 0},
		{
			{0x400, 0x0, 0x0},
			{0x0, 0x400, 0x0},
			{0x0, 0x0, 0x400},
		},
		{0, 0, 0},
		0,
		0,
	}
};

static struct pre_gamma_table_s pre_gamma;
struct eye_protect_s eye_protect;
static int hist_chl;

static unsigned int cm_slice_idx;

static void str_sapr_to_d(char *s, int *d, int n)
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

static void d_convert_str(int num,
			  int num_num, char cur_s[],
			  int char_bit, int bit_chose)
{
	char buf[9] = {0};
	int i, count;

	if (bit_chose == 10)
		snprintf(buf, sizeof(buf), "%d", num);
	else if (bit_chose == 16)
		snprintf(buf, sizeof(buf), "%x", num);
	count = strlen(buf);
	if (count > 4)
		count = 4;
	for (i = 0; i < count; i++)
		buf[i + char_bit] = buf[i];
	for (i = 0; i < char_bit; i++)
		buf[i] = '0';
	count = strlen(buf);
	for (i = 0; i < char_bit; i++)
		buf[i] = buf[count - char_bit + i];
	if (num_num > 0) {
		for (i = 0; i < char_bit; i++)
			cur_s[i + num_num * char_bit] =
				buf[i];
	} else {
		for (i = 0; i < char_bit; i++)
			cur_s[i] = buf[i];
	}
}

/* vpp brightness/contrast/saturation/hue */
int __init amvecm_load_pq_val(char *str)
{
	int i = 0, err = 0;
	char *tk = NULL, *tmp[4];
	long val;

	if (!str) {
		pr_err("[amvecm] pq val error !!!\n");
		return 0;
	}

	for (tk = strsep(&str, ","); tk; tk = strsep(&str, ",")) {
		tmp[i] = tk;
		err = kstrtol(tmp[i], 10, &val);
		if (err) {
			pr_err("[amvecm] pq string error !!!\n");
			break;
		}
		/* pr_err("[amvecm] pq[%d]: %d\n", i, (int)val[i]); */

		/* only need to get sat/hue value,*/
		/*brightness/contrast can be got from registers */
		if (i == 2)
			saturation_post = (int)val;
		else if (i == 3)
			hue_post = (int)val;
		i++;
		if (i >= 4)
			return 0;
	}

	return 0;
}
__setup("pq=", amvecm_load_pq_val);

void amvecm_vadj_latch_process(void)
{
	/*vadj switch control according to vadj1_en/vadj2_en*/
	unsigned int cur_vadj1_en, cur_vadj2_en;
	unsigned int vadj1_en = vdj_mode_s.vadj1_en;
	unsigned int vadj2_en = vdj_mode_s.vadj2_en;

	if (vecm_latch_flag & FLAG_VADJ_EN) {
		vecm_latch_flag &= ~FLAG_VADJ_EN;
		if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A) {
			cur_vadj1_en = READ_VPP_REG_BITS(VPP_VADJ1_MISC, 0, 1);
			cur_vadj2_en = READ_VPP_REG_BITS(VPP_VADJ2_MISC, 0, 1);
			if (cur_vadj1_en != vadj1_en) {
				VSYNC_WRITE_VPP_REG_BITS(VPP_VADJ1_MISC,
							 vadj1_en, 0, 1);
				pr_amvecm_dbg("[amvecm.]vadj1 switch[%d->%d]success.\n",
					      cur_vadj1_en, vadj1_en);
			} else {
				pr_amvecm_dbg("[amvecm.] vadj1_en status unchanged.\n");
			}

			if (cur_vadj2_en != vadj2_en) {
				VSYNC_WRITE_VPP_REG_BITS(VPP_VADJ2_MISC,
							 vadj2_en, 0, 1);
				pr_amvecm_dbg("[amvecm.] vadj2 switch [%d->%d] success.\n",
					      cur_vadj2_en, vadj2_en);
			} else {
				pr_amvecm_dbg("[amvecm.] vadj2_en status unchanged.\n");
			}
		} else {
			cur_vadj1_en = READ_VPP_REG_BITS(VPP_VADJ_CTRL, 0, 1);
			cur_vadj2_en = READ_VPP_REG_BITS(VPP_VADJ_CTRL, 2, 1);

			if (cur_vadj1_en != vadj1_en) {
				VSYNC_WRITE_VPP_REG_BITS(VPP_VADJ_CTRL,
							 vadj1_en, 0, 1);
				pr_amvecm_dbg("[amvecm] vadj1 switch [%d->%d] success.\n",
					      cur_vadj1_en, vadj1_en);
			} else {
				pr_amvecm_dbg("[amvecm] vadj1_en status unchanged.\n");
			}

			if (cur_vadj2_en != vadj2_en) {
				VSYNC_WRITE_VPP_REG_BITS(VPP_VADJ_CTRL,
							 vadj2_en, 2, 1);
				pr_amvecm_dbg("[amvecm] vadj2 switch [%d->%d] success.\n",
					      cur_vadj2_en, vadj2_en);
			} else {
				pr_amvecm_dbg("[amvecm] vadj2_en status unchanged.\n");
			}
		}
	}
}

static int amvecm_set_contrast2(int val)
{
	val += 0x80;

	if (chip_type_id == chip_a4) {
		WRITE_VPP_REG_BITS(VOUT_VADJ_Y,
			val, 0, 8);
	} else if (chip_type_id == chip_s5) {
		ve_contrast_set(val, VE_VADJ2, WR_VCB);
	} else if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A) {
		WRITE_VPP_REG_BITS(VPP_VADJ2_Y_2,
				   val, 0, 8);
		WRITE_VPP_REG_BITS(VPP_VADJ2_MISC, 1, 0, 1);
	} else {
		WRITE_VPP_REG_BITS(VPP_VADJ2_Y,
				   val, 0, 8);
		WRITE_VPP_REG_BITS(VPP_VADJ_CTRL, 1, 2, 1);
	}
	return 0;
}

static int amvecm_set_brightness2(int val)
{
	if (chip_type_id == chip_a4) {
		WRITE_VPP_REG_BITS(VOUT_VADJ_Y,
			val, 8, 11);
		return 0;
	} else if (chip_type_id == chip_s5) {
		ve_brigtness_set(val, VE_VADJ2, WR_VCB);
		return 0;
	} else if (get_cpu_type() <= MESON_CPU_MAJOR_ID_GXTVBB) {
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
		WRITE_VPP_REG_BITS(VPP_VADJ2_Y,
				   val, 8, 9);
#endif
	} else if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A)
		WRITE_VPP_REG_BITS(VPP_VADJ2_Y_2,
				   val, 8, 11);
	else
		WRITE_VPP_REG_BITS(VPP_VADJ2_Y,
				   val >> 1, 8, 10);

	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A)
		WRITE_VPP_REG_BITS(VPP_VADJ2_MISC, 1, 0, 1);
	else
		WRITE_VPP_REG_BITS(VPP_VADJ_CTRL, 1, 2, 1);
	return 0;
}

static void amvecm_size_patch(struct vframe_s *vf,
	unsigned int cm_in_w,
	unsigned int cm_in_h)
{
	unsigned int hs, he, vs, ve;

	if (chip_type_id == chip_s5) {
		cm2_frame_size_patch(vf, cm_in_w, cm_in_h);
		return;
	}

	if (!vf)
		return;

	if (cm_in_w == 0 && cm_in_h == 0) {
		hs = READ_VPP_REG_BITS(VPP_POSTBLEND_VD1_H_START_END, 16, 13);
		he = READ_VPP_REG_BITS(VPP_POSTBLEND_VD1_H_START_END, 0, 13);

		vs = READ_VPP_REG_BITS(VPP_POSTBLEND_VD1_V_START_END, 16, 13);
		ve = READ_VPP_REG_BITS(VPP_POSTBLEND_VD1_V_START_END, 0, 13);
		cm2_frame_size_patch(vf, he - hs + 1, ve - vs + 1);
	} else {
		cm2_frame_size_patch(vf, cm_in_w, cm_in_h);
	}
}

/* video adj1 */
static ssize_t video_adj1_brightness_show(struct class *cla,
					  struct class_attribute *attr,
					  char *buf)
{
	s32 val = 0;

	if (chip_type_id == chip_s5) {
		val = ve_brightness_contrast_get(VE_VADJ1);
		val = (val >> 8) & 0x7ff;
		val = (val << 21) >> 21;
		return sprintf(buf, "%d\n", val);
	} else if (get_cpu_type() <= MESON_CPU_MAJOR_ID_GXTVBB) {
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
		val = (READ_VPP_REG(VPP_VADJ1_Y) >> 8) & 0x1ff;
		val = (val << 23) >> 23;

		return sprintf(buf, "%d\n", val);
#endif
	} else if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A) {
		val = (READ_VPP_REG(VPP_VADJ1_Y_2) >> 8) & 0x7ff;
		val = (val << 21) >> 21;

		return sprintf(buf, "%d\n", val);
	}
	val = (READ_VPP_REG(VPP_VADJ1_Y) >> 8) & 0x3ff;
	val = (val << 22) >> 22;

	return sprintf(buf, "%d\n", val << 1);
}

static ssize_t video_adj1_brightness_store(struct class *cla,
					   struct class_attribute *attr,
					   const char *buf, size_t count)
{
	size_t r;
	int val;

	r = sscanf(buf, "%d\n", &val);
	if (r != 1 || val < -1024 || val > 1023)
		return -EINVAL;

	if (chip_type_id == chip_s5)
		ve_brigtness_set(val, VE_VADJ1, WR_VCB);
	else if (get_cpu_type() <= MESON_CPU_MAJOR_ID_GXTVBB)
		WRITE_VPP_REG_BITS(VPP_VADJ1_Y, val, 8, 9);
	else if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A)
		WRITE_VPP_REG_BITS(VPP_VADJ1_Y_2, val, 8, 11);
	else
		WRITE_VPP_REG_BITS(VPP_VADJ1_Y, val >> 1, 8, 10);

	if (chip_type_id == chip_s5)
		return count;

	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A)
		WRITE_VPP_REG_BITS(VPP_VADJ1_MISC, 1, 0, 1);
	else
		WRITE_VPP_REG_BITS(VPP_VADJ_CTRL, 1, 0, 1);

	return count;
}

static ssize_t video_adj1_contrast_show(struct class *cla,
					struct class_attribute *attr,
					char *buf)
{
	if (chip_type_id == chip_s5) {
		return sprintf(buf, "%d\n",
			(int)(ve_brightness_contrast_get(VE_VADJ1) & 0xff) - 0x80);
	} else if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A)
		return sprintf(buf, "%d\n",
			(int)(READ_VPP_REG(VPP_VADJ1_Y_2) & 0xff) - 0x80);
	else
		return sprintf(buf, "%d\n",
			(int)(READ_VPP_REG(VPP_VADJ1_Y) & 0xff) - 0x80);
}

static ssize_t video_adj1_contrast_store(struct class *cla,
					 struct class_attribute *attr,
					 const char *buf, size_t count)
{
	size_t r;
	int val;

	r = sscanf(buf, "%d\n", &val);
	if (r != 1 || val < -127 || val > 127)
		return -EINVAL;

	val += 0x80;

	if (chip_type_id == chip_s5) {
		ve_contrast_set(val, VE_VADJ1, WR_VCB);
	} else if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A) {
		WRITE_VPP_REG_BITS(VPP_VADJ1_Y_2, val, 0, 8);
		WRITE_VPP_REG_BITS(VPP_VADJ1_MISC, 1, 0, 1);
	} else {
		WRITE_VPP_REG_BITS(VPP_VADJ1_Y, val, 0, 8);
		WRITE_VPP_REG_BITS(VPP_VADJ_CTRL, 1, 0, 1);
	}

	return count;
}

/* video adj2 */
static ssize_t video_adj2_brightness_show(struct class *cla,
					  struct class_attribute *attr,
					  char *buf)
{
	s32 val = 0;

	if (chip_type_id == chip_a4) {
		val = (READ_VPP_REG(VOUT_VADJ_Y) >> 8) & 0x7ff;
		val = (val << 21) >> 21;
		return sprintf(buf, "%d\n", val);
	} else if (chip_type_id == chip_s5) {
		val = ve_brightness_contrast_get(VE_VADJ2);
		val = (val >> 8) & 0x7ff;
		val = (val << 21) >> 21;
		return sprintf(buf, "%d\n", val);
	} else if (get_cpu_type() <= MESON_CPU_MAJOR_ID_GXTVBB) {
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
		val = (READ_VPP_REG(VPP_VADJ2_Y) >> 8) & 0x1ff;
		val = (val << 23) >> 23;

		return sprintf(buf, "%d\n", val);
#endif
	} else if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A) {
		val = (READ_VPP_REG(VPP_VADJ2_Y_2) >> 8) & 0x7ff;
		val = (val << 21) >> 21;

		return sprintf(buf, "%d\n", val);
	}
	val = (READ_VPP_REG(VPP_VADJ2_Y) >> 8) & 0x3ff;
	val = (val << 22) >> 22;

	return sprintf(buf, "%d\n", val << 1);
}

static ssize_t video_adj2_brightness_store(struct class *cla,
					   struct class_attribute *attr,
			const char *buf, size_t count)
{
	size_t r;
	int val;

	r = sscanf(buf, "%d\n", &val);
	if (r != 1 || val < -1024 || val > 1023)
		return -EINVAL;
	amvecm_set_brightness2(val);
	return count;
}

static ssize_t video_adj2_contrast_show(struct class *cla,
					struct class_attribute *attr,
					char *buf)
{
	if (chip_type_id == chip_a4) {
		return sprintf(buf, "%d\n",
			(int)(READ_VPP_REG(VOUT_VADJ_Y) & 0xff) - 0x80);
	} else if (chip_type_id == chip_s5) {
		return sprintf(buf, "%d\n",
			(int)(ve_brightness_contrast_get(VE_VADJ2) & 0xff) - 0x80);
	} else if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A)
		return sprintf(buf, "%d\n",
			(int)(READ_VPP_REG(VPP_VADJ2_Y_2) & 0xff) - 0x80);
	else
		return sprintf(buf, "%d\n",
			(int)(READ_VPP_REG(VPP_VADJ2_Y) & 0xff) - 0x80);
}

static ssize_t video_adj2_contrast_store(struct class *cla,
					 struct class_attribute *attr,
					 const char *buf, size_t count)
{
	size_t r;
	int val;

	r = sscanf(buf, "%d\n", &val);
	if (r != 1 || val < -127 || val > 127)
		return -EINVAL;
	amvecm_set_contrast2(val);
	return count;
}

static ssize_t amvecm_usage_show(struct class *cla,
				 struct class_attribute *attr, char *buf)
{
	pr_info("Usage:");
	pr_info("brightness_val range:-255~255\n");
	pr_info("contrast_val range:-127~127\n");
	pr_info("saturation_val range:-128~128\n");
	pr_info("hue_val range:-25~25\n");
	pr_info("************video brightness & contrast & saturation_hue adj as flow*************\n");
	pr_info("echo brightness_val > /sys/class/amvecm/brightness1\n");
	pr_info("echo contrast_val > /sys/class/amvecm/contrast1\n");
	pr_info("echo saturation_val hue_val > /sys/class/amvecm/saturation_hue_pre\n");
	pr_info("************after video+osd blender, brightness & contrast & saturation_hue adj as flow*************\n");
	pr_info("echo brightness_val > /sys/class/amvecm/brightness2\n");
	pr_info("echo contrast_val > /sys/class/amvecm/contrast2\n");
	pr_info("echo saturation_val hue_val > /sys/class/amvecm/saturation_hue_post\n");
	return 0;
}

static void parse_param_amvecm(char *buf_orig, char **parm)
{
	char *ps, *token;
	unsigned int n = 0;
	char delim1[3] = " ";
	char delim2[2] = "\n";

	ps = buf_orig;
	strcat(delim1, delim2);
	while (1) {
		token = strsep(&ps, delim1);
		if (!token)
			break;
		if (*token == '\0')
			continue;
		parm[n++] = token;
	}
}

static void amvecm_3d_sync_status(void)
{
	unsigned int sync_h_start, sync_h_end, sync_v_start,
		sync_v_end, sync_polarity,
		sync_out_inv, sync_en;
	if (!is_meson_gxtvbb_cpu()) {
		pr_info("\n chip does not support 3D sync process!!!\n");
		return;
	}
	sync_h_start = READ_VPP_REG_BITS(VPU_VPU_3D_SYNC2, 0, 13);
	sync_h_end = READ_VPP_REG_BITS(VPU_VPU_3D_SYNC2, 16, 13);
	sync_v_start = READ_VPP_REG_BITS(VPU_VPU_3D_SYNC1, 0, 13);
	sync_v_end = READ_VPP_REG_BITS(VPU_VPU_3D_SYNC1, 16, 13);
	sync_polarity = READ_VPP_REG_BITS(VPU_VPU_3D_SYNC1, 29, 1);
	sync_out_inv = READ_VPP_REG_BITS(VPU_VPU_3D_SYNC1, 15, 1);
	sync_en = READ_VPP_REG_BITS(VPU_VPU_3D_SYNC1, 31, 1);
	pr_info("\n current 3d sync state:\n");
	pr_info("sync_h_start:%d\n", sync_h_start);
	pr_info("sync_h_end:%d\n", sync_h_end);
	pr_info("sync_v_start:%d\n", sync_v_start);
	pr_info("sync_v_end:%d\n", sync_v_end);
	pr_info("sync_polarity:%d\n", sync_polarity);
	pr_info("sync_out_inv:%d\n", sync_out_inv);
	pr_info("sync_en:%d\n", sync_en);
	pr_info("sync_3d_black_color:%d\n", sync_3d_black_color);
	pr_info("sync_3d_sync_to_vbo:%d\n", sync_3d_sync_to_vbo);
}

static ssize_t amvecm_3d_sync_show(struct class *cla,
				   struct class_attribute *attr, char *buf)
{
	ssize_t len = 0;

	len += sprintf(buf + len,
		"echo hstart val(D) > /sys/class/amvecm/sync_3d\n");
	len += sprintf(buf + len,
		"echo hend val(D) > /sys/class/amvecm/sync_3d\n");
	len += sprintf(buf + len,
		"echo vstart val(D) > /sys/class/amvecm/sync_3d\n");
	len += sprintf(buf + len,
		"echo vend val(D) > /sys/class/amvecm/sync_3d\n");
	len += sprintf(buf + len,
		"echo pola val(D) > /sys/class/amvecm/sync_3d\n");
	len += sprintf(buf + len,
		"echo inv val(D) > /sys/class/amvecm/sync_3d\n");
	len += sprintf(buf + len,
		"echo black_color val(Hex) > /sys/class/amvecm/sync_3d\n");
	len += sprintf(buf + len,
		"echo sync_to_vx1 val(D) > /sys/class/amvecm/sync_3d\n");
	len += sprintf(buf + len,
		"echo enable > /sys/class/amvecm/sync_3d\n");
	len += sprintf(buf + len,
		"echo disable > /sys/class/amvecm/sync_3d\n");
	len += sprintf(buf + len,
		"echo status > /sys/class/amvecm/sync_3d\n");
	return len;
}

static ssize_t amvecm_3d_sync_store(struct class *cla,
				    struct class_attribute *attr,
		const char *buf, size_t count)
{
	char *buf_orig, *parm[8] = {NULL};
	long val;

	if (!buf)
		return count;

	if (!is_meson_gxtvbb_cpu()) {
		pr_info("\n chip does not support 3D sync process!!!\n");
		return count;
	}

	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig)
		return -ENOMEM;
	parse_param_amvecm(buf_orig, (char **)&parm);
	if (!strncmp(parm[0], "hstart", 6)) {
		if (kstrtol(parm[1], 10, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		sync_3d_h_start = val & 0x1fff;
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC2, sync_3d_h_start, 0, 13);
	} else if (!strncmp(parm[0], "hend", 4)) {
		if (kstrtol(parm[1], 10, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		sync_3d_h_end = val & 0x1fff;
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC2, sync_3d_h_end, 16, 13);
	} else if (!strncmp(parm[0], "vstart", 6)) {
		if (kstrtol(parm[1], 10, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		sync_3d_v_start = val & 0x1fff;
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC1, sync_3d_v_start, 0, 13);
	} else if (!strncmp(parm[0], "vend", 4)) {
		if (kstrtol(parm[1], 10, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		sync_3d_v_end = val & 0x1fff;
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC1, sync_3d_v_end, 16, 13);
	} else if (!strncmp(parm[0], "pola", 4)) {
		if (kstrtol(parm[1], 10, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		sync_3d_polarity = val & 0x1;
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC1, sync_3d_polarity, 29, 1);
	} else if (!strncmp(parm[0], "inv", 3)) {
		if (kstrtol(parm[1], 10, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		sync_3d_out_inv = val & 0x1;
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC1, sync_3d_out_inv, 15, 1);
	} else if (!strncmp(parm[0], "black_color", 11)) {
		if (kstrtol(parm[1], 16, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		sync_3d_black_color = val & 0xffffff;
		WRITE_VPP_REG_BITS(VPP_BLEND_ONECOLOR_CTRL,
				   sync_3d_black_color, 0, 24);
	} else if (!strncmp(parm[0], "sync_to_vx1", 11)) {
		if (kstrtol(parm[1], 10, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		sync_3d_sync_to_vbo = val & 0x1;
	} else if (!strncmp(parm[0], "enable", 6)) {
		vecm_latch_flag |= FLAG_3D_SYNC_EN;
	} else if (!strncmp(parm[0], "disable", 7)) {
		vecm_latch_flag |= FLAG_3D_SYNC_DIS;
	} else if (!strncmp(parm[0], "status", 7)) {
		amvecm_3d_sync_status();
	}
	kfree(buf_orig);
	return count;
}

static ssize_t amvecm_vlock_show(struct class *cla,
				 struct class_attribute *attr, char *buf)
{
	return vlock_debug_show(cla, attr, buf);
}

static ssize_t amvecm_vlock_store(struct class *cla,
				  struct class_attribute *attr,
		const char *buf, size_t count)
{
	return vlock_debug_store(cla, attr, buf, count);
}

static ssize_t amvecm_frame_lock_show(struct class *cla,
				 struct class_attribute *attr, char *buf)
{
	return frame_lock_debug_show(cla, attr, buf);
}

static ssize_t amvecm_frame_lock_store(struct class *cla,
				  struct class_attribute *attr,
		const char *buf, size_t count)
{
	return frame_lock_debug_store(cla, attr, buf, count);
}

/* #endif */

unsigned int pr_hist;
/* return lum_ave:
 * -1: no vframe, error ave
 * 0~255: 8bit ave = lum_sum/pixel_sum
 */
static int lum_ave = -1;

int get_lum_ave(void)
{
	return lum_ave;
}
EXPORT_SYMBOL(get_lum_ave);

void set_lum_ave(int ave)
{
	lum_ave = ave;
}

static void vpp_backup_histgram(struct vframe_s *vf)
{
	unsigned int i = 0;
	int ave;

	vpp_hist_param.vpp_hist_pow = vf->prop.hist.hist_pow;
	vpp_hist_param.vpp_luma_sum = vf->prop.hist.vpp_luma_sum;
	vpp_hist_param.vpp_pixel_sum = vf->prop.hist.vpp_pixel_sum;
	for (i = 0; i < 64; i++)
		vpp_hist_param.vpp_histgram[i] = vf->prop.hist.vpp_gamma[i];
	for (i = 0; i < 128; i++)
		vpp_hist_param.hdr_histgram[i] = hdr_hist[NUM_HDR_HIST - 1][i];
	for (i = 0; i < 32; i++)
		vpp_hist_param.hue_histgram[i] = vf->prop.hist.vpp_hue_gamma[i];
	for (i = 0; i < 32; i++)
		vpp_hist_param.sat_histgram[i] = vf->prop.hist.vpp_sat_gamma[i];

	if (vpp_hist_param.vpp_pixel_sum == 0)
		ave = 0;
	else
		ave = vpp_hist_param.vpp_luma_sum / vpp_hist_param.vpp_pixel_sum;

	set_lum_ave(ave);

	if (pr_hist) {
		for (i = 0; i < 8; i++) {
			pr_info("%d, %d, %d, %d, %d, %d, %d, %d\n",
				vpp_hist_param.vpp_histgram[i * 8 + 0],
				vpp_hist_param.vpp_histgram[i * 8 + 1],
				vpp_hist_param.vpp_histgram[i * 8 + 2],
				vpp_hist_param.vpp_histgram[i * 8 + 3],
				vpp_hist_param.vpp_histgram[i * 8 + 4],
				vpp_hist_param.vpp_histgram[i * 8 + 5],
				vpp_hist_param.vpp_histgram[i * 8 + 6],
				vpp_hist_param.vpp_histgram[i * 8 + 7]);
			pr_info("ave = %d\n", ave);
		}
	}
}

static void vpp_dump_histgram(void)
{
	uint i;

	pr_info("%s:\n", __func__);
	if (hdr_source_type == HDRTYPE_HDR10) {
		pr_info("\t dump_hdr_hist begin\n");
		for (i = 0; i < 128; i++) {
			pr_info("[%d]0x%-8x\t", i,
				hdr_hist[NUM_HDR_HIST - 1][i]);
			if ((i + 1) % 8 == 0)
				pr_info("\n");
		}
		pr_info("\t dump_hdr_hist done\n");
	}

	pr_info("\n\t dump_dnlp_hist begin\n");
	for (i = 0; i < 64; i++) {
		pr_info("[%d]0x%-8x\t", i, vpp_hist_param.vpp_histgram[i]);
		if ((i + 1) % 8 == 0)
			pr_info("\n");
	}
	pr_info("\n\t dump_dnlp_hist done\n");
}

void vpp_get_hist_en(void)
{
	if (chip_type_id == chip_a4)
		return;

	if (chip_type_id == chip_s5) {
		vpp_luma_hist_init();
		return;
	}

	if (hist_chl)
		WRITE_VPP_REG_BITS(VI_HIST_CTRL, 0x2, 11, 3);
	else
		WRITE_VPP_REG_BITS(VI_HIST_CTRL, 0x1, 11, 3);
	WRITE_VPP_REG_BITS(VI_HIST_CTRL, 0x1, 0, 1);
	WRITE_VPP_REG(VI_HIST_GCLK_CTRL, 0xffffffff);
	WRITE_VPP_REG_BITS(VI_HIST_CTRL, 2, VI_HIST_POW_BIT, VI_HIST_POW_WID);
}

static unsigned int vpp_luma_max;

void get_cm_hist(struct vframe_s *vf)
{
	int i;
	int addr_port;
	int data_port;

	/*s5 not support color hist*/
	if (chip_type_id == chip_s5)
		return;

	addr_port = VPP_CHROMA_ADDR_PORT;
	data_port = VPP_CHROMA_DATA_PORT;

	if (enable_pattern_detect == 1) {
		for (i = 0; i < 32; i++) {
			WRITE_VPP_REG(addr_port,
				      RO_CM_HUE_HIST_BIN0 + i);
			vf->prop.hist.vpp_hue_gamma[i] =
				READ_VPP_REG(data_port);
		}
		for (i = 0; i < 32; i++) {
			WRITE_VPP_REG(addr_port,
				      RO_CM_SAT_HIST_BIN0 + i);
			vf->prop.hist.vpp_sat_gamma[i] =
				READ_VPP_REG(data_port);
		}
	}
}

void vpp_get_vframe_hist_info(struct vframe_s *vf)
{
	unsigned int hist_height, hist_width;
	u64 divid;

	if (chip_type_id == chip_s5) {
		get_luma_hist(vf);
		return;
	}

	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A) {
		/*TL1 remove VPP_IN_H_V_SIZE register*/
		hist_width = READ_VPP_REG_BITS(VPP_PREBLEND_H_SIZE, 0, 13);
		hist_height = READ_VPP_REG_BITS(VPP_PREBLEND_H_SIZE, 16, 13);
	} else {
		hist_height = READ_VPP_REG_BITS(VPP_IN_H_V_SIZE, 0, 13);
		hist_width = READ_VPP_REG_BITS(VPP_IN_H_V_SIZE, 16, 13);
	}

	if (hist_height != pre_hist_height ||
	    hist_width != pre_hist_width) {
		pre_hist_height = hist_height;
		pre_hist_width = hist_width;
		WRITE_VPP_REG_BITS(VI_HIST_PIC_SIZE, hist_height, 16, 13);
		WRITE_VPP_REG_BITS(VI_HIST_PIC_SIZE, hist_width, 0, 13);
	}
	/* fetch hist info */
	/* vf->prop.hist.luma_sum   = READ_CBUS_REG_BITS(VDIN_HIST_SPL_VAL,*/
	/* HIST_LUMA_SUM_BIT,    HIST_LUMA_SUM_WID   ); */
	vf->prop.hist.hist_pow   =
	READ_VPP_REG_BITS(VI_HIST_CTRL,
			  VI_HIST_POW_BIT, VI_HIST_POW_WID);
	vf->prop.hist.vpp_luma_sum   =
	READ_VPP_REG(VI_HIST_SPL_VAL);
	/* vf->prop.hist.chroma_sum = READ_CBUS_REG_BITS(VDIN_HIST_CHROMA_SUM,*/
	/* HIST_CHROMA_SUM_BIT,  HIST_CHROMA_SUM_WID ); */
	vf->prop.hist.vpp_chroma_sum =
	READ_VPP_REG(VI_HIST_CHROMA_SUM);
	vf->prop.hist.vpp_pixel_sum  =
	READ_VPP_REG_BITS(VI_HIST_SPL_PIX_CNT,
			  VI_HIST_PIX_CNT_BIT, VI_HIST_PIX_CNT_WID);
	vf->prop.hist.vpp_height     =
	READ_VPP_REG_BITS(VI_HIST_PIC_SIZE,
			  VI_HIST_PIC_HEIGHT_BIT, VI_HIST_PIC_HEIGHT_WID);
	vf->prop.hist.vpp_width      =
	READ_VPP_REG_BITS(VI_HIST_PIC_SIZE,
			  VI_HIST_PIC_WIDTH_BIT, VI_HIST_PIC_WIDTH_WID);
	vf->prop.hist.vpp_luma_max   =
	READ_VPP_REG_BITS(VI_HIST_MAX_MIN,
			  VI_HIST_MAX_BIT, VI_HIST_MAX_WID);
	vf->prop.hist.vpp_luma_min   =
	READ_VPP_REG_BITS(VI_HIST_MAX_MIN,
			  VI_HIST_MIN_BIT, VI_HIST_MIN_WID);
	vf->prop.hist.vpp_gamma[0]   =
	READ_VPP_REG_BITS(VI_DNLP_HIST00,
			  VI_HIST_ON_BIN_00_BIT, VI_HIST_ON_BIN_00_WID);
	vf->prop.hist.vpp_gamma[1]   =
	READ_VPP_REG_BITS(VI_DNLP_HIST00,
			  VI_HIST_ON_BIN_01_BIT, VI_HIST_ON_BIN_01_WID);
	vf->prop.hist.vpp_gamma[2]   =
	READ_VPP_REG_BITS(VI_DNLP_HIST01,
			  VI_HIST_ON_BIN_02_BIT, VI_HIST_ON_BIN_02_WID);
	vf->prop.hist.vpp_gamma[3]   =
	READ_VPP_REG_BITS(VI_DNLP_HIST01,
			  VI_HIST_ON_BIN_03_BIT, VI_HIST_ON_BIN_03_WID);
	vf->prop.hist.vpp_gamma[4]   =
	READ_VPP_REG_BITS(VI_DNLP_HIST02,
			  VI_HIST_ON_BIN_04_BIT, VI_HIST_ON_BIN_04_WID);
	vf->prop.hist.vpp_gamma[5]   =
	READ_VPP_REG_BITS(VI_DNLP_HIST02,
			  VI_HIST_ON_BIN_05_BIT, VI_HIST_ON_BIN_05_WID);
	vf->prop.hist.vpp_gamma[6]   =
	READ_VPP_REG_BITS(VI_DNLP_HIST03,
			  VI_HIST_ON_BIN_06_BIT, VI_HIST_ON_BIN_06_WID);
	vf->prop.hist.vpp_gamma[7]   =
	READ_VPP_REG_BITS(VI_DNLP_HIST03,
			  VI_HIST_ON_BIN_07_BIT, VI_HIST_ON_BIN_07_WID);
	vf->prop.hist.vpp_gamma[8]   =
	READ_VPP_REG_BITS(VI_DNLP_HIST04,
			  VI_HIST_ON_BIN_08_BIT, VI_HIST_ON_BIN_08_WID);
	vf->prop.hist.vpp_gamma[9]   =
	READ_VPP_REG_BITS(VI_DNLP_HIST04,
			  VI_HIST_ON_BIN_09_BIT, VI_HIST_ON_BIN_09_WID);
	vf->prop.hist.vpp_gamma[10]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST05,
			  VI_HIST_ON_BIN_10_BIT, VI_HIST_ON_BIN_10_WID);
	vf->prop.hist.vpp_gamma[11]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST05,
			  VI_HIST_ON_BIN_11_BIT, VI_HIST_ON_BIN_11_WID);
	vf->prop.hist.vpp_gamma[12]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST06,
			  VI_HIST_ON_BIN_12_BIT, VI_HIST_ON_BIN_12_WID);
	vf->prop.hist.vpp_gamma[13]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST06,
			  VI_HIST_ON_BIN_13_BIT, VI_HIST_ON_BIN_13_WID);
	vf->prop.hist.vpp_gamma[14]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST07,
			  VI_HIST_ON_BIN_14_BIT, VI_HIST_ON_BIN_14_WID);
	vf->prop.hist.vpp_gamma[15]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST07,
			  VI_HIST_ON_BIN_15_BIT, VI_HIST_ON_BIN_15_WID);
	vf->prop.hist.vpp_gamma[16]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST08,
			  VI_HIST_ON_BIN_16_BIT, VI_HIST_ON_BIN_16_WID);
	vf->prop.hist.vpp_gamma[17]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST08,
			  VI_HIST_ON_BIN_17_BIT, VI_HIST_ON_BIN_17_WID);
	vf->prop.hist.vpp_gamma[18]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST09,
			  VI_HIST_ON_BIN_18_BIT, VI_HIST_ON_BIN_18_WID);
	vf->prop.hist.vpp_gamma[19]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST09,
			  VI_HIST_ON_BIN_19_BIT, VI_HIST_ON_BIN_19_WID);
	vf->prop.hist.vpp_gamma[20]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST10,
			  VI_HIST_ON_BIN_20_BIT, VI_HIST_ON_BIN_20_WID);
	vf->prop.hist.vpp_gamma[21]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST10,
			  VI_HIST_ON_BIN_21_BIT, VI_HIST_ON_BIN_21_WID);
	vf->prop.hist.vpp_gamma[22]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST11,
			  VI_HIST_ON_BIN_22_BIT, VI_HIST_ON_BIN_22_WID);
	vf->prop.hist.vpp_gamma[23]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST11,
			  VI_HIST_ON_BIN_23_BIT, VI_HIST_ON_BIN_23_WID);
	vf->prop.hist.vpp_gamma[24]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST12,
			  VI_HIST_ON_BIN_24_BIT, VI_HIST_ON_BIN_24_WID);
	vf->prop.hist.vpp_gamma[25]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST12,
			  VI_HIST_ON_BIN_25_BIT, VI_HIST_ON_BIN_25_WID);
	vf->prop.hist.vpp_gamma[26]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST13,
			  VI_HIST_ON_BIN_26_BIT, VI_HIST_ON_BIN_26_WID);
	vf->prop.hist.vpp_gamma[27]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST13,
			  VI_HIST_ON_BIN_27_BIT, VI_HIST_ON_BIN_27_WID);
	vf->prop.hist.vpp_gamma[28]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST14,
			  VI_HIST_ON_BIN_28_BIT, VI_HIST_ON_BIN_28_WID);
	vf->prop.hist.vpp_gamma[29]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST14,
			  VI_HIST_ON_BIN_29_BIT, VI_HIST_ON_BIN_29_WID);
	vf->prop.hist.vpp_gamma[30]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST15,
			  VI_HIST_ON_BIN_30_BIT, VI_HIST_ON_BIN_30_WID);
	vf->prop.hist.vpp_gamma[31]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST15,
			  VI_HIST_ON_BIN_31_BIT, VI_HIST_ON_BIN_31_WID);
	vf->prop.hist.vpp_gamma[32]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST16,
			  VI_HIST_ON_BIN_32_BIT, VI_HIST_ON_BIN_32_WID);
	vf->prop.hist.vpp_gamma[33]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST16,
			  VI_HIST_ON_BIN_33_BIT, VI_HIST_ON_BIN_33_WID);
	vf->prop.hist.vpp_gamma[34]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST17,
			  VI_HIST_ON_BIN_34_BIT, VI_HIST_ON_BIN_34_WID);
	vf->prop.hist.vpp_gamma[35]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST17,
			  VI_HIST_ON_BIN_35_BIT, VI_HIST_ON_BIN_35_WID);
	vf->prop.hist.vpp_gamma[36]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST18,
			  VI_HIST_ON_BIN_36_BIT, VI_HIST_ON_BIN_36_WID);
	vf->prop.hist.vpp_gamma[37]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST18,
			  VI_HIST_ON_BIN_37_BIT, VI_HIST_ON_BIN_37_WID);
	vf->prop.hist.vpp_gamma[38]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST19,
			  VI_HIST_ON_BIN_38_BIT, VI_HIST_ON_BIN_38_WID);
	vf->prop.hist.vpp_gamma[39]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST19,
			  VI_HIST_ON_BIN_39_BIT, VI_HIST_ON_BIN_39_WID);
	vf->prop.hist.vpp_gamma[40]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST20,
			  VI_HIST_ON_BIN_40_BIT, VI_HIST_ON_BIN_40_WID);
	vf->prop.hist.vpp_gamma[41]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST20,
			  VI_HIST_ON_BIN_41_BIT, VI_HIST_ON_BIN_41_WID);
	vf->prop.hist.vpp_gamma[42]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST21,
			  VI_HIST_ON_BIN_42_BIT, VI_HIST_ON_BIN_42_WID);
	vf->prop.hist.vpp_gamma[43]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST21,
			  VI_HIST_ON_BIN_43_BIT, VI_HIST_ON_BIN_43_WID);
	vf->prop.hist.vpp_gamma[44]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST22,
			  VI_HIST_ON_BIN_44_BIT, VI_HIST_ON_BIN_44_WID);
	vf->prop.hist.vpp_gamma[45]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST22,
			  VI_HIST_ON_BIN_45_BIT, VI_HIST_ON_BIN_45_WID);
	vf->prop.hist.vpp_gamma[46]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST23,
			  VI_HIST_ON_BIN_46_BIT, VI_HIST_ON_BIN_46_WID);
	vf->prop.hist.vpp_gamma[47]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST23,
			  VI_HIST_ON_BIN_47_BIT, VI_HIST_ON_BIN_47_WID);
	vf->prop.hist.vpp_gamma[48]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST24,
			  VI_HIST_ON_BIN_48_BIT, VI_HIST_ON_BIN_48_WID);
	vf->prop.hist.vpp_gamma[49]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST24,
			  VI_HIST_ON_BIN_49_BIT, VI_HIST_ON_BIN_49_WID);
	vf->prop.hist.vpp_gamma[50]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST25,
			  VI_HIST_ON_BIN_50_BIT, VI_HIST_ON_BIN_50_WID);
	vf->prop.hist.vpp_gamma[51]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST25,
			  VI_HIST_ON_BIN_51_BIT, VI_HIST_ON_BIN_51_WID);
	vf->prop.hist.vpp_gamma[52]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST26,
			  VI_HIST_ON_BIN_52_BIT, VI_HIST_ON_BIN_52_WID);
	vf->prop.hist.vpp_gamma[53]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST26,
			  VI_HIST_ON_BIN_53_BIT, VI_HIST_ON_BIN_53_WID);
	vf->prop.hist.vpp_gamma[54]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST27,
			  VI_HIST_ON_BIN_54_BIT, VI_HIST_ON_BIN_54_WID);
	vf->prop.hist.vpp_gamma[55]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST27,
			  VI_HIST_ON_BIN_55_BIT, VI_HIST_ON_BIN_55_WID);
	vf->prop.hist.vpp_gamma[56]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST28,
			  VI_HIST_ON_BIN_56_BIT, VI_HIST_ON_BIN_56_WID);
	vf->prop.hist.vpp_gamma[57]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST28,
			  VI_HIST_ON_BIN_57_BIT, VI_HIST_ON_BIN_57_WID);
	vf->prop.hist.vpp_gamma[58]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST29,
			  VI_HIST_ON_BIN_58_BIT, VI_HIST_ON_BIN_58_WID);
	vf->prop.hist.vpp_gamma[59]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST29,
			  VI_HIST_ON_BIN_59_BIT, VI_HIST_ON_BIN_59_WID);
	vf->prop.hist.vpp_gamma[60]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST30,
			  VI_HIST_ON_BIN_60_BIT, VI_HIST_ON_BIN_60_WID);
	vf->prop.hist.vpp_gamma[61]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST30,
			  VI_HIST_ON_BIN_61_BIT, VI_HIST_ON_BIN_61_WID);
	vf->prop.hist.vpp_gamma[62]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST31,
			  VI_HIST_ON_BIN_62_BIT, VI_HIST_ON_BIN_62_WID);
	vf->prop.hist.vpp_gamma[63]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST31,
			  VI_HIST_ON_BIN_63_BIT, VI_HIST_ON_BIN_63_WID);

	if (get_cpu_type() == MESON_CPU_MAJOR_ID_T3 ||
		get_cpu_type() == MESON_CPU_MAJOR_ID_T5W) {
		vf->fmeter0_hcnt[0] =
		READ_VPP_REG(SRSHARP0_RO_FMETER_HCNT_TYPE0);
		vf->fmeter0_hcnt[1] =
		READ_VPP_REG(SRSHARP0_RO_FMETER_HCNT_TYPE1);
		vf->fmeter0_hcnt[2] =
		READ_VPP_REG(SRSHARP0_RO_FMETER_HCNT_TYPE2);
		vf->fmeter0_hcnt[3] =
		READ_VPP_REG(SRSHARP0_RO_FMETER_HCNT_TYPE3);
		vf->fmeter1_hcnt[0] =
		READ_VPP_REG(SRSHARP1_RO_FMETER_HCNT_TYPE0);
		vf->fmeter1_hcnt[1] =
		READ_VPP_REG(SRSHARP1_RO_FMETER_HCNT_TYPE1);
		vf->fmeter1_hcnt[2] =
		READ_VPP_REG(SRSHARP1_RO_FMETER_HCNT_TYPE2);
		vf->fmeter1_hcnt[3] =
		READ_VPP_REG(SRSHARP1_RO_FMETER_HCNT_TYPE3);

		vf->fmeter0_score = FMETER_SCORE(vf->fmeter0_hcnt[0],
			vf->fmeter0_hcnt[1], vf->fmeter0_hcnt[2]);

		vf->fmeter1_score = FMETER_SCORE(vf->fmeter1_hcnt[0],
			vf->fmeter1_hcnt[1], vf->fmeter1_hcnt[2]);
	}

	get_cm_hist(vf);

	if (debug_game_mode_1 &&
	    vpp_luma_max != vf->prop.hist.vpp_luma_max) {
		divid = sched_clock();
		vf->ready_clock_hist[1] = sched_clock();
		do_div(divid, 1000);
		pr_info("vpp output done %lld us. luma_max(0x%x-->0x%x)\n",
			divid, vpp_luma_max, vf->prop.hist.vpp_luma_max);
		vpp_luma_max = vf->prop.hist.vpp_luma_max;
	}
}

static void ioctrl_get_hdr_metadata(struct vframe_s *vf)
{
	if (((vf->signal_type >> 16) & 0xff) == 9) {
		if (vf->prop.master_display_colour.present_flag) {
			memcpy(vpp_hdr_metadata_s.primaries,
			       vf->prop.master_display_colour.primaries,
				sizeof(u32) * 6);
			memcpy(vpp_hdr_metadata_s.white_point,
			       vf->prop.master_display_colour.white_point,
				sizeof(u32) * 2);
			vpp_hdr_metadata_s.luminance[0] =
				vf->prop.master_display_colour.luminance[0];
			vpp_hdr_metadata_s.luminance[1] =
				vf->prop.master_display_colour.luminance[1];
		} else {
			memset(vpp_hdr_metadata_s.primaries, 0,
			       sizeof(vpp_hdr_metadata_s.primaries));
		}
	} else {
		memset(vpp_hdr_metadata_s.primaries, 0,
		       sizeof(vpp_hdr_metadata_s.primaries));
	}
}

void vpp_demo_func(struct vframe_s *vf,
	int sps_hsize, int sps_vsize,
	int cm_hsize, int cm_vsize)
{
	struct demo_data_s *p = &demo_data;
	unsigned int val;
	int cm_size_chg = 0;
	int sr_size_chg = 0;

	if (!vf) {
		if (p->en_st[E_DEMO_SR] &&
			!p->update_flag[E_DEMO_SR])
			p->update_flag[E_DEMO_SR] = 1;

		if (p->en_st[E_DEMO_CM] &&
			!p->update_flag[E_DEMO_CM])
			p->update_flag[E_DEMO_CM] = 1;

		if (p->en_st[E_DEMO_LC] &&
			!p->update_flag[E_DEMO_LC])
			p->update_flag[E_DEMO_LC] = 1;

		return;
	}

	/*because frame size delay one frame, add size change*/
	if (p->in_size.cm_hsize != cm_hsize ||
		p->in_size.cm_vsize != cm_vsize) {
		p->in_size.cm_hsize = cm_hsize;
		p->in_size.cm_vsize = cm_vsize;
		cm_size_chg = 1;
	}

	if (p->in_size.sr_hsize != sps_hsize ||
		p->in_size.sr_vsize != sps_vsize) {
		p->in_size.sr_hsize = sps_hsize;
		p->in_size.sr_vsize = sps_vsize;
		sr_size_chg = 1;
	}

	if (p->update_flag[E_DEMO_SR] ||
		(cm_size_chg && p->en_st[E_DEMO_SR])) {
		if (p->en_st[E_DEMO_SR]) {
			val = (2 << 17) |
				(1 << 16) |
				(p->in_size.sr_hsize >> 1);
			VSYNC_WRITE_VPP_REG(SRSHARP0_DEMO_CRTL, val);
			VSYNC_WRITE_VPP_REG(SRSHARP1_DEMO_CRTL, val);
		} else {
			val = (2 << 17) |
				(0 << 16) |
				(p->in_size.sr_hsize >> 1);
			VSYNC_WRITE_VPP_REG(SRSHARP0_DEMO_CRTL, val);
			VSYNC_WRITE_VPP_REG(SRSHARP1_DEMO_CRTL, val);
		}
		p->update_flag[E_DEMO_SR] = 0;
		pr_amvecm_dbg("sr hsize: %d, vsize: %d, sr_demo = %d\n",
			sps_hsize, sps_vsize, p->en_st[E_DEMO_SR]);
	}

	if (p->update_flag[E_DEMO_CM] ||
		(sr_size_chg && p->en_st[E_DEMO_CM])) {
		if (p->en_st[E_DEMO_CM]) {
			val = (p->in_size.cm_hsize >> 1) << 16;
			VSYNC_WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT, 0x209);
			VSYNC_WRITE_VPP_REG(VPP_CHROMA_DATA_PORT, val);

			val = p->in_size.cm_vsize << 16;
			VSYNC_WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT, 0x20a);
			VSYNC_WRITE_VPP_REG(VPP_CHROMA_DATA_PORT, val);

			VSYNC_WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT, 0x20f);
			VSYNC_WRITE_VPP_REG(VPP_CHROMA_DATA_PORT, 0x40);
		} else {
			VSYNC_WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT, 0x20f);
			VSYNC_WRITE_VPP_REG(VPP_CHROMA_DATA_PORT, 0x0);
		}
		p->update_flag[E_DEMO_CM] = 0;
		pr_amvecm_dbg("cm hsize: %d, vsize: %d, cm_demo = %d\n",
			cm_hsize, cm_vsize, p->en_st[E_DEMO_CM]);
	}

	if (p->update_flag[E_DEMO_LC]) {
		if (p->en_st[E_DEMO_LC])
			lc_demo_mode = 1;
		else
			lc_demo_mode = 0;
		p->update_flag[E_DEMO_LC] = 0;
		pr_amvecm_dbg("lc_demo = %d\n", lc_demo_mode);
	}
}

void amvecm_dejaggy_patch(struct vframe_s *vf)
{
	int gmv;

	if (!vf) {
		if (pd_detect_en)
			pd_detect_en = 0;
		return;
	}

	if (!pd_det)
		return;

	if (chip_type_id == chip_s5)
		return;

	gmv = vf->di_gmv / 10000;

	if (vf->height == 1080 &&
	    vf->width == 1920 &&
	    (vf->di_pulldown & (1 << 3)) &&
	    ((vf->di_pulldown & 0x7) || gmv >= gmv_th)) {
		if (pd_detect_en == 1)
			return;
		pd_detect_en = 1;
		pd_combing_fix_patch(pd_fix_lvl);
		pr_amvecm_dbg("pd_detect_en1 = %d; level = %d, vf->di_pulldown = 0x%x, gmv %d\n",
			      pd_detect_en, pd_fix_lvl, vf->di_pulldown, gmv);
	} else if ((vf->height == 1080) &&
		 (vf->width == 1920) &&
		 (vf->di_pulldown & (1 << 3)) &&
		 (gmv >= gmv_weak_th)) {
		if (pd_detect_en == 2)
			return;
		pd_detect_en = 2;

		pd_combing_fix_patch(pd_weak_fix_lvl);
		pr_amvecm_dbg("pd_detect_en2 = %d; level = %d, vf->di_pulldown = 0x%x, gmv %d\n",
			      pd_detect_en, pd_weak_fix_lvl, vf->di_pulldown, gmv);
	} else if (pd_detect_en) {
		pd_detect_en = 0;
		pd_combing_fix_patch(PD_DEF_LVL);
		pr_amvecm_dbg("pd_detect_en = %d; pd_fix_lvl = %d\n",
			      pd_detect_en, pd_fix_lvl);
	}
}

/*sr fmeter interface*/
void amvecm_fmeter_process(struct vframe_s *vf)
{
	u8 fmeter_score_unit, fmeter_score_ten, fmeter_score_hundred;
	uint val;
	uint reg_val;

	if (!vf)
		return;

	if (vf->height <= 1080 && vf->width <= 1920) {
		fmeter_score_hundred = vf->fmeter0_score / 100;
		fmeter_score_ten =
			(vf->fmeter0_score - fmeter_score_hundred * 100) / 10;
		fmeter_score_unit =
			vf->fmeter0_score - fmeter_score_hundred * 100
			- fmeter_score_ten * 10;
		if (fmeter_debug == 3)
			pr_info("fmeter0_hcnt0 = %d,hcnt1 = %d,hcnt2 = %d,hcnt3 = %d\n",
				vf->fmeter0_hcnt[0], vf->fmeter0_hcnt[1],
				vf->fmeter0_hcnt[2], vf->fmeter0_hcnt[3]);
		if (fmeter_debug == 2)
			pr_info("fmeter0_score = %d\n", vf->fmeter0_score);

#ifdef CONFIG_AMLOGIC_MEDIA_FRC
		if (fmeter_debug == 1)
			frc_set_seg_display(1, fmeter_score_hundred,
				fmeter_score_ten, fmeter_score_unit);
		if (fmeter_debug == 0)
			frc_set_seg_display(0, 0, 0, 0);
#endif

		cur_fmeter_level = vf->fmeter0_score / 10;
		if (cur_fmeter_level > 10)
			cur_fmeter_level = 10;
		if (cur_fmeter_level < 0)
			cur_fmeter_level = 0;

		if (cur_fmeter_level == pre_fmeter_level)
			fmeter_flag++;
		else
			fmeter_flag = 0;
	} else {
		fmeter_score_hundred = vf->fmeter1_score / 100;
		fmeter_score_ten =
			(vf->fmeter1_score - fmeter_score_hundred * 100) / 10;
		fmeter_score_unit =
			vf->fmeter1_score - fmeter_score_hundred * 100
			- fmeter_score_ten * 10;
		if (fmeter_debug == 3)
			pr_info("fmeter1_hcnt0 = %d,hcnt1 = %d,hcnt2 = %d,hcnt3 = %d\n",
				vf->fmeter1_hcnt[0], vf->fmeter1_hcnt[1],
				vf->fmeter1_hcnt[2], vf->fmeter1_hcnt[3]);
		if (fmeter_debug == 2)
			pr_info("fmeter1_score = %d\n", vf->fmeter1_score);

#ifdef CONFIG_AMLOGIC_MEDIA_FRC
		if (fmeter_debug == 1)
			frc_set_seg_display(1, fmeter_score_hundred,
				fmeter_score_ten, fmeter_score_unit);
		if (fmeter_debug == 0)
			frc_set_seg_display(0, 0, 0, 0);
#endif

		cur_fmeter_level = vf->fmeter1_score / 10;
		if (cur_fmeter_level > 10)
			cur_fmeter_level = 10;
		if (cur_fmeter_level < 0)
			cur_fmeter_level = 0;

		if (cur_fmeter_level == pre_fmeter_level)
			fmeter_flag++;
		else
			fmeter_flag = 0;
	}

	if (fmeter_flag == fmeter_count && fmeter_en) {
		fmeter_flag = 0;
		if (cur_sr_level < cur_fmeter_level)
			cur_sr_level++;
		else if (cur_sr_level > cur_fmeter_level)
			cur_sr_level--;
		else
			return;

		val = sr1_gain_val[cur_sr_level] << 24 |
			sr1_gain_val[cur_sr_level] << 16 |
			sr1_gain_val[cur_sr_level] << 8 |
			sr1_gain_val[cur_sr_level];
		VSYNC_WRITE_VPP_REG(SRSHARP1_SR7_PKLONG_PF_GAIN, val);

		reg_val = READ_VPP_REG(SRSHARP1_PK_OS_STATIC);

		reg_val &= ~(0x3ff | (0x3ff << 12));
		val = reg_val |
			((sr1_shoot_val[cur_sr_level] & 0x3ff) << 12) |
			(sr1_shoot_val[cur_sr_level] & 0x3ff);

		VSYNC_WRITE_VPP_REG(SRSHARP1_PK_OS_STATIC, val);

		val = sr0_gain_val[cur_sr_level] << 24 |
			sr0_gain_val[cur_sr_level] << 16 |
			sr0_gain_val[cur_sr_level] << 8 |
			sr0_gain_val[cur_sr_level];
		VSYNC_WRITE_VPP_REG(SRSHARP0_SR7_PKLONG_PF_GAIN, val);

		reg_val = READ_VPP_REG(SRSHARP0_PK_OS_STATIC);
		reg_val &= ~(0x3ff | (0x3ff << 12));
		val = reg_val |
			((sr0_shoot_val[cur_sr_level] & 0x3ff) << 12) |
			(sr0_shoot_val[cur_sr_level] & 0x3ff);

		VSYNC_WRITE_VPP_REG(SRSHARP0_PK_OS_STATIC, val);

		reg_val = READ_VPP_REG(SRSHARP1_PK_CON_2CIRHPGAIN_LIMIT);
		reg_val &= ~(0xff << 24);
		val = reg_val |
			((sr1_gain_lmt[cur_sr_level] & 0xff) << 24);

		VSYNC_WRITE_VPP_REG(SRSHARP1_PK_CON_2CIRHPGAIN_LIMIT, val);


		reg_val = READ_VPP_REG(SRSHARP1_PK_CON_2CIRBPGAIN_LIMIT);
		reg_val &= ~(0xff << 24);
		val = reg_val |
			((sr1_gain_lmt[cur_sr_level] & 0xff) << 24);

		VSYNC_WRITE_VPP_REG(SRSHARP1_PK_CON_2CIRBPGAIN_LIMIT, val);

		reg_val = READ_VPP_REG(SRSHARP0_PK_CON_2CIRHPGAIN_LIMIT);
		reg_val &= ~(0xff << 24);
		val = reg_val |
			((sr0_gain_lmt[cur_sr_level] & 0xff) << 24);

		VSYNC_WRITE_VPP_REG(SRSHARP0_PK_CON_2CIRHPGAIN_LIMIT, val);

		reg_val = READ_VPP_REG(SRSHARP0_PK_CON_2CIRBPGAIN_LIMIT);
		reg_val &= ~(0xff << 24);
		val = reg_val |
			((sr0_gain_lmt[cur_sr_level] & 0xff) << 24);

		VSYNC_WRITE_VPP_REG(SRSHARP0_PK_CON_2CIRBPGAIN_LIMIT, val);

		if (get_cpu_type() == MESON_CPU_MAJOR_ID_T3) {
			reg_val = READ_VPP_REG(NN_ADP_CORING);
			reg_val &= ~(0xff << 8);
			val = reg_val |
				((nn_coring[cur_sr_level] & 0xff) << 8);

			VSYNC_WRITE_VPP_REG(NN_ADP_CORING, val);
		}
	}
	pre_fmeter_level = cur_fmeter_level;
}

static int vpp_mtx_update(struct vpp_mtx_info_s *mtx_info)
{
	unsigned int matrix_coef00_01 = 0;
	unsigned int matrix_coef02_10 = 0;
	unsigned int matrix_coef11_12 = 0;
	unsigned int matrix_coef20_21 = 0;
	unsigned int matrix_coef22 = 0;
	unsigned int matrix_clip = 0;
	unsigned int matrix_offset0_1 = 0;
	unsigned int matrix_offset2 = 0;
	unsigned int matrix_pre_offset0_1 = 0;
	unsigned int matrix_pre_offset2 = 0;
	unsigned int matrix_en_ctrl = 0;

	enum vpp_matrix_e mtx_sel;
	unsigned int coef00_01 = 0;
	unsigned int coef02_10 = 0;
	unsigned int coef11_12 = 0;
	unsigned int coef20_21 = 0;
	unsigned int coef22 = 0;
	unsigned int clip = 0;
	unsigned int offset0_1 = 0;
	unsigned int offset2 = 0;
	unsigned int pre_offset0_1 = 0;
	unsigned int pre_offset2 = 0;
	unsigned int en = 0;

	if (!(vecm_latch_flag2 & (VPP_MARTIX_UPDATE | VPP_MARTIX_GET)))
		return 0;

	mtx_sel = mtx_info->mtx_sel;
	switch (mtx_sel) {
	case VD1_MTX:
		matrix_coef00_01 = VPP_VD1_MATRIX_COEF00_01;
		matrix_coef02_10 = VPP_VD1_MATRIX_COEF02_10;
		matrix_coef11_12 = VPP_VD1_MATRIX_COEF11_12;
		matrix_coef20_21 = VPP_VD1_MATRIX_COEF20_21;
		matrix_coef22 = VPP_VD1_MATRIX_COEF22;
		matrix_clip = VPP_VD1_MATRIX_CLIP;
		matrix_offset0_1 = VPP_VD1_MATRIX_OFFSET0_1;
		matrix_offset2 = VPP_VD1_MATRIX_OFFSET2;
		matrix_pre_offset0_1 = VPP_VD1_MATRIX_PRE_OFFSET0_1;
		matrix_pre_offset2 = VPP_VD1_MATRIX_PRE_OFFSET2;
		matrix_en_ctrl = VPP_VD1_MATRIX_EN_CTRL;
		break;
	case POST2_MTX:
		matrix_coef00_01 = VPP_POST2_MATRIX_COEF00_01;
		matrix_coef02_10 = VPP_POST2_MATRIX_COEF02_10;
		matrix_coef11_12 = VPP_POST2_MATRIX_COEF11_12;
		matrix_coef20_21 = VPP_POST2_MATRIX_COEF20_21;
		matrix_coef22 = VPP_POST2_MATRIX_COEF22;
		matrix_clip = VPP_POST2_MATRIX_CLIP;
		matrix_offset0_1 = VPP_POST2_MATRIX_OFFSET0_1;
		matrix_offset2 = VPP_POST2_MATRIX_OFFSET2;
		matrix_pre_offset0_1 = VPP_POST2_MATRIX_PRE_OFFSET0_1;
		matrix_pre_offset2 = VPP_POST2_MATRIX_PRE_OFFSET2;
		matrix_en_ctrl = VPP_POST2_MATRIX_EN_CTRL;
		break;
	case POST_MTX:
		matrix_coef00_01 = VPP_POST_MATRIX_COEF00_01;
		matrix_coef02_10 = VPP_POST_MATRIX_COEF02_10;
		matrix_coef11_12 = VPP_POST_MATRIX_COEF11_12;
		matrix_coef20_21 = VPP_POST_MATRIX_COEF20_21;
		matrix_coef22 = VPP_POST_MATRIX_COEF22;
		matrix_clip = VPP_POST_MATRIX_CLIP;
		matrix_offset0_1 = VPP_POST_MATRIX_OFFSET0_1;
		matrix_offset2 = VPP_POST_MATRIX_OFFSET2;
		matrix_pre_offset0_1 = VPP_POST_MATRIX_PRE_OFFSET0_1;
		matrix_pre_offset2 = VPP_POST_MATRIX_PRE_OFFSET2;
		matrix_en_ctrl = VPP_POST_MATRIX_EN_CTRL;
		break;
	case VPP1_POST2_MTX:
		matrix_coef00_01 = VPP1_POST2_MATRIX_COEF00_01;
		matrix_coef02_10 = VPP1_POST2_MATRIX_COEF02_10;
		matrix_coef11_12 = VPP1_POST2_MATRIX_COEF11_12;
		matrix_coef20_21 = VPP1_POST2_MATRIX_COEF20_21;
		matrix_coef22 = VPP1_POST2_MATRIX_COEF22;
		matrix_clip = VPP1_POST2_MATRIX_CLIP;
		matrix_offset0_1 = VPP1_POST2_MATRIX_OFFSET0_1;
		matrix_offset2 = VPP1_POST2_MATRIX_OFFSET2;
		matrix_pre_offset0_1 = VPP1_POST2_MATRIX_PRE_OFFSET0_1;
		matrix_pre_offset2 = VPP1_POST2_MATRIX_PRE_OFFSET2;
		matrix_en_ctrl = VPP1_POST2_MATRIX_EN_CTRL;
		break;
	case VPP2_POST2_MTX:
		matrix_coef00_01 = VPP2_POST2_MATRIX_COEF00_01;
		matrix_coef02_10 = VPP2_POST2_MATRIX_COEF02_10;
		matrix_coef11_12 = VPP2_POST2_MATRIX_COEF11_12;
		matrix_coef20_21 = VPP2_POST2_MATRIX_COEF20_21;
		matrix_coef22 = VPP2_POST2_MATRIX_COEF22;
		matrix_clip = VPP2_POST2_MATRIX_CLIP;
		matrix_offset0_1 = VPP2_POST2_MATRIX_OFFSET0_1;
		matrix_offset2 = VPP2_POST2_MATRIX_OFFSET2;
		matrix_pre_offset0_1 = VPP2_POST2_MATRIX_PRE_OFFSET0_1;
		matrix_pre_offset2 = VPP2_POST2_MATRIX_PRE_OFFSET2;
		matrix_en_ctrl = VPP2_POST2_MATRIX_EN_CTRL;
		break;

	case MTX_NULL:
	default:
		break;
	}

	if (!mtx_sel) {
		vecm_latch_flag2 &= ~VPP_MARTIX_UPDATE;
		vecm_latch_flag2 &= ~VPP_MARTIX_GET;
		return 0;
	}

	if (vecm_latch_flag2 & VPP_MARTIX_UPDATE) {
		pre_offset0_1 =
			(mtx_info->mtx_coef.pre_offset[0] << 16) |
			mtx_info->mtx_coef.pre_offset[1];
		pre_offset2 = mtx_info->mtx_coef.pre_offset[2];

		coef00_01 =
			(mtx_info->mtx_coef.matrix_coef[0][0] << 16) |
			mtx_info->mtx_coef.matrix_coef[0][1];
		coef02_10 =
			(mtx_info->mtx_coef.matrix_coef[0][2] << 16) |
			mtx_info->mtx_coef.matrix_coef[1][0];
		coef11_12 =
			(mtx_info->mtx_coef.matrix_coef[1][1] << 16) |
			mtx_info->mtx_coef.matrix_coef[1][2];
		coef20_21 =
			(mtx_info->mtx_coef.matrix_coef[2][0] << 16) |
			mtx_info->mtx_coef.matrix_coef[2][1];
		coef22 = mtx_info->mtx_coef.matrix_coef[2][2];

		offset0_1 =
			(mtx_info->mtx_coef.post_offset[0] << 16) |
			mtx_info->mtx_coef.post_offset[1];
		offset2 = mtx_info->mtx_coef.post_offset[2];

		en = mtx_info->mtx_coef.en;
		clip = mtx_info->mtx_coef.right_shift;

		VSYNC_WRITE_VPP_REG(matrix_coef00_01, coef00_01);
		VSYNC_WRITE_VPP_REG(matrix_coef02_10, coef02_10);
		VSYNC_WRITE_VPP_REG(matrix_coef11_12, coef11_12);
		VSYNC_WRITE_VPP_REG(matrix_coef20_21, coef20_21);
		VSYNC_WRITE_VPP_REG(matrix_coef22, coef22);
		VSYNC_WRITE_VPP_REG(matrix_offset0_1, offset0_1);
		VSYNC_WRITE_VPP_REG(matrix_offset2, offset2);
		VSYNC_WRITE_VPP_REG(matrix_pre_offset0_1, pre_offset0_1);
		VSYNC_WRITE_VPP_REG(matrix_pre_offset2, pre_offset2);
		VSYNC_WRITE_VPP_REG_BITS(matrix_en_ctrl, en, 0, 1);
		VSYNC_WRITE_VPP_REG_BITS(matrix_clip, clip, 5, 3);
		vecm_latch_flag2 &= ~VPP_MARTIX_UPDATE;
	}

	if (vecm_latch_flag2 & VPP_MARTIX_GET) {
		coef00_01 = READ_VPP_REG(matrix_coef00_01);
		coef02_10 = READ_VPP_REG(matrix_coef02_10);
		coef11_12 = READ_VPP_REG(matrix_coef11_12);
		coef20_21 = READ_VPP_REG(matrix_coef20_21);
		coef22 = READ_VPP_REG(matrix_coef22);
		pre_offset0_1 = READ_VPP_REG(matrix_pre_offset0_1);
		pre_offset2 = READ_VPP_REG(matrix_pre_offset2);
		offset0_1 = READ_VPP_REG(matrix_offset0_1);
		offset2 = READ_VPP_REG(matrix_offset2);
		en = READ_VPP_REG_BITS(matrix_en_ctrl, 0, 1);
		clip = READ_VPP_REG_BITS(matrix_clip, 5, 3);
		mtx_info->mtx_coef.pre_offset[0] =
			(u16)((pre_offset0_1 >> 16) & 0xffff);
		mtx_info->mtx_coef.pre_offset[1] =
			(u16)(pre_offset0_1 & 0xffff);
		mtx_info->mtx_coef.pre_offset[2] = (u16)(pre_offset2 & 0xffff);
		mtx_info->mtx_coef.matrix_coef[0][0] =
			(u16)((coef00_01 >> 16) & 0xffff);
		mtx_info->mtx_coef.matrix_coef[0][1] =
			(u16)(coef00_01 & 0xffff);
		mtx_info->mtx_coef.matrix_coef[0][2] =
			(u16)((coef02_10 >> 16) & 0xffff);
		mtx_info->mtx_coef.matrix_coef[1][0] =
			(u16)(coef02_10 & 0xffff);
		mtx_info->mtx_coef.matrix_coef[1][1] =
			(u16)((coef11_12 >> 16) & 0xffff);
		mtx_info->mtx_coef.matrix_coef[1][2] =
			(u16)(coef11_12 & 0xffff);
		mtx_info->mtx_coef.matrix_coef[2][0] =
			(u16)((coef20_21 >> 16) & 0xffff);
		mtx_info->mtx_coef.matrix_coef[2][1] =
			(u16)(coef20_21 & 0xffff);
		mtx_info->mtx_coef.matrix_coef[2][2] =
			(u16)(coef22 & 0xffff);
		mtx_info->mtx_coef.post_offset[0] =
			(u16)((offset0_1 >> 16) & 0xffff);
		mtx_info->mtx_coef.post_offset[1] =
			(u16)(offset0_1 & 0xffff);
		mtx_info->mtx_coef.post_offset[2] =
			(u16)(offset2 & 0xffff);
		mtx_info->mtx_coef.en = en;
		mtx_info->mtx_coef.right_shift = clip;
		vecm_latch_flag2 &= ~VPP_MARTIX_GET;
	}

	return 0;
}

void pre_gma_update(struct pre_gamma_table_s *pre_gma_lut)
{
	if (vecm_latch_flag2 & VPP_PRE_GAMMA_UPDATE) {
		set_pre_gamma_reg(pre_gma_lut);
		vecm_latch_flag2 &= ~VPP_PRE_GAMMA_UPDATE;
	}
}

void eye_prot_update(struct eye_protect_s *eye_prot)
{
	if (vecm_latch_flag2 & VPP_EYE_PROTECT_UPDATE) {
		eye_proc(eye_prot->mtx_ep, eye_prot->en);
		vecm_latch_flag2 &= ~VPP_EYE_PROTECT_UPDATE;
	}
}

void amvecm_saturation_hue_update(int offset_val)
{
	sat_hue_offset_val = offset_val;
	pq_user_latch_flag |= PQ_USER_CMS_SAT_HUE;
	if (debug_amvecm & 8)
		pr_info("[amvecm..] %s: saturation_hue:%d offset=%d\n",
		__func__,
		vdj_mode_s.saturation_hue,
		sat_hue_offset_val);
}

u32 _get_cur_enc_line(void)
{
	u32 ret = 0;

	switch (READ_VPP_REG(VPU_VIU_VENC_MUX_CTRL) & 0x3) {
	case 0:
		ret = (READ_VPP_REG(ENCL_INFO_READ) >> 16) & 0x1fff;
		break;
	case 1:
		ret = (READ_VPP_REG(ENCI_INFO_READ) >> 16) & 0x1fff;
		break;
	case 2:
		ret = (READ_VPP_REG(ENCP_INFO_READ) >> 16) & 0x1fff;
		break;
	case 3:
		ret = (READ_VPP_REG(ENCT_INFO_READ) >> 16) & 0x1fff;
		break;
	}
	return ret;
}

void bs_ct_update(void)
{
	unsigned long flags = 0;

	if (vecm_latch_flag2 & (LUT3D_UPDATE | BS_UPDATE | CT_UPDATE)) {
		spin_lock_irqsave(&vpp_3dlut_lock, flags);
		lut3d_set_api();
		spin_unlock_irqrestore(&vpp_3dlut_lock, flags);
	}
}

void lut3d_latch(void)
{
	vecm_latch_flag2 |= LUT3D_UPDATE;
}

void bs_latch(void)
{
	vecm_latch_flag2 |= BS_UPDATE;
}

void ct_latch(void)
{
	vecm_latch_flag2 |= CT_UPDATE;
}

void amvecm_video_latch(void)
{
	unsigned int temp;
	pc_mode_process();
	cm_latch_process();
	/*amvecm_size_patch();*/
	ve_dnlp_latch_process();
	/*venc*/
	if (cpu_after_eq_t7())
		temp = vpp_get_vout_viu_mux();
	else
		temp = vpp_get_encl_viu_mux();
	if (temp)
		ve_lcd_gamma_process();
	lvds_freq_process();
/* #if (MESON_CPU_TYPE >= MESON_CPU_TYPE_MESONG9TV) */
	if (0) {
		amvecm_3d_sync_process();
		amvecm_3d_black_process();
	}
/* #endif */
	pq_user_latch_process();
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1))
		ve_lc_latch_process();

	/* ioc vadj1/2 switch */
	amvecm_vadj_latch_process();
	/*matrix wr & rd latch */
	vpp_mtx_update(&mtx_info);
	pre_gma_update(&pre_gamma);
	eye_prot_update(&eye_protect);

	bs_ct_update();
}

static void amvecm_overscan_process(struct vframe_s *vf,
				    struct vframe_s *toggle_vf,
				    int flags,
				    enum vd_path_e vd_path)
{
	if (vd_path != VD1_PATH)
		return;
	if (flags & CSC_FLAG_CHECK_OUTPUT) {
		if (toggle_vf)
			amvecm_fresh_overscan(toggle_vf);
		else if (vf)
			amvecm_fresh_overscan(vf);
		return;
	}
	if (!toggle_vf && !vf)
		amvecm_reset_overscan();

	if (vf)
		amvecm_fresh_overscan(vf);
	else
		amvecm_reset_overscan();
}

static int cabc_add_hist_proc(struct vframe_s *vf)
{
	int *hist;
	int i;

	hist = vf_hist_get();

	if (chip_type_id == chip_s5)
		return 0;

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_T7)) {
		vpp_pst_hist_sta_read(hist);
		return 1;
	}

	if (vf) {
		for (i = 0; i < 64; i++)
			hist[i] = vf->prop.hist.vpp_gamma[i];
	} else {
		/*default 1080p linear hist*/
		for (i = 0; i < 64; i++)
			hist[i] = 1012;
	}

	return 1;
}

static int cabc_aad_on_vs(int vf_state)
{
	int cabc_en;

	cabc_en = fw_en_get();

	if (!vf_state)
		return 0;

	if (cabc_en)
		queue_work(aml_cabc_queue, &cabc_proc_work);
	else
		queue_work(aml_cabc_queue, &cabc_bypass_work);

	return 0;
}

#ifdef T7_BRINGUP_MULTI_VPP
int min_vpp_process(int vpp_top_index, enum vpp_index_e vpp_index)
{
	int result = 0;
	//write csc and gain/offset for vpp1/2 here
	// update hdr matrix
	result = amvecm_matrix_process(toggle_vf, vf, flags, vd_path, vpp_index);
	// to do

	return result
}
#endif

int amvecm_on_vs(struct vframe_s *vf,
		 struct vframe_s *toggle_vf,
		 int flags,
		 unsigned int sps_h_en,
		 unsigned int sps_v_en,
		 unsigned int sps_w_in,
		 unsigned int sps_h_in,
		 unsigned int cm_in_w,
		 unsigned int cm_in_h,
		 enum vd_path_e vd_path,
		 enum vpp_index_e vpp_index)
{
	int result = 0;
	int vf_state = 0;
	int stt;
	int ndt;

	stt = _get_cur_enc_line();

#ifdef CONFIG_AMLOGIC_MEDIA_VPP
		struct vpp_dev_s *vpp_devp = get_vpp_dev();

		struct vpp_vf_param_s vf_param;
		enum vpp_vd_path_e vd_paths = EN_VD1_PATH;
		enum vpp_vf_top_e vpp_tops = EN_VF_TOP0;

		vf_param.cm_in_h = cm_in_h;
		vf_param.cm_in_w = cm_in_w;
		vf_param.sps_h_en = sps_h_en;
		vf_param.sps_h_in = sps_h_in;
		vf_param.sps_v_en = sps_v_en;
		vf_param.sps_w_in = sps_w_in;

		if (vpp_devp) {
			if (vpp_devp->probe_ok == 1)
				vpp_vf_proc(vf, toggle_vf, &vf_param, flags, vd_paths, vpp_tops);
		}
#endif

#ifdef T7_BRINGUP_MULTI_VPP
	// to do, t7 vecm bringup,
	int vpp_top_index = 0;
	// temp flag, should update it from video display module in future
	// assuming here that post process only be used for vd1 input of vpp top0
	// and there is no post process for vpp top 1/2
#endif
	if (probe_ok == 0)
		return 0;

	if (vd_path == VD1_PATH)
		set_vpp_enh_clk(toggle_vf, vf);

	if (is_meson_s5_cpu() && !ai_color_enable)
		disable_ai_color();

#ifdef T7_BRINGUP_MULTI_VPP
	// todo, will not support in pxp bringup stage
	if (get_cpu_type() == MESON_CPU_MAJOR_ID_T7 &&
	    vpp_top_index != 0) {
		// for t7 case, for min vpp 1 & 2
		// keep the legacy driver for vd1 not changed for back compatible
		//and try to minimum the changes and add independent handler for min vpp newly added
		result = min_vpp_process(vpp_top_index, vpp_index);
		return result;
	}
#endif
	amvecm_overscan_process(vf, toggle_vf, flags, vd_path);
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	if (for_amdv_certification() && vd_path == VD1_PATH)
		return 0;
#endif
	if (!dnlp_insmod_ok && vd_path == VD1_PATH)
		dnlp_alg_param_init();

	if (flags & CSC_FLAG_CHECK_OUTPUT) {
		/* to test if output will change */
		return amvecm_matrix_process(toggle_vf, vf, flags, vd_path, vpp_index);
	} else if (vd_path == VD1_PATH) {
		send_hdr10_plus_pkt(vd_path, vpp_index);
		send_cuva_pkt(vd_path, vpp_index);
	}
	if ((toggle_vf) || (vf)) {
		/* matrix adjust */
		result = amvecm_matrix_process(toggle_vf, vf, flags, vd_path, vpp_index);
		if (toggle_vf) {
			ioctrl_get_hdr_metadata(toggle_vf);
			vf_state = cabc_add_hist_proc(toggle_vf);
		}

		if (vd_path == VD1_PATH)
			amvecm_size_patch(toggle_vf ? toggle_vf : vf, cm_in_w, cm_in_h);

		if (toggle_vf && vd_path == VD1_PATH) {
			lc_process(toggle_vf, sps_h_en, sps_v_en,
				   sps_w_in, sps_h_in);
			/*1080i pulldown combing workaround*/
			amvecm_dejaggy_patch(toggle_vf);
			if ((get_cpu_type() == MESON_CPU_MAJOR_ID_T3) ||
				(get_cpu_type() == MESON_CPU_MAJOR_ID_T5W)) {
				/*frequence meter size config*/
				amve_fmetersize_config(vf->width, vf->height,
					sps_w_in, sps_h_in);
				amvecm_fmeter_process(toggle_vf);
			}
		}
		/*refresh vframe*/
		if (!toggle_vf && vf) {
			if (vd_path == VD1_PATH) {
				lc_process(vf, sps_h_en, sps_v_en,
				   sps_w_in, sps_h_in);
				vf_state = cabc_add_hist_proc(vf);
			}
		}

		vpp_demo_func(toggle_vf ? toggle_vf : vf,
			sps_w_in, sps_h_in, cm_in_w, cm_in_h);
	} else {
		result = amvecm_matrix_process(NULL, NULL, flags, vd_path, vpp_index);
		if (vd_path == VD1_PATH) {
			lc_process(NULL, sps_h_en, sps_v_en,
				   sps_w_in, sps_h_in);
			/*1080i pulldown combing workaround*/
			amvecm_dejaggy_patch(NULL);
			amvecm_size_patch(NULL, 0, 0);
			vpp_demo_func(NULL, 0, 0, 0, 0);
		}
		vf_state = cabc_add_hist_proc(NULL);
	}

	if (vd_path != VD1_PATH)
		return result;

	/* add some flag to trigger */
	if (vf) {
		/*gxlx sharpness adaptive setting*/
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
		if (is_meson_gxlx_cpu())
			amve_sharpness_adaptive_setting(vf,
							sps_h_en, sps_v_en);
#endif
		amvecm_bricon_process(vd1_brightness,
				      vd1_contrast + vd1_contrast_offset, vf);

		amvecm_color_process(saturation_pre + saturation_offset,
				     hue_pre, vf);
	}

	/* pq latch process */
	amvecm_video_latch();
	/*wq for cacb and aad*/
	cabc_aad_on_vs(vf_state);

	ndt = _get_cur_enc_line();

	if (rd_vencl) {
		pr_info("startline = %d, endline = %d, diff = %d\n", stt, ndt, ndt - stt);
		rd_vencl--;
	}

	return result;
}
EXPORT_SYMBOL(amvecm_on_vs);

void refresh_on_vs(struct vframe_s *vf, struct vframe_s *rpt_vf)
{
	int ave = -1;
#ifdef CONFIG_AMLOGIC_MEDIA_VPP
	struct vpp_dev_s *vpp_devp = get_vpp_dev();

	if (vpp_devp) {
		if (vpp_devp->probe_ok == 1) {
			vpp_vf_refresh(vf, rpt_vf);
			return;
		}
	}
#endif

	if (probe_ok == 0)
		return;

#ifdef T7_BRINGUP_MULTI_VPP
	if (get_cpu_type() == MESON_CPU_MAJOR_ID_T7 &&
	    vpp_top_index != 0) {
		return 0;
	}
#endif

	if (vf || rpt_vf) {
		vpp_get_vframe_hist_info(vf ? vf : rpt_vf);
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
		if (!for_amdv_certification())
#endif
			ve_on_vs(vf ? vf : rpt_vf);
		if (is_video_layer_on(VD1_PATH)) {
			ve_hist_gamma_tgt(vf ? vf : rpt_vf);
			vpp_backup_histgram(vf ? vf : rpt_vf);
		}
		pattern_detect(vf ? vf : rpt_vf);
	} else {
		ve_hist_gamma_reset();
		set_lum_ave(ave);
	}
}
EXPORT_SYMBOL(refresh_on_vs);

static irqreturn_t amvecm_viu2_vsync_isr(int irq, void *dev_id)
{
	if (vpp_get_encl_viu_mux() == 2)
		ve_lcd_gamma_process();
	return IRQ_HANDLED;
}

static irqreturn_t amvecm_lc_curve_isr(int irq, void *dev_id)
{
	if (use_lc_curve_isr)
		lc_read_region(8, 12);
	return IRQ_HANDLED;
}

static int amvecm_open(struct inode *inode, struct file *file)
{
	struct amvecm_dev_s *devp;
	/* Get the per-device structure that contains this cdev */
	devp = container_of(inode->i_cdev, struct amvecm_dev_s, cdev);
	file->private_data = devp;
	/*init queue*/
	init_waitqueue_head(&devp->hdr_queue);
	return 0;
}

static int amvecm_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

static struct am_regs_s amregs_ext;
struct ve_pq_overscan_s overscan_table[TIMING_MAX];

static int parse_para_pq(const char *para, int para_num, int *result)
{
	char *token = NULL;
	char *params, *params_base;
	int *out = result;
	int len = 0, count = 0;
	int res = 0;

	if (!para)
		return 0;

	params = kstrdup(para, GFP_KERNEL);
	if (!params)
		return -ENOMEM;
	params_base = params;
	token = params;
	len = strlen(token);
	do {
		token = strsep(&params, " ");
		while (token && (isspace(*token) ||
				 !isgraph(*token)) && len) {
			token++;
			len--;
		}
		if (len == 0)
			break;
		if (!token || kstrtoint(token, 0, &res) < 0)
			break;
		len = strlen(token);
		*out++ = res;
		count++;
	} while ((token) && (count < para_num) && (len > 0));

	kfree(params_base);
	return count;
}

int amvecm_set_saturation_hue(int mab)
{
	s16 mc = 0, md = 0;
	s16 ma, mb;

	if (mab & 0xfc00fc00)
		return -EINVAL;
	ma = (s16)((mab << 6) >> 22);
	mb = (s16)((mab << 22) >> 22);

	saturation_ma = ma - 0x100;
	saturation_mb = mb;

	ma += saturation_ma_shift;
	mb += saturation_mb_shift;
	if (ma > 511)
		ma = 511;
	if (ma < -512)
		ma = -512;
	if (mb > 511)
		mb = 511;
	if (mb < -512)
		mb = -512;
	mab =  ((ma & 0x3ff) << 16) | (mb & 0x3ff);

	if (chip_type_id == chip_s5)
		ve_color_mab_set(mab, VE_VADJ1, WR_DMA);
	else if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A)
		WRITE_VPP_REG(VPP_VADJ1_MA_MB_2, mab);
	else
		WRITE_VPP_REG(VPP_VADJ1_MA_MB, mab);

	mc = (s16)((mab << 22) >> 22); /* mc = -mb */
	mc = 0 - mc;
	if (mc > 511)
		mc = 511;
	if (mc <  -512)
		mc = -512;
	md = (s16)((mab << 6) >> 22);  /* md =  ma; */
	mab = ((mc & 0x3ff) << 16) | (md & 0x3ff);

	if (chip_type_id == chip_s5) {
		ve_color_mcd_set(mab, VE_VADJ1, WR_DMA);
	} else if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A) {
		WRITE_VPP_REG(VPP_VADJ1_MC_MD_2, mab);
		WRITE_VPP_REG_BITS(VPP_VADJ1_MISC, 1, 0, 1);
	} else {
		WRITE_VPP_REG(VPP_VADJ1_MC_MD, mab);
		WRITE_VPP_REG_BITS(VPP_VADJ_CTRL, 1, 0, 1);
	}
	pr_amvecm_dbg("%s set video_saturation_hue OK!!!\n", __func__);
	return 0;
}

static int amvecm_set_saturation_hue_post(int val1,
					  int val2)
{
	int i, ma, mb, mab, mc, md;
	int hue_cos_len, hue_sin_len;
	int hue_cos[] = {
		/*0~12*/
		256, 256, 256, 255, 255, 254, 253, 252, 251, 250,
		248, 247, 245, 243, 241, 239, 237, 234, 231, 229,
		226, 223, 220, 216, 213, 209  /*13~25*/
	};
	int hue_sin[] = {
		-147, -142, -137, -132, -126, -121, -115, -109, -104,
		-98, -92, -86, -80, /*-25~-13*/-74,  -68,  -62,  -56,
		-50,  -44,  -38,  -31,  -25, -19, -13,  -6, /*-12~-1*/
		0, /*0*/
		6,   13,   19,	25,   31,   38,   44,	50,   56,
		62,	68,  74,      /*1~12*/	80,   86,   92,	98,  104,
		109,  115,  121,  126,  132, 137, 142, 147 /*13~25*/
	};
	hue_cos_len = sizeof(hue_cos) / sizeof(int);
	hue_sin_len = sizeof(hue_sin) / sizeof(int);
	i = (val2 > 0) ? val2 : -val2;
	if (val1 < -128 || val1 > 128 ||
	    val2 < -25 || val2 > 25 ||
	    i >= hue_cos_len ||
	    (val2 >= (hue_sin_len - 25)))
		return -EINVAL;
	saturation_post = val1;
	hue_post = val2;
	ma = (hue_cos[i] * (saturation_post + 128)) >> 7;
	mb = (hue_sin[25 + hue_post] * (saturation_post + 128)) >> 7;
	if (ma > 511)
		ma = 511;
	if (ma < -512)
		ma = -512;
	if (mb > 511)
		mb = 511;
	if (mb < -512)
		mb = -512;
	mab =  ((ma & 0x3ff) << 16) | (mb & 0x3ff);
	pr_info("\n[amvideo..] saturation_post:%d hue_post:%d mab:%x\n",
		saturation_post, hue_post, mab);

	if (chip_type_id == chip_a4)
		WRITE_VPP_REG(VOUT_VADJ_MA_MB, mab);
	else if (chip_type_id == chip_s5)
		ve_color_mab_set(mab, VE_VADJ2, WR_VCB);
	else if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A)
		WRITE_VPP_REG(VPP_VADJ2_MA_MB_2, mab);
	else
		WRITE_VPP_REG(VPP_VADJ2_MA_MB, mab);
	mc = (s16)((mab << 22) >> 22); /* mc = -mb */
	mc = 0 - mc;
	if (mc > 511)
		mc = 511;
	if (mc < -512)
		mc = -512;
	md = (s16)((mab << 6) >> 22);  /* md =	ma; */
	mab = ((mc & 0x3ff) << 16) | (md & 0x3ff);

	if (chip_type_id == chip_a4) {
		WRITE_VPP_REG(VOUT_VADJ_MC_MD, mab);
	} else if (chip_type_id == chip_s5) {
		ve_color_mcd_set(mab, VE_VADJ2, WR_VCB);
	} else if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A) {
		WRITE_VPP_REG(VPP_VADJ2_MC_MD_2, mab);
		WRITE_VPP_REG_BITS(VPP_VADJ2_MISC, 1, 0, 1);
	} else {
		WRITE_VPP_REG(VPP_VADJ2_MC_MD, mab);
		WRITE_VPP_REG_BITS(VPP_VADJ_CTRL, 1, 2, 1);
	}
	return 0;
}

static int gamma_table_compare(struct tcon_gamma_table_s *table1,
			       struct tcon_gamma_table_s *table2)
{
	int i = 0, flag = 0;

	for (i = 0; i < 256; i++)
		if (table1->data[i] != table2->data[i]) {
			flag = 1;
			break;
		}

	return flag;
}

static void parse_overscan_table(unsigned int length,
				 struct ve_pq_table_s *amvecm_pq_load_table)
{
	unsigned int i;
	unsigned int offset = TIMING_UHD + 1;

	memset(overscan_table, 0, sizeof(overscan_table));
	for (i = 0; i < length; i++) {
		overscan_table[i].load_flag =
			(amvecm_pq_load_table[i].src_timing >> 31) & 0x1;
		overscan_table[i].afd_enable =
			(amvecm_pq_load_table[i].src_timing >> 30) & 0x1;
		overscan_table[i].screen_mode =
			(amvecm_pq_load_table[i].src_timing >> 24) & 0x3f;
		overscan_table[i].source =
			(amvecm_pq_load_table[i].src_timing >> 16) & 0xff;
		overscan_table[i].timing =
			amvecm_pq_load_table[i].src_timing & 0xffff;
		overscan_table[i].hs =
			amvecm_pq_load_table[i].value1 & 0xffff;
		overscan_table[i].he =
			(amvecm_pq_load_table[i].value1 >> 16) & 0xffff;
		overscan_table[i].vs =
			amvecm_pq_load_table[i].value2 & 0xffff;
		overscan_table[i].ve =
				(amvecm_pq_load_table[i].value2 >> 16) & 0xffff;
	}
	/* overscan reset for dtv auto afd set.
	 * if auto set load_flag = 0 by user, overscan set by dtv afd
	 */
	if (!overscan_table[0].load_flag &&
		!overscan_table[offset].load_flag)
		pq_user_latch_flag |= PQ_USER_OVERSCAN_RESET;
	/*because SOURCE_TV is 0,so need to add a flg to check ATV*/
	if (overscan_table[offset].load_flag == 1 &&
	    overscan_table[offset].source == SOURCE_TV)
		atv_source_flg = 1;
	else
		atv_source_flg = 0;
}

static void hdr_tone_mapping_get(enum lut_type_e lut_type,
				 unsigned int length,
				 unsigned int *hdr_tm)
{
	int i;

	if (hdr_tm) {
		if (lut_type & HLG_LUT) {
			for (i = 0; i < length; i++)
				oo_y_lut_hlg_sdr[i] = hdr_tm[i];

			if (debug_amvecm & 4) {
				for (i = 0; i < length; i++) {
					pr_info("oo_y_lut_hlg_sdr[%d] = %d",
						i, oo_y_lut_hlg_sdr[i]);
					if (i % 8 == 0)
						pr_info("\n");
				}
				pr_info("\n");
			}
		} else if (lut_type & HDR_LUT) {
			for (i = 0; i < length; i++)
				oo_y_lut_hdr_sdr_def[i] = hdr_tm[i];

	vecm_latch_flag |= FLAG_HDR_OOTF_LATCH;

			if (debug_amvecm & 4) {
				for (i = 0; i < length; i++) {
					pr_info("oo_y_lut_hdr_sdr[%d] = %d",
						i, oo_y_lut_hdr_sdr_def[i]);
					if (i % 8 == 0)
						pr_info("\n");
				}
				pr_info("\n");
			}
		}
	}
}

static int parse_aipq_ofst_table(int *table_ptr,
				 unsigned int height, unsigned int width)
{
	unsigned int i;
	unsigned int size = 0;

	size = width * sizeof(int);
	for (i = 0; i < height; i++)
		memcpy(vpp_pq_data[i], table_ptr + (i * width), size);

#ifdef CONFIG_AMLOGIC_MEDIA_VIDEO
		update_aipq_data();
#endif

	return 0;
}

static long amvecm_ioctl(struct file *file,
			 unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	void __user *argp;
	unsigned int mem_size;
	struct ve_pq_load_s vpp_pq_load;
	struct ve_pq_table_s *vpp_pq_load_table = NULL;
	enum color_primary_e color_pri;
	struct hdr_tone_mapping_s hdr_tone_mapping;
	unsigned int *hdr_tm = NULL;
	struct vpp_pq_ctrl_s pq_ctrl;
	enum meson_cpu_ver_e cpu_ver;
	struct aipq_load_s aipq_load_table;
	char *aipq_char_data_ptr = NULL;
	int *aipq_int_data_ptr = NULL;
	int size, size1;
	int aipq_data_length;
	struct cm_color_md cm_color_mode;
	struct table_3dlut_s *p3dlut;
	int lut_order, lut_index;
	struct vpp_mtx_info_s *mtx_p = &mtx_info;
	struct pre_gamma_table_s *pre_gma_tb = NULL;
	struct hdr_tmo_sw pre_tmo_reg;
	struct db_cabc_param_s db_cabc_param;
	struct db_aad_param_s db_aad_param;
	struct eye_protect_s *eye_prot = NULL;
	int tmp;
	struct primary_s color_pr;
	struct video_color_matrix gamut_mtx;
	int cm_color = 0;
	struct ve_ble_whe_param_s ble_whe;
	struct color_param_s ct_parm1;
	struct color_tune_parm_s ct_param;

	if (debug_amvecm & 2)
		pr_info("[amvecm..] %s: cmd_nr = 0x%x\n",
			__func__, _IOC_NR(cmd));

	if (probe_ok == 0)
		return ret;

	if (pq_load_en == 0) {
		if (debug_amvecm & 4)
			pr_amvecm_dbg("[amvecm..] pq ioctl function disabled !!\n");
		return ret;
	}

	switch (cmd) {
	case AMVECM_IOC_LOAD_REG:
		if ((vecm_latch_flag & FLAG_REG_MAP0) &&
		    (vecm_latch_flag & FLAG_REG_MAP1) &&
			(vecm_latch_flag & FLAG_REG_MAP2) &&
			(vecm_latch_flag & FLAG_REG_MAP3) &&
			(vecm_latch_flag & FLAG_REG_MAP4) &&
			(vecm_latch_flag & FLAG_REG_MAP5)) {
			ret = -EBUSY;
			pr_amvecm_dbg("load regs error: loading regs, please wait\n");
			break;
		}
		if (copy_from_user(&amregs_ext,
				   (void __user *)arg,
				   sizeof(struct am_regs_s))) {
			pr_amvecm_dbg("0x%lx load reg errors: can't get buffer length\n",
				      FLAG_REG_MAP0);
			ret = -EFAULT;
		} else {
			ret = cm_load_reg(&amregs_ext);
		}
		break;
	case AMVECM_IOC_VE_NEW_DNLP:
		if (copy_from_user(&dnlp_curve_param_load,
				   (void __user *)arg,
			sizeof(struct ve_dnlp_curve_param_s))) {
			pr_amvecm_dbg("dnlp load fail\n");
			ret = -EFAULT;
		} else {
			ve_new_dnlp_param_update();
			pr_amvecm_dbg("dnlp load success\n");
		}
		break;
	case AMVECM_IOC_G_HIST_AVG:
		argp = (void __user *)arg;
		if (video_ve_hist.height == 0 || video_ve_hist.width == 0)
			ret = -EFAULT;
		else if (copy_to_user(argp,
				      &video_ve_hist,
				sizeof(struct ve_hist_s)))
			ret = -EFAULT;
		break;
	case AMVECM_IOC_G_HIST_BIN:
		argp = (void __user *)arg;
		if (vpp_hist_param.vpp_pixel_sum == 0)
			ret = -EFAULT;
		else if (copy_to_user(argp, &vpp_hist_param,
				      sizeof(struct vpp_hist_param_s)))
			ret = -EFAULT;
		break;
	case AMVECM_IOC_G_HDR_METADATA:
		argp = (void __user *)arg;
		if (copy_to_user(argp, &vpp_hdr_metadata_s,
				 sizeof(struct hdr_metadata_info_s)))
			ret = -EFAULT;
		break;
	/**********************************************************************/
	/*gamma ioctl*/
	/**********************************************************************/
	case AMVECM_IOC_GAMMA_TABLE_EN:
		if (!gamma_en)
			return -EINVAL;

		vecm_latch_flag |= FLAG_GAMMA_TABLE_EN;
		break;
	case AMVECM_IOC_GAMMA_TABLE_DIS:
		if (!gamma_en)
			return -EINVAL;

		vecm_latch_flag |= FLAG_GAMMA_TABLE_DIS;
		break;
	case AMVECM_IOC_GAMMA_TABLE_R:
		if (!gamma_en)
			return -EINVAL;

		if (copy_from_user(&video_gamma_table_ioctl_set,
				   (void __user *)arg,
				sizeof(struct tcon_gamma_table_s))) {
			ret = -EFAULT;
		} else if (gamma_table_compare(&video_gamma_table_ioctl_set,
					     &video_gamma_table_r)) {
			memcpy(&video_gamma_table_r,
			       &video_gamma_table_ioctl_set,
				sizeof(struct tcon_gamma_table_s));
			vecm_latch_flag |= FLAG_GAMMA_TABLE_R;
		} else {
			pr_amvecm_dbg("load same gamma_r table,no need to change\n");
		}
		break;
	case AMVECM_IOC_GAMMA_TABLE_G:
		if (!gamma_en)
			return -EINVAL;

		if (copy_from_user(&video_gamma_table_ioctl_set,
				   (void __user *)arg,
				sizeof(struct tcon_gamma_table_s))) {
			ret = -EFAULT;
		} else if (gamma_table_compare(&video_gamma_table_ioctl_set,
					     &video_gamma_table_g)) {
			memcpy(&video_gamma_table_g,
			       &video_gamma_table_ioctl_set,
				sizeof(struct tcon_gamma_table_s));
			vecm_latch_flag |= FLAG_GAMMA_TABLE_G;
		} else {
			pr_amvecm_dbg("load same gamma_g table,no need to change\n");
		}
		break;
	case AMVECM_IOC_GAMMA_TABLE_B:
		if (!gamma_en)
			return -EINVAL;

		if (copy_from_user(&video_gamma_table_ioctl_set,
				   (void __user *)arg,
				sizeof(struct tcon_gamma_table_s))) {
			ret = -EFAULT;
		} else if (gamma_table_compare(&video_gamma_table_ioctl_set,
					     &video_gamma_table_b)) {
			memcpy(&video_gamma_table_b,
			       &video_gamma_table_ioctl_set,
				sizeof(struct tcon_gamma_table_s));
			vecm_latch_flag |= FLAG_GAMMA_TABLE_B;
		} else {
			pr_amvecm_dbg("load same gamma_b table,no need to change\n");
		}
		break;
	case AMVECM_IOC_GAMMA_TABLE_EN_SUB:
		if (!gamma_en)
			return -EINVAL;

		vecm_latch_flag2 |= FLAG_GAMMA_TABLE_EN_SUB;
		break;
	case AMVECM_IOC_GAMMA_TABLE_DIS_SUB:
		if (!gamma_en)
			return -EINVAL;

		vecm_latch_flag2 |= FLAG_GAMMA_TABLE_DIS_SUB;
		break;
	case AMVECM_IOC_GAMMA_TABLE_R_SUB:
		if (!gamma_en)
			return -EINVAL;

		if (copy_from_user(&video_gamma_table_ioctl_set,
			(void __user *)arg,
			sizeof(struct tcon_gamma_table_s))) {
			ret = -EFAULT;
		} else if (gamma_table_compare(&video_gamma_table_ioctl_set,
				&video_gamma_table_r_sub)) {
			memcpy(&video_gamma_table_r_sub,
				&video_gamma_table_ioctl_set,
				sizeof(struct tcon_gamma_table_s));
			vecm_latch_flag2 |= FLAG_GAMMA_TABLE_R_SUB;
		} else {
			pr_amvecm_dbg("load same gamma_r_sub table,no need to change\n");
		}
		break;
	case AMVECM_IOC_GAMMA_TABLE_G_SUB:
		if (!gamma_en)
			return -EINVAL;

		if (copy_from_user(&video_gamma_table_ioctl_set,
			(void __user *)arg,
			sizeof(struct tcon_gamma_table_s))) {
			ret = -EFAULT;
		} else if (gamma_table_compare(&video_gamma_table_ioctl_set,
			&video_gamma_table_g_sub)) {
			memcpy(&video_gamma_table_g_sub,
				&video_gamma_table_ioctl_set,
				sizeof(struct tcon_gamma_table_s));
			vecm_latch_flag2 |= FLAG_GAMMA_TABLE_G_SUB;
		} else {
			pr_amvecm_dbg("load same gamma_g_sub table,no need to change\n");
		}
		break;
	case AMVECM_IOC_GAMMA_TABLE_B_SUB:
		if (!gamma_en)
			return -EINVAL;

		if (copy_from_user(&video_gamma_table_ioctl_set,
			(void __user *)arg,
			sizeof(struct tcon_gamma_table_s))) {
			ret = -EFAULT;
		} else if (gamma_table_compare(&video_gamma_table_ioctl_set,
			&video_gamma_table_b_sub)) {
			memcpy(&video_gamma_table_b_sub,
				&video_gamma_table_ioctl_set,
				sizeof(struct tcon_gamma_table_s));
			vecm_latch_flag2 |= FLAG_GAMMA_TABLE_B_SUB;
		} else {
			pr_amvecm_dbg("load same gamma_b_sub table,no need to change\n");
		}
		break;
	case AMVECM_IOC_GAMMA_SET:
		if (!gamma_en)
			return -EINVAL;

		if (copy_from_user(&gt, (void __user *)arg,
			sizeof(struct gm_tbl_s))) {
			pr_amvecm_dbg("load gamma table fail\n");
			ret = -EFAULT;
		} else {
			pr_amvecm_dbg(" gm_par_idx = %d, load gm success\n", gm_par_idx);
		}
		break;
	case AMVECM_IOC_S_RGB_OGO:
		pr_amvecm_dbg("AMVECM_IOC_S_RGB_OGO, wb_en=%d\n", wb_en);
		if (!wb_en)
			return -EINVAL;

		if (copy_from_user(&video_rgb_ogo,
				   (void __user *)arg,
				sizeof(struct tcon_rgb_ogo_s)))
			ret = -EFAULT;
		else
			ve_ogo_param_update();
		break;
	case AMVECM_IOC_G_RGB_OGO:
		pr_amvecm_dbg("AMVECM_IOC_G_RGB_OGO, wb_en=%d\n", wb_en);
		if (!wb_en)
			return -EINVAL;

		if (copy_to_user((void __user *)arg,
				 &video_rgb_ogo, sizeof(struct tcon_rgb_ogo_s)))
			ret = -EFAULT;

		break;
	case AMVECM_IOC_S_RGB_OGO_SUB:
			pr_amvecm_dbg("AMVECM_IOC_S_RGB_OGO_SUB, wb_en=%d\n", wb_en);
			if (!wb_en)
				return -EINVAL;

			if (copy_from_user(&video_rgb_ogo_sub,
				(void __user *)arg,
				sizeof(struct tcon_rgb_ogo_s)))
				ret = -EFAULT;
			else
				ve_ogo_param_update_sub();

			break;
		case AMVECM_IOC_G_RGB_OGO_SUB:
			pr_amvecm_dbg("AMVECM_IOC_G_RGB_OGO_SUB, wb_en=%d\n", wb_en);
			if (!wb_en)
				return -EINVAL;

			if (copy_to_user((void __user *)arg,
				&video_rgb_ogo_sub, sizeof(struct tcon_rgb_ogo_s)))
				ret = -EFAULT;

			break;
	case AMVECM_IOC_SET_3D_LUT:
		p3dlut = kmalloc(4913 * 3 * sizeof(unsigned int),
				 GFP_KERNEL);
		if (!p3dlut)
			return -ENOMEM;

		if (copy_from_user(p3dlut,
				   (void __user *)arg,
				   4913 * 3 * sizeof(unsigned int))) {
			ret = -EFAULT;
		} else {
			vpp_lut3d_table_init(0, 0, 0);
			vpp_set_lut3d(0, 0, p3dlut->data, 0);
			vpp_lut3d_table_release();
		}

		kfree(p3dlut);
		break;
	case AMVECM_IOC_LOAD_3D_LUT:
		if (copy_from_user(&lut_index,
				   (void __user *)arg,
				   sizeof(int))) {
			ret = -EFAULT;
			break;
		}

		vpp_lut3d_table_init(0, 0, 0);
		vpp_set_lut3d(lut3d_data_source, lut_index, 0, 0);
		vpp_lut3d_table_release();

		break;
	case AMVECM_IOC_SET_3D_LUT_ORDER:
		if (copy_from_user(&lut_order,
				   (void __user *)arg,
				   sizeof(int))) {
			ret = -EFAULT;
		} else {
			lut3d_order = lut_order;
		}

		break;
	case AMVECM_IOC_3D_LUT_EN:
		if (copy_from_user(&tmp,
				   (void __user *)arg,
				   sizeof(int))) {
			ret = -EFAULT;
		} else {
			lut3d_en = tmp;
			lut3d_en &= 0x1;
			vpp_enable_lut3d(lut3d_en);
		}
		break;
	case AMVECM_IOC_COLOR_PRI_EN:
		if (copy_from_user(&tmp,
				   (void __user *)arg,
				   sizeof(int))) {
			ret = -EFAULT;
		} else {
			force_primary = tmp;
			vecm_latch_flag |= FLAG_COLORPRI_LATCH;
			force_toggle();
		}
		break;
	case AMVECM_IOC_COLOR_PRIMARY:
		if (copy_from_user(&color_pr,
				   (void __user *)arg,
				   sizeof(struct primary_s))) {
			ret = -EFAULT;
		} else {
			memcpy(force_src_primary, color_pr.src,
				8 * sizeof(u32));
			memcpy(force_dst_primary, color_pr.dest,
				8 * sizeof(u32));
			vecm_latch_flag |= FLAG_COLORPRI_LATCH;
			force_toggle();
		}
		break;
	/*VLOCK*/
	case AMVECM_IOC_VLOCK_EN:
		vecm_latch_flag |= FLAG_VLOCK_EN;
		break;
	case AMVECM_IOC_VLOCK_DIS:
		vecm_latch_flag |= FLAG_VLOCK_DIS;
		break;
	/*3D-SYNC*/
	case AMVECM_IOC_3D_SYNC_EN:
		vecm_latch_flag |= FLAG_3D_SYNC_EN;
		break;
	case AMVECM_IOC_3D_SYNC_DIS:
		vecm_latch_flag |= FLAG_3D_SYNC_DIS;
		break;
	case AMVECM_IOC_SET_OVERSCAN:
		if (copy_from_user(&vpp_pq_load,
				   (void __user *)arg,
				sizeof(struct ve_pq_load_s))) {
			ret = -EFAULT;
			pr_amvecm_dbg("[amvecm..] pq ioctl copy fail!!\n");
			break;
		}
		if (!(vpp_pq_load.param_id & TABLE_NAME_OVERSCAN)) {
			ret = -EFAULT;
			pr_amvecm_dbg("[amvecm..] overscan ioctl param_id fail!!\n");
			break;
		}
		/*check pq_table length copy_from_user*/
		if (vpp_pq_load.length > TIMING_MAX ||
		    vpp_pq_load.length <= 0)  {
			ret = -EFAULT;
			pr_amvecm_dbg("[amvecm..] pq ioctl length check fail!!\n");
			break;
		}

		mem_size = vpp_pq_load.length * sizeof(struct ve_pq_table_s);
		vpp_pq_load_table = kmalloc(mem_size, GFP_KERNEL);
		if (!vpp_pq_load_table) {
			pr_info("vpp_pq_load_table kmalloc fail!!!\n");
			return -EFAULT;
		}
		argp = (void __user *)vpp_pq_load.param_ptr;
		if (copy_from_user(vpp_pq_load_table, argp, mem_size)) {
			pr_amvecm_dbg("[amvecm..] overscan copy fail!!\n");
			break;
		}
		parse_overscan_table(vpp_pq_load.length, vpp_pq_load_table);
		break;
	case AMVECM_IOC_S_HDR_TM:
		if (copy_from_user(&hdr_tone_mapping,
				   (void __user *)arg,
				   sizeof(struct hdr_tone_mapping_s))) {
			ret = -EFAULT;
			pr_amvecm_dbg("hdr ioc fail!!!\n");
			break;
		}

		if (hdr_tone_mapping.lutlength > HDR2_OOTF_LUT_SIZE) {
			pr_amvecm_dbg("hdr tm over size !!!\n");
			ret = -EFAULT;
			break;
		}
		mem_size = hdr_tone_mapping.lutlength * sizeof(unsigned int);
		hdr_tm = kmalloc(mem_size, GFP_KERNEL);
		argp = (void __user *)hdr_tone_mapping.tm_lut;
		if (!hdr_tm) {
			ret = -EFAULT;
			pr_amvecm_dbg("hdr tm kmalloc fail!!!\n");
			break;
		}
		if (copy_from_user(hdr_tm, argp, mem_size)) {
			pr_amvecm_dbg("[amvecm..] hdr_tm copy fail!!\n");
			ret = -EFAULT;
			break;
		}
		hdr_tone_mapping_get(hdr_tone_mapping.lut_type,
				     hdr_tone_mapping.lutlength,
				     hdr_tm);

		break;
	case AMVECM_IOC_G_DNLP_STATE:
		if (copy_to_user((void __user *)arg,
				 &dnlp_en, sizeof(enum dnlp_state_e)))
			ret = -EFAULT;
		break;
	case AMVECM_IOC_S_DNLP_STATE:
		if (copy_from_user(&dnlp_en,
				   (void __user *)arg,
				   sizeof(enum dnlp_state_e)))
			ret = -EFAULT;
		break;
	case AMVECM_IOC_G_PQMODE:
		argp = (void __user *)arg;
		if (copy_to_user(argp,
				 &pc_mode, sizeof(enum pc_mode_e)))
			ret = -EFAULT;
		break;
	case AMVECM_IOC_S_PQMODE:
		if (copy_from_user(&pc_mode,
				   (void __user *)arg,
				   sizeof(enum pc_mode_e)))
			ret = -EFAULT;
		else
			pc_mode_last = 0xff;
		break;
	case AMVECM_IOC_G_CSCTYPE:
		argp = (void __user *)arg;
		if (copy_to_user(argp,
				 &cur_csc_type[VD1_PATH],
				sizeof(enum vpp_matrix_csc_e)))
			ret = -EFAULT;
		break;
	case AMVECM_IOC_G_COLOR_PRI:
		argp = (void __user *)arg;
		color_pri = get_color_primary();
		if (copy_to_user(argp,
				 &color_pri,
				 sizeof(enum color_primary_e)))
			ret = -EFAULT;
		break;
	case AMVECM_IOC_S_CSCTYPE:
		if (copy_from_user(&cur_csc_type[VD1_PATH],
				   (void __user *)arg,
				   sizeof(enum vpp_matrix_csc_e))) {
			ret = -EFAULT;
			pr_amvecm_dbg("[amvecm..] cur_csc_type ioctl copy fail!!\n");
		}
		break;
	case AMVECM_IOC_G_PIC_MODE:
		argp = (void __user *)arg;
		if (copy_to_user(argp,
				 &vdj_mode_s, sizeof(struct am_vdj_mode_s)))
			ret = -EFAULT;
		break;
	case AMVECM_IOC_S_PIC_MODE:
		if (copy_from_user(&vdj_mode_s,
				   (void __user *)arg,
				   sizeof(struct am_vdj_mode_s))) {
			ret = -EFAULT;
			pr_amvecm_dbg("[amvecm..] vdj_mode_s ioctl copy fail!!\n");
			break;
		}
		vdj_mode_flg = vdj_mode_s.flag;
		/*vadj switch control according to vadj1_en/vadj2_en*/
		if (vdj_mode_flg & VDJ_FLAG_VADJ_EN) {
			pr_amvecm_dbg("IOC--vadj1_en=%d,vadj2_en=%d.\n",
				      vdj_mode_s.vadj1_en, vdj_mode_s.vadj2_en);
			vecm_latch_flag |= FLAG_VADJ_EN;
		}

		if (vdj_mode_flg & VDJ_FLAG_BRIGHTNESS) { /*brightness*/
			vd1_brightness = vdj_mode_s.brightness;
			vecm_latch_flag |= FLAG_VADJ1_BRI;
		}
		if (vdj_mode_flg & VDJ_FLAG_BRIGHTNESS2) { /*brightness2*/
			if (vdj_mode_s.brightness2 < -1024 ||
			    vdj_mode_s.brightness2 > 1023) {
				pr_amvecm_dbg("load brightness2 value invalid!!!\n");
				return -EINVAL;
			}
			ret = amvecm_set_brightness2(vdj_mode_s.brightness2);
		}
		if (vdj_mode_flg & VDJ_FLAG_SAT_HUE)	{ /*saturation_hue*/
			/*ai pq get saturation*/
			aipq_base_satur_param(vdj_mode_s.saturation_hue);
			//ret =
			//amvecm_set_saturation_hue(vdj_mode_s.saturation_hue);
			pq_user_latch_flag |= PQ_USER_CMS_SAT_HUE;
			pr_amvecm_dbg("vdj_mode_s.saturation_hue:%d\n",
				vdj_mode_s.saturation_hue);
		}
		if (vdj_mode_flg & VDJ_FLAG_SAT_HUE_POST) {
			/*saturation_hue_post*/
			int sat_post, hue_post, sat_hue_post;

			sat_hue_post = vdj_mode_s.saturation_hue_post;
			sat_post = (((sat_hue_post >> 16) & 0xffff) / 2) - 128;
			hue_post = sat_hue_post & 0xffff;
			if (hue_post >= 0 && hue_post <= 150)
				hue_post = hue_post / 6;
			else
				hue_post = (hue_post - 1024) / 6;
			ret =
			amvecm_set_saturation_hue_post(sat_post, hue_post);
			if (ret < 0)
				break;
		}
		if (vdj_mode_flg & 0x10) { /*contrast*/
			if (vdj_mode_s.contrast < -1024 ||
			    vdj_mode_s.contrast > 1023) {
				ret = -EINVAL;
				pr_amvecm_dbg("[amvecm..] ioctrl contrast value invalid!!\n");
				break;
			}
			vd1_contrast = vdj_mode_s.contrast;
			vecm_latch_flag |= FLAG_VADJ1_CON;
		}
		if (vdj_mode_flg & 0x20) { /*constract2*/
			if (vdj_mode_s.contrast2 < -127 ||
			    vdj_mode_s.contrast2 > 127) {
				ret = -EINVAL;
				pr_amvecm_dbg("[amvecm..] ioctrl contrast2 value invalid!!\n");
				break;
			}
			ret = amvecm_set_contrast2(vdj_mode_s.contrast2);
		}
		break;
	case AMVECM_IOC_S_LC_CURVE:
		if (copy_from_user(&lc_curve_parm_load,
				   (void __user *)arg,
			sizeof(struct ve_lc_curve_parm_s))) {
			pr_amvecm_dbg("lc load curve parm fail\n");
			ret = -EFAULT;
		} else {
			ve_lc_curve_update();
			pr_amvecm_dbg("lc load curve parm success\n");
		}
		break;
	case AMVECM_IOC_G_HDR_TYPE:
		argp = (void __user *)arg;
		if (copy_to_user(argp,
				 &hdr_source_type, sizeof(enum hdr_type_e)))
			ret = -EFAULT;
		break;
	case AMVECM_IOC_S_PQ_CTRL:
		if (copy_from_user(&pq_ctrl,
				   (void __user *)arg,
				   sizeof(struct vpp_pq_ctrl_s))) {
			ret = -EFAULT;
			pr_amvecm_dbg("pq control cp vpp_pq_ctrl_s fail\n");
		} else {
			argp = (void __user *)pq_ctrl.ptr;
			pr_amvecm_dbg("argp = %p\n", argp);
			mem_size = sizeof(struct pq_ctrl_s);
			if (pq_ctrl.length > mem_size) {
				pq_ctrl.length = mem_size;
				pr_amvecm_dbg("system control length > kernel length\n");
			}
			if (copy_from_user(&pq_cfg,
					   argp,
					   mem_size)) {
				ret = -EFAULT;
				pr_amvecm_dbg("pq control cp pq_ctrl_s fail\n");
			} else {
				pq_user_latch_flag |= PQ_USER_PQ_MODULE_CTL;
				pr_amvecm_dbg("pq control load success\n");
			}
		}
		break;
	case AMVECM_IOC_G_PQ_CTRL:
		argp = (void __user *)arg;
		pq_ctrl.length = sizeof(struct pq_ctrl_s);
		pq_ctrl.ptr = (void *)&pq_cfg;
		if (copy_to_user(argp, &pq_ctrl,
				 sizeof(struct vpp_pq_ctrl_s))) {
			ret = -EFAULT;
			pr_info("pq control cp to user fail\n");
		} else {
			pr_info("pq control cp to user success\n");
		}
		break;
	case AMVECM_IOC_S_MESON_CPU_VER:
		if (!is_meson_tm2_cpu())
			break;
		if (copy_from_user(&cpu_ver,
				   (void __user *)arg,
				   sizeof(enum meson_cpu_ver_e))) {
			ret = -EFAULT;
			pr_amvecm_dbg("copy cpu version fail\n");
		} else {
			if ((is_meson_rev_a() && cpu_ver == VER_A) ||
			    (is_meson_rev_b() && cpu_ver == VER_B))
				break;
			ret = -EINVAL;
			pr_amvecm_dbg("cpu version doesn't match\n");
		}
		break;
	case AMVECM_IOC_S_AIPQ_TABLE:
		if (copy_from_user(&aipq_load_table,
				   (void __user *)arg,
				   sizeof(struct aipq_load_s))) {
			ret = -EFAULT;
			pr_amvecm_dbg("aipq load ioc fail\n");
			break;
		}
		if (aipq_load_table.height > AIPQ_SCENE_MAX)
			aipq_load_table.height = AIPQ_SCENE_MAX;
		if (aipq_load_table.width > AIPQ_FUNC_MAX)
			aipq_load_table.width = AIPQ_FUNC_MAX;
		aipq_data_length = AIPQ_SINGL_DATA_LEN *
						aipq_load_table.height *
						aipq_load_table.width;

		size = (aipq_data_length + 1) * sizeof(char);
		aipq_char_data_ptr = kmalloc(size, GFP_KERNEL);
		if (!aipq_char_data_ptr) {
			ret = -EFAULT;
			pr_amvecm_dbg("aipq_char_data_ptr kmalloc fail!!!\n");
			break;
		}

		size1 = aipq_load_table.height *
			aipq_load_table.width *
			sizeof(int);
		aipq_int_data_ptr = kmalloc(size1, GFP_KERNEL);
		if (!aipq_int_data_ptr) {
			ret = -EFAULT;
			pr_amvecm_dbg("aipq_int_data_ptr kmalloc fail!!!\n");
			break;
		}

		argp = (void __user *)aipq_load_table.table_ptr;
		if (copy_from_user(aipq_char_data_ptr, argp, size)) {
			ret = -EFAULT;
			pr_amvecm_dbg("aipq table copy from user fail\n");
			break;
		}

		aipq_char_data_ptr[aipq_data_length] = '\0';
		if (strlen(aipq_char_data_ptr) != aipq_data_length) {
			pr_amvecm_dbg("aipq data length not eq 3*height*width!!!\n");
			break;
		}

		str_sapr_to_d(aipq_char_data_ptr, aipq_int_data_ptr, 4);

		parse_aipq_ofst_table(aipq_int_data_ptr,
				      aipq_load_table.height,
				      aipq_load_table.width);
		break;
	case AMVECM_IOC_S_CMS_LUMA:
		if (copy_from_user(&cm_color_mode, (void __user *)arg,
				   sizeof(struct cm_color_md))) {
			ret = -EFAULT;
		} else {
			if (cm_color_mode.color_type == cm_9_color)
				cm_color = cm_color_mode.cm_9_color_md;
			else
				cm_color = cm_color_mode.cm_14_color_md;

			cm_cur_work_color_md = cm_color_mode.color_type;
			if (cm_color >= 0 && cm_color <= (cm_14_ecm2colormode_max - 1)) {
				cm2_luma_array[cm_color][0] = cm_color_mode.color_value;
				cm2_luma_array[cm_color][1] = 0;
				cm2_luma_array[cm_color][2] = 1;
				cm2_luma(cm_color_mode,
					cm2_luma_array[cm_color][0],
					cm2_luma_array[cm_color][1]);
				pq_user_latch_flag |= PQ_USER_CMS_CURVE_LUMA;
			}
		}
		break;
	case AMVECM_IOC_S_CMS_SAT:
		if (copy_from_user(&cm_color_mode, (void __user *)arg,
				   sizeof(struct cm_color_md))) {
			ret = -EFAULT;
		} else {
			if (cm_color_mode.color_type == cm_9_color)
				cm_color = cm_color_mode.cm_9_color_md;
			else
				cm_color = cm_color_mode.cm_14_color_md;

			cm_cur_work_color_md = cm_color_mode.color_type;
			if (cm_color >= 0 && cm_color <= (cm_14_ecm2colormode_max - 1)) {
				cm2_sat_array[cm_color][0] = cm_color_mode.color_value;
				cm2_sat_array[cm_color][1] = 0;
				cm2_sat_array[cm_color][2] = 1;
				cm2_sat(cm_color_mode,
					cm2_sat_array[cm_color][0],
					cm2_sat_array[cm_color][1]);
				pq_user_latch_flag |= PQ_USER_CMS_CURVE_SAT;
			}
		}
		break;
	case AMVECM_IOC_S_CMS_HUE:
		if (copy_from_user(&cm_color_mode, (void __user *)arg,
				   sizeof(struct cm_color_md))) {
			ret = -EFAULT;
		} else {
			if (cm_color_mode.color_type == cm_9_color)
				cm_color = cm_color_mode.cm_9_color_md;
			else
				cm_color = cm_color_mode.cm_14_color_md;

			cm_cur_work_color_md = cm_color_mode.color_type;
			if (cm_color >= 0 && cm_color <= (cm_14_ecm2colormode_max - 1)) {
				cm2_hue_array[cm_color][0] = cm_color_mode.color_value;
				cm2_hue_array[cm_color][1] = 0;
				cm2_hue_array[cm_color][2] = 1;
				cm2_hue(cm_color_mode,
					cm2_hue_array[cm_color][0],
					cm2_hue_array[cm_color][1]);
				pq_user_latch_flag |= PQ_USER_CMS_CURVE_HUE;
			}
		}
		break;
	case AMVECM_IOC_S_CMS_HUE_HS:
		if (copy_from_user(&cm_color_mode, (void __user *)arg,
				   sizeof(struct cm_color_md))) {
			ret = -EFAULT;
		} else {
			if (cm_color_mode.color_type == cm_9_color)
				cm_color = cm_color_mode.cm_9_color_md;
			else
				cm_color = cm_color_mode.cm_14_color_md;

			cm_cur_work_color_md = cm_color_mode.color_type;
			if (cm_color >= 0 && cm_color <= (cm_14_ecm2colormode_max - 1)) {
				cm2_hue_by_hs_array[cm_color][0] =
				cm_color_mode.color_value;
				cm2_hue_by_hs_array[cm_color][1] = 0;
				cm2_hue_by_hs_array[cm_color][2] = 1;
				cm2_hue_by_hs(cm_color_mode,
					      cm2_hue_by_hs_array[cm_color][0],
					      cm2_hue_by_hs_array[cm_color][1]);
				pq_user_latch_flag |= PQ_USER_CMS_CURVE_HUE_HS;
			}
		}
		break;
	case AMVECM_IOC_S_MTX_COEF:
		if (copy_from_user(mtx_p, (void __user *)arg,
				   sizeof(struct vpp_mtx_info_s))) {
			ret = -EFAULT;
			pr_amvecm_dbg("mtx info cp from usr failed\n");
		} else {
			vecm_latch_flag2 |= VPP_MARTIX_UPDATE;
		}
		break;
	case AMVECM_IOC_G_MTX_COEF:
		argp = (void __user *)arg;
		vecm_latch_flag2 |= VPP_MARTIX_GET;
		vpp_mtx_update(mtx_p);
		if (copy_to_user(argp, mtx_p,
				 sizeof(struct vpp_mtx_info_s))) {
			ret = -EFAULT;
			pr_amvecm_dbg("mtx coef copy to user fail\n");
		} else {
			pr_amvecm_dbg("mtx coef copy to user success\n");
		}
		break;
	case AMVECM_IOC_S_PRE_GAMMA:
		mem_size = sizeof(struct pre_gamma_table_s);
		pre_gma_tb = kmalloc(mem_size, GFP_KERNEL);
		if (!pre_gma_tb) {
			pr_amvecm_dbg("pre_gma_tb malloc fail\n");
			ret = -ENOMEM;
			break;
		}
		if (copy_from_user(pre_gma_tb, (void __user *)arg,
				   sizeof(struct pre_gamma_table_s))) {
			ret = -EFAULT;
			pr_amvecm_dbg("pre gam struct cp from usr failed\n");
		} else {
			pr_amvecm_dbg("pre gam struct cp from usr success\n");
			pre_gamma.en = pre_gma_tb->en;
			memcpy(pre_gamma.lut_r,
			       pre_gma_tb->lut_r, 65 * sizeof(unsigned int));
			memcpy(pre_gamma.lut_g,
			       pre_gma_tb->lut_g, 65 * sizeof(unsigned int));
			memcpy(pre_gamma.lut_b,
			       pre_gma_tb->lut_b, 65 * sizeof(unsigned int));
			vecm_latch_flag2 |= VPP_PRE_GAMMA_UPDATE;
		}
		break;
	case AMVECM_IOC_G_PRE_GAMMA:
		argp = (void __user *)arg;
		mem_size = sizeof(struct pre_gamma_table_s);
		pre_gma_tb = kmalloc(mem_size, GFP_KERNEL);
		if (!pre_gma_tb) {
			pr_amvecm_dbg("pre_gma_tb malloc fail\n");
			ret = -EFAULT;
			break;
		}
		pre_gma_tb->en = pre_gamma.en;
		memcpy(pre_gma_tb->lut_r,
		       pre_gamma.lut_r, 65 * sizeof(unsigned int));
		memcpy(pre_gma_tb->lut_g,
		       pre_gamma.lut_g, 65 * sizeof(unsigned int));
		memcpy(pre_gma_tb->lut_b,
		       pre_gamma.lut_b, 65 * sizeof(unsigned int));
		if (copy_to_user(argp, pre_gma_tb, mem_size)) {
			ret = -EFAULT;
			pr_amvecm_dbg("pre gam struct cp to usr failed\n");
		} else {
			pr_amvecm_dbg("pre gam struct cp to usr success\n");
		}
		break;
	case AMVECM_IOC_S_HDR_TMO:
		if (copy_from_user(&pre_tmo_reg,
			(void __user *)arg,
			sizeof(struct hdr_tmo_sw))) {
			ret = -EFAULT;
			pr_info("tmo_reg info cp from usr failed\n");
		} else {
			hdr10_tmo_reg_set(&pre_tmo_reg);
			pr_info("tmo_reg set success\n");
		}
		break;
	case AMVECM_IOC_G_HDR_TMO:
		argp = (void __user *)arg;
		hdr10_tmo_reg_get(&pre_tmo_reg);
		if (copy_to_user(argp, &pre_tmo_reg,
			sizeof(struct hdr_tmo_sw))) {
			ret = -EFAULT;
			pr_info("tmo_reg copy to user fail\n");
		} else {
			pr_info("tmo_reg copy to user success\n");
		}
		break;
	case AMVECM_IOC_S_CABC_PARAM:
		if (copy_from_user(&db_cabc_param,
			(void __user *)arg,
			sizeof(struct db_cabc_param_s))) {
			ret = -EFAULT;
			pr_amvecm_dbg("db_cabc_param copy from user fail\n");
		} else {
			/*
			 * the same structure code db_aad_param_set is normal
			 */
			/* coverity[tainted_data:SUPPRESS] */
			db_cabc_param_set(&db_cabc_param);
			pr_amvecm_dbg("db_cabc_param set success\n");
		}
		break;
	case AMVECM_IOC_S_AAD_PARAM:
		if (copy_from_user(&db_aad_param,
			(void __user *)arg,
			sizeof(struct db_aad_param_s))) {
			ret = -EFAULT;
			pr_amvecm_dbg("db_aad_param copy from user fail\n");
		} else {
			db_aad_param_set(&db_aad_param);
			pr_amvecm_dbg("db_aad_param set success\n");
		}
		break;
	case AMVECM_IOC_S_COLOR_TUNE:
		if (copy_from_user(&ct_param, (void __user *)arg,
				   sizeof(struct color_tune_parm_s))) {
			pr_amvecm_dbg("set color tune failed\n");
			ret = -EFAULT;
		} else {
			if (cmp_ct_parm(&ct_param)) {
				memcpy(&ct_parm1, &ct_param, sizeof(struct color_tune_parm_s));
				ct_parm_set(&ct_parm1);
				bs_latch();
				pr_amvecm_dbg("set color tune success\n");
			} else {
				pr_amvecm_dbg("same color tune param, no need update\n");
			}
		}
		break;
	case AMVECM_IOC_S_EYE_PROT:
		mem_size = sizeof(struct eye_protect_s);
		eye_prot = kmalloc(mem_size, GFP_KERNEL);
		if (!eye_prot) {
			pr_amvecm_dbg("eye_protect malloc fail\n");
			ret = -ENOMEM;
			break;
		}
		if (copy_from_user(eye_prot, (void __user *)arg,
				   sizeof(struct eye_protect_s))) {
			ret = -EFAULT;
			pr_amvecm_dbg("eye_protect struct cp from usr failed\n");
		} else {
			pr_amvecm_dbg("eye_protect struct cp from usr success\n");
			eye_protect.en = eye_prot->en;
			memcpy(eye_protect.mtx_ep,
				eye_prot->mtx_ep, 16 * sizeof(int));
			vecm_latch_flag2 |= VPP_EYE_PROTECT_UPDATE;
		}
		break;
	case AMVECM_IOC_S_FREERUN_TYPE:
		if (copy_from_user(&freerun_en,
			(void __user *)arg,
			sizeof(enum freerun_type_e)))
			ret = -EFAULT;
		break;
	case AMVECM_IOC_S_GAMUT_CONV_EN:
		if (copy_from_user(&gamut_conv_enable,
			(void __user *)arg,
			sizeof(enum gamut_conv_enable_e))) {
			ret = -EFAULT;
			pr_amvecm_dbg("gamut conv enable cp from usr failed\n");
		} else {
			pr_amvecm_dbg("gamut conv enable cp from usr success status:%d\n",
				gamut_conv_enable);
		}
		break;
	case AMVECM_IOC_COLOR_MTX_EN:
		if (copy_from_user(&tmp,
				   (void __user *)arg,
				   sizeof(int))) {
			ret = -EFAULT;
			pr_amvecm_dbg("color matrix cp from usr failed\n");
		} else {
			pr_amvecm_dbg("color matrix cp from usr success\n");
			force_matrix = tmp;
			vecm_latch_flag |= FLAG_COLORPRI_LATCH;
			force_toggle();
		}
		break;
	case AMVECM_IOC_S_COLOR_MATRIX_DATA:
		if (copy_from_user(&gamut_mtx,
				   (void __user *)arg,
				   sizeof(struct video_color_matrix))) {
			ret = -EFAULT;
			pr_amvecm_dbg("color matrix data cp from usr failed\n");
		} else {
			pr_amvecm_dbg("color matrix data cp from usr success\n");
			memcpy(force_matrix_primary, gamut_mtx.data,
				sizeof(force_matrix_primary));
			vecm_latch_flag |= FLAG_COLORPRI_LATCH;
			force_toggle();
		}
		break;
	case AMVECM_IOC_G_COLOR_MATRIX_DATA:
		argp = (void __user *)arg;
		memcpy(gamut_mtx.data, force_matrix_primary,
				sizeof(force_matrix_primary));
		if (copy_to_user(argp, &gamut_mtx,
				 sizeof(struct video_color_matrix))) {
			ret = -EFAULT;
			pr_amvecm_dbg("gamut matrix copy to user fail\n");
		} else {
			pr_amvecm_dbg("gamut matrix copy to user success\n");
		}
		break;
	case AMVECM_IOC_S_BLE_WHE:
		if (copy_from_user(&ble_whe,
			(void __user *)arg,
			sizeof(struct ve_ble_whe_param_s))) {
			ret = -EFAULT;
			pr_amvecm_dbg("ble whe copy from user fail\n");
		} else {
			memcpy(&ble_whe_param_load, &ble_whe,
				sizeof(struct ve_ble_whe_param_s));
			vecm_latch_flag2 |= BLE_WHE_UPDATE;
			pr_amvecm_dbg("ble whe set success\n");
		}
		break;
	default:
		ret = -EINVAL;
		break;
	}

	kfree(vpp_pq_load_table);
	kfree(hdr_tm);
	kfree(aipq_char_data_ptr);
	kfree(aipq_int_data_ptr);
	kfree(pre_gma_tb);
	kfree(eye_prot);
	// kfree(pre_tmo_reg);
	return ret;
}

#ifdef CONFIG_COMPAT
static long amvecm_compat_ioctl(struct file *file, unsigned int cmd,
				unsigned long arg)
{
	unsigned long ret;

	arg = (unsigned long)compat_ptr(arg);
	ret = amvecm_ioctl(file, cmd, arg);
	return ret;
}
#endif

static unsigned int dnlp_dbg_flag;
static int dnlp_rd_param;
static char dnlp_rd_curve[400];
static ssize_t amvecm_dnlp_debug_show(struct class *cla,
				      struct class_attribute *attr,
				      char *buf)
{
	if (dnlp_dbg_flag & DNLP_PARAM_RD_UPDATE) {
		dnlp_dbg_flag &= ~DNLP_PARAM_RD_UPDATE;
		return sprintf(buf, "%d\n", dnlp_rd_param);
	}

	if (dnlp_dbg_flag & DNLP_CV_RD_UPDATE) {
		dnlp_dbg_flag &= ~DNLP_CV_RD_UPDATE;
		return sprintf(buf, "%s\n", dnlp_rd_curve);
	}

	return 0;
}

static ssize_t amvecm_dnlp_debug_store(struct class *cla,
				       struct class_attribute *attr,
				       const char *buf, size_t count)
{
	int i, *p;
	long val = 0;
	unsigned int num;
	char *buf_orig, *parm[8] = {NULL};
	int curve_val[65] = {0};
	char *stemp = NULL;
	unsigned short *hist_tmp;

	if (!buf)
		return count;

	stemp = kzalloc(400, GFP_KERNEL);
	if (!stemp)
		return 0;
	memset(stemp, 0, 400);
	memset(dnlp_rd_curve, 0, 400);

	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig) {
		kfree(stemp);
		return -ENOMEM;
	}
	parse_param_amvecm(buf_orig, (char **)&parm);

	if (!dnlp_insmod_ok) {
		pr_info("dnlp insmod fail\n");
		goto free_buf;
	}

	if (!strcmp(parm[0], "r")) {/*read param*/
		if (!strcmp(parm[1], "param")) {
			if (!strcmp(parm[2], "all")) {
				for (i = 0;
				dnlp_parse_cmd[i].value; i++) {
					pr_info("%d ",
						*dnlp_parse_cmd[i].value);
				}
				pr_info("\n");
			} else {
				pr_info("error cmd\n");
			}
		} else {
			for (i = 0;
				dnlp_parse_cmd[i].value; i++) {
				if (!strcmp(parm[1],
					    dnlp_parse_cmd[i].parse_string)) {
					dnlp_rd_param = *dnlp_parse_cmd[i].value;
					dnlp_dbg_flag |= DNLP_PARAM_RD_UPDATE;
					break;
				}
			}
			dnlp_dbg_node_copy();
		}
	} else if (!strcmp(parm[0], "w")) {/*write param*/
		for (i = 0; dnlp_parse_cmd[i].value; i++) {
			if (!strcmp(parm[1],
				    dnlp_parse_cmd[i].parse_string)) {
				if (kstrtoul(parm[2], 10, &val) < 0)
					goto free_buf;

				*dnlp_parse_cmd[i].value = val;
				pr_amvecm_dbg(" %s: %d\n",
					      dnlp_parse_cmd[i].parse_string,
					*dnlp_parse_cmd[i].value);
				break;
			}
			dnlp_dbg_node_copy();
		}
	} else if (!strcmp(parm[0], "rc")) {/*read curve*/
		if (!strcmp(parm[1], "scurv_low")) {
			if (!parm[2]) {
				pr_info("error cmd\n");
				goto free_buf;
			} else if (!strcmp(parm[2], "all")) {
				for (i = 0; i < 65; i++)
					d_convert_str(dnlp_scurv_low_copy[i],
						      i, stemp, 4, 10);
				memcpy(dnlp_rd_curve, stemp, sizeof(char) * 4 * 65);
				dnlp_dbg_flag |= DNLP_CV_RD_UPDATE;
			} else {
				if (kstrtoul(parm[2], 10, &val) < 0)
					goto free_buf;

				if (val > 64 || val < 0)
					pr_info("error cmd\n");
				else
					pr_info("%d\n",
						dnlp_scurv_low_copy[val]);
			}
		} else if (!strcmp(parm[1], "scurv_mid1")) {
			if (!parm[2]) {
				pr_info("error cmd\n");
				goto free_buf;
			} else if (!strcmp(parm[2], "all")) {
				for (i = 0; i < 65; i++)
					d_convert_str(dnlp_scurv_mid1_copy[i],
						      i, stemp, 4, 10);
				memcpy(dnlp_rd_curve, stemp, sizeof(char) * 4 * 65);
				dnlp_dbg_flag |= DNLP_CV_RD_UPDATE;
			} else {
				if (kstrtoul(parm[2], 10, &val) < 0)
					goto free_buf;

				if (val > 64 || val < 0)
					pr_info("error cmd\n");
				else
					pr_info("%d\n",
						dnlp_scurv_mid1_copy[val]);
			}
		} else if (!strcmp(parm[1], "scurv_mid2")) {
			if (!parm[2]) {
				pr_info("error cmd\n");
				goto free_buf;
			} else if (!strcmp(parm[2], "all")) {
				for (i = 0; i < 65; i++)
					d_convert_str(dnlp_scurv_mid2_copy[i],
						      i, stemp, 4, 10);
				memcpy(dnlp_rd_curve, stemp, sizeof(char) * 4 * 65);
				dnlp_dbg_flag |= DNLP_CV_RD_UPDATE;
			} else {
				if (kstrtoul(parm[2], 10, &val) < 0)
					goto free_buf;

				if (val > 64 || val < 0)
					pr_info("error cmd\n");
				else
					pr_info("%d\n",
						dnlp_scurv_mid2_copy[val]);
			}
		} else if (!strcmp(parm[1], "scurv_hgh1")) {
			if (!parm[2]) {
				pr_info("error cmd\n");
				goto free_buf;
			} else if (!strcmp(parm[2], "all")) {
				for (i = 0; i < 65; i++)
					d_convert_str(dnlp_scurv_hgh1_copy[i],
						      i, stemp, 4, 10);
				memcpy(dnlp_rd_curve, stemp, sizeof(char) * 4 * 65);
				dnlp_dbg_flag |= DNLP_CV_RD_UPDATE;
			} else {
				if (kstrtoul(parm[2], 10, &val) < 0)
					goto free_buf;

				if (val > 64 || val < 0)
					pr_info("error cmd\n");
				else
					pr_info("%d\n",
						dnlp_scurv_hgh1_copy[val]);
			}
		} else if (!strcmp(parm[1], "scurv_hgh2")) {
			if (!parm[2]) {
				pr_info("error cmd\n");
				goto free_buf;
			} else if (!strcmp(parm[2], "all")) {
				for (i = 0; i < 65; i++)
					d_convert_str(dnlp_scurv_hgh2_copy[i],
						      i, stemp, 4, 10);
				memcpy(dnlp_rd_curve, stemp, sizeof(char) * 4 * 65);
				dnlp_dbg_flag |= DNLP_CV_RD_UPDATE;
			} else {
				if (kstrtoul(parm[2], 10, &val) < 0)
					goto free_buf;

				if (val > 64 || val < 0)
					pr_info("error cmd\n");
				else
					pr_info("%d\n",
						dnlp_scurv_hgh2_copy[val]);
			}
		} else if (!strcmp(parm[1], "gain_var_lut49")) {
			if (!parm[2]) {
				pr_info("error cmd\n");
				goto free_buf;
			} else if (!strcmp(parm[2], "all")) {
				for (i = 0; i < 49; i++)
					d_convert_str(gain_var_lut49_copy[i],
						      i, stemp, 4, 10);
				memcpy(dnlp_rd_curve, stemp, sizeof(char) * 4 * 49);
				dnlp_dbg_flag |= DNLP_CV_RD_UPDATE;
			} else {
				if (kstrtoul(parm[2], 10, &val) < 0)
					goto free_buf;

				if (val > 48 || val < 0)
					pr_info("error cmd\n");
				else
					pr_info("%d\n",
						gain_var_lut49_copy[val]);
			}
		} else if (!strcmp(parm[1], "c_hist_gain")) {
			if (!parm[2]) {
				pr_info("error cmd\n");
				goto free_buf;
			} else if (!strcmp(parm[2], "all")) {
				for (i = 0; i < 65; i++)
					d_convert_str(c_hist_gain_copy[i],
						      i, stemp, 4, 10);
				memcpy(dnlp_rd_curve, stemp,
					sizeof(char) * 4 * 65);
				dnlp_dbg_flag |= DNLP_CV_RD_UPDATE;
			} else {
				if (kstrtoul(parm[2], 10, &val) < 0)
					goto free_buf;

				if (val > 64 || val < 0)
					pr_info("error cmd\n");
				else
					pr_info("%d\n",
						c_hist_gain_copy[val]);
			}
		} else if (!strcmp(parm[1], "s_hist_gain")) {
			if (!parm[2]) {
				pr_info("error cmd\n");
				goto free_buf;
			} else if (!strcmp(parm[2], "all")) {
				for (i = 0; i < 65; i++)
					d_convert_str(s_hist_gain_copy[i],
						      i, stemp, 4, 10);
				memcpy(dnlp_rd_curve, stemp,
					sizeof(char) * 4 * 65);
				dnlp_dbg_flag |= DNLP_CV_RD_UPDATE;
			} else {
				if (kstrtoul(parm[2], 10, &val) < 0)
					goto free_buf;

				if (val > 64 || val < 0)
					pr_info("error cmd\n");
				else
					pr_info("%d\n",
						s_hist_gain_copy[val]);
			}
		} else if (!strcmp(parm[1], "wext_gain")) {
			if (!parm[2]) {
				pr_info("error cmd\n");
				goto free_buf;
			} else if (!strcmp(parm[2], "all")) {
				for (i = 0; i < 48; i++)
					d_convert_str(wext_gain_copy[i],
						      i, stemp, 4, 10);
				memcpy(dnlp_rd_curve, stemp, sizeof(char) * 4 * 48);
				dnlp_dbg_flag |= DNLP_CV_RD_UPDATE;
			} else {
				if (kstrtoul(parm[2], 10, &val) < 0)
					goto free_buf;

				if (val > 47 || val < 0)
					pr_info("error cmd\n");
				else
					pr_info("%d\n", wext_gain_copy[val]);
			}
		} else if (!strcmp(parm[1], "adp_thrd")) {
			if (!parm[2]) {
				pr_info("error cmd\n");
				goto free_buf;
			} else if (!strcmp(parm[2], "all")) {
				for (i = 0; i < 33; i++)
					d_convert_str(adp_thrd_copy[i],
						      i, stemp, 4, 10);
				memcpy(dnlp_rd_curve, stemp, sizeof(char) * 4 * 33);
				dnlp_dbg_flag |= DNLP_CV_RD_UPDATE;
			} else {
				if (kstrtoul(parm[2], 10, &val) < 0)
					goto free_buf;

				if (val > 32 || val < 0)
					pr_info("error cmd\n");
				else
					pr_info("%d\n", adp_thrd_copy[val]);
			}
		} else if (!strcmp(parm[1], "reg_blk_boost_12")) {
			if (!parm[2]) {
				pr_info("error cmd\n");
				goto free_buf;
			} else if (!strcmp(parm[2], "all")) {
				for (i = 0; i < 13; i++)
					d_convert_str(reg_blk_boost_12_copy[i],
						      i, stemp, 4, 10);
				memcpy(dnlp_rd_curve, stemp, sizeof(char) * 4 * 13);
				dnlp_dbg_flag |= DNLP_CV_RD_UPDATE;
			} else {
				if (kstrtoul(parm[2], 10, &val) < 0)
					goto free_buf;

				if (val > 12 || val < 0)
					pr_info("error cmd\n");
				else
					pr_info("%d\n",
						reg_blk_boost_12_copy[val]);
			}
		} else if (!strcmp(parm[1], "reg_adp_ofset_20")) {
			if (!parm[2]) {
				pr_info("error cmd\n");
				goto free_buf;
			} else if (!strcmp(parm[2], "all")) {
				for (i = 0; i < 20; i++)
					d_convert_str(reg_adp_ofset_20_copy[i],
						      i, stemp, 4, 10);
				memcpy(dnlp_rd_curve, stemp, sizeof(char) * 4 * 20);
				dnlp_dbg_flag |= DNLP_CV_RD_UPDATE;
			} else {
				if (kstrtoul(parm[2], 10, &val) < 0)
					goto free_buf;

				if (val > 19 || val < 0)
					pr_info("error cmd\n");
				else
					pr_info("%d\n",
						reg_adp_ofset_20_copy[val]);
			}
		} else if (!strcmp(parm[1], "reg_mono_protect")) {
			if (!parm[2]) {
				pr_info("error cmd\n");
				goto free_buf;
			} else if (!strcmp(parm[2], "all")) {
				for (i = 0; i < 6; i++)
					d_convert_str(reg_mono_protect_copy[i],
						      i, stemp, 4, 10);
				memcpy(dnlp_rd_curve, stemp, sizeof(char) * 4 * 6);
				dnlp_dbg_flag |= DNLP_CV_RD_UPDATE;
			} else {
				if (kstrtoul(parm[2], 10, &val) < 0)
					goto free_buf;

				if (val > 5 || val < 0)
					pr_info("error cmd\n");
				else
					pr_info("%d\n",
						reg_mono_protect_copy[val]);
			}
		} else if (!strcmp(parm[1], "reg_trend_wht_expand_lut8")) {
			p = reg_trend_wht_expand_lut8_copy;
			if (!parm[2]) {
				pr_info("error cmd\n");
				goto free_buf;
			} else if (!strcmp(parm[2], "all")) {
				for (i = 0; i < 9; i++)
					d_convert_str(p[i],
						      i, stemp, 4, 10);
				memcpy(dnlp_rd_curve, stemp, sizeof(char) * 4 * 9);
				dnlp_dbg_flag |= DNLP_CV_RD_UPDATE;
			} else {
				if (kstrtoul(parm[2], 10, &val) < 0)
					goto free_buf;

				if (val > 8 || val < 0)
					pr_info("error cmd\n");
				else
					pr_info("%d\n",
						p[val]);
			}
		} else if (!strcmp(parm[1], "ve_dnlp_tgt")) {
			/*read only curve*/
			if (!parm[2]) {
				pr_info("error cmd\n");
				goto free_buf;
			} else if (!strcmp(parm[2], "all")) {
				for (i = 0; i < 65; i++)
					d_convert_str(ve_dnlp_tgt_copy[i],
						      i, stemp, 4, 10);
				memcpy(dnlp_rd_curve, stemp, sizeof(char) * 4 * 65);
				dnlp_dbg_flag |= DNLP_CV_RD_UPDATE;
			} else {
				pr_info("error cmd\n");
			}
		} else if (!strcmp(parm[1], "ve_dnlp_tgt_10b")) {
			/*read only curve*/
			if (!parm[2]) {
				pr_info("error cmd\n");
				goto free_buf;
			} else if (!strcmp(parm[2], "all")) {
				for (i = 0; i < 65; i++)
					d_convert_str(ve_dnlp_tgt_10b_copy[i],
						      i, stemp, 4, 10);
				memcpy(dnlp_rd_curve, stemp, sizeof(char) * 4 * 65);
				dnlp_dbg_flag |= DNLP_CV_RD_UPDATE;
			} else {
				pr_info("error cmd\n");
			}
		} else if (!strcmp(parm[1], "GmScurve")) {
			/*read only curve*/
			if (!parm[2]) {
				pr_info("error cmd\n");
				goto free_buf;
			} else if (!strcmp(parm[2], "all")) {
				for (i = 0; i < 65; i++)
					d_convert_str(gmscurve_copy[i],
						      i, stemp, 4, 10);
				memcpy(dnlp_rd_curve, stemp, sizeof(char) * 4 * 65);
				dnlp_dbg_flag |= DNLP_CV_RD_UPDATE;
			} else {
				pr_info("error cmd\n");
			}
		} else if (!strcmp(parm[1], "clash_curve")) {
			/*read only curve*/
			if (!parm[2]) {
				pr_info("error cmd\n");
				goto free_buf;
			} else if (!strcmp(parm[2], "all")) {
				for (i = 0; i < 65; i++)
					d_convert_str(clash_curve_copy[i],
						      i, stemp, 4, 10);
				memcpy(dnlp_rd_curve, stemp, sizeof(char) * 4 * 65);
				dnlp_dbg_flag |= DNLP_CV_RD_UPDATE;
			} else {
				pr_info("error cmd\n");
			}
		} else if (!strcmp(parm[1], "clsh_scvbld")) {
			/*read only curve*/
			if (!parm[2]) {
				pr_info("error cmd\n");
				goto free_buf;
			} else if (!strcmp(parm[2], "all")) {
				for (i = 0; i < 65; i++)
					d_convert_str(clsh_scvbld_copy[i],
						      i, stemp, 4, 10);
				memcpy(dnlp_rd_curve, stemp, sizeof(char) * 4 * 65);
				dnlp_dbg_flag |= DNLP_CV_RD_UPDATE;
			} else {
				pr_info("error cmd\n");
			}
		} else if (!strcmp(parm[1], "blkwht_ebld")) {
			/*read only curve*/
			if (!parm[2]) {
				pr_info("error cmd\n");
				goto free_buf;
			} else if (!strcmp(parm[2], "all")) {
				for (i = 0; i < 65; i++)
					d_convert_str(blkwht_ebld_copy[i],
						      i, stemp, 4, 10);
				memcpy(dnlp_rd_curve, stemp, sizeof(char) * 4 * 65);
				dnlp_dbg_flag |= DNLP_CV_RD_UPDATE;
			} else {
				pr_info("error cmd\n");
			}
		} else if (!strcmp(parm[1], "vpp_histgram")) {
			/*read only curve*/
			hist_tmp = vpp_hist_param.vpp_histgram;
			if (!parm[2]) {
				pr_info("error cmd\n");
				goto free_buf;
			} else if (!strcmp(parm[2], "all")) {
				for (i = 0; i < 64; i++)
					d_convert_str(hist_tmp[i],
						      i, stemp, 4, 10);
				memcpy(dnlp_rd_curve, stemp, sizeof(char) * 4 * 64);
				dnlp_dbg_flag |= DNLP_CV_RD_UPDATE;
			} else {
				pr_info("error cmd\n");
			}
		}
	} else if (!strcmp(parm[0], "wc")) {/*write curve*/
		if (!strcmp(parm[1], "scurv_low")) {
			if (!parm[2]) {
				pr_info("error cmd\n");
				goto free_buf;
			} else if (!strcmp(parm[2], "all")) {
				str_sapr_to_d(parm[3], curve_val, 5);
				for (i = 0; i < 65; i++)
					dnlp_scurv_low_copy[i] = curve_val[i];
			} else {
				if (kstrtoul(parm[2], 10, &val) < 0)
					goto free_buf;

				num = val;
				if (kstrtoul(parm[3], 10, &val) < 0)
					goto free_buf;

				if (num > 64)
					pr_info("error cmd\n");
				else
					dnlp_scurv_low_copy[num] = val;
			}
		} else if (!strcmp(parm[1], "scurv_mid1")) {
			if (!parm[2]) {
				pr_info("error cmd\n");
				goto free_buf;
			} else if (!strcmp(parm[2], "all")) {
				str_sapr_to_d(parm[3], curve_val, 5);
				for (i = 0; i < 65; i++)
					dnlp_scurv_mid1_copy[i] = curve_val[i];
			} else {
				if (kstrtoul(parm[2], 10, &val) < 0)
					goto free_buf;

				num = val;
				if (kstrtoul(parm[3], 10, &val) < 0)
					goto free_buf;

				if (num > 64)
					pr_info("error cmd\n");
				else
					dnlp_scurv_mid1_copy[num] = val;
			}
		} else if (!strcmp(parm[1], "scurv_mid2")) {
			if (!parm[2]) {
				pr_info("error cmd\n");
				goto free_buf;
			} else if (!strcmp(parm[2], "all")) {
				str_sapr_to_d(parm[3], curve_val, 5);
				for (i = 0; i < 65; i++)
					dnlp_scurv_mid2_copy[i] = curve_val[i];
			} else {
				if (kstrtoul(parm[2], 10, &val) < 0)
					goto free_buf;

				num = val;
				if (kstrtoul(parm[3], 10, &val) < 0)
					goto free_buf;

				if (num > 64)
					pr_info("error cmd\n");
				else
					dnlp_scurv_mid2_copy[num] = val;
			}
		} else if (!strcmp(parm[1], "scurv_hgh1")) {
			if (!parm[2]) {
				pr_info("error cmd\n");
				goto free_buf;
			} else if (!strcmp(parm[2], "all")) {
				str_sapr_to_d(parm[3], curve_val, 5);
				for (i = 0; i < 65; i++)
					dnlp_scurv_hgh1_copy[i] = curve_val[i];
			} else {
				if (kstrtoul(parm[2], 10, &val) < 0)
					goto free_buf;

				num = val;
				if (kstrtoul(parm[3], 10, &val) < 0)
					goto free_buf;

				if (num > 64)
					pr_info("error cmd\n");
				else
					dnlp_scurv_hgh1_copy[num] = val;
			}
		} else if (!strcmp(parm[1], "scurv_hgh2")) {
			if (!parm[2]) {
				pr_info("error cmd\n");
				goto free_buf;
			} else if (!strcmp(parm[2], "all")) {
				str_sapr_to_d(parm[3], curve_val, 5);
				for (i = 0; i < 65; i++)
					dnlp_scurv_hgh2_copy[i] = curve_val[i];
			} else {
				if (kstrtoul(parm[2], 10, &val) < 0)
					goto free_buf;

				num = val;
				if (kstrtoul(parm[3], 10, &val) < 0)
					goto free_buf;

				if (num > 64)
					pr_info("error cmd\n");
				else
					dnlp_scurv_hgh2_copy[num] = val;
			}
		} else if (!strcmp(parm[1], "gain_var_lut49")) {
			if (!parm[2]) {
				pr_info("error cmd\n");
				goto free_buf;
			} else if (!strcmp(parm[2], "all")) {
				str_sapr_to_d(parm[3], curve_val, 5);
				for (i = 0; i < 49; i++)
					gain_var_lut49_copy[i] = curve_val[i];
			} else {
				if (kstrtoul(parm[2], 10, &val) < 0)
					goto free_buf;

				num = val;
				if (kstrtoul(parm[3], 10, &val) < 0)
					goto free_buf;

				if (num > 48)
					pr_info("error cmd\n");
				else
					gain_var_lut49_copy[num] = val;
			}
		} else if (!strcmp(parm[1], "c_hist_gain")) {
			if (!parm[2]) {
				pr_info("error cmd\n");
				goto free_buf;
			} else if (!strcmp(parm[2], "all")) {
				str_sapr_to_d(parm[3], curve_val, 5);
				for (i = 0; i < 65; i++)
					c_hist_gain_copy[i] = curve_val[i];
			} else {
				if (kstrtoul(parm[2], 10, &val) < 0)
					goto free_buf;

				num = val;
				if (kstrtoul(parm[3], 10, &val) < 0)
					goto free_buf;

				if (num > 64)
					pr_info("error cmd\n");
				else
					c_hist_gain_copy[num] = val;
			}
		} else if (!strcmp(parm[1], "s_hist_gain")) {
			if (!parm[2]) {
				pr_info("error cmd\n");
				goto free_buf;
			} else if (!strcmp(parm[2], "all")) {
				str_sapr_to_d(parm[3], curve_val, 5);
				for (i = 0; i < 65; i++)
					s_hist_gain_copy[i] = curve_val[i];
			} else {
				if (kstrtoul(parm[2], 10, &val) < 0)
					goto free_buf;

				num = val;
				if (kstrtoul(parm[3], 10, &val) < 0)
					goto free_buf;

				if (num > 64)
					pr_info("error cmd\n");
				else
					s_hist_gain_copy[num] = val;
			}
		} else if (!strcmp(parm[1], "wext_gain")) {
			if (!parm[2]) {
				pr_info("error cmd\n");
				goto free_buf;
			} else if (!strcmp(parm[2], "all")) {
				str_sapr_to_d(parm[3], curve_val, 5);
				for (i = 0; i < 48; i++)
					wext_gain_copy[i] = curve_val[i];
			} else {
				if (kstrtoul(parm[2], 10, &val) < 0)
					goto free_buf;

				num = val;
				if (kstrtoul(parm[3], 10, &val) < 0)
					goto free_buf;

				if (num > 47)
					pr_info("error cmd\n");
				else
					wext_gain_copy[num] = val;
			}
		} else if (!strcmp(parm[1], "adp_thrd")) {
			if (!parm[2]) {
				pr_info("error cmd\n");
				goto free_buf;
			} else if (!strcmp(parm[2], "all")) {
				str_sapr_to_d(parm[3], curve_val, 5);
				for (i = 0; i < 33; i++)
					adp_thrd_copy[i] = curve_val[i];
			} else {
				if (kstrtoul(parm[2], 10, &val) < 0)
					goto free_buf;

				num = val;
				if (kstrtoul(parm[3], 10, &val) < 0)
					goto free_buf;

				if (num > 32)
					pr_info("error cmd\n");
				else
					adp_thrd_copy[num] = val;
			}
		} else if (!strcmp(parm[1], "reg_blk_boost_12")) {
			if (!parm[2]) {
				pr_info("error cmd\n");
				goto free_buf;
			} else if (!strcmp(parm[2], "all")) {
				str_sapr_to_d(parm[3], curve_val, 5);
				for (i = 0; i < 13; i++)
					reg_blk_boost_12_copy[i] = curve_val[i];
			} else {
				if (kstrtoul(parm[2], 10, &val) < 0)
					goto free_buf;

				num = val;
				if (kstrtoul(parm[3], 10, &val) < 0)
					goto free_buf;

				if (num > 12)
					pr_info("error cmd\n");
				else
					reg_blk_boost_12_copy[num] = val;
			}
		} else if (!strcmp(parm[1], "reg_adp_ofset_20")) {
			if (!parm[2]) {
				pr_info("error cmd\n");
				goto free_buf;
			} else if (!strcmp(parm[2], "all")) {
				str_sapr_to_d(parm[3], curve_val, 5);
				for (i = 0; i < 20; i++)
					reg_adp_ofset_20_copy[i] = curve_val[i];
			} else {
				if (kstrtoul(parm[2], 10, &val) < 0)
					goto free_buf;

				num = val;
				if (kstrtoul(parm[3], 10, &val) < 0)
					goto free_buf;

				if (num > 19)
					pr_info("error cmd\n");
				else
					reg_adp_ofset_20_copy[num] = val;
			}
		} else if (!strcmp(parm[1], "reg_mono_protect")) {
			if (!parm[2]) {
				pr_info("error cmd\n");
				goto free_buf;
			} else if (!strcmp(parm[2], "all")) {
				str_sapr_to_d(parm[3], curve_val, 5);
				for (i = 0; i < 6; i++)
					reg_mono_protect_copy[i] = curve_val[i];
			} else {
				if (kstrtoul(parm[2], 10, &val) < 0)
					goto free_buf;

				num = val;
				if (kstrtoul(parm[3], 10, &val) < 0)
					goto free_buf;

				if (num > 5)
					pr_info("error cmd\n");
				else
					reg_mono_protect_copy[num] = val;
			}
		} else if (!strcmp(parm[1], "reg_trend_wht_expand_lut8")) {
			p = reg_trend_wht_expand_lut8_copy;
			if (!parm[2]) {
				pr_info("error cmd\n");
				goto free_buf;
			} else if (!strcmp(parm[2], "all")) {
				str_sapr_to_d(parm[3], curve_val, 5);
				for (i = 0; i < 9; i++)
					reg_trend_wht_expand_lut8_copy[i] =
						curve_val[i];
			} else {
				if (kstrtoul(parm[2], 10, &val) < 0)
					goto free_buf;

				num = val;
				if (kstrtoul(parm[3], 10, &val) < 0)
					goto free_buf;

				if (num > 8)
					pr_info("error cmd\n");
				else
					reg_trend_wht_expand_lut8_copy[num] =
						val;
			}
		}
	} else if (!strcmp(parm[0], "ro")) {
		if (!strcmp(parm[1], "luma_avg4")) {
			dnlp_rd_param = *ro_luma_avg4_copy;
			dnlp_dbg_flag |= DNLP_PARAM_RD_UPDATE;
		} else if (!strcmp(parm[1], "var_d8")) {
			dnlp_rd_param = *ro_var_d8_copy;
			dnlp_dbg_flag |= DNLP_PARAM_RD_UPDATE;
		} else if (!strcmp(parm[1], "scurv_gain")) {
			dnlp_rd_param = *ro_scurv_gain_copy;
			dnlp_dbg_flag |= DNLP_PARAM_RD_UPDATE;
		} else if (!strcmp(parm[1], "blk_wht_ext0")) {
			dnlp_rd_param = *ro_blk_wht_ext0_copy;
			dnlp_dbg_flag |= DNLP_PARAM_RD_UPDATE;
		} else if (!strcmp(parm[1], "blk_wht_ext1")) {
			dnlp_rd_param = *ro_blk_wht_ext1_copy;
			dnlp_dbg_flag |= DNLP_PARAM_RD_UPDATE;
		} else if (!strcmp(parm[1], "dnlp_brightness")) {
			dnlp_rd_param = *ro_dnlp_brightness_copy;
			dnlp_dbg_flag |= DNLP_PARAM_RD_UPDATE;
		} else
			pr_info("error cmd\n");
	} else if (!strcmp(parm[0], "dnlp_print")) {
		if (kstrtoul(parm[1], 16, &val) < 0)
			goto free_buf;

		*dnlp_printk_copy = val;
		pr_info("dnlp_print = %x\n", *dnlp_printk_copy);
	}

	kfree(buf_orig);
	kfree(stemp);
	return count;

free_buf:
	kfree(buf_orig);
	kfree(stemp);
	return -EINVAL;
}

static ssize_t amvecm_cabc_aad_show(struct class *cla,
				    struct class_attribute *attr, char *buf)
{
	ssize_t len = 0;

	len = cabc_aad_print(buf);
	return len;
}

static ssize_t amvecm_cabc_aad_store(struct class *cla,
				     struct class_attribute *attr,
				     const char *buf, size_t count)
{
	int ret;
	char *buf_orig, *parm[8] = {NULL};

	if (!buf)
		return count;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	parse_param_amvecm(buf_orig, (char **)&parm);

	ret = cabc_aad_debug(parm);

	if (ret < 0)
		pr_info("set parameters failed\n");

	kfree(buf_orig);
	return count;
}

static ssize_t amvecm_brightness_show(struct class *cla,
				      struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", vd1_brightness);
}

static ssize_t amvecm_brightness_store(struct class *cla,
				       struct class_attribute *attr,
				       const char *buf, size_t count)
{
	size_t r;
	int val;

	r = sscanf(buf, "%d\n", &val);
	if (r != 1 || val < -1024 || val > 1024)
		return -EINVAL;

	vd1_brightness = val;
	/*vecm_latch_flag |= FLAG_BRI_CON;*/
	vecm_latch_flag |= FLAG_VADJ1_BRI;
	return count;
}

static ssize_t amvecm_contrast_show(struct class *cla,
				    struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", vd1_contrast);
}

static ssize_t amvecm_contrast_store(struct class *cla,
				     struct class_attribute *attr,
		const char *buf, size_t count)
{
	size_t r;
	int val;

	r = sscanf(buf, "%d\n", &val);
	if (r != 1 || val < -1024 || val > 1023)
		return -EINVAL;

	vd1_contrast = val;
	/*vecm_latch_flag |= FLAG_BRI_CON;*/
	vecm_latch_flag |= FLAG_VADJ1_CON;
	return count;
}

static ssize_t amvecm_saturation_hue_show(struct class *cla,
					  struct class_attribute *attr,
					  char *buf)
{
	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A)
		return sprintf(buf, "0x%x\n",
			READ_VPP_REG(VPP_VADJ1_MA_MB_2));
	else
		return sprintf(buf, "0x%x\n",
			READ_VPP_REG(VPP_VADJ1_MA_MB));
}

static ssize_t amvecm_saturation_hue_store(struct class *cla,
					   struct class_attribute *attr,
					   const char *buf, size_t count)
{
	size_t r;
	s32 mab = 0;

	r = sscanf(buf, "0x%x", &mab);
	if (r != 1 || (mab & 0xfc00fc00))
		return -EINVAL;
	amvecm_set_saturation_hue(mab);
	return count;
}

void vpp_vd_adj1_saturation_hue(signed int sat_val,
				signed int hue_val, struct vframe_s *vf)
{
	int i, ma, mb, mab, mc, md;
	int hue_cos[] = {
			/*0~12*/
		256, 256, 256, 255, 255, 254, 253, 252, 251, 250, 248, 247, 245,
		/*13~25*/
		243, 241, 239, 237, 234, 231, 229, 226, 223, 220, 216, 213, 209
	};
	int hue_sin[] = {
		/*-25~-13*/
		-147, -142, -137, -132, -126, -121, -115, -109, -104,
		 -98,  -92,  -86,  -80,  -74,  -68,  -62,  -56,  -50,
		 -44,  -38,  -31,  -25, -19, -13,  -6,      /*-12~-1*/
		0,  /*0*/
		 /*1~12*/
		6,   13,   19,	25,   31,   38,   44,	50,   56,  62,
		68,  74,   80,   86,   92,	98,  104,  109,  115,  121,
		126,  132, 137, 142, 147 /*13~25*/
	};

	i = (hue_val > 0) ? hue_val : -hue_val;
	ma = (hue_cos[i] * (sat_val + 128)) >> 7;
	mb = (hue_sin[25 + hue_val] * (sat_val + 128)) >> 7;
	saturation_ma_shift = ma - 0x100;
	saturation_mb_shift = mb;

	ma += saturation_ma;
	mb += saturation_mb;
	if (ma > 511)
		ma = 511;
	if (ma < -512)
		ma = -512;
	if (mb > 511)
		mb = 511;
	if (mb < -512)
		mb = -512;
	mab =  ((ma & 0x3ff) << 16) | (mb & 0x3ff);

	if (chip_type_id == chip_s5)
		ve_color_mab_set(mab, VE_VADJ1, WR_VCB);
	else if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A)
		VSYNC_WRITE_VPP_REG(VPP_VADJ1_MA_MB_2, mab);
	else
		VSYNC_WRITE_VPP_REG(VPP_VADJ1_MA_MB, mab);
	mc = (s16)((mab << 22) >> 22); /* mc = -mb */
	mc = 0 - mc;
	if (mc > 511)
		mc = 511;
	if (mc < -512)
		mc = -512;
	md = (s16)((mab << 6) >> 22);  /* md =	ma; */
	mab = ((mc & 0x3ff) << 16) | (md & 0x3ff);

	if (chip_type_id == chip_s5) {
		ve_color_mcd_set(mab, VE_VADJ1, WR_VCB);
	} else if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A) {
		VSYNC_WRITE_VPP_REG(VPP_VADJ1_MC_MD_2, mab);
		VSYNC_WRITE_VPP_REG_BITS(VPP_VADJ1_MISC, 1, 0, 1);
	} else {
		VSYNC_WRITE_VPP_REG(VPP_VADJ1_MC_MD, mab);
		VSYNC_WRITE_VPP_REG_BITS(VPP_VADJ_CTRL, 1, 0, 1);
	}
};

static ssize_t amvecm_saturation_hue_pre_show(struct class *cla,
					      struct class_attribute *attr,
					      char *buf)
{
	return snprintf(buf, 20, "%d %d\n", saturation_pre, hue_pre);
}

static ssize_t amvecm_saturation_hue_pre_store(struct class *cla,
					       struct class_attribute *attr,
					       const char *buf, size_t count)
{
	int parsed[2];

	if (likely(parse_para_pq(buf, 2, parsed) != 2))
		return -EINVAL;

	if (parsed[0] < -128 || parsed[0] > 128 ||
	    parsed[1] < -25 || parsed[1] > 25) {
		return -EINVAL;
	}
	saturation_pre = parsed[0];
	hue_pre = parsed[1];
	vecm_latch_flag |= FLAG_VADJ1_COLOR;

	return count;
}

static ssize_t amvecm_saturation_hue_post_show(struct class *cla,
					       struct class_attribute *attr,
					       char *buf)
{
	return snprintf(buf, 20, "%d %d\n", saturation_post, hue_post);
}

static ssize_t amvecm_saturation_hue_post_store(struct class *cla,
						struct class_attribute *attr,
						const char *buf, size_t count)
{
	int parsed[2], ret;

	if (likely(parse_para_pq(buf, 2, parsed) != 2))
		return -EINVAL;
	ret = amvecm_set_saturation_hue_post(parsed[0],
					     parsed[1]);
	if (ret < 0)
		return ret;
	return count;
}

static ssize_t amvecm_cm2_show(struct class *cla,
			       struct class_attribute *attr, char *buf)
{
	pr_info("Usage:");
	pr_info(" echo wm addr data0 data1 data2 data3 data4 ");
	pr_info("> /sys/class/amvecm/cm2\n");
	pr_info(" echo rm addr > /sys/class/amvecm/cm2\n");
	return 0;
}

static ssize_t amvecm_cm2_store(struct class *cls,
				struct class_attribute *attr,
				const char *buffer, size_t count)
{
	int n = 0;
	char *buf_orig, *ps, *token;
	char *parm[7];
	u32 addr;
	int data[5] = {0};
	unsigned int addr_port = VPP_CHROMA_ADDR_PORT;/* 0x1d70; */
	unsigned int data_port = VPP_CHROMA_DATA_PORT;/* 0x1d71; */
	long val;
	char delim1[3] = " ";
	char delim2[2] = "\n";
	struct cm_port_s cm_port;

	if (chip_type_id == chip_s5) {
		cm_port = get_cm_port();
		switch (cm_slice_idx) {
		case SLICE0:
			addr_port = cm_port.cm_addr_port[0];
			data_port = cm_port.cm_data_port[0];
			break;
		case SLICE1:
			addr_port = cm_port.cm_addr_port[1];
			data_port = cm_port.cm_data_port[1];
			break;
		case SLICE2:
			addr_port = cm_port.cm_addr_port[2];
			data_port = cm_port.cm_data_port[2];
			break;
		case SLICE3:
			addr_port = cm_port.cm_addr_port[3];
			data_port = cm_port.cm_data_port[3];
			break;
		default:
			break;
		}
	}

	buf_orig = kstrdup(buffer, GFP_KERNEL);
	if (!buf_orig)
		return -ENOMEM;
	ps = buf_orig;
	strcat(delim1, delim2);
	while (1) {
		token = strsep(&ps, delim1);
		if (!token)
			break;
		if (*token == '\0')
			continue;
		parm[n++] = token;
	}
	if (n == 0) {
		pr_info("strsep fail,parm[] uninitialized !\n");
		kfree(buf_orig);
		return -EINVAL;
	}
	if ((parm[0][0] == 'w') && parm[0][1] == 'm') {
		if (n != 7) {
			pr_info("read: invalid parameter\n");
			pr_info("please: cat /sys/class/amvecm/cm2\n");
			kfree(buf_orig);
			return count;
		}
		if (kstrtol(parm[1], 16, &val) < 0)
			return -EINVAL;
		addr = val;
		addr = addr - addr % 8;
		if (kstrtol(parm[2], 16, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		data[0] = val;
		if (kstrtol(parm[3], 16, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		data[1] = val;
		if (kstrtol(parm[4], 16, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		data[2] = val;
		if (kstrtol(parm[5], 16, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		data[3] = val;
		if (kstrtol(parm[6], 16, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		data[4] = val;
		WRITE_VPP_REG(addr_port, addr);
		WRITE_VPP_REG(data_port, data[0]);
		WRITE_VPP_REG(addr_port, addr + 1);
		WRITE_VPP_REG(data_port, data[1]);
		WRITE_VPP_REG(addr_port, addr + 2);
		WRITE_VPP_REG(data_port, data[2]);
		WRITE_VPP_REG(addr_port, addr + 3);
		WRITE_VPP_REG(data_port, data[3]);
		WRITE_VPP_REG(addr_port, addr + 4);
		WRITE_VPP_REG(data_port, data[4]);
		pr_info("wm: [0x%x] <-- 0x0\n", addr);
	} else if ((parm[0][0] == 'r') && parm[0][1] == 'm') {
		if (n != 2) {
			pr_info("read: invalid parameter\n");
			pr_info("please: cat /sys/class/amvecm/cm2\n");
			kfree(buf_orig);
			return count;
		}
		if (kstrtol(parm[1], 16, &val) < 0)
			return -EINVAL;
		addr = val;
		addr = addr - addr % 8;
		WRITE_VPP_REG(addr_port, addr);
		data[0] = READ_VPP_REG(data_port);
		data[0] = READ_VPP_REG(data_port);
		data[0] = READ_VPP_REG(data_port);
		WRITE_VPP_REG(addr_port, addr + 1);
		data[1] = READ_VPP_REG(data_port);
		data[1] = READ_VPP_REG(data_port);
		data[1] = READ_VPP_REG(data_port);
		WRITE_VPP_REG(addr_port, addr + 2);
		data[2] = READ_VPP_REG(data_port);
		data[2] = READ_VPP_REG(data_port);
		data[2] = READ_VPP_REG(data_port);
		WRITE_VPP_REG(addr_port, addr + 3);
		data[3] = READ_VPP_REG(data_port);
		data[3] = READ_VPP_REG(data_port);
		data[3] = READ_VPP_REG(data_port);
		WRITE_VPP_REG(addr_port, addr + 4);
		data[4] = READ_VPP_REG(data_port);
		data[4] = READ_VPP_REG(data_port);
		data[4] = READ_VPP_REG(data_port);
		pr_info("rm:[0x%x]-->[0x%x][0x%x][0x%x][0x%x][0x%x]\n",
			addr, data[0], data[1],
				data[2], data[3], data[4]);
	} else {
		pr_info("invalid command\n");
		pr_info("please: cat /sys/class/amvecm/bit");
	}
	kfree(buf_orig);
	return count;
}

/*cm2 v2 cmd used for s5 4slice*/
static ssize_t amvecm_cm2_idx_show(struct class *cla,
			       struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", cm_slice_idx);
}

static ssize_t amvecm_cm2_idx_store(struct class *cls,
				struct class_attribute *attr,
				const char *buf, size_t count)
{
	size_t r;
	int val;

	r = sscanf(buf, "%d\n", &val);
	if (r != 1 || val > 3)
		return -EINVAL;

	cm_slice_idx = val;
	return count;

}

static ssize_t amvecm_cm_reg_show(struct class *cla,
				  struct class_attribute *attr, char *buf)
{
	pr_info("Usage: echo addr value > /sys/class/amvecm/cm_reg");
	return 0;
}

static ssize_t amvecm_cm_reg_store(struct class *cls,
				   struct class_attribute *attr,
		 const char *buffer, size_t count)
{
	int data[5] = {0};
	unsigned int addr, value;
	long val = 0;
	int i, node, reg_node;
	unsigned int addr_port = VPP_CHROMA_ADDR_PORT;/* 0x1d70; */
	unsigned int data_port = VPP_CHROMA_DATA_PORT;/* 0x1d71; */
	char *buf_orig, *parm[2] = {NULL};
	struct cm_port_s cm_port;

	if (!buffer)
		return count;
	buf_orig = kstrdup(buffer, GFP_KERNEL);
	if (!buf_orig)
		return -ENOMEM;
	parse_param_amvecm(buf_orig, (char **)&parm);

	if (chip_type_id == chip_s5) {
		cm_port = get_cm_port();
		switch (cm_slice_idx) {
		case SLICE0:
			addr_port = cm_port.cm_addr_port[0];
			data_port = cm_port.cm_data_port[0];
			break;
		case SLICE1:
			addr_port = cm_port.cm_addr_port[1];
			data_port = cm_port.cm_data_port[1];
			break;
		case SLICE2:
			addr_port = cm_port.cm_addr_port[2];
			data_port = cm_port.cm_data_port[2];
			break;
		case SLICE3:
			addr_port = cm_port.cm_addr_port[3];
			data_port = cm_port.cm_data_port[3];
			break;
		default:
			break;
		}
	}

	if (kstrtoul(parm[0], 16, &val) < 0) {
		kfree(buf_orig);
		return -EINVAL;
	}
	addr = val;
	if (kstrtoul(parm[1], 16, &val) < 0) {
		kfree(buf_orig);
		return -EINVAL;
	}
	value = val;

	node = (addr - 0x100) / 8;
	reg_node = (addr - 0x100) % 8;

	for (i = 0; i < 5; i++) {
		if (i == reg_node) {
			data[i] = value;
			continue;
		}
		addr = node * 8 + 0x100 + i;
		WRITE_VPP_REG(addr_port, addr);
		data[i] = READ_VPP_REG(data_port);
	}

	for (i = 0; i < 5; i++) {
		addr = node * 8 + 0x100 + i;
		WRITE_VPP_REG(addr_port, addr);
		WRITE_VPP_REG(data_port, data[i]);
	}

	kfree(buf_orig);
	return count;
}

static ssize_t amvecm_write_reg_show(struct class *cla,
				     struct class_attribute *attr, char *buf)
{
	pr_info("Usage: echo w addr value > /sys/class/amvecm/pq_reg_rw\n");
	pr_info("Usage: echo bw addr value start length > /...\n");
	pr_info("Usage: echo r addr > /sys/class/amvecm/pq_reg_rw\n");
	pr_info("addr and value must be hex\n");
	return 0;
}

static ssize_t amvecm_write_reg_store(struct class *cls,
				      struct class_attribute *attr,
		 const char *buffer, size_t count)
{
	unsigned int addr, value, bitstart, bitlength;
	long val = 0;
	char *buf_orig, *parm[5] = {NULL};

	if (!buffer)
		return count;
	buf_orig = kstrdup(buffer, GFP_KERNEL);
	if (!buf_orig)
		return -ENOMEM;
	parse_param_amvecm(buf_orig, (char **)&parm);

	if (!strncmp(parm[0], "r", 1)) {
		if (kstrtoul(parm[1], 16, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		addr = val;
		value = READ_VPP_REG(addr);
		pr_info("0x%x=0x%x\n", addr, value);
	} else if (!strncmp(parm[0], "w", 1)) {
		if (kstrtoul(parm[1], 16, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		addr = val;
		if (kstrtoul(parm[2], 16, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		value = val;
		WRITE_VPP_REG(addr, value);
	} else if (!strncmp(parm[0], "bw", 2)) {
		if (kstrtoul(parm[1], 16, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		addr = val;
		if (kstrtoul(parm[2], 16, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		value = val;
		if (kstrtoul(parm[3], 16, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		bitstart = val;
		if (kstrtoul(parm[4], 16, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		bitlength = val;
		WRITE_VPP_REG_BITS(addr, value, bitstart, bitlength);
	}

	kfree(buf_orig);
	return count;
}

static unsigned int cal_crc32(unsigned int crc, const unsigned char *buf, int buf_len)
{
	unsigned int crcu32 = crc;
	unsigned char b;
	unsigned int s_crc32[16] = {
		0, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
		0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
		0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
		0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c,
	};

	if (buf_len <= 0)
		return 0;
	if (!buf)
		return 0;

	crcu32 = ~crcu32;
	while (buf_len--) {
		b = *buf++;
		crcu32 = (crcu32 >> 4) ^ s_crc32[(crcu32 & 0xf) ^ (b & 0xf)];
		crcu32 = (crcu32 >> 4) ^ s_crc32[(crcu32 & 0xf) ^ (b >> 4)];
	}

	return ~crcu32;
}

static ssize_t amvecm_gamma_show(struct class *cls,
				 struct class_attribute *attr,
			char *buf)
{
	int i;
	int len = 0;

	if (vecm_latch_flag2 & GAMMA_READ_R) {
		for (i = 0; i < 256; i++)
			len += sprintf(buf + len, "%03x", gamma_data_r[i]);
		len += sprintf(buf + len, "\n");
		vecm_latch_flag2 &= ~GAMMA_READ_R;
		return len;
	}

	if (vecm_latch_flag2 & GAMMA_READ_G) {
		for (i = 0; i < 256; i++)
			len += sprintf(buf + len, "%03x", gamma_data_g[i]);
		len += sprintf(buf + len, "\n");
		vecm_latch_flag2 &= ~GAMMA_READ_G;
		return len;
	}

	if (vecm_latch_flag2 & GAMMA_READ_B) {
		for (i = 0; i < 256; i++)
			len += sprintf(buf + len, "%03x", gamma_data_b[i]);
		len += sprintf(buf + len, "\n");
		vecm_latch_flag2 &= ~GAMMA_READ_B;
		return len;
	}

	if (vecm_latch_flag2 & GAMMA_CRC_PASS) {
		len += sprintf(buf + len, "gamma set crc pass\n");
		vecm_latch_flag2 &= ~GAMMA_CRC_PASS;
		return len;
	}

	if (vecm_latch_flag2 & GAMMA_CRC_FAIL) {
		len += sprintf(buf + len, "gamma set crc fail\n");
		vecm_latch_flag2 &= ~GAMMA_CRC_FAIL;
		return len;
	}

	pr_info("Usage:");
	pr_info("	echo sgr|sgg|sgb xxx...xx > /sys/class/amvecm/gamma\n");
	pr_info("Notes:\n");
	pr_info("	if the string xxx......xx is less than 256*3,");
	pr_info("	then the remaining will be set value 0\n");
	pr_info("	if the string xxx......xx is more than 256*3, ");
	pr_info("	then the remaining will be ignored\n");
	pr_info("Usage:");
	pr_info("	echo ggr|ggg|ggb xxx > /sys/class/amvecm/gamma\n");
	pr_info("Notes:\n");
	pr_info("	read all as point......xxx is 'all'.\n");
	pr_info("	read all as strings......xxx is 'all_str'.\n");
	pr_info("	read one point......xxx is a value '0~255'.\n ");
	return 0;
}

static ssize_t amvecm_gamma_store(struct class *cls,
				  struct class_attribute *attr,
			const char *buffer, size_t count)
{
	int n = 0;
	char *buf_orig, *ps, *token;
	char *parm[4] = {NULL};
	unsigned short *gamma_r;
	unsigned int gamma_count;
	char gamma[4];
	int i = 0;
	long val;
	char delim1[3] = " ";
	char delim2[2] = "\n";
	char *stemp = NULL;
	unsigned int len;
	unsigned int crc_data;

	stemp = kmalloc(600, GFP_KERNEL);
	if (!stemp)
		return -ENOMEM;

	gamma_r = kmalloc(256 * sizeof(unsigned short), GFP_KERNEL);
	if (!gamma_r) {
		kfree(stemp);
		return -ENOMEM;
	}

	buf_orig = kstrdup(buffer, GFP_KERNEL);
	if (!buf_orig)
		goto free_buf;
	ps = buf_orig;
	strcat(delim1, delim2);
	while (1) {
		token = strsep(&ps, delim1);
		if (!token)
			break;
		if (*token == '\0')
			continue;
		parm[n++] = token;
	}
	if (!gamma_r || !stemp || n == 0)
		goto free_buf;

	// parm[0] sgr/sgg/sgb/ggr/ggg/ggb
	if ((parm[0][0] == 's') && (parm[0][1] == 'g') &&
		((parm[0][2] == 'r') || (parm[0][2] == 'g') || (parm[0][2] == 'b'))) {
		// parm[1] gamma data (256 * 3 Bytes --- 10bit, need 3 ASCII Bytes)
		len = strlen(parm[1]);
		if (len != 768) {
			vecm_latch_flag2 |= GAMMA_CRC_FAIL;
			pr_info("data length is not 768 Bytes.\n");
			goto free_buf;
		}

		//gamma data should be hex character
		for (i = 0; i < len; i++) {
			if ((parm[1][i] - '0') < 10 || (parm[1][i] | 0x20) - 'a' < 6)
				continue;

			pr_info("error char\n");
			goto free_buf;
		}

		//parm[2] crc value
		if (parm[2]) {
			crc_data = cal_crc32(0, parm[1], len);
			if (kstrtoul(parm[2], 16, &val) < 0) {
				pr_info("cmd crc error\n");
				goto free_buf;
			}
			if (crc_data == val) {
				vecm_latch_flag2 |= GAMMA_CRC_PASS;
			} else {
				vecm_latch_flag2 |= GAMMA_CRC_FAIL;
				goto free_buf;
			}
		}
		memset(gamma_r, 0, 256 * sizeof(unsigned short));
		gamma_count = (strlen(parm[1]) + 2) / 3;
		if (gamma_count > 256)
			gamma_count = 256;

		for (i = 0; i < gamma_count; ++i) {
			gamma[0] = parm[1][3 * i + 0];
			gamma[1] = parm[1][3 * i + 1];
			gamma[2] = parm[1][3 * i + 2];
			gamma[3] = '\0';
			if (kstrtol(gamma, 16, &val) < 0)
				goto free_buf;
			gamma_r[i] = val;
		}

		switch (parm[0][2]) {
		case 'r':
			amve_write_gamma_table(gamma_r, H_SEL_R);
			break;

		case 'g':
			amve_write_gamma_table(gamma_r, H_SEL_G);
			break;

		case 'b':
			amve_write_gamma_table(gamma_r, H_SEL_B);
			break;
		default:
			break;
		}
	} else if (!strcmp(parm[0], "ggr")) {
		vpp_get_lcd_gamma_table(H_SEL_R);
		if (!strcmp(parm[1], "all")) {
			for (i = 0; i < 256; i++)
				pr_info("gamma_r[%d] = %x\n",
					i, gamma_data_r[i]);
		} else if (!strcmp(parm[1], "all_str")) {
			if (!parm[2]) {
				for (i = 0; i < 256; i++)
					d_convert_str(gamma_data_r[i], i, stemp, 3, 16);
				pr_info("gamma_r str: %s\n", stemp);
			} else if (!strcmp(parm[2], "adb")) {
				vecm_latch_flag2 |= GAMMA_READ_R;
			}
		} else {
			if (kstrtoul(parm[1], 10, &val) < 0) {
				pr_info("invalid command\n");
				goto free_buf;
			}
			i = val;
			if (i >= 0 && i <= 255)
				pr_info("gamma_r[%d] = %x\n",
					i, gamma_data_r[i]);
		}
	} else if (!strcmp(parm[0], "ggg")) {
		vpp_get_lcd_gamma_table(H_SEL_G);
		if (!strcmp(parm[1], "all")) {
			for (i = 0; i < 256; i++)
				pr_info("gamma_g[%d] = %x\n",
					i, gamma_data_g[i]);
		} else if (!strcmp(parm[1], "all_str")) {
			if (!parm[2]) {
				for (i = 0; i < 256; i++)
					d_convert_str(gamma_data_g[i], i, stemp, 3, 16);
				pr_info("gamma_g str: %s\n", stemp);
			} else if (!strcmp(parm[2], "adb")) {
				vecm_latch_flag2 |= GAMMA_READ_G;
			}
		} else {
			if (kstrtoul(parm[1], 10, &val) < 0) {
				pr_info("invalid command\n");
				goto free_buf;
			}
			i = val;
			if (i >= 0 && i <= 255)
				pr_info("gamma_g[%d] = %x\n",
					i, gamma_data_g[i]);
		}

	} else if (!strcmp(parm[0], "ggb")) {
		vpp_get_lcd_gamma_table(H_SEL_B);
		if (!strcmp(parm[1], "all")) {
			for (i = 0; i < 256; i++)
				pr_info("gamma_b[%d] = %x\n",
					i, gamma_data_b[i]);
		} else if (!strcmp(parm[1], "all_str")) {
			if (!parm[2]) {
				for (i = 0; i < 256; i++)
					d_convert_str(gamma_data_b[i], i, stemp, 3, 16);
				pr_info("gamma_b str: %s\n", stemp);
			} else if (!strcmp(parm[2], "adb")) {
				vecm_latch_flag2 |= GAMMA_READ_B;
			}
		} else {
			if (kstrtoul(parm[1], 10, &val) < 0) {
				pr_info("invalid command\n");
				goto free_buf;
			}
			i = val;
			if (i >= 0 && i <= 255)
				pr_info("gamma_b[%d] = %x\n",
					i, gamma_data_b[i]);
		}
	} else {
		pr_info("invalid command\n");
		pr_info("please: cat /sys/class/amvecm/gamma");
	}
	kfree(buf_orig);
	kfree(stemp);
	kfree(gamma_r);
	return count;
free_buf:
	kfree(buf_orig);
	kfree(stemp);
	kfree(gamma_r);
	return -EINVAL;
}

static ssize_t amvecm_gamma_v2_show(struct class *cls,
				 struct class_attribute *attr,
			char *buf)
{
	int i;
	int len = 0;
	struct gamma_data_s *p_gm;
	int max_idx;

	if (chip_type_id < chip_a4)
		return 0;

	p_gm = get_gm_data();
	max_idx = p_gm->max_idx;

	if (vecm_latch_flag2 & GAMMA_READ_R) {
		for (i = 0; i < max_idx; i++)
			len += sprintf(buf + len, "%03x",
				p_gm->dbg_gm_tbl.gamma_r[i]);
		len += sprintf(buf + len, "\n");
		vecm_latch_flag2 &= ~GAMMA_READ_R;
		return len;
	}

	if (vecm_latch_flag2 & GAMMA_READ_G) {
		for (i = 0; i < max_idx; i++)
			len += sprintf(buf + len, "%03x",
				p_gm->dbg_gm_tbl.gamma_g[i]);
		len += sprintf(buf + len, "\n");
		vecm_latch_flag2 &= ~GAMMA_READ_G;
		return len;
	}

	if (vecm_latch_flag2 & GAMMA_READ_B) {
		for (i = 0; i < max_idx; i++)
			len += sprintf(buf + len, "%03x",
				p_gm->dbg_gm_tbl.gamma_b[i]);
		len += sprintf(buf + len, "\n");
		vecm_latch_flag2 &= ~GAMMA_READ_B;
		return len;
	}

	if (vecm_latch_flag2 & GAMMA_CRC_PASS) {
		len += sprintf(buf + len, "gamma set crc pass\n");
		vecm_latch_flag2 &= ~GAMMA_CRC_PASS;
		return len;
	}

	if (vecm_latch_flag2 & GAMMA_CRC_FAIL) {
		len += sprintf(buf + len, "gamma set crc fail\n");
		vecm_latch_flag2 &= ~GAMMA_CRC_FAIL;
		return len;
	}

	pr_info("Usage:");
	pr_info("	echo sgr|sgg|sgb xxx...xx > /sys/class/amvecm/gamma_v2\n");
	pr_info("Notes:\n");
	pr_info("	if the string xxx......xx is less than 257*3,");
	pr_info("	then the remaining will be set value 0\n");
	pr_info("	if the string xxx......xx is more than 257*3, ");
	pr_info("	then the remaining will be ignored\n");
	pr_info("Usage:");
	pr_info("	echo ggr|ggg|ggb xxx > /sys/class/amvecm/gamma_v2\n");
	pr_info("Notes:\n");
	pr_info("	read all as point......xxx is 'all'.\n");
	pr_info("	read all as strings......xxx is 'all_str'.\n");
	pr_info("	read one point......xxx is a value '0~256'.\n ");
	return 0;
}

static ssize_t amvecm_gamma_v2_store(struct class *cls,
				  struct class_attribute *attr,
			const char *buffer, size_t count)
{
	int n = 0;
	char *buf_orig, *ps, *token;
	char *parm[4] = {NULL};
	unsigned short *gm_cv;
	unsigned int gamma_count;
	char gamma[4];
	int i = 0;
	long val;
	char delim1[3] = " ";
	char delim2[2] = "\n";
	char *stemp = NULL;
	unsigned int len;
	unsigned int crc_data;
	struct gamma_data_s *p_gm;
	int max_idx;

	stemp = kmalloc(600, GFP_KERNEL);
	if (!stemp)
		return -ENOMEM;

	if (chip_type_id < chip_a4) {
		kfree(stemp);
		return -EIO;
	}

	p_gm = get_gm_data();
	max_idx = p_gm->max_idx;

	gm_cv = kmalloc_array(max_idx, sizeof(unsigned short), GFP_KERNEL);
	if (!gm_cv) {
		kfree(stemp);
		return -ENOMEM;
	}

	buf_orig = kstrdup(buffer, GFP_KERNEL);
	if (!buf_orig)
		goto free_buf;
	ps = buf_orig;
	strcat(delim1, delim2);
	while (1) {
		token = strsep(&ps, delim1);
		if (!token)
			break;
		if (*token == '\0')
			continue;
		parm[n++] = token;
	}
	if (!gm_cv || !stemp || n == 0)
		goto free_buf;

	// parm[0] sgr/sgg/sgb/ggr/ggg/ggb
	if ((parm[0][0] == 's') && (parm[0][1] == 'g') &&
		((parm[0][2] == 'r') || (parm[0][2] == 'g') || (parm[0][2] == 'b'))) {
		len = strlen(parm[1]);
		if (len != max_idx * 3) {
			vecm_latch_flag2 |= GAMMA_CRC_FAIL;
			pr_info("data length is not 771 Bytes.\n");
			goto free_buf;
		}

		//gamma data should be hex character
		for (i = 0; i < len; i++) {
			if ((parm[1][i] - '0') < 10 || (parm[1][i] | 0x20) - 'a' < 6)
				continue;
			pr_info("error char\n");
			goto free_buf;
		}

		//parm[2] crc value
		if (parm[2]) {
			crc_data = cal_crc32(0, parm[1], len);
			if (kstrtoul(parm[2], 16, &val) < 0) {
				pr_info("cmd crc error\n");
				goto free_buf;
			}
			if (crc_data == val) {
				vecm_latch_flag2 |= GAMMA_CRC_PASS;
			} else {
				vecm_latch_flag2 |= GAMMA_CRC_FAIL;
				goto free_buf;
			}
		}
		memset(gm_cv, 0, max_idx * sizeof(unsigned short));
		gamma_count = (strlen(parm[1]) + 2) / 3;
		if (gamma_count > max_idx)
			gamma_count = max_idx;

		for (i = 0; i < gamma_count; ++i) {
			gamma[0] = parm[1][3 * i + 0];
			gamma[1] = parm[1][3 * i + 1];
			gamma[2] = parm[1][3 * i + 2];
			gamma[3] = '\0';
			if (kstrtol(gamma, 16, &val) < 0)
				goto free_buf;
			gm_cv[i] = val;
		}

		switch (parm[0][2]) {
		case 'r':
			amve_write_gamma_table(gm_cv, H_SEL_R);
			break;

		case 'g':
			amve_write_gamma_table(gm_cv, H_SEL_G);
			break;

		case 'b':
			amve_write_gamma_table(gm_cv, H_SEL_B);
			break;
		default:
			break;
		}
	} else if (!strcmp(parm[0], "ggr")) {
		vpp_get_lcd_gamma_table(H_SEL_R);
		if (!strcmp(parm[1], "all")) {
			for (i = 0; i < max_idx; i++)
				pr_info("gamma_r[%d] = %x\n",
					i, p_gm->dbg_gm_tbl.gamma_r[i]);
		} else if (!strcmp(parm[1], "all_str")) {
			if (!parm[2]) {
				for (i = 0; i < max_idx; i++)
					d_convert_str(p_gm->dbg_gm_tbl.gamma_r[i], i, stemp, 3, 16);
				pr_info("gamma_r str: %s\n", stemp);
			} else if (!strcmp(parm[2], "adb")) {
				vecm_latch_flag2 |= GAMMA_READ_R;
			}
		} else {
			if (kstrtoul(parm[1], 10, &val) < 0) {
				pr_info("invalid command\n");
				goto free_buf;
			}
			i = val;
			if (i >= 0 && i < max_idx)
				pr_info("gamma_r[%d] = %x\n",
					i, p_gm->dbg_gm_tbl.gamma_r[i]);
		}
	} else if (!strcmp(parm[0], "ggg")) {
		vpp_get_lcd_gamma_table(H_SEL_G);
		if (!strcmp(parm[1], "all")) {
			for (i = 0; i < max_idx; i++)
				pr_info("gamma_g[%d] = %x\n",
					i, p_gm->dbg_gm_tbl.gamma_g[i]);
		} else if (!strcmp(parm[1], "all_str")) {
			if (!parm[2]) {
				for (i = 0; i < max_idx; i++)
					d_convert_str(p_gm->dbg_gm_tbl.gamma_g[i], i, stemp, 3, 16);
				pr_info("gamma_g str: %s\n", stemp);
			} else if (!strcmp(parm[2], "adb")) {
				vecm_latch_flag2 |= GAMMA_READ_G;
			}
		} else {
			if (kstrtoul(parm[1], 10, &val) < 0) {
				pr_info("invalid command\n");
				goto free_buf;
			}
			i = val;
			if (i >= 0 && i < max_idx)
				pr_info("gamma_g[%d] = %x\n",
					i, p_gm->dbg_gm_tbl.gamma_g[i]);
		}

	} else if (!strcmp(parm[0], "ggb")) {
		vpp_get_lcd_gamma_table(H_SEL_B);
		if (!strcmp(parm[1], "all")) {
			for (i = 0; i < max_idx; i++)
				pr_info("gamma_b[%d] = %x\n",
					i, p_gm->dbg_gm_tbl.gamma_b[i]);
		} else if (!strcmp(parm[1], "all_str")) {
			if (!parm[2]) {
				for (i = 0; i < max_idx; i++)
					d_convert_str(p_gm->dbg_gm_tbl.gamma_b[i], i, stemp, 3, 16);
				pr_info("gamma_b str: %s\n", stemp);
			} else if (!strcmp(parm[2], "adb")) {
				vecm_latch_flag2 |= GAMMA_READ_B;
			}
		} else {
			if (kstrtoul(parm[1], 10, &val) < 0) {
				pr_info("invalid command\n");
				goto free_buf;
			}
			i = val;
			if (i >= 0 && i < max_idx)
				pr_info("gamma_b[%d] = %x\n",
					i, p_gm->dbg_gm_tbl.gamma_b[i]);
		}
	} else if (!strcmp(parm[0], "ggr_sub")) {
		vpp_get_lcd_gamma_table_sub();
		if (!strcmp(parm[1], "all")) {
			for (i = 0; i < 256; i++)
				pr_info("gamma_r[%d] = %x\n",
					i, gamma_data_r[i]);
		} else if (!strcmp(parm[1], "all_str")) {
			for (i = 0; i < 256; i++)
				d_convert_str(gamma_data_r[i], i, stemp, 3, 16);
			pr_info("gamma_r str: %s\n", stemp);
		} else {
			if (kstrtoul(parm[1], 10, &val) < 0) {
				pr_info("invalid command\n");
				goto free_buf;
			}
			i = val;
			if (i >= 0 && i <= 255)
				pr_info("gamma_r[%d] = %x\n",
					i, gamma_data_r[i]);
			}
	} else if (!strcmp(parm[0], "ggg_sub")) {
		vpp_get_lcd_gamma_table_sub();
		if (!strcmp(parm[1], "all")) {
			for (i = 0; i < 256; i++)
				pr_info("gamma_g[%d] = %x\n",
					i, gamma_data_g[i]);
		} else if (!strcmp(parm[1], "all_str")) {
			for (i = 0; i < 256; i++)
				d_convert_str(gamma_data_g[i], i, stemp, 3, 16);
			pr_info("gamma_g str: %s\n", stemp);
		} else {
			if (kstrtoul(parm[1], 10, &val) < 0) {
				pr_info("invalid command\n");
				goto free_buf;
			}
			i = val;
			if (i >= 0 && i <= 255)
				pr_info("gamma_g[%d] = %x\n",
					i, gamma_data_g[i]);
		}

	} else if (!strcmp(parm[0], "ggb_sub")) {
		vpp_get_lcd_gamma_table_sub();
		if (!strcmp(parm[1], "all")) {
			for (i = 0; i < 256; i++)
				pr_info("gamma_b[%d] = %x\n",
					i, gamma_data_b[i]);
		} else if (!strcmp(parm[1], "all_str")) {
			for (i = 0; i < 256; i++)
				d_convert_str(gamma_data_b[i], i, stemp, 3, 16);
			pr_info("gamma_b str: %s\n", stemp);
		} else {
			if (kstrtoul(parm[1], 10, &val) < 0) {
				pr_info("invalid command\n");
				goto free_buf;
			}
			i = val;
			if (i >= 0 && i <= 255)
				pr_info("gamma_b[%d] = %x\n",
					i, gamma_data_b[i]);
		}
	} else {
		pr_info("invalid command\n");
		pr_info("please: cat /sys/class/amvecm/gamma");
	}
	kfree(buf_orig);
	kfree(stemp);
	kfree(gm_cv);
	return count;
free_buf:
	kfree(buf_orig);
	kfree(stemp);
	kfree(gm_cv);
	return -EINVAL;
}

static ssize_t set_gamma_pattern_show(struct class *cla,
				      struct class_attribute *attr, char *buf)
{
	pr_info("8bit: echo r g b > /sys/class/amvecm/gamma_pattern\n");
	pr_info("10bit: echo r g b 0xa > /sys/class/amvecm/gamma_pattern\n");
	pr_info("	r g b should be hex\n");
	pr_info("disable gamma pattern:\n");
	pr_info("echo disable > /sys/class/amvecm/gamma_pattern\n");
	return 0;
}

static ssize_t set_gamma_pattern_store(struct class *cls,
				       struct class_attribute *attr,
			const char *buffer, size_t count)
{
	static unsigned short r_val[257], g_val[257], b_val[257];
	int n = 0;
	char *buf_orig, *ps, *token;
	char *parm[4];
	unsigned int gamma[3];
	long val, i;
	char deliml[3] = " ";
	char delim2[2] = "\n";

	buf_orig = kstrdup(buffer, GFP_KERNEL);
	if (!buf_orig)
		return -ENOMEM;
	ps = buf_orig;
	strcat(deliml, delim2);
	*(parm + 3) = NULL;
	while (1) {
		token = strsep(&ps, deliml);
		if (!token)
			break;
		if (*token == '\0')
			continue;
		parm[n++] = token;
	}

	if (!strcmp(parm[0], "disable")) {
		vecm_latch_flag |= FLAG_GAMMA_TABLE_R;
		vecm_latch_flag |= FLAG_GAMMA_TABLE_G;
		vecm_latch_flag |= FLAG_GAMMA_TABLE_B;
		kfree(buf_orig);
		return count;
	}

	if (*(parm + 3) != NULL) {
		if (kstrtol(parm[3], 16, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		if (val == 10) {
			if (kstrtol(parm[0], 16, &val) < 0) {
				kfree(buf_orig);
				return -EINVAL;
			}
			gamma[0] = val;

			if (kstrtol(parm[1], 16, &val) < 0) {
				kfree(buf_orig);
				return -EINVAL;
			}
			gamma[1] = val;

			if (kstrtol(parm[2], 16, &val) < 0) {
				kfree(buf_orig);
				return -EINVAL;
			}
			gamma[2] = val;
		} else {
			kfree(buf_orig);
			return count;
		}
	} else {
		if (kstrtol(parm[0], 16, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		gamma[0] = val << 2;

		if (kstrtol(parm[1], 16, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		gamma[1] = val << 2;

		if (kstrtol(parm[2], 16, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		gamma[2] = val << 2;
	}

	for (i = 0; i < 257; i++) {
		r_val[i] = gamma[0];
		g_val[i] = gamma[1];
		b_val[i] = gamma[2];
	}

	if (cpu_after_eq_t7()) {
		lcd_gamma_api(gamma_index, r_val, g_val, b_val, 0, 0);
	} else {
		amve_write_gamma_table(r_val, H_SEL_R);
		amve_write_gamma_table(g_val, H_SEL_G);
		amve_write_gamma_table(b_val, H_SEL_B);
	}

	kfree(buf_orig);
	return count;
}

void white_balance_adjust(int sel, int value)
{
	switch (sel) {
	/*0: en*/
	/*1: pre r   2: pre g   3: pre b*/
	/*4: gain r  5: gain g  6: gain b*/
	/*7: post r  8: post g  9: post b*/
	case 0:
		video_rgb_ogo.en = value;
		break;
	case 1:
		video_rgb_ogo.r_pre_offset = value;
		break;
	case 2:
		video_rgb_ogo.g_pre_offset = value;
		break;
	case 3:
		video_rgb_ogo.b_pre_offset = value;
		break;
	case 4:
		video_rgb_ogo.r_gain = value;
		break;
	case 5:
		video_rgb_ogo.g_gain = value;
		break;
	case 6:
		video_rgb_ogo.b_gain = value;
		break;
	case 7:
		video_rgb_ogo.r_post_offset = value;
		break;
	case 8:
		video_rgb_ogo.g_post_offset = value;
		break;
	case 9:
		video_rgb_ogo.b_post_offset = value;
		break;
	default:
		break;
	}
	ve_ogo_param_update();
}

static int wb_dbg_flag;
static int wb_rd_val;
static ssize_t amvecm_wb_show(struct class *cla,
			      struct class_attribute *attr, char *buf)
{
	if (wb_dbg_flag & WB_PARAM_RD_UPDATE) {
		wb_dbg_flag &= ~WB_PARAM_RD_UPDATE;
		return sprintf(buf, "%d\n", wb_rd_val);
	}

	pr_info("read:	echo r gain_r > /sys/class/amvecm/wb;");
	pr_info("cat /sys/class/amvecm/wb\n");
	pr_info("read:	echo r pre_r > /sys/class/amvecm/wb;");
	pr_info("cat /sys/class/amvecm/wb\n");
	pr_info("read:	echo r post_r > /sys/class/amvecm/wb;");
	pr_info("cat /sys/class/amvecm/wb\n");
	pr_info("write:	echo gain_r value > /sys/class/amvecm/wb\n");
	pr_info("write:	echo preofst_r value > /sys/class/amvecm/wb\n");
	pr_info("write:	echo postofst_r value > /sys/class/amvecm/wb\n");
	return 0;
}

static ssize_t amvecm_wb_store(struct class *cls,
			       struct class_attribute *attr,
			const char *buffer, size_t count)
{
	char *buf_orig, *parm[8] = {NULL};
	long value;

	if (!buffer)
		return count;
	buf_orig = kstrdup(buffer, GFP_KERNEL);
	if (!buf_orig)
		return -ENOMEM;
	parse_param_amvecm(buf_orig, (char **)&parm);

	if (!strncmp(parm[0], "r", 1)) {
		if (!strncmp(parm[1], "pre_r", 5)) {
			wb_dbg_flag |= WB_PARAM_RD_UPDATE;
			wb_rd_val = video_rgb_ogo.r_pre_offset;
		} else if (!strncmp(parm[1], "pre_g", 5)) {
			wb_dbg_flag |= WB_PARAM_RD_UPDATE;
			wb_rd_val = video_rgb_ogo.g_pre_offset;
		} else if (!strncmp(parm[1], "pre_b", 5)) {
			wb_dbg_flag |= WB_PARAM_RD_UPDATE;
			wb_rd_val = video_rgb_ogo.b_pre_offset;
		} else if (!strncmp(parm[1], "gain_r", 6)) {
			wb_dbg_flag |= WB_PARAM_RD_UPDATE;
			wb_rd_val = video_rgb_ogo.r_gain;
		} else if (!strncmp(parm[1], "gain_g", 6)) {
			wb_dbg_flag |= WB_PARAM_RD_UPDATE;
			wb_rd_val = video_rgb_ogo.g_gain;
		} else if (!strncmp(parm[1], "gain_b", 6)) {
			wb_dbg_flag |= WB_PARAM_RD_UPDATE;
			wb_rd_val = video_rgb_ogo.b_gain;
		} else if (!strncmp(parm[1], "post_r", 6)) {
			wb_dbg_flag |= WB_PARAM_RD_UPDATE;
			wb_rd_val = video_rgb_ogo.r_post_offset;
		} else if (!strncmp(parm[1], "post_g", 6)) {
			wb_dbg_flag |= WB_PARAM_RD_UPDATE;
			wb_rd_val = video_rgb_ogo.g_post_offset;
		} else if (!strncmp(parm[1], "post_b", 6)) {
			wb_dbg_flag |= WB_PARAM_RD_UPDATE;
			wb_rd_val = video_rgb_ogo.b_post_offset;
		} else if (!strncmp(parm[1], "en", 2)) {
			wb_dbg_flag |= WB_PARAM_RD_UPDATE;
			wb_rd_val = video_rgb_ogo.en;
		}
	} else {
		if (kstrtol(parm[1], 10, &value) < 0)
			return -EINVAL;
		if (!strncmp(parm[0], "wb_en", 5)) {
			white_balance_adjust(0, value);
			pr_info("\t set wb en\n");
		} else if (!strncmp(parm[0], "preofst_r", 9)) {
			if (value > 1023 || value < -1024) {
				pr_info("\t preofst r over range\n");
			} else {
				white_balance_adjust(1, value);
				pr_info("\t set wb preofst r\n");
			}
		} else if (!strncmp(parm[0], "preofst_g", 9)) {
			if (value > 1023 || value < -1024) {
				pr_info("\t preofst g over range\n");
			} else {
				white_balance_adjust(2, value);
				pr_info("\t set wb preofst g\n");
			}
		} else if (!strncmp(parm[0], "preofst_b", 9)) {
			if (value > 1023 || value < -1024) {
				pr_info("\t preofst b over range\n");
			} else {
				white_balance_adjust(3, value);
				pr_info("\t set wb preofst b\n");
			}
		} else if (!strncmp(parm[0], "gain_r", 6)) {
			if (value > 2047 || value < 0) {
				pr_info("\t gain r over range\n");
			} else {
				white_balance_adjust(4, value);
				pr_info("\t set wb gain r\n");
			}
		} else if (!strncmp(parm[0], "gain_g", 6)) {
			if (value > 2047 || value < 0) {
				pr_info("\t gain g over range\n");
			} else {
				white_balance_adjust(5, value);
				pr_info("\t set wb gain g\n");
			}
		} else if (!strncmp(parm[0], "gain_b", 6)) {
			if (value > 2047 || value < 0) {
				pr_info("\t gain b over range\n");
			} else {
				white_balance_adjust(6, value);
				pr_info("\t set wb gain b\n");
			}
		} else if (!strncmp(parm[0], "postofst_r", 10)) {
			if (value > 1023 || value < -1024) {
				pr_info("\t postofst r over range\n");
			} else {
				white_balance_adjust(7, value);
				pr_info("\t set wb postofst r\n");
			}
		} else if (!strncmp(parm[0], "postofst_g", 10)) {
			if (value > 1023 || value < -1024) {
				pr_info("\t postofst g over range\n");
			} else {
				white_balance_adjust(8, value);
				pr_info("\t set wb postofst g\n");
			}
		} else if (!strncmp(parm[0], "postofst_b", 10)) {
			if (value > 1023 || value < -1024) {
				pr_info("\t postofst b over range\n");
			} else {
				white_balance_adjust(9, value);
				pr_info("\t set wb postofst b\n");
			}
		}
	}

	kfree(buf_orig);
	return count;
}

static ssize_t set_hdr_289lut_show(struct class *cla,
				   struct class_attribute *attr, char *buf)
{
	int i;

	for (i = 0; i < 289; i++) {
		pr_info("0x%-8x\t", lut_289_mapping[i]);
		if ((i + 1) % 8 == 0)
			pr_info("\n");
	}
	return 0;
}

static ssize_t set_hdr_289lut_store(struct class *cls,
				    struct class_attribute *attr,
				    const char *buffer, size_t count)
{
	int n = 0;
	char *buf_orig, *ps, *token;
	char *parm[4];
	unsigned short *hdr289lut;
	unsigned int gamma_count;
	char gamma[4];
	int i = 0;
	long val;
	char deliml[3] = " ";
	char delim2[2] = "\n";

	hdr289lut = kmalloc(289 * sizeof(unsigned short), GFP_KERNEL);

	buf_orig = kstrdup(buffer, GFP_KERNEL);
	if (!buf_orig) {
		kfree(hdr289lut);
		return -ENOMEM;
	}
	ps = buf_orig;
	strcat(deliml, delim2);
	while (1) {
		token = strsep(&ps, deliml);
		if (!token)
			break;
		if (*token == '\0')
			continue;
		parm[n++] = token;
	}
	if (n == 0 || !hdr289lut) {
		kfree(buf_orig);
		kfree(hdr289lut);
		return -EINVAL;
	}
	memset(hdr289lut, 0, 289 * sizeof(unsigned short));
	gamma_count = (strlen(parm[0]) + 2) / 3;
	if (gamma_count > 289)
		gamma_count = 289;

	for (i = 0; i < gamma_count; ++i) {
		gamma[0] = parm[0][3 * i + 0];
		gamma[1] = parm[0][3 * i + 1];
		gamma[2] = parm[0][3 * i + 2];
		gamma[3] = '\0';
		if (kstrtol(gamma, 16, &val) < 0) {
			kfree(buf_orig);
			kfree(hdr289lut);
			return -EINVAL;
		}
		hdr289lut[i] = val;
	}

	for (i = 0; i < gamma_count; i++)
		lut_289_mapping[i] = hdr289lut[i];

	kfree(buf_orig);
	kfree(hdr289lut);
	return count;
}

static ssize_t amvecm_set_post_matrix_show(struct class *cla,
					   struct class_attribute *attr,
					   char *buf)
{
	int val;

	pr_info("Usage:\n");
	pr_info("echo port > /sys/class/amvecm/matrix_set\n");
	pr_info("1 : vadj1 input\n");
	pr_info("2 : vadj2 input\n");
	pr_info("4 : osd2 input\n");
	pr_info("8 : postblend input\n");
	pr_info("16 : osd1 input\n");
	pr_info("33 : vadj1 output\n");
	pr_info("34 : vadj2 output\n");
	pr_info("36 : osd2 output\n");
	pr_info("40 : postblend output\n");
	pr_info("48: osd1 output\n");
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_T7) &&
	    !is_meson_s4d_cpu() && !is_meson_s4_cpu()) {
		val = READ_VPP_REG(VPP_PROBE_CTRL);
		pr_info("current setting: %d\n", val & 0x3f);
	} else {
		val = READ_VPP_REG(VPP_MATRIX_CTRL);
		pr_info("current setting: %d\n", (val >> 10) & 0x3f);
	}

	return 0;
}

static ssize_t amvecm_set_post_matrix_store(struct class *cla,
					    struct class_attribute *attr,
					    const char *buf, size_t count)
{
	int val, reg_val;

	if (kstrtoint(buf, 10, &val) < 0)
		return -EINVAL;
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_T7) &&
	    !is_meson_s4d_cpu() && !is_meson_s4_cpu()) {
		reg_val = READ_VPP_REG(VPP_PROBE_CTRL);
		reg_val = reg_val & 0xffffffc0;
		/*reg_val |= 0x10000;*/
		/* enable probe hit */
		reg_val = reg_val | (val & 0x3f);
		if (val & (1 << 5))
			reg_val |= 1 << 15;
		else
			reg_val &= ~(1 << 15);

		WRITE_VPP_REG(VPP_PROBE_CTRL, reg_val);
		WRITE_VPP_REG(VPP_HI_COLOR, 0x80000000);

		pr_info("VPP_PROBE_CTRL is set\n");

	} else {
		reg_val = READ_VPP_REG(VPP_MATRIX_CTRL);
		reg_val = reg_val & 0xffff03ff;
		reg_val = reg_val | ((val & 0x3f) << 10);

		WRITE_VPP_REG(VPP_MATRIX_CTRL, reg_val);

		pr_info("VPP_MATRIX_CTRL is set\n");
	}
	return count;
}

static ssize_t amvecm_post_matrix_pos_show(struct class *cla,
					   struct class_attribute *attr,
					   char *buf)
{
	int val;

	pr_info("Usage:\n");
	pr_info("echo x y > /sys/class/amvecm/matrix_pos\n");

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_T7) &&
	    !is_meson_s4d_cpu() && !is_meson_s4_cpu())
		val = READ_VPP_REG(VPP_PROBE_POS);
	else
		val = READ_VPP_REG(VPP_MATRIX_PROBE_POS);
	pr_info("current position: %d %d\n",
		(val >> 16) & 0x1fff,
			(val >> 0) & 0x1fff);
	return 0;
}

static ssize_t amvecm_post_matrix_pos_store(struct class *cla,
					    struct class_attribute *attr,
					    const char *buf, size_t count)
{
	int val_x, val_y, reg_val;
	char *buf_orig, *parm[2] = {NULL};

	if (!buf)
		return count;
	buf_orig = kstrdup(buf, GFP_KERNEL);
	parse_param_amvecm(buf_orig, (char **)&parm);

	if (kstrtoint(parm[0], 10, &val_x) < 0) {
		kfree(buf_orig);
		return -EINVAL;
	}
	if (kstrtoint(parm[1], 10, &val_y) < 0) {
		kfree(buf_orig);
		return -EINVAL;
	}

	val_x = val_x & 0x1fff;
	val_y = val_y & 0x1fff;

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_T7) &&
	    !is_meson_s4d_cpu() && !is_meson_s4_cpu())
		reg_val = READ_VPP_REG(VPP_PROBE_POS);
	else
		reg_val = READ_VPP_REG(VPP_MATRIX_PROBE_POS);
	reg_val = reg_val & 0xe000e000;
	reg_val = reg_val | (val_x << 16) | val_y;

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_T7) &&
	    !is_meson_s4d_cpu() && !is_meson_s4_cpu())
		WRITE_VPP_REG(VPP_PROBE_POS, reg_val);
	else
		WRITE_VPP_REG(VPP_MATRIX_PROBE_POS, reg_val);

	kfree(buf_orig);
	return count;
}

static ssize_t amvecm_post_matrix_data_show(struct class *cla,
					    struct class_attribute *attr,
					    char *buf)
{
	int len = 0, val1 = 0, val2 = 0;
	u8 bit_depth = 12;
	u32 probe_color, probe_color1;


	if (cpu_after_eq(MESON_CPU_MAJOR_ID_T7) &&
		!is_meson_s4d_cpu() && !is_meson_s4_cpu()) {
		probe_color = VPP_PROBE_COLOR;
		probe_color1 = VPP_PROBE_COLOR1;
	} else {
		probe_color = VPP_MATRIX_PROBE_COLOR;
		probe_color1 = VPP_MATRIX_PROBE_COLOR1;
	}

	if (is_meson_tl1_cpu() ||
	    get_cpu_type() == MESON_CPU_MAJOR_ID_T5 ||
	    get_cpu_type() == MESON_CPU_MAJOR_ID_T5D ||
	    is_meson_s4_cpu() ||
	    get_cpu_type() == MESON_CPU_MAJOR_ID_T3 ||
	    get_cpu_type() == MESON_CPU_MAJOR_ID_T5W)
		bit_depth = 10;

	if (bit_depth == 10) {
		val1 = READ_VPP_REG(probe_color);
		len += sprintf(buf + len,
		"VPP_MATRIX_PROBE_COLOR %d, %d, %d\n",
		(val1 >> 20) & 0x3ff,
		(val1 >> 10) & 0x3ff,
		(val1 >> 0) & 0x3ff);
	} else {
		val1 = READ_VPP_REG(probe_color);
		val2 = READ_VPP_REG(probe_color1);
		len += sprintf(buf + len,
		"VPP_MATRIX_PROBE_COLOR %d, %d, %d\n",
		((val2 & 0xf) << 8) | ((val1 >> 24) & 0xff),
		(val1 >> 12) & 0xfff, val1 & 0xfff);
	}
	return len;
}

static ssize_t amvecm_post_matrix_data_store(struct class *cla,
					     struct class_attribute *attr,
					     const char *buf, size_t count)
{
	return 0;
}

static ssize_t amvecm_sr1_reg_show(struct class *cla,
				   struct class_attribute *attr,
				   char *buf)
{
	unsigned int addr;

	addr = ((sr1_index + 0x3280) << 2) | 0xd0100000;
	return sprintf(buf, "0x%x = 0x%x\n",
			addr, sr1_ret_val[sr1_index]);
}

static ssize_t amvecm_sr1_reg_store(struct class *cla,
				    struct class_attribute *attr,
				    const char *buf, size_t count)
{
	size_t r;
	unsigned int addr, off_addr = 0;

	r = sscanf(buf, "0x%x", &addr);
	addr = (addr & 0xffff) >> 2;
	if (r != 1 || addr > 0x32e4 || addr < 0x3280)
		return -EINVAL;
	off_addr = addr - 0x3280;
	sr1_index = off_addr;
	sr1_ret_val[off_addr] = sr1_reg_val[off_addr];

	return count;
}

static ssize_t amvecm_write_sr1_reg_val_show(struct class *cla,
					     struct class_attribute *attr,
					     char *buf)
{
	return 0;
}

static ssize_t amvecm_write_sr1_reg_val_store(struct class *cla,
					      struct class_attribute *attr,
					      const char *buf, size_t count)
{
	size_t r;
	unsigned int val;

	r = sscanf(buf, "0x%x", &val);
	if (r != 1)
		return -EINVAL;
	sr1_reg_val[sr1_index] = val;

	return count;
}

static ssize_t amvecm_dump_reg_show(struct class *cla,
				    struct class_attribute *attr,
				    char *buf)
{
	unsigned int addr;
	unsigned int value;
	unsigned int base_reg;

	base_reg = codecio_reg_start[CODECIO_VCBUS_BASE];

	pr_info("----dump sharpness0 reg----\n");
	for (addr = 0x3200;
		addr <= 0x3264; addr++)
		pr_info("[0x%x]vcbus[0x%04x]=0x%08x\n",
			(base_reg + (addr << 2)), addr,
				READ_VPP_REG_EX(addr, 0));
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
	if (is_meson_txl_cpu() || is_meson_txlx_cpu()) {
		for (addr = 0x3265;
			addr <= 0x3272; addr++)
			pr_info("[0x%x]vcbus[0x%04x]=0x%08x\n",
				(base_reg + (addr << 2)), addr,
					READ_VPP_REG_EX(addr, 0));
	}
	if (is_meson_txlx_cpu()) {
		for (addr = 0x3273;
			addr <= 0x327f; addr++)
			pr_info("[0x%x]vcbus[0x%04x]=0x%08x\n",
				(base_reg + (addr << 2)), addr,
					READ_VPP_REG_EX(addr, 0));
	}
#endif
	pr_info("----dump sharpness1 reg----\n");
	for (addr = (0x3200 + 0x80);
		addr <= (0x3264 + 0x80); addr++)
		pr_info("[0x%x]vcbus[0x%04x]=0x%08x\n",
			(base_reg + (addr << 2)), addr,
				READ_VPP_REG_EX(addr, 0));
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
	if (is_meson_txl_cpu() || is_meson_txlx_cpu()) {
		for (addr = (0x3265 + 0x80);
			addr <= (0x3272 + 0x80); addr++)
			pr_info("[0x%x]vcbus[0x%04x]=0x%08x\n",
				(base_reg + (addr << 2)), addr,
					READ_VPP_REG_EX(addr, 0));
	}
	if (is_meson_txlx_cpu()) {
		for (addr = (0x3273 + 0x80);
			addr <= (0x327f + 0x80); addr++)
			pr_info("[0x%x]vcbus[0x%04x]=0x%08x\n",
				(base_reg + (addr << 2)), addr,
					READ_VPP_REG_EX(addr, 0));
	}
#endif
	pr_info("----dump cm reg----\n");
	for (addr = 0x200; addr <= 0x21e; addr++) {
		WRITE_VPP_REG_EX(VPP_CHROMA_ADDR_PORT, addr, 0);
		value = READ_VPP_REG_EX(VPP_CHROMA_DATA_PORT, 0);
		pr_info("[0x%x]vcbus[0x%04x]=0x%08x\n",
			addr, addr,
				value);
	}
	for (addr = 0x100; addr <= 0x1fc; addr++) {
		WRITE_VPP_REG_EX(VPP_CHROMA_ADDR_PORT, addr, 0);
		value = READ_VPP_REG_EX(VPP_CHROMA_DATA_PORT, 0);
		pr_info("[0x%x]vcbus[0x%04x]=0x%08x\n",
			addr, addr,
				value);
	}

	pr_info("----dump vd1 IF0 reg----\n");
	for (addr = (0x1a50);
		addr <= (0x1a69); addr++)
		pr_info("[0x%x]vcbus[0x%04x]=0x%08x\n",
			(base_reg + (addr << 2)), addr,
				READ_VPP_REG_EX(addr, 0));
	pr_info("----dump vpp1 part1 reg----\n");
	for (addr = (0x1d00);
		addr <= (0x1d6e); addr++)
		pr_info("[0x%x]vcbus[0x%04x]=0x%08x\n",
			(base_reg + (addr << 2)), addr,
				READ_VPP_REG_EX(addr, 0));

	pr_info("----dump vpp1 part2 reg----\n");
	for (addr = (0x1d72);
		addr <= (0x1de4); addr++)
		pr_info("[0x%x]vcbus[0x%04x]=0x%08x\n",
			(base_reg + (addr << 2)), addr,
				READ_VPP_REG_EX(addr, 0));

	pr_info("----dump ndr reg----\n");
	for (addr = (0x2d00);
		addr <= (0x2d78); addr++)
		pr_info("[0x%x]vcbus[0x%04x]=0x%08x\n",
			(base_reg + (addr << 2)), addr,
				READ_VPP_REG_EX(addr, 0));
	pr_info("----dump nr3 reg----\n");
	for (addr = (0x2ff0);
		addr <= (0x2ff6); addr++)
		pr_info("[0x%x]vcbus[0x%04x]=0x%08x\n",
			(base_reg + (addr << 2)), addr,
				READ_VPP_REG_EX(addr, 0));
	pr_info("----dump vlock reg----\n");
	for (addr = (0x3000);
		addr <= (0x3020); addr++)
		pr_info("[0x%x]vcbus[0x%04x]=0x%08x\n",
			(base_reg + (addr << 2)), addr,
				READ_VPP_REG_EX(addr, 0));
	pr_info("----dump super scaler0 reg----\n");
	for (addr = (0x3100);
		addr <= (0x3115); addr++)
		pr_info("[0x%x]vcbus[0x%04x]=0x%08x\n",
			(base_reg + (addr << 2)), addr,
				READ_VPP_REG_EX(addr, 0));
	pr_info("----dump super scaler1 reg----\n");
	for (addr = (0x3118);
		addr <= (0x312e); addr++)
		pr_info("[0x%x]vcbus[0x%04x]=0x%08x\n",
			(base_reg + (addr << 2)), addr,
				READ_VPP_REG_EX(addr, 0));
	pr_info("----dump xvycc reg----\n");
	for (addr = (0x3158);
		addr <= (0x3179); addr++)
		pr_info("[0x%x]vcbus[0x%04x]=0x%08x\n",
			(base_reg + (addr << 2)), addr,
				READ_VPP_REG_EX(addr, 0));
	pr_info("----dump reg done----\n");
	return 0;
}

static ssize_t amvecm_dump_reg_store(struct class *cla,
				     struct class_attribute *attr,
				     const char *buf, size_t count)
{
	return 0;
}

static ssize_t amvecm_dump_vpp_hist_show(struct class *cla,
					 struct class_attribute *attr,
					 char *buf)
{
	vpp_dump_histgram();
	return 0;
}

static ssize_t amvecm_dump_vpp_hist_store(struct class *cla,
					  struct class_attribute *attr,
					   const char *buf, size_t count)
{
	return 0;
}

static ssize_t amvecm_hdr_dbg_show(struct class *cla,
				   struct class_attribute *attr,
				   char *buf)
{
	int ret;

	ret = amvecm_hdr_dbg(0);

	return 0;
}

static ssize_t amvecm_hdr_dbg_store(struct class *cla,
				    struct class_attribute *attr,
				    const char *buf, size_t count)
{
	long val = 0;
	char *buf_orig, *parm[5] = {NULL};
	int i;
	int curve_val[65] = {0};
	char *stemp = NULL;

	if (!buf)
		return count;

	stemp = kzalloc(400, GFP_KERNEL);
	if (!stemp)
		return 0;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig) {
		kfree(stemp);
		return -ENOMEM;
	}

	parse_param_amvecm(buf_orig, (char **)&parm);

	if (!strncmp(parm[0], "hdr_dbg", 7)) {
		if (kstrtoul(parm[1], 16, &val) < 0)
			goto free_buf;
		debug_hdr = val;
		pr_info("debug_hdr=0x%x\n", debug_hdr);
	} else if (!strncmp(parm[0], "hdr10_pr", 8)) {
		if (kstrtoul(parm[1], 16, &val) < 0)
			goto free_buf;
		hdr10_pr = val;
		pr_info("hdr10_pr=0x%x\n", hdr10_pr);
	} else if (!strncmp(parm[0], "clip_disable", 12)) {
		if (kstrtoul(parm[1], 16, &val) < 0)
			goto free_buf;
		hdr10_clip_disable = val;
		pr_info("hdr10_clip_disable=0x%x\n",
			hdr10_clip_disable);
	} else if (!strncmp(parm[0], "clip_luma", 9)) {
		if (kstrtoul(parm[1], 16, &val) < 0)
			goto free_buf;
		hdr10_clip_luma = val;
		pr_info("clip_luma=0x%x\n", hdr10_clip_luma);
	} else if (!strncmp(parm[0], "clip_margin", 11)) {
		if (kstrtoul(parm[1], 16, &val) < 0)
			goto free_buf;
		hdr10_clip_margin = val;
		pr_info("hdr10_clip_margin=0x%x\n", hdr10_clip_margin);
	} else if (!strncmp(parm[0], "hdr_sdr_ootf", 12)) {
		for (i = 0; i < HDR2_OOTF_LUT_SIZE; i++) {
			pr_info("%d ", oo_y_lut_hdr_sdr_def[i]);
			if ((i + 1) % 10 == 0)
				pr_info("\n");
		}
		pr_info("\n");
	} else if (!strncmp(parm[0], "cgain_lut", 9)) {
		if (!strncmp(parm[1], "rv", 2)) {
			for (i = 0; i < 65; i++)
				d_convert_str(cgain_lut_bypass[i],
					      i, stemp, 4, 10);
			pr_info("%s\n", stemp);
		} else if (!strncmp(parm[1], "wv", 2)) {
			str_sapr_to_d(parm[2], curve_val, 5);
			for (i = 0; i < 65; i++)
				cgain_lut_bypass[i] = curve_val[i];
		}
	} else if (!strcmp(parm[0], "cuva_dbg")) {
		cuva_hdr_dbg();
	} else if (!strcmp(parm[0], "reg_dump")) {
		if (!parm[1]) {
			hdr_reg_dump(0);
		} else {
			if (kstrtoul(parm[1], 16, &val) < 0)
				goto free_buf;
			hdr_reg_dump(val);
		}
	} else if (!strcmp(parm[0], "s5_reg_dump")) {
		if (!parm[1]) {
			s5_hdr_reg_dump(0);
		} else {
			if (kstrtoul(parm[1], 16, &val) < 0)
				goto free_buf;
			s5_hdr_reg_dump(val);
		}
	} else if (!strcmp(parm[0], "sbtm")) {
		sbtm_hdr10_tmo_dbg(parm);
		sbtm_sbtmdb_reg_dbg(parm);
	}

	hdr10_tmo_dbg(parm);
	ai_color_debug_store(parm);

free_buf:
	kfree(stemp);
	kfree(buf_orig);
	return count;
}

static ssize_t amvecm_hdr_reg_show(struct class *cla,
				   struct class_attribute *attr,
				   char *buf)
{
	int ret;

	ret = amvecm_hdr_dbg(1);

	return 0;
}

static ssize_t amvecm_hdr_reg_store(struct class *cla,
				    struct class_attribute *attr,
				    const char *buf, size_t count)
{
	return 0;
}

static ssize_t amvecm_hdr_tmo_show(struct class *cla,
			struct class_attribute *attr, char *buf)
{
	int len = 0;

	hdr10_tmo_parm_show();
	len = hdr_tmo_adb_show(buf);
	return len;

}

static ssize_t amvecm_hdr_tmo_store(struct class *cla,
			struct class_attribute *attr,
			const char *buf, size_t count)
{
	return 0;
}

static ssize_t amvecm_pc_mode_show(struct class *cla,
				   struct class_attribute *attr,
				   char *buf)
{
	pr_info("pc:echo 0x0 > /sys/class/amvecm/pc_mode\n");
	pr_info("other:echo 0x1 > /sys/class/amvecm/pc_mode\n");
	pr_info("pc_mode:%d,pc_mode_last:%d\n", pc_mode, pc_mode_last);
	return 0;
}

static ssize_t amvecm_pc_mode_store(struct class *cla,
				    struct class_attribute *attr,
				    const char *buf, size_t count)
{
	size_t r;
	int val;

	r = sscanf(buf, "%x\n", &val);
	if (r != 1)
		return -EINVAL;

	if (!pq_load_en)
		return count;

	if (val == 1) {
		pc_mode = 1;
		pc_mode_last = 0xff;
	} else if (val == 0) {
		pc_mode = 0;
		pc_mode_last = 0xff;
	}

	return count;
}

static ssize_t amvecm_color_tune_show(struct class *cla,
				    struct class_attribute *attr, char *buf)
{
	return 0;
}

static ssize_t amvecm_color_tune_store(struct class *cla,
				     struct class_attribute *attr,
				     const char *buf, size_t count)
{
	int ret;
	char *buf_orig, *parm[8] = {NULL};

	if (!buf)
		return count;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	parse_param_amvecm(buf_orig, (char **)&parm);

	ret = ct_dbg(parm);

	if (ret < 0)
		pr_info("set parameters failed\n");

	kfree(buf_orig);
	return count;
}

static ssize_t amvecm_dma_buf_show(struct class *cla,
				    struct class_attribute *attr, char *buf)
{
	read_dma_buf();
	return 0;
}

static ssize_t amvecm_dma_buf_store(struct class *cla,
				     struct class_attribute *attr,
				     const char *buf, size_t count)
{
	char *buf_orig, *parm[8] = {NULL};
	long tbl_id;
	long value;
	long table_offset = 0;

	if (!buf)
		return count;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	parse_param_amvecm(buf_orig, (char **)&parm);

	if (kstrtoul(parm[0], 10, &tbl_id) < 0)
		return -EINVAL;
	if (kstrtoul(parm[1], 16, &value) < 0)
		return -EINVAL;
	if (parm[2]) {
		if (kstrtoul(parm[2], 16, &table_offset) < 0)
			return -EINVAL;
	}

	write_dma_buf((u32)table_offset, (u32)tbl_id, (u32)value);

	kfree(buf_orig);
	return count;
}

static ssize_t amvecm_ble_whe_dbg_show(struct class *cla,
	struct class_attribute *attr, char *buf)
{
	ssize_t len = 0;

	if (dnlp_dbg_flag & BLK_ADJ_EN) {
		dnlp_dbg_flag &= ~BLK_ADJ_EN;
		len += sprintf(buf + len, "blk_adj_en = %d\n",
			ble_whe_param_load.blk_adj_en);
	}

	if (dnlp_dbg_flag & BLK_END) {
		dnlp_dbg_flag &= ~BLK_END;
		len += sprintf(buf + len, "blk_end = %d\n",
			ble_whe_param_load.blk_end);
	}

	if (dnlp_dbg_flag & BLK_SLP) {
		dnlp_dbg_flag &= ~BLK_SLP;
		len += sprintf(buf + len, "blk_slp = %d\n",
			ble_whe_param_load.blk_slp);
	}

	if (dnlp_dbg_flag & BRT_ADJ_EN) {
		dnlp_dbg_flag &= ~BRT_ADJ_EN;
		len += sprintf(buf + len, "brt_adj_en = %d\n",
			ble_whe_param_load.brt_adj_en);
	}

	if (dnlp_dbg_flag & BRT_START) {
		dnlp_dbg_flag &= ~BRT_START;
		len += sprintf(buf + len, "brt_start = %d\n",
			ble_whe_param_load.brt_start);
	}

	if (dnlp_dbg_flag & BRT_SLP) {
		dnlp_dbg_flag &= ~BRT_SLP;
		len += sprintf(buf + len, "brt_slp = %d\n",
			ble_whe_param_load.brt_slp);
	}

	return len;
}

static ssize_t amvecm_ble_whe_dbg_store(struct class *cla,
	struct class_attribute *attr,
	const char *buf, size_t count)
{
	long val = 0;
	char *buf_orig, *parm[8] = {NULL};

	if (!buf)
		return count;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	parse_param_amvecm(buf_orig, (char **)&parm);

	if (!strcmp(parm[0], "blk_adj_en")) {
		if (!parm[1]) {
			dnlp_dbg_flag |= BLK_ADJ_EN;
			goto for_read;
		}
		if (kstrtoul(parm[1], 10, &val) < 0)
			return -EINVAL;
		ble_whe_param_load.blk_adj_en = (int)val;
		pr_info("blk_adj_en = %d\n", (int)val);
	} else if (!strcmp(parm[0], "blk_end")) {
		if (!parm[1]) {
			dnlp_dbg_flag |= BLK_END;
			goto for_read;
		}
		if (kstrtoul(parm[1], 10, &val) < 0)
			return -EINVAL;
		ble_whe_param_load.blk_end = (int)val;
		pr_info("blk_end = %d\n", (int)val);
	} else if (!strcmp(parm[0], "blk_slp")) {
		if (!parm[1]) {
			dnlp_dbg_flag |= BLK_SLP;
			goto for_read;
		}
		if (kstrtoul(parm[1], 10, &val) < 0)
			return -EINVAL;
		ble_whe_param_load.blk_slp = (int)val;
		pr_info("blk_slp = %d\n", (int)val);
	} else if (!strcmp(parm[0], "brt_adj_en")) {
		if (!parm[1]) {
			dnlp_dbg_flag |= BRT_ADJ_EN;
			goto for_read;
		}
		if (kstrtoul(parm[1], 10, &val) < 0)
			return -EINVAL;
		ble_whe_param_load.brt_adj_en = (int)val;
		pr_info("brt_adj_en = %d\n", (int)val);
	} else if (!strcmp(parm[0], "brt_start")) {
		if (!parm[1]) {
			dnlp_dbg_flag |= BRT_START;
			goto for_read;
		}
		if (kstrtoul(parm[1], 10, &val) < 0)
			return -EINVAL;
		ble_whe_param_load.brt_start = (int)val;
		pr_info("brt_start = %d\n", (int)val);
	} else if (!strcmp(parm[0], "brt_slp")) {
		if (!parm[1]) {
			dnlp_dbg_flag |= BRT_SLP;
			goto for_read;
		}
		if (kstrtoul(parm[1], 10, &val) < 0)
			return -EINVAL;
		ble_whe_param_load.brt_slp = (int)val;
		pr_info("brt_slp = %d\n", (int)val);
	} else if (!strcmp(parm[0], "read_param")) {
		pr_info("blk_adj_en = %d\n", ble_whe_param_load.blk_adj_en);
		pr_info("blk_end = %d\n", ble_whe_param_load.blk_end);
		pr_info("blk_slp = %d\n", ble_whe_param_load.blk_slp);
		pr_info("brt_adj_en = %d\n", ble_whe_param_load.brt_adj_en);
		pr_info("brt_start = %d\n", ble_whe_param_load.brt_start);
		pr_info("brt_slp = %d\n", ble_whe_param_load.brt_slp);
	}

	vecm_latch_flag2 |= BLE_WHE_UPDATE;
	return count;

for_read:
	return count;
}

void pc_mode_process(void)
{
	unsigned int reg_val, drtlpf_config;

	if (pc_mode == 1 && pc_mode != pc_mode_last) {
		/* open dnlp clock gate */
		lc_en = pq_cfg.lc_en;
		dnlp_en = pq_cfg.dnlp_en;
		if (dnlp_en)
			ve_enable_dnlp();
		else
			ve_disable_dnlp();
		cm_en = pq_cfg.cm_en;

		/* sharpness on */
		VSYNC_WRITE_VPP_REG_BITS(SRSHARP0_PK_NR_ENABLE,
					 pq_cfg.sharpness0_en, 1, 1);
		VSYNC_WRITE_VPP_REG_BITS(SRSHARP1_PK_NR_ENABLE,
					 pq_cfg.sharpness1_en, 1, 1);
		reg_val = VSYNC_READ_VPP_REG(SRSHARP0_HCTI_FLT_CLP_DC);
		VSYNC_WRITE_VPP_REG(SRSHARP0_HCTI_FLT_CLP_DC,
				    reg_val | (pq_cfg.sharpness0_en << 28));
		VSYNC_WRITE_VPP_REG(SRSHARP1_HCTI_FLT_CLP_DC,
				    reg_val | (pq_cfg.sharpness1_en << 28));

		reg_val = VSYNC_READ_VPP_REG(SRSHARP0_HLTI_FLT_CLP_DC);
		VSYNC_WRITE_VPP_REG(SRSHARP0_HLTI_FLT_CLP_DC,
				    reg_val | (pq_cfg.sharpness0_en << 28));
		VSYNC_WRITE_VPP_REG(SRSHARP1_HLTI_FLT_CLP_DC,
				    reg_val | (pq_cfg.sharpness1_en << 28));

		reg_val = VSYNC_READ_VPP_REG(SRSHARP0_VLTI_FLT_CON_CLP);
		VSYNC_WRITE_VPP_REG(SRSHARP0_VLTI_FLT_CON_CLP,
				    reg_val | (pq_cfg.sharpness0_en << 14));
		VSYNC_WRITE_VPP_REG(SRSHARP1_VLTI_FLT_CON_CLP,
				    reg_val | (pq_cfg.sharpness1_en << 14));

		reg_val = VSYNC_READ_VPP_REG(SRSHARP0_VCTI_FLT_CON_CLP);
		VSYNC_WRITE_VPP_REG(SRSHARP0_VCTI_FLT_CON_CLP,
				    reg_val | (pq_cfg.sharpness0_en << 14));
		VSYNC_WRITE_VPP_REG(SRSHARP1_VCTI_FLT_CON_CLP,
				    reg_val | (pq_cfg.sharpness1_en << 14));

		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXL)) {
			VSYNC_WRITE_VPP_REG_BITS(SRSHARP0_DEJ_CTRL,
						 pq_cfg.sharpness0_en, 0, 1);
			drtlpf_config = pq_cfg.sharpness0_en ? 0x7 : 0x0;
			VSYNC_WRITE_VPP_REG_BITS(SRSHARP0_SR3_DRTLPF_EN,
						 drtlpf_config, 0, 3);
			VSYNC_WRITE_VPP_REG_BITS(SRSHARP0_SR3_DERING_CTRL,
						 pq_cfg.sharpness0_en, 28, 3);

			VSYNC_WRITE_VPP_REG_BITS(SRSHARP1_DEJ_CTRL,
						 pq_cfg.sharpness1_en, 0, 1);
			drtlpf_config = pq_cfg.sharpness1_en ? 0x7 : 0x0;
			VSYNC_WRITE_VPP_REG_BITS(SRSHARP1_SR3_DRTLPF_EN,
						 drtlpf_config, 0, 3);
			VSYNC_WRITE_VPP_REG_BITS(SRSHARP1_SR3_DERING_CTRL,
						 pq_cfg.sharpness1_en, 28, 3);
		}

		pc_mode_last = pc_mode;
	} else if ((pc_mode == 0) && (pc_mode != pc_mode_last)) {
		dnlp_en = 0;
		lc_en = 0;
		ve_disable_dnlp();
		cm_en = 0;

		VSYNC_WRITE_VPP_REG_BITS(SRSHARP0_PK_NR_ENABLE,
					 0, 1, 1);
		VSYNC_WRITE_VPP_REG_BITS(SRSHARP1_PK_NR_ENABLE,
					 0, 1, 1);
		reg_val = VSYNC_READ_VPP_REG(SRSHARP0_HCTI_FLT_CLP_DC);
		VSYNC_WRITE_VPP_REG(SRSHARP0_HCTI_FLT_CLP_DC,
				    reg_val & 0xefffffff);
		VSYNC_WRITE_VPP_REG(SRSHARP1_HCTI_FLT_CLP_DC,
				    reg_val & 0xefffffff);

		reg_val = VSYNC_READ_VPP_REG(SRSHARP0_HLTI_FLT_CLP_DC);
		VSYNC_WRITE_VPP_REG(SRSHARP0_HLTI_FLT_CLP_DC,
				    reg_val & 0xefffffff);
		VSYNC_WRITE_VPP_REG(SRSHARP1_HLTI_FLT_CLP_DC,
				    reg_val & 0xefffffff);

		reg_val = VSYNC_READ_VPP_REG(SRSHARP0_VLTI_FLT_CON_CLP);
		VSYNC_WRITE_VPP_REG(SRSHARP0_VLTI_FLT_CON_CLP,
				    reg_val & 0xffffbfff);
		VSYNC_WRITE_VPP_REG(SRSHARP1_VLTI_FLT_CON_CLP,
				    reg_val & 0xffffbfff);

		reg_val = VSYNC_READ_VPP_REG(SRSHARP0_VCTI_FLT_CON_CLP);
		VSYNC_WRITE_VPP_REG(SRSHARP0_VCTI_FLT_CON_CLP,
				    reg_val & 0xffffbfff);
		VSYNC_WRITE_VPP_REG(SRSHARP1_VCTI_FLT_CON_CLP,
				    reg_val & 0xffffbfff);

		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXL)) {
			VSYNC_WRITE_VPP_REG_BITS(SRSHARP0_DEJ_CTRL,
						 0, 0, 1);
			VSYNC_WRITE_VPP_REG_BITS(SRSHARP0_SR3_DRTLPF_EN,
						 0, 0, 3);
			VSYNC_WRITE_VPP_REG_BITS(SRSHARP0_SR3_DERING_CTRL,
						 0, 28, 3);

			VSYNC_WRITE_VPP_REG_BITS(SRSHARP1_DEJ_CTRL,
						 0, 0, 1);
			VSYNC_WRITE_VPP_REG_BITS(SRSHARP1_SR3_DRTLPF_EN,
						 0, 0, 3);
			VSYNC_WRITE_VPP_REG_BITS(SRSHARP1_SR3_DERING_CTRL,
						 0, 28, 3);
		}

		pc_mode_last = pc_mode;
	}
}

void amvecm_black_ext_enable(unsigned int enable)
{
	if (enable)
		WRITE_VPP_REG_BITS(VPP_VE_ENABLE_CTRL, 1, 3, 1);
	else
		WRITE_VPP_REG_BITS(VPP_VE_ENABLE_CTRL, 0, 3, 1);
}

void amvecm_black_ext_start_adj(unsigned int value)
{
	if (value > 255)
		return;
	WRITE_VPP_REG_BITS(VPP_BLACKEXT_CTRL, value, 24, 8);
}

void amvecm_black_ext_slope_adj(unsigned int value)
{
	if (value > 255)
		return;
	WRITE_VPP_REG_BITS(VPP_BLACKEXT_CTRL, value, 16, 8);
}

void amvecm_sr0_pk_enable(unsigned int enable)
{
	if (enable)
		WRITE_VPP_REG_BITS(SRSHARP0_PK_NR_ENABLE,
				   1, 1, 1);
	else
		WRITE_VPP_REG_BITS(SRSHARP0_PK_NR_ENABLE,
				   0, 1, 1);
}

void amvecm_sr1_pk_enable(unsigned int enable)
{
	if (enable)
		WRITE_VPP_REG_BITS(SRSHARP1_PK_NR_ENABLE,
				   1, 1, 1);
	else
		WRITE_VPP_REG_BITS(SRSHARP1_PK_NR_ENABLE,
				   0, 1, 1);
}

void amvecm_sr0_dering_enable(unsigned int enable)
{
	if (enable)
		WRITE_VPP_REG_BITS(SRSHARP0_SR3_DERING_CTRL,
				   1, 28, 3);
	else
		WRITE_VPP_REG_BITS(SRSHARP0_SR3_DERING_CTRL,
				   0, 28, 3);
}

void amvecm_sr1_dering_enable(unsigned int enable)
{
	if (enable)
		WRITE_VPP_REG_BITS(SRSHARP1_SR3_DERING_CTRL,
				   1, 28, 3);
	else
		WRITE_VPP_REG_BITS(SRSHARP1_SR3_DERING_CTRL,
				   0, 28, 3);
}

void amvecm_sr0_dejaggy_enable(unsigned int enable)
{
	if (enable)
		WRITE_VPP_REG_BITS(SRSHARP0_DEJ_CTRL,
				   1, 0, 1);
	else
		WRITE_VPP_REG_BITS(SRSHARP0_DEJ_CTRL,
				   0, 0, 1);
}

void amvecm_sr1_dejaggy_enable(unsigned int enable)
{
	if (enable)
		WRITE_VPP_REG_BITS(SRSHARP1_DEJ_CTRL,
				   1, 0, 1);
	else
		WRITE_VPP_REG_BITS(SRSHARP1_DEJ_CTRL,
				   0, 0, 1);
}

void amvecm_sr0_direction_enable(unsigned int enable)
{
	if (enable)
		WRITE_VPP_REG_BITS(SRSHARP0_SR3_DRTLPF_EN,
				   7, 0, 3);
	else
		WRITE_VPP_REG_BITS(SRSHARP0_SR3_DRTLPF_EN,
				   0, 0, 3);
}

void amvecm_sr1_direction_enable(unsigned int enable)
{
	if (enable)
		WRITE_VPP_REG_BITS(SRSHARP1_SR3_DRTLPF_EN,
				   7, 0, 3);
	else
		WRITE_VPP_REG_BITS(SRSHARP1_SR3_DRTLPF_EN,
				   0, 0, 3);
}

void pq_user_latch_process(void)
{
	int i = 0;
	int sat_hue_val = 0;
	int input_clr_md = 0;
	int cm_color_md_max = cm_14_ecm2colormode_max;
	struct cm_color_md cm_clr_md;

	if (pq_user_latch_flag & PQ_USER_BLK_EN) {
		pq_user_latch_flag &= ~PQ_USER_BLK_EN;
		amvecm_black_ext_enable(true);
	} else if (pq_user_latch_flag & PQ_USER_BLK_DIS) {
		pq_user_latch_flag &= ~PQ_USER_BLK_DIS;
		amvecm_black_ext_enable(false);
	} else if (pq_user_latch_flag & PQ_USER_BLK_START) {
		pq_user_latch_flag &= ~PQ_USER_BLK_START;
		amvecm_black_ext_start_adj(pq_user_value);
	} else if (pq_user_latch_flag & PQ_USER_BLK_SLOPE) {
		pq_user_latch_flag &= ~PQ_USER_BLK_SLOPE;
		amvecm_black_ext_slope_adj(pq_user_value);
	} else if (pq_user_latch_flag & PQ_USER_SR0_PK_EN) {
		pq_user_latch_flag &= ~PQ_USER_SR0_PK_EN;
		amvecm_sr0_pk_enable(true);
	} else if (pq_user_latch_flag & PQ_USER_SR0_PK_DIS) {
		pq_user_latch_flag &= ~PQ_USER_SR0_PK_DIS;
		amvecm_sr0_pk_enable(false);
	} else if (pq_user_latch_flag & PQ_USER_SR1_PK_EN) {
		pq_user_latch_flag &= ~PQ_USER_SR1_PK_EN;
		amvecm_sr1_pk_enable(true);
	} else if (pq_user_latch_flag & PQ_USER_SR1_PK_DIS) {
		pq_user_latch_flag &= ~PQ_USER_SR1_PK_DIS;
		amvecm_sr1_pk_enable(false);
	} else if (pq_user_latch_flag & PQ_USER_SR0_DERING_EN) {
		pq_user_latch_flag &= ~PQ_USER_SR0_DERING_EN;
		amvecm_sr0_dering_enable(true);
	} else if (pq_user_latch_flag & PQ_USER_SR0_DERING_DIS) {
		pq_user_latch_flag &= ~PQ_USER_SR0_DERING_DIS;
		amvecm_sr0_dering_enable(false);
	} else if (pq_user_latch_flag & PQ_USER_SR1_DERING_EN) {
		pq_user_latch_flag &= ~PQ_USER_SR1_DERING_EN;
		amvecm_sr1_dering_enable(true);
	} else if (pq_user_latch_flag & PQ_USER_SR1_DERING_DIS) {
		pq_user_latch_flag &= ~PQ_USER_SR1_DERING_DIS;
		amvecm_sr1_dering_enable(false);
	} else if (pq_user_latch_flag & PQ_USER_SR0_DEJAGGY_EN) {
		pq_user_latch_flag &= ~PQ_USER_SR0_DEJAGGY_EN;
		amvecm_sr0_dejaggy_enable(true);
	} else if (pq_user_latch_flag & PQ_USER_SR0_DEJAGGY_DIS) {
		pq_user_latch_flag &= ~PQ_USER_SR0_DEJAGGY_DIS;
		amvecm_sr0_dejaggy_enable(false);
	} else if (pq_user_latch_flag & PQ_USER_SR1_DEJAGGY_EN) {
		pq_user_latch_flag &= ~PQ_USER_SR1_DEJAGGY_EN;
		amvecm_sr1_dejaggy_enable(true);
	} else if (pq_user_latch_flag & PQ_USER_SR1_DEJAGGY_DIS) {
		pq_user_latch_flag &= ~PQ_USER_SR1_DEJAGGY_DIS;
		amvecm_sr1_dejaggy_enable(false);
	} else if (pq_user_latch_flag & PQ_USER_SR0_DIRECTION_EN) {
		pq_user_latch_flag &= ~PQ_USER_SR0_DIRECTION_EN;
		amvecm_sr0_direction_enable(true);
	} else if (pq_user_latch_flag & PQ_USER_SR0_DIRECTION_DIS) {
		pq_user_latch_flag &= ~PQ_USER_SR0_DIRECTION_DIS;
		amvecm_sr0_direction_enable(false);
	} else if (pq_user_latch_flag & PQ_USER_SR1_DIRECTION_EN) {
		pq_user_latch_flag &= ~PQ_USER_SR1_DIRECTION_EN;
		amvecm_sr1_direction_enable(true);
	} else if (pq_user_latch_flag & PQ_USER_SR1_DIRECTION_DIS) {
		pq_user_latch_flag &= ~PQ_USER_SR1_DIRECTION_DIS;
		amvecm_sr1_direction_enable(false);
	} else if (pq_user_latch_flag & PQ_USER_CMS_CURVE_SAT ||
		   pq_user_latch_flag & PQ_USER_CMS_CURVE_LUMA ||
		   pq_user_latch_flag & PQ_USER_CMS_CURVE_HUE_HS ||
		   pq_user_latch_flag & PQ_USER_CMS_CURVE_HUE) {
		if (cm_cur_work_color_md == cm_9_color) {
			cm_color_md_max = ecm2colormode_max;
			cm_clr_md.color_type = cm_9_color;
			cm_clr_md.cm_9_color_md = ecm2colormode_purple;
			cm_clr_md.cm_14_color_md = cm_14_ecm2colormode_max;
			cm_clr_md.color_value = 0;
		} else {
			cm_color_md_max = cm_14_ecm2colormode_max;
			cm_clr_md.color_type = cm_14_color;
			cm_clr_md.cm_9_color_md = ecm2colormode_max;
			cm_clr_md.cm_14_color_md =
				cm_14_ecm2colormode_blue_purple;
			cm_clr_md.color_value = 0;
		}

		if (cm2_debug) {
			pr_info("cm_color_md_max:%d\n",
				cm_color_md_max);
			pr_info("color_type:%d\n",
				cm_clr_md.color_type);
			pr_info("cm_9_color_md:%d\n",
				cm_clr_md.cm_9_color_md);
			pr_info("cm_14_color_md:%d\n",
				cm_clr_md.cm_14_color_md);
			pr_info("color_value:%d\n",
				cm_clr_md.color_value);
		}
		if (pq_user_latch_flag & PQ_USER_CMS_CURVE_SAT) {
			pq_user_latch_flag &= ~PQ_USER_CMS_CURVE_SAT;
			input_clr_md = 0;
			for (i = 0; i < cm_color_md_max; i++) {
				if (cm_cur_work_color_md == cm_9_color)
					cm_clr_md.cm_9_color_md = input_clr_md;
				else
					cm_clr_md.cm_14_color_md = input_clr_md;

				input_clr_md++;
				if (cm2_sat_array[i][2] == 1) {
					cm2_curve_update_sat(cm_clr_md);
					cm2_sat_array[i][2] = 0;
				}
			}
		}
		if (pq_user_latch_flag & PQ_USER_CMS_CURVE_LUMA) {
			pq_user_latch_flag &= ~PQ_USER_CMS_CURVE_LUMA;
			input_clr_md = 0;
			for (i = 0; i < cm_color_md_max; i++) {
				if (cm_cur_work_color_md == cm_9_color)
					cm_clr_md.cm_9_color_md = input_clr_md;
				else
					cm_clr_md.cm_14_color_md = input_clr_md;

				input_clr_md++;
				if (cm2_luma_array[i][2] == 1) {
					cm2_curve_update_luma(cm_clr_md);
					cm2_luma_array[i][2] = 0;
				}
			}
		}
		if (pq_user_latch_flag & PQ_USER_CMS_CURVE_HUE_HS) {
			pq_user_latch_flag &= ~PQ_USER_CMS_CURVE_HUE_HS;
			input_clr_md = 0;
			for (i = 0; i < cm_color_md_max; i++) {
				if (cm_cur_work_color_md == cm_9_color)
					cm_clr_md.cm_9_color_md = input_clr_md;
				else
					cm_clr_md.cm_14_color_md = input_clr_md;

				input_clr_md++;
				if (cm2_hue_by_hs_array[i][2] == 1) {
					cm2_curve_update_hue_by_hs(cm_clr_md);
					cm2_hue_by_hs_array[i][2] = 0;
				}
			}
		}
		if (pq_user_latch_flag & PQ_USER_CMS_CURVE_HUE) {
			pq_user_latch_flag &= ~PQ_USER_CMS_CURVE_HUE;
			input_clr_md = 0;
			for (i = 0; i < cm_color_md_max; i++) {
				if (cm_cur_work_color_md == cm_9_color)
					cm_clr_md.cm_9_color_md = input_clr_md;
				else
					cm_clr_md.cm_14_color_md = input_clr_md;

				input_clr_md++;
				if (cm2_hue_array[i][2] == 1) {
					cm2_curve_update_hue(cm_clr_md);
					cm2_hue_array[i][2] = 0;
				}
			}
		}
	} else if (pq_user_latch_flag & PQ_USER_CMS_SAT_HUE) {
		pq_user_latch_flag &= ~PQ_USER_CMS_SAT_HUE;
		sat_hue_val =
			aipq_saturation_hue_get_base_val() + sat_hue_offset_val;
		/* ai_pq switch on, */
		/* if saturation val(sat_hue_val  >> 16) > max val(511), */
		/*	saturation val can not add sat offset val. */
		if ((sat_hue_val  >> 16) > 511)
			sat_hue_val = aipq_saturation_hue_get_base_val();

		amvecm_set_saturation_hue(sat_hue_val);
	}

	if (pq_user_latch_flag & PQ_USER_PQ_MODULE_CTL) {
		pq_user_latch_flag &= ~PQ_USER_PQ_MODULE_CTL;
		vpp_pq_ctrl_config(pq_cfg, WR_DMA);
	}
}

static const char *amvecm_pq_user_usage_str = {
	"Usage:\n"
	"echo blk_ext_en > /sys/class/amvecm/pq_user_set: blk ext en\n"
	"echo blk_ext_dis > /sys/class/amvecm/pq_user_set: blk ext dis\n"
	"echo blk_start val > /sys/class/amvecm/pq_user_set: start adj\n"
	"echo blk_slope val > /sys/class/amvecm/pq_user_set: slope adj\n"
	"echo sr0_pk_en > /sys/class/amvecm/pq_user_set: sr0 pk en\n"
	"echo sr0_pk_dis > /sys/class/amvecm/pq_user_set: sr0 pk dis\n"
	"echo sr1_pk_en > /sys/class/amvecm/pq_user_set: sr0 pk en\n"
	"echo sr1_pk_dis > /sys/class/amvecm/pq_user_set: sr0 pk dis\n"
	"echo sr0_dering_en > /sys/class/amvecm/pq_user_set: sr0 dr en\n"
	"echo sr0_dering_dis > /sys/class/amvecm/pq_user_set: sr0 dr dis\n"
	"echo sr1_dering_en > /sys/class/amvecm/pq_user_set: sr1 dr en\n"
	"echo sr1_dering_dis > /sys/class/amvecm/pq_user_set: sr1 dr dis\n"
	"echo sr0_dejaggy_en > /sys/class/amvecm/pq_user_set: sr0 dj en\n"
	"echo sr0_dejaggy_dis > /sys/class/amvecm/pq_user_set: sr0 dj dis\n"
	"echo sr1_dejaggy_en > /sys/class/amvecm/pq_user_set: sr1 dj en\n"
	"echo sr1_dejaggy_dis > /sys/class/amvecm/pq_user_set: sr1 dj dis\n"
	"echo sr0_direc_en > /sys/class/amvecm/pq_user_set: sr0 direc en\n"
	"echo sr0_direc_dis > /sys/class/amvecm/pq_user_set: sr0 direc dis\n"
	"echo sr1_direc_en > /sys/class/amvecm/pq_user_set: sr1 direc en\n"
	"echo sr1_direc_dis > /sys/class/amvecm/pq_user_set: sr1 direc dis\n"

};

static ssize_t amvecm_pq_user_show(struct class *cla,
				   struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", amvecm_pq_user_usage_str);
}

static ssize_t amvecm_pq_user_store(struct class *cla,
				    struct class_attribute *attr,
				    const char *buf, size_t count)
{
	char *buf_orig, *parm[8] = {NULL};
	long val = 0;

	if (!buf)
		return count;
	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig)
		return -ENOMEM;
	parse_param_amvecm(buf_orig, (char **)&parm);

	if (!strncmp(parm[0], "blk_ext_en", 10)) {
		pq_user_latch_flag |= PQ_USER_BLK_EN;
	} else if (!strncmp(parm[0], "blk_ext_dis", 11)) {
		pq_user_latch_flag |= PQ_USER_BLK_DIS;
	} else if (!strncmp(parm[0], "blk_start", 9)) {
		if (kstrtoul(parm[1], 10, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		pq_user_value = val;
		pq_user_latch_flag |= PQ_USER_BLK_START;
	} else if (!strncmp(parm[0], "blk_slope", 9)) {
		if (kstrtoul(parm[1], 10, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		pq_user_value = val;
		pq_user_latch_flag |= PQ_USER_BLK_SLOPE;
	} else if (!strncmp(parm[0], "sr0_pk_en", 9)) {
		pq_user_latch_flag |= PQ_USER_SR0_PK_EN;
	} else if (!strncmp(parm[0], "sr0_pk_dis", 10)) {
		pq_user_latch_flag |= PQ_USER_SR0_PK_DIS;
	} else if (!strncmp(parm[0], "sr1_pk_en", 9)) {
		pq_user_latch_flag |= PQ_USER_SR1_PK_EN;
	} else if (!strncmp(parm[0], "sr1_pk_dis", 10)) {
		pq_user_latch_flag |= PQ_USER_SR1_PK_DIS;
	} else if (!strncmp(parm[0], "sr0_dering_en", 13)) {
		pq_user_latch_flag |= PQ_USER_SR0_DERING_EN;
	} else if (!strncmp(parm[0], "sr0_dering_dis", 14)) {
		pq_user_latch_flag |= PQ_USER_SR0_DERING_DIS;
	} else if (!strncmp(parm[0], "sr1_dering_en", 13)) {
		pq_user_latch_flag |= PQ_USER_SR1_DERING_EN;
	} else if (!strncmp(parm[0], "sr1_dering_dis", 14)) {
		pq_user_latch_flag |= PQ_USER_SR1_DERING_DIS;
	} else if (!strncmp(parm[0], "sr0_dejaggy_en", 14)) {
		pq_user_latch_flag |= PQ_USER_SR0_DEJAGGY_EN;
	} else if (!strncmp(parm[0], "sr0_dejaggy_dis", 15)) {
		pq_user_latch_flag |= PQ_USER_SR0_DEJAGGY_DIS;
	} else if (!strncmp(parm[0], "sr1_dejaggy_en", 14)) {
		pq_user_latch_flag |= PQ_USER_SR1_DEJAGGY_EN;
	} else if (!strncmp(parm[0], "sr1_dejaggy_dis", 15)) {
		pq_user_latch_flag |= PQ_USER_SR1_DEJAGGY_DIS;
	} else if (!strncmp(parm[0], "sr0_direc_en", 12)) {
		pq_user_latch_flag |= PQ_USER_SR0_DIRECTION_EN;
	} else if (!strncmp(parm[0], "sr0_direc_dis", 13)) {
		pq_user_latch_flag |= PQ_USER_SR0_DIRECTION_DIS;
	} else if (!strncmp(parm[0], "sr1_direc_en", 12)) {
		pq_user_latch_flag |= PQ_USER_SR1_DIRECTION_EN;
	} else if (!strncmp(parm[0], "sr1_direc_dis", 13)) {
		pq_user_latch_flag |= PQ_USER_SR1_DIRECTION_DIS;
	}

	kfree(buf_orig);
	return count;
}

static const char *dnlp_insmod_debug_usage_str = {
	"usage: echo 1 > /sys/class/amvecm/dnlp_insmod\n"
};

static ssize_t amvecm_dnlp_insmod_show(struct class *cla,
				       struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", dnlp_insmod_debug_usage_str);
}

static ssize_t amvecm_dnlp_insmod_store(struct class *cla,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	size_t r;
	int val;

	r = sscanf(buf, "%x\n", &val);
	if (r != 1)
		return -EINVAL;

	if (val == 1)
		dnlp_alg_param_init();

	return count;
}

static ssize_t amvecm_vpp_demo_show(struct class *cla,
				    struct class_attribute *attr,
				    char *buf)
{
	pr_info("echo sr_demo val(0/1) > /sys/class/amvecm/vpp_demo\n");
	pr_info("echo cm_demo val(0/1) > /sys/class/amvecm/vpp_demo\n");
	pr_info("echo lc_demo val(0/1) > /sys/class/amvecm/vpp_demo\n");
	return 0;
}

static ssize_t amvecm_vpp_demo_store(struct class *cla,
				     struct class_attribute *attr,
				     const char *buf, size_t count)
{
	char *buf_orig, *parm[8] = {NULL};
	long val = 0;
	struct demo_data_s *p = &demo_data;

	if (!buf)
		return count;
	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig)
		return -ENOMEM;
	parse_param_amvecm(buf_orig, (char **)&parm);

	if (!strncmp(parm[0], "sr_demo", 7)) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto kfree_buf;
		if (val)
			p->en_st[E_DEMO_SR] = E_ENABLE;
		else
			p->en_st[E_DEMO_SR] = E_DISABLE;
		p->update_flag[E_DEMO_SR] = 1;
		pr_amvecm_dbg("sr_demo = %d\n", p->en_st[E_DEMO_SR]);
	} else if (!strncmp(parm[0], "cm_demo", 7)) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto kfree_buf;
		if (val)
			p->en_st[E_DEMO_CM] = E_ENABLE;
		else
			p->en_st[E_DEMO_CM] = E_DISABLE;
		p->update_flag[E_DEMO_CM] = 1;
		pr_amvecm_dbg("cm_demo = %d\n", p->en_st[E_DEMO_CM]);
	} else if (!strncmp(parm[0], "lc_demo", 7)) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto kfree_buf;
		if (val)
			p->en_st[E_DEMO_LC] = E_ENABLE;
		else
			p->en_st[E_DEMO_LC] = E_DISABLE;
		p->update_flag[E_DEMO_LC] = 1;
		pr_amvecm_dbg("lc_demo = %d\n", p->en_st[E_DEMO_LC]);
	}

	kfree(buf_orig);
	return count;

kfree_buf:
	kfree(buf_orig);
	return -EINVAL;

}

static void dump_vpp_size_info(void)
{
	unsigned int vpp_input_h, vpp_input_v,
		pps_input_length, pps_input_height,
		pps_output_hs, pps_output_he, pps_output_vs, pps_output_ve,
		vd1_preblend_hs, vd1_preblend_he,
		vd1_preblend_vs, vd1_preblend_ve,
		vd2_preblend_hs, vd2_preblend_he,
		vd2_preblend_vs, vd2_preblend_ve,
		prelend_input_hsize,
		vd1_postblend_hs, vd1_postblend_he,
		vd1_postblend_vs, vd1_postblend_ve,
		postblend_hsize,
		ve_hsize, ve_vsize, psr_hsize, psr_vsize,
		cm_hsize, cm_vsize;
	vpp_input_h = READ_VPP_REG_BITS(VPP_IN_H_V_SIZE, 16, 13);
	vpp_input_v = READ_VPP_REG_BITS(VPP_IN_H_V_SIZE, 0, 13);
	pps_input_length = READ_VPP_REG_BITS(VPP_LINE_IN_LENGTH, 0, 13);
	pps_input_height = READ_VPP_REG_BITS(VPP_PIC_IN_HEIGHT, 0, 13);
	pps_output_hs = READ_VPP_REG_BITS(VPP_HSC_REGION12_STARTP, 16, 13);
	pps_output_he = READ_VPP_REG_BITS(VPP_HSC_REGION4_ENDP, 0, 13);
	pps_output_vs = READ_VPP_REG_BITS(VPP_VSC_REGION12_STARTP, 16, 13);
	pps_output_ve = READ_VPP_REG_BITS(VPP_VSC_REGION4_ENDP, 0, 13);
	vd1_preblend_he = READ_VPP_REG_BITS(VPP_PREBLEND_VD1_H_START_END,
					    0, 13);
	vd1_preblend_hs = READ_VPP_REG_BITS(VPP_PREBLEND_VD1_H_START_END,
					    16, 13);
	vd1_preblend_ve = READ_VPP_REG_BITS(VPP_PREBLEND_VD1_V_START_END,
					    0, 13);
	vd1_preblend_vs = READ_VPP_REG_BITS(VPP_PREBLEND_VD1_V_START_END,
					    16, 13);
	vd2_preblend_he = READ_VPP_REG_BITS(VPP_BLEND_VD2_H_START_END, 0, 13);
	vd2_preblend_hs = READ_VPP_REG_BITS(VPP_BLEND_VD2_H_START_END, 16, 13);
	vd2_preblend_ve = READ_VPP_REG_BITS(VPP_BLEND_VD2_V_START_END, 0, 13);
	vd2_preblend_vs = READ_VPP_REG_BITS(VPP_BLEND_VD2_V_START_END, 16, 13);
	prelend_input_hsize = READ_VPP_REG_BITS(VPP_PREBLEND_H_SIZE, 0, 13);
	vd1_postblend_he = READ_VPP_REG_BITS(VPP_POSTBLEND_VD1_H_START_END,
					     0, 13);
	vd1_postblend_hs = READ_VPP_REG_BITS(VPP_POSTBLEND_VD1_H_START_END,
					     16, 13);
	vd1_postblend_ve = READ_VPP_REG_BITS(VPP_POSTBLEND_VD1_V_START_END,
					     0, 13);
	vd1_postblend_vs = READ_VPP_REG_BITS(VPP_POSTBLEND_VD1_V_START_END,
					     16, 13);
	postblend_hsize = READ_VPP_REG_BITS(VPP_POSTBLEND_H_SIZE, 0, 13);
	ve_hsize = READ_VPP_REG_BITS(VPP_VE_H_V_SIZE, 16, 13);
	ve_vsize = READ_VPP_REG_BITS(VPP_VE_H_V_SIZE, 0, 13);
	psr_hsize = READ_VPP_REG_BITS(VPP_PSR_H_V_SIZE, 16, 13);
	psr_vsize = READ_VPP_REG_BITS(VPP_PSR_H_V_SIZE, 0, 13);
	WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT, 0x205);
	cm_hsize = READ_VPP_REG(VPP_CHROMA_DATA_PORT);
	cm_vsize = (cm_hsize >> 16) & 0xffff;
	cm_hsize = cm_hsize & 0xffff;
	pr_info("\n vpp size info:\n");
	pr_info("vpp_input_h:%d, vpp_input_v:%d\n"
		"pps_input_length:%d, pps_input_height:%d\n"
		"pps_output_hs:%d, pps_output_he:%d\n"
		"pps_output_vs:%d, pps_output_ve:%d\n"
		"vd1_preblend_hs:%d, vd1_preblend_he:%d\n"
		"vd1_preblend_vs:%d, vd1_preblend_ve:%d\n"
		"vd2_preblend_hs:%d, vd2_preblend_he:%d\n"
		"vd2_preblend_vs:%d, vd2_preblend_ve:%d\n"
		"prelend_input_hsize:%d\n"
		"vd1_postblend_hs:%d, vd1_postblend_he:%d\n"
		"vd1_postblend_vs:%d, vd1_postblend_ve:%d\n"
		"postblend_hsize:%d\n"
		"ve_hsize:%d, ve_vsize:%d\n"
		"psr_hsize:%d, psr_vsize:%d\n"
		"cm_hsize:%d, cm_vsize:%d\n",
		vpp_input_h, vpp_input_v,
		pps_input_length, pps_input_height,
		pps_output_hs, pps_output_he,
		pps_output_vs, pps_output_ve,
		vd1_preblend_hs, vd1_preblend_he,
		vd1_preblend_vs, vd1_preblend_ve,
		vd2_preblend_hs, vd2_preblend_he,
		vd2_preblend_vs, vd2_preblend_ve,
		prelend_input_hsize,
		vd1_postblend_hs, vd1_postblend_he,
		vd1_postblend_vs, vd1_postblend_ve,
		postblend_hsize,
		ve_hsize, ve_vsize,
		psr_hsize, psr_vsize,
		cm_hsize, cm_vsize);
}

void amvecm_sharpness_enable(int sel)
{
	/*0:peaking enable   1:peaking disable*/
	/*2:lti/cti enable   3:lti/cti disable*/
	switch (sel) {
	case 0:
		WRITE_VPP_REG_BITS(SRSHARP0_PK_NR_ENABLE,
				   1, 1, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_PK_NR_ENABLE,
				   1, 1, 1);
		break;
	case 1:
		WRITE_VPP_REG_BITS(SRSHARP0_PK_NR_ENABLE,
				   0, 1, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_PK_NR_ENABLE,
				   0, 1, 1);
		break;
	case 2:
		WRITE_VPP_REG_BITS(SRSHARP0_HCTI_FLT_CLP_DC,
				   1, 28, 1);
		WRITE_VPP_REG_BITS(SRSHARP0_HLTI_FLT_CLP_DC,
				   1, 28, 1);
		WRITE_VPP_REG_BITS(SRSHARP0_VLTI_FLT_CON_CLP,
				   1, 14, 1);
		WRITE_VPP_REG_BITS(SRSHARP0_VCTI_FLT_CON_CLP,
				   1, 14, 1);

		WRITE_VPP_REG_BITS(SRSHARP1_HCTI_FLT_CLP_DC,
				   1, 28, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_HLTI_FLT_CLP_DC,
				   1, 28, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_VLTI_FLT_CON_CLP,
				   1, 14, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_VCTI_FLT_CON_CLP,
				   1, 14, 1);
		break;
	case 3:
		WRITE_VPP_REG_BITS(SRSHARP0_HCTI_FLT_CLP_DC,
				   0, 28, 1);
		WRITE_VPP_REG_BITS(SRSHARP0_HLTI_FLT_CLP_DC,
				   0, 28, 1);
		WRITE_VPP_REG_BITS(SRSHARP0_VLTI_FLT_CON_CLP,
				   0, 14, 1);
		WRITE_VPP_REG_BITS(SRSHARP0_VCTI_FLT_CON_CLP,
				   0, 14, 1);

		WRITE_VPP_REG_BITS(SRSHARP1_HCTI_FLT_CLP_DC,
				   0, 28, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_HLTI_FLT_CLP_DC,
				   0, 28, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_VLTI_FLT_CON_CLP,
				   0, 14, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_VCTI_FLT_CON_CLP,
				   0, 14, 1);
		break;
	/*sr4 drtlpf theta en*/
	case 4:
		WRITE_VPP_REG_BITS(SRSHARP0_SR3_DRTLPF_EN,
				   7, 4, 3);
		WRITE_VPP_REG_BITS(SRSHARP1_SR3_DRTLPF_EN,
				   7, 3, 3);
		break;
	case 5:
		WRITE_VPP_REG_BITS(SRSHARP0_SR3_DRTLPF_EN,
				   0, 4, 3);
		WRITE_VPP_REG_BITS(SRSHARP1_SR3_DRTLPF_EN,
				   0, 3, 3);
		break;
	/*sr4 debanding en*/
	case 6:
		WRITE_VPP_REG_BITS(SRSHARP0_DB_FLT_CTRL,
				   1, 4, 1);
		WRITE_VPP_REG_BITS(SRSHARP0_DB_FLT_CTRL,
				   1, 5, 1);
		WRITE_VPP_REG_BITS(SRSHARP0_DB_FLT_CTRL,
				   1, 22, 1);
		WRITE_VPP_REG_BITS(SRSHARP0_DB_FLT_CTRL,
				   1, 23, 1);

		WRITE_VPP_REG_BITS(SRSHARP1_DB_FLT_CTRL,
				   1, 4, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_DB_FLT_CTRL,
				   1, 5, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_DB_FLT_CTRL,
				   1, 22, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_DB_FLT_CTRL,
				   1, 23, 1);
		break;
	case 7:
		WRITE_VPP_REG_BITS(SRSHARP0_DB_FLT_CTRL,
				   0, 4, 1);
		WRITE_VPP_REG_BITS(SRSHARP0_DB_FLT_CTRL,
				   0, 5, 1);
		WRITE_VPP_REG_BITS(SRSHARP0_DB_FLT_CTRL,
				   0, 22, 1);
		WRITE_VPP_REG_BITS(SRSHARP0_DB_FLT_CTRL,
				   0, 23, 1);

		WRITE_VPP_REG_BITS(SRSHARP1_DB_FLT_CTRL,
				   0, 4, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_DB_FLT_CTRL,
				   0, 5, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_DB_FLT_CTRL,
				   0, 22, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_DB_FLT_CTRL,
				   0, 23, 1);
		break;
	/*sr3 dejaggy en*/
	case 8:
		WRITE_VPP_REG_BITS(SRSHARP0_DEJ_CTRL,
				   1, 0, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_DEJ_CTRL,
				   1, 0, 1);
		break;
	case 9:
		WRITE_VPP_REG_BITS(SRSHARP0_DEJ_CTRL,
				   0, 0, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_DEJ_CTRL,
				   0, 0, 1);
		break;
	/*sr3 dering en*/
	case 10:
		WRITE_VPP_REG_BITS(SRSHARP0_SR3_DERING_CTRL,
				   1, 28, 3);
		WRITE_VPP_REG_BITS(SRSHARP1_SR3_DERING_CTRL,
				   1, 28, 3);
		break;
	case 11:
		WRITE_VPP_REG_BITS(SRSHARP0_SR3_DERING_CTRL,
				   0, 28, 3);
		WRITE_VPP_REG_BITS(SRSHARP1_SR3_DERING_CTRL,
				   0, 28, 3);
		break;
	/*sr3 direction lpf en*/
	case 12:
		WRITE_VPP_REG_BITS(SRSHARP0_SR3_DRTLPF_EN,
				   7, 0, 3);
		WRITE_VPP_REG_BITS(SRSHARP1_SR3_DRTLPF_EN,
				   7, 0, 3);
		break;
	case 13:
		WRITE_VPP_REG_BITS(SRSHARP0_SR3_DRTLPF_EN,
				   0, 0, 3);
		WRITE_VPP_REG_BITS(SRSHARP1_SR3_DRTLPF_EN,
				   0, 0, 3);
		break;

	default:
		break;
	}
}

void amvecm_clip_range_limit(bool limit_en)
{
	/*fix mbox av out flicker black dot*/
	if (limit_en) {
		/*cvbs output 16-235 16-240 16-240*/
		WRITE_VPP_REG(VPP_CLIP_MISC0, 0x3acf03c0);
		WRITE_VPP_REG(VPP_CLIP_MISC1, 0x4010040);
	} else {
		/*retore for other mode*/
		WRITE_VPP_REG(VPP_CLIP_MISC0, 0x3fffffff);
		WRITE_VPP_REG(VPP_CLIP_MISC1, 0x0);
	}
}
EXPORT_SYMBOL(amvecm_clip_range_limit);

static void amvecm_pq_enable(int enable)
{
	if (enable) {
		lc_en = 1;
		/* black_ext_en */
		WRITE_VPP_REG_BITS(VPP_VE_ENABLE_CTRL, 1, 3, 1);
		/* chroma_coring */
		WRITE_VPP_REG_BITS(VPP_VE_ENABLE_CTRL, 1, 4, 1);
		ve_enable_dnlp();
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
		if (!is_amdv_enable())
#endif
			amcm_enable(WR_VCB);
		WRITE_VPP_REG_BITS(SRSHARP0_PK_NR_ENABLE,
				   1, 1, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_PK_NR_ENABLE,
				   1, 1, 1);

		WRITE_VPP_REG_BITS(SRSHARP0_HCTI_FLT_CLP_DC,
				   1, 28, 1);
		WRITE_VPP_REG_BITS(SRSHARP0_HLTI_FLT_CLP_DC,
				   1, 28, 1);
		WRITE_VPP_REG_BITS(SRSHARP0_VLTI_FLT_CON_CLP,
				   1, 14, 1);
		WRITE_VPP_REG_BITS(SRSHARP0_VCTI_FLT_CON_CLP,
				   1, 14, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_HCTI_FLT_CLP_DC,
				   1, 28, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_HLTI_FLT_CLP_DC,
				   1, 28, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_VLTI_FLT_CON_CLP,
				   1, 14, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_VCTI_FLT_CON_CLP,
				   1, 14, 1);

		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXL)) {
			WRITE_VPP_REG_BITS(SRSHARP0_DEJ_CTRL,
					   1, 0, 1);
			WRITE_VPP_REG_BITS(SRSHARP0_SR3_DRTLPF_EN,
					   7, 0, 3);
			WRITE_VPP_REG_BITS(SRSHARP0_SR3_DERING_CTRL,
					   1, 28, 3);

			WRITE_VPP_REG_BITS(SRSHARP1_DEJ_CTRL,
					   1, 0, 1);
			WRITE_VPP_REG_BITS(SRSHARP1_SR3_DRTLPF_EN,
					   7, 0, 3);
			WRITE_VPP_REG_BITS(SRSHARP1_SR3_DERING_CTRL,
					   1, 28, 3);
		}
		/*sr4 drtlpf theta/ debanding en*/
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
		if (is_meson_txlx_cpu() || is_meson_txhd_cpu()) {
			WRITE_VPP_REG_BITS(SRSHARP0_SR3_DRTLPF_EN,
					   7, 4, 3);

			WRITE_VPP_REG_BITS(SRSHARP0_DB_FLT_CTRL,
					   1, 4, 1);
			WRITE_VPP_REG_BITS(SRSHARP0_DB_FLT_CTRL,
					   1, 5, 1);
			WRITE_VPP_REG_BITS(SRSHARP0_DB_FLT_CTRL,
					   1, 22, 1);
			WRITE_VPP_REG_BITS(SRSHARP0_DB_FLT_CTRL,
					   1, 23, 1);

			WRITE_VPP_REG_BITS(SRSHARP1_DB_FLT_CTRL,
					   1, 4, 1);
			WRITE_VPP_REG_BITS(SRSHARP1_DB_FLT_CTRL,
					   1, 5, 1);
			WRITE_VPP_REG_BITS(SRSHARP1_DB_FLT_CTRL,
					   1, 22, 1);
			WRITE_VPP_REG_BITS(SRSHARP1_DB_FLT_CTRL,
					   1, 23, 1);
		}
#endif

		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TM2)) {
			WRITE_VPP_REG_BITS(SRSHARP0_SR7_DRTLPF_EN,
					   0x3f, 0, 6);
			WRITE_VPP_REG_BITS(SRSHARP0_SR7_DRTLPF_EN,
					   0x7, 8, 3);
			WRITE_VPP_REG_BITS(SRSHARP0_SR7_PKDRT_BLD_EN,
					   1, 0, 1);
			WRITE_VPP_REG_BITS(SRSHARP0_SR7_TIBLD_PRT,
					   3, 2, 2);
			WRITE_VPP_REG_BITS(SRSHARP0_SR7_TIBLD_PRT,
					   3, 12, 2);
			WRITE_VPP_REG_BITS(SRSHARP0_SR7_XTI_SDFDEN,
					   3, 0, 2);
			WRITE_VPP_REG_BITS(SRSHARP0_SR7_TI_BPF_EN,
					   0xf, 0, 4);
			WRITE_VPP_REG_BITS(SRSHARP0_SR7_PKLONG_PF_EN,
					   3, 0, 2);
			WRITE_VPP_REG_BITS(SRSHARP0_SR7_CC_PK_ADJ,
					   1, 24, 1);
			WRITE_VPP_REG_BITS(SRSHARP0_SR7_GRAPHIC_CTRL,
					   1, 10, 1);

			WRITE_VPP_REG_BITS(SRSHARP1_SR7_DRTLPF_EN,
					   0x3f, 0, 6);
			WRITE_VPP_REG_BITS(SRSHARP1_SR7_DRTLPF_EN,
					   0x7, 8, 3);
			WRITE_VPP_REG_BITS(SRSHARP1_SR7_PKDRT_BLD_EN,
					   1, 0, 1);
			WRITE_VPP_REG_BITS(SRSHARP1_SR7_TIBLD_PRT,
					   3, 2, 2);
			WRITE_VPP_REG_BITS(SRSHARP1_SR7_TIBLD_PRT,
					   3, 12, 2);
			WRITE_VPP_REG_BITS(SRSHARP1_SR7_XTI_SDFDEN,
					   3, 0, 2);
			WRITE_VPP_REG_BITS(SRSHARP1_SR7_TI_BPF_EN,
					   0xf, 0, 4);
			WRITE_VPP_REG_BITS(SRSHARP1_SR7_PKLONG_PF_EN,
					   3, 0, 2);
			WRITE_VPP_REG_BITS(SRSHARP1_SR7_CC_PK_ADJ,
					   1, 24, 1);
			WRITE_VPP_REG_BITS(SRSHARP1_SR7_GRAPHIC_CTRL,
					   1, 10, 1);
		}

		white_balance_adjust(0, 1);

		vecm_latch_flag |= FLAG_GAMMA_TABLE_EN;
		if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A)
			WRITE_VPP_REG_BITS(VPP_VADJ1_MISC, 1, 0, 1);
		else
			WRITE_VPP_REG_BITS(VPP_VADJ_CTRL, 1, 0, 1);
	} else {
		lc_en = 0;
		/* black_ext_en */
		WRITE_VPP_REG_BITS(VPP_VE_ENABLE_CTRL, 0, 3, 1);
		/* chroma_coring */
		WRITE_VPP_REG_BITS(VPP_VE_ENABLE_CTRL, 0, 4, 1);
		ve_disable_dnlp();

		amcm_disable(WR_VCB);

		WRITE_VPP_REG_BITS(SRSHARP0_PK_NR_ENABLE,
				   0, 1, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_PK_NR_ENABLE,
				   0, 1, 1);

		WRITE_VPP_REG_BITS(SRSHARP0_HCTI_FLT_CLP_DC,
				   0, 28, 1);
		WRITE_VPP_REG_BITS(SRSHARP0_HLTI_FLT_CLP_DC,
				   0, 28, 1);
		WRITE_VPP_REG_BITS(SRSHARP0_VLTI_FLT_CON_CLP,
				   0, 14, 1);
		WRITE_VPP_REG_BITS(SRSHARP0_VCTI_FLT_CON_CLP,
				   0, 14, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_HCTI_FLT_CLP_DC,
				   0, 28, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_HLTI_FLT_CLP_DC,
				   0, 28, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_VLTI_FLT_CON_CLP,
				   0, 14, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_VCTI_FLT_CON_CLP,
				   0, 14, 1);

		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXL)) {
			WRITE_VPP_REG_BITS(SRSHARP0_DEJ_CTRL,
					   0, 0, 1);
			WRITE_VPP_REG_BITS(SRSHARP0_SR3_DRTLPF_EN
				, 0, 0, 3);
			WRITE_VPP_REG_BITS(SRSHARP0_SR3_DERING_CTRL
				, 0, 28, 3);

			WRITE_VPP_REG_BITS(SRSHARP1_DEJ_CTRL,
					   0, 0, 1);
			WRITE_VPP_REG_BITS(SRSHARP1_SR3_DRTLPF_EN
				, 0, 0, 3);
			WRITE_VPP_REG_BITS(SRSHARP1_SR3_DERING_CTRL
				, 0, 28, 3);
		}
		/*sr4 drtlpf theta/ debanding en*/
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
		if (is_meson_txlx_cpu()) {
			WRITE_VPP_REG_BITS(SRSHARP0_SR3_DRTLPF_EN
				, 0, 4, 3);

			WRITE_VPP_REG_BITS(SRSHARP0_DB_FLT_CTRL,
					   0, 4, 1);
			WRITE_VPP_REG_BITS(SRSHARP0_DB_FLT_CTRL,
					   0, 5, 1);
			WRITE_VPP_REG_BITS(SRSHARP0_DB_FLT_CTRL,
					   0, 22, 1);
			WRITE_VPP_REG_BITS(SRSHARP0_DB_FLT_CTRL,
					   0, 23, 1);

			WRITE_VPP_REG_BITS(SRSHARP1_DB_FLT_CTRL,
					   0, 4, 1);
			WRITE_VPP_REG_BITS(SRSHARP1_DB_FLT_CTRL,
					   0, 5, 1);
			WRITE_VPP_REG_BITS(SRSHARP1_DB_FLT_CTRL,
					   0, 22, 1);
			WRITE_VPP_REG_BITS(SRSHARP1_DB_FLT_CTRL,
					   0, 23, 1);
		}
#endif

		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TM2)) {
			WRITE_VPP_REG_BITS(SRSHARP0_SR7_DRTLPF_EN,
					   0, 0, 6);
			WRITE_VPP_REG_BITS(SRSHARP0_SR7_DRTLPF_EN,
					   0, 8, 3);
			WRITE_VPP_REG_BITS(SRSHARP0_SR7_PKDRT_BLD_EN,
					   0, 0, 1);
			WRITE_VPP_REG_BITS(SRSHARP0_SR7_TIBLD_PRT,
					   0, 2, 2);
			WRITE_VPP_REG_BITS(SRSHARP0_SR7_TIBLD_PRT,
					   0, 12, 2);
			WRITE_VPP_REG_BITS(SRSHARP0_SR7_XTI_SDFDEN,
					   0, 0, 2);
			WRITE_VPP_REG_BITS(SRSHARP0_SR7_TI_BPF_EN,
					   0, 0, 4);
			WRITE_VPP_REG_BITS(SRSHARP0_SR7_PKLONG_PF_EN,
					   0, 0, 2);
			WRITE_VPP_REG_BITS(SRSHARP0_SR7_CC_PK_ADJ,
					   0, 24, 1);
			WRITE_VPP_REG_BITS(SRSHARP0_SR7_GRAPHIC_CTRL,
					   0, 10, 1);

			WRITE_VPP_REG_BITS(SRSHARP1_SR7_DRTLPF_EN,
					   0, 0, 6);
			WRITE_VPP_REG_BITS(SRSHARP1_SR7_DRTLPF_EN,
					   0, 8, 3);
			WRITE_VPP_REG_BITS(SRSHARP1_SR7_PKDRT_BLD_EN,
					   0, 0, 1);
			WRITE_VPP_REG_BITS(SRSHARP1_SR7_TIBLD_PRT,
					   0, 2, 2);
			WRITE_VPP_REG_BITS(SRSHARP1_SR7_TIBLD_PRT,
					   0, 12, 2);
			WRITE_VPP_REG_BITS(SRSHARP1_SR7_XTI_SDFDEN,
					   0, 0, 2);
			WRITE_VPP_REG_BITS(SRSHARP1_SR7_TI_BPF_EN,
					   0, 0, 4);
			WRITE_VPP_REG_BITS(SRSHARP1_SR7_PKLONG_PF_EN,
					   0, 0, 2);
			WRITE_VPP_REG_BITS(SRSHARP1_SR7_CC_PK_ADJ,
					   0, 24, 1);
			WRITE_VPP_REG_BITS(SRSHARP1_SR7_GRAPHIC_CTRL,
					   0, 10, 1);
		}

		white_balance_adjust(0, 0);

		vecm_latch_flag |= FLAG_GAMMA_TABLE_DIS;

		if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A)
			WRITE_VPP_REG_BITS(VPP_VADJ1_MISC, 0, 0, 1);
		else
			WRITE_VPP_REG_BITS(VPP_VADJ_CTRL, 0, 0, 1);
	}
}

static void amvecm_dither_enable(int enable)
{
	switch (enable) {
		/*dither enable*/
	case 0:/*disable*/
		WRITE_VPP_REG_BITS(VPP_VE_DITHER_CTRL, 0, 0, 1);
		break;
	case 1:/*enable*/
		WRITE_VPP_REG_BITS(VPP_GAINOFF_CTRL0, 1, 0, 1);
		break;
		/*dither round enable*/
	case 2:/*disable*/
		WRITE_VPP_REG_BITS(VPP_VE_DITHER_CTRL, 0, 1, 1);
		break;
	case 3:/*enable*/
		WRITE_VPP_REG_BITS(VPP_VE_DITHER_CTRL, 1, 1, 1);
		break;
	default:
		break;
	}
}

static void amvecm_vpp_mtx_debug(int mtx_sel, int coef_sel)
{
	if (mtx_sel & (1 << VPP_MATRIX_1)) {
		WRITE_VPP_REG_BITS(VPP_MATRIX_CTRL, 1, 5, 1);
		WRITE_VPP_REG_BITS(VPP_MATRIX_CTRL, 1, 8, 3);
		mtx_sel_dbg &= ~(1 << VPP_MATRIX_1);
	} else if (mtx_sel & (1 << VPP_MATRIX_2)) {
		WRITE_VPP_REG_BITS(VPP_MATRIX_CTRL, 1, 0, 1);
		WRITE_VPP_REG_BITS(VPP_MATRIX_CTRL, 0, 8, 3);
		mtx_sel_dbg &= ~(1 << VPP_MATRIX_2);
	} else if (mtx_sel & (1 << VPP_MATRIX_3)) {
		WRITE_VPP_REG_BITS(VPP_MATRIX_CTRL, 1, 6, 1);
		WRITE_VPP_REG_BITS(VPP_MATRIX_CTRL, 3, 8, 3);
		mtx_sel_dbg &= ~(1 << VPP_MATRIX_3);
	}
	/*coef_sel 1: 10bit yuvl2rgb   2:rgb2yuvl*/
	/*coef_sel 3: 12bit yuvl2rgb   4:rgb2yuvl*/
	if (coef_sel == 1) {
		WRITE_VPP_REG(VPP_MATRIX_COEF00_01, 0x04A80000);
		WRITE_VPP_REG(VPP_MATRIX_COEF02_10, 0x072C04A8);
		WRITE_VPP_REG(VPP_MATRIX_COEF11_12, 0x1F261DDD);
		WRITE_VPP_REG(VPP_MATRIX_COEF20_21, 0x04A80876);
		WRITE_VPP_REG(VPP_MATRIX_COEF22, 0x0);
		WRITE_VPP_REG(VPP_MATRIX_OFFSET0_1, 0x0);
		WRITE_VPP_REG(VPP_MATRIX_OFFSET2, 0x0);
		WRITE_VPP_REG(VPP_MATRIX_PRE_OFFSET0_1, 0xfc00e00);
		WRITE_VPP_REG(VPP_MATRIX_PRE_OFFSET2, 0x0e00);
		WRITE_VPP_REG_BITS(VPP_MATRIX_CLIP, 0, 5, 3);
	} else if (coef_sel == 2) {
		WRITE_VPP_REG(VPP_MATRIX_COEF00_01, 0x00bb0275);
		WRITE_VPP_REG(VPP_MATRIX_COEF02_10, 0x003f1f99);
		WRITE_VPP_REG(VPP_MATRIX_COEF11_12, 0x1ea601c2);
		WRITE_VPP_REG(VPP_MATRIX_COEF20_21, 0x01c21e67);
		WRITE_VPP_REG(VPP_MATRIX_COEF22, 0x00001fd7);
		WRITE_VPP_REG(VPP_MATRIX_OFFSET0_1, 0x00400200);
		WRITE_VPP_REG(VPP_MATRIX_OFFSET2, 0x00000200);
		WRITE_VPP_REG(VPP_MATRIX_PRE_OFFSET0_1, 0x0);
		WRITE_VPP_REG(VPP_MATRIX_PRE_OFFSET2, 0x0);
		WRITE_VPP_REG_BITS(VPP_MATRIX_CLIP, 0, 5, 3);
	} else if (coef_sel == 3) {
		WRITE_VPP_REG(VPP_MATRIX_COEF00_01, 0x04A80000);
		WRITE_VPP_REG(VPP_MATRIX_COEF02_10, 0x072C04A8);
		WRITE_VPP_REG(VPP_MATRIX_COEF11_12, 0x1F261DDD);
		WRITE_VPP_REG(VPP_MATRIX_COEF20_21, 0x04A80876);
		WRITE_VPP_REG(VPP_MATRIX_COEF22, 0x0);
		WRITE_VPP_REG(VPP_MATRIX_OFFSET0_1, 0x8000800);
		WRITE_VPP_REG(VPP_MATRIX_OFFSET2, 0x800);
		WRITE_VPP_REG(VPP_MATRIX_PRE_OFFSET0_1, 0x7000000);
		WRITE_VPP_REG(VPP_MATRIX_PRE_OFFSET2, 0x0000);
		WRITE_VPP_REG_BITS(VPP_MATRIX_CLIP, 0, 5, 3);
	} else if (coef_sel == 4) {
		WRITE_VPP_REG(VPP_MATRIX_COEF00_01, 0x00bb0275);
		WRITE_VPP_REG(VPP_MATRIX_COEF02_10, 0x003f1f99);
		WRITE_VPP_REG(VPP_MATRIX_COEF11_12, 0x1ea601c2);
		WRITE_VPP_REG(VPP_MATRIX_COEF20_21, 0x01c21e67);
		WRITE_VPP_REG(VPP_MATRIX_COEF22, 0x00001fd7);
		WRITE_VPP_REG(VPP_MATRIX_OFFSET0_1, 0x01000000);
		WRITE_VPP_REG(VPP_MATRIX_OFFSET2, 0x00000000);
		WRITE_VPP_REG(VPP_MATRIX_PRE_OFFSET0_1, 0x0);
		WRITE_VPP_REG(VPP_MATRIX_PRE_OFFSET2, 0x0);
		WRITE_VPP_REG_BITS(VPP_MATRIX_CLIP, 0, 5, 3);
	}
}

static void vpp_clip_config(unsigned int mode_sel, unsigned int color,
			    unsigned int color_mode)
{
	unsigned int addr_cliptop, addr_clipbot, value_cliptop, value_clipbot;

	if (mode_sel == 0) {/*vd1*/
		addr_cliptop = VPP_VD1_CLIP_MISC0;
		addr_clipbot = VPP_VD1_CLIP_MISC1;
	} else if (mode_sel == 1) {/*vd2*/
		addr_cliptop = VPP_VD2_CLIP_MISC0;
		addr_clipbot = VPP_VD2_CLIP_MISC1;
	} else if (mode_sel == 2) {/*xvycc*/
		addr_cliptop = VPP_XVYCC_MISC0;
		addr_clipbot = VPP_XVYCC_MISC1;
	} else if (mode_sel == 3) {/*final clip*/
		addr_cliptop = VPP_CLIP_MISC0;
		addr_clipbot = VPP_CLIP_MISC1;
	} else {
		addr_cliptop = mode_sel;
		addr_clipbot = mode_sel + 1;
	}
	if (color == 0) {/*default*/
		value_cliptop = 0x3fffffff;
		value_clipbot = 0x0;
	} else if (color == 1) {/*Blue*/
		if (color_mode == 0) {/*yuv*/
			value_cliptop = (0x29 << 22) | (0xf0 << 12) |
				(0x6e << 2);
			value_clipbot = (0x29 << 22) | (0xf0 << 12) |
				(0x6e << 2);
		} else {/*RGB*/
			value_cliptop = 0xFF << 2;
			value_clipbot = 0xFF << 2;
		}
	} else if (color == 2) {/*Black*/
		if (color_mode == 0) {/*yuv*/
			value_cliptop = (0x10 << 22) | (0x80 << 12) |
				(0x80 << 2);
			value_clipbot = (0x10 << 22) | (0x80 << 12) |
				(0x80 << 2);
		} else {
			value_cliptop = 0;
			value_clipbot = 0;
		}
	} else {
		value_cliptop = color;
		value_clipbot = color;
	}
	WRITE_VPP_REG(addr_cliptop, value_cliptop);
	WRITE_VPP_REG(addr_clipbot, value_clipbot);
}

#define MAX_CLIP_VAL ((1 << 30) - 1)
static ssize_t amvecm_clamp_color_top_show(struct class *cla,
					   struct class_attribute *attr,
					   char *buf)
{
	return sprintf(buf, "0x%08x\n", READ_VPP_REG(VPP_CLIP_MISC0));
}

static ssize_t amvecm_clamp_color_top_store(struct class *cla,
					    struct class_attribute *attr,
					    const char *buf, size_t count)
{
	size_t r;
	u32 val;

	r = sscanf(buf, "%x\n", &val);
	if (r != 1 || val > MAX_CLIP_VAL)
		return -EINVAL;

	WRITE_VPP_REG(VPP_CLIP_MISC0, val);
	return count;
}

static ssize_t amvecm_clamp_color_bottom_show(struct class *cla,
					      struct class_attribute *attr,
					      char *buf)
{
	return sprintf(buf, "0x%08x\n", READ_VPP_REG(VPP_CLIP_MISC1));
}

static ssize_t amvecm_clamp_color_bottom_store(struct class *cla,
					       struct class_attribute *attr,
					       const char *buf, size_t count)
{
	size_t r;
	u32 val;

	r = sscanf(buf, "%x\n", &val);
	if (r != 1 || val > MAX_CLIP_VAL)
		return -EINVAL;

	WRITE_VPP_REG(VPP_CLIP_MISC1, val);
	return count;
}

static ssize_t amvecm_cpu_ver_show(struct class *cla,
				   struct class_attribute *attr, char *buf)
{
	pr_info("echo r cpu_ver > /sys/class/amvecm/cpu_ver");
	return 0;
}

static ssize_t amvecm_cpu_ver_store(struct class *cla,
				    struct class_attribute *attr,
			const char *buf, size_t count)
{
	char *buf_orig, *parm[8] = {NULL};

	if (!buf)
		return count;
	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig)
		return -ENOMEM;
	parse_param_amvecm(buf_orig, (char **)&parm);

	if (!strncmp(parm[0], "r", 1)) {
		if (!strncmp(parm[1], "cpu_ver", 7)) {
			if (!is_meson_tm2_cpu()) {
				pr_info("VER_NULL\n");
				kfree(buf_orig);
				return count;
			}
			if (is_meson_rev_a())
				pr_info("VER_A\n");
			else if (is_meson_rev_b())
				pr_info("VER_B\n");
			else
				pr_info("no ver\n");
		} else {
			pr_info("error cmd\n");
		}
	} else {
		pr_info("error cmd\n");
	}

	kfree(buf_orig);
	return count;
}

static ssize_t amvecm_cm2_hue_show(struct class *cla,
				   struct class_attribute *attr, char *buf)
{
	int i;
	int pos = 0;
	int cm_color_md_max = cm_14_ecm2colormode_max;

	if (cm_cur_work_color_md == cm_9_color)
		cm_color_md_max = ecm2colormode_max;

	for (i = 0; i < cm_color_md_max; i++)
		pos += sprintf(buf + pos, "%d %d %d\n", i,
			cm2_hue_array[i][0], cm2_hue_array[i][1]);
	return pos;
}

static ssize_t amvecm_cm2_hue_store(struct class *cla,
				    struct class_attribute *attr,
				    const char *buf, size_t count)
{
	char *buf_orig, *parm[8] = {NULL};
	long val = 0;
	unsigned int color_mode;
	struct cm_color_md cm_color_md_dbg;

	if (!buf)
		return count;
	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig)
		return -ENOMEM;
	parse_param_amvecm(buf_orig, (char **)&parm);
	if (!strncmp(parm[0], "cm2_hue", 7)) {
		//parm[1]: 0: 9-color; 1: 14-color
		{
			if (!strncmp(parm[1], "0", 1))
				cm_color_md_dbg.color_type = cm_9_color;
			else if (!strncmp(parm[1], "1", 1))
				cm_color_md_dbg.color_type = cm_14_color;
			else
				goto kfree_buf;
		}
		//parm[2]: color modes
		{
			if (kstrtoul(parm[2], 10, &val) < 0)
				goto kfree_buf;
			color_mode = val;
			if (cm_color_md_dbg.color_type == cm_9_color) {
				cm_color_md_dbg.cm_9_color_md = val;
				cm_color_md_dbg.cm_14_color_md =
					cm_14_ecm2colormode_max;
			} else {
				cm_color_md_dbg.cm_9_color_md = ecm2colormode_max;
				cm_color_md_dbg.cm_14_color_md = val;
			}
		}
		//parm[3]: color value
		{
			if (kstrtol(parm[3], 10, &val) < 0)
				goto kfree_buf;
			cm2_hue_array[color_mode][0] = val;
			cm_color_md_dbg.color_value = val;
		}
		//parm[4]: lpf flag
		{
			if (kstrtoul(parm[4], 10, &val) < 0)
				goto kfree_buf;
			cm2_hue_array[color_mode][1] = val;
			cm2_hue_array[color_mode][2] = 1;
		}

		cm2_hue(cm_color_md_dbg, cm2_hue_array[color_mode][0],
			cm2_hue_array[color_mode][1]);
		pq_user_latch_flag |= PQ_USER_CMS_CURVE_HUE;
		pr_info("cm2_hue ok\n");
	}
	kfree(buf_orig);
	return count;

kfree_buf:
	kfree(buf_orig);
	return -EINVAL;
}

static ssize_t amvecm_cm2_luma_show(struct class *cla,
				    struct class_attribute *attr,
				    char *buf)
{
	int i;
	int pos = 0;
	int cm_color_md_max = cm_14_ecm2colormode_max;

	if (cm_cur_work_color_md == cm_9_color)
		cm_color_md_max = ecm2colormode_max;

	for (i = 0; i < cm_color_md_max; i++)
		pos += sprintf(buf + pos, "%d %d %d\n", i,
			cm2_luma_array[i][0], cm2_luma_array[i][1]);
	return pos;
}

static ssize_t amvecm_cm2_luma_store(struct class *cla,
				     struct class_attribute *attr,
				     const char *buf, size_t count)
{
	char *buf_orig, *parm[8] = {NULL};
	long val = 0;
	unsigned int color_mode;
	struct cm_color_md cm_color_md_dbg;

	if (!buf)
		return count;
	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig)
		return -ENOMEM;
	parse_param_amvecm(buf_orig, (char **)&parm);
	if (!strncmp(parm[0], "cm2_luma", 7)) {
		//parm[1]: 0: 9-color; 1: 14-color
		{
			if (!strncmp(parm[1], "0", 1))
				cm_color_md_dbg.color_type = cm_9_color;
			else if (!strncmp(parm[1], "1", 1))
				cm_color_md_dbg.color_type = cm_14_color;
			else
				goto kfree_buf;
		}
		//parm[2]: color mode
		{
			if (kstrtoul(parm[2], 10, &val) < 0)
				goto kfree_buf;
			color_mode = val;
			if (cm_color_md_dbg.color_type == cm_9_color) {
				cm_color_md_dbg.cm_9_color_md = val;
				cm_color_md_dbg.cm_14_color_md =
					cm_14_ecm2colormode_max;
			} else {
				cm_color_md_dbg.cm_9_color_md = ecm2colormode_max;
				cm_color_md_dbg.cm_14_color_md = val;
			}
		}
		//parm[3]: color value
		{
			if (kstrtol(parm[3], 10, &val) < 0)
				goto kfree_buf;
			cm2_luma_array[color_mode][0] = val;
			cm_color_md_dbg.color_value = val;
		}
		//parm[4]: lpf flag
		{
			if (kstrtoul(parm[4], 10, &val) < 0)
				goto kfree_buf;
			cm2_luma_array[color_mode][1] = val;
			cm2_luma_array[color_mode][2] = 1;
		}

		cm2_luma(cm_color_md_dbg, cm2_luma_array[color_mode][0],
			 cm2_luma_array[color_mode][1]);
		pq_user_latch_flag |= PQ_USER_CMS_CURVE_LUMA;
		pr_info("cm2_luma ok\n");
	}
	kfree(buf_orig);
	return count;

kfree_buf:
	kfree(buf_orig);
	return -EINVAL;
}

static ssize_t amvecm_cm2_sat_show(struct class *cla,
				   struct class_attribute *attr,
				   char *buf)
{
	int i;
	int pos = 0;
	int cm_color_md_max = cm_14_ecm2colormode_max;

	if (cm_cur_work_color_md == cm_9_color)
		cm_color_md_max = ecm2colormode_max;

	for (i = 0; i < cm_color_md_max; i++)
		pos += sprintf(buf + pos, "%d %d %d\n", i,
			cm2_sat_array[i][0], cm2_sat_array[i][1]);
	return pos;
}

static ssize_t amvecm_cm2_sat_store(struct class *cla,
				    struct class_attribute *attr,
				    const char *buf, size_t count)
{
	char *buf_orig, *parm[8] = {NULL};
	long val = 0;
	unsigned int color_mode;
	struct cm_color_md cm_color_md_dbg;

	if (!buf)
		return count;
	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig)
		return -ENOMEM;
	parse_param_amvecm(buf_orig, (char **)&parm);
	if (!strncmp(parm[0], "cm2_sat", 7)) {
		//parm[1]: 0: 9-color; 1: 14-color
		{
			if (!strncmp(parm[1], "0", 1))
				cm_color_md_dbg.color_type = cm_9_color;
			else if (!strncmp(parm[1], "1", 1))
				cm_color_md_dbg.color_type = cm_14_color;
			else
				goto kfree_buf;
		}
		//parm[2]: color mode
		{
			if (kstrtoul(parm[2], 10, &val) < 0)
				goto kfree_buf;
			color_mode = val;
			if (cm_color_md_dbg.color_type == cm_9_color) {
				cm_color_md_dbg.cm_9_color_md = val;
				cm_color_md_dbg.cm_14_color_md =
					cm_14_ecm2colormode_max;
			} else {
				cm_color_md_dbg.cm_9_color_md = ecm2colormode_max;
				cm_color_md_dbg.cm_14_color_md = val;
			}
		}
		//parm[3]: color value
		{
			if (kstrtol(parm[3], 10, &val) < 0)
				goto kfree_buf;
			cm2_sat_array[color_mode][0] = val;
			cm_color_md_dbg.color_value = val;
		}
		//parm[4]: lpf flag
		{
			if (kstrtoul(parm[4], 10, &val) < 0)
				goto kfree_buf;
			cm2_sat_array[color_mode][1] = val;
			cm2_sat_array[color_mode][2] = 1;
		}

		cm2_sat(cm_color_md_dbg, cm2_sat_array[color_mode][0],
			cm2_sat_array[color_mode][1]);
		pq_user_latch_flag |= PQ_USER_CMS_CURVE_SAT;
		pr_info("cm2_sat ok\n");
	}
	kfree(buf_orig);
	return count;

kfree_buf:
	kfree(buf_orig);
	return -EINVAL;
}

static ssize_t amvecm_cm2_hue_by_hs_show(struct class *cla,
					 struct class_attribute *attr,
					 char *buf)
{
	int i;
	int pos = 0;
	int cm_color_md_max = cm_14_ecm2colormode_max;

	if (cm_cur_work_color_md == cm_9_color)
		cm_color_md_max = ecm2colormode_max;

	for (i = 0; i < cm_color_md_max; i++)
		pos += sprintf(buf + pos, "%d %d %d\n", i,
			cm2_hue_by_hs_array[i][0], cm2_hue_by_hs_array[i][1]);
	return pos;
}

static ssize_t amvecm_cm2_hue_by_hs_store(struct class *cla,
					  struct class_attribute *attr,
					  const char *buf, size_t count)
{
	char *buf_orig, *parm[8] = {NULL};
	long val = 0;
	unsigned int color_mode;
	struct cm_color_md cm_color_md_dbg;

	if (!buf)
		return count;
	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig)
		return -ENOMEM;
	parse_param_amvecm(buf_orig, (char **)&parm);
	if (!strncmp(parm[0], "cm2_hue_by_hs", 7)) {
		//parm[1]: 0: 9-color; 1: 14-color
		{
			if (!strncmp(parm[1], "0", 1))
				cm_color_md_dbg.color_type = cm_9_color;
			else if (!strncmp(parm[1], "1", 1))
				cm_color_md_dbg.color_type = cm_14_color;
			else
				goto kfree_buf;
		}
		//parm[2]: color mode
		{
			if (kstrtoul(parm[2], 10, &val) < 0)
				goto kfree_buf;
			color_mode = val;
			if (cm_color_md_dbg.color_type == cm_9_color) {
				cm_color_md_dbg.cm_9_color_md = val;
				cm_color_md_dbg.cm_14_color_md =
					cm_14_ecm2colormode_max;
			} else {
				cm_color_md_dbg.cm_9_color_md = ecm2colormode_max;
				cm_color_md_dbg.cm_14_color_md = val;
			}
		}
		//parm[3]: color value
		{
			if (kstrtol(parm[3], 10, &val) < 0)
				goto kfree_buf;
			cm2_hue_by_hs_array[color_mode][0] = val;
			cm_color_md_dbg.color_value = val;
		}
		//parm[4]: lpf flag
		{
			if (kstrtoul(parm[4], 10, &val) < 0)
				goto kfree_buf;
			cm2_hue_by_hs_array[color_mode][1] = val;
			cm2_hue_by_hs_array[color_mode][2] = 1;
		}

		cm2_hue_by_hs(cm_color_md_dbg,
				cm2_hue_by_hs_array[color_mode][0],
			      cm2_hue_by_hs_array[color_mode][1]);
		pq_user_latch_flag |= PQ_USER_CMS_CURVE_HUE_HS;
		pr_info("cm2_hue_by_hs ok\n");
	}
	kfree(buf_orig);
	return count;

kfree_buf:
	kfree(buf_orig);
	return -EINVAL;
}

static void cm_hist_config(unsigned int en, unsigned int mode,
			   unsigned int wd0, unsigned int wd1,
			   unsigned int wd2, unsigned int wd3)
{
	unsigned int value;

	WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT, STA_CFG_REG);
	value = READ_VPP_REG(VPP_CHROMA_DATA_PORT);
	value = (value & (~0xc0000000)) | (en << 30);
	WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT, STA_CFG_REG);
	WRITE_VPP_REG(VPP_CHROMA_DATA_PORT, value);

	WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT, LUMA_ADJ1_REG);
	value = READ_VPP_REG(VPP_CHROMA_DATA_PORT);
	value = (value & (~(0x1fff0000))) | (mode << 16);
	WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT, LUMA_ADJ1_REG);
	WRITE_VPP_REG(VPP_CHROMA_DATA_PORT, value);

	WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT, STA_WIN_XYXY0_REG);
	WRITE_VPP_REG(VPP_CHROMA_DATA_PORT, wd0 | (wd1 << 16));

	WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT, STA_WIN_XYXY1_REG);
	WRITE_VPP_REG(VPP_CHROMA_DATA_PORT, wd2 | (wd3 << 16));
}

static void cm_sta_hist_range_thrd(int r, int ro_frame,
				   int thrd0, int thrd1)
{
	unsigned int value0, value1;

	if (r) {
		WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT, STA_SAT_HIST0_REG);
		value0 = READ_VPP_REG(VPP_CHROMA_DATA_PORT);
		WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT, STA_SAT_HIST1_REG);
		value1 = READ_VPP_REG(VPP_CHROMA_DATA_PORT);
		pr_info("HIST0_REG = 0x%x, HIST1_REG = 0x%x\n", value0, value1);
	} else {
		WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT, STA_SAT_HIST0_REG);
		WRITE_VPP_REG(VPP_CHROMA_DATA_PORT, thrd0 | (ro_frame << 24));

		WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT, STA_SAT_HIST1_REG);
		WRITE_VPP_REG(VPP_CHROMA_DATA_PORT, thrd1);
	}
}

static void cm_luma_bri_con(int r, int brightness, int contrast,
			    int blk_lel)
{
	int value;

	if (r) {
		WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT, LUMA_ADJ0_REG);
		value = READ_VPP_REG(VPP_CHROMA_DATA_PORT);
		pr_info("contrast = 0x%x, blklel = 0x%x\n",
			value & 0xfff, (value >> 12) & 0x3ff);
		WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT, LUMA_ADJ1_REG);
		value = READ_VPP_REG(VPP_CHROMA_DATA_PORT);
		pr_info("bright = 0x%x, hist_mode = 0x%x\n",
			value & 0x1fff, (value >> 16) & 0x1fff);
	} else {
		WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT, LUMA_ADJ0_REG);
		WRITE_VPP_REG(VPP_CHROMA_DATA_PORT,
			      (blk_lel << 12) | contrast);

		WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT, LUMA_ADJ1_REG);
		value = READ_VPP_REG(VPP_CHROMA_DATA_PORT);
		WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT, LUMA_ADJ1_REG);
		WRITE_VPP_REG(VPP_CHROMA_DATA_PORT,
			      (value & (~(0x1fff))) | brightness);
	}
}

static void pr_cm_hist(enum cm_hist_e hist_sel)
{
	unsigned int *hist;
	int i;
	int addr_port = 0;
	int data_port = 0;
	//struct cm_port_s cm_port;

	if (chip_type_id == chip_s5) {
		/*s5 not support color hist*/
		//cm_port = get_cm_port();
		//addr_port = cm_port.cm_addr_port[SLICE0];
		//data_port = cm_port.cm_data_port[SLICE0];
		pr_info("not support\n");
	} else {
		addr_port = VPP_CHROMA_ADDR_PORT;
		data_port = VPP_CHROMA_DATA_PORT;
	}

	hist = kmalloc(32 * sizeof(unsigned int), GFP_KERNEL);
	if (!hist)
		return;
	memset(hist, 0, 32 * sizeof(unsigned int));

	switch (hist_sel) {
	case CM_HUE_HIST:
		for (i = 0; i < 32; i++) {
			WRITE_VPP_REG(addr_port,
				      RO_CM_HUE_HIST_BIN0 + i);
			hist[i] = READ_VPP_REG(data_port);
			pr_info("hist[%d] = 0x%8x\n", i, hist[i]);
		}
		break;
	case CM_SAT_HIST:
		for (i = 0; i < 32; i++) {
			WRITE_VPP_REG(addr_port,
				      RO_CM_SAT_HIST_BIN0 + i);
			hist[i] = READ_VPP_REG(data_port);
			pr_info("hist[%d] = 0x%8x\n", i, hist[i]);
		}
		break;
	default:
		break;
	}
	kfree(hist);
}

static void cm_init_config(int bitdepth)
{
	int i, j, m, reg;
	struct cm_port_s cm_port;
	unsigned int addr_port;
	unsigned int data_port;

	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A)
		am_set_regmap(&cm_default);
	else
		am_set_regmap(&cm_default_legacy);

	if (chip_type_id == chip_s5) {
		for (m = SLICE0; m < SLICE_MAX; m++) {
			cm_port = get_cm_port();
			addr_port = cm_port.cm_addr_port[m];
			data_port = cm_port.cm_data_port[m];

			WRITE_VPP_REG(addr_port, XVYCC_YSCP_REG);
			WRITE_VPP_REG(data_port, 0xfff0000);
			WRITE_VPP_REG(addr_port, XVYCC_USCP_REG);
			WRITE_VPP_REG(data_port, 0xfff0000);
			WRITE_VPP_REG(addr_port, XVYCC_VSCP_REG);
			WRITE_VPP_REG(data_port, 0xfff0000);
			WRITE_VPP_REG(addr_port, LUMA_ADJ0_REG);
			WRITE_VPP_REG(data_port, 0x100400);
			for (i = 0; i < 32; i++) {
				for (j = 0; j < 5; j++) {
					reg = CM2_ENH_COEF0_H00 + i * 8 + j;
					WRITE_VPP_REG(addr_port, reg);
					WRITE_VPP_REG(data_port, 0x0);
				}
			}
		}
	} else {
		if (bitdepth == 10) {
			WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT, XVYCC_YSCP_REG);
			WRITE_VPP_REG(VPP_CHROMA_DATA_PORT, 0x3ff0000);
			WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT, XVYCC_USCP_REG);
			WRITE_VPP_REG(VPP_CHROMA_DATA_PORT, 0x3ff0000);
			WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT, XVYCC_VSCP_REG);
			WRITE_VPP_REG(VPP_CHROMA_DATA_PORT, 0x3ff0000);
			WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT, LUMA_ADJ0_REG);
			WRITE_VPP_REG(VPP_CHROMA_DATA_PORT, 0x40400);
		} else if (bitdepth == 12) {
			WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT, XVYCC_YSCP_REG);
			WRITE_VPP_REG(VPP_CHROMA_DATA_PORT, 0xfff0000);
			WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT, XVYCC_USCP_REG);
			WRITE_VPP_REG(VPP_CHROMA_DATA_PORT, 0xfff0000);
			WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT, XVYCC_VSCP_REG);
			WRITE_VPP_REG(VPP_CHROMA_DATA_PORT, 0xfff0000);
			WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT, LUMA_ADJ0_REG);
			WRITE_VPP_REG(VPP_CHROMA_DATA_PORT, 0x100400);
		} else {
			WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT, XVYCC_YSCP_REG);
			WRITE_VPP_REG(VPP_CHROMA_DATA_PORT, 0x3ff0000);
			WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT, XVYCC_USCP_REG);
			WRITE_VPP_REG(VPP_CHROMA_DATA_PORT, 0x3ff0000);
			WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT, XVYCC_VSCP_REG);
			WRITE_VPP_REG(VPP_CHROMA_DATA_PORT, 0x3ff0000);
			WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT, LUMA_ADJ0_REG);
			WRITE_VPP_REG(VPP_CHROMA_DATA_PORT, 0x40400);
		}

		for (i = 0; i < 32; i++) {
			for (j = 0; j < 5; j++) {
				reg = CM2_ENH_COEF0_H00 + i * 8 + j;
				WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT, reg);
				WRITE_VPP_REG(VPP_CHROMA_DATA_PORT, 0x0);
			}
		}
	}
}

static void dnlp_init_config(void)
{
	memcpy(&dnlp_curve_param_load, &dnlp_default,
	       sizeof(struct ve_dnlp_curve_param_s));
	ve_new_dnlp_param_update();
}

static void sr_init_config(void)
{
	am_set_regmap(&sr0_default);
	am_set_regmap(&sr1_default);
}

void vlock_clk_config(struct amvecm_dev_s *devp, struct device *dev)
{
	if (!vlock_en)
		return;

	vlock_reg_config(dev);
	/*need set clock tree */
	devp->vlock_clk = devm_clk_get(dev, "cts_vid_lock_clk");
	if (!IS_ERR(devp->vlock_clk)) {
		clk_set_rate(devp->vlock_clk, 24000000);
		if (clk_prepare_enable(devp->vlock_clk) < 0)
			pr_info("vlock clk enable fail\n");
		/*clk_frq = clk_get_rate(clk);*/
		/*pr_info("cts_vid_lock_clk:%d\n", clk_frq);*/
		hw_clk_ok = 1;
	} else {
		pr_err("vlock clk not cfg\n");
		hw_clk_ok = 0;
	}
}

void vlock_clk_suspend(void)
{
	clk_disable_unprepare(amvecm_dev.vlock_clk);
}

void vlock_clk_resume(void)
{
	clk_prepare_enable(amvecm_dev.vlock_clk);
}

static const char *amvecm_debug_usage_str = {
	"Usage:\n"
	"echo vpp_size > /sys/class/amvecm/debug; get vpp size config\n"
	"echo wb enable > /sys/class/amvecm/debug\n"
	"echo wb disable > /sys/class/amvecm/debug\n"
	"echo gamma enable > /sys/class/amvecm/debug\n"
	"echo gamma disable > /sys/class/amvecm/debug\n"
	"echo gamma load_protect_en > /sys/class/amvecm/debug\n"
	"echo gamma load_protect_dis > /sys/class/amvecm/debug\n"
	"echo sr peaking_en > /sys/class/amvecm/debug\n"
	"echo sr peaking_dis > /sys/class/amvecm/debug\n"
	"echo sr lcti_en > /sys/class/amvecm/debug\n"
	"echo sr lcti_dis > /sys/class/amvecm/debug\n"
	"echo sr dejaggy_en > /sys/class/amvecm/debug\n"
	"echo sr dejaggy_dis > /sys/class/amvecm/debug\n"
	"echo sr dering_en > /sys/class/amvecm/debug\n"
	"echo sr dering_dis > /sys/class/amvecm/debug\n"
	"echo sr direc_en > /sys/class/amvecm/debug\n"
	"echo sr direc_dis > /sys/class/amvecm/debug\n"
	"echo sr theta_en > /sys/class/amvecm/debug\n"
	"echo sr theta_dis > /sys/class/amvecm/debug\n"
	"echo sr deband_en > /sys/class/amvecm/debug\n"
	"echo sr deband_dis > /sys/class/amvecm/debug\n"
	"echo cm enable > /sys/class/amvecm/debug\n"
	"echo cm disable > /sys/class/amvecm/debug\n"
	"echo cm cm2_clr_dbg val > /sys/class/amvecm/debug\n"
	"echo cm cur_color_md val(0:9clr 1:14clr) > /sys/class/amvecm/debug\n"
	"echo dnlp enable > /sys/class/amvecm/debug\n"
	"echo dnlp disable > /sys/class/amvecm/debug\n"
	"echo vpp_pq enable > /sys/class/amvecm/debug\n"
	"echo vpp_pq disable > /sys/class/amvecm/debug\n"
	"echo keystone_process > /sys/class/amvecm/debug; keystone init config\n"
	"echo keystone_status > /sys/class/amvecm/debug; keystone parameter status\n"
	"echo keystone_regs > /sys/class/amvecm/debug; keystone regs value\n"
	"echo keystone_config param1(D) param2(D) > /sys/class/amvecm/debug; keystone param config\n"
	"echo vpp_mtx xvycc_10 rgb2yuv > /sys/class/amvecm/debug; 10bit xvycc mtx\n"
	"echo vpp_mtx xvycc_10 yuv2rgb > /sys/class/amvecm/debug; 10bit xvycc mtx\n"
	"echo vpp_mtx post_10 rgb2yuv > /sys/class/amvecm/debug; 10bit post mtx\n"
	"echo vpp_mtx post_10 yuv2rgb > /sys/class/amvecm/debug; 10bit post mtx\n"
	"echo vpp_mtx vd1_10 rgb2yuv > /sys/class/amvecm/debug; 10bit vd1 mtx\n"
	"echo vpp_mtx vd1_10 yuv2rgb > /sys/class/amvecm/debug; 10bit vd1 mtx\n"
	"echo vpp_mtx xvycc_12 rgb2yuv > /sys/class/amvecm/debug; 12bit xvycc mtx\n"
	"echo vpp_mtx xvycc_12 yuv2rgb > /sys/class/amvecm/debug; 12bit xvycc mtx\n"
	"echo vpp_mtx post_12 rgb2yuv > /sys/class/amvecm/debug; 12bit post mtx\n"
	"echo vpp_mtx post_12 yuv2rgb > /sys/class/amvecm/debug; 12bit post mtx\n"
	"echo vpp_mtx vd1_12 rgb2yuv > /sys/class/amvecm/debug; 12bit vd1 mtx\n"
	"echo vpp_mtx vd1_12 yuv2rgb > /sys/class/amvecm/debug; 12bit vd1 mtx\n"
	"echo bitdepth 10/12/other-num > /sys/class/amvecm/debug; config data path\n"
	"echo datapath_config param1(D) param2(D) > /sys/class/amvecm/debug; config data path\n"
	"echo datapath_status > /sys/class/amvecm/debug; data path status\n"
	"echo clip_config 0/1/2/.. 0/1/... 0/1 > /sys/class/amvecm/debug; config clip\n"
};

static ssize_t amvecm_debug_show(struct class *cla,
				 struct class_attribute *attr, char *buf)
{
	pr_info("cm2_debug:%d\n", cm2_debug);
	pr_info("cm_cur_work_color_md:%d\n", cm_cur_work_color_md);
	return sprintf(buf, "%s\n", amvecm_debug_usage_str);
}

static ssize_t amvecm_debug_store(struct class *cla,
				  struct class_attribute *attr,
				  const char *buf, size_t count)
{
	char *buf_orig, *parm[8] = {NULL};
	long val = 0;
	unsigned int mode_sel, color, color_mode, i;
	struct vinfo_s *vinfo = get_current_vinfo();

	if (!buf)
		return count;
	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig)
		return -ENOMEM;
	parse_param_amvecm(buf_orig, (char **)&parm);
	if (!strncmp(parm[0], "vpp_size", 8)) {
		dump_vpp_size_info();
	} else if (!strncmp(parm[0], "vpp_state", 9)) {
		pr_info("amvecm driver version :  %s\n", AMVECM_VER);
	} else if (!strncmp(parm[0], "checkpattern", 12)) {
		if (!strncmp(parm[1], "enable", 6)) {
			pattern_detect_debug = 1;
			enable_pattern_detect = 1;
			pr_info("enable pattern detection\n");
		} else if (!strncmp(parm[1], "debug", 5)) {
			pattern_detect_debug = 2;
			pr_info("enable pattern detection debug info\n");
		} else if (!strncmp(parm[1], "disable", 7)) {
			pattern_detect_debug = 0;
			enable_pattern_detect = 0;
			pr_info("disable pattern detection\n");
		} else if (!strncmp(parm[1], "setmask", 7)) {
			if (kstrtoul(parm[2], 16, &val) < 0)
				goto free_buf;
			pattern_mask = val;
			pr_info("pattern_mask is 0x%x\n", pattern_mask);
		} else if (!strncmp(parm[1], "getmask", 7)) {
			pr_info("pattern_mask is 0x%x\n", pattern_mask);
		}
	} else if (!strncmp(parm[0], "wb", 2)) {
		if (!strncmp(parm[1], "enable", 6)) {
			amvecm_wb_enable(1);
			pr_info("enable wb\n");
		} else if (!strncmp(parm[1], "disable", 7)) {
			amvecm_wb_enable(0);
			pr_info("disable wb\n");
		}
	} else if (!strncmp(parm[0], "gamma", 5)) {
		if (!strncmp(parm[1], "enable", 6)) {
			vecm_latch_flag |= FLAG_GAMMA_TABLE_EN;	/* gamma off */
			pr_info("enable gamma\n");
		} else if (!strncmp(parm[1], "disable", 7)) {
			vecm_latch_flag |= FLAG_GAMMA_TABLE_DIS;/* gamma off */
			pr_info("disable gamma\n");
		} else if (!strncmp(parm[1], "load_protect_en", 15)) {
			gamma_loadprotect_en = 1;
			pr_info("disable gamma before loading new gamma\n");
		} else if (!strncmp(parm[1], "load_protect_dis", 16)) {
			gamma_loadprotect_en = 0;
			pr_info("loading new gamma without protect");
		}
	} else if (!strncmp(parm[0], "sr", 2)) {
		if (!strncmp(parm[1], "peaking_en", 10)) {
			amvecm_sharpness_enable(0);
			pr_info("enable peaking\n");
		} else if (!strncmp(parm[1], "peaking_dis", 11)) {
			amvecm_sharpness_enable(1);
			pr_info("disable peaking\n");
		} else if (!strncmp(parm[1], "lcti_en", 7)) {
			amvecm_sharpness_enable(2);
			pr_info("enable lti cti\n");
		} else if (!strncmp(parm[1], "lcti_dis", 8)) {
			amvecm_sharpness_enable(3);
			pr_info("disable lti cti\n");
		} else if (!strncmp(parm[1], "theta_en", 8)) {
			amvecm_sharpness_enable(4);
			pr_info("SR4 enable drtlpf theta\n");
		} else if (!strncmp(parm[1], "theta_dis", 9)) {
			amvecm_sharpness_enable(5);
			pr_info("SR4 disable drtlpf theta\n");
		} else if (!strncmp(parm[1], "deband_en", 9)) {
			amvecm_sharpness_enable(6);
			pr_info("SR4 enable debanding\n");
		} else if (!strncmp(parm[1], "deband_dis", 10)) {
			amvecm_sharpness_enable(7);
			pr_info("SR4 disable debanding\n");
		} else if (!strncmp(parm[1], "dejaggy_en", 10)) {
			amvecm_sharpness_enable(8);
			pr_info("SR3 enable dejaggy\n");
		} else if (!strncmp(parm[1], "dejaggy_dis", 11)) {
			amvecm_sharpness_enable(9);
			pr_info("SR3 disable dejaggy\n");
		} else if (!strncmp(parm[1], "dering_en", 9)) {
			amvecm_sharpness_enable(10);
			pr_info("SR3 enable dering\n");
		} else if (!strncmp(parm[1], "dering_dis", 10)) {
			amvecm_sharpness_enable(11);
			pr_info("SR3 disable dering\n");
		} else if (!strncmp(parm[1], "drlpf_en", 8)) {
			amvecm_sharpness_enable(12);
			pr_info("SR3 enable drlpf\n");
		} else if (!strncmp(parm[1], "drlpf_dis", 9)) {
			amvecm_sharpness_enable(13);
			pr_info("SR3 disable drlpf\n");
		} else if (!strncmp(parm[1], "enable", 6)) {
			amvecm_sharpness_enable(0);
			amvecm_sharpness_enable(2);
			amvecm_sharpness_enable(4);
			amvecm_sharpness_enable(6);
			amvecm_sharpness_enable(8);
			amvecm_sharpness_enable(10);
			amvecm_sharpness_enable(12);
			pr_info("SR enable\n");
		} else if (!strncmp(parm[1], "disable", 7)) {
			amvecm_sharpness_enable(1);
			amvecm_sharpness_enable(3);
			amvecm_sharpness_enable(5);
			amvecm_sharpness_enable(7);
			amvecm_sharpness_enable(9);
			amvecm_sharpness_enable(11);
			amvecm_sharpness_enable(13);
			pr_info("SR disable\n");
		}
	} else if (!strcmp(parm[0], "cm")) {
		if (!strncmp(parm[1], "enable", 6)) {
			cm_en = 1;
			pr_info("enable cm_en:%d\n", cm_en);
		} else if (!strncmp(parm[1], "disable", 7)) {
			cm_en = 0;
			pr_info("disable cm_en:%d\n", cm_en);
		} else if (!strcmp(parm[1], "cur_color_md")) {
			if (parm[2]) {
				if (kstrtoul(parm[2], 10, &val) < 0)
					goto free_buf;
				cm_cur_work_color_md = val;
			} else {
				pr_info("unsupport cm_cur_work_color_md cmd\n");
				goto free_buf;
			}
		} else if (!strcmp(parm[1], "cm2_clr_dbg")) {
			if (parm[2]) {
				if (kstrtoul(parm[2], 10, &val) < 0)
					goto free_buf;
				cm2_debug = val;
			} else {
				pr_info("unsupport cm2_clr_dbg cmd\n");
				goto free_buf;
			}
		} else if (!strcmp(parm[1], "cm_init")) {
			cm_init_config(12);
			pr_info("cm init\n");
		} else {
			pr_info("unsupport cm cmd\n");
			goto free_buf;
		}
	} else if (!strncmp(parm[0], "dnlp", 4)) {
		if (!strncmp(parm[1], "enable", 6)) {
			ve_enable_dnlp();
			pr_info("enable dnlp\n");
		} else if (!strncmp(parm[1], "disable", 7)) {
			ve_disable_dnlp();
			pr_info("disable dnlp\n");
		}
	} else if (!strncmp(parm[0], "vpp_pq", 6)) {
		if (!strncmp(parm[1], "enable", 6)) {
			amvecm_pq_enable(1);
			pr_info("enable vpp_pq\n");
		} else if (!strncmp(parm[1], "disable", 7)) {
			amvecm_pq_enable(0);
			pr_info("disable vpp_pq\n");
		}
	} else if (!strncmp(parm[0], "vpp_mtx", 7)) {
		if (!strncmp(parm[1], "vd1_10", 6)) {
			mtx_sel_dbg |= 1 << VPP_MATRIX_1;
			if (!strncmp(parm[2], "yuv2rgb", 7)) {
				amvecm_vpp_mtx_debug(mtx_sel_dbg, 1);
				pr_info("10bit vd1 mtx yuv2rgb\n");
			} else if (!strncmp(parm[2], "rgb2yuv", 7)) {
				amvecm_vpp_mtx_debug(mtx_sel_dbg, 2);
				pr_info("10bit vd1 mtx rgb2yuv\n");
			}
		} else if (!strncmp(parm[1], "post_10", 7)) {
			mtx_sel_dbg |= 1 << VPP_MATRIX_2;
			if (!strncmp(parm[2], "yuv2rgb", 7)) {
				amvecm_vpp_mtx_debug(mtx_sel_dbg, 1);
				pr_info("10bit post mtx yuv2rgb\n");
			} else if (!strncmp(parm[2], "rgb2yuv", 7)) {
				amvecm_vpp_mtx_debug(mtx_sel_dbg, 2);
				pr_info("10bit post mtx rgb2yuv\n");
			}
		} else if (!strncmp(parm[1], "xvycc_10", 8)) {
			mtx_sel_dbg |= 1 << VPP_MATRIX_3;
			if (!strncmp(parm[2], "yuv2rgb", 7)) {
				amvecm_vpp_mtx_debug(mtx_sel_dbg, 1);
				pr_info("10bit xvycc mtx yuv2rgb\n");
			} else if (!strncmp(parm[2], "rgb2yuv", 7)) {
				amvecm_vpp_mtx_debug(mtx_sel_dbg, 2);
				pr_info("10bit xvycc mtx rgb2yuv\n");
			}
		} else if (!strncmp(parm[1], "vd1_12", 6)) {
			mtx_sel_dbg |= 1 << VPP_MATRIX_1;
			if (!strncmp(parm[2], "yuv2rgb", 7)) {
				amvecm_vpp_mtx_debug(mtx_sel_dbg, 3);
				pr_info("1wbit vd1 mtx yuv2rgb\n");
			} else if (!strncmp(parm[2], "rgb2yuv", 7)) {
				amvecm_vpp_mtx_debug(mtx_sel_dbg, 4);
				pr_info("1wbit vd1 mtx rgb2yuv\n");
			}
		} else if (!strncmp(parm[1], "post_12", 7)) {
			mtx_sel_dbg |= 1 << VPP_MATRIX_2;
			if (!strncmp(parm[2], "yuv2rgb", 7)) {
				amvecm_vpp_mtx_debug(mtx_sel_dbg, 3);
				pr_info("1wbit post mtx yuv2rgb\n");
			} else if (!strncmp(parm[2], "rgb2yuv", 7)) {
				amvecm_vpp_mtx_debug(mtx_sel_dbg, 4);
				pr_info("1wbit post mtx rgb2yuv\n");
			}
		} else if (!strncmp(parm[1], "xvycc_12", 8)) {
			mtx_sel_dbg |= 1 << VPP_MATRIX_3;
			if (!strncmp(parm[2], "yuv2rgb", 7)) {
				amvecm_vpp_mtx_debug(mtx_sel_dbg, 3);
				pr_info("1wbit xvycc mtx yuv2rgb\n");
			} else if (!strncmp(parm[2], "rgb2yuv", 7)) {
				amvecm_vpp_mtx_debug(mtx_sel_dbg, 4);
				pr_info("1wbit xvycc mtx rgb2yuv\n");
			}
		}
	} else if (!strncmp(parm[0], "ve_dith", 7)) {
		if (!strncmp(parm[1], "enable", 6)) {
			amvecm_dither_enable(1);
			pr_info("enable ve dither\n");
		} else if (!strncmp(parm[1], "disable", 7)) {
			amvecm_dither_enable(0);
			pr_info("disable ve dither\n");
		} else if (!strncmp(parm[1], "rd_en", 5)) {
			amvecm_dither_enable(3);
			pr_info("enable ve round dither\n");
		} else if (!strncmp(parm[1], "rd_dis", 6)) {
			amvecm_dither_enable(2);
			pr_info("disable ve round dither\n");
		}
	} else if (!strcmp(parm[0], "keystone_process")) {
		keystone_correction_process();
		pr_info("keystone_correction_process done!\n");
	} else if (!strcmp(parm[0], "keystone_status")) {
		keystone_correction_status();
	} else if (!strcmp(parm[0], "keystone_regs")) {
		keystone_correction_regs();
	} else if (!strcmp(parm[0], "keystone_config")) {
		enum vks_param_e vks_param;
		unsigned int vks_param_val;

		if (!parm[2]) {
			pr_info("misss param\n");
			goto free_buf;
		}
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto free_buf;
		vks_param = val;
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto free_buf;
		vks_param_val = val;
		keystone_correction_config(vks_param, vks_param_val);
	} else if (!strcmp(parm[0], "bitdepth")) {
		unsigned int bitdepth;

		if (!parm[1]) {
			pr_info("misss param1\n");
			goto free_buf;
		}
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto free_buf;
		bitdepth = val;
		vpp_bitdepth_config(bitdepth);
	} else if (!strcmp(parm[0], "datapath_config")) {
		unsigned int node, param1, param2;

		if (!parm[1]) {
			pr_info("misss param1\n");
			goto free_buf;
		}
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto free_buf;
		node = val;

		if (!parm[2]) {
			pr_info("misss param2\n");
			goto free_buf;
		}
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto free_buf;
		param1 = val;

		if (!parm[3]) {
			pr_info("misss param3,default is 0\n");
			goto free_buf;
		}
		if (kstrtoul(parm[3], 10, &val) < 0)
			goto free_buf;
		param2 = val;
		vpp_datapath_config(node, param1, param2);
	} else if (!strcmp(parm[0], "datapath_status")) {
		vpp_datapath_status();
	} else if (!strcmp(parm[0], "clip_config")) {
		if (parm[1]) {
			if (kstrtoul(parm[1], 16, &val) < 0)
				goto free_buf;
			mode_sel = val;
		} else {
			mode_sel = 0;
		}
		if (parm[2]) {
			if (kstrtoul(parm[2], 16, &val) < 0)
				goto free_buf;
			color = val;
		} else {
			color = 0;
		}
		if (parm[3]) {
			if (kstrtoul(parm[3], 16, &val) < 0)
				goto free_buf;
			color_mode = val;
		} else {
			color_mode = 0;
		}
		vpp_clip_config(mode_sel, color, color_mode);
		pr_info("vpp_clip_config done!\n");
	} else if (!strcmp(parm[0], "3dlut_testpattern")) {
		int r, g, b;

		if (parm[1]) {
			if (kstrtol(parm[1], 10, &val) < 0)
				goto free_buf;
			r = val;
		} else {
			r = 0;
		}
		if (parm[2]) {
			if (kstrtol(parm[2], 10, &val) < 0)
				goto free_buf;
			g = val;
		} else {
			g = 0;
		}
		if (parm[3]) {
			if (kstrtol(parm[3], 10, &val) < 0)
				goto free_buf;
			b = val;
		} else {
			b = 0;
		}
		vpp_lut3d_table_init(r, g, b);
		vpp_set_lut3d(0, 0, 0, 0);
		vpp_lut3d_table_release();
	} else if (!strcmp(parm[0], "3dlut")) {
		if (!parm[1])
			goto free_buf;

		if (!strcmp(parm[1], "enable")) {
			lut3d_en = 1;
		} else if (!strcmp(parm[1], "disable")) {
			vpp_enable_lut3d(0);
			lut3d_en = 0;
		} else if (!strcmp(parm[1], "open")) {
			vpp_enable_lut3d(1);
		} else if (!strcmp(parm[1], "close")) {
			vpp_enable_lut3d(0);
		} else if (!strcmp(parm[1], "debug")) {
			if (parm[2]) {
				if (kstrtoul(parm[2], 10, &val) < 0)
					goto free_buf;
				lut3d_debug = val;
			} else {
				lut3d_debug = 0;
			}
		} else if (!strcmp(parm[1], "long_section")) {
			if (parm[2]) {
				if (kstrtoul(parm[2], 10, &val) < 0)
					goto free_buf;
				lut3d_long_sec_en = val;
			} else {
				lut3d_long_sec_en = 0;
			}
		} else if (!strcmp(parm[1], "compress")) {
			if (parm[2]) {
				if (kstrtoul(parm[2], 10, &val) < 0)
					goto free_buf;
				lut3d_compress = val;
			} else {
				lut3d_compress = 0;
			}
		} else if (!strcmp(parm[1], "write_source")) {
			if (parm[2]) {
				if (kstrtoul(parm[2], 10, &val) < 0)
					goto free_buf;
				lut3d_write_from_file = val;
			} else {
				lut3d_write_from_file = 0;
			}
		} else if (!strcmp(parm[1], "read_source")) {
			if (parm[2]) {
				if (kstrtoul(parm[2], 10, &val) < 0)
					goto free_buf;
				lut3d_data_source = val;
			} else {
				lut3d_data_source = 0;
			}
		} else if (!strcmp(parm[1], "order")) {
			if (parm[2]) {
				if (kstrtoul(parm[2], 10, &val) < 0)
					goto free_buf;
				lut3d_order = val;
			} else {
				lut3d_order = 0;
			}
		} else if (!strcmp(parm[1], "load")) {
			unsigned int index;

			if (parm[2]) {
				if (kstrtoul(parm[2], 10, &val) < 0)
					goto free_buf;
				index = val;
			} else {
				index = 0;
			}

			if (index > 2) {
				index = 2;
				pr_info("support up to 3 different luts\n");
			}

			vpp_lut3d_table_init(0, 0, 0);
			vpp_set_lut3d(lut3d_data_source, index, 0, 0);
			vpp_lut3d_table_release();
		} else if (!strcmp(parm[1], "writesection")) {
			unsigned int section_len, start, paracount;
			unsigned int *section_in;
			int readcount = 0;
			unsigned int encode_table_size;
			char data[4];
			char *buffer = NULL;
			struct file *fp;
			mm_segment_t fs;
			loff_t pos;

			encode_table_size =
				257 * sizeof(unsigned long);

			section_len = 17;
			if (lut3d_long_sec_en)
				section_len = 17 * 17 * 17;
			section_in = kmalloc(sizeof(unsigned int) * section_len * 3,
					     GFP_KERNEL);
			if (!section_in)
				goto free_buf;

			if (parm[2]) {
				if (kstrtoul(parm[2], 10, &val) < 0) {
					kfree(section_in);
					goto free_buf;
				}
				start = val;
			} else {
				start = 0;
			}

			if (start > ((17 * 17 * 17 / section_len) - 1)) {
				start = (17 * 17 * 17 / section_len) - 1;
				pr_info("section index should be in 0 ~ %d-1\n",
					(17 * 17 * 17 / section_len));
			}

			memset(section_in, 0,
			       section_len * 3 * sizeof(unsigned int));

			if (!parm[3]) {
				kfree(section_in);
				goto free_buf;
			}

			buffer = parm[3];
			readcount = strlen(buffer);
			if (lut3d_write_from_file) {
				buffer =
				kmalloc(section_len * 9 + encode_table_size,
					GFP_KERNEL);
				if (!buffer) {
					kfree(section_in);
					goto free_buf;
				}

				fp = filp_open(parm[3], O_RDONLY, 0);
				if (IS_ERR(fp)) {
					kfree(section_in);
					kfree(buffer);
					goto free_buf;
				}
				fs = get_fs();
				set_fs(KERNEL_DS);
				memset(buffer, 0,
				       section_len * 9 + encode_table_size);
				pos = 0;
				readcount =
				vfs_read(fp, buffer,
					 section_len * 9 + encode_table_size,
					 &pos);
				pr_info("read file lut data size %d\n",
					readcount);
				if (readcount <= 0) {
					kfree(section_in);
					kfree(buffer);
					goto free_buf;
				}
				filp_close(fp, NULL);
				set_fs(fs);
			}
			if (lut3d_compress) {
				huff64_decode(buffer, (unsigned int)readcount,
					      section_in, section_len * 3);
			} else {
				paracount = (strlen(buffer) + 2) / 3;
				if (paracount > (section_len * 3))
					paracount = section_len * 3;

				for (i = 0; i < paracount; ++i) {
					data[0] = buffer[3 * i + 0];
					data[1] = buffer[3 * i + 1];
					data[2] = buffer[3 * i + 2];
					data[3] = '\0';
					if (kstrtoul(data, 16, &val) < 0) {
						kfree(section_in);
						goto free_buf;
					}
					section_in[i] = val;
				}
			}

			vpp_write_lut3d_section(start,
						section_len,
						section_in);

			kfree(section_in);
			if (lut3d_write_from_file)
				kfree(buffer);
		} else if (!strcmp(parm[1], "readsection")) {
			unsigned int section_len, start, len;
			unsigned int *section_out;
			unsigned int encode_table_size;
			char *tmp, tmp1[10] = {0};
			struct file *fp;
			mm_segment_t fs;
			loff_t pos;

			encode_table_size =
				257 * sizeof(unsigned long);
			section_len = 17;
			if (lut3d_long_sec_en)
				section_len = 17 * 17 * 17;
			section_out =
			kmalloc(sizeof(unsigned int) * section_len * 3,
				GFP_KERNEL);
			if (!section_out)
				goto free_buf;
			/* extra space is for encoding table */
			/* make sure there is enough buffer to use */
			tmp =
			kmalloc(section_len * 9 + encode_table_size,
				GFP_KERNEL);
			if (!tmp) {
				kfree(section_out);
				goto free_buf;
			}
			memset(tmp, 0,
			       section_len * 9 + encode_table_size);

			if (parm[2]) {
				if (kstrtoul(parm[2], 10, &val) < 0) {
					kfree(section_out);
					kfree(tmp);
					goto free_buf;
				}
				start = val;
			} else {
				start = 0;
			}

			if (start > ((17 * 17 * 17 / section_len) - 1)) {
				start = (17 * 17 * 17 / section_len) - 1;
				pr_info("section index should be in 0 ~ %d-1\n",
					(17 * 17 * 17 / section_len));
			}

			vpp_read_lut3d_section(start,
					       section_len,
					       section_out);

			if (lut3d_compress) {
				len = huff64_encode(section_out,
						    section_len * 3,
						    tmp);
				tmp[len] = 0;
				pr_info("compressed len %d vs %d\n",
					len, section_len * 9);

			} else {
				for (i = 0; i < section_len; ++i) {
					sprintf(tmp1, "%03x%03x%03x",
						section_out[i * 3 + 0],
						section_out[i * 3 + 1],
						section_out[i * 3 + 2]);
					strcat(tmp, tmp1);
				}
				len = section_len * 9;
			}

			if (parm[3]) {
				fp = filp_open(parm[3],
					       O_RDWR | O_CREAT | O_APPEND,
					       0644);
				if (IS_ERR(fp)) {
					kfree(section_out);
					kfree(tmp);
					goto free_buf;
				}
				fs = get_fs();
				set_fs(KERNEL_DS);
				pos = fp->f_pos;
				vfs_write(fp, tmp,
					  len,
					  &pos);

				filp_close(fp, NULL);
				set_fs(fs);
			}

			kfree(section_out);
			kfree(tmp);
		} else {
			pr_info("unsupport cmd!\n");
		}
	} else if (!strcmp(parm[0], "3dlut_dump")) {
		if (!strcmp(parm[1], "init_tab"))
			dump_plut3d_table();
		else if (!strcmp(parm[1], "reg_tab"))
			dump_plut3d_reg_table();
		else
			pr_info("unsupport cmd!\n");
	} else if (!strcmp(parm[0], "cm_hist")) {
		if (!parm[1]) {
			pr_info("miss param1\n");
			goto free_buf;
		}
		if (!strcmp(parm[1], "hue"))
			pr_cm_hist(CM_HUE_HIST);
		else if (!strcmp(parm[1], "sat"))
			pr_cm_hist(CM_SAT_HIST);
		else
			pr_info("unsupport cmd\n");
	}  else if (!strcmp(parm[0], "cm_hist_config")) {
		unsigned int en, mode, wd0, wd1, wd2, wd3;

		if (!parm[6]) {
			pr_info("miss param1\n");
			goto free_buf;
		}
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto free_buf;
		en = val;
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto free_buf;
		mode = val;
		if (kstrtoul(parm[3], 10, &val) < 0)
			goto free_buf;
		wd0 = val;
		if (kstrtoul(parm[4], 10, &val) < 0)
			goto free_buf;
		wd1 = val;
		if (kstrtoul(parm[5], 10, &val) < 0)
			goto free_buf;
		wd2 = val;
		if (kstrtoul(parm[6], 10, &val) < 0)
			goto free_buf;
		wd3 = val;
		cm_hist_config(en, mode, wd0, wd1, wd2, wd3);
		pr_info("cm hist config success\n");
	} else if (!strcmp(parm[0], "cm_hist_thrd")) {
		int rd, ro_frame, thrd0, thrd1;

		if (parm[1]) {
			if (kstrtoul(parm[1], 16, &val) < 0)
				goto free_buf;
			rd = val;
		} else {
			pr_info("unsupport cmd\n");
			goto free_buf;
		}
		if (rd) {
			cm_sta_hist_range_thrd(rd, 0, 0, 0);
		} else {
			if (!parm[4]) {
				pr_info("miss param1\n");
				goto free_buf;
			}
			if (kstrtoul(parm[2], 16, &val) < 0)
				goto free_buf;
			ro_frame = val;
			if (kstrtoul(parm[3], 16, &val) < 0)
				goto free_buf;
			thrd0 = val;
			if (kstrtoul(parm[4], 16, &val) < 0)
				goto free_buf;
			thrd1 = val;
			cm_sta_hist_range_thrd(rd, ro_frame, thrd0, thrd1);
			pr_info("cm hist thrd set success\n");
		}
	} else if (!strcmp(parm[0], "cm_bri_con")) {
		int rd, bri, con, blk_lel;

		if (parm[1]) {
			if (kstrtoul(parm[1], 16, &val) < 0)
				goto free_buf;
			rd = val;
		} else {
			pr_info("unsupport cmd\n");
			goto free_buf;
		}

		if (rd) {
			cm_luma_bri_con(rd, 0, 0, 0);
		} else {
			if (!parm[3]) {
				pr_info("miss param1\n");
				goto free_buf;
			}
			if (kstrtoul(parm[1], 16, &val) < 0)
				goto free_buf;
			bri = val;
			if (kstrtoul(parm[2], 16, &val) < 0)
				goto free_buf;
			con = val;
			if (kstrtoul(parm[3], 16, &val) < 0)
				goto free_buf;
			blk_lel = val;
			cm_luma_bri_con(rd, bri, con, blk_lel);
			pr_info("cm hist bri_con set success\n");
		}
	} else if (!strcmp(parm[0], "dump_overscan_table")) {
		for (i = 0; i < TIMING_MAX; i++) {
			pr_info("*****dump overscan_tab[%d]*****\n", i);
			pr_info("hs:%d, he:%d, vs:%d, ve:%d, screen_mode:%d, afd:%d, flag:%d.\n",
				overscan_table[i].hs, overscan_table[i].he,
				overscan_table[i].vs, overscan_table[i].ve,
				overscan_table[i].screen_mode,
				overscan_table[i].afd_enable,
				overscan_table[i].load_flag);
		}
	} else if (!strcmp(parm[0], "set_gamma_lut_65")) {
		if (!strcmp(parm[1], "default"))
			set_gamma_regs(1, 0);
		else if (!strcmp(parm[1], "straight"))
			set_gamma_regs(1, 1);
		else
			pr_info("unsupport cmd\n");
	} else if (!strcmp(parm[0], "pd_fix_lvl")) {
		if (parm[1]) {
			if (kstrtoul(parm[1], 10, &val) < 0)
				goto free_buf;
		}
		if (val > PD_DEF_LVL)
			pd_fix_lvl = PD_DEF_LVL;
		else
			pd_fix_lvl = val;
	} else if (!strcmp(parm[0], "gmv_th")) {
		if (parm[1]) {
			if (kstrtoul(parm[1], 10, &val) < 0)
				goto free_buf;
		}
		gmv_th = val;
	} else if (!strcmp(parm[0], "pd_weak_fix_lvl")) {
		if (parm[1]) {
			if (kstrtoul(parm[1], 10, &val) < 0)
				goto free_buf;
		}
		if (val > PD_DEF_LVL)
			pd_weak_fix_lvl = PD_DEF_LVL;
		else
			pd_weak_fix_lvl = val;
	} else if (!strcmp(parm[0], "gmv_weak_th")) {
		if (parm[1]) {
			if (kstrtoul(parm[1], 10, &val) < 0)
				goto free_buf;
		}
		if (val >= gmv_th)
			gmv_weak_th = gmv_th - 1;
		else
			gmv_weak_th = val;
	} else if (!strcmp(parm[0], "post2mtx")) {
		if (parm[1]) {
			if (kstrtoul(parm[1], 16, &val) < 0)
				goto free_buf;
			mtx_setting(POST2_MTX, val, 1);
		}
	} else if (!strcmp(parm[0], "mltcast_ratio1")) {
		pr_info("current value: %d\n", mltcast_ratio1);
		if (parm[1]) {
			if (kstrtoul(parm[1], 10, &val) < 0)
				goto free_buf;
		}
		mltcast_ratio1 = val;
		pr_info("setting value: %d\n", mltcast_ratio1);
	} else if (!strcmp(parm[0], "mltcast_ratio2")) {
		pr_info("current value: %d\n", mltcast_ratio2);
		if (parm[1]) {
			if (kstrtoul(parm[1], 10, &val) < 0)
				goto free_buf;
		}
		mltcast_ratio2 = val;
		pr_info("setting value: %d\n", mltcast_ratio2);
	} else if (!strcmp(parm[0], "mltcast_skip_en")) {
		pr_info("current value: %d\n", mltcast_skip_en);
		if (parm[1]) {
			if (kstrtoul(parm[1], 10, &val) < 0)
				goto free_buf;
		}
		mltcast_skip_en = val;
		pr_info("setting value: %d\n", mltcast_skip_en);
	} else if (!strcmp(parm[0], "pst_hist_sel")) {
		if (parm[1]) {
			if (kstrtoul(parm[1], 10, &val) < 0)
				goto free_buf;
			if (val == 1) {
				vpp_pst_hist_sta_config(1, HIST_MAXRGB,
					AFTER_POST2_MTX, vinfo);
				pr_info("hist sel: max rgb hist\n");
			} else {
				vpp_pst_hist_sta_config(1, HIST_YR,
					BEFORE_POST2_MTX, vinfo);
				pr_info("hist sel: Y hist\n");
			}
		}
	} else if (!strcmp(parm[0], "pd_det")) {
		if (parm[1]) {
			if (kstrtoul(parm[1], 10, &val) < 0)
				goto free_buf;
		}
		pd_det = (uint)val;
		pr_info("pd_det: %d\n", pd_det);
	} else if (!strcmp(parm[0], "dyn_gamma")) {
		u8 dynamic_gamma_num;

		if (!strcmp(parm[1], "gamma_tb")) {
			if (parm[2]) {
				if (kstrtoul(parm[2], 10, &val) < 0)
					goto free_buf;
			}

			for (i = 0; i < 256; i++)
				pr_info("vd_gamma_r_data[%d] :%d vd_gamma_g_data[%d] :%d vd_gamma_b_data[%d] :%d\n",
				i, video_gamma_table_r.data[i],
				i, video_gamma_table_g.data[i],
				i, video_gamma_table_b.data[i]);
		} else if (!strcmp(parm[1], "gamma_gt_tb")) {
			if (parm[2]) {
				if (kstrtoul(parm[2], 10, &val) < 0)
					goto free_buf;
			}
			dynamic_gamma_num = (u8)val;

			if (dynamic_gamma_num >= FREESYNC_DYNAMIC_GAMMA_NUM)
				dynamic_gamma_num = FREESYNC_DYNAMIC_GAMMA_NUM - 1;

			for (i = 0; i < 256; i++)
				pr_info("gt_gamma_r_data[%d][%d] :%d gt_gamma_g_data[%d][%d] :%d gt_gamma_b_data[%d][%d] :%d\n",
				dynamic_gamma_num, i, gt.gm_tb[dynamic_gamma_num][0].data[i],
				dynamic_gamma_num, i, gt.gm_tb[dynamic_gamma_num][1].data[i],
				dynamic_gamma_num, i, gt.gm_tb[dynamic_gamma_num][2].data[i]);
		}
	} else if (!strcmp(parm[0], "pr_hist")) {
		if (parm[1]) {
			if (kstrtoul(parm[1], 10, &val) < 0)
				goto free_buf;
		}
		pr_hist = (uint)val;
		pr_info("pr_hist = %d\n", pr_hist);
	} else if (!strcmp(parm[0], "vlk_clk")) {
		if (!strcmp(parm[1], "disable")) {
			clk_disable_unprepare(amvecm_dev.vlock_clk);
			pr_info("vid_lock_clk disable!!!");
		} else if (!strcmp(parm[1], "enable")) {
			clk_prepare_enable(amvecm_dev.vlock_clk);
			pr_info("vid_lock_clk enable!!!");
		} else {
			pr_info("unsupport cmd\n");
		}
	} else {
		pr_info("unsupport cmd\n");
	}
	vpp_pst_hist_sta_config(1, HIST_MAXRGB,
		AFTER_POST2_MTX, vinfo);

	kfree(buf_orig);
	return count;

free_buf:
	kfree(buf_orig);
	return -EINVAL;
}

static const char *amvecm_reg_usage_str = {
	"Usage:\n"
	"echo rv addr(H) > /sys/class/amvecm/reg;\n"
	"echo rc addr(H) > /sys/class/amvecm/reg;\n"
	"echo rh addr(H) > /sys/class/amvecm/reg; read hiu reg\n"
	"echo wv addr(H) value(H) > /sys/class/amvecm/reg; write vpu reg\n"
	"echo wc addr(H) value(H) > /sys/class/amvecm/reg; write cbus reg\n"
	"echo wh addr(H) value(H) > /sys/class/amvecm/reg; write hiu reg\n"
	"echo dv|c|h addr(H) num > /sys/class/amvecm/reg; dump reg from addr\n"
};

static ssize_t amvecm_reg_show(struct class *cla,
			       struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", amvecm_reg_usage_str);
}

static ssize_t amvecm_reg_store(struct class *cla,
				struct class_attribute *attr,
				const char *buf, size_t count)
{
	char *buf_orig, *parm[8] = {NULL};
	long val = 0;
	unsigned int reg_addr, reg_val, i;

	if (!buf)
		return count;
	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig)
		return -ENOMEM;
	parse_param_amvecm(buf_orig, (char **)&parm);
	if (!strcmp(parm[0], "rv")) {
		if (kstrtoul(parm[1], 16, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		reg_addr = val;
		reg_val = READ_VPP_REG_EX(reg_addr, 0);
		pr_info("VPU[0x%04x]=0x%08x\n", reg_addr, reg_val);
	} else if (!strcmp(parm[0], "rc")) {
		if (kstrtoul(parm[1], 16, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		reg_addr = val;
		reg_val = aml_read_cbus(reg_addr);
		pr_info("CBUS[0x%04x]=0x%08x\n", reg_addr, reg_val);
	} else if (!strcmp(parm[0], "rh")) {
		if (kstrtoul(parm[1], 16, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		reg_addr = val;
		amvecm_hiu_reg_read(reg_addr, &reg_val);
		pr_info("HIU[0x%04x]=0x%08x\n", reg_addr, reg_val);
	} else if (!strcmp(parm[0], "wv")) {
		if (kstrtoul(parm[1], 16, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		reg_addr = val;
		if (kstrtoul(parm[2], 16, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		reg_val = val;
		WRITE_VPP_REG_EX(reg_addr, reg_val, 0);
	} else if (!strcmp(parm[0], "wc")) {
		if (kstrtoul(parm[1], 16, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		reg_addr = val;
		if (kstrtoul(parm[2], 16, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		reg_val = val;
		aml_write_cbus(reg_addr, reg_val);
	} else if (!strcmp(parm[0], "wh")) {
		if (kstrtoul(parm[1], 16, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		reg_addr = val;
		if (kstrtoul(parm[2], 16, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		reg_val = val;
		amvecm_hiu_reg_write(reg_addr, reg_val);
	} else if (parm[0][0] == 'd') {
		if (kstrtoul(parm[1], 16, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		reg_addr = val;
		if (kstrtoul(parm[2], 16, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		for (i = 0; i < val; i++) {
			if (parm[0][1] == 'v') {
				reg_val = READ_VPP_REG_EX(reg_addr + i, 0);
			} else if (parm[0][1] == 'c') {
				reg_val = aml_read_cbus(reg_addr + i);
			} else if (parm[0][1] == 'h') {
				amvecm_hiu_reg_read((reg_addr + i),
						    &reg_val);
			} else {
				pr_info("unsupport cmd!\n");
				kfree(buf_orig);
				return -EINVAL;
			}
			pr_info("REG[0x%04x]=0x%08x\n",
				(reg_addr + i), reg_val);
		}
	} else {
		pr_info("unsupport cmd!\n");
	}
	kfree(buf_orig);
	return count;
}

static ssize_t amvecm_get_hdr_type_show(struct class *cla,
					struct class_attribute *attr,
					char *buf)
{
	return sprintf(buf, "%x\n", hdr_source_type);
}

static ssize_t amvecm_get_hdr_type_store(struct class *cls,
					 struct class_attribute *attr,
					 const char *buffer, size_t count)
{
	return count;
}

static void lc_rd_reg(enum lc_reg_lut_e reg_sel, int data_type, char *buf)
{
	int i, j, tmp, tmp1, tmp2, len = 12;
	int lut_data[63] = {0};
	char *stemp = NULL;

	if (data_type == 1)
		goto dump_as_string;

	switch (reg_sel) {
	case SATUR_LUT:
		for (i = 0; i < 31 ; i++) {
			tmp = READ_VPP_REG(SRSHARP1_LC_SAT_LUT_0_1 + i);
			tmp1 = (tmp >> 16) & 0xfff;
			tmp2 = tmp & 0xfff;
			pr_info("reg_lc_satur_lut[%d] =%4d.\n",
				2 * i, tmp1);
			pr_info("reg_lc_satur_lut[%d] =%4d.\n",
				2 * i + 1, tmp2);
		}
		tmp = READ_VPP_REG(SRSHARP1_LC_SAT_LUT_62);
		pr_info("reg_lc_satur_lut[62] =%4d.\n",
			tmp & 0xfff);
		break;
	case YMINVAL_LMT:
		for (i = 0; i < 6 ; i++) {
			tmp = READ_VPP_REG(LC_CURVE_YMINVAL_LMT_0_1 + i);
			tmp1 = (tmp >> 16) & 0x3ff;
			tmp2 = tmp & 0x3ff;
			pr_info("reg_yminVal_lmt[%d] =%4d.\n",
				2 * i, tmp1);
			pr_info("reg_yminVal_lmt[%d] =%4d.\n",
				2 * i + 1, tmp2);
		}
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TM2)) {
			for (j = 0; j < 2 ; j++) {
				tmp =
				READ_VPP_REG(LC_CURVE_YMINVAL_LMT_12_13 + j);
				tmp1 = (tmp >> 16) & 0x3ff;
				tmp2 = tmp & 0x3ff;
				pr_info("reg_yminVal_lmt[%d] =%4d.\n",
					2 * i, tmp1);
				pr_info("reg_yminVal_lmt[%d] =%4d.\n",
					2 * i + 1, tmp2);
				i++;
			}
		}
		break;
	case YPKBV_YMAXVAL_LMT:
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TM2))
			break;

		for (i = 0; i < 6 ; i++) {
			tmp = READ_VPP_REG(LC_CURVE_YPKBV_YMAXVAL_LMT_0_1 + i);
			tmp1 = (tmp >> 16) & 0x3ff;
			tmp2 = tmp & 0x3ff;
			pr_info("reg_lc_ypkBV_ymaxVal_lmt[%d] =%4d.\n",
				2 * i, tmp1);
			pr_info("reg_lc_ypkBV_ymaxVal_lmt[%d] =%4d.\n",
				2 * i + 1, tmp2);
		}
		break;
	case YMAXVAL_LMT:
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TM2)) {
			for (i = 0; i < 6 ; i++) {
				tmp =
				READ_VPP_REG(LC_CURVE_YMAXVAL_LMT_0_1 + i);
				tmp1 = (tmp >> 16) & 0x3ff;
				tmp2 = tmp & 0x3ff;
				pr_info("reg_lc_ymaxVal_lmt[%d] =%4d.\n",
					2 * i, tmp1);
				pr_info("reg_lc_ymaxVal_lmt[%d] =%4d.\n",
					2 * i + 1, tmp2);
			}

			for (j = 0; j < 2 ; j++) {
				tmp =
				READ_VPP_REG(LC_CURVE_YMAXVAL_LMT_12_13 + j);
				tmp1 = (tmp >> 16) & 0x3ff;
				tmp2 = tmp & 0x3ff;
				pr_info("reg_lc_ymaxVal_lmt[%d] =%4d.\n",
					2 * i, tmp1);
				pr_info("reg_lc_ymaxVal_lmt[%d] =%4d.\n",
					2 * i + 1, tmp2);
				i++;
			}
		}
		break;
	case YPKBV_LMT:
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TM2)) {
			for (i = 0; i < 8 ; i++) {
				tmp =
				READ_VPP_REG(LC_CURVE_YPKBV_LMT_0_1 + i);
				tmp1 = (tmp >> 16) & 0x3ff;
				tmp2 = tmp & 0x3ff;
				pr_info("reg_lc_ypkBV_lmt[%d] =%4d.\n",
					2 * i, tmp1);
				pr_info("reg_lc_ypkBV_lmt[%d] =%4d.\n",
					2 * i + 1, tmp2);
			}
		}
		break;
	case YPKBV_RAT:
		tmp = READ_VPP_REG(LC_CURVE_YPKBV_RAT);
		pr_info("reg_lc_ypkBV_ratio[0] =%4d.\n",
			(tmp >> 24) & 0xff);
		pr_info("reg_lc_ypkBV_ratio[1] =%4d.\n",
			(tmp >> 16) & 0xff);
		pr_info("reg_lc_ypkBV_ratio[2] =%4d.\n",
			(tmp >> 8) & 0xff);
		pr_info("reg_lc_ypkBV_ratio[3] =%4d.\n",
			tmp & 0xff);
		break;
	case YPKBV_SLP_LMT:
		tmp = READ_VPP_REG(LC_CURVE_YPKBV_SLP_LMT);
		pr_info("reg_lc_ypkBV_slope_lmt[0] =%4d.\n",
			(tmp >> 8) & 0xff);
		pr_info("reg_lc_ypkBV_slope_lmt[1] =%4d.\n",
			tmp & 0xff);
		break;
	case CNTST_LMT:
		tmp = READ_VPP_REG(LC_CURVE_CONTRAST__LMT_LH);
		pr_info("reg_lc_cntstlmt_low[0] =%4d.\n",
			(tmp >> 24) & 0xff);
		pr_info("reg_lc_cntstlmt_high[0] =%4d.\n",
			(tmp >> 16) & 0xff);
		pr_info("reg_lc_cntstlmt_low[1] =%4d.\n",
			(tmp >> 8) & 0xff);
		pr_info("reg_lc_cntstlmt_high[1] =%4d.\n",
			tmp & 0xff);
		break;
	default:
		break;
	}
	return;

dump_as_string:
	stemp = kzalloc(300, GFP_KERNEL);
	if (!stemp)
		return;
	memset(stemp, 0, 300);
	switch (reg_sel) {
	case SATUR_LUT:
		for (i = 0; i < 31 ; i++) {
			tmp = READ_VPP_REG(SRSHARP1_LC_SAT_LUT_0_1 + i);
			tmp1 = (tmp >> 16) & 0xfff;
			tmp2 = tmp & 0xfff;
			lut_data[2 * i] = tmp1;
			lut_data[2 * i + 1] = tmp2;
		}
		tmp = READ_VPP_REG(SRSHARP1_LC_SAT_LUT_62);
		lut_data[62] = tmp & 0xfff;
		for (i = 0; i < 63 ; i++)
			d_convert_str(lut_data[i],
						i, stemp, 4, 10);
		memcpy(buf, stemp, 300);
		break;
	case YMINVAL_LMT:
		for (i = 0; i < 6 ; i++) {
			tmp = READ_VPP_REG(LC_CURVE_YMINVAL_LMT_0_1 + i);
			tmp1 = (tmp >> 16) & 0x3ff;
			tmp2 = tmp & 0x3ff;
			lut_data[2 * i] = tmp1;
			lut_data[2 * i + 1] = tmp2;
		}
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TM2)) {
			len = 16;
			for (j = 0; j < 2 ; j++) {
				tmp =
				READ_VPP_REG(LC_CURVE_YMINVAL_LMT_12_13 + j);
				tmp1 = (tmp >> 16) & 0x3ff;
				tmp2 = tmp & 0x3ff;
				lut_data[2 * i] = tmp1;
				lut_data[2 * i + 1] = tmp2;
				i++;
			}
		}
		for (i = 0; i < len ; i++)
			d_convert_str(lut_data[i],
						i, stemp, 4, 10);
		memcpy(buf, stemp, 300);
		break;
	case YPKBV_YMAXVAL_LMT:
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TM2))
			break;

		for (i = 0; i < 6 ; i++) {
			tmp = READ_VPP_REG(LC_CURVE_YPKBV_YMAXVAL_LMT_0_1 + i);
			tmp1 = (tmp >> 16) & 0x3ff;
			tmp2 = tmp & 0x3ff;
			lut_data[2 * i] = tmp1;
			lut_data[2 * i + 1] = tmp2;
		}
		for (i = 0; i < 12 ; i++)
			d_convert_str(lut_data[i],
				      i, stemp, 4, 10);
		memcpy(buf, stemp, 300);
		break;
	case YMAXVAL_LMT:
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TM2)) {
			for (i = 0; i < 6 ; i++) {
				tmp =
				READ_VPP_REG(LC_CURVE_YMAXVAL_LMT_0_1 + i);
				tmp1 = (tmp >> 16) & 0x3ff;
				tmp2 = tmp & 0x3ff;
				lut_data[2 * i] = tmp1;
				lut_data[2 * i + 1] = tmp2;
			}

			len = 16;
			for (j = 0; j < 2 ; j++) {
				tmp =
				READ_VPP_REG(LC_CURVE_YMAXVAL_LMT_12_13 + j);
				tmp1 = (tmp >> 16) & 0x3ff;
				tmp2 = tmp & 0x3ff;
				lut_data[2 * i] = tmp1;
				lut_data[2 * i + 1] = tmp2;
				i++;
			}
		}
		for (i = 0; i < len ; i++)
			d_convert_str(lut_data[i],
				      i, stemp, 4, 10);
		memcpy(buf, stemp, 300);
		break;
	case YPKBV_LMT:
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TM2)) {
			for (i = 0; i < 8 ; i++) {
				tmp = READ_VPP_REG(LC_CURVE_YPKBV_LMT_0_1 + i);
				tmp1 = (tmp >> 16) & 0x3ff;
				tmp2 = tmp & 0x3ff;
				lut_data[2 * i] = tmp1;
				lut_data[2 * i + 1] = tmp2;
			}
			for (i = 0; i < 16 ; i++)
				d_convert_str(lut_data[i],
					      i, stemp, 4, 10);
			memcpy(buf, stemp, 300);
		}
		break;
	case YPKBV_RAT:
		tmp = READ_VPP_REG(LC_CURVE_YPKBV_RAT);
		lut_data[0] = (tmp >> 24) & 0xff;
		lut_data[1] = (tmp >> 16) & 0xff;
		lut_data[2] = (tmp >> 8) & 0xff;
		lut_data[3] = tmp & 0xff;
		for (i = 0; i < 4 ; i++)
			d_convert_str(lut_data[i],
						i, stemp, 4, 10);
		memcpy(buf, stemp, 300);
		break;
	default:
		break;
	}
	kfree(stemp);
}

static void lc_wr_reg(int *p, enum lc_reg_lut_e reg_sel)
{
	int i, j, tmp, tmp1, tmp2;

	switch (reg_sel) {
	case SATUR_LUT:
		for (i = 0; i < 31 ; i++) {
			tmp1 = *(p + 2 * i);
			tmp2 = *(p + 2 * i + 1);
			tmp = ((tmp1 & 0xfff) << 16) | (tmp2 & 0xfff);
			WRITE_VPP_REG(SRSHARP1_LC_SAT_LUT_0_1 + i, tmp);
		}
		tmp = (*(p + 62)) & 0xfff;
		WRITE_VPP_REG(SRSHARP1_LC_SAT_LUT_62, tmp);
		break;
	case YMINVAL_LMT:
		for (i = 0; i < 6 ; i++) {
			tmp1 = *(p + 2 * i);
			tmp2 = *(p + 2 * i + 1);
			tmp = ((tmp1 & 0x3ff) << 16) | (tmp2 & 0x3ff);
			WRITE_VPP_REG(LC_CURVE_YMINVAL_LMT_0_1 + i, tmp);
		}
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TM2)) {
			for (j = 0; j < 2 ; j++) {
				tmp1 = *(p + 2 * i);
				tmp2 = *(p + 2 * i + 1);
				tmp = ((tmp1 & 0x3ff) << 16) | (tmp2 & 0x3ff);
				WRITE_VPP_REG(LC_CURVE_YMINVAL_LMT_12_13 + j,
					      tmp);
				i++;
			}
		}
		break;
	case YPKBV_YMAXVAL_LMT:
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TM2))
			break;

		for (i = 0; i < 6 ; i++) {
			tmp1 = *(p + 2 * i);
			tmp2 = *(p + 2 * i + 1);
			tmp = ((tmp1 & 0x3ff) << 16) | (tmp2 & 0x3ff);
			WRITE_VPP_REG(LC_CURVE_YPKBV_YMAXVAL_LMT_0_1 + i, tmp);
		}
		break;
	case YMAXVAL_LMT:
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TM2)) {
			for (i = 0; i < 6 ; i++) {
				tmp1 = *(p + 2 * i);
				tmp2 = *(p + 2 * i + 1);
				tmp = ((tmp1 & 0x3ff) << 16) | (tmp2 & 0x3ff);
				WRITE_VPP_REG(LC_CURVE_YMAXVAL_LMT_0_1 + i,
					      tmp);
			}

			for (j = 0; j < 2 ; j++) {
				tmp1 = *(p + 2 * i);
				tmp2 = *(p + 2 * i + 1);
				tmp = ((tmp1 & 0x3ff) << 16) | (tmp2 & 0x3ff);
				WRITE_VPP_REG(LC_CURVE_YMAXVAL_LMT_12_13 + j,
					      tmp);
				i++;
			}
		}
		break;
	case YPKBV_LMT:
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TM2)) {
			for (i = 0; i < 8 ; i++) {
				tmp1 = *(p + 2 * i);
				tmp2 = *(p + 2 * i + 1);
				tmp = ((tmp1 & 0x3ff) << 16) | (tmp2 & 0x3ff);
				WRITE_VPP_REG(LC_CURVE_YPKBV_LMT_0_1 + i,
					      tmp);
			}
		}
		break;
	case YPKBV_RAT:
		tmp = (*p & 0xff) << 24 | (*(p + 1) & 0xff) << 16 |
			(*(p + 2) & 0xff) << 8 | (*(p + 3) & 0xff);
		WRITE_VPP_REG(LC_CURVE_YPKBV_RAT, tmp);
		break;
	case YPKBV_SLP_LMT:
		tmp = (*p & 0xff) << 8 | (*(p + 1) & 0xff);
		WRITE_VPP_REG(LC_CURVE_YPKBV_SLP_LMT, tmp);
		break;
	case CNTST_LMT:
		tmp = (*p & 0xff) << 24 | (*(p + 1) & 0xff) << 16 |
			(*(p + 2) & 0xff) << 8 | (*(p + 3) & 0xff);
		WRITE_VPP_REG(LC_CURVE_CONTRAST__LMT_LH, tmp);
		break;
	default:
		break;
	}
}

void lc_load_curve(struct ve_lc_curve_parm_s *p)
{
	/*load lc parms*/
	lc_alg_parm.dbg_parm0 = p->param[lc_dbg_parm0];
	lc_alg_parm.dbg_parm1 = p->param[lc_dbg_parm1];
	lc_alg_parm.dbg_parm2 = p->param[lc_dbg_parm2];
	lc_alg_parm.dbg_parm3 = p->param[lc_dbg_parm3];
	lc_alg_parm.dbg_parm4 = p->param[lc_dbg_parm4];

	/*load lc_saturation curve*/
	lc_wr_reg(p->ve_lc_saturation, 0x1);
	/*load lc_yminval_lmt*/
	lc_wr_reg(p->ve_lc_yminval_lmt, 0x2);
	/*load lc_ypkbv_ymaxval_lmt*/
	lc_wr_reg(p->ve_lc_ypkbv_ymaxval_lmt, 0x4);
	/*load lc_ypkbV_ratio*/
	lc_wr_reg(p->ve_lc_ypkbv_ratio, 0x8);
	/*load lc_ymaxval_ratio*/
	lc_wr_reg(p->ve_lc_ymaxval_lmt, 0x10);
	/*load lc_ypkbV_ratio*/
	lc_wr_reg(p->ve_lc_ypkbv_lmt, 0x20);
}

static int lc_dbg_flag;
static enum lc_reg_lut_e reg_sel;
static int lc_temp;
static char lc_dbg_curve[100];
static ssize_t amvecm_lc_show(struct class *cla,
			      struct class_attribute *attr, char *buf)
{
	ssize_t len = 0;
	char *temp_cur;

	if (lc_dbg_flag & LC_CUR_RD_UPDATE) {
		temp_cur = kmalloc(300, GFP_KERNEL);
		if (!temp_cur)
			return len;
		memset(temp_cur, 0, 300);

		switch (reg_sel) {
		case SATUR_LUT:
			lc_rd_reg(SATUR_LUT, 1, temp_cur);
			break;
		case YMINVAL_LMT:
			lc_rd_reg(YMINVAL_LMT, 1, temp_cur);
			break;
		case YPKBV_YMAXVAL_LMT:
			lc_rd_reg(YPKBV_YMAXVAL_LMT, 1, temp_cur);
			break;
		case YMAXVAL_LMT:
			lc_rd_reg(YMAXVAL_LMT, 1, temp_cur);
			break;
		case YPKBV_LMT:
			lc_rd_reg(YPKBV_LMT, 1, temp_cur);
			break;
		case YPKBV_RAT:
			lc_rd_reg(YPKBV_RAT, 1, temp_cur);
			break;
		default:
			pr_info("unsupport cmd!\n");
			break;
		}
		lc_dbg_flag &= ~LC_CUR_RD_UPDATE;
		reg_sel = MAX_REG_LUT;
		len = sprintf(buf, "%s\n", temp_cur);
		kfree(temp_cur);
		return len;
	}

	if (lc_dbg_flag & LC_PARAM_RD_UPDATE) {
		lc_dbg_flag &= ~LC_PARAM_RD_UPDATE;
		return sprintf(buf, "%d\n", lc_temp);
	}

	if (lc_dbg_flag & LC_CUR2_RD_UPDATE) {
		lc_dbg_flag &= ~LC_CUR2_RD_UPDATE;
		return sprintf(buf, "%s\n", lc_dbg_curve);
	}

	len += sprintf(buf + len,
		"echo lc enable > /sys/class/amvecm/lc\n");
	len += sprintf(buf + len,
		"echo lc disable > /sys/class/amvecm/lc\n");
	len += sprintf(buf + len,
		"echo lc_dbg value > /sys/class/amvecm/lc\n");
	len += sprintf(buf + len,
		"echo lc_demo_mode enable/disable > /sys/class/amvecm/lc\n");
	len += sprintf(buf + len,
		"echo lc_dump_reg parm1 > /sys/class/amvecm/lc\n");
	len += sprintf(buf + len,
		"echo lc_rd_reg parm1 parm2 > /sys/class/amvecm/lc\n");
	len += sprintf(buf + len,
		"parm1: 0x1,0x2,0x4,0x8,0x10,0x20...\n");
	len += sprintf(buf + len,
		"parm2: decimal strings, each data width is 4.\n");
	len += sprintf(buf + len,
		"echo dump_hist all > /sys/class/amvecm/lc\n");
	len += sprintf(buf + len,
		"echo dump_hist chosen hs he vs ve cnt > /sys/class/amvecm/lc\n");
	len += sprintf(buf + len,
		"echo dump_curve cnt > /sys/class/amvecm/lc\n");
	len += sprintf(buf + len,
		"echo lc_osd_setting show > /sys/class/amvecm/lc\n");
	len += sprintf(buf + len,
		"echo lc_osd_setting set xxx ... xxx > /sys/class/amvecm/lc\n");
	len += sprintf(buf + len,
		"echo osd_iir_en val > /sys/class/amvecm/lc\n");
	len += sprintf(buf + len,
		"echo iir_refresh show > /sys/class/amvecm/lc\n");
	len += sprintf(buf + len,
		"echo iir_refresh set xxx ... xxx > /sys/class/amvecm/lc\n");
	len += sprintf(buf + len,
		"echo scene_change_th val > /sys/class/amvecm/lc\n");
	len += sprintf(buf + len,
		"echo irr_dbg_en val > /sys/class/amvecm/lc\n");
	return len;
}

static ssize_t amvecm_lc_store(struct class *cls,
			       struct class_attribute *attr,
		 const char *buf, size_t count)
{
	char *buf_orig, *parm[8] = {NULL};
	int reg_lut[63] = {0};
	int h, v, i, start_point;
	long val = 0;
	char *stemp = NULL;
	int curve_val[6] = {0};

	if (!buf)
		return count;
	stemp = kzalloc(100, GFP_KERNEL);
	if (!stemp)
		return 0;

	memset(stemp, 0, 100);
	memset(lc_dbg_curve, 0, sizeof(char) * 100);
	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig) {
		kfree(stemp);
		return -ENOMEM;
	}
	parse_param_amvecm(buf_orig, (char **)&parm);

	if (!strcmp(parm[0], "lc")) {
		if (!strcmp(parm[1], "enable"))
			lc_en = 1;
		else if (!strcmp(parm[1], "disable"))
			lc_en = 0;
		else
			pr_info("unsupport cmd!\n");
	} else if (!strcmp(parm[0], "lc_version")) {
		pr_info("lc driver version :  %s\n", LC_VER);
	} else if (!strcmp(parm[0], "lc_dbg")) {
		if (kstrtoul(parm[1], 16, &val) < 0)
			goto free_buf;
		amlc_debug = val;
	} else if (!strcmp(parm[0], "lc_curve_isr")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto free_buf;
		use_lc_curve_isr = val;
	} else if (!strcmp(parm[0], "lc_demo_mode")) {
		if (!strcmp(parm[1], "enable"))
			lc_demo_mode = 1;
		else if (!strcmp(parm[1], "disable"))
			lc_demo_mode = 0;
		else
			pr_info("unsupport cmd!\n");
	} else if (!strcmp(parm[0], "dump_lut_data")) {
		if (kstrtoul(parm[1], 16, &val) < 0)
			goto free_buf;
		reg_sel = val;
		if (reg_sel == SATUR_LUT)
			lc_rd_reg(SATUR_LUT, 0, NULL);
		else if (reg_sel == YMINVAL_LMT)
			lc_rd_reg(YMINVAL_LMT, 0, NULL);
		else if (reg_sel == YPKBV_YMAXVAL_LMT)
			lc_rd_reg(YPKBV_YMAXVAL_LMT, 0, NULL);
		else if (reg_sel == YMAXVAL_LMT)
			lc_rd_reg(YMAXVAL_LMT, 0, NULL);
		else if (reg_sel == YPKBV_LMT)
			lc_rd_reg(YPKBV_LMT, 0, NULL);
		else if (reg_sel == YPKBV_RAT)
			lc_rd_reg(YPKBV_RAT, 0, NULL);
		else if (reg_sel == YPKBV_SLP_LMT)
			lc_rd_reg(YPKBV_SLP_LMT, 0, NULL);
		else if (reg_sel == CNTST_LMT)
			lc_rd_reg(CNTST_LMT, 0, NULL);
		else
			pr_info("unsupport cmd!\n");
	} else if (!strcmp(parm[0], "dump_lut_str")) {
		if (kstrtoul(parm[1], 16, &val) < 0)
			goto free_buf;
		reg_sel = val;
		lc_dbg_flag |= LC_CUR_RD_UPDATE;
	} else if (!strcmp(parm[0], "lc_wr_lut")) {
		if (kstrtoul(parm[1], 16, &val) < 0)
			goto free_buf;
		reg_sel = val;
		if (!parm[2])
			goto free_buf;
		str_sapr_to_d(parm[2], reg_lut, 5);
		if (reg_sel == SATUR_LUT)
			lc_wr_reg(reg_lut, SATUR_LUT);
		else if (reg_sel == YMINVAL_LMT)
			lc_wr_reg(reg_lut, YMINVAL_LMT);
		else if (reg_sel == YPKBV_YMAXVAL_LMT)
			lc_wr_reg(reg_lut, YPKBV_YMAXVAL_LMT);
		else if (reg_sel == YMAXVAL_LMT)
			lc_wr_reg(reg_lut, YMAXVAL_LMT);
		else if (reg_sel == YPKBV_LMT)
			lc_wr_reg(reg_lut, YPKBV_LMT);
		else if (reg_sel == YPKBV_RAT)
			lc_wr_reg(reg_lut, YPKBV_RAT);
		else if (reg_sel == YPKBV_SLP_LMT)
			lc_wr_reg(reg_lut, YPKBV_SLP_LMT);
		else if (reg_sel == CNTST_LMT)
			lc_wr_reg(reg_lut, CNTST_LMT);
		else
			pr_info("unsupport cmd!\n");
	} else if (!strcmp(parm[0], "dump_hist")) {
		if (!strcmp(parm[1], "all")) {
			/*dump all hist of one frame*/
			lc_hist_prcnt = 1;
			amlc_debug = 0x8;
		} else if (!strcmp(parm[1], "chosen")) {
			/*dump multiple frames in selected area*/
			if (!parm[6])
				goto free_buf;
			if (kstrtoul(parm[2], 10, &val) < 0)
				goto free_buf;
			lc_hist_hs = val;
			if (kstrtoul(parm[3], 10, &val) < 0)
				goto free_buf;
			lc_hist_he = val;
			if (kstrtoul(parm[4], 10, &val) < 0)
				goto free_buf;
			lc_hist_vs = val;
			if (kstrtoul(parm[5], 10, &val) < 0)
				goto free_buf;
			lc_hist_ve = val;
			if (kstrtoul(parm[6], 10, &val) < 0)
				goto free_buf;
			lc_hist_prcnt = val;
			amlc_debug = 0x6;
		} else {
			pr_info("unsupport cmd!\n");
		}
	} else if (!strcmp(parm[0], "dump_curve")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto free_buf;
		lc_curve_prcnt = val;
	} else if (!strcmp(parm[0], "lc_osd_setting")) {
		if (!strcmp(parm[1], "show")) {
			pr_info("VNUM_STRT_BELOW: %d, VNUM_END_BELOW: %d\n",
				vnum_start_below, vnum_end_below);
			pr_info("VNUM_STRT_ABOVE: %d, VNUM_END_ABOVE: %d\n",
				vnum_start_above, vnum_end_above);
			pr_info("INVALID_BLK: %d, MIN_BV_PERCENT_TH: %d\n",
				invalid_blk, min_bv_percent_th);
		} else if (!strcmp(parm[1], "set")) {
			if (!parm[7])
				goto free_buf;
			if (kstrtoul(parm[2], 10, &val) < 0)
				goto free_buf;
			vnum_start_below = val;
			if (kstrtoul(parm[3], 10, &val) < 0)
				goto free_buf;
			vnum_end_below = val;
			if (kstrtoul(parm[4], 10, &val) < 0)
				goto free_buf;
			vnum_start_above = val;
			if (kstrtoul(parm[5], 10, &val) < 0)
				goto free_buf;
			vnum_end_above = val;
			if (kstrtoul(parm[6], 10, &val) < 0)
				goto free_buf;
			invalid_blk = val;
			if (kstrtoul(parm[7], 10, &val) < 0)
				goto free_buf;
			min_bv_percent_th = val;
		} else {
			pr_info("unsupport cmd!\n");
		}
	} else if (!strcmp(parm[0], "osd_iir_en")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto free_buf;
		osd_iir_en = val;
	} else if (!strcmp(parm[0], "iir_refresh")) {
		if (!strcmp(parm[1], "show")) {
			pr_info("current state: alpha1: %d, alpha2: %d, refresh_bit: %d, ts: %d\n",
				alpha1, alpha2, refresh_bit, ts);
		} else if (!strcmp(parm[1], "set")) {
			if (!parm[5])
				goto free_buf;
			if (kstrtoul(parm[2], 10, &val) < 0)
				goto free_buf;
			alpha1 = val;
			if (kstrtoul(parm[3], 10, &val) < 0)
				goto free_buf;
			alpha2 = val;
			if (kstrtoul(parm[4], 10, &val) < 0)
				goto free_buf;
			refresh_bit = val;
			if (kstrtoul(parm[5], 10, &val) < 0)
				goto free_buf;
			ts = val;
			pr_info("after setting: alpha1: %d, alpha2: %d, refresh_bit: %d, ts: %d\n",
				alpha1, alpha2, refresh_bit, ts);
		}  else {
			pr_info("unsupport cmd!\n");
		}
	} else if (!strcmp(parm[0], "scene_change_th")) {
		pr_info("current value: %d\n", scene_change_th);
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto free_buf;
		scene_change_th = val;
		pr_info("setting value: %d\n", scene_change_th);
	} else if (!strcmp(parm[0], "irr_dbg_en")) {
		pr_info("current value: %d\n", amlc_iir_debug_en);
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto free_buf;
		amlc_iir_debug_en = val;
		pr_info("setting value: %d\n", amlc_iir_debug_en);
	} else if (!strcmp(parm[0], "get_blk_region")) {
		val = READ_VPP_REG(LC_CURVE_HV_NUM);
		h = (val >> 8) & 0x1f;
		v = (val) & 0x1f;
		if (!strcmp(parm[1], "htotal")) {
			lc_temp = h;
			lc_dbg_flag |= LC_PARAM_RD_UPDATE;
		} else if (!strcmp(parm[1], "vtotal")) {
			lc_temp = v;
			lc_dbg_flag |= LC_PARAM_RD_UPDATE;
		} else
			pr_info("unsupport cmd!\n");
	} else if (!strcmp(parm[0], "get_hist")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto free_buf;
		h = val;
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto free_buf;
		v = val;
		if (h > 11 || v > 7)
			goto free_buf;
		start_point = (12 * v + h) * 17;
		for (i = 0; i < 17; i++)
			d_convert_str(lc_hist[start_point + i] >> 4,
						i, stemp, 4, 10);
		memcpy(lc_dbg_curve, stemp, 100);
		lc_dbg_flag |= LC_CUR2_RD_UPDATE;
	} else if (!strcmp(parm[0], "get_curve")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto free_buf;
		h = val;
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto free_buf;
		v = val;
		if (h > 11 || v > 7)
			goto free_buf;
		start_point = (12 * v + h) * 6;
		for (i = 0; i < 6; i++)
			d_convert_str(curve_nodes_cur[start_point + i],
						i, stemp, 4, 10);
		memcpy(lc_dbg_curve, stemp, 100);
		lc_dbg_flag |= LC_CUR2_RD_UPDATE;
	} else if (!strcmp(parm[0], "set_curve")) {
		if (!parm[3])
			goto free_buf;
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto free_buf;
		h = val;
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto free_buf;
		v = val;
		if (h > 11 || v > 7)
			goto free_buf;
		start_point = (12 * v + h) * 6;
		str_sapr_to_d(parm[3], curve_val, 5);
		for (i = 0; i < 6; i++)
			curve_nodes_cur[start_point + i] =
			curve_val[i];
	} else if (!strcmp(parm[0], "stop_refresh")) {
		lc_curve_fresh = false;
	} else if (!strcmp(parm[0], "refresh_curve")) {
		lc_curve_fresh = true;
	} else if (!strcmp(parm[0], "reg_lmtrat_sigbin")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto free_buf;
		lc_tune_curve.lc_reg_lmtrat_sigbin = val;
		pr_info("reg_lmtrat_sigbin = %d\n",
			lc_tune_curve.lc_reg_lmtrat_sigbin);
	} else if (!strcmp(parm[0], "reg_lmtrat_thd_max")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto free_buf;
		lc_tune_curve.lc_reg_lmtrat_thd_max = val;
		pr_info("reg_lmtrat_thd_max = %d\n",
			lc_tune_curve.lc_reg_lmtrat_thd_max);
	} else if (!strcmp(parm[0], "reg_lmtrat_thd_black")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto free_buf;
		lc_tune_curve.lc_reg_lmtrat_thd_black = val;
		pr_info("reg_lmtrat_thd_black = %d\n",
			lc_tune_curve.lc_reg_lmtrat_thd_black);
	} else if (!strcmp(parm[0], "reg_thd_black")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto free_buf;
		lc_tune_curve.lc_reg_thd_black = val;
		WRITE_VPP_REG_BITS(LC_STTS_BLACK_INFO,
				   lc_tune_curve.lc_reg_thd_black, 0, 8);
		pr_info("reg_thd_black = %d\n",
			lc_tune_curve.lc_reg_thd_black);
	} else if (!strcmp(parm[0], "ypkBV_black_thd")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto free_buf;
		lc_tune_curve.ypkbv_black_thd = val;
		pr_info("ypkBV_black_thd = %d\n",
			lc_tune_curve.ypkbv_black_thd);
	} else if (!strcmp(parm[0], "yminV_black_thd")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto free_buf;
		lc_tune_curve.yminv_black_thd = val;
		pr_info("yminV_black_thd = %d\n",
			lc_tune_curve.yminv_black_thd);
	} else if (!strcmp(parm[0], "tune_curve_debug")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto free_buf;
		lc_node_pos_v = val;
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto free_buf;
		lc_node_pos_h = val;
		pr_info("tune_curve_debug pos = v %d h %d\n",
			lc_node_pos_v, lc_node_pos_h);
	} else if (!strcmp(parm[0], "tune_curve_en")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto free_buf;
		tune_curve_en = val;
		amlc_debug = 0xc;
		lc_node_prcnt = 10;
		pr_info("tune_curve_en = %d\n", tune_curve_en);
	} else if (!strcmp(parm[0], "detect_signal_range_en")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto free_buf;
		amlc_debug = 0xe;
		detect_signal_range_en = val;
		pr_info("detect_signal_range_en = %d\n",
			detect_signal_range_en);
	} else if (!strcmp(parm[0], "detect_signal_range_threshold")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto free_buf;
		detect_signal_range_threshold_black = val;
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto free_buf;
		detect_signal_range_threshold_white = val;
		amlc_debug = 0xe;
		pr_info("detect_signal_range_threshold = %d %d\n",
			detect_signal_range_threshold_black,
			detect_signal_range_threshold_white);
	} else {
		pr_info("unsupport cmd!\n");
	}

	kfree(buf_orig);
	kfree(stemp);
	return count;

free_buf:
	pr_info("Missing parameters !\n");
	kfree(buf_orig);
	kfree(stemp);
	return -EINVAL;
}

static void def_hdr_sdr_mode(void)
{
	if (((READ_VPP_REG(VD1_HDR2_CTRL) >> 13) & 0x1) &&
	    ((READ_VPP_REG(OSD1_HDR2_CTRL) >> 13) & 0x1))
		sdr_mode = 2;
}

void hdr_hist_config_int(void)
{
	WRITE_VPP_REG(VD1_HDR2_HIST_CTRL, 0x5510);
	WRITE_VPP_REG(VD1_HDR2_HIST_H_START_END, 0x10000);
	WRITE_VPP_REG(VD1_HDR2_HIST_V_START_END, 0x0);

	if (get_cpu_type() != MESON_CPU_MAJOR_ID_T5 &&
	    get_cpu_type() != MESON_CPU_MAJOR_ID_T5D) {
		WRITE_VPP_REG(VD2_HDR2_HIST_CTRL, 0x5510);
		WRITE_VPP_REG(VD2_HDR2_HIST_H_START_END, 0x10000);
		WRITE_VPP_REG(VD2_HDR2_HIST_V_START_END, 0x0);

		WRITE_VPP_REG(OSD1_HDR2_HIST_CTRL, 0x5510);
		WRITE_VPP_REG(OSD1_HDR2_HIST_H_START_END, 0x10000);
		WRITE_VPP_REG(OSD1_HDR2_HIST_V_START_END, 0x0);
	}
}

#define PQ_TV 1
#define PQ_BOX 0
void init_pq_control(unsigned int enable)
{
	if (enable) {
		memcpy(&pq_cfg, &pq_cfg_init[TV_CFG_DEF],
		       sizeof(struct pq_ctrl_s));
		memcpy(&dv_cfg_bypass, &pq_cfg_init[TV_DV_BYPASS],
		       sizeof(struct pq_ctrl_s));
	} else {
		memcpy(&pq_cfg, &pq_cfg_init[OTT_CFG_DEF],
		       sizeof(struct pq_ctrl_s));
		memcpy(&dv_cfg_bypass, &pq_cfg_init[OTT_DV_BYPASS],
		       sizeof(struct pq_ctrl_s));
	}
}

int vinfo_lcd_support(void)
{
	struct vinfo_s *vinfo = get_current_vinfo();

	if (vinfo->mode == VMODE_LCD ||
		vinfo->mode == VMODE_DUMMY_ENCP)
		return 1;
	else
		return 0;
}

/*for s5 dsc enable, vpp convert yuv2rgb when hdmi rgb out
 *ret: 0 yuv, 1 rgb
 */
int vinfo_hdmi_out_fmt(void)
{
	struct vinfo_s *vinfo = get_current_vinfo();

	if (chip_type_id != chip_s5)
		return 0;

	if ((vinfo->mode & VMODE_MODE_BIT_MASK) == VMODE_HDMI &&
		vinfo->vpp_post_out_color_fmt)
		return 1;
	else
		return 0;
}

/* #if (MESON_CPU_TYPE == MESON_CPU_TYPE_MESONG9TV) */
void init_pq_setting(void)
{
	struct vinfo_s *vinfo = get_current_vinfo();

	int bitdepth;

	if (vinfo_lcd_support())
		init_pq_control(PQ_TV);
	else
		init_pq_control(PQ_BOX);

	if (get_cpu_type() == MESON_CPU_MAJOR_ID_SC2)
		init_pq_control(PQ_BOX);

	if (chip_type_id == chip_a4) {
		vpp_pq_ctrl_config(pq_cfg, WR_VCB);
		pq_reg_wr_rdma = 1;
		return;
	}

	/*ai pq interface*/
	ai_detect_scene_init();
	adaptive_param_init();

	if (is_meson_gxtvbb_cpu() || is_meson_txl_cpu() ||
	    is_meson_txlx_cpu() || is_meson_txhd_cpu() ||
		is_meson_tl1_cpu() || is_meson_tm2_cpu() ||
		get_cpu_type() == MESON_CPU_MAJOR_ID_T5 ||
		get_cpu_type() == MESON_CPU_MAJOR_ID_T5D ||
		get_cpu_type() == MESON_CPU_MAJOR_ID_T7 ||
		get_cpu_type() == MESON_CPU_MAJOR_ID_T3 ||
		get_cpu_type() == MESON_CPU_MAJOR_ID_T5W)
		goto tvchip_pq_setting;
	else if (is_meson_g12a_cpu() || is_meson_g12b_cpu() ||
		 is_meson_sm1_cpu() ||
		 get_cpu_type() == MESON_CPU_MAJOR_ID_SC2 ||
		 is_meson_s4_cpu() ||
		 is_meson_s4d_cpu()) {
		if (is_meson_s4_cpu())
			bitdepth = 10;
		else
			bitdepth = 12;
		/*confirm with vlsi-Lunhai.Chen, for G12A/G12B,
		 *VPP_GCLK_CTRL1 must enable
		 */
		WRITE_VPP_REG_BITS(VPP_GCLK_CTRL1, 0xf, 0, 4);
		sr_init_config();
		dnlp_init_config();
		cm_init_config(bitdepth);
		/*dnlp off*/
		WRITE_VPP_REG_BITS(VPP_VE_ENABLE_CTRL, 0,
				   DNLP_EN_BIT, DNLP_EN_WID);
		/*sr0  chroma filter bypass*/
		WRITE_VPP_REG(SRSHARP0_SHARP_SR2_CBIC_HCOEF0,
			      0x4000);
		WRITE_VPP_REG(SRSHARP0_SHARP_SR2_CBIC_VCOEF0,
			      0x4000);

		/*kernel sdr2hdr match uboot setting*/
		def_hdr_sdr_mode();
		vpp_pq_ctrl_config(pq_cfg, WR_VCB);

		pq_reg_wr_rdma = 1;
	} else if (chip_type_id == chip_s5) {
		bitdepth = 12;
		cm_top_ctl(WR_VCB, 1);
		cm_init_config(bitdepth);
		vpp_pq_ctrl_config(pq_cfg, WR_VCB);

		pq_reg_wr_rdma = 1;
	}
	return;

tvchip_pq_setting:
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
		if (is_meson_tl1_cpu())
			bitdepth = 10;
		else if (get_cpu_type() == MESON_CPU_MAJOR_ID_T5 ||
			     get_cpu_type() == MESON_CPU_MAJOR_ID_T5D ||
			     get_cpu_type() == MESON_CPU_MAJOR_ID_T3 ||
			     get_cpu_type() == MESON_CPU_MAJOR_ID_T5W)
			bitdepth = 10;
		else if (is_meson_tm2_cpu())
			bitdepth = 12;
		else
			bitdepth = 12;

		sr_init_config();
		dnlp_init_config();
		cm_init_config(bitdepth);
		/*lc init*/
		lc_init(bitdepth);

		/*frequence meter init*/
		if (get_cpu_type() == MESON_CPU_MAJOR_ID_T3 ||
			get_cpu_type() == MESON_CPU_MAJOR_ID_T5W)
			amve_fmeter_init(fmeter_en);

		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TM2))
			hdr_hist_config_int();

		if (cpu_after_eq(MESON_CPU_MAJOR_ID_T7))
			vpp_pst_hist_sta_config(1, HIST_MAXRGB,
				AFTER_POST2_MTX, vinfo);
	}

	/* enable vadj1 by default */
	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A)
		WRITE_VPP_REG_BITS(VPP_VADJ1_MISC, 1, 0, 1);
	else
		WRITE_VPP_REG(VPP_VADJ_CTRL, 0xd);

	/*probe close sr0 peaking for switch on video*/
	WRITE_VPP_REG_BITS(VPP_SRSHARP0_CTRL, 1, 0, 1);
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
		WRITE_VPP_REG_BITS(VPP_SRSHARP1_CTRL, 0, 0, 1);
		/*VPP_VADJ1_MISC bit1: minus black level enable for vadj1*/
		WRITE_VPP_REG_BITS(VPP_VADJ1_MISC, 1, 1, 1);
	} else {
		WRITE_VPP_REG_BITS(VPP_SRSHARP1_CTRL, 1, 0, 1);
	}
	/*default dnlp off*/
	WRITE_VPP_REG_BITS(SRSHARP0_PK_NR_ENABLE,
			   0, 1, 1);
	WRITE_VPP_REG_BITS(SRSHARP1_PK_NR_ENABLE,
			   0, 1, 1);
	WRITE_VPP_REG_BITS(VPP_VE_ENABLE_CTRL, 0, DNLP_EN_BIT, DNLP_EN_WID);
	/*end*/
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXL)) {
		WRITE_VPP_REG_BITS(SRSHARP1_PK_FINALGAIN_HP_BP,
				   2, 16, 2);

		/*sr0 sr1 chroma filter bypass*/
		WRITE_VPP_REG(SRSHARP0_SHARP_SR2_CBIC_HCOEF0,
			      0x4000);
		WRITE_VPP_REG(SRSHARP0_SHARP_SR2_CBIC_VCOEF0,
			      0x4000);
		WRITE_VPP_REG(SRSHARP1_SHARP_SR2_CBIC_HCOEF0,
			      0x4000);
		WRITE_VPP_REG(SRSHARP1_SHARP_SR2_CBIC_VCOEF0,
			      0x4000);
	}
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
	if (is_meson_gxlx_cpu())
		amve_sharpness_init();
#endif

	/*dnlp alg parameters init*/
	dnlp_alg_param_init();
	vpp_pq_ctrl_config(pq_cfg, WR_VCB);

	pq_reg_wr_rdma = 1;
}

/* #endif*/

struct gamma_data_s *get_gm_data(void)
{
	return &amvecm_dev.gm_data;
}

void amvecm_gamma_init(bool en)
{
	unsigned int i, j, k;
	unsigned short data[256];
	unsigned short temp;
	struct gamma_data_s *p_gm;

	if (chip_cls_id == STB_CHIP)
		return;

	if (chip_type_id == chip_a4) {
		p_gm = get_gm_data();
		p_gm->max_idx = 257;
		p_gm->auto_inc = 1 << L_H_AUTO_INC_2;
		p_gm->addr_port = LCD_GAMMA_ADDR_PORT0_A4;
		p_gm->data_port = LCD_GAMMA_DATA_PORT0_A4;
		for (i = 0; i < p_gm->max_idx; i++) {
			temp = i << 2;
			if (temp >= (1 << 10))
				temp = (1 << 10) - 1;
			p_gm->gm_tbl.gamma_r[i] = temp;
			p_gm->gm_tbl.gamma_g[i] = temp;
			p_gm->gm_tbl.gamma_b[i] = temp;
		}
		lcd_gamma_api(0, p_gm->gm_tbl.gamma_r,
				p_gm->gm_tbl.gamma_g,
				p_gm->gm_tbl.gamma_b,
				0, 0);
		return;
	}

	for (i = 0; i < 256; i++) {
		data[i] = i << 2;
		video_gamma_table_r.data[i] = data[i];
		video_gamma_table_g.data[i] = data[i];
		video_gamma_table_b.data[i] = data[i];

		for (k = 0; k < FREESYNC_DYNAMIC_GAMMA_NUM; k++)
			for (j = 0; j < FREESYNC_DYNAMIC_GAMMA_CHANNEL; j++)
				gt.gm_tb[k][j].data[i] = data[i];
	}

	if (cpu_after_eq_t7()) {
		if (is_meson_t7_cpu()) {
			vecm_latch_flag |= FLAG_GAMMA_TABLE_R;
			vecm_latch_flag |= FLAG_GAMMA_TABLE_G;
			vecm_latch_flag |= FLAG_GAMMA_TABLE_B;
		} else {
			lcd_gamma_api(0, video_gamma_table_r.data,
				video_gamma_table_g.data,
				video_gamma_table_b.data,
				0, 0);
		}
	} else {
		vpp_disable_lcd_gamma_table(0, 0);
		WRITE_VPP_REG_BITS(L_GAMMA_CNTL_PORT,
				0, GAMMA_EN, 1);
		amve_write_gamma_table(data,
					H_SEL_R);
		amve_write_gamma_table(data,
					H_SEL_G);
		amve_write_gamma_table(data,
					H_SEL_B);
	}

	if (en)
		vecm_latch_flag |= FLAG_GAMMA_TABLE_EN;
	else
		vecm_latch_flag |= FLAG_GAMMA_TABLE_DIS;
}

static void amvecm_wb_init(bool en)
{
	int *initcoef;

	initcoef = wb_init_bypass_coef;

	if (chip_type_id == chip_s5)
		return;

	if (chip_type_id == chip_a4) {
		WRITE_VPP_REG(VOUT_GAINOFF_CTRL0,
					(en << 31) | (1024 << 16) | 1024);
		WRITE_VPP_REG(VOUT_GAINOFF_CTRL1,
					(1024 << 16));
		return;
	}

	if (video_rgb_ogo_xvy_mtx) {
		WRITE_VPP_REG_BITS(VPP_MATRIX_CTRL, 3, 8, 3);

		WRITE_VPP_REG(VPP_MATRIX_PRE_OFFSET0_1,
					((initcoef[0] & 0xfff) << 16)
					| (initcoef[1] & 0xfff));
		WRITE_VPP_REG(VPP_MATRIX_PRE_OFFSET2,
					initcoef[2] & 0xfff);
		WRITE_VPP_REG(VPP_MATRIX_COEF00_01,
					((initcoef[3] & 0x1fff) << 16)
					| (initcoef[4] & 0x1fff));
		WRITE_VPP_REG(VPP_MATRIX_COEF02_10,
					((initcoef[5]  & 0x1fff) << 16)
					| (initcoef[6] & 0x1fff));
		WRITE_VPP_REG(VPP_MATRIX_COEF11_12,
					((initcoef[7] & 0x1fff) << 16)
					| (initcoef[8] & 0x1fff));
		WRITE_VPP_REG(VPP_MATRIX_COEF20_21,
					((initcoef[9] & 0x1fff) << 16)
					| (initcoef[10] & 0x1fff));
		WRITE_VPP_REG(VPP_MATRIX_COEF22,
					initcoef[11] & 0x1fff);
		if (initcoef[21]) {
			WRITE_VPP_REG(VPP_MATRIX_COEF13_14,
						((initcoef[12] & 0x1fff) << 16)
						| (initcoef[13] & 0x1fff));
			WRITE_VPP_REG(VPP_MATRIX_COEF15_25,
						((initcoef[14] & 0x1fff) << 16)
						| (initcoef[17] & 0x1fff));
			WRITE_VPP_REG(VPP_MATRIX_COEF23_24,
						((initcoef[15] & 0x1fff) << 16)
						| (initcoef[16] & 0x1fff));
		}
		WRITE_VPP_REG(VPP_MATRIX_OFFSET0_1,
					((initcoef[18] & 0xfff) << 16)
					| (initcoef[19] & 0xfff));
		WRITE_VPP_REG(VPP_MATRIX_OFFSET2,
					initcoef[20] & 0xfff);
		WRITE_VPP_REG_BITS(VPP_MATRIX_CLIP,
					initcoef[21], 3, 2);
		WRITE_VPP_REG_BITS(VPP_MATRIX_CLIP,
					initcoef[22], 5, 3);

		WRITE_VPP_REG_BITS(VPP_MATRIX_CTRL, en, 6, 1);
	} else {
		WRITE_VPP_REG(VPP_GAINOFF_CTRL0,
					(en << 31) | (1024 << 16) | 1024);
		WRITE_VPP_REG(VPP_GAINOFF_CTRL1,
					(1024 << 16));
	}
}

void amvecm_3dlut_init(bool en)
{
	if (chip_type_id == chip_s5 ||
		chip_type_id == chip_a4)
		return;

	if (ct_en) {
		lut3d_en = 1;
		vpp_lut3d_table_init(-1, -1, -1);
		vpp_set_lut3d(0, 0, 0, 0);
		if (ct_en)
			color_lut_init(ct_en);
		vpp_enable_lut3d(ct_en);
	} else {
		vpp_lut3d_table_init(-1, -1, -1);
		vpp_set_lut3d(0, 0, 0, 0);
		vpp_lut3d_table_release();

		vpp_enable_lut3d(en);
	}
}

static struct class_attribute amvecm_class_attrs[] = {
	__ATTR(debug, 0644,
	       amvecm_debug_show, amvecm_debug_store),
	__ATTR(dnlp_debug, 0644,
	       amvecm_dnlp_debug_show,
		amvecm_dnlp_debug_store),
	__ATTR(cm2_hue, 0644,
	       amvecm_cm2_hue_show,
		amvecm_cm2_hue_store),
	__ATTR(cm2_luma, 0644,
	       amvecm_cm2_luma_show,
		amvecm_cm2_luma_store),
	__ATTR(cm2_sat, 0644,
	       amvecm_cm2_sat_show,
		amvecm_cm2_sat_store),
	__ATTR(cm2_hue_by_hs, 0644,
	       amvecm_cm2_hue_by_hs_show,
		amvecm_cm2_hue_by_hs_store),
	__ATTR(brightness, 0644,
	       amvecm_brightness_show, amvecm_brightness_store),
	__ATTR(contrast, 0644,
	       amvecm_contrast_show, amvecm_contrast_store),
	__ATTR(saturation_hue, 0644,
	       amvecm_saturation_hue_show,
		amvecm_saturation_hue_store),
	__ATTR(saturation_hue_pre, 0644,
	       amvecm_saturation_hue_pre_show,
		amvecm_saturation_hue_pre_store),
	__ATTR(saturation_hue_post, 0644,
	       amvecm_saturation_hue_post_show,
		amvecm_saturation_hue_post_store),
	__ATTR(cm2, 0644,
	       amvecm_cm2_show,
		amvecm_cm2_store),
	__ATTR(cm2_idx, 0644,
	       amvecm_cm2_idx_show,
		amvecm_cm2_idx_store),
	__ATTR(cm_reg, 0644,
	       amvecm_cm_reg_show,
		amvecm_cm_reg_store),
	__ATTR(gamma, 0644,
	       amvecm_gamma_show,
		amvecm_gamma_store),
	__ATTR(gamma_v2, 0644,
	       amvecm_gamma_v2_show,
		amvecm_gamma_v2_store),
	__ATTR(wb, 0644,
	       amvecm_wb_show,
		amvecm_wb_store),
	__ATTR(brightness1, 0644,
	       video_adj1_brightness_show,
		video_adj1_brightness_store),
	__ATTR(contrast1, 0644,
	       video_adj1_contrast_show, video_adj1_contrast_store),
	__ATTR(brightness2, 0644,
	       video_adj2_brightness_show, video_adj2_brightness_store),
	__ATTR(contrast2, 0644,
	       video_adj2_contrast_show, video_adj2_contrast_store),
	__ATTR(help, 0644,
	       amvecm_usage_show, NULL),
/* #if (MESON_CPU_TYPE >= MESON_CPU_TYPE_MESONG9TV) */
	__ATTR(sync_3d, 0644,
	       amvecm_3d_sync_show,
		amvecm_3d_sync_store),
	__ATTR(vlock, 0644,
	       amvecm_vlock_show,
		amvecm_vlock_store),
	__ATTR(matrix_set, 0644,
	       amvecm_set_post_matrix_show, amvecm_set_post_matrix_store),
	__ATTR(matrix_pos, 0644,
	       amvecm_post_matrix_pos_show, amvecm_post_matrix_pos_store),
	__ATTR(matrix_data, 0644,
	       amvecm_post_matrix_data_show, amvecm_post_matrix_data_store),
	__ATTR(dump_reg, 0644,
	       amvecm_dump_reg_show, amvecm_dump_reg_store),
	__ATTR(sr1_reg, 0644,
	       amvecm_sr1_reg_show, amvecm_sr1_reg_store),
	__ATTR(write_sr1_reg_val, 0644,
	       amvecm_write_sr1_reg_val_show, amvecm_write_sr1_reg_val_store),
	__ATTR(dump_vpp_hist, 0644,
	       amvecm_dump_vpp_hist_show, amvecm_dump_vpp_hist_store),
	__ATTR(hdr_dbg, 0644,
	       amvecm_hdr_dbg_show, amvecm_hdr_dbg_store),
	__ATTR(hdr_reg, 0644,
	       amvecm_hdr_reg_show, amvecm_hdr_reg_store),
	__ATTR(hdr_tmo, 0644,
	       amvecm_hdr_tmo_show, amvecm_hdr_tmo_store),
	__ATTR(gamma_pattern, 0644,
	       set_gamma_pattern_show, set_gamma_pattern_store),
	__ATTR(pc_mode, 0644,
	       amvecm_pc_mode_show, amvecm_pc_mode_store),
	__ATTR(set_hdr_289lut, 0644,
	       set_hdr_289lut_show, set_hdr_289lut_store),
	__ATTR(vpp_demo, 0644,
	       amvecm_vpp_demo_show, amvecm_vpp_demo_store),
	__ATTR(reg, 0644,
	       amvecm_reg_show, amvecm_reg_store),
	__ATTR(pq_user_set, 0644,
	       amvecm_pq_user_show, amvecm_pq_user_store),
	__ATTR(pq_reg_rw, 0644,
	       amvecm_write_reg_show, amvecm_write_reg_store),
	__ATTR(get_hdr_type, 0644,
	       amvecm_get_hdr_type_show, amvecm_get_hdr_type_store),
	__ATTR(dnlp_insmod, 0644,
	       amvecm_dnlp_insmod_show, amvecm_dnlp_insmod_store),
	__ATTR(lc, 0644,
	       amvecm_lc_show,
		amvecm_lc_store),
	__ATTR(color_top, 0644,
	       amvecm_clamp_color_top_show, amvecm_clamp_color_top_store),
	__ATTR(color_bottom, 0644,
	       amvecm_clamp_color_bottom_show,
		amvecm_clamp_color_bottom_store),
	__ATTR(cpu_ver, 0644,
	       amvecm_cpu_ver_show,
	       amvecm_cpu_ver_store),
	__ATTR(cabc_aad, 0644,
	       amvecm_cabc_aad_show,
	       amvecm_cabc_aad_store),
	__ATTR(frame_lock, 0644,
	       amvecm_frame_lock_show,
		amvecm_frame_lock_store),
	__ATTR(color_tune, 0664,
		amvecm_color_tune_show,
		amvecm_color_tune_store),
	__ATTR(dma_buf, 0664,
		amvecm_dma_buf_show,
		amvecm_dma_buf_store),
	__ATTR(ble_whe_dbg, 0644,
		amvecm_ble_whe_dbg_show,
		amvecm_ble_whe_dbg_store),
	__ATTR_NULL
};

void amvecm_wakeup_queue(void)
{
	struct amvecm_dev_s *devp = &amvecm_dev;

	wake_up(&devp->hdr_queue);
}

static unsigned int amvecm_poll(struct file *file, poll_table *wait)
{
	struct amvecm_dev_s *devp = file->private_data;
	unsigned int mask = 0;

	poll_wait(file, &devp->hdr_queue, wait);
	mask = (POLLIN | POLLRDNORM);

	return mask;
}

static const struct file_operations amvecm_fops = {
	.owner   = THIS_MODULE,
	.open    = amvecm_open,
	.release = amvecm_release,
	.unlocked_ioctl   = amvecm_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = amvecm_compat_ioctl,
#endif
	.poll = amvecm_poll,
};

static const struct vecm_match_data_s vecm_dt_xxx = {
	.chip_id = chip_other,
	.chip_cls = OTHER_CLS,
	.vlk_chip = vlock_chip_txlx,
	.vlk_support = true,
	.vlk_new_fsm = 0,
	.vlk_hwver = vlock_hw_org,
	.vlk_phlock_en = false,
	.vlk_pll_sel = vlock_pll_sel_tcon,
};

//#ifndef CONFIG_AMLOGIC_REMOVE_OLD
static const struct vecm_match_data_s vecm_dt_tl1 = {
	.chip_id = chip_other,
	.chip_cls = TV_CHIP,
	.vlk_chip = vlock_chip_tl1,
	.vlk_support = true,
	.vlk_new_fsm = 1,
	.vlk_hwver = vlock_hw_ver2,
	.vlk_phlock_en = true,
	.vlk_pll_sel = vlock_pll_sel_tcon,
};

static const struct vecm_match_data_s vecm_dt_sm1 = {
	.chip_id = chip_other,
	.chip_cls = STB_CHIP,
	.vlk_chip = vlock_chip_sm1,
	.vlk_support = false,
	.vlk_new_fsm = 1,
	.vlk_hwver = vlock_hw_ver2,
	.vlk_phlock_en = false,
	.vlk_pll_sel = vlock_pll_sel_tcon,
};

static const struct vecm_match_data_s vecm_dt_tm2 = {
	.chip_id = chip_other,
	.chip_cls = STB_CHIP,
	.vlk_chip = vlock_chip_tm2,
	.vlk_support = true,
	.vlk_new_fsm = 1,
	.vlk_hwver = vlock_hw_ver2,
	.vlk_phlock_en = true,
	.vlk_pll_sel = vlock_pll_sel_tcon,
};

static const struct vecm_match_data_s vecm_dt_tm2_verb = {
	.chip_id = chip_other,
	.chip_cls = STB_CHIP,
	.vlk_chip = vlock_chip_tm2,
	.vlk_support = true,
	.vlk_new_fsm = 1,
	.vlk_hwver = vlock_hw_tm2verb,
	.vlk_phlock_en = true,
	.vlk_pll_sel = vlock_pll_sel_tcon,
};

static const struct vecm_match_data_s vecm_dt_t5 = {
	.chip_id = chip_other,
	.chip_cls = TV_CHIP,
	.vlk_chip = vlock_chip_t5,
	.vlk_support = true,
	.vlk_new_fsm = 1,
	.vlk_hwver = vlock_hw_tm2verb,
	.vlk_phlock_en = true,
	.vlk_pll_sel = vlock_pll_sel_tcon,
};

static const struct vecm_match_data_s vecm_dt_t5d = {
	.chip_id = chip_other,
	.chip_cls = TV_CHIP,
	.vlk_chip = vlock_chip_t5,/*same as t5d*/
	.vlk_support = true,
	.vlk_new_fsm = 1,
	.vlk_hwver = vlock_hw_tm2verb,
	.vlk_phlock_en = true,
	.vlk_pll_sel = vlock_pll_sel_tcon,
};

static const struct vecm_match_data_s vecm_dt_t7 = {
	.chip_id = chip_t7,
	.chip_cls = SMT_CHIP,
	.vlk_chip = vlock_chip_t7,
	.vlk_support = true,
	.vlk_new_fsm = 1,
	.vlk_hwver = vlock_hw_tm2verb,
	.vlk_phlock_en = true,
	.vlk_pll_sel = vlock_pll_sel_tcon,
	.vrr_support_flag = 1,
};

static const struct vecm_match_data_s vecm_dt_t3 = {
	.chip_id = chip_t3,
	.chip_cls = TV_CHIP,
	.vlk_chip = vlock_chip_t3,
	.vlk_support = true,
	.vlk_new_fsm = 1,
	.vlk_hwver = vlock_hw_tm2verb,
	.vlk_phlock_en = true,
	.vlk_pll_sel = vlock_pll_sel_tcon,
	.vlk_ctl_for_frc = 1,
	.vrr_support_flag = 1,
};

/*t5w vlock follow t5 */
static const struct vecm_match_data_s vecm_dt_t5w = {
	.chip_id = chip_t5w,
	.chip_cls = TV_CHIP,
	.vlk_chip = vlock_chip_t5,
	.vlk_support = true,
	.vlk_new_fsm = 1,
	.vlk_hwver = vlock_hw_tm2verb,
	.vlk_phlock_en = true,
	.vlk_pll_sel = vlock_pll_sel_tcon,
	.vrr_support_flag = 1,
};

static const struct vecm_match_data_s vecm_dt_s5 = {
	.chip_id = chip_s5,
	.chip_cls = STB_CHIP,
	.vlk_chip = vlock_chip_t5,
	.vlk_support = false,
	.vlk_new_fsm = 1,
	.vlk_hwver = vlock_hw_tm2verb,
	.vlk_phlock_en = false,
	.vlk_pll_sel = vlock_pll_sel_tcon,
};

static const struct vecm_match_data_s vecm_dt_a4 = {
	.chip_id = chip_a4,
	.chip_cls = AD_CHIP,
	.vlk_chip = vlock_chip_null,
	.vlk_support = false,
	.vlk_new_fsm = 1,
	.vlk_hwver = vlock_hw_tm2verb,
	.vlk_phlock_en = false,
	.vlk_pll_sel = vlock_pll_sel_tcon,
};

static const struct of_device_id aml_vecm_dt_match[] = {
	{
		.compatible = "amlogic, vecm",
		.data = &vecm_dt_xxx,
	},
//#ifndef CONFIG_AMLOGIC_REMOVE_OLD
	{
		.compatible = "amlogic, vecm-tl1",
		.data = &vecm_dt_tl1,
	},
//#endif
	{
		.compatible = "amlogic, vecm-sm1",
		.data = &vecm_dt_sm1,
	},
	{
		.compatible = "amlogic, vecm-tm2",
		.data = &vecm_dt_tm2,
	},
	{
		.compatible = "amlogic, vecm-tm2-verb",
		.data = &vecm_dt_tm2_verb,
	},
	{
		.compatible = "amlogic, vecm-t5",
		.data = &vecm_dt_t5,
	},
	{
		.compatible = "amlogic, vecm-t5d",
		.data = &vecm_dt_t5d,
	},
	{
		.compatible = "amlogic, vecm-t7",
		.data = &vecm_dt_t7,
	},
	{
		.compatible = "amlogic, vecm-t3",
		.data = &vecm_dt_t3,
	},
	{
		.compatible = "amlogic, vecm-t5w",
		.data = &vecm_dt_t5w,
	},
	{
		.compatible = "amlogic, vecm-s5",
		.data = &vecm_dt_s5,
	},
	{
		.compatible = "amlogic, vecm-a4",
		.data = &vecm_dt_a4,
	},
	{},
};

int pkt_adv_chip(void)
{
	int ret;

	if (chip_type_id == chip_t7 ||
		chip_type_id == chip_s5)
		ret = 1;
	else
		ret = 0;

	return ret;
}

static void aml_vecm_match_init(struct vecm_match_data_s *pdata)
{
	chip_type_id = pdata->chip_id;
	chip_cls_id = pdata->chip_cls;
	pr_info("vecm chip id: %d, chip_cls : %d\n", chip_type_id, chip_cls_id);
}

static void aml_vecm_dt_parse(struct amvecm_dev_s *devp, struct platform_device *pdev)
{
	struct device_node *node;
	unsigned int val;
	int ret;
	const struct of_device_id *of_id;
	struct vecm_match_data_s *matchdata;

	node = pdev->dev.of_node;
	/* get integer value */
	if (node) {
		ret = of_property_read_u32(node, "gamma_en", &val);
		if (ret)
			pr_amvecm_dbg("Can't find  gamma_en.\n");
		else
			gamma_en = val;
		ret = of_property_read_u32(node, "wb_en", &val);
		if (ret)
			pr_amvecm_dbg("Can't find  wb_en.\n");
		else
			wb_en = val;
		ret = of_property_read_u32(node, "lut3d_en", &val);
		if (ret)
			pr_info("Can't find  lut3d_en.\n");
		else
			lut3d_en = val;

		ret = of_property_read_u32(node, "ct_3dlut", &val);
		if (ret)
			pr_info("Can't find  ct_3dlut.\n");
		else
			ct_en = val;

		ret = of_property_read_u32(node, "cm_en", &val);
		if (ret)
			pr_amvecm_dbg("Can't find  cm_en.\n");
		else
			cm_en = val;
		ret = of_property_read_u32(node, "detect_colorbar", &val);
		if (ret) {
			pr_amvecm_dbg("Can't find  detect_colorbar.\n");
		} else {
			if (val == 0)
				pattern_mask =
				pattern_mask &
				(~PATTERN_MASK(PATTERN_75COLORBAR));
			else
				pattern_mask =
				pattern_mask |
				PATTERN_MASK(PATTERN_75COLORBAR);
		}
		ret = of_property_read_u32(node, "detect_face", &val);
		if (ret) {
			pr_amvecm_dbg("Can't find  detect_face.\n");
		} else {
			if (val == 0)
				pattern_mask =
				pattern_mask &
				(~PATTERN_MASK(PATTERN_SKIN_TONE_FACE));
			else
				pattern_mask =
				pattern_mask |
				PATTERN_MASK(PATTERN_SKIN_TONE_FACE);
		}
		ret = of_property_read_u32(node, "detect_corn", &val);
		if (ret) {
			pr_amvecm_dbg("Can't find  detect_corn.\n");
		} else {
			if (val == 0)
				pattern_mask =
				pattern_mask &
				(~PATTERN_MASK(PATTERN_GREEN_CORN));
			else
				pattern_mask =
				pattern_mask |
				PATTERN_MASK(PATTERN_GREEN_CORN);
		}
		ret = of_property_read_u32(node, "wb_sel", &val);
		if (ret)
			pr_amvecm_dbg("Can't find  wb_sel.\n");
		else
			video_rgb_ogo_xvy_mtx = val;

		ret = of_property_read_u32(node, "hist_sel", &val);
		if (ret)
			pr_amvecm_dbg("Can't find  hist_sel.\n");
		else
			hist_chl = val;

		/*hdr:cfg:osd_100*/
		ret = of_property_read_u32(node, "cfg_en_osd_100", &val);
		if (ret) {
			hdr_set_cfg_osd_100(0);
			pr_amvecm_dbg("hdr:Can't find  cfg_en_osd_100.\n");

		} else {
			hdr_set_cfg_osd_100((int)val);
		}
		ret = of_property_read_u32(node, "tx_op_color_primary", &val);
		if (ret)
			pr_amvecm_dbg("Can't find  tx_op_color_primary.\n");
		else
			tx_op_color_primary = val;

		/*get compatible matched device, to get chip related data*/
		of_id = of_match_device(aml_vecm_dt_match, &pdev->dev);
		if (of_id) {
			pr_amvecm_dbg("%s", of_id->compatible);
			matchdata = (struct vecm_match_data_s *)of_id->data;
		} else {
			matchdata = (struct vecm_match_data_s *)&vecm_dt_xxx;
			pr_amvecm_dbg("unable to get matched device\n");
		}
		aml_vecm_match_init(matchdata);
		vlock_dt_match_init(matchdata);

		frame_lock_set_vrr_support_flag(matchdata->vrr_support_flag);

		/*vlock param config*/
		vlock_param_config(node);
		vlock_clk_config(devp, &pdev->dev);

		vlock_status_init();

		/*vrr param config*/
		frame_lock_param_config(node);
	}
	/* init module status */
#ifdef CONFIG_AMLOGIC_PIXEL_PROBE
	vpp_probe_enable();
#endif
	amvecm_wb_init(wb_en);
	amvecm_gamma_init(gamma_en);
	amvecm_3dlut_init(lut3d_en);
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	if (!is_amdv_enable())
		WRITE_VPP_REG_BITS(VPP_MISC, 1, 28, 1);
#endif
	if (chip_type_id != chip_s5)
		WRITE_VPP_REG_BITS(VPP_MISC, 1, 28, 1);

	if (cm_en) {
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
		if (!is_amdv_enable())
#endif
			amcm_enable(WR_VCB);
	} else {
		amcm_disable(WR_VCB);
	}
	/* WRITE_VPP_REG_BITS(VPP_MISC, cm_en, 28, 1); */

	res_viu2_vsync_irq =
		platform_get_resource_byname(pdev, IORESOURCE_IRQ, "vsync2");
	res_lc_curve_irq =
		platform_get_resource_byname(pdev, IORESOURCE_IRQ, "lc_curve");
}

#ifdef CONFIG_AMLOGIC_LCD
static int aml_lcd_gamma_notifier(struct notifier_block *nb,
				  unsigned long event, void *data)
{
	unsigned int *param;

	if ((event & LCD_EVENT_GAMMA_UPDATE) == 0)
		return NOTIFY_DONE;

	param = (unsigned int *)data;
	/*gamma_index: select which vpp,  vpp0/vpp1/vpp2 gamma*/
	gamma_index = param[0];
	/*freesync 10 groups gamma table index, index:0~10
	 *0xff: default(not tcon) gamma
	 */
	gm_par_idx = param[1];

	if (gm_par_idx != 0xff) {
		if (!frame_lock_get_vrr_status())
			return NOTIFY_DONE;

		memcpy(&video_gamma_table_r,
			&gt.gm_tb[gm_par_idx][0],
			sizeof(struct tcon_gamma_table_s));
		memcpy(&video_gamma_table_g,
			&gt.gm_tb[gm_par_idx][1],
			sizeof(struct tcon_gamma_table_s));
		memcpy(&video_gamma_table_b,
			&gt.gm_tb[gm_par_idx][2],
			sizeof(struct tcon_gamma_table_s));
	}

	vecm_latch_flag |= FLAG_GAMMA_TABLE_R;
	vecm_latch_flag |= FLAG_GAMMA_TABLE_G;
	vecm_latch_flag |= FLAG_GAMMA_TABLE_B;

	pr_amvecm_dbg("%s: gamma_index = %d, gm_par_idx = %d\n",
		__func__, gamma_index, gm_par_idx);
	return NOTIFY_OK;
}

static struct notifier_block aml_lcd_gamma_nb = {
	.notifier_call = aml_lcd_gamma_notifier,
};
#endif

static struct notifier_block vlock_notifier_nb = {
	.notifier_call	= vlock_notify_callback,
};

static struct notifier_block flock_vdin_vrr_en_notifier_nb = {
	.notifier_call	= flock_vrr_nfy_callback,
};

static int aml_vecm_viu2_vsync_irq_init(void)
{
	if (res_viu2_vsync_irq) {
		if (request_irq(res_viu2_vsync_irq->start,
				amvecm_viu2_vsync_isr, IRQF_SHARED,
				"amvecm_vsync2", (void *)"amvecm_vsync2")) {
			pr_err("can't request amvecm_vsync2_irq\n");
		} else {
			pr_info("request amvecm_vsync2_irq successful\n");
		}
	}

	return 0;
}

static int aml_vecm_lc_curve_irq_init(void)
{
	if (res_lc_curve_irq) {
		if (request_irq(res_lc_curve_irq->start,
				amvecm_lc_curve_isr, IRQF_SHARED,
				"lc_curve_isr", (void *)"lc_curve_isr")) {
			pr_err("can't request res_lc_curve_irq\n");
		} else {
			lc_curve_isr_defined = 1;
			pr_info("request res_lc_curve_irq successful\n");
		}
	}

	return 0;
}

static int aml_vecm_probe(struct platform_device *pdev)
{
	int ret = 0;
	int i = 0;
	struct amvecm_dev_s *devp = &amvecm_dev;

	memset(devp, 0, (sizeof(struct amvecm_dev_s)));
	pr_info("\n VECM probe start\n");
	hdr_lut_buffer_malloc(pdev);
	ret = alloc_chrdev_region(&devp->devno, 0, 1, AMVECM_NAME);
	if (ret < 0)
		goto fail_alloc_region;

	devp->clsp = class_create(THIS_MODULE, AMVECM_CLASS_NAME);
	if (IS_ERR(devp->clsp)) {
		ret = PTR_ERR(devp->clsp);
		goto fail_create_class;
	}
	for (i = 0; amvecm_class_attrs[i].attr.name; i++) {
		if (class_create_file(devp->clsp, &amvecm_class_attrs[i]) < 0)
			goto fail_class_create_file;
	}
	cdev_init(&devp->cdev, &amvecm_fops);
	devp->cdev.owner = THIS_MODULE;
	ret = cdev_add(&devp->cdev, devp->devno, 1);
	if (ret)
		goto fail_add_cdev;

	devp->dev = device_create(devp->clsp, NULL, devp->devno,
				  NULL, AMVECM_NAME);
	if (IS_ERR(devp->dev)) {
		ret = PTR_ERR(devp->dev);
		goto fail_create_device;
	}

	spin_lock_init(&vpp_lcd_gamma_lock);
	spin_lock_init(&vpp_3dlut_lock);
	mutex_init(&vpp_lut3d_lock);
#ifdef CONFIG_AMLOGIC_LCD
	ret = aml_lcd_atomic_notifier_register(&aml_lcd_gamma_nb);
	if (ret)
		pr_info("register aml_lcd_gamma_notifier failed\n");

	INIT_WORK(&aml_lcd_vlock_param_work, vlock_lcd_param_work);
#endif
	/* register vout client */
	vout_register_client(&vlock_notifier_nb);
	/* register vdin vrr en client*/
	frame_lock_vrr_off_done_init();
	aml_vrr_atomic_notifier_register(&flock_vdin_vrr_en_notifier_nb);
	/* #endif */

	if (is_meson_txlx_cpu()) {
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
		vpp_set_12bit_datapath2();
		/*post matrix 12bit yuv2rgb*/
		/* mtx_sel_dbg |= 1 << VPP_MATRIX_2; */
		/* amvecm_vpp_mtx_debug(mtx_sel_dbg, 1);*/
		WRITE_VPP_REG(VPP_MATRIX_PROBE_POS, 0x1fff1fff);
#endif
	} else if (is_meson_txhd_cpu()) {
		vpp_set_10bit_datapath1();
	} else if (is_meson_g12a_cpu() || is_meson_g12b_cpu()) {
		vpp_set_12bit_datapath_g12a();
	}
	memset(&vpp_hist_param.vpp_histgram[0],
	       0, sizeof(unsigned short) * 64);
	/* box sdr_mode:auto, tv sdr_mode:off */
	/* disable contrast and saturation adjustment for HDR on TV */
	/* disable SDR to HDR convert on TV */
	if (is_meson_gxl_cpu() || is_meson_gxm_cpu()) {
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
		hdr_flag = (1 << 0) | (1 << 1) | (1 << 2) | (1 << 3);
#endif
	} else {
		hdr_flag = (1 << 0) | (1 << 1) | (0 << 2) | (0 << 3) | (1 << 4);
	}
	hdr_init(&amvecm_dev.hdr_d);
	aml_vecm_dt_parse(devp, pdev);

	init_pattern_detect();
	vpp_get_hist_en();
	init_pq_setting();
	aml_vecm_viu2_vsync_irq_init();
	lc_curve_isr_defined = 0;
	aml_vecm_lc_curve_irq_init();

	pkt_delay_flag_init();

	aml_cabc_queue = create_workqueue("cabc workqueue");
	if (!aml_cabc_queue) {
		pr_amvecm_dbg("cacb queue create failed");
		ret = -1;
		goto fail_create_wq;
	}
	INIT_WORK(&cabc_proc_work, aml_cabc_alg_process);
	INIT_WORK(&cabc_bypass_work, aml_cabc_alg_bypass);

	probe_ok = 1;
	pr_info("%s: ok\n", __func__);
	return 0;

fail_create_device:
	pr_info("[amvecm.] : amvecm device create error.\n");
	cdev_del(&devp->cdev);
	return ret;
fail_add_cdev:
	pr_info("[amvecm.] : amvecm add device error.\n");
	return ret;
fail_class_create_file:
	pr_info("[amvecm.] : amvecm class create file error.\n");
	for (i = 0; amvecm_class_attrs[i].attr.name; i++) {
		class_remove_file(devp->clsp,
				  &amvecm_class_attrs[i]);
	}
	class_destroy(devp->clsp);
	return ret;
fail_create_class:
	pr_info("[amvecm.] : amvecm class create error.\n");
	unregister_chrdev_region(devp->devno, 1);
	return ret;
fail_alloc_region:
	pr_info("[amvecm.] : amvecm alloc error.\n");
	pr_info("[amvecm.] : amvecm_init.\n");
	return ret;
fail_create_wq:
	pr_info("[amvecm.] : amvecm create wq error\n");
	return ret;
}

static int __exit aml_vecm_remove(struct platform_device *pdev)
{
	struct amvecm_dev_s *devp = &amvecm_dev;

	if (res_viu2_vsync_irq) {
		free_irq(res_viu2_vsync_irq->start,
			 (void *)"amvecm_vsync2");
	}
	hdr_lut_buffer_free(pdev);

	hdr_exit();
	device_destroy(devp->clsp, devp->devno);
	cdev_del(&devp->cdev);
	class_destroy(devp->clsp);
	unregister_chrdev_region(devp->devno, 1);
#ifdef CONFIG_AMLOGIC_LCD
	aml_lcd_atomic_notifier_unregister(&aml_lcd_gamma_nb);
	cancel_work_sync(&aml_lcd_vlock_param_work);
#endif
	vout_unregister_client(&vlock_notifier_nb);
	aml_vrr_atomic_notifier_unregister(&flock_vdin_vrr_en_notifier_nb);

	probe_ok = 0;
	lc_free();
	pr_info("[amvecm.] : amvecm_exit.\n");
	return 0;
}

#ifdef CONFIG_PM
static int amvecm_drv_suspend(struct platform_device *pdev,
			      pm_message_t state)
{
	if (probe_ok == 1)
		probe_ok = 0;

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_T5D))
		vlock_clk_suspend();

	pr_info("amvecm: suspend module\n");
	return 0;
}

static int amvecm_drv_resume(struct platform_device *pdev)
{
	if (probe_ok == 0)
		probe_ok = 1;

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_T5D))
		vlock_clk_resume();

	pr_info("amvecm: resume module\n");
	return 0;
}
#endif

static void amvecm_shutdown(struct platform_device *pdev)
{
	struct amvecm_dev_s *devp = &amvecm_dev;

	if (chip_type_id != chip_a4) {
		hdr_exit();
		ve_disable_dnlp();
		amcm_disable(WR_VCB);
		WRITE_VPP_REG(VPP_VADJ_CTRL, 0x0);
		amvecm_wb_enable(0);
		/*dnlp cm vadj1 wb gate*/
		WRITE_VPP_REG(VPP_GCLK_CTRL0, 0x11000400);
		WRITE_VPP_REG(VPP_GCLK_CTRL1, 0x14);
	}
	pr_info("amvecm: shutdown module\n");

	device_destroy(devp->clsp, devp->devno);
	cdev_del(&devp->cdev);
	class_destroy(devp->clsp);
	unregister_chrdev_region(devp->devno, 1);
#ifdef CONFIG_AML_LCD
	aml_lcd_atomic_notifier_unregister(&aml_lcd_gamma_nb);
#endif
	//lc_free();
	vpp_lut3d_table_release();
	lut_release();
}

static struct platform_driver aml_vecm_driver = {
	.driver = {
		.name = "aml_vecm",
		.owner = THIS_MODULE,
		.of_match_table = aml_vecm_dt_match,
	},
	.probe = aml_vecm_probe,
	.shutdown = amvecm_shutdown,
	.remove = __exit_p(aml_vecm_remove),
#ifdef CONFIG_PM
	.suspend    = amvecm_drv_suspend,
	.resume     = amvecm_drv_resume,
#endif

};

int __init aml_vecm_init(void)
{
	/*unsigned int hiu_reg_base;*/

	pr_info("%s:module init 20230726\n", __func__);

	if (platform_driver_register(&aml_vecm_driver)) {
		pr_err("failed to register bl driver module\n");
		return -ENODEV;
	}

	return 0;
}

void __exit aml_vecm_exit(void)
{
	pr_info("%s:module exit\n", __func__);
	/*iounmap(amvecm_hiu_reg_base);*/
	platform_driver_unregister(&aml_vecm_driver);
}

//MODULE_VERSION(AMVECM_VER);
//MODULE_DESCRIPTION("AMLOGIC amvecm driver");
//MODULE_LICENSE("GPL");

