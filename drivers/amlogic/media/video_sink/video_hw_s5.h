/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * drivers/amlogic/media/video_sink/video_hw_s5.h
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

#ifndef VIDEO_HW_S5_HH
#define VIDEO_HW_S5_HH
#include "video_reg_s5.h"

#define DEBUG_VD_PROC     BIT(0)
#define DEBUG_VPP_POST    BIT(1)
#define DEBUG_AISR        BIT(2)
#define DEBUG_FG          BIT(3)
#define DEBUG_SR          BIT(4)
#define DEBUG_PPS         BIT(5)
#define DEBUG_VD2_PROC    BIT(6)

enum sr0_path_sel {
	SR0_IN_SLICE0,
	SR0_IN_SLICE1,
};

enum {
	VD1_4SLICES_MODE    = 0,   //
	VD1_2_2SLICES_MODE  = 1,   //slice0/1 and slice2/3
	VD1_SLICES01_MODE   = 2,   //slice0/1
	VD1_SLICES23_MODE   = 3,   //slice2/3
	VD1_1SLICES_MODE    = 4    //slice0
};

enum {
	SR0_AFTER_PPS   = 0,
	SR0_BEFORE_PPS  = 1
};

enum {
	VD1_SLICES_DOUT_PI   = 0,  //pi path
	VD1_SLICES_DOUT_4S4P = 1,  //4slice4ppc path
	VD1_SLICES_DOUT_2S4P = 2,
    /*1SLICES_MODE: blend_1ppc->1slice4ppc path, 4SLICES_MODE: blend_1ppc->1slices4ppc path */
	VD1_SLICES_DOUT_1S4P = 3
};

enum {
	/* pi  path (1ppc->4ppc) */
	VD2_DOUT_PI = 0,
    /* s2p path (2ppc->4ppc) */
	VD2_DOUT_S2P = 1,
    /* vd1s0 prebld path  */
	VD2_DOUT_PREBLD = 2,
    /* vd1s1 prebld path */
	VD2_DOUT_PREBLD1 = 3
};
struct vd_proc_slice_s {
	u32 din_hsize;
	u32 din_vsize;
	u32 dout_hsize;
	u32 dout_vsize;
};

struct vd_proc_pps_s {
	u32 din_hsize;
	u32 din_vsize;
	u32 dout_hsize;
	u32 dout_vsize;
	u32 slice_x_st;
	u32 pps_slice;
	u32 prehsc_en;
	u32 prevsc_en;
	u32 prehsc_rate;
	u32 prevsc_rate;
	u32 horz_phase_step;
	u32 vert_phase_step;
};

struct vd_proc_sr_s {
	u32 sr_en;
	u32 din_hsize;
	u32 din_vsize;
	u32 dout_hsize;
	u32 dout_vsize;
	u32 h_scaleup_en;
	u32 v_scaleup_en;
	u32 sr_support;
	u32 core_v_enable_width_max;
	u32 core_v_disable_width_max;
};

struct vd_proc_hwin_s {
	u32 hwin_en;
	u32 hwin_din_hsize;
	u32 hwin_bgn;
	u32 hwin_end;
};

struct vd_proc_padding_s {
	u32 padding_en;
	u32 slice_pad_h_bgn;
	u32 slice_pad_v_bgn;
	u32 slice_pad_h_end;
	u32 slice_pad_v_end;
};

struct vd_proc_pi_s {
	u32 pi_en;
	u32 pi_in_hsize;
	u32 pi_in_vsize;
};

struct vd_proc_blend_s {
	u32 bld_out_en;
	u32 bld_out_w;
	u32 bld_out_h;
	u32 bld_dummy_data;

	u32 bld_src1_sel;
	u32 bld_src2_sel;
	u32 bld_src3_sel;
	u32 bld_src4_sel;

	u32 bld_din0_h_start;
	u32 bld_din0_h_end;
	u32 bld_din0_v_start;
	u32 bld_din0_v_end;

	u32 bld_din1_h_start;
	u32 bld_din1_h_end;
	u32 bld_din1_v_start;
	u32 bld_din1_v_end;

