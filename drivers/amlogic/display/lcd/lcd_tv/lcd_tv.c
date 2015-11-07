
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
#ifdef CONFIG_AML_VPU
#include <linux/amlogic/vpu.h>
#endif
#include <linux/amlogic/vout/vinfo.h>
#include <linux/amlogic/vout/vout_notify.h>
#include <linux/amlogic/vout/lcd_vout.h>
#include <linux/amlogic/vout/lcd_notify.h>
#include "lcd_tv.h"
#include "../lcd_reg.h"
#include "../lcd_common.h"

static char lcd_propname[20] = "lvds_0";
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
	},
};

static int lcd_get_vmode(enum vmode_e mode)
{
	int i, count = ARRAY_SIZE(lcd_info) - 1;

	for (i = 0; i < count; i++) {
		if (mode == lcd_info[i].mode)
			break;
	}
	return i;
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
	if (lcd_output_mode >= LCD_OUTPUT_MODE_MAX)
		lcd_output_mode = LCD_OUTPUT_MODE_MAX;
	return &lcd_info[lcd_output_mode];
}

static int lcd_set_current_vmode(enum vmode_e mode)
{
	int ret = 0;
	struct aml_lcd_drv_s *lcd_drv = aml_lcd_get_driver();

	mutex_lock(&lcd_vout_mutex);
	LCDPR("driver version: %s\n", lcd_drv->version);

	/* do not change mode value here, for bit mask is useful */
	lcd_output_mode = lcd_get_vmode(mode & VMODE_MODE_BIT_MASK);
	if (lcd_debug_print_flag) {
		LCDPR("%s vmode = %d, lcd_mode = %d\n",
			__func__, (mode & VMODE_MODE_BIT_MASK),
			lcd_output_mode);
	}

	if (!(mode & VMODE_LOGO_BIT_MASK)) {
		switch (mode & VMODE_MODE_BIT_MASK) {
		case VMODE_768P:
		case VMODE_768P_50HZ:
		case VMODE_1080P:
		case VMODE_1080P_50HZ:
		case VMODE_4K2K_60HZ_Y420:
		case VMODE_4K2K_50HZ_Y420:
		case VMODE_4K2K_60HZ:
		case VMODE_4K2K_50HZ:
			lcd_drv->driver_init();
			break;
		default:
			ret = -EINVAL;
		}
	}

	lcd_vcbus_write(VPP_POSTBLEND_H_SIZE, lcd_info[lcd_output_mode].width);

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
	struct aml_lcd_drv_s *lcd_drv = aml_lcd_get_driver();

	lcd_drv->module_enable();
	return 0;
}
static int lcd_resume(void)
{
	struct aml_lcd_drv_s *lcd_drv = aml_lcd_get_driver();

	lcd_drv->module_disable();
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

	strcpy(pconf->lcd_basic.model_name, lcd_propname);
	child = of_get_child_by_name(pdev->dev.of_node, lcd_propname);
	if (child == NULL) {
		LCDERR("failed to get %s\n", lcd_propname);
		return -1;
	}

	ret = of_property_read_string(child, "model_name", &str);
	if (ret) {
		LCDERR("failed to get model_name\n");
		strcpy(pconf->lcd_basic.model_name, lcd_propname);
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

	ret = of_property_read_u32_array(child, "clk_attr", &para[0], 3);
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

		pconf->pin = devm_pinctrl_get(&pdev->dev);
		if (IS_ERR(pconf->pin))
			LCDERR("get vbyone pinmux error\n");
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

static int lcd_get_power_config(struct lcd_config_s *pconf,
		struct platform_device *pdev)
{
	int ret = 0;
	unsigned int para[5];
	unsigned int val;
	struct device_node *child;
	struct lcd_power_ctrl_s *lcd_power = pconf->lcd_power;
	int i, j;
	unsigned int index;

	child = of_get_child_by_name(pdev->dev.of_node, lcd_propname);
	if (child == NULL) {
		LCDPR("error: failed to get %s\n", lcd_propname);
		return -1;
	}

	ret = of_property_read_u32_array(child, "power_on_step", &para[0], 4);
	if (ret) {
		LCDPR("failed to get power_on_step\n");
		lcd_power->power_on_step[0].type = LCD_POWER_TYPE_MAX;
	} else {
		i = 0;
		while (i < LCD_PWR_STEP_MAX) {
			j = 4 * i;
			ret = of_property_read_u32_index(child, "power_on_step",
				j, &val);
			lcd_power->power_on_step[i].type = (unsigned char)val;
			if (val == 0xff) /* ending */
				break;
			j = 4 * i + 1;
			ret = of_property_read_u32_index(child,
				"power_on_step", j, &val);
			lcd_power->power_on_step[i].index = val;
			j = 4 * i + 2;
			ret = of_property_read_u32_index(child,
				"power_on_step", j, &val);
			lcd_power->power_on_step[i].value = val;
			j = 4 * i + 3;
			ret = of_property_read_u32_index(child,
				"power_on_step", j, &val);
			lcd_power->power_on_step[i].delay = val;

			/* gpio request */
			switch (lcd_power->power_on_step[i].type) {
			case LCD_POWER_TYPE_CPU:
				index = lcd_power->power_on_step[i].index;
				if (index < LCD_CPU_GPIO_NUM_MAX)
					lcd_cpu_gpio_register(index);
				break;
			default:
				break;
			}
			if (lcd_debug_print_flag) {
				LCDPR("power_on %d type: %d\n", i,
					lcd_power->power_on_step[i].type);
				LCDPR("power_on %d index: %d\n", i,
					lcd_power->power_on_step[i].index);
				LCDPR("power_on %d value: %d\n", i,
					lcd_power->power_on_step[i].value);
				LCDPR("power_on %d delay: %d\n", i,
					lcd_power->power_on_step[i].delay);
			}
			i++;
		}
	}

	ret = of_property_read_u32_array(child, "power_off_step", &para[0], 4);
	if (ret) {
		LCDPR("failed to get power_off_step\n");
		lcd_power->power_off_step[0].type = LCD_POWER_TYPE_MAX;
	} else {
		i = 0;
		while (i < LCD_PWR_STEP_MAX) {
			j = 4 * i;
			ret = of_property_read_u32_index(child,
				"power_off_step", j, &val);
			lcd_power->power_off_step[i].type = (unsigned char)val;
			if (val == 0xff) /* ending */
				break;
			j = 4 * i + 1;
			ret = of_property_read_u32_index(child,
				"power_off_step", j, &val);
			lcd_power->power_off_step[i].index = val;
			j = 4 * i + 2;
			ret = of_property_read_u32_index(child,
				"power_off_step", j, &val);
			lcd_power->power_off_step[i].value = val;
			j = 4 * i + 3;
			ret = of_property_read_u32_index(child,
				"power_off_step", j, &val);
			lcd_power->power_off_step[i].delay = val;

			/* gpio request */
			switch (lcd_power->power_off_step[i].type) {
			case LCD_POWER_TYPE_CPU:
				index = lcd_power->power_off_step[i].index;
				if (index < LCD_CPU_GPIO_NUM_MAX)
					lcd_cpu_gpio_register(index);
				break;
			default:
				break;
			}
			if (lcd_debug_print_flag) {
				LCDPR("power_on %d type: %d\n", i,
					lcd_power->power_off_step[i].type);
				LCDPR("power_on %d index: %d\n", i,
					lcd_power->power_off_step[i].index);
				LCDPR("power_on %d value: %d\n", i,
					lcd_power->power_off_step[i].value);
				LCDPR("power_on %d delay: %d\n", i,
					lcd_power->power_off_step[i].delay);
			}
			i++;
		}
	}

	ret = of_property_read_u32(child, "backlight_index", &para[0]);
	if (ret) {
		LCDPR("failed to get backlight_index\n");
		pconf->backlight_index = 0;
	} else {
		pconf->backlight_index = para[0];
	}

	return ret;
}

static void lcd_clk_config(struct lcd_config_s *pconf)
{
	unsigned int clk;

	clk = pconf->lcd_basic.h_period * pconf->lcd_basic.v_period * 60;
	pconf->lcd_timing.lcd_clk = clk;
	pconf->lcd_timing.sync_duration_num = 60;
	pconf->lcd_timing.sync_duration_den = 1;
}

static void lcd_tcon_config(struct lcd_config_s *pconf)
{
	unsigned short h_period, v_period, h_active, v_active;
	unsigned short hsync_bp, hsync_width, vsync_bp, vsync_width;
	unsigned short de_hstart, de_vstart;
	unsigned short hstart, hend, vstart, vend;

	h_period = pconf->lcd_basic.h_period;
	v_period = pconf->lcd_basic.v_period;
	h_active = pconf->lcd_basic.h_active;
	v_active = pconf->lcd_basic.v_active;
	hsync_bp = pconf->lcd_timing.hsync_bp;
	hsync_width = pconf->lcd_timing.hsync_width;
	vsync_bp = pconf->lcd_timing.vsync_bp;
	vsync_width = pconf->lcd_timing.vsync_width;

	de_hstart = h_period - h_active - 1;
	de_vstart = v_period - v_active;

	pconf->lcd_timing.video_on_pixel = de_hstart;
	pconf->lcd_timing.video_on_line = de_vstart;

	hstart = (de_hstart + h_period - hsync_bp - hsync_width) % h_period;
	hend = (de_hstart + h_period - hsync_bp) % h_period;
	pconf->lcd_timing.hs_hs_addr = hstart;
	pconf->lcd_timing.hs_he_addr = hend;
	pconf->lcd_timing.hs_vs_addr = 0;
	pconf->lcd_timing.hs_ve_addr = v_period - 1;

	pconf->lcd_timing.vs_hs_addr = (hstart + h_period) % h_period;
	pconf->lcd_timing.vs_he_addr = pconf->lcd_timing.vs_hs_addr;
	vstart = (de_vstart + v_period - vsync_bp - vsync_width) % v_period;
	vend = (de_vstart + v_period - vsync_bp) % v_period;
	pconf->lcd_timing.vs_vs_addr = vstart;
	pconf->lcd_timing.vs_ve_addr = vend;

	if (lcd_debug_print_flag) {
		LCDPR("hs_hs_addr=%d, hs_he_addr=%d\n"
		"hs_vs_addr=%d, hs_ve_addr=%d\n"
		"vs_hs_addr=%d, vs_he_addr=%d\n"
		"vs_vs_addr=%d, vs_ve_addr=%d\n",
		pconf->lcd_timing.hs_hs_addr, pconf->lcd_timing.hs_he_addr,
		pconf->lcd_timing.hs_vs_addr, pconf->lcd_timing.hs_ve_addr,
		pconf->lcd_timing.vs_hs_addr, pconf->lcd_timing.vs_he_addr,
		pconf->lcd_timing.vs_vs_addr, pconf->lcd_timing.vs_ve_addr);
	}
}

static int lcd_get_config(struct lcd_config_s *pconf,
		struct platform_device *pdev)
{
	if (pdev->dev.of_node == NULL) {
		LCDPR("error: dev of_node is null\n");
		return -1;
	}
	lcd_get_model_timing(pconf, pdev);
	lcd_get_power_config(pconf, pdev);
	lcd_config_print(pconf);

	lcd_clk_config(pconf);
	lcd_tcon_config(pconf);

	return 0;
}

/* change frame_rate for different vmode */
static int lcd_vmode_change(struct lcd_config_s *pconf, int index)
{
	unsigned int pclk = pconf->lcd_timing.lcd_clk;
	unsigned char type = pconf->lcd_timing.fr_adjust_type;
	unsigned int h_period = pconf->lcd_basic.h_period;
	unsigned int v_period = pconf->lcd_basic.v_period;
	unsigned int sync_duration_num = lcd_info[index].sync_duration_num;
	unsigned int sync_duration_den = lcd_info[index].sync_duration_den;

	/* update lcd config sync_duration */
	pconf->lcd_timing.sync_duration_num = sync_duration_num;
	pconf->lcd_timing.sync_duration_den = sync_duration_den;

	/* frame rate adjust */
	switch (type) {
	case 1: /* htotal adjust */
		h_period = ((pclk / v_period) * sync_duration_den * 10) /
				sync_duration_num;
		h_period = (h_period + 5) / 10; /* round off */
		if (pconf->lcd_basic.h_period != h_period) {
			LCDPR("%s: adjust h_period %u -> %u\n",
				__func__, pconf->lcd_basic.h_period, h_period);
			pconf->lcd_basic.h_period = h_period;
		}
		break;
	case 2: /* vtotal adjust */
		v_period = ((pclk / h_period) * sync_duration_den * 10) /
				sync_duration_num;
		v_period = (v_period + 5) / 10; /* round off */
		if (pconf->lcd_basic.v_period != v_period) {
			LCDPR("%s: adjust v_period %u -> %u\n",
				__func__, pconf->lcd_basic.v_period, v_period);
			pconf->lcd_basic.v_period = v_period;
		}
		break;
	case 0: /* pixel clk adjust */
	default:
		pclk = (h_period * v_period * sync_duration_num) /
				sync_duration_den;
		if (pconf->lcd_timing.lcd_clk != pclk) {
			LCDPR("%s: adjust pclk %u.%03uMHz -> %u.%03uMHz\n",
				__func__, (pconf->lcd_timing.lcd_clk / 1000000),
				((pconf->lcd_timing.lcd_clk / 1000) % 1000),
				(pclk / 1000000), ((pclk / 1000) % 1000));
			pconf->lcd_timing.lcd_clk = pclk;
		}
		break;
	}

	return 0;
}

static void lcd_clk_gate_switch(int status)
{
	struct aml_lcd_drv_s *lcd_drv = aml_lcd_get_driver();
	struct lcd_config_s *pconf;

	if (lcd_debug_print_flag)
		LCDPR("%s\n", __func__);
	pconf = lcd_drv->lcd_config;
	if (status) {
		reset_control_deassert(pconf->rstc.encl);
		reset_control_deassert(pconf->rstc.vencl);
	} else {
		reset_control_assert(pconf->rstc.encl);
		reset_control_assert(pconf->rstc.vencl);
	}
}

static int lcd_driver_init(void)
{
	struct aml_lcd_drv_s *lcd_drv = aml_lcd_get_driver();
	struct lcd_config_s *pconf;

	pconf = lcd_drv->lcd_config;

	/* update clk & timing config */
	lcd_vmode_change(pconf, lcd_output_mode);
	switch (pconf->lcd_basic.lcd_type) {
	case LCD_LVDS:
		break;
	case LCD_VBYONE:
		set_vbyone_config(pconf);
		break;
	default:
		LCDPR("invalid lcd type\n");
		break;
	}
	lcd_clk_generate_parameter(pconf);
#ifdef CONFIG_AML_VPU
	request_vpu_clk_vmod(pconf->lcd_timing.lcd_clk, VPU_VENCL);
	switch_vpu_mem_pd_vmod(VPU_VENCL, VPU_MEM_POWER_ON);
#endif
	lcd_clk_gate_switch(1);

	/* init driver */
	switch (pconf->lcd_basic.lcd_type) {
	case LCD_LVDS:
		lvds_init(pconf);
		break;
	case LCD_VBYONE:
		vbyone_init(pconf);
		break;
	default:
		LCDPR("invalid lcd type\n");
		break;
	}
	lcd_drv->lcd_status = 1;

	return 0;
}

static void lcd_driver_disable(void)
{
	struct aml_lcd_drv_s *lcd_drv = aml_lcd_get_driver();
	struct lcd_config_s *pconf;

	pconf = lcd_drv->lcd_config;
	switch (pconf->lcd_basic.lcd_type) {
	case LCD_LVDS:
		lvds_disable(pconf);
		lcd_vcbus_setb(LVDS_GEN_CNTL, 0, 3, 1); /* disable lvds fifo */
		break;
	case LCD_VBYONE:
		vbyone_disable(pconf);
		break;
	default:
		break;
	}

	lcd_vcbus_write(ENCL_VIDEO_EN, 0); /* disable encl */

	clk_lcd_disable();

	lcd_clk_gate_switch(0);
#ifdef CONFIG_AML_VPU
	switch_vpu_mem_pd_vmod(VPU_VENCL, VPU_MEM_POWER_DOWN);
	release_vpu_clk_vmod(VPU_VENCL);
#endif
	lcd_drv->lcd_status = 0;
	LCDPR("disable driver\n");
}

int lcd_tv_probe(struct platform_device *pdev)
{
	struct aml_lcd_drv_s *lcd_drv = aml_lcd_get_driver();

	lcd_drv->version = LCD_DRV_VERSION;
	lcd_drv->vout_server_init = lcd_vout_server_init;
	lcd_drv->driver_init = lcd_driver_init;
	lcd_drv->driver_disable = lcd_driver_disable;

	lcd_get_config(lcd_drv->lcd_config, pdev);

	switch (lcd_drv->lcd_config->lcd_basic.lcd_type) {
	case LCD_VBYONE:
		vbyone_interrupt_up();
		break;
	default:
		break;
	}

	return 0;
}

int lcd_tv_remove(struct platform_device *pdev)
{
	return 0;
}

static int __init lcd_tv_boot_para_setup(char *s)
{
	if (NULL != s)
		sprintf(lcd_propname, "%s", s);

	LCDPR("lcd_propname: %s\n", lcd_propname);
	return 0;
}
__setup("panel_type=", lcd_tv_boot_para_setup);

