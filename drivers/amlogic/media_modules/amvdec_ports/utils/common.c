/*
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Description:
 */
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/string.h>

#include "common.h"
#ifdef CONFIG_AMLOGIC_MEDIA_V4L_SOFTWARE_PARSER
#include "pixfmt.h"
#endif

const u8 ff_zigzag_direct[64] = {
	0,  1,  8, 16, 9, 2, 3, 10,
	17, 24, 32, 25, 18, 11, 4, 5,
	12, 19, 26, 33, 40, 48, 41, 34,
	27, 20, 13, 6, 7, 14, 21, 28,
	35, 42, 49, 56, 57, 50, 43, 36,
	29, 22, 15, 23, 30, 37, 44, 51,
	58, 59, 52, 45, 38, 31, 39, 46,
	53, 60, 61, 54, 47, 55, 62, 63
};

const u8 ff_zigzag_scan[16 + 1] = {
	0 + 0 * 4, 1 + 0 * 4, 0 + 1 * 4, 0 + 2 * 4,
	1 + 1 * 4, 2 + 0 * 4, 3 + 0 * 4, 2 + 1 * 4,
	1 + 2 * 4, 0 + 3 * 4, 1 + 3 * 4, 2 + 2 * 4,
	3 + 1 * 4, 3 + 2 * 4, 2 + 3 * 4, 3 + 3 * 4,
};

const char * const color_space_names[] = {
	[AVCOL_SPC_RGB]		= "gbr",
	[AVCOL_SPC_BT709]	= "bt709",
	[AVCOL_SPC_UNSPECIFIED]	= "unknown",
	[AVCOL_SPC_RESERVED]	= "reserved",
	[AVCOL_SPC_FCC]		= "fcc",
	[AVCOL_SPC_BT470BG]	= "bt470bg",
	[AVCOL_SPC_SMPTE170M]	= "smpte170m",
	[AVCOL_SPC_SMPTE240M]	= "smpte240m",
	[AVCOL_SPC_YCGCO]	= "ycgco",
	[AVCOL_SPC_BT2020_NCL]	= "bt2020nc",
	[AVCOL_SPC_BT2020_CL]	= "bt2020c",
	[AVCOL_SPC_SMPTE2085]	= "smpte2085",
	[AVCOL_SPC_CHROMA_DERIVED_NCL] = "chroma-derived-nc",
	[AVCOL_SPC_CHROMA_DERIVED_CL] = "chroma-derived-c",
	[AVCOL_SPC_ICTCP]	= "ictcp",
};

const char *av_color_space_name(enum AVColorSpace space)
{
	return (unsigned) space < AVCOL_SPC_NB ?
		color_space_names[space] : NULL;
}

const char * const color_primaries_names[AVCOL_PRI_NB] = {
	[AVCOL_PRI_RESERVED0]	= "reserved",
	[AVCOL_PRI_BT709]	= "bt709",
	[AVCOL_PRI_UNSPECIFIED]	= "unknown",
	[AVCOL_PRI_RESERVED]	= "reserved",
	[AVCOL_PRI_BT470M]	= "bt470m",
	[AVCOL_PRI_BT470BG]	= "bt470bg",
	[AVCOL_PRI_SMPTE170M]	= "smpte170m",
	[AVCOL_PRI_SMPTE240M]	= "smpte240m",
	[AVCOL_PRI_FILM]	= "film",
	[AVCOL_PRI_BT2020]	= "bt2020",
	[AVCOL_PRI_SMPTE428]	= "smpte428",
	[AVCOL_PRI_SMPTE431]	= "smpte431",
	[AVCOL_PRI_SMPTE432]	= "smpte432",
	[AVCOL_PRI_JEDEC_P22]	= "jedec-p22",
};

const char *av_color_primaries_name(enum AVColorPrimaries primaries)
{
	return (unsigned) primaries < AVCOL_PRI_NB ?
		color_primaries_names[primaries] : NULL;
}

