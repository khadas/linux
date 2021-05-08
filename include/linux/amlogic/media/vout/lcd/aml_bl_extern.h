/* SPDX-License-Identifier: GPL-2.0+ */
/*
 *
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#ifndef _INC_AML_BL_EXTERN_H_
#define _INC_AML_BL_EXTERN_H_
#include <linux/amlogic/media/vout/lcd/aml_lcd.h>
#ifdef CONFIG_AMLOGIC_LCD_TABLET
#include <linux/amlogic/media/vout/lcd/lcd_mipi.h>
#endif

enum bl_extern_type_e {
	BL_EXTERN_I2C = 0,
	BL_EXTERN_SPI,
	BL_EXTERN_MIPI,
	BL_EXTERN_MAX,
};

#define BL_EXTERN_INIT_ON_MAX    300
#define BL_EXTERN_INIT_OFF_MAX   50

#define BL_EXTERN_GPIO_NUM_MAX   6
#define BL_EXTERN_INDEX_INVALID  0xff
#define BL_EXTERN_NAME_LEN_MAX   30

#define BL_EXT_I2C_DEV_MAX       10

struct bl_extern_i2c_dev_s {
	char name[30];
	struct i2c_client *client;
};

struct bl_extern_config_s {
	unsigned char index;
	char name[BL_EXTERN_NAME_LEN_MAX];
	enum bl_extern_type_e type;
	unsigned char i2c_addr;
	unsigned char i2c_bus;
	unsigned int dim_min;
	unsigned int dim_max;

	unsigned char init_loaded;
	unsigned char cmd_size;
	unsigned char *init_on;
	unsigned char *init_off;
	unsigned int init_on_cnt;
	unsigned int init_off_cnt;
};

/* global API */
struct bl_extern_driver_s {
	int index;
	unsigned int status;
	unsigned int brightness;
	unsigned int level_max;
	unsigned int level_min;
	int (*power_on)(struct bl_extern_driver_s *bext);
	int (*power_off)(struct bl_extern_driver_s *bext);
	int (*set_level)(struct bl_extern_driver_s *bext, unsigned int level);
	void (*config_print)(struct bl_extern_driver_s *bext);
	int (*device_power_on)(struct bl_extern_driver_s *bext);
	int (*device_power_off)(struct bl_extern_driver_s *bext);
	int (*device_bri_update)(struct bl_extern_driver_s *bext, unsigned int level);
	struct bl_extern_config_s config;
	struct bl_extern_i2c_dev_s *i2c_dev;
	struct device *dev;
};

struct bl_extern_driver_s *bl_extern_get_driver(int index);
int bl_extern_dev_index_add(int drv_index, int dev_index);

#endif
