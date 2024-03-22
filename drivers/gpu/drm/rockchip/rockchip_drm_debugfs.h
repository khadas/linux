/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Rockchip Electronics Co., Ltd.
 * Author: Sandy Huang <hjc@rock-chips.com>
 */

#ifndef ROCKCHIP_DRM_DEBUGFS_H
#define ROCKCHIP_DRM_DEBUGFS_H

struct vop_dump_info;

/**
 * struct vop_dump_list - store all buffer info per frame
 *
 * one frame maybe multiple buffer, all will be stored here.
 *
 */
struct vop_dump_list {
	struct list_head entry;
};

/**
 * @DUMP_DISABLE: Disable dump and do not record plane info into list.
 * @DUMP_ENABLE: Record plane info into list.
 * @DUMP_KEEP: Record plane info into list and keep to dump plane.
 */
enum vop_dump_status {
	DUMP_DISABLE = 0,
	DUMP_ENABLE,
	DUMP_KEEP
};

#if defined(CONFIG_ROCKCHIP_DRM_DEBUG)
int rockchip_drm_add_dump_buffer(struct drm_crtc *crtc, struct dentry *root);
int rockchip_drm_crtc_dump_plane_buffer(struct drm_crtc *crtc);
int rockchip_drm_debugfs_add_color_bar(struct drm_crtc *crtc, struct dentry *root);
#else
static inline int
rockchip_drm_add_dump_buffer(struct drm_crtc *crtc, struct dentry *root)
{
	return 0;
}

static inline int
rockchip_drm_crtc_dump_plane_buffer(struct drm_crtc *crtc)
{
	return 0;
}

static inline int
rockchip_drm_debugfs_add_color_bar(struct drm_crtc *crtc, struct dentry *root)
{
	return 0;
}
#endif

#endif
