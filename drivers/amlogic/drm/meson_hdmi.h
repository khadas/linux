/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __MESON_HDMI_H__
#define __MESON_HDMI_H__

#include "meson_drv.h"
#include <drm/drm_connector.h>
#include <drm/drm_encoder.h>
#include <linux/amlogic/media/vout/hdmi_tx/meson_drm_hdmitx.h>

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

enum {
	HDCP_STATE_START = 0,
	HDCP_STATE_SUCCESS,
	HDCP_STATE_FAIL,
	HDCP_STATE_STOP,
	HDCP_STATE_DISCONNECT,
};

struct am_hdmi_tx {
	struct meson_connector base;
	struct drm_encoder encoder;

	unsigned int input_color_format;
	unsigned int output_color_format;
	enum hdmi_color_depth color_depth;
	enum hdmi_color_space color_space;
	struct drm_property *color_depth_property;
	struct drm_property *color_space_property;

	/*drm request content type.*/
	int hdcp_request_content_type;
	int hdcp_request_content_protection;
	/*current hdcp running mode, HDCP_NULL means hdcp disabled.*/
	int hdcp_mode;
	/*hdcp auth result, HDCP_AUTH_UNKNOWN means havenot finished auth.*/
	int hdcp_state;

	/*TODO: android compatible, remove later*/
	bool android_path;

	/*amlogic property: force hdmitx update
	 *colorspace/colordepth from sysfs.
	 */
	struct drm_property *update_attr_prop;
};

struct am_hdmitx_connector_state {
	struct drm_connector_state base;
	bool update : 1;
};

#define to_am_hdmitx_connector_state(x)	container_of(x, struct am_hdmitx_connector_state, base)
#define meson_connector_to_am_hdmi(x)	container_of(x, struct am_hdmi_tx, base)
#define connector_to_am_hdmi(x) \
	container_of(connector_to_meson_connector(x), struct am_hdmi_tx, base)
#define encoder_to_am_hdmi(x)	container_of(x, struct am_hdmi_tx, encoder)
#endif
