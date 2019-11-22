/*
 * drivers/amlogic/drm/meson_plane.c
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

#include "meson_plane.h"
#include "meson_crtc.h"
#include "meson_vpu.h"
#include "meson_drv.h"
#include "meson_vpu_pipeline.h"

static const u32 supported_drm_formats[] = {
	DRM_FORMAT_XRGB8888,
	DRM_FORMAT_XBGR8888,
	DRM_FORMAT_RGBX8888,
	DRM_FORMAT_BGRX8888,
	DRM_FORMAT_ARGB8888,
	DRM_FORMAT_ABGR8888,
	DRM_FORMAT_RGBA8888,
	DRM_FORMAT_BGRA8888,
	DRM_FORMAT_RGB888,
	DRM_FORMAT_RGB565,
	DRM_FORMAT_ARGB1555,
	DRM_FORMAT_ARGB4444,
};

static u64 afbc_wb_modifier[] = {
	DRM_FORMAT_MOD_MESON_AFBC_WB,
	DRM_FORMAT_MOD_LINEAR,
	DRM_FORMAT_MOD_INVALID
};

static void
meson_plane_position_calc(struct meson_vpu_osd_layer_info *plane_info,
			  struct drm_plane_state *state,
			  struct drm_display_mode *disp_mode)
{
	u32 dst_w, dst_h, src_w, src_h, scan_mode_out;
	struct drm_display_mode *mode;

	if (IS_ERR_OR_NULL(state->crtc))
		mode = disp_mode;
	else
		mode = &state->crtc->mode;

	scan_mode_out = mode->flags & DRM_MODE_FLAG_INTERLACE;
	plane_info->src_x = state->src_x;
	plane_info->src_y = state->src_y;
	plane_info->src_w = (state->src_w >> 16) & 0xffff;
	plane_info->src_h = (state->src_h >> 16) & 0xffff;

	plane_info->dst_x = state->crtc_x;
	plane_info->dst_y = state->crtc_y;
	plane_info->dst_w = state->crtc_w;
	plane_info->dst_h = state->crtc_h;
	if (scan_mode_out) {
		plane_info->dst_y >>= 1;
		plane_info->dst_h >>= 1;
	}
	/*negative position process*/
	if (state->crtc_x < 0) {
		dst_w = state->crtc_w + state->crtc_x;
		if (dst_w > 0) {
			src_w = plane_info->src_w * dst_w / state->crtc_w;
			plane_info->src_x = plane_info->src_w - src_w;
			plane_info->src_w = src_w;
			plane_info->dst_w = dst_w;
			plane_info->dst_x = 0;
		} else {
			plane_info->enable = 0;
		}
	}
	if (state->crtc_y < 0) {
		dst_h = state->crtc_h + state->crtc_y;
		if (dst_h > 0) {
			src_h = plane_info->src_h * dst_h / state->crtc_h;
			plane_info->src_y = plane_info->src_h - src_h;
			plane_info->src_h = src_h;
			plane_info->dst_h = dst_h;
			plane_info->dst_y = 0;
		} else {
			plane_info->enable = 0;
		}
	}
	/*overdisplay process*/
	if ((plane_info->dst_x + plane_info->dst_w) > mode->hdisplay) {
		if (plane_info->dst_x >= mode->hdisplay)
			plane_info->enable = 0;
		else
			plane_info->dst_w =
				mode->hdisplay - plane_info->dst_x;
	}
	if ((plane_info->dst_y + plane_info->dst_h) > mode->vdisplay) {
		if (plane_info->dst_y >= mode->vdisplay)
			plane_info->enable = 0;
		else
			plane_info->dst_h = mode->vdisplay - plane_info->dst_y;
	}
}

static int
meson_plane_check_size_range(struct meson_vpu_osd_layer_info *plane_info)
{
	u32 dst_w, dst_h, src_w, src_h, ratio_x, ratio_y;
	int ret;

	src_w = plane_info->src_w;
	src_h = plane_info->src_h;
	dst_w = plane_info->dst_w;
	dst_h = plane_info->dst_h;
	ratio_x = 0;
	ratio_y = 0;
	ret = 0;

	if (src_w > dst_w)
		ratio_x = (src_w + dst_w - 1) / dst_w;
	if (src_h > dst_h)
		ratio_y = (src_h + dst_h - 1) / dst_h;
	if (ratio_x > MESON_OSD_SCLAE_DOWN_LIMIT ||
	    ratio_y > MESON_OSD_SCLAE_DOWN_LIMIT)
		ret = -EDOM;
	if (src_w < dst_w)
		ratio_x = (dst_w + src_w - 1) / src_w;
	if (src_h < dst_h)
		ratio_y = (dst_h + src_h - 1) / src_h;
	if (ratio_x > MESON_OSD_SCLAE_UP_LIMIT ||
	    ratio_y > MESON_OSD_SCLAE_UP_LIMIT)
		ret = -EDOM;
	return ret;
}

