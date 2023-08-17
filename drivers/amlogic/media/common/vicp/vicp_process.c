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

#define ZOOM_BITS       20
#define PHASE_BITS      16

struct completion vicp_proc_done;
struct completion vicp_rdma_done;
static int rdma_done_count;
static int current_dump_flag;
static u32 last_input_w, last_input_h, last_input_fmt, last_input_bit_depth;
static u32 last_fbcinput_en;
static u32 last_output_w, last_output_h, last_output_fbcfmt, last_output_miffmt;
static u32 last_fbcoutput_en, last_mifoutput_en;
static u32 last_output_begin_v, last_output_begin_h, last_output_end_v, last_output_end_h;
static u32 rdma_buf_init_flag;
static struct vicp_device_data_s vicp_dev;

irqreturn_t vicp_isr_handle(int irq, void *dev_id)
{
	complete(&vicp_proc_done);
	vicp_print(VICP_INFO, "proc done.\n");
	return IRQ_HANDLED;
}

irqreturn_t vicp_rdma_handle(int irq, void *dev_id)
{
	rdma_done_count++;
	complete(&vicp_rdma_done);
	vicp_print(VICP_INFO, "rdma done, count is %d.\n", rdma_done_count);
	return IRQ_HANDLED;
}

void set_vid_cmpr_shrink(int is_enable, int size, int mode_h, int mode_v)
{
	if (print_flag & VICP_SHRINK) {
		pr_info("vicp: ##########shrink config##########\n");
		pr_info("vicp: shrink enable = %d.\n", is_enable);
		pr_info("vicp: shrink size = %d.\n", size);
		pr_info("vicp: shrink mode: h %d, v %d.\n", mode_h, mode_v);
		pr_info("vicp: #################################.\n");
	};

	set_wrmif_shrk_enable(is_enable);
	set_wrmif_shrk_size(size);
	set_wrmif_shrk_mode_h(mode_h);
	set_wrmif_shrk_mode_v(mode_v);
}

void set_vid_cmpr_afbce(int enable, struct vid_cmpr_afbce_s *afbce, bool rdma_en)
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
	int lossy_luma_en, lossy_chrm_en, lossy_cr_en;
	int reg_fmt444_comb;
	int uncmp_size;
	int uncmp_bits;
	int sblk_num;
	int mmu_page_size;
	struct vicp_afbce_mode_reg_s afbce_mode_reg;
	struct vicp_afbce_color_format_reg_s color_format_reg;
	struct vicp_afbce_mmu_rmif_control1_reg_s rmif_control1;
	struct vicp_afbce_mmu_rmif_control3_reg_s rmif_control3;
	struct vicp_afbce_pip_control_reg_s pip_control;
	struct vicp_afbce_enable_reg_s enable_reg;
	struct vicp_afbce_loss_ctrl_reg_s loss_ctrl_reg;
	struct vicp_afbce_format_reg_s format_reg;
	struct vicp_afbce_mif_size_reg_s mif_size_reg;

	if (IS_ERR_OR_NULL(afbce)) {
		vicp_print(VICP_ERROR, "%s: invalid param,return.\n", __func__);
		return;
	}

	if (print_flag & VICP_AFBCE) {
		pr_info("vicp: ##########fbc_out config##########\n");
		pr_info("vicp: headaddr: 0x%llx, table_addr: 0x%llx.\n", afbce->head_baddr,
			afbce->table_baddr);
		pr_info("vicp: pip: init_flag=%d, mode=%d.\n", afbce->reg_init_ctrl,
			afbce->reg_pip_mode);
		pr_info("vicp: reg_ram_comb: %d.\n", afbce->reg_ram_comb);
		pr_info("vicp: output_format_mode: %d.\n", afbce->reg_format_mode);
		pr_info("vicp: compbit: y %d, c %d.\n", afbce->reg_compbits_y,
			afbce->reg_compbits_c);
		pr_info("vicp: input_size: h %d, v %d.\n", afbce->hsize_in, afbce->vsize_in);
		pr_info("vicp: inputbuf_size: h %d, v %d.\n", afbce->hsize_bgnd, afbce->vsize_bgnd);
		pr_info("vicp: output axis: %d %d %d %d.\n", afbce->enc_win_bgn_h,
			afbce->enc_win_end_h, afbce->enc_win_bgn_v, afbce->enc_win_end_v);
		pr_info("vicp: rev_mode %d.\n", afbce->rev_mode);
		pr_info("vicp: default color:%d %d %d %d.\n", afbce->def_color_0,
			afbce->def_color_1, afbce->def_color_2, afbce->def_color_3);
		pr_info("vicp: force_444_comb = %d\n", afbce->force_444_comb);
		pr_info("vicp: rotation enable %d\n", afbce->rot_en);
		pr_info("vicp: din_swt = %d\n", afbce->din_swt);
		pr_info("vicp: lossy_compress: mode: %d.\n", afbce->lossy_compress.compress_mode);
		pr_info("vicp: lossy_compress: brst_len_add_en: %d, value: %d.\n",
			afbce->lossy_compress.brst_len_add_en,
			afbce->lossy_compress.brst_len_add_value);
		pr_info("vicp: lossy_compress: ofset_brst4_en = %d.\n",
			afbce->lossy_compress.ofset_brst4_en);
		pr_info("vicp: mmu_page_size = %d.\n", afbce->mmu_page_size);
		pr_info("vicp: #################################.\n");
	};

	if (!enable) {
		vicp_print(VICP_INFO, "%s: afbce disable.\n", __func__);
		memset(&enable_reg, 0, sizeof(struct vicp_afbce_enable_reg_s));
		enable_reg.enc_enable = 0;
		set_afbce_enable(enable_reg);
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

	mmu_page_size = afbce->mmu_page_size == 0 ? 4096 : 8192;

	if (afbce->lossy_compress.compress_mode == LOSSY_COMPRESS_MODE_QUAN_LUMA) {
		lossy_luma_en = 1;
		lossy_chrm_en = 0;
		lossy_cr_en = 0;
	} else if (afbce->lossy_compress.compress_mode == LOSSY_COMPRESS_MODE_QUAN_CHRMA) {
		lossy_luma_en = 0;
		lossy_chrm_en = 1;
		lossy_cr_en = 0;
	} else if (afbce->lossy_compress.compress_mode == LOSSY_COMPRESS_MODE_QUAN_ALL) {
		lossy_luma_en = 1;
		lossy_chrm_en = 1;
		lossy_cr_en = 0;
	} else if (afbce->lossy_compress.compress_mode == LOSSY_COMPRESS_MODE_CR) {
		lossy_luma_en = 0;
		lossy_chrm_en = 0;
		lossy_cr_en = 1;
	} else {
		lossy_luma_en = 0;
		lossy_chrm_en = 0;
		lossy_cr_en = 0;
	}

	if (rdma_en)
		hold_line_num = 1;
	else
		hold_line_num = 2;

	memset(&afbce_mode_reg, 0, sizeof(struct vicp_afbce_mode_reg_s));
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

	set_afbce_head_addr(afbce->head_baddr);
	set_afbce_pixel_in_scope(1, afbce->enc_win_bgn_h, afbce->enc_win_end_h);
	set_afbce_pixel_in_scope(0, afbce->enc_win_bgn_v, afbce->enc_win_end_v);
	set_afbce_conv_control(2048, 256);
	set_afbce_mix_scope(1, blk_out_bgn_h, blk_out_end_h);
	set_afbce_mix_scope(0, blk_out_bgn_v, blk_out_end_v);

	memset(&color_format_reg, 0, sizeof(struct vicp_afbce_color_format_reg_s));
	color_format_reg.format_mode = afbce->reg_format_mode;
	color_format_reg.compbits_c = afbce->reg_compbits_c;
	color_format_reg.compbits_y = afbce->reg_compbits_y;
	set_afbce_colorfmt(color_format_reg);

	set_afbce_default_color1(afbce->def_color_3, afbce->def_color_0);
	set_afbce_default_color2(afbce->def_color_1, afbce->def_color_2);
	set_afbce_mmu_rmif_control4(afbce->table_baddr);

	memset(&rmif_control1, 0, sizeof(struct vicp_afbce_mmu_rmif_control1_reg_s));
	rmif_control1.cmd_intr_len = 1;
	rmif_control1.cmd_req_size = 1;
	rmif_control1.burst_len = 2;
	rmif_control1.little_endian = 1;
	rmif_control1.pack_mode = 3;
	set_afbce_mmu_rmif_control1(rmif_control1);

	set_afbce_mmu_rmif_scope(1, 0, 0x1ffe);
	set_afbce_mmu_rmif_scope(0, 0, 1);

	memset(&rmif_control3, 0, sizeof(struct vicp_afbce_mmu_rmif_control3_reg_s));
	rmif_control3.vstep = 1;
	rmif_control3.acc_mode = 1;
	rmif_control3.stride = 0x1fff;
	set_afbce_mmu_rmif_control3(rmif_control3);

	memset(&pip_control, 0, sizeof(struct vicp_afbce_pip_control_reg_s));
	pip_control.enc_align_en = 1;
	pip_control.pip_ini_ctrl = afbce->reg_init_ctrl;
	pip_control.pip_mode = afbce->reg_pip_mode;
	set_afbce_pip_control(pip_control);

	set_afbce_rotation_control(afbce->rot_en, 8);

	memset(&format_reg, 0, sizeof(struct vicp_afbce_format_reg_s));
	if (afbce->reg_format_mode == 0 && afbce->reg_compbits_y == 12) {//44412bit
		format_reg.burst_length_add_en = 1;
		if (afbce->reg_pip_mode == 1)
			format_reg.burst_length_add_value = 3;
		else
			format_reg.burst_length_add_value = 2;
	} else {
		format_reg.burst_length_add_en = 0;
		format_reg.burst_length_add_value = 2;
	}

	if (vicp_dev.ddr16_support &&
		(afbce->head_baddr >= ADDR_VALUE_8G || afbce->table_baddr >= ADDR_VALUE_8G))
		format_reg.ofset_burst4_en = 1;
	else
		format_reg.ofset_burst4_en = 0;
	format_reg.format_mode = afbce->reg_format_mode;
	format_reg.compbits_c = afbce->reg_compbits_c;
	format_reg.compbits_y = afbce->reg_compbits_y;
	set_afbce_format(format_reg);

	if (vicp_dev.cr_lossy_support) {
		memset(&loss_ctrl_reg, 0, sizeof(struct vicp_afbce_loss_ctrl_reg_s));
		if (lossy_cr_en) {
			loss_ctrl_reg.fix_cr_en = lossy_cr_en;
			loss_ctrl_reg.rc_en = 1;
			loss_ctrl_reg.write_qlevel_mode = 1;
			loss_ctrl_reg.debug_qlevel_en = 0;
			loss_ctrl_reg.cal_bit_early_mode = 2;
			loss_ctrl_reg.quant_diff_root_leave = 2;
			loss_ctrl_reg.debug_qlevel_value = 0;
			loss_ctrl_reg.debug_qlevel_max_0 = 10;
			loss_ctrl_reg.debug_qlevel_max_1 = 10;
			set_afbce_lossy_control(loss_ctrl_reg);

			if (afbce->lossy_compress.compress_rate == 1)//67% compress
				set_afbce_lossy_brust_num(4, 4, 4, 4);
			else if (afbce->lossy_compress.compress_rate == 2)//83% compress
				set_afbce_lossy_brust_num(5, 5, 5, 5);
			else//default
				set_afbce_lossy_brust_num(7, 8, 7, 8);
		} else {
			set_afbce_lossy_control(loss_ctrl_reg);
		}
	} else {
		vicp_print(VICP_INFO, "don't support cr lossy compress.\n");
	}

	memset(&mif_size_reg, 0, sizeof(struct vicp_afbce_mif_size_reg_s));
	mif_size_reg.ddr_blk_size = 1;
	mif_size_reg.cmd_blk_size = 3;
	mif_size_reg.uncmp_size = uncmp_size;
	mif_size_reg.mmu_page_size = afbce->mmu_page_size == 0 ? 4096 : 8192;
	set_afbce_mif_size(mif_size_reg);

	memset(&enable_reg, 0, sizeof(struct vicp_afbce_enable_reg_s));
	enable_reg.gclk_ctrl = 0;
	enable_reg.afbce_sync_sel = 0;
	enable_reg.enc_rst_mode = 0;
	enable_reg.enc_en_mode = 1;
	enable_reg.enc_enable = enable;
	enable_reg.pls_enc_frm_start = 0;
	set_afbce_enable(enable_reg);
}

