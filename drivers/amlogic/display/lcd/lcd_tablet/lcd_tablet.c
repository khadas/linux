
/*
 * drivers/amlogic/display/lcd/lcd_tablet/lcd_tablet.c
 *
 * Copyright (C) 2015 Amlogic, Inc. All rights reserved.
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

#include <linux/init.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/of.h>
#include <linux/reset.h>
#include <linux/amlogic/vout/vinfo.h>
#include <linux/amlogic/vout/vout_notify.h>
#include <linux/amlogic/vout/lcd_vout.h>
#include <linux/amlogic/vout/lcd_notify.h>
#include "lcd_tablet.h"
#include "../lcd_reg.h"
#include "../lcd_common.h"

#define PANEL_NAME    "panel"

static DEFINE_MUTEX(lcd_vout_mutex);

/* ************************************************** *
   vout server api
 * ************************************************** */
static enum vmode_e lcd_validate_vmode(char *mode)
{
	if (mode == NULL)
		return VMODE_MAX;

	if ((strncmp(mode, PANEL_NAME, strlen(PANEL_NAME))) == 0)
		return VMODE_LCD;

	return VMODE_MAX;
}

static const struct vinfo_s *lcd_get_current_info(void)
{
	struct aml_lcd_drv_s *lcd_drv = aml_lcd_get_driver();

	return lcd_drv->lcd_info;
}

static int lcd_set_current_vmode(enum vmode_e mode)
{
	int ret = 0;
	struct aml_lcd_drv_s *lcd_drv = aml_lcd_get_driver();

	mutex_lock(&lcd_vout_mutex);
	LCDPR("driver version: %s\n", lcd_drv->version);

	if (!(mode & VMODE_INIT_BIT_MASK)) {
		if (VMODE_LCD == (mode & VMODE_MODE_BIT_MASK))
			ret = lcd_drv->driver_init();
		else
			ret = -EINVAL;
	}

	lcd_vcbus_write(VPP_POSTBLEND_H_SIZE, lcd_drv->lcd_info->width);

	mutex_unlock(&lcd_vout_mutex);
	return ret;
}

static int lcd_vmode_is_supported(enum vmode_e mode)
{
	mode &= VMODE_MODE_BIT_MASK;
	LCDPR("%s vmode = %d\n", __func__, mode);

	if (mode == VMODE_LCD)
		return true;
	return false;
}

static int lcd_vout_disable(enum vmode_e cur_vmod)
{
	aml_lcd_notifier_call_chain(LCD_EVENT_POWER_OFF, NULL);
	return 0;
}

#ifdef CONFIG_PM
static int lcd_suspend(void)
{
	aml_lcd_notifier_call_chain(LCD_EVENT_POWER_OFF, NULL);
	return 0;
}
static int lcd_resume(void)
{
	aml_lcd_notifier_call_chain(LCD_EVENT_POWER_ON, NULL);
	return 0;
}
#endif

static struct vout_server_s lcd_vout_server = {
	.name = "lcd_vout_server",
	.op = {
		.get_vinfo = lcd_get_current_info,
		.set_vmode = lcd_set_current_vmode,
		.validate_vmode = lcd_validate_vmode,
		.vmode_is_supported = lcd_vmode_is_supported,
		.disable = lcd_vout_disable,
#ifdef CONFIG_PM
		.vout_suspend = lcd_suspend,
		.vout_resume = lcd_resume,
#endif
	},
};

static void lcd_vout_server_init(void)
{
	struct aml_lcd_drv_s *lcd_drv = aml_lcd_get_driver();
	struct vinfo_s *vinfo;
	struct lcd_config_s *pconf;

	vinfo = lcd_drv->lcd_info;
	pconf = lcd_drv->lcd_config;
	if (vinfo) {
		vinfo->name = PANEL_NAME;
		vinfo->mode = VMODE_LCD;
		vinfo->width = pconf->lcd_basic.h_active;
		vinfo->height = pconf->lcd_basic.v_active;
		vinfo->field_height = pconf->lcd_basic.v_active;
		vinfo->aspect_ratio_num = pconf->lcd_basic.screen_width;
		vinfo->aspect_ratio_den = pconf->lcd_basic.screen_height;
		vinfo->screen_real_width = pconf->lcd_basic.screen_width;
		vinfo->screen_real_height = pconf->lcd_basic.screen_height;
		vinfo->sync_duration_num = pconf->lcd_timing.sync_duration_num;
		vinfo->sync_duration_den = pconf->lcd_timing.sync_duration_den;
		vinfo->video_clk = pconf->lcd_timing.lcd_clk;
		vinfo->viu_color_fmt = TVIN_RGB444;
	}

	vout_register_server(&lcd_vout_server);
}

