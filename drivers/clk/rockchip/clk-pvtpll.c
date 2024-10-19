// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/clk-provider.h>
#include <linux/regulator/consumer.h>
#include "clk.h"

#define RV1103B_PVTPLL_GCK_CFG			0x20
#define RV1103B_PVTPLL_GCK_LEN			0x24
#define RV1103B_GCK_START			BIT(0)
#define RV1103B_GCK_EN				BIT(1)
#define RV1103B_GCK_MODE			BIT(5)
#define RV1103B_GCK_RING_LEN_SEL_OFFSET		0
#define RV1103B_GCK_RING_LEN_SEL_MASK		0x1ff
#define RV1103B_GCK_RING_SEL_OFFSET		10
#define RV1103B_GCK_RING_SEL_MASK		0x07

#define RK3506_GRF_CORE_PVTPLL_CON0_L		0x00
#define RK3506_GRF_CORE_PVTPLL_CON0_H		0x04
#define RK3506_OSC_RING_SEL_OFFSET		8
#define RK3506_OSC_RING_SEL_MASK		0x03
#define RK3506_OSC_EN				BIT(1)
#define RK3506_START				BIT(0)
#define RK3506_RING_LENGTH_SEL_OFFSET		0
#define RK3506_RING_LENGTH_SEL_MASK		0x7f

struct rockchip_clock_pvtpll;

struct pvtpll_table {
	unsigned int rate;
	u32 length;
	u32 length_frac;
	u32 ring_sel;
};

struct rockchip_clock_pvtpll_info {
	unsigned int table_size;
	struct pvtpll_table *table;
	int (*config)(struct rockchip_clock_pvtpll *pvtpll,
		      struct pvtpll_table *table);
};

struct rockchip_clock_pvtpll {
	const struct rockchip_clock_pvtpll_info *info;
	struct regmap *regmap;
	struct clk_hw hw;
	struct clk *main_clk;
	struct clk *sclk;
	struct clk *pvtpll_clk;
	struct clk *pvtpll_out;
	struct notifier_block pvtpll_nb;
	unsigned long cur_rate;
};

#define ROCKCHIP_PVTPLL(_rate, _sel, _len)			\
{								\
	.rate = _rate##U,					\
	.ring_sel = _sel,					\
	.length = _len,						\
}

static struct pvtpll_table rv1103b_core_pvtpll_table[] = {
	/* rate_hz, ring_sel, length */
	ROCKCHIP_PVTPLL(1608000000, 1, 6),
	ROCKCHIP_PVTPLL(1512000000, 1, 6),
	ROCKCHIP_PVTPLL(1416000000, 1, 6),
	ROCKCHIP_PVTPLL(1296000000, 1, 6),
	ROCKCHIP_PVTPLL(1200000000, 1, 14),
	ROCKCHIP_PVTPLL(1008000000, 1, 32),
	ROCKCHIP_PVTPLL(816000000, 1, 60),
};

static struct pvtpll_table rv1103b_npu_pvtpll_table[] = {
	/* rate_hz, ring_se, length */
	ROCKCHIP_PVTPLL(1000000000, 1, 12),
	ROCKCHIP_PVTPLL(900000000, 1, 12),
	ROCKCHIP_PVTPLL(800000000, 1, 16),
	ROCKCHIP_PVTPLL(700000000, 1, 36),
};

static struct pvtpll_table rk3506_core_pvtpll_table[] = {
	/* rate_hz, ring_sel, length */
	ROCKCHIP_PVTPLL(1608000000, 0, 6),
	ROCKCHIP_PVTPLL(1512000000, 0, 6),
	ROCKCHIP_PVTPLL(1416000000, 0, 6),
	ROCKCHIP_PVTPLL(1296000000, 0, 6),
	ROCKCHIP_PVTPLL(1200000000, 0, 8),
	ROCKCHIP_PVTPLL(1008000000, 0, 15),
};

static struct pvtpll_table
*rockchip_get_pvtpll_settings(struct rockchip_clock_pvtpll *pvtpll,
			      unsigned long rate)
{
	const struct rockchip_clock_pvtpll_info *info = pvtpll->info;
	int i;

	for (i = 0; i < info->table_size; i++) {
		if (rate == info->table[i].rate)
			return &info->table[i];
	}
	return NULL;
}

