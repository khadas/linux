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

#define LCD_EXTERN_NAME           "ext_default"
#define LCD_EXTERN_TYPE           LCD_EXTERN_MAX

static void set_lcd_csb(struct lcd_extern_driver_s *edrv,
			struct lcd_extern_dev_s *edev, unsigned int v)
{
	lcd_extern_gpio_set(edrv, edev->config.spi_gpio_cs, v);
	udelay(edev->config.spi_delay_us);
}

static void set_lcd_scl(struct lcd_extern_driver_s *edrv,
			struct lcd_extern_dev_s *edev, unsigned int v)
{
	lcd_extern_gpio_set(edrv, edev->config.spi_gpio_clk, v);
	udelay(edev->config.spi_delay_us);
}

static void set_lcd_sda(struct lcd_extern_driver_s *edrv,
			struct lcd_extern_dev_s *edev, unsigned int v)
{
	lcd_extern_gpio_set(edrv, edev->config.spi_gpio_data, v);
	udelay(edev->config.spi_delay_us);
}

static void spi_gpio_init(struct lcd_extern_driver_s *edrv,
			  struct lcd_extern_dev_s *edev)
{
	set_lcd_csb(edrv, edev, 1);
	set_lcd_scl(edrv, edev, 1);
	set_lcd_sda(edrv, edev, 1);
}

static void spi_gpio_off(struct lcd_extern_driver_s *edrv,
			 struct lcd_extern_dev_s *edev)
{
	set_lcd_sda(edrv, edev, 0);
	set_lcd_scl(edrv, edev, 0);
	set_lcd_csb(edrv, edev, 0);
}

static void spi_write_byte(struct lcd_extern_driver_s *edrv,
			   struct lcd_extern_dev_s *edev, unsigned char data)
{
	int i;

	for (i = 0; i < 8; i++) {
		set_lcd_scl(edrv, edev, 0);
		if (data & 0x80)
			set_lcd_sda(edrv, edev, 1);
		else
			set_lcd_sda(edrv, edev, 0);
		data <<= 1;
		set_lcd_scl(edrv, edev, 1);
	}
}

static int lcd_extern_spi_write(struct lcd_extern_driver_s *edrv,
				struct lcd_extern_dev_s *edev,
				unsigned char *buf, int len)
{
	int i;

	if (len < 2) {
		EXTERR("[%d]: %s: dev_%d len %d error\n",
		       edrv->index, __func__, edev->dev_index, len);
		return -1;
	}

	set_lcd_csb(edrv, edev, 0);

	for (i = 0; i < len; i++)
		spi_write_byte(edrv, edev, buf[i]);

	set_lcd_csb(edrv, edev, 1);
	set_lcd_scl(edrv, edev, 1);
	set_lcd_sda(edrv, edev, 1);
	udelay(edev->config.spi_delay_us);

	return 0;
}

static int lcd_extern_reg_read(struct lcd_extern_driver_s *edrv,
			       struct lcd_extern_dev_s *edev,
			       unsigned char reg_byte_len,
			       unsigned short reg, unsigned char *buf)
{
	struct lcd_extern_i2c_dev_s *i2c_dev;
	unsigned char tmp;
	int ret = 0;

	tmp = reg;
	switch (edev->config.type) {
	case LCD_EXTERN_I2C:
		if (edev->addr_sel >= 4) {
			EXTERR("[%d]: %s: dev_%d invalid addr_sel %d\n",
			       edrv->index, __func__, edev->dev_index, edev->addr_sel);
			return -1;
		}
		i2c_dev = edev->i2c_dev[edev->addr_sel];
		if (!i2c_dev) {
			EXTERR("[%d]: %s: dev_%d i2c[%d] device is null\n",
			       edrv->index, __func__, edev->dev_index, edev->addr_sel);
			return -1;
		}
		lcd_extern_i2c_read(i2c_dev->client, &tmp, 1, &tmp, 1);
		buf[0] = tmp;
		break;
	case LCD_EXTERN_SPI:
		EXTPR("[%d]: %s: not support\n", edrv->index, __func__);
		break;
	default:
		break;
	}

	return ret;
}

static int lcd_extern_reg_write(struct lcd_extern_driver_s *edrv,
			       struct lcd_extern_dev_s *edev,
			       unsigned char *buf, unsigned int len)
{
	struct lcd_extern_i2c_dev_s *i2c_dev;
	int ret = 0;

	switch (edev->config.type) {
	case LCD_EXTERN_I2C:
		if (edev->addr_sel >= 4) {
			EXTERR("[%d]: %s: dev_%d invalid addr_sel %d\n",
			       edrv->index, __func__, edev->dev_index, edev->addr_sel);
			return -1;
		}
		i2c_dev = edev->i2c_dev[edev->addr_sel];
		if (!i2c_dev) {
			EXTERR("[%d]: %s: dev_%d i2c[%d] device is null\n",
			       edrv->index, __func__, edev->dev_index, edev->addr_sel);
			return -1;
		}
		lcd_extern_i2c_write(i2c_dev->client, buf, len);
		break;
	case LCD_EXTERN_SPI:
		lcd_extern_spi_write(edrv, edev, buf, 2);
		break;
	default:
		break;
	}

	return ret;
}

