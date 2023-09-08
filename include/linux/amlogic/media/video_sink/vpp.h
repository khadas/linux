/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * include/linux/amlogic/media/video_sink/vpp.h
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

#ifndef VPP_H
#define VPP_H

#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/media/video_sink/vpp_afd.h>

#define TV_3D_FUNCTION_OPEN
#define TV_REVERSE

extern bool super_scaler;
extern struct sr_info_s sr_info;
#define VPP_FLAG_WIDEMODE_MASK       0x1F000000
#define VPP_WIDEMODE_BITS            24
#define VPP_FLAG_INTERLACE_OUT       0x00000010
#define VPP_FLAG_INTERLACE_IN        0x00000020
#define VPP_FLAG_HAS_DV_EL           0x00000040
#define VPP_FLAG_ZOOM_SHORTSIDE      0x00000080
#define VPP_FLAG_AR_MASK             0x0003ff00
#define VPP_FLAG_AR_BITS             8
#define VPP_FLAG_PORTRAIT_MODE       0x00040000
#define VPP_FLAG_VSCALE_DISABLE      0x00080000
#define VPP_FLAG_MORE_LOG            0x00100000
#define VPP_FLAG_FORCE_NO_COMPRESS   0x00200000
#define VPP_FLAG_FORCE_SWITCH_VF     0x00400000
#define VPP_FLAG_FORCE_NOT_SWITCH_VF 0x00800000
#define VPP_FLAG_FORCE_NO_OFFSET	 0x00000004
#define VPP_FLAG_FORCE_AFD_ENABLE	 0x00000002
#define VPP_FLAG_FROM_TOGGLE_FRAME	0x00000001

#define IDX_H           (2 << 8)
#define IDX_V_Y         BIT(13)
#define IDX_V_CBCR      ((1 << 13) | (1 << 8))

#define ASPECT_4_3      ((3 << 8) / 4)
#define ASPECT_16_9     ((9 << 8) / 16)

#define MAX_VSKIP_COUNT	8

#define SPEED_CHECK_DONE	0
#define SPEED_CHECK_HSKIP	1
#define SPEED_CHECK_VSKIP	2

#define NO_MIRROR  0
#define H_MIRROR   1
#define V_MIRROR   2
#define HV_MIRROR  3

enum vppfilter_state_e {
	vppfilter_fail = -1,
	vppfilter_success = 0,
	vppfilter_success_and_changed,
	vppfilter_success_and_switched,
	vppfilter_changed_but_hold,
	vppfilter_changed_but_switch
};

enum f2v_vphase_type_e {
	F2V_IT2IT = 0,
	F2V_IB2IB,
	F2V_IT2IB,
	F2V_IB2IT,
	F2V_P2IT,
	F2V_P2IB,
	F2V_IT2P,
	F2V_IB2P,
	F2V_P2P,
	F2V_TYPE_MAX
}; /* frame to video conversion type */

struct f2v_vphase_s {
	s8 repeat_skip;
	u8 phase;
};

struct vppfilter_mode_s {
	u32 vpp_hf_start_phase_step;
	u32 vpp_hf_start_phase_slope;
	u32 vpp_hf_end_phase_slope;

	const u32 *vpp_vert_coeff;
	const u32 *vpp_horz_coeff;
	u32 vpp_sc_misc_;
	u32 vpp_vsc_start_phase_step;
	u32 vpp_hsc_start_phase_step;
	bool vpp_pre_vsc_en;
	bool vpp_pre_hsc_en;
	u32 vpp_vert_filter;
	u32 vpp_horz_filter;
	const u32 *vpp_vert_chroma_coeff;
	u32 vpp_vert_chroma_filter_en;
};

struct vpp_filters_s {
	struct vppfilter_mode_s *top;
	struct vppfilter_mode_s *bottom;
};

struct vpp_frame_par_s {
	u32 VPP_hd_start_lines_;
	u32 VPP_hd_end_lines_;
	u32 VPP_vd_start_lines_;
	u32 VPP_vd_end_lines_;

	u32 VPP_vsc_startp;
	u32 VPP_vsc_endp;

	u32 VPP_hsc_startp;
	u32 VPP_hsc_linear_startp;
	u32 VPP_hsc_linear_endp;
	u32 VPP_hsc_endp;

	u32 VPP_hf_ini_phase_;
	u32 VPP_vf_init_phase;
	struct f2v_vphase_s VPP_vf_ini_phase_[9];

	u32 VPP_pic_in_height_;
	u32 VPP_line_in_length_;

	u32 VPP_post_blend_vd_v_start_;
	u32 VPP_post_blend_vd_v_end_;
	u32 VPP_post_blend_vd_h_start_;
	u32 VPP_post_blend_vd_h_end_;
	u32 VPP_post_blend_h_size_;

