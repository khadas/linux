/*
 * drivers/amlogic/media/video_sink/video_priv.h
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

#ifndef VIDEO_PRIV_HEADER_HH
#define VIDEO_PRIV_HEADER_HH

#include <linux/amlogic/media/video_sink/vpp.h>

#define VIDEO_ENABLE_STATE_IDLE       0
#define VIDEO_ENABLE_STATE_ON_REQ     1
#define VIDEO_ENABLE_STATE_ON_PENDING 2
#define VIDEO_ENABLE_STATE_OFF_REQ    3

#define DEBUG_FLAG_BLACKOUT     0x1
#define DEBUG_FLAG_PRINT_TOGGLE_FRAME 0x2
#define DEBUG_FLAG_PRINT_RDMA                0x4
#define DEBUG_FLAG_GET_COUNT                 0x8
#define DEBUG_FLAG_PRINT_DISBUF_PER_VSYNC        0x10
#define DEBUG_FLAG_PRINT_PATH_SWITCH        0x20
#define DEBUG_FLAG_LOG_RDMA_LINE_MAX         0x100
#define DEBUG_FLAG_TOGGLE_SKIP_KEEP_CURRENT  0x10000
#define DEBUG_FLAG_TOGGLE_FRAME_PER_VSYNC    0x20000
#define DEBUG_FLAG_RDMA_WAIT_1		     0x40000
#define DEBUG_FLAG_VSYNC_DONONE                0x80000
#define DEBUG_FLAG_GOFIELD_MANUL             0x100000
#define DEBUG_FLAG_LATENCY             0x200000
#define DEBUG_FLAG_PTS_TRACE            0x400000
#define DEBUG_FLAG_FRAME_DETECT            0x800000
#define DEBUG_FLAG_OMX_DEBUG_DROP_FRAME        0x1000000
#define DEBUG_FLAG_OMX_DISABLE_DROP_FRAME        0x2000000
#define DEBUG_FLAG_PRINT_DROP_FRAME        0x4000000
#define DEBUG_FLAG_OMX_DV_DROP_FRAME        0x8000000

#define VOUT_TYPE_TOP_FIELD 0
#define VOUT_TYPE_BOT_FIELD 1
#define VOUT_TYPE_PROG      2

#define VIDEO_DISABLE_NONE    0
#define VIDEO_DISABLE_NORMAL  1
#define VIDEO_DISABLE_FORNEXT 2

#define VIDEO_NOTIFY_TRICK_WAIT   0x01
#define VIDEO_NOTIFY_PROVIDER_GET 0x02
#define VIDEO_NOTIFY_PROVIDER_PUT 0x04
#define VIDEO_NOTIFY_FRAME_WAIT   0x08
#define VIDEO_NOTIFY_POS_CHANGED  0x10
#define VIDEO_NOTIFY_NEED_NO_COMP  0x20

#define MAX_VD_LAYER 2

#define DISPBUF_TO_PUT_MAX  8

#define COMPOSE_MODE_NONE			0
#define COMPOSE_MODE_3D			1
#define COMPOSE_MODE_DV			2
#define COMPOSE_MODE_BYPASS_CM	4

#define MAX_ZOOM_RATIO 300

#define VPP_PREBLEND_VD_V_END_LIMIT 2304

#define LAYER1_CANVAS_BASE_INDEX 0x58
#define LAYER2_CANVAS_BASE_INDEX 0x64

#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
#define CANVAS_TABLE_CNT 2
#else
#define CANVAS_TABLE_CNT 1
#endif

enum vd_path_id {
	VFM_PATH_DEF = -1,
	VFM_PATH_AMVIDEO = 0,
	VFM_PATH_PIP = 1,
	VFM_PATH_INVAILD = 0xff
};

enum toggle_out_fl_frame_e {
	OUT_FA_A_FRAME,
	OUT_FA_BANK_FRAME,
	OUT_FA_B_FRAME
};

struct video_dev_s {
	int vpp_off;
	int viu_off;
};

struct video_layer_s;

struct mif_pos_s {
	u32 id;
	u32 vd_reg_offt;
	u32 afbc_reg_offt;

	/* frame original size */
	u32 src_w;
	u32 src_h;

	/* mif start - end lines */
	u32 start_x_lines;
	u32 end_x_lines;
	u32 start_y_lines;
	u32 end_y_lines;

	/* left and right eye position, skip flag. */
	/* And if non 3d case, left eye = right eye */
	u32 l_hs_luma;
	u32 l_he_luma;
	u32 l_hs_chrm;
	u32 l_he_chrm;
	u32 r_hs_luma;
	u32 r_he_luma;
	u32 r_hs_chrm;
	u32 r_he_chrm;
	u32 h_skip;
	u32 hc_skip; /* afbc chroma skip */

	u32 l_vs_luma;
	u32 l_ve_luma;
	u32 l_vs_chrm;
	u32 l_ve_chrm;
	u32 r_vs_luma;
	u32 r_ve_luma;
	u32 r_vs_chrm;
	u32 r_ve_chrm;
	u32 v_skip;
	u32 vc_skip; /* afbc chroma skip */

	bool reverse;

	bool skip_afbc;
	u32 vpp_3d_mode;
};

