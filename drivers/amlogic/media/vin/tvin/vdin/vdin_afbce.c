// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * drivers/amlogic/media/vin/tvin/vdin/vdin_afbce.c
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

/******************** READ ME ************************
 *
 * at afbce mode, 1 block = 32 * 4 pixel
 * there is a header in one block.
 * for example at 1080p,
 * header numbers = block numbers = 1920 * 1080 / (32 * 4)
 *
 * table map(only at non-mmu mode):
 * afbce data was saved at "body" region,
 * body region has been divided for every 4K(4096 bytes) and 4K unit,
 * table map contents is : (body addr >> 12)
 *
 * at non-mmu mode(just vdin non-mmu mode):
 * ------------------------------
 *          header
 *     (can analysis body addr)
 * ------------------------------
 *          table map
 *     (save body addr)
 * ------------------------------
 *          body
 *     (save afbce data)
 * ------------------------------
 */
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/cma.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/dma-contiguous.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/slab.h>
#include <linux/amlogic/media/frame_provider/tvin/tvin.h>
#include "../tvin_global.h"
#include "../tvin_format_table.h"
#include "vdin_ctl.h"
#include "vdin_regs.h"
#include "vdin_drv.h"
#include "vdin_vf.h"
#include "vdin_canvas.h"
#include "vdin_afbce.h"
#include "vdin_mem_scatter.h"

bool vdin_is_afbce_enabled(struct vdin_dev_s *devp)
{
	return (devp->afbce_mode == 1 || devp->double_wr);
}

/* fixed config mif by default */
void vdin_mif_config_init(struct vdin_dev_s *devp)
{
	if (is_meson_s5_cpu()) {
		if (devp->index == 0) {
			W_VCBUS_BIT(VDIN_TOP_MISC0,
				0, WR_MIF_FIX_DISABLE_BIT, WR_MIF_FIX_DISABLE_WID);
			//W_VCBUS_BIT(VDIN_TOP_MISC0,
			//		0, VDIN0_OUT_AFBCE_BIT, 1);
			//W_VCBUS_BIT(VDIN_TOP_MISC0,
			//		1, VDIN0_OUT_MIF_BIT, 1);
		} else {
			W_VCBUS_BIT(VDIN_TOP_MISC1,
					0, WR_MIF_FIX_DISABLE_BIT, WR_MIF_FIX_DISABLE_WID);
			//W_VCBUS_BIT(VDIN_TOP_MISC1,
			//		0, VDIN_TOP_MISC1, 1);
			//W_VCBUS_BIT(VDIN_TOP_MISC0,
			//		1, VDIN_TOP_MISC1, 1);
		}
	} else {
		if (devp->index == 0) {
			W_VCBUS_BIT(VDIN_MISC_CTRL,
				    1, VDIN0_MIF_ENABLE_BIT, 1);
			W_VCBUS_BIT(VDIN_MISC_CTRL,
				    0, VDIN0_OUT_AFBCE_BIT, 1);
			W_VCBUS_BIT(VDIN_MISC_CTRL,
				    1, VDIN0_OUT_MIF_BIT, 1);
		} else {
			W_VCBUS_BIT(VDIN_MISC_CTRL,
				    1, VDIN1_MIF_ENABLE_BIT, 1);
			W_VCBUS_BIT(VDIN_MISC_CTRL,
				    0, VDIN1_OUT_AFBCE_BIT, 1);
			W_VCBUS_BIT(VDIN_MISC_CTRL,
				    1, VDIN1_OUT_MIF_BIT, 1);
		}
	}
}

