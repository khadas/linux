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
#include "peripheral_lcd_drv.h"

struct per_gpio_s plcd_gpio[PER_GPIO_NUM_MAX];
static struct per_lcd_reg_map_s plcd_reg_map;

struct peripheral_lcd_driver_s *plcd_drv;
unsigned char per_lcd_debug_flag;

static void plcd_ioremap(struct platform_device *pdev)
{
	struct resource *res;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		LCDERR("%s: lcd_reg resource get error\n", __func__);
		return;
	}
	plcd_reg_map.base_addr = res->start;
	plcd_reg_map.size = resource_size(res);
	plcd_reg_map.p = devm_ioremap_nocache(&pdev->dev, res->start, plcd_reg_map.size);
	if (!plcd_reg_map.p) {
		plcd_reg_map.flag = 0;
		LCDERR("%s: reg map failed: 0x%x\n", __func__, plcd_reg_map.base_addr);
		return;
	}
	plcd_reg_map.flag = 1;
	LCDPR("%s: reg mapped: 0x%x -> %p size: 0x%x\n", __func__, plcd_reg_map.base_addr,
	      plcd_reg_map.p, plcd_reg_map.size);
}

static int plcd_get_basic_config_dts(struct platform_device *pdev)
{
	unsigned int para[5];
	int ret;

	if (!pdev->dev.of_node) {
		LCDERR("no per_lcd of_node exist\n");
		return -1;
	}
	/* get ldim_dev_index from dts */
	ret = of_property_read_u32(pdev->dev.of_node, "per_lcd_dev_index", &para[0]);
	if (ret)
		LCDERR("failed to get per_lcd_dev_index\n");
	else
		plcd_drv->dev_index = (unsigned char)para[0];
	if (plcd_drv->dev_index < 0xff)
		LCDPR("get peripheral_lcd_dev_index = %d\n", plcd_drv->dev_index);

	plcd_drv->res_vs_irq = platform_get_resource_byname(pdev,
		IORESOURCE_IRQ, "per_lcd_vsync");
	if (!plcd_drv->res_vs_irq)
		LCDPR("no per_lcd_vsync interrupts exist\n");

	return 0;
}

static int plcd_add_dev_driver(void)
{
	int index = plcd_drv->dev_index;
	int ret = 0xff;

	switch (plcd_drv->pcfg->type) {
	case PLCD_TYPE_SPI:
	case PLCD_TYPE_QSPI:
		if (plcd_spi_driver_add())
			return -1;
		if (strcmp(plcd_drv->pcfg->name, "spi_st7789") == 0)
			ret = plcd_st7789_probe();
		else if (strcmp(plcd_drv->pcfg->name, "TL015WVC01-H1650A") == 0)
			ret = plcd_spd2010_probe();
		break;
	case PLCD_TYPE_MCU_8080:
		if (strcmp(plcd_drv->pcfg->name, "intel_8080") == 0)
			ret = plcd_i8080_probe();
		break;
	case PLCD_TYPE_MAX:
	default:
		LCDPR("%s: unsupported dev type: %d\n", __func__, plcd_drv->pcfg->type);
		return -1;
	}

	if (ret == 0xff) {
		LCDPR("%s: add device driver failed: %s\n", __func__, plcd_drv->pcfg->name);
		return -1;
	}

	plcd_drv->probe_flag = 1;
	LCDPR("add device driver: %s(%d)\n", plcd_drv->pcfg->name, index);
	return 0;
}

