/*
 * drivers/amlogic/wifi/wifi_dt.c
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

#include <linux/amlogic/wifi_dt.h>
#ifdef CONFIG_DHD_USE_STATIC_BUF
#include <linux/amlogic/dhd_buf.h>
#endif

#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/of_irq.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/pinctrl/consumer.h>
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/of_gpio.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/iomap.h>
#include <linux/io.h>

#define OWNER_NAME "sdio_wifi"

int wifi_power_gpio = 0;
int wifi_power_gpio2 = 0;

struct wifi_plat_info {
	int interrupt_pin;
	int irq_num;
	int irq_trigger_type;

	int power_on_pin;
	int power_on_pin_level;
	int power_on_pin2;

	int clock_32k_pin;
	struct gpio_desc *interrupt_desc;
	struct gpio_desc *powe_desc;

	int plat_info_valid;
	struct pinctrl *p;
	struct device		*dev;
};

static struct wifi_plat_info wifi_info;

#define WIFI_INFO(fmt, args...) \
		dev_info(wifi_info.dev, "[%s] " fmt, __func__, ##args);

#ifdef CONFIG_OF
static const struct of_device_id wifi_match[] = {
	{	.compatible = "amlogic, aml_broadcm_wifi",
		.data		= (void *)&wifi_info
	},
	{},
};

static struct wifi_plat_info *wifi_get_driver_data
	(struct platform_device *pdev)
{
	const struct of_device_id *match;
	match = of_match_node(wifi_match, pdev->dev.of_node);
	return (struct wifi_plat_info *)match->data;
}
#else
#define wifi_match NULL
#endif

#define CHECK_PROP(ret, msg, value)	\
{	\
	if (ret) { \
		WIFI_INFO("wifi_dt : no prop for %s\n", msg);	\
		return -1;	\
	} \
}	\

/*
#define CHECK_RET(ret) \
	if (ret) \
		WIFI_INFO("wifi_dt : gpio op failed(%d)
			at line %d\n", ret, __LINE__)
*/

/* extern const char *amlogic_cat_gpio_owner(unsigned int pin); */

#define SHOW_PIN_OWN(pin_str, pin_num)	\
	WIFI_INFO("%s(%d)\n", pin_str, pin_num)

static int set_wifi_power(int value)
{
	return gpio_direction_output(wifi_info.power_on_pin,
				value);
}

static int set_wifi_power2(int value)
{
	return gpio_direction_output(wifi_info.power_on_pin2,
				value);
}

