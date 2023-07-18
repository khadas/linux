// SPDX-License-Identifier: GPL-2.0+
/*
 * Amlogic Meson-G12A Clock Controller Driver
 *
 * Copyright (c) 2016 Baylibre SAS.
 * Author: Michael Turquette <mturquette@baylibre.com>
 *
 * Copyright (c) 2018 Amlogic, inc.
 * Author: Qiufang Dai <qiufang.dai@amlogic.com>
 * Author: Jian Hu <jian.hu@amlogic.com>
 */

#include <linux/clk-provider.h>
#include <linux/init.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/module.h>
#include "clk-mpll.h"
#include "clk-pll.h"
#include "clk-regmap.h"
#include "clk-cpu-dyndiv.h"
#include "vid-pll-div.h"
#include "meson-eeclk.h"
#include "tl1.h"
#include "clkcs_init.h"

static DEFINE_SPINLOCK(meson_clk_lock);
static const struct clk_ops meson_pll_clk_no_ops = {};

static struct clk_regmap tl1_fixed_pll_dco = {
	.data = &(struct meson_clk_pll_data){
		.en = {
			.reg_off = HHI_FIX_PLL_CNTL0,
			.shift   = 28,
			.width   = 1,
		},
		.m = {
			.reg_off = HHI_FIX_PLL_CNTL0,
			.shift   = 0,
			.width   = 9,
		},
		.n = {
			.reg_off = HHI_FIX_PLL_CNTL0,
			.shift   = 10,
			.width   = 5,
		},
#ifdef CONFIG_ARM
		.od = {
			.reg_off = HHI_FIX_PLL_CNTL0,
			.shift   = 16,
			.width   = 2,
		},
#endif
		.frac = {
			.reg_off = HHI_FIX_PLL_CNTL1,
			.shift   = 0,
			.width   = 19,
		},
		.l = {
			.reg_off = HHI_FIX_PLL_CNTL0,
			.shift   = 31,
			.width   = 1,
		},
		.rst = {
			.reg_off = HHI_FIX_PLL_CNTL0,
			.shift   = 29,
			.width   = 1,
		},
	},
	.hw.init = &(struct clk_init_data){
		.name = "fixed_pll_dco",
		.ops = &meson_clk_pll_ro_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "xtal",
		},
		.num_parents = 1,
	},
};

#ifdef CONFIG_ARM
static struct clk_regmap tl1_fixed_pll = {
	.hw.init = &(struct clk_init_data){
		.name = "fixed_pll",
		.ops = &meson_pll_clk_no_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_fixed_pll_dco.hw
		},
		.num_parents = 1,
		},
	};
#else
static struct clk_regmap tl1_fixed_pll = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_FIX_PLL_CNTL0,
		.shift = 16,
		.width = 2,
		.flags = CLK_DIVIDER_POWER_OF_TWO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "fixed_pll",
		.ops = &clk_regmap_divider_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_fixed_pll_dco.hw
		},
		.num_parents = 1,
	},
};
#endif

#ifdef CONFIG_ARM
static const struct pll_params_table tl1_sys_pll_params_table[] = {
	PLL_PARAMS(200, 1, 2), /*DCO=4800M OD=1200M*/
	PLL_PARAMS(234, 1, 2), /*DCO=5616M OD=1404M*/
	PLL_PARAMS(125, 1, 1), /*DCO=3000M OD=1500M*/
	PLL_PARAMS(134, 1, 1), /*DCO=3216M OD=1608M*/
	PLL_PARAMS(142, 1, 1), /*DCO=3408M OD=1704M*/
	PLL_PARAMS(150, 1, 1), /*DCO=3600M OD=1800M*/
	PLL_PARAMS(159, 1, 1), /*DCO=3816M OD=1908M*/
	{ /* sentinel */ },
};
#else
static const struct pll_params_table tl1_sys_pll_params_table[] = {
	PLL_PARAMS(200, 1), /*DCO=4800M OD=1200M*/
	PLL_PARAMS(234, 1), /*DCO=5616M OD=1404M*/
	PLL_PARAMS(125, 1), /*DCO=3000M OD=1500M*/
	PLL_PARAMS(134, 1), /*DCO=3216M OD=1608M*/
	PLL_PARAMS(142, 1), /*DCO=3408M OD=1704M*/
	PLL_PARAMS(150, 1), /*DCO=3600M OD=1800M*/
	PLL_PARAMS(159, 1), /*DCO=3816M OD=1908M*/
	{ /* sentinel */ },
};
#endif

static struct clk_regmap tl1_sys_pll_dco = {
	.data = &(struct meson_clk_pll_data){
		.en = {
			.reg_off = HHI_SYS_PLL_CNTL0,
			.shift   = 28,
			.width   = 1,
		},
		.m = {
			.reg_off = HHI_SYS_PLL_CNTL0,
			.shift   = 0,
			.width   = 9,
		},
		.n = {
			.reg_off = HHI_SYS_PLL_CNTL0,
			.shift   = 10,
			.width   = 5,
		},
#ifdef CONFIG_ARM
		/* od for 32bit */
		.od = {
			.reg_off = HHI_SYS_PLL_CNTL0,
			.shift	 = 16,
			.width	 = 2,
		},
#endif
		.table = tl1_sys_pll_params_table,
		.l = {
			.reg_off = HHI_SYS_PLL_CNTL0,
			.shift   = 31,
			.width   = 1,
		},
		.rst = {
			.reg_off = HHI_SYS_PLL_CNTL0,
			.shift   = 29,
			.width   = 1,
		},
	},
	.hw.init = &(struct clk_init_data){
		.name = "sys_pll_dco",
		.ops = &meson_clk_pll_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "xtal",
		},
		.num_parents = 1,
		/* This clock feeds the CPU, avoid disabling it */
		.flags = CLK_IGNORE_UNUSED
	},
};

#ifdef CONFIG_ARM
static struct clk_regmap tl1_sys_pll = {
	.hw.init = &(struct clk_init_data){
		.name = "sys_pll",
		.ops = &meson_pll_clk_no_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_sys_pll_dco.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};
#else
static struct clk_regmap tl1_sys_pll = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_SYS_PLL_CNTL0,
		.shift = 16,
		.width = 2,
		.flags = CLK_DIVIDER_POWER_OF_TWO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sys_pll",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_sys_pll_dco.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};
#endif

static struct clk_regmap tl1_sys_pll_div16_en = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_SYS_CPU_CLK_CNTL1,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sys_pll_div16_en",
		.ops = &clk_regmap_gate_ro_ops,
		.parent_hws = (const struct clk_hw *[]) { &tl1_sys_pll.hw },
		.num_parents = 1,
		/*
		 * This clock is used to debug the sys_pll range
		 * Linux should not change it at runtime
		 */
	},
};

static struct clk_fixed_factor tl1_sys_pll_div16 = {
	.mult = 1,
	.div = 16,
	.hw.init = &(struct clk_init_data){
		.name = "sys_pll_div16",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_sys_pll_div16_en.hw
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor tl1_fclk_div2_div = {
	.mult = 1,
	.div = 2,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div2_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) { &tl1_fixed_pll.hw },
		.num_parents = 1,
	},
};

static struct clk_regmap tl1_fclk_div2 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_FIX_PLL_CNTL1,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div2",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_fclk_div2_div.hw
		},
		.num_parents = 1,
		/*
		 * This clock feeds on CPU clock, it should be set
		 * by the platform to operate correctly.
		 */
		.flags = CLK_IS_CRITICAL,
	},
};

static struct clk_fixed_factor tl1_fclk_div3_div = {
	.mult = 1,
	.div = 3,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div3_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) { &tl1_fixed_pll.hw },
		.num_parents = 1,
	},
};

static struct clk_regmap tl1_fclk_div3 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_FIX_PLL_CNTL1,
		.bit_idx = 20,
	},
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div3",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_fclk_div3_div.hw
		},
		.num_parents = 1,
		/*
		 * This clock is used by the resident firmware and is required
		 * by the platform to operate correctly.
		 * Until the following condition are met, we need this clock to
		 * be marked as critical:
		 * a) Mark the clock used by a firmware resource, if possible
		 * b) CCF has a clock hand-off mechanism to make the sure the
		 *    clock stays on until the proper driver comes along
		 */
		.flags = CLK_IS_CRITICAL,
	},
};

static struct clk_fixed_factor tl1_fclk_div4_div = {
	.mult = 1,
	.div = 4,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div4_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) { &tl1_fixed_pll.hw },
		.num_parents = 1,
	},
};

static struct clk_regmap tl1_fclk_div4 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_FIX_PLL_CNTL1,
		.bit_idx = 21,
	},
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div4",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_fclk_div4_div.hw
		},
		.num_parents = 1,
		/*
		 * This clock feeds on GPU, it should be set
		 * by the platform to operate correctly.
		 */
		.flags = CLK_IS_CRITICAL,
	},
};

static struct clk_fixed_factor tl1_fclk_div5_div = {
	.mult = 1,
	.div = 5,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div5_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) { &tl1_fixed_pll.hw },
		.num_parents = 1,
	},
};

static struct clk_regmap tl1_fclk_div5 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_FIX_PLL_CNTL1,
		.bit_idx = 22,
	},
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div5",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_fclk_div5_div.hw
		},
		.num_parents = 1,
		/*
		 * This clock feeds on GPU, it should be set
		 * by the platform to operate correctly.
		 */
		.flags = CLK_IS_CRITICAL,
	},
};

static struct clk_fixed_factor tl1_fclk_div7_div = {
	.mult = 1,
	.div = 7,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div7_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) { &tl1_fixed_pll.hw },
		.num_parents = 1,
	},
};

static struct clk_regmap tl1_fclk_div7 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_FIX_PLL_CNTL1,
		.bit_idx = 23,
	},
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div7",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_fclk_div7_div.hw
		},
		.num_parents = 1,
		/*
		 * This clock feeds on GPU, it should be set
		 * by the platform to operate correctly.
		 */
		.flags = CLK_IS_CRITICAL,
	},
};

static struct clk_fixed_factor tl1_fclk_div2p5_div = {
	.mult = 1,
	.div = 5,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div2p5_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_fixed_pll_dco.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap tl1_fclk_div2p5 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_FIX_PLL_CNTL1,
		.bit_idx = 25,
	},
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div2p5",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_fclk_div2p5_div.hw
		},
		.num_parents = 1,
		/*
		 * This clock feeds on GPU, it should be set
		 * by the platform to operate correctly.
		 */
		.flags = CLK_IS_CRITICAL,
	},
};

#ifdef CONFIG_ARM
static const struct pll_params_table tl1_gp0_pll_table[] = {
	PLL_PARAMS(141, 1, 2), /* DCO = 3384M OD = 2 PLL = 846M */
	PLL_PARAMS(132, 1, 2), /* DCO = 3168M OD = 2 PLL = 792M */
	PLL_PARAMS(248, 1, 3), /* DCO = 5952M OD = 3 PLL = 744M */
	{ /* sentinel */  },
};
#else
static const struct pll_params_table tl1_gp0_pll_table[] = {
	PLL_PARAMS(141, 1), /* DCO = 3384M OD = 2 PLL = 846M*/
	PLL_PARAMS(132, 1), /* DCO = 3168M OD = 2 PLL = 792M */
	PLL_PARAMS(248, 1), /* DCO = 5952M OD = 3 PLL = 744M */
	{0, 0},
};
#endif

/*
 * Internal gp0 pll emulation configuration parameters
 */
static const struct reg_sequence tl1_gp0_init_regs[] = {
	{ .reg = HHI_GP0_PLL_CNTL1,	.def = 0x00000000 },
	{ .reg = HHI_GP0_PLL_CNTL2,	.def = 0x00000000 },
	{ .reg = HHI_GP0_PLL_CNTL3,	.def = 0x48681c00 },
	{ .reg = HHI_GP0_PLL_CNTL4,	.def = 0x33771290 },
	{ .reg = HHI_GP0_PLL_CNTL5,	.def = 0x39272000 },
	{ .reg = HHI_GP0_PLL_CNTL6,	.def = 0x56540000 },
};

static struct clk_regmap tl1_gp0_pll_dco = {
	.data = &(struct meson_clk_pll_data){
		.en = {
			.reg_off = HHI_GP0_PLL_CNTL0,
			.shift   = 28,
			.width   = 1,
		},
		.m = {
			.reg_off = HHI_GP0_PLL_CNTL0,
			.shift   = 0,
			.width   = 9,
		},
		.n = {
			.reg_off = HHI_GP0_PLL_CNTL0,
			.shift   = 10,
			.width   = 5,
		},
#ifdef CONFIG_ARM
		.od = {
			.reg_off = HHI_GP0_PLL_CNTL0,
			.shift	 = 16,
			.width	 = 3,
		},
#endif
		.frac = {
			.reg_off = HHI_GP0_PLL_CNTL1,
			.shift   = 0,
			.width   = 19,
		},
		.l = {
			.reg_off = HHI_GP0_PLL_CNTL0,
			.shift   = 31,
			.width   = 1,
		},
		.rst = {
			.reg_off = HHI_GP0_PLL_CNTL0,
			.shift   = 29,
			.width   = 1,
		},
		.table = tl1_gp0_pll_table,
		.init_regs = tl1_gp0_init_regs,
		.init_count = ARRAY_SIZE(tl1_gp0_init_regs),
	},
	.hw.init = &(struct clk_init_data){
		.name = "gp0_pll_dco",
		.ops = &meson_clk_pll_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "xtal",
		},
		.num_parents = 1,
	},
};

#ifdef CONFIG_ARM
static const struct pll_params_table tl1_gp1_pll_params_table[] = {
	PLL_PARAMS(200, 1, 2), /*DCO=4800M OD=1200M*/
	PLL_PARAMS(125, 1, 1), /*DCO=3000M OD=1500M*/
	{ /* sentinel */ },
};
#else
static const struct pll_params_table tl1_gp1_pll_params_table[] = {
	PLL_PARAMS(200, 1), /*DCO=4800M OD=1200M*/
	PLL_PARAMS(125, 1), /*DCO=3000M OD=1500M*/
	{ /* sentinel */ },
};
#endif

static struct clk_regmap tl1_gp1_pll_dco = {
	.data = &(struct meson_clk_pll_data){
		.en = {
			.reg_off = HHI_GP1_PLL_CNTL0,
			.shift   = 28,
			.width   = 1,
		},
		.m = {
			.reg_off = HHI_GP1_PLL_CNTL0,
			.shift   = 0,
			.width   = 9,
		},
		.n = {
			.reg_off = HHI_GP1_PLL_CNTL0,
			.shift   = 10,
			.width   = 5,
		},
#ifdef CONFIG_ARM
		.od = {
			.reg_off = HHI_GP1_PLL_CNTL0,
			.shift	 = 16,
			.width	 = 3,
		},
#endif
		.l = {
			.reg_off = HHI_GP1_PLL_CNTL0,
			.shift   = 31,
			.width   = 1,
		},
		.rst = {
			.reg_off = HHI_GP1_PLL_CNTL0,
			.shift   = 29,
			.width   = 1,
		},
		.table = tl1_gp1_pll_params_table
	},
	.hw.init = &(struct clk_init_data){
		.name = "gp1_pll_dco",
		.ops = &meson_clk_pll_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "xtal",
		},
		.num_parents = 1,
		/* This clock feeds the DSU, avoid disabling it */
		.flags = CLK_IGNORE_UNUSED,
	},
};

