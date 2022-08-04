// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * drivers/amlogic/pm/power_t3.c
 *
 * Copyright (C) 2021 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */
#include <linux/pm.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include "vad_power.h"
#include <linux/arm-smccc.h>

#define DMC_PHY_NUM	2
#define VDDIO_3V3_NAME	"vddio3v3_en"
#define P5V_NAME	"p5v_en"

//#define POWER_DEBUG

struct pm_private_data {
	struct clk *clk81;
	struct clk *fixed_pll;
	struct clk *dsu_clk;
	struct clk *cpu_clk;
	struct clk *axi_clk;
	int vddio3v3_en;
	int p5v_en;
	int init_done_flag;
	struct {
		void __iomem *addr;
		u32 value;
		u32 bak_val;
	} dmc_asr[DMC_PHY_NUM];
};

static struct pm_private_data pm_private_data;

enum {
	T3_SYS_CLK_SW_24M = 0x1000,
	T3_FIXPLL,
	T3_DMC,
	T3_AXI,
	T3_DUMP_INFO,
} power_ctrl;

int power_manage_by_smc(u32 func, u32 arg)
{
	struct arm_smccc_res res;

	arm_smccc_smc(SMCID_POWER_MANAGER, func,
			      arg, 0, 0, 0, 0, 0, &res);
	return 0;
}

static int pm_vad_init(struct platform_device *pdev)
{
	struct pm_data *p_data = platform_get_drvdata(pdev);
	struct device_node *np = pdev->dev.of_node;
	u32 paddr = 0;
	int ret;
	int i;

	/* Resolve clk parameters */
	pm_private_data.clk81 = devm_clk_get(&pdev->dev, "sys_clk");
	if (IS_ERR(pm_private_data.clk81)) {
		dev_err(&pdev->dev,
				"Can't get clk81!!\n");
		return PTR_ERR(pm_private_data.clk81);
	}

	pm_private_data.fixed_pll = devm_clk_get(&pdev->dev, "fixed_pll_dco");
	if (IS_ERR(pm_private_data.fixed_pll)) {
		dev_err(&pdev->dev,
				"Can't get fixed_pll!!\n");
		return PTR_ERR(pm_private_data.fixed_pll);
	}

	pm_private_data.cpu_clk = devm_clk_get(&pdev->dev, "cpu_clk");
	if (IS_ERR(pm_private_data.cpu_clk)) {
		dev_err(&pdev->dev,
				"Can't get cpu_clk!!!\n");
		return PTR_ERR(pm_private_data.cpu_clk);
	}

	pm_private_data.dsu_clk = devm_clk_get(&pdev->dev, "dsu_clk");
	if (IS_ERR(pm_private_data.dsu_clk)) {
		dev_err(&pdev->dev,
				"Can't get dsu_clk!!!\n");
		return PTR_ERR(pm_private_data.dsu_clk);
	}

	pm_private_data.axi_clk = devm_clk_get(&pdev->dev, "axi_clk");
	if (IS_ERR(pm_private_data.axi_clk)) {
		dev_err(&pdev->dev,
				"Can't get axi_clk!!!\n");
	}

	/* Read dmc data */
	ret = of_property_count_u32_elems(np, "meson,dmc_asr-addr");
	if (ret != DMC_PHY_NUM) {
		dev_warn(&pdev->dev,
				"The number of dmc addr does not match!\n");
		return ret;
	}

	ret = of_property_count_u32_elems(np, "meson,dmc_asr-val");
	if (ret != DMC_PHY_NUM) {
		dev_warn(&pdev->dev,
				"The number of dmc value does not match!\n");
		return ret;
	}

	for (i = 0; i < DMC_PHY_NUM; i++) {
		ret = of_property_read_u32_index(np, "meson,dmc_asr-addr",
				i, &paddr);
		if (ret) {
			dev_warn(&pdev->dev,
					"Read dmc_asr addr fail:%d\n", i);
			return ret;
		}
		pm_private_data.dmc_asr[i].addr = ioremap(paddr, 4);
		dev_info(&pdev->dev, "dmc physical addr:0x%x\n", paddr);

		ret = of_property_read_u32_index(np, "meson,dmc_asr-val",
				i, &paddr);
		if (ret) {
			dev_warn(&pdev->dev,
					"Read dmc_asr value fail:%d\n", i);
			return ret;
		}
		pm_private_data.dmc_asr[i].value = paddr;
		dev_info(&pdev->dev, "dmc physical value:0x%x\n", paddr);
	}

	p_data->data = (void *)&pm_private_data;
	pm_private_data.init_done_flag = 1;
	return 0;
}

static struct clk *before_dsu_parent;
static int fixed_pll_cnt;
static u32 sys_rate;

