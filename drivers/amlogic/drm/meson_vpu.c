/*
 * drivers/amlogic/drm/meson_vpu.c
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
#include <ion/ion_priv.h>
#endif

/* Amlogic Headers */
#include <linux/amlogic/media/vout/vout_notify.h>
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT
#include <linux/amlogic/media/amvecm/amvecm.h>
#endif
#include "osd.h"
#include "osd_drm.h"
#ifdef CONFIG_DRM_MESON_USE_ION
#include "meson_fb.h"
#endif
#include "meson_vpu.h"
#include "meson_plane.h"
#include "meson_crtc.h"
#include "meson_vpu_pipeline.h"

struct vpu_device_data_s {
	enum cpuid_type_e cpu_id;
	enum osd_ver_e osd_ver;
	enum osd_afbc_e afbc_type;
	u8 osd_count;
	u8 has_deband;
	u8 has_lut;
	u8 has_rdma;
	u8 osd_fifo_len;
	u32 vpp_fifo_len;
	u32 dummy_data;
	u32 has_viu2;
	u32 viu1_osd_count;
	struct clk *vpu_clkc;
};

static struct am_vout_mode am_vout_modes[] = {
	{ "1080p60hz", VMODE_HDMI, 1920, 1080, 60, 0},
	{ "1080p30hz", VMODE_HDMI, 1920, 1080, 30, 0},
	{ "1080p50hz", VMODE_HDMI, 1920, 1080, 50, 0},
	{ "1080p25hz", VMODE_HDMI, 1920, 1080, 25, 0},
	{ "1080p24hz", VMODE_HDMI, 1920, 1080, 24, 0},
	{ "2160p30hz", VMODE_HDMI, 3840, 2160, 30, 0},
	{ "2160p60hz", VMODE_HDMI, 3840, 2160, 60, 0},
	{ "2160p50hz", VMODE_HDMI, 3840, 2160, 50, 0},
	{ "2160p25hz", VMODE_HDMI, 3840, 2160, 25, 0},
	{ "2160p24hz", VMODE_HDMI, 3840, 2160, 24, 0},
	{ "1080i60hz", VMODE_HDMI, 1920, 1080, 60, DRM_MODE_FLAG_INTERLACE},
	{ "1080i50hz", VMODE_HDMI, 1920, 1080, 50, DRM_MODE_FLAG_INTERLACE},
	{ "720p60hz", VMODE_HDMI, 1280, 720, 60, 0},
	{ "720p50hz", VMODE_HDMI, 1280, 720, 50, 0},
	{ "480p60hz", VMODE_HDMI, 720, 480, 60, 0},
	{ "480i60hz", VMODE_HDMI, 720, 480, 60, DRM_MODE_FLAG_INTERLACE},
	{ "576p50hz", VMODE_HDMI, 720, 576, 50, 0},
	{ "576i50hz", VMODE_HDMI, 720, 576, 50, DRM_MODE_FLAG_INTERLACE},
	{ "480p60hz", VMODE_HDMI, 720, 480, 60, 0},
};


static struct osd_device_data_s osd_gxbb = {
	.cpu_id = __MESON_CPU_MAJOR_ID_GXBB,
	.osd_ver = OSD_NORMAL,
	.afbc_type = NO_AFBC,
	.osd_count = 2,
	.has_deband = 0,
	.has_lut = 0,
	.has_rdma = 1,
	.has_dolby_vision = 0,
	.osd_fifo_len = 32,
	.vpp_fifo_len = 0x77f,
	.dummy_data = 0x00808000,
	.has_viu2 = 0,
};

static struct osd_device_data_s osd_gxl = {
	.cpu_id = __MESON_CPU_MAJOR_ID_GXL,
	.osd_ver = OSD_NORMAL,
	.afbc_type = NO_AFBC,
	.osd_count = 2,
	.has_deband = 0,
	.has_lut = 0,
	.has_rdma = 1,
	.has_dolby_vision = 0,
	.osd_fifo_len = 32,
	.vpp_fifo_len = 0x77f,
	.dummy_data = 0x00808000,
	.has_viu2 = 0,
};

static struct osd_device_data_s osd_gxm = {
	.cpu_id = __MESON_CPU_MAJOR_ID_GXM,
	.osd_ver = OSD_NORMAL,
	.afbc_type = MESON_AFBC,
	.osd_count = 2,
	.has_deband = 0,
	.has_lut = 0,
	.has_rdma = 1,
	.has_dolby_vision = 0,
	.osd_fifo_len = 32,
	.vpp_fifo_len = 0xfff,
	.dummy_data = 0x00202000,/* dummy data is different */
	.has_viu2 = 0,
};

