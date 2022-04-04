/*
 * drivers/amlogic/media/enhancement/amvecm/dolby_vision/amdolby_vision.h
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
#ifndef _AMDV_H_
#define _AMDV_H_

/*  driver version */
#define DRIVER_VER "20220409"

#include <linux/types.h>

#define EXT_MD_LEVEL_1      BIT(0)
#define EXT_MD_LEVEL_2      BIT(1)
#define EXT_MD_LEVEL_4      BIT(2)
#define EXT_MD_LEVEL_5      BIT(3)
#define EXT_MD_LEVEL_6      BIT(4)
#define EXT_MD_LEVEL_255    BIT(31)
#define BACKLIGHT_LUT_SIZE  4096
#define AMBIENT_LUT_SIZE    8
#define TUNING_LUT_SIZE     14
#define DM4_TUNING_LUT_SIZE 7

#define TUNING_MODE_FORCE_ABSOLUTE         0x1
#define TUNING_MODE_EXTLEVEL_1_DISABLE     0x2
#define TUNING_MODE_EXTLEVEL_2_DISABLE     0x4
#define TUNING_MODE_EXTLEVEL_4_DISABLE     0x8
#define TUNING_MODE_EXTLEVEL_5_DISABLE     0x10
#define TUNING_MODE_EL_FORCE_DISABLE       0x20

#define VPP_VD1_DSC_CTRL                   0x1a83
#define VIU_VD1_PATH_CTRL                  0x1a73
#define VPP_VD2_DSC_CTRL                   0x1a84
#define VPP_VD3_DSC_CTRL                   0x1a85
#define DOLBY_PATH_SWAP_CTRL1              0x1a70
#define DOLBY_PATH_SWAP_CTRL2              0x1a71
#define MALI_AFBCD_TOP_CTRL                0x1a0f
#define MALI_AFBCD1_TOP_CTRL               0x1a55

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
	DOLBY_TVCORE = 0,
	DOLBY_CORE1A,
	DOLBY_CORE1B,
	DOLBY_CORE1C,
	DOLBY_CORE2A,
	DOLBY_CORE2B,
	DOLBY_CORE2C,
};

# pragma pack(push, 1)
struct tgt_out_csc_cfg {
	s32  lms2rgb_mat[3][3];
	s32  lms2rgb_mat_scale;
	u8   white_point[3];
	u8   white_point_scale;
	s32  reserved[3];
};
#pragma pack(pop)

# pragma pack(push, 1)
struct tgt_gc_cfg {
	s32   gd_enable;
	u32  gd_wmin;
	u32  gd_wmax;
	u32  gd_wmm;
	u32  gd_wdyn_rng_sqrt;
	u32  gd_weight_mean;
	u32  gd_weight_std;
	u32  gd_delay_msec_hdmi;
	s32  gd_rgb2yuv_ext;
	s32  gd_m33_rgb2yuv[3][3];
	s32  gd_m33_rgb2yuv_scale2p;
	s32  gd_rgb2yuv_off_ext;
	s32  gd_rgb2yuv_off[3];
	u32  gd_up_bound;
	u32  gd_low_bound;
	u32  last_max_pq;
	u16  gd_wmin_pq;
	u16  gd_wmax_pq;
	u16  gd_wm_pq;
	u16  gd_trigger_period;
	u32  gd_trigger_lin_thresh;
	u32  gd_delay_msec_ott;
	s16  gd_rise_weight;
	s16  gd_fall_weight;
	u32  gd_delay_msec_ll;
	u32  gd_contrast;
	u32  reserved[3];
};
#pragma pack(pop)

# pragma pack(push, 1)
struct ambient_cfg {
	u8  reserved[3];
	u8  dark_detail;
	u32 dark_detail_complum;
	u32 ambient;
	u32 t_front_lux;
	u32 t_front_lux_scale;
	u32 t_rear_lum;
	u32 t_rear_lum_scale;
	u32 t_whitexy[2];
	u32 t_surround_reflection;
	u32 t_screen_reflection;
	u32 al_delay;
	u32 al_rise;
	u32 al_fall;
};
#pragma pack(pop)

# pragma pack(push, 1)
struct tgt_ab_cfg {
	s32  ab_enable;
	u32  ab_highest_tmax;
	u32  ab_lowest_tmax;
	s16  ab_rise_weight;
	s16  ab_fall_weight;
	u32  ab_delay_msec_hdmi;
	u32  ab_delay_msec_ott;
	u32  ab_delay_msec_ll;
	u32  reserved[1];
};
#pragma pack(pop)

