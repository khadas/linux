/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * drivers/amlogic/media/di_multi/sc2/reg_bits_afbce.h
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

#ifndef __REG_BITS_AFBCE_H__
#define __REG_BITS_AFBCE_H__

/*register*/
#ifdef MARK_SC2 /*move to register.h*/
#define DI_AFBCE_CTRL			(0x2003)

#define DI_AFBCE_ENABLE			(0x2060)
#define DI_AFBCE_MODE			(0x2061)
#define DI_AFBCE_SIZE_IN		(0x2062)
#define DI_AFBCE_BLK_SIZE_IN		(0x2063)
#define DI_AFBCE_HEAD_BADDR		(0x2064)
#define DI_AFBCE_MIF_SIZE		(0x2065)
#define DI_AFBCE_PIXEL_IN_HOR_SCOPE	(0x2066)
#define DI_AFBCE_PIXEL_IN_VER_SCOPE	(0x2067)
#define DI_AFBCE_CONV_CTRL		(0x2068)
#define DI_AFBCE_MIF_HOR_SCOPE		(0x2069)
#define DI_AFBCE_MIF_VER_SCOPE		(0x206a)
#define DI_AFBCE_STAT1			(0x206b)
#define DI_AFBCE_STAT2			(0x206c)
#define DI_AFBCE_FORMAT			(0x206d)
#define DI_AFBCE_MODE_EN		(0x206e)
#define DI_AFBCE_DWSCALAR		(0x206f)
#define DI_AFBCE_DEFCOLOR_1		(0x2070)
#define DI_AFBCE_DEFCOLOR_2		(0x2071)
#define DI_AFBCE_QUANT_ENABLE		(0x2072)
#define DI_AFBCE_IQUANT_LUT_1		(0x2073)
#define DI_AFBCE_IQUANT_LUT_2		(0x2074)
#define DI_AFBCE_IQUANT_LUT_3		(0x2075)
#define DI_AFBCE_IQUANT_LUT_4		(0x2076)
#define DI_AFBCE_RQUANT_LUT_1		(0x2077)
#define DI_AFBCE_RQUANT_LUT_2		(0x2078)
#define DI_AFBCE_RQUANT_LUT_3		(0x2079)
#define DI_AFBCE_RQUANT_LUT_4		(0x207a)
#define DI_AFBCE_YUV_FORMAT_CONV_MODE	(0x207b)
#define DI_AFBCE_DUMMY_DATA		(0x207c)
#define DI_AFBCE_CLR_FLAG		(0x207d)
#define DI_AFBCE_STA_FLAGT		(0x207e)
#define DI_AFBCE_MMU_NUM		(0x207f)	/*read only*/
#define DI_AFBCE_MMU_RMIF_CTRL1		(0x2080)
#define DI_AFBCE_MMU_RMIF_CTRL2		(0x2081)
#define DI_AFBCE_MMU_RMIF_CTRL3		(0x2082)
#define DI_AFBCE_MMU_RMIF_CTRL4		(0x2083)
#define DI_AFBCE_MMU_RMIF_SCOPE_X	(0x2084)
#define DI_AFBCE_MMU_RMIF_SCOPE_Y	(0x2085)
#define DI_AFBCE_MMU_RMIF_RO_STAT	(0x2086)
#endif
/* AFBCE bits index */
#define E_AFBCE_AFBCE_ST	(0)
#define E_AFBCE_ENC_EN	(1)
#define E_AFBCE_ENC_EN_MODE	(2)
#define E_AFBCE_ENC_RST_MODE	(3)
#define E_AFBCE_AFBCE_SYNC_SEL	(4)
#define E_AFBCE_GCLK_CTRL	(5)
#define E_AFBCE_REG_FMT444_CMB	(6)
#define E_AFBCE_BURST_MODE	(7)
#define E_AFBCE_HOLD_LINE_NUM	(8)
#define E_AFBCE_MIF_URGENT	(9)
#define E_AFBCE_REV_MODE	(10)
#define E_AFBCE_SOFT_RST	(11)
#define E_AFBCE_V_IN	(12)
#define E_AFBCE_H_IN	(13)
#define E_AFBCE_VBLK_SIZE	(14)
#define E_AFBCE_HBLK_SIZE	(15)
#define E_AFBCE_HEAD_BADDR	(16)
#define E_AFBCE_MMU_PAGE_SIZE	(17)
#define E_AFBCE_UNCMP_SIZE	(18)
#define E_AFBCE_CMD_BLK_SIZE	(19)
#define E_AFBCE_DDR_BLK_SIZE	(20)
#define E_AFBCE_ENC_WIN_BGN_H	(21)
#define E_AFBCE_ENC_WIN_END_H	(22)
#define E_AFBCE_ENC_WIN_BGN_V	(23)
#define E_AFBCE_ENC_WIN_END_V	(24)
#define E_AFBCE_LBUF_DEPTH	(25)
#define E_AFBCE_FMT_YBUF_DEPTH	(26)
#define E_AFBCE_BLK_BGN_H	(27)
#define E_AFBCE_BLK_END_H	(28)
#define E_AFBCE_BLK_BGN_V	(29)
#define E_AFBCE_BLK_END_V	(30)
#define E_AFBCE_COMPBITS_Y	(31)
#define E_AFBCE_COMPBITS_C	(32)
#define E_AFBCE_FORMAT_MODE	(33)
#define E_AFBCE_FORCE_ORDER_EN	(34)
#define E_AFBCE_FORCE_ORDER_VALUE	(35)
#define E_AFBCE_DWDS_PADDING_UV128	(36)
#define E_AFBCE_INPUT_PADDING_UV128	(37)
#define E_AFBCE_UNCOMPRESS_SPLIT_MODE	(38)
#define E_AFBCE_ENABLE_16X4BLOCK	(39)
#define E_AFBCE_MINVAL_YENC_EN	(40)
#define E_AFBCE_DISABLE_ORDER_I0	(41)
#define E_AFBCE_DISABLE_ORDER_I1	(42)
#define E_AFBCE_DISABLE_ORDER_I2	(43)
#define E_AFBCE_DISABLE_ORDER_I3	(44)
#define E_AFBCE_DISABLE_ORDER_I4	(45)
#define E_AFBCE_DISABLE_ORDER_I5	(46)
#define E_AFBCE_DISABLE_ORDER_I6	(47)
#define E_AFBCE_ADPT_XINTERLEAVE_CHRM_RIDE	(48)
#define E_AFBCE_ADPT_XINTERLEAVE_LUMA_RIDE	(49)
#define E_AFBCE_ADPT_YINTERLEAVE_CHRM_RIDE	(50)
#define E_AFBCE_ADPT_YINTERLEAVE_LUMA_RIDE	(51)
#define E_AFBCE_ADPT_INTERLEAVE_CMODE	(52)
#define E_AFBCE_ADPT_INTERLEAVE_YMODE	(53)
#define E_AFBCE_DWSCALAR_H1	(54)
#define E_AFBCE_DWSCALAR_H0	(55)
#define E_AFBCE_DWSCALAR_W1	(56)
#define E_AFBCE_DWSCALAR_W0	(57)
#define E_AFBCE_ENC_DEFALUTCOLOR_0	(58)
#define E_AFBCE_ENC_DEFALUTCOLOR_3	(59)
#define E_AFBCE_ENC_DEFALUTCOLOR_1	(60)
#define E_AFBCE_ENC_DEFALUTCOLOR_2	(61)
#define E_AFBCE_QUANT_EN0	(62)
#define E_AFBCE_QUANT_EN1	(63)
#define E_AFBCE_BCLEAV_OFST	(64)
#define E_AFBCE_QUANT_EXP_EN0	(65)
#define E_AFBCE_QUANT_EXP_EN1	(66)
#define E_AFBCE_IQUANT_YCLUT_0_4	(67)
#define E_AFBCE_IQUANT_YCLUT_0_5	(68)
#define E_AFBCE_IQUANT_YCLUT_0_6	(69)
#define E_AFBCE_IQUANT_YCLUT_0_7	(70)
#define E_AFBCE_IQUANT_YCLUT_0_8	(71)
#define E_AFBCE_IQUANT_YCLUT_0_9	(72)
#define E_AFBCE_IQUANT_YCLUT_0_10	(73)
#define E_AFBCE_IQUANT_YCLUT_0_11	(74)
#define E_AFBCE_IQUANT_YCLUT_0_0	(75)
#define E_AFBCE_IQUANT_YCLUT_0_1	(76)
#define E_AFBCE_IQUANT_YCLUT_0_2	(77)
#define E_AFBCE_IQUANT_YCLUT_0_3	(78)
#define E_AFBCE_IQUANT_YCLUT_1_4	(79)
#define E_AFBCE_IQUANT_YCLUT_1_5	(80)
#define E_AFBCE_IQUANT_YCLUT_1_6	(81)
#define E_AFBCE_IQUANT_YCLUT_1_7	(82)
#define E_AFBCE_IQUANT_YCLUT_1_8	(83)
#define E_AFBCE_IQUANT_YCLUT_1_9	(84)
#define E_AFBCE_IQUANT_YCLUT_1_10	(85)
#define E_AFBCE_IQUANT_YCLUT_1_11	(86)
#define E_AFBCE_IQUANT_YCLUT_1_0	(87)
#define E_AFBCE_IQUANT_YCLUT_1_1	(88)
#define E_AFBCE_IQUANT_YCLUT_1_2	(89)
#define E_AFBCE_IQUANT_YCLUT_1_3	(90)
#define E_AFBCE_RQUANT_YCLUT_0_4	(91)
#define E_AFBCE_RQUANT_YCLUT_0_5	(92)
#define E_AFBCE_RQUANT_YCLUT_0_6	(93)
#define E_AFBCE_RQUANT_YCLUT_0_7	(94)
#define E_AFBCE_RQUANT_YCLUT_0_8	(95)
#define E_AFBCE_RQUANT_YCLUT_0_9	(96)
#define E_AFBCE_RQUANT_YCLUT_0_10	(97)
#define E_AFBCE_RQUANT_YCLUT_0_11	(98)
#define E_AFBCE_RQUANT_YCLUT_0_0	(99)
#define E_AFBCE_RQUANT_YCLUT_0_1	(100)
#define E_AFBCE_RQUANT_YCLUT_0_2	(101)
#define E_AFBCE_RQUANT_YCLUT_0_3	(102)
#define E_AFBCE_RQUANT_YCLUT_1_4	(103)
#define E_AFBCE_RQUANT_YCLUT_1_5	(104)
#define E_AFBCE_RQUANT_YCLUT_1_6	(105)
#define E_AFBCE_RQUANT_YCLUT_1_7	(106)
#define E_AFBCE_RQUANT_YCLUT_1_8	(107)
#define E_AFBCE_RQUANT_YCLUT_1_9	(108)
#define E_AFBCE_RQUANT_YCLUT_1_10	(109)
#define E_AFBCE_RQUANT_YCLUT_1_11	(110)
#define E_AFBCE_RQUANT_YCLUT_1_0	(111)
#define E_AFBCE_RQUANT_YCLUT_1_1	(112)
#define E_AFBCE_RQUANT_YCLUT_1_2	(113)
#define E_AFBCE_RQUANT_YCLUT_1_3	(114)
#define E_AFBCE_MODE_422TO420	(115)
#define E_AFBCE_MODE_444TO422	(116)
#define E_AFBCE_DUMMY_DATA	(117)
#define E_AFBCE_FRM_END_CLR	(118)
#define E_AFBCE_ENC_ERROR_CLR	(119)
#define E_AFBCE_PACK_MODE	(120)
#define E_AFBCE_X_REV	(121)
#define E_AFBCE_Y_REV	(122)
#define E_AFBCE_LITTLE_ENDIAN	(123)
#define E_AFBCE_SWAP_64BIT	(124)
#define E_AFBCE_BURST_LEN	(125)
#define E_AFBCE_CMD_REQ_SIZE	(126)
#define E_AFBCE_CMD_INTR_LEN	(127)
#define E_AFBCE_CANVAS_ID	(128)
#define E_AFBCE_SYNC_SEL	(129)
#define E_AFBCE_UNGENT_CTRL	(130)
#define E_AFBCE_RMIF_GCLK_CTRL	(131)
#define E_AFBCE_SW_RST	(132)
#define E_AFBCE_STRIDE	(133)
#define E_AFBCE_ACC_MODE	(134)
#define E_AFBCE_BADDR	(135)
#define E_AFBCE_X_START	(136)
#define E_AFBCE_X_END	(137)
#define E_AFBCE_Y_START	(138)
#define E_AFBCE_Y_END	(139)
#define E_AFBCE_STATUS	(140)

