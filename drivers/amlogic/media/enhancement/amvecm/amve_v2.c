// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/string.h>
#include <linux/spinlock.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/slab.h>
/* #include <mach/am_regs.h> */
#include <linux/amlogic/media/utils/amstream.h>
/* #include <linux/amlogic/aml_common.h> */
/* media module used media/registers/cpu_version.h since kernel 5.4 */
#include <linux/amlogic/media/registers/cpu_version.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/video_sink/video.h>
#include <linux/amlogic/media/amvecm/amvecm.h>
#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/vout/lcd/lcd_unifykey.h>
#include <linux/amlogic/media/utils/amstream.h>
#include "arch/vpp_regs_v2.h"
#include "amve_v2.h"
#include <linux/io.h>
#include "reg_helper.h"

static int vev2_dbg;
module_param(vev2_dbg, int, 0644);
MODULE_PARM_DESC(vev2_dbg, "ve dbg after s5");

#define pr_amve_v2(fmt, args...)\
	do {\
		if (vev2_dbg & 0x1)\
			pr_info("AMVE: " fmt, ## args);\
	} while (0)\

/*ve module slice1~slice3 offset*/
unsigned int ve_reg_ofst[3] = {
	0x0, 0x100, 0x200
};

unsigned int pst_reg_ofst[4] = {
	0x0, 0x100, 0x700, 0x1900
};

static void ve_brightness_cfg(int val,
	enum wr_md_e mode, enum vadj_index_e vadj_idx,
	enum vpp_slice_e slice)
{
	int reg;

	if (slice >= SLICE_MAX)
		return;

	switch (vadj_idx) {
	case VE_VADJ1:
		if (slice == SLICE0)
			reg = VPP_VADJ1_Y;
		else
			reg = VPP_SLICE1_VADJ1_Y + ve_reg_ofst[slice - 1];
		break;
	case VE_VADJ2:
		reg = VPP_VADJ2_Y + pst_reg_ofst[slice];
		break;
	default:
		break;
	}

	if (mode == WR_VCB)
		WRITE_VPP_REG_BITS(reg, val, 8, 11);
	else if (mode == WR_DMA)
		VSYNC_WRITE_VPP_REG_BITS(reg, val, 8, 11);

	pr_amve_v2("brigtness: val = %d, slice = %d", val, slice);
}

static void ve_contrast_cfg(int val,
	enum wr_md_e mode, enum vadj_index_e vadj_idx,
	enum vpp_slice_e slice)
{
	int reg;

	if (slice >= SLICE_MAX)
		return;

	switch (vadj_idx) {
	case VE_VADJ1:
		if (slice == SLICE0)
			reg = VPP_VADJ1_Y;
		else
			reg = VPP_SLICE1_VADJ1_Y + ve_reg_ofst[slice - 1];
		break;
	case VE_VADJ2:
		reg = VPP_VADJ2_Y + pst_reg_ofst[slice];
		break;
	default:
		break;
	}

	if (mode == WR_VCB)
		WRITE_VPP_REG_BITS(reg, val, 0, 8);
	else if (mode == WR_DMA)
		VSYNC_WRITE_VPP_REG_BITS(reg, val, 0, 8);

	pr_amve_v2("contrast: val = %d, slice = %d", val, slice);
}

static void ve_sat_hue_mab_cfg(int mab,
	enum wr_md_e mode, enum vadj_index_e vadj_idx,
	enum vpp_slice_e slice)
{
	int reg_mab;
	//int reg_mcd;

	if (slice >= SLICE_MAX)
		return;

	switch (vadj_idx) {
	case VE_VADJ1:
		if (slice == SLICE0) {
			reg_mab = VPP_VADJ1_MA_MB;
			//reg_mcd = VPP_VADJ1_MC_MD;
		} else {
			reg_mab = VPP_SLICE1_VADJ1_MA_MB + ve_reg_ofst[slice - 1];
			//reg_mcd = VPP_SLICE1_VADJ1_MC_MD + ve_reg_ofst[slice - 1];
		}
		break;
	case VE_VADJ2:
		reg_mab = VPP_VADJ2_MA_MB + pst_reg_ofst[slice];
		//reg_mcd = VPP_VADJ2_MC_MD + pst_reg_ofst[slice];
		break;
	default:
		break;
	}

	if (mode == WR_VCB)
		WRITE_VPP_REG(reg_mab, mab);
	else if (mode == WR_DMA)
		VSYNC_WRITE_VPP_REG(reg_mab, mab);

	pr_amve_v2("sat_hue: mab = %d, slice = %d", mab, slice);
}

static void ve_sat_hue_mcd_cfg(int mcd,
	enum wr_md_e mode, enum vadj_index_e vadj_idx,
	enum vpp_slice_e slice)
{
	//int reg_mab;
	int reg_mcd;

	if (slice >= SLICE_MAX)
		return;

	switch (vadj_idx) {
	case VE_VADJ1:
		if (slice == SLICE0) {
			//reg_mab = VPP_VADJ1_MA_MB;
			reg_mcd = VPP_VADJ1_MC_MD;
		} else {
			//reg_mab = VPP_SLICE1_VADJ1_MA_MB + ve_reg_ofst[slice - 1];
			reg_mcd = VPP_SLICE1_VADJ1_MC_MD + ve_reg_ofst[slice - 1];
		}
		break;
	case VE_VADJ2:
		//reg_mab = VPP_VADJ2_MA_MB + pst_reg_ofst[slice];
		reg_mcd = VPP_VADJ2_MC_MD + pst_reg_ofst[slice];
		break;
	default:
		break;
	}

	if (mode == WR_VCB)
		WRITE_VPP_REG(reg_mcd, mcd);
	else if (mode == WR_DMA)
		VSYNC_WRITE_VPP_REG(reg_mcd, mcd);

	pr_amve_v2("sat_hue: mcd = %d, slice = %d", mcd, slice);
}

void ve_brigtness_set(int val,
	enum vadj_index_e vadj_idx,
	enum wr_md_e mode)
{
	int i;

	for (i = SLICE0; i < SLICE_MAX; i++)
		ve_brightness_cfg(val, mode, vadj_idx, i);
}

void ve_contrast_set(int val,
	enum vadj_index_e vadj_idx,
	enum wr_md_e mode)
{
	int i;

	for (i = SLICE0; i < SLICE_MAX; i++)
		ve_contrast_cfg(val, mode, vadj_idx, i);
}

void ve_color_mab_set(int mab,
	enum vadj_index_e vadj_idx,
	enum wr_md_e mode)
{
	int i;

	for (i = SLICE0; i < SLICE_MAX; i++)
		ve_sat_hue_mab_cfg(mab, mode, vadj_idx, i);
}

void ve_color_mcd_set(int mcd,
	enum vadj_index_e vadj_idx,
	enum wr_md_e mode)
{
	int i;

	for (i = SLICE0; i < SLICE_MAX; i++)
		ve_sat_hue_mcd_cfg(mcd, mode, vadj_idx, i);
}

int ve_brightness_contrast_get(enum vadj_index_e vadj_idx)
{
	int val = 0;

	if (vadj_idx == VE_VADJ1)
		val = READ_VPP_REG(VPP_VADJ1_Y);
	else if (vadj_idx == VE_VADJ2)
		val = READ_VPP_REG(VPP_VADJ2_Y);
	return val;
}

void vpp_mtx_config_v2(struct matrix_coef_s *coef,
	enum wr_md_e mode, enum vpp_slice_e slice,
	enum vpp_matrix_e mtx_sel)
{
	int reg_pre_offset0_1 = 0;
	int reg_pre_offset2 = 0;
	int reg_coef00_01 = 0;
	int reg_coef02_10;
	int reg_coef11_12;
	int reg_coef20_21;
	int reg_coef22;
	int reg_offset0_1 = 0;
	int reg_offset2 = 0;
	int reg_en_ctl = 0;

	if (slice >= SLICE_MAX)
		return;

	switch (slice) {
	case SLICE0:
		if (mtx_sel == VD1_MTX) {
			reg_pre_offset0_1 = VPP_VD1_MATRIX_COEF00_01;
			reg_pre_offset2 = VPP_VD1_MATRIX_PRE_OFFSET2;
			reg_coef00_01 = VPP_VD1_MATRIX_COEF00_01;
			reg_coef02_10 = VPP_VD1_MATRIX_COEF02_10;
			reg_coef11_12 = VPP_VD1_MATRIX_COEF11_12;
			reg_coef20_21 = VPP_VD1_MATRIX_COEF20_21;
			reg_coef22 = VPP_VD1_MATRIX_COEF22;
			reg_offset0_1 = VPP_VD1_MATRIX_OFFSET0_1;
			reg_offset2 = VPP_VD1_MATRIX_OFFSET2;
			reg_en_ctl = VPP_VD1_MATRIX_EN_CTRL;
		} else if (mtx_sel == POST2_MTX) {
			reg_pre_offset0_1 = VPP_POST2_MATRIX_COEF00_01;
			reg_pre_offset2 = VPP_POST2_MATRIX_PRE_OFFSET2;
			reg_coef00_01 = VPP_POST2_MATRIX_COEF00_01;
			reg_coef02_10 = VPP_POST2_MATRIX_COEF02_10;
			reg_coef11_12 = VPP_POST2_MATRIX_COEF11_12;
			reg_coef20_21 = VPP_POST2_MATRIX_COEF20_21;
			reg_coef22 = VPP_POST2_MATRIX_COEF22;
			reg_offset0_1 = VPP_POST2_MATRIX_OFFSET0_1;
			reg_offset2 = VPP_POST2_MATRIX_OFFSET2;
			reg_en_ctl = VPP_POST2_MATRIX_EN_CTRL;
		} else if (mtx_sel == POST_MTX) {
			reg_pre_offset0_1 = VPP_POST_MATRIX_COEF00_01;
			reg_pre_offset2 = VPP_POST_MATRIX_PRE_OFFSET2;
			reg_coef00_01 = VPP_POST_MATRIX_COEF00_01;
			reg_coef02_10 = VPP_POST_MATRIX_COEF02_10;
			reg_coef11_12 = VPP_POST_MATRIX_COEF11_12;
			reg_coef20_21 = VPP_POST_MATRIX_COEF20_21;
			reg_coef22 = VPP_POST_MATRIX_COEF22;
			reg_offset0_1 = VPP_POST_MATRIX_OFFSET0_1;
			reg_offset2 = VPP_POST_MATRIX_OFFSET2;
			reg_en_ctl = VPP_POST_MATRIX_EN_CTRL;
		}
		break;
	case SLICE1:
	case SLICE2:
	case SLICE3:
		if (mtx_sel == VD1_MTX) {
			reg_pre_offset0_1 = VPP_SLICE1_VD1_MATRIX_PRE_OFFSET0_1 +
				ve_reg_ofst[slice - 1];
			reg_pre_offset2 = VPP_SLICE1_VD1_MATRIX_PRE_OFFSET2 +
				ve_reg_ofst[slice - 1];
			reg_coef00_01 = VPP_SLICE1_VD1_MATRIX_COEF00_01 +
				ve_reg_ofst[slice - 1];
			reg_coef02_10 = VPP_SLICE1_VD1_MATRIX_COEF02_10 +
				ve_reg_ofst[slice - 1];
			reg_coef11_12 = VPP_SLICE1_VD1_MATRIX_COEF11_12 +
				ve_reg_ofst[slice - 1];
			reg_coef20_21 = VPP_SLICE1_VD1_MATRIX_COEF20_21 +
				ve_reg_ofst[slice - 1];
			reg_coef22 = VPP_SLICE1_VD1_MATRIX_COEF22 +
				ve_reg_ofst[slice - 1];
			reg_offset0_1 = VPP_SLICE1_VD1_MATRIX_OFFSET0_1 +
				ve_reg_ofst[slice - 1];
			reg_offset2 = VPP_SLICE1_VD1_MATRIX_OFFSET2 +
				ve_reg_ofst[slice - 1];
			reg_en_ctl = VPP_SLICE1_VD1_MATRIX_EN_CTRL +
				ve_reg_ofst[slice - 1];
		} else if (mtx_sel == POST2_MTX) {
			reg_pre_offset0_1 = VPP_POST2_MATRIX_COEF00_01 +
				pst_reg_ofst[slice];
			reg_pre_offset2 = VPP_POST2_MATRIX_PRE_OFFSET2 +
				pst_reg_ofst[slice];
			reg_coef00_01 = VPP_POST2_MATRIX_COEF00_01 +
				pst_reg_ofst[slice];
			reg_coef02_10 = VPP_POST2_MATRIX_COEF02_10 +
				pst_reg_ofst[slice];
			reg_coef11_12 = VPP_POST2_MATRIX_COEF11_12 +
				pst_reg_ofst[slice];
			reg_coef20_21 = VPP_POST2_MATRIX_COEF20_21 +
				pst_reg_ofst[slice];
			reg_coef22 = VPP_POST2_MATRIX_COEF22 +
				pst_reg_ofst[slice];
			reg_offset0_1 = VPP_POST2_MATRIX_OFFSET0_1 +
				pst_reg_ofst[slice];
			reg_offset2 = VPP_POST2_MATRIX_OFFSET2 +
				pst_reg_ofst[slice];
			reg_en_ctl = VPP_POST2_MATRIX_EN_CTRL +
				pst_reg_ofst[slice];
		} else if (mtx_sel == POST_MTX) {
			reg_pre_offset0_1 = VPP_POST_MATRIX_COEF00_01 +
				pst_reg_ofst[slice];
			reg_pre_offset2 = VPP_POST_MATRIX_PRE_OFFSET2 +
				pst_reg_ofst[slice];
			reg_coef00_01 = VPP_POST_MATRIX_COEF00_01 +
				pst_reg_ofst[slice];
			reg_coef02_10 = VPP_POST_MATRIX_COEF02_10 +
				pst_reg_ofst[slice];
			reg_coef11_12 = VPP_POST_MATRIX_COEF11_12 +
				pst_reg_ofst[slice];
			reg_coef20_21 = VPP_POST_MATRIX_COEF20_21 +
				pst_reg_ofst[slice];
			reg_coef22 = VPP_POST_MATRIX_COEF22 +
				pst_reg_ofst[slice];
			reg_offset0_1 = VPP_POST_MATRIX_OFFSET0_1 +
				pst_reg_ofst[slice];
			reg_offset2 = VPP_POST_MATRIX_OFFSET2 +
				pst_reg_ofst[slice];
			reg_en_ctl = VPP_POST_MATRIX_EN_CTRL +
				pst_reg_ofst[slice];
		}
		break;
	default:
		break;
	}

	switch (mode) {
	case WR_VCB:
		WRITE_VPP_REG(reg_pre_offset0_1,
			(coef->pre_offset[0] << 16) | coef->pre_offset[1]);
		WRITE_VPP_REG(reg_pre_offset2, coef->pre_offset[2]);
		WRITE_VPP_REG(reg_coef00_01,
			(coef->matrix_coef[0][0] << 16) | coef->matrix_coef[0][1]);
		WRITE_VPP_REG(reg_coef00_01,
			(coef->matrix_coef[0][2] << 16) | coef->matrix_coef[1][0]);
		WRITE_VPP_REG(reg_coef00_01,
			(coef->matrix_coef[1][1] << 16) | coef->matrix_coef[1][2]);
		WRITE_VPP_REG(reg_coef00_01,
			(coef->matrix_coef[2][0] << 16) | coef->matrix_coef[2][1]);
		WRITE_VPP_REG(reg_coef00_01, coef->matrix_coef[2][2]);
		WRITE_VPP_REG(reg_offset0_1,
			(coef->post_offset[0] << 16) | coef->post_offset[1]);
		WRITE_VPP_REG(reg_offset2, coef->post_offset[2]);
		WRITE_VPP_REG_BITS(reg_en_ctl, coef->en, 0, 1);
		break;
	case WR_DMA:
		VSYNC_WRITE_VPP_REG(reg_pre_offset0_1,
			(coef->pre_offset[0] << 16) | coef->pre_offset[1]);
		VSYNC_WRITE_VPP_REG(reg_pre_offset2, coef->pre_offset[2]);
		VSYNC_WRITE_VPP_REG(reg_coef00_01,
			(coef->matrix_coef[0][0] << 16) | coef->matrix_coef[0][1]);
		VSYNC_WRITE_VPP_REG(reg_coef00_01,
			(coef->matrix_coef[0][2] << 16) | coef->matrix_coef[1][0]);
		VSYNC_WRITE_VPP_REG(reg_coef00_01,
			(coef->matrix_coef[1][1] << 16) | coef->matrix_coef[1][2]);
		VSYNC_WRITE_VPP_REG(reg_coef00_01,
			(coef->matrix_coef[2][0] << 16) | coef->matrix_coef[2][1]);
		VSYNC_WRITE_VPP_REG(reg_coef00_01, coef->matrix_coef[2][2]);
		VSYNC_WRITE_VPP_REG(reg_offset0_1,
			(coef->post_offset[0] << 16) | coef->post_offset[1]);
		VSYNC_WRITE_VPP_REG(reg_offset2, coef->post_offset[2]);
		VSYNC_WRITE_VPP_REG_BITS(reg_en_ctl, coef->en, 0, 1);
		break;
	default:
		break;
	}
}

void cm_top_ctl(enum wr_md_e mode, int en)
{
	switch (mode) {
	case WR_VCB:
		WRITE_VPP_REG_BITS(VPP_VE_ENABLE_CTRL, en, 4, 1);
		WRITE_VPP_REG_BITS(VPP_SLICE1_VE_ENABLE_CTRL, en, 4, 1);
		WRITE_VPP_REG_BITS(VPP_SLICE2_VE_ENABLE_CTRL, en, 4, 1);
		WRITE_VPP_REG_BITS(VPP_SLICE3_VE_ENABLE_CTRL, en, 4, 1);
		break;
	case WR_DMA:
		VSYNC_WRITE_VPP_REG_BITS(VPP_VE_ENABLE_CTRL, en, 4, 1);
		VSYNC_WRITE_VPP_REG_BITS(VPP_SLICE1_VE_ENABLE_CTRL, en, 4, 1);
		VSYNC_WRITE_VPP_REG_BITS(VPP_SLICE2_VE_ENABLE_CTRL, en, 4, 1);
		VSYNC_WRITE_VPP_REG_BITS(VPP_SLICE3_VE_ENABLE_CTRL, en, 4, 1);
		break;
	default:
		break;
	}
}

struct cm_port_s get_cm_port(void)
{
	struct cm_port_s port;

	port.cm_addr_port[0] = VPP_CHROMA_ADDR_PORT;
	port.cm_data_port[0] = VPP_CHROMA_DATA_PORT;
	port.cm_addr_port[1] = VPP_SLICE1_CHROMA_ADDR_PORT;
	port.cm_data_port[1] = VPP_SLICE1_CHROMA_DATA_PORT;
	port.cm_addr_port[2] = VPP_SLICE2_CHROMA_ADDR_PORT;
	port.cm_data_port[2] = VPP_SLICE2_CHROMA_DATA_PORT;
	port.cm_addr_port[3] = VPP_SLICE3_CHROMA_ADDR_PORT;
	port.cm_data_port[3] = VPP_SLICE3_CHROMA_DATA_PORT;

	return port;
}

/*modules after post matrix*/
void post_gainoff_cfg(struct tcon_rgb_ogo_s *p,
	enum wr_md_e mode, enum vpp_slice_e slice)
{
	unsigned int reg_ctl0;
	unsigned int reg_ctl1;
	unsigned int reg_ctl2;
	unsigned int reg_ctl3;
	unsigned int reg_ctl4;

	reg_ctl0 = VPP_GAINOFF_CTRL0 + pst_reg_ofst[slice];
	reg_ctl1 = VPP_GAINOFF_CTRL1 + pst_reg_ofst[slice];
	reg_ctl2 = VPP_GAINOFF_CTRL2 + pst_reg_ofst[slice];
	reg_ctl3 = VPP_GAINOFF_CTRL3 + pst_reg_ofst[slice];
	reg_ctl4 = VPP_GAINOFF_CTRL4 + pst_reg_ofst[slice];

	if (mode == WR_VCB) {
		WRITE_VPP_REG(reg_ctl0,
			((p->en << 31) & 0x80000000) |
			((p->r_gain << 16) & 0x07ff0000) |
			((p->g_gain <<  0) & 0x000007ff));
		WRITE_VPP_REG(reg_ctl1,
			((p->b_gain << 16) & 0x07ff0000) |
			((p->r_post_offset <<  0) & 0x00001fff));
		WRITE_VPP_REG(reg_ctl2,
			((p->g_post_offset << 16) & 0x1fff0000) |
			((p->b_post_offset <<  0) & 0x00001fff));
		WRITE_VPP_REG(reg_ctl3,
			((p->r_pre_offset  << 16) & 0x1fff0000) |
			((p->g_pre_offset  <<  0) & 0x00001fff));
		WRITE_VPP_REG(reg_ctl4,
			((p->b_pre_offset  <<  0) & 0x00001fff));
	} else if (mode == WR_DMA) {
		VSYNC_WRITE_VPP_REG(reg_ctl0,
			((p->en << 31) & 0x80000000) |
			((p->r_gain << 16) & 0x07ff0000) |
			((p->g_gain <<	0) & 0x000007ff));
		VSYNC_WRITE_VPP_REG(reg_ctl1,
			((p->b_gain << 16) & 0x07ff0000) |
			((p->r_post_offset <<  0) & 0x00001fff));
		VSYNC_WRITE_VPP_REG(reg_ctl2,
			((p->g_post_offset << 16) & 0x1fff0000) |
			((p->b_post_offset <<  0) & 0x00001fff));
		VSYNC_WRITE_VPP_REG(reg_ctl3,
			((p->r_pre_offset  << 16) & 0x1fff0000) |
			((p->g_pre_offset  <<  0) & 0x00001fff));
		VSYNC_WRITE_VPP_REG(reg_ctl4,
			((p->b_pre_offset  <<  0) & 0x00001fff));
	}
}

void post_gainoff_set(struct tcon_rgb_ogo_s *p,
	enum wr_md_e mode)
{
	int i;

	for (i = SLICE0; i < SLICE_MAX; i++)
		post_gainoff_cfg(p, mode, i);
}

void post_pre_gamma_set(int *lut)
{
	int i, j;
	unsigned int ctl_port;
	unsigned int addr_port;
	unsigned int data_port;

	for (j = SLICE0; j < SLICE_MAX; j++) {
		ctl_port = VPP_GAMMA_CTRL + pst_reg_ofst[j];
		addr_port = VPP_GAMMA_BIN_ADDR + pst_reg_ofst[j];
		data_port = VPP_GAMMA_BIN_DATA + pst_reg_ofst[j];
		WRITE_VPP_REG(addr_port, 0);
		for (i = 0; i < 33; i = i + 1)
			WRITE_VPP_REG(data_port,
				      (((lut[i * 2 + 1] << 2) & 0xffff) << 16 |
				      ((lut[i * 2] << 2) & 0xffff)));
		for (i = 0; i < 33; i = i + 1)
			WRITE_VPP_REG(data_port,
				      (((lut[i * 2 + 1] << 2) & 0xffff) << 16 |
				      ((lut[i * 2] << 2) & 0xffff)));
		for (i = 0; i < 33; i = i + 1)
			WRITE_VPP_REG(data_port,
				      (((lut[i * 2 + 1] << 2) & 0xffff) << 16 |
				      ((lut[i * 2] << 2) & 0xffff)));
		WRITE_VPP_REG_BITS(ctl_port, 0x3, 0, 2);
	}
}

/*vpp module enable/disable control*/
void ve_vadj_ctl(enum wr_md_e mode, enum vadj_index_e vadj_idx, int en)
{
	int i;

	switch (mode) {
	case WR_VCB:
		if (vadj_idx == VE_VADJ1) {
			WRITE_VPP_REG_BITS(VPP_VADJ1_MISC, en, 0, 1);
			for (i = SLICE1; i < SLICE_MAX; i++)
				WRITE_VPP_REG_BITS(VPP_SLICE1_VADJ1_MISC + ve_reg_ofst[i - 1],
					en, 0, 1);
		} else if (vadj_idx == VE_VADJ2) {
			for (i = SLICE0; i < SLICE_MAX; i++)
				WRITE_VPP_REG_BITS(VPP_VADJ2_MISC + pst_reg_ofst[i],
					en, 0, 1);
		}
		break;
	case WR_DMA:
		if (vadj_idx == VE_VADJ1) {
			VSYNC_WRITE_VPP_REG_BITS(VPP_VADJ1_MISC, en, 0, 1);
			for (i = SLICE1; i < SLICE_MAX; i++)
				VSYNC_WRITE_VPP_REG_BITS(VPP_SLICE1_VADJ1_MISC +
					ve_reg_ofst[i - 1], en, 0, 1);
		} else if (vadj_idx == VE_VADJ2) {
			for (i = SLICE0; i < SLICE_MAX; i++)
				VSYNC_WRITE_VPP_REG_BITS(VPP_VADJ2_MISC +
					pst_reg_ofst[i], en, 0, 1);
		}
		break;
	default:
		break;
	}
}

/*blue stretch can only use on slice0 on s5*/
void ve_bs_ctl(enum wr_md_e mode, int en)
{
	if (mode == WR_VCB) {
		WRITE_VPP_REG_BITS(VPP_BLUE_STRETCH_1, en, 31, 1);
		//for (i = SLICE1; i < SLICE_MAX; i++)
			//WRITE_VPP_REG_BITS(VPP_SLICE1_BLUE_STRETCH_1 +
				//ve_reg_ofst[i - 1], en, 31, 1);
	} else if (mode == WR_DMA) {
		VSYNC_WRITE_VPP_REG_BITS(VPP_BLUE_STRETCH_1, en, 31, 1);
		//for (i = SLICE1; i < SLICE_MAX; i++)
			//VSYNC_WRITE_VPP_REG_BITS(VPP_SLICE1_BLUE_STRETCH_1 +
				//ve_reg_ofst[i - 1], en, 31, 1);
	}
}

void ve_ble_ctl(enum wr_md_e mode, int en)
{
	int i;

	if (mode == WR_VCB) {
		WRITE_VPP_REG_BITS(VPP_VE_ENABLE_CTRL, en, 3, 1);
		for (i = SLICE1; i < SLICE_MAX; i++)
			WRITE_VPP_REG_BITS(VPP_SLICE1_VE_ENABLE_CTRL +
				ve_reg_ofst[i - 1], en, 3, 1);
	} else if (mode == WR_DMA) {
		VSYNC_WRITE_VPP_REG_BITS(VPP_VE_ENABLE_CTRL, en, 3, 1);
		for (i = SLICE1; i < SLICE_MAX; i++)
			VSYNC_WRITE_VPP_REG_BITS(VPP_SLICE1_VE_ENABLE_CTRL +
				ve_reg_ofst[i - 1], en, 3, 1);
	}
}

void ve_cc_ctl(enum wr_md_e mode, int en)
{
	int i;

	if (mode == WR_VCB) {
		WRITE_VPP_REG_BITS(VPP_VE_ENABLE_CTRL, en, 1, 1);
		for (i = SLICE1; i < SLICE_MAX; i++)
			WRITE_VPP_REG_BITS(VPP_SLICE1_VE_ENABLE_CTRL +
				ve_reg_ofst[i - 1], en, 1, 1);
	} else if (mode == WR_DMA) {
		VSYNC_WRITE_VPP_REG_BITS(VPP_VE_ENABLE_CTRL, en, 1, 1);
		for (i = SLICE1; i < SLICE_MAX; i++)
			VSYNC_WRITE_VPP_REG_BITS(VPP_SLICE1_VE_ENABLE_CTRL +
				ve_reg_ofst[i - 1], en, 1, 1);
	}
}

void post_wb_ctl(enum wr_md_e mode, int en)
{
	int i;

	if (mode == WR_VCB) {
		for (i = SLICE0; i < SLICE_MAX; i++)
			WRITE_VPP_REG_BITS(VPP_GAMMA_CTRL +
				pst_reg_ofst[i], en, 31, 1);
	} else if (mode == WR_DMA) {
		for (i = SLICE0; i < SLICE_MAX; i++)
			VSYNC_WRITE_VPP_REG_BITS(VPP_GAMMA_CTRL +
				pst_reg_ofst[i], en, 31, 1);
	}
}

void vpp_luma_hist_init(void)
{
	WRITE_VPP_REG_BITS(VI_HIST_CTRL, 2, 5, 3);
	/*select slice0,  all selection: vpp post, slc0~slc3,vd2, osd1,osd2,vd1 post*/
	WRITE_VPP_REG_BITS(VI_HIST_CTRL, 1, 11, 3);
	WRITE_VPP_REG_BITS(VI_HIST_CTRL, 0, 2, 1);
	/*full picture*/
	WRITE_VPP_REG_BITS(VI_HIST_CTRL, 0, 1, 1);
	/*enable*/
	WRITE_VPP_REG_BITS(VI_HIST_CTRL, 1, 0, 1);
}

void get_luma_hist(struct vframe_s *vf)
{
	static int pre_w, pre_h;
	int width, height;

	width = vf->width;
	height = vf->height;

	if (pre_w != width || pre_h != height) {
		WRITE_VPP_REG(VI_HIST_PIC_SIZE, width | (height << 16));
		pre_w = width;
		pre_h = height;
	}

	vf->prop.hist.vpp_luma_sum = READ_VPP_REG(VI_HIST_SPL_VAL);
	vf->prop.hist.vpp_pixel_sum = READ_VPP_REG(VI_HIST_SPL_PIX_CNT);
	vf->prop.hist.vpp_chroma_sum = READ_VPP_REG(VI_HIST_CHROMA_SUM);
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
}

