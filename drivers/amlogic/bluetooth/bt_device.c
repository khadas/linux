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
#include <linux/amlogic/iomap.h>
#include <linux/io.h>
#include <linux/amlogic/bt_device.h>
#include <linux/amlogic/wifi_dt.h>
#include <linux/random.h>
#ifdef CONFIG_AM_WIFI_SD_MMC
#include <linux/amlogic/wifi_dt.h>
#endif
#include "../../gpio/gpiolib.h"

#include <linux/interrupt.h>
#include <linux/pm_wakeup.h>
#include <linux/pm_wakeirq.h>
#include <linux/irq.h>

#include <linux/input.h>

#if defined(CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND) && defined(CONFIG_AMLOGIC_GX_SUSPEND)
#include <linux/amlogic/pm.h>
static struct early_suspend bt_early_suspend;
#endif

#define BT_RFKILL "bt_rfkill"
#define MODULE_ID 0x271
#define POWER_EVENT_DEF     0
#define POWER_EVENT_RESET   1
#define POWER_EVENT_EN      2

char bt_addr[18] = "";
char *btmac;
core_param(btmac, btmac, charp, 0644);
static struct class *bt_addr_class;
static int btwake_evt;
static int btirq_flag;
static int btpower_evt;
static int flag_n;
static int flag_p;
static int cnt;
static int rfk_reg = 1;

static int distinguish_module(void)
{
	int vendor_id = 0;

	vendor_id = sdio_get_vendor();

	if (vendor_id == MODULE_ID)
		return 0;

	return 1;
}

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

static void off_def_power(struct bt_dev_data *pdata, unsigned long down_time)
{
	pr_info("[%s]: pid: %d comm: %s\n", __func__,
		current->pid, current->comm);

	if (pdata->gpio_reset > 0) {
		if (pdata->power_on_pin_OD &&
			pdata->power_low_level) {
			gpio_direction_input(pdata->gpio_reset);
		} else {
			gpio_direction_output(pdata->gpio_reset,
				pdata->power_low_level);
		}

		if (down_time)
			msleep(down_time);
	}

	if (pdata->gpio_en > 0) {
		if (pdata->power_on_pin_OD &&
			pdata->power_low_level) {
			gpio_direction_input(pdata->gpio_en);
		} else {
			set_usb_bt_power(0);
		}
	}
}

static void on_def_power(struct bt_dev_data *pdata, unsigned long up_time)
{
	pr_info("[%s]: pid: %d comm: %s\n", __func__,
		current->pid, current->comm);

	if (pdata->gpio_reset > 0) {
		if (pdata->power_on_pin_OD &&
			!pdata->power_low_level) {
			gpio_direction_input(pdata->gpio_reset);
		} else {
			gpio_direction_output(pdata->gpio_reset,
				!pdata->power_low_level);
		}

		if (up_time)
			msleep(up_time);
	}

	if (pdata->gpio_en > 0) {
		if (pdata->power_on_pin_OD &&
			!pdata->power_low_level) {
			gpio_direction_input(pdata->gpio_en);
		} else {
			set_usb_bt_power(1);
		}
	}
}

static void off_reset_power(struct bt_dev_data *pdata, unsigned long down_time)
{
	if (pdata->gpio_reset > 0) {
		if (pdata->power_on_pin_OD &&
			pdata->power_low_level) {
			gpio_direction_input(pdata->gpio_reset);
		} else {
			gpio_direction_output(pdata->gpio_reset,
				pdata->power_low_level);
		}

		if (down_time)
			msleep(down_time);
	}
}

static void on_reset_power(struct bt_dev_data *pdata, unsigned long up_time)
{
	if (pdata->gpio_reset > 0) {
		if (pdata->power_on_pin_OD &&
			!pdata->power_low_level) {
			gpio_direction_input(pdata->gpio_reset);
		} else {
			gpio_direction_output(pdata->gpio_reset,
				!pdata->power_low_level);
		}

		if (up_time)
			msleep(up_time);
	}
}

static void bt_device_off(struct bt_dev_data *pdata)
{
	if (!distinguish_module())
		return;

	if (pdata->power_down_disable == 0) {
		switch (btpower_evt) {
		case POWER_EVENT_DEF:
			off_def_power(pdata, 0);
			break;
		case POWER_EVENT_RESET:
			off_reset_power(pdata, 0);
			break;
		case POWER_EVENT_EN:
			set_usb_bt_power(0);
			break;
		default:
			pr_err("%s default no electricity", __func__);
			break;
		}
		msleep(20);
	}
}

