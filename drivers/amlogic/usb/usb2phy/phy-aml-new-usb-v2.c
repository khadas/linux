// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/amlogic/usb-v2.h>
#include "../phy/phy-aml-new-usb-v2.h"

int amlogic_new_usbphy_reset_v2(struct amlogic_usb_v2 *phy)
{
	static int	init_count;

	if (!init_count) {
		init_count++;
		if (phy->usb_reset_bit == 2)
			writel((readl(phy->reset_regs) |
				(0x1 << phy->usb_reset_bit)), phy->reset_regs);
		else
			writel((0x1 << phy->usb_reset_bit), phy->reset_regs);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(amlogic_new_usbphy_reset_v2);

int amlogic_new_usbphy_reset_phycfg_v2(struct amlogic_usb_v2 *phy, int cnt)
{
	u32 val, i = 0;
	u32 temp = 0;
	size_t mask = 0;

	mask = (size_t)phy->reset_regs & 0xf;

	for (i = 0; i < cnt; i++)
		temp = temp | (1 << phy->phy_reset_level_bit[i]);

	/* set usb phy to low power mode */
	val = readl((void __iomem		*)
		((unsigned long)phy->reset_regs + (phy->reset_level - mask)));
	writel((val & (~temp)), (void __iomem	*)
		((unsigned long)phy->reset_regs + (phy->reset_level - mask)));

	udelay(100);

	val = readl((void __iomem *)
		((unsigned long)phy->reset_regs + (phy->reset_level - mask)));
	writel((val | temp), (void __iomem *)
		((unsigned long)phy->reset_regs + (phy->reset_level - mask)));

	return 0;
}
EXPORT_SYMBOL_GPL(amlogic_new_usbphy_reset_phycfg_v2);
