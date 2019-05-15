/*
 * drivers/amlogic/media/common/arch/chips/chips.c
 *
 * Copyright (C) 2016 Amlogic, Inc. All rights reserved.
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
#include <linux/module.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>

#include <linux/amlogic/media/utils/vformat.h>
#include <linux/amlogic/cpu_version.h>
#include "../../stream_input/amports/amports_priv.h"
#include "../../frame_provider/decoder/utils/vdec.h"
#include "chips.h"
#include <linux/amlogic/media/utils/log.h>
#include <linux/amlogic/media/utils/vdec_reg.h>
#include "decoder_cpu_ver_info.h"

#define VIDEO_FIRMWARE_FATHER_NAME "video"

/*
 *#define MESON_CPU_MAJOR_ID_M6		0x16
 *#define MESON_CPU_MAJOR_ID_M6TV		0x17
 *#define MESON_CPU_MAJOR_ID_M6TVL	0x18
 *#define MESON_CPU_MAJOR_ID_M8		0x19
 *#define MESON_CPU_MAJOR_ID_MTVD		0x1A
 *#define MESON_CPU_MAJOR_ID_M8B		0x1B
 *#define MESON_CPU_MAJOR_ID_MG9TV	0x1C
 *#define MESON_CPU_MAJOR_ID_M8M2		0x1D
 *#define MESON_CPU_MAJOR_ID_GXBB		0x1F
 *#define MESON_CPU_MAJOR_ID_GXTVBB		0x20
 *#define MESON_CPU_MAJOR_ID_GXL		0x21
 *#define MESON_CPU_MAJOR_ID_GXM		0x22
 *#define MESON_CPU_MAJOR_ID_TXL		0x23
 */
struct type_name {

	int type;

	const char *name;
};
static const struct type_name cpu_type_name[] = {
	{AM_MESON_CPU_MAJOR_ID_M6, "m6"},
	{AM_MESON_CPU_MAJOR_ID_M6TV, "m6tv"},
	{AM_MESON_CPU_MAJOR_ID_M6TVL, "m6tvl"},
	{AM_MESON_CPU_MAJOR_ID_M8, "m8"},
	{AM_MESON_CPU_MAJOR_ID_MTVD, "mtvd"},
	{AM_MESON_CPU_MAJOR_ID_M8B, "m8b"},
	{AM_MESON_CPU_MAJOR_ID_MG9TV, "mg9tv"},
	{AM_MESON_CPU_MAJOR_ID_M8M2, "m8"},
	{AM_MESON_CPU_MAJOR_ID_GXBB, "gxbb"},
	{AM_MESON_CPU_MAJOR_ID_GXTVBB, "gxtvbb"},
	{AM_MESON_CPU_MAJOR_ID_GXL, "gxl"},
	{AM_MESON_CPU_MAJOR_ID_GXM, "gxm"},
	{AM_MESON_CPU_MAJOR_ID_TXL, "txl"},
	{AM_MESON_CPU_MAJOR_ID_TXLX, "txlx"},
	{AM_MESON_CPU_MAJOR_ID_G12A, "g12a"},
	{AM_MESON_CPU_MAJOR_ID_G12B, "g12b"},
	{AM_MESON_CPU_MAJOR_ID_SM1, "sm1"},
	{AM_MESON_CPU_MAJOR_ID_TL1, "tl1"},
	{0, NULL},
};

static const char *get_type_name(const struct type_name *typename, int size,
	int type)
{

	const char *name = "unknown";

	int i;

	for (i = 0; i < size; i++) {

		if (type == typename[i].type)

			name = typename[i].name;

	}

	return name;
}

const char *get_cpu_type_name(void)
{

	return get_type_name(cpu_type_name,
		sizeof(cpu_type_name) / sizeof(struct type_name),
		get_cpu_major_id());
}
EXPORT_SYMBOL(get_cpu_type_name);

/*
 *enum vformat_e {
 *	VFORMAT_MPEG12 = 0,
 *	VFORMAT_MPEG4,
 *	VFORMAT_H264,
 *	VFORMAT_MJPEG,
 *	VFORMAT_REAL,
 *	VFORMAT_JPEG,
 *	VFORMAT_VC1,
 *	VFORMAT_AVS,
 *	VFORMAT_YUV,
 *	VFORMAT_H264MVC,
 *	VFORMAT_H264_4K2K,
 *	VFORMAT_HEVC,
 *	VFORMAT_H264_ENC,
 *	VFORMAT_JPEG_ENC,
 *	VFORMAT_VP9,
*	VFORMAT_AVS2,
 *	VFORMAT_MAX
 *};
 */
static const struct type_name vformat_type_name[] = {
	{VFORMAT_MPEG12, "mpeg12"},
	{VFORMAT_MPEG4, "mpeg4"},
	{VFORMAT_H264, "h264"},
	{VFORMAT_MJPEG, "mjpeg"},
	{VFORMAT_REAL, "real"},
	{VFORMAT_JPEG, "jpeg"},
	{VFORMAT_VC1, "vc1"},
	{VFORMAT_AVS, "avs"},
	{VFORMAT_YUV, "yuv"},
	{VFORMAT_H264MVC, "h264mvc"},
	{VFORMAT_H264_4K2K, "h264_4k"},
	{VFORMAT_HEVC, "hevc"},
	{VFORMAT_H264_ENC, "h264_enc"},
	{VFORMAT_JPEG_ENC, "jpeg_enc"},
	{VFORMAT_VP9, "vp9"},
	{VFORMAT_AVS2, "avs2"},
	{VFORMAT_YUV, "yuv"},
	{0, NULL},
};

const char *get_video_format_name(enum vformat_e type)
{

	return get_type_name(vformat_type_name,
		sizeof(vformat_type_name) / sizeof(struct type_name), type);
}
EXPORT_SYMBOL(get_video_format_name);

static struct chip_vdec_info_s current_chip_info;

struct chip_vdec_info_s *get_current_vdec_chip(void)
{

	return &current_chip_info;
}
EXPORT_SYMBOL(get_current_vdec_chip);

bool check_efuse_chip(int vformat)
{
	unsigned int status, i = 0;
	int type[] = {15, 14, 11, 2}; /* avs2, vp9, h265, h264 */

	status =  (READ_EFUSE_REG(EFUSE_LIC2) >> 8 & 0xf);
	if (!status)
		return false;

	do {
		if ((status & 1) && (type[i] == vformat))
			return true;
		i++;
	} while (status >>= 1);

	return false;
}
EXPORT_SYMBOL(check_efuse_chip);