void f2v_get_vertical_phase(unsigned int zoom_ratio, enum f2v_vphase_type_e type,
	unsigned char bank_length, struct vid_cmpr_f2v_vphase_s *vphase)
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

void set_vid_cmpr_hdr(int hdr2_top_en)
{
	if (hdr2_top_en)
		vicp_hdr_set(vicp_hdr, 1);
	else
		set_hdr_enable(0);
}

static void set_vid_cmpr_fgrain(struct vid_cmpr_fgrain_s fgrain)
{
	struct vicp_fgrain_ctrl_reg_s fgrain_ctrl_reg;
	int window_v_bgn, window_v_end, window_h_bgn, window_h_end;
	u8 *temp_addr;
	int data_size = 0, i = 0;

	if (!vicp_dev.film_grain_support) {
		vicp_print(VICP_INFO, "don't support film grain.\n");
		return;
	}

	if (print_flag & VICP_FGRAIN) {
		pr_info("vicp: ##########fgrain config##########\n");
		pr_info("vicp: fgrain enable = %d.\n", fgrain.enable);
		pr_info("vicp: fgrain is_afbc = %d.\n", fgrain.is_afbc);
		pr_info("vicp: fgrain fmt: %d, depth: %d.\n", fgrain.color_fmt, fgrain.color_depth);
		pr_info("vicp: fgrain fgs_table_adr: 0x%llx.\n", fgrain.fgs_table_adr);
		pr_info("vicp: fgrain window: %d %d %d %d.\n", fgrain.start_x, fgrain.end_x,
			fgrain.start_y, fgrain.end_y);
		pr_info("vicp: #################################.\n");
	};

	memset(&fgrain_ctrl_reg, 0, sizeof(struct vicp_fgrain_ctrl_reg_s));
	if (!fgrain.enable)//off fgrain
		return set_fgrain_control(fgrain_ctrl_reg);

	fgrain_ctrl_reg.sync_ctrl = 0;
	fgrain_ctrl_reg.dma_st_clr = 0;
	fgrain_ctrl_reg.hold4dma_scale = 0;
	fgrain_ctrl_reg.hold4dma_tbl = 0;
	fgrain_ctrl_reg.cin_uv_swap = 0;
	fgrain_ctrl_reg.cin_rev = 0;
	fgrain_ctrl_reg.yin_rev = 0;
	fgrain_ctrl_reg.use_par_apply_fgrain = 0;
	if (fgrain.is_afbc == 0) {
		fgrain_ctrl_reg.fgrain_last_ln_mode = 1;
		fgrain_ctrl_reg.fgrain_ext_imode = 1;
	} else {
		fgrain_ctrl_reg.fgrain_last_ln_mode = 0;
		fgrain_ctrl_reg.fgrain_ext_imode = 1;
	}
	fgrain_ctrl_reg.fgrain_use_sat4bp = 0;
	fgrain_ctrl_reg.apply_c_mode = 1;
	fgrain_ctrl_reg.fgrain_tbl_sign_mode = 1;
	fgrain_ctrl_reg.fgrain_tbl_ext_mode = 1;
	fgrain_ctrl_reg.fmt_mode = fgrain.color_fmt;
	if (fgrain.color_depth == 8)
		fgrain_ctrl_reg.comp_bits = 0;
	else if (fgrain.color_depth == 10)
		fgrain_ctrl_reg.comp_bits = 1;
	else
		fgrain_ctrl_reg.comp_bits = 2;
	fgrain_ctrl_reg.rev_mode = 0;
	fgrain_ctrl_reg.block_mode = fgrain.is_afbc;
	fgrain_ctrl_reg.fgrain_loc_en = 1;
	fgrain_ctrl_reg.fgrain_glb_en = 1;
	set_fgrain_control(fgrain_ctrl_reg);

	if (fgrain.is_afbc == 0) {//rdmif
		window_v_bgn = fgrain.start_y;
		window_v_end = fgrain.end_y;
		window_h_bgn = fgrain.start_x;
		window_h_end = fgrain.end_x;
	} else {//afbcd
		window_v_bgn = (fgrain.start_y) / 4 * 4;
		window_v_end = (fgrain.end_y + 3) / 4 * 4 - 4;
		window_h_bgn = (fgrain.start_x) / 32 * 32;
		window_h_end = (fgrain.end_x + 1 + 31) / 32 * 32 - 32;
	}

	set_fgrain_window_v(window_v_bgn, window_v_end);
	set_fgrain_window_h(window_h_bgn, window_h_end);
	set_fgrain_slice_window_h(0, window_h_end - window_h_bgn);

	data_size = FILM_GRAIN_LUT_DATA_SIZE * sizeof(int);
	temp_addr = codec_mm_vmap(fgrain.fgs_table_adr, data_size);
	codec_mm_dma_flush(temp_addr, data_size * sizeof(int), DMA_FROM_DEVICE);

	for (i = 0; i < FILM_GRAIN_LUT_DATA_SIZE; i++)
		set_fgrain_lut_data(*(u32 *)(temp_addr + i));
	codec_mm_unmap_phyaddr(temp_addr);
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

void set_vid_cmpr_scale(int is_enable, struct vid_cmpr_scaler_s *scaler)
{
	enum f2v_vphase_type_e top_conv_type;
	enum f2v_vphase_type_e bot_conv_type;
	struct vid_cmpr_f2v_vphase_s vphase;
	int topbot_conv;
	int top_conv, bot_conv;

	int hsc_en = 0;
	int vsc_en = 0;
	int coef_s_bits = 0;
	u32 p_src_w, p_src_h;
	u32 vert_phase_step, horz_phase_step;
	unsigned char top_rcv_num, bot_rcv_num;
	unsigned char top_rpt_num, bot_rpt_num;
	unsigned short top_vphase, bot_vphase;
	unsigned char is_frame;
	int blank_len;
	/*0:vd1_s0 1:vd1_s1 2:vd1_s2 3:vd1_s3 4:vd2 5:vid_cmp 6:RESHAPE*/
	int index;
	struct vicp_pre_scaler_ctrl_reg_s pre_scaler_ctrl_reg;
	struct vicp_vsc_phase_ctrl_reg_s vsc_phase_ctrl_reg;
	struct vicp_hsc_phase_ctrl_reg_s hsc_phase_ctrl_reg;
	struct vicp_scaler_misc_reg_s scaler_misc_reg;

	if (IS_ERR_OR_NULL(scaler)) {
		vicp_print(VICP_ERROR, "%s: invalid param,return.\n", __func__);
		return;
	}

	if (print_flag & VICP_SCALER) {
		pr_info("vicp: ##########scaler config##########\n");
		pr_info("vicp: input size: h %d, v %d.\n", scaler->din_hsize, scaler->din_vsize);
		pr_info("vicp: output size: h %d, v %d.\n", scaler->dout_hsize, scaler->dout_vsize);
		pr_info("vicp: vert_bank_length = %d.\n", scaler->vert_bank_length);
		pr_info("vicp: pre_scaler_en: h %d, v %d.\n", scaler->prehsc_en, scaler->prevsc_en);
		pr_info("vicp: pre_scaler_rate: h %d, v %d.\n", scaler->prehsc_rate,
			scaler->prevsc_rate);
		pr_info("vicp: high_res_coef_en = %d.\n", scaler->high_res_coef_en);
		pr_info("vicp: phase_step: h %d, v %d.\n", scaler->horz_phase_step,
			scaler->vert_phase_step);
		pr_info("vicp: slice_start = %d\n", scaler->slice_x_st);
		pr_info("vicp: slice_end: %d %d %d %d.\n",
			scaler->slice_x_end[0],
			scaler->slice_x_end[1],
			scaler->slice_x_end[2],
			scaler->slice_x_end[3]);
		pr_info("vicp: pps_slice = %d..\n", scaler->pps_slice);
		pr_info("vicp: phase_step_en %d, step %d.\n", scaler->phase_step_en,
			scaler->phase_step);
		pr_info("vicp: #################################.\n");
	};

	index = scaler->device_index;
	topbot_conv = index >> 16;
	top_conv = (topbot_conv >> 4) & 0xf;
	bot_conv = topbot_conv & 0xf;
	index = index & 0xffff;
	top_conv_type = scaler->vphase_type_top;
	bot_conv_type = scaler->vphase_type_bot;

	if (top_conv != 0)
		top_conv_type = (enum f2v_vphase_type_e)(top_conv - 1);
	if (bot_conv != 0)
		bot_conv_type = (enum f2v_vphase_type_e)(bot_conv - 1);

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

	memset(&pre_scaler_ctrl_reg, 0, sizeof(struct vicp_pre_scaler_ctrl_reg_s));
	pre_scaler_ctrl_reg.preh_hb_num = 8;
	pre_scaler_ctrl_reg.preh_vb_num = 8;
	pre_scaler_ctrl_reg.sc_coef_s11_mode = scaler->high_res_coef_en;
	pre_scaler_ctrl_reg.vsc_nor_rs_bits = coef_s_bits;
	pre_scaler_ctrl_reg.hsc_nor_rs_bits = coef_s_bits;
	pre_scaler_ctrl_reg.prehsc_flt_num = 4;
	pre_scaler_ctrl_reg.prevsc_flt_num = 4;
	pre_scaler_ctrl_reg.prehsc_rate = scaler->prehsc_rate;
	pre_scaler_ctrl_reg.prevsc_rate = scaler->prevsc_rate;
	set_pre_scaler_control(pre_scaler_ctrl_reg);

	set_scaler_coef_param(scaler->high_res_coef_en);

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

	set_vsc_region12_start(0, 0);
	set_vsc_region34_start(scaler->dout_vsize, scaler->dout_vsize);
	set_vsc_region4_end(scaler->dout_vsize - 1);

	set_vsc_start_phase_step(vert_phase_step);
	set_vsc_region_phase_slope(0, 0);
	set_vsc_region_phase_slope(1, 0);
	set_vsc_region_phase_slope(3, 0);
	set_vsc_region_phase_slope(4, 0);

	memset(&vsc_phase_ctrl_reg, 0, sizeof(struct vicp_vsc_phase_ctrl_reg_s));
	vsc_phase_ctrl_reg.double_line_mode = 0;
	vsc_phase_ctrl_reg.prog_interlace = (!is_frame);
	vsc_phase_ctrl_reg.bot_l0_out_en = 0;
	vsc_phase_ctrl_reg.bot_rpt_l0_num = bot_rpt_num;
	vsc_phase_ctrl_reg.bot_ini_rcv_num = bot_rcv_num;
	vsc_phase_ctrl_reg.top_l0_out_en = 0;
	vsc_phase_ctrl_reg.top_rpt_l0_num = top_rpt_num;
	vsc_phase_ctrl_reg.top_ini_rcv_num = top_rcv_num;
	set_vsc_phase_control(vsc_phase_ctrl_reg);

	set_vsc_ini_phase(bot_vphase, top_vphase);
	set_hsc_region12_start(0, 0);
	set_hsc_region34_start(scaler->dout_hsize, scaler->dout_hsize);
	set_hsc_region4_end(scaler->dout_hsize - 1);
	set_hsc_start_phase_step(horz_phase_step);
	set_hsc_region_phase_slope(0, 0);
	set_hsc_region_phase_slope(1, 0);
	set_hsc_region_phase_slope(3, 0);
	set_hsc_region_phase_slope(4, 0);

	memset(&hsc_phase_ctrl_reg, 0, sizeof(struct vicp_hsc_phase_ctrl_reg_s));
	hsc_phase_ctrl_reg.ini_rcv_num0_exp = 0;
	hsc_phase_ctrl_reg.rpt_p0_num0 = 3;
	hsc_phase_ctrl_reg.ini_rcv_num0 = 8;
	hsc_phase_ctrl_reg.ini_phase0 = 0;
	set_hsc_phase_control(hsc_phase_ctrl_reg);

	memset(&scaler_misc_reg, 0, sizeof(struct vicp_scaler_misc_reg_s));
	scaler_misc_reg.sc_din_mode = 0;
	scaler_misc_reg.reg_l0_out_fix = 0;
	scaler_misc_reg.hf_sep_coef_4srnet_en = 0;
	scaler_misc_reg.repeat_last_line_en = scaler->prevsc_en;
	scaler_misc_reg.old_prehsc_en = 0;
	scaler_misc_reg.hsc_len_div2_en = 0;
	scaler_misc_reg.prevsc_lbuf_mode = scaler->prevsc_en;
	scaler_misc_reg.prehsc_en = scaler->prehsc_en;
	scaler_misc_reg.prevsc_en = scaler->prevsc_en;
	scaler_misc_reg.vsc_en = vsc_en;
	scaler_misc_reg.hsc_en = hsc_en;
	scaler_misc_reg.sc_top_en = is_enable;
	scaler_misc_reg.sc_vd_en = 1;
	scaler_misc_reg.hsc_nonlinear_4region_en = 0;
	scaler_misc_reg.hsc_bank_length = 8;
	scaler_misc_reg.vsc_phase_field_mode = 0;
	scaler_misc_reg.vsc_nonlinear_4region_en = 0;
	scaler_misc_reg.vsc_bank_length = scaler->vert_bank_length;
	set_scaler_misc(scaler_misc_reg);
}

void set_vid_cmpr_afbcd(int hold_line_num, bool rdma_en, struct vid_cmpr_afbcd_s *afbcd)
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
	int lossy_luma_en, lossy_chrm_en, fix_cr_en;
	struct vicp_afbcd_mode_reg_s afbcd_mode;
	struct vicp_afbcd_general_reg_s afbcd_general;
	struct vicp_afbcd_cfmt_control_reg_s cfmt_control;
	struct vicp_afbcd_quant_control_reg_s quant_control;
	struct vicp_afbcd_rotate_control_reg_s rotate_control;
	struct vicp_afbcd_rotate_scope_reg_s rotate_scope;
	struct vicp_afbcd_lossy_ctrl_reg_s lossy_ctrl_reg;
	struct vicp_afbcd_burst_ctrl_reg_s burst_ctrl_reg;

	if (IS_ERR_OR_NULL(afbcd)) {
		vicp_print(VICP_ERROR, "%s: invalid param,return.\n", __func__);
		return;
	}

	if (print_flag & VICP_AFBCD) {
		pr_info("vicp: ##########fbc_in config##########\n");
		pr_info("vicp: blk_mem_mode = %d.\n", afbcd->blk_mem_mode);
		pr_info("vicp: index = %d.\n", afbcd->index);
		pr_info("vicp: size: h %d, v %d.\n", afbcd->hsize, afbcd->vsize);
		pr_info("vicp: addr: head:0x%x, body:0x%x.\n", afbcd->head_baddr,
			afbcd->body_baddr);
		pr_info("vicp: compbits: y:%d, u:%d, v:%d.\n", afbcd->compbits_y, afbcd->compbits_u,
			afbcd->compbits_v);
		pr_info("vicp: input_color_format = %d.\n", afbcd->fmt_mode);
		pr_info("vicp: ddr_sz_mode = %d.\n", afbcd->ddr_sz_mode);
		pr_info("vicp: fmt444_comb = %d.\n", afbcd->fmt444_comb);
		pr_info("vicp: v_skip: y:%d, uv:%d.\n", afbcd->v_skip_y, afbcd->v_skip_uv);
		pr_info("vicp: h_skip: y:%d, uv:%d.\n", afbcd->h_skip_y, afbcd->h_skip_uv);
		pr_info("vicp: rev_mode %d.\n", afbcd->rev_mode);
		pr_info("vicp: default_color: y:%d, u:%d, v:%d.\n", afbcd->def_color_y,
			afbcd->def_color_u, afbcd->def_color_v);
		pr_info("vicp: window_axis: %d %d %d %d.\n", afbcd->win_bgn_h, afbcd->win_end_h,
			afbcd->win_bgn_v, afbcd->win_end_v);
		pr_info("vicp: ini_phase: h:%d, v:%d.\n", afbcd->hz_ini_phase, afbcd->vt_ini_phase);
		pr_info("vicp: rpt_fst0_en: h:%d, v:%d.\n", afbcd->hz_rpt_fst0_en,
			afbcd->vt_rpt_fst0_en);
		pr_info("vicp: rotation_en = %d.\n", afbcd->rot_en);
		pr_info("vicp: rotation_begin: h:%d, v:%d.\n", afbcd->rot_hbgn, afbcd->rot_vbgn);
		pr_info("vicp: rotation_shrink: h:%d, v:%d.\n", afbcd->rot_hshrk, afbcd->rot_vshrk);
		pr_info("vicp: rotation_drop_mode = %d.\n", afbcd->rot_drop_mode);
		pr_info("vicp: rotation_output_format = %d.\n", afbcd->rot_ofmt_mode);
		pr_info("vicp: rotation_output_compbits = %d.\n", afbcd->rot_ocompbit);
		pr_info("vicp: pip_src_mode = %d.\n", afbcd->pip_src_mode);
		pr_info("vicp: lossy_compress: mode: %d.\n", afbcd->lossy_compress.compress_mode);
		pr_info("vicp: lossy_compress: brst_len_add_en: %d, value: %d.\n",
			afbcd->lossy_compress.brst_len_add_en,
			afbcd->lossy_compress.brst_len_add_value);
		pr_info("vicp: lossy_compress: ofset_brst4_en = %d.\n",
			afbcd->lossy_compress.ofset_brst4_en);
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

	if (rdma_en)
		hold_line_num = 1;
	else
		hold_line_num = hold_line_num > 4 ? hold_line_num - 4 : 0;

	memset(&afbcd_mode, 0, sizeof(struct vicp_afbcd_mode_reg_s));
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
	set_afbcd_headaddr(afbcd->head_baddr);
	set_afbcd_bodyaddr(afbcd->body_baddr);
	set_afbcd_mif_scope(0, mif_blk_bgn_h, mif_blk_end_h);
	set_afbcd_mif_scope(1, mif_blk_bgn_v, mif_blk_end_v);
	set_afbcd_pixel_scope(0, dec_pixel_bgn_h, dec_pixel_end_h);
	set_afbcd_pixel_scope(1, dec_pixel_bgn_v, dec_pixel_end_v);

	memset(&afbcd_general, 0, sizeof(struct vicp_afbcd_general_reg_s));
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

	if (afbcd->lossy_compress.compress_mode == LOSSY_COMPRESS_MODE_QUAN_LUMA) {
		lossy_luma_en = 1;
		lossy_chrm_en = 0;
		fix_cr_en = 0;
	} else if (afbcd->lossy_compress.compress_mode == LOSSY_COMPRESS_MODE_QUAN_CHRMA) {
		lossy_luma_en = 0;
		lossy_chrm_en = 1;
		fix_cr_en = 0;
	} else if (afbcd->lossy_compress.compress_mode == LOSSY_COMPRESS_MODE_QUAN_ALL) {
		lossy_luma_en = 1;
		lossy_chrm_en = 1;
		fix_cr_en = 0;
	} else if (afbcd->lossy_compress.compress_mode == LOSSY_COMPRESS_MODE_CR) {
		lossy_luma_en = 0;
		lossy_chrm_en = 0;
		fix_cr_en = 1;
	} else {
		lossy_luma_en = 0;
		lossy_chrm_en = 0;
		fix_cr_en = 0;
	}

	memset(&cfmt_control, 0, sizeof(struct vicp_afbcd_cfmt_control_reg_s));
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

	memset(&quant_control, 0, sizeof(struct vicp_afbcd_quant_control_reg_s));
	quant_control.lossy_chrm_en = lossy_chrm_en;
	quant_control.lossy_luma_en = lossy_luma_en;
	set_afbcd_quant_control(quant_control);

	memset(&rotate_control, 0, sizeof(struct vicp_afbcd_rotate_control_reg_s));
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

	memset(&rotate_scope, 0, sizeof(struct vicp_afbcd_rotate_scope_reg_s));
	rotate_scope.in_fmt_force444 = 1;
	rotate_scope.out_fmt_mode = afbcd->rot_ofmt_mode;
	rotate_scope.out_compbits_y = afbcd->rot_ocompbit;
	rotate_scope.out_compbits_uv = afbcd->rot_ocompbit;
	rotate_scope.win_bgn_v = afbcd->rot_vbgn;
	rotate_scope.win_bgn_h = afbcd->rot_hbgn;
	set_afbcd_rotate_scope(rotate_scope);

	memset(&lossy_ctrl_reg, 0, sizeof(struct vicp_afbcd_lossy_ctrl_reg_s));
	lossy_ctrl_reg.fix_cr_en = fix_cr_en;
	if (fix_cr_en)
		lossy_ctrl_reg.quant_diff_root_leave = afbcd->lossy_compress.quant_diff_root_leave;
	else
		lossy_ctrl_reg.quant_diff_root_leave = 2;
	set_afbcd_loss_control(lossy_ctrl_reg);

	if (fix_cr_en) {
		memset(&burst_ctrl_reg, 0, sizeof(struct vicp_afbcd_burst_ctrl_reg_s));
		burst_ctrl_reg.ofset_burst4_en = afbcd->lossy_compress.ofset_brst4_en;
		burst_ctrl_reg.burst_length_add_en = afbcd->lossy_compress.brst_len_add_en;
		burst_ctrl_reg.burst_length_add_value = afbcd->lossy_compress.brst_len_add_value;
	} else {
		burst_ctrl_reg.ofset_burst4_en = 0;
		burst_ctrl_reg.burst_length_add_en = 0;
		burst_ctrl_reg.burst_length_add_value = 2;
	}
	set_afbcd_burst_control(burst_ctrl_reg);
}

void set_vid_cmpr_crop(struct vid_cmpr_crop_s *crop_param)
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

void set_mif_stride(struct vid_cmpr_mif_s *mif, int *stride_y, int *stride_cb, int *stride_cr)
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
		comp_bits = 10; /*di output(full pack) 422 10 bit*/
	else
		comp_bits = 10;

	/*00: 4:4:4; 01: 4:2:2; 10: 4:2:0*/
	if (mif->fmt_mode == 0)
		comp_num = 3;
	else
		comp_num = 2;

	vicp_print(VICP_RDMIF | VICP_WRMIF, "%s: pic_hsize = %d.\n", __func__, pic_hsize);
	vicp_print(VICP_RDMIF | VICP_WRMIF, "%s: comp_bits = %d.\n", __func__, comp_bits);
	vicp_print(VICP_RDMIF | VICP_WRMIF, "%s: comp_num = %d.\n", __func__, comp_num);

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

	vicp_print(VICP_RDMIF | VICP_WRMIF, "%s: burst_stride_0 = %d.\n", __func__, burst_stride_0);
	vicp_print(VICP_RDMIF | VICP_WRMIF, "%s: burst_stride_1 = %d.\n", __func__, burst_stride_1);
	vicp_print(VICP_RDMIF | VICP_WRMIF, "%s: burst_stride_2 = %d.\n", __func__, burst_stride_2);

	/*now y cb cr all need 64bytes aligned */
	*stride_y = ((burst_stride_0 + 3) >> 2) << 2;
	*stride_cb = ((burst_stride_1 + 3) >> 2) << 2;
	*stride_cr = ((burst_stride_2 + 3) >> 2) << 2;

	vicp_print(VICP_RDMIF | VICP_WRMIF, "%s, stride_y: %d.\n", __func__, *stride_y);
	vicp_print(VICP_RDMIF | VICP_WRMIF, "%s: stride_cb = %d.\n", __func__, *stride_cb);
	vicp_print(VICP_RDMIF | VICP_WRMIF, "%s: stride_cr = %d.\n", __func__, *stride_cr);
};

