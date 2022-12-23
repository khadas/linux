// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 *
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/i2c.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/compat.h>
#include <linux/of.h>
#include <linux/amlogic/media/vout/lcd/lcd_extern.h>
#include <linux/amlogic/media/vout/lcd/lcd_unifykey.h>
#include <linux/amlogic/media/vout/lcd/lcd_notify.h>
#include "lcd_extern.h"

#include <linux/amlogic/gki_module.h>

#define EXT_CDEV_NAME "lcd_ext"
struct ext_cdev_s {
	dev_t           devno;
	struct class    *class;
};

static struct ext_cdev_s *ext_cdev;
/* for driver global resource init:
 *  0: none
 *  n: initialized cnt
 */
static unsigned char ext_global_init_flag;
static unsigned int ext_drv_init_state;

static int lcd_ext_dev_cnt[LCD_MAX_DRV];
static int lcd_ext_index_lut[LCD_MAX_DRV][LCD_EXTERN_DEV_MAX];
static struct lcd_extern_driver_s *ext_driver[LCD_MAX_DRV];

struct lcd_extern_driver_s *lcd_extern_get_driver(int drv_index)
{
	if (drv_index >= LCD_MAX_DRV)
		return NULL;

	return ext_driver[drv_index];
}

struct lcd_extern_dev_s *lcd_extern_get_dev(struct lcd_extern_driver_s *edrv, int dev_index)
{
	int i = 0;

	if (!edrv)
		return NULL;
	if (dev_index >= LCD_EXTERN_INDEX_INVALID)
		return NULL;

	for (i = 0; i < edrv->dev_cnt; i++) {
		if (!edrv->dev[i])
			break;

		if (lcd_debug_print_flag & LCD_DBG_PR_ADV) {
			EXTPR("%s: dev[%d]: name: %s, dev_index:%d, get dev_index:%d\n",
				__func__, i,
				edrv->dev[i]->config.name,
				edrv->dev[i]->dev_index,
				dev_index);
		}
		if (edrv->dev[i]->dev_index == dev_index)
			return edrv->dev[i];
	}

	EXTERR("[%d]: invalid dev_index: %d\n", edrv->index, dev_index);
	return NULL;
}

int lcd_extern_dev_index_add(int drv_index, int dev_index)
{
	int dev_cnt, i;

	if (drv_index >= LCD_MAX_DRV) {
		EXTERR("%s: invalid drv_index: %d\n", __func__, drv_index);
		return -1;
	}
	if (dev_index == 0xff)
		return 0;

	dev_cnt = lcd_ext_dev_cnt[drv_index];
	if (dev_cnt >= LCD_EXTERN_DEV_MAX) {
		EXTERR("[%d]: %s: out off dev_cnt support\n", drv_index, __func__);
		return -1;
	}

	for (i = 0; i < LCD_EXTERN_DEV_MAX; i++) {
		if (lcd_ext_index_lut[drv_index][i] == dev_index) {
			if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
				EXTPR("[%d]: %s: dev_index %d already exist\n",
				      drv_index, __func__, dev_index);
			}
			return 0;
		}
	}

	lcd_ext_index_lut[drv_index][dev_cnt] = dev_index;
	lcd_ext_dev_cnt[drv_index]++;
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
		EXTPR("[%d]: %s: dev_index: %d, dev_cnt: %d\n",
		      drv_index, __func__, dev_index,
		      lcd_ext_dev_cnt[drv_index]);
	}
	return 0;
}

int lcd_extern_dev_index_remove(int drv_index, int dev_index)
{
	int find, i;

	if (drv_index >= LCD_MAX_DRV) {
		EXTERR("%s: invalid drv_index: %d\n", __func__, dev_index);
		return -1;
	}
	if (dev_index == 0xff)
		return 0;

	if (lcd_ext_dev_cnt[drv_index] == 0)
		return -1;

	find = 0xff;
	for (i = 0; i < LCD_EXTERN_DEV_MAX; i++) {
		if (lcd_ext_index_lut[drv_index][i] == dev_index)
			find = i;
	}
	if (find == 0xff)
		return 0;

	lcd_ext_index_lut[drv_index][find] = LCD_EXTERN_INDEX_INVALID;
	for (i = (find + 1); i < LCD_EXTERN_DEV_MAX; i++) {
		if (lcd_ext_index_lut[drv_index][i] == LCD_EXTERN_INDEX_INVALID)
			break;
		lcd_ext_index_lut[drv_index][i - 1] = lcd_ext_index_lut[drv_index][i];
		lcd_ext_index_lut[drv_index][i] = LCD_EXTERN_INDEX_INVALID;
	}
	if (lcd_ext_dev_cnt[drv_index])
		lcd_ext_dev_cnt[drv_index]--;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		EXTPR("[%d]: %s: dev_index: %d\n", drv_index, __func__, dev_index);
	return 0;
}

int lcd_extern_init(void)
{
	int i, j;

	for (i = 0; i < LCD_MAX_DRV; i++) {
		for (j = 0; j < LCD_EXTERN_DEV_MAX; j++)
			lcd_ext_index_lut[i][j] = LCD_EXTERN_INDEX_INVALID;
		lcd_ext_dev_cnt[i] = 0;
	}

	return 0;
}

static void lcd_extern_gpio_probe(struct lcd_extern_driver_s *edrv, unsigned char index)
{
	struct lcd_ext_gpio_s *ext_gpio;
	const char *str;
	int ret;

	if (index >= LCD_EXTERN_GPIO_NUM_MAX) {
		EXTERR("[%d]: %s: invalid gpio index %d, exit\n",
		       edrv->index, __func__, index);
		return;
	}
	ext_gpio = &edrv->gpio[index];
	if (ext_gpio->probe_flag) {
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			EXTPR("[%d]: %s: gpio %s[%d] is already probed\n",
			      edrv->index, __func__, ext_gpio->name, index);
		}
		return;
	}

	/* get gpio name */
	ret = of_property_read_string_index(edrv->sub_dev->of_node,
					    "extern_gpio_names", index, &str);
	if (ret) {
		EXTERR("[%d]: %s: failed to get extern_gpio_names: %d\n",
		       edrv->index, __func__, index);
		str = "unknown";
	}
	strcpy(ext_gpio->name, str);

	/* init gpio flag */
	ext_gpio->probe_flag = 1;
	ext_gpio->register_flag = 0;
}

void lcd_extern_gpio_unregister(struct lcd_extern_driver_s *edrv, int index)
{
	struct lcd_ext_gpio_s *ext_gpio;

	if (index >= LCD_EXTERN_GPIO_NUM_MAX) {
		EXTERR("[%d]: %s: invalid gpio index %d, exit\n",
		       edrv->index, __func__, index);
		return;
	}
	ext_gpio = &edrv->gpio[index];
	if (ext_gpio->probe_flag == 0) {
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			EXTPR("[%d]: %s: gpio [%d] is not probed, exit\n",
			      edrv->index, __func__, index);
		}
		return;
	}
	if (ext_gpio->register_flag == 0) {
		EXTPR("[%d]: %s: gpio %s[%d] is already unregistered\n",
		      edrv->index, __func__, ext_gpio->name, index);
		return;
	}
	if (IS_ERR(ext_gpio->gpio)) {
		EXTERR("[%d]: %s: gpio %s[%d]: %p, err: %d\n",
		       edrv->index, __func__, ext_gpio->name, index,
		       ext_gpio->gpio, IS_ERR(ext_gpio->gpio));
		ext_gpio->gpio = NULL;
		return;
	}

	/* release gpio */
	devm_gpiod_put(edrv->sub_dev, ext_gpio->gpio);
	ext_gpio->register_flag = 0;
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
		EXTPR("[%d]: %s: release gpio %s[%d]\n",
		      edrv->index, __func__, ext_gpio->name, index);
	}
}

static int lcd_extern_gpio_register(struct lcd_extern_driver_s *edrv,
				    unsigned char index, int init_value)
{
	struct lcd_ext_gpio_s *ext_gpio;
	int value;

	if (index >= LCD_EXTERN_GPIO_NUM_MAX) {
		EXTERR("[%d]: %s: invalid gpio [%d], exit\n",
		       edrv->index, __func__, index);
		return -1;
	}
	ext_gpio = &edrv->gpio[index];
	if (ext_gpio->probe_flag == 0) {
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			EXTPR("[%d]: %s: gpio [%d] is not probed, exit\n",
			      edrv->index, __func__, index);
		}
		return -1;
	}
	if (ext_gpio->register_flag) {
		EXTPR("[%d]: %s: gpio %s[%d] is already registered\n",
		      edrv->index, __func__, ext_gpio->name, index);
		return 0;
	}

	switch (init_value) {
	case LCD_GPIO_OUTPUT_LOW:
		value = GPIOD_OUT_LOW;
		break;
	case LCD_GPIO_OUTPUT_HIGH:
		value = GPIOD_OUT_HIGH;
		break;
	case LCD_GPIO_INPUT:
	default:
		value = GPIOD_IN;
		break;
	}

	/* request gpio */
	ext_gpio->gpio = devm_gpiod_get_index(edrv->sub_dev, "extern", index, value);
	if (IS_ERR(ext_gpio->gpio)) {
		EXTERR("[%d]: %s: gpio %s[%d]: %p, err: %d\n",
		       edrv->index, __func__, ext_gpio->name, index, ext_gpio->gpio,
		       IS_ERR(ext_gpio->gpio));
		ext_gpio->gpio = NULL;
		return -1;
	}
	ext_gpio->register_flag = 1;
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
		EXTPR("[%d]: %s: gpio %s[%d]: %p, init value: %d\n",
		      edrv->index, __func__, ext_gpio->name, index,
		      ext_gpio->gpio, init_value);
	}

	return 0;
}

void lcd_extern_gpio_set(struct lcd_extern_driver_s *edrv, unsigned char index, int value)
{
	struct lcd_ext_gpio_s *ext_gpio;

	if (index >= LCD_EXTERN_GPIO_NUM_MAX) {
		EXTERR("[%d]: %s: invalid gpio [%d], exit\n",
		       edrv->index, __func__, index);
		return;
	}
	ext_gpio = &edrv->gpio[index];
	if (ext_gpio->probe_flag == 0) {
		EXTPR("[%d]: %s: gpio [%d] is not probed, exit\n",
		      edrv->index, __func__, index);
		return;
	}
	if (ext_gpio->register_flag == 0) {
		lcd_extern_gpio_register(edrv, index, value);
		return;
	}

	if (IS_ERR_OR_NULL(ext_gpio->gpio)) {
		EXTERR("[%d]: %s: gpio %s[%d]: %p, err: %ld\n",
		       edrv->index, __func__, ext_gpio->name, index,
		       ext_gpio->gpio, PTR_ERR(ext_gpio->gpio));
		return;
	}

	switch (value) {
	case LCD_GPIO_OUTPUT_LOW:
	case LCD_GPIO_OUTPUT_HIGH:
		gpiod_direction_output(ext_gpio->gpio, value);
		break;
	case LCD_GPIO_INPUT:
	default:
		gpiod_direction_input(ext_gpio->gpio);
		break;
	}
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
		EXTPR("[%d]: %s: set gpio %s[%d] value: %d\n",
		      edrv->index, __func__, ext_gpio->name, index, value);
	}
}

