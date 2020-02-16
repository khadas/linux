/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
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

#define V1_5
#define V2_4
/*  driver version */
#define DRIVER_VER "20181220"

#include <linux/types.h>

#define DEF_G2L_LUT_SIZE_2P        8
#define DEF_G2L_LUT_SIZE           BIT(DEF_G2L_LUT_SIZE_2P)

#ifdef V2_4
#define EXT_MD_AVAIL_LEVEL_1 BIT(0)
#define EXT_MD_AVAIL_LEVEL_2 BIT(1)
#define EXT_MD_AVAIL_LEVEL_4 BIT(2)
#define EXT_MD_AVAIL_LEVEL_5 BIT(3)
#define EXT_MD_AVAIL_LEVEL_6 BIT(4)
#define EXT_MD_AVAIL_LEVEL_255 BIT(31)
#endif
#define PQ2G_LUT_SIZE (4 + 1024 * 4 + 16 * 3)
#define GM_LUT_HDR_SIZE  (13 + 2 * 9)
#define LUT_DIM          17
#define GM_LUT_SIZE      (3 * LUT_DIM * LUT_DIM * LUT_DIM * 2)
#define BACLIGHT_LUT_SIZE 4096
#define TUNING_LUT_ENTRIES 14

#define TUNINGMODE_FORCE_ABSOLUTE        0x1
#define TUNINGMODE_EXTLEVEL1_DISABLE     0x2
#define TUNINGMODE_EXTLEVEL2_DISABLE     0x4
#define TUNINGMODE_EXTLEVEL4_DISABLE     0x8
#define TUNINGMODE_EXTLEVEL5_DISABLE     0x10
#define TUNINGMODE_EL_FORCEDDISABLE      0x20

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

/*! @brief Output CSC configuration.*/
# pragma pack(push, 1)
struct TGT_out_csc_cfg {
	s32   lms2_rgb_mat[3][3]; /**<@brief  LMS to RGB matrix */
	s32   lms2_rgb_mat_scale;  /**<@brief  LMS 2 RGB matrix scale */
	u8   white_point[3];    /**<@brief  White point */
	u8   white_point_scale;  /**<@brief  White point scale */
	s32   reserved[3];
};

#pragma pack(pop)

/*! @brief Global dimming configuration.*/
# pragma pack(push, 1)
struct TGT_gd_cfg {
	s32   gd_enable;
	u32  gd_w_min;
	u32  gd_w_max;
	u32  gd_w_mm;
	u32  gd_w_dynrng_sqrt;
	u32  gd_weightmean;
	u32  gd_weightstd;
	u32  gd_delaymilli_sec_hdmi;
	s32   gd_rgb2_yuv_ext;
	s32   gd_m33_rgb2_yuv[3][3];
	s32   gd_m33_rgb2_yuv_scale2p;
	s32   gd_rgb2_yuv_off_ext;
	s32   gd_V3_rgb2_yuv_off[3];
	u32  gd_up_bound;
	u32  gd_low_bound;
	u32  last_max_pq;
	u16  gd_w_min_pq;
	u16  gd_w_max_pq;
	u16  gd_w_mm_pq;
	u16  gd_trigger_period;
	u32  gd_trigger_lin_thresh;
	u32  gd_delay_milli_sec_ott;
#ifdef V1_5
	u32  reserved[6];
#else
	u32  reserved[9];
#endif
};

#pragma pack(pop)

