// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/clk-provider.h>
#include <linux/init.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/module.h>
#include <linux/of_address.h>

#include "clk-mpll.h"
#include "clk-pll.h"
#include "clk-regmap.h"
#include "clk-cpu-dyndiv.h"
#include "vid-pll-div.h"
#include "clk-dualdiv.h"
#include "a4.h"
#include "clkcs_init.h"
#include <dt-bindings/clock/amlogic,a4-clkc.h>
#include "clk-secure.h"

static const struct clk_ops meson_pll_clk_no_ops = {};

static struct clk_regmap a4_fixed_pll_dco = {
	.data = &(struct meson_clk_pll_data){
		.en = {
			.reg_off = ANACTRL_FIXPLL_CTRL0,
			.shift   = 28,
			.width   = 1,
		},
		.m = {
			.reg_off = ANACTRL_FIXPLL_CTRL0,
			.shift   = 0,
			.width   = 8,
		},
		.n = {
			.reg_off = ANACTRL_FIXPLL_CTRL0,
			.shift   = 16,
			.width   = 5,
		},
		.od = {
			.reg_off = ANACTRL_FIXPLL_CTRL0,
			.shift	 = 12,
			.width	 = 3,
		},
		.l = {
			.reg_off = ANACTRL_FIXPLL_CTRL0,
			.shift   = 31,
			.width   = 1,
		},
		.rst = {
			.reg_off = ANACTRL_FIXPLL_CTRL0,
			.shift   = 29,
			.width   = 1,
		},
	},
	.hw.init = &(struct clk_init_data){
		.name = "fixed_pll_dco",
		.ops = &meson_clk_pll_ro_ops,
		.parent_data = (const struct clk_parent_data []) {
			{ .fw_name = "xtal", }
		},
		.num_parents = 1,
		.flags = CLK_IS_CRITICAL | CLK_GET_RATE_NOCACHE,
	},
};

#ifdef CONFIG_ARM
static struct clk_regmap a4_fixed_pll = {
	.hw.init = &(struct clk_init_data){
		.name = "fixed_pll",
		.ops = &meson_pll_clk_no_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&a4_fixed_pll_dco.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};
#else
static struct clk_regmap a4_fixed_pll = {
	.data = &(struct clk_regmap_div_data){
		.offset = ANACTRL_FIXPLL_CTRL0,
		.shift = 12,
		.width = 3,
		.flags = CLK_DIVIDER_POWER_OF_TWO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "fixed_pll",
		.ops = &clk_regmap_divider_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&a4_fixed_pll_dco.hw
		},
		.num_parents = 1,
		/*
		 * This clock won't ever change at runtime so
		 * CLK_SET_RATE_PARENT is not required
		 */
		.flags = CLK_IS_CRITICAL | CLK_GET_RATE_NOCACHE,
	},
};
#endif

/*
 * the sys pll DCO value should be 3G~6G,
 * otherwise the sys pll can not lock.
 * od is for 32 bit.
 */

#ifdef CONFIG_ARM
static const struct pll_params_table a4_sys_pll_params_table[] = {
	PLL_PARAMS(100, 1, 1), /*DCO=2400M OD=DCO/2=1200M*/
	PLL_PARAMS(117, 1, 1), /*DCO=2808M OD=DCO/2=1404M*/
	PLL_PARAMS(125, 1, 1), /*DCO=3000M OD=DCO/2=1500M*/
	PLL_PARAMS(67, 1, 0),  /*DCO=1608M OD=DCO/1=1608M*/
	PLL_PARAMS(71, 1, 0),  /*DCO=1704M OD=DCO/1=1704M*/
	PLL_PARAMS(75, 1, 0),  /*DCO=1800M OD=DCO/1=1800M*/
	PLL_PARAMS(80, 1, 0),  /*DCO=1920M OD=DCO/1=1920M*/
	PLL_PARAMS(84, 1, 0),  /*DCO=2016M OD=DCO/1=2016M*/
	{ /* sentinel */ }
};
#else
static const struct pll_params_table a4_sys_pll_params_table[] = {
	/*
	 *  The DCO range of syspll on A5 is 1.6G-3.2G
	 *  OD=0 div=1  1.6G - 3.2G
	 *  OD=1 div=2  800M - 1.6G
	 *  OD=2 div=4  400M - 800M
	 *  OD=3 div=8  200M - 400M
	 *  OD=4 div=16 100M - 200M
	 */
	PLL_PARAMS(100, 1), /*DCO=2400M OD=DCO/2=1200M*/
	PLL_PARAMS(117, 1), /*DCO=2808M OD=DCO/2=1404M*/
	PLL_PARAMS(125, 1), /*DCO=3000M OD=DCO/2=1500M*/
	PLL_PARAMS(67, 1),  /*DCO=1608M OD=DCO/1=1608M*/
	PLL_PARAMS(71, 1),  /*DCO=1704M OD=DCO/1=1704M*/
	PLL_PARAMS(75, 1),  /*DCO=1800M OD=DCO/1=1800M*/
	PLL_PARAMS(80, 1),  /*DCO=1920M OD=DCO/1=1920M*/
	PLL_PARAMS(84, 1),  /*DCO=2016M OD=DCO/1=2016M*/
};
#endif

static struct clk_regmap a4_sys_pll_dco = {
	.data = &(struct meson_clk_pll_data){
		.en = {
			.reg_off = ANACTRL_SYSPLL_CTRL0,
			.shift   = 28,
			.width   = 1,
		},
		.m = {
			.reg_off = ANACTRL_SYSPLL_CTRL0,
			.shift   = 0,
			.width   = 8,
		},
		.n = {
			.reg_off = ANACTRL_SYSPLL_CTRL0,
			.shift   = 16,
			.width   = 5,
		},
		.od = {
			.reg_off = ANACTRL_SYSPLL_CTRL0,
			.shift	 = 12,
			.width	 = 3,
		},
		.l = {
			.reg_off = ANACTRL_SYSPLL_CTRL0,
			.shift   = 31,
			.width   = 1,
		},
		.rst = {
			.reg_off = ANACTRL_SYSPLL_CTRL0,
			.shift   = 29,
			.width   = 1,
		},
		.table = a4_sys_pll_params_table,
		.smc_id = SECURE_PLL_CLK,
		.secid_disable = SECID_SYS0_DCO_PLL_DIS,
		.secid = SECID_SYS0_DCO_PLL
	},
	.hw.init = &(struct clk_init_data){
		.name = "sys_pll_dco",
		.ops = &meson_secure_pll_v2_ops,
		.parent_data = (const struct clk_parent_data []) {
			{ .fw_name = "xtal", }
		},
		.num_parents = 1,
		/* This clock feeds the CPU, avoid disabling it */
		.flags = CLK_IS_CRITICAL | CLK_GET_RATE_NOCACHE,
	},
};

#ifdef CONFIG_ARM
/*
 * If DCO frequency is greater than 2.1G in 32bit,it will
 * overflow due to the callback .round_rate returns
 *  long (-2147483648 ~ +2147483647).
 * The OD output value is under 2G, For 32bit, the dco and
 * od should be described together to avoid overflow.
 * Beside, I have tried another methods but failed.
 * 1) change the freq unit to kHZ, it will crash (fixed xtal
 *   = 24000) and it will influences clock users.
 * 2) change the return value for .round_rate, a greater many
 *   code will be modified, related to whole CCF.
 * 3) dco pll using kHZ, other clock using HZ, when calculate pll
 *    it will be a lot of mass because of unit differences.
 *
 * Keep Consistent with 64bit, creat a Virtual clock for sys pll
 */
static struct clk_regmap a4_sys_pll = {
	.hw.init = &(struct clk_init_data){
		.name = "sys_pll",
		.ops = &meson_pll_clk_no_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&a4_sys_pll_dco.hw
		},
		.num_parents = 1,
		/*
		 * sys pll is used by cpu clock , it is initialized
		 * to 1200M in bl2, CLK_IGNORE_UNUSED is needed to
		 * prevent the system hang up which will be called
		 * by clk_disable_unused
		 */
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};
#else
static struct clk_regmap a4_sys_pll = {
	.data = &(struct clk_regmap_div_data){
		.offset = ANACTRL_SYSPLL_CTRL0,
		.shift = 12,
		.width = 3,
		.flags = CLK_DIVIDER_POWER_OF_TWO,
		.smc_id = SECURE_PLL_CLK,
		.secid = SECID_SYS0_PLL_OD
	},
	.hw.init = &(struct clk_init_data){
		.name = "sys_pll",
		.ops = &clk_regmap_secure_v2_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&a4_sys_pll_dco.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
	},
};
#endif

static struct clk_fixed_factor a4_fclk_div2_div = {
	.mult = 1,
	.div = 2,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div2_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) { &a4_fixed_pll.hw },
		.num_parents = 1,
	},
};

static struct clk_regmap a4_fclk_div2 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = ANACTRL_FIXPLL_CTRL1,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div2",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&a4_fclk_div2_div.hw
		},
		.num_parents = 1,
		.flags = CLK_IS_CRITICAL,
	},
};

static struct clk_fixed_factor a4_fclk_div3_div = {
	.mult = 1,
	.div = 3,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div3_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) { &a4_fixed_pll.hw },
		.num_parents = 1,
	},
};

static struct clk_regmap a4_fclk_div3 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = ANACTRL_FIXPLL_CTRL1,
		.bit_idx = 20,
	},
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div3",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&a4_fclk_div3_div.hw
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

static struct clk_fixed_factor a4_fclk_div4_div = {
	.mult = 1,
	.div = 4,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div4_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) { &a4_fixed_pll.hw },
		.num_parents = 1,
	},
};

static struct clk_regmap a4_fclk_div4 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = ANACTRL_FIXPLL_CTRL1,
		.bit_idx = 21,
	},
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div4",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&a4_fclk_div4_div.hw
		},
		.num_parents = 1,
		.flags = CLK_IS_CRITICAL,
	},
};

static struct clk_fixed_factor a4_fclk_div5_div = {
	.mult = 1,
	.div = 5,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div5_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) { &a4_fixed_pll.hw },
		.num_parents = 1,
	},
};

static struct clk_regmap a4_fclk_div5 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = ANACTRL_FIXPLL_CTRL1,
		.bit_idx = 22,
	},
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div5",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&a4_fclk_div5_div.hw
		},
		.num_parents = 1,
		.flags = CLK_IS_CRITICAL,
	},
};

static struct clk_fixed_factor a4_fclk_div7_div = {
	.mult = 1,
	.div = 7,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div7_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) { &a4_fixed_pll.hw },
		.num_parents = 1,
	},
};

static struct clk_regmap a4_fclk_div7 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = ANACTRL_FIXPLL_CTRL1,
		.bit_idx = 23,
	},
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div7",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&a4_fclk_div7_div.hw
		},
		.num_parents = 1,
		.flags = CLK_IS_CRITICAL,
	},
};

