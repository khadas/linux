/*
 * include/linux/amlogic/vout/lcd_extern.h
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

#ifndef _INC_AML_LCD_EXTERN_H_
#define _INC_AML_LCD_EXTERN_H_

enum lcd_extern_type_e {
	LCD_EXTERN_I2C = 0,
	LCD_EXTERN_SPI,
	LCD_EXTERN_MIPI,
	LCD_EXTERN_MAX,
};

#define LCD_EXTERN_GPIO_NUM_MAX    5
#define LCD_EXTERN_INDEX_INVALID   0xff
#define LCD_EXTERN_NAME_LEN_MAX    30
struct lcd_extern_config_s {
	int index;
	char name[LCD_EXTERN_NAME_LEN_MAX];
	enum lcd_extern_type_e type;
	int status;
	int i2c_addr;
	int i2c_bus;
	struct gpio_desc *spi_cs;
	struct gpio_desc *spi_clk;
	struct gpio_desc *spi_data;
};

/* global API */
#define LCD_EXT_DRIVER_MAX    10
struct aml_lcd_extern_driver_s {
	struct lcd_extern_config_s config;
	int (*reg_read)(unsigned char reg, unsigned char *buf);
	int (*reg_write)(unsigned char reg, unsigned char value);
	int (*power_on)(void);
	int (*power_off)(void);
	unsigned char *init_on_cmd_8;
	unsigned char *init_off_cmd_8;
};

extern struct aml_lcd_extern_driver_s *aml_lcd_extern_get_driver(int index);

#endif

