// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <drm/drmP.h>
#include <drm/drm_plane.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_fb_cma_helper.h>
#include <drm/drm_gem_cma_helper.h>

#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/of.h>
#include <linux/component.h>
#include <linux/of_reserved_mem.h>
#include <linux/dma-contiguous.h>
#include <linux/cma.h>
#ifdef CONFIG_DRM_MESON_USE_ION
#include <ion/ion_private.h>
#endif

/* Amlogic Headers */
#include <linux/amlogic/media/vout/vout_notify.h>
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT
#include <linux/amlogic/media/amvecm/amvecm.h>
#endif
#ifdef CONFIG_DRM_MESON_USE_ION
#include "meson_fb.h"
#endif
#include "meson_vpu.h"
#include "meson_plane.h"
#include "meson_crtc.h"
#include "meson_vpu_pipeline.h"

#define AM_VOUT_NULL_MODE "null"
static struct platform_device *gp_dev;
static unsigned long gem_mem_start, gem_mem_size;

static int am_meson_crtc_loader_protect(struct drm_crtc *crtc, bool on)
{
	struct am_meson_crtc *amcrtc = to_am_meson_crtc(crtc);

	DRM_INFO("%s  %d\n", __func__, on);

	if (on) {
		enable_irq(amcrtc->irq);
		drm_crtc_vblank_on(crtc);
	} else {
		disable_irq(amcrtc->irq);
		drm_crtc_vblank_off(crtc);
	}

	return 0;
}

static int am_meson_crtc_enable_vblank(struct drm_crtc *crtc)
{
	return 0;
}

static void am_meson_crtc_disable_vblank(struct drm_crtc *crtc)
{
}

const struct meson_crtc_funcs meson_private_crtc_funcs = {
	.loader_protect = am_meson_crtc_loader_protect,
	.enable_vblank = am_meson_crtc_enable_vblank,
	.disable_vblank = am_meson_crtc_disable_vblank,
};

char *am_meson_crtc_get_voutmode(struct drm_display_mode *mode)
{
	struct vinfo_s *vinfo;
	char *name = NULL;

	vinfo = get_current_vinfo();

	if (vinfo && vinfo->mode == VMODE_LCD)
		return mode->name;
#ifdef CONFIG_DRM_MESON_HDMI
	name = am_meson_hdmi_get_voutmode(mode);
#endif
#ifdef CONFIG_DRM_MESON_CVBS
	if (!name)
		name = am_cvbs_get_voutmode(mode);
#endif
	if (!name)
		return AM_VOUT_NULL_MODE;
	else
		return name;
}

void am_meson_crtc_handle_vsync(struct am_meson_crtc *amcrtc)
{
	unsigned long flags;
	struct drm_crtc *crtc;
	int i;

	crtc = &amcrtc->base;
	drm_crtc_handle_vblank(crtc);
	for (i = 0; i < VIDEO_LATENCY_VSYNC; i++) {
		if (!amcrtc->video_fence[i].fence)
			continue;
		atomic_dec(&amcrtc->video_fence[i].refcount);
		if (!atomic_read(&amcrtc->video_fence[i].refcount)) {
			dma_fence_signal(amcrtc->video_fence[i].fence);
			dma_fence_put(amcrtc->video_fence[i].fence);
			amcrtc->video_fence[i].fence = NULL;
			DRM_DEBUG("video fence signal done index:%d\n", i);
		}
	}
	spin_lock_irqsave(&crtc->dev->event_lock, flags);
	if (amcrtc->event) {
		drm_crtc_send_vblank_event(crtc, amcrtc->event);
		amcrtc->event = NULL;
	}
	spin_unlock_irqrestore(&crtc->dev->event_lock, flags);
}

void am_meson_crtc_irq(struct meson_drm *priv)
{
	struct am_meson_crtc *amcrtc = to_am_meson_crtc(priv->crtc);

	am_meson_crtc_handle_vsync(amcrtc);
}

static irqreturn_t am_meson_vpu_irq(int irq, void *arg)
{
	struct drm_device *dev = arg;
	struct meson_drm *priv = dev->dev_private;

	am_meson_crtc_irq(priv);

	return IRQ_HANDLED;
}

void am_meson_free_logo_memory(void)
{
	phys_addr_t logo_addr = page_to_phys(logo.logo_page);

	if (logo.size > 0) {
#ifdef CONFIG_CMA
		DRM_INFO("%s, free memory: addr:0x%pa,size:0x%x\n",
			 __func__, &logo_addr, logo.size);

		dma_release_from_contiguous(&gp_dev->dev,
					    logo.logo_page,
					    logo.size >> PAGE_SHIFT);
#endif
	}
	logo.alloc_flag = 0;
}

