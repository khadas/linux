// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <drm/drm_modeset_helper.h>
#include <drm/drmP.h>
#include <drm/drm_edid.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_connector.h>

#include <linux/component.h>
#include <linux/irq.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/kthread.h>
#include <linux/device.h>

#include <linux/workqueue.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/vout/hdmi_tx/hdmi_tx_module.h>
#include <linux/amlogic/media/vout/hdmi_tx/hdmi_common.h>

#include "meson_hdmi.h"
#include "meson_hdcp.h"
#include "meson_vpu.h"
#include "meson_crtc.h"

#define DEVICE_NAME "amhdmitx"
struct am_hdmi_tx am_hdmi_info;
/*for hw limitiation, limit to 1080p/720p for recovery ui.*/
bool hdmitx_set_smaller_pref = true;

/*TODO:will remove later.*/
static struct drm_display_mode dummy_mode = {
	.name = "dummy_l",
	.type = DRM_MODE_TYPE_USERDEF,
	.status = MODE_OK,
	.clock = 25000,
	.hdisplay = 720,
	.hsync_start = 736,
	.hsync_end = 798,
	.htotal = 858,
	.hskew = 0,
	.vdisplay = 480,
	.vsync_start = 489,
	.vsync_end = 495,
	.vtotal = 525,
	.vscan = 0,
	.vrefresh = 50,
	.flags =  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC,
};

char *am_meson_hdmi_get_voutmode(struct drm_display_mode *mode)
{
	return mode->name;
}

int am_hdmi_tx_get_modes(struct drm_connector *connector)
{
	struct edid *edid;
	int *vics;
	int count = 0, i = 0, len = 0;
	struct drm_display_mode *mode;
	struct hdmi_format_para *hdmi_para;
	struct hdmi_cea_timing *timing;
	char *strp = NULL;
	bool set_pref = false;

	edid = (struct edid *)drm_hdmitx_get_raw_edid();
	drm_connector_update_edid_property(connector, edid);

	/*add modes from hdmitx instead of edid*/
	count = drm_hdmitx_get_vic_list(&vics);
	if (count) {
		for (i = 0; i < count; i++) {
			hdmi_para = hdmi_get_fmt_paras(vics[i]);
			timing = &hdmi_para->timing;
			if (hdmi_para->vic == HDMI_UNKNOWN) {
				DRM_ERROR("Get hdmi para by vic [%d] failed.\n", vics[i]);
				continue;
			}

			mode = drm_mode_create(connector->dev);
			strcpy(mode->name, hdmi_para->hdmitx_vinfo.name);

			/*
			 * filter 4k420 mode, 4k420 mode end with "420"
			 * 2160p60hz420 to 2160p60hz
			 */
			strp = strstr(mode->name, "420");
			if (strp) {
				len = strlen(mode->name) - strlen("420");
				if (!strcmp(mode->name + len, "420"))
					*strp = '\0';
			}

			mode->type = DRM_MODE_TYPE_DRIVER;
			mode->vrefresh =
				(int)hdmi_para->hdmitx_vinfo.sync_duration_den /
				hdmi_para->hdmitx_vinfo.sync_duration_num;
			mode->clock = timing->pixel_freq;
			mode->hdisplay = timing->h_active;
			mode->hsync_start = timing->h_active + timing->h_front;
			mode->hsync_end = timing->h_active + timing->h_front + timing->h_sync;
			mode->htotal = timing->h_total;
			mode->hskew = 0;
			mode->flags |= timing->hsync_polarity ?
				DRM_MODE_FLAG_PHSYNC : DRM_MODE_FLAG_NHSYNC;

			mode->vdisplay = timing->v_active;
			mode->vsync_start = timing->v_active + timing->v_front;
			mode->vsync_end = timing->v_active + timing->v_front + timing->v_sync;
			mode->vtotal = timing->v_total;
			mode->vscan = 0;
			mode->flags |= timing->vsync_polarity ?
				DRM_MODE_FLAG_PVSYNC : DRM_MODE_FLAG_NVSYNC;

			if (hdmi_para->hdmitx_vinfo.field_height != hdmi_para->hdmitx_vinfo.height)
				mode->flags |= DRM_MODE_FLAG_INTERLACE;

			/*for recovery ui*/
			if (hdmitx_set_smaller_pref && !set_pref) {
				if ((mode->hdisplay == 1920 && mode->vdisplay == 1080) ||
					(mode->hdisplay == 1280 && mode->vdisplay == 720)) {
					mode->type |= DRM_MODE_TYPE_PREFERRED;
					set_pref = true;
				}
			}

			drm_mode_probed_add(connector, mode);

			DRM_INFO("add mode [%s]\n", mode->name);
		}

		kfree(vics);
	} else {
		DRM_ERROR("drm_hdmitx_get_vic_list return 0.\n");
	}

	/*TODO:add dummy mode temp.*/
	mode = drm_mode_duplicate(connector->dev, &dummy_mode);
	if (!mode) {
		DRM_INFO("[%s:%d]dup dummy mode failed.\n", __func__,
			 __LINE__);
	} else {
		drm_mode_probed_add(connector, mode);
		count++;
	}

	return count;
}

