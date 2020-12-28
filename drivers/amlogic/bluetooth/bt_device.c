// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * drivers/amlogic/bluetooth/bt_device.c
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
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

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/ctype.h>
#include <linux/leds.h>
#include <linux/gpio.h>
#include <linux/rfkill.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/pinctrl/consumer.h>
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/of_gpio.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/io.h>
#include <linux/amlogic/bt_device.h>
#include <linux/random.h>
#ifdef CONFIG_AM_WIFI_SD_MMC
#include <linux/amlogic/wifi_dt.h>
#endif
#include "../../gpio/gpiolib.h"

#if defined(CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND) && defined(CONFIG_AMLOGIC_GX_SUSPEND)
#include <linux/amlogic/pm.h>
static struct early_suspend bt_early_suspend;
#endif

#define BT_RFKILL "bt_rfkill"

char bt_addr[18] = "";
char *btmac;
core_param(btmac, btmac, charp, 0644);
static struct class *bt_addr_class;
static ssize_t value_show(struct class *cls,
			  struct class_attribute *attr, char *_buf)
{
	char local_addr[6];

	if (!_buf)
		return -EINVAL;

	if (strlen(bt_addr) == 0) {
		local_addr[0] = 0x22;
		local_addr[1] = 0x22;
		local_addr[2] = prandom_u32();
		local_addr[3] = prandom_u32();
		local_addr[4] = prandom_u32();
		local_addr[5] = prandom_u32();
		sprintf(bt_addr, "%02x:%02x:%02x:%02x:%02x:%02x",
			local_addr[0], local_addr[1], local_addr[2],
			local_addr[3], local_addr[4], local_addr[5]);
	}

	return sprintf(_buf, "%s\n", bt_addr);
}

static ssize_t value_store(struct class *cls,
			   struct class_attribute *attr,
			   const char __user *buf, size_t count)
{
	int ret = -EINVAL;

	if (!buf)
		return ret;

	snprintf(bt_addr, sizeof(bt_addr), "%s", buf);

	if (bt_addr[strlen(bt_addr) - 1] == '\n')
		bt_addr[strlen(bt_addr) - 1] = '\0';

	pr_info("bt_addr=%s\n", bt_addr);
	return count;
}
static CLASS_ATTR_RW(value);

struct bt_dev_runtime_data {
	struct rfkill *bt_rfk;
	struct bt_dev_data *pdata;
};

static void bt_device_off(struct bt_dev_data *pdata)
{
	if (pdata->power_down_disable != 0 ||
	    pdata->power_off_flag <= 0) {/*bt rc wakeup flag for bcm.*/
		return;
	}
	if (pdata->gpio_reset > 0) {
		if (pdata->power_on_pin_OD &&
		    pdata->power_low_level) {
			gpio_direction_input(pdata->gpio_reset);
		} else {
			gpio_direction_output(pdata->gpio_reset,
					      pdata->power_low_level);
		}
	}
	if (pdata->gpio_en > 0) {
		if (pdata->power_on_pin_OD &&
		    pdata->power_low_level) {
			gpio_direction_input(pdata->gpio_en);
		} else {
			set_usb_bt_power(0);
		}
	}
	msleep(20);
}

static void bt_device_init(struct bt_dev_data *pdata)
{
	int tmp = 0;

	if (pdata->gpio_reset > 0)
		gpio_request(pdata->gpio_reset, BT_RFKILL);

	if (pdata->gpio_en > 0)
		gpio_request(pdata->gpio_en, BT_RFKILL);

	if (pdata->gpio_hostwake > 0) {
		gpio_request(pdata->gpio_hostwake, BT_RFKILL);
		gpio_direction_output(pdata->gpio_hostwake, 1);
	}

	tmp = pdata->power_down_disable;
	pdata->power_down_disable = 0;
	bt_device_off(pdata);
	pdata->power_down_disable = tmp;
}

static void bt_device_deinit(struct bt_dev_data *pdata)
{
	if (pdata->gpio_reset > 0)
		gpio_free(pdata->gpio_reset);

	if (pdata->gpio_en > 0)
		gpio_free(pdata->gpio_en);

	if (pdata->gpio_hostwake > 0)
		gpio_free(pdata->gpio_hostwake);
}

