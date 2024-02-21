/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _TVIN_H_
#define _TVIN_H_

#include <linux/types.h>
#include "amvecm_ext.h"

/* *********************************************************************** */

/* * TVIN general definition/enum/struct *********************************** */

/* ************************************************************************ */

/* tvin input port select */
enum tvin_port_e {
	TVIN_PORT_NULL = 0x00000000,
	TVIN_PORT_MPEG0 = 0x00000100,
	TVIN_PORT_BT656 = 0x00000200,
	TVIN_PORT_BT601,
	TVIN_PORT_CAMERA,
	TVIN_PORT_BT656_HDMI,
	TVIN_PORT_BT601_HDMI,
	TVIN_PORT_CVBS0 = 0x00001000,
	TVIN_PORT_CVBS1,
	TVIN_PORT_CVBS2,
	TVIN_PORT_CVBS3,
	TVIN_PORT_HDMI0 = 0x00004000,
	TVIN_PORT_HDMI1,
	TVIN_PORT_HDMI2,
	TVIN_PORT_HDMI3,
	TVIN_PORT_HDMI4,
	TVIN_PORT_HDMI5,
	TVIN_PORT_HDMI6,
	TVIN_PORT_HDMI7,
	TVIN_PORT_DVIN0 = 0x00008000,
	TVIN_PORT_VIU1 = 0x0000a000,
	TVIN_PORT_VIU1_VIDEO, /* vpp0 preblend vd1 */
	TVIN_PORT_VIU1_WB0_VD1, /* vpp0 vadj1 output */
	TVIN_PORT_VIU1_WB0_VD2, /* vpp0 vd2 postblend input */
	TVIN_PORT_VIU1_WB0_OSD1, /* vpp0 osd1 postblend input */
	TVIN_PORT_VIU1_WB0_OSD2, /* vpp0 osd2 postblend input */
	TVIN_PORT_VIU1_WB0_VPP, /* vpp0 output */
	TVIN_PORT_VIU1_WB0_POST_BLEND, /* vpp0 postblend output */
	TVIN_PORT_VIU1_WB1_VDIN_BIST,
	TVIN_PORT_VIU1_WB1_VIDEO,
	TVIN_PORT_VIU1_WB1_VD1,
	TVIN_PORT_VIU1_WB1_VD2,
	TVIN_PORT_VIU1_WB1_OSD1,
	TVIN_PORT_VIU1_WB1_OSD2,
	TVIN_PORT_VIU1_WB1_VPP,
	TVIN_PORT_VIU1_WB1_POST_BLEND,
	TVIN_PORT_VIU1_MAX,
	TVIN_PORT_VIU2 = 0x0000C000,
	TVIN_PORT_VIU2_ENCL,
	TVIN_PORT_VIU2_ENCI,
	TVIN_PORT_VIU2_ENCP,
	TVIN_PORT_VIU2_VD1, /* vpp1 vd1 output */
	TVIN_PORT_VIU2_OSD1, /* vpp1 osd1 output */
	TVIN_PORT_VIU2_VPP, /* vpp1 output */
	TVIN_PORT_VIU2_MAX,
	TVIN_PORT_VIU3 = 0x0000D000,
	TVIN_PORT_VIU3_VD1, /* vpp2 vd1 output */
	TVIN_PORT_VIU3_OSD1, /* vpp2 osd1 output */
	TVIN_PORT_VIU3_VPP, /* vpp2 output */
	TVIN_PORT_VIU3_MAX,
	TVIN_PORT_VENC = 0x0000E000,
	TVIN_PORT_VENC0,
	TVIN_PORT_VENC1,
	TVIN_PORT_VENC2,
	TVIN_PORT_VENC_MAX,
	TVIN_PORT_MIPI = 0x00010000,
	TVIN_PORT_ISP = 0x00020000,
	TVIN_PORT_MAX = 0x80000000,
};

