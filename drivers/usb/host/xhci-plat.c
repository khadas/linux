// SPDX-License-Identifier: GPL-2.0
/*
 * xhci-plat.c - xHCI host controller driver platform Bus Glue.
 *
 * Copyright (C) 2012 Texas Instruments Incorporated - http://www.ti.com
 * Author: Sebastian Andrzej Siewior <bigeasy@linutronix.de>
 *
 * A lot of code borrowed from the Linux xHCI driver.
 */

#include <linux/clk.h>
#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/usb/phy.h>
#include <linux/slab.h>
#include <linux/acpi.h>
#include <linux/usb/of.h>
#ifdef CONFIG_AMLOGIC_USB
#include <linux/kthread.h>
#include <linux/amlogic/cpu_version.h>
#endif

#include "xhci.h"
#include "xhci-plat.h"
#include "xhci-mvebu.h"
#include "xhci-rcar.h"

static struct hc_driver __read_mostly xhci_plat_hc_driver;

static int xhci_plat_setup(struct usb_hcd *hcd);
static int xhci_plat_start(struct usb_hcd *hcd);

static const struct xhci_driver_overrides xhci_plat_overrides __initconst = {
	.extra_priv_size = sizeof(struct xhci_plat_priv),
	.reset = xhci_plat_setup,
	.start = xhci_plat_start,
};

static void xhci_priv_plat_start(struct usb_hcd *hcd)
{
	struct xhci_plat_priv *priv = hcd_to_xhci_priv(hcd);

	if (priv->plat_start)
		priv->plat_start(hcd);
}

static int xhci_priv_plat_setup(struct usb_hcd *hcd)
{
	struct xhci_plat_priv *priv = hcd_to_xhci_priv(hcd);

	if (!priv->plat_setup)
		return 0;

	return priv->plat_setup(hcd);
}

static int xhci_priv_init_quirk(struct usb_hcd *hcd)
{
	struct xhci_plat_priv *priv = hcd_to_xhci_priv(hcd);

	if (!priv->init_quirk)
		return 0;

	return priv->init_quirk(hcd);
}

static int xhci_priv_resume_quirk(struct usb_hcd *hcd)
{
	struct xhci_plat_priv *priv = hcd_to_xhci_priv(hcd);

	if (!priv->resume_quirk)
		return 0;

	return priv->resume_quirk(hcd);
}

static void xhci_plat_quirks(struct device *dev, struct xhci_hcd *xhci)
{
	struct xhci_plat_priv *priv = xhci_to_priv(xhci);

	/*
	 * As of now platform drivers don't provide MSI support so we ensure
	 * here that the generic code does not try to make a pci_dev from our
	 * dev struct in order to setup MSI
	 */
	xhci->quirks |= XHCI_PLAT | priv->quirks;
}

/* called during probe() after chip reset completes */
static int xhci_plat_setup(struct usb_hcd *hcd)
{
	int ret;


	ret = xhci_priv_init_quirk(hcd);
	if (ret)
		return ret;

	return xhci_gen_setup(hcd, xhci_plat_quirks);
}

static int xhci_plat_start(struct usb_hcd *hcd)
{
	xhci_priv_plat_start(hcd);
	return xhci_run(hcd);
}

#ifdef CONFIG_OF
static const struct xhci_plat_priv xhci_plat_marvell_armada = {
	.init_quirk = xhci_mvebu_mbus_init_quirk,
};

static const struct xhci_plat_priv xhci_plat_marvell_armada3700 = {
	.plat_setup = xhci_mvebu_a3700_plat_setup,
	.init_quirk = xhci_mvebu_a3700_init_quirk,
};

static const struct xhci_plat_priv xhci_plat_renesas_rcar_gen2 = {
	SET_XHCI_PLAT_PRIV_FOR_RCAR(XHCI_RCAR_FIRMWARE_NAME_V1)
};

static const struct xhci_plat_priv xhci_plat_renesas_rcar_gen3 = {
	SET_XHCI_PLAT_PRIV_FOR_RCAR(XHCI_RCAR_FIRMWARE_NAME_V3)
};

