/* SPDX-License-Identifier: ((GPL-2.0+ WITH Linux-syscall-note) OR BSD-3-Clause) */
/*
 *   TV LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 *   Copyright(c) 2018 by LG Electronics Inc.
 *
 *   All rights reserved. No part of this work may be reproduced, stored in a
 *   retrieval system, or transmitted by any means without prior written
 *   permission of LG Electronics Inc.
 */

#ifndef _VIDEODEV2_EXT_H
#define _VIDEODEV2_EXT_H

#include <linux/linuxtv-ext-ver.h>

/* FIXME: gstreamer-good uses uapi/linux/videodev2.h,
 * so add to check _UAPI_XX to prevent redefinition error
 */
#ifndef _UAPI__LINUX_VIDEODEV2_H
#include <linux/videodev2.h>
#endif

/* Common */
#define V4L2_EXT_DEV_NO_ATV 2
#define V4L2_EXT_DEV_NO_AV 10
#define V4L2_EXT_DEV_NO_ADC 11
#define V4L2_EXT_DEV_NO_HDMI 12
#define V4L2_EXT_DEV_NO_EARC 13
#define V4L2_EXT_DEV_NO_VDEC 20
#define V4L2_EXT_DEV_NO_VDOGAV 27

#define V4L2_EXT_DEV_NO_VDO0 28

#define V4L2_EXT_DEV_NO_VDO1 29
#define V4L2_EXT_DEV_NO_SCALER0 30
#define V4L2_EXT_DEV_NO_SCALER1 31
#define V4L2_EXT_DEV_NO_SCALER2 32
#define V4L2_EXT_DEV_NO_SCALER3 33
#define V4L2_EXT_DEV_NO_BACKEND 40
#define V4L2_EXT_DEV_NO_PQ 50
#define V4L2_EXT_DEV_NO_CAPTURE 60
#define V4L2_EXT_DEV_NO_GPSCALER 70

/* VBI */
#define V4L2_EXT_DEV_NO_VBI 1

#define V4L2_EXT_DEV_PATH_ATV "/dev/video2"
#define V4L2_EXT_DEV_PATH_AV "/dev/video10"
#define V4L2_EXT_DEV_PATH_ADC "/dev/video11"
#define V4L2_EXT_DEV_PATH_HDMI "/dev/video12"
#define V4L2_EXT_DEV_PATH_VDEC "/dev/video20"
#define V4L2_EXT_DEV_PATH_VDOGAV "/dev/video27"

#define V4L2_EXT_DEV_PATH_VDO0 "/dev/video28"

#define V4L2_EXT_DEV_PATH_VDO1 "/dev/video29"
#define V4L2_EXT_DEV_PATH_SCALER0 "/dev/video30"
#define V4L2_EXT_DEV_PATH_SCALER1 "/dev/video31"
#define V4L2_EXT_DEV_PATH_SCALER2 "/dev/video32"
#define V4L2_EXT_DEV_PATH_SCALER3 "/dev/video33"
#define V4L2_EXT_DEV_PATH_BACKEND "/dev/video40"
#define V4L2_EXT_DEV_PATH_PQ "/dev/video50"
#define V4L2_EXT_DEV_PATH_CAPTURE "/dev/video60"
#define V4L2_EXT_DEV_PATH_GPSCALER "/dev/video70"
#define V4L2_EXT_DEV_PATH_VBI "/dev/vbi1"
#define V4L2_EXT_DEV_PATH_EARC "/dev/earc0"

#define V4L2_EXT_LED_SPI_LDIM_EN 0x01	// bit0 enable local dimming
#define V4L2_EXT_LED_SPI_BPL_BIT1 0x02	// bit1 bpl strength
#define V4L2_EXT_LED_SPI_STOREMODE_EN 0x04 // bit2 enable led store mode
#define V4L2_EXT_LED_SPI_HALO_BIT1	0x08	// bit3 halo reduction strength
#define V4L2_EXT_LED_SPI_MOTIONPRO_EN 0x10 // bit4 enable motion pro
#define V4L2_EXT_LED_SPI_HALO_BIT2 0x20 // bit5	halo reduction strength
#define V4L2_EXT_LED_SPI_BPL_BIT2 0x40 // bit6	bpl strength
#define V4L2_EXT_LED_SPI_REVERSE 0x80  // bit7	screen reverse

#define V4L2_EXT_VPQ_BYPASS_MASK_NONE 0x00000000
#define V4L2_EXT_VPQ_BYPASS_MASK_SHARP_ENHANCE \
	0x00000001 // BIT0: //SharpnessEnhancer
#define V4L2_EXT_VPQ_BYPASS_MASK_OBJECT_CONTRAST \
	0x00000002 // BIT1: //ObjectContrast
#define V4L2_EXT_VPQ_BYPASS_MASK_CONTRAST_COLOR_ENHANCE \
	0x00000004 // BIT2: //Contrast, Local Contrast, Color Enhancer
#define V4L2_EXT_VPQ_BYPASS_MASK_GAMMA_LOCALDIMMING \
	0x00000008 // BIT3: //Localdimming, Gamma UI
#define V4L2_EXT_VPQ_BYPASS_MASK_NEAR_BE \
	0x00000010 // BIT4: //WB,DGA-4CH,POD,PCID,ODC
#define V4L2_EXT_VPQ_BYPASS_MASK_HDR_ALL 0x00000020 // BIT5: HDR
 #define V4L2_EXT_VPQ_BYPASS_MASK_HDR_EXCEPT_PCC \
	0x00000040 // BIT6: //HDR bypass,HDR-PCC enable

/* Scaler */
struct v4l2_ext_video_rect {
	unsigned short x;
	unsigned short y;
	unsigned short w;
	unsigned short h;
};

enum v4l2_ext_vsc_input_src {
	V4L2_EXT_VSC_INPUT_SRC_NONE = 0,
	V4L2_EXT_VSC_INPUT_SRC_AVD,
	V4L2_EXT_VSC_INPUT_SRC_ADC,
	V4L2_EXT_VSC_INPUT_SRC_HDMI,
	V4L2_EXT_VSC_INPUT_SRC_VDEC,
	V4L2_EXT_VSC_INPUT_SRC_JPEG,
	V4L2_EXT_VSC_INPUT_SRC_FB
};

enum v4l2_ext_vsc_dest {
	V4L2_EXT_VSC_DEST_NONE = 0,
	V4L2_EXT_VSC_DEST_DISPLAY,
	V4L2_EXT_VSC_DEST_VENC,
	V4L2_EXT_VSC_DEST_MEMORY,
	V4L2_EXT_VSC_DEST_AVE
};

struct v4l2_ext_vsc_input_src_info {
	enum v4l2_ext_vsc_input_src src;
	unsigned char index;
	unsigned char attr;
};

struct v4l2_ext_vsc_connect_info {
	struct v4l2_ext_vsc_input_src_info in;
	enum v4l2_ext_vsc_dest out;
};

enum v4l2_ext_vsc_vdo_port {
	V4L2_EXT_VSC_VDO_PORT_NONE = 0,
	V4L2_EXT_VSC_VDO_PORT_0,
	V4L2_EXT_VSC_VDO_PORT_1
};

struct v4l2_ext_vsc_vdo_mode {
	enum v4l2_ext_vsc_vdo_port vdo_port;
	unsigned short vdec_port;
};

enum v4l2_ext_vsc_win_color {
	V4L2_EXT_VSC_WIN_COLOR_NORMAL = 0,
	V4L2_EXT_VSC_WIN_COLOR_BLACK,
	V4L2_EXT_VSC_WIN_COLOR_BLUE,
	V4L2_EXT_VSC_WIN_COLOR_GRAY
};

enum v4l2_ext_vsc_rotation {
	V4L2_EXT_VSC_ROTATE_0 = 0,
	V4L2_EXT_VSC_ROTATE_90,
	V4L2_EXT_VSC_ROTATE_180,
	V4L2_EXT_VSC_ROTATE_270
};

struct v4l2_ext_vsc_input_region {
	struct v4l2_ext_video_rect crop;
	struct v4l2_ext_video_rect res;
};

struct v4l2_ext_vsc_win_region {
	struct v4l2_ext_vsc_input_region in;
	struct v4l2_ext_video_rect out;
	enum v4l2_ext_vsc_rotation rotation;
};

struct v4l2_ext_vsc_orbit_window {
	struct v4l2_ext_video_rect osd_input;
	struct v4l2_ext_video_rect video_input;
	struct v4l2_ext_video_rect osd_output;
	struct v4l2_ext_video_rect video_output;
};

struct v4l2_ext_vsc_orbit_move {
	struct v4l2_ext_video_rect osd_input;
	struct v4l2_ext_video_rect video_input;
	struct v4l2_ext_video_rect osd_output;
	struct v4l2_ext_video_rect video_output;
};

enum v4l2_ext_vsc_win_mode {
	V4L2_EXT_VSC_WIN_MODE_NONE = 0,
	V4L2_EXT_VSC_WIN_MODE_PIP,
	V4L2_EXT_VSC_WIN_MODE_PBP
};

enum v4l2_ext_vsc_mirror_mode {
	V4L2_EXT_VSC_MIRROR_MODE_NONE = 0,
	V4L2_EXT_VSC_MIRROR_MODE_ON,
	V4L2_EXT_VSC_MIRROR_MODE_OFF
};

enum v4l2_ext_vsc_memory_type {
	V4L2_EXT_VSC_MEMORY_TYPE_NONE = 0,
	V4L2_EXT_VSC_MEMORY_TYPE_SINGLE,
	V4L2_EXT_VSC_MEMORY_TYPE_MULTI
};

struct v4l2_ext_vsc_win_prop {
	enum v4l2_ext_vsc_win_mode win_mode;
	enum v4l2_ext_vsc_mirror_mode mirror_mode;
	enum v4l2_ext_vsc_memory_type mem_type;
};

struct v4l2_ext_vsc_zorder {
	unsigned char zorder;
	unsigned char alpha;
};

struct v4l2_ext_vsc_scaler_ratio {
	int h_scaleup_ratio;
	int v_scaleup_ratio;
	int h_scaledown_ratio;
	int v_scaledown_ratio;
};

enum v4l2_ext_vsc_latency_pattern {
	V4L2_EXT_VSC_PATTERN_BLACK = 0,
	V4L2_EXT_VSC_PATTERN_WHITE
};

struct v4l2_ext_vsc_latency_pattern_info {
	struct v4l2_ext_video_rect r;
	enum v4l2_ext_vsc_latency_pattern p;
};

struct v4l2_ext_vsc_active_win_info {
	struct v4l2_ext_video_rect original;
	struct v4l2_ext_video_rect active;
};

struct v4l2_ext_vsc_color_pixel_data {
	unsigned int y;  /* standard pixel color Y */
	unsigned int cb; /* standard pixel color Cb */
	unsigned int cr; /* standard pixel color Cr */
};

enum v4l2_ext_vsc_color_pixel_format {
	V4L2_EXT_VSC_COLOR_PIXEL_FORMAT_YUV = 0,
	V4L2_EXT_VSC_COLOR_PIXEL_FORMAT_RGB
};

enum v4l2_ext_vsc_color_pixel_depth {
	V4L2_EXT_VSC_COLOR_PIXEL_8BIT = 0,
	V4L2_EXT_VSC_COLOR_PIXEL_10BIT
};

struct v4l2_ext_vsc_pixel_color_info {
	struct v4l2_ext_video_rect r;
	union {
		struct v4l2_ext_vsc_color_pixel_data *p_data;
		unsigned int compat_data;
		unsigned long long sizer;
	};
	enum v4l2_ext_vsc_color_pixel_format format;
	enum v4l2_ext_vsc_color_pixel_depth depth;
};

/* PQ */
// histogram bin, chroma bin num
#define V4L2_EXT_VPQ_BIN_NUM 64
#define V4L2_EXT_VPQ_C_BIN_NUM 32
#define V4L2_EXT_VPQ_H_BIN_NUM 32
#define V4L2_EXT_LED_SMART_ADJ_ON (0x10)

// PQ COMMON DATA TYPE
struct v4l2_ext_vpq_cmn_data {
	unsigned int version;
	unsigned int length;
	unsigned char wid;
	union {
		unsigned char *p_data;
		unsigned int compat_data;
		unsigned long long sizer;
	};
};

//  LED Type Definition
enum v4l2_ext_led_ui_adj_type {
	V4L2_EXT_LED_UI_ADJ_OFF		= 0x00,
	V4L2_EXT_LED_UI_ADJ_SDR_LOW	= 0x01,
	V4L2_EXT_LED_UI_ADJ_SDR_MEDIUM = 0x02,
	V4L2_EXT_LED_UI_ADJ_SDR_HIGH   = 0x03,
	V4L2_EXT_LED_UI_ADJ_HDR_LOW	= 0x04,
	V4L2_EXT_LED_UI_ADJ_HDR_MEDIUM = 0x05,
	V4L2_EXT_LED_UI_ADJ_HDR_HIGH   = 0x06,
	V4L2_EXT_LED_UI_ADJ_MAX
};

enum v4l2_ext_led_localdimming_demo_type {
	V4L2_EXT_LED_ONOFF = 0,   // each led block flicker sequentially.
	V4L2_EXT_LED_H_ONOFF_EXT, // led horizontal moving box pattern on for
							  // external localdimming chip.
	V4L2_EXT_LED_V_ONOFF_EXT, // led vertical moving box pattern on for external
							  // localdimming chip.
	V4L2_EXT_LED_SPLIT_SCREEN, // half of screen is made from white pattern, and
							   // show local dimming effect.
	V4L2_EXT_LED_DEMOTYPE_SIZE_MAX
};

enum v4l2_ext_led_backlight_type {
	V4L2_EXT_LED_BACKLIGHT_DIRECT_L = 0,
	V4L2_EXT_LED_BACKLIGHT_EDGE_LED,
	V4L2_EXT_LED_BACKLIGHT_OLED,
	V4L2_EXT_LED_BACKLIGHT_DIRECT_VI,
	V4L2_EXT_LED_BACKLIGHT_DIRECT_SKY,
	V4L2_EXT_LED_BACKLIGHT_MINI_LED,
	V4L2_EXT_LED_BACKLIGHT_END,
};

enum v4l2_ext_led_panel_inch_type {
	V4L2_EXT_LED_INCH_32 = 0,
	V4L2_EXT_LED_INCH_39,
	V4L2_EXT_LED_INCH_42,
	V4L2_EXT_LED_INCH_47,
	V4L2_EXT_LED_INCH_49,
	V4L2_EXT_LED_INCH_50,
	V4L2_EXT_LED_INCH_55,
	V4L2_EXT_LED_INCH_58,
	V4L2_EXT_LED_INCH_60,
	V4L2_EXT_LED_INCH_65,
	V4L2_EXT_LED_INCH_70,
	V4L2_EXT_LED_INCH_77,
	V4L2_EXT_LED_INCH_79,
	V4L2_EXT_LED_INCH_84,
	V4L2_EXT_LED_INCH_98,
	V4L2_EXT_LED_INCH_105, // TV model
	V4L2_EXT_LED_INCH_23,
	V4L2_EXT_LED_INCH_24,
	V4L2_EXT_LED_INCH_26,
	V4L2_EXT_LED_INCH_27, // Smart Monitor TV
	V4L2_EXT_LED_INCH_22,
	V4L2_EXT_LED_INCH_28,
	V4L2_EXT_LED_INCH_40,
	V4L2_EXT_LED_INCH_43,
	V4L2_EXT_LED_INCH_86,
	V4L2_EXT_LED_INCH_BASE
};