/* only support init vdin0 mif/afbce */
void vdin_write_mif_or_afbce_init(struct vdin_dev_s *devp)
{
	enum vdin_output_mif_e sel;

	if (((devp->afbce_flag & VDIN_AFBCE_EN) == 0) || devp->index == 1 ||
	    devp->double_wr)
		return;

	if (devp->afbce_mode == 0)
		sel = VDIN_OUTPUT_TO_MIF;
	else
		sel = VDIN_OUTPUT_TO_AFBCE;

	if (sel == VDIN_OUTPUT_TO_MIF) {
		W_VCBUS_BIT(AFBCE_ENABLE, 0, AFBCE_EN_BIT, AFBCE_EN_WID);

		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TM2)) {
			W_VCBUS_BIT(VDIN_TOP_DOUBLE_CTRL, WR_SEL_VDIN0_NOR,
				    MIF0_OUT_SEL_BIT, VDIN_REORDER_SEL_WID);
			W_VCBUS_BIT(VDIN_TOP_DOUBLE_CTRL, WR_SEL_DIS,
				    AFBCE_OUT_SEL_BIT, VDIN_REORDER_SEL_WID);

			/* axi write protection
			 * for HDMI cable plug/unplug crash issue
			 */
			W_VCBUS_BIT(VPU_AXI_WR_PROTECT, 0x8000,
				    HOLD_NUM_BIT, HOLD_NUM_WID);
			W_VCBUS_BIT(VPU_AXI_WR_PROTECT, 1,
				    PROTECT_EN1_BIT, PROTECT_EN_WID);
			W_VCBUS_BIT(VPU_AXI_WR_PROTECT, 1,
				    PROTECT_EN21_BIT, PROTECT_EN_WID);
		} else {
			W_VCBUS_BIT(VDIN_MISC_CTRL, 1, VDIN0_MIF_ENABLE_BIT, 1);
			W_VCBUS_BIT(VDIN_MISC_CTRL, 0, VDIN0_OUT_AFBCE_BIT, 1);
			W_VCBUS_BIT(VDIN_MISC_CTRL, 1, VDIN0_OUT_MIF_BIT, 1);
		}
	} else if (sel == VDIN_OUTPUT_TO_AFBCE) {
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TM2)) {
			W_VCBUS_BIT(VDIN_TOP_DOUBLE_CTRL, WR_SEL_DIS,
				    MIF0_OUT_SEL_BIT, VDIN_REORDER_SEL_WID);
			W_VCBUS_BIT(VDIN_TOP_DOUBLE_CTRL, WR_SEL_VDIN0_NOR,
				    AFBCE_OUT_SEL_BIT, VDIN_REORDER_SEL_WID);

			/* axi write protection
			 * for HDMI cable plug/unplug crash issue
			 */
			W_VCBUS(VPU_AXI_WR_PROTECT, 0);
		} else {
			W_VCBUS_BIT(VDIN_MISC_CTRL, 1, VDIN0_MIF_ENABLE_BIT, 1);
			W_VCBUS_BIT(VDIN_MISC_CTRL, 0, VDIN0_OUT_MIF_BIT, 1);
			W_VCBUS_BIT(VDIN_MISC_CTRL, 1, VDIN0_OUT_AFBCE_BIT, 1);
		}

		W_VCBUS_BIT(AFBCE_ENABLE, 0, AFBCE_EN_BIT, AFBCE_EN_WID);
	}
}

/* only support config vdin0 mif/afbce dynamically */
void vdin_write_mif_or_afbce(struct vdin_dev_s *devp,
			     enum vdin_output_mif_e sel)
{
	if (((devp->afbce_flag & VDIN_AFBCE_EN) == 0) || devp->double_wr)
		return;

	if (sel == VDIN_OUTPUT_TO_MIF) {
		rdma_write_reg_bits(devp->rdma_handle, AFBCE_ENABLE, 0, AFBCE_EN_BIT, AFBCE_EN_WID);

		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TM2)) {
			rdma_write_reg_bits(devp->rdma_handle,
					    VDIN_TOP_DOUBLE_CTRL, WR_SEL_VDIN0_NOR,
					    MIF0_OUT_SEL_BIT, VDIN_REORDER_SEL_WID);
			rdma_write_reg_bits(devp->rdma_handle,
					    VDIN_TOP_DOUBLE_CTRL, WR_SEL_DIS,
					    AFBCE_OUT_SEL_BIT, VDIN_REORDER_SEL_WID);

			/* axi write protection
			 * for HDMI cable plug/unplug crash issue
			 */
			rdma_write_reg_bits(devp->rdma_handle,
					    VPU_AXI_WR_PROTECT, 0x8000,
					    HOLD_NUM_BIT, HOLD_NUM_WID);
			rdma_write_reg_bits(devp->rdma_handle,
					    VPU_AXI_WR_PROTECT, 1,
					    PROTECT_EN1_BIT, PROTECT_EN_WID);
			rdma_write_reg_bits(devp->rdma_handle,
					    VPU_AXI_WR_PROTECT, 1,
					    PROTECT_EN21_BIT, PROTECT_EN_WID);
		} else {
			rdma_write_reg_bits(devp->rdma_handle, VDIN_MISC_CTRL,
					    0, VDIN0_OUT_AFBCE_BIT, 1);
			rdma_write_reg_bits(devp->rdma_handle, VDIN_MISC_CTRL,
					    1, VDIN0_OUT_MIF_BIT, 1);
		}
	} else if (sel == VDIN_OUTPUT_TO_AFBCE) {
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TM2)) {
			rdma_write_reg_bits(devp->rdma_handle,
					    VDIN_TOP_DOUBLE_CTRL, WR_SEL_DIS,
					    MIF0_OUT_SEL_BIT, VDIN_REORDER_SEL_WID);
			rdma_write_reg_bits(devp->rdma_handle,
					    VDIN_TOP_DOUBLE_CTRL, WR_SEL_VDIN0_NOR,
					    AFBCE_OUT_SEL_BIT, VDIN_REORDER_SEL_WID);

			/* axi write protection
			 * for HDMI cable plug/unplug crash issue
			 */
			rdma_write_reg(devp->rdma_handle, VPU_AXI_WR_PROTECT,
				       0);
		} else {
			rdma_write_reg_bits(devp->rdma_handle, VDIN_MISC_CTRL,
					    0, VDIN0_OUT_MIF_BIT, 1);
			rdma_write_reg_bits(devp->rdma_handle, VDIN_MISC_CTRL,
					    1, VDIN0_OUT_AFBCE_BIT, 1);
		}

		if (devp->afbce_flag & VDIN_AFBCE_EN_LOSSY)
			rdma_write_reg(devp->rdma_handle, AFBCE_QUANT_ENABLE,
				       0xc11);
		rdma_write_reg_bits(devp->rdma_handle, AFBCE_ENABLE, 1, AFBCE_EN_BIT, AFBCE_EN_WID);
	}
}

