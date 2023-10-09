/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __MESON_CRTC_H
#define __MESON_CRTC_H

#include <linux/kernel.h>
#include <linux/mutex.h>
#include <drm/drmP.h>
#include <drm/drm_plane.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include "meson_vpu.h"
#include "meson_drv.h"
#include "meson_fb.h"
#include "meson_plane.h"

enum {
	MESON_HDR_POLICY_FOLLOW_SINK = 0,
	MESON_HDR_POLICY_FOLLOW_SOURCE,
};

enum {
	HDMI_EOTF_MESON_DOLBYVISION = (HDMI_EOTF_BT_2100_HLG + 0xf),
	HDMI_EOTF_MESON_DOLBYVISION_LL,
};

struct am_meson_crtc_present_fence {
	u32 fd;
	struct dma_fence *fence;
	struct sync_file *sync_file;
	/* lock to protect process context and interrupt context */
	spinlock_t lock;
};

struct am_meson_crtc_state {
	struct drm_crtc_state base;

	/*vout mode indicate connector type*/
	enum vmode_e vmode;
	enum vmode_e preset_vmode;

	int uboot_mode_init;
	/*policy update by y property*/
	u8 crtc_hdr_process_policy; /*follow sink or follow source*/
	/*only used to indicate if dv ll mode output now.*/
	u8 crtc_eotf_type;
	/*dv core enabled, control by userspace not driver*/
	bool crtc_dv_enable;
	/* dv mode: 0 dobly std 1:dobly ll*/
	bool dv_mode;
	/*hdr core enabled, always on if soc support hdr.*/
	bool crtc_hdr_enable;
	/*eotf policy update by property*/
	bool crtc_eotf_by_property_flag;
	/*eotf value by property*/
	u8 eotf_type_by_property;
	/*crtc background*/
	u64 crtc_bgcolor;
	/*basic refresh rate*/
	u32 brr;
	u32 valid_brr;
	/*brr mode string*/
	char brr_mode[DRM_DISPLAY_MODE_LEN];
};

struct am_meson_crtc {
	struct drm_crtc base;
	struct device *dev;
	struct drm_device *drm_dev;
	struct meson_drm *priv;

	unsigned int irq;
	int crtc_index;
	int vout_index;
	struct drm_pending_vblank_event *event;
	struct meson_vpu_pipeline *pipeline;

	struct drm_property *hdr_policy;
	struct drm_property *hdmi_eotf;
	struct drm_property *dv_enable_property;
	struct drm_property *dv_mode_property;
	struct drm_property *bgcolor_property;

	/*debug*/
	int dump_enable;
	int blank_enable;
	int dump_counts;
	int dump_index;
	char osddump_path[64];

	/*present fence*/
	struct am_meson_crtc_present_fence present_fence;

	/*commit*/
	struct mutex commit_mutex;
	atomic_t commit_num;

	int vpp_crc_enable;
	/*forced to detect crc several times after bootup.*/
	int force_crc_chk;
	/*funcs*/
	int (*get_scanout_position)(struct am_meson_crtc *crtc,
		bool in_vblank_irq, int *vpos, int *hpos,
		ktime_t *stime, ktime_t *etime,
		const struct drm_display_mode *mode);
};

#define to_am_meson_crtc(x) container_of(x, \
		struct am_meson_crtc, base)
#define to_am_meson_crtc_state(x) container_of(x, \
		struct am_meson_crtc_state, base)

struct am_meson_crtc *meson_crtc_bind(struct meson_drm *priv,
	int idx);
int meson_crtc_creat_present_fence_ioctl(struct drm_device *dev,
			void *data, struct drm_file *file_priv);

#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
void set_amdv_policy(int policy);
int get_amdv_policy(void);
void set_amdv_ll_policy(int policy);
int get_amdv_ll_policy(void);
void set_amdv_enable(bool enable);
int get_dv_support_info(void);
bool is_amdv_enable(void);
void set_amdv_mode(int mode);
int get_amdv_mode(void);
#endif
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_VECM
void set_hdr_policy(int policy);
int get_hdr_policy(void);
#endif
bool dv_support(void);
#endif
