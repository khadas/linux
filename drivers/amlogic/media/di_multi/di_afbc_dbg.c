// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * drivers/amlogic/media/di_multi/sc2/di_afbc_dbg.c
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

#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/semaphore.h>
#include <linux/workqueue.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/major.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/proc_fs.h>
#include <linux/list.h>
#include <linux/of_reserved_mem.h>
#include <linux/of_irq.h>
#include <linux/uaccess.h>
#include <linux/of_fdt.h>
#include <linux/cma.h>
#include <linux/dma-contiguous.h>
#include <linux/ctype.h>
#include <linux/string.h>
#include <linux/of_device.h>

#include <linux/amlogic/media/vfm/vframe.h>

/*dma_get_cma_size_int_byte*/
#include <linux/amlogic/media/codec_mm/codec_mm.h>

#include "deinterlace_dbg.h"
#include "deinterlace.h"
#include "di_data_l.h" //ary if run on di_multi
//#include "di_pqa.h"
#include "../deinterlace/di_pqa.h"

#include "register.h"
#include "nr_downscale.h"
#include "deinterlace_hw.h"
#include "register_nr4.h"

//#include "di_reg_tab.h"
#include "di_afbc_v3.h"

#include "reg_bits_afbc.h"
#include "reg_bits_afbce.h"
//only in di_multi #include "di_reg_tab.h"

#include <linux/amlogic/media/vpu/vpu.h>

#define rd aml_read_vcbus

