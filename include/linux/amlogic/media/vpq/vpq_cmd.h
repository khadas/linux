/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __VPQ_CMD_H__
#define __VPQ_CMD_H__

#define VPQ_PRE_GAMMA_TABLE_LEN		(65)
#define VPQ_GAMMA_TABLE_LEN			(256)
#define VPQ_HIST_BIN_COUNT			(64)
#define VPQ_COLOR_HIST_BIN_COUNT	(32)

struct vpq_pqtable_bin_param_s {
	unsigned int index;
	unsigned int len;
	union {
		void *ptr;
		long long ptr_length;
	};
};

struct vpq_gamma_table_s {
	unsigned short r_data[VPQ_GAMMA_TABLE_LEN];
	unsigned short g_data[VPQ_GAMMA_TABLE_LEN];
	unsigned short b_data[VPQ_GAMMA_TABLE_LEN];
};

struct vpq_blend_gamma_s {
	unsigned char gamma_curve;
	unsigned char ctemp_mode;
};

struct vpq_pre_gamma_table_s {
	unsigned short r_data[VPQ_PRE_GAMMA_TABLE_LEN];
	unsigned short g_data[VPQ_PRE_GAMMA_TABLE_LEN];
	unsigned short b_data[VPQ_PRE_GAMMA_TABLE_LEN];
};

struct vpq_rgb_ogo_s {
	unsigned int en;
	int r_pre_offset;  //range -1024~+1023, default is 0
	int g_pre_offset;  //range -1024~+1023, default is 0
	int b_pre_offset;  //range -1024~+1023, default is 0
	unsigned int r_gain; //range 0~2047, default is 1024 (1.0x)
	unsigned int g_gain; //range 0~2047, default is 1024 (1.0x)
	unsigned int b_gain; //range 0~2047, default is 1024 (1.0x)
	int r_post_offset; //range -1024~+1023, default is 0
	int g_post_offset; //range -1024~+1023, default is 0
	int b_post_offset; //range -1024~+1023, default is 0
};

enum vpq_cms_color_type_e {
	VPQ_CM_9_COLOR = 0,
	VPQ_CM_14_COLOR,
	VPQ_CM_MAX_COLOR,
};

enum vpq_cms_9_color_e {
	VPQ_COLOR_RED = 0,
	VPQ_COLOR_GREEN,
	VPQ_COLOR_BLUE,
	VPQ_COLOR_CYAN,
	VPQ_COLOR_MAGENTA,
	VPQ_COLOR_YELLOW,
	VPQ_COLOR_SKIN,
	VPQ_COLOR_YELLOW_GREEN,
	VPQ_COLOR_BLUE_GREEN,
	VPQ_COLOR_MAX,
};

enum vpq_cms_14_color_e {
	VPQ_COLOR_14_BLUE_PURPLE = 0,
	VPQ_COLOR_14_PURPLE,
	VPQ_COLOR_14_PURPLE_RED,
	VPQ_COLOR_14_RED,
	VPQ_COLOR_14_FLESHTONE_CHEEKS,
	VPQ_COLOR_14_FLESHTONE_HAIR_CHEEKS,
	VPQ_COLOR_14_FLESHTONE_YELLOW,
	VPQ_COLOR_14_YELLOW,
	VPQ_COLOR_14_YELLOW_GREEN,
	VPQ_COLOR_14_GREEN,
	VPQ_COLOR_14_GREEN_CYAN,
	VPQ_COLOR_14_CYAN,
	VPQ_COLOR_14_CYAN_BLUE,
	VPQ_COLOR_14_BLUE,
	VPQ_COLOR_14_MAX,
};

enum vpq_cms_type_e {
	VPQ_CMS_SAT = 0,
	VPQ_CMS_HUE,
	VPQ_CMS_LUMA,
	VPQ_CMS_MAX,
};

struct vpq_cms_s {
	enum vpq_cms_color_type_e color_type;
	enum vpq_cms_9_color_e color_9;
	enum vpq_cms_14_color_e color_14;
	enum vpq_cms_type_e cms_type;
	int value;
};