enum v4l2_ext_led_bar_type {
	V4L2_EXT_LED_BAR_6 = 0,
	V4L2_EXT_LED_BAR_12,
	V4L2_EXT_LED_BAR_32,
	V4L2_EXT_LED_BAR_36,
	V4L2_EXT_LED_BAR_40,
	V4L2_EXT_LED_BAR_48,
	V4L2_EXT_LED_BAR_50,
	V4L2_EXT_LED_BAR_90,
	V4L2_EXT_LED_BAR_96,
	V4L2_EXT_LED_BAR_120,
	V4L2_EXT_LED_BAR_60,
	V4L2_EXT_LED_BAR_144,
	V4L2_EXT_LED_BAR_108,
	V4L2_EXT_LED_BAR_160,
	V4L2_EXT_LED_BAR_720,
	V4L2_EXT_LED_BAR_960,
	V4L2_EXT_LED_BAR_1440,
	V4L2_EXT_LED_BAR_1800,
	V4L2_EXT_LED_BAR_1920,
	V4L2_EXT_LED_BAR_2400,
	V4L2_EXT_LED_BAR_1200,
	V4L2_EXT_LED_BAR_288,
	V4L2_EXT_LED_BAR_216,
	V4L2_EXT_LED_BAR_180,
	V4L2_EXT_LED_BAR_MAX,
	V4L2_EXT_LED_BAR_DEFALT = V4L2_EXT_LED_BAR_MAX,
};

enum v4l2_ext_led_module_maker_type {
	V4L2_EXT_LED_MODULE_LGD = 0,
	V4L2_EXT_LED_MODULE_CMI,
	V4L2_EXT_LED_MODULE_AUO,
	V4L2_EXT_LED_MODULE_SHARP,
	V4L2_EXT_LED_MODULE_IPS,
	V4L2_EXT_LED_MODULE_BOE,
	V4L2_EXT_LED_MODULE_CSOT,
	V4L2_EXT_LED_MODULE_INNOLUX,
	V4L2_EXT_LED_MODULE_LCD_END,
	V4L2_EXT_LED_MODULE_LGE = V4L2_EXT_LED_MODULE_LCD_END,
	V4L2_EXT_LED_MODULE_PANASONIC,
	V4L2_EXT_LED_MODULE_PDP_END,
	V4L2_EXT_LED_MODULE_BASE = V4L2_EXT_LED_MODULE_PDP_END,
};

enum v4l2_ext_led_lut_number_type {
	V4L2_EXT_LED_LUT_FRIST = 0,
	V4L2_EXT_LED_LUT_SECOND,
	V4L2_EXT_LED_LUT_THIRD,
	V4L2_EXT_LED_LUT_MAX
};

enum v4l2_ext_led_ldim_ic {
	V4L2_EXT_LED_LDIM_NONE	 = 0, // Not support Local dimming.
	V4L2_EXT_LED_LDIM_INTERNAL = 1, // Use internal Local dimming block.
	V4L2_EXT_LED_LDIM_EXTERNAL = 2, // Use external Local dimming IC.
};

enum v4l2_ext_led_wcg_panel_type {
	V4L2_EXT_LED_WCG_PANEL_LED		  = 0,
	V4L2_EXT_LED_WCG_PANEL_LED_ULTRAHD  = 1,
	V4L2_EXT_LED_WCG_PANEL_OLED		 = 2,
	V4L2_EXT_LED_WCG_PANEL_OLED_ULTRAHD = 3
};

struct v4l2_ext_led_panel_info {
	enum v4l2_ext_led_panel_inch_type panel_inch; // panel size   ex) 47, 55
	enum v4l2_ext_led_backlight_type backlight_type; // led backlight type  ex) alef, edge
	enum v4l2_ext_led_bar_type bar_type; // led bar type   ex) h6,h12, v12
	enum v4l2_ext_led_module_maker_type module_maker; // panel maker   ex) lgd, auo
	enum v4l2_ext_led_ldim_ic local_dim_ic_type; // localdimming control type
	// ex) internal localdiming
	// block
	enum v4l2_ext_led_wcg_panel_type panel_type;
};

enum v4l2_ext_led_ldim_demo_type {
	v4l2_ext_led_ldim_demo_type_linedemo  = 0,
	v4l2_ext_led_ldim_demo_type_leftright = 1,
	v4l2_ext_led_ldim_demo_type_topbottom = 2,
	v4l2_ext_led_ldim_demo_type_max
};

struct v4l2_ext_led_ldim_demo_info {
	enum v4l2_ext_led_ldim_demo_type eType;
	unsigned char bOnOff;
};

struct v4l2_ext_led_apl_info {
	unsigned short block_apl_min;
	unsigned short block_apl_max;
};

struct v4l2_ext_led_spi_ctrl_info {
	unsigned char bitMask; // see V4L2_EXT_LED_SPI_XXX for reference
	unsigned int ctrlValue;
};

struct v4l2_ext_led_bpl_info {
	unsigned short ai_brightness;
	unsigned short sensor_level;
};

// MEMC Type Definition
enum v4l2_ext_memc_type_old {
	V4L2_EXT_MEMC_TYPE_OFF = 0,
	V4L2_EXT_MEMC_TYPE_LOW,
	V4L2_EXT_MEMC_TYPE_HIGH,
	V4L2_EXT_MEMC_TYPE_USER,
	V4L2_EXT_MEMC_TYPE_55_PULLDOWN,
	V4L2_EXT_MEMC_TYPE_MEDIUM
};

enum v4l2_ext_memc_type {
	V4L2_EXT_MEMC_OFF		  = V4L2_EXT_MEMC_TYPE_OFF,
	V4L2_EXT_MEMC_CINEMA_CLEAR = V4L2_EXT_MEMC_TYPE_MEDIUM,
	V4L2_EXT_MEMC_NATURAL	  = V4L2_EXT_MEMC_TYPE_LOW,
	V4L2_EXT_MEMC_SMOOTH	   = V4L2_EXT_MEMC_TYPE_HIGH, //
	V4L2_EXT_MEMC_USER		 = V4L2_EXT_MEMC_TYPE_USER,
	V4L2_EXT_MEMC_PULLDOWN_55  = V4L2_EXT_MEMC_TYPE_55_PULLDOWN
};

struct v4l2_ext_memc_motion_comp_info {
	unsigned char blur_level;
	unsigned char judder_level;
	enum v4l2_ext_memc_type memc_type;
};

// VPQ Type Definition
enum v4l2_ext_vpq_rgb_index {
	V4L2_EXT_VPQ_RED	 = 0,
	V4L2_EXT_VPQ_GREEN   = 1,
	V4L2_EXT_VPQ_BLUE	= 2,
	V4L2_EXT_VPQ_RGB_MAX = 3
};

#define V4L2_EXT_HDR_PICINFO_SIZE 128

// HDR Type Definition
struct v4l2_ext_hdr_color_correction {
	unsigned int hue_blend;
	unsigned int sat_blend;

	unsigned int l_gain_x[8]; // L gain LUT x point
	unsigned int l_gain_y[8]; // L gain LUT y point

	unsigned int s_gain_x[8]; // S gain LUT x point
	unsigned int s_gain_y[8]; // S gain LUT y point
};

enum v4l2_ext_hdr_mode {
	V4L2_EXT_HDR_MODE_SDR,
	V4L2_EXT_HDR_MODE_DOLBY,
	V4L2_EXT_HDR_MODE_HDR10,
	V4L2_EXT_HDR_MODE_HLG,
	V4L2_EXT_HDR_MODE_TECHNICOLOR,
	V4L2_EXT_HDR_MODE_HDREFFECT,
	V4L2_EXT_HDR_MODE_MAX
};

enum v4l2_ext_colorimetry_info {
	V4L2_EXT_COLORIMETRY_BT601,
	V4L2_EXT_COLORIMETRY_BT709,
	V4L2_EXT_COLORIMETRY_BT2020,
	V4L2_EXT_COLORIMETRY_MAX
};

struct v4l2_ext_pq_mode_info {
	unsigned int hdrStatus;	 // see v4l2_ext_hdr_mode
	unsigned int colorimetry;   // see v4l2_ext_colorimetry_info
	unsigned int peakLuminance; // peakLuminance of panel
	unsigned int supportPrime;  // support Prime
	unsigned int reserved;
};

struct v4l2_ext_hdr_tonemap {
	enum v4l2_ext_hdr_mode hdr_mode;
	unsigned int r_data[66];
	unsigned int g_data[66];
	unsigned int b_data[66];
};

struct v4l2_ext_hdr_3dlut {
	enum v4l2_ext_hdr_mode hdr_mode;
	unsigned int data_size;
	union {
		unsigned int *p3dlut;
		unsigned int compat_data;
		unsigned long long sizer;
	};
};

enum v4l2_ext_dolby_config_type {
	V4L2_EXT_DOLBY_CONFIG_MAIN = 0,
	V4L2_EXT_DOLBY_CONFIG_BEST
};

struct v4l2_ext_dolby_config_path {
	enum v4l2_ext_dolby_config_type eConfigType;
	char sConfigPath[255];
};

struct v4l2_ext_dolby_picture_mode {
	unsigned int bOnOff;
	unsigned int uPictureMode;
};

enum v4l2_ext_dolby_picture_menu {
	V4L2_EXT_DOLBY_BACKLIGHT = 0,
	V4L2_EXT_DOLBY_BRIGHTNESS,
	V4L2_EXT_DOLBY_COLOR,
	V4L2_EXT_DOLBY_CONTRAST,
	V4L2_EXT_DOLBY_PICTURE_MENU_MAX
};

struct v4l2_ext_dolby_picture_data {
	enum v4l2_ext_dolby_picture_menu picture_menu;
	unsigned int on_off;
	int setting_value;
};

struct v4l2_ext_dolby_gd_delay {
	unsigned short ott_24;
	unsigned short ott_30;
	unsigned short ott_50;
	unsigned short ott_60;
	unsigned short ott_100;
	unsigned short ott_120;

	unsigned short hdmi_24;
	unsigned short hdmi_30;
	unsigned short hdmi_50;
	unsigned short hdmi_60;
	unsigned short hdmi_100;
	unsigned short hdmi_120;
};

struct v4l2_ext_dolby_gd_delay_lut {
	struct v4l2_ext_dolby_gd_delay standard_frc_off;
	struct v4l2_ext_dolby_gd_delay standard_frc_on;

	struct v4l2_ext_dolby_gd_delay vivid_frc_off;
	struct v4l2_ext_dolby_gd_delay vivid__frc_on;

	struct v4l2_ext_dolby_gd_delay cinema_home_frc_off;
	struct v4l2_ext_dolby_gd_delay cinema_home_frc_on;

	struct v4l2_ext_dolby_gd_delay cinema_frc_off;
	struct v4l2_ext_dolby_gd_delay cinema_frc_on;

	struct v4l2_ext_dolby_gd_delay game_frc_off;
	struct v4l2_ext_dolby_gd_delay game_frc_on;
};

struct v4l2_ext_dolby_gd_delay_param {
	struct v4l2_ext_dolby_gd_delay_lut dolby_GD_standard;
	struct v4l2_ext_dolby_gd_delay_lut dolby_GD_latency;
};

struct v4l2_ext_dolby_ambient_light_param {
	unsigned int onoff;
	unsigned int luxdata;
	union {
		unsigned int *rawdata;
		unsigned int compat_data;
		unsigned long long sizer;
	};
};

// VPQ Type Definition
enum v4l2_ext_vpq_inner_pattern_ire {
	V4L2_EXT_VPQ_INNER_PATTERN_IRE_0 = 0,
	V4L2_EXT_VPQ_INNER_PATTERN_IRE_2DOT5,
	V4L2_EXT_VPQ_INNER_PATTERN_IRE_5,
	V4L2_EXT_VPQ_INNER_PATTERN_IRE_7DOT5,
	V4L2_EXT_VPQ_INNER_PATTERN_IRE_10,
	V4L2_EXT_VPQ_INNER_PATTERN_IRE_15,
	V4L2_EXT_VPQ_INNER_PATTERN_IRE_20,
	V4L2_EXT_VPQ_INNER_PATTERN_IRE_25,
	V4L2_EXT_VPQ_INNER_PATTERN_IRE_30,
	V4L2_EXT_VPQ_INNER_PATTERN_IRE_35,
	V4L2_EXT_VPQ_INNER_PATTERN_IRE_40,
	V4L2_EXT_VPQ_INNER_PATTERN_IRE_45,
	V4L2_EXT_VPQ_INNER_PATTERN_IRE_50,
	V4L2_EXT_VPQ_INNER_PATTERN_IRE_55,
	V4L2_EXT_VPQ_INNER_PATTERN_IRE_60,
	V4L2_EXT_VPQ_INNER_PATTERN_IRE_65,
	V4L2_EXT_VPQ_INNER_PATTERN_IRE_70,
	V4L2_EXT_VPQ_INNER_PATTERN_IRE_75,
	V4L2_EXT_VPQ_INNER_PATTERN_IRE_80,
	V4L2_EXT_VPQ_INNER_PATTERN_IRE_85,
	V4L2_EXT_VPQ_INNER_PATTERN_IRE_90,
	V4L2_EXT_VPQ_INNER_PATTERN_IRE_95,
	V4L2_EXT_VPQ_INNER_PATTERN_IRE_100,
	V4L2_EXT_VPQ_INNER_PATTERN_DISABLE
};

enum v4l2_ext_vpq_input {
	V4L2_EXT_VPQ_INPUT_ATV = 0,
	V4L2_EXT_VPQ_INPUT_AV,
	V4L2_EXT_VPQ_INPUT_SCARTRGB,
	V4L2_EXT_VPQ_INPUT_COMP,
	V4L2_EXT_VPQ_INPUT_RGB_PC,
	V4L2_EXT_VPQ_INPUT_HDMI_TV,
	V4L2_EXT_VPQ_INPUT_HDMI_PC,
	V4L2_EXT_VPQ_INPUT_DTV,
	V4L2_EXT_VPQ_INPUT_PICWIZ,
	V4L2_EXT_VPQ_INPUT_PICTEST,
	V4L2_EXT_VPQ_INPUT_MEDIA_MOVIE,
	V4L2_EXT_VPQ_INPUT_MEDIA_PHOTO,
	V4L2_EXT_VPQ_INPUT_CAMERA,
	V4L2_EXT_VPQ_INPUT_PVR_DTV,
	V4L2_EXT_VPQ_INPUT_PVR_ATV,
	V4L2_EXT_VPQ_INPUT_PVR_AV,
	V4L2_EXT_VPQ_INPUT_MAX
};

struct v4l2_ext_vpq_dc2p_histodata_info {
	unsigned int apl;
	signed int min;
	signed int max;
	signed int peak_low;
	signed int peak_high;
	unsigned int skin_count;
	unsigned int sat_status;
	unsigned int diff_sum;
	unsigned int motion;
	unsigned int texture;
	unsigned int bin[V4L2_EXT_VPQ_BIN_NUM];
	unsigned int chrm_bin[V4L2_EXT_VPQ_C_BIN_NUM];
	unsigned int hue_bin[V4L2_EXT_VPQ_H_BIN_NUM];
};

struct v4l2_ext_vpq_color_temp {
	unsigned short rgb_gain[3];
	unsigned short rgb_offset[3];
};

struct v4l2_ext_vpq_gamut_add_info {
	unsigned int info_size;
	union {
		unsigned int *p_info_data;
		unsigned int compat_data;
		unsigned long long sizer;
	};
};

struct v4l2_ext_vpq_gamut_lut {
	unsigned int lut_version;
	unsigned int total_section_num;
	union {
		unsigned short *p_section_data;
		unsigned int compat_data;
		unsigned long long sizer;
	};
	struct v4l2_ext_vpq_gamut_add_info add_info;
};

struct v4l2_ext_oled_luminance_boost {
	unsigned int length;
	union {
		unsigned int *p_data;
		unsigned int compat_data;
		unsigned long long sizer;
	};
};

struct v4l2_ext_vpq_block_bypass {
	unsigned int bypassMask; // see V4L2_EXT_VPQ_BYPASS_MASK_XXX
	unsigned char bOnOff;
};

struct v4l2_ext_vpq_od {
	unsigned int length;
	union {
		unsigned char *p_data;
		unsigned int compat_data;
		unsigned long long sizer;
	};
};

struct v4l2_ext_vpq_od_extension {
	unsigned char ext_type;
	unsigned int ext_length;
	union {
		unsigned char *p_ext_data;
		unsigned int compat_data;
		unsigned long long sizer;
	};
};

struct v4l2_ext_gamma_lut {
	unsigned int table_num; // number of table elements
	union {
		unsigned int *table_red;
		unsigned int compat_table_red;
		unsigned long long sizer_r;
	};
	union {
		unsigned int *table_green;
		unsigned int compat_table_green;
		unsigned long long sizer_g;
	};
	union {
		unsigned int *table_blue;
		unsigned int compat_table_blue;
		unsigned long long sizer_b;
	};
};

