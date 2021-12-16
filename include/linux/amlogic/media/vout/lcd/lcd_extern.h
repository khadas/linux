/* SPDX-License-Identifier: GPL-2.0+ */
/*
 *
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#ifndef _INC_AML_LCD_EXTERN_H_
#define _INC_AML_LCD_EXTERN_H_
#include <linux/platform_device.h>
#include <linux/cdev.h>
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

#define LCD_EXTERN_DEV_MAX            20

struct lcd_extern_driver_s;

#define LCD_EXT_I2C_DEV_MAX    10
struct lcd_extern_i2c_dev_s {
	char name[30];
	struct i2c_client *client;
};

struct lcd_extern_multi_list_s {
	unsigned int index;
	unsigned int type;
	unsigned char data_len;
	unsigned char *data_buf;
	struct lcd_extern_multi_list_s *next;
};

struct lcd_extern_config_s {
	unsigned char index;
	char name[LCD_EXTERN_NAME_LEN_MAX];
	enum lcd_extern_type_e type;
	unsigned char status;

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
};

struct lcd_extern_dev_s {
	int dev_index;
	unsigned char addr_sel; /* internal used */
	struct lcd_extern_config_s config;
	struct lcd_extern_multi_list_s *multi_list_header;
	struct lcd_extern_i2c_dev_s *i2c_dev[4];
	unsigned char check_state[4];
	unsigned char check_flag;
	unsigned char check_offset;
	unsigned char check_len;

	int (*reg_read)(struct lcd_extern_driver_s *edrv,
			struct lcd_extern_dev_s *edev,
			unsigned char reg, unsigned char *buf);
	int (*reg_write)(struct lcd_extern_driver_s *edrv,
			struct lcd_extern_dev_s *edev,
			unsigned char *buf, unsigned int len);
	int (*power_on)(struct lcd_extern_driver_s *edrv,
			struct lcd_extern_dev_s *edev);
	int (*power_off)(struct lcd_extern_driver_s *edrv,
			 struct lcd_extern_dev_s *edev);
};

struct lcd_ext_gpio_s {
	char name[LCD_CPU_GPIO_NAME_MAX];
	struct gpio_desc *gpio;
	int probe_flag;
	int register_flag;
};

/* global API */
struct lcd_extern_driver_s {
	int index;
	unsigned char key_valid;
	unsigned char config_load;
	unsigned int dev_cnt;
	struct lcd_ext_gpio_s gpio[LCD_EXTERN_GPIO_NUM_MAX];
	struct lcd_extern_dev_s *dev[LCD_EXTERN_DEV_MAX];

	unsigned char i2c_bus;
	unsigned char pinmux_valid;
	unsigned int pinmux_flag;
	struct pinctrl *pin;

	struct cdev               cdev;
	struct platform_device    *pdev;
	struct device             *sub_dev;
	struct work_struct        dev_probe_work;
};

struct lcd_extern_driver_s *lcd_extern_get_driver(int drv_index);
struct lcd_extern_dev_s *lcd_extern_get_dev(struct lcd_extern_driver_s *edrv, int dev_index);
int lcd_extern_dev_index_add(int drv_index, int dev_index);
int lcd_extern_dev_index_remove(int drv_index, int dev_index);
int lcd_extern_init(void);
#endif