void set_vid_cmpr_rmif(struct vid_cmpr_mif_s *rd_mif, int urgent, int hold_line)
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
	struct vicp_rdmif_general_reg_s general_reg;
	struct vicp_rdmif_general_reg2_s general_reg2;
	struct vicp_rdmif_general_reg3_s general_reg3;
	struct vicp_rdmif_rpt_loop_s rpt_loop;
	struct vicp_rdmif_color_format_s color_format;

	if (IS_ERR_OR_NULL(rd_mif)) {
		vicp_print(VICP_ERROR, "%s: invalid param,return.\n", __func__);
		return;
	}

	if (print_flag & VICP_RDMIF) {
		pr_info("vicp: ##########mif_in config##########\n");
		pr_info("vicp: luma_x axis: %d, %d.\n", rd_mif->luma_x_start0, rd_mif->luma_x_end0);
		pr_info("vicp: luma_y axis: %d, %d.\n", rd_mif->luma_y_start0, rd_mif->luma_y_end0);
		pr_info("vicp: chrm_x axis: %d, %d.\n", rd_mif->chrm_x_start0, rd_mif->chrm_x_end0);
		pr_info("vicp: chrm_y axis: %d, %d.\n", rd_mif->chrm_y_start0, rd_mif->chrm_y_end0);
		pr_info("vicp: set_separate_en = %d.\n", rd_mif->set_separate_en);
		pr_info("vicp: src_field_mode = %d.\n", rd_mif->src_field_mode);
		pr_info("vicp: input_fmt = %d.\n", rd_mif->fmt_mode);
		pr_info("vicp: output_field_num = %d.\n", rd_mif->output_field_num);
		pr_info("vicp: input_color_dep = %d.\n", rd_mif->bits_mode);
		pr_info("vicp: stride: y %d, u %d, v %d.\n", rd_mif->burst_size_y,
			rd_mif->burst_size_cb, rd_mif->burst_size_cr);
		pr_info("vicp: input addr0: 0x%llx\n", rd_mif->canvas0_addr0);
		pr_info("vicp: input addr1: 0x%llx\n", rd_mif->canvas0_addr1);
		pr_info("vicp: input addr2: 0x%llx\n", rd_mif->canvas0_addr2);
		pr_info("vicp: stride: y:%d, cb:%d, cr:%d.\n", rd_mif->stride_y, rd_mif->stride_cb,
			rd_mif->stride_cr);
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
			chroma0_rpt_loop_pat = 0x8;
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
			cntl_bits_mode = 3; /*422 10 bit full pack*/
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

	memset(&general_reg3, 0x0, sizeof(struct vicp_rdmif_general_reg3_s));
	general_reg3.bits_mode = cntl_bits_mode;
	general_reg3.block_len = 3;
	general_reg3.burst_len = rd_mif->burst_len;
	general_reg3.bit_swap = bit_swap;

	general_reg3.f0_cav_blk_mode2 = rd_mif->block_mode;
	general_reg3.f0_cav_blk_mode1 = rd_mif->block_mode;
	general_reg3.f0_cav_blk_mode0 = rd_mif->block_mode;

	if (rd_mif->block_mode) {
		general_reg3.f0_stride32aligned2 = 1;
		general_reg3.f0_stride32aligned1 = 1;
		general_reg3.f0_stride32aligned1 = 1;
	}

	set_rdmif_general_reg3(general_reg3);

	memset(&general_reg, 0x0, sizeof(struct vicp_rdmif_general_reg_s));
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

	memset(&general_reg2, 0x0, sizeof(struct vicp_rdmif_general_reg2_s));
	if (rd_mif->set_separate_en == 2)
		general_reg2.color_map = 1;
	else
		general_reg2.color_map = 0;
	general_reg2.x_rev0 = rd_mif->rev_x;
	general_reg2.y_rev0 = rd_mif->rev_x;
	set_rdmif_general_reg2(general_reg2);

	set_rdmif_base_addr(RDMIF_BASEADDR_TYPE_Y, rd_mif->canvas0_addr0);
	set_rdmif_base_addr(RDMIF_BASEADDR_TYPE_CB, rd_mif->canvas0_addr1);
	set_rdmif_base_addr(RDMIF_BASEADDR_TYPE_CR, rd_mif->canvas0_addr2);
	set_rdmif_stride(RDMIF_STRIDE_TYPE_Y, (rd_mif->stride_y + 15) >> 4);
	set_rdmif_stride(RDMIF_STRIDE_TYPE_CB, (rd_mif->stride_cb + 15) >> 4);
	set_rdmif_stride(RDMIF_STRIDE_TYPE_CR, (rd_mif->stride_cr + 15) >> 4);

	set_rdmif_luma_position(0, 1, rd_mif->luma_x_start0, rd_mif->luma_x_end0);
	set_rdmif_luma_position(0, 0, rd_mif->luma_y_start0, rd_mif->luma_y_end0);
	set_rdmif_chroma_position(0, 1, rd_mif->chrm_x_start0, rd_mif->chrm_x_end0);
	set_rdmif_chroma_position(0, 0, rd_mif->chrm_y_start0, rd_mif->chrm_y_end0);

	memset(&rpt_loop, 0, sizeof(struct vicp_rdmif_rpt_loop_s));
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
		y_length = rd_mif->luma_x_end0 - rd_mif->luma_x_start0 + 1;
		c_length = rd_mif->chrm_x_end0 - rd_mif->chrm_x_start0 + 1;
		hz_rpt = 0;
	} else if (rd_mif->fmt_mode == 1) {
		hfmt_en = 1;
		hz_yc_ratio = 1;
		hz_ini_phase = 0;
		vfmt_en = 0;
		vt_yc_ratio = 0;
		y_length = rd_mif->luma_x_end0 - rd_mif->luma_x_start0 + 1;
		c_length = ((rd_mif->luma_x_end0 >> 1) - (rd_mif->luma_x_start0 >> 1) + 1);
		hz_rpt	 = 0;
	} else if (rd_mif->fmt_mode == 0) {
		hfmt_en = 0;
		hz_yc_ratio = 1;
		hz_ini_phase = 0;
		vfmt_en = 0;
		vt_yc_ratio = 0;
		y_length = rd_mif->luma_x_end0 - rd_mif->luma_x_start0 + 1;
		c_length = rd_mif->luma_x_end0 - rd_mif->luma_x_start0 + 1;
		hz_rpt = 0;
	}

	vt_ini_phase = 0xc;
	if (rd_mif->src_field_mode == 1) {
		if (rd_mif->output_field_num == 0)
			vt_ini_phase = 0xe; /*interlace top*/
		else if (rd_mif->output_field_num == 1)
			vt_ini_phase = 0xa; /*interlace bottom*/
	}

	memset(&color_format, 0, sizeof(struct vicp_rdmif_color_format_s));
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

