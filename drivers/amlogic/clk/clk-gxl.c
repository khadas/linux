/*
 * drivers/amlogic/clk/clk-gxl.c
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

#include <dt-bindings/clock/gxl.h>
#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/clk-provider.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include "mpll_clk.h"

#include "clk.h"

#undef pr_fmt
#define pr_fmt(fmt) "gxl_clk: " fmt

static void __iomem *reg_base_hiubus;
static void __iomem *reg_base_aobus;


#define	OFFSET(x)	(x << 2)
#undef	HHI_GCLK_MPEG0
#undef	HHI_MALI_CLK_CNTL
#undef	HHI_VAPBCLK_CNTL
#undef	HHI_XTAL_DIVN_CNTL

#define	HHI_GCLK_MPEG0			OFFSET(0x50)
#define	HHI_MALI_CLK_CNTL		OFFSET(0x6c)
#define	HHI_VAPBCLK_CNTL		OFFSET(0x7d)
#define	HHI_XTAL_DIVN_CNTL		OFFSET(0x2f)

#undef HHI_MPLL_CNTL
#define	HHI_MPLL_CNTL			OFFSET(0xa0)
#define	HHI_MPLL_CNTL7			OFFSET(0xa6)
#define	HHI_MPLL_CNTL8			OFFSET(0xa7)
#define	HHI_MPLL_CNTL9			OFFSET(0xa8)
#define	HHI_AUD_CLK_CNTL3		OFFSET(0x69)
#define	HHI_AUD_CLK_CNTL		OFFSET(0x5e)
#define	HHI_AUD_CLK_CNTL2		OFFSET(0x64)
#define	HHI_BT656_CLK_CNTL		OFFSET(0xf5)
#define	HHI_VDIN_MEAS_CLK_CNTL	OFFSET(0x94)
#define	HHI_VID_LOCK_CLK_CNTL		OFFSET(0xf2)
#define	HHI_PCM_CLK_CNTL		OFFSET(0x96)

#define gxl_RSTC_N_REGS	6
#define gxl_AO_OFF		((gxl_RSTC_N_REGS - 1) * BITS_PER_LONG + 4)
PNAME(mux_mali_0_p) = {"xtal", "gp0_pll", "mpll_clk_out1", "mpll_clk_out2",
		"fclk_div7", "fclk_div4", "fclk_div3", "fclk_div5"};
PNAME(mux_mali_1_p) = {"xtal", "gp0_pll", "mpll_clk_out1", "mpll_clk_out2",
		"fclk_div7", "fclk_div4", "fclk_div3", "fclk_div5"};
PNAME(mux_mali_p)   = {"clk_mali_0", "clk_mali_1"};
PNAME(mpll) = {"fixed_pll"};
PNAME(cts_pdm_p) = {"cts_amclk", "mpll_clk_out0",
				"mpll_clk_out1", "mpll_clk_out2"};
PNAME(cts_am_p) = {"ddr_pll_clk", "mpll_clk_out0", "mpll_clk_out1",
							"mpll_clk_out2"};
PNAME(cts_i958_p) = {"NULL", "mpll_clk_out0", "mpll_clk_out1", "mpll_clk_out2"};
PNAME(cts_spdif_p) = {"cts_amclk", "cts_i958"};

PNAME(mux_pcm_0_p) = {"mpll_clk_out0", "fclk_div4", "fclk_div3", "fclk_div5"};
PNAME(mux_pcm_1_p) = {"mux_pcm_0_p"};


PNAME(mux_vapb_0_p) = {"fclk_div4", "fclk_div3", "fclk_div5", "fclk_div7"};
PNAME(mux_vapb_1_p) = {"fclk_div4", "fclk_div3", "fclk_div5", "fclk_div7"};

PNAME(mux_ge2d_p) = {"clk_vapb_0", "clk_vapb_1"};

PNAME(cts_bt656_clk0_p) = {"fclk_div2", "fclk_div3", "fclk_div5", "fclk_div7"};

PNAME(cts_vid_lock_clk_p) = {"xtal", "cts_encl_clk", "cts_enci_clk",
							"cts_encp_clk"};
PNAME(cts_vdin_meas_clk_p) = {"xtal", "fclk_div4", "fclk_div3", "fclk_div5",
		"vid_pll_clk", "vid2_pll_clk"};

/* fixed rate clocks generated outside the soc */
static struct amlogic_fixed_rate_clock gxl_fixed_rate_ext_clks[] __initdata = {
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
static struct amlogic_mux_clock mux_clks[] __initdata = {
	MUX(CLK_MALI, "clk_mali", mux_mali_p, HHI_MALI_CLK_CNTL, 31, 1, 0),
	MUX(CLK_SPDIF, "clk_spdif", cts_spdif_p, HHI_AUD_CLK_CNTL2, 27, 1, 0)
};


static struct of_device_id ext_clk_match[] __initdata = {
	{ .compatible = "amlogic,clock-xtal", .data = (void *)0, },
	{},
};
static struct amlogic_clk_branch clk_branches[] __initdata = {
	COMPOSITE(CLK_MALI_0, "clk_mali_0", mux_mali_0_p,
		   CLK_SET_RATE_NO_REPARENT,
		   HHI_MALI_CLK_CNTL, 9, 3, 0,
		   HHI_MALI_CLK_CNTL, 0, 7,
		   CLK_DIVIDER_ROUND_CLOSEST,
		   HHI_MALI_CLK_CNTL, 8, 0),
	COMPOSITE(CLK_MALI_1, "clk_mali_1", mux_mali_1_p,
		   CLK_SET_RATE_NO_REPARENT,
		   HHI_MALI_CLK_CNTL, 25, 3, 0,
		   HHI_MALI_CLK_CNTL, 16, 7,
		   CLK_DIVIDER_ROUND_CLOSEST,
		   HHI_MALI_CLK_CNTL, 24, 0),
	COMPOSITE(CLK_VAPB_0, "clk_vapb_0", mux_vapb_0_p,
		   CLK_SET_RATE_NO_REPARENT,
		   HHI_VAPBCLK_CNTL, 9, 3, 0,
		   HHI_VAPBCLK_CNTL, 0, 7, 0,
		   HHI_VAPBCLK_CNTL, 8, 0),
	COMPOSITE(CLK_VAPB_1, "clk_vapb_1", mux_vapb_1_p,
		   CLK_SET_RATE_NO_REPARENT,
		   HHI_VAPBCLK_CNTL, 25, 3, 0,
		   HHI_VAPBCLK_CNTL, 16, 7, 0,
		   HHI_VAPBCLK_CNTL, 24, 0),
	COMPOSITE_NODIV(CLK_GE2D, "clk_ge2d", mux_ge2d_p,
		   CLK_SET_RATE_NO_REPARENT,
		   HHI_VAPBCLK_CNTL, 31, 1, 0,
		   HHI_VAPBCLK_CNTL, 30, 0),

