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
 * header nembers = block nembers = 1920 * 1080 / (32 * 4)
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

/* fixed config mif by default */
void vdin_mif_config_init(struct vdin_dev_s *devp)
{
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

		W_VCBUS_BIT(AFBCE_ENABLE, 1, AFBCE_EN_BIT, AFBCE_EN_WID);
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

		if (devp->afbce_flag & VDIN_AFBCE_EN_LOOSY)
			rdma_write_reg(devp->rdma_handle, AFBCE_QUANT_ENABLE,
				       0xc11);
		rdma_write_reg_bits(devp->rdma_handle, AFBCE_ENABLE, 1, AFBCE_EN_BIT, AFBCE_EN_WID);
	}
}

bool vdin_chk_is_comb_mode(struct vdin_dev_s *devp)
{
	enum vdin_format_convert_e vdinout_fmt;
	int reg_fmt444_rgb_en = false;
	int reg_fmt444_comb = false;

	vdinout_fmt = devp->format_convert;
	if (vdinout_fmt == VDIN_FORMAT_CONVERT_YUV_RGB ||
	    vdinout_fmt == VDIN_FORMAT_CONVERT_YUV_GBR ||
	    vdinout_fmt == VDIN_FORMAT_CONVERT_YUV_BRG ||
	    vdinout_fmt == VDIN_FORMAT_CONVERT_RGB_RGB)
		reg_fmt444_rgb_en = true;

	if ((vdinout_fmt == VDIN_FORMAT_CONVERT_YUV_YUV444 ||
	     vdinout_fmt == VDIN_FORMAT_CONVERT_RGB_YUV444 || reg_fmt444_rgb_en) &&
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
	int sblk_num;
	int uncmp_bits;
	int uncmp_size;

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
		sblk_num = 12;
		break;
	case VDIN_FORMAT_CONVERT_YUV_YUV422:
	case VDIN_FORMAT_CONVERT_RGB_YUV422:
	case VDIN_FORMAT_CONVERT_GBR_YUV422:
	case VDIN_FORMAT_CONVERT_BRG_YUV422:
		reg_format_mode = 1;
		sblk_num = 16;
		break;
	default:
		reg_format_mode = 0;
		sblk_num = 24;
		break;
	}
	uncmp_bits = devp->source_bitdepth;

	/* bit size of uncompression mode */
	uncmp_size = (((((16 * uncmp_bits * sblk_num) + 7) >> 3) + 31)
		      / 32) << 1;
	rdma_write_reg(devp->rdma_handle, AFBCE_MODE,
		       (0 & 0x7) << 29 | (0 & 0x3) << 26 | (3 & 0x3) << 24 |
		       (hold_line_num & 0x7f) << 16 |
		       (2 & 0x3) << 14 | (reg_fmt444_comb & 0x1));

	rdma_write_reg_bits(devp->rdma_handle,
			    AFBCE_MIF_SIZE,
			    (uncmp_size & 0x1fff), 16, 5);/* uncmp_size */

	rdma_write_reg(devp->rdma_handle, AFBCE_FORMAT,
		       (reg_format_mode  & 0x3) << 8 |
		       (uncmp_bits & 0xf) << 4 |
		       (uncmp_bits & 0xf));
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
	int sblk_num;
	int uncmp_bits;
	int uncmp_size;
	int def_color_0 = 0x3ff;
	int def_color_1 = 0x80;
	int def_color_2 = 0x80;
	int def_color_3 = 0;
	int hblksize_out = (devp->h_active + 31) >> 5;
	int vblksize_out = (devp->v_active + 3)  >> 2;
	int blk_out_end_h;//output blk scope
	int blk_out_bgn_h;//output blk scope
	int blk_out_end_v;//output blk scope
	int blk_out_bgn_v;//output blk scope
	int enc_win_bgn_h;//input scope
	int enc_win_end_h;//input scope
	int enc_win_bgn_v;//input scope
	int enc_win_end_v;//input scope
	int reg_fmt444_rgb_en = 0;
	enum vdin_format_convert_e vdinout_fmt;
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

	vdinout_fmt = devp->format_convert;
	if (vdinout_fmt == VDIN_FORMAT_CONVERT_YUV_RGB ||
	    vdinout_fmt == VDIN_FORMAT_CONVERT_YUV_GBR ||
	    vdinout_fmt == VDIN_FORMAT_CONVERT_YUV_BRG ||
	    vdinout_fmt == VDIN_FORMAT_CONVERT_RGB_RGB)
		reg_fmt444_rgb_en = 1;

	reg_fmt444_comb = vdin_chk_is_comb_mode(devp);
	if (vdinout_fmt == VDIN_FORMAT_CONVERT_YUV_NV12 ||
	    vdinout_fmt == VDIN_FORMAT_CONVERT_YUV_NV21 ||
	    vdinout_fmt == VDIN_FORMAT_CONVERT_RGB_NV12 ||
	    vdinout_fmt == VDIN_FORMAT_CONVERT_RGB_NV21) {
		reg_format_mode = 2;/*420*/
	    sblk_num = 12;
	} else if ((vdinout_fmt == VDIN_FORMAT_CONVERT_YUV_YUV422) ||
		(vdinout_fmt == VDIN_FORMAT_CONVERT_RGB_YUV422) ||
		(vdinout_fmt == VDIN_FORMAT_CONVERT_GBR_YUV422) ||
		(vdinout_fmt == VDIN_FORMAT_CONVERT_BRG_YUV422)) {
		reg_format_mode = 1;/*422*/
	    sblk_num = 16;
	} else {
		reg_format_mode = 0;/*444*/
	    sblk_num = 24;
	}

	uncmp_bits = devp->source_bitdepth;

	//bit size of uncompression mode
	uncmp_size = (((((16 * uncmp_bits * sblk_num) + 7) >> 3) + 31)
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

	W_VCBUS_BIT(AFBCE_QUANT_ENABLE, (lossy_luma_en & 0x1), 0, 1);//loosy
	W_VCBUS_BIT(AFBCE_QUANT_ENABLE, (lossy_chrm_en & 0x1), 4, 1);//loosy

	if (devp->afbce_flag & VDIN_AFBCE_EN_LOOSY) {
		W_VCBUS(AFBCE_QUANT_ENABLE, 0xc11);
		pr_info("afbce use lossy compression mode\n");
	}

	W_VCBUS(AFBCE_SIZE_IN,
		((devp->h_active & 0x1fff) << 16) |  // hsize_in of afbc input
		((devp->v_active & 0x1fff) << 0)    // vsize_in of afbc input
		);

	W_VCBUS(AFBCE_BLK_SIZE_IN,
		((hblksize_out & 0x1fff) << 16) |  // out blk hsize
		((vblksize_out & 0x1fff) << 0)	  // out blk vsize
		);

	//head addr of compressed data
	if (devp->dtdata->hw_ver >= VDIN_HW_T7)
		W_VCBUS(AFBCE_HEAD_BADDR,
			devp->afbce_info->fm_head_paddr[0] >> 4);
	else
		W_VCBUS(AFBCE_HEAD_BADDR,
			devp->afbce_info->fm_head_paddr[0]);

	W_VCBUS_BIT(AFBCE_MIF_SIZE, (uncmp_size & 0x1fff), 16, 5);//uncmp_size

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
		((blk_out_bgn_h & 0x3ff) << 16) |  // scope of out blk hsize
		((blk_out_end_h & 0xfff) << 0)	  // scope of out blk vsize
		);

	W_VCBUS(AFBCE_MIF_VER_SCOPE,
		((blk_out_bgn_v & 0x3ff) << 16) |  // scope of out blk hsize
		((blk_out_end_v & 0xfff) << 0)	  // scope of out blk vsize
		);

	W_VCBUS(AFBCE_FORMAT,
		(reg_format_mode  & 0x3) << 8 |
		(uncmp_bits & 0xf) << 4 |
		(uncmp_bits & 0xf));

	W_VCBUS(AFBCE_DEFCOLOR_1,
		((def_color_3 & 0xfff) << 12) |  // def_color_a
		((def_color_0 & 0xfff) << 0)	// def_color_y
		);

	if (devp->source_bitdepth >= VDIN_COLOR_DEEPS_8BIT &&
	    devp->source_bitdepth <= VDIN_COLOR_DEEPS_12BIT)
		bit_mode_shift = devp->source_bitdepth - VDIN_COLOR_DEEPS_8BIT;
	/*def_color_v*/
	/*def_color_u*/
	W_VCBUS(AFBCE_DEFCOLOR_2,
		(((def_color_2 << bit_mode_shift) & 0xfff) << 12) |
		(((def_color_1 << bit_mode_shift) & 0xfff) << 0));
	if (devp->dtdata->hw_ver >= VDIN_HW_T7)
		W_VCBUS_BIT(AFBCE_MMU_RMIF_CTRL4,
			    devp->afbce_info->table_paddr >> 4, 0, 32);
	else
		W_VCBUS_BIT(AFBCE_MMU_RMIF_CTRL4,
			    devp->afbce_info->table_paddr, 0, 32);

	W_VCBUS_BIT(AFBCE_MMU_RMIF_SCOPE_X, cur_mmu_used, 0, 12);
	/*for almost uncompressed pattern,garbage at bottom
	 *(h_active * v_active * bytes per pixel + 3M) / page_size - 1
	 *where 3M is the rest bytes of block,since every block must not be\
	 *separated by 2 pages
	 */
	W_VCBUS_BIT(AFBCE_MMU_RMIF_SCOPE_X, 0x1c4f, 16, 13);

	W_VCBUS_BIT(AFBCE_ENABLE, 1, AFBCE_WORK_MD_BIT, AFBCE_WORK_MD_WID);

	if (devp->double_wr)
		W_VCBUS_BIT(AFBCE_ENABLE, 1, AFBCE_EN_BIT, AFBCE_EN_WID);
	else
		W_VCBUS_BIT(AFBCE_ENABLE, 0, AFBCE_EN_BIT, AFBCE_EN_WID);
}