unsigned int lcd_extern_gpio_get(struct lcd_extern_driver_s *edrv, unsigned char index)
{
	struct lcd_ext_gpio_s *ext_gpio;

	if (index >= LCD_EXTERN_GPIO_NUM_MAX) {
		EXTERR("[%d]: %s: invalid gpio [%d], exit\n",
		       edrv->index, __func__, index);
		return -1;
	}
	ext_gpio = &edrv->gpio[index];
	if (ext_gpio->probe_flag == 0) {
		EXTPR("[%d]: %s: gpio [%d] is not probed, exit\n",
		      edrv->index, __func__, index);
		return -1;
	}
	if (ext_gpio->register_flag == 0) {
		EXTERR("[%d]: %s: gpio %s[%d] is not registered, exit\n",
		       edrv->index, __func__, ext_gpio->name, index);
		return -1;
	}
	if (IS_ERR_OR_NULL(ext_gpio->gpio)) {
		EXTERR("[%d]: %s: gpio %s[%d]: %p, err: %ld\n",
		       edrv->index, __func__, ext_gpio->name, index,
		       ext_gpio->gpio, PTR_ERR(ext_gpio->gpio));
		return -1;
	}

	return gpiod_get_value(ext_gpio->gpio);
}

#define LCD_EXTERN_PINMUX_MAX    3
static char *lcd_extern_pinmux_str[LCD_EXTERN_PINMUX_MAX] = {
	"extern_on",   /* 0 */
	"extern_off",  /* 1 */
	"none",
};

void lcd_extern_pinmux_set(struct lcd_extern_driver_s *edrv, int status)
{
	int index = 0xff;

	if (edrv->pinmux_valid == 0) {
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			EXTPR("[%d]: %s: pinmux invalid, bypass\n",
			      edrv->index, __func__);
		return;
	}

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		EXTPR("[%d]: %s: %d\n", edrv->index, __func__, status);

	index = (status) ? 0 : 1;
	if (edrv->pinmux_flag == index) {
		EXTPR("[%d]: %s: pinmux %s is already selected\n",
		      edrv->index, __func__, lcd_extern_pinmux_str[index]);
		return;
	}

	/* request pinmux */
	edrv->pin = devm_pinctrl_get_select(edrv->sub_dev, lcd_extern_pinmux_str[index]);
	if (IS_ERR(edrv->pin)) {
		EXTERR("[%d]: %s: set pinmux %s error\n",
		       edrv->index, __func__, lcd_extern_pinmux_str[index]);
	} else {
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			EXTPR("[%d]: %s: set pinmux %s ok\n",
			      edrv->index, __func__, lcd_extern_pinmux_str[index]);
		}
	}
	edrv->pinmux_flag = index;
}

#ifdef CONFIG_OF
struct device_node *aml_lcd_extern_get_dts_child(struct lcd_extern_driver_s *edrv, int index)
{
	char propname[15];
	struct device_node *child;

	sprintf(propname, "extern_%d", index);
	child = of_get_child_by_name(edrv->sub_dev->of_node, propname);
	return child;
}

static int lcd_extern_init_table_dynamic_dts(struct lcd_extern_driver_s *edrv,
					     struct device_node *np,
					     struct lcd_extern_config_s *econf,
					     int flag)
{
	unsigned char cmd_size, type;
	int i = 0, j, val, max_len, step = 0, ret = 0;
	unsigned char *table;
	char propname[20];

	if (flag) {
		max_len = LCD_EXTERN_INIT_ON_MAX;
		sprintf(propname, "init_on");
	} else {
		max_len = LCD_EXTERN_INIT_OFF_MAX;
		sprintf(propname, "init_off");
	}
	table = kcalloc(max_len, sizeof(unsigned char), GFP_KERNEL);
	if (!table)
		return -1;
	table[0] = LCD_EXT_CMD_TYPE_END;
	table[1] = 0;

	switch (econf->type) {
	case LCD_EXTERN_I2C:
	case LCD_EXTERN_SPI:
		while ((i + 1) < max_len) {
			/* type */
			ret = of_property_read_u32_index(np, propname, i, &val);
			if (ret) {
				EXTERR("%s: get %s type failed, step %d\n",
				       econf->name, propname, step);
				table[i] = LCD_EXT_CMD_TYPE_END;
				table[i + 1] = 0;
				goto init_table_dynamic_dts_err;
			}
			table[i] = (unsigned char)val;
			type = table[i];
			/* cmd_size */
			ret = of_property_read_u32_index(np, propname, (i + 1), &val);
			if (ret) {
				EXTERR("%s: get %s cmd_size failed, step %d\n",
				       econf->name, propname, step);
				table[i] = LCD_EXT_CMD_TYPE_END;
				table[i + 1] = 0;
				goto init_table_dynamic_dts_err;
			}
			table[i + 1] = (unsigned char)val;
			cmd_size = table[i + 1];

			if (type == LCD_EXT_CMD_TYPE_END)
				break;
			if (cmd_size == 0)
				goto init_table_dynamic_i2c_spi_dts_next;
			if ((i + 2 + cmd_size) > max_len) {
				EXTERR("%s: %s cmd_size out of support, step %d\n",
				       econf->name, propname, step);
				table[i] = LCD_EXT_CMD_TYPE_END;
				table[i + 1] = 0;
				goto init_table_dynamic_dts_err;
			}

			/* data */
			for (j = 0; j < cmd_size; j++) {
				ret = of_property_read_u32_index(np, propname,
								 (i + 2 + j), &val);
				if (ret) {
					EXTERR("%s: get %s data failed, step %d\n",
					       econf->name, propname, step);
					table[i] = LCD_EXT_CMD_TYPE_END;
					table[i + 1] = 0;
					goto init_table_dynamic_dts_err;
				}
				table[i + 2 + j] = (unsigned char)val;
			}

init_table_dynamic_i2c_spi_dts_next:
			i += (cmd_size + 2);
			step++;
		}
		if (flag)
			econf->table_init_on_cnt = i + 2;
		else
			econf->table_init_off_cnt = i + 2;
		break;
	case LCD_EXTERN_MIPI:
		while ((i + 1) < max_len) {
			/* type */
			ret = of_property_read_u32_index(np, propname, i, &val);
			if (ret) {
				EXTERR("%s: get %s type failed, step %d\n",
				       econf->name, propname, step);
				table[i] = LCD_EXT_CMD_TYPE_END;
				table[i + 1] = 0;
				goto init_table_dynamic_dts_err;
			}
			table[i] = (unsigned char)val;
			type = table[i];
			/* cmd_size */
			ret = of_property_read_u32_index(np, propname, (i + 1), &val);
			if (ret) {
				EXTERR("%s: get %s cmd_size failed, step %d\n",
				       econf->name, propname, step);
				table[i] = LCD_EXT_CMD_TYPE_END;
				table[i + 1] = 0;
				goto init_table_dynamic_dts_err;
			}
			table[i + 1] = (unsigned char)val;
			cmd_size = table[i + 1];

			if (type == LCD_EXT_CMD_TYPE_END) {
				if (cmd_size == 0xff || cmd_size == 0)
					break;
				cmd_size = 0;
			}
			if (cmd_size == 0)
				goto init_table_dynamic_mipi_dts_next;
			if ((i + 2 + cmd_size) > max_len) {
				EXTERR("%s: %s cmd_size out of support, step %d\n",
				       econf->name, propname, step);
				table[i] = LCD_EXT_CMD_TYPE_END;
				table[i + 1] = 0;
				goto init_table_dynamic_dts_err;
			}

			for (j = 0; j < cmd_size; j++) {
				ret = of_property_read_u32_index(np, propname,
								 (i + 2 + j), &val);
				if (ret) {
					EXTERR("%s: get %s failed, step %d\n",
					       econf->name, propname, step);
					table[i] = LCD_EXT_CMD_TYPE_END;
					table[i + 1] = 0;
					goto init_table_dynamic_dts_err;
				}
				table[i + 2 + j] = (unsigned char)val;
			}

init_table_dynamic_mipi_dts_next:
			i += (cmd_size + 2);
			step++;
		}
		if (flag)
			econf->table_init_on_cnt = i + 2;
		else
			econf->table_init_off_cnt = i + 2;
		break;
	default:
		goto init_table_dynamic_dts_end;
	}

	if (flag) {
		econf->table_init_on = kcalloc(econf->table_init_on_cnt,
					       sizeof(unsigned char), GFP_KERNEL);
		if (!econf->table_init_on)
			goto init_table_dynamic_dts_err;
		memcpy(econf->table_init_on, table, econf->table_init_on_cnt);
	} else {
		econf->table_init_off = kcalloc(econf->table_init_off_cnt,
						sizeof(unsigned char), GFP_KERNEL);
		if (!econf->table_init_off)
			goto init_table_dynamic_dts_err;
		memcpy(econf->table_init_off, table, econf->table_init_off_cnt);
	}

init_table_dynamic_dts_end:
	kfree(table);
	return 0;

init_table_dynamic_dts_err:
	kfree(table);
	return -1;
}