static int meson_plane_fb_check(struct drm_plane *plane,
				struct drm_plane_state *new_state,
				struct meson_vpu_osd_layer_info *plane_info)
{
	struct drm_framebuffer *fb = new_state->fb;
	#ifdef CONFIG_DRM_MESON_USE_ION
	struct am_osd_plane *osd_plane = to_am_osd_plane(plane);
	struct meson_drm *drv = osd_plane->drv;
	struct am_meson_fb *meson_fb;
	#else
	struct drm_gem_cma_object *gem;
	#endif
	phys_addr_t phyaddr;

	#ifdef CONFIG_DRM_MESON_USE_ION
	meson_fb = container_of(fb, struct am_meson_fb, base);
	if (!meson_fb) {
		DRM_INFO("meson_fb is NULL!\n");
		return -EINVAL;
	}
	DRM_DEBUG("meson_fb[id:%d,ref:%d]=0x%p\n",
		  meson_fb->base.base.id,
		  atomic_read(&meson_fb->base.base.refcount.refcount),
		  meson_fb);
	if (meson_fb->logo && meson_fb->logo->alloc_flag &&
	    meson_fb->logo->start) {
		phyaddr = meson_fb->logo->start;
		DRM_DEBUG("logo->phyaddr=0x%pa\n", &phyaddr);
	} else  if (meson_fb->bufp) {
		phyaddr = am_meson_gem_object_get_phyaddr(drv, meson_fb->bufp);
	} else {
		phyaddr = 0;
		DRM_INFO("don't find phyaddr!\n");
		return -EINVAL;
	}
	#else
	if (!fb) {
		DRM_INFO("fb is NULL!\n");
		return -EINVAL;
	}
	/* Update Canvas with buffer address */
	gem = drm_fb_cma_get_gem_obj(fb, 0);
	if (!gem) {
		DRM_INFO("gem is NULL!\n");
		return -EINVAL;
	}
	phyaddr = gem->paddr;
	#endif
	plane_info->phy_addr = phyaddr;
	return 0;
}

static int meson_plane_get_fb_info(struct drm_plane *plane,
				   struct drm_plane_state *new_state,
				   struct meson_vpu_osd_layer_info *plane_info)
{
	struct am_osd_plane *osd_plane = to_am_osd_plane(plane);
	struct drm_framebuffer *fb = new_state->fb;
	struct meson_drm *drv = osd_plane->drv;

	if (!drv) {
		DRM_INFO("%s new_state/meson_drm is NULL!\n", __func__);
		return -EINVAL;
	}
	if (osd_plane->plane_index >= MESON_MAX_OSDS) {
		DRM_INFO("%s invalid plane_index!\n", __func__);
		return -EINVAL;
	}
	plane_info->pixel_format = fb->pixel_format;
	plane_info->byte_stride = fb->pitches[0];

	/*setup afbc info*/
	switch (fb->modifier) {
	case DRM_FORMAT_MOD_MESON_AFBC:
		plane_info->afbc_en = 1;
		plane_info->afbc_inter_format = AFBC_EN;
		break;
	case DRM_FORMAT_MOD_MESON_AFBC_WB:
		plane_info->afbc_en = 1;
		plane_info->afbc_inter_format = AFBC_EN |
			YUV_TRANSFORM | BLOCK_SPLIT |
			SUPER_BLOCK_ASPECT;
		break;
	case DRM_FORMAT_MOD_INVALID:
	case DRM_FORMAT_MOD_LINEAR:
	default:
		plane_info->afbc_en = 0;
		plane_info->afbc_inter_format = 0;
		break;
	};

	DRM_DEBUG("flags:%d pixel_format:%d,modifer=%llu\n",
				fb->flags, fb->pixel_format,
				fb->modifier);
	DRM_DEBUG("plane afbc_en=%u, afbc_inter_format=%x\n",
		plane_info->afbc_en, plane_info->afbc_inter_format);

	DRM_DEBUG("phy_addr=0x%x,byte_stride=%d,pixel_format=%d\n",
		plane_info->phy_addr, plane_info->byte_stride,
		plane_info->pixel_format);
	DRM_DEBUG("plane_index %d, size %d.\n", osd_plane->plane_index,
							plane_info->fb_size);
	return 0;
}

static int meson_plane_atomic_get_property(struct drm_plane *plane,
					const struct drm_plane_state *state,
					struct drm_property *property,
					uint64_t *val)
{
	struct am_osd_plane *osd_plane;
	struct am_meson_plane_state *plane_state;

	osd_plane = to_am_osd_plane(plane);
	plane_state = to_am_meson_plane_state(state);
	if (property == osd_plane->prop_premult_en)
		*val = plane_state->premult_en;
	return 0;
}

