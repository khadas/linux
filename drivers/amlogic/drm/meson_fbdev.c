// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <drm/drm.h>
#include <drm/drmP.h>
#include <drm/drm_fb_helper.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_uapi.h>
#include <drm/drm_atomic_helper.h>
#include <linux/ion.h>

#include "meson_drv.h"
#include "meson_gem.h"
#include "meson_fb.h"
#include "meson_fbdev.h"
#include "meson_plane.h"

#define PREFERRED_BPP		32
#define PREFERRED_DEPTH	32
#define MESON_DRM_MAX_CONNECTOR	2

static int am_meson_fbdev_alloc_fb_gem(struct fb_info *info)
{
	struct am_meson_fb *meson_fb;
	struct drm_fb_helper *helper = info->par;
	struct meson_drm_fbdev *fbdev = container_of(helper, struct meson_drm_fbdev, base);
	struct drm_framebuffer *fb = helper->fb;
	struct drm_device *dev = helper->dev;
	size_t size = info->screen_size;
	struct am_meson_gem_object *meson_gem;
	void *vaddr;
	int roundup_flag, ret = 0;

	ret = of_property_read_u32(dev->dev->of_node, "fbdev_size_roundup_flag", &roundup_flag);
	if (!ret && roundup_flag == 1)
		size = roundup(size, (1024 * 1024));

	if (!fbdev->fb_gem) {
		meson_gem = am_meson_gem_object_create(dev, 0, size);
		if (IS_ERR(meson_gem)) {
			DRM_ERROR("alloc memory %d fail\n", (u32)size);
			return -ENOMEM;
		}
		fbdev->fb_gem = &meson_gem->base;
		fb = helper->fb;
		meson_fb = container_of(fb, struct am_meson_fb, base);
		if (!meson_fb) {
			DRM_INFO("meson_fb is NULL!\n");
			return -EINVAL;
		}
		meson_fb->bufp[0] = meson_gem;
		vaddr = ion_heap_map_kernel(meson_gem->ionbuffer->heap,
					meson_gem->ionbuffer);
		info->screen_base = (char __iomem *)vaddr;
		info->fix.smem_start = meson_gem->addr;

		MESON_DRM_FBDEV("alloc memory %d done\n", (u32)size);
	} else {
		MESON_DRM_FBDEV("no need repeate alloc memory %d\n", (u32)size);
	}
	return 0;
}

static void am_meson_fbdev_free_fb_gem(struct fb_info *info)
{
	struct drm_fb_helper *helper = info->par;
	struct meson_drm_fbdev *fbdev;
	struct drm_framebuffer *fb;
	struct am_meson_fb *meson_fb;

	if (!helper) {
		DRM_ERROR("fb helper is NULL!\n");
		return;
	}

	fbdev = container_of(helper, struct meson_drm_fbdev, base);
	if (fbdev && fbdev->fb_gem) {
		struct drm_gem_object *gem_obj = fbdev->fb_gem;
		struct am_meson_gem_object *meson_gem = container_of(gem_obj,
					struct am_meson_gem_object, base);

		ion_heap_unmap_kernel(meson_gem->ionbuffer->heap,
				meson_gem->ionbuffer);
		info->screen_base = NULL;

		am_meson_gem_object_free(fbdev->fb_gem);
		fbdev->fb_gem = NULL;

		fb = helper->fb;
		if (fb) {
			meson_fb = container_of(fb, struct am_meson_fb, base);
			if (meson_fb)
				meson_fb->bufp[0] = NULL;
			else
				DRM_ERROR("meson_fb is NULL!\n");
		} else {
			DRM_ERROR("drm framebuffer is NULL!\n");
		}

		MESON_DRM_FBDEV("free memory done\n");
	} else {
		DRM_DEBUG("memory already free before\n");
	}
}

static int am_meson_fbdev_open(struct fb_info *info, int arg)
{
	int ret = 0;
	struct drm_fb_helper *helper = info->par;
	struct meson_drm_fbdev *fbdev = container_of(helper, struct meson_drm_fbdev, base);
	struct am_osd_plane *osdplane = container_of(fbdev->plane, struct am_osd_plane, base);

	MESON_DRM_FBDEV("%s - %d\n", __func__, osdplane->plane_index);
	ret = am_meson_fbdev_alloc_fb_gem(info);
	return ret;
}

