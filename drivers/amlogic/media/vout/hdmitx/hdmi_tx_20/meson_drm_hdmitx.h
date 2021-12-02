/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef _MESON_DRM_HDMITX_H
#define _MESON_DRM_HDMITX_H
#include <linux/amlogic/media/vout/hdmi_tx/hdmi_tx_module.h>

int drm_hdmitx_register_hdcp_cb(struct drm_hdmitx_hdcp_cb *hdcp_cb);

void drm_hdmitx_hdcp22_init(void);

void drm_hdmitx_setup_attr(const char *buf);
void drm_hdmitx_get_attr(char attr[16]);
bool drm_hdmitx_chk_mode_attr_sup(char *mode, char *attr);

unsigned int drm_hdmitx_get_hdcp_cap(void);
unsigned int drm_get_rx_hdcp_cap(void);
int drm_hdmitx_hdcp_enable(unsigned int content_type);
int drm_hdmitx_hdcp_disable(unsigned int content_type);

int drm_get_hdcp_auth_sts(void);
void drm_hdmitx_avmute(unsigned char mute);
void drm_hdmitx_set_phy(unsigned char en);

#endif
