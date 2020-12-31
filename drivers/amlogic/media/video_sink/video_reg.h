/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef VIDEO_REG_HEADER_HH
#define VIDEO_REG_HEADER_HH

#include <linux/string.h>
#include <linux/ctype.h>

#define MAX_VD_LAYER_G12 2
#define MAX_SR_NUM   2

#define MAX_VD_LAYER_T7 3

struct hw_vd_reg_s {
	u32 vd_if0_gen_reg; /* VD1_IF0_GEN_REG */
	u32 vd_if0_canvas0;/* VD1_IF0_CANVAS0 */
	u32 vd_if0_canvas1;/* VD1_IF0_CANVAS1 */
	u32 vd_if0_luma_x0;/* VD1_IF0_LUMA_X0 */
	u32 vd_if0_luma_y0; /* VD1_IF0_LUMA_Y0 */
	u32 vd_if0_chroma_x0; /* VD1_IF0_CHROMA_X0 */
	u32 vd_if0_chroma_y0; /* VD1_IF0_CHROMA_Y0 */
	u32 vd_if0_luma_x1; /* VD1_IF0_LUMA_X1 */
	u32 vd_if0_luma_y1;/* VD1_IF0_LUMA_Y1 */
	u32 vd_if0_chroma_x1;/* VD1_IF0_CHROMA_X1 */
	u32 vd_if0_chroma_y1;/* VD1_IF0_CHROMA_Y1 */
	u32 vd_if0_rpt_loop; /* VD1_IF0_RPT_LOOP */
	u32 vd_if0_luma0_rpt_pat;/* VD1_IF0_LUMA0_RPT_PAT */
	u32 vd_if0_chroma0_rpt_pat;/* VD1_IF0_CHROMA0_RPT_PAT */
	u32 vd_if0_luma1_rpt_pat;/* VD1_IF0_LUMA1_RPT_PAT */
	u32 vd_if0_chroma1_rpt_pat;/* VD1_IF0_CHROMA1_RPT_PAT */
	u32 vd_if0_luma_psel;/* VD1_IF0_LUMA_PSEL */
	u32 vd_if0_chroma_psel;/* VD1_IF0_CHROMA_PSEL */
	u32 vd_if0_luma_fifo_size;/* VD1_IF0_LUMA_FIFO_SIZE */

	u32 vd_if0_gen_reg2;/* VD1_IF0_GEN_REG2 */
	u32 vd_if0_gen_reg3;/* VD1_IF0_GEN_REG3 */
	u32 viu_vd_fmt_ctrl;/* VIU_VD1_FMT_CTRL */
	u32 viu_vd_fmt_w;/* VIU_VD1_FMT_W */
};

struct hw_vd_linear_reg_s {
	u32 vd_if0_baddr_y;
	u32 vd_if0_baddr_cb;
	u32 vd_if0_baddr_cr;
	u32 vd_if0_stride_0;
	u32 vd_if0_stride_1;
	u32 vd_if0_baddr_y_f1;
	u32 vd_if0_baddr_cb_f1;
	u32 vd_if0_baddr_cr_f1;
	u32 vd_if0_stride_0_f1;
	u32 vd_if0_stride_1_f1;
};

struct hw_afbc_reg_s {
	u32 afbc_enable; /* AFBC_ENABLE */
	u32 afbc_mode;/* AFBC_MODE */
	u32 afbc_size_in;/* AFBC_SIZE_IN */
	u32 afbc_dec_def_color;/* AFBC_DEC_DEF_COLOR */
	u32 afbc_conv_ctrl; /* AFBC_CONV_CTRL */
	u32 afbc_lbuf_depth; /* AFBC_LBUF_DEPTH */
	u32 afbc_head_baddr; /* AFBC_HEAD_BADDR */
	u32 afbc_body_baddr; /* AFBC_BODY_BADDR */
	u32 afbc_size_out;/* AFBC_SIZE_OUT */
	u32 afbc_out_yscope;/* AFBC_OUT_YSCOPE */
	u32 afbc_stat;/* AFBC_STAT */
	u32 afbc_vd_cfmt_ctrl; /* AFBC_VD_CFMT_CTRL */
	u32 afbc_vd_cfmt_w;/* AFBC_VD_CFMT_W */
	u32 afbc_mif_hor_scope;/* AFBC_MIF_HOR_SCOPE */
	u32 afbc_mif_ver_scope;/* AFBC_MIF_VER_SCOPE */
	u32 afbc_pixel_hor_scope;/* AFBC_PIXEL_HOR_SCOPE */
	u32 afbc_pixel_ver_scope;/* AFBC_PIXEL_VER_SCOPE */
	u32 afbc_vd_cfmt_h;/* AFBC_VD_CFMT_H */
	u32 afbcdec_iquant_enable; /* AFBCDEC_IQUANT_ENABLE */
	u32 afbcdec_iquant_lut_1; /* AFBCDEC_IQUANT_LUT_1 */
	u32 afbcdec_iquant_lut_2; /* AFBCDEC_IQUANT_LUT_2 */
	u32 afbcdec_iquant_lut_3; /* AFBCDEC_IQUANT_LUT_3 */
	u32 afbcdec_iquant_lut_4; /* AFBCDEC_IQUANT_LUT_4 */
	u32 afbc_top_ctrl;
};

struct hw_sr_reg_s {
	u32 sr_sharp_sync_ctrl;
};

struct hw_fg_reg_s {
	u32 fgrain_ctrl;
	u32 fgrain_win_h;
	u32 fgrain_win_v;
};

extern struct hw_vd_reg_s vd_mif_reg_legacy_array[MAX_VD_LAYER_G12];
extern struct hw_vd_reg_s vd_mif_reg_g12_array[MAX_VD_LAYER_G12];
extern struct hw_vd_reg_s vd_mif_reg_sc2_array[MAX_VD_LAYER_G12];
extern struct hw_afbc_reg_s vd_afbc_reg_array[MAX_VD_LAYER_G12];
extern struct hw_afbc_reg_s vd_afbc_reg_sc2_array[MAX_VD_LAYER_G12];
extern struct hw_fg_reg_s fg_reg_g12_array[MAX_VD_LAYER_G12];
extern struct hw_fg_reg_s fg_reg_sc2_array[MAX_VD_LAYER_G12];

extern struct hw_vd_reg_s vd_mif_reg_t7_array[MAX_VD_LAYER_T7];
extern struct hw_vd_linear_reg_s vd_mif_linear_reg_t7_array[MAX_VD_LAYER_T7];
extern struct hw_afbc_reg_s vd_afbc_reg_t7_array[MAX_VD_LAYER_T7];
extern struct hw_fg_reg_s fg_reg_t7_array[MAX_VD_LAYER_T7];

#endif
