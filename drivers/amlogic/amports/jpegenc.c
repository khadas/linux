/*
 * drivers/amlogic/amports/jpegenc.c
 *
 * Copyright (C) 2015 Amlogic, Inc. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/ctype.h>
#include <linux/amlogic/amports/ptsserv.h>
#include <linux/amlogic/amports/amstream.h>
#include <linux/amlogic/canvas/canvas.h>
#include <linux/amlogic/canvas/canvas_mgr.h>

#include "vdec_reg.h"
#include "vdec.h"
#include <linux/delay.h>
#include <linux/poll.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/dma-contiguous.h>
#include "amports_config.h"
#include "amvdec.h"
#include "jpegenc.h"
#include "amlog.h"
#include "amports_priv.h"
#include <linux/of_reserved_mem.h>
#ifdef CONFIG_AM_ENCODER
#include "encoder.h"
#endif

#define JPEGENC_CANVAS_INDEX 0xE4
#define JPEGENC_CANVAS_MAX_INDEX 0xE7

#define ENC_CANVAS_OFFSET  JPEGENC_CANVAS_INDEX

#define LOG_ALL 0
#define LOG_INFO 1
#define LOG_DEBUG 2
#define LOG_ERROR 3

#define jenc_pr(level, x...) \
	do { \
		if (level >= jpegenc_print_level) \
			printk(x); \
	} while (0)

#define DRIVER_NAME "jpegenc"
#define CLASS_NAME "jpegenc"
#define DEVICE_NAME "jpegenc"

/* #define EXTEAN_QUANT_TABLE */

static s32 jpegenc_device_major;
static struct device *jpegenc_dev;
static u32 jpegenc_print_level = LOG_DEBUG;

static u32 clock_level = 1;
static u16 gQuantTable[2][DCTSIZE2];
#ifdef EXTEAN_QUANT_TABLE
static u16 *gExternalQuantTablePtr;
static bool external_quant_table_available;
#endif

static DEFINE_SPINLOCK(lock);

#define JPEGENC_BUFFER_LEVEL_VGA   0
#define JPEGENC_BUFFER_LEVEL_2M     1
#define JPEGENC_BUFFER_LEVEL_3M     2
#define JPEGENC_BUFFER_LEVEL_5M     3
#define JPEGENC_BUFFER_LEVEL_8M     4
#define JPEGENC_BUFFER_LEVEL_13M   5
#define JPEGENC_BUFFER_LEVEL_HD     6

const s8 *glevel_str[] = {
	"VGA",
	"2M",
	"3M",
	"5M",
	"8M",
	"13M",
	"HD",
};

const struct Jpegenc_BuffInfo_s jpegenc_buffspec[] = {
	{
		.lev_id = JPEGENC_BUFFER_LEVEL_VGA,
		.max_width = 640,
		.max_height = 640,
		.min_buffsize = 0x330000,
		.input = {
			.buf_start = 0,
			.buf_size = 0x12c000,
		},
		.assit = {
			.buf_start = 0x12d000,
			.buf_size = 0x2000,
		},
		.bitstream = {
			.buf_start = 0x130000,
			.buf_size = 0x200000,
		}
	}, {
		.lev_id = JPEGENC_BUFFER_LEVEL_2M,
		.max_width = 1600,
		.max_height = 1600,
		.min_buffsize = 0x960000,
		.input = {
			.buf_start = 0,
			.buf_size = 0x753000,
		},
		.assit = {
			.buf_start = 0x754000,
			.buf_size = 0x2000,
		},
		.bitstream = {
			.buf_start = 0x760000,
			.buf_size = 0x200000,
		}
	}, {
		.lev_id = JPEGENC_BUFFER_LEVEL_3M,
		.max_width = 2048,
		.max_height = 2048,
		.min_buffsize = 0xe10000,
		.input = {
			.buf_start = 0,
			.buf_size = 0xc00000,
		},
		.assit = {
			.buf_start = 0xc01000,
			.buf_size = 0x2000,
		},
		.bitstream = {
			.buf_start = 0xc10000,
			.buf_size = 0x200000,
		}
	}, {
		.lev_id = JPEGENC_BUFFER_LEVEL_5M,
		.max_width = 2624,
		.max_height = 2624,
		.min_buffsize = 0x1800000,
		.input = {
			.buf_start = 0,
			.buf_size = 0x13B3000,
		},
		.assit = {
			.buf_start = 0x13B4000,
			.buf_size = 0x2000,
		},
		.bitstream = {
			.buf_start = 0x1400000,
			.buf_size = 0x400000,
		}
	}, {
		.lev_id = JPEGENC_BUFFER_LEVEL_8M,
		.max_width = 3264,
		.max_height = 3264,
		.min_buffsize = 0x2300000,
		.input = {
			.buf_start = 0,
			.buf_size = 0x1e7b000,
		},
		.assit = {
			.buf_start = 0x1e7c000,
			.buf_size = 0x2000,
		},
		.bitstream = {
			.buf_start = 0x1f00000,
			.buf_size = 0x400000,
		}
	}, {
		.lev_id = JPEGENC_BUFFER_LEVEL_13M,
		.max_width = 8192,
		.max_height = 8192,
		.min_buffsize = 0xc400000,
		.input = {
			.buf_start = 0,
			.buf_size = 0xc000000,
		},
		.assit = {
			.buf_start = 0xc001000,
			.buf_size = 0x2000,
		},
		.bitstream = {
			.buf_start = 0xc010000,
			.buf_size = 0x3f0000,
		}
	}, {
		.lev_id = JPEGENC_BUFFER_LEVEL_HD,
		.max_width = 8192,
		.max_height = 8192,
		.min_buffsize = 0xc400000,
		.input = {
			.buf_start = 0,
			.buf_size = 0xc000000,
		},
		.assit = {
			.buf_start = 0xc001000,
			.buf_size = 0x2000,
		},
		.bitstream = {
			.buf_start = 0xc010000,
			.buf_size = 0x3f0000,
		}
	}
};

const char *jpegenc_ucode[] = {
	"jpegenc_mc",
};

static struct jpegenc_manager_s gJpegenc;

static const u16 jpeg_quant[7][DCTSIZE2] = {
	{ /* jpeg_quant[0][] : Luma, Canon */
		0x06, 0x06, 0x08, 0x0A, 0x0A, 0x10, 0x15, 0x19,
		0x06, 0x0A, 0x0A, 0x0E, 0x12, 0x1F, 0x29, 0x29,
		0x08, 0x0A, 0x0E, 0x12, 0x21, 0x29, 0x29, 0x29,
		0x0A, 0x0E, 0x12, 0x14, 0x23, 0x29, 0x29, 0x29,
		0x0A, 0x12, 0x21, 0x23, 0x27, 0x29, 0x29, 0x29,
		0x10, 0x1F, 0x29, 0x29, 0x29, 0x29, 0x29, 0x29,
		0x15, 0x29, 0x29, 0x29, 0x29, 0x29, 0x29, 0x29,
		0x19, 0x29, 0x29, 0x29, 0x29, 0x29, 0x29, 0x29
	},
	{ /* jpeg_quant[1][] : Chroma, Canon */
		0x0A, 0x0E, 0x10, 0x14, 0x15, 0x1D, 0x2B, 0x35,
		0x0E, 0x12, 0x14, 0x1D, 0x25, 0x3E, 0x54, 0x54,
		0x10, 0x14, 0x19, 0x25, 0x40, 0x54, 0x54, 0x54,
		0x14, 0x1D, 0x25, 0x27, 0x48, 0x54, 0x54, 0x54,
		0x15, 0x25, 0x40, 0x48, 0x4E, 0x54, 0x54, 0x54,
		0x1D, 0x3E, 0x54, 0x54, 0x54, 0x54, 0x54, 0x54,
		0x2B, 0x54, 0x54, 0x54, 0x54, 0x54, 0x54, 0x54,
		0x35, 0x54, 0x54, 0x54, 0x54, 0x54, 0x54, 0x54
	},
	{ /* jpeg_quant[2][] : Luma, spec example Table K.1 */
		16, 11, 10, 16, 24, 40, 51, 61,
		12, 12, 14, 19, 26, 58, 60, 55,
		14, 13, 16, 24, 40, 57, 69, 56,
		14, 17, 22, 29, 51, 87, 80, 62,
		18, 22, 37, 56, 68, 109, 103, 77,
		24, 35, 55, 64, 81, 104, 113, 92,
		49, 64, 78, 87, 103, 121, 120, 101,
		72, 92, 95, 98, 112, 100, 103, 99
	},
	{ /* jpeg_quant[3][] : Chroma, spec example Table K.2 */
		17, 18, 24, 47, 99, 99, 99, 99,
		18, 21, 26, 66, 99, 99, 99, 99,
		24, 26, 56, 99, 99, 99, 99, 99,
		47, 66, 99, 99, 99, 99, 99, 99,
		99, 99, 99, 99, 99, 99, 99, 99,
		99, 99, 99, 99, 99, 99, 99, 99,
		99, 99, 99, 99, 99, 99, 99, 99,
		99, 99, 99, 99, 99, 99, 99, 99
	},
	{ /* jpeg_quant[4][] : Luma, spec example Table K.1,
		modified to create long ZRL */
		16, 11, 10, 16, 24, 40, 51, 61,
		12, 12, 14, 19, 26, 58, 60, 55,
		14, 13, 16, 24, 40, 57, 69, 56,
		14, 17, 22, 29, 51, 87, 80, 62,
		18, 22, 37, 56, 68, 109, 103, 77,
		24, 35, 55, 64, 81, 104, 113, 92,
		49, 64, 78, 87, 103, 121, 120, 101,
		72, 92, 95, 98, 112, 100, 103, 16
	},
	{ /* jpeg_quant[5][] : Chroma, spec example Table K.2,
		modified to create long ZRL */
		17, 18, 24, 47, 99, 99, 99, 99,
		18, 21, 26, 66, 99, 99, 99, 99,
		24, 26, 56, 99, 99, 99, 99, 99,
		47, 66, 99, 99, 99, 99, 99, 99,
		99, 99, 99, 99, 99, 99, 99, 99,
		99, 99, 99, 99, 99, 99, 99, 99,
		99, 99, 99, 99, 99, 99, 99, 99,
		99, 99, 99, 99, 99, 99, 99, 17
	},
	{ /* jpeg_quant[6][] : no compression */
		1, 1, 1, 1, 1, 1, 1, 1,
		1, 1, 1, 1, 1, 1, 1, 1,
		1, 1, 1, 1, 1, 1, 1, 1,
		1, 1, 1, 1, 1, 1, 1, 1,
		1, 1, 1, 1, 1, 1, 1, 1,
		1, 1, 1, 1, 1, 1, 1, 1,
		1, 1, 1, 1, 1, 1, 1, 1,
		1, 1, 1, 1, 1, 1, 1, 1
	}
}; /* jpeg_quant */

static const u8 jpeg_huffman_dc[2][16 + 12] = {
	{ /* jpeg_huffman_dc[0][] */
		0x00, /* number of code length=1 */
		0x01,
		0x05,
		0x01,
		0x01,
		0x01,
		0x01,
		0x01,
		0x01,
		0x00,
		0x00,
		0x00,
		0x00,
		0x00,
		0x00,
		0x00, /* number of code length=16 */

		/* Entry index for code with
			minimum code length (=2 in this case) */
		0x00,
		0x01, 0x02, 0x03, 0x04, 0x05,
		0x06,
		0x07,
		0x08,
		0x09,
		0x0A,
		0x0B
	},
	{ /* jpeg_huffman_dc[1][] */
		0x00, /* number of code length=1 */
		0x03,
		0x01,
		0x01,
		0x01,
		0x01,
		0x01,
		0x01,
		0x01,
		0x01,
		0x01,
		0x00,
		0x00,
		0x00,
		0x00,
		0x00, /* number of code length=16 */

		/* Entry index for code with
			minimum code length (=2 in this case) */
		0x00, 0x01, 0x02,
		0x03,
		0x04,
		0x05,
		0x06,
		0x07,
		0x08,
		0x09,
		0x0A,
		0x0B
	}
}; /* jpeg_huffman_dc */

static const u8 jpeg_huffman_ac[2][16 + 162] = {
	{ /* jpeg_huffman_ac[0][] */
		0x00, /* number of code length=1 */
		0x02,
		0x01,
		0x03,
		0x03,
		0x02,
		0x04,
		0x03,
		0x05,
		0x05,
		0x04,
		0x04,
		0x00,
		0x00,
		0x01,
		0x7D, /* number of code length=16 */

		/* Entry index for code with
			minimum code length (=2 in this case) */
		0x01, 0x02,
		0x03,
		0x00, 0x04, 0x11,
		0x05, 0x12, 0x21,
		0x31, 0x41,
		0x06, 0x13, 0x51, 0x61,
		0x07, 0x22, 0x71,
		0x14, 0x32, 0x81, 0x91, 0xA1,
		0x08, 0x23, 0x42, 0xB1, 0xC1,
		0x15, 0x52, 0xD1, 0xF0,
		0x24, 0x33, 0x62, 0x72,
		0x82,
		0x09, 0x0A, 0x16, 0x17, 0x18, 0x19,
		0x1A, 0x25, 0x26, 0x27, 0x28, 0x29,
		0x2A, 0x34, 0x35, 0x36,
		0x37, 0x38, 0x39, 0x3A, 0x43, 0x44,
		0x45, 0x46, 0x47, 0x48, 0x49, 0x4A,
		0x53, 0x54, 0x55, 0x56,
		0x57, 0x58, 0x59, 0x5A, 0x63, 0x64,
		0x65, 0x66, 0x67, 0x68, 0x69, 0x6A,
		0x73, 0x74, 0x75, 0x76,
		0x77, 0x78, 0x79, 0x7A, 0x83, 0x84,
		0x85, 0x86, 0x87, 0x88, 0x89, 0x8A,
		0x92, 0x93, 0x94, 0x95,
		0x96, 0x97, 0x98, 0x99, 0x9A, 0xA2,
		0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8,
		0xA9, 0xAA, 0xB2, 0xB3,
		0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9,
		0xBA, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6,
		0xC7, 0xC8, 0xC9, 0xCA,
		0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7,
		0xD8, 0xD9, 0xDA, 0xE1, 0xE2, 0xE3,
		0xE4, 0xE5, 0xE6, 0xE7,
		0xE8, 0xE9, 0xEA, 0xF1, 0xF2, 0xF3,
		0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9,
		0xFA
	},
	{ /* jpeg_huffman_ac[1][] */
		0x00, /* number of code length=1 */
		0x02,
		0x01,
		0x02,
		0x04,
		0x04,
		0x03,
		0x04,
		0x07,
		0x05,
		0x04,
		0x04,
		0x00,
		0x01,
		0x02,
		0x77, /* number of code length=16 */

		/* Entry index for code with
			minimum code length (=2 in this case) */
		0x00, 0x01,
		0x02,
		0x03, 0x11,
		0x04, 0x05, 0x21, 0x31,
		0x06, 0x12, 0x41, 0x51,
		0x07, 0x61, 0x71,
		0x13, 0x22, 0x32, 0x81,
		0x08, 0x14, 0x42, 0x91, 0xA1, 0xB1, 0xC1,
		0x09, 0x23, 0x33, 0x52, 0xF0,
		0x15, 0x62, 0x72, 0xD1,
		0x0A, 0x16, 0x24, 0x34,

		0xE1,
		0x25, 0xF1,
		0x17, 0x18, 0x19, 0x1A, 0x26, 0x27,
		0x28, 0x29, 0x2A, 0x35, 0x36, 0x37,
		0x38, 0x39, 0x3A, 0x43,
		0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
		0x4A, 0x53, 0x54, 0x55, 0x56, 0x57,
		0x58, 0x59, 0x5A, 0x63,
		0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
		0x6A, 0x73, 0x74, 0x75, 0x76, 0x77,
		0x78, 0x79, 0x7A, 0x82,
		0x83, 0x84, 0x85, 0x86, 0x87, 0x88,
		0x89, 0x8A, 0x92, 0x93, 0x94, 0x95,
		0x96, 0x97, 0x98, 0x99,
		0x9A, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6,
		0xA7, 0xA8, 0xA9, 0xAA, 0xB2, 0xB3,
		0xB4, 0xB5, 0xB6, 0xB7,
		0xB8, 0xB9, 0xBA, 0xC2, 0xC3, 0xC4,
		0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA,
		0xD2, 0xD3, 0xD4, 0xD5,
		0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xE2,
		0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8,
		0xE9, 0xEA, 0xF2, 0xF3,
		0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA
	}
}; /* jpeg_huffman_ac */

