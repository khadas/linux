/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __INC_VIDEO_COMMON_H__
#define __INC_VIDEO_COMMON_H__

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
#endif