static void lcd_extern_init_reg_check(struct lcd_extern_dev_s *edev,
				      struct i2c_client *i2client,
				      unsigned char type,
				      unsigned char *raw_table,
				      unsigned char data_len)
{
	unsigned char *chk_table, *chk_buf, *raw_buf;
	unsigned char index;
	unsigned char temp_flag = edev->check_flag;
	int ret = 0;

	/* if not need to check return */
	if (edev->check_flag == 0)
		return;

	index = type & 0x0f;
	if (index >= 4 || data_len < 1 || !raw_table)
		goto parameter_check_err0;
	edev->check_state[index] = 0;
	edev->check_flag = 0;
	chk_table = kcalloc(data_len, sizeof(unsigned char), GFP_KERNEL);
	if (!chk_table)
		goto parameter_check_err0;
	if (((type & 0xf0) == LCD_EXT_CMD_TYPE_CMD) ||
	    ((type & 0xf0) == LCD_EXT_CMD_TYPE_CMD_BIN)) {
		if (data_len < 2)
			goto parameter_check_err1;
		chk_table[0] = raw_table[0];
		data_len--;
		raw_table++;
	}
	ret = lcd_extern_i2c_read(i2client, chk_table, 1, chk_table, data_len);

	if (!edev->check_len || edev->check_len > data_len)
		edev->check_len = data_len;
	if (edev->check_offset > data_len)
		edev->check_offset = 0;
	if ((ret) || (edev->check_offset + edev->check_len) > data_len)
		goto parameter_check_err1;
	chk_buf = chk_table + edev->check_offset;
	raw_buf = raw_table + edev->check_offset;
	ret = memcmp(chk_buf, raw_buf, edev->check_len);
	if (ret == 0)
		edev->check_state[index] = temp_flag;
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		EXTERR("%s: dev_%d ret: %d\n", __func__, edev->dev_index, ret);
	kfree(chk_table);
	return;

parameter_check_err1:
	kfree(chk_table);
parameter_check_err0:
	EXTERR("%s: dev_%d: error parameters\n", __func__, edev->dev_index);
}

static void lcd_extern_init_reg_check2(struct lcd_extern_dev_s *edev,
				       struct i2c_client *i2client,
				       unsigned char type,
				       unsigned char *raw_table,
				       unsigned char data_len)
{
	unsigned char *chk_table, *chk_buf, *raw_buf;
	unsigned char index;
	unsigned char temp_flag = edev->check_flag;
	int ret = 0;

	/* if not need to check return */
	if (edev->check_flag == 0)
		return;

	index = type & 0x0f;
	if (index >= 4 || data_len < 1 || !raw_table)
		goto parameter_check2_err0;
	edev->check_state[index] = 0;
	edev->check_flag = 0;
	chk_table = kcalloc(data_len, sizeof(unsigned char), GFP_KERNEL);
	if (!chk_table)
		goto parameter_check2_err0;
	ret = lcd_extern_i2c_read(i2client, chk_table, 1,
		chk_table, edev->check_offset + data_len);

	if (!edev->check_len || edev->check_len > data_len)
		edev->check_len = data_len;
	if (ret)
		goto parameter_check2_err1;
	chk_buf = chk_table + edev->check_offset;
	raw_buf = raw_table + 1;
	ret = memcmp(chk_buf, raw_buf, edev->check_len);
	if (ret == 0)
		edev->check_state[index] = temp_flag;
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		EXTERR("%s: dev_%d ret: %d\n", __func__, edev->dev_index, ret);
	kfree(chk_table);
	return;

parameter_check2_err1:
	kfree(chk_table);
parameter_check2_err0:
	EXTERR("%s: dev_%d: error parameters\n", __func__, edev->dev_index);
}

static int lcd_extern_cmd_multi_id(struct lcd_extern_driver_s *edrv,
				   struct lcd_extern_dev_s *edev,
				   unsigned char multi_id)
{
	struct aml_lcd_drv_s *pdrv;
	struct lcd_extern_multi_list_s *temp_list;
	unsigned int frame_rate, frame_rate_max, frame_rate_min;

	pdrv = aml_lcd_get_driver(edrv->index);
	if (!pdrv)
		return -1;
	if (pdrv->config.cus_ctrl.dlg_flag == 0)
		return -1;

	frame_rate = pdrv->config.timing.sync_duration_num /
		pdrv->config.timing.sync_duration_den;

	temp_list = edev->multi_list_header;
	while (temp_list) {
		if (multi_id == temp_list->index) {
			if (temp_list->type == LCD_EXT_CMD_TYPE_MULTI_FR) {
				frame_rate_min = temp_list->data_buf[0];
				frame_rate_max = temp_list->data_buf[1];
				if (frame_rate < frame_rate_min ||
				    frame_rate > frame_rate_max) {
					return -1;
				}
				EXTPR("[%d]: %s: dev_%d: multi_id=%d, type=0x%x, framerate=%d\n",
					edrv->index, __func__, edev->dev_index,
					temp_list->index, temp_list->type, frame_rate);
				return 0;
			}
		}
		temp_list = temp_list->next;
	}
	return -1;
}

