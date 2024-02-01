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

static int meson_lcd_bind(struct device *dev, struct device *master, void *data);
static void meson_lcd_unbind(struct device *dev, struct device *master, void *data);

struct drm_lcd_wrapper {
	struct meson_panel_dev drm_lcd_instance;
	struct aml_lcd_drv_s *lcd_drv;
	int drm_lcd_type;
	int drm_id;
};

#define to_drm_lcd_wrapper(x)	container_of(x, struct drm_lcd_wrapper, drm_lcd_instance)

static struct drm_lcd_wrapper drm_lcd_wrappers[LCD_MAX_DRV];

static void lcd_drm_vmode_update(struct aml_lcd_drv_s *pdrv, struct lcd_detail_timing_s *ptiming,
		struct drm_display_mode *pmode, unsigned int frame_rate)
{
	unsigned int pclk = ptiming->pixel_clk;
	unsigned int htotal = ptiming->h_period;
	unsigned int vtotal = ptiming->v_period;
	unsigned long long temp;

	if (!pmode)
		return;

	switch (ptiming->fr_adjust_type) {
	case 0: /* pixel clk adjust */
		pclk = frame_rate * htotal * vtotal;
		break;
	case 1: /* htotal adjust */
		temp = pclk;
		temp *= 100;
		htotal = vtotal * frame_rate;
		htotal = lcd_do_div(temp, htotal);
		htotal = (htotal + 99) / 100; /* round off */
		pclk = frame_rate * htotal * vtotal;
		break;
	case 2: /* vtotal adjust */
		temp = pclk;
		temp *= 100;
		vtotal = htotal * frame_rate;
		vtotal = lcd_do_div(temp, vtotal);
		vtotal = (vtotal + 99) / 100; /* round off */
		pclk = frame_rate * htotal * vtotal;
		break;
	case 3: /* free adjust, use min/max range to calculate */
		temp = pclk;
		temp *= 100;
		vtotal = htotal * frame_rate;
		vtotal = lcd_do_div(temp, vtotal);
		vtotal = (vtotal + 99) / 100; /* round off */
		if (vtotal > ptiming->v_period_max) {
			vtotal = ptiming->v_period_max;
			htotal = vtotal * frame_rate;
			htotal = lcd_do_div(temp, htotal);
			htotal = (htotal + 99) / 100; /* round off */
			if (htotal > ptiming->h_period_max)
				htotal = ptiming->h_period_max;
		} else if (vtotal < ptiming->v_period_min) {
			vtotal = ptiming->v_period_min;
			htotal = vtotal * frame_rate;
			htotal = lcd_do_div(temp, htotal);
			htotal = (htotal + 99) / 100; /* round off */
			if (htotal < ptiming->h_period_min)
				htotal = ptiming->h_period_min;
		}
		pclk = frame_rate * htotal * vtotal;
		break;
	case 4: /* hdmi mode */
		if (frame_rate == 59 || frame_rate == 119) {
			/* pixel clk adjust */
			pclk = frame_rate * htotal * vtotal;
		} else if (frame_rate == 47) {
			/* htotal adjust */
			temp = pclk;
			temp *= 100;
			htotal = vtotal * 50;
			htotal = lcd_do_div(temp, htotal);
			htotal = (htotal + 99) / 100; /* round off */
			pclk = frame_rate * htotal * vtotal;
		} else if (frame_rate == 95) {
			/* htotal adjust */
			temp = pclk;
			temp *= 100;
			htotal = vtotal * 100;
			htotal = lcd_do_div(temp, htotal);
			htotal = (htotal + 99) / 100; /* round off */
			pclk = frame_rate * htotal * vtotal;
		} else {
			/* htotal adjust */
			temp = pclk;
			temp *= 100;
			htotal = vtotal * frame_rate;
			htotal = lcd_do_div(temp, htotal);
			htotal = (htotal + 99) / 100; /* round off */
			pclk = frame_rate * htotal * vtotal;
		}
		break;
	default:
		LCDERR("[%d]: %s: invalid fr_adjust_type: %d\n",
		       pdrv->index, __func__, ptiming->fr_adjust_type);
		return;
	}

	pmode->clock = pclk / 1000;
	pmode->htotal = htotal;
	pmode->vtotal = vtotal;
}

static void lcd_drm_display_mode_add(struct aml_lcd_drv_s *pdrv,
		struct lcd_detail_timing_s *ptiming,
		struct drm_display_mode *pmode, unsigned short frame_rate)
{
	unsigned short tmp;

	pmode->clock = ptiming->pixel_clk / 1000;
	pmode->hdisplay = ptiming->h_active;
	tmp = ptiming->h_period - ptiming->hsync_width - ptiming->hsync_bp;
	pmode->hsync_start = tmp;
	pmode->hsync_end = ptiming->hsync_width + tmp;
	pmode->htotal = ptiming->h_period;
	pmode->vdisplay = ptiming->v_active;
	tmp = ptiming->v_period - ptiming->vsync_width - ptiming->vsync_bp;
	pmode->vsync_start = tmp;
	pmode->vsync_end = ptiming->vsync_width + tmp;
	pmode->vtotal = ptiming->v_period;
	pmode->width_mm = pdrv->config.basic.screen_width;
	pmode->height_mm = pdrv->config.basic.screen_height;

