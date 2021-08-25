/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __AM_DRM_LCD_H
#define __AM_DRM_LCD_H

#include "meson_drv.h"

struct am_drm_lcd_s {
	struct meson_connector base;
	struct drm_encoder encoder;
	struct drm_panel panel;
	struct mipi_dsi_host dsi_host;
	struct drm_device *drm;
	struct aml_lcd_drv_s *lcd_drv;
	struct drm_display_mode *mode;
	struct display_timing *timing;
	u32 prev_vrefresh;
	int prev_active;
};

#define connector_to_am_lcd(x) \
		container_of(connector_to_meson_connector(x), struct am_drm_lcd_s, base)
#define encoder_to_am_lcd(x) container_of(x, struct am_drm_lcd_s, encoder)
#define drm_plane_to_am_hdmi(x) container_of(x, struct am_drm_lcd_s, panel)

#endif