struct scaler_setting_s {
	u32 id;
	u32 misc_reg_offt;
	bool support;

	bool sc_h_enable;
	bool sc_v_enable;
	bool sc_top_enable;

	bool last_line_fix;

	u32 vinfo_width;
	u32 vinfo_height;
	/* u32 VPP_pic_in_height_; */
	/* u32 VPP_line_in_length_; */

	struct vpp_frame_par_s *frame_par;
};

struct blend_setting_s {
	u32 id;
	u32 misc_reg_offt;

	u32 layer_alpha;

	u32 preblend_h_start;
	u32 preblend_h_end;
	u32 preblend_v_start;
	u32 preblend_v_end;

	u32 preblend_h_size;

	u32 postblend_h_start;
	u32 postblend_h_end;
	u32 postblend_v_start;
	u32 postblend_v_end;

	u32 postblend_h_size;

	struct vpp_frame_par_s *frame_par;
};

enum mode_3d_e {
	mode_3d_disable = 0,
	mode_3d_enable,
	mode_3d_mvc_enable
};

struct video_layer_s {
	u8 layer_id;

	/* reg map offsett*/
	u32 misc_reg_offt;
	u32 vd_reg_offt;
	u32 afbc_reg_offt;

	u8 cur_canvas_id;
#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
	u8 next_canvas_id;
#endif

	/* vframe buffer */
	struct vframe_s *dispbuf;
	struct vframe_s **dispbuf_mapping;

	u32 canvas_tbl[CANVAS_TABLE_CNT][6];
	u32 disp_canvas[CANVAS_TABLE_CNT][2];

	bool property_changed;
	u8 force_config_cnt;
	bool new_vpp_setting;
	bool new_frame;
	u32 vout_type;
	bool bypass_pps;

	struct vpp_frame_par_s *cur_frame_par;
	struct vpp_frame_par_s *next_frame_par;
	struct vpp_frame_par_s frame_parms[2];

	/* struct disp_info_s disp_info; */
	struct mif_pos_s mif_setting;
	struct scaler_setting_s sc_setting;
	struct blend_setting_s bld_setting;

	u32 new_vframe_count;

	u32 start_x_lines;
	u32 end_x_lines;
	u32 start_y_lines;
	u32 end_y_lines;

	u32 disable_video;
	u32 enabled;
	u32 enabled_status_saved;
	u32 onoff_state;
	u32 onoff_time;

	u32 layer_alpha;
	u32 global_output;

	u8 keep_frame_id;

	u8 enable_3d_mode;

	u32 global_debug;
};

/* from video_hw.c */
extern struct video_layer_s vd_layer[MAX_VD_LAYER];
extern struct disp_info_s glayer_info[MAX_VD_LAYER];
extern struct video_dev_s *cur_dev;
extern bool legacy_vpp;

bool is_dolby_vision_enable(void);
bool is_dolby_vision_on(void);
bool is_dolby_vision_stb_mode(void);
bool is_dovi_tv_on(void);
bool for_dolby_vision_certification(void);

struct video_dev_s *get_video_cur_dev(void);
u32 get_video_enabled(void);
u32 get_videopip_enabled(void);

bool is_di_on(void);
bool is_di_post_on(void);
bool is_di_post_link_on(void);
bool is_afbc_enabled(u8 layer_id);
bool is_local_vf(struct vframe_s *vf);

