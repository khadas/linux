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
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/workqueue.h>
#include <linux/notifier.h>
#include <linux/amlogic/usbtype.h>
#include "../phy/phy-aml-new-usb-v2.h"

struct usb_aml_regs_v2 usb_crg_drd_aml_regs[2];

static int amlogic_crg_drd_usb3_suspend(struct usb_phy *x, int suspend)
{
	return 0;
}

static void amlogic_crg_drd_usb3phy_shutdown(struct usb_phy *x)
{
	struct amlogic_usb_v2 *phy = phy_to_amlusb(x);

	if (phy->phy.flags == AML_USB3_PHY_ENABLE) {
		if (!(IS_ERR(phy->hcsl_clk)))
			clk_disable_unprepare(phy->hcsl_clk);

		clk_disable_unprepare(phy->clk);

		writel(0xf5, phy->phy31_cfg);
	}

	phy->suspend_flag = 1;
}

static void cr_bus_addr(struct amlogic_usb_v2 *phy_v3, unsigned int addr)
{
	union phy3_r4 phy_r4 = {.d32 = 0};
	union phy3_r5 phy_r5 = {.d32 = 0};
	unsigned long timeout_jiffies;

	phy_r4.b.phy_cr_data_in = addr;
	writel(phy_r4.d32, phy_v3->phy3_cfg_r4);

	phy_r4.b.phy_cr_cap_addr = 0;
	writel(phy_r4.d32, phy_v3->phy3_cfg_r4);
	phy_r4.b.phy_cr_cap_addr = 1;
	writel(phy_r4.d32, phy_v3->phy3_cfg_r4);
	timeout_jiffies = jiffies +
			msecs_to_jiffies(1000);
	do {
		phy_r5.d32 = readl(phy_v3->phy3_cfg_r5);
	} while (phy_r5.b.phy_cr_ack == 0 &&
		time_is_after_jiffies(timeout_jiffies));

	phy_r4.b.phy_cr_cap_addr = 0;
	writel(phy_r4.d32, phy_v3->phy3_cfg_r4);
	timeout_jiffies = jiffies +
			msecs_to_jiffies(1000);
	do {
		phy_r5.d32 = readl(phy_v3->phy3_cfg_r5);
	} while (phy_r5.b.phy_cr_ack == 1 &&
		time_is_after_jiffies(timeout_jiffies));
}

static int cr_bus_read(struct amlogic_usb_v2 *phy_v3, unsigned int addr)
{
	int data;
	union phy3_r4 phy_r4 = {.d32 = 0};
	union phy3_r5 phy_r5 = {.d32 = 0};
	unsigned long timeout_jiffies;

	cr_bus_addr(phy_v3, addr);

	phy_r4.b.phy_cr_read = 0;
	writel(phy_r4.d32, phy_v3->phy3_cfg_r4);
	phy_r4.b.phy_cr_read = 1;
	writel(phy_r4.d32, phy_v3->phy3_cfg_r4);

	timeout_jiffies = jiffies +
			msecs_to_jiffies(1000);
	do {
		phy_r5.d32 = readl(phy_v3->phy3_cfg_r5);
	} while (phy_r5.b.phy_cr_ack == 0 &&
		time_is_after_jiffies(timeout_jiffies));

	data = phy_r5.b.phy_cr_data_out;

	phy_r4.b.phy_cr_read = 0;
	writel(phy_r4.d32, phy_v3->phy3_cfg_r4);
	timeout_jiffies = jiffies +
			msecs_to_jiffies(1000);
	do {
		phy_r5.d32 = readl(phy_v3->phy3_cfg_r5);
	} while (phy_r5.b.phy_cr_ack == 1 &&
		time_is_after_jiffies(timeout_jiffies));

	return data;
}

