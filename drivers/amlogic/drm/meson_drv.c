/*
 * drivers/amlogic/drm/meson_drv.c
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/of_graph.h>
#include <linux/component.h>

#include <drm/drmP.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_flip_work.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_plane_helper.h>
#include <drm/drm_gem_cma_helper.h>
#include <drm/drm_fb_cma_helper.h>
#include <drm/drm_rect.h>
#include <drm/drm_fb_helper.h>

#include "meson_fbdev.h"
#ifdef CONFIG_DRM_MESON_USE_ION
#include "meson_gem.h"
#include "meson_fb.h"
#endif
#include "meson_drv.h"
#include "meson_vpu.h"
#include "meson_vpu_pipeline.h"

#define DRIVER_NAME "meson"
#define DRIVER_DESC "Amlogic Meson DRM driver"

static void am_meson_fb_output_poll_changed(struct drm_device *dev)
{
#ifdef CONFIG_DRM_MESON_EMULATE_FBDEV
	struct meson_drm *priv = dev->dev_private;

	drm_fbdev_cma_hotplug_event(priv->fbdev);
#endif
}

static const struct drm_mode_config_funcs meson_mode_config_funcs = {
	.output_poll_changed = am_meson_fb_output_poll_changed,
	.atomic_check        = drm_atomic_helper_check,
	.atomic_commit       = drm_atomic_helper_commit,
#ifdef CONFIG_DRM_MESON_USE_ION
	.fb_create           = am_meson_fb_create,
#else
	.fb_create           = drm_fb_cma_create,
#endif
};

int am_meson_register_crtc_funcs(struct drm_crtc *crtc,
				 const struct meson_crtc_funcs *crtc_funcs)
{
	int pipe = drm_crtc_index(crtc);
	struct meson_drm *priv = crtc->dev->dev_private;

	if (pipe >= MESON_MAX_CRTC)
		return -EINVAL;

	priv->crtc_funcs[pipe] = crtc_funcs;

	return 0;
}
EXPORT_SYMBOL(am_meson_register_crtc_funcs);

void am_meson_unregister_crtc_funcs(struct drm_crtc *crtc)
{
	int pipe = drm_crtc_index(crtc);
	struct meson_drm *priv = crtc->dev->dev_private;

	if (pipe >= MESON_MAX_CRTC)
		return;

	priv->crtc_funcs[pipe] = NULL;
}
EXPORT_SYMBOL(am_meson_unregister_crtc_funcs);

static int am_meson_enable_vblank(struct drm_device *dev, unsigned int crtc)
{
	struct meson_drm *priv = dev->dev_private;

	if (crtc >= MESON_MAX_CRTC)
		return -EBADFD;

	priv->crtc_funcs[crtc]->enable_vblank(priv->crtc);
	return 0;
}

static void am_meson_disable_vblank(struct drm_device *dev, unsigned int crtc)
{
	struct meson_drm *priv = dev->dev_private;

	if (crtc >= MESON_MAX_CRTC)
		return;

	priv->crtc_funcs[crtc]->disable_vblank(priv->crtc);
}

struct am_meson_logo logo;
core_param(fb_width, logo.width, uint, 0644);
core_param(fb_height, logo.height, uint, 0644);
core_param(display_bpp, logo.bpp, uint, 0644);
core_param(outputmode, logo.outputmode_t, charp, 0644);

static struct drm_framebuffer *am_meson_logo_init_fb(struct drm_device *dev)
{
	struct drm_mode_fb_cmd2 mode_cmd;
	struct drm_framebuffer *fb;
	struct am_meson_fb *meson_fb;

	DRM_INFO("width=%d,height=%d,start_addr=0x%pa,size=%d\n",
		 logo.width, logo.height, &logo.start, logo.size);
	DRM_INFO("bpp=%d,alloc_flag=%d\n", logo.bpp, logo.alloc_flag);
	DRM_INFO("outputmode=%s\n", logo.outputmode);
	if (logo.bpp == 16)
		mode_cmd.pixel_format = DRM_FORMAT_RGB565;
	else
		mode_cmd.pixel_format = DRM_FORMAT_XRGB8888;
	mode_cmd.offsets[0] = 0;
	mode_cmd.width = logo.width;
	mode_cmd.height = logo.height;
	mode_cmd.modifier[0] = DRM_FORMAT_MOD_LINEAR;
	/*ToDo*/
	mode_cmd.pitches[0] = ALIGN(mode_cmd.width * logo.bpp, 32) / 8;
	fb = am_meson_fb_alloc(dev, &mode_cmd, NULL);
	if (IS_ERR_OR_NULL(fb))
		return NULL;
	meson_fb = to_am_meson_fb(fb);
	meson_fb->logo = &logo;

	return fb;
}

