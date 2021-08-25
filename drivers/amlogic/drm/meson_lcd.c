// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <drm/drm_modeset_helper.h>
#include <drm/drmP.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_panel.h>
#include <drm/drm_mipi_dsi.h>
#include <video/display_timing.h>
#include <linux/component.h>
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include <linux/amlogic/media/vout/lcd/lcd_notify.h>

#include "meson_crtc.h"
#include "meson_lcd.h"


static struct am_drm_lcd_s *am_drm_lcd;

static struct drm_display_mode tablet_lcd_mode = {
	.name = "panel",
	.status = 0,
	.clock = 74250,
	.hdisplay = 1280,
	.hsync_start = 1390,
	.hsync_end = 1430,
	.htotal = 1650,
	.hskew = 0,
	.vdisplay = 720,
	.vsync_start = 725,
	.vsync_end = 730,
	.vtotal = 750,
	.vscan = 0,
	.vrefresh = 60,
};

static struct drm_display_mode tv_lcd_mode_ref[] = {
	{ /* 600p */
		.name = "600p",
		.status = 0,
		.clock = 40234,
		.hdisplay = 1024,
		.hsync_start = 1026,
		.hsync_end = 1034,
		.htotal = 1056,
		.hskew = 0,
		.vdisplay = 600,
		.vsync_start = 624,
		.vsync_end = 630,
		.vtotal = 635,
		.vscan = 0,
		.vrefresh = 60,
	},
	{ /* 768p */
		.name = "768p",
		.status = 0,
		.clock = 75442,
		.hdisplay = 1366,
		.hsync_start = 1390,
		.hsync_end = 1430,
		.htotal = 1560,
		.hskew = 0,
		.vdisplay = 768,
		.vsync_start = 773,
		.vsync_end = 788,
		.vtotal = 806,
		.vscan = 0,
		.vrefresh = 60,
	},
	{ /* 1080p */
		.name = "1080p",
		.status = 0,
		.clock = 148500,
		.hdisplay = 1920,
		.hsync_start = 1930,
		.hsync_end = 1970,
		.htotal = 2200,
		.hskew = 0,
		.vdisplay = 1080,
		.vsync_start = 1090,
		.vsync_end = 1100,
		.vtotal = 1125,
		.vscan = 0,
		.vrefresh = 60,
	},
	{ /* 2160p */
		.name = "2160p",
		.status = 0,
		.clock = 594000,
		.hdisplay = 3840,
		.hsync_start = 4000,
		.hsync_end = 4060,
		.htotal = 4400,
		.hskew = 0,
		.vdisplay = 2160,
		.vsync_start = 2200,
		.vsync_end = 2210,
		.vtotal = 2250,
		.vscan = 0,
		.vrefresh = 60,
	},
	{ /* invalid */
		.name = "invalid",
		.status = 0,
		.clock = 148500,
		.hdisplay = 1920,
		.hsync_start = 1930,
		.hsync_end = 1970,
		.htotal = 2200,
		.hskew = 0,
		.vdisplay = 1080,
		.vsync_start = 1090,
		.vsync_end = 1100,
		.vtotal = 1125,
		.vscan = 0,
		.vrefresh = 60,
	},
};

static unsigned int lcd_std_frame_rate[] = {
	60,
	59,
	50,
	48,
	47
};

#define DRM_LCD_TV_MODE_CNT_MAX    5
static struct drm_display_mode tv_lcd_mode[DRM_LCD_TV_MODE_CNT_MAX];
static int lcd_tv_mode_cnt;

static struct display_timing am_lcd_timing = {
	.pixelclock = { 55000000, 65000000, 75000000 },
	.hactive = { 1024, 1024, 1024 },
	.hfront_porch = { 40, 40, 40 },
	.hback_porch = { 220, 220, 220 },
	.hsync_len = { 20, 60, 100 },
	.vactive = { 768, 768, 768 },
	.vfront_porch = { 7, 7, 7 },
	.vback_porch = { 21, 21, 21 },
	.vsync_len = { 10, 10, 10 },
	.flags = DISPLAY_FLAGS_DE_HIGH,
};

/* ***************************************************************** */
/*     drm driver function                                           */
/* ***************************************************************** */

char *am_lcd_get_voutmode(struct drm_connector *connector,
		struct drm_display_mode *mode)
{
	int i;
	struct am_drm_lcd_s *lcd;
	struct lcd_config_s *pconf;
	struct drm_display_mode *native_mode;