static int am_meson_logo_info_update(struct meson_drm *priv)
{
	logo.start = page_to_phys(logo.logo_page);
	logo.alloc_flag = 1;
	/*config 1080p logo as default*/
	if (!logo.width || !logo.height) {
		logo.width = 1920;
		logo.height = 1080;
	}
	if (!logo.bpp)
		logo.bpp = 16;
	if (!logo.outputmode_t) {
		strcpy(logo.outputmode, "1080p60hz");
	} else {
		strncpy(logo.outputmode, logo.outputmode_t, VMODE_NAME_LEN_MAX);
		logo.outputmode[VMODE_NAME_LEN_MAX - 1] = '\0';
	}
	priv->logo = &logo;

	return 0;
}

static void am_meson_vpu_power_config(bool en)
{
	meson_vpu_power_config(VPU_MAIL_AFBCD, en);
	meson_vpu_power_config(VPU_VIU_OSD2, en);
	meson_vpu_power_config(VPU_VIU_OSD_SCALE, en);
	meson_vpu_power_config(VPU_VD2_OSD2_SCALE, en);
	meson_vpu_power_config(VPU_VIU_OSD3, en);
	meson_vpu_power_config(VPU_OSD_BLD34, en);
	meson_vpu_power_config(VPU_VIU_OSD2, en);

	meson_vpu_power_config(VPU_VIU2_OSD1, en);
	meson_vpu_power_config(VPU_VIU2_OSD1, en);
	meson_vpu_power_config(VPU_VIU2_OSD_ROT, en);
}

static int am_meson_vpu_bind(struct device *dev,
			     struct device *master, void *data)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct drm_device *drm_dev = data;
	struct meson_drm *private = drm_dev->dev_private;
	struct meson_vpu_pipeline *pipeline = private->pipeline;
	struct am_meson_crtc *amcrtc;
#ifdef CONFIG_CMA
	struct cma *cma;
	struct reserved_mem *rmem = NULL;
	struct device_node *np, *mem_node;
#endif
	int ret, irq;

	/* Allocate crtc struct */
	DRM_INFO("[%s] in\n", __func__);
	amcrtc = devm_kzalloc(dev, sizeof(*amcrtc),
			      GFP_KERNEL);
	if (!amcrtc)
		return -ENOMEM;

	amcrtc->priv = private;
	amcrtc->dev = dev;
	amcrtc->drm_dev = drm_dev;

	dev_set_drvdata(dev, amcrtc);

	/* init reserved memory */
	ret = of_reserved_mem_device_init(&pdev->dev);
	if (ret != 0) {
		dev_err(dev, "failed to init reserved memory\n");
	} else {
#ifdef CONFIG_CMA
		np = pdev->dev.of_node;
		mem_node = of_parse_phandle(np, "memory-region", 0);
		if (mem_node) {
			rmem = of_reserved_mem_lookup(mem_node);
			of_node_put(mem_node);
			if (rmem) {
				logo.size = rmem->size;
				DRM_INFO("of read reservememsize=0x%x\n",
					 logo.size);
			}
		} else {
			DRM_ERROR("no memory-region\n");
		}
		gp_dev = pdev;
		cma = dev_get_cma_area(&pdev->dev);
		if (cma) {
			if (logo.size > 0) {
				logo.logo_page =
				dma_alloc_from_contiguous(&pdev->dev,
							  logo.size >>
							  PAGE_SHIFT,
							  0, 0);
				if (!logo.logo_page)
					DRM_INFO("allocate buffer failed\n");
				else
					am_meson_logo_info_update(private);
			}
		} else {
			DRM_INFO("------ NO CMA\n");
		}
#endif
		if (gem_mem_start) {
			dma_declare_coherent_memory(drm_dev->dev,
						    gem_mem_start,
						    gem_mem_start,
						    gem_mem_size);
			pr_info("meson drm mem_start = 0x%x, size = 0x%x\n",
				(u32)gem_mem_start, (u32)gem_mem_size);
		} else {
			DRM_INFO("------ NO reserved dma\n");
		}
	}

	ret = am_meson_plane_create(private);
	if (ret)
		return ret;

	ret = am_meson_crtc_create(amcrtc);
	if (ret)
		return ret;

	am_meson_register_crtc_funcs(private->crtc, &meson_private_crtc_funcs);

	ret = of_property_read_u8(dev->of_node,
				  "osd_ver", &pipeline->osd_version);

	if (0)
		am_meson_vpu_power_config(1);
	else
		osd_vpu_power_on();

	vpu_pipeline_init(pipeline);

	/*vsync irq.*/
	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(dev, "cannot find irq for vpu\n");
		return irq;
	}
	amcrtc->irq = (unsigned int)irq;

	ret = devm_request_irq(dev, amcrtc->irq, am_meson_vpu_irq,
			       IRQF_SHARED, dev_name(dev), drm_dev);
	if (ret)
		return ret;
	/* IRQ is initially disabled; it gets enabled in crtc_enable */
	disable_irq(amcrtc->irq);
	DRM_INFO("[%s] out\n", __func__);
	return 0;
}

