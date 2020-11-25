// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <drm/drm.h>
#include <drm/drmP.h>
#include <drm/drm_fb_helper.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>

#include "meson_drv.h"
#include "meson_gem.h"
#include "meson_fb.h"
#include "meson_fbdev.h"

#define PREFERRED_BPP		32
#define MESON_DRM_MAX_CONNECTOR	2

static int am_meson_fbdev_alloc_fb(struct fb_info *info)
{
	struct drm_fb_helper *helper = info->par;
	struct meson_drm *private;
	struct am_meson_gem_object *meson_gem;
	struct ion_client *client;
	struct drm_device *dev;
	struct am_meson_fb *meson_fb;
	struct drm_framebuffer *fb;
	size_t size;

	private = helper->dev->dev_private;
	client = (struct ion_client *)private->gem_client;
	dev = helper->dev;
	size = info->fix.smem_len;
	if (!private->fbdev_bo) {
		meson_gem = am_meson_gem_object_create(dev, 0, size, client);
		if (IS_ERR(meson_gem)) {
			DRM_ERROR("alloc memory %d fail\n", (u32)size);
			return -ENOMEM;
		}
		private->fbdev_bo = &meson_gem->base;
		fb = helper->fb;
		meson_fb = container_of(fb, struct am_meson_fb, base);
		if (!meson_fb) {
			DRM_INFO("meson_fb is NULL!\n");
			return -EINVAL;
		}
		meson_fb->bufp[0] = meson_gem;
		DRM_DEBUG("alloc memory %d done\n", (u32)size);
	} else {
		DRM_DEBUG("no need repeate alloc memory %d\n", (u32)size);
	}
	return 0;
}

static void am_meson_fbdev_free_fb(struct fb_info *info)
{
	struct drm_fb_helper *helper = info->par;
	struct meson_drm *private;

	private = helper->dev->dev_private;
	if (private->fbdev_bo) {
		am_meson_gem_object_free(private->fbdev_bo);
		private->fbdev_bo = NULL;
		DRM_DEBUG("free memory done\n");
	} else {
		DRM_DEBUG("memory already free before\n");
	}
}

static void am_meson_fbdev_update_fb(struct fb_info *info,
				     struct fb_var_screeninfo *var)
{
	struct drm_fb_helper *helper = info->par;
	struct drm_framebuffer *fb = helper->fb;
	int bytes_per_pixel, depth;

	fb->width = var->xres_virtual;
	fb->height = var->yres_virtual;
	bytes_per_pixel = DIV_ROUND_UP(var->bits_per_pixel, 8);
	fb->pitches[0] = ALIGN(var->xres_virtual * bytes_per_pixel, 64);
	if (var->bits_per_pixel == 32)
		depth = (var->transp.length > 0) ? 32 : 24;
	else
		depth = var->bits_per_pixel;
	fb->pixel_format = drm_mode_legacy_fb_format(var->bits_per_pixel,
						     depth);
}

static int am_meson_fbdev_open(struct fb_info *info, int arg)
{
	int ret = 0;

	DRM_DEBUG("%s\n", __func__);
	ret = am_meson_fbdev_alloc_fb(info);
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
	struct meson_drm *private;
	struct am_meson_gem_object *meson_gem;

	private = helper->dev->dev_private;
	meson_gem = container_of(private->fbdev_bo,
				 struct am_meson_gem_object, base);

	return am_meson_gem_object_mmap(meson_gem, vma);
}

static int am_meson_drm_fbdev_sync(struct fb_info *info)
{
	DRM_ERROR("%s no implement.\n", __func__);
	return 0;
}

static int am_meson_drm_fbdev_ioctl(struct fb_info *info,
				    unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	void __user *argp = (void __user *)arg;
	struct fb_dmabuf_export fbdma;
	struct meson_drm *private;
	struct drm_fb_helper *helper = info->par;
	struct ion_client *client;
	struct am_meson_fb *meson_fb;

	if (cmd == FBIOGET_OSD_DMABUF) {
		private = helper->dev->dev_private;
		client = (struct ion_client *)private->gem_client;
		meson_fb = container_of(helper->fb, struct am_meson_fb, base);
		fbdma.fd = ion_share_dma_buf_fd(client, meson_fb->bufp[0]->handle);
		fbdma.flags = O_CLOEXEC;
		ret = copy_to_user(argp, &fbdma, sizeof(fbdma)) ? -EFAULT : 0;
	} else {
		DRM_ERROR("%s no implement for CMD(%d)\n", __func__, cmd);
	}

	return ret;
}