#ifdef CONFIG_ARM
static struct clk_regmap tl1_gp0_pll = {
	.hw.init = &(struct clk_init_data){
		.name = "gp0_pll",
		.ops = &meson_pll_clk_no_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_gp0_pll_dco.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tl1_gp1_pll = {
	.hw.init = &(struct clk_init_data){
		.name = "gp1_pll",
		.ops = &meson_pll_clk_no_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_gp1_pll_dco.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};
#else
static struct clk_regmap tl1_gp0_pll = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_GP0_PLL_CNTL0,
		.shift = 16,
		.width = 3,
		.flags = (CLK_DIVIDER_POWER_OF_TWO |
			  CLK_DIVIDER_ROUND_CLOSEST),
	},
	.hw.init = &(struct clk_init_data){
		.name = "gp0_pll",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_gp0_pll_dco.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tl1_gp1_pll = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_GP1_PLL_CNTL0,
		.shift = 16,
		.width = 3,
		.flags = (CLK_DIVIDER_POWER_OF_TWO |
			  CLK_DIVIDER_ROUND_CLOSEST),
	},
	.hw.init = &(struct clk_init_data){
		.name = "gp1_pll",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_gp1_pll_dco.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};
#endif

/*
 * Internal hifi pll emulation configuration parameters
 */
static const struct reg_sequence tl1_hifi_init_regs[] = {
	{ .reg = HHI_HIFI_PLL_CNTL1,	.def = 0x00010E56 },
	{ .reg = HHI_HIFI_PLL_CNTL2,	.def = 0x00000000 },
	{ .reg = HHI_HIFI_PLL_CNTL3,	.def = 0x6a285c00 },
	{ .reg = HHI_HIFI_PLL_CNTL4,	.def = 0x65771290 },
	{ .reg = HHI_HIFI_PLL_CNTL5,	.def = 0x39272000 },
	{ .reg = HHI_HIFI_PLL_CNTL6,	.def = 0x56540000 },
};

#ifdef CONFIG_ARM
static const struct pll_params_table tl1_hifi_pll_params_table[] = {
	PLL_PARAMS(150, 1, 1), /*DCO=3600M OD=1800M*/
	{ /* sentinel */ },
};
#else
static const struct pll_params_table tl1_hifi_pll_params_table[] = {
	PLL_PARAMS(150, 1), /*DCO=3600M OD=1800M*/
	{ /* sentinel */ },
};
#endif

static struct clk_regmap tl1_hifi_pll_dco = {
	.data = &(struct meson_clk_pll_data){
		.en = {
			.reg_off = HHI_HIFI_PLL_CNTL0,
			.shift   = 28,
			.width   = 1,
		},
		.m = {
			.reg_off = HHI_HIFI_PLL_CNTL0,
			.shift   = 0,
			.width   = 9,
		},
		.n = {
			.reg_off = HHI_HIFI_PLL_CNTL0,
			.shift   = 10,
			.width   = 5,
		},
#ifdef CONFIG_ARM
		.od = {
			.reg_off = HHI_HIFI_PLL_CNTL0,
			.shift	 = 16,
			.width	 = 2,
		},
#endif
		.frac = {
			.reg_off = HHI_HIFI_PLL_CNTL1,
			.shift   = 0,
			.width   = 19,
		},
		.l = {
			.reg_off = HHI_HIFI_PLL_CNTL0,
			.shift   = 31,
			.width   = 1,
		},
		.rst = {
			.reg_off = HHI_HIFI_PLL_CNTL0,
			.shift   = 29,
			.width   = 1,
		},
		.table = tl1_hifi_pll_params_table,
		.init_regs = tl1_hifi_init_regs,
		.init_count = ARRAY_SIZE(tl1_hifi_init_regs),
		.flags = CLK_MESON_PLL_ROUND_CLOSEST,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hifi_pll_dco",
		.ops = &meson_clk_pll_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "xtal",
		},
		.num_parents = 1,
	},
};

#ifdef CONFIG_ARM
static struct clk_regmap tl1_hifi_pll = {
	.hw.init = &(struct clk_init_data){
		.name = "hifi_pll",
		.ops = &meson_pll_clk_no_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_hifi_pll_dco.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};
#else
static struct clk_regmap tl1_hifi_pll = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_HIFI_PLL_CNTL0,
		.shift = 16,
		.width = 2,
		.flags = (CLK_DIVIDER_POWER_OF_TWO |
			  CLK_DIVIDER_ROUND_CLOSEST),
	},
	.hw.init = &(struct clk_init_data){
		.name = "hifi_pll",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_hifi_pll_dco.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};
#endif

static struct clk_fixed_factor tl1_mpll_50m_div = {
	.mult = 1,
	.div = 80,
	.hw.init = &(struct clk_init_data){
		.name = "mpll_50m_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_fixed_pll_dco.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap tl1_mpll_50m = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_FIX_PLL_CNTL3,
		.mask = 0x1,
		.shift = 5,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpll_50m",
		.ops = &clk_regmap_mux_ro_ops,
		.parent_data = (const struct clk_parent_data []) {
			{ .fw_name = "xtal", },
			{ .hw = &tl1_mpll_50m_div.hw },
		},
		.num_parents = 2,
	},
};

static struct clk_fixed_factor tl1_mpll_prediv = {
	.mult = 1,
	.div = 2,
	.hw.init = &(struct clk_init_data){
		.name = "mpll_prediv",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_fixed_pll_dco.hw
		},
		.num_parents = 1,
	},
};

/*
 * put it in init regs, in kernel 4.9 it is dealing in set rate
 */
static const struct reg_sequence tl1_mpll0_init_regs[] = {
	{ .reg = HHI_MPLL_CNTL0,	.def = 0x00000543 },
	{ .reg = HHI_MPLL_CNTL2,	.def = 0x40000033 },
};

static struct clk_regmap tl1_mpll0_div = {
	.data = &(struct meson_clk_mpll_data){
		.sdm = {
			.reg_off = HHI_MPLL_CNTL1,
			.shift   = 0,
			.width   = 14,
		},
		.sdm_en = {
			.reg_off = HHI_MPLL_CNTL1,
			.shift   = 30,
			.width	 = 1,
		},
		.n2 = {
			.reg_off = HHI_MPLL_CNTL1,
			.shift   = 20,
			.width   = 9,
		},
		.ssen = {
			.reg_off = HHI_MPLL_CNTL1,
			.shift   = 29,
			.width	 = 1,
		},
		.lock = &meson_clk_lock,
		.init_regs = tl1_mpll0_init_regs,
		.init_count = ARRAY_SIZE(tl1_mpll0_init_regs),
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpll0_div",
		.ops = &meson_clk_mpll_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_mpll_prediv.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap tl1_mpll0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_MPLL_CNTL1,
		.bit_idx = 31,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpll0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &tl1_mpll0_div.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct reg_sequence tl1_mpll1_init_regs[] = {
	{ .reg = HHI_MPLL_CNTL4,	.def = 0x40000033 },
};

static struct clk_regmap tl1_mpll1_div = {
	.data = &(struct meson_clk_mpll_data){
		.sdm = {
			.reg_off = HHI_MPLL_CNTL3,
			.shift   = 0,
			.width   = 14,
		},
		.sdm_en = {
			.reg_off = HHI_MPLL_CNTL3,
			.shift   = 30,
			.width	 = 1,
		},
		.n2 = {
			.reg_off = HHI_MPLL_CNTL3,
			.shift   = 20,
			.width   = 9,
		},
		.ssen = {
			.reg_off = HHI_MPLL_CNTL3,
			.shift   = 29,
			.width	 = 1,
		},
		.lock = &meson_clk_lock,
		.init_regs = tl1_mpll1_init_regs,
		.init_count = ARRAY_SIZE(tl1_mpll1_init_regs),
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpll1_div",
		.ops = &meson_clk_mpll_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_mpll_prediv.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap tl1_mpll1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_MPLL_CNTL3,
		.bit_idx = 31,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpll1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &tl1_mpll1_div.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct reg_sequence tl1_mpll2_init_regs[] = {
	{ .reg = HHI_MPLL_CNTL6,	.def = 0x40000033 },
};

static struct clk_regmap tl1_mpll2_div = {
	.data = &(struct meson_clk_mpll_data){
		.sdm = {
			.reg_off = HHI_MPLL_CNTL5,
			.shift   = 0,
			.width   = 14,
		},
		.sdm_en = {
			.reg_off = HHI_MPLL_CNTL5,
			.shift   = 30,
			.width	 = 1,
		},
		.n2 = {
			.reg_off = HHI_MPLL_CNTL5,
			.shift   = 20,
			.width   = 9,
		},
		.ssen = {
			.reg_off = HHI_MPLL_CNTL5,
			.shift   = 29,
			.width	 = 1,
		},
		.lock = &meson_clk_lock,
		.init_regs = tl1_mpll2_init_regs,
		.init_count = ARRAY_SIZE(tl1_mpll2_init_regs),
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpll2_div",
		.ops = &meson_clk_mpll_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_mpll_prediv.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap tl1_mpll2 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_MPLL_CNTL5,
		.bit_idx = 31,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpll2",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &tl1_mpll2_div.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct reg_sequence tl1_mpll3_init_regs[] = {
	{ .reg = HHI_MPLL_CNTL8,	.def = 0x40000033 },
};

static struct clk_regmap tl1_mpll3_div = {
	.data = &(struct meson_clk_mpll_data){
		.sdm = {
			.reg_off = HHI_MPLL_CNTL7,
			.shift   = 0,
			.width   = 14,
		},
		.sdm_en = {
			.reg_off = HHI_MPLL_CNTL7,
			.shift   = 30,
			.width	 = 1,
		},
		.n2 = {
			.reg_off = HHI_MPLL_CNTL7,
			.shift   = 20,
			.width   = 9,
		},
		.ssen = {
			.reg_off = HHI_MPLL_CNTL7,
			.shift   = 29,
			.width	 = 1,
		},
		.lock = &meson_clk_lock,
		.init_regs = tl1_mpll3_init_regs,
		.init_count = ARRAY_SIZE(tl1_mpll3_init_regs),
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpll3_div",
		.ops = &meson_clk_mpll_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_mpll_prediv.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap tl1_mpll3 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_MPLL_CNTL7,
		.bit_idx = 31,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpll3",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &tl1_mpll3_div.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static u32 mux_table_clk81[]	= { 0, 2, 3, 4, 5, 6, 7 };
static const struct clk_parent_data clk81_parent_data[] = {
	{ .fw_name = "xtal", },
	{ .hw = &tl1_fclk_div7.hw },
	{ .hw = &tl1_mpll1.hw },
	{ .hw = &tl1_mpll2.hw },
	{ .hw = &tl1_fclk_div4.hw },
	{ .hw = &tl1_fclk_div3.hw },
	{ .hw = &tl1_fclk_div5.hw },
};

static struct clk_regmap tl1_mpeg_clk_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_MPEG_CLK_CNTL,
		.mask = 0x7,
		.shift = 12,
		.table = mux_table_clk81,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpeg_clk_sel",
		.ops = &clk_regmap_mux_ro_ops,
		.parent_data = clk81_parent_data,
		.num_parents = ARRAY_SIZE(clk81_parent_data),
	},
};

static struct clk_regmap tl1_mpeg_clk_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_MPEG_CLK_CNTL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpeg_clk_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_mpeg_clk_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tl1_clk81 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_MPEG_CLK_CNTL,
		.bit_idx = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "clk81",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_mpeg_clk_div.hw
		},
		.num_parents = 1,
		.flags = CLK_IS_CRITICAL,
	},
};

static const struct clk_parent_data tl1_sd_emmc_parent_data[] = {
	{ .fw_name = "xtal", },
	{ .hw = &tl1_fclk_div2.hw },
	{ .hw = &tl1_fclk_div3.hw },
	{ .hw = &tl1_fclk_div5.hw },
	{ .hw = &tl1_fclk_div2p5.hw },
	/*
	 * Following these parent clocks, we should also have had mpll2, mpll3
	 * and gp0_pll but these clocks are too precious to be used here. All
	 * the necessary rates for MMC and NAND operation can be achieved using
	 * tl1_ee_core or fclk_div clocks
	 */
};

/* SDcard clock */
static struct clk_regmap tl1_sd_emmc_b_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_SD_EMMC_CLK_CNTL,
		.mask = 0x7,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_b_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = tl1_sd_emmc_parent_data,
		.num_parents = ARRAY_SIZE(tl1_sd_emmc_parent_data),
	},
};

static struct clk_regmap tl1_sd_emmc_b_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_SD_EMMC_CLK_CNTL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_b_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_sd_emmc_b_sel.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap tl1_sd_emmc_b = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_SD_EMMC_CLK_CNTL,
		.bit_idx = 23,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sd_emmc_b",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_sd_emmc_b_div.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap tl1_sd_emmc_c_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_NAND_CLK_CNTL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_c_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = tl1_sd_emmc_parent_data,
		.num_parents = ARRAY_SIZE(tl1_sd_emmc_parent_data),
	},
};

static struct clk_regmap tl1_sd_emmc_c_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_NAND_CLK_CNTL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_c_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_sd_emmc_c_sel.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap tl1_sd_emmc_c = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_NAND_CLK_CNTL,
		.bit_idx = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sd_emmc_c",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_sd_emmc_c_div.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap tl1_hdmi_pll_dco = {
	.data = &(struct meson_clk_pll_data){
		.en = {
			.reg_off = HHI_HDMI_PLL_CNTL0,
			.shift   = 28,
			.width   = 1,
		},
		.m = {
			.reg_off = HHI_HDMI_PLL_CNTL0,
			.shift   = 0,
			.width   = 8,
		},
		.n = {
			.reg_off = HHI_HDMI_PLL_CNTL0,
			.shift   = 10,
			.width   = 5,
		},
		.frac = {
			.reg_off = HHI_HDMI_PLL_CNTL1,
			.shift   = 0,
			.width   = 16,
		},
		.l = {
			.reg_off = HHI_HDMI_PLL_CNTL0,
			.shift   = 30,
			.width   = 1,
		},
		.rst = {
			.reg_off = HHI_HDMI_PLL_CNTL0,
			.shift   = 29,
			.width   = 1,
		},
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmi_pll_dco",
		.ops = &meson_clk_pll_ro_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "xtal",
		},
		.num_parents = 1,
		/*
		 * Display directly handle hdmi pll registers ATM, we need
		 * NOCACHE to keep our view of the clock as accurate as possible
		 */
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap tl1_hdmi_pll_od = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_HDMI_PLL_CNTL0,
		.shift = 16,
		.width = 2,
		.flags = CLK_DIVIDER_POWER_OF_TWO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmi_pll_od",
		.ops = &clk_regmap_divider_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_hdmi_pll_dco.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tl1_hdmi_pll_od2 = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_HDMI_PLL_CNTL0,
		.shift = 18,
		.width = 2,
		.flags = CLK_DIVIDER_POWER_OF_TWO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmi_pll_od2",
		.ops = &clk_regmap_divider_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_hdmi_pll_od.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tl1_hdmi_pll = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_HDMI_PLL_CNTL0,
		.shift = 20,
		.width = 2,
		.flags = CLK_DIVIDER_POWER_OF_TWO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmi_pll",
		.ops = &clk_regmap_divider_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_hdmi_pll_od2.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

/* Video Clocks */
static struct clk_regmap tl1_vid_pll_div = {
	.data = &(struct meson_vid_pll_div_data){
		.val = {
			.reg_off = HHI_VID_PLL_CLK_DIV,
			.shift   = 0,
			.width   = 15,
		},
		.sel = {
			.reg_off = HHI_VID_PLL_CLK_DIV,
			.shift   = 16,
			.width   = 2,
		},
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vid_pll_div",
		.ops = &meson_vid_pll_div_ro_ops,
		.parent_hws = (const struct clk_hw *[]) { &tl1_hdmi_pll.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
	},
};

static const struct clk_hw *tl1_vid_pll_parent_hws[] = {
	&tl1_vid_pll_div.hw,
	&tl1_hdmi_pll.hw,
};

static struct clk_regmap tl1_vid_pll_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VID_PLL_CLK_DIV,
		.mask = 0x1,
		.shift = 18,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vid_pll_sel",
		.ops = &clk_regmap_mux_ops,
		/*
		 * bit 18 selects from 2 possible parents:
		 * vid_pll_div or hdmi_pll
		 */
		.parent_hws = tl1_vid_pll_parent_hws,
		.num_parents = ARRAY_SIZE(tl1_vid_pll_parent_hws),
		.flags = CLK_SET_RATE_NO_REPARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap tl1_vid_pll = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VID_PLL_CLK_DIV,
		.bit_idx = 19,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vid_pll",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_vid_pll_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

/* VPU Clock */

static const struct clk_hw *tl1_vpu_parent_hws[] = {
	&tl1_fclk_div3.hw,
	&tl1_fclk_div4.hw,
	&tl1_fclk_div5.hw,
	&tl1_fclk_div7.hw,
	&tl1_mpll1.hw,
	&tl1_vid_pll.hw,
	&tl1_hifi_pll.hw,
	&tl1_gp0_pll.hw,
};

static struct clk_regmap tl1_vpu_0_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VPU_CLK_CNTL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vpu_0_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = tl1_vpu_parent_hws,
		.num_parents = ARRAY_SIZE(tl1_vpu_parent_hws),
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tl1_vpu_0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VPU_CLK_CNTL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vpu_0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) { &tl1_vpu_0_sel.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tl1_vpu_0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VPU_CLK_CNTL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &tl1_vpu_0_div.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap tl1_vpu_1_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VPU_CLK_CNTL,
		.mask = 0x7,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vpu_1_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = tl1_vpu_parent_hws,
		.num_parents = ARRAY_SIZE(tl1_vpu_parent_hws),
		.flags = CLK_SET_RATE_NO_REPARENT,
	},
};

static struct clk_regmap tl1_vpu_1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VPU_CLK_CNTL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vpu_1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) { &tl1_vpu_1_sel.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tl1_vpu_1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VPU_CLK_CNTL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &tl1_vpu_1_div.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap tl1_vpu = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VPU_CLK_CNTL,
		.mask = 1,
		.shift = 31,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vpu",
		.ops = &clk_regmap_mux_ops,
		/*
		 * bit 31 selects from 2 possible parents:
		 * vpu_0 or vpu_1
		 */
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_vpu_0.hw,
			&tl1_vpu_1.hw,
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_NO_REPARENT,
	},
};

/* VDEC clocks */

static const struct clk_hw *tl1_vdec_parent_hws[] = {
	&tl1_fclk_div2p5.hw,
	&tl1_fclk_div3.hw,
	&tl1_fclk_div4.hw,
	&tl1_fclk_div5.hw,
	&tl1_fclk_div7.hw,
	&tl1_hifi_pll.hw,
	&tl1_gp0_pll.hw,
};

static struct clk_regmap tl1_vdec_1_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VDEC_CLK_CNTL,
		.mask = 0x7,
		.shift = 9,
		.flags = CLK_MUX_ROUND_CLOSEST,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vdec_1_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = tl1_vdec_parent_hws,
		.num_parents = ARRAY_SIZE(tl1_vdec_parent_hws),
	},
};

