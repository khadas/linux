// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/init.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include "lcd_common.h"

#include <linux/component.h>
#include <drm/amlogic/meson_drm_bind.h>

static int meson_lcd_bind(struct device *dev,
			      struct device *master, void *data);
static void meson_lcd_unbind(struct device *dev,
				 struct device *master, void *data);

struct drm_lcd_wrapper {
	struct meson_panel_dev drm_lcd_instance;
	struct aml_lcd_drv_s *lcd_drv;
	int drm_lcd_type;
	int drm_id;
};

#define to_drm_lcd_wrapper(x)	container_of(x, struct drm_lcd_wrapper, drm_lcd_instance)

static struct drm_lcd_wrapper drm_lcd_wrappers[LCD_MAX_DRV];

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

static int get_lcd_tv_modes(struct meson_panel_dev *panel,
		struct drm_display_mode **modes, int *num)
{
	struct drm_lcd_wrapper *wrapper = to_drm_lcd_wrapper(panel);
	struct aml_lcd_drv_s *pdrv = wrapper->lcd_drv;
	struct drm_display_mode *native_mode;
	struct drm_display_mode *nmodes;
	int i;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s\n", pdrv->index, __func__);
	switch (pdrv->config.basic.v_active) {
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
		LCDERR("[%d]: %s: invalid lcd mode\n", pdrv->index, __func__);
		break;
	}

	if (strstr(native_mode->name, "invalid")) {
		*num = 0;
		return -ENODEV;
	}

	*num = ARRAY_SIZE(lcd_std_frame_rate);
	nmodes = kmalloc_array(*num, sizeof(struct drm_display_mode), GFP_KERNEL);
	if (!nmodes) {
		*num = 0;
		return -ENOMEM;
	}

	for (i = 0; i < *num; i++) {
		memcpy(&nmodes[i], native_mode,
		       sizeof(struct drm_display_mode));
		memset(nmodes[i].name, 0, DRM_DISPLAY_MODE_LEN);
		sprintf(nmodes[i].name, "%s%dhz",
			native_mode->name, lcd_std_frame_rate[i]);
		nmodes[i].vrefresh = lcd_std_frame_rate[i];
	}

	*modes = nmodes;

	return 0;
}

static int get_lcd_tablet_modes(struct meson_panel_dev *panel,
		struct drm_display_mode **modes, int *num)
{
	struct drm_lcd_wrapper *wrapper = to_drm_lcd_wrapper(panel);
	struct aml_lcd_drv_s *pdrv = wrapper->lcd_drv;
	struct drm_display_mode *mode;
	struct lcd_config_s *pconf = &pdrv->config;
	unsigned short tmp;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s\n", pdrv->index, __func__);

	*num = 1;
	*modes = kmalloc_array(*num, sizeof(struct drm_display_mode), GFP_KERNEL);
	mode = modes[0];

	if (pdrv->index == 0) {
		snprintf(mode->name, DRM_DISPLAY_MODE_LEN, "panel");
	} else {
		snprintf(mode->name, DRM_DISPLAY_MODE_LEN,
			"panel%d", pdrv->index);
	}

	mode->clock = pconf->timing.lcd_clk / 1000;
	mode->hdisplay = pconf->basic.h_active;
	tmp = pconf->basic.h_period - pconf->basic.h_active -
		pconf->timing.hsync_bp;
	mode->hsync_start = pconf->basic.h_active + tmp -
		pconf->timing.hsync_width;
	mode->hsync_end = pconf->basic.h_active + tmp;
	mode->htotal = pconf->basic.h_period;
	mode->vdisplay = pconf->basic.v_active;
	tmp = pconf->basic.v_period - pconf->basic.v_active -
		pconf->timing.vsync_bp;
	mode->vsync_start = pconf->basic.v_active + tmp -
		pconf->timing.vsync_width;
	mode->vsync_end = pconf->basic.v_active + tmp;
	mode->vtotal = pconf->basic.v_period;
	mode->width_mm = pconf->basic.screen_width;
	mode->height_mm = pconf->basic.screen_height;
	mode->vrefresh = pconf->timing.sync_duration_num /
		pconf->timing.sync_duration_den;

