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
#define V1_6_1
#define V2_4
/*  driver version */
#define DRIVER_VER "20181220"

#include <linux/types.h>

/*#define DEF_G2L_LUT_SIZE_2P        8*/
#define DEF_G2L_LUT_SIZE           BIT(8) /*(1 << DEF_G2L_LUT_SIZE_2P)*/

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

#define VPP_VD1_DSC_CTRL                           0x1a83
#define VPP_VD2_DSC_CTRL                           0x1a84
#define VPP_VD3_DSC_CTRL                           0x1a85
#define DOLBY_PATH_SWAP_CTRL1                      0x1a70
#define DOLBY_PATH_SWAP_CTRL2                      0x1a71
#define MALI_AFBCD_TOP_CTRL                        0x1a0f
#define MALI_AFBCD1_TOP_CTRL                       0x1a55


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

/*! @brief Output CSC configuration.*/
# pragma pack(push, 1)
struct TgtOutCscCfg {
	s32   lms2RgbMat[3][3]; /**<@brief  LMS to RGB matrix */
	s32   lms2RgbMatScale;  /**<@brief  LMS 2 RGB matrix scale */
#ifdef V1_6_1
	s32   reserved[4];
#else
	u8   whitePoint[3];    /**<@brief  White point */
	u8   whitePointScale;  /**<@brief  White point scale */
	s32   reserved[3];
#endif
};
#pragma pack(pop)

/*! @brief Global dimming configuration.*/
# pragma pack(push, 1)
struct TgtGDCfg {
	s32   gdEnable;
	u32  gdWMin;
	u32  gdWMax;
	u32  gdWMm;
	u32  gdWDynRngSqrt;
	u32  gdWeightMean;
	u32  gdWeightStd;
	u32  gdDelayMilliSec_hdmi;
	s32   gdRgb2YuvExt;
	s32   gdM33Rgb2Yuv[3][3];
	s32   gdM33Rgb2YuvScale2P;
	s32   gdRgb2YuvOffExt;
	s32   gdV3Rgb2YuvOff[3];
	u32  gdUpBound;
	u32  gdLowBound;
	u32  lastMaxPq;
	u16  gdWMinPq;
	u16  gdWMaxPq;
	u16  gdWMmPq;
	u16  gdTriggerPeriod;
	u32  gdTriggerLinThresh;
	u32  gdDelayMilliSec_ott;
#ifdef V1_6_1
	s16   gdRiseWeight;      /*Back light rise weight signed Q3.12 */
	s16   gdFallWeight;      /*Back light fall weight signed Q3.12 */
	u32  gdDelayMilliSec_ll;/*Back light delay for LL case */
	u32  gdContrast;        /*GD Contrast DM 4 only */
	u32  reserved[3];
#else
#ifdef V1_5
	u32  reserved[6];
#else
	u32  reserved[9];
#endif
#endif
};

#pragma pack(pop)

#ifdef V1_6_1
#define AMBIENT_UPD_FRONT     (uint32_t)(1 << 0)
#define AMBIENT_UPD_REAR      (uint32_t)(1 << 1)
#define AMBIENT_UPD_WHITEXY   (uint32_t)(1 << 2)
#define AMBIENT_UPD_MODE      (uint32_t)(1 << 3)

struct ambient_cfg_s {
	u32 update_flag;
	u32 ambient; /* 1 << 16 */
	u32 tRearLum;
	u32 tFrontLux;
	u32 tWhiteX; /* 1 << 15 */
	u32 tWhiteY; /* 1 << 15 */
};

/*! @brief Ambient light configuration.*/
# pragma pack(push, 1)
struct AmbientCfg {
	u32 ambient;
	u32 tFrontLux;
	u32 tFrontLuxScale;
	u32 tRearLum;
	u32 tRearLumScale;
	u32 tWhitexy[2];
	u32 tSurroundReflection;
	u32 tScreenReflection;
	u32 alDelay;
	u32 alRise;
	u32 alFall;
};

#pragma pack(pop)