static struct clk_regmap tl1_vdec_1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VDEC_CLK_CNTL,
		.shift = 0,
		.width = 7,
		.flags = CLK_DIVIDER_ROUND_CLOSEST,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vdec_1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_vdec_1_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tl1_vdec_1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VDEC_CLK_CNTL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vdec_1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_vdec_1_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tl1_vdec_hevcf_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VDEC2_CLK_CNTL,
		.mask = 0x7,
		.shift = 9,
		.flags = CLK_MUX_ROUND_CLOSEST,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vdec_hevcf_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = tl1_vdec_parent_hws,
		.num_parents = ARRAY_SIZE(tl1_vdec_parent_hws),
	},
};

static struct clk_regmap tl1_vdec_hevcf_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VDEC2_CLK_CNTL,
		.shift = 0,
		.width = 7,
		.flags = CLK_DIVIDER_ROUND_CLOSEST,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vdec_hevcf_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_vdec_hevcf_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tl1_vdec_hevcf = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VDEC2_CLK_CNTL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vdec_hevcf",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_vdec_hevcf_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tl1_vdec_hevc_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VDEC2_CLK_CNTL,
		.mask = 0x7,
		.shift = 25,
		.flags = CLK_MUX_ROUND_CLOSEST,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vdec_hevc_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = tl1_vdec_parent_hws,
		.num_parents = ARRAY_SIZE(tl1_vdec_parent_hws),
	},
};

static struct clk_regmap tl1_vdec_hevc_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VDEC2_CLK_CNTL,
		.shift = 16,
		.width = 7,
		.flags = CLK_DIVIDER_ROUND_CLOSEST,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vdec_hevc_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_vdec_hevc_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tl1_vdec_hevc = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VDEC2_CLK_CNTL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vdec_hevc",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_vdec_hevc_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/* VAPB Clock */

static const struct clk_hw *tl1_vapb_parent_hws[] = {
	&tl1_fclk_div4.hw,
	&tl1_fclk_div3.hw,
	&tl1_fclk_div5.hw,
	&tl1_fclk_div7.hw,
	&tl1_mpll1.hw,
	&tl1_vid_pll.hw,
	&tl1_mpll2.hw,
	&tl1_fclk_div2p5.hw,
};

static struct clk_regmap tl1_vapb_0_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VAPBCLK_CNTL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vapb_0_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = tl1_vapb_parent_hws,
		.num_parents = ARRAY_SIZE(tl1_vapb_parent_hws),
		.flags = CLK_SET_RATE_NO_REPARENT,
	},
};

static struct clk_regmap tl1_vapb_0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VAPBCLK_CNTL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vapb_0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_vapb_0_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tl1_vapb_0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VAPBCLK_CNTL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vapb_0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_vapb_0_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap tl1_vapb_1_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VAPBCLK_CNTL,
		.mask = 0x3,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vapb_1_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = tl1_vapb_parent_hws,
		.num_parents = ARRAY_SIZE(tl1_vapb_parent_hws),
		.flags = CLK_SET_RATE_NO_REPARENT,
	},
};

static struct clk_regmap tl1_vapb_1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VAPBCLK_CNTL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vapb_1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_vapb_1_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tl1_vapb_1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VAPBCLK_CNTL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vapb_1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_vapb_1_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap tl1_vapb_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VAPBCLK_CNTL,
		.mask = 1,
		.shift = 31,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vapb_sel",
		.ops = &clk_regmap_mux_ops,
		/*
		 * bit 31 selects from 2 possible parents:
		 * vapb_0 or vapb_1
		 */
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_vapb_0.hw,
			&tl1_vapb_1.hw,
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_NO_REPARENT | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tl1_vapb = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VAPBCLK_CNTL,
		.bit_idx = 30,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vapb",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &tl1_vapb_sel.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static const struct clk_hw *tl1_vclk_parent_hws[] = {
	&tl1_vid_pll.hw,
	&tl1_gp0_pll.hw,
	&tl1_hifi_pll.hw,
	&tl1_mpll1.hw,
	&tl1_fclk_div3.hw,
	&tl1_fclk_div4.hw,
	&tl1_fclk_div5.hw,
	&tl1_fclk_div7.hw,
};

static struct clk_regmap tl1_vclk_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VID_CLK_CNTL,
		.mask = 0x7,
		.shift = 16,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vclk_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = tl1_vclk_parent_hws,
		.num_parents = ARRAY_SIZE(tl1_vclk_parent_hws),
		.flags = CLK_SET_RATE_NO_REPARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap tl1_vclk2_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VIID_CLK_CNTL,
		.mask = 0x7,
		.shift = 16,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vclk2_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = tl1_vclk_parent_hws,
		.num_parents = ARRAY_SIZE(tl1_vclk_parent_hws),
		.flags = CLK_SET_RATE_NO_REPARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap tl1_vclk_input = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VID_CLK_DIV,
		.bit_idx = 16,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk_input",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &tl1_vclk_sel.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap tl1_vclk2_input = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VIID_CLK_DIV,
		.bit_idx = 16,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk2_input",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &tl1_vclk2_sel.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap tl1_vclk_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VID_CLK_DIV,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vclk_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_vclk_input.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap tl1_vclk2_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VIID_CLK_DIV,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vclk2_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_vclk2_input.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap tl1_vclk = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VID_CLK_CNTL,
		.bit_idx = 19,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &tl1_vclk_div.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap tl1_vclk2 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VIID_CLK_CNTL,
		.bit_idx = 19,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk2",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &tl1_vclk2_div.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap tl1_vclk_div1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VID_CLK_CNTL,
		.bit_idx = 0,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk_div1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &tl1_vclk.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap tl1_vclk_div2_en = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VID_CLK_CNTL,
		.bit_idx = 1,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk_div2_en",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &tl1_vclk.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap tl1_vclk_div4_en = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VID_CLK_CNTL,
		.bit_idx = 2,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk_div4_en",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &tl1_vclk.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap tl1_vclk_div6_en = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VID_CLK_CNTL,
		.bit_idx = 3,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk_div6_en",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &tl1_vclk.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap tl1_vclk_div12_en = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VID_CLK_CNTL,
		.bit_idx = 4,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk_div12_en",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &tl1_vclk.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap tl1_vclk2_div1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VIID_CLK_CNTL,
		.bit_idx = 0,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk2_div1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &tl1_vclk2.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap tl1_vclk2_div2_en = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VIID_CLK_CNTL,
		.bit_idx = 1,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk2_div2_en",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &tl1_vclk2.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap tl1_vclk2_div4_en = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VIID_CLK_CNTL,
		.bit_idx = 2,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk2_div4_en",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &tl1_vclk2.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap tl1_vclk2_div6_en = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VIID_CLK_CNTL,
		.bit_idx = 3,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk2_div6_en",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &tl1_vclk2.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap tl1_vclk2_div12_en = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VIID_CLK_CNTL,
		.bit_idx = 4,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk2_div12_en",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &tl1_vclk2.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_fixed_factor tl1_vclk_div2 = {
	.mult = 1,
	.div = 2,
	.hw.init = &(struct clk_init_data){
		.name = "vclk_div2",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_vclk_div2_en.hw
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor tl1_vclk_div4 = {
	.mult = 1,
	.div = 4,
	.hw.init = &(struct clk_init_data){
		.name = "vclk_div4",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_vclk_div4_en.hw
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor tl1_vclk_div6 = {
	.mult = 1,
	.div = 6,
	.hw.init = &(struct clk_init_data){
		.name = "vclk_div6",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_vclk_div6_en.hw
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor tl1_vclk_div12 = {
	.mult = 1,
	.div = 12,
	.hw.init = &(struct clk_init_data){
		.name = "vclk_div12",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_vclk_div12_en.hw
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor tl1_vclk2_div2 = {
	.mult = 1,
	.div = 2,
	.hw.init = &(struct clk_init_data){
		.name = "vclk2_div2",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_vclk2_div2_en.hw
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor tl1_vclk2_div4 = {
	.mult = 1,
	.div = 4,
	.hw.init = &(struct clk_init_data){
		.name = "vclk2_div4",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_vclk2_div4_en.hw
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor tl1_vclk2_div6 = {
	.mult = 1,
	.div = 6,
	.hw.init = &(struct clk_init_data){
		.name = "vclk2_div6",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_vclk2_div6_en.hw
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor tl1_vclk2_div12 = {
	.mult = 1,
	.div = 12,
	.hw.init = &(struct clk_init_data){
		.name = "vclk2_div12",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_vclk2_div12_en.hw
		},
		.num_parents = 1,
	},
};

static u32 mux_table_cts_sel[] = { 0, 1, 2, 3, 4, 8, 9, 10, 11, 12 };
static const struct clk_hw *tl1_cts_parent_hws[] = {
	&tl1_vclk_div1.hw,
	&tl1_vclk_div2.hw,
	&tl1_vclk_div4.hw,
	&tl1_vclk_div6.hw,
	&tl1_vclk_div12.hw,
	&tl1_vclk2_div1.hw,
	&tl1_vclk2_div2.hw,
	&tl1_vclk2_div4.hw,
	&tl1_vclk2_div6.hw,
	&tl1_vclk2_div12.hw,
};

static struct clk_regmap tl1_cts_enci_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VID_CLK_DIV,
		.mask = 0xf,
		.shift = 28,
		.table = mux_table_cts_sel,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cts_enci_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = tl1_cts_parent_hws,
		.num_parents = ARRAY_SIZE(tl1_cts_parent_hws),
		.flags = CLK_SET_RATE_NO_REPARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap tl1_cts_encp_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VID_CLK_DIV,
		.mask = 0xf,
		.shift = 20,
		.table = mux_table_cts_sel,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cts_encp_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = tl1_cts_parent_hws,
		.num_parents = ARRAY_SIZE(tl1_cts_parent_hws),
		.flags = CLK_SET_RATE_NO_REPARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap tl1_cts_vdac_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VIID_CLK_DIV,
		.mask = 0xf,
		.shift = 28,
		.table = mux_table_cts_sel,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cts_vdac_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = tl1_cts_parent_hws,
		.num_parents = ARRAY_SIZE(tl1_cts_parent_hws),
		.flags = CLK_SET_RATE_NO_REPARENT | CLK_GET_RATE_NOCACHE,
	},
};

/* TOFIX: add support for cts_tcon */
static u32 mux_table_hdmi_tx_sel[] = { 0, 1, 2, 3, 4, 8, 9, 10, 11, 12 };
static const struct clk_hw *tl1_cts_hdmi_tx_parent_hws[] = {
	&tl1_vclk_div1.hw,
	&tl1_vclk_div2.hw,
	&tl1_vclk_div4.hw,
	&tl1_vclk_div6.hw,
	&tl1_vclk_div12.hw,
	&tl1_vclk2_div1.hw,
	&tl1_vclk2_div2.hw,
	&tl1_vclk2_div4.hw,
	&tl1_vclk2_div6.hw,
	&tl1_vclk2_div12.hw,
};

static struct clk_regmap tl1_hdmi_tx_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_HDMI_CLK_CNTL,
		.mask = 0xf,
		.shift = 16,
		.table = mux_table_hdmi_tx_sel,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmi_tx_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = tl1_cts_hdmi_tx_parent_hws,
		.num_parents = ARRAY_SIZE(tl1_cts_hdmi_tx_parent_hws),
		.flags = CLK_SET_RATE_NO_REPARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap tl1_cts_enci = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VID_CLK_CNTL2,
		.bit_idx = 0,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "cts_enci",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_cts_enci_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap tl1_cts_encp = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VID_CLK_CNTL2,
		.bit_idx = 2,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "cts_encp",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_cts_encp_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap tl1_cts_vdac = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VID_CLK_CNTL2,
		.bit_idx = 4,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "cts_vdac",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_cts_vdac_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap tl1_hdmi_tx = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VID_CLK_CNTL2,
		.bit_idx = 5,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmi_tx",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_hdmi_tx_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

/* HDMI Clocks */

static const struct clk_parent_data tl1_hdmi_parent_data[] = {
	{ .fw_name = "xtal", },
	{ .hw = &tl1_fclk_div4.hw },
	{ .hw = &tl1_fclk_div3.hw },
	{ .hw = &tl1_fclk_div5.hw },
};

static struct clk_regmap tl1_hdmi_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_HDMI_CLK_CNTL,
		.mask = 0x3,
		.shift = 9,
		.flags = CLK_MUX_ROUND_CLOSEST,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmi_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = tl1_hdmi_parent_data,
		.num_parents = ARRAY_SIZE(tl1_hdmi_parent_data),
		.flags = CLK_SET_RATE_NO_REPARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap tl1_hdmi_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_HDMI_CLK_CNTL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmi_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) { &tl1_hdmi_sel.hw },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap tl1_hdmi = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_HDMI_CLK_CNTL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmi",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &tl1_hdmi_div.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

/*
 * The MALI IP is clocked by two identical clocks (aali_0 and mali_1)
 * muxed by a glitch-free switch on Meson8b and Meson8m2 and later.
 *
 * CLK_SET_RATE_PARENT is added for mali_0_sel clock
 * 1.gp0 pll only support the 846M, avoid other rate 500/400M from it
 * 2.hifi pll is used for other module, skip it, avoid some rate from it
 */
static u32 mux_table_mali[] = { 0, 1, 3, 4, 5, 6, 7 };

static const struct clk_parent_data tl1_mali_0_1_parent_data[] = {
	{ .fw_name = "xtal", },
	{ .hw = &tl1_gp0_pll.hw },
	{ .hw = &tl1_fclk_div2p5.hw },
	{ .hw = &tl1_fclk_div3.hw },
	{ .hw = &tl1_fclk_div4.hw },
	{ .hw = &tl1_fclk_div5.hw },
	{ .hw = &tl1_fclk_div7.hw },
};

static struct clk_regmap tl1_mali_0_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_MALI_CLK_CNTL,
		.mask = 0x7,
		.shift = 9,
		.table = mux_table_mali,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mali_0_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = tl1_mali_0_1_parent_data,
		.num_parents = ARRAY_SIZE(tl1_mali_0_1_parent_data),
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tl1_mali_0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_MALI_CLK_CNTL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mali_0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_mali_0_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tl1_mali_0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_MALI_CLK_CNTL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mali_0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_mali_0_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_GATE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tl1_mali_1_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_MALI_CLK_CNTL,
		.mask = 0x7,
		.shift = 25,
		.table = mux_table_mali,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mali_1_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = tl1_mali_0_1_parent_data,
		.num_parents = ARRAY_SIZE(tl1_mali_0_1_parent_data),
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tl1_mali_1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_MALI_CLK_CNTL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mali_1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_mali_1_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tl1_mali_1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_MALI_CLK_CNTL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mali_1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_mali_1_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_GATE | CLK_SET_RATE_PARENT,
	},
};

static const struct clk_hw *tl1_mali_parent_hws[] = {
	&tl1_mali_0.hw,
	&tl1_mali_1.hw,
};

static struct clk_regmap tl1_mali = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_MALI_CLK_CNTL,
		.mask = 1,
		.shift = 31,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mali",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = tl1_mali_parent_hws,
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tl1_ts_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_TS_CLK_CNTL,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "ts_div",
		.ops = &clk_regmap_divider_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "xtal",
		},
		.num_parents = 1,
	},
};

static struct clk_regmap tl1_ts = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_TS_CLK_CNTL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "ts",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_ts_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/*
 * media clocks
 */

/* cts_vdec_clk */
static const struct clk_parent_data tl1_dec_parent_hws[] = {
	{ .hw = &tl1_fclk_div2p5.hw },
	{ .hw = &tl1_fclk_div3.hw },
	{ .hw = &tl1_fclk_div4.hw },
	{ .hw = &tl1_fclk_div5.hw },
	{ .hw = &tl1_fclk_div7.hw },
	{ .hw = &tl1_hifi_pll.hw },
	{ .hw = &tl1_gp0_pll.hw },
	{ .fw_name = "xtal", },
};

static struct clk_regmap tl1_vdec_p1_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VDEC3_CLK_CNTL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vdec_p1_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = tl1_dec_parent_hws,
		.num_parents = ARRAY_SIZE(tl1_dec_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap tl1_vdec_p1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VDEC3_CLK_CNTL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vdec_p1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_vdec_p1_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tl1_vdec_p1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VDEC3_CLK_CNTL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vdec_p1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_vdec_p1_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

/* cts_hcodec_clk */
static struct clk_regmap tl1_hcodec_p0_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VDEC_CLK_CNTL,
		.mask = 0x7,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hcodec_p0_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = tl1_dec_parent_hws,
		.num_parents = ARRAY_SIZE(tl1_dec_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap tl1_hcodec_p0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VDEC_CLK_CNTL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hcodec_p0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_hcodec_p0_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tl1_hcodec_p0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VDEC_CLK_CNTL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hcodec_p0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_hcodec_p0_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tl1_hcodec_p1_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VDEC3_CLK_CNTL,
		.mask = 0x7,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hcodec_p1_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = tl1_dec_parent_hws,
		.num_parents = ARRAY_SIZE(tl1_dec_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap tl1_hcodec_p1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VDEC3_CLK_CNTL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hcodec_p1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_hcodec_p1_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tl1_hcodec_p1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VDEC3_CLK_CNTL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hcodec_p1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_hcodec_p1_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data tl1_hcodec_mux_parent_hws[] = {
	{ .hw = &tl1_hcodec_p0.hw },
	{ .hw = &tl1_hcodec_p1.hw },
};

static struct clk_regmap tl1_hcodec_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VDEC3_CLK_CNTL,
		.mask = 0x1,
		.shift = 31,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hcodec_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = tl1_hcodec_mux_parent_hws,
		.num_parents = ARRAY_SIZE(tl1_hcodec_mux_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

/* cts_hevcb_clk */

static struct clk_regmap tl1_hevc_p1_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VDEC4_CLK_CNTL,
		.mask = 0x7,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hevc_p1_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = tl1_dec_parent_hws,
		.num_parents = ARRAY_SIZE(tl1_dec_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap tl1_hevc_p1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VDEC4_CLK_CNTL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hevc_p1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_hevc_p1_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tl1_hevc_p1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VDEC4_CLK_CNTL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hevc_p1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_hevc_p1_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

/* cts_hevcf_clk */

static struct clk_regmap tl1_hevcf_p1_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VDEC4_CLK_CNTL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hevcf_p1_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = tl1_dec_parent_hws,
		.num_parents = ARRAY_SIZE(tl1_dec_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap tl1_hevcf_p1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VDEC4_CLK_CNTL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hevcf_p1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_hevcf_p1_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tl1_hevcf_p1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VDEC4_CLK_CNTL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hevcf_p1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_hevcf_p1_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data vpu_parent_hws[] = {
	{ .hw = &tl1_fclk_div4.hw },
	{ .hw = &tl1_fclk_div3.hw },
	{ .hw = &tl1_fclk_div5.hw },
	{ .hw = &tl1_fclk_div7.hw },
};

static struct clk_regmap tl1_vpu_clkc_p0_mux  = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VPU_CLKC_CNTL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_clkc_p0_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = vpu_parent_hws,
		.num_parents = ARRAY_SIZE(vpu_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap tl1_vpu_clkc_p0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VPU_CLKC_CNTL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_clkc_p0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_vpu_clkc_p0_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tl1_vpu_clkc_p0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VPU_CLKC_CNTL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vpu_clkc_p0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_vpu_clkc_p0_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tl1_vpu_clkc_p1_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VPU_CLKC_CNTL,
		.mask = 0x7,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_clkc_p1_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = vpu_parent_hws,
		.num_parents = ARRAY_SIZE(vpu_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap tl1_vpu_clkc_p1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VPU_CLKC_CNTL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_clkc_p1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_vpu_clkc_p1_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tl1_vpu_clkc_p1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VPU_CLKC_CNTL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vpu_clkc_p1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_vpu_clkc_p1_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data tl1_vpu_mux_parent_hws[] = {
	{ .hw = &tl1_vpu_clkc_p0.hw },
	{ .hw = &tl1_vpu_clkc_p1.hw },
};

static struct clk_regmap tl1_vpu_clkc_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VPU_CLKC_CNTL,
		.mask = 0x1,
		.shift = 31,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_clkc_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = tl1_vpu_mux_parent_hws,
		.num_parents = ARRAY_SIZE(tl1_vpu_mux_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data tl1_meas_parent_hws[] = {
	{ .fw_name = "xtal", },
	{ .hw = &tl1_fclk_div4.hw },
	{ .hw = &tl1_fclk_div3.hw },
	{ .hw = &tl1_fclk_div5.hw },
	{ .hw = &tl1_vid_pll.hw },
	{ .hw = &tl1_gp0_pll.hw },
	{ .hw = &tl1_fclk_div2.hw },
	{ .hw = &tl1_fclk_div7.hw },
};

static struct clk_regmap tl1_dsi_meas_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VDIN_MEAS_CLK_CNTL,
		.mask = 0x7,
		.shift = 21,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "dsi_meas_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = tl1_meas_parent_hws,
		.num_parents = ARRAY_SIZE(tl1_meas_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap tl1_dsi_meas_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VDIN_MEAS_CLK_CNTL,
		.shift = 12,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "dsi_meas_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_dsi_meas_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tl1_dsi_meas = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VDIN_MEAS_CLK_CNTL,
		.bit_idx = 20,
	},
	.hw.init = &(struct clk_init_data){
		.name = "dsi_meas",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_dsi_meas_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data tl1_vdin_parent_hws[] = {
	{ .fw_name = "xtal", },
	{ .hw = &tl1_fclk_div4.hw },
	{ .hw = &tl1_fclk_div3.hw },
	{ .hw = &tl1_fclk_div5.hw },
	{ .hw = &tl1_vid_pll.hw }
};

static struct clk_regmap tl1_vdin_meas_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VDIN_MEAS_CLK_CNTL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vdin_meas_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = tl1_vdin_parent_hws,
		.num_parents = ARRAY_SIZE(tl1_vdin_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap tl1_vdin_meas_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VDIN_MEAS_CLK_CNTL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vdin_meas_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_vdin_meas_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tl1_vdin_meas = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VDIN_MEAS_CLK_CNTL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vdin_meas",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_vdin_meas_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data tl1_vdec_mux_parent_hws[] = {
	{ .hw = &tl1_vdec_1.hw },
	{ .hw = &tl1_vdec_p1.hw },
};

static struct clk_regmap tl1_vdec_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VDEC3_CLK_CNTL,
		.mask = 0x1,
		.shift = 15,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vdec_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = tl1_vdec_mux_parent_hws,
		.num_parents = ARRAY_SIZE(tl1_vdec_mux_parent_hws),
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data tl1_hevc_mux_parent_hws[] = {
	{ .hw = &tl1_vdec_hevc.hw },
	{ .hw = &tl1_hevc_p1.hw },
};

static struct clk_regmap tl1_hevc_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VDEC4_CLK_CNTL,
		.mask = 0x1,
		.shift = 31,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hevc_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = tl1_hevc_mux_parent_hws,
		.num_parents = ARRAY_SIZE(tl1_hevc_mux_parent_hws),
	},
};

static const struct clk_parent_data tl1_hevcf_mux_parent_hws[] = {
	{ .hw = &tl1_vdec_hevcf.hw },
	{ .hw = &tl1_hevcf_p1.hw },
};

static struct clk_regmap tl1_hevcf_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VDEC4_CLK_CNTL,
		.mask = 0x1,
		.shift = 15,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hevcf_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = tl1_hevcf_mux_parent_hws,
		.num_parents = ARRAY_SIZE(tl1_hevcf_mux_parent_hws),
	},
};

static const struct clk_parent_data vpu_clkb_tmp_parent_hws[] = {
	{ .hw = &tl1_vpu.hw },
	{ .hw = &tl1_fclk_div4.hw },
	{ .hw = &tl1_fclk_div5.hw },
	{ .hw = &tl1_fclk_div7.hw },
};

static struct clk_regmap tl1_vpu_clkb_tmp_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VPU_CLKB_CNTL,
		.mask = 0x3,
		.shift = 20,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_clkb_tmp_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = vpu_clkb_tmp_parent_hws,
		.num_parents = ARRAY_SIZE(vpu_clkb_tmp_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap tl1_vpu_clkb_tmp_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VPU_CLKB_CNTL,
		.shift = 16,
		.width = 4,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_clkb_tmp_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_vpu_clkb_tmp_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tl1_vpu_clkb_tmp = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VPU_CLKB_CNTL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vpu_clkb_tmp",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_vpu_clkb_tmp_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tl1_vpu_clkb_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VPU_CLKB_CNTL,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_clkb_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_vpu_clkb_tmp.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tl1_vpu_clkb = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VPU_CLKB_CNTL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vpu_clkb",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_vpu_clkb_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

/* cpu and dsu */
/* Datasheet names this field as "premux0" */
static struct clk_regmap tl1_cpu_clk_premux0 = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_SYS_CPU_CLK_CNTL0,
		.mask = 0x3,
		.shift = 0,
		.flags = CLK_MUX_ROUND_CLOSEST,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cpu_clk_dyn0_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = (const struct clk_parent_data []) {
			{ .fw_name = "xtal", },
			{ .hw = &tl1_fclk_div2.hw },
			{ .hw = &tl1_fclk_div3.hw },
		},
		.num_parents = 3,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct clk_hw *bt656_parent_hws[] = {
	&tl1_fclk_div2.hw,
	&tl1_fclk_div3.hw,
	&tl1_fclk_div3.hw,
	&tl1_fclk_div7.hw,
};

static struct clk_regmap tl1_bt656_sel  = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_BT656_CLK_CNTL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "bt656_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = bt656_parent_hws,
		.num_parents = ARRAY_SIZE(bt656_parent_hws),
	},
};

static struct clk_regmap tl1_bt656_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_BT656_CLK_CNTL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "bt656_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_bt656_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tl1_bt656 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_BT656_CLK_CNTL,
		.bit_idx = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "bt656",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_bt656_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data tl1_tcon_parent_data[] = {
	{ .fw_name = "xtal", },
	{ .hw = &tl1_fclk_div5.hw },
	{ .hw = &tl1_fclk_div4.hw },
	{ .hw = &tl1_fclk_div3.hw },
	{ .hw = &tl1_mpll2.hw },
	{ .hw = &tl1_mpll3.hw },
	{ .hw = &tl1_vid_pll.hw },
	{ .hw = &tl1_gp0_pll.hw },
};

static struct clk_regmap tl1_tcon_pll_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_TCON_CLK_CNTL,
		.mask = 0x7,
		.shift = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "tcon_pll_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = tl1_tcon_parent_data,
		.num_parents = ARRAY_SIZE(tl1_tcon_parent_data),
	},
};

static struct clk_regmap tl1_tcon_pll_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_TCON_CLK_CNTL,
		.shift = 0,
		.width = 6,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "tcon_pll_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_tcon_pll_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tl1_tcon_pll = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_TCON_CLK_CNTL,
		.bit_idx = 6,
	},
	.hw.init = &(struct clk_init_data){
		.name = "tcon_pll",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_tcon_pll_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/* Datasheet names this field as "premux1" */
static struct clk_regmap tl1_cpu_clk_premux1 = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_SYS_CPU_CLK_CNTL0,
		.mask = 0x3,
		.shift = 16,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cpu_clk_dyn1_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = (const struct clk_parent_data []) {
			{ .fw_name = "xtal", },
			{ .hw = &tl1_fclk_div2.hw },
			{ .hw = &tl1_fclk_div3.hw },
		},
		.num_parents = 3,
		/* This sub-tree is used a parking clock */
		.flags = CLK_SET_RATE_NO_REPARENT
	},
};

/* Datasheet names this field as "mux0_divn_tcnt" */
static struct clk_regmap tl1_cpu_clk_mux0_div = {
	.data = &(struct meson_clk_cpu_dyndiv_data){
		.div = {
			.reg_off = HHI_SYS_CPU_CLK_CNTL0,
			.shift = 4,
			.width = 6,
		},
		.dyn = {
			.reg_off = HHI_SYS_CPU_CLK_CNTL0,
			.shift = 26,
			.width = 1,
		},
	},
	.hw.init = &(struct clk_init_data){
		.name = "cpu_clk_dyn0_div",
		.ops = &meson_clk_cpu_dyndiv_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_cpu_clk_premux0.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/* Datasheet names this field as "postmux0" */
static struct clk_regmap tl1_cpu_clk_postmux0 = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_SYS_CPU_CLK_CNTL0,
		.mask = 0x1,
		.shift = 2,
		.flags = CLK_MUX_ROUND_CLOSEST,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cpu_clk_dyn0",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_cpu_clk_premux0.hw,
			&tl1_cpu_clk_mux0_div.hw,
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/* Datasheet names this field as "Mux1_divn_tcnt" */
static struct clk_regmap tl1_cpu_clk_mux1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_SYS_CPU_CLK_CNTL0,
		.shift = 20,
		.width = 6,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cpu_clk_dyn1_div",
		.ops = &clk_regmap_divider_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_cpu_clk_premux1.hw
		},
		.num_parents = 1,
	},
};

/* Datasheet names this field as "postmux1" */
static struct clk_regmap tl1_cpu_clk_postmux1 = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_SYS_CPU_CLK_CNTL0,
		.mask = 0x1,
		.shift = 18,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cpu_clk_dyn1",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_cpu_clk_premux1.hw,
			&tl1_cpu_clk_mux1_div.hw,
		},
		.num_parents = 2,
		/* This sub-tree is used a parking clock */
		.flags = CLK_SET_RATE_NO_REPARENT,
	},
};

/* Datasheet names this field as "Final_dyn_mux_sel" */
static struct clk_regmap tl1_cpu_clk_dyn = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_SYS_CPU_CLK_CNTL0,
		.mask = 0x1,
		.shift = 10,
		.flags = CLK_MUX_ROUND_CLOSEST,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cpu_clk_dyn",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_cpu_clk_postmux0.hw,
			&tl1_cpu_clk_postmux1.hw,
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/* Datasheet names this field as "Final_mux_sel" */
static struct clk_regmap tl1_cpu_clk = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_SYS_CPU_CLK_CNTL0,
		.mask = 0x1,
		.shift = 11,
		.flags = CLK_MUX_ROUND_CLOSEST,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cpu_clk",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_cpu_clk_dyn.hw,
			&tl1_sys_pll.hw,
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/* Datasheet names this field as "premux0" */
static struct clk_regmap tl1_dsu_clk_premux0 = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_SYS_CPU_CLK_CNTL5,
		.mask = 0x3,
		.shift = 0,
	},
	.hw.init = &(struct clk_init_data){
		.name = "dsu_clk_dyn0_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = (const struct clk_parent_data []) {
			{ .fw_name = "xtal", },
			{ .hw = &tl1_fclk_div2.hw },
			{ .hw = &tl1_fclk_div3.hw },
			{ .hw = &tl1_gp1_pll.hw },
		},
		.num_parents = 4,
		.flags = CLK_SET_RATE_PARENT | CLK_SET_RATE_NO_REPARENT,
	},
};