bool vdin_chk_is_comb_mode(struct vdin_dev_s *devp)
{
	enum vdin_format_convert_e vdin_out_fmt;
	int reg_fmt444_rgb_en = false;
	int reg_fmt444_comb = false;

	vdin_out_fmt = devp->format_convert;
	if (vdin_out_fmt == VDIN_FORMAT_CONVERT_YUV_RGB ||
	    vdin_out_fmt == VDIN_FORMAT_CONVERT_YUV_GBR ||
	    vdin_out_fmt == VDIN_FORMAT_CONVERT_YUV_BRG ||
	    vdin_out_fmt == VDIN_FORMAT_CONVERT_RGB_RGB)
		reg_fmt444_rgb_en = true;

	if ((vdin_out_fmt == VDIN_FORMAT_CONVERT_YUV_YUV444 ||
	     vdin_out_fmt == VDIN_FORMAT_CONVERT_RGB_YUV444 || reg_fmt444_rgb_en) &&
	     devp->h_active > 2048)
		reg_fmt444_comb = true;
	else
		reg_fmt444_comb = false;

	return reg_fmt444_comb;
}

#define VDIN_AFBCE_HOLD_LINE_NUM    4
void vdin_afbce_update(struct vdin_dev_s *devp)
{
	int hold_line_num = VDIN_AFBCE_HOLD_LINE_NUM;
	int reg_format_mode;/* 0:444 1:422 2:420 */
	int reg_fmt444_comb;
	int bits_num;
	int uncompress_bits;
	int uncompress_size;

	if (!devp->afbce_info)
		return;

#ifndef CONFIG_AMLOGIC_MEDIA_RDMA
	pr_info("##############################################\n");
	pr_info("vdin afbce must use RDMA,but it not be opened\n");
	pr_info("##############################################\n");
#endif
	reg_fmt444_comb = vdin_chk_is_comb_mode(devp);

	switch (devp->format_convert) {
	case VDIN_FORMAT_CONVERT_YUV_NV12:
	case VDIN_FORMAT_CONVERT_YUV_NV21:
	case VDIN_FORMAT_CONVERT_RGB_NV12:
	case VDIN_FORMAT_CONVERT_RGB_NV21:
		reg_format_mode = 2;
		bits_num = 12;
		break;
	case VDIN_FORMAT_CONVERT_YUV_YUV422:
	case VDIN_FORMAT_CONVERT_RGB_YUV422:
	case VDIN_FORMAT_CONVERT_GBR_YUV422:
	case VDIN_FORMAT_CONVERT_BRG_YUV422:
		reg_format_mode = 1;
		bits_num = 16;
		break;
	default:
		reg_format_mode = 0;
		bits_num = 24;
		break;
	}
	uncompress_bits = devp->source_bitdepth;

	/* bit size of uncompressed mode */
	uncompress_size = (((((16 * uncompress_bits * bits_num) + 7) >> 3) + 31)
		      / 32) << 1;
	rdma_write_reg(devp->rdma_handle, AFBCE_MODE,
		       (0 & 0x7) << 29 | (0 & 0x3) << 26 | (3 & 0x3) << 24 |
		       (hold_line_num & 0x7f) << 16 |
		       (2 & 0x3) << 14 | (reg_fmt444_comb & 0x1));

	rdma_write_reg_bits(devp->rdma_handle,
			    AFBCE_MIF_SIZE,
			    (uncompress_size & 0x1fff), 16, 5);/* uncmp_size */

	rdma_write_reg(devp->rdma_handle, AFBCE_FORMAT,
		       (reg_format_mode  & 0x3) << 8 |
		       (uncompress_bits & 0xf) << 4 |
		       (uncompress_bits & 0xf));
}