void vdin_afbce_maptable_init(struct vdin_dev_s *devp)
{
	unsigned int i, j;
	unsigned int highmem_flag = 0;
	unsigned long ptable = 0;
	unsigned int *vtable = NULL;
	unsigned int body;
	unsigned int size;
	void *p = NULL;

	if (!devp->afbce_info)
		return;

	size = roundup(devp->afbce_info->frame_body_size, 4096);

	ptable = devp->afbce_info->fm_table_paddr[0];
	if (devp->cma_config_flag == 0x101)
		highmem_flag = PageHighMem(phys_to_page(ptable));
	else
		highmem_flag = PageHighMem(phys_to_page(ptable));

	for (i = 0; i < devp->vfmem_max_cnt; i++) {
		ptable = devp->afbce_info->fm_table_paddr[i];
		if (highmem_flag == 0) {
			if (devp->cma_config_flag == 0x101)
				vtable = codec_mm_phys_to_virt(ptable);
			else if (devp->cma_config_flag == 0)
				vtable = phys_to_virt(ptable);
			else
				vtable = phys_to_virt(ptable);
		} else {
			vtable = (unsigned int *)vdin_vmap(ptable,
				devp->afbce_info->frame_table_size);
			if (vdin_dbg_en) {
				pr_err("----vdin vmap v: %p, p: %lx, size: %d\n",
				       vtable, ptable,
				       devp->afbce_info->frame_table_size);
			}
			if (!vtable) {
				pr_err("vmap fail, size: %d.\n",
				       devp->afbce_info->frame_table_size);
				return;
			}
		}

		p = vtable;
		body = devp->afbce_info->fm_body_paddr[i] & 0xffffffff;
		for (j = 0; j < size; j += 4096) {
			*vtable = ((j + body) >> 12) & 0x000fffff;
			vtable++;
		}

		/* clean tail data. */
		memset(vtable, 0, devp->afbce_info->frame_table_size -
		       ((char *)vtable - (char *)p));

		vdin_dma_flush(devp, p,
			       devp->afbce_info->frame_table_size,
			       DMA_TO_DEVICE);

		if (highmem_flag)
			vdin_unmap_phyaddr(p);

		vtable = NULL;
	}
}

