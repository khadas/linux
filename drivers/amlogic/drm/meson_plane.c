// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/types.h>
#include <linux/amlogic/media/osd/osd_logo.h>

#include "meson_plane.h"
#include "meson_crtc.h"
#include "meson_vpu.h"
#include "meson_drv.h"
#include "meson_vpu_pipeline.h"
#include "meson_osd_afbc.h"
#include "meson_gem.h"
#include "meson_logo.h"


static u64 afbc_modifier[] = {
	/*
	 * - TOFIX Support AFBC modifiers for YUV formats (16x16 + TILED)
	 * - SPLIT is mandatory for performances reasons when in 16x16
	 *   block size
	 * - 32x8 block size + SPLIT is mandatory with 4K frame size
	 *   for performances reasons
	 */
	DRM_FORMAT_MOD_ARM_AFBC(AFBC_FORMAT_MOD_BLOCK_SIZE_16x16 |
				AFBC_FORMAT_MOD_SPARSE |
				AFBC_FORMAT_MOD_SPLIT),
	DRM_FORMAT_MOD_ARM_AFBC(AFBC_FORMAT_MOD_BLOCK_SIZE_16x16 |
				AFBC_FORMAT_MOD_YTR |
				AFBC_FORMAT_MOD_SPARSE |
				AFBC_FORMAT_MOD_SPLIT),
	DRM_FORMAT_MOD_ARM_AFBC(AFBC_FORMAT_MOD_BLOCK_SIZE_16x16 |
				AFBC_FORMAT_MOD_TILED |
				AFBC_FORMAT_MOD_SC |
				AFBC_FORMAT_MOD_YTR |
				AFBC_FORMAT_MOD_SPARSE |
				AFBC_FORMAT_MOD_SPLIT),
	DRM_FORMAT_MOD_ARM_AFBC(AFBC_FORMAT_MOD_BLOCK_SIZE_32x8 |
				AFBC_FORMAT_MOD_SPARSE |
				AFBC_FORMAT_MOD_SPLIT),
	DRM_FORMAT_MOD_ARM_AFBC(AFBC_FORMAT_MOD_BLOCK_SIZE_32x8 |
				AFBC_FORMAT_MOD_YTR |
				AFBC_FORMAT_MOD_SPARSE |
				AFBC_FORMAT_MOD_SPLIT),
	DRM_FORMAT_MOD_ARM_AFBC(AFBC_FORMAT_MOD_BLOCK_SIZE_32x8 |
				AFBC_FORMAT_MOD_TILED |
				AFBC_FORMAT_MOD_SC |
				AFBC_FORMAT_MOD_YTR |
				AFBC_FORMAT_MOD_SPARSE |
				AFBC_FORMAT_MOD_SPLIT),
	DRM_FORMAT_MOD_LINEAR,
	DRM_FORMAT_MOD_INVALID
};

static const u32 supported_drm_formats[] = {
	DRM_FORMAT_ABGR2101010,
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
};

static u64 video_fbc_modifier[] = {
	DRM_FORMAT_MOD_AMLOGIC_FBC(AMLOGIC_FBC_LAYOUT_BASIC, 0),
	DRM_FORMAT_MOD_AMLOGIC_FBC(AMLOGIC_FBC_LAYOUT_BASIC,
		AMLOGIC_FBC_OPTION_MEM_SAVING),
	DRM_FORMAT_MOD_AMLOGIC_FBC(AMLOGIC_FBC_LAYOUT_SCATTER, 0),
	DRM_FORMAT_MOD_AMLOGIC_FBC(AMLOGIC_FBC_LAYOUT_SCATTER,
		AMLOGIC_FBC_OPTION_MEM_SAVING),
	DRM_FORMAT_MOD_LINEAR,
	DRM_FORMAT_MOD_INVALID
};

static const u32 video_supported_drm_formats[] = {
	DRM_FORMAT_NV12,
	DRM_FORMAT_NV21,
	DRM_FORMAT_UYVY,
	DRM_FORMAT_VUY888,
};

static void
meson_plane_position_calc(struct meson_vpu_osd_layer_info *plane_info,
			  struct drm_plane_state *state,
			  struct meson_vpu_pipeline *pipeline)
{
	u32 dst_w, dst_h, src_w, src_h, scan_mode_out;
	struct am_osd_plane *amp;
	struct drm_atomic_state *atomic_state = state->state;
	struct drm_crtc *crtc = state->crtc;
	struct drm_crtc_state *crtc_state;
	struct drm_display_mode *mode;

	if (crtc) {
		crtc_state = drm_atomic_get_crtc_state(atomic_state, crtc);
		mode = &crtc_state->mode;
	} else {
		DRM_DEBUG("Disabling plane %d, so skip postion calc",
			 plane_info->plane_index);
		return;
	}

	if (!crtc_state || !mode)
		mode = &pipeline->subs[crtc->index].mode;
	scan_mode_out = mode->flags & DRM_MODE_FLAG_INTERLACE;
	plane_info->src_x = state->src_x >> 16;
	plane_info->src_y = state->src_y >> 16;
	plane_info->src_w = (state->src_w >> 16) & 0xffff;
	plane_info->src_h = (state->src_h >> 16) & 0xffff;
	DRM_DEBUG("original source: src_x=%d, src_y=%d, src_w=%d, src_h=%d\n",
		plane_info->src_x, plane_info->src_y, plane_info->src_w, plane_info->src_h);

	plane_info->dst_x = state->crtc_x;
	plane_info->dst_y = state->crtc_y;
	plane_info->dst_w = state->crtc_w;
	plane_info->dst_h = state->crtc_h;
	plane_info->rotation = state->rotation;
	DRM_DEBUG("original destination: dst_x=%d, dst_y=%d, dst_w=%d, dst_h=%d\n",
		plane_info->dst_x, plane_info->dst_y, plane_info->dst_w, plane_info->dst_h);
	if (state->plane) {
		amp = to_am_osd_plane(state->plane);
		if (plane_info->rotation != amp->osd_reverse)
			plane_info->rotation = amp->osd_reverse;
		if (plane_info->blend_bypass != amp->osd_blend_bypass)
			plane_info->blend_bypass = amp->osd_blend_bypass;
		if (plane_info->read_ports != amp->osd_read_ports)
			plane_info->read_ports = amp->osd_read_ports;
		else
			plane_info->read_ports = 2;
	}
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
		DRM_DEBUG("state->crtc_x < 0\n");
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
		DRM_DEBUG("state->crtc_y < 0\n");
	}
	/*overdisplay process*/
	if ((plane_info->dst_x + plane_info->dst_w) > mode->hdisplay) {
		if (plane_info->dst_x >= mode->hdisplay) {
			plane_info->enable = 0;
		} else {
			dst_w = plane_info->dst_w;
			src_w = plane_info->src_w;
			plane_info->dst_w =
				mode->hdisplay - plane_info->dst_x;
			plane_info->src_w =
				(src_w * plane_info->dst_w) / dst_w;
		}
		DRM_DEBUG("(plane_info->dst_x + plane_info->dst_w) > mode->hdisplay\n");
	}
	if ((plane_info->dst_y + plane_info->dst_h) > mode->vdisplay) {
		if (plane_info->dst_y >= mode->vdisplay) {
			plane_info->enable = 0;
		} else {
			dst_h = plane_info->dst_h;
			src_h = plane_info->src_h;
			plane_info->dst_h =
				mode->vdisplay - plane_info->dst_y;
			plane_info->src_h =
				(src_h * plane_info->dst_h) / dst_h;
		}
		DRM_DEBUG("(plane_info->dst_y + plane_info->dst_h) > mode->vdisplay\n");
	}
	/*reverse process*/
	if (plane_info->rotation & DRM_MODE_REFLECT_X)
		plane_info->dst_x = mode->hdisplay - plane_info->dst_w -
			plane_info->dst_x;
	if (plane_info->rotation & DRM_MODE_REFLECT_Y)
		plane_info->dst_y = mode->vdisplay - plane_info->dst_h -
			plane_info->dst_y;
	DRM_DEBUG("fini source: src_x=%d, src_y=%d, src_w=%d, src_h=%d\n",
		plane_info->src_x, plane_info->src_y,
		plane_info->src_w, plane_info->src_h);
	DRM_DEBUG("fini destination: dst_x=%d, dst_y=%d, dst_w=%d, dst_h=%d\n",
		plane_info->dst_x, plane_info->dst_y,
		plane_info->dst_w, plane_info->dst_h);

}

