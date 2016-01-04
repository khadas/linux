/*
 * Amlogic iw7019 Driver for LCD Panel Backlight
 *
 * Author:
 *
 * Copyright (C) 2015 Amlogic Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/device.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/spinlock.h>
#include <linux/irq.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/amlogic/aml_gpio_consumer.h>
/*#include <linux/workqueue.h>*/
#include "iw7019_lpf.h"
#include "iw7019_bl.h"
#include "ldim_extern.h"
#define INT_VIU_VSYNC 35

#define NORMAL_MSG (0<<7)
#define BROADCAST_MSG (1<<7)
#define BLOCK_DATA (0<<6)
#define SINGLE_DATA (1<<6)
#define IW7019_DEV_ADDR 1

#define IW7019_REG_BRIGHTNESS 0x01
#define IW7019_REG_CHIPID 0x7f
#define IW7019_CHIPID 0x28

static int test_mode = 1;
struct tasklet_struct   ldim_tasklet;
spinlock_t  vsync_isr_lock;

struct iw7019 {
	int en_gpio;
	int irq;
	struct spi_device *spi;
	struct class cls;
	u8 cur_addr;
};

static struct iw7019 *iw7019_bl;

static u8 iw7019_ini_data[][2] = {
	/* step1: */
	{0x1, 0x8},
	/* step2:disable ocp and otp */
	{0x34, 0x14},
	{0x10, 0x53},
	/* step3: */
	{0x11, 0x00},
	{0x12, 0x02},
	{0x13, 0x00},
	{0x14, 0x40},
	{0x15, 0x06},
	{0x16, 0x00},
	{0x17, 0x80},
	{0x18, 0x0a},
	{0x19, 0x00},
	{0x1a, 0xc0},
	{0x1b, 0x0e},
	{0x1c, 0x00},
	{0x1d, 0x00},
	{0x1e, 0x50},
	{0x1f, 0x00},
	{0x20, 0x63},
	{0x21, 0xff},
	{0x2a, 0xff},
	{0x2b, 0x41},
	{0x2c, 0x28},
	{0x30, 0xff},
	{0x31, 0x00},
	{0x32, 0x0f},
	{0x33, 0x40},
	{0x34, 0x40},
	{0x35, 0x00},
	{0x3f, 0xa3},
	{0x45, 0x00},
	{0x47, 0x04},
	{0x48, 0x60},
	{0x4a, 0x0d},
	/* step4:set brightness */
	{0x01, 0xff},
	{0x02, 0xff},
	{0x03, 0xff},
	{0x04, 0xff},
	{0x05, 0xff},
	{0x06, 0xff},
	{0x07, 0xff},
	{0x08, 0xff},
	{0x09, 0xff},
	{0x0a, 0xff},
	{0x0b, 0xff},
	{0x0c, 0xff},
	/* step5: */
	{0x00, 0x09},
	/* step6: */
	{0x34, 0x00},
};

static int test_brightness[] = {
	0xccc, 0xccc, 0xccc, 0xccc, 0xccc, 0xccc, 0xccc, 0xccc
};



static int iw7019_rreg(struct spi_device *spi, u8 addr, u8 *val)
{
	u8 tbuf[3];
	int ret;

	dirspi_start(spi);
	tbuf[0] = NORMAL_MSG | SINGLE_DATA | IW7019_DEV_ADDR;
	tbuf[1] = addr | 0x80;
	tbuf[2] = 0;
	ret = dirspi_write(spi, tbuf, 3);
	ret = dirspi_read(spi, val, 1);
	dirspi_stop(spi);

	return ret;
}

static int iw7019_wreg(struct spi_device *spi, u8 addr, u8 val)
{
	u8 tbuf[3];
	int ret;

	dirspi_start(spi);
	tbuf[0] = NORMAL_MSG | SINGLE_DATA | IW7019_DEV_ADDR;
	tbuf[1] = addr & 0x7f;
	tbuf[2] = val;
	ret = dirspi_write(spi, tbuf, 3);
	dirspi_stop(spi);

	return ret;
}

static int iw7019_wregs(struct spi_device *spi, u8 addr, u8 *val, int len)
{
	u8 tbuf[20];
	int ret;

	dirspi_start(spi);
	tbuf[0] = NORMAL_MSG | BLOCK_DATA | IW7019_DEV_ADDR;
	tbuf[1] = len;
	tbuf[2] = addr & 0x7f;
	memcpy(&tbuf[3], val, len);
	ret = dirspi_write(spi, tbuf, len+3);
	dirspi_stop(spi);

	return ret;
}

