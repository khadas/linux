/*
 * drivers/amlogic/display/vout/lcd/lcd_common.c
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
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/reset.h>
#include <linux/amlogic/vout/lcd_vout.h>
#include "lcd_common.h"
#include "lcd_reg.h"

/* **********************************
 * lcd type
 * ********************************** */
struct lcd_type_match_s {
	char *name;
	enum lcd_type_e type;
};

static struct lcd_type_match_s lcd_type_match_table[] = {
	{"ttl",     LCD_TTL},
	{"lvds",    LCD_LVDS},
	{"vbyone",  LCD_VBYONE},
	{"mipi",    LCD_MIPI},
	{"edp",     LCD_EDP},
	{"invalid", LCD_TYPE_MAX},
};

int lcd_type_str_to_type(const char *str)
{
	int i;
	int type = LCD_TYPE_MAX;

	for (i = 0; i < LCD_TYPE_MAX; i++) {
		if (!strcmp(str, lcd_type_match_table[i].name)) {
			type = lcd_type_match_table[i].type;
			break;
		}
	}
	return type;
}

char *lcd_type_type_to_str(int type)
{
	int i;
	char *str = lcd_type_match_table[LCD_TYPE_MAX].name;

	for (i = 0; i < LCD_TYPE_MAX; i++) {
		if (type == lcd_type_match_table[i].type) {
			str = lcd_type_match_table[i].name;
			break;
		}
	}
	return str;
}

static char *lcd_mode_table[] = {
	"tv",
	"tablet",
	"invalid",
};

int lcd_mode_str_to_mode(const char *str)
{
	int mode;

	for (mode = 0; mode < ARRAY_SIZE(lcd_mode_table); mode++) {
		if (!strcmp(str, lcd_mode_table[mode]))
			break;
	}
	return mode;
}

char *lcd_mode_mode_to_str(int mode)
{
	return lcd_mode_table[mode];
}

/* **********************************
 * lcd gpio
 * ********************************** */
#if 0
#define lcd_gpio_request(dev, str)        gpiod_get(dev, str)
#define lcd_gpio_free(gdesc)              gpiod_put(gdesc)
#define lcd_gpio_input(gdesc)             gpiod_direction_input(gdesc)
#define lcd_gpio_output(gdesc, val)       gpiod_direction_output(gdesc, val)
#define lcd_gpio_get_value(gdesc)         gpiod_get_value(gdesc)
#define lcd_gpio_set_value(gdesc, val)    gpiod_set_value(gdesc, val)
#endif

void lcd_cpu_gpio_register(unsigned int index)
{
	struct aml_lcd_drv_s *lcd_drv = aml_lcd_get_driver();
	struct lcd_cpu_gpio_s *cpu_gpio;
	const char *str;
	int ret;

	if (index >= LCD_CPU_GPIO_NUM_MAX) {
		LCDERR("gpio index %d, exit\n", index);
		return;
	}
	cpu_gpio = &lcd_drv->lcd_config->lcd_power->cpu_gpio[index];
	if (cpu_gpio->flag) {
		if (lcd_debug_print_flag) {
			LCDPR("gpio %s[%d] is already registered\n",
				cpu_gpio->name, index);
		}
		return;
	}

	/* get gpio name */
	ret = of_property_read_string_index(lcd_drv->dev->of_node,
		"lcd_cpu_gpio_names", index, &str);
	if (ret) {
		LCDERR("failed to get lcd_cpu_gpio_names: %d\n", index);
		str = "unknown";
	}
	strcpy(cpu_gpio->name, str);

	/* request gpio */
	cpu_gpio->gpio = devm_gpiod_get_index(lcd_drv->dev, "lcd_cpu", index);
	if (IS_ERR(cpu_gpio->gpio)) {
		LCDERR("register gpio %s[%d]: %p, err: %ld\n",
			cpu_gpio->name, index, cpu_gpio->gpio,
			IS_ERR(cpu_gpio->gpio));
	} else {
		cpu_gpio->flag = 1;
		if (lcd_debug_print_flag) {
			LCDPR("register gpio %s[%d]: %p\n",
				cpu_gpio->name, index, cpu_gpio->gpio);
		}
	}
}

void lcd_cpu_gpio_set(unsigned int index, int value)
{
	struct aml_lcd_drv_s *lcd_drv = aml_lcd_get_driver();
	struct lcd_cpu_gpio_s *cpu_gpio;

	if (index >= LCD_CPU_GPIO_NUM_MAX) {
		LCDERR("gpio index %d, exit\n", index);
		return;
	}
	cpu_gpio = &lcd_drv->lcd_config->lcd_power->cpu_gpio[index];
	if (cpu_gpio->flag == 0) {
		LCDERR("gpio [%d] is not registered\n", index);
		return;
	}
	if (IS_ERR(cpu_gpio->gpio)) {
		LCDERR("gpio %s[%d]: %p, err: %ld\n",
			cpu_gpio->name, index, cpu_gpio->gpio,
			PTR_ERR(cpu_gpio->gpio));
		return;
	}

	switch (value) {
	case LCD_GPIO_OUTPUT_LOW:
	case LCD_GPIO_OUTPUT_HIGH:
		gpiod_direction_output(cpu_gpio->gpio, value);
		break;
	case LCD_GPIO_INPUT:
	default:
		gpiod_direction_input(cpu_gpio->gpio);
		break;
	}
	if (lcd_debug_print_flag) {
		LCDPR("set gpio %s[%d] value: %d\n",
			cpu_gpio->name, index, value);
	}
}

