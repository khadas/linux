/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __HDMI_TX_H__
#define __HDMI_TX_H__
#include <linux/hdmi.h>
#include <linux/amlogic/media/vout/hdmi_tx21/hdmi_tx_module.h>

int hdcpksv_valid(u8 *dat);
int hdmitx21_hdcp_init(void);
int hdmitx21_uboot_audio_en(void);

int hdmitx21_init_reg_map(struct platform_device *pdev);
void hdmitx21_set_audioclk(bool en);
void hdmitx21_disable_clk(struct hdmitx_dev *hdev);
u32 hdcp21_rd_hdcp22_ver(void);
struct hdmi_timing *hdmitx21_gettiming_from_vic(enum hdmi_vic vic);
struct hdmi_timing *hdmitx21_gettiming_from_name(const char *mode);
void hdmitx_infoframe_send(u8 info_type, u8 *body);

/* there are 2 ways to send out infoframes
 * xxx_infoframe_set() will take use of struct xxx_infoframe_set
 * xxx_infoframe_rawset() will directly send with rawdata
 * if info, hb, or pb == NULL, disable send infoframe
 */
void hdmi_vend_infoframe_set(struct hdmi_vendor_infoframe *info);
void hdmi_vend_infoframe_rawset(u8 *hb, u8 *pb);
void hdmi_avi_infoframe_set(struct hdmi_avi_infoframe *info);
void hdmi_avi_infoframe_rawset(u8 *hb, u8 *pb);
void hdmi_spd_infoframe_set(struct hdmi_spd_infoframe *info);
void hdmi_audio_infoframe_set(struct hdmi_audio_infoframe *info);
void hdmi_audio_infoframe_rawset(u8 *hb, u8 *pb);
void hdmi_drm_infoframe_set(struct hdmi_drm_infoframe *info);
void hdmi_drm_infoframe_rawset(u8 *hb, u8 *pb);

enum avi_component_conf {
	CONF_AVI_CS,
	CONF_AVI_BT2020,
	CONF_AVI_Q01,
	CONF_AVI_YQ01,
};

/* CONF_AVI_BT2020 */
#define CLR_AVI_BT2020	0x0
#define SET_AVI_BT2020	0x1
/* CONF_AVI_Q01 */
#define RGB_RANGE_DEFAULT	0
#define RGB_RANGE_LIM		1
#define RGB_RANGE_FUL		2
#define RGB_RANGE_RSVD		3
/* CONF_AVI_YQ01 */
#define YCC_RANGE_LIM		0
#define YCC_RANGE_FUL		1
#define YCC_RANGE_RSVD		2
void hdmi_avi_infoframe_config(enum avi_component_conf conf, u8 val);

int hdmitx_infoframe_rawget(u8 info_type, u8 *body);

#endif