static struct clk_fixed_factor a4_fclk_div2p5_div = {
	.mult = 2,
	.div = 5,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div2p5_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&a4_fixed_pll.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap a4_fclk_div2p5 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = ANACTRL_FIXPLL_CTRL1,
		.bit_idx = 25,
	},
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div2p5",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&a4_fclk_div2p5_div.hw
		},
		.num_parents = 1,
		.flags = CLK_IS_CRITICAL,
	},
};

#ifdef CONFIG_ARM
static const struct pll_params_table a4_gp0_pll_table[] = {
	PLL_PARAMS(141, 1, 2), /* DCO = 3384M OD = 2 PLL = 846M */
	PLL_PARAMS(132, 1, 2), /* DCO = 3168M OD = 2 PLL = 792M */
	PLL_PARAMS(248, 1, 3), /* DCO = 5952M OD = 3 PLL = 744M */
	PLL_PARAMS(128, 1, 2), /* DCO = 3072M OD = 2 PLL = 768M */
	PLL_PARAMS(192, 1, 2), /* DCO = 4608M OD = 4 PLL = 1152M */
	{ /* sentinel */  }
};
#else
static const struct pll_params_table a4_gp0_pll_table[] = {
	PLL_PARAMS(141, 1), /* DCO = 3384M OD = 2 PLL = 846M */
	PLL_PARAMS(132, 1), /* DCO = 3168M OD = 2 PLL = 792M */
	PLL_PARAMS(248, 1), /* DCO = 5952M OD = 3 PLL = 744M */
	PLL_PARAMS(128, 1), /* DCO = 3072M OD = 2 PLL = 768M */
	PLL_PARAMS(192, 1), /* DCO = 4608M OD = 4 PLL = 1152M */
	{ /* sentinel */  }
};
#endif

/*
 * Internal gp0 pll emulation configuration parameters
 */
static const struct reg_sequence a4_gp0_init_regs[] = {
	{ .reg = ANACTRL_GP0PLL_CTRL1,	.def = 0x00000000 },
	{ .reg = ANACTRL_GP0PLL_CTRL2,	.def = 0x00000000 },
	{ .reg = ANACTRL_GP0PLL_CTRL3,	.def = 0x6a295c00 },
	{ .reg = ANACTRL_GP0PLL_CTRL4,	.def = 0x65771290 },
	{ .reg = ANACTRL_GP0PLL_CTRL5,	.def = 0x3927200a },
	{ .reg = ANACTRL_GP0PLL_CTRL6,	.def = 0x54540000 }
};

