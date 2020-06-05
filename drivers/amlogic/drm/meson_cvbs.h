/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __MESON_CVBS_H_
#define __MESON_CVBS_H_

#include <drm/drm_connector.h>
#include <drm/drm_encoder.h>
#include <drm/drm_modes.h>

#include <linux/amlogic/media/vout/vinfo.h>

#include "meson_drv.h"

struct am_drm_cvbs_s {
	struct drm_device *drm;
	struct drm_connector connector;
	struct drm_encoder encoder;
};

#endif
