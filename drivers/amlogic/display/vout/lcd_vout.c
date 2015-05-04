/*
 * drivers/amlogic/display/vout/lcd_vout.c
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
#include <asm/fiq.h>
#include <linux/delay.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/of.h>
#include <linux/reset.h>
#include <linux/amlogic/vout/vinfo.h>
#include <linux/amlogic/vout/vout_notify.h>
#include <linux/amlogic/vout/lcdoutc.h>
#include "lcd/lcd_reg.h"

#define PANEL_NAME		"panel"

struct lcd_dev_s {
	struct Lcd_Config_s *pConf;
	struct vinfo_s lcd_info;
};

static struct lcd_dev_s *pDev;

#ifdef LCD_DEBUG_INFO
unsigned int lcd_print_flag = 1;
#else
unsigned int lcd_print_flag = 0;
#endif
void lcd_print(const char *fmt, ...)
{
	va_list args;

	if (lcd_print_flag == 0)
		return;
	va_start(args, fmt);
	vprintk(fmt, args);
	va_end(args);
}

void _enable_backlight(void)
{
	pDev->pConf->lcd_power_ctrl.backlight_power_ctrl(ON);
}
void _disable_backlight(void)
{
	pDev->pConf->lcd_power_ctrl.backlight_power_ctrl(OFF);
}

static void _lcd_module_enable(void)
{
	pDev->pConf->lcd_misc_ctrl.module_enable();
}

static void _lcd_module_disable(void)
{
	pDev->pConf->lcd_misc_ctrl.module_disable();
}

static const struct vinfo_s *lcd_get_current_info(void)
{
	if (pDev == NULL) {
		DPRINT("[error] no lcd device exist!\n");
		return NULL;
	} else {
		return &pDev->lcd_info;
	}
}

DEFINE_MUTEX(lcd_vout_mutex);
static int lcd_set_current_vmode(enum vmode_e mode)
{
	mutex_lock(&lcd_vout_mutex);
	if (VMODE_LCD != (mode & VMODE_MODE_BIT_MASK)) {
		mutex_unlock(&lcd_vout_mutex);
		return -EINVAL;
	}

	pDev->pConf->lcd_misc_ctrl.vpp_sel = 0;
	lcd_reg_write(VPP_POSTBLEND_H_SIZE, pDev->lcd_info.width);

	if (!(mode&VMODE_LOGO_BIT_MASK)) {
		_disable_backlight();
		_lcd_module_enable();
		_enable_backlight();
	}
	if (VMODE_INIT_NULL == pDev->lcd_info.mode)
		pDev->lcd_info.mode = VMODE_LCD;

	mutex_unlock(&lcd_vout_mutex);
	return 0;
}

#ifdef CONFIG_AM_TV_OUTPUT2
static int lcd_set_current_vmode2(enum vmode_e mode)
{
	mutex_lock(&lcd_vout_mutex);
	if (mode != VMODE_LCD) {
		mutex_unlock(&lcd_vout_mutex);
		return -EINVAL;
	}
	_disable_backlight();
	pDev->pConf->lcd_misc_ctrl.vpp_sel = 1;

	lcd_reg_write(VPP2_POSTBLEND_H_SIZE, pDev->lcd_info.width);

	_lcd_module_enable();
	if (VMODE_INIT_NULL == pDev->lcd_info.mode)
		pDev->lcd_info.mode = VMODE_LCD;
	_enable_backlight();
	mutex_unlock(&lcd_vout_mutex);
	return 0;
}
#endif

static enum vmode_e lcd_validate_vmode(char *mode)
{
	if (mode == NULL)
		return VMODE_MAX;

	if ((strncmp(mode, PANEL_NAME, strlen(PANEL_NAME))) == 0)
		return VMODE_LCD;

	return VMODE_MAX;
}
static int lcd_vmode_is_supported(enum vmode_e mode)
{
	mode &= VMODE_MODE_BIT_MASK;
	if (mode == VMODE_LCD)
		return true;
	return false;
}

static int lcd_vout_disable(enum vmode_e cur_vmod)
{
	mutex_lock(&lcd_vout_mutex);
	_disable_backlight();
	_lcd_module_disable();
	mutex_unlock(&lcd_vout_mutex);
	return 0;
}

#ifdef CONFIG_PM
static int lcd_suspend(void)
{
	mutex_lock(&lcd_vout_mutex);
	BUG_ON(pDev == NULL);
	DPRINT("lcd_suspend\n");
	_disable_backlight();
	_lcd_module_disable();
	mutex_unlock(&lcd_vout_mutex);
	return 0;
}
static int lcd_resume(void)
{
	mutex_lock(&lcd_vout_mutex);
	DPRINT("lcd_resume\n");
	_lcd_module_enable();
	_enable_backlight();
	mutex_unlock(&lcd_vout_mutex);
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

#ifdef CONFIG_AM_TV_OUTPUT2
static struct vout_server_s lcd_vout2_server = {
	.name = "lcd_vout2_server",
	.op = {
		.get_vinfo = lcd_get_current_info,
		.set_vmode = lcd_set_current_vmode2,
		.validate_vmode = lcd_validate_vmode,
		.vmode_is_supported = lcd_vmode_is_supported,
		.disable = lcd_vout_disable,
#ifdef CONFIG_PM
		.vout_suspend = lcd_suspend,
		.vout_resume = lcd_resume,
#endif
	},
};
#endif

void lcd_init_vout(void)
{
	struct vinfo_s *info;

	info = &(pDev->lcd_info);
	info->name = PANEL_NAME;
	info->mode = VMODE_LCD;
	info->width = pDev->pConf->lcd_basic.h_active;
	info->height = pDev->pConf->lcd_basic.v_active;
	info->field_height = pDev->pConf->lcd_basic.v_active;
	info->aspect_ratio_num = pDev->pConf->lcd_basic.screen_ratio_width;
	info->aspect_ratio_den = pDev->pConf->lcd_basic.screen_ratio_height;
	info->screen_real_width = pDev->pConf->lcd_basic.h_active_area;
	info->screen_real_height = pDev->pConf->lcd_basic.v_active_area;
	info->sync_duration_num = pDev->pConf->lcd_timing.sync_duration_num;
	info->sync_duration_den = pDev->pConf->lcd_timing.sync_duration_den;
	info->video_clk = pDev->pConf->lcd_timing.lcd_clk;

	/* add lcd actual active area size */
	DPRINT("lcd actual active area size: %d %d (mm)\n",
		pDev->pConf->lcd_basic.h_active_area,
		pDev->pConf->lcd_basic.v_active_area);
	vout_register_server(&lcd_vout_server);