static struct clk_regmap a4_gp0_pll_dco = {
	.data = &(struct meson_clk_pll_data){
		.en = {
			.reg_off = ANACTRL_GP0PLL_CTRL0,
			.shift   = 28,
			.width   = 1,
		},
		.m = {
			.reg_off = ANACTRL_GP0PLL_CTRL0,
			.shift   = 0,
			.width   = 9,
		},
		.n = {
			.reg_off = ANACTRL_GP0PLL_CTRL0,
			.shift   = 16,
			.width   = 5,
		},
		.od = {
			.reg_off = ANACTRL_GP0PLL_CTRL0,
			.shift	 = 10,
			.width	 = 3,
		},
		.frac = {
			.reg_off = ANACTRL_GP0PLL_CTRL1,
			.shift   = 0,
			.width   = 19,
		},
		.l = {
			.reg_off = ANACTRL_GP0PLL_CTRL0,
			.shift   = 31,
			.width   = 1,
		},
		.rst = {
			.reg_off = ANACTRL_GP0PLL_CTRL0,
			.shift   = 29,
			.width   = 1,
		},
		.table = a4_gp0_pll_table,
		.init_regs = a4_gp0_init_regs,
		.init_count = ARRAY_SIZE(a4_gp0_init_regs),
	},
	.hw.init = &(struct clk_init_data){
		.name = "gp0_pll_dco",
		.ops = &meson_clk_pll_ops,
		.parent_data = (const struct clk_parent_data []) {
			{ .fw_name = "xtal", }
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED,
	},
};

#ifdef CONFIG_ARM
static struct clk_regmap a4_gp0_pll = {
	.hw.init = &(struct clk_init_data){
		.name = "gp0_pll",
		.ops = &meson_pll_clk_no_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&a4_gp0_pll_dco.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};
#else
static struct clk_regmap a4_gp0_pll = {
	.data = &(struct clk_regmap_div_data){
		.offset = ANACTRL_GP0PLL_CTRL0,
		.shift = 10,
		.width = 3,
		.flags = (CLK_DIVIDER_POWER_OF_TWO |
			  CLK_DIVIDER_ROUND_CLOSEST),
	},
	.hw.init = &(struct clk_init_data){
		.name = "gp0_pll",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&a4_gp0_pll_dco.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE
				| CLK_IGNORE_UNUSED,
	},
};
#endif

#ifdef CONFIG_ARM
static const struct pll_params_table a4_gp1_pll_table[] = {
	PLL_PARAMS(100, 1, 1), /*DCO=2400M OD=1200M*/
	PLL_PARAMS(125, 1, 1), /*DCO=3000M OD=1500M*/
	{ /* sentinel */  }
};
#else
static const struct pll_params_table a4_gp1_pll_table[] = {
	/*
	 *  The DCO range of syspll sys1pll gp1pll on T3 is 1.6G-3.2G
	 *  OD=0 div=1  1.6G - 3.2G
	 *  OD=1 div=2  800M - 1.6G
	 *  OD=2 div=4  400M - 800M
	 *  OD=3 div=8  200M - 400M
	 *  OD=4 div=16 100M - 200M
	 */
	PLL_PARAMS(100, 1), /*DCO=2400M OD=DCO/2=1200M*/
	PLL_PARAMS(125, 1), /*DCO=3000M OD=DCO/2=1500M*/
	{ /* sentinel */  }
};
#endif

static struct clk_regmap a4_gp1_pll_dco = {
	.data = &(struct meson_clk_pll_data){
		.en = {
			.reg_off = ANACTRL_GP1PLL_CTRL0,
			.shift   = 28,
			.width   = 1,
		},
		.m = {
			.reg_off = ANACTRL_GP1PLL_CTRL0,
			.shift   = 0,
			.width   = 8,
		},
		.n = {
			.reg_off = ANACTRL_GP1PLL_CTRL0,
			.shift   = 16,
			.width   = 5,
		},
		.od = {
			.reg_off = ANACTRL_GP1PLL_CTRL0,
			.shift	 = 12,
			.width	 = 3,
		},
		.l = {
			.reg_off = ANACTRL_GP1PLL_CTRL0,
			.shift   = 31,
			.width   = 1,
		},
		.rst = {
			.reg_off = ANACTRL_GP1PLL_CTRL0,
			.shift   = 29,
			.width   = 1,
		},
		.table = a4_gp1_pll_table,
		.smc_id = SECURE_PLL_CLK,
		.secid_disable = SECID_GP1_DCO_PLL_DIS,
		.secid = SECID_GP1_DCO_PLL
	},
	.hw.init = &(struct clk_init_data){
		.name = "gp1_pll_dco",
		.ops = &meson_secure_pll_v2_ops,
		.parent_data = (const struct clk_parent_data []) {
			{ .fw_name = "xtal", }
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED,
	},
};

#ifdef CONFIG_ARM
static struct clk_regmap a4_gp1_pll = {
	.hw.init = &(struct clk_init_data){
		.name = "gp1_pll",
		.ops = &meson_pll_clk_no_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&a4_gp1_pll_dco.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};
#else
static struct clk_regmap a4_gp1_pll = {
	.data = &(struct clk_regmap_div_data){
		.offset = ANACTRL_GP1PLL_CTRL0,
		.shift = 12,
		.width = 3,
		.flags = (CLK_DIVIDER_POWER_OF_TWO |
			  CLK_DIVIDER_ROUND_CLOSEST),
		.smc_id = SECURE_PLL_CLK,
		.secid = SECID_GP1_PLL_OD
	},
	.hw.init = &(struct clk_init_data){
		.name = "gp1_pll",
		.ops = &clk_regmap_secure_v2_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&a4_gp1_pll_dco.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
	},
};
#endif

/*cpu_clk*/
static const struct cpu_dyn_table a4_cpu_dyn_table[] = {
	CPU_LOW_PARAMS(24000000, 0, 0, 0),
	CPU_LOW_PARAMS(100000000, 1, 1, 9),
	CPU_LOW_PARAMS(250000000, 1, 1, 3),
	CPU_LOW_PARAMS(333333333, 2, 1, 1),
	CPU_LOW_PARAMS(500000000, 1, 1, 1),
	CPU_LOW_PARAMS(667000000, 2, 0, 0),
	CPU_LOW_PARAMS(1000000000, 1, 0, 0),
};

static const struct clk_parent_data a4_cpu_dyn_clk_sel[] = {
	{ .fw_name = "xtal", },
	{ .hw = &a4_fclk_div2.hw },
	{ .hw = &a4_fclk_div3.hw },
	{ .hw = &a4_fclk_div2p5.hw },
};

static struct clk_regmap a4_cpu_dyn_clk = {
	.data = &(struct meson_sec_cpu_dyn_data){
		.table = a4_cpu_dyn_table,
		.table_cnt = ARRAY_SIZE(a4_cpu_dyn_table),
		.secid_dyn_rd = SECID_CPU_CLK_RD,
		.secid_dyn = SECID_CPU_CLK_DYN,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cpu_dyn_clk",
		.ops = &meson_sec_cpu_dyn_ops,
		.parent_data = a4_cpu_dyn_clk_sel,
		.num_parents = ARRAY_SIZE(a4_cpu_dyn_clk_sel),
	},
};

static struct clk_regmap a4_cpu_clk = {
	.data = &(struct clk_regmap_mux_data){
		.mask = 0x1,
		.shift = 11,
		.flags = CLK_MUX_ROUND_CLOSEST,
		.smc_id = SECURE_CPU_CLK,
		.secid = SECID_CPU_CLK_SEL,
		.secid_rd = SECID_CPU_CLK_RD
	},
	.hw.init = &(struct clk_init_data){
		.name = "cpu_clk",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&a4_cpu_dyn_clk.hw,
			&a4_sys_pll.hw,
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT | CLK_IS_CRITICAL,
	},
};

struct a4_sys_pll_nb_data {
	struct notifier_block nb;
	struct clk_hw *sys_pll;
	struct clk_hw *cpu_clk;
	struct clk_hw *cpu_clk_dyn;
};

static int a4_sys_pll_notifier_cb(struct notifier_block *nb,
				   unsigned long event, void *data)
{
	struct a4_sys_pll_nb_data *nb_data =
		container_of(nb, struct a4_sys_pll_nb_data, nb);

	switch (event) {
	case PRE_RATE_CHANGE:
		/*
		 * This notifier means sys_pll clock will be changed
		 * to feed cpu_clk, this the current path :
		 * cpu_clk
		 *    \- sys_pll
		 *          \- sys_pll_dco
		 */

		/*
		 * Configure cpu_clk to use cpu_clk_dyn
		 * Make sure cpu clk is 1G, cpu_clk_dyn may equal 24M
		 */
		if (clk_set_rate(nb_data->cpu_clk_dyn->clk, 1000000000))
			pr_err("%s: set CPU dyn clock to 1G failed\n", __func__);

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
		udelay(5);

		return NOTIFY_OK;

	case POST_RATE_CHANGE:
		/*
		 * The sys_pll has ben updated, now switch back cpu_clk to
		 * sys_pll
		 */

		/* Configure cpu_clk to use sys_pll */
		clk_hw_set_parent(nb_data->cpu_clk,
				  nb_data->sys_pll);

		udelay(5);
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

static struct a4_sys_pll_nb_data a4_sys_pll_nb_data = {
	.sys_pll = &a4_sys_pll.hw,
	.cpu_clk = &a4_cpu_clk.hw,
	.cpu_clk_dyn = &a4_cpu_dyn_clk.hw,
	.nb.notifier_call = a4_sys_pll_notifier_cb,
};

#ifdef CONFIG_ARM
static const struct pll_params_table a4_hifi_pll_table[] = {
	PLL_PARAMS(192, 1, 2), /* DCO = 4608M OD = 2 PLL = 1152M */
	PLL_PARAMS(150, 1, 1), /* DCO = 1806.336M OD = 1 */
	{ /* sentinel */  }
};
#else
static const struct pll_params_table a4_hifi_pll_table[] = {
	PLL_PARAMS(192, 1), /* DCO = 4608M OD = 2 PLL = 1152M */
	PLL_PARAMS(150, 1), /* DCO = 1806.336M */
	{ /* sentinel */  }
};
#endif

/*
 * Internal hifi pll emulation configuration parameters
 */
static const struct reg_sequence a4_hifi_init_regs[] = {
	{ .reg = ANACTRL_HIFIPLL_CTRL1,	.def = 0x00010e56 },
	{ .reg = ANACTRL_HIFIPLL_CTRL2,	.def = 0x00000000 },
	{ .reg = ANACTRL_HIFIPLL_CTRL3,	.def = 0x6a285c00 },
	{ .reg = ANACTRL_HIFIPLL_CTRL4,	.def = 0x65771290 },
	{ .reg = ANACTRL_HIFIPLL_CTRL5,	.def = 0x39272000 },
	{ .reg = ANACTRL_HIFIPLL_CTRL6,	.def = 0x56540000 }
};

static struct clk_regmap a4_hifi_pll_dco = {
	.data = &(struct meson_clk_pll_data){
		.en = {
			.reg_off = ANACTRL_HIFIPLL_CTRL0,
			.shift   = 28,
			.width   = 1,
		},
		.m = {
			.reg_off = ANACTRL_HIFIPLL_CTRL0,
			.shift   = 0,
			.width   = 8,
		},
		.n = {
			.reg_off = ANACTRL_HIFIPLL_CTRL0,
			.shift   = 16,
			.width   = 5,
		},
		.od = {
			.reg_off = ANACTRL_HIFIPLL_CTRL0,
			.shift	 = 10,
			.width	 = 3,
		},
		.frac = {
			.reg_off = ANACTRL_HIFIPLL_CTRL1,
			.shift   = 0,
			.width   = 19,
		},
		.l = {
			.reg_off = ANACTRL_HIFIPLL_CTRL0,
			.shift   = 31,
			.width   = 1,
		},
		.rst = {
			.reg_off = ANACTRL_HIFIPLL_CTRL0,
			.shift   = 29,
			.width   = 1,
		},
		.table = a4_hifi_pll_table,
		.init_regs = a4_hifi_init_regs,
		.init_count = ARRAY_SIZE(a4_hifi_init_regs),
		.flags = CLK_MESON_PLL_ROUND_CLOSEST,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hifi_pll_dco",
		.ops = &meson_clk_pll_ops,
		.parent_data = (const struct clk_parent_data []) {
			{ .fw_name = "xtal", }
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

#ifdef CONFIG_ARM
static struct clk_regmap a4_hifi_pll = {
	.hw.init = &(struct clk_init_data){
		.name = "hifi_pll",
		.ops = &meson_pll_clk_no_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&a4_hifi_pll_dco.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};
#else
static struct clk_regmap a4_hifi_pll = {
	.data = &(struct clk_regmap_div_data){
		.offset = ANACTRL_HIFIPLL_CTRL0,
		.shift = 10,
		.width = 3,
		.flags = (CLK_DIVIDER_POWER_OF_TWO |
			  CLK_DIVIDER_ROUND_CLOSEST),
	},
	.hw.init = &(struct clk_init_data){
		.name = "hifi_pll",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&a4_hifi_pll_dco.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
	},
};
#endif

static struct clk_fixed_factor a4_mpll_50m = {
	.mult = 1,
	.div = 80,
	.hw.init = &(struct clk_init_data){
		.name = "mpll_50m_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&a4_fixed_pll_dco.hw
		},
		.num_parents = 1,
	},
};

/*
 *rtc 32k clock
 *
 *xtal--GATE------------------GATE---------------------|\
 *	              |  --------                      | \
 *	              |  |      |                      |  \
 *	              ---| DUAL |----------------------|   |
 *	                 |      |                      |   |____GATE__
 *	                 --------                      |   |     rtc_32k_out
 *	   PAD-----------------------------------------|  /
 *	                                               | /
 *	   DUAL function:                              |/
 *	   bit 28 in RTC_BY_OSCIN_CTRL0 control the dual function.
 *	   when bit 28 = 0
 *	         f = 24M/N0
 *	   when bit 28 = 1
 *	         output N1 and N2 in turns.
 *	   T = (x*T1 + y*T2)/x+y
 *	   f = (24M/(N0*M0 + N1*M1)) * (M0 + M1)
 *	   f: the frequecy value (HZ)
 *	       |      | |      |
 *	       | Div1 |-| Cnt1 |
 *	      /|______| |______|\
 *	    -|  ______   ______  ---> Out
 *	      \|      | |      |/
 *	       | Div2 |-| Cnt2 |
 *	       |______| |______|
 **/

/*
 * rtc 32k clock in gate
 */
static struct clk_regmap a4_rtc_32k_clkin = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_RTC_BY_OSCIN_CTRL0,
		.bit_idx = 31,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "rtc_32k_clkin",
		.ops = &clk_regmap_gate_ops,
		.parent_data = (const struct clk_parent_data []) {
			{ .fw_name = "xtal", }
		},
		.num_parents = 1,
	},
};

static const struct meson_clk_dualdiv_param a4_32k_div_table[] = {
	{
		.dual	= 1,
		.n1	= 733,
		.m1	= 8,
		.n2	= 732,
		.m2	= 11,
	},
	{}
};

static struct clk_regmap a4_rtc_32k_div = {
	.data = &(struct meson_clk_dualdiv_data){
		.n1 = {
			.reg_off = CLKCTRL_RTC_BY_OSCIN_CTRL0,
			.shift   = 0,
			.width   = 12,
		},
		.n2 = {
			.reg_off = CLKCTRL_RTC_BY_OSCIN_CTRL0,
			.shift   = 12,
			.width   = 12,
		},
		.m1 = {
			.reg_off = CLKCTRL_RTC_BY_OSCIN_CTRL1,
			.shift   = 0,
			.width   = 12,
		},
		.m2 = {
			.reg_off = CLKCTRL_RTC_BY_OSCIN_CTRL1,
			.shift   = 12,
			.width   = 12,
		},
		.dual = {
			.reg_off = CLKCTRL_RTC_BY_OSCIN_CTRL0,
			.shift   = 28,
			.width   = 1,
		},
		.table = a4_32k_div_table,
	},
	.hw.init = &(struct clk_init_data){
		.name = "rtc_32k_div",
		.ops = &meson_clk_dualdiv_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&a4_rtc_32k_clkin.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap a4_rtc_32k_xtal = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_RTC_BY_OSCIN_CTRL1,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "rtc_32k_xtal",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&a4_rtc_32k_clkin.hw
		},
		.num_parents = 1,
		.flags = CLK_IGNORE_UNUSED,
	},
};

/*
 * three parent for rtc clock out
 * pad is from where?
 */
static u32 rtc_32k_sel[] = {0, 1};
static struct clk_regmap a4_rtc_32k_sel = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_RTC_CTRL,
		.mask = 0x3,
		.shift = 0,
		.table = rtc_32k_sel,
		.flags = CLK_MUX_ROUND_CLOSEST,
	},
	.hw.init = &(struct clk_init_data){
		.name = "rtc_32k_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&a4_rtc_32k_xtal.hw,
			&a4_rtc_32k_div.hw
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap a4_rtc_clk = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_RTC_BY_OSCIN_CTRL0,
		.bit_idx = 30,
	},
	.hw.init = &(struct clk_init_data){
		.name = "rtc_clk",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&a4_rtc_32k_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/* sys clk */
static u32 mux_table_sys_ab_clk_sel[] = { 0, 1, 2, 3, 4, 7 };
static const struct clk_parent_data sys_ab_clk_parent_data[] = {
	{ .fw_name = "xtal", },
	{ .hw = &a4_gp1_pll.hw },
	{ .hw = &a4_fclk_div3.hw },
	{ .hw = &a4_fclk_div4.hw },
	{ .hw = &a4_fclk_div5.hw },
	{ .hw = &a4_rtc_clk.hw }
};

static struct clk_regmap a4_sysclk_1_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_SYS_CLK_CTRL0,
		.mask = 0x7,
		.shift = 26,
		.table = mux_table_sys_ab_clk_sel,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sysclk_1_sel",
		.ops = &clk_regmap_mux_ro_ops,
		.parent_data = sys_ab_clk_parent_data,
		.num_parents = ARRAY_SIZE(sys_ab_clk_parent_data),
	},
};

static struct clk_regmap a4_sysclk_1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_SYS_CLK_CTRL0,
		.shift = 16,
		.width = 10,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sysclk_1_div",
		.ops = &clk_regmap_divider_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&a4_sysclk_1_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap a4_sysclk_1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_SYS_CLK_CTRL0,
		.bit_idx = 29,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sysclk_1",
		.ops = &clk_regmap_gate_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&a4_sysclk_1_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED
			 | CLK_IS_CRITICAL,
	},
};

static struct clk_regmap a4_sysclk_0_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_SYS_CLK_CTRL0,
		.mask = 0x7,
		.shift = 10,
		.table = mux_table_sys_ab_clk_sel,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sysclk_0_sel",
		.ops = &clk_regmap_mux_ro_ops,
		.parent_data = sys_ab_clk_parent_data,
		.num_parents = ARRAY_SIZE(sys_ab_clk_parent_data),
	},
};

static struct clk_regmap a4_sysclk_0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_SYS_CLK_CTRL0,
		.shift = 0,
		.width = 10,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sysclk_0_div",
		.ops = &clk_regmap_divider_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&a4_sysclk_0_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap a4_sysclk_0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_SYS_CLK_CTRL0,
		.bit_idx = 13,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sysclk_0",
		.ops = &clk_regmap_gate_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&a4_sysclk_0_div.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap a4_sys_clk = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_SYS_CLK_CTRL0,
		.mask = 0x1,
		.shift = 31,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sys_clk",
		.ops = &clk_regmap_mux_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&a4_sysclk_0.hw,
			&a4_sysclk_1.hw
		},
		.num_parents = 2,
		.flags = CLK_IS_CRITICAL,
	},
};