/*! @brief Adaptive Boost configuration.*/
# pragma pack(push, 1)
struct TgtABCfg {
	s32   abEnable;             /*Adaptive boost enabled */
	u32  abHighestTmax;        /*Adaptive boost highest Tmax */
	u32  abLowestTmax;         /*Adaptive boost lowest Tmax  */
	s16   abRiseWeight;         /*Back light rise weight signed Q3.12 */
	s16   abFallWeight;         /*Back light fall weight signed Q3.12 */
	u32  abDelayMilliSec_hdmi; /*Back light delay for HDMI case */
	u32  abDelayMilliSec_ott;  /*Back light delay for OTT case */
	u32  abDelayMilliSec_ll;   /*Back light delay for LL case */
	u32  reserved[3];
};

#pragma pack(pop)

# pragma pack(push, 1)
struct TargetDisplayConfig {
	u16 gamma;        /*Gamma */
	u16 eotf;         /*Target EOTF */
	u16 rangeSpec;    /*Target Range */
	u16 maxPq;        /*Target max luminance in PQ */
	u16 minPq;        /*Target min luminance in PQ */
	u16 maxPq_dm3;    /*Target max luminance for DM3 in PQ  */
	s32  min_lin;      /*Target min luminance in linear scale 2^18 */
	s32  max_lin;      /*Target max luminance in linear scale 2^18 */
	s32  max_lin_dm3;  /*Target max luminance for DM 3*/
	s32  tPrimaries[8];/*Target primaries */
	u16 mSWeight;  /*Multi-scale Weight,maybe overwritten by metadata*/
	s16  trimSlopeBias;  /*Trim Slope bias */
	s16  trimOffsetBias; /*Trim Offset bias */
	s16  trimPowerBias;      /*Trim Power bias */
	s16  msWeightBias;       /*Multi-scale weight bias */
	s16  chromaWeightBias;   /*Tone mapping chroma weight bias */
	s16  saturationGainBias; /*Tone mapping saturation gain bias */
	u16 tuningMode;         /*Tuning Mode */
	s16  brightness;         /*Brightness */
	s16  contrast;           /*Contrast */
	s16  dColorShift;        /*ColorShift */
	s16  dSaturation;        /*Saturation */
	s16  dBacklight;         /*Backlight */
	s16  dbgExecParamsPrintPeriod; /*Print reg values every n-th frame*/
	s16  dbgDmMdPrintPeriod;       /*Print DM metadata*/
	s16  dbgDmCfgPrintPeriod;      /*Print DM configuration*/
	struct TgtGDCfg  gdConfig;         /*Global Dimming configuration */
	struct TgtABCfg  abConfig;         /*Adaptive boost configuration */
	struct AmbientCfg ambientConfig;   /*Ambient light configuration */
	u8  vsvdb[7];
	u8  dm31_avail;
	u8  reference_mode_dark_id;
	u8  applyL11;
	u8  reserved1[1];
	s16  backlight_scaler;       /*Backlight Scaler */
	struct TgtOutCscCfg ocscConfig;  /*Output CSC configuration */
	s16  brightnessPreservation; /*Brightness preservation */
	u8  num_total_viewing_modes;/*Num of total viewing modes in cfg*/
	u8  viewing_mode_valid;     /*1 if the viewing mode is valid*/
	s16  padding[128];
};

#pragma pack(pop)

