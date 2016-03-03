
/*
 * drivers/amlogic/display/lcd/lcd_tv/lcd_tv.c
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
#include "lcd_tv.h"
#include "../lcd_reg.h"
#include "../lcd_common.h"

static unsigned int lcd_output_mode;
static DEFINE_MUTEX(lcd_vout_mutex);

enum {
	LCD_OUTPUT_MODE_768P60HZ = 0,
	LCD_OUTPUT_MODE_768P50HZ,
	LCD_OUTPUT_MODE_1080P60HZ,
	LCD_OUTPUT_MODE_1080P50HZ,
	LCD_OUTPUT_MODE_4K2K60HZ420,
	LCD_OUTPUT_MODE_4K2K50HZ420,
	LCD_OUTPUT_MODE_4K2K60HZ,
	LCD_OUTPUT_MODE_4K2K50HZ,
	LCD_OUTPUT_MODE_MAX,
};

static const struct vinfo_s lcd_info[] = {
	{
		.name              = "768p60hz",
		.mode              = VMODE_768P,
		.width             = 1366,
		.height            = 768,
		.field_height      = 768,
		.aspect_ratio_num  = 16,
		.aspect_ratio_den  = 9,
		.sync_duration_num = 60,
		.sync_duration_den = 1,
		.viu_color_fmt     = TVIN_RGB444,
	},
	{
		.name              = "768p50hz",
		.mode              = VMODE_768P_50HZ,
		.width             = 1366,
		.height            = 768,
		.field_height      = 768,
		.aspect_ratio_num  = 16,
		.aspect_ratio_den  = 9,
		.sync_duration_num = 50,
		.sync_duration_den = 1,
		.viu_color_fmt     = TVIN_RGB444,
	},
	{
		.name              = "1080p60hz",
		.mode              = VMODE_1080P,
		.width             = 1920,
		.height            = 1080,
		.field_height      = 1080,
		.aspect_ratio_num  = 16,
		.aspect_ratio_den  = 9,
		.sync_duration_num = 60,
		.sync_duration_den = 1,
		.viu_color_fmt     = TVIN_RGB444,
	},
	{
		.name              = "1080p50hz",
		.mode              = VMODE_1080P_50HZ,
		.width             = 1920,
		.height            = 1080,
		.field_height      = 1080,
		.aspect_ratio_num  = 16,
		.aspect_ratio_den  = 9,
		.sync_duration_num = 50,
		.sync_duration_den = 1,
		.viu_color_fmt     = TVIN_RGB444,
	},
	{
		.name              = "2160p60hz420",
		.mode              = VMODE_4K2K_60HZ_Y420,
		.width             = 3840,
		.height            = 2160,
		.field_height      = 2160,
		.aspect_ratio_num  = 16,
		.aspect_ratio_den  = 9,
		.sync_duration_num = 60,
		.sync_duration_den = 1,
		.viu_color_fmt     = TVIN_RGB444,
	},
	{
		.name              = "2160p50hz420",
		.mode              = VMODE_4K2K_50HZ_Y420,
		.width             = 3840,
		.height            = 2160,
		.field_height      = 2160,
		.aspect_ratio_num  = 16,
		.aspect_ratio_den  = 9,
		.sync_duration_num = 50,
		.sync_duration_den = 1,
		.viu_color_fmt     = TVIN_RGB444,
	},
	{
		.name              = "2160p60hz",
		.mode              = VMODE_4K2K_60HZ,
		.width             = 3840,
		.height            = 2160,
		.field_height      = 2160,
		.aspect_ratio_num  = 16,
		.aspect_ratio_den  = 9,
		.sync_duration_num = 60,
		.sync_duration_den = 1,
		.viu_color_fmt     = TVIN_RGB444,
	},
	{
		.name              = "2160p50hz",
		.mode              = VMODE_4K2K_50HZ,
		.width             = 3840,
		.height            = 2160,
		.field_height      = 2160,
		.aspect_ratio_num  = 16,
		.aspect_ratio_den  = 9,
		.sync_duration_num = 50,
		.sync_duration_den = 1,
		.viu_color_fmt     = TVIN_RGB444,
	},
	{
		.name              = "invalid",
		.mode              = VMODE_INIT_NULL,
		.width             = 1920,
		.height            = 1080,
		.field_height      = 1080,
		.aspect_ratio_num  = 16,
		.aspect_ratio_den  = 9,
		.sync_duration_num = 60,
		.sync_duration_den = 1,
		.viu_color_fmt     = TVIN_RGB444,
	},
};

static int lcd_get_vmode(enum vmode_e mode)
{
	int lcd_vmode = LCD_OUTPUT_MODE_MAX;
	int i, count = ARRAY_SIZE(lcd_info) - 1;

	for (i = 0; i < count; i++) {
		if (mode == lcd_info[i].mode) {
			lcd_vmode = i;
			break;
		}
	}
	return lcd_vmode;
}

static int lcd_vmode_is_mached(int index)
{
	struct aml_lcd_drv_s *lcd_drv = aml_lcd_get_driver();
	struct lcd_config_s *pconf;

	pconf = lcd_drv->lcd_config;
	if ((pconf->lcd_basic.h_active == lcd_info[index].width) &&
		(pconf->lcd_basic.v_active == lcd_info[index].height))
		return 0;
	else
		return -1;
}

static const struct vinfo_s *lcd_get_valid_vinfo(char *mode)
{
	const struct vinfo_s *vinfo = NULL;
	int i, count = ARRAY_SIZE(lcd_info);
	int name_len = 0;
	int ret;

	for (i = 0; i < count; i++) {
		if (strncmp(lcd_info[i].name, mode,
			strlen(lcd_info[i].name)) == 0) {
			if ((vinfo == NULL) ||
				(strlen(lcd_info[i].name) > name_len)) {
				ret = lcd_vmode_is_mached(i);
				if (ret == 0) {
					vinfo = &lcd_info[i];
					name_len = strlen(lcd_info[i].name);
				}
			}
		}
	}
	return vinfo;
}

const struct vinfo_s *lcd_tv_get_vinfo(void)
{
	const struct vinfo_s *info;
	struct aml_lcd_drv_s *lcd_drv = aml_lcd_get_driver();

	info = &lcd_info[lcd_output_mode];
	if (lcd_drv->lcd_info == NULL)
		LCDERR("no lcd_info exist\n");
	else
		info = lcd_drv->lcd_info;
	return info;
}

static void lcd_vmode_vinfo_update(enum vmode_e mode)
{
	const struct vinfo_s *info;
	struct aml_lcd_drv_s *lcd_drv = aml_lcd_get_driver();

	lcd_output_mode = lcd_get_vmode(mode);
	if (lcd_output_mode >= LCD_OUTPUT_MODE_MAX)
		lcd_output_mode = LCD_OUTPUT_MODE_MAX;
	info = &lcd_info[lcd_output_mode];
	if (lcd_debug_print_flag) {
		LCDPR("%s vmode = %d, lcd_mode = %d\n",
			__func__, mode, lcd_output_mode);
	}

	/* update vinfo */
	if (lcd_drv->lcd_info == NULL) {
		LCDERR("no lcd_info exist\n");
		return;
	}
	memcpy(lcd_drv->lcd_info, info, sizeof(struct vinfo_s));
}

