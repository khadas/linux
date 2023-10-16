// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * drivers/amlogic/media/enhancement/amvecm/amve.c
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

#include <linux/string.h>
#include <linux/spinlock.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/slab.h>
/* #include <mach/am_regs.h> */
#include <linux/amlogic/media/utils/amstream.h>
#include <linux/amlogic/media/amvecm/ve.h>
/* #include <linux/amlogic/aml_common.h> */
/* media module used media/registers/cpu_version.h since kernel 5.4 */
#include <linux/amlogic/media/registers/cpu_version.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/video_sink/video.h>
#include <linux/amlogic/media/amvecm/amvecm.h>
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
#include <linux/amlogic/media/amdolbyvision/dolby_vision.h>
#endif
#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/vout/lcd/lcd_unifykey.h>
#include "arch/vpp_regs.h"
#include "arch/ve_regs.h"
#include "arch/vpp_a4_regs.h"
#include "amve.h"
#include "amve_gamma_table.h"
#include <linux/io.h>
#include "dnlp_cal.h"
#include "local_contrast.h"
#include "amcm.h"
#include "reg_helper.h"
#include "amve_v2.h"

#define pr_amve_dbg(fmt, args...)\
	do {\
		if (amve_debug & 0x1)\
			pr_info("AMVE: " fmt, ## args);\
	} while (0)\
/* #define pr_amve_error(fmt, args...) */
/* printk(KERN_##(KERN_INFO) "AMVECM: " fmt, ## args) */

#define GAMMA_RETRY        1000
unsigned int gamma_loadprotect_en;
module_param(gamma_loadprotect_en, int, 0664);
MODULE_PARM_DESC(gamma_loadprotect_en, "gamma_loadprotect_en");

unsigned int gamma_update_flag_r;
module_param(gamma_update_flag_r, int, 0664);
MODULE_PARM_DESC(gamma_update_flag_r, "gamma_update_flag_r");

unsigned int gamma_update_flag_g;
module_param(gamma_update_flag_g, int, 0664);
MODULE_PARM_DESC(gamma_update_flag_g, "gamma_update_flag_g");

unsigned int gamma_update_flag_b;
module_param(gamma_update_flag_b, int, 0664);
MODULE_PARM_DESC(gamma_update_flag_b, "gamma_update_flag_b");

unsigned int gamma_disable_flag;
module_param(gamma_disable_flag, int, 0664);
MODULE_PARM_DESC(gamma_disable_flag, "gamma_disable_flag");

/* 0: Invalid */
/* 1: Valid */
/* 2: Updated in 2D mode */
/* 3: Updated in 3D mode */
unsigned long flags;

/* #if (MESON_CPU_TYPE>=MESON_CPU_TYPE_MESONG9TV) */
#define NEW_DNLP_IN_SHARPNESS 2
#define NEW_DNLP_IN_VPP 1

unsigned int dnlp_sel = NEW_DNLP_IN_SHARPNESS;
module_param(dnlp_sel, int, 0664);
MODULE_PARM_DESC(dnlp_sel, "dnlp_sel");
/* #endif */

static int amve_debug;
module_param(amve_debug, int, 0664);
MODULE_PARM_DESC(amve_debug, "amve_debug");

/*for fmeter en*/
int fmeter_en = 1;
module_param(fmeter_en, int, 0664);
MODULE_PARM_DESC(fmeter_en, "fmeter_en");

struct ve_hist_s video_ve_hist;

unsigned int vpp_log[128][10];

struct tcon_gamma_table_s video_gamma_table_r;
struct tcon_gamma_table_s video_gamma_table_g;
struct tcon_gamma_table_s video_gamma_table_b;
struct tcon_gamma_table_s video_gamma_table_r_adj;
struct tcon_gamma_table_s video_gamma_table_g_adj;
struct tcon_gamma_table_s video_gamma_table_b_adj;
struct tcon_gamma_table_s video_gamma_table_ioctl_set;
struct gm_tbl_s gt;
unsigned int gamma_index;
unsigned int gm_par_idx;
unsigned int gamma_index_sub = 1;

struct tcon_gamma_table_s video_gamma_table_r_sub;
struct tcon_gamma_table_s video_gamma_table_g_sub;
struct tcon_gamma_table_s video_gamma_table_b_sub;


struct tcon_rgb_ogo_s video_rgb_ogo = {
	0, /* wb enable */
	0, /* -1024~1023, r_pre_offset */
	0, /* -1024~1023, g_pre_offset */
	0, /* -1024~1023, b_pre_offset */
	1024, /* 0~2047, r_gain */
	1024, /* 0~2047, g_gain */
	1024, /* 0~2047, b_gain */
	0, /* -1024~1023, r_post_offset */
	0, /* -1024~1023, g_post_offset */
	0  /* -1024~1023, b_post_offset */
};

struct tcon_rgb_ogo_s video_rgb_ogo_sub = {
	0, /* wb enable */
	0, /* -1024~1023, r_pre_offset */
	0, /* -1024~1023, g_pre_offset */
	0, /* -1024~1023, b_pre_offset */
	1024, /* 0~2047, r_gain */
	1024, /* 0~2047, g_gain */
	1024, /* 0~2047, b_gain */
	0, /* -1024~1023, r_post_offset */
	0, /* -1024~1023, g_post_offset */
	0  /* -1024~1023, b_post_offset */
};

#define FLAG_LVDS_FREQ_SW1       BIT(6)

int dnlp_en;/* 0:disable;1:enable */
module_param(dnlp_en, int, 0664);
MODULE_PARM_DESC(dnlp_en, "\n enable or disable dnlp\n");
static int dnlp_status = 1;/* 0:done;1:todo */

int dnlp_en_2_pre = 1;
int dnlp_en_2 = 1;/* 0:disable;1:enable */
module_param(dnlp_en_2, int, 0664);
MODULE_PARM_DESC(dnlp_en_2, "\n enable or disable dnlp\n");

int dnlp_en_dsw = 1;/* 0:disable;1:enable */
module_param(dnlp_en_dsw, int, 0664);
MODULE_PARM_DESC(dnlp_en_dsw, "\n enable or disable dnlp_dsw\n");

int lut3d_en;/* lut3d_en enable/disable */
int lut3d_order;/* 0 RGB 1 GBR */
int lut3d_debug;

static int frame_lock_freq;
module_param(frame_lock_freq, int, 0664);
MODULE_PARM_DESC(frame_lock_freq, "frame_lock_50");

static int video_rgb_ogo_mode_sw;
module_param(video_rgb_ogo_mode_sw, int, 0664);
MODULE_PARM_DESC(video_rgb_ogo_mode_sw,
		 "enable/disable video_rgb_ogo_mode_sw");

int video_rgb_ogo_xvy_mtx;
module_param(video_rgb_ogo_xvy_mtx, int, 0664);
MODULE_PARM_DESC(video_rgb_ogo_xvy_mtx,
		 "enable/disable video_rgb_ogo_xvy_mtx");

int video_rgb_ogo_xvy_mtx_latch;

static unsigned int assist_cnt;/* ASSIST_SPARE8_REG1; */

/*0: recovery mode
 *1: certification mode
 *2: bypass mode, vadj1 follow config for ui setting
 */
static int dv_pq_bypass;

/* 3d sync parts begin */
unsigned int sync_3d_h_start;
unsigned int sync_3d_h_end;
unsigned int sync_3d_v_start = 10;
unsigned int sync_3d_v_end = 20;
unsigned int sync_3d_polarity;
unsigned int sync_3d_out_inv;
unsigned int sync_3d_black_color = 0x008080;/* yuv black */
/* 3d sync to v by one enable/disable */
unsigned int sync_3d_sync_to_vbo;
/* 3d sync parts end */

unsigned int contrast_adj_sel;/*0:vdj1, 1:vd1 mtx rgb contrast*/
module_param(contrast_adj_sel, uint, 0664);
MODULE_PARM_DESC(contrast_adj_sel, "\n contrast_adj_sel\n");

/*gxlx adaptive sr level*/
static unsigned int sr_adapt_level;
module_param(sr_adapt_level, uint, 0664);
MODULE_PARM_DESC(sr_adapt_level, "\n sr_adapt_level\n");

/* *********************************************************************** */
/* *** VPP_FIQ-oriented functions **************************************** */
/* *********************************************************************** */
void ve_hist_gamma_tgt(struct vframe_s *vf)
{
	int ave_luma;
	struct vframe_prop_s *p = &vf->prop;

	video_ve_hist.sum    = p->hist.vpp_luma_sum;
	video_ve_hist.width  = p->hist.vpp_width;
	video_ve_hist.height = p->hist.vpp_height;

	video_ve_hist.ave =
		video_ve_hist.sum / (video_ve_hist.height *
				video_ve_hist.width);
	if ((vf->source_type == VFRAME_SOURCE_TYPE_OTHERS &&
	     is_meson_gxtvbb_cpu()) ||
	     cpu_after_eq(MESON_CPU_MAJOR_ID_TXL)) {
		ave_luma = video_ve_hist.ave;
		ave_luma = (ave_luma - 16) < 0 ? 0 : (ave_luma - 16);
		video_ve_hist.ave = ave_luma * 255 / (235 - 16);
		if (video_ve_hist.ave > 255)
			video_ve_hist.ave = 255;
	}
}

void ve_hist_gamma_reset(void)
{
	if (video_ve_hist.height != 0 ||
	    video_ve_hist.width != 0)
		memset(&video_ve_hist, 0, sizeof(struct ve_hist_s));
}

void ve_dnlp_load_reg(void)
{
	int i;
	int dnlp_reg = 0;

	if  (dnlp_sel == NEW_DNLP_IN_SHARPNESS) {
		if (is_meson_gxlx_cpu() || is_meson_txlx_cpu()) {
			for (i = 0; i < 16; i++)
				WRITE_VPP_REG(SRSHARP1_DNLP_00 + i,
					      ve_dnlp_reg[i]);
		} else if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
			if (!vinfo_lcd_support() || is_meson_t7_cpu())
				dnlp_reg = SRSHARP0_DNLP2_00;
			else
				dnlp_reg = SRSHARP1_DNLP2_00;

			for (i = 0; i < 32; i++)
				WRITE_VPP_REG(dnlp_reg + i,
					      ve_dnlp_reg_v2[i]);
		} else {
			for (i = 0; i < 16; i++)
				WRITE_VPP_REG(SRSHARP0_DNLP_00 + i,
					      ve_dnlp_reg[i]);
		}
	} else {
		for (i = 0; i < 16; i++)
			WRITE_VPP_REG(VPP_DNLP_CTRL_00 + i,
				      ve_dnlp_reg[i]);
	}
}

static void ve_dnlp_load_def_reg(void)
{
	int i;
	int dnlp_reg = 0;

	if  (dnlp_sel == NEW_DNLP_IN_SHARPNESS) {
		if (is_meson_gxlx_cpu() || is_meson_txlx_cpu()) {
			for (i = 0; i < 16; i++)
				WRITE_VPP_REG(SRSHARP1_DNLP_00 + i,
					      ve_dnlp_reg[i]);
		} else if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
			if (!vinfo_lcd_support() || is_meson_t7_cpu())
				dnlp_reg = SRSHARP0_DNLP2_00;
			else
				dnlp_reg = SRSHARP1_DNLP2_00;

			for (i = 0; i < 32; i++)
				WRITE_VPP_REG(dnlp_reg + i,
					      ve_dnlp_reg_v2[i]);
		} else {
			for (i = 0; i < 16; i++)
				WRITE_VPP_REG(SRSHARP0_DNLP_00 + i,
					      ve_dnlp_reg[i]);
		}
	} else {
		for (i = 0; i < 16; i++)
			WRITE_VPP_REG(VPP_DNLP_CTRL_00 + i,
				      ve_dnlp_reg_def[i]);
	}
}

void ve_on_vs(struct vframe_s *vf)
{
	if (dnlp_en_dsw) {
		/* calculate dnlp target data */
		if (ve_dnlp_calculate_tgtx(vf)) {
			/* calculate dnlp low-pass-filter data */
			ve_dnlp_calculate_lpf();
			/* calculate dnlp reg data */
			ve_dnlp_calculate_reg();
			/* load dnlp reg data */
			ve_dnlp_load_reg();
		}
	}

	if (dnlp_en_2_pre != dnlp_en_2) {
		dnlp_en_2_pre = dnlp_en_2;

		if (dnlp_sel == NEW_DNLP_IN_SHARPNESS) {
			if (is_meson_gxlx_cpu() || is_meson_txlx_cpu())
				VSYNC_WRITE_VPP_REG_BITS(SRSHARP1_DNLP_EN, dnlp_en_2, 0, 1);
			else if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1))
				if (!vinfo_lcd_support() || is_meson_t7_cpu())
					VSYNC_WRITE_VPP_REG_BITS(SRSHARP0_DNLP_EN, dnlp_en_2, 0, 1);
				else
					VSYNC_WRITE_VPP_REG_BITS(SRSHARP1_DNLP_EN, dnlp_en_2, 0, 1);
			else
				VSYNC_WRITE_VPP_REG_BITS(SRSHARP0_DNLP_EN, dnlp_en_2, 0, 1);
		} else {
			WRITE_VPP_REG_BITS(VPP_VE_ENABLE_CTRL,
				dnlp_en_2, DNLP_EN_BIT, DNLP_EN_WID);
		}
	}

	/* sharpness process */
	sharpness_process(vf);
}

/* *********************************************************************** */
/* *** IOCTL-oriented functions ****************************************** */
/* *********************************************************************** */
int vpp_get_encl_viu_mux(void)
{
	unsigned int temp;

	temp = READ_VPP_REG(VPU_VIU_VENC_MUX_CTRL);
	if ((temp & 0x3) == 0)
		return 1;
	if (((temp >> 2) & 0x3) == 0)
		return 2;
	return 0;
}

int vpp_get_vout_viu_mux(void)
{
	unsigned int temp = 0;
	struct vinfo_s *vinfo1, *vinfo2, *vinfo3;

	vinfo1 = NULL;
	vinfo2 = NULL;
	vinfo3 = NULL;
	#ifdef CONFIG_AMLOGIC_VOUT_SERVE
	vinfo1 = get_current_vinfo();
	if (vinfo1->mode == 2) {
		temp = (vinfo1->viu_mux >> 4) & 0xf;
		if (temp == 0)
			temp = 1;
		else if (temp == 1)
			temp = 2;
		else if (temp > 1)
			temp = 3;
		return temp;
	}
	#endif

	#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
	vinfo2 = get_current_vinfo2();
	if (vinfo2->mode == 2) {
		temp = (vinfo2->viu_mux >> 4) & 0xf;
		if (temp == 0)
			temp = 1;
		else if (temp == 1)
			temp = 2;
		else if (temp > 1)
			temp = 3;
		return temp;
	}
	#endif

	#ifdef CONFIG_AMLOGIC_VOUT3_SERVE
	vinfo3 = get_current_vinfo3();
	if (vinfo3->mode == 2) {
		temp = (vinfo3->viu_mux >> 4) & 0xf;
		if (temp == 0)
			temp = 1;
		else if (temp == 1)
			temp = 2;
		else if (temp > 1)
			temp = 3;
		return temp;
	}
	#endif

	return temp;
}

void vpp_enable_lcd_gamma_table(int viu_sel, int rdma_write)
{
	unsigned int offset = 0x0;
	unsigned int reg;

	if (chip_type_id == chip_a4)
		reg = LCD_GAMMA_CNTL_PORT0_A4;
	else if (cpu_after_eq_t7())
		reg = LCD_GAMMA_CNTL_PORT0;
	else
		reg = L_GAMMA_CNTL_PORT;

	if (viu_sel == 0) /*venc0*/
		offset = 0;
	else if (viu_sel == 1) /*venc1*/
		offset = 0x100;
	else if (viu_sel == 2) /*venc2*/
		offset = 0x200;
	if (cpu_after_eq_t7()) {
		if (rdma_write)
			VSYNC_WRITE_VPP_REG_BITS(reg + offset,
				1, L_GAMMA_EN, 1);
		else
			WRITE_VPP_REG_BITS(reg + offset,
				1, L_GAMMA_EN, 1);
		return;
	}

	if (rdma_write == 1) /* viu1 vsync rdma */
		VSYNC_WRITE_VPP_REG_BITS(reg, 1, GAMMA_EN, 1);
	else
		WRITE_VPP_REG_BITS(reg, 1, GAMMA_EN, 1);

	pr_amve_dbg("\n[amve..] set enable_lcd_gamma_table.\n");
}

void vpp_disable_lcd_gamma_table(int viu_sel, int rdma_write)
{
	unsigned int offset = 0x0;
	unsigned int reg;

	if (chip_type_id == chip_a4)
		reg = LCD_GAMMA_CNTL_PORT0_A4;
	else if (cpu_after_eq_t7())
		reg = LCD_GAMMA_CNTL_PORT0;
	else
		reg = L_GAMMA_CNTL_PORT;

	if (viu_sel == 0) /*venc0*/
		offset = 0;
	else if (viu_sel == 1) /*venc1*/
		offset = 0x100;
	else if (viu_sel == 2) /*venc2*/
		offset = 0x200;

	if (cpu_after_eq_t7()) {
		if (rdma_write)
			VSYNC_WRITE_VPP_REG_BITS(reg + offset,
				0, L_GAMMA_EN, 1);
		else
			WRITE_VPP_REG_BITS(reg + offset,
				0, L_GAMMA_EN, 1);
		return;
	}

	if (rdma_write == 1) /* viu1 vsync rdma */
		VSYNC_WRITE_VPP_REG_BITS(reg, 0, GAMMA_EN, 1);
	else
		WRITE_VPP_REG_BITS(reg, 0, GAMMA_EN, 1);

	pr_amve_dbg("\n[amve..] set disable_lcd_gamma_table.\n");
}

/*new gamma interface, start from T7*/
/*data[3][256] is r/g/b 256 data
 *rdma_write:  1 rdma write, 0 write directly
 *rw_flag:  0 write gamma,  1: read gamma
 */