	COMPOSITE(CLK_AMCLK, "cts_amclk", cts_am_p,
			CLK_SET_RATE_NO_REPARENT,
			HHI_AUD_CLK_CNTL, 9, 2, 0,
			HHI_AUD_CLK_CNTL, 0, 8,
			CLK_DIVIDER_ROUND_CLOSEST,
			HHI_AUD_CLK_CNTL, 8, 0),

	COMPOSITE(CLK_PDM, "cts_pdm", cts_pdm_p,
			CLK_SET_RATE_NO_REPARENT,
			HHI_AUD_CLK_CNTL3, 17, 2, 0,
			HHI_AUD_CLK_CNTL3, 0, 16,
			CLK_DIVIDER_ROUND_CLOSEST,
			HHI_AUD_CLK_CNTL3, 16, 0),

	COMPOSITE(CLK_I958, "cts_i958", cts_i958_p,
			CLK_SET_RATE_NO_REPARENT,
			HHI_AUD_CLK_CNTL2, 25, 2, 0,
			HHI_AUD_CLK_CNTL2, 16, 8,
			CLK_DIVIDER_ROUND_CLOSEST,
			HHI_AUD_CLK_CNTL2, 24, 0),

	COMPOSITE(CLK_PCM_MCLK, "clk_pcm_mclk", mux_pcm_0_p,
			CLK_SET_RATE_NO_REPARENT,
			HHI_PCM_CLK_CNTL, 10, 2, 0,
			HHI_PCM_CLK_CNTL, 0, 9,
			CLK_DIVIDER_ROUND_CLOSEST,
			HHI_PCM_CLK_CNTL, 9, 0),

	COMPOSITE_NOMUX(CLK_PCM_SCLK, "clk_pcm_sclk", mux_pcm_1_p,
			CLK_SET_RATE_NO_REPARENT,
			HHI_PCM_CLK_CNTL,
			HHI_PCM_CLK_CNTL, 16, 5,
			CLK_DIVIDER_ROUND_CLOSEST,
			HHI_PCM_CLK_CNTL, 22, 0),

	COMPOSITE(CLK_BT656_CLK0, "cts_bt656_clk0", cts_bt656_clk0_p,
			CLK_SET_RATE_NO_REPARENT,
			HHI_BT656_CLK_CNTL, 9, 2, 0,
			HHI_BT656_CLK_CNTL, 0, 7,
			CLK_DIVIDER_ROUND_CLOSEST,
			HHI_BT656_CLK_CNTL, 7, 0),

	COMPOSITE(CLK_BT656_CLK1, "cts_bt656_clk1", cts_bt656_clk0_p,
			CLK_SET_RATE_NO_REPARENT,
			HHI_BT656_CLK_CNTL, 25, 2, 0,
			HHI_BT656_CLK_CNTL, 16, 7,
			CLK_DIVIDER_ROUND_CLOSEST,
			HHI_BT656_CLK_CNTL, 23, 0),