static int lcd_extern_init_table_fixed_dts(struct lcd_extern_driver_s *edrv,
					   struct device_node *np,
					   struct lcd_extern_config_s *econf,
					   int flag)
{
	unsigned char cmd_size;
	int i = 0, j, val, max_len, step = 0, ret = 0;
	unsigned char *table;
	char propname[20];

	cmd_size = econf->cmd_size;
	if (flag) {
		max_len = LCD_EXTERN_INIT_ON_MAX;
		sprintf(propname, "init_on");
	} else {
		max_len = LCD_EXTERN_INIT_OFF_MAX;
		sprintf(propname, "init_off");
	}
	table = kcalloc(max_len, sizeof(unsigned char), GFP_KERNEL);
	if (!table)
		return -1;
	table[0] = LCD_EXT_CMD_TYPE_END;
	table[1] = 0;

	while (i < max_len) { /* group detect */
		if ((i + cmd_size) > max_len) {
			EXTERR("%s: %s cmd_size out of support, step %d\n",
			       econf->name, propname, step);
			table[i] = LCD_EXT_CMD_TYPE_END;
			goto init_table_fixed_dts_err;
		}
		for (j = 0; j < cmd_size; j++) {
			ret = of_property_read_u32_index(np, propname, (i + j), &val);
			if (ret) {
				EXTERR("%s: get %s failed, step %d\n",
				       econf->name, propname, step);
				table[i] = LCD_EXT_CMD_TYPE_END;
				goto init_table_fixed_dts_err;
			}
			table[i + j] = (unsigned char)val;
		}
		if (table[i] == LCD_EXT_CMD_TYPE_END)
			break;
		i += cmd_size;
		step++;
	}

	if (flag) {
		econf->table_init_on_cnt = i + cmd_size;
		econf->table_init_on = kcalloc(econf->table_init_on_cnt,
					       sizeof(unsigned char), GFP_KERNEL);
		if (!econf->table_init_on)
			goto init_table_fixed_dts_err;
		memcpy(econf->table_init_on, table, econf->table_init_on_cnt);
	} else {
		econf->table_init_off_cnt = i + cmd_size;
		econf->table_init_off = kcalloc(econf->table_init_off_cnt,
						sizeof(unsigned char), GFP_KERNEL);
		if (!econf->table_init_off)
			goto init_table_fixed_dts_err;
		memcpy(econf->table_init_off, table, econf->table_init_off_cnt);
	}

	kfree(table);
	return 0;

init_table_fixed_dts_err:
	kfree(table);
	return -1;
}

static int lcd_extern_get_config_dts(struct device_node *np,
				     struct lcd_extern_driver_s *edrv,
				     struct lcd_extern_dev_s *edev)
{
	struct lcd_extern_config_s *econf;
	char snode[15];
	struct device_node *child;
	const char *str;
	unsigned int val;
	int ret;

	sprintf(snode, "extern_%d", edev->dev_index);
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		EXTPR("[%d]: %s: %s\n", edrv->index, __func__, snode);

	child = of_get_child_by_name(np, snode);
	if (!child) {
		EXTERR("[%d]: failed to get %s\n", edrv->index, snode);
		return -1;
	}

	econf = &edev->config;

	ret = of_property_read_u32(child, "index", &val);
	if (ret) {
		EXTERR("get index failed, exit\n");
		return -1;
	}
	econf->index = (unsigned char)val;
	if (econf->index == LCD_EXTERN_INDEX_INVALID) {
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			EXTPR("[%d]: dev_index %d is invalid\n", edrv->index, econf->index);
		return -1;
	}

	ret = of_property_read_string(child, "status", &str);
	if (ret) {
		EXTERR("[%d]: get index %d status failed\n", edrv->index, econf->index);
		return -1;
	}
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		EXTPR("[%d]: index %d status = %s\n", edrv->index, econf->index, str);
	if (strncmp(str, "okay", 2) == 0)
		econf->status = 1;
	else
		return -1;

	ret = of_property_read_string(child, "extern_name", &str);
	if (ret) {
		EXTERR("[%d]: get extern_name failed\n", edrv->index);
		strncpy(econf->name, "none", (LCD_EXTERN_NAME_LEN_MAX - 1));
	} else {
		strncpy(econf->name, str, (LCD_EXTERN_NAME_LEN_MAX - 1));
	}
	EXTPR("[%d]: load config: %s[%d]\n", edrv->index, econf->name, econf->index);

	ret = of_property_read_u32(child, "type", &econf->type);
	if (ret) {
		econf->type = LCD_EXTERN_MAX;
		EXTERR("[%d]: %s: get type failed, exit\n", edrv->index, econf->name);
		return -1;
	}

	switch (econf->type) {
	case LCD_EXTERN_I2C:
		ret = of_property_read_u32(child, "i2c_address", &val);
		if (ret) {
			EXTERR("[%d]: %s: get i2c_address failed, exit\n",
			       edrv->index, econf->name);
			econf->i2c_addr = LCD_EXT_I2C_ADDR_INVALID;
			return -1;
		}
		econf->i2c_addr = (unsigned char)val;
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			EXTPR("[%d]: %s: i2c_address = 0x%02x\n",
			      edrv->index, econf->name, econf->i2c_addr);
		}
		ret = of_property_read_u32(child, "i2c_address2", &val);
		if (ret) {
			if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
				EXTPR("[%d]: %s: no i2c_address2 exist\n",
				      edrv->index, econf->name);
			}
			econf->i2c_addr2 = LCD_EXT_I2C_ADDR_INVALID;
		} else {
			econf->i2c_addr2 = (unsigned char)val;
		}
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			EXTPR("[%d]: %s: i2c_address2 = 0x%02x\n",
			      edrv->index, econf->name, econf->i2c_addr2);
		}
		ret = of_property_read_u32(child, "i2c_address3", &val);
		if (ret) {
			if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
				EXTPR("[%d]: %s: no i2c_address3 exist\n",
				      edrv->index, econf->name);
			}
			econf->i2c_addr3 = LCD_EXT_I2C_ADDR_INVALID;
		} else {
			econf->i2c_addr3 = (unsigned char)val;
		}
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			EXTPR("[%d]: %s: i2c_address3 = 0x%02x\n",
			      edrv->index, econf->name, econf->i2c_addr3);
		}
		ret = of_property_read_u32(child, "i2c_address4", &val);
		if (ret) {
			if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
				EXTPR("[%d]: %s: no i2c_address4 exist\n",
				      edrv->index, econf->name);
			}
			econf->i2c_addr4 = LCD_EXT_I2C_ADDR_INVALID;
		} else {
			econf->i2c_addr4 = (unsigned char)val;
		}
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			EXTPR("[%d]: %s: i2c_address4 = 0x%02x\n",
			      edrv->index, econf->name, econf->i2c_addr4);
		}

		ret = of_property_read_u32(child, "cmd_size", &val);
		if (ret) {
			EXTPR("[%d]: %s: no cmd_size\n", edrv->index, econf->name);
			econf->cmd_size = 0;
		} else {
			econf->cmd_size = (unsigned char)val;
		}
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			EXTPR("[%d]: %s: cmd_size = %d\n",
			      edrv->index, econf->name, econf->cmd_size);
		}
		if (econf->cmd_size == 0)
			break;

		if (econf->cmd_size == LCD_EXT_CMD_SIZE_DYNAMIC) {
			ret = lcd_extern_init_table_dynamic_dts(edrv, child, econf, 1);
			if (ret)
				break;
			ret = lcd_extern_init_table_dynamic_dts(edrv, child, econf, 0);
		} else {
			ret = lcd_extern_init_table_fixed_dts(edrv, child, econf, 1);
			if (ret)
				break;
			ret = lcd_extern_init_table_fixed_dts(edrv, child, econf, 0);
		}
		if (ret == 0)
			econf->table_init_loaded = 1;
		break;
	case LCD_EXTERN_SPI:
		ret = of_property_read_u32(child, "gpio_spi_cs", &val);
		if (ret) {
			EXTERR("[%d]: %s: get gpio_spi_cs failed, exit\n",
			       edrv->index, econf->name);
			econf->spi_gpio_cs = LCD_EXT_GPIO_INVALID;
			return -1;
		}
		econf->spi_gpio_cs = val;
		lcd_extern_gpio_probe(edrv, val);
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			EXTPR("[%d]: %s: spi_gpio_cs: %d\n",
			      edrv->index, econf->name, econf->spi_gpio_cs);
		}
		ret = of_property_read_u32(child, "gpio_spi_clk", &val);
		if (ret) {
			EXTERR("[%d]: %s: get gpio_spi_clk failed, exit\n",
			       edrv->index, econf->name);
			econf->spi_gpio_clk = LCD_EXT_GPIO_INVALID;
			return -1;
		}
		econf->spi_gpio_clk = val;
		lcd_extern_gpio_probe(edrv, val);
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			EXTPR("[%d]: %s: spi_gpio_clk: %d\n",
			      edrv->index, econf->name, econf->spi_gpio_clk);
		}
		ret = of_property_read_u32(child, "gpio_spi_data", &val);
		if (ret) {
			EXTERR("[%d]: %s: get gpio_spi_data failed, exit\n",
			       edrv->index, econf->name);
			econf->spi_gpio_data = LCD_EXT_GPIO_INVALID;
			return -1;
		}
		econf->spi_gpio_data = val;
		lcd_extern_gpio_probe(edrv, val);
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			EXTPR("[%d]: %s: spi_gpio_data: %d\n",
			      edrv->index, econf->name, econf->spi_gpio_data);
		}
		ret = of_property_read_u32(child, "spi_clk_freq", &val);
		if (ret) {
			EXTERR("[%d]: %s: get spi_clk_freq failed, default to %dKHz\n",
			       edrv->index, econf->name, LCD_EXT_SPI_CLK_FREQ_DFT);
			econf->spi_clk_freq = LCD_EXT_SPI_CLK_FREQ_DFT;
		} else {
			econf->spi_clk_freq = val;
		}
		ret = of_property_read_u32(child, "spi_clk_pol", &val);
		if (ret) {
			EXTERR("[%d]: %s: get spi_clk_pol failed, default to 1\n",
			       edrv->index, econf->name);
			econf->spi_clk_pol = 1;
		} else {
			econf->spi_clk_pol = (unsigned char)val;
		}
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			EXTPR("[%d]: %s: spi_clk_freq: %dKHz, spi_clk_pol: %d\n",
			      edrv->index, econf->name, econf->spi_clk_freq,
			      econf->spi_clk_pol);
		}
		ret = of_property_read_u32(child, "cmd_size", &val);
		if (ret) {
			EXTPR("[%d]: %s: no cmd_size\n", edrv->index, econf->name);
			econf->cmd_size = 0;
		} else {
			econf->cmd_size = (unsigned char)val;
		}
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			EXTPR("[%d]: %s: cmd_size: %d\n",
			      edrv->index, econf->name, econf->cmd_size);
		}
		if (econf->cmd_size == 0)
			break;

		if (econf->cmd_size == LCD_EXT_CMD_SIZE_DYNAMIC) {
			ret = lcd_extern_init_table_dynamic_dts(edrv, child, econf, 1);
			if (ret)
				break;
			ret = lcd_extern_init_table_dynamic_dts(edrv, child, econf, 0);
		} else {
			ret = lcd_extern_init_table_fixed_dts(edrv, child, econf, 1);
			if (ret)
				break;
			ret = lcd_extern_init_table_fixed_dts(edrv, child, econf, 0);
		}
		if (ret == 0)
			econf->table_init_loaded = 1;
		break;
	case LCD_EXTERN_MIPI:
		ret = of_property_read_u32(child, "cmd_size", &val);
		if (ret) {
			EXTPR("[%d]: %s: no cmd_size\n", edrv->index, econf->name);
			econf->cmd_size = 0;
		} else {
			econf->cmd_size = (unsigned char)val;
		}
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			EXTPR("[%d]: %s: cmd_size = %d\n",
			      edrv->index, econf->name, econf->cmd_size);
		}
		if (econf->cmd_size != LCD_EXT_CMD_SIZE_DYNAMIC)
			break;

		ret = lcd_extern_init_table_dynamic_dts(edrv, child, econf, 1);
		if (ret)
			break;
		ret = lcd_extern_init_table_dynamic_dts(edrv, child, econf, 0);
		if (ret == 0)
			econf->table_init_loaded = 1;
		break;
	default:
		break;
	}

	return 0;
}
#endif

