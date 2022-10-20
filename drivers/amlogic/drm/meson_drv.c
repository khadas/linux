// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/of_graph.h>
#include <linux/component.h>

#include <uapi/linux/sched/types.h>

#include <drm/drmP.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_atomic_uapi.h>
#include <drm/drm_flip_work.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_plane_helper.h>
#include <drm/drm_gem_cma_helper.h>
#include <drm/drm_fb_cma_helper.h>
#include <drm/drm_gem_framebuffer_helper.h>
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
#include "meson_crtc.h"
#include "meson_sysfs.h"
#include "meson_writeback.h"
#include "meson_logo.h"
#include "meson_async_atomic.h"

#include <linux/amlogic/media/osd/osd_logo.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include "meson_hdmi.h"

#define DRIVER_NAME "meson"
#define DRIVER_DESC "Amlogic Meson DRM driver"
#define MESON_VERSION_MAJOR 2
#define MESON_VERSION_MINOR 0

#define MAX_CONNECTOR_NUM (3)

static void am_meson_fb_output_poll_changed(struct drm_device *dev)
{
#ifdef CONFIG_DRM_MESON_EMULATE_FBDEV
	int i;
	struct meson_drm_fbdev *fbdev;
	struct meson_drm *priv = dev->dev_private;

	for (i = 0; i < MESON_MAX_OSD; i++) {
		fbdev = priv->osd_fbdevs[i];
		if (fbdev)
			drm_fb_helper_hotplug_event(&fbdev->base);
	}
#endif
}

static const struct drm_mode_config_funcs meson_mode_config_funcs = {
	.output_poll_changed = am_meson_fb_output_poll_changed,
	.atomic_check        = drm_atomic_helper_check,
	.atomic_commit       = meson_atomic_commit,
#ifdef CONFIG_DRM_MESON_USE_ION
	.fb_create           = am_meson_fb_create,
#else
	.fb_create           = drm_gem_fb_create,
#endif
};

static const struct drm_mode_config_helper_funcs meson_mode_config_helpers = {
	.atomic_commit_tail = meson_atomic_helper_commit_tail,
};

bool meson_drm_get_scannout_position(struct drm_device *dev,
	unsigned int pipe, bool in_vblank_irq,
	int *vpos, int *hpos, ktime_t *stime, ktime_t *etime,
	const struct drm_display_mode *mode)
{
	int ret = 0;
	struct am_meson_crtc *crtc = to_am_meson_crtc
		(drm_crtc_from_index(dev, pipe));

	ret = crtc->get_scannout_position(crtc,
		in_vblank_irq, vpos, hpos, stime, etime, mode);

	return (ret == 0) ? true : false;
}

#ifdef CONFIG_DRM_MESON_USE_ION
static const struct drm_ioctl_desc meson_ioctls[] = {
	#ifdef CONFIG_DRM_MESON_USE_ION
	DRM_IOCTL_DEF_DRV(MESON_GEM_CREATE, am_meson_gem_create_ioctl,
			  DRM_UNLOCKED | DRM_AUTH | DRM_RENDER_ALLOW),
	#endif
	DRM_IOCTL_DEF_DRV(MESON_ASYNC_ATOMIC, meson_asyc_atomic_ioctl,
			  0),
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
	.get_scanout_position = meson_drm_get_scannout_position,
	.get_vblank_timestamp = drm_calc_vbltimestamp_from_scanoutpos,
#ifdef CONFIG_DEBUG_FS
	.debugfs_init = meson_debugfs_init,
#endif
#ifdef CONFIG_DRM_MESON_USE_ION
	/* PRIME Ops */
	.prime_handle_to_fd	= drm_gem_prime_handle_to_fd,
	.prime_fd_to_handle	= drm_gem_prime_fd_to_handle,

	.gem_prime_export	= am_meson_drm_gem_prime_export,
	.gem_prime_get_sg_table	= am_meson_gem_prime_get_sg_table,

	.gem_prime_import	= am_meson_drm_gem_prime_import,
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
	.dumb_map_offset	= drm_gem_dumb_map_offset,
	.gem_free_object_unlocked = drm_gem_cma_free_object,
	.gem_vm_ops		= &drm_gem_cma_vm_ops,
#endif

	/* Misc */
	.fops			= &fops,
	.name			= DRIVER_NAME,
	.desc			= DRIVER_DESC,
	.date			= "20220613",
	.major			= MESON_VERSION_MAJOR,
	.minor			= MESON_VERSION_MINOR,
};

static int meson_worker_thread_init(struct meson_drm *priv,
				    unsigned int num_crtcs)
{
	int i, ret;
	struct sched_param param;
	struct kthread_worker *worker;
	char thread_name[16];
	struct meson_drm_thread *drm_thread;
	struct drm_device *drm = priv->drm;

	param.sched_priority = 16;

	for (i = 0; i < num_crtcs; i++) {
		drm_thread = &priv->commit_thread[i];
		worker = &drm_thread->worker;
		kthread_init_worker(worker);
		drm_thread->dev = drm;
		snprintf(thread_name, 16, "crtc%d_commit", i);
		drm_thread->thread = kthread_run(kthread_worker_fn,
						 worker, thread_name);
		if (IS_ERR(drm_thread->thread)) {
			DRM_ERROR("failed to create commit thread\n");
			priv->commit_thread[0].thread = NULL;
			return -1;
		}

		ret = sched_setscheduler(drm_thread->thread, SCHED_FIFO, &param);
		if (ret)
			DRM_ERROR("failed to set priority\n");
	}

	return 0;
}