	COMPOSITE(CLK_VID_LOCK_CLK, "cts_vid_lock_clk", cts_vid_lock_clk_p,
			CLK_SET_RATE_NO_REPARENT,
			HHI_VID_LOCK_CLK_CNTL, 8, 2, 0,
			HHI_VID_LOCK_CLK_CNTL, 0, 7, 0,
			HHI_VID_LOCK_CLK_CNTL, 7, 0),
	COMPOSITE(CLK_VDIN_MEAS_CLK, "cts_vdin_meas_clk", cts_vdin_meas_clk_p,
			CLK_SET_RATE_NO_REPARENT,
			HHI_VDIN_MEAS_CLK_CNTL, 9, 3, 0,
			HHI_VDIN_MEAS_CLK_CNTL, 0, 7, 0,
			HHI_VDIN_MEAS_CLK_CNTL, 8, 0),
};
static struct mpll_clk_tab mpll_tab[] __initdata = {
	MPLL("mpll_clk_out0", mpll, HHI_MPLL_CNTL7, HHI_MPLL_CNTL,
			CLK_MPLL0, CLK_SET_RATE_NO_REPARENT),
	MPLL("mpll_clk_out1", mpll, HHI_MPLL_CNTL8, 0,
			CLK_MPLL1, CLK_SET_RATE_NO_REPARENT),
	MPLL("mpll_clk_out2", mpll, HHI_MPLL_CNTL9, 0,
			CLK_MPLL2, CLK_SET_RATE_NO_REPARENT),

};

static struct amlogic_gate_clock clk_gates[] __initdata = {
	GATE(CLK_CAMERA_12M, "clk_camera_12", "xtal",
	HHI_XTAL_DIVN_CNTL, 11, CLK_SET_RATE_NO_REPARENT,
	0, 0),
	GATE(CLK_CAMERA_24M, "clk_camera_24", "xtal",
	HHI_XTAL_DIVN_CNTL, 10, CLK_SET_RATE_NO_REPARENT,
	0, 0),
};

/* register gxl clocks */
static void __init gxl_clk_init(struct device_node *np)
{

	reg_base_hiubus = of_iomap(np, 0);
	reg_base_aobus = of_iomap(np, 1);
	if ((!reg_base_hiubus) || (!reg_base_aobus))
		panic("%s: failed to map registers\n", __func__);

	pr_debug("HIU base is 0x%p\n", reg_base_hiubus);
	pr_debug("ao base is 0x%p\n", reg_base_aobus);

	amlogic_clk_init(np, reg_base_hiubus, reg_base_aobus,
			CLK_NR_CLKS, NULL, 0, NULL, 0);
	amlogic_clk_of_register_fixed_ext(gxl_fixed_rate_ext_clks,
		  ARRAY_SIZE(gxl_fixed_rate_ext_clks), ext_clk_match);
	mpll_clk_init(reg_base_hiubus, mpll_tab, ARRAY_SIZE(mpll_tab));
	amlogic_clk_register_mux(mux_clks,
			ARRAY_SIZE(mux_clks));
	amlogic_clk_register_branches(clk_branches,
		  ARRAY_SIZE(clk_branches));
	amlogic_clk_register_gate(clk_gates,
	  ARRAY_SIZE(clk_gates));
	meson_register_rstc(np, gxl_RSTC_N_REGS, reg_base_aobus,
		reg_base_hiubus + HHI_GCLK_MPEG0, gxl_AO_OFF, 0);
	sys_pll_init(reg_base_hiubus, np, CLK_SYS_PLL);
	gp0_clk_gxl_init(reg_base_hiubus, GP0_PLL);

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
		struct clk *vapb;
		struct clk *fixdiv5;

		struct clk *clk_mali_0;
		struct clk *clk_mali;

		for (i = 0; i < count; i++) {
			char *clk_name = clks[i];
			pr_info("[ %s \t] ->clockrate: %luHz\n", clk_name,
				_get_rate(clk_name));
		}

		/* Force set vapb clock as 400MHZ and enable it */
		fixdiv5 = clk_get_sys("fclk_div5", "fclk_div5");
		vapb = clk_get_sys("clk_vapb_0", "clk_vapb_0");
		if ((!IS_ERR(vapb)) && (!IS_ERR(fixdiv5))) {
			clk_set_parent(vapb, fixdiv5);
			clk_set_rate(vapb, 400000000);
			clk_prepare_enable(vapb);
		}
		if (!IS_ERR(fixdiv5))
			clk_put(fixdiv5);
		if (!IS_ERR(vapb))
			clk_put(vapb);

		/*Force set mali clock to 400M"*/
		clk_mali_0 = clk_get_sys("clk_mali_0", "clk_mali_0");
		clk_mali = clk_get_sys("clk_mali_0", "clk_mali_0");
		if ((!IS_ERR(clk_mali_0)) && (!IS_ERR(clk_mali))) {
			clk_set_parent(clk_mali_0, fixdiv5);
			clk_set_parent(clk_mali, clk_mali_0);
			clk_prepare_enable(clk_mali);
		}
		if (!IS_ERR(clk_mali_0))
			clk_put(clk_mali_0);
		if (!IS_ERR(clk_mali))
			clk_put(clk_mali);
		if (!IS_ERR(fixdiv5))
			clk_put(fixdiv5);

	}
	pr_info("clock initialization complete\n");
}
CLK_OF_DECLARE(gxl, "amlogic, gxl-clock", gxl_clk_init);