static int iw7019_hw_init(struct iw7019 *bl)
{
	u8 addr, val;
	int i, ret;

	gpio_direction_output(bl->en_gpio, 1);
	mdelay(100);
	for (i = 0; i < ARRAY_SIZE(iw7019_ini_data); i++) {
		addr = iw7019_ini_data[i][0];
		val = iw7019_ini_data[i][1];
		ret = iw7019_wreg(bl->spi, addr, val);
		udelay(1);
	}
	return ret;
}

static void iw7019_smr(struct spi_device *spi)
{
	int i;
	u8 val[12];
	int br0, br1;

	for (i = 0; i < 4; i++) {
		if (test_mode) {
			br0 = test_brightness[i*2+0];
			br1 = test_brightness[i*2+1];
		} else {
			br0 = get_luma_hist(i*2+0);
			br1 = get_luma_hist(i*2+1);
		}
		val[i*3+0] = (br0>>4)&0xff; /* br0[11~4] */
		val[i*3+1] = ((br0&0xf)<<4)|((br1>>8)&0xf);
		/* br0[3~0]|br1[11~8] */
		val[i*3+2] = br1&0xff; /* br1[7~0] */
	}
	iw7019_wregs(spi, IW7019_REG_BRIGHTNESS,
	val, ARRAY_SIZE(val));
}

void iw7019_smr_spi(unsigned long data)
{
	iw7019_smr(iw7019_bl->spi);
	/*LDIMPR("hello amlogic!, data[%d]\n", (int)data);*/
}

static irqreturn_t iw7019_vsync_isr(int irq, void *dev_id)
{
	ulong flags;
	/*struct iw7019 *bl = (struct iw7019 *)dev_id;
		iw7019_smr(bl->spi);*/

	spin_lock_irqsave(&vsync_isr_lock, flags);
	tasklet_schedule(&ldim_tasklet);
	spin_unlock_irqrestore(&vsync_isr_lock, flags);

	return IRQ_HANDLED;
}


static ssize_t iw7019_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	struct iw7019 *bl = container_of(class, struct iw7019, cls);
	int ret = 0;
	u8 val = 0;

	if (!strcmp(attr->attr.name, "cur_addr"))
		ret = sprintf(buf, "0x%02x\n", bl->cur_addr);
	else if (!strcmp(attr->attr.name, "mode"))
		ret = sprintf(buf, "0x%02x\n", bl->spi->mode);
	else if (!strcmp(attr->attr.name, "speed"))
		ret = sprintf(buf, "%d\n", bl->spi->max_speed_hz);
	else if (!strcmp(attr->attr.name, "reg")) {
		iw7019_rreg(bl->spi, bl->cur_addr, &val);
		ret = sprintf(buf, "0x%02x\n", val);
	} else if (!strcmp(attr->attr.name, "test"))
		ret = sprintf(buf, "test mode=%d\n", test_mode);
	else if (!strcmp(attr->attr.name, "brightness")) {
		ret = sprintf(buf, "%d,%d,%d,%d,%d,%d,%d,%d\n",
				test_brightness[0],
				test_brightness[1],
				test_brightness[2],
				test_brightness[3],
				test_brightness[4],
				test_brightness[5],
				test_brightness[6],
				test_brightness[7]);
	}
	return ret;
}

#define MAX_ARG_NUM 3
static ssize_t iw7019_store(struct class *class,
struct class_attribute *attr, const char *buf, size_t count)
{
	struct iw7019 *bl = container_of(class, struct iw7019, cls);
	unsigned int val, val2;
	int i;

	if (!strcmp(attr->attr.name, "init"))
		iw7019_hw_init(bl);
	else if (!strcmp(attr->attr.name, "cur_addr")) {
		val = kstrtol(buf, 16, NULL);
		bl->cur_addr = val;
	} else if (!strcmp(attr->attr.name, "mode")) {
		val = kstrtol(buf, 16, NULL);
		bl->spi->mode = val;
	} else if (!strcmp(attr->attr.name, "speed")) {
		val = kstrtol(buf, 16, NULL);
		bl->spi->max_speed_hz = val*1000;
	} else if (!strcmp(attr->attr.name, "reg")) {
		val = kstrtol(buf, 16, NULL);
		iw7019_wreg(bl->spi, bl->cur_addr, val);
		bl->cur_addr++;
	} else if (!strcmp(attr->attr.name, "test")) {
		i = sscanf(buf, "%d", &val);
		/*printk("i=%d, set test mode to %d\n", i, val);*/
		LDIMPR("i=%d, set test mode to %d\n", i, val);
		test_mode = val;
	} else if (!strcmp(attr->attr.name, "brightness")) {
		i = sscanf(buf, "%d%d", &val, &val2);
		val &= 0xfff;
		/*printk("i=%d, brightness=%d, index=%d\n", i, val, val2);*/
		LDIMPR("i=%d, brightness=%d, index=%d\n", i, val, val2);
		if ((i == 2) && (val2 < ARRAY_SIZE(test_brightness)))
			test_brightness[val2] = val;
	} else
		/*printk(KERN_WARNING "argment error!\n");*/
		LDIMPR("LDIM argment error!\n");
	return count;
}