void vdin_afbce_config(struct vdin_dev_s *devp)
{
	int hold_line_num = VDIN_AFBCE_HOLD_LINE_NUM;
	int lbuf_depth = 256;
	int lossy_luma_en = 0;
	int lossy_chrm_en = 0;
	int cur_mmu_used = 0;
	int reg_format_mode;//0:444 1:422 2:420
	int reg_fmt444_comb;
	int bits_num;
	int uncompress_bits;
	int uncompress_size;
	int def_color_0 = 0x3ff;
	int def_color_1 = 0x80;
	int def_color_2 = 0x80;
	int def_color_3 = 0;
	int h_blk_size_out = (devp->h_active + 31) >> 5;
	int v_blk_size_out = (devp->v_active + 3)  >> 2;
	int blk_out_end_h;//output blk scope
	int blk_out_bgn_h;//output blk scope
	int blk_out_end_v;//output blk scope
	int blk_out_bgn_v;//output blk scope
	int enc_win_bgn_h;//input scope
	int enc_win_end_h;//input scope
	int enc_win_bgn_v;//input scope
	int enc_win_end_v;//input scope
	int reg_fmt444_rgb_en = 0;
	enum vdin_format_convert_e vdin_out_fmt;
	unsigned int bit_mode_shift = 0;

	if (!devp->afbce_info)
		return;

#ifndef CONFIG_AMLOGIC_MEDIA_RDMA
	pr_info("##############################################\n");
	pr_info("vdin afbce must use RDMA,but it not be opened\n");
	pr_info("##############################################\n");
#endif

	enc_win_bgn_h = 0;
	enc_win_end_h = devp->h_active - 1;
	enc_win_bgn_v = 0;
	enc_win_end_v = devp->v_active - 1;

	blk_out_end_h	=  enc_win_bgn_h      >> 5 ;//output blk scope
	blk_out_bgn_h	= (enc_win_end_h + 31)	>> 5 ;//output blk scope
	blk_out_end_v	=  enc_win_bgn_v      >> 2 ;//output blk scope
	blk_out_bgn_v	= (enc_win_end_v + 3) >> 2 ;//output blk scope

	vdin_out_fmt = devp->format_convert;
	if (vdin_out_fmt == VDIN_FORMAT_CONVERT_YUV_RGB ||
	    vdin_out_fmt == VDIN_FORMAT_CONVERT_YUV_GBR ||
	    vdin_out_fmt == VDIN_FORMAT_CONVERT_YUV_BRG ||
	    vdin_out_fmt == VDIN_FORMAT_CONVERT_RGB_RGB)
		reg_fmt444_rgb_en = 1;

	reg_fmt444_comb = vdin_chk_is_comb_mode(devp);
	if (vdin_out_fmt == VDIN_FORMAT_CONVERT_YUV_NV12 ||
	    vdin_out_fmt == VDIN_FORMAT_CONVERT_YUV_NV21 ||
	    vdin_out_fmt == VDIN_FORMAT_CONVERT_RGB_NV12 ||
	    vdin_out_fmt == VDIN_FORMAT_CONVERT_RGB_NV21) {
		reg_format_mode = 2;/*420*/
	    bits_num = 12;
	} else if ((vdin_out_fmt == VDIN_FORMAT_CONVERT_YUV_YUV422) ||
		(vdin_out_fmt == VDIN_FORMAT_CONVERT_RGB_YUV422) ||
		(vdin_out_fmt == VDIN_FORMAT_CONVERT_GBR_YUV422) ||
		(vdin_out_fmt == VDIN_FORMAT_CONVERT_BRG_YUV422)) {
		reg_format_mode = 1;/*422*/
	    bits_num = 16;
	} else {
		reg_format_mode = 0;/*444*/
	    bits_num = 24;
	}

	uncompress_bits = devp->source_bitdepth;

	//bit size of uncompressed mode
	uncompress_size = (((((16 * uncompress_bits * bits_num) + 7) >> 3) + 31)
		      / 32) << 1;
	pr_info("%s fmt_convert:%d comb:%d\n", __func__,
		devp->format_convert, reg_fmt444_comb);
	W_VCBUS_BIT(AFBCE_MODE_EN, 1, 18, 1);/* disable order mode */

	W_VCBUS_BIT(VDIN_WRARB_REQEN_SLV, 0x1, 3, 1);//vpu arb axi_enable
	W_VCBUS_BIT(VDIN_WRARB_REQEN_SLV, 0x1, 7, 1);//vpu arb axi_enable

	W_VCBUS(AFBCE_MODE,
		(0 & 0x7) << 29 | (0 & 0x3) << 26 | (3 & 0x3) << 24 |
		(hold_line_num & 0x7f) << 16 |
		(2 & 0x3) << 14 | (reg_fmt444_comb & 0x1));

	W_VCBUS_BIT(AFBCE_QUANT_ENABLE, (lossy_luma_en & 0x1), 0, 1);//lossy
	W_VCBUS_BIT(AFBCE_QUANT_ENABLE, (lossy_chrm_en & 0x1), 4, 1);//lossy

	if (devp->afbce_flag & VDIN_AFBCE_EN_LOSSY) {
		W_VCBUS(AFBCE_QUANT_ENABLE, 0xc11);
		pr_info("afbce use lossy compression mode\n");
	}

	W_VCBUS(AFBCE_SIZE_IN,
		((devp->h_active & 0x1fff) << 16) |  // hsize_in of afbc input
		((devp->v_active & 0x1fff) << 0)    // vsize_in of afbc input
		);

	W_VCBUS(AFBCE_BLK_SIZE_IN,
		((h_blk_size_out & 0x1fff) << 16) |  // out blk hsize
		((v_blk_size_out & 0x1fff) << 0)	  // out blk vsize
		);

	//head addr of compressed data
	if (devp->dtdata->hw_ver >= VDIN_HW_T7)
		W_VCBUS(AFBCE_HEAD_BADDR,
			devp->afbce_info->fm_head_paddr[0] >> 4);
	else
		W_VCBUS(AFBCE_HEAD_BADDR,
			devp->afbce_info->fm_head_paddr[0]);

	W_VCBUS_BIT(AFBCE_MIF_SIZE, (uncompress_size & 0x1fff), 16, 5);//uncompress_size

	/* how to set reg when we use crop ? */
	// scope of hsize_in ,should be a integer multiple of 32
	// scope of vsize_in ,should be a integer multiple of 4
	W_VCBUS(AFBCE_PIXEL_IN_HOR_SCOPE,
		((enc_win_end_h & 0x1fff) << 16) |
		((enc_win_bgn_h & 0x1fff) << 0));

	// scope of hsize_in ,should be a integer multiple of 32
	// scope of vsize_in ,should be a integer multiple of 4
	W_VCBUS(AFBCE_PIXEL_IN_VER_SCOPE,
		((enc_win_end_v & 0x1fff) << 16) |
		((enc_win_bgn_v & 0x1fff) << 0));

	W_VCBUS(AFBCE_CONV_CTRL, lbuf_depth);//fix 256

	W_VCBUS(AFBCE_MIF_HOR_SCOPE,
		((blk_out_bgn_h & 0x3ff) << 16) |  // scope of out blk h_size
		((blk_out_end_h & 0xfff) << 0)	  // scope of out blk v_size
		);

	W_VCBUS(AFBCE_MIF_VER_SCOPE,
		((blk_out_bgn_v & 0x3ff) << 16) |  // scope of out blk h_size
		((blk_out_end_v & 0xfff) << 0)	  // scope of out blk v_size
		);

	W_VCBUS(AFBCE_FORMAT,
		(reg_format_mode  & 0x3) << 8 |
		(uncompress_bits & 0xf) << 4 |
		(uncompress_bits & 0xf));

	W_VCBUS(AFBCE_DEF_COLOR_1,
		((def_color_3 & 0xfff) << 12) |  // def_color_a
		((def_color_0 & 0xfff) << 0)	// def_color_y
		);

	if (devp->source_bitdepth >= VDIN_COLOR_DEEPS_8BIT &&
	    devp->source_bitdepth <= VDIN_COLOR_DEEPS_12BIT)
		bit_mode_shift = devp->source_bitdepth - VDIN_COLOR_DEEPS_8BIT;
	/*def_color_v*/
	/*def_color_u*/
	W_VCBUS(AFBCE_DEF_COLOR_2,
		(((def_color_2 << bit_mode_shift) & 0xfff) << 12) |
		(((def_color_1 << bit_mode_shift) & 0xfff) << 0));
	if (devp->dtdata->hw_ver >= VDIN_HW_T7)
		W_VCBUS_BIT(AFBCE_MMU_RMIF_CTRL4,
			    devp->afbce_info->fm_table_paddr[0] >> 4, 0, 32);
	else
		W_VCBUS_BIT(AFBCE_MMU_RMIF_CTRL4,
			    devp->afbce_info->fm_table_paddr[0], 0, 32);

	W_VCBUS_BIT(AFBCE_MMU_RMIF_SCOPE_X, cur_mmu_used, 0, 12);
	/*for almost uncompressed pattern,garbage at bottom
	 *(h_active * v_active * bytes per pixel + 3M) / page_size - 1
	 *where 3M is the rest bytes of block,since every block must not be\
	 *separated by 2 pages
	 */
	W_VCBUS_BIT(AFBCE_MMU_RMIF_SCOPE_X, 0x1c4f, 16, 13);

	W_VCBUS_BIT(AFBCE_ENABLE, 0, AFBCE_WORK_MD_BIT, AFBCE_WORK_MD_WID);

//	if (devp->double_wr)
//		W_VCBUS_BIT(AFBCE_ENABLE, 1, AFBCE_EN_BIT, AFBCE_EN_WID);
//	else
//		W_VCBUS_BIT(AFBCE_ENABLE, 0, AFBCE_EN_BIT, AFBCE_EN_WID);

	if (devp->debug.dbg_print_cntl & VDIN_ADDRESS_DBG &&
	    devp->irq_cnt < VDIN_DBG_PRINT_CNT)
		pr_err("%s %#x:%#x(%#lx) %#x:%#x(%#lx)\n",
			__func__, AFBCE_HEAD_BADDR, rd(0, AFBCE_HEAD_BADDR),
			devp->afbce_info->fm_head_paddr[0],
			AFBCE_MMU_RMIF_CTRL4, rd(0, AFBCE_MMU_RMIF_CTRL4),
			devp->afbce_info->table_paddr);
}

