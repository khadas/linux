/*
 * drivers/amlogic/media/vout/spi/lcd_spi_api.c
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
#include <linux/of_address.h>
#include <linux/delay.h>
#include <linux/amlogic/media/vout/lcd/lcd_spi.h>
#include <linux/gpio/consumer.h>
#include "lcd_spi_api.h"

struct spi_dcx_gpio_s dcx_gpio[] = {
	{.probe_flag = 0, .register_flag = 0,},
};

struct spi_rst_gpio_s rst_gpio[] = {
	{.probe_flag = 0, .register_flag = 0,},
};

struct spi_irq_gpio_s irq_gpio[] = {
	{.probe_flag = 0, .register_flag = 0,},
};

void spi_dcx_gpio_set(int value)
{
	struct lcd_spi_driver_s *lcd_spi = lcd_spi_get_driver();

	if (dcx_gpio->register_flag == 0) {
		if (lcd_spi->device->dev == NULL) {
			LCDERR("get spi deriver failed\n");
			return;
		}

		/* request gpio */
		dcx_gpio->gpio = gpiod_get(lcd_spi->device->dev,
			"dcx", value);

		if (IS_ERR(dcx_gpio->gpio)) {
		LCDERR("dcx register gpio: %p, err: %d\n", dcx_gpio->gpio,
				IS_ERR(dcx_gpio->gpio));
			return;
		}

		dcx_gpio->register_flag = 1;
		LCDPR("%s: register ok\n", __func__);
	}

	if (IS_ERR_OR_NULL(dcx_gpio->gpio)) {
		LCDERR("dcx gpio: %p, err: %ld\n", dcx_gpio->gpio,
			PTR_ERR(dcx_gpio->gpio));
		return;
	}

	switch (value) {
	case LCD_GPIO_OUTPUT_LOW:
	case LCD_GPIO_OUTPUT_HIGH:
		gpiod_direction_output(dcx_gpio->gpio, value);
		break;
	case LCD_GPIO_INPUT:
	default:
		gpiod_direction_input(dcx_gpio->gpio);
		break;
	}
}

void spi_rst_gpio_set(int value)
{
	struct lcd_spi_driver_s *lcd_spi = lcd_spi_get_driver();

	if (rst_gpio->register_flag == 0) {
		if (lcd_spi->device->dev == NULL) {
			LCDERR("get spi driver failed");
			return;
		}

		rst_gpio->gpio = devm_gpiod_get(lcd_spi->device->dev,
			"rst", value);
		if (IS_ERR(rst_gpio->gpio)) {
			LCDERR("rst register gpio: %p, err: %d\n",
			rst_gpio->gpio, IS_ERR(rst_gpio->gpio));
			return;
		}
		rst_gpio->register_flag = 1;
		LCDPR("%s: register ok\n", __func__);
	}

	if (IS_ERR_OR_NULL(rst_gpio->gpio)) {
		LCDERR("reset gpio: %p, err: %ld\n", rst_gpio->gpio,
		PTR_ERR(rst_gpio->gpio));
		return;
	}

	switch (value) {
	case LCD_GPIO_OUTPUT_LOW:
	case LCD_GPIO_OUTPUT_HIGH:
		gpiod_direction_output(rst_gpio->gpio, value);
		break;
	case LCD_GPIO_INPUT:
	default:
		gpiod_direction_input(rst_gpio->gpio);
		break;
	}
}

void spi_irq_gpio_set(unsigned int value)
{
	struct lcd_spi_driver_s *lcd_spi = lcd_spi_get_driver();

	if (irq_gpio->register_flag == 0) {
		if (lcd_spi->device->dev == NULL) {
			LCDERR("get spi driver failed");
			return;
		}

		irq_gpio->gpio = devm_gpiod_get(lcd_spi->device->dev,
			"irq", value);
		if (IS_ERR(irq_gpio->gpio)) {
			LCDERR("irq register gpio: %p, err: %d\n",
			irq_gpio->gpio, IS_ERR(irq_gpio->gpio));
			return;
		}
		irq_gpio->register_flag = 1;
		LCDPR("%s: register gpio ok\n", __func__);
	}

	if (IS_ERR_OR_NULL(irq_gpio->gpio)) {
		LCDERR("reset gpio: %p, err: %ld\n", irq_gpio->gpio,
		PTR_ERR(irq_gpio->gpio));
		return;
	}

	switch (value) {
	case LCD_GPIO_OUTPUT_LOW:
	case LCD_GPIO_OUTPUT_HIGH:
		gpiod_direction_output(irq_gpio->gpio, value);
		break;
	case LCD_GPIO_INPUT:
	default:
		gpiod_direction_input(irq_gpio->gpio);
		break;
	}
}

