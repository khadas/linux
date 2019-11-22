/*
 * drivers/amlogic/pm/vad_power.h
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

struct pm_data {
	struct device *dev;
	int vddio3v3_en;
	bool vad_wakeup_disable;
	void __iomem *dmc_asr;
	void __iomem *cpu_reg;
	struct clk *switch_clk81;
	struct clk *clk81;
	struct clk *fixed_pll;
	struct clk *xtal;
};

int vad_wakeup_power_init(struct platform_device *pdev, struct pm_data *p_data);
int vad_wakeup_power_suspend(struct device *dev);
int vad_wakeup_power_resume(struct device *dev);

