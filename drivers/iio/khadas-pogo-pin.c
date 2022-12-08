// SPDX-License-Identifier: GPL-2.0-only
/*
 * Input driver for khadas-pogo-pin
 *
 * Copyright (c) 2022 Wesion Khadas
 */

#include <linux/err.h>
#include <linux/iio/consumer.h>
#include <linux/iio/types.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/property.h>
#include <linux/slab.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
/*
#include <linux/hrtimer.h>
struct timer_list pogo_pin_timer;
static void pogo_pin_timer_callback(struct timer_list *t)
{
	int value;

	printk("hlm pogo_pin_timer_callback value=%d\n", value);
	pogo_pin_timer.expires = jiffies + 100;
	add_timer(&pogo_pin_timer);
}

static void pogo_pin_init(struct device *dev)
{
	printk("hlm pogo_pin_timer_init\n");
	timer_setup(&pogo_pin_timer, pogo_pin_timer_callback, 0);
	pogo_pin_timer.expires = jiffies + 100;
	add_timer(&pogo_pin_timer);	
}
*/
struct pogo_pin_state {
	struct iio_channel *channel;
	int pogo_pwr_en_gpio;
	bool pogo_pwr_enable_flag;
};

static void pogo_pwr_enable(struct pogo_pin_state *st, bool enable)
{
	if(enable != st->pogo_pwr_enable_flag){
		gpio_set_value(st->pogo_pwr_en_gpio, enable);
		printk("pogo_pwr_enable=%d\n",enable);
		st->pogo_pwr_enable_flag = enable;
	}
}

static void pogo_pin_poll(struct input_dev *input)
{
	struct pogo_pin_state *st = input_get_drvdata(input);
	int value;

	iio_read_channel_processed(st->channel, &value);
	//printk("hlm pogo_pin_timer_callback value=%d\n", value);
	if(value<60){
		pogo_pwr_enable(st, 1);
	}
	else{
		pogo_pwr_enable(st, 0);
	}
}

static int pogo_pin_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct pogo_pin_state *st;
	struct input_dev *input;
	enum iio_chan_type type;
	enum of_gpio_flags flags;
	int value, ret;
	int error;

	st = devm_kzalloc(dev, sizeof(*st), GFP_KERNEL);
	if (!st)
		return -ENOMEM;

	st->channel = devm_iio_channel_get(dev, "pogo_pin");
	if (IS_ERR(st->channel))
		return PTR_ERR(st->channel);

	if (!st->channel->indio_dev)
		return -ENXIO;

	error = iio_get_channel_type(st->channel, &type);
	if (error < 0)
		return error;

	if (type != IIO_VOLTAGE) {
		dev_err(dev, "Incompatible channel type %d\n", type);
		return -EINVAL;
	}

	st->pogo_pwr_en_gpio = of_get_named_gpio_flags(np,
						       "pogo-pwr-en",
						       0,
						       &flags);
	if (st->pogo_pwr_en_gpio < 0) {
		dev_info(&pdev->dev, "Can not read property pogo_pwr_en_gpio\n");
		st->pogo_pwr_en_gpio = -1;
	} else {
		ret = devm_gpio_request_one(dev, st->pogo_pwr_en_gpio,
					    GPIOF_DIR_OUT, NULL);
		if (ret) {
			dev_err(&pdev->dev, "Failed to request pogo_pwr_en_gpio\n");
			return -EINVAL;
		}
		st->pogo_pwr_enable_flag = gpio_get_value(st->pogo_pwr_en_gpio);
	}

	input = devm_input_allocate_device(dev);
	if (!input) {
		dev_err(dev, "failed to allocate input device\n");
		return -ENOMEM;
	}

	input_set_drvdata(input, st);

	input->name = pdev->name;
	input->phys = "pogo-pin/input0";

	input->id.bustype = BUS_HOST;
	input->id.vendor = 0x0001;
	input->id.product = 0x0001;
	input->id.version = 0x0100;

	error = input_setup_polling(input, pogo_pin_poll);
	if (error) {
		dev_err(dev, "Unable to set up polling: %d\n", error);
		return error;
	}

	if (!device_property_read_u32(dev, "poll-interval", &value))
		input_set_poll_interval(input, value);

	error = input_register_device(input);
	if (error) {
		dev_err(dev, "Unable to register input device: %d\n", error);
		return error;
	}

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id pogo_pin_of_match[] = {
	{ .compatible = "khadas-pogo-pin", },
	{ }
};
MODULE_DEVICE_TABLE(of, pogo_pin_of_match);
#endif

static struct platform_driver __refdata pogo_pin_driver = {
	.driver = {
		.name = "pogo-pin",
		.of_match_table = of_match_ptr(pogo_pin_of_match),
	},
	.probe = pogo_pin_probe,
};
module_platform_driver(pogo_pin_driver);

MODULE_AUTHOR("Goenjoy Huang <goenjoy@khadas.com>");
MODULE_DESCRIPTION("Input driver for khadas_pogo-pin");
MODULE_LICENSE("GPL v2");
