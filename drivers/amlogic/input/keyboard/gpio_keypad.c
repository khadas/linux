// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/amlogic/pm.h>
#include <linux/irq.h>
#include <linux/amlogic/power_domain.h>

#undef pr_fmt
#define pr_fmt(fmt) "gpio-keypad: " fmt

#define DEFAULT_SCAN_PERION	20
#define	DEFAULT_POLL_MODE	0
#define KEY_JITTER_COUNT	1

struct pin_desc {
	int current_status;
	struct gpio_desc *desc;
	int irq_num;
	u32 code;
	u32 key_type;
	const char *name;
	int count;
};

struct gpio_keypad {
	int key_size;
	int use_irq;/* 1:irq mode ; 0:polling mode */
	int scan_period;
	struct pin_desc *key;
	struct pin_desc *current_key;
	struct timer_list polling_timer;
	struct input_dev *input_dev;
	struct class kp_class;
};

static irqreturn_t gpio_irq_handler(int irq, void *data)
{
	struct gpio_keypad *keypad;

	keypad = (struct gpio_keypad *)data;
	mod_timer(&keypad->polling_timer,
		  jiffies + msecs_to_jiffies(20));
	return IRQ_HANDLED;
}

static void report_key_code(struct gpio_keypad *keypad, int gpio_val)
{
	struct pin_desc *key = keypad->current_key;

	if (key->count >= KEY_JITTER_COUNT) {
		key->current_status = gpio_val;
		if (key->current_status) {
			input_event(keypad->input_dev, key->key_type,
				    key->code, 0);

			dev_info(&keypad->input_dev->dev,
				 "key %d up.\n", key->code);
		} else {
			input_event(keypad->input_dev, key->key_type,
				    key->code, 1);

			dev_info(&keypad->input_dev->dev,
				 "key %d down.\n", key->code);
		}
		input_sync(keypad->input_dev);

		key->count = 0;
	}
}

static void polling_timer_handler(struct timer_list *t)
{
	int i;
	int gpio_val;
	int is_pressing = 0;
	struct gpio_keypad *keypad = from_timer(keypad, t, polling_timer);

	if (keypad->use_irq) {/* irq mode */
		for (i = 0; i < keypad->key_size; i++) {
			gpio_val = gpiod_get_value(keypad->key[i].desc);
			if (gpio_val == 0) {
				keypad->key[i].count++;
				is_pressing = 1;
			}

			if (keypad->key[i].current_status != gpio_val) {
				keypad->current_key = &keypad->key[i];
				report_key_code(keypad, gpio_val);
			}
		}
		if (is_pressing)
			mod_timer(&keypad->polling_timer,
				  jiffies +
				  msecs_to_jiffies(keypad->scan_period));
	} else {/* polling mode */
		for (i = 0; i < keypad->key_size; i++) {
			gpio_val = gpiod_get_value(keypad->key[i].desc);
			if (gpio_val == 0)
				keypad->key[i].count++;
			if (keypad->key[i].current_status != gpio_val) {
				keypad->current_key = &keypad->key[i];
				report_key_code(keypad, gpio_val);
			}
		mod_timer(&keypad->polling_timer,
			  jiffies + msecs_to_jiffies(keypad->scan_period));
		}
	}
}

static ssize_t table_show(struct class *cls, struct class_attribute *attr,
			  char *buf)
{
	struct gpio_keypad *keypad = container_of(cls,
					struct gpio_keypad, kp_class);
	int i;
	int len = 0;

	for (i = 0; i < keypad->key_size; i++) {
		len += sprintf(buf + len,
			"[%d]: name = %-21s status = %-5d\n", i,
			keypad->key[i].name,
			keypad->key[i].current_status);
	}

	return len;
}

static CLASS_ATTR_RO(table);

static struct attribute *meson_gpiokey_attrs[] = {
	&class_attr_table.attr,
	NULL
};

ATTRIBUTE_GROUPS(meson_gpiokey);

