/*
 * drivers/amlogic/clk/tm2/tm2.c
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

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <dt-bindings/clock/amlogic,tl1-clkc.h>

#include "../clkc.h"
#include "../tl1/tl1.h"
#include "tm2.h"

static const struct pll_rate_table tm2_pcie_pll_rate_table[] = {
	PLL_RATE(100000000, 200, 1, 12),
	{ /* sentinel */ },
};

static struct meson_clk_pll tm2_pcie_pll = {
	.m = {
		.reg_off = HHI_PCIE_PLL_CNTL0,
		.shift   = 0,
		.width   = 8,
	},
	.n = {
		.reg_off = HHI_PCIE_PLL_CNTL0,
		.shift   = 10,
		.width   = 5,
	},
	.od = {
		.reg_off = HHI_PCIE_PLL_CNTL0,
		.shift   = 16,
		.width   = 5,
	},
	.frac = {
		.reg_off = HHI_PCIE_PLL_CNTL1,
		.shift   = 0,
		.width   = 12,
	},
	.rate_table = tm2_pcie_pll_rate_table,
	.rate_count = ARRAY_SIZE(tm2_pcie_pll_rate_table),
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "pcie_pll",
		.ops = &meson_tl1_pll_ops,
		.parent_names = (const char *[]){ "xtal" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_gate tm2_pcie01_enable = {
	.reg = (void *)HHI_PCIE_PLL_CNTL1,
	.bit_idx = 29,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "tm2_pcie01",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "pcie_pll" },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_gate tm2_pcie0_gate = {
	.reg = (void *)HHI_PCIE_PLL_CNTL5,
	.bit_idx = 3,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "tm2_pcie0_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "tm2_pcie01" },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_gate tm2_pcie1_gate = {
	.reg = (void *)HHI_PCIE_PLL_CNTL1,
	.bit_idx = 28,
	.lock = &clk_lock,
	.hw.init = &(struct clk_init_data){
		.name = "tm2_pcie1_gate",
		.ops = &clk_gate_ops,
		.parent_names = (const char *[]){ "tm2_pcie01" },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

/* clk81 gate for tm2 */
static MESON_GATE(tm2_vipnanoq, HHI_GCLK_MPEG1, 19);
static MESON_GATE(tm2_pcie1, HHI_GCLK_MPEG1, 24);
static MESON_GATE(tm2_pcie1phy, HHI_GCLK_MPEG1, 27);
static MESON_GATE(tm2_parserl, HHI_GCLK_MPEG1, 28);
static MESON_GATE(tm2_hdcp22_pclk, HHI_GCLK_MPEG2, 3);
static MESON_GATE(tm2_hdmitx_pclk, HHI_GCLK_MPEG2, 4);
static MESON_GATE(tm2_pcie0, HHI_GCLK_MPEG2, 6);
static MESON_GATE(tm2_pcie0phy, HHI_GCLK_MPEG2, 7);
static MESON_GATE(tm2_hdmirx_axi_pclk, HHI_GCLK_MPEG2, 12);
static MESON_GATE(tm2_dspb, HHI_GCLK_MPEG2, 26);
static MESON_GATE(tm2_dspa, HHI_GCLK_MPEG2, 27);

PNAME(dsp_parent_names) = { "fclk_div2", "fclk_div3",
	"fclk_div5", "fclk_div7", "xtal", "gp0_pll", "gp1_pll", "hifi_pll" };

static MUX(dspa_clk_a_mux, HHI_DSP_CLK_CNTL, 0x7, 4,
	dsp_parent_names, CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static DIV(dspa_clk_a_div, HHI_DSP_CLK_CNTL, 0, 4, "dspa_clk_a_mux",
	CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED | CLK_SET_RATE_PARENT);
static GATE(dspa_clk_a_gate, HHI_DSP_CLK_CNTL, 7, "dspa_clk_a_div",
	CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED | CLK_SET_RATE_PARENT);

static MUX(dspa_clk_b_mux, HHI_DSP_CLK_CNTL, 0x7, 12,
	dsp_parent_names, CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static DIV(dspa_clk_b_div, HHI_DSP_CLK_CNTL, 8, 4, "dspa_clk_b_mux",
	CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED | CLK_SET_RATE_PARENT);
static GATE(dspa_clk_b_gate, HHI_DSP_CLK_CNTL, 7, "dspa_clk_b_div",
	CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED | CLK_SET_RATE_PARENT);

PNAME(dspa_parent_names) = { "dspa_clk_a_gate",
				"dspa_clk_b_gate" };

static MESON_MUX(dspa_clk_mux, HHI_DSP_CLK_CNTL, 0x1, 15,
	dspa_parent_names, CLK_GET_RATE_NOCACHE);


static MUX(dspb_clk_a_mux, HHI_DSP_CLK_CNTL, 0x7, 20,
	dsp_parent_names, CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static DIV(dspb_clk_a_div, HHI_DSP_CLK_CNTL, 16, 4, "dspb_clk_a_mux",
	CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED | CLK_SET_RATE_PARENT);
static GATE(dspb_clk_a_gate, HHI_DSP_CLK_CNTL, 23, "dspb_clk_a_div",
	CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED | CLK_SET_RATE_PARENT);

static MUX(dspb_clk_b_mux, HHI_DSP_CLK_CNTL, 0x7, 28,
	dsp_parent_names, CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED);
static DIV(dspb_clk_b_div, HHI_DSP_CLK_CNTL, 24, 4, "dspb_clk_b_mux",
	CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED | CLK_SET_RATE_PARENT);
static GATE(dspb_clk_b_gate, HHI_DSP_CLK_CNTL, 23, "dspb_clk_b_div",
	CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED | CLK_SET_RATE_PARENT);

PNAME(dspb_parent_names) = { "dspb_clk_a_gate",
				"dspb_clk_b_gate" };

static MESON_MUX(dspb_clk_mux, HHI_DSP_CLK_CNTL, 0x1, 31,
	dspb_parent_names, CLK_GET_RATE_NOCACHE);

static struct clk_gate *tm2_clk_gates[] = {
	&tm2_vipnanoq,
	&tm2_pcie1,
	&tm2_pcie1phy,
	&tm2_parserl,
	&tm2_hdcp22_pclk,
	&tm2_hdmitx_pclk,
	&tm2_pcie0phy,
	&tm2_hdmirx_axi_pclk,
	&tm2_dspb,
	&tm2_dspa,
	&dspa_clk_a_gate,
	&dspa_clk_b_gate,
	&dspb_clk_a_gate,
	&dspb_clk_b_gate,
	&tm2_pcie0_gate,
	&tm2_pcie1_gate,
	&tm2_pcie0,
	&tm2_pcie01_enable,
};

static struct clk_mux *tm2_clk_mux[] = {
	&dspa_clk_a_mux,
	&dspa_clk_b_mux,
	&dspb_clk_a_mux,
	&dspb_clk_b_mux,
	&dspa_clk_mux,
	&dspb_clk_mux,
};

static struct clk_divider *tm2_clk_divs[] = {
	&dspa_clk_a_div,
	&dspa_clk_b_div,
	&dspb_clk_a_div,
	&dspb_clk_b_div,
};

/* Array of all clocks provided by this provider */
static struct clk_hw *tm2_clk_hws[] = {
	[CLKID_PCIE_PLL]	= &tm2_pcie_pll.hw,
	[CLKID_VIPNANOQ]	= &tm2_vipnanoq.hw,
	[CLKID_PCIE0]		= &tm2_pcie0.hw,
	[CLKID_PCIE1]		= &tm2_pcie1.hw,
	[CLKID_PCIE1PHY]	= &tm2_pcie1phy.hw,
	[CLKID_PARSER1]		= &tm2_parserl.hw,
	[CLKID_HDCP22_PCLK]	= &tm2_hdcp22_pclk.hw,
	[CLKID_HDMITX_PCLK]	= &tm2_hdmitx_pclk.hw,
	[CLKID_PCIE0PHY]	= &tm2_pcie0phy.hw,
	[CLKID_HDMITX_AXI_PCLK]	= &tm2_hdmirx_axi_pclk.hw,
	[CLKID_DSPB]		= &tm2_dspb.hw,
	[CLKID_DSPA]		= &tm2_dspa.hw,
	[CLKID_DSPA_MUX_A]	= &dspa_clk_a_mux.hw,
	[CLKID_DSPA_DIV_A]	= &dspa_clk_a_div.hw,
	[CLKID_DSPA_GATE_A]	= &dspa_clk_a_gate.hw,
	[CLKID_DSPA_MUX_B]	= &dspa_clk_b_mux.hw,
	[CLKID_DSPA_DIV_B]	= &dspa_clk_b_div.hw,
	[CLKID_DSPA_GATE_B]	= &dspa_clk_b_gate.hw,
	[CLKID_DSPA_MUX]	= &dspa_clk_mux.hw,
	[CLKID_DSPB_MUX_A]	= &dspb_clk_a_mux.hw,
	[CLKID_DSPB_DIV_A]	= &dspb_clk_a_div.hw,
	[CLKID_DSPB_GATE_A]	= &dspb_clk_a_gate.hw,
	[CLKID_DSPB_MUX_B]	= &dspb_clk_b_mux.hw,
	[CLKID_DSPB_DIV_B]	= &dspb_clk_b_div.hw,
	[CLKID_DSPB_GATE_B]	= &dspb_clk_b_gate.hw,
	[CLKID_DSPB_MUX]	= &dspb_clk_mux.hw,
	[CLKID_PCIE01_ENABLE] = &tm2_pcie01_enable.hw,
	[CLKID_PCIE0_GATE]	= &tm2_pcie0_gate.hw,
	[CLKID_PCIE1_GATE]	= &tm2_pcie1_gate.hw,
};

static void __init tm2_clkc_init(struct device_node *np)
{
	int clkid, i;

	if (!clk_base) {
		pr_err("tm2 clock basic clock driver not prepare\n");
		WARN_ON(IS_ERR(clk_base));
		return;
	}

	/* Populate base address for pcie pll */
	tm2_pcie_pll.base = clk_base;

	/* Populate base address for media muxes */
	for (i = 0; i < ARRAY_SIZE(tm2_clk_mux); i++)
		tm2_clk_mux[i]->reg = clk_base +
			(unsigned long)tm2_clk_mux[i]->reg;

	/* Populate base address for media divs */
	for (i = 0; i < ARRAY_SIZE(tm2_clk_divs); i++)
		tm2_clk_divs[i]->reg = clk_base +
			(unsigned long)tm2_clk_divs[i]->reg;

	/* Populate base address for gates */
	for (i = 0; i < ARRAY_SIZE(tm2_clk_gates); i++)
		tm2_clk_gates[i]->reg = clk_base +
			(unsigned long)tm2_clk_gates[i]->reg;

	/* register tm2 clks, pcie pll is the first clock index */
	for (clkid = CLKID_PCIE_PLL; clkid < GATE_BASE0; clkid++) {
		if (tm2_clk_hws[clkid]) {
			clks[clkid] = clk_register(NULL, tm2_clk_hws[clkid]);
			WARN_ON(IS_ERR(clks[clkid]));
		}
	}
}

CLK_OF_DECLARE(tm2, "amlogic,tm2-clkc", tm2_clkc_init);
