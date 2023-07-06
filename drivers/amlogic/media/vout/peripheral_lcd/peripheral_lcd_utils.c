// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/types.h>
#include <asm/delay.h>
#include <linux/delay.h>
#include <linux/spi/spi.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/amlogic/media/vout/peripheral_lcd.h>
#include "peripheral_lcd_drv.h"

extern struct per_gpio_s plcd_gpio[PER_GPIO_NUM_MAX];

static unsigned char *table_init_on_dft;
static unsigned char *table_init_off_dft;

unsigned int rgb565_color_data[8] = {
	0xffff,   0xf800,   0x7e0,    0x001f,   0x7ff,    0xf81f,   0xffe0,   0x0000};
unsigned int rgb666_color_data[8] = {
	0x3ffff,  0x3f000,  0x00fc0,  0x0003f,  0x00fff,  0x3f03f,  0x3ffc0,  0x00000};
unsigned int rgb888_color_data[8] = {
	0xffffff, 0xff0000, 0x00ff00, 0x0000ff, 0x00ffff, 0xff00ff, 0xffff00, 0x000000};

inline void plcd_usleep(unsigned int us)
{
	if (!us)
		return;
	else if (us < 20)
		udelay(us);
	else if (us < 20000)
		usleep_range(us, us + 10);
	else
		msleep_interruptible(us / 1000);
}

void plcd_gpio_probe(unsigned int index)
{
	const char *str;
	int ret;

	if (index >= PER_GPIO_NUM_MAX) {
		LCDERR("gpio index %d, exit\n", index);
		return;
	}

	if (plcd_gpio[index].probe_flag) {
		if (per_lcd_debug_flag)
			LCDPR("gpio %s[%d] is already registered\n", plcd_gpio[index].name, index);
		return;
	}

	/* get gpio name */
	ret = of_property_read_string_index(plcd_drv->dev->of_node,
					    "per_lcd_gpio_names", index, &str);
	if (ret) {
		LCDERR("failed to get lcd_per_gpio_names: %d\n", index);
		str = "unknown";
	}
	strcpy(plcd_gpio[index].name, str);

	/* init gpio flag */
	plcd_gpio[index].probe_flag = 1;
	plcd_gpio[index].register_flag = 0;
}

int plcd_gpio_regist(int index, int init_value)
{
	int value;

	if (index >= PER_GPIO_NUM_MAX) {
		LCDERR("%s: gpio index %d, exit\n", __func__, index);
		return -1;
	}
	if (plcd_gpio[index].probe_flag == 0) {
		LCDERR("%s: gpio [%d] is not probed, exit\n", __func__, index);
		return -1;
	}
	if (plcd_gpio[index].register_flag) {
		if (per_lcd_debug_flag) {
			LCDPR("%s: gpio %s[%d] is already registered\n",
			      __func__, plcd_gpio[index].name, index);
		}
		return 0;
	}

	switch (init_value) {
	case PER_GPIO_OUTPUT_LOW:
		value = GPIOD_OUT_LOW;
		break;
	case PER_GPIO_OUTPUT_HIGH:
		value = GPIOD_OUT_HIGH;
		break;
	case PER_GPIO_INPUT:
	default:
		value = GPIOD_IN;
		break;
	}
	/* request gpio */
	plcd_gpio[index].gpio = devm_gpiod_get_index(plcd_drv->dev, "per_lcd", index, value);
	if (IS_ERR(plcd_gpio[index].gpio)) {
		LCDERR("register gpio %s[%d]: %p, err: %d\n", plcd_gpio[index].name, index,
			plcd_gpio[index].gpio, IS_ERR(plcd_gpio[index].gpio));
		return -1;
	}
	plcd_gpio[index].register_flag = 1;
	if (per_lcd_debug_flag) {
		LCDPR("register gpio %s[%d]: %p, init value: %d\n",
		      plcd_gpio[index].name, index, plcd_gpio[index].gpio, init_value);
	}

	return 0;
}

