// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 *
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/amlogic/media/vout/lcd/aml_bl_extern.h>
#include "bl_extern.h"

#define BL_EXTERN_NAME			"mipi_lt070me05"
#define BL_EXTERN_TYPE			BL_EXTERN_MIPI

static int mipi_lt070me05_power_on(struct bl_extern_driver_s *bext)
{
	return 0;
}

static int mipi_lt070me05_power_off(struct bl_extern_driver_s *bext)
{
	return 0;
}

/******************** mipi command ********************/
/* format:  data_type, cmd_size, data.... */
/*	data_type=0xff,
 *		0 < cmd_size < 0xff means delay ms,
 *		cmd_size=0 or 0xff means ending.
 *	data_type=0xf0, for gpio control
 *		data0=gpio_index, data1=gpio_value.
 *		data0=gpio_index, data1=gpio_value, data2=delay.
 *	data_type=0xfd, for delay ms
 *		data0=delay, data_1=delay, ..., data_n=delay.
 */
static int mipi_lt070me05_set_level(struct bl_extern_driver_s *bext, unsigned int level)
{
#ifdef CONFIG_AMLOGIC_LCD_TABLET
	struct aml_lcd_drv_s *pdrv;
	unsigned char payload[] = {0x15, 2, 0x51, 0xe6, 0xff, 0xff};

	if (!pdrv)
		return -1;

	pdrv = aml_lcd_get_driver(bext->index);
	payload[3] = level & 0xff;
	dsi_write_cmd(pdrv, &payload[0]);
#endif

	return 0;
}

static int mipi_lt070me05_update(struct bl_extern_driver_s *bext)
{
	bext->device_power_on = mipi_lt070me05_power_on;
	bext->device_power_off = mipi_lt070me05_power_off;
	bext->device_bri_update = mipi_lt070me05_set_level;

	return 0;
}

int mipi_lt070me05_probe(struct bl_extern_driver_s *bext)
{
	int ret = 0;

	if (!bext)
		return;

	ret = mipi_lt070me05_update(bext);

	BLEX("[%d]: %s: %d\n", bext->index, __func__, ret);

	return ret;
}

int mipi_lt070me05_remove(struct bl_extern_driver_s *bext)
{
	return 0;
}