static void meson_parse_max_config(struct device_node *node, u32 *max_width,
				   u32 *max_height)
{
	int ret;
	u32 sizes[2];

	ret = of_property_read_u32_array(node, "max_sizes", sizes, 2);
	if (ret) {
		*max_width = 4096;
		*max_height = 4096;
	} else {
		*max_width = sizes[0];
		*max_height = sizes[1];
	}
}

static int am_meson_drm_bind(struct device *dev)
{
	struct meson_drm *priv;
	struct drm_device *drm;
	struct platform_device *pdev = to_platform_device(dev);
	u32 crtc_masks[ENCODER_MAX];
	u32 max_width, max_height;
	int i, vpu_dma_mask, ret = 0;

	meson_driver.driver_features = DRIVER_HAVE_IRQ | DRIVER_GEM |
		DRIVER_MODESET | DRIVER_ATOMIC | DRIVER_RENDER;

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
	/*bound data to be used by component driver.*/
	priv->bound_data.drm = drm;
	priv->bound_data.connector_component_bind = meson_connector_dev_bind;
	priv->bound_data.connector_component_unbind = meson_connector_dev_unbind;
	priv->osd_occupied_index = -1;
	/*initialize encoders crtc_masks, it will replaced by dts*/
	for (i = 0; i < ENCODER_MAX; i++)
		priv->crtc_masks[i] = 1;

	ret = of_property_read_u32_array(dev->of_node, "crtc_masks",
		crtc_masks, ENCODER_MAX);
	if (ret) {
		DRM_ERROR("crtc_masks get fail!\n");
	} else {
		for (i = 0; i < ENCODER_MAX; i++)
			priv->crtc_masks[i] = crtc_masks[i];
	}

	vpu_dma_mask = 0;
	ret = of_property_read_u32(dev->of_node, "vpu_dma_mask", &vpu_dma_mask);
	if (!ret && vpu_dma_mask == 1) {
		ret = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(64));
		if (ret)
			ret = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(32));

		if (ret)
			DRM_ERROR("drm set dma mask fail\n");
	}

	dev_set_drvdata(dev, priv);

#ifdef CONFIG_DRM_MESON_USE_ION
	ret = am_meson_gem_create(priv);
	if (ret)
		goto err_free2;
#endif

	drm_mode_config_init(drm);

	vpu_topology_init(pdev, priv);

	/* init meson config before bind other component,
	 * other component may use it.
	 */
	meson_parse_max_config(dev->of_node, &max_width, &max_height);
	drm->mode_config.max_width = max_width;
	drm->mode_config.max_height = max_height;
	drm->mode_config.funcs = &meson_mode_config_funcs;
	drm->mode_config.helper_private	= &meson_mode_config_helpers;
	drm->mode_config.allow_fb_modifiers = true;

	/* Try to bind all sub drivers. */
	ret = component_bind_all(dev, &priv->bound_data);
	if (ret)
		goto err_gem;

	/* Writeback should be registered after HDMI registration. */
	ret = am_meson_writeback_create(drm);
	if (ret)
		goto err_gem;
	DRM_INFO("mode_config crtc number:%d\n", drm->mode_config.num_crtc);

	ret = meson_worker_thread_init(priv, drm->mode_config.num_crtc);
	if (ret)
		goto err_unbind_all;

	ret = drm_vblank_init(drm, drm->mode_config.num_crtc);
	if (ret)
		goto err_unbind_all;

	drm_mode_config_reset(drm);
	/*
	 * enable drm irq mode.
	 * - with irq_enabled = true, we can use the vblank feature.
	 */
	drm->irq_enabled = true;

	drm_kms_helper_poll_init(drm);

	am_meson_logo_init(drm);

#ifdef CONFIG_DRM_MESON_EMULATE_FBDEV
	ret = am_meson_drm_fbdev_init(drm);
	if (ret)
		goto err_poll_fini;
#endif
	ret = drm_dev_register(drm, 0);
	if (ret)
		goto err_fbdev_fini;
	ret = meson_drm_sysfs_register(drm);
	if (ret)
		goto err_drm_dev_unregister;

	return 0;

err_drm_dev_unregister:
	drm_dev_unregister(drm);

err_fbdev_fini:
#ifdef CONFIG_DRM_MESON_EMULATE_FBDEV
	am_meson_drm_fbdev_fini(drm);
err_poll_fini:
#endif
	drm_kms_helper_poll_fini(drm);
	drm->irq_enabled = false;
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
	drm_dev_put(drm);

	return ret;
}