	return 0;
}

static int meson_lcd_bind(struct device *dev, struct device *master, void *data)
{
	struct meson_drm_bound_data *bound_data = data;
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)dev_get_drvdata(dev);
	int index = pdrv->index;
	int connector_type = 0;

	/*init drm instance*/
	drm_lcd_wrappers[index].lcd_drv = pdrv;
	drm_lcd_wrappers[index].drm_lcd_instance.base.ver = MESON_DRM_CONNECTOR_V10;
	if (pdrv->mode == LCD_MODE_TV)
		drm_lcd_wrappers[index].drm_lcd_instance.get_modes = get_lcd_tv_modes;
	else
		drm_lcd_wrappers[index].drm_lcd_instance.get_modes = get_lcd_tablet_modes;

	/*set lcd type.*/
	switch (pdrv->config.basic.lcd_type) {
	case LCD_LVDS:
		if (index == 1)
			connector_type = DRM_MODE_CONNECTOR_MESON_LVDS_B;
		else if (index == 2)
			connector_type = DRM_MODE_CONNECTOR_MESON_LVDS_C;
		else
			connector_type = DRM_MODE_CONNECTOR_MESON_LVDS_A;
		break;
	case LCD_VBYONE:
		if (index)
			connector_type = DRM_MODE_CONNECTOR_MESON_VBYONE_B;
		else
			connector_type = DRM_MODE_CONNECTOR_MESON_VBYONE_A;
		break;
	case LCD_MIPI:
		if (index)
			connector_type = DRM_MODE_CONNECTOR_MESON_MIPI_B;
		else
			connector_type = DRM_MODE_CONNECTOR_MESON_MIPI_A;
		break;
	case LCD_EDP:
		if (index)
			connector_type = DRM_MODE_CONNECTOR_MESON_EDP_B;
		else
			connector_type = DRM_MODE_CONNECTOR_MESON_EDP_A;
		break;
	default:
		break;
	}
	/*todo: add more connector_type here for tconless*/
	drm_lcd_wrappers[index].drm_lcd_type = connector_type;

	/*bind instance to drm*/
	if (bound_data->connector_component_bind) {
		drm_lcd_wrappers[index].drm_id = bound_data->connector_component_bind
			(bound_data->drm,
			drm_lcd_wrappers[index].drm_lcd_type,
			&drm_lcd_wrappers[index].drm_lcd_instance.base);
		LCDPR("[%d]: %s: connector_type: 0x%x, drm_id: %d\n",
			index, __func__,
			drm_lcd_wrappers[index].drm_lcd_type,
			drm_lcd_wrappers[index].drm_id);
	} else {
		LCDERR("[%d]: no bind func from drm\n", index);
	}

	return 0;
}

static void meson_lcd_unbind(struct device *dev, struct device *master, void *data)
{
	struct meson_drm_bound_data *bound_data = data;
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)dev_get_drvdata(dev);
	int index = pdrv->index;

	if (bound_data->connector_component_unbind) {
		bound_data->connector_component_unbind(bound_data->drm,
			drm_lcd_wrappers[index].drm_lcd_type,
			drm_lcd_wrappers[index].drm_id);
		LCDPR("[%d]: %s: connector_type: 0x%x, drm_id: %d\n",
			index, __func__,
			drm_lcd_wrappers[index].drm_lcd_type,
			drm_lcd_wrappers[index].drm_id);
	} else {
		LCDERR("[%d]: no unbind func from drm\n", index);
	}

	drm_lcd_wrappers[index].drm_id = 0;
}

static const struct component_ops meson_lcd_bind_ops = {
	.bind	= meson_lcd_bind,
	.unbind	= meson_lcd_unbind,
};

int lcd_drm_add(struct device *dev)
{
	/*bind to drm*/
	component_add(dev, &meson_lcd_bind_ops);

	return 0;
}

void lcd_drm_remove(struct device *dev)
{
	component_del(dev, &meson_lcd_bind_ops);
}
