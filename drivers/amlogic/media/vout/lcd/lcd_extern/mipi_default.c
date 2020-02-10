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
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/amlogic/media/vout/lcd/lcd_extern.h>
#include "lcd_extern.h"

#define LCD_EXTERN_INDEX		0
#define LCD_EXTERN_NAME			"mipi_default"
#define LCD_EXTERN_TYPE			LCD_EXTERN_MIPI

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
static unsigned char mipi_init_on_table[] = {
	0x05, 1,  0x11,
	0xfd, 1, 200,
	0x05, 1,  0x29,
	0xfd, 1, 20,
	0xff, 0,   /* ending */
};

static unsigned char mipi_init_off_table[] = {
	0x05, 1, 0x28, /* display off */
	0xfd, 1, 10,   /* delay 10ms */
	0x05, 1, 0x10, /* sleep in */
	0xfd, 1, 150,  /* delay 150ms */
	0xff, 0,   /* ending */
};

static int lcd_extern_driver_update(struct aml_lcd_extern_driver_s *ext_drv)
{
	if (!ext_drv) {
		EXTERR("%s driver is null\n", LCD_EXTERN_NAME);
		return -1;
	}

	if (ext_drv->config->table_init_loaded == 0) {
		ext_drv->config->cmd_size = LCD_EXT_CMD_SIZE_DYNAMIC;
		ext_drv->config->table_init_on  = &mipi_init_on_table[0];
		ext_drv->config->table_init_on_cnt =
			sizeof(mipi_init_on_table);
		ext_drv->config->table_init_off = &mipi_init_off_table[0];
		ext_drv->config->table_init_off_cnt =
			sizeof(mipi_init_off_table);
		EXTERR("%s: tablet_init is invalid\n", ext_drv->config->name);
		return -1;
	}

	return 0;
}

int aml_lcd_extern_mipi_default_probe(struct aml_lcd_extern_driver_s *ext_drv)
{
	int ret = 0;

	ret = lcd_extern_driver_update(ext_drv);

	if (lcd_debug_print_flag)
		EXTPR("%s: %d\n", __func__, ret);
	return ret;
}