static void am_meson_vpu_unbind(struct device *dev,
				struct device *master, void *data)
{
	struct drm_device *drm_dev = data;
	struct meson_drm *private = drm_dev->dev_private;

	am_meson_unregister_crtc_funcs(private->crtc);
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT
	amvecm_drm_gamma_disable(0);
	am_meson_ctm_disable();
#endif
	am_meson_vpu_power_config(0);
}

static const struct component_ops am_meson_vpu_component_ops = {
	.bind = am_meson_vpu_bind,
	.unbind = am_meson_vpu_unbind,
};

static const struct of_device_id am_meson_vpu_driver_dt_match[] = {
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
	{ .compatible = "amlogic, meson-gxbb-vpu",},
	{ .compatible = "amlogic, meson-gxl-vpu",},
	{ .compatible = "amlogic, meson-gxm-vpu",},
	{ .compatible = "amlogic, meson-txl-vpu",},
	{ .compatible = "amlogic, meson-txlx-vpu",},
	{ .compatible = "amlogic, meson-axg-vpu",},
	{.compatible = "amlogic, meson-tl1-vpu",},
#endif
	{ .compatible = "amlogic, meson-g12a-vpu",},
	{ .compatible = "amlogic, meson-g12b-vpu",},
	{.compatible = "amlogic, meson-sm1-vpu",},
	{.compatible = "amlogic, meson-tm2-vpu",},
	{.compatible = "amlogic, meson-t5-vpu",},
	{.compatible = "amlogic, meson-sc2-vpu",},
	{.compatible = "amlogic, meson-s4-vpu",},
	{}
};

MODULE_DEVICE_TABLE(of, am_meson_vpu_driver_dt_match);

static int am_meson_vpu_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	pr_info("[%s] in\n", __func__);
	if (!dev->of_node) {
		dev_err(dev, "can't find vpu devices\n");
		return -ENODEV;
	}
	DRM_INFO("[%s] out\n", __func__);
	return component_add(dev, &am_meson_vpu_component_ops);
}

static int am_meson_vpu_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &am_meson_vpu_component_ops);

	return 0;
}

static struct platform_driver am_meson_vpu_platform_driver = {
	.probe = am_meson_vpu_probe,
	.remove = am_meson_vpu_remove,
	.driver = {
		.name = "meson-vpu",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(am_meson_vpu_driver_dt_match),
	},
};

static int gem_mem_device_init(struct reserved_mem *rmem, struct device *dev)
{
	s32 ret = 0;

	if (!rmem) {
		pr_info("Can't get reverse mem!\n");
		ret = -EFAULT;
		return ret;
	}
	gem_mem_start = rmem->base;
	gem_mem_size = rmem->size;
	pr_info("init gem memsource addr:0x%x size:0x%x\n",
		(u32)gem_mem_start, (u32)gem_mem_size);

	return 0;
}

static const struct reserved_mem_ops rmem_gem_ops = {
	.device_init = gem_mem_device_init,
};

static int __init gem_mem_setup(struct reserved_mem *rmem)
{
	rmem->ops = &rmem_gem_ops;
	pr_info("gem mem setup\n");
	return 0;
}

RESERVEDMEM_OF_DECLARE(gem, "amlogic, gem_memory", gem_mem_setup);

int __init am_meson_vpu_init(void)
{
	return platform_driver_register(&am_meson_vpu_platform_driver);
}

void __exit am_meson_vpu_exit(void)
{
	platform_driver_unregister(&am_meson_vpu_platform_driver);
}

#ifndef MODULE
module_init(am_meson_vpu_init);
module_exit(am_meson_vpu_exit);
#endif

MODULE_AUTHOR("MultiMedia Amlogic <multimedia-sh@amlogic.com>");
MODULE_DESCRIPTION("Amlogic Meson Drm VPU driver");
MODULE_LICENSE("GPL");
