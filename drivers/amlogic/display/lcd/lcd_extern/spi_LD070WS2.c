/*
 * drivers/amlogic/display/vout/lcd/lcd_extern/spi_LD070WS2.c
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
#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/miscdevice.h>
#include <linux/mutex.h>
#include <linux/mm.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/sysctl.h>
#include <linux/uaccess.h>
#include <linux/amlogic/vout/lcd_extern.h>

static struct lcd_extern_config_s *ext_config;

/* #define LCD_EXT_DEBUG_INFO */
#ifdef LCD_EXT_DEBUG_INFO
#define DBG_PRINT(...)		pr_info(__VA_ARGS__)
#else
#define DBG_PRINT(...)
#endif

#define LCD_EXTERN_NAME			"lcd_spi_LD070WS2"

#define SPI_DELAY		30 /* unit: us */

static unsigned char spi_init_table[][2] = {
	{0x00, 0x21}, /* reset */
	{0x00, 0xa5}, /* standby */
	{0x01, 0x30}, /* enable FRC/Dither */
	{0x02, 0x40}, /* enable normally black */
	{0x0e, 0x5f}, /* enable test mode1 */
	{0x0f, 0xa4}, /* enable test mode2 */
	{0x0d, 0x00}, /* enable SDRRS, enlarge OE width */
	{0x02, 0x43}, /* adjust charge sharing time */
	{0x0a, 0x28}, /* trigger bias reduction */
	{0x10, 0x41}, /* adopt 2 line/1 dot */
	{0xff, 50}, /* delay 50ms */
	{0x00, 0xad}, /* display on */
	{0xff, 0xff}, /* ending flag */
};

static unsigned char spi_off_table[][2] = {
	{0x00, 0xa5}, /* standby */
	{0xff, 0xff}, /* ending flag */
};

static void set_lcd_csb(unsigned v)
{
	lcd_extern_gpio_output(ext_config->spi_cs, v);
	udelay(SPI_DELAY);
}

static void set_lcd_scl(unsigned v)
{
	lcd_extern_gpio_output(ext_config->spi_clk, v);
	udelay(SPI_DELAY);
}

static void set_lcd_sda(unsigned v)
{
	lcd_extern_gpio_output(ext_config->spi_data, v);
	udelay(SPI_DELAY);
}

static void spi_gpio_init(void)
{
	set_lcd_csb(1);
	set_lcd_scl(1);
	set_lcd_sda(1);
}

static void spi_gpio_off(void)
{
	set_lcd_sda(0);
	set_lcd_scl(0);
	set_lcd_csb(0);
}

static void spi_write_8(unsigned char addr, unsigned char data)
{
	int i;
	unsigned int sdata;

	sdata = (unsigned int)(addr & 0x3f);
	sdata <<= 10;
	sdata |= (data & 0xff);
	sdata &= ~(1<<9); /* write flag */

	set_lcd_csb(1);
	set_lcd_scl(1);
	set_lcd_sda(1);

	set_lcd_csb(0);
	for (i = 0; i < 16; i++) {
		set_lcd_scl(0);
		if (sdata & 0x8000)
			set_lcd_sda(1);
		else
			set_lcd_sda(0);
		sdata <<= 1;
		set_lcd_scl(1);
	}

	set_lcd_csb(1);
	set_lcd_scl(1);
	set_lcd_sda(1);
	udelay(SPI_DELAY);
}

static int lcd_extern_spi_init(void)
{
	int ending_flag = 0;
	int i = 0;

	spi_gpio_init();

	while (ending_flag == 0) {
		if (spi_init_table[i][0] == 0xff) {
			if (spi_init_table[i][1] == 0xff)
				ending_flag = 1;
			else
				mdelay(spi_init_table[i][1]);
		} else {
			spi_write_8(spi_init_table[i][0], spi_init_table[i][1]);
		}
		i++;
	}
	pr_info("%s\n", __func__);

	return 0;
}