/*keep order*/
static const struct regs_t reg_bits_tab_afbc[] = {
	{EAFBC_ENABLE,  0,  1, AFBC_B_DEC_FRM_START, "dec_frm_start"},
	{EAFBC_ENABLE,  1,  1, AFBC_B_HEAD_LEN_SEL, "head_len_sel"},
	{EAFBC_ENABLE,  8,  1, AFBC_B_DEC_ENABLE, "dec_enable"},
	{EAFBC_ENABLE,  9,  3, AFBC_B_CMD_BLK_SIZE, "cmd_blk_size"},
	{EAFBC_ENABLE, 12,  2, AFBC_B_DDR_BLK_SIZE, "ddr_blk_size"},
	{EAFBC_ENABLE, 16,  3, AFBC_B_SOFT_RST, "soft_rst"},
	{EAFBC_ENABLE, 19,  1, AFBC_B_DOS_UNCOMP_MODE, "dos_uncomp_mode"},
	{EAFBC_ENABLE, 20,  1, AFBC_B_FMT444_COMB, "fmt444_comb"},
	{EAFBC_ENABLE, 21,  1, AFBC_B_ADDR_LINK_EN, "addr_link_en"},
	{EAFBC_ENABLE, 22,  1, AFBC_B_FMT_SIZE_SW_MODE, "fmt_size_sw_mode"},
	{EAFBC_MODE,  0,  2, AFBC_B_HORZ_SKIP_UV, "horz_skip_uv"},
	{EAFBC_MODE,  2,  2, AFBC_B_VERT_SKIP_UV, "vert_skip_uv"},
	{EAFBC_MODE,  4,  2, AFBC_B_HORZ_SKIP_Y, "horz_skip_y"},
	{EAFBC_MODE,  6,  2, AFBC_B_VERT_SKIP_Y, "vert_skip_y"},
	{EAFBC_MODE,  8,  2, AFBC_B_COMPBITS_Y, "compbits_y"},
	{EAFBC_MODE, 10,  2, AFBC_B_COMPBITS_U, "compbits_u"},
	{EAFBC_MODE, 12,  2, AFBC_B_COMPBITS_V, "compbits_v"},
	{EAFBC_MODE, 14,  2, AFBC_B_BURST_LEN, "burst_len"},
	{EAFBC_MODE, 16,  7, AFBC_B_HOLD_LINE_NUM, "hold_line_num"},
	{EAFBC_MODE, 24,  2, AFBC_B_MIF_URGENT, "mif_urgent"},
	{EAFBC_MODE, 26,  2, AFBC_B_REV_MODE, "rev_mode"},
	{EAFBC_MODE, 28,  1, AFBC_B_BLK_MEM_MODE, "blk_mem_mode"},
	{EAFBC_MODE, 29,  1, AFBC_B_DDR_SZ_MODE, "ddr_sz_mode"},
	{EAFBC_SIZE_IN,  0, 13, AFBC_B_VSIZE_IN, "vsize_in"},
	{EAFBC_SIZE_IN, 16, 13, AFBC_B_HSIZE_IN, "hsize_in"},
	{EAFBC_DEC_DEF_COLOR,  0, 10, AFBC_B_DEF_COLOR_V, "def_color_v"},
	{EAFBC_DEC_DEF_COLOR, 10, 10, AFBC_B_DEF_COLOR_U, "def_color_u"},
	{EAFBC_DEC_DEF_COLOR, 20, 10, AFBC_B_DEF_COLOR_Y, "def_color_y"},
	{EAFBC_CONV_CTRL,  0, 12, AFBC_B_CONV_LBF_LEN, "conv_lbf_len"},
	{EAFBC_CONV_CTRL, 12,  2, AFBC_B_FMT_MODE, "fmt_mode"},
	{EAFBC_LBUF_DEPTH,  0, 12, AFBC_B_MIF_LBUF_DEPTH, "mif_lbuf_depth"},
	{EAFBC_LBUF_DEPTH, 16, 12, AFBC_B_DEC_LBUF_DEPTH, "dec_lbuf_depth"},
	{EAFBC_HEAD_BADDR,  0, 32, AFBC_B_MIF_INFO_BADDR, "mif_info_baddr"},
	{EAFBC_SIZE_OUT,  0, 13, AFBC_B_VSIZE_OUT, "vsize_out"},
	{EAFBC_SIZE_OUT, 16, 13, AFBC_B_HSIZE_OUT, "hsize_out"},
	{EAFBC_OUT_YSCOPE,  0, 13, AFBC_B_OUT_VERT_END, "out_vert_end"},
	{EAFBC_OUT_YSCOPE, 16, 13, AFBC_B_OUT_VERT_BGN, "out_vert_bgn"},
	{EAFBC_STAT,  0,  1, AFBC_B_FRM_END_STAT, "frm_end_stat"},
	{EAFBC_STAT,  1, 31, AFBC_B_RO_DBG_TOP_INFO, "ro_dbg_top_info"},
	{EAFBC_VD_CFMT_CTRL,  0,  1, AFBC_B_CVFMT_EN, "cvfmt_en"},
	{EAFBC_VD_CFMT_CTRL,  1,  7, AFBC_B_PHASE_STEP, "phase_step"},
	{EAFBC_VD_CFMT_CTRL,  8,  4, AFBC_B_CVFMT_INI_PHASE, "cvfmt_ini_phase"},
	{EAFBC_VD_CFMT_CTRL, 12,  4, AFBC_B_SKIP_LINE_NUM, "skip_line_num"},
	{EAFBC_VD_CFMT_CTRL, 16,  1, AFBC_B_RPT_LINE0_EN, "rpt_line0_en"},
	{EAFBC_VD_CFMT_CTRL, 17,  1, AFBC_B_PHASE0_NRPT_EN, "phase0_nrpt_en"},
	{EAFBC_VD_CFMT_CTRL, 18,  1, AFBC_B_RPT_LAST_DIS, "rpt_last_dis"},
	{EAFBC_VD_CFMT_CTRL, 19,  1,
		AFBC_B_PHASE0_ALWAYS_EN, "phase0_always_en"},
	{EAFBC_VD_CFMT_CTRL, 20,  1, AFBC_B_CHFMT_EN, "chfmt_en"},
	{EAFBC_VD_CFMT_CTRL, 21,  2, AFBC_B_YC_RATIO, "yc_ratio"},
	{EAFBC_VD_CFMT_CTRL, 23,  1, AFBC_B_RPT_P0_EN, "rpt_p0_en"},
	{EAFBC_VD_CFMT_CTRL, 24,  4, AFBC_B_INI_PHASE, "ini_phase"},
	{EAFBC_VD_CFMT_CTRL, 28,  1, AFBC_B_RPT_PIX, "rpt_pix"},
	{EAFBC_VD_CFMT_CTRL, 30,  1, AFBC_B_CFMT_SOFT_RST, "cfmt_soft_rst"},
	{EAFBC_VD_CFMT_CTRL, 31,  1, AFBC_B_GCLK_BIT_DIS, "gclk_bit_dis"},
	{EAFBC_VD_CFMT_W,  0, 12, AFBC_B_V_F_WIDTH, "v_f_width"},
	{EAFBC_VD_CFMT_W, 16, 12, AFBC_B_H_F_WIDTH, "h_f_width"},
	{EAFBC_MIF_HOR_SCOPE,  0, 10, AFBC_B_MIF_BLK_END_H, "mif_blk_end_h"},
	{EAFBC_MIF_HOR_SCOPE, 16, 10, AFBC_B_MIF_BLK_BGN_H, "mif_blk_bgn_h"},
	{EAFBC_MIF_VER_SCOPE,  0, 12, AFBC_B_MIF_BLK_END_V, "mif_blk_end_v"},
	{EAFBC_MIF_VER_SCOPE, 16, 12, AFBC_B_MIF_BLK_BGN_V, "mif_blk_bgn_v"},
	{EAFBC_PIXEL_HOR_SCOPE,  0, 13,
		AFBC_B_DEC_PIXEL_END_H, "dec_pixel_end_h"},
	{EAFBC_PIXEL_HOR_SCOPE, 16, 13,
		AFBC_B_DEC_PIXEL_BGN_H, "dec_pixel_bgn_h"},
	{EAFBC_PIXEL_VER_SCOPE,  0, 13,
		AFBC_B_DEC_PIXEL_END_V, "dec_pixel_end_v"},
	{EAFBC_PIXEL_VER_SCOPE, 16, 13,
		AFBC_B_DEC_PIXEL_BGN_V, "dec_pixel_bgn_v"},
	{EAFBC_VD_CFMT_H,  0, 13, AFBC_B_V_FMT_HEIGHT, "v_fmt_height"},
	{TABLE_FLG_END, TABLE_FLG_END, 0xff, 0xff, "end"},
};

