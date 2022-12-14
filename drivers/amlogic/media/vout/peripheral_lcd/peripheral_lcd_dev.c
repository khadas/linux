// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/device.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/spinlock.h>
#include <linux/irq.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/amlogic/media/vout/peripheral_lcd.h>
#include "peripheral_lcd_dev.h"
#include "peripheral_lcd_drv.h"

static struct spi_board_info per_lcd_spi_info = {
	.modalias = "peripheral_lcd",
	.mode = SPI_MODE_0,
	.max_speed_hz = 1000000, /* 1MHz */
	.bus_num = 0, /* SPI bus No. */
	.chip_select = 0, /* the device index on the spi bus */
	.controller_data = NULL,
};

static int per_lcd_dev_probe_flag;
static unsigned char *table_init_on_dft;
static unsigned char *table_init_off_dft;

static struct per_lcd_dev_config_s per_lcd_dev_conf = {
	.init_loaded = 0,
	.cs_hold_delay = 0,
	.cs_clk_delay = 0,
	.cmd_size = 4,
	.init_on = NULL,
	.init_off = NULL,
	.init_on_cnt = 0,
	.init_off_cnt = 0,
	.name = "none",
};

static int per_lcd_dev_init_table_dynamic_load(struct device_node *of_node,
		struct per_lcd_dev_config_s *per_lcd_dev_conf, int flag)
{
	unsigned char cmd_size, type = 0;
	int i = 0, j, val, max_len, step = 0, ret = 0;
	unsigned char *table;
	char propname[20];

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
			LCDERR("%s: get %s type failed, step %d\n",
			       per_lcd_dev_conf->name, propname, step);
			table[i] = PER_LCD_CMD_TYPE_END;
			table[i + 1] = 0;
			return -1;
		}
		table[i] = (unsigned char)val;
		type = table[i];
		/* cmd_size */
		ret = of_property_read_u32_index(of_node, propname, (i + 1), &val);
		if (ret) {
			LCDERR("%s: get %s cmd_size failed, step %d\n",
			       per_lcd_dev_conf->name, propname, step);
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
			LCDERR("%s: %s cmd_size out of support, step %d\n",
			       per_lcd_dev_conf->name, propname, step);
			table[i] = PER_LCD_CMD_TYPE_END;
			table[i + 1] = 0;
			return -1;
		}

		/* data */
		for (j = 0; j < cmd_size; j++) {
			ret = of_property_read_u32_index(of_node, propname, (i + 2 + j), &val);
			if (ret) {
				LCDERR("%s: get %s data failed, step %d\n",
				       per_lcd_dev_conf->name, propname, step);
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
		per_lcd_dev_conf->init_on_cnt = i + 2;
	else
		per_lcd_dev_conf->init_off_cnt = i + 2;

	return 0;
}

static int per_lcd_dev_init_table_fixed_load(struct device_node *of_node,
		struct per_lcd_dev_config_s *per_lcd_dev_conf, int flag)
{
	unsigned char cmd_size;
	int i = 0, j, val, max_len, step = 0, ret = 0;
	unsigned char *table;
	char propname[20];

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
	cmd_size = per_lcd_dev_conf->cmd_size;
	while (i < max_len) {
		/* group detect */
		if ((i + cmd_size) > max_len) {
			LCDERR("%s: %s cmd_size out of support, step %d\n",
			       per_lcd_dev_conf->name, propname, step);
			table[i] = PER_LCD_CMD_TYPE_END;
			return -1;
		}
		for (j = 0; j < cmd_size; j++) {
			ret = of_property_read_u32_index(of_node, propname,
							 (i + j), &val);
			if (ret) {
				LCDERR("%s: get %s failed, step %d\n",
				       per_lcd_dev_conf->name, propname, step);
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
		per_lcd_dev_conf->init_on_cnt = i + cmd_size;
	else
		per_lcd_dev_conf->init_off_cnt = i + cmd_size;

	return 0;
}

static int per_lcd_dev_tablet_init_dft_malloc(void)
{
	table_init_on_dft = kcalloc(PER_INIT_ON_MAX,
				    sizeof(unsigned char), GFP_KERNEL);
	if (!table_init_on_dft) {
		LCDERR("failed to alloc init_on table\n");
		return -1;
	}
	table_init_off_dft = kcalloc(PER_INIT_OFF_MAX,
				     sizeof(unsigned char), GFP_KERNEL);
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

static int per_lcd_dev_table_init_save(struct per_lcd_dev_config_s *per_lcd_dev_conf)
{
	if (per_lcd_dev_conf->init_on_cnt > 0) {
		per_lcd_dev_conf->init_on =
			kcalloc(per_lcd_dev_conf->init_on_cnt,
				sizeof(unsigned char), GFP_KERNEL);
		if (!per_lcd_dev_conf->init_on) {
			LCDERR("failed to alloc init_on table\n");
			return -1;
		}
		memcpy(per_lcd_dev_conf->init_on, table_init_on_dft,
		       per_lcd_dev_conf->init_on_cnt * sizeof(unsigned char));
	}
	if (per_lcd_dev_conf->init_off_cnt > 0) {
		per_lcd_dev_conf->init_off =
			kcalloc(per_lcd_dev_conf->init_off_cnt,
				sizeof(unsigned char), GFP_KERNEL);
		if (!per_lcd_dev_conf->init_off) {
			LCDERR("failed to alloc init_off table\n");
			kfree(per_lcd_dev_conf->init_on);
			return -1;
		}
		memcpy(per_lcd_dev_conf->init_off, table_init_off_dft,
		       per_lcd_dev_conf->init_off_cnt * sizeof(unsigned char));
	}

	return 0;
}

static int per_lcd_dev_get_config_from_dts(struct peripheral_lcd_driver_s *per_lcd_drv)
{
	struct device_node *np = per_lcd_drv->dev->of_node;
	struct device_node *child;
	char per_lcd_propname[20];
	unsigned int index, para[20], val;
	const char *str;
	int ret, i;

	/* get device config */
	index = per_lcd_drv->dev_index;
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
		str = "peripheral_lcd_dev";
	}
	strncpy(per_lcd_dev_conf.name, str, sizeof(per_lcd_dev_conf.name));
	per_lcd_dev_conf.name[sizeof(per_lcd_dev_conf.name) - 1] = '\0';

	ret = of_property_read_u32_array(child, "resolution", para, 2);
	if (ret) {
		LCDERR("failed to get peripheral_lcd col_row\n");
	} else {
		per_lcd_dev_conf.col = (unsigned short)para[0];
		per_lcd_dev_conf.row = (unsigned short)para[1];
	}
	LCDPR("get peripheral_lcd col = %d, row = %d\n",
	      per_lcd_dev_conf.col, per_lcd_dev_conf.row);

	ret = of_property_read_u32_array(child, "per_lcd_attr", para, 6);
	if (ret) {
		LCDERR("failed to get peripheral_lcd_attr\n");
	} else {
		per_lcd_dev_conf.data_format = para[0];
		per_lcd_dev_conf.color_format = para[1];
	}
	LCDPR("get peripheral_lcd data_format = %d, color_format = %d\n",
	      per_lcd_dev_conf.data_format, per_lcd_dev_conf.color_format);

	ret = of_property_read_u32(child, "type", para);
	if (ret) {
		LCDERR("failed to get type\n");
		per_lcd_dev_conf.type = PER_DEV_TYPE_NORMAL;
	} else {
		per_lcd_dev_conf.type = para[0];
		if (per_lcd_debug_flag)
			LCDPR("type: %d\n", per_lcd_dev_conf.type);
	}
	if (per_lcd_dev_conf.type >= PER_DEV_TYPE_MAX) {
		LCDERR("type num is out of support\n");
		return -1;
	}
	ret = per_lcd_dev_tablet_init_dft_malloc();
	if (ret)
		return -1;
	switch (per_lcd_dev_conf.type) {
	case PER_DEV_TYPE_SPI:
		/* get spi config */
		per_lcd_drv->spi_info = &per_lcd_spi_info;

		ret = of_property_read_u32(child, "spi_bus_num", &val);
		if (ret) {
			LCDERR("failed to get spi_bus_num\n");
		} else {
			per_lcd_spi_info.bus_num = val;
			if (per_lcd_debug_flag)
				LCDPR("spi bus_num: %d\n", per_lcd_spi_info.bus_num);
		}

		ret = of_property_read_u32(child, "spi_chip_select", &val);
		if (ret) {
			LCDERR("failed to get spi_chip_select\n");
		} else {
			per_lcd_spi_info.chip_select = val;
			if (per_lcd_debug_flag) {
				LCDPR("spi chip_select: %d\n",
				      per_lcd_spi_info.chip_select);
			}
		}

		ret = of_property_read_u32(child, "spi_max_frequency", &val);
		if (ret) {
			LCDERR("failed to get spi_chip_select\n");
		} else {
			per_lcd_spi_info.max_speed_hz = val;
			if (per_lcd_debug_flag) {
				LCDPR("spi max_speed_hz: %d\n",
				      per_lcd_spi_info.max_speed_hz);
			}
		}

		ret = of_property_read_u32(child, "spi_mode", &val);
		if (ret) {
			LCDERR("failed to get spi_mode\n");
		} else {
			per_lcd_spi_info.mode = val;
			if (per_lcd_debug_flag)
				LCDPR("spi mode: %d\n", per_lcd_spi_info.mode);
		}

		ret = of_property_read_u32_array(child, "spi_cs_delay", para, 2);
		if (ret) {
			per_lcd_dev_conf.cs_hold_delay = 0;
			per_lcd_dev_conf.cs_clk_delay = 0;
		} else {
			per_lcd_dev_conf.cs_hold_delay = para[0];
			per_lcd_dev_conf.cs_clk_delay = para[1];
		}
		break;
	case PER_DEV_TYPE_MCU_8080:
		ret = of_property_read_u32(child, "max_gpio_num", para);
		if (ret) {
			LCDERR("failed to get max_gpio_num\n");
			per_lcd_dev_conf.max_gpio_num = 0;
		} else {
			per_lcd_dev_conf.max_gpio_num = para[0];
			for (i = 0; i < per_lcd_dev_conf.max_gpio_num; i++)
				per_lcd_gpio_probe(i);
			if (per_lcd_debug_flag)
				LCDPR("max_gpio_num: %d\n",
				      per_lcd_dev_conf.max_gpio_num);
		}
		ret = of_property_read_u32_array(child, "mcu_attr", para, 13);
		if (ret) {
			per_lcd_dev_conf.reset_index = 0;
			per_lcd_dev_conf.nCS_index = 0;
			per_lcd_dev_conf.nRD_index = 0;
			per_lcd_dev_conf.nWR_index = 0;
			per_lcd_dev_conf.nRS_index = 0;
			per_lcd_dev_conf.data0_index = 0;
			per_lcd_dev_conf.data1_index = 0;
			per_lcd_dev_conf.data2_index = 0;
			per_lcd_dev_conf.data3_index = 0;
			per_lcd_dev_conf.data4_index = 0;
			per_lcd_dev_conf.data5_index = 0;
			per_lcd_dev_conf.data6_index = 0;
			per_lcd_dev_conf.data7_index = 0;
		} else {
			per_lcd_dev_conf.reset_index = para[0];
			per_lcd_dev_conf.nCS_index = para[1];
			per_lcd_dev_conf.nRD_index = para[2];
			per_lcd_dev_conf.nWR_index = para[3];
			per_lcd_dev_conf.nRS_index = para[4];
			per_lcd_dev_conf.data0_index = para[5];
			per_lcd_dev_conf.data1_index = para[6];
			per_lcd_dev_conf.data2_index = para[7];
			per_lcd_dev_conf.data3_index = para[8];
			per_lcd_dev_conf.data4_index = para[9];
			per_lcd_dev_conf.data5_index = para[10];
			per_lcd_dev_conf.data6_index = para[11];
			per_lcd_dev_conf.data7_index = para[12];
		}
		break;
	case PER_DEV_TYPE_NORMAL:
	default:
		break;
	}
		/* get init_cmd */
	ret = of_property_read_u32(child, "cmd_size", &val);
	if (ret) {
		LCDPR("no cmd_size\n");
		per_lcd_dev_conf.cmd_size = 0;
		goto per_lcd_get_config_init_table_err;
	} else {
		per_lcd_dev_conf.cmd_size = (unsigned char)val;
	}
	if (per_lcd_debug_flag) {
		LCDPR("%s: cmd_size = %d\n",
		      per_lcd_dev_conf.name,
		      per_lcd_dev_conf.cmd_size);
	}
	if (per_lcd_dev_conf.cmd_size == PER_LCD_CMD_SIZE_DYNAMIC) {
		ret = per_lcd_dev_init_table_dynamic_load(child, &per_lcd_dev_conf, 1);
		if (ret)
			goto per_lcd_get_config_init_table_err;
		per_lcd_dev_init_table_dynamic_load(child, &per_lcd_dev_conf, 0);
	} else {
		ret = per_lcd_dev_init_table_fixed_load(child, &per_lcd_dev_conf, 1);
		if (ret)
			goto per_lcd_get_config_init_table_err;
		per_lcd_dev_init_table_fixed_load(child, &per_lcd_dev_conf, 0);
	}
	if (ret == 0)
		per_lcd_dev_conf.init_loaded = 1;

	if (per_lcd_dev_conf.init_loaded > 0) {
		ret = per_lcd_dev_table_init_save(&per_lcd_dev_conf);
		if (ret)
			goto per_lcd_get_config_init_table_err;
	}

	kfree(table_init_on_dft);
	kfree(table_init_off_dft);
	return 0;
per_lcd_get_config_init_table_err:
	kfree(table_init_on_dft);
	kfree(table_init_off_dft);
	return -1;
}

static int per_lcd_dev_add_driver(struct peripheral_lcd_driver_s *per_lcd_drv)
{
	int index = per_lcd_drv->dev_index;
	int ret = 0;

	switch (per_lcd_dev_conf.type) {
	case PER_DEV_TYPE_SPI:
		ret = per_lcd_spi_driver_add(per_lcd_drv);
		break;
	case PER_DEV_TYPE_MCU_8080:
		break;
	case PER_DEV_TYPE_NORMAL:
	default:
		break;
	}

	if (ret)
		return ret;

	ret = -1;

	if (strcmp(per_lcd_dev_conf.name, "intel_8080") == 0)
		ret = per_lcd_dev_8080_probe(per_lcd_drv);
	else if (strcmp(per_lcd_dev_conf.name, "spi_st7789") == 0)
		ret = per_lcd_dev_spi_probe(per_lcd_drv);
	else
		LCDPR("%s:unsupported device driver: %s(%d)\n",
		      __func__, per_lcd_dev_conf.name, index);

	if (ret) {
		LCDPR("add device driver failed: %s(%d)\n",
		      per_lcd_dev_conf.name, index);
	} else {
		per_lcd_dev_probe_flag = 1;
		LCDPR("add device driver: %s(%d)\n",
		      per_lcd_dev_conf.name, index);
	}
	return ret;
}

static void per_lcd_dev_remove_driver(struct peripheral_lcd_driver_s *per_lcd_drv)
{
	int index = per_lcd_drv->dev_index;
	int ret = -1;

	if ((strcmp(per_lcd_dev_conf.name, "intel_8080") == 0) ||
	    (strcmp(per_lcd_dev_conf.name, "8080") == 0))
		ret = per_lcd_dev_8080_remove(per_lcd_drv);
	else if (strcmp(per_lcd_dev_conf.name, "spi_st7789") == 0)
		ret = per_lcd_dev_spi_remove(per_lcd_drv);
	else
		LCDPR("%s: unsupported device driver: %s(%d)\n",
		      __func__, per_lcd_dev_conf.name, index);

	if (ret) {
		LCDPR("remove device driver failed: %s(%d)\n",
		      per_lcd_dev_conf.name, index);
	} else {
		per_lcd_dev_probe_flag = 0;
		LCDPR("remove device driver: %s(%d)\n",
		      per_lcd_dev_conf.name, index);
	}

	switch (per_lcd_dev_conf.type) {
	case PER_DEV_TYPE_SPI:
		per_lcd_spi_driver_remove(per_lcd_drv);
		break;
	case PER_DEV_TYPE_MCU_8080:
		break;
	case PER_DEV_TYPE_NORMAL:
	default:
		break;
	}
}

static void per_lcd_config_update_dynamic_size(struct peripheral_lcd_driver_s *edrv, int flag)
{
	unsigned char type, size, *table;
	unsigned int max_len = 0, i = 0, index;

	if (flag) {
		max_len = edrv->per_lcd_dev_conf->init_on_cnt;
		table = edrv->per_lcd_dev_conf->init_on;
	} else {
		max_len = edrv->per_lcd_dev_conf->init_off_cnt;
		table = edrv->per_lcd_dev_conf->init_on;
	}

	while ((i + 1) < max_len) {
		type = table[i];
		size = table[i + 1];
		if (type == PER_LCD_CMD_TYPE_END)
			break;
		if (size == 0)
			goto per_lcd_config_update_dynamic_size_next;
		if ((i + 2 + size) > max_len)
			break;

		if (type == PER_LCD_CMD_TYPE_GPIO) {
			/* gpio probe */
			index = table[i + 2];
			if (index < PER_GPIO_MAX)
				per_lcd_gpio_probe(index);
		}
per_lcd_config_update_dynamic_size_next:
		i += (size + 2);
	}
}

static void per_lcd_config_update_fixed_size(struct peripheral_lcd_driver_s *edrv, int flag)
{
	int i = 0, max_len = 0;
	unsigned char type, cmd_size, index;
	unsigned char *table;

	cmd_size = edrv->per_lcd_dev_conf->cmd_size;
	if (cmd_size < 2) {
		LCDPR("[%d]: %s: dev_%d invalid cmd_size %d\n",
				edrv->dev_index, __func__, edrv->dev_index, cmd_size);
		return;
	}

	if (flag) {
		max_len = edrv->per_lcd_dev_conf->init_on_cnt;
		table = edrv->per_lcd_dev_conf->init_on;
	} else {
		max_len = edrv->per_lcd_dev_conf->init_off_cnt;
		table = edrv->per_lcd_dev_conf->init_on;
	}

	while ((i + cmd_size) <= max_len) {
		type = table[i];
		if (type == PER_LCD_CMD_TYPE_END)
			break;
		if (type == PER_LCD_CMD_TYPE_GPIO) {
			/* gpio probe */
			index = table[i + 1];
			if (index < PER_GPIO_MAX)
				per_lcd_gpio_probe(index);
		}
		i += cmd_size;
	}
}

static void per_lcd_config_update(struct peripheral_lcd_driver_s *per_drv)
{
	if (per_drv->per_lcd_dev_conf->cmd_size == PER_LCD_CMD_SIZE_DYNAMIC)	{
		LCDPR("%s dynamic\n", __func__);
		per_lcd_config_update_dynamic_size(per_drv, 1);
		per_lcd_config_update_dynamic_size(per_drv, 0);
	} else {
		LCDPR("%s fixed\n", __func__);
		per_lcd_config_update_fixed_size(per_drv, 1);
		per_lcd_config_update_fixed_size(per_drv, 0);
	}
}

int perl_lcd_dev_probe(struct peripheral_lcd_driver_s *per_lcd_drv)
{
	int ret;

	per_lcd_drv->per_lcd_dev_conf = &per_lcd_dev_conf;
	ret = per_lcd_dev_get_config_from_dts(per_lcd_drv);
	if (ret) {
		LCDERR("%s: get dts config error\n", __func__);
		per_lcd_drv->per_lcd_dev_conf = NULL;
		return -1;
	}

	per_lcd_config_update(per_lcd_drv);

	ret = per_lcd_dev_add_driver(per_lcd_drv);
	LCDPR("%s: ret=%d\n", __func__, ret);
	return ret;
}

void per_lcd_dev_remove(struct peripheral_lcd_driver_s *per_lcd_drv)
{
	if (!per_lcd_drv->per_lcd_dev_conf)
		return;

	per_lcd_dev_remove_driver(per_lcd_drv);
	LCDPR("%s OK\n", __func__);
}