void vdin_afbce_set_next_frame(struct vdin_dev_s *devp,
			       unsigned int rdma_enable, struct vf_entry *vfe)
{
	unsigned char i;

	if (!devp->afbce_info)
		return;

	i = vfe->af_num;
	vfe->vf.compHeadAddr = devp->afbce_info->fm_head_paddr[i];
	vfe->vf.compBodyAddr = devp->afbce_info->fm_body_paddr[i];

#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
	if (rdma_enable) {
		if (devp->dtdata->hw_ver >= VDIN_HW_T7) {
			rdma_write_reg(devp->rdma_handle, AFBCE_HEAD_BADDR,
				       devp->afbce_info->fm_head_paddr[i] >> 4);
			rdma_write_reg_bits(devp->rdma_handle,
					    AFBCE_MMU_RMIF_CTRL4,
					    devp->afbce_info->fm_table_paddr[i] >> 4,
					    0, 32);
		} else {
			rdma_write_reg(devp->rdma_handle, AFBCE_HEAD_BADDR,
				       devp->afbce_info->fm_head_paddr[i]);
			rdma_write_reg_bits(devp->rdma_handle,
					    AFBCE_MMU_RMIF_CTRL4,
					    devp->afbce_info->fm_table_paddr[i],
					    0, 32);
		}
		rdma_write_reg_bits(devp->rdma_handle, AFBCE_ENABLE, 1,
				    AFBCE_START_PULSE_BIT, AFBCE_START_PULSE_WID);

		if (devp->pause_dec)
			rdma_write_reg_bits(devp->rdma_handle, AFBCE_ENABLE, 0,
					    AFBCE_EN_BIT, AFBCE_EN_WID);
		else
			rdma_write_reg_bits(devp->rdma_handle, AFBCE_ENABLE, 1,
					    AFBCE_EN_BIT, AFBCE_EN_WID);
	}
#endif
	vdin_afbce_clear_writedown_flag(devp);
}