/********************************************************************/
/* bits define */
#ifdef MARK_SC2
#define AFBCE_ENABLE		(0x41a0)
#define AFBCE_MODE		(0x41a1)
#define AFBCE_SIZE_IN		(0x41a2)
#define AFBCE_BLK_SIZE_IN		(0x41a3)
#define AFBCE_HEAD_BADDR		(0x41a4)
#define AFBCE_MIF_SIZE		(0x41a5)
#define AFBCE_PIXEL_IN_HOR_SCOPE		(0x41a6)
#define AFBCE_PIXEL_IN_VER_SCOPE		(0x41a7)
#define AFBCE_CONV_CTRL		(0x41a8)
#define AFBCE_MIF_HOR_SCOPE		(0x41a9)
#define AFBCE_MIF_VER_SCOPE		(0x41aa)
/**/
#define AFBCE_FORMAT		(0x41ad)
#define AFBCE_DEFCOLOR_1		(0x41b0)
#define AFBCE_DEFCOLOR_2		(0x41b1)
#define AFBCE_QUANT_ENABLE		(0x41b2)
#define AFBCE_MMU_NUM		(0x41bf)

#define AFBCE_MMU_RMIF_CTRL1		(0x41c0)
#define AFBCE_MMU_RMIF_CTRL2		(0x41c1)
#define AFBCE_MMU_RMIF_CTRL3		(0x41c2)
#define AFBCE_MMU_RMIF_CTRL4		(0x41c3)
#define AFBCE_MMU_RMIF_SCOPE_X		(0x41c4)
#define AFBCE_MMU_RMIF_SCOPE_Y		(0x41c5)
#endif