/* tvin signal format table */
enum tvin_sig_fmt_e {
	TVIN_SIG_FMT_NULL = 0,
	/* HDMI Formats */
	TVIN_SIG_FMT_HDMI_640X480P_60HZ = 0x401,
	TVIN_SIG_FMT_HDMI_720X480P_60HZ = 0x402,
	TVIN_SIG_FMT_HDMI_1280X720P_60HZ = 0x403,
	TVIN_SIG_FMT_HDMI_1920X1080I_60HZ = 0x404,
	TVIN_SIG_FMT_HDMI_1440X480I_60HZ = 0x405,
	TVIN_SIG_FMT_HDMI_1440X240P_60HZ = 0x406,
	TVIN_SIG_FMT_HDMI_2880X480I_60HZ = 0x407,
	TVIN_SIG_FMT_HDMI_2880X240P_60HZ = 0x408,
	TVIN_SIG_FMT_HDMI_1440X480P_60HZ = 0x409,
	TVIN_SIG_FMT_HDMI_1920X1080P_60HZ = 0x40a,
	TVIN_SIG_FMT_HDMI_720X576P_50HZ = 0x40b,
	TVIN_SIG_FMT_HDMI_1280X720P_50HZ = 0x40c,
	TVIN_SIG_FMT_HDMI_1920X1080I_50HZ_A = 0x40d,
	TVIN_SIG_FMT_HDMI_1440X576I_50HZ = 0x40e,
	TVIN_SIG_FMT_HDMI_1440X288P_50HZ = 0x40f,
	TVIN_SIG_FMT_HDMI_2880X576I_50HZ = 0x410,
	TVIN_SIG_FMT_HDMI_2880X288P_50HZ = 0x411,
	TVIN_SIG_FMT_HDMI_1440X576P_50HZ = 0x412,
	TVIN_SIG_FMT_HDMI_1920X1080P_50HZ = 0x413,
	TVIN_SIG_FMT_HDMI_1920X1080P_24HZ = 0x414,
	TVIN_SIG_FMT_HDMI_1920X1080P_25HZ = 0x415,
	TVIN_SIG_FMT_HDMI_1920X1080P_30HZ = 0x416,
	TVIN_SIG_FMT_HDMI_2880X480P_60HZ = 0x417,
	TVIN_SIG_FMT_HDMI_2880X576P_50HZ = 0x418,
	TVIN_SIG_FMT_HDMI_1920X1080I_50HZ_B = 0x419,
	TVIN_SIG_FMT_HDMI_1920X1080I_100HZ = 0x41a,
	TVIN_SIG_FMT_HDMI_1280X720P_100HZ = 0x41b,
	TVIN_SIG_FMT_HDMI_720X576P_100HZ = 0x41c,
	TVIN_SIG_FMT_HDMI_1440X576I_100HZ = 0x41d,
	TVIN_SIG_FMT_HDMI_1920X1080I_120HZ = 0x41e,
	TVIN_SIG_FMT_HDMI_1280X720P_120HZ = 0x41f,
	TVIN_SIG_FMT_HDMI_720X480P_120HZ = 0x420,
	TVIN_SIG_FMT_HDMI_1440X480I_120HZ = 0x421,
	TVIN_SIG_FMT_HDMI_720X576P_200HZ = 0x422,
	TVIN_SIG_FMT_HDMI_1440X576I_200HZ = 0x423,
	TVIN_SIG_FMT_HDMI_720X480P_240HZ = 0x424,
	TVIN_SIG_FMT_HDMI_1440X480I_240HZ = 0x425,
	TVIN_SIG_FMT_HDMI_1280X720P_24HZ = 0x426,
	TVIN_SIG_FMT_HDMI_1280X720P_25HZ = 0x427,
	TVIN_SIG_FMT_HDMI_1280X720P_30HZ = 0x428,
	TVIN_SIG_FMT_HDMI_1920X1080P_120HZ = 0x429,
	TVIN_SIG_FMT_HDMI_1920X1080P_100HZ = 0x42a,
	TVIN_SIG_FMT_HDMI_1280X720P_60HZ_FRAME_PACKING = 0x42b,
	TVIN_SIG_FMT_HDMI_1280X720P_50HZ_FRAME_PACKING = 0x42c,
	TVIN_SIG_FMT_HDMI_1280X720P_24HZ_FRAME_PACKING = 0x42d,
	TVIN_SIG_FMT_HDMI_1280X720P_30HZ_FRAME_PACKING = 0x42e,
	TVIN_SIG_FMT_HDMI_1920X1080I_60HZ_FRAME_PACKING = 0x42f,
	TVIN_SIG_FMT_HDMI_1920X1080I_50HZ_FRAME_PACKING = 0x430,
	TVIN_SIG_FMT_HDMI_1920X1080P_24HZ_FRAME_PACKING = 0x431,
	TVIN_SIG_FMT_HDMI_1920X1080P_30HZ_FRAME_PACKING = 0x432,
	TVIN_SIG_FMT_HDMI_800X600_00HZ = 0x433,
	TVIN_SIG_FMT_HDMI_1024X768_00HZ = 0x434,
	TVIN_SIG_FMT_HDMI_720X400_00HZ = 0x435,
	TVIN_SIG_FMT_HDMI_1280X768_00HZ = 0x436,
	TVIN_SIG_FMT_HDMI_1280X800_00HZ = 0x437,
	TVIN_SIG_FMT_HDMI_1280X960_00HZ = 0x438,
	TVIN_SIG_FMT_HDMI_1280X1024_00HZ = 0x439,
	TVIN_SIG_FMT_HDMI_1360X768_00HZ = 0x43a,
	TVIN_SIG_FMT_HDMI_1366X768_00HZ = 0x43b,
	TVIN_SIG_FMT_HDMI_1600X1200_00HZ = 0x43c,
	TVIN_SIG_FMT_HDMI_1920X1200_00HZ = 0x43d,
	TVIN_SIG_FMT_HDMI_1440X900_00HZ = 0x43e,
	TVIN_SIG_FMT_HDMI_1400X1050_00HZ = 0x43f,
	TVIN_SIG_FMT_HDMI_1680X1050_00HZ = 0x440,
	/* for alternative and 4k2k */
	TVIN_SIG_FMT_HDMI_1920X1080I_60HZ_ALTERNATIVE = 0x441,
	TVIN_SIG_FMT_HDMI_1920X1080I_50HZ_ALTERNATIVE = 0x442,
	TVIN_SIG_FMT_HDMI_1920X1080P_24HZ_ALTERNATIVE = 0x443,
	TVIN_SIG_FMT_HDMI_1920X1080P_30HZ_ALTERNATIVE = 0x444,
	TVIN_SIG_FMT_HDMI_3840_2160_00HZ = 0x445,
	TVIN_SIG_FMT_HDMI_4096_2160_00HZ = 0x446,
	TVIN_SIG_FMT_HDMI_1600X900_60HZ = 0x447,/* upper layer set TVIN_SIG_FMT_HDMI_HDR */
	TVIN_SIG_FMT_HDMI_RESERVE8 = 0x448,
	TVIN_SIG_FMT_HDMI_RESERVE9 = 0x449,
	TVIN_SIG_FMT_HDMI_RESERVE10 = 0x44a,
	TVIN_SIG_FMT_HDMI_RESERVE11 = 0x44b,
	TVIN_SIG_FMT_HDMI_720X480P_60HZ_FRAME_PACKING = 0x44c,
	TVIN_SIG_FMT_HDMI_720X576P_50HZ_FRAME_PACKING = 0x44d,
	TVIN_SIG_FMT_HDMI_640X480P_72HZ = 0x44e,
	TVIN_SIG_FMT_HDMI_640X480P_75HZ = 0x44f,
	TVIN_SIG_FMT_HDMI_1152X864_00HZ = 0x450,
	TVIN_SIG_FMT_HDMI_3840X600_00HZ = 0x451,
	TVIN_SIG_FMT_HDMI_720X350_00HZ = 0x452,
	TVIN_SIG_FMT_HDMI_2688X1520_00HZ = 0x453,
	TVIN_SIG_FMT_HDMI_1920X2160_60HZ = 0x454,
	TVIN_SIG_FMT_HDMI_960X540_60HZ = 0x455,
	TVIN_SIG_FMT_HDMI_2560X1440_00HZ = 0x456,
	TVIN_SIG_FMT_HDMI_640X350_85HZ = 0x457,
	TVIN_SIG_FMT_HDMI_640X400_85HZ = 0x458,
	TVIN_SIG_FMT_HDMI_848X480_60HZ = 0x459,
	TVIN_SIG_FMT_HDMI_1792X1344_85HZ = 0x45a,
	TVIN_SIG_FMT_HDMI_1856X1392_00HZ = 0x45b,
	TVIN_SIG_FMT_HDMI_1920X1440_00HZ = 0x45c,
	TVIN_SIG_FMT_HDMI_2048X1152_60HZ = 0x45d,
	TVIN_SIG_FMT_HDMI_2560X1600_00HZ = 0x45e,
	TVIN_SIG_FMT_HDMI_720X480I_60HZ = 0x45f,
	TVIN_SIG_FMT_HDMI_720X576I_50HZ = 0x460,
	TVIN_SIG_FMT_HDMI_MAX,
	TVIN_SIG_FMT_HDMI_THRESHOLD = 0x600,
	/* Video Formats */
	TVIN_SIG_FMT_CVBS_NTSC_M = 0x601,
	TVIN_SIG_FMT_CVBS_NTSC_443 = 0x602,
	TVIN_SIG_FMT_CVBS_PAL_I = 0x603,
	TVIN_SIG_FMT_CVBS_PAL_M = 0x604,
	TVIN_SIG_FMT_CVBS_PAL_60 = 0x605,
	TVIN_SIG_FMT_CVBS_PAL_CN = 0x606,
	TVIN_SIG_FMT_CVBS_SECAM = 0x607,
	TVIN_SIG_FMT_CVBS_NTSC_50 = 0x608,
	TVIN_SIG_FMT_CVBS_MAX = 0x609,
	TVIN_SIG_FMT_CVBS_THRESHOLD = 0x800,
	/* 656 Formats */
	TVIN_SIG_FMT_BT656IN_576I_50HZ = 0x801,
	TVIN_SIG_FMT_BT656IN_480I_60HZ = 0x802,
	/* 601 Formats */
	TVIN_SIG_FMT_BT601IN_576I_50HZ = 0x803,
	TVIN_SIG_FMT_BT601IN_480I_60HZ = 0x804,
	/* Camera Formats */
	TVIN_SIG_FMT_CAMERA_640X480P_30HZ = 0x805,
	TVIN_SIG_FMT_CAMERA_800X600P_30HZ = 0x806,
	TVIN_SIG_FMT_CAMERA_1024X768P_30HZ = 0x807,
	TVIN_SIG_FMT_CAMERA_1920X1080P_30HZ = 0x808,
	TVIN_SIG_FMT_CAMERA_1280X720P_30HZ = 0x809,
	TVIN_SIG_FMT_BT601_MAX = 0x80a,
	TVIN_SIG_FMT_BT601_THRESHOLD = 0xa00,
	//upper use HDR Formats
	TVIN_SIG_FMT_RESERVED_THRESHOLD = 0xb00,
	TVIN_SIG_FMT_HDMI_HDR10 = 0xb01,
	TVIN_SIG_FMT_HDMI_HDR10PLUS = 0xb02,
	TVIN_SIG_FMT_HDMI_HLG = 0xb03,
	TVIN_SIG_FMT_HDMI_DOLBY = 0xb04,
	TVIN_SIG_FMT_HDMI_UPPER_FORMAT_MAX = 0xb20,
	TVIN_SIG_FMT_HDMI_UPPER_THRESHOLD = 0xc00,
	TVIN_SIG_FMT_MAX,
};