void plcd_gpio_set(int index, int value)
{
	if (per_lcd_debug_flag)
		LCDPR("%s: idx:val= [%d, %d]\n", __func__, index, value);
	if (index >= PER_GPIO_NUM_MAX) {
		LCDERR("gpio index %d, exit\n", index);
		return;
	}
	if (plcd_gpio[index].probe_flag == 0) {
		LCDERR("%s: gpio [%d] is not probed\n", __func__, index);
		return;
	}
	if (plcd_gpio[index].register_flag == 1) {
		if (per_lcd_debug_flag)
			LCDPR("no need regist\n");
	} else {
		plcd_gpio_regist(index, value);
		return;
	}

	if (IS_ERR_OR_NULL(plcd_gpio[index].gpio)) {
		LCDERR("gpio %s[%d]: %p, err: %ld\n", plcd_gpio[index].name, index,
			plcd_gpio[index].gpio, PTR_ERR(plcd_gpio[index].gpio));
		return;
	}

	switch (value) {
	case PER_GPIO_OUTPUT_LOW:
	case PER_GPIO_OUTPUT_HIGH:
		gpiod_direction_output(plcd_gpio[index].gpio, value);
		break;
	case PER_GPIO_INPUT:
	default:
		gpiod_direction_input(plcd_gpio[index].gpio);
		break;
	}
	if (per_lcd_debug_flag)
		LCDPR("set gpio %s[%d] value: %d\n", plcd_gpio[index].name, index, value);
}

int plcd_gpio_set_irq(int index)
{
	int irq_pin;

	if (index >= PER_GPIO_NUM_MAX) {
		LCDERR("gpio index %d, exit\n", index);
		return -1;
	}

	plcd_gpio_regist(index, 2);
	irq_pin = desc_to_gpio(plcd_gpio[index].gpio);
	plcd_drv->irq_num = gpio_to_irq(irq_pin);
	if (!plcd_drv->irq_num) {
		LCDERR("gpio to irq failed\n");
		return -1;
	}

	return 0;
}

int plcd_gpio_get(int index)
{
	if (index >= PER_GPIO_NUM_MAX) {
		LCDERR("gpio index %d, exit\n", index);
		return -1;
	}

	if (plcd_gpio[index].probe_flag == 0) {
		LCDERR("%s: gpio [%d] not probed, exit\n", __func__, index);
		return -1;
	}
	if (plcd_gpio[index].register_flag == 0) {
		LCDERR("%s: gpio %s[%d] not registered\n", __func__, plcd_gpio[index].name, index);
		return -1;
	}
	if (IS_ERR_OR_NULL(plcd_gpio[index].gpio)) {
		LCDERR("gpio %s[%d]: %p, err: %ld\n", plcd_gpio[index].name, index,
			plcd_gpio[index].gpio, PTR_ERR(plcd_gpio[index].gpio));
		return -1;
	}

	return gpiod_get_value(plcd_gpio[index].gpio);
}

static int plcd_init_table_dynamic_load(struct device_node *of_node,
		struct per_lcd_config_s *pconf, int flag)
{
	unsigned char cmd_size, type = 0;
	int i = 0, j, val, max_len, step = 0, ret = 0;
	unsigned char *table;
	char propname[9];

	if (flag) {
		table = table_init_on_dft;
		max_len = PER_INIT_ON_MAX;
		sprintf(propname, "init_on");
	} else {
		table = table_init_off_dft;
		max_len = PER_INIT_OFF_MAX;
		sprintf(propname, "init_off");
	}
	if (!table) {
		LCDERR("%s: init_table is null\n", __func__);
		return -1;
	}