static int lcd_extern_spi_off(void)
{
	int ending_flag = 0;
	int i = 0;

	spi_gpio_init();

	while (ending_flag == 0) {
		if (spi_off_table[i][0] == 0xff) {
			if (spi_off_table[i][1] == 0xff)
				ending_flag = 1;
			else
				mdelay(spi_off_table[i][1]);
		} else {
			spi_write_8(spi_off_table[i][0], spi_off_table[i][1]);
		}
		i++;
	}
	pr_info("%s\n", __func__);
	mdelay(10);
	spi_gpio_off();

	return 0;
}

static int get_lcd_extern_config(struct platform_device *pdev,
		struct lcd_extern_config_s *ext_cfg)
{
	int ret = 0;
	struct aml_lcd_extern_driver_s *lcd_ext;

	ret = get_lcd_extern_dt_data(pdev, ext_cfg);
	if (ret) {
		pr_info("[error] %s: failed to get dt data\n", LCD_EXTERN_NAME);
		return ret;
	}

	/* lcd extern driver update */
	lcd_ext = aml_lcd_extern_get_driver();
	if (lcd_ext) {
		lcd_ext->type     = ext_cfg->type;
		lcd_ext->name     = ext_cfg->name;
		lcd_ext->power_on   = lcd_extern_spi_init;
		lcd_ext->power_off  = lcd_extern_spi_off;
	} else {
		pr_info("[error] %s get lcd_extern_driver failed\n",
			ext_cfg->name);
		ret = -1;
	}

	return ret;
}

static int aml_LD070WS2_probe(struct platform_device *pdev)
{
	int ret = 0;

	if (lcd_extern_driver_check() == FALSE)
		return -1;
	if (ext_config == NULL)
		ext_config = kzalloc(sizeof(*ext_config), GFP_KERNEL);
	if (ext_config == NULL) {
		pr_info("[error] %s probe: failed to alloc data\n",
			LCD_EXTERN_NAME);
		return -1;
	}

	pdev->dev.platform_data = ext_config;
	ret = get_lcd_extern_config(pdev, ext_config);
	if (ret)
		goto lcd_extern_probe_failed;

	pr_info("%s probe ok\n", LCD_EXTERN_NAME);
	return ret;

lcd_extern_probe_failed:
	kfree(ext_config);
	ext_config = NULL;
	return -1;
}

static int aml_LD070WS2_remove(struct platform_device *pdev)
{
	kfree(pdev->dev.platform_data);
	return 0;
}

#ifdef CONFIG_USE_OF
static const struct of_device_id aml_LD070WS2_dt_match[] = {
	{
		.compatible = "amlogic, lcd_spi_LD070WS2",
	},
	{},
};
#else
#define aml_LD070WS2_dt_match NULL
#endif

static struct platform_driver aml_LD070WS2_driver = {
	.probe  = aml_LD070WS2_probe,
	.remove = aml_LD070WS2_remove,
	.driver = {
		.name  = LCD_EXTERN_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_USE_OF
		.of_match_table = aml_LD070WS2_dt_match,
#endif
	},
};

static int __init aml_LD070WS2_init(void)
{
	int ret;
	DBG_PRINT("%s\n", __func__);

	ret = platform_driver_register(&aml_LD070WS2_driver);
	if (ret) {
		pr_info("[error] %s failed ", __func__);
		pr_info("to register lcd extern driver module\n");
		return -ENODEV;
	}
	return ret;
}

static void __exit aml_LD070WS2_exit(void)
{
	platform_driver_unregister(&aml_LD070WS2_driver);
}

/* late_initcall(aml_LD070WS2_init); */
module_init(aml_LD070WS2_init);
module_exit(aml_LD070WS2_exit);

MODULE_AUTHOR("AMLOGIC");
MODULE_DESCRIPTION("LCD Extern driver for LD070WS2");
MODULE_LICENSE("GPL");