#define CSC_MUX_LUT_SIZE 4
struct v4l2_ext_csc_mux_lut {
	unsigned int mux_l3d_in;
	unsigned int mux_blend_in;
	unsigned int mux_4p_lut_in;
	unsigned int mux_oetf_out;
	unsigned int b4p_lut_x[CSC_MUX_LUT_SIZE];
	unsigned int b4p_lut_y[CSC_MUX_LUT_SIZE];
};

struct v4l2_ext_gamut_post {
	unsigned char gamma;
	unsigned char degamma;
	short matrix[9];
	struct v4l2_ext_csc_mux_lut mux_blend;
	union {
		unsigned char *pst_chip_data;
		unsigned int compat_data;
		unsigned long long sizer;
	};
};

enum v4l2_ext_cm_dynamic_color_level {
	V4L2_EXT_CM_DYNAMIC_COLOR_OFF	 = 0,
	V4L2_EXT_CM_DYNAMIC_COLOR_LOW	 = 1,
	V4L2_EXT_CM_DYNAMIC_COLOR_MEDIUM  = 2,
	V4L2_EXT_CM_DYNAMIC_COLOR_HIGH	= 3,
	V4L2_EXT_CM_DYNAMIC_COLOR_INVALID = 4
};

enum v4l2_ext_cm_perferred_color_type {
	V4L2_EXT_CM_PREFERRED_COLOR_SKIN	= 0,
	V4L2_EXT_CM_PREFERRED_COLOR_GRASS   = 1,
	V4L2_EXT_CM_PREFERRED_COLOR_SKYBLUE = 2,
	V4L2_EXT_CM_PREFERRED_COLOR_MAX
};

enum v4l2_ext_cm_cms_color_type { // Color Management System
	V4L2_EXT_CM_CMS_RED	 = 0,
	V4L2_EXT_CM_CMS_GREEN   = 1,
	V4L2_EXT_CM_CMS_BLUE	= 2,
	V4L2_EXT_CM_CMS_CYAN	= 3,
	V4L2_EXT_CM_CMS_MAGENTA = 4,
	V4L2_EXT_CM_CMS_YELLOW  = 5,
	V4L2_EXT_CM_CMS_MAX
};

struct v4l2_ext_cm_dynamic_color_ui {
	unsigned char enable; // 0: don't reference preferred_value(expert_control),
	// 1: reference preferred_value(not_use_cm_db==1 &&
	// advanced_control)
	enum v4l2_ext_cm_dynamic_color_level value; // ui value
};

struct v4l2_ext_cm_perferred_color_ui {
	unsigned char enable; // 0: don't reference preferred_value(expert_control),
						  // 1: reference preferred_value(advanced_control)
	signed char value[V4L2_EXT_CM_PREFERRED_COLOR_MAX]; // ui value per each color_type
};

struct v4l2_ext_cm_cms_ui { // Color Management System
	unsigned char enable;   // 0: don't reference value(advanced_control),  1:
	// reference value(not_use_cm_db==1 && expert_control)
	signed char gain_saturation[V4L2_EXT_CM_CMS_MAX];  // ui value per each color_type
	signed char gain_hue[V4L2_EXT_CM_CMS_MAX]; // ui value per each color_type
	signed char gain_luminance[V4L2_EXT_CM_CMS_MAX]; // ui value per each color_type
};

struct v4l2_ext_cm_ui_status {
	struct v4l2_ext_cm_dynamic_color_ui dynamic;
	struct v4l2_ext_cm_perferred_color_ui preferred;
	struct v4l2_ext_cm_cms_ui cms;
};

struct v4l2_ext_cm_info {
	unsigned char
		use_internal_cm_db; // 0: use dbInfo, 1: use driver internal db
	struct v4l2_ext_cm_ui_status uiInfo;
	union {
		unsigned char *dbinfo;
		unsigned int compat_data;
		unsigned long long sizer;
	};
};

/* dynamic contrast */
struct v4l2_ext_dynamnic_contrast_ctrl {
	unsigned short uDcVal;
	union {
		unsigned char *pst_chip_data;
		unsigned int compat_data;
		unsigned long long sizer;
	};
};

#define MAX_DYNAMIC_CONTRAST_LUT_SIZE 16
#define MAX_DYNAMIC_CONTRAST_SATURATION_LUT_SIZE 10
struct v4l2_ext_dynamnic_contrast_lut {
	signed int sLumaLutY[MAX_DYNAMIC_CONTRAST_LUT_SIZE];
	/* if support luma LUT variable X coordination */
	signed int sLumaLutX[MAX_DYNAMIC_CONTRAST_LUT_SIZE];
	/* if support DYNAMIC_CONTRAST_SATURATION_XY */
	unsigned int uSaturationY[MAX_DYNAMIC_CONTRAST_SATURATION_LUT_SIZE];
	unsigned int uSaturationX[MAX_DYNAMIC_CONTRAST_SATURATION_LUT_SIZE];
};

/* black level */
enum v4l2_ext_vpq_black_level_type {
	V4L2_EXT_VPQ_BLACKLEVEL_Y709_LINEAR_LIMIT_HIGH,
	V4L2_EXT_VPQ_BLACKLEVEL_Y709_BYPASS,
	V4L2_EXT_VPQ_BLACKLEVEL_RGB_Y709_LINEAR_LOW,
	V4L2_EXT_VPQ_BLACKLEVEL_RGB_Y709_LIMIT_HIGH,
	V4L2_EXT_VPQ_BLACKLEVEL_Y709_COMP_LOW,
	V4L2_EXT_VPQ_BLACKLEVEL_AV_RF_EXTENSION,
	V4L2_EXT_VPQ_BLACKLEVEL_RGB_BT2020_LINEAR_LOW,
	V4L2_EXT_VPQ_BLACKLEVEL_RGB_BT2020_LIMIT_HIGH,
	V4L2_EXT_VPQ_BLACKLEVEL_RGB_Y601_LINEAR_LOW,
	V4L2_EXT_VPQ_BLACKLEVEL_RGB_Y601_LIMIT_HIGH,
};

struct v4l2_ext_vpq_black_level_info {
	unsigned char ui_value;
	unsigned char curr_input;
	unsigned char color_space;
	enum v4l2_ext_vpq_black_level_type black_level_type;
};

enum v4l2_ext_vpq_picture_ctrl_type {
	V4L2_EXT_VPQ_PICTURE_CTRL_CONTRAST   = 0,
	V4L2_EXT_VPQ_PICTURE_CTRL_BRIGHTNESS = 1,
	V4L2_EXT_VPQ_PICTURE_CTRL_SATURATION = 2,
	V4L2_EXT_VPQ_PICTURE_CTRL_HUE		= 3,
	V4L2_EXT_VPQ_PICTURE_CTRL_MAX
};

struct v4l2_ext_vpq_picture_ctrl_data {
	signed int picture_ui_value[V4L2_EXT_VPQ_PICTURE_CTRL_MAX];
	signed int chipData_contrast;
	signed int chipData_brightness;
	signed int chipData_saturation;
	signed int chipData_hue;
};

struct v4l2_ext_vpq_super_resolution_data {
	unsigned char ui_value;
	union {
		unsigned char *pst_chip_data;
		unsigned int compat_data;
		unsigned long long sizer;
	};
};

struct v4l2_ext_vpq_mpeg_noise_reduction_data {
	unsigned char ui_value;
	union {
		unsigned char *pst_chip_data;
		unsigned int compat_data;
		unsigned long long sizer;
	};
};

struct v4l2_ext_vpq_decontour_data {
	unsigned char ui_value;
	union {
		unsigned char *pst_chip_data;
		unsigned int compat_data;
		unsigned long long sizer;
	};
};

struct v4l2_ext_vpq_sharpness_data {
	unsigned short ui_value[4];
	union {
		unsigned char *pst_chip_data;
		unsigned int compat_data;
		unsigned long long sizer;
	};
};

struct v4l2_ext_vpq_noise_reduction_data {
	unsigned short ui_value[2];
	union {
		unsigned char *pst_chip_data;
		unsigned int compat_data;
		unsigned long long sizer;
	};
};

/* V4L2_CID_EXT_VPQ_STEREO_FACE_CTRL */
#define V4L2_VPQ_EXT_STEREO_FACE_OFF (0)  // off
#define V4L2_VPQ_EXT_STEREO_FACE_ON (1)   // normal on
#define V4L2_VPQ_EXT_STEREO_FACE_DEMO (2) // demo mode

/* extra PQ inner pattern */
#define MAX_EXT_PATTERN_GRADATION_LINE (4) // it may depends on chip limitation
#define MAX_EXT_PATTERN_WINBOX (10)		// it may depends on chip limitation
#define MAX_EXT_PATTERN_GRADATION_H_STRIDE_SIZE (254) // it may depends on chip limitation
#define EXT_PATTERN_GRADATION_H_STRIDE_STEP (2) // it may depends on chip limitation
#define MAX_EXT_PATTERN_GRADATION_V_STRIDE_SIZE (127) // it may depends on chip limitation
#define EXT_PATTERN_GRADATION_V_STRIDE_STEP (1) // it may depends on chip limitation
#define EXT_PATTERN_WIDTH (3840)  // it may depends on chip limitation
#define EXT_PATTERN_HEIGHT (2160) // it may depends on chip limitation

enum V4L2_VPQ_EXT_PATTERN_MODE {
	V4L2_VPQ_EXT_PATTERN_WINBOX,
	V4L2_VPQ_EXT_PATTERN_GRADATION,
	V4L2_VPQ_EXT_PATTERN_MAX
};

enum V4L2_VPQ_EXT_PATTERN_GRADATION_DIRECTION {
	V4L2_VPQ_EXT_PATTERN_GRADATION_DIRECTION_HORIZONTAL,
	V4L2_VPQ_EXT_PATTERN_GRADATION_DIRECTION_VERTICAL,
	V4L2_VPQ_EXT_PATTERN_GRADATION_DIRECTION_MAX
};

struct v4l2_vpq_ext_pattern_gradation_line_attr {
	unsigned char lineIdx; // gradation line index
	unsigned short start_R; // 1st gradation block's red level as a 10bit resolution
	unsigned short start_G; // 1st gradation block's green level as a 10bit resolution
	unsigned short start_B; // 1st gradation block's blue level as a 10bit resolution
	unsigned short step_R;	 // step size for next gradation block
	unsigned short step_G;	 // step size for next gradation block
	unsigned short step_B;	 // step size for next gradation block
	unsigned short strideSize; // gradation block's width(horizontal
							   // mode)/height(vertical mode)
};

struct v4l2_vpq_ext_pattern_gradation_info {
	unsigned char numGrad; // number of gradation lines in a screen
	enum V4L2_VPQ_EXT_PATTERN_GRADATION_DIRECTION e_grad_mode;
	struct v4l2_vpq_ext_pattern_gradation_line_attr
		st_line_attr[MAX_EXT_PATTERN_GRADATION_LINE];
};

struct v4l2_vpq_ext_pattern_winbox_win_attr {
	unsigned short winIdx; // window layer index. 0:background
	unsigned short x;
	unsigned short y;
	unsigned short w;
	unsigned short h;
	unsigned short fill_R; // 10 bit resolution
	unsigned short fill_G; // 10 bit resolution
	unsigned short fill_B; // 10 bit resolution
};

struct v4l2_vpq_ext_pattern_winbox_info {
	unsigned char
		u8_num_win; // number of windows in a screen(including background window)
	struct v4l2_vpq_ext_pattern_winbox_win_attr
		st_win_box_attr[MAX_EXT_PATTERN_WINBOX];
};

struct
	v4l2_vpq_ext_pattern_info { // old version : this will be removed in near-time
	unsigned char b_on_off;
	enum V4L2_VPQ_EXT_PATTERN_MODE e_mode;
	struct v4l2_vpq_ext_pattern_gradation_info *pst_grad_info;
	struct v4l2_vpq_ext_pattern_winbox_info *pst_win_box_info;
};

#define USE_EXT_PATTERN_INFO_V2
struct v4l2_vpq_ext_pattern_info_v2 {
	unsigned char b_on_off;
	enum V4L2_VPQ_EXT_PATTERN_MODE emode;
	struct v4l2_vpq_ext_pattern_gradation_info st_grad_info;
	struct v4l2_vpq_ext_pattern_winbox_info st_win_box_info;
};

enum v4l2_ext_aipq_mode {
	V4L2_EXT_AIPQ_MODE_OFF = 0,
	V4L2_EXT_AIPQ_MODE_ON = 1,
	V4L2_EXT_AIPQ_MODE_DEMO_ON = 2,
};

enum v4l2_ext_aipq_scene_info {
	V4L2_EXT_AIPQ_SCENE_STANDARD = 0,
	V4L2_EXT_AIPQ_SCENE_BUILDING = 1,
	V4L2_EXT_AIPQ_SCENE_LANDSCAPE = 2,
	V4L2_EXT_AIPQ_SCENE_NIGHT = 3,
	V4L2_EXT_AIPQ_SCENE_INFO_MAX
};

enum v4l2_ext_aipq_genre_info {
	V4L2_EXT_AIPQ_GENRE_STANDARD = 0,
	V4L2_EXT_AIPQ_GENRE_SPORTS = 1,
	V4L2_EXT_AIPQ_GENRE_ANIMATION = 2,
	V4L2_EXT_AIPQ_GENRE_MOVIE = 3,
	V4L2_EXT_AIPQ_GENRE_INFO_MAX
};

/* VBE */
enum v4l2_ext_vbe_panel_inch {
	V4L2_EXT_VBE_PANEL_INCH_22 = 0,
	V4L2_EXT_VBE_PANEL_INCH_23,
	V4L2_EXT_VBE_PANEL_INCH_24,
	V4L2_EXT_VBE_PANEL_INCH_26,
	V4L2_EXT_VBE_PANEL_INCH_27,
	V4L2_EXT_VBE_PANEL_INCH_28,
	V4L2_EXT_VBE_PANEL_INCH_32,
	V4L2_EXT_VBE_PANEL_INCH_39,
	V4L2_EXT_VBE_PANEL_INCH_40,
	V4L2_EXT_VBE_PANEL_INCH_42,
	V4L2_EXT_VBE_PANEL_INCH_43,
	V4L2_EXT_VBE_PANEL_INCH_47,
	V4L2_EXT_VBE_PANEL_INCH_49,
	V4L2_EXT_VBE_PANEL_INCH_50,
	V4L2_EXT_VBE_PANEL_INCH_55,
	V4L2_EXT_VBE_PANEL_INCH_58,
	V4L2_EXT_VBE_PANEL_INCH_60,
	V4L2_EXT_VBE_PANEL_INCH_65,
	V4L2_EXT_VBE_PANEL_INCH_70,
	V4L2_EXT_VBE_PANEL_INCH_75,
	V4L2_EXT_VBE_PANEL_INCH_77,
	V4L2_EXT_VBE_PANEL_INCH_79,
	V4L2_EXT_VBE_PANEL_INCH_84,
	V4L2_EXT_VBE_PANEL_INCH_86,
	V4L2_EXT_VBE_PANEL_INCH_98,
	V4L2_EXT_VBE_PANEL_INCH_105,
	V4L2_EXT_VBE_PANEL_INCH_48, // 17Y Added
	V4L2_EXT_VBE_PANEL_INCH_MAX
};

enum v4l2_ext_vbe_panel_maker {
	V4L2_EXT_VBE_PANEL_MAKER_LGD = 0,
	V4L2_EXT_VBE_PANEL_MAKER_AUO,
	V4L2_EXT_VBE_PANEL_MAKER_SHARP,
	V4L2_EXT_VBE_PANEL_MAKER_BOE,
	V4L2_EXT_VBE_PANEL_MAKER_CSOT,
	V4L2_EXT_VBE_PANEL_MAKER_INNOLUX,
	V4L2_EXT_VBE_PANEL_MAKER_LGD_M,
	V4L2_EXT_VBE_PANEL_MAKER_ODM_B,
	V4L2_EXT_VBE_PANEL_MAKER_BOE_TPV,
	V4L2_EXT_VBE_PANEL_MAKER_HKC,
	V4L2_EXT_VBE_PANEL_MAKER_MAX
};

