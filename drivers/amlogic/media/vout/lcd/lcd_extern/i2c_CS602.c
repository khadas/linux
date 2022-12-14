// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/amlogic/i2c-amlogic.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/amlogic/media/vout/lcd/lcd_extern.h>
#include "lcd_extern.h"

#define LCD_EXTERN_NAME		  "i2c_CS602"

static int lcd_extern_reg_read(struct lcd_extern_driver_s *edrv,
			       struct lcd_extern_dev_s *edev,
			       unsigned char reg_byte_len,
			       unsigned short reg, unsigned char *buf)
{
	int ret = 0;

	return ret;
}

static int lcd_extern_reg_write(struct lcd_extern_driver_s *edrv,
				struct lcd_extern_dev_s *edev,
				unsigned char *buf, unsigned int len)
{
	int ret = 0;

	if (!edev->i2c_dev[0]) {
		EXTERR("%s: invalid i2c client\n", __func__);
		return -1;
	}
	if (!buf) {
		EXTERR("%s: buf is full\n", __func__);
		return -1;
	}
	if (!len) {
		EXTERR("%s: invalid len\n", __func__);
		return -1;
	}

	ret = lcd_extern_i2c_write(edev->i2c_dev[0]->client, buf, len);
	return ret;
}

static int lcd_extern_init_check(struct lcd_extern_driver_s *edrv,
				 struct lcd_extern_dev_s *edev, int len)
{
	int ret = 0;
	unsigned char *chk_table;
	int i;

	chk_table = kcalloc(len, sizeof(unsigned char), GFP_KERNEL);
	if (!chk_table)
		return -1;

	if (!edev->i2c_dev[0]) {
		EXTERR("%s: invalid i2c client\n", __func__);
		kfree(chk_table);
		return -1;
	}
	ret = lcd_extern_i2c_read(edev->i2c_dev[0]->client, chk_table, 1, chk_table, len);
	if (ret == 0) {
		for (i = 0; i < len; i++) {
			if (chk_table[i] != edev->config.table_init_on[i + 3]) {
				kfree(chk_table);
				return -1;
			}
		}
	}

	kfree(chk_table);
	return 0;
}

static int lcd_extern_power_cmd_dynamic_size(struct lcd_extern_driver_s *edrv,
					     struct lcd_extern_dev_s *edev,
					     unsigned char *table, int flag)
{
	int i = 0, j, step = 0, max_len = 0;
	unsigned char type, cmd_size;
	int delay_ms, ret = 0;

	if (flag)
		max_len = edev->config.table_init_on_cnt;
	else
		max_len = edev->config.table_init_off_cnt;

	while ((i + 1) < max_len) {
		type = table[i];
		if (type == LCD_EXT_CMD_TYPE_END)
			break;
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			EXTPR("%s: step %d: type=0x%02x, cmd_size=%d\n",
			      __func__, step, type, table[i + 1]);
		}
		cmd_size = table[i + 1];
		if (cmd_size == 0)
			goto power_cmd_dynamic_next;
		if ((i + 2 + cmd_size) > max_len)
			break;

		if (type == LCD_EXT_CMD_TYPE_NONE) {
			/* do nothing */
		} else if (type == LCD_EXT_CMD_TYPE_GPIO) {
			if (cmd_size < 2) {
				EXTERR("step %d: invalid cmd_size %d for GPIO\n",
				       step, cmd_size);
				goto power_cmd_dynamic_next;
			}
			if (table[i + 2] < LCD_GPIO_MAX)
				lcd_extern_gpio_set(edrv, table[i + 2], table[i + 3]);
			if (cmd_size > 2) {
				if (table[i + 4] > 0)
					lcd_delay_ms(table[i + 4]);
			}
		} else if (type == LCD_EXT_CMD_TYPE_DELAY) {
			delay_ms = 0;
			for (j = 0; j < cmd_size; j++)
				delay_ms += table[i + 2 + j];
			if (delay_ms > 0)
				lcd_delay_ms(delay_ms);
		} else if ((type == LCD_EXT_CMD_TYPE_CMD) ||
			   (type == LCD_EXT_CMD_TYPE_CMD_BIN)) {
			if (!edev->i2c_dev[0]) {
				EXTERR("invalid i2c device\n");
				return -1;
			}
			ret = lcd_extern_reg_write(edev, &table[i + 2], cmd_size);
		} else if (type == LCD_EXT_CMD_TYPE_CMD_DELAY) {
			if (!edev->i2c_dev[0]) {
				EXTERR("invalid i2c device\n");
				return -1;
			}
			ret = lcd_extern_reg_write(edev, &table[i + 2], (cmd_size - 1));
			if (table[i + cmd_size + 1] > 0)
				lcd_delay_ms(table[i + cmd_size + 1]);
		} else {
			EXTERR("%s: %s(%d): type 0x%02x invalid\n",
			       __func__, edev->config.name,
			       edev->config.index, type);
		}
power_cmd_dynamic_next:
		i += (cmd_size + 2);
		step++;
	}

	return ret;
}

