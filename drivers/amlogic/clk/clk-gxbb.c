/*
 * drivers/amlogic/clk/clk-gxbb.c
 *
 * Copyright (C) 2015 Amlogic, Inc. All rights reserved.
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

#include <dt-bindings/clock/gxbb.h>
#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/clk-provider.h>
#include <linux/of.h>
#include <linux/of_address.h>

#include "clk.h"
void __iomem *reg_base_hiubus;
#undef	 HHI_GCLK_MPEG0
#define     HHI_GCLK_MPEG0 (0x50 << 2)


#define GXBB_RSTC_N_REGS	6
#define GXBB_AO_OFF		((GXBB_RSTC_N_REGS - 1) * BITS_PER_LONG + 4)

/* fixed rate clocks generated outside the soc */
static struct amlogic_fixed_rate_clock gxbb_fixed_rate_ext_clks[] __initdata = {
/*obtain the clock speed of external fixed clock sources from device tree*/
	FRATE(CLK_XTAL, "xtal", NULL, CLK_IS_ROOT, 24000000),
	FRATE(0, "32Khz", NULL, CLK_IS_ROOT, 32000),
	FRATE(CLK_81, "clk81", NULL, CLK_IS_ROOT, 166666666),
	FRATE(CLK_FIXED_PLL, "fixed_pll", NULL, CLK_IS_ROOT, 2000000000),
	FRATE(CLK_FPLL_DIV2, "fclk_div2", NULL, CLK_IS_ROOT, 1000000000),
	FRATE(CLK_FPLL_DIV3, "fclk_div3", NULL, CLK_IS_ROOT,  666666666),
	FRATE(CLK_FPLL_DIV4, "fclk_div4", NULL, CLK_IS_ROOT,  500000000),
	FRATE(CLK_FPLL_DIV5, "fclk_div5", NULL, CLK_IS_ROOT,  400000000),
	FRATE(CLK_FPLL_DIV7, "fclk_div7", NULL, CLK_IS_ROOT,  285714285),
};

static struct of_device_id ext_clk_match[] __initdata = {
	{ .compatible = "amlogic,clock-xtal", .data = (void *)0, },
	{},
};

/* register gxbb clocks */
static void __init gxbb_clk_init(struct device_node *np)
{

	reg_base_hiubus = of_iomap(np, 0);
	reg_base_aobus = of_iomap(np, 1);
	if ((!reg_base_hiubus) || (!reg_base_aobus))
		panic("%s: failed to map registers\n", __func__);

	pr_debug("gxbb clk HIU base is 0x%p\n", reg_base_hiubus);
	pr_debug("gxbb clk ao base is 0x%p\n", reg_base_aobus);

	amlogic_clk_init(np, reg_base_hiubus, reg_base_aobus,
			CLK_NR_CLKS, NULL, 0, NULL, 0);
	amlogic_clk_of_register_fixed_ext(gxbb_fixed_rate_ext_clks,
		  ARRAY_SIZE(gxbb_fixed_rate_ext_clks), ext_clk_match);
	meson_register_rstc(np, GXBB_RSTC_N_REGS, reg_base_aobus,
		reg_base_hiubus + HHI_GCLK_MPEG0, GXBB_AO_OFF, 0);


	{
		/* Dump clocks */
		char *clks[] = {
				"xtal",
				"32Khz",
				"clk81",
				"fixed_pll",
				"fclk_div2",
				"fclk_div3",
				"fclk_div4",
				"fclk_div5",
				"fclk_div7",
		};
		int i;
		int count = ARRAY_SIZE(clks);

		for (i = 0; i < count; i++) {
			char *clk_name = clks[i];
			pr_info("clkrate [ %s \t] : %luHz\n", clk_name,
				_get_rate(clk_name));
		}

	}
	pr_info("gxbb clock initialization complete\n");
}
CLK_OF_DECLARE(gxbb, "amlogic, gxbb-clock", gxbb_clk_init);
