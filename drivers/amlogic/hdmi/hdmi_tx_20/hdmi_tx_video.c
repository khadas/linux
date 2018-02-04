/*
 * drivers/amlogic/hdmi/hdmi_tx_20/hdmi_tx_video.c
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

#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/major.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/cdev.h>

#include <linux/amlogic/hdmi_tx/hdmi_info_global.h>
#include <linux/amlogic/hdmi_tx/hdmi_tx_module.h>
#include <linux/amlogic/hdmi_tx/hdmi_tx_compliance.h>

unsigned char hdmi_output_rgb;
static void hdmitx_set_spd_info(struct hdmitx_dev *hdev);
static void hdmi_set_vend_spec_infofram(struct hdmitx_dev *hdev,
	enum hdmi_vic VideoCode);

static struct hdmitx_vidpara hdmi_tx_video_params[] = {
	{
		.VIC		= HDMI_640x480p60,
		.color_prefer   = COLORSPACE_RGB444,
		.color_depth	= COLORDEPTH_24B,
		.bar_info	= B_BAR_VERT_HORIZ,
		.repeat_time	= NO_REPEAT,
		.aspect_ratio   = TV_ASPECT_RATIO_4_3,
		.cc		= CC_ITU601,
		.ss		= SS_SCAN_UNDER,
		.sc		= SC_SCALE_HORIZ_VERT,
	},
	{
		.VIC		= HDMI_480p60,
		.color_prefer   = COLORSPACE_RGB444,
		.color_depth	= COLORDEPTH_24B,
		.bar_info	= B_BAR_VERT_HORIZ,
		.repeat_time	= NO_REPEAT,
		.aspect_ratio   = TV_ASPECT_RATIO_4_3,
		.cc		= CC_ITU601,
		.ss		= SS_SCAN_UNDER,
		.sc		= SC_SCALE_HORIZ_VERT,
	},
	{
		.VIC		= HDMI_480p60_16x9,
		.color_prefer   = COLORSPACE_RGB444,
		.color_depth	= COLORDEPTH_24B,
		.bar_info	= B_BAR_VERT_HORIZ,
		.repeat_time	= NO_REPEAT,
		.aspect_ratio   = TV_ASPECT_RATIO_16_9,
		.cc		= CC_ITU601,
		.ss		= SS_SCAN_UNDER,
		.sc		= SC_SCALE_HORIZ_VERT,
	},
	{
		.VIC		= HDMI_480p60_16x9_rpt,
		.color_prefer   = COLORSPACE_RGB444,
		.color_depth	= COLORDEPTH_24B,
		.bar_info	= B_BAR_VERT_HORIZ,
		.repeat_time	= HDMI_4_TIMES_REPEAT,
		.aspect_ratio   = TV_ASPECT_RATIO_16_9,
		.cc		= CC_ITU601,
		.ss		= SS_SCAN_UNDER,
		.sc		= SC_SCALE_HORIZ_VERT,
	},
	{
		.VIC		= HDMI_720p60,
		.color_prefer   = COLORSPACE_RGB444,
		.color_depth	= COLORDEPTH_24B,
		.bar_info	= B_BAR_VERT_HORIZ,
#ifdef DOUBLE_CLK_720P_1080I
		.repeat_time	= HDMI_2_TIMES_REPEAT,
#else
		.repeat_time	= NO_REPEAT,
#endif
		.aspect_ratio   = TV_ASPECT_RATIO_16_9,
		.cc		= CC_ITU709,
		.ss		= SS_SCAN_UNDER,
		.sc		= SC_SCALE_HORIZ_VERT,
	},
	{
		.VIC		= HDMI_1080i60,
		.color_prefer   = COLORSPACE_RGB444,
		.color_depth	= COLORDEPTH_24B,
		.bar_info	= B_BAR_VERT_HORIZ,
#ifdef DOUBLE_CLK_720P_1080I
		.repeat_time	= HDMI_2_TIMES_REPEAT,
#else
		.repeat_time	= NO_REPEAT,
#endif
		.aspect_ratio   = TV_ASPECT_RATIO_16_9,
		.cc		= CC_ITU709,
		.ss		= SS_SCAN_UNDER,
		.sc		= SC_SCALE_HORIZ_VERT,
	},
	{
		.VIC		= HDMI_480i60,
		.color_prefer   = COLORSPACE_RGB444,
		.color_depth	= COLORDEPTH_24B,
		.bar_info	= B_BAR_VERT_HORIZ,
		.repeat_time	= HDMI_2_TIMES_REPEAT,
		.aspect_ratio   = TV_ASPECT_RATIO_4_3,
		.cc		= CC_ITU601,
		.ss		= SS_SCAN_UNDER,
		.sc		= SC_SCALE_HORIZ_VERT,
	},
	{
		.VIC		= HDMI_480i60_16x9,
		.color_prefer   = COLORSPACE_RGB444,
		.color_depth	= COLORDEPTH_24B,
		.bar_info	= B_BAR_VERT_HORIZ,
		.repeat_time	= HDMI_2_TIMES_REPEAT,
		.aspect_ratio   = TV_ASPECT_RATIO_16_9,
		.cc		= CC_ITU601,
		.ss		= SS_SCAN_UNDER,
		.sc		= SC_SCALE_HORIZ_VERT,
	},
	{
		.VIC		= HDMI_480i60_16x9_rpt,
		.color_prefer   = COLORSPACE_RGB444,
		.color_depth	= COLORDEPTH_24B,
		.bar_info	= B_BAR_VERT_HORIZ,
		.repeat_time	= HDMI_4_TIMES_REPEAT,
		.aspect_ratio   = TV_ASPECT_RATIO_16_9,
		.cc		= CC_ITU601,
		.ss		= SS_SCAN_UNDER,
		.sc		= SC_SCALE_HORIZ_VERT,
	},
	{
		.VIC		= HDMI_1440x480p60,
		.color_prefer   = COLORSPACE_RGB444,
		.color_depth	= COLORDEPTH_24B,
		.bar_info	= B_BAR_VERT_HORIZ,
		.repeat_time	= NO_REPEAT,
		.aspect_ratio   = TV_ASPECT_RATIO_4_3,
		.cc		= CC_ITU601,
		.ss		= SS_SCAN_UNDER,
		.sc		= SC_SCALE_HORIZ_VERT,
	},
	{
		.VIC		= HDMI_1080p60,
		.color_prefer   = COLORSPACE_RGB444,
		.color_depth	= COLORDEPTH_24B,
		.bar_info	= B_BAR_VERT_HORIZ,
		.repeat_time	= NO_REPEAT,
		.aspect_ratio   = TV_ASPECT_RATIO_16_9,
		.cc		= CC_ITU709,
		.ss		= SS_SCAN_UNDER,
		.sc		= SC_SCALE_HORIZ_VERT,
	},
	{
		.VIC		= HDMI_576p50,
		.color_prefer   = COLORSPACE_RGB444,
		.color_depth	= COLORDEPTH_24B,
		.bar_info	= B_BAR_VERT_HORIZ,
		.repeat_time	= NO_REPEAT,
		.aspect_ratio   = TV_ASPECT_RATIO_4_3,
		.cc		= CC_ITU601,
		.ss		= SS_SCAN_UNDER,
		.sc		= SC_SCALE_HORIZ_VERT,
	},
	{
		.VIC		= HDMI_576p50_16x9,
		.color_prefer   = COLORSPACE_RGB444,
		.color_depth	= COLORDEPTH_24B,
		.bar_info	= B_BAR_VERT_HORIZ,
		.repeat_time	= NO_REPEAT,
		.aspect_ratio   = TV_ASPECT_RATIO_16_9,
		.cc		= CC_ITU601,
		.ss		= SS_SCAN_UNDER,
		.sc		= SC_SCALE_HORIZ_VERT,
	},
	{
		.VIC		= HDMI_576p50_16x9_rpt,
		.color_prefer   = COLORSPACE_RGB444,
		.color_depth	= COLORDEPTH_24B,
		.bar_info	= B_BAR_VERT_HORIZ,
		.repeat_time	= HDMI_4_TIMES_REPEAT,
		.aspect_ratio   = TV_ASPECT_RATIO_16_9,
		.cc		= CC_ITU601,
		.ss		= SS_SCAN_UNDER,
		.sc		= SC_SCALE_HORIZ_VERT,
	},
	{
		.VIC		= HDMI_720p50,
		.color_prefer   = COLORSPACE_RGB444,
		.color_depth	= COLORDEPTH_24B,
		.bar_info	= B_BAR_VERT_HORIZ,
		.repeat_time	= NO_REPEAT,
		.aspect_ratio   = TV_ASPECT_RATIO_16_9,
		.cc		= CC_ITU709,
		.ss		= SS_SCAN_UNDER,
		.sc		= SC_SCALE_HORIZ_VERT,
	},
	{
		.VIC		= HDMI_1080i50,
		.color_prefer   = COLORSPACE_RGB444,
		.color_depth	= COLORDEPTH_24B,
		.bar_info	= B_BAR_VERT_HORIZ,
		.repeat_time	= NO_REPEAT,
		.aspect_ratio   = TV_ASPECT_RATIO_16_9,
		.cc		= CC_ITU709,
		.ss		= SS_SCAN_UNDER,
		.sc		= SC_SCALE_HORIZ_VERT,
	},
	{
		.VIC		= HDMI_576i50,
		.color_prefer   = COLORSPACE_RGB444,
		.color_depth	= COLORDEPTH_24B,
		.bar_info	= B_BAR_VERT_HORIZ,
		.repeat_time	= HDMI_2_TIMES_REPEAT,
		.aspect_ratio   = TV_ASPECT_RATIO_4_3,
		.cc		= CC_ITU601,
		.ss		= SS_SCAN_UNDER,
		.sc		= SC_SCALE_HORIZ_VERT,
	},
	{
		.VIC		= HDMI_576i50_16x9,
		.color_prefer   = COLORSPACE_RGB444,
		.color_depth	= COLORDEPTH_24B,
		.bar_info	= B_BAR_VERT_HORIZ,
		.repeat_time	= HDMI_2_TIMES_REPEAT,
		.aspect_ratio   = TV_ASPECT_RATIO_16_9,
		.cc		= CC_ITU601,
		.ss		= SS_SCAN_UNDER,
		.sc		= SC_SCALE_HORIZ_VERT,
	},
	{
		.VIC		= HDMI_576i50_16x9_rpt,
		.color_prefer   = COLORSPACE_RGB444,
		.color_depth	= COLORDEPTH_24B,
		.bar_info	= B_BAR_VERT_HORIZ,
		.repeat_time	= HDMI_4_TIMES_REPEAT,
		.aspect_ratio   = TV_ASPECT_RATIO_16_9,
		.cc		= CC_ITU601,
		.ss		= SS_SCAN_UNDER,
		.sc		= SC_SCALE_HORIZ_VERT,
	},
	{
		.VIC		= HDMI_1080p50,
		.color_prefer   = COLORSPACE_RGB444,
		.color_depth	= COLORDEPTH_24B,
		.bar_info	= B_BAR_VERT_HORIZ,
		.repeat_time	= NO_REPEAT,
		.aspect_ratio   = TV_ASPECT_RATIO_16_9,
		.cc		= CC_ITU709,
		.ss		= SS_SCAN_UNDER,
		.sc		= SC_SCALE_HORIZ_VERT,
	},
	{
		.VIC		= HDMI_1080p24,
		.color_prefer   = COLORSPACE_RGB444,
		.color_depth	= COLORDEPTH_24B,
		.bar_info	= B_BAR_VERT_HORIZ,
		.repeat_time	= NO_REPEAT,
		.aspect_ratio   = TV_ASPECT_RATIO_16_9,
		.cc		= CC_ITU709,
		.ss		= SS_SCAN_UNDER,
		.sc		= SC_SCALE_HORIZ_VERT,
	},
	{
		.VIC		= HDMI_1080p25,
		.color_prefer   = COLORSPACE_RGB444,
		.color_depth	= COLORDEPTH_24B,
		.bar_info	= B_BAR_VERT_HORIZ,
		.repeat_time	= NO_REPEAT,
		.aspect_ratio   = TV_ASPECT_RATIO_16_9,
		.cc		= CC_ITU709,
		.ss		= SS_SCAN_UNDER,
		.sc		= SC_SCALE_HORIZ_VERT,
	},
	{
		.VIC		= HDMI_1080p30,
		.color_prefer   = COLORSPACE_RGB444,
		.color_depth	= COLORDEPTH_24B,
		.bar_info	= B_BAR_VERT_HORIZ,
		.repeat_time	= NO_REPEAT,
		.aspect_ratio   = TV_ASPECT_RATIO_16_9,
		.cc		= CC_ITU709,
		.ss		= SS_SCAN_UNDER,
		.sc		= SC_SCALE_HORIZ_VERT,
	},
	{
		.VIC		= HDMI_4k2k_30,
		.color_prefer   = COLORSPACE_RGB444,
		.color_depth	= COLORDEPTH_24B,
		.bar_info	= B_BAR_VERT_HORIZ,
		.repeat_time	= NO_REPEAT,
		.aspect_ratio   = TV_ASPECT_RATIO_16_9,
		.cc		= CC_ITU709,
		.ss		= SS_SCAN_UNDER,
		.sc		= SC_SCALE_HORIZ_VERT,
	},
	{
		.VIC		= HDMI_4k2k_25,
		.color_prefer   = COLORSPACE_RGB444,
		.color_depth	= COLORDEPTH_24B,
		.bar_info	= B_BAR_VERT_HORIZ,
		.repeat_time	= NO_REPEAT,
		.aspect_ratio   = TV_ASPECT_RATIO_16_9,
		.cc		= CC_ITU709,
		.ss		= SS_SCAN_UNDER,
		.sc		= SC_SCALE_HORIZ_VERT,
	},
	{
		.VIC		= HDMI_4k2k_24,
		.color_prefer   = COLORSPACE_RGB444,
		.color_depth	= COLORDEPTH_24B,
		.bar_info	= B_BAR_VERT_HORIZ,
		.repeat_time	= NO_REPEAT,
		.aspect_ratio   = TV_ASPECT_RATIO_16_9,
		.cc		= CC_ITU709,
		.ss		= SS_SCAN_UNDER,
		.sc		= SC_SCALE_HORIZ_VERT,
	},
	{
		.VIC		= HDMI_4k2k_smpte_24,
		.color_prefer   = COLORSPACE_RGB444,
		.color_depth	= COLORDEPTH_24B,
		.bar_info	= B_BAR_VERT_HORIZ,
		.repeat_time	= NO_REPEAT,
		.aspect_ratio   = TV_ASPECT_RATIO_16_9,
		.cc		= CC_ITU709,
		.ss		= SS_SCAN_UNDER,
		.sc		= SC_SCALE_HORIZ_VERT,
	},
	{
		.VIC		= HDMI_4096x2160p25_256x135,
		.color_prefer   = COLORSPACE_RGB444,
		.color_depth	= COLORDEPTH_24B,
		.bar_info	= B_BAR_VERT_HORIZ,
		.repeat_time	= NO_REPEAT,
		.aspect_ratio   = TV_ASPECT_RATIO_16_9,
		.cc		= CC_ITU709,
		.ss		= SS_SCAN_UNDER,
		.sc		= SC_SCALE_HORIZ_VERT,
	},
	{
		.VIC		= HDMI_4096x2160p30_256x135,
		.color_prefer   = COLORSPACE_RGB444,
		.color_depth	= COLORDEPTH_24B,
		.bar_info	= B_BAR_VERT_HORIZ,
		.repeat_time	= NO_REPEAT,
		.aspect_ratio   = TV_ASPECT_RATIO_16_9,
		.cc		= CC_ITU709,
		.ss		= SS_SCAN_UNDER,
		.sc		= SC_SCALE_HORIZ_VERT,
	},
	{
		.VIC		= HDMI_4096x2160p50_256x135,
		.color_prefer   = COLORSPACE_RGB444,
		.color_depth	= COLORDEPTH_24B,
		.bar_info	= B_BAR_VERT_HORIZ,
		.repeat_time	= NO_REPEAT,
		.aspect_ratio   = TV_ASPECT_RATIO_16_9,
		.cc		= CC_ITU709,
		.ss		= SS_SCAN_UNDER,
		.sc		= SC_SCALE_HORIZ_VERT,
	},
	{
		.VIC		= HDMI_4096x2160p60_256x135,
		.color_prefer   = COLORSPACE_RGB444,
		.color_depth	= COLORDEPTH_24B,
		.bar_info	= B_BAR_VERT_HORIZ,
		.repeat_time	= NO_REPEAT,
		.aspect_ratio   = TV_ASPECT_RATIO_16_9,
		.cc		= CC_ITU709,
		.ss		= SS_SCAN_UNDER,
		.sc		= SC_SCALE_HORIZ_VERT,
	},
	{
		.VIC		= HDMI_4k2k_60,
		.color_prefer   = COLORSPACE_RGB444,
		.color_depth	= COLORDEPTH_24B,
		.bar_info	= B_BAR_VERT_HORIZ,
		.repeat_time	= NO_REPEAT,
		.aspect_ratio   = TV_ASPECT_RATIO_16_9,
		.cc		= CC_ITU709,
		.ss		= SS_SCAN_UNDER,
		.sc		= SC_SCALE_HORIZ_VERT,
	},
	{
		.VIC		= HDMI_4k2k_50,
		.color_prefer   = COLORSPACE_RGB444,
		.color_depth	= COLORDEPTH_24B,
		.bar_info	= B_BAR_VERT_HORIZ,
		.repeat_time	= NO_REPEAT,
		.aspect_ratio   = TV_ASPECT_RATIO_16_9,
		.cc		= CC_ITU709,
		.ss		= SS_SCAN_UNDER,
		.sc		= SC_SCALE_HORIZ_VERT,
	},
	{
		.VIC		= HDMI_4k2k_60,
		.color_prefer   = COLORSPACE_RGB444,
		.color_depth	= COLORDEPTH_24B,
		.bar_info	= B_BAR_VERT_HORIZ,
		.repeat_time	= NO_REPEAT,
		.aspect_ratio   = TV_ASPECT_RATIO_16_9,
		.cc		= CC_ITU709,
		.ss		= SS_SCAN_UNDER,
		.sc		= SC_SCALE_HORIZ_VERT,
	},
	{
		.VIC		= HDMI_4k2k_50,
		.color_prefer   = COLORSPACE_RGB444,
		.color_depth	= COLORDEPTH_24B,
		.bar_info	= B_BAR_VERT_HORIZ,
		.repeat_time	= NO_REPEAT,
		.aspect_ratio   = TV_ASPECT_RATIO_16_9,
		.cc		= CC_ITU709,
		.ss		= SS_SCAN_UNDER,
		.sc		= SC_SCALE_HORIZ_VERT,
	},
	{
		.VIC		= HDMI_4k2k_60,
		.color_prefer   = COLORSPACE_RGB444,
		.color_depth	= COLORDEPTH_24B,
		.bar_info	= B_BAR_VERT_HORIZ,
		.repeat_time	= NO_REPEAT,
		.aspect_ratio   = TV_ASPECT_RATIO_16_9,
		.cc		= CC_ITU709,
		.ss		= SS_SCAN_UNDER,
		.sc		= SC_SCALE_HORIZ_VERT,
	},
	{
		.VIC		= HDMI_4k2k_50,
		.color_prefer   = COLORSPACE_RGB444,
		.color_depth	= COLORDEPTH_24B,
		.bar_info	= B_BAR_VERT_HORIZ,
		.repeat_time	= NO_REPEAT,
		.aspect_ratio   = TV_ASPECT_RATIO_16_9,
		.cc		= CC_ITU709,
		.ss		= SS_SCAN_UNDER,
		.sc		= SC_SCALE_HORIZ_VERT,
	},
	{
		.VIC		= HDMI_4k2k_60_y420,
		.color_prefer	= COLORSPACE_YUV420,
		.color_depth	= COLORDEPTH_24B,
		.bar_info	= B_BAR_VERT_HORIZ,
		.repeat_time	= NO_REPEAT,
		.aspect_ratio	= TV_ASPECT_RATIO_16_9,
		.cc		= CC_ITU709,
		.ss		= SS_SCAN_UNDER,
		.sc		= SC_SCALE_HORIZ_VERT,
	},
	{
		.VIC		= HDMI_4k2k_50_y420,
		.color_prefer	= COLORSPACE_YUV420,
		.color_depth	= COLORDEPTH_24B,
		.bar_info	= B_BAR_VERT_HORIZ,
		.repeat_time	= NO_REPEAT,
		.aspect_ratio	= TV_ASPECT_RATIO_16_9,
		.cc		= CC_ITU709,
		.ss		= SS_SCAN_UNDER,
		.sc		= SC_SCALE_HORIZ_VERT,
	},
	{
		.VIC		= HDMI_4k2k_smpte_60_y420,
		.color_prefer	= COLORSPACE_YUV420,
		.color_depth	= COLORDEPTH_24B,
		.bar_info	= B_BAR_VERT_HORIZ,
		.repeat_time	= NO_REPEAT,
		.aspect_ratio	= TV_ASPECT_RATIO_16_9,
		.cc		= CC_ITU709,
		.ss		= SS_SCAN_UNDER,
		.sc		= SC_SCALE_HORIZ_VERT,
	},
	{
		.VIC		= HDMI_4k2k_smpte_50_y420,
		.color_prefer	= COLORSPACE_YUV420,
		.color_depth	= COLORDEPTH_24B,
		.bar_info	= B_BAR_VERT_HORIZ,
		.repeat_time	= NO_REPEAT,
		.aspect_ratio	= TV_ASPECT_RATIO_16_9,
		.cc		= CC_ITU709,
		.ss		= SS_SCAN_UNDER,
		.sc		= SC_SCALE_HORIZ_VERT,
	},
};

static struct hdmitx_vidpara *hdmi_get_video_param(
	enum hdmi_vic VideoCode)
{
	struct hdmitx_vidpara *video_param = NULL;
	int  i;
	int count = ARRAY_SIZE(hdmi_tx_video_params);
	for (i = 0; i < count; i++) {
		if (hdmi_tx_video_params[i].VIC == VideoCode)
			break;
	}
	if (i < count)
		video_param = &(hdmi_tx_video_params[i]);
	return video_param;
}

static void hdmi_tx_construct_avi_packet(
	struct hdmitx_vidpara *video_param, char *AVI_DB)
{
	unsigned char color, bar_info, aspect_ratio, cc, ss, sc, ec = 0;
	ss = video_param->ss;
	bar_info = video_param->bar_info;
	if (video_param->color == COLORSPACE_YUV444)
		color = 2;
	else if (video_param->color == COLORSPACE_YUV422)
		color = 1;
	else
		color = 0;
	AVI_DB[0] = (ss) | (bar_info << 2) | (1<<4) | (color << 5);

	aspect_ratio = video_param->aspect_ratio;
	cc = video_param->cc;
	/*HDMI CT 7-24*/
	AVI_DB[1] = 8 | (aspect_ratio << 4) | (cc << 6);

	sc = video_param->sc;
	if (video_param->cc == CC_ITU601)
		ec = 0;
	if (video_param->cc == CC_ITU709)
		/*according to CEA-861-D, all other values are reserved*/
		ec = 1;
	AVI_DB[2] = (sc) | (ec << 4);

	AVI_DB[3] = video_param->VIC;
	if ((video_param->VIC == HDMI_4k2k_30) ||
		(video_param->VIC == HDMI_4k2k_25) ||
		(video_param->VIC == HDMI_4k2k_24) ||
		(video_param->VIC == HDMI_4k2k_smpte_24))
		/*HDMI Spec V1.4b P151*/
		AVI_DB[3] = 0;

	AVI_DB[4] = video_param->repeat_time;
}