enum vpq_csc_type_e {
	VPQ_CSC_MATRIX_NULL                = 0,
	VPQ_CSC_MATRIX_RGB_YUV601          = 0x1,
	VPQ_CSC_MATRIX_RGB_YUV601F         = 0x2,
	VPQ_CSC_MATRIX_RGB_YUV709          = 0x3,
	VPQ_CSC_MATRIX_RGB_YUV709F         = 0x4,
	VPQ_CSC_MATRIX_YUV601_RGB          = 0x10,
	VPQ_CSC_MATRIX_YUV601_YUV601F      = 0x11,
	VPQ_CSC_MATRIX_YUV601_YUV709       = 0x12,
	VPQ_CSC_MATRIX_YUV601_YUV709F      = 0x13,
	VPQ_CSC_MATRIX_YUV601F_RGB         = 0x14,
	VPQ_CSC_MATRIX_YUV601F_YUV601      = 0x15,
	VPQ_CSC_MATRIX_YUV601F_YUV709      = 0x16,
	VPQ_CSC_MATRIX_YUV601F_YUV709F     = 0x17,
	VPQ_CSC_MATRIX_YUV709_RGB          = 0x20,
	VPQ_CSC_MATRIX_YUV709_YUV601       = 0x21,
	VPQ_CSC_MATRIX_YUV709_YUV601F      = 0x22,
	VPQ_CSC_MATRIX_YUV709_YUV709F      = 0x23,
	VPQ_CSC_MATRIX_YUV709F_RGB         = 0x24,
	VPQ_CSC_MATRIX_YUV709F_YUV601      = 0x25,
	VPQ_CSC_MATRIX_YUV709F_YUV709      = 0x26,
	VPQ_CSC_MATRIX_YUV601L_YUV709L     = 0x27,
	VPQ_CSC_MATRIX_YUV709L_YUV601L     = 0x28,
	VPQ_CSC_MATRIX_YUV709F_YUV601F     = 0x29,
	VPQ_CSC_MATRIX_BT2020YUV_BT2020RGB = 0x40,
	VPQ_CSC_MATRIX_BT2020RGB_709RGB,
	VPQ_CSC_MATRIX_BT2020RGB_CUSRGB,
	VPQ_CSC_MATRIX_BT2020YUV_BT2020RGB_DYNAMIC = 0x50,
	VPQ_CSC_MATRIX_BT2020YUV_BT2020RGB_CUVA    = 0x51,
	VPQ_CSC_MATRIX_DEFAULT_CSCTYPE             = 0xffff,
};

struct vpq_histgm_ave_s {
	unsigned int sum;
	int width;
	int height;
	int ave;
};

struct vpq_histgm_param_s {
	unsigned int hist_pow;
	unsigned int luma_sum;
	unsigned int pixel_sum;
	unsigned int histgm[VPQ_HIST_BIN_COUNT];
	unsigned int dark_histgm[VPQ_HIST_BIN_COUNT];
	unsigned int hue_histgm[VPQ_COLOR_HIST_BIN_COUNT];
	unsigned int sat_histgm[VPQ_COLOR_HIST_BIN_COUNT];
};

enum vpq_hdr_lut_type_e {
	VPQ_HDR_LUT_TYPE_HLG = 0,
	VPQ_HDR_LUT_TYPE_HDR,
	VPQ_HDR_LUT_TYPE_MAX,
};

struct vpq_hdr_lut_s {
	enum vpq_hdr_lut_type_e lut_type;
	unsigned int lut_size;
	union {
		int *lut_data;
		long long lut_len;
	};
};

enum vpq_color_primary_e {
	VPQ_COLOR_PRI_NULL = 0,
	VPQ_COLOR_PRI_BT601,
	VPQ_COLOR_PRI_BT709,
	VPQ_COLOR_PRI_BT2020,
	VPQ_COLOR_PRI_MAX,
};

