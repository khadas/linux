/*
 * drivers/amlogic/display/vout/lcd/lcd_extern/lcd_extern.c
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
#include <linux/amlogic/vout/lcd_extern.h>
#include "lcd_extern.h"

static struct device *lcd_extern_dev;
static int lcd_ext_driver_num;
static struct aml_lcd_extern_driver_s *lcd_ext_driver[LCD_EXT_DRIVER_MAX];

struct aml_lcd_extern_driver_s *aml_lcd_extern_get_driver(int index)
{
	int i;
	struct aml_lcd_extern_driver_s *ext_driver = NULL;

	if (index >= LCD_EXTERN_INDEX_INVALID) {
		EXTERR("invalid driver index: %d\n", index);
		return NULL;
	}
	for (i = 0; i < lcd_ext_driver_num; i++) {
		if (lcd_ext_driver[i]->config.index == index) {
			ext_driver = lcd_ext_driver[i];
			break;
		}
	}
	if (ext_driver == NULL)
		EXTERR("invalid driver index: %d\n", index);
	return ext_driver;
}

#if 0
static struct aml_lcd_extern_driver_s
	*aml_lcd_extern_get_driver_by_name(char *name)
{
	int i;
	struct aml_lcd_extern_driver_s *ext_driver = NULL;

	for (i = 0; i < lcd_ext_driver_num; i++) {
		if (strcmp(lcd_ext_driver[i]->config.name, name) == 0) {
			ext_driver = lcd_ext_driver[i];
			break;
		}
	}
	if (ext_driver == NULL)
		EXTERR("invalid driver name: %s\n", name);
	return ext_driver;
}
#endif

#ifdef CONFIG_OF
static struct gpio_desc *lcd_extern_gpio_register(struct device_node *ext_node,
		unsigned int index)
{
	struct gpio_desc *ext_gpio;
	const char *str;
	int ret;

	if (index >= LCD_EXTERN_GPIO_NUM_MAX) {
		EXTERR("gpio index %d, exit\n", index);
		return NULL;
	}

	/* get gpio name */
	ret = of_property_read_string_index(ext_node,
		"extern_gpio_names", index, &str);
	if (ret) {
		EXTERR("failed to get extern_gpio_names: %d\n", index);
		str = "unknown";
	}

	/* request gpio */
	ext_gpio = devm_gpiod_get_index(lcd_extern_dev, "extern", index);
	if (IS_ERR(ext_gpio)) {
		EXTERR("register gpio %s[%d]: %p, err: %ld\n",
			str, index, ext_gpio, IS_ERR(ext_gpio));
		ext_gpio = NULL;
	} else {
		if (lcd_debug_print_flag) {
			EXTPR("register gpio %s[%d]: %p\n",
				str, index, ext_gpio);
		}
	}

	return ext_gpio;
}
#endif

void lcd_extern_gpio_set(struct gpio_desc *ext_gpio, int value)
{
	if (ext_gpio == NULL)
		return;
	if (IS_ERR(ext_gpio)) {
		EXTERR("gpio: %p, err: %ld\n", ext_gpio, PTR_ERR(ext_gpio));
		return;
	}

	switch (value) {
	case LCD_GPIO_OUTPUT_LOW:
	case LCD_GPIO_OUTPUT_HIGH:
		gpiod_direction_output(ext_gpio, value);
		break;
	case LCD_GPIO_INPUT:
	default:
		gpiod_direction_input(ext_gpio);
		break;
	}
	if (lcd_debug_print_flag)
		EXTPR("set gpio value: %p = %d\n", ext_gpio, value);
}

unsigned int lcd_extern_gpio_get(struct gpio_desc *ext_gpio)
{
	if (ext_gpio == NULL)
		return -1;
	if (IS_ERR(ext_gpio)) {
		EXTERR("gpio: %p, err: %ld\n", ext_gpio, PTR_ERR(ext_gpio));
		return -1;
	}

	return gpiod_get_value(ext_gpio);
}

#ifdef CONFIG_OF
static int lcd_extern_get_i2c_bus(const char *str)
{
	int i2c_bus;

	if (strncmp(str, "i2c_bus_ao", 10) == 0)
		i2c_bus = AML_I2C_MASTER_AO;
	else if (strncmp(str, "i2c_bus_a", 9) == 0)
		i2c_bus = AML_I2C_MASTER_A;
	else if (strncmp(str, "i2c_bus_b", 9) == 0)
		i2c_bus = AML_I2C_MASTER_B;
	else if (strncmp(str, "i2c_bus_c", 9) == 0)
		i2c_bus = AML_I2C_MASTER_C;
	else if (strncmp(str, "i2c_bus_d", 9) == 0)
		i2c_bus = AML_I2C_MASTER_D;
	else {
		i2c_bus = AML_I2C_MASTER_A;
		EXTERR("invalid i2c_bus: %s\n", str);
	}

	return i2c_bus;
}