/*! @defgroup general Enumerations and data structures*/
# pragma pack(push, 1)
struct TARGET_display_config {
	u16 gain;
	u16 offset;
	u16 gamma;         /**<@brief  Gamma */
	u16 eotf;
	u16 bit_depth;      /**<@brief  Bit Depth */
	u16 range_spec;
	u16 diag_size;      /**<@brief  Diagonal Size */
	u16 max_pq;
	u16 min_pq;
	u16 ms_weight;
	u16 ms_edge_weight;
	s16  min_pq_bias;
	s16  mid_pq_bias;
	s16  max_pq_bias;
	s16  trim_slope_bias;
	s16  trim_offset_bias;
	s16  trim_power_bias;
	s16  ms_weight_bias;
	s16  brightness;         /**<@brief  Brighness */
	s16  contrast;           /**<@brief  Contrast */
	s16  chroma_weight_bias;
	s16  saturation_gain_bias;
	u16 chroma_weight;
	u16 saturation_gain;
	u16 cross_talk;
	u16 tuning_mode;
	s16 reserved0;
	s16 dbg_exec_params_print_period;
	s16 dbg_dm_md_print_period;
	s16 dbg_dm_cfg_print_period;
	u16 max_pq_dupli;
	u16 min_pq_dupli;
	s32 key_weight;
	s32 intensity_vector_weight;
	s32 chroma_vector_weight;
	s16 chip_fpga_lowcomplex;
	s16 mid_pq_bias_lut[TUNING_LUT_ENTRIES];
	s16 saturation_gain_bias_lut[TUNING_LUT_ENTRIES];
	s16 chroma_weight_bias_lut[TUNING_LUT_ENTRIES];
	s16 slope_bias_lut[TUNING_LUT_ENTRIES];
	s16 offset_bias_lut[TUNING_LUT_ENTRIES];
	s16 backlight_bias_lut[TUNING_LUT_ENTRIES];
	struct TGT_gd_cfg gd_config;
#ifdef V1_5
	u8  vsvdb[7];
	u8  reserved1[5];
#endif
	s32  min_lin;
	s32  max_lin;
	s16  backlight_scaler;
	s32  min_lin_dupli;
	s32  max_lin_dupli;
	struct TGT_out_csc_cfg ocsc_config;
#ifdef V1_5
	s16  reserved2;
#else
	s16  reserved00;
#endif
	s16  brightness_preservation;
	s32  iintensity_vector_weight;
	s32  ichroma_vector_weight;
	s16  isaturation_gain_bias;
	s16  chip_12b_ocsc;
	s16  chip_512_tonecurve;
	s16  chip_nolvl5;
	s16  padding[8];
};

#pragma pack(pop)

/*! @brief PQ config main data structure.*/
struct pq_config_s {
	unsigned char default_gm_lut[GM_LUT_HDR_SIZE + GM_LUT_SIZE];
	unsigned char gd_gm_lut_min[GM_LUT_HDR_SIZE + GM_LUT_SIZE];
	unsigned char gd_gm_lut_max[GM_LUT_HDR_SIZE + GM_LUT_SIZE];
	unsigned char pq2gamma[sizeof(s32) * PQ2G_LUT_SIZE];
	unsigned char backlight_lut[BACLIGHT_LUT_SIZE];
	struct TARGET_display_config target_display_config;
};

enum input_mode_e {
	INPUT_MODE_OTT  = 0,
	INPUT_MODE_HDMI = 1
};

struct ui_menu_params_s {
	u16 u16_back_light_ui_val;
	u16 u16_brightness_ui_val;
	u16 u16_contrast_ui_val;
};

enum signal_format_e {
	FORMAT_INVALID = -1,
	FORMAT_DOVI = 0,
	FORMAT_HDR10 = 1,
	FORMAT_SDR = 2,
	FORMAT_DOVI_LL = 3,
	FORMAT_HLG = 4,
	FORMAT_HDR10PLUS = 5,
	FORMAT_SDR_2020 = 6,
	FORMAT_MVC = 7
};

enum priority_mode_e {
	VIDEO_PRIORITY = 0,
	GRAPHIC_PRIORITY = 1,
	/* same as video priority, but will only switch to video*/
	/* priority after scene refresh flag has been received */
	VIDEO_PRIORITY_DELAYED = 2
};

enum cp_signal_range_e {
	SIG_RANGE_SMPTE = 0,  /* head range */
	SIG_RANGE_FULL  = 1,  /* full range */
	SIG_RANGE_SDI   = 2           /* PQ */
};

enum graphics_format_e {
	GF_SDR_YUV = 0,  /* BT.709 YUV BT1886 */
	GF_SDR_RGB = 1,  /* BT.709 RGB BT1886 */
	GF_HDR_YUV = 2,  /* BT.2020 YUV PQ */
	GF_HDR_RGB = 3   /* BT.2020 RGB PQ */
};

struct run_mode_s {
	u16 width;
	u16 height;
	u16 el_width;
	u16 el_height;
	u16 hdmi_mode;
};