union AFBCE_ENABLE_BITS {
	unsigned int d32;
	struct {
		unsigned int afbce_st:1, /* bit 0 */
		//afbce_st Afbce start encod pulse,
		//active when AFBCE_ENABLE[12] high
		reserved3:7,
		enc_en:1,		/* bit 8 */
		reserved2:3,
		enc_en_mode:1,		/* bit 12? */
		/* enc_en_mode:default = 0, */
		//1: chose pulse staert mode,
		//need write a pule by AFBCE_ENABLE[0],
		//0: automatic start
		enc_rst_mode:1,		/* bit[13] ?*/
		reserved1:2,
		afbce_sync_sel:4,	/* bit[19:16]? */
		gclk_ctrl:12;		/* bit[31:20]? */
	} b;
};

union AFBCE_MODE_BITS {
	unsigned int d32;
	struct {
		unsigned int reg_fmt444_cmb:1,/* bit[0] ?*/
		reserved3:13,
		burst_mode:2,		/* bit[15:14] */
		hold_line_num:7,	/* bit[22:16] */
		reserved2:1,
		mif_urgent:2,		/* bit[25:24] */
		rev_mode:2,		/* bit[27:26] */
		reserved1:1,
		soft_rst:3;		/* bit[31:29] */
	} b;
};