static int lcd_extern_power_cmd_dynamic_size(struct lcd_extern_driver_s *edrv,
					     struct lcd_extern_dev_s *edev,
					     unsigned char *table, int flag)
{
	int i = 0, j = 0, step = 0, max_len = 0;
	unsigned char type, size, temp;
	int delay_ms, ret = 0;

	if (flag)
		max_len = edev->config.table_init_on_cnt;
	else
		max_len = edev->config.table_init_off_cnt;

	switch (edev->config.type) {
	case LCD_EXTERN_I2C:
		while ((i + 1) < max_len) {
			type = table[i];
			size = table[i + 1];
			if (type == LCD_EXT_CMD_TYPE_END)
				break;
			if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
				EXTPR("[%d]: %s: dev_%d step %d: type=0x%02x, size=%d\n",
				      edrv->index, __func__,
				      edev->dev_index, step, type, size);
			}
			if (size == 0)
				goto power_cmd_dynamic_i2c_next;
			if ((i + 2 + size) > max_len)
				break;

			if (type == LCD_EXT_CMD_TYPE_NONE) {
				/* do nothing */
			} else if (type == LCD_EXT_CMD_TYPE_GPIO) {
				if (size < 2) {
					EXTERR("[%d]: dev_%d step %d: invalid size %d for GPIO\n",
					       edrv->index, edev->dev_index, step, size);
					goto power_cmd_dynamic_i2c_next;
				}
				if (table[i + 2] < LCD_GPIO_MAX)
					lcd_extern_gpio_set(edrv, table[i + 2], table[i + 3]);
				if (size > 2) {
					if (table[i + 4] > 0)
						lcd_delay_ms(table[i + 4]);
				}
			} else if (type == LCD_EXT_CMD_TYPE_DELAY) {
				delay_ms = 0;
				for (j = 0; j < size; j++)
					delay_ms += table[i + 2 + j];
				if (delay_ms > 0)
					lcd_delay_ms(delay_ms);
			} else if (type == LCD_EXT_CMD_TYPE_CHECK) {
				if (size == 1) {
					edev->check_flag = table[i + 2];
					edev->check_offset = 0;
					edev->check_len = 0;
				} else if (size == 3) {
					edev->check_flag = table[i + 2];
					edev->check_offset = table[i + 3];
					edev->check_len = table[i + 4];
				} else {
					edev->check_flag = 0;
				}
			} else if (type == LCD_EXT_CMD_TYPE_CMD ||
				   type == LCD_EXT_CMD_TYPE_CMD_BIN ||
				   type == LCD_EXT_CMD_TYPE_CMD_BIN_DATA) {
				if (!edev->i2c_dev[0]) {
					EXTERR("[%d]: dev_%d invalid i2c0 device\n",
					       edrv->index, edev->dev_index);
					return -1;
				}
				lcd_extern_init_reg_check(edev, edev->i2c_dev[0]->client,
							  type, &table[i + 2], size);
				if (edev->check_state[0] == 1)
					goto power_cmd_dynamic_i2c_next;
				ret = lcd_extern_i2c_write(edev->i2c_dev[0]->client,
							   &table[i + 2], size);
			} else if (type == LCD_EXT_CMD_TYPE_CMD_BIN2) {
				if (!edev->i2c_dev[0]) {
					EXTERR("[%d]: dev_%d invalid i2c0 device\n",
					       edrv->index, edev->dev_index);
					return -1;
				}
				lcd_extern_init_reg_check2(edev, edev->i2c_dev[0]->client,
							   type, &table[i + 2], size);
				if (edev->check_state[0] == 1)
					goto power_cmd_dynamic_i2c_next;
				ret = lcd_extern_i2c_write(edev->i2c_dev[0]->client,
							   &table[i + 2], size);

			} else if ((type == LCD_EXT_CMD_TYPE_CMD2) ||
				   (type == LCD_EXT_CMD_TYPE_CMD2_BIN) ||
				   (type == LCD_EXT_CMD_TYPE_CMD2_BIN_DATA)) {
				if (!edev->i2c_dev[1]) {
					EXTERR("[%d]: dev_%d invalid i2c1 device\n",
					       edrv->index, edev->dev_index);
					return -1;
				}
				lcd_extern_init_reg_check(edev, edev->i2c_dev[1]->client,
							  type, &table[i + 2], size);
				if (edev->check_state[1] == 1)
					goto power_cmd_dynamic_i2c_next;
				ret = lcd_extern_i2c_write(edev->i2c_dev[1]->client,
							   &table[i + 2], size);
			} else if (type == LCD_EXT_CMD_TYPE_CMD2_BIN2) {
				if (!edev->i2c_dev[1]) {
					EXTERR("[%d]: dev_%d invalid i2c1 device\n",
					       edrv->index, edev->dev_index);
					return -1;
				}
				lcd_extern_init_reg_check2(edev, edev->i2c_dev[1]->client,
							   type, &table[i + 2], size);
				if (edev->check_state[1] == 1)
					goto power_cmd_dynamic_i2c_next;
				ret = lcd_extern_i2c_write(edev->i2c_dev[1]->client,
							   &table[i + 2], size);
			} else if ((type == LCD_EXT_CMD_TYPE_CMD3) ||
				   (type == LCD_EXT_CMD_TYPE_CMD3_BIN) ||
				   (type == LCD_EXT_CMD_TYPE_CMD3_BIN_DATA)) {
				if (!edev->i2c_dev[2]) {
					EXTERR("[%d]: dev_%d invalid i2c2 device\n",
					       edrv->index, edev->dev_index);
					return -1;
				}
				lcd_extern_init_reg_check(edev, edev->i2c_dev[2]->client,
							  type, &table[i + 2], size);
				if (edev->check_state[2] == 1)
					goto power_cmd_dynamic_i2c_next;
				ret = lcd_extern_i2c_write(edev->i2c_dev[2]->client,
							   &table[i + 2], size);
			} else if (type == LCD_EXT_CMD_TYPE_CMD3_BIN2) {
				if (!edev->i2c_dev[2]) {
					EXTERR("[%d]: dev_%d invalid i2c2 device\n",
					       edrv->index, edev->dev_index);
					return -1;
				}
				lcd_extern_init_reg_check2(edev, edev->i2c_dev[2]->client,
							   type, &table[i + 2], size);
				if (edev->check_state[2] == 1)
					goto power_cmd_dynamic_i2c_next;
				ret = lcd_extern_i2c_write(edev->i2c_dev[2]->client,
							   &table[i + 2], size);
			} else if ((type == LCD_EXT_CMD_TYPE_CMD4) ||
				   (type == LCD_EXT_CMD_TYPE_CMD4_BIN) ||
				   (type == LCD_EXT_CMD_TYPE_CMD4_BIN_DATA)) {
				if (!edev->i2c_dev[3]) {
					EXTERR("[%d]: dev_%d invalid i2c3 device\n",
					       edrv->index, edev->dev_index);
					return -1;
				}
				lcd_extern_init_reg_check(edev, edev->i2c_dev[3]->client,
							  type, &table[i + 2], size);
				if (edev->check_state[3] == 1)
					goto power_cmd_dynamic_i2c_next;
				ret = lcd_extern_i2c_write(edev->i2c_dev[3]->client,
							   &table[i + 2], size);
			} else if (type == LCD_EXT_CMD_TYPE_CMD4_BIN2) {
				if (!edev->i2c_dev[3]) {
					EXTERR("[%d]: dev_%d invalid i2c3 device\n",
					       edrv->index, edev->dev_index);
					return -1;
				}
				lcd_extern_init_reg_check2(edev, edev->i2c_dev[3]->client,
							   type, &table[i + 2], size);
				if (edev->check_state[3] == 1)
					goto power_cmd_dynamic_i2c_next;
				ret = lcd_extern_i2c_write(edev->i2c_dev[3]->client,
							   &table[i + 2], size);
			} else if (type == LCD_EXT_CMD_TYPE_MULTI_FR) {
				/* do nothing here */
			} else if (type == LCD_EXT_CMD_TYPE_CMD_MULTI) {
				if (!edev->i2c_dev[0]) {
					EXTERR("[%d]: dev_%d invalid i2c0 device\n",
					       edrv->index, edev->dev_index);
					return -1;
				}
				ret = lcd_extern_cmd_multi_id(edrv, edev, table[i + 2]);
				if (ret)
					goto power_cmd_dynamic_i2c_next;
				temp = (table[i + 3] << 4);
				if (temp == LCD_EXT_CMD_TYPE_CMD_BIN2) {
					lcd_extern_init_reg_check2(edev, edev->i2c_dev[0]->client,
						temp, &table[i + 4], (size - 2));
				} else {
					lcd_extern_init_reg_check(edev, edev->i2c_dev[0]->client,
						temp, &table[i + 4], (size - 2));
				}
				if (edev->check_state[0] == 1)
					goto power_cmd_dynamic_i2c_next;
				ret = lcd_extern_i2c_write(edev->i2c_dev[0]->client,
							&table[i + 4], (size - 2));
			} else if (type == LCD_EXT_CMD_TYPE_CMD2_MULTI) {
				if (!edev->i2c_dev[1]) {
					EXTERR("[%d]: dev_%d invalid i2c1 device\n",
					       edrv->index, edev->dev_index);
					return -1;
				}
				ret = lcd_extern_cmd_multi_id(edrv, edev, table[i + 2]);
				if (ret)
					goto power_cmd_dynamic_i2c_next;
				temp = (table[i + 3] << 4) | (1 << 0);
				if (temp == LCD_EXT_CMD_TYPE_CMD2_BIN2) {
					lcd_extern_init_reg_check2(edev, edev->i2c_dev[1]->client,
						temp, &table[i + 4], (size - 2));
				} else {
					lcd_extern_init_reg_check(edev, edev->i2c_dev[1]->client,
						temp, &table[i + 4], (size - 2));
				}
				if (edev->check_state[1] == 1)
					goto power_cmd_dynamic_i2c_next;
				ret = lcd_extern_i2c_write(edev->i2c_dev[1]->client,
							&table[i + 4], (size - 2));
			} else if (type == LCD_EXT_CMD_TYPE_CMD3_MULTI) {
				if (!edev->i2c_dev[2]) {
					EXTERR("[%d]: dev_%d invalid i2c2 device\n",
					       edrv->index, edev->dev_index);
					return -1;
				}
				ret = lcd_extern_cmd_multi_id(edrv, edev, table[i + 2]);
				if (ret)
					goto power_cmd_dynamic_i2c_next;
				temp = (table[i + 3] << 4) | (2 << 0);
				if (temp == LCD_EXT_CMD_TYPE_CMD3_BIN2) {
					lcd_extern_init_reg_check2(edev, edev->i2c_dev[2]->client,
						temp, &table[i + 4], (size - 2));
				} else {
					lcd_extern_init_reg_check(edev, edev->i2c_dev[2]->client,
						temp, &table[i + 4], (size - 2));
				}
				if (edev->check_state[2] == 1)
					goto power_cmd_dynamic_i2c_next;
				ret = lcd_extern_i2c_write(edev->i2c_dev[2]->client,
							&table[i + 4], (size - 2));
			} else if (type == LCD_EXT_CMD_TYPE_CMD4_MULTI) {
				if (!edev->i2c_dev[3]) {
					EXTERR("[%d]: dev_%d invalid i2c3 device\n",
					       edrv->index, edev->dev_index);
					return -1;
				}
				ret = lcd_extern_cmd_multi_id(edrv, edev, table[i + 2]);
				if (ret)
					goto power_cmd_dynamic_i2c_next;
				temp = (table[i + 3] << 4) | (3 << 0);
				if (temp == LCD_EXT_CMD_TYPE_CMD4_BIN2) {
					lcd_extern_init_reg_check2(edev, edev->i2c_dev[2]->client,
						temp, &table[i + 4], (size - 2));
				} else {
					lcd_extern_init_reg_check(edev, edev->i2c_dev[3]->client,
						temp, &table[i + 4], (size - 2));
				}
				if (edev->check_state[3] == 1)
					goto power_cmd_dynamic_i2c_next;
				ret = lcd_extern_i2c_write(edev->i2c_dev[3]->client,
							&table[i + 4], (size - 2));
			} else if (type == LCD_EXT_CMD_TYPE_CMD_DELAY) {
				if (!edev->i2c_dev[0]) {
					EXTERR("[%d]: dev_%d invalid i2c0 device\n",
					       edrv->index, edev->dev_index);
					return -1;
				}
				ret = lcd_extern_i2c_write(edev->i2c_dev[0]->client,
							   &table[i + 2], (size - 1));
				if (table[i + size + 1] > 0)
					lcd_delay_ms(table[i + size + 1]);
			} else if (type == LCD_EXT_CMD_TYPE_CMD2_DELAY) {
				if (!edev->i2c_dev[1]) {
					EXTERR("[%d]: dev_%d invalid i2c1 device\n",
					       edrv->index, edev->dev_index);
					return -1;
				}
				ret = lcd_extern_i2c_write(edev->i2c_dev[1]->client,
							   &table[i + 2], (size - 1));
				if (table[i + size + 1] > 0)
					lcd_delay_ms(table[i + size + 1]);
			} else if (type == LCD_EXT_CMD_TYPE_CMD3_DELAY) {
				if (!edev->i2c_dev[2]) {
					EXTERR("[%d]: dev_%d invalid i2c2 device\n",
					       edrv->index, edev->dev_index);
					return -1;
				}
				ret = lcd_extern_i2c_write(edev->i2c_dev[2]->client,
							   &table[i + 2], (size - 1));
				if (table[i + size + 1] > 0)
					lcd_delay_ms(table[i + size + 1]);
			} else if (type == LCD_EXT_CMD_TYPE_CMD4_DELAY) {
				if (!edev->i2c_dev[3]) {
					EXTERR("[%d]: dev_%d invalid i2c3 device\n",
					       edrv->index, edev->dev_index);
					return -1;
				}
				ret = lcd_extern_i2c_write(edev->i2c_dev[3]->client,
							   &table[i + 2], (size - 1));
				if (table[i + size + 1] > 0)
					lcd_delay_ms(table[i + size + 1]);
			} else {
				EXTERR("[%d]: %s: %s(%d): type 0x%02x invalid\n",
				       edrv->index, __func__,
				       edev->config.name, edev->config.index, type);
			}
power_cmd_dynamic_i2c_next:
			i += (size + 2);
			step++;
		}
		break;
	case LCD_EXTERN_SPI:
		while ((i + 1) < max_len) {
			type = table[i];
			size = table[i + 1];
			if (type == LCD_EXT_CMD_TYPE_END)
				break;
			if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
				EXTPR("[%d]: %s: dev_%d step %d: type=0x%02x, size=%d\n",
				      edrv->index, __func__,
				      edev->dev_index, step, type, size);
			}
			if (size == 0)
				goto power_cmd_dynamic_spi_next;
			if ((i + 2 + size) > max_len)
				break;

			if (type == LCD_EXT_CMD_TYPE_NONE) {
				/* do nothingy */
			} else if (type == LCD_EXT_CMD_TYPE_GPIO) {
				if (size < 2) {
					EXTERR("[%d]: dev_%d step %d: invalid size %d for GPIO\n",
					       edrv->index, edev->dev_index, step, size);
					goto power_cmd_dynamic_spi_next;
				}
				if (table[i + 2] < LCD_GPIO_MAX)
					lcd_extern_gpio_set(edrv, table[i + 2], table[i + 3]);
				if (size > 2) {
					if (table[i + 4] > 0)
						lcd_delay_ms(table[i + 4]);
				}
			} else if (type == LCD_EXT_CMD_TYPE_DELAY) {
				delay_ms = 0;
				for (j = 0; j < size; j++)
					delay_ms += table[i + 2 + j];
				if (delay_ms > 0)
					lcd_delay_ms(delay_ms);
			} else if ((type == LCD_EXT_CMD_TYPE_CMD) ||
				   (type == LCD_EXT_CMD_TYPE_CMD_BIN)) {
				ret = lcd_extern_spi_write(edrv, edev, &table[i + 2], size);
			} else if (type == LCD_EXT_CMD_TYPE_CMD_DELAY) {
				ret = lcd_extern_spi_write(edrv, edev, &table[i + 2], (size - 1));
				if (table[i + size + 1] > 0)
					lcd_delay_ms(table[i + size + 1]);
			} else {
				EXTERR("[%d]: %s: %s(%d): type 0x%02x invalid\n",
				       edrv->index, __func__,
				       edev->config.name, edev->config.index, type);
			}
power_cmd_dynamic_spi_next:
			i += (size + 2);
			step++;
		}
		break;
	default:
		EXTERR("[%d]: %s: %s(%d): extern_type %d is not support\n",
		       edrv->index, __func__,
		       edev->config.name, edev->config.index, edev->config.type);
		break;
	}

	return ret;
}