/* tvin signal status */
enum tvin_sig_status_e {
	TVIN_SIG_STATUS_NULL = 0,
	/* processing status from init to */
	/*the finding of the 1st confirmed status */

	TVIN_SIG_STATUS_NOSIG,	/* no signal - physically no signal */
	TVIN_SIG_STATUS_UNSTABLE,	/* unstable - physically bad signal */
	TVIN_SIG_STATUS_NOTSUP,
	/* not supported - physically good signal & not supported */

	TVIN_SIG_STATUS_STABLE,
	/* stable - physically good signal & supported */
};

enum tvin_trans_fmt {
	TVIN_TFMT_2D = 0,
	TVIN_TFMT_3D_LRH_OLOR,
	/* 1 Primary: Side-by-Side(Half) Odd/Left picture, Odd/Right p */

	TVIN_TFMT_3D_LRH_OLER,
	/* 2 Primary: Side-by-Side(Half) Odd/Left picture, Even/Right picture */

	TVIN_TFMT_3D_LRH_ELOR,
	/* 3 Primary: Side-by-Side(Half) Even/Left picture, Odd/Right picture */

	TVIN_TFMT_3D_LRH_ELER,
	/* 4 Primary: Side-by-Side(Half) Even/Left picture, Even/Right picture*/

	TVIN_TFMT_3D_TB,	/* 5 Primary: Top-and-Bottom */
	TVIN_TFMT_3D_FP,	/* 6 Primary: Frame Packing */
	TVIN_TFMT_3D_FA,	/* 7 Secondary: Field Alternative */
	TVIN_TFMT_3D_LA,	/* 8 Secondary: Line Alternative */
	TVIN_TFMT_3D_LRF,	/* 9 Secondary: Side-by-Side(Full) */
	TVIN_TFMT_3D_LD,	/* 10 Secondary: L+depth */
	TVIN_TFMT_3D_LDGD, /* 11 Secondary: L+depth+Graphics+Graphics-depth */
	/* normal 3D format */
	TVIN_TFMT_3D_DET_TB,	/* 12 */
	TVIN_TFMT_3D_DET_LR,	/* 13 */
	TVIN_TFMT_3D_DET_INTERLACE,	/* 14 */
	TVIN_TFMT_3D_DET_CHESSBOARD,	/* 15 */
};