void lcd_gamma_api(unsigned int index, u16 *r_data, u16 *g_data, u16 *b_data,
	int rdma_write, int rw_flag)
{
	int i;
	unsigned int val;
	unsigned int offset = 0;
	unsigned int max_idx;
	unsigned int auto_inc;
	unsigned int addr_port;
	unsigned int data_port;

	/*
	 * A4 gamma change to 257 points, other chip 256 points,
	 * Already separate A4 and other chip for gamma lut points
	 */
	/* coverity[overrun-buffer-arg: SUPPRESS]*/
	if (index == 0)
		offset = 0;
	else if (index == 1)
		offset = 0x100;
	else if (index == 2)
		offset = 0x200;

	if (chip_type_id == chip_a4) {
		auto_inc = L_H_AUTO_INC_2;
		max_idx = 257;
		addr_port = LCD_GAMMA_ADDR_PORT0_A4;
		data_port = LCD_GAMMA_DATA_PORT0_A4;
	} else {
		auto_inc = L_H_AUTO_INC;
		max_idx = 256;
		addr_port = LCD_GAMMA_ADDR_PORT0;
		data_port = LCD_GAMMA_DATA_PORT0;
	}

	if (rw_flag) {
		WRITE_VPP_REG(addr_port + offset,
			(0x1 << L_H_AUTO_INC));
		for (i = 0; i < max_idx; i++) {
			val = READ_VPP_REG(data_port + offset);
			r_data[i] = (val >> L_GAMMA_R) & 0x3ff;
			g_data[i] = (val >> L_GAMMA_G) & 0x3ff;
			b_data[i] = (val >> L_GAMMA_B) & 0x3ff;
		}
	} else {
		if (rdma_write) {
			VSYNC_WRITE_VPP_REG(addr_port + offset,
				(0x1 << auto_inc));
			for (i = 0; i < max_idx; i++)
				VSYNC_WRITE_VPP_REG(data_port +
					offset,
				(r_data[i] << L_GAMMA_R) | (g_data[i] << L_GAMMA_G) |
				(b_data[i] << L_GAMMA_B));
		} else {
			WRITE_VPP_REG(addr_port + offset,
				(0x1 << auto_inc));
			for (i = 0; i < max_idx; i++)
				WRITE_VPP_REG(data_port + offset,
				(r_data[i] << L_GAMMA_R) | (g_data[i] << L_GAMMA_G) |
				(b_data[i] << L_GAMMA_B));
		}
	}
}

void vpp_set_lcd_gamma_table(u16 *data, u32 rgb_mask, int viu_sel)
{
	int i;
	int cnt = 0;
	unsigned long flags = 0;

	if (!(READ_VPP_REG(ENCL_VIDEO_EN) & 0x1))
		return;

	spin_lock_irqsave(&vpp_lcd_gamma_lock, flags);

	while (!(READ_VPP_REG(L_GAMMA_CNTL_PORT) & (0x1 << ADR_RDY))) {
		udelay(10);
		if (cnt++ > GAMMA_RETRY)
			break;
	}
	cnt = 0;
	WRITE_VPP_REG(L_GAMMA_ADDR_PORT, (0x1 << H_AUTO_INC) |
				    (0x1 << rgb_mask)   |
				    (0x0 << HADR));
	for (i = 0; i < 256; i++) {
		while (!(READ_VPP_REG(L_GAMMA_CNTL_PORT) & (0x1 << WR_RDY))) {
			udelay(10);
			if (cnt++ > GAMMA_RETRY)
				break;
		}
		cnt = 0;
		WRITE_VPP_REG(L_GAMMA_DATA_PORT, data[i]);
	}
	while (!(READ_VPP_REG(L_GAMMA_CNTL_PORT) & (0x1 << ADR_RDY))) {
		udelay(10);
		if (cnt++ > GAMMA_RETRY)
			break;
	}
	WRITE_VPP_REG(L_GAMMA_ADDR_PORT, (0x1 << H_AUTO_INC) |
				    (0x1 << rgb_mask)   |
				    (0x23 << HADR));

	spin_unlock_irqrestore(&vpp_lcd_gamma_lock, flags);
}

u16 gamma_data_r[257] = {0};
u16 gamma_data_g[257] = {0};
u16 gamma_data_b[257] = {0};
void vpp_get_lcd_gamma_table(u32 rgb_mask)
{
	int i;
	int cnt = 0;
	struct gamma_data_s *p_gm;

	if (chip_type_id == chip_a4) {
		p_gm = get_gm_data();
		lcd_gamma_api(gamma_index, p_gm->dbg_gm_tbl.gamma_r,
			p_gm->dbg_gm_tbl.gamma_g, p_gm->dbg_gm_tbl.gamma_b,
			0, 1);
		return;
	}

	if (cpu_after_eq_t7()) {
		lcd_gamma_api(gamma_index, gamma_data_r,
			gamma_data_g, gamma_data_b, 0, 1);
		return;
	}

	if (!(READ_VPP_REG(ENCL_VIDEO_EN) & 0x1))
		return;
	pr_info("read gamma begin\n");
	while (!(READ_VPP_REG(L_GAMMA_CNTL_PORT) & (0x1 << ADR_RDY))) {
		udelay(10);
		if (cnt++ > GAMMA_RETRY)
			break;
	}
	cnt = 0;
	for (i = 0; i < 256; i++) {
		cnt = 0;
		while (!(READ_VPP_REG(L_GAMMA_CNTL_PORT) & (0x1 << ADR_RDY))) {
			udelay(10);
			if (cnt++ > GAMMA_RETRY) {
				pr_info("%s ADR_RDY timeout\n", __func__);
				break;
			}
		}
		WRITE_VPP_REG(L_GAMMA_ADDR_PORT, (0x1 << H_RD) |
						(0x0 << H_AUTO_INC) |
						(0x1 << rgb_mask)	|
						(i << HADR));

		cnt = 0;
		while (!(READ_VPP_REG(L_GAMMA_CNTL_PORT) & (0x1 << RD_RDY))) {
			udelay(10);
			if (cnt++ > GAMMA_RETRY) {
				pr_info("%s ADR_RDY timeout\n", __func__);
				break;
			}
		}
		if (rgb_mask == H_SEL_R)
			gamma_data_r[i] = READ_VPP_REG(L_GAMMA_DATA_PORT);
		else if (rgb_mask == H_SEL_G)
			gamma_data_g[i] = READ_VPP_REG(L_GAMMA_DATA_PORT);
		else if (rgb_mask == H_SEL_B)
			gamma_data_b[i] = READ_VPP_REG(L_GAMMA_DATA_PORT);
	}
	WRITE_VPP_REG(L_GAMMA_ADDR_PORT, (0x1 << H_AUTO_INC) |
				    (0x1 << rgb_mask)   |
				    (0x23 << HADR));
	pr_info("read gamma over\n");
}

void vpp_get_lcd_gamma_table_sub(void)
{
	if (is_meson_t7_cpu())
		lcd_gamma_api(gamma_index_sub,
			gamma_data_r,
			gamma_data_g,
			gamma_data_b,
			0, 1);
}

void amve_write_gamma_table(u16 *data, u32 rgb_mask)
{
	int i;
	int cnt = 0;
	unsigned long flags = 0;
	struct gamma_data_s *p_gm;
	int max_idx;

	if (chip_type_id == chip_a4) {
		p_gm = get_gm_data();
		max_idx = p_gm->max_idx;
		lcd_gamma_api(gamma_index, p_gm->dbg_gm_tbl.gamma_r,
			p_gm->dbg_gm_tbl.gamma_g, p_gm->dbg_gm_tbl.gamma_b,
			0, 1);
		if (rgb_mask == H_SEL_R)
			memcpy(p_gm->dbg_gm_tbl.gamma_r, data, sizeof(u16) * max_idx);
		else if (rgb_mask == H_SEL_G)
			memcpy(p_gm->dbg_gm_tbl.gamma_g, data, sizeof(u16) * max_idx);
		else if (rgb_mask == H_SEL_B)
			memcpy(p_gm->dbg_gm_tbl.gamma_b, data, sizeof(u16) * max_idx);
		lcd_gamma_api(gamma_index, p_gm->dbg_gm_tbl.gamma_r,
			p_gm->dbg_gm_tbl.gamma_g, p_gm->dbg_gm_tbl.gamma_b,
			0, 0);
		return;
	}

	if (cpu_after_eq_t7()) {
		lcd_gamma_api(gamma_index, gamma_data_r,
			gamma_data_g, gamma_data_b, 0, 1);
		if (rgb_mask == H_SEL_R)
			memcpy(gamma_data_r, data, sizeof(u16) * 256);
		else if (rgb_mask == H_SEL_G)
			memcpy(gamma_data_g, data, sizeof(u16) * 256);
		else if (rgb_mask == H_SEL_B)
			memcpy(gamma_data_b, data, sizeof(u16) * 256);
		lcd_gamma_api(gamma_index, gamma_data_r,
			gamma_data_g, gamma_data_b, 0, 0);
		return;
	}

	if (!(READ_VPP_REG(ENCL_VIDEO_EN) & 0x1))
		return;

	spin_lock_irqsave(&vpp_lcd_gamma_lock, flags);

	while (!(READ_VPP_REG(L_GAMMA_CNTL_PORT) & (0x1 << ADR_RDY))) {
		udelay(10);
		if (cnt++ > GAMMA_RETRY)
			break;
	}
	cnt = 0;
	WRITE_VPP_REG(L_GAMMA_ADDR_PORT, (0x1 << H_AUTO_INC) |
				    (0x1 << rgb_mask)   |
				    (0x0 << HADR));
	for (i = 0; i < 256; i++) {
		while (!(READ_VPP_REG(L_GAMMA_CNTL_PORT) & (0x1 << WR_RDY))) {
			udelay(10);
			if (cnt++ > GAMMA_RETRY)
				break;
		}
		cnt = 0;
		WRITE_VPP_REG(L_GAMMA_DATA_PORT, data[i]);
	}
	while (!(READ_VPP_REG(L_GAMMA_CNTL_PORT) & (0x1 << ADR_RDY))) {
		udelay(10);
		if (cnt++ > GAMMA_RETRY)
			break;
	}
	WRITE_VPP_REG(L_GAMMA_ADDR_PORT, (0x1 << H_AUTO_INC) |
				    (0x1 << rgb_mask)   |
				    (0x23 << HADR));

	spin_unlock_irqrestore(&vpp_lcd_gamma_lock, flags);
}

void amve_write_gamma_table_sub(u16 *data, u32 rgb_mask)
{
	if (is_meson_t7_cpu()) {
		lcd_gamma_api(gamma_index_sub,
			gamma_data_r, gamma_data_g, gamma_data_b, 0, 1);

		if (rgb_mask == H_SEL_R)
			memcpy(gamma_data_r, data, sizeof(u16) * 256);
		else if (rgb_mask == H_SEL_G)
			memcpy(gamma_data_g, data, sizeof(u16) * 256);
		else if (rgb_mask == H_SEL_B)
			memcpy(gamma_data_b, data, sizeof(u16) * 256);

		lcd_gamma_api(gamma_index_sub,
			gamma_data_r, gamma_data_g, gamma_data_b, 0, 0);
	}
}

#define COEFF_NORM(a) ((int)((((a) * 2048.0) + 1) / 2))
#define MATRIX_5x3_COEF_SIZE 24

static int RGB709_to_YUV709l_coeff[MATRIX_5x3_COEF_SIZE] = {
	0, 0, 0, /* pre offset */
	COEFF_NORM(0.181873),	COEFF_NORM(0.611831),	COEFF_NORM(0.061765),
	COEFF_NORM(-0.100251),	COEFF_NORM(-0.337249),	COEFF_NORM(0.437500),
	COEFF_NORM(0.437500),	COEFF_NORM(-0.397384),	COEFF_NORM(-0.040116),
	0, 0, 0, /* 10'/11'/12' */
	0, 0, 0, /* 20'/21'/22' */
	64, 512, 512, /* offset */
	0, 0, 0 /* mode, right_shift, clip_en */
};

static int bypass_coeff[MATRIX_5x3_COEF_SIZE] = {
	0, 0, 0, /* pre offset */
	COEFF_NORM(1.0),	COEFF_NORM(0.0),	COEFF_NORM(0.0),
	COEFF_NORM(0.0),	COEFF_NORM(1.0),	COEFF_NORM(0.0),
	COEFF_NORM(0.0),	COEFF_NORM(0.0),	COEFF_NORM(1.0),
	0, 0, 0, /* 10'/11'/12' */
	0, 0, 0, /* 20'/21'/22' */
	0, 0, 0, /* offset */
	0, 0, 0 /* mode, right_shift, clip_en */
};

#define MTX_RS 0
#define MTX_ENABLE 1
#define MATRIX_3x3_COEF_SIZE 17
static int matrix_yuv_bypass_coef[MATRIX_3x3_COEF_SIZE] = {
	-64, -512, -512,
	COEFF_NORM(1.0), COEFF_NORM(0), COEFF_NORM(0),
	COEFF_NORM(0), COEFF_NORM(1.0), COEFF_NORM(0),
	COEFF_NORM(0), COEFF_NORM(0), COEFF_NORM(1.0),
	64, 512, 512,
	MTX_RS,
	MTX_ENABLE
};

void vpp_set_rgb_ogo_sub(struct tcon_rgb_ogo_s *p)
{
	/*for t7 vpp1 go*/
	if (is_meson_t7_cpu()) {
		WRITE_VPP_REG(VPP1_GAINOFF_CTRL0,
			((p->en << 31) & 0x80000000) |
			((p->r_gain << 16) & 0x07ff0000) |
			((p->g_gain << 0) & 0x000007ff));
		WRITE_VPP_REG(VPP1_GAINOFF_CTRL1,
			((p->b_gain << 16) & 0x07ff0000) |
			((p->r_post_offset << 0) & 0x00001fff));
		WRITE_VPP_REG(VPP1_GAINOFF_CTRL2,
			((p->g_post_offset << 16) & 0x1fff0000) |
			((p->b_post_offset << 0) & 0x00001fff));
		WRITE_VPP_REG(VPP1_GAINOFF_CTRL3,
			((p->r_pre_offset << 16) & 0x1fff0000) |
			((p->g_pre_offset << 0) & 0x00001fff));
		WRITE_VPP_REG(VPP1_GAINOFF_CTRL4,
			((p->b_pre_offset << 0) & 0x00001fff));
	}
}