static void dma_flush(u32 buf_start, u32 buf_size);

static s32 zigzag(s32 i)
{
	s32 zigzag_i;
	switch (i) {
	case 0:
		zigzag_i = 0;
		break;
	case 1:
		zigzag_i = 1;
		break;
	case 2:
		zigzag_i = 8;
		break;
	case 3:
		zigzag_i = 16;
		break;
	case 4:
		zigzag_i = 9;
		break;
	case 5:
		zigzag_i = 2;
		break;
	case 6:
		zigzag_i = 3;
		break;
	case 7:
		zigzag_i = 10;
		break;
	case 8:
		zigzag_i = 17;
		break;
	case 9:
		zigzag_i = 24;
		break;
	case 10:
		zigzag_i = 32;
		break;
	case 11:
		zigzag_i = 25;
		break;
	case 12:
		zigzag_i = 18;
		break;
	case 13:
		zigzag_i = 11;
		break;
	case 14:
		zigzag_i = 4;
		break;
	case 15:
		zigzag_i = 5;
		break;
	case 16:
		zigzag_i = 12;
		break;
	case 17:
		zigzag_i = 19;
		break;
	case 18:
		zigzag_i = 26;
		break;
	case 19:
		zigzag_i = 33;
		break;
	case 20:
		zigzag_i = 40;
		break;
	case 21:
		zigzag_i = 48;
		break;
	case 22:
		zigzag_i = 41;
		break;
	case 23:
		zigzag_i = 34;
		break;
	case 24:
		zigzag_i = 27;
		break;
	case 25:
		zigzag_i = 20;
		break;
	case 26:
		zigzag_i = 13;
		break;
	case 27:
		zigzag_i = 6;
		break;
	case 28:
		zigzag_i = 7;
		break;
	case 29:
		zigzag_i = 14;
		break;
	case 30:
		zigzag_i = 21;
		break;
	case 31:
		zigzag_i = 28;
		break;
	case 32:
		zigzag_i = 35;
		break;
	case 33:
		zigzag_i = 42;
		break;
	case 34:
		zigzag_i = 49;
		break;
	case 35:
		zigzag_i = 56;
		break;
	case 36:
		zigzag_i = 57;
		break;
	case 37:
		zigzag_i = 50;
		break;
	case 38:
		zigzag_i = 43;
		break;
	case 39:
		zigzag_i = 36;
		break;
	case 40:
		zigzag_i = 29;
		break;
	case 41:
		zigzag_i = 22;
		break;
	case 42:
		zigzag_i = 15;
		break;
	case 43:
		zigzag_i = 23;
		break;
	case 44:
		zigzag_i = 30;
		break;
	case 45:
		zigzag_i = 37;
		break;
	case 46:
		zigzag_i = 44;
		break;
	case 47:
		zigzag_i = 51;
		break;
	case 48:
		zigzag_i = 58;
		break;
	case 49:
		zigzag_i = 59;
		break;
	case 50:
		zigzag_i = 52;
		break;
	case 51:
		zigzag_i = 45;
		break;
	case 52:
		zigzag_i = 38;
		break;
	case 53:
		zigzag_i = 31;
		break;
	case 54:
		zigzag_i = 39;
		break;
	case 55:
		zigzag_i = 46;
		break;
	case 56:
		zigzag_i = 53;
		break;
	case 57:
		zigzag_i = 60;
		break;
	case 58:
		zigzag_i = 61;
		break;
	case 59:
		zigzag_i = 54;
		break;
	case 60:
		zigzag_i = 47;
		break;
	case 61:
		zigzag_i = 55;
		break;
	case 62:
		zigzag_i = 62;
		break;
	default:
		zigzag_i = 63;
		break;
	}
	return zigzag_i;
}

/* Perform convertion from Q to 1/Q */
u32 reciprocal(u32 q)
{
	u32 q_recip;

	/* 65535 * (1/q) */
	switch (q) {
	case 0:
		q_recip = 0;
		break;
	case 1:
		q_recip = 65535;
		break;
	case 2:
		q_recip = 32768;
		break;
	case 3:
		q_recip = 21845;
		break;
	case 4:
		q_recip = 16384;
		break;
	case 5:
		q_recip = 13107;
		break;
	case 6:
		q_recip = 10923;
		break;
	case 7:
		q_recip = 9362;
		break;
	case 8:
		q_recip = 8192;
		break;
	case 9:
		q_recip = 7282;
		break;
	case 10:
		q_recip = 6554;
		break;
	case 11:
		q_recip = 5958;
		break;
	case 12:
		q_recip = 5461;
		break;
	case 13:
		q_recip = 5041;
		break;
	case 14:
		q_recip = 4681;
		break;
	case 15:
		q_recip = 4369;
		break;
	case 16:
		q_recip = 4096;
		break;
	case 17:
		q_recip = 3855;
		break;
	case 18:
		q_recip = 3641;
		break;
	case 19:
		q_recip = 3449;
		break;
	case 20:
		q_recip = 3277;
		break;
	case 21:
		q_recip = 3121;
		break;
	case 22:
		q_recip = 2979;
		break;
	case 23:
		q_recip = 2849;
		break;
	case 24:
		q_recip = 2731;
		break;
	case 25:
		q_recip = 2621;
		break;
	case 26:
		q_recip = 2521;
		break;
	case 27:
		q_recip = 2427;
		break;
	case 28:
		q_recip = 2341;
		break;
	case 29:
		q_recip = 2260;
		break;
	case 30:
		q_recip = 2185;
		break;
	case 31:
		q_recip = 2114;
		break;
	case 32:
		q_recip = 2048;
		break;
	case 33:
		q_recip = 1986;
		break;
	case 34:
		q_recip = 1928;
		break;
	case 35:
		q_recip = 1872;
		break;
	case 36:
		q_recip = 1820;
		break;
	case 37:
		q_recip = 1771;
		break;
	case 38:
		q_recip = 1725;
		break;
	case 39:
		q_recip = 1680;
		break;
	case 40:
		q_recip = 1638;
		break;
	case 41:
		q_recip = 1598;
		break;
	case 42:
		q_recip = 1560;
		break;
	case 43:
		q_recip = 1524;
		break;
	case 44:
		q_recip = 1489;
		break;
	case 45:
		q_recip = 1456;
		break;
	case 46:
		q_recip = 1425;
		break;
	case 47:
		q_recip = 1394;
		break;
	case 48:
		q_recip = 1365;
		break;
	case 49:
		q_recip = 1337;
		break;
	case 50:
		q_recip = 1311;
		break;
	case 51:
		q_recip = 1285;
		break;
	case 52:
		q_recip = 1260;
		break;
	case 53:
		q_recip = 1237;
		break;
	case 54:
		q_recip = 1214;
		break;
	case 55:
		q_recip = 1192;
		break;
	case 56:
		q_recip = 1170;
		break;
	case 57:
		q_recip = 1150;
		break;
	case 58:
		q_recip = 1130;
		break;
	case 59:
		q_recip = 1111;
		break;
	case 60:
		q_recip = 1092;
		break;
	case 61:
		q_recip = 1074;
		break;
	case 62:
		q_recip = 1057;
		break;
	case 63:
		q_recip = 1040;
		break;
	case 64:
		q_recip = 1024;
		break;
	case 65:
		q_recip = 1008;
		break;
	case 66:
		q_recip = 993;
		break;
	case 67:
		q_recip = 978;
		break;
	case 68:
		q_recip = 964;
		break;
	case 69:
		q_recip = 950;
		break;
	case 70:
		q_recip = 936;
		break;
	case 71:
		q_recip = 923;
		break;
	case 72:
		q_recip = 910;
		break;
	case 73:
		q_recip = 898;
		break;
	case 74:
		q_recip = 886;
		break;
	case 75:
		q_recip = 874;
		break;
	case 76:
		q_recip = 862;
		break;
	case 77:
		q_recip = 851;
		break;
	case 78:
		q_recip = 840;
		break;
	case 79:
		q_recip = 830;
		break;
	case 80:
		q_recip = 819;
		break;
	case 81:
		q_recip = 809;
		break;
	case 82:
		q_recip = 799;
		break;
	case 83:
		q_recip = 790;
		break;
	case 84:
		q_recip = 780;
		break;
	case 85:
		q_recip = 771;
		break;
	case 86:
		q_recip = 762;
		break;
	case 87:
		q_recip = 753;
		break;
	case 88:
		q_recip = 745;
		break;
	case 89:
		q_recip = 736;
		break;
	case 90:
		q_recip = 728;
		break;
	case 91:
		q_recip = 720;
		break;
	case 92:
		q_recip = 712;
		break;
	case 93:
		q_recip = 705;
		break;
	case 94:
		q_recip = 697;
		break;
	case 95:
		q_recip = 690;
		break;
	case 96:
		q_recip = 683;
		break;
	case 97:
		q_recip = 676;
		break;
	case 98:
		q_recip = 669;
		break;
	case 99:
		q_recip = 662;
		break;
	case 100:
		q_recip = 655;
		break;
	case 101:
		q_recip = 649;
		break;
	case 102:
		q_recip = 643;
		break;
	case 103:
		q_recip = 636;
		break;
	case 104:
		q_recip = 630;
		break;
	case 105:
		q_recip = 624;
		break;
	case 106:
		q_recip = 618;
		break;
	case 107:
		q_recip = 612;
		break;
	case 108:
		q_recip = 607;
		break;
	case 109:
		q_recip = 601;
		break;
	case 110:
		q_recip = 596;
		break;
	case 111:
		q_recip = 590;
		break;
	case 112:
		q_recip = 585;
		break;
	case 113:
		q_recip = 580;
		break;
	case 114:
		q_recip = 575;
		break;
	case 115:
		q_recip = 570;
		break;
	case 116:
		q_recip = 565;
		break;
	case 117:
		q_recip = 560;
		break;
	case 118:
		q_recip = 555;
		break;
	case 119:
		q_recip = 551;
		break;
	case 120:
		q_recip = 546;
		break;
	case 121:
		q_recip = 542;
		break;
	case 122:
		q_recip = 537;
		break;
	case 123:
		q_recip = 533;
		break;
	case 124:
		q_recip = 529;
		break;
	case 125:
		q_recip = 524;
		break;
	case 126:
		q_recip = 520;
		break;
	case 127:
		q_recip = 516;
		break;
	case 128:
		q_recip = 512;
		break;
	case 129:
		q_recip = 508;
		break;
	case 130:
		q_recip = 504;
		break;
	case 131:
		q_recip = 500;
		break;
	case 132:
		q_recip = 496;
		break;
	case 133:
		q_recip = 493;
		break;
	case 134:
		q_recip = 489;
		break;
	case 135:
		q_recip = 485;
		break;
	case 136:
		q_recip = 482;
		break;
	case 137:
		q_recip = 478;
		break;
	case 138:
		q_recip = 475;
		break;
	case 139:
		q_recip = 471;
		break;
	case 140:
		q_recip = 468;
		break;
	case 141:
		q_recip = 465;
		break;
	case 142:
		q_recip = 462;
		break;
	case 143:
		q_recip = 458;
		break;
	case 144:
		q_recip = 455;
		break;
	case 145:
		q_recip = 452;
		break;
	case 146:
		q_recip = 449;
		break;
	case 147:
		q_recip = 446;
		break;
	case 148:
		q_recip = 443;
		break;
	case 149:
		q_recip = 440;
		break;
	case 150:
		q_recip = 437;
		break;
	case 151:
		q_recip = 434;
		break;
	case 152:
		q_recip = 431;
		break;
	case 153:
		q_recip = 428;
		break;
	case 154:
		q_recip = 426;
		break;
	case 155:
		q_recip = 423;
		break;
	case 156:
		q_recip = 420;
		break;
	case 157:
		q_recip = 417;
		break;
	case 158:
		q_recip = 415;
		break;
	case 159:
		q_recip = 412;
		break;
	case 160:
		q_recip = 410;
		break;
	case 161:
		q_recip = 407;
		break;
	case 162:
		q_recip = 405;
		break;
	case 163:
		q_recip = 402;
		break;
	case 164:
		q_recip = 400;
		break;
	case 165:
		q_recip = 397;
		break;
	case 166:
		q_recip = 395;
		break;
	case 167:
		q_recip = 392;
		break;
	case 168:
		q_recip = 390;
		break;
	case 169:
		q_recip = 388;
		break;
	case 170:
		q_recip = 386;
		break;
	case 171:
		q_recip = 383;
		break;
	case 172:
		q_recip = 381;
		break;
	case 173:
		q_recip = 379;
		break;
	case 174:
		q_recip = 377;
		break;
	case 175:
		q_recip = 374;
		break;
	case 176:
		q_recip = 372;
		break;
	case 177:
		q_recip = 370;
		break;
	case 178:
		q_recip = 368;
		break;
	case 179:
		q_recip = 366;
		break;
	case 180:
		q_recip = 364;
		break;
	case 181:
		q_recip = 362;
		break;
	case 182:
		q_recip = 360;
		break;
	case 183:
		q_recip = 358;
		break;
	case 184:
		q_recip = 356;
		break;
	case 185:
		q_recip = 354;
		break;
	case 186:
		q_recip = 352;
		break;
	case 187:
		q_recip = 350;
		break;
	case 188:
		q_recip = 349;
		break;
	case 189:
		q_recip = 347;
		break;
	case 190:
		q_recip = 345;
		break;
	case 191:
		q_recip = 343;
		break;
	case 192:
		q_recip = 341;
		break;
	case 193:
		q_recip = 340;
		break;
	case 194:
		q_recip = 338;
		break;
	case 195:
		q_recip = 336;
		break;
	case 196:
		q_recip = 334;
		break;
	case 197:
		q_recip = 333;
		break;
	case 198:
		q_recip = 331;
		break;
	case 199:
		q_recip = 329;
		break;
	case 200:
		q_recip = 328;
		break;
	case 201:
		q_recip = 326;
		break;
	case 202:
		q_recip = 324;
		break;
	case 203:
		q_recip = 323;
		break;
	case 204:
		q_recip = 321;
		break;
	case 205:
		q_recip = 320;
		break;
	case 206:
		q_recip = 318;
		break;
	case 207:
		q_recip = 317;
		break;
	case 208:
		q_recip = 315;
		break;
	case 209:
		q_recip = 314;
		break;
	case 210:
		q_recip = 312;
		break;
	case 211:
		q_recip = 311;
		break;
	case 212:
		q_recip = 309;
		break;
	case 213:
		q_recip = 308;
		break;
	case 214:
		q_recip = 306;
		break;
	case 215:
		q_recip = 305;
		break;
	case 216:
		q_recip = 303;
		break;
	case 217:
		q_recip = 302;
		break;
	case 218:
		q_recip = 301;
		break;
	case 219:
		q_recip = 299;
		break;
	case 220:
		q_recip = 298;
		break;
	case 221:
		q_recip = 297;
		break;
	case 222:
		q_recip = 295;
		break;
	case 223:
		q_recip = 294;
		break;
	case 224:
		q_recip = 293;
		break;
	case 225:
		q_recip = 291;
		break;
	case 226:
		q_recip = 290;
		break;
	case 227:
		q_recip = 289;
		break;
	case 228:
		q_recip = 287;
		break;
	case 229:
		q_recip = 286;
		break;
	case 230:
		q_recip = 285;
		break;
	case 231:
		q_recip = 284;
		break;
	case 232:
		q_recip = 282;
		break;
	case 233:
		q_recip = 281;
		break;
	case 234:
		q_recip = 280;
		break;
	case 235:
		q_recip = 279;
		break;
	case 236:
		q_recip = 278;
		break;
	case 237:
		q_recip = 277;
		break;
	case 238:
		q_recip = 275;
		break;
	case 239:
		q_recip = 274;
		break;
	case 240:
		q_recip = 273;
		break;
	case 241:
		q_recip = 272;
		break;
	case 242:
		q_recip = 271;
		break;
	case 243:
		q_recip = 270;
		break;
	case 244:
		q_recip = 269;
		break;
	case 245:
		q_recip = 267;
		break;
	case 246:
		q_recip = 266;
		break;
	case 247:
		q_recip = 265;
		break;
	case 248:
		q_recip = 264;
		break;
	case 249:
		q_recip = 263;
		break;
	case 250:
		q_recip = 262;
		break;
	case 251:
		q_recip = 261;
		break;
	case 252:
		q_recip = 260;
		break;
	case 253:
		q_recip = 259;
		break;
	case 254:
		q_recip = 258;
		break;
	default:
		q_recip = 257;
		break;
	}
	return q_recip;
} /* reciprocal */