#define FPS_DELTA_LIMIT 1
struct drm_display_mode *
am_meson_drm_display_mode_init(struct drm_connector *connector)
{
	struct drm_display_mode *mode;
	struct drm_device *dev;
	u32 found, num_modes;

	if (!connector || !connector->dev)
		return NULL;
	dev = connector->dev;
	found = 0;
	drm_modeset_lock_all(dev);
	if (drm_modeset_is_locked(&dev->mode_config.connection_mutex))
		drm_modeset_unlock(&dev->mode_config.connection_mutex);
	num_modes = connector->funcs->fill_modes(connector,
						 dev->mode_config.max_width,
						 dev->mode_config.max_height);
	drm_modeset_unlock_all(dev);
	if (!num_modes) {
		DRM_INFO("%s:num_modes is zero\n", __func__);
		return NULL;
	}
	list_for_each_entry(mode, &connector->modes, head) {
		if (am_meson_crtc_check_mode(mode, logo.outputmode) == true) {
			found = 1;
			break;
		}
	}
	if (found)
		return mode;
	else
		return NULL;
}

static int am_meson_update_output_state(struct drm_atomic_state *state,
					struct drm_mode_set *set)
{
	struct drm_device *dev = set->crtc->dev;
	struct drm_crtc *crtc;
	struct drm_crtc_state *crtc_state;
	struct drm_connector *connector;
	struct drm_connector_state *conn_state;
	int ret, i;

	ret = drm_modeset_lock(&dev->mode_config.connection_mutex,
			       state->acquire_ctx);
	if (ret)
		return ret;

	/* First disable all connectors on the target crtc. */
	ret = drm_atomic_add_affected_connectors(state, set->crtc);
	if (ret)
		return ret;

	for_each_connector_in_state(state, connector, conn_state, i) {
		if (conn_state->crtc == set->crtc) {
			ret = drm_atomic_set_crtc_for_connector(conn_state,
								NULL);
			if (ret)
				return ret;
		}
	}

	/* Then set all connectors from set->connectors on the target crtc */
	for (i = 0; i < set->num_connectors; i++) {
		conn_state = drm_atomic_get_connector_state(state,
							    set->connectors[i]);
		if (IS_ERR(conn_state))
			return PTR_ERR(conn_state);

		ret = drm_atomic_set_crtc_for_connector(conn_state,
							set->crtc);
		if (ret)
			return ret;
	}

	for_each_crtc_in_state(state, crtc, crtc_state, i) {
		/* Don't update ->enable for the CRTC in the set_config request,
		 * since a mismatch would indicate a bug in the upper layers.
		 * The actual modeset code later on will catch any
		 * inconsistencies here.
		 */
		if (crtc == set->crtc)
			continue;

		if (!crtc_state->connector_mask) {
			ret = drm_atomic_set_mode_prop_for_crtc(crtc_state,
								NULL);
			if (ret < 0)
				return ret;

			crtc_state->active = false;
		}
	}

	return 0;
}

static int __am_meson_drm_set_config(struct drm_mode_set *set,
				     struct drm_atomic_state *state)
{
	struct drm_crtc_state *crtc_state;
	struct drm_plane_state *primary_state;
	struct drm_crtc *crtc = set->crtc;
	int hdisplay, vdisplay;
	int ret;

	crtc_state = drm_atomic_get_crtc_state(state, crtc);
	if (IS_ERR(crtc_state))
		return PTR_ERR(crtc_state);

	primary_state = drm_atomic_get_plane_state(state, crtc->primary);
	if (IS_ERR(primary_state))
		return PTR_ERR(primary_state);

	if (!set->mode) {
		WARN_ON(set->fb);
		WARN_ON(set->num_connectors);

		ret = drm_atomic_set_mode_for_crtc(crtc_state, NULL);
		if (ret != 0)
			return ret;

		crtc_state->active = false;

		ret = drm_atomic_set_crtc_for_plane(primary_state, NULL);
		if (ret != 0)
			return ret;

		drm_atomic_set_fb_for_plane(primary_state, NULL);

		goto commit;
	}