static int lcd_extern_power_cmd_fixed_size(struct lcd_extern_driver_s *edrv,
					   struct lcd_extern_dev_s *edev,
					   unsigned char *table, int flag)
{
	int i = 0, j, step = 0, max_len = 0;
	unsigned char type, cmd_size;
	int delay_ms, ret = 0;

	cmd_size = edev->config.cmd_size;
	if (cmd_size < 2) {
		EXTERR("[%d]: %s: dev_%d invalid cmd_size %d\n",
		       edrv->index, __func__, edev->dev_index, cmd_size);
		return -1;
	}

	if (flag)
		max_len = edev->config.table_init_on_cnt;
	else
		max_len = edev->config.table_init_off_cnt;

	switch (edev->config.type) {
	case LCD_EXTERN_I2C:
		while ((i + cmd_size) <= max_len) {
			type = table[i];
			if (type == LCD_EXT_CMD_TYPE_END)
				break;
			if (lcd_debug_print_flag) {
				EXTPR("[%d]: %s: dev_%d step %d: type=0x%02x, cmd_size=%d\n",
				      edrv->index, __func__,
				      edev->dev_index, step, type, cmd_size);
			}
			if (type == LCD_EXT_CMD_TYPE_NONE) {
				/* do nothing */
			} else if (type == LCD_EXT_CMD_TYPE_GPIO) {
				if (table[i + 1] < LCD_GPIO_MAX)
					lcd_extern_gpio_set(edrv, table[i + 1], table[i + 2]);
				if (cmd_size > 3) {
					if (table[i + 3] > 0)
						lcd_delay_ms(table[i + 3]);
				}
			} else if (type == LCD_EXT_CMD_TYPE_DELAY) {
				delay_ms = 0;
				for (j = 0; j < (cmd_size - 1); j++)
					delay_ms += table[i + 1 + j];
				if (delay_ms > 0)
					lcd_delay_ms(delay_ms);
			} else if ((type == LCD_EXT_CMD_TYPE_CMD) ||
				   (type == LCD_EXT_CMD_TYPE_CMD_BIN) ||
				   (type == LCD_EXT_CMD_TYPE_CMD_BIN2) ||
				   (type == LCD_EXT_CMD_TYPE_CMD_BIN_DATA)) {
				if (!edev->i2c_dev[0]) {
					EXTERR("[%d]: dev_%d invalid i2c0 device\n",
					       edrv->index, edev->dev_index);
					return -1;
				}
				ret = lcd_extern_i2c_write(edev->i2c_dev[0]->client,
							   &table[i + 1], (cmd_size - 1));
			} else if ((type == LCD_EXT_CMD_TYPE_CMD2) ||
				   (type == LCD_EXT_CMD_TYPE_CMD2_BIN) ||
				   (type == LCD_EXT_CMD_TYPE_CMD2_BIN2) ||
				   (type == LCD_EXT_CMD_TYPE_CMD2_BIN_DATA)) {
				if (!edev->i2c_dev[1]) {
					EXTERR("[%d]: dev_%d invalid i2c1 device\n",
					       edrv->index, edev->dev_index);
					return -1;
				}
				ret = lcd_extern_i2c_write(edev->i2c_dev[1]->client,
							   &table[i + 1], (cmd_size - 1));
			} else if ((type == LCD_EXT_CMD_TYPE_CMD3) ||
				   (type == LCD_EXT_CMD_TYPE_CMD3_BIN) ||
				   (type == LCD_EXT_CMD_TYPE_CMD3_BIN2) ||
				   (type == LCD_EXT_CMD_TYPE_CMD3_BIN_DATA)) {
				if (!edev->i2c_dev[2]) {
					EXTERR("[%d]: dev_%d invalid i2c2 device\n",
					       edrv->index, edev->dev_index);
					return -1;
				}
				ret = lcd_extern_i2c_write(edev->i2c_dev[2]->client,
							   &table[i + 1], (cmd_size - 1));
			} else if ((type == LCD_EXT_CMD_TYPE_CMD4) ||
				   (type == LCD_EXT_CMD_TYPE_CMD4_BIN) ||
				   (type == LCD_EXT_CMD_TYPE_CMD4_BIN2) ||
				   (type == LCD_EXT_CMD_TYPE_CMD4_BIN_DATA)) {
				if (!edev->i2c_dev[3]) {
					EXTERR("[%d]: dev_%d invalid i2c3 device\n",
					       edrv->index, edev->dev_index);
					return -1;
				}
				ret = lcd_extern_i2c_write(edev->i2c_dev[3]->client,
							   &table[i + 1], (cmd_size - 1));
			} else if (type == LCD_EXT_CMD_TYPE_CMD_DELAY) {
				if (!edev->i2c_dev[0]) {
					EXTERR("[%d]: dev_%d invalid i2c0 device\n",
					       edrv->index, edev->dev_index);
					return -1;
				}
				ret = lcd_extern_i2c_write(edev->i2c_dev[0]->client,
							   &table[i + 1], (cmd_size - 2));
				if (table[i + cmd_size - 1] > 0)
					lcd_delay_ms(table[i + cmd_size - 1]);
			} else if (type == LCD_EXT_CMD_TYPE_CMD2_DELAY) {
				if (!edev->i2c_dev[1]) {
					EXTERR("[%d]: dev_%d invalid i2c1 device\n",
					       edrv->index, edev->dev_index);
					return -1;
				}
				ret = lcd_extern_i2c_write(edev->i2c_dev[1]->client,
							   &table[i + 1], (cmd_size - 2));
				if (table[i + cmd_size - 1] > 0)
					lcd_delay_ms(table[i + cmd_size - 1]);
			} else if (type == LCD_EXT_CMD_TYPE_CMD3_DELAY) {
				if (!edev->i2c_dev[2]) {
					EXTERR("[%d]: dev_%d invalid i2c2 device\n",
					       edrv->index, edev->dev_index);
					return -1;
				}
				ret = lcd_extern_i2c_write(edev->i2c_dev[2]->client,
							   &table[i + 1], (cmd_size - 2));
				if (table[i + cmd_size - 1] > 0)
					lcd_delay_ms(table[i + cmd_size - 1]);
			} else if (type == LCD_EXT_CMD_TYPE_CMD4_DELAY) {
				if (!edev->i2c_dev[3]) {
					EXTERR("[%d]: dev_%d invalid i2c3 device\n",
					       edrv->index, edev->dev_index);
					return -1;
				}
				ret = lcd_extern_i2c_write(edev->i2c_dev[3]->client,
							   &table[i + 1], (cmd_size - 2));
				if (table[i + cmd_size - 1] > 0)
					lcd_delay_ms(table[i + cmd_size - 1]);
			} else {
				EXTERR("[%d]: %s: %s(%d): type 0x%02x invalid\n",
				       edrv->index, __func__,
				       edev->config.name, edev->config.index, type);
			}
			i += cmd_size;
			step++;
		}
		break;
	case LCD_EXTERN_SPI:
		while ((i + cmd_size) <= max_len) {
			type = table[i];
			if (type == LCD_EXT_CMD_TYPE_END)
				break;
			if (lcd_debug_print_flag) {
				EXTPR("[%d]: %s: dev_%d step %d: type=0x%02x, cmd_size=%d\n",
				      edrv->index, __func__,
				      edev->dev_index, step, type, cmd_size);
			}
			if (type == LCD_EXT_CMD_TYPE_NONE) {
				/* do nothing */
			} else if (type == LCD_EXT_CMD_TYPE_GPIO) {
				if (table[i + 1] < LCD_GPIO_MAX)
					lcd_extern_gpio_set(edrv, table[i + 1], table[i + 2]);
				if (cmd_size > 3) {
					if (table[i + 3] > 0)
						lcd_delay_ms(table[i + 3]);
				}
			} else if (type == LCD_EXT_CMD_TYPE_DELAY) {
				delay_ms = 0;
				for (j = 0; j < (cmd_size - 1); j++)
					delay_ms += table[i + 1 + j];
				if (delay_ms > 0)
					lcd_delay_ms(delay_ms);
			} else if (type == LCD_EXT_CMD_TYPE_CMD) {
				ret = lcd_extern_spi_write(edrv, edev,
							   &table[i + 1], (cmd_size - 1));
			} else if (type == LCD_EXT_CMD_TYPE_CMD_DELAY) {
				ret = lcd_extern_spi_write(edrv, edev,
							   &table[i + 1], (cmd_size - 2));
				if (table[i + cmd_size - 1] > 0)
					lcd_delay_ms(table[i + cmd_size - 1]);
			} else {
				EXTERR("[%d]: %s: %s(%d): type 0x%02x invalid\n",
				       edrv->index, __func__,
				       edev->config.name, edev->config.index, type);
			}
			i += cmd_size;
			step++;
		}
		break;
	default:
		EXTERR("[%d]: %s: %s(%d): extern_type %d is not support\n",
		       edrv->index, __func__,
		       edev->config.name, edev->config.index, edev->config.type);
		break;
	}

	return ret;
}