static void
meson_video_plane_position_calc(struct meson_vpu_video_layer_info *plane_info,
				struct drm_plane_state *state,
				struct meson_vpu_pipeline *pipeline)
{
	u32 dst_w, dst_h, src_w, src_h;
	struct drm_atomic_state *atomic_state = state->state;
	struct drm_crtc *crtc = state->crtc;
	struct drm_crtc_state *crtc_state;
	struct drm_display_mode *mode;

	if (crtc) {
		crtc_state = drm_atomic_get_crtc_state(atomic_state, crtc);
		mode = &crtc_state->mode;
	} else {
		DRM_DEBUG("Disabling video plane %d, so skip postion calc",
			 plane_info->plane_index);
		return;
	}

	if (!crtc_state || !mode)
		mode = &pipeline->subs[crtc->index].mode;
	plane_info->src_x = state->src_x >> 16;
	plane_info->src_y = state->src_y >> 16;
	plane_info->src_w = (state->src_w >> 16) & 0xffff;
	plane_info->src_h = (state->src_h >> 16) & 0xffff;

	plane_info->dst_x = state->crtc_x;
	plane_info->dst_y = state->crtc_y;
	plane_info->dst_w = state->crtc_w;
	plane_info->dst_h = state->crtc_h;
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
	DRM_DEBUG("mode->hdisplay=%d, mode->vdisplay=%d\n",
		  mode->hdisplay, mode->vdisplay);
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
	if (ratio_x > MESON_OSD_SCALE_DOWN_LIMIT ||
	    ratio_y > MESON_OSD_SCALE_DOWN_LIMIT)
		ret = -EDOM;
	if (src_w < dst_w)
		ratio_x = (dst_w + src_w - 1) / src_w;
	if (src_h < dst_h)
		ratio_y = (dst_h + src_h - 1) / src_h;
	if (ratio_x > MESON_OSD_SCALE_UP_LIMIT ||
	    ratio_y > MESON_OSD_SCALE_UP_LIMIT)
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
	size_t fb_size = 0;
	phys_addr_t phyaddr;

	#ifdef CONFIG_DRM_MESON_USE_ION
	meson_fb = container_of(fb, struct am_meson_fb, base);
	if (!meson_fb) {
		return -EINVAL;
	}
	DRM_DEBUG("meson_fb[id:%d,ref:%d]=0x%p\n",
		  meson_fb->base.base.id,
		  kref_read(&meson_fb->base.base.refcount), meson_fb);
	if (meson_fb->logo && meson_fb->logo->alloc_flag &&
	    meson_fb->logo->start) {
		phyaddr = meson_fb->logo->start;
		DRM_DEBUG("logo->phyaddr=0x%pa\n", &phyaddr);
	} else  if (meson_fb->bufp[0]) {
		phyaddr = am_meson_gem_object_get_phyaddr(drv,
							  meson_fb->bufp[0],
							  &fb_size);
	} else {
		phyaddr = 0;
		DRM_INFO("don't find phyaddr!\n");
		return -EINVAL;
	}
	#else
	if (!fb) {
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
	plane_info->fb_size = (u32)fb_size;
	return 0;
}

static int meson_video_plane_fb_check(struct drm_plane *plane,
				      struct drm_plane_state *new_state,
				struct meson_vpu_video_layer_info *plane_info)
{
	struct drm_framebuffer *fb = new_state->fb;
	#ifdef CONFIG_DRM_MESON_USE_ION
	struct am_video_plane *video_plane = to_am_video_plane(plane);
	struct meson_drm *drv = video_plane->drv;
	struct am_meson_fb *meson_fb;
	struct uvm_buf_obj *ubo;
	#else
	struct drm_gem_cma_object *gem;
	#endif
	size_t fb_size[2] = {0};
	phys_addr_t phyaddr, phyaddr1 = 0;

	#ifdef CONFIG_DRM_MESON_USE_ION
	meson_fb = container_of(fb, struct am_meson_fb, base);
	if (!meson_fb) {
		return -EINVAL;
	}
	DRM_DEBUG("meson_fb[id:%d,ref:%d]=0x%p\n",
		  meson_fb->base.base.id,
		  kref_read(&meson_fb->base.base.refcount), meson_fb);
	if (meson_fb->bufp[0]) {
		phyaddr =
			am_meson_gem_object_get_phyaddr(drv,
							meson_fb->bufp[0],
							&fb_size[0]);
	} else {
		//phyaddr = 0;
		DRM_INFO("don't find phyaddr!\n");
		return -EINVAL;
	}
	if (meson_fb->bufp[1] && meson_fb->bufp[1] != meson_fb->bufp[0] &&
	    (fb->format->format == DRM_FORMAT_NV12 ||
				  fb->format->format == DRM_FORMAT_NV21))
		phyaddr1 = am_meson_gem_object_get_phyaddr(drv,
							   meson_fb->bufp[1],
							   &fb_size[1]);
	/* start to get vframe from uvm */
	if (meson_fb->bufp[0]->is_uvm) {
		ubo = &meson_fb->bufp[0]->ubo;
		plane_info->vf = dmabuf_get_vframe(ubo->dmabuf);
		dmabuf_put_vframe(ubo->dmabuf);
		plane_info->is_uvm = meson_fb->bufp[0]->is_uvm;
		plane_info->dmabuf = ubo->dmabuf;
	} else {
		plane_info->dmabuf = meson_fb->bufp[0]->base.dma_buf;
		if (!plane_info->dmabuf)
			plane_info->dmabuf = meson_fb->bufp[0]->dmabuf;
	}
	if (!plane_info->dmabuf)
		return -EINVAL;

	DRM_DEBUG("%s dmabuf %px\n", __func__, plane_info->dmabuf);
	#else
	if (!fb) {
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
	plane_info->phy_addr[0] = phyaddr;
	plane_info->fb_size[0] = (u32)fb_size[0];
	plane_info->phy_addr[1] = phyaddr1;
	plane_info->fb_size[1] = (u32)fb_size[1];
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
	plane_info->pixel_format = fb->format->format;
	plane_info->byte_stride = fb->pitches[0];
	plane_info->afbc_en = 0;
	plane_info->afbc_inter_format = 0;
	plane_info->fb_w = fb->width;
	plane_info->fb_h = fb->height;

	/*setup afbc info*/
	if (fb->modifier) {
		plane_info->afbc_en = 1;
		plane_info->afbc_inter_format = AFBC_EN;
	}

	if (fb->modifier & AFBC_FORMAT_MOD_YTR)
		plane_info->afbc_inter_format |= YUV_TRANSFORM;

	if (fb->modifier & AFBC_FORMAT_MOD_SPLIT)
		plane_info->afbc_inter_format |= BLOCK_SPLIT;

	if (fb->modifier & AFBC_FORMAT_MOD_TILED)
		plane_info->afbc_inter_format |= TILED_HEADER_EN;

	if ((fb->modifier & AFBC_FORMAT_MOD_BLOCK_SIZE_MASK) ==
	    AFBC_FORMAT_MOD_BLOCK_SIZE_32x8)
		plane_info->afbc_inter_format |= SUPER_BLOCK_ASPECT;

	DRM_DEBUG("flags:%d pixel_format:%d,modifier=%llu\n",
		  fb->flags, fb->format->format,
				fb->modifier);
	DRM_DEBUG("plane afbc_en=%u, afbc_inter_format=%x\n",
		  plane_info->afbc_en, plane_info->afbc_inter_format);

	DRM_DEBUG("phy_addr=0x%pa,byte_stride=%d,pixel_format=%d\n",
		  &plane_info->phy_addr, plane_info->byte_stride,
		  plane_info->pixel_format);
	DRM_DEBUG("plane_index %d, size %d.\n",
		  osd_plane->plane_index,
		  plane_info->fb_size);
	return 0;
}

static int meson_video_plane_get_fb_info(struct drm_plane *plane,
					 struct drm_plane_state *new_state,
			struct meson_vpu_video_layer_info *plane_info)
{
	struct am_video_plane *video_plane = to_am_video_plane(plane);
	struct drm_framebuffer *fb = new_state->fb;
	struct meson_drm *drv = video_plane->drv;

