/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __MESON_PANEL_H
#define __MESON_PANEL_H

#include "meson_drv.h"
#include <drm/drm_encoder.h>
#include <drm/amlogic/meson_connector_dev.h>

struct meson_panel {
	struct meson_connector base;
	struct drm_encoder encoder;
	struct drm_device *drm;

	struct meson_panel_dev *panel_dev;
	struct drm_property *panel_type_prop;
	int panel_type;
};

#define connector_to_meson_panel(x) \
		container_of(connector_to_meson_connector(x), struct meson_panel, base)
#define encoder_to_meson_panel(x) container_of(x, struct meson_panel, encoder)
#define drm_panel_to_meson_panel(x) container_of(x, struct meson_panel, panel)

int meson_panel_dev_bind(struct drm_device *drm,
	int type, struct meson_connector_dev *intf);
int meson_panel_dev_unbind(struct drm_device *drm,
	int type, int connector_id);

#endif

