/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * drivers/amlogic/media/video_sink/video_reg_s5.h
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

#ifndef VIDEO_REG_S5_HH
#define VIDEO_REG_S5_HH
#include <linux/string.h>
#include <linux/ctype.h>

#define SLICE_NUM 4
#define MAX_VD_LAYER_S5 5
#define MAX_VD_CHAN_S5 2
#define AISR_SCHN     (MAX_VD_LAYER_S5)

struct vd_mif_reg_s {
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

struct vd_mif_linear_reg_s {
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

struct vd_afbc_reg_s {
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

struct vd_fg_reg_s {
	u32 fgrain_ctrl;
	u32 fgrain_win_h;
	u32 fgrain_win_v;
	u32 fgrain_slice_win_h;
};

struct vd_pps_reg_s {
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

struct vd_proc_sr_reg_s {
	u32 vd_proc_s0_sr0_in_size;
	u32 vd_proc_s1_sr0_in_size;
	u32 vd_proc_s0_sr1_in_size;
	u32 vd_proc_sr0_ctrl;
	u32 vd_proc_sr1_ctrl;
	u32 srsharp0_sharp_sr2_ctrl;
	u32 srsharp1_sharp_sr2_ctrl;
	u32 srsharp0_sharp_sr2_ctrl2;
	u32 srsharp1_sharp_sr2_ctrl2;
	u32 srsharp1_nn_post_top;
	u32 srsharp1_demo_mode_window_ctrl0;
	u32 srsharp1_demo_mode_window_ctrl1;
};

struct vd_proc_slice_reg_s {
	u32 vd_proc_s0_in_size;
	u32 vd_proc_s0_out_size;
	u32 vd_s0_hwin_cut;
	u32 vd_s0_detunl_ctrl;
	u32 vd1_hdr_s0_ctrl;
	u32 vd1_s0_dv_bypass_ctrl;
	u32 vd1_s0_clip_misc0;
	u32 vd1_s0_clip_misc1;
};

struct vd_proc_pps_reg_s {
	u32 vd_proc_s0_pps_in_size;
};

struct vd_proc_pi_reg_s {
	u32 vd_proc_pi_ctrl;
};

struct vd_proc_misc_reg_s {
	u32 vd_proc_sr0_ctrl;
	u32 vd_proc_bypass_ctrl;
	u32 vd1_pi_ctrl;
};

struct vd2_proc_misc_reg_s {
	u32 vd2_proc_in_size;
	u32 vd2_detunl_ctrl;
	u32 vd2_hdr_ctrl;
	u32 vd2_pilite_ctrl;
	u32 vd2_proc_out_size;
	u32 vd2_dv_bypass_ctrl;
};

struct vpp_post_blend_reg_s {
	u32 vpp_osd1_bld_h_scope;
	u32 vpp_osd1_bld_v_scope;
	u32 vpp_osd2_bld_h_scope;
	u32 vpp_osd2_bld_v_scope;
	u32 vpp_postblend_vd1_h_start_end;
	u32 vpp_postblend_vd1_v_start_end;
	u32 vpp_postblend_vd2_h_start_end;
	u32 vpp_postblend_vd2_v_start_end;
	u32 vpp_postblend_vd3_h_start_end;
	u32 vpp_postblend_vd3_v_start_end;
	u32 vpp_postblend_h_v_size;
	u32 vpp_post_blend_blend_dummy_data;
	u32 vpp_post_blend_dummy_alpha;
	u32 vpp_post_blend_dummy_alpha1;

	u32 vd1_blend_src_ctrl;
	u32 vd2_blend_src_ctrl;
	u32 vd3_blend_src_ctrl;
	u32 osd1_blend_src_ctrl;
	u32 osd2_blend_src_ctrl;
	u32 vd1_postblend_alpha;
	u32 vd2_postblend_alpha;
	u32 vd3_postblend_alpha;