/* ************************************************** *
   vout server api
 * ************************************************** */
static enum vmode_e lcd_validate_vmode(char *mode)
{
	const struct vinfo_s *info;

	if (mode == NULL)
		return VMODE_MAX;

	info = lcd_get_valid_vinfo(mode);
	if (info)
		return info->mode;

	return VMODE_MAX;
}

static const struct vinfo_s *lcd_get_current_info(void)
{
	return lcd_tv_get_vinfo();
}

static int lcd_set_current_vmode(enum vmode_e mode)
{
	int ret = 0;
	struct aml_lcd_drv_s *lcd_drv = aml_lcd_get_driver();
	const struct vinfo_s *info;

	mutex_lock(&lcd_vout_mutex);
	LCDPR("driver version: %s\n", lcd_drv->version);

	/* do not change mode value here, for bit mask is useful */
	lcd_vmode_vinfo_update(mode & VMODE_MODE_BIT_MASK);

	if (!(mode & VMODE_INIT_BIT_MASK)) {
		switch (mode & VMODE_MODE_BIT_MASK) {
		case VMODE_768P:
		case VMODE_768P_50HZ:
		case VMODE_1080P:
		case VMODE_1080P_50HZ:
		case VMODE_4K2K_60HZ_Y420:
		case VMODE_4K2K_50HZ_Y420:
		case VMODE_4K2K_60HZ:
		case VMODE_4K2K_50HZ:
			ret = lcd_drv->driver_init();
			break;
		default:
			ret = -EINVAL;
		}
	}

	info = lcd_tv_get_vinfo();
	lcd_vcbus_write(VPP_POSTBLEND_H_SIZE, info->width);

	mutex_unlock(&lcd_vout_mutex);
	return ret;
}