static int lcd_extern_power_ctrl(struct lcd_extern_driver_s *edrv,
				 struct lcd_extern_dev_s *edev, int flag)
{
	unsigned char *table;
	unsigned char cmd_size;
	unsigned char check_data = 0;
	unsigned char check_len = 1;
	int ret = 0;
	int i = 0;

	cmd_size = edev->config.cmd_size;
	if (flag)
		table = edev->config.table_init_on;
	else
		table = edev->config.table_init_off;
	if (!table) {
		EXTERR("%s: init_table %d is NULL\n", __func__, flag);
		return -1;
	}
	if (cmd_size != LCD_EXT_CMD_SIZE_DYNAMIC) {
		EXTERR("%s: cmd_size %d is invalid\n", __func__, cmd_size);
		return -1;
	}

	ret = lcd_extern_init_check(cmd_size);
	if (!ret)
		ret = lcd_extern_power_cmd_dynamic_size(table, flag);

	for (i = 0; i < 10; i++) {
		ret = lcd_extern_i2c_read(edev->i2c_dev[0]->client,
					  &check_data, 1, &check_data, check_len);
		if (ret)
			continue;
		if ((check_data >> 6) & 0x1)
			break;

		ret = lcd_extern_power_cmd_dynamic_size(table, flag);
	}

	EXTPR("%s: %s(%d): %d\n",
	      __func__, edev->config.name, edev->config.index, flag);
	return ret;
}

static int lcd_extern_power_on(struct lcd_extern_driver_s *edrv,
			       struct lcd_extern_dev_s *edev)
{
	int ret;

	lcd_extern_pinmux_set(edrv, 1);
	ret = lcd_extern_power_ctrl(edrv, edev, 1);
	return ret;
}

static int lcd_extern_power_off(struct lcd_extern_driver_s *edrv,
				struct lcd_extern_dev_s *edev)
{
	int ret;

	ret = lcd_extern_power_ctrl(edrv, edev, 0);
	lcd_extern_pinmux_set(edrv, 0);
	return ret;
}

static int lcd_extern_driver_update(struct lcd_extern_driver_s *edrv,
				    struct lcd_extern_dev_s *edev)
{
	if (edev->config.table_init_loaded == 0) {
		EXTERR("[%d]: dev_%d(%s): table_init is invalid\n",
		       edrv->index, edev->dev_index, edev->config.name);
		return -1;
	}

	edev->power_on  = lcd_extern_power_on;
	edev->power_off = lcd_extern_power_off;
	edev->reg_read = lcd_extern_reg_read;

	return 0;
}

int lcd_extern_i2c_CS602_probe(struct lcd_extern_driver_s *edrv, struct lcd_extern_dev_s *edev)
{
	int ret = 0;

	if (!edrv || !edev) {
		EXTERR("%s: %s dev is null\n", __func__, LCD_EXTERN_NAME);
		return -1;
	}

	edev->i2c_dev[0] = lcd_extern_get_i2c_device(edev->config.i2c_addr);
	if (edev->i2c_dev[0]) {
		EXTERR("[%d]: %s: dev_%d invalid i2c device\n",
		       edrv->index, __func__, edev->dev_index);
		return -1;
	}
	EXTPR("[%d]: %s: get i2c device: %s, addr 0x%02x OK\n",
	      edrv->index, __func__, edev->dev_index,
	      edev->i2c_dev[0]->name, edev->i2c_dev[0]->client->addr);

	ret = lcd_extern_driver_update(edrv, edev);
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
		EXTPR("[%d]: %s: dev_%d %d\n",
		      edrv->index, __func__, edev->dev_index, ret);
	}
	return ret;
}