	lcd = connector_to_am_lcd(connector);
	if (!lcd)
		return NULL;
	if (!lcd->lcd_drv)
		return NULL;

	pconf = &lcd->lcd_drv->config;
	switch (lcd->lcd_drv->mode) {
	case LCD_MODE_TV:
		for (i = 0; i < lcd_tv_mode_cnt; i++) {
			native_mode = &tv_lcd_mode[i];
			if (native_mode->hdisplay == mode->hdisplay &&
			    native_mode->vdisplay == mode->vdisplay &&
			    native_mode->vrefresh == mode->vrefresh)
				return native_mode->name;
		}
		break;
	case LCD_MODE_TABLET:
		native_mode = &tablet_lcd_mode;
		if (native_mode->hdisplay == mode->hdisplay &&
			native_mode->vdisplay == mode->vdisplay &&
			native_mode->vrefresh == mode->vrefresh)
			return native_mode->name;
		break;
	default:
		break;
	}
	return NULL;
}

static int am_lcd_connector_get_modes(struct drm_connector *connector)
{
	struct drm_display_mode *mode, *native_mode;
	struct am_drm_lcd_s *lcd;
	int i;

	lcd = connector_to_am_lcd(connector);

	DRM_DEBUG("***************************************************\n");
	DRM_DEBUG("am_drm_lcd: %s: lcd mode [%s] display size: %d x %d\n",
		__func__, lcd->mode->name,
		lcd->mode->hdisplay, lcd->mode->vdisplay);

	switch (lcd->lcd_drv->mode) {
	case LCD_MODE_TV:
		for (i = 0; i < lcd_tv_mode_cnt; i++) {
			native_mode = &tv_lcd_mode[i];
			if (strstr(native_mode->name, "invalid"))
				continue;

			mode = drm_mode_duplicate(connector->dev, native_mode);
			if (!mode) {
				DRM_INFO("[%s:%d]duplicate failed\n", __func__,
					 __LINE__);
				return 0;
			}

			drm_mode_probed_add(connector, mode);
		}
		break;
	case LCD_MODE_TABLET:
		native_mode = &tablet_lcd_mode;
		mode = drm_mode_duplicate(connector->dev, native_mode);
		if (!mode) {
			DRM_INFO("[%s:%d]duplicate failed\n", __func__,
				 __LINE__);
			return 0;
		}
		drm_mode_probed_add(connector, mode);
		break;
	default:
		break;
	}

	return 0;
}

enum drm_mode_status
am_lcd_connector_mode_valid(struct drm_connector *connector,
		struct drm_display_mode *mode)
{
	int i;
	struct am_drm_lcd_s *lcd;
	struct drm_display_mode *native_mode;

	lcd = connector_to_am_lcd(connector);
	if (!lcd)
		return MODE_ERROR;
	if (!lcd->lcd_drv)
		return MODE_ERROR;

	DRM_DEBUG("am_drm_lcd: %s: mode [%s] display size: %d x %d\n",
		__func__, mode->name, mode->hdisplay, mode->vdisplay);
	DRM_DEBUG("am_drm_lcd: %s: lcd config size: %d x %d\n",
		__func__, lcd->lcd_drv->config.basic.h_active,
		lcd->lcd_drv->config.basic.v_active);

	switch (lcd->lcd_drv->mode) {
	case LCD_MODE_TV:
		for (i = 0; i < lcd_tv_mode_cnt; i++) {
			native_mode = &tv_lcd_mode[i];
			if (strstr(native_mode->name, "invalid"))
				continue;
			if (native_mode->hdisplay == mode->hdisplay &&
				native_mode->vdisplay == mode->vdisplay &&
				native_mode->vrefresh == mode->vrefresh)
				return MODE_OK;
		}
		break;
	case LCD_MODE_TABLET:
		native_mode = &tablet_lcd_mode;
		if (native_mode->hdisplay == mode->hdisplay &&
			native_mode->vdisplay == mode->vdisplay &&
			native_mode->vrefresh == mode->vrefresh)
			return MODE_OK;
		break;
	default:
		break;
	}

	DRM_DEBUG("hdisplay = %d\nvdisplay = %d\n"
		  "vrefresh = %d\n", mode->hdisplay,
		  mode->vdisplay, mode->vrefresh);

	return MODE_NOMODE;
}

static const struct drm_connector_helper_funcs am_lcd_connector_helper_funcs = {
	.get_modes = am_lcd_connector_get_modes,
	.mode_valid = am_lcd_connector_mode_valid,
	//.best_encoder
	//.atomic_best_encoder
};