static int meson_plane_atomic_set_property(struct drm_plane *plane,
					 struct drm_plane_state *state,
					 struct drm_property *property,
					 uint64_t val)
{
	struct am_osd_plane *osd_plane;
	struct am_meson_plane_state *plane_state;

	osd_plane = to_am_osd_plane(plane);
	plane_state = to_am_meson_plane_state(state);
	if (property == osd_plane->prop_premult_en)
		plane_state->premult_en = val;

	return 0;
}

static struct drm_plane_state *
meson_plane_duplicate_state(struct drm_plane *plane)
{
	struct am_meson_plane_state *meson_plane_state, *old_plane_state;

	if (WARN_ON(!plane->state))
		return NULL;

	old_plane_state = to_am_meson_plane_state(plane->state);
	meson_plane_state = kmemdup(old_plane_state,
		sizeof(*meson_plane_state), GFP_KERNEL);
	if (!meson_plane_state)
		return NULL;

	__drm_atomic_helper_plane_duplicate_state(plane,
		&meson_plane_state->base);

	return &meson_plane_state->base;
}

static void meson_plane_destroy_state(struct drm_plane *plane,
					   struct drm_plane_state *state)
{
	struct am_meson_plane_state *amps;

	amps = to_am_meson_plane_state(state);
	__drm_atomic_helper_plane_destroy_state(state);
	kfree(amps);
}

bool am_meson_vpu_check_format_mod(struct drm_plane *plane,
				   u32 format, u64 modifier)
{
	bool ret = false;

	switch (modifier) {
	case DRM_FORMAT_MOD_LINEAR:
		ret = true;
		break;
	case DRM_FORMAT_MOD_MESON_AFBC:
		if (osd_meson_dev.afbc_type == MESON_AFBC &&
		    plane->type == DRM_PLANE_TYPE_PRIMARY)
			ret = true;
		break;
	case DRM_FORMAT_MOD_MESON_AFBC_WB:
		if (osd_meson_dev.afbc_type == MALI_AFBC &&
		    plane->type == DRM_PLANE_TYPE_PRIMARY) {
			if (format == DRM_FORMAT_BGR565)
				ret = false;
			else
				ret = true;
		}
		break;
	};

	DRM_DEBUG("modifier %llu return %d",
		  modifier, ret);
	return ret;
}

static const struct drm_plane_funcs am_osd_plane_funs = {
	.update_plane		= drm_atomic_helper_update_plane,
	.disable_plane		= drm_atomic_helper_disable_plane,
	.destroy		= drm_plane_cleanup,
	.reset			= drm_atomic_helper_plane_reset,
	.atomic_duplicate_state = meson_plane_duplicate_state,
	.atomic_destroy_state	= meson_plane_destroy_state,
	.atomic_set_property = meson_plane_atomic_set_property,
	.atomic_get_property = meson_plane_atomic_get_property,
	.format_mod_supported = am_meson_vpu_check_format_mod,
};

static int meson_plane_prepare_fb(struct drm_plane *plane,
				  struct drm_plane_state *new_state)
{
	return 0;
}

static void meson_plane_cleanup_fb(struct drm_plane *plane,
				   struct drm_plane_state *old_state)
{
	struct am_osd_plane *osd_plane = to_am_osd_plane(plane);

	DRM_DEBUG("osd %d.\n", osd_plane->plane_index);
}

static void meson_plane_atomic_update(struct drm_plane *plane,
				      struct drm_plane_state *old_state)
{

	DRM_DEBUG("plane atomic_update.\n");
}

static int meson_plane_atomic_check(struct drm_plane *plane,
		struct drm_plane_state *state)
{
	struct meson_vpu_osd_layer_info *plane_info;
	struct meson_vpu_pipeline_state *mvps;
	struct am_osd_plane *osd_plane = to_am_osd_plane(plane);
	struct meson_drm *drv = osd_plane->drv;
	struct am_meson_plane_state *plane_state;
	int ret;

	if (!state || !drv) {
		DRM_INFO("%s state/meson_drm is NULL!\n", __func__);
		return -EINVAL;
	}
	mvps = meson_vpu_pipeline_get_state(drv->pipeline, state->state);
	if (!mvps || osd_plane->plane_index >= MESON_MAX_OSDS) {
		DRM_INFO("%s mvps/osd_plane is NULL!\n", __func__);
		return -EINVAL;
	}
	plane_info = &mvps->plane_info[osd_plane->plane_index];
	plane_info->plane_index = osd_plane->plane_index;
	plane_info->zorder = state->zpos;

