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
	struct hdmi_avi_infoframe *info = &hdev->infoframes.avi.avi;
	struct hdmi_format_para *para = hdev->para;
	int ret;

	ret = hdmi_avi_infoframe_init(info);
	if (ret)
		pr_info("init avi infoframe failed\n");
	info->version = 2;
	info->colorspace = para->cs;
	info->scan_mode = HDMI_SCAN_MODE_NONE;
	if (para->timing.v_active <= 576)
		info->colorimetry = HDMI_COLORIMETRY_ITU_601;
	else
		info->colorimetry = HDMI_COLORIMETRY_ITU_709;
	info->picture_aspect = HDMI_PICTURE_ASPECT_16_9;
	info->active_aspect = HDMI_ACTIVE_ASPECT_PICTURE;
	info->itc = 0;
	info->extended_colorimetry = HDMI_EXTENDED_COLORIMETRY_XV_YCC_601;
	info->quantization_range = HDMI_QUANTIZATION_RANGE_LIMITED;
	info->nups = HDMI_NUPS_UNKNOWN;
	info->video_code = para->timing.vic;
	if (para->timing.vic == HDMI_95_3840x2160p30_16x9 ||
	    para->timing.vic == HDMI_94_3840x2160p25_16x9 ||
	    para->timing.vic == HDMI_93_3840x2160p24_16x9 ||
	    para->timing.vic == HDMI_98_4096x2160p24_256x135)
		/*HDMI Spec V1.4b P151*/
		if (!hdev->frl_rate) /* TODO, clear under FRL */
			info->video_code = 0;
	/* refer to CTA-861-H Page 69 */
	if (info->video_code >= 128)
		info->version = 3;
	info->ycc_quantization_range = HDMI_YCC_QUANTIZATION_RANGE_LIMITED;
	info->content_type = HDMI_CONTENT_TYPE_GRAPHICS;
	info->pixel_repeat = 0;
	if (para->timing.pi_mode == 0) { /* interlaced modes */
		if (para->timing.h_active == 1440)
			info->pixel_repeat = 1;
		if (para->timing.h_active == 2880)
			info->pixel_repeat = 3;
	}
	info->top_bar = 0;
	info->bottom_bar = 0;
	info->left_bar = 0;
	info->right_bar = 0;
	hdmi_avi_infoframe_set(info);
}

/************************************
 *	hdmitx protocol level interface
 *************************************/

/*
 * HDMI Identifier = HDMI_IEEE_OUI 0x000c03
 * If not, treated as a DVI Device
 */
static int is_dvi_device(struct rx_cap *prxcap)
{
	if (prxcap->ieeeoui != HDMI_IEEE_OUI)
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
	if (vic != HDMI_0_UNKNOWN && vic == videocode)
		hdev->cur_VIC = vic;

	param = hdev->para;
	if (!param) {
		pr_info("%s[%d]\n", __func__, __LINE__);
		return ret;
	}

	if (param->cs == HDMI_COLORSPACE_YUV444)
		if (!(hdev->rxcap.native_Mode & (1 << 5))) {
			param->cs = HDMI_COLORSPACE_YUV422;
			pr_info("change cs from 444 to 422\n");
		}
	if (param->cs == HDMI_COLORSPACE_YUV422)
		if (!(hdev->rxcap.native_Mode & (1 << 4))) {
			param->cs = HDMI_COLORSPACE_RGB;
			pr_info("change cs from 422 to rgb\n");
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

	if (param->cs == HDMI_COLORSPACE_RGB)
		pr_info(VID "rx edid only support RGB format\n");

	if (videocode >= HDMITX_VESA_OFFSET) {
		hdev->para->cs = HDMI_COLORSPACE_RGB;
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
		    videocode == HDMI_98_4096x2160p24_256x135) {
			if (!hdev->frl_rate) /* TODO */
				hdmi_set_vend_spec_infofram(hdev, videocode);
		} else if ((!hdev->flag_3dfp) && (!hdev->flag_3dtb) &&
			 (!hdev->flag_3dss))
			hdmi_set_vend_spec_infofram(hdev, 0);
		else
			;

		if (hdev->allm_mode) {
			hdmitx21_construct_vsif(hdev, VT_ALLM, 1, NULL);
			hdev->hwop.cntlconfig(hdev, CONF_CT_MODE,
				SET_CT_OFF);
		} else {
			hdev->hwop.cntlconfig(hdev, CONF_CT_MODE,
				hdev->ct_mode | hdev->it_content << 4);
		}
		ret = 0;
	}
	hdmitx_set_spd_info(hdev);

	return ret;
}

