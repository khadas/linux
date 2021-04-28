/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __MESON_HDMI_H__
#define __MESON_HDMI_H__

#include "meson_drv.h"
#include <drm/drm_connector.h>
#include <drm/drm_encoder.h>

#define DDC_SEGMENT_ADDR   0x30
#define VIC_MAX_NUM        512
#define DRM_MODE_LEN_MAX	32
#define DRM_ATTR_LEN_MAX	16
#define DRM_HDMITX_VER     "20210428"
//Not sure the default value
#define MESON_DEFAULT_COLOR_DEPTH COLORDEPTH_24B
#define MESON_DEFAULT_COLOR_SPACE HDMI_COLORSPACE_RGB

#define COLOR_YCBCR444_12BIT             "444,12bit"
#define COLOR_YCBCR444_10BIT             "444,10bit"
#define COLOR_YCBCR444_8BIT              "444,8bit"
#define COLOR_YCBCR422_12BIT             "422,12bit"
#define COLOR_YCBCR422_10BIT             "422,10bit"
#define COLOR_YCBCR422_8BIT              "422,8bit"
#define COLOR_YCBCR420_12BIT             "420,12bit"
#define COLOR_YCBCR420_10BIT             "420,10bit"
#define COLOR_YCBCR420_8BIT              "420,8bit"
#define COLOR_RGB_12BIT                  "rgb,12bit"
#define COLOR_RGB_10BIT                  "rgb,10bit"
#define COLOR_RGB_8BIT                   "rgb,8bit"

#define MODE_4K2K24HZ                   "2160p24hz"
#define MODE_4K2K25HZ                   "2160p25hz"
#define MODE_4K2K30HZ                   "2160p30hz"
#define MODE_4K2K50HZ                   "2160p50hz"
#define MODE_4K2K60HZ                   "2160p60hz"
#define MODE_4K2KSMPTE                  "smpte24hz"
#define MODE_4K2KSMPTE30HZ              "smpte30hz"
#define MODE_4K2KSMPTE50HZ              "smpte50hz"
#define MODE_4K2KSMPTE60HZ              "smpte60hz"

struct am_hdmi_data {
	unsigned int vic;
	u8 sink_is_hdmi;
	u8 sink_has_audio;
	unsigned int colorimetry;
	unsigned int  cd; /* cd8, cd10 or cd12 */
	unsigned int  cs; /* rgb, y444, y422, y420 */
	unsigned int  cr; /* limit, full */
	struct hdmi_pwr_ctl *pwr_ctl;
	unsigned int aud_output_ch;
	unsigned int tx_aud_cfg; /* 0, off; 1, on */
	unsigned int tmds_clk_div40;
	unsigned int VIC[VIC_MAX_NUM];
};

struct am_hdmi_tx {
	struct device *dev;
	struct drm_encoder encoder;
	struct drm_connector connector;
	struct meson_drm *priv;
	int irq;
	unsigned int input_color_format;
	unsigned int output_color_format;
	enum hdmi_color_depth color_depth;
	enum hdmi_color_space color_space;
	struct drm_property *color_depth_property;
	struct drm_property *color_space_property;
	struct drm_display_mode previous_mode;
	struct am_hdmi_data hdmi_info;

	unsigned int hpd_flag;/*0:none   1:up    2:down*/
	unsigned int hdcp_tx_type;/*bit0:hdcp14 bit 1:hdcp22*/
	unsigned int hdcp_downstream_type;/*bit0:hdcp14 bit 1:hdcp22*/
	unsigned int hdcp_user_type;/*0: null hdcp 1: hdcp14 2: hdcp22*/
	unsigned int hdcp_execute_type;/*0: null hdcp 1: hdcp14 2: hdcp22*/

	struct miscdevice hdcp_comm_device;
	wait_queue_head_t hdcp_comm_queue;
	struct drm_property *update_attr_prop;
	int hdcp_result;
	int hdcp_report;
	int hdcp_poll_report;
	unsigned int hdcp_en;
	unsigned int hdcp_ctl_lvl;
};

struct am_hdmitx_connector_state {
	struct drm_connector_state base;
	bool update : 1;
};

#define to_am_hdmitx_connector_state(x)	container_of(x, struct am_hdmitx_connector_state, base)
#define connector_to_am_hdmi(x)	container_of(x, struct am_hdmi_tx, connector)
#define encoder_to_am_hdmi(x)	container_of(x, struct am_hdmi_tx, encoder)
#endif