/* Datasheet names this field as "premux1" */
static struct clk_regmap tl1_dsu_clk_premux1 = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_SYS_CPU_CLK_CNTL5,
		.mask = 0x3,
		.shift = 16,
	},
	.hw.init = &(struct clk_init_data){
		.name = "dsu_clk_dyn1_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = (const struct clk_parent_data []) {
			{ .fw_name = "xtal", },
			{ .hw = &tl1_fclk_div2.hw },
			{ .hw = &tl1_fclk_div3.hw },
			{ .hw = &tl1_gp1_pll.hw },
		},
		.num_parents = 4,
		.flags = CLK_SET_RATE_PARENT | CLK_SET_RATE_NO_REPARENT,
	},
};

/* Datasheet names this field as "Mux0_divn_tcnt" */
static struct clk_regmap tl1_dsu_clk_mux0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_SYS_CPU_CLK_CNTL5,
		.shift = 4,
		.width = 6,
	},
	.hw.init = &(struct clk_init_data){
		.name = "dsu_clk_dyn0_div",
		.ops = &clk_regmap_divider_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_dsu_clk_premux0.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/* Datasheet names this field as "postmux0" */
static struct clk_regmap tl1_dsu_clk_postmux0 = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_SYS_CPU_CLK_CNTL5,
		.mask = 0x1,
		.shift = 2,
	},
	.hw.init = &(struct clk_init_data){
		.name = "dsu_clk_dyn0",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_dsu_clk_premux0.hw,
			&tl1_dsu_clk_mux0_div.hw,
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/* Datasheet names this field as "Mux1_divn_tcnt" */
static struct clk_regmap tl1_dsu_clk_mux1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_SYS_CPU_CLK_CNTL5,
		.shift = 20,
		.width = 6,
	},
	.hw.init = &(struct clk_init_data){
		.name = "dsu_clk_dyn1_div",
		.ops = &clk_regmap_divider_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_dsu_clk_premux1.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/* Datasheet names this field as "postmux1" */
static struct clk_regmap tl1_dsu_clk_postmux1 = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_SYS_CPU_CLK_CNTL5,
		.mask = 0x1,
		.shift = 18,
	},
	.hw.init = &(struct clk_init_data){
		.name = "dsu_clk_dyn1",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_dsu_clk_premux1.hw,
			&tl1_dsu_clk_mux1_div.hw,
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/* Datasheet names this field as "Final_dyn_mux_sel" */
static struct clk_regmap tl1_dsu_clk_dyn = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_SYS_CPU_CLK_CNTL5,
		.mask = 0x1,
		.shift = 10,
	},
	.hw.init = &(struct clk_init_data){
		.name = "dsu_clk_dyn",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_dsu_clk_postmux0.hw,
			&tl1_dsu_clk_postmux1.hw,
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT | CLK_SET_RATE_NO_REPARENT,
	},
};

/* Datasheet names this field as "Final_mux_sel" */
static struct clk_regmap tl1_dsu_final_clk = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_SYS_CPU_CLK_CNTL5,
		.mask = 0x1,
		.shift = 11,
	},
	.hw.init = &(struct clk_init_data){
		.name = "dsu_clk_final",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_dsu_clk_dyn.hw,
			&tl1_sys_pll.hw,
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT | CLK_SET_RATE_NO_REPARENT,
	},
};

static struct clk_regmap tl1_dsu_clk = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_SYS_CPU_CLK_CNTL6,
		.mask = 0x1,
		.shift = 27,
	},
	.hw.init = &(struct clk_init_data){
		.name = "dsu_clk",
		.ops = &clk_regmap_mux_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_cpu_clk.hw,
			&tl1_dsu_final_clk.hw,
		},
		.num_parents = 2,
	},
};

struct tl1_cpu_clk_postmux_nb_data {
	struct notifier_block nb;
	struct clk_hw *fclk_div2;
	struct clk_hw *cpu_clk_dyn;
	struct clk_hw *cpu_clk_postmux0;
	struct clk_hw *cpu_clk_postmux1;
	struct clk_hw *cpu_clk_premux1;
};

