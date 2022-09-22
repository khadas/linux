// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/io.h>
#include <linux/list.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/of.h>
#include <linux/acpi.h>
#include <linux/pinctrl/consumer.h>
#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>
#include <linux/usb/of.h>
#include <linux/usb/otg.h>
#include <linux/amlogic/usbtype.h>
#include <linux/clk.h>
#include <linux/phy/phy.h>
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/amlogic/cpu_version.h>

#include <linux/kthread.h>

#include "../../../usb/host/xhci.h"

#define CRG_DEFAULT_AUTOSUSPEND_DELAY	5000 /* ms */
#define CRG_XHCI_RESOURCES_NUM	2
#define CRG_GCTL_PRTCAP_HOST	1
#define CRG_GCTL_PRTCAP_DEVICE	2
#define CRG_GCTL_PRTCAP_OTG	3
#define CRG_XHCI_REGS_END		0x7fff
#define	CRG_U3DC_CFG0_MAXSPEED_SS		(0x4L << 0)

struct crg_drd {
	struct device		*dev;

	struct platform_device	*xhci;
	struct resource		xhci_resources[CRG_XHCI_RESOURCES_NUM];

	void __iomem		*regs;
	size_t			regs_size;
	enum usb_dr_mode	dr_mode;
	u32			maximum_speed;
	void			*mem;
	struct usb_phy		*usb2_phy;
	struct usb_phy		*usb3_phy;

	unsigned		super_speed_support:1;
	bool		usb_phy_init;
	bool		usb_suspend;
};

/* -------------------------------------------------------------------------- */
static void crg_set_mode(struct crg_drd *crg, u32 mode)
{
	u64 tmp;

	if (mode == USB_DR_MODE_HOST) {
		/* set controller host role*/
		tmp = readl(crg->regs + 0x20FC) & ~0x1;
		writel(tmp, crg->regs + 0x20FC);
	}
}

static int crg_core_soft_reset(struct crg_drd *crg)
{
	if (crg->usb2_phy)
		usb_phy_init(crg->usb2_phy);

	if (crg->usb3_phy)
		usb_phy_init(crg->usb3_phy);

	return 0;
}

static void crg_core_exit(struct crg_drd	*crg)
{
	if (crg->usb2_phy)
		usb_phy_shutdown(crg->usb2_phy);
	if (crg->usb3_phy)
		usb_phy_shutdown(crg->usb3_phy);

	if (crg->usb2_phy)
		usb_phy_set_suspend(crg->usb2_phy, 1);
	if (crg->usb3_phy)
		usb_phy_set_suspend(crg->usb3_phy, 1);
}

static int crg_core_init(struct crg_drd *crg)
{
	switch (crg->dr_mode) {
	case USB_DR_MODE_PERIPHERAL:
		crg_set_mode(crg, CRG_GCTL_PRTCAP_DEVICE);
		break;
	case USB_DR_MODE_HOST:
		crg_set_mode(crg, CRG_GCTL_PRTCAP_HOST);
		break;
	case USB_DR_MODE_OTG:
		crg_set_mode(crg, CRG_GCTL_PRTCAP_OTG);
		break;
	default:
		dev_warn(crg->dev, "Unsupported mode %d\n", crg->dr_mode);
		break;
	}

	return 0;
}

static int crg_core_resume(struct crg_drd *crg)
{
	int			ret;

	ret = crg_core_soft_reset(crg);
	if (ret)
		return ret;

	usb_phy_set_suspend(crg->usb2_phy, 0);
	usb_phy_set_suspend(crg->usb3_phy, 0);

	switch (crg->dr_mode) {
	case USB_DR_MODE_PERIPHERAL:
		crg_set_mode(crg, CRG_GCTL_PRTCAP_DEVICE);
		break;
	case USB_DR_MODE_HOST:
		crg_set_mode(crg, CRG_GCTL_PRTCAP_HOST);
		break;
	case USB_DR_MODE_OTG:
		crg_set_mode(crg, CRG_GCTL_PRTCAP_OTG);
		break;
	default:
		dev_warn(crg->dev, "Unsupported mode %d\n", crg->dr_mode);
		break;
	}

	return 0;
}

static int crg_core_get_phy(struct crg_drd *crg)
{
	struct device *dev = crg->dev;

	crg->super_speed_support = 0;

	crg->usb2_phy = devm_usb_get_phy_by_phandle(dev, "usb-phy", 0);

	crg->usb3_phy = devm_usb_get_phy_by_phandle(dev, "usb-phy", 1);

	if (crg->usb3_phy)
		if (crg->usb3_phy->flags == AML_USB3_PHY_ENABLE)
			crg->super_speed_support = 1;

	return 0;
}