static void cr_bus_write(struct amlogic_usb_v2 *phy_v3,
			 unsigned int addr, unsigned int data)
{
	union phy3_r4 phy_r4 = {.d32 = 0};
	union phy3_r5 phy_r5 = {.d32 = 0};
	unsigned long timeout_jiffies;

	cr_bus_addr(phy_v3, addr);

	phy_r4.b.phy_cr_data_in = data;
	writel(phy_r4.d32, phy_v3->phy3_cfg_r4);

	phy_r4.b.phy_cr_cap_data = 0;
	writel(phy_r4.d32, phy_v3->phy3_cfg_r4);
	phy_r4.b.phy_cr_cap_data = 1;
	writel(phy_r4.d32, phy_v3->phy3_cfg_r4);
	timeout_jiffies = jiffies +
		msecs_to_jiffies(1000);
	do {
		phy_r5.d32 = readl(phy_v3->phy3_cfg_r5);
	} while (phy_r5.b.phy_cr_ack == 0 &&
		time_is_after_jiffies(timeout_jiffies));

	phy_r4.b.phy_cr_cap_data = 0;
	writel(phy_r4.d32, phy_v3->phy3_cfg_r4);
	timeout_jiffies = jiffies +
		msecs_to_jiffies(1000);
	do {
		phy_r5.d32 = readl(phy_v3->phy3_cfg_r5);
	} while (phy_r5.b.phy_cr_ack == 1 &&
		time_is_after_jiffies(timeout_jiffies));

	phy_r4.b.phy_cr_write = 0;
	writel(phy_r4.d32, phy_v3->phy3_cfg_r4);
	phy_r4.b.phy_cr_write = 1;
	writel(phy_r4.d32, phy_v3->phy3_cfg_r4);
	timeout_jiffies = jiffies +
		msecs_to_jiffies(1000);
	do {
		phy_r5.d32 = readl(phy_v3->phy3_cfg_r5);
	} while (phy_r5.b.phy_cr_ack == 0 &&
		time_is_after_jiffies(timeout_jiffies));

	phy_r4.b.phy_cr_write = 0;
	writel(phy_r4.d32, phy_v3->phy3_cfg_r4);
	timeout_jiffies = jiffies +
		msecs_to_jiffies(1000);
	do {
		phy_r5.d32 = readl(phy_v3->phy3_cfg_r5);
	} while (phy_r5.b.phy_cr_ack == 1 &&
		time_is_after_jiffies(timeout_jiffies));
}

static void usb3_phy_cr_config(struct amlogic_usb_v2 *phy)
{
	u32 data = 0;

	/*
	 * WORKAROUND: There is SSPHY suspend bug due to
	 * which USB enumerates
	 * in HS mode instead of SS mode. Workaround it by asserting
	 * LANE0.TX_ALT_BLOCK.EN_ALT_BUS to enable TX to use alt bus
	 * mode
	 */
	data = cr_bus_read(phy, 0x102d);
	data |= (1 << 7);
	cr_bus_write(phy, 0x102D, data);

	data = cr_bus_read(phy, 0x1010);
	data &= ~0xff0;
	data |= 0x20;
	cr_bus_write(phy, 0x1010, data);

	/*
	 * Fix RX Equalization setting as follows
	 * LANE0.RX_OVRD_IN_HI. RX_EQ_EN set to 0
	 * LANE0.RX_OVRD_IN_HI.RX_EQ_EN_OVRD set to 1
	 * LANE0.RX_OVRD_IN_HI.RX_EQ set to 3
	 * LANE0.RX_OVRD_IN_HI.RX_EQ_OVRD set to 1
	 */
	data = cr_bus_read(phy, 0x1006);
	data &= ~(1 << 6);
	data |= (1 << 7);
	data &= ~(0x7 << 8);
	data |= (0x3 << 8);
	data |= (0x1 << 11);
	cr_bus_write(phy, 0x1006, data);

	/*
	 * S	et EQ and TX launch amplitudes as follows
	 * LANE0.TX_OVRD_DRV_LO.PREEMPH set to 22
	 * LANE0.TX_OVRD_DRV_LO.AMPLITUDE set to 127
	 * LANE0.TX_OVRD_DRV_LO.EN set to 1.
	 */
	data = cr_bus_read(phy, 0x1002);
	data &= ~0x3f80;
	data |= (0x16 << 7);
	data &= ~0x7f;
	data |= (0x7f | (1 << 14));
	cr_bus_write(phy, 0x1002, data);

	/*
	 * MPLL_LOOP_CTL.PROP_CNTRL
	 */
	data = cr_bus_read(phy, 0x30);
	data &= ~(0xf << 4);
	data |= (0x8 << 4);
	cr_bus_write(phy, 0x30, data);
	udelay(2);
}