static void bt_device_init(struct bt_dev_data *pdata)
{
	btpower_evt = 0;
	btirq_flag = 0;

	if (pdata->gpio_reset > 0)
		gpio_request(pdata->gpio_reset, BT_RFKILL);

	if (pdata->gpio_en > 0)
		gpio_request(pdata->gpio_en, BT_RFKILL);

	if (pdata->gpio_hostwake > 0) {
		gpio_request(pdata->gpio_hostwake, BT_RFKILL);
		gpio_direction_output(pdata->gpio_hostwake, 1);
	}

	if (pdata->gpio_btwakeup > 0) {
		gpio_request(pdata->gpio_btwakeup, BT_RFKILL);
		gpio_direction_input(pdata->gpio_btwakeup);
	}
}

static void bt_device_deinit(struct bt_dev_data *pdata)
{
	if (pdata->gpio_reset > 0)
		gpio_free(pdata->gpio_reset);

	if (pdata->gpio_en > 0)
		gpio_free(pdata->gpio_en);

	btpower_evt = 0;
	if (pdata->gpio_hostwake > 0)
		gpio_free(pdata->gpio_hostwake);
}

static void bt_device_on(struct bt_dev_data *pdata, unsigned long down_time, unsigned long up_time)
{
	if (pdata->power_down_disable == 0) {
		switch (btpower_evt) {
		case POWER_EVENT_DEF:
			off_def_power(pdata, down_time);
			on_def_power(pdata, up_time);
			break;
		case POWER_EVENT_RESET:
			off_reset_power(pdata, down_time);
			on_reset_power(pdata, up_time);
			break;
		case POWER_EVENT_EN:
			set_usb_bt_power(0);
			set_usb_bt_power(1);
			break;
		default:
			pr_err("%s default no electricity", __func__);
			break;
		}
	}
}

/*The system calls this function when GPIOC_14 interrupt occurs*/
static irqreturn_t bt_interrupt(int irq, void *dev_id)
{
	struct bt_dev_data *pdata = (struct bt_dev_data *)dev_id;

	if (btirq_flag == 1) {
		schedule_work(&pdata->btwakeup_work);
		pr_info("freeze: test BT IRQ\n");
	}

	return IRQ_HANDLED;
}

static enum hrtimer_restart btwakeup_timer_handler(struct hrtimer *timer)
{
	struct bt_dev_data *pdata  = container_of(timer,
			struct bt_dev_data, timer);

	if  (!gpio_get_value(pdata->gpio_btwakeup) && cnt  < 5)
		cnt++;
	if (cnt >= 5 && cnt < 15) {
		if (gpio_get_value(pdata->gpio_btwakeup))
			flag_p++;
		else if (!gpio_get_value(pdata->gpio_btwakeup))
			flag_n++;
		cnt++;
	}
	pr_info("%s power: %d,netflix:%d\n", __func__, flag_p, flag_n);
	if (flag_p >= 7) {
		pr_info("%s power: %d\n", __func__, flag_p);
		btwake_evt = 2;
		cnt = 0;
		flag_p = 0;
		btirq_flag = 0;
		input_event(pdata->input_dev,
			EV_KEY, KEY_POWER, 1);
		input_sync(pdata->input_dev);
		input_event(pdata->input_dev,
			EV_KEY, KEY_POWER, 0);
		input_sync(pdata->input_dev);
	} else if (flag_n >= 7) {
		pr_info("%s netflix: %d\n", __func__, flag_n);
		btwake_evt = 2;
		cnt = 0;
		flag_n = 0;
		btirq_flag = 0;
		input_event(pdata->input_dev, EV_KEY, 133, 1);
		input_sync(pdata->input_dev);
		input_event(pdata->input_dev, EV_KEY, 133, 0);
		input_sync(pdata->input_dev);
	}
	if (btwake_evt != 2 && cnt != 0)
		hrtimer_start(&pdata->timer,
			ktime_set(0, 20 * 1000000), HRTIMER_MODE_REL);
	return HRTIMER_NORESTART;
}

static void get_btwakeup_irq_work(struct work_struct *work)
{
	struct bt_dev_data *pdata  = container_of(work,
		struct bt_dev_data, btwakeup_work);

	if (btwake_evt == 2)
		return;
	pr_info("%s", __func__);
	hrtimer_start(&pdata->timer,
			ktime_set(0, 100 * 1000000), HRTIMER_MODE_REL);
}