	if (!drv) {
		DRM_INFO("%s new_state/meson_drm is NULL!\n", __func__);
		return -EINVAL;
	}
	if (video_plane->plane_index >= MESON_MAX_VIDEO) {
		DRM_INFO("%s invalid plane_index!\n", __func__);
		return -EINVAL;
	}
	plane_info->pixel_format = fb->format->format;
	plane_info->byte_stride = fb->pitches[0];
	plane_info->fb_w = fb->width;
	plane_info->fb_h = fb->height;

	DRM_DEBUG("flags:%d pixel_format:%d,modifier=%llu\n",
		  fb->flags, fb->format->format,
				fb->modifier);
	DRM_DEBUG("phy_addr[0]=0x%pa,byte_stride=%d,pixel_format=%d\n",
		  &plane_info->phy_addr[0], plane_info->byte_stride,
		  plane_info->pixel_format);
	DRM_DEBUG("phy_addr[1]=0x%pa, plane_index %d, size-0 %d, size-1 %d.\n",
		  &plane_info->phy_addr[1],
		  video_plane->plane_index,
		  plane_info->fb_size[0],
		  plane_info->fb_size[1]);
	return 0;
}

static u32 meson_video_parse_config(struct drm_device *dev)
{
	u32 mode_flag = 0;
	int ret;

	ret = of_property_read_u32(dev->dev->of_node,
				   "vfm_mode", &mode_flag);
	if (ret)
		DRM_INFO("%s parse vfm mode fail!\n", __func__);

	DRM_INFO("vfm_mode=%d\n", mode_flag);
	return mode_flag;
}

static bool meson_video_plane_is_repeat_frame(struct drm_plane *plane,
				struct drm_plane_state *new_state, struct meson_vpu_video *mvv)
{
	struct meson_vpu_video_layer_info *plane_info, *old_plane_info;
	struct meson_vpu_pipeline_state *mvps, *old_mvps;
	struct am_video_plane *video_plane = to_am_video_plane(plane);
	struct meson_drm *drv = video_plane->drv;

	mvps = meson_vpu_pipeline_get_new_state(drv->pipeline, new_state->state);
	if (mvps) {
		plane_info = &mvps->video_plane_info[video_plane->plane_index];
		old_mvps = meson_vpu_pipeline_get_old_state(drv->pipeline, new_state->state);
		if (old_mvps) {
			old_plane_info = &old_mvps->video_plane_info[video_plane->plane_index];
			if (plane_info->dmabuf == old_plane_info->dmabuf) {
				mvv->repeat_frame = 1;
				MESON_DRM_FENCE("video repeat frame!");
				return true;
			}
		}
	}

	mvv->repeat_frame = 0;
	return false;
}

static const char *am_meson_video_fence_get_driver_name(struct dma_fence *fence)
{
	return "meson";
}

static const char *
am_meson_video_fence_get_timeline_name(struct dma_fence *fence)
{
	return "meson_video_fence";
}

static void
am_meson_video_fence_release(struct dma_fence *fence)
{
	kfree_rcu(fence, rcu);
	fence = NULL;
}

static const struct dma_fence_ops am_meson_video_plane_fence_ops = {
	.get_driver_name = am_meson_video_fence_get_driver_name,
	.get_timeline_name = am_meson_video_fence_get_timeline_name,
	.release = am_meson_video_fence_release,
};

static struct dma_fence *am_meson_video_create_fence(spinlock_t *lock)
{
	struct dma_fence *fence;

	fence = kzalloc(sizeof(*fence), GFP_KERNEL);
	if (!fence)
		return NULL;

	dma_fence_init(fence, &am_meson_video_plane_fence_ops,
		lock, 0, 0);

	return fence;
}

static int meson_video_prepare_fence(struct drm_plane *plane,
					  struct drm_plane_state *state,
				struct meson_vpu_video *mvv)
{
	struct am_video_plane *video_plane = to_am_video_plane(plane);
	struct dma_fence *fence;

	/*creat implicit fence as out_fence, and next will directly
	 *export to video composer module
	 */
	fence = am_meson_video_create_fence(&video_plane->lock);
	if (!fence)
		return -ENOMEM;

	mvv->fence = fence;
	MESON_DRM_FENCE("creat fence %s fence(%px) plane_index%d\n",
		__func__, fence, video_plane->plane_index);
	return 0;
}

static int meson_plane_atomic_get_property(struct drm_plane *plane,
					   const struct drm_plane_state *state,
					struct drm_property *property,
					uint64_t *val)
{
	struct am_osd_plane *osd_plane = to_am_osd_plane(plane);
	int ret = 0;

	if (property == osd_plane->occupied_property) {
		*val = osd_plane->osd_occupied;
		return 0;
	}

	return ret;
}

static int meson_plane_atomic_set_property(struct drm_plane *plane,
					   struct drm_plane_state *state,
					 struct drm_property *property,
					 uint64_t val)
{
	struct am_osd_plane *osd_plane = to_am_osd_plane(plane);
	int ret = 0;

	if (property == osd_plane->occupied_property) {
		osd_plane->osd_occupied = val;
		return 0;
	}

	return ret;
}

static struct drm_plane_state *
meson_plane_duplicate_state(struct drm_plane *plane)
{
	struct am_meson_plane_state *meson_plane_state;

	if (WARN_ON(!plane->state))
		return NULL;

	DRM_DEBUG("%s (%s)\n", __func__, plane->name);

	meson_plane_state = kzalloc(sizeof(*meson_plane_state), GFP_KERNEL);
	if (!meson_plane_state)
		return NULL;

	__drm_atomic_helper_plane_duplicate_state(plane,
						  &meson_plane_state->base);
	return &meson_plane_state->base;
}

static void meson_plane_destroy_state(struct drm_plane *plane,
				      struct drm_plane_state *state)
{
	struct am_meson_plane_state *meson_plane_state;

