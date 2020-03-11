// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/err.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/reset-controller.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/of_device.h>
#include <linux/amlogic/media/registers/regs/dos_regs.h>

static const int reset_reg_addr[] = {
	DOS_SW_RESET0,
	DOS_SW_RESET1,
	DOS_SW_RESET2,
	DOS_SW_RESET3,
	DOS_SW_RESET4,
};

struct meson_dos_reset_desc {
	const int *resets;
	size_t num_resets;
};

static const struct meson_dos_reset_desc default_desc = {
	.resets = reset_reg_addr,
	.num_resets = ARRAY_SIZE(reset_reg_addr) * 32,
};

struct meson_dos_reset {
	struct reset_controller_dev rcdev;
	const struct meson_dos_reset_desc *desc;
	void __iomem *base;
	spinlock_t lock;
};

static inline struct meson_dos_reset *
to_reset_data(struct reset_controller_dev *rcdev)
{
	return container_of(rcdev, struct meson_dos_reset, rcdev);
}

static int meson_dos_reset_status(struct reset_controller_dev *rcdev,
				  ulong id)
{
	struct meson_dos_reset *data = to_reset_data(rcdev);
	const int *map = data->desc->resets;
	int index = id / 32, offset = id % 32;
	void __iomem *vaddr;
	ulong flags;
	u32 val;

	spin_lock_irqsave(&data->lock, flags);

	vaddr = data->base + (map[index] << 2) + offset;
	val = readl(vaddr);

	spin_unlock_irqrestore(&data->lock, flags);

	return val & BIT(offset);
}

static int meson_dos_reset_switch(struct reset_controller_dev *rcdev,
				  ulong id, bool assert)
{
	struct meson_dos_reset *data = to_reset_data(rcdev);
	const int *map = data->desc->resets;
	int index = id / 32, offset = id % 32;
	void __iomem *vaddr;
	ulong flags;
	u32 val;

	spin_lock_irqsave(&data->lock, flags);

	vaddr = data->base + (map[index] << 2) + offset;
	val = readl(vaddr);

	pr_debug("%s, ID: %d, va: %p, bit: %d, [%s]\n",
		__func__, index, vaddr, offset,
		assert ? "ON" : "OFF");

	if (assert)
		writel(val | BIT(offset), vaddr);
	else
		writel(val & ~BIT(offset), vaddr);

	spin_unlock_irqrestore(&data->lock, flags);

	return 0;
}

static int meson_dos_reset_assert(struct reset_controller_dev *rcdev,
				  ulong id)
{
	return meson_dos_reset_switch(rcdev, id, true);
}

static int meson_dos_reset_deassert(struct reset_controller_dev *rcdev,
				    ulong id)
{
	return meson_dos_reset_switch(rcdev, id, false);
}

static const struct reset_control_ops meson_dos_reset_ops = {
	.assert		= meson_dos_reset_assert,
	.deassert	= meson_dos_reset_deassert,
	.status		= meson_dos_reset_status,
};

static const struct of_device_id meson_dos_reset_dt_ids[] = {
	{ .compatible = "amlogic,meson-gxl-dos-reset",	.data = &default_desc},
	{ .compatible = "amlogic,meson-g12a-dos-reset",	.data = &default_desc},
	{ .compatible = "amlogic,meson-sm1-dos-reset",	.data = &default_desc},
	{ },
};

static int meson_dos_reset_probe(struct platform_device *pdev)
{
	struct meson_dos_reset *data;
	struct resource *res;

	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	data->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(data->base))
		return PTR_ERR(data->base);

	data->desc = of_device_get_match_data(&pdev->dev);
	if (!data->desc)
		return -ENODEV;

	platform_set_drvdata(pdev, data);

	spin_lock_init(&data->lock);

	data->rcdev.owner = THIS_MODULE;
	data->rcdev.nr_resets = data->desc->num_resets;
	data->rcdev.ops = &meson_dos_reset_ops;
	data->rcdev.of_node = pdev->dev.of_node;

	return devm_reset_controller_register(&pdev->dev, &data->rcdev);
}

static struct platform_driver meson_dos_reset_driver = {
	.probe	= meson_dos_reset_probe,
	.driver = {
		.name		= "meson_dos_reset",
		.of_match_table	= meson_dos_reset_dt_ids,
	},
};

builtin_platform_driver(meson_dos_reset_driver);
