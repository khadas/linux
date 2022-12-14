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
#include <linux/amlogic/media/vout/lcd/lcd_extern.h>
#include "lcd_extern.h"

#define LCD_EXTERN_NAME			"i2c_ANX6862_7911"

#define LCD_EXTERN_I2C_ADDR		(0x20 >> 1) /* ANX6862 7bit address */
#define LCD_EXTERN_I2C_ADDR2		(0x74 >> 1) /* ANX7911 7bit address */

static unsigned char ANX6862_NVM_wr[] = {0xff, 0x80};
static unsigned char ANX7911_NVM_wr[] = {0x00, 0x0a};

#define ANX6862_REG_CNT   12
#define ANX7911_REG_CNT   23

static int lcd_extern_reg_read(struct lcd_extern_dev_s *edev,
			       unsigned char reg_byte_len,
			       unsigned short reg, unsigned char *buf)
{
	struct aml_lcd_extern_i2c_dev_s *i2c_dev;
	unsigned char tmp;
	int ret = 0;

	tmp = reg;
	if (edev->config.addr_sel)
		i2c_dev = edev->i2c_dev[1];
	else
		i2c_dev = edev->i2c_dev[0];
	if (!i2c_dev) {
		EXTERR("invalid i2c device\n");
		return -1;
	}
	lcd_extern_i2c_read(i2c_dev->client, &tmp, 1, &tmp, 1);
	buf[0] = tmp;

	return ret;
}

static int lcd_extern_reg_write(struct lcd_extern_dev_s *edev,
				unsigned char *buf, unsigned int len)
{
	struct aml_lcd_extern_i2c_dev_s *i2c_dev;
	int ret = 0;

	if (edev->config.addr_sel)
		i2c_dev = edev->i2c_dev[1];
	else
		i2c_dev = edev->i2c_dev[0];
	if (!i2c_dev) {
		EXTERR("invalid i2c device\n");
		return -1;
	}
	lcd_extern_i2c_write(i2c_dev->client, buf, 2);

	return ret;
}

static int lcd_extern_power_cmd_dynamic_size(struct lcd_extern_driver_s *edrv,
					     struct lcd_extern_dev_s *edev,
					     unsigned char *table, unsigned char flag)
{
	int i = 0, j, step = 0, max_len;
	unsigned char type, cmd_size, type_chk, type_other;
	struct aml_lcd_extern_i2c_dev_s *i2c_dev;
	int delay_bypass, delay_ms, ret = 0;

	if (!table)
		return -1;

	max_len = edev->config.table_init_on_cnt;
	if (flag) {
		i2c_dev = edev->i2c_dev[1];
		type_chk = LCD_EXT_CMD_TYPE_CMD2;
		type_other = LCD_EXT_CMD_TYPE_CMD;
	} else {
		i2c_dev = edev->i2c_dev[0];
		type_chk = LCD_EXT_CMD_TYPE_CMD;
		type_other = LCD_EXT_CMD_TYPE_CMD2;
	}
	if (!i2c_dev) {
		EXTERR("invalid i2c%d device\n", flag);
		return -1;
	}

	delay_bypass = 0;
	while ((i + 1) < max_len) {
		type = table[i];
		if (type == LCD_EXT_CMD_TYPE_END)
			break;
		cmd_size = table[i + 1];
		if (cmd_size == 0)
			goto power_cmd_dynamic_next;
		if ((i + 2 + cmd_size) > max_len)
			break;

		if (type == type_chk) {
			if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
				EXTPR("%s: step %d: type=0x%02x, cmd_size=%d\n",
				      __func__, step, type, table[i + 1]);
			}
			ret = lcd_extern_i2c_write(i2c_dev->client, &table[i + 2], cmd_size);
			delay_bypass = 0;
		} else if (type == LCD_EXT_CMD_TYPE_DELAY) {
			if (delay_bypass)
				goto power_cmd_dynamic_next;
			if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
				EXTPR("%s: step %d: type=0x%02x, cmd_size=%d\n",
				      __func__, step, type, table[i + 1]);
			}
			delay_ms = 0;
			for (j = 0; j < cmd_size; j++)
				delay_ms += table[i + 2 + j];
			if (delay_ms > 0)
				lcd_delay_ms(delay_ms);
		} else if (type == type_other) {
			delay_bypass = 1;
		} else {
			delay_bypass = 1;
			EXTERR("%s(%d: %s): type 0x%02x invalid\n",
			       __func__, edev->config.index,
			       edev->config.name, type);
		}
power_cmd_dynamic_next:
		i += (cmd_size + 2);
		step++;
	}

	return ret;
}

static int lcd_extern_power_cmd(struct lcd_extern_driver_s *edrv,
				struct lcd_extern_dev_s *edev, unsigned char flag)
{
	unsigned char *table;
	unsigned char cmd_size;
	int ret = 0;

	table = edev->config.table_init_on;
	cmd_size = edev->config.cmd_size;
	if (cmd_size != LCD_EXT_CMD_SIZE_DYNAMIC) {
		EXTERR("%s: cmd_size %d is invalid\n", __func__, cmd_size);
		return -1;
	}
	if (!table) {
		EXTERR("%s: init_on_table is NULL\n", __func__);
		return -1;
	}

	ret = lcd_extern_power_cmd_dynamic_size(edrv, edev, table, flag);

	EXTPR("%s: %s(%d): %d\n",
	      __func__, edev->config.name, edev->config.index, flag);
	return ret;
}

