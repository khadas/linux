// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * drivers/amlogic/media/video_processor/common/vicp/vicp_process.c
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

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

#ifdef CONFIG_AMLOGIC_MEDIA_CODEC_MM
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#endif

#include "vicp_log.h"
#include "vicp_reg.h"
#include "vicp_hardware.h"
#include "vicp_process.h"

#define IE_BW      10
#define FLTZERO 0xfc000
#define VD1_S0_HDR2_MAT 0
#define VD1_S1_HDR2_MAT 1
#define VD1_S2_HDR2_MAT 2
#define VD1_S3_HDR2_MAT 3
#define VD2_HDR2_MAT    4

#define ZOOM_BITS       20
#define PHASE_BITS      16

#define VID_CMPR_PROC_DONE        (10 * 2 + 11)
#define VID_CMPR_DMA_DONE         (10 * 32 + 10)
#define VID_CMPR_ERROR            (10 * 32 + 9)

static int pps_lut_tap8[33][8] = {{0, 0, 0, 128, 0, 0, 0, 0},
			{-1, 1, 0, 127, 2, -1, 1, -1},
			{-1, 2, -2, 127, 4, -2, 1, -1},
			{-2, 3,  -4, 127,  6, -3, 2, -1},
			{-3, 4,  -7, 127, 10, -4, 3, -2},
			{-3, 5,  -9, 127, 12, -5, 3, -2},
			{-4, 6, -11, 127, 15, -6, 4, -3},
			{-4, 7, -13, 127, 16, -7, 5, -3},
			{-5, 7, -14, 127, 20, -8, 5, -4},
			{-5, 8, -16, 127, 21, -9, 6, -4},
			{-6, 9, -17, 127, 24, -11, 7, -5},
			{-6, 10, -18, 126, 26, -12, 7, -5},
			{-7, 10, -20, 127, 29, -13, 8, -6},
			{-7, 11, -21, 124, 32, -14, 9, -6},
			{-8, 12, -22, 124, 35, -15, 9, -7},
			{-8, 12, -23, 123, 37, -16, 10, -7},
			{-9, 13, -24, 121, 40, -17, 11, -7},
			{-9, 14, -25, 120, 43, -18, 11, -8},
			{-10, 14, -26, 119, 46, -19, 12, -8},
			{-10, 15, -27, 118, 49, -20, 12, -9},
			{-10, 15, -27, 115, 52, -21, 13, -9},
			{-10, 15, -28, 114, 55, -22, 13, -9},
			{-11, 16, -28, 112, 58, -23, 14, -10},
			{-11, 16, -29, 111, 61, -24, 14, -10},
			{-11, 16, -29, 107, 64, -24, 15, -10},
			{-11, 17, -29, 104, 67, -25, 15, -10},
			{-11, 17, -29, 103, 70, -26, 15, -11},
			{-12, 17, -29, 100, 73, -26, 16, -11},
			{-12, 17, -29, 98, 76, -27, 16, -11},
			{-12, 17, -29, 96, 79, -28, 16, -11},
			{-12, 17, -29, 92, 82, -28, 17, -11},
			{-12, 17, -29, 90, 85, -29, 17, -11},
			{-12, 17, -29, 88, 88, -29, 17, -12}};
static int pps_lut_tap8_s11[33][8] = {{0, 0, 0, 512, 0, 0, 0, 0},
			{-1, 3, -7, 512, 7, -3, 1, 0},
			{-2, 5, -14, 511, 15, -5, 2, 0},
			{-2, 7, -20, 510, 23, -8, 3, -1},
			{-3, 10, -27, 509, 31, -11, 3, 0},
			{-4, 12, -32, 507, 39, -14, 4, 0},
			{-4, 14, -38, 504, 48, -17, 5, 0},
			{-5, 16, -43, 501, 57, -19, 6, -1},
			{-5, 18, -49, 498, 66, -22, 7, -1},
			{-6, 19, -53, 495, 75, -26, 8, 0},
			{-6, 21, -58, 490, 85, -29, 10, -1},
			{-6, 22, -62, 486, 94, -32, 11, -1},
			{-7, 24, -66, 481, 104, -35, 12, -1},
			{-7, 25, -69, 476, 114, -38, 13, -2},
			{-7, 26, -72, 470, 124, -41, 14, -2},
			{-8, 27, -75, 464, 134, -44, 15, -1},
			{-8, 28, -78, 458, 145, -47, 16, -2},
			{-8, 29, -80, 451, 155, -50, 17, -2},
			{-8, 30, -83, 444, 166, -53, 18, -2},
			{-8, 31, -84, 437, 177, -56, 19, -4},
			{-8, 31, -86, 429, 188, -59, 20, -3},
			{-9, 32, -87, 421, 199, -62, 22, -4},
			{-8, 32, -88, 413, 209, -64, 23, -5},
			{-8, 32, -89, 405, 220, -67, 24, -5},
			{-9, 33, -89, 396, 231, -69, 25, -6},
			{-8, 33, -90, 387, 242, -72, 25, -5},
			{-8, 33, -90, 377, 253, -74, 26, -5},
			{-8, 32, -89, 368, 264, -76, 27, -6},
			{-8, 32, -89, 358, 275, -78, 28, -6},
			{-8, 32, -88, 348, 286, -80, 29, -7},
			{-7, 32, -88, 338, 297, -82, 29, -7},
			{-7, 31, -86, 328, 307, -84, 30, -7},
			{-8, 31, -85, 318, 318, -85, 31, -8}};

static int pps_lut_tap4_s11[33][4] =  {{0, 512, 0, 0},
			{-5, 512, 5, 0},
			{-10, 511, 11, 0},
			{-14, 510, 17, -1},
			{-18, 508, 23, -1},
			{-22, 506, 29, -1},
			{-25, 503, 36, -2},
			{-28, 500, 43, -3},
			{-32, 496, 51, -3},
			{-34, 491, 59, -4},
			{-37, 487, 67, -5},
			{-39, 482, 75, -6},
			{-41, 476, 84, -7},
			{-42, 470, 92, -8},
			{-44, 463, 102, -9},
			{-45, 456, 111, -10},
			{-45, 449, 120, -12},
			{-47, 442, 130, -13},
			{-47, 434, 140, -15},
			{-47, 425, 151, -17},
			{-47, 416, 161, -18},
			{-47, 407, 172, -20},
			{-47, 398, 182, -21},
			{-47, 389, 193, -23},
			{-46, 379, 204, -25},
			{-45, 369, 215, -27},
			{-44, 358, 226, -28},
			{-43, 348, 237, -30},
			{-43, 337, 249, -31},
			{-41, 326, 260, -33},
			{-40, 316, 271, -35},
			{-39, 305, 282, -36},
			{-37, 293, 293, -37}};

struct completion vicp_isr_done;
static bool is_rdma_enable;
static int current_dump_flag;

irqreturn_t vicp_isr_handle(int irq, void *dev_id)
{
	complete(&vicp_isr_done);
	vicp_print(VICP_INFO, "vicp: isr\n");
	return IRQ_HANDLED;
}

void set_vid_cmpr_shrink(int is_enable, int size, int mode_h, int mode_v)
{
	if (print_flag & VICP_SHRINK) {
		pr_info("vicp: ##########shrink config##########\n");
		pr_info("vicp: shrink_en = %d.\n", is_enable);
		pr_info("vicp: shrink_size = %d.\n", size);
		pr_info("vicp: shrink_mode: h %d, v %d.\n", mode_h, mode_v);
		pr_info("vicp: #################################.\n");
	};

	set_wrmif_shrk_enable(is_enable);
	set_wrmif_shrk_size(size);
	set_wrmif_shrk_mode_h(mode_h);
	set_wrmif_shrk_mode_v(mode_v);
}

void set_vid_cmpr_afbce(int enable, struct vid_cmpr_afbce_t *afbce)
{
	int hold_line_num = 0;

	int hsize_buf = afbce->reg_pip_mode ? afbce->hsize_bgnd : afbce->hsize_in;
	int vsize_buf = afbce->reg_pip_mode ? afbce->vsize_bgnd : afbce->vsize_in;
	int hblksize_buf = (hsize_buf + 31) >> 5;
	int vblksize_buf = (vsize_buf + 3) >> 2;
	int blk_out_bgn_h = afbce->enc_win_bgn_h >> 5;
	int blk_out_end_h = (afbce->enc_win_end_h + 31) >> 5;
	int blk_out_bgn_v = afbce->enc_win_bgn_v >> 2;
	int blk_out_end_v = (afbce->enc_win_end_v + 3) >> 2;
	int lossy_luma_en;
	int lossy_chrm_en;
	int reg_fmt444_comb;
	int uncmp_size;
	int uncmp_bits;
	int sblk_num;
	struct vicp_afbce_mode_reg_t afbce_mode_reg;
	struct vicp_afbce_color_format_reg_t color_format_reg;
	struct vicp_afbce_mmu_rmif_control1_reg_t rmif_control1;
	struct vicp_afbce_mmu_rmif_control3_reg_t rmif_control3;
	struct vicp_afbce_pip_control_reg_t pip_control;
	struct vicp_afbce_enable_reg_t enable_reg;

	if (IS_ERR_OR_NULL(afbce)) {
		vicp_print(VICP_ERROR, "%s: invalid param,return.\n", __func__);
		return;
	}

	if (print_flag & VICP_AFBCE) {
		pr_info("vicp: ##########fbc_out config##########\n");
		pr_info("vicp: headaddr: 0x%llx, 0x%llx.\n", afbce->head_baddr, afbce->table_baddr);
		pr_info("vicp: pip: init_flag=%d, mode=%d.\n", afbce->reg_init_ctrl,
			afbce->reg_pip_mode);
		pr_info("vicp: reg_ram_comb %d.\n", afbce->reg_ram_comb);
		pr_info("vicp: output_format_mode %d.\n", afbce->reg_format_mode);
		pr_info("vicp: compbit: y %d, c %d.\n", afbce->reg_compbits_y,
			afbce->reg_compbits_c);
		pr_info("vicp: input_size: h %d, v %d.\n", afbce->hsize_in, afbce->vsize_in);
		pr_info("vicp: inputbuf_size: h %d, v %d.\n", afbce->hsize_bgnd, afbce->vsize_bgnd);
		pr_info("vicp: output axis: %d %d %d %d.\n", afbce->enc_win_bgn_h,
			afbce->enc_win_end_h, afbce->enc_win_bgn_v, afbce->enc_win_end_v);
		pr_info("vicp: loosy_mode %d, rev_mode %d.\n", afbce->loosy_mode, afbce->rev_mode);
		pr_info("vicp: default color:%d %d %d %d.\n", afbce->def_color_0,
			afbce->def_color_1, afbce->def_color_2, afbce->def_color_3);
		pr_info("vicp: force_444_comb = %d\n", afbce->force_444_comb);
		pr_info("vicp: rotation enable %d\n", afbce->rot_en);
		pr_info("vicp: din_swt = %d\n", afbce->din_swt);
		pr_info("vicp: #################################.\n");
	};

	vicp_print(VICP_INFO, "%s: enable = %d.\n", __func__, enable);
	if (!enable) {
		vicp_print(VICP_INFO, "%s: afbce disable.\n", __func__);
		vicp_reg_set_bits(VID_CMPR_AFBCE_ENABLE, 0, 8, 1);
		return;
	}

	if (afbce->force_444_comb != 0 && afbce->reg_format_mode == 0)
		reg_fmt444_comb = 1;
	else
		reg_fmt444_comb = 0;

	if (afbce->reg_format_mode == 1)
		sblk_num = 16;
	else if (afbce->reg_format_mode == 2)
		sblk_num = 12;
	else
		sblk_num = 24;

	if (afbce->reg_compbits_y > afbce->reg_compbits_c)
		uncmp_bits = afbce->reg_compbits_y;
	else
		uncmp_bits = afbce->reg_compbits_c;

	uncmp_size = (((((16 * uncmp_bits * sblk_num) + 7) >> 3) + 31) / 32) << 1;

	if (afbce->loosy_mode == 0) {
		lossy_luma_en = 0;
		lossy_chrm_en = 0;
	} else if (afbce->loosy_mode == 1) {
		lossy_luma_en = 1;
		lossy_chrm_en = 0;
	} else if (afbce->loosy_mode == 2) {
		lossy_luma_en = 0;
		lossy_chrm_en = 1;
	} else {
		lossy_luma_en = 1;
		lossy_chrm_en = 1;
	}

	if (is_rdma_enable)
		hold_line_num = 1;
	else
		hold_line_num = 2;

	memset(&afbce_mode_reg, 0, sizeof(struct vicp_afbce_mode_reg_t));
	afbce_mode_reg.soft_rst = 0;
	afbce_mode_reg.rev_mode = afbce->rev_mode;
	afbce_mode_reg.mif_urgent = 3;
	afbce_mode_reg.hold_line_num = hold_line_num;
	afbce_mode_reg.burst_mode = 2;
	afbce_mode_reg.fmt444_comb = reg_fmt444_comb;
	set_afbce_mode(afbce_mode_reg);

	set_afbce_lossy_luma_enable(lossy_luma_en);
	set_afbce_lossy_chrm_enable(lossy_chrm_en);
	set_afbce_input_size(hsize_buf, vsize_buf);
	set_afbce_blk_size(hblksize_buf, vblksize_buf);

	vicp_print(VICP_INFO, "%s: head_baddr = 0x%lx.\n", __func__, afbce->head_baddr);
	set_afbce_head_addr(afbce->head_baddr);
	set_afbce_mif_size(uncmp_size);
	set_afbce_pixel_in_scope(1, afbce->enc_win_bgn_h, afbce->enc_win_end_h);
	set_afbce_pixel_in_scope(0, afbce->enc_win_bgn_v, afbce->enc_win_end_v);
	set_afbce_conv_control(2048, 256);
	set_afbce_mix_scope(1, blk_out_bgn_h, blk_out_end_h);
	set_afbce_mix_scope(0, blk_out_bgn_v, blk_out_end_v);

	memset(&color_format_reg, 0, sizeof(struct vicp_afbce_color_format_reg_t));
	color_format_reg.format_mode = afbce->reg_format_mode;
	color_format_reg.compbits_c = afbce->reg_compbits_c;
	color_format_reg.compbits_y = afbce->reg_compbits_y;
	set_afbce_colorfmt(color_format_reg);

	set_afbce_default_color1(afbce->def_color_3, afbce->def_color_0);
	set_afbce_default_color2(afbce->def_color_1, afbce->def_color_2);
	set_afbce_mmu_rmif_control4(afbce->table_baddr);

	memset(&rmif_control1, 0, sizeof(struct vicp_afbce_mmu_rmif_control1_reg_t));
	rmif_control1.cmd_intr_len = 1;
	rmif_control1.cmd_req_size = 1;
	rmif_control1.burst_len = 2;
	rmif_control1.little_endian = 1;
	rmif_control1.pack_mode = 3;
	set_afbce_mmu_rmif_control1(rmif_control1);

	set_afbce_mmu_rmif_scope(1, 0, 0x1ffe);
	set_afbce_mmu_rmif_scope(0, 0, 1);

	memset(&rmif_control3, 0, sizeof(struct vicp_afbce_mmu_rmif_control3_reg_t));
	rmif_control3.vstep = 1;
	rmif_control3.acc_mode = 1;
	rmif_control3.stride = 0x1fff;
	set_afbce_mmu_rmif_control3(rmif_control3);

	vicp_print(VICP_INFO, "%s: init_ctrl = %d, pip_mode = %d.\n",
		__func__, afbce->reg_init_ctrl, afbce->reg_pip_mode);
	memset(&pip_control, 0, sizeof(struct vicp_afbce_pip_control_reg_t));
	pip_control.enc_align_en = 1;
	pip_control.pip_ini_ctrl = afbce->reg_init_ctrl;
	pip_control.pip_mode = afbce->reg_pip_mode;
	set_afbce_pip_control(pip_control);

	set_afbce_rotation_control(afbce->rot_en, 8);

	memset(&enable_reg, 0, sizeof(struct vicp_afbce_enable_reg_t));
	enable_reg.pls_enc_frm_start = enable;
	enable_reg.enc_enable = enable;
	set_afbce_enable(enable_reg);
}