static u32 mux_table_axi_clk_sel[] = { 0, 1, 2, 3, 4, 7 };

static const struct clk_parent_data a4_axi_clk_parent_data[] = {
	{ .fw_name = "xtal", },
	{ .hw = &a4_gp1_pll.hw },
	{ .hw = &a4_fclk_div3.hw },
	{ .hw = &a4_fclk_div4.hw },
	{ .hw = &a4_fclk_div5.hw },
	{ .hw = &a4_rtc_clk.hw }
};

static struct clk_regmap a4_axiclk_1_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_AXI_CLK_CTRL0,
		.mask = 0x7,
		.shift = 26,
		.table = mux_table_axi_clk_sel,
	},
	.hw.init = &(struct clk_init_data){
		.name = "axiclk_1_sel",
		.ops = &clk_regmap_mux_ro_ops,
		.parent_data = a4_axi_clk_parent_data,
		.num_parents = ARRAY_SIZE(a4_axi_clk_parent_data),
	},
};

static struct clk_regmap a4_axiclk_1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_AXI_CLK_CTRL0,
		.shift = 16,
		.width = 10,
		.flags = CLK_DIVIDER_ALLOW_ZERO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "axiclk_1_div",
		.ops = &clk_regmap_divider_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&a4_axiclk_1_sel.hw,
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap a4_axiclk_1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_AXI_CLK_CTRL0,
		.bit_idx = 29,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "axiclk_1",
		.ops = &clk_regmap_gate_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&a4_axiclk_1_div.hw,
		},
		.num_parents = 1,
	},
};

static struct clk_regmap a4_axiclk_0_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_AXI_CLK_CTRL0,
		.mask = 0x7,
		.shift = 10,
		.table = mux_table_axi_clk_sel,
	},
	.hw.init = &(struct clk_init_data){
		.name = "axiclk_0_sel",
		.ops = &clk_regmap_mux_ro_ops,
		.parent_data = a4_axi_clk_parent_data,
		.num_parents = ARRAY_SIZE(a4_axi_clk_parent_data),
	},
};

static struct clk_regmap a4_axiclk_0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_AXI_CLK_CTRL0,
		.shift = 0,
		.width = 10,
		.flags = CLK_DIVIDER_ALLOW_ZERO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "axiclk_0_div",
		.ops = &clk_regmap_divider_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&a4_axiclk_0_sel.hw,
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap a4_axiclk_0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_AXI_CLK_CTRL0,
		.bit_idx = 13,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "axiclk_0",
		.ops = &clk_regmap_gate_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&a4_axiclk_0_div.hw,
		},
		.num_parents = 1,
	},
};

static struct clk_regmap a4_axi_clk = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_AXI_CLK_CTRL0,
		.mask = 0x1,
		.shift = 15,
	},
	.hw.init = &(struct clk_init_data){
		.name = "axi_clk",
		.ops = &clk_regmap_mux_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&a4_axiclk_0.hw,
			&a4_axiclk_1.hw
		},
		.num_parents = 2,
		.flags = CLK_IS_CRITICAL,
	},
};

static u32 mux_table_rama_clk_sel[] = { 0, 1, 2, 3, 4, 6, 7 };

static const struct clk_parent_data a4_rama_clk_parent_data[] = {
	{ .fw_name = "xtal", },
	{ .hw = &a4_fclk_div2.hw },
	{ .hw = &a4_fclk_div3.hw },
	{ .hw = &a4_fclk_div4.hw },
	{ .hw = &a4_fclk_div5.hw },
	{ .hw = &a4_fclk_div7.hw },
	{ .hw = &a4_rtc_clk.hw }
};

static struct clk_regmap a4_ramaclk_1_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_RAMA_CLK_CTRL0,
		.mask = 0x7,
		.shift = 26,
		.table = mux_table_rama_clk_sel,
	},
	.hw.init = &(struct clk_init_data){
		.name = "ramaclk_1_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = a4_rama_clk_parent_data,
		.num_parents = ARRAY_SIZE(a4_rama_clk_parent_data),
	},
};

static struct clk_regmap a4_ramaclk_1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_RAMA_CLK_CTRL0,
		.shift = 16,
		.width = 10,
		.flags = CLK_DIVIDER_ALLOW_ZERO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "ramaclk_1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&a4_ramaclk_1_sel.hw,
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap a4_ramaclk_1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_RAMA_CLK_CTRL0,
		.bit_idx = 29,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "ramaclk_1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&a4_ramaclk_1_div.hw,
		},
		.num_parents = 1,
	},
};

static struct clk_regmap a4_ramaclk_0_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_RAMA_CLK_CTRL0,
		.mask = 0x7,
		.shift = 10,
		.table = mux_table_rama_clk_sel,
	},
	.hw.init = &(struct clk_init_data){
		.name = "ramaclk_0_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = a4_rama_clk_parent_data,
		.num_parents = ARRAY_SIZE(a4_rama_clk_parent_data),
	},
};

static struct clk_regmap a4_ramaclk_0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_RAMA_CLK_CTRL0,
		.shift = 0,
		.width = 10,
		.flags = CLK_DIVIDER_ALLOW_ZERO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "ramaclk_0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&a4_ramaclk_0_sel.hw,
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap a4_ramaclk_0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_RAMA_CLK_CTRL0,
		.bit_idx = 13,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "ramaclk_0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&a4_ramaclk_0_div.hw,
		},
		.num_parents = 1,
	},
};

static struct clk_regmap a4_rama_clk = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_RAMA_CLK_CTRL0,
		.mask = 0x1,
		.shift = 15,
	},
	.hw.init = &(struct clk_init_data){
		.name = "rama_clk",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&a4_ramaclk_0.hw,
			&a4_ramaclk_1.hw
		},
		.num_parents = 2,
		.flags = CLK_IS_CRITICAL,
	},
};

/*12_24M clk*/
static struct clk_regmap a4_24M_clk_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_CLK12_24_CTRL,
		.bit_idx = 11,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "24m",
		.ops = &clk_regmap_gate_ops,
		.parent_data = (const struct clk_parent_data []) {
			{ .fw_name = "xtal", }
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor a4_12M_clk_div = {
	.mult = 1,
	.div = 2,
	.hw.init = &(struct clk_init_data){
		.name = "24m_div2",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&a4_24M_clk_gate.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap a4_12M_clk_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_CLK12_24_CTRL,
		.bit_idx = 10,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "12m",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&a4_12M_clk_div.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap a4_25m_clk_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_CLK12_24_CTRL,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "25m_clk_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&a4_fclk_div2.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap a4_25m_clk = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_CLK12_24_CTRL,
		.bit_idx = 12,
	},
	.hw.init = &(struct clk_init_data){
		.name = "25m_clk",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&a4_25m_clk_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/*eth clk*/
static u32 a4_eth_rmii_table[] = { 0 };
static struct clk_regmap a4_eth_rmii_sel = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_ETH_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
		.table = a4_eth_rmii_table
	},
	.hw.init = &(struct clk_init_data){
		.name = "eth_rmii_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&a4_fclk_div2.hw,
		},
		.num_parents = 1
	},
};

static struct clk_regmap a4_eth_rmii_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_ETH_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "eth_rmii_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&a4_eth_rmii_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap a4_eth_rmii = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_ETH_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "eth_rmii",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&a4_eth_rmii_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static struct clk_fixed_factor a4_eth_div8 = {
	.mult = 1,
	.div = 8,
	.hw.init = &(struct clk_init_data){
		.name = "eth_div8",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) { &a4_fclk_div2.hw },
		.num_parents = 1,
	},
};

static struct clk_regmap a4_eth_125m = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_ETH_CLK_CTRL,
		.bit_idx = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "eth_125m",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&a4_eth_div8.hw
		},
		.num_parents = 1,
	},
};

/*ts_clk*/
static struct clk_regmap a4_ts_clk_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_TS_CLK_CTRL,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "ts_clk_div",
		.ops = &clk_regmap_divider_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "xtal",
		},
		.num_parents = 1,
	},
};

static struct clk_regmap a4_ts_clk = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_TS_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "ts_clk",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&a4_ts_clk_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

/* a4 cts_nand_clk*/
static u32 a4_sd_emmc_clk0_table[] = {0, 1, 2, 4, 7};
static const struct clk_parent_data a4_sd_emmc_clk0_parent_data[] = {
	{ .fw_name = "xtal", },
	{ .hw = &a4_fclk_div2.hw },
	{ .hw = &a4_fclk_div3.hw },
	{ .hw = &a4_fclk_div2p5.hw },
	{ .hw = &a4_gp0_pll.hw }
};

static struct clk_regmap a4_sd_emmc_c_clk0_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_NAND_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
		.table = a4_sd_emmc_clk0_table,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_c_clk0_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = a4_sd_emmc_clk0_parent_data,
		.num_parents = ARRAY_SIZE(a4_sd_emmc_clk0_parent_data),
	},
};

static struct clk_regmap a4_sd_emmc_c_clk0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_NAND_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_c_clk0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&a4_sd_emmc_c_clk0_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap a4_sd_emmc_c_clk0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_NAND_CLK_CTRL,
		.bit_idx = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sd_emmc_c_clk0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&a4_sd_emmc_c_clk0_div.hw
		},
		.num_parents = 1,
		.flags = CLK_IGNORE_UNUSED,
	},
};

/*cts_sd_emmc_a/b_clk*/
static struct clk_regmap a4_sd_emmc_a_clk0_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_SD_EMMC_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
		.table = a4_sd_emmc_clk0_table,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_a_clk0_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = a4_sd_emmc_clk0_parent_data,
		.num_parents = ARRAY_SIZE(a4_sd_emmc_clk0_parent_data),
	},
};

static struct clk_regmap a4_sd_emmc_a_clk0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_SD_EMMC_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_a_clk0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&a4_sd_emmc_a_clk0_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap a4_sd_emmc_a_clk0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_SD_EMMC_CLK_CTRL,
		.bit_idx = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sd_emmc_a_clk0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&a4_sd_emmc_a_clk0_div.hw
		},
		.num_parents = 1,
		.flags = CLK_IGNORE_UNUSED,
	},
};

/*cts_spicc_0/1_clk*/
static u32 a4_spicc_table[] = {0, 1, 2, 4, 7};
static const struct clk_parent_data a4_spicc_parent_hws[] = {
	{ .fw_name = "xtal", },
	{ .hw = &a4_fclk_div2.hw },
	{ .hw = &a4_fclk_div3.hw },
	{ .hw = &a4_fclk_div2p5.hw },
	{ .hw = &a4_gp0_pll.hw }
};

static struct clk_regmap a4_spicc0_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_SPICC_CLK_CTRL,
		.mask = 0x7,
		.shift = 7,
		.table = a4_spicc_table,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spicc0_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = a4_spicc_parent_hws,
		.num_parents = ARRAY_SIZE(a4_spicc_parent_hws),
	},
};

