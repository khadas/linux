/*
 * xhci-plat.c - xHCI host controller driver platform Bus Glue.
 *
 * Copyright (C) 2012 Texas Instruments Incorporated - http://www.ti.com
 * Author: Sebastian Andrzej Siewior <bigeasy@linutronix.de>
 *
 * A lot of code borrowed from the Linux xHCI driver.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 */

#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/usb/xhci_pdriver.h>
#include <linux/io.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/preempt.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>
#include <linux/wakelock_android.h>

#include "xhci.h"

static void xhci_plat_quirks(struct device *dev, struct xhci_hcd *xhci)
{
	/*
	 * As of now platform drivers don't provide MSI support so we ensure
	 * here that the generic code does not try to make a pci_dev from our
	 * dev struct in order to setup MSI
	 */
	xhci->quirks |= XHCI_PLAT;
}

/* called during probe() after chip reset completes */
static int xhci_plat_setup(struct usb_hcd *hcd)
{
	return xhci_gen_setup(hcd, xhci_plat_quirks);
}

static const struct hc_driver xhci_plat_xhci_driver = {
	.description =		"xhci-hcd",
	.product_desc =		"xHCI Host Controller",
	.hcd_priv_size =	sizeof(struct xhci_hcd *),

	/*
	 * generic hardware linkage
	 */
	.irq =			xhci_irq,
	.flags =		HCD_MEMORY | HCD_USB3 | HCD_SHARED,

	/*
	 * basic lifecycle operations
	 */
	.reset =		xhci_plat_setup,
	.start =		xhci_run,
	.stop =			xhci_stop,
	.shutdown =		xhci_shutdown,

	/*
	 * managing i/o requests and associated device resources
	 */
	.urb_enqueue =		xhci_urb_enqueue,
	.urb_dequeue =		xhci_urb_dequeue,
	.alloc_dev =		xhci_alloc_dev,
	.free_dev =			xhci_free_dev,
	.alloc_streams =	xhci_alloc_streams,
	.free_streams =		xhci_free_streams,
	.add_endpoint =		xhci_add_endpoint,
	.drop_endpoint =	xhci_drop_endpoint,
	.endpoint_reset =	xhci_endpoint_reset,
	.check_bandwidth =	xhci_check_bandwidth,
	.reset_bandwidth =	xhci_reset_bandwidth,
	.address_device =	xhci_address_device,
	.enable_device =	xhci_enable_device,
	.update_hub_device =	xhci_update_hub_device,
	.reset_device =		xhci_discover_or_reset_device,

	/*
	 * scheduling support
	 */
	.get_frame_number =	xhci_get_frame,

	/* Root hub support */
	.hub_control =		xhci_hub_control,
	.hub_status_data =	xhci_hub_status_data,
	.bus_suspend =		xhci_bus_suspend,
	.bus_resume =		xhci_bus_resume,
};

struct timer_list	xhci_reset_timer;
EXPORT_SYMBOL(xhci_reset_timer);
static int xhci_plat_remove(struct platform_device *dev);
struct platform_device *g_pdev;
#define X_FILE  "/sys/devices/c9000000.dwc3/xhci-hcd.0.auto/xhci_power"
struct delayed_work xhci_work;

int connect_status = 0;
struct wake_lock xhci_wake_lock;
static DEFINE_MUTEX(xhci_power_lock);

void xhci_reset_test(unsigned long arg)
{
	schedule_delayed_work(&xhci_work, msecs_to_jiffies(10));
}

static ssize_t show_xhci_status(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	return sprintf(buf, "%d\n", connect_status);
}


