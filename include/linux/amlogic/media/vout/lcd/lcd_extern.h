/* SPDX-License-Identifier: GPL-2.0+ */
/*
 *
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#ifndef _INC_AML_LCD_EXTERN_H_
#define _INC_AML_LCD_EXTERN_H_
#include <linux/amlogic/media/vout/lcd/aml_lcd.h>

enum lcd_extern_type_e {
	LCD_EXTERN_I2C = 0,
	LCD_EXTERN_SPI,
	LCD_EXTERN_MIPI,
	LCD_EXTERN_MAX,
};

#define LCD_EXTERN_INIT_ON_MAX        3000
#define LCD_EXTERN_INIT_OFF_MAX       100

#define LCD_EXTERN_GPIO_NUM_MAX       6
#define LCD_EXTERN_INDEX_INVALID      0xff
#define LCD_EXTERN_NAME_LEN_MAX       30
struct lcd_ext_common_s {
	unsigned char key_valid;
	unsigned char i2c_bus;
	unsigned char i2c_sck_gpio;
	unsigned char i2c_sda_gpio;
	struct pinctrl *pin;
	unsigned char pinmux_valid;
	unsigned char pinmux_gpio_off;
	unsigned int pinmux_flag;
};

struct lcd_extern_config_s {
	unsigned char index;
	char name[LCD_EXTERN_NAME_LEN_MAX];
	enum lcd_extern_type_e type;
	unsigned char status;
	unsigned char addr_sel; /* internal used */

	unsigned char i2c_addr;
	unsigned char i2c_addr2;
	unsigned char i2c_addr3;
	unsigned char i2c_addr4;

	unsigned char spi_gpio_cs;
	unsigned char spi_gpio_clk;
	unsigned char spi_gpio_data;
	unsigned char spi_clk_pol;
	unsigned short spi_clk_freq; /*KHz */
	unsigned short spi_delay_us;

	unsigned char cmd_size;
	unsigned char table_init_loaded; /* internal use */
	unsigned int table_init_on_cnt;
	unsigned int table_init_off_cnt;
	unsigned char *table_init_on;
	unsigned char *table_init_off;
	struct lcd_ext_common_s *common_config;
};

/* global API */
struct aml_lcd_extern_driver_s {
	struct lcd_extern_config_s *config;
	int (*reg_read)(unsigned char reg, unsigned char *buf);
	int (*reg_write)(unsigned char *buf, unsigned int len);
	int (*power_on)(void);
	int (*power_off)(void);
};

#define LCD_EXT_I2C_DEV_MAX    10
struct aml_lcd_extern_i2c_dev_s {
	char name[30];
	struct i2c_client *client;
};

struct aml_lcd_extern_driver_s *aml_lcd_extern_get_driver(int index);
void lcd_extern_index_lut_add(int index);
#endif