void vpp_set_rgb_ogo(struct tcon_rgb_ogo_s *p)
{
	int m[24];
	int i;
	unsigned int gainoff_ctl0;
	unsigned int gainoff_ctl1;
	unsigned int gainoff_ctl2;
	unsigned int gainoff_ctl3;
	unsigned int gainoff_ctl4;

	/* write to registers */
	if (video_rgb_ogo_xvy_mtx) {
		if (video_rgb_ogo_xvy_mtx_latch & MTX_BYPASS_RGB_OGO) {
			memcpy(m, bypass_coeff, sizeof(int) * 24);
			video_rgb_ogo_xvy_mtx_latch &= ~MTX_BYPASS_RGB_OGO;
		} else if (video_rgb_ogo_xvy_mtx_latch & MTX_RGB2YUVL_RGB_OGO) {
			memcpy(m, RGB709_to_YUV709l_coeff, sizeof(int) * 24);
			video_rgb_ogo_xvy_mtx_latch &= ~MTX_RGB2YUVL_RGB_OGO;
		} else {
			memcpy(m, bypass_coeff, sizeof(int) * 24);
		}

		m[3] = p->r_gain * m[3] / COEFF_NORM(1.0);
		m[4] = p->g_gain * m[4] / COEFF_NORM(1.0);
		m[5] = p->b_gain * m[5] / COEFF_NORM(1.0);
		m[6] = p->r_gain * m[6] / COEFF_NORM(1.0);
		m[7] = p->g_gain * m[7] / COEFF_NORM(1.0);
		m[8] = p->b_gain * m[8] / COEFF_NORM(1.0);
		m[9] = p->r_gain * m[9] / COEFF_NORM(1.0);
		m[10] = p->g_gain * m[10] / COEFF_NORM(1.0);
		m[11] = p->b_gain * m[11] / COEFF_NORM(1.0);

		if (vinfo_lcd_support()) {
			m[18] = (p->r_pre_offset + m[18] + 1024)
				* p->r_gain / COEFF_NORM(1.0)
				- p->r_gain + p->r_post_offset;
			m[19] = (p->g_pre_offset + m[19] + 1024)
				* p->g_gain / COEFF_NORM(1.0)
				- p->g_gain + p->g_post_offset;
			m[20] = (p->b_pre_offset + m[20] + 1024)
				* p->b_gain / COEFF_NORM(1.0)
				- p->b_gain + p->b_post_offset;
		} else {
			m[0] = p->r_gain * p->r_pre_offset / COEFF_NORM(1.0) +
				p->r_post_offset;
			m[1] = p->g_gain * p->g_pre_offset / COEFF_NORM(1.0) +
				p->g_post_offset;
			m[2] = p->b_gain * p->b_pre_offset / COEFF_NORM(1.0) +
				p->b_post_offset;
		}

		for (i = 18; i < 21; i++) {
			if (m[i] > 1023)
				m[i] = 1023;
			if (m[i] < -1024)
				m[i] = -1024;
		}

		if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A) {
			WRITE_VPP_REG_BITS(VPP_POST_MATRIX_EN_CTRL,
					   p->en, 0, 1);
			WRITE_VPP_REG(VPP_POST_MATRIX_PRE_OFFSET0_1,
				      ((m[0] & 0xfff) << 16)
				      | (m[1] & 0xfff));
			WRITE_VPP_REG(VPP_POST_MATRIX_PRE_OFFSET2,
				      m[2] & 0xfff);
			WRITE_VPP_REG(VPP_POST_MATRIX_COEF00_01,
				      ((m[3] & 0x1fff) << 16)
				      | (m[4] & 0x1fff));
			WRITE_VPP_REG(VPP_POST_MATRIX_COEF02_10,
				      ((m[5]	& 0x1fff) << 16)
				      | (m[6] & 0x1fff));
			WRITE_VPP_REG(VPP_POST_MATRIX_COEF11_12,
				      ((m[7] & 0x1fff) << 16)
				      | (m[8] & 0x1fff));
			WRITE_VPP_REG(VPP_POST_MATRIX_COEF20_21,
				      ((m[9] & 0x1fff) << 16)
				      | (m[10] & 0x1fff));
			WRITE_VPP_REG(VPP_POST_MATRIX_COEF22,
				      m[11] & 0x1fff);
			if (m[21]) {
				WRITE_VPP_REG(VPP_POST_MATRIX_COEF13_14,
					      ((m[12] & 0x1fff) << 16)
					      | (m[13] & 0x1fff));
				WRITE_VPP_REG(VPP_POST_MATRIX_COEF15_25,
					      ((m[14] & 0x1fff) << 16)
					      | (m[17] & 0x1fff));
				WRITE_VPP_REG(VPP_POST_MATRIX_COEF23_24,
					      ((m[15] & 0x1fff) << 16)
					      | (m[16] & 0x1fff));
			}
			WRITE_VPP_REG(VPP_POST_MATRIX_OFFSET0_1,
				      ((m[18] & 0xfff) << 16)
				      | (m[19] & 0xfff));
			WRITE_VPP_REG(VPP_POST_MATRIX_OFFSET2,
				      m[20] & 0xfff);
			WRITE_VPP_REG_BITS(VPP_POST_MATRIX_CLIP,
					   m[21], 3, 2);
			WRITE_VPP_REG_BITS(VPP_POST_MATRIX_CLIP,
					   m[22], 5, 3);
			return;
		}

		WRITE_VPP_REG_BITS(VPP_MATRIX_CTRL, p->en, 6, 1);
		WRITE_VPP_REG_BITS(VPP_MATRIX_CTRL, 3, 8, 2);

		WRITE_VPP_REG(VPP_MATRIX_PRE_OFFSET0_1,
			      ((m[0] & 0xfff) << 16)
			      | (m[1] & 0xfff));
		WRITE_VPP_REG(VPP_MATRIX_PRE_OFFSET2,
			      m[2] & 0xfff);
		WRITE_VPP_REG(VPP_MATRIX_COEF00_01,
			      ((m[3] & 0x1fff) << 16)
			      | (m[4] & 0x1fff));
		WRITE_VPP_REG(VPP_MATRIX_COEF02_10,
			      ((m[5]	& 0x1fff) << 16)
			      | (m[6] & 0x1fff));
		WRITE_VPP_REG(VPP_MATRIX_COEF11_12,
			      ((m[7] & 0x1fff) << 16)
			      | (m[8] & 0x1fff));
		WRITE_VPP_REG(VPP_MATRIX_COEF20_21,
			      ((m[9] & 0x1fff) << 16)
			      | (m[10] & 0x1fff));
		WRITE_VPP_REG(VPP_MATRIX_COEF22,
			      m[11] & 0x1fff);
		if (m[21]) {
			WRITE_VPP_REG(VPP_MATRIX_COEF13_14,
				      ((m[12] & 0x1fff) << 16)
				      | (m[13] & 0x1fff));
			WRITE_VPP_REG(VPP_MATRIX_COEF15_25,
				      ((m[14] & 0x1fff) << 16)
				      | (m[17] & 0x1fff));
			WRITE_VPP_REG(VPP_MATRIX_COEF23_24,
				      ((m[15] & 0x1fff) << 16)
				      | (m[16] & 0x1fff));
		}
		WRITE_VPP_REG(VPP_MATRIX_OFFSET0_1,
			      ((m[18] & 0xfff) << 16)
			      | (m[19] & 0xfff));
		WRITE_VPP_REG(VPP_MATRIX_OFFSET2,
			      m[20] & 0xfff);
		WRITE_VPP_REG_BITS(VPP_MATRIX_CLIP,
				   m[21], 3, 2);
		WRITE_VPP_REG_BITS(VPP_MATRIX_CLIP,
				   m[22], 5, 3);
	} else {
		/*for txlx and txhd, pre_offset and post_offset become 13 bit*/
		if (chip_type_id == chip_s5) {
			post_gainoff_set(p, WR_DMA);
		} else if (is_meson_txlx_cpu() || is_meson_txhd_cpu() ||
		    is_meson_tm2_cpu() || is_meson_t7_cpu()) {
			WRITE_VPP_REG(VPP_GAINOFF_CTRL0,
				      ((p->en << 31) & 0x80000000) |
				      ((p->r_gain << 16) & 0x07ff0000) |
				      ((p->g_gain <<  0) & 0x000007ff));
			WRITE_VPP_REG(VPP_GAINOFF_CTRL1,
				      ((p->b_gain << 16) & 0x07ff0000) |
				      ((p->r_post_offset <<  0) & 0x00001fff));
			WRITE_VPP_REG(VPP_GAINOFF_CTRL2,
				      ((p->g_post_offset << 16) & 0x1fff0000) |
				      ((p->b_post_offset <<  0) & 0x00001fff));
			WRITE_VPP_REG(VPP_GAINOFF_CTRL3,
				      ((p->r_pre_offset  << 16) & 0x1fff0000) |
				      ((p->g_pre_offset  <<  0) & 0x00001fff));
			WRITE_VPP_REG(VPP_GAINOFF_CTRL4,
				      ((p->b_pre_offset  <<  0) & 0x00001fff));
		} else {
		/*txl and before txl, and tl1 10bit path offset is 11bit*/
			if (chip_type_id == chip_a4) {
				gainoff_ctl0 = VOUT_GAINOFF_CTRL0;
				gainoff_ctl1 = VOUT_GAINOFF_CTRL1;
				gainoff_ctl2 = VOUT_GAINOFF_CTRL2;
				gainoff_ctl3 = VOUT_GAINOFF_CTRL3;
				gainoff_ctl4 = VOUT_GAINOFF_CTRL4;
			} else {
				gainoff_ctl0 = VPP_GAINOFF_CTRL0;
				gainoff_ctl1 = VPP_GAINOFF_CTRL1;
				gainoff_ctl2 = VPP_GAINOFF_CTRL2;
				gainoff_ctl3 = VPP_GAINOFF_CTRL3;
				gainoff_ctl4 = VPP_GAINOFF_CTRL4;
			}
			WRITE_VPP_REG(gainoff_ctl0,
				      ((p->en << 31) & 0x80000000) |
				      ((p->r_gain << 16) & 0x07ff0000) |
				      ((p->g_gain <<  0) & 0x000007ff));
			WRITE_VPP_REG(gainoff_ctl1,
				      ((p->b_gain << 16) & 0x07ff0000) |
				      ((p->r_post_offset <<  0) & 0x000007ff));
			WRITE_VPP_REG(gainoff_ctl2,
				      ((p->g_post_offset << 16) & 0x07ff0000) |
				      ((p->b_post_offset <<  0) & 0x000007ff));
			WRITE_VPP_REG(gainoff_ctl3,
				      ((p->r_pre_offset  << 16) & 0x07ff0000) |
				      ((p->g_pre_offset  <<  0) & 0x000007ff));
			WRITE_VPP_REG(gainoff_ctl4,
				      ((p->b_pre_offset  <<  0) & 0x000007ff));
		}
	}
}

void ve_enable_dnlp(void)
{
	ve_en = 1;
/* #ifdef NEW_DNLP_IN_SHARPNESS */
/* if(dnlp_sel == NEW_DNLP_IN_SHARPNESS){ */
	if (dnlp_sel == NEW_DNLP_IN_SHARPNESS) {
		if (is_meson_gxlx_cpu() || is_meson_txlx_cpu())
			WRITE_VPP_REG_BITS(SRSHARP1_DNLP_EN, 1, 0, 1);
		else if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
			if ((!vinfo_lcd_support() && chip_type_id != chip_s5) ||
				is_meson_t7_cpu())
				WRITE_VPP_REG_BITS(SRSHARP0_DNLP_EN, 1, 0, 1);
			else
				WRITE_VPP_REG_BITS(SRSHARP1_DNLP_EN, 1, 0, 1);
		} else {
			WRITE_VPP_REG_BITS(SRSHARP0_DNLP_EN, 1, 0, 1);
		}
	} else {
		/* #endif */
		WRITE_VPP_REG_BITS(VPP_VE_ENABLE_CTRL,
				   1, DNLP_EN_BIT, DNLP_EN_WID);
	}
}

void ve_disable_dnlp(void)
{
	ve_en = 0;
	if (dnlp_sel == NEW_DNLP_IN_SHARPNESS)
		if (is_meson_gxlx_cpu() || is_meson_txlx_cpu())
			WRITE_VPP_REG_BITS(SRSHARP1_DNLP_EN, 0, 0, 1);
		else if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
			if ((!vinfo_lcd_support() && chip_type_id != chip_s5) ||
				is_meson_t7_cpu())
				WRITE_VPP_REG_BITS(SRSHARP0_DNLP_EN, 0, 0, 1);
			else
				WRITE_VPP_REG_BITS(SRSHARP1_DNLP_EN, 0, 0, 1);
		} else {
			WRITE_VPP_REG_BITS(SRSHARP0_DNLP_EN, 0, 0, 1);
		}
	else
		WRITE_VPP_REG_BITS(VPP_VE_ENABLE_CTRL,
				   0, DNLP_EN_BIT, DNLP_EN_WID);
}

void ve_dnlp_ctrl_vsync(int enable)
{
	dnlp_en_2 = enable;
}
void ve_set_dnlp_2(void)
{
	ulong i = 0;
	/* clear historic luma sum */
	if (dnlp_insmod_ok == 0)
		return;
	*ve_dnlp_luma_sum_copy = 0;
	/* init tgt & lpf */
	for (i = 0; i < 64; i++) {
		ve_dnlp_tgt_copy[i] = i << 2;
		ve_dnlp_tgt_10b_copy[i] = i << 4;
		ve_dnlp_lpf[i] = (ulong)ve_dnlp_tgt_copy[i] << ve_dnlp_rt;
	}
	/* calculate dnlp reg data */
	ve_dnlp_calculate_reg();
	/* load dnlp reg data */
	/*ve_dnlp_load_reg();*/
	ve_dnlp_load_def_reg();
}

unsigned int ve_get_vs_cnt(void)
{
	return READ_VPP_REG(VPP_VDO_MEAS_VS_COUNT_LO);
}

void vpp_phase_lock_on_vs(unsigned int cycle,
			  unsigned int stamp,
			  bool         lock50,
			  unsigned int range_fast,
			  unsigned int range_slow)
{
	unsigned int vtotal_ori = READ_VPP_REG(ENCL_VIDEO_MAX_LNCNT);
	unsigned int vtotal     = lock50 ? 1349 : 1124;
	unsigned int stamp_in   = READ_VPP_REG(VDIN_MEAS_VS_COUNT_LO);
	unsigned int stamp_out  = ve_get_vs_cnt();
	unsigned int phase      = 0;
	unsigned int cnt = assist_cnt;/* READ_VPP_REG(ASSIST_SPARE8_REG1); */
	int step = 0, i = 0;
	/* get phase */
	if (stamp_out < stamp)
		phase = 0xffffffff - stamp + stamp_out + 1;
	else
		phase = stamp_out - stamp;
	while (phase >= cycle)
		phase -= cycle;
	/* 225~315 degree => tune fast panel output */
	if ((phase > ((cycle * 5) >> 3)) && (phase < ((cycle * 7) >> 3))) {
		vtotal -= range_slow;
		step = 1;
	} else if ((phase > (cycle >> 3)) && (phase < ((cycle * 3) >> 3))) {
		/* 45~135 degree => tune slow panel output */
		vtotal += range_slow;
		step = -1;
	} else if (phase >= ((cycle * 7) >> 3)) {
		/* 315~360 degree => tune fast panel output */
		vtotal -= range_fast;
		step = + 2;
	} else if (phase <= (cycle >> 3)) {
		/* 0~45 degree => tune slow panel output */
		vtotal += range_fast;
		step = -2;
	} else {/* 135~225 degree => keep still */
		vtotal = vtotal_ori;
		step = 0;
	}
	if (vtotal != vtotal_ori)
		WRITE_VPP_REG(ENCL_VIDEO_MAX_LNCNT, vtotal);
	if (cnt) {
		cnt--;
		/* WRITE_VPP_REG(ASSIST_SPARE8_REG1, cnt); */
		assist_cnt = cnt;
		if (cnt) {
			vpp_log[cnt][0] = stamp;
			vpp_log[cnt][1] = stamp_in;
			vpp_log[cnt][2] = stamp_out;
			vpp_log[cnt][3] = cycle;
			vpp_log[cnt][4] = phase;
			vpp_log[cnt][5] = vtotal;
			vpp_log[cnt][6] = step;
		} else {
			for (i = 127; i > 0; i--) {
				pr_amve_dbg("Ti=%10u Tio=%10u To=%10u CY=%6u ",
					    vpp_log[i][0],
					    vpp_log[i][1],
					    vpp_log[i][2],
					    vpp_log[i][3]);
				pr_amve_dbg("PH =%10u Vt=%4u S=%2d\n",
					    vpp_log[i][4],
					    vpp_log[i][5],
					    vpp_log[i][6]);
			}
		}
	}
}

void ve_frame_size_patch(unsigned int width, unsigned int height)
{
	unsigned int vpp_size = height | (width << 16);

	if (READ_VPP_REG(VPP_VE_H_V_SIZE) != vpp_size)
		WRITE_VPP_REG(VPP_VE_H_V_SIZE, vpp_size);
}

void ve_dnlp_latch_process(void)
{
	if (vecm_latch_flag & FLAG_VE_NEW_DNLP) {
		vecm_latch_flag &= ~FLAG_VE_NEW_DNLP;
		ve_set_v3_dnlp(&dnlp_curve_param_load);
	}

	if (vecm_latch_flag2 & BLE_WHE_UPDATE) {
		vecm_latch_flag2 &= ~BLE_WHE_UPDATE;
		ble_whe_param_update(&ble_whe_param_load);
	}

	if (dnlp_en && dnlp_status) {
		dnlp_status = 0;
		ve_set_dnlp_2();
		ve_enable_dnlp();
		pr_amve_dbg("\n[amve..] set vpp_enable_dnlp OK!!!\n");
	} else if (dnlp_en == 0 && !dnlp_status) {
		dnlp_status = 1;
		ve_disable_dnlp();
		pr_amve_dbg("\n[amve..] set vpp_disable_dnlp OK!!!\n");
	}
}

void ve_lc_latch_process(void)
{
	if (vecm_latch_flag & FLAG_VE_LC_CURV) {
		vecm_latch_flag &= ~FLAG_VE_LC_CURV;
		lc_load_curve(&lc_curve_parm_load);
	}
}

void ve_lcd_gamma_process(void)
{
	int viu_sel;
	struct tcon_gamma_table_s *ptable;
	struct gamma_data_s *p_gm;
	unsigned int tmp = 0;

	if (cpu_after_eq_t7()) {
		viu_sel = vpp_get_vout_viu_mux();
		viu_sel = viu_sel - 1;
		if (viu_sel < 0)
			viu_sel = 0;
		gamma_index = viu_sel;

		if (is_meson_t7_cpu()) {
			if (gamma_index == 0)
				gamma_index_sub = 1;
			else
				gamma_index_sub = 0;
		}
	} else {
		viu_sel = vpp_get_encl_viu_mux();

		if (gamma_loadprotect_en && gamma_disable_flag) {
			if (gamma_update_flag_r) {
				vpp_set_lcd_gamma_table(video_gamma_table_r.data,
					H_SEL_R, viu_sel);
				gamma_update_flag_r = 0;
				pr_amve_dbg("\n[amve..] update R_lcd_gamma_table OK!!!\n");
			}

			if (gamma_update_flag_g) {
				vpp_set_lcd_gamma_table(video_gamma_table_g.data,
					H_SEL_G, viu_sel);
				gamma_update_flag_g = 0;
				pr_amve_dbg("\n[amve..] update G_lcd_gamma_table OK!!!\n");
			}

			if (gamma_update_flag_b) {
				vpp_set_lcd_gamma_table(video_gamma_table_b.data,
					H_SEL_B, viu_sel);
				gamma_update_flag_b = 0;
				pr_amve_dbg("\n[amve..] update B_lcd_gamma_table OK!!!\n");
			}

			if (!gamma_update_flag_r &&
				!gamma_update_flag_g &&
				!gamma_update_flag_b) {
				tmp = READ_VPP_REG(L_GAMMA_CNTL_PORT);
				tmp = (tmp & 0xfffffffe) | gamma_en;

				pr_amve_dbg("[amve..] enable_val/gamma_en = %d/%d\n",
					tmp, gamma_en);

				if (viu_sel == 1) { /* viu1 vsync rdma */
					VSYNC_WRITE_VPP_REG(L_GAMMA_CNTL_PORT, tmp);
				} else { /* viu2 directly write, rdma todo */
					WRITE_VPP_REG(L_GAMMA_CNTL_PORT, tmp);
				}

				gamma_disable_flag = 0;
			}
		}
	}

	if (vecm_latch_flag & FLAG_GAMMA_TABLE_EN) {
		vecm_latch_flag &= ~FLAG_GAMMA_TABLE_EN;
		vpp_enable_lcd_gamma_table(viu_sel, 1);
		pr_amve_dbg("\n[amve..] set vpp_enable_lcd_gamma_table OK!!!\n");
	}

	if (vecm_latch_flag & FLAG_GAMMA_TABLE_DIS) {
		vecm_latch_flag &= ~FLAG_GAMMA_TABLE_DIS;
		vpp_disable_lcd_gamma_table(viu_sel, 1);
		pr_amve_dbg("\n[amve..] set vpp_disable_lcd_gamma_table OK!!!\n");
	}

	if (cpu_after_eq_t7() || chip_type_id == chip_a4) {
		if ((vecm_latch_flag & FLAG_GAMMA_TABLE_R) &&
			(vecm_latch_flag & FLAG_GAMMA_TABLE_G) &&
			(vecm_latch_flag & FLAG_GAMMA_TABLE_B)) {
			vecm_latch_flag &= ~FLAG_GAMMA_TABLE_R;
			vecm_latch_flag &= ~FLAG_GAMMA_TABLE_G;
			vecm_latch_flag &= ~FLAG_GAMMA_TABLE_B;
			if (chip_type_id == chip_a4) {
				p_gm = get_gm_data();
				lcd_gamma_api(gamma_index, p_gm->gm_tbl.gamma_r,
					p_gm->gm_tbl.gamma_g,
					p_gm->gm_tbl.gamma_b,
					1, 0);
			} else {
				lcd_gamma_api(gamma_index, video_gamma_table_r.data,
					video_gamma_table_g.data,
					video_gamma_table_b.data,
					1, 0);
			}
			pr_amve_dbg("\n[amve..] set vpp_set_lcd_gamma_table OK!!!\n");
		}
	} else {
		if (vecm_latch_flag & FLAG_GAMMA_TABLE_R) {
			vecm_latch_flag &= ~FLAG_GAMMA_TABLE_R;
			if (!gamma_loadprotect_en) {
				vpp_set_lcd_gamma_table(video_gamma_table_r.data,
					H_SEL_R, viu_sel);
				pr_amve_dbg("\n[amve..] set R vpp_set_lcd_gamma_table OK!!!\n");
			} else {
				gamma_update_flag_r = 1;
				if (!gamma_disable_flag) {
					vpp_disable_lcd_gamma_table(viu_sel, 0);
					gamma_disable_flag = 1;
				}
			}
		}

		if (vecm_latch_flag & FLAG_GAMMA_TABLE_G) {
			vecm_latch_flag &= ~FLAG_GAMMA_TABLE_G;
			if (!gamma_loadprotect_en) {
				vpp_set_lcd_gamma_table(video_gamma_table_g.data,
					H_SEL_G, viu_sel);
				pr_amve_dbg("\n[amve..] set G vpp_set_lcd_gamma_table OK!!!\n");
			} else {
				gamma_update_flag_g = 1;
				if (!gamma_disable_flag) {
					vpp_disable_lcd_gamma_table(viu_sel, 0);
					gamma_disable_flag = 1;
				}
			}
		}

		if (vecm_latch_flag & FLAG_GAMMA_TABLE_B) {
			vecm_latch_flag &= ~FLAG_GAMMA_TABLE_B;
			if (!gamma_loadprotect_en) {
				vpp_set_lcd_gamma_table(video_gamma_table_b.data,
					H_SEL_B, viu_sel);
				pr_amve_dbg("\n[amve..] set B vpp_set_lcd_gamma_table OK!!!\n");
			} else {
				gamma_update_flag_b = 1;
				if (!gamma_disable_flag) {
					vpp_disable_lcd_gamma_table(viu_sel, 0);
					gamma_disable_flag = 1;
				}
			}
		}
	}

	if (vecm_latch_flag & FLAG_RGB_OGO) {
		vecm_latch_flag &= ~FLAG_RGB_OGO;
		if (video_rgb_ogo_mode_sw) {
			if (video_rgb_ogo.en) {
				ptable = &video_gamma_table_r_adj;
				vpp_set_lcd_gamma_table(ptable->data,
							H_SEL_R, viu_sel);
				ptable = &video_gamma_table_g_adj;
				vpp_set_lcd_gamma_table(ptable->data,
							H_SEL_G, viu_sel);
				ptable = &video_gamma_table_b_adj;
				vpp_set_lcd_gamma_table(ptable->data,
							H_SEL_B, viu_sel);
			} else {
				ptable = &video_gamma_table_r;
				vpp_set_lcd_gamma_table(ptable->data,
							H_SEL_R, viu_sel);
				ptable = &video_gamma_table_g;
				vpp_set_lcd_gamma_table(ptable->data,
							H_SEL_G, viu_sel);
				ptable = &video_gamma_table_b;
				vpp_set_lcd_gamma_table(ptable->data,
							H_SEL_B, viu_sel);
			}
			pr_amve_dbg("\n[amve] vpp_set_lcd_gamma_table OK\n");
		} else {
			vpp_set_rgb_ogo(&video_rgb_ogo);
			if (is_meson_t7_cpu()) {
				vpp_set_rgb_ogo_sub(&video_rgb_ogo_sub);
				pr_amve_dbg("\n[amve] set_rgb_ogo_sub OK!!!\n");
			}
			pr_amve_dbg("\n[amve] vpp_set_rgb_ogo OK!!!\n");
		}
	}

	/*for t7 vpp1 lcd gamma*/
	if (is_meson_t7_cpu()) {
		if (vecm_latch_flag2 & FLAG_GAMMA_TABLE_EN_SUB) {
			vecm_latch_flag2 &= ~FLAG_GAMMA_TABLE_EN_SUB;
			vpp_enable_lcd_gamma_table(gamma_index_sub, 1);
			pr_amve_dbg("\n[amve..] set enable_lcd_gamma_sub OK!!!\n");
		}

		if (vecm_latch_flag2 & FLAG_GAMMA_TABLE_DIS_SUB) {
			vecm_latch_flag2 &= ~FLAG_GAMMA_TABLE_DIS_SUB;
			vpp_disable_lcd_gamma_table(gamma_index_sub, 1);
			pr_amve_dbg("\n[amve..] set disable_lcd_gamma_sub OK!!!\n");
		}

		if ((vecm_latch_flag2 & FLAG_GAMMA_TABLE_R_SUB) &&
			(vecm_latch_flag2 & FLAG_GAMMA_TABLE_G_SUB) &&
			(vecm_latch_flag2 & FLAG_GAMMA_TABLE_B_SUB)) {
			vecm_latch_flag2 &= ~FLAG_GAMMA_TABLE_R_SUB;
			vecm_latch_flag2 &= ~FLAG_GAMMA_TABLE_G_SUB;
			vecm_latch_flag2 &= ~FLAG_GAMMA_TABLE_B_SUB;
			lcd_gamma_api(gamma_index_sub,
				video_gamma_table_r_sub.data,
				video_gamma_table_g_sub.data,
				video_gamma_table_b_sub.data,
				1, 0);
			pr_amve_dbg("\n[amve] set_lcd_gamma_table_sub OK!!!\n");
		}
	}
}