void set_vid_cmpr_wmif(struct vid_cmpr_mif_s *wr_mif, int wrmif_en)
{
	u32 stride_y, stride_cb, stride_cr, rgb_mode, bit10_mode;
	struct vicp_wrmif_control_s wrmif_control;

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
		pr_info("vicp: hsize = %d, vsize = %d.\n", wr_mif->buf_hsize, wr_mif->buf_vsize);
		pr_info("vicp: endian = %d.\n", wr_mif->endian);
		pr_info("vicp: block_mode = %d.\n", wr_mif->block_mode);
		pr_info("vicp: burst_len = %d.\n", wr_mif->burst_len);
		pr_info("vicp: swap_cbcr = %d.\n", wr_mif->swap_cbcr);
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

	memset(&wrmif_control, 0, sizeof(struct vicp_wrmif_control_s));
	wrmif_control.swap_64bits_en = 0;
	wrmif_control.burst_limit = 2;
	wrmif_control.canvas_sync_en = 0;
	wrmif_control.gate_clock_en = 0;
	wrmif_control.rgb_mode = rgb_mode;
	wrmif_control.h_conv = 0;
	wrmif_control.v_conv = 0;
	wrmif_control.swap_cbcr = wr_mif->swap_cbcr;/*1 NV12, 0 NV21*/
	wrmif_control.urgent = 0;
	wrmif_control.word_limit = 4;
	wrmif_control.data_ext_ena = 0;
	wrmif_control.little_endian = wr_mif->endian;
	wrmif_control.bit10_mode = bit10_mode;
	wrmif_control.wrmif_en = wrmif_en;
	set_wrmif_control(wrmif_control);
};

