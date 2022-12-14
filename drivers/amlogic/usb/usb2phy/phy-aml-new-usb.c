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
#include <linux/amlogic/usb-gxl.h>
#include <linux/amlogic/iomap.h>
#include "phy-aml-new-usb.h"

int amlogic_new_usbphy_reset(struct amlogic_usb *phy)
{
	static int	init_count;

	if (!init_count) {
		init_count++;
		writel((0x1 << 2), phy->reset_regs);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(amlogic_new_usbphy_reset);
