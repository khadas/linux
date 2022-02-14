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
	0xff, 0,   /* ending */
};

static unsigned char mipi_init_off_table[] = {
	0xff, 0,   /* ending */
};

static int lcd_extern_driver_update(struct lcd_extern_driver_s *edrv,
				    struct lcd_extern_dev_s *edev)
{
	if (edev->config.table_init_loaded == 0) {
		edev->config.cmd_size = LCD_EXT_CMD_SIZE_DYNAMIC;
		edev->config.table_init_on  = &mipi_init_on_table[0];
		edev->config.table_init_on_cnt = sizeof(mipi_init_on_table);
		edev->config.table_init_off = &mipi_init_off_table[0];
		edev->config.table_init_off_cnt = sizeof(mipi_init_off_table);
		EXTERR("[%d]: %s: dev_%d(%s) tablet_init is invalid\n",
		       edrv->index, __func__, edev->dev_index, edev->config.name);
		return -1;
	}

	return 0;
}

int lcd_extern_mipi_default_probe(struct lcd_extern_driver_s *edrv, struct lcd_extern_dev_s *edev)
{
	int ret = 0;

	if (!edrv || !edev) {
		EXTERR("%s: dev is null\n", __func__);
		return -1;
	}

	if (edev->config.type != LCD_EXTERN_MIPI) {
		EXTERR("[%d]: %s: invalid dev: %s(%d) type %d\n",
		       edrv->index, __func__,
		       edev->config.name, edev->dev_index,
		       edev->config.type);
		return -1;
	}
	if (edev->config.cmd_size != LCD_EXT_CMD_SIZE_DYNAMIC) {
		EXTERR("[%d]: %s: %s(%d): cmd_size %d is invalid\n",
			edrv->index, __func__,
			edev->config.name,
			edev->dev_index,
			edev->config.cmd_size);
		return -1;
	}

	ret = lcd_extern_driver_update(edrv, edev);

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		EXTPR("[%d]: %s: dev_%d %d\n", edrv->index, __func__, edev->dev_index, ret);
	return ret;
}