static void bt_device_on(struct bt_dev_data *pdata)
{
	if (pdata->power_down_disable == 0) {
		if (pdata->gpio_reset > 0) {
			if (pdata->power_on_pin_OD &&
			    pdata->power_low_level) {
				gpio_direction_input(pdata->gpio_reset);
			} else {
				gpio_direction_output(pdata->gpio_reset,
						      pdata->power_low_level);
			}
		}
		if (pdata->gpio_en > 0) {
			if (pdata->power_on_pin_OD &&
			    pdata->power_low_level) {
				gpio_direction_input(pdata->gpio_en);
			} else {
				set_usb_bt_power(0);
			}
		}
		msleep(200);
	}
	if (pdata->gpio_reset > 0) {
		if (pdata->power_on_pin_OD &&
		    !pdata->power_low_level) {
			gpio_direction_input(pdata->gpio_reset);
		} else {
			gpio_direction_output(pdata->gpio_reset,
					      !pdata->power_low_level);
		}
	}
	if (pdata->gpio_en > 0) {
		if (pdata->power_on_pin_OD &&
		    !pdata->power_low_level) {
			gpio_direction_input(pdata->gpio_en);
		} else {
			set_usb_bt_power(1);
		}
	}
	msleep(200);
}

static int bt_set_block(void *data, bool blocked)
{
	struct bt_dev_data *pdata = data;

	pr_info("BT_RADIO going: %s\n", blocked ? "off" : "on");

	if (!blocked) {
		pr_info("AML_BT: going ON\n");
		bt_device_on(pdata);
	} else {
		pr_info("AML_BT: going OFF\n");
	bt_device_off(pdata);
	}
	return 0;
}

static const struct rfkill_ops bt_rfkill_ops = {
	.set_block = bt_set_block,
};

#if defined(CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND) && defined(CONFIG_AMLOGIC_GX_SUSPEND)
static void bt_earlysuspend(struct early_suspend *h)
{
}

static void bt_lateresume(struct early_suspend *h)
{
}
#endif

static int bt_suspend(struct platform_device *pdev,
		      pm_message_t state)
{
	return 0;
}

static int bt_resume(struct platform_device *pdev)
{
	return 0;
}