static int lcd_extern_init_dynamic_ukey(struct lcd_extern_driver_s *edrv,
					struct lcd_extern_config_s *econf,
					unsigned char *p, int key_len,
					int len, int flag)
{
	unsigned char cmd_size = 0;
	int i = 0, j, max_len, ret = 0;
	unsigned char *table, *buf;
	char propname[20];

	if (flag) {
		max_len = LCD_EXTERN_INIT_ON_MAX;
		sprintf(propname, "init_on");
		buf = p;
	} else {
		max_len = LCD_EXTERN_INIT_OFF_MAX;
		sprintf(propname, "init_off");
		buf = p + econf->table_init_on_cnt;
	}
	table = kcalloc(max_len, sizeof(unsigned char), GFP_KERNEL);
	if (!table)
		return -1;
	table[0] = LCD_EXT_CMD_TYPE_END;
	table[1] = 0;

	switch (econf->type) {
	case LCD_EXTERN_I2C:
	case LCD_EXTERN_SPI:
		while ((i + 1) < max_len) {
			/* type */
			len += 1;
			ret = lcd_unifykey_len_check(key_len, len);
			if (ret) {
				EXTERR("%s: get %s failed\n", econf->name, propname);
				table[i] = LCD_EXT_CMD_TYPE_END;
				table[i + 1] = 0;
				goto init_table_dynamic_ukey_err;
			}
			table[i] = *(buf + LCD_UKEY_EXT_INIT + i);
			/* cmd_size */
			len += 1;
			ret = lcd_unifykey_len_check(key_len, len);
			if (ret) {
				EXTERR("%s: get %s failed\n", econf->name, propname);
				table[i] = LCD_EXT_CMD_TYPE_END;
				table[i + 1] = 0;
				goto init_table_dynamic_ukey_err;
			}
			table[i + 1] = *(buf + LCD_UKEY_EXT_INIT + i + 1);
			cmd_size = table[i + 1];

			if (table[i] == LCD_EXT_CMD_TYPE_END)
				break;
			if (cmd_size == 0)
				goto init_table_dynamic_i2c_spi_ukey_next;
			if ((i + 2 + cmd_size) > max_len) {
				EXTERR("%s: %s cmd_size out of support\n", econf->name, propname);
				table[i] = LCD_EXT_CMD_TYPE_END;
				table[i + 1] = 0;
				goto init_table_dynamic_ukey_err;
			}

			/* step3: data */
			len += cmd_size;
			ret = lcd_unifykey_len_check(key_len, len);
			if (ret) {
				EXTERR("%s: get %s failed\n", econf->name, propname);
				table[i] = LCD_EXT_CMD_TYPE_END;
				table[i + 1] = 0;
				goto init_table_dynamic_ukey_err;
			}
			for (j = 0; j < cmd_size; j++)
				table[i + 2 + j] = *(buf + LCD_UKEY_EXT_INIT + i + 2 + j);
init_table_dynamic_i2c_spi_ukey_next:
			i += (cmd_size + 2);
		}
		if (flag)
			econf->table_init_on_cnt = i + 2;
		else
			econf->table_init_off_cnt = i + 2;
		break;
	case LCD_EXTERN_MIPI:
		while ((i + 1) < max_len) {
			/* type */
			len += 1;
			ret = lcd_unifykey_len_check(key_len, len);
			if (ret) {
				EXTERR("%s: get %s failed\n", econf->name, propname);
				table[i] = LCD_EXT_CMD_TYPE_END;
				table[i + 1] = 0;
				goto init_table_dynamic_ukey_err;
			}
			table[i] = *(buf + LCD_UKEY_EXT_INIT + i);
			/* cmd_size */
			len += 1;
			ret = lcd_unifykey_len_check(key_len, len);
			if (ret) {
				EXTERR("%s: get %s failed\n", econf->name, propname);
				table[i] = LCD_EXT_CMD_TYPE_END;
				table[i + 1] = 0;
				goto init_table_dynamic_ukey_err;
			}
			table[i + 1] = *(buf + LCD_UKEY_EXT_INIT + i + 1);
			cmd_size = table[i + 1];

			if (table[i] == LCD_EXT_CMD_TYPE_END) {
				if (cmd_size == 0xff || cmd_size == 0)
					break;
				cmd_size = 0;
			}
			if (cmd_size == 0)
				goto init_table_dynamic_mipi_ukey_next;
			if ((i + 2 + cmd_size) > max_len) {
				EXTERR("%s: %s cmd_size out of max\n", econf->name, propname);
				table[i] = LCD_EXT_CMD_TYPE_END;
				table[i + 1] = 0;
				goto init_table_dynamic_ukey_err;
			}

			/* data */
			len += cmd_size;
			ret = lcd_unifykey_len_check(key_len, len);
			if (ret) {
				EXTERR("%s: get %s failed\n", econf->name, propname);
				table[i] = LCD_EXT_CMD_TYPE_END;
				table[i + 1] = 0;
				goto init_table_dynamic_ukey_err;
			}
			for (j = 0; j < cmd_size; j++)
				table[i + 2 + j] = *(buf + LCD_UKEY_EXT_INIT + i + 2 + j);
init_table_dynamic_mipi_ukey_next:
			i += (cmd_size + 2);
		}
		if (flag)
			econf->table_init_on_cnt = i + 2;
		else
			econf->table_init_off_cnt = i + 2;
		break;
	default:
		goto init_table_dynamic_ukey_end;
	}

	if (flag) {
		econf->table_init_on = kcalloc(econf->table_init_on_cnt,
					       sizeof(unsigned char), GFP_KERNEL);
		if (!econf->table_init_on)
			goto init_table_dynamic_ukey_err;
		memcpy(econf->table_init_on, table, econf->table_init_on_cnt);
	} else {
		econf->table_init_off = kcalloc(econf->table_init_off_cnt,
						sizeof(unsigned char), GFP_KERNEL);
		if (!econf->table_init_off)
			goto init_table_dynamic_ukey_err;
		memcpy(econf->table_init_off, table, econf->table_init_off_cnt);
	}

init_table_dynamic_ukey_end:
	kfree(table);
	return 0;

init_table_dynamic_ukey_err:
	kfree(table);
	return -1;
}

static int lcd_extern_init_fixed_ukey(struct lcd_extern_driver_s *edrv,
				      struct lcd_extern_config_s *econf,
				      unsigned char *p, int key_len,
				      int len, int flag)
{
	unsigned char cmd_size;
	int i = 0, j, max_len, ret = 0;
	unsigned char *table, *buf;
	char propname[20];

	cmd_size = econf->cmd_size;
	if (flag) {
		max_len = LCD_EXTERN_INIT_ON_MAX;
		sprintf(propname, "init_on");
		buf = p;
	} else {
		max_len = LCD_EXTERN_INIT_OFF_MAX;
		sprintf(propname, "init_off");
		buf = p + econf->table_init_on_cnt;
	}
	table = kcalloc(max_len, sizeof(unsigned char), GFP_KERNEL);
	if (!table)
		return -1;
	table[0] = LCD_EXT_CMD_TYPE_END;
	table[1] = 0;

	while (i < max_len) {
		if ((i + cmd_size) > max_len) {
			EXTERR("%s: %s cmd_size out of max\n", econf->name, propname);
			table[i] = LCD_EXT_CMD_TYPE_END;
			goto init_table_fixed_ukey_err;
		}
		len += cmd_size;
		ret = lcd_unifykey_len_check(key_len, len);
		if (ret) {
			EXTERR("%s: get %s failed\n", econf->name, propname);
			table[i] = LCD_EXT_CMD_TYPE_END;
			for (j = 1; j < cmd_size; j++)
				table[i + j] = 0;
			goto init_table_fixed_ukey_err;
		}
		for (j = 0; j < cmd_size; j++)
			table[i + j] = *(buf + LCD_UKEY_EXT_INIT + i + j);
		if (table[i] == LCD_EXT_CMD_TYPE_END)
			break;
		i += cmd_size;
	}
	if (flag) {
		econf->table_init_on_cnt = i + cmd_size;
		econf->table_init_on = kcalloc(econf->table_init_on_cnt,
					       sizeof(unsigned char), GFP_KERNEL);
		if (!econf->table_init_on)
			goto init_table_fixed_ukey_err;
		memcpy(econf->table_init_on, table, econf->table_init_on_cnt);
	} else {
		econf->table_init_off_cnt = i + cmd_size;
		econf->table_init_off = kcalloc(econf->table_init_off_cnt,
						sizeof(unsigned char), GFP_KERNEL);
		if (!econf->table_init_off)
			goto init_table_fixed_ukey_err;
		memcpy(econf->table_init_off, table, econf->table_init_off_cnt);
	}

	kfree(table);
	return 0;

init_table_fixed_ukey_err:
	kfree(table);
	return -1;
}

static int lcd_extern_get_config_unifykey(struct lcd_extern_driver_s *edrv,
					  struct lcd_extern_dev_s *edev)
{
	struct lcd_extern_config_s *econf;
	struct aml_lcd_unifykey_header_s ext_header;
	unsigned char *para, *p;
	int key_len, len;
	const char *str;
	int ret;

	if (!edev)
		return -1;

	econf = &edev->config;
	key_len = LCD_UKEY_LCD_EXT_SIZE;
	para = kcalloc(key_len, sizeof(unsigned char), GFP_KERNEL);
	if (!para)
		return -1;

	ret = lcd_unifykey_get(edrv->ukey_name, para, &key_len);
	if (ret)
		goto lcd_ext_get_config_ukey_error;

	/* check lcd_extern unifykey length */
	len = 10 + 33 + 10;
	ret = lcd_unifykey_len_check(key_len, len);
	if (ret) {
		EXTERR("unifykey length is not correct\n");
		goto lcd_ext_get_config_ukey_error;
	}