enum v4l2_ext_vbe_panel_interface {
	V4L2_EXT_VBE_PANEL_EPI = 0,
	V4L2_EXT_VBE_PANEL_LVDS,
	V4L2_EXT_VBE_PANEL_VX1,
	V4L2_EXT_VBE_PANEL_CEDS,
	V4L2_EXT_VBE_PANEL_EPI_QSAC,
	V4L2_EXT_VBE_PANEL_RESERVED1,
	V4L2_EXT_VBE_PANEL_RESERVED2,
	V4L2_EXT_VBE_PANEL_INTERFACE_MAX
};

enum v4l2_ext_vbe_panel_resolution {
	V4L2_EXT_VBE_PANEL_RESOLUTION_1024X768 = 0,
	V4L2_EXT_VBE_PANEL_RESOLUTION_1280X720,
	V4L2_EXT_VBE_PANEL_RESOLUTION_1366X768,
	V4L2_EXT_VBE_PANEL_RESOLUTION_1920X1080,
	V4L2_EXT_VBE_PANEL_RESOLUTION_2560X1080,
	V4L2_EXT_VBE_PANEL_RESOLUTION_3840X2160,
	V4L2_EXT_VBE_PANEL_RESOLUTION_5120X2160,
	V4L2_EXT_VBE_PANEL_RESOLUTION_7680X4320,
	V4L2_EXT_VBE_PANEL_RESOLUTION_RESERVED1,
	V4L2_EXT_VBE_PANEL_RESOLUTION_RESERVED2,
	V4L2_EXT_VBE_PANEL_RESOLUTION_RESERVED3,
	V4L2_EXT_VBE_PANEL_RESOLUTION_RESERVED4,
	V4L2_EXT_VBE_PANEL_RESOLUTION_RESERVED5,
	V4L2_EXT_VBE_PANEL_RESOLUTION_MAX
};

enum v4l2_ext_vbe_panel_version {
	V4L2_EXT_VBE_PANEL_NONE = 0,
	V4L2_EXT_VBE_PANEL_V12,
	V4L2_EXT_VBE_PANEL_V13,
	V4L2_EXT_VBE_PANEL_V14,
	V4L2_EXT_VBE_PANEL_V15,
	V4L2_EXT_VBE_PANEL_V16,
	V4L2_EXT_VBE_PANEL_V17,
	V4L2_EXT_VBE_PANEL_V18,
	V4L2_EXT_VBE_PANEL_MAX
};

enum v4l2_ext_vbe_panel_framerate {
	V4L2_EXT_VBE_PANEL_FRAMERATE_60HZ,
	V4L2_EXT_VBE_PANEL_FRAMERATE_120HZ,
	V4L2_EXT_VBE_PANEL_FRAMERATE_MAX
};

enum v4l2_ext_vbe_panel_backlight_type {
	V4L2_EXT_VBE_PANEL_BL_DIRECT_L = 0,
	V4L2_EXT_VBE_PANEL_BL_EDGE_LED,
	V4L2_EXT_VBE_PANEL_BL_OLED,
	V4L2_EXT_VBE_PANEL_BL_DIRECT_VI,
	V4L2_EXT_VBE_PANEL_BL_DIRECT_SKY,
	V4L2_EXT_VBE_PANEL_BL_MAX
};

enum v4l2_ext_vbe_panel_led_bar_type {
	V4L2_EXT_VBE_PANEL_LED_BAR_6 = 0,
	V4L2_EXT_VBE_PANEL_LED_BAR_12,
	V4L2_EXT_VBE_PANEL_LED_BAR_36,
	V4L2_EXT_VBE_PANEL_LED_BAR_40,
	V4L2_EXT_VBE_PANEL_LED_BAR_48,
	V4L2_EXT_VBE_PANEL_LED_BAR_50,
	V4L2_EXT_VBE_PANEL_LED_BAR_96,
	V4L2_EXT_VBE_PANEL_LED_BAR_MAX
};

enum v4l2_ext_vbe_panel_cell_type {
	V4L2_EXT_VBE_PANEL_CELL_TYPE_RGB = 0,
	V4L2_EXT_VBE_PANEL_CELL_TYPE_RGBW,
	V4L2_EXT_VBE_PANEL_CELL_TYPE_MAX
};

enum v4l2_ext_vbe_panel_bandwidth {
	V4L2_EXT_VBE_PANEL_BANDWIDTH_DEFAULT = 0,
	V4L2_EXT_VBE_PANEL_BANDWIDTH_1_5G,
	V4L2_EXT_VBE_PANEL_BANDWIDTH_2_1G,
	V4L2_EXT_VBE_PANEL_BANDWIDTH_3_0G,
	V4L2_EXT_VBE_PANEL_BANDWIDTH_MAX
};

enum v4l2_ext_vbe_frc_chip {
	V4L2_EXT_VBE_FRC_CHIP_NONE = 0,
	V4L2_EXT_VBE_FRC_CHIP_INTERNAL,
	V4L2_EXT_VBE_FRC_CHIP_EXTERNAL1, // Reserved1
	V4L2_EXT_VBE_FRC_CHIP_EXTERNAL2, // Reserved2
	V4L2_EXT_VBE_FRC_CHIP_TYPE_MAX
};

enum v4l2_ext_vbe_lvds_colordepth {
	V4L2_EXT_VBE_LVDS_COLOR_DEPTH_8BIT,
	V4L2_EXT_VBE_LVDS_COLOR_DEPTH_10BIT,
	V4L2_EXT_VBE_LVDS_COLOR_DEPTH_MAX
};

enum v4l2_ext_vbe_lvds_type {
	V4L2_EXT_VBE_LVDS_TYPE_VESA,
	V4L2_EXT_VBE_LVDS_TYPE_JEIDA,
	V4L2_EXT_VBE_LVDS_TYPE_MAX
};

union v4l2_ext_vbe_user_option {
	unsigned int all;
//	struct _flags {
//		unsigned int SocOptionBIT0 : 1, SocOptionBIT1 : 1, SocOptionBIT2 : 1,
//			SocOptionBIT3 : 1, SocOptionBIT4 : 1, SocOptionBIT5 : 1,
//			SocOptionBIT6 : 1, SocOptionBIT7 : 1, SocOptionBIT8 : 1,
//			SocOptionBIT9 : 1, SocOptionBIT10 : 1, SocOptionBIT11 : 1,
//			SocOptionBIT12 : 1, SocOptionBIT13 : 1, SocOptionBIT14 : 1,
//			SocOptionBIT15 : 1, SocOptionBIT16 : 1, SocOptionBIT17 : 1,
//			SocOptionBIT18 : 1, SocOptionBIT19 : 1, SocOptionBIT20 : 1,
//			SocOptionBIT21 : 1, SocOptionBIT22 : 1, SocOptionBIT23 : 1,
//			SocOptionBIT24 : 1, SocOptionBIT25 : 1, SocOptionBIT26 : 1,
//			SocOptionBIT27 : 1, SocOptionBIT28 : 1, SocOptionBIT29 : 1,
//			SocOptionBIT30 : 1, SocOptionBIT31 : 1;
//	} flags;
};

struct v4l2_ext_vbe_panel_info {
	enum v4l2_ext_vbe_panel_inch panelInch;			   // Panel Inch
	enum v4l2_ext_vbe_panel_maker panelMaker;			 // Panel maker
	enum v4l2_ext_vbe_panel_interface panelInterface;	 // Panel Interface
	enum v4l2_ext_vbe_panel_resolution panelResolution;   // Panel Resolution
	enum v4l2_ext_vbe_panel_version panelVersion;		 // Panel Version
	enum v4l2_ext_vbe_panel_framerate panelFrameRate;	 // Panel Frame Rate
	enum v4l2_ext_vbe_panel_led_bar_type panelLedBarType; // LED bar type
	enum v4l2_ext_vbe_panel_backlight_type panelBacklightType; // LED Backlight type
	enum v4l2_ext_vbe_panel_cell_type panelCellType;   // Panel Cell type
	enum v4l2_ext_vbe_panel_bandwidth dispOutLaneBW;   // Output lane bandwidth
	enum v4l2_ext_vbe_frc_chip frcType;				// FRC type
	enum v4l2_ext_vbe_lvds_colordepth lvdsColorDepth;  // LVDS Color depth
	enum v4l2_ext_vbe_lvds_type lvdsType;			  // LVDS Type
	union v4l2_ext_vbe_user_option userSpecificOption; // Reserved Options
};

struct v4l2_ext_vbe_ssc {
	unsigned int on_off;
	unsigned short percent;
	unsigned short period;
};

struct v4l2_ext_vbe_mirror {
	unsigned int bIsH;
	unsigned int bIsV;
};

enum v4l2_ext_vbe_mplus_mode {
	V4L2_EXT_VBE_MPLUS_MPLUS_MODE0 = 0,// V4L2_EXT_VBE_MPLUS_HIGH_LUM1_MSE_ON = 0,
	V4L2_EXT_VBE_MPLUS_MPLUS_MODE1,  // V4L2_EXT_VBE_MPLUS_HIGH_LUM2,
	V4L2_EXT_VBE_MPLUS_MPLUS_MODE2,  // V4L2_EXT_VBE_MPLUS_LOW_POWER1,
	V4L2_EXT_VBE_MPLUS_MPLUS_MODE3,  // V4L2_EXT_VBE_MPLUS_MLE_MODE_OFF,
	V4L2_EXT_VBE_MPLUS_MPLUS_MODE4,  // V4L2_EXT_VBE_MPLUS_HIGH_LUM1_MSE_OFF,
	V4L2_EXT_VBE_MPLUS_MPLUS_MODE5,  // V4L2_EXT_VBE_MPLUS_LOW_POWER2,
	V4L2_EXT_VBE_MPLUS_MPLUS_MODE6,  // V4L2_EXT_VBE_MPLUS_LOW_POWER2_SC_OFF,
	V4L2_EXT_VBE_MPLUS_MPLUS_MODE7,  // MHE Mode1 09/20/2018
	V4L2_EXT_VBE_MPLUS_MPLUS_MODE8,  // MHE Mode2 09/20/2018
	V4L2_EXT_VBE_MPLUS_MPLUS_MODE9,  // MHE Mode3 09/20/2018
	V4L2_EXT_VBE_MPLUS_MPLUS_MODE10, // MHE Mode4 09/20/2018
	V4L2_EXT_VBE_MPLUS_MPLUS_MODEMAX
};

struct v4l2_ext_vbe_dga4ch {
	union {
		unsigned int *p_red_gamma_table;
		unsigned int compat_data_red;
		unsigned long long sizer_red;
	};
	union {
		unsigned int *p_green_gamma_gable;
		unsigned int compat_data_green;
		unsigned long long sizer_green;
	};
	union {
		unsigned int *p_blue_gamma_table;
		unsigned int compat_data_blue;
		unsigned long long sizer_blue;
	};
	union {
		unsigned int *p_white_gamma_table;
		unsigned int compat_data_white;
		unsigned long long sizer_white;
	};
};

struct v4l2_ext_vbe_mplus_param {
	unsigned short nFrameGainLimit;
	unsigned short nPixelGainLimit;
};

struct v4l2_ext_vbe_inner_pattern {
	unsigned char bOnOff;
	unsigned char ip;
	unsigned char type;
};

struct v4l2_ext_vbe_panel_tscic {
	union {
		unsigned char *u8p_control_tbl;//unsigned char *u8pControlTbl;
		unsigned int compat_data_ctrl;
		unsigned long long sizer_ctrl;
	};
	unsigned int u32Ctrlsize;
	union {
		unsigned int *u32ptscictbl;//unsigned int *u32pTSCICTbl;
		unsigned int compat_data_tscic;
		unsigned long long sizer_tscic;
	};
	unsigned int u32Tscicsize;
};

struct v4l2_ext_vbe_mplus_data {
	union {
		void *p_register_set;
		unsigned int compat_data;
		unsigned long long sizer;
	};
	unsigned char nPanelMaker;
};

enum v4l2_ext_vbe_pwm_pin_sel {
	V4L2_EXT_VBE_PWM_DEV_PIN0 = 0,
	V4L2_EXT_VBE_PWM_DEV_PIN1,
	V4L2_EXT_VBE_PWM_DEV_PIN2,
	V4L2_EXT_VBE_PWM_DEV_PIN3,
	V4L2_EXT_VBE_PWM_DEV_PIN4,
	V4L2_EXT_VBE_PWM_DEV_MAX,
	V4L2_EXT_VBE_PWM_DEV_NONE = 0xFF,
};

struct v4l2_ext_vbe_pwm_adapt_freq_param {
	unsigned int pwm_adapt_freq_enable;
	unsigned int pwmfreq_48nHz; // PWM frequency 48xN Hz from DB table
	unsigned int pwmfreq_50nHz; // PWM frequency 50xN Hz from DB table
	unsigned int pwmfreq_60nHz; // PWM frequency 60xN Hz from DB table
};

struct v4l2_ext_vbe_pwm_param_data {
	unsigned int pwm_enable;
	unsigned int pwm_duty;
	unsigned int pwm_frequency; // If pwm_adapt_freq_enable == TRUE, ignored
	struct v4l2_ext_vbe_pwm_adapt_freq_param pwm_adapt_freq_param;
	unsigned int pwm_lock;
	unsigned int pwm_pos_start;
	unsigned int pwm_scanning_enable;
	unsigned int pwm_low_power_enable; // It has been used to set low power mode
	// for M16P only until now, 170210.
};

enum v4l2_ext_vbe_pwm_pin_sel_mask {
	V4L2_EXT_VBE_PWM_DEV_PIN_0_MASK =
		1 << V4L2_EXT_VBE_PWM_DEV_PIN0,
	V4L2_EXT_VBE_PWM_DEV_PIN_1_MASK =
		1 << V4L2_EXT_VBE_PWM_DEV_PIN1,
	V4L2_EXT_VBE_PWM_DEV_PIN_2_MASK =
		1 << V4L2_EXT_VBE_PWM_DEV_PIN2,
	V4L2_EXT_VBE_PWM_DEV_PIN_3_MASK =
		1 << V4L2_EXT_VBE_PWM_DEV_PIN3,
	V4L2_EXT_VBE_PWM_DEV_PIN_4_MASK =
		1 << V4L2_EXT_VBE_PWM_DEV_PIN4,
	V4L2_EXT_VBE_PWM_DEV_PIN_0_1_MASK =
		V4L2_EXT_VBE_PWM_DEV_PIN_0_MASK |
		V4L2_EXT_VBE_PWM_DEV_PIN_1_MASK,
	V4L2_EXT_VBE_PWM_DEV_PIN_0_2_MASK =
		V4L2_EXT_VBE_PWM_DEV_PIN_0_MASK |
		V4L2_EXT_VBE_PWM_DEV_PIN_2_MASK,
	V4L2_EXT_VBE_PWM_DEV_PIN_1_2_MASK =
		V4L2_EXT_VBE_PWM_DEV_PIN_1_MASK |
		V4L2_EXT_VBE_PWM_DEV_PIN_2_MASK,
	/* If necessary, add case */
	V4L2_EXT_VBE_PWM_DEV_PIN_ALL_MASK =
		V4L2_EXT_VBE_PWM_DEV_PIN_0_MASK | V4L2_EXT_VBE_PWM_DEV_PIN_1_MASK |
		V4L2_EXT_VBE_PWM_DEV_PIN_2_MASK | V4L2_EXT_VBE_PWM_DEV_PIN_3_MASK |
		V4L2_EXT_VBE_PWM_DEV_PIN_4_MASK,
	V4L2_EXT_VBE_PWM_DEV_PIN_DEFAULT_MASK
};

struct v4l2_ext_vbe_pwm_param {
	enum v4l2_ext_vbe_pwm_pin_sel pwmIndex;
	union {
		struct v4l2_ext_vbe_pwm_param_data *pst_pwm_param;
		unsigned int compat_data;
		unsigned long long sizer;
	};
};

struct v4l2_ext_vbe_pwm_duty {
	enum v4l2_ext_vbe_pwm_pin_sel_mask pwmIndex;
	unsigned int pwm_duty;
};