/*   drm_display_mode	     :		 hdmi_format_para
 *		hdisp     : h_active
 *		hsync_start(hss)    : h_active + h_front
 *		hsync_end(hse) : h_active + h_front + h_sync
 *		htotal : h_total
 */
enum drm_mode_status am_hdmi_tx_check_mode(struct drm_connector *connector,
					   struct drm_display_mode *mode)
{
	return MODE_OK;
}

static struct drm_encoder *am_hdmi_connector_best_encoder
	(struct drm_connector *connector)
{
	struct am_hdmi_tx *am_hdmi = connector_to_am_hdmi(connector);

	return &am_hdmi->encoder;
}

static enum drm_connector_status am_hdmi_connector_detect
	(struct drm_connector *connector, bool force)
{
	int hpdstat = drm_hdmitx_detect_hpd();

	DRM_INFO("am_hdmi_connector_detect [%d]\n", hpdstat);
	return hpdstat == 1 ?
		connector_status_connected : connector_status_disconnected;
}

static int am_hdmi_connector_atomic_set_property
	(struct drm_connector *connector,
	struct drm_connector_state *state,
	struct drm_property *property, uint64_t val)
{
	struct am_hdmi_tx *am_hdmi = connector_to_am_hdmi(connector);
	enum vmode_e vmode;

	if (property == am_hdmi->update_attr_prop) {
		if (val == 1) {
			vmode = get_current_vmode();
			if (vmode == VMODE_HDMI) {
				DRM_INFO("UPDATE attr\n");
			} else {
				DRM_INFO("enable fail! vmode:%d\n", vmode);
				return 0;
			}
			DRM_DEBUG("set update prop to update vmode.\n");
			vout_notifier_call_chain(VOUT_EVENT_MODE_CHANGE_PRE, &vmode);
			set_vout_vmode(vmode);
			vout_notifier_call_chain(VOUT_EVENT_MODE_CHANGE, &vmode);
			msleep(1000);
		}
		return 0;
	}

	return -EINVAL;
}

static int am_hdmi_connector_atomic_get_property
	(struct drm_connector *connector,
	const struct drm_connector_state *state,
	struct drm_property *property, uint64_t *val)
{
	struct am_hdmi_tx *am_hdmi = connector_to_am_hdmi(connector);

	if (property == am_hdmi->update_attr_prop) {
		*val = 0;
		return 0;
	}

	return -EINVAL;
}

static void am_hdmi_connector_destroy(struct drm_connector *connector)
{
	drm_connector_unregister(connector);
	drm_connector_cleanup(connector);
}

static const
struct drm_connector_helper_funcs am_hdmi_connector_helper_funcs = {
	.get_modes = am_hdmi_tx_get_modes,
	.mode_valid = am_hdmi_tx_check_mode,
	.best_encoder = am_hdmi_connector_best_encoder,
};

static const struct drm_connector_funcs am_hdmi_connector_funcs = {
	.detect			= am_hdmi_connector_detect,
	.fill_modes		= drm_helper_probe_single_connector_modes,
	.atomic_set_property	= am_hdmi_connector_atomic_set_property,
	.atomic_get_property	= am_hdmi_connector_atomic_get_property,
	.destroy		= am_hdmi_connector_destroy,
	.reset			= drm_atomic_helper_connector_reset,
	.atomic_duplicate_state	= drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state	= drm_atomic_helper_connector_destroy_state,
};

void am_hdmi_encoder_atomic_mode_set(struct drm_encoder *encoder,
	struct drm_crtc_state *crtc_state,
	struct drm_connector_state *conn_state)
{
}

void am_hdmi_encoder_atomic_enable(struct drm_encoder *encoder,
	struct drm_atomic_state *state)
{
	enum vmode_e vmode = get_current_vmode();
	struct am_meson_crtc_state *meson_crtc_state = to_am_meson_crtc_state(encoder->crtc->state);

	if (vmode == VMODE_HDMI) {
		DRM_INFO("enable\n");
	} else {
		DRM_INFO("enable fail! vmode:%d\n", vmode);
		return;
	}

	if (meson_crtc_state->uboot_mode_init == 1)
		vmode |= VMODE_INIT_BIT_MASK;

	set_vout_mode_pre_process(vmode);
	set_vout_vmode(vmode);
	set_vout_mode_post_process(vmode);
	msleep(1000);
}

void am_hdmi_encoder_atomic_disable(struct drm_encoder *encoder,
	struct drm_atomic_state *state)
{
}

static int am_hdmi_encoder_atomic_check(struct drm_encoder *encoder,
	struct drm_crtc_state *crtc_state,
	struct drm_connector_state *conn_state)
{
	return 0;
}

static const struct drm_encoder_helper_funcs am_hdmi_encoder_helper_funcs = {
	.atomic_mode_set = am_hdmi_encoder_atomic_mode_set,
	.atomic_enable = am_hdmi_encoder_atomic_enable,
	.atomic_disable = am_hdmi_encoder_atomic_disable,
	.atomic_check = am_hdmi_encoder_atomic_check,
};