static void hdmi_set_vend_spec_infofram(struct hdmitx_dev *hdev,
					enum hdmi_vic videocode)
{
	int i;
	u8 db[28];
	u8 *ven_db = &db[1];
	u8 ven_hb[3];

	ven_hb[0] = 0x81;
	ven_hb[1] = 0x01;
	ven_hb[2] = 0x5;

	if (videocode == 0) {	   /* For non-4kx2k mode setting */
		hdmi_vend_infoframe_rawset(NULL, NULL);
		return;
	}

	if (hdev->rxcap.dv_info.block_flag == CORRECT ||
	    hdev->dv_src_feature == 1) {	   /* For dolby */
		return;
	}

	for (i = 0; i < 0x6; i++)
		ven_db[i] = 0;
	ven_db[0] = GET_OUI_BYTE0(HDMI_IEEE_OUI);
	ven_db[1] = GET_OUI_BYTE1(HDMI_IEEE_OUI);
	ven_db[2] = GET_OUI_BYTE2(HDMI_IEEE_OUI);
	ven_db[3] = 0x00;    /* 4k x 2k  Spec P156 */

	if (videocode == HDMI_95_3840x2160p30_16x9) {
		ven_db[3] = 0x20;
		ven_db[4] = 0x1;
	} else if (videocode == HDMI_94_3840x2160p25_16x9) {
		ven_db[3] = 0x20;
		ven_db[4] = 0x2;
	} else if (videocode == HDMI_93_3840x2160p24_16x9) {
		ven_db[3] = 0x20;
		ven_db[4] = 0x3;
	} else if (videocode == HDMI_98_4096x2160p24_256x135) {
		ven_db[3] = 0x20;
		ven_db[4] = 0x4;
	} else {
		;
	}
	hdmi_vend_infoframe2_rawset(ven_hb, db);
}

int hdmi21_set_3d(struct hdmitx_dev *hdev, int type, u32 param)
{
	int i;
	u8 db[28];
	u8 *ven_db = &db[1];
	u8 ven_hb[3];
	struct hdmi_vendor_infoframe *info;

	info = &hdev->infoframes.vend.vendor.hdmi;

	ven_hb[0] = 0x81;
	ven_hb[1] = 0x01;
	ven_hb[2] = 0x6;
	if (type == T3D_DISABLE) {
		hdmi_vend_infoframe_rawset(ven_hb, db);
	} else {
		for (i = 0; i < 0x6; i++)
			ven_db[i] = 0;
		ven_db[0] = GET_OUI_BYTE0(HDMI_IEEE_OUI);
		ven_db[1] = GET_OUI_BYTE1(HDMI_IEEE_OUI);
		ven_db[2] = GET_OUI_BYTE2(HDMI_IEEE_OUI);
		ven_db[3] = 0x40;
		ven_db[4] = type << 4;
		ven_db[5] = param << 4;
		hdmi_vend_infoframe_rawset(ven_hb, db);
	}
	return 0;
}

/* Set Source Product Descriptor InfoFrame
 */
