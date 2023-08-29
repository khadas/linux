/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef _AMDV_H_
#define _AMDV_H_

/*#define V2_4_3*/

/*  driver version */
#define DRIVER_VER "2023000512"

#include <linux/types.h>
#include "amdv_pq_config.h"

#define DOLBY_VISION_LL_DISABLE		0
#define DOLBY_VISION_LL_YUV422		1
#define DOLBY_VISION_LL_RGB444		2

#define BYPASS_PROCESS	0
#define SDR_PROCESS	1
#define HDR_PROCESS	2
#define DV_PROCESS	3
#define HLG_PROCESS	4

#define DV_CORE1_RECONFIG_CNT 2
#define DV_CORE2_RECONFIG_CNT 120

#define EXT_MD_LEVEL_1      BIT(0)
#define EXT_MD_LEVEL_2      BIT(1)
#define EXT_MD_LEVEL_4      BIT(2)
#define EXT_MD_LEVEL_5      BIT(3)
#define EXT_MD_LEVEL_6      BIT(4)
#define EXT_MD_LEVEL_255    BIT(31)

#define TUNING_MODE_FORCE_ABSOLUTE         0x1
#define TUNING_MODE_EXTLEVEL_1_DISABLE     0x2
#define TUNING_MODE_EXTLEVEL_2_DISABLE     0x4
#define TUNING_MODE_EXTLEVEL_4_DISABLE     0x8
#define TUNING_MODE_EXTLEVEL_5_DISABLE     0x10
#define TUNING_MODE_EL_FORCE_DISABLE      0x20

#define VPP_VD1_DSC_CTRL                   0x1a83
#define VIU_VD1_PATH_CTRL                  0x1a73
#define VPP_VD2_DSC_CTRL                   0x1a84
#define VPP_VD3_DSC_CTRL                   0x1a85
#define AMDV_PATH_SWAP_CTRL1              0x1a70
#define AMDV_PATH_SWAP_CTRL2              0x1a71
#define MALI_AFBCD_TOP_CTRL                0x1a0f
#define MALI_AFBCD1_TOP_CTRL               0x1a55

#define FLAG_FORCE_CVM			0x01
#define FLAG_BYPASS_CVM			0x02
#define FLAG_BYPASS_VPP			0x04
#define FLAG_USE_SINK_MIN_MAX		0x08
#define FLAG_CLKGATE_WHEN_LOAD_LUT	0x10
#define FLAG_SINGLE_STEP		0x20
#define FLAG_CERTIFICATION		0x40
#define FLAG_CHANGE_SEQ_HEAD		0x80
#define FLAG_DISABLE_COMPOSER		0x100
#define FLAG_BYPASS_CSC			0x200
#define FLAG_CHECK_ES_PTS		0x400
#define FLAG_DISABE_CORE_SETTING	0x800
#define FLAG_DISABLE_DMA_UPDATE		0x1000
#define FLAG_DISABLE_DV_OUT		0x2000
#define FLAG_FORCE_DV_LL		0x4000
#define FLAG_FORCE_RGB_OUTPUT		0x8000
#define FLAG_DOVI2HDR10_NOMAPPING	0x100000
#define FLAG_PRIORITY_GRAPHIC		0x200000
#define FLAG_DISABLE_LOAD_VSVDB		0x400000
#define FLAG_DISABLE_CRC		0x800000
#define FLAG_ENABLE_EL			0x1000000
#define FLAG_DEBUG_CORE2_TIMING		0x2000000
#define FLAG_MUTE			0x4000000
#define FLAG_FORCE_HDMI_PKT		0x8000000
#define FLAG_SINGLE_STEP_PIP		0x10000000
#define FLAG_RX_EMP_VSEM		0x20000000
#define FLAG_TOGGLE_FRAME		0x80000000

enum core1_switch_type {
	NO_SWITCH = 0,
	SWITCH_BEFORE_DVCORE_1,
	SWITCH_BEFORE_DVCORE_2,
	SWITCH_AFTER_DVCORE,
};