static int lcd_vmode_is_supported(enum vmode_e mode)
{
	int m;

	mode &= VMODE_MODE_BIT_MASK;
	m = lcd_get_vmode(mode);
	LCDPR("%s vmode = %d, lcd_mode = %d(%s)\n",
		__func__, mode, m, lcd_info[m].name);

	switch (mode) {
	case VMODE_768P:
	case VMODE_768P_50HZ:
	case VMODE_1080P:
	case VMODE_1080P_50HZ:
	case VMODE_4K2K_60HZ_Y420:
	case VMODE_4K2K_50HZ_Y420:
	case VMODE_4K2K_60HZ:
	case VMODE_4K2K_50HZ:
		return true;
		break;
	default:
		return false;
	}
}

static int lcd_vout_disable(enum vmode_e cur_vmod)
{
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
	vout_register_server(&lcd_vout_server);
}

/* ************************************************** *
   lcd tv config
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

	if (pconf->lcd_basic.lcd_type == LCD_VBYONE) {
		LCDPR("lane_count = %d\n",
			pconf->lcd_control.vbyone_config->lane_count);
		LCDPR("byte_mode = %d\n",
			pconf->lcd_control.vbyone_config->byte_mode);
		LCDPR("region_num = %d\n",
			pconf->lcd_control.vbyone_config->region_num);
		LCDPR("color_fmt = %d\n",
			pconf->lcd_control.vbyone_config->color_fmt);
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
	} else {
		pconf->lcd_timing.fr_adjust_type = (unsigned char)(para[0]);
		pconf->lcd_timing.ss_level = (unsigned char)(para[1]);
		pconf->lcd_timing.clk_auto = (unsigned char)(para[2]);
	}

	switch (pconf->lcd_basic.lcd_type) {
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
		ret = of_property_read_u32_array(child, "phy_attr",
			&para[0], 2);
		if (ret) {
			if (lcd_debug_print_flag)
				LCDPR("failed to get phy_attr\n");
		} else {
			pconf->lcd_control.lvds_config->phy_vswing = para[0];
			pconf->lcd_control.lvds_config->phy_preem = para[1];
			LCDPR("set phy vswing=%d, preemphasis=%d\n",
				pconf->lcd_control.lvds_config->phy_vswing,
				pconf->lcd_control.lvds_config->phy_preem);
		}
		break;
	case LCD_VBYONE:
		ret = of_property_read_u32_array(child, "vbyone_attr",
			&para[0], 4);
		if (ret) {
			LCDERR("failed to get vbyone_attr\n");
		} else {
			pconf->lcd_control.vbyone_config->lane_count = para[0];
			pconf->lcd_control.vbyone_config->region_num = para[1];
			pconf->lcd_control.vbyone_config->byte_mode = para[2];
			pconf->lcd_control.vbyone_config->color_fmt = para[3];
		}
		ret = of_property_read_u32_array(child, "phy_attr",
			&para[0], 2);
		if (ret) {
			if (lcd_debug_print_flag)
				LCDPR("failed to get phy_attr\n");
		} else {
			pconf->lcd_control.vbyone_config->phy_vswing = para[0];
			pconf->lcd_control.vbyone_config->phy_preem = para[1];
			LCDPR("set phy vswing=%d, preemphasis=%d\n",
				pconf->lcd_control.vbyone_config->phy_vswing,
				pconf->lcd_control.vbyone_config->phy_preem);
		}

		if (lcd_drv->lcd_status) { /* lock pinmux if lcd in on */
			pconf->pin = devm_pinctrl_get_select(lcd_drv->dev,
				"vbyone");
			if (IS_ERR(pconf->pin))
				LCDERR("get vbyone pinmux error\n");
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