	WARN_ON(!set->fb);
	WARN_ON(!set->num_connectors);

	ret = drm_atomic_set_mode_for_crtc(crtc_state, set->mode);
	if (ret != 0)
		return ret;

	crtc_state->active = true;

	ret = drm_atomic_set_crtc_for_plane(primary_state, crtc);
	if (ret != 0)
		return ret;

	drm_crtc_get_hv_timing(set->mode, &hdisplay, &vdisplay);

	drm_atomic_set_fb_for_plane(primary_state, set->fb);
	primary_state->crtc_x = 0;
	primary_state->crtc_y = 0;
	primary_state->crtc_w = hdisplay;
	primary_state->crtc_h = vdisplay;
	primary_state->src_x = set->x << 16;
	primary_state->src_y = set->y << 16;
	if (drm_rotation_90_or_270(primary_state->rotation)) {
		primary_state->src_w = set->fb->height << 16;
		primary_state->src_h = set->fb->width << 16;
	} else {
		primary_state->src_w = set->fb->width << 16;
		primary_state->src_h = set->fb->height << 16;
	}

commit:
	ret = am_meson_update_output_state(state, set);
	if (ret)
		return ret;

	return 0;
}

static int am_meson_drm_set_config(struct drm_mode_set *set)
{
	struct drm_atomic_state *state;
	struct drm_crtc *crtc = set->crtc;
	int ret = 0;

	state = drm_atomic_state_alloc(crtc->dev);
	if (!state)
		return -ENOMEM;

	state->legacy_set_config = true;
	state->acquire_ctx = drm_modeset_legacy_acquire_ctx(crtc);
retry:
	ret = __am_meson_drm_set_config(set, state);
	if (ret != 0)
		goto fail;

	ret = drm_atomic_commit(state);
	if (ret != 0)
		goto fail;

	/* Driver takes ownership of state on successful commit. */
	return 0;
fail:
	if (ret == -EDEADLK)
		goto backoff;

	drm_atomic_state_free(state);

	return ret;
backoff:
	drm_atomic_state_clear(state);
	drm_atomic_legacy_backoff(state);

	/*
	 * Someone might have exchanged the framebuffer while we dropped locks
	 * in the backoff code. We need to fix up the fb refcount tracking the
	 * core does for us.
	 */
	crtc->primary->old_fb = crtc->primary->fb;

	goto retry;
}

static void am_meson_load_logo(struct drm_device *dev)
{
	struct drm_mode_set set;
	struct drm_framebuffer *fb;
	struct drm_display_mode *mode;
	struct drm_connector **connector_set;
	struct meson_drm *private = dev->dev_private;

	if (!logo.alloc_flag) {
		DRM_INFO("%s: logo memory is not cma alloc\n", __func__);
		return;
	}
	fb = am_meson_logo_init_fb(dev);
	if (!fb) {
		DRM_INFO("%s:framebuffer is NULL!\n", __func__);
		return;
	}
	connector_set = kmalloc_array(1, sizeof(struct drm_connector *),
				      GFP_KERNEL);
	if (!connector_set)
		return;
	connector_set[0] = am_meson_hdmi_connector();
	if (!connector_set[0]) {
		DRM_INFO("%s:connector is NULL!\n", __func__);
		kfree(connector_set);
		return;
	}
	mode = am_meson_drm_display_mode_init(connector_set[0]);
	if (!mode) {
		DRM_INFO("%s:display mode is NULL!\n", __func__);
		kfree(connector_set);
		return;
	}
	DRM_INFO("find the match display mode:%s\n", mode->name);
	set.crtc = private->crtc;
	set.x = 0;
	set.y = 0;
	set.mode = mode;
	set.connectors = connector_set;
	set.num_connectors = 1;
	set.fb = fb;
	drm_modeset_lock_all(dev);
	if (am_meson_drm_set_config(&set))
		DRM_INFO("[%s]am_meson_drm_set_config fail\n", __func__);
	if (drm_framebuffer_read_refcount(fb) > 1)
		drm_framebuffer_unreference(fb);
	drm_modeset_unlock_all(dev);

	kfree(connector_set);
}

#ifdef CONFIG_DRM_MESON_USE_ION
static const struct drm_ioctl_desc meson_ioctls[] = {
	DRM_IOCTL_DEF_DRV(MESON_GEM_CREATE, am_meson_gem_create_ioctl,
			  DRM_UNLOCKED | DRM_AUTH | DRM_RENDER_ALLOW),
};
#endif