const struct regs_t reg_bits_tab_afbce[] = {
	{EAFBCE_ENABLE,  0,  1, E_AFBCE_AFBCE_ST, "afbce_st"},
	{EAFBCE_ENABLE,  8,  1, E_AFBCE_ENC_EN, "enc_en"},
	{EAFBCE_ENABLE, 12,  1, E_AFBCE_ENC_EN_MODE, "enc_en_mode"},
	{EAFBCE_ENABLE, 13,  1, E_AFBCE_ENC_RST_MODE, "enc_rst_mode"},
	{EAFBCE_ENABLE, 16,  4, E_AFBCE_AFBCE_SYNC_SEL, "afbce_sync_sel"},
	{EAFBCE_ENABLE, 20, 12, E_AFBCE_GCLK_CTRL, "gclk_ctrl"},
	{EAFBCE_MODE,  0,  1, E_AFBCE_REG_FMT444_CMB, "reg_fmt444_cmb"},
	{EAFBCE_MODE, 14,  2, E_AFBCE_BURST_MODE, "burst_mode"},
	{EAFBCE_MODE, 16,  7, E_AFBCE_HOLD_LINE_NUM, "hold_line_num"},
	{EAFBCE_MODE, 24,  2, E_AFBCE_MIF_URGENT, "mif_urgent"},
	{EAFBCE_MODE, 26,  2, E_AFBCE_REV_MODE, "rev_mode"},
	{EAFBCE_MODE, 29,  3, E_AFBCE_SOFT_RST, "soft_rst"},
	{EAFBCE_SIZE_IN,  0, 13, E_AFBCE_V_IN, "v_in"},
	{EAFBCE_SIZE_IN, 16, 13, E_AFBCE_H_IN, "h_in"},
	{EAFBCE_BLK_SIZE_IN,  0, 13, E_AFBCE_VBLK_SIZE, "vblk_size"},
	{EAFBCE_BLK_SIZE_IN, 16, 13, E_AFBCE_HBLK_SIZE, "hblk_size"},
	{EAFBCE_HEAD_BADDR,  0, 32, E_AFBCE_HEAD_BADDR, "head_baddr"},
	{EAFBCE_MIF_SIZE,  0, 16, E_AFBCE_MMU_PAGE_SIZE, "mmu_page_size"},
	{EAFBCE_MIF_SIZE, 16,  5, E_AFBCE_UNCMP_SIZE, "uncmp_size"},
	{EAFBCE_MIF_SIZE, 24,  3, E_AFBCE_CMD_BLK_SIZE, "cmd_blk_size"},
	{EAFBCE_MIF_SIZE, 28,  2, E_AFBCE_DDR_BLK_SIZE, "ddr_blk_size"},
	{EAFBCE_PIXEL_IN_HOR_SCOPE,  0, 13,
		E_AFBCE_ENC_WIN_BGN_H, "enc_win_bgn_h"},
	{EAFBCE_PIXEL_IN_HOR_SCOPE, 16, 13,
		E_AFBCE_ENC_WIN_END_H, "enc_win_end_h"},
	{EAFBCE_PIXEL_IN_VER_SCOPE,  0, 13,
		E_AFBCE_ENC_WIN_BGN_V, "enc_win_bgn_v"},
	{EAFBCE_PIXEL_IN_VER_SCOPE, 16, 13,
		E_AFBCE_ENC_WIN_END_V, "enc_win_end_v"},
	{EAFBCE_CONV_CTRL,  0, 12, E_AFBCE_LBUF_DEPTH, "lbuf_depth"},
	{EAFBCE_CONV_CTRL, 16, 13, E_AFBCE_FMT_YBUF_DEPTH, "fmt_ybuf_depth"},
	{EAFBCE_MIF_HOR_SCOPE,  0, 10, E_AFBCE_BLK_BGN_H, "blk_bgn_h"},
	{EAFBCE_MIF_HOR_SCOPE, 16, 10, E_AFBCE_BLK_END_H, "blk_end_h"},
	{EAFBCE_MIF_VER_SCOPE,  0, 12, E_AFBCE_BLK_BGN_V, "blk_bgn_v"},
	{EAFBCE_MIF_VER_SCOPE, 16, 12, E_AFBCE_BLK_END_V, "blk_end_v"},
	{EAFBCE_FORMAT,  0,  4, E_AFBCE_COMPBITS_Y, "compbits_y"},
	{EAFBCE_FORMAT,  4,  4, E_AFBCE_COMPBITS_C, "compbits_c"},
	{EAFBCE_FORMAT,  8,  2, E_AFBCE_FORMAT_MODE, "format_mode"},
	{EAFBCE_MODE_EN,  0,  1, E_AFBCE_FORCE_ORDER_EN, "force_order_en"},
	{EAFBCE_MODE_EN,  1,  3,
		E_AFBCE_FORCE_ORDER_VALUE, "force_order_value"},
	{EAFBCE_MODE_EN,  4,  1,
		E_AFBCE_DWDS_PADDING_UV128, "dwds_padding_uv128"},
	{EAFBCE_MODE_EN,  5,  1,
		E_AFBCE_INPUT_PADDING_UV128, "input_padding_uv128"},
	{EAFBCE_MODE_EN,  8,  1,
		E_AFBCE_UNCOMPRESS_SPLIT_MODE, "uncompress_split_mode"},
	{EAFBCE_MODE_EN,  9,  1, E_AFBCE_ENABLE_16X4BLOCK, "enable_16x4block"},
	{EAFBCE_MODE_EN, 10,  1, E_AFBCE_MINVAL_YENC_EN, "minval_yenc_en"},
	{EAFBCE_MODE_EN, 12,  1, E_AFBCE_DISABLE_ORDER_I0, "disable_order_i0"},
	{EAFBCE_MODE_EN, 13,  1, E_AFBCE_DISABLE_ORDER_I1, "disable_order_i1"},
	{EAFBCE_MODE_EN, 14,  1, E_AFBCE_DISABLE_ORDER_I2, "disable_order_i2"},
	{EAFBCE_MODE_EN, 15,  1, E_AFBCE_DISABLE_ORDER_I3, "disable_order_i3"},
	{EAFBCE_MODE_EN, 16,  1, E_AFBCE_DISABLE_ORDER_I4, "disable_order_i4"},
	{EAFBCE_MODE_EN, 17,  1, E_AFBCE_DISABLE_ORDER_I5, "disable_order_i5"},
	{EAFBCE_MODE_EN, 18,  1, E_AFBCE_DISABLE_ORDER_I6, "disable_order_i6"},
	{EAFBCE_MODE_EN, 20,  1,
		E_AFBCE_ADPT_XINTERLEAVE_CHRM_RIDE,
		"adpt_xinterleave_chrm_ride"},
	{EAFBCE_MODE_EN, 21,  1,
		E_AFBCE_ADPT_XINTERLEAVE_LUMA_RIDE,
		"adpt_xinterleave_luma_ride"},
	{EAFBCE_MODE_EN, 22,  1,
		E_AFBCE_ADPT_YINTERLEAVE_CHRM_RIDE,
		"adpt_yinterleave_chrm_ride"},
	{EAFBCE_MODE_EN, 23,  1,
		E_AFBCE_ADPT_YINTERLEAVE_LUMA_RIDE,
		"adpt_yinterleave_luma_ride"},
	{EAFBCE_MODE_EN, 24,  1,
		E_AFBCE_ADPT_INTERLEAVE_CMODE, "adpt_interleave_cmode"},
	{EAFBCE_MODE_EN, 25,  1,
		E_AFBCE_ADPT_INTERLEAVE_YMODE, "adpt_interleave_ymode"},
	{EAFBCE_DWSCALAR,  0,  2, E_AFBCE_DWSCALAR_H1, "dwscalar_h1"},
	{EAFBCE_DWSCALAR,  2,  2, E_AFBCE_DWSCALAR_H0, "dwscalar_h0"},
	{EAFBCE_DWSCALAR,  4,  2, E_AFBCE_DWSCALAR_W1, "dwscalar_w1"},
	{EAFBCE_DWSCALAR,  6,  2, E_AFBCE_DWSCALAR_W0, "dwscalar_w0"},
	{EAFBCE_DEFCOLOR_1,  0, 12,
		E_AFBCE_ENC_DEFAULT_COLOR_0, "enc_default_color_0"},
	{EAFBCE_DEFCOLOR_1, 12, 12,
		E_AFBCE_ENC_DEFAULT_COLOR_3, "enc_default_color_3"},
	{EAFBCE_DEFCOLOR_2,  0, 12,
		E_AFBCE_ENC_DEFAULT_COLOR_1, "enc_default_color_1"},
	{EAFBCE_DEFCOLOR_2, 12, 12,
		E_AFBCE_ENC_DEFAULT_COLOR_2, "enc_default_color_2"},
	{EAFBCE_QUANT_ENABLE,  0,  1, E_AFBCE_QUANT_EN0, "quant_en0"},
	{EAFBCE_QUANT_ENABLE,  4,  1, E_AFBCE_QUANT_EN1, "quant_en1"},
	{EAFBCE_QUANT_ENABLE,  8,  2, E_AFBCE_BCLEAV_OFST, "bcleav_ofst"},
	{EAFBCE_QUANT_ENABLE, 10,  1, E_AFBCE_QUANT_EXP_EN0, "quant_exp_en0"},
	{EAFBCE_QUANT_ENABLE, 11,  1, E_AFBCE_QUANT_EXP_EN1, "quant_exp_en1"},
	{EAFBCE_IQUANT_LUT_1,  0,  3,
		E_AFBCE_IQUANT_YCLUT_0_4, "iquant_yclut_0_4"},
	{EAFBCE_IQUANT_LUT_1,  4,  3,
		E_AFBCE_IQUANT_YCLUT_0_5, "iquant_yclut_0_5"},
	{EAFBCE_IQUANT_LUT_1,  8,  3,
		E_AFBCE_IQUANT_YCLUT_0_6, "iquant_yclut_0_6"},
	{EAFBCE_IQUANT_LUT_1, 12,  3,
		E_AFBCE_IQUANT_YCLUT_0_7, "iquant_yclut_0_7"},
	{EAFBCE_IQUANT_LUT_1, 16,  3,
		E_AFBCE_IQUANT_YCLUT_0_8, "iquant_yclut_0_8"},
	{EAFBCE_IQUANT_LUT_1, 20,  3,
		E_AFBCE_IQUANT_YCLUT_0_9, "iquant_yclut_0_9"},
	{EAFBCE_IQUANT_LUT_1, 24,  3,
		E_AFBCE_IQUANT_YCLUT_0_10, "iquant_yclut_0_10"},
	{EAFBCE_IQUANT_LUT_1, 28,  3,
		E_AFBCE_IQUANT_YCLUT_0_11, "iquant_yclut_0_11"},
	{EAFBCE_IQUANT_LUT_2,  0,  3,
		E_AFBCE_IQUANT_YCLUT_0_0, "iquant_yclut_0_0"},
	{EAFBCE_IQUANT_LUT_2,  4,  3,
		E_AFBCE_IQUANT_YCLUT_0_1, "iquant_yclut_0_1"},
	{EAFBCE_IQUANT_LUT_2,  8,  3,
		E_AFBCE_IQUANT_YCLUT_0_2, "iquant_yclut_0_2"},
	{EAFBCE_IQUANT_LUT_2, 12,  3,
		E_AFBCE_IQUANT_YCLUT_0_3, "iquant_yclut_0_3"},
	{EAFBCE_IQUANT_LUT_3,  0,  3,
		E_AFBCE_IQUANT_YCLUT_1_4, "iquant_yclut_1_4"},
	{EAFBCE_IQUANT_LUT_3,  4,  3,
		E_AFBCE_IQUANT_YCLUT_1_5, "iquant_yclut_1_5"},
	{EAFBCE_IQUANT_LUT_3,  8,  3,
		E_AFBCE_IQUANT_YCLUT_1_6, "iquant_yclut_1_6"},
	{EAFBCE_IQUANT_LUT_3, 12,  3,
		E_AFBCE_IQUANT_YCLUT_1_7, "iquant_yclut_1_7"},
	{EAFBCE_IQUANT_LUT_3, 16,  3,
		E_AFBCE_IQUANT_YCLUT_1_8, "iquant_yclut_1_8"},
	{EAFBCE_IQUANT_LUT_3, 20,  3,
		E_AFBCE_IQUANT_YCLUT_1_9, "iquant_yclut_1_9"},
	{EAFBCE_IQUANT_LUT_3, 24,  3,
		E_AFBCE_IQUANT_YCLUT_1_10, "iquant_yclut_1_10"},
	{EAFBCE_IQUANT_LUT_3, 28,  3,
		E_AFBCE_IQUANT_YCLUT_1_11, "iquant_yclut_1_11"},
	{EAFBCE_IQUANT_LUT_4,  0,  3,
		E_AFBCE_IQUANT_YCLUT_1_0, "iquant_yclut_1_0"},
	{EAFBCE_IQUANT_LUT_4,  4,  3,
		E_AFBCE_IQUANT_YCLUT_1_1, "iquant_yclut_1_1"},
	{EAFBCE_IQUANT_LUT_4,  8,  3,
		E_AFBCE_IQUANT_YCLUT_1_2, "iquant_yclut_1_2"},
	{EAFBCE_IQUANT_LUT_4, 12,  3,
		E_AFBCE_IQUANT_YCLUT_1_3, "iquant_yclut_1_3"},
	{EAFBCE_RQUANT_LUT_1,  0,  3,
		E_AFBCE_RQUANT_YCLUT_0_4, "rquant_yclut_0_4"},
	{EAFBCE_RQUANT_LUT_1,  4,  3,
		E_AFBCE_RQUANT_YCLUT_0_5, "rquant_yclut_0_5"},
	{EAFBCE_RQUANT_LUT_1,  8,  3,
		E_AFBCE_RQUANT_YCLUT_0_6, "rquant_yclut_0_6"},
	{EAFBCE_RQUANT_LUT_1, 12,  3,
		E_AFBCE_RQUANT_YCLUT_0_7, "rquant_yclut_0_7"},
	{EAFBCE_RQUANT_LUT_1, 16,  3,
		E_AFBCE_RQUANT_YCLUT_0_8, "rquant_yclut_0_8"},
	{EAFBCE_RQUANT_LUT_1, 20,  3,
		E_AFBCE_RQUANT_YCLUT_0_9, "rquant_yclut_0_9"},
	{EAFBCE_RQUANT_LUT_1, 24,  3,
		E_AFBCE_RQUANT_YCLUT_0_10, "rquant_yclut_0_10"},
	{EAFBCE_RQUANT_LUT_1, 28,  3,
		E_AFBCE_RQUANT_YCLUT_0_11, "rquant_yclut_0_11"},
	{EAFBCE_RQUANT_LUT_2,  0,  3,
		E_AFBCE_RQUANT_YCLUT_0_0, "rquant_yclut_0_0"},
	{EAFBCE_RQUANT_LUT_2,  4,  3,
		E_AFBCE_RQUANT_YCLUT_0_1, "rquant_yclut_0_1"},
	{EAFBCE_RQUANT_LUT_2,  8,  3,
		E_AFBCE_RQUANT_YCLUT_0_2, "rquant_yclut_0_2"},
	{EAFBCE_RQUANT_LUT_2, 12,  3,
		E_AFBCE_RQUANT_YCLUT_0_3, "rquant_yclut_0_3"},
	{EAFBCE_RQUANT_LUT_3,  0,  3,
		E_AFBCE_RQUANT_YCLUT_1_4, "rquant_yclut_1_4"},
	{EAFBCE_RQUANT_LUT_3,  4,  3,
		E_AFBCE_RQUANT_YCLUT_1_5, "rquant_yclut_1_5"},
	{EAFBCE_RQUANT_LUT_3,  8,  3,
		E_AFBCE_RQUANT_YCLUT_1_6, "rquant_yclut_1_6"},
	{EAFBCE_RQUANT_LUT_3, 12,  3,
		E_AFBCE_RQUANT_YCLUT_1_7, "rquant_yclut_1_7"},
	{EAFBCE_RQUANT_LUT_3, 16,  3,
		E_AFBCE_RQUANT_YCLUT_1_8, "rquant_yclut_1_8"},
	{EAFBCE_RQUANT_LUT_3, 20,  3,
		E_AFBCE_RQUANT_YCLUT_1_9, "rquant_yclut_1_9"},
	{EAFBCE_RQUANT_LUT_3, 24,  3,
		E_AFBCE_RQUANT_YCLUT_1_10, "rquant_yclut_1_10"},
	{EAFBCE_RQUANT_LUT_3, 28,  3,
		E_AFBCE_RQUANT_YCLUT_1_11, "rquant_yclut_1_11"},
	{EAFBCE_RQUANT_LUT_4,  0,  3,
		E_AFBCE_RQUANT_YCLUT_1_0, "rquant_yclut_1_0"},
	{EAFBCE_RQUANT_LUT_4,  4,  3,
		E_AFBCE_RQUANT_YCLUT_1_1, "rquant_yclut_1_1"},
	{EAFBCE_RQUANT_LUT_4,  8,  3,
		E_AFBCE_RQUANT_YCLUT_1_2, "rquant_yclut_1_2"},
	{EAFBCE_RQUANT_LUT_4, 12,  3,
		E_AFBCE_RQUANT_YCLUT_1_3, "rquant_yclut_1_3"},
	{EAFBCE_YUV_FORMAT_CONV_MODE,  0,  3,
		E_AFBCE_MODE_422TO420, "mode_422to420"},
	{EAFBCE_YUV_FORMAT_CONV_MODE,  4,  3,
		E_AFBCE_MODE_444TO422, "mode_444to422"},
	{EAFBCE_DUMMY_DATA,  0, 30, E_AFBCE_DUMMY_DATA, "dummy_data"},
	{EAFBCE_CLR_FLAG,  0,  1, E_AFBCE_FRM_END_CLR, "frm_end_clr"},
	{EAFBCE_CLR_FLAG,  1,  1, E_AFBCE_ENC_ERROR_CLR, "enc_error_clr"},
	{EAFBCE_MMU_RMIF_CTRL1,  0,  3, E_AFBCE_PACK_MODE, "pack_mode"},
	{EAFBCE_MMU_RMIF_CTRL1,  4,  1, E_AFBCE_X_REV, "x_rev"},
	{EAFBCE_MMU_RMIF_CTRL1,  5,  1, E_AFBCE_Y_REV, "y_rev"},
	{EAFBCE_MMU_RMIF_CTRL1,  6,  1, E_AFBCE_LITTLE_ENDIAN, "little_endian"},
	{EAFBCE_MMU_RMIF_CTRL1,  7,  1, E_AFBCE_SWAP_64BIT, "swap_64bit"},
	{EAFBCE_MMU_RMIF_CTRL1,  8,  2, E_AFBCE_BURST_LEN, "burst_len"},
	{EAFBCE_MMU_RMIF_CTRL1, 10,  2, E_AFBCE_CMD_REQ_SIZE, "cmd_req_size"},
	{EAFBCE_MMU_RMIF_CTRL1, 12,  3, E_AFBCE_CMD_INTR_LEN, "cmd_intr_len"},
	{EAFBCE_MMU_RMIF_CTRL1, 16,  8, E_AFBCE_CANVAS_ID, "canvas_id"},
	{EAFBCE_MMU_RMIF_CTRL1, 24,  2, E_AFBCE_SYNC_SEL, "sync_sel"},
	{EAFBCE_MMU_RMIF_CTRL2,  0, 17, E_AFBCE_UNGENT_CTRL, "ungent_ctrl"},
	{EAFBCE_MMU_RMIF_CTRL2, 18,  6,
		E_AFBCE_RMIF_GCLK_CTRL, "rmif_gclk_ctrl"},
	{EAFBCE_MMU_RMIF_CTRL2, 30,  2, E_AFBCE_SW_RST, "sw_rst"},
	{EAFBCE_MMU_RMIF_CTRL3,  0, 13, E_AFBCE_STRIDE, "stride"},
	{EAFBCE_MMU_RMIF_CTRL3, 16,  1, E_AFBCE_ACC_MODE, "acc_mode"},
	{EAFBCE_MMU_RMIF_CTRL4,  0, 32, E_AFBCE_BADDR, "baddr"},
	{EAFBCE_MMU_RMIF_SCOPE_X,  0, 13, E_AFBCE_X_START, "x_start"},
	{EAFBCE_MMU_RMIF_SCOPE_X, 16, 13, E_AFBCE_X_END, "x_end"},
	{EAFBCE_MMU_RMIF_SCOPE_Y,  0, 13, E_AFBCE_Y_START, "y_start"},
	{EAFBCE_MMU_RMIF_SCOPE_Y, 16, 13, E_AFBCE_Y_END, "y_end"},
	{EAFBCE_MMU_RMIF_RO_STAT,  0, 16, E_AFBCE_STATUS, "status"},
	{TABLE_FLG_END, TABLE_FLG_END, 0xff, 0xff, "end"},
};

