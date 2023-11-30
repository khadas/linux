// SPDX-License-Identifier: GPL-2.0+
/*
 * Amlogic Meson-S4 Clock Controller Driver
 *
 * Copyright (c) 2018 Amlogic, inc.
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
#include "s4.h"
#include "clkcs_init.h"
#include <dt-bindings/clock/amlogic,s4-clkc.h>
#include "clk-secure.h"

static DEFINE_SPINLOCK(meson_clk_lock);

static struct clk_regmap s4_fixed_pll_dco = {
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
			.shift   = 10,
			.width   = 5,
		},
		.frac = {
			.reg_off = ANACTRL_FIXPLL_CTRL1,
			.shift   = 0,
			.width   = 19,
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

static struct clk_regmap s4_fixed_pll = {
	.data = &(struct clk_regmap_div_data){
		.offset = ANACTRL_FIXPLL_CTRL0,
		.shift = 16,
		.width = 2,
		.flags = CLK_DIVIDER_POWER_OF_TWO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "fixed_pll",
		.ops = &clk_regmap_divider_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_fixed_pll_dco.hw
		},
		.num_parents = 1,
		/*
		 * This clock won't ever change at runtime so
		 * CLK_SET_RATE_PARENT is not required
		 */
		.flags = CLK_IS_CRITICAL | CLK_GET_RATE_NOCACHE,
	},
};

#ifdef CONFIG_ARM
static const struct clk_ops meson_pll_clk_no_ops = {};
#endif

/*
 * the sys pll DCO value should be 3G~6G,
 * otherwise the sys pll can not lock.
 * od is for 32 bit.
 */

#ifdef CONFIG_ARM
static const struct pll_params_table s4_sys_pll_params_table[] = {
	PLL_PARAMS(168, 1, 2), /*DCO=4032M OD=1008M*/
	PLL_PARAMS(184, 1, 2), /*DCO=4416M OD=1104M*/
	PLL_PARAMS(200, 1, 2), /*DCO=4800M OD=1200M*/
	PLL_PARAMS(216, 1, 2), /*DCO=5184M OD=1296M*/
	PLL_PARAMS(233, 1, 2), /*DCO=5592M OD=1398M*/
	PLL_PARAMS(234, 1, 2), /*DCO=5616M OD=1404M*/
	PLL_PARAMS(249, 1, 2), /*DCO=5976M OD=1494M*/
	PLL_PARAMS(125, 1, 1), /*DCO=3000M OD=1500M*/
	PLL_PARAMS(126, 1, 1), /*DCO=3024M OD=1512M*/
	PLL_PARAMS(134, 1, 1), /*DCO=3216M OD=1608M*/
	PLL_PARAMS(142, 1, 1), /*DCO=3408M OD=1704M*/
	PLL_PARAMS(150, 1, 1), /*DCO=3600M OD=1800M*/
	PLL_PARAMS(158, 1, 1), /*DCO=3792M OD=1896M*/
	PLL_PARAMS(159, 1, 1), /*DCO=3816M OD=1908*/
	PLL_PARAMS(160, 1, 1), /*DCO=3840M OD=1920M*/
	PLL_PARAMS(167, 1, 1), /*DCO=4008M OD=2004M*/
	{ /* sentinel */ }
};
#else
static const struct pll_params_table s4_sys_pll_params_table[] = {
	PLL_PARAMS(168, 1), /*DCO=4032M OD=1008M*/
	PLL_PARAMS(184, 1), /*DCO=4416M OD=1104M*/
	PLL_PARAMS(200, 1), /*DCO=4800M OD=1200M*/
	PLL_PARAMS(216, 1), /*DCO=5184M OD=1296M*/
	PLL_PARAMS(233, 1), /*DCO=5592M OD=1398M*/
	PLL_PARAMS(234, 1), /*DCO=5616M OD=1404M*/
	PLL_PARAMS(249, 1), /*DCO=5976M OD=1494M*/
	PLL_PARAMS(125, 1), /*DCO=3000M OD=1500M*/
	PLL_PARAMS(126, 1), /*DCO=3024M OD=1512M*/
	PLL_PARAMS(134, 1), /*DCO=3216M OD=1608M*/
	PLL_PARAMS(142, 1), /*DCO=3408M OD=1704M*/
	PLL_PARAMS(150, 1), /*DCO=3600M OD=1800M*/
	PLL_PARAMS(158, 1), /*DCO=3792M OD=1896M*/
	PLL_PARAMS(159, 1), /*DCO=3816M OD=1908*/
	PLL_PARAMS(160, 1), /*DCO=3840M OD=1920M*/
	PLL_PARAMS(167, 1), /*DCO=4008M OD=2004M*/
	{ /* sentinel */ }
};
#endif

static struct clk_regmap s4_sys_pll_dco = {
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
			.shift   = 10,
			.width   = 5,
		},
#ifdef CONFIG_ARM
		/* od for 32bit */
		.od = {
			.reg_off = ANACTRL_SYSPLL_CTRL0,
			.shift	 = 16,
			.width	 = 3,
		},
#endif
		.table = s4_sys_pll_params_table,
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
		.flags = CLK_IS_CRITICAL,
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
static struct clk_regmap s4_sys_pll = {
	.hw.init = &(struct clk_init_data){
		.name = "sys_pll",
		.ops = &meson_pll_clk_no_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_sys_pll_dco.hw
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
static struct clk_regmap s4_sys_pll = {
	.data = &(struct clk_regmap_div_data){
		.offset = ANACTRL_SYSPLL_CTRL0,
		.shift = 16,
		.width = 3,
		.flags = CLK_DIVIDER_POWER_OF_TWO,
		.smc_id = SECURE_PLL_CLK,
		.secid = SECID_SYS0_PLL_OD
	},
	.hw.init = &(struct clk_init_data){
		.name = "sys_pll",
		.ops = &clk_regmap_secure_v2_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_sys_pll_dco.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};
#endif

static struct clk_fixed_factor s4_fclk_div2_div = {
	.mult = 1,
	.div = 2,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div2_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) { &s4_fixed_pll.hw },
		.num_parents = 1,
	},
};

static struct clk_regmap s4_fclk_div2 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = ANACTRL_FIXPLL_CTRL1,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div2",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_fclk_div2_div.hw
		},
		.num_parents = 1,
		.flags = CLK_IS_CRITICAL,
	},
};

static struct clk_fixed_factor s4_fclk_div3_div = {
	.mult = 1,
	.div = 3,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div3_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) { &s4_fixed_pll.hw },
		.num_parents = 1,
	},
};

static struct clk_regmap s4_fclk_div3 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = ANACTRL_FIXPLL_CTRL1,
		.bit_idx = 20,
	},
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div3",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_fclk_div3_div.hw
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

static struct clk_fixed_factor s4_fclk_div4_div = {
	.mult = 1,
	.div = 4,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div4_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) { &s4_fixed_pll.hw },
		.num_parents = 1,
	},
};

static struct clk_regmap s4_fclk_div4 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = ANACTRL_FIXPLL_CTRL1,
		.bit_idx = 21,
	},
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div4",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_fclk_div4_div.hw
		},
		.num_parents = 1,
		.flags = CLK_IS_CRITICAL,
	},
};

static struct clk_fixed_factor s4_fclk_div5_div = {
	.mult = 1,
	.div = 5,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div5_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) { &s4_fixed_pll.hw },
		.num_parents = 1,
	},
};

static struct clk_regmap s4_fclk_div5 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = ANACTRL_FIXPLL_CTRL1,
		.bit_idx = 22,
	},
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div5",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_fclk_div5_div.hw
		},
		.num_parents = 1,
		.flags = CLK_IS_CRITICAL,
	},
};

static struct clk_fixed_factor s4_fclk_div7_div = {
	.mult = 1,
	.div = 7,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div7_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) { &s4_fixed_pll.hw },
		.num_parents = 1,
	},
};

static struct clk_regmap s4_fclk_div7 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = ANACTRL_FIXPLL_CTRL1,
		.bit_idx = 23,
	},
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div7",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_fclk_div7_div.hw
		},
		.num_parents = 1,
		.flags = CLK_IS_CRITICAL,
	},
};

static struct clk_fixed_factor s4_fclk_div2p5_div = {
	.mult = 2,
	.div = 5,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div2p5_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_fixed_pll.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap s4_fclk_div2p5 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = ANACTRL_FIXPLL_CTRL1,
		.bit_idx = 25,
	},
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div2p5",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_fclk_div2p5_div.hw
		},
		.num_parents = 1,
		.flags = CLK_IS_CRITICAL,
	},
};

#ifdef CONFIG_ARM
static const struct pll_params_table s4_gp0_pll_table[] = {
	PLL_PARAMS(141, 1, 2), /* DCO = 3384M OD = 2 PLL = 846M */
	PLL_PARAMS(132, 1, 2), /* DCO = 3168M OD = 2 PLL = 792M */
	PLL_PARAMS(248, 1, 3), /* DCO = 5952M OD = 3 PLL = 744M */
	{ /* sentinel */  }
};
#else
static const struct pll_params_table s4_gp0_pll_table[] = {
	PLL_PARAMS(141, 1), /* DCO = 3384M OD = 2 PLL = 846M */
	PLL_PARAMS(132, 1), /* DCO = 3168M OD = 2 PLL = 792M */
	PLL_PARAMS(248, 1), /* DCO = 5952M OD = 3 PLL = 744M */
	{ /* sentinel */  }
};
#endif

/*
 * Internal gp0 pll emulation configuration parameters
 */
static const struct reg_sequence s4_gp0_init_regs[] = {
	{ .reg = ANACTRL_GP0PLL_CTRL1,	.def = 0x00000000 },
	{ .reg = ANACTRL_GP0PLL_CTRL2,	.def = 0x00000000 },
	{ .reg = ANACTRL_GP0PLL_CTRL3,	.def = 0x48681c00 },
	{ .reg = ANACTRL_GP0PLL_CTRL4,	.def = 0x88770290 },
	{ .reg = ANACTRL_GP0PLL_CTRL5,	.def = 0x39272000 },
	{ .reg = ANACTRL_GP0PLL_CTRL6,	.def = 0x56540000 }
};

