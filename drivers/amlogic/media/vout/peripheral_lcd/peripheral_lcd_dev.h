/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __PER_LCD_DEV_DRV_H
#define __PER_LCD_DEV_DRV_H
#include <linux/amlogic/media/vout/peripheral_lcd.h>

extern unsigned int per_lcd_debug_flag;
extern unsigned long long test_t[3];

extern unsigned int rgb888_color_data[8];
extern unsigned int rgb666_color_data[8];
extern unsigned int rgb565_color_data[8];

/* peripheral lcd global api */
void per_lcd_gpio_probe(unsigned int index);
void per_lcd_gpio_set(int index, int value);
int per_lcd_gpio_get(int index);
int per_lcd_gpio_set_irq(int index);

/* peripheral lcd intel_8080 dev api */
int per_lcd_dev_8080_probe(struct peripheral_lcd_driver_s *per_lcd_drv);
int per_lcd_dev_8080_remove(struct peripheral_lcd_driver_s *per_lcd_drv);

/* peripheral lcd spi interface api */
int per_lcd_dev_spi_probe(struct peripheral_lcd_driver_s *per_lcd_drv);
int per_lcd_dev_spi_remove(struct peripheral_lcd_driver_s *per_lcd_drv);
/* peripheral lcd spi device api */
int per_lcd_spi_driver_add(struct peripheral_lcd_driver_s *per_lcd_drv);
int per_lcd_spi_driver_remove(struct peripheral_lcd_driver_s *per_lcd_drv);
int per_lcd_spi_write(struct spi_device *spi, unsigned char *tbuf, int tlen,
		      int word_bits);
int per_lcd_spi_read(struct spi_device *spi, unsigned char *tbuf,
		     int tlen, unsigned char *rbuf, int rlen);

int perl_lcd_dev_probe(struct peripheral_lcd_driver_s *per_lcd_drv);
void per_lcd_dev_remove(struct peripheral_lcd_driver_s *per_lcd_drv);

#endif /* __PER_LCD_DEV_DRV_H */