static void crg_host_exit(struct crg_drd *crg)
{
	crg_reset_thread_stop(crg->xhci);
	platform_device_unregister(crg->xhci);
}

static int crg_host_init(struct crg_drd *crg)
{
	struct property_entry	props[5];
	struct platform_device	*xhci;
	int			ret, irq;
	struct resource		*res;
	struct platform_device	*crg_pdev = to_platform_device(crg->dev);
	int			prop_idx = 0;

	irq = platform_get_irq_byname(crg_pdev, "host");
	if (irq == -EPROBE_DEFER)
		return irq;

	if (irq <= 0) {
		irq = platform_get_irq_byname(crg_pdev, "crg_usb3");
		if (irq == -EPROBE_DEFER)
			return irq;

		if (irq <= 0) {
			irq = platform_get_irq(crg_pdev, 0);
			if (irq <= 0) {
				if (irq != -EPROBE_DEFER) {
					dev_err(crg->dev,
						"missing host IRQ\n");
				}
				if (!irq)
					irq = -EINVAL;
				return irq;
			}
			res = platform_get_resource(crg_pdev,
							IORESOURCE_IRQ, 0);

		} else {
			res = platform_get_resource_byname(crg_pdev,
							   IORESOURCE_IRQ,
							   "crg_usb3");
		}

	} else {
		res = platform_get_resource_byname(crg_pdev, IORESOURCE_IRQ,
						   "host");
	}

	crg->xhci_resources[1].start = irq;
	crg->xhci_resources[1].end = irq;
	crg->xhci_resources[1].flags = res->flags;
	crg->xhci_resources[1].name = res->name;

	xhci = platform_device_alloc("xhci-hcd", PLATFORM_DEVID_AUTO);
	if (!xhci) {
		dev_err(crg->dev, "couldn't allocate xHCI device\n");
		return -ENOMEM;
	}

	dma_set_coherent_mask(&xhci->dev, crg->dev->coherent_dma_mask);

	xhci->dev.parent	= crg->dev;
	xhci->dev.dma_mask	= crg->dev->dma_mask;
	xhci->dev.dma_parms	= crg->dev->dma_parms;

	crg->xhci = xhci;

	ret = platform_device_add_resources(xhci, crg->xhci_resources,
						CRG_XHCI_RESOURCES_NUM);
	if (ret) {
		dev_err(crg->dev, "couldn't add resources to xHCI device\n");
		goto err1;
	}

	memset(props, 0, sizeof(struct property_entry) * ARRAY_SIZE(props));

	if (crg->super_speed_support)
		props[prop_idx++].name = "super_speed_support";
	if (((is_meson_s4_cpu()) && (is_meson_rev_a())) ||
		((is_meson_s4d_cpu()) && (is_meson_rev_a())) ||
		((is_meson_t3_cpu()) && (is_meson_rev_a())) ||
		((is_meson_t5w_cpu()) && (is_meson_rev_a())))
		props[prop_idx++].name = "xhci-crg-host-011";

	props[prop_idx++].name = "xhci-crg-drd";

	if (((is_meson_t7_cpu()) && (is_meson_rev_a())) ||
		((is_meson_t7_cpu()) && (is_meson_rev_b())) ||
		((is_meson_t3_cpu()) && (is_meson_rev_a())) ||
		((is_meson_t3_cpu()) && (is_meson_rev_b())))
		props[prop_idx++].name = "xhci-crg-host-010";

	if (prop_idx) {
		ret = platform_device_add_properties(xhci, props);
		if (ret) {
			dev_err(crg->dev, "failed to add properties to xHCI\n");
			goto err1;
		}
	}

	ret = platform_device_add(xhci);
	if (ret) {
		dev_err(crg->dev, "failed to register xHCI device\n");
		goto err1;
	}

	return 0;

err1:
	platform_device_put(xhci);
	return ret;
}

static int crg_core_init_mode(struct crg_drd *crg)
{
	struct device *dev = crg->dev;
	int ret;

	switch (crg->dr_mode) {
	case USB_DR_MODE_PERIPHERAL:
	case USB_DR_MODE_OTG:
		break;
	case USB_DR_MODE_HOST:
		ret = crg_host_init(crg);
		if (ret) {
			if (ret != -EPROBE_DEFER)
				dev_err(dev, "failed to initialize host\n");
			return ret;
		}
		break;
	default:
		dev_err(dev, "Unsupported mode of operation %d\n", crg->dr_mode);
		return -EINVAL;
	}

	return 0;
}

