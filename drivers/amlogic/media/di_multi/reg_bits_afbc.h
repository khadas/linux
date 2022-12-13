/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * drivers/amlogic/media/di_multi/sc2/reg_bits_afbc.h
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

#ifndef __REG_BITS_AFBC_H__
#define __REG_BITS_AFBC_H__

#ifdef MARK_SC2 //move to hw.h?
struct regs_t {
	unsigned int add;/*add index*/
	unsigned int bit;
	unsigned int wid;
	unsigned int id;
//	unsigned int df_val;
	char *name;
};
#endif

#ifdef MARK_SC2
#define AFBC_ENABLE		(0x1ae0)
#define AFBC_MODE		(0x1ae1)
#define AFBC_SIZE_IN		(0x1ae2)
#define AFBC_DEC_DEF_COLOR		(0x1ae3)
#define AFBC_CONV_CTRL		(0x1ae4)
#define AFBC_LBUF_DEPTH		(0x1ae5)
#define AFBC_HEAD_BADDR		(0x1ae6)
#define AFBC_BODY_BADDR		(0x1ae7)
#define AFBC_SIZE_OUT		(0x1ae8)
#define AFBC_OUT_YSCOPE		(0x1ae9)
#define AFBC_STAT		(0x1aea)
#define AFBC_VD_CFMT_CTRL		(0x1aeb)
#define AFBC_VD_CFMT_W		(0x1aec)
#define AFBC_MIF_HOR_SCOPE		(0x1aed)
#define AFBC_MIF_VER_SCOPE		(0x1aee)
#define AFBC_PIXEL_HOR_SCOPE		(0x1aef)
#define AFBC_PIXEL_VER_SCOPE		(0x1af0)
#define AFBC_VD_CFMT_H		(0x1af1)
#endif

//#define AFBC_REG_INDEX_NUB	(18)
//#define AFBC_DEC_NUB		(2)

union AFBC_ENABLE_BITS {
	unsigned int d32;
	struct {
		unsigned int dec_frm_start:1,
		head_len_sel:1,
		reserved3:6,
		dec_enable:1,
		cmd_blk_size:3,
		ddr_blk_size:2,
		reserved2:2,
		soft_rst:3,
		dos_uncomp_mode:1,
		fmt444_comb:1,
		addr_link_en:1,
		fmt_size_sw_mode:1,
		reserved1:9;
	} b;
};

union AFBC_MODE_BITS {
	unsigned int d32;
	struct {
		unsigned int horz_skip_uv:2,
		vert_skip_uv:2,
		horz_skip_y:2,
		vert_skip_y:2,
		compbits_y:2,	/* 0-8bits 1-9bits 2-10bits */
		compbits_u:2,	/* 0-8bits 1-9bits 2-10bits */
		compbits_v:2,	/* 0-8bits 1-9bits 2-10bits */
		burst_len:2,
		hold_line_num:7,
		reserved2:1,
		mif_urgent:2,
		rev_mode:2,
		blk_mem_mode:1,
		ddr_sz_mode:1,
		reserved1:2;
	} b;
};

union AFBC_SIZE_IN_BITS {
	unsigned int d32;
	struct {
		unsigned int vsize_in:13,
		reserved2:3,
		hsize_in:13,
		reserved1:3;
	} b;
};

union AFBC_DEC_DEF_COLOR_BITS {
	unsigned int d32;
	struct {
		unsigned int def_color_v:10,
		def_color_u:10,
		def_color_y:10,
		reserved1:2;
	} b;
};

union AFBC_CONV_CTRL_BITS {
	unsigned int d32;
	struct {
		unsigned int conv_lbf_len:12,
		fmt_mode:2,
		reserved1:18;
	} b;
};

union AFBC_LBUF_DEPTH_BITS {
	unsigned int d32;
	struct {
		unsigned int mif_lbuf_depth:12,
		reserved2:4,
		dec_lbuf_depth:12,
		reserved1:4;
	} b;
};

union AFBC_SIZE_OUT_BITS {
	unsigned int d32;
	struct {
		unsigned int vsize_out:13,
		reserved2:3,
		hsize_out:13,
		reserved1:3;
	} b;
};

union AFBC_OUT_YSCOPE_BITS {
	unsigned int d32;
	struct {
		unsigned int out_vert_end:13,
		reserved2:3,
		out_vert_bgn:13,
		reserved1:3;
	} b;
};