union AFBCE_SIZE_IN_BITS {
	unsigned int d32;
	struct {
		unsigned int v_in:13,	/* bit[12:0] */
		reserved2:3,
		h_in:13,		/* bit[28:16] */
		reserved1:3;
	} b;
};

union AFBCE_BLK_SIZE_IN_BITS {
	unsigned int d32;
	struct {
		unsigned int vblk_size:13,	/* bit[12:0] */
		reserved2:3,
		hblk_size:13,		/* bit[28:16] */
		reserved1:3;
	} b;
};

#ifdef MARK_SC2
union AFBCE_HEAD_BADDR_BITS {
	unsigned int d32;
	struct {
		unsigned int head_baddr:32;
	} b;
};
#endif
union AFBCE_MIF_SIZE_BITS {
	unsigned int d32;
	struct {
		unsigned int mmu_page_size:16,	/* bit[15:0] ?*/
		uncmp_size:5,	/* bit[20:16] */
		reserved3:3,
		cmd_blk_size:3,	/* bit[26:24] */
		reserved2:1,
		ddr_blk_size:2,	/* bit[29:28] */
		reserved1:2;
	} b;
};

union AFBCE_PIXEL_IN_HOR_SCOPE_BITS {
	unsigned int d32;
	struct {
		unsigned int enc_win_bgn_h:13,	/* bit[12:0] */
		reserved2:3,
		enc_win_end_h:13,	/* bit[28:16] */
		reserved1:3;
	} b;
};

