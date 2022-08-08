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
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/amlogic/media/utils/amstream.h>
#include <linux/amlogic/media/utils/vformat.h>
#include "../../../common/chips/decoder_cpu_ver_info.h"
#include "vdec.h"



//static const struct codec_profile_t *vcodec_feature[SUPPORT_VDEC_NUM] = { 0 };

struct vcodec_feature {
	int format;
	int is_v4l;
};

static struct vcodec_feature feature[SUPPORT_VDEC_NUM];

static int vcodec_feature_idx;
static ulong last_time;
u8 buf[4096];


static const char * const format_name[] = {
	"ammvdec_mpeg12",
	"ammvdec_mpeg4",
	"ammvdec_h264",
	"ammvdec_mjpeg",
	"ammvdec_real",
	"ammjpegdec",
	"ammvdec_vc1",
	"ammvdec_avs",
	"ammvdec_yuv",
	"ammvdec_h264mvc",
	"ammvdec_h264_4k2k",
	"ammvdec_h265",
	"amvenc_avc",
	"jpegenc",
	"ammvdec_vp9",
	"ammvdec_avs2",
	"ammvdec_av1",
};

static int vcodec_feature_CC(u8 *buf, int size, int vformat, int is_v4l)
{
	u8 *pbuf = buf;

	if (!is_v4l) {
		switch (vformat) {
			case VFORMAT_MPEG12:
			case VFORMAT_H264:
			case VFORMAT_AVS:
				pbuf += snprintf(pbuf, size, "        \"CC subtitle\" : \"true\"\n");
				break;
			default:
				break;
		}
	}

	return pbuf - buf;
}

static int vcodec_feature_report_information(u8 *buf, int size, int vformat, int is_v4l)
{
	u8 *pbuf = buf;

	if (!is_v4l) {
		pbuf += snprintf(pbuf, size, "        \"Decoder information report\" : \"true\"\n");
	}

	return pbuf - buf;
}

static int vcodec_feature_i_only_mode(u8 *buf, int size, int vformat)
{
	u8 *pbuf = buf;

	switch (vformat) {
		case VFORMAT_MPEG12:
		case VFORMAT_H264:
		case VFORMAT_MPEG4:
		case VFORMAT_HEVC:
		case VFORMAT_AVS2:
			pbuf += snprintf(pbuf, size, "        \"I only mode\" : \"true\"\n");
			break;
		default:
			break;
	}

	return pbuf - buf;
}

static int vcodec_feature_dolbyVison(u8 *buf, int size, int vformat)
{
	u8 *pbuf = buf;

	switch (vformat) {
		case VFORMAT_H264:
		case VFORMAT_HEVC:
		case VFORMAT_AV1:
			if ((get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_GXM) &&
				(get_cpu_major_id() != AM_MESON_CPU_MAJOR_ID_T5) &&
				(get_cpu_major_id() != AM_MESON_CPU_MAJOR_ID_T5D) &&
				(get_cpu_major_id() != AM_MESON_CPU_MAJOR_ID_S4)) {
				pbuf += snprintf(pbuf, size, "        \"DolbyVision\" : \"true\"\n");
				pbuf += snprintf(pbuf, size, "        \"multi_frame_dv\" : \"true\"\n");
			}
			break;
		default:
			break;
	}

	return pbuf - buf;
}

static int vcodec_feature_HDR(u8 *buf, int size, int vformat)
{
	u8 *pbuf = buf;

	switch (vformat) {
		case VFORMAT_H264:
		case VFORMAT_HEVC:
		case VFORMAT_AV1:
		case VFORMAT_AVS2:
		case VFORMAT_VP9:
				pbuf += snprintf(pbuf, size, "        \"HDR\" : \"true\"\n");
			break;
		default:
			break;
	}

	return pbuf - buf;
}


