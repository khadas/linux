/*
 * drivers/amlogic/clk/clk-meson8m2.c
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

#include <dt-bindings/clock/meson8.h>
#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/clk-provider.h>
#include <linux/of.h>
#include <linux/of_address.h>

#include "clk.h"
#include "clk-pll.h"
#include <linux/delay.h>
#define CLK_GATE_BASE 0x100B
#define CLK_GATE_1050 0x1050
#define CLK_GATE_1051 0x1051
#define CLK_GATE_1052 0x1052
#define CLK_GATE_1054 0x1054
#define CLK_GATE_AO_BASE 0xc8100000
#define OFFSET(x) (((x) - CLK_GATE_BASE)*4) /*(x -0x100B)*4 */

#define AO_RTI_GEN_CTNL_REG0 0xc8100040
#define OFFSET_AO(x) (((x) - CLK_GATE_AO_BASE)*4)
void __iomem *reg_base_cbus, *reg_base_aobus, *reg_base_hiubase;
#define MESON8_RSTC_N_REGS	6
#define MESON8_AO_OFF		((MESON8_RSTC_N_REGS - 1) * BITS_PER_LONG + 3)
/* list of PLLs */
enum meson8m2_plls {
	fixed_pll, sys_pll, vid_pll, g_pll, hdmi_pll,
	nr_plls			/* number of PLLs */
};

/* list of all parent clocks */
PNAME(clk81_p1) = {"xtal", "32Khz"};
PNAME(clk81_p2) = {"clk81_m_1", "", "fclk_div7",
	    "mpll_clk_out1", "mpll_clk_out2",
	    "fclk_div4", "fclk_div3", "fclk_div5"};
PNAME(clk81_p3) = {"clk81_m_1", "clk81_g"};
PNAME(mux_hdmi_sys_p) = {"xtal", "fclk_div4", "fclk_div3", "fclk_div5"};
PNAME(mux_mali_0_p) = {"xtal", "g_pll", "mpll_clk_out1",
	"mpll_clk_out2", "fclk_div7",
	"fclk_div4", "fclk_div3", "fclk_div5"};
PNAME(mux_mali_1_p) = {"xtal", "g_pll", "mpll_clk_out1",
	"mpll_clk_out2", "fclk_div7",
	"fclk_div4", "fclk_div3", "fclk_div5"};
PNAME(mux_mali_p) = {"clk_mali_0", "clk_mali_1"};

/* fixed rate clocks generated outside the soc */
static struct amlogic_fixed_rate_clock meson8_fixed_rate_ext_clks[] __initdata = {
/*obtain the clock speed of external fixed clock sources from device tree*/
	FRATE(CLK_XTAL, "xtal", NULL, CLK_IS_ROOT, 24000000),
	FRATE(0, "32Khz", NULL, CLK_IS_ROOT, 32000),
};

/* fixed factor clocks */
static struct amlogic_fixed_factor_clock meson8_fixed_factor_clks[] __initdata = {
	FFACTOR(CLK_FPLL_DIV3,
			"fclk_div3", "fixed_pll", 1, 3, CLK_GET_RATE_NOCACHE),
	FFACTOR(CLK_FPLL_DIV4,
			"fclk_div4", "fixed_pll", 1, 4, CLK_GET_RATE_NOCACHE),
	FFACTOR(CLK_FPLL_DIV5,
			"fclk_div5", "fixed_pll", 1, 5, CLK_GET_RATE_NOCACHE),
	FFACTOR(CLK_FPLL_DIV7,
			"fclk_div7", "fixed_pll", 1, 7, CLK_GET_RATE_NOCACHE),
	FFACTOR(0,
			"sys_pll_div2", "sys_pll", 1, 2, CLK_GET_RATE_NOCACHE),
	FFACTOR(0,
			"sys_pll_div3", "sys_pll", 1, 3, CLK_GET_RATE_NOCACHE),
};

/* mux clocks */
static struct amlogic_mux_clock meson8_mux_clks[] __initdata = {
/*clk81*/
	MUX_A(0, "clk81_m_1", clk81_p1, OFFSET(HHI_MPEG_CLK_CNTL),
		9, 1, "ext_osc", 0),
	MUX(0, "clk81_m_2", clk81_p2, OFFSET(HHI_MPEG_CLK_CNTL), 12, 3, 0),
	MUX(CLK_81, "clk81", clk81_p3, OFFSET(HHI_MPEG_CLK_CNTL), 8, 1, 0),

    /*mali clk*/
	MUX(CLK_MALI, "clk_mali", mux_mali_p,
			OFFSET(HHI_MALI_CLK_CNTL), 31, 1, 0),
};

/* divider clocks */
static struct amlogic_div_clock meson8_div_clks[] __initdata = {
/*clk81*/
	DIV(0, "clk81_d", "clk81_m_2", OFFSET(HHI_MPEG_CLK_CNTL), 0, 6, 0),
};

/* gate clocks */
static struct amlogic_gate_clock meson8_gate_clks[] __initdata = {
/*clk81*/
	GATE(0, "clk81_g", "clk81_d", OFFSET(HHI_MPEG_CLK_CNTL), 7, 0, 0, 0),
};


