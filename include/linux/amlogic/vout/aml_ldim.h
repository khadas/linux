/*
 * include/linux/amlogic/vout/aml_ldim.h
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

#ifndef _INC_AML_LDIM_H_
#define _INC_AML_LDIM_H_
#include <linux/types.h>
#include <linux/platform_device.h>
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/pinctrl/consumer.h>
#include <linux/amlogic/vout/aml_bl.h>
#include <linux/spi/spi.h>

#define LDIM_SPI_INIT_ON_SIZE     300
#define LDIM_SPI_INIT_OFF_SIZE    20
struct ldim_dev_config_s {
	char name[20];
	int cs_hold_delay;
	int cs_clk_delay;
	int en_gpio;
	int en_gpio_on;
	int en_gpio_off;
	int lamp_err_gpio;
	unsigned char fault_check;
	unsigned char write_check;

	unsigned int dim_min;
	unsigned int dim_max;
	unsigned char cmd_size;
	unsigned char *init_on;
	unsigned char *init_off;

	struct bl_pwm_config_s pwm_config;
};

/*******global API******/
struct aml_ldim_driver_s {
	int valid_flag;
	int dev_index;
	int static_pic_flag;
	struct ldim_dev_config_s *ldev_conf;
	unsigned short *ldim_matrix_2_spi;
	int (*init)(void);
	int (*power_on)(void);
	int (*power_off)(void);
	int (*set_level)(unsigned int level);
	int (*pinmux_ctrl)(int status);
	int (*device_power_on)(void);
	int (*device_power_off)(void);
	int (*device_bri_update)(unsigned short *buf, unsigned char len);
	void (*config_print)(void);
	void (*test_ctrl)(int flag);
	struct pinctrl *pin;
	struct device *dev;
	struct spi_device *spi;
};

extern struct aml_ldim_driver_s *aml_ldim_get_driver(void);
extern int aml_ldim_probe(struct platform_device *pdev);
extern int aml_ldim_remove(void);

#endif

