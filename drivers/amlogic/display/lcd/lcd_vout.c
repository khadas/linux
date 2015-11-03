
/*
 * drivers/amlogic/display/lcd/lcd_vout.c
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
#ifdef CONFIG_OF
#include <linux/of.h>
#endif
#include <linux/reset.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/vout/vinfo.h>
#include <linux/amlogic/vout/vout_notify.h>
#include <linux/amlogic/vout/lcd_vout.h>
#include <linux/amlogic/vout/lcd_notify.h>
#include "lcd_reg.h"
#include "lcd_common.h"

unsigned int lcd_debug_print_flag;
static struct aml_lcd_drv_s *lcd_driver;

static char *lcd_mode_table[] = {
	"tv",
	"tablet",
	"invalid",
};

/* *********************************************************
 * lcd config define
 * ********************************************************* */
static struct ttl_config_s lcd_ttl_config = {
	.pol_ctrl = ((0 << 3) | (1 << 2) | (0 << 1) | (0 << 0)),
	.sync_valid = ((1 << 1) | (1 << 0)),
	.swap_ctrl = ((0 << 1) | (0 << 0)),
};

static struct lvds_config_s lcd_lvds_config = {
	.lvds_vswing = 1,
	.lvds_repack = 1,
	.dual_port = 0,
	.pn_swap = 0,
	.port_swap = 0,
	.port_sel = 0,
};

static struct vbyone_config_s lcd_vbyone_config = {
	.lane_count = 8,
	.region_num = 2,
	.byte_mode = 4,
	.color_fmt = 4,
	.phy_div = 1,
	.bit_rate = 0,
};

static unsigned char dsi_init_on_table[DSI_INIT_ON_MAX] = {0xff, 0xff};
static unsigned char dsi_init_off_table[DSI_INIT_OFF_MAX] = {0xff, 0xff};
static struct dsi_config_s lcd_mipi_config = {
	.lane_num = 4,
	.bit_rate_min = 0,
	.bit_rate_max = 0,
	.factor_numerator = 0,
	.factor_denominator = 10,
	.transfer_ctrl = 0,
	.dsi_init_on = &dsi_init_on_table[0],
	.dsi_init_off = &dsi_init_off_table[0],
	.extern_init = 0,
};

static struct edp_config_s lcd_edp_config = {
	.max_lane_count = 4,
	.link_user = 0,
	.link_rate = 1,
	.lane_count = 4,
	.link_adaptive = 0,
	.vswing = 0,
	.preemphasis = 0,
	.sync_clock_mode = 1,
};

static struct lcd_power_ctrl_s lcd_power_config = {
	.cpu_gpio = {
		{.flag = 0,},
		{.flag = 0,},
		{.flag = 0,},
		{.flag = 0,},
		{.flag = 0,},
		{.flag = 0,},
		{.flag = 0,},
		{.flag = 0,},
		{.flag = 0,},
		{.flag = 0,},
	},
	.power_on_step = {
		{
			.type = LCD_POWER_TYPE_MAX,
		},
	},
	.power_off_step = {
		{
			.type = LCD_POWER_TYPE_MAX,
		},
	},
};

static struct lcd_config_s lcd_config_dft = {
	.lcd_basic = {
		.lcd_type = LCD_TYPE_MAX,
	},
	.lcd_timing = {
		.lcd_clk = 0,
		.clk_auto = 1,
		.ss_level = 0,
		.pol_ctrl = ((1 << POL_CTRL_DE) | (0 << POL_CTRL_VS) |
			(0 << POL_CTRL_HS)),

		/* for video process */
		.vso_hstart = 10,
		.vso_vstart = 10,
		.vso_user = 0, /* use default config */
	},
	.lcd_effect = {
		.rgb_base_addr = 0xf0,
		.rgb_coeff_addr = 0x74a,
		.dith_user = 0,
		.gamma_ctrl = ((0 << GAMMA_CTRL_REV) | (1 << GAMMA_CTRL_EN)),
		.gamma_r_coeff = 100,
		.gamma_g_coeff = 100,
		.gamma_b_coeff = 100,
	},
	.lcd_control = {
		.ttl_config = &lcd_ttl_config,
		.lvds_config = &lcd_lvds_config,
		.vbyone_config = &lcd_vbyone_config,
		.mipi_config = &lcd_mipi_config,
		.edp_config = &lcd_edp_config,
	},
	.lcd_power = &lcd_power_config,
};

struct aml_lcd_drv_s *aml_lcd_get_driver(void)
{
	return lcd_driver;
}
/* ********************************************************* */

