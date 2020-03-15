// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#undef pr_fmt
#define pr_fmt(fmt) "audio_clocks: " fmt

#include <linux/of_device.h>

#include "audio_clks.h"

#define DRV_NAME "audio-clocks"

DEFINE_SPINLOCK(aclk_lock);

static const struct of_device_id audio_clocks_of_match[] = {
	{
		.compatible = "amlogic, sm1-audio-clocks",
		.data		= &sm1_audio_clks_init,
	},
	{
		.compatible = "amlogic, tm2-audio-clocks",
		.data		= &tm2_audio_clks_init,
	},
	{},
};
MODULE_DEVICE_TABLE(of, audio_clocks_of_match);

static int audio_clocks_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct clk **clks;
	struct clk_onecell_data *clk_data;
	void __iomem *clk_base;
	struct audio_clk_init *p_audioclk_init;
	int ret;

	p_audioclk_init = (struct audio_clk_init *)
		of_device_get_match_data(dev);
	if (!p_audioclk_init) {
		dev_warn_once(dev,
			      "check to update audio clk chipinfo\n");
		return -EINVAL;
	}

	clk_base = of_iomap(np, 0);
	if (!clk_base) {
		dev_err(dev,
			"Unable to map clk base\n");
		return -ENXIO;
	}

	clk_data = devm_kmalloc(dev, sizeof(*clk_data), GFP_KERNEL);
	if (!clk_data)
		return -ENOMEM;

	clks = devm_kmalloc(dev,
			    p_audioclk_init->clk_num * sizeof(*clks),
			    GFP_KERNEL);
	if (!clks)
		return -ENOMEM;

	if (p_audioclk_init) {
		p_audioclk_init->clk_gates(clks, clk_base);
		p_audioclk_init->clks(clks, clk_base);
	}

	clk_data->clks = clks;
	clk_data->clk_num = p_audioclk_init->clk_num;

	ret = of_clk_add_provider(np, of_clk_src_onecell_get, clk_data);
	if (ret) {
		dev_err(dev, "%s fail ret: %d\n", __func__, ret);

		return ret;
	}

	pr_info("%s done\n", __func__);

	return 0;
}

static struct platform_driver audio_clocks_driver = {
	.driver = {
		.name           = DRV_NAME,
		.of_match_table = audio_clocks_of_match,
	},
	.probe  = audio_clocks_probe,
};

int __init audio_clocks_init(void)
{
	return platform_driver_register(&audio_clocks_driver);
}

void __exit audio_clocks_exit(void)
{
	platform_driver_unregister(&audio_clocks_driver);
}

#ifndef MODULE
core_initcall(audio_clocks_init);
module_exit(audio_clocks_exit);
MODULE_AUTHOR("Amlogic, Inc.");
MODULE_DESCRIPTION("Amlogic audio clocks ASoc driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRV_NAME);
#endif