static int lcd_extern_power_ctrl(struct lcd_extern_driver_s *edrv,
				 struct lcd_extern_dev_s *edev, int flag)
{
	unsigned char *table;
	unsigned char cmd_size;
	int ret = 0;

	if (edev->config.type == LCD_EXTERN_SPI)
		spi_gpio_init(edrv, edev);

	cmd_size = edev->config.cmd_size;
	if (flag)
		table = edev->config.table_init_on;
	else
		table = edev->config.table_init_off;
	if (cmd_size < 1) {
		EXTERR("[%d]: %s: dev_%d cmd_size %d is invalid\n",
		       edrv->index, __func__, edev->dev_index, cmd_size);
		return -1;
	}
	if (!table) {
		EXTERR("[%d]: %s: dev_%d init_table %d is NULL\n",
		       edrv->index, __func__, edev->dev_index, flag);
		return -1;
	}

	if (cmd_size == LCD_EXT_CMD_SIZE_DYNAMIC)
		ret = lcd_extern_power_cmd_dynamic_size(edrv, edev, table, flag);
	else
		ret = lcd_extern_power_cmd_fixed_size(edrv, edev, table, flag);

	if (edev->config.type == LCD_EXTERN_SPI)
		spi_gpio_off(edrv, edev);

	EXTPR("[%d]: %s: %s(%d): %d\n",
	      edrv->index, __func__,
	      edev->config.name, edev->config.index, flag);
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
		EXTERR("[%d]: dev_%d(%s): tablet_init is invalid\n",
		       edrv->index, edev->dev_index, edev->config.name);
		return -1;
	}

	if (edev->config.type == LCD_EXTERN_SPI)
		edev->config.spi_delay_us = 1000 / edev->config.spi_clk_freq;

	edev->reg_read  = lcd_extern_reg_read;
	edev->reg_write = lcd_extern_reg_write;
	edev->power_on  = lcd_extern_power_on;
	edev->power_off = lcd_extern_power_off;

	return 0;
}