static int wifi_dev_probe(struct platform_device *pdev)
{
	int ret;

#ifdef CONFIG_OF
	struct wifi_plat_info *plat;
	const char *value;
	struct gpio_desc *desc;
#else
	struct wifi_plat_info *plat =
	 (struct wifi_plat_info *)(pdev->dev.platform_data);
#endif

#ifdef CONFIG_OF
	if (pdev->dev.of_node) {
		plat = wifi_get_driver_data(pdev);
		plat->plat_info_valid = 0;
		plat->dev = &pdev->dev;

		ret = of_property_read_string(pdev->dev.of_node,
			"interrupt_pin", &value);
		CHECK_PROP(ret, "interrupt_pin", value);
		desc = of_get_named_gpiod_flags(pdev->dev.of_node,
			"interrupt_pin", 0, NULL);
		plat->interrupt_desc = desc;
		plat->interrupt_pin = desc_to_gpio(desc);

		/* amlogic_gpio_name_map_num(value); */

		plat->irq_num = irq_of_parse_and_map(pdev->dev.of_node, 0);
		/*
		ret = of_property_read_u32(pdev->dev.of_node,
			"irq_num", &plat->irq_num);
		*/
		CHECK_PROP(ret, "irq_num", "null");


		ret = of_property_read_string(pdev->dev.of_node,
		"irq_trigger_type", &value);
		CHECK_PROP(ret, "irq_trigger_type", value);
		if (strcmp(value, "GPIO_IRQ_HIGH") == 0)
			plat->irq_trigger_type = GPIO_IRQ_HIGH;
		else if (strcmp(value, "GPIO_IRQ_LOW") == 0)
			plat->irq_trigger_type = GPIO_IRQ_LOW;
		else if (strcmp(value, "GPIO_IRQ_RISING") == 0)
			plat->irq_trigger_type = GPIO_IRQ_RISING;
		else if (strcmp(value, "GPIO_IRQ_FALLING") == 0)
			plat->irq_trigger_type = GPIO_IRQ_FALLING;
		else {
			WIFI_INFO("wifi_dt:unknown irq trigger type-%s\n",
			 value);
			return -1;
		}


		ret = of_property_read_string(pdev->dev.of_node,
			"power_on_pin", &value);
		if (!ret) {
			CHECK_PROP(ret, "power_on_pin", value);
			wifi_power_gpio = 1;
			desc = of_get_named_gpiod_flags(pdev->dev.of_node,
			"power_on_pin", 0, NULL);
			plat->powe_desc = desc;
			plat->power_on_pin = desc_to_gpio(desc);
			/* amlogic_gpio_name_map_num(value); */
		}

		ret = of_property_read_u32(pdev->dev.of_node,
		"power_on_pin_level", &plat->power_on_pin_level);

		ret = of_property_read_string(pdev->dev.of_node,
			"power_on_pin2", &value);
		if (!ret) {
			CHECK_PROP(ret, "power_on_pin2", value);
			wifi_power_gpio2 = 1;
			desc = of_get_named_gpiod_flags(pdev->dev.of_node,
				"power_on_pin2", 0, NULL);
			plat->power_on_pin2 = desc_to_gpio(desc);
			/* amlogic_gpio_name_map_num(value); */
		}
#if 0
		ret = of_property_read_string(pdev->dev.of_node,
			"clock_32k_pin", &value);
		/* CHECK_PROP(ret, "clock_32k_pin", value); */
		if (ret)
			plat->clock_32k_pin = 0;
		else
			desc = of_get_named_gpiod_flags(pdev->dev.of_node,
				"clock_32k_pin", 0, NULL);
			plat->clock_32k_pin = desc_to_gpio(desc);
#endif
			/* amlogic_gpio_name_map_num(value); */
		if (of_get_property(pdev->dev.of_node,
			"pinctrl-names", NULL)) {
			unsigned int pwm_misc;

			if (get_cpu_type() == MESON_CPU_MAJOR_ID_GXBB) {
				/* pwm_e */
				WIFI_INFO("set pwm as 32k output");
				aml_write_cbus(0x21b0, 0x7f107f2);
				pwm_misc = aml_read_cbus(0x21b2);
				pwm_misc &= ~((0x7f << 8) | (3 << 4) |
					(1 << 2) | (1 << 0));
				pwm_misc |= ((1 << 15) | (4 << 8) | (3 << 4));
				aml_write_cbus(0x21b2, pwm_misc);
				aml_write_cbus(0x21b2, (pwm_misc | (1 << 0)));
			} else if (get_cpu_type() == MESON_CPU_MAJOR_ID_M8B) {
				/* pwm_e */
				WIFI_INFO("set pwm as 32k output");
				aml_write_cbus(0x21b0, 0x7980799);
				pwm_misc = aml_read_cbus(0x21b2);
				pwm_misc &= ~((0x7f << 8) | (3 << 4) |
					(1 << 2) | (1 << 0));
				pwm_misc |= ((1 << 15) | (4 << 8) | (2 << 4));
				aml_write_cbus(0x21b2, pwm_misc);
				aml_write_cbus(0x21b2, (pwm_misc | (1 << 0)));
			}

			plat->p = devm_pinctrl_get_select(&pdev->dev,
				"wifi_32k_pins");
		}
#ifdef CONFIG_DHD_USE_STATIC_BUF
		if (of_get_property(pdev->dev.of_node,
			"dhd_static_buf", NULL)) {
			WIFI_INFO("dhd_static_buf setup\n");
			bcmdhd_init_wlan_mem();
		}
#endif

		plat->plat_info_valid = 1;

		WIFI_INFO("interrupt_pin=%d\n", plat->interrupt_pin);
		WIFI_INFO("irq_num=%d, irq_trigger_type=%d\n",
			plat->irq_num, plat->irq_trigger_type);
		WIFI_INFO("power_on_pin=%d\n", plat->power_on_pin);
		WIFI_INFO("clock_32k_pin=%d\n", plat->clock_32k_pin);
	}
#endif

	return 0;
}

static int wifi_dev_remove(struct platform_device *pdev)
{
	WIFI_INFO("wifi_dev_remove\n");
	return 0;
}

static struct platform_driver wifi_plat_driver = {
	.probe = wifi_dev_probe,
	.remove = wifi_dev_remove,
	.driver = {
	.name = "aml_broadcm_wifi",
	.owner = THIS_MODULE,
	.of_match_table = wifi_match
	},
};

static int __init wifi_dt_init(void)
{
	int ret;
	ret = platform_driver_register(&wifi_plat_driver);
	return ret;
}
/* module_init(wifi_dt_init); */
fs_initcall_sync(wifi_dt_init);