static const struct of_device_id usb_xhci_of_match[] = {
	{
		.compatible = "generic-xhci",
	}, {
		.compatible = "xhci-platform",
	}, {
		.compatible = "marvell,armada-375-xhci",
		.data = &xhci_plat_marvell_armada,
	}, {
		.compatible = "marvell,armada-380-xhci",
		.data = &xhci_plat_marvell_armada,
	}, {
		.compatible = "marvell,armada3700-xhci",
		.data = &xhci_plat_marvell_armada3700,
	}, {
		.compatible = "renesas,xhci-r8a7790",
		.data = &xhci_plat_renesas_rcar_gen2,
	}, {
		.compatible = "renesas,xhci-r8a7791",
		.data = &xhci_plat_renesas_rcar_gen2,
	}, {
		.compatible = "renesas,xhci-r8a7793",
		.data = &xhci_plat_renesas_rcar_gen2,
	}, {
		.compatible = "renesas,xhci-r8a7795",
		.data = &xhci_plat_renesas_rcar_gen3,
	}, {
		.compatible = "renesas,xhci-r8a7796",
		.data = &xhci_plat_renesas_rcar_gen3,
	}, {
		.compatible = "renesas,rcar-gen2-xhci",
		.data = &xhci_plat_renesas_rcar_gen2,
	}, {
		.compatible = "renesas,rcar-gen3-xhci",
		.data = &xhci_plat_renesas_rcar_gen3,
	},
	{},
};
MODULE_DEVICE_TABLE(of, usb_xhci_of_match);
#endif

#ifdef CONFIG_AMLOGIC_USB
struct crg_reset crg_task[CRG_XHCI_MAX_COUNT];
static unsigned int crg_xhci_count;

static int xhci_plat_probe(struct platform_device *pdev);
static int xhci_plat_remove(struct platform_device *dev);

static void crg_reset_init(void)
{
	int i;

	for (i = 0; i < CRG_XHCI_MAX_COUNT; i++)
		crg_task[i].id = -1;
}

static int crg_reset_thread(void *data)
{
	struct platform_device *plat_dev = data;
	struct usb_hcd	*hcd = platform_get_drvdata(plat_dev);
	int i, mutex_id;

	mutex_id = 0;
	for (i = 0; i < CRG_XHCI_MAX_COUNT; i++) {
		if (crg_task[i].id == plat_dev->id) {
			mutex_id = i;
			break;
		}
	}
	while (!crg_task[mutex_id].hcd_mutex)
		msleep(20);
	while (!kthread_should_stop()) {
		mutex_lock(crg_task[mutex_id].hcd_mutex);
		hcd = platform_get_drvdata(plat_dev);
		if (crg_task[mutex_id].hcd_removed_flag == 1) {
			mutex_unlock(crg_task[mutex_id].hcd_mutex);
			msleep(500);
		} else if (hcd && hcd->crg_do_reset == 1) {
			mutex_unlock(crg_task[mutex_id].hcd_mutex);
			xhci_plat_remove(plat_dev);
			xhci_plat_probe(plat_dev);
		} else {
			mutex_unlock(crg_task[mutex_id].hcd_mutex);
		}
		msleep(20);
	}

	crg_task[mutex_id].crg_reset_task = NULL;
	crg_task[mutex_id].id = -1;
	kfree(crg_task[mutex_id].hcd_mutex);

	return 0;
}

void crg_reset_thread_stop(struct platform_device *pdev)
{
	int i;
	struct xhci_hcd		*xhci;
	struct usb_hcd	*hcd = platform_get_drvdata(pdev);

	xhci = hcd_to_xhci(hcd);
	if (!(xhci->quirks & XHCI_CRG_HOST) &&
		!is_meson_t7_cpu()) {
		return;
	}

	for (i = 0; i < CRG_XHCI_MAX_COUNT; i++) {
		if (crg_task[i].id == pdev->id)
			break;
	}
	if (i >= CRG_XHCI_MAX_COUNT) {
		dev_err(&pdev->dev,
				"Can't find the crg_task.id( dev->id=%d), not do unregister\n",
				pdev->id);
		return;
	}
	if (crg_task[i].crg_reset_task)
		kthread_stop(crg_task[i].crg_reset_task);
}
#endif