unsigned int lcd_cpu_gpio_get(unsigned int index)
{
	struct aml_lcd_drv_s *lcd_drv = aml_lcd_get_driver();
	struct lcd_cpu_gpio_s *cpu_gpio;

	cpu_gpio = &lcd_drv->lcd_config->lcd_power->cpu_gpio[index];
	if (cpu_gpio->flag == 0) {
		LCDERR("gpio[%d] is not registered\n", index);
		return -1;
	}
	if (IS_ERR(cpu_gpio->gpio)) {
		LCDERR("gpio[%d]: %p, err: %ld\n",
			index, cpu_gpio->gpio, PTR_ERR(cpu_gpio->gpio));
		return -1;
	}

	return gpiod_get_value(cpu_gpio->gpio);
}

int lcd_pinmux_set(const char *str)
{
	struct aml_lcd_drv_s *lcd_drv = aml_lcd_get_driver();
	struct pinctrl *pin;
	struct pinctrl_state *s;
	int ret;

	/* get pinmux control */
	pin = lcd_drv->lcd_config->pin;
	if (IS_ERR(pin)) {
		LCDERR("set pinmux error 1\n");
		return -1;
	}

	/* select pinmux */
	s = pinctrl_lookup_state(pin, str);
	if (IS_ERR(s)) {
		LCDERR("set pinmux error 2\n");
		devm_pinctrl_put(pin); /* pinctrl_put(pin); //release pins */
		return -1;
	}

	/* set pinmux and lock pins */
	ret = pinctrl_select_state(pin, s);
	if (ret < 0) {
		LCDERR("set pinmux error 3\n");
		devm_pinctrl_put(pin);
		return -1;
	}
	return 0;
}

void vpp_set_matrix_ycbcr2rgb(int vd1_or_vd2_or_post, int mode)
{
	if (vd1_or_vd2_or_post == 0) { /* vd1 */
		lcd_vcbus_setb(VPP_MATRIX_CTRL, 1, 5, 1);
		lcd_vcbus_setb(VPP_MATRIX_CTRL, 1, 8, 2);
	} else if (vd1_or_vd2_or_post == 1) { /* vd2 */
		lcd_vcbus_setb(VPP_MATRIX_CTRL, 1, 4, 1);
		lcd_vcbus_setb(VPP_MATRIX_CTRL, 2, 8, 2);
	} else {
		lcd_vcbus_setb(VPP_MATRIX_CTRL, 1, 0, 1);
		lcd_vcbus_setb(VPP_MATRIX_CTRL, 0, 8, 2);
		if (mode == 0)
			lcd_vcbus_setb(VPP_MATRIX_CTRL, 1, 1, 2);
		else if (mode == 1)
			lcd_vcbus_setb(VPP_MATRIX_CTRL, 0, 1, 2);
	}

	if (mode == 0) { /* ycbcr not full range, 601 conversion */
		lcd_vcbus_write(VPP_MATRIX_PRE_OFFSET0_1, 0x0064C8FF);
		lcd_vcbus_write(VPP_MATRIX_PRE_OFFSET2, 0x006400C8);
		/* 1.164     0       1.596
		// 1.164   -0.392    -0.813
		// 1.164   2.017     0 */
		lcd_vcbus_write(VPP_MATRIX_COEF00_01, 0x04A80000);
		lcd_vcbus_write(VPP_MATRIX_COEF02_10, 0x066204A8);
		lcd_vcbus_write(VPP_MATRIX_COEF11_12, 0x1e701cbf);
		lcd_vcbus_write(VPP_MATRIX_COEF20_21, 0x04A80812);
		lcd_vcbus_write(VPP_MATRIX_COEF22, 0x00000000);
		lcd_vcbus_write(VPP_MATRIX_OFFSET0_1, 0x00000000);
		lcd_vcbus_write(VPP_MATRIX_OFFSET2, 0x00000000);
		lcd_vcbus_write(VPP_MATRIX_PRE_OFFSET0_1, 0x0FC00E00);
		lcd_vcbus_write(VPP_MATRIX_PRE_OFFSET2, 0x00000E00);
	} else if (mode == 1) {/* ycbcr full range, 601 conversion */
		lcd_vcbus_write(VPP_MATRIX_PRE_OFFSET0_1, 0x0000E00);
		lcd_vcbus_write(VPP_MATRIX_PRE_OFFSET2, 0x0E00);
		/* 1    0           1.402
		// 1    -0.34414    -0.71414
		// 1    1.772       0 */
		lcd_vcbus_write(VPP_MATRIX_COEF00_01, (0x400 << 16) | 0);
		lcd_vcbus_write(VPP_MATRIX_COEF02_10, (0x59c << 16) | 0x400);
		lcd_vcbus_write(VPP_MATRIX_COEF11_12, (0x1ea0 << 16) | 0x1d24);
		lcd_vcbus_write(VPP_MATRIX_COEF20_21, (0x400 << 16) | 0x718);
		lcd_vcbus_write(VPP_MATRIX_COEF22, 0x0);
		lcd_vcbus_write(VPP_MATRIX_OFFSET0_1, 0x0);
		lcd_vcbus_write(VPP_MATRIX_OFFSET2, 0x0);
	}
}

void lcd_tcon_config(struct lcd_config_s *pconf)
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

void lcd_clk_gate_switch(int status)
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