/************************************
*	hdmitx protocol level interface
*************************************/

void hdmitx_init_parameters(struct hdmitx_info *info)
{
	memset(info, 0, sizeof(struct hdmitx_info));

	info->video_out_changing_flag = 1;

	info->audio_flag = 1;
	info->audio_info.type = CT_REFER_TO_STREAM;
	info->audio_info.format = AF_I2S;
	info->audio_info.fs = FS_44K1;
	info->audio_info.ss = SS_16BITS;
	info->audio_info.channels = CC_2CH;
	info->audio_out_changing_flag = 1;

	info->auto_hdcp_ri_flag = 1;
	info->hw_sha_calculator_flag = 1;

}

/*
 * HDMI Identifier = 0x000c03
 * If not, treated as a DVI Device
 */
static int is_dvi_device(struct rx_cap *pRXCap)
{
	if (pRXCap->IEEEOUI != 0x000c03)
		return 1;
	else
		return 0;
}

void hdmitx_output_rgb(void)
{
	hdmi_output_rgb = 1;
}

int hdmitx_set_display(struct hdmitx_dev *hdev, enum hdmi_vic VideoCode)
{
	struct hdmitx_vidpara *param = NULL;
	enum hdmi_vic vic;
	int i, ret = -1;
	unsigned char AVI_DB[32];
	unsigned char AVI_HB[32];
	AVI_HB[0] = TYPE_AVI_INFOFRAMES;
	AVI_HB[1] = AVI_INFOFRAMES_VERSION;
	AVI_HB[2] = AVI_INFOFRAMES_LENGTH;
	for (i = 0; i < 32; i++)
		AVI_DB[i] = 0;

	vic = hdev->HWOp.GetState(hdev, STAT_VIDEO_VIC, 0);
	hdmi_print(IMP, SYS "already init VIC = %d  Now VIC = %d\n",
		vic, VideoCode);
	if ((vic != HDMI_Unkown) && (vic == VideoCode)) {
		hdev->cur_VIC = vic;
		/* return 1; */
	}

	param = hdmi_get_video_param(VideoCode);
	hdev->cur_video_param = param;
	if (param) {
		param->color = param->color_prefer;
		if (hdmi_output_rgb) {
			param->color = COLORSPACE_RGB444;
		} else {
			/* HDMI CT 7-24 Pixel Encoding
			 * YCbCr to YCbCr Sink
			 */
			switch (hdev->RXCap.native_Mode & 0x30) {
			case 0x20:/*bit5==1, then support YCBCR444 + RGB*/
			case 0x30:
				param->color = COLORSPACE_YUV444;
				break;
			case 0x10:/*bit4==1, then support YCBCR422 + RGB*/
				param->color = COLORSPACE_YUV422;
				break;
			default:
				param->color = COLORSPACE_RGB444;
			}
			/* For Y420 modes */
			switch (VideoCode) {
			case HDMI_3840x2160p50_16x9_Y420:
			case HDMI_3840x2160p60_16x9_Y420:
			case HDMI_4096x2160p50_256x135_Y420:
			case HDMI_4096x2160p60_256x135_Y420:
				param->color = COLORSPACE_YUV420;
				break;
			default:
				break;
			}
			if (param->color == COLORSPACE_RGB444) {
				hdev->para->cs = COLORSPACE_RGB444;
				hdev->para->cd = COLORDEPTH_24B;
				pr_info("hdmitx: rx edid only support RGB format\n");
			}

		}
		if (hdev->HWOp.SetDispMode(hdev) >= 0) {
			/* HDMI CT 7-33 DVI Sink, no HDMI VSDB nor any
			 * other VSDB, No GB or DI expected
			 * TMDS_MODE[hdmi_config]
			 * 0: DVI Mode	   1: HDMI Mode
			 */
			if (is_dvi_device(&hdev->RXCap)) {
				hdmi_print(1, "Sink is DVI device\n");
				hdev->HWOp.CntlConfig(hdev,
					CONF_HDMI_DVI_MODE, DVI_MODE);
			} else {
				hdmi_print(1, "Sink is HDMI device\n");
				hdev->HWOp.CntlConfig(hdev,
					CONF_HDMI_DVI_MODE, HDMI_MODE);
			}
			hdmi_tx_construct_avi_packet(param, (char *)AVI_DB);

			if ((VideoCode == HDMI_4k2k_30) ||
				(VideoCode == HDMI_4k2k_25) ||
				(VideoCode == HDMI_4k2k_24) ||
				(VideoCode == HDMI_4k2k_smpte_24))
				hdmi_set_vend_spec_infofram(hdev, VideoCode);
			else if ((!hdev->flag_3dfp) && (!hdev->flag_3dtb) &&
				(!hdev->flag_3dss))
				hdmi_set_vend_spec_infofram(hdev, 0);
			else /* nothing */
				;
			ret = 0;
		}
	}
	hdmitx_set_spd_info(hdev);
#if 0
	hdmitx_special_handler_video(hdev);
#endif
	return ret;
}