static struct osd_device_data_s osd_txl = {
	.cpu_id = __MESON_CPU_MAJOR_ID_TXL,
	.osd_ver = OSD_NORMAL,
	.afbc_type = NO_AFBC,
	.osd_count = 2,
	.has_deband = 0,
	.has_lut = 0,
	.has_rdma = 1,
	.has_dolby_vision = 0,
	.osd_fifo_len = 64,
	.vpp_fifo_len = 0x77f,
	.dummy_data = 0x00808000,
	.has_viu2 = 0,
};

static struct osd_device_data_s osd_txlx = {
	.cpu_id = __MESON_CPU_MAJOR_ID_TXLX,
	.osd_ver = OSD_NORMAL,
	.afbc_type = NO_AFBC,
	.osd_count = 2,
	.has_deband = 1,
	.has_lut = 1,
	.has_rdma = 1,
	.has_dolby_vision = 1,
	.osd_fifo_len = 64, /* fifo len 64*8 = 512 */
	.vpp_fifo_len = 0x77f,
	.dummy_data = 0x00808000,
	.has_viu2 = 0,
};

static struct osd_device_data_s osd_axg = {
	.cpu_id = __MESON_CPU_MAJOR_ID_AXG,
	.osd_ver = OSD_SIMPLE,
	.afbc_type = NO_AFBC,
	.osd_count = 1,
	.has_deband = 1,
	.has_lut = 1,
	.has_rdma = 0,
	.has_dolby_vision = 0,
	 /* use iomap its self, no rdma, no canvas, no freescale */
	.osd_fifo_len = 64, /* fifo len 64*8 = 512 */
	.vpp_fifo_len = 0x400,
	.dummy_data = 0x00808000,
	.has_viu2 = 0,
};

static struct osd_device_data_s osd_g12a = {
	.cpu_id = __MESON_CPU_MAJOR_ID_G12A,
	.osd_ver = OSD_HIGH_ONE,
	.afbc_type = MALI_AFBC,
	.osd_count = 4,
	.has_deband = 1,
	.has_lut = 1,
	.has_rdma = 1,
	.has_dolby_vision = 0,
	.osd_fifo_len = 64, /* fifo len 64*8 = 512 */
	.vpp_fifo_len = 0xfff,/* 2048 */
	.dummy_data = 0x00808000,
	.has_viu2 = 1,
};

static struct osd_device_data_s osd_g12b = {
	.cpu_id = __MESON_CPU_MAJOR_ID_G12B,
	.osd_ver = OSD_HIGH_ONE,
	.afbc_type = MALI_AFBC,
	.osd_count = 4,
	.has_deband = 1,
	.has_lut = 1,
	.has_rdma = 1,
	.has_dolby_vision = 0,
	.osd_fifo_len = 64, /* fifo len 64*8 = 512 */
	.vpp_fifo_len = 0xfff,/* 2048 */
	.dummy_data = 0x00808000,
	.has_viu2 = 1,
};

static struct osd_device_data_s osd_tl1 = {
	.cpu_id = __MESON_CPU_MAJOR_ID_TL1,
	.osd_ver = OSD_HIGH_ONE,
	.afbc_type = MALI_AFBC,
	.osd_count = 3,
	.has_deband = 1,
	.has_lut = 1,
	.has_rdma = 1,
	.has_dolby_vision = 0,
	.osd_fifo_len = 64, /* fifo len 64*8 = 512 */
	.vpp_fifo_len = 0xfff,/* 2048 */
	.dummy_data = 0x00808000,
	.has_viu2 = 1,
	.osd0_sc_independ = 0,
};

static struct osd_device_data_s osd_sm1 = {
	.cpu_id = __MESON_CPU_MAJOR_ID_SM1,
	.osd_ver = OSD_HIGH_ONE,
	.afbc_type = MALI_AFBC,
	.osd_count = 4,
	.has_deband = 1,
	.has_lut = 1,
	.has_rdma = 1,
	.has_dolby_vision = 1,
	.osd_fifo_len = 64, /* fifo len 64*8 = 512 */
	.vpp_fifo_len = 0xfff,/* 2048 */
	.dummy_data = 0x00808000,
	.has_viu2 = 1,
	.osd0_sc_independ = 0,
};