void f2v_get_vertical_phase(unsigned int zoom_ratio, enum f2v_vphase_type_e type,
	unsigned char bank_length, struct vid_cmpr_f2v_vphase_t *vphase)
{
	int offset_in, offset_out;
	unsigned char f2v_420_in_pos_luma[F2V_TYPE_MAX] = {0, 2, 0, 2, 0, 0, 0, 2, 0};
	unsigned char f2v_420_out_pos[F2V_TYPE_MAX] = {0, 2, 2, 0, 0, 2, 0, 0, 0};

	offset_in = f2v_420_in_pos_luma[type] << PHASE_BITS;
	offset_out = (f2v_420_out_pos[type] * zoom_ratio) >> (ZOOM_BITS - PHASE_BITS);

	vphase->rcv_num = bank_length;
	if (bank_length == 4 || bank_length == 3)
		vphase->rpt_num = 1;
	else
		vphase->rpt_num = 0;

	if (offset_in > offset_out) {
		vphase->rpt_num = vphase->rpt_num + 1;
		vphase->phase = ((4 << PHASE_BITS) + offset_out - offset_in) >> 2;
	} else {
		while ((offset_in + (4 << PHASE_BITS)) <= offset_out) {
			if (vphase->rpt_num == 1)
				vphase->rpt_num = 0;
			else
				vphase->rcv_num++;
			offset_in += 4 << PHASE_BITS;
		}
			vphase->phase = (offset_out - offset_in) >> 2;
	}
}

void set_vid_cmpr_hdr2(enum hdr2_sel_e hdr2_sel, int hdr2_top_en, int hdr2_only_mat,
	int hdr2_fmt_cfg, int in_fmt, int rgb_out_en)
{
	if (hdr2_sel != VID_CMPR_HDR2) {
		vicp_print(VICP_ERROR, "Not HDR2 compser!!!\n");
		return;
	}

	if (print_flag & VICP_HDR) {
		pr_info("vicp: ##########hdr config##########\n");
		pr_info("vicp: hdr_en = %d.\n", hdr2_top_en);
		pr_info("vicp: hdr2_only_mat = %d.\n", hdr2_only_mat);
		pr_info("vicp: hdr2_fmt_cfg = %d.\n", hdr2_fmt_cfg);
		pr_info("vicp: input_fmt = %d.\n", in_fmt);
		pr_info("vicp: rgb_out_en = %d.\n", rgb_out_en);
		pr_info("vicp: #################################.\n");
	};

	vicp_reg_set_bits(VID_CMPR_HDR2_CTRL, hdr2_top_en, 13, 1);
	vicp_reg_set_bits(VID_CMPR_HDR2_CTRL, hdr2_only_mat, 16, 1);
}

static int get_presc_out_size(int presc_en, int presc_rate, int src_size)
{
	int size = 0;

	if (presc_en) {
		if (presc_rate == 1)
			size = (src_size + 1) >> 1;
		else if (presc_rate == 2)
			size = (src_size + 3) >> 2;
		else
			size = src_size;
	} else {
		size = src_size;
	}

	return size;
}

static int get_phase_step(int presc_size, int dst_size)
{
	int step = 0;

	if (presc_size > 2048)
		step = ((presc_size << 18) / dst_size) << 2;
	else
		step = (presc_size << 20) / dst_size;

	step = (step << 4);

	return step;
}

void set_vid_cmpr_scale(int is_enable, struct vid_cmpr_scaler_t *scaler)
{
	enum f2v_vphase_type_e top_conv_type = F2V_P2P;
	enum f2v_vphase_type_e bot_conv_type = F2V_P2P;
	struct vid_cmpr_f2v_vphase_t vphase;
	int topbot_conv;
	int top_conv, bot_conv;

	int i = 0;
	int hsc_en = 0;
	int vsc_en = 0;
	int vsc_double_line_mode = 0;
	int coef_s_bits = 0;
	u32 p_src_w, p_src_h;
	u32 vert_phase_step, horz_phase_step;
	unsigned char top_rcv_num, bot_rcv_num;
	unsigned char top_rpt_num, bot_rpt_num;
	unsigned short top_vphase, bot_vphase;
	unsigned char is_frame;
	int blank_len;
	/*0:vd1_s0 1:vd1_s1 2:vd1_s2 3:vd1_s3 4:vd2 5:vid_cmp 6:RESHAPE*/
	int index = 5;

	if (IS_ERR_OR_NULL(scaler)) {
		vicp_print(VICP_ERROR, "%s: invalid param,return.\n", __func__);
		return;
	}

	if (print_flag & VICP_SCALER) {
		pr_info("vicp: ##########scaler config##########\n");
		pr_info("vicp: in_size: h=%d, v=%d.\n", scaler->din_hsize, scaler->din_vsize);
		pr_info("vicp: out_size: h=%d, v=%d.\n", scaler->dout_hsize, scaler->dout_vsize);
		pr_info("vicp: vert_bank_length = %d.\n", scaler->vert_bank_length);
		pr_info("vicp: pre_scaler_en: h=%d, v=%d.\n", scaler->prehsc_en,
			scaler->prevsc_en);
		pr_info("vicp: pre_scaler_rate: h=%d, v=%d.\n", scaler->prehsc_rate,
			scaler->prevsc_rate);
		pr_info("vicp: high_res_coef_en = %d.\n", scaler->high_res_coef_en);
		pr_info("vicp: phase_step: h=%d, v=%d.\n", scaler->horz_phase_step,
			scaler->vert_phase_step);
		pr_info("vicp: slice_start = %d\n", scaler->slice_x_st);
		pr_info("vicp: slice_end: %d %d %d %d.\n",
			scaler->slice_x_end[0],
			scaler->slice_x_end[1],
			scaler->slice_x_end[2],
			scaler->slice_x_end[3]);
		pr_info("vicp: pps_slice = %d..\n", scaler->pps_slice);
		pr_info("vicp: phase_step_en=%d., step=%d.\n", scaler->phase_step_en,
			scaler->phase_step);
		pr_info("vicp: #################################.\n");
	};

	topbot_conv = index >> 16;
	top_conv = (topbot_conv >> 4) & 0xf;
	bot_conv = topbot_conv & 0xf;
	index = index & 0xffff;

	if (top_conv != 0)
		top_conv_type = (enum f2v_vphase_type_e)(top_conv - 1);
	if (bot_conv != 0)
		bot_conv_type = (enum f2v_vphase_type_e)(bot_conv - 1);

	vsc_double_line_mode = 0;

	if (scaler->din_hsize != scaler->dout_hsize)
		vsc_en = 1;
	if (scaler->din_vsize != scaler->dout_vsize)
		hsc_en = 1;

	p_src_h = get_presc_out_size(scaler->prevsc_en, scaler->prevsc_rate, scaler->din_vsize);
	vert_phase_step = get_phase_step(p_src_h, scaler->dout_vsize);

	p_src_w = get_presc_out_size(scaler->prehsc_en, scaler->prehsc_rate, scaler->din_hsize);
	if (scaler->phase_step_en)
		horz_phase_step = scaler->phase_step;
	else
		horz_phase_step = get_phase_step(p_src_w, scaler->dout_hsize);

	if (horz_phase_step == 0 || vert_phase_step == 0)
		vicp_print(VICP_ERROR,
			"%s: horz_phase_step or vert_phase_step should be set correctly!",
			__func__);

	if (scaler->high_res_coef_en)
		coef_s_bits = 9;
	else
		coef_s_bits = 7;

	vicp_reg_write(VID_CMPR_PRE_SCALE_CTRL,
			(8 << 21) |
			(8 << 25) |
			(scaler->high_res_coef_en << 20) |
			(coef_s_bits << 16) |
			(coef_s_bits << 12) |
			(4 << 7) |
			(4 << 4) |
			(scaler->prehsc_rate << 2) |
			(scaler->prevsc_rate << 0)
			);

	if (scaler->high_res_coef_en == 0) {
		vicp_reg_write(VID_CMPR_SCALE_COEF_IDX, 0x0100);
		for (i = 0; i < 33; i++)
			vicp_reg_write(VID_CMPR_SCALE_COEF,
				((pps_lut_tap8[i][0] & 0xff) << 24) |
				((pps_lut_tap8[i][1] & 0xff) << 16) |
				((pps_lut_tap8[i][2] & 0xff) << 8) |
				((pps_lut_tap8[i][3] & 0xff) << 0)
				);
		vicp_reg_write(VID_CMPR_SCALE_COEF_IDX, 0x0180);
		for (i = 0; i < 33; i++)
			vicp_reg_write(VID_CMPR_SCALE_COEF,
				((pps_lut_tap8[i][4] & 0xff) << 24) |
				((pps_lut_tap8[i][5] & 0xff) << 16) |
				((pps_lut_tap8[i][6] & 0xff) << 8) |
				((pps_lut_tap8[i][7] & 0xff) << 0)
				);
	} else {
		vicp_reg_write(VID_CMPR_SCALE_COEF_IDX, 0x0000);
		for (i = 0; i < 33; i++) {
			vicp_reg_write(VID_CMPR_SCALE_COEF,
				((pps_lut_tap4_s11[i][0] & 0x7ff) << 16) |
				((pps_lut_tap4_s11[i][1] & 0x7ff) << 0)
				);
			vicp_reg_write(VID_CMPR_SCALE_COEF,
				((pps_lut_tap4_s11[i][2] & 0x7ff) << 16) |
				((pps_lut_tap4_s11[i][3] & 0x7ff) << 0)
				);
		}

		vicp_reg_write(VID_CMPR_SCALE_COEF_IDX, 0x0100);
		for (i = 0; i < 33; i++) {
			vicp_reg_write(VID_CMPR_SCALE_COEF,
				((pps_lut_tap8_s11[i][0] & 0x7ff) << 16) |
				((pps_lut_tap8_s11[i][1] & 0x7ff) << 0)
				);
			vicp_reg_write(VID_CMPR_SCALE_COEF,
				((pps_lut_tap8_s11[i][2] & 0x7ff) << 16) |
				((pps_lut_tap8_s11[i][3] & 0x7ff) << 0)
				);
		}
		vicp_reg_write(VID_CMPR_SCALE_COEF_IDX, 0x0180);
		for (i = 0; i < 33; i++) {
			vicp_reg_write(VID_CMPR_SCALE_COEF,
				((pps_lut_tap8_s11[i][4] & 0x7ff) << 16) |
				((pps_lut_tap8_s11[i][5] & 0x7ff) << 0)
				);
			vicp_reg_write(VID_CMPR_SCALE_COEF,
				((pps_lut_tap8_s11[i][6] & 0x7ff) << 16) |
				((pps_lut_tap8_s11[i][7] & 0x7ff) << 0)
				);
		}
	}

	is_frame = (top_conv_type == F2V_IT2P) ||
		(top_conv_type == F2V_IB2P) ||
		(top_conv_type == F2V_P2P);

	blank_len = scaler->vert_bank_length;
	if (is_frame) {
		f2v_get_vertical_phase(vert_phase_step, top_conv_type, blank_len, &vphase);
		top_rcv_num = vphase.rcv_num;
		top_rpt_num = vphase.rpt_num;
		top_vphase  = vphase.phase;

		bot_rcv_num = 0;
		bot_rpt_num = 0;
		bot_vphase  = 0;
	} else {
		f2v_get_vertical_phase(vert_phase_step, top_conv_type, blank_len, &vphase);
		top_rcv_num = vphase.rcv_num;
		top_rpt_num = vphase.rpt_num;
		top_vphase = vphase.phase;

		f2v_get_vertical_phase(vert_phase_step, bot_conv_type, blank_len, &vphase);
		bot_rcv_num = vphase.rcv_num;
		bot_rpt_num = vphase.rpt_num;
		bot_vphase = vphase.phase;
	}

	vicp_reg_write(VID_CMPR_VSC_REGION12_STARTP, 0);
	vicp_reg_write(VID_CMPR_VSC_REGION34_STARTP,
		((scaler->dout_vsize << 16) | scaler->dout_vsize));
	vicp_reg_write(VID_CMPR_VSC_REGION4_ENDP, scaler->dout_vsize - 1);

	vicp_reg_write(VID_CMPR_VSC_START_PHASE_STEP, vert_phase_step);
	vicp_reg_write(VID_CMPR_VSC_REGION0_PHASE_SLOPE, 0);
	vicp_reg_write(VID_CMPR_VSC_REGION1_PHASE_SLOPE, 0);
	vicp_reg_write(VID_CMPR_VSC_REGION3_PHASE_SLOPE, 0);
	vicp_reg_write(VID_CMPR_VSC_REGION4_PHASE_SLOPE, 0);

	vicp_reg_write(VID_CMPR_VSC_PHASE_CTRL,
		((vsc_double_line_mode << 17) | (!is_frame) << 16) |
		(0 << 15) |
		(bot_rpt_num << 13) |
		(bot_rcv_num << 8) |
		(0 << 7) |
		(top_rpt_num << 5) |
		(top_rcv_num)
		);
	vicp_reg_write(VID_CMPR_VSC_INI_PHASE, (bot_vphase << 16) | top_vphase);
	vicp_reg_write(VID_CMPR_HSC_REGION12_STARTP, 0);
	vicp_reg_write(VID_CMPR_HSC_REGION34_STARTP,
		(scaler->dout_hsize << 16) | scaler->dout_hsize);
	vicp_reg_write(VID_CMPR_HSC_REGION4_ENDP, scaler->dout_hsize - 1);

	vicp_reg_write(VID_CMPR_HSC_START_PHASE_STEP, horz_phase_step);
	vicp_reg_write(VID_CMPR_HSC_REGION0_PHASE_SLOPE, 0);
	vicp_reg_write(VID_CMPR_HSC_REGION1_PHASE_SLOPE, 0);
	vicp_reg_write(VID_CMPR_HSC_REGION3_PHASE_SLOPE, 0);
	vicp_reg_write(VID_CMPR_HSC_REGION4_PHASE_SLOPE, 0);

	vicp_reg_write(VID_CMPR_HSC_PHASE_CTRL, (3 << 20) | (8 << 16) | 0);
	vicp_reg_write(VID_CMPR_SC_MISC,
		(scaler->prevsc_en << 24) |
		(scaler->prevsc_en << 21) |
		(scaler->prehsc_en << 20) |
		(scaler->prevsc_en << 19) |
		(vsc_en << 18) |
		(hsc_en << 17) |
		(is_enable << 16) |
		(1 << 15) |
		(0 << 12) |
		(8 << 8) |
		(0 << 5) |
		(0 << 4) |
		(scaler->vert_bank_length << 0)
		);
}