static void hdmi_set_vend_spec_infofram(struct hdmitx_dev *hdev,
	enum hdmi_vic VideoCode)
{
	int i;
	unsigned char VEN_DB[6];
	unsigned char VEN_HB[3];
	VEN_HB[0] = 0x81;
	VEN_HB[1] = 0x01;
	VEN_HB[2] = 0x5;

	for (i = 0; i < 0x6; i++)
		VEN_DB[i] = 0;
	VEN_DB[0] = 0x03;
	VEN_DB[1] = 0x0c;
	VEN_DB[2] = 0x00;
	VEN_DB[3] = 0x20;    /* 4k x 2k  Spec P156 */
	if (VideoCode == 0) {	   /* For non-4kx2k mode setting */
		hdev->HWOp.SetPacket(HDMI_PACKET_VEND, NULL, VEN_HB);
		return;
	}
	if (VideoCode == HDMI_4k2k_30)
		VEN_DB[4] = 0x1;
	else if (VideoCode == HDMI_4k2k_25)
		VEN_DB[4] = 0x2;
	else if (VideoCode == HDMI_4k2k_24)
		VEN_DB[4] = 0x3;
	else if (VideoCode == HDMI_4k2k_smpte_24)
		VEN_DB[4] = 0x4;
	else
		;
	hdev->HWOp.SetPacket(HDMI_PACKET_VEND, VEN_DB, VEN_HB);
}

