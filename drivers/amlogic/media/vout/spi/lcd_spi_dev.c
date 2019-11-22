/*
 * drivers/amlogic/media/vout/spi/lcd_spi_dev.c
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

#include <linux/spi/spi.h>
#include <linux/spi/spi_gpio.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/of_address.h>
#include <linux/delay.h>
#include <linux/amlogic/media/vout/lcd/lcd_spi.h>

#include "lcd_spi_api.h"

#define ROW  320
#define COL  240

static struct spi_board_info lcd_spi_info = {
	.modalias = "lcd_spi",
	.mode = SPI_MODE_0,
	//.max_speed_hz = 1000000, /* 1MHz */
	.max_speed_hz = 64000000, /* 64MHz */
	.bus_num = 1, /* SPI bus No. */
	.chip_select = 0, /* the device index on the spi bus */
	.controller_data = NULL,
};

static ssize_t display_color_store(struct class *class,
		struct class_attribute *attr, const char *buf, size_t count)
{
	struct lcd_spi_driver_s *lcd_spi = lcd_spi_get_driver();
	int ret = 0;
	unsigned int data32;
	unsigned int buf_size;
	unsigned char *frame_buffer;

	ret = kstrtouint(buf, 16, &data32);
	if (ret) {
		LCDERR("invalid data\n");
		return -EINVAL;
	}

	buf_size = ROW * COL * 2;
	frame_buffer = kmalloc(buf_size, GFP_KERNEL);
	if (frame_buffer == NULL)
		return count;

	for (ret = 0; ret < buf_size; ret += 2) {
		frame_buffer[ret] = data32 & 0xff;
		frame_buffer[ret+1] = data32 >> 8;
	}

	frame_post((char *)frame_buffer, COL, ROW);
	LCDPR("%s ok\n", __func__);
	kfree(frame_buffer);
	return count;
}

static ssize_t display_init_store(struct class *class,
		struct class_attribute *attr, const char *buf, size_t count)
{
	struct lcd_spi_driver_s *lcd_spi = lcd_spi_get_driver();
	int ret = 0;
	unsigned int temp;

	ret = kstrtouint(buf, 16, &temp);
	if (ret) {
		LCDERR("invalid data\n");
		return -EINVAL;
	}

	if (temp == 1) {
		if (lcd_spi->device->spi_dev == NULL) {
			LCDERR("Invalid device\n");
			return count;
		}

		lcd_spi_panel_init(lcd_spi->device->spi_dev);
		LCDPR("%s ok\n", __func__);
	}
	lcd_spi->device->spi_dev->bits_per_word = 32;
	return count;
}

static ssize_t lcd_vsync_store(struct class *class,
		struct class_attribute *attr, const char *buf, size_t count)
{
	struct lcd_spi_driver_s *lcd_spi = lcd_spi_get_driver();
	int ret = 0;
	unsigned int temp;

	ret = kstrtouint(buf, 16, &temp);
	if (ret) {
		LCDERR("invalid data\n");
		return -EINVAL;
	}

	lcd_spi_vsync(lcd_spi->device->spi_dev, temp);
	return count;
}

static struct class_attribute lcd_spi_class_attrs[] = {
	__ATTR(display_color, 0644, NULL, display_color_store),
	__ATTR(display_init, 0644, NULL, display_init_store),
	__ATTR(lcd_vsync, 0644, NULL, lcd_vsync_store),
	__ATTR_NULL
};

static struct class lcd_spi_class;

static void lcd_spi_class_create(void)
{
	int ret;

	lcd_spi_class.name = kzalloc(10, GFP_KERNEL);
	if (lcd_spi_class.name == NULL) {
		LCDERR("%s: malloc failed\n", __func__);
		return;
	}
	sprintf((char *)lcd_spi_class.name, "lcd_spi");
	lcd_spi_class.class_attrs = lcd_spi_class_attrs;
	ret = class_register(&lcd_spi_class);
	if (ret < 0)
		LCDERR("register lcd_spi_class failed\n");
}

static int lcd_spi_probe(struct platform_device *pdev)
{
	struct lcd_spi_driver_s *lcd_spi = lcd_spi_get_driver();

	LCDPR("%s\n", __func__);
	lcd_spi->device->spi_info = &lcd_spi_info;
	lcd_spi->device->dev = &pdev->dev;
	lcd_spi_class_create();
	lcd_spi_add_driver(lcd_spi);
	return 0;
}

static int lcd_spi_remove(struct platform_device *pdev)
{
	return 0;
}


static const struct of_device_id lcd_spi_dt_match[] = {
	{.compatible = "amlogic, lcd-spi-g12a"},
	{}
};

/* driver device registration */
static struct platform_driver lcd_spi_driver = {
	.probe = lcd_spi_probe,
	.remove = lcd_spi_remove,
	.driver = {
		.name   = "lcd_spi",
		.owner  = THIS_MODULE,
		.of_match_table = lcd_spi_dt_match,
	},
};

static int __init lcd_spi_init(void)
{
	if (platform_driver_register(&lcd_spi_driver)) {
		LCDPR("%s fail\n", __func__);
		return -1;
	}
	LCDPR("%s ok\n", __func__);
	return 0;
}

static void __exit lcd_spi_exit(void)
{
	LCDPR("%s\n", __func__);
	platform_driver_unregister(&lcd_spi_driver);
}

late_initcall(lcd_spi_init);
module_exit(lcd_spi_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Shaochan liu <shaochan.liu@amlogic.com>");
MODULE_DESCRIPTION("TV panel module");

