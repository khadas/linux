/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __AM_MESON_VPU_H
#define __AM_MESON_VPU_H

#include <linux/amlogic/media/vout/vout_notify.h>


struct am_vout_mode {
	char name[DRM_DISPLAY_MODE_LEN];
	enum vmode_e mode;
	int width, height, vrefresh;
	unsigned int flags;
};

struct meson_vpu_crtc_func {
	void (*init_default_reg)(void);
};

struct meson_vpu_data {
	struct meson_vpu_crtc_func *crtc_func;
	struct meson_vpu_block_ops *osd_ops;
	struct meson_vpu_block_ops *afbc_ops;
	struct meson_vpu_block_ops *scaler_ops;
	struct meson_vpu_block_ops *osdblend_ops;
	struct meson_vpu_block_ops *hdr_ops;
	struct meson_vpu_block_ops *dv_ops;
	struct meson_vpu_block_ops *postblend_ops;
	struct meson_vpu_block_ops *video_ops;
};

extern struct am_meson_logo logo;
char *am_meson_crtc_get_voutmode(struct drm_display_mode *mode);
char *am_meson_crtc2_get_voutmode(struct drm_display_mode *mode);

void am_meson_free_logo_memory(void);
char *am_meson_hdmi_get_voutmode(struct drm_display_mode *mode);
#ifdef CONFIG_DRM_MESON_CVBS
char *am_cvbs_get_voutmode(struct drm_display_mode *mode);
#endif

#endif /* __AM_MESON_VPU_H */