void set_vid_cmpr_afbcd(int hold_line_num, struct vid_cmpr_afbcd_t *afbcd)
{
	u32 compbits_yuv;
	u32 conv_lbuf_len;
	u32 dec_lbuf_depth;
	u32 mif_lbuf_depth;
	u32 mif_blk_bgn_h;
	u32 mif_blk_bgn_v;
	u32 mif_blk_end_h;
	u32 mif_blk_end_v;
	u32 dec_pixel_bgn_h;
	u32 dec_pixel_bgn_v;
	u32 dec_pixel_end_h;
	u32 dec_pixel_end_v;
	u32 dec_hsize_proc;
	u32 dec_vsize_proc;
	u32 out_end_dif_h;
	u32 out_end_dif_v;
	u32 rev_mode_h;
	u32 rev_mode_v;
	u32 fmt_size_h;
	u32 fmt_size_v;
	u32 hfmt_en = 0;
	u32 vfmt_en = 0;
	u32 uv_vsize_in = 0;
	u32 vt_yc_ratio = 0;
	u32 hz_yc_ratio = 0;
	u32 compbits_eq8;
	u32 use_4kram;
	u32 real_hsize_mt2k;
	u32 comp_mt_20bit;
	struct vicp_afbcd_mode_reg_t afbcd_mode;
	struct vicp_afbcd_general_reg_t afbcd_general;
	struct vicp_afbcd_cfmt_control_reg_t cfmt_control;
	struct vicp_afbcd_quant_control_reg_t quant_control;
	struct vicp_afbcd_rotate_control_reg_t rotate_control;
	struct vicp_afbcd_rotate_scope_reg_t rotate_scope;

	if (IS_ERR_OR_NULL(afbcd)) {
		vicp_print(VICP_ERROR, "%s: invalid param,return.\n", __func__);
		return;
	}

	if (print_flag & VICP_AFBCD) {
		pr_info("vicp: ##########fbc_in config##########\n");
		pr_info("vicp: blk_mem_mode = %d.\n", afbcd->blk_mem_mode);
		pr_info("vicp: index = %d.\n", afbcd->index);
		pr_info("vicp: size: h %d, v %d.\n", afbcd->hsize, afbcd->vsize);
		pr_info("vicp: addr: head 0x%x, body 0x%x.\n", afbcd->head_baddr,
			afbcd->body_baddr);
		pr_info("vicp: compbits: y%d, u%d, v%d.\n", afbcd->compbits_y, afbcd->compbits_u,
			afbcd->compbits_v);
		pr_info("vicp: input_color_format = %d.\n", afbcd->fmt_mode);
		pr_info("vicp: ddr_sz_mode = %d.\n", afbcd->ddr_sz_mode);
		pr_info("vicp: fmt444_comb = %d.\n", afbcd->fmt444_comb);
		pr_info("vicp: v_skip: y%d, uv%d.\n", afbcd->v_skip_y, afbcd->v_skip_uv);
		pr_info("vicp: h_skip: y%d, uv%d.\n", afbcd->h_skip_y, afbcd->h_skip_uv);
		pr_info("vicp: rev_mode %d.\n", afbcd->rev_mode);
		pr_info("vicp: lossy_en = %d.\n", afbcd->lossy_en);
		pr_info("vicp: default_color: y%d, u%d, v%d.\n", afbcd->def_color_y,
			afbcd->def_color_u, afbcd->def_color_v);
		pr_info("vicp: window_axis: %d %d %d %d.\n", afbcd->win_bgn_h, afbcd->win_end_h,
			afbcd->win_bgn_v, afbcd->win_end_v);
		pr_info("vicp: ini_phase: h%d, v%d.\n", afbcd->hz_ini_phase, afbcd->vt_ini_phase);
		pr_info("vicp: rpt_fst0_en: h%d, v%d.\n", afbcd->hz_rpt_fst0_en,
			afbcd->vt_rpt_fst0_en);
		pr_info("vicp: rotation_en = %d.\n", afbcd->rot_en);
		pr_info("vicp: rotation_begin: h%d, v%d.\n", afbcd->rot_hbgn, afbcd->rot_vbgn);
		pr_info("vicp: rotation_shrink: h%d, v%d.\n", afbcd->rot_hshrk, afbcd->rot_vshrk);
		pr_info("vicp: rotation_drop_mode = %d.\n", afbcd->rot_drop_mode);
		pr_info("vicp: rotation_output_format = %d.\n", afbcd->rot_ofmt_mode);
		pr_info("vicp: rotation_output_compbits = %d.\n", afbcd->rot_ocompbit);
		pr_info("vicp: pip_src_mode = %d.\n", afbcd->pip_src_mode);
		pr_info("vicp: #################################.\n");
	}

	rev_mode_h = afbcd->rev_mode & 0x1;
	rev_mode_v = afbcd->rev_mode & 0x2;

	compbits_yuv = ((afbcd->compbits_v & 0x3) << 4) |
			((afbcd->compbits_u & 0x3) << 2) |
			((afbcd->compbits_y & 0x3) << 0);

	compbits_eq8 = (afbcd->compbits_y == 0 && afbcd->compbits_u == 0 && afbcd->compbits_v == 0);
	conv_lbuf_len = 256;

	dec_lbuf_depth = 128;
	mif_lbuf_depth = 128;

	dec_hsize_proc = ((afbcd->hsize >> 5) + ((afbcd->hsize & 0x1f) != 0)) * 32;
	dec_vsize_proc = ((afbcd->vsize >> 2) + ((afbcd->vsize & 0x3) != 0)) * 4;

	mif_blk_bgn_h = afbcd->win_bgn_h >> 5;
	mif_blk_bgn_v = afbcd->win_bgn_v >> 2;
	mif_blk_end_h = afbcd->win_end_h >> 5;
	mif_blk_end_v = afbcd->win_end_v >> 2;

	out_end_dif_h = afbcd->win_end_h - afbcd->win_bgn_h;
	out_end_dif_v = afbcd->win_end_v - afbcd->win_bgn_v;

	dec_pixel_bgn_h = afbcd->win_bgn_h & 0x1f;
	dec_pixel_bgn_v = afbcd->win_bgn_v & 0x03;
	dec_pixel_end_h = dec_pixel_bgn_h + out_end_dif_h;
	dec_pixel_end_v = dec_pixel_bgn_v + out_end_dif_v;

	fmt_size_h = ((afbcd->win_end_h >> 1) << 1) + 1 - ((afbcd->win_bgn_h >> 1) << 1);
	fmt_size_v = ((afbcd->win_end_v >> 1) << 1) + 1 - ((afbcd->win_bgn_v >> 1) << 1);
	fmt_size_h = afbcd->h_skip_y != 0 ? (fmt_size_h >> 1) + 1 : fmt_size_h + 1;
	fmt_size_v = afbcd->v_skip_y != 0 ? (fmt_size_v >> 1) + 1 : fmt_size_v + 1;

	comp_mt_20bit = (afbcd->fmt_mode == 0) &&
			((afbcd->fmt444_comb == 0) || (compbits_yuv > 0)) &&
			(afbcd->h_skip_y == 0);
	real_hsize_mt2k = comp_mt_20bit ? afbcd->hsize > 1024 : afbcd->hsize > 2048;
	/*use_4kram must be 1 if RAM_SIZE_4K==1*/
	use_4kram = real_hsize_mt2k || 1;

	set_afbcd_4k_enable(use_4kram);
	set_input_path(VICP_INPUT_PATH_AFBCD);

	if (is_rdma_enable)
		hold_line_num = 1;
	else
		hold_line_num = hold_line_num > 4 ? hold_line_num - 4 : 0;

	memset(&afbcd_mode, 0, sizeof(struct vicp_afbcd_mode_reg_t));
	afbcd_mode.ddr_sz_mode = afbcd->ddr_sz_mode;
	afbcd_mode.blk_mem_mode = afbcd->blk_mem_mode;
	afbcd_mode.rev_mode = afbcd->rev_mode;
	afbcd_mode.mif_urgent = 3;
	afbcd_mode.hold_line_num = hold_line_num;
	afbcd_mode.burst_len = 2;
	afbcd_mode.compbits_yuv = compbits_yuv;
	afbcd_mode.vert_skip_y = afbcd->v_skip_y;
	afbcd_mode.horz_skip_y = afbcd->h_skip_y;
	afbcd_mode.vert_skip_uv = afbcd->v_skip_uv;
	afbcd_mode.horz_skip_uv = afbcd->h_skip_uv;
	set_afbcd_mode(afbcd_mode);
	set_afbcd_input_size(afbcd->hsize, afbcd->vsize);
	set_afbcd_default_color(afbcd->def_color_y, afbcd->def_color_u, afbcd->def_color_v);
	set_afbcd_conv_control((enum vicp_color_format_e)afbcd->fmt_mode, conv_lbuf_len);
	set_afbcd_lbuf_depth(dec_lbuf_depth, mif_lbuf_depth);
	set_afbcd_addr(0, afbcd->head_baddr);
	set_afbcd_addr(1, afbcd->body_baddr);
	set_afbcd_mif_scope(0, mif_blk_bgn_h, mif_blk_end_h);
	set_afbcd_mif_scope(1, mif_blk_bgn_v, mif_blk_end_v);
	set_afbcd_pixel_scope(0, dec_pixel_bgn_h, dec_pixel_end_h);
	set_afbcd_pixel_scope(1, dec_pixel_bgn_v, dec_pixel_end_v);

	memset(&afbcd_general, 0, sizeof(struct vicp_afbcd_general_reg_t));
	afbcd_general.gclk_ctrl_core = 0;
	afbcd_general.fmt_size_sw_mode = 0;
	afbcd_general.addr_link_en = 1;
	afbcd_general.fmt444_comb = afbcd->fmt444_comb;
	afbcd_general.dos_uncomp_mode = afbcd->dos_uncomp;
	afbcd_general.soft_rst = 4;
	afbcd_general.ddr_blk_size = 3;
	afbcd_general.cmd_blk_size = 3;
	afbcd_general.dec_enable = 1;
	afbcd_general.head_len_sel = 0;
	afbcd_general.reserved = 0;
	set_afbcd_general(afbcd_general);

	if (afbcd->fmt_mode == 2) {/*420*/
		hfmt_en = ((afbcd->h_skip_y != 0) && (afbcd->h_skip_uv == 0)) ? 0 : 1;
		vfmt_en = ((afbcd->v_skip_y != 0) && (afbcd->v_skip_uv == 0)) ? 0 : 1;
		if (vfmt_en) {
			if (afbcd->v_skip_uv != 0) {
				uv_vsize_in = (fmt_size_v >> 2);
				vt_yc_ratio = afbcd->v_skip_y == 0 ? 2 : 1;
			} else {
				uv_vsize_in = (fmt_size_v >> 1);
				vt_yc_ratio = 1;
			}
		} else {
			uv_vsize_in = fmt_size_v;
			vt_yc_ratio = 0;
		}

		if (hfmt_en) {
			if (afbcd->h_skip_uv != 0)
				hz_yc_ratio = afbcd->h_skip_y == 0 ? 2 : 1;
			else
				hz_yc_ratio = 1;
		} else {
			hz_yc_ratio = 0;
		}
	} else if (afbcd->fmt_mode == 1) {/*422*/
		hfmt_en = ((afbcd->h_skip_y != 0) && (afbcd->h_skip_uv == 0)) ? 0 : 1;
		vfmt_en = ((afbcd->v_skip_y == 0) && (afbcd->v_skip_uv != 0)) ? 1 : 0;
		if (vfmt_en) {
			if (afbcd->v_skip_uv != 0) {
				uv_vsize_in = (fmt_size_v >> 1);
				vt_yc_ratio = afbcd->v_skip_y == 0 ? 1 : 0;
			} else {
				uv_vsize_in = (fmt_size_v >> 1);
				vt_yc_ratio = 1;
			}
		} else {
			uv_vsize_in = fmt_size_v;
			vt_yc_ratio = 0;
		}

		if (hfmt_en) {
			if (afbcd->h_skip_uv != 0)
				hz_yc_ratio = afbcd->h_skip_y == 0 ? 2 : 1;
			else
				hz_yc_ratio = 1;
		} else {
			hz_yc_ratio = 0;
		}
	} else if (afbcd->fmt_mode == 0) {/*444*/
		hfmt_en = ((afbcd->h_skip_y == 0) && (afbcd->h_skip_uv != 0)) ? 1 : 0;
		vfmt_en = ((afbcd->v_skip_y == 0) && (afbcd->v_skip_uv != 0)) ? 1 : 0;
		if (vfmt_en) {
			uv_vsize_in = (fmt_size_v >> 1);
			vt_yc_ratio = 1;
		} else {
			uv_vsize_in = fmt_size_v;
			vt_yc_ratio = 0;
		}

		if (hfmt_en)
			hz_yc_ratio = 1;
		else
			hz_yc_ratio = 0;
	}

	memset(&cfmt_control, 0, sizeof(struct vicp_afbcd_cfmt_control_reg_t));
	cfmt_control.cfmt_h_ini_phase = afbcd->hz_ini_phase;
	cfmt_control.cfmt_h_rpt_p0_en = afbcd->hz_rpt_fst0_en;
	cfmt_control.cfmt_h_yc_ratio = hz_yc_ratio;
	cfmt_control.cfmt_h_en = hfmt_en;
	cfmt_control.cfmt_v_phase0_nrpt_en = 1;
	cfmt_control.cfmt_v_rpt_line0_en = afbcd->vt_rpt_fst0_en;
	cfmt_control.cfmt_v_ini_phase = afbcd->vt_ini_phase;
	cfmt_control.cfmt_v_phase_step = (16 >> vt_yc_ratio);
	cfmt_control.cfmt_v_en = vfmt_en;
	set_afbcd_colorformat_control(cfmt_control);
	set_afbcd_colorformat_size(1, fmt_size_h, (fmt_size_h >> hz_yc_ratio));
	set_afbcd_colorformat_size(0, uv_vsize_in, 0);

	memset(&quant_control, 0, sizeof(struct vicp_afbcd_quant_control_reg_t));
	quant_control.lossy_chrm_en = afbcd->lossy_en;
	quant_control.lossy_luma_en = afbcd->lossy_en;
	set_afbcd_quant_control(quant_control);

	memset(&rotate_control, 0, sizeof(struct vicp_afbcd_rotate_control_reg_t));
	rotate_control.pip_mode = afbcd->pip_src_mode;
	rotate_control.uv_shrk_drop_mode_v = afbcd->rot_drop_mode;
	rotate_control.uv_shrk_drop_mode_h = afbcd->rot_drop_mode;
	rotate_control.uv_shrk_ratio_v = afbcd->rot_vshrk;
	rotate_control.uv_shrk_ratio_h = afbcd->rot_hshrk;
	rotate_control.y_shrk_drop_mode_v = afbcd->rot_drop_mode;
	rotate_control.y_shrk_drop_mode_h = afbcd->rot_drop_mode;
	rotate_control.y_shrk_ratio_v = afbcd->rot_vshrk;
	rotate_control.y_shrk_ratio_h = afbcd->rot_vshrk;
	rotate_control.uv422_drop_mode = 0;
	rotate_control.out_fmt_for_uv422 = 0;
	rotate_control.enable = afbcd->rot_en;
	set_afbcd_rotate_control(rotate_control);

	memset(&rotate_scope, 0, sizeof(struct vicp_afbcd_rotate_scope_reg_t));
	rotate_scope.in_fmt_force444 = 1;
	rotate_scope.out_fmt_mode = afbcd->rot_ofmt_mode;
	rotate_scope.out_compbits_y = afbcd->rot_ocompbit;
	rotate_scope.out_compbits_uv = afbcd->rot_ocompbit;
	rotate_scope.win_bgn_v = afbcd->rot_vbgn;
	rotate_scope.win_bgn_h = afbcd->rot_hbgn;
	set_afbcd_rotate_scope(rotate_scope);
}

