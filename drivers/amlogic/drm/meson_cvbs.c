// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <drm/drm_modeset_helper.h>
#include <drm/drmP.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_connector.h>

#include <linux/component.h>
#include <linux/irq.h>
#include <linux/io.h>
#include <linux/device.h>
#include <linux/string.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/vout/vinfo.h>

#include "meson_crtc.h"
#include "meson_cvbs.h"
#include <vout/cvbs/cvbs_out.h>

static struct drm_display_mode cvbs_mode[] = {
	{ /* MODE_480CVBS */
		.name = "480cvbs",
		.status = 0,
		.clock = 27000,
		.hdisplay = 720,
		.hsync_start = 1390,
		.hsync_end = 1430,
		.htotal = 1716,
		.hskew = 0,
		.vdisplay = 240,//TODO: follow vout field_height, vdisplay is 1/2 height;
		.vsync_start = 725,
		.vsync_end = 730,
		.vtotal = 750,
		.vscan = 0,
		.vrefresh = 60,
		.flags = DRM_MODE_FLAG_INTERLACE,
	},
	{ /* MODE_576CVBS*/
		.name = "576cvbs",
		.status = 0,
		.clock = 27000,
		.hdisplay = 720,
		.hsync_start = 1390,
		.hsync_end = 1430,
		.htotal = 1650,
		.hskew = 0,
		.vdisplay = 288,//TODO: follow vout field_height, vdisplay is 1/2 height;
		.vsync_start = 725,
		.vsync_end = 730,
		.vtotal = 750,
		.vscan = 0,
		.vrefresh = 50,
		.flags = DRM_MODE_FLAG_INTERLACE,
	},
};

static struct am_drm_cvbs_s *am_drm_cvbs;

char *am_cvbs_get_voutmode(struct drm_display_mode *mode)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(cvbs_mode); i++) {
		if (cvbs_mode[i].hdisplay == mode->hdisplay &&
		    cvbs_mode[i].vdisplay == mode->vdisplay &&
		    cvbs_mode[i].vrefresh == mode->vrefresh)
			return cvbs_mode[i].name;
	}
	return NULL;
}

static inline struct am_drm_cvbs_s *con_to_cvbs(struct drm_connector *con)
{
	return container_of(con, struct am_drm_cvbs_s, connector);
}

static inline struct am_drm_cvbs_s *encoder_to_cvbs(struct drm_encoder *encoder)
{
	return container_of(encoder, struct am_drm_cvbs_s, encoder);
}

int am_cvbs_tx_get_modes(struct drm_connector *connector)
{
	int i, count;
	struct drm_display_mode *mode;

	count = 0;
	for (i = 0; i < ARRAY_SIZE(cvbs_mode); i++) {
		mode = drm_mode_duplicate(connector->dev, &cvbs_mode[i]);
		if (!mode) {
			DRM_INFO("[%s:%d]duplicate failed\n", __func__,
				 __LINE__);
			continue;
		}

		drm_mode_probed_add(connector, mode);
		count++;
	}

	return count;
}

enum drm_mode_status am_cvbs_tx_check_mode(struct drm_connector *connector,
					   struct drm_display_mode *mode)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(cvbs_mode); i++) {
		if (cvbs_mode[i].hdisplay == mode->hdisplay &&
		    cvbs_mode[i].vdisplay == mode->vdisplay &&
		    cvbs_mode[i].vrefresh == mode->vrefresh)
			return MODE_OK;
		else
			DRM_INFO("hdisplay = %d\nvdisplay = %d\n"
			 "vrefresh = %d\n", mode->hdisplay,
			 mode->vdisplay, mode->vrefresh);
	}

	return MODE_NOMODE;
}

static struct drm_encoder *am_cvbs_connector_best_encoder
	(struct drm_connector *connector)
{
	struct am_drm_cvbs_s *am_drm_cvbs = con_to_cvbs(connector);

	return &am_drm_cvbs->encoder;
}