	meson_plane_state = to_am_meson_plane_state(state);
	__drm_atomic_helper_plane_destroy_state(&meson_plane_state->base);
	kfree(meson_plane_state);
}

static void meson_plane_reset(struct drm_plane *plane)
{
	struct am_meson_plane_state *meson_plane_state;

	if (plane->state) {
		meson_plane_destroy_state(plane, plane->state);
		plane->state = NULL;
	}

	meson_plane_state = kzalloc(sizeof(*meson_plane_state), GFP_KERNEL);
	if (!meson_plane_state)
		return;

	__drm_atomic_helper_plane_reset(plane, &meson_plane_state->base);
	meson_plane_state->base.pixel_blend_mode = DRM_MODE_BLEND_COVERAGE;
}

bool am_meson_vpu_check_format_mod(struct drm_plane *plane,
				   u32 format, u64 modifier)
{
	DRM_DEBUG("modifier %llu", modifier);
	if (modifier == DRM_FORMAT_MOD_INVALID)
		return false;

	if (modifier == DRM_FORMAT_MOD_LINEAR)
		return true;

	switch (format) {
	case DRM_FORMAT_XRGB8888:
	case DRM_FORMAT_ARGB8888:
		/* YTR is forbidden for non XBGR formats */
		if (modifier & AFBC_FORMAT_MOD_YTR)
			return false;
		fallthrough;
	case DRM_FORMAT_XBGR8888:
	case DRM_FORMAT_ABGR8888:
	case DRM_FORMAT_ABGR2101010:
		return true;
	case DRM_FORMAT_RGB888:
		/* YTR is forbidden for non XBGR formats */
		if (modifier & AFBC_FORMAT_MOD_YTR)
			return false;
		return true;
	case DRM_FORMAT_RGB565:
		/* YTR is forbidden for non XBGR formats */
		if (modifier & AFBC_FORMAT_MOD_YTR)
			return false;
		return true;
	/* TOFIX support mode formats */
	default:
		DRM_DEBUG("unsupported afbc format[%08x]\n", format);
		return false;
	}
}

bool am_meson_vpu_check_video_format_mod(struct drm_plane *plane,
		u32 format, u64 modifier)
{
	if (modifier == DRM_FORMAT_MOD_LINEAR &&
	    format != DRM_FORMAT_YUV420_8BIT &&
	    format != DRM_FORMAT_YUV420_10BIT)
		return true;

	if ((modifier & DRM_FORMAT_MOD_AMLOGIC_FBC(0, 0)) ==
			DRM_FORMAT_MOD_AMLOGIC_FBC(0, 0)) {
		unsigned int layout = modifier &
			DRM_FORMAT_MOD_AMLOGIC_FBC(__fourcc_mod_amlogic_layout_mask, 0);
		unsigned int options =
			(modifier >> __fourcc_mod_amlogic_options_shift) &
			__fourcc_mod_amlogic_options_mask;

		if (format != DRM_FORMAT_YUV420_8BIT &&
		    format != DRM_FORMAT_YUV420_10BIT) {
			DRM_DEBUG_KMS("%llx invalid format 0x%08x\n",
				      modifier, format);
			return false;
		}

		if (layout != AMLOGIC_FBC_LAYOUT_BASIC &&
		    layout != AMLOGIC_FBC_LAYOUT_SCATTER) {
			DRM_DEBUG_KMS("%llx invalid layout %x\n",
				      modifier, layout);
			return false;
		}

		if (options &&
		    options != AMLOGIC_FBC_OPTION_MEM_SAVING) {
			DRM_DEBUG_KMS("%llx invalid layout %x\n",
				      modifier, layout);
			return false;
		}

		return true;
	}

	DRM_DEBUG_KMS("invalid modifier %llx for format 0x%08x\n",
		      modifier, format);

	return false;
}

static void meson_osd_plane_atomic_print_state(struct drm_printer *p,
		const struct drm_plane_state *state)
{
	struct drm_plane *plane;
	struct meson_vpu_osd_layer_info *plane_info;
	struct meson_vpu_pipeline_state *mvps;
	struct am_osd_plane *osd_plane;
	struct meson_drm *drv;
	struct drm_private_state *obj_state;

	if (!state) {
		DRM_INFO("%s state/meson_drm is NULL!\n", __func__);
		return;
	}

	plane = state->plane;
	if (!plane) {
		DRM_INFO("%s drm_plane is NULL!\n", __func__);
		return;
	}

	osd_plane = to_am_osd_plane(plane);
	DRM_DEBUG("%s [%d]\n", __func__, osd_plane->plane_index);

	drv = osd_plane->drv;
	if (!drv || !drv->pipeline) {
		DRM_INFO("%s private state or pipeline is NULL!\n", __func__);
		return;
	}

	obj_state = drv->pipeline->obj.state;
	if (!obj_state) {
		DRM_ERROR("null pipeline obj state!\n");
		return;
	}

	mvps = container_of(obj_state, struct meson_vpu_pipeline_state, obj);

	if (!mvps || osd_plane->plane_index >= MESON_MAX_OSDS) {
		DRM_INFO("%s mvps/osd_plane is NULL!\n", __func__);
		return;
	}

	plane_info = &mvps->plane_info[osd_plane->plane_index];

	drm_printf(p, "\tmeson osd plane %d info:\n", osd_plane->plane_index);
	drm_printf(p, "\t\tsrc_x=%u\n", plane_info->src_x);
	drm_printf(p, "\t\tsrc_y=%u\n", plane_info->src_y);
	drm_printf(p, "\t\tsrc_w=%u\n", plane_info->src_w);
	drm_printf(p, "\t\tsrc_h=%u\n", plane_info->src_h);
	drm_printf(p, "\t\tdst_w=%u\n", plane_info->dst_w);
	drm_printf(p, "\t\tdst_h=%u\n", plane_info->dst_h);
	drm_printf(p, "\t\tdst_x=%d\n", plane_info->dst_x);
	drm_printf(p, "\t\tdst_y=%d\n", plane_info->dst_y);
	drm_printf(p, "\t\tfb_w=%u\n", plane_info->fb_w);
	drm_printf(p, "\t\tfb_h=%u\n", plane_info->fb_h);
	drm_printf(p, "\t\tzorder=%u\n", plane_info->zorder);
	drm_printf(p, "\t\tbyte_stride=%u\n", plane_info->byte_stride);
	drm_printf(p, "\t\tpixel_format=%u\n", plane_info->pixel_format);
	drm_printf(p, "\t\tphy_addr=0x%llx\n", plane_info->phy_addr);
	drm_printf(p, "\t\tplane_index=%u\n", plane_info->plane_index);
	drm_printf(p, "\t\tuhd_plane_index=%u\n", plane_info->uhd_plane_index);
	drm_printf(p, "\t\tenable=%u\n", plane_info->enable);
	drm_printf(p, "\t\tratio_x=%u\n", plane_info->ratio_x);
	drm_printf(p, "\t\tafbc_inter_format=%u\n",
					plane_info->afbc_inter_format);
	drm_printf(p, "\t\tafbc_en=%u\n", plane_info->afbc_en);
	drm_printf(p, "\t\tfb_size=%u\n", plane_info->fb_size);
	drm_printf(p, "\t\tpixel_blend=%u\n", plane_info->pixel_blend);
	drm_printf(p, "\t\trotation=%u\n", plane_info->rotation);
	drm_printf(p, "\t\tblend_bypass=%u\n", plane_info->blend_bypass);
	drm_printf(p, "\t\tglobal_alpha=%u\n", plane_info->global_alpha);
	drm_printf(p, "\t\tscaling_filter=%u\n", plane_info->scaling_filter);
	drm_printf(p, "\t\tread_ports=%u\n", plane_info->read_ports);
}

static void meson_video_plane_atomic_print_state(struct drm_printer *p,
		const struct drm_plane_state *state)
{
	struct drm_plane *plane;
	struct meson_vpu_video_layer_info *plane_info;
	struct meson_vpu_pipeline_state *mvps;
	struct am_video_plane *video_plane;
	struct meson_drm *drv;
	struct drm_private_state *obj_state;

	if (!state) {
		DRM_INFO("%s state/meson_drm is NULL!\n", __func__);
		return;
	}

	plane = state->plane;
	if (!plane) {
		DRM_INFO("%s drm_plane is NULL!\n", __func__);
		return;
	}

	video_plane = to_am_video_plane(plane);
	DRM_DEBUG("%s [%d]\n", __func__, video_plane->plane_index);

	drv = video_plane->drv;
	if (!drv || !drv->pipeline) {
		DRM_INFO("%s private state is NULL!\n", __func__);
		return;
	}

	obj_state = drv->pipeline->obj.state;
	if (!obj_state) {
		DRM_ERROR("null pipeline obj state!\n");
		return;
	}

	mvps = container_of(obj_state, struct meson_vpu_pipeline_state, obj);

	if (!mvps || video_plane->plane_index >= MESON_MAX_VIDEO) {
		DRM_INFO("%s mvps/video_plane is NULL!\n", __func__);
		return;
	}

	plane_info = &mvps->video_plane_info[video_plane->plane_index];

	drm_printf(p, "\tmeson video plane %d info:\n",
					video_plane->plane_index);
	drm_printf(p, "\t\tsrc_x=%u\n", plane_info->src_x);
	drm_printf(p, "\t\tsrc_y=%u\n", plane_info->src_y);
	drm_printf(p, "\t\tsrc_w=%u\n", plane_info->src_w);
	drm_printf(p, "\t\tsrc_h=%u\n", plane_info->src_h);
	drm_printf(p, "\t\tdst_w=%u\n", plane_info->dst_w);
	drm_printf(p, "\t\tdst_h=%u\n", plane_info->dst_h);
	drm_printf(p, "\t\tdst_x=%d\n", plane_info->dst_x);
	drm_printf(p, "\t\tdst_y=%d\n", plane_info->dst_y);
	drm_printf(p, "\t\tzorder=%u\n", plane_info->zorder);
	drm_printf(p, "\t\tbyte_stride=%u\n", plane_info->byte_stride);
	drm_printf(p, "\t\tpixel_format=%u\n", plane_info->pixel_format);
	drm_printf(p, "\t\tphy_addr[0]=0x%llx\n", plane_info->phy_addr[0]);
	drm_printf(p, "\t\tphy_addr[1]=0x%llx\n", plane_info->phy_addr[1]);
	drm_printf(p, "\t\tplane_index=%u\n", plane_info->plane_index);
	drm_printf(p, "\t\tenable=%u\n", plane_info->enable);
	drm_printf(p, "\t\tratio_x=%u\n", plane_info->ratio_x);
	drm_printf(p, "\t\tfb_size[0]=%u\n", plane_info->fb_size[0]);
	drm_printf(p, "\t\tfb_size[1]=%u\n", plane_info->fb_size[1]);
	drm_printf(p, "\t\tpixel_blend=%u\n", plane_info->pixel_blend);
	/*TODO: vframe_s */
	drm_printf(p, "\t\tis_uvm=%u\n", plane_info->is_uvm);
	drm_printf(p, "\t\tvfm_mode=%u\n", plane_info->vfm_mode);
}

static const struct drm_plane_funcs am_osd_plane_funs = {
	.update_plane		= drm_atomic_helper_update_plane,
	.disable_plane		= drm_atomic_helper_disable_plane,
	.destroy		= drm_plane_cleanup,
	.reset			= meson_plane_reset,
	.atomic_duplicate_state = meson_plane_duplicate_state,
	.atomic_destroy_state	= meson_plane_destroy_state,
	.atomic_set_property = meson_plane_atomic_set_property,
	.atomic_get_property = meson_plane_atomic_get_property,
	.format_mod_supported = am_meson_vpu_check_format_mod,
	.atomic_print_state = meson_osd_plane_atomic_print_state,
};

static const struct drm_plane_funcs am_video_plane_funs = {
	.update_plane		= drm_atomic_helper_update_plane,
	.disable_plane		= drm_atomic_helper_disable_plane,
	.destroy		= drm_plane_cleanup,
	.reset			= meson_plane_reset,
	.atomic_duplicate_state = meson_plane_duplicate_state,
	.atomic_destroy_state	= meson_plane_destroy_state,
	.atomic_set_property = meson_plane_atomic_set_property,
	.atomic_get_property = meson_plane_atomic_get_property,
	.format_mod_supported = am_meson_vpu_check_video_format_mod,
	.atomic_print_state = meson_video_plane_atomic_print_state,
};

static int meson_plane_prepare_fb(struct drm_plane *plane,
				  struct drm_plane_state *new_state)
{
	return 0;
}

static void meson_plane_cleanup_fb(struct drm_plane *plane,
				   struct drm_plane_state *old_state)
{
	//struct am_osd_plane *osd_plane = to_am_osd_plane(plane);