	/* header: 10byte */
	lcd_unifykey_header_check(para, &ext_header);
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
		EXTPR("unifykey header:\n");
		EXTPR("crc32             = 0x%08x\n", ext_header.crc32);
		EXTPR("data_len          = %d\n", ext_header.data_len);
		EXTPR("version           = 0x%04x\n", ext_header.version);
	}

	/* basic: 33byte */
	p = para;
	str = (const char *)(p + LCD_UKEY_HEAD_SIZE);
	strncpy(econf->name, str, (LCD_EXTERN_NAME_LEN_MAX - 1));
	econf->index = *(p + LCD_UKEY_EXT_INDEX);
	econf->type = *(p + LCD_UKEY_EXT_TYPE);
	econf->status = *(p + LCD_UKEY_EXT_STATUS);

	if (econf->index == LCD_EXTERN_INDEX_INVALID) {
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			EXTPR("index %d is invalid\n", econf->index);
		goto lcd_ext_get_config_ukey_error;
	}

	/* type: 10byte */
	switch (econf->type) {
	case LCD_EXTERN_I2C:
		if (*(p + LCD_UKEY_EXT_TYPE_VAL_0))
			econf->i2c_addr = *(p + LCD_UKEY_EXT_TYPE_VAL_0);
		else
			econf->i2c_addr = LCD_EXT_I2C_ADDR_INVALID;
		if (*(p + LCD_UKEY_EXT_TYPE_VAL_1))
			econf->i2c_addr2 = *(p + LCD_UKEY_EXT_TYPE_VAL_1);
		else
			econf->i2c_addr2 = LCD_EXT_I2C_ADDR_INVALID;
		if (*(p + LCD_UKEY_EXT_TYPE_VAL_4))
			econf->i2c_addr3 = *(p + LCD_UKEY_EXT_TYPE_VAL_4);
		else
			econf->i2c_addr3 = LCD_EXT_I2C_ADDR_INVALID;
		if (*(p + LCD_UKEY_EXT_TYPE_VAL_5))
			econf->i2c_addr4 = *(p + LCD_UKEY_EXT_TYPE_VAL_5);
		else
			econf->i2c_addr4 = LCD_EXT_I2C_ADDR_INVALID;

		econf->cmd_size = *(p + LCD_UKEY_EXT_TYPE_VAL_3);
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			EXTPR("%s: cmd_size = %d\n", econf->name, econf->cmd_size);

		/* init */
		if (econf->cmd_size == 0)
			break;
		if (econf->cmd_size == LCD_EXT_CMD_SIZE_DYNAMIC) {
			ret = lcd_extern_init_dynamic_ukey(edrv, econf, p, key_len, len, 1);
			if (ret)
				break;
			ret = lcd_extern_init_dynamic_ukey(edrv, econf, p, key_len, len, 0);
		} else {
			ret = lcd_extern_init_fixed_ukey(edrv, econf, p, key_len, len, 1);
			if (ret)
				break;
			ret = lcd_extern_init_fixed_ukey(edrv, econf, p, key_len, len, 0);
		}
		if (ret == 0)
			econf->table_init_loaded = 1;
		break;
	case LCD_EXTERN_SPI:
		econf->spi_gpio_cs = *(p + LCD_UKEY_EXT_TYPE_VAL_0);
		lcd_extern_gpio_probe(edrv, econf->spi_gpio_cs);
		econf->spi_gpio_clk = *(p + LCD_UKEY_EXT_TYPE_VAL_1);
		lcd_extern_gpio_probe(edrv, econf->spi_gpio_clk);
		econf->spi_gpio_data = *(p + LCD_UKEY_EXT_TYPE_VAL_2);
		lcd_extern_gpio_probe(edrv, econf->spi_gpio_data);
		econf->spi_clk_freq = (*(p + LCD_UKEY_EXT_TYPE_VAL_3) |
			((*(p + LCD_UKEY_EXT_TYPE_VAL_4)) << 8));
		econf->spi_clk_pol = *(p + LCD_UKEY_EXT_TYPE_VAL_5);
		econf->cmd_size = *(p + LCD_UKEY_EXT_TYPE_VAL_6);
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			EXTPR("%s: cmd_size = %d\n", econf->name, econf->cmd_size);

		/* init */
		if (econf->cmd_size == 0)
			break;
		if (econf->cmd_size == LCD_EXT_CMD_SIZE_DYNAMIC) {
			ret = lcd_extern_init_dynamic_ukey(edrv, econf, p, key_len, len, 1);
			if (ret)
				break;
			ret = lcd_extern_init_dynamic_ukey(edrv, econf, p, key_len, len, 0);
		} else {
			ret = lcd_extern_init_fixed_ukey(edrv, econf, p, key_len, len, 1);
			if (ret)
				break;
			ret = lcd_extern_init_fixed_ukey(edrv, econf, p, key_len, len, 0);
		}
		if (ret == 0)
			econf->table_init_loaded = 1;
		break;
	case LCD_EXTERN_MIPI:
		econf->cmd_size = *(p + LCD_UKEY_EXT_TYPE_VAL_0);
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			EXTPR("%s: cmd_size = %d\n", econf->name, econf->cmd_size);
		if (econf->cmd_size != LCD_EXT_CMD_SIZE_DYNAMIC)
			break;
		ret = lcd_extern_init_dynamic_ukey(edrv, econf, p, key_len, len, 1);
		if (ret)
			break;
		ret = lcd_extern_init_dynamic_ukey(edrv, econf, p, key_len, len, 0);
		if (ret == 0)
			econf->table_init_loaded = 1;
		break;
	default:
		break;
	}

	kfree(para);
	return 0;

lcd_ext_get_config_ukey_error:
	kfree(para);
	return 0;
}

static void lcd_extern_multi_list_add(struct lcd_extern_dev_s *edev,
		unsigned int index, unsigned int type,
		unsigned char data_len, unsigned char *data_buf)
{
	struct lcd_extern_multi_list_s *temp_list;
	struct lcd_extern_multi_list_s *cur_list;

	/* creat list */
	cur_list = kzalloc(sizeof(*cur_list), GFP_KERNEL);
	if (!cur_list)
		return;
	cur_list->index = index;
	cur_list->type = type;
	cur_list->data_len = data_len;
	cur_list->data_buf = data_buf;

	if (!edev->multi_list_header) {
		edev->multi_list_header = cur_list;
	} else {
		temp_list = edev->multi_list_header;
		while (temp_list->next) {
			if (temp_list->index == cur_list->index) {
				EXTERR("%s: dev_%d: index=%d(type=%d) already in list\n",
					__func__, edev->dev_index,
					cur_list->index, cur_list->type);
				kfree(cur_list);
				return;
			}
			temp_list = temp_list->next;
		}
		temp_list->next = cur_list;
	}

	EXTPR("%s: dev_%d: index=%d, type=%d\n",
	       __func__, edev->dev_index, cur_list->index, cur_list->type);
}

static int lcd_extern_multi_list_remove(struct lcd_extern_dev_s *edev)
{
	struct lcd_extern_multi_list_s *cur_list;
	struct lcd_extern_multi_list_s *next_list;

	/* add to exist list */
	cur_list = edev->multi_list_header;
	while (cur_list) {
		next_list = cur_list->next;
		kfree(cur_list);
		cur_list = next_list;
	}
	edev->multi_list_header = NULL;

	return 0;
}

static void lcd_extern_config_update_dynamic_size(struct lcd_extern_driver_s *edrv,
						  struct lcd_extern_dev_s *edev,
						  int flag)
{
	unsigned char type, size, *table;
	unsigned int max_len = 0, i = 0, j, index;

	if (flag) {
		max_len = edev->config.table_init_on_cnt;
		table = edev->config.table_init_on;
	} else {
		max_len = edev->config.table_init_off_cnt;
		table = edev->config.table_init_off;
	}

	while ((i + 1) < max_len) {
		type = table[i];
		size = table[i + 1];
		if (type == LCD_EXT_CMD_TYPE_END)
			break;
		if (size == 0)
			goto lcd_extern_config_update_dynamic_size_next;
		if ((i + 2 + size) > max_len)
			break;

		if (type == LCD_EXT_CMD_TYPE_GPIO) {
			/* gpio probe */
			index = table[i + 2];
			if (index < LCD_EXTERN_GPIO_NUM_MAX)
				lcd_extern_gpio_probe(edrv, index);
		} else if (type == LCD_EXT_CMD_TYPE_MULTI_FR) {
			for (j = 0; j < size; j += 3) {
				index = i + 2 + j;
				lcd_extern_multi_list_add(edev, table[index],
					type, 2, &table[index + 1]);
			}
		}
lcd_extern_config_update_dynamic_size_next:
		i += (size + 2);
	}
}

static void lcd_extern_config_update_fixed_size(struct lcd_extern_driver_s *edrv,
						struct lcd_extern_dev_s *edev,
						int flag)
{
	int i = 0, max_len = 0;
	unsigned char type, cmd_size, index;
	unsigned char *table;

	cmd_size = edev->config.cmd_size;
	if (cmd_size < 2) {
		EXTERR("[%d]: %s: dev_%d invalid cmd_size %d\n",
		       edrv->index, __func__, edev->dev_index, cmd_size);
		return;
	}

	if (flag) {
		max_len = edev->config.table_init_on_cnt;
		table = edev->config.table_init_on;
	} else {
		max_len = edev->config.table_init_off_cnt;
		table = edev->config.table_init_off;
	}

	while ((i + cmd_size) <= max_len) {
		type = table[i];
		if (type == LCD_EXT_CMD_TYPE_END)
			break;
		if (type == LCD_EXT_CMD_TYPE_GPIO) {
			/* gpio probe */
			index = table[i + 1];
			if (index < LCD_EXTERN_GPIO_NUM_MAX)
				lcd_extern_gpio_probe(edrv, index);
		}
		i += cmd_size;
	}
}

static void lcd_extern_config_update(struct lcd_extern_driver_s *edrv,
				     struct lcd_extern_dev_s *edev)
{
	if (edev->config.cmd_size == LCD_EXT_CMD_SIZE_DYNAMIC) {
		lcd_extern_config_update_dynamic_size(edrv, edev, 1);
		lcd_extern_config_update_dynamic_size(edrv, edev, 0);
	} else {
		lcd_extern_config_update_fixed_size(edrv, edev, 1);
		lcd_extern_config_update_fixed_size(edrv, edev, 0);
	}
}

static struct lcd_extern_dev_s *lcd_extern_dev_malloc(int dev_index)
{
	struct lcd_extern_dev_s *edev;

	edev = kzalloc(sizeof(*edev), GFP_KERNEL);
	if (!edev)
		return NULL;

	edev->dev_index = dev_index;
	edev->config.index = LCD_EXTERN_INDEX_INVALID;
	edev->config.type = LCD_EXTERN_MAX;
	edev->config.i2c_addr = LCD_EXT_I2C_ADDR_INVALID;
	edev->config.i2c_addr2 = LCD_EXT_I2C_ADDR_INVALID;
	edev->config.i2c_addr3 = LCD_EXT_I2C_ADDR_INVALID;
	edev->config.i2c_addr4 = LCD_EXT_I2C_ADDR_INVALID;