static struct class_attribute iw7019_class_attrs[] = {
	__ATTR(init, S_IRUGO | S_IWUSR, iw7019_show, iw7019_store),
	__ATTR(cur_addr, S_IRUGO | S_IWUSR, iw7019_show, iw7019_store),
	__ATTR(mode, S_IRUGO | S_IWUSR, iw7019_show, iw7019_store),
	__ATTR(speed, S_IRUGO | S_IWUSR, iw7019_show, iw7019_store),
	__ATTR(reg, S_IRUGO | S_IWUSR, iw7019_show, iw7019_store),
	__ATTR(test, S_IRUGO | S_IWUSR, iw7019_show, iw7019_store),
	__ATTR(brightness, S_IRUGO | S_IWUSR, iw7019_show, iw7019_store),
	__ATTR_NULL
};

static int iw7019_probe(struct spi_device *spi)
{
	struct device_node *np = spi->dev.of_node;
	int ret;
	const char *str;

	ret = of_property_read_string(np, "status", &str);
	if (ret) {
		LDIMPR("get status failed\n");
		return 0;
	} else {
		if (strncmp(str, "disabled", 2) == 0)
			return 0;
	}
	iw7019_bl = kzalloc(sizeof(struct iw7019), GFP_KERNEL);
	if (iw7019_bl == NULL) {
			pr_err("%s: malloc iw7019_bl failed\n", __func__);
			return -ENODEV;
		}

	iw7019_bl->spi = spi;
	iw7019_bl->en_gpio = of_get_named_gpio(np, "en-gpio", 0);
	pr_err("en_gpio = %d\n", iw7019_bl->en_gpio);
	if (iw7019_bl->en_gpio)
		gpio_request(iw7019_bl->en_gpio, "iw7019_en");
	spin_lock_init(&vsync_isr_lock);

	tasklet_init(&ldim_tasklet, iw7019_smr_spi, iw7019_bl->en_gpio);
	iw7019_bl->irq = INT_VIU_VSYNC;
	dev_set_drvdata(&spi->dev, iw7019_bl);
	spi->bits_per_word = 8;
	spi->mode = SPI_MODE_0;
	ret = spi_setup(spi);

	/* init panel parameters */
	if (iw7019_hw_init(iw7019_bl) < 0) {
		pr_err("%s: hw init failed\n", __func__);
		return -ENODEV;
	}

	ret = request_irq(iw7019_bl->irq, &iw7019_vsync_isr,
				IRQF_SHARED, "iw7019", (void *)iw7019_bl);
	if (ret) {
		pr_err("%s: request_irq error!\n", __func__);
		return ret;
	}

	iw7019_bl->cls.name = kzalloc(10, GFP_KERNEL);
	sprintf((char *)iw7019_bl->cls.name, "iw7019");
	iw7019_bl->cls.class_attrs = iw7019_class_attrs;
	ret = class_register(&iw7019_bl->cls);
	if (ret < 0)
		pr_err("register class failed! (%d)\n", ret);

	pr_info("%s probe ok!\n", __func__);

	return ret;
}

static int iw7019_remove(struct spi_device *spi)
{
	/*tasklet_kill(&ldim_tasklet);
		free_irq(INT_VIU_VSYNC, iw7019_bl->en_gpio);*/
	/* @todo */
	return 0;
}

static int iw7019_suspend(struct spi_device *spi, pm_message_t mesg)
{
	/* @todo iw7019_suspend */
	return 0;
}

static int iw7019_resume(struct spi_device *spi)
{
	/* @todo iw7019_resume */
	return 0;
}

static const struct of_device_id iw7019_of_match[] = {
	{	.compatible = "amlogic, iw7019", },
};
MODULE_DEVICE_TABLE(of, iw7019_of_match);

static struct spi_driver iw7019_driver = {
	.probe = iw7019_probe,
	.remove = iw7019_remove,
	.suspend	= iw7019_suspend,
	.resume		= iw7019_resume,
	.driver = {
		.name = "iw7019",
		.owner = THIS_MODULE,
		.bus	= &spi_bus_type,
		.of_match_table = iw7019_of_match,
	},
};

static int __init iw7019_init(void)
{
	/*printk("%s start\n", __func__);*/
	LDIMPR("%s start\n", __func__);
	return spi_register_driver(&iw7019_driver);
}

static void __exit iw7019_exit(void)
{
	spi_unregister_driver(&iw7019_driver);
}

module_init(iw7019_init);
module_exit(iw7019_exit);


MODULE_DESCRIPTION("iW7019 LED Driver for LCD Panel Backlight");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Bobby Yang <bo.yang@amlogic.com>");