void lvds_freq_process(void)
{
/* #if ((MESON_CPU_TYPE==MESON_CPU_TYPE_MESON6TV)|| */
/* (MESON_CPU_TYPE==MESON_CPU_TYPE_MESON6TVD)) */
/*  lvds freq 50Hz/60Hz */
/* if (frame_lock_freq == 1){//50 hz */
/* // panel freq is 60Hz => change back to 50Hz */
/* if (READ_VPP_REG(ENCP_VIDEO_MAX_LNCNT) < 1237) */
/* (1124 + 1349 +1) / 2 */
/* WRITE_VPP_REG(ENCP_VIDEO_MAX_LNCNT, 1349); */
/* } */
/* else if (frame_lock_freq == 2){//60 hz */
/* // panel freq is 50Hz => change back to 60Hz */
/* if(READ_VPP_REG(ENCP_VIDEO_MAX_LNCNT) >= 1237) */
/* (1124 + 1349 + 1) / 2 */
/* WRITE_VPP_REG(ENCP_VIDEO_MAX_LNCNT, 1124); */
/* } */
/* else if (frame_lock_freq == 0){ */
/*  lvds freq 50Hz/60Hz */
/* if (vecm_latch_flag & FLAG_LVDS_FREQ_SW){  //50 hz */
/* // panel freq is 60Hz => change back to 50Hz */
/* if (READ_VPP_REG(ENCP_VIDEO_MAX_LNCNT) < 1237) */
/* (1124 + 1349 +1) / 2 */
/* WRITE_VPP_REG(ENCP_VIDEO_MAX_LNCNT, 1349); */
/* }else{	 //60 hz */
/* // panel freq is 50Hz => change back to 60Hz */
/* if (READ_VPP_REG(ENCP_VIDEO_MAX_LNCNT) >= 1237) */
/* (1124 + 1349 + 1) / 2 */
/* WRITE_VPP_REG(ENCP_VIDEO_MAX_LNCNT, 1124); */
/* } */
/* } */
/* #endif */
}

void ve_dnlp_param_update(void)
{
	vecm_latch_flag |= FLAG_VE_DNLP;
}

void ve_new_dnlp_param_update(void)
{
	vecm_latch_flag |= FLAG_VE_NEW_DNLP;
}

void ve_lc_curve_update(void)
{
	vecm_latch_flag |= FLAG_VE_LC_CURV;
}

static void video_data_limitation(int *val)
{
	if (*val > 1023)
		*val = 1023;
	if (*val < 0)
		*val = 0;
}

static void video_lookup(struct tcon_gamma_table_s *tbl, int *val)
{
	unsigned int idx = (*val) >> 2, mod = (*val) & 3;

	if (idx < 255)
		*val = tbl->data[idx] +
		(((tbl->data[idx + 1] - tbl->data[idx]) * mod + 2) >> 2);
	else
		*val = tbl->data[idx] +
		(((1023 - tbl->data[idx]) * mod + 2) >> 2);
}

static void video_set_rgb_ogo(void)
{
	int i = 0, r = 0, g = 0, b = 0;

	for (i = 0; i < 256; i++) {
		r = video_curve_2d2.data[i];
		g = video_curve_2d2.data[i];
		b = video_curve_2d2.data[i];
		/* Pre_offset */
		r += video_rgb_ogo.r_pre_offset;
		g += video_rgb_ogo.g_pre_offset;
		b += video_rgb_ogo.b_pre_offset;
		video_data_limitation(&r);
		video_data_limitation(&g);
		video_data_limitation(&b);
		/* Gain */
		r  *= video_rgb_ogo.r_gain;
		r >>= 10;
		g  *= video_rgb_ogo.g_gain;
		g >>= 10;
		b  *= video_rgb_ogo.b_gain;
		b >>= 10;
		video_data_limitation(&r);
		video_data_limitation(&g);
		video_data_limitation(&b);
		/* Post_offset */
		r += video_rgb_ogo.r_post_offset;
		g += video_rgb_ogo.g_post_offset;
		b += video_rgb_ogo.b_post_offset;
		video_data_limitation(&r);
		video_data_limitation(&g);
		video_data_limitation(&b);
		video_lookup(&video_curve_2d2_inv, &r);
		video_lookup(&video_curve_2d2_inv, &g);
		video_lookup(&video_curve_2d2_inv, &b);
		/* Get gamma_ogo = curve_2d2_inv_ogo * gamma */
		video_lookup(&video_gamma_table_r, &r);
		video_lookup(&video_gamma_table_g, &g);
		video_lookup(&video_gamma_table_b, &b);
		/* Save gamma_ogo */
		video_gamma_table_r_adj.data[i] = r;
		video_gamma_table_g_adj.data[i] = g;
		video_gamma_table_b_adj.data[i] = b;
	}
}

void ve_ogo_param_update_sub(void)
{
	if (video_rgb_ogo_sub.en > 1)
		video_rgb_ogo_sub.en = 1;
	if (video_rgb_ogo_sub.r_pre_offset > 1023)
		video_rgb_ogo_sub.r_pre_offset = 1023;
	if (video_rgb_ogo_sub.r_pre_offset < -1024)
		video_rgb_ogo_sub.r_pre_offset = -1024;
	if (video_rgb_ogo_sub.g_pre_offset > 1023)
		video_rgb_ogo_sub.g_pre_offset = 1023;
	if (video_rgb_ogo_sub.g_pre_offset < -1024)
		video_rgb_ogo_sub.g_pre_offset = -1024;
	if (video_rgb_ogo_sub.b_pre_offset > 1023)
		video_rgb_ogo_sub.b_pre_offset = 1023;
	if (video_rgb_ogo_sub.b_pre_offset < -1024)
		video_rgb_ogo_sub.b_pre_offset = -1024;
	if (video_rgb_ogo_sub.r_gain > 2047)
		video_rgb_ogo_sub.r_gain = 2047;
	if (video_rgb_ogo_sub.g_gain > 2047)
		video_rgb_ogo_sub.g_gain = 2047;
	if (video_rgb_ogo_sub.b_gain > 2047)
		video_rgb_ogo_sub.b_gain = 2047;
	if (video_rgb_ogo_sub.r_post_offset > 1023)
		video_rgb_ogo_sub.r_post_offset = 1023;
	if (video_rgb_ogo_sub.r_post_offset < -1024)
		video_rgb_ogo_sub.r_post_offset = -1024;
	if (video_rgb_ogo_sub.g_post_offset > 1023)
		video_rgb_ogo_sub.g_post_offset = 1023;
	if (video_rgb_ogo_sub.g_post_offset < -1024)
		video_rgb_ogo_sub.g_post_offset = -1024;
	if (video_rgb_ogo_sub.b_post_offset > 1023)
		video_rgb_ogo_sub.b_post_offset = 1023;
	if (video_rgb_ogo_sub.b_post_offset < -1024)
		video_rgb_ogo_sub.b_post_offset = -1024;

	vecm_latch_flag |= FLAG_RGB_OGO;
}

void ve_ogo_param_update(void)
{
	if (video_rgb_ogo.en > 1)
		video_rgb_ogo.en = 1;
	if (video_rgb_ogo.r_pre_offset > 1023)
		video_rgb_ogo.r_pre_offset = 1023;
	if (video_rgb_ogo.r_pre_offset < -1024)
		video_rgb_ogo.r_pre_offset = -1024;
	if (video_rgb_ogo.g_pre_offset > 1023)
		video_rgb_ogo.g_pre_offset = 1023;
	if (video_rgb_ogo.g_pre_offset < -1024)
		video_rgb_ogo.g_pre_offset = -1024;
	if (video_rgb_ogo.b_pre_offset > 1023)
		video_rgb_ogo.b_pre_offset = 1023;
	if (video_rgb_ogo.b_pre_offset < -1024)
		video_rgb_ogo.b_pre_offset = -1024;
	if (video_rgb_ogo.r_gain > 2047)
		video_rgb_ogo.r_gain = 2047;
	if (video_rgb_ogo.g_gain > 2047)
		video_rgb_ogo.g_gain = 2047;
	if (video_rgb_ogo.b_gain > 2047)
		video_rgb_ogo.b_gain = 2047;
	if (video_rgb_ogo.r_post_offset > 1023)
		video_rgb_ogo.r_post_offset = 1023;
	if (video_rgb_ogo.r_post_offset < -1024)
		video_rgb_ogo.r_post_offset = -1024;
	if (video_rgb_ogo.g_post_offset > 1023)
		video_rgb_ogo.g_post_offset = 1023;
	if (video_rgb_ogo.g_post_offset < -1024)
		video_rgb_ogo.g_post_offset = -1024;
	if (video_rgb_ogo.b_post_offset > 1023)
		video_rgb_ogo.b_post_offset = 1023;
	if (video_rgb_ogo.b_post_offset < -1024)
		video_rgb_ogo.b_post_offset = -1024;
	if (video_rgb_ogo_mode_sw)
		video_set_rgb_ogo();

	vecm_latch_flag |= FLAG_RGB_OGO;
}

/* sharpness process begin */
void sharpness_process(struct vframe_s *vf)
{
}

/* sharpness process end */

/*for gxbbtv rgb contrast adj in vd1 matrix */
void vpp_vd1_mtx_rgb_contrast(signed int cont_val, struct vframe_s *vf)
{
	unsigned int vd1_contrast;
	unsigned int con_minus_value, rgb_con_en;

	if (cont_val > 1023 || cont_val < -1024)
		return;
	cont_val = cont_val + 1024;
	/*close rgb contrast protect*/
	VSYNC_WRITE_VPP_REG_BITS(XVYCC_VD1_RGB_CTRST, 0, 0, 1);
	/*VPP_VADJ_CTRL bit 1 on for rgb contrast adj*/
	rgb_con_en = READ_VPP_REG_BITS(XVYCC_VD1_RGB_CTRST, 1, 1);
	if (!rgb_con_en)
		VSYNC_WRITE_VPP_REG_BITS(XVYCC_VD1_RGB_CTRST, 1, 1, 1);

	/*select full or limit range setting*/
	con_minus_value = READ_VPP_REG_BITS(XVYCC_VD1_RGB_CTRST, 4, 10);
	if (vf->source_type == VFRAME_SOURCE_TYPE_OTHERS) {
		if (con_minus_value != 64)
			VSYNC_WRITE_VPP_REG_BITS(XVYCC_VD1_RGB_CTRST, 64, 4, 10);
	} else {
		if (con_minus_value != 0)
			VSYNC_WRITE_VPP_REG_BITS(XVYCC_VD1_RGB_CTRST, 0, 4, 10);
	}

	vd1_contrast = (READ_VPP_REG(XVYCC_VD1_RGB_CTRST) & 0xf000ffff) |
					(cont_val << 16);

	VSYNC_WRITE_VPP_REG(XVYCC_VD1_RGB_CTRST, vd1_contrast);
}

void vpp_contrast_adj_by_uv(int cont_u, int cont_v)
{
	unsigned int coef00 = 0;
	unsigned int coef01 = 0;
	unsigned int coef02 = 0;
	unsigned int coef10 = 0;
	unsigned int coef11 = 0;
	unsigned int coef12 = 0;
	unsigned int coef20 = 0;
	unsigned int coef21 = 0;
	unsigned int coef22 = 0;
	unsigned int offst0 = 0;
	unsigned int offst1 = 0;
	unsigned int offst2 = 0;
	unsigned int pre_offst0 = 0;
	unsigned int pre_offst1 = 0;
	unsigned int pre_offst2 = 0;
	unsigned int rs = 0;
	unsigned int en = 0;

	coef00 = matrix_yuv_bypass_coef[3];
	coef01 = matrix_yuv_bypass_coef[4];
	coef02 = matrix_yuv_bypass_coef[5];
	coef10 = matrix_yuv_bypass_coef[6];
	coef11 = cont_u;
	coef12 = matrix_yuv_bypass_coef[8];
	coef20 = matrix_yuv_bypass_coef[9];
	coef21 = matrix_yuv_bypass_coef[10];
	coef22 = cont_v;
	pre_offst0 = matrix_yuv_bypass_coef[0];
	pre_offst1 = matrix_yuv_bypass_coef[1];
	pre_offst2 = matrix_yuv_bypass_coef[2];
	offst0 = matrix_yuv_bypass_coef[12];
	offst1 = matrix_yuv_bypass_coef[13];
	offst2 = matrix_yuv_bypass_coef[14];
	rs = matrix_yuv_bypass_coef[15];
	en = matrix_yuv_bypass_coef[16];

	VSYNC_WRITE_VPP_REG(VPP_VD1_MATRIX_COEF00_01,
			    ((coef00 & 0x1fff) << 16) | (coef01 & 0x1fff));
	VSYNC_WRITE_VPP_REG(VPP_VD1_MATRIX_COEF02_10,
			    ((coef02 & 0x1fff) << 16) | (coef10 & 0x1fff));
	VSYNC_WRITE_VPP_REG(VPP_VD1_MATRIX_COEF11_12,
			    ((coef11 & 0x1fff) << 16) | (coef12 & 0x1fff));
	VSYNC_WRITE_VPP_REG(VPP_VD1_MATRIX_COEF20_21,
			    ((coef20 & 0x1fff) << 16) | (coef21 & 0x1fff));
	VSYNC_WRITE_VPP_REG(VPP_VD1_MATRIX_COEF22,
			    (coef22 & 0x1fff));
	VSYNC_WRITE_VPP_REG(VPP_VD1_MATRIX_OFFSET0_1,
			    ((offst0 & 0xfff) << 16) | (offst1 & 0xfff));
	VSYNC_WRITE_VPP_REG(VPP_VD1_MATRIX_OFFSET2,
			    (offst2 & 0xfff));
	VSYNC_WRITE_VPP_REG(VPP_VD1_MATRIX_PRE_OFFSET0_1,
			    ((pre_offst0 & 0xfff) << 16) |
			    (pre_offst1 & 0xfff));
	VSYNC_WRITE_VPP_REG(VPP_VD1_MATRIX_PRE_OFFSET2,
			    (pre_offst2 & 0xfff));

	VSYNC_WRITE_VPP_REG_BITS(VPP_VD1_MATRIX_CLIP, rs, 5, 3);
	VSYNC_WRITE_VPP_REG_BITS(VPP_VD1_MATRIX_EN_CTRL, en, 0, 1);
}