	u32 bld_din2_h_start;
	u32 bld_din2_h_end;
	u32 bld_din2_v_start;
	u32 bld_din2_v_end;

	u32 bld_din3_h_start;
	u32 bld_din3_h_end;
	u32 bld_din3_v_start;
	u32 bld_din3_v_end;

	//usually the bottom layer set 1, for example postbld_src1_sel = 1,set 0x1
	u32 bld_din0_premult_en;
	u32 bld_din1_premult_en;
	u32 bld_din2_premult_en;
	u32 bld_din3_premult_en;
};

struct vd_proc_unit_s {
	u32 slice_index;
	u32 sr0_dpath_sel;
	u32 sr0_pps_dpsel;
	u32 sr0_en;
	u32 sr1_en;
	u32 reg_bypass_prebld;
	u32 din_hsize;
	u32 din_vsize;
	u32 dout_hsize;
	u32 dout_vsize;
	u32 dout_x_start;
	u32 dout_y_start;
	u32 prebld_hsize;
	u32 prebld_vsize;
	u32 bypass_detunnel;
	u32 bypass_hdr;
	u32 bypass_dv;
	u32 bypass_ve;
	struct vd_proc_slice_s vd_proc_slice;
	struct vd_proc_pps_s vd_proc_pps;
	struct vd_proc_sr_s vd_proc_sr0;
	struct vd_proc_sr_s vd_proc_sr1;
	struct vd_proc_hwin_s vd_proc_hwin;
	struct vd_proc_padding_s vd_proc_padding;
};

struct vd_proc_mosaic_s {
	u32 vd1_proc_slice_pad_en[SLICE_NUM];
	u32 vd1_proc_slice_pad_h_bgn_0[SLICE_NUM];
	u32 vd1_proc_slice_pad_h_end_0[SLICE_NUM];
	u32 vd1_proc_slice_pad_v_bgn_0[SLICE_NUM];
	u32 vd1_proc_slice_pad_v_end_0[SLICE_NUM];
	u32 vd1_proc_slice_pad_h_bgn_1[SLICE_NUM];
	u32 vd1_proc_slice_pad_h_end_1[SLICE_NUM];
	u32 vd1_proc_slice_pad_v_bgn_1[SLICE_NUM];
	u32 vd1_proc_slice_pad_v_end_1[SLICE_NUM];
	u32 mosaic_vd1_dout_hsize;
	u32 mosaic_vd1_dout_vsize;
	u32 mosaic_vd2_dout_hsize;
	u32 mosaic_vd2_dout_vsize;
	u32 h_padding;
	u32 v_padding;
};

struct vd_proc_slice_info_s {
	/* vd1 Real input size to each slice */
	/* with overlap */
	u32 vd1_slice_din_hsize[SLICE_NUM];
	u32 vd1_slice_din_vsize[SLICE_NUM];
	u32 vd1_slice_x_st[SLICE_NUM];
	u32 vd1_slice_x_end[SLICE_NUM];
};

struct vd_proc_preblend_info_s {
	u32 vd1s0_vd2_prebld_en;/* input */
	u32 vd1s1_vd2_prebld_en;/* input */
	u32 prebld_dout_hsize;  /* input */
	u32 prebld_dout_vsize;  /* input */
};

struct vd_proc_vd2_info_s {
	u32 vd2_dout_dpsel;
	u32 vd2_din_hsize;
	u32 vd2_din_vsize;
	u32 vd2_dout_hsize;
	u32 vd2_dout_vsize;
	u32 vd2_dout_x_start;
	u32 vd2_dout_y_start;
	u32 crop_left;
};

struct vd_proc_vd1_info_s {
	u32 slice_num;
	u32 vd1_work_mode;
	u32 vd1_slices_dout_dpsel;
	u32 vd1_overlap_hsize;
	/* whole frame in hsize */
	u32 vd1_src_din_hsize[SLICE_NUM];
	u32 vd1_src_din_vsize[SLICE_NUM];
	/* without overlap */
	u32 vd1_proc_unit_dout_hsize[SLICE_NUM];
	u32 vd1_proc_unit_dout_vsize[SLICE_NUM];
	/* whole vd1 output size */
	u32 vd1_dout_hsize[SLICE_NUM];
	u32 vd1_dout_vsize[SLICE_NUM];
	u32 vd1_dout_x_start[SLICE_NUM];
	u32 vd1_dout_y_start[SLICE_NUM];
	u32 vd1_whole_dout_x_start;
	u32 vd1_whole_dout_y_start;
	u32 vd1_whole_hsize;
	u32 vd1_whole_vsize;
	u32 crop_left;
};