union AFBC_STAT_BITS {
	unsigned int d32;
	struct {
		unsigned int frm_end_stat:1,
		ro_dbg_top_info:31;
	} b;
};

union AFBC_VD_CFMT_CTRL_BITS {
	unsigned int d32;
	struct {
		unsigned int cvfmt_en:1,
		phase_step:7,
		cvfmt_ini_phase:4,
		skip_line_num:4,
		rpt_line0_en:1,
		phase0_nrpt_en:1,
		rpt_last_dis:1,
		phase0_always_en:1,
		chfmt_en:1,
		yc_ratio:2,
		rpt_p0_en:1,
		ini_phase:4,
		rpt_pix:1,
		reserved1:1,
		cfmt_soft_rst:1,
		gclk_bit_dis:1;
	} b;
};

union AFBC_VD_CFMT_W_BITS {
	unsigned int d32;
	struct {
		unsigned int v_f_width:12,
		reserved2:4,
		h_f_width:12,
		reserved1:4;
	} b;
};

union AFBC_MIF_HOR_SCOPE_BITS {
	unsigned int d32;
	struct {
		unsigned int mif_blk_end_h:10,
		reserved2:6,
		mif_blk_bgn_h:10,
		reserved1:6;
	} b;
};

union AFBC_MIF_VER_SCOPE_BITS {
	unsigned int d32;
	struct {
		unsigned int mif_blk_end_v:12,
		reserved2:4,
		mif_blk_bgn_v:12,
		reserved1:4;
	} b;
};

union AFBC_PIXEL_HOR_SCOPE_BITS {
	unsigned int d32;
	struct {
		unsigned int dec_pixel_end_h:13,
		reserved2:3,
		dec_pixel_bgn_h:13,
		reserved1:3;
	} b;
};

union AFBC_PIXEL_VER_SCOPE_BITS {
	unsigned int d32;
	struct {
		unsigned int dec_pixel_end_v:13,
		reserved2:3,
		dec_pixel_bgn_v:13,
		reserved1:3;
	} b;
};

union AFBC_VD_CFMT_H_BITS {
	unsigned int d32;
	struct {
		unsigned int v_fmt_height:13,
		reserved1:19;
	} b;
};

struct reg_afbc_bits_s {
	union AFBC_ENABLE_BITS enable;
	union AFBC_MODE_BITS mode;
	union AFBC_SIZE_IN_BITS size_in;
	union AFBC_DEC_DEF_COLOR_BITS dec_color;
	union AFBC_CONV_CTRL_BITS conv_ctr;
	union AFBC_LBUF_DEPTH_BITS lbuf_depth;
	/*
	 *
	 */
	unsigned int head_addr;
	unsigned int body_addr;
	union AFBC_SIZE_OUT_BITS size_out;
	union AFBC_OUT_YSCOPE_BITS out_yscope;
	union AFBC_STAT_BITS state;
	union AFBC_VD_CFMT_CTRL_BITS vd_cfmt;
	union AFBC_VD_CFMT_W_BITS vd_cfmt_w;
	union AFBC_MIF_HOR_SCOPE_BITS mif_hor_scope;
	union AFBC_MIF_VER_SCOPE_BITS mif_v_scope;
	union AFBC_PIXEL_HOR_SCOPE_BITS pixel_h;
	union AFBC_PIXEL_VER_SCOPE_BITS pixel_v;
	union AFBC_VD_CFMT_H_BITS vd_cfmt_h;
};

struct afbc_ctr_s {
	union {
		unsigned int d32;
		struct {
		//unsigned int support	: 1;
		unsigned int int_flg	: 1; /*addr ini*/
		unsigned int en		: 1;
		unsigned int rev1	: 2;

		unsigned int chg_level	: 2;
		unsigned int rev2	: 26;
		} b;
	};
	unsigned int l_vtype;
	unsigned int l_h;
	unsigned int l_w;
	unsigned int l_bitdepth;
	unsigned int addr_h;
	unsigned int addr_b;
};

#ifdef MARK_SC2	/*temp for AFBC_REG_INDEX_NUB define*/
struct reg_afbs_s {
	unsigned int support;
	struct afbc_ctr_s ctr;
	union {
		struct reg_afbc_bits_s bits;
		unsigned int	d32[AFBC_REG_INDEX_NUB];
	};
	unsigned int addr[AFBC_REG_INDEX_NUB];
};
#endif
/**************************************************************/
/*move from deinterlace.c*/