/*for gxbbtv contrast adj in vadj1*/
void vpp_vd_adj1_contrast(signed int cont_val, struct vframe_s *vf)
{
	unsigned int vd1_contrast;
	unsigned int vdj1_ctl;
	int contrast_uv;
	int contrast_u;
	int contrast_v;

	if (cont_val > 1023 || cont_val < -1024)
		return;
	contrast_uv = cont_val + 1024;
	cont_val = ((cont_val + 1024) >> 3);

	if (contrast_uv < 1024) {
		contrast_u = contrast_uv;
		contrast_v = contrast_uv;
	} else {
		contrast_u = (0x600 - 0x400) * (contrast_uv - 0x400) / 0x400
			+ 0x400;
		contrast_v = contrast_uv;
	}

	if (is_meson_gxtvbb_cpu()) {
		/*VPP_VADJ_CTRL bit 1 off for contrast adj*/
		vdj1_ctl = READ_VPP_REG_BITS(VPP_VADJ_CTRL, 1, 1);
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
		if (vf->source_type == VFRAME_SOURCE_TYPE_OTHERS) {
			if (!vdj1_ctl)
				VSYNC_WRITE_VPP_REG_BITS(VPP_VADJ_CTRL, 1, 1, 1);
		} else {
			if (vdj1_ctl)
				VSYNC_WRITE_VPP_REG_BITS(VPP_VADJ_CTRL, 0, 1, 1);
		}
#endif
	}
	if (chip_type_id == chip_s5) {
		ve_contrast_set(cont_val, VE_VADJ1, WR_DMA);
		return;
	} else if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A) {
		vd1_contrast = (READ_VPP_REG(VPP_VADJ1_Y_2) & 0x7ff00) |
						(cont_val << 0);
		//VSYNC_WRITE_VPP_REG(VPP_VADJ1_Y_2, vd1_contrast);
		VSYNC_WRITE_VPP_REG_BITS(VPP_VADJ1_Y_2, cont_val, 0, 8);

		vpp_contrast_adj_by_uv(contrast_u, contrast_v);
		return;
	} else if (get_cpu_type() > MESON_CPU_MAJOR_ID_GXTVBB) {
		vd1_contrast = (READ_VPP_REG(VPP_VADJ1_Y) & 0x3ff00) |
						(cont_val << 0);
	} else {
		vd1_contrast = (READ_VPP_REG(VPP_VADJ1_Y) & 0x1ff00) |
						(cont_val << 0);
	}
	VSYNC_WRITE_VPP_REG_BITS(VPP_VADJ1_Y, cont_val, 0, 8);
}

void vpp_vd_adj1_brightness(signed int bri_val, struct vframe_s *vf)
{
	signed int vd1_brightness;

	signed int ao0 =  -64;
	signed int ao1 = -512;
	signed int ao2 = -512;
	unsigned int a01 =    0, a_2 =    0;
	/* enable vd0_csc */

	unsigned int ori = READ_VPP_REG(VPP_MATRIX_CTRL) | 0x00000020;
	/* point to vd0_csc */
	unsigned int ctl = (ori & 0xfffffcff) | 0x00000100;

	if (bri_val > 1023 || bri_val < -1024)
		return;

	if (chip_type_id == chip_s5) {
		ve_brigtness_set(bri_val, VE_VADJ1, WR_DMA);
	} else if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A) {
		vd1_brightness = (READ_VPP_REG(VPP_VADJ1_Y_2) & 0xff) |
			(bri_val << 8);

		//WRITE_VPP_REG(VPP_VADJ1_Y_2, vd1_brightness);
		VSYNC_WRITE_VPP_REG_BITS(VPP_VADJ1_Y_2, bri_val, 8, 11);
	} else if (get_cpu_type() > MESON_CPU_MAJOR_ID_GXTVBB) {
		bri_val = bri_val >> 1;
		vd1_brightness = (READ_VPP_REG(VPP_VADJ1_Y) & 0xff) |
			(bri_val << 8);

		VSYNC_WRITE_VPP_REG_BITS(VPP_VADJ1_Y, bri_val, 8, 10);
	} else {
		if (vf->source_type == VFRAME_SOURCE_TYPE_TUNER ||
		    vf->source_type == VFRAME_SOURCE_TYPE_CVBS ||
		    vf->source_type == VFRAME_SOURCE_TYPE_COMP)
			vd1_brightness = bri_val;
		else if (vf->source_type == VFRAME_SOURCE_TYPE_HDMI) {
			if ((((vf->signal_type >> 29) & 0x1) == 1) &&
			    (((vf->signal_type >> 16) & 0xff) == 9)) {
				bri_val += ao0;
				if (bri_val < -1024)
					bri_val = -1024;
				vd1_brightness = bri_val;
			} else {
				vd1_brightness = bri_val;
			}
		} else {
			bri_val += ao0;
			if (bri_val < -1024)
				bri_val = -1024;
			vd1_brightness = bri_val;
		}

		a01 = ((vd1_brightness << 16) & 0x0fff0000) |
				((ao1 <<  0) & 0x00000fff);
		a_2 = ((ao2 <<	0) & 0x00000fff);
		/*p01 = ((po0 << 16) & 0x0fff0000) |*/
				/*((po1 <<  0) & 0x00000fff);*/
		/*p_2 = ((po2 <<	0) & 0x00000fff);*/

		WRITE_VPP_REG(VPP_MATRIX_CTRL, ctl);
		WRITE_VPP_REG(VPP_MATRIX_PRE_OFFSET0_1, a01);
		WRITE_VPP_REG(VPP_MATRIX_PRE_OFFSET2, a_2);
		WRITE_VPP_REG(VPP_MATRIX_CTRL, ori);
	}
}

/* brightness/contrast adjust process begin */
static void vd1_brightness_contrast(signed int brightness,
				    signed int contrast)
{
	signed int ao0 =  -64, g00 = 1024, g01 =    0, g02 =    0, po0 =  64;
	signed int ao1 = -512, g10 =    0, g11 = 1024, g12 =    0, po1 = 512;
	signed int ao2 = -512, g20 =    0, g21 =    0, g22 = 1024, po2 = 512;
	unsigned int gc0 =    0, gc1 =    0, gc2 =    0, gc3 =    0, gc4 = 0;
	unsigned int a01 =    0, a_2 =    0, p01 =    0, p_2 =    0;
	/* enable vd0_csc */
	unsigned int ori = READ_VPP_REG(VPP_MATRIX_CTRL) | 0x00000020;
	/* point to vd0_csc */
	unsigned int ctl = (ori & 0xfffffcff) | 0x00000100;

	po0 += brightness >> 1;
	if (po0 >  1023)
		po0 =  1023;
	if (po0 < -1024)
		po0 = -1024;
	g00  *= contrast + 2048;
	g00 >>= 11;
	if (g00 >  4095)
		g00 =  4095;
	if (g00 < -4096)
		g00 = -4096;
	if (contrast < 0) {
		g11  *= contrast   + 2048;
		g11 >>= 11;
	}
	if (brightness < 0) {
		g11  += brightness >> 1;
		if (g11 >  4095)
			g11 =  4095;
		if (g11 < -4096)
			g11 = -4096;
	}
	if (contrast < 0) {
		g22  *= contrast   + 2048;
		g22 >>= 11;
	}
	if (brightness < 0) {
		g22  += brightness >> 1;
		if (g22 >  4095)
			g22 =  4095;
		if (g22 < -4096)
			g22 = -4096;
	}
	gc0 = ((g00 << 16) & 0x1fff0000) | ((g01 <<  0) & 0x00001fff);
	gc1 = ((g02 << 16) & 0x1fff0000) | ((g10 <<  0) & 0x00001fff);
	gc2 = ((g11 << 16) & 0x1fff0000) | ((g12 <<  0) & 0x00001fff);
	gc3 = ((g20 << 16) & 0x1fff0000) | ((g21 <<  0) & 0x00001fff);
	gc4 = ((g22 <<  0) & 0x00001fff);
	/* #if (MESON_CPU_TYPE >= MESON_CPU_TYPE_MESONG9TV) */
	if (is_meson_gxtvbb_cpu()) {
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
		a01 = ((ao0 << 16) & 0x0fff0000) |
				((ao1 <<  0) & 0x00000fff);
		a_2 = ((ao2 <<  0) & 0x00000fff);
		p01 = ((po0 << 16) & 0x0fff0000) |
				((po1 <<  0) & 0x00000fff);
		p_2 = ((po2 <<  0) & 0x00000fff);
#endif
	} else {
		/* #else */
		a01 = ((ao0 << 16) & 0x07ff0000) |
				((ao1 <<  0) & 0x000007ff);
		a_2 = ((ao2 <<  0) & 0x000007ff);
		p01 = ((po0 << 16) & 0x07ff0000) |
				((po1 <<  0) & 0x000007ff);
		p_2 = ((po2 <<  0) & 0x000007ff);
	}
	/* #endif */
	WRITE_VPP_REG(VPP_MATRIX_CTRL, ctl);
	WRITE_VPP_REG(VPP_MATRIX_COEF00_01, gc0);
	WRITE_VPP_REG(VPP_MATRIX_COEF02_10, gc1);
	WRITE_VPP_REG(VPP_MATRIX_COEF11_12, gc2);
	WRITE_VPP_REG(VPP_MATRIX_COEF20_21, gc3);
	WRITE_VPP_REG(VPP_MATRIX_COEF22, gc4);
	WRITE_VPP_REG(VPP_MATRIX_PRE_OFFSET0_1, a01);
	WRITE_VPP_REG(VPP_MATRIX_PRE_OFFSET2, a_2);
	WRITE_VPP_REG(VPP_MATRIX_OFFSET0_1, p01);
	WRITE_VPP_REG(VPP_MATRIX_OFFSET2, p_2);
	WRITE_VPP_REG(VPP_MATRIX_CTRL, ori);
}

void amvecm_bricon_process(signed int bri_val,
			   signed int cont_val, struct vframe_s *vf)
{
	if (vecm_latch_flag & FLAG_VADJ1_BRI) {
		vecm_latch_flag &= ~FLAG_VADJ1_BRI;
		vpp_vd_adj1_brightness(bri_val, vf);
		pr_amve_dbg("\n[amve..] set vd1_brightness OK!!!\n");
		if (amve_debug & 0x100)
			pr_info("\n[amve..]%s :brightness:%d!!!\n",
				__func__, bri_val);
	}

	if (vecm_latch_flag & FLAG_VADJ1_CON) {
		vecm_latch_flag &= ~FLAG_VADJ1_CON;
		if (contrast_adj_sel)
			vpp_vd1_mtx_rgb_contrast(cont_val, vf);
		else
			vpp_vd_adj1_contrast(cont_val, vf);
		pr_amve_dbg("\n[amve..] set vd1_contrast OK!!!\n");
		if (amve_debug & 0x100)
			pr_info("\n[amve..]%s :contrast:%d!!!\n",
				__func__, cont_val);
	}

	if (0) { /* vecm_latch_flag & FLAG_BRI_CON) { */
		vecm_latch_flag &= ~FLAG_BRI_CON;
		vd1_brightness_contrast(bri_val, cont_val);
		pr_amve_dbg("\n[amve..] set vd1_brightness_contrast OK!!!\n");
	}
}

/* brightness/contrast adjust process end */

void amvecm_color_process(signed int sat_val,
			  signed int hue_val, struct vframe_s *vf)
{
	if (vecm_latch_flag & FLAG_VADJ1_COLOR) {
		vecm_latch_flag &= ~FLAG_VADJ1_COLOR;
		vpp_vd_adj1_saturation_hue(sat_val, hue_val, vf);
		if (amve_debug & 0x100)
			pr_info("\n[amve..]%s :saturation:%d,hue:%d!!!\n",
				__func__, sat_val, hue_val);
	}
}

/* saturation/hue adjust process end */

/* 3d process begin */
void amvecm_3d_black_process(void)
{
	if (vecm_latch_flag & FLAG_3D_BLACK_DIS) {
		/* disable reg_3dsync_enable */
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC1, 0, 31, 1);
		WRITE_VPP_REG_BITS(VIU_MISC_CTRL0, 0, 8, 1);
		WRITE_VPP_REG_BITS(VPP_BLEND_ONECOLOR_CTRL, 0, 26, 1);
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC1, 0, 13, 1);
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC2, 0, 31, 1);
		vecm_latch_flag &= ~FLAG_3D_BLACK_DIS;
	}
	if (vecm_latch_flag & FLAG_3D_BLACK_EN) {
		WRITE_VPP_REG_BITS(VIU_MISC_CTRL0, 1, 8, 1);
		WRITE_VPP_REG_BITS(VPP_BLEND_ONECOLOR_CTRL, 1, 26, 1);
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC2, 1, 31, 1);
		WRITE_VPP_REG_BITS(VPP_BLEND_ONECOLOR_CTRL,
				   sync_3d_black_color & 0xffffff, 0, 24);
		if (sync_3d_sync_to_vbo)
			WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC1, 1, 13, 1);
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC1, 1, 31, 1);
		vecm_latch_flag &= ~FLAG_3D_BLACK_EN;
	}
}

void amvecm_3d_sync_process(void)
{
	if (vecm_latch_flag & FLAG_3D_SYNC_DIS) {
		/* disable reg_3dsync_enable */
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC1, 0, 31, 1);
		vecm_latch_flag &= ~FLAG_3D_SYNC_DIS;
	}
	if (vecm_latch_flag & FLAG_3D_SYNC_EN) {
		/*select vpu pwm source clock*/
		switch (READ_VPP_REG_BITS(VPU_VIU_VENC_MUX_CTRL, 0, 2)) {
		case 0:/* ENCL */
			WRITE_VPP_REG_BITS(VPU_VPU_PWM_V0, 0, 29, 2);
			break;
		case 1:/* ENCI */
			WRITE_VPP_REG_BITS(VPU_VPU_PWM_V0, 1, 29, 2);
			break;
		case 2:/* ENCP */
			WRITE_VPP_REG_BITS(VPU_VPU_PWM_V0, 2, 29, 2);
			break;
		case 3:/* ENCT */
			WRITE_VPP_REG_BITS(VPU_VPU_PWM_V0, 3, 29, 2);
			break;
		default:
			break;
		}
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC2, sync_3d_h_start, 0, 13);
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC2, sync_3d_h_end, 16, 13);
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC1, sync_3d_v_start, 0, 13);
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC1, sync_3d_v_end, 16, 13);
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC1, sync_3d_polarity, 29, 1);
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC1, sync_3d_out_inv, 15, 1);
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC1, 1, 31, 1);
		vecm_latch_flag &= ~FLAG_3D_SYNC_EN;
	}
}

/* 3d process end */

/*gxlx sr adaptive param*/
#define SR_SD_SCALE_LEVEL 0x1
#define SR_HD_SCALE_LEVEL 0x2
#define SR_4k_LEVEL 0x4
#define SR_CVBS_LEVEL 0x8
#define SR_NOSCALE_LEVEL 0x10
static void amve_sr_reg_setting(unsigned int adaptive_level)
{
	if (adaptive_level & SR_SD_SCALE_LEVEL) {
		am_set_regmap(&sr1reg_sd_scale);
	} else if (adaptive_level & SR_HD_SCALE_LEVEL) {
		am_set_regmap(&sr1reg_hd_scale);
	} else if (adaptive_level & SR_4k_LEVEL) {
		amvecm_sharpness_enable(1);/*peaking*/
		amvecm_sharpness_enable(3);/*lcti*/
		amvecm_sharpness_enable(5);/*drtlpf theta*/
		amvecm_sharpness_enable(7);/*debanding*/
		amvecm_sharpness_enable(9);/*dejaggy*/
		amvecm_sharpness_enable(11);/*dering*/
		amvecm_sharpness_enable(13);/*drlpf*/
	} else if (adaptive_level & SR_CVBS_LEVEL) {
		am_set_regmap(&sr1reg_cvbs);
	} else if (adaptive_level & SR_NOSCALE_LEVEL) {
		am_set_regmap(&sr1reg_hv_noscale);
	}
}

void amve_sharpness_adaptive_setting(struct vframe_s *vf,
				     unsigned int sps_h_en,
				     unsigned int sps_v_en)
{
	static unsigned int adaptive_level = 1;
	unsigned int cur_level;
	unsigned int width, height;
	struct vinfo_s *vinfo = get_current_vinfo();

	if (!vf)
		return;
	if (vinfo->mode == VMODE_CVBS) {
		cur_level = SR_CVBS_LEVEL;
	} else {
		width = (vf->type & VIDTYPE_COMPRESS) ?
			vf->compWidth : vf->width;
		height = (vf->type & VIDTYPE_COMPRESS) ?
			vf->compHeight : vf->height;

		if (sps_h_en == 1 && sps_v_en == 1) {
			/*super scaler h and v scale up x2 */
			if (height >= 2160 && width >= 3840)
				cur_level = SR_4k_LEVEL;
			else if (height >= 720 && width >= 1280)
				cur_level = SR_HD_SCALE_LEVEL;
			else
				cur_level = SR_SD_SCALE_LEVEL;
		} else {
			/*1. super scaler no up scale */
			/*2. super scaler h up scale x2*/
			if (height >= 2160 && width >= 3840)
				cur_level = SR_4k_LEVEL;
			else
				cur_level = SR_NOSCALE_LEVEL;
		}
	}

	if (adaptive_level == cur_level)
		return;

	amve_sr_reg_setting(cur_level);
	adaptive_level = cur_level;
	sr_adapt_level = adaptive_level;
	pr_amve_dbg("\n[amcm..]sr_adapt_level = %d :1->sd;2->hd;4->4k\n",
		    sr_adapt_level);
}

/*gxlx sr init*/
void amve_sharpness_init(void)
{
	am_set_regmap(&sr1reg_sd_scale);
}

static int overscan_timing = TIMING_MAX;
module_param(overscan_timing, uint, 0664);
MODULE_PARM_DESC(overscan_timing, "\n overscan_control\n");

static int overscan_screen_mode = 0xff;
module_param(overscan_screen_mode, uint, 0664);
MODULE_PARM_DESC(overscan_screen_mode, "\n overscan_screen_mode\n");

static int overscan_disable;
module_param(overscan_disable, uint, 0664);
MODULE_PARM_DESC(overscan_disable, "\n overscan_disable\n");

