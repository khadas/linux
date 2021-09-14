// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/of_gpio.h>
#include <linux/amlogic/usbtype.h>
#include <linux/reset.h>
#include <linux/platform_device.h>
#include <linux/amlogic/usb-v2.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>

/*
 * M chip USB clock setting
 */

struct clk_reset {
	struct clk *usb_reset_usb_general;
	struct clk *usb_reset_usb;
};

struct clk_reset crg_p_clk_reset[4];

/* ret 1: device plug in */
/* ret 0: device plug out */
int crg_device_status_v2(unsigned long usb_peri_reg)
{
	struct u2p_aml_regs_v2 u2p_aml_regs;
	union u2p_r1_v2 reg1;
	int ret = 1;

	u2p_aml_regs.u2p_r_v2[1] = (void __iomem	*)
				((unsigned long)usb_peri_reg + 0x4);
	reg1.d32 = readl(u2p_aml_regs.u2p_r_v2[1]);
	if (!reg1.b.OTGSESSVLD0)
		ret = 0;

	return ret;
}
EXPORT_SYMBOL(crg_device_status_v2);

static void crg_set_device_mode_v2(struct platform_device *pdev,
				unsigned long reg_addr, int controller_type)
{
	struct u2p_aml_regs_v2 u2p_aml_regs;
	union u2p_r0_v2 reg0;

	u2p_aml_regs.u2p_r_v2[0] = (void __iomem *)
				((unsigned long)reg_addr);

	reg0.d32 = readl(u2p_aml_regs.u2p_r_v2[0]);
	reg0.b.host_device = 0;
	reg0.b.POR = 0;
	writel(reg0.d32, u2p_aml_regs.u2p_r_v2[0]);
}

int crg_clk_enable_usb_v2(struct platform_device *pdev,
			unsigned long usb_peri_reg, int controller_type)
{
	struct clk *usb_reset;

	usb_reset = devm_clk_get(&pdev->dev, "usb_general");
	clk_prepare_enable(usb_reset);
	crg_p_clk_reset[pdev->id].usb_reset_usb_general = usb_reset;

	if (controller_type != USB_M31)
		crg_set_device_mode_v2(pdev, usb_peri_reg, controller_type);

	return 0;
}

void crg_clk_disable_usb_v2(struct platform_device *pdev,
				unsigned long usb_peri_reg)
{
	struct clk *usb_reset;

	usb_reset = devm_clk_get(&pdev->dev, "usb_general");
	if (IS_ERR_OR_NULL(usb_reset))
		return;

	clk_disable_unprepare(usb_reset);
}

int crg_clk_resume_usb_v2(struct platform_device *pdev,
			unsigned long usb_peri_reg)
{
	struct clk *usb_reset;

	if (pdev->id == 0) {
		usb_reset = crg_p_clk_reset[pdev->id].usb_reset_usb_general;
		clk_prepare_enable(usb_reset);
	} else if (pdev->id == 1) {
		usb_reset = crg_p_clk_reset[pdev->id].usb_reset_usb_general;
		clk_prepare_enable(usb_reset);
	} else {
		dev_err(&pdev->dev, "bad usb clk name.\n");
		return -1;
	}

	dmb(4);

	return 0;
}

int crg_clk_enable_usb(struct platform_device *pdev,
		unsigned long usb_peri_reg, int controller_type)
{
	int ret = 0;

	if (!pdev)
		return -1;

	ret = crg_clk_enable_usb_v2(pdev,
			usb_peri_reg, controller_type);

	/*add other cpu type's usb clock enable*/

	return ret;
}
EXPORT_SYMBOL(crg_clk_enable_usb);

int crg_clk_disable_usb(struct platform_device *pdev,
		unsigned long usb_peri_reg)
{
	if (!pdev)
		return -1;

	crg_clk_disable_usb_v2(pdev, usb_peri_reg);

	dmb(4);
	return 0;
}
EXPORT_SYMBOL(crg_clk_disable_usb);

int crg_clk_resume_usb(struct platform_device *pdev,
		unsigned long usb_peri_reg)
{
	int ret = 0;

	if (!pdev)
		return -1;

	ret = crg_clk_resume_usb_v2(pdev, usb_peri_reg);

	/*add other cpu type's usb clock enable*/

	return ret;
}
EXPORT_SYMBOL(crg_clk_resume_usb);

int crg_clk_suspend_usb(struct platform_device *pdev,
		unsigned long usb_peri_reg)
{
	if (!pdev)
		return -1;

	crg_clk_disable_usb_v2(pdev, usb_peri_reg);

	dmb(4);
	return 0;
}
EXPORT_SYMBOL(crg_clk_suspend_usb);