static void __exit wifi_dt_exit(void)
{
	platform_driver_unregister(&wifi_plat_driver);
}
module_exit(wifi_dt_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("AMLOGIC");
MODULE_DESCRIPTION("wifi device tree driver");

#ifdef CONFIG_OF

int wifi_setup_dt(void)
{
	int ret;
	uint flag;

	WIFI_INFO("wifi_setup_dt\n");
	if (!wifi_info.plat_info_valid) {
		WIFI_INFO("wifi_setup_dt : invalid device tree setting\n");
		return -1;
	}

/*
#if ((!(defined CONFIG_ARCH_MESON8))
	&& (!(defined CONFIG_ARCH_MESON8B)))
	//setup sdio pullup
	aml_clr_reg32_mask(P_PAD_PULL_UP_REG4,
		0xf|1<<8|1<<9|1<<11|1<<12);
	aml_clr_reg32_mask(P_PAD_PULL_UP_REG2,1<<7|1<<8|1<<9);
#endif
*/

	/* setup irq */
	ret = gpio_request(wifi_info.interrupt_pin,
		OWNER_NAME);
	if (ret)
		WIFI_INFO("interrupt_pin request failed(%d)\n", ret);
	ret = gpio_set_pullup(wifi_info.interrupt_pin, 1);
	if (ret)
		WIFI_INFO("interrupt_pin disable pullup failed(%d)\n", ret)
	ret = gpio_direction_input(wifi_info.interrupt_pin);
	if (ret)
		WIFI_INFO("set interrupt_pin input failed(%d)\n", ret);
	if (wifi_info.irq_num) {
		flag = AML_GPIO_IRQ(wifi_info.irq_num,
			FILTER_NUM4, wifi_info.irq_trigger_type);
	} else {
		WIFI_INFO("wifi_dt : unsupported irq number - %d\n",
			wifi_info.irq_num);
		return -1;
	}
	ret = gpio_for_irq(wifi_info.interrupt_pin, flag);
	if (ret)
		WIFI_INFO("gpio to irq failed(%d)\n", ret)
	SHOW_PIN_OWN("interrupt_pin", wifi_info.interrupt_pin);

	/* setup power */
	if (wifi_power_gpio) {
		ret = gpio_request(wifi_info.power_on_pin, OWNER_NAME);
		if (ret)
			WIFI_INFO("power_on_pin request failed(%d)\n", ret);
		if (wifi_info.power_on_pin_level)
			ret = set_wifi_power(1);
		else
			ret = set_wifi_power(0);
		if (ret)
			WIFI_INFO("power_on_pin output failed(%d)\n", ret);
		SHOW_PIN_OWN("power_on_pin", wifi_info.power_on_pin);
	}

	if (wifi_power_gpio2) {
		ret = gpio_request(wifi_info.power_on_pin2,
			OWNER_NAME);
		if (ret)
			WIFI_INFO("power_on_pin2 request failed(%d)\n", ret);
		ret = set_wifi_power2(0);
		if (ret)
			WIFI_INFO("power_on_pin2 output failed(%d)\n", ret);
		SHOW_PIN_OWN("power_on_pin2", wifi_info.power_on_pin2);
	}

	return 0;
}
EXPORT_SYMBOL(wifi_setup_dt);

void wifi_teardown_dt(void)
{

	WIFI_INFO("wifi_teardown_dt\n");
	if (!wifi_info.plat_info_valid) {
		WIFI_INFO("wifi_teardown_dt : invalid device tree setting\n");
		return;
	}

	if (wifi_power_gpio)
		gpio_free(wifi_info.power_on_pin);

	if (wifi_power_gpio2)
		gpio_free(wifi_info.power_on_pin2);

	gpio_free(wifi_info.interrupt_pin);

}
EXPORT_SYMBOL(wifi_teardown_dt);

void extern_wifi_set_enable(int is_on)
{
	int ret = 0;

	if (is_on) {
		if (wifi_power_gpio) {
			if (wifi_info.power_on_pin_level)
				ret = set_wifi_power(0);
			else
				ret = set_wifi_power(1);

			if (ret)
				WIFI_INFO("wifi power failed(%d)\n", ret);
		}
		if (wifi_power_gpio2) {
			if (wifi_info.power_on_pin_level)
				ret = set_wifi_power2(0);
			else
				ret = set_wifi_power2(1);
			if (ret)
				WIFI_INFO("wifi power2 failed(%d)\n", ret);
		}
		WIFI_INFO("WIFI  Enable! %d\n", wifi_info.power_on_pin);
	} else {
		if (wifi_power_gpio) {
			if (wifi_info.power_on_pin_level)
				ret = set_wifi_power(1);
			else
				ret = set_wifi_power(0);

			if (ret)
				WIFI_INFO("wifi power down failed(%d)\n", ret);
		}
		if (wifi_power_gpio2) {
			if (wifi_info.power_on_pin_level)
				ret = set_wifi_power2(1);
			else
				ret = set_wifi_power2(0);
			if (ret)
				WIFI_INFO("wifi power2 down failed(%d)\n", ret);
		}
		WIFI_INFO("WIFI  Disable! %d\n", wifi_info.power_on_pin);

	}
}
EXPORT_SYMBOL(extern_wifi_set_enable);

int wifi_irq_num(void)
{
	return wifi_info.irq_num;
}
EXPORT_SYMBOL(wifi_irq_num);

int wifi_irq_trigger_level(void)
{
	return wifi_info.irq_trigger_type;
}
EXPORT_SYMBOL(wifi_irq_trigger_level);
#else

int wifi_setup_dt(void)
{
	return 0;
}
EXPORT_SYMBOL(wifi_setup_dt);

void wifi_teardown_dt(void)
{
}
EXPORT_SYMBOL(wifi_teardown_dt);

#endif