void amvecm_fresh_overscan(struct vframe_s *vf)
{
	unsigned int height = 0;
	unsigned int cur_overscan_timing = 0;
#ifdef CONFIG_AMLOGIC_MEDIA_TVIN
	unsigned int cur_fmt;
	unsigned int offset = TIMING_UHD + 1;/*av&atv*/
#endif

	if (overscan_disable)
		return;

	if (overscan_table[0].load_flag) {
		height = (vf->type & VIDTYPE_COMPRESS) ?
			vf->compHeight : vf->height;
		if (height <= 480)
			cur_overscan_timing = TIMING_SD_480;
		else if (height <= 576)
			cur_overscan_timing = TIMING_SD_576;
		else if (height <= 720)
			cur_overscan_timing = TIMING_HD;
		else if (height <= 1088)
			cur_overscan_timing = TIMING_FHD;
		else
			cur_overscan_timing = TIMING_UHD;

		overscan_timing = cur_overscan_timing;
		overscan_screen_mode =
			overscan_table[overscan_timing].screen_mode;

		vf->pic_mode.AFD_enable =
			overscan_table[overscan_timing].afd_enable;
		/*local play screen mode set by decoder*/
		if (!(vf->pic_mode.screen_mode == VIDEO_WIDEOPTION_CUSTOM &&
		      vf->pic_mode.AFD_enable)) {
			if (overscan_table[0].source == SOURCE_MPEG)
				vf->pic_mode.screen_mode = 0xff;
			else
				vf->pic_mode.screen_mode = overscan_screen_mode;
		}

		if (vf->pic_mode.provider == PIC_MODE_PROVIDER_WSS &&
		    vf->pic_mode.AFD_enable) {
			vf->pic_mode.hs += overscan_table[overscan_timing].hs;
			vf->pic_mode.he += overscan_table[overscan_timing].he;
			vf->pic_mode.vs += overscan_table[overscan_timing].vs;
			vf->pic_mode.ve += overscan_table[overscan_timing].ve;
		} else {
			vf->pic_mode.hs = overscan_table[overscan_timing].hs;
			vf->pic_mode.he = overscan_table[overscan_timing].he;
			vf->pic_mode.vs = overscan_table[overscan_timing].vs;
			vf->pic_mode.ve = overscan_table[overscan_timing].ve;
		}
		vf->ratio_control |= DISP_RATIO_ADAPTED_PICMODE;
	}
#ifdef CONFIG_AMLOGIC_MEDIA_TVIN
	if (overscan_table[offset].load_flag) {
		cur_fmt = vf->sig_fmt;
		if (cur_fmt == TVIN_SIG_FMT_CVBS_NTSC_M)
			cur_overscan_timing = TIMING_NTST_M;
		else if (cur_fmt == TVIN_SIG_FMT_CVBS_NTSC_443)
			cur_overscan_timing = TIMING_NTST_443;
		else if (cur_fmt == TVIN_SIG_FMT_CVBS_PAL_I)
			cur_overscan_timing = TIMING_PAL_I;
		else if (cur_fmt == TVIN_SIG_FMT_CVBS_PAL_M)
			cur_overscan_timing = TIMING_PAL_M;
		else if (cur_fmt == TVIN_SIG_FMT_CVBS_PAL_60)
			cur_overscan_timing = TIMING_PAL_60;
		else if (cur_fmt == TVIN_SIG_FMT_CVBS_PAL_CN)
			cur_overscan_timing = TIMING_PAL_CN;
		else if (cur_fmt == TVIN_SIG_FMT_CVBS_SECAM)
			cur_overscan_timing = TIMING_SECAM;
		else if (cur_fmt == TVIN_SIG_FMT_CVBS_NTSC_50)
			cur_overscan_timing = TIMING_NTSC_50;
		else
			return;
		overscan_timing = cur_overscan_timing;
		overscan_screen_mode =
			overscan_table[overscan_timing].screen_mode;
		vf->pic_mode.AFD_enable =
			overscan_table[overscan_timing].afd_enable;

		if (!(vf->pic_mode.screen_mode == VIDEO_WIDEOPTION_CUSTOM &&
		      vf->pic_mode.AFD_enable))
			vf->pic_mode.screen_mode = overscan_screen_mode;

		if (vf->pic_mode.provider == PIC_MODE_PROVIDER_WSS &&
		    vf->pic_mode.AFD_enable) {
			vf->pic_mode.hs += overscan_table[overscan_timing].hs;
			vf->pic_mode.he += overscan_table[overscan_timing].he;
			vf->pic_mode.vs += overscan_table[overscan_timing].vs;
			vf->pic_mode.ve += overscan_table[overscan_timing].ve;
		} else {
			vf->pic_mode.hs = overscan_table[overscan_timing].hs;
			vf->pic_mode.he = overscan_table[overscan_timing].he;
			vf->pic_mode.vs = overscan_table[overscan_timing].vs;
			vf->pic_mode.ve = overscan_table[overscan_timing].ve;
		}
		vf->ratio_control |= DISP_RATIO_ADAPTED_PICMODE;
	}
#endif
	if (pq_user_latch_flag & PQ_USER_OVERSCAN_RESET) {
		pq_user_latch_flag &= ~PQ_USER_OVERSCAN_RESET;
		vf->pic_mode.AFD_enable = 0;
		vf->pic_mode.screen_mode = 0;
		vf->pic_mode.hs = 0;
		vf->pic_mode.he = 0;
		vf->pic_mode.vs = 0;
		vf->pic_mode.ve = 0;
		vf->ratio_control &= ~DISP_RATIO_ADAPTED_PICMODE;
	}

}

void amvecm_reset_overscan(void)
{
	unsigned int offset = TIMING_UHD + 1;/*av&atv*/
	enum ve_source_input_e source0;

	source0 = overscan_table[0].source;
	if (overscan_disable)
		return;
	if (overscan_timing != TIMING_MAX) {
		overscan_timing = TIMING_MAX;
		if (source0 != SOURCE_DTV &&
		    source0 != SOURCE_MPEG)
			overscan_table[0].load_flag = 0;
		else if (!atv_source_flg)
			overscan_table[offset].load_flag = 0;
		if (source0 != SOURCE_DTV &&
		    source0 != SOURCE_MPEG &&
		    !atv_source_flg)
			overscan_screen_mode = 0xff;
	}
}

unsigned int *plut3d;
static int P3dlut_tab[289] = {0};
static int P3dlut_regtab[291] = {0};
static unsigned char *pkeylutall;
static unsigned int *pkeylut;
static unsigned int max_3dlut_count = 3;
#define LUT3DBIN_PATH "/vendor/etc/tvconfig/panel/3dlut.bin"
/*plut3d[]: 17*17*17*3*10bit for tl1*/
/*plut3d[]: 17*17*17*3*12bit for tm2*/
/*bfromkey == 0 data from input para */
/*bfromkey == 1 data from unifykey */
/*bfromkey == 2 data from bin file */
int vpp_set_lut3d(int bfromkey,
		  int keyindex,
		  unsigned int p3dlut_in[][3],
		  int blut3dcheck)
{
#ifdef CONFIG_AMLOGIC_LCD
	int i, key_len, key_count, ret, offset = 0;
#else
	int i, key_len, key_count, offset = 0;
#endif
	int d0, d1, d2, counter, size, index0, index1, idn;
	u32 dwtemp, ctltemp, wrgb[3];
	unsigned int combine3 = 0;
	struct file *fp;
	mm_segment_t fs;
	loff_t pos;

	if (!lut3d_en)
		return 1;

	if (!plut3d)
		return 1;

	mutex_lock(&vpp_lut3d_lock);

	/* load 3d lut from unifykey store */
	if (bfromkey) {
		/* allocate a little bit more buffer than required for safety */
		key_len = (4914 * 5 * max_3dlut_count);
		pkeylutall = kmalloc(key_len, GFP_KERNEL);
		if (!pkeylutall)
			return 1;

		pkeylut = kmalloc(4914 * sizeof(int) * 3,
				  GFP_KERNEL);
		if (!pkeylut) {
			kfree(pkeylutall);
			return 1;
		}

		if (bfromkey == 1) {
#ifdef CONFIG_AMLOGIC_LCD
			ret =
			lcd_unifykey_get_no_header("lcd_3dlut",
						   (unsigned char *)pkeylutall,
						   &key_len);
			if (ret < 0) {
				kfree(pkeylutall);
				kfree(pkeylut);
				return 1;
			}
#endif
		} else if (bfromkey == 2) {
			fp = filp_open(LUT3DBIN_PATH, O_RDONLY, 0);
			if (IS_ERR(fp)) {
				kfree(pkeylutall);
				kfree(pkeylut);
				return 1;
			}
			fs = get_fs();
			set_fs(KERNEL_DS);
			pos = 0;
			key_len = vfs_read(fp, pkeylutall, key_len, &pos);

			if (key_len == 0) {
				kfree(pkeylutall);
				kfree(pkeylut);
				return 1;
			}
			filp_close(fp, NULL);
			set_fs(fs);
		}

		size = (4914 * 9) / 2;
		key_count = key_len / size;
		if (keyindex >= key_count || keyindex <= INT_MIN) {
			pr_info("warn: key index out of range");
			kfree(pkeylutall);
			kfree(pkeylut);
			return 1;
		}

		offset = keyindex * size;
		counter = 0;
		combine3 = 0;
		for (i = 0; i < size; i += 3) {
			combine3 = pkeylutall[offset + i];
			combine3 |= (pkeylutall[offset + i + 1] << 8);
			combine3 |= (pkeylutall[offset + i + 2] << 16);
			pkeylut[counter] = combine3 & 0xfff;
			++counter;
			pkeylut[counter] = (combine3 >> 12) & 0xfff;
			++counter;
		}

		if (counter != 4914 * 3)
			pr_info("warn: key lut read size error");

		for (d0 = 0; d0 < 17; d0++) {
			for (d1 = 0; d1 < 17; d1++) {
				for (d2 = 0; d2 < 17; d2++) {
					index0 = d0 * 289 + d1 * 17 + d2;
					index1 = d2 * 289 + d1 * 17 + d0;
					idn =
					lut3d_order ? index1 : index0;
					plut3d[index0 * 3 + 0] =
					pkeylut[idn * 3 + 0] & 0xfff;
					plut3d[index0 * 3 + 1] =
					pkeylut[idn * 3 + 1] & 0xfff;
					plut3d[index0 * 3 + 2] =
					pkeylut[idn * 3 + 2] & 0xfff;
				}
			}
		}

		kfree(pkeylutall);
		kfree(pkeylut);
	} else {
		if (p3dlut_in) {
			for (d0 = 0; d0 < 17; d0++) {
				for (d1 = 0; d1 < 17; d1++) {
					for (d2 = 0; d2 < 17; d2++) {
						index0 =
						d0 * 289 + d1 * 17 + d2;
						index1 =
						d2 * 289 + d1 * 17 + d0;
						idn =
						lut3d_order ? index1 : index0;
						plut3d[index0 * 3 + 0] =
						p3dlut_in[idn][0] & 0xfff;
						plut3d[index0 * 3 + 1] =
						p3dlut_in[idn][1] & 0xfff;
						plut3d[index0 * 3 + 2] =
						p3dlut_in[idn][2] & 0xfff;
					}
				}
			}
		}
	}

	ctltemp  = READ_VPP_REG(VPP_LUT3D_CTRL);
	WRITE_VPP_REG(VPP_LUT3D_CTRL, ctltemp & 0xFFFFFFFE);
	usleep_range(16000, 16001);

	WRITE_VPP_REG(VPP_LUT3D_CBUS2RAM_CTRL, 1);
	WRITE_VPP_REG(VPP_LUT3D_RAM_ADDR, 0 | (0 << 31));
	for (i = 0; i < 17 * 17 * 17; i++) {
		//{comp0, comp1, comp2}
		WRITE_VPP_REG(VPP_LUT3D_RAM_DATA,
			      ((plut3d[i * 3 + 1] & 0xfff) << 16) |
			      (plut3d[i * 3 + 2] & 0xfff));
		WRITE_VPP_REG(VPP_LUT3D_RAM_DATA,
			      (plut3d[i * 3 + 0] & 0xfff)); /*MSB*/
		if (lut3d_debug == 1 && (i < 17 * 17))
			pr_info("%d: %03x %03x %03x\n",
				i,
				plut3d[i * 3 + 0],
				plut3d[i * 3 + 1],
				plut3d[i * 3 + 2]);
	}

	if (blut3dcheck) {
		WRITE_VPP_REG(VPP_LUT3D_CBUS2RAM_CTRL, 1);
		WRITE_VPP_REG(VPP_LUT3D_RAM_ADDR, 0 | (1 << 31));
		for (i = 0; i < 17 * 17 * 17; i++) {
			dwtemp  = READ_VPP_REG(VPP_LUT3D_RAM_DATA);
			wrgb[2] = dwtemp & 0xfff;
			wrgb[1] = (dwtemp >> 16) & 0xfff;
			dwtemp  = READ_VPP_REG(VPP_LUT3D_RAM_DATA);
			wrgb[0] = dwtemp & 0xfff;
			if (i < 97) {
				P3dlut_regtab[i * 3 + 2] = wrgb[2];
				P3dlut_regtab[i * 3 + 1] = wrgb[1];
				P3dlut_regtab[i * 3 + 0] = wrgb[0];
			}
			if (wrgb[0] != plut3d[i * 3 + 0]) {
				pr_info("%s:Error: Lut3d check error at R[%d]\n",
					__func__, i);
				WRITE_VPP_REG(VPP_LUT3D_CBUS2RAM_CTRL, 0);
				return 1;
			}
			if (wrgb[1] != plut3d[i * 3 + 1]) {
				pr_info("%s:Error: Lut3d check error at G[%d]\n",
					__func__, i);
				WRITE_VPP_REG(VPP_LUT3D_CBUS2RAM_CTRL, 0);
				return 1;
			}
			if (wrgb[2] != plut3d[i * 3 + 2]) {
				pr_info("%s:\n", __func__);
				WRITE_VPP_REG(VPP_LUT3D_CBUS2RAM_CTRL, 0);
				return 1;
			}
		}
		pr_info("%s: Lut3d check ok!!\n", __func__);
	}

	WRITE_VPP_REG(VPP_LUT3D_CBUS2RAM_CTRL, 0);
	WRITE_VPP_REG(VPP_LUT3D_CTRL, ctltemp);

	mutex_unlock(&vpp_lut3d_lock);

	return 0;
}

void lut3d_update(unsigned int p3dlut_in[][3])
{
	int d0, d1, d2, index0;
	int i;
	int offset = 0;

	if (!plut3d)
		return;

	if (p3dlut_in) {
		if (is_meson_t7_cpu())
			offset = 2;

		for (d0 = 0; d0 < 17; d0++) {
			for (d1 = 0; d1 < 17; d1++) {
				for (d2 = 0; d2 < 17; d2++) {
					index0 = d0 * 289 + d1 * 17 + d2;
					plut3d[index0 * 3 + 0] =
					(p3dlut_in[index0][0] << offset) & 0xfff;
					plut3d[index0 * 3 + 1] =
					(p3dlut_in[index0][1] << offset) & 0xfff;
					plut3d[index0 * 3 + 2] =
					(p3dlut_in[index0][2] << offset) & 0xfff;
				}
			}
		}
	}

	VSYNC_WRITE_VPP_REG(VPP_LUT3D_CBUS2RAM_CTRL, 1);
	VSYNC_WRITE_VPP_REG(VPP_LUT3D_RAM_ADDR, 0 | (0 << 31));
	for (i = 0; i < 17 * 17 * 17; i++) {
		//{comp0, comp1, comp2}
		VSYNC_WRITE_VPP_REG(VPP_LUT3D_RAM_DATA,
			      ((plut3d[i * 3 + 1] & 0xfff) << 16) |
			      (plut3d[i * 3 + 2] & 0xfff));
		VSYNC_WRITE_VPP_REG(VPP_LUT3D_RAM_DATA,
			      (plut3d[i * 3 + 0] & 0xfff)); /*MSB*/
		if (lut3d_debug == 1 && (i < 17 * 17))
			pr_info("%d: %03x %03x %03x\n",
				i,
				plut3d[i * 3 + 0],
				plut3d[i * 3 + 1],
				plut3d[i * 3 + 2]);
	}

	VSYNC_WRITE_VPP_REG(VPP_LUT3D_CBUS2RAM_CTRL, 0);
}

int vpp_write_lut3d_section(int index, int section_len,
			    unsigned int *p3dlut_section_in)
{
	int i;
	int r_offset, g_offset;
	u32 ctltemp;

	if (!lut3d_en)
		return 1;

	index = index * section_len / 17;

	g_offset = index % 17;
	r_offset = index / 17;

	ctltemp  = READ_VPP_REG(VPP_LUT3D_CTRL);
	WRITE_VPP_REG(VPP_LUT3D_CTRL, ctltemp & 0xFFFFFFFE);
	usleep_range(16000, 16001);

	WRITE_VPP_REG(VPP_LUT3D_CBUS2RAM_CTRL, 1);
	/* LUT3D_RAM_ADDR  bit20:16 R, bit12:8 G, bit4:0 B */
	WRITE_VPP_REG(VPP_LUT3D_RAM_ADDR,
		      (r_offset << 16) | (g_offset << 8) | (0 << 31));
	for (i = 0; i < section_len; i++) {
		//{comp0, comp1, comp2}
		WRITE_VPP_REG(VPP_LUT3D_RAM_DATA,
			      ((p3dlut_section_in[i * 3 + 1] & 0xfff) << 16) |
			      (p3dlut_section_in[i * 3 + 2] & 0xfff));
		WRITE_VPP_REG(VPP_LUT3D_RAM_DATA,
			      (p3dlut_section_in[i * 3 + 0] & 0xfff)); /*MSB*/
	}

	WRITE_VPP_REG(VPP_LUT3D_CBUS2RAM_CTRL, 0);
	WRITE_VPP_REG(VPP_LUT3D_CTRL, ctltemp);
	return 0;
}

int vpp_read_lut3d_section(int index, int section_len,
			   unsigned int *p3dlut_section_out)
{
	int i;
	int r_offset, g_offset;
	u32 dwtemp;

	if (!lut3d_en)
		return 1;

	index = index * section_len / 17;

	g_offset = index % 17;
	r_offset = index / 17;

	WRITE_VPP_REG(VPP_LUT3D_CBUS2RAM_CTRL, 1);
	/* LUT3D_RAM_ADDR  bit20:16 R, bit12:8 G, bit4:0 B */
	WRITE_VPP_REG(VPP_LUT3D_RAM_ADDR,
		      (r_offset << 16) | (g_offset << 8) | (1 << 31));
	for (i = 0; i < section_len; i++) {
		dwtemp  = READ_VPP_REG(VPP_LUT3D_RAM_DATA);
		p3dlut_section_out[i * 3 + 2] = dwtemp & 0xfff;
		p3dlut_section_out[i * 3 + 1] = (dwtemp >> 16) & 0xfff;
		dwtemp  = READ_VPP_REG(VPP_LUT3D_RAM_DATA);
		p3dlut_section_out[i * 3 + 0] = dwtemp & 0xfff;
	}

	WRITE_VPP_REG(VPP_LUT3D_CBUS2RAM_CTRL, 0);
	return 0;
}