struct vpq_hdr_metadata_s {
	unsigned int primaries[3][2]; /*normalized 50000 in G,B,R order*/
	unsigned int white_point[2];  /*normalized 50000*/
	unsigned int luminance[2];    /*max/min luminance, normalized 10000*/
};

struct vpq_pq_en_ctrl_s {
	unsigned char vadj1_en;
	unsigned char vd1_ctrst_en;
	unsigned char vadj2_en;
	unsigned char post_ctrst_en;
	unsigned char pregamma_en;
	unsigned char gamma_en;
	unsigned char wb_en;
	unsigned char dnlp_en;
	unsigned char lc_en;
	unsigned char blue_stretch_en;
	unsigned char black_ext_en;
	unsigned char chroma_cor_en;
	unsigned char sharpness0_en;
	unsigned char sharpness1_en;
	unsigned char cm_en;
	unsigned char lut3d_en;
	unsigned char dejaggy_sr0_en;
	unsigned char dejaggy_sr1_en;
	unsigned char dering_sr0_en;
	unsigned char dering_sr1_en;
};

struct vpq_pq_state_s {
	unsigned char pq_en;
	struct vpq_pq_en_ctrl_s pq_cfg;
};

struct vpq_overscan_table_s {
	int id; //actually not usd
	unsigned int length;
	union {
		void *param_ptr;
		long long param_ptr_len;
	};
	union {
		void *reserved;
		long long reserved_len;
	};
};

enum vpq_pc_mode_e {
	VPQ_PC_MODE_OFF = 0,
	VPQ_PC_MODE_ON,
};

enum vpq_event_info_e {
	VPQ_EVENT_NONE = 0,
	VPQ_EVENT_SIG_INFO_CHANGE,
};

enum vpq_source_type_e {
	VPQ_SRC_TYPE_MPEG = 0,
	VPQ_SRC_TYPE_ATV,
	VPQ_SRC_TYPE_CVBS,
	VPQ_SRC_TYPE_HDMI,
};

enum vpq_hdmi_port_e {
	VPQ_HDMI_PORT_NULL = 0,
	VPQ_HDMI_PORT_0,
	VPQ_HDMI_PORT_1,
	VPQ_HDMI_PORT_2,
	VPQ_HDMI_PORT_3,
};

enum vpq_sig_mode_e {
	VPQ_SIG_MODE_OTHERS = 0,
	VPQ_SIG_MODE_PAL,
	VPQ_SIG_MODE_NTSC,
	VPQ_SIG_MODE_SECAM,
};

enum vpq_hdr_type_e {
	VPQ_HDRTYPE_NONE = 0,
	VPQ_HDRTYPE_SDR,
	VPQ_HDRTYPE_HDR10,
	VPQ_HDRTYPE_HLG,
	VPQ_HDRTYPE_HDR10PLUS,
	VPQ_HDRTYPE_DOBVI,
	VPQ_HDRTYPE_MVC,
	VPQ_HDRTYPE_CUVA_HDR,
	VPQ_HDRTYPE_CUVA_HLG,
};

struct vpq_signal_info_s {
	enum vpq_source_type_e src_type;
	enum vpq_hdmi_port_e hdmi_port;
	enum vpq_sig_mode_e sig_mode;
	enum vpq_hdr_type_e hdr_type;
	unsigned int height;
	unsigned int width;
};

enum vpq_frame_status_e {
	VPQ_VFRAME_UNSTABLE = 0,
	VPQ_VFRAME_START,
	VPQ_VFRAME_STOP,
};

struct vpq_frame_info_s {
	unsigned int shared_fd;
	unsigned int reserved[8];
};

#define VPQ_DEVICE_NAME                  "vpq"
#define VPQ_IOC_MAGIC                    'C'