static struct clk_regmap s4_gp0_pll_dco = {
	.data = &(struct meson_clk_pll_data){
		.en = {
			.reg_off = ANACTRL_GP0PLL_CTRL0,
			.shift   = 28,
			.width   = 1,
		},
		.m = {
			.reg_off = ANACTRL_GP0PLL_CTRL0,
			.shift   = 0,
			.width   = 8,
		},
		.n = {
			.reg_off = ANACTRL_GP0PLL_CTRL0,
			.shift   = 10,
			.width   = 5,
		},
#ifdef CONFIG_ARM
		/* for 32bit */
		.od = {
			.reg_off = ANACTRL_GP0PLL_CTRL0,
			.shift	 = 16,
			.width	 = 3,
		},
#endif
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
		.table = s4_gp0_pll_table,
		.init_regs = s4_gp0_init_regs,
		.init_count = ARRAY_SIZE(s4_gp0_init_regs),
	},
	.hw.init = &(struct clk_init_data){
		.name = "gp0_pll_dco",
		.ops = &meson_clk_pll_ops,
		.parent_data = (const struct clk_parent_data []) {
			{ .fw_name = "xtal", }
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

#ifdef CONFIG_ARM
static struct clk_regmap s4_gp0_pll = {
	.hw.init = &(struct clk_init_data){
		.name = "gp0_pll",
		.ops = &meson_pll_clk_no_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_gp0_pll_dco.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};
#else
static struct clk_regmap s4_gp0_pll = {
	.data = &(struct clk_regmap_div_data){
		.offset = ANACTRL_GP0PLL_CTRL0,
		.shift = 16,
		.width = 3,
		.flags = (CLK_DIVIDER_POWER_OF_TWO |
			  CLK_DIVIDER_ROUND_CLOSEST),
	},
	.hw.init = &(struct clk_init_data){
		.name = "gp0_pll",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_gp0_pll_dco.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
	},
};
#endif

#ifdef CONFIG_ARM
static const struct pll_params_table s4d_gp1_pll_table[] = {
	PLL_PARAMS(192, 1, 2), /* DCO = 4608M OD = 2 PLL = 1152M */
	PLL_PARAMS(132, 1, 2), /* DCO = 3168M OD = 2 PLL = 792M */
	PLL_PARAMS(200, 1, 2), /* DCO = 4800M OD = 2 PLL = 1200M*/
	{ /* sentinel */  }
};
#else
static const struct pll_params_table s4d_gp1_pll_table[] = {
	PLL_PARAMS(192, 1), /* DCO = 4608M OD = 2 PLL = 1152M */
	PLL_PARAMS(132, 1), /* DCO = 3168M OD = 2 PLL = 792M */
	PLL_PARAMS(200, 1), /* DCO = 4800M OD = 2 PLL = 1200M*/
	{ /* sentinel */  }
};
#endif

static struct clk_regmap s4d_gp1_pll_dco = {
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
			.shift   = 10,
			.width   = 5,
		},
		.od = {
			.reg_off = ANACTRL_GP1PLL_CTRL0,
			.shift	 = 16,
			.width	 = 3,
		},
		.frac = {
			.reg_off = ANACTRL_GP1PLL_CTRL1,
			.shift   = 0,
			.width   = 19,
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
		.table = s4d_gp1_pll_table,
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
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

#ifdef CONFIG_ARM
static struct clk_regmap s4d_gp1_pll = {
	.hw.init = &(struct clk_init_data){
		.name = "gp1_pll",
		.ops = &meson_pll_clk_no_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4d_gp1_pll_dco.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};
#else
static struct clk_regmap s4d_gp1_pll = {
	.data = &(struct clk_regmap_div_data){
		.offset = ANACTRL_GP1PLL_CTRL0,
		.shift = 16,
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
			&s4d_gp1_pll_dco.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
	},
};
#endif

/*cpu_clk*/
static const struct cpu_dyn_table s4_cpu_dyn_table[] = {
	CPU_LOW_PARAMS(24000000, 0, 0, 0),
	CPU_LOW_PARAMS(100000000, 1, 1, 9),
	CPU_LOW_PARAMS(250000000, 1, 1, 3),
	CPU_LOW_PARAMS(333333333, 2, 1, 1),
	CPU_LOW_PARAMS(500000000, 1, 1, 1),
	CPU_LOW_PARAMS(666666666, 2, 0, 0),
	CPU_LOW_PARAMS(1000000000, 1, 0, 0),
};

static struct clk_regmap s4_cpu_dyn_clk = {
	.data = &(struct meson_sec_cpu_dyn_data){
		.table = s4_cpu_dyn_table,
		.table_cnt = ARRAY_SIZE(s4_cpu_dyn_table),
		.secid_dyn_rd = SECID_CPU_CLK_RD,
		.secid_dyn = SECID_CPU_CLK_DYN,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cpu_dyn_clk",
		.ops = &meson_sec_cpu_dyn_ops,
		.parent_data = (const struct clk_parent_data []) {
			{ .fw_name = "xtal", },
			{ .hw = &s4_fclk_div2.hw },
			{ .hw = &s4_fclk_div3.hw }
		},
		.num_parents = 3,
	},
};

static struct clk_regmap s4_cpu_clk = {
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
			&s4_cpu_dyn_clk.hw,
			&s4_sys_pll.hw
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

struct s4_sys_pll_nb_data {
	struct notifier_block nb;
	struct clk_hw *sys_pll;
	struct clk_hw *cpu_clk;
	struct clk_hw *cpu_clk_dyn;
};

static int s4_sys_pll_notifier_cb(struct notifier_block *nb,
				   unsigned long event, void *data)
{
	struct s4_sys_pll_nb_data *nb_data =
		container_of(nb, struct s4_sys_pll_nb_data, nb);

	switch (event) {
	case PRE_RATE_CHANGE:
		/*
		 * This notifier means sys_pll clock will be changed
		 * to feed cpu_clk, this the current path :
		 * cpu_clk
		 *    \- sys_pll
		 *          \- sys_pll_dco
		 */

		/* make sure cpu_clk 1G*/
		if (clk_set_rate(nb_data->cpu_clk_dyn->clk, 1000000000))
			pr_err("%s in %d\n", __func__, __LINE__);
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

static struct s4_sys_pll_nb_data s4_sys_pll_nb_data = {
	.sys_pll = &s4_sys_pll.hw,
	.cpu_clk = &s4_cpu_clk.hw,
	.cpu_clk_dyn = &s4_cpu_dyn_clk.hw,
	.nb.notifier_call = s4_sys_pll_notifier_cb,
};

#ifdef CONFIG_ARM
static const struct pll_params_table s4_hifi_pll_table[] = {
	//PLL_PARAMS(192, 1, 2), /* DCO = 4608M OD = 2 PLL = 1152M */
	PLL_PARAMS(196, 1, 2), /* DCO = 4718.592M OD = 2 PLL = 1179.648M */
	PLL_PARAMS(150, 1, 1), /* DCO = 1806.336M OD = 1 */
	{ /* sentinel */  }
};
#else
static const struct pll_params_table s4_hifi_pll_table[] = {
	//PLL_PARAMS(192, 1), /* DCO = 4608M OD = 2 PLL = 1152M */
	PLL_PARAMS(196, 1), /* DCO = 4718.592M OD = 2 PLL = 1179.648M */
	PLL_PARAMS(150, 1), /* DCO = 1806.336M */
	{ /* sentinel */  }
};
#endif

/*
 * Internal hifi pll emulation configuration parameters
 */

/*
 * Because the audio of s4 and emmc use hifi_pll as the same parent clock,
 * fix the hifi_pll clock to 1179648000 to prevent clock adjustment
 */
#define S4_HIFI_PLL_DEAL

#ifndef S4_HIFI_PLL_DEAL
static const struct reg_sequence s4_hifi_init_regs[] = {
	{ .reg = ANACTRL_HIFIPLL_CTRL1,	.def = 0x0000ed80 },
	{ .reg = ANACTRL_HIFIPLL_CTRL2,	.def = 0x00000000 },
	{ .reg = ANACTRL_HIFIPLL_CTRL3,	.def = 0x6a285c00 },
	{ .reg = ANACTRL_HIFIPLL_CTRL4,	.def = 0x65771290 },
	{ .reg = ANACTRL_HIFIPLL_CTRL5,	.def = 0x39272000 },
	{ .reg = ANACTRL_HIFIPLL_CTRL6,	.def = 0x56540000 }
};
#else
static const struct reg_sequence s4_hifi_init_regs[] = {
	{ .reg = ANACTRL_HIFIPLL_CTRL0,	.def = 0X080204c4 },
	{ .reg = ANACTRL_HIFIPLL_CTRL0,	.def = 0x380204c4 },
	{ .reg = ANACTRL_HIFIPLL_CTRL1,	.def = 0x0000ed80 },
	{ .reg = ANACTRL_HIFIPLL_CTRL2,	.def = 0x00000000 },
	{ .reg = ANACTRL_HIFIPLL_CTRL3,	.def = 0x6a285c00 },
	{ .reg = ANACTRL_HIFIPLL_CTRL4,	.def = 0x65771290 },
	{ .reg = ANACTRL_HIFIPLL_CTRL5,	.def = 0x39272000 },
	{ .reg = ANACTRL_HIFIPLL_CTRL6,	.def = 0x56540000 },
	{ .reg = ANACTRL_HIFIPLL_CTRL0,	.def = 0x180204c4 }
};

#ifndef FRAC_BASE
#define FRAC_BASE	100000
#endif

#ifndef CLK_MESON_PLL_IGNORE_INIT
#define CLK_MESON_PLL_IGNORE_INIT			BIT(1)
#endif

#ifndef CLK_MESON_PLL_FIXED_FRAC_WEIGHT_PRECISION
#define CLK_MESON_PLL_FIXED_FRAC_WEIGHT_PRECISION	BIT(2)
#endif

static inline struct meson_clk_pll_data *
meson_clk_pll_data(struct clk_regmap *clk)
{
	return (struct meson_clk_pll_data *)clk->data;
}

#if defined CONFIG_ARM
static unsigned long __pll_params_to_rate(unsigned long parent_rate,
					  unsigned int m, unsigned int n,
					  unsigned int frac,
					  struct meson_clk_pll_data *pll,
					  unsigned int od)
#else
static unsigned long __pll_params_to_rate(unsigned long parent_rate,
					  unsigned int m, unsigned int n,
					  unsigned int frac,
					  struct meson_clk_pll_data *pll)
#endif
{
	u64 rate = (u64)parent_rate * m;
	u64 frac_rate;
	unsigned int frac_max;

	/*
	 * Assume that pll->frac.width = n, each bit represents the following:
	 * bit(n-1): positive or negative
	 * bit(n-2): fixed set to 1 by default
	 * bit(n-3) ~ bit0: frac value
	 */
	if (frac && pll->frac.width > 2) {
		frac_rate = (u64)parent_rate * frac;
		if (pll->flags & CLK_MESON_PLL_FIXED_FRAC_WEIGHT_PRECISION)
			frac_max = FRAC_BASE;
		else
			frac_max = 1 << (pll->frac.width - 2);

		if (frac & (1 << (pll->frac.width - 1))) {
			rate -= DIV_ROUND_UP_ULL(frac_rate, frac_max);
		} else {
			rate += DIV_ROUND_UP_ULL(frac_rate, frac_max);
		}
	}

	if (n == 0)
		return 0;

#if defined CONFIG_AMLOGIC_MODIFY && defined CONFIG_ARM
	return DIV_ROUND_UP_ULL(rate, n) >> od;
#else
	return DIV_ROUND_UP_ULL(rate, n);
#endif
}

static int meson_clk_pll_wait_lock(struct clk_hw *hw)
{
	struct clk_regmap *clk = to_clk_regmap(hw);
	struct meson_clk_pll_data *pll = meson_clk_pll_data(clk);
	int delay = 1000;

	do {
		/* Is the clock locked now ? */
		if (meson_parm_read(clk->map, &pll->l))
			return 0;
		udelay(1);
	} while (delay--);

	return -ETIMEDOUT;
}

static void meson_s4_clk_hifi_pll_init(struct clk_hw *hw)
{
	struct clk_regmap *clk = to_clk_regmap(hw);
	struct meson_clk_pll_data *pll = meson_clk_pll_data(clk);

	/* Do not init pll, it will gate pll which is needed in RTOS */
	if (pll->flags & CLK_MESON_PLL_IGNORE_INIT) {
		pr_warn("ignore %s clock init\n", clk_hw_get_name(hw));
		return;
	}

	if (pll->init_count) {
		meson_parm_write(clk->map, &pll->rst, 1);
		regmap_multi_reg_write(clk->map, pll->init_regs,
				       pll->init_count);
		meson_parm_write(clk->map, &pll->rst, 0);
	}
}

#if defined CONFIG_ARM
static unsigned long meson_s4_clk_hifi_pll_recalc_rate(struct clk_hw *hw,
					       unsigned long parent_rate)
{
	struct clk_regmap *clk = to_clk_regmap(hw);
	struct meson_clk_pll_data *pll = meson_clk_pll_data(clk);
	unsigned int m, n, frac, od;

	n = meson_parm_read(clk->map, &pll->n);
	m = meson_parm_read(clk->map, &pll->m);
	od = meson_parm_read(clk->map, &pll->od);

	frac = MESON_PARM_APPLICABLE(&pll->frac) ?
		meson_parm_read(clk->map, &pll->frac) :
		0;

	return __pll_params_to_rate(parent_rate, m, n, frac, pll, od);
}
#else
static unsigned long meson_s4_clk_hifi_pll_recalc_rate(struct clk_hw *hw,
					       unsigned long parent_rate)
{
	struct clk_regmap *clk = to_clk_regmap(hw);
	struct meson_clk_pll_data *pll = meson_clk_pll_data(clk);
	unsigned int m, n, frac;

	n = meson_parm_read(clk->map, &pll->n);

	/*
	 * On some HW, N is set to zero on init. This value is invalid as
	 * it would result in a division by zero. The rate can't be
	 * calculated in this case
	 */
	if (n == 0)
		return 0;

	m = meson_parm_read(clk->map, &pll->m);

	frac = MESON_PARM_APPLICABLE(&pll->frac) ?
		meson_parm_read(clk->map, &pll->frac) :
		0;

	return __pll_params_to_rate(parent_rate, m, n, frac, pll);
}
#endif

static int meson_s4_clk_hifi_pll_is_enabled(struct clk_hw *hw)
{
	struct clk_regmap *clk = to_clk_regmap(hw);
	struct meson_clk_pll_data *pll = meson_clk_pll_data(clk);

	if (meson_parm_read(clk->map, &pll->rst) ||
	    !meson_parm_read(clk->map, &pll->en) ||
	    !meson_parm_read(clk->map, &pll->l))
		return 0;

	return 1;
}

static int meson_s4_clk_hifi_pll_enable(struct clk_hw *hw)
{
	struct clk_regmap *clk = to_clk_regmap(hw);
	struct meson_clk_pll_data *pll = meson_clk_pll_data(clk);

	/* do nothing if the PLL is already enabled */
	if (clk_hw_is_enabled(hw))
		return 0;

	/* Make sure the pll is in reset */
	meson_parm_write(clk->map, &pll->rst, 1);

	/* Enable the pll */
	meson_parm_write(clk->map, &pll->en, 1);
	/*
	 * Make the PLL more stable, if not,
	 * It will probably lock failed (GP0 PLL)
	 */
	udelay(50);

	/* Take the pll out reset */
	meson_parm_write(clk->map, &pll->rst, 0);

	if (meson_clk_pll_wait_lock(hw))
		return -EIO;

	return 0;
}

static void meson_s4_clk_hifi_pll_disable(struct clk_hw *hw)
{
	struct clk_regmap *clk = to_clk_regmap(hw);
	struct meson_clk_pll_data *pll = meson_clk_pll_data(clk);

	/* Put the pll is in reset */
	meson_parm_write(clk->map, &pll->rst, 1);

	/* Disable the pll */
	meson_parm_write(clk->map, &pll->en, 0);
}

/* s4 hifi pll special ops */
const struct clk_ops meson_s4_clk_hifi_pll_ops = {
	.init		= meson_s4_clk_hifi_pll_init,
	.recalc_rate	= meson_s4_clk_hifi_pll_recalc_rate,
	.is_enabled	= meson_s4_clk_hifi_pll_is_enabled,
	.enable		= meson_s4_clk_hifi_pll_enable,
	.disable	= meson_s4_clk_hifi_pll_disable
};
#endif

static struct clk_regmap s4_hifi_pll_dco = {
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
			.shift   = 10,
			.width   = 5,
		},
#ifdef CONFIG_ARM
		/* for 32bit */
		.od = {
			.reg_off = ANACTRL_HIFIPLL_CTRL0,
			.shift	 = 16,
			.width	 = 2,
		},
#endif
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
		.table = s4_hifi_pll_table,
		.init_regs = s4_hifi_init_regs,
		.init_count = ARRAY_SIZE(s4_hifi_init_regs),
		.flags = CLK_MESON_PLL_ROUND_CLOSEST | CLK_MESON_PLL_FIXED_FRAC_WEIGHT_PRECISION
	},
	.hw.init = &(struct clk_init_data){
		.name = "hifi_pll_dco",
#ifdef S4_HIFI_PLL_DEAL
		.ops = &meson_s4_clk_hifi_pll_ops,
#else
		.ops = &meson_clk_pll_ops,
#endif
		.parent_data = (const struct clk_parent_data []) {
			{ .fw_name = "xtal", }
		},
		.num_parents = 1,
	},
};

#ifdef CONFIG_ARM
static struct clk_regmap s4_hifi_pll = {
	.hw.init = &(struct clk_init_data){
		.name = "hifi_pll",
		.ops = &meson_pll_clk_no_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_hifi_pll_dco.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};
#else
static struct clk_regmap s4_hifi_pll = {
	.data = &(struct clk_regmap_div_data){
		.offset = ANACTRL_HIFIPLL_CTRL0,
		.shift = 16,
		.width = 2,
		.flags = (CLK_DIVIDER_POWER_OF_TWO |
				CLK_DIVIDER_ROUND_CLOSEST),
	},
	.hw.init = &(struct clk_init_data){
		.name = "hifi_pll",
#ifdef S4_HIFI_PLL_DEAL
		.ops = &clk_regmap_divider_ro_ops,
#else
		.ops = &clk_regmap_divider_ops,
#endif
		.parent_hws = (const struct clk_hw *[]) {
			&s4_hifi_pll_dco.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
	},
};
#endif

#ifdef CONFIG_ARM
static const struct pll_params_table s4d_hifi_pll_table[] = {
	PLL_PARAMS(163, 1, 1), /* DCO = 3932.16M */
	/*PLL_PARAMS(150, 1, 1),  DCO = 1806.336M OD = 1 */
	{ /* sentinel */  }
};
#else
static const struct pll_params_table s4d_hifi_pll_table[] = {
	PLL_PARAMS(163, 1), /* DCO = 3932.16M */
	/*PLL_PARAMS(150, 1),  DCO = 1806.336M */
	{ /* sentinel */  }
};
#endif

/*
 * Internal hifi pll emulation configuration parameters
 */
static const struct reg_sequence s4d_hifi_init_regs[] = {
	{ .reg = ANACTRL_HIFIPLL_CTRL1,	.def = 0x00014820 },
	{ .reg = ANACTRL_HIFIPLL_CTRL2,	.def = 0x00000000 },
	{ .reg = ANACTRL_HIFIPLL_CTRL3,	.def = 0x6a285c00 },
	{ .reg = ANACTRL_HIFIPLL_CTRL4,	.def = 0x65771290 },
	{ .reg = ANACTRL_HIFIPLL_CTRL5,	.def = 0x39272000 },
	{ .reg = ANACTRL_HIFIPLL_CTRL6,	.def = 0x56540000 }
};

static struct clk_regmap s4d_hifi_pll_dco = {
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
			.shift   = 10,
			.width   = 5,
		},
#ifdef CONFIG_ARM
		/* for 32bit */
		.od = {
			.reg_off = ANACTRL_HIFIPLL_CTRL0,
			.shift	 = 16,
			.width	 = 2,
		},
#endif
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
		.table = s4d_hifi_pll_table,
		.init_regs = s4d_hifi_init_regs,
		.init_count = ARRAY_SIZE(s4d_hifi_init_regs),
		.flags = CLK_MESON_PLL_ROUND_CLOSEST | CLK_MESON_PLL_FIXED_FRAC_WEIGHT_PRECISION
	},
	.hw.init = &(struct clk_init_data){
		.name = "hifi_pll_dco",
		.ops = &meson_clk_pll_ops,
		.parent_data = (const struct clk_parent_data []) {
			{ .fw_name = "xtal", }
		},
		.num_parents = 1,
	},
};

#ifdef CONFIG_ARM
static struct clk_regmap s4d_hifi_pll = {
	.hw.init = &(struct clk_init_data){
		.name = "hifi_pll",
		.ops = &meson_pll_clk_no_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4d_hifi_pll_dco.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};
#else
static struct clk_regmap s4d_hifi_pll = {
	.data = &(struct clk_regmap_div_data){
		.offset = ANACTRL_HIFIPLL_CTRL0,
		.shift = 16,
		.width = 2,
		.flags = (CLK_DIVIDER_POWER_OF_TWO |
			  CLK_DIVIDER_ROUND_CLOSEST),
	},
	.hw.init = &(struct clk_init_data){
		.name = "hifi_pll",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4d_hifi_pll_dco.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
	},
};
#endif

static struct clk_regmap s4_hdmi_pll_dco = {
	.data = &(struct meson_clk_pll_data){
		.en = {
			.reg_off = ANACTRL_HDMIPLL_CTRL0,
			.shift   = 28,
			.width   = 1,
		},
		.m = {
			.reg_off = ANACTRL_HDMIPLL_CTRL0,
			.shift   = 0,
			.width   = 8,
		},
		.n = {
			.reg_off = ANACTRL_HDMIPLL_CTRL0,
			.shift   = 10,
			.width   = 5,
		},
		.frac = {
			.reg_off = ANACTRL_HDMIPLL_CTRL1,
			.shift   = 0,
			.width   = 19,
		},
		.l = {
			.reg_off = ANACTRL_HDMIPLL_CTRL0,
			.shift   = 31,
			.width   = 1,
		},
		.rst = {
			.reg_off = ANACTRL_HDMIPLL_CTRL0,
			.shift   = 29,
			.width   = 1,
		},
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmi_pll_dco",
		.ops = &meson_clk_pll_ro_ops,
		.parent_data = (const struct clk_parent_data []) {
			{ .fw_name = "xtal", }
		},
		.num_parents = 1,
		/*
		 * Display directly handle hdmi pll registers ATM, we need
		 * NOCACHE to keep our view of the clock as accurate as
		 * possible
		 */
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap s4_hdmi_pll_od = {
	.data = &(struct clk_regmap_div_data){
		.offset = ANACTRL_HDMIPLL_CTRL0,
		.shift = 16,
		.width = 4,
		.flags = CLK_DIVIDER_POWER_OF_TWO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmi_pll_od",
		.ops = &clk_regmap_divider_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_hdmi_pll_dco.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s4_hdmi_pll = {
	.data = &(struct clk_regmap_div_data){
		.offset = ANACTRL_HDMIPLL_CTRL0,
		.shift = 20,
		.width = 2,
		.flags = CLK_DIVIDER_POWER_OF_TWO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmi_pll",
		.ops = &clk_regmap_divider_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_hdmi_pll_od.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_fixed_factor s4_mpll_50m_div = {
	.mult = 1,
	.div = 80,
	.hw.init = &(struct clk_init_data){
		.name = "mpll_50m_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_fixed_pll_dco.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap s4_mpll_50m = {
	.data = &(struct clk_regmap_mux_data){
		.offset = ANACTRL_FIXPLL_CTRL3,
		.mask = 0x1,
		.shift = 5,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpll_50m",
		.ops = &clk_regmap_mux_ro_ops,
		.parent_data = (const struct clk_parent_data []) {
			{ .fw_name = "xtal", },
			{ .hw = &s4_mpll_50m_div.hw },
		},
		.num_parents = 2,
	},
};

static struct clk_fixed_factor s4_mpll_prediv = {
	.mult = 1,
	.div = 2,
	.hw.init = &(struct clk_init_data){
		.name = "mpll_prediv",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_fixed_pll_dco.hw
		},
		.num_parents = 1,
	},
};

static const struct reg_sequence s4_mpll0_init_regs[] = {
	{ .reg = ANACTRL_MPLL_CTRL0, .def = 0x00000543 }, /*write one time for mpll0/1/2/3*/
	{ .reg = ANACTRL_MPLL_CTRL2, .def = 0x40000033 }
};

static struct clk_regmap s4_mpll0_div = {
	.data = &(struct meson_clk_mpll_data){
		.sdm = {
			.reg_off = ANACTRL_MPLL_CTRL1,
			.shift   = 0,
			.width   = 14,
		},
		.sdm_en = {
			.reg_off = ANACTRL_MPLL_CTRL1,
			.shift   = 30,
			.width	 = 1,
		},
		.n2 = {
			.reg_off = ANACTRL_MPLL_CTRL1,
			.shift   = 20,
			.width   = 9,
		},
		.ssen = {
			.reg_off = ANACTRL_MPLL_CTRL1,
			.shift   = 29,
			.width	 = 1,
		},
		.lock = &meson_clk_lock,
		.init_regs = s4_mpll0_init_regs,
		.init_count = ARRAY_SIZE(s4_mpll0_init_regs),
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpll0_div",
		.ops = &meson_clk_mpll_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_mpll_prediv.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap s4_mpll0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = ANACTRL_MPLL_CTRL1,
		.bit_idx = 31,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpll0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &s4_mpll0_div.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct reg_sequence s4_mpll1_init_regs[] = {
	{ .reg = ANACTRL_MPLL_CTRL4,	.def = 0x40000033 }
};

static struct clk_regmap s4_mpll1_div = {
	.data = &(struct meson_clk_mpll_data){
		.sdm = {
			.reg_off = ANACTRL_MPLL_CTRL3,
			.shift   = 0,
			.width   = 14,
		},
		.sdm_en = {
			.reg_off = ANACTRL_MPLL_CTRL3,
			.shift   = 30,
			.width	 = 1,
		},
		.n2 = {
			.reg_off = ANACTRL_MPLL_CTRL3,
			.shift   = 20,
			.width   = 9,
		},
		.ssen = {
			.reg_off = ANACTRL_MPLL_CTRL3,
			.shift   = 29,
			.width	 = 1,
		},
		.lock = &meson_clk_lock,
		.init_regs = s4_mpll1_init_regs,
		.init_count = ARRAY_SIZE(s4_mpll1_init_regs),
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpll1_div",
		.ops = &meson_clk_mpll_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_mpll_prediv.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap s4_mpll1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = ANACTRL_MPLL_CTRL3,
		.bit_idx = 31,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpll1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &s4_mpll1_div.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct reg_sequence s4_mpll2_init_regs[] = {
	{ .reg = ANACTRL_MPLL_CTRL6, .def = 0x40000033 }
};

static struct clk_regmap s4_mpll2_div = {
	.data = &(struct meson_clk_mpll_data){
		.sdm = {
			.reg_off = ANACTRL_MPLL_CTRL5,
			.shift   = 0,
			.width   = 14,
		},
		.sdm_en = {
			.reg_off = ANACTRL_MPLL_CTRL5,
			.shift   = 30,
			.width	 = 1,
		},
		.n2 = {
			.reg_off = ANACTRL_MPLL_CTRL5,
			.shift   = 20,
			.width   = 9,
		},
		.ssen = {
			.reg_off = ANACTRL_MPLL_CTRL5,
			.shift   = 29,
			.width	 = 1,
		},
		.lock = &meson_clk_lock,
		.init_regs = s4_mpll2_init_regs,
		.init_count = ARRAY_SIZE(s4_mpll2_init_regs),
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpll2_div",
		.ops = &meson_clk_mpll_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_mpll_prediv.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap s4_mpll2 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = ANACTRL_MPLL_CTRL5,
		.bit_idx = 31,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpll2",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &s4_mpll2_div.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct reg_sequence s4_mpll3_init_regs[] = {
	{ .reg = ANACTRL_MPLL_CTRL8, .def = 0x40000033 }
};

static struct clk_regmap s4_mpll3_div = {
	.data = &(struct meson_clk_mpll_data){
		.sdm = {
			.reg_off = ANACTRL_MPLL_CTRL7,
			.shift   = 0,
			.width   = 14,
		},
		.sdm_en = {
			.reg_off = ANACTRL_MPLL_CTRL7,
			.shift   = 30,
			.width	 = 1,
		},
		.n2 = {
			.reg_off = ANACTRL_MPLL_CTRL7,
			.shift   = 20,
			.width   = 9,
		},
		.ssen = {
			.reg_off = ANACTRL_MPLL_CTRL7,
			.shift   = 29,
			.width	 = 1,
		},
		.lock = &meson_clk_lock,
		.init_regs = s4_mpll3_init_regs,
		.init_count = ARRAY_SIZE(s4_mpll3_init_regs),
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpll3_div",
		.ops = &meson_clk_mpll_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_mpll_prediv.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap s4_mpll3 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = ANACTRL_MPLL_CTRL7,
		.bit_idx = 31,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpll3",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &s4_mpll3_div.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
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
static struct clk_regmap s4_rtc_32k_clkin = {
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

static const struct meson_clk_dualdiv_param s4_32k_div_table[] = {
	{
		.dual	= 1,
		.n1	= 733,
		.m1	= 8,
		.n2	= 732,
		.m2	= 11,
	},
	{}
};

static struct clk_regmap s4_rtc_32k_div = {
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
		.table = s4_32k_div_table,
	},
	.hw.init = &(struct clk_init_data){
		.name = "rtc_32k_div",
		.ops = &meson_clk_dualdiv_ops,
		.parent_hws = (const struct clk_hw *[]) { &s4_rtc_32k_clkin.hw },
		.num_parents = 1,
	},
};

static struct clk_regmap s4_rtc_32k_xtal = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_RTC_BY_OSCIN_CTRL1,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "rtc_32k_xtal",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &s4_rtc_32k_clkin.hw },
		.num_parents = 1,
	},
};

/*
 * three parent for rtc clock out
 * pad is from where?
 */
static u32 rtc_32k_sel[] = {0, 1};
static const struct clk_parent_data rtc_32k_sel_parent_data[] = {
	{ .hw = &s4_rtc_32k_xtal.hw },
	{ .hw = &s4_rtc_32k_div.hw },
	{ .fw_name = "pad",  }
};

static struct clk_regmap s4_rtc_32k_sel = {
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
		.parent_data = rtc_32k_sel_parent_data,
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s4_rtc_clk = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_RTC_BY_OSCIN_CTRL0,
		.bit_idx = 30,
	},
	.hw.init = &(struct clk_init_data){
		.name = "rtc_clk",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_rtc_32k_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/* sys clk */
static u32 mux_table_sys_ab_clk_sel[] = { 0, 1, 2, 3, 4, 6, 7 };
static const struct clk_parent_data sys_ab_clk_parent_data[] = {
	{ .fw_name = "xtal", },
	{ .hw = &s4_fclk_div2.hw },
	{ .hw = &s4_fclk_div3.hw },
	{ .hw = &s4_fclk_div4.hw },
	{ .hw = &s4_fclk_div5.hw },
	{ .fw_name = "axi_clk_frcpu",},
	{ .hw = &s4_fclk_div7.hw },
	{ .hw = &s4_rtc_clk.hw }
};

static struct clk_regmap s4_sysclk_b_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_SYS_CLK_CTRL0,
		.mask = 0x7,
		.shift = 26,
		.table = mux_table_sys_ab_clk_sel,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sysclk_b_sel",
		.ops = &clk_regmap_mux_ro_ops,
		.parent_data = sys_ab_clk_parent_data,
		.num_parents = ARRAY_SIZE(sys_ab_clk_parent_data),
	},
};

static struct clk_regmap s4_sysclk_b_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_SYS_CLK_CTRL0,
		.shift = 16,
		.width = 10,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sysclk_b_div",
		.ops = &clk_regmap_divider_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_sysclk_b_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s4_sysclk_b = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_SYS_CLK_CTRL0,
		.bit_idx = 29,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sysclk_b",
		.ops = &clk_regmap_gate_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_sysclk_b_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED
			 | CLK_IS_CRITICAL,
	},
};

static struct clk_regmap s4_sysclk_a_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_SYS_CLK_CTRL0,
		.mask = 0x7,
		.shift = 10,
		.table = mux_table_sys_ab_clk_sel,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sysclk_a_sel",
		.ops = &clk_regmap_mux_ro_ops,
		.parent_data = sys_ab_clk_parent_data,
		.num_parents = ARRAY_SIZE(sys_ab_clk_parent_data),
	},
};

static struct clk_regmap s4_sysclk_a_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_SYS_CLK_CTRL0,
		.shift = 0,
		.width = 10,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sysclk_a_div",
		.ops = &clk_regmap_divider_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_sysclk_a_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s4_sysclk_a = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_SYS_CLK_CTRL0,
		.bit_idx = 13,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sysclk_a",
		.ops = &clk_regmap_gate_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_sysclk_a_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED
			 | CLK_IS_CRITICAL,
	},
};

static struct clk_regmap s4_sys_clk = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_SYS_CLK_CTRL0,
		.mask = 0x1,
		.shift = 31,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sys_clk",
		.ops = &clk_regmap_mux_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_sysclk_a.hw,
			&s4_sysclk_b.hw
		},
		.num_parents = 2,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

/*axi clk*/

/*ceca_clk*/
static struct clk_regmap s4_ceca_32k_clkin = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_CECA_CTRL0,
		.bit_idx = 31,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "ceca_32k_clkin",
		.ops = &clk_regmap_gate_ops,
		.parent_data = (const struct clk_parent_data []) {
			{ .fw_name = "xtal", }
		},
		.num_parents = 1,
	},
};

static struct clk_regmap s4_ceca_32k_div = {
	.data = &(struct meson_clk_dualdiv_data){
		.n1 = {
			.reg_off = CLKCTRL_CECA_CTRL0,
			.shift   = 0,
			.width   = 12,
		},
		.n2 = {
			.reg_off = CLKCTRL_CECA_CTRL0,
			.shift   = 12,
			.width   = 12,
		},
		.m1 = {
			.reg_off = CLKCTRL_CECA_CTRL1,
			.shift   = 0,
			.width   = 12,
		},
		.m2 = {
			.reg_off = CLKCTRL_CECA_CTRL1,
			.shift   = 12,
			.width   = 12,
		},
		.dual = {
			.reg_off = CLKCTRL_CECA_CTRL0,
			.shift   = 28,
			.width   = 1,
		},
		.table = s4_32k_div_table,
	},
	.hw.init = &(struct clk_init_data){
		.name = "ceca_32k_div",
		.ops = &meson_clk_dualdiv_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_ceca_32k_clkin.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap s4_ceca_32k_sel_pre = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_CECA_CTRL1,
		.mask = 0x1,
		.shift = 24,
		.flags = CLK_MUX_ROUND_CLOSEST,
	},
	.hw.init = &(struct clk_init_data){
		.name = "ceca_32k_sel_pre",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_ceca_32k_div.hw,
			&s4_ceca_32k_clkin.hw
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s4_ceca_32k_sel = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_CECA_CTRL1,
		.mask = 0x1,
		.shift = 31,
		.flags = CLK_MUX_ROUND_CLOSEST,
	},
	.hw.init = &(struct clk_init_data){
		.name = "ceca_32k_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_ceca_32k_sel_pre.hw,
			&s4_rtc_clk.hw
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s4_ceca_32k_clkout = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_CECA_CTRL0,
		.bit_idx = 30,
	},
	.hw.init = &(struct clk_init_data){
		.name = "ceca_32k_clkout",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_ceca_32k_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/*cecb_clk*/
static struct clk_regmap s4_cecb_32k_clkin = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_CECB_CTRL0,
		.bit_idx = 31,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "cecb_32k_clkin",
		.ops = &clk_regmap_gate_ops,
		.parent_data = (const struct clk_parent_data []) {
			{ .fw_name = "xtal", }
		},
		.num_parents = 1,
	},
};

static struct clk_regmap s4_cecb_32k_div = {
	.data = &(struct meson_clk_dualdiv_data){
		.n1 = {
			.reg_off = CLKCTRL_CECB_CTRL0,
			.shift   = 0,
			.width   = 12,
		},
		.n2 = {
			.reg_off = CLKCTRL_CECB_CTRL0,
			.shift   = 12,
			.width   = 12,
		},
		.m1 = {
			.reg_off = CLKCTRL_CECB_CTRL1,
			.shift   = 0,
			.width   = 12,
		},
		.m2 = {
			.reg_off = CLKCTRL_CECB_CTRL1,
			.shift   = 12,
			.width   = 12,
		},
		.dual = {
			.reg_off = CLKCTRL_CECB_CTRL0,
			.shift   = 28,
			.width   = 1,
		},
		.table = s4_32k_div_table,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cecb_32k_div",
		.ops = &meson_clk_dualdiv_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_cecb_32k_clkin.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap s4_cecb_32k_sel_pre = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_CECB_CTRL1,
		.mask = 0x1,
		.shift = 24,
		.flags = CLK_MUX_ROUND_CLOSEST,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cecb_32k_sel_pre",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_cecb_32k_div.hw,
			&s4_cecb_32k_clkin.hw
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s4_cecb_32k_sel = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_CECB_CTRL1,
		.mask = 0x1,
		.shift = 31,
		.flags = CLK_MUX_ROUND_CLOSEST,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cecb_32k_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_cecb_32k_sel_pre.hw,
			&s4_rtc_clk.hw
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s4_cecb_32k_clkout = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_CECB_CTRL0,
		.bit_idx = 30,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cecb_32k_clkout",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_cecb_32k_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/*cts_sc_clk*/
static const struct clk_parent_data s4_sc_parent_data[] = {
	{ .hw = &s4_fclk_div4.hw },
	{ .hw = &s4_fclk_div3.hw },
	{ .hw = &s4_fclk_div5.hw },
	{ .fw_name = "xtal", }
};

static struct clk_regmap s4_sc_clk_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_SC_CLK_CTRL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sc_clk_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = s4_sc_parent_data,
		.num_parents = ARRAY_SIZE(s4_sc_parent_data),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap s4_sc_clk_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_SC_CLK_CTRL,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sc_clk_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_sc_clk_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s4_sc_clk_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_SC_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sc_clk_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_sc_clk_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

/*rama_clk*/

/*12_24M clk*/
static struct clk_regmap s4_24M_clk_gate = {
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

static struct clk_fixed_factor s4_12M_clk_div = {
	.mult = 1,
	.div = 2,
	.hw.init = &(struct clk_init_data){
		.name = "24m_div2",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_24M_clk_gate.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap s4_12M_clk_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_CLK12_24_CTRL,
		.bit_idx = 10,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "12m",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_12M_clk_div.hw
		},
		.num_parents = 1,
	},
};

/* Video Clocks */
static struct clk_regmap s4_vid_pll_div = {
	.data = &(struct meson_vid_pll_div_data){
		.val = {
			.reg_off = CLKCTRL_VID_PLL_CLK_DIV,
			.shift   = 0,
			.width   = 15,
		},
		.sel = {
			.reg_off = CLKCTRL_VID_PLL_CLK_DIV,
			.shift   = 16,
			.width   = 2,
		},
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vid_pll_div",
		.ops = &meson_vid_pll_div_ro_ops,
		.parent_hws = (const struct clk_hw *[]) { &s4_hdmi_pll.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap s4_vid_pll_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VID_PLL_CLK_DIV,
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
		.parent_hws = (const struct clk_hw *[]) {
			&s4_vid_pll_div.hw,
			&s4_hdmi_pll.hw
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_NO_REPARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap s4_vid_pll = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VID_PLL_CLK_DIV,
		.bit_idx = 19,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vid_pll",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_vid_pll_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static const struct clk_hw *s4_vclk_parent_hws[] = {
	&s4_vid_pll.hw,
	&s4_gp0_pll.hw,
	&s4_hifi_pll.hw,
	&s4_mpll1.hw,
	&s4_fclk_div3.hw,
	&s4_fclk_div4.hw,
	&s4_fclk_div5.hw,
	&s4_fclk_div7.hw
};

static struct clk_regmap s4_vclk_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VID_CLK_CTRL,
		.mask = 0x7,
		.shift = 16,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vclk_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = s4_vclk_parent_hws,
		.num_parents = ARRAY_SIZE(s4_vclk_parent_hws),
		.flags = CLK_SET_RATE_NO_REPARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap s4_vclk2_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VIID_CLK_CTRL,
		.mask = 0x7,
		.shift = 16,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vclk2_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = s4_vclk_parent_hws,
		.num_parents = ARRAY_SIZE(s4_vclk_parent_hws),
		.flags = CLK_SET_RATE_NO_REPARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap s4_vclk_input = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VID_CLK_DIV,
		.bit_idx = 16,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk_input",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &s4_vclk_sel.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap s4_vclk2_input = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VIID_CLK_DIV,
		.bit_idx = 16,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk2_input",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &s4_vclk2_sel.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap s4_vclk_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_VID_CLK_DIV,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vclk_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_vclk_input.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap s4_vclk2_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_VIID_CLK_DIV,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vclk2_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_vclk2_input.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap s4_vclk = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VID_CLK_CTRL,
		.bit_idx = 19,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &s4_vclk_div.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap s4_vclk2 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VIID_CLK_CTRL,
		.bit_idx = 19,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk2",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &s4_vclk2_div.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap s4_vclk_div1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VID_CLK_CTRL,
		.bit_idx = 0,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk_div1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &s4_vclk.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap s4_vclk_div2_en = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VID_CLK_CTRL,
		.bit_idx = 1,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk_div2_en",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &s4_vclk.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap s4_vclk_div4_en = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VID_CLK_CTRL,
		.bit_idx = 2,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk_div4_en",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &s4_vclk.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap s4_vclk_div6_en = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VID_CLK_CTRL,
		.bit_idx = 3,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk_div6_en",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &s4_vclk.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap s4_vclk_div12_en = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VID_CLK_CTRL,
		.bit_idx = 4,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk_div12_en",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &s4_vclk.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap s4_vclk2_div1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VIID_CLK_CTRL,
		.bit_idx = 0,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk2_div1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &s4_vclk2.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap s4_vclk2_div2_en = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VIID_CLK_CTRL,
		.bit_idx = 1,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk2_div2_en",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &s4_vclk2.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap s4_vclk2_div4_en = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VIID_CLK_CTRL,
		.bit_idx = 2,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk2_div4_en",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &s4_vclk2.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap s4_vclk2_div6_en = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VIID_CLK_CTRL,
		.bit_idx = 3,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk2_div6_en",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &s4_vclk2.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap s4_vclk2_div12_en = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VIID_CLK_CTRL,
		.bit_idx = 4,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk2_div12_en",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &s4_vclk2.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_fixed_factor s4_vclk_div2 = {
	.mult = 1,
	.div = 2,
	.hw.init = &(struct clk_init_data){
		.name = "vclk_div2",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_vclk_div2_en.hw
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor s4_vclk_div4 = {
	.mult = 1,
	.div = 4,
	.hw.init = &(struct clk_init_data){
		.name = "vclk_div4",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_vclk_div4_en.hw
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor s4_vclk_div6 = {
	.mult = 1,
	.div = 6,
	.hw.init = &(struct clk_init_data){
		.name = "vclk_div6",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_vclk_div6_en.hw
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor s4_vclk_div12 = {
	.mult = 1,
	.div = 12,
	.hw.init = &(struct clk_init_data){
		.name = "vclk_div12",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_vclk_div12_en.hw
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor s4_vclk2_div2 = {
	.mult = 1,
	.div = 2,
	.hw.init = &(struct clk_init_data){
		.name = "vclk2_div2",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_vclk2_div2_en.hw
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor s4_vclk2_div4 = {
	.mult = 1,
	.div = 4,
	.hw.init = &(struct clk_init_data){
		.name = "vclk2_div4",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_vclk2_div4_en.hw
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor s4_vclk2_div6 = {
	.mult = 1,
	.div = 6,
	.hw.init = &(struct clk_init_data){
		.name = "vclk2_div6",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_vclk2_div6_en.hw
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor s4_vclk2_div12 = {
	.mult = 1,
	.div = 12,
	.hw.init = &(struct clk_init_data){
		.name = "vclk2_div12",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_vclk2_div12_en.hw
		},
		.num_parents = 1,
	},
};

static u32 mux_table_cts_sel[] = { 0, 1, 2, 3, 4, 8, 9, 10, 11, 12 };
static const struct clk_hw *s4_cts_parent_hws[] = {
	&s4_vclk_div1.hw,
	&s4_vclk_div2.hw,
	&s4_vclk_div4.hw,
	&s4_vclk_div6.hw,
	&s4_vclk_div12.hw,
	&s4_vclk2_div1.hw,
	&s4_vclk2_div2.hw,
	&s4_vclk2_div4.hw,
	&s4_vclk2_div6.hw,
	&s4_vclk2_div12.hw
};

static struct clk_regmap s4_cts_enci_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VID_CLK_DIV,
		.mask = 0xf,
		.shift = 28,
		.table = mux_table_cts_sel,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cts_enci_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = s4_cts_parent_hws,
		.num_parents = ARRAY_SIZE(s4_cts_parent_hws),
		.flags = CLK_SET_RATE_NO_REPARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap s4_cts_encp_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VID_CLK_DIV,
		.mask = 0xf,
		.shift = 20,
		.table = mux_table_cts_sel,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cts_encp_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = s4_cts_parent_hws,
		.num_parents = ARRAY_SIZE(s4_cts_parent_hws),
		.flags = CLK_SET_RATE_NO_REPARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap s4_cts_vdac_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VIID_CLK_DIV,
		.mask = 0xf,
		.shift = 28,
		.table = mux_table_cts_sel,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cts_vdac_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = s4_cts_parent_hws,
		.num_parents = ARRAY_SIZE(s4_cts_parent_hws),
		.flags = CLK_SET_RATE_NO_REPARENT | CLK_GET_RATE_NOCACHE,
	},
};

/* TOFIX: add support for cts_tcon */
static u32 mux_table_hdmi_tx_sel[] = { 0, 1, 2, 3, 4, 8, 9, 10, 11, 12 };
static const struct clk_hw *s4_cts_hdmi_tx_parent_hws[] = {
	&s4_vclk_div1.hw,
	&s4_vclk_div2.hw,
	&s4_vclk_div4.hw,
	&s4_vclk_div6.hw,
	&s4_vclk_div12.hw,
	&s4_vclk2_div1.hw,
	&s4_vclk2_div2.hw,
	&s4_vclk2_div4.hw,
	&s4_vclk2_div6.hw,
	&s4_vclk2_div12.hw
};

static struct clk_regmap s4_hdmi_tx_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_HDMI_CLK_CTRL,
		.mask = 0xf,
		.shift = 16,
		.table = mux_table_hdmi_tx_sel,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmi_tx_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = s4_cts_hdmi_tx_parent_hws,
		.num_parents = ARRAY_SIZE(s4_cts_hdmi_tx_parent_hws),
		.flags = CLK_SET_RATE_NO_REPARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap s4_cts_enci = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VID_CLK_CTRL2,
		.bit_idx = 0,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "cts_enci",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_cts_enci_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap s4_cts_encp = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VID_CLK_CTRL2,
		.bit_idx = 2,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "cts_encp",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_cts_encp_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap s4_cts_vdac = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VID_CLK_CTRL2,
		.bit_idx = 4,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "cts_vdac",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_cts_vdac_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap s4_hdmi_tx = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VID_CLK_CTRL2,
		.bit_idx = 5,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmi_tx",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_hdmi_tx_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

/*hdmi_clk*/
static const struct clk_parent_data s4_hdmi_parent_data[] = {
	{ .fw_name = "xtal", },
	{ .hw = &s4_fclk_div4.hw },
	{ .hw = &s4_fclk_div3.hw },
	{ .hw = &s4_fclk_div5.hw }
};

static struct clk_regmap s4_hdmi_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_HDMI_CLK_CTRL,
		.mask = 0x3,
		.shift = 9,
		.flags = CLK_MUX_ROUND_CLOSEST,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmi_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = s4_hdmi_parent_data,
		.num_parents = ARRAY_SIZE(s4_hdmi_parent_data),
		.flags = CLK_SET_RATE_NO_REPARENT | CLK_GET_RATE_NOCACHE |
			 CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap s4_hdmi_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_HDMI_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmi_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) { &s4_hdmi_sel.hw },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT |
			 CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap s4_hdmi = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_HDMI_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmi",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &s4_hdmi_div.hw },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT |
			 CLK_IGNORE_UNUSED,
	},
};

/*vid_lock_clk*/

/*eth_clk :125M*/

/*ts_clk*/
static struct clk_regmap s4_ts_clk_div = {
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

static struct clk_regmap s4_ts_clk_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_TS_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "ts_clk_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_ts_clk_div.hw
		},
		.num_parents = 1,
	},
};

/*mali_clk*/
/*
 * The MALI IP is clocked by two identical clocks (mali_0 and mali_1)
 * muxed by a glitch-free switch on Meson8b and Meson8m2 and later.
 *
 * CLK_SET_RATE_PARENT is added for mali_0_sel clock
 * 1.gp0 pll only support the 846M, avoid other rate 500/400M from it
 * 2.hifi pll is used for other module, skip it, avoid some rate from it
 */
static u32 mux_table_mali[] = { 0, 1, 3, 4, 5, 6, 7 };

static const struct clk_parent_data s4_mali_0_1_parent_data[] = {
	{ .fw_name = "xtal", },
	{ .hw = &s4_gp0_pll.hw },
	{ .hw = &s4_fclk_div2p5.hw },
	{ .hw = &s4_fclk_div3.hw },
	{ .hw = &s4_fclk_div4.hw },
	{ .hw = &s4_fclk_div5.hw },
	{ .hw = &s4_fclk_div7.hw }
};

static struct clk_regmap s4_mali_0_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_MALI_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
		.table = mux_table_mali,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mali_0_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = s4_mali_0_1_parent_data,
		.num_parents = ARRAY_SIZE(s4_mali_0_1_parent_data),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap s4_mali_0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_MALI_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mali_0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_mali_0_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s4_mali_0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_MALI_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mali_0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_mali_0_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_GATE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s4_mali_1_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_MALI_CLK_CTRL,
		.mask = 0x7,
		.shift = 25,
		.table = mux_table_mali,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mali_1_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = s4_mali_0_1_parent_data,
		.num_parents = ARRAY_SIZE(s4_mali_0_1_parent_data),
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s4_mali_1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_MALI_CLK_CTRL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mali_1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_mali_1_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s4_mali_1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_MALI_CLK_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mali_1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_mali_1_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_GATE | CLK_SET_RATE_PARENT,
	},
};

static const struct clk_hw *s4_mali_parent_hws[] = {
	&s4_mali_0.hw,
	&s4_mali_1.hw
};

static struct clk_regmap s4_mali_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_MALI_CLK_CTRL,
		.mask = 1,
		.shift = 31,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mali",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = s4_mali_parent_hws,
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/* cts_vdec_clk */
static const struct clk_parent_data s4_dec_parent_hws[] = {
	{ .hw = &s4_fclk_div2p5.hw },
	{ .hw = &s4_fclk_div3.hw },
	{ .hw = &s4_fclk_div4.hw },
	{ .hw = &s4_fclk_div5.hw },
	{ .hw = &s4_fclk_div7.hw },
	{ .hw = &s4_hifi_pll.hw },
	{ .hw = &s4_gp0_pll.hw },
	{ .fw_name = "xtal", }
};

static struct clk_regmap s4_vdec_p0_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VDEC_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vdec_p0_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = s4_dec_parent_hws,
		.num_parents = ARRAY_SIZE(s4_dec_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap s4_vdec_p0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_VDEC_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vdec_p0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_vdec_p0_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s4_vdec_p0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VDEC_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vdec_p0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_vdec_p0_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s4_vdec_p1_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VDEC3_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vdec_p1_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = s4_dec_parent_hws,
		.num_parents = ARRAY_SIZE(s4_dec_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap s4_vdec_p1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_VDEC3_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vdec_p1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_vdec_p1_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s4_vdec_p1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VDEC3_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vdec_p1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_vdec_p1_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static const struct clk_hw *s4_vdec_mux_parent_hws[] = {
	&s4_vdec_p0.hw,
	&s4_vdec_p1.hw
};

static struct clk_regmap s4_vdec_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VDEC3_CLK_CTRL,
		.mask = 0x1,
		.shift = 15,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vdec_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = s4_vdec_mux_parent_hws,
		.num_parents = ARRAY_SIZE(s4_vdec_mux_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

/* cts_hevcf_clk */
static struct clk_regmap s4_hevcf_p0_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VDEC2_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hevcf_p0_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = s4_dec_parent_hws,
		.num_parents = ARRAY_SIZE(s4_dec_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap s4_hevcf_p0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_VDEC2_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hevcf_p0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_hevcf_p0_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s4_hevcf_p0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VDEC2_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hevcf_p0_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_hevcf_p0_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s4_hevcf_p1_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VDEC4_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hevcf_p1_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = s4_dec_parent_hws,
		.num_parents = ARRAY_SIZE(s4_dec_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap s4_hevcf_p1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_VDEC4_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hevcf_p1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_hevcf_p1_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s4_hevcf_p1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VDEC4_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hevcf_p1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_hevcf_p1_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static const struct clk_hw *s4_hevcf_mux_parent_hws[] = {
	&s4_hevcf_p0.hw,
	&s4_hevcf_p1.hw
};

static struct clk_regmap s4_hevcf_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VDEC4_CLK_CTRL,
		.mask = 0x1,
		.shift = 15,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hevcf_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = s4_hevcf_mux_parent_hws,
		.num_parents = ARRAY_SIZE(s4_hevcf_mux_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

/* cts_vpu_clk */
static const struct clk_hw *s4_vpu_parent_hws[] = {
	&s4_fclk_div3.hw,
	&s4_fclk_div4.hw,
	&s4_fclk_div5.hw,
	&s4_fclk_div7.hw,
	&s4_mpll1.hw,
	&s4_vid_pll.hw,
	&s4_hifi_pll.hw,
	&s4_gp0_pll.hw
};

static struct clk_regmap s4_vpu_0_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VPU_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vpu_0_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = s4_vpu_parent_hws,
		.num_parents = ARRAY_SIZE(s4_vpu_parent_hws),
	},
};

static struct clk_regmap s4_vpu_0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_VPU_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vpu_0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) { &s4_vpu_0_sel.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s4_vpu_0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VPU_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &s4_vpu_0_div.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap s4_vpu_1_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VPU_CLK_CTRL,
		.mask = 0x7,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vpu_1_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = s4_vpu_parent_hws,
		.num_parents = ARRAY_SIZE(s4_vpu_parent_hws),
	},
};

static struct clk_regmap s4_vpu_1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_VPU_CLK_CTRL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vpu_1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) { &s4_vpu_1_sel.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s4_vpu_1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VPU_CLK_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &s4_vpu_1_div.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap s4_vpu = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VPU_CLK_CTRL,
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
			&s4_vpu_0.hw,
			&s4_vpu_1.hw,
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_NO_REPARENT,
	},
};

/*cts_vpu_clkb*/
static const struct clk_parent_data vpu_clkb_tmp_parent_hws[] = {
	{ .hw = &s4_vpu.hw },
	{ .hw = &s4_fclk_div4.hw },
	{ .hw = &s4_fclk_div5.hw },
	{ .hw = &s4_fclk_div7.hw }
};

static struct clk_regmap s4_vpu_clkb_tmp_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VPU_CLKB_CTRL,
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

static struct clk_regmap s4_vpu_clkb_tmp_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_VPU_CLKB_CTRL,
		.shift = 16,
		.width = 4,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_clkb_tmp_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_vpu_clkb_tmp_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s4_vpu_clkb_tmp = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VPU_CLKB_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vpu_clkb_tmp",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_vpu_clkb_tmp_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s4_vpu_clkb_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_VPU_CLKB_CTRL,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_clkb_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_vpu_clkb_tmp.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s4_vpu_clkb = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VPU_CLKB_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vpu_clkb",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_vpu_clkb_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

/* cts_vpu_clkc */
static const struct clk_hw *vpu_clkc_parent_hws[] = {
	&s4_fclk_div4.hw,
	&s4_fclk_div3.hw,
	&s4_fclk_div5.hw,
	&s4_fclk_div7.hw,
	&s4_mpll1.hw,
	&s4_vid_pll.hw,
	&s4_mpll2.hw,
	&s4_gp0_pll.hw
};

static struct clk_regmap s4_vpu_clkc_p0_mux  = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VPU_CLKC_CTRL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_clkc_p0_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = vpu_clkc_parent_hws,
		.num_parents = ARRAY_SIZE(vpu_clkc_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap s4_vpu_clkc_p0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_VPU_CLKC_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_clkc_p0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_vpu_clkc_p0_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s4_vpu_clkc_p0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VPU_CLKC_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vpu_clkc_p0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_vpu_clkc_p0_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s4_vpu_clkc_p1_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VPU_CLKC_CTRL,
		.mask = 0x7,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_clkc_p1_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = vpu_clkc_parent_hws,
		.num_parents = ARRAY_SIZE(vpu_clkc_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap s4_vpu_clkc_p1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_VPU_CLKC_CTRL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_clkc_p1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_vpu_clkc_p1_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s4_vpu_clkc_p1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VPU_CLKC_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vpu_clkc_p1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_vpu_clkc_p1_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static const struct clk_hw *s4_vpu_mux_parent_hws[] = {
	&s4_vpu_clkc_p0.hw,
	&s4_vpu_clkc_p1.hw
};

static struct clk_regmap s4_vpu_clkc_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VPU_CLKC_CTRL,
		.mask = 0x1,
		.shift = 31,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_clkc_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = s4_vpu_mux_parent_hws,
		.num_parents = ARRAY_SIZE(s4_vpu_mux_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

/*cts_vapb_clk*/
static const struct clk_hw *s4_vapb_parent_hws[] = {
	&s4_fclk_div4.hw,
	&s4_fclk_div3.hw,
	&s4_fclk_div5.hw,
	&s4_fclk_div7.hw,
	&s4_mpll1.hw,
	&s4_vid_pll.hw,
	&s4_mpll2.hw,
	&s4_fclk_div2p5.hw
};

static struct clk_regmap s4_vapb_0_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VAPBCLK_CTRL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vapb_0_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = s4_vapb_parent_hws,
		.num_parents = ARRAY_SIZE(s4_vapb_parent_hws),
	},
};

static struct clk_regmap s4_vapb_0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_VAPBCLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vapb_0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_vapb_0_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s4_vapb_0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VAPBCLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vapb_0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_vapb_0_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap s4_vapb_1_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VAPBCLK_CTRL,
		.mask = 0x7,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vapb_1_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = s4_vapb_parent_hws,
		.num_parents = ARRAY_SIZE(s4_vapb_parent_hws),
		.flags = CLK_SET_RATE_NO_REPARENT,
	},
};

static struct clk_regmap s4_vapb_1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_VAPBCLK_CTRL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vapb_1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_vapb_1_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s4_vapb_1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VAPBCLK_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vapb_1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_vapb_1_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap s4_vapb = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VAPBCLK_CTRL,
		.mask = 1,
		.shift = 31,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vapb_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_vapb_0.hw,
			&s4_vapb_1.hw
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_NO_REPARENT | CLK_SET_RATE_PARENT,
	},
};

/*cts_ge2d_clk*/
static struct clk_regmap s4_ge2d_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VAPBCLK_CTRL,
		.bit_idx = 30,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "ge2d_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &s4_vapb.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/*cts_hdcp22_esmclk*/
static const struct clk_hw *s4_esmclk_parent_hws[] = {
	&s4_fclk_div7.hw,
	&s4_fclk_div4.hw,
	&s4_fclk_div3.hw,
	&s4_fclk_div5.hw
};

static struct clk_regmap s4_hdcp22_esmclk_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_HDCP22_CTRL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdcp22_esmclk_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = s4_esmclk_parent_hws,
		.num_parents = ARRAY_SIZE(s4_esmclk_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap s4_hdcp22_esmclk_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_HDCP22_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdcp22_esmclk_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_hdcp22_esmclk_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s4_hdcp22_esmclk_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_HDCP22_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdcp22_esmclk_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_hdcp22_esmclk_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

/*cts_hdcp22_skpclk*/
static const struct clk_parent_data s4_skpclk_parent_hws[] = {
	{ .fw_name = "xtal", },
	{ .hw = &s4_fclk_div4.hw },
	{ .hw = &s4_fclk_div3.hw },
	{ .hw = &s4_fclk_div5.hw }
};

static struct clk_regmap s4_hdcp22_skpclk_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_HDCP22_CTRL,
		.mask = 0x3,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdcp22_skpclk_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = s4_skpclk_parent_hws,
		.num_parents = ARRAY_SIZE(s4_skpclk_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap s4_hdcp22_skpclk_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_HDCP22_CTRL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdcp22_skpclk_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_hdcp22_skpclk_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s4_hdcp22_skpclk_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_HDCP22_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdcp22_skpclk_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_hdcp22_skpclk_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

/* cts_vdin_meas_clk */
static const struct clk_parent_data s4_vdin_parent_hws[]  = {
	{ .fw_name = "xtal", },
	{ .hw = &s4_fclk_div4.hw },
	{ .hw = &s4_fclk_div3.hw },
	{ .hw = &s4_fclk_div5.hw },
	{ .hw = &s4_vid_pll.hw }
};

static struct clk_regmap s4_vdin_meas_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VDIN_MEAS_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vdin_meas_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = s4_vdin_parent_hws,
		.num_parents = ARRAY_SIZE(s4_vdin_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap s4_vdin_meas_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_VDIN_MEAS_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vdin_meas_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_vdin_meas_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s4_vdin_meas_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VDIN_MEAS_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vdin_meas_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_vdin_meas_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

/* s4 cts_nand_clk*/
static const struct clk_parent_data s4_sd_emmc_clk0_parent_data[] = {
	{ .fw_name = "xtal", },
	{ .hw = &s4_fclk_div2.hw },
	{ .hw = &s4_fclk_div3.hw },
	{ .hw = &s4_hifi_pll.hw },
	{ .hw = &s4_fclk_div2p5.hw },
	{ .hw = &s4_mpll2.hw },
	{ .hw = &s4_mpll3.hw },
	{ .hw = &s4_gp0_pll.hw }
};

static struct clk_regmap s4_sd_emmc_c_clk0_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_NAND_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_c_clk0_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = s4_sd_emmc_clk0_parent_data,
		.num_parents = ARRAY_SIZE(s4_sd_emmc_clk0_parent_data),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap s4_sd_emmc_c_clk0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_NAND_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_c_clk0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_sd_emmc_c_clk0_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap s4_sd_emmc_c_clk0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_NAND_CLK_CTRL,
		.bit_idx = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sd_emmc_c_clk0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_sd_emmc_c_clk0_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

/*cts_sd_emmc_a/b_clk*/
static struct clk_regmap s4_sd_emmc_a_clk0_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_SD_EMMC_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_a_clk0_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = s4_sd_emmc_clk0_parent_data,
		.num_parents = ARRAY_SIZE(s4_sd_emmc_clk0_parent_data),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap s4_sd_emmc_a_clk0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_SD_EMMC_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_a_clk0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_sd_emmc_a_clk0_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap s4_sd_emmc_a_clk0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_SD_EMMC_CLK_CTRL,
		.bit_idx = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sd_emmc_a_clk0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_sd_emmc_a_clk0_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap s4_sd_emmc_b_clk0_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_SD_EMMC_CLK_CTRL,
		.mask = 0x7,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_b_clk0_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = s4_sd_emmc_clk0_parent_data,
		.num_parents = ARRAY_SIZE(s4_sd_emmc_clk0_parent_data),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap s4_sd_emmc_b_clk0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_SD_EMMC_CLK_CTRL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_b_clk0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_sd_emmc_b_clk0_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap s4_sd_emmc_b_clk0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_SD_EMMC_CLK_CTRL,
		.bit_idx = 23,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sd_emmc_b_clk0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_sd_emmc_b_clk0_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

/*
 * s4d cts_nand_clk
 */
static const struct clk_parent_data s4d_sd_emmc_clk0_parent_data[]  = {
	{ .fw_name = "xtal", },
	{ .hw = &s4_fclk_div2.hw },
	{ .hw = &s4_fclk_div3.hw },
	{ .hw = &s4d_gp1_pll.hw },
	{ .hw = &s4_fclk_div2p5.hw },
	{ .hw = &s4_mpll2.hw },
	{ .hw = &s4_mpll3.hw },
	{ .hw = &s4_gp0_pll.hw }
};

static struct clk_regmap s4d_sd_emmc_c_clk0_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_NAND_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_c_clk0_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = s4d_sd_emmc_clk0_parent_data,
		.num_parents = ARRAY_SIZE(s4d_sd_emmc_clk0_parent_data),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap s4d_sd_emmc_c_clk0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_NAND_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_c_clk0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4d_sd_emmc_c_clk0_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap s4d_sd_emmc_c_clk0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_NAND_CLK_CTRL,
		.bit_idx = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sd_emmc_c_clk0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4d_sd_emmc_c_clk0_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

/*cts_sd_emmc_a/b_clk*/
static struct clk_regmap s4d_sd_emmc_a_clk0_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_SD_EMMC_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_a_clk0_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = s4d_sd_emmc_clk0_parent_data,
		.num_parents = ARRAY_SIZE(s4d_sd_emmc_clk0_parent_data),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap s4d_sd_emmc_a_clk0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_SD_EMMC_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_a_clk0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4d_sd_emmc_a_clk0_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap s4d_sd_emmc_a_clk0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_SD_EMMC_CLK_CTRL,
		.bit_idx = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sd_emmc_a_clk0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4d_sd_emmc_a_clk0_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap s4d_sd_emmc_b_clk0_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_SD_EMMC_CLK_CTRL,
		.mask = 0x7,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_b_clk0_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = s4d_sd_emmc_clk0_parent_data,
		.num_parents = ARRAY_SIZE(s4d_sd_emmc_clk0_parent_data),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap s4d_sd_emmc_b_clk0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_SD_EMMC_CLK_CTRL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_b_clk0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4d_sd_emmc_b_clk0_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap s4d_sd_emmc_b_clk0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_SD_EMMC_CLK_CTRL,
		.bit_idx = 23,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sd_emmc_b_clk0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4d_sd_emmc_b_clk0_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

/*cts_cdac_clk*/

/*cts_spicc_0/1_clk*/
static const struct clk_parent_data s4_spicc_parent_hws[] = {
	{ .fw_name = "xtal", },
	{ .hw = &s4_sys_clk.hw },
	{ .hw = &s4_fclk_div4.hw },
	{ .hw = &s4_fclk_div3.hw },
	{ .hw = &s4_fclk_div2.hw },
	{ .hw = &s4_fclk_div5.hw },
	{ .hw = &s4_fclk_div7.hw },
	{ .hw = &s4_gp0_pll.hw }
};

static struct clk_regmap s4_spicc0_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_SPICC_CLK_CTRL,
		.mask = 0x7,
		.shift = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spicc0_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = s4_spicc_parent_hws,
		.num_parents = ARRAY_SIZE(s4_spicc_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap s4_spicc0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_SPICC_CLK_CTRL,
		.shift = 0,
		.width = 6,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spicc0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_spicc0_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s4_spicc0_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_SPICC_CLK_CTRL,
		.bit_idx = 6,
	},
	.hw.init = &(struct clk_init_data){
		.name = "spicc0_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_spicc0_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

/*cts_pwm_*_clk*/
static const struct clk_parent_data s4_pwm_parent_data[] = {
	{ .fw_name = "xtal", },
	{ .hw = &s4_vid_pll.hw },
	{ .hw = &s4_fclk_div4.hw },
	{ .hw = &s4_fclk_div3.hw }
};

static struct clk_regmap s4_pwm_a_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_AB_CTRL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_a_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = s4_pwm_parent_data,
		.num_parents = ARRAY_SIZE(s4_pwm_parent_data),
		.flags = CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap s4_pwm_a_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_AB_CTRL,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_a_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_pwm_a_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap s4_pwm_a_gate = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_AB_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_a_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_pwm_a_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap s4_pwm_b_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_AB_CTRL,
		.mask = 0x3,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_b_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = s4_pwm_parent_data,
		.num_parents = ARRAY_SIZE(s4_pwm_parent_data),
		.flags = CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap s4_pwm_b_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_AB_CTRL,
		.shift = 16,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_b_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_pwm_b_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap s4_pwm_b_gate = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_AB_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_b_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_pwm_b_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap s4_pwm_c_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_CD_CTRL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_c_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = s4_pwm_parent_data,
		.num_parents = ARRAY_SIZE(s4_pwm_parent_data),
		.flags = CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap s4_pwm_c_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_CD_CTRL,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_c_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_pwm_c_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap s4_pwm_c_gate = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_CD_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_c_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_pwm_c_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap s4_pwm_d_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_CD_CTRL,
		.mask = 0x3,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_d_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = s4_pwm_parent_data,
		.num_parents = ARRAY_SIZE(s4_pwm_parent_data),
		.flags = CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap s4_pwm_d_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_CD_CTRL,
		.shift = 16,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_d_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_pwm_d_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap s4_pwm_d_gate = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_CD_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_d_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_pwm_d_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap s4_pwm_e_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_EF_CTRL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_e_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = s4_pwm_parent_data,
		.num_parents = ARRAY_SIZE(s4_pwm_parent_data),
		.flags = CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap s4_pwm_e_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_EF_CTRL,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_e_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_pwm_e_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap s4_pwm_e_gate = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_EF_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_e_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_pwm_e_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap s4_pwm_f_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_EF_CTRL,
		.mask = 0x3,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_f_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = s4_pwm_parent_data,
		.num_parents = ARRAY_SIZE(s4_pwm_parent_data),
		.flags = CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap s4_pwm_f_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_EF_CTRL,
		.shift = 16,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_f_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_pwm_f_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap s4_pwm_f_gate = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_EF_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_f_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_pwm_f_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap s4_pwm_g_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_GH_CTRL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_g_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = s4_pwm_parent_data,
		.num_parents = ARRAY_SIZE(s4_pwm_parent_data),
		.flags = CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap s4_pwm_g_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_GH_CTRL,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_g_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_pwm_g_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap s4_pwm_g_gate = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_GH_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_g_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_pwm_g_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap s4_pwm_h_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_GH_CTRL,
		.mask = 0x3,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_h_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = s4_pwm_parent_data,
		.num_parents = ARRAY_SIZE(s4_pwm_parent_data),
		.flags = CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap s4_pwm_h_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_GH_CTRL,
		.shift = 16,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_h_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_pwm_h_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap s4_pwm_h_gate = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_GH_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_h_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_pwm_h_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap s4_pwm_i_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_IJ_CTRL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_i_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = s4_pwm_parent_data,
		.num_parents = ARRAY_SIZE(s4_pwm_parent_data),
		.flags = CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap s4_pwm_i_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_IJ_CTRL,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_i_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_pwm_i_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap s4_pwm_i_gate = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_IJ_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_i_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_pwm_i_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap s4_pwm_j_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_IJ_CTRL,
		.mask = 0x3,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_j_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = s4_pwm_parent_data,
		.num_parents = ARRAY_SIZE(s4_pwm_parent_data),
		.flags = CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap s4_pwm_j_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_IJ_CTRL,
		.shift = 16,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_j_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_pwm_h_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap s4_pwm_j_gate = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_IJ_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_j_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_pwm_j_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

/*cts_sar_adc_clk*/
static struct clk_regmap s4_saradc_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_SAR_CLK_CTRL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "saradc_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = (const struct clk_parent_data []) {
			{ .fw_name = "xtal", },
			{ .hw = &s4_sys_clk.hw },
		},
		.num_parents = 2,
	},
};

static struct clk_regmap s4_saradc_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_SAR_CLK_CTRL,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "saradc_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_saradc_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s4_saradc_gate = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_SAR_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "saradc_clk",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_saradc_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

/* gen clk */
static u32 s4_gen_clk_mux_table[] = { 0, 5, 7, 19, 21, 22,
				23, 24, 25, 26, 27, 28 };

static const struct clk_parent_data s4_gen_clk_parent_data[] = {
	{ .fw_name = "xtal", },
	{ .hw = &s4_gp0_pll.hw },
	{ .hw = &s4_hifi_pll.hw },
	{ .hw = &s4_fclk_div2.hw },
	{ .hw = &s4_fclk_div3.hw },
	{ .hw = &s4_fclk_div4.hw },
	{ .hw = &s4_fclk_div5.hw },
	{ .hw = &s4_fclk_div7.hw },
	{ .hw = &s4_mpll0.hw },
	{ .hw = &s4_mpll1.hw },
	{ .hw = &s4_mpll2.hw },
	{ .hw = &s4_mpll3.hw }
};

static struct clk_regmap s4_gen_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_GEN_CLK_CTRL,
		.mask = 0x1f,
		.shift = 12,
		.table = s4_gen_clk_mux_table,
	},
	.hw.init = &(struct clk_init_data){
		.name = "gen_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = s4_gen_clk_parent_data,
		.num_parents = ARRAY_SIZE(s4_gen_clk_parent_data),
	},
};

static struct clk_regmap s4_gen_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_GEN_CLK_CTRL,
		.shift = 0,
		.width = 11,
	},
	.hw.init = &(struct clk_init_data){
		.name = "gen_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_gen_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s4_gen = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_GEN_CLK_CTRL,
		.bit_idx = 11,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "gen",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_gen_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/*demod clk*/
static const struct clk_parent_data adc_extclk_in_parent_data[]  = {
	{ .fw_name = "xtal", },
	{ .hw = &s4_fclk_div4.hw },
	{ .hw = &s4_fclk_div3.hw },
	{ .hw = &s4_fclk_div5.hw },
	{ .hw = &s4_fclk_div7.hw },
	{ .hw = &s4_mpll2.hw },
	{ .hw = &s4_gp0_pll.hw },
	{ .hw = &s4_hifi_pll.hw }
};

static struct clk_regmap s4_adc_extclk_in_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_DEMOD_CLK_CTRL,
		.mask = 0x7,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data){
		.name = "adc_extclk_in_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = adc_extclk_in_parent_data,
		.num_parents = ARRAY_SIZE(adc_extclk_in_parent_data),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap s4_adc_extclk_in_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_DEMOD_CLK_CTRL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "adc_extclk_in_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_adc_extclk_in_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap s4_adc_extclk_in_gate = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_DEMOD_CLK_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "adc_extclk_in",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_adc_extclk_in_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap s4_demod_core_clk_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_DEMOD_CLK_CTRL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "demod_core_clk_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = (const struct clk_parent_data []) {
			{ .fw_name = "xtal", },
			{ .hw = &s4_fclk_div7.hw },
			{ .hw = &s4_fclk_div4.hw },
			{ .hw = &s4_adc_extclk_in_gate.hw }
		},
		.num_parents = 4,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap s4_demod_core_clk_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_DEMOD_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "demod_core_clk_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_demod_core_clk_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap s4_demod_core_clk_gate = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_DEMOD_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "demod_core_clk",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_demod_core_clk_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap s4_demod_core_t2_clk_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_DEMOD_CLK_CTRL1,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "demod_core_t2_clk_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = (const struct clk_parent_data []) {
			{ .fw_name = "xtal", },
			{ .hw = &s4_fclk_div5.hw },
			{ .hw = &s4_fclk_div4.hw },
			{ .hw = &s4_adc_extclk_in_gate.hw }
		},
		.num_parents = 4,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap s4_demod_core_t2_clk_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_DEMOD_CLK_CTRL1,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "demod_core_t2_clk_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_demod_core_t2_clk_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap s4_demod_core_t2_clk_gate = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_DEMOD_CLK_CTRL1,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "demod_core_t2_clk",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4_demod_core_t2_clk_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
	},
};

/*
 *demod 32k clock
 */
static struct clk_regmap s4d_demod_32k_clkin = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_DEMOD_32K_CTRL0,
		.bit_idx = 31,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "demod_32k_clkin",
		.ops = &clk_regmap_gate_ops,
		.parent_data = (const struct clk_parent_data []) {
			{ .fw_name = "xtal", }
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static const struct meson_clk_dualdiv_param s4d_32k_div_table[] = {
	{
		.dual	= 1,
		.n1	= 733,
		.m1	= 8,
		.n2	= 732,
		.m2	= 11,
	},
	{}
};

static struct clk_regmap s4d_demod_32k_div = {
	.data = &(struct meson_clk_dualdiv_data){
		.n1 = {
			.reg_off = CLKCTRL_DEMOD_32K_CTRL0,
			.shift   = 0,
			.width   = 12,
		},
		.n2 = {
			.reg_off = CLKCTRL_DEMOD_32K_CTRL0,
			.shift   = 12,
			.width   = 12,
		},
		.m1 = {
			.reg_off = CLKCTRL_DEMOD_32K_CTRL1,
			.shift   = 0,
			.width   = 12,
		},
		.m2 = {
			.reg_off = CLKCTRL_DEMOD_32K_CTRL1,
			.shift   = 12,
			.width   = 12,
		},
		.dual = {
			.reg_off = CLKCTRL_DEMOD_32K_CTRL0,
			.shift   = 28,
			.width   = 1,
		},
		.table = s4d_32k_div_table,
	},
	.hw.init = &(struct clk_init_data){
		.name = "demod_32k_div",
		.ops = &meson_clk_dualdiv_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4d_demod_32k_clkin.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap s4d_demod_32k_xtal = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_DEMOD_32K_CTRL1,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "demod_32k_xtal",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4d_demod_32k_clkin.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap s4d_demod_32k_clk = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_RTC_BY_OSCIN_CTRL0,
		.bit_idx = 30,
	},
	.hw.init = &(struct clk_init_data){
		.name = "demod_32k_clk",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4d_demod_32k_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

#define MESON_SC2_SYS_GATE(_name, _reg, _bit)				\
struct clk_regmap _name = {						\
	.data = &(struct clk_regmap_gate_data) {			\
		.offset = (_reg),					\
		.bit_idx = (_bit),					\
	},								\
	.hw.init = &(struct clk_init_data) {				\
		.name = #_name,						\
		.ops = &clk_regmap_gate_ops,				\
		.parent_hws = (const struct clk_hw *[]) {		\
			&s4_sys_clk.hw					\
		},							\
		.num_parents = 1,					\
		.flags = CLK_IGNORE_UNUSED,				\
	},								\
}

/*CLKCTRL_SYS_CLK_EN0_REG0*/
static MESON_SC2_SYS_GATE(s4_ddr, CLKCTRL_SYS_CLK_EN0_REG0, 0);
static MESON_SC2_SYS_GATE(s4_dos, CLKCTRL_SYS_CLK_EN0_REG0, 1);
static MESON_SC2_SYS_GATE(s4_ethphy, CLKCTRL_SYS_CLK_EN0_REG0, 4);
static MESON_SC2_SYS_GATE(s4_mali, CLKCTRL_SYS_CLK_EN0_REG0, 6);
static MESON_SC2_SYS_GATE(s4_aocpu, CLKCTRL_SYS_CLK_EN0_REG0, 13);
static MESON_SC2_SYS_GATE(s4_aucpu, CLKCTRL_SYS_CLK_EN0_REG0, 14);
static MESON_SC2_SYS_GATE(s4_cec, CLKCTRL_SYS_CLK_EN0_REG0, 16);
static MESON_SC2_SYS_GATE(s4_sdemmca, CLKCTRL_SYS_CLK_EN0_REG0, 24);
static MESON_SC2_SYS_GATE(s4_sdemmcb, CLKCTRL_SYS_CLK_EN0_REG0, 25);
static MESON_SC2_SYS_GATE(s4_nand, CLKCTRL_SYS_CLK_EN0_REG0, 26);
static MESON_SC2_SYS_GATE(s4_smartcard, CLKCTRL_SYS_CLK_EN0_REG0, 27);
static MESON_SC2_SYS_GATE(s4_acodec, CLKCTRL_SYS_CLK_EN0_REG0, 28);
static MESON_SC2_SYS_GATE(s4_spifc, CLKCTRL_SYS_CLK_EN0_REG0, 29);
static MESON_SC2_SYS_GATE(s4_msr_clk, CLKCTRL_SYS_CLK_EN0_REG0, 30);
static MESON_SC2_SYS_GATE(s4_ir_ctrl, CLKCTRL_SYS_CLK_EN0_REG0, 31);

/*CLKCTRL_SYS_CLK_EN0_REG1*/
static MESON_SC2_SYS_GATE(s4_audio, CLKCTRL_SYS_CLK_EN0_REG1, 0);
static MESON_SC2_SYS_GATE(s4_eth, CLKCTRL_SYS_CLK_EN0_REG1, 3);
static MESON_SC2_SYS_GATE(s4_uart_a, CLKCTRL_SYS_CLK_EN0_REG1, 5);
static MESON_SC2_SYS_GATE(s4_uart_b, CLKCTRL_SYS_CLK_EN0_REG1, 6);
static MESON_SC2_SYS_GATE(s4_uart_c, CLKCTRL_SYS_CLK_EN0_REG1, 7);
static MESON_SC2_SYS_GATE(s4_uart_d, CLKCTRL_SYS_CLK_EN0_REG1, 8);
static MESON_SC2_SYS_GATE(s4_uart_e, CLKCTRL_SYS_CLK_EN0_REG1, 9);
static MESON_SC2_SYS_GATE(s4_aififo, CLKCTRL_SYS_CLK_EN0_REG1, 11);
static MESON_SC2_SYS_GATE(s4_ts_ddr, CLKCTRL_SYS_CLK_EN0_REG1, 15);
static MESON_SC2_SYS_GATE(s4_ts_pll, CLKCTRL_SYS_CLK_EN0_REG1, 16);
static MESON_SC2_SYS_GATE(s4_g2d, CLKCTRL_SYS_CLK_EN0_REG1, 20);
static MESON_SC2_SYS_GATE(s4_spicc0, CLKCTRL_SYS_CLK_EN0_REG1, 21);
static MESON_SC2_SYS_GATE(s4_usb, CLKCTRL_SYS_CLK_EN0_REG1, 26);
static MESON_SC2_SYS_GATE(s4_i2c_m_a, CLKCTRL_SYS_CLK_EN0_REG1, 30);
static MESON_SC2_SYS_GATE(s4_i2c_m_b, CLKCTRL_SYS_CLK_EN0_REG1, 31);

/*CLKCTRL_SYS_CLK_EN0_REG2*/
static MESON_SC2_SYS_GATE(s4_i2c_m_c, CLKCTRL_SYS_CLK_EN0_REG2, 0);
static MESON_SC2_SYS_GATE(s4_i2c_m_d, CLKCTRL_SYS_CLK_EN0_REG2, 1);
static MESON_SC2_SYS_GATE(s4_i2c_m_e, CLKCTRL_SYS_CLK_EN0_REG2, 2);
static MESON_SC2_SYS_GATE(s4_hdmitx_apb, CLKCTRL_SYS_CLK_EN0_REG2, 4);
static MESON_SC2_SYS_GATE(s4_i2c_s_a, CLKCTRL_SYS_CLK_EN0_REG2, 5);
static MESON_SC2_SYS_GATE(s4_usb1_to_ddr, CLKCTRL_SYS_CLK_EN0_REG2, 8);
static MESON_SC2_SYS_GATE(s4_hdcp22, CLKCTRL_SYS_CLK_EN0_REG2, 10);
static MESON_SC2_SYS_GATE(s4_mmc_apb, CLKCTRL_SYS_CLK_EN0_REG2, 11);
static MESON_SC2_SYS_GATE(s4_rsa, CLKCTRL_SYS_CLK_EN0_REG2, 18);
static MESON_SC2_SYS_GATE(s4_cpu_debug, CLKCTRL_SYS_CLK_EN0_REG2, 19);
static MESON_SC2_SYS_GATE(s4_vpu_intr, CLKCTRL_SYS_CLK_EN0_REG2, 25);
static MESON_SC2_SYS_GATE(s4_demod, CLKCTRL_SYS_CLK_EN0_REG2, 27);
static MESON_SC2_SYS_GATE(s4_sar_adc, CLKCTRL_SYS_CLK_EN0_REG2, 28);
static MESON_SC2_SYS_GATE(s4_gic, CLKCTRL_SYS_CLK_EN0_REG2, 30);

/*CLKCTRL_SYS_CLK_EN0_REG3*/
static MESON_SC2_SYS_GATE(s4_pwm_ab, CLKCTRL_SYS_CLK_EN0_REG3, 7);
static MESON_SC2_SYS_GATE(s4_pwm_cd, CLKCTRL_SYS_CLK_EN0_REG3, 8);
static MESON_SC2_SYS_GATE(s4_pwm_ef, CLKCTRL_SYS_CLK_EN0_REG3, 9);
static MESON_SC2_SYS_GATE(s4_pwm_gh, CLKCTRL_SYS_CLK_EN0_REG3, 10);
static MESON_SC2_SYS_GATE(s4_pwm_ij, CLKCTRL_SYS_CLK_EN0_REG3, 11);

/* Array of all clocks provided by this provider */
static struct clk_hw_onecell_data s4_hw_onecell_data = {
	.hws = {
		[CLKID_FIXED_PLL_DCO]		= &s4_fixed_pll_dco.hw,
		[CLKID_FIXED_PLL]		= &s4_fixed_pll.hw,
		[CLKID_SYS_PLL_DCO]		= &s4_sys_pll_dco.hw,
		[CLKID_SYS_PLL]			= &s4_sys_pll.hw,
		[CLKID_FCLK_DIV2_DIV]		= &s4_fclk_div2_div.hw,
		[CLKID_FCLK_DIV2]		= &s4_fclk_div2.hw,
		[CLKID_FCLK_DIV3_DIV]		= &s4_fclk_div3_div.hw,
		[CLKID_FCLK_DIV3]		= &s4_fclk_div3.hw,
		[CLKID_FCLK_DIV4_DIV]		= &s4_fclk_div4_div.hw,
		[CLKID_FCLK_DIV4]		= &s4_fclk_div4.hw,
		[CLKID_FCLK_DIV5_DIV]		= &s4_fclk_div5_div.hw,
		[CLKID_FCLK_DIV5]		= &s4_fclk_div5.hw,
		[CLKID_FCLK_DIV7_DIV]		= &s4_fclk_div7_div.hw,
		[CLKID_FCLK_DIV7]		= &s4_fclk_div7.hw,
		[CLKID_FCLK_DIV2P5_DIV]		= &s4_fclk_div2p5_div.hw,
		[CLKID_FCLK_DIV2P5]		= &s4_fclk_div2p5.hw,

		[CLKID_GP0_PLL_DCO]		= &s4_gp0_pll_dco.hw,
		[CLKID_GP0_PLL]			= &s4_gp0_pll.hw,

		[CLKID_CPU_CLK_DYN]		= &s4_cpu_dyn_clk.hw,
		[CLKID_CPU_CLK]			= &s4_cpu_clk.hw,

		[CLKID_HIFI_PLL_DCO]		= &s4_hifi_pll_dco.hw,
		[CLKID_HIFI_PLL]		= &s4_hifi_pll.hw,
		[CLKID_HDMI_PLL_DCO]		= &s4_hdmi_pll_dco.hw,
		[CLKID_HDMI_PLL_OD]		= &s4_hdmi_pll_od.hw,
		[CLKID_HDMI_PLL]		= &s4_hdmi_pll.hw,
		[CLKID_MPLL_50M_DIV]		= &s4_mpll_50m_div.hw,
		[CLKID_MPLL_50M]		= &s4_mpll_50m.hw,
		[CLKID_MPLL_PREDIV]		= &s4_mpll_prediv.hw,
		[CLKID_MPLL0_DIV]		= &s4_mpll0_div.hw,
		[CLKID_MPLL0]			= &s4_mpll0.hw,
		[CLKID_MPLL1_DIV]		= &s4_mpll1_div.hw,
		[CLKID_MPLL1]			= &s4_mpll1.hw,
		[CLKID_MPLL2_DIV]		= &s4_mpll2_div.hw,
		[CLKID_MPLL2]			= &s4_mpll2.hw,
		[CLKID_MPLL3_DIV]		= &s4_mpll3_div.hw,
		[CLKID_MPLL3]			= &s4_mpll3.hw,

		[CLKID_RTC_32K_CLKIN]		= &s4_rtc_32k_clkin.hw,
		[CLKID_RTC_32K_DIV]		= &s4_rtc_32k_div.hw,
		[CLKID_RTC_32K_XATL]		= &s4_rtc_32k_xtal.hw,
		[CLKID_RTC_32K_MUX]		= &s4_rtc_32k_sel.hw,
		[CLKID_RTC_CLK]			= &s4_rtc_clk.hw,

		[CLKID_SYS_CLK_B_MUX]		= &s4_sysclk_b_sel.hw,
		[CLKID_SYS_CLK_B_DIV]		= &s4_sysclk_b_div.hw,
		[CLKID_SYS_CLK_B_GATE]		= &s4_sysclk_b.hw,
		[CLKID_SYS_CLK_A_MUX]		= &s4_sysclk_a_sel.hw,
		[CLKID_SYS_CLK_A_DIV]		= &s4_sysclk_a_div.hw,
		[CLKID_SYS_CLK_A_GATE]		= &s4_sysclk_a.hw,
		[CLKID_SYS_CLK]			= &s4_sys_clk.hw,

		[CLKID_CECA_32K_CLKIN]		= &s4_ceca_32k_clkin.hw,
		[CLKID_CECA_32K_DIV]		= &s4_ceca_32k_div.hw,
		[CLKID_CECA_32K_MUX_PRE]	= &s4_ceca_32k_sel_pre.hw,
		[CLKID_CECA_32K_MUX]		= &s4_ceca_32k_sel.hw,
		[CLKID_CECA_32K_CLKOUT]		= &s4_ceca_32k_clkout.hw,

		[CLKID_CECB_32K_CLKIN]		= &s4_cecb_32k_clkin.hw,
		[CLKID_CECB_32K_DIV]		= &s4_cecb_32k_div.hw,
		[CLKID_CECB_32K_MUX_PRE]	= &s4_cecb_32k_sel_pre.hw,
		[CLKID_CECB_32K_MUX]		= &s4_cecb_32k_sel.hw,
		[CLKID_CECB_32K_CLKOUT]		= &s4_cecb_32k_clkout.hw,

		[CLKID_SC_CLK_MUX]		= &s4_sc_clk_mux.hw,
		[CLKID_SC_CLK_DIV]		= &s4_sc_clk_div.hw,
		[CLKID_SC_CLK_GATE]		= &s4_sc_clk_gate.hw,

		[CLKID_24M_CLK_GATE]		= &s4_24M_clk_gate.hw,
		[CLKID_12M_CLK_DIV]		= &s4_12M_clk_div.hw,
		[CLKID_12M_CLK_GATE]		= &s4_12M_clk_gate.hw,

		[CLKID_VID_PLL]			= &s4_vid_pll_div.hw,
		[CLKID_VID_PLL_MUX]		= &s4_vid_pll_sel.hw,
		[CLKID_VID_PLL]			= &s4_vid_pll.hw,
		[CLKID_VCLK_MUX]		= &s4_vclk_sel.hw,
		[CLKID_VCLK2_MUX]		= &s4_vclk2_sel.hw,
		[CLKID_VCLK_INPUT]		= &s4_vclk_input.hw,
		[CLKID_VCLK2_INPUT]		= &s4_vclk2_input.hw,
		[CLKID_VCLK_DIV]		= &s4_vclk_div.hw,
		[CLKID_VCLK2_DIV]		= &s4_vclk2_div.hw,
		[CLKID_VCLK]			= &s4_vclk.hw,
		[CLKID_VCLK2]			= &s4_vclk2.hw,
		[CLKID_VCLK_DIV1]		= &s4_vclk_div1.hw,
		[CLKID_VCLK_DIV2_EN]		= &s4_vclk_div2_en.hw,
		[CLKID_VCLK_DIV4_EN]		= &s4_vclk_div4_en.hw,
		[CLKID_VCLK_DIV6_EN]		= &s4_vclk_div6_en.hw,
		[CLKID_VCLK_DIV12_EN]		= &s4_vclk_div12_en.hw,
		[CLKID_VCLK2_DIV1]		= &s4_vclk2_div1.hw,
		[CLKID_VCLK2_DIV2_EN]		= &s4_vclk2_div2_en.hw,
		[CLKID_VCLK2_DIV4_EN]		= &s4_vclk2_div4_en.hw,
		[CLKID_VCLK2_DIV6_EN]		= &s4_vclk2_div6_en.hw,
		[CLKID_VCLK2_DIV12_EN]		= &s4_vclk2_div12_en.hw,
		[CLKID_VCLK_DIV2]		= &s4_vclk_div2.hw,
		[CLKID_VCLK_DIV4]		= &s4_vclk_div4.hw,
		[CLKID_VCLK_DIV6]		= &s4_vclk_div6.hw,
		[CLKID_VCLK_DIV12]		= &s4_vclk_div12.hw,
		[CLKID_VCLK2_DIV2]		= &s4_vclk2_div2.hw,
		[CLKID_VCLK2_DIV4]		= &s4_vclk2_div4.hw,
		[CLKID_VCLK2_DIV6]		= &s4_vclk2_div6.hw,
		[CLKID_VCLK2_DIV12]		= &s4_vclk2_div12.hw,
		[CLKID_CTS_ENCI_MUX]		= &s4_cts_enci_sel.hw,
		[CLKID_CTS_ENCP_MUX]		= &s4_cts_encp_sel.hw,
		[CLKID_CTS_VDAC_MUX]		= &s4_cts_vdac_sel.hw,
		[CLKID_HDMI_TX_MUX]		= &s4_hdmi_tx_sel.hw,
		[CLKID_CTS_ENCI]		= &s4_cts_enci.hw,
		[CLKID_CTS_ENCP]		= &s4_cts_encp.hw,
		[CLKID_CTS_VDAC]		= &s4_cts_vdac.hw,
		[CLKID_HDMI_TX]			= &s4_hdmi_tx.hw,
		[CLKID_HDMI_MUX]		= &s4_hdmi_sel.hw,
		[CLKID_HDMI_DIV]		= &s4_hdmi_div.hw,
		[CLKID_HDMI]			= &s4_hdmi.hw,

		[CLKID_TS_CLK_DIV]		= &s4_ts_clk_div.hw,
		[CLKID_TS_CLK_GATE]		= &s4_ts_clk_gate.hw,

		[CLKID_MALI_0_SEL]		= &s4_mali_0_sel.hw,
		[CLKID_MALI_0_DIV]		= &s4_mali_0_div.hw,
		[CLKID_MALI_0]			= &s4_mali_0.hw,
		[CLKID_MALI_1_SEL]		= &s4_mali_1_sel.hw,
		[CLKID_MALI_1_DIV]		= &s4_mali_1_div.hw,
		[CLKID_MALI_1]			= &s4_mali_1.hw,
		[CLKID_MALI]			= &s4_mali_mux.hw,

		[CLKID_VDEC_P0_MUX]		= &s4_vdec_p0_mux.hw,
		[CLKID_VDEC_P0_DIV]		= &s4_vdec_p0_div.hw,
		[CLKID_VDEC_P0]			= &s4_vdec_p0.hw,
		[CLKID_VDEC_P1_MUX]		= &s4_vdec_p1_mux.hw,
		[CLKID_VDEC_P1_DIV]		= &s4_vdec_p1_div.hw,
		[CLKID_VDEC_P1]			= &s4_vdec_p1.hw,
		[CLKID_VDEC_MUX]		= &s4_vdec_mux.hw,

		[CLKID_HEVCF_P0_MUX]		= &s4_hevcf_p0_mux.hw,
		[CLKID_HEVCF_P0_DIV]		= &s4_hevcf_p0_div.hw,
		[CLKID_HEVCF_P0]		= &s4_hevcf_p0.hw,
		[CLKID_HEVCF_P1_MUX]		= &s4_hevcf_p1_mux.hw,
		[CLKID_HEVCF_P1_DIV]		= &s4_hevcf_p1_div.hw,
		[CLKID_HEVCF_P1]		= &s4_hevcf_p1.hw,
		[CLKID_HEVCF_MUX]		= &s4_hevcf_mux.hw,

		[CLKID_VPU_0_MUX]		= &s4_vpu_0_sel.hw,
		[CLKID_VPU_0_DIV]		= &s4_vpu_0_div.hw,
		[CLKID_VPU_0]			= &s4_vpu_0.hw,
		[CLKID_VPU_1_MUX]		= &s4_vpu_1_sel.hw,
		[CLKID_VPU_1_DIV]		= &s4_vpu_1_div.hw,
		[CLKID_VPU_1]			= &s4_vpu_1.hw,
		[CLKID_VPU]			= &s4_vpu.hw,

		[CLKID_VPU_CLKB_TMP_MUX]	= &s4_vpu_clkb_tmp_mux.hw,
		[CLKID_VPU_CLKB_TMP_DIV]	= &s4_vpu_clkb_tmp_div.hw,
		[CLKID_VPU_CLKB_TMP]		= &s4_vpu_clkb_tmp.hw,
		[CLKID_VPU_CLKB_DIV]		= &s4_vpu_clkb_div.hw,
		[CLKID_VPU_CLKB]		= &s4_vpu_clkb.hw,
		[CLKID_VPU_CLKC_P0_MUX]		= &s4_vpu_clkc_p0_mux.hw,
		[CLKID_VPU_CLKC_P0_DIV]		= &s4_vpu_clkc_p0_div.hw,
		[CLKID_VPU_CLKC_P0]		= &s4_vpu_clkc_p0.hw,
		[CLKID_VPU_CLKC_P1_MUX]		= &s4_vpu_clkc_p1_mux.hw,
		[CLKID_VPU_CLKC_P1_DIV]		= &s4_vpu_clkc_p1_div.hw,
		[CLKID_VPU_CLKC_P1]		= &s4_vpu_clkc_p1.hw,
		[CLKID_VPU_CLKC_MUX]		= &s4_vpu_clkc_mux.hw,

		[CLKID_VAPB_0_MUX]		= &s4_vapb_0_sel.hw,
		[CLKID_VAPB_0_DIV]		= &s4_vapb_0_div.hw,
		[CLKID_VAPB_0]			= &s4_vapb_0.hw,
		[CLKID_VAPB_1_MUX]		= &s4_vapb_1_sel.hw,
		[CLKID_VAPB_1_DIV]		= &s4_vapb_1_div.hw,
		[CLKID_VAPB_1]			= &s4_vapb_1.hw,
		[CLKID_VAPB]			= &s4_vapb.hw,

		[CLKID_GE2D]			= &s4_ge2d_gate.hw,

		[CLKID_VDIN_MEAS_MUX]		= &s4_vdin_meas_mux.hw,
		[CLKID_VDIN_MEAS_DIV]		= &s4_vdin_meas_div.hw,
		[CLKID_VDIN_MEAS_GATE]		= &s4_vdin_meas_gate.hw,

		[CLKID_SD_EMMC_C_CLK_MUX]	= &s4_sd_emmc_c_clk0_sel.hw,
		[CLKID_SD_EMMC_C_CLK_DIV]	= &s4_sd_emmc_c_clk0_div.hw,
		[CLKID_SD_EMMC_C_CLK]		= &s4_sd_emmc_c_clk0.hw,

		[CLKID_SD_EMMC_A_CLK_MUX]	= &s4_sd_emmc_a_clk0_sel.hw,
		[CLKID_SD_EMMC_A_CLK_DIV]	= &s4_sd_emmc_a_clk0_div.hw,
		[CLKID_SD_EMMC_A_CLK]		= &s4_sd_emmc_a_clk0.hw,
		[CLKID_SD_EMMC_B_CLK_MUX]	= &s4_sd_emmc_b_clk0_sel.hw,
		[CLKID_SD_EMMC_B_CLK_DIV]	= &s4_sd_emmc_b_clk0_div.hw,
		[CLKID_SD_EMMC_B_CLK]		= &s4_sd_emmc_b_clk0.hw,

		[CLKID_SPICC0_MUX]		= &s4_spicc0_mux.hw,
		[CLKID_SPICC0_DIV]		= &s4_spicc0_div.hw,
		[CLKID_SPICC0_GATE]		= &s4_spicc0_gate.hw,

		[CLKID_PWM_A_MUX]		= &s4_pwm_a_mux.hw,
		[CLKID_PWM_A_DIV]		= &s4_pwm_a_div.hw,
		[CLKID_PWM_A_GATE]		= &s4_pwm_a_gate.hw,
		[CLKID_PWM_B_MUX]		= &s4_pwm_b_mux.hw,
		[CLKID_PWM_B_DIV]		= &s4_pwm_b_div.hw,
		[CLKID_PWM_B_GATE]		= &s4_pwm_b_gate.hw,
		[CLKID_PWM_C_MUX]		= &s4_pwm_c_mux.hw,
		[CLKID_PWM_C_DIV]		= &s4_pwm_c_div.hw,
		[CLKID_PWM_C_GATE]		= &s4_pwm_c_gate.hw,
		[CLKID_PWM_D_MUX]		= &s4_pwm_d_mux.hw,
		[CLKID_PWM_D_DIV]		= &s4_pwm_d_div.hw,
		[CLKID_PWM_D_GATE]		= &s4_pwm_d_gate.hw,
		[CLKID_PWM_E_MUX]		= &s4_pwm_e_mux.hw,
		[CLKID_PWM_E_DIV]		= &s4_pwm_e_div.hw,
		[CLKID_PWM_E_GATE]		= &s4_pwm_e_gate.hw,
		[CLKID_PWM_F_MUX]		= &s4_pwm_f_mux.hw,
		[CLKID_PWM_F_DIV]		= &s4_pwm_f_div.hw,
		[CLKID_PWM_F_GATE]		= &s4_pwm_f_gate.hw,
		[CLKID_PWM_G_MUX]		= &s4_pwm_g_mux.hw,
		[CLKID_PWM_G_DIV]		= &s4_pwm_g_div.hw,
		[CLKID_PWM_G_GATE]		= &s4_pwm_g_gate.hw,
		[CLKID_PWM_H_MUX]		= &s4_pwm_h_mux.hw,
		[CLKID_PWM_H_DIV]		= &s4_pwm_h_div.hw,
		[CLKID_PWM_H_GATE]		= &s4_pwm_h_gate.hw,
		[CLKID_PWM_I_MUX]		= &s4_pwm_i_mux.hw,
		[CLKID_PWM_I_DIV]		= &s4_pwm_i_div.hw,
		[CLKID_PWM_I_GATE]		= &s4_pwm_i_gate.hw,
		[CLKID_PWM_J_MUX]		= &s4_pwm_j_mux.hw,
		[CLKID_PWM_J_DIV]		= &s4_pwm_j_div.hw,
		[CLKID_PWM_J_GATE]		= &s4_pwm_j_gate.hw,

		[CLKID_SARADC_MUX]		= &s4_saradc_mux.hw,
		[CLKID_SARADC_DIV]		= &s4_saradc_div.hw,
		[CLKID_SARADC_GATE]		= &s4_saradc_gate.hw,

		[CLKID_GEN_MUX]			= &s4_gen_sel.hw,
		[CLKID_GEN_DIV]			= &s4_gen_div.hw,
		[CLKID_GEN_GATE]		= &s4_gen.hw,

		[CLKID_DDR]			= &s4_ddr.hw,
		[CLKID_DOS]			= &s4_dos.hw,
		[CLKID_ETHPHY]			= &s4_ethphy.hw,
		[CLKID_MALI_GATE]		= &s4_mali.hw,
		[CLKID_AOCPU]			= &s4_aocpu.hw,
		[CLKID_AUCPU]			= &s4_aucpu.hw,
		[CLKID_CEC]			= &s4_cec.hw,
		[CLKID_SD_EMMC_A]		= &s4_sdemmca.hw,
		[CLKID_SD_EMMC_B]		= &s4_sdemmcb.hw,
		[CLKID_NAND]			= &s4_nand.hw,
		[CLKID_SMARTCARD]		= &s4_smartcard.hw,
		[CLKID_ACODEC]			= &s4_acodec.hw,
		[CLKID_SPIFC]			= &s4_spifc.hw,
		[CLKID_MSR_CLK]			= &s4_msr_clk.hw,
		[CLKID_IR_CTRL]			= &s4_ir_ctrl.hw,

		[CLKID_AUDIO]			= &s4_audio.hw,
		[CLKID_ETH]			= &s4_eth.hw,
		[CLKID_UART_A]			= &s4_uart_a.hw,
		[CLKID_UART_B]			= &s4_uart_b.hw,
		[CLKID_UART_C]			= &s4_uart_c.hw,
		[CLKID_UART_D]			= &s4_uart_d.hw,
		[CLKID_UART_E]			= &s4_uart_e.hw,
		[CLKID_AIFIFO]			= &s4_aififo.hw,
		[CLKID_TS_DDR]			= &s4_ts_ddr.hw,
		[CLKID_TS_PLL]			= &s4_ts_pll.hw,
		[CLKID_G2D]			= &s4_g2d.hw,
		[CLKID_SPICC0]			= &s4_spicc0.hw,
		[CLKID_USB]			= &s4_usb.hw,
		[CLKID_I2C_M_A]			= &s4_i2c_m_a.hw,
		[CLKID_I2C_M_B]			= &s4_i2c_m_b.hw,
		[CLKID_I2C_M_C]			= &s4_i2c_m_c.hw,
		[CLKID_I2C_M_D]			= &s4_i2c_m_d.hw,
		[CLKID_I2C_M_E]			= &s4_i2c_m_e.hw,
		[CLKID_HDMITX_APB]		= &s4_hdmitx_apb.hw,
		[CLKID_I2C_S_A]			= &s4_i2c_s_a.hw,
		[CLKID_USB1_TO_DDR]		= &s4_usb1_to_ddr.hw,
		[CLKID_HDCP22]			= &s4_hdcp22.hw,
		[CLKID_MMC_APB]			= &s4_mmc_apb.hw,
		[CLKID_RSA]			= &s4_rsa.hw,
		[CLKID_CPU_DEBUG]		= &s4_cpu_debug.hw,
		[CLKID_VPU_INTR]		= &s4_vpu_intr.hw,
		[CLKID_DEMOD]			= &s4_demod.hw,
		[CLKID_SAR_ADC]			= &s4_sar_adc.hw,
		[CLKID_GIC]			= &s4_gic.hw,
		[CLKID_PWM_AB]			= &s4_pwm_ab.hw,
		[CLKID_PWM_CD]			= &s4_pwm_cd.hw,
		[CLKID_PWM_EF]			= &s4_pwm_ef.hw,
		[CLKID_PWM_GH]			= &s4_pwm_gh.hw,
		[CLKID_PWM_IJ]			= &s4_pwm_ij.hw,

		[CLKID_HDCP22_ESMCLK_MUX]	= &s4_hdcp22_esmclk_mux.hw,
		[CLKID_HDCP22_ESMCLK_DIV]	= &s4_hdcp22_esmclk_div.hw,
		[CLKID_HDCP22_ESMCLK_GATE]	= &s4_hdcp22_esmclk_gate.hw,
		[CLKID_HDCP22_SKPCLK_MUX]	= &s4_hdcp22_skpclk_mux.hw,
		[CLKID_HDCP22_SKPCLK_DIV]	= &s4_hdcp22_skpclk_div.hw,
		[CLKID_HDCP22_SKPCLK_GATE]	= &s4_hdcp22_skpclk_gate.hw,

		[NR_CLKS]			= NULL
	},
	.num = NR_CLKS,
};

static struct clk_hw_onecell_data s4d_hw_onecell_data = {
	.hws = {
		[CLKID_FIXED_PLL_DCO]		= &s4_fixed_pll_dco.hw,
		[CLKID_FIXED_PLL]		= &s4_fixed_pll.hw,
		[CLKID_SYS_PLL_DCO]		= &s4_sys_pll_dco.hw,
		[CLKID_SYS_PLL]			= &s4_sys_pll.hw,
		[CLKID_FCLK_DIV2_DIV]		= &s4_fclk_div2_div.hw,
		[CLKID_FCLK_DIV2]		= &s4_fclk_div2.hw,
		[CLKID_FCLK_DIV3_DIV]		= &s4_fclk_div3_div.hw,
		[CLKID_FCLK_DIV3]		= &s4_fclk_div3.hw,
		[CLKID_FCLK_DIV4_DIV]		= &s4_fclk_div4_div.hw,
		[CLKID_FCLK_DIV4]		= &s4_fclk_div4.hw,
		[CLKID_FCLK_DIV5_DIV]		= &s4_fclk_div5_div.hw,
		[CLKID_FCLK_DIV5]		= &s4_fclk_div5.hw,
		[CLKID_FCLK_DIV7_DIV]		= &s4_fclk_div7_div.hw,
		[CLKID_FCLK_DIV7]		= &s4_fclk_div7.hw,
		[CLKID_FCLK_DIV2P5_DIV]		= &s4_fclk_div2p5_div.hw,
		[CLKID_FCLK_DIV2P5]		= &s4_fclk_div2p5.hw,

		[CLKID_GP0_PLL_DCO]		= &s4_gp0_pll_dco.hw,
		[CLKID_GP0_PLL]			= &s4_gp0_pll.hw,
		[CLKID_GP1_PLL_DCO]		= &s4d_gp1_pll_dco.hw,
		[CLKID_GP1_PLL]			= &s4d_gp1_pll.hw,

		[CLKID_CPU_CLK_DYN]		= &s4_cpu_dyn_clk.hw,
		[CLKID_CPU_CLK]			= &s4_cpu_clk.hw,

		[CLKID_HIFI_PLL_DCO]		= &s4d_hifi_pll_dco.hw,
		[CLKID_HIFI_PLL]		= &s4d_hifi_pll.hw,
		[CLKID_HDMI_PLL_DCO]		= &s4_hdmi_pll_dco.hw,
		[CLKID_HDMI_PLL_OD]		= &s4_hdmi_pll_od.hw,
		[CLKID_HDMI_PLL]		= &s4_hdmi_pll.hw,
		[CLKID_MPLL_50M_DIV]		= &s4_mpll_50m_div.hw,
		[CLKID_MPLL_50M]		= &s4_mpll_50m.hw,
		[CLKID_MPLL_PREDIV]		= &s4_mpll_prediv.hw,
		[CLKID_MPLL0_DIV]		= &s4_mpll0_div.hw,
		[CLKID_MPLL0]			= &s4_mpll0.hw,
		[CLKID_MPLL1_DIV]		= &s4_mpll1_div.hw,
		[CLKID_MPLL1]			= &s4_mpll1.hw,
		[CLKID_MPLL2_DIV]		= &s4_mpll2_div.hw,
		[CLKID_MPLL2]			= &s4_mpll2.hw,
		[CLKID_MPLL3_DIV]		= &s4_mpll3_div.hw,
		[CLKID_MPLL3]			= &s4_mpll3.hw,

		[CLKID_RTC_32K_CLKIN]		= &s4_rtc_32k_clkin.hw,
		[CLKID_RTC_32K_DIV]		= &s4_rtc_32k_div.hw,
		[CLKID_RTC_32K_XATL]		= &s4_rtc_32k_xtal.hw,
		[CLKID_RTC_32K_MUX]		= &s4_rtc_32k_sel.hw,
		[CLKID_RTC_CLK]			= &s4_rtc_clk.hw,

		[CLKID_SYS_CLK_B_MUX]		= &s4_sysclk_b_sel.hw,
		[CLKID_SYS_CLK_B_DIV]		= &s4_sysclk_b_div.hw,
		[CLKID_SYS_CLK_B_GATE]		= &s4_sysclk_b.hw,
		[CLKID_SYS_CLK_A_MUX]		= &s4_sysclk_a_sel.hw,
		[CLKID_SYS_CLK_A_DIV]		= &s4_sysclk_a_div.hw,
		[CLKID_SYS_CLK_A_GATE]		= &s4_sysclk_a.hw,
		[CLKID_SYS_CLK]			= &s4_sys_clk.hw,

		[CLKID_CECA_32K_CLKIN]		= &s4_ceca_32k_clkin.hw,
		[CLKID_CECA_32K_DIV]		= &s4_ceca_32k_div.hw,
		[CLKID_CECA_32K_MUX_PRE]	= &s4_ceca_32k_sel_pre.hw,
		[CLKID_CECA_32K_MUX]		= &s4_ceca_32k_sel.hw,
		[CLKID_CECA_32K_CLKOUT]		= &s4_ceca_32k_clkout.hw,

		[CLKID_CECB_32K_CLKIN]		= &s4_cecb_32k_clkin.hw,
		[CLKID_CECB_32K_DIV]		= &s4_cecb_32k_div.hw,
		[CLKID_CECB_32K_MUX_PRE]	= &s4_cecb_32k_sel_pre.hw,
		[CLKID_CECB_32K_MUX]		= &s4_cecb_32k_sel.hw,
		[CLKID_CECB_32K_CLKOUT]		= &s4_cecb_32k_clkout.hw,

		[CLKID_SC_CLK_MUX]		= &s4_sc_clk_mux.hw,
		[CLKID_SC_CLK_DIV]		= &s4_sc_clk_div.hw,
		[CLKID_SC_CLK_GATE]		= &s4_sc_clk_gate.hw,

		[CLKID_24M_CLK_GATE]		= &s4_24M_clk_gate.hw,
		[CLKID_12M_CLK_DIV]		= &s4_12M_clk_div.hw,
		[CLKID_12M_CLK_GATE]		= &s4_12M_clk_gate.hw,

		[CLKID_VID_PLL]			= &s4_vid_pll_div.hw,
		[CLKID_VID_PLL_MUX]		= &s4_vid_pll_sel.hw,
		[CLKID_VID_PLL]			= &s4_vid_pll.hw,
		[CLKID_VCLK_MUX]		= &s4_vclk_sel.hw,
		[CLKID_VCLK2_MUX]		= &s4_vclk2_sel.hw,
		[CLKID_VCLK_INPUT]		= &s4_vclk_input.hw,
		[CLKID_VCLK2_INPUT]		= &s4_vclk2_input.hw,
		[CLKID_VCLK_DIV]		= &s4_vclk_div.hw,
		[CLKID_VCLK2_DIV]		= &s4_vclk2_div.hw,
		[CLKID_VCLK]			= &s4_vclk.hw,
		[CLKID_VCLK2]			= &s4_vclk2.hw,
		[CLKID_VCLK_DIV1]		= &s4_vclk_div1.hw,
		[CLKID_VCLK_DIV2_EN]		= &s4_vclk_div2_en.hw,
		[CLKID_VCLK_DIV4_EN]		= &s4_vclk_div4_en.hw,
		[CLKID_VCLK_DIV6_EN]		= &s4_vclk_div6_en.hw,
		[CLKID_VCLK_DIV12_EN]		= &s4_vclk_div12_en.hw,
		[CLKID_VCLK2_DIV1]		= &s4_vclk2_div1.hw,
		[CLKID_VCLK2_DIV2_EN]		= &s4_vclk2_div2_en.hw,
		[CLKID_VCLK2_DIV4_EN]		= &s4_vclk2_div4_en.hw,
		[CLKID_VCLK2_DIV6_EN]		= &s4_vclk2_div6_en.hw,
		[CLKID_VCLK2_DIV12_EN]		= &s4_vclk2_div12_en.hw,
		[CLKID_VCLK_DIV2]		= &s4_vclk_div2.hw,
		[CLKID_VCLK_DIV4]		= &s4_vclk_div4.hw,
		[CLKID_VCLK_DIV6]		= &s4_vclk_div6.hw,
		[CLKID_VCLK_DIV12]		= &s4_vclk_div12.hw,
		[CLKID_VCLK2_DIV2]		= &s4_vclk2_div2.hw,
		[CLKID_VCLK2_DIV4]		= &s4_vclk2_div4.hw,
		[CLKID_VCLK2_DIV6]		= &s4_vclk2_div6.hw,
		[CLKID_VCLK2_DIV12]		= &s4_vclk2_div12.hw,
		[CLKID_CTS_ENCI_MUX]		= &s4_cts_enci_sel.hw,
		[CLKID_CTS_ENCP_MUX]		= &s4_cts_encp_sel.hw,
		[CLKID_CTS_VDAC_MUX]		= &s4_cts_vdac_sel.hw,
		[CLKID_HDMI_TX_MUX]		= &s4_hdmi_tx_sel.hw,
		[CLKID_CTS_ENCI]		= &s4_cts_enci.hw,
		[CLKID_CTS_ENCP]		= &s4_cts_encp.hw,
		[CLKID_CTS_VDAC]		= &s4_cts_vdac.hw,
		[CLKID_HDMI_TX]			= &s4_hdmi_tx.hw,
		[CLKID_HDMI_MUX]		= &s4_hdmi_sel.hw,
		[CLKID_HDMI_DIV]		= &s4_hdmi_div.hw,
		[CLKID_HDMI]			= &s4_hdmi.hw,

		[CLKID_TS_CLK_DIV]		= &s4_ts_clk_div.hw,
		[CLKID_TS_CLK_GATE]		= &s4_ts_clk_gate.hw,

		[CLKID_MALI_0_SEL]		= &s4_mali_0_sel.hw,
		[CLKID_MALI_0_DIV]		= &s4_mali_0_div.hw,
		[CLKID_MALI_0]			= &s4_mali_0.hw,
		[CLKID_MALI_1_SEL]		= &s4_mali_1_sel.hw,
		[CLKID_MALI_1_DIV]		= &s4_mali_1_div.hw,
		[CLKID_MALI_1]			= &s4_mali_1.hw,
		[CLKID_MALI]			= &s4_mali_mux.hw,

		[CLKID_VDEC_P0_MUX]		= &s4_vdec_p0_mux.hw,
		[CLKID_VDEC_P0_DIV]		= &s4_vdec_p0_div.hw,
		[CLKID_VDEC_P0]			= &s4_vdec_p0.hw,
		[CLKID_VDEC_P1_MUX]		= &s4_vdec_p1_mux.hw,
		[CLKID_VDEC_P1_DIV]		= &s4_vdec_p1_div.hw,
		[CLKID_VDEC_P1]			= &s4_vdec_p1.hw,
		[CLKID_VDEC_MUX]		= &s4_vdec_mux.hw,

		[CLKID_HEVCF_P0_MUX]		= &s4_hevcf_p0_mux.hw,
		[CLKID_HEVCF_P0_DIV]		= &s4_hevcf_p0_div.hw,
		[CLKID_HEVCF_P0]		= &s4_hevcf_p0.hw,
		[CLKID_HEVCF_P1_MUX]		= &s4_hevcf_p1_mux.hw,
		[CLKID_HEVCF_P1_DIV]		= &s4_hevcf_p1_div.hw,
		[CLKID_HEVCF_P1]		= &s4_hevcf_p1.hw,
		[CLKID_HEVCF_MUX]		= &s4_hevcf_mux.hw,

		[CLKID_VPU_0_MUX]		= &s4_vpu_0_sel.hw,
		[CLKID_VPU_0_DIV]		= &s4_vpu_0_div.hw,
		[CLKID_VPU_0]			= &s4_vpu_0.hw,
		[CLKID_VPU_1_MUX]		= &s4_vpu_1_sel.hw,
		[CLKID_VPU_1_DIV]		= &s4_vpu_1_div.hw,
		[CLKID_VPU_1]			= &s4_vpu_1.hw,
		[CLKID_VPU]			= &s4_vpu.hw,

		[CLKID_VPU_CLKB_TMP_MUX]	= &s4_vpu_clkb_tmp_mux.hw,
		[CLKID_VPU_CLKB_TMP_DIV]	= &s4_vpu_clkb_tmp_div.hw,
		[CLKID_VPU_CLKB_TMP]		= &s4_vpu_clkb_tmp.hw,
		[CLKID_VPU_CLKB_DIV]		= &s4_vpu_clkb_div.hw,
		[CLKID_VPU_CLKB]		= &s4_vpu_clkb.hw,
		[CLKID_VPU_CLKC_P0_MUX]		= &s4_vpu_clkc_p0_mux.hw,
		[CLKID_VPU_CLKC_P0_DIV]		= &s4_vpu_clkc_p0_div.hw,
		[CLKID_VPU_CLKC_P0]		= &s4_vpu_clkc_p0.hw,
		[CLKID_VPU_CLKC_P1_MUX]		= &s4_vpu_clkc_p1_mux.hw,
		[CLKID_VPU_CLKC_P1_DIV]		= &s4_vpu_clkc_p1_div.hw,
		[CLKID_VPU_CLKC_P1]		= &s4_vpu_clkc_p1.hw,
		[CLKID_VPU_CLKC_MUX]		= &s4_vpu_clkc_mux.hw,

		[CLKID_VAPB_0_MUX]		= &s4_vapb_0_sel.hw,
		[CLKID_VAPB_0_DIV]		= &s4_vapb_0_div.hw,
		[CLKID_VAPB_0]			= &s4_vapb_0.hw,
		[CLKID_VAPB_1_MUX]		= &s4_vapb_1_sel.hw,
		[CLKID_VAPB_1_DIV]		= &s4_vapb_1_div.hw,
		[CLKID_VAPB_1]			= &s4_vapb_1.hw,
		[CLKID_VAPB]			= &s4_vapb.hw,

		[CLKID_GE2D]			= &s4_ge2d_gate.hw,

		[CLKID_VDIN_MEAS_MUX]		= &s4_vdin_meas_mux.hw,
		[CLKID_VDIN_MEAS_DIV]		= &s4_vdin_meas_div.hw,
		[CLKID_VDIN_MEAS_GATE]		= &s4_vdin_meas_gate.hw,

		[CLKID_SD_EMMC_C_CLK_MUX]	= &s4d_sd_emmc_c_clk0_sel.hw,
		[CLKID_SD_EMMC_C_CLK_DIV]	= &s4d_sd_emmc_c_clk0_div.hw,
		[CLKID_SD_EMMC_C_CLK]		= &s4d_sd_emmc_c_clk0.hw,

		[CLKID_SD_EMMC_A_CLK_MUX]	= &s4d_sd_emmc_a_clk0_sel.hw,
		[CLKID_SD_EMMC_A_CLK_DIV]	= &s4d_sd_emmc_a_clk0_div.hw,
		[CLKID_SD_EMMC_A_CLK]		= &s4d_sd_emmc_a_clk0.hw,
		[CLKID_SD_EMMC_B_CLK_MUX]	= &s4d_sd_emmc_b_clk0_sel.hw,
		[CLKID_SD_EMMC_B_CLK_DIV]	= &s4d_sd_emmc_b_clk0_div.hw,
		[CLKID_SD_EMMC_B_CLK]		= &s4d_sd_emmc_b_clk0.hw,

		[CLKID_SPICC0_MUX]		= &s4_spicc0_mux.hw,
		[CLKID_SPICC0_DIV]		= &s4_spicc0_div.hw,
		[CLKID_SPICC0_GATE]		= &s4_spicc0_gate.hw,

		[CLKID_PWM_A_MUX]		= &s4_pwm_a_mux.hw,
		[CLKID_PWM_A_DIV]		= &s4_pwm_a_div.hw,
		[CLKID_PWM_A_GATE]		= &s4_pwm_a_gate.hw,
		[CLKID_PWM_B_MUX]		= &s4_pwm_b_mux.hw,
		[CLKID_PWM_B_DIV]		= &s4_pwm_b_div.hw,
		[CLKID_PWM_B_GATE]		= &s4_pwm_b_gate.hw,
		[CLKID_PWM_C_MUX]		= &s4_pwm_c_mux.hw,
		[CLKID_PWM_C_DIV]		= &s4_pwm_c_div.hw,
		[CLKID_PWM_C_GATE]		= &s4_pwm_c_gate.hw,
		[CLKID_PWM_D_MUX]		= &s4_pwm_d_mux.hw,
		[CLKID_PWM_D_DIV]		= &s4_pwm_d_div.hw,
		[CLKID_PWM_D_GATE]		= &s4_pwm_d_gate.hw,
		[CLKID_PWM_E_MUX]		= &s4_pwm_e_mux.hw,
		[CLKID_PWM_E_DIV]		= &s4_pwm_e_div.hw,
		[CLKID_PWM_E_GATE]		= &s4_pwm_e_gate.hw,
		[CLKID_PWM_F_MUX]		= &s4_pwm_f_mux.hw,
		[CLKID_PWM_F_DIV]		= &s4_pwm_f_div.hw,
		[CLKID_PWM_F_GATE]		= &s4_pwm_f_gate.hw,
		[CLKID_PWM_G_MUX]		= &s4_pwm_g_mux.hw,
		[CLKID_PWM_G_DIV]		= &s4_pwm_g_div.hw,
		[CLKID_PWM_G_GATE]		= &s4_pwm_g_gate.hw,
		[CLKID_PWM_H_MUX]		= &s4_pwm_h_mux.hw,
		[CLKID_PWM_H_DIV]		= &s4_pwm_h_div.hw,
		[CLKID_PWM_H_GATE]		= &s4_pwm_h_gate.hw,
		[CLKID_PWM_I_MUX]		= &s4_pwm_i_mux.hw,
		[CLKID_PWM_I_DIV]		= &s4_pwm_i_div.hw,
		[CLKID_PWM_I_GATE]		= &s4_pwm_i_gate.hw,
		[CLKID_PWM_J_MUX]		= &s4_pwm_j_mux.hw,
		[CLKID_PWM_J_DIV]		= &s4_pwm_j_div.hw,
		[CLKID_PWM_J_GATE]		= &s4_pwm_j_gate.hw,

		[CLKID_SARADC_MUX]		= &s4_saradc_mux.hw,
		[CLKID_SARADC_DIV]		= &s4_saradc_div.hw,
		[CLKID_SARADC_GATE]		= &s4_saradc_gate.hw,

		[CLKID_GEN_MUX]			= &s4_gen_sel.hw,
		[CLKID_GEN_DIV]			= &s4_gen_div.hw,
		[CLKID_GEN_GATE]		= &s4_gen.hw,

		[CLKID_DDR]			= &s4_ddr.hw,
		[CLKID_DOS]			= &s4_dos.hw,
		[CLKID_ETHPHY]			= &s4_ethphy.hw,
		[CLKID_MALI_GATE]		= &s4_mali.hw,
		[CLKID_AOCPU]			= &s4_aocpu.hw,
		[CLKID_AUCPU]			= &s4_aucpu.hw,
		[CLKID_CEC]			= &s4_cec.hw,
		[CLKID_SD_EMMC_A]		= &s4_sdemmca.hw,
		[CLKID_SD_EMMC_B]		= &s4_sdemmcb.hw,
		[CLKID_NAND]			= &s4_nand.hw,
		[CLKID_SMARTCARD]		= &s4_smartcard.hw,
		[CLKID_ACODEC]			= &s4_acodec.hw,
		[CLKID_SPIFC]			= &s4_spifc.hw,
		[CLKID_MSR_CLK]			= &s4_msr_clk.hw,
		[CLKID_IR_CTRL]			= &s4_ir_ctrl.hw,

		[CLKID_AUDIO]			= &s4_audio.hw,
		[CLKID_ETH]			= &s4_eth.hw,
		[CLKID_UART_A]			= &s4_uart_a.hw,
		[CLKID_UART_B]			= &s4_uart_b.hw,
		[CLKID_UART_C]			= &s4_uart_c.hw,
		[CLKID_UART_D]			= &s4_uart_d.hw,
		[CLKID_UART_E]			= &s4_uart_e.hw,
		[CLKID_AIFIFO]			= &s4_aififo.hw,
		[CLKID_TS_DDR]			= &s4_ts_ddr.hw,
		[CLKID_TS_PLL]			= &s4_ts_pll.hw,
		[CLKID_G2D]			= &s4_g2d.hw,
		[CLKID_SPICC0]			= &s4_spicc0.hw,
		[CLKID_USB]			= &s4_usb.hw,
		[CLKID_I2C_M_A]			= &s4_i2c_m_a.hw,
		[CLKID_I2C_M_B]			= &s4_i2c_m_b.hw,
		[CLKID_I2C_M_C]			= &s4_i2c_m_c.hw,
		[CLKID_I2C_M_D]			= &s4_i2c_m_d.hw,
		[CLKID_I2C_M_E]			= &s4_i2c_m_e.hw,
		[CLKID_HDMITX_APB]		= &s4_hdmitx_apb.hw,
		[CLKID_I2C_S_A]			= &s4_i2c_s_a.hw,
		[CLKID_USB1_TO_DDR]		= &s4_usb1_to_ddr.hw,
		[CLKID_HDCP22]			= &s4_hdcp22.hw,
		[CLKID_MMC_APB]			= &s4_mmc_apb.hw,
		[CLKID_RSA]			= &s4_rsa.hw,
		[CLKID_CPU_DEBUG]		= &s4_cpu_debug.hw,
		[CLKID_VPU_INTR]		= &s4_vpu_intr.hw,
		[CLKID_DEMOD]			= &s4_demod.hw,
		[CLKID_SAR_ADC]			= &s4_sar_adc.hw,
		[CLKID_GIC]			= &s4_gic.hw,
		[CLKID_PWM_AB]			= &s4_pwm_ab.hw,
		[CLKID_PWM_CD]			= &s4_pwm_cd.hw,
		[CLKID_PWM_EF]			= &s4_pwm_ef.hw,
		[CLKID_PWM_GH]			= &s4_pwm_gh.hw,
		[CLKID_PWM_IJ]			= &s4_pwm_ij.hw,

		[CLKID_HDCP22_ESMCLK_MUX]	= &s4_hdcp22_esmclk_mux.hw,
		[CLKID_HDCP22_ESMCLK_DIV]	= &s4_hdcp22_esmclk_div.hw,
		[CLKID_HDCP22_ESMCLK_GATE]	= &s4_hdcp22_esmclk_gate.hw,
		[CLKID_HDCP22_SKPCLK_MUX]	= &s4_hdcp22_skpclk_mux.hw,
		[CLKID_HDCP22_SKPCLK_DIV]	= &s4_hdcp22_skpclk_div.hw,
		[CLKID_HDCP22_SKPCLK_GATE]	= &s4_hdcp22_skpclk_gate.hw,

		[CLKID_DEMOD_CORE_CLK_MUX]	= &s4_demod_core_clk_mux.hw,
		[CLKID_DEMOD_CORE_CLK_DIV]	= &s4_demod_core_clk_div.hw,
		[CLKID_DEMOD_CORE_CLK_GATE]	= &s4_demod_core_clk_gate.hw,
		[CLKID_ADC_EXTCLK_IN_MUX]	= &s4_adc_extclk_in_mux.hw,
		[CLKID_ADC_EXTCLK_IN_DIV]	= &s4_adc_extclk_in_div.hw,
		[CLKID_ADC_EXTCLK_IN_GATE]	= &s4_adc_extclk_in_gate.hw,
		[CLKID_DEMOD_CORE_T2_CLK_MUX]	= &s4_demod_core_t2_clk_mux.hw,
		[CLKID_DEMOD_CORE_T2_CLK_DIV]	= &s4_demod_core_t2_clk_div.hw,
		[CLKID_DEMOD_CORE_T2_CLK_GATE]	= &s4_demod_core_t2_clk_gate.hw,
		[CLKID_DEMOD_32K_CLKIN]		= &s4d_demod_32k_clkin.hw,
		[CLKID_DEMOD_32K_DIV]		= &s4d_demod_32k_div.hw,
		[CLKID_DEMOD_32K_XTAL]		= &s4d_demod_32k_xtal.hw,
		[CLKID_DEMOD_32K_CLK]		= &s4d_demod_32k_clk.hw,

		[NR_CLKS]			= NULL
	},
	.num = NR_CLKS,
};

/* Convenience table to populate regmap in .probe */
static struct clk_regmap *const s4_clk_regmaps[] __initconst  = {
	&s4_rtc_32k_clkin,
	&s4_rtc_32k_div,
	&s4_rtc_32k_xtal,
	&s4_rtc_32k_sel,
	&s4_rtc_clk,

	&s4_sysclk_b_sel,
	&s4_sysclk_b_div,
	&s4_sysclk_b,
	&s4_sysclk_a_sel,
	&s4_sysclk_a_div,
	&s4_sysclk_a,
	&s4_sys_clk,

	&s4_ceca_32k_clkin,
	&s4_ceca_32k_div,
	&s4_ceca_32k_sel_pre,
	&s4_ceca_32k_sel,
	&s4_ceca_32k_clkout,
	&s4_cecb_32k_clkin,
	&s4_cecb_32k_div,
	&s4_cecb_32k_sel_pre,
	&s4_cecb_32k_sel,
	&s4_cecb_32k_clkout,

	&s4_sc_clk_mux,
	&s4_sc_clk_div,
	&s4_sc_clk_gate,

	&s4_24M_clk_gate,
	&s4_12M_clk_gate,
	&s4_vid_pll_div,
	&s4_vid_pll_sel,
	&s4_vid_pll,
	&s4_vclk_sel,
	&s4_vclk2_sel,
	&s4_vclk_input,
	&s4_vclk2_input,
	&s4_vclk_div,
	&s4_vclk2_div,
	&s4_vclk,
	&s4_vclk2,
	&s4_vclk_div1,
	&s4_vclk_div2_en,
	&s4_vclk_div4_en,
	&s4_vclk_div6_en,
	&s4_vclk_div12_en,
	&s4_vclk2_div1,
	&s4_vclk2_div2_en,
	&s4_vclk2_div4_en,
	&s4_vclk2_div6_en,
	&s4_vclk2_div12_en,
	&s4_cts_enci_sel,
	&s4_cts_encp_sel,
	&s4_cts_vdac_sel,
	&s4_hdmi_tx_sel,
	&s4_cts_enci,
	&s4_cts_encp,
	&s4_cts_vdac,
	&s4_hdmi_tx,

	&s4_hdmi_sel,
	&s4_hdmi_div,
	&s4_hdmi,
	&s4_ts_clk_div,
	&s4_ts_clk_gate,

	&s4_mali_0_sel,
	&s4_mali_0_div,
	&s4_mali_0,
	&s4_mali_1_sel,
	&s4_mali_1_div,
	&s4_mali_1,
	&s4_mali_mux,

	&s4_vdec_p0_mux,
	&s4_vdec_p0_div,
	&s4_vdec_p0,
	&s4_vdec_p1_mux,
	&s4_vdec_p1_div,
	&s4_vdec_p1,
	&s4_vdec_mux,

	&s4_hevcf_p0_mux,
	&s4_hevcf_p0_div,
	&s4_hevcf_p0,
	&s4_hevcf_p1_mux,
	&s4_hevcf_p1_div,
	&s4_hevcf_p1,
	&s4_hevcf_mux,

	&s4_vpu_0_sel,
	&s4_vpu_0_div,
	&s4_vpu_0,
	&s4_vpu_1_sel,
	&s4_vpu_1_div,
	&s4_vpu_1,
	&s4_vpu,
	&s4_vpu_clkb_tmp_mux,
	&s4_vpu_clkb_tmp_div,
	&s4_vpu_clkb_tmp,
	&s4_vpu_clkb_div,
	&s4_vpu_clkb,
	&s4_vpu_clkc_p0_mux,
	&s4_vpu_clkc_p0_div,
	&s4_vpu_clkc_p0,
	&s4_vpu_clkc_p1_mux,
	&s4_vpu_clkc_p1_div,
	&s4_vpu_clkc_p1,
	&s4_vpu_clkc_mux,

	&s4_vapb_0_sel,
	&s4_vapb_0_div,
	&s4_vapb_0,
	&s4_vapb_1_sel,
	&s4_vapb_1_div,
	&s4_vapb_1,
	&s4_vapb,

	&s4_ge2d_gate,

	&s4_hdcp22_esmclk_mux,
	&s4_hdcp22_esmclk_div,
	&s4_hdcp22_esmclk_gate,
	&s4_hdcp22_skpclk_mux,
	&s4_hdcp22_skpclk_div,
	&s4_hdcp22_skpclk_gate,

	&s4_vdin_meas_mux,
	&s4_vdin_meas_div,
	&s4_vdin_meas_gate,

	&s4_sd_emmc_c_clk0_sel,
	&s4_sd_emmc_c_clk0_div,
	&s4_sd_emmc_c_clk0,
	&s4_sd_emmc_a_clk0_sel,
	&s4_sd_emmc_a_clk0_div,
	&s4_sd_emmc_a_clk0,
	&s4_sd_emmc_b_clk0_sel,
	&s4_sd_emmc_b_clk0_div,
	&s4_sd_emmc_b_clk0,

	&s4d_sd_emmc_c_clk0_sel,
	&s4d_sd_emmc_c_clk0_div,
	&s4d_sd_emmc_c_clk0,
	&s4d_sd_emmc_a_clk0_sel,
	&s4d_sd_emmc_a_clk0_div,
	&s4d_sd_emmc_a_clk0,
	&s4d_sd_emmc_b_clk0_sel,
	&s4d_sd_emmc_b_clk0_div,
	&s4d_sd_emmc_b_clk0,

	&s4_spicc0_mux,
	&s4_spicc0_div,
	&s4_spicc0_gate,

	&s4_pwm_a_mux,
	&s4_pwm_a_div,
	&s4_pwm_a_gate,
	&s4_pwm_b_mux,
	&s4_pwm_b_div,
	&s4_pwm_b_gate,
	&s4_pwm_c_mux,
	&s4_pwm_c_div,
	&s4_pwm_c_gate,
	&s4_pwm_d_mux,
	&s4_pwm_d_div,
	&s4_pwm_d_gate,
	&s4_pwm_e_mux,
	&s4_pwm_e_div,
	&s4_pwm_e_gate,
	&s4_pwm_f_mux,
	&s4_pwm_f_div,
	&s4_pwm_f_gate,
	&s4_pwm_g_mux,
	&s4_pwm_g_div,
	&s4_pwm_g_gate,
	&s4_pwm_h_mux,
	&s4_pwm_h_div,
	&s4_pwm_h_gate,
	&s4_pwm_i_mux,
	&s4_pwm_i_div,
	&s4_pwm_i_gate,
	&s4_pwm_j_mux,
	&s4_pwm_j_div,
	&s4_pwm_j_gate,

	&s4_saradc_mux,
	&s4_saradc_div,
	&s4_saradc_gate,

	&s4_gen_sel,
	&s4_gen_div,
	&s4_gen,

	&s4_ddr,
	&s4_dos,
	&s4_ethphy,
	&s4_mali,
	&s4_aocpu,
	&s4_aucpu,
	&s4_cec,
	&s4_sdemmca,
	&s4_sdemmcb,
	&s4_nand,
	&s4_smartcard,
	&s4_acodec,
	&s4_spifc,
	&s4_msr_clk,
	&s4_ir_ctrl,
	&s4_audio,
	&s4_eth,
	&s4_uart_a,
	&s4_uart_b,
	&s4_uart_c,
	&s4_uart_d,
	&s4_uart_e,
	&s4_aififo,
	&s4_ts_ddr,
	&s4_ts_pll,
	&s4_g2d,
	&s4_spicc0,
	&s4_usb,
	&s4_i2c_m_a,
	&s4_i2c_m_b,
	&s4_i2c_m_c,
	&s4_i2c_m_d,
	&s4_i2c_m_e,
	&s4_hdmitx_apb,
	&s4_i2c_s_a,
	&s4_usb1_to_ddr,
	&s4_hdcp22,
	&s4_mmc_apb,
	&s4_rsa,
	&s4_cpu_debug,
	&s4_vpu_intr,
	&s4_demod,
	&s4_sar_adc,
	&s4_gic,
	&s4_pwm_ab,
	&s4_pwm_cd,
	&s4_pwm_ef,
	&s4_pwm_gh,
	&s4_pwm_ij,
	&s4_demod_core_clk_mux,
	&s4_demod_core_clk_div,
	&s4_demod_core_clk_gate,
	&s4_adc_extclk_in_mux,
	&s4_adc_extclk_in_div,
	&s4_adc_extclk_in_gate,
	&s4_demod_core_t2_clk_mux,
	&s4_demod_core_t2_clk_div,
	&s4_demod_core_t2_clk_gate,
	&s4d_demod_32k_clkin,
	&s4d_demod_32k_div,
	&s4d_demod_32k_xtal,
	&s4d_demod_32k_clk
};

static struct clk_regmap *const s4_cpu_clk_regmaps[] __initconst = {
	&s4_cpu_dyn_clk,
	&s4_cpu_clk
};

static struct clk_regmap *const s4_pll_clk_regmaps[] __initconst = {
	&s4_fixed_pll_dco,
	&s4_fixed_pll,
	&s4_sys_pll_dco,
	&s4_sys_pll,
	&s4_fclk_div2,
	&s4_fclk_div3,
	&s4_fclk_div4,
	&s4_fclk_div5,
	&s4_fclk_div7,
	&s4_fclk_div2p5,
	&s4_gp0_pll_dco,
	&s4_gp0_pll,
	&s4d_gp1_pll_dco,
	&s4d_gp1_pll,

	&s4_hifi_pll_dco,
	&s4_hifi_pll,
	&s4d_hifi_pll_dco,
	&s4d_hifi_pll,
	&s4_hdmi_pll_dco,
	&s4_hdmi_pll_od,
	&s4_hdmi_pll,
	&s4_mpll_50m,
	&s4_mpll0_div,
	&s4_mpll0,
	&s4_mpll1_div,
	&s4_mpll1,
	&s4_mpll2_div,
	&s4_mpll2,
	&s4_mpll3_div,
	&s4_mpll3
};

static int meson_s4_dvfs_setup(struct platform_device *pdev)
{
	int ret;

	/*for pxp*/

	/* Setup clock notifier for sys_pll */
	ret = clk_notifier_register(s4_sys_pll.hw.clk,
				    &s4_sys_pll_nb_data.nb);
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

static struct regmap *s4_regmap_resource(struct device *dev, char *name)
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

static int __ref meson_s4_probe(struct platform_device *pdev)
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
	basic_map = s4_regmap_resource(dev, "basic");
	if (IS_ERR(basic_map)) {
		dev_err(dev, "basic clk registers not found\n");
		return PTR_ERR(basic_map);
	}

	pll_map = s4_regmap_resource(dev, "pll");
	if (IS_ERR(pll_map)) {
		dev_err(dev, "pll clk registers not found\n");
		return PTR_ERR(pll_map);
	}

	cpu_clk_map = s4_regmap_resource(dev, "cpu_clk");
	if (IS_ERR(cpu_clk_map)) {
		dev_err(dev, "cpu clk registers not found\n");
		return PTR_ERR(cpu_clk_map);
	}

	/* Populate regmap for the regmap backed clocks */
	for (i = 0; i < ARRAY_SIZE(s4_clk_regmaps); i++)
		s4_clk_regmaps[i]->map = basic_map;

	for (i = 0; i < ARRAY_SIZE(s4_cpu_clk_regmaps); i++)
		s4_cpu_clk_regmaps[i]->map = cpu_clk_map;

	for (i = 0; i < ARRAY_SIZE(s4_pll_clk_regmaps); i++)
		s4_pll_clk_regmaps[i]->map = pll_map;

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

	meson_s4_dvfs_setup(pdev);

	return devm_of_clk_add_hw_provider(dev, of_clk_hw_onecell_get,
					   (void *)hw_onecell_data);
}

static const struct of_device_id clkc_match_table[] = {
	{
		.compatible = "amlogic,s4-clkc",
		.data = &s4_hw_onecell_data
	},
	{
		.compatible = "amlogic,s4d-clkc",
		.data = &s4d_hw_onecell_data
	},
	{}
};

static struct platform_driver s4_driver = {
	.probe		= meson_s4_probe,
	.driver		= {
		.name	= "s4-clkc",
		.of_match_table = clkc_match_table,
	},
};

#ifndef CONFIG_AMLOGIC_MODIFY
builtin_platform_driver(s4_driver);
#else
#ifndef MODULE
static int __init s4_clkc_init(void)
{
	return platform_driver_register(&s4_driver);
}
arch_initcall_sync(s4_clkc_init);
#else
int __init meson_s4_clkc_init(void)
{
	return platform_driver_register(&s4_driver);
}
#endif
#endif

MODULE_LICENSE("GPL v2");
