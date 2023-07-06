// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/spi/spi.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/amlogic/media/vout/peripheral_lcd.h>
#include "peripheral_lcd_drv.h"

static struct class *per_lcd_class;

static ssize_t plcd_info_show(struct class *class, struct class_attribute *attr, char *buf)
{
	ssize_t n = 0;

	if (!plcd_drv)
		return sprintf(buf, "peripheral lcd driver is NULL\n");

	n += sprintf(buf + n,
		"peripheral lcd driver %s(%d) info:\n"
		"table_loaded:       %d\n"
		"cmd_size:           %d\n"
		"table_init_on_cnt:  %d\n"
		"table_init_off_cnt: %d\n",
		plcd_drv->pcfg->name, plcd_drv->dev_index,
		plcd_drv->init_flag, plcd_drv->pcfg->cmd_size,
		plcd_drv->pcfg->init_on_cnt, plcd_drv->pcfg->init_off_cnt);
	switch (plcd_drv->pcfg->type) {
	case PLCD_TYPE_SPI:
	case PLCD_TYPE_QSPI:
		break;
	case PLCD_TYPE_MCU_8080:
		n += sprintf(buf + n,
			"reset_index: %d\n"
			"nCS_index:   %d\n"
			"nRD_index:   %d\n"
			"nWR_index:   %d\n"
			"nRS_index:   %d\n"
			"data0_index: %d\n"
			"data1_index: %d\n"
			"data2_index: %d\n"
			"data3_index: %d\n"
			"data4_index: %d\n"
			"data5_index: %d\n"
			"data6_index: %d\n"
			"data7_index: %d\n",
			plcd_drv->pcfg->i8080_cfg.reset_index,
			plcd_drv->pcfg->i8080_cfg.nCS_index,
			plcd_drv->pcfg->i8080_cfg.nRD_index,
			plcd_drv->pcfg->i8080_cfg.nWR_index,
			plcd_drv->pcfg->i8080_cfg.nRS_index,
			plcd_drv->pcfg->i8080_cfg.data0_index,
			plcd_drv->pcfg->i8080_cfg.data1_index,
			plcd_drv->pcfg->i8080_cfg.data2_index,
			plcd_drv->pcfg->i8080_cfg.data3_index,
			plcd_drv->pcfg->i8080_cfg.data4_index,
			plcd_drv->pcfg->i8080_cfg.data5_index,
			plcd_drv->pcfg->i8080_cfg.data6_index,
			plcd_drv->pcfg->i8080_cfg.data7_index);
		break;
	default:
		n += sprintf(buf + n, "not support per_type\n");
		break;
	}

	return n;
}

static ssize_t plcd_init_store(struct class *class, struct class_attribute *attr,
				  const char *buf, size_t count)
{
	int flag = 0, ret;

	if (!plcd_drv) {
		LCDPR("peripheral_lcd_drv is NULL\n");
		return count;
	}
	ret = kstrtoint(buf, 10, &flag);

	if (flag && plcd_drv->enable) {
		plcd_drv->enable();
		return count;
	} else if (!flag && plcd_drv->disable) {
		plcd_drv->disable();
		return count;
	}

	LCDPR("peripheral_lcd_drv %s is not supported\n", flag ? "enable" : "disable");
	return count;
}

static ssize_t plcd_test_store(struct class *class, struct class_attribute *attr,
				  const char *buf, size_t count)
{
	if (!plcd_drv) {
		LCDPR("peripheral_lcd_drv is NULL\n");
		return 0;
	}

	if (!plcd_drv->test) {
		LCDPR("peripheral_lcd_drv test is NULL\n");
		return 0;
	}

	if (plcd_drv)
		plcd_drv->test(buf);
	return count;
}

static ssize_t plcd_print_store(struct class *class, struct class_attribute *attr,
				   const char *buf, size_t count)
{
	int flag = 0, ret;

	ret = kstrtoint(buf, 10, &flag);
	if (ret)
		per_lcd_debug_flag = flag;
	return count;
}

static ssize_t plcd_print_show(struct class *class,
				  struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "per_lcd_debug_flag = %d\n", per_lcd_debug_flag);
}

static ssize_t plcd_display_addr_store(struct class *class, struct class_attribute *attr,
				  const char *buf, size_t count)
{
	unsigned long long addr = 0;
	int ret;

	if (!plcd_drv) {
		LCDPR("peripheral_lcd_drv is NULL\n");
		return count;
	}
	ret = kstrtoull(buf, 16, &addr);
	if (addr == 0 || ret)
		return count;

	LCDPR("flush frame at %px", (unsigned char *)addr);
	if (plcd_drv->frame_flush)
		plcd_drv->frame_flush((unsigned char *)addr);
	else if (plcd_drv->frame_post)
		plcd_drv->frame_post((unsigned char *)addr,
				     0, plcd_drv->pcfg->h, 0, plcd_drv->pcfg->v);
	else
		LCDERR("device not support frame_flush or frame_post\n");

	return count;
}

static struct class_attribute per_lcd_class_attrs[] = {
	__ATTR(info,  0444, plcd_info_show, NULL),
	__ATTR(print, 0644, plcd_print_show, plcd_print_store),
	__ATTR(init,  0200, NULL, plcd_init_store),
	__ATTR(test,  0200, NULL, plcd_test_store),
	__ATTR(display_addr,  0200, NULL, plcd_display_addr_store),
};

int plcd_class_create(void)
{
	int i;

	per_lcd_class = class_create(THIS_MODULE, "peripheral_lcd");
	if (IS_ERR_OR_NULL(per_lcd_class)) {
		LCDERR("create debug class failed\n");
		return -1;
	}

	for (i = 0; i < ARRAY_SIZE(per_lcd_class_attrs); i++) {
		if (class_create_file(per_lcd_class, &per_lcd_class_attrs[i])) {
			LCDERR("create debug attribute %s failed\n",
			       per_lcd_class_attrs[i].attr.name);
		}
	}

	return 0;
}

void plcd_class_remove(void)
{
	int i;

	if (!per_lcd_class)
		return;

	for (i = 0; i < ARRAY_SIZE(per_lcd_class_attrs); i++)
		class_remove_file(per_lcd_class, &per_lcd_class_attrs[i]);

	class_destroy(per_lcd_class);
	per_lcd_class = NULL;
}

