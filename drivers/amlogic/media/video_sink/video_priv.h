/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
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
#include "video_reg.h"

#define VIDEO_ENABLE_STATE_IDLE       0
#define VIDEO_ENABLE_STATE_ON_REQ     1
#define VIDEO_ENABLE_STATE_ON_PENDING 2
#define VIDEO_ENABLE_STATE_OFF_REQ    3

#define DEBUG_FLAG_BASIC_INFO     0x1
#define DEBUG_FLAG_PRINT_TOGGLE_FRAME 0x2
#define DEBUG_FLAG_PRINT_RDMA                0x4
#define DEBUG_FLAG_GET_COUNT                 0x8
#define DEBUG_FLAG_PRINT_DISBUF_PER_VSYNC        0x10
#define DEBUG_FLAG_PRINT_PATH_SWITCH        0x20
#define DEBUG_FLAG_TRACE_EVENT	        0x40
#define DEBUG_FLAG_LOG_RDMA_LINE_MAX         0x100
#define DEBUG_FLAG_BLACKOUT     0x200
#define DEBUG_FLAG_NO_CLIP_SETTING     0x400
#define DEBUG_FLAG_VPP_GET_BUFFER_TIME     0x800
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
#define DEBUG_FLAG_COMPOSER_NO_DROP_FRAME     0x10000000
#define DEBUG_FLAG_AXIS_NO_UPDATE     0x20000000
#define DEBUG_FLAG_HDMI_AVSYNC_DEBUG     0x40000000
#define DEBUG_FLAG_HDMI_DV_CRC     0x80000000

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
#define VIDEO_NOTIFY_NEED_NO_COMP  0x10

#define MAX_VD_LAYER 3
#define COMPOSE_MODE_NONE			0
#define COMPOSE_MODE_3D			1
#define COMPOSE_MODE_DV			2
#define COMPOSE_MODE_BYPASS_CM	4

#define VIDEO_PROP_CHANGE_NONE		0
#define VIDEO_PROP_CHANGE_SIZE		0x1
#define VIDEO_PROP_CHANGE_FMT		0x2
#define VIDEO_PROP_CHANGE_ENABLE	0x4
#define VIDEO_PROP_CHANGE_DISABLE	0x8
#define VIDEO_PROP_CHANGE_AXIS		0x10

#define MAX_ZOOM_RATIO 300

#define VPP_PREBLEND_VD_V_END_LIMIT 2304

#define LAYER1_CANVAS_BASE_INDEX 0x58
#define LAYER2_CANVAS_BASE_INDEX 0x64
#define LAYER3_CANVAS_BASE_INDEX 0xd8

#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
#define CANVAS_TABLE_CNT 2
#else
#define CANVAS_TABLE_CNT 1
#endif

#define VIDEO_AUTO_POST_BLEND_DUMMY BIT(24)

#define DISPBUF_TO_PUT_MAX 3

#define IS_DI_PROCESSED(vftype) ((vftype) & (VIDTYPE_PRE_INTERLACE | VIDTYPE_DI_PW))
#define IS_DI_POST(vftype) \
	(((vftype) & (VIDTYPE_PRE_INTERLACE | VIDTYPE_DI_PW)) \
	 == VIDTYPE_PRE_INTERLACE)
#define IS_DI_POSTWRTIE(vftype) ((vftype) & VIDTYPE_DI_PW)
#define MAX_PIP_WINDOW    16
#define VPP_FILER_COEFS_NUM   33
#define VPP_NUM 3
#define RDMA_INTERFACE_NUM 4

#define OP_VPP_MORE_LOG 1
#define OP_FORCE_SWITCH_VF 2
#define OP_FORCE_NOT_SWITCH_VF 4

enum vd_path_id {
	VFM_PATH_DEF = -1,
	VFM_PATH_AMVIDEO = 0,
	VFM_PATH_PIP = 1,
	VFM_PATH_VIDEO_RENDER0 = 2,
	VFM_PATH_VIDEO_RENDER1 = 3,
	VFM_PATH_PIP2 = 4,
	VFM_PATH_VIDEO_RENDER2 = 5,
	VFM_PATH_VIDEO_RENDER3 = 6,
	VFM_PATH_VIDEO_RENDER4 = 7,
	VFM_PATH_VIDEO_RENDER5 = 8,
	VFM_PATH_VIDEO_RENDER6 = 9,
	VFM_PATH_AUTO = 0xfe,
	VFM_PATH_INVALID = 0xff
};