static int lcd_extern_check_reg_dynamic_size(struct lcd_extern_dev_s *edev,
					     unsigned char *table,
					     unsigned char *chk_table, int cnt,
					     unsigned char flag)
{
	int i = 0, step = 0, max_len;
	unsigned char type, cmd_size, type_chk, reg;
	int ret = 0;

	if (!table)
		return -1;

	max_len = edev->config.table_init_on_cnt;
	type_chk = (flag) ? LCD_EXT_CMD_TYPE_CMD2 : LCD_EXT_CMD_TYPE_CMD;

	while ((i + 1) < max_len) {
		type = table[i];
		if (type == LCD_EXT_CMD_TYPE_END)
			break;
		cmd_size = table[i + 1];
		if (cmd_size == 0) {
			i += 2;
			continue;
		}
		if ((i + 2 + cmd_size) > max_len)
			break;

		if (type == type_chk) {
			reg = table[i + 2];
			if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
				EXTPR("%s: step %d: reg 0x%02x, val 0x%02x,0x%02x\n",
				      __func__, step,
				      reg, table[i + 3], chk_table[reg]);
			}
			if (chk_table[reg] != table[i + 3])
				return -1;
		}
		i += (cmd_size + 2);
		step++;
	}

	return ret;
}

static int lcd_extern_init_check(struct lcd_extern_dev_s *edev, unsigned char flag)
{
	struct aml_lcd_extern_i2c_dev_s *i2c_dev;
	unsigned char *table, *chk_table, cmd_size;
	int cnt, ret = 0;

	table = edev->config.table_init_on;
	cmd_size = edev->config.cmd_size;
	cnt = (flag) ? ANX7911_REG_CNT : ANX6862_REG_CNT;
	if (cmd_size != LCD_EXT_CMD_SIZE_DYNAMIC) {
		EXTERR("%s: cmd_size %d is invalid\n", __func__, cmd_size);
		return -1;
	}
	if (!table) {
		EXTERR("%s: init_table %d is NULL\n", __func__, flag);
		return -1;
	}
	if (flag)
		i2c_dev = edev->i2c_dev[1];
	else
		i2c_dev = edev->i2c_dev[0];
	if (!i2c_dev) {
		EXTERR("%s: i2c_dev %d is NULL\n", __func__, flag);
		return -1;
	}

	chk_table = kcalloc(cnt, sizeof(unsigned char), GFP_KERNEL);
	if (!chk_table)
		return -1;

	ret = lcd_extern_i2c_read(i2c_dev->client, chk_table, 1, chk_table, cnt);
	if (ret) {
		EXTERR("%s: i2c read error\n", __func__);
		return -1;
	}

	ret = lcd_extern_check_reg_dynamic_size(table, chk_table, cnt, flag);

	return ret;
}

static int lcd_extern_power_on(struct lcd_extern_driver_s *edrv,
			       struct lcd_extern_dev_s *edev)
{
	int ret;

	lcd_extern_pinmux_set(edrv, 1);

	/* check voltage is init or not */
	/* step1: ANX6862 */
	if (!edev->i2c_dev[0]) {
		EXTERR("%s: invalid i2c0_dev\n", __func__);
		return -1;
	}
	ret = lcd_extern_init_check(edev, 0);
	if (ret) {
		EXTPR("ANX6862: need init voltage and NVM write\n");
		/* init voltage */
		lcd_extern_power_cmd(edrv, edev, 0);
		/* NVM write */
		lcd_extern_i2c_write(edev->i2c_dev[0]->client, ANX6862_NVM_wr, 2);
	}

	/* step2: ANX7911 */
	if (!edev->i2c_dev[1]) {
		EXTERR("%s: invalid i2c1_dev\n", __func__);
		return -1;
	}
	ret = lcd_extern_init_check(edev, 1);
	if (ret) {
		EXTPR("ANX7911: need init voltage and NVM write\n");
		/* init voltage */
		lcd_extern_power_cmd(edrv, edev, 1);
		/* NVM write */
		lcd_extern_i2c_write(edev->i2c_dev[1]->client, ANX7911_NVM_wr, 2);
	}

	EXTPR("%s\n", __func__);
	return ret;
}

static int lcd_extern_power_off(struct lcd_extern_driver_s *edrv,
				struct lcd_extern_dev_s *edev)
{
	lcd_extern_pinmux_set(edrv, 0);
	return 0;
}

static int lcd_extern_driver_update(struct lcd_extern_driver_s *edrv,
				    struct lcd_extern_dev_s *edev)
{
	if (edev->config.table_init_loaded == 0) {
		EXTERR("[%d]: dev_%d(%s): table_init is invalid\n",
		       edrv->index, edev->dev_index, edev->config.name);
		return -1;
	}

	ext_drv->reg_read  = lcd_extern_reg_read;
	ext_drv->reg_write = lcd_extern_reg_write;
	ext_drv->power_on  = lcd_extern_power_on;
	ext_drv->power_off = lcd_extern_power_off;

	return 0;
}

int lcd_extern_i2c_ANX6862_7911_probe(struct lcd_extern_driver_s *edrv,
				      struct lcd_extern_dev_s *edev)
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

	edev->i2c_dev[1] = lcd_extern_get_i2c_device(edev->config.i2c_addr2);
	if (edev->i2c_dev[1]) {
		EXTERR("[%d]: %s: dev_%d invalid i2c2 device\n",
		       edrv->index, __func__, edev->dev_index);
		return -1;
	}
	EXTPR("[%d]: %s: get i2c2 device: %s, addr 0x%02x OK\n",
	      edrv->index, __func__, edev->dev_index,
	      edev->i2c_dev[1]->name, edev->i2c_dev[1]->client->addr);

	ret = lcd_extern_driver_update(edrv, edev);
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
		EXTPR("[%d]: %s: dev_%d %d\n",
		      edrv->index, __func__, edev->dev_index, ret);
	}

	return ret;
}