struct vd2_proc_s {
	u32 bypass_dv;
	u32 vd2_dout_dpsel;
	u32 din_hsize;
	u32 din_vsize;
	u32 dout_hsize;
	u32 dout_vsize;
	u32 vd2_dout_x_start;
	u32 vd2_dout_y_start;
	/* detunnel */
	u32 bypass_detunnel;
	/* hdr2 */
	u32 bypass_hdr;
	struct vd_proc_pps_s vd_proc_pps;
	struct vd_proc_pi_s vd_proc_pi;
};

struct vd_proc_s {
	/* input param */
	u32 vd1_used;
	u32 vd2_used;
	u32 bypass_detunnel;
	u32 bypass_hdr;
	u32 bypass_dv;
	u32 bypass_ve;
	/* vd1 */
	struct vd_proc_vd1_info_s vd_proc_vd1_info;
	/* vd2 */
	struct vd_proc_vd2_info_s vd_proc_vd2_info;
	/* preblend (sometimes input) */
	struct vd_proc_preblend_info_s vd_proc_preblend_info;
	/* mosaic (sometimes input) */
	struct vd_proc_mosaic_s vd_proc_mosaic;

	/* calculated param */
	/* slice calculated */
	struct vd_proc_slice_info_s vd_proc_slice_info;
	/* proc unit calculated */
	struct vd_proc_unit_s vd_proc_unit[SLICE_NUM];
	/* proc pi */
	struct vd_proc_pi_s vd_proc_pi;
	/* proc blend calculated */
	struct vd_proc_blend_s vd_proc_blend;

	struct vd_proc_blend_s vd_proc_preblend;
	/* vd2 proc */
	struct vd2_proc_s vd2_proc;
	struct video_layer_s *layer;
};

struct vd_proc_reg_s {
	struct vd_mif_reg_s vd_mif_reg[MAX_VD_LAYER_S5];
	struct vd_mif_linear_reg_s vd_mif_linear_reg[MAX_VD_LAYER_S5];
	struct vd_afbc_reg_s vd_afbc_reg[MAX_VD_LAYER_S5];
	struct vd_fg_reg_s vd_fg_reg[MAX_VD_LAYER_S5];
	struct vd_pps_reg_s vd_pps_reg[MAX_VD_LAYER_S5 + 1];
	struct vd_proc_slice_reg_s vd_proc_slice_reg[SLICE_NUM];
	struct vd_proc_sr_reg_s vd_proc_sr_reg;
	struct vd_proc_pi_reg_s vd_proc_pi_reg;
	struct vd_proc_blend_reg_s vd_proc_blend_reg;
	struct vd_proc_misc_reg_s vd_proc_misc_reg;
	struct vd1_slice_pad_reg_s vd1_slice_pad_size0_reg[SLICE_NUM];
	struct vd1_slice_pad_reg_s vd1_slice_pad_size1_reg[SLICE_NUM];
	struct vd_aisr_reshape_reg_s aisr_reshape_reg;
	struct vd2_pre_blend_reg_s vd2_pre_blend_reg;
	struct vd2_proc_misc_reg_s vd2_proc_misc_reg;
	struct vd_pip_alpha_reg_s vd_pip_alpha_reg[MAX_VD_CHAN_S5];
};

struct vd_pps_val_s {
	u32 vd_vsc_phase_ctrl_val;
	u32 vd_hsc_phase_ctrl_val;
	u32 vd_sc_misc_val;
	u32 vd_hsc_phase_ctrl1_val;
	u32 vd_prehsc_coef_val;
	u32 vd_pre_scale_ctrl_val;/* sc2 VPP_PREHSC_CTRL */
	u32 vd_prevsc_coef_val;
	u32 vd_prehsc_coef1_val;
};