static const
struct drm_connector_helper_funcs am_cvbs_connector_helper_funcs = {
	.get_modes = am_cvbs_tx_get_modes,
	.mode_valid = am_cvbs_tx_check_mode,
	.best_encoder = am_cvbs_connector_best_encoder,
};

static enum drm_connector_status am_cvbs_connector_detect
	(struct drm_connector *connector, bool force)
{
	return connector_status_connected;
}

static int am_cvbs_connector_set_property(struct drm_connector *connector,
					  struct drm_property *property,
					  uint64_t val)
{
	struct am_drm_cvbs_s *am_drm_cvbs = con_to_cvbs(connector);
	struct drm_connector_state *state = am_drm_cvbs->connector.state;
	struct drm_mode_config *config = &connector->dev->mode_config;

	if (property == config->content_protection_property) {
		DRM_INFO("property:%s       val: %lld\n", property->name, val);
		/* For none atomic commit */
		/* atomic will be filter on drm_moder_object.c */
		if (val == DRM_MODE_CONTENT_PROTECTION_ENABLED) {
			DRM_DEBUG_KMS("only drivers can set CP Enabled\n");
			return -EINVAL;
		}
		state->content_protection = val;
	}
	/*other parperty todo*/
	return 0;
}

static int am_cvbs_connector_atomic_get_property
	(struct drm_connector *connector,
	const struct drm_connector_state *state,
	struct drm_property *property, uint64_t *val)
{
	struct drm_mode_config *config = &connector->dev->mode_config;

	if (property == config->content_protection_property) {
		DRM_INFO("get content_protection val: %d\n",
			 state->content_protection);
		*val = state->content_protection;
	} else {
		DRM_DEBUG_ATOMIC("Unknown property %s\n", property->name);
		return -EINVAL;
	}
	return 0;
}

static void am_cvbs_connector_destroy(struct drm_connector *connector)
{
	drm_connector_unregister(connector);
	drm_connector_cleanup(connector);
}

static const struct drm_connector_funcs am_cvbs_connector_funcs = {
	.detect			= am_cvbs_connector_detect,
	.fill_modes		= drm_helper_probe_single_connector_modes,
	.set_property		= am_cvbs_connector_set_property,
	.atomic_get_property	= am_cvbs_connector_atomic_get_property,
	.destroy		= am_cvbs_connector_destroy,
	.reset			= drm_atomic_helper_connector_reset,
	.atomic_duplicate_state	= drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state	= drm_atomic_helper_connector_destroy_state,
};

void am_cvbs_encoder_mode_set(struct drm_encoder *encoder,
	struct drm_crtc_state *crtc_state,
	struct drm_connector_state *conn_state)
{
}

void am_cvbs_encoder_enable(struct drm_encoder *encoder,
	struct drm_atomic_state *state)
{
	enum vmode_e vmode = get_current_vmode();
	struct am_meson_crtc_state *meson_crtc_state = to_am_meson_crtc_state(encoder->crtc->state);

	if (meson_crtc_state->uboot_mode_init == 1)
		vmode |= VMODE_INIT_BIT_MASK;

	set_vout_mode_pre_process(vmode);
	cvbs_set_current_vmode(vmode);
	set_vout_mode_post_process(vmode);
}

void am_cvbs_encoder_disable(struct drm_encoder *encoder,
	struct drm_atomic_state *state)
{
	struct am_drm_cvbs_s *am_drm_cvbs = encoder_to_cvbs(encoder);
	struct drm_connector_state *con_state = am_drm_cvbs->connector.state;

	con_state->content_protection = DRM_MODE_CONTENT_PROTECTION_UNDESIRED;
}

static const struct drm_encoder_helper_funcs am_cvbs_encoder_helper_funcs = {
	.atomic_mode_set = am_cvbs_encoder_mode_set,
	.atomic_enable = am_cvbs_encoder_enable,
	.atomic_disable = am_cvbs_encoder_disable,
};

static const struct drm_encoder_funcs am_cvbs_encoder_funcs = {
	.destroy        = drm_encoder_cleanup,
};

