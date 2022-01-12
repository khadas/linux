/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __MESON_CRTC_H
#define __MESON_CRTC_H

#include <linux/kernel.h>
#include <drm/drmP.h>
#include <drm/drm_plane.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include "meson_vpu.h"
#include "meson_drv.h"
#include "meson_fb.h"

enum {
	MESON_HDR_POLICY_FOLLOW_SINK = 0,
	MESON_HDR_POLICY_FOLLOW_SOURCE,
};

enum {
	HDMI_EOTF_MESON_DOLBYVISION = (HDMI_EOTF_BT_2100_HLG + 0xf),
	HDMI_EOTF_MESON_DOLBYVISION_LL,
};

struct am_meson_crtc_state {
	struct drm_crtc_state base;

	int uboot_mode_init;
	/*policy update by y property*/
	u8 crtc_hdr_process_policy; /*follow sink or follow source*/
	/*only used to indicate if dv ll mode output now.*/
	u8 crtc_eotf_type;
	/*dv core enabled, control by usersapce not driver*/
	bool crtc_dv_enable;
	/*hdr core enabled, always on if soc support hdr.*/
	bool crtc_hdr_enable;
	/*etof policy update by property*/
	bool crtc_eotf_by_property_flag;
	/*etof value by property*/
	u8 eotf_type_by_property;
};

struct am_meson_crtc {
	struct drm_crtc base;
	struct device *dev;
	struct drm_device *drm_dev;
	struct meson_drm *priv;

	unsigned int irq;
	int crtc_index;
	struct drm_pending_vblank_event *event;
	struct meson_vpu_pipeline *pipeline;

	struct drm_property *hdr_policy;
	struct drm_property *hdmi_etof;

    /*debug*/
	int dump_enable;
	int blank_enable;
	int dump_counts;
	int dump_index;
	char osddump_path[64];

	int vpp_crc_enable;

	/*funcs*/
	int (*get_scannout_position)(struct am_meson_crtc *crtc,
		bool in_vblank_irq, int *vpos, int *hpos,
		ktime_t *stime, ktime_t *etime,
		const struct drm_display_mode *mode);
};

#define to_am_meson_crtc(x) container_of(x, \
		struct am_meson_crtc, base)
#define to_am_meson_crtc_state(x) container_of(x, \
		struct am_meson_crtc_state, base)

int am_meson_crtc_create(struct am_meson_crtc *amcrtc);
int am_meson_crtcs_add(struct meson_drm *private, struct device *dev);

#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
void set_dolby_vision_policy(int policy);
int get_dolby_vision_policy(void);
void set_dolby_vision_ll_policy(int policy);
void set_dolby_vision_enable(bool enable);
int get_dv_support_info(void);
bool is_dolby_vision_enable(void);
void set_dolby_vision_mode(int mode);
int get_dolby_vision_mode(void);
#endif
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_VECM
void set_hdr_policy(int policy);
int get_hdr_policy(void);
#endif
bool dv_support(void);
#endif