enum core3_switch_type {
	CORE3_AFTER_WM = 0,
	CORE3_AFTER_OSD1_HDR,
	CORE3_AFTER_VD2_HDR,
};

enum core_type {
	AMDV_TVCORE = 0,
	AMDV_CORE1A,
	AMDV_CORE1B,
	AMDV_CORE1C,
	AMDV_CORE2A,
	AMDV_CORE2B,
	AMDV_CORE2C,
};

enum input_mode_enum {
	IN_MODE_OTT = 0,
	IN_MODE_HDMI = 1,
	IN_MODE_GRAPHICS = 2
};

struct ui_menu_params {
	u16 u16_backlight_ui_val;
	u16 u16_brightness_ui_val;
	u16 u16_contrast_ui_val;
};

enum signal_format_enum {
	FORMAT_INVALID = -1,
	FORMAT_DOVI = 0,
	FORMAT_HDR10 = 1,
	FORMAT_SDR = 2,/*FORMAT_SDR8*/
	FORMAT_DOVI_LL = 3,
	FORMAT_HLG = 4,
	FORMAT_HDR10PLUS = 5,
	FORMAT_SDR_2020 = 6,
	FORMAT_MVC = 7,
	FORMAT_SDR10 = 8,
	FORMAT_HDR8 = 9,
	FORMAT_CUVA = 10,
	FORMAT_PRIMESL = 11
};

enum priority_mode_enum  {
	V_PRIORITY = 0,
	G_PRIORITY = 1,
	VIDEO_PRIORITY_DELAY = 2
};

enum cp_chroma_format_enum {
	CP_P420 = 0,
	CP_UYVY = 1,
	CP_P444 = 2,
	CP_I444 = 3
};

enum cp_signal_range_enum  {
	SIGNAL_RANGE_SMPTE = 0,
	SIGNAL_RANGE_FULL  = 1,
	SIGNAL_RANGE_SDI   = 2
};

enum cp_clr_enum {
	CP_YUV = 0,
	CP_RGB = 1,
	CP_IPT = 2
};

enum graphics_format_enum  {
	G_SDR_YUV = 0,
	G_SDR_RGB = 1,
	G_HDR_YUV = 2,
	G_HDR_RGB = 3
};

enum dv_type_enum {
	DV_TYPE_DOVI = 0,
	DV_TYPE_ATSC = 1,
	DV_TYPE_DVB  = 2
};

struct dm_reg_ipcore2 {
	u32 s_range;
	u32 s_range_inverse;
	u32 y2rgb_coeff1;
	u32 y2rgb_coeff2;
	u32 y2rgb_coeff3;
	u32 y2rgb_coeff4;
	u32 y2rgb_coeff5;
	u32 y2rgb_off1;
	u32 y2rgb_off2;
	u32 y2rgb_off3;
	u32 frame_fmt;
	u32 eotf;
	u32 a2b_coeff1;
	u32 a2b_coeff2;
	u32 a2b_coeff3;
	u32 a2b_coeff4;
	u32 a2b_coeff5;
	u32 c2d_coeff1;
	u32 c2d_coeff2;
	u32 c2d_coeff3;
	u32 c2d_coeff4;
	u32 c2d_coeff5;
	u32 c2d_off;
	u32 vdr_res;
};

struct dm_reg_ipcore3 {
	u32 d2c_coeff1;
	u32 d2c_coeff2;
	u32 d2c_coeff3;
	u32 d2c_coeff4;
	u32 d2c_coeff5;
	u32 b2a_coeff1;
	u32 b2a_coeff2;
	u32 b2a_coeff3;
	u32 b2a_coeff4;
	u32 b2a_coeff5;
	u32 eotf_param1;
	u32 eotf_param2;
	u32 ipt_scale;
	u32 ipt_off1;
	u32 ipt_off2;
	u32 ipt_off3;
	u32 output_range1;
	u32 output_range2;
	u32 rgb2yuv_coeff_reg1;
	u32 rgb2yuv_coeff_reg2;
	u32 rgb2yuv_coeff_reg3;
	u32 rgb2yuv_coeff_reg4;
	u32 rgb2yuv_coeff_reg5;
	u32 rgb2yuv_off0;
	u32 rgb2yuv_off1;
	u32 rgb2yuv_off2;
};