struct mosaic_frame_s {
	u8 slice_id;
	u32 canvas_tbl[CANVAS_TABLE_CNT][3];
	u32 disp_canvas[CANVAS_TABLE_CNT];
	struct vframe_s *vf;
	struct disp_info_s virtual_layer_info;
	struct video_layer_s virtual_layer;
};

extern u32 debug_flag_s5;
extern struct mosaic_frame_s g_mosaic_frame[4];
struct vd_proc_s *get_vd_proc_info(void);
void dump_s5_vd_proc_regs(void);
void set_video_slice_policy(struct video_layer_s *layer,
	struct vframe_s *vf);

void set_vd_mif_linear_cs_s5(struct video_layer_s *layer,
				   struct canvas_s *cs0,
				   struct canvas_s *cs1,
				   struct canvas_s *cs2,
				   struct vframe_s *vf,
				   u32 lr_select);
void set_vd_mif_linear_s5(struct video_layer_s *layer,
				   struct canvas_config_s *config,
				   u32 planes,
				   struct vframe_s *vf,
				   u32 lr_select);
void disable_vd1_blend_s5(struct video_layer_s *layer);
void disable_vd2_blend_s5(struct video_layer_s *layer);
void aisr_reshape_addr_set_s5(struct video_layer_s *layer,
				  struct aisr_setting_s *aisr_mif_setting);
void aisr_reshape_output_s5(u32 enable);
void aisr_demo_enable_s5(void);
void aisr_demo_axis_set_s5(struct video_layer_s *layer);
void aisr_sr1_nn_enable_s5(u32 enable);
void aisr_sr1_nn_enable_sync_s5(u32 enable);
bool is_sr_phase_changed_s5(void);
void vd_s5_hw_set(struct video_layer_s *layer,
	struct vframe_s *dispbuf, struct vpp_frame_par_s *frame_par);
void canvas_update_for_mif_slice(struct video_layer_s *layer,
			     struct vframe_s *vf,
			     u32 slice);
void canvas_update_for_mif_mosaic(struct video_layer_s *layer,
			     struct vframe_s *vf,
			     u32 slice, u32 frame_id);
ssize_t video_vd_proc_state_dump(char *buf);
void set_module_bypass_s5(u32 bypass_module);
int get_module_bypass_s5(void);
u32 get_slice_num(u32 layer_id);
u32 get_pi_enabled(u32 layer_id);
int fgrain_init_s5(u8 layer_id, u32 table_size);
void fgrain_uninit_s5(u8 layer_id);
void fgrain_update_table_s5(struct video_layer_s *layer,
			 struct vframe_s *vf);
void vd_set_alpha_s5(struct video_layer_s *layer,
			     u32 win_en, struct pip_alpha_scpxn_s *alpha_win);
void check_afbc_status(void);
int vpp_crc_check_s5(u32 vpp_crc_en, u8 vpp_index);
void set_vdx_probe_ctrl_s5(u8 probe_id, u32 output);
void set_osdx_probe_ctrl_s5(u8 probe_id, u32 output);
void set_post_probe_ctrl_s5(u8 probe_id, u32 output);
u32 get_probe_pos_s5(u8 probe_id);
void set_probe_pos_s5(u32 val_x, u32 val_y, u8 probe_id, u32 output);
void get_probe_data_s5(u32 *val1, u32 *val2, u8 probe_id);
u32 get_cur_enc_num_s5(void);
u32 get_cur_enc_line_s5(void);
int set_vpu_super_urgent_s5(u32 module_id, u32 urgent_level);
int get_vpu_urgent_info_s5(void);
void set_vd_pi_input_size(void);
void adjust_video_slice_policy(u32 layer_id,
	struct vframe_s *vf, bool no_compress);
void vd_set_blk_mode_s5(struct video_layer_s *layer, u8 block_mode);
void vd_switch_frm_idx(u32 vpp_index, u32 frm_idx);
void enable_mosaic_mode(u32 vpp_index, u8 enable);
void dump_mosaic_pps(void);
void set_frm_idx(u32 vpp_index, u32 frm_idx);
void save_pps_data(int slice, u32 vd_vsc_phase_ctrl_val);
#endif