void set_vid_cmpr_crop(struct vid_cmpr_crop_t *crop_param)
{
	if (IS_ERR_OR_NULL(crop_param)) {
		vicp_print(VICP_ERROR, "%s: invalid param,return.\n", __func__);
		return;
	}

	if (print_flag & VICP_CROP) {
		pr_info("vicp: ##########crop config##########\n");
		pr_info("vicp: enable = %d.\n", crop_param->enable);
		pr_info("vicp: hold_line = %d.\n", crop_param->hold_line);
		pr_info("vicp: dimm_layer_en: %d, dimm_data %d.\n", crop_param->dimm_layer_en,
			crop_param->dimm_data);
		pr_info("vicp: frame size: w: %d, h %d.\n", crop_param->frame_size_h,
			crop_param->frame_size_v);
		pr_info("vicp: window axis: %d %d %d %d.\n", crop_param->win_bgn_h,
			crop_param->win_end_h, crop_param->win_bgn_v, crop_param->win_end_v);
		pr_info("vicp: #################################.\n");
	};

	if (crop_param->enable) {
		set_crop_enable(1);
		set_crop_holdline(crop_param->hold_line);
		set_crop_dimm(crop_param->dimm_layer_en, crop_param->dimm_data);
		set_crop_size_in(crop_param->frame_size_h, crop_param->frame_size_v);
		set_crop_scope_h(crop_param->win_bgn_h, crop_param->win_end_h);
		set_crop_scope_v(crop_param->win_bgn_v, crop_param->win_end_v);
	} else {
		vicp_print(VICP_INFO, "%s: crop module disabled.\n", __func__);
		set_crop_enable(0);
	}
}

void set_mif_stride(struct vid_cmpr_mif_t *mif, int *stride_y, int *stride_cb, int *stride_cr)
{
	int pic_hsize = 0, comp_bits = 0, comp_num = 0;

	if (IS_ERR_OR_NULL(mif)) {
		vicp_print(VICP_ERROR, "%s: invalid param,return.\n", __func__);
		return;
	}

	/*if support scope,need change this to real hsize*/
	if (mif->buf_crop_en)
		pic_hsize = mif->buf_hsize;
	else
		pic_hsize = mif->luma_x_end0 - mif->luma_x_start0 + 1;

	/*0:8 bits 1:10 bits 422(old mode,12bit) 2: 10bit 444 3:10bit 422(full pack) or 444*/
	if (mif->bits_mode == 0)
		comp_bits = 8;
	else if (mif->bits_mode == 1)
		comp_bits = 12;
	else
		comp_bits = 10;

	/*00: 4:4:4; 01: 4:2:2; 10: 4:2:0*/
	if (mif->fmt_mode == 0)
		comp_num = 3;
	else
		comp_num = 2;

	vicp_print(VICP_INFO, "%s: pic_hsize = %d.\n", __func__, pic_hsize);
	vicp_print(VICP_INFO, "%s: comp_bits = %d.\n", __func__, comp_bits);
	vicp_print(VICP_INFO, "%s: comp_num = %d.\n", __func__, comp_num);

	u32 burst_stride_0;
	u32 burst_stride_1;
	u32 burst_stride_2;
	/*00 : one canvas. 01: 3 canvas(old 4:2:0). 10: 2 canvas(NV21). */
	if (mif->set_separate_en == 0) {
		burst_stride_0 = (pic_hsize * comp_num * comp_bits + 127) >> 7;
		burst_stride_1 = 0;
		burst_stride_2 = 0;
	} else if (mif->set_separate_en == 1) {
		burst_stride_0 = (pic_hsize * comp_bits + 127) >> 7;
		burst_stride_1 = (((pic_hsize + 1) >> 1) * comp_bits + 127) >> 7;
		burst_stride_2 = (((pic_hsize + 1) >> 1) * comp_bits + 127) >> 7;
	} else {
		burst_stride_0 = (pic_hsize * comp_bits + 127) >> 7;
		burst_stride_1 = (pic_hsize * comp_bits + 127) >> 7;
		burst_stride_2 = 0;
	}

	vicp_print(VICP_INFO, "%s: burst_stride_0 = %d.\n", __func__, burst_stride_0);
	vicp_print(VICP_INFO, "%s: burst_stride_1 = %d.\n", __func__, burst_stride_1);
	vicp_print(VICP_INFO, "%s: burst_stride_2 = %d.\n", __func__, burst_stride_2);

	/*now y cb cr all need 64bytes aligned */
	*stride_y = ((burst_stride_0 + 3) >> 2) << 2;
	*stride_cb = ((burst_stride_1 + 3) >> 2) << 2;
	*stride_cr = ((burst_stride_2 + 3) >> 2) << 2;

	vicp_print(VICP_INFO, "%s, stride_y: %d.\n", __func__, *stride_y);
	vicp_print(VICP_INFO, "%s: stride_cb = %d.\n", __func__, *stride_cb);
	vicp_print(VICP_INFO, "%s: stride_cr = %d.\n", __func__, *stride_cr);
};