#else
/*! @defgroup general Enumerations and data structures*/
# pragma pack(push, 1)
struct TargetDisplayConfig {
	u16 gain;
	u16 offset;
	u16 gamma;         /**<@brief  Gamma */
	u16 eotf;
	u16 bitDepth;      /**<@brief  Bit Depth */
	u16 rangeSpec;
	u16 diagSize;      /**<@brief  Diagonal Size */
	u16 maxPq;
	u16 minPq;
	u16 mSWeight;
	u16 mSEdgeWeight;
	s16  minPQBias;
	s16  midPQBias;
	s16  maxPQBias;
	s16  trimSlopeBias;
	s16  trimOffsetBias;
	s16  trimPowerBias;
	s16  msWeightBias;
	s16  brightness;         /**<@brief  Brighness */
	s16  contrast;           /**<@brief  Contrast */
	s16  chromaWeightBias;
	s16  saturationGainBias;
	u16 chromaWeight;
	u16 saturationGain;
	u16 crossTalk;
	u16 tuningMode;
	s16  reserved0;
	s16  dbgExecParamsPrintPeriod;
	s16  dbgDmMdPrintPeriod;
	s16  dbgDmCfgPrintPeriod;
	u16 maxPq_dupli;
	u16 minPq_dupli;
	s32  keyWeight;
	s32  intensityVectorWeight;
	s32  chromaVectorWeight;
	s16  chip_fpga_lowcomplex;
	s16  midPQBiasLut[TUNING_LUT_ENTRIES];
	s16  saturationGainBiasLut[TUNING_LUT_ENTRIES];
	s16  chromaWeightBiasLut[TUNING_LUT_ENTRIES];
	s16  slopeBiasLut[TUNING_LUT_ENTRIES];
	s16  offsetBiasLut[TUNING_LUT_ENTRIES];
	s16  backlightBiasLut[TUNING_LUT_ENTRIES];
	struct TgtGDCfg gdConfig;
#ifdef V1_5
	u8  vsvdb[7];
	u8  reserved1[5];
#endif
	s32  min_lin;
	s32  max_lin;
	s16  backlight_scaler;
	s32  min_lin_dupli;
	s32  max_lin_dupli;
	struct TgtOutCscCfg ocscConfig;
#ifdef V1_5
	s16  reserved2;
#else
	s16  reserved00;
#endif
	s16  brightnessPreservation;
	s32  iintensityVectorWeight;
	s32  ichromaVectorWeight;
	s16  isaturationGainBias;
	s16  chip_12b_ocsc;
	s16  chip_512_tonecurve;
	s16  chip_nolvl5;
	s16  padding[8];
};

#pragma pack(pop)
#endif
/*! @brief PQ config main data structure.*/
struct pq_config_s {
#ifndef V1_6_1
	unsigned char default_gm_lut[GM_LUT_HDR_SIZE + GM_LUT_SIZE];
	unsigned char gd_gm_lut_min[GM_LUT_HDR_SIZE + GM_LUT_SIZE];
	unsigned char gd_gm_lut_max[GM_LUT_HDR_SIZE + GM_LUT_SIZE];
	unsigned char pq2gamma[sizeof(s32) * PQ2G_LUT_SIZE];
#endif
	unsigned char backlight_lut[BACLIGHT_LUT_SIZE];
	struct TargetDisplayConfig target_display_config;
};

enum input_mode_e {
	INPUT_MODE_OTT  = 0,
	INPUT_MODE_HDMI = 1
};

struct ui_menu_params_s {
	u16 u16BackLightUIVal;
	u16 u16BrightnessUIVal;
	u16 u16ContrastUIVal;
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
	u32 Composer_Mode;
	u32 VDR_Resolution;
	u32 Bit_Depth;
	u32 Coefficient_Log2_Denominator;
	u32 BL_Num_Pivots_Y;
	u32 BL_Pivot[5];
	u32 BL_Order;
	u32 BL_Coefficient_Y[8][3];
	u32 EL_NLQ_Offset_Y;
	u32 EL_Coefficient_Y[3];
	u32 Mapping_IDC_U;
	u32 BL_Num_Pivots_U;
	u32 BL_Pivot_U[3];
	u32 BL_Order_U;
	u32 BL_Coefficient_U[4][3];
	u32 MMR_Coefficient_U[22][2];
	u32 MMR_Order_U;
	u32 EL_NLQ_Offset_U;
	u32 EL_Coefficient_U[3];
	u32 Mapping_IDC_V;
	u32 BL_Num_Pivots_V;
	u32 BL_Pivot_V[3];
	u32 BL_Order_V;
	u32 BL_Coefficient_V[4][3];
	u32 MMR_Coefficient_V[22][2];
	u32 MMR_Order_V;
	u32 EL_NLQ_Offset_V;
	u32 EL_Coefficient_V[3];
};