static void am_meson_drm_unbind(struct device *dev)
{
	struct drm_device *drm = dev_get_drvdata(dev);

	meson_drm_sysfs_unregister(drm);
	drm_dev_unregister(drm);
#ifdef CONFIG_DRM_MESON_EMULATE_FBDEV
	am_meson_drm_fbdev_fini(drm);
#endif
	drm_kms_helper_poll_fini(drm);
	drm->irq_enabled = false;
	component_unbind_all(dev, drm);
	drm_mode_config_cleanup(drm);
#ifdef CONFIG_DRM_MESON_USE_ION
	am_meson_gem_cleanup(drm->dev_private);
#endif
	drm->dev_private = NULL;
	dev_set_drvdata(dev, NULL);
	drm_dev_put(drm);
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
	meson_driver.driver_features = DRIVER_GEM;

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
	drm_dev_put(drm);
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
	drm_dev_put(drm);

	return 0;
}

static int am_meson_drv_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct device_node *port;
	struct component_match *match = NULL;
	int i;

	DRM_INFO("[%s] in\n", __func__);
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
	DRM_INFO("[%s] out\n", __func__);
#ifdef CONFIG_AMLOGIC_VOUT_SERVE
	disable_vout_mode_set_sysfs();
#endif
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
	{ .compatible = "amlogic, drm-subsystem" },
	{}
};
MODULE_DEVICE_TABLE(of, am_meson_drm_dt_match);

#ifdef CONFIG_PM_SLEEP
static void am_meson_drm_fb_suspend(struct drm_device *drm)
{
#ifdef CONFIG_DRM_MESON_EMULATE_FBDEV
	int i;
	struct meson_drm_fbdev *fbdev;
	struct meson_drm *priv = drm->dev_private;

	for (i = 0; i < MESON_MAX_OSD; i++) {
		fbdev = priv->osd_fbdevs[i];
		if (fbdev)
			drm_fb_helper_set_suspend(&fbdev->base, 1);
	}
#endif
}

static void am_meson_drm_fb_resume(struct drm_device *drm)
{
#ifdef CONFIG_DRM_MESON_EMULATE_FBDEV
	int i;
	struct meson_drm_fbdev *fbdev;
	struct meson_drm *priv = drm->dev_private;

	for (i = 0; i < MESON_MAX_OSD; i++) {
		fbdev = priv->osd_fbdevs[i];
		if (fbdev)
			drm_fb_helper_set_suspend(&fbdev->base, 0);
	}
#endif
}

static int am_meson_drm_pm_suspend(struct device *dev)
{
	struct drm_device *drm;
	struct meson_drm *priv;

	priv = dev_get_drvdata(dev);
	if (!priv) {
		DRM_ERROR("%s: Failed to get meson drm!\n", __func__);
		return 0;
	}
	drm = priv->drm;
	if (!drm) {
		DRM_ERROR("%s: Failed to get drm device!\n", __func__);
		return 0;
	}
	drm_kms_helper_poll_disable(drm);
	am_meson_drm_fb_suspend(drm);
	priv->state = drm_atomic_helper_suspend(drm);
	if (IS_ERR(priv->state)) {
		am_meson_drm_fb_resume(drm);
		drm_kms_helper_poll_enable(drm);
		DRM_INFO("%s: drm_atomic_helper_suspend fail\n", __func__);
		return PTR_ERR(priv->state);
	}
	DRM_INFO("%s: done\n", __func__);
	return 0;
}

static int am_meson_drm_pm_resume(struct device *dev)
{
	struct drm_device *drm;
	struct meson_drm *priv;

	priv = dev_get_drvdata(dev);
	if (!priv) {
		DRM_ERROR("%s: Failed to get meson drm!\n", __func__);
		return 0;
	}
	drm = priv->drm;
	if (!drm) {
		DRM_ERROR("%s: Failed to get drm device!\n", __func__);
		return 0;
	}
	drm_atomic_helper_resume(drm, priv->state);
	am_meson_drm_fb_resume(drm);
	drm_kms_helper_poll_enable(drm);
	DRM_INFO("%s: done\n", __func__);
	return 0;
}
#endif

static const struct dev_pm_ops am_meson_drm_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(am_meson_drm_pm_suspend,
				am_meson_drm_pm_resume)
};

static struct platform_driver am_meson_drm_platform_driver = {
	.probe      = am_meson_drv_probe,
	.remove     = am_meson_drv_remove,
	.driver     = {
		.owner  = THIS_MODULE,
		.name   = DRIVER_NAME,
		.of_match_table = am_meson_drm_dt_match,
		.pm = &am_meson_drm_pm_ops,
	},
};

int __init am_meson_drm_init(void)
{
	return platform_driver_register(&am_meson_drm_platform_driver);
}

void __exit am_meson_drm_exit(void)
{
	platform_driver_unregister(&am_meson_drm_platform_driver);
}

#ifndef MODULE
module_init(am_meson_drm_init);
module_exit(am_meson_drm_exit);
#endif

MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");
