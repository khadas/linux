// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
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

#include <linux/amlogic/media/vout/hdmi_tx21/hdmi_info_global.h>
#include <linux/amlogic/media/vout/hdmi_tx21/hdmi_tx_module.h>
#include "hw/common.h"

static void hdmitx_set_spd_info(struct hdmitx_dev *hdmitx_device);
static void hdmi_set_vend_spec_infofram(struct hdmitx_dev *hdev,
					enum hdmi_vic videocode);

static struct hdmitx_vidpara hdmi_tx_video_params[] = {
	{
		.VIC		= HDMI_16_1920x1080p60_16x9,
		.color_prefer = COLORSPACE_RGB444,
		.color_depth	= COLORDEPTH_24B,
		.bar_info	= B_BAR_VERT_HORIZ,
		.repeat_time	= NO_REPEAT,
		.aspect_ratio = TV_ASPECT_RATIO_16_9,
		.cc		= CC_ITU709,
		.ss		= SS_SCAN_UNDER,
		.sc		= SC_SCALE_HORIZ_VERT,
	},
};

static struct
hdmitx_vidpara *hdmi_get_video_param(enum hdmi_vic videocode)
{
	struct hdmitx_vidpara *video_param = NULL;
	int  i;
	int count = ARRAY_SIZE(hdmi_tx_video_params);

	for (i = 0; i < count; i++) {
		if (videocode == hdmi_tx_video_params[i].VIC)
			break;
	}
	if (i < count)
		video_param = &hdmi_tx_video_params[i];
	return video_param;
}

static void hdmi_tx_construct_avi_packet(struct hdmitx_vidpara *video_param,
					 char *AVI_DB)
{
	u8 color, bar_info, aspect_ratio, cc, ss, sc, ec = 0;

	ss = video_param->ss;
	bar_info = video_param->bar_info;
	if (video_param->color == COLORSPACE_YUV444)
		color = 2;
	else if (video_param->color == COLORSPACE_YUV422)
		color = 1;
	else
		color = 0;
	AVI_DB[0] = (ss) | (bar_info << 2) | (1 << 4) | (color << 5);

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
	if (video_param->VIC == HDMI_95_3840x2160p30_16x9 ||
	    video_param->VIC == HDMI_94_3840x2160p25_16x9 ||
	    video_param->VIC == HDMI_93_3840x2160p24_16x9 ||
	    video_param->VIC == HDMI_98_4096x2160p24_256x135)
		/*HDMI Spec V1.4b P151*/
		AVI_DB[3] = 0;

	AVI_DB[4] = video_param->repeat_time;
}

/************************************
 *	hdmitx protocol level interface
 *************************************/

/*
 * HDMI Identifier = HDMI_IEEEOUI 0x000c03
 * If not, treated as a DVI Device
 */
static int is_dvi_device(struct rx_cap *prxcap)
{
	if (prxcap->ieeeoui != HDMI_IEEEOUI)
		return 1;
	else
		return 0;
}