static struct osd_device_data_s osd_tm2 = {
	.cpu_id = __MESON_CPU_MAJOR_ID_TM2,
	.osd_ver = OSD_HIGH_ONE,
	.afbc_type = MALI_AFBC,
	.osd_count = 4,
	.has_deband = 1,
	.has_lut = 1,
	.has_rdma = 1,
	.has_dolby_vision = 1,
	.osd_fifo_len = 64, /* fifo len 64*8 = 512 */
	.vpp_fifo_len = 0xfff,/* 2048 */
	.dummy_data = 0x00808000,
	.has_viu2 = 1,
	.osd0_sc_independ = 1,
};
struct osd_device_data_s osd_meson_dev;
static struct platform_device *gp_dev;
static unsigned long gem_mem_start, gem_mem_size;

int am_meson_crtc_dts_info_set(const void *dt_match_data)
{
	struct osd_device_data_s *osd_meson;

	osd_meson = (struct osd_device_data_s *)dt_match_data;
	if (osd_meson) {
		memcpy(&osd_meson_dev, osd_meson,
			sizeof(struct osd_device_data_s));
		osd_meson_dev.viu1_osd_count = osd_meson_dev.osd_count;
		if (osd_meson_dev.has_viu2) {
			/* set viu1 osd count */
			osd_meson_dev.viu1_osd_count--;
			osd_meson_dev.viu2_index = osd_meson_dev.viu1_osd_count;
		}
	} else {
		DRM_ERROR("%s data NOT match\n", __func__);
		return -1;
	}

	return 0;
}

static int am_meson_crtc_loader_protect(struct drm_crtc *crtc, bool on)
{
	struct am_meson_crtc *amcrtc = to_am_meson_crtc(crtc);

	DRM_INFO("%s  %d\n", __func__, on);

	if (on) {
		enable_irq(amcrtc->vblank_irq);
		drm_crtc_vblank_on(crtc);
	} else {
		disable_irq(amcrtc->vblank_irq);
		drm_crtc_vblank_off(crtc);
	}

	return 0;
}

static int am_meson_crtc_enable_vblank(struct drm_crtc *crtc)
{
	unsigned long flags;
	struct am_meson_crtc *amcrtc = to_am_meson_crtc(crtc);

	spin_lock_irqsave(&amcrtc->vblank_irq_lock, flags);
	amcrtc->vblank_enable = true;
	spin_unlock_irqrestore(&amcrtc->vblank_irq_lock, flags);

	return 0;
}

static void am_meson_crtc_disable_vblank(struct drm_crtc *crtc)
{
	unsigned long flags;
	struct am_meson_crtc *amcrtc = to_am_meson_crtc(crtc);

	spin_lock_irqsave(&amcrtc->vblank_irq_lock, flags);
	amcrtc->vblank_enable = false;
	spin_unlock_irqrestore(&amcrtc->vblank_irq_lock, flags);
}

const struct meson_crtc_funcs meson_private_crtc_funcs = {
	.loader_protect = am_meson_crtc_loader_protect,
	.enable_vblank = am_meson_crtc_enable_vblank,
	.disable_vblank = am_meson_crtc_disable_vblank,
};

char *am_meson_crtc_get_voutmode(struct drm_display_mode *mode)
{
	int i;
	struct vinfo_s *vinfo;

	vinfo = get_current_vinfo();

	if (vinfo && vinfo->mode == VMODE_LCD)
		return mode->name;

	for (i = 0; i < ARRAY_SIZE(am_vout_modes); i++) {
		if (am_vout_modes[i].width == mode->hdisplay &&
		    am_vout_modes[i].height == mode->vdisplay &&
		    am_vout_modes[i].vrefresh == mode->vrefresh &&
		    am_vout_modes[i].flags ==
		    (mode->flags & DRM_MODE_FLAG_INTERLACE))
			return am_vout_modes[i].name;
	}
	return NULL;
}

bool am_meson_crtc_check_mode(struct drm_display_mode *mode, char *outputmode)
{
	int i;

	if (!mode || !outputmode)
		return false;
	if (!strcmp(mode->name, "panel"))
		return true;

	for (i = 0; i < ARRAY_SIZE(am_vout_modes); i++) {
		if (!strcmp(am_vout_modes[i].name, outputmode) &&
		    am_vout_modes[i].width == mode->hdisplay &&
		    am_vout_modes[i].height == mode->vdisplay &&
		    am_vout_modes[i].vrefresh == mode->vrefresh &&
		    am_vout_modes[i].flags ==
		    (mode->flags & DRM_MODE_FLAG_INTERLACE)) {
			return true;
		}
	}
	return false;
}