struct composer_register_ipcore_s {
	/* offset 0xc8 */
	u32 COMPOSER_mode;
	u32 VDR_resolution;
	u32 BIT_depth;
	u32 COEFFICIENT_log2_denominator;
	u32 BL_num_pivots_y;
	u32 BL_pivot[5];
	u32 BL_order;
	u32 BL_coefficient_y[8][3];
	u32 EL_NLQ_offset_y;
	u32 EL_coefficient_y[3];
	u32 MAPPING_IDC_u;
	u32 BL_num_pivots_u;
	u32 BL_pivot_U[3];
	u32 BL_order_U;
	u32 BL_coefficient_U[4][3];
	u32 MMR_coefficient_U[22][2];
	u32 MMR_order_u;
	u32 EL_NLQ_offset_u;
	u32 EL_coefficient_u[3];
	u32 MAPPING_idc_v;
	u32 BL_num_pivots_v;
	u32 BL_pivot_v[3];
	u32 BL_order_v;
	u32 BL_coefficient_v[4][3];
	u32 MMR_coefficient_v[22][2];
	u32 MMR_order_v;
	u32 EL_NLQ_offset_v;
	u32 EL_coefficient_v[3];
};

/** @brief DM registers for IPCORE 1 */
struct dm_register_ipcore_1_s {
	u32 SRANGE;
	u32 SRANGE_inverse;
	u32 FRAME_format_1;
	u32 FRAME_format_2;
	u32 FRAME_pixel_def;
	u32 Y2RGB_coefficient_1;
	u32 Y2RGB_coefficient_2;
	u32 Y2RGB_coefficient_3;
	u32 Y2RGB_coefficient_4;
	u32 Y2RGB_coefficient_5;
	u32 Y2RGB_offset_1;
	u32 Y2RGB_offset_2;
	u32 Y2RGB_offset_3;
	u32 EOTF;
/*	u32 Sparam_1;*/
/*	u32 Sparam_2;*/
/*	u32 Sgamma; */
	u32 A2B_coefficient_1;
	u32 A2B_coefficient_2;
	u32 A2B_coefficient_3;
	u32 A2B_coefficient_4;
	u32 A2B_coefficient_5;
	u32 C2D_coefficient_1;
	u32 C2D_coefficient_2;
	u32 C2D_coefficient_3;
	u32 C2D_coefficient_4;
	u32 C2D_coefficient_5;
	u32 C2D_offset;
	u32 ACTIVE_area_left_top;
	u32 ACTIVE_area_bottom_right;
};

/** @brief DM registers for IPCORE 2 */
struct dm_register_ipcore_2_s {
	u32 SRANGE;
	u32 SRANGE_inverse;
	u32 Y2RGB_coefficient_1;
	u32 Y2RGB_coefficient_2;
	u32 Y2RGB_coefficient_3;
	u32 Y2RGB_coefficient_4;
	u32 Y2RGB_coefficient_5;
	u32 Y2RGB_offset_1;
	u32 Y2RGB_offset_2;
	u32 Y2RGB_offset_3;
	u32 FRAME_format;
	u32 EOTF;
	u32 A2B_coefficient_1;
	u32 A2B_coefficient_2;
	u32 A2B_coefficient_3;
	u32 A2B_coefficient_4;
	u32 A2B_coefficient_5;
	u32 C2D_coefficient_1;
	u32 C2D_coefficient_2;
	u32 C2D_coefficient_3;
	u32 C2D_coefficient_4;
	u32 C2D_coefficient_5;
	u32 C2D_offset;
	u32 VDR_resolution;
};

/** @brief DM registers for IPCORE 3 */
struct dm_register_ipcore_3_s {
	u32 D2C_coefficient_1;
	u32 D2C_coefficient_2;
	u32 D2C_coefficient_3;
	u32 D2C_coefficient_4;
	u32 D2C_coefficient_5;
	u32 B2A_coefficient_1;
	u32 B2A_coefficient_2;
	u32 B2A_coefficient_3;
	u32 B2A_coefficient_4;
	u32 B2A_coefficient_5;
	u32 EOTF_param_1;
	u32 EOTF_param_2;
	u32 IPT_scale;
	u32 IPT_offset_1;
	u32 IPT_offset_2;
	u32 IPT_offset_3;
	u32 OUTPUT_range_1;
	u32 OUTPUT_range_2;
	u32 RGB2YUV_coefficient_register1;
	u32 RGB2YUV_coefficient_register2;
	u32 RGB2YUV_coefficient_register3;
	u32 RGB2YUV_coefficient_register4;
	u32 RGB2YUV_coefficient_register5;
	u32 RGB2YUV_offset_0;
	u32 RGB2YUV_offset_1;
	u32 RGB2YUV_offset_2;
};