static struct clk_regmap a4_spicc0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_SPICC_CLK_CTRL,
		.shift = 0,
		.width = 6,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spicc0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&a4_spicc0_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap a4_spicc0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_SPICC_CLK_CTRL,
		.bit_idx = 6,
	},
	.hw.init = &(struct clk_init_data){
		.name = "spicc0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&a4_spicc0_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap a4_spicc1_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_SPICC_CLK_CTRL,
		.mask = 0x7,
		.shift = 23,
		.table = a4_spicc_table,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spicc1_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = a4_spicc_parent_hws,
		.num_parents = ARRAY_SIZE(a4_spicc_parent_hws),
	},
};

static struct clk_regmap a4_spicc1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_SPICC_CLK_CTRL,
		.shift = 16,
		.width = 6,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spicc1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&a4_spicc1_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap a4_spicc1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_SPICC_CLK_CTRL,
		.bit_idx = 22,
	},
	.hw.init = &(struct clk_init_data){
		.name = "spicc1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&a4_spicc1_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/*cts_pwm_*_clk*/
static u32 a4_pwm_clk_table[] = {0, 2, 3};
static const struct clk_parent_data a4_pwm_parent_data[] = {
	{ .fw_name = "xtal", },
	{ .hw = &a4_fclk_div4.hw },
	{ .hw = &a4_fclk_div3.hw }
};

static struct clk_regmap a4_pwm_a_sel = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_AB_CTRL,
		.mask = 0x3,
		.shift = 9,
		.table = a4_pwm_clk_table,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_a_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = a4_pwm_parent_data,
		.num_parents = ARRAY_SIZE(a4_pwm_parent_data),
		.flags = CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap a4_pwm_a_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_AB_CTRL,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_a_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&a4_pwm_a_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap a4_pwm_a = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_AB_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_a",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&a4_pwm_a_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap a4_pwm_b_sel = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_AB_CTRL,
		.mask = 0x3,
		.shift = 25,
		.table = a4_pwm_clk_table,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_b_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = a4_pwm_parent_data,
		.num_parents = ARRAY_SIZE(a4_pwm_parent_data),
		.flags = CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap a4_pwm_b_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_AB_CTRL,
		.shift = 16,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_b_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&a4_pwm_b_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap a4_pwm_b = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_AB_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_b",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&a4_pwm_b_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap a4_pwm_c_sel = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_CD_CTRL,
		.mask = 0x3,
		.shift = 9,
		.table = a4_pwm_clk_table,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_c_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = a4_pwm_parent_data,
		.num_parents = ARRAY_SIZE(a4_pwm_parent_data),
		.flags = CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap a4_pwm_c_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_CD_CTRL,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_c_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&a4_pwm_c_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap a4_pwm_c = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_CD_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_c",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&a4_pwm_c_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap a4_pwm_d_sel = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_CD_CTRL,
		.mask = 0x3,
		.shift = 25,
		.table = a4_pwm_clk_table,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_d_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = a4_pwm_parent_data,
		.num_parents = ARRAY_SIZE(a4_pwm_parent_data),
		.flags = CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap a4_pwm_d_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_CD_CTRL,
		.shift = 16,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_d_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&a4_pwm_d_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap a4_pwm_d = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_CD_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_d",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&a4_pwm_d_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap a4_pwm_e_sel = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_EF_CTRL,
		.mask = 0x3,
		.shift = 9,
		.table = a4_pwm_clk_table,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_e_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = a4_pwm_parent_data,
		.num_parents = ARRAY_SIZE(a4_pwm_parent_data),
		.flags = CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap a4_pwm_e_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_EF_CTRL,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_e_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&a4_pwm_e_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap a4_pwm_e = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_EF_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_e",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&a4_pwm_e_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap a4_pwm_f_sel = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_EF_CTRL,
		.mask = 0x3,
		.shift = 25,
		.table = a4_pwm_clk_table,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_f_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = a4_pwm_parent_data,
		.num_parents = ARRAY_SIZE(a4_pwm_parent_data),
		.flags = CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap a4_pwm_f_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_EF_CTRL,
		.shift = 16,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_f_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&a4_pwm_f_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap a4_pwm_f = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_EF_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_f",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&a4_pwm_f_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap a4_pwm_g_sel = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_GH_CTRL,
		.mask = 0x3,
		.shift = 9,
		.table = a4_pwm_clk_table,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_g_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = a4_pwm_parent_data,
		.num_parents = ARRAY_SIZE(a4_pwm_parent_data),
		.flags = CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap a4_pwm_g_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_GH_CTRL,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_g_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&a4_pwm_g_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap a4_pwm_g = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_GH_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_g",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&a4_pwm_g_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap a4_pwm_h_sel = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_GH_CTRL,
		.mask = 0x3,
		.shift = 25,
		.table = a4_pwm_clk_table,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_h_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = a4_pwm_parent_data,
		.num_parents = ARRAY_SIZE(a4_pwm_parent_data),
		.flags = CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap a4_pwm_h_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_GH_CTRL,
		.shift = 16,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_h_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&a4_pwm_h_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap a4_pwm_h = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_GH_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_h",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&a4_pwm_h_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static const struct clk_parent_data a4_saradc_clk_sel[] = {
	{ .fw_name = "xtal", },
	{ .hw = &a4_sys_clk.hw },
};

/*cts_sar_adc_clk*/
static struct clk_regmap a4_saradc_sel = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_SAR_CLK_CTRL0,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "saradc_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = a4_saradc_clk_sel,
		.num_parents = ARRAY_SIZE(a4_saradc_clk_sel),
	},
};

static struct clk_regmap a4_saradc_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_SAR_CLK_CTRL0,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "saradc_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&a4_saradc_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap a4_saradc = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_SAR_CLK_CTRL0,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "saradc",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&a4_saradc_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/* gen clk */
static u32 a4_gen_clk_mux_table[] = { 0, 1, 2, 6, 7, 12,
				17, 19, 20, 21, 22, 23, 24 };

static const struct clk_parent_data a4_gen_clk_parent_data[] = {
	{ .fw_name = "xtal", },
	{ .hw = &a4_rtc_clk.hw },
	{ .name = "sys_pll_div16" },
	{ .hw = &a4_gp1_pll.hw },
	{ .hw = &a4_hifi_pll.hw },
	{ .name = "cts_msr_clk" },
	{ .name = "sys_cpu_clk_div16" },
	{ .hw = &a4_fclk_div2.hw },
	{ .hw = &a4_fclk_div2p5.hw },
	{ .hw = &a4_fclk_div3.hw },
	{ .hw = &a4_fclk_div4.hw },
	{ .hw = &a4_fclk_div5.hw },
	{ .hw = &a4_fclk_div7.hw }
};

static struct clk_regmap a4_gen_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_GEN_CLK_CTRL,
		.mask = 0x1f,
		.shift = 12,
		.table = a4_gen_clk_mux_table,
	},
	.hw.init = &(struct clk_init_data){
		.name = "gen_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = a4_gen_clk_parent_data,
		.num_parents = ARRAY_SIZE(a4_gen_clk_parent_data),
	},
};

static struct clk_regmap a4_gen_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_GEN_CLK_CTRL,
		.shift = 0,
		.width = 11,
	},
	.hw.init = &(struct clk_init_data){
		.name = "gen_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&a4_gen_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap a4_gen = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_GEN_CLK_CTRL,
		.bit_idx = 11,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "gen",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&a4_gen_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/* cts_vout_mclk */
static u32 a4_vout_mclk_mux_table[] = { 0, 1, 2, 3, 4, 6, 7 };
static const struct clk_parent_data a4_vout_mclk_sel_table[] = {
	{ .hw = &a4_fclk_div2p5.hw },
	{ .hw = &a4_fclk_div3.hw },
	{ .hw = &a4_fclk_div4.hw },
	{ .hw = &a4_fclk_div5.hw },
	{ .hw = &a4_gp0_pll.hw },
	{ .hw = &a4_gp1_pll.hw },
	{ .hw = &a4_fclk_div7.hw },
};

static struct clk_regmap a4_vout_mclk_sel = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_VOUTENC_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
		.table = a4_vout_mclk_mux_table,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vout_mclk_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = a4_vout_mclk_sel_table,
		.num_parents = ARRAY_SIZE(a4_vout_mclk_sel_table),
	},
};

static struct clk_regmap a4_vout_mclk_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_VOUTENC_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vout_mclk_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&a4_vout_mclk_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap a4_vout_mclk = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_VOUTENC_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vout_mclk",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&a4_vout_mclk_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/* cts_vout_venc_clk */
static u32 a4_vout_venc_mclk_mux_table[] = { 0, 1, 2, 3, 4, 6, 7 };
static const struct clk_parent_data a4_vout_venc_mclk_sel_table[] = {
	{ .hw = &a4_gp1_pll.hw },
	{ .hw = &a4_fclk_div3.hw },
	{ .hw = &a4_fclk_div4.hw },
	{ .hw = &a4_fclk_div5.hw },
	{ .hw = &a4_gp0_pll.hw },
	{ .hw = &a4_fclk_div2p5.hw },
	{ .hw = &a4_fclk_div7.hw },
};

static struct clk_regmap a4_vout_venc_mclk_sel = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_VOUTENC_CLK_CTRL,
		.mask = 0x7,
		.shift = 25,
		.table = a4_vout_venc_mclk_mux_table,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vout_venc_mclk_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = a4_vout_venc_mclk_sel_table,
		.num_parents = ARRAY_SIZE(a4_vout_venc_mclk_sel_table),
	},
};

static struct clk_regmap a4_vout_venc_mclk_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_VOUTENC_CLK_CTRL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vout_venc_mclk_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&a4_vout_venc_mclk_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap a4_vout_venc_mclk = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_VOUTENC_CLK_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vout_venc_mclk",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&a4_vout_venc_mclk_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/* cts_audio_core_clk */
static u32 a4_audio_core_clk_mux_table[] = { 0, 1 };
static const struct clk_parent_data a4_audio_core_clk_sel_table[] = {
	{ .hw = &a4_fclk_div2p5.hw },
	{ .hw = &a4_gp1_pll.hw },
};

static struct clk_regmap a4_audio_core_clk_sel = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_AUDIO_CLK_CTRL,
		.mask = 0x7,
		.shift = 25,
		.table = a4_audio_core_clk_mux_table,
	},
	.hw.init = &(struct clk_init_data){
		.name = "audio_core_clk_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = a4_audio_core_clk_sel_table,
		.num_parents = ARRAY_SIZE(a4_audio_core_clk_sel_table),
	},
};

static struct clk_regmap a4_audio_core_clk_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_AUDIO_CLK_CTRL,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "audio_core_clk_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&a4_audio_core_clk_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap a4_audio_core_clk = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_AUDIO_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "audio_core_clk",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&a4_audio_core_clk_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

#define MESON_A4_SYS_GATE(_name, _reg, _bit)				\
struct clk_regmap _name = {						\
	.data = &(struct clk_regmap_gate_data) {			\
		.offset = (_reg),					\
		.bit_idx = (_bit),					\
	},								\
	.hw.init = &(struct clk_init_data) {				\
		.name = #_name,						\
		.ops = &clk_regmap_gate_ops,				\
		.parent_hws = (const struct clk_hw *[]) {		\
			&a4_sys_clk.hw					\
		},							\
		.num_parents = 1,					\
		.flags = CLK_IGNORE_UNUSED,				\
	},								\
}

