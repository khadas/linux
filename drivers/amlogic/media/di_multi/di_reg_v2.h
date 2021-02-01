/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * drivers/amlogic/media/di_multi/di_reg_v2.h
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

#ifndef __DI_REG_V2_H__
#define __DI_REG_V2_H__

/**********************************************************
 * register before sc2
 **********************************************************/
#define DI_PRE_GL_CTRL					0x20ab
#define DI_PRE_GL_THD					0x20ac
#define DI_POST_GL_CTRL					0x20ad
#define DI_POST_GL_THD					0x20ae

/* pre nr */
#define DI_NRWR_X                        ((0x17c0)) /* << 2) + 0xd0100000) */
#define DI_NRWR_Y                        ((0x17c1)) /* << 2) + 0xd0100000) */
/* bit 31:30				nrwr_words_lim */
/* bit 29				nrwr_rev_y */
/* bit 28:16				nrwr_start_y */
/* bit 15				nrwr_ext_en */
/* bit 14		Nrwr bit10 mode */
/* bit 12:0				nrwr_end_y */
#define DI_NRWR_CTRL                     ((0x17c2)) /* << 2) + 0xd0100000) */
/* bit 31				pending_ddr_wrrsp_diwr */
/* bit 30				nrwr_reg_swap */
/* bit 29:26				nrwr_burst_lim */
/* bit 25				nrwr_canvas_syncen */
/* bit 24				nrwr_no_clk_gate */
/* bit 23:22				nrwr_rgb_mode
 *					0:422 to one canvas;
 *					1:4:4:4 to one canvas;
 */
/* bit 21:20				nrwr_hconv_mode */
/* bit 19:18				nrwr_vconv_mode */
/* bit 17				nrwr_swap_cbcr */
/* bit 16				nrwr_urgent */
/* bit 15:8				nrwr_canvas_index_chroma */
/* bit 7:0				nrwr_canvas_index_luma */
#define DI_MTNWR_X                       ((0x17c3)) /* << 2) + 0xd0100000) */

/* pst wr */
#define DI_DIWR_X                        ((0x17c6)) /* << 2) + 0xd0100000) */
#define DI_DIWR_Y                        ((0x17c7)) /* << 2) + 0xd0100000) */
/* bit 31:30				diwr_words_lim */
/* bit 29				diwr_rev_y */
/* bit 28:16				diwr_start_y */
/* bit 15				diwr_ext_en */
/* bit 12:0				diwr_end_y */
#define DI_DIWR_CTRL                     ((0x17c8)) /* << 2) + 0xd0100000) */
/* bit 31				pending_ddr_wrrsp_diwr */
/* bit 30				diwr_reg_swap */
/* bit 29:26				diwr_burst_lim */
/* bit 25				diwr_canvas_syncen */
/* bit 24				diwr_no_clk_gate */
/* bit 23:22				diwr_rgb_mode  0:422 to one canvas;
 * 1:4:4:4 to one canvas;
 */
/* bit 21:20				diwr_hconv_mode */
/* bit 19:18				diwr_vconv_mode */
/* bit 17				diwr_swap_cbcr */
/* bit 16				diwr_urgent */
/* bit 15:8				diwr_canvas_index_chroma */
/* bit 7:0				diwr_canvas_index_luma */