static int xhci_plat_probe(struct platform_device *pdev)
{
	const struct xhci_plat_priv *priv_match;
	const struct hc_driver	*driver;
	struct device		*sysdev, *tmpdev;
	struct xhci_hcd		*xhci;
	struct resource         *res;
	struct usb_hcd		*hcd;
	int			ret;
	int			irq;
	struct xhci_plat_priv	*priv = NULL;
#ifdef CONFIG_AMLOGIC_USB
	char crg_thread_name[32];
	int i, j, task_exsit_flag = 0;
#endif

	if (usb_disabled())
		return -ENODEV;

	driver = &xhci_plat_hc_driver;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return irq;

	/*
	 * sysdev must point to a device that is known to the system firmware
	 * or PCI hardware. We handle these three cases here:
	 * 1. xhci_plat comes from firmware
	 * 2. xhci_plat is child of a device from firmware (dwc3-plat)
	 * 3. xhci_plat is grandchild of a pci device (dwc3-pci)
	 */
	for (sysdev = &pdev->dev; sysdev; sysdev = sysdev->parent) {
		if (is_of_node(sysdev->fwnode) ||
			is_acpi_device_node(sysdev->fwnode))
			break;
#ifdef CONFIG_PCI
		else if (sysdev->bus == &pci_bus_type)
			break;
#endif
	}

	if (!sysdev)
		sysdev = &pdev->dev;

	/* Try to set 64-bit DMA first */
	if (WARN_ON(!sysdev->dma_mask))
		/* Platform did not initialize dma_mask */
		ret = dma_coerce_mask_and_coherent(sysdev,
						   DMA_BIT_MASK(64));
	else
		ret = dma_set_mask_and_coherent(sysdev, DMA_BIT_MASK(64));

	/* If seting 64-bit DMA mask fails, fall back to 32-bit DMA mask */
	if (ret) {
		ret = dma_set_mask_and_coherent(sysdev, DMA_BIT_MASK(32));
		if (ret)
			return ret;
	}

	pm_runtime_set_active(&pdev->dev);
	pm_runtime_enable(&pdev->dev);
	pm_runtime_get_noresume(&pdev->dev);

	hcd = __usb_create_hcd(driver, sysdev, &pdev->dev,
			       dev_name(&pdev->dev), NULL);
	if (!hcd) {
		ret = -ENOMEM;
		goto disable_runtime;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	hcd->regs = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(hcd->regs)) {
		ret = PTR_ERR(hcd->regs);
		goto put_hcd;
	}

	hcd->rsrc_start = res->start;
	hcd->rsrc_len = resource_size(res);

#ifdef CONFIG_AMLOGIC_USB
	set_bit(HCD_FLAG_DWC3, &hcd->flags);
#endif

	xhci = hcd_to_xhci(hcd);


	/*
	 * Not all platforms have clks so it is not an error if the
	 * clock do not exist.
	 */
	xhci->reg_clk = devm_clk_get_optional(&pdev->dev, "reg");
	if (IS_ERR(xhci->reg_clk)) {
		ret = PTR_ERR(xhci->reg_clk);
		goto put_hcd;
	}

	ret = clk_prepare_enable(xhci->reg_clk);
	if (ret)
		goto put_hcd;

	xhci->clk = devm_clk_get_optional(&pdev->dev, NULL);
	if (IS_ERR(xhci->clk)) {
		ret = PTR_ERR(xhci->clk);
		goto disable_reg_clk;
	}

	ret = clk_prepare_enable(xhci->clk);
	if (ret)
		goto disable_reg_clk;

	priv_match = of_device_get_match_data(&pdev->dev);
	if (priv_match) {
		priv = hcd_to_xhci_priv(hcd);
		/* Just copy data for now */
		if (priv_match)
			*priv = *priv_match;
	}

	device_set_wakeup_capable(&pdev->dev, true);

	xhci->main_hcd = hcd;
	xhci->shared_hcd = __usb_create_hcd(driver, sysdev, &pdev->dev,
			dev_name(&pdev->dev), hcd);
	if (!xhci->shared_hcd) {
		ret = -ENOMEM;
		goto disable_clk;
	}

	/* imod_interval is the interrupt moderation value in nanoseconds. */
	xhci->imod_interval = 40000;

	/* Iterate over all parent nodes for finding quirks */
	for (tmpdev = &pdev->dev; tmpdev; tmpdev = tmpdev->parent) {

		if (device_property_read_bool(tmpdev, "usb2-lpm-disable"))
			xhci->quirks |= XHCI_HW_LPM_DISABLE;

		if (device_property_read_bool(tmpdev, "usb3-lpm-capable"))
			xhci->quirks |= XHCI_LPM_SUPPORT;

		if (device_property_read_bool(tmpdev, "quirk-broken-port-ped"))
			xhci->quirks |= XHCI_BROKEN_PORT_PED;

		device_property_read_u32(tmpdev, "imod-interval-ns",
					 &xhci->imod_interval);
	}

#ifdef CONFIG_AMLOGIC_USB
	if (device_property_read_bool(&pdev->dev, "super_speed_support"))
		xhci->quirks |= XHCI_AML_SUPER_SPEED_SUPPORT;

	if (device_property_read_bool(&pdev->dev, "xhci-crg-host"))
		xhci->quirks |= XHCI_CRG_HOST;
	if (device_property_read_bool(&pdev->dev, "xhci-crg-host-011"))
		xhci->quirks |= XHCI_CRG_HOST_011;
	if (device_property_read_bool(&pdev->dev, "xhci-crg-drd"))
		xhci->quirks |= XHCI_CRG_DRD;
	if (device_property_read_bool(&pdev->dev, "xhci-crg-host-010"))
		xhci->quirks |= XHCI_CRG_HOST_010;
#endif

	hcd->usb_phy = devm_usb_get_phy_by_phandle(sysdev, "usb-phy", 0);
	if (IS_ERR(hcd->usb_phy)) {
		ret = PTR_ERR(hcd->usb_phy);
		if (ret == -EPROBE_DEFER)
			goto put_usb3_hcd;
		hcd->usb_phy = NULL;
	} else {
		ret = usb_phy_init(hcd->usb_phy);
		if (ret)
			goto put_usb3_hcd;
	}

	hcd->tpl_support = of_usb_host_tpl_support(sysdev->of_node);
	xhci->shared_hcd->tpl_support = hcd->tpl_support;

	if (priv) {
		ret = xhci_priv_plat_setup(hcd);
		if (ret)
			goto disable_usb_phy;
	}

	if ((xhci->quirks & XHCI_SKIP_PHY_INIT) || (priv && (priv->quirks & XHCI_SKIP_PHY_INIT)))
		hcd->skip_phy_initialization = 1;

	ret = usb_add_hcd(hcd, irq, IRQF_SHARED);
	if (ret)
		goto disable_usb_phy;

	if (HCC_MAX_PSA(xhci->hcc_params) >= 4)
		xhci->shared_hcd->can_do_streams = 1;

	ret = usb_add_hcd(xhci->shared_hcd, irq, IRQF_SHARED);
	if (ret)
		goto dealloc_usb2_hcd;

	device_enable_async_suspend(&pdev->dev);
	pm_runtime_put_noidle(&pdev->dev);

	/*
	 * Prevent runtime pm from being on as default, users should enable
	 * runtime pm using power/control in sysfs.
	 */
	pm_runtime_forbid(&pdev->dev);

#ifdef CONFIG_AMLOGIC_USB
	if (!crg_task[0].crg_reset_task)
		crg_reset_init();
	for (i = 0; i < CRG_XHCI_MAX_COUNT; i++) {
		if (!(xhci->quirks & XHCI_CRG_HOST) &&
			!is_meson_t7_cpu()) {
			break;
		}
		if (!crg_task[i].crg_reset_task) {
			for (j = 0; j < i; j++) {
				if (crg_task[j].id == pdev->id) {
					crg_task[j].hcd_removed_flag = 0;
					task_exsit_flag = 1;
					break;
				}
			}
			if (task_exsit_flag == 1)
				break;
			crg_task[i].hcd_mutex = kmalloc(sizeof(*crg_task[i].hcd_mutex),
				GFP_KERNEL);
			if (!crg_task[i].hcd_mutex)
				goto dealloc_usb2_hcd;
			mutex_init(crg_task[i].hcd_mutex);

			crg_task[i].id = pdev->id;
			crg_task[i].hcd_removed_flag = 0;
			crg_xhci_count = i;

			sprintf(crg_thread_name, "crg_reset_%d_thr\n", i);
			crg_task[i].crg_reset_task =
				kthread_run(crg_reset_thread, pdev, crg_thread_name);
			if (IS_ERR(crg_task[i].crg_reset_task)) {
				xhci_err(xhci, "unable to start crg_reset_thread\n");
				goto dealloc_usb2_hcd;
			}
			break;
		}
	}
#endif

	return 0;


dealloc_usb2_hcd:
	usb_remove_hcd(hcd);

disable_usb_phy:
	usb_phy_shutdown(hcd->usb_phy);

put_usb3_hcd:
	usb_put_hcd(xhci->shared_hcd);

disable_clk:
	clk_disable_unprepare(xhci->clk);

disable_reg_clk:
	clk_disable_unprepare(xhci->reg_clk);

put_hcd:
	usb_put_hcd(hcd);

disable_runtime:
	pm_runtime_put_noidle(&pdev->dev);
	pm_runtime_disable(&pdev->dev);

	return ret;
}