/*CLKCTRL_SYS_CLK_EN0_REG0*/
static MESON_A4_SYS_GATE(a4_clk_ctrl, CLKCTRL_SYS_CLK_EN0_REG0, 0);
static MESON_A4_SYS_GATE(a4_reset_ctrl, CLKCTRL_SYS_CLK_EN0_REG0, 1);
static MESON_A4_SYS_GATE(a4_analog_ctrl, CLKCTRL_SYS_CLK_EN0_REG0, 2);
static MESON_A4_SYS_GATE(a4_pwr_ctrl, CLKCTRL_SYS_CLK_EN0_REG0, 3);
static MESON_A4_SYS_GATE(a4_pad_ctrl, CLKCTRL_SYS_CLK_EN0_REG0, 4);
static MESON_A4_SYS_GATE(a4_sys_ctrl, CLKCTRL_SYS_CLK_EN0_REG0, 5);
static MESON_A4_SYS_GATE(a4_ts_pll, CLKCTRL_SYS_CLK_EN0_REG0, 6);
static MESON_A4_SYS_GATE(a4_dev_arb, CLKCTRL_SYS_CLK_EN0_REG0, 7);
static MESON_A4_SYS_GATE(a4_mmc_pclk, CLKCTRL_SYS_CLK_EN0_REG0, 8);
static MESON_A4_SYS_GATE(a4_capu, CLKCTRL_SYS_CLK_EN0_REG0, 9);
static MESON_A4_SYS_GATE(a4_mailbox, CLKCTRL_SYS_CLK_EN0_REG0, 10);
static MESON_A4_SYS_GATE(a4_cpu_ctrl, CLKCTRL_SYS_CLK_EN0_REG0, 11);
static MESON_A4_SYS_GATE(a4_jtag_ctrl, CLKCTRL_SYS_CLK_EN0_REG0, 12);
static MESON_A4_SYS_GATE(a4_ir_ctrl, CLKCTRL_SYS_CLK_EN0_REG0, 13);
static MESON_A4_SYS_GATE(a4_irq_ctrl, CLKCTRL_SYS_CLK_EN0_REG0, 14);
static MESON_A4_SYS_GATE(a4_msr_clk, CLKCTRL_SYS_CLK_EN0_REG0, 15);
static MESON_A4_SYS_GATE(a4_rom, CLKCTRL_SYS_CLK_EN0_REG0, 16);
static MESON_A4_SYS_GATE(a4_aocpu, CLKCTRL_SYS_CLK_EN0_REG0, 17);
static MESON_A4_SYS_GATE(a4_cpu_apb, CLKCTRL_SYS_CLK_EN0_REG0, 18);
static MESON_A4_SYS_GATE(a4_rsa, CLKCTRL_SYS_CLK_EN0_REG0, 19);
static MESON_A4_SYS_GATE(a4_sar_adc, CLKCTRL_SYS_CLK_EN0_REG0, 20);
static MESON_A4_SYS_GATE(a4_startup, CLKCTRL_SYS_CLK_EN0_REG0, 21);
static MESON_A4_SYS_GATE(a4_secure, CLKCTRL_SYS_CLK_EN0_REG0, 22);
static MESON_A4_SYS_GATE(a4_spifc, CLKCTRL_SYS_CLK_EN0_REG0, 23);
static MESON_A4_SYS_GATE(a4_led_ctrl, CLKCTRL_SYS_CLK_EN0_REG0, 24);
static MESON_A4_SYS_GATE(a4_eth_phy, CLKCTRL_SYS_CLK_EN0_REG0, 25);
static MESON_A4_SYS_GATE(a4_eth_mac, CLKCTRL_SYS_CLK_EN0_REG0, 26);
static MESON_A4_SYS_GATE(a4_gic, CLKCTRL_SYS_CLK_EN0_REG0, 27);
static MESON_A4_SYS_GATE(a4_rama, CLKCTRL_SYS_CLK_EN0_REG0, 28);
static MESON_A4_SYS_GATE(a4_big_nic, CLKCTRL_SYS_CLK_EN0_REG0, 29);
static MESON_A4_SYS_GATE(a4_ramb, CLKCTRL_SYS_CLK_EN0_REG0, 30);

/*CLKCTRL_SYS_CLK_EN0_REG1*/
static MESON_A4_SYS_GATE(a4_audio_top, CLKCTRL_SYS_CLK_EN0_REG1, 0);
static MESON_A4_SYS_GATE(a4_audio_vad, CLKCTRL_SYS_CLK_EN0_REG1, 1);
static MESON_A4_SYS_GATE(a4_usb, CLKCTRL_SYS_CLK_EN0_REG1, 2);
static MESON_A4_SYS_GATE(a4_sd_emmca, CLKCTRL_SYS_CLK_EN0_REG1, 3);
static MESON_A4_SYS_GATE(a4_sd_emmcc, CLKCTRL_SYS_CLK_EN0_REG1, 4);
static MESON_A4_SYS_GATE(a4_pwm_ab, CLKCTRL_SYS_CLK_EN0_REG1, 5);
static MESON_A4_SYS_GATE(a4_pwm_cd, CLKCTRL_SYS_CLK_EN0_REG1, 6);
static MESON_A4_SYS_GATE(a4_pwm_ef, CLKCTRL_SYS_CLK_EN0_REG1, 7);
static MESON_A4_SYS_GATE(a4_pwm_gh, CLKCTRL_SYS_CLK_EN0_REG1, 8);
static MESON_A4_SYS_GATE(a4_spicc_1, CLKCTRL_SYS_CLK_EN0_REG1, 9);
static MESON_A4_SYS_GATE(a4_spicc_0, CLKCTRL_SYS_CLK_EN0_REG1, 10);
static MESON_A4_SYS_GATE(a4_uart_a, CLKCTRL_SYS_CLK_EN0_REG1, 11);
static MESON_A4_SYS_GATE(a4_uart_b, CLKCTRL_SYS_CLK_EN0_REG1, 12);
static MESON_A4_SYS_GATE(a4_uart_c, CLKCTRL_SYS_CLK_EN0_REG1, 13);
static MESON_A4_SYS_GATE(a4_uart_d, CLKCTRL_SYS_CLK_EN0_REG1, 14);
static MESON_A4_SYS_GATE(a4_uart_e, CLKCTRL_SYS_CLK_EN0_REG1, 15);
static MESON_A4_SYS_GATE(a4_i2c_m_a, CLKCTRL_SYS_CLK_EN0_REG1, 16);
static MESON_A4_SYS_GATE(a4_i2c_m_b, CLKCTRL_SYS_CLK_EN0_REG1, 17);
static MESON_A4_SYS_GATE(a4_i2c_m_c, CLKCTRL_SYS_CLK_EN0_REG1, 18);
static MESON_A4_SYS_GATE(a4_i2c_m_d, CLKCTRL_SYS_CLK_EN0_REG1, 19);
static MESON_A4_SYS_GATE(a4_rtc, CLKCTRL_SYS_CLK_EN0_REG1, 21);
static MESON_A4_SYS_GATE(a4_vout, CLKCTRL_SYS_CLK_EN0_REG1, 22);
static MESON_A4_SYS_GATE(a4_usb_ctrl, CLKCTRL_SYS_CLK_EN0_REG1, 26);
static MESON_A4_SYS_GATE(a4_acodec, CLKCTRL_SYS_CLK_EN0_REG1, 27);