int hdmitx21_set_display(struct hdmitx_dev *hdev, enum hdmi_vic videocode)
{
	struct hdmitx_vidpara *param = NULL;
	enum hdmi_vic vic;
	int i, ret = -1;
	u8 AVI_DB[32];
	u8 AVI_HB[32];

	AVI_HB[0] = TYPE_AVI_INFOFRAMES;
	AVI_HB[1] = AVI_INFOFRAMES_VERSION;
	AVI_HB[2] = AVI_INFOFRAMES_LENGTH;
	for (i = 0; i < 32; i++)
		AVI_DB[i] = 0;

	vic = hdev->hwop.getstate(hdev, STAT_VIDEO_VIC, 0);
	if (hdev->vend_id_hit)
		pr_info(VID "special tv detected\n");
	pr_info(VID "already init VIC = %d  Now VIC = %d\n",
		vic, videocode);
	if (vic != HDMI_UNKNOWN && vic == videocode)
		hdev->cur_VIC = vic;

	param = hdmi_get_video_param(videocode);
	hdev->cur_video_param = param;
	if (param) {
		param->color = param->color_prefer;
		/* HDMI CT 7-24 Pixel Encoding
		 * YCbCr to YCbCr Sink
		 */
		switch (hdev->rxcap.native_Mode & 0x30) {
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
		switch (videocode) {
		case HDMI_96_3840x2160p50_16x9:
		case HDMI_97_3840x2160p60_16x9:
		case HDMI_101_4096x2160p50_256x135:
		case HDMI_102_4096x2160p60_256x135:
			param->color = COLORSPACE_YUV420;
			break;
		default:
			break;
		}

		if (param->color == COLORSPACE_RGB444) {
			hdev->para->cs = hdev->cur_video_param->color;
			pr_info(VID "rx edid only support RGB format\n");
		}

		if (videocode >= HDMITX_VESA_OFFSET) {
			hdev->para->cs = COLORSPACE_RGB444;
			hdev->para->cd = COLORDEPTH_24B;
			pr_info("hdmitx: VESA only support RGB format\n");
		}

		if (hdev->hwop.setdispmode(hdev) >= 0) {
			/* HDMI CT 7-33 DVI Sink, no HDMI VSDB nor any
			 * other VSDB, No GB or DI expected
			 * TMDS_MODE[hdmi_config]
			 * 0: DVI Mode	   1: HDMI Mode
			 */
			if (is_dvi_device(&hdev->rxcap)) {
				pr_info(VID "Sink is DVI device\n");
				hdev->hwop.cntlconfig(hdev,
					CONF_HDMI_DVI_MODE, DVI_MODE);
			} else {
				pr_info(VID "Sink is HDMI device\n");
				hdev->hwop.cntlconfig(hdev,
					CONF_HDMI_DVI_MODE, HDMI_MODE);
			}
			hdmi_tx_construct_avi_packet(param, (char *)AVI_DB);

			if (videocode == HDMI_95_3840x2160p30_16x9 ||
			    videocode == HDMI_94_3840x2160p25_16x9 ||
			    videocode == HDMI_93_3840x2160p24_16x9 ||
			    videocode == HDMI_98_4096x2160p24_256x135)
				hdmi_set_vend_spec_infofram(hdev, videocode);
			else if ((!hdev->flag_3dfp) && (!hdev->flag_3dtb) &&
				 (!hdev->flag_3dss))
				hdmi_set_vend_spec_infofram(hdev, 0);
			else
				;

			if (hdev->allm_mode) {
				hdmitx21_construct_vsif(hdev, VT_ALLM, 1, NULL);
				hdev->hwop.cntlconfig(hdev, CONF_CT_MODE,
					SET_CT_OFF);
			}
			hdev->hwop.cntlconfig(hdev, CONF_CT_MODE,
				hdev->ct_mode);
			ret = 0;
		}
	}
	hdmitx_set_spd_info(hdev);

	return ret;
}

static void hdmi_set_vend_spec_infofram(struct hdmitx_dev *hdev,
					enum hdmi_vic videocode)
{
	int i;
	u8 VEN_DB[6];
	u8 VEN_HB[3];

	VEN_HB[0] = 0x81;
	VEN_HB[1] = 0x01;
	VEN_HB[2] = 0x5;

	if (videocode == 0) {	   /* For non-4kx2k mode setting */
		hdev->hwop.setpacket(HDMI_PACKET_VEND, NULL, VEN_HB);
		return;
	}

	if (hdev->rxcap.dv_info.block_flag == CORRECT ||
	    hdev->dv_src_feature == 1) {	   /* For dolby */
		return;
	}

	for (i = 0; i < 0x6; i++)
		VEN_DB[i] = 0;
	VEN_DB[0] = GET_OUI_BYTE0(HDMI_IEEEOUI);
	VEN_DB[1] = GET_OUI_BYTE1(HDMI_IEEEOUI);
	VEN_DB[2] = GET_OUI_BYTE2(HDMI_IEEEOUI);
	VEN_DB[3] = 0x00;    /* 4k x 2k  Spec P156 */

	if (videocode == HDMI_95_3840x2160p30_16x9) {
		VEN_DB[3] = 0x20;
		VEN_DB[4] = 0x1;
	} else if (videocode == HDMI_94_3840x2160p25_16x9) {
		VEN_DB[3] = 0x20;
		VEN_DB[4] = 0x2;
	} else if (videocode == HDMI_93_3840x2160p24_16x9) {
		VEN_DB[3] = 0x20;
		VEN_DB[4] = 0x3;
	} else if (videocode == HDMI_98_4096x2160p24_256x135) {
		VEN_DB[3] = 0x20;
		VEN_DB[4] = 0x4;
	} else {
		;
	}
	hdev->hwop.setpacket(HDMI_PACKET_VEND, VEN_DB, VEN_HB);
}

int hdmi21_set_3d(struct hdmitx_dev *hdev, int type, u32 param)
{
	int i;
	u8 VEN_DB[6];
	u8 VEN_HB[3];

	VEN_HB[0] = 0x81;
	VEN_HB[1] = 0x01;
	VEN_HB[2] = 0x6;
	if (type == T3D_DISABLE) {
		hdev->hwop.setpacket(HDMI_PACKET_VEND, NULL, VEN_HB);
	} else {
		for (i = 0; i < 0x6; i++)
			VEN_DB[i] = 0;
		VEN_DB[0] = GET_OUI_BYTE0(HDMI_IEEEOUI);
		VEN_DB[1] = GET_OUI_BYTE1(HDMI_IEEEOUI);
		VEN_DB[2] = GET_OUI_BYTE2(HDMI_IEEEOUI);
		VEN_DB[3] = 0x40;
		VEN_DB[4] = type << 4;
		VEN_DB[5] = param << 4;
		hdev->hwop.setpacket(HDMI_PACKET_VEND, VEN_DB, VEN_HB);
	}
	return 0;
}

/* Set Source Product Descriptor InfoFrame
 */
static void hdmitx_set_spd_info(struct hdmitx_dev *hdev)
{
	u8 SPD_DB[25] = {0x00};
	u8 SPD_HB[3] = {0x83, 0x1, 0x19};
	u32 len = 0;
	struct vendor_info_data *vend_data;

	if (hdev->config_data.vend_data) {
		vend_data = hdev->config_data.vend_data;
	} else {
		pr_info(VID "packet: can\'t get vendor data\n");
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
	hdev->hwop.setpacket(HDMI_SOURCE_DESCRIPTION, SPD_DB, SPD_HB);
}

static void fill_hdmi4k_vsif_data(enum hdmi_vic vic, u8 *DB,
				  u8 *HB)
{
	if (!DB || !HB)
		return;

	if (vic == HDMI_95_3840x2160p30_16x9)
		DB[4] = 0x1;
	else if (vic == HDMI_94_3840x2160p25_16x9)
		DB[4] = 0x2;
	else if (vic == HDMI_93_3840x2160p24_16x9)
		DB[4] = 0x3;
	else if (vic == HDMI_98_4096x2160p24_256x135)
		DB[4] = 0x4;
	else
		return;
	HB[0] = 0x81;
	HB[1] = 0x01;
	HB[2] = 0x5;
	DB[3] = 0x20;
}

int hdmitx21_construct_vsif(struct hdmitx_dev *hdev, enum vsif_type type,
			  int on, void *param)
{
	u8 HB[3] = {0x81, 0x1, 0};
	u8 len = 0; /* HB[2] = len */
	u8 DB[27]; /* to be fulfilled */
	u32 ieeeoui = 0;

	if (!hdev || type >= VT_MAX)
		return 0;
	memset(DB, 0, sizeof(DB));

	switch (type) {
	case VT_DEFAULT:
		break;
	case VT_HDMI14_4K:
		ieeeoui = HDMI_IEEEOUI;
		len = 5;
		if (_is_hdmi14_4k(hdev->cur_VIC)) {
			fill_hdmi4k_vsif_data(hdev->cur_VIC, DB, HB);
			hdmitx21_set_avi_vic(0);
		}
		break;
	case VT_ALLM:
		ieeeoui = HF_IEEEOUI;
		len = 5;
		DB[3] = 0x1; /* Fixed value */
		if (on) {
			DB[4] |= 1 << 1; /* set bit1, ALLM_MODE */
			if (_is_hdmi14_4k(hdev->cur_VIC))
				hdmitx21_set_avi_vic(hdev->cur_VIC);
		} else {
			DB[4] &= ~(1 << 1); /* clear bit1, ALLM_MODE */
			/* still send out HS_VSIF, no set AVI.VIC = 0 */
		}
		break;
	default:
		break;
	}

	HB[2] = len;
	DB[0] = GET_OUI_BYTE0(ieeeoui);
	DB[1] = GET_OUI_BYTE1(ieeeoui);
	DB[2] = GET_OUI_BYTE2(ieeeoui);

	hdev->hwop.setdatapacket(HDMI_PACKET_VEND, DB, HB);
	return 1;
}
