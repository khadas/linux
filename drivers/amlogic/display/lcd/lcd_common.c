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
#include <linux/amlogic/vout/lcd_vout.h>
#include "lcd_common.h"
#include "lcd_reg.h"

/* **********************************
 * lcd type
 * ********************************** */
static char *lcd_type_table[] = {
	"ttl",
	"lvds",
	"vbyone",
	"mipi",
	"edp",
	"invalid",
};

int lcd_type_str_to_type(const char *str)
{
	int type;

	for (type = 0; type < LCD_TYPE_MAX; type++) {
		if (!strcmp(str, lcd_type_table[type]))
			break;
	}
	return type;
}

char *lcd_type_type_to_str(int type)
{
	return lcd_type_table[type];
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
		LCDPR("gpio index %d, exit\n", index);
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
		LCDPR("failed to get lcd_cpu_gpio_names: %d\n", index);
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
		LCDPR("gpio index %d, exit\n", index);
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

/* **********************************
 * lcd clk
 * ********************************** */
void lcd_clocks_set_vid_clk_div(int div_sel)
{
	int shift_val = 0;
	int shift_sel = 0;

	/* Disable the output clock */
	lcd_hiu_setb(HHI_VID_PLL_CLK_DIV, 0, 19, 1);
	lcd_hiu_setb(HHI_VID_PLL_CLK_DIV, 0, 15, 1);

	switch (div_sel) {
	case CLK_DIV_SEL_1:
		shift_val = 0xFFFF;
		shift_sel = 0;
		break;
	case CLK_DIV_SEL_2:
		shift_val = 0x0aaa;
		shift_sel = 0;
		break;
	case CLK_DIV_SEL_3:
		shift_val = 0x0db6;
		shift_sel = 0;
		break;
	case CLK_DIV_SEL_3p5:
		shift_val = 0x36cc;
		shift_sel = 1;
		break;
	case CLK_DIV_SEL_3p75:
		shift_val = 0x6666;
		shift_sel = 2;
		break;
	case CLK_DIV_SEL_4:
		shift_val = 0x0ccc;
		shift_sel = 0;
		break;
	case CLK_DIV_SEL_5:
		shift_val = 0x739c;
		shift_sel = 2;
		break;
	case CLK_DIV_SEL_6:
		shift_val = 0x0e38;
		shift_sel = 0;
		break;
	case CLK_DIV_SEL_6p25:
		shift_val = 0x0000;
		shift_sel = 3;
		break;
	case CLK_DIV_SEL_7:
		shift_val = 0x3c78;
		shift_sel = 1;
		break;
	case CLK_DIV_SEL_7p5:
		shift_val = 0x78f0;
		shift_sel = 2;
		break;
	case CLK_DIV_SEL_12:
		shift_val = 0x0fc0;
		shift_sel = 0;
		break;
	case CLK_DIV_SEL_14:
		shift_val = 0x3f80;
		shift_sel = 1;
		break;
	case CLK_DIV_SEL_15:
		shift_val = 0x7f80;
		shift_sel = 2;
		break;
	case CLK_DIV_SEL_2p5:
		shift_val = 0x5294;
		shift_sel = 2;
		break;
	default:
		LCDPR("error: clocks_set_vid_clk_div: Invalid parameter\n");
	break;
	}

	if (shift_val == 0xffff) { /* if divide by 1 */
		lcd_hiu_setb(HHI_VID_PLL_CLK_DIV, 1, 18, 1);
	} else {
		lcd_hiu_setb(HHI_VID_PLL_CLK_DIV, 0, 18, 1);
		lcd_hiu_setb(HHI_VID_PLL_CLK_DIV, 0, 16, 2);
		lcd_hiu_setb(HHI_VID_PLL_CLK_DIV, 0, 15, 1);
		lcd_hiu_setb(HHI_VID_PLL_CLK_DIV, 0, 0, 14);

		lcd_hiu_setb(HHI_VID_PLL_CLK_DIV, shift_sel, 16, 2);
		lcd_hiu_setb(HHI_VID_PLL_CLK_DIV, 1, 15, 1);
		lcd_hiu_setb(HHI_VID_PLL_CLK_DIV, shift_val, 0, 14);
		lcd_hiu_setb(HHI_VID_PLL_CLK_DIV, 0, 15, 1);
	}
	/* Enable the final output clock */
	lcd_hiu_setb(HHI_VID_PLL_CLK_DIV, 1, 19, 1);
}

void lcd_set_crt_video_enc(int vIdx, int inSel, int DivN)
{
	if (vIdx == 0) { /* V1 */
		/* [19] -disable clk_div0 */
		lcd_hiu_setb(HHI_VID_CLK_CNTL, 0, 19, 1);
		udelay(2);
		/* [18:16] - cntl_clk_in_sel */
		lcd_hiu_setb(HHI_VID_CLK_CNTL, inSel,   16, 3);
		/* [7:0]   - cntl_xd0 */
		lcd_hiu_setb(HHI_VID_CLK_DIV, (DivN-1), 0, 8);
		udelay(5);
		/* [19] -enable clk_div0 */
		lcd_hiu_setb(HHI_VID_CLK_CNTL, 1, 19, 1);
	} else { /* V2 */
		/* [19] -disable clk_div0 */
		lcd_hiu_setb(HHI_VIID_CLK_CNTL, 0, 19, 1);
		udelay(2);
		/* [18:16] - cntl_clk_in_sel */
		lcd_hiu_setb(HHI_VIID_CLK_CNTL, inSel,  16, 3);
		/* [7:0]   - cntl_xd0 */
		lcd_hiu_setb(HHI_VIID_CLK_DIV, (DivN - 1), 0, 8);
		udelay(5);
		/* [19] -enable clk_div0 */
		lcd_hiu_setb(HHI_VIID_CLK_CNTL, 1, 19, 1);
	}
	udelay(5);
}

void lcd_enable_crt_video_encl(int enable, int inSel)
{
	/* encl_clk_sel:hi_viid_clk_div[15:12] */
	lcd_hiu_setb(HHI_VIID_CLK_DIV, inSel, 12, 4);

	if (inSel <= 4) /* V1 */
		lcd_hiu_setb(HHI_VID_CLK_CNTL, 1, inSel, 1);
	else
		lcd_hiu_setb(HHI_VIID_CLK_CNTL, 1, (inSel - 5), 1);

	/* gclk_encl_clk:hi_vid_clk_cntl2[3] */
	lcd_hiu_setb(HHI_VID_CLK_CNTL2, enable, 3, 1);

	/* vencl_clk_en_force: vpu_misc_ctrl[0] */
	lcd_vcbus_setb(VPU_MISC_CTRL, 1, 0, 1);
}

void lcd_clk_gate_on(void)
{
	lcd_hiu_setb(HHI_GCLK_OTHER, 0xf, 22, 4);
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