static int tl1_cpu_clk_postmux_notifier_cb(struct notifier_block *nb,
					   unsigned long event, void *data)
{
	struct tl1_cpu_clk_postmux_nb_data *nb_data =
		container_of(nb, struct tl1_cpu_clk_postmux_nb_data, nb);

	switch (event) {
	case PRE_RATE_CHANGE:
		/*
		 * This notifier means cpu_clk_postmux0 clock will be changed
		 * to feed cpu_clk, this is the current path :
		 * cpu_clk
		 *    \- cpu_clk_dyn
		 *          \- cpu_clk_postmux0
		 *                \- cpu_clk_muxX_div
		 *                      \- cpu_clk_premux0
		 *				\- fclk_div3 or fclk_div2
		 *		OR
		 *                \- cpu_clk_premux0
		 *			\- fclk_div3 or fclk_div2
		 */

		/* Setup cpu_clk_premux1 to fclk_div2 */
		clk_hw_set_parent(nb_data->cpu_clk_premux1,
				  nb_data->fclk_div2);

		/* Setup cpu_clk_postmux1 to bypass divider */
		clk_hw_set_parent(nb_data->cpu_clk_postmux1,
				  nb_data->cpu_clk_premux1);

		/* Switch to parking clk on cpu_clk_postmux1 */
		clk_hw_set_parent(nb_data->cpu_clk_dyn,
				  nb_data->cpu_clk_postmux1);

		/*
		 * Now, cpu_clk is 1G in the current path :
		 * cpu_clk
		 *    \- cpu_clk_dyn
		 *          \- cpu_clk_postmux1
		 *                \- cpu_clk_premux1
		 *                      \- fclk_div2
		 */
		return NOTIFY_OK;

	case POST_RATE_CHANGE:
		/*
		 * The cpu_clk_postmux0 has ben updated, now switch back
		 * cpu_clk_dyn to cpu_clk_postmux0 and take the changes
		 * in account.
		 */

		/* Configure cpu_clk_dyn back to cpu_clk_postmux0 */
		clk_hw_set_parent(nb_data->cpu_clk_dyn,
				  nb_data->cpu_clk_postmux0);

		/*
		 * new path :
		 * cpu_clk
		 *    \- cpu_clk_dyn
		 *          \- cpu_clk_postmux0
		 *                \- cpu_clk_muxX_div
		 *                      \- cpu_clk_premux0
		 *				\- fclk_div3 or fclk_div2
		 *		OR
		 *                \- cpu_clk_premux0
		 *			\- fclk_div3 or fclk_div2
		 */
		return NOTIFY_OK;

	default:
		return NOTIFY_DONE;
	}
}

static struct tl1_cpu_clk_postmux_nb_data tl1_cpu_clk_postmux0_nb_data = {
	.cpu_clk_dyn = &tl1_cpu_clk_dyn.hw,
	.cpu_clk_postmux0 = &tl1_cpu_clk_postmux0.hw,
	.cpu_clk_postmux1 = &tl1_cpu_clk_postmux1.hw,
	.cpu_clk_premux1 = &tl1_cpu_clk_premux1.hw,
	.nb.notifier_call = tl1_cpu_clk_postmux_notifier_cb,
};

struct tl1_sys_pll_nb_data {
	struct notifier_block nb;
	struct clk_hw *sys_pll;
	struct clk_hw *cpu_clk;
	struct clk_hw *cpu_clk_dyn;
};

static int tl1_sys_pll_notifier_cb(struct notifier_block *nb,
				   unsigned long event, void *data)
{
	struct tl1_sys_pll_nb_data *nb_data =
		container_of(nb, struct tl1_sys_pll_nb_data, nb);

	switch (event) {
	case PRE_RATE_CHANGE:
		/*
		 * This notifier means sys_pll clock will be changed
		 * to feed cpu_clk, this the current path :
		 * cpu_clk
		 *    \- sys_pll
		 *          \- sys_pll_dco
		 */

		/* Configure cpu_clk to use cpu_clk_dyn */
		clk_hw_set_parent(nb_data->cpu_clk,
				  nb_data->cpu_clk_dyn);

		/*
		 * Now, cpu_clk uses the dyn path
		 * cpu_clk
		 *    \- cpu_clk_dyn
		 *          \- cpu_clk_dynX
		 *                \- cpu_clk_dynX_sel
		 *		     \- cpu_clk_dynX_div
		 *                      \- xtal/fclk_div2/fclk_div3
		 *                   \- xtal/fclk_div2/fclk_div3
		 */
		return NOTIFY_OK;

	case POST_RATE_CHANGE:
		/*
		 * The sys_pll has ben updated, now switch back cpu_clk to
		 * sys_pll
		 */

		/* Configure cpu_clk to use sys_pll */
		clk_hw_set_parent(nb_data->cpu_clk,
				  nb_data->sys_pll);
		/* new path :
		 * cpu_clk
		 *    \- sys_pll
		 *          \- sys_pll_dco
		 */

		return NOTIFY_OK;

	default:
		return NOTIFY_DONE;
	}
}

static struct tl1_sys_pll_nb_data tl1_sys_pll_nb_data = {
	.sys_pll = &tl1_sys_pll.hw,
	.cpu_clk = &tl1_cpu_clk.hw,
	.cpu_clk_dyn = &tl1_cpu_clk_dyn.hw,
	.nb.notifier_call = tl1_sys_pll_notifier_cb,
};

/* tl1 dsu notify */
struct tl1_dsu_clk_postmux_nb_data {
	struct notifier_block nb;
	struct clk_hw *dsu_clk_dyn;
	struct clk_hw *dsu_clk_postmux0;
	struct clk_hw *dsu_clk_postmux1;
};

static int tl1_dsu_clk_postmux_notifier_cb(struct notifier_block *nb,
					   unsigned long event, void *data)
{
	struct tl1_dsu_clk_postmux_nb_data *nb_data =
		container_of(nb, struct tl1_dsu_clk_postmux_nb_data, nb);
	int ret = 0;

	switch (event) {
	case PRE_RATE_CHANGE:
		ret = clk_hw_set_parent(nb_data->dsu_clk_dyn,
					nb_data->dsu_clk_postmux1);
		if (ret)
			return notifier_from_errno(ret);

		return NOTIFY_OK;
	case POST_RATE_CHANGE:
		ret = clk_hw_set_parent(nb_data->dsu_clk_dyn,
					nb_data->dsu_clk_postmux0);
		if (ret)
			return notifier_from_errno(ret);

		return NOTIFY_OK;
	default:
		return NOTIFY_DONE;
	}
}

static struct tl1_dsu_clk_postmux_nb_data tl1_dsu_clk_postmux0_nb_data = {
	.dsu_clk_dyn = &tl1_dsu_clk_dyn.hw,
	.dsu_clk_postmux0 = &tl1_dsu_clk_postmux0.hw,
	.dsu_clk_postmux1 = &tl1_dsu_clk_postmux1.hw,
	.nb.notifier_call = tl1_dsu_clk_postmux_notifier_cb,
};

static struct clk_regmap tl1_cpu_clk_div16_en = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_SYS_CPU_CLK_CNTL1,
		.bit_idx = 1,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "cpu_clk_div16_en",
		.ops = &clk_regmap_gate_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_cpu_clk.hw
		},
		.num_parents = 1,
		/*
		 * This clock is used to debug the cpu_clk range
		 * Linux should not change it at runtime
		 */
	},
};

static struct clk_fixed_factor tl1_cpu_clk_div16 = {
	.mult = 1,
	.div = 16,
	.hw.init = &(struct clk_init_data){
		.name = "cpu_clk_div16",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_cpu_clk_div16_en.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap tl1_cpu_clk_apb_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_SYS_CPU_CLK_CNTL1,
		.shift = 3,
		.width = 3,
		.flags = CLK_DIVIDER_POWER_OF_TWO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cpu_clk_apb_div",
		.ops = &clk_regmap_divider_ro_ops,
		.parent_hws = (const struct clk_hw *[]) { &tl1_cpu_clk.hw },
		.num_parents = 1,
	},
};

static struct clk_regmap tl1_cpu_clk_apb = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_SYS_CPU_CLK_CNTL1,
		.bit_idx = 1,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "cpu_clk_apb",
		.ops = &clk_regmap_gate_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_cpu_clk_apb_div.hw
		},
		.num_parents = 1,
		/*
		 * This clock is set by the ROM monitor code,
		 * Linux should not change it at runtime
		 */
	},
};

static struct clk_regmap tl1_cpu_clk_atb_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_SYS_CPU_CLK_CNTL1,
		.shift = 6,
		.width = 3,
		.flags = CLK_DIVIDER_POWER_OF_TWO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cpu_clk_atb_div",
		.ops = &clk_regmap_divider_ro_ops,
		.parent_hws = (const struct clk_hw *[]) { &tl1_cpu_clk.hw },
		.num_parents = 1,
	},
};

static struct clk_regmap tl1_cpu_clk_atb = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_SYS_CPU_CLK_CNTL1,
		.bit_idx = 17,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "cpu_clk_atb",
		.ops = &clk_regmap_gate_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_cpu_clk_atb_div.hw
		},
		.num_parents = 1,
		/*
		 * This clock is set by the ROM monitor code,
		 * Linux should not change it at runtime
		 */
	},
};

static struct clk_regmap tl1_cpu_clk_axi_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_SYS_CPU_CLK_CNTL1,
		.shift = 9,
		.width = 3,
		.flags = CLK_DIVIDER_POWER_OF_TWO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cpu_clk_axi_div",
		.ops = &clk_regmap_divider_ro_ops,
		.parent_hws = (const struct clk_hw *[]) { &tl1_cpu_clk.hw },
		.num_parents = 1,
	},
};

static struct clk_regmap tl1_cpu_clk_axi = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_SYS_CPU_CLK_CNTL1,
		.bit_idx = 18,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "cpu_clk_axi",
		.ops = &clk_regmap_gate_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_cpu_clk_axi_div.hw
		},
		.num_parents = 1,
		/*
		 * This clock is set by the ROM monitor code,
		 * Linux should not change it at runtime
		 */
	},
};

static struct clk_regmap tl1_cpu_clk_trace_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_SYS_CPU_CLK_CNTL1,
		.shift = 20,
		.width = 3,
		.flags = CLK_DIVIDER_POWER_OF_TWO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cpu_clk_trace_div",
		.ops = &clk_regmap_divider_ro_ops,
		.parent_hws = (const struct clk_hw *[]) { &tl1_cpu_clk.hw },
		.num_parents = 1,
	},
};

static struct clk_regmap tl1_cpu_clk_trace = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_SYS_CPU_CLK_CNTL1,
		.bit_idx = 23,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "cpu_clk_trace",
		.ops = &clk_regmap_gate_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_cpu_clk_trace_div.hw
		},
		.num_parents = 1,
		/*
		 * This clock is set by the ROM monitor code,
		 * Linux should not change it at runtime
		 */
	},
};

static const struct clk_parent_data tl1_spicc_parent_hws[] = {
	{ .fw_name = "xtal", },
	{ .hw = &tl1_clk81.hw },
	{ .hw = &tl1_fclk_div4.hw },
	{ .hw = &tl1_fclk_div3.hw },
	{ .hw = &tl1_fclk_div2.hw },
	{ .hw = &tl1_fclk_div5.hw },
	{ .hw = &tl1_fclk_div7.hw },
	{ .hw = &tl1_gp0_pll.hw },
};

static struct clk_regmap tl1_spicc0_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_SPICC_CLK_CNTL,
		.mask = 0x7,
		.shift = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spicc0_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = tl1_spicc_parent_hws,
		.num_parents = ARRAY_SIZE(tl1_spicc_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap tl1_spicc0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_SPICC_CLK_CNTL,
		.shift = 0,
		.width = 6,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spicc0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_spicc0_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tl1_spicc0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_SPICC_CLK_CNTL,
		.bit_idx = 6,
	},
	.hw.init = &(struct clk_init_data){
		.name = "spicc0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_spicc0_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tl1_spicc1_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_SPICC_CLK_CNTL,
		.mask = 0x7,
		.shift = 23,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spicc1_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = tl1_spicc_parent_hws,
		.num_parents = ARRAY_SIZE(tl1_spicc_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap tl1_spicc1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_SPICC_CLK_CNTL,
		.shift = 16,
		.width = 6,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spicc1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_spicc1_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tl1_spicc1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_SPICC_CLK_CNTL,
		.bit_idx = 22,
	},
	.hw.init = &(struct clk_init_data){
		.name = "spicc1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_spicc1_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data tl1_vipnanoq_parent_hws[] = {
	{ .fw_name = "xtal", },
	{ .hw = &tl1_gp0_pll.hw },
	{ .hw = &tl1_hifi_pll.hw },
	{ .hw = &tl1_fclk_div2p5.hw },
	{ .hw = &tl1_fclk_div3.hw },
	{ .hw = &tl1_fclk_div4.hw },
	{ .hw = &tl1_fclk_div5.hw },
	{ .hw = &tl1_fclk_div7.hw },
};

static struct clk_regmap tl1_vipnanoq_core_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VIPNANOQ_CLK_CNTL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vipnanoq_core_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = tl1_vipnanoq_parent_hws,
		.num_parents = ARRAY_SIZE(tl1_vipnanoq_parent_hws),
	},
};

static struct clk_regmap tl1_vipnanoq_core_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VIPNANOQ_CLK_CNTL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vipnanoq_core_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_vipnanoq_core_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tl1_vipnanoq_core_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VIPNANOQ_CLK_CNTL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vipnanoq_core_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_vipnanoq_core_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tl1_vipnanoq_axi_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VIPNANOQ_CLK_CNTL,
		.mask = 0x7,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vipnanoq_axi_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = tl1_vipnanoq_parent_hws,
		.num_parents = ARRAY_SIZE(tl1_vipnanoq_parent_hws),
	},
};

static struct clk_regmap tl1_vipnanoq_axi_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VIPNANOQ_CLK_CNTL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vipnanoq_axi_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_vipnanoq_axi_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tl1_vipnanoq_axi_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VIPNANOQ_CLK_CNTL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vipnanoq_axi_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_vipnanoq_axi_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data tl1_hdmirx_parent_hws[] = {
	{ .fw_name = "xtal", },
	{ .hw = &tl1_fclk_div4.hw },
	{ .hw = &tl1_fclk_div3.hw },
	{ .hw = &tl1_fclk_div5.hw },
};

static struct clk_regmap tl1_hdmirx_cfg_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_HDMIRX_CLK_CNTL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmirx_cfg_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = tl1_hdmirx_parent_hws,
		.num_parents = ARRAY_SIZE(tl1_hdmirx_parent_hws),
	},
};

static struct clk_regmap tl1_hdmirx_cfg_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_HDMIRX_CLK_CNTL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmirx_cfg_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_hdmirx_cfg_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tl1_hdmirx_cfg_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_HDMIRX_CLK_CNTL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmirx_cfg_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_hdmirx_cfg_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tl1_hdmirx_modet_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_HDMIRX_CLK_CNTL,
		.mask = 0x3,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmirx_modet_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = tl1_hdmirx_parent_hws,
		.num_parents = ARRAY_SIZE(tl1_hdmirx_parent_hws),
	},
};

static struct clk_regmap tl1_hdmirx_modet_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_HDMIRX_CLK_CNTL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmirx_modet_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_hdmirx_modet_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tl1_hdmirx_modet_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_HDMIRX_CLK_CNTL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmirx_modet_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_hdmirx_modet_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data tl1_hdmirx_ref_parent_hws[] = {
	{ .hw = &tl1_fclk_div4.hw },
	{ .hw = &tl1_fclk_div3.hw },
	{ .hw = &tl1_fclk_div5.hw },
	{ .hw = &tl1_fclk_div7.hw },
};

static struct clk_regmap tl1_hdmirx_acr_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_HDMIRX_AUD_CLK_CNTL,
		.mask = 0x3,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmirx_acr_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = tl1_hdmirx_ref_parent_hws,
		.num_parents = ARRAY_SIZE(tl1_hdmirx_ref_parent_hws),
	},
};

static struct clk_regmap tl1_hdmirx_acr_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_HDMIRX_AUD_CLK_CNTL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmirx_acr_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_hdmirx_acr_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tl1_hdmirx_acr_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_HDMIRX_AUD_CLK_CNTL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmirx_acr_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_hdmirx_acr_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data tl1_hdmirx_meter_parent_hws[] = {
	{ .fw_name = "xtal", },
	{ .hw = &tl1_fclk_div4.hw },
	{ .hw = &tl1_fclk_div3.hw },
	{ .hw = &tl1_fclk_div5.hw },
};

static struct clk_regmap tl1_hdmirx_meter_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_HDMIRX_METER_CLK_CNTL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmirx_meter_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = tl1_hdmirx_meter_parent_hws,
		.num_parents = ARRAY_SIZE(tl1_hdmirx_meter_parent_hws),
	},
};

static struct clk_regmap tl1_hdmirx_meter_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_HDMIRX_METER_CLK_CNTL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmirx_meter_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_hdmirx_meter_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tl1_hdmirx_meter_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_HDMIRX_METER_CLK_CNTL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmirx_meter_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_hdmirx_meter_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tl1_hdmirx_axi_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_HDMIRX_AXI_CLK_CNTL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmirx_axi_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = tl1_hdmirx_meter_parent_hws,
		.num_parents = ARRAY_SIZE(tl1_hdmirx_meter_parent_hws),
	},
};