/*mif*/
#define DI_INP_GEN_REG                   ((0x17ce)) /* << 2) + 0xd0100000) */
#define DI_INP_CANVAS0                   ((0x17cf)) /* << 2) + 0xd0100000) */
#define DI_INP_LUMA_X0                   ((0x17d0)) /* << 2) + 0xd0100000) */
#define DI_INP_LUMA_Y0                   ((0x17d1)) /* << 2) + 0xd0100000) */
#define DI_INP_CHROMA_X0                 ((0x17d2)) /* << 2) + 0xd0100000) */
#define DI_INP_CHROMA_Y0                 ((0x17d3)) /* << 2) + 0xd0100000) */
#define DI_INP_RPT_LOOP                  ((0x17d4)) /* << 2) + 0xd0100000) */
#define DI_INP_LUMA0_RPT_PAT             ((0x17d5)) /* << 2) + 0xd0100000) */
#define DI_INP_CHROMA0_RPT_PAT           ((0x17d6)) /* << 2) + 0xd0100000) */
#define DI_INP_DUMMY_PIXEL               ((0x17d7)) /* << 2) + 0xd0100000) */
#define DI_INP_LUMA_FIFO_SIZE            ((0x17d8)) /* << 2) + 0xd0100000) */
#define DI_INP_RANGE_MAP_Y               ((0x17ba)) /* << 2) + 0xd0100000) */
#define DI_INP_RANGE_MAP_CB              ((0x17bb)) /* << 2) + 0xd0100000) */
#define DI_INP_RANGE_MAP_CR              ((0x17bc)) /* << 2) + 0xd0100000) */
#define DI_INP_GEN_REG2                  ((0x1791)) /* << 2) + 0xd0100000) */
#define DI_INP_FMT_CTRL                  ((0x17d9)) /* << 2) + 0xd0100000) */
#define DI_INP_FMT_W                     ((0x17da)) /* << 2) + 0xd0100000) */
#define DI_MEM_GEN_REG                   ((0x17db)) /* << 2) + 0xd0100000) */
#define DI_MEM_CANVAS0                   ((0x17dc)) /* << 2) + 0xd0100000) */
#define DI_MEM_LUMA_X0                   ((0x17dd)) /* << 2) + 0xd0100000) */
#define DI_MEM_LUMA_Y0                   ((0x17de)) /* << 2) + 0xd0100000) */
#define DI_MEM_CHROMA_X0                 ((0x17df)) /* << 2) + 0xd0100000) */
#define DI_MEM_CHROMA_Y0                 ((0x17e0)) /* << 2) + 0xd0100000) */
#define DI_MEM_RPT_LOOP                  ((0x17e1)) /* << 2) + 0xd0100000) */
#define DI_MEM_LUMA0_RPT_PAT             ((0x17e2)) /* << 2) + 0xd0100000) */
#define DI_MEM_CHROMA0_RPT_PAT           ((0x17e3)) /* << 2) + 0xd0100000) */
#define DI_MEM_DUMMY_PIXEL               ((0x17e4)) /* << 2) + 0xd0100000) */
#define DI_MEM_LUMA_FIFO_SIZE            ((0x17e5)) /* << 2) + 0xd0100000) */
#define DI_MEM_RANGE_MAP_Y               ((0x17bd)) /* << 2) + 0xd0100000) */
#define DI_MEM_RANGE_MAP_CB              ((0x17be)) /* << 2) + 0xd0100000) */
#define DI_MEM_RANGE_MAP_CR              ((0x17bf)) /* << 2) + 0xd0100000) */
#define DI_MEM_GEN_REG2                  ((0x1792)) /* << 2) + 0xd0100000) */
#define DI_MEM_FMT_CTRL                  ((0x17e6)) /* << 2) + 0xd0100000) */
#define DI_MEM_FMT_W                     ((0x17e7)) /* << 2) + 0xd0100000) */
	/* #define DI_IF1_GEN_REG		    ((0x17e8)) + 0xd0100000) */
