/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * include/linux/amlogic/media/vfm/vframe.h
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

#ifndef VFRAME_H
#define VFRAME_H

#include <linux/types.h>
#ifdef CONFIG_AMLOGIC_MEDIA_TVIN
#include <linux/amlogic/media/frame_provider/tvin/tvin.h>
#endif
#include <linux/amlogic/media/canvas/canvas.h>
#include <linux/atomic.h>
#include <linux/amlogic/iomap.h>
#include <linux/amlogic/media/utils/amstream.h>

#define VIDTYPE_PROGRESSIVE             0x0
#define VIDTYPE_INTERLACE_TOP           0x1
#define VIDTYPE_INTERLACE_BOTTOM        0x3
#define VIDTYPE_TYPEMASK                0x7
#define VIDTYPE_INTERLACE               0x1
#define VIDTYPE_INTERLACE_FIRST         0x8
#define VIDTYPE_MVC                     0x10
#define VIDTYPE_NO_VIDEO_ENABLE         0x20
#define VIDTYPE_SEC_MD			0x40
#define VIDTYPE_VIU_NV12                0x80
#define VIDTYPE_VIU_422                 0x800
#define VIDTYPE_VIU_FIELD               0x1000
#define VIDTYPE_VIU_SINGLE_PLANE        0x2000
#define VIDTYPE_VIU_444                 0x4000
#define VIDTYPE_VIU_NV21                0x8000
#define VIDTYPE_VSCALE_DISABLE          0x10000
#define VIDTYPE_CANVAS_TOGGLE           0x20000
#define VIDTYPE_PRE_INTERLACE           0x40000
#define VIDTYPE_HIGHRUN                 0x80000
#define VIDTYPE_COMPRESS                0x100000
#define VIDTYPE_PIC		        0x200000
#define VIDTYPE_SCATTER                 0x400000
#define VIDTYPE_VD2			0x800000
#define VIDTYPE_COMPRESS_LOSS		0x1000000
#define VIDTYPE_COMB_MODE		0x2000000
#define VIDTYPE_NO_DW			0x4000000
#define VIDTYPE_SUPPORT_COMPRESS	0x8000000
#define VIDTYPE_PRE_DI_AFBC		0x10000000
#define VIDTYPE_RGB_444			0x20000000
#define VIDTYPE_DI_PW			0x40000000
#define VIDTYPE_V4L_EOS			0x80000000

#define DISP_RATIO_FORCECONFIG          0x80000000
#define DISP_RATIO_FORCE_NORMALWIDE     0x40000000
#define DISP_RATIO_FORCE_FULL_STRETCH   0x20000000
#define DISP_RATIO_ADAPTED_PICMODE   0x10000000
#define DISP_RATIO_INFOFRAME_AVAIL   0x08000000
#define DISP_RATIO_CTRL_MASK            0x00000003
#define DISP_RATIO_NO_KEEPRATIO         0x00000000
#define DISP_RATIO_KEEPRATIO            0x00000001
#define DISP_RATIO_PORTRAIT_MODE        0x00000004

#define DISP_RATIO_ASPECT_RATIO_MASK    0x0003ff00
#define DISP_RATIO_ASPECT_RATIO_BIT     8
#define DISP_RATIO_ASPECT_RATIO_MAX     0x3ff

#define TB_DETECT_MASK    0x00000040
#define TB_DETECT_MASK_BIT     6
#define TB_DETECT_NONE          0
#define TB_DETECT_INVERT       1
#define TB_DETECT_NC               0
#define TB_DETECT_TFF             1
#define TB_DETECT_BFF             2
#define TB_DETECT_TBF             3