void am_meson_crtc_handle_vsync(struct am_meson_crtc *amcrtc)
{
	unsigned long flags;
	struct drm_crtc *crtc;

	crtc = &amcrtc->base;
	drm_crtc_handle_vblank(crtc);

	spin_lock_irqsave(&crtc->dev->event_lock, flags);
	if (amcrtc->event) {
		drm_crtc_send_vblank_event(crtc, amcrtc->event);
		drm_crtc_vblank_put(crtc);
		amcrtc->event = NULL;
	}
	spin_unlock_irqrestore(&crtc->dev->event_lock, flags);
}

void am_meson_crtc_irq(struct meson_drm *priv)
{
	unsigned long flags;
	struct am_meson_crtc *amcrtc = to_am_meson_crtc(priv->crtc);

	spin_lock_irqsave(&amcrtc->vblank_irq_lock, flags);
	if (amcrtc->vblank_enable) {
		osd_drm_vsync_isr_handler();
		am_meson_crtc_handle_vsync(amcrtc);
	}
	spin_unlock_irqrestore(&amcrtc->vblank_irq_lock, flags);
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
		gp_dev = pdev;
		cma = dev_get_cma_area(&pdev->dev);
		if (cma) {
			logo.size = cma_get_size(cma);
			DRM_INFO("reserved memory base:0x%x, size:0x%x\n",
				 (u32)cma_get_base(cma), logo.size);
			if (logo.size > 0) {
				logo.logo_page =
				dma_alloc_from_contiguous(&pdev->dev,
							  logo.size >>
							  PAGE_SHIFT,
							  0);
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
						    gem_mem_size,
						    DMA_MEMORY_EXCLUSIVE);
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
	vpu_pipeline_init(pipeline);

	/*vsync irq.*/
	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(dev, "cannot find irq for vpu\n");
		return irq;
	}
	amcrtc->vblank_irq = (unsigned int)irq;

	spin_lock_init(&amcrtc->vblank_irq_lock);
	amcrtc->vblank_enable = false;

	ret = devm_request_irq(dev, amcrtc->vblank_irq, am_meson_vpu_irq,
		IRQF_SHARED, dev_name(dev), drm_dev);
	if (ret)
		return ret;

	disable_irq(amcrtc->vblank_irq);
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
	osd_drm_debugfs_exit();
}

static const struct component_ops am_meson_vpu_component_ops = {
	.bind = am_meson_vpu_bind,
	.unbind = am_meson_vpu_unbind,
};

static const struct of_device_id am_meson_vpu_driver_dt_match[] = {
	{ .compatible = "amlogic,meson-gxbb-vpu",
	 .data = &osd_gxbb, },
	{ .compatible = "amlogic,meson-gxl-vpu",
	 .data = &osd_gxl, },
	{ .compatible = "amlogic,meson-gxm-vpu",
	 .data = &osd_gxm, },
	{ .compatible = "amlogic,meson-txl-vpu",
	 .data = &osd_txl, },
	{ .compatible = "amlogic,meson-txlx-vpu",
	 .data = &osd_txlx, },
	{ .compatible = "amlogic,meson-axg-vpu",
	 .data = &osd_axg, },
	{ .compatible = "amlogic,meson-g12a-vpu",
	 .data = &osd_g12a, },
	{ .compatible = "amlogic,meson-g12b-vpu",
	.data = &osd_g12b, },
	{.compatible = "amlogic, meson-tl1-vpu",
	.data = &osd_tl1,},
	{.compatible = "amlogic, meson-sm1-vpu",
	.data = &osd_sm1,},
	{.compatible = "amlogic, meson-tm2-vpu",
	.data = &osd_tm2,},
	{}
};

MODULE_DEVICE_TABLE(of, am_meson_vpu_driver_dt_match);

static int am_meson_vpu_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	const void *vpu_data;
	int ret;

	pr_info("[%s] in\n", __func__);
	if (!dev->of_node) {
		dev_err(dev, "can't find vpu devices\n");
		return -ENODEV;
	}

	vpu_data = of_device_get_match_data(dev);
	if (vpu_data) {
		ret = am_meson_crtc_dts_info_set(vpu_data);
		if (ret < 0)
			return -ENODEV;
	} else {
		dev_err(dev, "%s NOT match\n", __func__);
		return -ENODEV;
	}
	pr_info("[%s] out\n", __func__);
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

module_platform_driver(am_meson_vpu_platform_driver);

MODULE_AUTHOR("MultiMedia Amlogic <multimedia-sh@amlogic.com>");
MODULE_DESCRIPTION("Amlogic Meson Drm VPU driver");
MODULE_LICENSE("GPL");