static struct clk_regmap tl1_hdmirx_axi_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_HDMIRX_AXI_CLK_CNTL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmirx_axi_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_hdmirx_axi_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tl1_hdmirx_axi_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_HDMIRX_AXI_CLK_CNTL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmirx_axi_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_hdmirx_axi_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tl1_hdmirx_skp_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_HDCP22_CLK_CNTL,
		.mask = 0x3,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmirx_skp_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = tl1_hdmirx_meter_parent_hws,
		.num_parents = ARRAY_SIZE(tl1_hdmirx_meter_parent_hws),
	},
};

static struct clk_regmap tl1_hdmirx_skp_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_HDMIRX_METER_CLK_CNTL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmirx_skp_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_hdmirx_skp_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tl1_hdmirx_skp_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_HDMIRX_METER_CLK_CNTL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmirx_skp_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_hdmirx_skp_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data tl1_hdmirx_esm_parent_hws[] = {
	{ .hw = &tl1_fclk_div7.hw },
	{ .hw = &tl1_fclk_div4.hw },
	{ .hw = &tl1_fclk_div3.hw },
	{ .hw = &tl1_fclk_div5.hw },
};

static struct clk_regmap tl1_hdmirx_esm_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_HDCP22_CLK_CNTL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmirx_esm_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = tl1_hdmirx_esm_parent_hws,
		.num_parents = ARRAY_SIZE(tl1_hdmirx_esm_parent_hws),
	},
};

static struct clk_regmap tl1_hdmirx_esm_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_HDCP22_CLK_CNTL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmirx_esm_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_hdmirx_esm_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tl1_hdmirx_esm_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_HDCP22_CLK_CNTL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmirx_esm_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_hdmirx_esm_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

#define MESON_GATE(_name, _reg, _bit) \
	MESON_PCLK(_name, _reg, _bit, &tl1_clk81.hw)

static MESON_GATE(tl1_clk81_ddr, HHI_GCLK_MPEG0,		0);
static MESON_GATE(tl1_clk81_dos, HHI_GCLK_MPEG0,		1);
static MESON_GATE(tl1_clk81_eth_phy, HHI_GCLK_MPEG0,		4);
static MESON_GATE(tl1_clk81_isa, HHI_GCLK_MPEG0,		5);
static MESON_GATE(tl1_clk81_pl310, HHI_GCLK_MPEG0,		6);
static MESON_GATE(tl1_clk81_periphs, HHI_GCLK_MPEG0,		7);
static MESON_GATE(tl1_clk81_spicc0, HHI_GCLK_MPEG0,		8);
static MESON_GATE(tl1_clk81_i2c, HHI_GCLK_MPEG0,		9);
static MESON_GATE(tl1_clk81_sana, HHI_GCLK_MPEG0,		10);
static MESON_GATE(tl1_clk81_smart_card, HHI_GCLK_MPEG0,		11);
static MESON_GATE(tl1_clk81_uart0, HHI_GCLK_MPEG0,		13);
static MESON_GATE(tl1_clk81_spicc1, HHI_GCLK_MPEG0,		14);
static MESON_GATE(tl1_clk81_stream, HHI_GCLK_MPEG0,		15);
static MESON_GATE(tl1_clk81_async_fifo, HHI_GCLK_MPEG0,		16);
static MESON_GATE(tl1_clk81_tvfe, HHI_GCLK_MPEG0,		18);
static MESON_GATE(tl1_clk81_hiu_reg, HHI_GCLK_MPEG0,		19);
static MESON_GATE(tl1_clk81_hdmirx_pclk, HHI_GCLK_MPEG0,	21);
static MESON_GATE(tl1_clk81_atv_demod, HHI_GCLK_MPEG0,		22);
static MESON_GATE(tl1_clk81_assist_misc, HHI_GCLK_MPEG0,	23);
static MESON_GATE(tl1_clk81_emmc_b, HHI_GCLK_MPEG0,		25);
static MESON_GATE(tl1_clk81_emmc_c, HHI_GCLK_MPEG0,		26);
static MESON_GATE(tl1_clk81_adec, HHI_GCLK_MPEG0,		27);
static MESON_GATE(tl1_clk81_acodec, HHI_GCLK_MPEG0,		28);
static MESON_GATE(tl1_clk81_tcon, HHI_GCLK_MPEG0,		29);
static MESON_GATE(tl1_clk81_spi, HHI_GCLK_MPEG0,		30);
static MESON_GATE(tl1_clk81_dsp, HHI_GCLK_MPEG0,		31);
static MESON_GATE(tl1_clk81_audio, HHI_GCLK_MPEG1,		0);
static MESON_GATE(tl1_clk81_eth_core, HHI_GCLK_MPEG1,		3);
static MESON_GATE(tl1_clk81_demux, HHI_GCLK_MPEG1,		4);
static MESON_GATE(tl1_clk81_aififo, HHI_GCLK_MPEG1,		11);
static MESON_GATE(tl1_clk81_adc, HHI_GCLK_MPEG1,		13);
static MESON_GATE(tl1_clk81_uart1, HHI_GCLK_MPEG1,		16);
static MESON_GATE(tl1_clk81_g2d, HHI_GCLK_MPEG1,		20);
static MESON_GATE(tl1_clk81_reset, HHI_GCLK_MPEG1,		23);
static MESON_GATE(tl1_clk81_u_parser, HHI_GCLK_MPEG1,		25);
static MESON_GATE(tl1_clk81_usb_general, HHI_GCLK_MPEG1,	26);
static MESON_GATE(tl1_clk81_ahb_arb0, HHI_GCLK_MPEG1,		29);
static MESON_GATE(tl1_clk81_ahb_data_bus, HHI_GCLK_MPEG2,	1);
static MESON_GATE(tl1_clk81_ahb_ctrl_bus, HHI_GCLK_MPEG2,	2);
static MESON_GATE(tl1_clk81_bt656, HHI_GCLK_MPEG2,		6);
static MESON_GATE(tl1_clk81_usb1_to_ddr, HHI_GCLK_MPEG2,	8);
static MESON_GATE(tl1_clk81_mmc_pclk, HHI_GCLK_MPEG2,		11);
static MESON_GATE(tl1_clk81_hdcp22_pclk, HHI_GCLK_MPEG2,	13);
static MESON_GATE(tl1_clk81_uart2, HHI_GCLK_MPEG2,		15);
static MESON_GATE(tl1_clk81_ts, HHI_GCLK_MPEG2,			22);
static MESON_GATE(tl1_clk81_demod_comb, HHI_GCLK_MPEG2,		25);
static MESON_GATE(tl1_clk81_vpu_intr, HHI_GCLK_MPEG2,		28);
static MESON_GATE(tl1_clk81_gic, HHI_GCLK_MPEG2,		30);
static MESON_GATE(tl1_clk81_vclk2_venci0, HHI_GCLK_OTHER,	1);
static MESON_GATE(tl1_clk81_vclk2_venci1, HHI_GCLK_OTHER,	2);
static MESON_GATE(tl1_clk81_vclk2_vencp0, HHI_GCLK_OTHER,	3);
static MESON_GATE(tl1_clk81_vclk2_vencp1, HHI_GCLK_OTHER,	4);
static MESON_GATE(tl1_clk81_vclk2_venct0, HHI_GCLK_OTHER,	5);
static MESON_GATE(tl1_clk81_vclk2_venct1, HHI_GCLK_OTHER,	6);
static MESON_GATE(tl1_clk81_vclk2_other, HHI_GCLK_OTHER,	7);
static MESON_GATE(tl1_clk81_vclk2_enci, HHI_GCLK_OTHER,		8);
static MESON_GATE(tl1_clk81_vclk2_encp, HHI_GCLK_OTHER,		9);
static MESON_GATE(tl1_clk81_dac_clk, HHI_GCLK_OTHER,		10);
static MESON_GATE(tl1_clk81_enc480p, HHI_GCLK_OTHER,		20);
static MESON_GATE(tl1_clk81_rng1, HHI_GCLK_OTHER,		21);
static MESON_GATE(tl1_clk81_vclk2_enct, HHI_GCLK_OTHER,		22);
static MESON_GATE(tl1_clk81_vclk2_encl, HHI_GCLK_OTHER,		23);
static MESON_GATE(tl1_clk81_vclk2_venclmmc, HHI_GCLK_OTHER,	24);
static MESON_GATE(tl1_clk81_vclk2_vencl, HHI_GCLK_OTHER,	25);
static MESON_GATE(tl1_clk81_vclk2_other1, HHI_GCLK_OTHER,	26);
static MESON_GATE(tl1_clk81_dma, HHI_GCLK_AO,			0);
static MESON_GATE(tl1_clk81_efuse, HHI_GCLK_AO,			1);
static MESON_GATE(tl1_clk81_rom_boot, HHI_GCLK_AO,		2);
static MESON_GATE(tl1_clk81_reset_sec, HHI_GCLK_AO,		3);
static MESON_GATE(tl1_clk81_sec_ahb_apb3, HHI_GCLK_AO,		4);