#define VFRAME_FLAG_NO_DISCONTINUE		1
#define VFRAME_FLAG_SWITCHING_FENSE		2
#define VFRAME_FLAG_HIGH_BANDWIDTH		4
#define VFRAME_FLAG_ERROR_RECOVERY		8
#define VFRAME_FLAG_SYNCFRAME			0x10
#define VFRAME_FLAG_GAME_MODE			0x20
#define VFRAME_FLAG_VIDEO_COMPOSER		0x40
#define VFRAME_FLAG_VIDEO_COMPOSER_BYPASS	0x80
#define VFRAME_FLAG_COMPOSER_DONE		0x100
#define VFRAME_FLAG_VIDEO_COMPOSER_DMA		0x200
#define VFRAME_FLAG_VIDEO_LINEAR		0x400
#define VFRAME_FLAG_EMPTY_FRAME_V4L		0x800
#define VFRAME_FLAG_FAKE_FRAME			0x1000
#define VFRAME_FLAG_DOUBLE_FRAM		        0x2000
#define VFRAME_FLAG_VIDEO_DRM			0x4000
#define VFRAME_FLAG_VIDEO_VDETECT		0x8000
#define VFRAME_FLAG_VIDEO_VDETECT_PUT		0x10000
#define VFRAME_FLAG_VIDEO_SECURE		0x20000
#define VFRAME_FLAG_DI_P_ONLY			0x40000 /* ary */
#define VFRAME_FLAG_CONTAIN_POST_FRAME	0x80000
#define VFRAME_FLAG_DI_PW_VFM			0x100000
#define VFRAME_FLAG_DI_PW_N_LOCAL		0x200000
#define VFRAME_FLAG_DI_PW_N_EXT			0x400000
#define VFRAME_FLAG_HF				0x800000 /*HF*/
#define VFRAME_FLAG_KEEPED			0x1000000
#define VFRAME_FLAG_THROUGH_VDETECT		0x2000000
#define VFRAME_FLAG_DI_DW			0x4000000 /* di double write*/
#define VFRAME_FLAG_FIX_TUNNEL			0x8000000 /*source fixtunnel*/
#define VFRAME_FLAG_MIRROR_H                    0x10000000 /*mirror frame horizontally*/
#define VFRAME_FLAG_MIRROR_V                    0x20000000 /*mirror frame vertically*/
#define VFRAME_FLAG_PC_MODE			0x40000000

/* need check folllowing bits when toggle frame, to trigger property change */
/* add more bits which indicates display attr change in vf->flag */
#define VFRAME_FLAG_DISP_ATTR_MASK VFRAME_FLAG_VIDEO_LINEAR

enum pixel_aspect_ratio_e {
	PIXEL_ASPECT_RATIO_1_1,
	PIXEL_ASPECT_RATIO_8_9,
	PIXEL_ASPECT_RATIO_16_15,
};

/*
 * If pixel_sum[21:0] == 0, then all Histogram information are invalid
 */
struct vframe_hist_s {
	unsigned int hist_pow;
	unsigned int luma_sum;
	unsigned int chroma_sum;
	unsigned int pixel_sum;	/* [31:30] POW [21:0] PIXEL_SUM */
	unsigned int height;
	unsigned int width;
	unsigned char luma_max;
	unsigned char luma_min;
	unsigned short gamma[64];
	unsigned int vpp_luma_sum;	/*vpp hist info */
	unsigned int vpp_chroma_sum;
	unsigned int vpp_pixel_sum;
	unsigned int vpp_height;
	unsigned int vpp_width;
	unsigned char vpp_luma_max;
	unsigned char vpp_luma_min;
	unsigned short vpp_gamma[64];
	unsigned int vpp_hue_gamma[32];
	unsigned int vpp_sat_gamma[32];

#ifdef AML_LOCAL_DIMMING
	unsigned int ldim_max[100];
#endif
} /*vframe_hist_t */;