struct v4l2_ext_vbe_vcom_pat_draw {
	union {
		unsigned short *vcompattern;
		unsigned int compat_data;
		unsigned long long sizer;
	};
	unsigned short nSize;
};

enum v4l2_ext_vbe_vcom_pat_ctrl {
	V4L2_EXT_VBE_VCOM_PAT_CTRL_OFF = 0,// Mandatory Implementation
	V4L2_EXT_VBE_VCOM_PAT_CTRL_ON,	  // Mandatory Implementation
	// Optional Implementation, PGEN_VCOM1 and CTRL_ON, for available SoC
	V4L2_EXT_VBE_VCOM_PAT_CTRL_VCOM1,
	// Optional Implementation, PGEN_VCOM2 and CTRL_ON, for available SoC
	V4L2_EXT_VBE_VCOM_PAT_CTRL_VCOM2,
	// Optional Implementation, PGEN_VCOM2 and CTRL_ON, for available SoC
	V4L2_EXT_VBE_VCOM_PAT_CTRL_VCOM3,
	// Optional Implementation, PGEN_VCOM3 and CTRL_ON, for available SoC
	V4L2_EXT_VBE_VCOM_PAT_CTRL_VCOM4,
	// Optional Implementation, PGEN_VCOM4 and CTRL_ON, for available SoC
	V4L2_EXT_VBE_VCOM_PAT_CTRL_VCOM5,
	// Optional Implementation, PGEN_VCOM5 and CTRL_ON, for available SoC
	V4L2_EXT_VBE_VCOM_PAT_CTRL_VCOM6,
	// Optional Implementation, PGEN_VCOM6 and CTRL_ON, for available SoC
	V4L2_EXT_VBE_VCOM_PAT_CTRL_VCOM7,
	// Optional Implementation, PGEN_VCOM7 and CTRL_ON, for available SoC
	V4L2_EXT_VBE_VCOM_PAT_CTRL_VCOM8,
	// Optional Implementation, PGEN_VCOM8 and CTRL_ON, for available SoC
	V4L2_EXT_VBE_VCOM_PAT_CTRL_MAX	// Limit to control.
};

enum v4l2_ext_vbe_panel_orbit_mode {
	V4L2_EXT_VBE_PANEL_ORBIT_JUSTSCAN_MODE = 0,
	V4L2_EXT_VBE_PANEL_ORBIT_AUTO_MODE,
	V4L2_EXT_VBE_PANEL_ORBIT_STORE_MODE,
	V4L2_EXT_VBE_PANEL_OREBIT_MODE_MAX
};

struct v4l2_ext_vbe_panel_orbit_info {
	unsigned int on_off;
	enum v4l2_ext_vbe_panel_orbit_mode orbitmode;
};

enum v4l2_ext_vbe_panel_lsr_mode {
	V4L2_EXT_VBE_PANEL_LSR_OFF = 0,
	V4L2_EXT_VBE_PANEL_LSR_LIGHT,  // vivid light
	V4L2_EXT_VBE_PANEL_LSR_STRONG, // vivid strong
	V4L2_EXT_VBE_PANEL_LSR_STRONG_OTHERS,
	V4L2_EXT_VBE_PANEL_LSR_LIGHT_OTHERS,
	V4L2_EXT_VBE_PANEL_LSR_MAX
};

struct v4l2_ext_vbe_panel_lsr_info {
	union {
		unsigned int *plsrtable;//unsigned int *pLsrTable;
		unsigned int compat_data_lsrtable;
		unsigned long long sizer_lsrtable;
	};
	enum v4l2_ext_vbe_panel_lsr_mode lsrstep;
};

struct v4l2_ext_vbe_panel_gsr_info {
	union {
		unsigned int *pgsrtable;//unsigned int *pGsrTable;
		unsigned int compat_data_gsrtable;
		unsigned long long sizer_gsrtable;
	};
};

struct v4l2_ext_vbe_panel_osd_gain_info {
	union {
		unsigned int *levelval;
		unsigned int compat_data_levelval;
		unsigned long long sizer_levelval;
	};
	unsigned int on_off;
	unsigned int size;
};

struct v4l2_ext_vbe_panel_alpha_osd_info {
	union {
		unsigned int *alpha_able;
		unsigned int compat_data_alphatable;
		unsigned long long sizer_alphatable;
	};
	unsigned int size;
};

struct v4l2_ext_vbe_panel_demura {
	unsigned int enable; // 0: disable, 1: enable
	union {
		unsigned char *config;
		unsigned int compat_data_ctrl;
		unsigned long long sizer_ctrl;
	};
	unsigned int configsize;
	unsigned int configcrc;
	union {
		unsigned char *data;
		unsigned int compat_data_demura;
		unsigned long long sizer_demura;
	};
	unsigned int datasize;
	unsigned int datacrc;
};

struct v4l2_ext_vbe_panel_pclrc {
	unsigned int enable; // 0: disable, 1: enable
	unsigned int positionAfterLineOD; // Signal Path 0 : PCLRC -> LOD, 1 : LOD -> PCLRC
	union {
		unsigned char *data; //It should write to 8192 to 12543 in SRAM LUT.
		unsigned int compat_data_pclrc;
		unsigned long long sizer_pclrc;
		};
		unsigned int datasize;
};

struct v4l2_ext_vbe_panel_second_gsr_info {
	union {
		unsigned int *p_gsr_table;
		unsigned int compat_data_gsrtable;
		unsigned long long sizer_gsrtable;
	};
};

struct v4l2_ext_vbe_panel_irr_info {
	union {
		unsigned int *pirrlum;//unsigned int *pIrrlum;
		unsigned int compat_data_irr;
		unsigned long long sizer_irr;
	};
};

/* HDMI */
#define V4L2_EXT_HDMI_PACKET_DATA_LENGTH 28
#define V4L2_EXT_HDMI_INFOFRAME_PACKET_LEN 28
#define V4L2_EXT_HDMI_VENDOR_SPECIFIC_REGID_LEN 3
#define V4L2_EXT_HDMI_VENDOR_SPECIFIC_PAYLOAD_LEN \
		(V4L2_EXT_HDMI_INFOFRAME_PACKET_LEN - \
		V4L2_EXT_HDMI_VENDOR_SPECIFIC_REGID_LEN)

#define V4L2_EXT_HDMI_SPD_IF_VENDOR_LEN 8
#define V4L2_EXT_HDMI_SPD_IF_DESC_LEN 16
#define V4L2_EXT_HDMI_TMDS_CH_NUM 4

enum v4l2_ext_hdmi_input_port {
	V4L2_EXT_HDMI_INPUT_PORT_NONE = 0,
	V4L2_EXT_HDMI_INPUT_PORT_1,
	V4L2_EXT_HDMI_INPUT_PORT_2,
	V4L2_EXT_HDMI_INPUT_PORT_3,
	V4L2_EXT_HDMI_INPUT_PORT_4,
	V4L2_EXT_HDMI_INPUT_PORT_5,
	V4L2_EXT_HDMI_INPUT_PORT_ALL,
};

enum v4l2_ext_hdmi_mode {
	V4L2_EXT_HDMI_MODE_DVI = 0,
	V4L2_EXT_HDMI_MODE_HDMI,
};

enum v4l2_ext_hdmi_color_depth {
	V4L2_EXT_HDMI_COLOR_DEPTH_8BIT = 0,
	V4L2_EXT_HDMI_COLOR_DEPTH_10BIT,
	V4L2_EXT_HDMI_COLOR_DEPTH_12BIT,
	V4L2_EXT_HDMI_COLOR_DEPTH_16BIT,
	V4L2_EXT_HDMI_COLOR_DEPTH_RESERVED
};

struct v4l2_ext_hdmi_timing_info {
	enum v4l2_ext_hdmi_input_port port;
	unsigned short h_freq;
	unsigned short v_vreq;
	unsigned short h_total;
	unsigned short v_total;
	unsigned short h_porch;
	unsigned short v_porch;
	struct v4l2_ext_video_rect active;
	unsigned short scan_type;
	enum v4l2_ext_hdmi_mode dvi_hdmi;
	enum v4l2_ext_hdmi_color_depth color_depth;
	unsigned char allm_mode;
};

enum v4l2_ext_hdmi_override_eotf {
	V4L2_EXT_HDMI_OVERRIDE_EOTF_SDR_LUMINANCE_RANGE = 0,
	V4L2_EXT_HDMI_OVERRIDE_EOTF_HDR_LUMINANCE_RANGE,
	V4L2_EXT_HDMI_OVERRIDE_EOTF_SMPTE_ST_2084,
	V4L2_EXT_HDMI_OVERRIDE_EOTF_HLG,
	V4L2_EXT_HDMI_OVERRIDE_EOTF_RESERVED_4,
	V4L2_EXT_HDMI_OVERRIDE_EOTF_RESERVED_5,
	V4L2_EXT_HDMI_OVERRIDE_EOTF_RESERVED_6,
	V4L2_EXT_HDMI_OVERRIDE_EOTF_RESERVED_7,
	V4L2_EXT_HDMI_OVERRIDE_EOTF_AUTO,
};

struct v4l2_ext_hdmi_override_drm_info {
	enum v4l2_ext_hdmi_input_port port;
	enum v4l2_ext_hdmi_override_eotf override_eotf;
};

enum v4l2_ext_hdmi_drm_eotf {
	V4L2_EXT_HDMI_DRM_EOTF_SDR_LUMINANCE_RANGE =
		0,
	V4L2_EXT_HDMI_DRM_EOTF_HDR_LUMINANCE_RANGE,
	V4L2_EXT_HDMI_DRM_EOTF_SMPTE_ST_2084,
	V4L2_EXT_HDMI_DRM_EOTF_HLG,
	V4L2_EXT_HDMI_DRM_EOTF_RESERVED_4,
	V4L2_EXT_HDMI_DRM_EOTF_RESERVED_5,
	V4L2_EXT_HDMI_DRM_EOTF_RESERVED_6,
	V4L2_EXT_HDMI_DRM_EOTF_RESERVED_7,
};

enum v4l2_ext_hdmi_drm_meta_desc {
	V4L2_EXT_HDMI_DRM_META_DESC_TYPE1 = 0,
	V4L2_EXT_HDMI_DRM_META_DESC_RESERVED1,
	V4L2_EXT_HDMI_DRM_META_DESC_RESERVED2,
	V4L2_EXT_HDMI_DRM_META_DESC_RESERVED3,
	V4L2_EXT_HDMI_DRM_META_DESC_RESERVED4,
	V4L2_EXT_HDMI_DRM_META_DESC_RESERVED5,
	V4L2_EXT_HDMI_DRM_META_DESC_RESERVED6,
	V4L2_EXT_HDMI_DRM_META_DESC_RESERVED7,
};

struct v4l2_ext_hdmi_drm_info {
	enum v4l2_ext_hdmi_input_port port;
	unsigned char version;
	unsigned char length;
	enum v4l2_ext_hdmi_drm_eotf eotf_type;
	enum v4l2_ext_hdmi_drm_meta_desc meta_desc;
	unsigned short display_primaries_x0;
	unsigned short display_primaries_y0;
	unsigned short display_primaries_x1;
	unsigned short display_primaries_y1;
	unsigned short display_primaries_x2;
	unsigned short display_primaries_y2;
	unsigned short white_point_x;
	unsigned short white_point_y;
	unsigned short max_display_mastering_luminance;
	unsigned short min_display_mastering_luminance;
	unsigned short maximum_content_light_level;
	unsigned short maximum_frame_average_light_level;
};

struct v4l2_ext_hdmi_hpd_low_duration_dc_on {
	enum v4l2_ext_hdmi_input_port port;
	int hpd_low_duration;
};

enum v4l2_ext_hdmi_vsi_video_format {
	V4L2_EXT_HDMI_VSI_VIDEO_FORMAT_NO_ADDITIONAL_FORMAT = 0,
	V4L2_EXT_HDMI_VSI_VIDEO_FORMAT_EXTENDED_RESOLUTION_FORMAT,
	V4L2_EXT_HDMI_VSI_VIDEO_FORMAT_3D_FORMAT,
};

enum v4l2_ext_hdmi_vsi_3d_structure {
	V4L2_EXT_HDMI_VSI_3D_STRUCTURE_FRAME_PACKING = 0,
	V4L2_EXT_HDMI_VSI_3D_STRUCTURE_FIELD_ALTERNATIVE,
	V4L2_EXT_HDMI_VSI_3D_STRUCTURE_LINE_ALTERNATIVE,
	V4L2_EXT_HDMI_VSI_3D_STRUCTURE_SIDEBYSIDE_FULL,
	V4L2_EXT_HDMI_VSI_3D_STRUCTURE_L_DEPTH,
	V4L2_EXT_HDMI_VSI_3D_STRUCTURE_L_DEPTH_GRAPHICS,
	V4L2_EXT_HDMI_VSI_3D_STRUCTURE_TOP_BOTTOM,
	/*reserved 7*/
	V4L2_EXT_HDMI_VSI_3D_STRUCTURE_SIDEBYSIDE_HALF	= 0x08,
	V4L2_EXT_HDMI_VSI_3D_STRUCTURE_TOP_BOTTOM_DIRECTV = 0x09,
};

enum v4l2_ext_hdmi_vsi_3d_ext_data {
	V4L2_EXT_HDMI_VSI_3D_EXT_DATA_HOR_SUB_SAMPL_0 = 0,
	V4L2_EXT_HDMI_VSI_3D_EXT_DATA_HOR_SUB_SAMPL_1,
	V4L2_EXT_HDMI_VSI_3D_EXT_DATA_HOR_SUB_SAMPL_2,
	V4L2_EXT_HDMI_VSI_3D_EXT_DATA_HOR_SUB_SAMPL_3,
	V4L2_EXT_HDMI_VSI_3D_EXT_DATA_QUINCUNX_MATRIX_0,
	V4L2_EXT_HDMI_VSI_3D_EXT_DATA_QUINCUNX_MATRIX_1,
	V4L2_EXT_HDMI_VSI_3D_EXT_DATA_QUINCUNX_MATRIX_2,
	V4L2_EXT_HDMI_VSI_3D_EXT_DATA_QUINCUNX_MATRIX_3,
};

enum v4l2_ext_hdmi_vsi_vic {
	V4L2_EXT_HDMI_VSI_VIC_RESERVED = 0,
	V4L2_EXT_HDMI_VSI_VIC_4K2K_30HZ,
	V4L2_EXT_HDMI_VSI_VIC_4K2K_25HZ,
	V4L2_EXT_HDMI_VSI_VIC_4K2K_24HZ,
	V4L2_EXT_HDMI_VSI_VIC_4K2K_24HZ_SMPTE,
};

enum v4l2_ext_hdmi_packet_status {
	V4L2_EXT_HDMI_PACKET_STATUS_NOT_RECEIVED = 0,
	V4L2_EXT_HDMI_PACKET_STATUS_STOPPED,
	V4L2_EXT_HDMI_PACKET_STATUS_UPDATED,
};

struct v4l2_ext_hdmi_in_packet {
	unsigned char type;
	unsigned char version;
	unsigned char length;
	unsigned char data_bytes[V4L2_EXT_HDMI_PACKET_DATA_LENGTH];
};

struct v4l2_ext_hdmi_vsi_info {
	enum v4l2_ext_hdmi_input_port port;
	enum v4l2_ext_hdmi_vsi_video_format video_format;
	enum v4l2_ext_hdmi_vsi_3d_structure st_3d;
	enum v4l2_ext_hdmi_vsi_3d_ext_data ext_data_3d;
	enum v4l2_ext_hdmi_vsi_vic vic;
	unsigned char regid[V4L2_EXT_HDMI_VENDOR_SPECIFIC_REGID_LEN];
	unsigned char payload[V4L2_EXT_HDMI_VENDOR_SPECIFIC_PAYLOAD_LEN];
	enum v4l2_ext_hdmi_packet_status packet_status;
	struct v4l2_ext_hdmi_in_packet packet;
};

struct v4l2_ext_hdmi_spd_info {
	enum v4l2_ext_hdmi_input_port port;
	unsigned char vendor_name[V4L2_EXT_HDMI_SPD_IF_VENDOR_LEN + 1];
	unsigned char product_description[V4L2_EXT_HDMI_SPD_IF_DESC_LEN + 1];
	unsigned char source_device_info;
	enum v4l2_ext_hdmi_packet_status packet_status;
	struct v4l2_ext_hdmi_in_packet packet;
};