void set_vid_cmpr_rmif(struct vid_cmpr_mif_t *rd_mif, int urgent, int hold_line)
{
	/*0: 1 byte per pixel, 1: 2 bytes per pixel, 2: 3 bytes per pixel */
	int bytes_per_pixel;
	int demux_mode;
	int chro_rpt_lastl_ctrl;
	int luma0_rpt_loop_start;
	int luma0_rpt_loop_end;
	int luma0_rpt_loop_pat;
	int chroma0_rpt_loop_start;
	int chroma0_rpt_loop_end;
	int chroma0_rpt_loop_pat;
	int cntl_bits_mode;
	int hfmt_en = 0;
	int hz_yc_ratio = 0;
	int hz_ini_phase = 0;
	int vfmt_en = 0;
	int vt_yc_ratio = 0;
	int vt_ini_phase = 0;
	int y_length = 0;
	int c_length = 0;
	int hz_rpt = 0;
	int bit_swap = 0, endian = 0;
	struct vicp_rdmif_general_reg_t general_reg;
	struct vicp_rdmif_general_reg2_t general_reg2;
	struct vicp_rdmif_general_reg3_t general_reg3;
	struct vicp_rdmif_rpt_loop_t rpt_loop;
	struct vicp_rdmif_color_format_t color_format;

	if (IS_ERR_OR_NULL(rd_mif)) {
		vicp_print(VICP_ERROR, "%s: invalid param,return.\n", __func__);
		return;
	}

	if (print_flag & VICP_RDMIF) {
		pr_info("vicp: ##########mif_in config##########\n");
		pr_info("vicp: luma_x: %d, %d.\n", rd_mif->luma_x_start0, rd_mif->luma_x_end0);
		pr_info("vicp: luma_y: %d, %d.\n", rd_mif->luma_y_start0, rd_mif->luma_y_end0);
		pr_info("vicp: chrm_x: %d, %d.\n", rd_mif->chrm_x_start0, rd_mif->chrm_x_end0);
		pr_info("vicp: chrm_y: %d, %d.\n", rd_mif->chrm_y_start0, rd_mif->chrm_y_end0);
		pr_info("vicp: set_separate_en = %d.\n", rd_mif->set_separate_en);
		pr_info("vicp: src_field_mode = %d.\n", rd_mif->src_field_mode);
		pr_info("vicp: input_fmt = %d.\n", rd_mif->fmt_mode);
		pr_info("vicp: input_field_num = %d.\n", rd_mif->output_field_num);
		pr_info("vicp: input_color_dep = %d.\n", rd_mif->bits_mode);
		pr_info("vicp: stride: y %d, u %d, v %d.\n", rd_mif->burst_size_y,
			rd_mif->burst_size_cb, rd_mif->burst_size_cr);
		pr_info("vicp: input addr0: 0x%llx\n", rd_mif->canvas0_addr0);
		pr_info("vicp: input addr1: 0x%llx\n", rd_mif->canvas0_addr1);
		pr_info("vicp: input addr2: 0x%llx\n", rd_mif->canvas0_addr2);
		pr_info("vicp: reversion: x %d, y %d.\n", rd_mif->rev_x, rd_mif->rev_y);
		pr_info("vicp: crop_en = %d.\n", rd_mif->buf_crop_en);
		pr_info("vicp: hsize = %d.\n", rd_mif->buf_hsize);
		pr_info("vicp: endian = %d.\n", rd_mif->endian);
		pr_info("vicp: block_mode = %d.\n", rd_mif->block_mode);
		pr_info("vicp: burst_len = %d.\n", rd_mif->burst_len);
		pr_info("vicp: #################################.\n");
	};

	if (rd_mif->set_separate_en == 0) {
		if (rd_mif->src_field_mode == 1) {
			chro_rpt_lastl_ctrl = 0;
			luma0_rpt_loop_start = 1;
			luma0_rpt_loop_end = 1;
			chroma0_rpt_loop_start = 0;
			chroma0_rpt_loop_end = 0;
			luma0_rpt_loop_pat = 0x80;
			chroma0_rpt_loop_pat = 0x00;
			set_rdmif_luma_fifo_size(0xc0);
		} else {
			chro_rpt_lastl_ctrl = 0;
			luma0_rpt_loop_start = 0;
			luma0_rpt_loop_end = 0;
			chroma0_rpt_loop_start = 0;
			chroma0_rpt_loop_end = 0;
			luma0_rpt_loop_pat = 0x00;
			chroma0_rpt_loop_pat = 0x00;
			set_rdmif_luma_fifo_size(0xc0);
		}

		if (rd_mif->fmt_mode == 1) {
			bytes_per_pixel = 1;
			demux_mode = 0;
		} else if (rd_mif->fmt_mode == 0) {
			bytes_per_pixel = 2;
			demux_mode = 1;
		} else {
			bytes_per_pixel = 3;
			demux_mode = 1;
		}
	} else {
		if (rd_mif->src_field_mode == 0) {
			if (rd_mif->fmt_mode == 2)
				chro_rpt_lastl_ctrl = 1;
			else
				chro_rpt_lastl_ctrl = 0;

			luma0_rpt_loop_start = 0;
			luma0_rpt_loop_end = 0;
			chroma0_rpt_loop_start = 0;
			chroma0_rpt_loop_end = 0;
			luma0_rpt_loop_pat = 0x0;
			chroma0_rpt_loop_pat = 0x0;
		} else if (rd_mif->src_field_mode == 1) {
			if (rd_mif->fmt_mode == 0)
				chro_rpt_lastl_ctrl = 1;
			else
				chro_rpt_lastl_ctrl = 0;

			luma0_rpt_loop_start = 1;
			luma0_rpt_loop_end = 1;
			chroma0_rpt_loop_start = 1;
			chroma0_rpt_loop_end = 1;
			luma0_rpt_loop_pat = 0x80;
			chroma0_rpt_loop_pat = 0x80;
		} else {
			chro_rpt_lastl_ctrl = 0;
			luma0_rpt_loop_start = 0;
			luma0_rpt_loop_end = 0;
			chroma0_rpt_loop_start = 0;
			chroma0_rpt_loop_end = 0;
			luma0_rpt_loop_pat = 0x00;
			chroma0_rpt_loop_pat = 0x00;
			set_rdmif_luma_fifo_size(0xc0);
		}

		bytes_per_pixel = 0;
		demux_mode = 0;
	}

	if (rd_mif->bits_mode == 3)
		bytes_per_pixel = 1;

	if (rd_mif->bits_mode == 0) {
		cntl_bits_mode = 0;
	} else if (rd_mif->bits_mode == 1) {
		if (rd_mif->fmt_mode == 1)
			cntl_bits_mode = 1;
		else if (rd_mif->fmt_mode == 0)
			cntl_bits_mode = 2;
		else
			cntl_bits_mode = 3;
		;
	} else {
		cntl_bits_mode = 3;
	}

	if (rd_mif->endian == 1) {
		endian = 1;
		bit_swap = 0;
	} else {
		endian = 0;
		bit_swap = 1;
	}

	memset(&general_reg3, 0x0, sizeof(struct vicp_rdmif_general_reg3_t));
	general_reg3.bits_mode = cntl_bits_mode;
	general_reg3.block_len = 3;
	general_reg3.burst_len = 2;
	general_reg3.bit_swap = bit_swap;
	set_rdmif_general_reg3(general_reg3);

	memset(&general_reg, 0x0, sizeof(struct vicp_rdmif_general_reg_t));
	general_reg.urgent_chroma = urgent;
	general_reg.urgent_luma = urgent;
	general_reg.luma_end_at_last_line = 1;
	general_reg.hold_lines = hold_line;
	general_reg.last_line_mode = 1;
	general_reg.demux_mode = demux_mode;
	general_reg.bytes_per_pixel = bytes_per_pixel;
	general_reg.ddr_burst_size_cr = rd_mif->burst_size_cr;
	general_reg.ddr_burst_size_cb = rd_mif->burst_size_cb;
	general_reg.ddr_burst_size_y = rd_mif->burst_size_y;
	general_reg.chroma_rpt_lastl = chro_rpt_lastl_ctrl;
	general_reg.little_endian = endian;
	if (rd_mif->set_separate_en == 0)
		general_reg.set_separate_en = 0;
	else
		general_reg.set_separate_en = 1;
	general_reg.enable = 1;
	set_rdmif_general_reg(general_reg);

	memset(&general_reg2, 0x0, sizeof(struct vicp_rdmif_general_reg2_t));
	if (rd_mif->set_separate_en == 2)
		general_reg2.color_map = 1;
	else
		general_reg2.color_map = 0;
	general_reg2.x_rev0 = rd_mif->rev_x;
	general_reg2.y_rev0 = rd_mif->rev_x;
	set_rdmif_general_reg2(general_reg2);

	u32 stride_y;
	u32 stride_cb;
	u32 stride_cr;

	set_mif_stride(rd_mif, &stride_y, &stride_cb, &stride_cr);

	vicp_print(VICP_INFO, "rdmif baddr=%x, stride=%d\n", rd_mif->canvas0_addr0 >> 4, stride_y);
	set_rdmif_base_addr(RDMIF_BASEADDR_TYPE_Y, rd_mif->canvas0_addr0);
	set_rdmif_base_addr(RDMIF_BASEADDR_TYPE_CB, rd_mif->canvas0_addr1);
	set_rdmif_base_addr(RDMIF_BASEADDR_TYPE_CR, rd_mif->canvas0_addr2);
	set_rdmif_stride(RDMIF_STRIDE_TYPE_Y, stride_y);
	set_rdmif_stride(RDMIF_STRIDE_TYPE_CB, stride_cb);
	set_rdmif_stride(RDMIF_STRIDE_TYPE_CR, stride_cr);

	set_rdmif_luma_position(0, 1, rd_mif->luma_x_start0, rd_mif->luma_x_end0);
	set_rdmif_luma_position(0, 0, rd_mif->luma_y_start0, rd_mif->luma_y_end0);
	set_rdmif_chroma_position(0, 1, rd_mif->chrm_x_start0, rd_mif->chrm_x_end0);
	set_rdmif_chroma_position(0, 0, rd_mif->chrm_y_start0, rd_mif->chrm_y_end0);

	memset(&rpt_loop, 0, sizeof(struct vicp_rdmif_rpt_loop_t));
	rpt_loop.rpt_loop1_chroma_start = 0;
	rpt_loop.rpt_loop1_chroma_end = 0;
	rpt_loop.rpt_loop1_luma_start = 0;
	rpt_loop.rpt_loop1_luma_end = 0;
	rpt_loop.rpt_loop0_chroma_start = chroma0_rpt_loop_start;
	rpt_loop.rpt_loop0_chroma_end = chroma0_rpt_loop_end;
	rpt_loop.rpt_loop0_luma_start = luma0_rpt_loop_start;
	rpt_loop.rpt_loop0_luma_end = luma0_rpt_loop_end;
	set_rdmif_rpt_loop(rpt_loop);
	set_rdmif_luma_rpt_pat(0, luma0_rpt_loop_pat);
	set_rdmif_chroma_rpt_pat(0, chroma0_rpt_loop_pat);
	set_rdmif_dummy_pixel(0x00808000);

	if (rd_mif->fmt_mode == 2) {
		hfmt_en = 1;
		hz_yc_ratio = 1;
		hz_ini_phase = 0;
		vfmt_en = 1;
		vt_yc_ratio = 1;
		vt_ini_phase = 0;
		y_length = rd_mif->luma_x_end0 - rd_mif->luma_x_start0 + 1;
		c_length = rd_mif->chrm_x_end0 - rd_mif->chrm_x_start0 + 1;
		hz_rpt = 0;
	} else if (rd_mif->fmt_mode == 1) {
		hfmt_en = 1;
		hz_yc_ratio = 1;
		hz_ini_phase = 0;
		vfmt_en = 0;
		vt_yc_ratio = 0;
		vt_ini_phase = 0;
		y_length = rd_mif->luma_x_end0 - rd_mif->luma_x_start0 + 1;
		c_length = ((rd_mif->luma_x_end0 >> 1) - (rd_mif->luma_x_start0 >> 1) + 1);
		hz_rpt	 = 0;
	} else if (rd_mif->fmt_mode == 0) {
		hfmt_en = 0;
		hz_yc_ratio = 1;
		hz_ini_phase = 0;
		vfmt_en = 0;
		vt_yc_ratio = 0;
		vt_ini_phase = 0;
		y_length = rd_mif->luma_x_end0 - rd_mif->luma_x_start0 + 1;
		c_length = rd_mif->luma_x_end0 - rd_mif->luma_x_start0 + 1;
		hz_rpt = 0;
	}

	memset(&color_format, 0, sizeof(struct vicp_rdmif_color_format_t));
	color_format.cfmt_h_rpt_pix = hz_rpt;
	color_format.cfmt_h_ini_phase = hz_ini_phase;
	color_format.cfmt_h_rpt_p0_en = 0;
	color_format.cfmt_h_yc_ratio = hz_yc_ratio;
	color_format.cfmt_h_en = hfmt_en;
	color_format.cfmt_v_phase0_nrpt_en = 1;
	color_format.cfmt_v_rpt_line0_en = 0;
	color_format.cfmt_v_skip_line_num = 0;
	color_format.cfmt_v_ini_phase = vt_ini_phase;
	color_format.cfmt_v_phase_step = (16 >> vt_yc_ratio);
	color_format.cfmt_v_en = vfmt_en;
	set_rdmif_color_format_control(color_format);
	set_rdmif_color_format_width(y_length, c_length);
};