static unsigned int get_reg_bits(unsigned int val, unsigned int bstart,
				 unsigned int bw)
{
	return((val &
	       (((1L << bw) - 1) << bstart)) >> (bstart));
}

//static
void dbg_regs_tab(struct seq_file *s, const struct regs_t *pregtab,
			  const unsigned int *padd)
{
	struct regs_t creg;
	int i;
	unsigned int l_add;
	unsigned int val32 = 1, val;
//	char *bname;
//	char *info;

	i = 0;
	l_add = 0xffff;
	creg = pregtab[i];

	do {
		if (creg.add != l_add) {
			val32 = rd(padd[creg.add]);		/*RD*/
			seq_printf(s, "add:0x%x = 0x%08x, %s\n",
				   padd[creg.add], val32, creg.name);
			l_add = creg.add;
		}
		val = get_reg_bits(val32, creg.bit, creg.wid);	/*RD_B*/

		seq_printf(s, "\t%d:\tbit[%d,%d]:\t0x%x[%d]\t%s\n",
			   creg.id, creg.bit, creg.wid, val, val, creg.name);

		i++;
		creg = pregtab[i];
		if (i > DIMTABLE_LEN_MAX) {
			pr_info("too long, stop\n");
			break;
		}
	} while (creg.add != TABLE_FLG_END);
}

void dbg_afbcd_bits_show(struct seq_file *s, enum EAFBC_DEC eidx)
{
	seq_printf(s, "dump bits:afbcd[%d]\n", eidx);
	dbg_regs_tab(s, &reg_bits_tab_afbc[0], dim_afds()->get_d_addrp(eidx));
}
EXPORT_SYMBOL(dbg_afbcd_bits_show);

void dbg_afbce_bits_show(struct seq_file *s, enum EAFBC_ENC eidx)
{
	seq_printf(s, "dump bits:afbcd[%d]\n", eidx);
	dbg_regs_tab(s, &reg_bits_tab_afbce[0], dim_afds()->get_e_addrp(eidx));
}
EXPORT_SYMBOL(dbg_afbce_bits_show);

void dbg_mif_wr_bits_show(struct seq_file *s, enum EDI_MIFSM mifsel)
{
	seq_printf(s, "dump bits:wr[%d]\n", mifsel);
	if (opl1()->reg_mif_wr_bits_tab)
		dbg_regs_tab(s, opl1()->reg_mif_wr_bits_tab,
			     opl1()->reg_mif_wr_tab[mifsel]);
}
EXPORT_SYMBOL(dbg_mif_wr_bits_show);