static enum drm_connector_status
am_lcd_connector_detect(struct drm_connector *connector, bool force)
{
	DRM_DEBUG("am_drm_lcd: %s %d\n", __func__, __LINE__);
	return connector_status_connected;
}

static int am_lcd_connector_dpms(struct drm_connector *connector, int mode)
{
	int ret = 0;

	DRM_DEBUG("am_drm_lcd: %s %d\n", __func__, __LINE__);

	ret = drm_helper_connector_dpms(connector, mode);
	return ret;
}

static int am_lcd_connector_fill_modes(struct drm_connector *connector,
				       u32 maxX, uint32_t maxY)
{
	int count = 0;

	DRM_DEBUG("am_drm_lcd: %s %d\n", __func__, __LINE__);
	count = drm_helper_probe_single_connector_modes(connector, maxX, maxY);
	DRM_DEBUG("am_drm_lcd: %s %d: count=%d\n", __func__, __LINE__, count);
	return count;
}

static void am_lcd_connector_destroy(struct drm_connector *connector)
{
	DRM_DEBUG("am_drm_lcd: %s %d\n", __func__, __LINE__);

	drm_connector_cleanup(connector);
}

static void am_lcd_connector_reset(struct drm_connector *connector)
{
	DRM_DEBUG("am_drm_lcd: %s %d\n", __func__, __LINE__);

	drm_atomic_helper_connector_reset(connector);
}

static struct drm_connector_state *
am_lcd_connector_duplicate_state(struct drm_connector *connector)
{
	struct drm_connector_state *state;

	DRM_DEBUG("am_drm_lcd: %s %d\n", __func__, __LINE__);

	state = drm_atomic_helper_connector_duplicate_state(connector);
	return state;
}

static void am_lcd_connector_destroy_state(struct drm_connector *connector,
					   struct drm_connector_state *state)
{
	DRM_DEBUG("am_drm_lcd: %s %d\n", __func__, __LINE__);

	drm_atomic_helper_connector_destroy_state(connector, state);
}

static const struct drm_connector_funcs am_lcd_connector_funcs = {
	.dpms			= am_lcd_connector_dpms,
	.detect			= am_lcd_connector_detect,
	.fill_modes		= am_lcd_connector_fill_modes,
	.destroy		= am_lcd_connector_destroy,
	.reset			= am_lcd_connector_reset,
	.atomic_duplicate_state	= am_lcd_connector_duplicate_state,
	.atomic_destroy_state	= am_lcd_connector_destroy_state,
};

static void am_lcd_encoder_mode_set(struct drm_encoder *encoder,
			struct drm_crtc_state *crtc_state,
			struct drm_connector_state *conn_state)
{
	DRM_DEBUG("am_drm_lcd: %s %d\n", __func__, __LINE__);
}

static void am_lcd_encoder_enable(struct drm_encoder *encoder,
	struct drm_atomic_state *state)
{
	enum vmode_e vmode = get_current_vmode();
	struct am_drm_lcd_s *lcd = encoder_to_am_lcd(encoder);
	struct drm_display_mode *mode = &encoder->crtc->mode;
	struct am_meson_crtc_state *meson_crtc_state =
				to_am_meson_crtc_state(encoder->crtc->state);
	struct drm_crtc *crtc = encoder->crtc;

	if (!lcd || !lcd->lcd_drv)
		return;

	if (vmode == VMODE_LCD)
		DRM_DEBUG("enable\n");
	else
		DRM_DEBUG("enable fail! vmode:%d\n", vmode);

	if (meson_crtc_state->uboot_mode_init == 1)
		vmode |= VMODE_INIT_BIT_MASK;

	DRM_DEBUG("am_drm_lcd: %s %d\n", __func__, __LINE__);

	if (lcd->lcd_drv->mode == LCD_MODE_TV &&
		crtc->enabled && !crtc->state->active_changed &&
		lcd->prev_vrefresh != mode->vrefresh) {
		DRM_DEBUG("am_drm_lcd: %s pre_active:%d, pre_vrefresh:%u\n",
			__func__, crtc->enabled, mode->vrefresh);
		lcd->prev_vrefresh = mode->vrefresh;
		return;
	}

	lcd->prev_vrefresh = mode->vrefresh;