#define DI_IF1_CANVAS0                   ((0x17e9)) /* << 2) + 0xd0100000) */
#define DI_IF1_LUMA_X0                   ((0x17ea)) /* << 2) + 0xd0100000) */
#define DI_IF1_LUMA_Y0                   ((0x17eb)) /* << 2) + 0xd0100000) */
#define DI_IF1_CHROMA_X0                 ((0x17ec)) /* << 2) + 0xd0100000) */
#define DI_IF1_CHROMA_Y0                 ((0x17ed)) /* << 2) + 0xd0100000) */
#define DI_IF1_RPT_LOOP                  ((0x17ee)) /* << 2) + 0xd0100000) */
#define DI_IF1_LUMA0_RPT_PAT             ((0x17ef)) /* << 2) + 0xd0100000) */
#define DI_IF1_CHROMA0_RPT_PAT           ((0x17f0)) /* << 2) + 0xd0100000) */
#define DI_IF1_DUMMY_PIXEL               ((0x17f1)) /* << 2) + 0xd0100000) */
#define DI_IF1_LUMA_FIFO_SIZE            ((0x17f2)) /* << 2) + 0xd0100000) */
#define DI_IF1_RANGE_MAP_Y               ((0x17fc)) /* << 2) + 0xd0100000) */
#define DI_IF1_RANGE_MAP_CB              ((0x17fd)) /* << 2) + 0xd0100000) */
#define DI_IF1_RANGE_MAP_CR              ((0x17fe)) /* << 2) + 0xd0100000) */
#define DI_IF1_GEN_REG2                  ((0x1790)) /* << 2) + 0xd0100000) */
#define DI_IF1_FMT_CTRL                  ((0x17f3)) /* << 2) + 0xd0100000) */
#define DI_IF1_FMT_W                     ((0x17f4)) /* << 2) + 0xd0100000) */
#define DI_CHAN2_GEN_REG                 ((0x17f5)) /* << 2) + 0xd0100000) */
#define DI_CHAN2_CANVAS0                 ((0x17f6)) /* << 2) + 0xd0100000) */
#define DI_CHAN2_LUMA_X0                 ((0x17f7)) /* << 2) + 0xd0100000) */
#define DI_CHAN2_LUMA_Y0                 ((0x17f8)) /* << 2) + 0xd0100000) */
#define DI_CHAN2_CHROMA_X0               ((0x17f9)) /* << 2) + 0xd0100000) */
#define DI_CHAN2_CHROMA_Y0               ((0x17fa)) /* << 2) + 0xd0100000) */
#define DI_CHAN2_RPT_LOOP                ((0x17fb)) /* << 2) + 0xd0100000) */
#define DI_CHAN2_LUMA0_RPT_PAT           ((0x17b0)) /* << 2) + 0xd0100000) */
#define DI_CHAN2_CHROMA0_RPT_PAT         ((0x17b1)) /* << 2) + 0xd0100000) */
#define DI_CHAN2_DUMMY_PIXEL             ((0x17b2)) /* << 2) + 0xd0100000) */
#define DI_CHAN2_LUMA_FIFO_SIZE          ((0x17b3)) /* << 2) + 0xd0100000) */
#define DI_CHAN2_RANGE_MAP_Y             ((0x17b4)) /* << 2) + 0xd0100000) */
#define DI_CHAN2_RANGE_MAP_CB            ((0x17b5)) /* << 2) + 0xd0100000) */
#define DI_CHAN2_RANGE_MAP_CR            ((0x17b6)) /* << 2) + 0xd0100000) */
#define DI_CHAN2_GEN_REG2                ((0x17b7)) /* << 2) + 0xd0100000) */
#define DI_CHAN2_FMT_CTRL                ((0x17b8)) /* << 2) + 0xd0100000) */
#define DI_CHAN2_FMT_W                   ((0x17b9)) /* << 2) + 0xd0100000) */

#define VD1_IF0_GEN_REG				    0x1a50
#define VD1_IF0_LUMA_FIFO_SIZE          0x1a63
/* ((0x1a63  << 2) + 0xd0100000) */

#define VIU_VD1_FMT_CTRL			    0x1a68
/* ((0x1a68  << 2) + 0xd0100000) */
/* Bit 31    it true, disable clock, otherwise enable clock        */
/* Bit 30    soft rst bit */
/* Bit 28    if true, horizontal formatter use repeating to */
/* generete pixel, otherwise use bilinear interpolation */
/* Bit 27:24 horizontal formatter initial phase */
/* Bit 23    horizontal formatter repeat pixel 0 enable */
/* Bit 22:21 horizontal Y/C ratio, 00: 1:1, 01: 2:1, 10: 4:1 */
/* Bit 20    horizontal formatter enable */
/* Bit 19    if true, always use phase0 while vertical formater, */
/* meaning always repeat data, no interpolation */
/* Bit 18    if true, disable vertical formatter chroma repeat last line */
/* Bit 17    veritcal formatter dont need repeat */
/* line on phase0, 1: enable, 0: disable */
/* Bit 16    veritcal formatter repeat line 0 enable */
/* Bit 15:12 vertical formatter skip line num at the beginning */
/* Bit 11:8  vertical formatter initial phase */
/* Bit 7:1   vertical formatter phase step (3.4) */
/* Bit 0     vertical formatter enable */
#define VIU_VD1_FMT_W				    0x1a69
/* ((0x1a69  << 2) + 0xd0100000) */
/* Bit 27:16  horizontal formatter width */
/* Bit 11:0   vertical formatter width */

#define VD1_IF0_GEN_REG2		0x1a6d

#define DI_INP_GEN_REG3                 0x20a8
		/* 0xd01082a0 */
/*bit9:8	bit mode: 0 = 8bits, 1=10bits 422,  2 = 10bits 444 */
#define DI_MEM_GEN_REG3                 0x20a9
		/* 0xd01082a4 */
/*bit9:8	bit mode: 0 = 8bits, 1=10bits 422,  2 = 10bits 444 */
#define DI_CHAN2_GEN_REG3               0x20aa
		/* 0xd01082a8 */