enum v4l2_ext_hdmi_avi_csc {
	V4L2_EXT_HDMI_AVI_CSC_RGB = 0,
	V4L2_EXT_HDMI_AVI_CSC_YCBCR422,
	V4L2_EXT_HDMI_AVI_CSC_YCBCR444,
	V4L2_EXT_HDMI_AVI_CSC_YCBCR420,
};

enum v4l2_ext_hdmi_avi_active_info {
	V4L2_EXT_HDMI_AVI_ACTIVE_INFO_INVALID = 0,
	V4L2_EXT_HDMI_AVI_ACTIVE_INFO_VALID,
};

enum v4l2_ext_hdmi_avi_bar_info {
	V4L2_EXT_HDMI_AVI_BAR_INFO_INVALID = 0,
	V4L2_EXT_HDMI_AVI_BAR_INFO_VERTICALVALID,
	V4L2_EXT_HDMI_AVI_BAR_INFO_HORIZVALID,
	V4L2_EXT_HDMI_AVI_BAR_INFO_VERTHORIZVALID,
};

enum v4l2_ext_hdmi_avi_scan_info {
	V4L2_EXT_HDMI_AVI_SCAN_INFO_NODATA = 0,
	V4L2_EXT_HDMI_AVI_SCAN_INFO_OVERSCANNED,
	V4L2_EXT_HDMI_AVI_SCAN_INFO_UNDERSCANNED,
	V4L2_EXT_HDMI_AVI_SCAN_INFO_FUTURE,
};

enum v4l2_ext_hdmi_avi_colorimetry {
	V4L2_EXT_HDMI_AVI_COLORIMETRY_NODATA = 0,
	V4L2_EXT_HDMI_AVI_COLORIMETRY_SMPTE170,
	V4L2_EXT_HDMI_AVI_COLORIMETRY_ITU709,
	V4L2_EXT_HDMI_AVI_COLORIMETRY_FUTURE,
	V4L2_EXT_HDMI_AVI_COLORIMETRY_EXTENDED =
		V4L2_EXT_HDMI_AVI_COLORIMETRY_FUTURE,

};

enum v4l2_ext_hdmi_avi_picture_arc {
	V4L2_EXT_HDMI_AVI_PICTURE_ARC_NODATA = 0,
	V4L2_EXT_HDMI_AVI_PICTURE_ARC_4_3,
	V4L2_EXT_HDMI_AVI_PICTURE_ARC_16_9,
	V4L2_EXT_HDMI_AVI_PICTURE_ARC_FUTURE,
};

enum v4l2_ext_hdmi_avi_active_format_arc {
	V4L2_EXT_HDMI_AVI_ACTIVE_FORMAT_ARC_PICTURE	= 8,
	V4L2_EXT_HDMI_AVI_ACTIVE_FORMAT_ARC_4_3CENTER  = 9,
	V4L2_EXT_HDMI_AVI_ACTIVE_FORMAT_ARC_16_9CENTER = 10,
	V4L2_EXT_HDMI_AVI_ACTIVE_FORMAT_ARC_14_9CENTER = 11,
	V4L2_EXT_HDMI_AVI_ACTIVE_FORMAT_ARC_OTHER	  = 0,
};

enum v4l2_ext_hdmi_avi_scaling {
	V4L2_EXT_HDMI_AVI_SCALING_NOSCALING = 0,
	V4L2_EXT_HDMI_AVI_SCALING_HSCALING,
	V4L2_EXT_HDMI_AVI_SCALING_VSCALING,
	V4L2_EXT_HDMI_AVI_SCALING_HVSCALING,
};

enum v4l2_ext_hdmi_avi_it_content {
	V4L2_EXT_HDMI_AVI_IT_CONTENT_NODATA = 0,
	V4L2_EXT_HDMI_AVI_IT_CONTENT_ITCONTENT,
};

enum v4l2_ext_hdmi_avi_ext_colorimetry {
	V4L2_EXT_HDMI_AVI_EXT_COLORIMETRY_XVYCC601 = 0,
	V4L2_EXT_HDMI_AVI_EXT_COLORIMETRY_XVYCC709,
	V4L2_EXT_HDMI_AVI_EXT_COLORIMETRY_SYCC601,
	V4L2_EXT_HDMI_AVI_EXT_COLORIMETRY_ADOBEYCC601,
	V4L2_EXT_HDMI_AVI_EXT_COLORIMETRY_ADOBERGB,
	V4L2_EXT_HDMI_AVI_EXT_COLORIMETRY_BT2020_YCCBCCRC,
	V4L2_EXT_HDMI_AVI_EXT_COLORIMETRY_BT2020_RGBORYCBCR,
	V4L2_EXT_HDMI_AVI_EXT_COLORIMETRY_XVRESERED,
};

enum v4l2_ext_hdmi_avi_rgb_quantization_range {
	V4L2_EXT_HDMI_AVI_RGB_QUANTIZATION_RANGE_DEFAULT = 0,
	V4L2_EXT_HDMI_AVI_RGB_QUANTIZATION_RANGE_LIMITEDRANGE,
	V4L2_EXT_HDMI_AVI_RGB_QUANTIZATION_RANGE_FULLRANGE,
	V4L2_EXT_HDMI_AVI_RGB_QUANTIZATION_RANGE_RESERVED,
};

enum v4l2_ext_hdmi_avi_ycc_quantization_range {
	V4L2_EXT_HDMI_AVI_YCC_QUANTIZATION_RANGE_LIMITEDRANGE = 0,
	V4L2_EXT_HDMI_AVI_YCC_QUANTIZATION_RANGE_FULLRANGE,
	V4L2_EXT_HDMI_AVI_YCC_QUANTIZATION_RANGE_RESERVED,
};

enum v4l2_ext_hdmi_avi_content_type {
	V4L2_EXT_HDMI_AVI_CONTENT_TYPE_GRAPHICS = 0,
	V4L2_EXT_HDMI_AVI_CONTENT_TYPE_PHOTO,
	V4L2_EXT_HDMI_AVI_CONTENT_TYPE_CINEMA,
	V4L2_EXT_HDMI_AVI_CONTENT_TYPE_GAME,
};

enum v4l2_ext_hdmi_avi_additional_colorimetry {
	V4L2_EXT_HDMI_AVI_ADDITIONAL_COLORIMETRY_DCI_P3_D65 = 0,
	V4L2_EXT_HDMI_AVI_ADDITIONAL_COLORIMETRY_DCI_P3_THEATER,
	V4L2_EXT_HDMI_AVI_ADDITIONAL_COLORIMETRY_RESERVED,
};

struct v4l2_ext_hdmi_avi_info {
	enum v4l2_ext_hdmi_input_port port;
	enum v4l2_ext_hdmi_mode mode;

	enum v4l2_ext_hdmi_avi_csc pixel_encoding;
	enum v4l2_ext_hdmi_avi_active_info active_info;
	enum v4l2_ext_hdmi_avi_bar_info bar_info;
	enum v4l2_ext_hdmi_avi_scan_info scan_info;
	enum v4l2_ext_hdmi_avi_colorimetry colorimetry;
	enum v4l2_ext_hdmi_avi_picture_arc picture_aspect_ratio;
	enum v4l2_ext_hdmi_avi_active_format_arc active_format_aspect_ratio;
	enum v4l2_ext_hdmi_avi_scaling scaling;

	unsigned char vic;
	unsigned char pixel_repeat;

	enum v4l2_ext_hdmi_avi_it_content it_content;
	enum v4l2_ext_hdmi_avi_ext_colorimetry extended_colorimetry;
	enum v4l2_ext_hdmi_avi_rgb_quantization_range rgb_quantization_range;
	enum v4l2_ext_hdmi_avi_ycc_quantization_range ycc_quantization_range;
	enum v4l2_ext_hdmi_avi_content_type content_type;
	enum v4l2_ext_hdmi_avi_additional_colorimetry additional_colorimetry;

	unsigned short top_bar_end_line_number;
	unsigned short bottom_bar_start_line_number;
	unsigned short left_bar_end_pixel_number;
	unsigned short right_bar_end_pixel_number;

	enum v4l2_ext_hdmi_packet_status packet_status;
	struct v4l2_ext_hdmi_in_packet packet;
};

struct v4l2_ext_hdmi_packet_info {
	enum v4l2_ext_hdmi_input_port port;
	enum v4l2_ext_hdmi_mode mode;
	struct v4l2_ext_hdmi_avi_info avi;
	struct v4l2_ext_hdmi_spd_info spd;
	struct v4l2_ext_hdmi_vsi_info vsi;
};

enum v4l2_ext_hdmi_dolby_hdr_type {
	V4L2_EXT_HDMI_DOLBY_HDR_TYPE_SDR = 0,
	V4L2_EXT_HDMI_DOLBY_HDR_TYPE_STANDARD_VSIF_1,
	V4L2_EXT_HDMI_DOLBY_HDR_TYPE_STANDARD_VSIF_2,
	V4L2_EXT_HDMI_DOLBY_HDR_TYPE_LOW_LATENCY,
};

struct v4l2_ext_hdmi_dolby_hdr {
	enum v4l2_ext_hdmi_input_port port;
	enum v4l2_ext_hdmi_dolby_hdr_type type;
};

enum v4l2_ext_hdmi_edid_size {
	V4L2_EXT_HDMI_EDID_SIZE_128 = 0,
	V4L2_EXT_HDMI_EDID_SIZE_256,
	V4L2_EXT_HDMI_EDID_SIZE_512,
};

struct v4l2_ext_hdmi_edid {
	enum v4l2_ext_hdmi_input_port port;
	enum v4l2_ext_hdmi_edid_size size;
	union {
		unsigned char *pdata;//unsigned char *pData;
		unsigned int compat_data;
		unsigned long long sizer;
	};
};

struct v4l2_ext_hdmi_connection_state {
	enum v4l2_ext_hdmi_input_port port;
	unsigned char state;
};

enum v4l2_ext_hdmi_hpd_state {
	V4L2_EXT_HDMI_HPD_DISABLE = 0,
	V4L2_EXT_HDMI_HPD_ENABLE,
	V4L2_EXT_HDMI_HPD_RESTART,
};

struct v4l2_ext_hdmi_hpd {
	enum v4l2_ext_hdmi_input_port port;
	enum v4l2_ext_hdmi_hpd_state hpd_state;
};

enum v4l2_ext_hdmi_hdcp_version {
	V4L2_EXT_HDMI_HDCP_VERSION_14 = 0,
	V4L2_EXT_HDMI_HDCP_VERSION_22,
	V4L2_EXT_HDMI_HDCP_VERSION_RESERVED,
};

struct v4l2_ext_hdmi_hdcp_key {
	enum v4l2_ext_hdmi_input_port port;
	enum v4l2_ext_hdmi_hdcp_version version;
	unsigned int key_size;
	union {
		unsigned char *p_data;
		unsigned int compat_data;
		unsigned long long sizer;
	};
};

struct v4l2_ext_hdmi_vrr_frequency {
	enum v4l2_ext_hdmi_input_port port;
	unsigned short frequency;
};

enum v4l2_ext_hdmi_emp_type {
	V4L2_EXT_HDMI_EMP_TYPE_UNDEFINED = 0,
	V4L2_EXT_HDMI_EMP_TYPE_VSEM,
	V4L2_EXT_HDMI_EMP_TYPE_DYNAMICHDREM,
	V4L2_EXT_HDMI_EMP_TYPE_VTEM,
	V4L2_EXT_HDMI_EMP_TYPE_CVTEM
};

struct v4l2_ext_hdmi_emp_info {
	enum v4l2_ext_hdmi_input_port port;
	enum v4l2_ext_hdmi_emp_type type;
	unsigned short current_packet_index;
	unsigned short total_packet_number;
	unsigned char data[31];
};

struct v4l2_ext_hdmi_hdcp_repeater {
	enum v4l2_ext_hdmi_input_port port;
	unsigned char repeater_mode;
	unsigned char receiver_id[5];
	unsigned char repeater_hpd;
};

struct v4l2_ext_hdmi_hdcp_repeater_topology {
	enum v4l2_ext_hdmi_input_port port;	 // HDMI port number
	unsigned char repeater_mode;			// 0 : receiver, 1 : repeater
	unsigned char count;					// Number of devices
	unsigned char depth;					// Depth of connected device
	unsigned char receiver_id[32][5];	   // RX IO of connected device
	unsigned char repeater_hpd;			 // HPD control
	unsigned int msg_id;	// Increase by 1 when it changes
	//any value(init value =0, default set value = 1)
};

enum v4l2_ext_hdmi_hdcp_repeater_stream_manage_transmit {
	STREAM_MANAGE_TRANSMIT_ALLOW,
	STREAM_MANAGE_TRANSMIT_DENY,
	STREAM_MANAGE_RESERVED,
};

struct v4l2_ext_hdmi_hdcp_repeater_stream_manage {
	enum v4l2_ext_hdmi_input_port port;
	enum v4l2_ext_hdmi_hdcp_repeater_stream_manage_transmit value;	 // 0 or 1 or reserved
};

// HDMI Capabilities Flags
// The HDMI device supports HDCP1.4 Key. So, need to write HDCP1.4 key to driver.
#define V4L2_EXT_HDMI_HDCP14			(0x1 << 0) // 0x0000000000000001
// HDCP1.4 Key is OTP on Soc. So, No need to download HDCP1.4 Key to secure storage.
#define V4L2_EXT_HDMI_HDCP14_KEY_OTP	(0x1 << 1) // 0x0000000000000002
// The HDMI device supports HDCP2.3 key. So, need to write HDCP2.3 key to driver.
// HDCP2.2 key is include in HDCP2.3 key.
#define V4L2_EXT_HDMI_HDCP23		(0x1 << 2) // 0x0000000000000004
// HDCP2.3(HDCP2.2) Key is OTP on Soc. So,
//No need to download HDCP2.3(HDCP2.2) Key to secure storage.
#define V4L2_EXT_HDMI_HDCP23_KEY_OTP (0x1 << 3) // 0x0000000000000008
// The HDMI device supports HDMI1.4 Specification
#define V4L2_EXT_HDMI_HDMI14		(0x1 << 12) // 0x0000000000001000
// The HDMI device supports HDMI2.0 Specification
#define V4L2_EXT_HDMI_HDMI20		(0x1 << 13) // 0x0000000000002000
// The HDMI device supports HDMI2.1 Specification.
// Set this flag even if the device supports one feature in the HDMI2.1 specification.
#define V4L2_EXT_HDMI_HDMI21		(0x1 << 14) // 0x0000000000004000
// The HDMI device supports TMDS 3.4Gbps per Channel
#define V4L2_EXT_HDMI_TMDS_3G		(0x1 << 20) // 0x0000000000100000
// The HDMI device supports TMDS 6.0Gbps per Channel
#define V4L2_EXT_HDMI_TMDS_6G		(0x1 << 21) // 0x0000000000200000
// The HDMI device supports FRL 3Gbps per Lane on 3 Lanes(0,1, and 2)
#define V4L2_EXT_HDMI_FRL_3L_3G		(0x1 << 22) // 0x0000000000400000
// The HDMI device supports FRL 6Gbps per Lane on 3 Lanes(0,1, and 2)
#define V4L2_EXT_HDMI_FRL_3L_6G		(0x1 << 23) // 0x0000000000800000
// The HDMI device supports FRL 6Gbps per Lane on 4 Lanes(0,1, 2, and 3)
#define V4L2_EXT_HDMI_FRL_4L_6G		(0x1 << 24) // 0x0000000001000000
// The HDMI device supports FRL 8Gbps per Lane on 4 Lanes(0,1, 2, and 3)
#define V4L2_EXT_HDMI_FRL_4L_8G		(0x1 << 25) // 0x0000000002000000
// The HDMI device supports FRL 10Gbps per Lane on 4 Lanes(0,1, 2, and 3)
#define V4L2_EXT_HDMI_FRL_4L_10G	(0x1 << 26) // 0x0000000004000000
// The HDMI device supports FRL 12Gbps per Lane on 4 Lanes(0,1, 2, and 3)
#define V4L2_EXT_HDMI_FRL_4L_12G	(0x1 << 27) // 0x0000000008000000