/*hdmi metadata for core3 */
struct md_reg_ipcore3 {
	u32 raw_metadata[512];
	u32 size;
};

struct hdr10_infoframe {
	u8 type_code;
	u8 version_number;
	u8 len_of_info_frame;
	u8 data_byte1;
	u8 data_byte2;
	u8 primaries_x_0_lsb;
	u8 primaries_x_0_msb;
	u8 primaries_y_0_lsb;
	u8 primaries_y_0_msb;
	u8 primaries_x_1_lsb;
	u8 primaries_x_1_msb;
	u8 primaries_y_1_lsb;
	u8 primaries_y_1_msb;
	u8 primaries_x_2_lsb;
	u8 primaries_x_2_msb;
	u8 primaries_y_2_lsb;
	u8 primaries_y_2_msb;
	u8 white_point_x_lsb;
	u8 white_point_x_msb;
	u8 white_point_y_lsb;
	u8 white_point_y_msb;
	u8 max_display_mastering_lum_lsb;
	u8 max_display_mastering_lum_msb;
	u8 min_display_mastering_lum_lsb;
	u8 min_display_mastering_lum_msb;
	u8 max_content_light_level_lsb;
	u8 max_content_light_level_msb;
	u8 max_frame_avg_light_level_lsb;
	u8 max_frame_avg_light_level_msb;
};

struct hdr10_parameter {
	u32 min_display_mastering_lum;
	u32 max_display_mastering_lum;
	u16 r_x;
	u16 r_y;
	u16 g_x;
	u16 g_y;
	u16 b_x;
	u16 b_y;
	u16 w_x;
	u16 w_y;
	u16 max_content_light_level;
	u16 max_frame_avg_light_level;
};

struct ext_level_1 {
	u8 min_pq_h;
	u8 min_pq_l;
	u8 max_pq_h;
	u8 max_pq_l;
	u8 avg_pq_h;
	u8 avg_pq_l;
};

struct ext_level_2 {
	u8 target_max_pq_h;
	u8 target_max_pq_l;
	u8 trim_slope_h;
	u8 trim_slope_l;
	u8 trim_off_h;
	u8 trim_off_l;
	u8 trim_power_h;
	u8 trim_power_l;
	u8 trim_chroma_weight_h;
	u8 trim_chroma_weight_l;
	u8 trim_sat_gain_h;
	u8 trim_sat_gain_l;
	u8 ms_weight_h;
	u8 ms_weight_l;
};

struct ext_level_4 {
	u8 anchor_pq_h;
	u8 anchor_pq_l;
	u8 anchor_power_h;
	u8 anchor_power_l;
};

struct ext_level_5 {
	u8 active_area_left_off_h;
	u8 active_area_left_off_l;
	u8 active_area_right_off_h;
	u8 active_area_right_off_l;
	u8 active_area_top_off_h;
	u8 active_area_top_off_l;
	u8 active_area_bot_off_h;
	u8 active_area_bot_off_l;
};

struct ext_level_6 {
	u8 max_display_mastering_lum_h;
	u8 max_display_mastering_lum_l;
	u8 min_display_mastering_lum_h;
	u8 min_display_mastering_lum_l;
	u8 max_content_light_level_h;
	u8 max_content_light_level_l;
	u8 max_frame_avg_light_level_h;
	u8 max_frame_avg_light_level_l;
};

struct ext_level_254 {
	u8 mode;
	u8 version_index;
};

struct ext_level_255 {
	u8 run_mode;
	u8 run_version;
	u8 dm_debug_0;
	u8 dm_debug_1;
	u8 dm_debug_2;
	u8 dm_debug_3;
};

struct ext_md_s {
	u32 avail_level_mask;
	struct ext_level_1 level_1;
	struct ext_level_2 level_2;
	struct ext_level_4 level_4;
	struct ext_level_5 level_5;
	struct ext_level_6 level_6;
	struct ext_level_255 level_255;
};