/*sync from pan_display_atomic to adatp to the case of
 *input and output size is different
 */
static int am_meson_pan_display_atomic(struct fb_var_screeninfo *var,
				       struct fb_info *info)
{
	struct drm_fb_helper *fb_helper = info->par;
	struct drm_device *dev = fb_helper->dev;
	struct drm_atomic_state *state;
	struct drm_plane *plane;
	int i, ret;
	unsigned int plane_mask;

	state = drm_atomic_state_alloc(dev);
	if (!state)
		return -ENOMEM;

	state->acquire_ctx = dev->mode_config.acquire_ctx;
retry:
	plane_mask = 0;
	for (i = 0; i < fb_helper->crtc_count; i++) {
		struct drm_mode_set *mode_set;

		mode_set = &fb_helper->crtc_info[i].mode_set;

		mode_set->x = var->xoffset;
		mode_set->y = var->yoffset;

		ret = __am_meson_drm_set_config(mode_set, state);
		if (ret != 0)
			goto fail;

		plane = mode_set->crtc->primary;
		plane_mask |= (1 << drm_plane_index(plane));
		plane->old_fb = plane->fb;
	}

	ret = drm_atomic_commit(state);
	if (ret != 0)
		goto fail;

	info->var.xoffset = var->xoffset;
	info->var.yoffset = var->yoffset;

fail:
	drm_atomic_clean_old_fb(dev, plane_mask, ret);

	if (ret == -EDEADLK)
		goto backoff;

	if (ret != 0)
		drm_atomic_state_free(state);

	return ret;

backoff:
	drm_atomic_state_clear(state);
	drm_atomic_legacy_backoff(state);

	goto retry;
}

static bool am_meson_drm_fb_helper_is_bound(struct drm_fb_helper *fb_helper)
{
	struct drm_device *dev = fb_helper->dev;
	struct drm_crtc *crtc;
	int bound = 0, crtcs_bound = 0;

	/* Sometimes user space wants everything disabled, so don't steal the
	 * display if there's a master.
	 */
	if (READ_ONCE(dev->master))
		return false;

	drm_for_each_crtc(crtc, dev) {
		if (crtc->primary->fb)
			crtcs_bound++;
		if (crtc->primary->fb == fb_helper->fb)
			bound++;
	}

	if (bound < crtcs_bound)
		return false;

	return true;
}

/*sync from drm_fb_helper_pan_display to adatp to the case of
 *input and output size is different
 */
static int am_meson_drm_fb_pan_display(struct fb_var_screeninfo *var,
				       struct fb_info *info)
{
	struct drm_fb_helper *fb_helper = info->par;
	struct drm_device *dev = fb_helper->dev;
	struct drm_mode_set *modeset;
	int ret = 0;
	int i;

	DRM_DEBUG("%s in\n", __func__);
	if (oops_in_progress)
		return -EBUSY;

	drm_modeset_lock_all(dev);
	if (!am_meson_drm_fb_helper_is_bound(fb_helper)) {
		drm_modeset_unlock_all(dev);
		return -EBUSY;
	}

	if (dev->mode_config.funcs->atomic_commit) {
		ret = am_meson_pan_display_atomic(var, info);
		goto unlock;
	}

	for (i = 0; i < fb_helper->crtc_count; i++) {
		modeset = &fb_helper->crtc_info[i].mode_set;

		modeset->x = var->xoffset;
		modeset->y = var->yoffset;

		if (modeset->num_connectors) {
			ret = drm_mode_set_config_internal(modeset);
			if (!ret) {
				info->var.xoffset = var->xoffset;
				info->var.yoffset = var->yoffset;
			}
		}
	}
unlock:
	drm_modeset_unlock_all(dev);
	DRM_DEBUG("%s out\n", __func__);
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

	if (var->pixclock != 0 || in_dbg_master())
		return -EINVAL;