static const struct of_device_id am_meson_cvbs_dt_ids[] = {
	{ .compatible = "amlogic, drm-cvbsout", },
	{}
};
MODULE_DEVICE_TABLE(of, am_meson_cvbs_dt_ids);

static int am_meson_cvbs_bind(struct device *dev,
			      struct device *master, void *data)
{
	struct drm_device *drm = data;
	struct drm_encoder *encoder;
	struct drm_connector *connector;
	int ret = 0;

	DRM_INFO("[%s] in\n", __func__);

	am_drm_cvbs = kzalloc(sizeof(*am_drm_cvbs), GFP_KERNEL);
	if (!am_drm_cvbs) {
		DRM_ERROR("[%s,%d] kzalloc failed\n", __func__, __LINE__);
		return -ENOMEM;
	}

	am_drm_cvbs->drm = drm;
	encoder = &am_drm_cvbs->encoder;
	connector = &am_drm_cvbs->connector;

	/* Encoder */
	drm_encoder_helper_add(encoder, &am_cvbs_encoder_helper_funcs);

	ret = drm_encoder_init(drm, encoder, &am_cvbs_encoder_funcs,
			       DRM_MODE_ENCODER_TVDAC, "am_cvbs_encoder");
	if (ret) {
		DRM_ERROR("Failed to init cvbs encoder\n");
		goto cvbs_err;
	}

	encoder->possible_crtcs = BIT(0);

	/* Connector */
	drm_connector_helper_add(connector,
				 &am_cvbs_connector_helper_funcs);

	ret = drm_connector_init(drm, connector, &am_cvbs_connector_funcs,
				 DRM_MODE_CONNECTOR_TV);
	if (ret) {
		DRM_ERROR("Failed to init cvbs OUT connector\n");
		goto cvbs_err;
	}

	connector->interlace_allowed = 1;

	ret = drm_connector_attach_encoder(connector, encoder);
	if (ret) {
		DRM_ERROR("Failed to init cvbs attach\n");
		goto cvbs_err;
	}

	DRM_INFO("[%s] out\n", __func__);
	return ret;

cvbs_err:
	kfree(am_drm_cvbs);

	return ret;
}

static void am_meson_cvbs_unbind(struct device *dev,
				 struct device *master, void *data)
{
	am_drm_cvbs->connector.funcs->destroy(&am_drm_cvbs->connector);
	am_drm_cvbs->encoder.funcs->destroy(&am_drm_cvbs->encoder);
	kfree(am_drm_cvbs);
}

static const struct component_ops am_meson_cvbs_ops = {
	.bind	= am_meson_cvbs_bind,
	.unbind	= am_meson_cvbs_unbind,
};

static int am_meson_cvbs_probe(struct platform_device *pdev)
{
	DRM_INFO("[%s:%d] in\n", __func__, __LINE__);
	return component_add(&pdev->dev, &am_meson_cvbs_ops);
}

static int am_meson_cvbs_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &am_meson_cvbs_ops);
	return 0;
}

static struct platform_driver am_meson_cvbs_pltfm_driver = {
	.probe  = am_meson_cvbs_probe,
	.remove = am_meson_cvbs_remove,
	.driver = {
		.name = "meson-amcvbsout",
		.of_match_table = am_meson_cvbs_dt_ids,
	},
};

int __init am_meson_cvbs_init(void)
{
	return platform_driver_register(&am_meson_cvbs_pltfm_driver);
}

void __exit am_meson_cvbs_exit(void)
{
	platform_driver_unregister(&am_meson_cvbs_pltfm_driver);
}

#ifndef MODULE
module_init(am_meson_cvbs_init);
module_exit(am_meson_cvbs_exit);
#endif

MODULE_AUTHOR("MultiMedia Amlogic <multimedia-sh@amlogic.com>");
MODULE_DESCRIPTION("Amlogic Meson Drm CVBS driver");
MODULE_LICENSE("GPL");