enum toggle_out_fl_frame_e {
	OUT_FA_A_FRAME,
	OUT_FA_BANK_FRAME,
	OUT_FA_B_FRAME
};

enum pre_hscaler_e {
	PRE_HSCALER_2TAP = 2,
	PRE_HSCALER_4TAP = 4,
	PRE_HSCALER_6TAP = 6,
	PRE_HSCALER_8TAP = 8
};

enum pre_vscaler_e {
	PRE_VSCALER_2TAP = 2,
	PRE_VSCALER_4TAP = 4,
	PRE_VSCALER_6TAP = 6,
	PRE_VSCALER_8TAP = 8
};

enum vpp_type_e {
	VPP0,
	VPP1,
	VPP2,
};

enum reshape_mode_e {
	MODE_2X2 = 1,
	MODE_3X3 = 2,
	MODE_4X4 = 3,
};

enum VPU_MODULE_e {
	FRC0_R,
	FRC0_W,
	FRC1_R,
	FRC1_W,
	FRC2_R,
	VPU0_R,
	VPU0_W,
	VPU1_R,
	VPU1_W,
	VPU2_R,
};

typedef u32 (*rdma_rd_op)(u32 reg);
typedef int (*rdma_wr_op)(u32 reg, u32 val);
typedef int (*rdma_wr_bits_op)(u32 reg, u32 val, u32 start, u32 len);

struct rdma_fun_s {
	rdma_rd_op rdma_rd;
	rdma_wr_op rdma_wr;
	rdma_wr_bits_op rdma_wr_bits;
};

struct video_dev_s {
	int vpp_off;
	int viu_off;
	int mif_linear;
	int t7_display;
	int max_vd_layers;
	int vd2_independ_blend_ctrl;
	int aisr_support;
	int aisr_enable;
	int scaler_sep_coef_en;
	int pps_auto_calc;
	bool di_hf_y_reverse;
	bool aisr_demo_en;
	u32 aisr_demo_xstart;
	u32 aisr_demo_ystart;
	u32 aisr_demo_xend;
	u32 aisr_demo_yend;
	bool power_ctrl;
	struct hw_pps_reg_s aisr_pps_reg;
	struct vpp_frame_par_s aisr_frame_parms;
	struct rdma_fun_s rdma_func[RDMA_INTERFACE_NUM];
	u32 sr_in_size;
};

struct video_layer_s;

struct mif_pos_s {
	u32 id;
	struct hw_vd_reg_s *p_vd_mif_reg;
	struct hw_afbc_reg_s *p_vd_afbc_reg;

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
	u8 block_mode;
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

struct clip_setting_s {
	u32 id;
	u32 misc_reg_offt;

	u32 clip_max;
	u32 clip_min;

	bool clip_done;
};

struct pip_alpha_scpxn_s {
	u32 scpxn_bgn_h[MAX_PIP_WINDOW];
	u32 scpxn_end_h[MAX_PIP_WINDOW];
	u32 scpxn_bgn_v[MAX_PIP_WINDOW];
	u32 scpxn_end_v[MAX_PIP_WINDOW];
};

struct fgrain_setting_s {
	u32 id;
	u32 start_x;
	u32 end_x;
	u32 start_y;
	u32 end_y;
	u32 fmt_mode; /* only support 420 */
	u32 bitdepth; /* 8 bit or 10 bit */
	u32 reverse;
	u32 afbc; /* afbc or not */
	u32 last_in_mode; /* related with afbc */
	u32 used;
	/* lut dma */
	u32 fgs_table_adr;
	u32 table_size;
};

struct aisr_setting_s {
	u32 aisr_enable;
	u32 in_ratio; /* 1:2x2  2:3x3 3:4x4 */
	u32 src_w;
	u32 src_h;
	u32 src_align_w;
	u32 src_align_h;
	u32 buf_align_w;
	u32 buf_align_h;
	u32 x_start;
	u32 x_end;
	u32 y_start;
	u32 y_end;
	u32 little_endian;
	u32 swap_64bit;
	u32 di_hf_y_reverse;
	u32 vscale_skip_count;
	ulong phy_addr;
};

enum mode_3d_e {
	mode_3d_disable = 0,
	mode_3d_enable,
	mode_3d_mvc_enable
};

struct video_layer_s {
	u8 layer_id;
	u8 layer_support;
	/* reg map offsett*/
	u32 misc_reg_offt;
	struct hw_vd_reg_s vd_mif_reg;
	struct hw_vd_linear_reg_s vd_mif_linear_reg;
	struct hw_afbc_reg_s vd_afbc_reg;
	struct hw_fg_reg_s fg_reg;
	struct hw_pps_reg_s pps_reg;
	struct hw_vpp_blend_reg_s vpp_blend_reg;
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
	bool switch_vf;
	u8 force_switch_mode;
	struct vframe_s *vf_ext;