	/*
	 * Changes struct fb_var_screeninfo are currently not pushed back
	 * to KMS, hence fail if different settings are requested.
	 */
	DRM_DEBUG("fb requested w/h/bpp  %dx%d-%d (virtual %dx%d)\n",
		  var->xres, var->yres, var->bits_per_pixel,
		  var->xres_virtual, var->yres_virtual);
	DRM_DEBUG("current fb w/h/bpp %dx%d-%d\n",
		  fb->width, fb->height, fb->bits_per_pixel);
	if (var->bits_per_pixel != fb->bits_per_pixel ||
	    var->xres_virtual != fb->width ||
	    var->yres_virtual != fb->height)
		DRM_DEBUG("%s need realloc buffer\n", __func__);

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

	if (oops_in_progress)
		return -EBUSY;

	if (var->pixclock != 0) {
		DRM_ERROR("PIXEL CLOCK SET\n");
		return -EINVAL;
	}

	drm_fb_helper_restore_fbdev_mode_unlocked(fb_helper);
	if (var->bits_per_pixel != fb->bits_per_pixel ||
	    var->xres_virtual != fb->width ||
	    var->yres_virtual != fb->height) {
		/*realloc framebuffer, free old then alloc new gem*/
		am_meson_fbdev_free_fb(info);
		am_meson_fbdev_update_fb(info, var);
		if (am_meson_fbdev_alloc_fb(info)) {
			DRM_DEBUG("%s realloc fb fail\n", __func__);
			return -ENOMEM;
		}
		DRM_DEBUG("%s realloc fb done\n", __func__);
	}

	return 0;
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
	.fb_blank	= drm_fb_helper_blank,
	.fb_pan_display	= am_meson_drm_fb_pan_display,
	.fb_setcmap	= drm_fb_helper_setcmap,
	.fb_sync	= am_meson_drm_fbdev_sync,
	.fb_ioctl       = am_meson_drm_fbdev_ioctl,
#ifdef CONFIG_COMPAT
	.fb_compat_ioctl = am_meson_drm_fbdev_ioctl,
#endif
};

static int am_meson_drm_fbdev_create(struct drm_fb_helper *helper,
				     struct drm_fb_helper_surface_size *sizes)
{
	struct meson_drm *private = helper->dev->dev_private;
	struct drm_mode_fb_cmd2 mode_cmd = { 0 };
	struct drm_device *dev = helper->dev;
	struct drm_framebuffer *fb;
	unsigned int bytes_per_pixel;
	unsigned long offset;
	struct fb_info *fbi;
	size_t size;
	int ret;

	bytes_per_pixel = DIV_ROUND_UP(sizes->surface_bpp, 8);

	if (private->ui_config.fb_w)
		mode_cmd.width = private->ui_config.fb_w;
	else if (logo.width)
		mode_cmd.width = logo.width;
	else
		mode_cmd.width = sizes->surface_width;
	if (private->ui_config.fb_h)
		mode_cmd.height = private->ui_config.fb_h;
	else  if (logo.height)
		mode_cmd.height = logo.height;
	else
		mode_cmd.height = sizes->surface_height;
	DRM_INFO("mode_cmd.width = %d\n", mode_cmd.width);
	DRM_INFO("mode_cmd.height = %d\n", mode_cmd.height);

	mode_cmd.pitches[0] = ALIGN(sizes->surface_width * bytes_per_pixel, 64);
	mode_cmd.pixel_format = drm_mode_legacy_fb_format(sizes->surface_bpp,
		sizes->surface_depth);

	size = mode_cmd.pitches[0] * mode_cmd.height;

	fbi = drm_fb_helper_alloc_fbi(helper);
	if (IS_ERR(fbi)) {
		dev_err(dev->dev, "Failed to create framebuffer info.\n");
		ret = PTR_ERR(fbi);
		return ret;
	}

	helper->fb = am_meson_drm_framebuffer_init(dev, &mode_cmd,
						   private->fbdev_bo);
	if (IS_ERR(helper->fb)) {
		dev_err(dev->dev, "Failed to allocate DRM framebuffer.\n");
		ret = PTR_ERR(helper->fb);
		goto err_release_fbi;
	}

	fbi->par = helper;
	fbi->flags = FBINFO_FLAG_DEFAULT;
	fbi->fbops = &meson_drm_fbdev_ops;

	fb = helper->fb;
	fb->bits_per_pixel = sizes->surface_bpp;
	fb->depth = sizes->surface_depth;
	drm_fb_helper_fill_fix(fbi, fb->pitches[0], fb->depth);
	if (private->ui_config.ui_w)
		sizes->fb_width = private->ui_config.ui_w;
	if (private->ui_config.ui_h)
		sizes->fb_height = private->ui_config.ui_h;
	drm_fb_helper_fill_var(fbi, helper, sizes->fb_width, sizes->fb_height);