void dcx_init(void)
{
	spi_dcx_gpio_set(0);
}

void dcx_high(void)
{
	spi_dcx_gpio_set(1);
}

void dcx_low(void)
{
	spi_dcx_gpio_set(0);
}

void rst_high(void)
{
	spi_rst_gpio_set(2);
}

void rst_low(void)
{
	spi_rst_gpio_set(0);
}

static irqreturn_t lcd_spi_vsync_isr(int irq, void *dev_id)
{
	struct lcd_spi_driver_s *lcd_spi = lcd_spi_get_driver();

	if (lcd_spi->vsync_isr_cb != NULL)
		lcd_spi->vsync_isr_cb();

	return IRQ_HANDLED;
}

static int lcd_spi_vsync_irq_init(void)
{
	struct lcd_spi_driver_s *lcd_spi = lcd_spi_get_driver();
	int irq_pin, ret;

	spi_irq_gpio_set(2);
	irq_pin = desc_to_gpio(irq_gpio->gpio);
	lcd_spi->irq_num = gpio_to_irq(irq_pin);
	if (!lcd_spi->irq_num) {
		LCDERR("gpio to irq failed\n");
		return -1;
	}

	ret = request_irq(lcd_spi->irq_num, lcd_spi_vsync_isr,
			  IRQF_TRIGGER_RISING, "lcd_spi_vsync_irq",
			  (void *)"lcd_spi");
	if (ret) {
		LCDERR("can't request lcd_spi_vsync_irq\n");
	} else {
		if (lcd_debug_print_flag)
			LCDPR("request lcd_spi_vsync_irq successful\n");
	}
	return 0;
}

static unsigned long long reverse_bytes(unsigned long long value)
{
	return (value & 0x00000000000000FFUL) << 48 |
	       (value & 0x000000000000FF00UL) << 48 |
	       (value & 0x0000000000FF0000UL) << 16 |
	       (value & 0x00000000FF000000UL) << 16 |
	       (value & 0x000000FF00000000UL) >> 16 |
	       (value & 0x0000FF0000000000UL) >> 16 |
	       (value & 0x00FF000000000000UL) >> 48 |
	       (value & 0xFF00000000000000UL) >> 48;
}

static int endian64bit_convert(void *src_data, int size)
{
	unsigned char *src = (unsigned char *)src_data;

	if (size % 8) {
		LCDPR("endian64bit_convert size error %d\n", size);
		return -1;
	}
	while (size > 0) {
		*(unsigned long long *)src =
			reverse_bytes(*(unsigned long long *)src);
		src += 8;
		size -= 8;
	}
	return 0;
}

int lcd_spi_write(struct spi_device *spi, unsigned char *tbuf, int tlen,
		  int word_bits)
{
	struct spi_transfer xfer;
	struct spi_message msg;
	int ret;

	//LCDPR("%s\n", __func__);
	spi_message_init(&msg);
	memset(&xfer, 0, sizeof(xfer));
	xfer.tx_buf = (void *)tbuf;
	xfer.rx_buf = NULL;
	xfer.len = tlen;
	xfer.bits_per_word = word_bits;
	spi_message_add_tail(&xfer, &msg);
	ret = spi_sync(spi, &msg);

	return ret;
}

static void WriteComm(struct spi_device *spi, unsigned char command)
{
	dcx_low();
	udelay(1);
	lcd_spi_write(spi, &command, 1, 8);
}

static void WriteData(struct spi_device *spi, unsigned char data)
{
	dcx_high();
	udelay(1);
	lcd_spi_write(spi, &data, 1, 8);
}

void lcd_spi_block_write(struct spi_device *spi,
		unsigned int Xstart, unsigned int Xend,
		unsigned int Ystart, unsigned int Yend)
{
	WriteComm(spi, 0x2A);
	WriteData(spi, Xstart>>8);
	WriteData(spi, Xstart&0xff);
	WriteData(spi, Xend>>8);
	WriteData(spi, Xend&0xff);

	WriteComm(spi, 0x2B);
	WriteData(spi, Ystart>>8);
	WriteData(spi, Ystart&0xff);
	WriteData(spi, Yend>>8);
	WriteData(spi, Yend&0xff);

	WriteComm(spi, 0x2c);
}