union AFBCE_PIXEL_IN_VER_SCOPE_BITS {
	unsigned int d32;
	struct {
		unsigned int enc_win_bgn_v:13,	/* bit[12:0] */
		reserved2:3,
		enc_win_end_v:13,	/* bit[28:16] */
		reserved1:3;
	} b;
};

union AFBCE_CONV_CTRL_BITS {	/*diff with ucode, need check if err*/
	unsigned int d32;
	struct {
		unsigned int lbuf_depth:12,	/* bit[11:0] */
		reserved2:4,
		fmt_ybuf_depth:13,	/* bit[28:16] ?*/
		reserved1:3;
	} b;
};

union AFBCE_MIF_HOR_SCOPE_BITS {
	unsigned int d32;
	struct {
		unsigned int blk_bgn_h:10,	/* bit[9:0] */
		reserved2:6,
		blk_end_h:10,	/* bit[25:16] */
		reserved1:6;
	} b;
};

union AFBCE_MIF_VER_SCOPE_BITS {
	unsigned int d32;
	struct {
		unsigned int blk_bgn_v:12,	/* bit[11:0] */
		reserved2:4,
		blk_end_v:12,	/* bit[27:16] */
		reserved1:4;
	} b;
};

/**/

union AFBCE_FORMAT_BITS {
	unsigned int d32;
	struct {
		unsigned int compbits_y:4,	/* bit[3:0] */
		compbits_c:4,		/* bit[7:4] */
		format_mode:2,		/* bit[9:8]? */
		reserved1:22;
	} b;
};

/**/

union AFBCE_DEFCOLOR_1_BITS {
	unsigned int d32;
	struct {
		unsigned int enc_defalutcolor_0:12,	/* bit[11:0] */
		enc_defalutcolor_3:12,	/* bit[23:12] */
		reserved1:8;
	} b;
};

union AFBCE_DEFCOLOR_2_BITS {
	unsigned int d32;
	struct {
		unsigned int enc_defalutcolor_1:12,	/* bit[11:0] */
		enc_defalutcolor_2:12,	/* bit[23:12] */
		reserved1:8;
	} b;
};

union AFBCE_QUANT_ENABLE_BITS {
	unsigned int d32;
	struct {
		unsigned int quant_en0:1, /* bit[0] */
		reserved3:3,
		quant_en1:1,	/* bit[4] */
		reserved2:3,
		bcleav_ofst:2,	/* bit[9:8] */
		quant_exp_en0:1,	/* bit[10] ?*/
		quant_exp_en1:1,	/* bit[11] ?*/
		reserved1:20;
	} b;
};