/* ************************************************** *
   lcd tablet config
 * ************************************************** */
static void lcd_config_print(struct lcd_config_s *pconf)
{
	LCDPR("%s, %s, %dbit, %dx%d\n",
		pconf->lcd_basic.model_name,
		lcd_type_type_to_str(pconf->lcd_basic.lcd_type),
		pconf->lcd_basic.lcd_bits,
		pconf->lcd_basic.h_active, pconf->lcd_basic.v_active);

	if (lcd_debug_print_flag == 0)
		return;

	LCDPR("h_period = %d\n", pconf->lcd_basic.h_period);
	LCDPR("v_period = %d\n", pconf->lcd_basic.v_period);
	LCDPR("screen_width = %d\n", pconf->lcd_basic.screen_width);
	LCDPR("screen_height = %d\n", pconf->lcd_basic.screen_height);

	LCDPR("hsync_width = %d\n", pconf->lcd_timing.hsync_width);
	LCDPR("hsync_bp = %d\n", pconf->lcd_timing.hsync_bp);
	LCDPR("hsync_pol = %d\n", pconf->lcd_timing.hsync_pol);
	LCDPR("vsync_width = %d\n", pconf->lcd_timing.vsync_width);
	LCDPR("vsync_bp = %d\n", pconf->lcd_timing.vsync_bp);
	LCDPR("vsync_pol = %d\n", pconf->lcd_timing.vsync_pol);

	LCDPR("fr_adjust_type = %d\n", pconf->lcd_timing.fr_adjust_type);
	LCDPR("ss_level = %d\n", pconf->lcd_timing.ss_level);
	LCDPR("clk_auto = %d\n", pconf->lcd_timing.clk_auto);

	if (pconf->lcd_basic.lcd_type == LCD_TTL) {
		LCDPR("clk_pol = %d\n",
			pconf->lcd_control.ttl_config->clk_pol);
		LCDPR("sync_valid = %d\n",
			pconf->lcd_control.ttl_config->sync_valid);
		LCDPR("swap_ctrl = %d\n",
			pconf->lcd_control.ttl_config->swap_ctrl);
	} else if (pconf->lcd_basic.lcd_type == LCD_LVDS) {
		LCDPR("lvds_repack = %d\n",
			pconf->lcd_control.lvds_config->lvds_repack);
		LCDPR("pn_swap = %d\n",
			pconf->lcd_control.lvds_config->pn_swap);
		LCDPR("dual_port = %d\n",
			pconf->lcd_control.lvds_config->dual_port);
		LCDPR("port_swap = %d\n",
			pconf->lcd_control.lvds_config->port_swap);
	}
}

static int lcd_get_model_timing(struct lcd_config_s *pconf,
		struct platform_device *pdev)
{
	int ret = 0;
	const char *str;
	unsigned int para[10];
	struct device_node *child;
	struct aml_lcd_drv_s *lcd_drv = aml_lcd_get_driver();

	child = of_get_child_by_name(pdev->dev.of_node, pconf->lcd_propname);
	if (child == NULL) {
		LCDERR("failed to get %s\n", pconf->lcd_propname);
		return -1;
	}

	ret = of_property_read_string(child, "model_name", &str);
	if (ret) {
		LCDERR("failed to get model_name\n");
		strcpy(pconf->lcd_basic.model_name, pconf->lcd_propname);
	} else {
		strcpy(pconf->lcd_basic.model_name, str);
	}

	ret = of_property_read_string(child, "interface", &str);
	if (ret) {
		LCDERR("failed to get interface\n");
		str = "invalid";
	}
	pconf->lcd_basic.lcd_type = lcd_type_str_to_type(str);