void lcd_spi_vsync(struct spi_device *spi, unsigned int flag)
{
	if (flag == 1) {
		/* open TE signal */
		LCDPR("open TE signal\n");
		switch (lcd_debug_print_flag) {
		case 0:
			LCDPR("open TE signal by spi\n");
			WriteComm(spi, 0xb7);
			WriteData(spi, 0x35);
			break;
		case 1:
			LCDPR("open TE signal by gpio\n");
			spi_irq_gpio_set(1);
			break;
		default:
			LCDPR("open TE signal by spi and gpio\n");
			WriteComm(spi, 0xb7);
			WriteData(spi, 0x35);
			spi_irq_gpio_set(1);
			break;
		}
	} else {
		/* close TE signal */
		switch (lcd_debug_print_flag) {
		case 0:
			LCDPR("close TE signal by spi\n");
			WriteComm(spi, 0x36);
			break;
		case 1:
			LCDPR("close TE signal by gpio\n");
			spi_irq_gpio_set(0);
			break;
		default:
			LCDPR("close TE signal by spi and gpio\n");
			WriteComm(spi, 0x36);
			spi_irq_gpio_set(0);
			break;
		}
	}
}

void lcd_spi_panel_init(struct spi_device *spi)
{
//	cs_high();
//	cs_init();
	dcx_high();
	dcx_init();
	usleep_range(100, 101);

	rst_high();
	msleep(200);

	rst_low();
	msleep(800);

	rst_high();
	msleep(800);

	WriteComm(spi, 0x11);//turn off sleep mode
	msleep(120);
	/* ST7789S Frame rate setting */
	WriteComm(spi, 0xb2); //Porch Setting
	WriteData(spi, 0x0c);
	WriteData(spi, 0x0c);
	WriteData(spi, 0x00);
	WriteData(spi, 0x33);
	WriteData(spi, 0x33);
	WriteComm(spi, 0xb7); //Gate Control
	WriteData(spi, 0x35);
	/* ST7789S Power setting */
	WriteComm(spi, 0xbb); //VCOM Setting
	WriteData(spi, 0x28);
	WriteComm(spi, 0xc3); //VRH Set
	WriteData(spi, 0x25);
	WriteComm(spi, 0Xc4); //VDV Set
	WriteData(spi, 0x20);
	WriteComm(spi, 0xc6);//Frame Rate Control in Normal Mode
	WriteData(spi, 0x0f);//60hz
	WriteComm(spi, 0xd0); //Power Control 1
	WriteData(spi, 0xa4);
	WriteData(spi, 0xa2);
	/* ST7789S gamma setting */
	WriteComm(spi, 0xe0); //Positive Voltage Gamma Control
	WriteData(spi, 0xd0);
	WriteData(spi, 0x03);
	WriteData(spi, 0x08);
	WriteData(spi, 0x0b);
	WriteData(spi, 0x0f);
	WriteData(spi, 0x2c);
	WriteData(spi, 0x41);
	WriteData(spi, 0x54);
	WriteData(spi, 0x4e);
	WriteData(spi, 0x07);
	WriteData(spi, 0x0e);
	WriteData(spi, 0x0c);
	WriteData(spi, 0x1e);
	WriteData(spi, 0x23);
	WriteComm(spi, 0xe1); //Negative Voltage Gamma Control
	WriteData(spi, 0xd0);
	WriteData(spi, 0x03);
	WriteData(spi, 0x09);
	WriteData(spi, 0x0b);
	WriteData(spi, 0x0d);
	WriteData(spi, 0x19);
	WriteData(spi, 0x3c);
	WriteData(spi, 0x54);
	WriteData(spi, 0x4f);
	WriteData(spi, 0x0e);
	WriteData(spi, 0x1d);
	WriteData(spi, 0x1c);
	WriteData(spi, 0x20);
	WriteData(spi, 0x22);

	WriteComm(spi, 0x35);
	WriteData(spi, 0x0);

	//WriteComm(spi, 0x36);
	/* 00-vertical, 0x20-horizontal */
	//WriteData(spi, 0x00);

	WriteComm(spi, 0x3A); //Interface Pixel Format
	WriteData(spi, 0x55); //16bit/pixel 65k RGB

	WriteComm(spi, 0x29); // Display On
	WriteComm(spi, 0x2C); //Memory Write


	LCDPR("%s ok\n", __func__);
}