/** @brief DM luts for IPCORE 1 and 2 */
struct dm_lut_ipcore_s {
	u32 tm_lut_i[64 * 4];
	u32 tm_lut_s[64 * 4];
	u32 sm_lut_i[64 * 4];
	u32 sm_lut_s[64 * 4];
	u32 G2L[DEF_G2L_LUT_SIZE];
};

/** @brief hdmi metadata for IPCORE 3 */
struct md_reister_ipcore_3_s {
	u32 raw_metadata[512];
	u32 size;
};

struct hdr_10_infoframe_s {
	u8 infoframe_type_code;
	u8 infoframe_version_number;
	u8 length_of_info_frame;
	u8 data_byte_1;
	u8 data_byte_2;
	u8 display_primaries_x_0_LSB;
	u8 display_primaries_x_0_MSB;
	u8 display_primaries_y_0_LSB;
	u8 display_primaries_y_0_MSB;
	u8 display_primaries_x_1_LSB;
	u8 display_primaries_x_1_MSB;
	u8 display_primaries_y_1_LSB;
	u8 display_primaries_y_1_MSB;
	u8 display_primaries_x_2_LSB;
	u8 display_primaries_x_2_MSB;
	u8 display_primaries_y_2_LSB;
	u8 display_primaries_y_2_MSB;
	u8 white_point_x_LSB;
	u8 white_point_x_MSB;
	u8 white_point_y_LSB;
	u8 white_point_y_MSB;
	u8 max_display_mastering_luminance_LSB;
	u8 max_display_mastering_luminance_MSB;
	u8 min_display_mastering_luminance_LSB;
	u8 min_display_mastering_luminance_MSB;
	u8 max_content_light_level_LSB;
	u8 max_content_light_level_MSB;
	u8 max_frame_average_light_level_LSB;
	u8 max_frame_average_light_level_MSB;
};

struct hdr10_param_s {
	u32 min_display_mastering_luminance;
	u32 max_display_mastering_luminance;
	u16 Rx;
	u16 Ry;
	u16 Gx;
	u16 Gy;
	u16 Bx;
	u16 By;
	u16 Wx;
	u16 Wy;
	u16 max_content_light_level;
	u16 max_pic_average_light_level;
};

#ifdef V2_4
struct ext_level_1_s {
	u8 min_PQ_hi;
	u8 min_PQ_lo;
	u8 max_PQ_hi;
	u8 max_PQ_lo;
	u8 avg_PQ_hi;
	u8 avg_PQ_lo;
};

struct ext_level_2_s {
	u8 target_max_PQ_hi;
	u8 target_max_PQ_lo;
	u8 trim_slope_hi;
	u8 trim_slope_lo;
	u8 trim_offset_hi;
	u8 trim_offset_lo;
	u8 trim_power_hi;
	u8 trim_power_lo;
	u8 trim_chroma_weight_hi;
	u8 trim_chroma_weight_lo;
	u8 trim_saturation_gain_hi;
	u8 trim_saturation_gain_lo;
	u8 ms_weight_hi;
	u8 ms_weight_lo;
};

struct ext_level_4_s {
	u8 anchor_PQ_hi;
	u8 anchor_PQ_lo;
	u8 anchor_power_hi;
	u8 anchor_power_lo;
};

struct ext_level_5_s {
	u8 active_area_left_offset_hi;
	u8 active_area_left_offset_lo;
	u8 active_area_right_offset_hi;
	u8 active_area_right_offset_lo;
	u8 active_area_top_offset_hi;
	u8 active_area_top_offset_lo;
	u8 active_area_bottom_offset_hi;
	u8 active_area_bottom_offset_lo;
};

struct ext_level_6_s {
	u8 max_display_mastering_luminance_hi;
	u8 max_display_mastering_luminance_lo;
	u8 min_display_mastering_luminance_hi;
	u8 min_display_mastering_luminance_lo;
	u8 max_content_light_level_hi;
	u8 max_content_light_level_lo;
	u8 max_frame_average_light_level_hi;
	u8 max_frame_average_light_level_lo;
};