void vdin_afbce_maptable_init(struct vdin_dev_s *devp)
{
	unsigned int i, j;
	unsigned int highmem_flag = 0;
	unsigned long phys_addr = 0;
	unsigned int *virt_addr = NULL;
	unsigned int body;
	unsigned int size;
	void *p = NULL;

	if (!devp->afbce_info)
		return;

	if (devp->mem_type == VDIN_MEM_TYPE_SCT) {
		pr_info("%s,use VDIN_MEM_TYPE_SCT\n", __func__);
		return;
	}

	size = roundup(devp->afbce_info->frame_body_size, 4096);

	phys_addr = devp->afbce_info->fm_table_paddr[0];
	if ((devp->cma_config_flag & 0xfff) == 0x101)
		highmem_flag = PageHighMem(phys_to_page(phys_addr));
	else
		highmem_flag = PageHighMem(phys_to_page(phys_addr));

	for (i = 0; i < devp->vf_mem_max_cnt; i++) {
		phys_addr = devp->afbce_info->fm_table_paddr[i];
		if (!phys_addr) {
			pr_info("%s,i=%d,phys_addr == NULL.\n", __func__, i);
			continue;
		}
		if (highmem_flag == 0) {
			if ((devp->cma_config_flag & 0xfff) == 0x101)
				virt_addr = codec_mm_phys_to_virt(phys_addr);
			else if (devp->cma_config_flag == 0)
				virt_addr = phys_to_virt(phys_addr);
			else
				virt_addr = phys_to_virt(phys_addr);
		} else {
			virt_addr = (unsigned int *)vdin_vmap(phys_addr,
				devp->afbce_info->frame_table_size);
			if (vdin_dbg_en) {
				pr_err("----vdin vmap v: %p, p: %lx, size: %d\n",
				       virt_addr, phys_addr,
				       devp->afbce_info->frame_table_size);
			}
			if (!virt_addr) {
				pr_err("vmap fail, size: %d.\n",
				       devp->afbce_info->frame_table_size);
				return;
			}
		}

		p = virt_addr;
		body = devp->afbce_info->fm_body_paddr[i] & 0xffffffff;
		for (j = 0; j < size; j += 4096) {
			*virt_addr = ((j + body) >> 12) & 0x000fffff;
			virt_addr++;
		}

		/* clean tail data. */
		memset(virt_addr, 0, devp->afbce_info->frame_table_size -
		       ((char *)virt_addr - (char *)p));

		vdin_dma_flush(devp, p,
			       devp->afbce_info->frame_table_size,
			       DMA_TO_DEVICE);

		if (highmem_flag)
			vdin_unmap_phyaddr(p);
	}
}