int vpp_enable_lut3d(int enable)
{
	u32 temp;

	WRITE_VPP_REG(VPP_LUT3D_CBUS2RAM_CTRL, 0);

	temp  = READ_VPP_REG(VPP_LUT3D_CTRL);
	temp = (temp & 0xFFFFFF8E) | (enable & 0x1) | (0x7 << 4);
	/*reg_lut3d_extend_en[6:4]*/
	/*reg_lut3d_en*/
	WRITE_VPP_REG(VPP_LUT3D_CTRL, temp);
	return 0;
}

/*table: use for yuv->rgb*/
void vpp_lut3d_table_init(int r, int g, int b)
{
	int d0, d1, d2, step, max_val = 4095;
	unsigned int i, index;

	if (!plut3d)
		plut3d = kzalloc(14739 * sizeof(int), GFP_ATOMIC);

	if (!plut3d)
		return;

	if (is_meson_tl1_cpu() ||
	    is_meson_t5d_cpu() ||
	    is_meson_t5_cpu() ||
	    is_meson_t3_cpu() ||
	    is_meson_t5w_cpu())
		max_val = 1023;

	/*T7 is 12bit, max_val = 4095*/
	step = (max_val + 1) / 16;

	/*initialize the lut3d ad same input and output;*/
	for (d0 = 0; d0 < 17; d0++) {
		for (d1 = 0; d1 < 17; d1++) {
			for (d2 = 0; d2 < 17; d2++) {
				index =
				d0 * 17 * 17 * 3 + d1 * 17 * 3 + d2 * 3;
				if (r < 0 || g < 0 || b < 0) {
					/* bypass data */
					plut3d[index + 0] =
						min(max_val, d0 * step);
					plut3d[index + 1] =
						min(max_val, d1 * step);
					plut3d[index + 2] =
						min(max_val, d2 * step);
				} else {
					plut3d[index + 0] =
						min(max_val, r);
					plut3d[index + 1] =
						min(max_val, g);
					plut3d[index + 2] =
						min(max_val, b);
				}
			}
		}
	}
	for (i = 0; i < 289; i++)
		P3dlut_tab[i] = plut3d[i];
}

void vpp_lut3d_table_release(void)
{
	kfree(plut3d);
	plut3d = NULL;
}

void dump_plut3d_table(void)
{
	unsigned int i, j;

	pr_info("*****dump lut3d table:plut3d[0]~plut3d[288]*****\n");
	for (i = 0; i < 17; i++) {
		pr_info("*****dump plut3d:17* %d*****\n", i);
		for (j = 0; j < 17; j++) {
			if ((j % 16) == 0)
				pr_info("\n");
			pr_info("%d\t", P3dlut_tab[i * 17 + j]);
		}
	}
}

void dump_plut3d_reg_table(void)
{
	unsigned int i, j;

	pr_info("*****dump lut3d reg table:[0]~[288]*****\n");
	for (i = 0; i < 17; i++) {
		pr_info("*****dump plut3d regtab:17* %d*****\n", i);
		for (j = 0; j < 17; j++) {
			if ((j % 16) == 0)
				pr_info("\n");
			pr_info("%d\t", P3dlut_regtab[i * 17 + j]);
		}
	}
}

void set_gamma_regs(int en, int sel)
{
	int i;
	int *gamma_lut = NULL;

	static int gamma_lut_default[66] =  {
	0, 0, 0, 1, 2, 4, 6, 8, 11, 14, 17, 21, 26, 31,
	36, 42, 49, 55, 63, 71, 79, 88, 98, 108, 118,
	129, 141, 153, 166, 179, 193, 208, 223, 238,
	255, 271, 289, 307, 325, 344, 364, 384, 405,
	427, 449, 472, 495, 519, 544, 569, 595, 621,
	649, 676, 705, 734, 763, 794, 825, 856, 888,
	921, 955, 989, 1023, 0};

	static int gamma_lut_straight[66] =  {
	 0, 16, 32, 48, 64, 80, 96, 112, 128, 144, 160,
	 176, 192, 208, 224, 240, 256, 272, 288, 304, 320,
	 336, 352, 368, 384, 400, 416, 432, 448, 464, 480,
	 496, 512, 528, 544, 560, 576, 592, 608, 624, 640,
	 656, 672, 688, 704, 720, 736, 752, 768, 784, 800,
	 816, 832, 848, 864, 880, 896, 912, 928, 944, 960,
	 976, 992, 1008, 1023, 0};

	if (!sel)
		gamma_lut = gamma_lut_default;
	else
		gamma_lut = gamma_lut_straight;

	if (en) {
		if (chip_type_id == chip_s5) {
			post_pre_gamma_set(gamma_lut);
		} else {
			WRITE_VPP_REG(VPP_GAMMA_BIN_ADDR, 0);
			for (i = 0; i < 33; i = i + 1)
				WRITE_VPP_REG(VPP_GAMMA_BIN_DATA,
					      (((gamma_lut[i * 2 + 1] << 2) & 0xffff) << 16 |
					      ((gamma_lut[i * 2] << 2) & 0xffff)));
			for (i = 0; i < 33; i = i + 1)
				WRITE_VPP_REG(VPP_GAMMA_BIN_DATA,
					      (((gamma_lut[i * 2 + 1] << 2) & 0xffff) << 16 |
					      ((gamma_lut[i * 2] << 2) & 0xffff)));
			for (i = 0; i < 33; i = i + 1)
				WRITE_VPP_REG(VPP_GAMMA_BIN_DATA,
					      (((gamma_lut[i * 2 + 1] << 2) & 0xffff) << 16 |
					      ((gamma_lut[i * 2] << 2) & 0xffff)));
			WRITE_VPP_REG_BITS(VPP_GAMMA_CTRL, 0x3, 0, 2);
		}
	}
}

void set_pre_gamma_reg(struct pre_gamma_table_s *pre_gma_tb)
{
	int i;
	int en;

	if (!pre_gma_tb)
		return;

	en = ((pre_gma_tb->en & 0x1) << 1) | (pre_gma_tb->en & 0x1);

	VSYNC_WRITE_VPP_REG(VPP_GAMMA_BIN_ADDR, 0);
	for (i = 0; i < 32; i++)
		VSYNC_WRITE_VPP_REG(VPP_GAMMA_BIN_DATA,
				    (((pre_gma_tb->lut_r[i * 2 + 1] << 2) & 0xffff) << 16 |
				    ((pre_gma_tb->lut_r[i * 2] << 2) & 0xffff)));
	VSYNC_WRITE_VPP_REG(VPP_GAMMA_BIN_DATA, (pre_gma_tb->lut_r[64] << 2) & 0xffff);
	for (i = 0; i < 32; i++)
		VSYNC_WRITE_VPP_REG(VPP_GAMMA_BIN_DATA,
				    (((pre_gma_tb->lut_g[i * 2 + 1] << 2) & 0xffff) << 16 |
				    ((pre_gma_tb->lut_g[i * 2] << 2) & 0xffff)));
	VSYNC_WRITE_VPP_REG(VPP_GAMMA_BIN_DATA, (pre_gma_tb->lut_g[64] << 2) & 0xffff);
	for (i = 0; i < 32; i++)
		VSYNC_WRITE_VPP_REG(VPP_GAMMA_BIN_DATA,
				    (((pre_gma_tb->lut_b[i * 2 + 1] << 2) & 0xffff) << 16 |
				    ((pre_gma_tb->lut_b[i * 2] << 2) & 0xffff)));
	VSYNC_WRITE_VPP_REG(VPP_GAMMA_BIN_DATA, (pre_gma_tb->lut_b[64] << 2) & 0xffff);

	VSYNC_WRITE_VPP_REG_BITS(VPP_GAMMA_CTRL, en, 0, 2);
}

void vpp_pst_hist_sta_config(int en,
	enum pst_hist_mod mod,
	enum pst_hist_pos pos,
	struct vinfo_s *vinfo)
{
	int cfg = 0;
	unsigned int height = 0;
	unsigned int width = 0;

	/*config*/
	cfg = (0 << 16) | /*rd index*/
		(0 << 8) | /*00: clk_en, 01: close clk, 1x: original clk*/
		(1 << 4) | /*pst_hist_window_en: 1: win_en*/
		(mod << 2) | /*pst hist model sel  2: y/r 1:U/G 0:V/B 3: max(y/r 1:U/G 0:V/B )*/
		(pos << 1) | /*pst hist after csc 1: rgb 0, yuv*/
		(en << 0);/*pst hist sta en  1: en*/
	WRITE_VPP_REG(VPP_PST_STA_CTRL, cfg);
	if (vinfo) {
		height = vinfo->height - 1;
		width = vinfo->width - 1;
	}
	WRITE_VPP_REG(VPP_PST_STA_WIN_X, width << 16);
	WRITE_VPP_REG(VPP_PST_STA_WIN_Y, height << 16);
}

void vpp_pst_hist_sta_read(unsigned int *hist)
{
	int i;

	WRITE_VPP_REG_BITS(VPP_PST_STA_CTRL, 0, 16, 16);
	for (i = 0; i < 64; i++)
		hist[i] = READ_VPP_REG(VPP_PST_STA_RO_HIST);
}

void amvecm_wb_enable(int enable)
{
	if (enable) {
		wb_en = 1;

		if (chip_type_id == chip_a4) {
			WRITE_VPP_REG_BITS(VOUT_GAINOFF_CTRL0, 1, 31, 1);
			return;
		}
		if (video_rgb_ogo_xvy_mtx)
			WRITE_VPP_REG_BITS(VPP_MATRIX_CTRL, 1, 6, 1);
		else
			WRITE_VPP_REG_BITS(VPP_GAINOFF_CTRL0, 1, 31, 1);
	} else {
		wb_en = 0;
		if (chip_type_id == chip_a4) {
			WRITE_VPP_REG_BITS(VOUT_GAINOFF_CTRL0, 0, 31, 1);
			return;
		}
		if (video_rgb_ogo_xvy_mtx)
			WRITE_VPP_REG_BITS(VPP_MATRIX_CTRL, 0, 6, 1);
		else
			WRITE_VPP_REG_BITS(VPP_GAINOFF_CTRL0, 0, 31, 1);
	}
}

void amvecm_wb_enable_sub(int enable)
{
	if (enable)
		WRITE_VPP_REG_BITS(0x59a1, 1, 31, 1);
	else
		WRITE_VPP_REG_BITS(0x59a1, 0, 31, 1);
}

/*frequence meter init*/
void amve_fmeter_init(int enable)
{
	if (!enable)
		return;
	/*sr0 fmeter*/
	WRITE_VPP_REG_BITS(SRSHARP0_FMETER_CTRL, enable, 0, 1);
	WRITE_VPP_REG_BITS(SRSHARP0_FMETER_CTRL, 0xe, 4, 4);
	WRITE_VPP_REG_BITS(SRSHARP0_FMETER_CTRL, 0xa, 8, 4);
	WRITE_VPP_REG(SRSHARP0_FMETER_CORING, 0x04040404);
	WRITE_VPP_REG(SRSHARP0_FMETER_RATIO_H, 0x00040404);
	WRITE_VPP_REG(SRSHARP0_FMETER_RATIO_V, 0x00040404);
	WRITE_VPP_REG(SRSHARP0_FMETER_RATIO_D, 0x00040404);
	WRITE_VPP_REG(SRSHARP0_FMETER_H_FILTER0_9TAP_0, 0xe11ddc24);
	WRITE_VPP_REG_BITS(SRSHARP0_FMETER_H_FILTER0_9TAP_1, 0x14, 0, 10);
	WRITE_VPP_REG(SRSHARP0_FMETER_H_FILTER1_9TAP_0, 0x20f0ed26);
	WRITE_VPP_REG_BITS(SRSHARP0_FMETER_H_FILTER1_9TAP_1, 0xf0, 0, 10);
	WRITE_VPP_REG(SRSHARP0_FMETER_H_FILTER2_9TAP_0, 0xe2e81932);
	WRITE_VPP_REG_BITS(SRSHARP0_FMETER_H_FILTER2_9TAP_1, 0x04, 0, 10);

	/*sr1 fmeter*/
	WRITE_VPP_REG_BITS(SRSHARP1_FMETER_CTRL, enable, 0, 1);
	WRITE_VPP_REG_BITS(SRSHARP1_FMETER_CTRL, 0xe, 4, 4);
	WRITE_VPP_REG_BITS(SRSHARP1_FMETER_CTRL, 0xa, 8, 4);
	WRITE_VPP_REG(SRSHARP1_FMETER_CORING, 0x04040404);
	WRITE_VPP_REG(SRSHARP1_FMETER_RATIO_H, 0x00040404);
	WRITE_VPP_REG(SRSHARP1_FMETER_RATIO_V, 0x00040404);
	WRITE_VPP_REG(SRSHARP1_FMETER_RATIO_D, 0x00040404);
	WRITE_VPP_REG(SRSHARP1_FMETER_H_FILTER0_9TAP_0, 0xe11ddc24);
	WRITE_VPP_REG_BITS(SRSHARP1_FMETER_H_FILTER0_9TAP_1, 0x14, 0, 10);
	WRITE_VPP_REG(SRSHARP1_FMETER_H_FILTER1_9TAP_0, 0x20f0ed26);
	WRITE_VPP_REG_BITS(SRSHARP1_FMETER_H_FILTER1_9TAP_1, 0xf0, 0, 10);
	WRITE_VPP_REG(SRSHARP1_FMETER_H_FILTER2_9TAP_0, 0xe2e81932);
	WRITE_VPP_REG_BITS(SRSHARP1_FMETER_H_FILTER2_9TAP_1, 0x04, 0, 10);

	/*sr default value*/
	WRITE_VPP_REG(SRSHARP1_SR7_PKLONG_PF_GAIN, 0x40404040);
	WRITE_VPP_REG_BITS(SRSHARP1_PK_OS_STATIC, 0x14, 0, 10);
	WRITE_VPP_REG_BITS(SRSHARP1_PK_OS_STATIC, 0x14, 12, 10);
}

/*frequence meter size config*/
void amve_fmetersize_config(u32 sr0_w, u32 sr0_h, u32 sr1_w, u32 sr1_h)
{
	static u32 pre_fm0_x_st, pre_fm0_y_end, pre_fm1_x_end, pre_fm1_y_end;
	u32 fm0_x_st, fm0_x_end, fm0_y_st, fm0_y_end;
	u32 fm1_x_st, fm1_x_end, fm1_y_st, fm1_y_end;

	fm0_x_st = 0;
	fm0_x_end = sr0_w & 0x1fff;
	fm0_y_st = 0;
	fm0_y_end = sr0_h & 0x1fff;

	fm1_x_st = 0;
	fm1_x_end = sr1_w & 0x1fff;
	fm1_y_st = 0;
	fm1_y_end = sr1_h & 0x1fff;

	if (fm0_x_end == pre_fm0_x_st &&
		fm0_y_end == pre_fm0_y_end &&
		fm1_x_end == pre_fm1_x_end &&
		fm1_y_end == pre_fm1_y_end)
		return;

	if (fmeter_en) {
		VSYNC_WRITE_VPP_REG(SRSHARP0_FMETER_WIN_HOR,
			fm0_x_st | fm0_x_end << 16);
		VSYNC_WRITE_VPP_REG(SRSHARP0_FMETER_WIN_VER,
			fm0_y_st | fm0_y_end << 16);

		VSYNC_WRITE_VPP_REG(SRSHARP1_FMETER_WIN_HOR,
			fm1_x_st | fm1_x_end << 16);
		VSYNC_WRITE_VPP_REG(SRSHARP1_FMETER_WIN_VER,
			fm1_y_st | fm1_y_end << 16);

		pre_fm0_x_st = fm0_x_end;
		pre_fm0_y_end = fm0_y_end;
		pre_fm1_x_end = fm1_x_end;
		pre_fm1_y_end = fm1_y_end;
	}
}