	edev->config.spi_gpio_cs = LCD_EXT_GPIO_INVALID;
	edev->config.spi_gpio_clk = LCD_EXT_GPIO_INVALID;
	edev->config.spi_gpio_data = LCD_EXT_GPIO_INVALID;
	edev->config.spi_clk_pol = 1;

	return edev;
}

static void lcd_extern_dev_free(struct lcd_extern_dev_s *edev)
{
	if (!edev)
		return;

	lcd_extern_multi_list_remove(edev);

	kfree(edev->config.table_init_on);
	edev->config.table_init_on = NULL;

	kfree(edev->config.table_init_off);
	edev->config.table_init_off = NULL;

	kfree(edev);
}

static int lcd_extern_add_dev(struct lcd_extern_driver_s *edrv,
			      struct lcd_extern_dev_s *edev)
{
	struct aml_lcd_drv_s *pdrv;
	int ret = -1;

	if (edev->config.status == 0) {
		EXTERR("[%d]: %s: %s[%d] status is disabled\n",
		       edrv->index, __func__,
		       edev->config.name, edev->dev_index);
		return -1;
	}

	if (strcmp(edev->config.name, "ext_default") == 0) {
		if (edev->config.type == LCD_EXTERN_MIPI)
			ret = lcd_extern_mipi_default_probe(edrv, edev);
		else
			ret = lcd_extern_default_probe(edrv, edev);
	} else if (strcmp(edev->config.name, "mipi_default") == 0) {
		ret = lcd_extern_mipi_default_probe(edrv, edev);
#ifdef CONFIG_AMLOGIC_LCD_EXTERN_I2C_CS602
	} else if (strcmp(edev->config.name, "i2c_CS602") == 0) {
		ret = lcd_extern_i2c_CS602_probe(edrv, edev);
#endif
#ifdef CONFIG_AMLOGIC_LCD_EXTERN_I2C_ANX6862_7911
	} else if (strcmp(edev->config.name, "i2c_ANX6862_7911") == 0) {
		ret = lcd_extern_i2c_ANX6862_7911_probe(edrv, edev);
#endif
#ifdef CONFIG_AMLOGIC_LCD_EXTERN_I2C_OLED
	} else if (strcmp(edev->config.name, "i2c_oled") == 0) {
		ret = lcd_extern_i2c_oled_probe(edrv, edev);
#endif
	} else {
		EXTERR("[%d]: %s: invalid dev: %s(%d)\n",
		       edrv->index, __func__,
		       edev->config.name, edev->dev_index);
	}

	if (ret) {
		EXTERR("[%d]: %s: %s(%d) failed\n",
		       edrv->index, __func__,
		       edev->config.name, edev->dev_index);
		return -1;
	}

	pdrv = aml_lcd_get_driver(edrv->index);
	if (pdrv && (pdrv->status & LCD_STATUS_IF_ON)) {
		if (edev->init)
			edev->init(edrv, edev);
	}

	EXTPR("[%d]: %s: %s(%d) ok\n",
	      edrv->index, __func__,
	      edev->config.name, edev->dev_index);
	return 0;
}

static void lcd_extern_dev_probe_work(struct work_struct *p_work)
{
	struct delayed_work *d_work;
	struct lcd_extern_driver_s *edrv;
	bool is_init;
	int index, dev_index;
	int ret;

	d_work = container_of(p_work, struct delayed_work, work);
	edrv = container_of(d_work, struct lcd_extern_driver_s, dev_probe_dly_work);

	index = edrv->index;

	is_init = lcd_unifykey_init_get();
	if (!is_init) {
		if (edrv->retry_cnt++ < LCD_UNIFYKEY_WAIT_TIMEOUT) {
			lcd_queue_delayed_work(&edrv->dev_probe_dly_work,
				LCD_UNIFYKEY_RETRY_INTERVAL);
			return;
		}
		EXTERR("[%d]: %s: key_init_flag=%d, exit\n", edrv->index, __func__, is_init);
		goto lcd_ext_dev_probe_work_err;
	}

	if (edrv->index == 0)
		sprintf(edrv->ukey_name, "lcd_extern");
	else
		sprintf(edrv->ukey_name, "lcd%d_extern", edrv->index);
	ret = lcd_unifykey_check(edrv->ukey_name);
	if (ret)
		goto lcd_ext_dev_probe_work_err;

	dev_index = 0;

	edrv->dev[edrv->dev_cnt] = lcd_extern_dev_malloc(dev_index);
	EXTPR("[%d]: %s: from unifykey\n", edrv->index, __func__);
	ret = lcd_extern_get_config_unifykey(edrv, edrv->dev[edrv->dev_cnt]);
	if (ret) {
		lcd_extern_dev_free(edrv->dev[edrv->dev_cnt]);
		edrv->dev[edrv->dev_cnt] = NULL;
		goto lcd_ext_dev_probe_work_err;
	}

	lcd_extern_config_update(edrv, edrv->dev[edrv->dev_cnt]);
	ret = lcd_extern_add_dev(edrv, edrv->dev[edrv->dev_cnt]);
	if (ret) {
		lcd_extern_dev_free(edrv->dev[edrv->dev_cnt]);
		edrv->dev[edrv->dev_cnt] = NULL;
		goto lcd_ext_dev_probe_work_err;
	} else {
		edrv->dev_cnt++;
	}

	return;

lcd_ext_dev_probe_work_err:
	/* free drvdata */
	platform_set_drvdata(edrv->pdev, NULL);
	/* free drv */
	kfree(edrv);
	ext_driver[index] = NULL;
	EXTPR("[%d]: %s: failed\n", index, __func__);
}

static int lcd_extern_config_load(struct lcd_extern_driver_s *edrv)
{
	struct device_node *np;
	unsigned int para[5];
	const char *str;
	int dev_index, i;
	int ret;

	if (!edrv->pdev->dev.of_node) {
		EXTERR("[%d]: %s: dev of_node is null\n", edrv->index, __func__);
		return -1;
	}
	np = edrv->pdev->dev.of_node;

	ret = of_property_read_string(np, "i2c_bus", &str);
	if (ret)
		edrv->i2c_bus = LCD_EXT_I2C_BUS_MAX;
	else
		edrv->i2c_bus = aml_lcd_i2c_bus_get_str(str);

	ret = of_property_read_string(np, "pinctrl-names", &str);
	if (ret)
		edrv->pinmux_valid = 0;
	else
		edrv->pinmux_valid = 1;
	edrv->pinmux_flag = 0xff;

	ret = of_property_read_u32(np, "key_valid", &para[0]);
	if (ret) {
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			EXTPR("failed to get key_valid\n");
		edrv->key_valid = 0;
	} else {
		edrv->key_valid = (unsigned char)para[0];
	}
	EXTPR("[%d]: key_valid: %d\n", edrv->index, edrv->key_valid);

	if (edrv->key_valid) {
		edrv->config_load = 1;
		lcd_queue_delayed_work(&edrv->dev_probe_dly_work, 0);
	} else {
		edrv->config_load = 0;
		EXTPR("[%d]: %s from dts\n", edrv->index, __func__);
		for (i = 0; i < lcd_ext_dev_cnt[edrv->index]; i++) {
			dev_index = lcd_ext_index_lut[edrv->index][i];
			if (dev_index == LCD_EXTERN_INDEX_INVALID) {
				EXTPR("[%d]: %s: invalid dev_index\n", edrv->index, __func__);
				continue;
			}
			edrv->dev[edrv->dev_cnt] = lcd_extern_dev_malloc(dev_index);
			if (!edrv->dev[edrv->dev_cnt])
				continue;

			ret = lcd_extern_get_config_dts(np, edrv, edrv->dev[edrv->dev_cnt]);
			if (ret) {
				lcd_extern_dev_free(edrv->dev[edrv->dev_cnt]);
				edrv->dev[edrv->dev_cnt] = NULL;
				continue;
			}

			lcd_extern_config_update(edrv, edrv->dev[edrv->dev_cnt]);
			ret = lcd_extern_add_dev(edrv, edrv->dev[edrv->dev_cnt]);
			if (ret) {
				lcd_extern_dev_free(edrv->dev[edrv->dev_cnt]);
				edrv->dev[edrv->dev_cnt] = NULL;
			} else {
				edrv->dev_cnt++;
			}
		}
	}

	return 0;
}

/* *********************************************************
 * debug function
 * *********************************************************
 */
static int lcd_extern_init_dynamic_print(char *buf, struct lcd_extern_config_s *econf, int flag)
{
	int i, j, max_len;
	unsigned char type, size;
	unsigned char *table;
	int len = 0;

	if (flag) {
		len = sprintf(buf, "power on:\n");
		table = econf->table_init_on;
		max_len = econf->table_init_on_cnt;
	} else {
		len = sprintf(buf, "power off:\n");
		table = econf->table_init_off;
		max_len = econf->table_init_off_cnt;
	}
	if (!table) {
		len += sprintf(buf + len, "init_table %d is NULL\n", flag);
		return len;
	}

	i = 0;
	switch (econf->type) {
	case LCD_EXTERN_I2C:
	case LCD_EXTERN_SPI:
		while ((i + 1) < max_len) {
			type = table[i];
			size = table[i + 1];
			if (type == LCD_EXT_CMD_TYPE_END) {
				len += sprintf(buf + len, "  0x%02x,%d,\n", type, size);
				break;
			}

			len += sprintf(buf + len, "  0x%02x,%d,", type, size);
			if (size == 0)
				goto init_table_dynamic_print_i2c_spi_next;
			if (i + 2 + size > max_len) {
				len += sprintf(buf + len, "size out of support\n");
				break;
			}

			if (type == LCD_EXT_CMD_TYPE_GPIO ||
			    type == LCD_EXT_CMD_TYPE_DELAY) {
				for (j = 0; j < size; j++)
					len += sprintf(buf + len, "%d,", table[i + 2 + j]);
			} else if ((type == LCD_EXT_CMD_TYPE_CMD) ||
				   (type == LCD_EXT_CMD_TYPE_CMD2) ||
				   (type == LCD_EXT_CMD_TYPE_CMD3) ||
				   (type == LCD_EXT_CMD_TYPE_CMD4) ||
				   (type == LCD_EXT_CMD_TYPE_CMD_BIN) ||
				   (type == LCD_EXT_CMD_TYPE_CMD2_BIN) ||
				   (type == LCD_EXT_CMD_TYPE_CMD3_BIN) ||
				   (type == LCD_EXT_CMD_TYPE_CMD4_BIN) ||
				   (type == LCD_EXT_CMD_TYPE_CMD_BIN_DATA) ||
				   (type == LCD_EXT_CMD_TYPE_CMD2_BIN_DATA) ||
				   (type == LCD_EXT_CMD_TYPE_CMD3_BIN_DATA) ||
				   (type == LCD_EXT_CMD_TYPE_CMD4_BIN_DATA)) {
				for (j = 0; j < size; j++)
					len += sprintf(buf + len, "0x%02x,", table[i + 2 + j]);
			} else if ((type == LCD_EXT_CMD_TYPE_CMD_DELAY) ||
				   (type == LCD_EXT_CMD_TYPE_CMD2_DELAY)) {
				for (j = 0; j < (size - 1); j++)
					len += sprintf(buf + len, "0x%02x,", table[i + 2 + j]);
				len += sprintf(buf + len, "%d,", table[i + size + 1]);
			} else {
				for (j = 0; j < size; j++)
					len += sprintf(buf + len, "0x%02x,", table[i + 2 + j]);
			}

init_table_dynamic_print_i2c_spi_next:
			len += sprintf(buf + len, "\n");
			i += (size + 2);
		}
		break;
	case LCD_EXTERN_MIPI:
		while ((i + 1) < max_len) {
			type = table[i];
			size = table[i + 1];
			if (type == LCD_EXT_CMD_TYPE_END) {
				if (size == 0xff) {
					len += sprintf(buf + len, "  0x%02x,0x%02x,\n",
						type, size);
					break;
				}
				if (size == 0) {
					len += sprintf(buf + len, "  0x%02x,%d,\n", type, size);
					break;
				}
				size = 0;
			}

			len += sprintf(buf + len, "  0x%02x,%d,", type, size);
			if (size == 0)
				goto init_table_dynamic_print_mipi_next;
			if (i + 2 + size > max_len) {
				len += sprintf(buf + len, "size out of support\n");
				break;
			}

			if (type == LCD_EXT_CMD_TYPE_GPIO ||
			    type == LCD_EXT_CMD_TYPE_DELAY) {
				for (j = 0; j < size; j++)
					len += sprintf(buf + len, "%d,", table[i + 2 + j]);
			} else if ((type & 0xf) == 0x0) {
				len += sprintf(buf + len, "  init_%s wrong data_type: 0x%02x\n",
					       flag ? "on" : "off", type);
				break;
			} else {
				size = table[i + DSI_CMD_SIZE_INDEX];
				len += sprintf(buf + len, "  0x%02x,%d,", type, size);
				for (j = 0; j < size; j++)
					len += sprintf(buf + len, "0x%02x,", table[i + 2 + j]);
			}

init_table_dynamic_print_mipi_next:
			len += sprintf(buf + len, "\n");
			i += (size + 2);
		}
		break;
	default:
		break;
	}

	return len;
}

