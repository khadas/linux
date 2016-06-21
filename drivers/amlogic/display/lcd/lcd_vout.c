
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
#include <linux/amlogic/vout/lcd_unifykey.h>
#ifdef CONFIG_AML_LCD_EXTERN
#include <linux/amlogic/vout/lcd_extern.h>
#endif
#include "lcd_reg.h"
#include "lcd_common.h"

unsigned int lcd_debug_print_flag;
static struct aml_lcd_drv_s *lcd_driver;

static char lcd_propname[20] = "lvds_0";

/* *********************************************************
 * lcd config define
 * ********************************************************* */
static struct ttl_config_s lcd_ttl_config = {
	.clk_pol = 0,
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
	.phy_vswing = LVDS_PHY_VSWING_DFT,
	.phy_preem = LVDS_PHY_PREEM_DFT,
};

static struct vbyone_config_s lcd_vbyone_config = {
	.lane_count = 8,
	.region_num = 2,
	.byte_mode = 4,
	.color_fmt = 4,
	.phy_div = 1,
	.bit_rate = 0,
	.phy_vswing = VX1_PHY_VSWING_DFT,
	.phy_preem = VX1_PHY_PREEM_DFT,
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
	.lcd_propname = lcd_propname,
	.lcd_basic = {
		.lcd_type = LCD_TYPE_MAX,
	},
	.lcd_timing = {
		.lcd_clk = 0,
		.clk_auto = 1,
		.ss_level = 0,
		.fr_adjust_type = 0,

		/* for video process */
		.vso_hstart = 10,
		.vso_vstart = 10,
		.vso_user = 0, /* use default config */
	},
	.lcd_effect = {
		.rgb_base_addr = 0xf0,
		.rgb_coeff_addr = 0x74a,
		.dith_user = 0,
		/*
		.gamma_ctrl = ((0 << GAMMA_CTRL_REV) | (0 << GAMMA_CTRL_EN)),
		.gamma_r_coeff = 100,
		.gamma_g_coeff = 100,
		.gamma_b_coeff = 100,
		*/
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

static struct vinfo_s lcd_vinfo = {
	.name = "panel",
	.mode = VMODE_LCD,
	.viu_color_fmt = TVIN_RGB444,
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

	LCDPR("%s: %d\n", __func__, status);
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
			index = power_step->index;
			ext_drv = aml_lcd_extern_get_driver(index);
			if (ext_drv) {
				if (status) {
					if (ext_drv->power_on)
						ext_drv->power_on();
					else
						LCDERR("no ext power on\n");
				} else {
					if (ext_drv->power_off)
						ext_drv->power_off();
					else
						LCDERR("no ext power off\n");
				}
			}
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
		LCDPR("%s: %d finished\n", __func__, status);
}

static void lcd_module_enable(void)
{
	/*LCDPR("driver version: %s\n", lcd_driver->version);*/
	lcd_driver->driver_init_pre();
	lcd_driver->power_ctrl(1);
	lcd_driver->lcd_status = 1;
}

static void lcd_module_disable(void)
{
	lcd_driver->lcd_status = 0;
	lcd_driver->power_ctrl(0);
}

static void lcd_module_reset(void)
{
	lcd_module_disable();
	mdelay(200);
	lcd_module_enable();
}

/* ****************************************
 * lcd notify
 * **************************************** */
static int lcd_power_notifier(struct notifier_block *nb,
		unsigned long event, void *data)
{
	if (lcd_debug_print_flag)
		LCDPR("%s: 0x%lx\n", __func__, event);

	if (event & LCD_EVENT_LCD_ON)
		lcd_module_enable();
	else if (event & LCD_EVENT_LCD_OFF)
		lcd_module_disable();
	else
		return NOTIFY_DONE;

	return NOTIFY_OK;
}

static struct notifier_block lcd_power_nb = {
	.notifier_call = lcd_power_notifier,
	.priority = LCD_PRIORITY_POWER_LCD,
};

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
	case MESON_CPU_MAJOR_ID_TXL:
		lcd_driver->chip_type = LCD_CHIP_TXL;
		break;
	default:
		lcd_driver->chip_type = LCD_CHIP_MAX;
	}