struct tvin_hdr10p_data_s {
	u32 vsif_hb;
	u32 vsif_ieee_code;
	struct pb4_st {
		u8 rvd:1;
		u8 max_lumin:5;
		u8 app_ver:2;
	} __packed pb4_st;
	u8 average_maxrgb;
	u8 distrib_valus0;
	u8 distrib_valus1;
	u8 distrib_valus2;
	u8 distrib_valus3;
	u8 distrib_valus4;
	u8 distrib_valus5;
	u8 distrib_valus6;
	u8 distrib_valus7;
	u8 distrib_valus8;
	struct pb15_18_st {
		u32 knee_point_x_9_6:4;
		u32 num_bezier_curve_anchors:4;
		u32 knee_point_y_9_8:2;
		u32 knee_point_x_5_0:6;
		u32 knee_point_y_7_0:8;
		u32 bezier_curve_anchors0:8;
	} __packed pb15_18_st;
	u8 bezier_curve_anchors1;
	u8 bezier_curve_anchors2;
	u8 bezier_curve_anchors3;
	u8 bezier_curve_anchors4;
	u8 bezier_curve_anchors5;
	u8 bezier_curve_anchors6;
	u8 bezier_curve_anchors7;
	u8 bezier_curve_anchors8;
	struct pb27_st {
		u8 rvd:6;
		u8 no_delay_flag:1;
		u8 overlay_flag:1;
	} __packed pb27_st;
} __packed;

/*vdin dolby vsi info param*/
struct tvin_dv_vsif_s {
	u8 dobly_vision_signal:1;
	u8 backlt_ctrl_MD_present:1;
	u8 auxiliary_MD_present:1;
	u8 eff_tmax_PQ_hi;
	u8 eff_tmax_PQ_low;
	u8 auxiliary_runmode;
	u8 auxiliary_runversion;
	u8 auxiliary_debug0;
};

/*
 * If bottom == 0 or right == 0, then all Blackbar information are invalid
 */
struct vframe_bbar_s {
	unsigned short top;
	unsigned short bottom;
	unsigned short left;
	unsigned short right;
} /*vfame_bbar_t */;

/*
 * If vsin == 0, then all Measurement information are invalid
 */
struct vframe_meas_s {
	/* float frin; frame Rate of Video Input in the unit of Hz */
	unsigned int vs_span_cnt;
	unsigned long long vs_cnt;
	unsigned int hs_cnt0;
	unsigned int hs_cnt1;
	unsigned int hs_cnt2;
	unsigned int hs_cnt3;
	unsigned int vs_cycle;
	unsigned int vs_stamp;
} /*vframe_meas_t */;

struct vframe_view_s {
	int start_x;
	int start_y;
	unsigned int width;
	unsigned int height;
} /*vframe_view_t */;

#define SEI_PicTiming         1
#define SEI_MasteringDisplayColorVolume 137
#define SEI_ContentLightLevel 144
struct vframe_content_light_level_s {
	u32 present_flag;
	u32 max_content;
	u32 max_pic_average;
}; /* content_light_level from SEI */

struct vframe_master_display_colour_s {
	u32 present_flag;
	u32 primaries[3][2];
	u32 white_point[2];
	u32 luminance[2];
	struct vframe_content_light_level_s content_light_level;
}; /* master_display_colour_info_volume from SEI */

/* vframe properties */
struct vframe_prop_s {
	struct vframe_hist_s hist;
	struct vframe_bbar_s bbar;
	struct vframe_meas_s meas;
	struct vframe_master_display_colour_s
	 master_display_colour;
	struct tvin_hdr10p_data_s hdr10p_data;
} /*vframe_prop_t */;

struct vdisplay_info_s {
	u32 frame_hd_start_lines_;
	u32 frame_hd_end_lines_;
	u32 frame_vd_start_lines_;
	u32 frame_vd_end_lines_;
	u32 display_vsc_startp;
	u32 display_vsc_endp;
	u32 display_hsc_startp;
	u32 display_hsc_endp;
	u32 screen_vd_v_start_;
	u32 screen_vd_v_end_;
	u32 screen_vd_h_start_;
	u32 screen_vd_h_end_;
};