static int vcodec_feature_doublewrite(u8 *buf, int size, int vformat)
{
	u8 *pbuf = buf;
	int tsize = 0;
	int s;

	switch (vformat) {
		case VFORMAT_HEVC:
		case VFORMAT_VP9:
		case VFORMAT_AVS2:
		case VFORMAT_AV1:
			s = snprintf(pbuf, size - tsize, "        \"DoubleWrite\" ");
			tsize += s;
			pbuf += s;
			if ((get_cpu_major_id() != AM_MESON_CPU_MAJOR_ID_T7) &&
				(get_cpu_major_id() != AM_MESON_CPU_MAJOR_ID_T3)) {
				s = snprintf(pbuf, size - tsize, "[ \"0\", \"1\", \"2\", \"3\", \"4\", \"0x10\", \"0x10000\", \"0x20000\"]\n");
				tsize += s;
				pbuf += s;
			}
			else {
				s = snprintf(pbuf, size - tsize, "[ \"0\", \"1\", \"2\", \"3\", \"4\", \"8\", \"0x10\", \"0x10000\", \"0x20000\"]\n");
				tsize += s;
				pbuf += s;
			}
			break;
		default:
			break;
	}

	return pbuf - buf;
}

static int vcodec_feature_vdec_fence(u8 *buf, int size, int vformat)
{
	u8 *pbuf = buf;

	switch (vformat) {
		case VFORMAT_H264:
		case VFORMAT_HEVC:
		case VFORMAT_VP9:
			if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_GXM)
				pbuf += snprintf(pbuf, size, "        \"GameMode\" : \"true\"\n");
			break;
		default:
			break;
	}

	return pbuf - buf;
}

static int vcodec_feature_bitdepth(u8 *buf, int size, int vformat)
{
	u8 *pbuf = buf;

	switch (vformat) {
		case VFORMAT_HEVC:
		case VFORMAT_VP9:
		case VFORMAT_AVS2:
		case VFORMAT_AV1:
			if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_GXBB)
				pbuf += snprintf(pbuf, size, "        \"BitDepth\" : \"10\"\n");
			else
				pbuf += snprintf(pbuf, size, "        \"BitDepth\" : \"8\"\n");
			break;
		default:
			break;
	}

	return pbuf - buf;
}


static int vcodec_feature_MaxResolution(u8 *buf, int size, int vformat)
{
	u8 *pbuf = buf;

	switch (vformat) {
		case VFORMAT_HEVC:
		case VFORMAT_VP9:
		case VFORMAT_AVS2:
		case VFORMAT_AV1:
			if ((get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_SM1) &&
				(get_cpu_major_id() != AM_MESON_CPU_MAJOR_ID_T5D))
				pbuf += snprintf(pbuf, size, "        \"MaximumResolution\" : \"8k\"\n");
			else if (vdec_is_support_4k() &&
					(get_cpu_major_id() != AM_MESON_CPU_MAJOR_ID_T5D))
				pbuf += snprintf(pbuf, size, "        \"MaximumResolution\" : \"4k60\"\n");
			else
				pbuf += snprintf(pbuf, size, "        \"MaximumResolution\" : \"1080p60\"\n");
			break;
		case VFORMAT_H264:
			if (vdec_is_support_4k())
				pbuf += snprintf(pbuf, size, "        \"MaximumResolution\" : \"4k30\"\n");
			else
				pbuf += snprintf(pbuf, size, "        \"MaximumResolution\" : \"1080p60\"\n");
			break;
		case VFORMAT_MPEG12:
		case VFORMAT_MPEG4:
		case VFORMAT_MJPEG:
		case VFORMAT_VC1:
		case VFORMAT_AVS:
			pbuf += snprintf(pbuf, size, "        \"MaximumResolution\" : \"1080p60\"\n");
			break;
		default:
			break;
	}

	return pbuf - buf;
}