static void push_word(u8 *base, s32 *offset, u32 word)
{
	u8 *ptr;
	s32 i;
	s32 bytes = (word >> 24) & 0xff;
	for (i = bytes - 1; i >= 0; i--) {
		ptr = base + *offset;
		(*offset)++;
		if (i == 0)
			*ptr = word & 0xff;
		else if (i == 1)
			*ptr = (word >> 8) & 0xff;
		else if (i == 2)
			*ptr = (word >> 16) & 0xff;
	}
}

static s32 jpeg_quality_scaling(s32 quality)
{
	if (quality <= 0)
		quality = 1;
	if (quality > 100)
		quality = 100;

	if (quality < 50)
		quality = 5000 / quality;
	else
		quality = 200 - quality * 2;
	return quality;
}

static void _convert_quant_table(u16 *qtable, u16 *basic_table,
	s32 scale_factor, bool force_baseline)
{
	s32 i = 0;
	s32 temp;
	for (i = 0; i < DCTSIZE2; i++) {
		temp = ((s32)basic_table[i] * scale_factor + 50) / 100;
		/* limit the values to the valid range */
		if (temp <= 0)
			temp = 1;
		/* max quantizer needed for 12 bits */
		if (temp > 32767)
			temp = 32767;
		/* limit to baseline range if requested */
		if (force_baseline && temp > 255)
			temp = 255;
		qtable[i] = (u16)temp;
	}
}

static void convert_quant_table(u16 *qtable, u16 *basic_table,
	s32 scale_factor)
{
	_convert_quant_table(qtable, basic_table, scale_factor, true);
}

static void write_jpeg_quant_lut(s32 table_num)
{
	s32 i;
	u32 data32;

	for (i = 0; i < DCTSIZE2; i += 2) {
		data32 = reciprocal(gQuantTable[table_num][i]);
		data32 |= reciprocal(gQuantTable[table_num][i + 1]) << 16;
		WRITE_HREG(HCODEC_QDCT_JPEG_QUANT_DATA, data32);
	}
}

static void write_jpeg_huffman_lut_dc(s32 table_num)
{
	u32 code_len, code_word, pos, addr;
	u32 num_code_len;
	u32 lut[12];
	u32 i, j;

	code_len = 1;
	code_word = 1;
	pos = 16;

	/* Construct DC Huffman table */
	for (i = 0; i < 16; i++) {
		num_code_len = jpeg_huffman_dc[table_num][i];
		for (j = 0; j < num_code_len; j++) {
			code_word = (code_word + 1) & ((1 << code_len) - 1);
			if (code_len < i + 1) {
				code_word <<= (i + 1 - code_len);
				code_len = i + 1;
			}
			addr = jpeg_huffman_dc[table_num][pos];
			lut[addr] = ((code_len - 1) << 16) | (code_word);
			pos++;
		}
	}

	/* Write DC Huffman table to HW */
	for (i = 0; i < 12; i++)
		WRITE_HREG(HCODEC_VLC_HUFFMAN_DATA, lut[i]);
}

static void write_jpeg_huffman_lut_ac(s32 table_num)
{
	u32 code_len, code_word, pos;
	u32 num_code_len;
	u32 run, size;
	u32 data, addr = 0;
	u32 lut[162];
	u32 i, j;
	code_len = 1;
	code_word = 1;
	pos = 16;

	/* Construct AC Huffman table */
	for (i = 0; i < 16; i++) {
		num_code_len = jpeg_huffman_ac[table_num][i];
		for (j = 0; j < num_code_len; j++) {
			code_word = (code_word + 1) & ((1 << code_len) - 1);
			if (code_len < i + 1) {
				code_word <<= (i + 1 - code_len);
				code_len = i + 1;
			}
			run = jpeg_huffman_ac[table_num][pos] >> 4;
			size = jpeg_huffman_ac[table_num][pos] & 0xf;
			data = ((code_len - 1) << 16) | (code_word);
			if (size == 0) {
				if (run == 0)
					addr = 0;	 /* EOB */
				else if (run == 0xf)
					addr = 161; /* ZRL */
				else
					jenc_pr(LOG_ERROR,
						"Error: Illegal AC Huffman table format!\n");
			} else if (size <= 0xa)
				addr = 1 + 16 * (size - 1) + run;
			else
				jenc_pr(LOG_ERROR,
					"Error: Illegal AC Huffman table format!\n");
			lut[addr] = data;
			pos++;
		}
	}

	/* Write AC Huffman table to HW */
	for (i = 0; i < 162; i++)
		WRITE_HREG(HCODEC_VLC_HUFFMAN_DATA, lut[i]);
}

static void prepare_jpeg_header(struct jpegenc_wq_s *wq)
{
	s32 pic_format;
	s32 pic_width, pic_height;
	s32 q_sel_comp0, q_sel_comp1, q_sel_comp2;
	s32 dc_huff_sel_comp0, dc_huff_sel_comp1, dc_huff_sel_comp2;
	s32 ac_huff_sel_comp0, ac_huff_sel_comp1, ac_huff_sel_comp2;
	s32 lastcoeff_sel;
	s32 jdct_intr_sel;
	s32 h_factor_comp0, v_factor_comp0;
	s32 h_factor_comp1, v_factor_comp1;
	s32 h_factor_comp2, v_factor_comp2;
	s32 q_num;
	s32 tq[2];
	s32 dc_huff_num, ac_huff_num;
	s32 dc_th[2], ac_th[2];
	u32 header_bytes = 0;
	u32 bak_header_bytes = 0;
	s32 i, j;
	u8 *assitbuf = (u8 *)wq->AssitstreamStartVirtAddr;

	if (wq->cmd.output_fmt >= JPEGENC_MAX_FRAME_FMT)
		jenc_pr(LOG_ERROR, "Input format is wrong!\n");
	switch (wq->cmd.output_fmt) {
	case JPEGENC_FMT_NV21:
	case JPEGENC_FMT_NV12:
	case JPEGENC_FMT_YUV420:
		pic_format = 3;
		break;
	case JPEGENC_FMT_YUV422_SINGLE:
		pic_format = 2;
		break;
	case JPEGENC_FMT_YUV444_SINGLE:
	case JPEGENC_FMT_YUV444_PLANE:
		pic_format = 1;
		break;
	default:
		pic_format = 0;
		break;
	}

	pic_width = wq->cmd.encoder_width;
	pic_height = wq->cmd.encoder_height;

	q_sel_comp0 = QUANT_SEL_COMP0;
	q_sel_comp1 = QUANT_SEL_COMP1;
	q_sel_comp2 = QUANT_SEL_COMP2;

	dc_huff_sel_comp0 = DC_HUFF_SEL_COMP0;
	dc_huff_sel_comp1 = DC_HUFF_SEL_COMP1;
	dc_huff_sel_comp2 = DC_HUFF_SEL_COMP2;
	ac_huff_sel_comp0 = AC_HUFF_SEL_COMP0;
	ac_huff_sel_comp1 = AC_HUFF_SEL_COMP1;
	ac_huff_sel_comp2 = AC_HUFF_SEL_COMP2;
	lastcoeff_sel = JDCT_LASTCOEFF_SEL;
	jdct_intr_sel = JDCT_INTR_SEL;

	if (pic_format == 2) {
		/* YUV422 */
		h_factor_comp0 = 1;
		v_factor_comp0 = 0;
		h_factor_comp1 = 0;
		v_factor_comp1 = 0;
		h_factor_comp2 = 0;
		v_factor_comp2 = 0;
	} else if (pic_format == 3) {
		/* YUV420 */
		h_factor_comp0 = 1;
		v_factor_comp0 = 1;
		h_factor_comp1 = 0;
		v_factor_comp1 = 0;
		h_factor_comp2 = 0;
		v_factor_comp2 = 0;
	} else {
		/* RGB or YUV */
		h_factor_comp0 = 0;
		v_factor_comp0 = 0;
		h_factor_comp1 = 0;
		v_factor_comp1 = 0;
		h_factor_comp2 = 0;
		v_factor_comp2 = 0;
	}

	/* SOI marke */
	push_word(assitbuf, &header_bytes,
		(2 << 24) | /* Number of bytes */
		(0xffd8 << 0)); /* data: SOI marker */

	/* Define quantization tables */
	q_num = 1;
	if ((q_sel_comp0 != q_sel_comp1) ||
		(q_sel_comp0 != q_sel_comp2) ||
		(q_sel_comp1 != q_sel_comp2))
		q_num++;
#if 0
	tq[0] = q_sel_comp0;
	tq[1] = (q_sel_comp0 != q_sel_comp1) ? q_sel_comp1 :
		(q_sel_comp0 != q_sel_comp2) ? q_sel_comp2 :
		q_sel_comp0;
#endif
	tq[0] = 0;
	tq[1] = q_num - 1;

	/* data: DQT marker */
	push_word(assitbuf, &header_bytes,
		(2 << 24) | (0xffdb << 0));
	/* data: Lq */
	push_word(assitbuf, &header_bytes,
		(2 << 24) | ((2 + 65 * q_num) << 0));

	/* Add Quantization table bytes */
	/* header_bytes += (2 + (2 + 65 * q_num)); */
	for (i = 0; i < q_num; i++) {
		/* data: {Pq,Tq} */
		push_word(assitbuf, &header_bytes,
			(1 << 24) | (i << 0));
		for (j = 0; j < DCTSIZE2; j++) {
			/* data: Qk */
			push_word(assitbuf, &header_bytes,
				(1 << 24) |
				((gQuantTable[tq[i]][zigzag(j)]) << 0));
		}
	}

	/* Define Huffman tables */
	dc_huff_num = 1;
	if ((dc_huff_sel_comp0 != dc_huff_sel_comp1) ||
		(dc_huff_sel_comp0 != dc_huff_sel_comp2) ||
		(dc_huff_sel_comp1 != dc_huff_sel_comp2))
		dc_huff_num++;

	ac_huff_num = 1;
	if ((ac_huff_sel_comp0 != ac_huff_sel_comp1) ||
		(ac_huff_sel_comp0 != ac_huff_sel_comp2) ||
		(ac_huff_sel_comp1 != ac_huff_sel_comp2))
		ac_huff_num++;

	dc_th[0] = dc_huff_sel_comp0;
	dc_th[1] = (dc_huff_sel_comp0 != dc_huff_sel_comp1) ?
		dc_huff_sel_comp1 : (dc_huff_sel_comp0 != dc_huff_sel_comp2) ?
		dc_huff_sel_comp2 : dc_huff_sel_comp0;

	ac_th[0] = ac_huff_sel_comp0;
	ac_th[1] = (ac_huff_sel_comp0 != ac_huff_sel_comp1) ?
		ac_huff_sel_comp1 : (ac_huff_sel_comp0 != ac_huff_sel_comp2) ?
		ac_huff_sel_comp2 : ac_huff_sel_comp0;

	/* data: DHT marker */
	push_word(assitbuf, &header_bytes,
		(2 << 24) | (0xffc4 << 0));
	/* data: Lh */
	push_word(assitbuf, &header_bytes,
		(2 << 24) |
		((2 + (1 + 16 + 12) * dc_huff_num +
		(1 + 16 + 162) * ac_huff_num) << 0));

	/* Add Huffman table bytes */
	/* data: {Tc,Th} */
	for (i = 0; i < dc_huff_num; i++) {
		push_word(assitbuf, &header_bytes,
			(1 << 24) | (i << 0));
		for (j = 0; j < 16 + 12; j++) {
			/* data: Li then Vi,j */
			push_word(assitbuf, &header_bytes,
				(1 << 24) |
				((jpeg_huffman_dc[dc_th[i]][j]) << 0));
		}
	}
	for (i = 0; i < ac_huff_num; i++) {
		push_word(assitbuf, &header_bytes,
			(1 << 24) |
			(1 << 4) | /* data: Tc */
			(i << 0)); /* data: Th */
		for (j = 0; j < 16 + 162; j++) {
			/* data: Li then Vi,j */
			push_word(assitbuf, &header_bytes,
				(1 << 24) |
				((jpeg_huffman_ac[ac_th[i]][j]) << 0));
		}
	}

	/* Frame header */
	/* Add Frame header bytes */
	/* header_bytes += (2 + (8 + 3 * 3)); */
	push_word(assitbuf, &header_bytes,
		(2 << 24) | /* Number of bytes */
		(0xffc0 << 0)); /* data: SOF_0 marker */
	/* data: Lf */
	push_word(assitbuf, &header_bytes,
		(2 << 24) | ((8 + 3 * 3) << 0));
	/* data: P -- Sample precision */
	push_word(assitbuf,
		&header_bytes, (1 << 24) | (8 << 0));
	/* data: Y -- Number of lines */
	push_word(assitbuf,
		&header_bytes, (2 << 24) | (pic_height << 0));
	/* data: X -- Number of samples per line */
	push_word(assitbuf,
		&header_bytes, (2 << 24) | (pic_width << 0));
	/* data: Nf -- Number of components in a frame */
	push_word(assitbuf,
		&header_bytes, (1 << 24) | (3 << 0));
	/* data: C0 -- Comp0 identifier */
	push_word(assitbuf,
		&header_bytes, (1 << 24) | (0 << 0));
	push_word(assitbuf,
		&header_bytes, (1 << 24) |
		/* data: H0 -- Comp0 horizontal sampling factor */
		((h_factor_comp0 + 1) << 4) |
		/* data: V0 -- Comp0 vertical sampling factor */
		((v_factor_comp0 + 1) << 0));

	/* data: Tq0 -- Comp0 quantization table seletor */
	push_word(assitbuf,
		&header_bytes, (1 << 24) | (0 << 0));
	/* data: C1 -- Comp1 identifier */
	push_word(assitbuf,
		&header_bytes, (1 << 24) | (1 << 0));
	push_word(assitbuf,
		&header_bytes, (1 << 24) |
		/* data: H1 -- Comp1 horizontal sampling factor */
		((h_factor_comp1 + 1) << 4) |
		/* data: V1 -- Comp1 vertical sampling factor */
		((v_factor_comp1 + 1) << 0));
	/* data: Tq1 -- Comp1 quantization table seletor */
	push_word(assitbuf,
		&header_bytes, (1 << 24) |
		(((q_sel_comp0 != q_sel_comp1) ? 1 : 0) << 0));
	/* data: C2 -- Comp2 identifier */
	push_word(assitbuf,
		&header_bytes, (1 << 24) | (2 << 0));
	push_word(assitbuf,
		&header_bytes, (1 << 24) |
		/* data: H2 -- Comp2 horizontal sampling factor */
		((h_factor_comp2 + 1) << 4) |
		/* data: V2 -- Comp2 vertical sampling factor */
		((v_factor_comp2 + 1) << 0));
	/* data: Tq2 -- Comp2 quantization table seletor */
	push_word(assitbuf,
		&header_bytes, (1 << 24) |
		(((q_sel_comp0 != q_sel_comp2) ? 1 : 0) << 0));

	/* Scan header */
	bak_header_bytes = header_bytes + (2 + (6 + 2 * 3));

	/* Add Scan header bytes */
	/* header_bytes += (2 + (6+2*3)); */
	/* If total header bytes is not multiple of 8,
	     then fill 0xff byte between Frame header segment
	     and the Scan header segment. */
	/* header_bytes = ((header_bytes + 7)/8)*8; */
	bak_header_bytes = ((bak_header_bytes + 7) / 8) * 8 - bak_header_bytes;
	for (i = 0; i < bak_header_bytes; i++)
		push_word(assitbuf,
			&header_bytes,
			(1 << 24) | (0xff << 0)); /* 0xff filler */

	push_word(assitbuf,
		&header_bytes,
		(2 << 24) | /* Number of bytes */
		(0xffda << 0)); /* data: SOS marker */

	/* data: Ls */
	push_word(assitbuf,
		&header_bytes, (2 << 24) | ((6 + 2 * 3) << 0));
	/* data: Ns -- Number of components in a scan */
	push_word(assitbuf,
		&header_bytes, (1 << 24) | (3 << 0));
	/* data: Cs0 -- Comp0 identifier */
	push_word(assitbuf,
		&header_bytes, (1 << 24) | (0 << 0));
	push_word(assitbuf,
		&header_bytes, (1 << 24) |
		(0 << 4) | /* data: Td0 -- Comp0 DC Huffman table selector */
		(0 << 0)); /* data: Ta0 -- Comp0 AC Huffman table selector */
	/* data: Cs1 -- Comp1 identifier */
	push_word(assitbuf,
		&header_bytes, (1 << 24) | (1 << 0));
	push_word(assitbuf,
		&header_bytes, (1 << 24) |
		/* data: Td1 -- Comp1 DC Huffman table selector */
		(((dc_huff_sel_comp0 != dc_huff_sel_comp1) ? 1 : 0) << 4) |
		/* data: Ta1 -- Comp1 AC Huffman table selector */
		(((ac_huff_sel_comp0 != ac_huff_sel_comp1) ? 1 : 0) << 0));
	/* data: Cs2 -- Comp2 identifier */
	push_word(assitbuf,
		&header_bytes, (1 << 24) | (2 << 0));
	push_word(assitbuf,
		&header_bytes, (1 << 24) |
		/* data: Td2 -- Comp2 DC Huffman table selector */
		(((dc_huff_sel_comp0 != dc_huff_sel_comp2) ? 1 : 0) << 4) |
		/* data: Ta2 -- Comp2 AC Huffman table selector */
		(((ac_huff_sel_comp0 != ac_huff_sel_comp2) ? 1 : 0) << 0));
	push_word(assitbuf, &header_bytes,
		(3 << 24) |
		(0 << 16) | /* data: Ss = 0 */
		(63 << 8) | /* data: Se = 63 */
		(0 << 4) | /* data: Ah = 0 */
		(0 << 0)); /* data: Al = 0 */
	jenc_pr(LOG_INFO, "jpeg header bytes is %d\n", header_bytes);
	wq->headbytes = header_bytes;
}