enum vframe_source_type_e {
	VFRAME_SOURCE_TYPE_OTHERS = 0,
	VFRAME_SOURCE_TYPE_TUNER,
	VFRAME_SOURCE_TYPE_CVBS,
	VFRAME_SOURCE_TYPE_COMP,
	VFRAME_SOURCE_TYPE_HDMI,
	VFRAME_SOURCE_TYPE_PPMGR,
	VFRAME_SOURCE_TYPE_OSD,
	VFRAME_SOURCE_TYPE_HWC,
};

enum vframe_source_mode_e {
	VFRAME_SOURCE_MODE_OTHERS = 0,
	VFRAME_SOURCE_MODE_PAL,
	VFRAME_SOURCE_MODE_NTSC,
	VFRAME_SOURCE_MODE_SECAM,
};

enum vframe_secam_phase_e {
	VFRAME_PHASE_DB = 0,
	VFRAME_PHASE_DR,
};

enum vframe_disp_mode_e {
	VFRAME_DISP_MODE_NULL = 0,
	VFRAME_DISP_MODE_UNKNOWN,
	VFRAME_DISP_MODE_SKIP,
	VFRAME_DISP_MODE_OK,
};

enum vframe_signal_fmt_e {
	VFRAME_SIGNAL_FMT_INVALID = -1,
	VFRAME_SIGNAL_FMT_SDR = 0,
	VFRAME_SIGNAL_FMT_HDR10 = 1,
	VFRAME_SIGNAL_FMT_HDR10PLUS = 2,
	VFRAME_SIGNAL_FMT_HDR10PRIME = 3,
	VFRAME_SIGNAL_FMT_HLG = 4,
	VFRAME_SIGNAL_FMT_DOVI = 5,
	VFRAME_SIGNAL_FMT_DOVI_LL = 6,
	VFRAME_SIGNAL_FMT_MVC = 7
};

#define SEI_MAGIC_CODE 0x53656920 /* SEI */

/* signal format and sei data */
struct vframe_src_fmt_s {
	enum vframe_signal_fmt_e fmt;
	u32 sei_magic_code;
	void *sei_ptr;
	u32 sei_size;
	bool dual_layer;
	char *md_buf;
	char *comp_buf;
	int md_size;
	int comp_size;
	int parse_ret_flags;
	u32 play_id;
};

enum pic_mode_provider_e {
	PIC_MODE_PROVIDER_DB = 0,
	PIC_MODE_PROVIDER_WSS,
	PIC_MODE_UNKNOWN,
};

struct vframe_pic_mode_s {
	int hs;
	int he;
	int vs;
	int ve;
	u32 screen_mode;
	u32 custom_ar;
	u32 AFD_enable;
	enum pic_mode_provider_e provider;
};

#define BITDEPTH_Y_SHIFT 8
#define BITDEPTH_Y8    (0 << BITDEPTH_Y_SHIFT)
#define BITDEPTH_Y9    (BIT(BITDEPTH_Y_SHIFT))
#define BITDEPTH_Y10   (2 << BITDEPTH_Y_SHIFT)
#define BITDEPTH_Y12   (3 << BITDEPTH_Y_SHIFT)
#define BITDEPTH_YMASK (3 << BITDEPTH_Y_SHIFT)

#define BITDEPTH_U_SHIFT 10
#define BITDEPTH_U8    (0 << BITDEPTH_U_SHIFT)
#define BITDEPTH_U9    (BIT(BITDEPTH_U_SHIFT))
#define BITDEPTH_U10   (2 << BITDEPTH_U_SHIFT)
#define BITDEPTH_U12   (3 << BITDEPTH_U_SHIFT)
#define BITDEPTH_UMASK (3 << BITDEPTH_U_SHIFT)

