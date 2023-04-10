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
#include <linux/delay.h>
#include <linux/usb/phy.h>
#include <linux/amlogic/usb-v2.h>
#include <linux/of_gpio.h>
#include <linux/workqueue.h>
#include <linux/notifier.h>
#include <linux/amlogic/usbtype.h>
#include <linux/amlogic/power_ctrl.h>
#include "../phy/phy-aml-new-usb-v2.h"

#define	phy_to_m31usb(x)	container_of((x), struct amlogic_usb_m31, phy)

static int amlogic_usb3_m31_suspend(struct usb_phy *x, int suspend)
{
	return 0;
}

static void amlogic_usb3_m31_shutdown(struct usb_phy *x)
{
	struct amlogic_usb_m31 *phy = phy_to_m31usb(x);
	u32 val, temp, shift = 0;
	size_t mask = 0;

	if (phy->portnum > 0) {
		mask = (size_t)phy->reset_regs & 0xf;
		shift = (phy->m31phy_reset_level_bit / 32) * 4;
		temp = 1 << (phy->m31phy_reset_level_bit % 32);
		val = readl((void __iomem		*)
			((unsigned long)phy->reset_regs +
			(phy->reset_level - mask) + shift));
		writel((val & (~temp)), (void __iomem	*)
			((unsigned long)phy->reset_regs +
			(phy->reset_level - mask) + shift));

		temp = 1 << (phy->m31ctl_reset_level_bit % 32);
		shift = (phy->m31ctl_reset_level_bit / 32) * 4;
		val = readl((void __iomem		*)
			((unsigned long)phy->reset_regs +
			(phy->reset_level - mask) + shift));
		writel((val & (~temp)), (void __iomem	*)
			((unsigned long)phy->reset_regs +
			(phy->reset_level - mask) + shift));
	}

	phy->suspend_flag = 1;
}

static int amlogic_usb3_m31_init(struct usb_phy *x)
{
	struct amlogic_usb_m31 *phy = phy_to_m31usb(x);
	u32 val, temp, shift = 0;
	size_t mask = 0;

	if (phy->portnum > 0) {
		mask = (size_t)phy->reset_regs & 0xf;
		temp = 1 << (phy->m31phy_reset_level_bit % 32);
		shift = (phy->m31phy_reset_level_bit / 32) * 4;
		val = readl((void __iomem		*)
			((unsigned long)phy->reset_regs +
			(phy->reset_level - mask) + shift));
		writel((val & (~temp)), (void __iomem	*)
			((unsigned long)phy->reset_regs +
			(phy->reset_level - mask) + shift));
		udelay(10);
		writel((val | (temp)), (void __iomem	*)
			((unsigned long)phy->reset_regs +
			(phy->reset_level - mask) + shift));

		temp = 1 << (phy->m31ctl_reset_level_bit % 32);
		shift = (phy->m31ctl_reset_level_bit / 32) * 4;
		val = readl((void __iomem		*)
			((unsigned long)phy->reset_regs +
			(phy->reset_level - mask) + shift));
		writel((val & (~temp)), (void __iomem	*)
			((unsigned long)phy->reset_regs +
			(phy->reset_level - mask) + shift));
		udelay(10);
		writel((val | (temp)), (void __iomem	*)
			((unsigned long)phy->reset_regs +
			(phy->reset_level - mask) + shift));
		dev_info(phy->dev, "m31phy reset\n");
	}

	if (phy->suspend_flag) {
		phy->suspend_flag = 0;
		return 0;
	}

	return 0;
}

static bool device_is_available(const struct device_node *device)
{
	const char *status;
	int statlen;

	if (!device)
		return false;

	status = of_get_property(device, "status", &statlen);
	if (!status)
		return true;

	if (statlen > 0) {
		if (!strcmp(status, "okay") || !strcmp(status, "ok"))
			return true;
	}

	return false;
}

