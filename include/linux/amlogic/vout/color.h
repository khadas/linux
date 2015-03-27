/*
 * Amlogic Frame buffer driver
 *
 * Copyright (C) 2015 Amlogic, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Author: Amlogic Platform-BJ <platform.bj@amlogic.com>
 *
 */

#ifndef _COLOR_H_
#define _COLOR_H_

enum color_index_e {
	COLOR_INDEX_02_PAL4    = 2,  /* 0 */
	COLOR_INDEX_04_PAL16   = 4, /* 0 */
	COLOR_INDEX_08_PAL256 = 8,
	COLOR_INDEX_16_655 = 9,
	COLOR_INDEX_16_844 = 10,
	COLOR_INDEX_16_6442 = 11 ,
	COLOR_INDEX_16_4444_R = 12,
	COLOR_INDEX_16_4642_R = 13,
	COLOR_INDEX_16_1555_A = 14,
	COLOR_INDEX_16_4444_A = 15,
	COLOR_INDEX_16_565 = 16,

	COLOR_INDEX_24_6666_A = 19,
	COLOR_INDEX_24_6666_R = 20,
	COLOR_INDEX_24_8565 = 21,
	COLOR_INDEX_24_5658 = 22,
	COLOR_INDEX_24_888_B = 23,
	COLOR_INDEX_24_RGB = 24,

	COLOR_INDEX_32_BGRA = 29,
	COLOR_INDEX_32_ABGR = 30,
	COLOR_INDEX_32_RGBA = 31,
	COLOR_INDEX_32_ARGB = 32,

	COLOR_INDEX_YUV_422 = 33,
};

#endif