	//DRM_DEBUG("osd %d.\n", osd_plane->plane_index);
}

static void meson_plane_atomic_update(struct drm_plane *plane,
				      struct drm_plane_state *old_state)
{
	DRM_DEBUG("osd plane atomic_update.\n");
}

static void meson_video_plane_atomic_update(struct drm_plane *plane,
				      struct drm_plane_state *old_state)
{
	int video_index, crtc_index;
	struct meson_vpu_sub_pipeline *sub_pipe;
	struct am_video_plane *video_plane = to_am_video_plane(plane);
	struct drm_crtc *crtc = plane->state->crtc;
	struct am_meson_crtc *amcrtc = to_am_meson_crtc(crtc);
	struct meson_vpu_pipeline *pipeline = amcrtc->pipeline;
	struct drm_atomic_state *old_atomic_state = old_state->state;
	struct meson_vpu_video *mvv = pipeline->video[video_plane->plane_index];

	crtc_index = crtc->index;
	video_index = video_plane->plane_index;
	sub_pipe = &pipeline->subs[crtc_index];

	DRM_DEBUG("video plane atomic_update.\n");
	if (!meson_video_plane_is_repeat_frame(plane, old_state, mvv))
		meson_video_prepare_fence(plane, old_state, mvv);
	vpu_video_plane_update(sub_pipe, old_atomic_state, video_index);
}

static int meson_plane_atomic_check(struct drm_plane *plane,
				    struct drm_plane_state *state)
{
	struct meson_vpu_osd_layer_info *plane_info;
	struct meson_vpu_pipeline_state *mvps;
	struct am_osd_plane *osd_plane = to_am_osd_plane(plane);
	struct meson_drm *drv = osd_plane->drv;
	int ret;

	if (!state || !drv) {
		DRM_INFO("%s state/meson_drm is NULL!\n", __func__);
		return -EINVAL;
	}

	DRM_DEBUG("%s [%d]\n", __func__, osd_plane->plane_index);

	mvps = meson_vpu_pipeline_get_state(drv->pipeline, state->state);
	if (!mvps || osd_plane->plane_index >= MESON_MAX_OSDS) {
		DRM_INFO("%s mvps/osd_plane is NULL!\n", __func__);
		return -EINVAL;
	}
	plane_info = &mvps->plane_info[osd_plane->plane_index];
	if ((plane_info->src_w != ((state->src_w >> 16) & 0xffff)) ||
		(plane_info->src_h != ((state->src_h >> 16) & 0xffff)) ||
		plane_info->dst_x != state->crtc_x ||
		plane_info->dst_y != state->crtc_y ||
		plane_info->dst_w != state->crtc_w ||
		plane_info->dst_h != state->crtc_h ||
		plane_info->zorder != state->zpos ||
		!plane_info->enable)
		plane_info->status_changed = 1;
	else
		plane_info->status_changed = 0;

	plane_info->plane_index = osd_plane->plane_index;
	/*get plane prop value*/
	plane_info->zorder = state->zpos;
	plane_info->pixel_blend = state->pixel_blend_mode;
	plane_info->global_alpha = state->alpha;
	plane_info->scaling_filter = (u32)state->scaling_filter;

