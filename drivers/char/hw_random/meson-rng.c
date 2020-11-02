// SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause
/*
 * Copyright (c) 2016 BayLibre, SAS.
 * Author: Neil Armstrong <narmstrong@baylibre.com>
 * Copyright (C) 2014 Amlogic, Inc.
 */
#include <linux/err.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/hw_random.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/of.h>
#include <linux/clk.h>

#define RNG_DATA 0x00
#define RNG_CFG  0x00
#define RNG_OUT0 0x08

#define RNG_VERSION_1   1
#define RNG_VERSION_2   2

#define TIMEOUT_CNT     100

static int rng_version;
struct meson_rng_data {
	void __iomem *base;
	struct platform_device *pdev;
	struct hwrng rng;
	struct clk *core_clk;
};

static int meson_rng_read(struct hwrng *rng, void *buf, size_t max, bool wait)
{
	u32 status;
	u32 cnt = 0;
	struct meson_rng_data *data =
			container_of(rng, struct meson_rng_data, rng);
	void __iomem *rng_cfg = data->base + RNG_CFG;

	if (rng_version == RNG_VERSION_2) {
		writel_relaxed(readl_relaxed(rng_cfg) | (1 << 31), rng_cfg);
		do {
			status = readl_relaxed(rng_cfg) & (1 << 31);
		} while (status && (cnt++ < TIMEOUT_CNT));
		cnt = 0;
		do {
			status = readl_relaxed(rng_cfg) & (1 << 0);
		} while (status && (cnt++ < TIMEOUT_CNT));
		cnt = 0;

		*(u32 *)buf = readl_relaxed(data->base + RNG_OUT0);
	} else {
		*(u32 *)buf = readl_relaxed(data->base + RNG_DATA);
	}

	return sizeof(u32);
}

static void meson_rng_clk_disable(void *data)
{
	clk_disable_unprepare(data);
}

static int meson_rng_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct meson_rng_data *data;
	struct resource *res;
	int ret;
	const void *prop;
#ifdef CONFIG_OF
	int of_ret = 0;
#endif

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->pdev = pdev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	data->base = devm_ioremap_resource(dev, res);
	if (IS_ERR(data->base))
		return PTR_ERR(data->base);

	data->core_clk = devm_clk_get(dev, "core");
	if (IS_ERR(data->core_clk))
		data->core_clk = NULL;

	if (data->core_clk) {
		ret = clk_prepare_enable(data->core_clk);
		if (ret)
			return ret;
		ret = devm_add_action_or_reset(dev, meson_rng_clk_disable,
					       data->core_clk);
		if (ret)
			return ret;
	}

	data->rng.name = pdev->name;
	data->rng.read = meson_rng_read;

	prop = of_get_property(dev->of_node, "version", NULL);
	if (prop)
		rng_version = of_read_ulong(prop, 1);
	else
		rng_version = RNG_VERSION_1;

#ifdef CONFIG_OF
	of_ret =
		of_property_read_u16(pdev->dev.of_node, "quality",
				&data->rng.quality);
	if (of_ret) {
		pr_info("Unable to get quality(%d)\n", of_ret);
		// If there is no quality value specified in dts,
		// set default quality to 1000 for the reason that
		// HW RNG should be good source of entropy, and it
		// also leaves room for others to give better
		// entropy source
		data->rng.quality = 1000;
	}
#endif

	platform_set_drvdata(pdev, data);

	return devm_hwrng_register(dev, &data->rng);
}

static const struct of_device_id meson_rng_of_match[] = {
	{ .compatible = "amlogic,meson-rng", },
	{},
};
MODULE_DEVICE_TABLE(of, meson_rng_of_match);

static struct platform_driver meson_rng_driver = {
	.probe	= meson_rng_probe,
	.driver	= {
		.name = "meson-rng",
		.of_match_table = meson_rng_of_match,
	},
};

module_platform_driver(meson_rng_driver);

MODULE_DESCRIPTION("Meson H/W Random Number Generator driver");
MODULE_AUTHOR("Lawrence Mok <lawrence.mok@amlogic.com>");
MODULE_AUTHOR("Neil Armstrong <narmstrong@baylibre.com>");
MODULE_LICENSE("Dual BSD/GPL");