static int pm_vad_suspend(struct platform_device *pdev)
{
	struct pm_data *p_data = platform_get_drvdata(pdev);
	struct pm_private_data *private_data = p_data->data;
	int i;

	if (!private_data->init_done_flag) {
		dev_warn(&pdev->dev, "Current vad item not initialization\n");
		return -1;
	}

	/* suspend clk*/
	// [suspend clk] 1. switch clk81 to 24M
	sys_rate = clk_get_rate(private_data->clk81);
	dev_info(&pdev->dev,
			"before clk81 rate:%u\n",
			sys_rate);
	if (clk_set_rate(private_data->clk81, 24000000))
		dev_info(&pdev->dev, "Switch clk81 to 24M fail!!\n");
	else
		dev_info(&pdev->dev, "Switch clk81 to 24M!!\n");

	dev_info(&pdev->dev,
			"clk81 rate:%ld\n",
			clk_get_rate(private_data->clk81));

	// [suspend clk] 2. switch cpu clk to sys pll
	dev_info(&pdev->dev, "Current cpu freq:%lu\n",
			clk_get_rate(private_data->cpu_clk));

	// [suspend clk] 3. switch dsu clk to cpu clk
	before_dsu_parent = clk_get_parent(private_data->dsu_clk);
	dev_info(&pdev->dev, "DSU clk current parents:%s\n",
			__clk_get_name(before_dsu_parent));

	// [suspend clk] 4. Switch axi clk to cpu clk
	if (clk_set_rate(private_data->axi_clk, 200000000)) {
		dev_err(&pdev->dev,
				" axi clk switch fail!!!\n");
	} else {
		dev_err(&pdev->dev,
				" axi clk switch successful!!!\n");
	}
	dev_info(&pdev->dev, "Current axi freq:%lu\n",
			clk_get_rate(private_data->axi_clk));

	// [suspend clk] 5. Ready to close fixpll
	if (__clk_is_enabled(private_data->fixed_pll)) {
		dev_info(&pdev->dev,
				"before fixpll is enable.\n");
	} else {
		dev_info(&pdev->dev,
				"before fixpll is close!!\n");
	}

	fixed_pll_cnt = __clk_get_enable_count(private_data->fixed_pll);
	if (fixed_pll_cnt) {
		dev_info(&pdev->dev,
				"Now fix pll enable count:%d\n", fixed_pll_cnt);
		for (i = 0; i < fixed_pll_cnt; i++)
			clk_disable_unprepare(private_data->fixed_pll);
	}

	dev_info(&pdev->dev,
			"fixpll enable counter:%d\n",
			__clk_get_enable_count(private_data->fixed_pll));

	if (__clk_is_enabled(private_data->fixed_pll)) {
		dev_info(&pdev->dev,
				"fixpll is open.\n");
	} else {
		dev_info(&pdev->dev,
				"fixpll is close !!\n");
	}

	/* suspend dmc */
	// [suspend dmc] 1. Enable ASR mode
	dev_info(&pdev->dev, "DDR ready to enter ASR mode\n");
	power_manage_by_smc(SMCSUBID_PM_DDR_ASR, 0);

	/* dump clk and power status */
	power_manage_by_smc(SMCSUBID_PM_DUMP_INFO, 0);

	return 0;
}

static int pm_vad_resume(struct platform_device *pdev)
{
	struct pm_data *p_data = platform_get_drvdata(pdev);
	struct pm_private_data *private_data = p_data->data;
	int i;

	if (!private_data->init_done_flag) {
		dev_warn(&pdev->dev, "Current vad item not initialization\n");
		return -1;
	}

	/* resume dmc */
	dev_info(&pdev->dev, "DDR ready to exit ASR mode...\n");
	power_manage_by_smc(SMCSUBID_PM_DDR_ASR, 1);

	/* resume clk */
	// [resume clk] 1. Ready to close fixpll
	for (i = 0; i < fixed_pll_cnt; i++) {
		if (clk_prepare_enable(private_data->fixed_pll))
			dev_err(&pdev->dev, "failed to enable fixed pll\n");
	}

	dev_info(&pdev->dev,
			"fixpll enable counter:%d\n",
			__clk_get_enable_count(private_data->fixed_pll));

	// [resume clk] 2. Restore dsu clk parent
	dev_info(&pdev->dev, "DSU clk current parents:%s\n",
			__clk_get_name(clk_get_parent(private_data->dsu_clk)));

	// [resume clk] 3. Restore axi clock
	if (clk_set_rate(private_data->axi_clk, 500000000)) {
		dev_err(&pdev->dev,
				" axi clk restore fail!!!\n");
	} else {
		dev_err(&pdev->dev,
				" axi clk restore successful!!!\n");
	}
	dev_info(&pdev->dev, "Current axi freq:%lu\n",
			clk_get_rate(private_data->axi_clk));

	// [suspend clk] 4. Restore clk81
	if (clk_set_rate(private_data->clk81, sys_rate))
		dev_info(&pdev->dev, "Restore clk81  fail!!\n");
	else
		dev_info(&pdev->dev, "Restore clk81 successful!!\n");

	return 0;
}

struct pm_ops t3_pm_ops = {
	.vad_pm_init = pm_vad_init,
	.vad_pm_suspend = pm_vad_suspend,
	.vad_pm_resume = pm_vad_resume,
};