static int rv1103b_pvtpll_configs(struct rockchip_clock_pvtpll *pvtpll,
				  struct pvtpll_table *table)
{
	u32 val;
	int ret = 0;

	val = HIWORD_UPDATE(table->ring_sel, RV1103B_GCK_RING_SEL_MASK,
			    RV1103B_GCK_RING_SEL_OFFSET);
	ret = regmap_write(pvtpll->regmap, RV1103B_PVTPLL_GCK_LEN, val);
	if (ret)
		return ret;

	val = HIWORD_UPDATE(table->length, RV1103B_GCK_RING_LEN_SEL_MASK,
			    RV1103B_GCK_RING_LEN_SEL_OFFSET);
	ret = regmap_write(pvtpll->regmap, RV1103B_PVTPLL_GCK_LEN, val);
	if (ret)
		return ret;

	ret = regmap_write(pvtpll->regmap, RV1103B_PVTPLL_GCK_CFG,
			   RV1103B_GCK_EN | (RV1103B_GCK_EN << 16) |
			   RV1103B_GCK_MODE | (RV1103B_GCK_MODE << 16));
	if (ret)
		return ret;

	ret = regmap_write(pvtpll->regmap, RV1103B_PVTPLL_GCK_CFG,
			   RV1103B_GCK_START | (RV1103B_GCK_START << 16));
	if (ret)
		return ret;

	return ret;
}

static int rk3506_pvtpll_configs(struct rockchip_clock_pvtpll *pvtpll,
				  struct pvtpll_table *table)
{
	u32 val;
	int ret = 0;

	val = HIWORD_UPDATE(table->ring_sel, RK3506_OSC_RING_SEL_MASK,
			    RK3506_OSC_RING_SEL_OFFSET);
	ret = regmap_write(pvtpll->regmap, RK3506_GRF_CORE_PVTPLL_CON0_L, val);
	if (ret)
		return ret;

	val = HIWORD_UPDATE(table->length, RK3506_RING_LENGTH_SEL_MASK,
			    RK3506_RING_LENGTH_SEL_OFFSET);
	ret = regmap_write(pvtpll->regmap, RK3506_GRF_CORE_PVTPLL_CON0_H, val);
	if (ret)
		return ret;

	ret = regmap_write(pvtpll->regmap, RK3506_GRF_CORE_PVTPLL_CON0_L,
			   RK3506_START | (RK3506_START << 16) |
			   RK3506_OSC_EN | (RK3506_OSC_EN << 16));
	if (ret)
		return ret;

	return ret;
}

static int rockchip_clock_pvtpll_set_rate(struct clk_hw *hw,
					  unsigned long rate,
					  unsigned long parent_rate)
{
	struct rockchip_clock_pvtpll *pvtpll;
	struct pvtpll_table *table;
	int ret = 0;

	pvtpll = container_of(hw, struct rockchip_clock_pvtpll, hw);

	if (!pvtpll)
		return 0;

	table = rockchip_get_pvtpll_settings(pvtpll, rate);
	if (!table)
		return 0;

	ret = pvtpll->info->config(pvtpll, table);

	pvtpll->cur_rate = rate;
	return ret;
}

static unsigned long
rockchip_clock_pvtpll_recalc_rate(struct clk_hw *hw,
				  unsigned long parent_rate)
{
	struct rockchip_clock_pvtpll *pvtpll;

	pvtpll = container_of(hw, struct rockchip_clock_pvtpll, hw);

	if (!pvtpll)
		return 0;

	return pvtpll->cur_rate;
}

static long rockchip_clock_pvtpll_round_rate(struct clk_hw *hw, unsigned long rate,
					     unsigned long *prate)
{
	struct rockchip_clock_pvtpll *pvtpll;
	struct pvtpll_table *table;

	pvtpll = container_of(hw, struct rockchip_clock_pvtpll, hw);

	if (!pvtpll)
		return 0;

	table = rockchip_get_pvtpll_settings(pvtpll, rate);
	if (!table)
		return 0;

	return rate;
}