static int am_meson_fbdev_release(struct fb_info *info, int arg)
{
	DRM_DEBUG("may no need to release memory\n");
	return 0;
}

static int am_meson_fbdev_mmap(struct fb_info *info,
			       struct vm_area_struct *vma)
{
	struct drm_fb_helper *helper = info->par;
	struct meson_drm_fbdev *fbdev = container_of(helper, struct meson_drm_fbdev, base);
	struct am_meson_gem_object *meson_gem;

	meson_gem = container_of(fbdev->fb_gem,
				 struct am_meson_gem_object, base);

	return am_meson_gem_object_mmap(meson_gem, vma);
}

static int am_meson_drm_fbdev_sync(struct fb_info *info)
{
	return 0;
}

static int am_meson_drm_fbdev_ioctl(struct fb_info *info,
				    unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	void __user *argp = (void __user *)arg;
	struct fb_dmabuf_export fbdma;
	struct drm_fb_helper *helper = info->par;
	struct meson_drm_fbdev *fbdev = container_of(helper, struct meson_drm_fbdev, base);
	struct drm_plane *plane = fbdev->plane;
	struct am_meson_fb *meson_fb;

	MESON_DRM_FBDEV("[%x] - [%d] IN\n", cmd, plane->index);

	/*amlogic fbdev ioctl, used by gpu fbdev backend.*/
	if (cmd == FBIOGET_OSD_DMABUF) {
		memset(&fbdma, 0, sizeof(fbdma));
		meson_fb = container_of(helper->fb, struct am_meson_fb, base);
		fbdma.fd = dma_buf_fd(meson_fb->bufp[0]->dmabuf, O_CLOEXEC);
		fbdma.flags = O_CLOEXEC;
		ret = copy_to_user(argp, &fbdma, sizeof(fbdma)) ? -EFAULT : 0;
	} else if (cmd == FBIO_WAITFORVSYNC) {
		if (plane->crtc) {
			drm_wait_one_vblank(helper->dev, plane->crtc->index);
		} else if (fbdev->modeset.crtc) {
			drm_wait_one_vblank(helper->dev, fbdev->modeset.crtc->index);
			MESON_DRM_FBDEV("crtc is not set for plane [%d]\n", plane->index);
		}
	} else if (cmd == FBIOPUT_OSD_WINDOW_AXIS) {
		struct drm_meson_fbdev_rect rect;

		if (copy_from_user(&rect, argp, sizeof(rect)))
			return -EFAULT;

		fbdev->dst.x = rect.xstart;
		fbdev->dst.y = rect.ystart;
		fbdev->dst.w = rect.width;
		fbdev->dst.h = rect.height;
		fbdev->dst.isforce = rect.mask;
	} else if (cmd == FBIOGET_DISPLAY_MODE) {
		struct drm_mode_set *mode_set = &fbdev->modeset;
		struct drm_display_mode mode;
		char mode_name[DRM_DISPLAY_MODE_LEN];

		mode = mode_set->crtc->mode;
		strncpy(mode_name, mode.name, DRM_DISPLAY_MODE_LEN);
		mode_name[DRM_DISPLAY_MODE_LEN - 1] = '\0';
		ret = copy_to_user(argp, &mode_name, sizeof(mode_name)) ? -EFAULT : 0;
	}

	MESON_DRM_FBDEV("[%x] - [%d] OUT\n", cmd, plane->index);
	return ret;
}

/**
 * am_meson_drm_fb_helper_check_var - implementation for ->fb_check_var
 * @var: screeninfo to check
 * @info: fbdev registered by the helper
 */
static int am_meson_drm_fb_helper_check_var(struct fb_var_screeninfo *var,
					    struct fb_info *info)
{
	struct drm_fb_helper *fb_helper = info->par;
	struct drm_framebuffer *fb = fb_helper->fb;
	int depth;

	if (var->pixclock != 0 || in_dbg_master()) {
		DRM_ERROR("%s FAILED.\n", __func__);
		return -EINVAL;
	}