static ssize_t store_xhci_power(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int power_on;
	struct usb_hcd	*hcd = platform_get_drvdata(g_pdev);
	struct resource         *res;
	int ret = 0;
	struct xhci_hcd		*xhci;
	int irq;
	const struct hc_driver	*driver;

	if (sscanf(buf, "%d", &power_on) != 1)
		return -EINVAL;

	mutex_lock(&xhci_power_lock);

	if (power_on == 0) {
		if (connect_status == 0) {
			clear_bit(HCD_FLAG_DEAD, &hcd->flags);
			xhci_plat_remove(g_pdev);
			connect_status = 1;
		}
	} else if (power_on == 1) {
		if (connect_status == 1) {
			driver = &xhci_plat_xhci_driver;

			irq = platform_get_irq(g_pdev, 0);
			if (irq < 0) {
				mutex_unlock(&xhci_power_lock);
				return -ENODEV;
			}

			res = platform_get_resource(g_pdev, IORESOURCE_MEM, 0);
			if (!res) {
				mutex_unlock(&xhci_power_lock);
				return -ENODEV;
			}
			hcd = usb_create_hcd(driver, &g_pdev->dev,
					dev_name(&g_pdev->dev));
			if (!hcd) {
				mutex_unlock(&xhci_power_lock);
				return -ENOMEM;
			}

			hcd->rsrc_start = res->start;
			hcd->rsrc_len = resource_size(res);
			set_bit(HCD_FLAG_DWC3, &hcd->flags);

			if (!request_mem_region(hcd->rsrc_start, hcd->rsrc_len,
					driver->description)) {
				ret = -EBUSY;
				mutex_unlock(&xhci_power_lock);
				return ret;
			}

			hcd->regs = ioremap_nocache(hcd->rsrc_start,
							hcd->rsrc_len);
			if (!hcd->regs) {
				ret = -EFAULT;
				mutex_unlock(&xhci_power_lock);
				return ret;
			}

			ret = usb_add_hcd(hcd, irq, IRQF_SHARED);
			if (ret) {
				mutex_unlock(&xhci_power_lock);
				return ret;
			}
			device_wakeup_enable(hcd->self.controller);

			xhci = hcd_to_xhci(hcd);
			xhci->shared_hcd = usb_create_shared_hcd(driver,
				&g_pdev->dev, dev_name(&g_pdev->dev), hcd);
			if (!xhci->shared_hcd) {
				ret = -ENOMEM;
				mutex_unlock(&xhci_power_lock);
				return ret;
			}

			*((struct xhci_hcd **) xhci->shared_hcd->hcd_priv)
					= xhci;

			ret = usb_add_hcd(xhci->shared_hcd, irq, IRQF_SHARED);
			if (ret) {
				mutex_unlock(&xhci_power_lock);
				return ret;
			}
		}
		connect_status = 0;
	}

	mutex_unlock(&xhci_power_lock);

	return count;
}


DEVICE_ATTR(xhci_power, 0777, show_xhci_status, store_xhci_power);


static void amlogic_xhci_work(struct work_struct *work)
{
	mm_segment_t oldfs;
	struct file *filp;

	pr_info("%s lock\n", __func__);
	wake_lock(&xhci_wake_lock);

	oldfs = get_fs();
	set_fs(KERNEL_DS);

	filp = filp_open(X_FILE, O_RDWR, 0);

	if (!filp || IS_ERR(filp)) {
		pr_info("%s amlogic_power: failed to access XHCI\n", __func__);
	} else {
		filp->f_op->write(filp, "0", 1, &filp->f_pos);

		msleep(100);

		filp->f_op->write(filp, "1", 1, &filp->f_pos);

		filp_close(filp, NULL);
	}

	set_fs(oldfs);
	pr_info("%s unlock\n", __func__);
	wake_unlock(&xhci_wake_lock);
}