struct device_node *aml_lcd_extern_get_dt_child(int index)
{
	char propname[30];
	struct device_node *child;

	sprintf(propname, "extern_%d", index);
	child = of_get_child_by_name(lcd_extern_dev->of_node, propname);
	return child;
}

static int lcd_extern_get_dt_config(struct device_node *of_node,
		struct lcd_extern_config_s *extconf)
{
	int ret;
	int val;
	const char *str;

	ret = of_property_read_u32(of_node, "index", &extconf->index);
	if (ret) {
		EXTERR("get index failed, exit\n");
		return -1;
	}

	ret = of_property_read_string(of_node, "status", &str);
	if (ret) {
		EXTERR("get index %d status failed\n", extconf->index);
		return -1;
	} else {
		if (lcd_debug_print_flag)
			EXTPR("index %d status = %s\n", extconf->index, str);
		if (strncmp(str, "disabled", 3) == 0)
			return -1;
	}

	ret = of_property_read_string(of_node, "extern_name", &str);
	if (ret) {
		str = "none";
		EXTERR("get extern_name failed\n");
	}
	strcpy(extconf->name, str);
	EXTPR("load config: %s[%d]\n", extconf->name, extconf->index);

	ret = of_property_read_u32(of_node, "type", &extconf->type);
	if (ret) {
		extconf->type = LCD_EXTERN_MAX;
		EXTERR("get type failed, exit\n");
		return -1;
	}

	switch (extconf->type) {
	case LCD_EXTERN_I2C:
		ret = of_property_read_u32(of_node, "i2c_address",
			&extconf->i2c_addr);
		if (ret) {
			EXTERR("get %s i2c_address failed, exit\n",
				extconf->name);
			extconf->i2c_addr = 0;
			return -1;
		}
		if (lcd_debug_print_flag) {
			EXTPR("%s i2c_address=0x%02x\n",
				extconf->name, extconf->i2c_addr);
		}

		ret = of_property_read_string(of_node, "i2c_bus", &str);
		if (ret) {
			EXTERR("get %s i2c_bus failed, exit\n", extconf->name);
			extconf->i2c_bus = AML_I2C_MASTER_A;
			return -1;
		} else {
			extconf->i2c_bus = lcd_extern_get_i2c_bus(str);
		}
		if (lcd_debug_print_flag) {
			EXTPR("%s i2c_bus=%s[%d]\n",
				extconf->name, str, extconf->i2c_bus);
		}
		break;
	case LCD_EXTERN_SPI:
		ret = of_property_read_u32(of_node, "gpio_spi_cs", &val);
		if (ret) {
			EXTERR("get %s gpio_spi_cs failed, exit\n",
				extconf->name);
			extconf->spi_cs = NULL;
			return -1;
		} else {
			extconf->spi_cs = lcd_extern_gpio_register(of_node,
				val);
			if (lcd_debug_print_flag)
				EXTPR("spi_cs gpio: %p\n", extconf->spi_cs);
		}
		ret = of_property_read_u32(of_node, "gpio_spi_clk", &val);
		if (ret) {
			EXTERR("get %s gpio_spi_clk failed, exit\n",
				extconf->name);
			extconf->spi_clk = NULL;
			return -1;
		} else {
			extconf->spi_clk = lcd_extern_gpio_register(of_node,
				val);
			if (lcd_debug_print_flag)
				EXTPR("spi_clk gpio: %p\n", extconf->spi_clk);
		}
		ret = of_property_read_u32(of_node, "gpio_spi_data", &val);
		if (ret) {
			EXTERR("get %s gpio_spi_data failed, exit\n",
				extconf->name);
			extconf->spi_data = NULL;
			return -1;
		} else {
			extconf->spi_data = lcd_extern_gpio_register(of_node,
				val);
			if (lcd_debug_print_flag)
				EXTPR("spi_data gpio: %p\n", extconf->spi_data);
		}
		break;
	case LCD_EXTERN_MIPI:
		break;
	default:
		break;
	}

	return 0;
}
#endif

