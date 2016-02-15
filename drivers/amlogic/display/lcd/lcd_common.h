
/*
 * drivers/amlogic/display/lcd/lcd_common.h
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

#ifndef __AML_LCD_COMMON_H__
#define __AML_LCD_COMMON_H__
#include <linux/platform_device.h>
#include <linux/amlogic/vout/lcd_vout.h>
#include "lcd_clk_config.h"

#define VPP_OUT_SATURATE            (1 << 0)

/* lcd common */
extern int lcd_type_str_to_type(const char *str);
extern char *lcd_type_type_to_str(int type);
extern int lcd_mode_str_to_mode(const char *str);
extern char *lcd_mode_mode_to_str(int mode);

extern void lcd_cpu_gpio_register(unsigned int index);
extern void lcd_cpu_gpio_set(unsigned int index, int value);
extern unsigned int lcd_cpu_gpio_get(unsigned int index);
extern void lcd_ttl_pinmux_set(int status);
extern int lcd_get_power_config(struct lcd_config_s *pconf,
		struct platform_device *pdev);

extern void lcd_tcon_config(struct lcd_config_s *pconf);
extern int lcd_vmode_change(struct lcd_config_s *pconf);
extern void lcd_clk_gate_switch(int status);

/* lcd debug */
extern int lcd_class_creat(void);
extern int lcd_class_remove(void);

/* lcd driver */
#ifdef CONFIG_AML_LCD_TV
extern int lcd_tv_probe(struct platform_device *pdev);
extern int lcd_tv_remove(struct platform_device *pdev);
#endif
#ifdef CONFIG_AML_LCD_TABLET
extern int lcd_tablet_probe(struct platform_device *pdev);
extern int lcd_tablet_remove(struct platform_device *pdev);
#endif

#endif