static int vcodec_feature_clock(u8 *buf, int size, int vformat)
{
	u8 *pbuf = buf;

	switch (vformat) {
		case VFORMAT_HEVC:
		case VFORMAT_VP9:
		case VFORMAT_AVS2:
		case VFORMAT_AV1:
			if ((get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_G12B) &&
				(get_cpu_major_id() != AM_MESON_CPU_MAJOR_ID_GXLX2) &&
				(get_cpu_major_id() != AM_MESON_CPU_MAJOR_ID_T5) &&
				(get_cpu_major_id() != AM_MESON_CPU_MAJOR_ID_T5D))
				pbuf += snprintf(pbuf, size, "        \"ClockFrequency\" : \"800MHZ\"\n");
			else
				pbuf += snprintf(pbuf, size, "        \"ClockFrequency\" : \"667MHZ\"\n");
			break;
		case VFORMAT_H264:
		case VFORMAT_MPEG12:
		case VFORMAT_MPEG4:
		case VFORMAT_MJPEG:
		case VFORMAT_VC1:
		case VFORMAT_AVS:
			if ((get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_SM1) &&
				(get_cpu_major_id() != AM_MESON_CPU_MAJOR_ID_TL1) &&
				(get_cpu_major_id() != AM_MESON_CPU_MAJOR_ID_GXLX2) &&
				(get_cpu_major_id() != AM_MESON_CPU_MAJOR_ID_T5) &&
				(get_cpu_major_id() != AM_MESON_CPU_MAJOR_ID_T5D))
				pbuf += snprintf(pbuf, size, "        \"ClockFrequency\" : \"800MHZ\"\n");
			else
				pbuf += snprintf(pbuf, size, "        \"ClockFrequency\" : \"667MHZ\"\n");
			break;
		default:
			break;
	}

	return pbuf - buf;
}

static int vcodec_feature_support_format(int vformat)
{

	switch (vformat) {
		case VFORMAT_VP9:
			if ((get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_GXL) &&
				(get_cpu_major_id() != AM_MESON_CPU_MAJOR_ID_T5))
				return 1;
			else
				return 0;
		case VFORMAT_AVS2:
			if ((get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_G12A) &&
				(get_cpu_major_id() != AM_MESON_CPU_MAJOR_ID_T5D))
				return 1;
			else
				return 0;
		case VFORMAT_AV1:
			if (((get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_TM2) &&
					is_cpu_tm2_revb()) ||
				((get_cpu_major_id() > AM_MESON_CPU_MAJOR_ID_TM2) &&
				(get_cpu_major_id() != AM_MESON_CPU_MAJOR_ID_T5)))
				return 1;
			else
				return 0;
		case VFORMAT_AVS:
			if ((get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_GXM))
				return 1;
			else
				return 0;
		case VFORMAT_MJPEG:
		case VFORMAT_VC1:
		case VFORMAT_MPEG4:
		case VFORMAT_MPEG12:
		case VFORMAT_H264:
		case VFORMAT_HEVC:
			return 1;
		default:
			break;
	}

	return 0;
}

static int vcodec_feature_FCC(u8 *buf, int size, int vformat, int is_v4l)
{
	u8 *pbuf = buf;

	if (!is_v4l) {
		switch (vformat) {
			case VFORMAT_HEVC:
			case VFORMAT_H264:
			case VFORMAT_MPEG12:
				pbuf += snprintf(pbuf, size, "        \"Decoder FCC support\" : \"true\"\n");
				break;
			default:
				break;
		}
	}

	return pbuf - buf;
}

static int vcodec_feature_RDMA(u8 *buf, int size, int vformat)
{
	u8 *pbuf = buf;

	if (get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_T3)
		pbuf += snprintf(pbuf, size, "        \"Decoder RDMA support\" : \"true\"\n");

	return pbuf - buf;
}

static int vcodec_feature_v4ldec_nr(u8 *buf, int size, int vformat, int is_v4l)
{
	u8 *pbuf = buf;

	if (is_v4l) {
		pbuf += snprintf(pbuf, size, "        \"V4ldec nr\" : \"true\"\n");
	}

	return pbuf - buf;
}

static int vcodec_feature_ge2d_wrapper(u8 *buf, int size, int vformat, int is_v4l)
{
	u8 *pbuf = buf;

	if (is_v4l && (get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_T7)) {
		pbuf += snprintf(pbuf, size, "        \"Ge2d wrapper\" : \"true\"\n");
	}

	return pbuf - buf;
}