struct dovi_setting_s {
	struct composer_reg_ipcore comp_reg;
	struct dm_reg_ipcore1 dm_reg1;
	struct dm_reg_ipcore2 dm_reg2;
	struct dm_reg_ipcore3 dm_reg3;
	struct dm_lut_ipcore dm_lut1;
	struct dm_lut_ipcore dm_lut2;
	/* for dovi output */
	struct md_reg_ipcore3 md_reg3;
	/* for hdr10 output */
	struct hdr10_infoframe hdr_info;
	/* current process */
	enum signal_format_enum src_format;
	enum signal_format_enum dst_format;
	/* enhanced layer */
	bool el_flag;
	bool el_halfsize_flag;
	/* frame width & height */
	u32 video_width;
	u32 video_height;
	/* use for stb 2.4 */
	enum graphics_format_enum g_format;
	u32 g_bitdepth;
	u32 dovi2hdr10_nomapping;
	u32 use_ll_flag;
	u32 ll_rgb_desired;
	u32 diagnostic_enable;
	u32 diagnostic_mux_select;
	u32 dovi_ll_enable;
	u32 vout_width;
	u32 vout_height;
	u8 vsvdb_tbl[32];
	struct ext_md_s ext_md;
	u32 vsvdb_len;
	u32 vsvdb_changed;
	u32 mode_changed;
};

#define NUM_INST 15
#define NUM_IPCORE1 2
#define NUM_IPCORE2 1
#define IPCORE2_ID NUM_IPCORE1
#define OUTPUT_CONTROL_DATA_SIZE 0x1000

struct dovi_setting_video_s {
	struct composer_reg_ipcore comp_reg;
	struct dm_reg_ipcore1 dm_reg;
	struct dm_lut_ipcore dm_lut;
};

struct private_info_s {
	int valid; /*1: input is valid; 0: input is invalid*/
	enum signal_format_enum src_format;

	/* enhanced layer */
	bool el_flag;
	bool el_halfsize_flag;

	/* frame width & height */
	u32 video_width;
	u32 video_height;

	/* Dovi LL or non Dovi */
	int set_bit_depth;
	enum cp_chroma_format_enum set_chroma_format;
	enum cp_signal_range_enum set_yuv_range;
	enum cp_clr_enum color_format;

	char *in_comp;
	int in_comp_size;
	char *in_md;
	int in_md_size;
	char *vsem_if;
	int vsem_if_size;
	enum input_mode_enum input_mode;
	int use_primaries_for_dv;
	struct hdr10_parameter *p_hdr10_param;
};

enum ctrl_data_type_enum {
	TYPE_INVALID = -1,
	TYPE_VSIF = 0,
	TYPE_VSEM = 1
};

enum cp_dv_type_eunm {
	SRC_TYPE_NONDOVI = 0x0, /*output is not amdv*/
	SRC_TYPE_DV    = 0x1, /*input is dv content, and output is amdv. */
	SRC_TYPE_HDR10   = 0x3, /*input is HDR10, and output is a amdv. */
	SRC_TYPE_SDR     = 0x5, /*input is SDR, and output is a amdv*/
	SRC_TYPE_HLG     = 0x7  /*input is HLG, and output is a amdv */
};

struct vsif_parameter_s {
	int lowlatency;
	int backlight_ctrl_md_present;
	int src_dm_version;
	int eff_tmax_pq;
	enum cp_dv_type_eunm dobly_vision_signal;
	int auxi_md_present;
	int l11_md_present;
	u8 auxi_runmode;
	u8 auxi_runversion;
	u8 auxi_debug0;
	u8 content_type;
	u8 intended_white_point;
	u8 l11_byte2;
	u8 l11_byte3;
	int bt2020_container;
};

struct content_info_s {
	u8 content_type_info;
	u8 white_point;
	u8 l11_byte2;
	u8 l11_byte3;
};

struct m_dovi_setting_s {
	/*input to dv lib*/
	int num_input;
	int num_video;
	int pri_input;
	int enable_multi_core1;
	int enable_debug;
	enum priority_mode_enum set_priority;
	u32 dovi2hdr10_nomapping;
	u32 use_ll_flag;
	u32 ll_rgb_desired;
	u32 vout_width;
	u32 vout_height;
	u8 vsvdb_tbl[32];
	u32 vsvdb_len;
	u32 vsvdb_changed;
	u32 mode_changed;
	enum signal_format_enum dst_format;