static const struct file_operations fops = {
	.owner		= THIS_MODULE,
	.open		= drm_open,
	.release	= drm_release,
	.unlocked_ioctl	= drm_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= drm_compat_ioctl,
#endif
	.poll		= drm_poll,
	.read		= drm_read,
	.llseek		= no_llseek,
#ifdef CONFIG_DRM_MESON_USE_ION
	.mmap		= am_meson_gem_mmap,
#else
	.mmap		= drm_gem_cma_mmap,
#endif
};

static struct drm_driver meson_driver = {
	/*driver_features setting move to probe functions*/
	.driver_features	= 0,
	/* Vblank */
	.enable_vblank		= am_meson_enable_vblank,
	.disable_vblank		= am_meson_disable_vblank,
	.get_vblank_counter	= drm_vblank_no_hw_counter,
#ifdef CONFIG_DEBUG_FS
	.debugfs_init = meson_debugfs_init,
	.debugfs_cleanup = meson_debugfs_cleanup,
#endif
#ifdef CONFIG_DRM_MESON_USE_ION
	/* PRIME Ops */
	.prime_handle_to_fd	= drm_gem_prime_handle_to_fd,
	.prime_fd_to_handle	= drm_gem_prime_fd_to_handle,

	.gem_prime_export	= drm_gem_prime_export,
	.gem_prime_get_sg_table	= am_meson_gem_prime_get_sg_table,

	.gem_prime_import	= drm_gem_prime_import,
	/*
	 * If gem_prime_import_sg_table is NULL,only buffer created
	 * by meson driver can be imported ok.
	 */
	.gem_prime_import_sg_table = am_meson_gem_prime_import_sg_table,

	.gem_prime_vmap		= am_meson_gem_prime_vmap,
	.gem_prime_vunmap	= am_meson_gem_prime_vunmap,
	.gem_prime_mmap		= am_meson_gem_prime_mmap,

	/* GEM Ops */
	.dumb_create			= am_meson_gem_dumb_create,
	.dumb_destroy		= am_meson_gem_dumb_destroy,
	.dumb_map_offset		= am_meson_gem_dumb_map_offset,
	.gem_free_object_unlocked	= am_meson_gem_object_free,
	.gem_vm_ops			= &drm_gem_cma_vm_ops,
	.ioctls			= meson_ioctls,
	.num_ioctls		= ARRAY_SIZE(meson_ioctls),
#else
	/* PRIME Ops */
	.prime_handle_to_fd	= drm_gem_prime_handle_to_fd,
	.prime_fd_to_handle	= drm_gem_prime_fd_to_handle,
	.gem_prime_import	= drm_gem_prime_import,
	.gem_prime_export	= drm_gem_prime_export,
	.gem_prime_get_sg_table	= drm_gem_cma_prime_get_sg_table,
	.gem_prime_import_sg_table = drm_gem_cma_prime_import_sg_table,
	.gem_prime_vmap		= drm_gem_cma_prime_vmap,
	.gem_prime_vunmap	= drm_gem_cma_prime_vunmap,
	.gem_prime_mmap		= drm_gem_cma_prime_mmap,

	/* GEM Ops */
	.dumb_create		= drm_gem_cma_dumb_create,
	.dumb_destroy		= drm_gem_dumb_destroy,
	.dumb_map_offset	= drm_gem_cma_dumb_map_offset,
	.gem_free_object_unlocked = drm_gem_cma_free_object,
	.gem_vm_ops		= &drm_gem_cma_vm_ops,
#endif

	/* Misc */
	.fops			= &fops,
	.name			= DRIVER_NAME,
	.desc			= DRIVER_DESC,
	.date			= "20180321",
	.major			= 1,
	.minor			= 0,
};

static int am_meson_drm_bind(struct device *dev)
{
	struct meson_drm *priv;
	struct drm_device *drm;
	struct platform_device *pdev = to_platform_device(dev);
	int ret = 0;

	meson_driver.driver_features = DRIVER_HAVE_IRQ | DRIVER_GEM |
		DRIVER_MODESET | DRIVER_PRIME |
		DRIVER_ATOMIC | DRIVER_IRQ_SHARED;

	drm = drm_dev_alloc(&meson_driver, dev);
	if (!drm)
		return -ENOMEM;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		ret = -ENOMEM;
		goto err_free1;
	}
	drm->dev_private = priv;
	priv->drm = drm;
	priv->dev = dev;
	dev_set_drvdata(dev, priv);

