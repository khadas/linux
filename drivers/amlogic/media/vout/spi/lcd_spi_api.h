/*
 * drivers/amlogic/media/vout/spi/lcd_spi_api.h
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

#ifndef _AML_LCD_SPI_API_H_
#define _AML_LCD_SPI_API_H_
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/spi/spi.h>
#include <linux/amlogic/sd.h>
#include <linux/amlogic/iomap.h>
#include <linux/amlogic/media/vout/lcd/lcd_spi.h>

//g12a
extern void dcx_init(void);
extern void dcx_high(void);
extern void dcx_low(void);
extern void rst_high(void);
extern void rst_low(void);
extern int lcd_spi_add_driver(struct lcd_spi_driver_s *lcd_spi);
extern void lcd_spi_panel_init(struct spi_device *spi);
extern void lcd_spi_vsync(struct spi_device *spi, unsigned int flag);
extern void lcd_spi_block_write(struct spi_device *spi,
		unsigned int Xstart, unsigned int Xend,
		unsigned int Ystart, unsigned int Yend);
extern int frame_post(unsigned char *addr, unsigned int col, unsigned int row);
extern int frame_flush(void);
extern int set_color_format(unsigned int cfmt);
extern int set_gamma(unsigned char *table, unsigned int rgb_sel);
extern int set_flush_rate(unsigned int rate);
extern int te_cb(void);
#endif
