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
#define MAX_VPP_NUM     3
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

struct hw_pps_reg_s {
	u32 vd_scale_coef_idx;
	u32 vd_scale_coef;
	u32 vd_vsc_region12_startp;
	u32 vd_vsc_region34_startp;
	u32 vd_vsc_region4_endp;
	u32 vd_vsc_start_phase_step;
	u32 vd_vsc_region1_phase_slope;
	u32 vd_vsc_region3_phase_slope;
	u32 vd_vsc_phase_ctrl;
	u32 vd_vsc_init_phase;
	u32 vd_hsc_region12_startp;
	u32 vd_hsc_region34_startp;
	u32 vd_hsc_region4_endp;
	u32 vd_hsc_start_phase_step;
	u32 vd_hsc_region1_phase_slope;
	u32 vd_hsc_region3_phase_slope;
	u32 vd_hsc_phase_ctrl;
	u32 vd_sc_misc;
	u32 vd_hsc_phase_ctrl1;
	u32 vd_prehsc_coef;
	u32 vd_pre_scale_ctrl;/* sc2 VPP_PREHSC_CTRL */
	u32 vd_prevsc_coef;
	u32 vd_prehsc_coef1;
};

struct hw_vpp_blend_reg_s {
	u32 preblend_h_start_end;
	u32 preblend_v_start_end;
	u32 preblend_h_size;
	u32 postblend_h_start_end;
	u32 postblend_v_start_end;
	/* pip window */
	u32 vd_pip_alph_ctrl;
	u32 vd_pip_alph_scp_h;
	u32 vd_pip_alph_scp_v;
};

struct hw_vppx_blend_reg_s {
	u32 vpp_bld_din0_hscope;
	u32 vpp_bld_din0_vscope;
	u32 vpp_bld_out_size;
	u32 vpp_bld_ctrl;
	u32 vpp_bld_dummy_data;
	u32 vpp_bld_dummy_alpha;
};

struct hw_viu_misc_reg_s   {
	u32 mali_afbcd_top_ctrl;
	u32 mali_afbcd1_top_ctrl;
	u32 mali_afbcd2_top_ctrl;
	u32 vpp_vd1_top_ctrl;
	u32 vpp_vd2_top_ctrl;
	u32 vpp_vd3_top_ctrl;
	u32 vd_path_misc_ctrl;
	u32 path_start_sel;
	u32 vpp_misc;
	u32 vpp_misc1;
};

struct hw_vpp_path_size_s {
	u32 vd1_hdr_in_size;
	u32 vd2_hdr_in_size;
	u32 vd3_hdr_in_size;
	u32 vpp_line_in_length;
	u32 vpp_pic_in_height;
	u32 vd1_sc_h_startp;
	u32 vd1_sc_h_endp;
	u32 vd1_sc_v_startp;
	u32 vd1_sc_v_endp;
	u32 vd2_sc_h_startp;
	u32 vd2_sc_h_endp;
	u32 vd2_sc_v_startp;
	u32 vd2_sc_v_endp;
	u32 vd3_sc_h_startp;
	u32 vd3_sc_h_endp;
	u32 vd3_sc_v_startp;
	u32 vd3_sc_v_endp;
	u32 vpp_preblend_h_size;
	u32 preblend_vd1_h_start_end;
	u32 preblend_vd1_v_start_end;
	u32 vpp_ve_h_v_size;
	u32 vpp_postblend_h_size;
	u32 vpp_out_h_v_size;
	u32 postblend_vd1_h_start_end;
	u32 postlend_vd1_v_start_end;
	u32 blend_vd2_h_start_end;
	u32 blend_vd2_v_start_end;
	u32 blend_vd3_h_start_end;
	u32 blend_vd3_v_start_end;
};

extern struct hw_vd_reg_s vd_mif_reg_legacy_array[MAX_VD_LAYER_G12];
extern struct hw_vd_reg_s vd_mif_reg_g12_array[MAX_VD_LAYER_G12];
extern struct hw_vd_reg_s vd_mif_reg_sc2_array[MAX_VD_LAYER_G12];
extern struct hw_afbc_reg_s vd_afbc_reg_array[MAX_VD_LAYER_G12];
extern struct hw_afbc_reg_s vd_afbc_reg_sc2_array[MAX_VD_LAYER_G12];
extern struct hw_fg_reg_s fg_reg_g12_array[MAX_VD_LAYER_G12];
extern struct hw_fg_reg_s fg_reg_sc2_array[MAX_VD_LAYER_G12];
extern struct hw_pps_reg_s pps_reg_array[MAX_VD_LAYER_G12];
extern struct hw_pps_reg_s pps_reg_array_t5d[MAX_VD_LAYER_G12];
extern struct hw_vpp_blend_reg_s vpp_blend_reg_array[MAX_VD_LAYER_G12];

extern struct hw_vd_reg_s vd_mif_reg_t7_array[MAX_VD_LAYER_T7];
extern struct hw_vd_linear_reg_s vd_mif_linear_reg_t7_array[MAX_VD_LAYER_T7];
extern struct hw_afbc_reg_s vd_afbc_reg_t7_array[MAX_VD_LAYER_T7];
extern struct hw_fg_reg_s fg_reg_t7_array[MAX_VD_LAYER_T7];
extern struct hw_pps_reg_s pps_reg_t7_array[MAX_VD_LAYER_T7];
extern struct hw_vpp_blend_reg_s vpp_blend_reg_t7_array[MAX_VD_LAYER_T7];
extern struct hw_vpp_path_size_s vpp_path_size_reg;
extern struct hw_viu_misc_reg_s viu_misc_reg;
extern struct hw_vppx_blend_reg_s vppx_blend_reg_array[MAX_VPP_NUM - 1];
#endif
