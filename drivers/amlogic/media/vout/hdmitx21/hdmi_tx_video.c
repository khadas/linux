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

static void construct_avi_packet(struct hdmitx_dev *hdev)
{
	u8 avi_hb[3] = {0};
	u8 avi_db[28] = {0};
	struct hdmi_format_para *para = hdev->para;

	avi_hb[0] = IF_AVI;
	avi_hb[1] = 0x02;
	avi_hb[2] = 13;
	/* TODO */
	avi_db[1] = (0x2 << 5) | 0;
	avi_db[2] = (0x2 << 6) | (0x2 << 4);
	avi_db[3] = 0;
	avi_db[4] = para->timing.vic;
	avi_db[5] = 0;

	if (para->timing.vic == HDMI_95_3840x2160p30_16x9 ||
	    para->timing.vic == HDMI_94_3840x2160p25_16x9 ||
	    para->timing.vic == HDMI_93_3840x2160p24_16x9 ||
	    para->timing.vic == HDMI_98_4096x2160p24_256x135)
		/*HDMI Spec V1.4b P151*/
		avi_db[3] = 0;
	hdev->hwop.setinfoframe(HDMI_PACKET_AVI, avi_hb, &avi_db[1]);
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
	struct hdmi_format_para *param = NULL;
	enum hdmi_vic vic;
	int ret = -1;

	vic = hdev->hwop.getstate(hdev, STAT_VIDEO_VIC, 0);
	if (hdev->vend_id_hit)
		pr_info(VID "special tv detected\n");
	pr_info(VID "already init VIC = %d  Now VIC = %d\n",
		vic, videocode);
	if (vic != HDMI_UNKNOWN && vic == videocode)
		hdev->cur_VIC = vic;

	param = hdev->para;
	if (param) {
		/* HDMI CT 7-24 Pixel Encoding
		 * YCbCr to YCbCr Sink
		 */
		switch (hdev->rxcap.native_Mode & 0x30) {
		case 0x20:/*bit5==1, then support YCBCR444 + RGB*/
		case 0x30:
			param->cs = COLORSPACE_YUV444;
			break;
		case 0x10:/*bit4==1, then support YCBCR422 + RGB*/
			param->cs = COLORSPACE_YUV422;
			break;
		default:
			param->cs = COLORSPACE_RGB444;
		}
		/* For Y420 modes */
		switch (videocode) {
		case HDMI_96_3840x2160p50_16x9:
		case HDMI_97_3840x2160p60_16x9:
		case HDMI_101_4096x2160p50_256x135:
		case HDMI_102_4096x2160p60_256x135:
			//param->cs = COLORSPACE_YUV420; /* TODO */
			break;
		default:
			break;
		}

		if (param->cs == COLORSPACE_RGB444)
			pr_info(VID "rx edid only support RGB format\n");

		if (videocode >= HDMITX_VESA_OFFSET) {
			hdev->para->cs = COLORSPACE_RGB444;
			hdev->para->cd = COLORDEPTH_24B;
			pr_info("hdmitx: VESA only support RGB format\n");
		}

		if (hdev->hwop.setdispmode(hdev) >= 0) {
			construct_avi_packet(hdev);

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
	u8 VEN_DB[28];
	u8 VEN_HB[3];

	VEN_HB[0] = 0x81;
	VEN_HB[1] = 0x01;
	VEN_HB[2] = 0x5;

	if (videocode == 0) {	   /* For non-4kx2k mode setting */
		hdev->hwop.setinfoframe(HDMI_PACKET_VEND, NULL, NULL);
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
	hdev->hwop.setinfoframe(HDMI_PACKET_VEND, VEN_HB, VEN_DB);
}

int hdmi21_set_3d(struct hdmitx_dev *hdev, int type, u32 param)
{
	int i;
	u8 VEN_DB[28];
	u8 VEN_HB[3];

	VEN_HB[0] = 0x81;
	VEN_HB[1] = 0x01;
	VEN_HB[2] = 0x6;
	if (type == T3D_DISABLE) {
		hdev->hwop.setinfoframe(HDMI_PACKET_VEND, NULL, NULL);
	} else {
		for (i = 0; i < 0x6; i++)
			VEN_DB[i] = 0;
		VEN_DB[0] = GET_OUI_BYTE0(HDMI_IEEEOUI);
		VEN_DB[1] = GET_OUI_BYTE1(HDMI_IEEEOUI);
		VEN_DB[2] = GET_OUI_BYTE2(HDMI_IEEEOUI);
		VEN_DB[3] = 0x40;
		VEN_DB[4] = type << 4;
		VEN_DB[5] = param << 4;
		hdev->hwop.setinfoframe(HDMI_PACKET_VEND, VEN_HB, VEN_DB);
	}
	return 0;
}

/* Set Source Product Descriptor InfoFrame
 */
static void hdmitx_set_spd_info(struct hdmitx_dev *hdev)
{
	u8 SPD_DB[28] = {0x00};
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
	hdev->hwop.setinfoframe(HDMI_SOURCE_DESCRIPTION, SPD_HB, SPD_DB);
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
	u8 DB[28]; /* to be fulfilled */
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

	hdev->hwop.setinfoframe(HDMI_PACKET_VEND, HB, DB);
	return 1;
}