	mvps->plane_index[osd_plane->plane_index] = osd_plane->plane_index;
	meson_plane_position_calc(plane_info, state, mvps->pipeline);
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
		DRM_DEBUG("fb is invalid, disable plane[%d].\n", plane_info->src_w);
		return ret;
	}

	plane_info->enable = 1;
	if (state->crtc)
		plane_info->crtc_index = state->crtc->index;

	DRM_DEBUG("OSD PLANE index=%d, zorder=%d, premult= %d, alpha = %d, phy = %llx\n",
		  plane_info->plane_index, plane_info->zorder,
		  state->pixel_blend_mode, plane_info->global_alpha,
		  plane_info->phy_addr);
	DRM_DEBUG("w/h = %d/%d, src_x/y/w/h=%d/%d/%d/%d\n",
		  plane_info->fb_w, plane_info->fb_h,
		  plane_info->src_x, plane_info->src_y,
		plane_info->src_w, plane_info->src_h);
	DRM_DEBUG("status_changed = %d, dst_x/y/w/h=%d/%d/%d/%d\n",
		  plane_info->status_changed, plane_info->dst_x, plane_info->dst_y,
		plane_info->dst_w, plane_info->dst_h);
	return 0;
}

static int meson_video_plane_atomic_check(struct drm_plane *plane,
					  struct drm_plane_state *state)
{
	struct meson_vpu_video_layer_info *plane_info;
	struct meson_vpu_pipeline_state *mvps;
	struct am_video_plane *video_plane = to_am_video_plane(plane);
	struct meson_drm *drv = video_plane->drv;
	int ret;

	if (!state || !drv) {
		DRM_INFO("%s state/meson_drm is NULL!\n", __func__);
		return -EINVAL;
	}

	DRM_DEBUG("plane_index [%d]\n", video_plane->plane_index);

	mvps = meson_vpu_pipeline_get_state(drv->pipeline, state->state);
	if (!mvps || video_plane->plane_index >= MESON_MAX_VIDEO) {
		DRM_INFO("%s mvps/video_plane is NULL!\n", __func__);
		return -EINVAL;
	}

	plane_info = &mvps->video_plane_info[video_plane->plane_index];
	plane_info->plane_index = video_plane->plane_index;
	plane_info->vfm_mode = video_plane->vfm_mode;
	plane_info->zorder = state->zpos + plane_info->plane_index;

	mvps->plane_index[video_plane->plane_index] = video_plane->plane_index;
	meson_video_plane_position_calc(plane_info, state,
					mvps->pipeline);
	ret = meson_video_plane_fb_check(plane, state, plane_info);
	if (ret < 0) {
		plane_info->enable = 0;
		DRM_DEBUG("plane%d fb is NULL,disable the plane!\n",
			  plane_info->plane_index);
		return 0;
	}
	ret = meson_video_plane_get_fb_info(plane, state, plane_info);
	if (ret < 0 ||
	    plane_info->src_w == 0) {
		plane_info->enable = 0;
		return ret;
	}