	while ((i + 1) < max_len) {
		/* type */
		ret = of_property_read_u32_index(of_node, propname, i, &val);
		if (ret) {
			LCDERR("%s: get %s type failed, step %d\n", pconf->name, propname, step);
			table[i] = PER_LCD_CMD_TYPE_END;
			table[i + 1] = 0;
			return -1;
		}
		table[i] = (unsigned char)val;
		type = table[i];
		/* cmd_size */
		ret = of_property_read_u32_index(of_node, propname, (i + 1), &val);
		if (ret) {
			LCDERR("%s:get %s cmd_size fail, step %d\n", pconf->name, propname, step);
			table[i] = PER_LCD_CMD_TYPE_END;
			table[i + 1] = 0;
			return -1;
		}
		table[i + 1] = (unsigned char)val;
		cmd_size = table[i + 1];

		if (type == PER_LCD_CMD_TYPE_END)
			break;
		if (cmd_size == 0)
			goto init_table_dynamic_dts_next;
		if ((i + 2 + cmd_size) > max_len) {
			LCDERR("%s: %s cmd_size too much, step %d\n", pconf->name, propname, step);
			table[i] = PER_LCD_CMD_TYPE_END;
			table[i + 1] = 0;
			return -1;
		}
		/* data */
		for (j = 0; j < cmd_size; j++) {
			ret = of_property_read_u32_index(of_node, propname, (i + 2 + j), &val);
			if (ret) {
				LCDERR("%s: get %s failed, step %d\n", pconf->name, propname, step);
				table[i] = PER_LCD_CMD_TYPE_END;
				table[i + 1] = 0;
				return -1;
			}
			table[i + 2 + j] = (unsigned char)val;
		}
init_table_dynamic_dts_next:
		i += (cmd_size + 2);
		step++;
	}
	if (flag)
		pconf->init_on_cnt = i + 2;
	else
		pconf->init_off_cnt = i + 2;

	return 0;
}

static int plcd_init_table_fixed_load(struct device_node *of_node,
		struct per_lcd_config_s *pconf, int flag)
{
	unsigned char cmd_size;
	int i = 0, j, val, max_len, step = 0, ret = 0;
	unsigned char *table;
	char propname[9];

	if (flag) {
		table = table_init_on_dft;
		max_len = PER_INIT_ON_MAX;
		sprintf(propname, "init_on");
	} else {
		table = table_init_off_dft;
		max_len = PER_INIT_OFF_MAX;
		sprintf(propname, "init_off");
	}
	if (!table) {
		LCDERR("%s: init_table is null\n", __func__);
		return -1;
	}
	cmd_size = pconf->cmd_size;
	while (i < max_len) {
		/* group detect */
		if ((i + cmd_size) > max_len) {
			LCDERR("%s: %s cmd_size too much, step %d\n", pconf->name, propname, step);
			table[i] = PER_LCD_CMD_TYPE_END;
			return -1;
		}
		for (j = 0; j < cmd_size; j++) {
			ret = of_property_read_u32_index(of_node, propname, (i + j), &val);
			if (ret) {
				LCDERR("%s: get %s failed, step %d\n", pconf->name, propname, step);
				table[i] = PER_LCD_CMD_TYPE_END;
				return -1;
			}
			table[i + j] = (unsigned char)val;
		}
		if (table[i] == PER_LCD_CMD_TYPE_END)
			break;

		i += cmd_size;
		step++;
	}

	if (flag)
		pconf->init_on_cnt = i + cmd_size;
	else
		pconf->init_off_cnt = i + cmd_size;

	return 0;
}

static int plcd_init_table_dft_malloc(void)
{
	table_init_on_dft = kcalloc(PER_INIT_ON_MAX, sizeof(unsigned char), GFP_KERNEL);
	if (!table_init_on_dft) {
		LCDERR("failed to alloc init_on table\n");
		return -1;
	}
	table_init_off_dft = kcalloc(PER_INIT_OFF_MAX, sizeof(unsigned char), GFP_KERNEL);
	if (!table_init_off_dft) {
		LCDERR("failed to alloc init_off table\n");
		kfree(table_init_on_dft);
		return -1;
	}
	table_init_on_dft[0] = PER_LCD_CMD_TYPE_END;
	table_init_on_dft[1] = 0;
	table_init_off_dft[0] = PER_LCD_CMD_TYPE_END;
	table_init_off_dft[1] = 0;

	return 0;
}