struct ext_level_255_s {
	u8 dm_run_mode;
	u8 dm_run_version;
	u8 dm_debug0;
	u8 dm_debug1;
	u8 dm_debug2;
	u8 dm_debug3;
};

struct ext_md_s {
	u32 available_level_mask;
	struct ext_level_1_s level_1;
	struct ext_level_2_s level_2;
	struct ext_level_4_s level_4;
	struct ext_level_5_s level_5;
	struct ext_level_6_s level_6;
	struct ext_level_255_s level_255;
};
#endif

struct dovi_setting_s {
	struct composer_register_ipcore_s comp_reg;
	struct dm_register_ipcore_1_s dm_reg1;
	struct dm_register_ipcore_2_s dm_reg2;
	struct dm_register_ipcore_3_s dm_reg3;
	struct dm_lut_ipcore_s dm_lut1;
	struct dm_lut_ipcore_s dm_lut2;
	/* for dovi output */
	struct md_reister_ipcore_3_s md_reg3;
	/* for hdr10 output */
	struct hdr_10_infoframe_s hdr_info;
	/* current process */
	enum signal_format_e src_format;
	enum signal_format_e dst_format;
	/* enhanced layer */
	bool el_flag;
	bool el_halfsize_flag;
	/* frame width & height */
	u32 video_width;
	u32 video_height;
#ifdef V2_4
	/* use for stb 2.4 */
	enum graphics_format_e g_format;
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
#endif
};

enum cpu_id_e {
	_CPU_MAJOR_ID_GXM,
	_CPU_MAJOR_ID_TXLX,
	_CPU_MAJOR_ID_G12,
	_CPU_MAJOR_ID_TM2,
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

extern int control_path
	(enum signal_format_e in_format,
	 enum signal_format_e out_format,
	 char *in_comp, int in_comp_size,
	 char *in_md, int in_md_size,
	 enum priority_mode_e set_priority,
	 int set_bit_depth, int set_chroma_format, int set_yuv_range,
	 int set_graphic_min_lum, int set_graphic_max_lum,
	 int set_target_min_lum, int set_target_max_lum,
	 int set_no_el,
	 struct hdr10_param_s *hdr10_param,
	 struct dovi_setting_s *output);

struct tv_dovi_setting_s {
	u64 core1_reg_lut[3754];
	/* current process */
	enum signal_format_e src_format;
	enum signal_format_e dst_format;
	/* enhanced layer */
	bool el_flag;
	bool el_halfsize_flag;
	/* frame width & height */
	u32 video_width;
	u32 video_height;
	enum input_mode_e input_mode;
};

int tv_control_path
	(enum signal_format_e in_format,
	 enum input_mode_e in_mode,
	 char *in_comp, int in_comp_size,
	 char *in_md, int in_md_size,
	 int set_bit_depth, int set_chroma_format, int set_yuv_range,
	 struct pq_config_s *pq_config,
	 struct ui_menu_params_s *menu_param,
	 int set_no_el,
	 struct hdr10_param_s *hdr10_param,
	 struct tv_dovi_setting_s *output);

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

	int (*metadata_parser_reset)(int flag);

	int (*metadata_parser_process)
		(char  *src_rpu, int rpu_len,
		 char  *dst_comp, int *comp_len,
		 char  *dst_md, int *md_len, bool src_eos);

	void (*metadata_parser_release)(void);

	int (*control_path)
		(enum signal_format_e in_format,
		 enum signal_format_e out_format,
		 char *in_comp, int in_comp_size,
		 char *in_md, int in_md_size,
		 enum priority_mode_e set_priority,
		 int set_bit_depth, int set_chroma_format, int set_yuv_range,
		 int set_graphic_min_lum, int set_graphic_max_lum,
		 int set_target_min_lum, int set_target_max_lum,
		 int set_no_el,
		 struct hdr10_param_s *hdr10_param,
		 struct dovi_setting_s *output);
	int (*tv_control_path)
		(enum signal_format_e in_format,
		 enum input_mode_e in_mode,
		 char *in_comp, int in_comp_size,
		 char *in_md, int in_md_size,
		 int set_bit_depth, int set_chroma_format, int set_yuv_range,
		 struct pq_config_s *pq_config,
		 struct ui_menu_params_s *menu_param,
		 int set_no_el,
		 struct hdr10_param_s *hdr10_param,
		 struct tv_dovi_setting_s *output);
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
u32 get_video_enabled(void);
#endif