	/*
	 * Changes struct fb_var_screeninfo are currently not pushed back
	 * to KMS, hence fail if different settings are requested.
	 */
	MESON_DRM_FBDEV("fb requested w/h/bpp  %dx%d-%d (virtual %dx%d)\n",
		  var->xres, var->yres, var->bits_per_pixel,
		  var->xres_virtual, var->yres_virtual);
	MESON_DRM_FBDEV("current fb w/h/bpp %dx%d-%d\n",
		  fb->width, fb->height, fb->format->depth);
	if (var->bits_per_pixel != fb->format->depth ||
	    var->xres_virtual != fb->width ||
	    var->yres_virtual != fb->height)
		MESON_DRM_FBDEV("%s need realloc buffer\n", __func__);

	switch (var->bits_per_pixel) {
	case 16:
		depth = (var->green.length == 6) ? 16 : 15;
		break;
	case 32:
		depth = (var->transp.length > 0) ? 32 : 24;
		break;
	default:
		depth = var->bits_per_pixel;
		break;
	}

	switch (depth) {
	case 8:
		var->red.offset = 0;
		var->green.offset = 0;
		var->blue.offset = 0;
		var->red.length = 8;
		var->green.length = 8;
		var->blue.length = 8;
		var->transp.length = 0;
		var->transp.offset = 0;
		break;
	case 15:
		var->red.offset = 10;
		var->green.offset = 5;
		var->blue.offset = 0;
		var->red.length = 5;
		var->green.length = 5;
		var->blue.length = 5;
		var->transp.length = 1;
		var->transp.offset = 15;
		break;
	case 16:
		var->red.offset = 11;
		var->green.offset = 5;
		var->blue.offset = 0;
		var->red.length = 5;
		var->green.length = 6;
		var->blue.length = 5;
		var->transp.length = 0;
		var->transp.offset = 0;
		break;
	case 24:
		var->red.offset = 16;
		var->green.offset = 8;
		var->blue.offset = 0;
		var->red.length = 8;
		var->green.length = 8;
		var->blue.length = 8;
		var->transp.length = 0;
		var->transp.offset = 0;
		break;
	case 32:
		var->red.offset = 16;
		var->green.offset = 8;
		var->blue.offset = 0;
		var->red.length = 8;
		var->green.length = 8;
		var->blue.length = 8;
		var->transp.length = 8;
		var->transp.offset = 24;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

/**
 * drm_fb_helper_set_par - implementation for ->fb_set_par
 * @info: fbdev registered by the helper
 *
 * This will let fbcon do the mode init and is called at initialization time by
 * the fbdev core when registering the driver, and later on through the hotplug
 * callback.
 */
int am_meson_drm_fb_helper_set_par(struct fb_info *info)
{
	struct drm_fb_helper *fb_helper = info->par;
	struct fb_var_screeninfo *var = &info->var;
	struct drm_framebuffer *fb = fb_helper->fb;
	struct meson_drm_fbdev *fbdev = container_of(fb_helper, struct meson_drm_fbdev, base);
	struct drm_gem_object *fb_gem = fbdev->fb_gem;
	struct drm_fb_helper_surface_size sizes;
	unsigned int bytes_per_pixel;
	u32 xres_set = var->xres;
	u32 yres_set = var->yres;

	if (oops_in_progress)
		return -EBUSY;

	if (var->pixclock != 0) {
		DRM_ERROR("PIXEL CLOCK SET\n");
		return -EINVAL;
	}

	DRM_INFO("%s IN\n", __func__);
	if (var->bits_per_pixel != fb->format->depth ||
	    var->xres_virtual != fb->width ||
	    var->yres_virtual != fb->height) {
		/*realloc framebuffer, free old then alloc new gem*/
		sizes.fb_height = var->yres_virtual;
		sizes.fb_width = var->xres_virtual;
		sizes.surface_width = sizes.fb_width;
		sizes.surface_height = sizes.fb_height;
		sizes.surface_bpp = var->bits_per_pixel;
		sizes.surface_depth = PREFERRED_DEPTH;

		fb->width = sizes.fb_width;
		fb->height = sizes.fb_height;
		bytes_per_pixel = DIV_ROUND_UP(sizes.surface_bpp, 8);
		fb->pitches[0] =  ALIGN(fb->width * bytes_per_pixel, 64);

		info->screen_size = fb->pitches[0] * fb->height;
		//info->fix.smem_len = info->screen_size;

		drm_fb_helper_fill_info(info, fb_helper, &sizes);
		/* fix some bug in drm_fb_helper_full_info */
		var->xres = xres_set;
		var->yres = yres_set;

		if (fb_gem && fb_gem->size < info->screen_size) {
			MESON_DRM_FBDEV("GEM SIZE is not enough, no re-allocate.\n");
			am_meson_fbdev_free_fb_gem(info);
			fb_gem = NULL;
		}

		if (!fb_gem) {
			if (am_meson_fbdev_alloc_fb_gem(info)) {
				DRM_ERROR("%s realloc fb fail\n", __func__);
				return -ENOMEM;
			}
			MESON_DRM_FBDEV("%s reallocate success.\n", __func__);
		}
	}
	drm_wait_one_vblank(fb_helper->dev, 0);

	DRM_INFO("%s OUT\n", __func__);

	return 0;
}

/* sync of drm_client_modeset_commit_atomic(),
 * 1. add fbdev for non-primary plane.
 * 2. remove crtc set.
 */
static int am_meson_drm_fb_pan_display(struct fb_var_screeninfo *var,
				       struct fb_info *info)
{
	struct drm_fb_helper *fb_helper = info->par;
	struct meson_drm_fbdev *fbdev = container_of(fb_helper, struct meson_drm_fbdev, base);
	struct drm_device *dev = fb_helper->dev;
	struct drm_atomic_state *state;
	struct drm_plane_state *plane_state;
	struct drm_plane *plane = fbdev->plane;
	struct drm_mode_set *mode_set;
	int hdisplay, vdisplay;
	int ret;

	if (fbdev->blank) {
		DRM_DEBUG("%s skip blank.\n", __func__);
		return 0;
	}

	drm_modeset_lock_all(dev);
	MESON_DRM_FBDEV("%s IN [%d]\n", __func__, plane->index);

	state = drm_atomic_state_alloc(dev);
	if (!state) {
		ret = -ENOMEM;
		goto unlock_exit;
	}

	state->acquire_ctx = dev->mode_config.acquire_ctx;
retry:
	MESON_DRM_FBDEV("%s for plane [%d-%p]\n", __func__, plane->type, fb_helper->fb);

	mode_set = &fbdev->modeset;

	/*update plane state, refer to drm_atomic_plane_set_property()*/
	plane_state = drm_atomic_get_plane_state(state, plane);
	if (IS_ERR(plane_state)) {
		ret = PTR_ERR(plane_state);
		goto fail;
	}

	ret = drm_atomic_set_crtc_for_plane(plane_state, mode_set->crtc);
	if (ret != 0) {
		DRM_ERROR("set crtc for plane failed.\n");
		goto fail;
	}

	drm_mode_get_hv_timing(&mode_set->crtc->mode, &hdisplay, &vdisplay);

	if (fbdev->dst.isforce) {
		plane_state->crtc_x = fbdev->dst.x;
		plane_state->crtc_y = fbdev->dst.y;
		plane_state->crtc_w = fbdev->dst.w;
		plane_state->crtc_h = fbdev->dst.h;
	} else {
		plane_state->crtc_x = 0;
		plane_state->crtc_y = 0;
		plane_state->crtc_w = hdisplay;
		plane_state->crtc_h = vdisplay;
	}

	drm_atomic_set_fb_for_plane(plane_state, fb_helper->fb);
	if (fb_helper->fb) {
		plane_state->src_x = var->xoffset << 16;
		plane_state->src_y = var->yoffset << 16;
		plane_state->src_w = var->xres << 16;
		plane_state->src_h = var->yres << 16;
		plane_state->zpos = fbdev->zorder;
	} else {
		plane_state->src_x = 0;
		plane_state->src_y = 0;
		plane_state->src_w = 0;
		plane_state->src_h = 0;
		plane_state->zpos = fbdev->zorder;
	}
	/* fix alpha */
	plane_state->pixel_blend_mode = DRM_MODE_BLEND_COVERAGE;

	MESON_DRM_FBDEV("update fb [%x-%x, %x-%x]-%d->[%d-%d-%d-%d] force-%d",
		plane_state->src_x, plane_state->src_y,
		plane_state->src_w, plane_state->src_h,
		plane_state->zpos,
		plane_state->crtc_x, plane_state->crtc_y,
		plane_state->crtc_w, plane_state->crtc_h,
		fbdev->dst.isforce);

	state->legacy_cursor_update = true;
	ret = drm_atomic_commit(state);
	if (ret != 0)
		goto fail;

	info->var.xoffset = var->xoffset;
	info->var.yoffset = var->yoffset;

fail:
	if (ret == -EDEADLK)
		goto backoff;

	drm_atomic_state_put(state);

unlock_exit:
	drm_modeset_unlock_all(dev);

	if (ret)
		DRM_ERROR("%s failed .\n", __func__);
	else
		MESON_DRM_FBDEV("%s OUT [%d]\n", __func__, plane->index);

	return ret;

backoff:
	drm_atomic_state_clear(state);
	drm_modeset_backoff(state->acquire_ctx);

	goto retry;
}

/**
 * the implement if different from drm_fb_helper.
 * for plane based fbdev, we only disable corresponding plane
 * but not the whole crtc.
 */
int am_meson_drm_fb_blank(int blank, struct fb_info *info)
{
	struct drm_fb_helper *helper = info->par;
	struct meson_drm_fbdev *fbdev = container_of(helper, struct meson_drm_fbdev, base);
	struct drm_device *dev = helper->dev;
	int ret = 0;

	if (blank == 0) {
		MESON_DRM_FBDEV("meson_fbdev[%s] goto UNBLANK.\n", fbdev->plane->name);
		fbdev->blank = false;
		ret = am_meson_drm_fb_pan_display(&info->var, info);
	} else {
		MESON_DRM_FBDEV("meson_fbdev[%s-%p] goto blank.\n",
			fbdev->plane->name, fbdev->plane->fb);
		drm_modeset_lock_all(dev);
		drm_atomic_helper_disable_plane(fbdev->plane, dev->mode_config.acquire_ctx);
		drm_modeset_unlock_all(dev);

		fbdev->blank = true;
	}

	return ret;
}

static struct fb_ops meson_drm_fbdev_ops = {
	.owner		= THIS_MODULE,
	.fb_open        = am_meson_fbdev_open,
	.fb_release     = am_meson_fbdev_release,
	.fb_mmap	= am_meson_fbdev_mmap,
	.fb_fillrect	= drm_fb_helper_cfb_fillrect,
	.fb_copyarea	= drm_fb_helper_cfb_copyarea,
	.fb_imageblit	= drm_fb_helper_cfb_imageblit,
	.fb_check_var	= am_meson_drm_fb_helper_check_var,
	.fb_set_par	= am_meson_drm_fb_helper_set_par,
	.fb_blank	= am_meson_drm_fb_blank,
	.fb_pan_display	= am_meson_drm_fb_pan_display,
	.fb_setcmap	= drm_fb_helper_setcmap,
	.fb_sync	= am_meson_drm_fbdev_sync,
	.fb_ioctl       = am_meson_drm_fbdev_ioctl,
#ifdef CONFIG_COMPAT
	.fb_compat_ioctl = am_meson_drm_fbdev_ioctl,
#endif
};

static int am_meson_drm_fbdev_modeset_create(struct drm_fb_helper *helper)
{
	struct drm_device *dev = helper->dev;
	struct meson_drm_fbdev *fbdev = container_of(helper, struct meson_drm_fbdev, base);

	fbdev->modeset.crtc = drm_crtc_from_index(dev, 0);
	return 0;
}

static int am_meson_drm_fbdev_probe(struct drm_fb_helper *helper,
				     struct drm_fb_helper_surface_size *sizesxx)
{
	struct drm_device *dev = helper->dev;
	struct meson_drm *private = dev->dev_private;
	struct meson_drm_fbdev *fbdev = container_of(helper, struct meson_drm_fbdev, base);
	struct drm_mode_fb_cmd2 mode_cmd = { 0 };
	struct drm_fb_helper_surface_size sizes;
	struct drm_framebuffer *fb;
	struct fb_info *fbi;
	unsigned int bytes_per_pixel;
	int ret;

	sizes.fb_width = private->ui_config.ui_w;
	sizes.fb_height = private->ui_config.ui_h;
	sizes.surface_width = private->ui_config.fb_w;
	sizes.surface_height = private->ui_config.fb_h;
	sizes.surface_bpp = private->ui_config.fb_bpp;
	sizes.surface_depth = PREFERRED_DEPTH;

	bytes_per_pixel = DIV_ROUND_UP(sizes.surface_bpp, 8);
	mode_cmd.width = sizes.surface_width;
	mode_cmd.height = sizes.surface_height;
	mode_cmd.pixel_format = drm_mode_legacy_fb_format(sizes.surface_bpp, PREFERRED_DEPTH);
	mode_cmd.pitches[0] = ALIGN(mode_cmd.width * bytes_per_pixel, 64);

	DRM_INFO("mode_cmd.width = %d\n", mode_cmd.width);
	DRM_INFO("mode_cmd.height = %d\n", mode_cmd.height);
	DRM_INFO("mode_cmd.pixel_format = %d-%d\n", mode_cmd.pixel_format, DRM_FORMAT_ARGB8888);

	fbi = drm_fb_helper_alloc_fbi(helper);
	if (IS_ERR(fbi)) {
		DRM_ERROR("Failed to create framebuffer info.\n");
		ret = PTR_ERR(fbi);
		return ret;
	}

	helper->fb = am_meson_drm_framebuffer_init(dev, &mode_cmd,
						   fbdev->fb_gem);
	if (IS_ERR(helper->fb)) {
		dev_err(dev->dev, "Failed to allocate DRM framebuffer.\n");
		ret = PTR_ERR(helper->fb);
		goto err_release_fbi;
	}
	fb = helper->fb;

	fbi->par = helper;
	fbi->flags = FBINFO_FLAG_DEFAULT;
	fbi->fbops = &meson_drm_fbdev_ops;
	fbi->skip_vt_switch = true;
	fbi->screen_size = fb->pitches[0] * fb->height;
	fbi->fix.smem_len = fbi->screen_size;

	drm_fb_helper_fill_info(fbi, helper, &sizes);
	am_meson_drm_fbdev_modeset_create(helper);

	return 0;

err_release_fbi:
	drm_fb_helper_fini(helper);
	return ret;
}

static const struct drm_fb_helper_funcs meson_drm_fb_helper_funcs = {
	.fb_probe = am_meson_drm_fbdev_probe,
};

static int am_meson_fbdev_parse_config(struct drm_device *dev)
{
	struct meson_drm *private = dev->dev_private;
	u32 sizes[5];
	int ret;

	ret = of_property_read_u32_array(dev->dev->of_node,
				   "fbdev_sizes", sizes, 5);
	if (!ret) {
		private->ui_config.ui_w = sizes[0];
		private->ui_config.ui_h = sizes[1];
		private->ui_config.fb_w = sizes[2];
		private->ui_config.fb_h = sizes[3];
		private->ui_config.fb_bpp = sizes[4];
	}

	return ret;
}

static ssize_t show_force_free_mem(struct device *device,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "Usage: echo 1 > force_free mem\n");
}

static ssize_t store_force_free_mem(struct device *device,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct fb_info *fb_info = dev_get_drvdata(device);

	if (strncmp("1", buf, 1) == 0) {
		am_meson_fbdev_free_fb_gem(fb_info);
		DRM_INFO("fb mem is freed !\n");
	}
	return count;
}

static struct device_attribute fbdev_device_attrs[] = {
	__ATTR(force_free_mem, 0644, show_force_free_mem, store_force_free_mem),
};

struct meson_drm_fbdev *am_meson_create_drm_fbdev(struct drm_device *dev,
					    struct drm_plane *plane)
{
	struct meson_drm *drmdev = dev->dev_private;
	struct meson_drm_fbdev *fbdev;
	struct drm_fb_helper *helper;
	int ret, bpp;
	struct fb_info *fbinfo;
	int i = 0;