int plcd_init_table_save(struct per_lcd_config_s *pconf)
{
	if (pconf->init_on_cnt > 0) {
		pconf->init_on = kcalloc(pconf->init_on_cnt, sizeof(unsigned char), GFP_KERNEL);
		if (!pconf->init_on) {
			LCDERR("failed to alloc init_on table\n");
			return -1;
		}
		memcpy(pconf->init_on, table_init_on_dft,
		       pconf->init_on_cnt * sizeof(unsigned char));
	}
	if (pconf->init_off_cnt > 0) {
		pconf->init_off = kcalloc(pconf->init_off_cnt, sizeof(unsigned char), GFP_KERNEL);
		if (!pconf->init_off) {
			LCDERR("failed to alloc init_off table\n");
			kfree(pconf->init_on);
			return -1;
		}
		memcpy(pconf->init_off, table_init_off_dft,
		       pconf->init_off_cnt * sizeof(unsigned char));
	}

	return 0;
}

int plcd_get_detail_config_dts(void)
{
	struct device_node *np = plcd_drv->dev->of_node;
	struct device_node *child;
	char per_lcd_propname[20];
	unsigned int index, para[20], val;
	const char *str;
	int ret, i;

	/* get device config */
	index = plcd_drv->dev_index;
	struct per_lcd_config_s *pcfg = plcd_drv->pcfg;

	sprintf(per_lcd_propname, "per_lcd_%d", index);
	LCDPR("load: %s\n", per_lcd_propname);
	child = of_get_child_by_name(np, per_lcd_propname);
	if (!child) {
		LCDERR("child device_node is null\n");
		return -1;
	}

	ret = of_property_read_string(child, "peripheral_lcd_dev_name", &str);
	if (ret) {
		LCDERR("failed to get peripheral_lcd_dev_name\n");
		return -1;
	}
	strncpy(pcfg->name, str, sizeof(pcfg->name));
	pcfg->name[sizeof(pcfg->name) - 1] = '\0';

	ret = of_property_read_u32_array(child, "resolution", para, 2);
	if (ret) {
		LCDERR("failed to get peripheral_lcd resolution\n");
		return -1;
	}
	pcfg->h = (unsigned short)para[0];
	pcfg->v = (unsigned short)para[1];
	LCDPR("get peripheral_lcd h = %d, v = %d\n", pcfg->h, pcfg->v);

	ret = of_property_read_u32_array(child, "per_lcd_attr", para, 6);
	if (ret)
		LCDERR("failed to get per_lcd_attr\n");
	pcfg->cfmt = ret ? CFMT_RGB888 : (unsigned char)para[0];
	LCDPR("get peripheral_lcd color_format = %d\n", pcfg->cfmt);

	ret = of_property_read_u32(child, "type", para);
	if (ret) {
		LCDERR("failed to get type\n");
		return -1;
	}
	pcfg->type = (unsigned char)para[0];
	LCDPR("type: %d\n", pcfg->type);

	if (pcfg->type >= PLCD_TYPE_MAX) {
		LCDERR("type num is out of support\n");
		return -1;
	}
	ret = plcd_init_table_dft_malloc();
	if (ret)
		return -1;
	switch (pcfg->type) {
	case PLCD_TYPE_SPI:
	case PLCD_TYPE_QSPI:
		/* get spi config */
		pcfg->spi_cfg.spi_info = kcalloc(1, sizeof(struct spi_board_info), GFP_KERNEL);

		ret = of_property_read_u32(child, "spi_bus_num", &val);
		if (ret) {
			LCDERR("failed to get spi_bus_num\n");
			return -1;
		}
		pcfg->spi_cfg.spi_info->bus_num = val;
		if (per_lcd_debug_flag)
			LCDPR("spi bus_num: %d\n", pcfg->spi_cfg.spi_info->bus_num);

		ret = of_property_read_u32(child, "spi_chip_select", &val);
		if (ret)
			LCDERR("failed to get spi_chip_select\n");
		pcfg->spi_cfg.spi_info->chip_select = ret ? 0 : val;
		if (per_lcd_debug_flag)
			LCDPR("spi chip_select: %d\n", pcfg->spi_cfg.spi_info->chip_select);

		ret = of_property_read_u32(child, "spi_max_frequency", &val);
		if (ret)
			LCDERR("failed to get spi_max_frequency\n");
		pcfg->spi_cfg.spi_info->max_speed_hz = ret ? 1000000 : val;
		if (per_lcd_debug_flag)
			LCDPR("spi max_speed_hz: %d\n", pcfg->spi_cfg.spi_info->max_speed_hz);

		ret = of_property_read_u32(child, "spi_mode", &val);
		if (ret)
			LCDERR("failed to get spi_mode\n");
		pcfg->spi_cfg.spi_info->mode = ret ? 0 : val;
		if (per_lcd_debug_flag)
			LCDPR("spi mode: %d\n", pcfg->spi_cfg.spi_info->mode);

		ret = of_property_read_u32_array(child, "spi_cs_delay", para, 2);
		pcfg->spi_cfg.cs_hold_delay = ret ? 0 : para[0];
		pcfg->spi_cfg.cs_clk_delay = ret ? 0 : para[1];
		break;

	case PLCD_TYPE_MCU_8080:
		ret = of_property_read_u32(child, "max_gpio_num", para);
		if (ret) {
			LCDERR("failed to get max_gpio_num\n");
			return -1;
		}
		pcfg->i8080_cfg.max_gpio_num = para[0];
		for (i = 0; i < pcfg->i8080_cfg.max_gpio_num; i++)
			plcd_gpio_probe(i);
		if (per_lcd_debug_flag)
			LCDPR("max_gpio_num: %d\n", pcfg->i8080_cfg.max_gpio_num);

		ret = of_property_read_u32_array(child, "mcu_attr", para, 13);
		pcfg->i8080_cfg.reset_index = ret ? 0 : para[0];
		pcfg->i8080_cfg.nCS_index   = ret ? 0 : para[1];
		pcfg->i8080_cfg.nRD_index   = ret ? 0 : para[2];
		pcfg->i8080_cfg.nWR_index   = ret ? 0 : para[3];
		pcfg->i8080_cfg.nRS_index   = ret ? 0 : para[4];
		pcfg->i8080_cfg.data0_index = ret ? 0 : para[5];
		pcfg->i8080_cfg.data1_index = ret ? 0 : para[6];
		pcfg->i8080_cfg.data2_index = ret ? 0 : para[7];
		pcfg->i8080_cfg.data3_index = ret ? 0 : para[8];
		pcfg->i8080_cfg.data4_index = ret ? 0 : para[9];
		pcfg->i8080_cfg.data5_index = ret ? 0 : para[10];
		pcfg->i8080_cfg.data6_index = ret ? 0 : para[11];
		pcfg->i8080_cfg.data7_index = ret ? 0 : para[12];
		break;
	case PLCD_TYPE_MAX:
	default:
		break;
	}
	/* get init_cmd */
	ret = of_property_read_u32(child, "cmd_size", &val);
	if (ret) {
		LCDPR("no cmd_size\n");
		pcfg->cmd_size = 0;
		goto per_lcd_get_config_init_table_err;
	} else {
		pcfg->cmd_size = (unsigned char)val;
	}
	if (per_lcd_debug_flag)
		LCDPR("%s: cmd_size = %d\n", pcfg->name, pcfg->cmd_size);

	if (pcfg->cmd_size == PER_LCD_CMD_SIZE_DYNAMIC) {
		ret = plcd_init_table_dynamic_load(child, pcfg, 1);
		if (ret)
			goto per_lcd_get_config_init_table_err;
		plcd_init_table_dynamic_load(child, pcfg, 0);
	} else {
		ret = plcd_init_table_fixed_load(child, pcfg, 1);
		if (ret)
			goto per_lcd_get_config_init_table_err;
		plcd_init_table_fixed_load(child, pcfg, 0);
	}
	plcd_drv->init_flag = 1;
	ret = plcd_init_table_save(pcfg);
	if (ret)
		goto per_lcd_get_config_init_table_err;

	kfree(table_init_on_dft);
	kfree(table_init_off_dft);
	return 0;
per_lcd_get_config_init_table_err:
	kfree(table_init_on_dft);
	kfree(table_init_off_dft);
	return -1;
}