int vpp_pq_ctrl_config(struct pq_ctrl_s pq_cfg, enum wr_md_e md)
{
	switch (md) {
	case WR_VCB:
		if (chip_type_id == chip_a4) {
			WRITE_VPP_REG_BITS(VOUT_VADJ_Y,
					pq_cfg.vadj2_en, 0, 1);
			amvecm_wb_enable(pq_cfg.wb_en);
			gamma_en = pq_cfg.gamma_en;
			if (gamma_en)
				vecm_latch_flag |= FLAG_GAMMA_TABLE_EN;
			else
				vecm_latch_flag |= FLAG_GAMMA_TABLE_DIS;
		} else if (chip_type_id == chip_s5) {
			WRITE_VPP_REG_BITS(SRSHARP0_PK_NR_ENABLE,
						 pq_cfg.sharpness0_en, 1, 1);

			WRITE_VPP_REG_BITS(SRSHARP1_PK_NR_ENABLE,
						 pq_cfg.sharpness1_en, 1, 1);

			if (pq_cfg.dnlp_en) {
				ve_enable_dnlp();
				dnlp_en = 1;
			} else {
				ve_disable_dnlp();
				dnlp_en = 0;
			}

			if (pq_cfg.cm_en) {
				amcm_enable(WR_VCB);
				cm_en = 1;
			} else {
				amcm_disable(WR_VCB);
				cm_en = 0;
			}

			ve_vadj_ctl(md, VE_VADJ1, pq_cfg.vadj1_en);
			ve_vadj_ctl(md, VE_VADJ2, pq_cfg.vadj2_en);
			ve_bs_ctl(md, 0);
			ve_ble_ctl(md, pq_cfg.black_ext_en);
			ve_cc_ctl(md, pq_cfg.chroma_cor_en);
			post_wb_ctl(md, pq_cfg.wb_en);
		} else {
			WRITE_VPP_REG_BITS(SRSHARP0_PK_NR_ENABLE,
						 pq_cfg.sharpness0_en, 1, 1);

			WRITE_VPP_REG_BITS(SRSHARP1_PK_NR_ENABLE,
						 pq_cfg.sharpness1_en, 1, 1);

			if (pq_cfg.dnlp_en) {
				ve_enable_dnlp();
				dnlp_en = 1;
			} else {
				ve_disable_dnlp();
				dnlp_en = 0;
			}

			if (pq_cfg.cm_en) {
				amcm_enable(WR_VCB);
				cm_en = 1;
			} else {
				amcm_disable(WR_VCB);
				cm_en = 0;
			}
			if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A)
				WRITE_VPP_REG_BITS(VPP_VADJ1_MISC,
					pq_cfg.vadj1_en, 0, 1);
			else
				WRITE_VPP_REG_BITS(VPP_VADJ_CTRL,
					pq_cfg.vadj1_en, 0, 1);

			WRITE_VPP_REG_BITS(VPP_VD1_RGB_CTRST,
				pq_cfg.vd1_ctrst_en, 1, 1);

			if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A)
				WRITE_VPP_REG_BITS(VPP_VADJ2_MISC,
					pq_cfg.vadj2_en, 0, 1);
			else
				WRITE_VPP_REG_BITS(VPP_VADJ_CTRL,
					pq_cfg.vadj2_en, 2, 1);

			WRITE_VPP_REG_BITS(VPP_POST_RGB_CTRST,
				pq_cfg.post_ctrst_en, 1, 1);

			amvecm_wb_enable(pq_cfg.wb_en);

			gamma_en = pq_cfg.gamma_en;
			if (gamma_en)
				vecm_latch_flag |= FLAG_GAMMA_TABLE_EN;
			else
				vecm_latch_flag |= FLAG_GAMMA_TABLE_DIS;

			if (pq_cfg.lc_en) {
				lc_en = 1;
			} else {
				lc_en = 0;
				if (is_meson_tl1_cpu() ||
					is_meson_tm2_cpu())
					lc_disable();
			}

			WRITE_VPP_REG_BITS(VPP_VE_ENABLE_CTRL,
				pq_cfg.black_ext_en, 3, 1);

			WRITE_VPP_REG_BITS(VPP_VE_ENABLE_CTRL,
				pq_cfg.chroma_cor_en, 4, 1);
		}
		break;
	case WR_DMA:
		if (chip_type_id == chip_a4) {
			VSYNC_WRITE_VPP_REG_BITS(VOUT_VADJ_Y,
					pq_cfg.vadj2_en, 0, 1);
			amvecm_wb_enable(pq_cfg.wb_en);
			gamma_en = pq_cfg.gamma_en;
			if (gamma_en)
				vpp_enable_lcd_gamma_table(0, 1);
			else
				vpp_disable_lcd_gamma_table(0, 1);
		} else if (chip_type_id == chip_s5) {
			VSYNC_WRITE_VPP_REG_BITS(SRSHARP0_PK_NR_ENABLE,
				pq_cfg.sharpness0_en, 1, 1);

			VSYNC_WRITE_VPP_REG_BITS(SRSHARP1_PK_NR_ENABLE,
				pq_cfg.sharpness1_en, 1, 1);

			if (pq_cfg.dnlp_en) {
				ve_enable_dnlp();
				dnlp_en = 1;
			} else {
				ve_disable_dnlp();
				dnlp_en = 0;
			}

			if (pq_cfg.cm_en) {
				amcm_enable(WR_DMA);
				cm_en = 1;
			} else {
				amcm_disable(WR_DMA);
				cm_en = 0;
			}

			ve_vadj_ctl(md, VE_VADJ1, pq_cfg.vadj1_en);
			ve_vadj_ctl(md, VE_VADJ2, pq_cfg.vadj2_en);
			ve_bs_ctl(md, 0);
			ve_ble_ctl(md, pq_cfg.black_ext_en);
			ve_cc_ctl(md, pq_cfg.chroma_cor_en);
			post_wb_ctl(md, pq_cfg.wb_en);
		} else {
			VSYNC_WRITE_VPP_REG_BITS(SRSHARP0_PK_NR_ENABLE,
				pq_cfg.sharpness0_en, 1, 1);

			VSYNC_WRITE_VPP_REG_BITS(SRSHARP1_PK_NR_ENABLE,
				pq_cfg.sharpness1_en, 1, 1);

			if (pq_cfg.dnlp_en) {
				ve_enable_dnlp();
				dnlp_en = 1;
			} else {
				ve_disable_dnlp();
				dnlp_en = 0;
			}

			if (pq_cfg.cm_en) {
				amcm_enable(WR_DMA);
				cm_en = 1;
			} else {
				amcm_disable(WR_DMA);
				cm_en = 0;
			}

			if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A)
				VSYNC_WRITE_VPP_REG_BITS(VPP_VADJ1_MISC,
					pq_cfg.vadj1_en, 0, 1);
			else
				VSYNC_WRITE_VPP_REG_BITS(VPP_VADJ_CTRL,
					pq_cfg.vadj1_en, 0, 1);

			VSYNC_WRITE_VPP_REG_BITS(VPP_VD1_RGB_CTRST,
				pq_cfg.vd1_ctrst_en, 1, 1);

			if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A)
				VSYNC_WRITE_VPP_REG_BITS(VPP_VADJ2_MISC,
					pq_cfg.vadj2_en, 0, 1);
			else
				VSYNC_WRITE_VPP_REG_BITS(VPP_VADJ_CTRL,
					pq_cfg.vadj2_en, 2, 1);

			VSYNC_WRITE_VPP_REG_BITS(VPP_POST_RGB_CTRST,
				pq_cfg.post_ctrst_en, 1, 1);

			amvecm_wb_enable(pq_cfg.wb_en);

			gamma_en = pq_cfg.gamma_en;
			if (gamma_en) {
				if (is_meson_t7_cpu())
					vecm_latch_flag |= FLAG_GAMMA_TABLE_EN;
				else
					vpp_enable_lcd_gamma_table(0, 1);
			} else {
				if (is_meson_t7_cpu())
					vecm_latch_flag |= FLAG_GAMMA_TABLE_DIS;
				else
					vpp_disable_lcd_gamma_table(0, 1);
			}

			if (pq_cfg.lc_en) {
				lc_en = 1;
			} else {
				lc_en = 0;
				if (is_meson_tl1_cpu() ||
				    is_meson_tm2_cpu())
					lc_disable();
			}

			VSYNC_WRITE_VPP_REG_BITS(VPP_VE_ENABLE_CTRL,
				pq_cfg.black_ext_en, 3, 1);

			VSYNC_WRITE_VPP_REG_BITS(VPP_VE_ENABLE_CTRL,
				pq_cfg.chroma_cor_en, 4, 1);
		}
		break;
	default:
		break;
	}

	return 0;
}

unsigned int skip_pq_ctrl_load(struct am_reg_s *p)
{
	unsigned int ret = 0;
	struct pq_ctrl_s cfg;

	if (dv_pq_bypass == 3) {
		memcpy(&cfg, &dv_cfg_bypass, sizeof(struct pq_ctrl_s));
		cfg.vadj1_en = pq_cfg.vadj1_en;
	} else if (dv_pq_bypass == 2) {
		memcpy(&cfg, &dv_cfg_bypass, sizeof(struct pq_ctrl_s));
		cfg.sharpness0_en = pq_cfg.sharpness0_en;
		cfg.sharpness1_en = pq_cfg.sharpness1_en;
	} else if (dv_pq_bypass == 1) {
		memcpy(&cfg, &dv_cfg_bypass, sizeof(struct pq_ctrl_s));
	} else {
		memcpy(&cfg, &pq_cfg, sizeof(struct pq_ctrl_s));
	}

	if (!cfg.sharpness0_en) {
		if (p->addr == (SRSHARP0_PK_NR_ENABLE)) {
			ret |= 1 << 1;
			return ret;
		}
	}

	if (!cfg.sharpness1_en) {
		if (p->addr == (SRSHARP1_PK_NR_ENABLE)) {
			ret |= 1 << 1;
			return ret;
		}
	}

	if (!cfg.cm_en) {
		if (p->addr == 0x208) {
			if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A)
				ret |= 1 << 0;
			else
				ret |= 1 << 1;
			return ret;
		}
	}

	if (!cfg.vadj1_en) {
		if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A) {
			if (p->addr == VPP_VADJ1_MISC) {
				ret |= 1 << 0;
				return ret;
			}
		} else {
			if (p->addr == VPP_VADJ_CTRL) {
				ret |= 1 << 0;
				return ret;
			}
		}
	}

	if (!cfg.vd1_ctrst_en) {
		if (p->addr == VPP_VD1_RGB_CTRST) {
			ret |= 1 << 1;
			return ret;
		}
	}

	if (!cfg.vadj2_en) {
		if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A) {
			if (p->addr == VPP_VADJ2_MISC) {
				ret |= 1 << 0;
				return ret;
			}
		} else {
			if (p->addr == VPP_VADJ2_Y) {
				ret |= 1 << 0;
				return ret;
			}
		}
	}

	if (!cfg.post_ctrst_en) {
		if (p->addr == VPP_POST_RGB_CTRST) {
			ret |= 1 << 1;
			return ret;
		}
	}

	if (p->addr == VPP_VE_ENABLE_CTRL) {
		if (!cfg.black_ext_en)
			ret |= 1 << 3;
		if (!cfg.chroma_cor_en)
			ret |= 1 << 4;
		return ret;
	}

	return ret;
}

int dv_pq_ctl(enum dv_pq_ctl_e ctl)
{
	struct pq_ctrl_s cfg;

	switch (ctl) {
	case DV_PQ_TV_BYPASS:
		memcpy(&cfg, &dv_cfg_bypass, sizeof(struct pq_ctrl_s));
		cfg.vadj1_en = pq_cfg.vadj1_en;
		vpp_pq_ctrl_config(cfg, WR_DMA);
		dv_pq_bypass = 3;
		pr_amve_dbg("dv enable, for TV pq disable, dv_pq_bypass = %d\n",
			    dv_pq_bypass);
		break;
	case DV_PQ_STB_BYPASS:
		memcpy(&cfg, &dv_cfg_bypass, sizeof(struct pq_ctrl_s));
		cfg.sharpness0_en = pq_cfg.sharpness0_en;
		cfg.sharpness1_en = pq_cfg.sharpness1_en;
		vpp_pq_ctrl_config(cfg, WR_DMA);
		eye_proc(eye_protect.mtx_ep, 0);
		dv_pq_bypass = 2;
		pr_amve_dbg("dv enable, for STB pq disable, dv_pq_bypass = %d\n",
				dv_pq_bypass);
		break;
	case DV_PQ_CERT:
		vpp_pq_ctrl_config(dv_cfg_bypass, WR_DMA);
		eye_proc(eye_protect.mtx_ep, 0);
		dv_pq_bypass = 1;
		pr_amve_dbg("dv certification mode, pq disable, dv_pq_bypass = %d\n",
			    dv_pq_bypass);
		break;
	case DV_PQ_REC:
		vpp_pq_ctrl_config(pq_cfg, WR_DMA);
		vecm_latch_flag2 |= VPP_EYE_PROTECT_UPDATE;
		dv_pq_bypass = 0;
		pr_amve_dbg("dv disable, pq recovery, dv_pq_bypass = %d\n",
			    dv_pq_bypass);
		break;
	default:
		break;
	}
	return 0;
}
EXPORT_SYMBOL(dv_pq_ctl);

int mtx_mul_mtx(int (*mtx_a)[3], int (*mtx_b)[3], int (*mtx_out)[3])
{
	int i, j, k;

	for (i = 0; i < 3; i++) {
		for (j = 0; j < 3; j++) {
			mtx_out[i][j] = 0;
			for (k = 0; k < 3; k++)
				mtx_out[i][j] += mtx_a[i][k] * mtx_b[k][j];
		}
	}

	for (i = 0; i < 3; i++) {
		for (j = 0; j < 3; j++)
			mtx_out[i][j] = (mtx_out[i][j] + (1 << 9)) >> 10;
	}

	return 0;
}

int mtx_multi(int mtx_ep[][4], int (*mtx_out)[3])
{
	int i, j;
	int mtx_rgb[3][3] = {0};
	int mtx_in[3][3] = {0};
	int mtx_rgbto709l[3][3] = {
		{187, 629, 63},
		{-103, -346, 450},
		{450, -409, -41},
	};
	int mtx_709ltorgb[3][3] = {
		{1192, 0, 1836},
		{1192, -218, -547},
		{1192, 2166, 0},
	};

	for (i = 0; i < 4; i++) {
		for (j = 0; j < 4; j++)
			pr_amve_dbg("mtx_out[%d][%d] = %d\n",
			i, j, mtx_ep[i][j]);
	}

	/* because use only one matrix, so Tr/Tg/Tb can not be used,
	 * if add Tr/Tg/Tb in RGB out, need two matrix to workaround
	 */
	for (i = 0; i < 3; i++)
		for (j = 0; j < 3; j++)
			mtx_in[i][j] = mtx_ep[j][i];

	mtx_mul_mtx(mtx_in, mtx_709ltorgb, mtx_rgb);
	mtx_mul_mtx(mtx_rgbto709l, mtx_rgb, mtx_out);

	for (i = 0; i < 3; i++) {
		for (j = 0; j < 3; j++)
			pr_amve_dbg("mtx_out[%d][%d] = 0x%x\n",
			i, j, mtx_out[i][j]);
	}
	return 0;
}

void eye_proc(int mtx_ep[][4], int mtx_on)
{
	unsigned int matrix_coef00_01 = 0;
	unsigned int matrix_coef02_10 = 0;
	unsigned int matrix_coef11_12 = 0;
	unsigned int matrix_coef20_21 = 0;
	unsigned int matrix_coef22 = 0;
	unsigned int matrix_coef13_14 = 0;
	unsigned int matrix_coef23_24 = 0;
	unsigned int matrix_coef15_25 = 0;
	unsigned int matrix_clip = 0;
	unsigned int matrix_offset0_1 = 0;
	unsigned int matrix_offset2 = 0;
	unsigned int matrix_pre_offset0_1 = 0;
	unsigned int matrix_pre_offset2 = 0;
	unsigned int matrix_en_ctrl = 0;
	int mtx_out[3][3] = {0};
	int pre_offset[3] = {
		-64, -512, -512
	};

	int offset[3] = {
		64, 512, 512
	};

	if (vinfo_lcd_support())
		return;

	matrix_coef00_01 = VPP_POST_MATRIX_COEF00_01;
	matrix_coef02_10 = VPP_POST_MATRIX_COEF02_10;
	matrix_coef11_12 = VPP_POST_MATRIX_COEF11_12;
	matrix_coef20_21 = VPP_POST_MATRIX_COEF20_21;
	matrix_coef22 = VPP_POST_MATRIX_COEF22;
	matrix_coef13_14 = VPP_POST_MATRIX_COEF13_14;
	matrix_coef23_24 = VPP_POST_MATRIX_COEF23_24;
	matrix_coef15_25 = VPP_POST_MATRIX_COEF15_25;
	matrix_clip = VPP_POST_MATRIX_CLIP;
	matrix_offset0_1 = VPP_POST_MATRIX_OFFSET0_1;
	matrix_offset2 = VPP_POST_MATRIX_OFFSET2;
	matrix_pre_offset0_1 = VPP_POST_MATRIX_PRE_OFFSET0_1;
	matrix_pre_offset2 = VPP_POST_MATRIX_PRE_OFFSET2;
	matrix_en_ctrl = VPP_POST_MATRIX_EN_CTRL;

	VSYNC_WRITE_VPP_REG_BITS(VPP_POST_MATRIX_EN_CTRL, mtx_on, 0, 1);

	if (!mtx_on)
		return;

	mtx_multi(mtx_ep, mtx_out);
	VSYNC_WRITE_VPP_REG(matrix_coef00_01,
		((mtx_out[0][0] & 0x1fff) << 16) | (mtx_out[0][1] & 0x1fff));
	VSYNC_WRITE_VPP_REG(matrix_coef02_10,
		((mtx_out[0][2] & 0x1fff) << 16) | (mtx_out[1][0] & 0x1fff));
	VSYNC_WRITE_VPP_REG(matrix_coef11_12,
		((mtx_out[1][1] & 0x1fff) << 16) | (mtx_out[1][2] & 0x1fff));
	VSYNC_WRITE_VPP_REG(matrix_coef20_21,
		((mtx_out[2][0] & 0x1fff) << 16) | (mtx_out[2][1] & 0x1fff));
	VSYNC_WRITE_VPP_REG(matrix_coef22, (mtx_out[2][2] & 0x1fff));
	VSYNC_WRITE_VPP_REG(matrix_offset0_1,
		((offset[0] & 0x7ff) << 16) | (offset[1] & 0x7ff));
	VSYNC_WRITE_VPP_REG(matrix_offset2, (offset[2] & 0x7ff));
	VSYNC_WRITE_VPP_REG(matrix_pre_offset0_1,
		((pre_offset[0] & 0x7ff) << 16) | (pre_offset[1] & 0x7ff));
	VSYNC_WRITE_VPP_REG(matrix_pre_offset2, (pre_offset[2] & 0x7ff));
}

/*t3 for power consumption, disable clock
 *modules only before post blend can be disable
 *because after postblend it used for color temperature
 *or color correction
 */
static void vpp_enhence_clk_ctl(int en)
{
	VSYNC_WRITE_VPP_REG_BITS(VPP_GCLK_CTRL0, en, 6, 2); /*blue stretch*/
	VSYNC_WRITE_VPP_REG_BITS(VPP_GCLK_CTRL1,
		(en | (en << 2)), 0, 4); /*cm*/
	VSYNC_WRITE_VPP_REG_BITS(VPP_VE_ENABLE_CTRL,
		(en | (en << 2)), 24, 4); /*ble, chroma coring*/
}

void set_vpp_enh_clk(struct vframe_s *vf, struct vframe_s *rpt_vf)
{
	/*cm / blue stretch/ black extension/ chroma coring*/
	/*other modules(sr0/sr1/dnlp/lc disable in amvideo)*/
	/*en: 1 disable clock,  0: enable clock*/
	static enum pw_state_e pre_state = PW_MAX;
	enum pw_state_e pw_state = PW_MAX;

	if (get_cpu_type() < MESON_CPU_MAJOR_ID_T3)
		return;

	if (chip_type_id == chip_s5)
		return;

	if (vf || rpt_vf)
		pw_state = PW_ON;
	else
		pw_state = PW_OFF;

	switch (pw_state) {
	case PW_ON:
		if (pre_state != PW_ON) {
			vpp_enhence_clk_ctl(PW_ON);
			pre_state = pw_state;
			pr_amve_dbg("PW_ON: pre_state: %d\n", pre_state);
		}
		break;
	case PW_OFF:
		if (pre_state != PW_OFF) {
			vpp_enhence_clk_ctl(PW_ON);
			pre_state = pw_state;
			pr_amve_dbg("PW_OFF: pre_state: %d\n", pre_state);
		}
		break;
	//case PW_MAX:
	/*
	 * missing the default will caused build error
	 */
	/* coverity[dead_error_begin:SUPPRESS] */
	default:
		pre_state = PW_MAX;
		break;
	}
}

