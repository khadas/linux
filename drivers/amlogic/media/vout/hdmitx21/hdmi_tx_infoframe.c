// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/major.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/reset.h>
#include <linux/compiler.h>
#include <linux/arm-smccc.h>

#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/media/vout/hdmi_tx21/enc_clk_config.h>
#include <linux/amlogic/media/vout/hdmi_tx21/hdmi_info_global.h>
#include <linux/amlogic/media/vout/hdmi_tx21/hdmi_tx_module.h>

#include "hdmi_tx.h"

void hdmi_vend_infoframe_set(struct hdmi_vendor_infoframe *info)
{
	u8 body[31] = {0};

	if (!info) {
		hdmitx_infoframe_send(HDMI_INFOFRAME_TYPE_VENDOR, NULL);
		return;
	}

	hdmi_vendor_infoframe_pack(info, body, sizeof(body));
	hdmitx_infoframe_send(HDMI_INFOFRAME_TYPE_VENDOR, body);
}

void hdmi_vend_infoframe_rawset(u8 *hb, u8 *pb)
{
	u8 body[31] = {0};

	if (!hb || !pb) {
		hdmitx_infoframe_send(HDMI_INFOFRAME_TYPE_VENDOR, NULL);
		return;
	}

	memcpy(body, hb, 3);
	memcpy(&body[3], pb, 28);
	hdmitx_infoframe_send(HDMI_INFOFRAME_TYPE_VENDOR, body);
}

void hdmi_avi_infoframe_set(struct hdmi_avi_infoframe *info)
{
	u8 body[31] = {0};

	if (!info) {
		hdmitx_infoframe_send(HDMI_INFOFRAME_TYPE_AVI, NULL);
		return;
	}

	hdmi_avi_infoframe_pack(info, body, sizeof(body));
	hdmitx_infoframe_send(HDMI_INFOFRAME_TYPE_AVI, body);
}

void hdmi_avi_infoframe_rawset(u8 *hb, u8 *pb)
{
	u8 body[31] = {0};

	if (!hb || !pb) {
		hdmitx_infoframe_send(HDMI_INFOFRAME_TYPE_AVI, NULL);
		return;
	}

	memcpy(body, hb, 3);
	memcpy(&body[3], pb, 28);
	hdmitx_infoframe_send(HDMI_INFOFRAME_TYPE_AVI, body);
}

void hdmi_avi_infoframe_config(enum avi_component_conf conf, u8 val)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();
	struct hdmi_avi_infoframe *info = &hdev->infoframes.avi.avi;
	struct hdmi_format_para *para = hdev->para;

	switch (conf) {
	case CONF_AVI_CS:
		info->colorspace = val;
		break;
	case CONF_AVI_BT2020:
		if (val == SET_AVI_BT2020) {
			info->colorimetry = HDMI_COLORIMETRY_EXTENDED;
			info->extended_colorimetry = HDMI_EXTENDED_COLORIMETRY_BT2020;
		}
		if (val == CLR_AVI_BT2020) {
			if (para->timing.v_total <= 576) {/* SD formats */
				info->colorimetry = HDMI_COLORIMETRY_ITU_601;
				info->extended_colorimetry = 0;
			} else {
				if (hdev->colormetry) {
					info->colorimetry = HDMI_COLORIMETRY_EXTENDED;
					info->extended_colorimetry =
						HDMI_EXTENDED_COLORIMETRY_BT2020;
				} else {
					info->colorimetry = HDMI_COLORIMETRY_ITU_709;
					info->extended_colorimetry = 0;
				}
			}
		}
		break;
	case CONF_AVI_Q01:
		info->quantization_range = val;
		break;
	case CONF_AVI_YQ01:
		info->ycc_quantization_range = val;
		break;
	case CONF_AVI_VIC:
		info->video_code = val;
		break;
	case CONF_AVI_AR:
		info->picture_aspect = val;
		break;
	case CONF_AVI_CT_TYPE:
		info->itc = (val >> 4) & 0x1;
		val = val & 0xf;
		if (val == SET_CT_OFF)
			info->content_type = 0;
		else if (val == SET_CT_GRAPHICS)
			info->content_type = 0;
		else if (val == SET_CT_PHOTO)
			info->content_type = 1;
		else if (val == SET_CT_CINEMA)
			info->content_type = 2;
		else if (val == SET_CT_GAME)
			info->content_type = 3;
		break;
	default:
		break;
	}
	hdmi_avi_infoframe_set(info);
}