/* txl add if2 */
#define DI_IF2_GEN_REG					0x2010
#define DI_IF2_CANVAS0					0x2011
#define DI_IF2_LUMA_X0					0x2012
#define DI_IF2_LUMA_Y0					0x2013
#define DI_IF2_CHROMA_X0				0x2014
#define DI_IF2_CHROMA_Y0				0x2015
#define DI_IF2_RPT_LOOP					0x2016
#define DI_IF2_LUMA0_RPT_PAT			0x2017
#define DI_IF2_CHROMA0_RPT_PAT			0x2018
#define DI_IF2_DUMMY_PIXEL				0x2019
#define DI_IF2_LUMA_FIFO_SIZE			0x201a
#define DI_IF2_RANGE_MAP_Y				0x201b
#define DI_IF2_RANGE_MAP_CB				0x201c
#define DI_IF2_RANGE_MAP_CR				0x201d
#define DI_IF2_GEN_REG2					0x201e
#define DI_IF2_FMT_CTRL					0x201f
#define DI_IF2_FMT_W					0x2020
#define DI_IF2_URGENT_CTRL				0x2021
#define DI_IF2_GEN_REG3					0x2022
/*txl new add end*/

/* g12 new added */
/* IF0 MIF */
#define DI_IF0_GEN_REG					0x2030
#define DI_IF0_CANVAS0					0x2031
#define DI_IF0_LUMA_X0					0x2032
#define DI_IF0_LUMA_Y0					0x2033
#define DI_IF0_CHROMA_X0				0x2034
#define DI_IF0_CHROMA_Y0				0x2035
#define DI_IF0_REPEAT_LOOP				0x2036
#define DI_IF0_LUMA0_RPT_PAT			0x2037
#define DI_IF0_CHROMA0_RPT_PAT			0x2038
#define DI_IF0_DUMMY_PIXEL				0x2039
#define DI_IF0_LUMA_FIFO_SIZE			0x203A
#define DI_IF0_RANGE_MAP_Y				0x203B
#define DI_IF0_RANGE_MAP_CB				0x203C
#define DI_IF0_RANGE_MAP_CR				0x203D
#define DI_IF0_GEN_REG2					0x203E
#define DI_IF0_FMT_CTRL					0x203F
#define DI_IF0_FMT_W					0x2040
#define DI_IF0_FMT_W					0x2040
#define DI_IF0_URGENT_CTRL				0x2041
#define DI_IF0_GEN_REG3					0x2042

#define DI_BLEND_CTRL                     ((0x170d)) /* << 2) + 0xd0100000) */
/* bit 31,      blend_1_en */
/* bit 30,      blend_mtn_lpf */
/* bit 28,      post_mb_en */
/* bit 27,      blend_mtn3p_max */
/* bit 26,      blend_mtn3p_min */
/* bit 25,      blend_mtn3p_ave */
/* bit 24,      blend_mtn3p_maxtb */
/* bit 23,      blend_mtn_flt_en */
/* bit 22,      blend_data_flt_en */
/* bit 21:20,   blend_top_mode */
/* bit 19,      blend_reg3_enable */
/* bit 18,      blend_reg2_enable */
/* bit 17,      blend_reg1_enable */
/* bit 16,      blend_reg0_enable */
/* bit 15:14,   blend_reg3_mode */
/* bit 13:12,   blend_reg2_mode */
/* bit 11:10,   blend_reg1_mode */
/* bit 9:8,     blend_reg0_mode */
/* bit 7:0,     kdeint */
/* `define DI_BLEND_CTRL1            8'h0e */
/* no use */
/* `define DI_BLEND_CTRL2            8'h0f */
/* no use */

/* crc t5/t5d ,function same as sc2*/
#define DI_T5_CRC_CHK0		((0x17cB))
#define DI_T5_CRC_CHK1		((0x17cc))
#define DI_T5_RO_CRC_NRWR	((0x17c3))
#define DI_T5_RO_CRC_MTNWR	((0x17c4))
#define DI_T5_RO_CRC_DEINT	((0x17c5))

/* new pulldown t5,function same as sc2 */
#define DI_T5_PD_RO_SUM_P		((0x20f9))
#define DI_T5_PD_RO_SUM_N		((0x20fa))
#define DI_T5_PD_RO_CNT_P		((0x20fb))
#define DI_T5_PD_RO_CNT_N		((0x20fc))

#endif	/*__DI_REG_V2_H__*/