int lcd_extern_default_probe(struct lcd_extern_driver_s *edrv, struct lcd_extern_dev_s *edev)
{
	int ret = 0;

	if (!edrv || !edev) {
		EXTERR("%s: dev is null\n", __func__);
		return -1;
	}

	if (edev->config.cmd_size < 2) {
		EXTERR("[%d]: %s: %s(%d): cmd_size %d is invalid\n",
			edrv->index, __func__,
			edev->config.name,
			edev->dev_index,
			edev->config.cmd_size);
		return -1;
	}

	switch (edev->config.type) {
	case LCD_EXTERN_I2C:
		if (edev->config.i2c_addr < LCD_EXT_I2C_ADDR_INVALID) {
			edev->i2c_dev[0] = lcd_extern_get_i2c_device(edev->config.i2c_addr);
			if (!edev->i2c_dev[0]) {
				EXTERR("[%d]: %s: dev_%d invalid i2c0 device\n",
				       edrv->index, __func__, edev->dev_index);
				return -1;
			}
			EXTPR("[%d]: %s: dev_%d get i2c0 device: %s, addr 0x%02x OK\n",
			      edrv->index, __func__, edev->dev_index,
			      edev->i2c_dev[0]->name, edev->i2c_dev[0]->client->addr);
		}
		if (edev->config.i2c_addr2 < LCD_EXT_I2C_ADDR_INVALID) {
			edev->i2c_dev[1] = lcd_extern_get_i2c_device(edev->config.i2c_addr2);
			if (!edev->i2c_dev[1]) {
				EXTERR("[%d]: %s: dev_%d invalid i2c1 device\n",
				       edrv->index, __func__, edev->dev_index);
				edev->i2c_dev[0] = NULL;
				return -1;
			}
			EXTPR("[%d]: %s: dev_%d get i2c1 device: %s, addr 0x%02x OK\n",
			      edrv->index, __func__, edev->dev_index,
			      edev->i2c_dev[1]->name, edev->i2c_dev[1]->client->addr);
		}
		if (edev->config.i2c_addr3 < LCD_EXT_I2C_ADDR_INVALID) {
			edev->i2c_dev[2] = lcd_extern_get_i2c_device(edev->config.i2c_addr3);
			if (!edev->i2c_dev[2]) {
				EXTERR("[%d]: %s: dev_%d invalid i2c2 device\n",
				       edrv->index, __func__, edev->dev_index);
				edev->i2c_dev[0] = NULL;
				edev->i2c_dev[1] = NULL;
				return -1;
			}
			EXTPR("[%d]: %s: dev_%d get i2c2 device: %s, addr 0x%02x OK\n",
			      edrv->index, __func__, edev->dev_index,
			      edev->i2c_dev[2]->name, edev->i2c_dev[2]->client->addr);
		}
		if (edev->config.i2c_addr4 < LCD_EXT_I2C_ADDR_INVALID) {
			edev->i2c_dev[3] = lcd_extern_get_i2c_device(edev->config.i2c_addr4);
			if (!edev->i2c_dev[3]) {
				EXTERR("[%d]: %s: dev_%d invalid i2c3 device\n",
				       edrv->index, __func__, edev->dev_index);
				edev->i2c_dev[0] = NULL;
				edev->i2c_dev[1] = NULL;
				edev->i2c_dev[2] = NULL;
				return -1;
			}
			EXTPR("[%d]: %s: dev_%d get i2c3 device: %s, addr 0x%02x OK\n",
			      edrv->index, __func__, edev->dev_index,
			      edev->i2c_dev[3]->name, edev->i2c_dev[3]->client->addr);
		}
		break;
	case LCD_EXTERN_SPI:
		break;
	default:
		break;
	}

	ret = lcd_extern_driver_update(edrv, edev);
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
		EXTPR("[%d]: %s: dev_%d %d\n",
		      edrv->index, __func__, edev->dev_index, ret);
	}
	return ret;
}