static void init_jpeg_encoder(struct jpegenc_wq_s *wq)
{
	u32 data32;
	s32 pic_format; /* 0=RGB; 1=YUV; 2=YUV422; 3=YUV420 */
	s32 pic_x_start, pic_x_end, pic_y_start, pic_y_end;
	s32 pic_width, pic_height;
	s32 q_sel_comp0, q_sel_comp1, q_sel_comp2;
	s32 dc_huff_sel_comp0, dc_huff_sel_comp1, dc_huff_sel_comp2;
	s32 ac_huff_sel_comp0, ac_huff_sel_comp1, ac_huff_sel_comp2;
	s32 lastcoeff_sel;
	s32 jdct_intr_sel;
	s32 h_factor_comp0, v_factor_comp0;
	s32 h_factor_comp1, v_factor_comp1;
	s32 h_factor_comp2, v_factor_comp2;

	jenc_pr(LOG_INFO, "Initialize JPEG Encoder ....\n");
	if (wq->cmd.output_fmt >= JPEGENC_MAX_FRAME_FMT)
		jenc_pr(LOG_ERROR, "Input format is wrong!\n");
	switch (wq->cmd.output_fmt) {
	case JPEGENC_FMT_NV21:
	case JPEGENC_FMT_NV12:
	case JPEGENC_FMT_YUV420:
		pic_format = 3;
		break;
	case JPEGENC_FMT_YUV422_SINGLE:
		pic_format = 2;
		break;
	case JPEGENC_FMT_YUV444_SINGLE:
	case JPEGENC_FMT_YUV444_PLANE:
		pic_format = 1;
		break;
	default:
		pic_format = 0;
		break;
	}

	pic_x_start = 0;
	pic_x_end = wq->cmd.encoder_width - 1;
	pic_y_start = 0;
	pic_y_end = wq->cmd.encoder_height - 1;
	pic_width = wq->cmd.encoder_width;
	pic_height = wq->cmd.encoder_height;

	q_sel_comp0 = wq->cmd.QuantTable_id * 2;
	q_sel_comp1 = q_sel_comp0 + 1;
	q_sel_comp2 = q_sel_comp1;

	dc_huff_sel_comp0 = DC_HUFF_SEL_COMP0;
	dc_huff_sel_comp1 = DC_HUFF_SEL_COMP1;
	dc_huff_sel_comp2 = DC_HUFF_SEL_COMP2;
	ac_huff_sel_comp0 = AC_HUFF_SEL_COMP0;
	ac_huff_sel_comp1 = AC_HUFF_SEL_COMP1;
	ac_huff_sel_comp2 = AC_HUFF_SEL_COMP2;
	lastcoeff_sel = JDCT_LASTCOEFF_SEL;
	jdct_intr_sel = JDCT_INTR_SEL;

	if (pic_format == 2) {
		/* YUV422 */
		h_factor_comp0 = 1;
		v_factor_comp0 = 0;
		h_factor_comp1 = 0;
		v_factor_comp1 = 0;
		h_factor_comp2 = 0;
		v_factor_comp2 = 0;
	} else if (pic_format == 3) {
		/* YUV420 */
		h_factor_comp0 = 1;
		v_factor_comp0 = 1;
		h_factor_comp1 = 0;
		v_factor_comp1 = 0;
		h_factor_comp2 = 0;
		v_factor_comp2 = 0;
	} else {
		/* RGB or YUV */
		h_factor_comp0 = 0;
		v_factor_comp0 = 0;
		h_factor_comp1 = 0;
		v_factor_comp1 = 0;
		h_factor_comp2 = 0;
		v_factor_comp2 = 0;
	}

	/* Configure picture size and format */
	WRITE_HREG(HCODEC_VLC_PIC_SIZE, pic_width | (pic_height << 16));
	WRITE_HREG(HCODEC_VLC_PIC_POSITION, pic_format | (lastcoeff_sel << 4));
	WRITE_HREG(HCODEC_QDCT_JPEG_X_START_END,
		   ((pic_x_end << 16) | (pic_x_start << 0)));
	WRITE_HREG(HCODEC_QDCT_JPEG_Y_START_END,
		   ((pic_y_end << 16) | (pic_y_start << 0)));

	/* Configure quantization tables */
#ifdef EXTEAN_QUANT_TABLE
	if (external_quant_table_available) {
		convert_quant_table(&gQuantTable[0][0],
			&gExternalQuantTablePtr[0],
			wq->cmd.jpeg_quality);
		convert_quant_table(&gQuantTable[1][0],
			&gExternalQuantTablePtr[DCTSIZE2],
			wq->cmd.jpeg_quality);
		q_sel_comp0 = 0;
		q_sel_comp1 = 1;
		q_sel_comp2 = 1;
	} else
#endif
	{
		s32 tq[2];
		tq[0] = q_sel_comp0;
		tq[1] = (q_sel_comp0 != q_sel_comp1) ?
			q_sel_comp1 : (q_sel_comp0 != q_sel_comp2) ?
			q_sel_comp2 : q_sel_comp0;
		convert_quant_table(&gQuantTable[0][0],
			(u16 *)&jpeg_quant[tq[0]],
			wq->cmd.jpeg_quality);
		if (tq[0] != tq[1])
			convert_quant_table(&gQuantTable[1][0],
				(u16 *)&jpeg_quant[tq[1]],
				wq->cmd.jpeg_quality);
		q_sel_comp0 = tq[0];
		q_sel_comp1 = tq[1];
		q_sel_comp2 = tq[1];
	}

	/* Set Quantization LUT start address */
	data32 = 0;
	data32 |= 0 << 8; /* [8] 0=Write LUT, 1=Read */
	data32 |= 0 << 0; /* [5:0] Start addr = 0 */

	WRITE_HREG(HCODEC_QDCT_JPEG_QUANT_ADDR, data32);

	/* Burst-write Quantization LUT data */
	write_jpeg_quant_lut(0);
	if (q_sel_comp0 != q_sel_comp1)
		write_jpeg_quant_lut(1);
#if 0
	write_jpeg_quant_lut(q_sel_comp0);
	if (q_sel_comp1 != q_sel_comp0)
		write_jpeg_quant_lut(q_sel_comp1);
	if ((q_sel_comp2 != q_sel_comp0) && (q_sel_comp2 != q_sel_comp1))
		write_jpeg_quant_lut(q_sel_comp2);
#endif

	/* Configure Huffman tables */

	/* Set DC Huffman LUT start address */
	data32 = 0;
	data32 |= 0 << 16; /* [16] 0=Write LUT, 1=Read */
	data32 |= 0 << 0; /* [8:0] Start addr = 0 */
	WRITE_HREG(HCODEC_VLC_HUFFMAN_ADDR, data32);

	/* Burst-write DC Huffman LUT data */
	write_jpeg_huffman_lut_dc(dc_huff_sel_comp0);
	if (dc_huff_sel_comp1 != dc_huff_sel_comp0)
		write_jpeg_huffman_lut_dc(dc_huff_sel_comp1);

	if ((dc_huff_sel_comp2 != dc_huff_sel_comp0)
		&& (dc_huff_sel_comp2 != dc_huff_sel_comp1))
		write_jpeg_huffman_lut_dc(dc_huff_sel_comp2);

	/* Set AC Huffman LUT start address */
	data32 = 0;
	data32 |= 0 << 16; /* [16] 0=Write LUT, 1=Read */
	data32 |= 24 << 0; /* [8:0] Start addr = 0 */
	WRITE_HREG(HCODEC_VLC_HUFFMAN_ADDR, data32);

	/* Burst-write AC Huffman LUT data */
	write_jpeg_huffman_lut_ac(ac_huff_sel_comp0);
	if (ac_huff_sel_comp1 != ac_huff_sel_comp0)
		write_jpeg_huffman_lut_ac(ac_huff_sel_comp1);

	if ((ac_huff_sel_comp2 != ac_huff_sel_comp0)
		&& (ac_huff_sel_comp2 != ac_huff_sel_comp1))
		write_jpeg_huffman_lut_ac(ac_huff_sel_comp2);

	/* Configure general control registers */
	data32 = 0;
	/* [19:18] dct_inflow_ctrl: 0=No halt; */
	/* 1=DCT halts request at end of each 8x8 block; */
	/* 2=DCT halts request at end of each MCU. */
	data32 |= 0 << 18;
	/* [17:16] jpeg_coeff_last_sel: */
	/* 0=Mark last coeff at the end of an 8x8 block, */
	/* 1=Mark last coeff at the end of an MCU */
	/* 2=Mark last coeff at the end of a scan */
	data32 |= lastcoeff_sel << 16;
	/* [15] jpeg_quant_sel_comp2 */
	data32 |= ((q_sel_comp2 == q_sel_comp0) ? 0 : 1) << 15;
	/* [14] jpeg_v_factor_comp2 */
	data32 |= v_factor_comp2 << 14;
	/* [13] jpeg_h_factor_comp2 */
	data32 |= h_factor_comp2 << 13;
	/* [12] jpeg_comp2_en */
	data32 |= 1 << 12;
	/* [11] jpeg_quant_sel_comp1 */
	data32 |= ((q_sel_comp1 == q_sel_comp0) ? 0 : 1) << 11;
	/* [10] jpeg_v_factor_comp1 */
	data32 |= v_factor_comp1 << 10;
	/* [9] jpeg_h_factor_comp1 */
	data32 |= h_factor_comp1 << 9;
	/* [8] jpeg_comp1_en */
	data32 |= 1 << 8;
	/* [7] jpeg_quant_sel_comp0 */
	data32 |= 0 << 7;
	/* [6] jpeg_v_factor_comp0 */
	data32 |= v_factor_comp0 << 6;
	/* [5] jpeg_h_factor_comp0 */
	data32 |= h_factor_comp0 << 5;
	/* [4] jpeg_comp0_en */
	data32 |= 1 << 4;
	/* [3:1] jdct_intr_sel:0=Disable intr; */
	/* 1=Intr at end of each 8x8 block of DCT input; */
	/* 2=Intr at end of each MCU of DCT input; */
	/* 3=Intr at end of a scan of DCT input; */
	/* 4=Intr at end of each 8x8 block of DCT output; */
	/* 5=Intr at end of each MCU of DCT output; */
	/* 6=Intr at end of a scan of DCT output. */
	data32 |= jdct_intr_sel << 1;
	/* [0] jpeg_en */
	data32 |= 1 << 0;
	WRITE_HREG(HCODEC_QDCT_JPEG_CTRL, data32);

	data32 = 0;
	/* [29] jpeg_comp2_ac_table_sel */
	data32 |= ((ac_huff_sel_comp2 == ac_huff_sel_comp0) ? 0 : 1) << 29;
	/* [28] jpeg_comp2_dc_table_sel */
	data32 |= ((dc_huff_sel_comp2 == dc_huff_sel_comp0) ? 0 : 1) << 28;
	/* [26:25] jpeg_comp2_cnt_max */
	data32 |= ((h_factor_comp2 + 1) * (v_factor_comp2 + 1) - 1) << 25;
	/* [24] jpeg_comp2_en */
	data32 |= 1 << 24;
	/* [21] jpeg_comp1_ac_table_sel */
	data32 |= ((ac_huff_sel_comp1 == ac_huff_sel_comp0) ? 0 : 1) << 21;
	/* [20] jpeg_comp1_dc_table_sel */
	data32 |= ((dc_huff_sel_comp1 == dc_huff_sel_comp0) ? 0 : 1) << 20;
	/* [18:17] jpeg_comp1_cnt_max */
	data32 |= ((h_factor_comp1 + 1) * (v_factor_comp1 + 1) - 1) << 17;
	/* [16] jpeg_comp1_en */
	data32 |= 1 << 16;
	/* [13] jpeg_comp0_ac_table_sel */
	data32 |= 0 << 13;
	/* [12] jpeg_comp0_dc_table_sel */
	data32 |= 0 << 12;
	/* [10:9] jpeg_comp0_cnt_max */
	data32 |= ((h_factor_comp0 + 1) * (v_factor_comp0 + 1) - 1) << 9;
	/* [8] jpeg_comp0_en */
	data32 |= 1 << 8;
	/* [0] jpeg_en, will be enbled by amrisc */
	data32 |= 0 << 0;
	WRITE_HREG(HCODEC_VLC_JPEG_CTRL, data32);

	WRITE_HREG(HCODEC_QDCT_MB_CONTROL,
		(1 << 9) | /* mb_info_soft_reset */
		(1 << 0)); /* mb read buffer soft reset */

	WRITE_HREG(HCODEC_QDCT_MB_CONTROL,
		(0 << 28) | /* ignore_t_p8x8 */
		(0 << 27) | /* zero_mc_out_null_non_skipped_mb */
		(0 << 26) | /* no_mc_out_null_non_skipped_mb */
		(0 << 25) | /* mc_out_even_skipped_mb */
		(0 << 24) | /* mc_out_wait_cbp_ready */
		(0 << 23) | /* mc_out_wait_mb_type_ready */
		(0 << 29) | /* ie_start_int_enable */
		(0 << 19) | /* i_pred_enable */
		(0 << 20) | /* ie_sub_enable */
		(0 << 18) | /* iq_enable */
		(0 << 17) | /* idct_enable */
		(0 << 14) | /* mb_pause_enable */
		(1 << 13) | /* q_enable */
		(1 << 12) | /* dct_enable */
		(0 << 10) | /* mb_info_en */
		(0 << 3) | /* endian */
		(0 << 1) | /* mb_read_en */
		(0 << 0)); /* soft reset */

	/* Assember JPEG file header */
	prepare_jpeg_header(wq);
}