void set_vid_cmpr_wmif(struct vid_cmpr_mif_t *wr_mif, int wrmif_en)
{
	u32 stride_y, stride_cb, stride_cr, rgb_mode, bit10_mode;
	struct vicp_wrmif_control_t wrmif_control;

	if (IS_ERR_OR_NULL(wr_mif)) {
		vicp_print(VICP_ERROR, "%s: invalid param,return.\n", __func__);
		return;
	}

	if (print_flag & VICP_WRMIF) {
		pr_info("vicp: ##########mif_out config##########\n");
		pr_info("vicp: luma_x: %d, %d.\n", wr_mif->luma_x_start0, wr_mif->luma_x_end0);
		pr_info("vicp: luma_y: %d, %d.\n", wr_mif->luma_y_start0, wr_mif->luma_y_end0);
		pr_info("vicp: chrm_x: %d, %d.\n", wr_mif->chrm_x_start0, wr_mif->chrm_x_end0);
		pr_info("vicp: chrm_y: %d, %d.\n", wr_mif->chrm_y_start0, wr_mif->chrm_y_end0);
		pr_info("vicp: set_separate_en = %d.\n", wr_mif->set_separate_en);
		pr_info("vicp: src_field_mode = %d.\n", wr_mif->src_field_mode);
		pr_info("vicp: input_fmt = %d.\n", wr_mif->fmt_mode);
		pr_info("vicp: output_field_num = %d.\n", wr_mif->output_field_num);
		pr_info("vicp: output_color_dep = %d.\n", wr_mif->bits_mode);
		pr_info("vicp: stride: y %d, u %d, v %d.\n", wr_mif->burst_size_y,
			wr_mif->burst_size_cb, wr_mif->burst_size_cr);
		pr_info("vicp: output addr0: 0x%llx.\n", wr_mif->canvas0_addr0);
		pr_info("vicp: output addr1: 0x%llx.\n", wr_mif->canvas0_addr1);
		pr_info("vicp: output addr2: 0x%llx.\n", wr_mif->canvas0_addr2);
		pr_info("vicp: reversion: x %d, y %d.\n", wr_mif->rev_x, wr_mif->rev_y);
		pr_info("vicp: crop_en = %d.\n", wr_mif->buf_crop_en);
		pr_info("vicp: hsize = %d.\n", wr_mif->buf_hsize);
		pr_info("vicp: #################################.\n");
	};

	set_mif_stride(wr_mif, &stride_y, &stride_cb, &stride_cr);

	set_wrmif_base_addr(0, wr_mif->canvas0_addr0);
	set_wrmif_base_addr(1, wr_mif->canvas0_addr1);
	set_wrmif_stride(1, stride_y);
	set_wrmif_stride(0, stride_cb);
	set_wrmif_range(1, wr_mif->rev_x, wr_mif->luma_x_start0, wr_mif->luma_x_end0);
	set_wrmif_range(0, wr_mif->rev_y, wr_mif->luma_y_start0, wr_mif->luma_y_end0);

	if (wr_mif->fmt_mode == 1)
		rgb_mode = 0;
	else if (wr_mif->fmt_mode == 0)
		rgb_mode = 1;
	else
		rgb_mode = 2;

	if (wr_mif->bits_mode == 1)
		bit10_mode = 1;
	else
		bit10_mode = 0;

	memset(&wrmif_control, 0, sizeof(struct vicp_wrmif_control_t));
	wrmif_control.swap_64bits_en = 0;
	wrmif_control.burst_limit = 2;
	wrmif_control.canvas_sync_en = 0;
	wrmif_control.gate_clock_en = 0;
	wrmif_control.rgb_mode = rgb_mode;
	wrmif_control.h_conv = 0;
	wrmif_control.v_conv = 0;
	wrmif_control.swap_cbcr = 0;
	wrmif_control.urgent = 0;
	wrmif_control.word_limit = 4;
	wrmif_control.data_ext_ena = 0;
	wrmif_control.little_endian = 1;
	wrmif_control.bit10_mode = bit10_mode;
	wrmif_control.wrmif_en = wrmif_en;
	set_wrmif_control(wrmif_control);
};

static void dump_yuv(int flag, struct vframe_s *vframe)
{
	struct file *fp = NULL;
	char name_buf[32];
	int data_size;
	u8 *data_addr;
	mm_segment_t fs;
	loff_t pos;

	if (flag == 0) {
		snprintf(name_buf, sizeof(name_buf), "/data/in_%d_%d_nv12.yuv",
				vframe->width, vframe->height);
	} else {
		snprintf(name_buf, sizeof(name_buf), "/data/out_%d_%d_nv12.yuv",
			vframe->width, vframe->height);
	}
	data_size = vframe->width * vframe->height * 3 / 2;
	data_addr = codec_mm_vmap(vframe->canvas0_config[0].phy_addr, data_size);
	codec_mm_dma_flush(data_addr, data_size, DMA_FROM_DEVICE);

	fp = filp_open(name_buf, O_CREAT | O_RDWR, 0644);
	if (IS_ERR(fp) || IS_ERR_OR_NULL(data_addr)) {
		vicp_print(VICP_ERROR, "%s: vmap failed.\n", __func__);
		return;
	}

	fs = get_fs();
	set_fs(KERNEL_DS);
	pos = fp->f_pos;
	vfs_write(fp, data_addr, data_size, &pos);
	fp->f_pos = pos;
	vfs_fsync(fp, 0);
	vicp_print(VICP_INFO, "%s: write %u size.\n", __func__, data_size);
	codec_mm_unmap_phyaddr(data_addr);
	set_fs(fs);
	filp_close(fp, NULL);
}

static int get_input_color_bitdepth(struct vframe_s *vf)
{
	int bitdepth = 0;

	if (IS_ERR_OR_NULL(vf)) {
		vicp_print(VICP_ERROR, "%s: NULL param.\n", __func__);
		return -1;
	}

	if (vf->bitdepth & BITDEPTH_Y8)
		bitdepth = 8;
	else if (vf->bitdepth & BITDEPTH_Y9)
		bitdepth = 9;
	else if (vf->bitdepth & BITDEPTH_Y10)
		bitdepth = 10;
	else if (vf->bitdepth & BITDEPTH_Y12)
		bitdepth = 12;
	else
		bitdepth = 8;

	return bitdepth;
}

static int get_input_color_format(struct vframe_s *vf)
{
	int format = 0;

	if (IS_ERR_OR_NULL(vf)) {
		vicp_print(VICP_ERROR, "%s: NULL param.\n", __func__);
		return -1;
	}

	/*0:yuv444 1:yuv422 2:yuv420 */
	if (vf->type & VIDTYPE_VIU_444)
		format = 0;
	else if (vf->type & VIDTYPE_VIU_422)
		format = 1;
	else
		format = 2;

	return format;
}