union AFBCE_MMU_RMIF_CTRL1_BITS {
	unsigned int d32;
	struct {
		unsigned int pack_mode:3,	/* bit[2:0] */
		reserved3:1,
		x_rev:1,	/* bit[4] */
		y_rev:1,	/* bit[5] */
		little_endian:1,	/* bit[6] */
		swap_64bit:1,		/* bit[7] */
		burst_len:2,		/* bit[9:8] */
		cmd_req_size:2,		/* bit[11:10] */
		cmd_intr_len:3,		/* bit[14:12] */
		reserved2:1,
		canvas_id:8,		/* bit[23:16] */
		sync_sel:2,		/* bit[25:24] */
		reserved1:6;
	} b;
};

union AFBCE_MMU_RMIF_CTRL2_BITS {
	unsigned int d32;
	struct {
		unsigned int ungent_ctrl:17,	/* bit[16:0] */
		reserved2:1,
		gclk_ctrl:6,	/* bit[23:18] */
		reserved1:6,	/* bit[29:24] */
		sw_rst:2;	/* bit[31:30] */
	} b;
};

union AFBCE_MMU_RMIF_CTRL3_BITS {
	unsigned int d32;
	struct {
		unsigned int stride:13,	/* bit[12:0] */
		reserved2:3,
		acc_mode:1,	/* bit[16] */
		reserved1:15;
	} b;
};

#ifdef MARK_SC2
union AFBCE_MMU_RMIF_CTRL4_BITS {
	unsigned int d32;
	struct {
		unsigned int baddr:32;
	} b;
};
#endif

union AFBCE_MMU_RMIF_SCOPE_X_BITS {
	unsigned int d32;
	struct {
		unsigned int x_start:13,	/* bit[12:0] */
		reserved2:3,
		x_end:13,	/* bit[28:16] */
		reserved1:3;
	} b;
};

union AFBCE_MMU_RMIF_SCOPE_Y_BITS {
	unsigned int d32;
	struct {
		unsigned int y_start:13,	/* bit[12:0] */
		reserved2:3,
		y_end:13,	/* bit[28:16] */
		reserved1:3;
	} b;
};

//ary-----

struct afbce_bits_s {
	union AFBCE_ENABLE_BITS enable;			/*0*/
	union AFBCE_MODE_BITS mode;			/*1*/
	union AFBCE_SIZE_IN_BITS size_in;		/*2*/
	union AFBCE_BLK_SIZE_IN_BITS blk_size_in;	/*3*/
	unsigned int head_baddr;			/*4*/

	union AFBCE_MIF_SIZE_BITS mif_size;		/*5*/
	union AFBCE_PIXEL_IN_HOR_SCOPE_BITS pixel_in_h_scope;	/*6*/
	union AFBCE_PIXEL_IN_VER_SCOPE_BITS pixel_in_v_scope;	/*7*/
	union AFBCE_CONV_CTRL_BITS conv_ctrl;		/*8*/
	union AFBCE_MIF_HOR_SCOPE_BITS mif_h_scope;	/*9*/
	union AFBCE_MIF_VER_SCOPE_BITS mif_v_scope;	/*10*/
	/**/

	union AFBCE_FORMAT_BITS format;			/*11*/
	/**/
	union AFBCE_DEFCOLOR_1_BITS defcolor_1;		/*12*/
	union AFBCE_DEFCOLOR_2_BITS defcolor_2;		/*13*/

	union AFBCE_QUANT_ENABLE_BITS quant_en;		/*14*/
	//unsigned int mmu_num;
	union AFBCE_MMU_RMIF_CTRL1_BITS mmu_rmif_ctr1;	/*15*/
	union AFBCE_MMU_RMIF_CTRL2_BITS mmu_rmif_ctr2;	/*16*/
	union AFBCE_MMU_RMIF_CTRL3_BITS mmu_rmif_ctr3;	/*17*/
	unsigned int mmu_rmif_ctr4_baddr;		/*18*/
	union AFBCE_MMU_RMIF_SCOPE_X_BITS mmu_rmif_scope_x;	/*19*/
	union AFBCE_MMU_RMIF_SCOPE_Y_BITS mmu_rmif_scope_y;	/*20*/
};