enum tvin_color_fmt_e {
	TVIN_RGB444 = 0,
	TVIN_YUV422,		/* 1 */
	TVIN_YUV444,		/* 2 */
	TVIN_YUYV422,		/* 3 */
	TVIN_YVYU422,		/* 4 */
	TVIN_UYVY422,		/* 5 */
	TVIN_VYUY422,		/* 6 */
	TVIN_NV12,		/* 7 */
	TVIN_NV21,		/* 8 */
	TVIN_BGGR,		/* 9  raw data */
	TVIN_RGGB,		/* 10 raw data */
	TVIN_GBRG,		/* 11 raw data */
	TVIN_GRBG,		/* 12 raw data */
	TVIN_COLOR_FMT_MAX,
};

enum tvin_aspect_ratio_e {
	TVIN_ASPECT_NULL = 0,
	TVIN_ASPECT_1x1,
	TVIN_ASPECT_4x3_FULL,
	TVIN_ASPECT_14x9_FULL,
	TVIN_ASPECT_14x9_LB_CENTER,
	TVIN_ASPECT_14x9_LB_TOP,
	TVIN_ASPECT_16x9_FULL,
	TVIN_ASPECT_16x9_LB_CENTER,
	TVIN_ASPECT_16x9_LB_TOP,
	TVIN_ASPECT_MAX,
};