	if (lcd_debug_print_flag)
		LCDPR("check chip: %d\n", lcd_driver->chip_type);
}

static void lcd_init_vout(void)
{
	if (lcd_driver->vout_server_init)
		lcd_driver->vout_server_init();
	else
		LCDERR("no vout_server_init function\n");
}

static int lcd_reboot_notifier(struct notifier_block *nb,
		unsigned long event, void *data)
{
	if (lcd_debug_print_flag)
		LCDPR("%s: %lu\n", __func__, event);
	if (lcd_driver->lcd_status == 0)
		return NOTIFY_DONE;

	aml_lcd_notifier_call_chain(LCD_EVENT_POWER_OFF, NULL);

	return NOTIFY_OK;
}
static struct notifier_block lcd_reboot_nb = {
	.notifier_call = lcd_reboot_notifier,
};

static int lcd_mode_probe(struct device *dev)
{
	int ret;

	switch (lcd_driver->lcd_mode) {
#ifdef CONFIG_AML_LCD_TV
	case LCD_MODE_TV:
		lcd_tv_probe(dev);
		break;
#endif
#ifdef CONFIG_AML_LCD_TABLET
	case LCD_MODE_TABLET:
		lcd_tablet_probe(dev);
		break;
#endif
	default:
		LCDPR("invalid lcd mode\n");
		break;
	}

	lcd_class_creat();
	ret = aml_lcd_notifier_register(&lcd_bl_select_nb);
	if (ret)
		LCDERR("register aml_bl_on_notifier failed\n");
	ret = aml_lcd_notifier_register(&lcd_power_nb);
	if (ret)
		LCDPR("register lcd_power_notifier failed\n");
	ret = register_reboot_notifier(&lcd_reboot_nb);
	if (ret)
		LCDERR("register lcd_reboot_notifier failed\n");

	return 0;
}

static int lcd_mode_remove(struct device *dev)
{
	switch (lcd_driver->lcd_mode) {
#ifdef CONFIG_AML_LCD_TV
	case LCD_MODE_TV:
		lcd_tv_remove(dev);
		break;
#endif
#ifdef CONFIG_AML_LCD_TABLET
	case LCD_MODE_TABLET:
		lcd_tablet_remove(dev);
		break;
#endif
	default:
		LCDPR("invalid lcd mode\n");
		break;
	}
	return 0;
}

static void lcd_config_probe_delayed(struct work_struct *work)
{
	int key_init_flag = 0;
	int i = 0;
	int ret;

	if (lcd_driver->lcd_key_valid) {
		key_init_flag = key_unify_get_init_flag();
		while (key_init_flag == 0) {
			if (i++ >= LCD_UNIFYKEY_WAIT_TIMEOUT)
				break;
			msleep(20);
			key_init_flag = key_unify_get_init_flag();
		}
		LCDPR("key_init_flag=%d, i=%d\n", key_init_flag, i);
	}
	ret = lcd_mode_probe(lcd_driver->dev);
	if (ret) {
		kfree(lcd_driver);
		lcd_driver = NULL;
		LCDERR("probe exit\n");
	}
}

