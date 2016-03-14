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
#include "phy-aml-gxl-usb.h"

#define HOST_MODE	0
#define DEVICE_MODE	1

struct usb_aml_regs_t usb_gxl_aml_regs;

static int amlogic_gxl_usb3_suspend(struct usb_phy *x, int suspend)
{
	return 0;
}

static int amlogic_gxl_usb3_init(struct usb_phy *x)
{
	struct amlogic_usb *phy = phy_to_amlusb(x);

	union usb_r0_t r0 = {.d32 = 0};
	union usb_r1_t r1 = {.d32 = 0};
	union usb_r5_t r5 = {.d32 = 0};
	int i, j;

	amlogic_gxl_usbphy_reset();

	for (i = 0; i < phy->portnum; i++) {
		for (j = 0; j < 7; j++) {
			usb_gxl_aml_regs.usb_r[j] = (void __iomem *)
				((unsigned long)phy->regs + i*PHY_REGISTER_SIZE
				+ 4*j);
		}

		r0.d32 = readl(usb_gxl_aml_regs.usb_r[0]);
		r0.b.p30_phy_reset = 1;
		writel(r0.d32, usb_gxl_aml_regs.usb_r[0]);

		udelay(2);

		r0.b.p30_phy_reset = 0;
		writel(r0.d32, usb_gxl_aml_regs.usb_r[0]);

		r1.d32 = readl(usb_gxl_aml_regs.usb_r[1]);
		r1.b.u3h_host_u2_port_disable = 0xc;
		/*r1.b.u3h_host_u3_port_disable = 0x1;*/
		writel(r1.d32, usb_gxl_aml_regs.usb_r[1]);

		r5.d32 = readl(usb_gxl_aml_regs.usb_r[5]);
		r5.b.iddig_en0 = 1;
		r5.b.iddig_en1 = 1;
		writel(r5.d32, usb_gxl_aml_regs.usb_r[5]);
	}

	return 0;
}

static void set_mode(unsigned long reg_addr, int mode)
{
	struct u2p_aml_regs_t u2p_aml_regs;
	struct usb_aml_regs_t usb_gxl_aml_regs;
	union u2p_r0_t reg0;
	union usb_r0_t r0 = {.d32 = 0};
	union usb_r1_t r1 = {.d32 = 0};
	union usb_r4_t r4 = {.d32 = 0};

	u2p_aml_regs.u2p_r[0] = (void __iomem	*)
				((unsigned long)reg_addr + PHY_REGISTER_SIZE);
	reg0.d32 = readl(u2p_aml_regs.u2p_r[0]);
	if (mode == DEVICE_MODE) {
		reg0.b.dmpulldown = 0;
		reg0.b.dppulldown = 0;
	} else {
		reg0.b.dmpulldown = 1;
		reg0.b.dppulldown = 1;
	}
	writel(reg0.d32, u2p_aml_regs.u2p_r[0]);

	usb_gxl_aml_regs.usb_r[0] = (void __iomem *)
				((unsigned long)reg_addr + 4*PHY_REGISTER_SIZE
				+ 4*0);
	usb_gxl_aml_regs.usb_r[1] = (void __iomem *)
				((unsigned long)reg_addr + 4*PHY_REGISTER_SIZE
				+ 4*1);
	usb_gxl_aml_regs.usb_r[4] = (void __iomem *)
				((unsigned long)reg_addr + 4*PHY_REGISTER_SIZE
				+ 4*4);
	r0.d32 = readl(usb_gxl_aml_regs.usb_r[0]);
	if (mode == DEVICE_MODE)
		r0.b.u2d_act = 1;
	else
		r0.b.u2d_act = 0;

	writel(r0.d32, usb_gxl_aml_regs.usb_r[0]);

	r4.d32 = readl(usb_gxl_aml_regs.usb_r[4]);
	if (mode == DEVICE_MODE)
		r4.b.p21_SLEEPM0 = 0x1;
	else
		r4.b.p21_SLEEPM0 = 0x0;

	writel(r4.d32, usb_gxl_aml_regs.usb_r[4]);

	r1.d32 = readl(usb_gxl_aml_regs.usb_r[1]);
	if (mode == DEVICE_MODE)
		r1.b.u3h_host_u2_port_disable = 0xE;
	else
		r1.b.u3h_host_u2_port_disable = 0xC;

	writel(r1.d32, usb_gxl_aml_regs.usb_r[1]);
}