	bpp = drmdev->ui_config.fb_bpp;
	fbdev = devm_kzalloc(dev->dev, sizeof(struct meson_drm_fbdev), GFP_KERNEL);
	if (!fbdev)
		return NULL;

	helper = &fbdev->base;

	if (plane)
		fbdev->plane = plane;
	else
		return NULL;

	drm_fb_helper_prepare(dev, helper, &meson_drm_fb_helper_funcs);

	ret = drm_fb_helper_init(dev, helper, MESON_DRM_MAX_CONNECTOR);
	if (ret < 0) {
		dev_err(dev->dev, "Failed to initialize drm fb helper - %d.\n",
			ret);
		goto err_free;
	}

	ret = drm_fb_helper_single_add_all_connectors(helper);
	if (ret < 0) {
		dev_err(dev->dev, "Failed to add connectors - %d.\n", ret);
		goto err_drm_fb_helper_fini;
	}

	ret = drm_fb_helper_initial_config(helper, bpp);
	if (ret < 0) {
		dev_err(dev->dev, "Failed to set initial hw config - %d.\n",
			ret);
		goto err_drm_fb_helper_fini;
	}

	fbinfo = helper->fbdev;
	if (fbinfo && fbinfo->dev) {
		for (i = 0; i < ARRAY_SIZE(fbdev_device_attrs); i++) {
			ret = device_create_file(fbinfo->dev,
						&fbdev_device_attrs[i]);
			if (ret) {
				DRM_ERROR("Failed to create file - %d.\n", ret);
				continue;
			}
		}
		//#if IS_ENABLED(CONFIG_DRM_FBDEV_LEAK_PHYS_SMEM)
		fbinfo->flags &= ~FBINFO_HIDE_SMEM_START;
		//#endif
	}