static int lcd_extern_init_fixed_print(char *buf, struct lcd_extern_config_s *econf, int flag)
{
	int i, j, max_len;
	unsigned char cmd_size;
	unsigned char *table;
	int len = 0;

	cmd_size = econf->cmd_size;
	if (flag) {
		len = sprintf(buf, "power on:\n");
		table = econf->table_init_on;
		max_len = econf->table_init_on_cnt;
	} else {
		len = sprintf(buf, "power off:\n");
		table = econf->table_init_off;
		max_len = econf->table_init_off_cnt;
	}
	if (!table) {
		len += sprintf(buf + len, "init_table %d is NULL\n", flag);
		return len;
	}

	i = 0;
	while ((i + cmd_size) <= max_len) {
		len += sprintf(buf + len, " ");
		for (j = 0; j < cmd_size; j++)
			len += sprintf(buf + len, " 0x%02x", table[i + j]);
		len += sprintf(buf + len, "\n");

		if (table[i] == LCD_EXT_CMD_TYPE_END)
			break;
		i += cmd_size;
	}

	return len;
}

static int lcd_extern_multi_list_print(char *buf, struct lcd_extern_dev_s *edev)
{
	struct lcd_extern_multi_list_s *temp_list;
	int len = 0, i;

	if (!edev->multi_list_header) {
		len = sprintf(buf, "multi_list: NULL\n");
		return len;
	}

	temp_list = edev->multi_list_header;
	while (temp_list) {
		len += sprintf(buf + len, "multi_list[%d]:\n", temp_list->index);
		len += sprintf(buf + len, "  type: 0x%x\n", temp_list->type);
		len += sprintf(buf + len, "  data:");
		for (i = 0; i < temp_list->data_len; i++)
			len += sprintf(buf + len, " %d", temp_list->data_buf[i]);
		len += sprintf(buf + len, "\n");
		temp_list = temp_list->next;
	}

	return len;
}

static ssize_t lcd_extern_info_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct lcd_extern_driver_s *edrv = dev_get_drvdata(dev);
	struct lcd_extern_dev_s *edev;
	ssize_t len = 0;
	int i = 0;

	len = sprintf(buf, "lcd extern driver[%d] info:\n", edrv->index);
	for (i = 0; i < edrv->dev_cnt; i++) {
		edev = edrv->dev[i];
		if (!edev)
			continue;

		len += sprintf(buf + len, "dev[%d]: %s\n",
			       edev->dev_index, edev->config.name);
		len += sprintf(buf + len, "status:             %d\n", edev->config.status);
		switch (edev->config.type) {
		case LCD_EXTERN_I2C:
			len += sprintf(buf + len,
				"type:               i2c(%d)\n"
				"i2c_addr:           0x%02x\n"
				"i2c_addr2:          0x%02x\n"
				"i2c_addr3:          0x%02x\n"
				"i2c_addr4:          0x%02x\n"
				"i2c_bus:            %d\n"
				"table_loaded:       %d\n"
				"cmd_size:           %d\n"
				"table_init_on_cnt:  %d\n"
				"table_init_off_cnt: %d\n",
				edev->config.type,
				edev->config.i2c_addr, edev->config.i2c_addr2,
				edev->config.i2c_addr3, edev->config.i2c_addr4,
				edrv->i2c_bus,
				edev->config.table_init_loaded, edev->config.cmd_size,
				edev->config.table_init_on_cnt,
				edev->config.table_init_off_cnt);
			if (edev->config.cmd_size == 0)
				break;
			if (edev->config.cmd_size == LCD_EXT_CMD_SIZE_DYNAMIC) {
				len += lcd_extern_init_dynamic_print(buf + len, &edev->config, 1);
				len += lcd_extern_init_dynamic_print(buf + len, &edev->config, 0);
			} else {
				len += lcd_extern_init_fixed_print(buf + len, &edev->config, 1);
				len += lcd_extern_init_fixed_print(buf + len, &edev->config, 0);
			}
			len += lcd_extern_multi_list_print(buf + len, edev);
			break;
		case LCD_EXTERN_SPI:
			len += sprintf(buf + len,
				"type:               spi(%d)\n"
				"spi_gpio_cs:        %d\n"
				"spi_gpio_clk:       %d\n"
				"spi_gpio_data:      %d\n"
				"spi_clk_freq:       %dKHz\n"
				"spi_delay_us:       %d\n"
				"spi_clk_pol:        %d\n"
				"table_loaded:       %d\n"
				"cmd_size:           %d\n"
				"table_init_on_cnt:  %d\n"
				"table_init_off_cnt: %d\n",
				edev->config.type,
				edev->config.spi_gpio_cs, edev->config.spi_gpio_clk,
				edev->config.spi_gpio_data, edev->config.spi_clk_freq,
				edev->config.spi_delay_us, edev->config.spi_clk_pol,
				edev->config.table_init_loaded, edev->config.cmd_size,
				edev->config.table_init_on_cnt,
				edev->config.table_init_off_cnt);
			if (edev->config.cmd_size == 0)
				break;
			if (edev->config.cmd_size == LCD_EXT_CMD_SIZE_DYNAMIC) {
				len += lcd_extern_init_dynamic_print(buf + len, &edev->config, 1);
				len += lcd_extern_init_dynamic_print(buf + len, &edev->config, 0);
			} else {
				len += lcd_extern_init_fixed_print(buf + len, &edev->config, 1);
				len += lcd_extern_init_fixed_print(buf + len, &edev->config, 0);
			}
			break;
		case LCD_EXTERN_MIPI:
			len += sprintf(buf + len,
				"type:            mipi(%d)\n"
				"table_loaded:    %d\n"
				"cmd_size:        %d\n"
				"table_init_on_cnt:  %d\n"
				"table_init_off_cnt: %d\n",
				edev->config.type,
				edev->config.table_init_loaded,
				edev->config.cmd_size,
				edev->config.table_init_on_cnt,
				edev->config.table_init_off_cnt);
			if (edev->config.cmd_size != LCD_EXT_CMD_SIZE_DYNAMIC)
				break;
			len += lcd_extern_init_dynamic_print(buf + len, &edev->config, 1);
			len += lcd_extern_init_dynamic_print(buf + len, &edev->config, 0);
			break;
		default:
			len += sprintf(buf + len, "invalid extern_type\n");
			break;
		}

		if (edrv->pinmux_valid) {
			len += sprintf(buf + len,
				"pinmux_flag:     %d\n"
				"pinmux_pointer:  0x%p\n",
				edrv->pinmux_flag, edrv->pin);
		}
	}

	return len;
}

static const char *lcd_extern_debug_usage_str = {
"Usage:\n"
"    echo test <on/off> > debug ; test power on/off for extern device\n"
"        <on/off>: 1 for power on, 0 for power off\n"
"    echo r <addr_sel> <reg> > debug ; read reg for extern device\n"
"    echo d <addr_sel> <reg> <cnt> > debug ; dump regs for extern device\n"
"    echo w <addr_sel> <reg> <value> > debug ; write reg for extern device\n"
};

static ssize_t lcd_extern_debug_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", lcd_extern_debug_usage_str);
}

