/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef MESON_DRM_BIND_H
#define MESON_DRM_BIND_H
#include <drm/amlogic/meson_connector_dev.h>

struct meson_drm_bound_data {
	struct drm_device *drm;

	/*bind to drm and return corresponding object id.*/
	int (*connector_component_bind)(struct drm_device *drm,
		int type, struct meson_connector_dev *intf);
	/*unbind from drm and return operation result.*/
	int (*connector_component_unbind)(struct drm_device *drm,
		int type, int connector_id);
};

#endif
