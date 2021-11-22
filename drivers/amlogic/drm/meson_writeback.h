/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __MESON_WRITEBACK_H__
#define __MESON_WRITEBACK_H__

#include "meson_drv.h"
#include <drm/drm_connector.h>
#include <drm/drm_encoder.h>
#include <drm/drm_writeback.h>

struct am_drm_writeback {
	struct meson_connector base;
	struct drm_writeback_connector wb_connector;
	struct drm_device drm_dev;
	struct drm_framebuffer *fb;
	struct workqueue_struct *writeback_wq;
	struct work_struct writeback_work;

	/*capture port prop*/
	struct drm_property *capture_port_prop;
	u32 capture_port;
};

struct am_drm_writeback_state {
	struct drm_connector_state base;
	u32 capture_port;
};

#define to_am_writeback_state(x)   \
	container_of(x, struct am_drm_writeback_state, base)
#define connector_to_wb_connector(x)   \
	container_of((x), struct drm_writeback_connector, base)
#define connector_to_am_writeback(x)   \
	container_of(connector_to_wb_connector(x), \
		struct am_drm_writeback, wb_connector)

int am_meson_writeback_create(struct drm_device *drm);

#endif
