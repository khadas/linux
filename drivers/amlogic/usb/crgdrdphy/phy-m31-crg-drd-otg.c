// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/irqreturn.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/pm_runtime.h>
#include <linux/i2c.h>
#include <linux/regmap.h>
#include <linux/delay.h>
#include <linux/usb/phy.h>
#include <linux/of_gpio.h>
#include <linux/amlogic/usb-v2.h>
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/workqueue.h>
#include <linux/notifier.h>
#include <linux/amlogic/usbtype.h>
#include "../phy/phy-aml-new-usb-v2.h"

#include <linux/amlogic/gki_module.h>
#define HOST_MODE	0
#define DEVICE_MODE	1

static int UDC_exist_flag = -1;
static char crg_UDC_name[128];

const struct regmap_config hd3s3200_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.cache_type = REGCACHE_RBTREE,
};

struct hd3s3200_priv {
	struct device           *dev;
	struct i2c_client *i2c;
	struct regmap *regmap;
	struct delayed_work     work;
	struct gpio_desc *idgpiodesc;
	struct gpio_desc *usb_gpio_desc;
	int vbus_power_pin;
};

bool m31_crg_force_device_mode;
module_param_named(otg_device, m31_crg_force_device_mode,
		bool, 0644);

static char m31_otg_mode_string[2] = "0";
static int m31_force_otg_mode(char *s)
{
	if (s)
		sprintf(m31_otg_mode_string, "%s", s);
	if (strncmp(m31_otg_mode_string, "0", 1) == 0)
		m31_crg_force_device_mode = 0;
	else
		m31_crg_force_device_mode = 1;
	return 0;
}
__setup("otg_device=", m31_force_otg_mode);

static u8 aml_m31_i2c_read(struct i2c_client *client, u16 reg)
{
	int err;

	err = i2c_smbus_read_byte_data(client, reg & 0xff);
	if (err < 0) {
		dev_err(&client->dev,
			"Failed to read from register 0x%03x, err %d\n",
			(int)reg, err);
		return 0x00;	/* Arbitrary */
	}

	return err;
}

static int amlogic_m31_crg_otg_init(struct hd3s3200_priv *hd3s3200)
{
	/* to do */

	return 0;
}

static void set_mode(u32 mode)
{
	/* to do */
}

static void set_usb_vbus_power
	(struct gpio_desc *usb_gd, int pin, char is_power_on)
{
	if (is_power_on)
		/*set vbus on by gpio*/
		gpiod_direction_output(usb_gd, 1);
	else
		/*set vbus off by gpio first*/
		gpiod_direction_output(usb_gd, 0);
}

static void amlogic_m31_set_vbus_power
		(struct hd3s3200_priv *hd3s3200, char is_power_on)
{
	if (hd3s3200->vbus_power_pin != -1)
		set_usb_vbus_power(hd3s3200->usb_gpio_desc,
				   hd3s3200->vbus_power_pin, is_power_on);
}

static void amlogic_m31_crg_otg_work(struct work_struct *work)
{
	int ret;
	int current_mode = 0;
	struct hd3s3200_priv *hd3s3200 =
		container_of(work, struct hd3s3200_priv, work.work);

	current_mode = aml_m31_i2c_read(hd3s3200->i2c, 9);
	if (current_mode < 0) {
		dev_info(hd3s3200->dev, "work hd3s3200 i2c error\n");
		return;
	}

	current_mode = ((current_mode >> 6) & 0x3);
	dev_info(hd3s3200->dev, "work current_mode is 0x%x\n", current_mode);

	if (current_mode == 1) {
		/* to do*/
		crg_gadget_exit();
		amlogic_m31_set_vbus_power(hd3s3200, 1);
		set_mode(HOST_MODE);
		crg_init();
	} else {
		/* to do*/
		crg_exit();
		set_mode(DEVICE_MODE);
		amlogic_m31_set_vbus_power(hd3s3200, 0);
		crg_gadget_init();
		if (UDC_exist_flag != 1) {
			ret = crg_otg_write_UDC(crg_UDC_name);
			if (ret == 0 || ret == -EBUSY)
				UDC_exist_flag = 1;
		}
	}
}

static irqreturn_t phy_m31_id_gpio_detect_irq(int irq, void *dev)
{
	struct hd3s3200_priv *hd3s3200 = (struct hd3s3200_priv *)dev;

	/* to do */

	schedule_delayed_work(&hd3s3200->work, msecs_to_jiffies(10));

	return IRQ_HANDLED;
}

static int phy_m31_id_pin_config(struct hd3s3200_priv *hd3s3200)
{
	int ret;
	struct gpio_desc *desc;
	int id_irqnr;

	desc = gpiod_get_index(hd3s3200->dev, NULL, 1, GPIOD_IN);
	if (IS_ERR(desc)) {
		pr_err("fail to get id-gpioirq\n");
		return -1;
	}

	hd3s3200->idgpiodesc = desc;

	id_irqnr = gpiod_to_irq(desc);

	ret = devm_request_irq(hd3s3200->dev, id_irqnr,
			       phy_m31_id_gpio_detect_irq,
			       ID_GPIO_IRQ_FLAGS,
			       "phy_aml_id_gpio_detect", hd3s3200);

	if (ret) {
		pr_err("failed to request ret=%d, id_irqnr=%d\n",
		       ret, id_irqnr);
		return -ENODEV;
	}

	pr_info("<%s> ok\n", __func__);

	return 0;
}

