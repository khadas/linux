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
struct drm_hdmitx_hpd_cb {
	void (*callback)(void *data);
	void *data;
};

int drm_hdmitx_register_hpd_cb(struct drm_hdmitx_hpd_cb *hpd_cb);

struct drm_hdmitx_hdcp_cb {
	void (*callback)(void *data, int auth);
	void *data;
};

int drm_hdmitx_register_hdcp_cb(struct drm_hdmitx_hdcp_cb *hdcp_cb);

int drm_hdmitx_detect_hpd(void);

unsigned char *drm_hdmitx_get_raw_edid(void);
int drm_hdmitx_get_vic_list(int **vics);

void drm_hdmitx_setup_attr(const char *buf);
void drm_hdmitx_get_attr(char attr[16]);
bool drm_hdmitx_chk_mode_attr_sup(char *mode, char *attr);

unsigned int drm_hdmitx_get_contenttypes(void);
int drm_hdmitx_set_contenttype(int content_type);

int drm_hdmitx_get_hdcp_cap(void);
unsigned int drm_get_rx_hdcp_cap(void);
int drm_hdmitx_hdcp_enable(unsigned int content_type);
int drm_hdmitx_hdcp_disable(unsigned int content_type);

int drm_get_hdcp_auth_sts(void);
void drm_hdmitx_hdcp22_init(void);
void drm_hdmitx_avmute(unsigned char mute);
void drm_hdmitx_set_phy(unsigned char en);

const struct dv_info *drm_hdmitx_get_dv_info(void);
const struct hdr_info *drm_hdmitx_get_hdr_info(void);

int drm_hdmitx_get_hdr_priority(void);

#endif