static int meson_gpio_kp_probe(struct platform_device *pdev)
{
	struct gpio_desc *desc;
	int ret, i;
	struct input_dev *input_dev;
	struct gpio_keypad *keypad;
	struct irq_desc *d;
	unsigned int number;
	int wakeup_source = 0;
	int register_flag = 0;

	if (!(pdev->dev.of_node)) {
		dev_err(&pdev->dev,
			"pdev->dev.of_node == NULL!\n");
		return -EINVAL;
	}
	keypad = devm_kzalloc(&pdev->dev,
			      sizeof(struct gpio_keypad), GFP_KERNEL);
	if (!keypad)
		return -EINVAL;
	ret = of_property_read_u32(pdev->dev.of_node,
				   "detect_mode", &keypad->use_irq);
	if (ret)
		/* The default mode is polling. */
		keypad->use_irq = DEFAULT_POLL_MODE;
	ret = of_property_read_u32(pdev->dev.of_node,
				   "scan_period", &keypad->scan_period);
	if (ret)
		/* he default scan period is 20. */
		keypad->scan_period = DEFAULT_SCAN_PERION;
	if (of_property_read_bool(pdev->dev.of_node, "wakeup-source"))
		wakeup_source = 1;
	ret = of_property_read_u32(pdev->dev.of_node,
				   "key_num", &keypad->key_size);
	if (ret) {
		dev_err(&pdev->dev,
			"failed to get key_num!\n");
		return -EINVAL;
	}
	keypad->key = devm_kzalloc(&pdev->dev,
				   (keypad->key_size) * sizeof(*keypad->key),
				   GFP_KERNEL);
	if (!(keypad->key))
		return -EINVAL;
	for (i = 0; i < keypad->key_size; i++) {
		/* get all gpio desc. */
		desc = devm_gpiod_get_index(&pdev->dev, "key", i, GPIOD_IN);
		if (IS_ERR_OR_NULL(desc))
			return -EINVAL;
		keypad->key[i].desc = desc;
		/* The gpio default is high level. */
		keypad->key[i].current_status = 1;
		ret = of_property_read_u32_index(pdev->dev.of_node,
						 "key_code", i,
						 &keypad->key[i].code);
		if (ret < 0) {
			dev_err(&pdev->dev,
				"find key_code=%d finished\n", i);
			return -EINVAL;
		}

		ret = of_property_read_u32_index(pdev->dev.of_node,
						 "key_type", i,
						 &keypad->key[i].key_type);
		if (ret)
			keypad->key[i].key_type = EV_KEY;

		ret = of_property_read_string_index(pdev->dev.of_node,
						    "key_name", i,
						    &keypad->key[i].name);
		if (ret < 0) {
			dev_err(&pdev->dev,
				"find key_name=%d finished\n", i);
			return -EINVAL;
		}
		gpiod_direction_input(keypad->key[i].desc);
		gpiod_set_pull(keypad->key[i].desc, GPIOD_PULL_UP);
	}

	keypad->kp_class.name = "gpio_keypad";
	keypad->kp_class.owner = THIS_MODULE;
	keypad->kp_class.class_groups = meson_gpiokey_groups;
	ret = class_register(&keypad->kp_class);
	if (ret) {
		dev_err(&pdev->dev, "fail to create gpio keypad class.\n");
		return -EINVAL;
	}

	/* input */
	input_dev = input_allocate_device();
	if (!input_dev)
		return -EINVAL;
	for (i = 0; i < keypad->key_size; i++) {
		input_set_capability(input_dev, keypad->key[i].key_type,
				     keypad->key[i].code);

		dev_info(&pdev->dev, "%s key(%d) type(0x%x) registered.\n",
			 keypad->key[i].name, keypad->key[i].code,
			 keypad->key[i].key_type);
	}
	input_dev->name = "gpio_keypad";
	input_dev->phys = "gpio_keypad/input0";
	input_dev->dev.parent = &pdev->dev;
	input_dev->id.bustype = BUS_ISA;
	input_dev->id.vendor = 0x0001;
	input_dev->id.product = 0x0001;
	input_dev->id.version = 0x0100;
	input_dev->rep[REP_DELAY] = 0xffffffff;
	input_dev->rep[REP_PERIOD] = 0xffffffff;
	input_dev->keycodesize = sizeof(unsigned short);
	input_dev->keycodemax = 0x1ff;
	keypad->input_dev = input_dev;
	ret = input_register_device(keypad->input_dev);
	if (ret < 0) {
		input_free_device(keypad->input_dev);
		return -EINVAL;
	}
	platform_set_drvdata(pdev, keypad);
	timer_setup(&keypad->polling_timer,
		    polling_timer_handler, 0);
	if (keypad->use_irq) {
		for (i = 0; i < keypad->key_size; i++) {
			keypad->key[i].count = 0;
			keypad->key[i].irq_num =
				gpiod_to_irq(keypad->key[i].desc);
			ret = devm_request_irq(&pdev->dev,
					       keypad->key[i].irq_num,
					       gpio_irq_handler,
					       IRQF_TRIGGER_FALLING,
					       "gpio_keypad", keypad);
			if (ret) {
				dev_err(&pdev->dev,
					"Requesting irq failed!\n");
				input_free_device(keypad->input_dev);
				del_timer(&keypad->polling_timer);
				return -EINVAL;
			}
			if (keypad->key[i].code == KEY_POWER) {
				d = irq_to_desc(keypad->key[i].irq_num);
				if (d) {
					number =
					 d->irq_data.parent_data->hwirq - 32;
					pwr_ctrl_irq_set(number, 1, 0);
					register_flag = 1;
				}
			}
		}
	} else {
		mod_timer(&keypad->polling_timer,
			  jiffies + msecs_to_jiffies(keypad->scan_period));
	}
	if (wakeup_source) {
		if (register_flag)
			dev_info(&pdev->dev,
				 "succeed to register wakeup source!\n");
		else
			dev_info(&pdev->dev,
				 "failed to register wakeup source!\n");
	}

	return 0;
}