static void set_vid_cmpr_rdma_config(bool rdma_en, int input_count, int input_number)
{
	if (print_flag & VICP_RDMA) {
		pr_info("vicp: ##########RDMA config##########\n");
		pr_info("vicp: rdma enable: %d.\n", rdma_en);
		pr_info("vicp: input_count: %d.\n", input_count);
		pr_info("vicp: input_number: %d.\n", input_number);
	}

	if (!rdma_en) {
		vicp_print(VICP_RDMA, "%s: rdma disabled.\n", __func__);
		set_rdma_flag(0);
		vicp_rdma_enable(0, 0, 0);
		vicp_rdma_errorflag_clear();
		return;
	}

	if (input_count <= 0 || input_count > MAX_INPUTSOURCE_COUNT) {
		vicp_print(VICP_ERROR, "%s: invalid param.\n", __func__);
		set_rdma_flag(0);
		vicp_rdma_enable(0, 0, 0);
		return;
	}

	set_rdma_flag(1);
	if (input_count != 0 && input_number == 0) {//first enter
		vicp_print(VICP_RDMA, "%s: %d line.\n", __func__, __LINE__);
		vicp_rdma_enable(1, 0, 0);
		vicp_rdma_reset();
		vicp_rdma_trigger();
		vicp_rdma_errorflag_clear();
		set_rdma_start(input_count);
	}

	set_vicp_rdma_buf_choice(input_number);
}

static void set_vid_cmpr_security(bool sec_en)
{
	u32 dma_sec = 0, mmu_sec = 0, input_sec = 0;

	vicp_print(VICP_INFO, "enter %s, sec_en = %d.\n", __func__, sec_en);

	if (sec_en) {
		input_sec = 1;
	} else {
		dma_sec = 0;
		mmu_sec = 0;
		input_sec = 0;
	}

	return set_security_enable(dma_sec, mmu_sec, input_sec);
}

static void set_vid_cmpr_crc(int rotation_en)
{
	struct vicp_crc_reg_t crc_reg;

	memset(&crc_reg, 0, sizeof(struct vicp_crc_reg_t));

	if (rotation_en) {
		crc_reg.crc_check_en = 3;
		crc_reg.crc_start = 3;
		crc_reg.crc_sec_sel = 0;
	} else {
		crc_reg.crc_check_en = 1;
		crc_reg.crc_start = 1;
		crc_reg.crc_sec_sel = 0;
	}

	return set_crc_control(crc_reg);
}