static void plcd_remove_dev_driver(void)
{
	int index = plcd_drv->dev_index;
	int ret = -1;

	if ((strcmp(plcd_drv->pcfg->name, "intel_8080") == 0) ||
	    (strcmp(plcd_drv->pcfg->name, "8080") == 0))
		ret = plcd_i8080_remove();
	else if (strcmp(plcd_drv->pcfg->name, "spi_st7789") == 0)
		ret = plcd_st7789_remove();
	else if (strcmp(plcd_drv->pcfg->name, "TL015WVC01-H1650A") == 0)
		ret = plcd_spd2010_remove();
	else
		LCDPR("%s: unsupported device: %s(%d)\n", __func__, plcd_drv->pcfg->name, index);

	if (ret) {
		LCDPR("remove device driver failed: %s(%d)\n", plcd_drv->pcfg->name, index);
	} else {
		plcd_drv->probe_flag = 0;
		LCDPR("remove device driver: %s(%d)\n", plcd_drv->pcfg->name, index);
	}

	switch (plcd_drv->pcfg->type) {
	case PLCD_TYPE_SPI:
	case PLCD_TYPE_QSPI:
		plcd_spi_driver_remove();
		break;
	case PLCD_TYPE_MCU_8080:
		break;
	case PLCD_TYPE_MAX:
	default:
		break;
	}
}

static void plcd_config_update_dynamic_size(int flag)
{
	unsigned char type, size, *table;
	unsigned int max_len = 0, i = 0, index;

	if (flag) {
		max_len = plcd_drv->pcfg->init_on_cnt;
		table = plcd_drv->pcfg->init_on;
	} else {
		max_len = plcd_drv->pcfg->init_off_cnt;
		table = plcd_drv->pcfg->init_on;
	}

	while ((i + 1) < max_len) {
		type = table[i];
		size = table[i + 1];
		if (type == PER_LCD_CMD_TYPE_END)
			break;
		if (size == 0)
			goto plcd_config_update_dynamic_size_next;
		if ((i + 2 + size) > max_len)
			break;

		if (type == PER_LCD_CMD_TYPE_GPIO) {
			/* gpio probe */
			index = table[i + 2];
			if (index < PER_GPIO_MAX)
				plcd_gpio_probe(index);
		}
plcd_config_update_dynamic_size_next:
		i += (size + 2);
	}
}

static void plcd_config_update_fixed_size(int flag)
{
	int i = 0, max_len = 0;
	unsigned char type, cmd_size, index;
	unsigned char *table;

	cmd_size = plcd_drv->pcfg->cmd_size;
	if (cmd_size < 2) {
		LCDPR("[%d]: %s: invalid cmd_size %d\n", plcd_drv->dev_index, __func__, cmd_size);
		return;
	}

	if (flag) {
		max_len = plcd_drv->pcfg->init_on_cnt;
		table = plcd_drv->pcfg->init_on;
	} else {
		max_len = plcd_drv->pcfg->init_off_cnt;
		table = plcd_drv->pcfg->init_on;
	}

	while ((i + cmd_size) <= max_len) {
		type = table[i];
		if (type == PER_LCD_CMD_TYPE_END)
			break;
		if (type == PER_LCD_CMD_TYPE_GPIO) {
			/* gpio probe */
			index = table[i + 1];
			if (index < PER_GPIO_MAX)
				plcd_gpio_probe(index);
		}
		i += cmd_size;
	}
}

static void plcd_config_update(void)
{
	if (plcd_drv->pcfg->cmd_size == PER_LCD_CMD_SIZE_DYNAMIC)	{
		LCDPR("%s dynamic\n", __func__);
		plcd_config_update_dynamic_size(1);
		plcd_config_update_dynamic_size(0);
	} else {
		LCDPR("%s fixed\n", __func__);
		plcd_config_update_fixed_size(1);
		plcd_config_update_fixed_size(0);
	}
}

int plcd_dev_probe(void)
{
	int ret;

	ret = plcd_get_detail_config_dts();
	if (ret) {
		LCDERR("%s: get dts config error\n", __func__);
		plcd_drv->pcfg = NULL;
		return -1;
	}

	plcd_config_update();
	ret = plcd_add_dev_driver();
	LCDPR("%s: ret=%d\n", __func__, ret);
	return ret;
}

void plcd_dev_remove(void)
{
	if (!plcd_drv->pcfg)
		return;

	plcd_remove_dev_driver();
	LCDPR("%s OK\n", __func__);
}