static void jpegenc_init_output_buffer(struct jpegenc_wq_s *wq)
{
	WRITE_HREG(HCODEC_VLC_VB_MEM_CTL,
		((1 << 31) | (0x3f << 24) |
		(0x20 << 16) | (2 << 0)));
	WRITE_HREG(HCODEC_VLC_VB_START_PTR,
		wq->BitstreamStart);
	WRITE_HREG(HCODEC_VLC_VB_WR_PTR,
		wq->BitstreamStart);
	WRITE_HREG(HCODEC_VLC_VB_SW_RD_PTR,
		wq->BitstreamStart);
	WRITE_HREG(HCODEC_VLC_VB_END_PTR,
		wq->BitstreamEnd);
	WRITE_HREG(HCODEC_VLC_VB_CONTROL, 1);
	WRITE_HREG(HCODEC_VLC_VB_CONTROL,
		((0 << 14) | (7 << 3) |
		(1 << 1) | (0 << 0)));
}

static void jpegenc_buffspec_init(struct jpegenc_wq_s *wq)
{
	/* input dct buffer config */
	wq->InputBuffStart = wq->buf_start +
		gJpegenc.mem.bufspec->input.buf_start;
	wq->InputBuffEnd = wq->InputBuffStart +
		gJpegenc.mem.bufspec->input.buf_size - 1;
	jenc_pr(LOG_INFO, "InputBuffStart is 0x%x\n", wq->InputBuffStart);

	/* assit stream buffer config */
	wq->AssitStart =  wq->buf_start +
		gJpegenc.mem.bufspec->assit.buf_start;
	wq->AssitEnd = wq->BitstreamStart +
		gJpegenc.mem.bufspec->assit.buf_size - 1;

	/* output stream buffer config */
	wq->BitstreamStart =  wq->buf_start +
		gJpegenc.mem.bufspec->bitstream.buf_start;
	wq->BitstreamEnd = wq->BitstreamStart +
		gJpegenc.mem.bufspec->bitstream.buf_size - 1;
	jenc_pr(LOG_INFO, "BitstreamStart is 0x%x\n", wq->BitstreamStart);

	wq->AssitstreamStartVirtAddr = phys_to_virt(wq->AssitStart);
	jenc_pr(LOG_INFO, "AssitstreamStartVirtAddr is %p\n",
		wq->AssitstreamStartVirtAddr);
}

/* for temp */
#define HCODEC_MFDIN_REGC_MBLP		(HCODEC_MFDIN_REGB_AMPC + 0x1)
#define HCODEC_MFDIN_REG0D			(HCODEC_MFDIN_REGB_AMPC + 0x2)
#define HCODEC_MFDIN_REG0E			(HCODEC_MFDIN_REGB_AMPC + 0x3)
#define HCODEC_MFDIN_REG0F			(HCODEC_MFDIN_REGB_AMPC + 0x4)
#define HCODEC_MFDIN_REG10			(HCODEC_MFDIN_REGB_AMPC + 0x5)
#define HCODEC_MFDIN_REG11			(HCODEC_MFDIN_REGB_AMPC + 0x6)
#define HCODEC_MFDIN_REG12			(HCODEC_MFDIN_REGB_AMPC + 0x7)
#define HCODEC_MFDIN_REG13			(HCODEC_MFDIN_REGB_AMPC + 0x8)
#define HCODEC_MFDIN_REG14			(HCODEC_MFDIN_REGB_AMPC + 0x9)
#define HCODEC_MFDIN_REG15			(HCODEC_MFDIN_REGB_AMPC + 0xa)
#define HCODEC_MFDIN_REG16			(HCODEC_MFDIN_REGB_AMPC + 0xb)

static void mfdin_basic_jpeg(
	u32 input, u8 iformat, u8 oformat, u32 picsize_x,
	u32 picsize_y, u8 r2y_en, u8 ifmt_extra)
{
	u8 dsample_en; /* Downsample Enable */
	u8 interp_en; /* Interpolation Enable */
	u8 y_size; /* 0:16 Pixels for y direction pickup; 1:8 pixels */
	u8 r2y_mode; /* RGB2YUV Mode, range(0~3) */
	/* mfdin_reg3_canv[25:24]; */
	/* bytes per pixel in x direction for index0, 0:half 1:1 2:2 3:3 */
	u8 canv_idx0_bppx;
	/* mfdin_reg3_canv[27:26]; */
	/* bytes per pixel in x direction for index1-2, 0:half 1:1 2:2 3:3 */
	u8 canv_idx1_bppx;
	/* mfdin_reg3_canv[29:28]; */
	/* bytes per pixel in y direction for index0, 0:half 1:1 2:2 3:3 */
	u8 canv_idx0_bppy;
	/* mfdin_reg3_canv[31:30]; */
	/* bytes per pixel in y direction for index1-2, 0:half 1:1 2:2 3:3 */
	u8 canv_idx1_bppy;
	u8 ifmt444, ifmt422, ifmt420, linear_bytes4p;
	u32 linear_bytesperline;
	bool linear_enable = false;
	s32 reg_offset;
	bool format_err = false;

	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_TXL) {
		if ((iformat == 7) && (ifmt_extra > 2))
			format_err = true;
	} else if (iformat == 7)
		format_err = true;

	if (format_err) {
		jenc_pr(LOG_ERROR,
			"mfdin format err, iformat:%d, ifmt_extra:%d\n",
			iformat, ifmt_extra);
		return;
	}
	if (iformat != 7)
		ifmt_extra = 0;

	ifmt444 = ((iformat == 1) || (iformat == 5) || (iformat == 8)
		|| (iformat == 9) || (iformat == 12)) ? 1 : 0;
	if (iformat == 7 && ifmt_extra == 1)
		ifmt444 = 1;
	ifmt422 = ((iformat == 0) || (iformat == 10)) ? 1 : 0;
	if (iformat == 7 && ifmt_extra != 1)
		ifmt422 = 1;
	ifmt420 = ((iformat == 2) || (iformat == 3) || (iformat == 4)
		|| (iformat == 11)) ? 1 : 0;
	dsample_en = ((ifmt444 && (oformat != 2))
		|| (ifmt422 && (oformat == 0))) ? 1 : 0;
	interp_en = ((ifmt422 && (oformat == 2))
		     || (ifmt420 && (oformat != 0))) ? 1 : 0;
	y_size = (oformat != 0) ? 1 : 0;
	/* r2y_mode = (r2y_en == 1) ? 1 : 0; */
	r2y_mode = 1;
	canv_idx0_bppx = (iformat == 1) ? 3 : (iformat == 0) ? 2 : 1;
	canv_idx1_bppx = (iformat == 4) ? 0 : 1;
	canv_idx0_bppy = 1;
	canv_idx1_bppy = (iformat == 5) ? 1 : 0;
	if ((iformat == 8) || (iformat == 9) || (iformat == 12))
		linear_bytes4p = 3;
	else if (iformat == 10)
		linear_bytes4p = 2;
	else if (iformat == 11)
		linear_bytes4p = 1;
	else
		linear_bytes4p = 0;
	linear_bytesperline = picsize_x * linear_bytes4p;

	if (iformat < 8)
		linear_enable = false;
	else
		linear_enable = true;

	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXBB) {
		reg_offset = -8;
		WRITE_HREG((HCODEC_MFDIN_REG8_DMBL + reg_offset),
			(picsize_x << 14) | (picsize_y << 0));
	} else {
		reg_offset = 0;
		WRITE_HREG((HCODEC_MFDIN_REG8_DMBL + reg_offset),
			(picsize_x << 12) | (picsize_y << 0));
	}

	WRITE_HREG((HCODEC_MFDIN_REG1_CTRL + reg_offset),
		(iformat << 0) | (oformat << 4) |
		(dsample_en << 6) | (y_size << 8) |
		(interp_en << 9) | (r2y_en << 12) |
		(r2y_mode << 13) | (ifmt_extra << 16) |
		(2 << 29));

	if (linear_enable == false) {
		WRITE_HREG((HCODEC_MFDIN_REG3_CANV + reg_offset),
			(input & 0xffffff) |
			(canv_idx1_bppy << 30) |
			(canv_idx0_bppy << 28) |
			(canv_idx1_bppx << 26) |
			(canv_idx0_bppx << 24));
		WRITE_HREG((HCODEC_MFDIN_REG4_LNR0 + reg_offset),
			(0 << 16) | (0 << 0));
		WRITE_HREG((HCODEC_MFDIN_REG5_LNR1 + reg_offset), 0);
	} else {
		WRITE_HREG((HCODEC_MFDIN_REG3_CANV + reg_offset),
			(canv_idx1_bppy << 30) |
			(canv_idx0_bppy << 28) |
			(canv_idx1_bppx << 26) |
			(canv_idx0_bppx << 24));
		WRITE_HREG((HCODEC_MFDIN_REG4_LNR0 + reg_offset),
			(linear_bytes4p << 16) | (linear_bytesperline << 0));
		WRITE_HREG((HCODEC_MFDIN_REG5_LNR1 + reg_offset), input);
	}

	WRITE_HREG((HCODEC_MFDIN_REG9_ENDN + reg_offset),
		(7 << 0) | (6 << 3) | (5 << 6) |
		(4 << 9) | (3 << 12) | (2 << 15) |
		(1 << 18) | (0 << 21));
	return;
}

static s32 set_jpeg_input_format(struct jpegenc_wq_s *wq,
	struct jpegenc_request_s *cmd)
{
	s32 ret = 0;
	u8 iformat = JPEGENC_MAX_FRAME_FMT;
	u8 oformat = JPEGENC_MAX_FRAME_FMT;
	u8 r2y_en = 0;
	u32 picsize_x, picsize_y;
	u32 canvas_w = 0;
	u32 input = cmd->src;
	u8 ifmt_extra = 0;

	jenc_pr(LOG_INFO, "************begin set input format**************\n");
	jenc_pr(LOG_INFO, "type is %d\n", cmd->type);
	jenc_pr(LOG_INFO, "input_fmt is %d\n", cmd->input_fmt);
	jenc_pr(LOG_INFO, "output_fmt is %d\n", cmd->output_fmt);
	jenc_pr(LOG_INFO, "input is 0x%x\n", cmd->src);
	jenc_pr(LOG_INFO, "size is %d\n", cmd->framesize);
	jenc_pr(LOG_INFO, "quality is %d\n", cmd->jpeg_quality);
	jenc_pr(LOG_INFO, "quant tbl_id is %d\n", cmd->QuantTable_id);
	jenc_pr(LOG_INFO, "flush flag is %d\n", cmd->flush_flag);
	jenc_pr(LOG_INFO, "************end set input format**************\n");

	if ((cmd->input_fmt == JPEGENC_FMT_RGB565)
		|| (cmd->input_fmt >= JPEGENC_MAX_FRAME_FMT))
		return -1;