int vcodec_feature_get_feature(u8 *buf, int vformat, int is_v4l)
{
	u8 *pbuf = buf;
	int size = PAGE_SIZE;
	int tsize = 0;
	int s;

	s = snprintf(pbuf, size - tsize, "    \"%s%s\": ", format_name[vformat], is_v4l ? "_v4l" : "");
	tsize += s;
	pbuf += s;

	s = snprintf(pbuf, size - tsize, "{\n");
	tsize += s;
	pbuf += s;

	s = vcodec_feature_CC(pbuf, size - tsize, vformat, is_v4l);
	tsize += s;
	pbuf += s;

	s = vcodec_feature_report_information(pbuf, size - tsize, vformat, is_v4l);
	tsize += s;
	pbuf += s;

	s = vcodec_feature_vdec_fence(pbuf, size - tsize, vformat);
	tsize += s;
	pbuf += s;

	s = vcodec_feature_i_only_mode(pbuf, size - tsize, vformat);
	tsize += s;
	pbuf += s;

	s = vcodec_feature_dolbyVison(pbuf, size - tsize, vformat);
	tsize += s;
	pbuf += s;

	s = vcodec_feature_HDR(pbuf, size - tsize, vformat);
	tsize += s;
	pbuf += s;

	s = vcodec_feature_doublewrite(pbuf, size - tsize, vformat);
	tsize += s;
	pbuf += s;

	s = vcodec_feature_bitdepth(pbuf, size - tsize, vformat);
	tsize += s;
	pbuf += s;

	s = vcodec_feature_MaxResolution(pbuf, size - tsize, vformat);
	tsize += s;
	pbuf += s;

	s = vcodec_feature_clock(pbuf, size - tsize, vformat);
	tsize += s;
	pbuf += s;

	s = vcodec_feature_FCC(pbuf, size - tsize, vformat, is_v4l);
	tsize += s;
	pbuf += s;

	s = vcodec_feature_RDMA(pbuf, size - tsize, vformat);
	tsize += s;
	pbuf += s;

	s = vcodec_feature_v4ldec_nr(pbuf, size - tsize, vformat, is_v4l);
	tsize += s;
	pbuf += s;

	s = vcodec_feature_ge2d_wrapper(pbuf, size - tsize, vformat, is_v4l);
	tsize += s;
	pbuf += s;

	s = snprintf(pbuf, size - tsize, "        \"UcodeVersionRequest\" : \"0.3.10\"\n");
	tsize += s;
	pbuf += s;

	s = snprintf(pbuf, size - tsize, "    }\n");
	tsize += s;
	pbuf += s;

	return pbuf - buf;
}


ssize_t vcodec_feature_read(char *buf)
{
	static int read_count;
	char *pbuf = buf;

	if (jiffies - last_time > 5 * HZ) {
		read_count = 0;
		/*timeout :not continue dump,dump from first. */
	}

	if (vcodec_feature_idx > 0) {
		if (read_count == 0)
			pbuf += snprintf(pbuf, PAGE_SIZE - (pbuf - buf), "{\n");
		pbuf += vcodec_feature_get_feature(pbuf, feature[read_count].format, feature[read_count].is_v4l);
		read_count++;
		if (read_count >= vcodec_feature_idx) {
			read_count = 0;
			pbuf += snprintf(pbuf, PAGE_SIZE - (pbuf - buf), "}");
		}
	}
	last_time = jiffies;
	return pbuf - buf;
}
EXPORT_SYMBOL(vcodec_feature_read);


int vcodec_feature_register(int vformat, int is_v4l)
{
	if ((vcodec_feature_idx < SUPPORT_VDEC_NUM) && vcodec_feature_support_format(vformat)) {
		feature[vcodec_feature_idx].format = vformat;
		feature[vcodec_feature_idx].is_v4l = is_v4l;
		vcodec_feature_idx++;
		//pr_debug("regist %s codec profile\n", vdec_profile->name);
	}

	return 0;
}
EXPORT_SYMBOL(vcodec_feature_register);



