/*
 * phy-aml-gxl-usb3.c
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
#include <linux/amlogic/usb-gxl.h>
#include "phy-aml-new-usb.h"

#define HOST_MODE	0
#define DEVICE_MODE	1

struct usb_aml_regs_t usb_new_aml_regs;

static int amlogic_new_usb3_suspend(struct usb_phy *x, int suspend)
{
	return 0;
}

static int amlogic_new_usb3_init(struct usb_phy *x)
{
	return 0;
}

static int amlogic_new_usb3_probe(struct platform_device *pdev)
{
	struct amlogic_usb			*phy;
	struct device *dev = &pdev->dev;
	struct resource *phy_mem;
	void __iomem	*phy_base;
	const void *prop;
	int portnum = 0;

	prop = of_get_property(dev->of_node, "portnum", NULL);
	if (prop)
		portnum = of_read_ulong(prop, 1);

	if (!portnum)
		dev_err(&pdev->dev, "This phy has no usb port\n");

	phy_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	phy_base = devm_ioremap_resource(dev, phy_mem);
	if (IS_ERR(phy_base))
		return PTR_ERR(phy_base);
	phy = devm_kzalloc(&pdev->dev, sizeof(*phy), GFP_KERNEL);
	if (!phy) {
		dev_err(&pdev->dev, "unable to allocate memory for USB3 PHY\n");
		return -ENOMEM;
	}

	dev_info(&pdev->dev, "USB3 phy probe:phy_mem:0x%lx, iomap phy_base:0x%lx\n",
			(unsigned long)phy_mem->start, (unsigned long)phy_base);

	phy->dev		= dev;
	phy->regs		= phy_base;
	phy->portnum      = portnum;
	phy->phy.dev		= phy->dev;
	phy->phy.label		= "amlogic-usbphy3";
	phy->phy.init		= amlogic_new_usb3_init;
	phy->phy.set_suspend	= amlogic_new_usb3_suspend;
	phy->phy.type		= USB_PHY_TYPE_USB3;

	usb_add_phy_dev(&phy->phy);

	platform_set_drvdata(pdev, phy);

	pm_runtime_enable(phy->dev);

	return 0;
}

static int amlogic_new_usb3_remove(struct platform_device *pdev)
{
	return 0;
}

#ifdef CONFIG_PM_RUNTIME

static int amlogic_new_usb3_runtime_suspend(struct device *dev)
{
	return 0;
}

static int amlogic_new_usb3_runtime_resume(struct device *dev)
{
	u32 ret = 0;

	return ret;
}

static const struct dev_pm_ops amlogic_new_usb3_pm_ops = {
	SET_RUNTIME_PM_OPS(amlogic_new_usb3_runtime_suspend,
		amlogic_new_usb3_runtime_resume,
		NULL)
};

#define DEV_PM_OPS     (&amlogic_new_usb3_pm_ops)
#else
#define DEV_PM_OPS     NULL
#endif

#ifdef CONFIG_OF
static const struct of_device_id amlogic_new_usb3_id_table[] = {
	{ .compatible = "amlogic, amlogic-new-usb3" },
	{}
};
MODULE_DEVICE_TABLE(of, amlogic_new_usb3_id_table);
#endif

static struct platform_driver amlogic_new_usb3_driver = {
	.probe		= amlogic_new_usb3_probe,
	.remove		= amlogic_new_usb3_remove,
	.driver		= {
		.name	= "amlogic-new-usb3",
		.owner	= THIS_MODULE,
		.pm	= DEV_PM_OPS,
		.of_match_table = of_match_ptr(amlogic_new_usb3_id_table),
	},
};

module_platform_driver(amlogic_new_usb3_driver);

MODULE_ALIAS("platform: amlogic_usb3");
MODULE_AUTHOR("Amlogic Inc.");
MODULE_DESCRIPTION("amlogic USB3 phy driver");
MODULE_LICENSE("GPL v2");