	picsize_x = ((cmd->encoder_width + 15) >> 4) << 4;
	picsize_y = ((cmd->encoder_height + 15) >> 4) << 4;
	if (cmd->output_fmt == JPEGENC_FMT_YUV422_SINGLE)
		oformat = 1;
	else
		oformat = 0;
	if ((cmd->type == JPEGENC_LOCAL_BUFF) ||
		(cmd->type == JPEGENC_PHYSICAL_BUFF)) {
		if (cmd->type == JPEGENC_LOCAL_BUFF) {
			if (cmd->flush_flag & JPEGENC_FLUSH_FLAG_INPUT)
				dma_flush(wq->InputBuffStart,
					cmd->framesize);
			input = wq->InputBuffStart;
		}
		if ((cmd->input_fmt <= JPEGENC_FMT_YUV444_PLANE) ||
			(cmd->input_fmt >= JPEGENC_FMT_YUV422_12BIT))
			r2y_en = 0;
		else
			r2y_en = 1;

		if (cmd->input_fmt >= JPEGENC_FMT_YUV422_12BIT) {
			iformat = 7;
			ifmt_extra =
				cmd->input_fmt - JPEGENC_FMT_YUV422_12BIT;
			if (cmd->input_fmt == JPEGENC_FMT_YUV422_12BIT)
				canvas_w = picsize_x * 24 / 8;
			else if (cmd->input_fmt == JPEGENC_FMT_YUV444_10BIT)
				canvas_w = picsize_x * 32 / 8;
			else
				canvas_w = (picsize_x * 20 + 7) / 8;
			canvas_w = ((canvas_w + 31) >> 5) << 5;
			canvas_config(ENC_CANVAS_OFFSET,
				input,
				canvas_w, picsize_y,
				CANVAS_ADDR_NOWRAP,
				CANVAS_BLKMODE_LINEAR);
			input = ENC_CANVAS_OFFSET;
			input = input & 0xff;
		} else if (cmd->input_fmt == JPEGENC_FMT_YUV422_SINGLE) {
			iformat = 0;
			canvas_w = picsize_x * 2;
			canvas_w = ((canvas_w + 31) >> 5) << 5;
			canvas_config(ENC_CANVAS_OFFSET,
				input,
				canvas_w, picsize_y,
				CANVAS_ADDR_NOWRAP,
				CANVAS_BLKMODE_LINEAR);
			input = ENC_CANVAS_OFFSET;
		} else if ((cmd->input_fmt == JPEGENC_FMT_YUV444_SINGLE)
			|| (cmd->input_fmt == JPEGENC_FMT_RGB888)) {
			iformat = 1;
			if (cmd->input_fmt == JPEGENC_FMT_RGB888)
				r2y_en = 1;
			canvas_w = picsize_x * 3;
			canvas_w = ((canvas_w + 31) >> 5) << 5;
			canvas_config(ENC_CANVAS_OFFSET,
				input,
				canvas_w, picsize_y,
				CANVAS_ADDR_NOWRAP,
				CANVAS_BLKMODE_LINEAR);
			input = ENC_CANVAS_OFFSET;
		} else if ((cmd->input_fmt == JPEGENC_FMT_NV21)
			|| (cmd->input_fmt == JPEGENC_FMT_NV12)) {
			canvas_w = ((cmd->encoder_width + 31) >> 5) << 5;
			iformat = (cmd->input_fmt == JPEGENC_FMT_NV21) ? 2 : 3;
			canvas_config(ENC_CANVAS_OFFSET,
				input,
				canvas_w, picsize_y,
				CANVAS_ADDR_NOWRAP,
				CANVAS_BLKMODE_LINEAR);
			canvas_config(ENC_CANVAS_OFFSET + 1,
				input + canvas_w * picsize_y, canvas_w,
				picsize_y / 2, CANVAS_ADDR_NOWRAP,
				CANVAS_BLKMODE_LINEAR);
			input = ((ENC_CANVAS_OFFSET + 1) << 8) |
				ENC_CANVAS_OFFSET;
		} else if (cmd->input_fmt == JPEGENC_FMT_YUV420) {
			iformat = 4;
			canvas_w = ((cmd->encoder_width + 63) >> 6) << 6;
			canvas_config(ENC_CANVAS_OFFSET,
				input,
				canvas_w, picsize_y,
				CANVAS_ADDR_NOWRAP,
				CANVAS_BLKMODE_LINEAR);
			canvas_config(ENC_CANVAS_OFFSET + 2,
				input + canvas_w * picsize_y,
				canvas_w / 2, picsize_y / 2,
				CANVAS_ADDR_NOWRAP,
				CANVAS_BLKMODE_LINEAR);
			canvas_config(ENC_CANVAS_OFFSET + 2,
				input + canvas_w * picsize_y * 5 / 4,
				canvas_w / 2, picsize_y / 2,
				CANVAS_ADDR_NOWRAP,
				CANVAS_BLKMODE_LINEAR);
			input = ((ENC_CANVAS_OFFSET + 2) << 16) |
				((ENC_CANVAS_OFFSET + 1) << 8) |
				ENC_CANVAS_OFFSET;
		} else if ((cmd->input_fmt == JPEGENC_FMT_YUV444_PLANE)
			|| (cmd->input_fmt == JPEGENC_FMT_RGB888_PLANE)) {
			iformat = 5;
			if (cmd->input_fmt == JPEGENC_FMT_RGB888_PLANE)
				r2y_en = 1;
			canvas_w = ((cmd->encoder_width + 31) >> 5) << 5;
			canvas_config(ENC_CANVAS_OFFSET,
				input,
				canvas_w, picsize_y,
				CANVAS_ADDR_NOWRAP,
				CANVAS_BLKMODE_LINEAR);
			canvas_config(ENC_CANVAS_OFFSET + 1,
				input + canvas_w * picsize_y, canvas_w,
				picsize_y, CANVAS_ADDR_NOWRAP,
				CANVAS_BLKMODE_LINEAR);
			canvas_config(ENC_CANVAS_OFFSET + 2,
				input + canvas_w * picsize_y * 2,
				canvas_w, picsize_y, CANVAS_ADDR_NOWRAP,
				CANVAS_BLKMODE_LINEAR);
			input = ((ENC_CANVAS_OFFSET + 2) << 16) |
				((ENC_CANVAS_OFFSET + 1) << 8) |
				ENC_CANVAS_OFFSET;
		} else if (cmd->input_fmt == JPEGENC_FMT_RGBA8888) {
			iformat = 12;
			r2y_en = 1;
		}
		ret = 0;
	} else if (cmd->type == JPEGENC_CANVAS_BUFF) {
		r2y_en = 0;
		if (cmd->input_fmt == JPEGENC_FMT_YUV422_SINGLE) {
			iformat = 0;
			input = input & 0xff;
		} else if (cmd->input_fmt == JPEGENC_FMT_YUV444_SINGLE) {
			iformat = 1;
			input = input & 0xff;
		} else if ((cmd->input_fmt == JPEGENC_FMT_NV21)
			|| (cmd->input_fmt == JPEGENC_FMT_NV12)) {
			iformat = (cmd->input_fmt == JPEGENC_FMT_NV21) ? 2 : 3;
			input = input & 0xffff;
		} else if (cmd->input_fmt == JPEGENC_FMT_YUV420) {
			iformat = 4;
			input = input & 0xffffff;
		} else if ((cmd->input_fmt == JPEGENC_FMT_YUV444_PLANE)
			|| (cmd->input_fmt == JPEGENC_FMT_RGB888_PLANE)) {
			if (cmd->input_fmt == JPEGENC_FMT_RGB888_PLANE)
				r2y_en = 1;
			iformat = 5;
			input = input & 0xffffff;
		} else if ((cmd->input_fmt == JPEGENC_FMT_YUV422_12BIT)
			|| (cmd->input_fmt == JPEGENC_FMT_YUV444_10BIT)
			|| (cmd->input_fmt == JPEGENC_FMT_YUV422_10BIT)) {
			iformat = 7;
			ifmt_extra = cmd->input_fmt - JPEGENC_FMT_YUV422_12BIT;
			input = input & 0xff;
		} else
			ret = -1;
	}
	if (ret == 0)
		mfdin_basic_jpeg(input, iformat, oformat,
			picsize_x, picsize_y, r2y_en, ifmt_extra);
	return ret;
}

static void jpegenc_isr_tasklet(ulong data)
{
	struct jpegenc_manager_s *manager = (struct jpegenc_manager_s *)data;
	jenc_pr(LOG_INFO, "encoder is done %d\n", manager->encode_hw_status);
	if ((manager->encode_hw_status == JPEGENC_ENCODER_DONE)
		&& (manager->process_irq)) {
		manager->wq.hw_status = manager->encode_hw_status;
		manager->wq.output_size = READ_HREG(HCODEC_VLC_TOTAL_BYTES);
		jenc_pr(LOG_INFO, "encoder size %d\n", manager->wq.output_size);
		atomic_inc(&manager->wq.ready);
		wake_up_interruptible(&manager->wq.complete);
	}
}

static irqreturn_t jpegenc_isr(s32 irq_number, void *para)
{
	struct jpegenc_manager_s *manager = (struct jpegenc_manager_s *)para;
	WRITE_HREG(HCODEC_ASSIST_MBOX2_CLR_REG, 1);
	manager->encode_hw_status  = READ_HREG(JPEGENC_ENCODER_STATUS);
	if (manager->encode_hw_status == JPEGENC_ENCODER_DONE) {
		manager->process_irq = true;
		tasklet_schedule(&manager->tasklet);
	}
	return IRQ_HANDLED;
}

static void jpegenc_start(void)
{
	READ_VREG(DOS_SW_RESET1);
	READ_VREG(DOS_SW_RESET1);
	READ_VREG(DOS_SW_RESET1);

	WRITE_VREG(DOS_SW_RESET1, (1 << 12) | (1 << 11));
	WRITE_VREG(DOS_SW_RESET1, 0);

	READ_VREG(DOS_SW_RESET1);
	READ_VREG(DOS_SW_RESET1);
	READ_VREG(DOS_SW_RESET1);

	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXBB)
		WRITE_HREG((HCODEC_MFDIN_REG7_SCMD - 8), (1 << 28)),
	WRITE_HREG(HCODEC_MPSR, 0x0001);
}

static void _jpegenc_stop(void)
{
	ulong timeout = jiffies + HZ;

	WRITE_HREG(HCODEC_MPSR, 0);
	WRITE_HREG(HCODEC_CPSR, 0);
	while (READ_HREG(HCODEC_IMEM_DMA_CTRL) & 0x8000) {
		if (time_after(jiffies, timeout))
			break;
	}
	READ_VREG(DOS_SW_RESET1);
	READ_VREG(DOS_SW_RESET1);
	READ_VREG(DOS_SW_RESET1);

	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXBB)
		WRITE_VREG(DOS_SW_RESET1,
			   (1 << 12) | (1 << 11) |
			   (1 << 2) | (1 << 6) |
			   (1 << 7) | (1 << 8) |
			   (1 << 14) | (1 << 16) |
			   (1 << 17));
	else
		WRITE_VREG(DOS_SW_RESET1,
			   (1 << 12) | (1 << 11) |
			   (1 << 2) | (1 << 6) |
			   (1 << 7) | (1 << 8) |
			   (1 << 16) | (1 << 17));

	WRITE_VREG(DOS_SW_RESET1, 0);

	READ_VREG(DOS_SW_RESET1);
	READ_VREG(DOS_SW_RESET1);
	READ_VREG(DOS_SW_RESET1);
}

static void __iomem *mc_addr;
static u32 mc_addr_map;
#define MC_SIZE (4096 * 4)
s32 jpegenc_loadmc(const char *p)
{
	ulong timeout;
	s32 ret = 0;

	/* use static mempry*/
	if (mc_addr == NULL) {
		mc_addr = kmalloc(MC_SIZE, GFP_KERNEL);
		if (!mc_addr) {
			jenc_pr(LOG_ERROR,
				"jpegenc loadmc iomap mc addr error.\n");
			return -ENOMEM;
		}
	}

	ret = get_decoder_firmware_data(VFORMAT_JPEG_ENC, p,
		(u8 *)mc_addr, MC_SIZE);
	if (ret < 0) {
		jenc_pr(LOG_ERROR,
			"jpegenc microcode fail ret=%d, name: %s.\n",
			ret, p);
	}

	mc_addr_map = dma_map_single(
		&gJpegenc.this_pdev->dev,
		mc_addr, MC_SIZE, DMA_TO_DEVICE);

	WRITE_HREG(HCODEC_MPSR, 0);
	WRITE_HREG(HCODEC_CPSR, 0);

	/* Read CBUS register for timing */
	timeout = READ_HREG(HCODEC_MPSR);
	timeout = READ_HREG(HCODEC_MPSR);

	timeout = jiffies + HZ;

	WRITE_HREG(HCODEC_IMEM_DMA_ADR, mc_addr_map);
	WRITE_HREG(HCODEC_IMEM_DMA_COUNT, 0x1000);
	WRITE_HREG(HCODEC_IMEM_DMA_CTRL, (0x8000 | (7 << 16)));

	while (READ_HREG(HCODEC_IMEM_DMA_CTRL) & 0x8000) {
		if (time_before(jiffies, timeout))
			schedule();
		else {
			jenc_pr(LOG_ERROR, "hcodec load mc error\n");
			ret = -EBUSY;
			break;
		}
	}

	dma_unmap_single(
		&gJpegenc.this_pdev->dev,
		mc_addr_map, MC_SIZE, DMA_TO_DEVICE);
	return ret;
}

bool jpegenc_on(void)
{
	bool hcodec_on;
	ulong flags;

	spin_lock_irqsave(&lock, flags);

	hcodec_on = vdec_on(VDEC_HCODEC);
	hcodec_on &= (gJpegenc.opened > 0);

	spin_unlock_irqrestore(&lock, flags);
	return hcodec_on;
}

static s32 jpegenc_poweron(u32 clock)
{
	ulong flags;
	u32 data32;

	data32 = 0;

	amports_switch_gate("vdec", 1);

	spin_lock_irqsave(&lock, flags);

	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_M8) {
		WRITE_AOREG(AO_RTI_PWR_CNTL_REG0,
			(READ_AOREG(AO_RTI_PWR_CNTL_REG0) & (~0x18)));
		udelay(10);
		/* Powerup HCODEC */
		/* [1:0] HCODEC */
		WRITE_AOREG(AO_RTI_GEN_PWR_SLEEP0,
			(READ_AOREG(AO_RTI_GEN_PWR_SLEEP0) & (~0x3)));
		udelay(10);
	}

	WRITE_VREG(DOS_SW_RESET1, 0xffffffff);
	WRITE_VREG(DOS_SW_RESET1, 0);

	/* Enable Dos internal clock gating */
	jpegenc_clock_enable(clock);

	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_M8) {
		/* Powerup HCODEC memories */
		WRITE_VREG(DOS_MEM_PD_HCODEC, 0x0);

		/* Remove HCODEC ISO */
		WRITE_AOREG(AO_RTI_GEN_PWR_ISO0,
			(READ_AOREG(AO_RTI_GEN_PWR_ISO0) & (~0x30)));
		udelay(10);
	}
	/* Disable auto-clock gate */
	WRITE_VREG(DOS_GEN_CTRL0, (READ_VREG(DOS_GEN_CTRL0) | 0x1));
	WRITE_VREG(DOS_GEN_CTRL0, (READ_VREG(DOS_GEN_CTRL0) & 0xFFFFFFFE));

	spin_unlock_irqrestore(&lock, flags);

	mdelay(10);
	return 0;
}

static s32 jpegenc_poweroff(void)
{
	ulong flags;

	spin_lock_irqsave(&lock, flags);

	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_M8) {
		/* enable HCODEC isolation */
		WRITE_AOREG(AO_RTI_GEN_PWR_ISO0,
			READ_AOREG(AO_RTI_GEN_PWR_ISO0) | 0x30);
		/* power off HCODEC memories */
		WRITE_VREG(DOS_MEM_PD_HCODEC, 0xffffffffUL);
	}
	/* disable HCODEC clock */
	jpegenc_clock_disable();

	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_M8) {
		/* HCODEC power off */
		WRITE_AOREG(AO_RTI_GEN_PWR_SLEEP0,
			READ_AOREG(AO_RTI_GEN_PWR_SLEEP0) | 0x3);
	}

	spin_unlock_irqrestore(&lock, flags);

	/* release DOS clk81 clock gating */
	amports_switch_gate("vdec", 0);
	return 0;
}