static int xhci_plat_probe(struct platform_device *pdev)
{
	struct device_node	*node = pdev->dev.of_node;
	struct usb_xhci_pdata	*pdata = dev_get_platdata(&pdev->dev);
	const struct hc_driver	*driver;
	struct xhci_hcd		*xhci;
	struct resource         *res;
	struct usb_hcd		*hcd;
	int			ret;
	int			irq;

	if (usb_disabled())
		return -ENODEV;

	g_pdev = pdev;

	driver = &xhci_plat_xhci_driver;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return -ENODEV;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -ENODEV;

	/* Initialize dma_mask and coherent_dma_mask to 32-bits */
	ret = dma_set_coherent_mask(&pdev->dev, DMA_BIT_MASK(32));
	if (ret)
		return ret;
	if (!pdev->dev.dma_mask)
		pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;
	else
		dma_set_mask(&pdev->dev, DMA_BIT_MASK(32));

	hcd = usb_create_hcd(driver, &pdev->dev, dev_name(&pdev->dev));
	if (!hcd)
		return -ENOMEM;

	hcd->rsrc_start = res->start;
	hcd->rsrc_len = resource_size(res);
	set_bit(HCD_FLAG_DWC3, &hcd->flags);

	if (!request_mem_region(hcd->rsrc_start, hcd->rsrc_len,
				driver->description)) {
		dev_dbg(&pdev->dev, "controller already in use\n");
		ret = -EBUSY;
		goto put_hcd;
	}

	hcd->regs = ioremap_nocache(hcd->rsrc_start, hcd->rsrc_len);
	if (!hcd->regs) {
		dev_dbg(&pdev->dev, "error mapping memory\n");
		ret = -EFAULT;
		goto release_mem_region;
	}

	ret = usb_add_hcd(hcd, irq, IRQF_SHARED);
	if (ret)
		goto unmap_registers;
	device_wakeup_enable(hcd->self.controller);

	/* USB 2.0 roothub is stored in the platform_device now. */
	hcd = platform_get_drvdata(pdev);
	xhci = hcd_to_xhci(hcd);
	xhci->shared_hcd = usb_create_shared_hcd(driver, &pdev->dev,
			dev_name(&pdev->dev), hcd);
	if (!xhci->shared_hcd) {
		ret = -ENOMEM;
		goto dealloc_usb2_hcd;
	}

	if ((node && of_property_read_bool(node, "usb3-lpm-capable")) ||
			(pdata && pdata->usb3_lpm_capable))
		xhci->quirks |= XHCI_LPM_SUPPORT;
	/*
	 * Set the xHCI pointer before xhci_plat_setup() (aka hcd_driver.reset)
	 * is called by usb_add_hcd().
	 */
	*((struct xhci_hcd **) xhci->shared_hcd->hcd_priv) = xhci;

	ret = usb_add_hcd(xhci->shared_hcd, irq, IRQF_SHARED);
	if (ret)
		goto put_usb3_hcd;

	init_timer(&xhci_reset_timer);
	xhci_reset_timer.data = (unsigned long) pdev;
	xhci_reset_timer.function = xhci_reset_test;

	device_create_file(&pdev->dev, &dev_attr_xhci_power);

	INIT_DELAYED_WORK(&xhci_work, amlogic_xhci_work);

	wake_lock_init(&xhci_wake_lock, WAKE_LOCK_SUSPEND,  "xhci");

	return 0;

put_usb3_hcd:
	usb_put_hcd(xhci->shared_hcd);

dealloc_usb2_hcd:
	usb_remove_hcd(hcd);

unmap_registers:
	iounmap(hcd->regs);

release_mem_region:
	release_mem_region(hcd->rsrc_start, hcd->rsrc_len);

put_hcd:
	usb_put_hcd(hcd);

	return ret;
}

static int xhci_plat_remove(struct platform_device *dev)
{
	struct usb_hcd	*hcd = platform_get_drvdata(dev);
	struct xhci_hcd	*xhci = hcd_to_xhci(hcd);

	usb_remove_hcd(xhci->shared_hcd);
	usb_put_hcd(xhci->shared_hcd);

	usb_remove_hcd(hcd);
	iounmap(hcd->regs);
	release_mem_region(hcd->rsrc_start, hcd->rsrc_len);
	usb_put_hcd(hcd);
	kfree(xhci);

	return 0;
}

#ifdef CONFIG_PM
static int xhci_plat_suspend(struct device *dev)
{
	struct usb_hcd	*hcd = dev_get_drvdata(dev);
	struct xhci_hcd	*xhci;

	if (!hcd)
		return -EBUSY;

	xhci = hcd_to_xhci(hcd);

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

static int xhci_plat_resume(struct device *dev)
{
	struct usb_hcd	*hcd = dev_get_drvdata(dev);
	struct xhci_hcd	*xhci;

	if (!hcd)
		return -EBUSY;

	xhci = hcd_to_xhci(hcd);

	return xhci_resume(xhci, 0);
}

static const struct dev_pm_ops xhci_plat_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(xhci_plat_suspend, xhci_plat_resume)
};
#define DEV_PM_OPS	(&xhci_plat_pm_ops)
#else
#define DEV_PM_OPS	NULL
#endif /* CONFIG_PM */

#ifdef CONFIG_OF
static const struct of_device_id usb_xhci_of_match[] = {
	{ .compatible = "xhci-platform" },
	{ },
};
MODULE_DEVICE_TABLE(of, usb_xhci_of_match);
#endif

static struct platform_driver usb_xhci_driver = {
	.probe	= xhci_plat_probe,
	.remove	= xhci_plat_remove,
	.driver	= {
		.name = "xhci-hcd",
		.pm = DEV_PM_OPS,
		.of_match_table = of_match_ptr(usb_xhci_of_match),
	},
};
MODULE_ALIAS("platform:xhci-hcd");

int xhci_register_plat(void)
{
	return platform_driver_register(&usb_xhci_driver);
}

void xhci_unregister_plat(void)
{
	platform_driver_unregister(&usb_xhci_driver);
}