static void hdmitx_set_spd_info(struct hdmitx_dev *hdev)
{
	u8 spd_db[28] = {0x00};
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
		strncpy(&spd_db[0], vend_data->vendor_name,
			(len > 8) ? 8 : len);
	}
	if (vend_data->product_desc) {
		len = strlen(vend_data->product_desc);
		strncpy(&spd_db[8], vend_data->product_desc,
			(len > 16) ? 16 : len);
	}
	spd_db[24] = 0x1;
	// TODO hdev->hwop.setinfoframe(HDMI_INFOFRAME_TYPE_SPD, SPD_HB);
}

static void fill_hdmi4k_vsif_data(enum hdmi_vic vic, u8 *db,
				  u8 *hb)
{
	if (!db || !hb)
		return;

	if (vic == HDMI_95_3840x2160p30_16x9)
		db[4] = 0x1;
	else if (vic == HDMI_94_3840x2160p25_16x9)
		db[4] = 0x2;
	else if (vic == HDMI_93_3840x2160p24_16x9)
		db[4] = 0x3;
	else if (vic == HDMI_98_4096x2160p24_256x135)
		db[4] = 0x4;
	else
		return;
	hb[0] = 0x81;
	hb[1] = 0x01;
	hb[2] = 0x5;
	db[3] = 0x20;
}

int hdmitx21_construct_vsif(struct hdmitx_dev *hdev, enum vsif_type type,
			  int on, void *param)
{
	u8 hb[3] = {0x81, 0x1, 0};
	u8 len = 0; /* hb[2] = len */
	u8 vsif_db[28] = {0}; /* to be fulfilled */
	u8 *db = &vsif_db[1]; /* to be fulfilled */
	u32 ieeeoui = 0;

	if (!hdev || type >= VT_MAX)
		return 0;

	switch (type) {
	case VT_DEFAULT:
		break;
	case VT_HDMI14_4K:
		ieeeoui = HDMI_IEEE_OUI;
		len = 5;
		hb[2] = len;
		db[0] = GET_OUI_BYTE0(ieeeoui);
		db[1] = GET_OUI_BYTE1(ieeeoui);
		db[2] = GET_OUI_BYTE2(ieeeoui);
		if (_is_hdmi14_4k(hdev->cur_VIC)) {
			fill_hdmi4k_vsif_data(hdev->cur_VIC, db, hb);
			hdmitx21_set_avi_vic(0);
			hdmi_vend_infoframe_rawset(hb, vsif_db);
		}
		break;
	case VT_ALLM:
		ieeeoui = HDMI_FORUM_IEEE_OUI;
		len = 5;
		hb[2] = len;
		db[0] = GET_OUI_BYTE0(ieeeoui);
		db[1] = GET_OUI_BYTE1(ieeeoui);
		db[2] = GET_OUI_BYTE2(ieeeoui);
		db[3] = 0x1; /* Fixed value */
		if (on) {
			db[4] |= 1 << 1; /* set bit1, ALLM_MODE */
			if (_is_hdmi14_4k(hdev->cur_VIC))
				hdmitx21_set_avi_vic(hdev->cur_VIC);
			hdmi_vend_infoframe2_rawset(hb, vsif_db);
		} else {
			db[4] &= ~(1 << 1); /* clear bit1, ALLM_MODE */
			/* 1.When the Source stops transmitting the HF-VSIF,
			 * the Sink shall interpret this as an indication
			 * that transmission offeatures described in this
			 * Infoframe has stopped
			 * 2.When a Source is utilizing the HF-VSIF for ALLM
			 * signaling only and indicates the Sink should
			 * revert fromow-latency Mode to its previous mode,
			 * the Source should send ALLM Mode = 0 to quickly
			 * and reliably request the change. If a Source
			 * indicates ALLM Mode = 0 in this manner , the
			 * Source should transmit an HF-VSIF with ALLM Mode = 0
			 * for at least 4 frames but for not more than 1 second.
			 */
			/* hdmi_vend_infoframe2_rawset(hb, vsif_db); */
			/* wait for 4frames ~ 1S, then stop send HF-VSIF */
			hdmi_vend_infoframe2_rawset(NULL, NULL);
		}
		break;
	default:
		break;
	}
	return 1;
}