	/*private info for each instance*/
	struct private_info_s input[NUM_IPCORE1 + NUM_IPCORE2];

	/*only used for graphic*/
	int set_graphic_min_lum;
	int set_graphic_max_lum;
	int set_target_min_lum;
	int set_target_max_lum;

	/*get from dv lib*/
	struct dovi_setting_video_s core1[NUM_IPCORE1];
	struct dm_reg_ipcore2 dm_reg2;
	struct dm_reg_ipcore3 dm_reg3;
	struct dm_lut_ipcore dm_lut2;
	/* for dovi output */
	struct md_reg_ipcore3 md_reg3;
	/* for hdr10 output */
	struct hdr10_infoframe hdr_info;

	u32 diagnostic_enable;
	u32 diagnostic_mux_select;
	u32 dovi_ll_enable;
	struct ext_md_s ext_md;

	/*output vsif info from cp*/
	struct vsif_parameter_s output_vsif;
	u8 *output_ctrl_data; /* vsem or vsif */
	u32 output_ctrl_data_len;
	enum ctrl_data_type_enum ctrl_data_type;
	struct content_info_s content_info;/*copy src content infos*/

	/*reserved*/
	u32 reserved[128];/*user_l11+content_type+wp+byte2+byte3*/
};

struct dv_inst_s {
	char *md_buf[2];
	char *comp_buf[2];
	int current_id;/*metadata id*/
	int backup_comp_size;
	int backup_md_size;
	u32 last_total_md_size;
	u32 last_total_comp_size;
	bool last_mel_mode;
	struct hdr10_parameter hdr10_param;
	struct vframe_s *dv_vf[16][2];
	u32 frame_count;
	u32 amdv_src_format;/*kernel side*/
	enum signal_format_enum src_format;/*ko side*/
	int amdv_wait_count;
	bool amdv_wait_init;
	bool dv_unique_drm;
	void *metadata_parser;
	bool mapped;
	int layer_id;/*display on vd1 or vd2*/
	int valid;
	/* enhanced layer */
	bool el_flag;
	bool el_halfsize_flag;
	/* frame width & height */
	u32 video_width;
	u32 video_height;
	/* Dovi LL or non Dovi */
	int set_bit_depth;
	enum cp_chroma_format_enum set_chroma_format;
	enum cp_signal_range_enum set_yuv_range;
	enum cp_clr_enum color_format;
	char *in_comp;
	int in_comp_size;
	char *in_md;
	int in_md_size;
	char *vsem_if;
	int vsem_if_size;
	enum input_mode_enum input_mode;
};

struct dv_core1_inst_s {
	bool core1_on;
	bool amdv_setting_video_flag;
	u32 core1_on_cnt;
	u32 run_mode_count;
	u32 core1_disp_hsize;
	u32 core1_disp_vsize;
};

struct tv_input_info_s {
	s16 brightness_off[8][2];
	s32 content_fps;
	s32 gd_rf_adjust;
	s32 tid;
	s32 debug_buf[497];
};

#define PREFIX_SEI_NUT_NAL 39
#define SUFFIX_SEI_NUT_NAL 40
#define SEI_ITU_T_T35 4
#define SEI_TYPE_MASTERING_DISP_COLOUR_VOLUME 137
#define ATSC_T35_PROV_CODE    0x0031
#define DVB_T35_PROV_CODE     0x003B
#define ATSC_USER_ID_CODE     0x47413934
#define DVB_USER_ID_CODE      0x00000000
#define DM_MD_USER_TYPE_CODE  0x09

#define MAX_LEN_2086_SEI 256
#define MAX_LEN_2094_SEI 256

/* vui params for ATSC 3.0*/
struct dv_vui_parameters {
	s32  video_fmt_i;
	s32  video_fullrange_b;
	s32  color_description_b;
	s32  color_primaries_i;
	s32  trans_characteristic_i;
	s32  matrix_coeff_i;
};