/* Array of all clocks provided by this provider */
static struct clk_hw_onecell_data tl1_hw_onecell_data = {
	.hws = {
		[CLKID_SYS_PLL_DCO]		= &tl1_sys_pll_dco.hw,
		[CLKID_SYS_PLL]			= &tl1_sys_pll.hw,
		[CLKID_FIXED_PLL_DCO]		= &tl1_fixed_pll_dco.hw,
		[CLKID_FIXED_PLL]		= &tl1_fixed_pll.hw,
		[CLKID_FCLK_DIV2_DIV]		= &tl1_fclk_div2_div.hw,
		[CLKID_FCLK_DIV3_DIV]		= &tl1_fclk_div3_div.hw,
		[CLKID_FCLK_DIV4_DIV]		= &tl1_fclk_div4_div.hw,
		[CLKID_FCLK_DIV5_DIV]		= &tl1_fclk_div5_div.hw,
		[CLKID_FCLK_DIV7_DIV]		= &tl1_fclk_div7_div.hw,
		[CLKID_FCLK_DIV2P5_DIV]		= &tl1_fclk_div2p5_div.hw,
		[CLKID_FCLK_DIV2]		= &tl1_fclk_div2.hw,
		[CLKID_FCLK_DIV3]		= &tl1_fclk_div3.hw,
		[CLKID_FCLK_DIV4]		= &tl1_fclk_div4.hw,
		[CLKID_FCLK_DIV5]		= &tl1_fclk_div5.hw,
		[CLKID_FCLK_DIV7]		= &tl1_fclk_div7.hw,
		[CLKID_FCLK_DIV2P5]		= &tl1_fclk_div2p5.hw,
		[CLKID_GP0_PLL_DCO]		= &tl1_gp0_pll_dco.hw,
		[CLKID_GP0_PLL]			= &tl1_gp0_pll.hw,
		[CLKID_GP1_PLL]			= &tl1_gp1_pll.hw,
		[CLKID_GP1_PLL_DCO]		= &tl1_gp1_pll_dco.hw,
		[CLKID_HIFI_PLL_DCO]		= &tl1_hifi_pll_dco.hw,
		[CLKID_HIFI_PLL]		= &tl1_hifi_pll.hw,
		[CLKID_MPEG_SEL]		= &tl1_mpeg_clk_sel.hw,
		[CLKID_MPEG_DIV]		= &tl1_mpeg_clk_div.hw,
		[CLKID_CLK81]			= &tl1_clk81.hw,
		[CLKID_MPLL_PREDIV]		= &tl1_mpll_prediv.hw,
		[CLKID_MPLL0_DIV]		= &tl1_mpll0_div.hw,
		[CLKID_MPLL1_DIV]		= &tl1_mpll1_div.hw,
		[CLKID_MPLL2_DIV]		= &tl1_mpll2_div.hw,
		[CLKID_MPLL3_DIV]		= &tl1_mpll3_div.hw,
		[CLKID_MPLL0]			= &tl1_mpll0.hw,
		[CLKID_MPLL1]			= &tl1_mpll1.hw,
		[CLKID_MPLL2]			= &tl1_mpll2.hw,
		[CLKID_MPLL3]			= &tl1_mpll3.hw,
		[CLKID_CLK81_DDR]		= &tl1_clk81_ddr.hw,
		[CLKID_CLK81_DOS]		= &tl1_clk81_dos.hw,
		[CLKID_CLK81_ETH_PHY]		= &tl1_clk81_eth_phy.hw,
		[CLKID_CLK81_ISA]		= &tl1_clk81_isa.hw,
		[CLKID_CLK81_PL310]		= &tl1_clk81_pl310.hw,
		[CLKID_CLK81_PERIPHS]		= &tl1_clk81_periphs.hw,
		[CLKID_CLK81_SPICC0]		= &tl1_clk81_spicc0.hw,
		[CLKID_CLK81_I2C]		= &tl1_clk81_i2c.hw,
		[CLKID_CLK81_SANA]		= &tl1_clk81_sana.hw,
		[CLKID_CLK81_SMART_CARD]	= &tl1_clk81_smart_card.hw,
		[CLKID_CLK81_UART0]		= &tl1_clk81_uart0.hw,
		[CLKID_CLK81_SPICC1]		= &tl1_clk81_spicc1.hw,
		[CLKID_CLK81_STREAM]		= &tl1_clk81_stream.hw,
		[CLKID_CLK81_ASYNC_FIFO]	= &tl1_clk81_async_fifo.hw,
		[CLKID_CLK81_TVFE]		= &tl1_clk81_tvfe.hw,
		[CLKID_CLK81_HIU_REG]		= &tl1_clk81_hiu_reg.hw,
		[CLKID_CLK81_HDMIRX_PCLK]	= &tl1_clk81_hdmirx_pclk.hw,
		[CLKID_CLK81_ATV_DEMOD]		= &tl1_clk81_atv_demod.hw,
		[CLKID_CLK81_ASSIST_MISC]	= &tl1_clk81_assist_misc.hw,
		[CLKID_CLK81_SD_EMMC_B]		= &tl1_clk81_emmc_b.hw,
		[CLKID_CLK81_SD_EMMC_C]		= &tl1_clk81_emmc_c.hw,
		[CLKID_CLK81_ADEC]		= &tl1_clk81_adec.hw,
		[CLKID_CLK81_ACODEC]		= &tl1_clk81_acodec.hw,
		[CLKID_CLK81_TCON]		= &tl1_clk81_tcon.hw,
		[CLKID_CLK81_SPI]		= &tl1_clk81_spi.hw,
		[CLKID_CLK81_DSP]		= &tl1_clk81_dsp.hw,
		[CLKID_CLK81_AUDIO]		= &tl1_clk81_audio.hw,
		[CLKID_CLK81_ETH_CORE]		= &tl1_clk81_eth_core.hw,
		[CLKID_CLK81_DEMUX]		= &tl1_clk81_demux.hw,
		[CLKID_CLK81_AIFIFO]		= &tl1_clk81_aififo.hw,
		[CLKID_CLK81_ADC]		= &tl1_clk81_adc.hw,
		[CLKID_CLK81_UART1]		= &tl1_clk81_uart1.hw,
		[CLKID_CLK81_G2D]		= &tl1_clk81_g2d.hw,
		[CLKID_CLK81_RESET]		= &tl1_clk81_reset.hw,
		[CLKID_CLK81_U_PARSER]		= &tl1_clk81_u_parser.hw,
		[CLKID_CLK81_USB_GENERAL]	= &tl1_clk81_usb_general.hw,
		[CLKID_CLK81_AHB_ARB0]		= &tl1_clk81_ahb_arb0.hw,
		[CLKID_CLK81_AHB_DATA_BUS]	= &tl1_clk81_ahb_data_bus.hw,
		[CLKID_CLK81_AHB_CTRL_BUS]	= &tl1_clk81_ahb_ctrl_bus.hw,
		[CLKID_CLK81_BT656]		= &tl1_clk81_bt656.hw,
		[CLKID_CLK81_USB1_TO_DDR]	= &tl1_clk81_usb1_to_ddr.hw,
		[CLKID_CLK81_MMC_PCLK]		= &tl1_clk81_mmc_pclk.hw,
		[CLKID_CLK81_HDCP22_PCLK]	= &tl1_clk81_hdcp22_pclk.hw,
		[CLKID_CLK81_UART2]		= &tl1_clk81_uart2.hw,
		[CLKID_CLK81_TS]		= &tl1_clk81_ts.hw,
		[CLKID_CLK81_DEMOD_COMB]	= &tl1_clk81_demod_comb.hw,
		[CLKID_CLK81_VPU_INTR]		= &tl1_clk81_vpu_intr.hw,
		[CLKID_CLK81_GIC]		= &tl1_clk81_gic.hw,
		[CLKID_CLK81_VCLK2_VENCI0]	= &tl1_clk81_vclk2_venci0.hw,
		[CLKID_CLK81_VCLK2_VENCI1]	= &tl1_clk81_vclk2_venci1.hw,
		[CLKID_CLK81_VCLK2_VENCP0]	= &tl1_clk81_vclk2_vencp0.hw,
		[CLKID_CLK81_VCLK2_VENCP1]	= &tl1_clk81_vclk2_vencp1.hw,
		[CLKID_CLK81_VCLK2_VENCT0]	= &tl1_clk81_vclk2_venct0.hw,
		[CLKID_CLK81_VCLK2_VENCT1]	= &tl1_clk81_vclk2_venct1.hw,
		[CLKID_CLK81_VCLK2_OTHER]	= &tl1_clk81_vclk2_other.hw,
		[CLKID_CLK81_VCLK2_ENCI]	= &tl1_clk81_vclk2_enci.hw,
		[CLKID_CLK81_VCLK2_ENCP]	= &tl1_clk81_vclk2_encp.hw,
		[CLKID_CLK81_DAC_CLK]		= &tl1_clk81_dac_clk.hw,
		[CLKID_CLK81_ENC480P]		= &tl1_clk81_enc480p.hw,
		[CLKID_CLK81_RNG1]		= &tl1_clk81_rng1.hw,
		[CLKID_CLK81_VCLK2_ENCT]	= &tl1_clk81_vclk2_enct.hw,
		[CLKID_CLK81_VCLK2_ENCL]	= &tl1_clk81_vclk2_encl.hw,
		[CLKID_CLK81_VCLK2_VENCLMMC]	= &tl1_clk81_vclk2_venclmmc.hw,
		[CLKID_CLK81_VCLK2_VENCL]	= &tl1_clk81_vclk2_vencl.hw,
		[CLKID_CLK81_VCLK2_OTHER1]	= &tl1_clk81_vclk2_other1.hw,
		[CLKID_CLK81_DMA]		= &tl1_clk81_dma.hw,
		[CLKID_CLK81_EFUSE]		= &tl1_clk81_efuse.hw,
		[CLKID_CLK81_ROM_BOOT]		= &tl1_clk81_rom_boot.hw,
		[CLKID_CLK81_RESET_SEC]		= &tl1_clk81_reset_sec.hw,
		[CLKID_CLK81_SEC_APH_APB3]	= &tl1_clk81_sec_ahb_apb3.hw,
		[CLKID_SD_EMMC_B_SEL]		= &tl1_sd_emmc_b_sel.hw,
		[CLKID_SD_EMMC_B_DIV]		= &tl1_sd_emmc_b_div.hw,
		[CLKID_SD_EMMC_B]		= &tl1_sd_emmc_b.hw,
		[CLKID_SD_EMMC_C_SEL]		= &tl1_sd_emmc_c_sel.hw,
		[CLKID_SD_EMMC_C_DIV]		= &tl1_sd_emmc_c_div.hw,
		[CLKID_SD_EMMC_C]		= &tl1_sd_emmc_c.hw,
		[CLKID_VPU_0_SEL]		= &tl1_vpu_0_sel.hw,
		[CLKID_VPU_0_DIV]		= &tl1_vpu_0_div.hw,
		[CLKID_VPU_0]			= &tl1_vpu_0.hw,
		[CLKID_VPU_1_SEL]		= &tl1_vpu_1_sel.hw,
		[CLKID_VPU_1_DIV]		= &tl1_vpu_1_div.hw,
		[CLKID_VPU_1]			= &tl1_vpu_1.hw,
		[CLKID_VPU]			= &tl1_vpu.hw,
		[CLKID_VAPB_0_SEL]		= &tl1_vapb_0_sel.hw,
		[CLKID_VAPB_0_DIV]		= &tl1_vapb_0_div.hw,
		[CLKID_VAPB_0]			= &tl1_vapb_0.hw,
		[CLKID_VAPB_1_SEL]		= &tl1_vapb_1_sel.hw,
		[CLKID_VAPB_1_DIV]		= &tl1_vapb_1_div.hw,
		[CLKID_VAPB_1]			= &tl1_vapb_1.hw,
		[CLKID_VAPB_SEL]		= &tl1_vapb_sel.hw,
		[CLKID_VAPB]			= &tl1_vapb.hw,
		[CLKID_HDMI_PLL_DCO]		= &tl1_hdmi_pll_dco.hw,
		[CLKID_HDMI_PLL_OD]		= &tl1_hdmi_pll_od.hw,
		[CLKID_HDMI_PLL_OD2]		= &tl1_hdmi_pll_od2.hw,
		[CLKID_HDMI_PLL]		= &tl1_hdmi_pll.hw,
		[CLKID_VID_PLL]			= &tl1_vid_pll_div.hw,
		[CLKID_VID_PLL_SEL]		= &tl1_vid_pll_sel.hw,
		[CLKID_VID_PLL_DIV]		= &tl1_vid_pll.hw,
		[CLKID_VCLK_SEL]		= &tl1_vclk_sel.hw,
		[CLKID_VCLK2_SEL]		= &tl1_vclk2_sel.hw,
		[CLKID_VCLK_INPUT]		= &tl1_vclk_input.hw,
		[CLKID_VCLK2_INPUT]		= &tl1_vclk2_input.hw,
		[CLKID_VCLK_DIV]		= &tl1_vclk_div.hw,
		[CLKID_VCLK2_DIV]		= &tl1_vclk2_div.hw,
		[CLKID_VCLK]			= &tl1_vclk.hw,
		[CLKID_VCLK2]			= &tl1_vclk2.hw,
		[CLKID_VCLK_DIV1]		= &tl1_vclk_div1.hw,
		[CLKID_VCLK_DIV2_EN]		= &tl1_vclk_div2_en.hw,
		[CLKID_VCLK_DIV4_EN]		= &tl1_vclk_div4_en.hw,
		[CLKID_VCLK_DIV6_EN]		= &tl1_vclk_div6_en.hw,
		[CLKID_VCLK_DIV12_EN]		= &tl1_vclk_div12_en.hw,
		[CLKID_VCLK2_DIV1]		= &tl1_vclk2_div1.hw,
		[CLKID_VCLK2_DIV2_EN]		= &tl1_vclk2_div2_en.hw,
		[CLKID_VCLK2_DIV4_EN]		= &tl1_vclk2_div4_en.hw,
		[CLKID_VCLK2_DIV6_EN]		= &tl1_vclk2_div6_en.hw,
		[CLKID_VCLK2_DIV12_EN]		= &tl1_vclk2_div12_en.hw,
		[CLKID_VCLK_DIV2]		= &tl1_vclk_div2.hw,
		[CLKID_VCLK_DIV4]		= &tl1_vclk_div4.hw,
		[CLKID_VCLK_DIV6]		= &tl1_vclk_div6.hw,
		[CLKID_VCLK_DIV12]		= &tl1_vclk_div12.hw,
		[CLKID_VCLK2_DIV2]		= &tl1_vclk2_div2.hw,
		[CLKID_VCLK2_DIV4]		= &tl1_vclk2_div4.hw,
		[CLKID_VCLK2_DIV6]		= &tl1_vclk2_div6.hw,
		[CLKID_VCLK2_DIV12]		= &tl1_vclk2_div12.hw,
		[CLKID_CTS_ENCI_SEL]		= &tl1_cts_enci_sel.hw,
		[CLKID_CTS_ENCP_SEL]		= &tl1_cts_encp_sel.hw,
		[CLKID_CTS_VDAC_SEL]		= &tl1_cts_vdac_sel.hw,
		[CLKID_HDMI_TX_SEL]		= &tl1_hdmi_tx_sel.hw,
		[CLKID_CTS_ENCI]		= &tl1_cts_enci.hw,
		[CLKID_CTS_ENCP]		= &tl1_cts_encp.hw,
		[CLKID_CTS_VDAC]		= &tl1_cts_vdac.hw,
		[CLKID_HDMI_TX]			= &tl1_hdmi_tx.hw,
		[CLKID_HDMI_SEL]		= &tl1_hdmi_sel.hw,
		[CLKID_HDMI_DIV]		= &tl1_hdmi_div.hw,
		[CLKID_HDMI]			= &tl1_hdmi.hw,
		[CLKID_MALI_0_SEL]		= &tl1_mali_0_sel.hw,
		[CLKID_MALI_0_DIV]		= &tl1_mali_0_div.hw,
		[CLKID_MALI_0]			= &tl1_mali_0.hw,
		[CLKID_MALI_1_SEL]		= &tl1_mali_1_sel.hw,
		[CLKID_MALI_1_DIV]		= &tl1_mali_1_div.hw,
		[CLKID_MALI_1]			= &tl1_mali_1.hw,
		[CLKID_MALI]			= &tl1_mali.hw,
		[CLKID_MPLL_50M_DIV]		= &tl1_mpll_50m_div.hw,
		[CLKID_MPLL_50M]		= &tl1_mpll_50m.hw,
		[CLKID_SYS_PLL_DIV16_EN]	= &tl1_sys_pll_div16_en.hw,
		[CLKID_SYS_PLL_DIV16]		= &tl1_sys_pll_div16.hw,
		[CLKID_CPU_CLK_DYN0_SEL]	= &tl1_cpu_clk_premux0.hw,
		[CLKID_CPU_CLK_DYN0_DIV]	= &tl1_cpu_clk_mux0_div.hw,
		[CLKID_CPU_CLK_DYN0]		= &tl1_cpu_clk_postmux0.hw,
		[CLKID_CPU_CLK_DYN1_SEL]	= &tl1_cpu_clk_premux1.hw,
		[CLKID_CPU_CLK_DYN1_DIV]	= &tl1_cpu_clk_mux1_div.hw,
		[CLKID_CPU_CLK_DYN1]		= &tl1_cpu_clk_postmux1.hw,
		[CLKID_CPU_CLK_DYN]		= &tl1_cpu_clk_dyn.hw,
		[CLKID_CPU_CLK]			= &tl1_cpu_clk.hw,
		[CLKID_CPU_CLK_DIV16_EN]	= &tl1_cpu_clk_div16_en.hw,
		[CLKID_CPU_CLK_DIV16]		= &tl1_cpu_clk_div16.hw,
		[CLKID_CPU_CLK_APB_DIV]		= &tl1_cpu_clk_apb_div.hw,
		[CLKID_CPU_CLK_APB]		= &tl1_cpu_clk_apb.hw,
		[CLKID_CPU_CLK_ATB_DIV]		= &tl1_cpu_clk_atb_div.hw,
		[CLKID_CPU_CLK_ATB]		= &tl1_cpu_clk_atb.hw,
		[CLKID_CPU_CLK_AXI_DIV]		= &tl1_cpu_clk_axi_div.hw,
		[CLKID_CPU_CLK_AXI]		= &tl1_cpu_clk_axi.hw,
		[CLKID_CPU_CLK_TRACE_DIV]	= &tl1_cpu_clk_trace_div.hw,
		[CLKID_CPU_CLK_TRACE]		= &tl1_cpu_clk_trace.hw,
		[CLKID_VDEC_1_SEL]		= &tl1_vdec_1_sel.hw,
		[CLKID_VDEC_1_DIV]		= &tl1_vdec_1_div.hw,
		[CLKID_VDEC_1]			= &tl1_vdec_1.hw,
		[CLKID_VDEC_HEVC_SEL]		= &tl1_vdec_hevc_sel.hw,
		[CLKID_VDEC_HEVC_DIV]		= &tl1_vdec_hevc_div.hw,
		[CLKID_VDEC_HEVC]		= &tl1_vdec_hevc.hw,
		[CLKID_VDEC_HEVCF_SEL]		= &tl1_vdec_hevcf_sel.hw,
		[CLKID_VDEC_HEVCF_DIV]		= &tl1_vdec_hevcf_div.hw,
		[CLKID_VDEC_HEVCF]		= &tl1_vdec_hevcf.hw,
		[CLKID_TS_DIV]			= &tl1_ts_div.hw,
		[CLKID_TS]			= &tl1_ts.hw,
		[CLKID_DSI_MEAS_MUX]		= &tl1_dsi_meas_mux.hw,
		[CLKID_DSI_MEAS_DIV]		= &tl1_dsi_meas_div.hw,
		[CLKID_DSI_MEAS]		= &tl1_dsi_meas.hw,
		[CLKID_VDEC_P1_MUX]		= &tl1_vdec_p1_mux.hw,
		[CLKID_VDEC_P1_DIV]		= &tl1_vdec_p1_div.hw,
		[CLKID_VDEC_P1]			= &tl1_vdec_p1.hw,
		[CLKID_VDEC_MUX]		= &tl1_vdec_mux.hw,
		[CLKID_HCODEC_P0_MUX]		= &tl1_hcodec_p0_mux.hw,
		[CLKID_HCODEC_P0_DIV]		= &tl1_hcodec_p0_div.hw,
		[CLKID_HCODEC_P0]		= &tl1_hcodec_p0.hw,
		[CLKID_HCODEC_P1_MUX]		= &tl1_hcodec_p1_mux.hw,
		[CLKID_HCODEC_P1_DIV]		= &tl1_hcodec_p1_div.hw,
		[CLKID_HCODEC_P1]		= &tl1_hcodec_p1.hw,
		[CLKID_HCODEC_MUX]		= &tl1_hcodec_mux.hw,
		[CLKID_HEVC_P1_MUX]		= &tl1_hevc_p1_mux.hw,
		[CLKID_HEVC_P1_DIV]		= &tl1_hevc_p1_div.hw,
		[CLKID_HEVC_P1]			= &tl1_hevc_p1.hw,
		[CLKID_HEVC_MUX]		= &tl1_hevc_mux.hw,
		[CLKID_HEVCF_P1_MUX]		= &tl1_hevcf_p1_mux.hw,
		[CLKID_HEVCF_P1_DIV]		= &tl1_hevcf_p1_div.hw,
		[CLKID_HEVCF_P1]		= &tl1_hevcf_p1.hw,
		[CLKID_HEVCF_MUX]		= &tl1_hevcf_mux.hw,
		[CLKID_VPU_CLKB_TMP_MUX]	= &tl1_vpu_clkb_tmp_mux.hw,
		[CLKID_VPU_CLKB_TMP_DIV]	= &tl1_vpu_clkb_tmp_div.hw,
		[CLKID_VPU_CLKB_TMP]		= &tl1_vpu_clkb_tmp.hw,
		[CLKID_VPU_CLKB_DIV]		= &tl1_vpu_clkb_div.hw,
		[CLKID_VPU_CLKB]		= &tl1_vpu_clkb.hw,
		[CLKID_VPU_CLKC_P0_MUX]		= &tl1_vpu_clkc_p0_mux.hw,
		[CLKID_VPU_CLKC_P0_DIV]		= &tl1_vpu_clkc_p0_div.hw,
		[CLKID_VPU_CLKC_P0]		= &tl1_vpu_clkc_p0.hw,
		[CLKID_VPU_CLKC_P1_MUX]		= &tl1_vpu_clkc_p1_mux.hw,
		[CLKID_VPU_CLKC_P1_DIV]		= &tl1_vpu_clkc_p1_div.hw,
		[CLKID_VPU_CLKC_P1]		= &tl1_vpu_clkc_p1.hw,
		[CLKID_VPU_CLKC_MUX]		= &tl1_vpu_clkc_mux.hw,
		[CLKID_SPICC0_MUX]		= &tl1_spicc0_mux.hw,
		[CLKID_SPICC0_DIV]		= &tl1_spicc0_div.hw,
		[CLKID_SPICC0]			= &tl1_spicc0.hw,
		[CLKID_SPICC1_MUX]		= &tl1_spicc1_mux.hw,
		[CLKID_SPICC1_DIV]		= &tl1_spicc1_div.hw,
		[CLKID_SPICC1]			= &tl1_spicc1.hw,
		[CLKID_VIPNANOQ_CORE_MUX]	= &tl1_vipnanoq_core_mux.hw,
		[CLKID_VIPNANOQ_CORE_DIV]	= &tl1_vipnanoq_core_div.hw,
		[CLKID_VIPNANOQ_CORE_GATE]	= &tl1_vipnanoq_core_gate.hw,
		[CLKID_VIPNANOQ_AXI_MUX]	= &tl1_vipnanoq_axi_mux.hw,
		[CLKID_VIPNANOQ_AXI_DIV]	= &tl1_vipnanoq_axi_div.hw,
		[CLKID_VIPNANOQ_AXI_GATE]	= &tl1_vipnanoq_axi_gate.hw,
		[CLKID_DSU_CLK_DYN0_SEL]	= &tl1_dsu_clk_premux0.hw,
		[CLKID_DSU_CLK_DYN0_DIV]	= &tl1_dsu_clk_premux1.hw,
		[CLKID_DSU_CLK_DYN0]		= &tl1_dsu_clk_mux0_div.hw,
		[CLKID_DSU_CLK_DYN1_SEL]	= &tl1_dsu_clk_postmux0.hw,
		[CLKID_DSU_CLK_DYN1_DIV]	= &tl1_dsu_clk_mux1_div.hw,
		[CLKID_DSU_CLK_DYN1]		= &tl1_dsu_clk_postmux1.hw,
		[CLKID_DSU_CLK_DYN]		= &tl1_dsu_clk_dyn.hw,
		[CLKID_DSU_CLK_FINAL]		= &tl1_dsu_final_clk.hw,
		[CLKID_DSU_CLK]			= &tl1_dsu_clk.hw,
		[CLKID_VDIN_MEAS_MUX]		= &tl1_vdin_meas_mux.hw,
		[CLKID_VDIN_MEAS_DIV]		= &tl1_vdin_meas_div.hw,
		[CLKID_VDIN_MEAS]		= &tl1_vdin_meas.hw,
		[CLKID_HDMIRX_CFG_MUX]		= &tl1_hdmirx_cfg_mux.hw,
		[CLKID_HDMIRX_CFG_DIV]		= &tl1_hdmirx_cfg_div.hw,
		[CLKID_HDMIRX_CFG_GATE]		= &tl1_hdmirx_cfg_gate.hw,
		[CLKID_HDMIRX_MODET_MUX]	= &tl1_hdmirx_modet_mux.hw,
		[CLKID_HDMIRX_MODET_DIV]	= &tl1_hdmirx_modet_div.hw,
		[CLKID_HDMIRX_MODET_GATE]	= &tl1_hdmirx_modet_gate.hw,
		[CLKID_HDMIRX_ACR_MUX]		= &tl1_hdmirx_acr_mux.hw,
		[CLKID_HDMIRX_ACR_DIV]		= &tl1_hdmirx_acr_div.hw,
		[CLKID_HDMIRX_ACR_GATE]		= &tl1_hdmirx_acr_gate.hw,
		[CLKID_HDMIRX_METER_MUX]	= &tl1_hdmirx_meter_mux.hw,
		[CLKID_HDMIRX_METER_DIV]	= &tl1_hdmirx_meter_div.hw,
		[CLKID_HDMIRX_METER_GATE]	= &tl1_hdmirx_meter_gate.hw,
		[CLKID_HDMIRX_AXI_MUX]		= &tl1_hdmirx_axi_mux.hw,
		[CLKID_HDMIRX_AXI_DIV]		= &tl1_hdmirx_axi_div.hw,
		[CLKID_HDMIRX_AXI_GATE]		= &tl1_hdmirx_axi_gate.hw,
		[CLKID_HDMIRX_SKP_MUX]		= &tl1_hdmirx_skp_mux.hw,
		[CLKID_HDMIRX_SKP_DIV]		= &tl1_hdmirx_skp_div.hw,
		[CLKID_HDMIRX_SKP_GATE]		= &tl1_hdmirx_skp_gate.hw,
		[CLKID_HDMIRX_ESM_MUX]		= &tl1_hdmirx_esm_mux.hw,
		[CLKID_HDMIRX_ESM_DIV]		= &tl1_hdmirx_esm_div.hw,
		[CLKID_HDMIRX_ESM_GATE]		= &tl1_hdmirx_esm_gate.hw,
		[CLKID_BT656_MUX]		= &tl1_bt656_sel.hw,
		[CLKID_BT656_DIV]		= &tl1_bt656_div.hw,
		[CLKID_BT656]			= &tl1_bt656.hw,
		[CLKID_TCON_PLL_MUX]		= &tl1_tcon_pll_sel.hw,
		[CLKID_TCON_PLL_DIV]		= &tl1_tcon_pll_div.hw,
		[CLKID_TCON_PLL]		= &tl1_tcon_pll.hw,
		[NR_CLKS]			= NULL,
	},
	.num = NR_CLKS,
};