static int lcd_extern_add_i2c(struct aml_lcd_extern_driver_s *ext_drv)
{
	int ret = 0;

	if (strcmp(ext_drv->config.name, "i2c_T5800Q") == 0) {
#ifdef CONFIG_AML_LCD_EXTERN_I2C_T5800Q
		ret = aml_lcd_extern_i2c_T5800Q_probe(ext_drv);
#endif
	} else if (strcmp(ext_drv->config.name, "i2c_tc101") == 0) {
#ifdef CONFIG_AML_LCD_EXTERN_I2C_TC101
		ret = aml_lcd_extern_i2c_tc101_probe(ext_drv);
#endif
	} else if (strcmp(ext_drv->config.name, "i2c_anx6345") == 0) {
#ifdef CONFIG_AML_LCD_EXTERN_I2C_ANX6345
		ret = aml_lcd_extern_i2c_anx6345_probe(ext_drv);
#endif
	} else {
		EXTERR("invalid driver name: %s\n", ext_drv->config.name);
		ret = -1;
	}
	return ret;
}

static int lcd_extern_add_spi(struct aml_lcd_extern_driver_s *ext_drv)
{
	int ret = 0;

	if (strcmp(ext_drv->config.name, "spi_LD070WS2") == 0) {
#ifdef CONFIG_AML_LCD_EXTERN_SPI_LD070WS2
		ret = aml_lcd_extern_spi_LD070WS2_probe(ext_drv);
#endif
	} else {
		EXTERR("invalid driver name: %s\n", ext_drv->config.name);
		ret = -1;
	}
	return ret;
}

static int lcd_extern_add_mipi(struct aml_lcd_extern_driver_s *ext_drv)
{
	int ret = 0;

	if (strcmp(ext_drv->config.name, "mipi_N070ICN") == 0) {
#ifdef CONFIG_AML_LCD_EXTERN_MIPI_N070ICN
		ret = aml_lcd_extern_mipi_N070ICN_probe(ext_drv);
#endif
	} else if (strcmp(ext_drv->config.name, "mipi_KD080D13") == 0) {
#ifdef CONFIG_AML_LCD_EXTERN_MIPI_KD080D13
		ret = aml_lcd_extern_mipi_KD080D13_probe(ext_drv);
#endif
	} else {
		EXTERR("invalid driver name: %s\n", ext_drv->config.name);
		ret = -1;
	}
	return ret;
}

static int lcd_extern_add_invalid(struct aml_lcd_extern_driver_s *ext_drv)
{
	return -1;
}

static int lcd_extern_add_driver(struct lcd_extern_config_s *extconf)
{
	struct aml_lcd_extern_driver_s *ext_drv;
	int i;
	int ret = 0;

	if (lcd_ext_driver_num >= LCD_EXT_DRIVER_MAX) {
		EXTERR("driver num is out of support\n");
		return -1;
	}

	i = lcd_ext_driver_num;
	lcd_ext_driver[i] =
		kmalloc(sizeof(struct aml_lcd_extern_driver_s), GFP_KERNEL);
	if (lcd_ext_driver[i] == NULL) {
		EXTERR("failed to alloc driver %s[%d], not enough memory\n",
			extconf->name, extconf->index);
		return -1;
	}

	ext_drv = lcd_ext_driver[i];
	/* fill config parameters */
	ext_drv->config.index = extconf->index;
	strcpy(ext_drv->config.name, extconf->name);
	ext_drv->config.type = extconf->type;

	/* fill config parameters by different type */
	switch (ext_drv->config.type) {
	case LCD_EXTERN_I2C:
		ext_drv->config.i2c_addr = extconf->i2c_addr;
		ext_drv->config.i2c_bus = extconf->i2c_bus;
		ret = lcd_extern_add_i2c(ext_drv);
		break;
	case LCD_EXTERN_SPI:
		ext_drv->config.spi_cs = extconf->spi_cs;
		ext_drv->config.spi_clk = extconf->spi_clk;
		ext_drv->config.spi_data = extconf->spi_data;
		ret = lcd_extern_add_spi(ext_drv);
		break;
	case LCD_EXTERN_MIPI:
		ret = lcd_extern_add_mipi(ext_drv);
		break;
	default:
		ret = lcd_extern_add_invalid(ext_drv);
		EXTERR("don't support type %d\n", ext_drv->config.type);
		break;
	}
	if (ret) {
		EXTERR("add driver failed\n");
		kfree(lcd_ext_driver[i]);
		lcd_ext_driver[i] = NULL;
		return -1;
	}
	lcd_ext_driver_num++;
	EXTPR("add driver %s(%d)\n",
		ext_drv->config.name, ext_drv->config.index);
	return 0;
}

/* *********************************************************
 debug function
 ********************************************************* */