#ifdef CONFIG_AMLOGIC_USB
static int xhci_plat_and_thread_remove(struct platform_device *dev)
{
	int i;
	struct usb_hcd	*hcd;
	struct xhci_hcd	*xhci;
	struct clk *clk;
	struct clk *reg_clk;
	struct usb_hcd *shared_hcd;

	for (i = 0; i < CRG_XHCI_MAX_COUNT; i++) {
		if (dev && crg_task[i].id == dev->id)
			break;
	}
	if (i >= CRG_XHCI_MAX_COUNT) {
		dev_err(&dev->dev,
				"xhci:Can't find the crg_task.id, dev->id=%d\n",
				dev->id);
		dev_err(&dev->dev,
				"%s do noting and return\n", __func__);
		return 0;
	}
	if (crg_task[i].hcd_removed_flag == 1) {
		dev_err(&dev->dev,
				"the platform dev has been removed, dev->id=%d\n",
				dev->id);
		return 0;
	}
	if (crg_task[i].hcd_mutex) {
		mutex_lock(crg_task[i].hcd_mutex);
		crg_task[i].hcd_removed_flag = 1;
	}
	hcd = platform_get_drvdata(dev);
	if (hcd) {
		xhci = hcd_to_xhci(hcd);
		clk = xhci->clk;
		reg_clk = xhci->reg_clk;
		shared_hcd = xhci->shared_hcd;
	} else {
		if (crg_task[i].hcd_mutex)
			mutex_unlock(crg_task[i].hcd_mutex);
		return 0;
	}

	pm_runtime_get_sync(&dev->dev);
	xhci->xhc_state |= XHCI_STATE_REMOVING;

	usb_remove_hcd(shared_hcd);
	xhci->shared_hcd = NULL;
	usb_phy_shutdown(hcd->usb_phy);

	usb_remove_hcd(hcd);
	usb_put_hcd(shared_hcd);

	clk_disable_unprepare(clk);
	clk_disable_unprepare(reg_clk);

	devm_release_mem_region(&dev->dev, hcd->rsrc_start, hcd->rsrc_len);
	usb_put_hcd(hcd);

	pm_runtime_disable(&dev->dev);
	pm_runtime_put_noidle(&dev->dev);
	pm_runtime_set_suspended(&dev->dev);

	if (crg_task[i].hcd_mutex)
		mutex_unlock(crg_task[i].hcd_mutex);

	return 0;
}
#endif

