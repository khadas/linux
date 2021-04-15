// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/io.h>
#include <linux/usb/phy_companion.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/pm_runtime.h>
#include <linux/delay.h>
#include <linux/usb/phy.h>
#include <linux/amlogic/power_ctrl.h>
#include <linux/amlogic/usb-v2.h>
#include <linux/amlogic/usbtype.h>

static int amlogic_usb2_m31_init(struct usb_phy *x)
{
	/* to do */

	return 0;
}

static int amlogic_usb2_m31_suspend(struct usb_phy *x, int suspend)
{
	return 0;
}

static void amlogic_usb2_m31_shutdown(struct usb_phy *x)
{
	/* to do */
}

static int amlogic_usb2_m31_probe(struct platform_device *pdev)
{
	struct amlogic_usb_m31			*phy;
	struct device *dev = &pdev->dev;

	phy = devm_kzalloc(&pdev->dev, sizeof(*phy), GFP_KERNEL);
	if (!phy)
		return -ENOMEM;

	phy->dev		= dev;
	phy->phy.dev		= phy->dev;
	phy->phy.label		= "amlogic-usb2-m31-phy";
	phy->phy.init		= amlogic_usb2_m31_init;
	phy->phy.set_suspend	= amlogic_usb2_m31_suspend;
	phy->phy.shutdown	= amlogic_usb2_m31_shutdown;
	phy->phy.type		= USB_PHY_TYPE_USB2;

	usb_add_phy_dev(&phy->phy);

	platform_set_drvdata(pdev, phy);

	pm_runtime_enable(phy->dev);

	return 0;
}

static int amlogic_usb2_m31_remove(struct platform_device *pdev)
{
	return 0;
}

#ifdef CONFIG_PM_RUNTIME

static int amlogic_usb2_m31_runtime_suspend(struct device *dev)
{
	return 0;
}

static int amlogic_usb2_m31_runtime_resume(struct device *dev)
{
	unsigned int ret = 0;

	return ret;
}

static const struct dev_pm_ops amlogic_usb2_m31_pm_ops = {
	SET_RUNTIME_PM_OPS(amlogic_usb2_m31_runtime_suspend,
		amlogic_usb2_m31_runtime_resume,
		NULL)
};

#define DEV_PM_OPS     (&amlogic_usb2_m31_pm_ops)
#else
#define DEV_PM_OPS     NULL
#endif

#ifdef CONFIG_OF
static const struct of_device_id amlogic_usb2_m31_id_table[] = {
	{ .compatible = "amlogic, amlogic-usb2-m31-phy" },
	{}
};
MODULE_DEVICE_TABLE(of, amlogic_usb2_m31_id_table);
#endif

static struct platform_driver amlogic_usb2_m31_driver = {
	.probe		= amlogic_usb2_m31_probe,
	.remove		= amlogic_usb2_m31_remove,
	.driver		= {
		.name	= "amlogic-usb2-m31-phy",
		.owner	= THIS_MODULE,
		.pm	= DEV_PM_OPS,
		.of_match_table = of_match_ptr(amlogic_usb2_m31_id_table),
	},
};

module_platform_driver(amlogic_usb2_m31_driver);

MODULE_ALIAS("platform: amlogic_usb2_m31");
MODULE_AUTHOR("Amlogic Inc.");
MODULE_DESCRIPTION("amlogic USB2 m31 phy driver");
MODULE_LICENSE("GPL v2");
