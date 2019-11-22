/*
 * include/linux/amlogic/media/vout/lcd/lcd_spi.h
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
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

#ifndef _INC_AML_LCD_SPI_H_
#define _INC_AML_LCD_SPI_H_
#include <linux/types.h>
#include <linux/platform_device.h>
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/pinctrl/consumer.h>
#include <linux/amlogic/media/vout/lcd/aml_bl.h>
#include <linux/amlogic/media/vout/lcd/aml_lcd.h>
#include <linux/amlogic/media/vout/lcd/ldim_alg.h>
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include <linux/spi/spi.h>

#define LCD_SPI_GPIO_NUM_MAX       2

struct spi_dcx_gpio_s {
	char name[15];
	struct gpio_desc *gpio;
	int probe_flag;
	int register_flag;
};

struct spi_rst_gpio_s {
	char name[15];
	struct gpio_desc *gpio;
	int probe_flag;
	int register_flag;
};

struct spi_irq_gpio_s {
	char name[15];
	struct gpio_desc *gpio;
	int probe_flag;
	int register_flag;
};

struct driver_info_s {
	char model_name[30];
	unsigned int type;
	unsigned int hsize;
	unsigned int vsize;
	unsigned int state;
	unsigned int cfmt;
	unsigned int rate;
	unsigned char *gamma_r;
	unsigned char *gamma_g;
	unsigned char *gamma_b;
};


struct lcd_spi_device_s {
	struct device *dev;
	struct spi_device *spi_dev;
	struct spi_board_info *spi_info;
	struct spi_dcx_gpio_s spi_dcx_gpio;
	struct spi_rst_gpio_s spi_rst_gpio;
	struct spi_irq_gpio_s spi_irq_gpio;
};

struct lcd_spi_driver_s {
	struct lcd_spi_device_s *device;
	struct driver_info_s *dri_info;

	int (*frame_post)(unsigned char *addr, unsigned int col,
			  unsigned int row);
	int (*frame_flush)(void);
	int (*set_color_format)(unsigned int cfmt);
	int (*set_gamma)(unsigned char *table, unsigned int rgb_sel);
	int (*set_flush_rate)(unsigned int rate);
	int (*vsync_isr_cb)(void);
	int irq_num;
};

extern struct lcd_spi_driver_s *lcd_spi_get_driver(void);
#endif