	u32 vpp_postblend_ctrl;
};

struct vd_proc_blend_reg_s {
	u32 vpp_vd_blnd_h_v_size;
	u32 vpp_vd_blend_dummy_data;
	u32 vpp_vd_blend_dummy_alpha;
	u32 vpp_blend_vd1_s0_h_start_end;
	u32 vpp_blend_vd1_s0_v_start_end;
	u32 vpp_blend_vd1_s1_h_start_end;
	u32 vpp_blend_vd1_s1_v_start_end;
	u32 vpp_blend_vd1_s2_h_start_end;
	u32 vpp_blend_vd1_s2_v_start_end;
	u32 vpp_blend_vd1_s3_h_start_end;
	u32 vpp_blend_vd1_s3_v_start_end;
	u32 vd1_s0_blend_src_ctrl;
	u32 vd1_s1_blend_src_ctrl;
	u32 vd1_s2_blend_src_ctrl;
	u32 vd1_s3_blend_src_ctrl;
	u32 vpp_vd_blnd_ctrl;
};

struct vd1_slice_pad_reg_s {
	u32 vd1_slice_pad_h_size;
	u32 vd1_slice_pad_v_size;
};

struct vd2_pre_blend_reg_s {
	u32 vpp_vd_preblend_h_v_size;
	u32 vpp_vd_pre_blend_dummy_data;
	u32 vpp_vd_pre_blend_dummy_alpha;
	u32 vpp_preblend_vd1_h_start_end;
	u32 vpp_preblend_vd1_v_start_end;
	u32 vpp_preblend_vd2_h_start_end;
	u32 vpp_preblend_vd2_v_start_end;
	u32 vd1_preblend_src_ctrl;
	u32 vd2_preblend_src_ctrl;
	u32 vd1_preblend_alpha;
	u32 vd2_preblend_alpha;
	u32 vpp_vd_preblend_ctrl;
};

struct vd_pip_alpha_reg_s {
	/* pip window */
	u32 vd_pip_alph_ctrl;
	u32 vd_pip_alph_scp_h;
	u32 vd_pip_alph_scp_v;
};

struct vd_aisr_reshape_reg_s {
	u32 aisr_reshape_ctrl0;
	u32 aisr_reshape_ctrl1;
	u32 aisr_reshape_scope_x;
	u32 aisr_reshape_scope_y;
	u32 aisr_reshape_baddr00;
	u32 aisr_reshape_baddr01;
	u32 aisr_reshape_baddr02;
	u32 aisr_reshape_baddr03;
	u32 aisr_reshape_baddr10;
	u32 aisr_reshape_baddr11;
	u32 aisr_reshape_baddr12;
	u32 aisr_reshape_baddr13;
	u32 aisr_reshape_baddr20;
	u32 aisr_reshape_baddr21;
	u32 aisr_reshape_baddr22;
	u32 aisr_reshape_baddr23;
	u32 aisr_reshape_baddr30;
	u32 aisr_reshape_baddr31;
	u32 aisr_reshape_baddr32;
	u32 aisr_reshape_baddr33;
	u32 aisr_post_ctrl;
	u32 aisr_post_size;
	u32 aisr_sr1_nn_post_top;
};

struct vpp_post_misc_reg_s {
	u32 vpp_postblend_ctrl;
	u32 vpp_obuf_ram_ctrl;
	u32 vpp_4p4s_ctrl;
	u32 vpp_4s4p_ctrl;
	u32 vpp_post_vd1_win_cut_ctrl;
	u32 vpp_post_win_cut_ctrl;
	u32 vpp_post_pad_hsize;
	u32 vpp_post_pad_ctrl;
	u32 vpp_out_h_v_size;
	u32 vpp_ofifo_size;
	u32 vpp_slc_deal_ctrl;
	u32 vpp_hwin_size;
	u32 vpp_align_fifo_size;
};

extern struct vpp_post_reg_s vpp_post_reg;
extern struct vpp_post_blend_reg_s vpp_post_blend_reg_s5;
extern struct vpp_post_misc_reg_s vpp_post_misc_reg_s5;

extern struct vd_proc_reg_s vd_proc_reg;
extern struct vd_pps_reg_s pps_reg_s5_array[MAX_VD_LAYER_S5 + 1];
extern struct vd_proc_sr_reg_s vd_proc_sr_reg_s5;
extern struct vd_proc_slice_reg_s vd_proc_slice_reg_s5[SLICE_NUM];
extern struct vd_proc_pi_reg_s vd_proc_pi_reg_s5;
extern struct vd_proc_misc_reg_s vd_proc_misc_reg_s5;
extern struct vd_proc_blend_reg_s vd_proc_blend_reg_s5;
extern struct vd1_slice_pad_reg_s vd1_slice_pad_size0_reg_s5[SLICE_NUM];
extern struct vd1_slice_pad_reg_s vd1_slice_pad_size1_reg_s5[SLICE_NUM];
extern struct vd_aisr_reshape_reg_s aisr_reshape_reg_s5;
extern struct vd2_proc_misc_reg_s vd2_proc_misc_reg_s5;
extern struct vd2_pre_blend_reg_s vd2_pre_blend_reg_s5;
extern struct vd_fg_reg_s fg_reg_s5_array[MAX_VD_LAYER_S5];
extern struct vd_mif_reg_s vd_mif_reg_s5_array[MAX_VD_LAYER_S5];
extern struct vd_mif_linear_reg_s vd_mif_linear_reg_s5_array[MAX_VD_LAYER_S5];
extern struct vd_afbc_reg_s vd_afbc_reg_s5_array[MAX_VD_LAYER_S5];
extern struct vd_pip_alpha_reg_s vd_pip_alpha_reg_s5[MAX_VD_CHAN_S5];
#endif
