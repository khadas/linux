/*
 * drivers/amlogic/usb/phy/phy-aml-new-otg.c
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
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/irqreturn.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/pm_runtime.h>
#include <linux/delay.h>
#include <linux/usb/phy.h>
#include <linux/amlogic/usb-v2.h>
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/workqueue.h>
#include <linux/notifier.h>
#include <linux/amlogic/usbtype.h>
#include "phy-aml-new-usb-v2.h"

#define HOST_MODE	0
#define DEVICE_MODE	1

struct usb_aml_regs_v2 usb_otg_aml_regs;
struct amlogic_otg	*g_otg;

struct amlogic_otg {
	struct device           *dev;
	void __iomem    *phy3_cfg;
	void __iomem    *phy3_cfg_r1;
	void __iomem    *phy3_cfg_r2;
	void __iomem    *phy3_cfg_r4;
	void __iomem    *phy3_cfg_r5;
	void __iomem    *usb2_phy_cfg;
	/* Set VBus Power though GPIO */
	int vbus_power_pin;
	int vbus_power_pin_work_mask;
	struct delayed_work     work;
	struct gpio_desc *usb_gpio_desc;
};

static void set_mode(unsigned long reg_addr, int mode);
BLOCKING_NOTIFIER_HEAD(aml_new_otg_notifier_list);

int aml_new_otg_register_notifier(struct notifier_block *nb)
{
	int ret;

	ret = blocking_notifier_chain_register
			(&aml_new_otg_notifier_list, nb);

	return ret;
}
EXPORT_SYMBOL(aml_new_otg_register_notifier);

int aml_new_otg_unregister_notifier(struct notifier_block *nb)
{
	int ret;

	ret = blocking_notifier_chain_unregister
			(&aml_new_otg_notifier_list, nb);

	return ret;
}
EXPORT_SYMBOL(aml_new_otg_unregister_notifier);