//ary not use?-----

//#define AFBCE_MODE_EN		(0x41ae)
union AFBCE_MODE_EN_BITS {
	unsigned int d32;
	struct {
		unsigned int force_order_en:1,
		force_order_value:3,
		dwds_padding_uv128:1,
		input_padding_uv128:1,
		reserved4:2,
		uncompress_split_mode:1,
		enable_16x4block:1,
		minval_yenc_en:1,
		reserved3:1,
		disable_order_i0:1,
		disable_order_i1:1,
		disable_order_i2:1,
		disable_order_i3:1,
		disable_order_i4:1,
		disable_order_i5:1,
		disable_order_i6:1,
		reserved2:1,
		adpt_xinterleave_chrm_ride:1,
		adpt_xinterleave_luma_ride:1,
		adpt_yinterleave_chrm_ride:1,
		adpt_yinterleave_luma_ride:1,
		adpt_interleave_cmode:1,
		adpt_interleave_ymode:1,
		reserved1:6;
	} b;
};

//#define AFBCE_DWSCALAR		(0x41af)
union AFBCE_DWSCALAR_BITS {
	unsigned int d32;
	struct {
		unsigned int dwscalar_h1:2,
		dwscalar_h0:2,
		dwscalar_w1:2,
		dwscalar_w0:2,
		reserved1:24;
	} b;
};

//#define AFBCE_IQUANT_LUT_1		(0x41b3)
union AFBCE_IQUANT_LUT_1_BITS {
	unsigned int d32;
	struct {
		unsigned int iquant_yclut_0_4:3,
		reserved8:1,
		iquant_yclut_0_5:3,
		reserved7:1,
		iquant_yclut_0_6:3,
		reserved6:1,
		iquant_yclut_0_7:3,
		reserved5:1,
		iquant_yclut_0_8:3,
		reserved4:1,
		iquant_yclut_0_9:3,
		reserved3:1,
		iquant_yclut_0_10:3,
		reserved2:1,
		iquant_yclut_0_11:3,
		reserved1:1;
	} b;
};

//#define AFBCE_IQUANT_LUT_2		(0x41b4)
union AFBCE_IQUANT_LUT_2_BITS {
	unsigned int d32;
	struct {
		unsigned int iquant_yclut_0_0:3,
		reserved4:1,
		iquant_yclut_0_1:3,
		reserved3:1,
		iquant_yclut_0_2:3,
		reserved2:1,
		iquant_yclut_0_3:3,
		reserved1:17;
	} b;
};

//#define AFBCE_IQUANT_LUT_3		(0x41b5)
union AFBCE_IQUANT_LUT_3_BITS {
	unsigned int d32;
	struct {
		unsigned int iquant_yclut_1_4:3,
		reserved8:1,
		iquant_yclut_1_5:3,
		reserved7:1,
		iquant_yclut_1_6:3,
		reserved6:1,
		iquant_yclut_1_7:3,
		reserved5:1,
		iquant_yclut_1_8:3,
		reserved4:1,
		iquant_yclut_1_9:3,
		reserved3:1,
		iquant_yclut_1_10:3,
		reserved2:1,
		iquant_yclut_1_11:3,
		reserved1:1;
	} b;
};

//#define AFBCE_IQUANT_LUT_4		(0x41b6)
union AFBCE_IQUANT_LUT_4_BITS {
	unsigned int d32;
	struct {
		unsigned int iquant_yclut_1_0:3,
		reserved4:1,
		iquant_yclut_1_1:3,
		reserved3:1,
		iquant_yclut_1_2:3,
		reserved2:1,
		iquant_yclut_1_3:3,
		reserved1:17;
	} b;
};

