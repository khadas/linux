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
#include <linux/amlogic/vout/lcd_unifykey.h>
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

unsigned char lcd_mode_str_to_mode(const char *str)
{
	unsigned char mode;

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

const char *lcd_ttl_pinmux_str[] = {
	"ttl_6bit_hvsync_on",      /* 0 */
	"ttl_6bit_de_on",          /* 1 */
	"ttl_6bit_hvsync_de_on",   /* 2 */
	"ttl_6bit_hvsync_de_off",  /* 3 */
	"ttl_8bit_hvsync_on",      /* 4 */
	"ttl_8bit_de_on",          /* 5 */
	"ttl_8bit_hvsync_de_on",   /* 6 */
	"ttl_8bit_hvsync_de_off",  /* 7 */
};

void lcd_ttl_pinmux_set(int status)
{
	struct aml_lcd_drv_s *lcd_drv = aml_lcd_get_driver();
	struct lcd_config_s *pconf;
	unsigned int index, num;

	if (lcd_debug_print_flag)
		LCDPR("%s: %d\n", __func__, status);

	pconf = lcd_drv->lcd_config;
	if (pconf->lcd_basic.lcd_bits == 6)
		index = 0;
	else
		index = 4;

	if (status) {
		switch (pconf->lcd_control.ttl_config->sync_valid) {
		case 1: /* hvsync */
			num = index + 0;
			break;
		case 2: /* DE */
			num = index + 1;
			break;
		default:
		case 3: /* DE + hvsync */
			num = index + 2;
			break;
		}
	} else {
		num = index + 3;
	}

	/* request pinmux */
	pconf->pin = devm_pinctrl_get_select(lcd_drv->dev,
		lcd_ttl_pinmux_str[num]);
	if (IS_ERR(pconf->pin))
		LCDERR("set ttl pinmux error\n");
}

int lcd_power_load_from_dts(struct lcd_config_s *pconf,
		struct device_node *child)
{
	int ret = 0;
	unsigned int para[5];
	unsigned int val;
	struct lcd_power_ctrl_s *lcd_power = pconf->lcd_power;
	int i, j;
	unsigned int index;

	if (child == NULL) {
		LCDPR("error: failed to get %s\n", pconf->lcd_propname);
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
				LCDPR("power_off %d type: %d\n", i,
					lcd_power->power_off_step[i].type);
				LCDPR("power_off %d index: %d\n", i,
					lcd_power->power_off_step[i].index);
				LCDPR("power_off %d value: %d\n", i,
					lcd_power->power_off_step[i].value);
				LCDPR("power_off %d delay: %d\n", i,
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

int lcd_power_load_from_unifykey(struct lcd_config_s *pconf,
		unsigned char *buf, int key_len)
{
	int i, len;
	unsigned char *p;
	unsigned int index;
	int ret;

	len = 10 + 36 + 18 + 31 + 20;
	/* power: (5byte * n) */
	p = buf + len;
	i = 0;
	if (lcd_debug_print_flag)
		LCDPR("power_on step:\n");
	while (i < LCD_PWR_STEP_MAX) {
		len += 5;
		ret = lcd_unifykey_len_check(key_len, len);
		if (ret < 0) {
			pconf->lcd_power->power_on_step[i].type = 0xff;
			pconf->lcd_power->power_on_step[i].index = 0;
			pconf->lcd_power->power_on_step[i].value = 0;
			pconf->lcd_power->power_on_step[i].delay = 0;
			return -1;
		}
		pconf->lcd_power->power_on_step[i].type = *p;
		p += LCD_UKEY_PWR_TYPE;
		pconf->lcd_power->power_on_step[i].index = *p;
		p += LCD_UKEY_PWR_INDEX;
		pconf->lcd_power->power_on_step[i].value = *p;
		p += LCD_UKEY_PWR_VAL;
		pconf->lcd_power->power_on_step[i].delay =
				(*p | ((*(p + 1)) << 8));
		p += LCD_UKEY_PWR_DELAY;

		/* gpio request */
		switch (pconf->lcd_power->power_on_step[i].type) {
		case LCD_POWER_TYPE_CPU:
			index = pconf->lcd_power->power_on_step[i].index;
			if (index < LCD_CPU_GPIO_NUM_MAX)
				lcd_cpu_gpio_register(index);
			break;
		default:
			break;
		}
		if (lcd_debug_print_flag) {
			LCDPR("%d: type=%d, index=%d, value=%d, delay=%d\n",
				i, pconf->lcd_power->power_on_step[i].type,
				pconf->lcd_power->power_on_step[i].index,
				pconf->lcd_power->power_on_step[i].value,
				pconf->lcd_power->power_on_step[i].delay);
		}
		if (pconf->lcd_power->power_on_step[i].type >=
			LCD_POWER_TYPE_MAX) {
			break;
		} else {
			i++;
		}
	}
	i = 0;
	if (lcd_debug_print_flag)
		LCDPR("power_off step:\n");
	while (i < LCD_PWR_STEP_MAX) {
		len += 5;
		ret = lcd_unifykey_len_check(key_len, len);
		if (ret < 0) {
			pconf->lcd_power->power_off_step[i].type = 0xff;
			pconf->lcd_power->power_off_step[i].index = 0;
			pconf->lcd_power->power_off_step[i].value = 0;
			pconf->lcd_power->power_off_step[i].delay = 0;
			return -1;
		}
		pconf->lcd_power->power_off_step[i].type = *p;
		p += LCD_UKEY_PWR_TYPE;
		pconf->lcd_power->power_off_step[i].index = *p;
		p += LCD_UKEY_PWR_INDEX;
		pconf->lcd_power->power_off_step[i].value = *p;
		p += LCD_UKEY_PWR_VAL;
		pconf->lcd_power->power_off_step[i].delay =
				(*p | ((*(p + 1)) << 8));
		p += LCD_UKEY_PWR_DELAY;

		/* gpio request */
		switch (pconf->lcd_power->power_off_step[i].type) {
		case LCD_POWER_TYPE_CPU:
			index = pconf->lcd_power->power_off_step[i].index;
			if (index < LCD_CPU_GPIO_NUM_MAX)
				lcd_cpu_gpio_register(index);
			break;
		default:
			break;
		}
		if (lcd_debug_print_flag) {
			LCDPR("%d: type=%d, index=%d, value=%d, delay=%d\n",
				i, pconf->lcd_power->power_off_step[i].type,
				pconf->lcd_power->power_off_step[i].index,
				pconf->lcd_power->power_off_step[i].value,
				pconf->lcd_power->power_off_step[i].delay);
		}
		if (pconf->lcd_power->power_off_step[i].type >=
			LCD_POWER_TYPE_MAX) {
			break;
		} else {
			i++;
		}
	}

	return 0;
}

void lcd_tcon_config(struct lcd_config_s *pconf)
{
	unsigned short h_period, v_period, h_active, v_active;
	unsigned short hsync_bp, hsync_width, vsync_bp, vsync_width;
	unsigned short de_hstart, de_vstart;
	unsigned short hstart, hend, vstart, vend;
	unsigned short h_delay;

	switch (pconf->lcd_basic.lcd_type) {
	case LCD_TTL:
		h_delay = TTL_DELAY;
		break;
	default:
		h_delay = 0;
		break;
	}
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

	pconf->lcd_timing.video_on_pixel = de_hstart - h_delay;
	pconf->lcd_timing.video_on_line = de_vstart;

	pconf->lcd_timing.de_hs_addr = de_hstart;
	pconf->lcd_timing.de_he_addr = de_hstart + h_active;
	pconf->lcd_timing.de_vs_addr = de_vstart;
	pconf->lcd_timing.de_ve_addr = de_vstart + v_active - 1;

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

/* change frame_rate for different vmode */
int lcd_vmode_change(struct lcd_config_s *pconf)
{
	unsigned int pclk = pconf->lcd_timing.lcd_clk_dft; /* avoid offset */
	unsigned char type = pconf->lcd_timing.fr_adjust_type;
	unsigned int h_period = pconf->lcd_basic.h_period;
	unsigned int v_period = pconf->lcd_basic.v_period;
	unsigned int sync_duration_num = pconf->lcd_timing.sync_duration_num;
	unsigned int sync_duration_den = pconf->lcd_timing.sync_duration_den;

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
			/* check clk frac update */
			pclk = (h_period * v_period) / sync_duration_den *
				sync_duration_num;
			if (pconf->lcd_timing.lcd_clk != pclk)
				pconf->lcd_timing.lcd_clk = pclk;
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
			/* check clk frac update */
			pclk = (h_period * v_period) / sync_duration_den *
				sync_duration_num;
			if (pconf->lcd_timing.lcd_clk != pclk)
				pconf->lcd_timing.lcd_clk = pclk;
		}
		break;
	case 0: /* pixel clk adjust */
	default:
		pclk = (h_period * v_period) / sync_duration_den *
			sync_duration_num;
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

void lcd_clk_gate_switch(int status)
{
	struct aml_lcd_drv_s *lcd_drv = aml_lcd_get_driver();
	struct lcd_config_s *pconf;

	if (lcd_debug_print_flag)
		LCDPR("%s\n", __func__);
	pconf = lcd_drv->lcd_config;
	if (status) {
		if (pconf->rstc.encl)
			reset_control_deassert(pconf->rstc.encl);
		if (pconf->rstc.vencl)
			reset_control_deassert(pconf->rstc.vencl);
	} else {
		if (pconf->rstc.encl)
			reset_control_assert(pconf->rstc.encl);
		if (pconf->rstc.vencl)
			reset_control_assert(pconf->rstc.vencl);
	}
}