/* Array of all clocks provided by this provider */
static struct clk_hw_onecell_data a4_hw_onecell_data = {
	.hws = {
		[CLKID_FIXED_PLL_DCO]			= &a4_fixed_pll_dco.hw,
		[CLKID_FIXED_PLL]			= &a4_fixed_pll.hw,
		[CLKID_SYS_PLL_DCO]			= &a4_sys_pll_dco.hw,
		[CLKID_SYS_PLL]				= &a4_sys_pll.hw,
		[CLKID_FCLK_DIV2_DIV]			= &a4_fclk_div2_div.hw,
		[CLKID_FCLK_DIV2]			= &a4_fclk_div2.hw,
		[CLKID_FCLK_DIV3_DIV]			= &a4_fclk_div3_div.hw,
		[CLKID_FCLK_DIV3]			= &a4_fclk_div3.hw,
		[CLKID_FCLK_DIV4_DIV]			= &a4_fclk_div4_div.hw,
		[CLKID_FCLK_DIV4]			= &a4_fclk_div4.hw,
		[CLKID_FCLK_DIV5_DIV]			= &a4_fclk_div5_div.hw,
		[CLKID_FCLK_DIV5]			= &a4_fclk_div5.hw,
		[CLKID_FCLK_DIV7_DIV]			= &a4_fclk_div7_div.hw,
		[CLKID_FCLK_DIV7]			= &a4_fclk_div7.hw,
		[CLKID_FCLK_DIV2P5_DIV]			= &a4_fclk_div2p5_div.hw,
		[CLKID_FCLK_DIV2P5]			= &a4_fclk_div2p5.hw,
		[CLKID_GP0_PLL_DCO]			= &a4_gp0_pll_dco.hw,
		[CLKID_GP0_PLL]				= &a4_gp0_pll.hw,
		[CLKID_GP1_PLL_DCO]			= &a4_gp1_pll_dco.hw,
		[CLKID_GP1_PLL]				= &a4_gp1_pll.hw,
		[CLKID_CPU_CLK_DYN]			= &a4_cpu_dyn_clk.hw,
		[CLKID_CPU_CLK]				= &a4_cpu_clk.hw,
		[CLKID_HIFI_PLL_DCO]			= &a4_hifi_pll_dco.hw,
		[CLKID_HIFI_PLL]			= &a4_hifi_pll.hw,
		[CLKID_MPLL_50M]			= &a4_mpll_50m.hw,

		[CLKID_RTC_32K_CLKIN]			= &a4_rtc_32k_clkin.hw,
		[CLKID_RTC_32K_DIV]			= &a4_rtc_32k_div.hw,
		[CLKID_RTC_32K_XATL]			= &a4_rtc_32k_xtal.hw,
		[CLKID_RTC_32K_SEL]			= &a4_rtc_32k_sel.hw,
		[CLKID_RTC_CLK]				= &a4_rtc_clk.hw,
		[CLKID_SYS_CLK_1_SEL]			= &a4_sysclk_1_sel.hw,
		[CLKID_SYS_CLK_1_DIV]			= &a4_sysclk_1_div.hw,
		[CLKID_SYS_CLK_1]			= &a4_sysclk_1.hw,
		[CLKID_SYS_CLK_0_SEL]			= &a4_sysclk_0_sel.hw,
		[CLKID_SYS_CLK_0_DIV]			= &a4_sysclk_0_div.hw,
		[CLKID_SYS_CLK_0]			= &a4_sysclk_0.hw,
		[CLKID_SYS_CLK]				= &a4_sys_clk.hw,
		[CLKID_AXI_CLK_1_SEL]			= &a4_axiclk_1_sel.hw,
		[CLKID_AXI_CLK_1_DIV]			= &a4_axiclk_1_div.hw,
		[CLKID_AXI_CLK_1]			= &a4_axiclk_1.hw,
		[CLKID_AXI_CLK_0_SEL]			= &a4_axiclk_0_sel.hw,
		[CLKID_AXI_CLK_0_DIV]			= &a4_axiclk_0_div.hw,
		[CLKID_AXI_CLK_0]			= &a4_axiclk_0.hw,
		[CLKID_AXI_CLK]				= &a4_axi_clk.hw,
		[CLKID_RAMA_CLK_1_SEL]			= &a4_ramaclk_1_sel.hw,
		[CLKID_RAMA_CLK_1_DIV]			= &a4_ramaclk_1_div.hw,
		[CLKID_RAMA_CLK_1]			= &a4_ramaclk_1.hw,
		[CLKID_RAMA_CLK_0_SEL]			= &a4_ramaclk_0_sel.hw,
		[CLKID_RAMA_CLK_0_DIV]			= &a4_ramaclk_0_div.hw,
		[CLKID_RAMA_CLK_0]			= &a4_ramaclk_0.hw,
		[CLKID_RAMA_CLK]			= &a4_rama_clk.hw,
		[CLKID_24M_CLK_GATE]			= &a4_24M_clk_gate.hw,
		[CLKID_12M_CLK_DIV]			= &a4_12M_clk_div.hw,
		[CLKID_12M_CLK_GATE]			= &a4_12M_clk_gate.hw,
		[CLKID_25M_CLK_DIV]			= &a4_25m_clk_div.hw,
		[CLKID_25M_CLK_GATE]			= &a4_25m_clk.hw,
		[CLKID_ETH_RMII_SEL]			= &a4_eth_rmii_sel.hw,
		[CLKID_ETH_RMII_DIV]			= &a4_eth_rmii_div.hw,
		[CLKID_ETH_RMII]			= &a4_eth_rmii.hw,
		[CLKID_ETH_DIV8]			= &a4_eth_div8.hw,
		[CLKID_ETH_125M]			= &a4_eth_125m.hw,
		[CLKID_TS_CLK_DIV]			= &a4_ts_clk_div.hw,
		[CLKID_TS_CLK]				= &a4_ts_clk.hw,
		[CLKID_SD_EMMC_C_CLK_SEL]		= &a4_sd_emmc_c_clk0_sel.hw,
		[CLKID_SD_EMMC_C_CLK_DIV]		= &a4_sd_emmc_c_clk0_div.hw,
		[CLKID_SD_EMMC_C_CLK]			= &a4_sd_emmc_c_clk0.hw,
		[CLKID_SD_EMMC_A_CLK_SEL]		= &a4_sd_emmc_a_clk0_sel.hw,
		[CLKID_SD_EMMC_A_CLK_DIV]		= &a4_sd_emmc_a_clk0_div.hw,
		[CLKID_SD_EMMC_A_CLK]			= &a4_sd_emmc_a_clk0.hw,
		[CLKID_SPICC0_SEL]			= &a4_spicc0_sel.hw,
		[CLKID_SPICC0_DIV]			= &a4_spicc0_div.hw,
		[CLKID_SPICC0]				= &a4_spicc0.hw,
		[CLKID_SPICC1_SEL]			= &a4_spicc1_sel.hw,
		[CLKID_SPICC1_DIV]			= &a4_spicc1_div.hw,
		[CLKID_SPICC1]				= &a4_spicc1.hw,
		[CLKID_PWM_A_SEL]			= &a4_pwm_a_sel.hw,
		[CLKID_PWM_A_DIV]			= &a4_pwm_a_div.hw,
		[CLKID_PWM_A]				= &a4_pwm_a.hw,
		[CLKID_PWM_B_SEL]			= &a4_pwm_b_sel.hw,
		[CLKID_PWM_B_DIV]			= &a4_pwm_b_div.hw,
		[CLKID_PWM_B]				= &a4_pwm_b.hw,
		[CLKID_PWM_C_SEL]			= &a4_pwm_c_sel.hw,
		[CLKID_PWM_C_DIV]			= &a4_pwm_c_div.hw,
		[CLKID_PWM_C]				= &a4_pwm_c.hw,
		[CLKID_PWM_D_SEL]			= &a4_pwm_d_sel.hw,
		[CLKID_PWM_D_DIV]			= &a4_pwm_d_div.hw,
		[CLKID_PWM_D]				= &a4_pwm_d.hw,
		[CLKID_PWM_E_SEL]			= &a4_pwm_e_sel.hw,
		[CLKID_PWM_E_DIV]			= &a4_pwm_e_div.hw,
		[CLKID_PWM_E]				= &a4_pwm_e.hw,
		[CLKID_PWM_F_SEL]			= &a4_pwm_f_sel.hw,
		[CLKID_PWM_F_DIV]			= &a4_pwm_f_div.hw,
		[CLKID_PWM_F]				= &a4_pwm_f.hw,
		[CLKID_PWM_G_SEL]			= &a4_pwm_g_sel.hw,
		[CLKID_PWM_G_DIV]			= &a4_pwm_g_div.hw,
		[CLKID_PWM_G]				= &a4_pwm_g.hw,
		[CLKID_PWM_H_SEL]			= &a4_pwm_h_sel.hw,
		[CLKID_PWM_H_DIV]			= &a4_pwm_h_div.hw,
		[CLKID_PWM_H]				= &a4_pwm_h.hw,
		[CLKID_SARADC_SEL]			= &a4_saradc_sel.hw,
		[CLKID_SARADC_DIV]			= &a4_saradc_div.hw,
		[CLKID_SARADC]				= &a4_saradc.hw,
		[CLKID_GEN_SEL]				= &a4_gen_sel.hw,
		[CLKID_GEN_DIV]				= &a4_gen_div.hw,
		[CLKID_GEN]				= &a4_gen.hw,
		[CLKID_VOUT_MCLK_SEL]			= &a4_vout_mclk_sel.hw,
		[CLKID_VOUT_MCLK_DIV]			= &a4_vout_mclk_div.hw,
		[CLKID_VOUT_MCLK]			= &a4_vout_mclk.hw,
		[CLKID_VOUT_VENC_MCLK_SEL]		= &a4_vout_venc_mclk_sel.hw,
		[CLKID_VOUT_VENC_MCLK_DIV]		= &a4_vout_venc_mclk_div.hw,
		[CLKID_VOUT_VENC_MCLK]			= &a4_vout_venc_mclk.hw,
		[CLKID_AUDIO_CORE_SEL]			= &a4_audio_core_clk_sel.hw,
		[CLKID_AUDIO_CORE_DIV]			= &a4_audio_core_clk_div.hw,
		[CLKID_AUDIO_CORE]			= &a4_audio_core_clk.hw,

		[CLKID_SYS_CLK_CTRL]			= &a4_clk_ctrl.hw,
		[CLKID_SYS_CLK_RESET_CTRL]		= &a4_reset_ctrl.hw,
		[CLKID_SYS_CLK_ANALOG_CTRL]		= &a4_analog_ctrl.hw,
		[CLKID_SYS_CLK_PWR_CTRL]		= &a4_pwr_ctrl.hw,
		[CLKID_SYS_CLK_PAD_CTRL]		= &a4_pad_ctrl.hw,
		[CLKID_SYS_CLK_SYS_CTRL]		= &a4_sys_ctrl.hw,
		[CLKID_SYS_CLK_TS_PLL]			= &a4_ts_pll.hw,
		[CLKID_SYS_CLK_DEV_ARB]			= &a4_dev_arb.hw,
		[CLKID_SYS_CLK_MMC_PCLK]		= &a4_mmc_pclk.hw,
		[CLKID_SYS_CLK_CAPU]			= &a4_capu.hw,
		[CLKID_SYS_CLK_MAILBOX]			= &a4_mailbox.hw,
		[CLKID_SYS_CLK_CPU_CTRL]		= &a4_cpu_ctrl.hw,
		[CLKID_SYS_CLK_JTAG_CTRL]		= &a4_jtag_ctrl.hw,
		[CLKID_SYS_CLK_IR_CTRL]			= &a4_ir_ctrl.hw,
		[CLKID_SYS_CLK_IRQ_CTRL]		= &a4_irq_ctrl.hw,
		[CLKID_SYS_CLK_MSR_CLK]			= &a4_msr_clk.hw,
		[CLKID_SYS_CLK_ROM]			= &a4_rom.hw,
		[CLKID_SYS_CLK_AOCPU]			= &a4_aocpu.hw,
		[CLKID_SYS_CLK_CPU_APB]			= &a4_cpu_apb.hw,
		[CLKID_SYS_CLK_RSA]			= &a4_rsa.hw,
		[CLKID_SYS_CLK_SAR_ADC]			= &a4_sar_adc.hw,
		[CLKID_SYS_CLK_STARTUP]			= &a4_startup.hw,
		[CLKID_SYS_CLK_SECURE]			= &a4_secure.hw,
		[CLKID_SYS_CLK_SPIFC]			= &a4_spifc.hw,
		[CLKID_SYS_CLK_LED_CTRL]		= &a4_led_ctrl.hw,
		[CLKID_SYS_CLK_ETH_PHY]			= &a4_eth_phy.hw,
		[CLKID_SYS_CLK_ETH_MAC]			= &a4_eth_mac.hw,
		[CLKID_SYS_CLK_GIC]			= &a4_gic.hw,
		[CLKID_SYS_CLK_RAMA]			= &a4_rama.hw,
		[CLKID_SYS_CLK_BIG_NIC]			= &a4_big_nic.hw,
		[CLKID_SYS_CLK_RAMB]			= &a4_ramb.hw,
		[CLKID_SYS_CLK_AUDIO_TOP]		= &a4_audio_top.hw,
		[CLKID_SYS_CLK_AUDIO_VAD]		= &a4_audio_vad.hw,
		[CLKID_SYS_CLK_USB]			= &a4_usb.hw,
		[CLKID_SYS_CLK_SD_EMMCA]		= &a4_sd_emmca.hw,
		[CLKID_SYS_CLK_SD_EMMCC]		= &a4_sd_emmcc.hw,
		[CLKID_SYS_CLK_PWM_AB]			= &a4_pwm_ab.hw,
		[CLKID_SYS_CLK_PWM_CD]			= &a4_pwm_cd.hw,
		[CLKID_SYS_CLK_PWM_EF]			= &a4_pwm_ef.hw,
		[CLKID_SYS_CLK_PWM_GH]			= &a4_pwm_gh.hw,
		[CLKID_SYS_CLK_SPICC_0]			= &a4_spicc_0.hw,
		[CLKID_SYS_CLK_SPICC_1]			= &a4_spicc_1.hw,
		[CLKID_SYS_CLK_UART_A]			= &a4_uart_a.hw,
		[CLKID_SYS_CLK_UART_B]			= &a4_uart_b.hw,
		[CLKID_SYS_CLK_UART_C]			= &a4_uart_c.hw,
		[CLKID_SYS_CLK_UART_D]			= &a4_uart_d.hw,
		[CLKID_SYS_CLK_UART_E]			= &a4_uart_e.hw,
		[CLKID_SYS_CLK_I2C_M_A]			= &a4_i2c_m_a.hw,
		[CLKID_SYS_CLK_I2C_M_B]			= &a4_i2c_m_b.hw,
		[CLKID_SYS_CLK_I2C_M_C]			= &a4_i2c_m_c.hw,
		[CLKID_SYS_CLK_I2C_M_D]			= &a4_i2c_m_d.hw,
		[CLKID_SYS_CLK_RTC]			= &a4_rtc.hw,
		[CLKID_SYS_CLK_VOUT]			= &a4_vout.hw,
		[CLKID_SYS_CLK_USB_CTRL]		= &a4_usb_ctrl.hw,
		[CLKID_SYS_CLK_ACODEC]			= &a4_acodec.hw,
		[NR_CLKS]				= NULL
	},
	.num = NR_CLKS,
};