/** @brief DM registers for IPCORE 1 */
struct dm_register_ipcore_1_s {
	u32 SRange;
	u32 Srange_Inverse;
	u32 Frame_Format_1;
	u32 Frame_Format_2;
	u32 Frame_Pixel_Def;
	u32 Y2RGB_Coefficient_1;
	u32 Y2RGB_Coefficient_2;
	u32 Y2RGB_Coefficient_3;
	u32 Y2RGB_Coefficient_4;
	u32 Y2RGB_Coefficient_5;
	u32 Y2RGB_Offset_1;
	u32 Y2RGB_Offset_2;
	u32 Y2RGB_Offset_3;
	u32 EOTF;
/*	u32 Sparam_1;*/
/*	u32 Sparam_2;*/
/*	u32 Sgamma; */
	u32 A2B_Coefficient_1;
	u32 A2B_Coefficient_2;
	u32 A2B_Coefficient_3;
	u32 A2B_Coefficient_4;
	u32 A2B_Coefficient_5;
	u32 C2D_Coefficient_1;
	u32 C2D_Coefficient_2;
	u32 C2D_Coefficient_3;
	u32 C2D_Coefficient_4;
	u32 C2D_Coefficient_5;
	u32 C2D_Offset;
	u32 Active_area_left_top;
	u32 Active_area_bottom_right;
};

/** @brief DM registers for IPCORE 2 */
struct dm_register_ipcore_2_s {
	u32 SRange;
	u32 Srange_Inverse;
	u32 Y2RGB_Coefficient_1;
	u32 Y2RGB_Coefficient_2;
	u32 Y2RGB_Coefficient_3;
	u32 Y2RGB_Coefficient_4;
	u32 Y2RGB_Coefficient_5;
	u32 Y2RGB_Offset_1;
	u32 Y2RGB_Offset_2;
	u32 Y2RGB_Offset_3;
	u32 Frame_Format;
	u32 EOTF;
	u32 A2B_Coefficient_1;
	u32 A2B_Coefficient_2;
	u32 A2B_Coefficient_3;
	u32 A2B_Coefficient_4;
	u32 A2B_Coefficient_5;
	u32 C2D_Coefficient_1;
	u32 C2D_Coefficient_2;
	u32 C2D_Coefficient_3;
	u32 C2D_Coefficient_4;
	u32 C2D_Coefficient_5;
	u32 C2D_Offset;
	u32 VDR_Resolution;
};