static int hd3s3200_i2c_probe(struct i2c_client *i2c,
			      const struct i2c_device_id *id)
{
	const void *prop;
	struct device *dev = &i2c->dev;
	struct gpio_desc *usb_gd = NULL;
	struct regmap *regmap;
	struct regmap_config config = hd3s3200_regmap;
	struct hd3s3200_priv *hd3s3200;
	int len, otg = 0;
	int controller_type = USB_NORMAL;
	const char *udc_name = NULL;
	u32 current_mode = 0;
	const char *gpio_name = NULL;
	int gpio_vbus_power_pin = -1;

	hd3s3200 = devm_kzalloc(&i2c->dev,
		sizeof(struct hd3s3200_priv), GFP_KERNEL);
	if (!hd3s3200)
		return -ENOMEM;

	regmap = devm_regmap_init_i2c(i2c, &config);
	if (IS_ERR(regmap))
		return PTR_ERR(regmap);

	gpio_name = of_get_property(dev->of_node, "gpio-vbus-power", NULL);
	if (gpio_name) {
		gpio_vbus_power_pin = 1;
		usb_gd = devm_gpiod_get_index
			(&i2c->dev, NULL, 0, GPIOD_OUT_LOW);
		if (IS_ERR(usb_gd))
			return -1;
	}

	prop = of_get_property(dev->of_node, "controller-type", NULL);
	if (prop)
		controller_type = of_read_ulong(prop, 1);

	udc_name = of_get_property(i2c->dev.of_node, "udc-name", NULL);
	if (!udc_name)
		udc_name = "fdd00000.crgudc2";
	len = strlen(udc_name);
	if (len >= 128) {
		dev_info(&i2c->dev, "udc_name is too long: %d\n", len);
		return -EINVAL;
	}
	strncpy(crg_UDC_name, udc_name, len);
	crg_UDC_name[len] = '\0';

	if (controller_type == USB_OTG)
		otg = 1;

	dev_info(&i2c->dev, "controller_type is %d\n", controller_type);
	dev_info(&i2c->dev, "force_device_mode is %d\n",
		 m31_crg_force_device_mode);
	dev_info(&i2c->dev, "otg is %d\n", otg);
	dev_info(&i2c->dev, "udc_name: %s\n", crg_UDC_name);

	hd3s3200->vbus_power_pin = gpio_vbus_power_pin;
	hd3s3200->usb_gpio_desc = usb_gd;
	hd3s3200->regmap = regmap;
	hd3s3200->i2c = i2c;
	hd3s3200->dev = dev;
	dev_set_drvdata(&i2c->dev, hd3s3200);

	INIT_DELAYED_WORK(&hd3s3200->work, amlogic_m31_crg_otg_work);

	if (otg)
		phy_m31_id_pin_config(hd3s3200);

	amlogic_m31_crg_otg_init(hd3s3200);

	if (otg == 0) {
		if (m31_crg_force_device_mode ||
		 controller_type == USB_DEVICE_ONLY) {
			set_mode(DEVICE_MODE);
			amlogic_m31_set_vbus_power(hd3s3200, 0);
			crg_gadget_init();
		} else if (controller_type == USB_HOST_ONLY) {
			crg_init();
			amlogic_m31_set_vbus_power(hd3s3200, 1);
			set_mode(HOST_MODE);
		}
	} else {
		current_mode = aml_m31_i2c_read(hd3s3200->i2c, 9);
		current_mode = ((current_mode >> 6) & 0x3);
		dev_info(&i2c->dev, "probe current_mode is 0x%x\n",
			 current_mode);
		if (current_mode == 1) {
			amlogic_m31_set_vbus_power(hd3s3200, 1);
			set_mode(HOST_MODE);
			crg_init();
		} else {
			set_mode(DEVICE_MODE);
			amlogic_m31_set_vbus_power(hd3s3200, 0);
			crg_gadget_init();
		}
	}

	return 0;
}

static int hd3s3200_i2c_remove(struct i2c_client *i2c)
{
	devm_kfree(&i2c->dev, i2c_get_clientdata(i2c));

	return 0;
}

static const struct i2c_device_id hd3s3200_i2c_id[] = {
	{"hd3s3200",},
	{}
};

MODULE_DEVICE_TABLE(i2c, hd3s3200_i2c_id);

#ifdef CONFIG_OF
static const struct of_device_id hd3s3200_of_match[] = {
	{.compatible = "ti, hd3s3200",},
	{}
};

MODULE_DEVICE_TABLE(of, tas5805m_of_match);
#endif

static struct i2c_driver hd3s3200_i2c_driver = {
	.probe = hd3s3200_i2c_probe,
	.remove = hd3s3200_i2c_remove,
	.id_table = hd3s3200_i2c_id,
	.driver = {
		   .name = "hd3s3200",
		   .of_match_table = hd3s3200_of_match,
		   },
};

module_i2c_driver(hd3s3200_i2c_driver);

MODULE_AUTHOR("Amlogic Inc.");
MODULE_DESCRIPTION("amlogic m31 crg otg driver");
MODULE_LICENSE("GPL v2");