static int meson_gpio_kp_remove(struct platform_device *pdev)
{
	struct gpio_keypad *keypad;

	keypad = platform_get_drvdata(pdev);
	class_unregister(&keypad->kp_class);
	input_unregister_device(keypad->input_dev);
	input_free_device(keypad->input_dev);
	del_timer(&keypad->polling_timer);
	return 0;
}

static const struct of_device_id key_dt_match[] = {
	{	.compatible = "amlogic, gpio_keypad", },
	{},
};

static int meson_gpio_kp_suspend(struct platform_device *dev,
				 pm_message_t state)
{
	struct gpio_keypad *pdata;

	pdata = (struct gpio_keypad *)platform_get_drvdata(dev);
	if (!pdata->use_irq)
		del_timer(&pdata->polling_timer);
	return 0;
}

static int meson_gpio_kp_resume(struct platform_device *dev)
{
	int i;
	struct gpio_keypad *pdata;

	pdata = (struct gpio_keypad *)platform_get_drvdata(dev);
	if (!pdata->use_irq)
		mod_timer(&pdata->polling_timer,
			  jiffies + msecs_to_jiffies(5));
	if (get_resume_method() == POWER_KEY_WAKEUP) {
		for (i = 0; i < pdata->key_size; i++) {
			if (pdata->key[i].code == KEY_POWER) {
				pr_info("gpio keypad wakeup\n");
				input_report_key(pdata->input_dev,
						 KEY_POWER,  1);
				input_sync(pdata->input_dev);
				input_report_key(pdata->input_dev,
						 KEY_POWER,  0);
				input_sync(pdata->input_dev);
				break;
			}
		}
	}
	return 0;
}

static struct platform_driver meson_gpio_kp_driver = {
	.probe = meson_gpio_kp_probe,
	.remove = meson_gpio_kp_remove,
	.suspend = meson_gpio_kp_suspend,
	.resume = meson_gpio_kp_resume,
	.driver = {
		.name = "gpio-keypad",
		.of_match_table = key_dt_match,
	},
};

module_platform_driver(meson_gpio_kp_driver);
MODULE_AUTHOR("Amlogic");
MODULE_DESCRIPTION("GPIO Keypad Driver");
MODULE_LICENSE("GPL");
