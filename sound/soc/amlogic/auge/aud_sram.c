// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/of.h>
#include <linux/io.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/debugfs.h>
#include <linux/regmap.h>
#include <linux/module.h>
#include <sound/memalloc.h>

#include "aud_sram.h"

#define DRV_NAME "aud_sram"

void __iomem *aud_sram_iomap;
struct resource *aud_sram_res;

void aud_buffer_force2sram(struct snd_dma_buffer *buf)
{
	buf->area = aud_sram_iomap;
	buf->bytes = resource_size(aud_sram_res);
	buf->addr = aud_sram_res->start;
}

static int aud_sram_iomap_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;
	struct resource *res;
	void __iomem *regs;
	struct regmap *map;
	struct regmap_config regmap_cfg = {
		.reg_bits	= 32,
		.val_bits	= 32,
		.reg_stride = 4,
	};

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	regs = devm_ioremap_resource(dev, res);
	if (IS_ERR(regs))
		return PTR_ERR(regs);

	aud_sram_res = res;
	aud_sram_iomap =
		ioremap_nocache(res->start, resource_size(res));
	pr_info("%s reg:%x, size:%x\n",
		__func__, (u32)res->start, (u32)resource_size(res));

	regmap_cfg.max_register = resource_size(res) - 4;
	regmap_cfg.name =
		devm_kasprintf(dev, GFP_KERNEL, "%s-%s", node->name, "sram");
	if (!regmap_cfg.name)
		return -ENOMEM;

	map = devm_regmap_init_mmio(dev, regs, &regmap_cfg);
	if (IS_ERR(map)) {
		dev_err(dev, "failed to init regmap: %ld\n",
			PTR_ERR(map));
		return PTR_ERR(map);
	}

	return 0;
}

static const struct of_device_id aud_sram_iomap_dt_match[] = {
	{ .compatible = "amlogic, audio-sram" },
	{}
};

struct platform_driver aud_sram_iomap_platform_driver = {
	.driver = {
		.owner    = THIS_MODULE,
		.name     = DRV_NAME,
		.of_match_table = aud_sram_iomap_dt_match,
	},
	.probe	 = aud_sram_iomap_probe,
};

int __init aud_sram_init(void)
{
	return platform_driver_register(&aud_sram_iomap_platform_driver);
}

void __exit aud_sram_exit(void)
{
	platform_driver_unregister(&aud_sram_iomap_platform_driver);
}

#ifndef MODULE
module_init(aud_sram_init);
module_exit(aud_sram_exit);
MODULE_AUTHOR("Amlogic, Inc.");
MODULE_DESCRIPTION("Amlogic TDM ASoc driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRV_NAME);
#endif

