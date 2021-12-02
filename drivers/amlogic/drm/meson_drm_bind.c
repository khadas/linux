// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <drm/amlogic/meson_drm_bind.h>
#include "meson_hdmi.h"
#include "meson_cvbs.h"

int meson_connector_dev_bind(struct drm_device *drm,
	int type, struct meson_connector_dev *intf)
{
	switch (type) {
	case DRM_MODE_CONNECTOR_HDMIA:
	case DRM_MODE_CONNECTOR_HDMIB:
		return meson_hdmitx_dev_bind(drm, type, intf);

#ifdef CONFIG_DRM_MESON_CVBS
	case DRM_MODE_CONNECTOR_TV:
		return meson_cvbs_dev_bind(drm, type, intf);
#endif

	default:
		pr_err("unknown connector tye %d\n", type);
		return 0;
	};

	return 0;
}
EXPORT_SYMBOL(meson_connector_dev_bind);

int meson_connector_dev_unbind(struct drm_device *drm,
	int type, int connector_id)
{
	switch (type) {
	case DRM_MODE_CONNECTOR_HDMIA:
	case DRM_MODE_CONNECTOR_HDMIB:
		return meson_hdmitx_dev_unbind(drm, type, connector_id);

#ifdef CONFIG_DRM_MESON_CVBS
	case DRM_MODE_CONNECTOR_TV:
		return meson_cvbs_dev_unbind(drm, type, connector_id);
#endif

	default:
		pr_err("unknown connector tye %d\n", type);
		return 0;
	};
}
EXPORT_SYMBOL(meson_connector_dev_unbind);