# pragma pack(push, 1)
struct target_config {
	u16 gamma;
	u16 eotf;
	u16 range_spec;
	u16 max_pq;
	u16 min_pq;
	u16 max_pq_dm3;
	u32 min_lin;
	u32 max_lin;
	u32 max_lin_dm3;
	s32 t_primaries[8];
	u16 m_sweight;
	s16 trim_slope_bias;
	s16 trim_offset_bias;
	s16 trim_power_bias;
	s16 ms_weight_bias;
	s16 chroma_weight_bias;
	s16 saturation_gain_bias;
	u16 tuning_mode;
	s16 d_brightness;
	s16 d_contrast;
	s16 d_color_shift;
	s16 d_saturation;
	s16 d_backlight;
	s16 dbg_exec_params_print_period;
	s16 dbg_dm_md_print_period;
	s16 dbg_dm_cfg_print_period;
	struct tgt_gc_cfg gd_config;
	struct tgt_ab_cfg ab_config;
	struct ambient_cfg ambient_config;
	u8 vsvdb[7];
	u8 dm31_avail;
	u8 ref_mode_dark_id;
	u8 apply_l11_wp;
	u8 reserved1[1];
	s16 backlight_scaler;
	struct tgt_out_csc_cfg ocsc_config;
	s16 bright_preservation;
	u8 total_viewing_modes_num;
	u8 viewing_mode_valid;
	u32 ambient_frontlux[AMBIENT_LUT_SIZE];
	u32 ambient_complevel[AMBIENT_LUT_SIZE];
	s16 mid_pq_bias_lut[TUNING_LUT_SIZE];
	s16 slope_bias_lut[TUNING_LUT_SIZE];
	s16 backlight_bias_lut[TUNING_LUT_SIZE];
	s16 user_brightness_ui_lut[DM4_TUNING_LUT_SIZE];
	u16 padding2;
	s16 blu_pwm[5];
	s16 blu_light[5];
	s16 padding[36];
};
#pragma pack(pop)

struct pq_config {
	unsigned char backlight_lut[BACKLIGHT_LUT_SIZE];
	struct target_config tdc;
};

enum input_mode_enum {
	IN_MODE_OTT  = 0,
	IN_MODE_HDMI = 1
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
	FORMAT_SDR = 2,
	FORMAT_DOVI_LL = 3,
	FORMAT_HLG = 4,
	FORMAT_HDR10PLUS = 5,
	FORMAT_SDR_2020 = 6,
	FORMAT_MVC = 7,
	FORMAT_PRIMESL = 8,
};

enum priority_mode_enum  {
	V_PRIORITY = 0,
	G_PRIORITY = 1,
	VIDEO_PRIORITY_DELAY = 2
};

enum cp_signal_range_enum  {
	SIGNAL_RANGE_SMPTE = 0,
	SIGNAL_RANGE_FULL  = 1,
	SIGNAL_RANGE_SDI   = 2
};

enum graphics_format_enum  {
	G_SDR_YUV = 0,
	G_SDR_RGB = 1,
	G_HDR_YUV = 2,
	G_HDR_RGB = 3
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

struct dv_cfg_info_s {
	int id;
	char pic_mode_name[32];
	s16  brightness;        /*Brightness */
	s16  contrast;          /*Contrast */
	s16  colorshift;        /*ColorShift or Tint*/
	s16  saturation;        /*Saturation or color */
	u8  vsvdb[7];
	int dark_detail;        /*dark detail, on or off*/
};

struct dv_pq_center_value_s {
	s16  brightness;        /*Brightness */
	s16  contrast;          /*Contrast */
	s16  colorshift;        /*ColorShift or Tint*/
	s16  saturation;        /*Saturation or color */
};

struct dv_pq_range_s {
	s16  left;
	s16  right;
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

enum cpuID_e {
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
	_CPU_MAJOR_ID_UNKNOWN,
};

struct dv_device_data_s {
	enum cpuID_e cpu_id;
};

struct amdolby_vision_port_t {
	const char *name;
	struct device *dev;
	const struct file_operations *fops;
	void *runtime;
};

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

void *metadata_parser_init(int flag);
int metadata_parser_reset(int flag);
int metadata_parser_process
	(char  *src_rpu, int rpu_len,
	 char  *dst_comp, int *comp_len,
	 char  *dst_md, int *md_len, bool src_eos);
void metadata_parser_release(void);

struct dolby_vision_func_s {
	const char *version_info;
	void * (*metadata_parser_init)(int flag);
	/*flag: bit0 flag, bit1 0->dv, 1->atsc*/
	int (*metadata_parser_reset)(int flag);
	int (*metadata_parser_process)
		(char  *src_rpu, int rpu_len,
		 char  *dst_comp, int *comp_len,
		 char  *dst_md, int *md_len, bool src_eos);
	void (*metadata_parser_release)(void);
	int (*control_path)
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
	int (*tv_control_path)
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
int get_dv_vpu_mem_power_status(enum vpu_mod_e mode);
bool get_disable_video_flag(enum vd_path_e vd_path);
#endif