/* EMP packets is different as other packets
 * no checksum, the successive packets in a video frame...
 */
void hdmi_emp_infoframe_set(struct emp_packet_st *info)
{
	u8 body[31] = {0};

	if (!info) {
		hdmitx_infoframe_send(HDMI_INFOFRAME_TYPE_EMP, NULL);
		return;
	}

	/* head body, 3bytes */
	body[0] = info->header.header;
	body[1] = info->header.first << 7 | info->header.last << 6;
	body[2] = info->header.seq_idx;
	/* packet body, 28bytes */
	body[3] = info->body.emp0.new << 7 |
		info->body.emp0.end << 6 |
		info->body.emp0.ds_type << 4 |
		info->body.emp0.afr << 3 |
		info->body.emp0.vfr << 2 |
		info->body.emp0.sync << 1;
	body[4] = 0; /* RSVD */
	body[5] = info->body.emp0.org_id;
	body[6] = info->body.emp0.ds_tag >> 8 & 0xff;
	body[7] = info->body.emp0.ds_tag & 0xff;
	body[8] = info->body.emp0.ds_length >> 8 & 0xff;
	body[9] = info->body.emp0.ds_length & 0xff;
	if (info->type == T_VRR_GAME) {
		body[10] = info->body.emp0.md.game_md.fva_factor_m1 << 4 |
			info->body.emp0.md.game_md.vrr_en << 0;
		body[11] = info->body.emp0.md.game_md.base_vfront;
		body[12] = info->body.emp0.md.game_md.brr_rate >> 8 & 3;
		body[13] = info->body.emp0.md.game_md.brr_rate & 0xff;
	} else { /* QMS-VRR */
		body[10] = info->body.emp0.md.qms_md.qms_en << 2 |
			info->body.emp0.md.qms_md.m_const << 1;
		body[11] = info->body.emp0.md.qms_md.base_vfront;
		body[12] = info->body.emp0.md.qms_md.next_tfr << 4 |
			(info->body.emp0.md.qms_md.brr_rate >> 8 & 3);
		body[13] = info->body.emp0.md.qms_md.brr_rate & 0xff;
	}
	hdmitx_infoframe_send(HDMI_INFOFRAME_TYPE_EMP, body);
}

/* this is only configuring the EMP frame body, not send by HW
 * then call hdmi_emp_infoframe_set to send out
 */
void hdmi_emp_frame_set_member(struct emp_packet_st *info,
	enum vrr_component_conf conf, u32 val)
{
	if (!info)
		return;