void jpegenc_reset(void)
{
	READ_VREG(DOS_SW_RESET1);
	READ_VREG(DOS_SW_RESET1);
	READ_VREG(DOS_SW_RESET1);
	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXBB)
		WRITE_VREG(DOS_SW_RESET1,
			   (1 << 2) | (1 << 6) |
			   (1 << 7) | (1 << 8) |
			   (1 << 14) | (1 << 16) |
			   (1 << 17));
	else
		WRITE_VREG(DOS_SW_RESET1,
			   (1 << 2) | (1 << 6) | (1 << 7) |
			   (1 << 8) | (1 << 16) | (1 << 17));
	WRITE_VREG(DOS_SW_RESET1, 0);
	READ_VREG(DOS_SW_RESET1);
	READ_VREG(DOS_SW_RESET1);
	READ_VREG(DOS_SW_RESET1);
}

static s32 jpegenc_init(void)
{
	jpegenc_poweron(clock_level);
	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_MG9TV)
		WRITE_HREG(HCODEC_ASSIST_MMC_CTRL1, 0x32);
	else
		WRITE_HREG(HCODEC_ASSIST_MMC_CTRL1, 0x2);

	jenc_pr(LOG_ALL, "start to load microcode\n");
	if (jpegenc_loadmc(jpegenc_ucode[0]) < 0)
		return -EBUSY;
	jenc_pr(LOG_ALL, "succeed to load microcode\n");
	gJpegenc.process_irq = false;

	if (request_irq(gJpegenc.irq_num, jpegenc_isr, IRQF_SHARED,
		"jpegenc-irq", (void *)&gJpegenc) == 0)
		gJpegenc.irq_requested = true;
	else
		gJpegenc.irq_requested = false;
	WRITE_HREG(JPEGENC_ENCODER_STATUS, JPEGENC_ENCODER_IDLE);
	gJpegenc.inited = true;
	return 0;
}

static s32 convert_cmd(struct jpegenc_wq_s *wq, u32 *cmd_info)
{
	if (!wq) {
		jenc_pr(LOG_ERROR, "jpegenc convert_cmd error\n");
		return -1;
	}
	memset(&wq->cmd, 0, sizeof(struct jpegenc_request_s));
	wq->cmd.type = cmd_info[0];
	wq->cmd.input_fmt = cmd_info[1];
	wq->cmd.output_fmt = cmd_info[2];
	wq->cmd.encoder_width = cmd_info[3];
	wq->cmd.encoder_height = cmd_info[4];
	wq->cmd.framesize = cmd_info[5];
	wq->cmd.src = cmd_info[6];
	wq->cmd.jpeg_quality = cmd_info[7];
	wq->cmd.QuantTable_id = cmd_info[8];
	wq->cmd.flush_flag = cmd_info[9];
	if ((wq->cmd.encoder_width > wq->max_width) ||
		(wq->cmd.encoder_height > wq->max_height)) {
		jenc_pr(LOG_ERROR,
			"set encode size %dx%d is larger than supported (%dx%d).\n",
			wq->cmd.encoder_width,
			wq->cmd.encoder_height,
			wq->max_width,
			wq->max_height);
		return -1;
	}
	wq->cmd.jpeg_quality = jpeg_quality_scaling((s32)cmd_info[7]);
	if (wq->cmd.QuantTable_id < 4) {
		jenc_pr(LOG_INFO,
			"JPEGENC_SEL_QUANT_TABLE : %d.\n",
			wq->cmd.QuantTable_id);
	} else {
		wq->cmd.QuantTable_id = 0;
		jenc_pr(LOG_ERROR,
			"JPEGENC_SEL_QUANT_TABLE invaild. target value: %d.\n",
			cmd_info[8]);
	}
	jenc_pr(LOG_INFO,
		"target quality : %d,  jpeg_quality value: %d.\n",
		cmd_info[7], wq->cmd.jpeg_quality);
	return 0;
}

static void jpegenc_start_cmd(struct jpegenc_wq_s *wq)
{
	gJpegenc.process_irq = false;
	gJpegenc.encode_hw_status = JPEGENC_ENCODER_IDLE;

	jpegenc_reset();
	set_jpeg_input_format(wq, &wq->cmd);

	init_jpeg_encoder(wq);
	jpegenc_init_output_buffer(wq);
	/* clear mailbox interrupt */
	WRITE_HREG(HCODEC_ASSIST_MBOX2_CLR_REG, 1);

	/* enable mailbox interrupt */
	WRITE_HREG(HCODEC_ASSIST_MBOX2_MASK, 1);
	gJpegenc.encode_hw_status = JPEGENC_ENCODER_IDLE;
	WRITE_HREG(JPEGENC_ENCODER_STATUS, JPEGENC_ENCODER_IDLE);
	gJpegenc.process_irq = false;
	jpegenc_start();
	jenc_pr(LOG_INFO, "jpegenc_start\n");
}

static void jpegenc_stop(void)
{
	if ((gJpegenc.irq_num >= 0) &&
		(gJpegenc.irq_requested == true)) {
		free_irq(gJpegenc.irq_num, &gJpegenc);
		gJpegenc.irq_requested = false;
	}
	_jpegenc_stop();
	jpegenc_poweroff();
	jenc_pr(LOG_INFO, "jpegenc_stop\n");
}

static void dma_flush(u32 buf_start, u32 buf_size)
{
	dma_sync_single_for_device(&gJpegenc.this_pdev->dev,
		buf_start, buf_size, DMA_TO_DEVICE);
}

static void cache_flush(u32 buf_start, u32 buf_size)
{
	dma_sync_single_for_cpu(&gJpegenc.this_pdev->dev,
		buf_start, buf_size, DMA_FROM_DEVICE);
}

static s32 jpegenc_open(struct inode *inode, struct file *file)
{
	struct jpegenc_wq_s *wq;
	s32 r;
	jenc_pr(LOG_DEBUG, "jpegenc open\n");
#ifdef CONFIG_AM_ENCODER
	if (amvenc_avc_on() == true) {
		jenc_pr(LOG_ERROR, "hcodec in use for AVC Encode now.\n");
		return -EBUSY;
	}
#endif
	file->private_data = NULL;

	spin_lock(&gJpegenc.sem_lock);
	if (gJpegenc.opened > 0) {
		spin_unlock(&gJpegenc.sem_lock);
		jenc_pr(LOG_ERROR, "jpegenc open busy.\n");
		return -EBUSY;
	}
	wq = &gJpegenc.wq;
	wq->buf_start = gJpegenc.mem.buf_start;
	wq->buf_size = gJpegenc.mem.buf_size;
	gJpegenc.opened++;
	spin_unlock(&gJpegenc.sem_lock);
#ifdef CONFIG_CMA
	if (gJpegenc.use_reserve == false) {
		wq->venc_pages = dma_alloc_from_contiguous(
			&gJpegenc.this_pdev->dev,
			gJpegenc.mem.buf_size >> PAGE_SHIFT, 0);
		if (wq->venc_pages) {
			wq->buf_start = page_to_phys(wq->venc_pages);
			wq->buf_size = gJpegenc.mem.buf_size;
			jenc_pr(LOG_DEBUG,
				"allocating phys 0x%x, size %dk.\n",
				wq->buf_start, wq->buf_size >> 10);
		} else {
			jenc_pr(LOG_ERROR,
				"CMA failed to allocate dma buffer for %s.\n",
				gJpegenc.this_pdev->name);
			spin_lock(&gJpegenc.sem_lock);
			gJpegenc.opened--;
			spin_unlock(&gJpegenc.sem_lock);
			return -ENOMEM;
		}
	}
#endif
	spin_lock(&gJpegenc.sem_lock);
	init_waitqueue_head(&wq->complete);
	atomic_set(&wq->ready, 0);
	wq->AssitstreamStartVirtAddr = NULL;
	memset(gQuantTable, 0, sizeof(gQuantTable));
	wq->cmd.QuantTable_id = 0;
	wq->cmd.jpeg_quality = 90;
	wq->max_width = gJpegenc.mem.bufspec->max_width;
	wq->max_height = gJpegenc.mem.bufspec->max_height;
	wq->headbytes = 0;
	file->private_data = (void *)wq;
#ifdef EXTEAN_QUANT_TABLE
	gExternalQuantTablePtr = NULL;
	external_quant_table_available = false;
#endif
	spin_unlock(&gJpegenc.sem_lock);
	r = 0;
	return r;
}

static s32 jpegenc_release(struct inode *inode, struct file *file)
{
	struct jpegenc_wq_s *wq = (struct jpegenc_wq_s *)file->private_data;

	if (wq != &gJpegenc.wq) {
		jenc_pr(LOG_ERROR, "jpegenc release error\n");
		return -1;
	}
	if (gJpegenc.inited) {
		jpegenc_stop();
		gJpegenc.inited = false;
	}
	memset(gQuantTable, 0, sizeof(gQuantTable));

	if (wq->AssitstreamStartVirtAddr)
		wq->AssitstreamStartVirtAddr = NULL;

#ifdef CONFIG_CMA
	if (wq->venc_pages) {
		dma_release_from_contiguous(
			&gJpegenc.this_pdev->dev,
			wq->venc_pages,
			wq->buf_size >> PAGE_SHIFT);
		wq->venc_pages = NULL;
	}
#endif
	wq->buf_start = 0;
	wq->buf_size = 0;
#ifdef EXTEAN_QUANT_TABLE
	kfree(gExternalQuantTablePtr);
	gExternalQuantTablePtr = NULL;
	external_quant_table_available = false;
#endif
	spin_lock(&gJpegenc.sem_lock);
	if (gJpegenc.opened > 0)
		gJpegenc.opened--;
	spin_unlock(&gJpegenc.sem_lock);
	jenc_pr(LOG_DEBUG, "jpegenc release\n");
	return 0;
}

static long jpegenc_ioctl(struct file *file, u32 cmd, ulong arg)
{
	long r = 0;
	struct jpegenc_wq_s *wq = (struct jpegenc_wq_s *)file->private_data;
#define MAX_ADDR_INFO_SIZE 30
	u32 addr_info[MAX_ADDR_INFO_SIZE + 4];
	switch (cmd) {
	case JPEGENC_IOC_NEW_CMD:
		if (copy_from_user(addr_info, (void *)arg,
			MAX_ADDR_INFO_SIZE * sizeof(u32))) {
			jenc_pr(LOG_ERROR,
				"jpegenc get new cmd error.\n");
			return -1;
		}
		if (!convert_cmd(wq, addr_info))
			jpegenc_start_cmd(wq);
		break;
	case JPEGENC_IOC_GET_STAGE:
		put_user(wq->hw_status, (u32 *)arg);
		break;
	case JPEGENC_IOC_GET_OUTPUT_SIZE:
		cache_flush(wq->BitstreamStart, wq->output_size);
		addr_info[0] = wq->headbytes;
		addr_info[1] = wq->output_size;
		r = copy_to_user((u32 *)arg, addr_info , 2 * sizeof(u32));
		break;
	case JPEGENC_IOC_CONFIG_INIT:
		jpegenc_init();
		jpegenc_buffspec_init(wq);
		break;
	case JPEGENC_IOC_GET_BUFFINFO:
		addr_info[0] = gJpegenc.mem.buf_size;
		addr_info[1] = gJpegenc.mem.bufspec->input.buf_start;
		addr_info[2] = gJpegenc.mem.bufspec->input.buf_size;
		addr_info[3] = gJpegenc.mem.bufspec->assit.buf_start;
		addr_info[4] = gJpegenc.mem.bufspec->assit.buf_size;
		addr_info[5] = gJpegenc.mem.bufspec->bitstream.buf_start;
		addr_info[6] = gJpegenc.mem.bufspec->bitstream.buf_size;
		r = copy_to_user((u32 *)arg, addr_info , 7 * sizeof(u32));
		break;
	case JPEGENC_IOC_GET_DEVINFO:
		if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXL) {
			/* GXL send same id of GXTVBB to upper*/
			r = copy_to_user((s8 *)arg, JPEGENC_DEVINFO_GXTVBB,
				strlen(JPEGENC_DEVINFO_GXTVBB));
		} else if (get_cpu_type() == MESON_CPU_MAJOR_ID_GXTVBB) {
			r = copy_to_user((s8 *)arg, JPEGENC_DEVINFO_GXTVBB,
				strlen(JPEGENC_DEVINFO_GXTVBB));
		} else if (get_cpu_type() == MESON_CPU_MAJOR_ID_GXBB) {
			r = copy_to_user((s8 *)arg, JPEGENC_DEVINFO_GXBB,
				strlen(JPEGENC_DEVINFO_GXBB));
		} else if (get_cpu_type() == MESON_CPU_MAJOR_ID_MG9TV) {
			r = copy_to_user((s8 *)arg, JPEGENC_DEVINFO_G9,
				strlen(JPEGENC_DEVINFO_G9));
		} else {
			r = copy_to_user((s8 *)arg, JPEGENC_DEVINFO_M8,
				strlen(JPEGENC_DEVINFO_M8));
		}
		break;
	case JPEGENC_IOC_SET_EXT_QUANT_TABLE:
#ifdef EXTEAN_QUANT_TABLE
		if (arg == 0) {
			kfree(gExternalQuantTablePtr);
			gExternalQuantTablePtr = NULL;
			external_quant_table_available = false;
		} else {
			void __user *argp = (void __user *)arg;
			gExternalQuantTablePtr =
				kmalloc(sizeof(u16) * DCTSIZE2 * 2,
				GFP_KERNEL);
			if (gExternalQuantTablePtr) {
				if (copy_from_user
					(gExternalQuantTablePtr, argp,
					sizeof(u16) * DCTSIZE2 * 2)) {
					r = -1;
					break;
				}
				external_quant_table_available = true;
				r = 0;
			} else {
				jenc_pr(LOG_ERROR,
					"gExternalQuantTablePtr malloc fail\n");
				r = -1;
			}
		}
#else
		r = 0;
#endif
		break;
	default:
		r = -1;
		break;
	}
	return r;
}

#ifdef CONFIG_COMPAT
static long jpegenc_compat_ioctl(struct file *filp,
	unsigned int cmd, unsigned long args)
{
	unsigned long ret;

	args = (unsigned long)compat_ptr(args);
	ret = jpegenc_ioctl(filp, cmd, args);
	return ret;
}
#endif

static s32 jpegenc_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct jpegenc_wq_s *wq = (struct jpegenc_wq_s *)filp->private_data;
	ulong off = vma->vm_pgoff << PAGE_SHIFT;
	ulong vma_size = vma->vm_end - vma->vm_start;

	if (vma_size == 0) {
		jenc_pr(LOG_ERROR, "vma_size is 0\n");
		return -EAGAIN;
	}
	off += wq->buf_start;
	jenc_pr(LOG_INFO, "vma_size is %ld, off is %ld\n", vma_size, off);
	vma->vm_flags |= VM_DONTEXPAND | VM_DONTDUMP | VM_IO;
	/* vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot); */
	if (remap_pfn_range(vma, vma->vm_start, off >> PAGE_SHIFT,
		vma->vm_end - vma->vm_start, vma->vm_page_prot)) {
		jenc_pr(LOG_ERROR, "set_cached: failed remap_pfn_range\n");
		return -EAGAIN;
	}
	return 0;
}