#define BITDEPTH_V_SHIFT 12
#define BITDEPTH_V8    (0 << BITDEPTH_V_SHIFT)
#define BITDEPTH_V9    (BIT(BITDEPTH_V_SHIFT))
#define BITDEPTH_V10   (2 << BITDEPTH_V_SHIFT)
#define BITDEPTH_V12   (3 << BITDEPTH_V_SHIFT)
#define BITDEPTH_VMASK (3 << BITDEPTH_V_SHIFT)

#define BITDEPTH_MASK (BITDEPTH_YMASK | BITDEPTH_UMASK | BITDEPTH_VMASK)
#define BITDEPTH_SAVING_MODE	0x1
#define FULL_PACK_422_MODE		0x2

struct codec_mm_box_s {
	void    *mmu_box;
	int     mmu_idx;
	void    *bmmu_box;
	int     bmmu_idx;
};

struct vsif_info {
	void *addr;
	unsigned int size;
};

/* point to hdr rawdata of prop */
struct drm_info_t {
	void *addr;
	unsigned int size;
};

struct emp_info {
	void *addr;
	unsigned int size;
};

struct spd_data {
	void *addr;
	unsigned int size;
};

struct vtem_data {
	void *addr;
	unsigned int size;
};

#define MAX_COMPOSER_COUNT 9
#define AXIS_INFO_COUNT    4

struct componser_info_t {
	int count;
	int axis[MAX_COMPOSER_COUNT][AXIS_INFO_COUNT];
};

struct nn_value_t {
	int maxclass;
	int maxprob;
};

#define AI_PQ_TOP 5

struct dcntr_mem_s {
	u32 index;
	unsigned int cnt_in;
	unsigned long grd_addr;
	unsigned long yds_addr;
	unsigned long cds_addr;
	u32 grd_size;
	u32 yds_size;
	u32 cds_size;
	u32 ds_ratio;
	u32 pre_out_fmt;//VIDTYPE_VIU_NV12, VIDTYPE_VIU_444
	u32 yflt_wrmif_length;
	u32 cflt_wrmif_length;
	bool free;
	bool use_org;
	bool grd_swap_64bit;
	bool yds_swap_64bit;
	bool cds_swap_64bit;
	bool grd_little_endian;
	bool yds_little_endian;
	bool cds_little_endian;
	bool yds_canvas_mode;//0:linear address mode  1:canvas mode
	bool cds_canvas_mode;
	unsigned int ori_w; //add for copy from vfm
	unsigned int ori_h; //add for copy from vfm
};

struct hf_info_t {
	bool revert_mode;
	u32 index;
	ulong phy_addr;
	u32 width;
	u32 height;
	u32 buffer_w;
	u32 buffer_h;
};

enum nn_status_e {
	NN_INVALID = 0,
	NN_WAIT_DOING = 1,
	NN_START_DOING = 2,
	NN_DONE = 3,
	NN_DISPLAYED = 4
};

enum nn_mode_e {
	NN_MODE_2X2 = 1,
	NN_MODE_3X3 = 2,
	NN_MODE_4X4 = 3,
};

struct vf_nn_sr_t {
	int nn_out_fd;
	struct dma_buf *nn_out_dma_buf;
	u64 nn_out_phy_addr;
	struct file *nn_out_file;
	u32 nn_out_file_count;
	u32 nn_out_width;
	u32 nn_out_height;
	struct dma_fence *fence;
	int shared_fd;
	u32 hf_phy_addr;
	u32 hf_width;
	u32 hf_height;
	u32 hf_align_w;
	u32 hf_align_h;
	u32 buf_align_w;
	u32 buf_align_h;
	u32 nn_status;
	u32 nn_index;
	u32 nn_mode;
	struct timeval start_time;
};

struct vf_aipq_t {
	struct nn_value_t nn_value[AI_PQ_TOP];
	s32 aipq_buf_index;
	s32 aipq_value_index;
};

#define VC_FLAG_AI_SR	0x1
#define VC_FLAG_FIRST_FRAME	0x2