	plane_info->enable = 1;
	if (state->crtc)
		plane_info->crtc_index = state->crtc->index;
	DRM_DEBUG("VIDOE PLANE index=%d, zorder=%d\n",
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

static void meson_video_plane_atomic_disable(struct drm_plane *plane,
					     struct drm_plane_state *old_state)
{
	int video_index, crtc_index;
	struct meson_vpu_sub_pipeline *sub_pipe;
	struct am_video_plane *video_plane = to_am_video_plane(plane);
	struct meson_vpu_pipeline *pipeline = video_plane->pipeline;
	struct drm_atomic_state *old_atomic_state = old_state->state;
	unsigned long pos_crtcs = plane->possible_crtcs;

	crtc_index = find_first_bit(&pos_crtcs, 32);
	if (crtc_index >= MESON_MAX_CRTC)
		crtc_index = 0;

	video_index = video_plane->plane_index;
	sub_pipe = &pipeline->subs[crtc_index];
	DRM_DEBUG("%s video %d, crtc %d.\n", __func__, video_index, crtc_index);
	vpu_video_plane_update(sub_pipe, old_atomic_state, video_index);
}

/*add async check & atomic funs*/
int meson_osd_plane_async_check(struct drm_plane *plane,
	struct drm_plane_state *new_state)
{
	int ret = 0;
	struct meson_vpu_pipeline_state *mvps;
	struct am_osd_plane *osd_plane = to_am_osd_plane(plane);
	struct meson_drm *drv = osd_plane->drv;
	struct meson_vpu_osd_layer_info *plane_info;

	DRM_DEBUG("plane_index-%d\n", osd_plane->plane_index);

	if (!new_state->state) {
		DRM_ERROR("atomic state is NULL!\n");
		return -EINVAL;
	}

	mvps = meson_vpu_pipeline_get_state(drv->pipeline, new_state->state);
	if (!mvps) {
		DRM_ERROR("mvps is NULL.\n");
		return -EINVAL;
	}

	plane_info = &mvps->plane_info[osd_plane->plane_index];
	if ((plane_info->enable && !new_state->fb) || !plane_info->enable) {
		DRM_ERROR("plane visible state changed.\n");
		return -EINVAL;
	}

	if (plane_info->status_changed) {
		DRM_ERROR("osd%d plane info changed\n", osd_plane->plane_index);
		return -EINVAL;
	}

	return ret;
}

int meson_video_plane_async_check(struct drm_plane *plane,
	struct drm_plane_state *new_state)
{
	struct am_video_plane *video_plane = to_am_video_plane(plane);
	struct meson_vpu_video_layer_info *plane_info;
	struct meson_vpu_pipeline_state *mvps;
	struct meson_drm *drv = video_plane->drv;

	DRM_DEBUG("plane_index-%d\n", video_plane->plane_index);

	if (!new_state->state) {
		DRM_ERROR("atomic state is NULL!\n");
		return -EINVAL;
	}

	mvps = meson_vpu_pipeline_get_state(drv->pipeline, new_state->state);
	if (!mvps) {
		DRM_ERROR("mvps is NULL.\n");
		return -EINVAL;
	}

	plane_info = &mvps->video_plane_info[video_plane->plane_index];
	if ((plane_info->enable && !new_state->fb) || !plane_info->enable) {
		DRM_ERROR("plane visible state changed.\n");
		return -EINVAL;
	}

	return 0;
}

void meson_osd_plane_async_update(struct drm_plane *plane,
	struct drm_plane_state *new_state)
{
	struct am_osd_plane *osd_plane = to_am_osd_plane(plane);
	struct meson_vpu_sub_pipeline *sub_pipe;
	struct am_meson_crtc *amcrtc;
	struct meson_vpu_pipeline *pipeline;
	int crtc_index;

	if (!new_state || !new_state->state) {
		DRM_ERROR("plane state is null.\n");
		return;
	}

	amcrtc = to_am_meson_crtc(new_state->crtc);
	pipeline = amcrtc->pipeline;
	crtc_index = amcrtc->crtc_index;
	sub_pipe = &pipeline->subs[crtc_index];

	DRM_DEBUG("plane_index-%d\n", osd_plane->plane_index);
	plane->state->fb = new_state->fb;
	plane->state->src_x = new_state->src_x;
	plane->state->src_y = new_state->src_y;
	plane->state->crtc_x = new_state->crtc_x;
	plane->state->crtc_y = new_state->crtc_y;

	vpu_pipeline_prepare_update(pipeline, new_state->crtc->mode.vdisplay,
			new_state->crtc->mode.vrefresh, crtc_index);
	vpu_pipeline_osd_update(sub_pipe, new_state->state);
	vpu_pipeline_finish_update(pipeline, crtc_index);
}

void meson_video_plane_async_update(struct drm_plane *plane,
	struct drm_plane_state *new_state)
{
	struct am_video_plane *video_plane = to_am_video_plane(plane);
	struct meson_vpu_sub_pipeline *sub_pipe;
	struct am_meson_crtc *amcrtc;
	struct meson_vpu_pipeline *pipeline;
	struct meson_vpu_video *mvv;
	int crtc_index;

	if (!new_state || !new_state->state) {
		DRM_ERROR("plane state is null.\n");
		return;
	}

	DRM_DEBUG("%s plane_index-%d, crtc-%p\n", __func__,
		video_plane->plane_index, new_state->crtc);

	amcrtc = to_am_meson_crtc(new_state->crtc);
	pipeline = amcrtc->pipeline;
	crtc_index = amcrtc->crtc_index;
	sub_pipe = &pipeline->subs[crtc_index];
	mvv = pipeline->video[video_plane->plane_index];

	plane->state->fb = new_state->fb;
	plane->state->src_x = new_state->src_x;
	plane->state->src_y = new_state->src_y;
	plane->state->crtc_x = new_state->crtc_x;
	plane->state->crtc_y = new_state->crtc_y;

	if (!meson_video_plane_is_repeat_frame(plane, new_state, mvv))
		meson_video_prepare_fence(plane, new_state, mvv);
	vpu_pipeline_video_update(sub_pipe, new_state->state);
}

static const struct drm_plane_helper_funcs am_osd_helper_funcs = {
	.prepare_fb = meson_plane_prepare_fb,
	.cleanup_fb = meson_plane_cleanup_fb,
	.atomic_update	= meson_plane_atomic_update,
	.atomic_check	= meson_plane_atomic_check,
	.atomic_disable	= meson_plane_atomic_disable,
	.atomic_async_check = meson_osd_plane_async_check,
	.atomic_async_update = meson_osd_plane_async_update,
};

static const struct drm_plane_helper_funcs am_video_helper_funcs = {
	.prepare_fb = meson_plane_prepare_fb,
	.cleanup_fb = meson_plane_cleanup_fb,
	.atomic_update	= meson_video_plane_atomic_update,
	.atomic_check	= meson_video_plane_atomic_check,
	.atomic_disable	= meson_video_plane_atomic_disable,
	.atomic_async_check = meson_video_plane_async_check,
	.atomic_async_update = meson_video_plane_async_update,
};

struct drm_property *
meson_create_scaling_filter_prop(struct drm_device *dev,
			       unsigned int supported_filters)
{
	struct drm_property *prop;
	static const struct drm_prop_enum_list props[] = {
	{ DRM_SCALING_FILTER_DEFAULT, "Default" },
	{ DRM_SCALING_FILTER_NEAREST_NEIGHBOR, "Nearest Neighbor" },
	{ DRM_SCALING_FILTER_BICUBIC_SHARP, "Bicubic_Sharp" },
	{ DRM_SCALING_FILTER_BICUBIC, "Bicubic" },
	{ DRM_SCALING_FILTER_BILINEAR, "Bilinear" },
	{ DRM_SCALING_FILTER_2POINT_BILINEAR, "2Point_Bilinear" },
	{ DRM_SCALING_FILTER_3POINT_TRIANGLE_SHARP, "3Point_Triangle_Sharp" },
	{ DRM_SCALING_FILTER_3POINT_TRIANGLE, "3Point_Triangle" },
	{ DRM_SCALING_FILTER_4POINT_TRIANGLE, "4Point_Triangle" },
	{ DRM_SCALING_FILTER_4POINT_BSPLINE, "4Point_BSPline" },
	{ DRM_SCALING_FILTER_3POINT_BSPLINE, "3Point_BSPline" },
	{ DRM_SCALING_FILTER_REPEATE, "Repeate" },
	};
	unsigned int valid_mode_mask = BIT(DRM_SCALING_FILTER_DEFAULT) |
				BIT(DRM_SCALING_FILTER_NEAREST_NEIGHBOR) |
				BIT(DRM_SCALING_FILTER_BICUBIC_SHARP) |
				BIT(DRM_SCALING_FILTER_BICUBIC) |
				BIT(DRM_SCALING_FILTER_BILINEAR) |
				BIT(DRM_SCALING_FILTER_2POINT_BILINEAR)  |
				BIT(DRM_SCALING_FILTER_3POINT_TRIANGLE_SHARP) |
				BIT(DRM_SCALING_FILTER_3POINT_TRIANGLE) |
				BIT(DRM_SCALING_FILTER_4POINT_TRIANGLE) |
				BIT(DRM_SCALING_FILTER_4POINT_BSPLINE) |
				BIT(DRM_SCALING_FILTER_3POINT_BSPLINE) |
				BIT(DRM_SCALING_FILTER_REPEATE);

	int i;

	if (WARN_ON((supported_filters & ~valid_mode_mask) ||
		((supported_filters & BIT(DRM_SCALING_FILTER_DEFAULT)) == 0)))
		return ERR_PTR(-EINVAL);

	prop = drm_property_create(dev, DRM_MODE_PROP_ENUM,
				   "SCALING_FILTER",
				   hweight32(supported_filters));
	if (!prop)
		return ERR_PTR(-ENOMEM);

	for (i = 0; i < ARRAY_SIZE(props); i++) {
		int ret;

		if (!(BIT(props[i].type) & supported_filters))
			continue;

		ret = drm_property_add_enum(prop, props[i].type,
					    props[i].name);

		if (ret) {
			drm_property_destroy(dev, prop);

			return ERR_PTR(ret);
		}
	}

	return prop;
}

int meson_plane_create_scaling_filter_property(struct drm_plane *plane,
					     unsigned int supported_filters)
{
	struct drm_property *prop =
		meson_create_scaling_filter_prop(plane->dev, supported_filters);

	if (IS_ERR(prop))
		return PTR_ERR(prop);

	drm_object_attach_property(&plane->base, prop,
				   DRM_SCALING_FILTER_DEFAULT);
	plane->scaling_filter_property = prop;

	return 0;
}

static void meson_plane_add_occupied_property(struct drm_device *drm_dev,
						  struct am_osd_plane *osd_plane)
{
	struct drm_property *prop;

	prop = drm_property_create_bool(drm_dev, 0, "meson.plane.occupied");
	if (prop) {
		osd_plane->occupied_property = prop;
		drm_object_attach_property(&osd_plane->base.base, prop, 0);
	} else {
		DRM_ERROR("Failed to occupied property\n");
	}
}

static const u32 meson_plane_fb_size_list[] = {
	1080 << 16 | 1920,
	2160 << 16 | 3840,
};

static void meson_plane_add_max_fb_property(struct drm_device *drm_dev,
					    struct am_osd_plane *osd_plane)
{
	int ret;
	u32 size_index, max_fb_size;
	struct drm_property *prop;

	ret = of_property_read_u32(drm_dev->dev->of_node, "max_fb_size", &size_index);
	if (ret)
		max_fb_size = meson_plane_fb_size_list[0];
	else
		max_fb_size = meson_plane_fb_size_list[size_index];

	prop = drm_property_create_range(drm_dev, DRM_MODE_PROP_IMMUTABLE,
					"max_fb_size", 0, UINT_MAX);
	if (prop) {
		osd_plane->max_fb_property = prop;
		drm_object_attach_property(&osd_plane->base.base, prop, max_fb_size);
	} else {
		DRM_ERROR("Failed to create max_fb property\n");
	}
}

static void meson_plane_get_primary_plane(struct meson_drm *priv,
			enum drm_plane_type *type)
{
	int i, j, first_plane;

	for (j = 0; j < MESON_MAX_OSDS; j++)
		type[j] = DRM_PLANE_TYPE_OVERLAY;

	for (i = 0; i < MESON_MAX_CRTC; i++) {
		for (j = 0; j < MESON_MAX_OSDS; j++) {
			if (i == priv->crtc_mask_osd[j] &&
				priv->osd_occupied_index != j) {
				first_plane = j;

				if (first_plane < MESON_MAX_OSDS) {
					type[j] = DRM_PLANE_TYPE_PRIMARY;
					priv->primary_plane_index[i] = j;
					break;
				}
			}
		}
	}
}

static struct am_osd_plane *am_osd_plane_create(struct meson_drm *priv,
		int i, u32 crtc_mask, enum drm_plane_type type)
{
	struct am_osd_plane *osd_plane;
	struct drm_plane *plane;
	u32  zpos, min_zpos, max_zpos, osd_index;
	char plane_name[8];
	const char *const_plane_name;

	osd_plane = devm_kzalloc(priv->drm->dev, sizeof(*osd_plane),
				 GFP_KERNEL);
	if (!osd_plane)
		return 0;

	min_zpos = OSD_PLANE_BEGIN_ZORDER;
	max_zpos = OSD_PLANE_END_ZORDER;

	osd_plane->drv = priv;
	osd_plane->plane_index = i;
	osd_plane->plane_type = OSD_PLANE;

	get_logo_osd_reverse(&osd_index, &logo.osd_reverse);
	switch (logo.osd_reverse) {
	case 1:
		osd_plane->osd_reverse = DRM_MODE_REFLECT_MASK;
		break;
	case 2:
		osd_plane->osd_reverse = DRM_MODE_REFLECT_X;
		break;
	case 3:
		osd_plane->osd_reverse = DRM_MODE_REFLECT_Y;
		break;
	default:
		osd_plane->osd_reverse = DRM_MODE_ROTATE_0;
		break;
	}

	zpos = osd_plane->plane_index + min_zpos;

	plane = &osd_plane->base;
	sprintf(plane_name, "osd%d", i);
	const_plane_name = plane_name;

	if (i == priv->osd_occupied_index)
		osd_plane->osd_occupied = true;
	else
		osd_plane->osd_occupied = false;

	drm_universal_plane_init(priv->drm, plane, 1 << crtc_mask,
				 &am_osd_plane_funs,
				 supported_drm_formats,
				 ARRAY_SIZE(supported_drm_formats),
				 afbc_modifier,
				 type, const_plane_name);
	drm_plane_create_blend_mode_property(plane,
				BIT(DRM_MODE_BLEND_PIXEL_NONE) |
				BIT(DRM_MODE_BLEND_PREMULTI)   |
				BIT(DRM_MODE_BLEND_COVERAGE));
	drm_plane_create_alpha_property(plane);
	drm_plane_create_rotation_property(plane,
				DRM_MODE_ROTATE_0,
				DRM_MODE_ROTATE_0 |
				DRM_MODE_REFLECT_MASK);
	drm_plane_create_zpos_property(plane, zpos, min_zpos, max_zpos);
	drm_plane_helper_add(plane, &am_osd_helper_funcs);
	meson_plane_create_scaling_filter_property(plane,
				BIT(DRM_SCALING_FILTER_DEFAULT) |
				BIT(DRM_SCALING_FILTER_NEAREST_NEIGHBOR) |
				BIT(DRM_SCALING_FILTER_BICUBIC_SHARP) |
				BIT(DRM_SCALING_FILTER_BICUBIC) |
				BIT(DRM_SCALING_FILTER_BILINEAR) |
				BIT(DRM_SCALING_FILTER_2POINT_BILINEAR) |
				BIT(DRM_SCALING_FILTER_3POINT_TRIANGLE_SHARP) |
				BIT(DRM_SCALING_FILTER_3POINT_TRIANGLE) |
				BIT(DRM_SCALING_FILTER_4POINT_TRIANGLE) |
				BIT(DRM_SCALING_FILTER_4POINT_BSPLINE) |
				BIT(DRM_SCALING_FILTER_3POINT_BSPLINE) |
				BIT(DRM_SCALING_FILTER_REPEATE));
	meson_plane_add_occupied_property(priv->drm, osd_plane);
	meson_plane_add_max_fb_property(priv->drm, osd_plane);
	DRM_INFO("osd plane %d create done, occupied-%d crtc_mask-%d type-%d\n",
		i, osd_plane->osd_occupied, crtc_mask, type);
	return osd_plane;
}

static struct am_video_plane *am_video_plane_create(struct meson_drm *priv,
						    int i, u32 crtc_mask)
{
	struct am_video_plane *video_plane;
	struct drm_plane *plane;
	char plane_name[8];
	u32 zpos, min_zpos, max_zpos;
	const char *const_plane_name;

	video_plane = devm_kzalloc(priv->drm->dev, sizeof(*video_plane),
				   GFP_KERNEL);
	if (!video_plane) {
		DRM_INFO("no memory to alloc video plane\n");
		return 0;
	}

	min_zpos = 0;
	max_zpos = 255;

	video_plane->drv = priv;
	video_plane->plane_index = i;

	video_plane->plane_type = VIDEO_PLANE;
	video_plane->pipeline = priv->pipeline;
	zpos = video_plane->plane_index + min_zpos;

	plane = &video_plane->base;
	sprintf(plane_name, "video%d", i);
	const_plane_name = plane_name;
	spin_lock_init(&video_plane->lock);

	drm_universal_plane_init(priv->drm, plane, 1 << crtc_mask,
				 &am_video_plane_funs,
				 video_supported_drm_formats,
				 ARRAY_SIZE(video_supported_drm_formats),
				 video_fbc_modifier,
				 DRM_PLANE_TYPE_OVERLAY, const_plane_name);

	drm_plane_create_zpos_property(plane, zpos, min_zpos, max_zpos);
	drm_plane_helper_add(plane, &am_video_helper_funcs);
	DRM_INFO("video plane %d create done\n", i);
	return video_plane;
}

int am_meson_plane_create(struct meson_drm *priv)
{
	struct am_osd_plane *plane;
	struct am_video_plane *video_plane;
	struct meson_vpu_pipeline *pipeline = priv->pipeline;
	enum drm_plane_type type[MESON_MAX_OSD];
	int i, osd_index, video_index;
	u32 vfm_mode;

	memset(priv->osd_planes, 0, sizeof(struct am_osd_plane *) * MESON_MAX_OSD);
	memset(priv->video_planes, 0, sizeof(struct am_video_plane *) * MESON_MAX_VIDEO);

	/*calculate primary plane*/
	meson_plane_get_primary_plane(priv, type);

	/*osd plane*/
	for (i = 0; i < MESON_MAX_OSD; i++) {
		if (!pipeline->osds[i])
			continue;

		osd_index = pipeline->osds[i]->base.index;
		plane = am_osd_plane_create(priv, osd_index, priv->crtc_mask_osd[i], type[i]);

		if (!plane)
			return -ENOMEM;

		if (i == 0)
			priv->primary_plane = &plane->base;

		priv->osd_planes[i] = plane;
		priv->num_planes++;
	}
	DRM_INFO("create %d osd plane done\n", pipeline->num_osds);
	vfm_mode = meson_video_parse_config(priv->drm);

	/*video plane: init after osd to provide osd id at first.*/
	for (i = 0; i < pipeline->num_video; i++) {
		video_index = pipeline->video[i]->base.index;
		video_plane = am_video_plane_create(priv, video_index, priv->crtc_mask_video[i]);
		if (!video_plane)
			return -ENOMEM;

		video_plane->vfm_mode = vfm_mode;
		priv->video_planes[i] = video_plane;
		priv->num_planes++;
	}
	DRM_INFO("create %d video plane done\n", pipeline->num_video);

	return 0;
}