#define AFBC_B_DEC_FRM_START	(0)
#define AFBC_B_HEAD_LEN_SEL	(1)
#define AFBC_B_DEC_ENABLE	(2)
#define AFBC_B_CMD_BLK_SIZE	(3)
#define AFBC_B_DDR_BLK_SIZE	(4)
#define AFBC_B_SOFT_RST	(5)
#define AFBC_B_DOS_UNCOMP_MODE	(6)
#define AFBC_B_FMT444_COMB	(7)
#define AFBC_B_ADDR_LINK_EN	(8)
#define AFBC_B_FMT_SIZE_SW_MODE	(9)
#define AFBC_B_HORZ_SKIP_UV	(10)
#define AFBC_B_VERT_SKIP_UV	(11)
#define AFBC_B_HORZ_SKIP_Y	(12)
#define AFBC_B_VERT_SKIP_Y	(13)
#define AFBC_B_COMPBITS_Y	(14)
#define AFBC_B_COMPBITS_U	(15)
#define AFBC_B_COMPBITS_V	(16)
#define AFBC_B_BURST_LEN	(17)
#define AFBC_B_HOLD_LINE_NUM	(18)
#define AFBC_B_MIF_URGENT	(19)
#define AFBC_B_REV_MODE	(20)
#define AFBC_B_BLK_MEM_MODE	(21)
#define AFBC_B_DDR_SZ_MODE	(22)
#define AFBC_B_VSIZE_IN	(23)
#define AFBC_B_HSIZE_IN	(24)
#define AFBC_B_DEF_COLOR_V	(25)
#define AFBC_B_DEF_COLOR_U	(26)
#define AFBC_B_DEF_COLOR_Y	(27)
#define AFBC_B_CONV_LBF_LEN	(28)
#define AFBC_B_FMT_MODE	(29)
#define AFBC_B_MIF_LBUF_DEPTH	(30)
#define AFBC_B_DEC_LBUF_DEPTH	(31)
#define AFBC_B_MIF_INFO_BADDR	(32)
#define AFBC_B_VSIZE_OUT	(33)
#define AFBC_B_HSIZE_OUT	(34)
#define AFBC_B_OUT_VERT_END	(35)
#define AFBC_B_OUT_VERT_BGN	(36)
#define AFBC_B_FRM_END_STAT	(37)
#define AFBC_B_RO_DBG_TOP_INFO	(38)
#define AFBC_B_CVFMT_EN	(39)
#define AFBC_B_PHASE_STEP	(40)
#define AFBC_B_CVFMT_INI_PHASE	(41)
#define AFBC_B_SKIP_LINE_NUM	(42)
#define AFBC_B_RPT_LINE0_EN	(43)
#define AFBC_B_PHASE0_NRPT_EN	(44)
#define AFBC_B_RPT_LAST_DIS	(45)
#define AFBC_B_PHASE0_ALWAYS_EN	(46)
#define AFBC_B_CHFMT_EN	(47)
#define AFBC_B_YC_RATIO	(48)
#define AFBC_B_RPT_P0_EN	(49)
#define AFBC_B_INI_PHASE	(50)
#define AFBC_B_RPT_PIX	(51)
#define AFBC_B_CFMT_SOFT_RST	(52)
#define AFBC_B_GCLK_BIT_DIS	(53)
#define AFBC_B_V_F_WIDTH	(54)
#define AFBC_B_H_F_WIDTH	(55)
#define AFBC_B_MIF_BLK_END_H	(56)
#define AFBC_B_MIF_BLK_BGN_H	(57)
#define AFBC_B_MIF_BLK_END_V	(58)
#define AFBC_B_MIF_BLK_BGN_V	(59)
#define AFBC_B_DEC_PIXEL_END_H	(60)
#define AFBC_B_DEC_PIXEL_BGN_H	(61)
#define AFBC_B_DEC_PIXEL_END_V	(62)
#define AFBC_B_DEC_PIXEL_BGN_V	(63)
#define AFBC_B_V_FMT_HEIGHT	(64)

#endif /*__REG_BITS_AFBC_H__*/

