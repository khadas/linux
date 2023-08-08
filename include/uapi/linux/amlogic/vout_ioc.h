/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef _VOUT_IOC_H
#define _VOUT_IOC_H

#include <linux/types.h>

enum vmode_e {
	 VMODE_HDMI = 0,
	 VMODE_CVBS,
	 VMODE_LCD,
	 VMODE_NULL, /* null mode is used as temporary witch mode state */
	 VMODE_INVALID,
	 VMODE_DUMMY_ENCP,
	 VMODE_DUMMY_ENCI,
	 VMODE_DUMMY_ENCL,
	 VMODE_MAX,
	 VMODE_INIT_NULL,
	 VMODE_MASK = 0xFF,
};

enum color_fmt_e {
	COLOR_FMT_RGB444 = 0,
	COLOR_FMT_YUV422,		/* 1 */
	COLOR_FMT_YUV444,		/* 2 */
	COLOR_FMT_YUYV422,		/* 3 */
	COLOR_FMT_YVYU422,		/* 4 */
	COLOR_FMT_UYVY422,		/* 5 */
	COLOR_FMT_VYUY422,		/* 6 */
	COLOR_FMT_NV12,		/* 7 */
	COLOR_FMT_NV21,		/* 8 */
	COLOR_FMT_BGGR,		/* 9  raw data */
	COLOR_FMT_RGGB,		/* 10 raw data */
	COLOR_FMT_GBRG,		/* 11 raw data */
	COLOR_FMT_GRBG,		/* 12 raw data */
	COLOR_FMT_MAX,
};

struct vinfo_base_s {
	enum vmode_e mode;
	__u32 width;
	__u32 height;
	__u32 field_height;
	__u32 aspect_ratio_num;
	__u32 aspect_ratio_den;
	__u32 sync_duration_num;
	__u32 sync_duration_den;
	__u32 screen_real_width;
	__u32 screen_real_height;
	__u32 video_clk;
	enum color_fmt_e viu_color_fmt;
};

struct optical_base_s {
	__u32 primaries[3][2];	/* normalized 50000 in G,B,R order */
	__u32 white_point[2];	/* normalized 50000 */
	__u32 lumi_max; /* max/min lumin, normalized 10000 */
	__u32 lumi_min; /* max/min lumin, normalized 10000 */
	__u32 lumi_avg; /* max/min lumin, normalized 10000 */
	__u32 lumi_peak;

	__u8 ldim_support;
	__u8 dummy_flag1;
	__u8 dummy_flag2;
	__u8 dummy_flag3;
	__u32 dummy_val[8];
};

struct venc_base_s {
	__u8 venc_index; /*such as 0, 1, 2*/
	__u8 venc_sel;  /*such as 0=encl, 1=enci, 2=encp, 3=invalid*/
};

#define VOUT_IOC_TYPE                   'C'
#define VOUT_IOC_NR_GET_VINFO           0x0
#define VOUT_IOC_NR_GET_OPTICAL_INFO    0x1
#define VOUT_IOC_NR_GET_VENC_INFO       0x2
#define VOUT_IOC_NR_GET_BL_BRIGHTNESS   0x3
#define VOUT_IOC_NR_SET_BL_BRIGHTNESS   0x4

#define VOUT_IOC_CMD_GET_VINFO   \
		_IOR(VOUT_IOC_TYPE, VOUT_IOC_NR_GET_VINFO, struct vinfo_base_s)
#define VOUT_IOC_CMD_GET_OPTICAL_INFO   \
		_IOR(VOUT_IOC_TYPE, VOUT_IOC_NR_GET_OPTICAL_INFO, struct optical_base_s)
#define VOUT_IOC_CMD_GET_VENC_INFO   \
		_IOR(VOUT_IOC_TYPE, VOUT_IOC_NR_GET_VENC_INFO, struct venc_base_s)
#define VOUT_IOC_CMD_GET_BL_BRIGHTNESS   \
		_IOR(VOUT_IOC_TYPE, VOUT_IOC_NR_GET_BL_BRIGHTNESS, unsigned int)
#define VOUT_IOC_CMD_SET_BL_BRIGHTNESS   \
		_IOW(VOUT_IOC_TYPE, VOUT_IOC_NR_SET_BL_BRIGHTNESS, unsigned int)

#endif