/* atsc related params.*/
struct dv_atsc {
	struct dv_vui_parameters vui_param;
	u32 len_2086_sei;
	u8  sei_2086[MAX_LEN_2086_SEI];
	u32 len_2094_sei;
	u8  sei_2094[MAX_LEN_2094_SEI];
};

enum cpu_id_e {
	_CPU_MAJOR_ID_GXM,
	_CPU_MAJOR_ID_TXLX,
	_CPU_MAJOR_ID_G12,
	_CPU_MAJOR_ID_TM2,
	_CPU_MAJOR_ID_TM2_REVB,
	_CPU_MAJOR_ID_SC2,
	_CPU_MAJOR_ID_T7,
	_CPU_MAJOR_ID_T3,
	_CPU_MAJOR_ID_S4D,
	_CPU_MAJOR_ID_T5W,
	_CPU_MAJOR_ID_S5,
	_CPU_MAJOR_ID_UNKNOWN,
};

struct dv_device_data_s {
	enum cpu_id_e cpu_id;
};

struct amdolby_vision_port_t {
	const char *name;
	struct device *dev;
	const struct file_operations *fops;
	void *runtime;
};

/***extern****/
extern const struct dolby_vision_func_s *p_funcs_stb;
extern const struct dolby_vision_func_s *p_funcs_tv;
extern const struct dolby_vision_func_s *p_funcs_m_stb;
extern bool force_stb_mode;
extern unsigned int debug_dolby;
extern unsigned int dolby_vision_flags;
extern uint amdv_mask;
extern bool dolby_vision_on;
extern bool amdv_core1_on;
extern u32 amdv_core1_on_cnt;
extern u32 amdv_core2_on_cnt;
extern uint amdv_on_count;
extern struct dv_core1_inst_s dv_core1[NUM_IPCORE1];
extern uint amdv_run_mode;
extern uint amdv_run_mode_delay;
extern struct tv_dovi_setting_s *tv_dovi_setting;
extern struct dovi_setting_s dovi_setting;
extern struct dovi_setting_s new_dovi_setting;
extern struct m_dovi_setting_s m_dovi_setting;
extern struct m_dovi_setting_s new_m_dovi_setting;
extern struct m_dovi_setting_s invalid_m_dovi_setting;
extern bool multi_dv_mode;
extern u32 dolby_vision_mode;
extern u32 dolby_vision_ll_policy;
extern u32 last_dolby_vision_ll_policy;
extern bool amdv_setting_video_flag;
extern void *pq_config_fake;
extern struct dv_inst_s dv_inst[NUM_INST];
extern int hdmi_path_id;
extern u32 dv_cert_graphic_width;
extern u32 dv_cert_graphic_height;
extern bool force_set_lut;
extern int use_target_lum_from_cfg;
extern s16 brightness_off[8][2];
extern bool disable_aoi;
extern u32 aoi_info[2][4];
extern int debug_disable_aoi;
extern bool update_aoi_info;
extern int debug_dma_start_line;
extern unsigned int debug_vpotch;
extern int copy_core1a;
extern int core2_sel;
extern bool force_bypass_from_prebld_to_vadj1;
extern struct hdr10_parameter hdr10_param;
extern int cur_valid_video_num;
extern int (*get_osd_status)(u32 index);
extern bool amdv_wait_on;
extern struct vpp_post_info_t core3_slice_info;
/************/