	switch (conf) {
	case CONF_HEADER_INIT:
		info->header.header = HDMI_INFOFRAME_TYPE_EMP; /* fixed value */
		break;
	case CONF_HEADER_LAST:
		info->header.last = !!val;
		break;
	case CONF_HEADER_FIRST:
		info->header.first = !!val;
		break;
	case CONF_HEADER_SEQ_INDEX:
		info->header.seq_idx = val;
		break;
	case CONF_SYNC:
		info->body.emp0.sync = !!val;
		break;
	case CONF_VFR:
		info->body.emp0.vfr = !!val;
		break;
	case CONF_AFR:
		info->body.emp0.afr = !!val;
		break;
	case CONF_DS_TYPE:
		info->body.emp0.ds_type = val & 3;
		break;
	case CONF_END:
		info->body.emp0.end = !!val;
		break;
	case CONF_NEW:
		info->body.emp0.new = !!val;
		break;
	case CONF_ORG_ID:
		info->body.emp0.org_id = val;
		break;
	case CONF_DATA_SET_TAG:
		info->body.emp0.ds_tag = val;
		break;
	case CONF_DATA_SET_LENGTH:
		info->body.emp0.ds_length = val;
		break;
	case CONF_VRR_EN:
		info->body.emp0.md.game_md.vrr_en = !!val;
		break;
	case CONF_FACTOR_M1:
		info->body.emp0.md.game_md.fva_factor_m1 = val;
		break;
	case CONF_QMS_EN:
		info->body.emp0.md.qms_md.qms_en = !!val;
		break;
	case CONF_M_CONST:
		info->body.emp0.md.qms_md.m_const = !!val;
		break;
	case CONF_BASE_VFRONT:
		info->body.emp0.md.qms_md.base_vfront = val;
		break;
	case CONF_NEXT_TFR:
		info->body.emp0.md.qms_md.next_tfr = val;
		break;
	case CONF_BASE_REFRESH_RATE:
		info->body.emp0.md.qms_md.brr_rate = val & 0x3ff;
		break;
	default:
		break;
	}
}

void hdmi_spd_infoframe_set(struct hdmi_spd_infoframe *info)
{
	u8 body[31] = {0};

	if (!info) {
		hdmitx_infoframe_send(HDMI_INFOFRAME_TYPE_SPD, NULL);
		return;
	}

	hdmi_spd_infoframe_pack(info, body, sizeof(body));
	hdmitx_infoframe_send(HDMI_INFOFRAME_TYPE_SPD, body);
}

void hdmi_audio_infoframe_set(struct hdmi_audio_infoframe *info)
{
	u8 body[31] = {0};

	if (!info) {
		hdmitx_infoframe_send(HDMI_INFOFRAME_TYPE_AUDIO, NULL);
		return;
	}

	hdmi_audio_infoframe_pack(info, body, sizeof(body));
	hdmitx_infoframe_send(HDMI_INFOFRAME_TYPE_AUDIO, body);
}

void hdmi_audio_infoframe_rawset(u8 *hb, u8 *pb)
{
	u8 body[31] = {0};

	if (!hb || !pb) {
		hdmitx_infoframe_send(HDMI_INFOFRAME_TYPE_AUDIO, NULL);
		return;
	}

	memcpy(body, hb, 3);
	memcpy(&body[3], pb, 28);
	hdmitx_infoframe_send(HDMI_INFOFRAME_TYPE_AUDIO, body);
}

void hdmi_drm_infoframe_set(struct hdmi_drm_infoframe *info)
{
	u8 body[31] = {0};

	if (!info) {
		hdmitx_infoframe_send(HDMI_INFOFRAME_TYPE_DRM, NULL);
		return;
	}

	hdmi_drm_infoframe_pack(info, body, sizeof(body));
	hdmitx_infoframe_send(HDMI_INFOFRAME_TYPE_DRM, body);
}

void hdmi_drm_infoframe_rawset(u8 *hb, u8 *pb)
{
	u8 body[31] = {0};

	if (!hb || !pb) {
		hdmitx_infoframe_send(HDMI_INFOFRAME_TYPE_DRM, NULL);
		return;
	}

	memcpy(body, hb, 3);
	memcpy(&body[3], pb, 28);
	hdmitx_infoframe_send(HDMI_INFOFRAME_TYPE_DRM, body);
}

/* for only 8bit */
void hdmi_gcppkt_manual_set(bool en)
{
	u8 body[31] = {0};

	body[0] = HDMI_PACKET_TYPE_GCP;
	if (en)
		hdmitx_infoframe_send(HDMI_PACKET_TYPE_GCP, body);
	else
		hdmitx_infoframe_send(HDMI_PACKET_TYPE_GCP, NULL);
}