static void dump_dma(int flag, struct dma_data_config_s *dma_data)
{
	struct file *fp = NULL;
	char name_buf[32];
	int data_size;
	u8 *data_addr;
	mm_segment_t fs;
	loff_t pos;

	if (IS_ERR_OR_NULL(dma_data)) {
		vicp_print(VICP_ERROR, "%s: invalid param,return.\n", __func__);
		return;
	}

	if (flag == 0) {
		snprintf(name_buf, sizeof(name_buf), "/data/in_%d_%d_nv12.yuv",
			dma_data->data_width, dma_data->data_height);
	} else {
		snprintf(name_buf, sizeof(name_buf), "/data/out_%d_%d_nv12.yuv",
			dma_data->data_width, dma_data->data_height);
	}
	data_size = dma_data->buf_stride_w * dma_data->buf_stride_h * 3 / 2;
	data_addr = codec_mm_vmap(dma_data->buf_addr, data_size);
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

static void dump_output(struct output_data_param_s *output_data_param)
{
	struct dma_data_config_s dma_data;

	if (IS_ERR_OR_NULL(output_data_param)) {
		vicp_print(VICP_ERROR, "%s: invalid param,return.\n", __func__);
		return;
	}

	memset(&dma_data, 0, sizeof(struct dma_data_config_s));
	dma_data.buf_addr = output_data_param->phy_addr[0];
	dma_data.buf_stride_w = output_data_param->width;
	dma_data.buf_stride_h = output_data_param->height;
	dma_data.data_width = output_data_param->width;
	dma_data.data_height =  output_data_param->height;

	dump_dma(1, &dma_data);
}

static int get_input_color_bitdepth(struct vframe_s *vf)
{
	int bitdepth = 0;

	if (IS_ERR_OR_NULL(vf)) {
		vicp_print(VICP_ERROR, "%s: NULL param.\n", __func__);
		return -1;
	}

	if (vf->bitdepth & BITDEPTH_Y9)
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

static int init_rdma_buf(void)
{
	int buf_size = 0;
	int num = 0;
	u64 cmd_buf_addr, load_buf_addr;
	ulong addr_start;

	if (rdma_buf_init_flag) {
		vicp_print(VICP_RDMA, "%s: no need init again.\n", __func__);
		return 0;
	}

	buf_size = MAX_INPUTSOURCE_COUNT * (RDMA_CMD_BUF_LEN + RDMA_LOAD_BUF_LEN);
	addr_start = codec_mm_alloc_for_dma("vicp", buf_size / PAGE_SIZE, 0, CODEC_MM_FLAGS_DMA);
	for (num = 0; num < MAX_INPUTSOURCE_COUNT; num++) {
		cmd_buf_addr = addr_start + (RDMA_CMD_BUF_LEN + RDMA_LOAD_BUF_LEN) * num;
		load_buf_addr = cmd_buf_addr + RDMA_CMD_BUF_LEN;
		vicp_rdma_buf_init(num, cmd_buf_addr, load_buf_addr);
	}

	rdma_buf_init_flag = 1;
	return 0;
}

static int uninit_rdma_buf(void)
{
	int buf_size = 0;
	u8 *temp_addr;
	ulong rdma_buf_start_addr;
	struct rdma_buf_type_s *first_rdma_buf;

	buf_size = MAX_INPUTSOURCE_COUNT * (RDMA_CMD_BUF_LEN + RDMA_LOAD_BUF_LEN);
	first_rdma_buf = get_vicp_rdma_buf_choice(0);
	rdma_buf_start_addr = first_rdma_buf->cmd_buf_start_addr;

	temp_addr = codec_mm_vmap(rdma_buf_start_addr, buf_size);
	memset(temp_addr, 0, buf_size);
	codec_mm_unmap_phyaddr(temp_addr);
	codec_mm_free_for_dma("vicp", rdma_buf_start_addr);

	rdma_buf_init_flag = 0;
	return 0;
}

static int add_jumpcmd_to_buf(int input_count)
{
	int num, buf_size;
	u8 *temp_addr;
	struct rdma_buf_type_s *buf;

	buf_size = MAX_INPUTSOURCE_COUNT * (RDMA_CMD_BUF_LEN + RDMA_LOAD_BUF_LEN);
	buf = get_vicp_rdma_buf_choice(0);
	temp_addr = codec_mm_vmap(buf->cmd_buf_start_addr, buf_size);
	for (num = 1; num < input_count; num++)
		vicp_rdma_jmp(get_vicp_rdma_buf_choice(num - 1), get_vicp_rdma_buf_choice(num), 1);

	codec_mm_dma_flush(temp_addr, buf_size, DMA_TO_DEVICE);
	codec_mm_unmap_phyaddr(temp_addr);
	codec_mm_free_for_dma("vicp", buf->cmd_buf_start_addr);
	return 0;
}

static void set_vid_cmpr_basic_param(struct vid_cmpr_top_s *vid_cmpr_top)
{
	u32 buf_h, buf_v;
	struct vid_cmpr_fgrain_s fgrain;

	if (IS_ERR_OR_NULL(vid_cmpr_top)) {
		vicp_print(VICP_ERROR, "%s: invalid param,return.\n", __func__);
		return;
	}

	vicp_print(VICP_INFO, "update basic vicp param.\n");

	if (vid_cmpr_top->src_compress == 1) {
		set_afbcd_headaddr(vid_cmpr_top->src_head_baddr);
		set_afbcd_bodyaddr(vid_cmpr_top->src_body_baddr);
	} else {
		set_rdmif_base_addr(RDMIF_BASEADDR_TYPE_Y, vid_cmpr_top->rdmif_canvas0_addr0);
		set_rdmif_base_addr(RDMIF_BASEADDR_TYPE_CB, vid_cmpr_top->rdmif_canvas0_addr1);
		set_rdmif_base_addr(RDMIF_BASEADDR_TYPE_CR, vid_cmpr_top->rdmif_canvas0_addr2);
	}

	memset(&fgrain, 0, sizeof(struct vid_cmpr_fgrain_s));
	if (vid_cmpr_top->film_grain_en) {
		fgrain.enable = true;
		fgrain.start_x = vid_cmpr_top->src_win_bgn_h;
		fgrain.end_x = vid_cmpr_top->src_win_end_h;
		fgrain.start_y = vid_cmpr_top->src_win_bgn_v;
		fgrain.end_y = vid_cmpr_top->src_win_end_v;
		fgrain.is_afbc = vid_cmpr_top->src_compress;
		fgrain.color_fmt = vid_cmpr_top->src_fmt_mode;
		fgrain.color_depth = vid_cmpr_top->src_compbits;
		fgrain.fgs_table_adr = vid_cmpr_top->film_lut_addr;
	} else {
		fgrain.enable = false;
	}
	set_vid_cmpr_fgrain(fgrain);

	if (vid_cmpr_top->wrmif_en == 1) {
		write_vicp_reg_bits(VID_CMPR_WR_PATH_CTRL, 1, 0, 1);
		if (vid_cmpr_top->out_shrk_en == 1) {
			buf_h = vid_cmpr_top->out_hsize_bgnd >> (1 + vid_cmpr_top->out_shrk_mode);
			buf_v = vid_cmpr_top->out_vsize_bgnd >> (1 + vid_cmpr_top->out_shrk_mode);
		} else {
			buf_h = vid_cmpr_top->out_hsize_bgnd;
			buf_v = vid_cmpr_top->out_vsize_bgnd;
		}
		set_wrmif_base_addr(0, vid_cmpr_top->wrmif_canvas0_addr0);
		set_wrmif_base_addr(1, vid_cmpr_top->wrmif_canvas0_addr0 + (buf_h * buf_v));
	} else {
		write_vicp_reg_bits(VID_CMPR_WR_PATH_CTRL, 0, 0, 1);
	}

	if (vid_cmpr_top->out_afbce_enable == 1) {
		write_vicp_reg_bits(VID_CMPR_WR_PATH_CTRL, 1, 1, 1);
		set_afbce_head_addr(vid_cmpr_top->out_head_baddr);
		set_afbce_mmu_rmif_control4(vid_cmpr_top->out_mmu_info_baddr);
		if (vicp_dev.ddr16_support &&
			(vid_cmpr_top->out_head_baddr >= ADDR_VALUE_8G ||
			vid_cmpr_top->out_mmu_info_baddr >= ADDR_VALUE_8G))
			set_afbce_ofset_burst4_en(1);
		else
			set_afbce_ofset_burst4_en(0);
	} else {
		write_vicp_reg_bits(VID_CMPR_WR_PATH_CTRL, 0, 1, 1);
	}
}

static void set_vid_cmpr_all_param(struct vid_cmpr_top_s *vid_cmpr_top)
{
	struct vid_cmpr_afbcd_s vid_cmpr_afbcd;
	struct vid_cmpr_fgrain_s fgrain;
	struct vid_cmpr_scaler_s vid_cmpr_scaler;
	struct vid_cmpr_hdr_s vid_cmpr_hdr;
	struct vid_cmpr_afbce_s vid_cmpr_afbce;

	struct vid_cmpr_mif_s vid_cmpr_rmif;
	struct vid_cmpr_mif_s vid_cmpr_wmif;
	int output_begin_h, output_begin_v, output_end_h, output_end_v;
	int shrink_enable, shrink_size, shrink_mode;
	int scaler_enable;
	u32 type;
	bool is_interlace = false;

	if (IS_ERR_OR_NULL(vid_cmpr_top)) {
		vicp_print(VICP_ERROR, "%s: invalid param,return.\n", __func__);
		return;
	}

	vicp_print(VICP_INFO, "update all vicp param.\n");

	if (vid_cmpr_top->src_compress == 1) {
		set_rdmif_enable(0);
		set_input_path(VICP_INPUT_PATH_AFBCD);
		memset(&vid_cmpr_afbcd, 0, sizeof(struct vid_cmpr_afbcd_s));
		if (vid_cmpr_top->src_vf->bitdepth & BITDEPTH_SAVING_MODE)
			vid_cmpr_afbcd.blk_mem_mode = 1;
		else
			vid_cmpr_afbcd.blk_mem_mode = 0;
		vid_cmpr_afbcd.hsize = vid_cmpr_top->src_hsize;
		vid_cmpr_afbcd.vsize = vid_cmpr_top->src_vsize;
		vid_cmpr_afbcd.head_baddr = vid_cmpr_top->src_head_baddr;
		vid_cmpr_afbcd.body_baddr = vid_cmpr_top->src_body_baddr;
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
		if (vid_cmpr_top->skip_mode == VICP_SKIP_MODE_OFF) {
			vid_cmpr_afbcd.h_skip_y = 0;
			vid_cmpr_afbcd.h_skip_uv = 0;
			vid_cmpr_afbcd.v_skip_y = 0;
			vid_cmpr_afbcd.v_skip_uv = 0;
		} else if (vid_cmpr_top->skip_mode == VICP_SKIP_MODE_HORZ) {
			vid_cmpr_afbcd.h_skip_y = 1;
			vid_cmpr_afbcd.h_skip_uv = 1;
			vid_cmpr_afbcd.v_skip_y = 0;
			vid_cmpr_afbcd.v_skip_uv = 0;
		} else if (vid_cmpr_top->skip_mode == VICP_SKIP_MODE_VERT) {
			vid_cmpr_afbcd.h_skip_y = 0;
			vid_cmpr_afbcd.h_skip_uv = 0;
			vid_cmpr_afbcd.v_skip_y = 1;
			vid_cmpr_afbcd.v_skip_uv = 1;
		} else if (vid_cmpr_top->skip_mode == VICP_SKIP_MODE_ALL) {
			vid_cmpr_afbcd.h_skip_y = 3;
			vid_cmpr_afbcd.h_skip_uv = 3;
			vid_cmpr_afbcd.v_skip_y = 3;
			vid_cmpr_afbcd.v_skip_uv = 3;
		} else {
			vid_cmpr_afbcd.h_skip_y = 0;
			vid_cmpr_afbcd.h_skip_uv = 0;
			vid_cmpr_afbcd.v_skip_y = 0;
			vid_cmpr_afbcd.v_skip_uv = 0;
		}
		vid_cmpr_afbcd.rev_mode = vid_cmpr_top->rot_rev_mode;
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
		vid_cmpr_afbcd.lossy_compress = vid_cmpr_top->src_lossy_compress;

		set_vid_cmpr_afbcd(2, vid_cmpr_top->rdma_enable, &vid_cmpr_afbcd);
	} else {
		set_afbcd_enable(0);
		set_input_path(VICP_INPUT_PATH_RDMIF);
		memset(&vid_cmpr_rmif, 0, sizeof(struct vid_cmpr_mif_s));
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

		if (vid_cmpr_top->src_vf) {
			type = vid_cmpr_top->src_vf->type;
			if (type & VIDTYPE_TYPEMASK) {
				/* interlace source */
				is_interlace = true;
				vid_cmpr_rmif.src_field_mode = 1;
				if ((type & VIDTYPE_TYPEMASK) == VIDTYPE_INTERLACE_TOP)
					vid_cmpr_rmif.output_field_num = 0;
				else
					vid_cmpr_rmif.output_field_num = 1;
			}
		}

		vid_cmpr_rmif.stride_y = vid_cmpr_top->canvas_width[0];
		vid_cmpr_rmif.stride_cb = vid_cmpr_top->canvas_width[1];
		vid_cmpr_rmif.stride_cr = vid_cmpr_top->canvas_width[2];
		vid_cmpr_rmif.swap_cbcr = vid_cmpr_top->src_need_swap_cbcr;

		set_vid_cmpr_rmif(&vid_cmpr_rmif, 0, 2);
	}

	memset(&fgrain, 0, sizeof(struct vid_cmpr_fgrain_s));
	if (vid_cmpr_top->film_grain_en) {
		fgrain.enable = true;
		fgrain.start_x = vid_cmpr_top->src_win_bgn_h;
		fgrain.end_x = vid_cmpr_top->src_win_end_h;
		fgrain.start_y = vid_cmpr_top->src_win_bgn_v;
		fgrain.end_y = vid_cmpr_top->src_win_end_v;
		fgrain.is_afbc = vid_cmpr_top->src_compress;
		fgrain.color_fmt = vid_cmpr_top->src_fmt_mode;
		fgrain.color_depth = vid_cmpr_top->src_compbits;
		fgrain.fgs_table_adr = vid_cmpr_top->film_lut_addr;
	} else {
		fgrain.enable = false;
	}
	set_vid_cmpr_fgrain(fgrain);

	memset(&vid_cmpr_scaler, 0, sizeof(struct vid_cmpr_scaler_s));
	vid_cmpr_scaler.din_hsize = vid_cmpr_top->src_hsize;
	if (is_interlace)
		vid_cmpr_scaler.din_vsize = vid_cmpr_top->src_vsize >> 1;
	else
		vid_cmpr_scaler.din_vsize = vid_cmpr_top->src_vsize;

	if (vid_cmpr_top->out_rot_en == 1) {
		vid_cmpr_scaler.dout_hsize = vid_cmpr_top->src_hsize;
		if (is_interlace)
			vid_cmpr_scaler.dout_vsize = vid_cmpr_top->src_vsize >> 1;
		else
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
	vid_cmpr_scaler.device_index = 5;
	vid_cmpr_scaler.vphase_type_top = F2V_P2P;
	vid_cmpr_scaler.vphase_type_bot = F2V_P2P;

	if (!scaler_en)
		scaler_enable = 0;
	else
		scaler_enable = 1;

	if (vid_cmpr_top->skip_mode == VICP_SKIP_MODE_HORZ) {
		vid_cmpr_scaler.din_hsize = vid_cmpr_scaler.din_hsize >> 1;
	} else if (vid_cmpr_top->skip_mode == VICP_SKIP_MODE_VERT) {
		vid_cmpr_scaler.din_vsize = vid_cmpr_scaler.din_vsize >> 1;
	} else if (vid_cmpr_top->skip_mode == VICP_SKIP_MODE_ALL) {
		vid_cmpr_scaler.din_hsize = vid_cmpr_scaler.din_hsize >> 1;
		vid_cmpr_scaler.din_vsize = vid_cmpr_scaler.din_vsize >> 1;
	}

	set_input_size(vid_cmpr_scaler.din_vsize, vid_cmpr_scaler.din_hsize);
	set_vid_cmpr_scale(scaler_enable, &vid_cmpr_scaler);
	set_output_size(vid_cmpr_top->out_vsize_in, vid_cmpr_top->out_hsize_in);

	memset(&vid_cmpr_hdr, 0, sizeof(struct vid_cmpr_hdr_s));
	if (!hdr_en)
		vid_cmpr_hdr.hdr2_en = 0;
	else
		vid_cmpr_hdr.hdr2_en = vid_cmpr_top->hdr_en;
	vid_cmpr_hdr.hdr2_only_mat = 0;
	vid_cmpr_hdr.hdr2_fmt_cfg = 1;
	vid_cmpr_hdr.input_fmt = 1;
	vid_cmpr_hdr.rgb_out_en = 0;
	set_vid_cmpr_hdr(vid_cmpr_hdr.hdr2_en);

	if (vid_cmpr_top->wrmif_en == 1) {
		write_vicp_reg_bits(VID_CMPR_WR_PATH_CTRL, 1, 0, 1);
		memset(&vid_cmpr_wmif, 0, sizeof(struct vid_cmpr_mif_s));
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

		if (shrink_enable == 1) {
			vid_cmpr_wmif.luma_x_start0 = output_begin_h >> (1 + shrink_mode);
			vid_cmpr_wmif.luma_x_end0 = ((output_end_h - output_begin_h +
				(1 << (1 + shrink_mode))) >> (1 + shrink_mode)) - 1;
			vid_cmpr_wmif.luma_y_start0 = output_begin_v >> (1 + shrink_mode);
			vid_cmpr_wmif.luma_y_end0 = ((output_end_v - output_begin_v +
				(1 << (1 + shrink_mode))) >> (1 + shrink_mode)) - 1;
			vid_cmpr_wmif.luma_x_end0 += vid_cmpr_wmif.luma_x_start0;
			vid_cmpr_wmif.luma_y_end0 += vid_cmpr_wmif.luma_y_start0;
			vid_cmpr_wmif.buf_hsize = vid_cmpr_top->out_hsize_bgnd >> (1 + shrink_mode);
			vid_cmpr_wmif.buf_vsize = vid_cmpr_top->out_vsize_bgnd >> (1 + shrink_mode);
		} else {
			vid_cmpr_wmif.luma_x_start0 = output_begin_h;
			vid_cmpr_wmif.luma_x_end0 = output_end_h;
			vid_cmpr_wmif.luma_y_start0 = output_begin_v;
			vid_cmpr_wmif.luma_y_end0 = output_end_v;
			vid_cmpr_wmif.buf_hsize = vid_cmpr_top->out_hsize_bgnd;
			vid_cmpr_wmif.buf_vsize = vid_cmpr_top->out_vsize_bgnd;
		}
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
			(vid_cmpr_wmif.buf_hsize * vid_cmpr_wmif.buf_vsize);
		vid_cmpr_wmif.canvas0_addr2 = vid_cmpr_top->wrmif_canvas0_addr2;
		vid_cmpr_wmif.rev_x = 0;
		vid_cmpr_wmif.rev_y = 0;

		if (!crop_en)
			vid_cmpr_wmif.buf_crop_en = 0;
		else
			vid_cmpr_wmif.buf_crop_en = 1;
		vid_cmpr_wmif.src_field_mode = 0;
		vid_cmpr_wmif.output_field_num = 1;
		vid_cmpr_wmif.endian = vid_cmpr_top->out_endian;
		vid_cmpr_wmif.swap_cbcr = vid_cmpr_top->out_need_swap_cbcr;

		set_vid_cmpr_wmif(&vid_cmpr_wmif, vid_cmpr_top->wrmif_en);
	} else {
		write_vicp_reg_bits(VID_CMPR_WR_PATH_CTRL, 0, 0, 1);
	}

	memset(&vid_cmpr_afbce, 0, sizeof(struct vid_cmpr_afbce_s));
	if (vid_cmpr_top->out_afbce_enable == 1) {
		write_vicp_reg_bits(VID_CMPR_WR_PATH_CTRL, 1, 1, 1);
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
		vid_cmpr_afbce.rev_mode = 0;
		vid_cmpr_afbce.def_color_0 = 0x3ff;/*<< (vid_cmpr_top->out_reg_compbits - 8);*/
		vid_cmpr_afbce.def_color_1 = 0x80 << (vid_cmpr_top->out_reg_compbits - 8);
		vid_cmpr_afbce.def_color_2 = 0x80 << (vid_cmpr_top->out_reg_compbits - 8);
		vid_cmpr_afbce.def_color_3 = 0x00 << (vid_cmpr_top->out_reg_compbits - 8);
		vid_cmpr_afbce.force_444_comb = 0;
		vid_cmpr_afbce.rot_en = vid_cmpr_top->out_rot_en;
		vid_cmpr_afbce.din_swt = 0;
		vid_cmpr_afbce.lossy_compress = vid_cmpr_top->out_lossy_compress;
		vid_cmpr_afbce.mmu_page_size = vid_cmpr_top->src_mmu_page_size_mode;
		set_vid_cmpr_afbce(1, &vid_cmpr_afbce, vid_cmpr_top->rdma_enable);
	} else {
		write_vicp_reg_bits(VID_CMPR_WR_PATH_CTRL, 0, 1, 1);
		set_vid_cmpr_afbce(0, &vid_cmpr_afbce, vid_cmpr_top->rdma_enable);
	}

	set_top_holdline();
}

int vicp_crc0_check(int check_val)
{
	int reg_val = 0;

	reg_val = read_vicp_reg(VID_CMPR_CRC0_OUT);
	vicp_print(VICP_INFO, "%s: reg_val0 is 0x%x.\n", __func__, reg_val);
	if (reg_val != check_val)
		return 1;
	else
		return 0;
}

int vicp_crc1_check(int chroma_en, int chroma_check, int lumma_check)
{
	int reg_val = 0;
	int check_val = 3;
	int ret = 0;

	if (chroma_en == 0) {
		reg_val = read_vicp_reg(VID_CMPR_CRC1_0_OUT);
		vicp_print(VICP_INFO, "%s: reg_val1 is 0x%x.\n", __func__, reg_val);
		if (reg_val != lumma_check)
			check_val &= ~(1 << 1);
	} else if (chroma_en == 1) {
		reg_val = read_vicp_reg(VID_CMPR_CRC1_1_OUT);
		vicp_print(VICP_INFO, "%s: reg_val2 is 0x%x.\n", __func__, reg_val);
		if (reg_val != chroma_check)
			check_val &= ~(1 << 2);
	} else {
		vicp_print(VICP_ERROR, "%s: invalid config.\n", __func__);
		check_val = 0;
	}

	if ((check_val & 3) != 3) {
		pr_info("%s failed: check_val = 0x%x.\n", __func__, check_val);
		ret = 1;
	} else {
		pr_info("%s success.\n", __func__);
		ret = 0;
	}

	return ret;
}

void vicp_device_init(struct vicp_device_data_s device)
{
	vicp_dev = device;
}

int vicp_process_config(struct vicp_data_config_s *data_config,
	struct vid_cmpr_top_s *vid_cmpr_top)
{
	struct vframe_s *input_vframe = NULL;
	struct dma_data_config_s *input_dma = NULL;
	enum vicp_rotation_mode_e rotation;
	enum vframe_signal_fmt_e signal_fmt;
	struct vid_cmpr_lossy_compress_s temp_compress_param;
	u32 canvas_width = 0;

	if (IS_ERR_OR_NULL(data_config) || IS_ERR_OR_NULL(vid_cmpr_top)) {
		vicp_print(VICP_ERROR, "%s: NULL param.\n", __func__);
		return -1;
	}

	memset(&temp_compress_param, 0, sizeof(temp_compress_param));

	if (data_config->input_data.is_vframe) {
		input_vframe = data_config->input_data.data_vf;
		if (IS_ERR_OR_NULL(input_vframe)) {
			vicp_print(VICP_ERROR, "%s: NULL vframe.\n", __func__);
			return -1;
		}

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

			if (input_vframe->type & VIDTYPE_COMPRESS_LOSS &&
			    input_vframe->vf_lossycomp_param.lossy_mode == 1) {
				temp_compress_param.compress_mode = LOSSY_COMPRESS_MODE_CR;
				temp_compress_param.quant_diff_root_leave =
					input_vframe->vf_lossycomp_param.quant_diff_root_leave;
				temp_compress_param.brst_len_add_en =
					input_vframe->vf_lossycomp_param.burst_length_add_en;
				temp_compress_param.brst_len_add_value =
					input_vframe->vf_lossycomp_param.burst_length_add_value;
				temp_compress_param.ofset_brst4_en =
					input_vframe->vf_lossycomp_param.ofset_burst4_en;
			} else {
				temp_compress_param.compress_mode = LOSSY_COMPRESS_MODE_OFF;
			}
		} else {
			vid_cmpr_top->src_compress = 0;
			temp_compress_param.compress_mode = LOSSY_COMPRESS_MODE_OFF;
			vid_cmpr_top->src_hsize = input_vframe->width;
			vid_cmpr_top->src_vsize = input_vframe->height;
			vid_cmpr_top->src_head_baddr = 0;
			vid_cmpr_top->src_body_baddr = 0;
			vid_cmpr_top->rdmif_canvas0_addr0 =
				input_vframe->canvas0_config[0].phy_addr;
			vid_cmpr_top->rdmif_canvas0_addr1 =
				input_vframe->canvas0_config[1].phy_addr;
			vid_cmpr_top->rdmif_canvas0_addr2 =
				input_vframe->canvas0_config[2].phy_addr;

			if (input_vframe->canvas0Addr == (u32)-1) {
				vid_cmpr_top->canvas_width[0] =
					input_vframe->canvas0_config[0].width;
				vid_cmpr_top->canvas_width[1] =
					input_vframe->canvas0_config[1].width;
				vid_cmpr_top->canvas_width[2] =
					input_vframe->canvas0_config[2].width;
			} else {
				vid_cmpr_top->canvas_width[0] =
					canvas_get_width(input_vframe->canvas0Addr & 0xff);
				vid_cmpr_top->canvas_width[1] =
					canvas_get_width(input_vframe->canvas0Addr >> 8 & 0xff);
				vid_cmpr_top->canvas_width[2] =
					canvas_get_width(input_vframe->canvas0Addr >> 16 & 0xff);
			}
		}
		vid_cmpr_top->src_vf = input_vframe;
		vid_cmpr_top->src_fmt_mode = get_input_color_format(input_vframe);
		vid_cmpr_top->src_compbits = get_input_color_bitdepth(input_vframe);
		vid_cmpr_top->src_win_bgn_h = 0;
		vid_cmpr_top->src_win_end_h = vid_cmpr_top->src_hsize - 1;
		vid_cmpr_top->src_win_bgn_v = 0;
		vid_cmpr_top->src_win_end_v = vid_cmpr_top->src_vsize - 1;
		if (input_vframe->flag & VFRAME_FLAG_VIDEO_LINEAR)
			vid_cmpr_top->src_endian = 1;
		else
			vid_cmpr_top->src_endian = 0;
		vid_cmpr_top->src_block_mode = input_vframe->canvas0_config[0].block_mode;
		canvas_width = vid_cmpr_top->canvas_width[0];
		signal_fmt = get_vframe_src_fmt(input_vframe);
		if (signal_fmt != VFRAME_SIGNAL_FMT_INVALID && signal_fmt != VFRAME_SIGNAL_FMT_SDR)
			vid_cmpr_top->hdr_en = 1;
		else
			vid_cmpr_top->hdr_en = 0;

		if (input_vframe->fgs_valid && input_vframe->fgs_table_adr) {
			vid_cmpr_top->film_grain_en = 1;
			vid_cmpr_top->film_lut_addr = input_vframe->fgs_table_adr;
			input_vframe->fgs_valid = false;
		} else {
			vid_cmpr_top->film_grain_en = 0;
			vid_cmpr_top->film_lut_addr = 0;
		}
	} else {
		input_dma = data_config->input_data.data_dma;
		vid_cmpr_top->src_compress = 0;
		temp_compress_param.compress_mode = LOSSY_COMPRESS_MODE_OFF;
		vid_cmpr_top->src_hsize = input_dma->data_width;
		vid_cmpr_top->src_vsize = input_dma->data_height;
		vid_cmpr_top->src_fmt_mode = input_dma->color_format;
		vid_cmpr_top->src_compbits = input_dma->color_depth;
		vid_cmpr_top->rdmif_canvas0_addr0 = input_dma->buf_addr;
		vid_cmpr_top->rdmif_canvas0_addr1 = vid_cmpr_top->rdmif_canvas0_addr0 +
			input_dma->buf_stride_w * input_dma->buf_stride_h;
		vid_cmpr_top->rdmif_canvas0_addr2 = vid_cmpr_top->rdmif_canvas0_addr1 +
			input_dma->buf_stride_w * input_dma->buf_stride_h;

		canvas_width = input_dma->buf_stride_w;
		if (input_dma->color_format == VICP_COLOR_FORMAT_YUV420) {
			vid_cmpr_top->canvas_width[0] = input_dma->buf_stride_w;
			vid_cmpr_top->canvas_width[1] = input_dma->buf_stride_w;
			vid_cmpr_top->canvas_width[2] = 0;
		} else if (input_dma->color_format == VICP_COLOR_FORMAT_YUV422) {
			vid_cmpr_top->canvas_width[0] = input_dma->buf_stride_w;
			vid_cmpr_top->canvas_width[1] = 0;
			vid_cmpr_top->canvas_width[2] = 0;
		} else if (input_dma->color_format == VICP_COLOR_FORMAT_YUV444) {
			vid_cmpr_top->canvas_width[0] = input_dma->buf_stride_w;
			vid_cmpr_top->canvas_width[1] = 0;
			vid_cmpr_top->canvas_width[2] = 0;
		} else {
			vicp_print(VICP_ERROR, "unsupport fmt %d\n", input_dma->color_format);
		}

		vid_cmpr_top->src_win_bgn_h = 0;
		vid_cmpr_top->src_win_end_h = vid_cmpr_top->src_hsize - 1;
		vid_cmpr_top->src_win_bgn_v = 0;
		vid_cmpr_top->src_win_end_v = vid_cmpr_top->src_vsize - 1;
		vid_cmpr_top->src_endian = input_dma->endian;
		vid_cmpr_top->src_block_mode = 0;
		vid_cmpr_top->hdr_en = 0;
		vid_cmpr_top->src_need_swap_cbcr = input_dma->need_swap_cbcr;
		vid_cmpr_top->film_grain_en = 0;
		vid_cmpr_top->film_lut_addr = 0;
	}

	vid_cmpr_top->src_lossy_compress = temp_compress_param;
	if (!fgrain_en)
		vid_cmpr_top->film_grain_en = 0;

	/* vd mif burst len is 2 as default */
	vid_cmpr_top->src_burst_len = 2;
	if (canvas_width % 32)
		vid_cmpr_top->src_burst_len = 0;
	else if (canvas_width % 64)
		vid_cmpr_top->src_burst_len = 1;

	if (vid_cmpr_top->src_block_mode)
		vid_cmpr_top->src_burst_len = vid_cmpr_top->src_block_mode;

	vid_cmpr_top->src_pip_src_mode = 0;

	if (vid_cmpr_top->src_fmt_mode == VICP_COLOR_FORMAT_YUV420)
		vid_cmpr_top->rdmif_separate_en = 2;
	else
		vid_cmpr_top->rdmif_separate_en = 0;

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

	if (data_config->data_option.shrink_mode == VICP_SHRINK_MODE_OFF)
		vid_cmpr_top->out_shrk_en = 0;
	else
		vid_cmpr_top->out_shrk_en = 1;
	vid_cmpr_top->out_shrk_mode = data_config->data_option.shrink_mode;
	vid_cmpr_top->skip_mode = data_config->data_option.skip_mode;

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
		vid_cmpr_top->out_rot_en = 0;
		vid_cmpr_top->rot_rev_mode = 0;
	}

	vid_cmpr_top->rot_hshrk_ratio = 0;
	vid_cmpr_top->rot_vshrk_ratio = 0;

	vid_cmpr_top->out_endian = data_config->output_data.endian;

	if (vid_cmpr_top->out_rot_en == 1)
		vid_cmpr_top->wrmif_en = 0;
	else
		vid_cmpr_top->wrmif_en = data_config->output_data.mif_out_en;
	vid_cmpr_top->wrmif_fmt_mode = data_config->output_data.mif_color_fmt;
	vid_cmpr_top->wrmif_bits_mode = data_config->output_data.mif_color_dep;
	vid_cmpr_top->wrmif_canvas0_addr0 = data_config->output_data.phy_addr[0];
	if (vid_cmpr_top->wrmif_fmt_mode == VICP_COLOR_FORMAT_YUV420)
		vid_cmpr_top->wrmif_set_separate_en = 2;
	else
		vid_cmpr_top->wrmif_set_separate_en = 0;
	vid_cmpr_top->wrmif_canvas0_addr1 = 0;
	vid_cmpr_top->wrmif_canvas0_addr2 = 0;

	if (!rdma_en)
		vid_cmpr_top->rdma_enable = false;
	else
		vid_cmpr_top->rdma_enable = data_config->data_option.rdma_enable;

	vid_cmpr_top->src_count = data_config->data_option.input_source_count;
	vid_cmpr_top->src_num = data_config->data_option.input_source_number;

	vid_cmpr_top->security_en = data_config->data_option.security_enable;

	vid_cmpr_top->out_need_swap_cbcr = data_config->output_data.need_swap_cbcr;

	if (data_config->data_option.compress_rate) {
		vid_cmpr_top->out_lossy_compress.compress_mode = 4;
		vid_cmpr_top->out_lossy_compress.compress_rate =
			data_config->data_option.compress_rate;
	} else {
		vid_cmpr_top->out_lossy_compress.compress_mode = 0;
		vid_cmpr_top->out_lossy_compress.compress_rate = 0;
	}

	return 0;
}

int vicp_process_task(struct vid_cmpr_top_s *vid_cmpr_top)
{
	int ret = 0;
	int time = 0;
	int i = 0;
	bool is_need_update_all = false;

	vicp_print(VICP_INFO, "enter %s, rdma_en is %d.\n", __func__, vid_cmpr_top->rdma_enable);
	if (IS_ERR_OR_NULL(vid_cmpr_top)) {
		vicp_print(VICP_ERROR, "%s: NULL param, please check.\n", __func__);
		return -1;
	}

	set_vid_cmpr_rdma_config(vid_cmpr_top->rdma_enable, vid_cmpr_top->src_count,
		vid_cmpr_top->src_num);

	set_vid_cmpr_security(vid_cmpr_top->security_en);

	if (vid_cmpr_top->rdma_enable) {
		write_vicp_reg_bits(VID_CMPR_WR_PATH_CTRL, 3, 0, 2);
		set_module_enable(1);
		set_module_reset();
		set_vid_cmpr_all_param(vid_cmpr_top);
		set_module_start(1);
		write_vicp_reg_bits(VID_CMPR_AFBCE_ENABLE, 1, 0, 1);
		set_vid_cmpr_crc(vid_cmpr_top->out_rot_en);
		vicp_rdma_end(get_current_vicp_rdma_buf());
		if (vid_cmpr_top->src_num + 1 == vid_cmpr_top->src_count) {
			init_completion(&vicp_rdma_done);
			init_completion(&vicp_proc_done);
			add_jumpcmd_to_buf(vid_cmpr_top->src_count);
			vicp_rdma_buf_load(vid_cmpr_top->src_count);
			time = wait_for_completion_timeout(&vicp_proc_done, msecs_to_jiffies(300));
			if (!time) {
				vicp_print(VICP_ERROR, "vicp_task wait isr timeout\n");
				if (debug_rdma_en) {
					vicp_rdma_errorflag_parser();
					vicp_rdma_cpsr_dump();
					for (i = 0; i < vid_cmpr_top->src_count; i++)
						vicp_rdma_buf_dump(vid_cmpr_top->src_count, i);
				}
				ret = -2;
			} else {
				if (rdma_done_count == vid_cmpr_top->src_count)
					vicp_print(VICP_INFO, "rdma all complete.\n");
				else
					vicp_print(VICP_ERROR, "wait rdma isr.\n");
			}

			rdma_done_count = 0;

			vicp_rdma_errorflag_clear();
		}
	} else {
		if (last_input_w != vid_cmpr_top->src_hsize ||
			last_input_h != vid_cmpr_top->src_vsize ||
			last_input_fmt != vid_cmpr_top->src_fmt_mode ||
			last_input_bit_depth != vid_cmpr_top->src_compbits ||
			last_fbcinput_en != vid_cmpr_top->src_compress ||
			last_output_w != vid_cmpr_top->out_hsize_bgnd ||
			last_output_h != vid_cmpr_top->out_vsize_bgnd ||
			last_output_miffmt != vid_cmpr_top->wrmif_fmt_mode ||
			last_output_fbcfmt != vid_cmpr_top->out_reg_format_mode ||
			last_fbcoutput_en != vid_cmpr_top->out_afbce_enable ||
			last_mifoutput_en != vid_cmpr_top->wrmif_en ||
			last_output_begin_v != vid_cmpr_top->out_win_bgn_v ||
			last_output_end_v != vid_cmpr_top->out_win_end_v ||
			last_output_begin_h != vid_cmpr_top->out_win_bgn_h ||
			last_output_end_h != vid_cmpr_top->out_win_end_h) {
			is_need_update_all = true;
			last_input_w = vid_cmpr_top->src_hsize;
			last_input_h = vid_cmpr_top->src_vsize;
			last_input_fmt = vid_cmpr_top->src_fmt_mode;
			last_input_bit_depth = vid_cmpr_top->src_compbits;
			last_fbcinput_en = vid_cmpr_top->src_compress;
			last_output_w = vid_cmpr_top->out_hsize_bgnd;
			last_output_h = vid_cmpr_top->out_vsize_bgnd;
			last_output_miffmt = vid_cmpr_top->wrmif_fmt_mode;
			last_output_fbcfmt = vid_cmpr_top->out_reg_format_mode;
			last_fbcoutput_en = vid_cmpr_top->out_afbce_enable;
			last_mifoutput_en = vid_cmpr_top->wrmif_en;
			last_output_begin_v = vid_cmpr_top->out_win_bgn_v;
			last_output_end_v = vid_cmpr_top->out_win_end_v;
			last_output_begin_h = vid_cmpr_top->out_win_bgn_h;
			last_output_end_h = vid_cmpr_top->out_win_end_h;
		}

		if (print_flag & VICP_INFO) {
			pr_info("vicp: ##########basic param##########\n");
			pr_info("vicp: input: w:%d, h:%d, fmt:%d, bitdepth:%d, src_compress:%d.\n",
				vid_cmpr_top->src_hsize,
				vid_cmpr_top->src_vsize,
				vid_cmpr_top->src_fmt_mode,
				vid_cmpr_top->src_compbits,
				vid_cmpr_top->src_compress);
			pr_info("vicp: skip_mode: %d.\n", vid_cmpr_top->skip_mode);
			pr_info("vicp: output: w:%d, h:%d.\n",
				vid_cmpr_top->out_hsize_bgnd,
				vid_cmpr_top->out_vsize_bgnd);
			pr_info("vicp: output: mif_en:%d, mif_fmt:%d.\n",
				vid_cmpr_top->wrmif_en,
				vid_cmpr_top->wrmif_fmt_mode);
			pr_info("vicp: output: fbc_en:%d, fbc_fmt:%d.\n",
				vid_cmpr_top->out_afbce_enable,
				vid_cmpr_top->out_reg_format_mode);
			pr_info("vicp: output: axis:%d %d %d %d.\n",
				vid_cmpr_top->out_win_bgn_h,
				vid_cmpr_top->out_win_end_h,
				vid_cmpr_top->out_win_bgn_v,
				vid_cmpr_top->out_win_end_v);
			pr_info("vicp: #################################.\n");
		};

		init_completion(&vicp_proc_done);
		write_vicp_reg_bits(VID_CMPR_WR_PATH_CTRL, 3, 0, 2);
		set_module_enable(1);
		set_module_reset();
		if (is_need_update_all)
			set_vid_cmpr_all_param(vid_cmpr_top);
		else
			set_vid_cmpr_basic_param(vid_cmpr_top);
		set_module_start(1);
		write_vicp_reg_bits(VID_CMPR_AFBCE_ENABLE, 1, 0, 1);
		set_vid_cmpr_crc(vid_cmpr_top->out_rot_en);
		time = wait_for_completion_timeout(&vicp_proc_done, msecs_to_jiffies(200));
		if (!time) {
			vicp_print(VICP_ERROR, "vicp_task wait isr timeout\n");
			ret = -2;
		}
	}

	vicp_print(VICP_DUMP_REG, "%s: set reg end.\n", __func__);

	return ret;
}

int vicp_process_reset(void)
{
	return 0;
}

int vicp_process_enable(int enable)
{
	vicp_print(VICP_INFO, "%s: %d.\n", __func__, enable);

	if (enable) {
		init_rdma_buf();
	} else {
		last_input_w = 0;
		last_input_h = 0;
		last_input_fmt = 0;
		last_input_bit_depth = 0;
		last_fbcinput_en = 0;
		last_output_w = 0;
		last_output_h = 0;
		last_output_miffmt = 0;
		last_output_fbcfmt = 0;
		last_fbcoutput_en = 0;
		last_mifoutput_en = 0;
		last_output_begin_v = 0;
		last_output_end_v = 0;
		last_output_begin_h = 0;
		last_output_end_h = 0;
		uninit_rdma_buf();
	}

	return 0;
}
EXPORT_SYMBOL(vicp_process_enable);

int  vicp_process(struct vicp_data_config_s *data_config)
{
	int ret = 0;
	struct vid_cmpr_top_s vid_cmpr_top;

	mutex_lock(&vicp_mutex);
	if (IS_ERR_OR_NULL(data_config)) {
		vicp_print(VICP_ERROR, "%s: NULL param, please check.\n", __func__);
		mutex_unlock(&vicp_mutex);
		return -1;
	}

	memset(&vid_cmpr_top, 0, sizeof(struct vid_cmpr_top_s));
	ret = vicp_process_config(data_config, &vid_cmpr_top);
	if (ret < 0) {
		vicp_print(VICP_ERROR, "vicp config failed.\n");
		mutex_unlock(&vicp_mutex);
		return ret;
	}

	ret = vicp_process_task(&vid_cmpr_top);
	if (ret < 0)
		vicp_print(VICP_ERROR, "vicp task failed.\n");

	if (current_dump_flag != dump_yuv_flag) {
		dump_output(&data_config->output_data);
		current_dump_flag = dump_yuv_flag;
	}

	vicp_process_reset();
	mutex_unlock(&vicp_mutex);

	return  0;
}
EXPORT_SYMBOL(vicp_process);