static void lcd_power_ctrl(int status)
{
	struct lcd_power_ctrl_s *lcd_power = lcd_driver->lcd_config->lcd_power;
	struct lcd_power_step_s *power_step;
#ifdef CONFIG_AML_LCD_EXTERN
	struct aml_lcd_extern_driver_s *ext_drv;
#endif
	int i, index;
	int ret = 0;

	i = 0;
	while (i < LCD_PWR_STEP_MAX) {
		if (status)
			power_step = &lcd_power->power_on_step[i];
		else
			power_step = &lcd_power->power_off_step[i];

		if (power_step->type >= LCD_POWER_TYPE_MAX)
			break;
		if (lcd_debug_print_flag) {
			LCDPR("power_ctrl: %d, step %d\n", status, i);
			LCDPR("type=%d, index=%d, value=%d, delay=%d\n",
				power_step->type, power_step->index,
				power_step->value, power_step->delay);
		}
		switch (power_step->type) {
		case LCD_POWER_TYPE_CPU:
			index = power_step->index;
			lcd_cpu_gpio_set(index, power_step->value);
			break;
		case LCD_POWER_TYPE_PMU:
			LCDPR("to do\n");
			break;
		case LCD_POWER_TYPE_SIGNAL:
			if (status)
				ret = lcd_driver->driver_init();
			else
				lcd_driver->driver_disable();
			break;
#ifdef CONFIG_AML_LCD_EXTERN
		case LCD_POWER_TYPE_EXTERN:
			ext_drv = aml_lcd_extern_get_driver(index);
			if (status)
				ext_drv->power_on();
			else
				ext_drv->power_off();
			break;
#endif
		default:
			break;
		}
		if (power_step->delay)
			mdelay(power_step->delay);
		i++;
	}

	if (lcd_debug_print_flag)
		LCDPR("%s: %d\n", __func__, status);
}

static void lcd_module_enable(void)
{
	LCDPR("driver version: %s\n", lcd_driver->version);
	lcd_driver->power_ctrl(1);
}

static void lcd_module_disable(void)
{
	lcd_driver->power_ctrl(0);
}

/* ****************************************
 * lcd notify
 * **************************************** */
static int lcd_power_notifier(struct notifier_block *nb,
		unsigned long event, void *data)
{
	LCDPR("%s: 0x%lx\n", __func__, event);

	if (event & LCD_EVENT_LCD_ON)
		lcd_driver->module_enable();
	else if (event & LCD_EVENT_LCD_OFF)
		lcd_driver->module_disable();
	else
		return NOTIFY_DONE;

	return NOTIFY_OK;
}

static struct notifier_block lcd_power_nb = {
	.notifier_call = lcd_power_notifier,
	.priority = LCD_PRIORITY_POWER_LCD,
};
/* **************************************** */

static void lcd_chip_detect(void)
{
	unsigned int cpu_type;

	cpu_type = get_cpu_type();
	switch (cpu_type) {
	case MESON_CPU_MAJOR_ID_M8:
		lcd_driver->chip_type = LCD_CHIP_M8;
		break;
	case MESON_CPU_MAJOR_ID_M8B:
		lcd_driver->chip_type = LCD_CHIP_M8B;
		break;
	case MESON_CPU_MAJOR_ID_M8M2:
		lcd_driver->chip_type = LCD_CHIP_M8M2;
		break;
	case MESON_CPU_MAJOR_ID_MG9TV:
		lcd_driver->chip_type = LCD_CHIP_G9TV;
		break;
	case MESON_CPU_MAJOR_ID_GXTVBB:
		lcd_driver->chip_type = LCD_CHIP_GXTVBB;
		break;
	default:
		lcd_driver->chip_type = LCD_CHIP_MAX;
	}

	if (lcd_debug_print_flag)
		LCDPR("check chip: %d\n", lcd_driver->chip_type);
}

static void lcd_init_vout(void)
{
	lcd_driver->vout_server_init();
}