static int xhci_plat_remove(struct platform_device *dev)
{
	struct usb_hcd	*hcd = platform_get_drvdata(dev);
	struct xhci_hcd	*xhci = hcd_to_xhci(hcd);
	struct clk *clk = xhci->clk;
	struct clk *reg_clk = xhci->reg_clk;
	struct usb_hcd *shared_hcd = xhci->shared_hcd;

#ifdef CONFIG_AMLOGIC_USB
	if ((xhci->quirks & XHCI_CRG_HOST) &&
		is_meson_t7_cpu()) {
		xhci_plat_and_thread_remove(dev);
		return 0;
	}
#endif
	pm_runtime_get_sync(&dev->dev);
	xhci->xhc_state |= XHCI_STATE_REMOVING;

	usb_remove_hcd(shared_hcd);
	xhci->shared_hcd = NULL;
	usb_phy_shutdown(hcd->usb_phy);

	usb_remove_hcd(hcd);
	usb_put_hcd(shared_hcd);

	clk_disable_unprepare(clk);
	clk_disable_unprepare(reg_clk);
#ifdef CONFIG_AMLOGIC_USB
	devm_release_mem_region(&dev->dev, hcd->rsrc_start, hcd->rsrc_len);
#endif
	usb_put_hcd(hcd);

	pm_runtime_disable(&dev->dev);
	pm_runtime_put_noidle(&dev->dev);
	pm_runtime_set_suspended(&dev->dev);

	return 0;
}