#define CRG_ALIGN_MASK		(16 - 1)

static int crg_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct resource	*res;
	struct crg_drd *crg;
	int	ret;
	void *mem;
	u32 tmp;

	mem = devm_kzalloc(dev, sizeof(*crg) + CRG_ALIGN_MASK, GFP_KERNEL);
	if (!mem)
		return -ENOMEM;

	crg = PTR_ALIGN(mem, CRG_ALIGN_MASK + 1);
	crg->mem = mem;
	crg->dev = dev;
	crg->dr_mode = USB_DR_MODE_HOST;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(dev, "missing memory resource\n");
		return -ENODEV;
	}

	crg->xhci_resources[0].start = res->start;
	crg->xhci_resources[0].end = crg->xhci_resources[0].start +
					CRG_XHCI_REGS_END;
	crg->xhci_resources[0].flags = res->flags;
	crg->xhci_resources[0].name = res->name;

	crg->regs = ioremap(res->start, resource_size(res));
	if (!crg->regs) {
		dev_err(dev, "ioremap failed\n");
		return -ENODEV;
	}

	platform_set_drvdata(pdev, crg);

	ret = crg_core_get_phy(crg);
	if (ret)
		goto err0;

	if (!dev->dma_mask) {
		dev->dma_mask = dev->parent->dma_mask;
		dev->dma_parms = dev->parent->dma_parms;
		dma_set_coherent_mask(dev, dev->parent->coherent_dma_mask);
	}

	pm_runtime_set_active(dev);
	pm_runtime_use_autosuspend(dev);
	pm_runtime_set_autosuspend_delay(dev, CRG_DEFAULT_AUTOSUSPEND_DELAY);
	pm_runtime_enable(dev);
	ret = pm_runtime_get_sync(dev);
	if (ret < 0)
		goto err0;

	pm_runtime_forbid(dev);

	ret = crg_core_init(crg);
	if (ret) {
		dev_err(dev, "failed to initialize core\n");
		goto err0;
	}

	if (crg->super_speed_support)
		crg->maximum_speed = USB_SPEED_SUPER;
	else
		crg->maximum_speed = USB_SPEED_HIGH;

	/* Check the maximum_speed parameter */
	switch (crg->maximum_speed) {
	case USB_SPEED_LOW:
	case USB_SPEED_FULL:
	case USB_SPEED_HIGH:
		break;
	case USB_SPEED_SUPER:
		tmp = CRG_U3DC_CFG0_MAXSPEED_SS;
		writel(tmp, crg->regs + 0x2410);
	case USB_SPEED_SUPER_PLUS:
		break;
	default:
		dev_err(dev, "invalid maximum_speed parameter %d\n",
			crg->maximum_speed);
		break;
	}

	ret = crg_core_init_mode(crg);
	if (ret)
		goto err0;

	pm_runtime_put(dev);

	return 0;

err0:
	iounmap(crg->regs);
	return ret;
}

static void crg_shutdown(struct platform_device *pdev)
{
	struct crg_drd     *crg = platform_get_drvdata(pdev);

	pm_runtime_get_sync(&pdev->dev);

	crg_core_exit(crg);
	crg_host_exit(crg);

	pm_runtime_put_sync(&pdev->dev);
	pm_runtime_allow(&pdev->dev);
	pm_runtime_disable(&pdev->dev);
}

static int crg_remove(struct platform_device *pdev)
{
	struct crg_drd	   *crg = platform_get_drvdata(pdev);

	pm_runtime_get_sync(&pdev->dev);

	crg_host_exit(crg);

	pm_runtime_put_sync(&pdev->dev);
	pm_runtime_allow(&pdev->dev);
	pm_runtime_disable(&pdev->dev);

	return 0;
}

#ifdef CONFIG_PM
static int crg_suspend_common(struct crg_drd *crg)
{
	switch (crg->dr_mode) {
	case USB_DR_MODE_PERIPHERAL:
	case USB_DR_MODE_OTG:
		break;
	case USB_DR_MODE_HOST:
	default:
		/* do nothing */
		break;
	}

	crg_core_exit(crg);

	return 0;
}

static int crg_resume_common(struct crg_drd *crg)
{
	int		ret;

	ret = crg_core_resume(crg);
	if (ret)
		return ret;

	crg_set_mode(crg, CRG_GCTL_PRTCAP_HOST);

	return 0;
}