static void lcd_vmode_init(struct lcd_config_s *pconf)
{
	enum vmode_e mode;

	mode = get_logo_vmode();
	lcd_vmode_vinfo_update(mode & VMODE_MODE_BIT_MASK);
	lcd_tv_config_update(pconf);
}

static void lcd_config_init(struct lcd_config_s *pconf)
{
	struct lcd_clk_config_s *cconf = get_lcd_clk_config();
	unsigned int ss_level;
	unsigned int clk;

	clk = pconf->lcd_basic.h_period * pconf->lcd_basic.v_period * 60;
	pconf->lcd_timing.lcd_clk = clk;
	pconf->lcd_timing.sync_duration_num = 60;
	pconf->lcd_timing.sync_duration_den = 1;

	pconf->lcd_timing.lcd_clk_dft = pconf->lcd_timing.lcd_clk;
	lcd_tcon_config(pconf);
	lcd_vmode_init(pconf);
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
	if (lcd_drv->lcd_info == NULL) {
		LCDERR("no lcd_info exist\n");
		return;
	}

	/* update vinfo */
	lcd_drv->lcd_info->sync_duration_num = sync_duration;
	lcd_drv->lcd_info->sync_duration_den = 100;
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
   lcd tv
 * ************************************************** */
int lcd_tv_probe(struct platform_device *pdev)
{
	struct aml_lcd_drv_s *lcd_drv = aml_lcd_get_driver();
	int ret;

	lcd_drv->lcd_info = NULL;
	lcd_drv->lcd_info = kmalloc(sizeof(struct vinfo_s), GFP_KERNEL);
	if (!lcd_drv->lcd_info)
		LCDERR("tv_probe: Not enough memory\n");

	lcd_drv->version = LCD_DRV_VERSION;
	lcd_drv->vout_server_init = lcd_vout_server_init;
	lcd_drv->driver_init = lcd_tv_driver_init;
	lcd_drv->driver_disable = lcd_tv_driver_disable;

	lcd_get_config(lcd_drv->lcd_config, pdev);

	switch (lcd_drv->lcd_config->lcd_basic.lcd_type) {
	case LCD_VBYONE:
		lcd_vbyone_interrupt_up();
		break;
	default:
		break;
	}

	ret = aml_lcd_notifier_register(&lcd_frame_rate_adjust_nb);
	if (ret)
		LCDERR("register lcd_frame_rate_adjust_nb failed\n");

	return 0;
}

int lcd_tv_remove(struct platform_device *pdev)
{
	struct aml_lcd_drv_s *lcd_drv = aml_lcd_get_driver();

	aml_lcd_notifier_unregister(&lcd_frame_rate_adjust_nb);
	switch (lcd_drv->lcd_config->lcd_basic.lcd_type) {
	case LCD_VBYONE:
		lcd_vbyone_interrupt_down();
		break;
	default:
		break;
	}

	kfree(lcd_drv->lcd_info);
	lcd_drv->lcd_info = NULL;
	return 0;
}

