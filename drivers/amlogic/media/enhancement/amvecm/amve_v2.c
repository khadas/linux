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
	int reg_pre_offset0_1;
	int reg_pre_offset2;
	int reg_coef00_01;
	int reg_coef02_10;
	int reg_coef11_12;
	int reg_coef20_21;
	int reg_coef22;
	int reg_offset0_1;
	int reg_offset2;
	int reg_en_ctl;

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
		WRITE_VPP_REG_BITS(VPP_VE_ENABLE_CTRL, en, 4, 1);
		for (i = SLICE1; i < SLICE_MAX; i++)
			WRITE_VPP_REG_BITS(VPP_SLICE1_VE_ENABLE_CTRL +
				ve_reg_ofst[i - 1], en, 4, 1);
	} else if (mode == WR_DMA) {
		VSYNC_WRITE_VPP_REG_BITS(VPP_VE_ENABLE_CTRL, en, 4, 1);
		for (i = SLICE1; i < SLICE_MAX; i++)
			VSYNC_WRITE_VPP_REG_BITS(VPP_SLICE1_VE_ENABLE_CTRL +
				ve_reg_ofst[i - 1], en, 4, 1);
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