// The HDMI device supports background timing detection.
// It means that driver can get HDMI signal information(timing, SPD, AVI, etc...)
// without watching HDMI port.
// If the HDMI device supports background timing detection,
// it also support fast switching between HDMI ports.
#define V4L2_EXT_HDMI_BACKGROUND_TIMING_DETECT (0x1 << 28) // 0x0000000010000000

// The HDMI device supports FVA
#define V4L2_EXT_HDMI_FVA (0x1 << 29) // 0x0000000020000000
// The HDMI device supports VRR
#define V4L2_EXT_HDMI_VRR (0x1 << 30) // 0x0000000040000000
// The HDMI device supports CinemaVRR
#define V4L2_EXT_HDMI_CINEMAVRR (0x1 << 31) // 0x0000000080000000

// The HDMI device supports  Dolby vision
#define V4L2_EXT_HDMI_DOLBY_VISION (0x1 << 32) // 0x0000000100000000
// The HDMI device supports  Dolby vision low latency mode
#define V4L2_EXT_HDMI_DOLBY_VISION_LL (0x1 << 33) // 0x0000000200000000
// The HDMI device supports  Dolby vision on HDMI2.1
#define V4L2_EXT_HDMI_DOLBY_VISION_HDMI21 (0x1 << 34) // 0x0000000400000000
// The HDMI device supports 512bytes EDID
#define V4L2_EXT_HDMI_512BYTE_EDID (0x1 << 35) // 0x0000000800000000
// The HDMI device supports EMPACKET
#define V4L2_EXT_HDMI_EMPACKET (0x1 << 36) // 0x0000001000000000
// The HDMI device supports HDCP Repeater
#define V4L2_EXT_HDMI_HDCP_REPEATER (0x1 << 37) // 0x0000002000000000
// The HDMI device supports DSC
#define V4L2_EXT_HDMI_DSC (0x1 << 38) // 0x0000004000000000

struct v4l2_ext_hdmi_capability {
	unsigned char chip[16];		  // chip name. for example "e60"
	unsigned int version;			// linuxtv header version
	unsigned long long capabilities; // HDMI Capabilities Flags
	unsigned long long reserved[3];
};

struct v4l2_ext_hdmi_querycap {
	enum v4l2_ext_hdmi_input_port port;
	struct v4l2_ext_hdmi_capability hdmi_capability;
};

enum  v4l2_ext_hdmi_sleep_mode {
	V4L2_EXT_HDMI_SLEEP_MODE = 0,
	V4L2_EXT_HDMI_WAKEUP_MODE,
};

struct v4l2_ext_hdmi_sleep {
	enum v4l2_ext_hdmi_input_port port;
	enum v4l2_ext_hdmi_sleep_mode mode;
};

/* HDMI DIAGNOSTICS */
enum v4l2_ext_hdmi_link_type {
	V4L2_EXT_HDMI_LINK_TYPE_TMDS = 0,
	V4L2_EXT_HDMI_LINK_TYPE_FRL,
};

enum v4l2_ext_hdmi_link_lane_number {
	V4L2_EXT_HDMI_LINK_LANE_NUMBER_3 = 3,
	V4L2_EXT_HDMI_LINK_LANE_NUMBER_4 = 4,
};

enum v4l2_ext_hdmi_link_rate {
	V4L2_EXT_HDMI_LINK_RATE_3G  = 3,
	V4L2_EXT_HDMI_LINK_RATE_6G  = 6,
	V4L2_EXT_HDMI_LINK_RATE_8G  = 8,
	V4L2_EXT_HDMI_LINK_RATE_10G = 10,
	V4L2_EXT_HDMI_LINK_RATE_12G = 12,

};

struct v4l2_ext_hdmi_phy_status {
	enum v4l2_ext_hdmi_input_port port;
	unsigned char lock_status;
	unsigned int tmds_clk_khz;

	enum v4l2_ext_hdmi_link_type link_type;
	enum v4l2_ext_hdmi_link_lane_number link_lane;
	enum v4l2_ext_hdmi_link_rate link_rate;

	unsigned int ctle_eq_min_range[V4L2_EXT_HDMI_TMDS_CH_NUM];
	unsigned int ctle_eq_max_range[V4L2_EXT_HDMI_TMDS_CH_NUM];
	unsigned int ctle_eq_result[V4L2_EXT_HDMI_TMDS_CH_NUM];
	unsigned int error[V4L2_EXT_HDMI_TMDS_CH_NUM];
};

enum v4l2_ext_hdmi_audio_format {
	V4L2_EXT_HDMI_AUDIO_FORMAT_UNKNOWN	  = 0x00,
	V4L2_EXT_HDMI_AUDIO_FORMAT_PCM		  = 0x01,
	V4L2_EXT_HDMI_AUDIO_FORMAT_AC3		  = 0x10,
	V4L2_EXT_HDMI_AUDIO_FORMAT_EAC3		 = 0x11,
	V4L2_EXT_HDMI_AUDIO_FORMAT_EAC3_ATMOS   = 0x12,
	V4L2_EXT_HDMI_AUDIO_FORMAT_MAT		  = 0x15,
	V4L2_EXT_HDMI_AUDIO_FORMAT_MAT_ATMOS	= 0x16,
	V4L2_EXT_HDMI_AUDIO_FORMAT_TRUEHD	   = 0x17,
	V4L2_EXT_HDMI_AUDIO_FORMAT_TRUEHD_ATMOS = 0x18,
	V4L2_EXT_HDMI_AUDIO_FORMAT_AAC		  = 0x19,
	V4L2_EXT_HDMI_AUDIO_FORMAT_MPEG		 = 0x20,
	V4L2_EXT_HDMI_AUDIO_FORMAT_DTS		  = 0x30,
	V4L2_EXT_HDMI_AUDIO_FORMAT_DTS_HD_MA	= 0x31,
	V4L2_EXT_HDMI_AUDIO_FORMAT_DTS_EXPRESS  = 0x32,
	V4L2_EXT_HDMI_AUDIO_FORMAT_DTS_CD	   = 0x33,
	V4L2_EXT_HDMI_AUDIO_FORMAT_NOAUDIO	  = 0x41,
};

struct v4l2_ext_hdmi_link_status {
	enum v4l2_ext_hdmi_input_port port;
	unsigned char hpd;
	unsigned char hdmi_5v;
	unsigned char rx_sense;
	unsigned int frame_rate_x100_hz;
	enum v4l2_ext_hdmi_mode dvi_hdmi_mode;

	unsigned short video_width;
	unsigned short video_height;
	enum v4l2_ext_hdmi_avi_csc color_space;
	unsigned char color_depth;
	enum v4l2_ext_hdmi_avi_colorimetry colorimetry;
	enum v4l2_ext_hdmi_avi_ext_colorimetry ext_colorimetry;
	enum v4l2_ext_hdmi_avi_additional_colorimetry additional_colorimetry;
	enum v4l2_ext_hdr_mode hdr_type;

	enum v4l2_ext_hdmi_audio_format audio_format;
	unsigned int audio_sampling_freq;
	unsigned char audio_channel_number;
};

struct v4l2_ext_hdmi_video_status {
	enum v4l2_ext_hdmi_input_port port;
	unsigned short video_width_real;
	unsigned short video_htotal_real;
	unsigned short video_height_real;
	unsigned short video_vtotal_real;
	unsigned int pixel_clock_khz;
	unsigned int current_vrr_refresh_rate;
};

struct v4l2_ext_hdmi_audio_status {
	enum v4l2_ext_hdmi_input_port port;
	unsigned int pcm_N;
	unsigned int pcm_CTS;
	unsigned char LayoutBitValue;
	unsigned char ChannelStatusBits;
};

enum v4l2_ext_hdmi_hdcp_auth_status {
	V4L2_EXT_HDMI_HDCP_AUTH_STATUS_NO_TX_CONNECTED = 0,
	V4L2_EXT_HDMI_HDCP_AUTH_STATUS_UNAUTHENTICATED = 1,
	V4L2_EXT_HDMI_HDCP_AUTH_STATUS_IN_PROGRESS	 = 2,
	V4L2_EXT_HDMI_HDCP_AUTH_STATUS_AUTHENTICATED   = 3,
};

struct v4l2_ext_hdmi_hdcp14_status {
	enum v4l2_ext_hdmi_input_port port;
	unsigned char An[8];
	unsigned char Aksv[5];
	unsigned char Bksv[5];
	unsigned char Ri[2];
	unsigned char Bcaps;
	unsigned char Bstatus[2];
};

struct v4l2_ext_hdmi_hdcp22_status {
	enum v4l2_ext_hdmi_input_port port;
	unsigned short ake_init_count_since_5v;
	unsigned short reauth_req_count_since_5v;
};

struct v4l2_ext_hdmi_hdcp_status {
	enum v4l2_ext_hdmi_input_port port;
	enum v4l2_ext_hdmi_hdcp_version hdcp_version;
	enum v4l2_ext_hdmi_hdcp_auth_status auth_status;
	unsigned char encEn;
	struct v4l2_ext_hdmi_hdcp14_status hdcp14_status;
	struct v4l2_ext_hdmi_hdcp22_status hdcp22_status;
};

struct v4l2_ext_hdmi_scdc_status {
	enum v4l2_ext_hdmi_input_port port;
	unsigned char source_version;
	unsigned char sink_version;

	unsigned char rsed_update;
	unsigned char flt_update;
	unsigned char frl_start;
	unsigned char source_test_update;
	unsigned char rr_test;
	unsigned char ced_update;
	unsigned char status_update;

	unsigned char tmds_bit_clock_ratio;
	unsigned char scrambling_enable;
	unsigned char tmds_scrambler_status;

	unsigned char flt_no_retrain;
	unsigned char rr_enable;
	unsigned char ffe_levels;
	unsigned char frl_rate;

	unsigned char dsc_decode_fail;
	unsigned char flt_ready;
	unsigned char clk_detect;
	unsigned char ch0_locked;
	unsigned char ch1_locked;
	unsigned char ch2_locked;
	unsigned char ch3_locked;

	unsigned char lane0_ltp_request;
	unsigned char lane1_ltp_request;
	unsigned char lane2_ltp_request;
	unsigned char lane3_ltp_request;

	unsigned char ch0_ced_valid;
	unsigned char ch1_ced_valid;
	unsigned char ch2_ced_valid;
	unsigned char ch3_ced_valid;
	unsigned int ch0_ced;
	unsigned int ch1_ced;
	unsigned int ch2_ced;
	unsigned int ch3_ced;
	unsigned char rs_correction_valid;
	unsigned int rs_correcton_count;
};

struct v4l2_ext_hdmi_diagnostics_status {
	enum v4l2_ext_hdmi_input_port port;
	struct v4l2_ext_hdmi_link_status link_status;
	struct v4l2_ext_hdmi_phy_status phy_status;
	struct v4l2_ext_hdmi_video_status video_status;
	struct v4l2_ext_hdmi_audio_status audio_status;
	struct v4l2_ext_hdmi_hdcp_status hdcp_status;
	struct v4l2_ext_hdmi_scdc_status scdc_status;
};

enum v4l2_ext_hdmi_error_type {
	V4L2_EXT_HDMI_ERROR_TYPE_NONE		  = 0x0000,
	V4L2_EXT_HDMI_ERROR_TYPE_GCP_ERROR	 = 0x0001,
	V4L2_EXT_HDMI_ERROR_TYPE_HDCP22_REAUTH = 0x0002,
	V4L2_EXT_HDMI_ERROR_TYPE_TMDS_ERROR	= 0x0004,
	V4L2_EXT_HDMI_ERROR_TYPE_PHY_LOW_RANGE = 0x0008,
	V4L2_EXT_HDMI_ERROR_TYPE_PHY_ABNORMAL  = 0x0010,
	V4L2_EXT_HDMI_ERROR_TYPE_CED_ERROR	 = 0x0020,
	V4L2_EXT_HDMI_ERROR_TYPE_AUDIO_BUFFER  = 0x0040,
	V4L2_EXT_HDMI_ERROR_TYPE_UNSTABLE_SYNC = 0x0080,
	V4L2_EXT_HDMI_ERROR_TYPE_BCH		   = 0x0100,
	V4L2_EXT_HDMI_ERROR_TYPE_FLT		   = 0x0200,
	V4L2_EXT_HDMI_ERROR_TYPE_FAILED		= 0xFFFFFFFE,
};

struct v4l2_ext_hdmi_error_status {
	enum v4l2_ext_hdmi_input_port port;
	enum v4l2_ext_hdmi_error_type error;
	unsigned int param1;
	unsigned int param2;
};

enum v4l2_ext_hdmi_expert_setting_type {
	V4L2_EXT_HDMI_EXPERT_SETTING_TYPE_HPD_LOW_DURATION = 0,

	V4L2_EXT_HDMI_EXPERT_SETTING_TYPE_TMDS_MANUAL_EQ_MODE,
	V4L2_EXT_HDMI_EXPERT_SETTING_TYPE_TMDS_MANUAL_EQ_CH0,
	V4L2_EXT_HDMI_EXPERT_SETTING_TYPE_TMDS_MANUAL_EQ_CH1,
	V4L2_EXT_HDMI_EXPERT_SETTING_TYPE_TMDS_MANUAL_EQ_CH2,
	V4L2_EXT_HDMI_EXPERT_SETTING_TYPE_TMDS_MANUAL_EQ_CH3,
	V4L2_EXT_HDMI_EXPERT_SETTING_TYPE_TMDS_EQ_PERIOD,

	V4L2_EXT_HDMI_EXPERT_SETTING_TYPE_VIDEO_STABLE_COUNT,
	V4L2_EXT_HDMI_EXPERT_SETTING_TYPE_AUDIO_STABLE_COUNT,

	V4L2_EXT_HDMI_EXPERT_SETTING_TYPE_DISABLE_HDCP22,
	V4L2_EXT_HDMI_EXPERT_SETTING_TYPE_REAUTH_HDCP22,

	V4L2_EXT_HDMI_EXPERT_SETTING_TYPE_ON_TO_RXSENSE_TIME,
	V4L2_EXT_HDMI_EXPERT_SETTING_TYPE_RXSENSE_TO_HPD_TIME
};

struct v4l2_ext_hdmi_expert_setting {
	enum v4l2_ext_hdmi_input_port port;
	enum v4l2_ext_hdmi_expert_setting_type type;
	unsigned int param1;
	unsigned int param2;
	unsigned int param3;
};

/* ADC */
enum v4l2_ext_adc_input_src {
	V4L2_EXT_ADC_INPUT_SRC_NONE = 0,
	V4L2_EXT_ADC_INPUT_SRC_COMP,
	V4L2_EXT_ADC_INPUT_SRC_RGB,
	V4L2_EXT_ADC_INPUT_SRC_COMP_RESERVED2,
};

struct v4l2_ext_adc_timing_info {
	unsigned short h_freq;
	unsigned short v_freq;
	unsigned short h_total;
	unsigned short v_total;
	unsigned short h_porch;
	unsigned short v_porch;
	struct v4l2_ext_video_rect active;
	unsigned short scan_type;
	unsigned short phase;
};

struct v4l2_ext_adc_calibration_data {
	unsigned short r_gain;
	unsigned short g_gain;
	unsigned short b_gain;
	unsigned short r_offset;
	unsigned short g_offset;
	unsigned short b_offset;
};

/* AVD */
enum v4l2_ext_avd_input_src {
	V4L2_EXT_AVD_INPUT_SRC_NONE = 0,
	V4L2_EXT_AVD_INPUT_SRC_ATV,
	V4L2_EXT_AVD_INPUT_SRC_AV,
	V4L2_EXT_AVD_INPUT_SRC_AVD_RESERVED1,
	V4L2_EXT_AVD_INPUT_SRC_AVD_RESERVED2,
};

struct v4l2_ext_avd_timing_info {
	unsigned short h_freq;
	unsigned short v_freq;
	unsigned short h_porch;
	unsigned short v_porch;
	struct v4l2_ext_video_rect active;
	unsigned char vd_lock;
	unsigned char h_lock;
	unsigned char v_lock;
};

