/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _MESON_DRM_HDMITX_H
#define _MESON_DRM_HDMITX_H
/*
 * The following interface is for amlogic DRM driver,
 * it is similar to hdmi_tx_main.c.
 * Currently it share most code of hdmi_tx_main.c.
 */

/*drm hpd*/
typedef void (*drm_hpd_cb)(void *data);

int drm_hdmitx_register_hpd_cb(drm_hpd_cb cb, void *data);
int drm_hdmitx_detect_hpd(void);

unsigned char *drm_hdmitx_get_raw_edid(void);
int drm_hdmitx_get_vic_list(int **vics);

int drm_hdmitx_get_hdcp_cap(void);
unsigned int drm_get_rx_hdcp_cap(void);
int drm_hdmitx_hdcp_enable(unsigned int content_type);
int drm_hdmitx_hdcp_disable(unsigned int content_type);
int drm_get_hdcp_auth_sts(void);
void drm_hdmitx_hdcp22_init(void);
void drm_hdmitx_avmute(unsigned char mute);
void drm_hdmitx_set_phy(unsigned char en);
void amhdmitx_setup_attr(const char *buf);
void amhdmitx_get_attr(char attr[16]);
bool drm_chk_mode_attr_sup(char *mode, char *attr);
bool drm_chk_hdmi_mode(const char *mode);

#endif