	vout_notifier_call_chain(VOUT_EVENT_MODE_CHANGE_PRE, &vmode);
	mutex_lock(&lcd_power_mutex);
	aml_lcd_notifier_call_chain(LCD_EVENT_PREPARE, (void *)lcd->lcd_drv);
	if (lcd->lcd_drv->boot_ctrl->init_level !=
	    LCD_INIT_LEVEL_KERNEL_OFF) {
		aml_lcd_notifier_call_chain(LCD_EVENT_ENABLE,
							(void *)lcd->lcd_drv);
		lcd->lcd_drv->config.retry_enable_cnt = 0;
		while (lcd->lcd_drv->config.retry_enable_flag) {
			if (lcd->lcd_drv->config.retry_enable_cnt++ >=
				LCD_ENABLE_RETRY_MAX)
				break;
			DRM_DEBUG("am_drm_lcd: retry enable...%d\n",
				lcd->lcd_drv->config.retry_enable_cnt);
			aml_lcd_notifier_call_chain(LCD_EVENT_IF_POWER_OFF,
						    (void *)lcd->lcd_drv);
			msleep(1000);
			aml_lcd_notifier_call_chain(LCD_EVENT_IF_POWER_ON,
						    (void *)lcd->lcd_drv);
		}
		lcd->lcd_drv->config.retry_enable_cnt = 0;
	}
	mutex_unlock(&lcd_power_mutex);
	vout_notifier_call_chain(VOUT_EVENT_MODE_CHANGE, &vmode);
	DRM_DEBUG("am_drm_lcd: %s %d\n", __func__, __LINE__);
}

static void am_lcd_encoder_disable(struct drm_encoder *encoder,
	struct drm_atomic_state *state)
{
	struct am_drm_lcd_s *lcd = encoder_to_am_lcd(encoder);
	struct drm_crtc *crtc = encoder->crtc;

	if (!lcd || !lcd->lcd_drv)
		return;

	DRM_DEBUG("am_drm_lcd:%s, active:%d, vrefresh:%u, %u\n", __func__,
		crtc->state->enable, lcd->prev_vrefresh,
			crtc->state->mode.vrefresh);
	if (lcd->lcd_drv->mode == LCD_MODE_TV && crtc->state->enable) {
		pr_info("am_drm_lcd: %s, skip lcd disable.\n", __func__);
		return;
	}

	DRM_DEBUG("am_drm_lcd: %s %d\n", __func__, __LINE__);
	mutex_lock(&lcd_power_mutex);
	aml_lcd_notifier_call_chain(LCD_EVENT_DISABLE, (void *)lcd->lcd_drv);
	aml_lcd_notifier_call_chain(LCD_EVENT_UNPREPARE, (void *)lcd->lcd_drv);
	mutex_unlock(&lcd_power_mutex);

	DRM_DEBUG("am_drm_lcd: %s %d\n", __func__, __LINE__);
}

static void am_lcd_encoder_commit(struct drm_encoder *encoder)
{
	DRM_DEBUG("am_drm_lcd: %s %d\n", __func__, __LINE__);
}

static int am_lcd_encoder_atomic_check(struct drm_encoder *encoder,
				       struct drm_crtc_state *crtc_state,
				struct drm_connector_state *conn_state)
{
	DRM_DEBUG("am_drm_lcd: %s %d\n", __func__, __LINE__);
	return 0;
}

static const struct drm_encoder_helper_funcs am_lcd_encoder_helper_funcs = {
	.commit = am_lcd_encoder_commit,
	.atomic_mode_set = am_lcd_encoder_mode_set,
	.atomic_enable = am_lcd_encoder_enable,
	.atomic_disable = am_lcd_encoder_disable,
	.atomic_check = am_lcd_encoder_atomic_check,
};

static const struct drm_encoder_funcs am_lcd_encoder_funcs = {
	.destroy        = drm_encoder_cleanup,
};

static int am_lcd_disable(struct drm_panel *panel)
{
	struct am_drm_lcd_s *lcd = drm_plane_to_am_hdmi(panel);

	if (!lcd || !lcd->lcd_drv)
		return -ENODEV;

	mutex_lock(&lcd_power_mutex);
	aml_lcd_notifier_call_chain(LCD_EVENT_DISABLE, (void *)lcd->lcd_drv);
	mutex_unlock(&lcd_power_mutex);

	return 0;
}

static int am_lcd_unprepare(struct drm_panel *panel)
{
	struct am_drm_lcd_s *lcd = drm_plane_to_am_hdmi(panel);

	if (!lcd || !lcd->lcd_drv)
		return -ENODEV;

	mutex_lock(&lcd_power_mutex);
	aml_lcd_notifier_call_chain(LCD_EVENT_UNPREPARE, (void *)lcd->lcd_drv);
	mutex_unlock(&lcd_power_mutex);

	return 0;
}