static int bt_probe(struct platform_device *pdev)
{
	int ret = 0;
	const void *prop;
	struct rfkill *bt_rfk;
	struct bt_dev_data *pdata = NULL;
	struct bt_dev_runtime_data *prdata;

#ifdef CONFIG_OF
	if (pdev && pdev->dev.of_node) {
		const char *str;
		//struct gpio_desc *desc;

		pr_debug("enter %s of_node\n", __func__);
		pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
		if (!pdata) {
			ret = -ENOMEM;
			goto err_res;
		}

		ret = of_property_read_string(pdev->dev.of_node,
					      "reset-gpios", &str);
		if (ret) {
			pr_warn("not get gpio_reset\n");
			pdata->gpio_reset = 0;
		} else {
			pdata->gpio_reset = of_get_named_gpio_flags
							(pdev->dev.of_node,
							"reset-gpios", 0, NULL);
		}

		ret = of_property_read_string(pdev->dev.of_node,
					      "bt_en-gpios", &str);
		if (ret) {
			pr_warn("not get gpio_en\n");
			pdata->gpio_en = 0;
		} else {
			pdata->gpio_en = of_get_named_gpio_flags
							(pdev->dev.of_node,
							"bt_en-gpios", 0, NULL);
		}
		ret = of_property_read_string(pdev->dev.of_node,
					      "hostwake-gpios", &str);
		if (ret) {
			pr_warn("not get gpio_hostwake\n");
			pdata->gpio_hostwake = 0;
		} else {
			pdata->gpio_hostwake = of_get_named_gpio_flags
							(pdev->dev.of_node,
							"hostwake-gpios",
							0, NULL);
		}

		prop = of_get_property(pdev->dev.of_node,
				       "power_low_level", NULL);
		if (prop) {
			pr_debug("power on valid level is low");
			pdata->power_low_level = 1;
		} else {
			pr_debug("power on valid level is high");
			pdata->power_low_level = 0;
			pdata->power_on_pin_OD = 0;
		}
		ret = of_property_read_u32(pdev->dev.of_node,
					   "power_on_pin_OD",
					   &pdata->power_on_pin_OD);
		if (ret)
			pdata->power_on_pin_OD = 0;
		pr_debug("bt: power_on_pin_OD = %d;\n", pdata->power_on_pin_OD);
		ret = of_property_read_u32(pdev->dev.of_node,
					   "power_off_flag",
					   &pdata->power_off_flag);
		if (ret)
			pdata->power_off_flag = 1;/*bt poweroff*/
		pr_debug("bt: power_off_flag = %d;\n", pdata->power_off_flag);

		ret = of_property_read_u32(pdev->dev.of_node,
					   "power_down_disable",
					   &pdata->power_down_disable);
		if (ret)
			pdata->power_down_disable = 0;
		pr_debug("dis power down = %d;\n", pdata->power_down_disable);
	} else if (pdev) {
		pdata = (struct bt_dev_data *)(pdev->dev.platform_data);
	} else {
		ret = -ENOENT;
		goto err_res;
	}
#else
	pdata = (struct bt_dev_data *)(pdev->dev.platform_data);
#endif
	bt_addr_class = class_create(THIS_MODULE, "bt_addr");
	ret = class_create_file(bt_addr_class, &class_attr_value);

	bt_device_init(pdata);
	if (pdata->power_down_disable == 1) {
		pdata->power_down_disable = 0;
		bt_device_on(pdata);
		pdata->power_down_disable = 1;
	}

	/* default to bluetooth off */
	/* rfkill_switch_all(RFKILL_TYPE_BLUETOOTH, 1); */
	/* bt_device_off(pdata); */

	bt_rfk = rfkill_alloc("bt-dev", &pdev->dev,
			      RFKILL_TYPE_BLUETOOTH,
			      &bt_rfkill_ops, pdata);

	if (!bt_rfk) {
		pr_info("rfk alloc fail\n");
		ret = -ENOMEM;
		goto err_rfk_alloc;
	}

	rfkill_init_sw_state(bt_rfk, false);
	ret = rfkill_register(bt_rfk);
	if (ret) {
		pr_err("rfkill_register fail\n");
		goto err_rfkill;
	}
	prdata = kmalloc(sizeof(*prdata), GFP_KERNEL);

	if (!prdata)
		goto err_rfkill;

	prdata->bt_rfk = bt_rfk;
	prdata->pdata = pdata;
	platform_set_drvdata(pdev, prdata);
#if defined(CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND) && defined(CONFIG_AMLOGIC_GX_SUSPEND)
	bt_early_suspend.level =
		EARLY_SUSPEND_LEVEL_DISABLE_FB;
	bt_early_suspend.suspend = bt_earlysuspend;
	bt_early_suspend.resume = bt_lateresume;
	bt_early_suspend.param = pdev;
	register_early_suspend(&bt_early_suspend);
#endif

	return 0;

err_rfkill:
	rfkill_destroy(bt_rfk);
err_rfk_alloc:
	bt_device_deinit(pdata);
err_res:
	return ret;
}

static int bt_remove(struct platform_device *pdev)
{
	struct bt_dev_runtime_data *prdata =
		platform_get_drvdata(pdev);
	struct rfkill *rfk = NULL;
	struct bt_dev_data *pdata = NULL;

	platform_set_drvdata(pdev, NULL);

	if (prdata) {
		rfk = prdata->bt_rfk;
		pdata = prdata->pdata;
	}

	if (pdata) {
		bt_device_deinit(pdata);
		kfree(pdata);
	}

	if (rfk) {
		rfkill_unregister(rfk);
		rfkill_destroy(rfk);
	}
	rfk = NULL;

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id bt_dev_dt_match[] = {
	{	.compatible = "amlogic, aml-bt",
	},
	{},
};
#else
#define bt_dev_dt_match NULL
#endif

static struct platform_driver bt_driver = {
	.driver		= {
		.name	= "aml_bt",
		.of_match_table = bt_dev_dt_match,
	},
	.probe		= bt_probe,
	.remove		= bt_remove,
	.suspend	= bt_suspend,
	.resume		= bt_resume,
};

static int __init bt_init(void)
{
	pr_info("amlogic rfkill init\n");

	return platform_driver_register(&bt_driver);
}

static void __exit bt_exit(void)
{
	platform_driver_unregister(&bt_driver);
}

module_init(bt_init);
module_exit(bt_exit);
MODULE_DESCRIPTION("bt rfkill");
MODULE_AUTHOR("");
MODULE_LICENSE("GPL");

/**************** bt mac *****************/

static int __init mac_addr_set(char *line)
{
	if (line) {
		pr_info("try to read bt mac from emmc key!\n");
		strncpy(bt_addr, line, sizeof(bt_addr) - 1);
		bt_addr[sizeof(bt_addr) - 1] = '\0';
		btmac = (char *)bt_addr;
	}

	return 1;
}

__setup("mac_bt=", mac_addr_set);