	struct vppfilter_mode_s vpp_filter;

	u32 VPP_postproc_misc_;
	u32 vscale_skip_count;
	u32 hscale_skip_count;
	u32 vpp_3d_mode;
	u32 trans_fmt;
	u32 vpp_2pic_mode;
	/* bit[1:0] 0: 1 pic,1:two pic one buf,2:tow pic two buf */
	/* bit[2]0:select pic0,1:select pic1 */
	/* bit[3]0:pic0 first,1:pic1 first */
	bool vpp_3d_scale;

	bool supsc0_enable;
	bool supsc1_enable;
	bool supsc0_hori_ratio;
	bool supsc1_hori_ratio;
	bool supsc0_vert_ratio;
	bool supsc1_vert_ratio;
	u32 supscl_path;
	u32 spsc0_w_in;
	u32 spsc0_h_in;
	u32 spsc1_w_in;
	u32 spsc1_h_in;
	u32 video_input_w;
	u32 video_input_h;
	u32 cm_input_w;
	u32 cm_input_h;
	u32 nnhf_input_w;
	u32 nnhf_input_h;
	u32 reshape_output_w;
	u32 reshape_output_h;
	u32 reshape_scaler_w;
	u32 reshape_scaler_h;
	u32 aisr_enable;
	u32 clk_in_pps;

	bool nocomp;

	u8 sr0_position;
	u8 sr1_position;
	u8 sr_core_support;
	u32 hsc_rpt_p0_num0;
	u32 vsc_top_rpt_l0_num;
	s64 h_phase[3];
	s64 v_phase[3];
	u32 crop_top;
	u32 crop_bottom;
	u32 crop_left;
	u32 crop_right;
	u32 crop_top_adjust;
	u32 crop_bottom_adjust;
};

struct disp_info_s {
	u8 layer_id;
	u8 layer_support;
	u32 angle;
	u32 custom_ar;

	bool reverse;
	u32 proc_3d_type;
	bool vpp_3d_scale;
	u32 nonlinear_factor;
	u32 nonlinear_t_factor;

	u32 wide_mode;
	u32 zoom_ratio;
	s32 zoom_center_x;
	s32 zoom_center_y;
	s32 layer_top;
	s32 layer_left;
	s32 layer_width;
	s32 layer_height;
	u32 crop_top;
	u32 crop_left;
	u32 crop_bottom;
	u32 crop_right;
	s32 global_offset_x;
	s32 global_offset_y;
	u32 speed_check_width;
	u32 speed_check_height;

	u32 zorder;
	u32 cur_sel_port;
	u32 last_sel_port;
	s32 display_path_id;

	bool afbc_support;
	bool pps_support;

	bool need_no_compress;
	s32 sideband_type;
	bool fgrain_support;
	bool fgrain_enable;
	bool fgrain_start;
	bool fgrain_force_update;
	bool lut_dma_support;
	bool dv_support;
	bool alpha_support;
	u32 mirror;
	u32 src_width_max;
	u32 src_height_max;
	u32 slice_num;
	bool ver_coef_adjust;
	bool afd_enable;
	struct crop_rect_s afd_crop;
	struct pos_rect_s afd_pos;
};

enum select_scaler_path_e {
	CORE0_PPS_CORE1 = 0,	/*CORE0_PPS_CORE1_POSTBLEND*/
	CORE0_CORE1_PPS,	/*CORE0_PPS_POSTBLEND_CORE1*/
	/*gxlx only have core1,support below two mode*/
	CORE1_BEFORE_PPS,
	CORE1_AFTER_PPS,
	/*txhd only have core0,support below two mode*/
	CORE0_BEFORE_PPS,
	CORE0_AFTER_PPS,
	/*tl1 have core0/core1, support below mode*/
	PPS_CORE0_CORE1,
	PPS_CORE0_POSTBLEND_CORE1,
	CORE0_PPS_POSTBLEND_CORE1,
	/* t5d only have core1, support below tow mode */
	PPS_POSTBLEND_CORE1,
	PPS_CORE1_CM,
	SCALER_PATH_MAX,
};

/*
 * note from vlsi!!!
 * if core0 v enable,core0 input width max=1024;
 * if core0 v disable,core0 input width max=2048;
 * if core1 v enable,core1 input width max=2048;
 * if core1 v disable,core1 input width max=4096;
 * gxlx only have core1,txhd/g12a only have core0
 */
#define SUPER_CORE0_SUPPORT  BIT(0)
#define SUPER_CORE1_SUPPORT  BIT(1)

struct sr_info_s {
	u32 sr_support;
	u32 core0_v_enable_width_max;
	u32 core0_v_disable_width_max;
	u32 core1_v_enable_width_max;
	u32 core1_v_disable_width_max;
	u32 sr_reg_offt;
	u32 sr_reg_offt2;	/*for tl1*/
	u32 sr0_sharp_sync_ctrl;
	u32 sr1_sharp_sync_ctrl;
	u8 supscl_path;
	u8 core_support;
};