static void vdin_afbce_addr_check(struct vdin_dev_s *devp, unsigned long fm_table_paddr)
{
	unsigned int i = 0, j, cnt = 0;
	unsigned int *virt_addr = NULL;
	unsigned int body;
	unsigned int size;

	if (!devp->afbce_info)
		return;

	if (devp->mem_type == VDIN_MEM_TYPE_SCT) {
		pr_info("%s,use VDIN_MEM_TYPE_SCT\n", __func__);
		return;
	}

	size = roundup(devp->afbce_info->frame_body_size, 4096);
	for (i = 0; i < devp->vf_mem_max_cnt; i++) {
		if (!devp->afbce_info->fm_table_paddr[i])
			return;
		if (fm_table_paddr == devp->afbce_info->fm_table_paddr[i])
			break;
	}
	if (i >= devp->vf_mem_max_cnt)
		return;

	pr_err("fm_idx:%d,table val(%#lx)\n", i, fm_table_paddr);

	virt_addr = phys_to_virt(fm_table_paddr);

	body = devp->afbce_info->fm_body_paddr[i] & 0xffffffff;
	for (j = 0; j < size; j += 4096) {
		if ((*virt_addr != (((j + body) >> 12) & 0x000fffff)) ||
			(devp->debug.dbg_print_cntl & VDIN_AFBC_RD_DBG)) {
			pr_err("offset[%#x] should be:(%#x),readback:(%#x)\n",
				cnt, (((j + body) >> 12) & 0x000fffff), *virt_addr);
		}
		cnt++;
		virt_addr++;
	}
}

