/*
 * drivers/amlogic/pm/vad_power.c
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
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
#include <linux/errno.h>
#include <linux/of_address.h>
#include <linux/amlogic/pm.h>
#include <linux/kobject.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include "../../gpio/gpiolib.h"
#include "vad_power.h"

#define IO3V3_EN "vddio3v3_en"

static int fixed_pll_cnt;

int vad_wakeup_power_init(struct platform_device *pdev, struct pm_data *p_data)
{
	int ret;
	const char *value;
	struct gpio_desc *desc;
	u32 paddr = 0;

	ret = of_property_read_string(pdev->dev.of_node, "vddio3v3_en", &value);
	if (ret) {
		pr_info("no vddio3v3_en pin");
		p_data->vddio3v3_en = 0;
	} else {
		desc = of_get_named_gpiod_flags(pdev->dev.of_node,
						"vddio3v3_en", 0, NULL);
		p_data->vddio3v3_en = desc_to_gpio(desc);
	}
	if (p_data->vddio3v3_en > 0)
		gpio_request(p_data->vddio3v3_en, IO3V3_EN);

	ret = of_property_read_u32(pdev->dev.of_node,
				   "vad_wakeup_disable", &paddr);
	if (!ret) {
		p_data->vad_wakeup_disable = paddr;
		pr_info("vad_wakeup_disable: 0x%x\n", paddr);
	} else {
		p_data->vad_wakeup_disable = 1;
	}

	ret = of_property_read_u32(pdev->dev.of_node,
				   "dmc_asr", &paddr);
	if (!ret) {
		pr_info("dmc_asr: 0x%x\n", paddr);
		p_data->dmc_asr = ioremap(paddr, 0x4);
	} else {
		p_data->dmc_asr = 0;
	}

	ret = of_property_read_u32(pdev->dev.of_node,
				   "cpu_reg", &paddr);
	if (!ret) {
		pr_info("cpu_reg: 0x%x\n", paddr);
		p_data->cpu_reg = ioremap(paddr, 0x4);
	} else {
		p_data->cpu_reg = 0;
	}

	p_data->switch_clk81 = devm_clk_get(&pdev->dev, "switch_clk81");
	if (IS_ERR(p_data->switch_clk81)) {
		dev_err(&pdev->dev,
			"Can't get switch_clk81\n");
		return PTR_ERR(p_data->switch_clk81);
	}
	p_data->clk81 = devm_clk_get(&pdev->dev, "clk81");
	if (IS_ERR(p_data->clk81)) {
		dev_err(&pdev->dev,
			"Can't get clk81\n");
		return PTR_ERR(p_data->clk81);
	}
	p_data->xtal = devm_clk_get(&pdev->dev, "xtal");
	if (IS_ERR(p_data->xtal)) {
		dev_err(&pdev->dev,
			"Can't get xtal\n");
		return PTR_ERR(p_data->xtal);
	}
	p_data->fixed_pll = devm_clk_get(&pdev->dev, "fixed_pll");
	if (IS_ERR(p_data->fixed_pll)) {
		dev_err(&pdev->dev,
			"Can't get fixed_pll\n");
		return PTR_ERR(p_data->fixed_pll);
	}

	return 0;
}

void cpu_clk_switch_to_gp1(unsigned int flag, void __iomem *paddr)
{
	u32 control;
	u32 dyn_pre_mux  = 0;
	u32 dyn_post_mux = 1;
	u32 dyn_div = 1;

	control = readl(paddr);
	/*check cpu busy or not*/
	do {
		control = readl(paddr);
	} while (control & (1 << 28));

	if (!flag) {
		dyn_pre_mux  = 3;
		dyn_post_mux = 1;
		dyn_div = 1;
		/*cpu clk sel channel a*/
		if (control & (1 << 10)) {
			control = (control & ~((1 << 10) | (0x3f << 4)
						| (1 << 2) | (0x3 << 0)))
						| ((0 << 10)
						| (dyn_div << 4)
						| (dyn_post_mux << 2)
						| (dyn_pre_mux << 0));
		} else {
			/*cpu clk sel channel b*/
			control = (control & ~((1 << 10) | (0x3f << 20)
						| (1 << 18) | (0x3 << 16)))
						| ((1 << 10)
						| (dyn_div << 20)
						| (dyn_post_mux << 18)
						| (dyn_pre_mux << 16));
		}
	} else {
		if (control & (1 << 10))
			control = control & ~(1 << 10);
		else
			control = control | (1 << 10);
	}
	writel(control, paddr);
}

int vad_wakeup_power_suspend(struct device *dev)
{
	struct pm_data *p_data = dev_get_drvdata(dev);
	int i;

	if (!is_pm_freeze_mode() || p_data->vad_wakeup_disable)
		return 0;

	clk_set_parent(p_data->switch_clk81, p_data->xtal);
	pr_info("switch clk81 to 24M.\n");

	/*cpu clk switch to gp1*/
	cpu_clk_switch_to_gp1(0, p_data->cpu_reg);
	pr_info("cpu clk switch to gp1.\n");

	fixed_pll_cnt = __clk_get_enable_count(p_data->fixed_pll);
	if (fixed_pll_cnt > 1)
		dev_warn(dev,
			 "Now fixed pll enable count = %d\n",
			 fixed_pll_cnt);

	for (i = 0; i < fixed_pll_cnt; i++)
		clk_disable_unprepare(p_data->fixed_pll);

	gpio_direction_output(p_data->vddio3v3_en, 0);
	pr_info("power off vddio_3v3.\n");

	if (p_data->dmc_asr) {
		writel(0x3fe00, p_data->dmc_asr);
		pr_info("enable dmc asr\n");
	}

	return 0;
}

int vad_wakeup_power_resume(struct device *dev)
{
	struct pm_data *p_data = dev_get_drvdata(dev);
	int i;

	if (!is_pm_freeze_mode() || p_data->vad_wakeup_disable)
		return 0;

	gpio_direction_output(p_data->vddio3v3_en, 1);
	pr_info("power on vddio_3v3.\n");

	if (p_data->dmc_asr) {
		writel(0x0, p_data->dmc_asr);
		pr_info("disable dmc asr\n");
	}

	/* enable fixed pll */
	for (i = 0; i < fixed_pll_cnt; i++) {
		if (clk_prepare_enable(p_data->fixed_pll))
			dev_err(dev, "failed to enable fixed pll\n");
	}

	/*restore cpu clk*/
	cpu_clk_switch_to_gp1(1, p_data->cpu_reg);
	pr_info("cpu clk restore.\n");
	clk_set_parent(p_data->switch_clk81, p_data->clk81);
	pr_info("switch clk81 to 166M.\n");

	return 0;
}