static irqreturn_t amlogic_botg_detect_irq(int irq, void *dev)
{
	struct amlogic_usb *phy = (struct amlogic_usb *)dev;

	union usb_r5_t r5 = {.d32 = 0};
	unsigned long reg_addr = ((unsigned long)phy->regs - 0x80);

	r5.d32 = readl(usb_gxl_aml_regs.usb_r[5]);
	if (0 == r5.b.iddig_curr)
		set_mode(reg_addr, DEVICE_MODE);
	else
		set_mode(reg_addr, HOST_MODE);
	r5.b.iddig_irq = 0;
	writel(r5.d32, usb_gxl_aml_regs.usb_r[5]);

	return IRQ_HANDLED;
}

static int amlogic_gxl_usb3_probe(struct platform_device *pdev)
{
	struct amlogic_usb			*phy;
	struct device *dev = &pdev->dev;
	struct resource *phy_mem;
	void __iomem	*phy_base;
	const void *prop;
	int portnum = 0;
	int irq;
	int retval;

	prop = of_get_property(dev->of_node, "portnum", NULL);
	if (prop)
		portnum = of_read_ulong(prop, 1);

	if (!portnum) {
		dev_err(&pdev->dev, "This phy has no usb port\n");
		return -ENOMEM;
	}
	phy_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	phy_base = devm_ioremap_resource(dev, phy_mem);
	if (IS_ERR(phy_base))
		return PTR_ERR(phy_base);
	phy = devm_kzalloc(&pdev->dev, sizeof(*phy), GFP_KERNEL);
	if (!phy) {
		dev_err(&pdev->dev, "unable to allocate memory for USB3 PHY\n");
		return -ENOMEM;
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return -ENODEV;

	dev_dbg(&pdev->dev, "registering (common) handler for irq%d\n", irq);

	retval = request_irq(irq, amlogic_botg_detect_irq,
				IRQF_SHARED | IRQF_DISABLED | IRQ_LEVEL,
				"amlogic_botg_detect", phy);

	if (retval) {
		dev_err(&pdev->dev, "request of irq%d failed\n", irq);
		retval = -EBUSY;
		return retval;
	}

	dev_info(&pdev->dev, "USB3 phy probe:phy_mem:0x%lx, iomap phy_base:0x%lx\n",
			(unsigned long)phy_mem->start, (unsigned long)phy_base);

	phy->dev		= dev;
	phy->regs		= phy_base;
	phy->portnum      = portnum;
	phy->phy.dev		= phy->dev;
	phy->phy.label		= "amlogic-usbphy2";
	phy->phy.init		= amlogic_gxl_usb3_init;
	phy->phy.set_suspend	= amlogic_gxl_usb3_suspend;
	phy->phy.type		= USB_PHY_TYPE_USB3;

	usb_add_phy_dev(&phy->phy);

	platform_set_drvdata(pdev, phy);

	pm_runtime_enable(phy->dev);

	return 0;
}

static int amlogic_gxl_usb3_remove(struct platform_device *pdev)
{
	return 0;
}

#ifdef CONFIG_PM_RUNTIME

static int amlogic_gxl_usb3_runtime_suspend(struct device *dev)
{
	return 0;
}

static int amlogic_gxl_usb3_runtime_resume(struct device *dev)
{
	u32 ret = 0;

	return ret;
}

static const struct dev_pm_ops amlogic_gxl_usb3_pm_ops = {
	SET_RUNTIME_PM_OPS(amlogic_gxl_usb3_runtime_suspend,
		amlogic_gxl_usb3_runtime_resume,
		NULL)
};

#define DEV_PM_OPS     (&amlogic_gxl_usb3_pm_ops)
#else
#define DEV_PM_OPS     NULL
#endif

#ifdef CONFIG_OF
static const struct of_device_id amlogic_gxl_usb3_id_table[] = {
	{ .compatible = "amlogic, amlogic-gxl-usb3" },
	{}
};
MODULE_DEVICE_TABLE(of, amlogic_gxl_usb3_id_table);
#endif

static struct platform_driver amlogic_gxl_usb3_driver = {
	.probe		= amlogic_gxl_usb3_probe,
	.remove		= amlogic_gxl_usb3_remove,
	.driver		= {
		.name	= "amlogic-gxl-usb3",
		.owner	= THIS_MODULE,
		.pm	= DEV_PM_OPS,
		.of_match_table = of_match_ptr(amlogic_gxl_usb3_id_table),
	},
};

module_platform_driver(amlogic_gxl_usb3_driver);

MODULE_ALIAS("platform: amlogic_usb3");
MODULE_AUTHOR("Amlogic Inc.");
MODULE_DESCRIPTION("amlogic USB3 phy driver");
MODULE_LICENSE("GPL v2");