enum tvin_force_color_range_e {
	COLOR_RANGE_AUTO = 0,
	COLOR_RANGE_FULL,
	COLOR_RANGE_LIMIT,
	COLOR_RANGE_NULL,
};

enum tvin_scan_mode_e {
	TVIN_SCAN_MODE_NULL = 0,
	TVIN_SCAN_MODE_PROGRESSIVE,
	TVIN_SCAN_MODE_INTERLACED,
};

struct tvin_to_vpp_info_s {
	__u32 is_dv;
	enum tvin_scan_mode_e scan_mode;
	unsigned int fps;
	unsigned int width;
	unsigned int height;
	enum tvin_color_fmt_e cfmt;
};

struct tvin_info_s {
	enum tvin_trans_fmt trans_fmt;
	enum tvin_sig_fmt_e fmt;
	enum tvin_sig_status_e status;
	enum tvin_color_fmt_e cfmt;
	unsigned int fps;
	unsigned int is_dvi;

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
	unsigned int signal_type;
	/*
	 * 0:xvYCC601  1:xvYCC709    2:sYCC601            3:Adobe ycc601
	 * 4:Adobe rgb 5:BT2020(ycc) 6:BT2020(rgb or yuv) 7:reserved
	 */
	unsigned int input_colorimetry;
	enum tvin_aspect_ratio_e aspect_ratio;
	/*
	 * 0:no dv 1:visf 2:emp
	 */
	__u8 dolby_vision;
	/*
	 * 0:sink-led 1:source-led
	 */
	__u8 low_latency;
};

struct tvin_frontend_info_s {
	enum tvin_scan_mode_e scan_mode;
	enum tvin_color_fmt_e cfmt;
	unsigned int fps;
	unsigned int width;
	unsigned int height;
	unsigned int colordepth;
};

struct tvin_buf_info_s {
	unsigned int vf_size;
	unsigned int buf_count;
	unsigned int buf_width;
	unsigned int buf_height;
	unsigned int buf_size;
	unsigned int wr_list_size;
};