void set_vid_cmpr(struct vid_cmpr_top_t *vid_cmpr_top)
{
	struct vid_cmpr_afbcd_t vid_cmpr_afbcd;
	struct vid_cmpr_scaler_t vid_cmpr_scaler;
	struct vid_cmpr_hdr_t vid_cmpr_hdr;
	struct vid_cmpr_afbce_t vid_cmpr_afbce;
	enum hdr2_sel_e hdr2_sel;

	struct vid_cmpr_mif_t vid_cmpr_rmif;
	struct vid_cmpr_mif_t vid_cmpr_wmif;
	int output_begin_h, output_begin_v, output_end_h, output_end_v;
	int shrink_enable, shrink_size, shrink_mode;
	int scaler_enable;

	if (IS_ERR_OR_NULL(vid_cmpr_top)) {
		vicp_print(VICP_ERROR, "%s: invalid param,return.\n", __func__);
		return;
	}

	vicp_print(VICP_INFO, "==== input_win_bgn_h: %d, input_win_end_h: %d ====\n",
				vid_cmpr_top->src_win_bgn_h, vid_cmpr_top->src_win_end_h);
	vicp_print(VICP_INFO, "==== input_win_bgn_v: %d, input_win_end_v: %d ====\n",
				vid_cmpr_top->src_win_bgn_v, vid_cmpr_top->src_win_end_v);

	vicp_print(VICP_INFO, "==== out_win_bgn_h: %d, out_win_end_h: %d ====\n",
				vid_cmpr_top->out_win_bgn_h, vid_cmpr_top->out_win_end_h);
	vicp_print(VICP_INFO, "==== out_win_bgn_v: %d, out_win_end_v: %d ====\n",
				vid_cmpr_top->out_win_bgn_v, vid_cmpr_top->out_win_end_v);

	vicp_print(VICP_INFO, "==== out_shrk_mode: %d\n", vid_cmpr_top->out_shrk_mode);

	if (vid_cmpr_top->src_compress == 1) {
		set_rdmif_enable(0);
		set_input_path(VICP_INPUT_PATH_AFBCD);
		vicp_print(VICP_INFO, "%s: start config AFBCD parameter.\n", __func__, __LINE__);
		memset(&vid_cmpr_afbcd, 0, sizeof(struct vid_cmpr_afbcd_t));
		if (vid_cmpr_top->src_vf->bitdepth & BITDEPTH_SAVING_MODE)
			vid_cmpr_afbcd.blk_mem_mode = 1;
		else
			vid_cmpr_afbcd.blk_mem_mode = 0;
		vid_cmpr_afbcd.hsize = vid_cmpr_top->src_hsize;
		vid_cmpr_afbcd.vsize = vid_cmpr_top->src_vsize;
		vid_cmpr_afbcd.head_baddr = vid_cmpr_top->src_head_baddr >> 4;
		vid_cmpr_afbcd.body_baddr = vid_cmpr_top->src_body_baddr >> 4;
		vid_cmpr_afbcd.compbits_y = vid_cmpr_top->src_compbits - 8;
		vid_cmpr_afbcd.compbits_u = vid_cmpr_top->src_compbits - 8;
		vid_cmpr_afbcd.compbits_v = vid_cmpr_top->src_compbits - 8;
		vid_cmpr_afbcd.fmt_mode  = vid_cmpr_top->src_fmt_mode;
		if (vid_cmpr_top->src_vf->type & VIDTYPE_SCATTER)
			vid_cmpr_afbcd.ddr_sz_mode = 1;
		else
			vid_cmpr_afbcd.ddr_sz_mode = 0;

		if (vid_cmpr_top->src_vf->type & VIDTYPE_COMB_MODE)
			vid_cmpr_afbcd.fmt444_comb = 1;
		else
			vid_cmpr_afbcd.fmt444_comb = 0;

		if (!(vid_cmpr_top->src_vf->type & VIDTYPE_VIU_422))
			vid_cmpr_afbcd.dos_uncomp = 1;
		else
			vid_cmpr_afbcd.dos_uncomp = 0;

		vid_cmpr_afbcd.rot_en = vid_cmpr_top->out_rot_en;
		vid_cmpr_afbcd.rot_hbgn = 0;
		vid_cmpr_afbcd.rot_vbgn = 0;
		vid_cmpr_afbcd.h_skip_y = 0;
		vid_cmpr_afbcd.v_skip_y = 0;
		vid_cmpr_afbcd.h_skip_uv = 0;
		vid_cmpr_afbcd.v_skip_uv = 0;
		vid_cmpr_afbcd.rev_mode = vid_cmpr_top->rot_rev_mode;
		vid_cmpr_afbcd.lossy_en = 0;
		vid_cmpr_afbcd.def_color_y = 0x00 << (vid_cmpr_top->src_compbits - 8);
		vid_cmpr_afbcd.def_color_u = 0x80 << (vid_cmpr_top->src_compbits - 8);
		vid_cmpr_afbcd.def_color_v = 0x80 << (vid_cmpr_top->src_compbits - 8);
		vid_cmpr_afbcd.win_bgn_h = vid_cmpr_top->src_win_bgn_h;
		vid_cmpr_afbcd.win_end_h = vid_cmpr_top->src_win_end_h;
		vid_cmpr_afbcd.win_bgn_v = vid_cmpr_top->src_win_bgn_v;
		vid_cmpr_afbcd.win_end_v = vid_cmpr_top->src_win_end_v;
		vid_cmpr_afbcd.hz_ini_phase = 0;
		vid_cmpr_afbcd.vt_ini_phase = 0;
		vid_cmpr_afbcd.hz_rpt_fst0_en = 0;
		vid_cmpr_afbcd.vt_rpt_fst0_en = 0;
		vid_cmpr_afbcd.pip_src_mode = vid_cmpr_top->src_pip_src_mode;
		vid_cmpr_afbcd.rot_vshrk = vid_cmpr_top->rot_vshrk_ratio;
		vid_cmpr_afbcd.rot_hshrk = vid_cmpr_top->rot_hshrk_ratio;
		vid_cmpr_afbcd.rot_drop_mode = 0;
		vid_cmpr_afbcd.rot_ofmt_mode = vid_cmpr_top->out_reg_format_mode;
		vid_cmpr_afbcd.rot_ocompbit = vid_cmpr_top->out_reg_compbits - 8;

		set_vid_cmpr_afbcd(2, &vid_cmpr_afbcd);
		vicp_print(VICP_INFO, "AFBCD config done.\n");
	} else {
		set_afbcd_enable(0);
		set_input_path(VICP_INPUT_PATH_RDMIF);
		vicp_print(VICP_INFO, "%s: start config RDMIF parameter.\n", __func__, __LINE__);
		memset(&vid_cmpr_rmif, 0, sizeof(struct vid_cmpr_mif_t));
		vid_cmpr_rmif.luma_x_start0 = vid_cmpr_top->src_win_bgn_h;
		vid_cmpr_rmif.luma_x_end0 = vid_cmpr_top->src_win_end_h;
		vid_cmpr_rmif.luma_y_start0 = vid_cmpr_top->src_win_bgn_v;
		vid_cmpr_rmif.luma_y_end0 = vid_cmpr_top->src_win_end_v;
		vid_cmpr_rmif.set_separate_en = vid_cmpr_top->rdmif_separate_en;
		vid_cmpr_rmif.fmt_mode = vid_cmpr_top->src_fmt_mode;
		vid_cmpr_rmif.endian = vid_cmpr_top->src_endian;
		vid_cmpr_rmif.block_mode = vid_cmpr_top->src_block_mode;
		vid_cmpr_rmif.burst_len = vid_cmpr_top->src_burst_len;

		if (vid_cmpr_top->src_fmt_mode == 0) {
			vid_cmpr_rmif.chrm_x_start0 = vid_cmpr_rmif.luma_x_start0;
			vid_cmpr_rmif.chrm_x_end0 = vid_cmpr_rmif.luma_x_end0;
			vid_cmpr_rmif.chrm_y_start0 = vid_cmpr_rmif.luma_y_start0;
			vid_cmpr_rmif.chrm_y_end0 = vid_cmpr_rmif.luma_y_end0;
		} else if (vid_cmpr_top->src_fmt_mode == 2) {
			vid_cmpr_rmif.chrm_x_start0 = (vid_cmpr_rmif.luma_x_start0 + 1) >> 1;
			vid_cmpr_rmif.chrm_x_end0 = ((vid_cmpr_rmif.luma_x_end0 + 1) >> 1) - 1;
			vid_cmpr_rmif.chrm_y_start0 = (vid_cmpr_rmif.luma_y_start0 + 1) >> 1;
			vid_cmpr_rmif.chrm_y_end0 = ((vid_cmpr_rmif.luma_y_end0 + 1) >> 1) - 1;
		} else {
			vid_cmpr_rmif.chrm_x_start0 = (vid_cmpr_rmif.luma_x_start0 + 1) >> 1;
			vid_cmpr_rmif.chrm_x_end0 = ((vid_cmpr_rmif.luma_x_end0 + 1) >> 1) - 1;
			vid_cmpr_rmif.chrm_y_start0 = vid_cmpr_rmif.luma_y_start0;
			vid_cmpr_rmif.chrm_y_end0 = vid_cmpr_rmif.luma_y_end0;
		}

		if (vid_cmpr_top->src_compbits == 8)
			vid_cmpr_rmif.bits_mode = 0;
		else if (vid_cmpr_top->src_compbits == 10)
			vid_cmpr_rmif.bits_mode = 1;
		else
			vid_cmpr_rmif.bits_mode = 2;

		vid_cmpr_rmif.canvas0_addr0 = vid_cmpr_top->rdmif_canvas0_addr0;
		vid_cmpr_rmif.canvas0_addr1 = vid_cmpr_top->rdmif_canvas0_addr1;
		vid_cmpr_rmif.canvas0_addr2 = vid_cmpr_top->rdmif_canvas0_addr2;
		vid_cmpr_rmif.rev_x = 0;
		vid_cmpr_rmif.rev_y = 0;
		vid_cmpr_rmif.buf_crop_en = 0;
		vid_cmpr_rmif.src_field_mode = 0;
		vid_cmpr_rmif.output_field_num = 1;

		set_vid_cmpr_rmif(&vid_cmpr_rmif, 0, 2);
		vicp_print(VICP_INFO, "RDMIF config done.\n");
	}
	vicp_print(VICP_INFO, "%s-%d: start config Scaler parameter.\n", __func__, __LINE__);
	memset(&vid_cmpr_scaler, 0, sizeof(struct vid_cmpr_scaler_t));
	vid_cmpr_scaler.din_hsize = vid_cmpr_top->src_hsize;
	vid_cmpr_scaler.din_vsize = vid_cmpr_top->src_vsize;
	if (vid_cmpr_top->out_rot_en == 1) {
		vid_cmpr_scaler.dout_hsize = vid_cmpr_top->src_hsize;
		vid_cmpr_scaler.dout_vsize = vid_cmpr_top->src_vsize;
	} else {
		vid_cmpr_scaler.dout_hsize = vid_cmpr_top->out_hsize_in;
		vid_cmpr_scaler.dout_vsize = vid_cmpr_top->out_vsize_in;
	}
	vid_cmpr_scaler.vert_bank_length = 4;
	vid_cmpr_scaler.prehsc_en = 0;
	vid_cmpr_scaler.prevsc_en = 0;
	vid_cmpr_scaler.prehsc_rate = 0;
	vid_cmpr_scaler.prevsc_rate = 0;
	vid_cmpr_scaler.high_res_coef_en = 1;
	vid_cmpr_scaler.phase_step_en = 0;
	vid_cmpr_scaler.phase_step = 0;

	if (!scaler_en)
		scaler_enable = 0;
	else
		scaler_enable = 1;

	set_input_size(vid_cmpr_scaler.din_vsize, vid_cmpr_scaler.din_hsize);
	set_vid_cmpr_scale(scaler_enable, &vid_cmpr_scaler);
	vicp_print(VICP_INFO, "Scaler config done.\n");
	vicp_print(VICP_INFO, "%s-%d: start config HDR parameter.\n", __func__, __LINE__);
	memset(&vid_cmpr_hdr, 0, sizeof(struct vid_cmpr_hdr_t));
	hdr2_sel = VID_CMPR_HDR2;
	if (!hdr_en)
		vid_cmpr_hdr.hdr2_en = 0;
	else
		vid_cmpr_hdr.hdr2_en = vid_cmpr_top->hdr_en;
	vid_cmpr_hdr.hdr2_only_mat = 0;
	vid_cmpr_hdr.hdr2_fmt_cfg = 0;
	vid_cmpr_hdr.in_fmt = 1;
	vid_cmpr_hdr.rgb_out_en = 0;

	set_vid_cmpr_hdr2(hdr2_sel,
		vid_cmpr_hdr.hdr2_en,
		vid_cmpr_hdr.hdr2_only_mat,
		vid_cmpr_hdr.hdr2_fmt_cfg,
		vid_cmpr_hdr.in_fmt,/*1:yuv in   0:rgb in */
		vid_cmpr_hdr.rgb_out_en
	);
	vicp_print(VICP_INFO, "HDR config done\n");

	if (vid_cmpr_top->wrmif_en == 1) {
		if (vid_cmpr_top->out_afbce_enable == 0)
			set_output_path(VICP_OUTPUT_PATH_WRMIF);
		else
			set_output_path(VICP_OUTPUT_PATH_ALL);
		vicp_print(VICP_INFO, "%s: start config WRMIF parameter.\n", __func__, __LINE__);
		memset(&vid_cmpr_wmif, 0, sizeof(struct vid_cmpr_mif_t));
		output_begin_h = vid_cmpr_top->out_win_bgn_h;
		output_begin_v = vid_cmpr_top->out_win_bgn_v;
		output_end_h = vid_cmpr_top->out_win_end_h;
		output_end_v = vid_cmpr_top->out_win_end_v;

		if (!shrink_en)
			shrink_enable = 0;
		else
			shrink_enable = vid_cmpr_top->out_shrk_en;
		shrink_size = (output_end_h - output_begin_h + 1) << 13 |
				(output_end_v - output_begin_v + 1);
		shrink_mode = vid_cmpr_top->out_shrk_mode;
		set_vid_cmpr_shrink(shrink_enable, shrink_size, shrink_mode, shrink_mode);
		set_output_size(vid_cmpr_top->out_vsize_in, vid_cmpr_top->out_hsize_in);

		if (shrink_enable == 1) {
			vid_cmpr_wmif.luma_x_start0 = output_begin_h >> (1 + shrink_mode);
			vid_cmpr_wmif.luma_x_end0 = ((output_end_h - output_begin_h +
				(1 << (1 + shrink_mode))) >> (1 + shrink_mode)) - 1;
			vid_cmpr_wmif.luma_y_start0 = output_begin_v >> (1 + shrink_mode);
			vid_cmpr_wmif.luma_y_end0 = ((output_end_v - output_begin_v +
				(1 << (1 + shrink_mode))) >> (1 + shrink_mode)) - 1;
			vid_cmpr_wmif.luma_x_end0 += vid_cmpr_wmif.luma_x_start0;
			vid_cmpr_wmif.luma_y_end0 += vid_cmpr_wmif.luma_y_start0;
			vid_cmpr_wmif.buf_hsize =
				(vid_cmpr_top->out_hsize_bgnd + 1) >> (1 + shrink_mode);
		} else {
			vid_cmpr_wmif.luma_x_start0 = output_begin_h;
			vid_cmpr_wmif.luma_x_end0 = output_end_h;
			vid_cmpr_wmif.luma_y_start0 = output_begin_v;
			vid_cmpr_wmif.luma_y_end0 = output_end_v;
			vid_cmpr_wmif.buf_hsize = vid_cmpr_top->out_hsize_bgnd;
		}
		vicp_print(VICP_INFO, "==== double wrmif luma start_x: %d, end_x: %d ====\n",
					vid_cmpr_wmif.luma_x_start0, vid_cmpr_wmif.luma_x_end0);
		vicp_print(VICP_INFO, "==== double wrmif luma start_y: %d, end_y: %d ====\n",
					vid_cmpr_wmif.luma_y_start0, vid_cmpr_wmif.luma_y_end0);
		vid_cmpr_wmif.set_separate_en = vid_cmpr_top->wrmif_set_separate_en;
		vid_cmpr_wmif.fmt_mode = vid_cmpr_top->wrmif_fmt_mode;

		if (vid_cmpr_top->wrmif_bits_mode == 8)
			vid_cmpr_wmif.bits_mode = 0;
		else if (vid_cmpr_top->wrmif_bits_mode == 10)
			vid_cmpr_wmif.bits_mode = 1;
		else
			vid_cmpr_wmif.bits_mode = 2;

		vid_cmpr_wmif.canvas0_addr0 = vid_cmpr_top->wrmif_canvas0_addr0;
		vid_cmpr_wmif.canvas0_addr1 = vid_cmpr_top->wrmif_canvas0_addr0 +
			((vid_cmpr_wmif.luma_x_end0 + 1) * (vid_cmpr_wmif.luma_y_end0 + 1));
		vid_cmpr_wmif.canvas0_addr2 = vid_cmpr_top->wrmif_canvas0_addr2;
		vid_cmpr_wmif.rev_x = 0;
		vid_cmpr_wmif.rev_y = 0;

		if (!crop_en)
			vid_cmpr_wmif.buf_crop_en = 0;
		else
			vid_cmpr_wmif.buf_crop_en = 1;
		vid_cmpr_wmif.src_field_mode = 0;
		vid_cmpr_wmif.output_field_num = 1;
		vicp_print(VICP_INFO, "wrmif_bits_mode: %d, wrmif_canvas0_addr: %lx.\n",
			vid_cmpr_top->wrmif_bits_mode, vid_cmpr_top->wrmif_canvas0_addr0);

		set_vid_cmpr_wmif(&vid_cmpr_wmif, vid_cmpr_top->wrmif_en);
		vicp_print(VICP_INFO, "WRMIF config done.\n");
	} else {
		set_output_path(VICP_OUTPUT_PATH_AFBCE);
	}

	vicp_print(VICP_INFO, "%s-%d: start config AFBCE parameter.\n", __func__, __LINE__);
	memset(&vid_cmpr_afbce, 0, sizeof(struct vid_cmpr_afbce_t));
	if (vid_cmpr_top->out_afbce_enable == 1) {
		vid_cmpr_afbce.head_baddr = vid_cmpr_top->out_head_baddr;
		vid_cmpr_afbce.table_baddr = vid_cmpr_top->out_mmu_info_baddr;
		vid_cmpr_afbce.reg_init_ctrl = vid_cmpr_top->out_reg_init_ctrl;
		vid_cmpr_afbce.reg_pip_mode = vid_cmpr_top->out_reg_pip_mode;
		vid_cmpr_afbce.reg_ram_comb = 0;
		vid_cmpr_afbce.reg_format_mode = vid_cmpr_top->out_reg_format_mode;
		vid_cmpr_afbce.reg_compbits_y = vid_cmpr_top->out_reg_compbits;
		vid_cmpr_afbce.reg_compbits_c = vid_cmpr_top->out_reg_compbits;
		vid_cmpr_afbce.hsize_in = vid_cmpr_top->out_hsize_in;
		vid_cmpr_afbce.vsize_in = vid_cmpr_top->out_vsize_in;
		vid_cmpr_afbce.hsize_bgnd = vid_cmpr_top->out_hsize_bgnd;
		vid_cmpr_afbce.vsize_bgnd = vid_cmpr_top->out_vsize_bgnd;
		vid_cmpr_afbce.enc_win_bgn_h = vid_cmpr_top->out_win_bgn_h;
		vid_cmpr_afbce.enc_win_end_h = vid_cmpr_top->out_win_end_h;
		vid_cmpr_afbce.enc_win_bgn_v = vid_cmpr_top->out_win_bgn_v;
		vid_cmpr_afbce.enc_win_end_v = vid_cmpr_top->out_win_end_v;
		vid_cmpr_afbce.loosy_mode = 0;
		vid_cmpr_afbce.rev_mode = 0;
		vid_cmpr_afbce.def_color_0 = 0x3ff;/*<< (vid_cmpr_top->out_reg_compbits - 8);*/
		vid_cmpr_afbce.def_color_1 = 0x80 << (vid_cmpr_top->out_reg_compbits - 8);
		vid_cmpr_afbce.def_color_2 = 0x80 << (vid_cmpr_top->out_reg_compbits - 8);
		vid_cmpr_afbce.def_color_3 = 0x00 << (vid_cmpr_top->out_reg_compbits - 8);
		vid_cmpr_afbce.force_444_comb = 0;
		vid_cmpr_afbce.rot_en = vid_cmpr_top->out_rot_en;
		vid_cmpr_afbce.din_swt = 0;
		set_vid_cmpr_afbce(vid_cmpr_top->out_afbce_enable, &vid_cmpr_afbce);
	} else {
		set_vid_cmpr_afbce(vid_cmpr_top->out_afbce_enable, &vid_cmpr_afbce);
	}
	vicp_print(VICP_INFO, "AFBCE config done\n");

	vicp_reg_set_bits(VID_CMPR_AFBCE_MODE, 0x5, 16, 7);
	vicp_reg_set_bits(VID_CMPR_HOLD_LINE, 0x5, 0, 16);
}