static int __maybe_unused xhci_plat_suspend(struct device *dev)
{
	struct usb_hcd	*hcd = dev_get_drvdata(dev);
	struct xhci_hcd	*xhci = hcd_to_xhci(hcd);

	/*
	 * xhci_suspend() needs `do_wakeup` to know whether host is allowed
	 * to do wakeup during suspend. Since xhci_plat_suspend is currently
	 * only designed for system suspend, device_may_wakeup() is enough
	 * to dertermine whether host is allowed to do wakeup. Need to
	 * reconsider this when xhci_plat_suspend enlarges its scope, e.g.,
	 * also applies to runtime suspend.
	 */
	return xhci_suspend(xhci, device_may_wakeup(dev));
}

static int __maybe_unused xhci_plat_resume(struct device *dev)
{
	struct usb_hcd	*hcd = dev_get_drvdata(dev);
	struct xhci_hcd	*xhci = hcd_to_xhci(hcd);
	int ret;

	ret = xhci_priv_resume_quirk(hcd);
	if (ret)
		return ret;

	return xhci_resume(xhci, 0);
}

static int __maybe_unused xhci_plat_runtime_suspend(struct device *dev)
{
	struct usb_hcd  *hcd = dev_get_drvdata(dev);
	struct xhci_hcd *xhci = hcd_to_xhci(hcd);

	return xhci_suspend(xhci, true);
}

static int __maybe_unused xhci_plat_runtime_resume(struct device *dev)
{
	struct usb_hcd  *hcd = dev_get_drvdata(dev);
	struct xhci_hcd *xhci = hcd_to_xhci(hcd);

	return xhci_resume(xhci, 0);
}

static const struct dev_pm_ops xhci_plat_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(xhci_plat_suspend, xhci_plat_resume)

	SET_RUNTIME_PM_OPS(xhci_plat_runtime_suspend,
			   xhci_plat_runtime_resume,
			   NULL)
};

static const struct acpi_device_id usb_xhci_acpi_match[] = {
	/* XHCI-compliant USB Controller */
	{ "PNP0D10", },
	{ }
};
MODULE_DEVICE_TABLE(acpi, usb_xhci_acpi_match);

static struct platform_driver usb_xhci_driver = {
	.probe	= xhci_plat_probe,
	.remove	= xhci_plat_remove,
	.shutdown = usb_hcd_platform_shutdown,
	.driver	= {
		.name = "xhci-hcd",
		.pm = &xhci_plat_pm_ops,
		.of_match_table = of_match_ptr(usb_xhci_of_match),
		.acpi_match_table = ACPI_PTR(usb_xhci_acpi_match),
	},
};
MODULE_ALIAS("platform:xhci-hcd");

static int __init xhci_plat_init(void)
{
	xhci_init_driver(&xhci_plat_hc_driver, &xhci_plat_overrides);
	return platform_driver_register(&usb_xhci_driver);
}
module_init(xhci_plat_init);

static void __exit xhci_plat_exit(void)
{
	platform_driver_unregister(&usb_xhci_driver);
}
module_exit(xhci_plat_exit);

MODULE_DESCRIPTION("xHCI Platform Host Controller Driver");
MODULE_LICENSE("GPL");