static int am_lcd_prepare(struct drm_panel *panel)
{
	struct am_drm_lcd_s *lcd = drm_plane_to_am_hdmi(panel);

	if (!lcd || !lcd->lcd_drv)
		return -ENODEV;

	mutex_lock(&lcd_power_mutex);
	aml_lcd_notifier_call_chain(LCD_EVENT_PREPARE, (void *)lcd->lcd_drv);
	mutex_unlock(&lcd_power_mutex);

	return 0;
}

static int am_lcd_enable(struct drm_panel *panel)
{
	struct am_drm_lcd_s *lcd = drm_plane_to_am_hdmi(panel);

	if (!lcd || !lcd->lcd_drv)
		return -ENODEV;

	DRM_DEBUG("am_drm_lcd: %s %d\n", __func__, __LINE__);

	mutex_lock(&lcd_power_mutex);
	aml_lcd_notifier_call_chain(LCD_EVENT_ENABLE, (void *)lcd->lcd_drv);
	mutex_unlock(&lcd_power_mutex);

	DRM_DEBUG("am_drm_lcd: %s %d\n", __func__, __LINE__);

	return 0;
}

static int am_lcd_get_modes(struct drm_panel *panel)
{
	struct am_drm_lcd_s *lcd = drm_plane_to_am_hdmi(panel);
	struct drm_connector *connector = panel->connector;
	struct drm_device *drm = panel->drm;
	struct drm_display_mode *mode;
	struct lcd_config_s *pconf;

	if (!lcd->mode)
		return 0;

	/*ToDo:the below is no use,may delete it!*/
	mode = drm_mode_duplicate(drm, lcd->mode);
	if (!mode)
		return 0;

	mode->type |= DRM_MODE_TYPE_DRIVER;
	mode->type |= DRM_MODE_TYPE_PREFERRED;

	drm_mode_set_name(mode);

	drm_mode_probed_add(connector, mode);

	pconf = &lcd->lcd_drv->config;
	connector->display_info.bpc = pconf->basic.lcd_bits * 3;
	connector->display_info.width_mm = pconf->basic.screen_width;
	connector->display_info.height_mm = pconf->basic.screen_height;

	connector->display_info.bus_flags = DRM_BUS_FLAG_DE_HIGH;

	DRM_DEBUG("am_drm_lcd: %s %d\n", __func__, __LINE__);

	return 1;
}

static int am_lcd_get_timings(struct drm_panel *panel,
			      unsigned int num_timings,
				    struct display_timing *timings)
{
	struct am_drm_lcd_s *lcd = drm_plane_to_am_hdmi(panel);

	if (!lcd || !lcd->timing)
		return 0;

	DRM_DEBUG("am_drm_lcd: %s %d\n", __func__, __LINE__);

	if (timings)
		memcpy(&timings[0], lcd->timing, sizeof(struct display_timing));

	DRM_DEBUG("am_drm_lcd: %s %d\n", __func__, __LINE__);

	return 1;
}

static const struct drm_panel_funcs am_drm_lcd_funcs = {
	.disable = am_lcd_disable,
	.unprepare = am_lcd_unprepare,
	.prepare = am_lcd_prepare,
	.enable = am_lcd_enable,
	.get_modes = am_lcd_get_modes,
	.get_timings = am_lcd_get_timings,
};

static void am_drm_lcd_connector_tv_mode_init(struct am_drm_lcd_s *lcd)
{
	struct drm_display_mode *native_mode;
	int i;

	if (!lcd->lcd_drv) {
		DRM_DEBUG("invalid lcd driver\n");
		return;
	}

	if (lcd->lcd_drv->mode != LCD_MODE_TV)
		return;

	DRM_DEBUG("am_drm_lcd: %s %d\n", __func__, __LINE__);

	switch (lcd->lcd_drv->config.basic.v_active) {
	case 600:
		native_mode = &tv_lcd_mode_ref[0];
		break;
	case 768:
		native_mode = &tv_lcd_mode_ref[1];
		break;
	case 1080:
		native_mode = &tv_lcd_mode_ref[2];
		break;
	case 2160:
		native_mode = &tv_lcd_mode_ref[3];
		break;
	default:
		native_mode = &tv_lcd_mode_ref[4];
		DRM_DEBUG("invalid lcd mode\n");
		break;
	}

	lcd_tv_mode_cnt = 0;
	for (i = 0; i < DRM_LCD_TV_MODE_CNT_MAX; i++) {
		memset(&tv_lcd_mode[i], 0, sizeof(struct drm_display_mode));
		sprintf(tv_lcd_mode[i].name, "invalid");
	}
	for (i = 0; i < ARRAY_SIZE(lcd_std_frame_rate); i++) {
		if (strstr(native_mode->name, "invalid"))
			continue;
		if (i >= DRM_LCD_TV_MODE_CNT_MAX)
			break;

		memcpy(&tv_lcd_mode[i], native_mode,
		       sizeof(struct drm_display_mode));
		memset(tv_lcd_mode[i].name, 0, DRM_DISPLAY_MODE_LEN);
		sprintf(tv_lcd_mode[i].name, "%s%dhz",
			native_mode->name, lcd_std_frame_rate[i]);
		tv_lcd_mode[i].vrefresh = lcd_std_frame_rate[i];
		lcd_tv_mode_cnt++;
	}

	DRM_DEBUG("am_drm_lcd: %s %d\n", __func__, __LINE__);
}

