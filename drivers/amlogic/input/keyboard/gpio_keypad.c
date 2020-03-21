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
#include <linux/amlogic/pwr_ctrl.h>

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
	const char *name;
};

struct gpio_keypad {
	int key_size;
	int use_irq;/* 1:irq mode ; 0:polling mode */
	int scan_period;
	int current_irq;
	int count;
	int index;
	struct pin_desc *key;
	struct pin_desc *current_key;
	struct timer_list polling_timer;
	struct input_dev *input_dev;
};

static irqreturn_t gpio_irq_handler(int irq, void *data)
{
	struct gpio_keypad *keypad;

	keypad = (struct gpio_keypad *)data;
	keypad->current_irq  = irq;
	keypad->count++;
	mod_timer(&keypad->polling_timer,
		  jiffies + msecs_to_jiffies(20));
	return IRQ_HANDLED;
}

static struct pin_desc *get_current_key(struct gpio_keypad *keypad)
{
	int i;

	for (i = 0; i < keypad->key_size; i++) {
		if (keypad->current_irq == keypad->key[i].irq_num)
			return &keypad->key[i];
	}
	return NULL;
}

static void report_key_code(struct gpio_keypad *keypad, int gpio_val)
{
	struct pin_desc *key;

	if (keypad->count < KEY_JITTER_COUNT) {
		keypad->count++;
	} else {
		key = keypad->current_key;
		key->current_status = gpio_val;
		if (key->current_status) {
			input_report_key(keypad->input_dev,
					 key->code, 0);
			if (keypad->use_irq)
				enable_irq(key->irq_num);
			dev_info(&keypad->input_dev->dev,
				 "key %d up.\n", key->code);
		} else {
			input_report_key(keypad->input_dev,
					 key->code, 1);
			if (keypad->use_irq)
				disable_irq_nosync(key->irq_num);

			dev_info(&keypad->input_dev->dev,
				 "key %d down.\n", key->code);
		}
		input_sync(keypad->input_dev);

		keypad->count = 0;
	}
}

static void polling_timer_handler(struct timer_list *t)
{
	struct pin_desc *key;
	int i;
	int gpio_val;
	struct gpio_keypad *keypad = from_timer(keypad, t, polling_timer);

	if (keypad->use_irq) {/* irq mode */
		keypad->current_key = get_current_key(keypad);
		if (!(keypad->current_key))
			return;
		key = keypad->current_key;
		gpio_val = gpiod_get_value(key->desc);
		if (key->current_status != gpio_val)
			report_key_code(keypad, gpio_val);
		else
			keypad->count = 0;
		if (key->current_status == 0)
			mod_timer(&keypad->polling_timer,
				  jiffies +
				  msecs_to_jiffies(keypad->scan_period));
	} else {/* polling mode */
		for (i = 0; i < keypad->key_size; i++) {
			gpio_val = gpiod_get_value(keypad->key[i].desc);
			if (keypad->key[i].current_status != gpio_val) {
				keypad->index = i;
				keypad->current_key = &keypad->key[i];
				report_key_code(keypad, gpio_val);
			} else if (keypad->index ==  i) {
				keypad->count = 0;
			}
		mod_timer(&keypad->polling_timer,
			  jiffies + msecs_to_jiffies(keypad->scan_period));
		}
	}
}

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
	/* input */
	input_dev = input_allocate_device();
	if (!input_dev)
		return -EINVAL;
	set_bit(EV_KEY,  input_dev->evbit);
	for (i = 0; i < keypad->key_size; i++) {
		set_bit(keypad->key[i].code,  input_dev->keybit);
		dev_info(&pdev->dev, "%s key(%d) registed.\n",
			 keypad->key[i].name, keypad->key[i].code);
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
	keypad->count = 0;
	keypad->index = -1;
	timer_setup(&keypad->polling_timer,
		    polling_timer_handler, 0);
	if (keypad->use_irq) {
		for (i = 0; i < keypad->key_size; i++) {
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