#ifdef CONFIG_DRM_MESON_USE_ION
	ret = am_meson_gem_create(priv);
	if (ret)
		goto err_free2;
#endif

	vpu_topology_init(pdev, priv);
	meson_vpu_block_state_init(priv, priv->pipeline);

	drm_mode_config_init(drm);

	/* Try to bind all sub drivers. */
	ret = component_bind_all(dev, drm);
	if (ret)
		goto err_gem;
	DRM_INFO("mode_config crtc number:%d\n", drm->mode_config.num_crtc);

	ret = drm_vblank_init(drm, drm->mode_config.num_crtc);
	if (ret)
		goto err_unbind_all;

	drm_mode_config_reset(drm);
	drm->mode_config.max_width = 4096;
	drm->mode_config.max_height = 4096;
	drm->mode_config.funcs = &meson_mode_config_funcs;
	drm->mode_config.allow_fb_modifiers = true;
	/*
	 * irq will init in each crtc, just mark the enable flag here.
	 */
	drm->irq_enabled = true;

	drm_kms_helper_poll_init(drm);

	am_meson_load_logo(drm);

#ifdef CONFIG_DRM_MESON_EMULATE_FBDEV
	ret = am_meson_drm_fbdev_init(drm);
	if (ret)
		goto err_poll_fini;
#endif
	ret = drm_dev_register(drm, 0);
	if (ret)
		goto err_fbdev_fini;

	return 0;

err_fbdev_fini:
#ifdef CONFIG_DRM_MESON_EMULATE_FBDEV
	am_meson_drm_fbdev_fini(drm);
err_poll_fini:
#endif
	drm_kms_helper_poll_fini(drm);
	drm->irq_enabled = false;
	drm_vblank_cleanup(drm);
err_unbind_all:
	component_unbind_all(dev, drm);
err_gem:
	drm_mode_config_cleanup(drm);
#ifdef CONFIG_DRM_MESON_USE_ION
	am_meson_gem_cleanup(drm->dev_private);
err_free2:
#endif
	drm->dev_private = NULL;
	dev_set_drvdata(dev, NULL);
err_free1:
	drm_dev_unref(drm);

	return ret;
}

static void am_meson_drm_unbind(struct device *dev)
{
	struct drm_device *drm = dev_get_drvdata(dev);

	drm_dev_unregister(drm);
#ifdef CONFIG_DRM_MESON_EMULATE_FBDEV
	am_meson_drm_fbdev_fini(drm);
#endif
	drm_kms_helper_poll_fini(drm);
	drm->irq_enabled = false;
	drm_vblank_cleanup(drm);
	component_unbind_all(dev, drm);
	drm_mode_config_cleanup(drm);
#ifdef CONFIG_DRM_MESON_USE_ION
	am_meson_gem_cleanup(drm->dev_private);
#endif
	drm->dev_private = NULL;
	dev_set_drvdata(dev, NULL);
	drm_dev_unref(drm);
}

static int compare_of(struct device *dev, void *data)
{
	struct device_node *np = data;

	return dev->of_node == np;
}

static void am_meson_add_endpoints(struct device *dev,
				   struct component_match **match,
				   struct device_node *port)
{
	struct device_node *ep, *remote;

	for_each_child_of_node(port, ep) {
		remote = of_graph_get_remote_port_parent(ep);
		if (!remote || !of_device_is_available(remote)) {
			of_node_put(remote);
			continue;
		} else if (!of_device_is_available(remote->parent)) {
			of_node_put(remote);
			continue;
		}
		component_match_add(dev, match, compare_of, remote);
		of_node_put(remote);
	}
}

static const struct component_master_ops am_meson_drm_ops = {
	.bind = am_meson_drm_bind,
	.unbind = am_meson_drm_unbind,
};

static bool am_meson_drv_use_osd(void)
{
	struct device_node *node;
	const  char *str;
	int ret;

	node = of_find_node_by_path("/meson-fb");
	if (node) {
		ret = of_property_read_string(node, "status", &str);
		if (ret) {
			DRM_INFO("get 'status' failed:%d\n", ret);
			return false;
		}

		if (strcmp(str, "okay") && strcmp(str, "ok")) {
			DRM_INFO("device %s status is %s\n",
				 node->name, str);
		} else {
			DRM_INFO("device %s status is %s\n",
				 node->name, str);
			return true;
		}
	}
	return false;
}