struct video_composer_private {
	u32 index;
	u32 flag; /*if  & VC_FLAG_AI_SR, and VPP will get AI_SR_out*/
	struct vf_nn_sr_t *srout_data;
	struct vframe_s *src_vf;
	u32 last_disp_count; /*last frame disp vsync count*/
	u32 vsync_index;
	/*used to control mbp buffer*/
	void (*lock_buffer_cb)(void *arg);
	void (*unlock_buffer_cb)(void *arg);
};

#define VF_UD_MAX_SIZE 5120 /* 5K size */
#define UD_MAGIC_CODE 0x55445020 /* UDP */
#define is_ud_param_valid(ud) ((ud.magic_code) == UD_MAGIC_CODE)

struct vf_ud_param_s {
	u32 magic_code;
	struct userdata_param_t ud_param;
};

struct vframe_s {
	u32 index;
	u32 index_disp;
	u32 omx_index;
	u32 type;
	u32 type_backup;
	u32 type_original;
	u32 blend_mode;
	u32 duration;
	u32 duration_pulldown;
	u32 pts;
	u64 pts_us64;
	bool next_vf_pts_valid;
	u32 next_vf_pts;
	u32 disp_pts;
	u64 disp_pts_us64;
	u64 timestamp;
	u32 flag;

	unsigned int fmeter0_hcnt[4];
	unsigned int fmeter1_hcnt[4];
	int fmeter0_score;
	int fmeter1_score;

	u32 canvas0Addr;
	u32 canvas1Addr;
	ulong compHeadAddr;
	ulong compBodyAddr;

	u32 plane_num;
	struct canvas_config_s canvas0_config[3];
	struct canvas_config_s canvas1_config[3];

	u32 bufWidth;
	u32 width;
	u32 height;
	u32 compWidth;
	u32 compHeight;
	u32 ratio_control;
	u32 bitdepth;

	/*
	 * bit 30: is_dv
	 * bit 29: present_flag
	 * bit 28-26: video_format
	 *	"component", "PAL", "NTSC", "SECAM", "MAC", "unspecified"
	 * bit 25: range "limited", "full_range"
	 * bit 24: color_description_present_flag
	 * bit 23-16: color_primaries
	 *	"unknown", "bt709", "undef", "bt601", "bt470m", "bt470bg",
	 *	"smpte170m", "smpte240m", "film", "bt2020"
	 * bit 15-8: transfer_characteristic
	 *	"unknown", "bt709", "undef", "bt601", "bt470m", "bt470bg",
	 *	"smpte170m", "smpte240m", "linear", "log100", "log316",
	 *	"iec61966-2-4", "bt1361e", "iec61966-2-1", "bt2020-10",
	 *	"bt2020-12", "smpte-st-2084", "smpte-st-428"
	 * bit 7-0: matrix_coefficient
	 *	"GBR", "bt709", "undef", "bt601", "fcc", "bt470bg",
	 *	"smpte170m", "smpte240m", "YCgCo", "bt2020nc", "bt2020c"
	 */
	u32 signal_type;
	u32 orientation;
	u32 video_angle;
	enum vframe_source_type_e source_type;
	enum vframe_secam_phase_e phase;
	enum vframe_source_mode_e source_mode;
#ifdef CONFIG_AMLOGIC_MEDIA_TVIN
	enum tvin_sig_fmt_e sig_fmt;
	enum tvin_trans_fmt trans_fmt;
	struct tvafe_vga_parm_s vga_parm;
#endif
	struct vframe_view_s left_eye;
	struct vframe_view_s right_eye;
	u32 mode_3d_enable;