static const struct clk_ops clock_pvtpll_ops = {
	.recalc_rate = rockchip_clock_pvtpll_recalc_rate,
	.round_rate = rockchip_clock_pvtpll_round_rate,
	.set_rate = rockchip_clock_pvtpll_set_rate,
};

static int clock_pvtpll_regitstor(struct device *dev,
				  struct rockchip_clock_pvtpll *pvtpll)
{
	struct clk_init_data init = {};

	init.parent_names = NULL;
	init.num_parents = 0;
	init.flags = CLK_GET_RATE_NOCACHE;
	init.name = "pvtpll";
	init.ops = &clock_pvtpll_ops;

	pvtpll->hw.init = &init;

	/* optional override of the clockname */
	of_property_read_string_index(dev->of_node, "clock-output-names",
				      0, &init.name);
	pvtpll->pvtpll_out = devm_clk_register(dev, &pvtpll->hw);
	if (IS_ERR(pvtpll->pvtpll_out))
		return PTR_ERR(pvtpll->pvtpll_out);

	return of_clk_add_provider(dev->of_node, of_clk_src_simple_get,
				   pvtpll->pvtpll_out);
}

static const struct rockchip_clock_pvtpll_info rv1103b_core_pvtpll_data = {
	.config = rv1103b_pvtpll_configs,
	.table_size = ARRAY_SIZE(rv1103b_core_pvtpll_table),
	.table = rv1103b_core_pvtpll_table,
};

static const struct rockchip_clock_pvtpll_info rv1103b_npu_pvtpll_data = {
	.config = rv1103b_pvtpll_configs,
	.table_size = ARRAY_SIZE(rv1103b_npu_pvtpll_table),
	.table = rv1103b_npu_pvtpll_table,
};

static const struct rockchip_clock_pvtpll_info rk3506_core_pvtpll_data = {
	.config = rk3506_pvtpll_configs,
	.table_size = ARRAY_SIZE(rk3506_core_pvtpll_table),
	.table = rk3506_core_pvtpll_table,
};

static const struct of_device_id rockchip_clock_pvtpll_match[] = {
	{
		.compatible = "rockchip,rv1103b-core-pvtpll",
		.data = (void *)&rv1103b_core_pvtpll_data,
	},
	{
		.compatible = "rockchip,rv1103b-npu-pvtpll",
		.data = (void *)&rv1103b_npu_pvtpll_data,
	},
	{
		.compatible = "rockchip,rk3506-core-pvtpll",
		.data = (void *)&rk3506_core_pvtpll_data,
	},
	{}
};
MODULE_DEVICE_TABLE(of, rockchip_clock_pvtpll_match);

static int rockchip_clock_pvtpll_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = pdev->dev.of_node;
	struct rockchip_clock_pvtpll *pvtpll;
	int error = 0;

	pvtpll = devm_kzalloc(dev, sizeof(*pvtpll), GFP_KERNEL);
	if (!pvtpll)
		return -ENOMEM;

	pvtpll->info = (const struct rockchip_clock_pvtpll_info *)device_get_match_data(&pdev->dev);
	if (!pvtpll->info)
		return -EINVAL;

	pvtpll->regmap = device_node_to_regmap(np);
	if (IS_ERR(pvtpll->regmap))
		return PTR_ERR(pvtpll->regmap);

	platform_set_drvdata(pdev, pvtpll);

	error = clock_pvtpll_regitstor(&pdev->dev, pvtpll);
	if (error) {
		dev_err(&pdev->dev, "failed to register clock: %d\n",
			error);
	}

	return error;
}

static int rockchip_clock_pvtpll_remove(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;

	of_clk_del_provider(np);

	return 0;
}

static struct platform_driver rockchip_clock_pvtpll_driver = {
	.driver = {
		.name = "rockchip-clcok-pvtpll",
		.of_match_table = rockchip_clock_pvtpll_match,
	},
	.probe = rockchip_clock_pvtpll_probe,
	.remove = rockchip_clock_pvtpll_remove,
};

module_platform_driver(rockchip_clock_pvtpll_driver);

MODULE_DESCRIPTION("Rockchip Clock Pvtpll Driver");
MODULE_LICENSE("GPL");