static void am_drm_lcd_display_mode_timing_init(struct am_drm_lcd_s *lcd)
{
	struct lcd_config_s *pconf;
	unsigned int frame_rate;
	unsigned short tmp;
	char *vout_mode;
	int i;

	if (!lcd->lcd_drv) {
		DRM_DEBUG("invalid lcd driver\n");
		return;
	}

	DRM_DEBUG("am_drm_lcd: %s %d\n", __func__, __LINE__);

	pconf = &lcd->lcd_drv->config;
	vout_mode = get_vout_mode_internal();

	if (lcd->lcd_drv->mode == LCD_MODE_TV) {
		lcd->mode = NULL;
		frame_rate = pconf->timing.sync_duration_num /
			pconf->timing.sync_duration_den;
		for (i = 0; i < ARRAY_SIZE(lcd_std_frame_rate); i++) {
			if (i >= DRM_LCD_TV_MODE_CNT_MAX)
				break;
			if (frame_rate == lcd_std_frame_rate[i]) {
				lcd->mode = &tv_lcd_mode[i];
				break;
			}
		}
		if (!lcd->mode) {
			lcd->mode = &tv_lcd_mode_ref[4];
			DRM_DEBUG("invalid lcd mode\n");
		}
	} else {
		lcd->mode = &tablet_lcd_mode;
	}
	lcd->timing = &am_lcd_timing;

	if (vout_mode) {
		strncpy(lcd->mode->name, vout_mode, DRM_DISPLAY_MODE_LEN);
		lcd->mode->name[DRM_DISPLAY_MODE_LEN - 1] = 0;
		/*ToDo:change it according to lcd drivers config*/
		if (!strcmp(vout_mode, "panel"))
			lcd->base.connector.connector_type = DRM_MODE_CONNECTOR_DSI;
		else
			lcd->base.connector.connector_type = DRM_MODE_CONNECTOR_LVDS;
	}
	lcd->mode->clock = pconf->timing.lcd_clk / 1000;
	lcd->mode->hdisplay = pconf->basic.h_active;
	tmp = pconf->basic.h_period - pconf->basic.h_active -
			pconf->timing.hsync_bp;
	lcd->mode->hsync_start = pconf->basic.h_active + tmp -
			pconf->timing.hsync_width;
	lcd->mode->hsync_end = pconf->basic.h_active + tmp;
	lcd->mode->htotal = pconf->basic.h_period;
	lcd->mode->vdisplay = pconf->basic.v_active;
	tmp = pconf->basic.v_period - pconf->basic.v_active -
			pconf->timing.vsync_bp;
	lcd->mode->vsync_start = pconf->basic.v_active + tmp -
			pconf->timing.vsync_width;
	lcd->mode->vsync_end = pconf->basic.v_active + tmp;
	lcd->mode->vtotal = pconf->basic.v_period;
	lcd->mode->width_mm = pconf->basic.screen_width;
	lcd->mode->height_mm = pconf->basic.screen_height;
	lcd->mode->vrefresh = pconf->timing.sync_duration_num /
				pconf->timing.sync_duration_den;

	lcd->timing->pixelclock.min = pconf->timing.lcd_clk;
	lcd->timing->pixelclock.typ = pconf->timing.lcd_clk;
	lcd->timing->pixelclock.max = pconf->timing.lcd_clk;
	lcd->timing->hactive.min = pconf->basic.h_active;
	lcd->timing->hactive.typ = pconf->basic.h_active;
	lcd->timing->hactive.max = pconf->basic.h_active;
	tmp = pconf->basic.h_period - pconf->basic.h_active -
		pconf->timing.hsync_bp - pconf->timing.hsync_width;
	lcd->timing->hfront_porch.min = tmp;
	lcd->timing->hfront_porch.typ = tmp;
	lcd->timing->hfront_porch.max = tmp;
	lcd->timing->hback_porch.min = pconf->timing.hsync_bp;
	lcd->timing->hback_porch.typ = pconf->timing.hsync_bp;
	lcd->timing->hback_porch.max = pconf->timing.hsync_bp;
	lcd->timing->hsync_len.min = pconf->timing.hsync_width;
	lcd->timing->hsync_len.typ = pconf->timing.hsync_width;
	lcd->timing->hsync_len.max = pconf->timing.hsync_width;
	lcd->timing->vactive.min = pconf->basic.v_active;
	lcd->timing->vactive.typ = pconf->basic.v_active;
	lcd->timing->vactive.max = pconf->basic.v_active;
	tmp = pconf->basic.v_period - pconf->basic.v_active -
		pconf->timing.vsync_bp - pconf->timing.vsync_width;
	lcd->timing->vfront_porch.min = tmp;
	lcd->timing->vfront_porch.typ = tmp;
	lcd->timing->vfront_porch.max = tmp;
	lcd->timing->vback_porch.min = pconf->timing.vsync_bp;
	lcd->timing->vback_porch.typ = pconf->timing.vsync_bp;
	lcd->timing->vback_porch.max = pconf->timing.vsync_bp;
	lcd->timing->vsync_len.min = pconf->timing.vsync_width;
	lcd->timing->vsync_len.typ = pconf->timing.vsync_width;
	lcd->timing->vsync_len.max = pconf->timing.vsync_width;

	DRM_DEBUG("am_drm_lcd: %s: lcd config:\n"
		"lcd_clk             %d\n"
		"h_active            %d\n"
		"v_active            %d\n"
		"screen_width        %d\n"
		"screen_height       %d\n"
		"sync_duration_den   %d\n"
		"sync_duration_num   %d\n",
		__func__,
		lcd->lcd_drv->config.timing.lcd_clk,
		lcd->lcd_drv->config.basic.h_active,
		lcd->lcd_drv->config.basic.v_active,
		lcd->lcd_drv->config.basic.screen_width,
		lcd->lcd_drv->config.basic.screen_height,
		lcd->lcd_drv->config.timing.sync_duration_den,
		lcd->lcd_drv->config.timing.sync_duration_num);
	DRM_DEBUG("am_drm_lcd: %s: display mode:\n"
		"clock       %d\n"
		"hdisplay    %d\n"
		"vdisplay    %d\n"
		"width_mm    %d\n"
		"height_mm   %d\n"
		"vrefresh    %d\n",
		__func__,
		lcd->mode->clock,
		lcd->mode->hdisplay,
		lcd->mode->vdisplay,
		lcd->mode->width_mm,
		lcd->mode->height_mm,
		lcd->mode->vrefresh);
	DRM_DEBUG("am_drm_lcd: %s: timing:\n"
		"pixelclock   %d\n"
		"hactive      %d\n"
		"vactive      %d\n",
		__func__,
		lcd->timing->pixelclock.typ,
		lcd->timing->hactive.typ,
		lcd->timing->vactive.typ);

	DRM_DEBUG("am_drm_lcd: %s %d\n", __func__, __LINE__);
}