static int crg_runtime_checks(struct crg_drd *crg)
{
	switch (crg->dr_mode) {
	case USB_DR_MODE_PERIPHERAL:
	case USB_DR_MODE_OTG:
	case USB_DR_MODE_HOST:
	default:
		/* do nothing */
		break;
	}

	return 0;
}

static int crg_runtime_suspend(struct device *dev)
{
	struct crg_drd     *crg = dev_get_drvdata(dev);
	int		ret;

	if (crg_runtime_checks(crg))
		return -EBUSY;

	ret = crg_suspend_common(crg);
	if (ret)
		return ret;

	device_init_wakeup(dev, true);

	return 0;
}

static int crg_runtime_resume(struct device *dev)
{
	struct crg_drd     *crg = dev_get_drvdata(dev);
	int		ret;

	device_init_wakeup(dev, false);

	ret = crg_resume_common(crg);
	if (ret)
		return ret;

	switch (crg->dr_mode) {
	case USB_DR_MODE_PERIPHERAL:
	case USB_DR_MODE_OTG:
	case USB_DR_MODE_HOST:
	default:
		/* do nothing */
		break;
	}

	pm_runtime_mark_last_busy(dev);
	pm_runtime_put(dev);

	return 0;
}

static int crg_runtime_idle(struct device *dev)
{
	struct crg_drd     *crg = dev_get_drvdata(dev);

	switch (crg->dr_mode) {
	case USB_DR_MODE_PERIPHERAL:
	case USB_DR_MODE_OTG:
		if (crg_runtime_checks(crg))
			return -EBUSY;
		break;
	case USB_DR_MODE_HOST:
	default:
		/* do nothing */
		break;
	}

	pm_runtime_mark_last_busy(dev);
	pm_runtime_autosuspend(dev);

	return 0;
}
#endif /* CONFIG_PM */

#ifdef CONFIG_PM_SLEEP
static int crg_suspend(struct device *dev)
{
	struct crg_drd	*crg = dev_get_drvdata(dev);
	int		ret;

	crg->usb_suspend = 1;
	ret = crg_suspend_common(crg);
	if (ret)
		return ret;

	pinctrl_pm_select_sleep_state(dev);

	return 0;
}

static int crg_resume(struct device *dev)
{
	struct crg_drd	*crg = dev_get_drvdata(dev);
	int		ret;

	crg->usb_suspend = 0;

	pinctrl_pm_select_default_state(dev);

	ret = crg_resume_common(crg);
	if (ret)
		return ret;

	pm_runtime_disable(dev);
	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);

	return 0;
}
#endif /* CONFIG_PM_SLEEP */

static const struct dev_pm_ops crg_dev_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(crg_suspend, crg_resume)
	SET_RUNTIME_PM_OPS(crg_runtime_suspend, crg_runtime_resume,
			crg_runtime_idle)
};

#ifdef CONFIG_OF
static const struct of_device_id of_crg_match[] = {
	{
		.compatible = "amlogic, crg-drd"
	},
	{ },
};
MODULE_DEVICE_TABLE(of, of_crg_match);
#endif

static struct platform_driver crg_driver = {
	.probe		= crg_probe,
	.remove		= crg_remove,
	.shutdown	= crg_shutdown,
	.driver		= {
		.name	= "crg_drd_otg",
		.of_match_table	= of_match_ptr(of_crg_match),
		.pm	= &crg_dev_pm_ops,
	},
};

#ifdef CONFIG_OF
static const struct of_device_id of_crg_host_match[] = {
	{
		.compatible = "amlogic, crg-host-drd"
	},
	{ },
};
MODULE_DEVICE_TABLE(of, of_crg_host_match);
#endif

static struct platform_driver crg_host_driver = {
	.remove		= crg_remove,
	.shutdown	= crg_shutdown,
	.driver		= {
		.name	= "crg_drd",
		.of_match_table	= of_match_ptr(of_crg_host_match),
		.pm	= &crg_dev_pm_ops,
	},
};

void crg_exit(void)
{
	if (!crg_driver.driver.p)
		return;
	platform_driver_unregister(&crg_driver);
}

int crg_init(void)
{
	return platform_driver_register(&crg_driver);
}

/* AMLOGIC corigine driver does not allow module unload */
static int __init amlogic_crg_init(void)
{
	platform_driver_probe(&crg_host_driver, crg_probe);

	return 0;
}
late_initcall(amlogic_crg_init);

MODULE_ALIAS("platform:crg");
MODULE_AUTHOR("yue wang <yue.wang@amlogic.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("corigine USB3 DRD Controller Driver");