#define pr_dv_dbg(fmt, args...)\
	do {\
		if (debug_dolby)\
			pr_info("AMDV: " fmt, ## args);\
	} while (0)
#define pr_dv_error(fmt, args...)\
	pr_info("AMDV ERROR: " fmt, ## args)

int control_path
	(enum signal_format_enum in_format,
	 enum signal_format_enum out_format,
	 char *in_comp, int in_comp_size,
	 char *in_md, int in_md_size,
	 enum priority_mode_enum set_priority,
	 int set_bit_depth, int set_chroma_format, int set_yuv_range,
	 int set_graphic_min_lum, int set_graphic_max_lum,
	 int set_target_min_lum, int set_target_max_lum,
	 int set_no_el,
	 struct hdr10_parameter *hdr10_param,
	 struct dovi_setting_s *output);

int multi_control_path(struct m_dovi_setting_s *cp_para);

struct tv_dovi_setting_s {
	u64 core1_reg_lut[3754];
	/* current process */
	enum signal_format_enum src_format;
	enum signal_format_enum dst_format;
	/* enhanced layer */
	bool el_flag;
	bool el_halfsize_flag;
	/* frame width & height */
	u32 video_width;
	u32 video_height;
	enum input_mode_enum input_mode;
	u16 backlight;
};

int tv_control_path
	(enum signal_format_enum in_format,
	 enum input_mode_enum in_mode,
	 char *in_comp, int in_comp_size,
	 char *in_md, int in_md_size,
	 int set_bit_depth, int set_chroma_format, int set_yuv_range,
	 struct pq_config *pq_config,
	 struct ui_menu_params *menu_param,
	 int set_no_el,
	 struct hdr10_parameter *hdr10_param,
	 struct tv_dovi_setting_s *output,
	 char *vsem_if, int vsem_if_size,
	 struct ambient_cfg_s *ambient_cfg,
	 struct tv_input_info_s *input_info);

struct dolby_vision_func_s {
	const char *version_info;
	void * (*metadata_parser_init)(int flag);
	/*flag: bit0 flag, bit1 0->dv, 1->atsc*/
	int (*metadata_parser_reset)(int flag);
	int (*metadata_parser_process)(char  *src_rpu,
				       int rpu_len,
				       char  *dst_comp,
				       int *comp_len,
				       char  *dst_md,
				       int *md_len,
				       bool src_eos);
	void (*metadata_parser_release)(void);
	int (*control_path)(enum signal_format_enum in_format,
			    enum signal_format_enum out_format,
			    char *in_comp, int in_comp_size,
			    char *in_md, int in_md_size,
			    enum priority_mode_enum set_priority,
			    int set_bit_depth, int set_chroma_format,
			    int set_yuv_range, int set_graphic_min_lum,
			    int set_graphic_max_lum,
			    int set_target_min_lum,
			    int set_target_max_lum,
			    int set_no_el,
			    struct hdr10_parameter *hdr10_param,
			    struct dovi_setting_s *output);
	int (*tv_control_path)(enum signal_format_enum in_format,
			       enum input_mode_enum in_mode,
			       char *in_comp, int in_comp_size,
			       char *in_md, int in_md_size,
			       int set_bit_depth, int set_chroma_format,
			       int set_yuv_range,
			       struct pq_config *pq_config,
			       struct ui_menu_params *menu_param,
			       int set_no_el,
			       struct hdr10_parameter *hdr10_param,
			       struct tv_dovi_setting_s *output,
			       char *vsem_if, int vsem_if_size,
			       struct ambient_cfg_s *ambient_cfg,
			       struct tv_input_info_s *input_info);
	int (*multi_control_path)(struct m_dovi_setting_s *cp_para);
	void *(*multi_mp_init)(int flag);
	int (*multi_mp_reset)(void *ctx_arg, int flag);
	int (*multi_mp_process)(void *ctx_arg,
				char *src_rpu, int rpu_len,
				char *dst_comp, int *comp_len,
				char *dst_md, int *md_len, bool src_eos,
				enum dv_type_enum input_format);
	void (*multi_mp_release)(void **ctx_arg);
};

int register_dv_functions(const struct dolby_vision_func_s *func);
int unregister_dv_functions(void);
#ifndef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
#define VSYNC_WR_MPEG_REG(adr, val) WRITE_VPP_REG(adr, val)
#define VSYNC_RD_MPEG_REG(adr) READ_VPP_REG(adr)
#define VSYNC_WR_MPEG_REG_BITS(adr, val, start, len) \
	WRITE_VPP_REG_BITS(adr, val, start, len)
#else
int VSYNC_WR_MPEG_REG_BITS(u32 adr, u32 val, u32 start, u32 len);
u32 VSYNC_RD_MPEG_REG(u32 adr);
int VSYNC_WR_MPEG_REG(u32 adr, u32 val);
#endif

void dv_mem_power_on(enum vpu_mod_e mode);
void dv_mem_power_off(enum vpu_mod_e mode);
int get_dv_mem_power_flag(enum vpu_mod_e mode);

bool is_aml_gxm(void);
bool is_aml_g12(void);
bool is_aml_box(void);
bool is_aml_txlx(void);
bool is_aml_txlx_tvmode(void);
bool is_aml_txlx_stbmode(void);
bool is_aml_tm2(void);
bool is_aml_tm2_tvmode(void);
bool is_aml_tm2_stbmode(void);
bool is_aml_stb_hdmimode(void);
bool is_aml_tvmode(void);
bool is_aml_tm2revb(void);
bool is_aml_sc2(void);
bool is_aml_s4d(void);
bool is_aml_t7(void);
bool is_aml_t7_tvmode(void);
bool is_aml_t7_stbmode(void);
bool is_aml_t3(void);
bool is_aml_t3_tvmode(void);
bool is_aml_t5w(void);
bool is_amdv_stb_mode(void);
bool is_aml_s5(void);

u32 VSYNC_RD_DV_REG(u32 adr);
int VSYNC_WR_DV_REG(u32 adr, u32 val);
int VSYNC_WR_DV_REG_BITS(u32 adr, u32 val, u32 start, u32 len);
u32 READ_VPP_DV_REG(u32 adr);
int WRITE_VPP_DV_REG(u32 adr, const u32 val);

int tv_dv_core1_set(u64 *dma_data,
			     dma_addr_t dma_paddr,
			     int hsize, int vsize,
			     int bl_enable, int el_enable,
			     int el_41_mode, int src_chroma_format,
			     bool hdmi, bool hdr10, bool reset);

void apply_stb_core_settings(dma_addr_t dma_paddr,
				    bool enable, bool enable_core1_s1,
				    unsigned int mask, bool reset_core1a,
				    bool reset_core1b, u32 frame_size,
				    u32 frame_size_1, u8 pps_state);
void adjust_vpotch(u32 graphics_w, u32 graphics_h);
void adjust_vpotch_tv(void);
void video_effect_bypass(int bypass);
void enable_amdv(int enable);
int get_mute_type(void);
void set_update_cfg(bool flag);
void update_pwm_control(void);
void parse_param_amdv(char *buf_orig, char **parm);
int prepare_stb_dvcore1_reg
	(u32 run_mode, u32 *p_core1_dm_regs, u32 *p_core1_comp_regs);
void prepare_stb_dvcore1_lut(u32 base, u32 *p_core1_lut);
bool need_skip_cvm(unsigned int is_graphic);
void set_debug_bypass_vpp_pq(int val);
void set_bypass_all_vpp_pq(int flag);
void update_dma_buf(void);
void set_dovi_setting_update_flag(bool flag);
void set_vsync_count(int val);
void set_force_reset_core2(bool flag);
void set_amdv_wait_on(void);
void clear_dolby_vision_wait(void);
void set_frame_count(int val);
int get_frame_count(void);
void reset_dv_param(void);
void update_stb_core_setting_flag(int flag);
u32 get_graphic_width(u32 index);
u32 get_graphic_height(u32 index);
bool need_send_emp_meta(const struct vinfo_s *vinfo);
void convert_hdmi_metadata(uint32_t *md);
bool get_core1a_core1b_switch(void);
bool core1_detunnel(void);
bool get_disable_video_flag(enum vd_path_e vd_path);
void set_operate_mode(int mode);
int get_operate_mode(void);
void set_dv_control_backlight_status(bool flag);
void set_vf_crc_valid(int val);
int get_dv_vpu_mem_power_status(enum vpu_mod_e mode);
void calculate_panel_max_pq
	(enum signal_format_enum src_format,
	 const struct vinfo_s *vinfo,
	 struct target_config *config);
int layer_id_to_dv_id(enum vd_path_e vd_path);
bool layerid_valid(int layerid);
bool dv_inst_valid(int id);

#endif
