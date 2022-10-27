// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <drm/amlogic/meson_drm_bind.h>
#include "meson_hdmi.h"
#include "meson_cvbs.h"
#include "meson_lcd.h"

int meson_connector_dev_bind(struct drm_device *drm,
	int type, struct meson_connector_dev *intf)
{
	/*amlogic extend lcd*/
	if (type > DRM_MODE_MESON_CONNECTOR_PANEL_START &&
		type < DRM_MODE_MESON_CONNECTOR_PANEL_END) {
		return meson_panel_dev_bind(drm, type, intf);
	}

	switch (type) {
	case DRM_MODE_CONNECTOR_HDMIA:
	case DRM_MODE_CONNECTOR_HDMIB:
		return meson_hdmitx_dev_bind(drm, type, intf);

	case DRM_MODE_CONNECTOR_TV:
		return meson_cvbs_dev_bind(drm, type, intf);

	case DRM_MODE_CONNECTOR_LVDS:
	case DRM_MODE_CONNECTOR_DSI:
	case DRM_MODE_CONNECTOR_eDP:
		return meson_panel_dev_bind(drm, type, intf);

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
	/*amlogic extend lcd*/
	if (type > DRM_MODE_MESON_CONNECTOR_PANEL_START &&
		type < DRM_MODE_MESON_CONNECTOR_PANEL_END) {
		return meson_panel_dev_unbind(drm, type, connector_id);
	}

	switch (type) {
	case DRM_MODE_CONNECTOR_HDMIA:
	case DRM_MODE_CONNECTOR_HDMIB:
		return meson_hdmitx_dev_unbind(drm, type, connector_id);

	case DRM_MODE_CONNECTOR_TV:
		return meson_cvbs_dev_unbind(drm, type, connector_id);

	case DRM_MODE_CONNECTOR_LVDS:
	case DRM_MODE_CONNECTOR_DSI:
	case DRM_MODE_CONNECTOR_eDP:
		return meson_panel_dev_unbind(drm, type, connector_id);

	default:
		pr_err("unknown connector tye %d\n", type);
		return 0;
	};
}
EXPORT_SYMBOL(meson_connector_dev_unbind);

