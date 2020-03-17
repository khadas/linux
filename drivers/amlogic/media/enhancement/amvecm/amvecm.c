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
#include <linux/of.h>
#include <linux/of_device.h>
/* #include <linux/amlogic/aml_common.h> */
#include <linux/ctype.h>/* for parse_para_pq */
#include <linux/vmalloc.h>
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

#include "arch/vpp_regs.h"
#include "arch/ve_regs.h"
#include "arch/cm_regs.h"
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
#include "arch/vpp_hdr_regs.h"
#include "set_hdr2_v0.h"

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

};

#ifdef CONFIG_AMLOGIC_LCD
struct work_struct aml_lcd_vlock_param_work;
#endif

static struct amvecm_dev_s amvecm_dev;
static struct resource *res_viu2_vsync_irq;

/*define spin lock for gamma*/
spinlock_t vpp_lcd_gamma_lock;

signed int vd1_brightness = 0, vd1_contrast;

static int hue_pre;  /*-25~25*/
static int saturation_pre;  /*-128~127*/
static int hue_post;  /*-25~25*/
static int saturation_post;  /*-128~127*/

static s16 saturation_ma;
static s16 saturation_mb;
static s16 saturation_ma_shift;
static s16 saturation_mb_shift;

static int cm2_hue_array[ecm2colormd_max][3];
static int cm2_luma_array[ecm2colormd_max][3];
static int cm2_sat_array[ecm2colormd_max][3];
static int cm2_hue_by_hs_array[ecm2colormd_max][3];

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

static int vdj_mode_flg;
struct am_vdj_mode_s vdj_mode_s;

/*void __iomem *amvecm_hiu_reg_base;*//* = *ioremap(0xc883c000, 0x2000); */

static int debug_amvecm;
module_param(debug_amvecm, int, 0664);
MODULE_PARM_DESC(debug_amvecm, "\n debug_amvecm\n");

unsigned int vecm_latch_flag;
module_param(vecm_latch_flag, uint, 0664);
MODULE_PARM_DESC(vecm_latch_flag, "\n vecm_latch_flag\n");

unsigned int vpp_demo_latch_flag;
module_param(vpp_demo_latch_flag, uint, 0664);
MODULE_PARM_DESC(vpp_demo_latch_flag, "\n vpp_demo_latch_flag\n");

unsigned int pq_load_en = 1;/* load pq table enable/disable */
module_param(pq_load_en, uint, 0664);
MODULE_PARM_DESC(pq_load_en, "\n pq_load_en\n");

bool gamma_en;  /* wb_gamma_en enable/disable */
module_param(gamma_en, bool, 0664);
MODULE_PARM_DESC(gamma_en, "\n gamma_en\n");

bool wb_en;  /* wb_en enable/disable */
module_param(wb_en, bool, 0664);
MODULE_PARM_DESC(wb_en, "\n wb_en\n");

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

/*0: 709/601	1: bt2020*/
int tx_op_color_primary;
module_param(tx_op_color_primary, int, 0664);
MODULE_PARM_DESC(tx_op_color_primary,
		 "tx output color_primary");

unsigned int debug_game_mode_1;
module_param(debug_game_mode_1, uint, 0664);
MODULE_PARM_DESC(debug_game_mode_1, "\n debug_game_mode_1\n");
unsigned int pq_user_value;
enum hdr_type_e hdr_source_type = HDRTYPE_NONE;

#define SR0_OFFSET 0xc00
#define SR1_OFFSET 0xc80
unsigned int sr_offset[2] = {0, 0};

unsigned int sr_demo_flag;

bool pd_detect_en;
int pd_fix_lvl = PD_HIG_LVL;

unsigned int gmv_th = 17;

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

#ifndef MODULE
/* vpp brightness/contrast/saturation/hue */
static int __init amvecm_load_pq_val(char *str)
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
#endif

static int amvecm_set_contrast2(int val)
{
	val += 0x80;
	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A) {
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
	if (get_cpu_type() <= MESON_CPU_MAJOR_ID_GXTVBB)
		WRITE_VPP_REG_BITS(VPP_VADJ2_Y,
				   val, 8, 9);
	else if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A)
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

static void amvecm_size_patch(unsigned int cm_in_w,
			      unsigned int cm_in_h)
{
	unsigned int hs, he, vs, ve;

	if (cm_in_w == 0 && cm_in_h == 0) {
		hs = READ_VPP_REG_BITS(VPP_POSTBLEND_VD1_H_START_END, 16, 13);
		he = READ_VPP_REG_BITS(VPP_POSTBLEND_VD1_H_START_END, 0, 13);

		vs = READ_VPP_REG_BITS(VPP_POSTBLEND_VD1_V_START_END, 16, 13);
		ve = READ_VPP_REG_BITS(VPP_POSTBLEND_VD1_V_START_END, 0, 13);
		cm2_frame_size_patch(he - hs + 1, ve - vs + 1);
	} else {
		cm2_frame_size_patch(cm_in_w, cm_in_h);
	}
}

/* video adj1 */
static ssize_t video_adj1_brightness_show(struct class *cla,
					  struct class_attribute *attr,
					  char *buf)
{
	s32 val = 0;

	if (get_cpu_type() <= MESON_CPU_MAJOR_ID_GXTVBB) {
		val = (READ_VPP_REG(VPP_VADJ1_Y) >> 8) & 0x1ff;
		val = (val << 23) >> 23;

		return sprintf(buf, "%d\n", val);
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

	if (get_cpu_type() <= MESON_CPU_MAJOR_ID_GXTVBB)
		WRITE_VPP_REG_BITS(VPP_VADJ1_Y, val, 8, 9);
	else if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A)
		WRITE_VPP_REG_BITS(VPP_VADJ1_Y_2, val, 8, 11);
	else
		WRITE_VPP_REG_BITS(VPP_VADJ1_Y, val >> 1, 8, 10);

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
	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A)
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

	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A) {
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

	if (get_cpu_type() <= MESON_CPU_MAJOR_ID_GXTVBB) {
		val = (READ_VPP_REG(VPP_VADJ2_Y) >> 8) & 0x1ff;
		val = (val << 23) >> 23;

		return sprintf(buf, "%d\n", val);
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
	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A)
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
	ssize_t len = 0;

	len += sprintf(buf + len,
		"echo vlock_mode val(0/1/2) > /sys/class/amvecm/vlock\n");
	len += sprintf(buf + len,
		"echo vlock_en val(0/1) > /sys/class/amvecm/vlock\n");
	len += sprintf(buf + len,
		"echo vlock_adapt val(0/1) > /sys/class/amvecm/vlock\n");
	len += sprintf(buf + len,
		"echo vlock_dis_cnt_limit val(D) > /sys/class/amvecm/vlock\n");
	len += sprintf(buf + len,
		"echo vlock_delta_limit val(D) > /sys/class/amvecm/vlock\n");
	len += sprintf(buf + len,
		"echo vlock_pll_m_limit val(D) > /sys/class/amvecm/vlock\n");
	len += sprintf(buf + len,
		"echo vlock_delta_cnt_limit val(D) > /sys/class/amvecm/vlock\n");
	len += sprintf(buf + len,
		"echo vlock_debug val(0x111) > /sys/class/amvecm/vlock\n");
	len += sprintf(buf + len,
		"echo vlock_dynamic_adjust val(0/1) > /sys/class/amvecm/vlock\n");
	len += sprintf(buf + len,
		"echo vlock_dis_cnt_no_vf_limit val(D) > /sys/class/amvecm/vlock\n");
	len += sprintf(buf + len,
		"echo vlock_line_limit val(D) > /sys/class/amvecm/vlock\n");
	len += sprintf(buf + len,
		"echo vlock_support val(D) > /sys/class/amvecm/vlock\n");
	len += sprintf(buf + len,
		"echo enable > /sys/class/amvecm/vlock\n");
	len += sprintf(buf + len,
		"echo disable > /sys/class/amvecm/vlock\n");
	len += sprintf(buf + len,
		"echo status > /sys/class/amvecm/vlock\n");
	len += sprintf(buf + len,
		"echo dump_reg > /sys/class/amvecm/vlock\n");
	len += sprintf(buf + len,
		"echo log_start > /sys/class/amvecm/vlock\n");
	len += sprintf(buf + len,
		"echo log_stop > /sys/class/amvecm/vlock\n");
	len += sprintf(buf + len,
		"echo log_print > /sys/class/amvecm/vlock\n");
	return len;
}