void safe_switch_videolayer(
	u8 layer_id, bool on, bool async);

#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
void correct_vd1_mif_size_for_DV(
	struct vpp_frame_par_s *par, struct vframe_s *el_vf);
void config_dvel_position(
	struct video_layer_s *layer,
	struct mif_pos_s *setting,
	struct vframe_s *el_vf);
s32 config_dvel_pps(
	struct video_layer_s *layer,
	struct scaler_setting_s *setting,
	const struct vinfo_s *info);
s32 config_dvel_blend(
	struct video_layer_s *layer,
	struct blend_setting_s *setting,
	struct vframe_s *dvel_vf);
#endif

#ifdef TV_3D_FUNCTION_OPEN
void config_3d_vd2_position(
	struct video_layer_s *layer,
	struct mif_pos_s *setting);
s32 config_3d_vd2_pps(
	struct video_layer_s *layer,
	struct scaler_setting_s *setting,
	const struct vinfo_s *info);
s32 config_3d_vd2_blend(
	struct video_layer_s *layer,
	struct blend_setting_s *setting);
void switch_3d_view_per_vsync(
	struct video_layer_s *layer);
#endif

s32 config_vd_position(
	struct video_layer_s *layer,
	struct mif_pos_s *setting);
s32 config_vd_pps(
	struct video_layer_s *layer,
	struct scaler_setting_s *setting,
	const struct vinfo_s *info);
s32 config_vd_blend(
	struct video_layer_s *layer,
	struct blend_setting_s *setting);
void vd_set_dcu(
	u8 layer_id,
	struct video_layer_s *layer,
	struct vpp_frame_par_s *frame_par,
	struct vframe_s *vf);
void vd_mif_setting(
	u8 layer_id,
	struct mif_pos_s *setting);
void vd_scaler_setting(
	u8 layer_id,
	struct scaler_setting_s *setting);
void vd_blend_setting(
	u8 layer_id,
	struct blend_setting_s *setting);
void proc_vd_vsc_phase_per_vsync(
	u8 layer_id,
	struct video_layer_s *layer,
	struct vpp_frame_par_s *frame_par,
	struct vframe_s *vf);

void vpp_blend_update(
	const struct vinfo_s *vinfo);

int get_layer_display_canvas(u8 layer_id);
int set_layer_display_canvas(
	u8 layer_id, struct vframe_s *vf,
	struct vpp_frame_par_s *cur_frame_par,
	struct disp_info_s *disp_info);
u32 *get_canvase_tbl(u8 layer_id);
s32 layer_swap_frame(
	struct vframe_s *vf, u8 layer_id,
	bool force_toggle,
	const struct vinfo_s *vinfo);

int detect_vout_type(
	const struct vinfo_s *vinfo);
int calc_hold_line(void);
u32 get_cur_enc_line(void);
void vpu_work_process(void);

int video_hw_init(void);
int video_early_init(void);
int video_late_uninit(void);

/* from video.c */
extern u32 osd_vpp_misc;
extern u32 osd_vpp_misc_mask;
extern bool update_osd_vpp_misc;
extern u32 framepacking_support;
extern unsigned int framepacking_blank;
extern unsigned int process_3d_type;
#ifdef TV_3D_FUNCTION_OPEN
extern unsigned int force_3d_scaler;
extern int toggle_3d_fa_frame;
#endif
extern bool reverse;
extern struct vframe_s vf_local;
extern struct vframe_s vf_local2;
extern struct vframe_s local_pip;
extern struct vframe_s *cur_dispbuf;
extern struct vframe_s *cur_pipbuf;
extern bool need_disable_vd2;
extern u32 last_el_status;

bool black_threshold_check(u8 id);
void update_cur_dispbuf(void *buf);
struct vframe_s *get_cur_dispbuf(void);

/*for video related files only.*/
void video_module_lock(void);
void video_module_unlock(void);
int get_video_debug_flags(void);
int _video_set_disable(u32 val);
int _videopip_set_disable(u32 val);
struct device *get_video_device(void);

#ifdef CONFIG_AMLOGIC_MEDIA_VIDEOCAPTURE
int ext_frame_capture_poll(int endflags);
#endif

#endif
/*VIDEO_PRIV_HEADER_HH*/