int vicp_process_config(struct vicp_data_config_t *data_config,
	struct vid_cmpr_top_t *vid_cmpr_top)
{
	struct vframe_s *input_vframe = NULL;
	struct dma_data_config_t *input_dma = NULL;
	enum vicp_rotation_mode_e rotation;
	u32 canvas_width = 0;

	if (IS_ERR_OR_NULL(data_config) || IS_ERR_OR_NULL(vid_cmpr_top)) {
		vicp_print(VICP_ERROR, "%s: NULL param.\n", __func__);
		return -1;
	}

	if (data_config->input_data.is_vframe) {
		input_vframe = data_config->input_data.data_vf;
		vicp_print(VICP_INFO, "%s: src_addr = 0x%lx.\n", __func__,
			input_vframe->canvas0_config[0].phy_addr);

		if (current_dump_flag != dump_yuv_flag) {
			dump_yuv(0, input_vframe);
			current_dump_flag = dump_yuv_flag;
		}

		if (input_vframe->type & VIDTYPE_COMPRESS) {
			vid_cmpr_top->src_compress = 1;
			vid_cmpr_top->src_hsize = input_vframe->compWidth;
			vid_cmpr_top->src_vsize = input_vframe->compHeight;
			vid_cmpr_top->src_head_baddr = input_vframe->compHeadAddr;
			vid_cmpr_top->src_body_baddr = input_vframe->compBodyAddr;
		} else {
			vid_cmpr_top->src_compress = 0;
			vid_cmpr_top->src_hsize = input_vframe->width;
			vid_cmpr_top->src_vsize = input_vframe->height;
			vid_cmpr_top->src_head_baddr = 0;
			vid_cmpr_top->src_body_baddr = 0;
			vid_cmpr_top->rdmif_canvas0_addr0 =
				input_vframe->canvas0_config[0].phy_addr;
		}
		vid_cmpr_top->src_vf = input_vframe;
		vid_cmpr_top->src_fmt_mode = get_input_color_format(input_vframe);
		vid_cmpr_top->src_compbits = get_input_color_bitdepth(input_vframe);
		vid_cmpr_top->src_win_bgn_h = 0;
		vid_cmpr_top->src_win_end_h = vid_cmpr_top->src_hsize - 1;
		vid_cmpr_top->src_win_bgn_v = 0;
		vid_cmpr_top->src_win_end_v = vid_cmpr_top->src_vsize - 1;
		vid_cmpr_top->src_endian = input_vframe->canvas0_config[0].endian;
		vid_cmpr_top->src_block_mode = input_vframe->canvas0_config[0].block_mode;
		canvas_width = canvas_get_width(input_vframe->canvas0Addr & 0xff);
	} else {
		input_dma = data_config->input_data.data_dma;
		vicp_print(VICP_INFO, "%s: src_addr = 0x%lx.\n", __func__, input_dma->buf_addr);
		vid_cmpr_top->src_compress = 0;
		vid_cmpr_top->src_hsize = input_dma->data_width;
		vid_cmpr_top->src_vsize = input_dma->data_height;
		vid_cmpr_top->src_fmt_mode = input_dma->color_format;
		vid_cmpr_top->src_compbits = input_dma->color_depth;
		vid_cmpr_top->rdmif_canvas0_addr0 = input_dma->buf_addr;
		vid_cmpr_top->src_endian = 0;
		vid_cmpr_top->src_block_mode = 0;
		canvas_width = input_dma->buf_stride;
	}

	if (canvas_width % 32)
		vid_cmpr_top->src_burst_len = 0;
	else if (canvas_width % 64)
		vid_cmpr_top->src_burst_len = 1;

	if (vid_cmpr_top->src_block_mode)
		vid_cmpr_top->src_burst_len = vid_cmpr_top->src_block_mode;

	vid_cmpr_top->src_pip_src_mode = 0;
	vid_cmpr_top->rdmif_canvas0_addr1 = input_vframe->canvas0_config[1].phy_addr;
	vid_cmpr_top->rdmif_canvas0_addr2 = input_vframe->canvas0_config[2].phy_addr;

	if (vid_cmpr_top->src_fmt_mode == VICP_COLOR_FORMAT_YUV420)
		vid_cmpr_top->rdmif_separate_en = 2;
	else
		vid_cmpr_top->rdmif_separate_en = 0;

	vid_cmpr_top->hdr_en = 0;

	if (debug_axis_en) {
		data_config->data_option.output_axis.left = axis.left;
		data_config->data_option.output_axis.top = axis.top;
		data_config->data_option.output_axis.width = axis.width;
		data_config->data_option.output_axis.height = axis.height;
	} else {
		axis.left = data_config->data_option.output_axis.left;
		axis.top = data_config->data_option.output_axis.top;
		axis.width = data_config->data_option.output_axis.width;
		axis.height = data_config->data_option.output_axis.height;
	}

	vid_cmpr_top->out_afbce_enable = data_config->output_data.fbc_out_en;
	vid_cmpr_top->out_head_baddr = data_config->output_data.phy_addr[1];
	vid_cmpr_top->out_mmu_info_baddr = data_config->output_data.phy_addr[2];
	vid_cmpr_top->out_reg_init_ctrl = data_config->output_data.fbc_init_ctrl;
	vid_cmpr_top->out_reg_pip_mode = data_config->output_data.fbc_pip_mode;
	vid_cmpr_top->out_reg_format_mode = data_config->output_data.fbc_color_fmt;
	vid_cmpr_top->out_reg_compbits = data_config->output_data.fbc_color_dep;
	vid_cmpr_top->out_hsize_in = data_config->data_option.output_axis.width;
	vid_cmpr_top->out_vsize_in = data_config->data_option.output_axis.height;
	vid_cmpr_top->out_hsize_bgnd = data_config->output_data.width;
	vid_cmpr_top->out_vsize_bgnd = data_config->output_data.height;
	vid_cmpr_top->out_win_bgn_h = data_config->data_option.output_axis.left;
	vid_cmpr_top->out_win_end_h = vid_cmpr_top->out_win_bgn_h +
		data_config->data_option.output_axis.width - 1;
	vid_cmpr_top->out_win_bgn_v = data_config->data_option.output_axis.top;
	vid_cmpr_top->out_win_end_v = vid_cmpr_top->out_win_bgn_v +
		data_config->data_option.output_axis.height - 1;
	vid_cmpr_top->out_shrk_en = 1;
	vid_cmpr_top->out_shrk_mode = 1;

	rotation = data_config->data_option.rotation_mode;
	if (rotation == VICP_ROTATION_90) {
		vid_cmpr_top->out_rot_en = 1;
		vid_cmpr_top->rot_rev_mode = 2;
	} else if (rotation == VICP_ROTATION_180) {
		vid_cmpr_top->out_rot_en = 0;
		vid_cmpr_top->rot_rev_mode = 3;
	} else if (rotation == VICP_ROTATION_270) {
		vid_cmpr_top->out_rot_en = 1;
		vid_cmpr_top->rot_rev_mode = 1;
	} else if (rotation == VICP_ROTATION_MIRROR_H) {
		vid_cmpr_top->out_rot_en = 0;
		vid_cmpr_top->rot_rev_mode = 1;
	} else if (rotation == VICP_ROTATION_MIRROR_V) {
		vid_cmpr_top->out_rot_en = 0;
		vid_cmpr_top->rot_rev_mode = 2;
	} else {
		vicp_print(VICP_INFO, "%s: no need rotation.\n", __func__);
		vid_cmpr_top->out_rot_en = 0;
		vid_cmpr_top->rot_rev_mode = 0;
	}

	vid_cmpr_top->rot_hshrk_ratio = 0;
	vid_cmpr_top->rot_vshrk_ratio = 0;

	if (vid_cmpr_top->out_rot_en == 1)
		vid_cmpr_top->wrmif_en = 0;
	else
		vid_cmpr_top->wrmif_en = data_config->output_data.mif_out_en;
	vid_cmpr_top->wrmif_fmt_mode = data_config->output_data.mif_color_fmt;
	vid_cmpr_top->wrmif_bits_mode = data_config->output_data.mif_color_dep;
	vid_cmpr_top->wrmif_canvas0_addr0 = data_config->output_data.phy_addr[0];
	if (vid_cmpr_top->wrmif_fmt_mode == VICP_COLOR_FORMAT_YUV420) {
		vid_cmpr_top->wrmif_set_separate_en = 2;
		vid_cmpr_top->wrmif_canvas0_addr1 = vid_cmpr_top->wrmif_canvas0_addr0 +
			(data_config->output_data.width * data_config->output_data.height);
	} else {
		vid_cmpr_top->wrmif_set_separate_en = 0;
		vid_cmpr_top->wrmif_canvas0_addr1 = 0;
	}
	vid_cmpr_top->wrmif_canvas0_addr2 = 0;

	return 0;
}

int vicp_process_task(struct vid_cmpr_top_t *vid_cmpr_top)
{
	int ret = 0;
	int timeout = 0;

	vicp_print(VICP_INFO, "enter %s.\n", __func__);

	set_vid_cmpr(vid_cmpr_top);
	init_completion(&vicp_isr_done);
	set_module_enable(1);
	set_module_start(1);

	timeout = wait_for_completion_timeout(&vicp_isr_done, msecs_to_jiffies(200));
	if (!timeout)
		vicp_print(VICP_ERROR, "vicp_task wait isr timeout\n");

	return ret;
}

int vicp_process_reset(void)
{
	vicp_print(VICP_INFO, "enter %s.\n", __func__);
	return 0;
}

int  vicp_process(struct vicp_data_config_t *data_config)
{
	int ret = 0;
	struct vid_cmpr_top_t vid_cmpr_top;

	vicp_print(VICP_INFO, "enter %s.\n", __func__);
	mutex_lock(&vicp_mutex);
	if (IS_ERR_OR_NULL(data_config)) {
		vicp_print(VICP_ERROR, "%s: NULL param, please check.\n", __func__);
		mutex_unlock(&vicp_mutex);
		return -1;
	}

	memset(&vid_cmpr_top, 0, sizeof(struct vid_cmpr_top_t));
	ret = vicp_process_config(data_config, &vid_cmpr_top);
	if (ret < 0) {
		vicp_print(VICP_ERROR, "vicp config failed.\n");
		mutex_unlock(&vicp_mutex);
		return ret;
	}

	ret = vicp_process_task(&vid_cmpr_top);
	if (ret < 0)
		vicp_print(VICP_ERROR, "vicp task failed.\n");

	vicp_process_reset();
	mutex_unlock(&vicp_mutex);

	return  0;
}
EXPORT_SYMBOL(vicp_process);
