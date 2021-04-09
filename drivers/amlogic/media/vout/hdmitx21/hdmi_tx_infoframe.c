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
	default:
		break;
	}
	hdmi_avi_infoframe_set(info);
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