int am_drm_lcd_notify_callback(struct notifier_block *block, unsigned long cmd,
			       void *para)
{
	am_drm_lcd->lcd_drv = aml_lcd_get_driver(0);
	if (!am_drm_lcd->lcd_drv) {
		DRM_ERROR("invalid lcd driver, exit\n");
		return -ENODEV;
	}

	switch (cmd) {
	case VOUT_EVENT_MODE_CHANGE:
		am_drm_lcd_display_mode_timing_init(am_drm_lcd);
		DRM_DEBUG("%s:event MODE_CHANGE\n", __func__);
		break;
	default:
		break;
	}
	return 0;
}

static struct notifier_block am_drm_lcd_notifier_nb = {
	.notifier_call	= am_drm_lcd_notify_callback,
};

static const struct of_device_id am_meson_lcd_dt_ids[] = {
	{ .compatible = "amlogic, drm-lcd", },
	{},
};

static int am_meson_lcd_bind(struct device *dev, struct device *master,
			     void *data)
{
	struct drm_device *drm = data;
	struct drm_connector *connector;
	struct drm_encoder *encoder;
	int encoder_type, connector_type;
	int ret = 0;

	am_drm_lcd = kzalloc(sizeof(*am_drm_lcd), GFP_KERNEL);
	if (!am_drm_lcd)
		return -ENOMEM;

	DRM_DEBUG("am_drm_lcd: %s %d\n", __func__, __LINE__);

	am_drm_lcd->lcd_drv = aml_lcd_get_driver(0);
	if (!am_drm_lcd->lcd_drv) {
		pr_err("invalid lcd driver, exit\n");
		return -ENODEV;
	}
	/*
	 * register vout client for timing init,
	 * avoid init with null info when lcd probe with unifykey case.
	 */
	vout_register_client(&am_drm_lcd_notifier_nb);
	am_drm_lcd_connector_tv_mode_init(am_drm_lcd);
	am_drm_lcd_display_mode_timing_init(am_drm_lcd);

	drm_panel_init(&am_drm_lcd->panel);
	am_drm_lcd->panel.dev = NULL;
	am_drm_lcd->panel.funcs = &am_drm_lcd_funcs;

	DRM_DEBUG("am_drm_lcd: %s %d\n", __func__, __LINE__);

	ret = drm_panel_add(&am_drm_lcd->panel);
	if (ret < 0)
		return ret;

	DRM_DEBUG("am_drm_lcd: %s %d\n", __func__, __LINE__);

	am_drm_lcd->drm = drm;

	encoder = &am_drm_lcd->encoder;
	connector = &am_drm_lcd->base.connector;
	encoder_type = DRM_MODE_ENCODER_LVDS;
	connector_type = DRM_MODE_CONNECTOR_LVDS;

	/* Encoder */
	drm_encoder_helper_add(encoder, &am_lcd_encoder_helper_funcs);
	ret = drm_encoder_init(drm, encoder, &am_lcd_encoder_funcs,
			       encoder_type, "am_lcd_encoder");
	if (ret) {
		pr_err("error: am_drm_lcd: Failed to init lcd encoder\n");
		return ret;
	}
	DRM_DEBUG("am_drm_lcd: %s %d: encoder possible_crtcs=%d\n",
		__func__, __LINE__, encoder->possible_crtcs);

	/* Connector */
	drm_connector_helper_add(connector, &am_lcd_connector_helper_funcs);
	ret = drm_connector_init(drm, connector, &am_lcd_connector_funcs,
				 connector_type);
	if (ret) {
		pr_err("error: am_drm_lcd: Failed to init lcd connector\n");
		return ret;
	}

	/* force possible_crtcs */
	encoder->possible_crtcs = BIT(0);

	drm_connector_attach_encoder(connector, encoder);

	DRM_DEBUG("am_drm_lcd: register ok\n");

	return ret;
}