void vdin_pause_afbce_write(struct vdin_dev_s *devp, unsigned int rdma_enable)
{
#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
	if (rdma_enable)
		rdma_write_reg_bits(devp->rdma_handle, AFBCE_ENABLE, 0,
				    AFBCE_EN_BIT, AFBCE_EN_WID);
#endif
	vdin_afbce_clear_writedown_flag(devp);
}

void vdin_afbce_clear_writedown_flag(struct vdin_dev_s *devp)
{
	rdma_write_reg(devp->rdma_handle, AFBCE_CLR_FLAG, 1);
}

/* return 1: write down*/
int vdin_afbce_read_writedown_flag(void)
{
	int val1, val2;

	val1 = rd_bits(0, AFBCE_STA_FLAGT, 0, 1);
	val2 = rd_bits(0, AFBCE_STA_FLAGT, 2, 2);

	if (val1 == 1 || val2 == 0)
		return 1;
	else
		return 0;
}

void vdin_afbce_soft_reset(void)
{
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
		if (devp->h_active > 1920 && devp->v_active > 1080) {
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

	/* default non-afbce mode
	 * switch to afbce_mode if need by vpp notify
	 */
	devp->afbce_mode = 0;
	devp->afbce_mode_pre = devp->afbce_mode;
	pr_info("vdin%d init afbce_mode: %d\n", devp->index, devp->afbce_mode);
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