static void aml_new_usb_notifier_call(unsigned long is_device_on)
{
	blocking_notifier_call_chain
			(&aml_new_otg_notifier_list, is_device_on, NULL);
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

static void amlogic_new_set_vbus_power
		(struct amlogic_otg *phy, char is_power_on)
{
	if (phy->vbus_power_pin != -1)
		set_usb_vbus_power(phy->usb_gpio_desc,
			phy->vbus_power_pin, is_power_on);
}



void aml_new_otg_init(void)
{
	union usb_r5_v2 r5 = {.d32 = 0};
	unsigned long reg_addr;

	if (!g_otg)
		return;

	reg_addr = (unsigned long)g_otg->usb2_phy_cfg;

	r5.d32 = readl(usb_otg_aml_regs.usb_r_v2[5]);
	if (r5.b.iddig_curr == 0) {
		amlogic_new_set_vbus_power(g_otg, 1);
		aml_new_usb_notifier_call(0);
		set_mode(reg_addr, HOST_MODE);
	}
}
EXPORT_SYMBOL(aml_new_otg_init);

int aml_new_otg_get_mode(void)
{
	union usb_r5_v2 r5 = {.d32 = 0};

	r5.d32 = readl(usb_otg_aml_regs.usb_r_v2[5]);
	if (r5.b.iddig_curr == 0)
		return 0;
	else
		return 1;
}
EXPORT_SYMBOL(aml_new_otg_get_mode);


static int amlogic_new_otg_init(struct amlogic_otg *phy)
{
	union usb_r1_v2 r1 = {.d32 = 0};
	union usb_r5_v2 r5 = {.d32 = 0};
	int i = 0;

	for (i = 0; i < 6; i++) {
		usb_otg_aml_regs.usb_r_v2[i] = (void __iomem *)
			((unsigned long)phy->usb2_phy_cfg + 0x80 + 4*i);
	}

	r1.d32 = readl(usb_otg_aml_regs.usb_r_v2[1]);
	r1.b.u3h_fladj_30mhz_reg = 0x20;
	writel(r1.d32, usb_otg_aml_regs.usb_r_v2[1]);

	r5.d32 = readl(usb_otg_aml_regs.usb_r_v2[5]);
	r5.b.iddig_en0 = 1;
	r5.b.iddig_en1 = 1;
	r5.b.iddig_th = 255;
	writel(r5.d32, usb_otg_aml_regs.usb_r_v2[5]);

	return 0;
}

static void set_mode(unsigned long reg_addr, int mode)
{
	struct u2p_aml_regs_v2 u2p_aml_regs;
	struct usb_aml_regs_v2 usb_gxl_aml_regs;
	union u2p_r0_v2 reg0;
	union usb_r0_v2 r0 = {.d32 = 0};
	union usb_r4_v2 r4 = {.d32 = 0};

	u2p_aml_regs.u2p_r_v2[0] = (void __iomem	*)
				((unsigned long)reg_addr + PHY_REGISTER_SIZE);

	usb_gxl_aml_regs.usb_r_v2[0] = (void __iomem *)
				((unsigned long)reg_addr + 4*PHY_REGISTER_SIZE
				+ 4*0);
	usb_gxl_aml_regs.usb_r_v2[1] = (void __iomem *)
				((unsigned long)reg_addr + 4*PHY_REGISTER_SIZE
				+ 4*1);
	usb_gxl_aml_regs.usb_r_v2[4] = (void __iomem *)
				((unsigned long)reg_addr + 4*PHY_REGISTER_SIZE
				+ 4*4);

	r0.d32 = readl(usb_gxl_aml_regs.usb_r_v2[0]);
	if (mode == DEVICE_MODE) {
		r0.b.u2d_act = 1;
		r0.b.u2d_ss_scaledown_mode = 0;
	} else
		r0.b.u2d_act = 0;
	writel(r0.d32, usb_gxl_aml_regs.usb_r_v2[0]);

	r4.d32 = readl(usb_gxl_aml_regs.usb_r_v2[4]);
	if (mode == DEVICE_MODE)
		r4.b.p21_SLEEPM0 = 0x1;
	else
		r4.b.p21_SLEEPM0 = 0x0;
	writel(r4.d32, usb_gxl_aml_regs.usb_r_v2[4]);

	reg0.d32 = readl(u2p_aml_regs.u2p_r_v2[0]);
	if (mode == DEVICE_MODE) {
		reg0.b.host_device = 0;
		reg0.b.POR = 0;
	} else {
		reg0.b.host_device = 1;
		reg0.b.POR = 0;
	}
	writel(reg0.d32, u2p_aml_regs.u2p_r_v2[0]);

	udelay(500);
}

static void amlogic_gxl_work(struct work_struct *work)
{
	struct amlogic_otg *phy =
		container_of(work, struct amlogic_otg, work.work);
	union usb_r5_v2 r5 = {.d32 = 0};
	unsigned long reg_addr = ((unsigned long)phy->usb2_phy_cfg);

	r5.d32 = readl(usb_otg_aml_regs.usb_r_v2[5]);
	if (r5.b.iddig_curr == 0) {
		amlogic_new_set_vbus_power(phy, 1);
		aml_new_usb_notifier_call(0);
		set_mode(reg_addr, HOST_MODE);
	} else {
		set_mode(reg_addr, DEVICE_MODE);
		aml_new_usb_notifier_call(1);
		amlogic_new_set_vbus_power(phy, 0);
	}
	r5.b.usb_iddig_irq = 0;
	writel(r5.d32, usb_otg_aml_regs.usb_r_v2[5]);
}

static irqreturn_t amlogic_botg_detect_irq(int irq, void *dev)
{
	struct amlogic_otg *phy = (struct amlogic_otg *)dev;
	union usb_r5_v2 r5 = {.d32 = 0};

	r5.d32 = readl(usb_otg_aml_regs.usb_r_v2[5]);
	r5.b.usb_iddig_irq = 0;
	writel(r5.d32, usb_otg_aml_regs.usb_r_v2[5]);

	schedule_delayed_work(&phy->work, msecs_to_jiffies(10));

	return IRQ_HANDLED;
}

static int amlogic_new_otg_probe(struct platform_device *pdev)
{
	struct amlogic_otg			*phy;
	struct device *dev = &pdev->dev;
	void __iomem *usb2_phy_base;
	unsigned int usb2_phy_mem;
	unsigned int usb2_phy_mem_size = 0;
	const char *gpio_name = NULL;
	struct gpio_desc *usb_gd = NULL;
	const void *prop;
	int irq;
	int retval;
	int gpio_vbus_power_pin = -1;
	int otg = 0;

	gpio_name = of_get_property(dev->of_node, "gpio-vbus-power", NULL);
	if (gpio_name) {
		gpio_vbus_power_pin = 1;
		usb_gd = gpiod_get_index(&pdev->dev,
				 NULL, 0, GPIOD_OUT_LOW);
		if (IS_ERR(usb_gd))
			return -1;
	}

	prop = of_get_property(dev->of_node, "otg", NULL);
	if (prop)
		otg = of_read_ulong(prop, 1);

	retval = of_property_read_u32
				(dev->of_node, "usb2-phy-reg", &usb2_phy_mem);
	if (retval < 0)
		return -EINVAL;

	retval = of_property_read_u32
		(dev->of_node, "usb2-phy-reg-size", &usb2_phy_mem_size);
	if (retval < 0)
		return -EINVAL;

	usb2_phy_base = devm_ioremap_nocache
				(&(pdev->dev), (resource_size_t)usb2_phy_mem,
				(unsigned long)usb2_phy_mem_size);
	if (!usb2_phy_base)
		return -ENOMEM;

	phy = devm_kzalloc(&pdev->dev, sizeof(*phy), GFP_KERNEL);
	if (!phy)
		return -ENOMEM;

	if (otg) {
		irq = platform_get_irq(pdev, 0);
		if (irq < 0)
			return -ENODEV;
		retval = request_irq(irq, amlogic_botg_detect_irq,
				IRQF_SHARED | IRQ_LEVEL,
				"amlogic_botg_detect", phy);

		if (retval) {
			dev_err(&pdev->dev, "request of irq%d failed\n", irq);
			retval = -EBUSY;
			return retval;
		}
	}

	dev_info(&pdev->dev, "phy_mem:0x%lx, iomap phy_base:0x%lx\n",
			(unsigned long)usb2_phy_mem,
			(unsigned long)usb2_phy_base);

	phy->dev		= dev;
	phy->usb2_phy_cfg	= usb2_phy_base;
	phy->vbus_power_pin = gpio_vbus_power_pin;
	phy->usb_gpio_desc = usb_gd;

	INIT_DELAYED_WORK(&phy->work, amlogic_gxl_work);

	platform_set_drvdata(pdev, phy);

	pm_runtime_enable(phy->dev);
	g_otg = phy;

	amlogic_new_otg_init(phy);

	return 0;
}

static int amlogic_new_otg_remove(struct platform_device *pdev)
{
	return 0;
}

#ifdef CONFIG_PM_RUNTIME

static int amlogic_new_otg_runtime_suspend(struct device *dev)
{
	return 0;
}

static int amlogic_new_otg_runtime_resume(struct device *dev)
{
	u32 ret = 0;

	return ret;
}

static const struct dev_pm_ops amlogic_new_otg_pm_ops = {
	SET_RUNTIME_PM_OPS(amlogic_new_otg_runtime_suspend,
		amlogic_new_otg_runtime_resume,
		NULL)
};

#define DEV_PM_OPS     (&amlogic_new_otg_pm_ops)
#else
#define DEV_PM_OPS     NULL
#endif

#ifdef CONFIG_OF
static const struct of_device_id amlogic_new_otg_id_table[] = {
	{ .compatible = "amlogic, amlogic-new-otg" },
	{}
};
MODULE_DEVICE_TABLE(of, amlogic_new_otg_id_table);
#endif

static struct platform_driver amlogic_new_otg_driver = {
	.probe		= amlogic_new_otg_probe,
	.remove		= amlogic_new_otg_remove,
	.driver		= {
		.name	= "amlogic-new-otg",
		.owner	= THIS_MODULE,
		.pm	= DEV_PM_OPS,
		.of_match_table = of_match_ptr(amlogic_new_otg_id_table),
	},
};

module_platform_driver(amlogic_new_otg_driver);

MODULE_ALIAS("platform: amlogic_usb3_v2");
MODULE_AUTHOR("Amlogic Inc.");
MODULE_DESCRIPTION("amlogic USB3 v2 phy driver");
MODULE_LICENSE("GPL v2");