static void am_meson_lcd_unbind(struct device *dev, struct device *master,
				void *data)
{
	if (!am_drm_lcd)
		return;

	if (!am_drm_lcd->lcd_drv)
		return;

	DRM_DEBUG("am_drm_lcd: %s %d\n", __func__, __LINE__);

	drm_panel_detach(&am_drm_lcd->panel);
	drm_panel_remove(&am_drm_lcd->panel);

	DRM_DEBUG("am_drm_lcd: %s %d\n", __func__, __LINE__);
}

static const struct component_ops am_meson_lcd_ops = {
	.bind	= am_meson_lcd_bind,
	.unbind	= am_meson_lcd_unbind,
};

static int am_meson_lcd_probe(struct platform_device *pdev)
{
	return component_add(&pdev->dev, &am_meson_lcd_ops);
}

static int am_meson_lcd_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &am_meson_lcd_ops);
	return 0;
}

static struct platform_driver am_meson_lcd_pltfm_driver = {
	.probe  = am_meson_lcd_probe,
	.remove = am_meson_lcd_remove,
	.driver = {
		.name = "meson-lcd",
		.of_match_table = am_meson_lcd_dt_ids,
	},
};

int __init am_meson_lcd_init(void)
{
	return platform_driver_register(&am_meson_lcd_pltfm_driver);
}

void __exit am_meson_lcd_exit(void)
{
	platform_driver_unregister(&am_meson_lcd_pltfm_driver);
}

#ifndef MODULE
module_init(am_meson_lcd_init);
module_exit(am_meson_lcd_exit);
#endif

MODULE_AUTHOR("MultiMedia Amlogic <multimedia-sh@amlogic.com>");
MODULE_DESCRIPTION("Amlogic Meson Drm LCD driver");
MODULE_LICENSE("GPL");