void vdin_afbce_set_next_frame(struct vdin_dev_s *devp,
			       unsigned int rdma_enable, struct vf_entry *vfe)
{
	unsigned char vf_idx;
	unsigned long compHeadAddr;
	unsigned long compTableAddr;
	unsigned long compBodyAddr;

	if (!devp->afbce_info)
		return;

	if ((vfe->flag & VF_FLAG_ONE_BUFFER_MODE) &&
	     devp->af_num < VDIN_CANVAS_MAX_CNT)
		vf_idx = devp->af_num; //one buffer mode
	else
		vf_idx = vfe->af_num;

	compHeadAddr	= devp->afbce_info->fm_head_paddr[vf_idx];
	compTableAddr	= devp->afbce_info->fm_table_paddr[vf_idx];
	compBodyAddr	= devp->afbce_info->fm_body_paddr[vf_idx];

	vfe->vf.compHeadAddr = compHeadAddr;
	vfe->vf.compBodyAddr = compBodyAddr;

#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
	if (rdma_enable) {
		if (devp->dtdata->hw_ver >= VDIN_HW_T7) {
			rdma_write_reg(devp->rdma_handle, AFBCE_HEAD_BADDR,
				compHeadAddr >> 4);
			rdma_write_reg(devp->rdma_handle, AFBCE_MMU_RMIF_CTRL4,
				compTableAddr >> 4);
		} else {
			rdma_write_reg(devp->rdma_handle, AFBCE_HEAD_BADDR,
				compHeadAddr);
			rdma_write_reg(devp->rdma_handle, AFBCE_MMU_RMIF_CTRL4,
				compTableAddr);
		}
		rdma_write_reg_bits(devp->rdma_handle, AFBCE_ENABLE, 1,
				AFBCE_START_PULSE_BIT, AFBCE_START_PULSE_WID);

		if (devp->pause_dec || devp->debug.pause_afbce_dec)
			rdma_write_reg_bits(devp->rdma_handle, AFBCE_ENABLE, 0,
				AFBCE_EN_BIT, AFBCE_EN_WID);
		else
			rdma_write_reg_bits(devp->rdma_handle, AFBCE_ENABLE, 1,
				AFBCE_EN_BIT, AFBCE_EN_WID);
	} else {
		if (devp->dtdata->hw_ver >= VDIN_HW_T7) {
			W_VCBUS(AFBCE_HEAD_BADDR, compHeadAddr >> 4);
			W_VCBUS(AFBCE_MMU_RMIF_CTRL4, compTableAddr >> 4);
		} else {
			W_VCBUS(AFBCE_HEAD_BADDR, compHeadAddr);
			W_VCBUS(AFBCE_MMU_RMIF_CTRL4, compTableAddr);
		}
	}
#endif
	vdin_afbce_clear_write_down_flag(devp);

	if (devp->debug.dbg_print_cntl & VDIN_ADDRESS_DBG &&
	    devp->irq_cnt < VDIN_DBG_PRINT_CNT) {
		pr_err("%s head,%#x:%#x(%#lx) table,%#x:%#x(%#lx) body,%#lx\n",
			__func__, AFBCE_HEAD_BADDR, rd(0, AFBCE_HEAD_BADDR),
			compHeadAddr,
			AFBCE_MMU_RMIF_CTRL4, rd(0, AFBCE_MMU_RMIF_CTRL4) << 4,
			compTableAddr, compBodyAddr);
		if (devp->debug.dbg_print_cntl & VDIN_AFBC_ADDR_CHK)
			vdin_afbce_addr_check(devp, rd(0, AFBCE_MMU_RMIF_CTRL4) << 4);
	}
}

void vdin_pause_afbce_write(struct vdin_dev_s *devp, unsigned int rdma_enable)
{
#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
	if (rdma_enable)
		rdma_write_reg_bits(devp->rdma_handle, AFBCE_ENABLE, 0,
				    AFBCE_EN_BIT, AFBCE_EN_WID);
	else
		W_VCBUS_BIT(AFBCE_ENABLE, 0, AFBCE_EN_BIT, AFBCE_EN_WID);
#endif
	vdin_afbce_clear_write_down_flag(devp);
}