static const struct drm_encoder_funcs am_hdmi_encoder_funcs = {
	.destroy = drm_encoder_cleanup,
};

static const struct of_device_id am_meson_hdmi_dt_ids[] = {
	{ .compatible = "amlogic, drm-amhdmitx", },
	{},
};

MODULE_DEVICE_TABLE(of, am_meson_hdmi_dt_ids);

struct drm_connector *am_meson_hdmi_connector(void)
{
	return &am_hdmi_info.connector;
}

static void am_meson_hdmi_connector_init_property(struct drm_device *drm_dev,
						  struct am_hdmi_tx *am_hdmi)
{
	struct drm_property *prop;

	prop = drm_property_create_bool(drm_dev, 0, "UPDATE");
	if (prop) {
		am_hdmi->update_attr_prop = prop;
		drm_object_attach_property(&am_hdmi->connector.base, prop, 0);
	} else {
		DRM_ERROR("Failed to UPDATE property\n");
	}
}

static void am_meson_hdmi_hpd_cb(void *data)
{
	struct am_hdmi_tx *am_hdmi = (struct am_hdmi_tx *)data;

	DRM_INFO("drm hdmitx hpd notify\n");
	drm_helper_hpd_irq_event(am_hdmi->connector.dev);
}

static int am_meson_hdmi_bind(struct device *dev,
			      struct device *master, void *data)
{
	struct drm_device *drm = data;
	struct meson_drm *priv = drm->dev_private;
	struct am_hdmi_tx *am_hdmi;
	struct drm_connector *connector;
	struct drm_encoder *encoder;
	int ret;

	DRM_INFO("[%s] in\n", __func__);
	am_hdmi = &am_hdmi_info;
	memset(am_hdmi, 0, sizeof(*am_hdmi));

	DRM_INFO("drm hdmitx init and version:%s\n", DRM_HDMITX_VER);
	am_hdmi->priv = priv;
	encoder = &am_hdmi->encoder;
	connector = &am_hdmi->connector;

	/* Connector */
	am_hdmi->connector.polled = DRM_CONNECTOR_POLL_HPD;
	drm_connector_helper_add(connector,
				 &am_hdmi_connector_helper_funcs);

	ret = drm_connector_init(drm, connector, &am_hdmi_connector_funcs,
				 DRM_MODE_CONNECTOR_HDMIA);
	if (ret) {
		dev_err(priv->dev, "Failed to init hdmi tx connector\n");
		return ret;
	}
	am_meson_hdmi_connector_init_property(drm, am_hdmi);

	connector->interlace_allowed = 1;

	/* Encoder */
	drm_encoder_helper_add(encoder, &am_hdmi_encoder_helper_funcs);
	ret = drm_encoder_init(drm, encoder, &am_hdmi_encoder_funcs,
			       DRM_MODE_ENCODER_TVDAC, "am_hdmi_encoder");
	if (ret) {
		dev_err(priv->dev, "Failed to init hdmi encoder\n");
		return ret;
	}
	encoder->possible_crtcs = BIT(0);
	drm_connector_attach_encoder(connector, encoder);

	/*hpd irq moved to amhdmitx, registe call back */
	drm_hdmitx_register_hpd_cb(am_meson_hdmi_hpd_cb, (void *)am_hdmi);

	DRM_INFO("[%s] out\n", __func__);
	return 0;
}

static void am_meson_hdmi_unbind(struct device *dev,
				 struct device *master, void *data)
{
	am_hdmi_info.connector.funcs->destroy(&am_hdmi_info.connector);
	am_hdmi_info.encoder.funcs->destroy(&am_hdmi_info.encoder);
}

static const struct component_ops am_meson_hdmi_ops = {
	.bind	= am_meson_hdmi_bind,
	.unbind	= am_meson_hdmi_unbind,
};

static int am_meson_hdmi_probe(struct platform_device *pdev)
{
	DRM_INFO("[%s] in\n", __func__);
	return component_add(&pdev->dev, &am_meson_hdmi_ops);
}

static int am_meson_hdmi_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &am_meson_hdmi_ops);
	return 0;
}

static struct platform_driver am_meson_hdmi_pltfm_driver = {
	.probe  = am_meson_hdmi_probe,
	.remove = am_meson_hdmi_remove,
	.driver = {
		.name = "meson-amhdmitx",
		.of_match_table = am_meson_hdmi_dt_ids,
	},
};

int __init am_meson_hdmi_init(void)
{
	return platform_driver_register(&am_meson_hdmi_pltfm_driver);
}

void __exit am_meson_hdmi_exit(void)
{
	platform_driver_unregister(&am_meson_hdmi_pltfm_driver);
}

#ifndef MODULE
module_init(am_meson_hdmi_init);
module_exit(am_meson_hdmi_exit);
#endif

MODULE_AUTHOR("MultiMedia Amlogic <multimedia-sh@amlogic.com>");
MODULE_DESCRIPTION("Amlogic Meson Drm HDMI driver");
MODULE_LICENSE("GPL");