//VPP
#define VPQ_IOC_SET_PQTABLE_PARAM        _IOW(VPQ_IOC_MAGIC, 0x00, struct vpq_pqtable_bin_param_s)
#define VPQ_IOC_SET_PQ_MODULE_CFG        _IO(VPQ_IOC_MAGIC, 0x01)
#define VPQ_IOC_SET_BRIGHTNESS           _IOW(VPQ_IOC_MAGIC, 0x02, int)
#define VPQ_IOC_SET_CONTRAST             _IOW(VPQ_IOC_MAGIC, 0x03, int)
#define VPQ_IOC_SET_SATURATION           _IOW(VPQ_IOC_MAGIC, 0x04, int)
#define VPQ_IOC_SET_HUE                  _IOW(VPQ_IOC_MAGIC, 0x05, int)
#define VPQ_IOC_SET_SHARPNESS            _IOW(VPQ_IOC_MAGIC, 0x06, int)
#define VPQ_IOC_SET_BRIGHTNESS_POST      _IOW(VPQ_IOC_MAGIC, 0x07, int)
#define VPQ_IOC_SET_CONTRAST_POST        _IOW(VPQ_IOC_MAGIC, 0x08, int)
#define VPQ_IOC_SET_SATURATION_POST      _IOW(VPQ_IOC_MAGIC, 0x09, int)
#define VPQ_IOC_SET_HUE_POST             _IOW(VPQ_IOC_MAGIC, 0x0a, int)
#define VPQ_IOC_SET_OVERSCAN_TABLE       _IOW(VPQ_IOC_MAGIC, 0x0b, struct vpq_overscan_table_s)
#define VPQ_IOC_SET_BACKLIGHT            _IOW(VPQ_IOC_MAGIC, 0x0c, int)
#define VPQ_IOC_SET_GAMMA_ONOFF          _IOW(VPQ_IOC_MAGIC, 0x0d, int)
#define VPQ_IOC_SET_GAMMA_INDEX          _IOW(VPQ_IOC_MAGIC, 0x0e, int)
#define VPQ_IOC_SET_GAMMA_TABLE          _IOW(VPQ_IOC_MAGIC, 0x0f, struct vpq_gamma_table_s)
#define VPQ_IOC_SET_BLEND_GAMMA          _IOW(VPQ_IOC_MAGIC, 0x10, struct vpq_blend_gamma_s)
#define VPQ_IOC_SET_PRE_GAMMA_DATA       _IOW(VPQ_IOC_MAGIC, 0x11, struct vpq_pre_gamma_table_s)
#define VPQ_IOC_SET_MANUAL_GAMMA         _IOW(VPQ_IOC_MAGIC, 0x12, int)
#define VPQ_IOC_SET_RGB_OGO              _IOW(VPQ_IOC_MAGIC, 0x13, struct vpq_rgb_ogo_s)
#define VPQ_IOC_SET_COLOR_MANAGEMENT     _IOW(VPQ_IOC_MAGIC, 0x14, int)
#define VPQ_IOC_SET_COLOR_CUSTOMIZE      _IOW(VPQ_IOC_MAGIC, 0x15, struct vpq_cms_s)
#define VPQ_IOC_SET_HDMI_COLOR_RANGE_MODE _IOW(VPQ_IOC_MAGIC, 0x16, int)
#define VPQ_IOC_SET_COLOR_SPACE          _IOW(VPQ_IOC_MAGIC, 0x17, int)
#define VPQ_IOC_SET_GLOBAL_DIMMING       _IOW(VPQ_IOC_MAGIC, 0x18, int)
#define VPQ_IOC_SET_LOCAL_DIMMING        _IOW(VPQ_IOC_MAGIC, 0x19, int)
#define VPQ_IOC_SET_BLACK_STRETCH        _IOW(VPQ_IOC_MAGIC, 0x1a, int)
#define VPQ_IOC_SET_DNLP_MODE            _IOW(VPQ_IOC_MAGIC, 0x1b, int)
#define VPQ_IOC_SET_LC_MODE              _IOW(VPQ_IOC_MAGIC, 0x1c, int)
#define VPQ_IOC_SET_SR                   _IOW(VPQ_IOC_MAGIC, 0x1d, int)
#define VPQ_IOC_GET_HIST_AVG             _IOR(VPQ_IOC_MAGIC, 0x1e, struct vpq_histgm_ave_s)
#define VPQ_IOC_GET_HIST_BIN             _IOR(VPQ_IOC_MAGIC, 0x1f, struct vpq_histgm_param_s)
#define VPQ_IOC_SET_CSC_TYPE             _IOW(VPQ_IOC_MAGIC, 0x20, enum vpq_csc_type_e)
#define VPQ_IOC_GET_CSC_TYPE             _IOR(VPQ_IOC_MAGIC, 0x21, enum vpq_csc_type_e)
#define VPQ_IOC_SET_HDR_TMO_MODE         _IOW(VPQ_IOC_MAGIC, 0x22, int)
#define VPQ_IOC_SET_HDR_TMO_DATA         _IOW(VPQ_IOC_MAGIC, 0x23, struct vpq_hdr_lut_s)
#define VPQ_IOC_SET_AIPQ_MODE            _IOW(VPQ_IOC_MAGIC, 0x24, int)
#define VPQ_IOC_SET_AISR_MODE            _IOW(VPQ_IOC_MAGIC, 0x25, int)
#define VPQ_IOC_GET_COLOR_PRIM           _IOR(VPQ_IOC_MAGIC, 0x26, enum vpq_color_primary_e)
#define VPQ_IOC_GET_HDR_METADATA         _IOR(VPQ_IOC_MAGIC, 0x27, struct vpq_hdr_metadata_s)
#define VPQ_IOC_SET_BLUE_STRETCH         _IOW(VPQ_IOC_MAGIC, 0x28, int)
#define VPQ_IOC_SET_CHROMA_CORING        _IOW(VPQ_IOC_MAGIC, 0x29, int)
#define VPQ_IOC_SET_XVYCC_MODE           _IOW(VPQ_IOC_MAGIC, 0x2a, int)
#define VPQ_IOC_SET_COLOR_BASE           _IOW(VPQ_IOC_MAGIC, 0x2b, int)
#define VPQ_IOC_SET_CABC                 _IO(VPQ_IOC_MAGIC, 0x2c)
#define VPQ_IOC_SET_ADD                  _IO(VPQ_IOC_MAGIC, 0x2d)
#define VPQ_IOC_SET_PC_MODE              _IOW(VPQ_IOC_MAGIC, 0x2e, enum vpq_pc_mode_e)
#define VPQ_IOC_GET_EVENT_INFO	         _IOR(VPQ_IOC_MAGIC, 0x2f, enum vpq_event_info_e)
#define VPQ_IOC_GET_SIGNAL_INFO          _IOR(VPQ_IOC_MAGIC, 0x30, struct vpq_signal_info_s)
#define VPQ_IOC_SET_FRAME_STATUS         _IOW(VPQ_IOC_MAGIC, 0x31, enum vpq_frame_status_e)
#define VPQ_IOC_SET_FRAME                _IOW(VPQ_IOC_MAGIC, 0x32, struct vpq_frame_info_s)

//MEMC
/* TBD from 0x40*/

//DI
#define VPQ_IOC_SET_NR                   _IOW(VPQ_IOC_MAGIC, 0x60, int)
#define VPQ_IOC_SET_DEBLOCK              _IOW(VPQ_IOC_MAGIC, 0x61, int)
#define VPQ_IOC_SET_DEMOSQUITO           _IOW(VPQ_IOC_MAGIC, 0x62, int)
#define VPQ_IOC_SET_SMOOTHPLUS_MODE      _IOW(VPQ_IOC_MAGIC, 0x63, int)
#define VPQ_IOC_SET_MCDI_MODE            _IOW(VPQ_IOC_MAGIC, 0x64, int)

//TVAFE
#define VPQ_IOC_SET_PLL_VALUE            _IO(VPQ_IOC_MAGIC, 0x70)
#define VPQ_IOC_SET_CVD2_VALUE           _IO(VPQ_IOC_MAGIC, 0x71)
#endif