	mvps->plane_index[osd_plane->plane_index] = osd_plane->plane_index;
	meson_plane_position_calc(plane_info, state, &mvps->pipeline->mode);
	ret = meson_plane_check_size_range(plane_info);
	if (ret < 0) {
		plane_info->enable = 0;
		DRM_INFO("plane%d size check unsupport!!!\n",
			plane_info->plane_index);
		return ret;
	}
	ret = meson_plane_fb_check(plane, state, plane_info);
	if (ret < 0) {
		plane_info->enable = 0;
		DRM_DEBUG("plane%d fb is NULL,disable the plane!\n",
			plane_info->plane_index);
		return 0;
	}
	ret = meson_plane_get_fb_info(plane, state, plane_info);
	if (ret < 0 || plane_info->src_w > MESON_OSD_INPUT_W_LIMIT ||
	    plane_info->src_w == 0) {
		plane_info->enable = 0;
		return ret;
	}

	plane_state = to_am_meson_plane_state(state);
	plane_info->premult_en = plane_state->premult_en;
	plane_info->enable = 1;
	DRM_DEBUG("index=%d, zorder=%d\n",
		plane_info->plane_index, plane_info->zorder);
	DRM_DEBUG("src_x/y/w/h=%d/%d/%d/%d\n",
		plane_info->src_x, plane_info->src_y,
		plane_info->src_w, plane_info->src_h);
	DRM_DEBUG("dst_x/y/w/h=%d/%d/%d/%d\n",
		plane_info->dst_x, plane_info->dst_y,
		plane_info->dst_w, plane_info->dst_h);
	return 0;
}

static void meson_plane_atomic_disable(struct drm_plane *plane,
		struct drm_plane_state *old_state)
{
	struct am_osd_plane *osd_plane = to_am_osd_plane(plane);

	DRM_DEBUG("%s osd %d.\n", __func__, osd_plane->plane_index);
}

static const struct drm_plane_helper_funcs am_osd_helper_funcs = {
	.prepare_fb = meson_plane_prepare_fb,
	.cleanup_fb = meson_plane_cleanup_fb,
	.atomic_update	= meson_plane_atomic_update,
	.atomic_check	= meson_plane_atomic_check,
	.atomic_disable	= meson_plane_atomic_disable,
};

int drm_plane_create_premult_en_property(struct drm_plane *plane)
{
	struct drm_device *dev = plane->dev;
	struct drm_property *prop;
	struct am_osd_plane *osd_plane;

	osd_plane = to_am_osd_plane(plane);
	prop = drm_property_create_bool(dev, DRM_MODE_PROP_ATOMIC,
					"PREMULT_EN");
	if (!prop)
		return -ENOMEM;

	drm_object_attach_property(&plane->base, prop, 0);
	osd_plane->prop_premult_en = prop;

	return 0;
}

static struct am_osd_plane *am_plane_create(struct meson_drm *priv, int i)
{
	struct am_osd_plane *osd_plane;
	struct drm_plane *plane;
	u32 type = 0;
	char plane_name[8];
	const u64 *format_modifiers = afbc_wb_modifier;

	osd_plane = devm_kzalloc(priv->drm->dev, sizeof(*osd_plane),
				   GFP_KERNEL);
	if (!osd_plane)
		return 0;

	if (i == 0)
		type = DRM_PLANE_TYPE_PRIMARY;
	else
		type = DRM_PLANE_TYPE_OVERLAY;

	osd_plane->drv = priv;
	osd_plane->plane_index = i;

	plane = &osd_plane->base;
	sprintf(plane_name, "osd%d", i);

	drm_universal_plane_init(priv->drm, plane, 0xFF,
				 &am_osd_plane_funs,
				 supported_drm_formats,
				 ARRAY_SIZE(supported_drm_formats),
				 format_modifiers,
				 type, plane_name);

	drm_plane_create_premult_en_property(plane);
	drm_plane_helper_add(plane, &am_osd_helper_funcs);
	osd_drm_debugfs_add(&osd_plane->plane_debugfs_dir,
			    plane_name, osd_plane->plane_index);
	return osd_plane;
}

int am_meson_plane_create(struct meson_drm *priv)
{
	struct am_osd_plane *plane;
	struct meson_vpu_pipeline *pipeline = priv->pipeline;
	int i, osd_index;


	for (i = 0; i < pipeline->num_osds; i++) {
		osd_index = pipeline->osds[i]->base.index;
		plane = am_plane_create(priv, osd_index);

		if (!plane)
			return -ENOMEM;

		if (i == 0)
			priv->primary_plane = &plane->base;

		priv->planes[priv->num_planes++] = plane;
	}

	DRM_DEBUG("%s. enter\n", __func__);

	return 0;
}