	offset = fbi->var.xoffset * bytes_per_pixel;
	offset += fbi->var.yoffset * fb->pitches[0];

	dev->mode_config.fb_base = 0;
	fbi->screen_size = size;
	fbi->fix.smem_len = size;

	DRM_DEBUG_KMS("FB [%dx%d]-%d offset=%ld size=%zu\n",
		      fb->width, fb->height, fb->depth, offset, size);

	fbi->skip_vt_switch = true;

	return 0;

err_release_fbi:
	drm_fb_helper_release_fbi(helper);
	return ret;
}

static const struct drm_fb_helper_funcs meson_drm_fb_helper_funcs = {
	.fb_probe = am_meson_drm_fbdev_create,
};

static void am_meson_fbdev_parse_param(struct drm_device *dev)
{
	int ret;
	struct meson_drm *private = dev->dev_private;

	ret = of_property_read_u32(dev->dev->of_node,
				   "fbdev_ui_w", &private->ui_config.ui_w);
	if (ret) {
		DRM_INFO("don't find  match fbdev_ui_w\n");
		private->ui_config.ui_w = 0;
	}
	ret = of_property_read_u32(dev->dev->of_node,
				   "fbdev_ui_h", &private->ui_config.ui_h);
	if (ret) {
		DRM_INFO("don't find  match fbdev_ui_h\n");
		private->ui_config.ui_h = 0;
	}
	ret = of_property_read_u32(dev->dev->of_node,
				   "fbdev_fb_w", &private->ui_config.fb_w);
	if (ret) {
		DRM_INFO("don't find  match fbdev_fb_w\n");
		private->ui_config.fb_w = 0;
	}
	ret = of_property_read_u32(dev->dev->of_node,
				   "fbdev_fb_h", &private->ui_config.fb_h);
	if (ret) {
		DRM_INFO("don't find  match fbdev_fb_h\n");
		private->ui_config.fb_h = 0;
	}
	ret = of_property_read_u32(dev->dev->of_node,
				   "fbdev_fb_bpp",
				   &private->ui_config.fb_bpp);
	if (ret) {
		DRM_INFO("don't find  match fbdev_fb_bpp\n");
		private->ui_config.fb_bpp = 0;
	}
}

int am_meson_drm_fbdev_init(struct drm_device *dev)
{
	struct meson_drm *private = dev->dev_private;
	struct drm_fb_helper *helper;
	unsigned int num_crtc;
	int ret, bpp;

	DRM_INFO("%s in\n", __func__);
	am_meson_fbdev_parse_param(dev);
	if (!dev->mode_config.num_crtc || !dev->mode_config.num_connector)
		return -EINVAL;

	num_crtc = dev->mode_config.num_crtc;

	helper = devm_kzalloc(dev->dev, sizeof(*helper), GFP_KERNEL);
	if (!helper)
		return -ENOMEM;

	drm_fb_helper_prepare(dev, helper, &meson_drm_fb_helper_funcs);

	ret = drm_fb_helper_init(dev, helper, num_crtc,
				 MESON_DRM_MAX_CONNECTOR);
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

	if (private->ui_config.fb_bpp)
		bpp = private->ui_config.fb_bpp;
	else
		bpp = PREFERRED_BPP;
	ret = drm_fb_helper_initial_config(helper, bpp);
	if (ret < 0) {
		dev_err(dev->dev, "Failed to set initial hw config - %d.\n",
			ret);
		goto err_drm_fb_helper_fini;
	}

	private->fbdev_helper = helper;
	DRM_INFO("%s out\n", __func__);

	return 0;

err_drm_fb_helper_fini:
	drm_fb_helper_fini(helper);
err_free:
	kfree(helper);
	return ret;
}

void am_meson_drm_fbdev_fini(struct drm_device *dev)
{
	struct meson_drm *private = dev->dev_private;
	struct drm_fb_helper *helper = private->fbdev_helper;

	if (!helper)
		return;

	drm_fb_helper_unregister_fbi(helper);
	drm_fb_helper_release_fbi(helper);

	if (helper->fb)
		drm_framebuffer_unreference(helper->fb);

	drm_fb_helper_fini(helper);
}
