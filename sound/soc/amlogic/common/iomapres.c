// SPDX-License-Identifier: GPL-2.0
/*
 * iomapres driver
 *
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/device.h>

#include "iomapres.h"

struct regmap *regmap_resource(struct device *dev, char *name)
{
	struct resource res;
	void __iomem *base;
	struct device_node *node = dev->of_node;
	static struct regmap_config aud_regmap_config = {
		.reg_bits = 32,
		.val_bits = 32,
		.reg_stride = 4,
	};
	int i;

	i = of_property_match_string(node, "reg-names", name);
	if (of_address_to_resource(node, i, &res))
		return ERR_PTR(-ENOENT);

	base = devm_ioremap_resource(dev, &res);
	if (IS_ERR(base))
		return ERR_CAST(base);

	pr_debug("%s, %s, start:%#x, size:%#x\n",
		__func__, name, (u32)res.start, (u32)resource_size(&res));

	aud_regmap_config.max_register = resource_size(&res) - 4;
	aud_regmap_config.name =
		devm_kasprintf(dev, GFP_KERNEL, "%s-%s", node->name, name);
	if (!aud_regmap_config.name)
		return ERR_PTR(-ENOMEM);

	return devm_regmap_init_mmio(dev, base, &aud_regmap_config);
}

unsigned int mmio_read(struct regmap *map, unsigned int reg_ofs)
{
	unsigned int val;

	regmap_read(map, (reg_ofs << 2), &val);

	return val;
}

int mmio_write(struct regmap *map, unsigned int reg_ofs, unsigned int value)
{
	return regmap_write(map, (reg_ofs << 2), value);
}

int mmio_update_bits(struct regmap *map,
		     unsigned int reg_ofs,
		     unsigned int mask,
		     unsigned int value)
{
	return regmap_update_bits(map, (reg_ofs << 2), mask, value);
}