	struct vpp_frame_par_s *cur_frame_par;
	struct vpp_frame_par_s *next_frame_par;
	struct vpp_frame_par_s frame_parms[2];

	/* struct disp_info_s disp_info; */
	struct mif_pos_s mif_setting;
	struct scaler_setting_s sc_setting;
	struct blend_setting_s bld_setting;
	struct fgrain_setting_s fgrain_setting;
	struct clip_setting_s clip_setting;
	struct aisr_setting_s aisr_mif_setting;
	struct scaler_setting_s aisr_sc_setting;

	u32 new_vframe_count;

	u32 start_x_lines;
	u32 end_x_lines;
	u32 start_y_lines;
	u32 end_y_lines;

	u32 disable_video;
	u32 enabled;
	u32 pre_blend_en;
	u32 post_blend_en;
	u32 enabled_status_saved;
	u32 onoff_state;
	u32 onoff_time;

	u32 layer_alpha;
	u32 global_output;

	u8 keep_frame_id;

	u8 enable_3d_mode;

	u32 global_debug;

	bool need_switch_vf;
	bool do_switch;
	bool force_black;
	bool force_disable;
	u8 vpp_index;
	u8 vppx_blend_en;
	bool vd1_vd2_mux;
	u32 video_en_bg_color;
	u32 video_dis_bg_color;
	u32 dummy_alpha;
	u32 compWidth;
	u32 compHeight;
	u32 src_width;
	u32 src_height;
	bool alpha_win_en;
	struct pip_alpha_scpxn_s alpha_win;
};

enum {
	ONLY_CORE0,
	ONLY_CORE1,
	NEW_CORE0_CORE1,
	OLD_CORE0_CORE1,
};
enum cpu_type_e {
	MESON_CPU_MAJOR_ID_COMPATIBALE = 0x1,
	MESON_CPU_MAJOR_ID_TM2_REVB,
	MESON_CPU_MAJOR_ID_SC2_,
	MESON_CPU_MAJOR_ID_T5_,
	MESON_CPU_MAJOR_ID_T5D_,
	MESON_CPU_MAJOR_ID_T7_,
	MESON_CPU_MAJOR_ID_S4_,
	MESON_CPU_MAJOR_ID_T5D_REVB_,
	MESON_CPU_MAJOR_ID_T3_,
	MESON_CPU_MAJOR_ID_T5W_,
	MESON_CPU_MAJOR_ID_UNKNOWN_,
};

struct video_device_hw_s {
	u32 vd2_independ_blend_ctrl;
	u32 aisr_support;
	u32 frc_support;
	u32 di_hf_y_reverse;
	u32 sr_in_size;
};

struct amvideo_device_data_s {
	enum cpu_type_e cpu_type;
	u32 sr_reg_offt;
	u32 sr_reg_offt2;
	u8 layer_support[MAX_VD_LAYER];
	u8 afbc_support[MAX_VD_LAYER];
	u8 pps_support[MAX_VD_LAYER];
	u8 alpha_support[MAX_VD_LAYER];
	u8 dv_support;
	u8 sr0_support;
	u8 sr1_support;
	u32 core_v_disable_width_max[MAX_SR_NUM];
	u32 core_v_enable_width_max[MAX_SR_NUM];
	u8 supscl_path;
	u8 fgrain_support[MAX_VD_LAYER];
	u8 has_hscaler_8tap[MAX_VD_LAYER];
	/* 1: 4tap; 2: 8tap */
	u8 has_pre_hscaler_ntap[MAX_VD_LAYER];
	u8 has_pre_vscaler_ntap[MAX_VD_LAYER];
	u32 src_width_max[MAX_VD_LAYER];
	u32 src_height_max[MAX_VD_LAYER];
	u32 ofifo_size;
	u32 afbc_conv_lbuf_len[MAX_VD_LAYER];
	u8 mif_linear;
	u8 t7_display;
	u8 max_vd_layers;
	u8 has_vpp1;
	u8 has_vpp2;
	struct video_device_hw_s dev_property;
};

struct path_id_s {
	s32 vd1_path_id;
	s32 vd2_path_id;
	s32 vd3_path_id;
};

struct pre_scaler_info {
	u32 force_pre_scaler;
	u32 pre_hscaler_ntap_enable;
	u32 pre_hscaler_ntap_set;
	u32 pre_hscaler_ntap;
	u32 pre_hscaler_rate;
	u32 pre_hscaler_coef[4];
	u32 pre_hscaler_coef_set;
	u32 pre_vscaler_ntap_enable;
	u32 pre_vscaler_ntap_set;
	u32 pre_vscaler_ntap;
	u32 pre_vscaler_rate;
	u32 pre_vscaler_coef[4];
	u32 pre_vscaler_coef_set;
};
/* from video_hw.c */
extern struct video_layer_s vd_layer[MAX_VD_LAYER];
extern struct disp_info_s glayer_info[MAX_VD_LAYER];
extern struct video_dev_s *cur_dev;
extern bool legacy_vpp;
extern bool hscaler_8tap_enable[MAX_VD_LAYER];
extern struct pre_scaler_info pre_scaler[MAX_VD_LAYER];
extern bool vd1_vd2_mux;
extern bool aisr_en;
extern u32 vd1_vd2_mux_dts;
extern u32 osd_vpp1_bld_ctrl;
extern u32 osd_vpp2_bld_ctrl;
extern bool update_osd_vpp1_bld_ctrl;
extern bool update_osd_vpp2_bld_ctrl;
extern int vdec_out_size_threshold_8k;
extern int vpp_in_size_threshold_8k;
extern int vdec_out_size_threshold_4k;
extern int vpp_in_size_threshold_4k;

bool is_dolby_vision_enable(void);
bool is_dolby_vision_on(void);
bool is_dolby_vision_stb_mode(void);
bool is_dovi_tv_on(void);
bool for_dolby_vision_certification(void);

struct video_dev_s *get_video_cur_dev(void);
u32 get_video_enabled(void);
u32 get_videopip_enabled(void);
u32 get_videopip2_enabled(void);
u32 get_video_onoff_state(void);
u32 get_videopip_onoff_state(void);
u32 get_videopip2_onoff_state(void);

bool is_di_on(void);
bool is_di_post_on(void);
bool is_di_post_link_on(void);
bool is_di_post_mode(struct vframe_s *vf);
bool is_afbc_enabled(u8 layer_id);
bool is_local_vf(struct vframe_s *vf);
bool is_picmode_changed(u8 layer_id, struct vframe_s *vf);

void safe_switch_videolayer(u8 layer_id,
			    bool on, bool async);

#ifndef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
bool is_dolby_vision_enable(void);
bool is_dolby_vision_on(void);
bool is_dolby_vision_stb_mode(void);
bool for_dolby_vision_certification(void);
bool is_dovi_frame(struct vframe_s *vf);
void dolby_vision_set_toggle_flag(int flag);
#endif

#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
void correct_vd1_mif_size_for_DV(struct vpp_frame_par_s *par,
				 struct vframe_s *el_vf);
void config_dvel_position(struct video_layer_s *layer,
			  struct mif_pos_s *setting,
			  struct vframe_s *el_vf);
s32 config_dvel_pps(struct video_layer_s *layer,
		    struct scaler_setting_s *setting,
		    const struct vinfo_s *info);
s32 config_dvel_blend(struct video_layer_s *layer,
		      struct blend_setting_s *setting,
		      struct vframe_s *dvel_vf);
#endif

#ifdef TV_3D_FUNCTION_OPEN
void config_3d_vd2_position(struct video_layer_s *layer,
			    struct mif_pos_s *setting);
s32 config_3d_vd2_pps(struct video_layer_s *layer,
		      struct scaler_setting_s *setting,
		      const struct vinfo_s *info);
s32 config_3d_vd2_blend(struct video_layer_s *layer,
			struct blend_setting_s *setting);
void switch_3d_view_per_vsync(struct video_layer_s *layer);
#endif

s32 config_vd_position(struct video_layer_s *layer,
		       struct mif_pos_s *setting);
s32 config_vd_pps(struct video_layer_s *layer,
		  struct scaler_setting_s *setting,
		  const struct vinfo_s *info);
s32 config_vd_blend(struct video_layer_s *layer,
		    struct blend_setting_s *setting);
void vd_set_dcu(u8 layer_id,
		struct video_layer_s *layer,
		struct vpp_frame_par_s *frame_par,
		struct vframe_s *vf);
void vd_mif_setting(struct video_layer_s *layer,
		    struct mif_pos_s *setting);
void vd_scaler_setting(struct video_layer_s *layer,
		       struct scaler_setting_s *setting);
void vd_blend_setting(struct video_layer_s *layer,
		      struct blend_setting_s *setting);
void vd_clip_setting(u8 layer_id,
		     struct clip_setting_s *setting);
void proc_vd_vsc_phase_per_vsync(struct video_layer_s *layer,
				 struct vpp_frame_par_s *frame_par,
				 struct vframe_s *vf);

void vpp_blend_update(const struct vinfo_s *vinfo);
void vpp_blend_update_t7(const struct vinfo_s *vinfo);
void vppx_vd_blend_setting(struct video_layer_s *layer, struct blend_setting_s *setting);
void vppx_blend_update(const struct vinfo_s *vinfo, u32 vpp_index);
int get_layer_display_canvas(u8 layer_id);
int set_layer_display_canvas(struct video_layer_s *layer,
			     struct vframe_s *vf,
			     struct vpp_frame_par_s *cur_frame_par,
			     struct disp_info_s *disp_info);
u32 *get_canvase_tbl(u8 layer_id);
s32 layer_swap_frame(struct vframe_s *vf, struct video_layer_s *layer,
		     bool force_toggle,
		     const struct vinfo_s *vinfo);
int detect_vout_type(const struct vinfo_s *vinfo);
int calc_hold_line(void);
u32 get_active_start_line(void);
u32 get_cur_enc_line(void);
void vpu_work_process(void);
int vpp_crc_check(u32 vpp_crc_en, u8 vpp_index);
void enable_vpp_crc_viu2(u32 vpp_crc_en);
int vpp_crc_viu2_check(u32 vpp_crc_en);
void dump_pps_coefs_info(u8 layer_id, u8 bit9_mode, u8 coef_type);
struct video_layer_s *get_layer_by_layer_id(u8 layer_id);
void update_vd_src_info(u8 layer_id,
							u32 src_width, u32 src_height,
							u32 compWidth, u32 compHeight);
bool is_bandwidth_policy_hit(u8 layer_id);
int video_hw_init(void);
int video_early_init(struct amvideo_device_data_s *p_amvideo);
int video_late_uninit(void);

/* from video.c */
extern u32 osd_vpp_misc;
extern u32 osd_vpp_misc_mask;
extern bool update_osd_vpp_misc;
extern u32 osd_preblend_en;
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
extern struct vframe_s local_pip2;
extern struct vframe_s *cur_dispbuf;
extern struct vframe_s *cur_pipbuf;
extern struct vframe_s *cur_pipbuf2;
extern bool need_disable_vd2;
extern bool need_disable_vd3;
extern u32 last_el_status;
extern u32 video_prop_status;
extern u32 force_blackout;
extern atomic_t video_unreg_flag;
extern atomic_t video_unreg_flag_vpp[2];
extern atomic_t video_inirq_flag;
extern atomic_t video_inirq_flag_vpp[2];
extern uint load_pps_coef;
extern atomic_t video_recv_cnt;
extern struct video_recv_s *gvideo_recv[3];
extern struct video_recv_s *gvideo_recv_vpp[2];
extern uint load_pps_coef;
extern atomic_t gafbc_request;
extern atomic_t video_unreg_flag;
extern atomic_t video_pause_flag;
extern bool video_suspend;
extern u32 video_suspend_cycle;
extern int log_out;
extern int debug_flag;
extern bool bypass_pps;
extern bool rdma_enable_pre;
extern struct vpp_frame_par_s *curpip_frame_par;
extern struct vpp_frame_par_s *curpip2_frame_par;
extern struct video_layer_s vd_layer_vpp[2];
extern u32 force_switch_vf_mode;
extern u32 video_info_change_status;

bool black_threshold_check(u8 id);
extern atomic_t primary_src_fmt;
extern atomic_t cur_primary_src_fmt;

struct vframe_s *get_cur_dispbuf(void);
s32 set_video_path_select(const char *recv_name, u8 layer_id);
s32 set_sideband_type(s32 type, u8 layer_id);

/*for video related files only.*/
void video_module_lock(void);
void video_module_unlock(void);
int get_video_debug_flags(void);
int _video_set_disable(u32 val);
int _videopip_set_disable(u32 index, u32 val);
struct device *get_video_device(void);
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
struct vframe_s *dvel_toggle_frame(struct vframe_s *vf,
				   bool new_frame);
#endif

#ifdef CONFIG_AMLOGIC_MEDIA_VIDEOCAPTURE
int ext_frame_capture_poll(int endflags);
#endif
bool is_meson_tm2_revb(void);
bool video_is_meson_sc2_cpu(void);
bool video_is_meson_t5d_cpu(void);
bool video_is_meson_t7_cpu(void);
bool video_is_meson_s4_cpu(void);
bool video_is_meson_t5d_revb_cpu(void);
bool video_is_meson_t3_cpu(void);
bool video_is_meson_t5w_cpu(void);
void alpha_win_set(struct video_layer_s *layer);
void fgrain_config(struct video_layer_s *layer,
		   struct vpp_frame_par_s *frame_par,
		   struct mif_pos_s *mif_setting,
		   struct fgrain_setting_s *setting,
		   struct vframe_s *vf);
void fgrain_setting(struct video_layer_s *layer,
		    struct fgrain_setting_s *setting,
		    struct vframe_s *vf);
void fgrain_update_table(struct video_layer_s *layer,
			 struct vframe_s *vf);
void video_secure_set(u8 vpp_index);
bool has_hscaler_8tap(u8 layer_id);
bool has_pre_hscaler_ntap(u8 layer_id);
bool has_pre_hscaler_8tap(u8 layer_id);
bool has_pre_vscaler_ntap(u8 layer_id);
void _set_video_window(struct disp_info_s *layer, int *p);
void _set_video_crop(struct disp_info_s *layer, int *p);
void set_alpha_scpxn(struct video_layer_s *layer,
			   struct componser_info_t *componser_info);
void di_used_vd1_afbc(bool di_used);
void pip_swap_frame(struct video_layer_s *layer, struct vframe_s *vf,
		    const struct vinfo_s *vinfo);
s32 pip_render_frame(struct video_layer_s *layer, const struct vinfo_s *vinfo);
void pip2_swap_frame(struct video_layer_s *layer, struct vframe_s *vf,
		     const struct vinfo_s *vinfo);
s32 pip2_render_frame(struct video_layer_s *layer, const struct vinfo_s *vinfo);
bool aisr_update_frame_info(struct video_layer_s *layer,
			 struct vframe_s *vf);
void aisr_reshape_addr_set(struct video_layer_s *layer,
				  struct aisr_setting_s *aisr_mif_setting);
void aisr_reshape_cfg(struct video_layer_s *layer,
		      struct aisr_setting_s *aisr_mif_setting);
void aisr_scaler_setting(struct video_layer_s *layer,
				    struct scaler_setting_s *setting);
s32 config_aisr_pps(struct video_layer_s *layer,
			 struct scaler_setting_s *aisr_setting);
s32 config_aisr_position(struct video_layer_s *layer,
			     struct aisr_setting_s *aisr_mif_setting);
void aisr_demo_enable(void);
void aisr_demo_axis_set(void);
void aisr_reshape_output(u32 enable);
void pre_process_for_3d(struct vframe_s *vf);
bool tvin_vf_disp_mode_check(struct vframe_s *vf);
int get_vpu_urgent_info(void);
int set_vpu_super_urgent(u32 module_id, u32 low_level, u32 high_level);
#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
void vsync_rdma_process(void);
#endif

#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_VECM
void amvecm_process(struct path_id_s *path_id, struct video_recv_s *p_gvideo_recv,
			    struct vframe_s *new_frame);
#endif

#ifndef CONFIG_AMLOGIC_MEDIA_FRAME_SYNC
enum avevent_e {
	VIDEO_START,
	VIDEO_PAUSE,
	VIDEO_STOP,
	VIDEO_TSTAMP_DISCONTINUITY,
	AUDIO_START,
	AUDIO_PAUSE,
	AUDIO_RESUME,
	AUDIO_STOP,
	AUDIO_TSTAMP_DISCONTINUITY,
	AUDIO_PRE_START,
	AUDIO_WAIT
};

enum tsync_mode_e {
	TSYNC_MODE_VMASTER,
	TSYNC_MODE_AMASTER,
	TSYNC_MODE_PCRMASTER,
};

enum {
	PTS_TYPE_VIDEO = 0,
	PTS_TYPE_AUDIO = 1,
	PTS_TYPE_HEVC = 2,
	PTS_TYPE_MAX = 3
};

static inline void tsync_avevent_locked(enum avevent_e event, u32 param) {}
static inline void tsync_avevent(enum avevent_e event, u32 param) {}
static inline int tsync_get_mode(void) { return 0; }
static inline void tsync_set_enable(int enable) {}
static inline void tsync_set_syncthresh(unsigned int sync_thresh) {}

static inline int tsync_get_sync_adiscont(void) { return 0; }
static inline int tsync_get_sync_vdiscont(void) { return 0; }
static inline void tsync_set_sync_adiscont(int syncdiscont) {}
static inline void tsync_set_sync_vdiscont(int syncdiscont) {}
static inline u32 tsync_get_sync_adiscont_diff(void) { return 0; }
static inline u32 tsync_get_sync_vdiscont_diff(void) { return 0; }
static inline void tsync_set_sync_adiscont_diff(u32 discontinue_diff) {}
static inline void tsync_set_sync_vdiscont_diff(u32 discontinue_diff) {}
static inline void tsync_trick_mode(int trick_mode) {}
static inline int tsync_set_tunnel_mode(int mode) { return 0; }
static inline void tsync_set_avthresh(unsigned int av_thresh) {}
static inline u32 tsync_vpts_discontinuity_margin(void)
{
	return 0;
}

static inline u32 timestamp_vpts_get(void) { return 0; }
static inline void timestamp_vpts_set(u32 pts) {}
static inline void timestamp_vpts_inc(s32 val)  {}
static inline u32 timestamp_apts_get(void)  { return 0; }
static inline void timestamp_apts_set(u32 pts) {}
static inline void timestamp_apts_inc(s32 val) {}
static inline u32 timestamp_pcrscr_get(void) { return 0; }
static inline void timestamp_pcrscr_set(u32 pts) {}
static inline void timestamp_pcrscr_inc(s32 val) {}
static inline void timestamp_pcrscr_inc_scale(s32 inc, u32 base) {}
static inline void timestamp_pcrscr_enable(u32 enable) {}
static inline u32 timestamp_pcrscr_enable_state(void) { return 0; }

static inline int calculation_stream_delayed_ms(u8 type, u32 *latestbirate,
						u32 *avg_bitare)
{return 0; }

static inline bool tsync_check_vpts_discontinuity(unsigned int vpts)
{
	return false;
}
#endif

#ifndef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_VECM
enum vd_path_e {
	VD1_PATH = 0,
	VD2_PATH = 1,
	VD3_PATH = 2,
	VD_PATH_MAX = 3
};
#endif
#endif
/*VIDEO_PRIV_HEADER_HH*/