	fbdev->blank = false;

	DRM_INFO("create fbdev success.\n");
	return fbdev;

err_drm_fb_helper_fini:
	drm_fb_helper_fini(helper);
err_free:
	kfree(fbdev);
	fbdev = NULL;
	DRM_INFO("create drm fbdev failed[%d]\n", ret);
	return NULL;
}

int am_meson_drm_fbdev_init(struct drm_device *dev)
{
	struct meson_drm *drmdev = dev->dev_private;
	struct meson_drm_fbdev *fbdev;
	struct am_osd_plane *osd_plane;
	int i, fbdev_cnt = 1;
	int ret = 0;

	DRM_INFO("%s in\n", __func__);

	ret = am_meson_fbdev_parse_config(dev);
	if (ret) {
		DRM_ERROR("don't find fbdev_sizes, please config it\n");
		return ret;
	}

	if (drmdev->primary_plane) {
		fbdev = am_meson_create_drm_fbdev(dev, drmdev->primary_plane);
		fbdev->zorder = OSD_PLANE_BEGIN_ZORDER;
		DRM_INFO("create fbdev for primary plane [%p]\n", fbdev);
	}

	/*only create fbdev for viu1*/
	for (i = 0; i < MESON_MAX_OSD; i++) {
		osd_plane = drmdev->osd_planes[i];
		if (!osd_plane)
			break;

		if (osd_plane->base.type == DRM_PLANE_TYPE_PRIMARY)
			continue;

		fbdev = am_meson_create_drm_fbdev(dev, &osd_plane->base);
		fbdev->zorder = OSD_PLANE_BEGIN_ZORDER + fbdev_cnt;
		fbdev_cnt++;
		DRM_INFO("create fbdev for plane [%d]-[%p]\n", osd_plane->plane_index, fbdev);
	}

	DRM_INFO("%s create %d out\n", __func__, fbdev_cnt);
	return 0;
}

void am_meson_drm_fbdev_fini(struct drm_device *dev)
{
	struct meson_drm *private = dev->dev_private;
	struct meson_drm_fbdev *fbdev;
	struct drm_fb_helper *helper;
	struct fb_info *fbinfo;
	int i;

	for (i = 0; i < MESON_MAX_OSD; i++) {
		fbdev = private->osd_fbdevs[i];
		if (!fbdev) {
			dev_err(dev->dev, "fbdev is NULL.\n");
			continue;
		}

		helper = &fbdev->base;
		if (!helper || !helper->fbdev) {
			kfree(fbdev);
			dev_err(dev->dev, "helper or fbinfo is NULL.\n");
			continue;
		}

		fbinfo = helper->fbdev;
		if (fbinfo && fbinfo->dev) {
			for (i = 0; i < ARRAY_SIZE(fbdev_device_attrs); i++) {
				device_remove_file(fbinfo->dev,
						&fbdev_device_attrs[i]);
			}
		}

		drm_fb_helper_unregister_fbi(helper);
		drm_fb_helper_fini(helper);
		if (helper->fb)
			drm_framebuffer_put(helper->fb);
		fbdev->fb_gem = NULL;
		drm_fb_helper_fini(helper);
		kfree(fbdev);
	}
}
