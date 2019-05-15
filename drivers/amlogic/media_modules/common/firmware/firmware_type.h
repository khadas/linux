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
* FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
* more details.
*
* You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*
* Description:
*/
#ifndef __VIDEO_FIRMWARE_FORMAT_
#define __VIDEO_FIRMWARE_FORMAT_

#include <linux/slab.h>

/* example: #define VIDEO_DEC_AV1  TAG('A', 'V', '1', '-')*/
#define TAG(a, b, c, d)\
	((a << 24) | (b << 16) | (c << 8) | d)

/* fws define */
#define VIDEO_DEC_MPEG12		(0)
#define VIDEO_DEC_MPEG4_3		(1)
#define VIDEO_DEC_MPEG4_4		(2)
#define VIDEO_DEC_MPEG4_5		(3)
#define VIDEO_DEC_H263			(4)
#define VIDEO_DEC_MJPEG			(5)
#define VIDEO_DEC_MJPEG_MULTI		(6)
#define VIDEO_DEC_REAL_V8		(7)
#define VIDEO_DEC_REAL_V9		(8)
#define VIDEO_DEC_VC1			(9)
#define VIDEO_DEC_AVS			(10)
#define VIDEO_DEC_H264			(11)
#define VIDEO_DEC_H264_4k2K		(12)
#define VIDEO_DEC_H264_4k2K_SINGLE	(13)
#define VIDEO_DEC_H264_MVC		(14)
#define VIDEO_DEC_H264_MULTI		(15)
#define VIDEO_DEC_HEVC			(16)
#define VIDEO_DEC_HEVC_MMU		(17)
#define VIDEO_DEC_VP9			(18)
#define VIDEO_DEC_VP9_MMU		(19)
#define VIDEO_ENC_H264			(20)
#define VIDEO_ENC_JPEG			(21)
#define VIDEO_DEC_H264_MULTI_MMU	(23)
#define VIDEO_DEC_HEVC_G12A		(24)
#define VIDEO_DEC_VP9_G12A		(25)
#define VIDEO_DEC_AVS2			(26)
#define VIDEO_DEC_AVS2_MMU		(27)
#define VIDEO_DEC_AVS_GXM		(28)
#define VIDEO_DEC_AVS_NOCABAC		(29)
#define VIDEO_DEC_H264_MULTI_GXM	(30)
#define VIDEO_DEC_H264_MVC_GXM		(31)
#define VIDEO_DEC_VC1_G12A		(32)
#define VIDEO_DEC_MPEG12_MULTI		TAG('M', '1', '2', 'M')
#define VIDEO_DEC_MPEG4_4_MULTI		TAG('M', '4', '4', 'M')
#define VIDEO_DEC_MPEG4_5_MULTI		TAG('M', '4', '5', 'M')
#define VIDEO_DEC_H263_MULTI		TAG('2', '6', '3', 'M')
#define VIDEO_DEC_HEVC_MMU_SWAP		TAG('2', '6', '5', 'S')
/* ... */
#define FIRMWARE_MAX			(UINT_MAX)

#define VIDEO_PACKAGE			(0)
#define VIDEO_FW_FILE			(1)

#define VIDEO_DECODE			(0)
#define VIDEO_ENCODE			(1)
#define VIDEO_MISC			(2)

#define OPTEE_VDEC_LEGENCY		(0)
#define OPTEE_VDEC			(1)
#define OPTEE_VDEC_HEVC			(2)

struct format_name_s {
	unsigned int format;
	const char *name;
};

struct cpu_type_s {
	int type;
	const char *name;
};

const char *get_fw_format_name(unsigned int format);
unsigned int get_fw_format(const char *name);
int fw_get_cpu(const char *name);

#endif