static void lcd_extern_config_dump(struct aml_lcd_extern_driver_s *ext_drv)
{
	if (ext_drv == NULL)
		return;

	EXTPR("driver %s(%d) info:\n",
		ext_drv->config.name, ext_drv->config.index);
	switch (ext_drv->config.type) {
	case LCD_EXTERN_I2C:
		pr_info("    type:        i2c(%d)\n", ext_drv->config.type);
		pr_info("    i2c_addr:    0x%02x\n"
			"    i2c_bus:     %d\n",
			ext_drv->config.i2c_addr, ext_drv->config.i2c_bus);
		break;
	case LCD_EXTERN_SPI:
		pr_info("    type:        spi(%d)\n", ext_drv->config.type);
		pr_info("    spi_cs:      %p\n"
			"    spi_clk:     %p\n"
			"    spi_data:    %p\n",
			ext_drv->config.spi_cs,
			ext_drv->config.spi_clk,
			ext_drv->config.spi_data);
		break;
	case LCD_EXTERN_MIPI:
		pr_info("    type:        mipi(%d)\n", ext_drv->config.type);
		break;
	default:
		break;
	}
	pr_info("\n");
}

static const char *lcd_extern_debug_usage_str = {
"Usage:\n"
"    echo index <n> > info ; dump specified index driver config\n"
"    echo all > info ; dump all driver config\n"
};

static ssize_t lcd_extern_debug_help(struct class *class,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", lcd_extern_debug_usage_str);
}

static ssize_t lcd_extern_info_dump(struct class *class,
		struct class_attribute *attr, const char *buf, size_t count)
{
	unsigned int ret;
	int i, index;
	struct aml_lcd_extern_driver_s *ext_drv;

	index = LCD_EXTERN_INDEX_INVALID;
	switch (buf[0]) {
	case 'i':
		ret = sscanf(buf, "index %d", &index);
		ext_drv = aml_lcd_extern_get_driver(index);
		lcd_extern_config_dump(ext_drv);
		break;
	case 'a':
		for (i = 0; i < lcd_ext_driver_num; i++)
			lcd_extern_config_dump(lcd_ext_driver[i]);
		break;
	default:
		EXTERR("invalid command\n");
		break;
	}

	if (ret != 1 || ret != 2)
		return -EINVAL;

	return count;
}

static struct class_attribute lcd_extern_class_attrs[] = {
	__ATTR(info, S_IRUGO | S_IWUSR,
		lcd_extern_debug_help, lcd_extern_info_dump),
};

static struct class *debug_class;
static int creat_lcd_extern_class(void)
{
	int i;

	debug_class = class_create(THIS_MODULE, "lcd_ext");
	if (IS_ERR(debug_class)) {
		EXTERR("create debug class failed\n");
		return -1;
	}

	for (i = 0; i < ARRAY_SIZE(lcd_extern_class_attrs); i++) {
		if (class_create_file(debug_class,
			&lcd_extern_class_attrs[i])) {
			EXTERR("create debug attribute %s failed\n",
				lcd_extern_class_attrs[i].attr.name);
		}
	}

	return 0;
}

static int remove_lcd_extern_class(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(lcd_extern_class_attrs); i++)
		class_remove_file(debug_class, &lcd_extern_class_attrs[i]);

	class_destroy(debug_class);
	debug_class = NULL;

	return 0;
}
/* ********************************************************* */

static int aml_lcd_extern_probe(struct platform_device *pdev)
{
	struct device_node *child;
	struct lcd_extern_config_s extconf;
	int ret = 0;

	lcd_extern_dev = &pdev->dev;
	lcd_ext_driver_num = 0;
#ifdef CONFIG_OF
	for_each_child_of_node(lcd_extern_dev->of_node, child) {
		ret = lcd_extern_get_dt_config(child, &extconf);
		if (ret == 0)
			lcd_extern_add_driver(&extconf);
	}
#endif
	creat_lcd_extern_class();

	EXTPR("%s ok\n", __func__);
	return 0;
}

static int aml_lcd_extern_remove(struct platform_device *pdev)
{
	int i;

	remove_lcd_extern_class();
	for (i = 0; i < lcd_ext_driver_num; i++) {
		kfree(lcd_ext_driver[i]);
		lcd_ext_driver[i] = NULL;
	}
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

static int __init aml_lcd_extern_init(void)
{
	int ret;
	if (lcd_debug_print_flag)
		EXTPR("%s\n", __func__);

	ret = platform_driver_register(&aml_lcd_extern_driver);
	if (ret) {
		EXTERR("driver register failed\n");
		return -ENODEV;
	}
	return ret;
}

static void __exit aml_lcd_extern_exit(void)
{
	platform_driver_unregister(&aml_lcd_extern_driver);
}

module_init(aml_lcd_extern_init);
module_exit(aml_lcd_extern_exit);

MODULE_AUTHOR("AMLOGIC");
MODULE_DESCRIPTION("LCD extern driver");
MODULE_LICENSE("GPL");