static int amlogic_usb3_m31_probe(struct platform_device *pdev)
{
	struct amlogic_usb_m31			*phy;
	struct device *dev = &pdev->dev;
	const void *prop;
	int portnum = 0;
	int ret;
	struct device_node *tsi_pci;
	void __iomem *phy3_base;
	void __iomem	*reset_base = NULL;
	unsigned int phy3_mem;
	unsigned int phy3_mem_size = 0;
	union phy_m31_r0 r0;
	struct resource *reset_mem;
	u32 reset_level = 0x84;
	u32 m31phy_reset_level_bit;
	u32 m31ctl_reset_level_bit;
	u32 m31_utmi_reset_level_bit;
	u32 u3_combx0_reset_bit;
	u32 val;
	int uncomposite;
	u32 shift;
	u32 mask;
	u32 temp;
	int m31_utmi_reset_level_flag = 0;
	int u3_combx0_reset_flag = 0;
	int m31phy_reset_level_bit_flag = 0;

	prop = of_get_property(dev->of_node, "portnum", NULL);
	if (prop)
		portnum = of_read_ulong(prop, 1);

	if (!portnum)
		dev_err(&pdev->dev, "This phy has no usb port\n");

	prop = of_get_property(dev->of_node, "uncomposite", NULL);
	if (prop)
		uncomposite = of_read_ulong(prop, 1);
	else
		uncomposite = 0;

	if (uncomposite == 0) {
		tsi_pci = of_find_node_by_type(NULL, "pci");
		if (tsi_pci) {
			if (device_is_available(tsi_pci)) {
				dev_info(&pdev->dev,
					"pci-e driver probe, disable USB 3.0 function!!!\n");
				portnum = 0;
			}
		}
	}

	ret = of_property_read_u32(dev->of_node, "phy-reg", &phy3_mem);
	if (ret < 0)
		return -EINVAL;

	ret = of_property_read_u32
				(dev->of_node, "phy-reg-size", &phy3_mem_size);
	if (ret < 0)
		return -EINVAL;

	phy3_base = devm_ioremap_nocache
				(&pdev->dev, (resource_size_t)phy3_mem,
				(unsigned long)phy3_mem_size);
	if (!phy3_base)
		return -ENOMEM;

	phy = devm_kzalloc(&pdev->dev, sizeof(*phy), GFP_KERNEL);
	if (!phy)
		return -ENOMEM;

	reset_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (reset_mem) {
		reset_base = ioremap(reset_mem->start,
			resource_size(reset_mem));
		if (IS_ERR(reset_base))
			return PTR_ERR(reset_base);
	}

	prop = of_get_property(dev->of_node, "reset-level", NULL);
	if (prop)
		reset_level = of_read_ulong(prop, 1);
	else
		reset_level = 0x84;
	prop = of_get_property(dev->of_node, "m31phy-reset-level-bit", NULL);
	if (prop) {
		m31phy_reset_level_bit = of_read_ulong(prop, 1);
		m31phy_reset_level_bit_flag = 1;
	} else {
		m31phy_reset_level_bit = 13;
	}

	prop = of_get_property(dev->of_node, "m31ctl-reset-level-bit", NULL);
	if (prop)
		m31ctl_reset_level_bit = of_read_ulong(prop, 1);
	else
		m31ctl_reset_level_bit = 6;

	prop = of_get_property(dev->of_node, "m31-utmi-reset-level-bit", NULL);
	if (prop) {
		m31_utmi_reset_level_bit = of_read_ulong(prop, 1);
		m31_utmi_reset_level_flag = 1;
	} else {
		m31_utmi_reset_level_bit = 0;
	}

	prop = of_get_property(dev->of_node, "u3-combx0-reset-bit", NULL);
	if (prop) {
		u3_combx0_reset_bit = of_read_ulong(prop, 1);
		u3_combx0_reset_flag = 1;
	} else {
		u3_combx0_reset_bit = 0;
	}

	phy->dev		= dev;
	phy->portnum      = portnum;
	phy->suspend_flag = 0;
	phy->phy.dev		= phy->dev;
	phy->phy3_cfg = phy3_base;
	phy->phy.label		= "amlogic-usbm31phy3";
	phy->phy.init		= amlogic_usb3_m31_init;
	phy->phy.set_suspend	= amlogic_usb3_m31_suspend;
	phy->phy.shutdown	= amlogic_usb3_m31_shutdown;
	phy->phy.type		= USB_PHY_TYPE_USB3;
	phy->phy.flags		= AML_USB3_PHY_DISABLE;
	phy->reset_regs = reset_base;
	phy->reset_level = reset_level;
	phy->m31phy_reset_level_bit = m31phy_reset_level_bit;
	phy->m31ctl_reset_level_bit = m31ctl_reset_level_bit;

	if (m31_utmi_reset_level_flag == 1 && u3_combx0_reset_flag == 1) {
		dev_info(&pdev->dev, "reset m31 utmi combx0!!!!!!\n");
		mask = (size_t)phy->reset_regs & 0xf;
		temp = 1 << (m31ctl_reset_level_bit % 32);
		shift = (m31ctl_reset_level_bit / 32) * 4;
		val = readl((void __iomem		*)
			((unsigned long)phy->reset_regs +
			(phy->reset_level - mask) + shift));
		writel((val & (~temp)), (void __iomem	*)
			((unsigned long)phy->reset_regs +
			(phy->reset_level - mask) + shift));
		usleep_range(90, 100);
		writel((val | (temp)), (void __iomem	*)
			((unsigned long)phy->reset_regs +
			(phy->reset_level - mask) + shift));
		usleep_range(90, 100);

		temp = 1 << (m31_utmi_reset_level_bit % 32);
		shift = (m31_utmi_reset_level_bit / 32) * 4;
		val = readl((void __iomem		*)
			((unsigned long)phy->reset_regs +
			(phy->reset_level - mask) + shift));
		writel((val & (~temp)), (void __iomem	*)
			((unsigned long)phy->reset_regs +
			(phy->reset_level - mask) + shift));
		usleep_range(90, 100);
		writel((val | (temp)), (void __iomem	*)
			((unsigned long)phy->reset_regs +
			(phy->reset_level - mask) + shift));

		usleep_range(90, 100);

		temp = 1 << (u3_combx0_reset_bit % 32);
		shift = (u3_combx0_reset_bit / 32) * 4;
		val = readl((void __iomem		*)
			((unsigned long)phy->reset_regs +
			(phy->reset_level - mask) + shift));
		writel((val & (~temp)), (void __iomem	*)
			((unsigned long)phy->reset_regs +
			(phy->reset_level - mask) + shift));
		usleep_range(90, 100);
		writel((val | (temp)), (void __iomem	*)
			((unsigned long)phy->reset_regs +
			(phy->reset_level - mask) + shift));
		usleep_range(90, 100);
	}

	if (m31phy_reset_level_bit_flag == 1) {
		dev_info(&pdev->dev, "reset m31phy_reset_level_bit!!!!!!\n");
		mask = (size_t)phy->reset_regs & 0xf;
		temp = 1 << (m31phy_reset_level_bit % 32);
		shift = (m31phy_reset_level_bit / 32) * 4;
		val = readl((void __iomem		*)
			((unsigned long)phy->reset_regs +
			(phy->reset_level - mask) + shift));
		writel((val & (~temp)), (void __iomem	*)
			((unsigned long)phy->reset_regs +
			(phy->reset_level - mask) + shift));
		usleep_range(90, 100);
		writel((val | (temp)), (void __iomem	*)
			((unsigned long)phy->reset_regs +
			(phy->reset_level - mask) + shift));
		usleep_range(90, 100);
	}

	/* set the phy from pcie to usb3 */
	if (phy->portnum > 0) {
		writel(0x1, phy->phy3_cfg + 0x8);
		usleep_range(90, 100);

		writel(0, phy->phy3_cfg + 0xc);
		usleep_range(90, 100);

		if (uncomposite) {
			val = readl(phy->phy3_cfg + 0x82c);
			val |= (1 << 5);
			writel(val, phy->phy3_cfg + 0x82c);
			usleep_range(90, 100);

			val = readl(phy->phy3_cfg + 0x848);
			val |= (3 << 0);
			writel(val, phy->phy3_cfg + 0x848);
			usleep_range(90, 100);
		}

		r0.d32 = readl(phy->phy3_cfg);
		r0.b.PHY_SEL = 0;
		r0.b.U3_HOST_PHY = 1;
		r0.b.PCIE_CLKSEL = 0;
		r0.b.U3_SSRX_SEL = 1;
		r0.b.U3_SSTX_SEL = 1;
		r0.b.REFPAD_EXT_100M_EN = 0;
		r0.b.TX_ENABLE_N = 1;
		r0.b.TX_SE0 = 0;
		r0.b.FSLSSERIALMODE = 0;

		writel(r0.d32, phy->phy3_cfg);
		usleep_range(90, 100);

		phy->phy.flags = AML_USB3_PHY_ENABLE;
	} else {
		if (uncomposite) {
			r0.d32 = readl(phy->phy3_cfg);
			r0.b.TX_ENABLE_N = 1;
			r0.b.PHY_SEL = 1;
			r0.b.FSLSSERIALMODE = 0;
			r0.b.PHY_SSCG_ON = 0;
			writel(r0.d32, phy->phy3_cfg);
			usleep_range(90, 100);
		}
	}

	usb_add_phy_dev(&phy->phy);

	platform_set_drvdata(pdev, phy);

	pm_runtime_enable(phy->dev);

	return 0;
}