#ifdef CONFIG_AM_TV_OUTPUT2
	vout2_register_server(&lcd_vout2_server);
#endif
}

static int lcd_reboot_notifier(struct notifier_block *nb,
		unsigned long state, void *cmd)
{
	lcd_print("[%s]: %lu\n", __func__, state);
	if (pDev->pConf->lcd_misc_ctrl.lcd_status == 0)
		return NOTIFY_DONE;

	_disable_backlight();
	_lcd_module_disable();

	return NOTIFY_OK;
}

#ifdef CONFIG_USE_OF
static const struct of_device_id lcd_dt_match[] = {
	{
		.compatible = "amlogic, lcd",
	},
	{},
};
#else
#define lcd_dt_match NULL
#endif

static struct notifier_block lcd_reboot_nb;
static int lcd_probe(struct platform_device *pdev)
{
	int ret = 0;

	pDev = kmalloc(sizeof(*pDev), GFP_KERNEL);
	if (!pDev) {
		DPRINT("[lcd probe]: Not enough memory\n");
		return -ENOMEM;
	}

#ifdef CONFIG_USE_OF
	pDev->pConf = get_lcd_of_config();
#else
	pDev->pConf = (struct Lcd_Config_s *)(pdev->dev.platform_data->pConf);
#endif
	lcd_config_probe(pDev->pConf, pdev);

	pDev->pConf->lcd_misc_ctrl.print_version();
	lcd_config_init(pDev->pConf);
	lcd_init_vout();

	lcd_reboot_nb.notifier_call = lcd_reboot_notifier;
	ret = register_reboot_notifier(&lcd_reboot_nb);
	if (ret)
		DPRINT("notifier register lcd_reboot_notifier fail!\n");

	DPRINT("LCD probe ok\n");
	return 0;
}

static int lcd_remove(struct platform_device *pdev)
{
	unregister_reboot_notifier(&lcd_reboot_nb);

	lcd_config_remove(pDev->pConf);
	kfree(pDev);

	return 0;
}

/* device tree */
static struct platform_driver lcd_driver = {
	.probe = lcd_probe,
	.remove = lcd_remove,
	.driver = {
		.name = "mesonlcd",
		.owner = THIS_MODULE,
#ifdef CONFIG_USE_OF
		.of_match_table = lcd_dt_match,
#endif
	},
};

static int __init lcd_init(void)
{
	lcd_print("LCD driver init\n");
	if (platform_driver_register(&lcd_driver)) {
		DPRINT("failed to register lcd driver module\n");
		return -ENODEV;
	}

	return 0;
}

static void __exit lcd_exit(void)
{
	platform_driver_unregister(&lcd_driver);
}

subsys_initcall(lcd_init);
module_exit(lcd_exit);

MODULE_DESCRIPTION("Meson LCD Panel Driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Amlogic, Inc.");