static int bt_set_block(void *data, bool blocked)
{
	struct bt_dev_data *pdata = data;

	if (rfk_reg) {
		pr_info("first rfkill_register skip\n");
		rfk_reg = 0;
		return 0;
	}

	pr_info("BT_RADIO going: %s\n", blocked ? "off" : "on");

	if (!blocked) {
		pr_info("AML_BT: going ON,btpower_evt=%d\n", btpower_evt);
		bt_device_on(pdata, 200, 200);
	} else {
		pr_info("AML_BT: going OFF,btpower_evt=%d\n", btpower_evt);
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
	struct bt_dev_runtime_data *prdata = platform_get_drvdata(pdev);
	btwake_evt = 0;
	pr_info("bt suspend\n");
	disable_irq(prdata->pdata->irqno_wakeup);

	return 0;
}

static int bt_resume(struct platform_device *pdev)
{
	struct bt_dev_runtime_data *prdata = platform_get_drvdata(pdev);
	pr_info("bt resume\n");
	enable_irq(prdata->pdata->irqno_wakeup);
	btwake_evt = 0;

	if ((get_resume_method() == RTC_WAKEUP) ||
		(get_resume_method() == AUTO_WAKEUP)) {
		btwake_evt = 1;
		btirq_flag = 1;
	    flag_n = 0;
		flag_p = 0;
		cnt = 0;
	}
	if (distinguish_module() && get_resume_method() == BT_WAKEUP) {
		input_event(prdata->pdata->input_dev,
			EV_KEY, KEY_POWER, 1);
		input_sync(prdata->pdata->input_dev);
		input_event(prdata->pdata->input_dev,
			EV_KEY, KEY_POWER, 0);
		input_sync(prdata->pdata->input_dev);
	}

	return 0;
}

static int bt_probe(struct platform_device *pdev)
{
	int ret = 0;
	const void *prop;
	struct rfkill *bt_rfk;
	struct bt_dev_data *pdata = NULL;
	struct bt_dev_runtime_data *prdata;
	struct input_dev *input_dev;

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
		/*gpio_btwakeup = BT_WAKE_HOST*/
		ret = of_property_read_string(pdev->dev.of_node,
			"btwakeup-gpios", &str);
		if (ret) {
			pr_warn("not get btwakeup-gpios\n");
			pdata->gpio_btwakeup = 0;
		} else {
			pdata->gpio_btwakeup = of_get_named_gpio_flags
							(pdev->dev.of_node,
							"btwakeup-gpios",
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
		bt_device_on(pdata, 100, 0);
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

	rfkill_init_sw_state(bt_rfk, true);
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

	/*1.Set BT_WAKE_HOST to the input state;*/
	/*2.Get interrupt number(irqno_wakeup).*/
	pdata->irqno_wakeup = gpio_to_irq(pdata->gpio_btwakeup);

	/*Register interrupt service function*/
	ret = request_irq(pdata->irqno_wakeup, bt_interrupt,
			IRQF_TRIGGER_FALLING, "bt-irq", (void *)pdata);
	if (ret < 0)
		pr_err("request_irq error ret=%d\n", ret);

	//disable_irq(pdata->irqno_wakeup);

	ret = device_init_wakeup(&pdev->dev, 1);
	if (ret)
		pr_err("device_init_wakeup failed: %d\n", ret);
	/*Wake up the interrupt*/
	ret = dev_pm_set_wake_irq(&pdev->dev, pdata->irqno_wakeup);
	if (ret)
		pr_err("dev_pm_set_wake_irq failed: %d\n", ret);

	INIT_WORK(&pdata->btwakeup_work, get_btwakeup_irq_work);

	//input
	input_dev = input_allocate_device();
	if (!input_dev) {
		pr_err("[abner test]input_allocate_device failed: %d\n", ret);
		return -EINVAL;
	}
	set_bit(EV_KEY,  input_dev->evbit);
	set_bit(KEY_POWER, input_dev->keybit);
	set_bit(133, input_dev->keybit);

	input_dev->name = "input_btrcu";
	input_dev->phys = "input_btrcu/input0";
	input_dev->dev.parent = &pdev->dev;
	input_dev->id.bustype = BUS_ISA;
	input_dev->id.vendor = 0x0001;
	input_dev->id.product = 0x0001;
	input_dev->id.version = 0x0100;
	input_dev->rep[REP_DELAY] = 0xffffffff;
	input_dev->rep[REP_PERIOD] = 0xffffffff;
	input_dev->keycodesize = sizeof(unsigned short);
	input_dev->keycodemax = 0x1ff;
	ret = input_register_device(input_dev);
	if (ret < 0) {
		pr_err("[abner test]input_register_device failed: %d\n", ret);
		input_free_device(input_dev);
		return -EINVAL;
	}
	pdata->input_dev = input_dev;

	hrtimer_init(&pdata->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	pdata->timer.function = btwakeup_timer_handler;

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

module_param(btpower_evt, int, 0664);
MODULE_PARM_DESC(btpower_evt, "btpower_evt");

module_param(btwake_evt, int, 0664);
MODULE_PARM_DESC(btwake_evt, "btwake_evt");
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