struct tvin_video_buf_s {
	unsigned int index;
	unsigned int reserved;
};

struct tvin_parm_s {
	int index;		/* index of frontend for vdin */
	enum tvin_port_e port;	/* must set port in IOCTL */
	struct tvin_info_s info;
	unsigned int hist_pow;
	unsigned int luma_sum;
	unsigned int pixel_sum;
	unsigned short histgram[64];
	unsigned int flag;
	unsigned short dest_width;	/* for vdin horizontal scale down */
	unsigned short dest_height;	/* for vdin vertical scale down */
	__u8 h_reverse;		/* for vdin horizontal reverse */
	__u8 v_reverse;		/* for vdin vertical reverse */
	unsigned int reserved;
};

/* ************************************************************************* */

/* *** AFE module definition/enum/struct *********************************** */

/* ************************************************************************* */
struct tvafe_vga_parm_s {
	signed short clk_step;	/* clock < 0, tune down clock freq */
	/* clock > 0, tune up clock freq */
	unsigned short phase;	/* phase is 0~31, it is absolute value */
	signed short hpos_step;	/* hpos_step < 0, shift display to left */
	/* hpos_step > 0, shift display to right */
	signed short vpos_step;	/* vpos_step < 0, shift display to top */
	/* vpos_step > 0, shift display to bottom */
	unsigned int vga_in_clean;	/* flage for vga clean screen */
};

enum tvafe_cvbs_video_e {
	TVAFE_CVBS_VIDEO_HV_UNLOCKED = 0,
	TVAFE_CVBS_VIDEO_H_LOCKED,
	TVAFE_CVBS_VIDEO_V_LOCKED,
	TVAFE_CVBS_VIDEO_HV_LOCKED,
};

struct vdin_event_info {
	/*enum tvin_sg_chg_flg*/
	__u32 event_sts;
};

enum tvin_sync_pol_e {
	TVIN_SYNC_POL_NULL = 0,
	TVIN_SYNC_POL_NEGATIVE,
	TVIN_SYNC_POL_POSITIVE,
};

struct tvin_format_s {
	/* Th in the unit of pixel */
	unsigned short         h_active;
	 /* Tv in the unit of line */
	unsigned short         v_active;
	/* Th in the unit of T, while 1/T = 24MHz or 27MHz or even 100MHz */
	unsigned short         h_cnt;
	/* Tolerance of h_cnt */
	unsigned short         h_cnt_offset;
	/* Tolerance of v_cnt */
	unsigned short         v_cnt_offset;
	/* Ths in the unit of T, while 1/T = 24MHz or 27MHz or even 100MHz */
	unsigned short         hs_cnt;
	/* Tolerance of hs_cnt */
	unsigned short         hs_cnt_offset;
	/* Th in the unit of pixel */
	unsigned short         h_total;
	/* Tv in the unit of line */
	unsigned short         v_total;
	/* h front proch */
	unsigned short         hs_front;
	 /* HS in the unit of pixel */
	unsigned short         hs_width;
	 /* HS in the unit of pixel */
	unsigned short         hs_bp;
	/* vs front proch in the unit of line */
	unsigned short         vs_front;
	 /* VS width in the unit of line */
	unsigned short         vs_width;
	/* vs back proch in the unit of line */
	unsigned short         vs_bp;
	enum tvin_sync_pol_e   hs_pol;
	enum tvin_sync_pol_e   vs_pol;
	enum tvin_scan_mode_e  scan_mode;
	/* (Khz/10) */
	unsigned short         pixel_clk;
	unsigned short         vbi_line_start;
	unsigned short         vbi_line_end;
	unsigned int           duration;
};

enum vdin_vrr_mode_e {
	VDIN_VRR_OFF = 0,
	VDIN_VRR_BASIC,
	VDIN_VRR_FREESYNC,
	VDIN_VRR_FREESYNC_PREMIUM,
	VDIN_VRR_FREESYNC_PREMIUM_PRO,
	VDIN_VRR_FREESYNC_PREMIUM_G_SYNC,
	VDIN_VRR_NUM
};

struct vdin_vrr_freesync_param_s {
	enum vdin_vrr_mode_e cur_vrr_status;
	__u8 tone_mapping_en;
	__u8 local_dimming_disable;
	__u8 native_color_en;
};