static ssize_t lcd_extern_debug_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	struct lcd_extern_driver_s *edrv = dev_get_drvdata(dev);
	struct lcd_extern_dev_s *edev;
	unsigned int ret, j;
	unsigned int val[3];
	unsigned short reg;
	unsigned char value, reg_buf[2];
	unsigned int index = LCD_EXTERN_INDEX_INVALID;

	switch (buf[0]) {
	case 't':
		ret = sscanf(buf, "test %d %d", &index, &val[0]);
		if (ret == 2) {
			edev = lcd_extern_get_dev(edrv, index);
			if (!edev)
				return -EINVAL;

			if (val[0]) {
				if (edev->power_on)
					edev->power_on(edrv, edev);
			} else {
				if (edev->power_off)
					edev->power_off(edrv, edev);
			}
		} else {
			pr_info("invalid data\n");
			return -EINVAL;
		}
		break;
	case 'r':
		ret = sscanf(buf, "r %d %d %x", &index, &val[0], &val[1]);
		if (ret == 3) {
			edev = lcd_extern_get_dev(edrv, index);
			if (!edev)
				return -EINVAL;
			edev->addr_sel = (unsigned char)val[0];
			reg = (unsigned short)val[1];
			if (edev->reg_read) {
				edev->reg_read(edrv, edev, 1, reg, &value);
				pr_info("reg read: 0x%02x = 0x%02x\n", reg, value);
			}
		} else {
			pr_info("invalid data\n");
			return -EINVAL;
		}
		break;
	case 'd':
		ret = sscanf(buf, "d %d %d %x %d", &index, &val[0], &val[1], &val[2]);
		if (ret == 4) {
			edev = lcd_extern_get_dev(edrv, index);
			if (!edev)
				return -EINVAL;
			edev->addr_sel = (unsigned char)val[0];
			reg = (unsigned short)val[1];
			if (edev->reg_read) {
				pr_info("reg dump:\n");
				for (j = 0; j < val[2]; j++) {
					edev->reg_read(edrv, edev, 1, reg + j, &value);
					pr_info("  0x%02x = 0x%02x\n", reg, value);
				}
			}
		} else {
			pr_info("invalid data\n");
			return -EINVAL;
		}
		break;
	case 'w':
		ret = sscanf(buf, "w %d %d %x %x", &index, &val[0],
			     &val[1], &val[2]);
		if (ret == 4) {
			edev = lcd_extern_get_dev(edrv, index);
			if (!edev)
				return -EINVAL;
			edev->addr_sel = (unsigned char)val[0];
			reg = (unsigned short)val[1];
			value = (unsigned char)val[2];
			if (edev->reg_write) {
				reg_buf[0] = (unsigned char)val[1];
				reg_buf[1] = (unsigned char)val[2];
				edev->reg_write(edrv, edev, reg_buf, 2);
				if (edev->reg_read) {
					edev->reg_read(edrv, edev, 1, reg, &value);
					pr_info("reg write 0x%02x = 0x%02x, readback: 0x%02x\n",
						reg, val[2], value);
				} else {
					pr_info("reg write 0x%02x = 0x%02x\n", reg, value);
				}
			}
		} else {
			pr_info("invalid data\n");
			return -EINVAL;
		}
		break;
	default:
		pr_info("invalid data\n");
		break;
	}

	return count;
}

static struct device_attribute lcd_extern_debug_attrs[] = {
	__ATTR(info, 0444, lcd_extern_info_show, NULL),
	__ATTR(debug, 0644, lcd_extern_debug_show, lcd_extern_debug_store),
};

static int lcd_extern_debug_file_creat(struct lcd_extern_driver_s *edrv)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(lcd_extern_debug_attrs); i++) {
		if (device_create_file(edrv->sub_dev, &lcd_extern_debug_attrs[i])) {
			EXTERR("[%d]: create debug attribute %s fail\n",
			       edrv->index, lcd_extern_debug_attrs[i].attr.name);
		}
	}

	return 0;
}

static int lcd_extern_debug_file_remove(struct lcd_extern_driver_s *edrv)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(lcd_extern_debug_attrs); i++)
		device_remove_file(edrv->sub_dev, &lcd_extern_debug_attrs[i]);

	return 0;
}

/* ************************************************************* */
static int ext_io_open(struct inode *inode, struct file *file)
{
	struct lcd_extern_driver_s *edrv;

	edrv = container_of(inode->i_cdev, struct lcd_extern_driver_s, cdev);
	file->private_data = edrv;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		EXTPR("%s\n", __func__);

	return 0;
}

static int ext_io_release(struct inode *inode, struct file *file)
{
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		EXTPR("%s\n", __func__);
	file->private_data = NULL;
	return 0;
}

static long ext_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	return 0;
}

#ifdef CONFIG_COMPAT
static long ext_compat_ioctl(struct file *file, unsigned int cmd,
			     unsigned long arg)
{
	unsigned long ret;

	arg = (unsigned long)compat_ptr(arg);
	ret = ext_ioctl(file, cmd, arg);
	return ret;
}
#endif

static const struct file_operations ext_fops = {
	.owner          = THIS_MODULE,
	.open           = ext_io_open,
	.release        = ext_io_release,
	.unlocked_ioctl = ext_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl   = ext_compat_ioctl,
#endif
};

static int ext_cdev_add(struct lcd_extern_driver_s *edrv, struct device *parent)
{
	dev_t devno;
	int ret = 0;

	if (!edrv) {
		EXTERR("%s: edrv is null\n", __func__);
		return -1;
	}
	if (!ext_cdev) {
		ret = 1;
		goto ext_cdev_add_failed;
	}

	devno = MKDEV(MAJOR(ext_cdev->devno), edrv->index);

	cdev_init(&edrv->cdev, &ext_fops);
	edrv->cdev.owner = THIS_MODULE;
	ret = cdev_add(&edrv->cdev, devno, 1);
	if (ret) {
		ret = 2;
		goto ext_cdev_add_failed;
	}

	edrv->sub_dev = device_create(ext_cdev->class, parent,
				      devno, NULL, "ext%d", edrv->index);
	if (IS_ERR_OR_NULL(edrv->sub_dev)) {
		ret = 3;
		goto ext_cdev_add_failed1;
	}

	dev_set_drvdata(edrv->sub_dev, edrv);
	edrv->sub_dev->of_node = parent->of_node;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		EXTPR("[%d]: %s OK\n", edrv->index, __func__);
	return 0;

ext_cdev_add_failed1:
	cdev_del(&edrv->cdev);
ext_cdev_add_failed:
	EXTERR("[%d]: %s: failed: %d\n", edrv->index, __func__, ret);
	return -1;
}

static void ext_cdev_remove(struct lcd_extern_driver_s *edrv)
{
	dev_t devno;

	if (!ext_cdev || !edrv)
		return;

	devno = MKDEV(MAJOR(ext_cdev->devno), edrv->index);
	device_destroy(ext_cdev->class, devno);
	cdev_del(&edrv->cdev);
}

static int ext_global_init_once(void)
{
	int ret;

	if (ext_global_init_flag) {
		ext_global_init_flag++;
		return 0;
	}
	ext_global_init_flag++;

	ext_cdev = kzalloc(sizeof(*ext_cdev), GFP_KERNEL);
	if (!ext_cdev)
		return -1;

	ret = alloc_chrdev_region(&ext_cdev->devno, 0,
				  LCD_MAX_DRV, EXT_CDEV_NAME);
	if (ret) {
		ret = 1;
		goto ext_global_init_once_err;
	}

	ext_cdev->class = class_create(THIS_MODULE, "lcd_ext");
	if (IS_ERR_OR_NULL(ext_cdev->class)) {
		ret = 2;
		goto ext_global_init_once_err_1;
	}

	return 0;

ext_global_init_once_err_1:
	unregister_chrdev_region(ext_cdev->devno, LCD_MAX_DRV);
ext_global_init_once_err:
	kfree(ext_cdev);
	ext_cdev = NULL;
	EXTERR("%s: failed: %d\n", __func__, ret);
	return -1;
}

static void ext_global_remove_once(void)
{
	if (ext_global_init_flag > 1) {
		ext_global_init_flag--;
		return;
	}
	ext_global_init_flag--;

	if (!ext_cdev)
		return;

	class_destroy(ext_cdev->class);
	unregister_chrdev_region(ext_cdev->devno, LCD_MAX_DRV);
	kfree(ext_cdev);
	ext_cdev = NULL;
}

/* **************************************** */
static int aml_lcd_extern_probe(struct platform_device *pdev)
{
	struct lcd_extern_driver_s *edrv;
	int index = 0;
	int ret;

	ext_global_init_once();

	if (!pdev->dev.of_node)
		return -1;
	ret = of_property_read_u32(pdev->dev.of_node, "index", &index);
	if (ret) {
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			EXTPR("%s: no index exist, default to 0\n", __func__);
		index = 0;
	}
	if (index >= LCD_MAX_DRV) {
		EXTERR("%s: invalid index %d\n", __func__, index);
		return -1;
	}
	if (ext_drv_init_state & (1 << index)) {
		EXTERR("%s: index %d driver already registered\n",
		       __func__, index);
		return -1;
	}
	ext_drv_init_state |= (1 << index);

	edrv = kzalloc(sizeof(*edrv), GFP_KERNEL);
	if (!edrv)
		return -ENOMEM;
	edrv->index = index;
	ext_driver[index] = edrv;

	/* set drvdata */
	platform_set_drvdata(pdev, edrv);
	ext_cdev_add(edrv, &pdev->dev);
	edrv->pdev = pdev;
	edrv->dev_cnt = 0;

	INIT_DELAYED_WORK(&edrv->dev_probe_dly_work, lcd_extern_dev_probe_work);
	ret = lcd_extern_config_load(edrv);
	if (ret)
		goto lcd_extern_probe_err;

	lcd_extern_debug_file_creat(edrv);

	EXTPR("[%d]: probe OK, init_state:0x%x\n", index, ext_drv_init_state);
	return 0;

lcd_extern_probe_err:
	/* free drvdata */
	platform_set_drvdata(pdev, NULL);
	/* free drv */
	kfree(edrv);
	ext_driver[index] = NULL;
	ext_drv_init_state &= ~(1 << index);
	EXTPR("[%d]: %s: failed\n", index, __func__);
	return -1;
}

static int aml_lcd_extern_remove(struct platform_device *pdev)
{
	struct lcd_extern_driver_s *edrv = platform_get_drvdata(pdev);
	int index, i;

	if (!edrv)
		return 0;

	index = edrv->index;

	cancel_delayed_work(&edrv->dev_probe_dly_work);
	lcd_extern_debug_file_remove(edrv);
	ext_cdev_remove(edrv);

	platform_set_drvdata(pdev, NULL);
	for (i = 0; i < edrv->dev_cnt; i++)
		lcd_extern_dev_free(edrv->dev[i]);
	kfree(edrv);
	ext_driver[index] = NULL;

	ext_drv_init_state &= ~(1 << index);
	ext_global_remove_once();

	EXTPR("[%d]: %s, init_state:0x%x\n",
		index, __func__, ext_drv_init_state);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id aml_lcd_extern_dt_match[] = {
	{
		.compatible = "amlogic, lcd_extern",
	},
	{},
};
#endif

static struct platform_driver aml_lcd_extern_driver = {
	.probe  = aml_lcd_extern_probe,
	.remove = aml_lcd_extern_remove,
	.driver = {
		.name  = "lcd_extern",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = aml_lcd_extern_dt_match,
#endif
	},
};

int __init aml_lcd_extern_init(void)
{
	int ret;

	ret = platform_driver_register(&aml_lcd_extern_driver);
	if (ret) {
		EXTERR("driver register failed\n");
		return -ENODEV;
	}
	return ret;
}

void __exit aml_lcd_extern_exit(void)
{
	platform_driver_unregister(&aml_lcd_extern_driver);
}

//MODULE_AUTHOR("AMLOGIC");
//MODULE_DESCRIPTION("LCD extern driver");
//MODULE_LICENSE("GPL");