	if (frame_rate != ptiming->frame_rate)
		lcd_drm_vmode_update(pdrv, ptiming, pmode, frame_rate);

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
		LCDPR("[%d]: %s: %s, clock=%d, htotal=%d, vtotal=%d\n",
			pdrv->index, __func__, pmode->name, pmode->clock,
			pmode->htotal, pmode->vtotal);
	}
}

static int get_lcd_tv_modes(struct meson_panel_dev *panel,
		struct drm_display_mode **modes, int *num)
{
	struct drm_lcd_wrapper *wrapper = to_drm_lcd_wrapper(panel);
	struct aml_lcd_drv_s *pdrv = wrapper->lcd_drv;
	struct drm_display_mode *nmodes;
	struct lcd_vmode_list_s *temp_list;
	struct lcd_detail_timing_s *ptiming;
	unsigned int i, frame_rate, mode_cnt = 0, mode_idx;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s\n", pdrv->index, __func__);

	if (!pdrv || !pdrv->vmode_mgr.vmode_list_header)
		return 0;

	mode_cnt = pdrv->vmode_mgr.vmode_cnt;
	nmodes = kcalloc(mode_cnt, sizeof(struct drm_display_mode), GFP_KERNEL);
	if (!nmodes) {
		*num = 0;
		return -ENOMEM;
	}

	mode_idx = 0;
	temp_list = pdrv->vmode_mgr.vmode_list_header;
	while (temp_list) {
		if (!temp_list->info || !temp_list->info->dft_timing)
			continue;

		ptiming = temp_list->info->dft_timing;
		for (i = 0; i < temp_list->info->duration_cnt; i++) {
			frame_rate = temp_list->info->duration[i].frame_rate;
			if (frame_rate == 0)
				break;

			memset(nmodes[mode_idx].name, 0, DRM_DISPLAY_MODE_LEN);
			sprintf(nmodes[mode_idx].name, "%s%dhz",
				temp_list->info->name, frame_rate);
			lcd_drm_display_mode_add(pdrv, ptiming, &nmodes[mode_idx], frame_rate);
			mode_idx++;
		}
		temp_list = temp_list->next;
	}

	*num = mode_idx;
	*modes = nmodes;

	return 0;
}

static int get_lcd_tablet_modes(struct meson_panel_dev *panel,
		struct drm_display_mode **modes, int *num)
{
	struct drm_lcd_wrapper *wrapper = to_drm_lcd_wrapper(panel);
	struct aml_lcd_drv_s *pdrv = wrapper->lcd_drv;
	struct drm_display_mode *mode;
	struct lcd_detail_timing_s *ptiming;
	unsigned short tmp;

	if (!pdrv)
		return -ENODEV;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s\n", pdrv->index, __func__);

	*num = 1;
	*modes = kmalloc_array(*num, sizeof(struct drm_display_mode), GFP_KERNEL);
	mode = modes[0];
	memset(mode, 0, sizeof(struct drm_display_mode));

	if (pdrv->index == 0)
		snprintf(mode->name, DRM_DISPLAY_MODE_LEN, "panel");
	else
		snprintf(mode->name, DRM_DISPLAY_MODE_LEN, "panel%d", pdrv->index);

	ptiming = &pdrv->config.timing.base_timing;
	mode->clock = ptiming->pixel_clk / 1000;
	mode->hdisplay = ptiming->h_active;
	tmp = ptiming->h_period - ptiming->h_active - ptiming->hsync_bp;
	mode->hsync_start = ptiming->h_active + tmp - ptiming->hsync_width;
	mode->hsync_end = ptiming->h_active + tmp;
	mode->htotal = ptiming->h_period;
	mode->vdisplay = ptiming->v_active;
	tmp = ptiming->v_period - ptiming->v_active - ptiming->vsync_bp;
	mode->vsync_start = ptiming->v_active + tmp - ptiming->vsync_width;
	mode->vsync_end = ptiming->v_active + tmp;
	mode->vtotal = ptiming->v_period;
	mode->width_mm = pdrv->config.basic.screen_width;
	mode->height_mm = pdrv->config.basic.screen_height;
	//mode->vrefresh = ptiming->sync_duration_num/
		//ptiming->sync_duration_den;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
		LCDPR("[%d]: %s: %s, clock=%d, htotal=%d, vtotal=%d, %dx%d@%dhz\n",
			pdrv->index, __func__, mode->name, mode->clock,
			mode->htotal, mode->vtotal,
			mode->hdisplay, mode->vdisplay, ptiming->frame_rate);
	}

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
	case LCD_MLVDS:
		if (index == 1)
			connector_type = DRM_MODE_CONNECTOR_MESON_LVDS_B;
		else if (index == 2)
			connector_type = DRM_MODE_CONNECTOR_MESON_LVDS_C;
		else
			connector_type = DRM_MODE_CONNECTOR_MESON_LVDS_A;
		break;
	case LCD_VBYONE:
	case LCD_P2P:
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
