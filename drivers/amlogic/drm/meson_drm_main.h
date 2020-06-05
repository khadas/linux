/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _MESON_DRM_MAIN_H__
#define _MESON_DRM_MAIN_H__

#ifdef CONFIG_DRM_MESON_CVBS
int am_meson_cvbs_init(void);
#else
static inline int am_meson_cvbs_init(void)
{
	return 0;
}
#endif

#ifdef CONFIG_DRM_MESON_HDMI
int am_meson_hdmi_init(void);
#else
static inline int am_meson_hdmi_init(void)
{
	return 0;
}
#endif

#ifdef CONFIG_DRM_MESON_PANEL
int am_meson_lcd_init(void);
#else
static inline int am_meson_lcd_init(void)
{
	return 0;
}
#endif

#ifdef CONFIG_DRM_MESON_VPU
int am_meson_vpu_init(void);
#else
static inline int am_meson_vpu_init(void)
{
	return 0;
}
#endif

#ifdef CONFIG_AMLOGIC_DRM
int am_meson_drm_init(void);
#else
static inline int am_meson_drm_init(void)
{
	return 0;
}
#endif

#endif