/* Convenience table to populate regmap in .probe */
static struct clk_regmap *const a4_clk_regmaps[] __initconst  = {
	&a4_rtc_32k_clkin,
	&a4_rtc_32k_div,
	&a4_rtc_32k_xtal,
	&a4_rtc_32k_sel,
	&a4_rtc_clk,
	&a4_sysclk_1_sel,
	&a4_sysclk_1_div,
	&a4_sysclk_1,
	&a4_sysclk_0_sel,
	&a4_sysclk_0_div,
	&a4_sysclk_0,
	&a4_sys_clk,
	&a4_axiclk_1_sel,
	&a4_axiclk_1_div,
	&a4_axiclk_1,
	&a4_axiclk_0_sel,
	&a4_axiclk_0_div,
	&a4_axiclk_0,
	&a4_axi_clk,
	&a4_ramaclk_1_sel,
	&a4_ramaclk_1_div,
	&a4_ramaclk_1,
	&a4_ramaclk_0_sel,
	&a4_ramaclk_0_div,
	&a4_ramaclk_0,
	&a4_rama_clk,
	&a4_24M_clk_gate,
	&a4_12M_clk_gate,
	&a4_25m_clk_div,
	&a4_25m_clk,
	&a4_eth_rmii_sel,
	&a4_eth_rmii_div,
	&a4_eth_rmii,
	&a4_eth_125m,
	&a4_ts_clk_div,
	&a4_ts_clk,
	&a4_sd_emmc_c_clk0_sel,
	&a4_sd_emmc_c_clk0_div,
	&a4_sd_emmc_c_clk0,
	&a4_sd_emmc_a_clk0_sel,
	&a4_sd_emmc_a_clk0_div,
	&a4_sd_emmc_a_clk0,
	&a4_spicc0_sel,
	&a4_spicc0_div,
	&a4_spicc0,
	&a4_spicc1_sel,
	&a4_spicc1_div,
	&a4_spicc1,
	&a4_pwm_a_sel,
	&a4_pwm_a_div,
	&a4_pwm_a,
	&a4_pwm_b_sel,
	&a4_pwm_b_div,
	&a4_pwm_b,
	&a4_pwm_c_sel,
	&a4_pwm_c_div,
	&a4_pwm_c,
	&a4_pwm_d_sel,
	&a4_pwm_d_div,
	&a4_pwm_d,
	&a4_pwm_e_sel,
	&a4_pwm_e_div,
	&a4_pwm_e,
	&a4_pwm_f_sel,
	&a4_pwm_f_div,
	&a4_pwm_f,
	&a4_pwm_g_sel,
	&a4_pwm_g_div,
	&a4_pwm_g,
	&a4_pwm_h_sel,
	&a4_pwm_h_div,
	&a4_pwm_h,
	&a4_saradc_sel,
	&a4_saradc_div,
	&a4_saradc,
	&a4_gen_sel,
	&a4_gen_div,
	&a4_gen,
	&a4_vout_mclk_sel,
	&a4_vout_mclk_div,
	&a4_vout_mclk,
	&a4_vout_venc_mclk_sel,
	&a4_vout_venc_mclk_div,
	&a4_vout_venc_mclk,
	&a4_audio_core_clk_sel,
	&a4_audio_core_clk_div,
	&a4_audio_core_clk,
	&a4_clk_ctrl,
	&a4_reset_ctrl,
	&a4_analog_ctrl,
	&a4_pwr_ctrl,
	&a4_pad_ctrl,
	&a4_sys_ctrl,
	&a4_ts_pll,
	&a4_dev_arb,
	&a4_mmc_pclk,
	&a4_capu,
	&a4_mailbox,
	&a4_cpu_ctrl,
	&a4_jtag_ctrl,
	&a4_ir_ctrl,
	&a4_irq_ctrl,
	&a4_msr_clk,
	&a4_rom,
	&a4_aocpu,
	&a4_cpu_apb,
	&a4_rsa,
	&a4_sar_adc,
	&a4_startup,
	&a4_secure,
	&a4_spifc,
	&a4_led_ctrl,
	&a4_eth_phy,
	&a4_eth_mac,
	&a4_gic,
	&a4_rama,
	&a4_big_nic,
	&a4_ramb,
	&a4_audio_top,
	&a4_audio_vad,
	&a4_usb,
	&a4_sd_emmca,
	&a4_sd_emmcc,
	&a4_pwm_ab,
	&a4_pwm_cd,
	&a4_pwm_ef,
	&a4_pwm_gh,
	&a4_spicc_0,
	&a4_spicc_1,
	&a4_uart_a,
	&a4_uart_b,
	&a4_uart_c,
	&a4_uart_d,
	&a4_uart_e,
	&a4_i2c_m_a,
	&a4_i2c_m_b,
	&a4_i2c_m_c,
	&a4_i2c_m_d,
	&a4_rtc,
	&a4_vout,
	&a4_usb_ctrl,
	&a4_acodec,
};

static struct clk_regmap *const a4_cpu_clk_regmaps[] __initconst = {
	&a4_cpu_dyn_clk,
	&a4_cpu_clk,
};

static struct clk_regmap *const a4_pll_clk_regmaps[] __initconst = {
	&a4_fixed_pll_dco,
	&a4_fixed_pll,
	&a4_sys_pll_dco,
	&a4_sys_pll,
	&a4_fclk_div2,
	&a4_fclk_div3,
	&a4_fclk_div4,
	&a4_fclk_div5,
	&a4_fclk_div7,
	&a4_fclk_div2p5,
	&a4_gp0_pll_dco,
	&a4_gp0_pll,
	&a4_gp1_pll_dco,
	&a4_gp1_pll,
	&a4_hifi_pll_dco,
	&a4_hifi_pll,
};

static int meson_a4_dvfs_setup(struct platform_device *pdev)
{
	int ret;

	/* Setup clock notifier for sys_pll */
	ret = clk_notifier_register(a4_sys_pll.hw.clk,
				    &a4_sys_pll_nb_data.nb);
	if (ret) {
		dev_err(&pdev->dev, "failed to register sys_pll notifier\n");
		return ret;
	}

	return 0;
}

static struct regmap_config clkc_regmap_config = {
	.reg_bits       = 32,
	.val_bits       = 32,
	.reg_stride     = 4,
};

static struct regmap *a4_regmap_resource(struct device *dev, char *name)
{
	struct resource res;
	void __iomem *base;
	int i;
	struct device_node *node = dev->of_node;

	i = of_property_match_string(node, "reg-names", name);
	if (of_address_to_resource(node, i, &res))
		return ERR_PTR(-ENOENT);

	base = devm_ioremap_resource(dev, &res);
	if (IS_ERR(base))
		return ERR_CAST(base);

	clkc_regmap_config.max_register = resource_size(&res) - 4;
	clkc_regmap_config.name = devm_kasprintf(dev, GFP_KERNEL,
						 "%s-%s", node->name,
						 name);
	if (!clkc_regmap_config.name)
		return ERR_PTR(-ENOMEM);

	return devm_regmap_init_mmio(dev, base, &clkc_regmap_config);
}

static int __ref meson_a4_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct regmap *basic_map;
	struct regmap *pll_map;
	struct regmap *cpu_clk_map;
	const struct clk_hw_onecell_data *hw_onecell_data;
	int ret, i;

	hw_onecell_data = of_device_get_match_data(&pdev->dev);
	if (!hw_onecell_data)
		return -EINVAL;

	/* Get regmap for different clock area */
	basic_map = a4_regmap_resource(dev, "basic");
	if (IS_ERR(basic_map)) {
		dev_err(dev, "basic clk registers not found\n");
		return PTR_ERR(basic_map);
	}

	pll_map = a4_regmap_resource(dev, "pll");
	if (IS_ERR(pll_map)) {
		dev_err(dev, "pll clk registers not found\n");
		return PTR_ERR(pll_map);
	}

	cpu_clk_map = a4_regmap_resource(dev, "cpu_clk");
	if (IS_ERR(cpu_clk_map)) {
		dev_err(dev, "cpu clk registers not found\n");
		return PTR_ERR(cpu_clk_map);
	}

	/* Populate regmap for the regmap backed clocks */
	for (i = 0; i < ARRAY_SIZE(a4_clk_regmaps); i++)
		a4_clk_regmaps[i]->map = basic_map;

	for (i = 0; i < ARRAY_SIZE(a4_cpu_clk_regmaps); i++)
		a4_cpu_clk_regmaps[i]->map = cpu_clk_map;

	for (i = 0; i < ARRAY_SIZE(a4_pll_clk_regmaps); i++)
		a4_pll_clk_regmaps[i]->map = pll_map;

	for (i = 0; i < hw_onecell_data->num; i++) {
		/* array might be sparse */
		if (!hw_onecell_data->hws[i])
			continue;
		/*
		 *dev_err(dev, "register %d  %s\n",i,
		 *    hw_onecell_data->hws[i]->init->name);
		 */
		ret = devm_clk_hw_register(dev, hw_onecell_data->hws[i]);
		if (ret) {
			dev_err(dev, "Clock registration failed\n");
			return ret;
		}
	}

	meson_a4_dvfs_setup(pdev);

	return devm_of_clk_add_hw_provider(dev, of_clk_hw_onecell_get,
					   (void *)hw_onecell_data);
}

static const struct of_device_id clkc_match_table[] = {
	{
		.compatible = "amlogic,a4-clkc",
		.data = &a4_hw_onecell_data
	},
	{}
};

static struct platform_driver a4_driver = {
	.probe		= meson_a4_probe,
	.driver		= {
		.name	= "a4-clkc",
		.of_match_table = clkc_match_table,
	},
};

#ifndef CONFIG_AMLOGIC_MODIFY
builtin_platform_driver(a4_driver);
#else
#ifndef MODULE
static int __init a4_clkc_init(void)
{
	return platform_driver_register(&a4_driver);
}
arch_initcall_sync(a4_clkc_init);
#else
int __init meson_a4_clkc_init(void)
{
	return platform_driver_register(&a4_driver);
}
#endif
#endif

MODULE_LICENSE("GPL v2");