static int amlogic_crg_drd_usb3_init(struct usb_phy *x)
{
	struct amlogic_usb_v2 *phy = phy_to_amlusb(x);
	union usb_r3_v2 r3 = {.d32 = 0};
	union usb_r7_v2 r7 = {.d32 = 0};
	union phy3_r2 p3_r2 = {.d32 = 0};
	union phy3_r1 p3_r1 = {.d32 = 0};
	int i = 0;
	u32 val;

	if (phy->suspend_flag) {
		if (phy->phy.flags == AML_USB3_PHY_ENABLE) {
			clk_prepare_enable(phy->clk);
			if (!(IS_ERR(phy->hcsl_clk)))
				clk_prepare_enable(phy->hcsl_clk);

			val = readl(phy->phy31_cfg);
			val |= (3 << 5);
			val &= (~(1));
			writel(val, phy->phy31_cfg);
		}

		phy->suspend_flag = 0;
		return 0;
	}

	if (phy->phy.flags != AML_USB3_PHY_ENABLE)
		return 0;

	for (i = 0; i < 8; i++)
		usb_crg_drd_aml_regs[phy->phy_id].usb_r_v2[i] = (void __iomem *)
			((unsigned long)phy->regs + 4 * i);

	if (phy->phy.flags == AML_USB3_PHY_ENABLE) {
		r3.d32 = readl(usb_crg_drd_aml_regs[phy->phy_id].usb_r_v2[3]);
		r3.b.p30_ref_ssp_en = 1;
		writel(r3.d32, usb_crg_drd_aml_regs[phy->phy_id].usb_r_v2[3]);
		udelay(2);
		r7.d32 = readl(usb_crg_drd_aml_regs[phy->phy_id].usb_r_v2[7]);
		r7.b.p31_ssc_en = 1;
		r7.b.p31_ssc_range = 2;
		writel(r7.d32, usb_crg_drd_aml_regs[phy->phy_id].usb_r_v2[7]);
		udelay(2);
		r7.d32 = readl(usb_crg_drd_aml_regs[phy->phy_id].usb_r_v2[7]);
		r7.b.p31_pcs_tx_deemph_6db = 0x20;
		r7.b.p31_pcs_tx_swing_full = 127;
		writel(r7.d32, usb_crg_drd_aml_regs[phy->phy_id].usb_r_v2[7]);
		udelay(2);
		r3.d32 = readl(usb_crg_drd_aml_regs[phy->phy_id].usb_r_v2[3]);
		r3.b.p31_pcs_tx_deemph_3p5db = 0x15;
		writel(r3.d32, usb_crg_drd_aml_regs[phy->phy_id].usb_r_v2[3]);

		udelay(2);
		p3_r2.d32 = readl(phy->phy3_cfg_r2);
		p3_r2.b.phy_tx_vboost_lvl = 0x4;
		writel(p3_r2.d32, phy->phy3_cfg_r2);
		udelay(2);

		usb3_phy_cr_config(phy);

		/*
		 * LOS_BIAS to 0x5
		 * LOS_LEVEL to 0x9
		 */
		p3_r1.d32 = readl(phy->phy3_cfg_r1);
		p3_r1.b.phy_los_bias = 0x5;
		p3_r1.b.phy_los_level = 0x9;
		writel(p3_r1.d32, phy->phy3_cfg_r1);
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

static int amlogic_crg_drd_usb3_probe(struct platform_device *pdev)
{
	struct amlogic_usb_v2			*phy;
	struct device *dev = &pdev->dev;
	struct resource *phy_mem;
	void __iomem	*phy_base;
	void __iomem *phy31_base;
	void __iomem *phy_pcie_base;
	void __iomem	*reset_base = NULL;
	unsigned int phy31_mem;
	unsigned int phy31_mem_size = 0;
	unsigned int phy_pcie_mem;
	unsigned int phy_pcie_mem_size = 0;
	unsigned int reset_mem;
	unsigned int reset_mem_size = 0;
	const void *prop;
	int portnum = 0;
	int retval;
	int ret;
	unsigned long rate;
#define PCIE_PLL_RATE 100000000
	u32 val;
	u32 phy_id = 0;
	int reset_level = 0x40;
	u32 usb3_apb_reset_bit = 23;
	u32 usb3_phy_reset_bit = 22;
	u32 usb3_reset_shift = 4;
	struct device_node *tsi_pci;

	prop = of_get_property(dev->of_node, "portnum", NULL);
	if (prop)
		portnum = of_read_ulong(prop, 1);

	if (!portnum)
		dev_err(&pdev->dev, "This phy has no usb port\n");

	prop = of_get_property(dev->of_node, "phy-id", NULL);
	if (prop)
		phy_id = of_read_ulong(prop, 1);
	else
		phy_id = 0;

	phy_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	phy_base = ioremap(phy_mem->start, resource_size(phy_mem));
	if (IS_ERR(phy_base))
		return PTR_ERR(phy_base);

	retval = of_property_read_u32(dev->of_node, "phy1-reg", &phy31_mem);
	if (retval < 0)
		return -EINVAL;

	retval = of_property_read_u32
				(dev->of_node, "phy1-reg-size",
					&phy31_mem_size);
	if (retval < 0)
		return -EINVAL;

	phy31_base = ioremap((resource_size_t)phy31_mem,
					(unsigned long)phy31_mem_size);
	if (!phy31_base)
		return -ENOMEM;

	retval = of_property_read_u32(dev->of_node, "reset-reg", &reset_mem);
	if (retval < 0)
		return -EINVAL;

	retval = of_property_read_u32
			(dev->of_node, "reset-reg-size",
				&reset_mem_size);
	if (retval < 0)
		return -EINVAL;

	reset_base = ioremap((resource_size_t)reset_mem,
					(unsigned long)reset_mem_size);
	if (!reset_base)
		return -ENOMEM;

	prop = of_get_property(dev->of_node, "reset-level", NULL);
	if (prop)
		reset_level = of_read_ulong(prop, 1);
	else
		reset_level = 0x40;

	prop = of_get_property(dev->of_node, "usb3-apb-reset-bit", NULL);
	if (prop)
		usb3_apb_reset_bit = of_read_ulong(prop, 1);
	else
		usb3_apb_reset_bit = 23;

	prop = of_get_property(dev->of_node, "usb3-phy-reset-bit", NULL);
	if (prop)
		usb3_phy_reset_bit = of_read_ulong(prop, 1);
	else
		usb3_phy_reset_bit = 22;

	prop = of_get_property(dev->of_node, "usb3-reset-shit", NULL);
	if (prop)
		usb3_reset_shift = of_read_ulong(prop, 1);
	else
		usb3_reset_shift = 4;

	phy = devm_kzalloc(&pdev->dev, sizeof(*phy), GFP_KERNEL);
	if (!phy)
		return -ENOMEM;

	dev_info(&pdev->dev, "USB3 phy probe:phy_mem:0x%lx, iomap phy_base:0x%lx\n",
		 (unsigned long)phy_mem->start, (unsigned long)phy_base);

	phy->dev		= dev;
	phy->regs		= phy_base;
	phy->phy31_cfg	= phy31_base;
	phy->phy3_cfg_r1 = (void __iomem *)
			((unsigned long)phy->phy31_cfg + 4 * 1);
	phy->phy3_cfg_r2 = (void __iomem *)
			((unsigned long)phy->phy31_cfg + 4 * 2);
	phy->phy3_cfg_r4 = (void __iomem *)
			((unsigned long)phy->phy31_cfg + 4 * 4);
	phy->phy3_cfg_r5 = (void __iomem *)
			((unsigned long)phy->phy31_cfg + 4 * 5);
	phy->portnum      = portnum;
	phy->suspend_flag = 0;
	phy->phy.dev		= phy->dev;
	phy->phy.label		= "amlogic-crg-drd-usbphy3";
	phy->phy.init		= amlogic_crg_drd_usb3_init;
	phy->phy.set_suspend	= amlogic_crg_drd_usb3_suspend;
	phy->phy.shutdown	= amlogic_crg_drd_usb3phy_shutdown;
	phy->phy.type		= USB_PHY_TYPE_USB3;
	phy->phy.flags		= AML_USB3_PHY_DISABLE;
	phy->reset_regs = reset_base;
	phy->phy_id = phy_id;
	phy->reset_level = reset_level;
	phy->usb3_apb_reset_bit = usb3_apb_reset_bit;
	phy->usb3_phy_reset_bit = usb3_phy_reset_bit;
	phy->usb3_reset_shift = usb3_reset_shift;

	/* set the phy from pcie to usb3 */
	if (phy->portnum > 0) {
		retval = of_property_read_u32(dev->of_node,
			 "phy-pcie-reg", &phy_pcie_mem);
		if (retval < 0)
			return -EINVAL;

		retval = of_property_read_u32
					(dev->of_node, "phy-pcie-reg-size",
						&phy_pcie_mem_size);
		if (retval < 0)
			return -EINVAL;

		phy_pcie_base = ioremap((resource_size_t)phy_pcie_mem,
				(unsigned long)phy_pcie_mem_size);
		if (!phy_pcie_base)
			return -ENOMEM;

		tsi_pci = of_find_node_by_type(NULL, "pci");
		if (tsi_pci) {
			if (!device_is_available(tsi_pci)) {
				dev_info(&pdev->dev, "no pci-e driver!,power down pcie phy\n");
				writel(0x1d, phy_pcie_base);
			}
		}

		writel((0x1 << usb3_apb_reset_bit) | (0x1 << usb3_phy_reset_bit),
			(void __iomem *)
			((unsigned long)phy->reset_regs + phy->usb3_reset_shift));

		usleep_range(100, 200);

		val = readl(phy->reset_regs + phy->reset_level + phy->usb3_reset_shift);
		writel(val | (0x1 << usb3_apb_reset_bit) | (0x1 << usb3_phy_reset_bit),
			(void __iomem *)
			((unsigned long)phy->reset_regs + phy->reset_level
			+ phy->usb3_reset_shift));

		usleep_range(100, 200);

		val = readl(phy->phy31_cfg);
		val |= (3 << 5);
		writel(val, phy->phy31_cfg);

		usleep_range(100, 200);

		val = readl(phy->phy31_cfg + 0x18);
		val &= (~(0x3 << 17));
		val |= (0x1 << 18);
		writel(val, phy->phy31_cfg + 0x18);

		usleep_range(100, 200);

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

		rate = clk_get_rate(phy->clk);
		if (rate != PCIE_PLL_RATE)
			dev_err(dev, "pcie_refpll is not 100M, it is %ld\n",
				rate);

		phy->hcsl_clk = devm_clk_get(dev, "pcie_hcsl");
		if (IS_ERR(phy->hcsl_clk)) {
			dev_dbg(dev, "Failed to get usb3 hcsl clock\n");
			ret = PTR_ERR(phy->hcsl_clk);
		} else {
			ret = clk_prepare_enable(phy->hcsl_clk);
			if (ret) {
				dev_err(dev, "Failed to enable usb3 hcsl clock\n");
				ret = PTR_ERR(phy->hcsl_clk);
				return ret;
			}
		}

		phy->phy.flags = AML_USB3_PHY_ENABLE;
	}

	usb_add_phy_dev(&phy->phy);

	platform_set_drvdata(pdev, phy);

	pm_runtime_enable(phy->dev);

	amlogic_crg_drd_usb3_init(&phy->phy);

	return 0;
}

static int amlogic_crg_drd_usb3_remove(struct platform_device *pdev)
{
	return 0;
}

#ifdef CONFIG_PM_RUNTIME

static int amlogic_crg_drd_usb3_runtime_suspend(struct device *dev)
{
	return 0;
}

static int amlogic_crg_drd_usb3_runtime_resume(struct device *dev)
{
	u32 ret = 0;

	return ret;
}

static const struct dev_pm_ops amlogic_crg_drd_usb3_pm_ops = {
	SET_RUNTIME_PM_OPS(amlogic_crg_drd_usb3_runtime_suspend,
			   amlogic_crg_drd_usb3_runtime_resume,
			   NULL)
};

#define DEV_PM_OPS     (&amlogic_crg_drd_usb3_pm_ops)
#else
#define DEV_PM_OPS     NULL
#endif

#ifdef CONFIG_OF
static const struct of_device_id amlogic_crg_drd_usb3_id_table[] = {
	{ .compatible = "amlogic, amlogic-crg-drd-usb3" },
	{}
};
MODULE_DEVICE_TABLE(of, amlogic_crg_drd_usb3_id_table);
#endif

static struct platform_driver amlogic_crg_drd_usb3_driver = {
	.probe		= amlogic_crg_drd_usb3_probe,
	.remove		= amlogic_crg_drd_usb3_remove,
	.driver		= {
		.name	= "amlogic-crg-drd-usb3",
		.owner	= THIS_MODULE,
		.pm	= DEV_PM_OPS,
		.of_match_table = of_match_ptr(amlogic_crg_drd_usb3_id_table),
	},
};

module_platform_driver(amlogic_crg_drd_usb3_driver);

MODULE_ALIAS("platform: amlogic_cgc_drd_usb3");
MODULE_AUTHOR("Amlogic Inc.");
MODULE_DESCRIPTION("amlogic crg drd USB3 phy driver");
MODULE_LICENSE("GPL v2");
