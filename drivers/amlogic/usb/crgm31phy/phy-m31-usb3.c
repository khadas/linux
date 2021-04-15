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

	if (phy->phy.flags == AML_USB3_PHY_ENABLE)
		clk_disable_unprepare(phy->clk);

	phy->suspend_flag = 1;
}

static int amlogic_usb3_m31_init(struct usb_phy *x)
{
	struct amlogic_usb_m31 *phy = phy_to_m31usb(x);

	if (phy->suspend_flag) {
		if (phy->phy.flags == AML_USB3_PHY_ENABLE)
			clk_prepare_enable(phy->clk);
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
	unsigned int phy3_mem;
	unsigned int phy3_mem_size = 0;
	union phy_m31_r0 r0;

	prop = of_get_property(dev->of_node, "portnum", NULL);
	if (prop)
		portnum = of_read_ulong(prop, 1);

	if (!portnum)
		dev_err(&pdev->dev, "This phy has no usb port\n");

	tsi_pci = of_find_node_by_type(NULL, "pci");
	if (tsi_pci) {
		if (device_is_available(tsi_pci)) {
			dev_info(&pdev->dev,
				"pci-e driver probe, disable USB 3.0 function!!!\n");
			portnum = 0;
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

	/* set the phy from pcie to usb3 */
	if (phy->portnum > 0) {
		r0.d32 = readl(phy->phy3_cfg);
		r0.b.PHY_SEL = 0;
		r0.b.U3_HOST_PHY = 1;
		r0.b.PCIE_CLKSEL = 0;
		r0.b.U3_SSRX_SEL = 0;
		r0.b.U3_SSTX_SEL = 0;
		r0.b.REFPAD_EXT_100M_EN = 0;
		r0.b.TX_ENABLE_N = 1;
		r0.b.TX_SE0 = 0;
		r0.b.FSLSSERIALMODE = 0;
		writel(r0.d32, phy->phy3_cfg);
		usleep_range(90, 100);

		phy->clk = devm_clk_get(dev, "pcie_refpll");
		if (IS_ERR(phy->clk)) {
			dev_err(dev, "Failed to get usb3 bus clock\n");
			ret = PTR_ERR(phy->clk);
			return ret;
		}

		ret = clk_prepare_enable(phy->clk);
		if (ret) {
			dev_err(dev, "Failed to enable usb3 bus clock\n");
			ret = PTR_ERR(phy->clk);
			return ret;
		}
		phy->phy.flags = AML_USB3_PHY_ENABLE;
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