//#define AFBCE_RQUANT_LUT_1		(0x41b7)
union AFBCE_RQUANT_LUT_1_BITS {
	unsigned int d32;
	struct {
		unsigned int rquant_yclut_0_4:3,
		reserved8:1,
		rquant_yclut_0_5:3,
		reserved7:1,
		rquant_yclut_0_6:3,
		reserved6:1,
		rquant_yclut_0_7:3,
		reserved5:1,
		rquant_yclut_0_8:3,
		reserved4:1,
		rquant_yclut_0_9:3,
		reserved3:1,
		rquant_yclut_0_10:3,
		reserved2:1,
		rquant_yclut_0_11:3,
		reserved1:1;
	} b;
};

//#define AFBCE_RQUANT_LUT_2		(0x41b8)
union AFBCE_RQUANT_LUT_2_BITS {
	unsigned int d32;
	struct {
		unsigned int rquant_yclut_0_0:3,
		reserved4:1,
		rquant_yclut_0_1:3,
		reserved3:1,
		rquant_yclut_0_2:3,
		reserved2:1,
		rquant_yclut_0_3:3,
		reserved1:17;
	} b;
};

//#define AFBCE_RQUANT_LUT_3		(0x41b9)
union AFBCE_RQUANT_LUT_3_BITS {
	unsigned int d32;
	struct {
		unsigned int rquant_yclut_1_4:3,
		reserved8:1,
		rquant_yclut_1_5:3,
		reserved7:1,
		rquant_yclut_1_6:3,
		reserved6:1,
		rquant_yclut_1_7:3,
		reserved5:1,
		rquant_yclut_1_8:3,
		reserved4:1,
		rquant_yclut_1_9:3,
		reserved3:1,
		rquant_yclut_1_10:3,
		reserved2:1,
		rquant_yclut_1_11:3,
		reserved1:1;
	} b;
};

//#define AFBCE_RQUANT_LUT_4		(0x41ba)
union AFBCE_RQUANT_LUT_4_BITS {
	unsigned int d32;
	struct {
		unsigned int rquant_yclut_1_0:3,
		reserved4:1,
		rquant_yclut_1_1:3,
		reserved3:1,
		rquant_yclut_1_2:3,
		reserved2:1,
		rquant_yclut_1_3:3,
		reserved1:17;
	} b;
};

//#define AFBCE_YUV_FORMAT_CONV_MODE		(0x41bb)
union AFBCE_YUV_FORMAT_CONV_MODE_BITS {
	unsigned int d32;
	struct {
		unsigned int mode_422to420:3,
		reserved2:1,
		mode_444to422:3,
		reserved1:25;
	} b;
};

//#define AFBCE_DUMMY_DATA		(0x41bc)
union AFBCE_DUMMY_DATA_BITS {
	unsigned int d32;
	struct {
		unsigned int dummy_data:30,
		reserved1:2;
	} b;
};

//#define AFBCE_CLR_FLAG		(0x41bd)
union AFBCE_CLR_FLAG_BITS {
	unsigned int d32;
	struct {
		unsigned int frm_end_clr:1,
		enc_error_clr:1,
		reserved1:30;
	} b;
};

/* read only */
//#define AFBCE_STAT1		(0x41ab)
//#define AFBCE_STAT2		(0x410c)

//#define AFBCE_STA_FLAGT		(0x41be)
//#define AFBCE_MMU_RMIF_RO_STAT		(0x41c6)
union AFBCE_MMU_RMIF_RO_STAT_BITS {
	unsigned int d32;
	struct {
		unsigned int status:16,
		reserved1:16;
	} b;
};

/**/
struct afbce_ctr_s {
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
	/*need set as config*/
	unsigned int compbits_y;/*bits num after compression*/
	unsigned int compbits_c;
	unsigned int format_mode;/*0:444 1:422 2:420*/
	unsigned int loosy_mode;
	/*loosy_mode*/
	//0:close 1:luma lossy 2:chrma lossy 3: luma & chrma lossy
	unsigned int rev_mode;
};

struct reg_afbce_s {
	unsigned int support;
	struct afbce_ctr_s ctr;
	union {
		struct afbce_bits_s bits;
		unsigned int	d32[DIM_AFBCE_UP_NUB];
	};
	unsigned int addr[DIM_AFBCE_NUB];
};

#endif /*__REG_BITS_AFBCE_H__*/