static ssize_t amvecm_vlock_store(struct class *cla,
				  struct class_attribute *attr,
		const char *buf, size_t count)
{
	char *buf_orig, *parm[8] = {NULL};
	long val;
	unsigned int temp_val;
	enum vlock_param_e sel = VLOCK_PARAM_MAX;

	if (!buf)
		return count;
	if (!is_meson_gxtvbb_cpu() &&
	    !is_meson_gxbb_cpu() &&
		(get_cpu_type() < MESON_CPU_MAJOR_ID_GXL)) {
		pr_info("\n chip does not support vlock process!!!\n");
		return count;
	}

	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig)
		return -ENOMEM;
	parse_param_amvecm(buf_orig, (char **)&parm);
	if (!strncmp(parm[0], "vlock_mode", 10)) {
		if (kstrtol(parm[1], 10, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		temp_val = val;
		sel = VLOCK_MODE;
	} else if (!strncmp(parm[0], "vlock_en", 8)) {
		if (kstrtol(parm[1], 10, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		temp_val = val;
		sel = VLOCK_EN;
	} else if (!strncmp(parm[0], "vlock_adapt", 11)) {
		if (kstrtol(parm[1], 10, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		temp_val = val;
		sel = VLOCK_ADAPT;
	} else if (!strncmp(parm[0], "vlock_dis_cnt_limit", 19)) {
		if (kstrtol(parm[1], 10, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		temp_val = val;
		sel = VLOCK_DIS_CNT_LIMIT;
	} else if (!strncmp(parm[0], "vlock_delta_limit", 17)) {
		if (kstrtol(parm[1], 10, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		temp_val = val;
		sel = VLOCK_DELTA_LIMIT;
	} else if (!strncmp(parm[0], "vlock_pll_m_limit", 17)) {
		if (kstrtol(parm[1], 10, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		temp_val = val;
		sel = VLOCK_PLL_M_LIMIT;
	} else if (!strncmp(parm[0], "vlock_delta_cnt_limit", 21)) {
		if (kstrtol(parm[1], 10, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		temp_val = val;
		sel = VLOCK_DELTA_CNT_LIMIT;
	} else if (!strncmp(parm[0], "vlock_debug", 11)) {
		if (kstrtol(parm[1], 16, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		temp_val = val;
		sel = VLOCK_DEBUG;
	} else if (!strncmp(parm[0], "vlock_dynamic_adjust", 20)) {
		if (kstrtol(parm[1], 10, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		temp_val = val;
		sel = VLOCK_DYNAMIC_ADJUST;
	} else if (!strncmp(parm[0], "vlock_line_limit", 17)) {
		if (kstrtol(parm[1], 10, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		temp_val = val;
		sel = VLOCK_LINE_LIMIT;
	} else if (!strncmp(parm[0], "vlock_dis_cnt_no_vf_limit", 25)) {
		if (kstrtol(parm[1], 10, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		temp_val = val;
		sel = VLOCK_DIS_CNT_NO_VF_LIMIT;
	} else if (!strncmp(parm[0], "vlock_line_limit", 16)) {
		if (kstrtol(parm[1], 10, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		temp_val = val;
		sel = VLOCK_LINE_LIMIT;
	} else if (!strncmp(parm[0], "vlock_support", 13)) {
		if (kstrtol(parm[1], 10, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		temp_val = val;
		sel = VLOCK_SUPPORT;
	} else if (!strncmp(parm[0], "enable", 6)) {
		vecm_latch_flag |= FLAG_VLOCK_EN;
		vlock_set_en(true);
	} else if (!strncmp(parm[0], "disable", 7)) {
		vecm_latch_flag |= FLAG_VLOCK_DIS;
	} else if (!strncmp(parm[0], "status", 6)) {
		vlock_status();
	} else if (!strncmp(parm[0], "dump_reg", 8)) {
		vlock_reg_dump();
	} else if (!strncmp(parm[0], "log_start", 9)) {
		vlock_log_start();
	} else if (!strncmp(parm[0], "log_stop", 8)) {
		vlock_log_stop();
	} else if (!strncmp(parm[0], "log_print", 9)) {
		vlock_log_print();
	} else if (!strncmp(parm[0], "phase", 5)) {
		if (kstrtol(parm[1], 10, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		vlock_set_phase(val);
	} else if (!strncmp(parm[0], "phlock_en", 9)) {
		if (kstrtol(parm[1], 10, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		vlock_set_phase_en(val);
	} else {
		pr_info("----cmd list -----\n");
		pr_info("vlock_mode val\n");
		pr_info("vlock_en val\n");
		pr_info("vlock_debug val\n");
		pr_info("vlock_adapt val\n");
		pr_info("vlock_dis_cnt_limit val\n");
		pr_info("vlock_delta_limit val\n");
		pr_info("vlock_dynamic_adjust val\n");
		pr_info("vlock_line_limit val\n");
		pr_info("vlock_dis_cnt_no_vf_limit val\n");
		pr_info("vlock_line_limit val\n");
		pr_info("vlock_support val\n");
		pr_info("enable\n");
		pr_info("disable\n");
		pr_info("status\n");
		pr_info("dump_reg\n");
		pr_info("log_start\n");
		pr_info("log_stop\n");
		pr_info("log_print\n");
		pr_info("phase persent\n");
		pr_info("phlock_en val\n");
	}
	if (sel < VLOCK_PARAM_MAX)
		vlock_param_set(temp_val, sel);
	kfree(buf_orig);
	return count;
}

/* #endif */

static void vpp_backup_histgram(struct vframe_s *vf)
{
	unsigned int i = 0;

	vpp_hist_param.vpp_hist_pow = vf->prop.hist.hist_pow;
	vpp_hist_param.vpp_luma_sum = vf->prop.hist.vpp_luma_sum;
	vpp_hist_param.vpp_pixel_sum = vf->prop.hist.vpp_pixel_sum;
	for (i = 0; i < 64; i++)
		vpp_hist_param.vpp_histgram[i] = vf->prop.hist.vpp_gamma[i];
	for (i = 0; i < 128; i++)
		vpp_hist_param.hdr_histgram[i] = hdr_hist[NUM_HDR_HIST - 1][i];
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
	WRITE_VPP_REG_BITS(VI_HIST_CTRL, 0x1, 11, 3);
	WRITE_VPP_REG_BITS(VI_HIST_CTRL, 0x1, 0, 1);
	WRITE_VPP_REG(VI_HIST_GCLK_CTRL, 0xffffffff);
	WRITE_VPP_REG_BITS(VI_HIST_CTRL, 2, VI_HIST_POW_BIT, VI_HIST_POW_WID);
}

static unsigned int vpp_luma_max;
void vpp_get_vframe_hist_info(struct vframe_s *vf)
{
	unsigned int hist_height, hist_width, i;
	u64 divid;

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

	if (enable_pattern_detect == 1) {
		for (i = 0; i < 32; i++) {
			WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT,
				      RO_CM_HUE_HIST_BIN0 + i);
			vf->prop.hist.vpp_hue_gamma[i] =
				READ_VPP_REG(VPP_CHROMA_DATA_PORT);
		}
		for (i = 0; i < 32; i++) {
			WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT,
				      RO_CM_SAT_HIST_BIN0 + i);
			vf->prop.hist.vpp_sat_gamma[i] =
				READ_VPP_REG(VPP_CHROMA_DATA_PORT);
		}
	}
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

void vpp_demo_config(struct vframe_s *vf)
{
	unsigned int reg_value;
	/*dnlp demo config*/
	if (vpp_demo_latch_flag & VPP_DEMO_DNLP_EN) {
		WRITE_VPP_REG_BITS(VPP_VE_ENABLE_CTRL, 1, 18, 1);
		/*bit14-15   left: 2   right: 3*/
		WRITE_VPP_REG_BITS(VPP_VE_ENABLE_CTRL, 2, 14, 2);
		reg_value = READ_VPP_REG_BITS(VPP_SRSHARP1_CTRL, 0, 1);
		if ((vf->height > 1080 && vf->width > 1920) ||
		    reg_value == 0)
			WRITE_VPP_REG_BITS(VPP_VE_DEMO_LEFT_TOP_SCREEN_WIDTH,
					   1920, 0, 12);
		else
			WRITE_VPP_REG_BITS(VPP_VE_DEMO_LEFT_TOP_SCREEN_WIDTH,
					   960, 0, 12);
		vpp_demo_latch_flag &= ~VPP_DEMO_DNLP_EN;
	} else if (vpp_demo_latch_flag & VPP_DEMO_DNLP_DIS) {
		WRITE_VPP_REG_BITS(VPP_VE_ENABLE_CTRL, 0, 18, 1);
		WRITE_VPP_REG_BITS(VPP_VE_ENABLE_CTRL, 0, 14, 2);
		WRITE_VPP_REG_BITS(VPP_VE_DEMO_LEFT_TOP_SCREEN_WIDTH,
				   0xfff, 0, 12);
		vpp_demo_latch_flag &= ~VPP_DEMO_DNLP_DIS;
	}
	/*cm demo config*/
	if (vpp_demo_latch_flag & VPP_DEMO_CM_EN) {
		/*left: 0x1   right: 0x4*/
		WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT, 0x20f);
		WRITE_VPP_REG(VPP_CHROMA_DATA_PORT, 0x1);
		vpp_demo_latch_flag &= ~VPP_DEMO_CM_EN;
	} else if (vpp_demo_latch_flag & VPP_DEMO_CM_DIS) {
		WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT, 0x20f);
		WRITE_VPP_REG(VPP_CHROMA_DATA_PORT, 0x0);
		vpp_demo_latch_flag &= ~VPP_DEMO_CM_DIS;
	}
}

void vpp_game_mode_process(struct vframe_s *vf)
{
	if (vf->flag & VFRAME_FLAG_GAME_MODE) {
		if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A)
			WRITE_VPP_REG_BITS(VPP_VADJ1_MISC, 0, 0, 1);
		else
			WRITE_VPP_REG_BITS(VPP_VADJ_CTRL, 0, 0, 1);
	}
}

void amvecm_dejaggy_patch(struct vframe_s *vf)
{
	if (!vf) {
		if (pd_detect_en)
			pd_detect_en = 0;
		return;
	}

	if (vf->height == 1080 &&
	    vf->width == 1920 &&
	    (vf->di_pulldown & (1 << 3)) &&
	    ((vf->di_pulldown & 0x7) || ((vf->di_gmv / 10000) >= gmv_th))) {
		if (pd_detect_en == 1)
			return;
		pd_detect_en = 1;
		pd_combing_fix_patch(pd_fix_lvl);
		pr_amvecm_dbg("pd_detect_en = %d; pd_fix_lvl = %d\n",
			      pd_detect_en, pd_fix_lvl);
	} else if (pd_detect_en) {
		pd_detect_en = 0;
		pd_combing_fix_patch(PD_DEF_LVL);
		pr_amvecm_dbg("pd_detect_en = %d; pd_fix_lvl = %d\n",
			      pd_detect_en, pd_fix_lvl);
	}
}

void amvecm_video_latch(void)
{
	pc_mode_process();
	cm_latch_process();
	/*amvecm_size_patch();*/
	ve_dnlp_latch_process();
	if (vpp_get_encl_viu_mux() == 1)
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
}

int amvecm_on_vs(struct vframe_s *vf,
		 struct vframe_s *toggle_vf,
		 int flags,
		 unsigned int sps_h_en,
		 unsigned int sps_v_en,
		 unsigned int sps_w_in,
		 unsigned int sps_h_in,
		 unsigned int cm_in_w,
		 unsigned int cm_in_h,
		 enum vd_path_e vd_path)
{
	int result = 0, temp;

	if (probe_ok == 0)
		return 0;
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	if (for_dolby_vision_video_effect() && vd_path == VD1_PATH)
		return 0;
#endif
	if (!dnlp_insmod_ok && vd_path == VD1_PATH)
		dnlp_alg_param_init();

	if (flags & CSC_FLAG_CHECK_OUTPUT) {
		if (vd_path == VD1_PATH) {
			if (toggle_vf)
				amvecm_fresh_overscan(toggle_vf);
			else if (vf)
				amvecm_fresh_overscan(vf);
		}
		/* to test if output will change */
		return amvecm_matrix_process(toggle_vf, vf, flags, vd_path);
	} else if (vd_path == VD1_PATH) {
		send_hdr10_plus_pkt(vd_path);
	}
	if ((toggle_vf) || (vf)) {
		/* matrix adjust */
		result = amvecm_matrix_process(toggle_vf, vf, flags, vd_path);
		if (toggle_vf)
			ioctrl_get_hdr_metadata(toggle_vf);

		if (toggle_vf && vd_path == VD1_PATH) {
			lc_process(toggle_vf, sps_h_en, sps_v_en,
				   sps_w_in, sps_h_in);
			amvecm_size_patch(cm_in_w, cm_in_h);
			/*1080i pulldown combing workaround*/
			amvecm_dejaggy_patch(toggle_vf);
		}
		/*refresh vframe*/
		if (!toggle_vf && vf)
			refresh_on_vs(vf);
	} else {
		if (vd_path == VD1_PATH)
			amvecm_reset_overscan();
		result = amvecm_matrix_process(NULL, NULL, flags, vd_path);
		if (vd_path == VD1_PATH) {
			ve_hist_gamma_reset();
			lc_process(NULL, sps_h_en, sps_v_en,
				   sps_w_in, sps_h_in);
			/*1080i pulldown combing workaround*/
			amvecm_dejaggy_patch(NULL);
		}
	}

	if (vd_path != VD1_PATH)
		return result;

	/* add some flag to trigger */
	if (vf) {
		/*gxlx sharpness adaptive setting*/
		if (is_meson_gxlx_cpu())
			amve_sharpness_adaptive_setting(vf,
							sps_h_en, sps_v_en);

		vpp_game_mode_process(vf);

		if (pc_mode != 0 &&
		    (!(vf->flag & VFRAME_FLAG_GAME_MODE))) {
			temp = vd1_contrast + vd1_contrast_offset;
			amvecm_bricon_process(vd1_brightness,
					      temp, vf);
			temp = saturation_pre + saturation_offset;
			amvecm_color_process(temp,
					     hue_pre, vf);
		}

		vpp_demo_config(vf);
	}
	if (vf)
		amvecm_fresh_overscan(vf);
	else
		amvecm_reset_overscan();

	/* pq latch process */
	amvecm_video_latch();
	return result;
}
EXPORT_SYMBOL(amvecm_on_vs);

void refresh_on_vs(struct vframe_s *vf)
{
	if (probe_ok == 0)
		return;
	if (vf) {
		vpp_get_vframe_hist_info(vf);
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
		if (!for_dolby_vision_video_effect())
#endif
			ve_on_vs(vf);
		vpp_backup_histgram(vf);
		pattern_detect(vf);
	}
}
EXPORT_SYMBOL(refresh_on_vs);

static irqreturn_t amvecm_viu2_vsync_isr(int irq, void *dev_id)
{
	if (vpp_get_encl_viu_mux() == 2)
		ve_lcd_gamma_process();
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

static int amvecm_set_saturation_hue(int mab)
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

	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A)
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

	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A) {
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
	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A)
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
	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A) {
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
	/*because SOURCE_TV is 0,so need to add a flg to check ATV*/
	if (overscan_table[offset].load_flag == 1 &&
	    overscan_table[offset].source == SOURCE_TV)
		atv_source_flg = 1;
	else
		atv_source_flg = 0;
}

static void hdr_tone_mapping_get(unsigned int length,
				 unsigned int *hdr_tm)
{
	int i;

	if (hdr_tm) {
		for (i = 0; i < length; i++)
			oo_y_lut_hdr_sdr_def[i] = hdr_tm[i];
	}

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

	if (debug_amvecm & 2)
		pr_info("[amvecm..] %s: cmd_nr = 0x%x\n",
			__func__, _IOC_NR(cmd));

	if (probe_ok == 0)
		return ret;

	if (pq_load_en == 0) {
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
	case AMVECM_IOC_S_RGB_OGO:
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
		if (!wb_en)
			return -EINVAL;

		if (copy_to_user((void __user *)arg,
				 &video_rgb_ogo, sizeof(struct tcon_rgb_ogo_s)))
			ret = -EFAULT;

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
			pr_amvecm_dbg("[amvecm..] ovescan copy fail!!\n");
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
		argp = hdr_tone_mapping.tm_lut;
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
		hdr_tone_mapping_get(hdr_tone_mapping.lutlength, hdr_tm);
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
			ret =
			amvecm_set_saturation_hue(vdj_mode_s.saturation_hue);
		}
		if (vdj_mode_flg & VDJ_FLAG_SAT_HUE_POST) {
			/*saturation_hue_post*/
			int sat_post, hue_post, sat_hue_post;

			sat_hue_post = vdj_mode_s.saturation_hue;
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
	default:
		ret = -EINVAL;
		break;
	}
	kfree(vpp_pq_load_table);

	kfree(hdr_tm);
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

static ssize_t amvecm_dnlp_debug_show(struct class *cla,
				      struct class_attribute *attr,
				      char *buf)
{
	return 0;
}

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

	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig) {
		kfree(stemp);
		return -ENOMEM;
	}
	parse_param_amvecm(buf_orig, (char **)&parm);

	if (!dnlp_insmod_ok) {
		pr_info("dnlp insmod fial\n");
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
					pr_info("%d\n",
						*dnlp_parse_cmd[i].value);
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
					pr_info("%s\n", stemp);
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
					pr_info("%s\n", stemp);
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
					pr_info("%s\n", stemp);
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
					pr_info("%s\n", stemp);
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
					pr_info("%s\n", stemp);
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
					pr_info("%s\n", stemp);
			} else {
				if (kstrtoul(parm[2], 10, &val) < 0)
					goto free_buf;

				if (val > 48 || val < 0)
					pr_info("error cmd\n");
				else
					pr_info("%d\n",
						gain_var_lut49_copy[val]);
			}
		} else if (!strcmp(parm[1], "wext_gain")) {
			if (!parm[2]) {
				pr_info("error cmd\n");
				goto free_buf;
			} else if (!strcmp(parm[2], "all")) {
				for (i = 0; i < 48; i++)
					d_convert_str(wext_gain_copy[i],
						      i, stemp, 4, 10);
					pr_info("%s\n", stemp);
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
					pr_info("%s\n", stemp);
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
					pr_info("%s\n", stemp);
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
					pr_info("%s\n", stemp);
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
					pr_info("%s\n", stemp);
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
					pr_info("%s\n", stemp);
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
					pr_info("%s\n", stemp);
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
					pr_info("%s\n", stemp);
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
					pr_info("%s\n", stemp);
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
					pr_info("%s\n", stemp);
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
					pr_info("%s\n", stemp);
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
					pr_info("%s\n", stemp);
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
					pr_info("%s\n", stemp);
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
					p[i] = curve_val[i];
			} else {
				if (kstrtoul(parm[2], 10, &val) < 0)
					goto free_buf;

				num = val;
				if (kstrtoul(parm[3], 10, &val) < 0)
					goto free_buf;

				if (num > 8)
					pr_info("error cmd\n");
				else
					p[num] = val;
			}
		}
	} else if (!strcmp(parm[0], "ro")) {
		if (!strcmp(parm[1], "luma_avg4"))
			pr_info("%d\n", *ro_luma_avg4_copy);
		else if (!strcmp(parm[1], "var_d8"))
			pr_info("%d\n", *ro_var_d8_copy);
		else if (!strcmp(parm[1], "scurv_gain"))
			pr_info("%d\n", *ro_scurv_gain_copy);
		else if (!strcmp(parm[1], "blk_wht_ext0"))
			pr_info("%d\n", *ro_blk_wht_ext0_copy);
		else if (!strcmp(parm[1], "blk_wht_ext1"))
			pr_info("%d\n", *ro_blk_wht_ext1_copy);
		else if (!strcmp(parm[1], "dnlp_brightness"))
			pr_info("%d\n", *ro_dnlp_brightness_copy);
		else
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
	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A)
		WRITE_VPP_REG(VPP_VADJ1_MA_MB_2, mab);
	else
		WRITE_VPP_REG(VPP_VADJ1_MA_MB, mab);
	mc = (s16)((mab << 22) >> 22); /* mc = -mb */
	mc = 0 - mc;
	if (mc > 511)
		mc = 511;
	if (mc < -512)
		mc = -512;
	md = (s16)((mab << 6) >> 22);  /* md =	ma; */
	mab = ((mc & 0x3ff) << 16) | (md & 0x3ff);

	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A) {
		WRITE_VPP_REG(VPP_VADJ1_MC_MD_2, mab);
		WRITE_VPP_REG_BITS(VPP_VADJ1_MISC, 1, 0, 1);
	} else {
		WRITE_VPP_REG(VPP_VADJ1_MC_MD, mab);
		WRITE_VPP_REG_BITS(VPP_VADJ_CTRL, 1, 0, 1);
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

	if (!buffer)
		return count;
	buf_orig = kstrdup(buffer, GFP_KERNEL);
	if (!buf_orig)
		return -ENOMEM;
	parse_param_amvecm(buf_orig, (char **)&parm);

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

static ssize_t amvecm_gamma_show(struct class *cls,
				 struct class_attribute *attr,
			char *buf)
{
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
	char *parm[4];
	unsigned short *gamma_r, *gamma_g, *gamma_b;
	unsigned int gamma_count;
	char gamma[4];
	int i = 0;
	long val;
	char delim1[3] = " ";
	char delim2[2] = "\n";
	char *stemp = NULL;

	stemp = kmalloc(600, GFP_KERNEL);
	if (!stemp)
		return -ENOMEM;

	gamma_r = kmalloc(256 * sizeof(unsigned short), GFP_KERNEL);
	if (!gamma_r) {
		kfree(stemp);
		return -ENOMEM;
	}
	gamma_g = kmalloc(256 * sizeof(unsigned short), GFP_KERNEL);
	if (!gamma_g) {
		kfree(stemp);
		kfree(gamma_r);
		return -ENOMEM;
	}
	gamma_b = kmalloc(256 * sizeof(unsigned short), GFP_KERNEL);
	if (!gamma_b) {
		kfree(stemp);
		kfree(gamma_r);
		kfree(gamma_g);
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
	if (!gamma_r || !gamma_g || !gamma_b || !stemp || n == 0)
		goto free_buf;

	if ((parm[0][0] == 's') && (parm[0][1] == 'g')) {
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
	} else if (!strcmp(parm[0], "ggg")) {
		vpp_get_lcd_gamma_table(H_SEL_G);
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

	} else if (!strcmp(parm[0], "ggb")) {
		vpp_get_lcd_gamma_table(H_SEL_B);
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
	kfree(gamma_r);
	kfree(gamma_g);
	kfree(gamma_b);
	return count;
free_buf:
	kfree(buf_orig);
	kfree(stemp);
	kfree(gamma_r);
	kfree(gamma_g);
	kfree(gamma_b);
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
	static unsigned short r_val[256], g_val[256], b_val[256];
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

	for (i = 0; i < 256; i++) {
		r_val[i] = gamma[0];
		g_val[i] = gamma[1];
		b_val[i] = gamma[2];
	}

	amve_write_gamma_table(r_val, H_SEL_R);
	amve_write_gamma_table(g_val, H_SEL_G);
	amve_write_gamma_table(b_val, H_SEL_B);

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

static ssize_t amvecm_wb_show(struct class *cla,
			      struct class_attribute *attr, char *buf)
{
	pr_info("read:	echo r gain_r > /sys/class/amvecm/wb\n");
	pr_info("read:	echo r pre_r > /sys/class/amvecm/wb\n");
	pr_info("read:	echo r post_r > /sys/class/amvecm/wb\n");
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
		if (!strncmp(parm[1], "pre_r", 5))
			pr_info("\t Pre_R = %d\n", video_rgb_ogo.r_pre_offset);
		else if (!strncmp(parm[1], "pre_g", 5))
			pr_info("\t Pre_G = %d\n", video_rgb_ogo.g_pre_offset);
		else if (!strncmp(parm[1], "pre_b", 5))
			pr_info("\t Pre_B = %d\n", video_rgb_ogo.b_pre_offset);
		else if (!strncmp(parm[1], "gain_r", 6))
			pr_info("\t Gain_R = %d\n", video_rgb_ogo.r_gain);
		else if (!strncmp(parm[1], "gain_g", 6))
			pr_info("\t Gain_G = %d\n", video_rgb_ogo.g_gain);
		else if (!strncmp(parm[1], "gain_b", 6))
			pr_info("\t Gain_B = %d\n", video_rgb_ogo.b_gain);
		else if (!strncmp(parm[1], "post_r", 6))
			pr_info("\t Post_R = %d\n",
				video_rgb_ogo.r_post_offset);
		else if (!strncmp(parm[1], "post_g", 6))
			pr_info("\t Post_G = %d\n",
				video_rgb_ogo.g_post_offset);
		else if (!strncmp(parm[1], "post_b", 6))
			pr_info("\t Post_B = %d\n",
				video_rgb_ogo.b_post_offset);
		else if (!strncmp(parm[1], "en", 2))
			pr_info("\t En = %d\n", video_rgb_ogo.en);
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

	val = READ_VPP_REG(VPP_MATRIX_CTRL);
	pr_info("current setting: %d\n", (val >> 10) & 0x3f);

	return 0;
}

static ssize_t amvecm_set_post_matrix_store(struct class *cla,
					    struct class_attribute *attr,
					    const char *buf, size_t count)
{
	int val, reg_val;

	if (kstrtoint(buf, 10, &val) < 0)
		return -EINVAL;

	reg_val = READ_VPP_REG(VPP_MATRIX_CTRL);
	reg_val = reg_val & 0xffff03ff;
	reg_val = reg_val | ((val & 0x3f) << 10);

	WRITE_VPP_REG(VPP_MATRIX_CTRL, reg_val);

	pr_info("VPP_MATRIX_CTRL is set\n");
	return count;
}

static ssize_t amvecm_post_matrix_pos_show(struct class *cla,
					   struct class_attribute *attr,
					   char *buf)
{
	int val;

	pr_info("Usage:\n");
	pr_info("echo x y > /sys/class/amvecm/matrix_pos\n");

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

	reg_val = READ_VPP_REG(VPP_MATRIX_PROBE_POS);
	reg_val = reg_val & 0xe000e000;
	reg_val = reg_val | (val_x << 16) | val_y;

	WRITE_VPP_REG(VPP_MATRIX_PROBE_POS, reg_val);

	kfree(buf_orig);
	return count;
}

static ssize_t amvecm_post_matrix_data_show(struct class *cla,
					    struct class_attribute *attr,
					    char *buf)
{
	int len = 0, val1 = 0, val2 = 0;

	val1 = READ_VPP_REG(VPP_MATRIX_PROBE_COLOR);
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A)) {
		len += sprintf(buf + len,
		"VPP_MATRIX_PROBE_COLOR %d, %d, %d\n",
		(val1 >> 20) & 0x3ff,
		(val1 >> 10) & 0x3ff,
		(val1 >> 0) & 0x3ff);
	} else {
		val2 = READ_VPP_REG(VPP_MATRIX_PROBE_COLOR1);
		len += sprintf(buf + len,
		"VPP_MATRIX_PROBE_COLOR %x, %x, %x\n",
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
				READ_VPP_REG(addr));
	if (is_meson_txl_cpu() || is_meson_txlx_cpu()) {
		for (addr = 0x3265;
			addr <= 0x3272; addr++)
			pr_info("[0x%x]vcbus[0x%04x]=0x%08x\n",
				(base_reg + (addr << 2)), addr,
					READ_VPP_REG(addr));
	}
	if (is_meson_txlx_cpu()) {
		for (addr = 0x3273;
			addr <= 0x327f; addr++)
			pr_info("[0x%x]vcbus[0x%04x]=0x%08x\n",
				(base_reg + (addr << 2)), addr,
					READ_VPP_REG(addr));
	}
	pr_info("----dump sharpness1 reg----\n");
	for (addr = (0x3200 + 0x80);
		addr <= (0x3264 + 0x80); addr++)
		pr_info("[0x%x]vcbus[0x%04x]=0x%08x\n",
			(base_reg + (addr << 2)), addr,
				READ_VPP_REG(addr));
	if (is_meson_txl_cpu() || is_meson_txlx_cpu()) {
		for (addr = (0x3265 + 0x80);
			addr <= (0x3272 + 0x80); addr++)
			pr_info("[0x%x]vcbus[0x%04x]=0x%08x\n",
				(base_reg + (addr << 2)), addr,
					READ_VPP_REG(addr));
	}
	if (is_meson_txlx_cpu()) {
		for (addr = (0x3273 + 0x80);
			addr <= (0x327f + 0x80); addr++)
			pr_info("[0x%x]vcbus[0x%04x]=0x%08x\n",
				(base_reg + (addr << 2)), addr,
					READ_VPP_REG(addr));
	}
	pr_info("----dump cm reg----\n");
	for (addr = 0x200; addr <= 0x21e; addr++) {
		WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT, addr);
		value = READ_VPP_REG(VPP_CHROMA_DATA_PORT);
		pr_info("[0x%x]vcbus[0x%04x]=0x%08x\n",
			addr, addr,
				value);
	}
	for (addr = 0x100; addr <= 0x1fc; addr++) {
		WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT, addr);
		value = READ_VPP_REG(VPP_CHROMA_DATA_PORT);
		pr_info("[0x%x]vcbus[0x%04x]=0x%08x\n",
			addr, addr,
				value);
	}

	pr_info("----dump vd1 IF0 reg----\n");
	for (addr = (0x1a50);
		addr <= (0x1a69); addr++)
		pr_info("[0x%x]vcbus[0x%04x]=0x%08x\n",
			(base_reg + (addr << 2)), addr,
				READ_VPP_REG(addr));
	pr_info("----dump vpp1 part1 reg----\n");
	for (addr = (0x1d00);
		addr <= (0x1d6e); addr++)
		pr_info("[0x%x]vcbus[0x%04x]=0x%08x\n",
			(base_reg + (addr << 2)), addr,
				READ_VPP_REG(addr));

	pr_info("----dump vpp1 part2 reg----\n");
	for (addr = (0x1d72);
		addr <= (0x1de4); addr++)
		pr_info("[0x%x]vcbus[0x%04x]=0x%08x\n",
			(base_reg + (addr << 2)), addr,
				READ_VPP_REG(addr));

	pr_info("----dump ndr reg----\n");
	for (addr = (0x2d00);
		addr <= (0x2d78); addr++)
		pr_info("[0x%x]vcbus[0x%04x]=0x%08x\n",
			(base_reg + (addr << 2)), addr,
				READ_VPP_REG(addr));
	pr_info("----dump nr3 reg----\n");
	for (addr = (0x2ff0);
		addr <= (0x2ff6); addr++)
		pr_info("[0x%x]vcbus[0x%04x]=0x%08x\n",
			(base_reg + (addr << 2)), addr,
				READ_VPP_REG(addr));
	pr_info("----dump vlock reg----\n");
	for (addr = (0x3000);
		addr <= (0x3020); addr++)
		pr_info("[0x%x]vcbus[0x%04x]=0x%08x\n",
			(base_reg + (addr << 2)), addr,
				READ_VPP_REG(addr));
	pr_info("----dump super scaler0 reg----\n");
	for (addr = (0x3100);
		addr <= (0x3115); addr++)
		pr_info("[0x%x]vcbus[0x%04x]=0x%08x\n",
			(base_reg + (addr << 2)), addr,
				READ_VPP_REG(addr));
	pr_info("----dump super scaler1 reg----\n");
	for (addr = (0x3118);
		addr <= (0x312e); addr++)
		pr_info("[0x%x]vcbus[0x%04x]=0x%08x\n",
			(base_reg + (addr << 2)), addr,
				READ_VPP_REG(addr));
	pr_info("----dump xvycc reg----\n");
	for (addr = (0x3158);
		addr <= (0x3179); addr++)
		pr_info("[0x%x]vcbus[0x%04x]=0x%08x\n",
			(base_reg + (addr << 2)), addr,
				READ_VPP_REG(addr));
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
	} else if (!strncmp(parm[0], "force_clip", 10)) {
		if (kstrtoul(parm[1], 16, &val) < 0)
			goto free_buf;
		hdr10_force_clip = val;
		pr_info("hdr10_force_clip=0x%x\n", hdr10_force_clip);
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
	} else if (!strncmp(parm[0], "clip_mode", 9)) {
		if (kstrtoul(parm[1], 16, &val) < 0)
			goto free_buf;
		hdr10_clip_mode = val;
		pr_info("hdr10_clip_mode=0x%x\n", hdr10_clip_mode);
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
	} else {
		pr_info("error cmd\n");
	}

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

	if (val == 1) {
		pc_mode = 1;
		pc_mode_last = 0xff;
	} else if (val == 0) {
		pc_mode = 0;
		pc_mode_last = 0xff;
	}

	return count;
}

void pc_mode_process(void)
{
	unsigned int reg_val;

	if (pc_mode == 1 && pc_mode != pc_mode_last) {
		/* open dnlp clock gate */
		dnlp_en = 1;
		lc_en = 1;
		ve_enable_dnlp();
		/* open cm clock gate */
		if (!(is_meson_g12a_cpu() || is_meson_g12b_cpu() ||
		      is_meson_sm1_cpu()))
			cm_en = 1;
			/* sharpness on */
		VSYNC_WR_MPEG_REG_BITS(SRSHARP0_PK_NR_ENABLE + sr_offset[0],
				       1, 1, 1);
		VSYNC_WR_MPEG_REG_BITS(SRSHARP1_PK_NR_ENABLE + sr_offset[1],
				       1, 1, 1);
		reg_val = VSYNC_RD_MPEG_REG(SRSHARP0_HCTI_FLT_CLP_DC
			+ sr_offset[0]);
		VSYNC_WR_MPEG_REG(SRSHARP0_HCTI_FLT_CLP_DC + sr_offset[0],
				  reg_val | 0x10000000);
		VSYNC_WR_MPEG_REG(SRSHARP1_HCTI_FLT_CLP_DC + sr_offset[1],
				  reg_val | 0x10000000);

		reg_val = VSYNC_RD_MPEG_REG(SRSHARP0_HLTI_FLT_CLP_DC +
					sr_offset[0]);
		VSYNC_WR_MPEG_REG(SRSHARP0_HLTI_FLT_CLP_DC + sr_offset[0],
				  reg_val | 0x10000000);
		VSYNC_WR_MPEG_REG(SRSHARP1_HLTI_FLT_CLP_DC + sr_offset[1],
				  reg_val | 0x10000000);

		reg_val = VSYNC_RD_MPEG_REG(SRSHARP0_VLTI_FLT_CON_CLP +
					sr_offset[0]);
		VSYNC_WR_MPEG_REG(SRSHARP0_VLTI_FLT_CON_CLP + sr_offset[0],
				  reg_val | 0x4000);
		VSYNC_WR_MPEG_REG(SRSHARP1_VLTI_FLT_CON_CLP + sr_offset[1],
				  reg_val | 0x4000);

		reg_val = VSYNC_RD_MPEG_REG(SRSHARP0_VCTI_FLT_CON_CLP +
					sr_offset[0]);
		VSYNC_WR_MPEG_REG(SRSHARP0_VCTI_FLT_CON_CLP + sr_offset[0],
				  reg_val | 0x4000);
		VSYNC_WR_MPEG_REG(SRSHARP1_VCTI_FLT_CON_CLP + sr_offset[1],
				  reg_val | 0x4000);

		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXL)) {
			VSYNC_WR_MPEG_REG_BITS(SRSHARP0_DEJ_CTRL + sr_offset[0],
					       1, 0, 1);
			VSYNC_WR_MPEG_REG_BITS(SRSHARP0_SR3_DRTLPF_EN
				+ sr_offset[0], 7, 0, 3);
			VSYNC_WR_MPEG_REG_BITS(SRSHARP0_SR3_DERING_CTRL
				+ sr_offset[0], 1, 28, 3);

			VSYNC_WR_MPEG_REG_BITS(SRSHARP1_DEJ_CTRL + sr_offset[1],
					       1, 0, 1);
			VSYNC_WR_MPEG_REG_BITS(SRSHARP1_SR3_DRTLPF_EN
				+ sr_offset[1], 7, 0, 3);
			VSYNC_WR_MPEG_REG_BITS(SRSHARP1_SR3_DERING_CTRL
				+ sr_offset[1], 1, 28, 3);
		}

		if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A)
			WRITE_VPP_REG_BITS(VPP_VADJ1_MISC, 1, 0, 1);
		else
			VSYNC_WR_MPEG_REG(VPP_VADJ_CTRL, 0xd);
		pc_mode_last = pc_mode;
	} else if ((pc_mode == 0) && (pc_mode != pc_mode_last)) {
		dnlp_en = 0;
		lc_en = 0;
		ve_disable_dnlp();
		cm_en = 0;

		VSYNC_WR_MPEG_REG_BITS(SRSHARP0_PK_NR_ENABLE + sr_offset[0],
				       0, 1, 1);
		VSYNC_WR_MPEG_REG_BITS(SRSHARP1_PK_NR_ENABLE + sr_offset[1],
				       0, 1, 1);
		reg_val = VSYNC_RD_MPEG_REG(SRSHARP0_HCTI_FLT_CLP_DC
			+ sr_offset[0]);
		VSYNC_WR_MPEG_REG(SRSHARP0_HCTI_FLT_CLP_DC + sr_offset[0],
				  reg_val & 0xefffffff);
		VSYNC_WR_MPEG_REG(SRSHARP1_HCTI_FLT_CLP_DC + sr_offset[1],
				  reg_val & 0xefffffff);

		reg_val = VSYNC_RD_MPEG_REG(SRSHARP0_HLTI_FLT_CLP_DC
			+ sr_offset[0]);
		VSYNC_WR_MPEG_REG(SRSHARP0_HLTI_FLT_CLP_DC + sr_offset[0],
				  reg_val & 0xefffffff);
		VSYNC_WR_MPEG_REG(SRSHARP1_HLTI_FLT_CLP_DC + sr_offset[1],
				  reg_val & 0xefffffff);

		reg_val = VSYNC_RD_MPEG_REG(SRSHARP0_VLTI_FLT_CON_CLP
			+ sr_offset[0]);
		VSYNC_WR_MPEG_REG(SRSHARP0_VLTI_FLT_CON_CLP + sr_offset[0],
				  reg_val & 0xffffbfff);
		VSYNC_WR_MPEG_REG(SRSHARP1_VLTI_FLT_CON_CLP + sr_offset[1],
				  reg_val & 0xffffbfff);

		reg_val = VSYNC_RD_MPEG_REG(SRSHARP0_VCTI_FLT_CON_CLP
			+ sr_offset[0]);
		VSYNC_WR_MPEG_REG(SRSHARP0_VCTI_FLT_CON_CLP + sr_offset[0],
				  reg_val & 0xffffbfff);
		VSYNC_WR_MPEG_REG(SRSHARP1_VCTI_FLT_CON_CLP + sr_offset[1],
				  reg_val & 0xffffbfff);

		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXL)) {
			VSYNC_WR_MPEG_REG_BITS(SRSHARP0_DEJ_CTRL + sr_offset[0],
					       0, 0, 1);
			VSYNC_WR_MPEG_REG_BITS(SRSHARP0_SR3_DRTLPF_EN
				+ sr_offset[0], 0, 0, 3);
			VSYNC_WR_MPEG_REG_BITS(SRSHARP0_SR3_DERING_CTRL
				+ sr_offset[0], 0, 28, 3);

			VSYNC_WR_MPEG_REG_BITS(SRSHARP1_DEJ_CTRL + sr_offset[1],
					       0, 0, 1);
			VSYNC_WR_MPEG_REG_BITS(SRSHARP1_SR3_DRTLPF_EN
				+ sr_offset[1], 0, 0, 3);
			VSYNC_WR_MPEG_REG_BITS(SRSHARP1_SR3_DERING_CTRL
				+ sr_offset[1], 0, 28, 3);
		}

		if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A)
			WRITE_VPP_REG_BITS(VPP_VADJ1_MISC, 0, 0, 1);
		else
			VSYNC_WR_MPEG_REG(VPP_VADJ_CTRL, 0x0);

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
		WRITE_VPP_REG_BITS(SRSHARP0_PK_NR_ENABLE + sr_offset[0],
				   1, 1, 1);
	else
		WRITE_VPP_REG_BITS(SRSHARP0_PK_NR_ENABLE + sr_offset[0],
				   0, 1, 1);
}

void amvecm_sr1_pk_enable(unsigned int enable)
{
	if (enable)
		WRITE_VPP_REG_BITS(SRSHARP1_PK_NR_ENABLE + sr_offset[1],
				   1, 1, 1);
	else
		WRITE_VPP_REG_BITS(SRSHARP1_PK_NR_ENABLE + sr_offset[1],
				   0, 1, 1);
}

void amvecm_sr0_dering_enable(unsigned int enable)
{
	if (enable)
		WRITE_VPP_REG_BITS(SRSHARP0_SR3_DERING_CTRL + sr_offset[0],
				   1, 28, 3);
	else
		WRITE_VPP_REG_BITS(SRSHARP0_SR3_DERING_CTRL + sr_offset[0],
				   0, 28, 3);
}

void amvecm_sr1_dering_enable(unsigned int enable)
{
	if (enable)
		WRITE_VPP_REG_BITS(SRSHARP1_SR3_DERING_CTRL + sr_offset[1],
				   1, 28, 3);
	else
		WRITE_VPP_REG_BITS(SRSHARP1_SR3_DERING_CTRL + sr_offset[1],
				   0, 28, 3);
}

void amvecm_sr0_dejaggy_enable(unsigned int enable)
{
	if (enable)
		WRITE_VPP_REG_BITS(SRSHARP0_DEJ_CTRL + sr_offset[0],
				   1, 0, 1);
	else
		WRITE_VPP_REG_BITS(SRSHARP0_DEJ_CTRL + sr_offset[0],
				   0, 0, 1);
}

void amvecm_sr1_dejaggy_enable(unsigned int enable)
{
	if (enable)
		WRITE_VPP_REG_BITS(SRSHARP1_DEJ_CTRL + sr_offset[1],
				   1, 0, 1);
	else
		WRITE_VPP_REG_BITS(SRSHARP1_DEJ_CTRL + sr_offset[1],
				   0, 0, 1);
}

void amvecm_sr0_derection_enable(unsigned int enable)
{
	if (enable)
		WRITE_VPP_REG_BITS(SRSHARP0_SR3_DRTLPF_EN + sr_offset[0],
				   7, 0, 3);
	else
		WRITE_VPP_REG_BITS(SRSHARP0_SR3_DRTLPF_EN + sr_offset[0],
				   0, 0, 3);
}

void amvecm_sr1_derection_enable(unsigned int enable)
{
	if (enable)
		WRITE_VPP_REG_BITS(SRSHARP1_SR3_DRTLPF_EN + sr_offset[1],
				   7, 0, 3);
	else
		WRITE_VPP_REG_BITS(SRSHARP1_SR3_DRTLPF_EN + sr_offset[1],
				   0, 0, 3);
}

void pq_user_latch_process(void)
{
	int i = 0;

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
	} else if (pq_user_latch_flag & PQ_USER_SR0_DERECTION_EN) {
		pq_user_latch_flag &= ~PQ_USER_SR0_DERECTION_EN;
		amvecm_sr0_derection_enable(true);
	} else if (pq_user_latch_flag & PQ_USER_SR0_DERECTION_DIS) {
		pq_user_latch_flag &= ~PQ_USER_SR0_DERECTION_DIS;
		amvecm_sr0_derection_enable(false);
	} else if (pq_user_latch_flag & PQ_USER_SR1_DERECTION_EN) {
		pq_user_latch_flag &= ~PQ_USER_SR1_DERECTION_EN;
		amvecm_sr1_derection_enable(true);
	} else if (pq_user_latch_flag & PQ_USER_SR1_DERECTION_DIS) {
		pq_user_latch_flag &= ~PQ_USER_SR1_DERECTION_DIS;
		amvecm_sr1_derection_enable(false);
	} else if (pq_user_latch_flag & PQ_USER_CMS_CURVE_SAT ||
		   pq_user_latch_flag & PQ_USER_CMS_CURVE_LUMA ||
		   pq_user_latch_flag & PQ_USER_CMS_CURVE_HUE_HS ||
		   pq_user_latch_flag & PQ_USER_CMS_CURVE_HUE) {
		if (pq_user_latch_flag & PQ_USER_CMS_CURVE_SAT) {
			pq_user_latch_flag &= ~PQ_USER_CMS_CURVE_SAT;
			for (i = 0; i < ecm2colormd_max; i++) {
				if (cm2_sat_array[i][2] == 1) {
					cm2_curve_update_sat(i);
					cm2_sat_array[i][2] = 0;
				}
			}
		}
		if (pq_user_latch_flag & PQ_USER_CMS_CURVE_LUMA) {
			pq_user_latch_flag &= ~PQ_USER_CMS_CURVE_LUMA;
			for (i = 0; i < ecm2colormd_max; i++) {
				if (cm2_luma_array[i][2] == 1) {
					cm2_curve_update_luma(i);
					cm2_luma_array[i][2] = 0;
				}
			}
		}
		if (pq_user_latch_flag & PQ_USER_CMS_CURVE_HUE_HS) {
			pq_user_latch_flag &= ~PQ_USER_CMS_CURVE_HUE_HS;
			for (i = 0; i < ecm2colormd_max; i++) {
				if (cm2_hue_by_hs_array[i][2] == 1) {
					cm2_curve_update_hue_by_hs(i);
					cm2_hue_by_hs_array[i][2] = 0;
				}
			}
		}
		if (pq_user_latch_flag & PQ_USER_CMS_CURVE_HUE) {
			pq_user_latch_flag &= ~PQ_USER_CMS_CURVE_HUE;
			for (i = 0; i < ecm2colormd_max; i++) {
				if (cm2_hue_array[i][2] == 1) {
					cm2_curve_update_hue(i);
					cm2_hue_array[i][2] = 0;
				}
			}
		}
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
	"echo sr0_derec_en > /sys/class/amvecm/pq_user_set: sr0 drec en\n"
	"echo sr0_derec_dis > /sys/class/amvecm/pq_user_set: sr0 drec dis\n"
	"echo sr1_derec_en > /sys/class/amvecm/pq_user_set: sr1 drec en\n"
	"echo sr1_derec_dis > /sys/class/amvecm/pq_user_set: sr1 drec dis\n"

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
	} else if (!strncmp(parm[0], "sr0_derec_en", 12)) {
		pq_user_latch_flag |= PQ_USER_SR0_DERECTION_EN;
	} else if (!strncmp(parm[0], "sr0_derec_dis", 13)) {
		pq_user_latch_flag |= PQ_USER_SR0_DERECTION_DIS;
	} else if (!strncmp(parm[0], "sr1_derec_en", 12)) {
		pq_user_latch_flag |= PQ_USER_SR1_DERECTION_EN;
	} else if (!strncmp(parm[0], "sr1_derec_dis", 13)) {
		pq_user_latch_flag |= PQ_USER_SR1_DERECTION_DIS;
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
	return 0;
}

static ssize_t amvecm_vpp_demo_store(struct class *cla,
				     struct class_attribute *attr,
				     const char *buf, size_t count)
{
	size_t r;
	int val;

	r = sscanf(buf, "%x\n", &val);
	if (r != 1)
		return -EINVAL;

	if (val & VPP_DEMO_CM_EN)
		vpp_demo_latch_flag |= VPP_DEMO_CM_EN;
	else if (val & VPP_DEMO_CM_DIS)
		vpp_demo_latch_flag |= VPP_DEMO_CM_DIS;

	if (val & VPP_DEMO_DNLP_EN)
		vpp_demo_latch_flag |= VPP_DEMO_DNLP_EN;
	else if (val & VPP_DEMO_DNLP_DIS)
		vpp_demo_latch_flag |= VPP_DEMO_DNLP_DIS;

	return count;
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

static void amvecm_wb_enable(int enable)
{
	if (enable) {
		wb_en = 1;
		if (video_rgb_ogo_xvy_mtx)
			WRITE_VPP_REG_BITS(VPP_MATRIX_CTRL, 1, 6, 1);
		else
			WRITE_VPP_REG_BITS(VPP_GAINOFF_CTRL0, 1, 31, 1);
	} else {
		wb_en = 0;
		if (video_rgb_ogo_xvy_mtx)
			WRITE_VPP_REG_BITS(VPP_MATRIX_CTRL, 0, 6, 1);
		else
			WRITE_VPP_REG_BITS(VPP_GAINOFF_CTRL0, 0, 31, 1);
	}
}

void amvecm_sharpness_enable(int sel)
{
	/*0:peaking enable   1:peaking disable*/
	/*2:lti/cti enable   3:lti/cti disable*/
	switch (sel) {
	case 0:
		WRITE_VPP_REG_BITS(SRSHARP0_PK_NR_ENABLE + sr_offset[0],
				   1, 1, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_PK_NR_ENABLE + sr_offset[1],
				   1, 1, 1);
		break;
	case 1:
		WRITE_VPP_REG_BITS(SRSHARP0_PK_NR_ENABLE + sr_offset[0],
				   0, 1, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_PK_NR_ENABLE + sr_offset[1],
				   0, 1, 1);
		break;
	case 2:
		WRITE_VPP_REG_BITS(SRSHARP0_HCTI_FLT_CLP_DC + sr_offset[0],
				   1, 28, 1);
		WRITE_VPP_REG_BITS(SRSHARP0_HLTI_FLT_CLP_DC + sr_offset[0],
				   1, 28, 1);
		WRITE_VPP_REG_BITS(SRSHARP0_VLTI_FLT_CON_CLP + sr_offset[0],
				   1, 14, 1);
		WRITE_VPP_REG_BITS(SRSHARP0_VCTI_FLT_CON_CLP + sr_offset[0],
				   1, 14, 1);

		WRITE_VPP_REG_BITS(SRSHARP1_HCTI_FLT_CLP_DC + sr_offset[1],
				   1, 28, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_HLTI_FLT_CLP_DC + sr_offset[1],
				   1, 28, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_VLTI_FLT_CON_CLP + sr_offset[1],
				   1, 14, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_VCTI_FLT_CON_CLP + sr_offset[1],
				   1, 14, 1);
		break;
	case 3:
		WRITE_VPP_REG_BITS(SRSHARP0_HCTI_FLT_CLP_DC + sr_offset[0],
				   0, 28, 1);
		WRITE_VPP_REG_BITS(SRSHARP0_HLTI_FLT_CLP_DC + sr_offset[0],
				   0, 28, 1);
		WRITE_VPP_REG_BITS(SRSHARP0_VLTI_FLT_CON_CLP + sr_offset[0],
				   0, 14, 1);
		WRITE_VPP_REG_BITS(SRSHARP0_VCTI_FLT_CON_CLP + sr_offset[0],
				   0, 14, 1);

		WRITE_VPP_REG_BITS(SRSHARP1_HCTI_FLT_CLP_DC + sr_offset[1],
				   0, 28, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_HLTI_FLT_CLP_DC + sr_offset[1],
				   0, 28, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_VLTI_FLT_CON_CLP + sr_offset[1],
				   0, 14, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_VCTI_FLT_CON_CLP + sr_offset[1],
				   0, 14, 1);
		break;
	/*sr4 drtlpf theta en*/
	case 4:
		WRITE_VPP_REG_BITS(SRSHARP0_SR3_DRTLPF_EN + sr_offset[0],
				   7, 4, 3);
		WRITE_VPP_REG_BITS(SRSHARP1_SR3_DRTLPF_EN + sr_offset[1],
				   7, 3, 3);
		break;
	case 5:
		WRITE_VPP_REG_BITS(SRSHARP0_SR3_DRTLPF_EN + sr_offset[0],
				   0, 4, 3);
		WRITE_VPP_REG_BITS(SRSHARP1_SR3_DRTLPF_EN + sr_offset[1],
				   0, 3, 3);
		break;
	/*sr4 debanding en*/
	case 6:
		WRITE_VPP_REG_BITS(SRSHARP0_DB_FLT_CTRL + sr_offset[0],
				   1, 4, 1);
		WRITE_VPP_REG_BITS(SRSHARP0_DB_FLT_CTRL + sr_offset[0],
				   1, 5, 1);
		WRITE_VPP_REG_BITS(SRSHARP0_DB_FLT_CTRL + sr_offset[0],
				   1, 22, 1);
		WRITE_VPP_REG_BITS(SRSHARP0_DB_FLT_CTRL + sr_offset[0],
				   1, 23, 1);

		WRITE_VPP_REG_BITS(SRSHARP1_DB_FLT_CTRL + sr_offset[1],
				   1, 4, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_DB_FLT_CTRL + sr_offset[1],
				   1, 5, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_DB_FLT_CTRL + sr_offset[1],
				   1, 22, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_DB_FLT_CTRL + sr_offset[1],
				   1, 23, 1);
		break;
	case 7:
		WRITE_VPP_REG_BITS(SRSHARP0_DB_FLT_CTRL + sr_offset[0],
				   0, 4, 1);
		WRITE_VPP_REG_BITS(SRSHARP0_DB_FLT_CTRL + sr_offset[0],
				   0, 5, 1);
		WRITE_VPP_REG_BITS(SRSHARP0_DB_FLT_CTRL + sr_offset[0],
				   0, 22, 1);
		WRITE_VPP_REG_BITS(SRSHARP0_DB_FLT_CTRL + sr_offset[0],
				   0, 23, 1);

		WRITE_VPP_REG_BITS(SRSHARP1_DB_FLT_CTRL + sr_offset[1],
				   0, 4, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_DB_FLT_CTRL + sr_offset[1],
				   0, 5, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_DB_FLT_CTRL + sr_offset[1],
				   0, 22, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_DB_FLT_CTRL + sr_offset[1],
				   0, 23, 1);
		break;
	/*sr3 dejaggy en*/
	case 8:
		WRITE_VPP_REG_BITS(SRSHARP0_DEJ_CTRL + sr_offset[0],
				   1, 0, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_DEJ_CTRL + sr_offset[1],
				   1, 0, 1);
		break;
	case 9:
		WRITE_VPP_REG_BITS(SRSHARP0_DEJ_CTRL + sr_offset[0],
				   0, 0, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_DEJ_CTRL + sr_offset[1],
				   0, 0, 1);
		break;
	/*sr3 dering en*/
	case 10:
		WRITE_VPP_REG_BITS(SRSHARP0_SR3_DERING_CTRL + sr_offset[0],
				   1, 28, 3);
		WRITE_VPP_REG_BITS(SRSHARP1_SR3_DERING_CTRL + sr_offset[1],
				   1, 28, 3);
		break;
	case 11:
		WRITE_VPP_REG_BITS(SRSHARP0_SR3_DERING_CTRL + sr_offset[0],
				   0, 28, 3);
		WRITE_VPP_REG_BITS(SRSHARP1_SR3_DERING_CTRL + sr_offset[1],
				   0, 28, 3);
		break;
	/*sr3 derection lpf en*/
	case 12:
		WRITE_VPP_REG_BITS(SRSHARP0_SR3_DRTLPF_EN + sr_offset[0],
				   7, 0, 3);
		WRITE_VPP_REG_BITS(SRSHARP1_SR3_DRTLPF_EN + sr_offset[1],
				   7, 0, 3);
		break;
	case 13:
		WRITE_VPP_REG_BITS(SRSHARP0_SR3_DRTLPF_EN + sr_offset[0],
				   0, 0, 3);
		WRITE_VPP_REG_BITS(SRSHARP1_SR3_DRTLPF_EN + sr_offset[1],
				   0, 0, 3);
		break;

	default:
		break;
	}
}

void amvecm_sr_demo(int enable)
{
	if (enable) {
		sr_demo_flag = 1;
		WRITE_VPP_REG_BITS(SHARP0_DEMO_CRTL, 2, 17, 2);
		WRITE_VPP_REG_BITS(SHARP0_DEMO_CRTL, 1, 16, 1);
		WRITE_VPP_REG_BITS(SHARP0_DEMO_CRTL, 0x438, 0, 13);
		WRITE_VPP_REG_BITS(SHARP1_DEMO_CRTL, 2, 17, 2);
		WRITE_VPP_REG_BITS(SHARP1_DEMO_CRTL, 1, 16, 1);
		WRITE_VPP_REG_BITS(SHARP1_DEMO_CRTL, 0x438, 0, 13);

	} else {
		sr_demo_flag = 0;
		WRITE_VPP_REG_BITS(SHARP0_DEMO_CRTL, 2, 17, 2);
		WRITE_VPP_REG_BITS(SHARP0_DEMO_CRTL, 0, 16, 1);
		WRITE_VPP_REG_BITS(SHARP0_DEMO_CRTL, 0x438, 0, 13);
		WRITE_VPP_REG_BITS(SHARP1_DEMO_CRTL, 2, 17, 2);
		WRITE_VPP_REG_BITS(SHARP1_DEMO_CRTL, 0, 16, 1);
		WRITE_VPP_REG_BITS(SHARP1_DEMO_CRTL, 0x438, 0, 13);
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
		vecm_latch_flag |= FLAG_VE_DNLP_EN;
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
		if (!is_dolby_vision_enable())
#endif
			amcm_enable();
		WRITE_VPP_REG_BITS(SRSHARP0_PK_NR_ENABLE + sr_offset[0],
				   1, 1, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_PK_NR_ENABLE + sr_offset[1],
				   1, 1, 1);

		WRITE_VPP_REG_BITS(SRSHARP0_HCTI_FLT_CLP_DC + sr_offset[0],
				   1, 28, 1);
		WRITE_VPP_REG_BITS(SRSHARP0_HLTI_FLT_CLP_DC + sr_offset[0],
				   1, 28, 1);
		WRITE_VPP_REG_BITS(SRSHARP0_VLTI_FLT_CON_CLP + sr_offset[0],
				   1, 14, 1);
		WRITE_VPP_REG_BITS(SRSHARP0_VCTI_FLT_CON_CLP + sr_offset[0],
				   1, 14, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_HCTI_FLT_CLP_DC + sr_offset[1],
				   1, 28, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_HLTI_FLT_CLP_DC + sr_offset[1],
				   1, 28, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_VLTI_FLT_CON_CLP + sr_offset[1],
				   1, 14, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_VCTI_FLT_CON_CLP + sr_offset[1],
				   1, 14, 1);

		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXL)) {
			WRITE_VPP_REG_BITS(SRSHARP0_DEJ_CTRL + sr_offset[0],
					   1, 0, 1);
			WRITE_VPP_REG_BITS(SRSHARP0_SR3_DRTLPF_EN
				+ sr_offset[0], 7, 0, 3);
			WRITE_VPP_REG_BITS(SRSHARP0_SR3_DERING_CTRL
				+ sr_offset[0], 1, 28, 3);

			WRITE_VPP_REG_BITS(SRSHARP1_DEJ_CTRL + sr_offset[1],
					   1, 0, 1);
			WRITE_VPP_REG_BITS(SRSHARP1_SR3_DRTLPF_EN
				+ sr_offset[1], 7, 0, 3);
			WRITE_VPP_REG_BITS(SRSHARP1_SR3_DERING_CTRL
				+ sr_offset[1], 1, 28, 3);
		}
		/*sr4 drtlpf theta/ debanding en*/
		if (is_meson_txlx_cpu() || is_meson_txhd_cpu()) {
			WRITE_VPP_REG_BITS(SRSHARP0_SR3_DRTLPF_EN
				+ sr_offset[0], 7, 4, 3);

			WRITE_VPP_REG_BITS(SRSHARP0_DB_FLT_CTRL + sr_offset[0],
					   1, 4, 1);
			WRITE_VPP_REG_BITS(SRSHARP0_DB_FLT_CTRL + sr_offset[0],
					   1, 5, 1);
			WRITE_VPP_REG_BITS(SRSHARP0_DB_FLT_CTRL + sr_offset[0],
					   1, 22, 1);
			WRITE_VPP_REG_BITS(SRSHARP0_DB_FLT_CTRL + sr_offset[0],
					   1, 23, 1);

			WRITE_VPP_REG_BITS(SRSHARP1_DB_FLT_CTRL + sr_offset[1],
					   1, 4, 1);
			WRITE_VPP_REG_BITS(SRSHARP1_DB_FLT_CTRL + sr_offset[1],
					   1, 5, 1);
			WRITE_VPP_REG_BITS(SRSHARP1_DB_FLT_CTRL + sr_offset[1],
					   1, 22, 1);
			WRITE_VPP_REG_BITS(SRSHARP1_DB_FLT_CTRL + sr_offset[1],
					   1, 23, 1);
		}

		amvecm_wb_enable(true);

		vecm_latch_flag |= FLAG_GAMMA_TABLE_EN;
		if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A)
			WRITE_VPP_REG_BITS(VPP_VADJ1_MISC, 1, 0, 1);
		else
			WRITE_VPP_REG_BITS(VPP_VADJ_CTRL, 1, 0, 1);
	} else {
		vecm_latch_flag |= FLAG_VE_DNLP_DIS;

		amcm_disable();

		WRITE_VPP_REG_BITS(SRSHARP0_PK_NR_ENABLE + sr_offset[0],
				   0, 1, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_PK_NR_ENABLE + sr_offset[1],
				   0, 1, 1);

		WRITE_VPP_REG_BITS(SRSHARP0_HCTI_FLT_CLP_DC + sr_offset[0],
				   0, 28, 1);
		WRITE_VPP_REG_BITS(SRSHARP0_HLTI_FLT_CLP_DC + sr_offset[0],
				   0, 28, 1);
		WRITE_VPP_REG_BITS(SRSHARP0_VLTI_FLT_CON_CLP + sr_offset[0],
				   0, 14, 1);
		WRITE_VPP_REG_BITS(SRSHARP0_VCTI_FLT_CON_CLP + sr_offset[0],
				   0, 14, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_HCTI_FLT_CLP_DC + sr_offset[1],
				   0, 28, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_HLTI_FLT_CLP_DC + sr_offset[1],
				   0, 28, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_VLTI_FLT_CON_CLP + sr_offset[1],
				   0, 14, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_VCTI_FLT_CON_CLP + sr_offset[1],
				   0, 14, 1);

		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXL)) {
			WRITE_VPP_REG_BITS(SRSHARP0_DEJ_CTRL + sr_offset[0],
					   0, 0, 1);
			WRITE_VPP_REG_BITS(SRSHARP0_SR3_DRTLPF_EN
				+ sr_offset[0], 0, 0, 3);
			WRITE_VPP_REG_BITS(SRSHARP0_SR3_DERING_CTRL
				+ sr_offset[0], 0, 28, 3);

			WRITE_VPP_REG_BITS(SRSHARP1_DEJ_CTRL + sr_offset[1],
					   0, 0, 1);
			WRITE_VPP_REG_BITS(SRSHARP1_SR3_DRTLPF_EN
				+ sr_offset[1], 0, 0, 3);
			WRITE_VPP_REG_BITS(SRSHARP1_SR3_DERING_CTRL
				+ sr_offset[1], 0, 28, 3);
		}
		/*sr4 drtlpf theta/ debanding en*/
		if (is_meson_txlx_cpu()) {
			WRITE_VPP_REG_BITS(SRSHARP0_SR3_DRTLPF_EN
				+ sr_offset[0], 0, 4, 3);

			WRITE_VPP_REG_BITS(SRSHARP0_DB_FLT_CTRL + sr_offset[0],
					   0, 4, 1);
			WRITE_VPP_REG_BITS(SRSHARP0_DB_FLT_CTRL + sr_offset[0],
					   0, 5, 1);
			WRITE_VPP_REG_BITS(SRSHARP0_DB_FLT_CTRL + sr_offset[0],
					   0, 22, 1);
			WRITE_VPP_REG_BITS(SRSHARP0_DB_FLT_CTRL + sr_offset[0],
					   0, 23, 1);

			WRITE_VPP_REG_BITS(SRSHARP1_DB_FLT_CTRL + sr_offset[1],
					   0, 4, 1);
			WRITE_VPP_REG_BITS(SRSHARP1_DB_FLT_CTRL + sr_offset[1],
					   0, 5, 1);
			WRITE_VPP_REG_BITS(SRSHARP1_DB_FLT_CTRL + sr_offset[1],
					   0, 22, 1);
			WRITE_VPP_REG_BITS(SRSHARP1_DB_FLT_CTRL + sr_offset[1],
					   0, 23, 1);
		}

		amvecm_wb_enable(false);

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

static ssize_t amvecm_cm2_hue_show(struct class *cla,
				   struct class_attribute *attr,
				   char *buf)
{
	int i;
	int pos = 0;

	for (i = 0; i < ecm2colormd_max; i++)
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

	if (!buf)
		return count;
	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig)
		return -ENOMEM;
	parse_param_amvecm(buf_orig, (char **)&parm);
	if (!strncmp(parm[0], "cm2_hue", 7)) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto kfree_buf;

		color_mode = val;
		if (kstrtol(parm[2], 10, &val) < 0)
			goto kfree_buf;

		cm2_hue_array[color_mode][0] = val;
		if (kstrtoul(parm[3], 10, &val) < 0)
			goto kfree_buf;

		cm2_hue_array[color_mode][1] = val;
		cm2_hue_array[color_mode][2] = 1;
		cm2_hue(color_mode, cm2_hue_array[color_mode][0],
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

	for (i = 0; i < ecm2colormd_max; i++)
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

	if (!buf)
		return count;
	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig)
		return -ENOMEM;
	parse_param_amvecm(buf_orig, (char **)&parm);
	if (!strncmp(parm[0], "cm2_luma", 7)) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto kfree_buf;

		color_mode = val;
		if (kstrtol(parm[2], 10, &val) < 0)
			goto kfree_buf;

		cm2_luma_array[color_mode][0] = val;
		if (kstrtoul(parm[3], 10, &val) < 0)
			goto kfree_buf;

		cm2_luma_array[color_mode][1] = val;
		cm2_luma_array[color_mode][2] = 1;
		cm2_luma(color_mode, cm2_luma_array[color_mode][0],
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

	for (i = 0; i < ecm2colormd_max; i++)
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

	if (!buf)
		return count;
	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig)
		return -ENOMEM;
	parse_param_amvecm(buf_orig, (char **)&parm);
	if (!strncmp(parm[0], "cm2_sat", 7)) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto kfree_buf;

		color_mode = val;
		if (kstrtol(parm[2], 10, &val) < 0)
			goto kfree_buf;

		cm2_sat_array[color_mode][0] = val;
		if (kstrtoul(parm[3], 10, &val) < 0)
			goto kfree_buf;

		cm2_sat_array[color_mode][1] = val;
		cm2_sat_array[color_mode][2] = 1;
		cm2_sat(color_mode, cm2_sat_array[color_mode][0],
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

	for (i = 0; i < ecm2colormd_max; i++)
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

	if (!buf)
		return count;
	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig)
		return -ENOMEM;
	parse_param_amvecm(buf_orig, (char **)&parm);
	if (!strncmp(parm[0], "cm2_hue_by_hs", 7)) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto kfree_buf;

		color_mode = val;
		if (kstrtol(parm[2], 10, &val) < 0)
			goto kfree_buf;

		cm2_hue_by_hs_array[color_mode][0] = val;
		if (kstrtoul(parm[3], 10, &val) < 0)
			goto kfree_buf;

		cm2_hue_by_hs_array[color_mode][1] = val;
		cm2_hue_by_hs_array[color_mode][2] = 1;
		cm2_hue_by_hs(color_mode, cm2_hue_by_hs_array[color_mode][0],
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

static void get_cm_hist(enum cm_hist_e hist_sel)
{
	unsigned int *hist;
	unsigned int i;

	hist = kmalloc(32 * sizeof(unsigned int), GFP_KERNEL);
	memset(hist, 0, 32 * sizeof(unsigned int));

	switch (hist_sel) {
	case CM_HUE_HIST:
		for (i = 0; i < 32; i++) {
			WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT,
				      RO_CM_HUE_HIST_BIN0 + i);
			hist[i] = READ_VPP_REG(VPP_CHROMA_DATA_PORT);
			pr_info("hist[%d] = 0x%8x\n", i, hist[i]);
		}
		break;
	case CM_SAT_HIST:
		for (i = 0; i < 32; i++) {
			WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT,
				      RO_CM_SAT_HIST_BIN0 + i);
			hist[i] = READ_VPP_REG(VPP_CHROMA_DATA_PORT);
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
	"echo sr derec_en > /sys/class/amvecm/debug\n"
	"echo sr derec_dis > /sys/class/amvecm/debug\n"
	"echo sr theta_en > /sys/class/amvecm/debug\n"
	"echo sr theta_dis > /sys/class/amvecm/debug\n"
	"echo sr deband_en > /sys/class/amvecm/debug\n"
	"echo sr deband_dis > /sys/class/amvecm/debug\n"
	"echo cm enable > /sys/class/amvecm/debug\n"
	"echo cm disable > /sys/class/amvecm/debug\n"
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
	return sprintf(buf, "%s\n", amvecm_debug_usage_str);
}

static ssize_t amvecm_debug_store(struct class *cla,
				  struct class_attribute *attr,
				  const char *buf, size_t count)
{
	char *buf_orig, *parm[8] = {NULL};
	long val = 0;
	unsigned int mode_sel, color, color_mode, i;

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
			pr_info("loading new gamma without pretect");
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
		}  else if (!strncmp(parm[1], "demo", 4)) {
			if (!strncmp(parm[2], "enable", 6)) {
				amvecm_sr_demo(1);
				pr_info("sr demo enable\n");
			} else if (!strncmp(parm[2], "disable", 7)) {
				amvecm_sr_demo(0);
				pr_info("sr demo disable\n");
			}
		}
	} else if (!strcmp(parm[0], "cm")) {
		if (!strncmp(parm[1], "enable", 6)) {
			amcm_enable();
			pr_info("enable cm\n");
		} else if (!strncmp(parm[1], "disable", 7)) {
			amcm_disable();
			pr_info("disable cm\n");
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
			param2 = 0;
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
	} else if (!strcmp(parm[0], "3dlut_set")) {
		int *plut3d;
		unsigned int bitdepth;

		plut3d = kmalloc(14739 * sizeof(int), GFP_KERNEL);
		if (!plut3d) {
			kfree(plut3d);
			goto free_buf;
		}
		if (parm[1]) {
			if (kstrtoul(parm[1], 10, &val) < 0) {
				kfree(plut3d);
				goto free_buf;
			}
			bitdepth = val;
		} else {
			pr_info("unsupport cmd\n");
			kfree(plut3d);
			goto free_buf;
		}

		vpp_lut3d_table_init(plut3d, bitdepth);
		if (!strcmp(parm[2], "enable"))
			vpp_set_lut3d(1, 1, plut3d, 1);
		else if (!strcmp(parm[2], "disable"))
			vpp_set_lut3d(0, 0, plut3d, 0);
		else
			pr_info("unsupprt cmd!\n");
		kfree(plut3d);
	} else if (!strcmp(parm[0], "3dlut_dump")) {
		if (!strcmp(parm[1], "init_tab"))
			dump_plut3d_table();
		else if (!strcmp(parm[1], "reg_tab"))
			dump_plut3d_reg_table();
		else
			pr_info("unsupprt cmd!\n");
	} else if (!strcmp(parm[0], "cm_hist")) {
		if (!parm[1]) {
			pr_info("miss param1\n");
			goto free_buf;
		}
		if (!strcmp(parm[1], "hue"))
			get_cm_hist(CM_HUE_HIST);
		else if (!strcmp(parm[1], "sat"))
			get_cm_hist(CM_SAT_HIST);
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
	} else if (!strcmp(parm[0], "post2mtx")) {
		if (parm[1]) {
			if (kstrtoul(parm[1], 16, &val) < 0)
				goto free_buf;
			mtx_setting(POST2_MTX, val, 1);
		}
	} else {
		pr_info("unsupport cmd\n");
	}

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
		reg_val = READ_VPP_REG(reg_addr);
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
		WRITE_VPP_REG(reg_addr, reg_val);
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
				reg_val = READ_VPP_REG(reg_addr + i);
			} else if (parm[0][1] == 'c') {
				reg_val = aml_read_cbus(reg_addr + i);
			} else if (parm[0][1] == 'h') {
				amvecm_hiu_reg_read((reg_addr + i),
						    &reg_val);
			} else {
				pr_info("unsupprt cmd!\n");
				kfree(buf_orig);
				return -EINVAL;
			}
			pr_info("REG[0x%04x]=0x%08x\n",
				(reg_addr + i), reg_val);
		}
	} else {
		pr_info("unsupprt cmd!\n");
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

static void lc_rd_reg(enum lc_reg_lut_e reg_sel, int data_type)
{
	int i, tmp, tmp1, tmp2;
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
		break;
	case YPKBV_YMAXVAL_LMT:
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
		pr_info("%s\n", stemp);
		break;
	case YMINVAL_LMT:
		for (i = 0; i < 6 ; i++) {
			tmp = READ_VPP_REG(LC_CURVE_YMINVAL_LMT_0_1 + i);
			tmp1 = (tmp >> 16) & 0x3ff;
			tmp2 = tmp & 0x3ff;
			lut_data[2 * i] = tmp1;
			lut_data[2 * i + 1] = tmp2;
		}
		for (i = 0; i < 12 ; i++)
			d_convert_str(lut_data[i],
				      i, stemp, 4, 10);
		pr_info("%s\n", stemp);
		break;
	case YPKBV_YMAXVAL_LMT:
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
		pr_info("%s\n", stemp);
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
		pr_info("%s\n", stemp);
		break;
	default:
		break;
	}
	kfree(stemp);
}

static void lc_wr_reg(int *p, enum lc_reg_lut_e reg_sel)
{
	int i, tmp, tmp1, tmp2;

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
		break;
	case YPKBV_YMAXVAL_LMT:
		for (i = 0; i < 6 ; i++) {
			tmp1 = *(p + 2 * i);
			tmp2 = *(p + 2 * i + 1);
			tmp = ((tmp1 & 0x3ff) << 16) | (tmp2 & 0x3ff);
			WRITE_VPP_REG(LC_CURVE_YPKBV_YMAXVAL_LMT_0_1 + i, tmp);
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

unsigned int lc_saturation_curv[63];
unsigned int lc_yminval_lmt_curv[12];
unsigned int lc_ypkbv_ymaxval_lmt_curv[12];
unsigned int lc_ypkbv_ratio_curv[4];

void lc_load_curve(struct ve_lc_curve_parm_s *p)
{
	unsigned int i;

	/*load lc parms*/
	lc_alg_parm.dbg_parm0 = p->param[lc_dbg_parm0];
	lc_alg_parm.dbg_parm1 = p->param[lc_dbg_parm1];
	lc_alg_parm.dbg_parm2 = p->param[lc_dbg_parm2];
	lc_alg_parm.dbg_parm3 = p->param[lc_dbg_parm3];
	lc_alg_parm.dbg_parm4 = p->param[lc_dbg_parm4];

	/*load lc curve*/
	for (i = 0; i < 63; i++)
		lc_saturation_curv[i] = p->ve_lc_saturation[i];
	for (i = 0; i < 12; i++) {
		lc_yminval_lmt_curv[i] =
			p->ve_lc_yminval_lmt[i];
		lc_ypkbv_ymaxval_lmt_curv[i] =
			p->ve_lc_ypkbv_ymaxval_lmt[i];
	}
	for (i = 0; i < 4; i++)
		lc_ypkbv_ratio_curv[i] = p->ve_lc_ypkbv_ratio[i];

	/*load lc_staturation curve*/
	lc_wr_reg(lc_saturation_curv, 0x1);
	/*load lc_yminval_lmt*/
	lc_wr_reg(lc_yminval_lmt_curv, 0x2);
	/*load lc_ypkbv_ymaxval_lmt*/
	lc_wr_reg(lc_ypkbv_ymaxval_lmt_curv, 0x4);
	/*load lc_ypkbV_ratio*/
	lc_wr_reg(lc_ypkbv_ratio_curv, 0x8);
}

static ssize_t amvecm_lc_show(struct class *cla,
			      struct class_attribute *attr, char *buf)
{
	ssize_t len = 0;

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
	enum lc_reg_lut_e reg_sel;
	int h, v, i, start_point;
	long val = 0;
	char *stemp = NULL;
	int curve_val[6] = {0};

	if (!buf)
		return count;
	stemp = kzalloc(100, GFP_KERNEL);
	if (!stemp)
		return 0;

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
			pr_info("unsupprt cmd!\n");
	} else if (!strcmp(parm[0], "lc_version")) {
		pr_info("lc driver version :  %s\n", LC_VER);
	} else if (!strcmp(parm[0], "lc_dbg")) {
		if (kstrtoul(parm[1], 16, &val) < 0)
			goto free_buf;
		amlc_debug = val;
	} else if (!strcmp(parm[0], "lc_demo_mode")) {
		if (!strcmp(parm[1], "enable"))
			lc_demo_mode = 1;
		else if (!strcmp(parm[1], "disable"))
			lc_demo_mode = 0;
		else
			pr_info("unsupprt cmd!\n");
	} else if (!strcmp(parm[0], "dump_lut_data")) {
		if (kstrtoul(parm[1], 16, &val) < 0)
			goto free_buf;
		reg_sel = val;
		if (reg_sel == SATUR_LUT)
			lc_rd_reg(SATUR_LUT, 0);
		else if (reg_sel == YMINVAL_LMT)
			lc_rd_reg(YMINVAL_LMT, 0);
		else if (reg_sel == YPKBV_YMAXVAL_LMT)
			lc_rd_reg(YPKBV_YMAXVAL_LMT, 0);
		else if (reg_sel == YPKBV_RAT)
			lc_rd_reg(YPKBV_RAT, 0);
		else if (reg_sel == YPKBV_SLP_LMT)
			lc_rd_reg(YPKBV_SLP_LMT, 0);
		else if (reg_sel == CNTST_LMT)
			lc_rd_reg(CNTST_LMT, 0);
		else
			pr_info("unsupprt cmd!\n");
	} else if (!strcmp(parm[0], "dump_lut_str")) {
		if (kstrtoul(parm[1], 16, &val) < 0)
			goto free_buf;
		reg_sel = val;
		if (reg_sel == SATUR_LUT)
			lc_rd_reg(SATUR_LUT, 1);
		else if (reg_sel == YMINVAL_LMT)
			lc_rd_reg(YMINVAL_LMT, 1);
		else if (reg_sel == YPKBV_YMAXVAL_LMT)
			lc_rd_reg(YPKBV_YMAXVAL_LMT, 1);
		else if (reg_sel == YPKBV_RAT)
			lc_rd_reg(YPKBV_RAT, 1);
		else
			pr_info("unsupprt cmd!\n");
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
		else if (reg_sel == YPKBV_RAT)
			lc_wr_reg(reg_lut, YPKBV_RAT);
		else if (reg_sel == YPKBV_SLP_LMT)
			lc_wr_reg(reg_lut, YPKBV_SLP_LMT);
		else if (reg_sel == CNTST_LMT)
			lc_wr_reg(reg_lut, CNTST_LMT);
		else
			pr_info("unsupprt cmd!\n");
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
			pr_info("unsupprt cmd!\n");
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
			pr_info("unsupprt cmd!\n");
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
			pr_info("unsupprt cmd!\n");
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
		if (!strcmp(parm[1], "htotal"))
			pr_info("%d\n", h);
		else if (!strcmp(parm[1], "vtotal"))
			pr_info("%d\n", v);
		else
			pr_info("unsupprt cmd!\n");
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
		pr_info("%s\n", stemp);
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
		pr_info("%s\n", stemp);
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
		pr_info("unsupprt cmd!\n");
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
	VSYNC_WR_MPEG_REG(VD1_HDR2_HIST_CTRL, 0x5510);
	VSYNC_WR_MPEG_REG(VD1_HDR2_HIST_H_START_END, 0x10000);
	VSYNC_WR_MPEG_REG(VD1_HDR2_HIST_V_START_END, 0x0);

	VSYNC_WR_MPEG_REG(VD2_HDR2_HIST_CTRL, 0x5510);
	VSYNC_WR_MPEG_REG(VD2_HDR2_HIST_H_START_END, 0x10000);
	VSYNC_WR_MPEG_REG(VD2_HDR2_HIST_V_START_END, 0x0);

	VSYNC_WR_MPEG_REG(OSD1_HDR2_HIST_CTRL, 0x5510);
	VSYNC_WR_MPEG_REG(OSD1_HDR2_HIST_H_START_END, 0x10000);
	VSYNC_WR_MPEG_REG(OSD1_HDR2_HIST_V_START_END, 0x0);
}

/* #if (MESON_CPU_TYPE == MESON_CPU_TYPE_MESONG9TV) */
void init_pq_setting(void)
{
	int bitdepth;

	if (is_meson_gxtvbb_cpu() || is_meson_txl_cpu() ||
	    is_meson_txlx_cpu() || is_meson_txhd_cpu() ||
		is_meson_tl1_cpu() || is_meson_tm2_cpu())
		goto tvchip_pq_setting;
	else if (is_meson_g12a_cpu() || is_meson_g12b_cpu() ||
		 is_meson_sm1_cpu()) {
		sr_offset[0] = SR0_OFFSET;
		bitdepth = 12;
		/*confirm with vlsi-Lunhai.Chen, for G12A/G12B,
		 *VPP_GCLK_CTRL1 must enable
		 */
		WRITE_VPP_REG_BITS(VPP_GCLK_CTRL1, 0xf, 0, 4);
		cm_init_config(bitdepth);
		/*dnlp off*/
		WRITE_VPP_REG_BITS(VPP_VE_ENABLE_CTRL, 0,
				   DNLP_EN_BIT, DNLP_EN_WID);
		/*sr0  chroma filter bypass*/
		WRITE_VPP_REG(SRSHARP0_SHARP_SR2_CBIC_HCOEF0 + sr_offset[0],
			      0x4000);
		WRITE_VPP_REG(SRSHARP0_SHARP_SR2_CBIC_VCOEF0 + sr_offset[0],
			      0x4000);

		/*kernel sdr2hdr match uboot setting*/
		def_hdr_sdr_mode();
	}
	return;

tvchip_pq_setting:
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
		if (is_meson_tl1_cpu())
			bitdepth = 10;
		else if (is_meson_tm2_cpu())
			bitdepth = 12;
		else
			bitdepth = 12;
		/*sr0 & sr1 register shfit*/
		sr_offset[0] = SR0_OFFSET;
		sr_offset[1] = SR1_OFFSET;
		/*cm register init*/
		cm_init_config(bitdepth);
		/*lc init*/
		lc_init(bitdepth);

		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TM2))
			hdr_hist_config_int();
	}
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
	WRITE_VPP_REG_BITS(SRSHARP0_PK_NR_ENABLE + sr_offset[0],
			   0, 1, 1);
	WRITE_VPP_REG_BITS(SRSHARP1_PK_NR_ENABLE + sr_offset[1],
			   0, 1, 1);
	WRITE_VPP_REG_BITS(VPP_VE_ENABLE_CTRL, 0, DNLP_EN_BIT, DNLP_EN_WID);
	/*end*/
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXL)) {
		WRITE_VPP_REG_BITS(SRSHARP1_PK_FINALGAIN_HP_BP + sr_offset[1],
				   2, 16, 2);

		/*sr0 sr1 chroma filter bypass*/
		WRITE_VPP_REG(SRSHARP0_SHARP_SR2_CBIC_HCOEF0 + sr_offset[0],
			      0x4000);
		WRITE_VPP_REG(SRSHARP0_SHARP_SR2_CBIC_VCOEF0 + sr_offset[0],
			      0x4000);
		WRITE_VPP_REG(SRSHARP1_SHARP_SR2_CBIC_HCOEF0 + sr_offset[1],
			      0x4000);
		WRITE_VPP_REG(SRSHARP1_SHARP_SR2_CBIC_VCOEF0 + sr_offset[1],
			      0x4000);
	}
	if (is_meson_gxlx_cpu())
		amve_sharpness_init();

	/*dnlp alg parameters init*/
	dnlp_alg_param_init();
}

/* #endif*/

void amvecm_gamma_init(bool en)
{
	unsigned int i;
	static unsigned short data[256];

	for (i = 0; i < 256; i++) {
		data[i] = i << 2;
		video_gamma_table_r.data[i] = data[i];
		video_gamma_table_g.data[i] = data[i];
		video_gamma_table_b.data[i] = data[i];
	}

	if (en) {
		WRITE_VPP_REG_BITS(L_GAMMA_CNTL_PORT,
				   0, GAMMA_EN, 1);
		amve_write_gamma_table(data,
				       H_SEL_R);
		amve_write_gamma_table(data,
				       H_SEL_G);
		amve_write_gamma_table(data,
				       H_SEL_B);

		WRITE_VPP_REG_BITS(L_GAMMA_CNTL_PORT,
				   1, GAMMA_EN, 1);
	}
}

static void amvecm_wb_init(bool en)
{
	int *initcoef;

	initcoef = wb_init_bypass_coef;
	if (en) {
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
		} else {
			WRITE_VPP_REG(VPP_GAINOFF_CTRL0,
				      (1024 << 16) | 1024);
			WRITE_VPP_REG(VPP_GAINOFF_CTRL1,
				      (1024 << 16));
		}
	}

	if (video_rgb_ogo_xvy_mtx)
		WRITE_VPP_REG_BITS(VPP_MATRIX_CTRL, en, 6, 1);
	else
		WRITE_VPP_REG_BITS(VPP_GAINOFF_CTRL0, en, 31, 1);
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
	__ATTR(cm_reg, 0644,
	       amvecm_cm_reg_show,
		amvecm_cm_reg_store),
	__ATTR(gamma, 0644,
	       amvecm_gamma_show,
		amvecm_gamma_store),
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
	.vlk_support = true,
	.vlk_new_fsm = 0,
	.vlk_hwver = vlock_hw_org,
	.vlk_phlock_en = false,
	.vlk_pll_sel = vlock_pll_sel_tcon,
};

static const struct vecm_match_data_s vecm_dt_tl1 = {
	.vlk_support = true,
	.vlk_new_fsm = 1,
	.vlk_hwver = vlock_hw_ver2,
	.vlk_phlock_en = true,
	.vlk_pll_sel = vlock_pll_sel_tcon,
};

static const struct vecm_match_data_s vecm_dt_sm1 = {
	.vlk_support = false,
	.vlk_new_fsm = 1,
	.vlk_hwver = vlock_hw_ver2,
	.vlk_phlock_en = false,
	.vlk_pll_sel = vlock_pll_sel_tcon,
};

static const struct vecm_match_data_s vecm_dt_tm2 = {
	.vlk_support = false,
	.vlk_new_fsm = 1,
	.vlk_hwver = vlock_hw_ver2,
	.vlk_phlock_en = false,
	.vlk_pll_sel = vlock_pll_sel_hdmi,
};

static const struct of_device_id aml_vecm_dt_match[] = {
	{
		.compatible = "amlogic, vecm",
		.data = &vecm_dt_xxx,
	},
	{
		.compatible = "amlogic, vecm-tl1",
		.data = &vecm_dt_tl1,
	},
	{
		.compatible = "amlogic, vecm-sm1",
		.data = &vecm_dt_sm1,
	},
	{
		.compatible = "amlogic, vecm-tm2",
		.data = &vecm_dt_tm2,
	},
	{},
};

static void aml_vecm_dt_parse(struct platform_device *pdev)
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
			pr_info("Can't find  gamma_en.\n");
		else
			gamma_en = val;
		ret = of_property_read_u32(node, "wb_en", &val);
		if (ret)
			pr_info("Can't find  wb_en.\n");
		else
			wb_en = val;
		ret = of_property_read_u32(node, "cm_en", &val);
		if (ret)
			pr_info("Can't find  cm_en.\n");
		else
			cm_en = val;
		ret = of_property_read_u32(node, "detect_colorbar", &val);
		if (ret) {
			pr_info("Can't find  detect_colorbar.\n");
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
			pr_info("Can't find  detect_face.\n");
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
			pr_info("Can't find  detect_corn.\n");
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
			pr_info("Can't find  wb_sel.\n");
		else
			video_rgb_ogo_xvy_mtx = val;
		/*hdr:cfg:osd_100*/
		ret = of_property_read_u32(node, "cfg_en_osd_100", &val);
		if (ret) {
			hdr_set_cfg_osd_100(0);
			pr_info("hdr:Can't find  cfg_en_osd_100.\n");

		} else {
			hdr_set_cfg_osd_100((int)val);
		}
		ret = of_property_read_u32(node, "tx_op_color_primary", &val);
		if (ret)
			pr_info("Can't find  tx_op_color_primary.\n");
		else
			tx_op_color_primary = val;

		/*get compatible matched device, to get chip related data*/
		of_id = of_match_device(aml_vecm_dt_match, &pdev->dev);
		if (of_id) {
			pr_info("%s", of_id->compatible);
			matchdata = (struct vecm_match_data_s *)of_id->data;
		} else {
			matchdata = (struct vecm_match_data_s *)&vecm_dt_xxx;
			pr_info("unable to get matched device\n");
		}
		vlock_dt_match_init(matchdata);

		/*vlock param config*/
		vlock_param_config(node);

		vlock_status_init();
	}
	/* init module status */
#ifdef CONFIG_AMLOGIC_PIXEL_PROBE
	vpp_probe_enable();
#endif
	amvecm_wb_init(wb_en);
	amvecm_gamma_init(0);
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	if (!is_dolby_vision_enable())
#endif
		WRITE_VPP_REG_BITS(VPP_MISC, 1, 28, 1);
	if (cm_en) {
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
		if (!is_dolby_vision_enable())
#endif
			amcm_enable();
	} else {
		amcm_disable();
	}
	/* WRITE_VPP_REG_BITS(VPP_MISC, cm_en, 28, 1); */

	res_viu2_vsync_irq =
		platform_get_resource_byname(pdev, IORESOURCE_IRQ, "vsync2");
}

#ifdef CONFIG_AMLOGIC_LCD
static int aml_lcd_gamma_notifier(struct notifier_block *nb,
				  unsigned long event, void *data)
{
	if ((event & LCD_EVENT_GAMMA_UPDATE) == 0)
		return NOTIFY_DONE;

	vecm_latch_flag |= FLAG_GAMMA_TABLE_R;
	vecm_latch_flag |= FLAG_GAMMA_TABLE_G;
	vecm_latch_flag |= FLAG_GAMMA_TABLE_B;

	return NOTIFY_OK;
}

static struct notifier_block aml_lcd_gamma_nb = {
	.notifier_call = aml_lcd_gamma_notifier,
};
#endif

static struct notifier_block vlock_notifier_nb = {
	.notifier_call	= vlock_notify_callback,
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

static int aml_vecm_probe(struct platform_device *pdev)
{
	int ret = 0;
	int i = 0;

	struct amvecm_dev_s *devp = &amvecm_dev;

	memset(devp, 0, (sizeof(struct amvecm_dev_s)));
	pr_info("\n VECM probe start\n");
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
#ifdef CONFIG_AMLOGIC_LCD
	ret = aml_lcd_notifier_register(&aml_lcd_gamma_nb);
	if (ret)
		pr_info("register aml_lcd_gamma_notifier failed\n");

	INIT_WORK(&aml_lcd_vlock_param_work, vlock_lcd_param_work);
#endif
	/* register vout client */
	vout_register_client(&vlock_notifier_nb);

	init_pq_setting();
	init_pattern_detect();
	/* #endif */
	vpp_get_hist_en();

	if (is_meson_txlx_cpu()) {
		vpp_set_12bit_datapath2();
		/*post matrix 12bit yuv2rgb*/
		/* mtx_sel_dbg |= 1 << VPP_MATRIX_2; */
		/* amvecm_vpp_mtx_debug(mtx_sel_dbg, 1);*/
		WRITE_VPP_REG(VPP_MATRIX_PROBE_POS, 0x1fff1fff);
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
	if (is_meson_gxl_cpu() || is_meson_gxm_cpu())
		hdr_flag = (1 << 0) | (1 << 1) | (1 << 2) | (1 << 3);
	else
		hdr_flag = (1 << 0) | (1 << 1) | (0 << 2) | (0 << 3) | (1 << 4);

	hdr_init(&amvecm_dev.hdr_d);
	aml_vecm_dt_parse(pdev);

	aml_vecm_viu2_vsync_irq_init();

	probe_ok = 1;
	pr_info("%s: ok\n", __func__);
	return 0;

fail_create_device:
	pr_info("[amvecm.] : amvecm device create error.\n");
	cdev_del(&devp->cdev);
fail_add_cdev:
	pr_info("[amvecm.] : amvecm add device error.\n");
fail_class_create_file:
	pr_info("[amvecm.] : amvecm class create file error.\n");
	for (i = 0; amvecm_class_attrs[i].attr.name; i++) {
		class_remove_file(devp->clsp,
				  &amvecm_class_attrs[i]);
	}
	class_destroy(devp->clsp);
fail_create_class:
	pr_info("[amvecm.] : amvecm class create error.\n");
	unregister_chrdev_region(devp->devno, 1);
fail_alloc_region:
	pr_info("[amvecm.] : amvecm alloc error.\n");
	pr_info("[amvecm.] : amvecm_init.\n");
	return ret;
}

static int __exit aml_vecm_remove(struct platform_device *pdev)
{
	struct amvecm_dev_s *devp = &amvecm_dev;

	if (res_viu2_vsync_irq) {
		free_irq(res_viu2_vsync_irq->start,
			 (void *)"amvecm_vsync2");
	}

	hdr_exit();
	device_destroy(devp->clsp, devp->devno);
	cdev_del(&devp->cdev);
	class_destroy(devp->clsp);
	unregister_chrdev_region(devp->devno, 1);
#ifdef CONFIG_AMLOGIC_LCD
	aml_lcd_notifier_unregister(&aml_lcd_gamma_nb);
	cancel_work_sync(&aml_lcd_vlock_param_work);
#endif
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
	pr_info("amvecm: suspend module\n");
	return 0;
}

static int amvecm_drv_resume(struct platform_device *pdev)
{
	if (probe_ok == 0)
		probe_ok = 1;

	pr_info("amvecm: resume module\n");
	return 0;
}
#endif

static void amvecm_shutdown(struct platform_device *pdev)
{
	struct amvecm_dev_s *devp = &amvecm_dev;

	hdr_exit();
	ve_disable_dnlp();
	amcm_disable();
	WRITE_VPP_REG(VPP_VADJ_CTRL, 0x0);
	amvecm_wb_enable(0);
	/*dnlp cm vadj1 wb gate*/
	WRITE_VPP_REG(VPP_GCLK_CTRL0, 0x11000400);
	WRITE_VPP_REG(VPP_GCLK_CTRL1, 0x14);
	pr_info("amvecm: shutdown module\n");

	device_destroy(devp->clsp, devp->devno);
	cdev_del(&devp->cdev);
	class_destroy(devp->clsp);
	unregister_chrdev_region(devp->devno, 1);
#ifdef CONFIG_AML_LCD
	aml_lcd_notifier_unregister(&aml_lcd_gamma_nb);
#endif
	lc_free();
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

	pr_info("%s:module init\n", __func__);

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

#ifndef MODULE
module_init(aml_vecm_init);
module_exit(aml_vecm_exit);
#endif

//MODULE_VERSION(AMVECM_VER);
//MODULE_DESCRIPTION("AMLOGIC amvecm driver");
//MODULE_LICENSE("GPL");