int hdmi_set_3d(struct hdmitx_dev *hdev, int type, unsigned int param)
{
	int i;
	unsigned char VEN_DB[6];
	unsigned char VEN_HB[3];
	VEN_HB[0] = 0x81;
	VEN_HB[1] = 0x01;
	VEN_HB[2] = 0x6;
	if (type == T3D_DISABLE)
		hdev->HWOp.SetPacket(HDMI_PACKET_VEND, NULL, VEN_HB);
	else {
		for (i = 0; i < 0x6; i++)
			VEN_DB[i] = 0;
		VEN_DB[0] = 0x03;
		VEN_DB[1] = 0x0c;
		VEN_DB[2] = 0x00;
		VEN_DB[3] = 0x40;
		VEN_DB[4] = type<<4;
		VEN_DB[5] = param<<4;
		hdev->HWOp.SetPacket(HDMI_PACKET_VEND, VEN_DB, VEN_HB);
	}
	return 0;

}

/* Set Source Product Descriptor InfoFrame
 */
static void hdmitx_set_spd_info(struct hdmitx_dev *hdev)
{
	unsigned char SPD_DB[25] = {0x00};
	unsigned char SPD_HB[3] = {0x83, 0x1, 0x19};
	unsigned int len = 0;
	struct vendor_info_data *vend_data;
	if (hdev->config_data.vend_data)
		vend_data = hdev->config_data.vend_data;
	else {
		hdmi_print(INF, SYS "packet: can\'t get vendor data\n");
		return;
	}
	if (vend_data->vendor_name) {
		len = strlen(vend_data->vendor_name);
		strncpy(&SPD_DB[0], vend_data->vendor_name,
			(len > 8) ? 8 : len);
	}
	if (vend_data->product_desc) {
		len = strlen(vend_data->product_desc);
		strncpy(&SPD_DB[8], vend_data->product_desc,
			(len > 16) ? 16 : len);
	}
	SPD_DB[24] = 0x1;
	hdev->HWOp.SetPacket(HDMI_SOURCE_DESCRIPTION, SPD_DB, SPD_HB);
}
