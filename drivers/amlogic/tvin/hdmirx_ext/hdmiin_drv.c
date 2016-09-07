/*
 * drivers/amlogic/tvin/hdmirx_ext/hdmiin_drv.c
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

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/slab.h>
/*#include <linux/i2c.h>*/
#include <linux/amlogic/i2c-amlogic.h>
#include <linux/pinctrl/consumer.h>
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/of_gpio.h>

#include "hdmiin_drv.h"
#include "hdmiin_hw_iface.h"
#include "hdmiin_attrs.h"
#include "vdin_iface.h"

#define HDMIIN_I2C_PRESETTING_ADDR	0x92

int hdmiin_debug_print;

static struct hdmiin_drv_s *hdmiin_drv;
static struct class *hdmiin_class;
static dev_t hdmiin_device_id;
static struct device *hdmiin_device;

struct hdmiin_drv_s *hdmiin_get_driver(void)
{
	if (hdmiin_drv == NULL)
		HDMIIN_PE("%s: hdmiin_drv is null\n", __func__);

	return hdmiin_drv;
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

static int hdmiin_start(struct hdmiin_drv_s *pdev)
{
	int ret = 0;
	/* struct i2c_board_info board_info;
	struct i2c_adapter *adapter; */

	/* HDMIIN_PD("[%s] start!\n", __func__); */
	ret = __hw_init();
	if (ret < 0) {
		HDMIIN_PE("[%s] failed to init hw, err = %d!\n", __func__, ret);
		return -2;
	}

/*
	ret = __hw_setup_timer();
	if (ret < 0) {
		HDMIIN_PE("[%s] failed to setup timer, err = %d!\n",
			__func__, ret);
		return -3;
	}

	ret = __hw_setup_irq();
	if (ret < 0) {
		HDMIIN_PE("[%s] failed to setup irq, err = %d!\n",
			__func__, ret);
		return -4;
	}

	ret = __hw_setup_task();
	if (ret < 0) {
		HDMIIN_PE("[%s] failed to setup task, err = %d!\n",
			__func__, ret);
		return -5;
	}
*/

	return ret;
}

static void hdmiin_device_probe(struct platform_device *pdev)
{
	if (hdmiin_debug_print)
		HDMIIN_PD("[%s] start\n", __func__);

	if (!strncmp(hdmiin_drv->name, "sii9135", 7)) {
#if defined(CONFIG_TVIN_HDMI_EXT_SII9135)
		hdmiin_register_hw_sii9135_ops(hdmiin_drv);
#endif
	} else {
		HDMIIN_PE("[%s]: invalid device\n", __func__);
	}
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

static void hdmiin_set_default_params(void)
{
	hdmiin_drv->hw.i2c_addr = 0xff;
	hdmiin_drv->hw.i2c_bus_index = -1;
	/* hdmiin_drv->hw.reset_gpio = 0; */
	hdmiin_drv->hw.reset_gpio.gpio = NULL;
	hdmiin_drv->hw.reset_gpio.flag = 0;
	hdmiin_drv->hw.pin = NULL;
	hdmiin_drv->hw.pinmux_flag = 0;
	/* hdmiin_drv->hw.irq_gpio = 0; */
	/* hdmiin_drv->hw.irq_num  = -1; */

	hdmiin_drv->vdin.started = 0;
	hdmiin_drv->user_cmd     = 0;

	hdmiin_drv->status.cable      = 0;
	hdmiin_drv->status.signal     = 0;
	hdmiin_drv->status.video_mode = 0xff;
	hdmiin_drv->status.audio_sr   = 0;

	hdmiin_drv->hw_ops.get_cable_status		= NULL;
	hdmiin_drv->hw_ops.get_signal_status		= NULL;
	hdmiin_drv->hw_ops.get_input_port		= NULL;
	hdmiin_drv->hw_ops.set_input_port		= NULL;
	hdmiin_drv->hw_ops.get_video_timming		= NULL;
	hdmiin_drv->hw_ops.is_hdmi_mode			= NULL;
	hdmiin_drv->hw_ops.get_audio_sample_rate	= NULL;
	hdmiin_drv->hw_ops.debug			= NULL;
	hdmiin_drv->hw_ops.get_chip_version		= NULL;
	hdmiin_drv->hw_ops.init				= NULL;

	return;
}

#ifdef CONFIG_OF
static int hdmiin_get_of_data(struct device *dev)
{
	struct device_node *hdmiin_node = dev->of_node;
	unsigned int i2c_index;
	const char *str;
	unsigned int val;
	int ret = 0;

	/* for device name */
	ret = of_property_read_string(hdmiin_node, "hdmiin_dev", &str);
	if (ret) {
		HDMIIN_PW("failed to get hdmiin_dev\n");
		str = "invalid";
	}
	strcpy(hdmiin_drv->name, str);

	/* for i2c address */
	ret = of_property_read_u32(hdmiin_node, "i2c_addr", &val);
	if (ret) {
		HDMIIN_PE("[%s] failed to get i2c_addr\n", __func__);
	} else {
		hdmiin_drv->hw.i2c_addr = (unsigned char)val;
		HDMIIN_PD("i2c_addr = 0x%02x\n", hdmiin_drv->hw.i2c_addr);
	}

	/* for i2c bus */
	ret = of_property_read_string(hdmiin_node, "i2c_bus", &str);
	if (ret) {
		HDMIIN_PE("[%s] failed to get i2c_bus\n", __func__);
		return -1;
	} else {
		if (!strncmp(str, "i2c_bus_ao", 10))
			i2c_index = AML_I2C_MASTER_AO;
		else if (!strncmp(str, "i2c_bus_a", 9))
			i2c_index = AML_I2C_MASTER_A;
		else if (!strncmp(str, "i2c_bus_b", 9))
			i2c_index = AML_I2C_MASTER_B;
		else if (!strncmp(str, "i2c_bus_c", 9))
			i2c_index = AML_I2C_MASTER_C;
		else if (!strncmp(str, "i2c_bus_d", 9))
			i2c_index = AML_I2C_MASTER_D;
		else {
			HDMIIN_PE("[%s] invalid i2c bus \"%s\" from dts\n",
				__func__, str);
			return -1;
		}
		HDMIIN_PD("i2c_bus = %s\n", str);
	}
	hdmiin_drv->hw.i2c_bus_index = i2c_index;

	/* for gpio_reset */
	ret = of_property_read_string(hdmiin_node, "reset_gpio_name", &str);
	if (ret) {
		HDMIIN_PW("failed to get reset_gpio_name\n");
		str = "invalid";
	}
	strcpy(hdmiin_drv->hw.reset_gpio.name, str);
	/* request gpio */
	hdmiin_drv->hw.reset_gpio.gpio = devm_gpiod_get(dev, "reset");
	if (IS_ERR(hdmiin_drv->hw.reset_gpio.gpio)) {
		hdmiin_drv->hw.reset_gpio.flag = 0;
		HDMIIN_PW("register reset_gpio %s: %p, err: %ld\n",
			hdmiin_drv->hw.reset_gpio.name,
			hdmiin_drv->hw.reset_gpio.gpio,
			IS_ERR(hdmiin_drv->hw.reset_gpio.gpio));
	} else {
		hdmiin_drv->hw.reset_gpio.flag = 1;
		HDMIIN_PD("register gpio %s: %p\n",
			hdmiin_drv->hw.reset_gpio.name,
			hdmiin_drv->hw.reset_gpio.gpio);
	}

/*
// for irq gpio pin
	ret = of_property_read_string(hdmiin_node,"irq_gpio",&str);
	if(ret) {
		HDMIIN_PW("[%s] failed to get irq_gpio!\n",__func__);
	} else {
		HDMIIN_PD("__JT_ hdmiin_of, irq_gpio = %s\n", str);
		ret = amlogic_gpio_name_map_num(str);
		if(ret < 0 ) {
			HDMIIN_PW("[%s] failed to map irq_gpio!\n",__func__);
		} else
			hdmiin_drv->hw.irq_gpio = ret;
	}

// for irq number, int0 ~ int7
	ret = of_property_read_string(hdmiin_node,"irq_num",&str);
	if(ret) {
		HDMIIN_PW("[%s] failed to get irq_num!\n",__func__);
	} else {
		HDMIIN_PD("__JT_ hdmiin_of, irq_num = %s\n", str);
		hdmiin_drv->hw.irq_num = simple_strtoul(str, NULL, 0);
	}
*/

	return 0;
}
#endif

static int hdmiin_open(struct inode *node, struct file *filp)
{
	struct hdmiin_drv_s *pdev;

	pdev = container_of(node->i_cdev, struct hdmiin_drv_s, cdev);
	filp->private_data = pdev;

	return 0;
}

static int hdmiin_release(struct inode *node, struct file *filp)
{
	return 0;
}

static const struct file_operations hdmiin_fops = {
	.owner		= THIS_MODULE,
	.open		= hdmiin_open,
	.release	= hdmiin_release,
};

static int hdmiin_probe(struct platform_device *pdev)
{
	int ret = -1;

	hdmiin_debug_print = 0;
	/* HDMIIN_PI("[%s] start!\n", __func__); */

	hdmiin_drv = kmalloc(sizeof(struct hdmiin_drv_s), GFP_KERNEL);
	if (!hdmiin_drv) {
		ret = -ENOMEM;
		HDMIIN_PE("[%s] failed to alloc hdmiin_drv!\n", __func__);
		goto fail;
	}
	hdmiin_drv->dev = &pdev->dev;
	hdmiin_drv->vops = get_vdin_v4l2_ops();

	/* hdmiin_drv default value setting */
	hdmiin_set_default_params();

#ifdef CONFIG_OF
	ret = hdmiin_get_of_data(&pdev->dev);
	if (ret) {
		HDMIIN_PE("[%s] invalid dts config\n", __func__);
		goto cleanup;
	}
#endif
	hdmiin_device_probe(pdev);

	ret = alloc_chrdev_region(&hdmiin_device_id, 0, 1, HDMIIN_DEV_NAME);
	if (ret < 0) {
		HDMIIN_PE("[%s] failed to alloc char dev\n", __func__);
		goto cleanup;
	}

	cdev_init(&(hdmiin_drv->cdev), &hdmiin_fops);
	hdmiin_drv->cdev.owner = THIS_MODULE;

	ret = cdev_add(&(hdmiin_drv->cdev), hdmiin_device_id, 1);
	if (ret) {
		HDMIIN_PE("[%s] failed to add hdmiin cdev, err = %d\n",
			__func__, ret);
		goto unregister;
	}

	hdmiin_class = class_create(THIS_MODULE, HDMIIN_DRV_NAME);
	if (IS_ERR(hdmiin_class)) {
		ret = PTR_ERR(hdmiin_class);
		HDMIIN_PE("[%s] failed to create hdmiin class, err = %d\n",
			__func__, ret);
		goto destroy_cdev;
	}

	hdmiin_device = device_create(hdmiin_class, NULL, hdmiin_device_id,
		NULL, "%s", HDMIIN_DEV_NAME);
	if (IS_ERR(hdmiin_device)) {
		ret = PTR_ERR(hdmiin_device);
		HDMIIN_PE("[%s] failed to create hdmiin device, err = %d\n",
			__func__, ret);
		goto destroy_class;
	}

	ret = hdmiin_create_device_attrs(hdmiin_device);
	if (ret < 0) {
		HDMIIN_PE("[%s] failed to create device attrs, err = %d\n",
			__func__, ret);
		goto destroy_device;
	}

	dev_set_drvdata(hdmiin_device, hdmiin_drv);

	ret = hdmiin_start(hdmiin_drv);
	if (ret) {
		HDMIIN_PE("[%s] failed to setup resource, err = %d\n",
			__func__, ret);
		goto destroy_attrs;
	}

	HDMIIN_PI("[%s] success to run hdmiin\n", __func__);

	return 0;

destroy_attrs:
	hdmiin_destroy_device_attrs(hdmiin_device);

destroy_device:
	device_destroy(hdmiin_class, hdmiin_device_id);

destroy_class:
	class_destroy(hdmiin_class);

destroy_cdev:
	cdev_del(&(hdmiin_drv->cdev));

unregister:
	unregister_chrdev_region(MKDEV(MAJOR(hdmiin_device_id),
		MINOR(hdmiin_device_id)), 1);

cleanup:
	kfree(hdmiin_drv);

fail:
	return ret;
}

static int hdmiin_remove(struct platform_device *pdev)
{
	HDMIIN_PI("[%s] start\n", __func__);

	hdmiin_destroy_device_attrs(hdmiin_device);

	if (hdmiin_class) {
		device_destroy(hdmiin_class, hdmiin_device_id);
		class_destroy(hdmiin_class);
	}

	if (hdmiin_drv) {
		cdev_del(&(hdmiin_drv->cdev));
		kfree(hdmiin_drv);
	}

	unregister_chrdev_region(hdmiin_device_id, 1);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id hdmiin_dt_match[] = {
	{
		.compatible = "amlogic, hdmirx_ext",
	},
	{},
};
#else
#define hdmiin_dt_match  NULL
#endif


static struct platform_driver hdmiin_driver = {
	.probe  = hdmiin_probe,
	.remove = hdmiin_remove,
	.driver = {
		.name		= HDMIIN_DRV_NAME,
		.owner		= THIS_MODULE,
		.of_match_table = hdmiin_dt_match,
	}
};
static int __init hdmiin_drv_init(void)
{
	HDMIIN_PI("[%s] ver: %s\n", __func__, HDMIIN_DRV_VER);

	if (platform_driver_register(&hdmiin_driver)) {
		HDMIIN_PE("[%s] failed to register driver\n", __func__);
		return -ENODEV;
	}

	return 0;
}

static void __exit hdmiin_drv_exit(void)
{
	HDMIIN_PFUNC();
	platform_driver_unregister(&hdmiin_driver);
	class_unregister(hdmiin_class);

	return;
}

late_initcall(hdmiin_drv_init);
module_exit(hdmiin_drv_exit);

MODULE_DESCRIPTION("AML HDMIRX_EXT driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0.0");
