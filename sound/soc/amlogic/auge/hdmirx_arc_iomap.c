// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

/*#define DEBUG*/

#include <linux/of.h>
#include <linux/io.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/regmap.h>

#include "../common/iomapres.h"

#include "regs.h"

#define DEV_NAME	"hdmirx_arc_iomap"

struct regmap *hdmirx_arc_map;

void hdmirx_arc_write_reg(int reg, int value)
{
	pr_info("%s reg 0x%x, value 0x%x\n", __func__, reg, value);
	mmio_write(hdmirx_arc_map, reg, value);
}

void hdmirx_arc_update_reg(int reg, int mask, int value)
{
	pr_info("%s reg 0x%x, mask 0x%x, value 0x%x\n", __func__, reg, mask, value);
	mmio_update_bits(hdmirx_arc_map, reg, mask, value);
}

static int hdmirx_arc_iomap_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	hdmirx_arc_map = regmap_resource(dev, "hdmirx-arc");
	if (!hdmirx_arc_map) {
		dev_err(dev, "Can't get hdmirx arc regmap!!\n");
		return PTR_ERR(hdmirx_arc_map);
	}

	return 0;
}

static const struct of_device_id hdmirx_arc_iomap_dt_match[] = {
	{ .compatible = "amlogic, hdmirx-arc-iomap" },
	{},
};

static  struct platform_driver hdmirx_arc_iomap_platform_driver = {
	.probe		= hdmirx_arc_iomap_probe,
	.driver		= {
		.owner		    = THIS_MODULE,
		.name		    = DEV_NAME,
		.of_match_table	= hdmirx_arc_iomap_dt_match,
	},
};

int __init auge_hdmirx_arc_iomap_init(void)
{
	return platform_driver_register(&hdmirx_arc_iomap_platform_driver);
}

void __exit auge_hdmirx_arc_iomap_exit(void)
{
	platform_driver_unregister(&hdmirx_arc_iomap_platform_driver);
}

#ifndef MODULE
core_initcall(auge_hdmirx_arc_iomap_init);
module_exit(auge_hdmirx_arc_iomap_exit);
#endif
