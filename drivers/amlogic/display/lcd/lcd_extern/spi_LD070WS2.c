/*
 * drivers/amlogic/display/vout/lcd/lcd_extern/spi_LD070WS2.c
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


#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/miscdevice.h>
#include <linux/mutex.h>
#include <linux/mm.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/sysctl.h>
#include <linux/uaccess.h>
#include <linux/amlogic/vout/lcd_extern.h>
#include "lcd_extern.h"

static struct lcd_extern_config_s *ext_config;

#define LCD_EXTERN_NAME		"lcd_spi_LD070WS2"

#define SPI_DELAY		30 /* unit: us */

static unsigned char spi_init_table[][2] = {
	{0x00, 0x21}, /* reset */
	{0x00, 0xa5}, /* standby */
	{0x01, 0x30}, /* enable FRC/Dither */
	{0x02, 0x40}, /* enable normally black */
	{0x0e, 0x5f}, /* enable test mode1 */
	{0x0f, 0xa4}, /* enable test mode2 */
	{0x0d, 0x00}, /* enable SDRRS, enlarge OE width */
	{0x02, 0x43}, /* adjust charge sharing time */
	{0x0a, 0x28}, /* trigger bias reduction */
	{0x10, 0x41}, /* adopt 2 line/1 dot */
	{0xff, 50},   /* delay 50ms */
	{0x00, 0xad}, /* display on */
	{0xff, 0xff}, /* ending flag */
};

static unsigned char spi_off_table[][2] = {
	{0x00, 0xa5}, /* standby */
	{0xff, 0xff}, /* ending flag */
};

static void set_lcd_csb(unsigned int v)
{
	lcd_extern_gpio_set(ext_config->spi_cs, v);
	udelay(SPI_DELAY);
}

static void set_lcd_scl(unsigned int v)
{
	lcd_extern_gpio_set(ext_config->spi_clk, v);
	udelay(SPI_DELAY);
}

static void set_lcd_sda(unsigned int v)
{
	lcd_extern_gpio_set(ext_config->spi_data, v);
	udelay(SPI_DELAY);
}

static void spi_gpio_init(void)
{
	set_lcd_csb(1);
	set_lcd_scl(1);
	set_lcd_sda(1);
}

static void spi_gpio_off(void)
{
	set_lcd_sda(0);
	set_lcd_scl(0);
	set_lcd_csb(0);
}

static void spi_write_8(unsigned char addr, unsigned char data)
{
	int i;
	unsigned int sdata;

	sdata = (unsigned int)(addr & 0x3f);
	sdata <<= 10;
	sdata |= (data & 0xff);
	sdata &= ~(1<<9); /* write flag */

	set_lcd_csb(1);
	set_lcd_scl(1);
	set_lcd_sda(1);

	set_lcd_csb(0);
	for (i = 0; i < 16; i++) {
		set_lcd_scl(0);
		if (sdata & 0x8000)
			set_lcd_sda(1);
		else
			set_lcd_sda(0);
		sdata <<= 1;
		set_lcd_scl(1);
	}

	set_lcd_csb(1);
	set_lcd_scl(1);
	set_lcd_sda(1);
	udelay(SPI_DELAY);
}

static int lcd_extern_power_on(void)
{
	int ending_flag = 0;
	int i = 0;

	spi_gpio_init();

	while (ending_flag == 0) {
		if (spi_init_table[i][0] == 0xff) {
			if (spi_init_table[i][1] == 0xff)
				ending_flag = 1;
			else
				mdelay(spi_init_table[i][1]);
		} else {
			spi_write_8(spi_init_table[i][0], spi_init_table[i][1]);
		}
		i++;
	}
	EXTPR("%s\n", __func__);

	return 0;
}

static int lcd_extern_power_off(void)
{
	int ending_flag = 0;
	int i = 0;

	spi_gpio_init();

	while (ending_flag == 0) {
		if (spi_off_table[i][0] == 0xff) {
			if (spi_off_table[i][1] == 0xff)
				ending_flag = 1;
			else
				mdelay(spi_off_table[i][1]);
		} else {
			spi_write_8(spi_off_table[i][0], spi_off_table[i][1]);
		}
		i++;
	}

	mdelay(10);
	spi_gpio_off();
	EXTPR("%s\n", __func__);

	return 0;
}

static int lcd_extern_driver_update(struct aml_lcd_extern_driver_s *ext_drv)
{
	int ret = 0;

	if (ext_drv) {
		ext_drv->power_on  = lcd_extern_power_on;
		ext_drv->power_off = lcd_extern_power_off;
	} else {
		EXTERR("%s driver is null\n", LCD_EXTERN_NAME);
		ret = -1;
	}

	return ret;
}

int aml_lcd_extern_spi_LD070WS2_probe(struct aml_lcd_extern_driver_s *ext_drv)
{
	int ret = 0;

	ext_config = &ext_drv->config;
	ret = lcd_extern_driver_update(ext_drv);

	if (lcd_debug_print_flag)
		EXTPR("%s: %d\n", __func__, ret);
	return ret;
}