/* VBI */
/* CGMS copy protection enum */
enum v4l2_ext_vbi_cgms {
	V4L2_EXT_VBI_CGMS_PERMIT = 0,
	V4L2_EXT_VBI_CGMS_ONCE,
	V4L2_EXT_VBI_CGMS_RESERVED,
	V4L2_EXT_VBI_CGMS_NO_PERMIT,
};

/* APS copy protection enum */
enum v4l2_ext_vbi_aps {
	V4L2_EXT_VBI_APS_OFF = 0,
	V4L2_EXT_VBI_APS_ON_BURST_OFF,
	V4L2_EXT_VBI_APS_ON_BURST_2,
	V4L2_EXT_VBI_APS_ON_BURST_4,
};

/* MACROVISION copy protection enum */
enum v4l2_ext_vbi_macrovision {
	V4L2_EXT_VBI_MACROVISION_PSP_OFF = 0,
	V4L2_EXT_VBI_MACROVISION_PSP_ON_BURST_OFF,
	V4L2_EXT_VBI_MACROVISION_PSP_ON_BURST_2,
	V4L2_EXT_VBI_MACROVISION_PSP_ON_BURST_4,
};

struct v4l2_ext_vbi_copy_protection {
	enum v4l2_ext_vbi_cgms cgms_cp_info;
	enum v4l2_ext_vbi_aps aps_cp_info;
	enum v4l2_ext_vbi_macrovision macrovision_cp_info;
};

/* VDEC */

/* VDEC class control values V4L2_CID_EXT_VDEC_DECODING_SPEED*/

#define V4L2_EXT_VDEC_DECODING_SPEED_PAUSE 0

#define V4L2_EXT_VDEC_DECODING_SPEED_SLOW 500

#define V4L2_EXT_VDEC_DECODING_SPEED_NORMAL 1000

#define V4L2_EXT_VDEC_DECODING_SPEED_2x 2000

/* VDEC class control values V4L2_CID_EXT_VDEC_DECODE_MODE */

#define V4L2_EXT_VDEC_DECODE_I_FRAME (1)

#define V4L2_EXT_VDEC_DECODE_IP_FRAME (2)

#define V4L2_EXT_VDEC_DECODE_ALL_FRAME (3)

/* VDEC State for v4l2_ext_decoder_status vdecState*/

#define V4L2_EXT_VDEC_STATE_UNINIT (0)

#define V4L2_EXT_VDEC_STATE_INIT (1)

#define V4L2_EXT_VDEC_STATE_PLAYING (2)

#define V4L2_EXT_VDEC_STATE_STOPPED (3)

#define V4L2_EXT_VDEC_STATE_FREEZED (4)

/* VDEC class control values for V4L2_CID_EXT_VDEC_VIDEO_INFO */

enum v4l2_ext_vdec_video_info {
	VDEC_V_SIZE = 0,
	VDEC_H_SIZE,
	VDEC_VIDEO_FORMAT,
	VDEC_FRAME_RATE,
	VDEC_ASPECT_RATIO,
	VDEC_BIT_RATE,
	VDEC_PROGRESSIVE_SEQUENCE
};

/* VDEC class control values for V4L2_CID_EXT_VDEC_DECODER_INFO */

struct v4l2_ext_decoder_status {
	__u8 vdec_state;
	__u8 codec_type;
	__u8 av_libsync;
	__u8 still_picture;
	__u8 drip_decoding;
	__u32 es_data_size;
	__u32 decoded_pic_num;
	__u32 mb_dec_error_count;
	__u32 pts_matched;
};

/* VDEC class control values for V4L2_CID_EXT_VDEC_STREAM_INFO */

struct v4l2_ext_stream_info {
	__u32 v_size;
	__u32 h_size;
	__u32 video_format;
	__u32 frame_rate;
	__u32 aspect_ratio;
	__u32 bit_rate;
	__u32 sequence_header_scan_type;
	__u32 frame_scan_type;
	__u32 picture_type;
	__u32 temporal_reference;
	__u32 picture_struct;
	__u32 top_field_first;
	__u32 repeat_field_first;
};

/* VDEC class control values for V4L2_CID_EXT_VDEC_PICINFO_EVENT_DATA */

struct v4l2_ext_picinfo_msg {
	__u8 version; // picture info msg version
	__u8 channel; // VDEC driver channel info

	__u16 frame_rate;		// Frame rate numerator
	__u16 aspect_ratio;	  // Aspect ratio
	__u16 h_size;			// Luminance resolution h size
	__u16 v_size;			// Luminance resolution v size
	__u16 bitrate;		   // Input Bitrate
	__u8 afd;				// Active_format
	__u16 progressive_seq;   // GOP Scan type(Progressive/interlace)
	__u16 progressive_frame; // Frame Scan type(Progressive/interlace)
	__u16 active_x;		  // Active x stard point
	__u16 active_y;		  // Active y stard point
	__u16 active_w;		  // Active width
	__u16 active_h;		  // Active height
	__u16 display_h_size;	// Display H size
	__u16 display_v_size;	// Display V size
	__u8 aspect_ratio_idc;   // VUI info, aspect_ratio_idc
	__u32 sar_width;		 // VUI info, sar_width
	__u32 sar_height;		// VUI info, sar_height

	__u8 three_d_info; // 3D info

	__u32 colour_primaries;	  // VUI info, colour_primaries
	__u32 transfer_chareristics; // VUI info, transfer_characteristics
	__u32 matrix_coeffs;		 // VUI info, matrix_coefficients
	__u32 display_primaries_x0;  // SEI info, display_primaries_x0
	__u32 display_primaries_y0;  // SEI info, display_primaries_y0
	__u32 display_primaries_x1;  // SEI info, display_primaries_x1
	__u32 display_primaries_y1;  // SEI info, display_primaries_y1
	__u32 display_primaries_x2;  // SEI info, display_primaries_x2
	__u32 display_primaries_y2;  // SEI info, display_primaries_y2
	__u32 white_point_x;		 // SEI info, white_point_x
	__u32 white_point_y;		 // SEI info, white_point_y
	__u32 max_display_mastering_luminance; // SEI info, max_display_mastering_luminance
	__u32 min_display_mastering_luminance; // SEI info, min_display_mastering_luminance
	__u8 overscan_appropriate;		   // VUI info, overscan_appropriate_flag
	__u32 video_full_range_flag;		 // VUI info, video_full_range_flag
	__u32 hdr_transfer_characteristic_idc; // SEI info, preferred_transfer_characteristics
};

/* VDEC class control values for V4L2_CID_EXT_VDEC_PICINFO_EVENT_DATA_EXT */

struct v4l2_ext_picinfo_msg_ext {
	__u8 channel;
	__u16 aspect_ratio;
	__u16 h_size;
	__u16 v_size;
	__u16 bitrate;
	__u8 afd;
	__u16 progressive_seq;
	__u16 progressive_frame;
	__u16 active_x;
	__u16 active_y;
	__u16 active_w;
	__u16 active_h;
	__u16 display_h_size;
	__u16 display_v_size;
	__u8 aspect_ratio_idc;
	__u32 sar_width;
	__u32 sar_height;

	__u8 three_d_info;

	__u32 colour_primaries;
	__u32 transfer_chareristics;
	__u32 matrix_coeffs;
	__u32 display_primaries_x0;
	__u32 display_primaries_y0;
	__u32 display_primaries_x1;
	__u32 display_primaries_y1;
	__u32 display_primaries_x2;
	__u32 display_primaries_y2;
	__u32 white_point_x;
	__u32 white_point_y;
	__u32 max_display_mastering_luminance;
	__u32 min_display_mastering_luminance;
	__u8 overscan_appropriate;
	__u32 video_full_range_flag;
	__u32 hdr_transfer_characteristic_idc;

	struct v4l2_fract frame_rate;
};

/* VDEC class control values for V4L2_CID_EXT_VDO_VDEC_CONNECTING */

struct v4l2_ext_vdec_vdo_connection {
	__u32 vdo_port;
	__u32 vdec_port;
};

/* VDEC class control values for v4l2_ext_decoded_picture_buffer */

struct v4l2_ext_decoded_picture_buffer {
	__u32 command;
	__u64 target_pts;
	__u32 bFrozen;
	__u32 hashValue;
};

/* Capture */
enum v4l2_ext_capture_video_frame_buffer_pixel_format {
	V4L2_EXT_CAPTURE_VIDEO_FRAME_BUFFER_PIXEL_FORMAT_YUV420_PLANAR = 0,
	V4L2_EXT_CAPTURE_VIDEO_FRAME_BUFFER_PIXEL_FORMAT_YUV420_SEMI_PLANAR,
	V4L2_EXT_CAPTURE_VIDEO_FRAME_BUFFER_PIXEL_FORMAT_YUV420_INTERLEAVED,
	V4L2_EXT_CAPTURE_VIDEO_FRAME_BUFFER_PIXEL_FORMAT_YUV422_PLANAR,
	V4L2_EXT_CAPTURE_VIDEO_FRAME_BUFFER_PIXEL_FORMAT_YUV422_SEMI_PLANAR,
	V4L2_EXT_CAPTURE_VIDEO_FRAME_BUFFER_PIXEL_FORMAT_YUV422_INTERLEAVED,
	V4L2_EXT_CAPTURE_VIDEO_FRAME_BUFFER_PIXEL_FORMAT_YUV444_PLANAR,
	V4L2_EXT_CAPTURE_VIDEO_FRAME_BUFFER_PIXEL_FORMAT_YUV444_SEMI_PLANAR,
	V4L2_EXT_CAPTURE_VIDEO_FRAME_BUFFER_PIXEL_FORMAT_YUV444_INTERLEAVED,
	V4L2_EXT_CAPTURE_VIDEO_FRAME_BUFFER_PIXEL_FORMAT_RGB,
	V4L2_EXT_CAPTURE_VIDEO_FRAME_BUFFER_PIXEL_FORMAT_ARGB
};

enum v4l2_ext_capture_video_frame_buffer_plane_num {
	V4L2_EXT_CAPTURE_VIDEO_FRAME_BUFFER_PLANE_INTERLEAVED =
		1, // INTERLEAVED has one plane.
	V4L2_EXT_CAPTURE_VIDEO_FRAME_BUFFER_PLANE_SEMI_PLANAR, // SEMI PLANAR has two plane.
	V4L2_EXT_CAPTURE_VIDEO_FRAME_BUFFER_PLANE_PLANAR // PLANAR has three plane.
};

struct v4l2_ext_capture_capability_info {
	unsigned int flags;
	unsigned int scale_up_limit_w;
	unsigned int scale_up_limit_h;
	unsigned int scale_down_limit_w;
	unsigned int scale_down_limit_h;
	unsigned int num_video_frame_buffer;
	struct v4l2_ext_video_rect max_res;
	enum v4l2_ext_capture_video_frame_buffer_plane_num num_plane;
	enum v4l2_ext_capture_video_frame_buffer_pixel_format pixel_format;
};

/* Flags for the 'flags' field in struct v4l2_ext_capture_capability_info. */
#define V4L2_EXT_CAPTURE_CAP_INPUT_VIDEO_DEINTERLACE 0x0001
#define V4L2_EXT_CAPTURE_CAP_DISPLAY_VIDEO_DEINTERLACE 0x0002
#define V4L2_EXT_CAPTURE_CAP_SCALE_UP 0x0004
#define V4L2_EXT_CAPTURE_CAP_SCALE_DOWN 0x0008
#define V4L2_EXT_CAPTURE_CAP_DIVIDE_FRAMERATE 0x0010
#define V4L2_EXT_CAPTURE_CAP_VIDEOTOVIDEO 0x0020

struct v4l2_ext_capture_plane_info {
	unsigned int stride;
	struct v4l2_ext_video_rect plane_region;
	struct v4l2_ext_video_rect active_region;
};

enum v4l2_ext_capture_video_scan_type {
	V4L2_EXT_CAPTURE_VIDEO_INTERLACED = 0,
	V4L2_EXT_CAPTURE_VIDEO_PROGRESSIVE
};

struct v4l2_ext_capture_video_win_info {
	enum v4l2_ext_capture_video_scan_type type;
	struct v4l2_ext_video_rect in;
	struct v4l2_ext_video_rect out;
	struct v4l2_ext_video_rect panel;
};

//V4L2_EXT_CAPTURE_SCALER_INPUT = 0,
//AML:TVIN_PORT_VIU1_WB0_VD1
//V4L2_EXT_CAPTURE_SCALER_OUTPUT,
//AML:no match point,force to TVIN_PORT_VIU1_WB0_VD1
//V4L2_EXT_CAPTURE_DISPLAY_OUTPUT,
//AML:TVIN_PORT_VIU1_WB0_VPP
//V4L2_EXT_CAPTURE_BLENDED_OUTPUT,
//AML:TVIN_PORT_VIU1_WB0_OSD1or2
//V4L2_EXT_CAPTURE_OSD_OUTPUT
enum v4l2_ext_capture_location {
	V4L2_EXT_CAPTURE_SCALER_INPUT = 0, // for VTV - before de-interlace
	V4L2_EXT_CAPTURE_SCALER_OUTPUT,
	V4L2_EXT_CAPTURE_DISPLAY_OUTPUT,
	V4L2_EXT_CAPTURE_BLENDED_OUTPUT,
	V4L2_EXT_CAPTURE_OSD_OUTPUT
};

enum v4l2_ext_capture_buf_location {
	V4L2_EXT_CAPTURE_INPUT_BUF = 0,
	V4L2_EXT_CAPTURE_OUTPUT_BUF
};

struct v4l2_ext_capture_plane_prop {
	enum v4l2_ext_capture_location l;
	struct v4l2_ext_video_rect plane;/* reserved */
	int buf_count;/* reserved */
};

struct v4l2_ext_capture_freeze_mode {
	int plane_index;
	unsigned char val;
};

struct v4l2_ext_capture_physical_memory_info {
	int buf_index;
	union {
		unsigned int *y;
		unsigned int compat_y_data;
		unsigned long long y_sizer;
	};

	union {
		unsigned int *c;
		unsigned int compat_c_data;
		unsigned long long c_sizer;
	};
	enum v4l2_ext_capture_buf_location buf_location;
};

/*eARC*/
/* Number of eARC capability bytes*/
#define V4L2_EXT_EARC_CAPABILITY_BYTES 256

/*eARC ERX_LATENCY_REQ Type*/
#define V4L2_EXT_EARC_ERX_LATENCY_REQ_MINIMIZE	\
	0 // 0: request to minimize eARC RX latency
#define V4L2_EXT_EARC_ERX_LATENCY_REQ_NON_SYNC	\
	254 // 254: No synchronization required
#define V4L2_EXT_EARC_ERX_LATENCY_REQ_UNKNOWN	\
	255 // 255: unknown eARC TX latency

enum v4l2_ext_earc_output_port {
	V4L2_EXT_EARC_OUTPUT_PORT_NONE = 0,
	V4L2_EXT_EARC_OUTPUT_PORT_1,
	V4L2_EXT_EARC_OUTPUT_PORT_2,
	V4L2_EXT_EARC_OUTPUT_PORT_3,
	V4L2_EXT_EARC_OUTPUT_PORT_4,
	V4L2_EXT_EARC_OUTPUT_PORT_ALL,
};

enum v4l2_ext_earc_enable {
	V4L2_EXT_EARC_DISABLE = 0,
	V4L2_EXT_EARC_ENABLE  = 1,
};

struct v4l2_ext_earc {
	enum v4l2_ext_earc_output_port port;
	enum v4l2_ext_earc_enable earc_enable_state;
};

enum v4l2_ext_earc_status {
	V4L2_EXT_EARC_IDLE1 = 0,
	V4L2_EXT_EARC_IDLE2 = 1,
	V4L2_EXT_EARC_DISC1 = 2,
	V4L2_EXT_EARC_DISC2 = 3,
	V4L2_EXT_EARC_EARC  = 4,
};

struct v4l2_ext_earc_connection_info {
	enum v4l2_ext_earc_output_port port;
	enum v4l2_ext_earc_status status;
	unsigned char capability[V4L2_EXT_EARC_CAPABILITY_BYTES];	// Short Audio
	// Description Max
	// 256 byte
	unsigned char erx_latency_req;	// 0~255
	unsigned char erx_latency;	// 0~255
};
#endif
