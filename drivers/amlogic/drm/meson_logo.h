/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __AM_MESON_LOGO_H
#define __AM_MESON_LOGO_H
#include <stdarg.h>

#define VMODE_NAME_LEN_MAX    64

#define VPP0     0
#define VPP1     1
#define VPP2     2

struct am_meson_logo {
	struct page *logo_page;
	phys_addr_t start;
	int panel_index;
	int vpp_index;
	u32 size;
	u32 width;
	u32 height;
	u32 bpp;
	u32 alloc_flag;
	u32 info_loaded_mask;
	u32 osd_reverse;
	u32 vmode;
	u32 debug;
	u32 loaded;
	char *outputmode_t;
	char outputmode[VMODE_NAME_LEN_MAX];
};

void am_meson_logo_init(struct drm_device *dev);
void am_meson_free_logo_memory(void);

#endif