#ifdef TV_3D_FUNCTION_OPEN
/*cmd use for 3d operation*/
#define MODE_3D_DISABLE     0x00000000
#define MODE_3D_ENABLE      0x00000001
#define MODE_3D_AUTO        0x00000002
#define MODE_3D_LR          0x00000004
#define MODE_3D_TB          0x00000008
#define MODE_3D_LA          0x00000010
#define MODE_3D_FA          0x00000020
#define MODE_3D_LR_SWITCH   0x00000100
#define MODE_3D_TO_2D_L     0x00000200
#define MODE_3D_TO_2D_R     0x00000400
#define MODE_3D_MVC	    0x00000800
#define MODE_3D_OUT_TB	0x00010000
#define MODE_3D_OUT_LR	0x00020000
#define MODE_FORCE_3D_TO_2D_LR	0x00100000
#define MODE_FORCE_3D_TO_2D_TB	0x00200000
#define MODE_FORCE_3D_LR	0x01000000
#define MODE_FORCE_3D_TB	0x02000000
#define MODE_3D_FP			0x04000000
#define MODE_FORCE_3D_FA_LR	0x10000000
#define MODE_FORCE_3D_FA_TB	0x20000000

/*when the output mode is field alterlative*/

/* LRLRLRLRL mode */
#define MODE_3D_OUT_FA_L_FIRST	0x00001000
#define MODE_3D_OUT_FA_R_FIRST	0x00002000

/* LBRBLBRB */
#define MODE_3D_OUT_FA_LB_FIRST 0x00004000
#define MODE_3D_OUT_FA_RB_FIRST	0x00008000

#define MODE_3D_OUT_FA_MASK	\
	(MODE_3D_OUT_FA_L_FIRST | \
	MODE_3D_OUT_FA_R_FIRST | \
	MODE_3D_OUT_FA_LB_FIRST | \
	MODE_3D_OUT_FA_RB_FIRST)

#define MODE_3D_TO_2D_MASK  \
	(MODE_3D_TO_2D_L | \
	MODE_3D_TO_2D_R | \
	MODE_3D_OUT_FA_MASK | \
	MODE_FORCE_3D_TO_2D_LR | \
	MODE_FORCE_3D_TO_2D_TB)

#define VPP_3D_MODE_NULL 0x0
#define VPP_3D_MODE_LR   0x1
#define VPP_3D_MODE_TB   0x2
#define VPP_3D_MODE_LA	 0x3
#define VPP_3D_MODE_FA	 0x4

#define VPP_SELECT_PIC0  0x0
#define VPP_SELECT_PIC1  0x4

#define VPP_PIC0_FIRST	0x0
#define VPP_PIC1_FIRST	0x8

void get_vpp_3d_mode
	(u32 process_3d_type,
	u32 trans_fmt,
	u32 *vpp_3d_mode);
#endif

int vpp_set_filters
	(struct disp_info_s *input,
	struct vframe_s *vf,
	struct vpp_frame_par_s *next_frame_par,
	const struct vinfo_s *vinfo,
	bool bypass_sr, u32 op_flag);

s32 vpp_set_nonlinear_factor(struct disp_info_s *info, u32 f);
u32 vpp_get_nonlinear_factor(struct disp_info_s *info);
s32 vpp_set_nonlinear_t_factor(struct disp_info_s *info, u32 f);
u32 vpp_get_nonlinear_t_factor(struct disp_info_s *info);

void vpp_disp_info_init(struct disp_info_s *info, u8 id);

void vpp_super_scaler_support(void);

void vpp_bypass_ratio_config(void);

int vpp_set_super_scaler_regs
	(int scaler_path_sel,
	int reg_srscl0_enable,
	int reg_srscl0_hsize,
	int reg_srscl0_vsize,
	int reg_srscl0_hori_ratio,
	int reg_srscl0_vert_ratio,
	int reg_srscl1_enable,
	int reg_srscl1_hsize,
	int reg_srscl1_vsize,
	int reg_srscl1_hori_ratio,
	int reg_srscl1_vert_ratio,
	int vpp_postblend_out_width,
	int vpp_postblend_out_height);
struct sr_info_s *get_super_scaler_info(void);
bool get_super_scaler_status(void);
void aisr_sr1_nn_enable(u32 enable);
void aisr_sr1_nn_enable_sync(u32 enable);
void aisr_reshape_output(u32 enable);
void aisr_set_filters(struct disp_info_s *input,
			struct vpp_frame_par_s *next_frame_par,
			struct vframe_s *vf,
			const struct vinfo_s *vinfo,
			u32 vpp_flags);
#endif /* VPP_H */