/* frm_end will not pull up if using rdma IF to clear afbce flag */
void vdin_afbce_clear_write_down_flag(struct vdin_dev_s *devp)
{
	/* bit0:frm_end_clr;bit1:enc_error_clr */
	W_VCBUS_BIT(AFBCE_CLR_FLAG, 3, 0, 2);
}

/* return 1: write down */
int vdin_afbce_read_write_down_flag(void)
{
	int frm_end = -1, wr_abort = -1;

	frm_end = rd_bits(0, AFBCE_STA_FLAG, 0, 1);
	//frm_end = rd_bits(0, AFBCE_STAT1, 31, 1);
	wr_abort = rd_bits(0, AFBCE_STA_FLAG, 2, 2);

	if (vdin_isr_monitor & VDIN_ISR_MONITOR_WRITE_DONE)
		pr_info("frm_end:%#x,wr_abort:%#x\n",
			frm_end, wr_abort);

	if (frm_end == 1 && wr_abort == 0)
		return 1;
	else
		return 0;
}

void vdin_afbce_soft_reset(void)
{
	if (is_meson_s5_cpu())
		return; //TODO

	W_VCBUS_BIT(AFBCE_ENABLE, 0, AFBCE_EN_BIT, AFBCE_EN_WID);
	W_VCBUS_BIT(AFBCE_MODE, 0, 30, 1);
	W_VCBUS_BIT(AFBCE_MODE, 1, 30, 1);
	W_VCBUS_BIT(AFBCE_MODE, 0, 30, 1);
}

void vdin_afbce_mode_init(struct vdin_dev_s *devp)
{
	/* afbce_valid means can switch into afbce mode */
	devp->afbce_valid = 0;
	if (devp->afbce_flag & VDIN_AFBCE_EN) {
		if (devp->h_active > 1920 && devp->v_active >= 1080) {
			if (devp->afbce_flag & VDIN_AFBCE_EN_4K)
				devp->afbce_valid = 1;
		} else if (devp->h_active > 1280 && devp->v_active > 720) {
			if (devp->afbce_flag & VDIN_AFBCE_EN_1080P)
				devp->afbce_valid = 1;
		} else if (devp->h_active > 720 && devp->v_active > 576) {
			if (devp->afbce_flag & VDIN_AFBCE_EN_720P)
				devp->afbce_valid = 1;
		} else {
			if (devp->afbce_flag & VDIN_AFBCE_EN_SMALL)
				devp->afbce_valid = 1;
		}
		/*afbc up up 4k 444*/
		/* if is hdr mode, not enable afbc mode*/
		/* if (devp->prop.hdr_info.hdr_state == HDR_STATE_GET) {
		 *	if ((devp->prop.hdr_info.hdr_data.eotf ==
		 *			EOTF_HDR) ||
		 *		(devp->prop.hdr_info.hdr_data.eotf ==
		 *			EOTF_SMPTE_ST_2048) ||
		 *		(devp->prop.hdr_info.hdr_data.eotf ==
		 *			EOTF_HLG))
		 *	devp->afbce_valid = false;
		 *}
		 *
		 *if (devp->prop.hdr10p_info.hdr10p_on)
		 *	devp->afbce_valid = false;
		 */
	}

	devp->afbce_flag = devp->dts_config.afbce_flag_cfg;
	/* In dolby afbce mode,disable lossy */
	if (devp->afbce_valid && vdin_is_dolby_signal_in(devp) &&
		(devp->vdin_function_sel & VDIN_AFBCE_DOLBY))
		devp->afbce_flag &= ~(VDIN_AFBCE_EN_LOSSY);

	/* default non-afbce mode
	 * switch to afbce_mode if need by vpp notify
	 */
	devp->afbce_mode = 0;
	devp->afbce_mode_pre = devp->afbce_mode;
	if (vdin_dbg_en)
		pr_info("vdin%d init afbce_mode: %d,afbce_flag:%#x %#x\n",
			devp->index, devp->afbce_mode,
			devp->dts_config.afbce_flag_cfg, devp->afbce_flag);
}

void vdin_afbce_mode_update(struct vdin_dev_s *devp)
{
	/* vdin mif/afbce mode update */
	if (devp->afbce_mode)
		vdin_write_mif_or_afbce(devp, VDIN_OUTPUT_TO_AFBCE);
	else
		vdin_write_mif_or_afbce(devp, VDIN_OUTPUT_TO_MIF);

	if (vdin_dbg_en) {
		pr_info("vdin.%d: change afbce_mode %d->%d\n",
			devp->index, devp->afbce_mode_pre, devp->afbce_mode);
	}
	devp->afbce_mode_pre = devp->afbce_mode;
}