static int lcd_mode_probe(struct platform_device *pdev)
{
	const char *str;
	int i;
	int ret = 0;

	if (pdev->dev.of_node == NULL) {
		LCDPR("error: dev of_node is null\n");
		lcd_driver->lcd_mode = LCD_MODE_MAX;
		return -1;
	}

	/* lcd driver assign */
	ret = of_property_read_string(pdev->dev.of_node, "mode", &str);
	if (ret) {
		str = "none";
		LCDPR("error: failed to get mode\n");
	}
	for (i = 0; i < LCD_MODE_MAX; i++) {
		if (!strcmp(str, lcd_mode_table[i]))
			break;
	}
	LCDPR("detect mode: %s\n", str);
	lcd_driver->lcd_mode = i;

	lcd_driver->lcd_config = &lcd_config_dft;
	lcd_driver->lcd_status = 1;
	lcd_driver->vpp_sel = 1;
	lcd_driver->power_ctrl = lcd_power_ctrl;
	lcd_driver->module_enable = lcd_module_enable;
	lcd_driver->module_disable = lcd_module_disable;

	switch (lcd_driver->lcd_mode) {
#ifdef CONFIG_AML_LCD_TV
	case LCD_MODE_TV:
		lcd_tv_probe(pdev);
		break;
#endif
#ifdef CONFIG_AML_LCD_TABLET
	case LCD_MODE_TABLET:
		lcd_tablet_probe(pdev);
		break;
#endif
	default:
		LCDPR("invalid lcd mode\n");
		break;
	}
	return 0;
}

static int lcd_mode_remove(struct platform_device *pdev)
{
	switch (lcd_driver->lcd_mode) {
#ifdef CONFIG_AML_LCD_TV
	case LCD_MODE_TV:
		lcd_tv_remove(pdev);
		break;
#endif
#ifdef CONFIG_AML_LCD_TABLET
	case LCD_MODE_TABLET:
		lcd_tablet_remove(pdev);
		break;
#endif
	default:
		LCDPR("invalid lcd mode\n");
		break;
	}
	return 0;
}

static int lcd_bl_select_notifier(struct notifier_block *nb,
		unsigned long event, void *data)
{
	unsigned int *index;

	/* LCDPR("%s: 0x%lx\n", __func__, event); */
	if ((event & LCD_EVENT_BACKLIGHT_SEL) == 0)
		return NOTIFY_DONE;

	index = (unsigned int *)data;
	*index = lcd_driver->lcd_config->backlight_index;

	return NOTIFY_OK;
}

static struct notifier_block lcd_bl_select_nb = {
	.notifier_call = lcd_bl_select_notifier,
};

static int lcd_reboot_notifier(struct notifier_block *nb,
		unsigned long event, void *data)
{
	LCDPR("%s: %lu\n", __func__, event);
	if (lcd_driver->lcd_status == 0)
		return NOTIFY_DONE;

	aml_lcd_notifier_call_chain(LCD_EVENT_POWER_OFF, NULL);

	return NOTIFY_OK;
}
static struct notifier_block lcd_reboot_nb = {
	.notifier_call = lcd_reboot_notifier,
};

static int lcd_probe(struct platform_device *pdev)
{
	int ret = 0;

#ifdef LCD_DEBUG_INFO
	lcd_debug_print_flag = 1;
#else
	lcd_debug_print_flag = 0;
#endif
	lcd_driver = kmalloc(sizeof(struct aml_lcd_drv_s), GFP_KERNEL);
	if (!lcd_driver) {
		LCDPR("probe: Not enough memory\n");
		return -ENOMEM;
	}
	lcd_driver->dev = &pdev->dev;

	lcd_chip_detect();
	lcd_ioremap();
	lcd_mode_probe(pdev);
	lcd_clk_config_probe();
	lcd_init_vout();

	lcd_class_creat();
	ret = aml_lcd_notifier_register(&lcd_bl_select_nb);
	if (ret)
		LCDPR("register aml_bl_on_notifier failed\n");
	ret = aml_lcd_notifier_register(&lcd_power_nb);
	if (ret)
		LCDPR("register lcd_power_notifier failed\n");
	ret = register_reboot_notifier(&lcd_reboot_nb);
	if (ret)
		LCDPR("register lcd_reboot_notifier failed\n");

	LCDPR("probe ok\n");
	return 0;
}

static int lcd_remove(struct platform_device *pdev)
{
	if (lcd_driver) {
		lcd_class_remove();
		lcd_mode_remove(pdev);
		kfree(lcd_driver);
		lcd_driver = NULL;
	}
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id lcd_dt_match[] = {
	{
		.compatible = "amlogic, lcd",
	},
	{},
};
#endif

static struct platform_driver lcd_platform_driver = {
	.probe = lcd_probe,
	.remove = lcd_remove,
	.driver = {
		.name = "mesonlcd",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = lcd_dt_match,
#endif
	},
};

static int __init lcd_init(void)
{
	if (platform_driver_register(&lcd_platform_driver)) {
		LCDPR("failed to register lcd driver module\n");
		return -ENODEV;
	}

	return 0;
}

static void __exit lcd_exit(void)
{
	platform_driver_unregister(&lcd_platform_driver);
}

arch_initcall(lcd_init);
module_exit(lcd_exit);

MODULE_DESCRIPTION("Meson LCD Panel Driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Amlogic, Inc.");