	ret = of_property_read_u32_array(child, "basic_setting", &para[0], 7);
	if (ret) {
		LCDERR("failed to get basic_setting\n");
	} else {
		pconf->lcd_basic.h_active = para[0];
		pconf->lcd_basic.v_active = para[1];
		pconf->lcd_basic.h_period = para[2];
		pconf->lcd_basic.v_period = para[3];
		pconf->lcd_basic.lcd_bits = para[4];
		pconf->lcd_basic.screen_width = para[5];
		pconf->lcd_basic.screen_height = para[6];
	}

	ret = of_property_read_u32_array(child, "lcd_timing", &para[0], 6);
	if (ret) {
		LCDERR("failed to get lcd_timing\n");
	} else {
		pconf->lcd_timing.hsync_width = (unsigned short)(para[0]);
		pconf->lcd_timing.hsync_bp = (unsigned short)(para[1]);
		pconf->lcd_timing.hsync_pol = (unsigned short)(para[2]);
		pconf->lcd_timing.vsync_width = (unsigned short)(para[3]);
		pconf->lcd_timing.vsync_bp = (unsigned short)(para[4]);
		pconf->lcd_timing.vsync_pol = (unsigned short)(para[5]);
	}

	ret = of_property_read_u32_array(child, "clk_attr", &para[0], 4);
	if (ret) {
		LCDERR("failed to get clk_attr\n");
		pconf->lcd_timing.fr_adjust_type = 0;
		pconf->lcd_timing.ss_level = 0;
		pconf->lcd_timing.clk_auto = 1;
		pconf->lcd_timing.lcd_clk = 60;
	} else {
		pconf->lcd_timing.fr_adjust_type = (unsigned char)(para[0]);
		pconf->lcd_timing.ss_level = (unsigned char)(para[1]);
		pconf->lcd_timing.clk_auto = (unsigned char)(para[2]);
		if (para[3] > 0) {
			pconf->lcd_timing.lcd_clk = para[3];
		} else { /* avoid 0 mistake */
			pconf->lcd_timing.lcd_clk = 60;
			LCDERR("lcd_clk is 0, default to 60Hz\n");
		}
	}
	if (pconf->lcd_timing.clk_auto == 0) {
		ret = of_property_read_u32_array(child, "clk_para",
			&para[0], 3);
		if (ret) {
			LCDERR("failed to get clk_para\n");
		} else {
			pconf->lcd_timing.pll_ctrl = para[0];
			pconf->lcd_timing.div_ctrl = para[1];
			pconf->lcd_timing.clk_ctrl = para[2];
		}
	}

	switch (pconf->lcd_basic.lcd_type) {
	case LCD_TTL:
		ret = of_property_read_u32_array(child, "ttl_attr",
			&para[0], 5);
		if (ret) {
			LCDERR("failed to get ttl_attr\n");
		} else {
			pconf->lcd_control.ttl_config->clk_pol = para[0];
			pconf->lcd_control.ttl_config->sync_valid =
				((para[1] << 1) | (para[2] << 0));
			pconf->lcd_control.ttl_config->swap_ctrl =
				((para[3] << 1) | (para[4] << 0));
		}

		if (lcd_drv->lcd_status) /* lock pinmux if lcd in on */
			lcd_ttl_pinmux_set(1);
		else
			lcd_ttl_pinmux_set(0);
		break;
	case LCD_LVDS:
		ret = of_property_read_u32_array(child, "lvds_attr",
			&para[0], 4);
		if (ret) {
			LCDERR("failed to get lvds_attr\n");
		} else {
			pconf->lcd_control.lvds_config->lvds_repack = para[0];
			pconf->lcd_control.lvds_config->dual_port = para[1];
			pconf->lcd_control.lvds_config->pn_swap = para[2];
			pconf->lcd_control.lvds_config->port_swap = para[3];
		}
		break;
	default:
		LCDERR("invalid lcd type\n");
		break;
	}

	pconf->rstc.encl = devm_reset_control_get(&pdev->dev, "encl");
	if (IS_ERR(pconf->rstc.encl))
		LCDERR("get reset control encl error\n");
	pconf->rstc.vencl = devm_reset_control_get(&pdev->dev, "vencl");
	if (IS_ERR(pconf->rstc.vencl))
		LCDERR("get reset control vencl error\n");

	return ret;
}