const char * const color_transfer_names[] = {
    [AVCOL_TRC_RESERVED0]	= "reserved",
    [AVCOL_TRC_BT709]		= "bt709",
    [AVCOL_TRC_UNSPECIFIED]	= "unknown",
    [AVCOL_TRC_RESERVED]	= "reserved",
    [AVCOL_TRC_GAMMA22]		= "bt470m",
    [AVCOL_TRC_GAMMA28]		= "bt470bg",
    [AVCOL_TRC_SMPTE170M]	= "smpte170m",
    [AVCOL_TRC_SMPTE240M]	= "smpte240m",
    [AVCOL_TRC_LINEAR]		= "linear",
    [AVCOL_TRC_LOG]		= "log100",
    [AVCOL_TRC_LOG_SQRT]	= "log316",
    [AVCOL_TRC_IEC61966_2_4]	= "iec61966-2-4",
    [AVCOL_TRC_BT1361_ECG]	= "bt1361e",
    [AVCOL_TRC_IEC61966_2_1]	= "iec61966-2-1",
    [AVCOL_TRC_BT2020_10]	= "bt2020-10",
    [AVCOL_TRC_BT2020_12]	= "bt2020-12",
    [AVCOL_TRC_SMPTE2084]	= "smpte2084",
    [AVCOL_TRC_SMPTE428]	= "smpte428",
    [AVCOL_TRC_ARIB_STD_B67]	= "arib-std-b67",
};

const char *av_color_transfer_name(enum AVColorTransferCharacteristic transfer)
{
	return (unsigned) transfer < AVCOL_TRC_NB ?
		color_transfer_names[transfer] : NULL;
}

//math
const u8 ff_log2_tab[256]={
	0,0,1,1,2,2,2,2,3,3,3,3,3,3,3,3,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
	5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
	6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
	6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7
};

int av_log2(u32 v)
{
	int n = 0;

	if (v & 0xffff0000) {
		v >>= 16;
		n += 16;
	}
	if (v & 0xff00) {
		v >>= 8;
		n += 8;
	}
		n += ff_log2_tab[v];

	return n;
}

//bitstream
int find_start_code(u8 *data, int data_sz)
{
	if (data_sz > 3 && data[0] == 0 && data[1] == 0 && data[2] == 1)
		return 3;

	if (data_sz > 4 && data[0] == 0 && data[1] == 0 && data[2] == 0 && data[3] == 1)
		return 4;

	return -1;
}

int calc_nal_len(u8 *data, int len)
{
	int i;

	for (i = 0; i < len - 4; i++) {
		if (data[i])
			continue;

		if ((data[i] == 0 && data[i + 1] == 0 && data[i + 2] == 1) ||
			(data[i] == 0 && data[i + 1] == 0 &&
			data[i + 2]==0 && data[i + 3] == 1))
		 return i;
	}
	return len; //Not find the end of nalu
}

u8 *nal_unit_extract_rbsp(const u8 *src, u32 src_len, u32 *dst_len)
{
	u8 *dst;
	u32 i, len;

	dst = vmalloc(src_len + AV_INPUT_BUFFER_PADDING_SIZE);
	if (!dst)
		return NULL;

	/* NAL unit header (2 bytes) */
	i = len = 0;
	while (i < 2 && i < src_len)
		dst[len++] = src[i++];

	while (i + 2 < src_len)
	if (!src[i] && !src[i + 1] && src[i + 2] == 3) {
		dst[len++] = src[i++];
		dst[len++] = src[i++];
		i++; // remove emulation_prevention_three_byte
	} else
		dst[len++] = src[i++];

	while (i < src_len)
		dst[len++] = src[i++];

	memset(dst + len, 0, AV_INPUT_BUFFER_PADDING_SIZE);

	*dst_len = len;

	return dst;
}

//debug
static void _pr_hex(const char *fmt, ...)
{
	u8 buf[512];
	int len = 0;

	va_list args;
	va_start(args, fmt);
	vsnprintf(buf + len, 512 - len, fmt, args);
	printk("%s", buf);
	va_end(args);
}

void print_hex_debug(u8 *data, u32 len, int max)
{
	int i, l;

	l = len > max ? max : len;

	for (i = 0; i < l; i++) {
		if ((i & 0xf) == 0)
			_pr_hex("%06x:", i);
		_pr_hex("%02x ", data[i]);
		if ((((i + 1) & 0xf) == 0) || ((i + 1) == l))
			_pr_hex("\n");
	}

	_pr_hex("print hex ending. len %d\n\n", l);
}

bool is_over_size(int w, int h, int size)
{
	if (h != 0 && (w > size / h))
		return true;

	return false;
}

u8 *aml_yuv_dump(struct file *fp, u8 *start_addr, u32 real_width, u32 real_height, u32 align)
{
	u32 index;
	u32 coded_width = ALIGN(real_width, align);
	u32 coded_height = ALIGN(real_height, align);
	u8 *yuv_data_addr = start_addr;

	if (real_width != coded_width) {
		for (index = 0; index < real_height; index++) {
			kernel_write(fp, yuv_data_addr, real_width, 0);
			yuv_data_addr += coded_width;
		}
	} else {
		kernel_write(fp, yuv_data_addr, real_width * real_height, 0);
	}

	return (start_addr + coded_width * coded_height);
}