	/* vframe extension */
	int (*early_process_fun)(void *arg, struct vframe_s *vf);
	int (*process_fun)(void *arg, unsigned int zoom_start_x_lines,
			   unsigned int zoom_end_x_lines,
			   unsigned int zoom_start_y_lines,
			   unsigned int zoom_end_y_lines, struct vframe_s *vf);
	struct hf_info_t *hf_info;	/* hg data*/
	void *private_data;
	struct video_composer_private *vc_private;
	/* vframe properties */
	struct vframe_prop_s prop;
	struct list_head list;
/* pixel aspect ratio */
	enum pixel_aspect_ratio_e pixel_ratio;
	u64 ready_jiffies64;	/* ready from decode on  jiffies_64 */
	long long ready_clock[5];/*ns*/
	long long ready_clock_hist[2];/*ns*/
	atomic_t use_cnt;

	atomic_t use_cnt_pip;
	/* atomic_t use_cnt_pip2; */
	u32 frame_dirty;
	/*
	 *prog_proc_config:
	 *1: process p from decoder as filed;
	 *0: process p from decoder as frame
	 */
	u32 prog_proc_config;
	/* used for indicate current video is motion or static */
	int combing_cur_lev;

	/*
	 *for vframe's memory,
	 * used by memory owner.
	 */
	void *mem_handle;
	/* in secure memory */
	int mem_sec;
	/*for MMU H265/VP9 compress header*/
	void *mem_head_handle;
	struct vframe_pic_mode_s pic_mode;

	unsigned long v4l_mem_handle;

	u32 sar_width;
	u32 sar_height;
	/*****************
	 * di pulldown info
	 * bit 3: interlace
	 * bit 2: flmxx
	 * bit 1: flm22
	 * bit 0: flm32
	 *****************/
	u32 di_pulldown;
	u32 di_gmv;

	u32 axis[4];
	u32 crop[4];

	struct vsif_info vsif;
	struct drm_info_t drm_if;
	struct emp_info emp;
	struct spd_data spd;
	struct vtem_data vtem;

	u32 zorder;
	u32 repeat_count[2];
	struct file *file_vf;
	bool rendered;

	struct codec_mm_box_s mm_box;

	/* signal format and sei data */
	struct vframe_src_fmt_s src_fmt;
	/*for di process NR and cts, storage dec vf*/
	void *vf_ext;
	u32 di_cm_cnt;
	u32 dwHeadAddr;
	u32 dwBodyAddr;
	bool fgs_valid;
	ulong fgs_table_adr;

	u32 di_instance_id;

	int sidebind_type;
	int sidebind_channel_id;

	/*for double write VP9/AV1 vf*/
	void *mem_dw_handle;
	struct nn_value_t nn_value[AI_PQ_TOP];
	struct dma_fence *fence;
		/*current is dv input*/
	bool dv_input;
	/* dv mode crc check:
	 * true: crc check ok
	 * false: crc check fail
	 */
	bool dv_crc_sts;

	/* currently only for keystone use */
	unsigned int crc;
	bool ai_pq_enable;
	struct componser_info_t *componser_info;
	void *decontour_pre;

	u32 hdr10p_data_size;
	char *hdr10p_data_buf;

	bool discard_dv_data;

	u32 frame_type;

	u32 meta_data_size;
	char *meta_data_buf;

	/* data address of userdata_param_t structure */
	struct vf_ud_param_s vf_ud_param;
} /*vframe_t */;

int get_curren_frame_para(int *top, int *left, int *bottom, int *right);

u8 is_vpp_postblend(void);

void pause_video(unsigned char pause_flag);
s32 update_vframe_src_fmt(struct vframe_s *vf,
			  void *sei,
			  u32 size,
			  bool dual_layer,
			  char *prov_name,
			  char *recv_name);
void *get_sei_from_src_fmt(struct vframe_s *vf, u32 *sei_size);
enum vframe_signal_fmt_e get_vframe_src_fmt(struct vframe_s *vf);
s32 clear_vframe_src_fmt(struct vframe_s *vf);
int get_md_from_src_fmt(struct vframe_s *vf);
#endif /* VFRAME_H */
