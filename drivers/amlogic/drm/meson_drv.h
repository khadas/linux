/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __AM_MESON_DRV_H
#define __AM_MESON_DRV_H

#include <linux/platform_device.h>
#include <linux/kthread.h>
#include <linux/of.h>
#include <drm/drmP.h>
#ifdef CONFIG_DRM_MESON_USE_ION
#include <ion/ion_private.h>
#endif
/*CONFIG_DRM_MESON_EMULATE_FBDEV*/
#include <meson_fbdev.h>

#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>
#include <linux/amlogic/media/vfm/vfm_ext.h>

#define MESON_MAX_CRTC		2
#define MESON_MAX_OSD		4
#define MESON_MAX_VIDEO		2

/*
 * Amlogic drm private crtc funcs.
 * @loader_protect: protect loader logo crtc's power
 * @enable_vblank: enable crtc vblank irq.
 * @disable_vblank: disable crtc vblank irq.
 */
struct meson_crtc_funcs {
	int (*loader_protect)(struct drm_crtc *crtc, bool on);
	int (*enable_vblank)(struct drm_crtc *crtc);
	void (*disable_vblank)(struct drm_crtc *crtc);
};

struct meson_drm_thread {
	struct kthread_worker worker;
	struct drm_device *dev;
	struct task_struct *thread;
	unsigned int crtc_id;
};

struct meson_connector {
	struct drm_connector connector;
	struct meson_drm *drm_priv;
	void (*update)(struct drm_connector_state *new_state,
		struct drm_connector_state *old_state);
};

#define connector_to_meson_connector(x) container_of(x, struct meson_connector, connector)

struct meson_drm {
	struct device *dev;

	struct drm_device *drm;
	struct drm_crtc *crtc;
	const struct meson_crtc_funcs *crtc_funcs[MESON_MAX_CRTC];
	struct drm_plane *primary_plane;
	struct drm_plane *cursor_plane;
	struct drm_property_blob *gamma_lut_blob;

#ifdef CONFIG_DRM_MESON_USE_ION
	struct ion_client *gem_client;
#endif

	struct meson_vpu_pipeline *pipeline;
	struct meson_vpu_funcs *funcs;
	struct am_meson_logo *logo;
	struct drm_atomic_state *state;

	u32 num_crtcs;
	struct am_meson_crtc *crtcs[MESON_MAX_CRTC];
	struct meson_drm_thread commit_thread[MESON_MAX_CRTC];

	u32 num_planes;
	struct am_osd_plane *osd_planes[MESON_MAX_OSD];
	struct am_video_plane *video_planes[MESON_MAX_VIDEO];

	/*CONFIG_DRM_MESON_EMULATE_FBDEV*/
	struct meson_drm_fbdev_config ui_config;
	struct meson_drm_fbdev *osd_fbdevs[MESON_MAX_OSD];

	bool compat_mode;
};

static inline int meson_vpu_is_compatible(struct meson_drm *priv,
					  const char *compat)
{
	return of_device_is_compatible(priv->dev->of_node, compat);
}

int am_meson_register_crtc_funcs(struct drm_crtc *crtc,
				 const struct meson_crtc_funcs *crtc_funcs);
void am_meson_unregister_crtc_funcs(struct drm_crtc *crtc);
struct drm_connector *am_meson_hdmi_connector(void);

/*meson mode config atomic func*/
int meson_atomic_commit(struct drm_device *dev,
			     struct drm_atomic_state *state,
			     bool nonblock);
void meson_atomic_helper_commit_tail(struct drm_atomic_state *old_state);
/*******************************/

#ifdef CONFIG_DEBUG_FS
int meson_debugfs_init(struct drm_minor *minor);
#endif
int __am_meson_drm_set_config(struct drm_mode_set *set,
			      struct drm_atomic_state *state);
#endif /* __AM_MESON_DRV_H */