int lcd_spi_read(struct spi_device *spi, unsigned char *tbuf, int tlen,
		unsigned char *rbuf, int rlen)
{
	struct spi_transfer xfer[2];
	struct spi_message msg;
	int ret;

	spi_message_init(&msg);
	memset(&xfer, 0, sizeof(xfer));
	xfer[0].tx_buf = (void *)tbuf;
	xfer[0].rx_buf = NULL;
	xfer[0].len = tlen;
	spi_message_add_tail(&xfer[0], &msg);
	xfer[1].tx_buf = NULL;
	xfer[1].rx_buf = (void *)rbuf;
	xfer[1].len = rlen;
	spi_message_add_tail(&xfer[1], &msg);
	ret = spi_sync(spi, &msg);

	return ret;
}

int frame_post(unsigned char *addr, unsigned int col, unsigned int row)
{
	struct lcd_spi_driver_s *lcd_spi = lcd_spi_get_driver();
	int ret, size;

	if (addr == NULL) {
		LCDERR("buff is null\n");
		return -1;
	}

	if ((!col) || (!row)) {
		LCDERR("col or row size is null\n");
		return -1;
	}

	lcd_spi_block_write(lcd_spi->device->spi_dev, 0, (col - 1),
			    0, (row - 1));

	dcx_high();
	mdelay(1);
	size = col * row * 2;
	endian64bit_convert(addr, size);
	ret = lcd_spi_write(lcd_spi->device->spi_dev, addr, size, 64);
	if (ret) {
		LCDERR("%s: fail\n", __func__);
		return -1;
	}

	return 0;
}

int frame_flush(void)
{
	return 0;
}

int set_color_format(unsigned int cfmt)
{
	return 0;
}

int set_gamma(unsigned char *table, unsigned int rgb_sel)
{
	return 0;
}

int set_flush_rate(unsigned int rate)
{
	return 0;
}

static struct lcd_spi_device_s lcd_device = {
	.dev = NULL,
	.spi_dev = NULL,
	.spi_info = NULL,
};

static struct lcd_spi_driver_s lcd_spi = {
	.device = &lcd_device,
	.frame_post = frame_post,
	.frame_flush = frame_flush,
	.set_color_format = set_color_format,
	.set_gamma = set_gamma,
	.set_flush_rate = set_flush_rate,
	.vsync_isr_cb = te_cb,
};

struct lcd_spi_driver_s *lcd_spi_get_driver(void)
{
	return &lcd_spi;
}

static int lcd_spi_dev_probe(struct spi_device *spi)
{
	int ret;

	lcd_spi.device->spi_dev = spi;

	spi->bits_per_word = 8;
	ret = spi_setup(spi);
	if (ret) {
		LCDERR("lcd_spi set up fail\n");
		return -1;
	}

	lcd_spi_vsync_irq_init();
	return 0;
}

static int lcd_spi_dev_remove(struct spi_device *spi)
{
	lcd_spi.device->spi_dev = NULL;
	LCDPR("%s ok\n", __func__);

	return 0;
}

static struct spi_driver lcd_spi_dev_driver = {
	.probe = lcd_spi_dev_probe,
	.remove = lcd_spi_dev_remove,
	.driver = {
		.name = "lcd_spi",
		.owner = THIS_MODULE,
	},
};

int lcd_spi_add_driver(struct lcd_spi_driver_s *lcd_spi)
{
	int ret;


	LCDPR("%s\n", __func__);

	if (lcd_spi->device->spi_info == NULL)
		return -1;

	spi_register_board_info(lcd_spi->device->spi_info, 1);
	ret = spi_register_driver(&lcd_spi_dev_driver);
	if (ret) {
		LCDERR("%s failed\n", __func__);
		return -1;
	}

	if (lcd_spi->device->dev == NULL) {
		LCDERR("%s failed\n", __func__);
		return -1;
	}

	LCDPR("%s ok\n", __func__);
	return 0;
}

int lcd_spi_driver_remove(struct lcd_spi_driver_s *lcd_spi)
{
	free_irq(lcd_spi->irq_num, (void *)"lcd_spi");
	if (lcd_spi->device->dev != NULL)
		spi_unregister_driver(&lcd_spi_dev_driver);

	LCDPR("%s ok\n", __func__);
	return 0;
}