static int am_meson_drv_probe_prune(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct meson_drm *priv;
	struct drm_device *drm;
	int ret;

	/*driver_features reset to DRIVER_GEM | DRIVER_PRIME, for prune drm*/
	meson_driver.driver_features = DRIVER_GEM | DRIVER_PRIME;

	drm = drm_dev_alloc(&meson_driver, dev);
	if (!drm)
		return -ENOMEM;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		ret = -ENOMEM;
		goto err_free1;
	}
	drm->dev_private = priv;
	priv->drm = drm;
	priv->dev = dev;

	platform_set_drvdata(pdev, priv);

#ifdef CONFIG_DRM_MESON_USE_ION
	ret = am_meson_gem_create(priv);
	if (ret)
		goto err_free2;
#endif

	ret = drm_dev_register(drm, 0);
	if (ret)
		goto err_gem;

	return 0;

err_gem:
#ifdef CONFIG_DRM_MESON_USE_ION
	am_meson_gem_cleanup(drm->dev_private);
err_free2:
#endif
	drm->dev_private = NULL;
	platform_set_drvdata(pdev, NULL);
err_free1:
	drm_dev_unref(drm);
	return ret;
}

static int am_meson_drv_remove_prune(struct platform_device *pdev)
{
	struct drm_device *drm = platform_get_drvdata(pdev);

	drm_dev_unregister(drm);
#ifdef CONFIG_DRM_MESON_USE_ION
	am_meson_gem_cleanup(drm->dev_private);
#endif
	drm->dev_private = NULL;
	platform_set_drvdata(pdev, NULL);
	drm_dev_unref(drm);

	return 0;
}

static int am_meson_drv_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct device_node *port;
	struct component_match *match = NULL;
	int i;

	pr_info("[%s] in\n", __func__);
	if (am_meson_drv_use_osd())
		return am_meson_drv_probe_prune(pdev);

	if (!np)
		return -ENODEV;

	/*
	 * Bind the crtc ports first, so that
	 * drm_of_find_possible_crtcs called from encoder .bind callbacks
	 * works as expected.
	 */
	for (i = 0;; i++) {
		port = of_parse_phandle(np, "ports", i);
		if (!port)
			break;

		if (!of_device_is_available(port->parent)) {
			of_node_put(port);
			continue;
		}

		component_match_add(dev, &match, compare_of, port->parent);
		of_node_put(port);
	}

	if (i == 0) {
		dev_err(dev, "missing 'ports' property.\n");
		return -ENODEV;
	}

	if (!match) {
		dev_err(dev, "No available vout found for display-subsystem.\n");
		return -ENODEV;
	}

	/*
	 * For each bound crtc, bind the encoders attached to its
	 * remote endpoint.
	 */
	for (i = 0;; i++) {
		port = of_parse_phandle(np, "ports", i);
		if (!port)
			break;

		if (!of_device_is_available(port->parent)) {
			of_node_put(port);
			continue;
		}

		am_meson_add_endpoints(dev, &match, port);
		of_node_put(port);
	}
	pr_info("[%s] out\n", __func__);
	return component_master_add_with_match(dev, &am_meson_drm_ops, match);
}

static int am_meson_drv_remove(struct platform_device *pdev)
{
	if (am_meson_drv_use_osd())
		return am_meson_drv_remove_prune(pdev);

	component_master_del(&pdev->dev, &am_meson_drm_ops);
	return 0;
}

static const struct of_device_id am_meson_drm_dt_match[] = {
	{ .compatible = "amlogic,drm-subsystem" },
	{}
};
MODULE_DEVICE_TABLE(of, am_meson_drm_dt_match);

static struct platform_driver am_meson_drm_platform_driver = {
	.probe      = am_meson_drv_probe,
	.remove     = am_meson_drv_remove,
	.driver     = {
		.owner  = THIS_MODULE,
		.name   = DRIVER_NAME,
		.of_match_table = am_meson_drm_dt_match,
	},
};

module_platform_driver(am_meson_drm_platform_driver);

MODULE_AUTHOR("Jasper St. Pierre <jstpierre@mecheye.net>");
MODULE_AUTHOR("Neil Armstrong <narmstrong@baylibre.com>");
MODULE_AUTHOR("MultiMedia Amlogic <multimedia-sh@amlogic.com>");
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");
