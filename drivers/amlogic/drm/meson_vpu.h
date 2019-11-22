/*
 * drivers/amlogic/drm/meson_vpu.h
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#ifndef __AM_MESON_VPU_H
#define __AM_MESON_VPU_H

#include <linux/amlogic/media/vout/vout_notify.h>

struct am_meson_vpu_data {
	u32 version;
};

struct am_vout_mode {
	char name[DRM_DISPLAY_MODE_LEN];
	enum vmode_e mode;
	int width, height, vrefresh;
	unsigned int flags;
};

extern struct am_meson_logo logo;
extern struct osd_device_data_s osd_meson_dev;
char *am_meson_crtc_get_voutmode(struct drm_display_mode *mode);
void am_meson_free_logo_memory(void);
bool am_meson_crtc_check_mode(struct drm_display_mode *mode, char *outputmode);

#endif /* __AM_MESON_VPU_H */