/** @brief DM registers for IPCORE 3 */
struct dm_register_ipcore_3_s {
	u32 D2C_coefficient_1;
	u32 D2C_coefficient_2;
	u32 D2C_coefficient_3;
	u32 D2C_coefficient_4;
	u32 D2C_coefficient_5;
	u32 B2A_Coefficient_1;
	u32 B2A_Coefficient_2;
	u32 B2A_Coefficient_3;
	u32 B2A_Coefficient_4;
	u32 B2A_Coefficient_5;
	u32 Eotf_param_1;
	u32 Eotf_param_2;
	u32 IPT_Scale;
	u32 IPT_Offset_1;
	u32 IPT_Offset_2;
	u32 IPT_Offset_3;
	u32 Output_range_1;
	u32 Output_range_2;
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
	u32 TmLutI[64 * 4];
	u32 TmLutS[64 * 4];
	u32 SmLutI[64 * 4];
	u32 SmLutS[64 * 4];
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

struct dv_cfg_info_s {
	int id;
	char pic_mode_name[32];
	s16  brightness;        /*Brightness */
	s16  contrast;          /*Contrast */
	s16  colorshift;        /*ColorShift or Tint*/
	s16  saturation;        /*Saturation or color */
	u8  vsvdb[7];
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
	s32 debug_buf[500];
};

#ifdef V1_6_1
#define PREFIX_SEI_NUT 39
#define SUFFIX_SEI_NUT 40
#define SEI_USER_DATA_REGISTERED_ITU_T_T35 4
#define SEI_MASTERING_DISPLAY_COLOUR_VOLUME 137

#define MAX_LENGTH_2086_SEI 256
#define MAX_LENGTH_2094_SEI 256

/* VUI parameters required to generate Dolby Vision metadata for ATSC 3.0*/
struct _dv_vui_param_s_ {
	s32  i_video_format;
	s32      b_video_full_range;
	s32      b_colour_description;
	s32  i_colour_primaries;
	s32  i_transfer_characteristics;
	s32  i_matrix_coefficients;
};

/* Data structure that combines all ATSC related parameters.*/
struct _dv_atsc_s_ {
	struct _dv_vui_param_s_  vui_param;
	u32 length_2086_sei;
	u8  payload_2086_sei[MAX_LENGTH_2086_SEI];
	u32 length_2094_sei;
	u8  payload_2094_sei[MAX_LENGTH_2094_SEI];
};
#endif
enum cpuID_e {
	_CPU_MAJOR_ID_GXM,
	_CPU_MAJOR_ID_TXLX,
	_CPU_MAJOR_ID_G12,
	_CPU_MAJOR_ID_TM2,
	_CPU_MAJOR_ID_TM2_REVB,
	_CPU_MAJOR_ID_SC2,
	_CPU_MAJOR_ID_T7,
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

int control_path(enum signal_format_e in_format,
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
	u16 backlight;
};

#ifdef V1_6_1
int tv_control_path(enum signal_format_e in_format,
	enum input_mode_e in_mode,
	char *in_comp, int in_comp_size,
	char *in_md, int in_md_size,
	int set_bit_depth, int set_chroma_format, int set_yuv_range,
	struct pq_config_s *pq_config,
	struct ui_menu_params_s *menu_param,
	int set_no_el,
	struct hdr10_param_s *hdr10_param,
	struct tv_dovi_setting_s *output,
	char *vsem_if, int vsem_if_size,
	struct ambient_cfg_s *ambient_cfg,
	struct tv_input_info_s *input_info);
#else
int tv_control_path(enum signal_format_e in_format,
	enum input_mode_e in_mode,
	char *in_comp, int in_comp_size,
	char *in_md, int in_md_size,
	int set_bit_depth, int set_chroma_format, int set_yuv_range,
	struct pq_config_s *pq_config,
	struct ui_menu_params_s *menu_param,
	int set_no_el,
	struct hdr10_param_s *hdr10_param,
	struct tv_dovi_setting_s *output);

#endif
void *metadata_parser_init(int flag);
int metadata_parser_reset(int flag);
int metadata_parser_process(char  *src_rpu, int rpu_len,
	char  *dst_comp, int *comp_len,
	char  *dst_md, int *md_len, bool src_eos);
void metadata_parser_release(void);

struct dolby_vision_func_s {
	const char *version_info;
	void * (*metadata_parser_init)(int flag);
	/*flag: bit0 flag, bit1 0->dv, 1->atsc*/
	int (*metadata_parser_reset)(int flag);
	int (*metadata_parser_process)(char  *src_rpu, int rpu_len,
		char  *dst_comp, int *comp_len,
		char  *dst_md, int *md_len, bool src_eos);
	void (*metadata_parser_release)(void);
	int (*control_path)(enum signal_format_e in_format,
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
#ifdef V1_6_1
	int (*tv_control_path)(enum signal_format_e in_format,
		enum input_mode_e in_mode,
		char *in_comp, int in_comp_size,
		char *in_md, int in_md_size,
		int set_bit_depth, int set_chroma_format, int set_yuv_range,
		struct pq_config_s *pq_config,
		struct ui_menu_params_s *menu_param,
		int set_no_el,
		struct hdr10_param_s *hdr10_param,
		struct tv_dovi_setting_s *output,
		char *vsem_if, int vsem_if_size,
		struct ambient_cfg_s *ambient_cfg,
		struct tv_input_info_s *input_info);
#else
	int (*tv_control_path)(enum signal_format_e in_format,
		enum input_mode_e in_mode,
		char *in_comp, int in_comp_size,
		char *in_md, int in_md_size,
		int set_bit_depth, int set_chroma_format, int set_yuv_range,
		struct pq_config_s *pq_config,
		struct ui_menu_params_s *menu_param,
		int set_no_el,
		struct hdr10_param_s *hdr10_param,
		struct tv_dovi_setting_s *output);
#endif
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