static u32 jpegenc_poll(struct file *file, poll_table *wait_table)
{
	struct jpegenc_wq_s *wq = (struct jpegenc_wq_s *)file->private_data;
	poll_wait(file, &wq->complete, wait_table);

	if (atomic_read(&wq->ready)) {
		atomic_dec(&wq->ready);
		return POLLIN | POLLRDNORM;
	}
	return 0;
}

static const struct file_operations jpegenc_fops = {
	.owner = THIS_MODULE,
	.open = jpegenc_open,
	.mmap = jpegenc_mmap,
	.release = jpegenc_release,
	.unlocked_ioctl = jpegenc_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = jpegenc_compat_ioctl,
#endif
	.poll = jpegenc_poll,
};

static s32 jpegenc_wq_init(void)
{
	jenc_pr(LOG_DEBUG, "jpegenc_wq_init.\n");
	gJpegenc.irq_requested = false;
	gJpegenc.process_irq = false;
	gJpegenc.inited = false;
	gJpegenc.opened = 0;
	gJpegenc.encode_hw_status = JPEGENC_ENCODER_IDLE;
	spin_lock_init(&gJpegenc.sem_lock);

	tasklet_init(&gJpegenc.tasklet,
		     jpegenc_isr_tasklet,
		     (ulong)&gJpegenc);

	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXBB)
		clock_level = 5;
	else if (get_cpu_type() >= MESON_CPU_MAJOR_ID_M8M2)
		clock_level = 3;
	else
		clock_level = 1;
	return 0;
}

static s32 jpegenc_wq_uninit(void)
{
	s32 r = -1;
	jenc_pr(LOG_DEBUG, "uninit encode wq.\n");
	if (gJpegenc.encode_hw_status == JPEGENC_ENCODER_IDLE) {
		if ((gJpegenc.irq_num >= 0) &&
			(gJpegenc.irq_requested == true)) {
			free_irq(gJpegenc.irq_num, &gJpegenc);
			gJpegenc.irq_requested = false;
		}
		r = 0;
	}
	return  r;
}

static ssize_t jpegenc_status_show(struct class *cla,
	struct class_attribute *attr, char *buf)
{
	s32 irq_num;
	u32 hw_status, width, height;
	bool process_irq;
	bool inited;
	bool use_reserve;
	u32 cma_size, max_w, max_h;
	u32 buffer_start, buffer_size;
	u8 lev, opened;
	struct Jpegenc_Buff_s res;

	spin_lock(&gJpegenc.sem_lock);

	irq_num = gJpegenc.irq_num;
	hw_status = gJpegenc.encode_hw_status;
	process_irq = gJpegenc.process_irq;
	inited = gJpegenc.inited;
	opened = gJpegenc.opened;
	use_reserve = gJpegenc.use_reserve;
	res.buf_start = gJpegenc.mem.reserve_mem.buf_start;
	res.buf_size = gJpegenc.mem.reserve_mem.buf_size;
	buffer_start = gJpegenc.mem.buf_start;
	buffer_size = gJpegenc.mem.buf_size;
	lev = gJpegenc.mem.cur_buf_lev;
	max_w = gJpegenc.mem.bufspec->max_width;
	max_h = gJpegenc.mem.bufspec->max_height;
	width = gJpegenc.wq.cmd.encoder_width;
	height = gJpegenc.wq.cmd.encoder_height;
#ifdef CONFIG_CMA
	cma_size = gJpegenc.mem.cma_pool_size / SZ_1M;
#endif
	spin_unlock(&gJpegenc.sem_lock);

	jenc_pr(LOG_DEBUG,
		"jpegenc width: %d, encode height: %d.\n",
		width, height);
	jenc_pr(LOG_DEBUG,
		"jpegenc hw_status: %d, process_irq: %s.\n",
		hw_status, process_irq ? "true" : "false");
	jenc_pr(LOG_DEBUG,
		"jpegenc irq num: %d,  inited: %s, opened: %d\n",
		irq_num, inited ? "true" : "false", opened);
	if (use_reserve) {
		jenc_pr(LOG_DEBUG,
			"jpegenc reserve memory, buffer start: 0x%x, size: %d MB.\n",
			res.buf_start, res.buf_size / SZ_1M);
	} else {
#ifdef CONFIG_CMA
		jenc_pr(LOG_DEBUG, "jpegenc cma pool size: %d.\n", cma_size);
#endif
	}
	jenc_pr(LOG_DEBUG, "jpegenc buffer start: 0x%x, size: 0x%x\n",
		buffer_start, buffer_size);
	jenc_pr(LOG_DEBUG, "buffer level: %s\n", glevel_str[lev]);
	return snprintf(buf, 40, "max size: %dx%d\n", max_w, max_h);
}

static struct class_attribute jpegenc_class_attrs[] = {
	__ATTR(encode_status,
	S_IRUGO | S_IWUSR,
	jpegenc_status_show,
	NULL),
	__ATTR_NULL
};

static struct class jpegenc_class = {
	.name = CLASS_NAME,
	.class_attrs = jpegenc_class_attrs,
};

s32 init_jpegenc_device(void)
{
	s32 r = 0;
	r = register_chrdev(0, DEVICE_NAME, &jpegenc_fops);
	if (r <= 0) {
		jenc_pr(LOG_ERROR, "register jpegenc device error\n");
		return r;
	}
	jpegenc_device_major = r;

	r = class_register(&jpegenc_class);
	if (r < 0) {
		jenc_pr(LOG_ERROR, "error create jpegenc class.\n");
		return r;
	}

	jpegenc_dev = device_create(&jpegenc_class, NULL,
		MKDEV(jpegenc_device_major, 0), NULL,
		DEVICE_NAME);

	if (IS_ERR(jpegenc_dev)) {
		jenc_pr(LOG_ERROR, "create jpegenc device error.\n");
		class_unregister(&jpegenc_class);
		return -1;
	}
	return r;
}

s32 uninit_jpegenc_device(void)
{
	if (jpegenc_dev)
		device_destroy(&jpegenc_class, MKDEV(jpegenc_device_major, 0));

	class_destroy(&jpegenc_class);

	unregister_chrdev(jpegenc_device_major, DEVICE_NAME);
	return 0;
}

static s32 jpegenc_mem_device_init(struct reserved_mem *rmem,
		struct device *dev)
{
	s32 r;
	struct resource res;
	if (!rmem) {
		jenc_pr(LOG_ERROR,
			"Can't obtain I/O memory, will allocate jpegenc buffer!\n");
		r = -EFAULT;
		return r;
	}
	res.start = (phys_addr_t) rmem->base;
	res.end = res.start + (phys_addr_t) rmem->size - 1;
	gJpegenc.mem.reserve_mem.buf_start = res.start;
	gJpegenc.mem.reserve_mem.buf_size = res.end - res.start + 1;
	if (gJpegenc.mem.reserve_mem.buf_size >=
	    jpegenc_buffspec[JPEGENC_BUFFER_LEVEL_VGA].min_buffsize)
		gJpegenc.use_reserve = true;
	else {
		jenc_pr(LOG_ERROR,
			"jpegenc reserve_mem too small, size is %d.\n",
			gJpegenc.mem.reserve_mem.buf_size);
		gJpegenc.mem.reserve_mem.buf_start = 0;
		gJpegenc.mem.reserve_mem.buf_size = 0;
		return -EFAULT;
	}
	return r;
}

static s32 jpegenc_probe(struct platform_device *pdev)
{
	s32 res_irq;
	s32 idx;

	jenc_pr(LOG_DEBUG, "jpegenc probe start.\n");

	gJpegenc.this_pdev = pdev;
	gJpegenc.use_reserve = false;
	memset(&gJpegenc.mem, 0, sizeof(struct jpegenc_meminfo_s));

	idx = of_reserved_mem_device_init(&pdev->dev);
	if (idx != 0) {
		jenc_pr(LOG_DEBUG,
			"jpegenc memory resource undefined.\n");
	}

	if (gJpegenc.use_reserve == false) {
#ifndef CONFIG_CMA
		jenc_pr(LOG_ERROR,
			"jpegenc memory is invaild, probe fail!\n");
		return -EFAULT;
#else
		gJpegenc.mem.cma_pool_size =
			dma_get_cma_size_int_byte(&pdev->dev);
		jenc_pr(LOG_DEBUG,
			"jpegenc - cma memory pool size: %d MB\n",
			(u32)gJpegenc.mem.cma_pool_size / SZ_1M);
		gJpegenc.mem.buf_size = gJpegenc.mem.cma_pool_size;
#endif
	} else {
		gJpegenc.mem.buf_start = gJpegenc.mem.reserve_mem.buf_start;
		gJpegenc.mem.buf_size = gJpegenc.mem.reserve_mem.buf_size;
	}

	if (gJpegenc.mem.buf_size >=
		jpegenc_buffspec[JPEGENC_BUFFER_LEVEL_HD].min_buffsize) {
		gJpegenc.mem.cur_buf_lev = JPEGENC_BUFFER_LEVEL_HD;
		gJpegenc.mem.bufspec = (struct Jpegenc_BuffInfo_s *)
			&jpegenc_buffspec[JPEGENC_BUFFER_LEVEL_HD];
	} else if (gJpegenc.mem.buf_size >=
		jpegenc_buffspec[JPEGENC_BUFFER_LEVEL_13M].min_buffsize) {
		gJpegenc.mem.cur_buf_lev = JPEGENC_BUFFER_LEVEL_13M;
		gJpegenc.mem.bufspec = (struct Jpegenc_BuffInfo_s *)
			&jpegenc_buffspec[JPEGENC_BUFFER_LEVEL_13M];
	} else if (gJpegenc.mem.buf_size >=
		jpegenc_buffspec[JPEGENC_BUFFER_LEVEL_8M].min_buffsize) {
		gJpegenc.mem.cur_buf_lev = JPEGENC_BUFFER_LEVEL_8M;
		gJpegenc.mem.bufspec = (struct Jpegenc_BuffInfo_s *)
			&jpegenc_buffspec[JPEGENC_BUFFER_LEVEL_8M];
	} else if (gJpegenc.mem.buf_size >=
		jpegenc_buffspec[JPEGENC_BUFFER_LEVEL_5M].min_buffsize) {
		gJpegenc.mem.cur_buf_lev = JPEGENC_BUFFER_LEVEL_5M;
		gJpegenc.mem.bufspec = (struct Jpegenc_BuffInfo_s *)
			&jpegenc_buffspec[JPEGENC_BUFFER_LEVEL_5M];
	} else if (gJpegenc.mem.buf_size >=
		jpegenc_buffspec[JPEGENC_BUFFER_LEVEL_3M].min_buffsize) {
		gJpegenc.mem.cur_buf_lev = JPEGENC_BUFFER_LEVEL_3M;
		gJpegenc.mem.bufspec = (struct Jpegenc_BuffInfo_s *)
			&jpegenc_buffspec[JPEGENC_BUFFER_LEVEL_3M];
	} else if (gJpegenc.mem.buf_size >=
		jpegenc_buffspec[JPEGENC_BUFFER_LEVEL_2M].min_buffsize) {
		gJpegenc.mem.cur_buf_lev = JPEGENC_BUFFER_LEVEL_2M;
		gJpegenc.mem.bufspec = (struct Jpegenc_BuffInfo_s *)
			&jpegenc_buffspec[JPEGENC_BUFFER_LEVEL_2M];
	} else if (gJpegenc.mem.buf_size >=
		jpegenc_buffspec[JPEGENC_BUFFER_LEVEL_VGA].min_buffsize) {
		gJpegenc.mem.cur_buf_lev = JPEGENC_BUFFER_LEVEL_VGA;
		gJpegenc.mem.bufspec = (struct Jpegenc_BuffInfo_s *)
			&jpegenc_buffspec[JPEGENC_BUFFER_LEVEL_VGA];
	} else {
		jenc_pr(LOG_ERROR,
			"jpegenc probe memory too small, size is %d.\n",
			gJpegenc.mem.buf_size);
		gJpegenc.mem.buf_start = 0;
		gJpegenc.mem.buf_size = 0;
		return -EFAULT;
	}

	res_irq = platform_get_irq(pdev, 0);
	if (res_irq < 0) {
		jenc_pr(LOG_ERROR, "[%s] get irq error!", __func__);
		return -EINVAL;
	}

	gJpegenc.irq_num = res_irq;

	jenc_pr(LOG_DEBUG,
		"jpegenc memory config sucess, buff size is 0x%x, level: %s\n",
		gJpegenc.mem.buf_size,
		glevel_str[gJpegenc.mem.cur_buf_lev]);

	jpegenc_wq_init();
	init_jpegenc_device();
	jenc_pr(LOG_DEBUG, "jpegenc probe end.\n");
	return 0;
}

static s32 jpegenc_remove(struct platform_device *pdev)
{
	if (jpegenc_wq_uninit())
		jenc_pr(LOG_ERROR, "jpegenc_wq_uninit error.\n");
	uninit_jpegenc_device();
	jenc_pr(LOG_DEBUG, "jpegenc remove.\n");
	return 0;
}

static const struct of_device_id amlogic_jpegenc_dt_match[] = {
	{
		.compatible = "amlogic, jpegenc",
	},
	{},
};

static struct platform_driver jpegenc_driver = {
	.probe = jpegenc_probe,
	.remove = jpegenc_remove,
	.driver = {
		.name = DRIVER_NAME,
		.of_match_table = amlogic_jpegenc_dt_match,
	}
};

static struct codec_profile_t jpegenc_profile = {
	.name = "jpegenc",
	.profile = ""
};

static s32 __init jpegenc_driver_init_module(void)
{
	jenc_pr(LOG_DEBUG, "jpegenc module init\n");

	if (platform_driver_register(&jpegenc_driver)) {
		jenc_pr(LOG_ERROR, "failed to register jpegenc driver\n");
		return -ENODEV;
	}
	vcodec_profile_register(&jpegenc_profile);
	return 0;
}

static void __exit jpegenc_driver_remove_module(void)
{
	jenc_pr(LOG_DEBUG, "jpegenc module remove.\n");
	platform_driver_unregister(&jpegenc_driver);
}

static const struct reserved_mem_ops rmem_jpegenc_ops = {
	.device_init = jpegenc_mem_device_init,
};

static s32 __init jpegenc_mem_setup(struct reserved_mem *rmem)
{
	rmem->ops = &rmem_jpegenc_ops;
	jenc_pr(LOG_DEBUG, "jpegenc reserved mem setup.\n");
	return 0;
}

module_param(clock_level, uint, 0664);
MODULE_PARM_DESC(clock_level, "\n clock_level\n");

module_param(jpegenc_print_level, uint, 0664);
MODULE_PARM_DESC(jpegenc_print_level, "\n jpegenc_print_level\n");

module_init(jpegenc_driver_init_module);
module_exit(jpegenc_driver_remove_module);
RESERVEDMEM_OF_DECLARE(jpegenc, "amlogic, jpegenc-memory", jpegenc_mem_setup);

MODULE_DESCRIPTION("AMLOGIC JPEG Encoder Driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("simon.zheng <simon.zheng@amlogic.com>");