/* Convenience table to populate regmap in .probe */
static struct clk_regmap *const tl1_clk_regmaps[] __initconst = {
	&tl1_fixed_pll_dco,
	&tl1_sys_pll_dco,
	&tl1_gp0_pll_dco,
	&tl1_gp1_pll_dco,
	&tl1_hifi_pll_dco,
	&tl1_fixed_pll,
	&tl1_sys_pll,
	&tl1_gp0_pll,
	&tl1_gp1_pll,
	&tl1_hifi_pll,
	&tl1_fclk_div2,
	&tl1_fclk_div3,
	&tl1_fclk_div4,
	&tl1_fclk_div5,
	&tl1_fclk_div7,
	&tl1_fclk_div2p5,
	&tl1_mpeg_clk_sel,
	&tl1_mpeg_clk_div,
	&tl1_clk81,
	&tl1_mpll0_div,
	&tl1_mpll1_div,
	&tl1_mpll2_div,
	&tl1_mpll3_div,
	&tl1_mpll0,
	&tl1_mpll1,
	&tl1_mpll2,
	&tl1_mpll3,
	&tl1_clk81_ddr,
	&tl1_clk81_dos,
	&tl1_clk81_eth_phy,
	&tl1_clk81_isa,
	&tl1_clk81_pl310,
	&tl1_clk81_periphs,
	&tl1_clk81_i2c,
	&tl1_clk81_spicc0,
	&tl1_clk81_spicc1,
	&tl1_clk81_sana,
	&tl1_clk81_smart_card,
	&tl1_clk81_uart0,
	&tl1_clk81_stream,
	&tl1_clk81_async_fifo,
	&tl1_clk81_tvfe,
	&tl1_clk81_hiu_reg,
	&tl1_clk81_hdmirx_pclk,
	&tl1_clk81_atv_demod,
	&tl1_clk81_assist_misc,
	&tl1_clk81_emmc_b,
	&tl1_clk81_emmc_c,
	&tl1_clk81_adec,
	&tl1_clk81_acodec,
	&tl1_clk81_tcon,
	&tl1_clk81_spi,
	&tl1_clk81_dsp,
	&tl1_clk81_audio,
	&tl1_clk81_eth_core,
	&tl1_clk81_demux,
	&tl1_clk81_aififo,
	&tl1_clk81_adc,
	&tl1_clk81_uart1,
	&tl1_clk81_g2d,
	&tl1_clk81_reset,
	&tl1_clk81_u_parser,
	&tl1_clk81_usb_general,
	&tl1_clk81_ahb_arb0,
	&tl1_clk81_ahb_data_bus,
	&tl1_clk81_ahb_ctrl_bus,
	&tl1_clk81_bt656,
	&tl1_clk81_usb1_to_ddr,
	&tl1_clk81_mmc_pclk,
	&tl1_clk81_hdcp22_pclk,
	&tl1_clk81_uart2,
	&tl1_clk81_ts,
	&tl1_clk81_demod_comb,
	&tl1_clk81_vpu_intr,
	&tl1_clk81_gic,
	&tl1_clk81_vclk2_venci0,
	&tl1_clk81_vclk2_venci1,
	&tl1_clk81_vclk2_vencp0,
	&tl1_clk81_vclk2_vencp1,
	&tl1_clk81_vclk2_venct0,
	&tl1_clk81_vclk2_venct1,
	&tl1_clk81_vclk2_other,
	&tl1_clk81_vclk2_enci,
	&tl1_clk81_vclk2_encp,
	&tl1_clk81_dac_clk,
	&tl1_clk81_enc480p,
	&tl1_clk81_rng1,
	&tl1_clk81_vclk2_enct,
	&tl1_clk81_vclk2_encl,
	&tl1_clk81_vclk2_venclmmc,
	&tl1_clk81_vclk2_vencl,
	&tl1_clk81_vclk2_other1,
	&tl1_clk81_dma,
	&tl1_clk81_efuse,
	&tl1_clk81_rom_boot,
	&tl1_clk81_reset_sec,
	&tl1_clk81_sec_ahb_apb3,
	&tl1_sd_emmc_b,
	&tl1_sd_emmc_c,
	&tl1_sd_emmc_b_div,
	&tl1_sd_emmc_c_div,
	&tl1_sd_emmc_b_sel,
	&tl1_sd_emmc_c_sel,
	&tl1_vpu_0_sel,
	&tl1_vpu_0_div,
	&tl1_vpu_0,
	&tl1_vpu_1_sel,
	&tl1_vpu_1_div,
	&tl1_vpu_1,
	&tl1_vpu,
	&tl1_vapb_0_sel,
	&tl1_vapb_0_div,
	&tl1_vapb_0,
	&tl1_vapb_1_sel,
	&tl1_vapb_1_div,
	&tl1_vapb_1,
	&tl1_vapb_sel,
	&tl1_vapb,
	&tl1_hdmi_pll_dco,
	&tl1_hdmi_pll_od,
	&tl1_hdmi_pll_od2,
	&tl1_hdmi_pll,
	&tl1_vid_pll_div,
	&tl1_vid_pll_sel,
	&tl1_vid_pll,
	&tl1_vclk_sel,
	&tl1_vclk2_sel,
	&tl1_vclk_input,
	&tl1_vclk2_input,
	&tl1_vclk_div,
	&tl1_vclk2_div,
	&tl1_vclk,
	&tl1_vclk2,
	&tl1_vclk_div1,
	&tl1_vclk_div2_en,
	&tl1_vclk_div4_en,
	&tl1_vclk_div6_en,
	&tl1_vclk_div12_en,
	&tl1_vclk2_div1,
	&tl1_vclk2_div2_en,
	&tl1_vclk2_div4_en,
	&tl1_vclk2_div6_en,
	&tl1_vclk2_div12_en,
	&tl1_cts_enci_sel,
	&tl1_cts_encp_sel,
	&tl1_cts_vdac_sel,
	&tl1_hdmi_tx_sel,
	&tl1_cts_enci,
	&tl1_cts_encp,
	&tl1_cts_vdac,
	&tl1_hdmi_tx,
	&tl1_hdmi_sel,
	&tl1_hdmi_div,
	&tl1_hdmi,
	&tl1_mali_0_sel,
	&tl1_mali_0_div,
	&tl1_mali_0,
	&tl1_mali_1_sel,
	&tl1_mali_1_div,
	&tl1_mali_1,
	&tl1_mali,
	&tl1_mpll_50m,
	&tl1_sys_pll_div16_en,
	&tl1_cpu_clk_premux0,
	&tl1_cpu_clk_mux0_div,
	&tl1_cpu_clk_postmux0,
	&tl1_cpu_clk_premux1,
	&tl1_cpu_clk_mux1_div,
	&tl1_cpu_clk_postmux1,
	&tl1_cpu_clk_dyn,
	&tl1_cpu_clk,
	&tl1_cpu_clk_div16_en,
	&tl1_cpu_clk_apb_div,
	&tl1_cpu_clk_apb,
	&tl1_cpu_clk_atb_div,
	&tl1_cpu_clk_atb,
	&tl1_cpu_clk_axi_div,
	&tl1_cpu_clk_axi,
	&tl1_cpu_clk_trace_div,
	&tl1_cpu_clk_trace,
	&tl1_vdec_1_sel,
	&tl1_vdec_1_div,
	&tl1_vdec_1,
	&tl1_vdec_hevc_sel,
	&tl1_vdec_hevc_div,
	&tl1_vdec_hevc,
	&tl1_vdec_hevcf_sel,
	&tl1_vdec_hevcf_div,
	&tl1_vdec_hevcf,
	&tl1_ts_div,
	&tl1_ts,
	&tl1_dsi_meas_mux,
	&tl1_dsi_meas_div,
	&tl1_dsi_meas,
	&tl1_vdin_meas_mux,
	&tl1_vdin_meas_div,
	&tl1_vdin_meas,
	&tl1_vdec_p1_mux,
	&tl1_vdec_p1_div,
	&tl1_vdec_p1,
	&tl1_vdec_mux,
	&tl1_hcodec_p0_mux,
	&tl1_hcodec_p0_div,
	&tl1_hcodec_p0,
	&tl1_hcodec_p1_mux,
	&tl1_hcodec_p1_div,
	&tl1_hcodec_p1,
	&tl1_hcodec_mux,
	&tl1_hevc_p1_mux,
	&tl1_hevc_p1_div,
	&tl1_hevc_p1,
	&tl1_hevc_mux,
	&tl1_hevcf_p1_mux,
	&tl1_hevcf_p1_div,
	&tl1_hevcf_p1,
	&tl1_hevcf_mux,
	&tl1_vpu_clkb_tmp_mux,
	&tl1_vpu_clkb_tmp_div,
	&tl1_vpu_clkb_tmp,
	&tl1_vpu_clkb_div,
	&tl1_vpu_clkb,
	&tl1_vpu_clkc_p0_mux,
	&tl1_vpu_clkc_p0_div,
	&tl1_vpu_clkc_p0,
	&tl1_vpu_clkc_p1_mux,
	&tl1_vpu_clkc_p1_div,
	&tl1_vpu_clkc_p1,
	&tl1_vpu_clkc_mux,
	&tl1_spicc0_mux,
	&tl1_spicc0_div,
	&tl1_spicc0,
	&tl1_spicc1_mux,
	&tl1_spicc1_div,
	&tl1_spicc1,
	&tl1_vipnanoq_core_mux,
	&tl1_vipnanoq_core_div,
	&tl1_vipnanoq_core_gate,
	&tl1_vipnanoq_axi_mux,
	&tl1_vipnanoq_axi_div,
	&tl1_vipnanoq_axi_gate,
	&tl1_dsu_clk_premux0,
	&tl1_dsu_clk_premux1,
	&tl1_dsu_clk_mux0_div,
	&tl1_dsu_clk_postmux0,
	&tl1_dsu_clk_mux1_div,
	&tl1_dsu_clk_postmux1,
	&tl1_dsu_clk_dyn,
	&tl1_dsu_final_clk,
	&tl1_dsu_clk,
	&tl1_hdmirx_cfg_mux,
	&tl1_hdmirx_cfg_div,
	&tl1_hdmirx_cfg_gate,
	&tl1_hdmirx_modet_mux,
	&tl1_hdmirx_modet_div,
	&tl1_hdmirx_modet_gate,
	&tl1_hdmirx_acr_mux,
	&tl1_hdmirx_acr_div,
	&tl1_hdmirx_acr_gate,
	&tl1_hdmirx_meter_mux,
	&tl1_hdmirx_meter_div,
	&tl1_hdmirx_meter_gate,
	&tl1_hdmirx_axi_mux,
	&tl1_hdmirx_axi_div,
	&tl1_hdmirx_axi_gate,
	&tl1_hdmirx_skp_mux,
	&tl1_hdmirx_skp_div,
	&tl1_hdmirx_skp_gate,
	&tl1_hdmirx_esm_mux,
	&tl1_hdmirx_esm_div,
	&tl1_hdmirx_esm_gate,
	&tl1_bt656_sel,
	&tl1_bt656_div,
	&tl1_bt656,
	&tl1_tcon_pll_sel,
	&tl1_tcon_pll_div,
	&tl1_tcon_pll
};

static const struct reg_sequence tl1_init_regs[] = {
	{ .reg = HHI_MPLL_CNTL0,	.def = 0x00000543 },
};

static int meson_tl1_dvfs_setup_common(struct platform_device *pdev,
					struct clk_hw **hws)
{
	struct clk *notifier_clk;
	int ret;

	/* Setup clock notifier for cpu_clk_postmux0 */
	tl1_cpu_clk_postmux0_nb_data.fclk_div2 = &tl1_fclk_div2.hw;
	notifier_clk = tl1_cpu_clk_postmux0.hw.clk;
	ret = clk_notifier_register(notifier_clk,
				    &tl1_cpu_clk_postmux0_nb_data.nb);
	if (ret) {
		dev_err(&pdev->dev, "failed to register the cpu_clk_postmux0 notifier\n");
		return ret;
	}

	/* Setup clock notifier for dsu */
	/* set tl1_dsu_clk_premux1 parent to fclk_div2 1G */
	ret = clk_set_parent(tl1_dsu_clk_premux1.hw.clk,
			     tl1_fclk_div2.hw.clk);
	if (ret < 0) {
		pr_err("%s: failed to set dsu parent\n", __func__);
		return ret;
	}

	ret = clk_notifier_register(tl1_dsu_clk_postmux0.hw.clk,
				    &tl1_dsu_clk_postmux0_nb_data.nb);
	if (ret) {
		dev_err(&pdev->dev, "failed to register dsu notifier\n");
		return ret;
	}

	return 0;
}

static int meson_tl1_dvfs_setup(struct platform_device *pdev)
{
	struct clk_hw **hws = tl1_hw_onecell_data.hws;
	struct clk *notifier_clk;
	int ret;

	ret = meson_tl1_dvfs_setup_common(pdev, hws);
	if (ret)
		return ret;

	/* Setup clock notifier for sys_pll */
	notifier_clk = tl1_sys_pll.hw.clk;
	ret = clk_notifier_register(notifier_clk, &tl1_sys_pll_nb_data.nb);
	if (ret) {
		dev_err(&pdev->dev, "failed to register the sys_pll notifier\n");
		return ret;
	}

	return 0;
}

struct meson_tl1_data {
	const struct meson_eeclkc_data eeclkc_data;
	int (*dvfs_setup)(struct platform_device *pdev);
};

static int meson_tl1_probe(struct platform_device *pdev)
{
	const struct meson_eeclkc_data *eeclkc_data;
	const struct meson_tl1_data *tl1_data;
	int ret;

	eeclkc_data = of_device_get_match_data(&pdev->dev);
	if (!eeclkc_data)
		return -EINVAL;

	ret = meson_eeclkc_probe(pdev);
	if (ret)
		return ret;

	tl1_data = container_of(eeclkc_data, struct meson_tl1_data,
				 eeclkc_data);

	if (tl1_data->dvfs_setup)
		return tl1_data->dvfs_setup(pdev);

	return 0;
}

static const struct meson_tl1_data tl1_clkc_data = {
	.eeclkc_data = {
		.regmap_clks = tl1_clk_regmaps,
		.regmap_clk_num = ARRAY_SIZE(tl1_clk_regmaps),
		.hw_onecell_data = &tl1_hw_onecell_data,
		.init_regs = tl1_init_regs,
		.init_count = ARRAY_SIZE(tl1_init_regs),
	},
	.dvfs_setup = meson_tl1_dvfs_setup,
};

static const struct of_device_id clkc_match_table[] = {
	{
		.compatible = "amlogic,tl1-clkc",
		.data = &tl1_clkc_data.eeclkc_data
	},
	{}
};

static struct platform_driver tl1_driver = {
	.probe		= meson_tl1_probe,
	.driver		= {
		.name	= "tl1-clkc",
		.of_match_table = clkc_match_table,
	},
};

#ifndef MODULE
static int tl1_clkc_init(void)
{
	return platform_driver_register(&tl1_driver);
}

arch_initcall_sync(tl1_clkc_init);
#else
int __init meson_tl1_clkc_init(void)
{
	return platform_driver_register(&tl1_driver);
}
#endif

MODULE_LICENSE("GPL v2");