static int lcd_config_probe(void)
{
	const char *str;
	unsigned int val;
	int ret = 0;

	if (lcd_driver->dev->of_node == NULL) {
		LCDERR("dev of_node is null\n");
		lcd_driver->lcd_mode = LCD_MODE_MAX;
		return -1;
	}

	/* lcd driver assign */
	ret = of_property_read_string(lcd_driver->dev->of_node, "mode", &str);
	if (ret) {
		str = "none";
		LCDERR("failed to get mode\n");
		return -1;
	}
	lcd_driver->lcd_mode = lcd_mode_str_to_mode(str);
	ret = of_property_read_u32(lcd_driver->dev->of_node,
		"fr_auto_policy", &val);
	if (ret) {
		if (lcd_debug_print_flag)
			LCDPR("failed to get fr_auto_policy\n");
		lcd_driver->fr_auto_policy = 0;
	} else {
		lcd_driver->fr_auto_policy = (unsigned char)val;
	}
	ret = of_property_read_u32(lcd_driver->dev->of_node, "key_valid", &val);
	if (ret) {
		if (lcd_debug_print_flag)
			LCDPR("failed to get key_valid\n");
		lcd_driver->lcd_key_valid = 0;
	} else {
		lcd_driver->lcd_key_valid = (unsigned char)val;
	}
	LCDPR("detect mode: %s, fr_auto_policy: %d, key_valid: %d\n",
		str, lcd_driver->fr_auto_policy, lcd_driver->lcd_key_valid);

	lcd_driver->lcd_info = &lcd_vinfo;
	lcd_driver->lcd_config = &lcd_config_dft;
	lcd_driver->lcd_config->pinmux_flag = 0;
	lcd_driver->vpp_sel = 1;
	lcd_driver->power_ctrl = lcd_power_ctrl;
	lcd_driver->module_reset = lcd_module_reset;
	if (lcd_vcbus_read(ENCL_VIDEO_EN))
		lcd_driver->lcd_status = 1;
	else
		lcd_driver->lcd_status = 0;
	LCDPR("status: %d\n", lcd_driver->lcd_status);

	if (lcd_driver->lcd_key_valid) {
		if (lcd_driver->workqueue) {
			queue_delayed_work(lcd_driver->workqueue,
				&lcd_driver->lcd_delayed_work,
				msecs_to_jiffies(2000));
		} else {
			LCDPR("Warning: no lcd_probe_delayed workqueue\n");
			ret = lcd_mode_probe(lcd_driver->dev);
			if (ret) {
				kfree(lcd_driver);
				lcd_driver = NULL;
				LCDERR("probe exit\n");
			}
		}
	} else {
		ret = lcd_mode_probe(lcd_driver->dev);
		if (ret) {
			kfree(lcd_driver);
			lcd_driver = NULL;
			LCDERR("probe exit\n");
		}
	}

	return 0;
}

static int lcd_probe(struct platform_device *pdev)
{
#ifdef LCD_DEBUG_INFO
	lcd_debug_print_flag = 1;
#else
	lcd_debug_print_flag = 0;
#endif
	lcd_driver = kmalloc(sizeof(struct aml_lcd_drv_s), GFP_KERNEL);
	if (!lcd_driver) {
		LCDERR("probe: Not enough memory\n");
		return -ENOMEM;
	}
	lcd_driver->dev = &pdev->dev;

	/* init workqueue */
	INIT_DELAYED_WORK(&lcd_driver->lcd_delayed_work,
		lcd_config_probe_delayed);
	lcd_driver->workqueue = create_workqueue("lcd_probe_queue");
	if (lcd_driver->workqueue == NULL)
		LCDERR("can't create lcd workqueue\n");

	lcd_chip_detect();
	lcd_ioremap();
	lcd_clk_config_probe();
	lcd_config_probe();
	lcd_init_vout();

	LCDPR("%s\n", __func__);
	return 0;
}

static int lcd_remove(struct platform_device *pdev)
{
	int ret;

	ret = cancel_delayed_work(&lcd_driver->lcd_delayed_work);
	if (lcd_driver->workqueue)
		destroy_workqueue(lcd_driver->workqueue);

	if (lcd_driver) {
		aml_lcd_notifier_unregister(&lcd_power_nb);
		aml_lcd_notifier_unregister(&lcd_bl_select_nb);
		lcd_class_remove();
		lcd_mode_remove(lcd_driver->dev);
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

static int __init lcd_boot_para_setup(char *s)
{
	if (NULL != s)
		sprintf(lcd_propname, "%s", s);

	LCDPR("panel_type: %s\n", lcd_propname);
	return 0;
}
__setup("panel_type=", lcd_boot_para_setup);

MODULE_DESCRIPTION("Meson LCD Panel Driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Amlogic, Inc.");