static void lcd_config_init(struct lcd_config_s *pconf)
{
	struct lcd_clk_config_s *cconf = get_lcd_clk_config();
	unsigned int ss_level;
	unsigned int clk;
	unsigned int sync_duration, h_period, v_period;

	clk = pconf->lcd_timing.lcd_clk;
	h_period = pconf->lcd_basic.h_period;
	v_period = pconf->lcd_basic.v_period;
	if (clk < 200) { /* regard as frame_rate */
		sync_duration = clk * 100;
		pconf->lcd_timing.lcd_clk = clk * h_period * v_period;
	} else { /* regard as pixel clock */
		sync_duration = ((clk / h_period) * 100) / v_period;
	}
	pconf->lcd_timing.sync_duration_num = sync_duration;
	pconf->lcd_timing.sync_duration_den = 100;

	pconf->lcd_timing.lcd_clk_dft = pconf->lcd_timing.lcd_clk;
	lcd_tcon_config(pconf);
	lcd_clk_generate_parameter(pconf);
	ss_level = pconf->lcd_timing.ss_level;
	cconf->ss_level = (ss_level >= cconf->ss_level_max) ? 0 : ss_level;
}

static int lcd_get_config(struct lcd_config_s *pconf,
		struct platform_device *pdev)
{
	if (pdev->dev.of_node == NULL) {
		LCDERR("dev of_node is null\n");
		return -1;
	}
	lcd_get_model_timing(pconf, pdev);
	lcd_get_power_config(pconf, pdev);
	lcd_config_print(pconf);

	lcd_config_init(pconf);

	return 0;
}

/* ************************************************** *
   lcd notify
 * ************************************************** */
/* sync_duration is real_value * 100 */
static void lcd_set_vinfo(unsigned int sync_duration)
{
	struct aml_lcd_drv_s *lcd_drv = aml_lcd_get_driver();

	LCDPR("%s: sync_duration=%d\n", __func__, sync_duration);

	/* update vinfo */
	lcd_drv->lcd_info->sync_duration_num = sync_duration;
	lcd_drv->lcd_info->sync_duration_den = 100;
	/* update lcd config sync_duration */
	lcd_drv->lcd_config->lcd_timing.sync_duration_num = sync_duration;
	lcd_drv->lcd_config->lcd_timing.sync_duration_den = 100;

	/* update clk & timing config */
	lcd_vmode_change(lcd_drv->lcd_config);
	/* update interface timing if needed, current no need */
	/* update clk parameter */
	lcd_clk_generate_parameter(lcd_drv->lcd_config);
}

static int lcd_frame_rate_adjust_notifier(struct notifier_block *nb,
		unsigned long event, void *data)
{
	unsigned int *sync_duration;

	/* LCDPR("%s: 0x%lx\n", __func__, event); */
	if ((event & LCD_EVENT_FRAME_RATE_ADJUST) == 0)
		return NOTIFY_DONE;

	sync_duration = (unsigned int *)data;
	lcd_set_vinfo(*sync_duration);

	return NOTIFY_OK;
}

static struct notifier_block lcd_frame_rate_adjust_nb = {
	.notifier_call = lcd_frame_rate_adjust_notifier,
};

/* ************************************************** *
   lcd tablet
 * ************************************************** */
int lcd_tablet_probe(struct platform_device *pdev)
{
	struct aml_lcd_drv_s *lcd_drv = aml_lcd_get_driver();
	int ret;

	lcd_drv->lcd_info = NULL;
	lcd_drv->lcd_info = kmalloc(sizeof(struct vinfo_s), GFP_KERNEL);
	if (!lcd_drv->lcd_info)
		LCDERR("tablet_probe: Not enough memory\n");

	lcd_drv->version = LCD_DRV_VERSION;
	lcd_drv->vout_server_init = lcd_vout_server_init;
	lcd_drv->driver_init = lcd_tablet_driver_init;
	lcd_drv->driver_disable = lcd_tablet_driver_disable;

	lcd_get_config(lcd_drv->lcd_config, pdev);

	ret = aml_lcd_notifier_register(&lcd_frame_rate_adjust_nb);
	if (ret)
		LCDERR("register lcd_frame_rate_adjust_nb failed\n");

	return 0;
}

int lcd_tablet_remove(struct platform_device *pdev)
{
	struct aml_lcd_drv_s *lcd_drv = aml_lcd_get_driver();

	aml_lcd_notifier_unregister(&lcd_frame_rate_adjust_nb);
	kfree(lcd_drv->lcd_info);
	lcd_drv->lcd_info = NULL;
	return 0;
}