static int amlogic_usb3_m31_remove(struct platform_device *pdev)
{
	return 0;
}

#ifdef CONFIG_PM_RUNTIME
static int amlogic_usb3_m31_runtime_suspend(struct device *dev)
{
	return 0;
}

static int amlogic_usb3_m31_runtime_resume(struct device *dev)
{
	u32 ret = 0;

	return ret;
}

static const struct dev_pm_ops amlogic_usb3_m31_pm_ops = {
	SET_RUNTIME_PM_OPS(amlogic_usb3_m31_runtime_suspend,
		amlogic_usb3_m31_runtime_resume,
		NULL)
};

#define DEV_PM_OPS     (&amlogic_usb3_m31_pm_ops)
#else
#define DEV_PM_OPS     NULL
#endif

#ifdef CONFIG_OF
static const struct of_device_id amlogic_usb3_m31_id_table[] = {
	{ .compatible = "amlogic, amlogic-usb3-m31-phy" },
	{}
};
MODULE_DEVICE_TABLE(of, amlogic_usb3_m31_id_table);
#endif

static struct platform_driver amlogic_usb3_m31_driver = {
	.probe		= amlogic_usb3_m31_probe,
	.remove		= amlogic_usb3_m31_remove,
	.driver		= {
		.name	= "amlogic-usb3-m31-phy",
		.owner	= THIS_MODULE,
		.pm	= DEV_PM_OPS,
		.of_match_table = of_match_ptr(amlogic_usb3_m31_id_table),
	},
};

module_platform_driver(amlogic_usb3_m31_driver);

MODULE_ALIAS("platform: amlogic_usb3_m31");
MODULE_AUTHOR("Amlogic Inc.");
MODULE_DESCRIPTION("amlogic USB3 m31 phy driver");
MODULE_LICENSE("GPL v2");