struct vdin_hist_s {
	__kernel_long_t sum;
	int width;
	int height;
	int ave;
	unsigned short hist[64];
};

enum port_mode {
	capture_osd_plus_video = 0,
	capture_only_video,
};

struct vdin_v4l2_param_s {
	int width;
	int height;
	int fps;
	enum tvin_color_fmt_e dst_fmt;
	int dst_width;	/* H scaling down */
	int dst_height;	/* v scaling down */
	unsigned int bit_order;	/* raw data bit order(0:none std, 1: std)*/
	enum port_mode mode;	/*0:osd + video 1:video only*/
	int bit_dep;
	__u8 secure_memory_en; /* 0:not secure memory 1:secure memory */
};

enum tvin_cn_type_e {
	GRAPHICS,
	PHOTO,
	CINEMA,
	GAME,
};

struct tvin_latency_s {
	__u8 allm_mode; /* bit0:hdmi allm, bit1:dv allm */
	__u8 it_content;
	enum tvin_cn_type_e cn_type;
};

/* only for keystone use begin */
struct vdin_set_canvas_s {
	int fd;
	int index;
};

enum tvin_sg_chg_flg {
	TVIN_SIG_CHG_NONE = 0,
	TVIN_SIG_CHG_SDR2HDR	= 0x01,
	TVIN_SIG_CHG_HDR2SDR	= 0x02,
	TVIN_SIG_CHG_DV2NO	= 0x04,
	TVIN_SIG_CHG_NO2DV	= 0x08,
	TVIN_SIG_CHG_COLOR_FMT	= 0x10,
	TVIN_SIG_CHG_RANGE	= 0x20,	/*color range:full or limit*/
	TVIN_SIG_CHG_BIT	= 0x40,	/*color bit depth: 8,10,12 ...*/
	TVIN_SIG_CHG_VS_FRQ	= 0x80,
	TVIN_SIG_CHG_DV_ALLM	= 0x100,
	TVIN_SIG_CHG_AFD	= 0x200,/*aspect ratio*/
	TVIN_SIG_CHG_VRR        = 0x1000, /*vrr*/
	TVIN_SIG_CHG_CLOSE_FE	= 0x40000000,	/*closed frontend*/
	TVIN_SIG_CHG_STS	= 0x80000000,	/*sm state change*/
};

#define TVIN_SIG_DV_CHG		(TVIN_SIG_CHG_DV2NO | TVIN_SIG_CHG_NO2DV)
#define TVIN_SIG_HDR_CHG	(TVIN_SIG_CHG_SDR2HDR | TVIN_SIG_CHG_HDR2SDR)

/* ************************************************************************* */

/* *** IOCTL command definition ******************************************* */

/* ************************************************************************* */

#define _TM_T 'T'

/* GENERAL */
#define TVIN_IOC_OPEN               _IOW(_TM_T, 0x01, struct tvin_parm_s)
#define TVIN_IOC_START_DEC          _IOW(_TM_T, 0x02, struct tvin_parm_s)
#define TVIN_IOC_STOP_DEC           _IO(_TM_T, 0x03)
#define TVIN_IOC_CLOSE              _IO(_TM_T, 0x04)
#define TVIN_IOC_G_PARM             _IOR(_TM_T, 0x05, struct tvin_parm_s)
#define TVIN_IOC_S_PARM             _IOW(_TM_T, 0x06, struct tvin_parm_s)
#define TVIN_IOC_G_SIG_INFO         _IOR(_TM_T, 0x07, struct tvin_info_s)
#define TVIN_IOC_G_BUF_INFO         _IOR(_TM_T, 0x08, struct tvin_buf_info_s)
#define TVIN_IOC_START_GET_BUF      _IO(_TM_T, 0x09)
#define TVIN_IOC_G_EVENT_INFO	_IOW(_TM_T, 0x0a, struct vdin_event_info)