struct peripheral_lcd_driver_s *peripheral_lcd_get_driver(void)
{
	return plcd_drv;
}

int plcd_set_mem(void)
{
	unsigned int f_size;

	if (plcd_drv->frame_addr) {
		LCDERR("%s: internal-host mem %px alloced, unset first!!!\n",
				__func__, plcd_drv->frame_addr);
		return -1;
	}

	if (plcd_drv->pcfg->cfmt == CFMT_RGB888 || plcd_drv->pcfg->cfmt == CFMT_RGB666_24B)
		f_size = plcd_drv->pcfg->v * plcd_drv->pcfg->h * 3;
	else if (plcd_drv->pcfg->cfmt == CFMT_RGB565)
		f_size = plcd_drv->pcfg->v * plcd_drv->pcfg->h * 2;
	else if (plcd_drv->pcfg->cfmt == CFMT_RGB666_18B)
		f_size = (plcd_drv->pcfg->v * plcd_drv->pcfg->h * 18 + 7) / 8;
	else
		f_size = plcd_drv->pcfg->v * plcd_drv->pcfg->h * 3;

	plcd_drv->frame_addr = kcalloc(f_size, sizeof(unsigned char), GFP_DMA);
	LCDPR("%s: internal-host mem(%d) -> %px\n", __func__, f_size, plcd_drv->frame_addr);
	if (plcd_drv->frame_addr)
		return 0;
	return -1;
}

void plcd_unset_mem(void)
{
	kfree(plcd_drv->frame_addr);
	plcd_drv->frame_addr = NULL;
	LCDPR("%s: unset internal-host mem\n", __func__);
}

static int plcd_probe(struct platform_device *pdev)
{
	int ret;

	plcd_drv = kzalloc((int)sizeof(*plcd_drv), GFP_KERNEL);
	if (!plcd_drv) {
		LCDERR("%s: per lcd driver no enough memory\n", __func__);
		return -ENOMEM;
	}
	plcd_drv->pcfg = kzalloc(sizeof(*plcd_drv->pcfg), GFP_KERNEL);
	if (!plcd_drv->pcfg) {
		LCDERR("%s: per lcd config no enough memory\n", __func__);
		return -ENOMEM;
	}

	plcd_drv->dev = &pdev->dev;
	plcd_drv->plcd_reg_map = &plcd_reg_map;
	plcd_ioremap(pdev);
	memset(plcd_gpio, 0, sizeof(*plcd_gpio));
	ret = plcd_get_basic_config_dts(pdev);

	if (ret)
		goto plcd_probe_failed;
	ret = plcd_dev_probe();
	if (ret)
		goto plcd_probe_failed;
	plcd_class_create();

	LCDPR("%s ok\n", __func__);
	return 0;

plcd_probe_failed:
	LCDPR("%s failed\n", __func__);
	kfree(plcd_drv);
	plcd_drv = NULL;
	return -1;
}

static int plcd_remove(struct platform_device *pdev)
{
	if (!plcd_drv)
		return 0;

	plcd_class_remove();
	plcd_dev_remove();
	plcd_unset_mem();

	kfree(plcd_drv->pcfg);
	plcd_drv->pcfg = NULL;

	kfree(plcd_drv);
	plcd_drv = NULL;

	LCDPR("%s ok\n", __func__);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id per_lcd_dt_match[] = {
	{
		.compatible = "amlogic, peripheral_lcd",
	},
	{}
};
#endif

static struct platform_driver peripheral_lcd_platform_driver = {
	.driver = {
		.name  = "peripheral_lcd",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = per_lcd_dt_match,
#endif
	},
	.probe   = plcd_probe,
	.remove  = plcd_remove,
};

int __init peripheral_lcd_init(void)
{
	if (platform_driver_register(&peripheral_lcd_platform_driver)) {
		LCDPR("failed to register peripheral lcd driver module\n");
		return -ENODEV;
	}
	return 0;
}

void __exit peripheral_lcd_exit(void)
{
	platform_driver_unregister(&peripheral_lcd_platform_driver);
}