static struct amlogic_pll_clock meson8m2_plls[] __initdata = {
	PLL(pll_2550, CLK_FIXED_PLL, "fixed_pll", "xtal",
		OFFSET(HHI_MPLL_CNTL), NULL),
	PLL(pll_1500, CLK_G_PLL, "g_pll", "xtal",
		OFFSET(HHI_GP_PLL_CNTL), NULL),
};
/*	[ddr_pll] = PLL(pll_1500, CLK_DDR_PLL, "ddr_pll", "xtal", DDRPLL_LOCK,
		DDRPLL_CON0, NULL),*/
static struct amlogic_clk_branch meson8m2_clk_branches[] __initdata = {
	COMPOSITE(CLK_HDMITX_SYS, "hdmi_sys_clk", mux_hdmi_sys_p,
		CLK_SET_RATE_NO_REPARENT,
		OFFSET(HHI_HDMI_CLK_CNTL), 9, 2, 0,
		OFFSET(HHI_HDMI_CLK_CNTL), 0, 6, 0,
		OFFSET(HHI_HDMI_CLK_CNTL), 8, 0),
	COMPOSITE(CLK_MALI_0, "clk_mali_0", mux_mali_0_p,
		CLK_SET_RATE_NO_REPARENT,
		OFFSET(HHI_MALI_CLK_CNTL), 9, 3, 0,
		OFFSET(HHI_MALI_CLK_CNTL), 0, 7,
		CLK_DIVIDER_ROUND_CLOSEST,
		OFFSET(HHI_MALI_CLK_CNTL), 8, 0),
	COMPOSITE(CLK_MALI_1, "clk_mali_1", mux_mali_1_p,
		CLK_SET_RATE_NO_REPARENT,
		OFFSET(HHI_MALI_CLK_CNTL), 25, 3, 0,
		OFFSET(HHI_MALI_CLK_CNTL), 16, 7,
		CLK_DIVIDER_ROUND_CLOSEST,
		OFFSET(HHI_MALI_CLK_CNTL), 24, 0),
};
static struct of_device_id ext_clk_match[] __initdata = {
	{ .compatible = "amlogic,clock-xtal", .data = (void *)0, },
	{},
};

/* register meson8 clocks */
static void __init meson8_clk_init(struct device_node *np)
{

	reg_base_cbus = of_iomap(np, 0);
	reg_base_aobus = of_iomap(np, 1);
	reg_base_hiubase = of_iomap(np, 2);
	if ((!reg_base_cbus) || (!reg_base_aobus))
		panic("%s: failed to map registers\n", __func__);

	pr_debug("meson8 clk reg_base_cbus is 0x%p\n", reg_base_cbus);
	pr_debug("meson8 clk reg_base_aobus is 0x%p\n", reg_base_aobus);

	amlogic_clk_init(np, reg_base_cbus, reg_base_aobus,
			CLK_NR_CLKS, NULL, 0, NULL, 0);
	amlogic_clk_of_register_fixed_ext(meson8_fixed_rate_ext_clks,
		  ARRAY_SIZE(meson8_fixed_rate_ext_clks), ext_clk_match);

	amlogic_clk_register_pll(meson8m2_plls,
	    ARRAY_SIZE(meson8m2_plls), reg_base_cbus);
	hdmi_clk_init(reg_base_cbus);
	amlogic_clk_register_fixed_factor(meson8_fixed_factor_clks,
			ARRAY_SIZE(meson8_fixed_factor_clks));

	amlogic_clk_register_mux(meson8_mux_clks,
			ARRAY_SIZE(meson8_mux_clks));
	amlogic_clk_register_div(meson8_div_clks,
			ARRAY_SIZE(meson8_div_clks));
	amlogic_clk_register_gate(meson8_gate_clks,
			ARRAY_SIZE(meson8_gate_clks));
	amlogic_clk_register_branches(meson8m2_clk_branches,
				  ARRAY_SIZE(meson8m2_clk_branches));
	meson_register_rstc(np, MESON8_RSTC_N_REGS, reg_base_aobus,
		reg_base_cbus + OFFSET(0x1050), MESON8_AO_OFF, 0);
	sys_pll_init(reg_base_hiubase, np, CLK_SYS_PLL);

	{
		/* Dump clocks */
		char *clks[] = {
				"xtal",
				"32Khz",
				"fixed_pll",
				"sys_pll",
				"fclk_div3",
				"fclk_div4",
				"fclk_div5",
				"fclk_div7",
				"clk81_m_1",
				"clk81_m_2",
				"clk81_d",
				"clk81_g",
				"clk81",
				"hdmi_pll_lvds",
				"hdmi_sys_clk"
		};
		int i;
		int count = ARRAY_SIZE(clks);

		for (i = 0; i < count; i++) {
			char *clk_name = clks[i];
			pr_info("clkrate [ %s \t] : %lu\n", clk_name,
				_get_rate(clk_name));
		}
	}
	pr_info("meson8 clock initialization complete\n");
}
CLK_OF_DECLARE(meson8, "amlogic,meson8m2-clock", meson8_clk_init);