#define TVIN_IOC_GET_BUF            _IOR(_TM_T, 0x10, struct tvin_video_buf_s)
#define TVIN_IOC_PAUSE_DEC          _IO(_TM_T, 0x41)
#define TVIN_IOC_RESUME_DEC         _IO(_TM_T, 0x42)
#define TVIN_IOC_VF_REG             _IO(_TM_T, 0x43)
#define TVIN_IOC_VF_UNREG           _IO(_TM_T, 0x44)
#define TVIN_IOC_FREEZE_VF          _IO(_TM_T, 0x45)
#define TVIN_IOC_UNFREEZE_VF        _IO(_TM_T, 0x46)
#define TVIN_IOC_SNOW_ON             _IO(_TM_T, 0x47)
#define TVIN_IOC_SNOW_OFF            _IO(_TM_T, 0x48)
#define TVIN_IOC_GET_COLOR_RANGE	_IOR(_TM_T, 0X49,\
	enum tvin_force_color_range_e)
#define TVIN_IOC_SET_COLOR_RANGE	_IOW(_TM_T, 0X4a,\
	enum tvin_force_color_range_e)
#define TVIN_IOC_GAME_MODE          _IOW(_TM_T, 0x4b, unsigned int)
#define TVIN_IOC_VRR_MODE           _IOW(_TM_T, 0x54, unsigned int)
#define TVIN_IOC_GET_LATENCY_MODE		_IOR(_TM_T, 0x4d,\
	struct tvin_latency_s)
#define TVIN_IOC_G_FRONTEND_INFO    _IOR(_TM_T, 0x4e,\
	struct tvin_frontend_info_s)
#define TVIN_IOC_S_CANVAS_ADDR  _IOW(_TM_T, 0x4f,\
	struct vdin_set_canvas_s)
#define TVIN_IOC_S_PC_MODE		_IOW(_TM_T, 0x50, unsigned int)
#define TVIN_IOC_S_FRAME_WR_EN		_IOW(_TM_T, 0x51, unsigned int)
#define TVIN_IOC_G_INPUT_TIMING		_IOR(_TM_T, 0x52, struct tvin_format_s)
#define TVIN_IOC_G_VRR_STATUS		_IOR(_TM_T, 0x53, struct vdin_vrr_freesync_param_s)
#define TVIN_IOC_G_VDIN_STATUS         _IOR(_TM_T, 0x54, unsigned int)

#define TVIN_IOC_S_CANVAS_RECOVERY  _IO(_TM_T, 0x0a)
/* TVAFE */
#define TVIN_IOC_S_AFE_VGA_PARM     _IOW(_TM_T, 0x16, struct tvafe_vga_parm_s)
#define TVIN_IOC_G_AFE_VGA_PARM     _IOR(_TM_T, 0x17, struct tvafe_vga_parm_s)
#define TVIN_IOC_S_AFE_VGA_AUTO     _IO(_TM_T, 0x18)
#define TVIN_IOC_G_AFE_CVBS_LOCK    _IOR(_TM_T, 0x1a, enum tvafe_cvbs_video_e)
#define TVIN_IOC_S_AFE_CVBS_STD     _IOW(_TM_T, 0x1b, enum tvin_sig_fmt_e)
#define TVIN_IOC_CALLMASTER_SET     _IOW(_TM_T, 0x1c, enum tvin_port_e)
#define TVIN_IOC_CALLMASTER_GET	    _IO(_TM_T, 0x1d)
#define TVIN_IOC_G_AFE_CVBS_STD     _IOW(_TM_T, 0x1e, enum tvin_sig_fmt_e)
#define TVIN_IOC_LOAD_REG          _IOW(_TM_T, 0x20, struct am_regs_s)
#define TVIN_IOC_S_AFE_SNOW_ON     _IO(_TM_T, 0x22)
#define TVIN_IOC_S_AFE_SNOW_OFF     _IO(_TM_T, 0x23)
#define TVIN_IOC_G_VDIN_HIST       _IOW(_TM_T, 0x24, struct vdin_hist_s)
#define TVIN_IOC_S_VDIN_V4L2START  _IOW(_TM_T, 0x25, struct vdin_v4l2_param_s)
#define TVIN_IOC_S_VDIN_V4L2STOP   _IO(_TM_T, 0x26)
#define TVIN_IOC_S_AFE_SNOW_CFG     _IOW(_TM_T, 0x27, unsigned int)
#define TVIN_IOC_S_DV_DESCRAMBLE	_IOW(_TM_T, 0x28, unsigned int)
#define TVIN_IOC_S_AFE_ATV_SEARCH  _IOW(_TM_T, 0x29, unsigned int)

#endif

