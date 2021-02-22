// SPDX-License-Identifier: GPL-2.0+
/*
 * Amlogic Meson-T7 Clock Controller Driver
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
#include "t7.h"
#include "clkcs_init.h"
#include <dt-bindings/clock/amlogic,t7-clkc.h>
#include "clk-secure.h"

static DEFINE_SPINLOCK(meson_clk_lock);

static struct clk_regmap t7_fixed_pll_dco = {
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
		.parent_names = (const char *[]){ "xtal" },
		.num_parents = 1,
		.flags = CLK_IS_CRITICAL | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap t7_fixed_pll = {
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
			&t7_fixed_pll_dco.hw
		},
		.num_parents = 1,
		/*
		 * This clock won't ever change at runtime so
		 * CLK_SET_RATE_PARENT is not required
		 */
		.flags = CLK_IS_CRITICAL | CLK_GET_RATE_NOCACHE,
	},
};

static const struct clk_ops meson_pll_clk_no_ops = {};

/*
 * the sys pll DCO value should be 3G~6G,
 * otherwise the sys pll can not lock.
 * od is for 32 bit.
 */

#ifdef CONFIG_ARM
static const struct pll_params_table t7_sys_pll_params_table[] = {
	PLL_PARAMS(67, 1, 0), /*DCO=1608M OD=1608MM*/
	PLL_PARAMS(71, 1, 0), /*DCO=1704MM OD=1704M*/
	PLL_PARAMS(75, 1, 0), /*DCO=1800M OD=1800M*/
	PLL_PARAMS(126, 1, 1), /*DCO=3024 OD=1512M*/
	PLL_PARAMS(116, 1, 1), /*DCO=2784 OD=1392M*/
	PLL_PARAMS(118, 1, 1), /*DCO=2832M OD=1416M*/
	PLL_PARAMS(100, 1, 1), /*DCO=2400M OD=1200M*/
	PLL_PARAMS(79, 1, 0), /*DCO=1896M OD=1896M*/
	PLL_PARAMS(80, 1, 0), /*DCO=1920M OD=1920M*/
	PLL_PARAMS(84, 1, 0), /*DCO=2016M OD=2016M*/
	PLL_PARAMS(92, 1, 0), /*DCO=2208M OD=2208M*/
	{ /* sentinel */ }
};
#else
static const struct pll_params_table t7_sys_pll_params_table[] = {
	PLL_PARAMS(67, 1), /*DCO=1608M OD=1608MM*/
	PLL_PARAMS(71, 1), /*DCO=1704MM OD=1704M*/
	PLL_PARAMS(75, 1), /*DCO=1800M OD=1800M*/
	PLL_PARAMS(126, 1), /*DCO=3024 OD=1512M*/
	PLL_PARAMS(116, 1), /*DCO=2784 OD=1392M*/
	PLL_PARAMS(118, 1), /*DCO=2832M OD=1416M*/
	PLL_PARAMS(100, 1), /*DCO=2400M OD=1200M*/
	PLL_PARAMS(79, 1), /*DCO=1896M OD=1896M*/
	PLL_PARAMS(80, 1), /*DCO=1920M OD=1920M*/
	PLL_PARAMS(84, 1), /*DCO=2016M OD=2016M*/
	PLL_PARAMS(92, 1), /*DCO=2208M OD=2208M*/
	{ /* sentinel */ }
};
#endif

static struct clk_regmap t7_sys_pll_dco = {
	.data = &(struct meson_clk_pll_data){
		.en = {
			.reg_off = ANACTRL_SYS0PLL_CTRL0,
			.shift   = 28,
			.width   = 1,
		},
		.m = {
			.reg_off = ANACTRL_SYS0PLL_CTRL0,
			.shift   = 0,
			.width   = 8,
		},
		.n = {
			.reg_off = ANACTRL_SYS0PLL_CTRL0,
			.shift   = 16,
			.width   = 5,
		},
#ifdef CONFIG_ARM
		/* od for 32bit */
		.od = {
			.reg_off = ANACTRL_SYS0PLL_CTRL0,
			.shift	 = 12,
			.width	 = 3,
		},
#endif
		.table = t7_sys_pll_params_table,
		.l = {
			.reg_off = ANACTRL_SYS0PLL_CTRL0,
			.shift   = 31,
			.width   = 1,
		},
		.rst = {
			.reg_off = ANACTRL_SYS0PLL_CTRL0,
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
		.parent_names = (const char *[]){ "xtal" },
		.num_parents = 1,
		/* This clock feeds the CPU, avoid disabling it */
		.flags = CLK_IS_CRITICAL,
	},
};

static struct clk_regmap t7_sys1_pll_dco = {
	.data = &(struct meson_clk_pll_data){
		.en = {
			.reg_off = ANACTRL_SYS1PLL_CTRL0,
			.shift   = 28,
			.width   = 1,
		},
		.m = {
			.reg_off = ANACTRL_SYS1PLL_CTRL0,
			.shift   = 0,
			.width   = 8,
		},
		.n = {
			.reg_off = ANACTRL_SYS1PLL_CTRL0,
			.shift   = 16,
			.width   = 5,
		},
#ifdef CONFIG_ARM
		/* od for 32bit */
		.od = {
			.reg_off = ANACTRL_SYS1PLL_CTRL0,
			.shift	 = 12,
			.width	 = 3,
		},
#endif
		.table = t7_sys_pll_params_table,
		.l = {
			.reg_off = ANACTRL_SYS1PLL_CTRL0,
			.shift   = 31,
			.width   = 1,
		},
		.rst = {
			.reg_off = ANACTRL_SYS1PLL_CTRL0,
			.shift   = 29,
			.width   = 1,
		},
		.smc_id = SECURE_PLL_CLK,
		.secid_disable = SECID_SYS1_DCO_PLL_DIS,
		.secid = SECID_SYS1_DCO_PLL,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sys1_pll_dco",
		.ops = &meson_secure_pll_v2_ops,
		.parent_names = (const char *[]){ "xtal" },
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
 *    it will be a lot of mass because of unit deferentces.
 *
 * Keep Consistent with 64bit, creat a Virtual clock for sys pll
 */
static struct clk_regmap t7_sys_pll = {
	.hw.init = &(struct clk_init_data){
		.name = "sys_pll",
		.ops = &meson_pll_clk_no_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_sys_pll_dco.hw
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

static struct clk_regmap t7_sys1_pll = {
	.hw.init = &(struct clk_init_data){
		.name = "sys1_pll",
		.ops = &meson_pll_clk_no_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_sys1_pll_dco.hw
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
static struct clk_regmap t7_sys_pll = {
	.data = &(struct clk_regmap_div_data){
		.offset = ANACTRL_SYS0PLL_CTRL0,
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
			&t7_sys_pll_dco.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_sys1_pll = {
	.data = &(struct clk_regmap_div_data){
		.offset = ANACTRL_SYS1PLL_CTRL0,
		.shift = 12,
		.width = 3,
		.flags = CLK_DIVIDER_POWER_OF_TWO,
		.smc_id = SECURE_PLL_CLK,
		.secid = SECID_SYS1_PLL_OD
	},
	.hw.init = &(struct clk_init_data){
		.name = "sys1_pll",
		.ops = &clk_regmap_secure_v2_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_sys1_pll_dco.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};
#endif

static struct clk_fixed_factor t7_fclk_div2_div = {
	.mult = 1,
	.div = 2,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div2_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) { &t7_fixed_pll.hw },
		.num_parents = 1,
	},
};

static struct clk_regmap t7_fclk_div2 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = ANACTRL_FIXPLL_CTRL1,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div2",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_fclk_div2_div.hw
		},
		.num_parents = 1,
		.flags = CLK_IS_CRITICAL,
	},
};

static struct clk_fixed_factor t7_fclk_div3_div = {
	.mult = 1,
	.div = 3,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div3_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) { &t7_fixed_pll.hw },
		.num_parents = 1,
	},
};

static struct clk_regmap t7_fclk_div3 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = ANACTRL_FIXPLL_CTRL1,
		.bit_idx = 20,
	},
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div3",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_fclk_div3_div.hw
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

static struct clk_fixed_factor t7_fclk_div4_div = {
	.mult = 1,
	.div = 4,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div4_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) { &t7_fixed_pll.hw },
		.num_parents = 1,
	},
};

static struct clk_regmap t7_fclk_div4 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = ANACTRL_FIXPLL_CTRL1,
		.bit_idx = 21,
	},
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div4",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_fclk_div4_div.hw
		},
		.num_parents = 1,
		.flags = CLK_IS_CRITICAL,
	},
};

static struct clk_fixed_factor t7_fclk_div5_div = {
	.mult = 1,
	.div = 5,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div5_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) { &t7_fixed_pll.hw },
		.num_parents = 1,
	},
};

static struct clk_regmap t7_fclk_div5 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = ANACTRL_FIXPLL_CTRL1,
		.bit_idx = 22,
	},
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div5",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_fclk_div5_div.hw
		},
		.num_parents = 1,
		.flags = CLK_IS_CRITICAL,
	},
};

static struct clk_fixed_factor t7_fclk_div7_div = {
	.mult = 1,
	.div = 7,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div7_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) { &t7_fixed_pll.hw },
		.num_parents = 1,
	},
};

static struct clk_regmap t7_fclk_div7 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = ANACTRL_FIXPLL_CTRL1,
		.bit_idx = 23,
	},
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div7",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_fclk_div7_div.hw
		},
		.num_parents = 1,
		.flags = CLK_IS_CRITICAL,
	},
};

static struct clk_fixed_factor t7_fclk_div2p5_div = {
	.mult = 2,
	.div = 5,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div2p5_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_fixed_pll.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap t7_fclk_div2p5 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = ANACTRL_FIXPLL_CTRL1,
		.bit_idx = 25,
	},
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div2p5",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_fclk_div2p5_div.hw
		},
		.num_parents = 1,
		.flags = CLK_IS_CRITICAL,
	},
};

#ifdef CONFIG_ARM
static const struct pll_params_table t7_gp0_pll_table[] = {
	PLL_PARAMS(141, 1, 2), /* DCO = 3384M OD = 2 PLL = 846M */
	PLL_PARAMS(132, 1, 2), /* DCO = 3168M OD = 2 PLL = 792M */
	PLL_PARAMS(248, 1, 3), /* DCO = 5952M OD = 3 PLL = 744M */
	PLL_PARAMS(96, 1, 1), /* DCO = 2304M OD = 1 PLL = 1152M */
	{ /* sentinel */  }
};
#else
static const struct pll_params_table t7_gp0_pll_table[] = {
	PLL_PARAMS(141, 1), /* DCO = 3384M OD = 2 PLL = 846M */
	PLL_PARAMS(132, 1), /* DCO = 3168M OD = 2 PLL = 792M */
	PLL_PARAMS(248, 1), /* DCO = 5952M OD = 3 PLL = 744M */
	PLL_PARAMS(96, 1), /* DCO = 2304M OD = 1 PLL = 1152M */
	{ /* sentinel */  }
};
#endif

/*
 * Internal gp0 pll emulation configuration parameters
 */
static const struct reg_sequence t7_gp0_init_regs[] = {
	{ .reg = ANACTRL_GP0PLL_CTRL1,	.def = 0x00000000 },
	{ .reg = ANACTRL_GP0PLL_CTRL2,	.def = 0x00000000 },
	{ .reg = ANACTRL_GP0PLL_CTRL3,	.def = 0x48681c00 },
	{ .reg = ANACTRL_GP0PLL_CTRL4,	.def = 0x88770290 },
	{ .reg = ANACTRL_GP0PLL_CTRL5,	.def = 0x39272000 },
	{ .reg = ANACTRL_GP0PLL_CTRL6,	.def = 0x56540000 }
};

static struct clk_regmap t7_gp0_pll_dco = {
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
		.table = t7_gp0_pll_table,
		.init_regs = t7_gp0_init_regs,
		.init_count = ARRAY_SIZE(t7_gp0_init_regs),
	},
	.hw.init = &(struct clk_init_data){
		.name = "gp0_pll_dco",
		.ops = &meson_clk_pll_ops,
		.parent_names = (const char *[]){ "xtal" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

#ifdef CONFIG_ARM
static struct clk_regmap t7_gp0_pll = {
	.hw.init = &(struct clk_init_data){
		.name = "gp0_pll",
		.ops = &meson_pll_clk_no_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_gp0_pll_dco.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};
#else
static struct clk_regmap t7_gp0_pll = {
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
			&t7_gp0_pll_dco.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
	},
};
#endif

#ifdef CONFIG_ARM
static const struct pll_params_table t7_gp1_pll_table[] = {
	PLL_PARAMS(200, 1, 2), /*DCO=4800M OD=1200M*/
	PLL_PARAMS(125, 1, 1), /*DCO=3000M OD=1500M*/
	{ /* sentinel */  }
};
#else
static const struct pll_params_table t7_gp1_pll_table[] = {
	PLL_PARAMS(200, 1), /*DCO=4800M OD=1200M*/
	PLL_PARAMS(125, 1), /*DCO=3000M OD=1500M*/
	{ /* sentinel */  }
};
#endif

static struct clk_regmap t7_gp1_pll_dco = {
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
		.table = t7_gp1_pll_table,
	},
	.hw.init = &(struct clk_init_data){
		.name = "gp1_pll_dco",
		.ops = &meson_secure_clk_pll_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "xtal",
		},
		.num_parents = 1,
		/* This clock feeds the DSU, avoid disabling it */
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap t7_gp1_pll = {
	.data = &(struct clk_regmap_div_data){
		.offset = ANACTRL_GP1PLL_CTRL0,
		.shift = 16,
		.width = 3,
		.flags = (CLK_DIVIDER_POWER_OF_TWO |
			  CLK_DIVIDER_ROUND_CLOSEST),
	},
	.hw.init = &(struct clk_init_data){
		.name = "gp1_pll",
		.ops = &clk_regmap_divider_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_gp1_pll_dco.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

#ifdef CONFIG_ARM
static const struct pll_params_table t7_mclk_pll_table[] = {
	PLL_PARAMS(99, 1, 1), /* DCO = 2376M OD = 1 PLL = 1152M */
	PLL_PARAMS(100, 1, 1), /* DCO = 2400M */
	{ /* sentinel */  }
};
#else
static const struct pll_params_table t7_mclk_pll_table[] = {
	PLL_PARAMS(99, 1), /* DCO = 2376M OD = 1 PLL = 1152M */
	PLL_PARAMS(100, 1), /* DCO = 2400M */
	{ /* sentinel */  }
};
#endif

static const struct reg_sequence t7_mclk_init_regs[] = {
	{ .reg = ANACTRL_MCLK_PLL_CNTL0, .def = 0x20011064, .delay_us = 20 },
	{ .reg = ANACTRL_MCLK_PLL_CNTL0, .def = 0x30011064 },
	{ .reg = ANACTRL_MCLK_PLL_CNTL1, .def = 0x1470500f },
	{ .reg = ANACTRL_MCLK_PLL_CNTL2, .def = 0x00023041 },
	{ .reg = ANACTRL_MCLK_PLL_CNTL3, .def = 0x18180000 },
	{ .reg = ANACTRL_MCLK_PLL_CNTL4, .def = 0x00180303 },
	{ .reg = ANACTRL_MCLK_PLL_CNTL0, .def = 0x10011064, .delay_us = 20 },
	{ .reg = ANACTRL_MCLK_PLL_CNTL2, .def = 0x00023001, .delay_us = 20 }
};

static struct clk_regmap t7_mclk_pll_dco = {
	.data = &(struct meson_clk_pll_data){
		.en = {
			.reg_off = ANACTRL_MCLK_PLL_CNTL0,
			.shift   = 28,
			.width   = 1,
		},
		.m = {
			.reg_off = ANACTRL_MCLK_PLL_CNTL0,
			.shift   = 0,
			.width   = 8,
		},
		.n = {
			.reg_off = ANACTRL_MCLK_PLL_CNTL0,
			.shift   = 16,
			.width   = 5,
		},
#ifdef CONFIG_ARM
		/* for 32bit */
		.od = {
			.reg_off = ANACTRL_MCLK_PLL_CNTL0,
			.shift	 = 12,
			.width	 = 3,
		},
#endif
		.l = {
			.reg_off = ANACTRL_MCLK_PLL_CNTL0,
			.shift   = 31,
			.width   = 1,
		},
		.rst = {
			.reg_off = ANACTRL_MCLK_PLL_CNTL0,
			.shift   = 29,
			.width   = 1,
		},
		.table = t7_mclk_pll_table,
		.init_regs = t7_mclk_init_regs,
		.init_count = ARRAY_SIZE(t7_mclk_init_regs),
	},
	.hw.init = &(struct clk_init_data){
		.name = "mclk_pll_dco",
		.ops = &meson_clk_pll_ops,
		.parent_names = (const char *[]){ "xtal" },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_IS_CRITICAL
	},
};

/* max div is 16 */
static const struct clk_div_table mclk_div[] = {
	{ .val = 0, .div = 1 },
	{ .val = 1, .div = 2 },
	{ .val = 2, .div = 4 },
	{ .val = 3, .div = 8 },
	{ .val = 4, .div = 16 },
	{ /* sentinel */ }
};

static struct clk_regmap t7_mclk_pre_od = {
	.data = &(struct clk_regmap_div_data){
		.offset = ANACTRL_MCLK_PLL_CNTL0,
		.shift = 12,
		.width = 3,
		.table = mclk_div,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mclk_pre_od",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_mclk_pll_dco.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_mclk_pll = {
	.data = &(struct clk_regmap_div_data){
		.offset = ANACTRL_MCLK_PLL_CNTL4,
		.shift = 16,
		.width = 5,
		.flags = CLK_DIVIDER_ONE_BASED | CLK_DIVIDER_ROUND_CLOSEST,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mclk_pll",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_mclk_pre_od.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

/*a53 cpu_clk, get the table from ucode */
static const struct cpu_dyn_table t7_cpu_dyn_table[] = {
	CPU_LOW_PARAMS(24000000, 0, 0, 0),
	CPU_LOW_PARAMS(100000000, 1, 1, 9),
	CPU_LOW_PARAMS(250000000, 1, 1, 3),
	CPU_LOW_PARAMS(333333333, 2, 1, 1),
	CPU_LOW_PARAMS(500000000, 1, 1, 1),
	CPU_LOW_PARAMS(666666666, 2, 0, 0),
	CPU_LOW_PARAMS(1000000000, 1, 0, 0),
};

static struct clk_regmap t7_cpu_dyn_clk = {
	.data = &(struct meson_sec_cpu_dyn_data){
		.table = t7_cpu_dyn_table,
		.table_cnt = ARRAY_SIZE(t7_cpu_dyn_table),
		.secid_dyn_rd = SECID_CPU_CLK_RD,
		.secid_dyn = SECID_CPU_CLK_DYN,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cpu_dyn_clk",
		.ops = &meson_sec_cpu_dyn_ops,
		.parent_data = (const struct clk_parent_data []) {
			{ .fw_name = "xtal", },
			{ .hw = &t7_fclk_div2.hw },
			{ .hw = &t7_fclk_div3.hw },
			{ .hw = &t7_fclk_div2p5.hw }
		},
		.num_parents = 4,
	},
};

static struct clk_regmap t7_cpu_clk = {
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
			&t7_cpu_dyn_clk.hw,
			&t7_sys1_pll.hw,
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_a73_dyn_clk = {
	.data = &(struct meson_sec_cpu_dyn_data){
		.table = t7_cpu_dyn_table,
		.table_cnt = ARRAY_SIZE(t7_cpu_dyn_table),
		.secid_dyn_rd = SECID_A73_CLK_RD,
		.secid_dyn = SECID_A73_CLK_DYN,
	},
	.hw.init = &(struct clk_init_data){
		.name = "a73_dyn_clk",
		.ops = &meson_sec_cpu_dyn_ops,
		.parent_data = (const struct clk_parent_data []) {
			{ .fw_name = "xtal", },
			{ .hw = &t7_fclk_div2.hw },
			{ .hw = &t7_fclk_div3.hw },
			{ .hw = &t7_fclk_div2p5.hw }
		},
		.num_parents = 4,
	},
};

static struct clk_regmap t7_a73_clk = {
	.data = &(struct clk_regmap_mux_data){
		.mask = 0x1,
		.shift = 11,
		.flags = CLK_MUX_ROUND_CLOSEST,
		.smc_id = SECURE_CPU_CLK,
		.secid = SECID_A73_CLK_SEL,
		.secid_rd = SECID_A73_CLK_RD
	},
	.hw.init = &(struct clk_init_data){
		.name = "a73_clk",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_a73_dyn_clk.hw,
			&t7_sys_pll.hw,
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

#if 0
/* Datasheet names this field as "Cpu_clk_sync_mux_sel" bit 0 */
static struct clk_regmap t7_cpu1_clk = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CPUCTRL_SYS_CPU_CLK_CTRL6,
		.mask = 0x1,
		.shift = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cpu1_clk",
		.ops = &clk_regmap_secure_mux_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_cpu_clk.hw,
			/* This CPU also have a dedicated clock tree */
		},
		.num_parents = 1,
	},
};

/* Datasheet names this field as "Cpu_clk_sync_mux_sel" bit 1 */
static struct clk_regmap t7_cpu2_clk = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CPUCTRL_SYS_CPU_CLK_CTRL6,
		.mask = 0x1,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cpu2_clk",
		.ops = &clk_regmap_secure_mux_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_cpu_clk.hw,
			/* This CPU also have a dedicated clock tree */
		},
		.num_parents = 1,
	},
};

/* Datasheet names this field as "Cpu_clk_sync_mux_sel" bit 2 */
static struct clk_regmap t7_cpu3_clk = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CPUCTRL_SYS_CPU_CLK_CTRL6,
		.mask = 0x1,
		.shift = 26,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cpu3_clk",
		.ops = &clk_regmap_secure_mux_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_cpu_clk.hw,
			/* This CPU also have a dedicated clock tree */
		},
		.num_parents = 1,
	},
};

#endif

struct t7_sys_pll_nb_data {
	struct notifier_block nb;
	struct clk_hw *sys_pll;
	struct clk_hw *cpu_clk;
	struct clk_hw *cpu_dyn_clk;
};

static int t7_sys_pll_notifier_cb(struct notifier_block *nb,
				   unsigned long event, void *data)
{
	struct t7_sys_pll_nb_data *nb_data =
		container_of(nb, struct t7_sys_pll_nb_data, nb);

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
				  nb_data->cpu_dyn_clk);

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

		udelay(100);

		return NOTIFY_OK;

	case POST_RATE_CHANGE:
		/*
		 * The sys_pll has ben updated, now switch back cpu_clk to
		 * sys_pll
		 */

		/* Configure cpu_clk to use sys_pll */
		clk_hw_set_parent(nb_data->cpu_clk,
				  nb_data->sys_pll);

		udelay(100);

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

static struct t7_sys_pll_nb_data t7_sys1_pll_nb_data = {
	.sys_pll = &t7_sys1_pll.hw,
	.cpu_clk = &t7_cpu_clk.hw,
	.cpu_dyn_clk = &t7_cpu_dyn_clk.hw,
	.nb.notifier_call = t7_sys_pll_notifier_cb,
};

static struct t7_sys_pll_nb_data t7_sys_pll_nb_data = {
	.sys_pll = &t7_sys_pll.hw,
	.cpu_clk = &t7_a73_clk.hw,
	.cpu_dyn_clk = &t7_a73_dyn_clk.hw,
	.nb.notifier_call = t7_sys_pll_notifier_cb,
};

#ifdef CONFIG_ARM
static const struct pll_params_table t7_hifi_pll_table[] = {
	PLL_PARAMS(150, 1, 1), /* DCO = 1806.336M OD = 1 */
	{ /* sentinel */  }
};
#else
static const struct pll_params_table t7_hifi_pll_table[] = {
	PLL_PARAMS(150, 1), /* DCO = 1806.336M */
	{ /* sentinel */  }
};
#endif

/*
 * Internal hifi pll emulation configuration parameters
 */
static const struct reg_sequence t7_hifi_init_regs[] = {
	{ .reg = ANACTRL_HIFIPLL_CTRL1,	.def = 0x00010e56 },
	{ .reg = ANACTRL_HIFIPLL_CTRL2,	.def = 0x00000000 },
	{ .reg = ANACTRL_HIFIPLL_CTRL3,	.def = 0x6a285c00 },
	{ .reg = ANACTRL_HIFIPLL_CTRL4,	.def = 0x65771290 },
	{ .reg = ANACTRL_HIFIPLL_CTRL5,	.def = 0x39272000 },
	{ .reg = ANACTRL_HIFIPLL_CTRL6,	.def = 0x56540000 }
};

static struct clk_regmap t7_hifi_pll_dco = {
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
		.table = t7_hifi_pll_table,
		.init_regs = t7_hifi_init_regs,
		.init_count = ARRAY_SIZE(t7_hifi_init_regs),
		.flags = CLK_MESON_PLL_ROUND_CLOSEST,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hifi_pll_dco",
		.ops = &meson_clk_pll_ops,
		.parent_names = (const char *[]){ "xtal" },
		.num_parents = 1,
	},
};

static struct clk_regmap t7_hifi_pll = {
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
			&t7_hifi_pll_dco.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
	},
};

/*
 * The Meson t7 PCIE PLL is fined tuned to deliver a very precise
 * 100MHz reference clock for the PCIe Analog PHY, and thus requires
 * a strict register sequence to enable the PLL.
 */
static const struct reg_sequence t7_pcie_pll_init_regs[] = {
	{ .reg = ANACTRL_PCIEPLL_CTRL0,	.def = 0x200c04c8 },
	{ .reg = ANACTRL_PCIEPLL_CTRL0,	.def = 0x300c04c8 },
	{ .reg = ANACTRL_PCIEPLL_CTRL1,	.def = 0x30000000 },
	{ .reg = ANACTRL_PCIEPLL_CTRL2,	.def = 0x00001100 },
	{ .reg = ANACTRL_PCIEPLL_CTRL3,	.def = 0x10058e00 },
	{ .reg = ANACTRL_PCIEPLL_CTRL4,	.def = 0x000100c0 },
	{ .reg = ANACTRL_PCIEPLL_CTRL5,	.def = 0x68000040 },
	{ .reg = ANACTRL_PCIEPLL_CTRL5,	.def = 0x68000060, .delay_us = 20 },
	{ .reg = ANACTRL_PCIEPLL_CTRL4,	.def = 0x008100c0, .delay_us = 10 },
	{ .reg = ANACTRL_PCIEPLL_CTRL0,	.def = 0x340c04c8 },
	{ .reg = ANACTRL_PCIEPLL_CTRL0,	.def = 0x140c04c8, .delay_us = 10 },
	{ .reg = ANACTRL_PCIEPLL_CTRL2,	.def = 0x00001000 }
};

#ifdef CONFIG_ARM64
/* Keep a single entry table for recalc/round_rate() ops */
static const struct pll_params_table t7_pcie_pll_table[] = {
	PLL_PARAMS(150, 1),
	{0, 0}
};
#else
static const struct pll_params_table t7_pcie_pll_table[] = {
	PLL_PARAMS(150, 1, 0),
	{0, 0, 0}
};
#endif

static struct clk_regmap t7_pcie_pll_dco = {
	.data = &(struct meson_clk_pll_data){
		.en = {
			.reg_off = ANACTRL_PCIEPLL_CTRL0,
			.shift   = 28,
			.width   = 1,
		},
		.m = {
			.reg_off = ANACTRL_PCIEPLL_CTRL0,
			.shift   = 0,
			.width   = 8,
		},
		.n = {
			.reg_off = ANACTRL_PCIEPLL_CTRL0,
			.shift   = 10,
			.width   = 5,
		},
		.frac = {
			.reg_off = ANACTRL_PCIEPLL_CTRL1,
			.shift   = 0,
			.width   = 12,
		},
		.l = {
			.reg_off = ANACTRL_PCIEPLL_CTRL0,
			.shift   = 31,
			.width   = 1,
		},
		.rst = {
			.reg_off = ANACTRL_PCIEPLL_CTRL0,
			.shift   = 29,
			.width   = 1,
		},
		.table = t7_pcie_pll_table,
		.init_regs = t7_pcie_pll_init_regs,
		.init_count = ARRAY_SIZE(t7_pcie_pll_init_regs),
	},
	.hw.init = &(struct clk_init_data){
		.name = "pcie_pll_dco",
		.ops = &meson_clk_pcie_pll_ops,
		.parent_names = (const char *[]){ "xtal" },
		.num_parents = 1,
	},
};

static struct clk_fixed_factor t7_pcie_pll_dco_div2 = {
	.mult = 1,
	.div = 2,
	.hw.init = &(struct clk_init_data){
		.name = "pcie_pll_dco_div2",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_pcie_pll_dco.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_pcie_pll_od = {
	.data = &(struct clk_regmap_div_data){
		.offset = ANACTRL_PCIEPLL_CTRL0,
		.shift = 16,
		.width = 5,
		.flags = CLK_DIVIDER_ROUND_CLOSEST |
			 CLK_DIVIDER_ONE_BASED |
			 CLK_DIVIDER_ALLOW_ZERO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pcie_pll_od",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_pcie_pll_dco_div2.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_fixed_factor t7_pcie_pll = {
	.mult = 1,
	.div = 2,
	.hw.init = &(struct clk_init_data){
		.name = "pcie_pll",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_pcie_pll_od.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_pcie_hcsl = {
	.data = &(struct clk_regmap_gate_data){
		.offset = ANACTRL_PCIEPLL_CTRL5,
		.bit_idx = 3,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pcie_hcsl",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &t7_pcie_pll.hw },
		.num_parents = 1,
	},
};

static struct clk_regmap t7_hdmi_pll_dco = {
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
		.parent_names = (const char *[]){ "xtal" },
		.num_parents = 1,
		/*
		 * Display directly handle hdmi pll registers ATM, we need
		 * NOCACHE to keep our view of the clock as accurate as
		 * possible
		 */
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap t7_hdmi_pll_od = {
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
			&t7_hdmi_pll_dco.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_hdmi_pll = {
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
			&t7_hdmi_pll_od.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_fixed_factor t7_mpll_50m_div = {
	.mult = 1,
	.div = 80,
	.hw.init = &(struct clk_init_data){
		.name = "mpll_50m_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_fixed_pll_dco.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap t7_mpll_50m = {
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
			{ .hw = &t7_mpll_50m_div.hw },
		},
		.num_parents = 2,
	},
};

static struct clk_fixed_factor t7_mpll_prediv = {
	.mult = 1,
	.div = 2,
	.hw.init = &(struct clk_init_data){
		.name = "mpll_prediv",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_fixed_pll_dco.hw
		},
		.num_parents = 1,
	},
};

static const struct reg_sequence t7_mpll0_init_regs[] = {
	{ .reg = ANACTRL_MPLL_CTRL2, .def = 0x40000033 }
};

static struct clk_regmap t7_mpll0_div = {
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
		.init_regs = t7_mpll0_init_regs,
		.init_count = ARRAY_SIZE(t7_mpll0_init_regs),
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpll0_div",
		.ops = &meson_clk_mpll_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_mpll_prediv.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap t7_mpll0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = ANACTRL_MPLL_CTRL1,
		.bit_idx = 31,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpll0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &t7_mpll0_div.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct reg_sequence t7_mpll1_init_regs[] = {
	{ .reg = ANACTRL_MPLL_CTRL4,	.def = 0x40000033 }
};

static struct clk_regmap t7_mpll1_div = {
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
		.init_regs = t7_mpll1_init_regs,
		.init_count = ARRAY_SIZE(t7_mpll1_init_regs),
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpll1_div",
		.ops = &meson_clk_mpll_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_mpll_prediv.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap t7_mpll1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = ANACTRL_MPLL_CTRL3,
		.bit_idx = 31,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpll1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &t7_mpll1_div.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct reg_sequence t7_mpll2_init_regs[] = {
	{ .reg = ANACTRL_MPLL_CTRL6, .def = 0x40000033 }
};

static struct clk_regmap t7_mpll2_div = {
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
		.init_regs = t7_mpll2_init_regs,
		.init_count = ARRAY_SIZE(t7_mpll2_init_regs),
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpll2_div",
		.ops = &meson_clk_mpll_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_mpll_prediv.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap t7_mpll2 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = ANACTRL_MPLL_CTRL5,
		.bit_idx = 31,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpll2",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &t7_mpll2_div.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct reg_sequence t7_mpll3_init_regs[] = {
	{ .reg = ANACTRL_MPLL_CTRL8, .def = 0x40000033 }
};

static struct clk_regmap t7_mpll3_div = {
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
		.init_regs = t7_mpll3_init_regs,
		.init_count = ARRAY_SIZE(t7_mpll3_init_regs),
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpll3_div",
		.ops = &meson_clk_mpll_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_mpll_prediv.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap t7_mpll3 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = ANACTRL_MPLL_CTRL7,
		.bit_idx = 31,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpll3",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &t7_mpll3_div.hw },
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
 *	         output N1 and N2 in rurn.
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
static struct clk_regmap t7_rtc_32k_clkin = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_RTC_BY_OSCIN_CTRL0,
		.bit_idx = 31,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "rtc_32k_clkin",
		.ops = &clk_regmap_gate_ops,
		.parent_names = (const char *[]){ "xtal" },
		.num_parents = 1,
	},
};

static const struct meson_clk_dualdiv_param t7_32k_div_table[] = {
	{
		.dual	= 1,
		.n1	= 733,
		.m1	= 8,
		.n2	= 732,
		.m2	= 11,
	},
	{}
};

static struct clk_regmap t7_rtc_32k_div = {
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
		.table = t7_32k_div_table,
	},
	.hw.init = &(struct clk_init_data){
		.name = "rtc_32k_div",
		.ops = &meson_clk_dualdiv_ops,
		.parent_names = (const char *[]){ "rtc_32k_clkin" },
		.num_parents = 1,
	},
};

static struct clk_regmap t7_rtc_32k_xtal = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_RTC_BY_OSCIN_CTRL1,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "rtc_32k_xtal",
		.ops = &clk_regmap_gate_ops,
		.parent_names = (const char *[]){ "rtc_32k_clkin" },
		.num_parents = 1,
	},
};

/*
 * three parent for rtc clock out
 * pad is from where?
 */
static u32 rtc_32k_sel[] = {0, 1};
static const char * const rtc_32k_sel_parent_names[] = {
	"rtc_32k_xtal", "rtc_32k_div", "pad"
};

static struct clk_regmap t7_rtc_32k_sel = {
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
		.parent_names = rtc_32k_sel_parent_names,
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_rtc_clk = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_RTC_BY_OSCIN_CTRL0,
		.bit_idx = 30,
	},
	.hw.init = &(struct clk_init_data){
		.name = "rtc_clk",
		.ops = &clk_regmap_gate_ops,
		.parent_names = (const char *[]){ "rtc_32k_sel" },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/* sys clk */
static u32 mux_table_sys_ab_clk_sel[] = { 0, 1, 2, 3, 4, 5, 7 };

static const char * const sys_ab_clk_parent_names[] = {
	"xtal", "fclk_div2", "fclk_div3", "fclk_div4",
	"fclk_div5", "axi_clk_frcpu", "rtc_clk"
};

static struct clk_regmap t7_sysclk_b_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_SYS_CLK_CTRL0,
		.mask = 0x7,
		.shift = 26,
		.table = mux_table_sys_ab_clk_sel,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sysclk_b_sel",
		.ops = &clk_regmap_mux_ro_ops,
		.parent_names = sys_ab_clk_parent_names,
		.num_parents = ARRAY_SIZE(sys_ab_clk_parent_names),
	},
};

static struct clk_regmap t7_sysclk_b_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_SYS_CLK_CTRL0,
		.shift = 16,
		.width = 10,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sysclk_b_div",
		.ops = &clk_regmap_divider_ro_ops,
		.parent_names = (const char *[]){ "sysclk_b_sel" },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_sysclk_b = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_SYS_CLK_CTRL0,
		.bit_idx = 29,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sysclk_b",
		.ops = &clk_regmap_gate_ro_ops,
		.parent_names = (const char *[]){ "sysclk_b_div" },
		.num_parents = 1,
	},
};

static struct clk_regmap t7_sysclk_a_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_SYS_CLK_CTRL0,
		.mask = 0x7,
		.shift = 10,
		.table = mux_table_sys_ab_clk_sel,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sysclk_a_sel",
		.ops = &clk_regmap_mux_ro_ops,
		.parent_names = sys_ab_clk_parent_names,
		.num_parents = ARRAY_SIZE(sys_ab_clk_parent_names),
	},
};

static struct clk_regmap t7_sysclk_a_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_SYS_CLK_CTRL0,
		.shift = 0,
		.width = 10,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sysclk_a_div",
		.ops = &clk_regmap_divider_ro_ops,
		.parent_names = (const char *[]){ "sysclk_a_sel" },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_sysclk_a = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_SYS_CLK_CTRL0,
		.bit_idx = 13,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sysclk_a",
		.ops = &clk_regmap_gate_ro_ops,
		.parent_names = (const char *[]){ "sysclk_a_div" },
		.num_parents = 1,
	},
};

static const char * const sys_clk_parent_names[] = {
	"sysclk_a", "sysclk_b"};

static struct clk_regmap t7_sys_clk = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_SYS_CLK_CTRL0,
		.mask = 0x1,
		.shift = 15,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sys_clk",
		.ops = &clk_regmap_mux_ro_ops,
		.parent_names = sys_clk_parent_names,
		.num_parents = ARRAY_SIZE(sys_clk_parent_names),
	},
};

/*axi clk*/

/*ceca_clk*/
static struct clk_regmap t7_ceca_32k_clkin = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_CECA_CTRL0,
		.bit_idx = 31,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "ceca_32k_clkin",
		.ops = &clk_regmap_gate_ops,
		.parent_names = (const char *[]){ "xtal" },
		.num_parents = 1,
	},
};

static struct clk_regmap t7_ceca_32k_div = {
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
		.table = t7_32k_div_table,
	},
	.hw.init = &(struct clk_init_data){
		.name = "ceca_32k_div",
		.ops = &meson_clk_dualdiv_ops,
		.parent_names = (const char *[]){ "ceca_32k_clkin" },
		.num_parents = 1,
	},
};

static struct clk_regmap t7_ceca_32k_sel_pre = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_CECA_CTRL1,
		.mask = 0x1,
		.shift = 24,
		.flags = CLK_MUX_ROUND_CLOSEST,
	},
	.hw.init = &(struct clk_init_data){
		.name = "ceca_32k_sel_pre",
		.ops = &clk_regmap_mux_ops,
		.parent_names = (const char *[]){ "ceca_32k_div",
						"ceca_32k_clkin" },
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_ceca_32k_sel = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_CECA_CTRL1,
		.mask = 0x1,
		.shift = 31,
		.flags = CLK_MUX_ROUND_CLOSEST,
	},
	.hw.init = &(struct clk_init_data){
		.name = "ceca_32k_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_names = (const char *[]){ "ceca_32k_sel_pre",
						"rtc_clk" },
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_ceca_32k_clkout = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_CECA_CTRL0,
		.bit_idx = 30,
	},
	.hw.init = &(struct clk_init_data){
		.name = "ceca_32k_clkout",
		.ops = &clk_regmap_gate_ops,
		.parent_names = (const char *[]){ "ceca_32k_sel" },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/*cecb_clk*/
static struct clk_regmap t7_cecb_32k_clkin = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_CECB_CTRL0,
		.bit_idx = 31,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "cecb_32k_clkin",
		.ops = &clk_regmap_gate_ops,
		.parent_names = (const char *[]){ "xtal" },
		.num_parents = 1,
	},
};

static struct clk_regmap t7_cecb_32k_div = {
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
		.table = t7_32k_div_table,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cecb_32k_div",
		.ops = &meson_clk_dualdiv_ops,
		.parent_names = (const char *[]){ "cecb_32k_clkin" },
		.num_parents = 1,
	},
};

static struct clk_regmap t7_cecb_32k_sel_pre = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_CECB_CTRL1,
		.mask = 0x1,
		.shift = 24,
		.flags = CLK_MUX_ROUND_CLOSEST,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cecb_32k_sel_pre",
		.ops = &clk_regmap_mux_ops,
		.parent_names = (const char *[]){ "cecb_32k_div",
						"cecb_32k_clkin" },
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_cecb_32k_sel = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_CECB_CTRL1,
		.mask = 0x1,
		.shift = 31,
		.flags = CLK_MUX_ROUND_CLOSEST,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cecb_32k_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_names = (const char *[]){ "cecb_32k_sel_pre",
						"rtc_clk" },
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_cecb_32k_clkout = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_CECB_CTRL0,
		.bit_idx = 30,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cecb_32k_clkout",
		.ops = &clk_regmap_gate_ops,
		.parent_names = (const char *[]){ "cecb_32k_sel" },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data t7_sc_parent_data[] = {
	{ .hw = &t7_fclk_div4.hw },
	{ .hw = &t7_fclk_div3.hw },
	{ .hw = &t7_fclk_div5.hw },
	{ .fw_name = "xtal", }
};

static struct clk_regmap t7_sc_clk_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_SC_CLK_CTRL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sc_clk_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t7_sc_parent_data,
		.num_parents = ARRAY_SIZE(t7_sc_parent_data),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap t7_sc_clk_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_SC_CLK_CTRL,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sc_clk_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_sc_clk_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_sc_clk_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_SC_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sc_clk_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_sc_clk_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

/*rama_clk*/

/*dspa_clk*/
static const struct clk_parent_data t7_dsp_parent_hws[] = {
	{ .fw_name = "xtal", },
	{ .hw = &t7_fclk_div2p5.hw },
	{ .hw = &t7_fclk_div3.hw },
	{ .hw = &t7_fclk_div5.hw },
	{ .hw = &t7_hifi_pll.hw },
	{ .hw = &t7_fclk_div4.hw },
	{ .hw = &t7_fclk_div7.hw },
	{ .hw = &t7_rtc_clk.hw }
};

static struct clk_regmap t7_dspa_a_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_DSPA_CLK_CTRL0,
		.mask = 0x7,
		.shift = 10,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "dspa_a_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t7_dsp_parent_hws,
		.num_parents = ARRAY_SIZE(t7_dsp_parent_hws),
	},
};

static struct clk_regmap t7_dspa_a_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_DSPA_CLK_CTRL0,
		.shift = 0,
		.width = 10,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "dspa_a_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_dspa_a_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_dspa_a_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_DSPA_CLK_CTRL0,
		.bit_idx = 13,
	},
	.hw.init = &(struct clk_init_data){
		.name = "dspa_a_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_dspa_a_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_dspa_b_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_DSPA_CLK_CTRL0,
		.mask = 0x7,
		.shift = 26,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "dspa_b_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t7_dsp_parent_hws,
		.num_parents = ARRAY_SIZE(t7_dsp_parent_hws),
	},
};

static struct clk_regmap t7_dspa_b_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_DSPA_CLK_CTRL0,
		.shift = 16,
		.width = 10,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "dspa_b_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_dspa_b_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_dspa_b_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_DSPA_CLK_CTRL0,
		.bit_idx = 29,
	},
	.hw.init = &(struct clk_init_data){
		.name = "dspa_b_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_dspa_b_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_dspa_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_DSPA_CLK_CTRL0,
		.mask = 0x1,
		.shift = 15,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "dspa_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_dspa_a_gate.hw,
			&t7_dspa_b_gate.hw,
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_dspb_a_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_DSPB_CLK_CTRL0,
		.mask = 0x7,
		.shift = 10,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "dspb_a_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t7_dsp_parent_hws,
		.num_parents = ARRAY_SIZE(t7_dsp_parent_hws),
	},
};

static struct clk_regmap t7_dspb_a_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_DSPB_CLK_CTRL0,
		.shift = 0,
		.width = 10,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "dspb_a_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_dspb_a_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_dspb_a_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_DSPB_CLK_CTRL0,
		.bit_idx = 13,
	},
	.hw.init = &(struct clk_init_data){
		.name = "dspb_a_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_dspb_a_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_dspb_b_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_DSPB_CLK_CTRL0,
		.mask = 0x7,
		.shift = 26,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "dspb_b_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t7_dsp_parent_hws,
		.num_parents = ARRAY_SIZE(t7_dsp_parent_hws),
	},
};

static struct clk_regmap t7_dspb_b_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_DSPB_CLK_CTRL0,
		.shift = 16,
		.width = 10,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "dspb_b_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_dspb_b_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_dspb_b_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_DSPB_CLK_CTRL0,
		.bit_idx = 29,
	},
	.hw.init = &(struct clk_init_data){
		.name = "dspb_b_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_dspb_b_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_dspb_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_DSPB_CLK_CTRL0,
		.mask = 0x1,
		.shift = 15,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "dspb_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_dspb_a_gate.hw,
			&t7_dspb_b_gate.hw,
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/*12_24M clk*/
static struct clk_regmap t7_24M_clk_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_CLK12_24_CTRL,
		.bit_idx = 11,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "24m",
		.ops = &clk_regmap_gate_ops,
		.parent_names = (const char *[]){ "xtal" },
		.num_parents = 1,
	},
};

static struct clk_fixed_factor t7_12M_clk_div = {
	.mult = 1,
	.div = 2,
	.hw.init = &(struct clk_init_data){
		.name = "24m_div2",
		.ops = &clk_fixed_factor_ops,
		.parent_names = (const char *[]){ "24m" },
		.num_parents = 1,
	},
};

static struct clk_regmap t7_12M_clk_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_CLK12_24_CTRL,
		.bit_idx = 10,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "12m",
		.ops = &clk_regmap_gate_ops,
		.parent_names = (const char *[]){ "24m_div2" },
		.num_parents = 1,
	},
};

static struct clk_regmap t7_25M_clk_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_CLK12_24_CTRL,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "25M_clk_div",
		.ops = &clk_regmap_divider_ops,
		.parent_names = (const char *[]){ "fclk_div2" },
		.num_parents = 1,
	},
};

static struct clk_regmap t7_25M_clk_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_CLK12_24_CTRL,
		.bit_idx = 12,
	},
	.hw.init = &(struct clk_init_data){
		.name = "25m",
		.ops = &clk_regmap_gate_ops,
		.parent_names = (const char *[]){ "25M_clk_div" },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/* Video Clocks */
static struct clk_regmap t7_vid_pll_div = {
	.data = &(struct meson_vid_pll_div_data){
		.val = {
			.reg_off = CLKCTRL_VID_PLL_CLK0_DIV,
			.shift   = 0,
			.width   = 15,
		},
		.sel = {
			.reg_off = CLKCTRL_VID_PLL_CLK0_DIV,
			.shift   = 16,
			.width   = 2,
		},
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vid_pll_div",
		.ops = &meson_vid_pll_div_ro_ops,
		.parent_hws = (const struct clk_hw *[]) { &t7_hdmi_pll.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
	},
};

static const struct clk_hw *t7_vid_pll_parent_hws[] = {
	&t7_vid_pll_div.hw,
	&t7_hdmi_pll.hw
};

static struct clk_regmap t7_vid_pll_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VID_PLL_CLK0_DIV,
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
		.parent_hws = t7_vid_pll_parent_hws,
		.num_parents = ARRAY_SIZE(t7_vid_pll_parent_hws),
		.flags = CLK_SET_RATE_NO_REPARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap t7_vid_pll = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VID_PLL_CLK0_DIV,
		.bit_idx = 19,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vid_pll",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_vid_pll_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static const struct clk_hw *t7_vclk_parent_hws[] = {
	&t7_vid_pll.hw,
	&t7_gp0_pll.hw,
	&t7_hifi_pll.hw,
	&t7_mpll1.hw,
	&t7_fclk_div3.hw,
	&t7_fclk_div4.hw,
	&t7_fclk_div5.hw,
	&t7_fclk_div7.hw
};

static struct clk_regmap t7_vclk_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VID_CLK0_CTRL,
		.mask = 0x7,
		.shift = 16,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vclk_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = t7_vclk_parent_hws,
		.num_parents = ARRAY_SIZE(t7_vclk_parent_hws),
		.flags = CLK_SET_RATE_NO_REPARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap t7_vclk2_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VIID_CLK0_CTRL,
		.mask = 0x7,
		.shift = 16,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vclk2_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = t7_vclk_parent_hws,
		.num_parents = ARRAY_SIZE(t7_vclk_parent_hws),
		.flags = CLK_SET_RATE_NO_REPARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap t7_vclk_input = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VID_CLK0_DIV,
		.bit_idx = 16,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk_input",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &t7_vclk_sel.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap t7_vclk2_input = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VIID_CLK0_DIV,
		.bit_idx = 16,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk2_input",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &t7_vclk2_sel.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap t7_vclk_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_VID_CLK0_DIV,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vclk_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_vclk_input.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap t7_vclk2_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_VIID_CLK0_DIV,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vclk2_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_vclk2_input.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap t7_vclk = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VID_CLK0_CTRL,
		.bit_idx = 19,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &t7_vclk_div.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap t7_vclk2 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VIID_CLK0_CTRL,
		.bit_idx = 19,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk2",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &t7_vclk2_div.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap t7_vclk_div1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VID_CLK0_CTRL,
		.bit_idx = 0,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk_div1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &t7_vclk.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap t7_vclk_div2_en = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VID_CLK0_CTRL,
		.bit_idx = 1,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk_div2_en",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &t7_vclk.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap t7_vclk_div4_en = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VID_CLK0_CTRL,
		.bit_idx = 2,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk_div4_en",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &t7_vclk.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap t7_vclk_div6_en = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VID_CLK0_CTRL,
		.bit_idx = 3,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk_div6_en",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &t7_vclk.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap t7_vclk_div12_en = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VID_CLK0_CTRL,
		.bit_idx = 4,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk_div12_en",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &t7_vclk.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap t7_vclk2_div1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VIID_CLK0_CTRL,
		.bit_idx = 0,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk2_div1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &t7_vclk2.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap t7_vclk2_div2_en = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VIID_CLK0_CTRL,
		.bit_idx = 1,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk2_div2_en",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &t7_vclk2.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap t7_vclk2_div4_en = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VIID_CLK0_CTRL,
		.bit_idx = 2,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk2_div4_en",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &t7_vclk2.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap t7_vclk2_div6_en = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VIID_CLK0_CTRL,
		.bit_idx = 3,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk2_div6_en",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &t7_vclk2.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap t7_vclk2_div12_en = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VIID_CLK0_CTRL,
		.bit_idx = 4,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk2_div12_en",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &t7_vclk2.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_fixed_factor t7_vclk_div2 = {
	.mult = 1,
	.div = 2,
	.hw.init = &(struct clk_init_data){
		.name = "vclk_div2",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_vclk_div2_en.hw
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor t7_vclk_div4 = {
	.mult = 1,
	.div = 4,
	.hw.init = &(struct clk_init_data){
		.name = "vclk_div4",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_vclk_div4_en.hw
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor t7_vclk_div6 = {
	.mult = 1,
	.div = 6,
	.hw.init = &(struct clk_init_data){
		.name = "vclk_div6",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_vclk_div6_en.hw
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor t7_vclk_div12 = {
	.mult = 1,
	.div = 12,
	.hw.init = &(struct clk_init_data){
		.name = "vclk_div12",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_vclk_div12_en.hw
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor t7_vclk2_div2 = {
	.mult = 1,
	.div = 2,
	.hw.init = &(struct clk_init_data){
		.name = "vclk2_div2",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_vclk2_div2_en.hw
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor t7_vclk2_div4 = {
	.mult = 1,
	.div = 4,
	.hw.init = &(struct clk_init_data){
		.name = "vclk2_div4",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_vclk2_div4_en.hw
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor t7_vclk2_div6 = {
	.mult = 1,
	.div = 6,
	.hw.init = &(struct clk_init_data){
		.name = "vclk2_div6",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_vclk2_div6_en.hw
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor t7_vclk2_div12 = {
	.mult = 1,
	.div = 12,
	.hw.init = &(struct clk_init_data){
		.name = "vclk2_div12",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_vclk2_div12_en.hw
		},
		.num_parents = 1,
	},
};

static u32 mux_table_cts_sel[] = { 0, 1, 2, 3, 4, 8, 9, 10, 11, 12 };
static const struct clk_hw *t7_cts_parent_hws[] = {
	&t7_vclk_div1.hw,
	&t7_vclk_div2.hw,
	&t7_vclk_div4.hw,
	&t7_vclk_div6.hw,
	&t7_vclk_div12.hw,
	&t7_vclk2_div1.hw,
	&t7_vclk2_div2.hw,
	&t7_vclk2_div4.hw,
	&t7_vclk2_div6.hw,
	&t7_vclk2_div12.hw
};

static struct clk_regmap t7_cts_enci_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VID_CLK0_DIV,
		.mask = 0xf,
		.shift = 28,
		.table = mux_table_cts_sel,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cts_enci_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = t7_cts_parent_hws,
		.num_parents = ARRAY_SIZE(t7_cts_parent_hws),
		.flags = CLK_SET_RATE_NO_REPARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap t7_cts_encp_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VID_CLK1_DIV,
		.mask = 0xf,
		.shift = 20,
		.table = mux_table_cts_sel,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cts_encp_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = t7_cts_parent_hws,
		.num_parents = ARRAY_SIZE(t7_cts_parent_hws),
		.flags = CLK_SET_RATE_NO_REPARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap t7_cts_vdac_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VIID_CLK1_DIV,
		.mask = 0xf,
		.shift = 28,
		.table = mux_table_cts_sel,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cts_vdac_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = t7_cts_parent_hws,
		.num_parents = ARRAY_SIZE(t7_cts_parent_hws),
		.flags = CLK_SET_RATE_NO_REPARENT | CLK_GET_RATE_NOCACHE,
	},
};

/* TOFIX: add support for cts_tcon */
static u32 mux_table_hdmi_tx_sel[] = { 0, 1, 2, 3, 4, 8, 9, 10, 11, 12 };
static const struct clk_hw *t7_cts_hdmi_tx_parent_hws[] = {
	&t7_vclk_div1.hw,
	&t7_vclk_div2.hw,
	&t7_vclk_div4.hw,
	&t7_vclk_div6.hw,
	&t7_vclk_div12.hw,
	&t7_vclk2_div1.hw,
	&t7_vclk2_div2.hw,
	&t7_vclk2_div4.hw,
	&t7_vclk2_div6.hw,
	&t7_vclk2_div12.hw
};

static struct clk_regmap t7_hdmi_tx_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_HDMI_CLK_CTRL,
		.mask = 0xf,
		.shift = 16,
		.table = mux_table_hdmi_tx_sel,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmi_tx_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = t7_cts_hdmi_tx_parent_hws,
		.num_parents = ARRAY_SIZE(t7_cts_hdmi_tx_parent_hws),
		.flags = CLK_SET_RATE_NO_REPARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap t7_cts_enci = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VID_CLK2_CTRL2,
		.bit_idx = 0,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "cts_enci",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_cts_enci_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap t7_cts_encp = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VID_CLK2_CTRL2,
		.bit_idx = 2,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "cts_encp",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_cts_encp_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap t7_cts_vdac = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VID_CLK2_CTRL2,
		.bit_idx = 4,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "cts_vdac",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_cts_vdac_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap t7_hdmi_tx = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VID_CLK2_CTRL2,
		.bit_idx = 5,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmi_tx",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_hdmi_tx_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static const struct clk_parent_data t7_hdmitx_sys_parent_data[] = {
	{ .fw_name = "xtal", },
	{ .hw = &t7_fclk_div4.hw },
	{ .hw = &t7_fclk_div3.hw },
	{ .hw = &t7_fclk_div5.hw }
};

static struct clk_regmap t7_hdmitx_sys_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_HDMI_CLK_CTRL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmitx_sys_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t7_hdmitx_sys_parent_data,
		.num_parents = ARRAY_SIZE(t7_hdmitx_sys_parent_data),
	},
};

static struct clk_regmap t7_hdmitx_sys_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_HDMI_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmitx_sys_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) { &t7_hdmitx_sys_sel.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t7_hdmitx_sys = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_HDMI_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmitx_sys",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &t7_hdmitx_sys.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t7_hdmitx_prif_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_HTX_CLK_CTRL0,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmitx_prif_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t7_hdmitx_sys_parent_data,
		.num_parents = ARRAY_SIZE(t7_hdmitx_sys_parent_data),
	},
};

static struct clk_regmap t7_hdmitx_prif_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_HTX_CLK_CTRL0,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmitx_prif_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) { &t7_hdmitx_prif_sel.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t7_hdmitx_prif = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_HTX_CLK_CTRL0,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmitx_prif",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &t7_hdmitx_prif.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t7_hdmitx_200m_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_HTX_CLK_CTRL0,
		.mask = 0x3,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmitx_200m_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t7_hdmitx_sys_parent_data,
		.num_parents = ARRAY_SIZE(t7_hdmitx_sys_parent_data),
	},
};

static struct clk_regmap t7_hdmitx_200m_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_HTX_CLK_CTRL0,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmitx_200m_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) { &t7_hdmitx_200m_sel.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t7_hdmitx_200m = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_HTX_CLK_CTRL0,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmitx_200m",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &t7_hdmitx_200m.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t7_hdmitx_aud_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_HTX_CLK_CTRL1,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmitx_aud_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t7_hdmitx_sys_parent_data,
		.num_parents = ARRAY_SIZE(t7_hdmitx_sys_parent_data),
	},
};

static struct clk_regmap t7_hdmitx_aud_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_HTX_CLK_CTRL1,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmitx_aud_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) { &t7_hdmitx_aud_sel.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t7_hdmitx_aud  = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_HTX_CLK_CTRL1,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmitx_aud",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &t7_hdmitx_aud_sel.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t7_hdmirx_5m_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_HRX_CLK_CTRL0,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmirx_5m_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t7_hdmitx_sys_parent_data,
		.num_parents = ARRAY_SIZE(t7_hdmitx_sys_parent_data),
	},
};

static struct clk_regmap t7_hdmirx_5m_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_HRX_CLK_CTRL0,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmirx_5m_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) { &t7_hdmirx_5m_sel.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t7_hdmirx_5m  = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_HRX_CLK_CTRL0,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmirx_5m",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &t7_hdmirx_5m_div.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t7_hdmirx_2m_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_HRX_CLK_CTRL0,
		.mask = 0x3,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmirx_2m_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t7_hdmitx_sys_parent_data,
		.num_parents = ARRAY_SIZE(t7_hdmitx_sys_parent_data),
	},
};

static struct clk_regmap t7_hdmirx_2m_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_HRX_CLK_CTRL0,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmirx_2m_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) { &t7_hdmirx_2m_sel.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t7_hdmirx_2m = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_HRX_CLK_CTRL0,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmirx_2m",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &t7_hdmirx_2m_div.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t7_hdmirx_cfg_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_HRX_CLK_CTRL1,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmirx_cfg_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t7_hdmitx_sys_parent_data,
		.num_parents = ARRAY_SIZE(t7_hdmitx_sys_parent_data),
	},
};

static struct clk_regmap t7_hdmirx_cfg_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_HRX_CLK_CTRL1,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmirx_cfg_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) { &t7_hdmirx_cfg_sel.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t7_hdmirx_cfg  = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_HRX_CLK_CTRL1,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmirx_cfg",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &t7_hdmirx_cfg_div.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t7_hdmirx_hdcp_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_HRX_CLK_CTRL1,
		.mask = 0x3,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmirx_hdcp_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t7_hdmitx_sys_parent_data,
		.num_parents = ARRAY_SIZE(t7_hdmitx_sys_parent_data),
	},
};

static struct clk_regmap t7_hdmirx_hdcp_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_HRX_CLK_CTRL1,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmirx_hdcp_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) { &t7_hdmirx_hdcp_sel.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t7_hdmirx_hdcp = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_HRX_CLK_CTRL1,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmirx_hdcp",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &t7_hdmirx_hdcp_div.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t7_hdmirx_aud_pll_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_HRX_CLK_CTRL2,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmirx_aud_pll_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t7_hdmitx_sys_parent_data,
		.num_parents = ARRAY_SIZE(t7_hdmitx_sys_parent_data),
	},
};

static struct clk_regmap t7_hdmirx_aud_pll_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_HRX_CLK_CTRL2,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmirx_aud_pll_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) { &t7_hdmirx_aud_pll_sel.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t7_hdmirx_aud_pll  = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_HRX_CLK_CTRL2,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmirx_aud_pll",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &t7_hdmirx_aud_pll_div.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t7_hdmirx_acr_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_HRX_CLK_CTRL2,
		.mask = 0x3,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmirx_acr_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t7_hdmitx_sys_parent_data,
		.num_parents = ARRAY_SIZE(t7_hdmitx_sys_parent_data),
	},
};

static struct clk_regmap t7_hdmirx_acr_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_HRX_CLK_CTRL2,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmirx_acr_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) { &t7_hdmirx_acr_sel.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t7_hdmirx_acr = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_HRX_CLK_CTRL2,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmirx_acr",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &t7_hdmirx_acr_div.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t7_hdmirx_meter_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_HRX_CLK_CTRL3,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmirx_meter_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t7_hdmitx_sys_parent_data,
		.num_parents = ARRAY_SIZE(t7_hdmitx_sys_parent_data),
	},
};

static struct clk_regmap t7_hdmirx_meter_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_HRX_CLK_CTRL3,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmirx_meter_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) { &t7_hdmirx_meter_sel.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t7_hdmirx_meter  = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_HRX_CLK_CTRL3,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmirx_meter",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &t7_hdmirx_meter_div.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t7_vid_lock_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_VID_LOCK_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vid_lock_div",
		.ops = &clk_regmap_divider_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "xtal",
		},
		.num_parents = 1,
	},
};

static struct clk_regmap t7_vid_lock_clk  = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VID_LOCK_CLK_CTRL,
		.bit_idx = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vid_lock_clk",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &t7_vid_lock_div.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t7_ts_clk_div = {
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

static struct clk_regmap t7_ts_clk_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_TS_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "ts_clk_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_ts_clk_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
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
static u32 mux_table_mali[] = { 0, 3, 4, 5, 6};

static const struct clk_parent_data t7_mali_0_1_parent_data[] = {
	{ .fw_name = "xtal", },
	{ .hw = &t7_fclk_div2p5.hw },
	{ .hw = &t7_fclk_div3.hw },
	{ .hw = &t7_fclk_div4.hw },
	{ .hw = &t7_fclk_div5.hw },
};

static struct clk_regmap t7_mali_0_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_MALI_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
		.table = mux_table_mali,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mali_0_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t7_mali_0_1_parent_data,
		.num_parents = ARRAY_SIZE(t7_mali_0_1_parent_data),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap t7_mali_0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_MALI_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mali_0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_mali_0_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_mali_0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_MALI_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mali_0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_mali_0_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_GATE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_mali_1_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_MALI_CLK_CTRL,
		.mask = 0x7,
		.shift = 25,
		.table = mux_table_mali,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mali_1_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t7_mali_0_1_parent_data,
		.num_parents = ARRAY_SIZE(t7_mali_0_1_parent_data),
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_mali_1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_MALI_CLK_CTRL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mali_1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_mali_1_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_mali_1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_MALI_CLK_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mali_1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_mali_1_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_GATE | CLK_SET_RATE_PARENT,
	},
};

static const struct clk_hw *t7_mali_parent_hws[] = {
	&t7_mali_0.hw,
	&t7_mali_1.hw
};

static struct clk_regmap t7_mali_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_MALI_CLK_CTRL,
		.mask = 1,
		.shift = 31,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mali",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = t7_mali_parent_hws,
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/* cts_vdec_clk */
static const struct clk_parent_data t7_dec_parent_hws[] = {
	{ .hw = &t7_fclk_div2p5.hw },
	{ .hw = &t7_fclk_div3.hw },
	{ .hw = &t7_fclk_div4.hw },
	{ .hw = &t7_fclk_div5.hw },
	{ .hw = &t7_fclk_div7.hw },
	{ .hw = &t7_hifi_pll.hw },
	{ .hw = &t7_gp0_pll.hw },
	{ .fw_name = "xtal", }
};

static struct clk_regmap t7_vdec_p0_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VDEC_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vdec_p0_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t7_dec_parent_hws,
		.num_parents = ARRAY_SIZE(t7_dec_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap t7_vdec_p0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_VDEC_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vdec_p0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_vdec_p0_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_vdec_p0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VDEC_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vdec_p0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_vdec_p0_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_vdec_p1_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VDEC3_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vdec_p1_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t7_dec_parent_hws,
		.num_parents = ARRAY_SIZE(t7_dec_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap t7_vdec_p1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_VDEC3_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vdec_p1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_vdec_p1_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_vdec_p1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VDEC3_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vdec_p1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_vdec_p1_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_vdec_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VDEC3_CLK_CTRL,
		.mask = 0x1,
		.shift = 15,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vdec_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_vdec_p0.hw,
			&t7_vdec_p1.hw
		},
		.num_parents = 2,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_hcodec_p0_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VDEC_CLK_CTRL,
		.mask = 0x7,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hcodec_p0_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t7_dec_parent_hws,
		.num_parents = ARRAY_SIZE(t7_dec_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap t7_hcodec_p0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_VDEC_CLK_CTRL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hcodec_p0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_hcodec_p0_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_hcodec_p0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VDEC_CLK_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hcodec_p0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_hcodec_p0_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_hcodec_p1_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VDEC3_CLK_CTRL,
		.mask = 0x7,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hcodec_p1_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t7_dec_parent_hws,
		.num_parents = ARRAY_SIZE(t7_dec_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap t7_hcodec_p1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_VDEC3_CLK_CTRL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hcodec_p1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_hcodec_p1_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_hcodec_p1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VDEC3_CLK_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hcodec_p1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_hcodec_p1_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_hcodec_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VDEC3_CLK_CTRL,
		.mask = 0x1,
		.shift = 31,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hcodec_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_hcodec_p0.hw,
			&t7_hcodec_p1.hw
		},
		.num_parents = 2,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static u32 mux_table_vdec[] = { 0, 1, 2, 3, 4};

static const struct clk_hw *t7_vdec_parent_hws[] = {
	&t7_fclk_div2p5.hw,
	&t7_fclk_div3.hw,
	&t7_fclk_div4.hw,
	&t7_fclk_div5.hw,
	&t7_fclk_div7.hw
};

static struct clk_regmap t7_hevcb_p0_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VDEC2_CLK_CTRL,
		.mask = 0x7,
		.shift = 25,
		.flags = CLK_MUX_ROUND_CLOSEST,
		.table = mux_table_vdec,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hevcb_p0_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = t7_vdec_parent_hws,
		.num_parents = ARRAY_SIZE(t7_vdec_parent_hws),
	},
};

static struct clk_regmap t7_hevcb_p0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_VDEC2_CLK_CTRL,
		.shift = 16,
		.width = 7,
		.flags = CLK_DIVIDER_ROUND_CLOSEST,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hevcb_p0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_hevcb_p0_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_hevcb_p0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VDEC2_CLK_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hevcb_p0_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_hevcb_p0_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_hevcb_p1_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VDEC4_CLK_CTRL,
		.mask = 0x7,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hevcb_p1_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = t7_vdec_parent_hws,
		.num_parents = ARRAY_SIZE(t7_vdec_parent_hws),
	},
};

static struct clk_regmap t7_hevcb_p1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_VDEC4_CLK_CTRL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hevc_p1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_hevcb_p1_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_hevcb_p1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VDEC4_CLK_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hevcb_p1_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_hevcb_p1_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_hevcb_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VDEC4_CLK_CTRL,
		.mask = 0x1,
		.shift = 31,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hevcb_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_hevcb_p0.hw,
			&t7_hevcb_p1.hw
		},
		.num_parents = 2,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_hevcf_p0_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VDEC2_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hevcf_p0_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t7_dec_parent_hws,
		.num_parents = ARRAY_SIZE(t7_dec_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap t7_hevcf_p0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_VDEC2_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hevcf_p0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_hevcf_p0_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_hevcf_p0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VDEC2_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hevcf_p0_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_hevcf_p0_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_hevcf_p1_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VDEC4_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hevcf_p1_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t7_dec_parent_hws,
		.num_parents = ARRAY_SIZE(t7_dec_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap t7_hevcf_p1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_VDEC4_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hevcf_p1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_hevcf_p1_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_hevcf_p1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VDEC4_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hevcf_p1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_hevcf_p1_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_hevcf_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VDEC4_CLK_CTRL,
		.mask = 0x1,
		.shift = 15,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hevcf_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_hevcf_p0.hw,
			&t7_hevcf_p1.hw
		},
		.num_parents = 2,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

/*cts_wave420l_a/b/c_clk*/
static const struct clk_parent_data t7_wave_parent_data[] = {
	{ .fw_name = "xtal", },
	{ .hw = &t7_fclk_div4.hw },
	{ .hw = &t7_fclk_div3.hw },
	{ .hw = &t7_fclk_div5.hw },
	{ .hw = &t7_fclk_div7.hw },
	{ .hw = &t7_mpll2.hw },
	{ .hw = &t7_mpll3.hw },
	{ .hw = &t7_gp1_pll.hw }
};

static struct clk_regmap t7_wave_a_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_WAVE521_CLK_CTRL2,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "wave_a_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t7_wave_parent_data,
		.num_parents = ARRAY_SIZE(t7_wave_parent_data),
	},
};

static struct clk_regmap t7_wave_a_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_WAVE521_CLK_CTRL2,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "wave_a_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_wave_a_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_wave_aclk = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_WAVE521_CLK_CTRL2,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "wave_aclk",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_wave_a_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_wave_b_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_WAVE521_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "wave_b_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t7_wave_parent_data,
		.num_parents = ARRAY_SIZE(t7_wave_parent_data),
	},
};

static struct clk_regmap t7_wave_b_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_WAVE521_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "wave_b_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_wave_b_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_wave_bclk = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_WAVE521_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "wave_bclk",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_wave_b_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_wave_c_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_WAVE521_CLK_CTRL,
		.mask = 0x7,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data){
		.name = "wave_c_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t7_wave_parent_data,
		.num_parents = ARRAY_SIZE(t7_wave_parent_data),
	},
};

static struct clk_regmap t7_wave_c_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_WAVE521_CLK_CTRL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "wave_c_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_wave_c_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_wave_cclk = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_WAVE521_CLK_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "wave_cclk",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_wave_c_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_mipi_isp_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_MIPI_ISP_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mipi_isp_sel",
		.ops = &clk_regmap_mux_ops,
		/* Share parent with wave clk */
		.parent_data = t7_wave_parent_data,
		.num_parents = ARRAY_SIZE(t7_wave_parent_data),
	},
};

static struct clk_regmap t7_mipi_isp_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_MIPI_ISP_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mipi_isp_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_mipi_isp_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_mipi_isp = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_MIPI_ISP_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mipi_isp",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_mipi_isp_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data t7_mipi_csi_parent_data[] = {
	{ .fw_name = "xtal", },
	{ .hw = &t7_gp1_pll.hw },
	{ .hw = &t7_mpll1.hw },
	{ .hw = &t7_mpll2.hw },
	{ .hw = &t7_fclk_div3.hw },
	{ .hw = &t7_fclk_div4.hw },
	{ .hw = &t7_fclk_div5.hw },
	{ .hw = &t7_fclk_div7.hw }
};

static struct clk_regmap t7_mipi_csi_phy_sel0 = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_MIPI_CSI_PHY_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mipi_csi_phy_sel0",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t7_mipi_csi_parent_data,
		.num_parents = ARRAY_SIZE(t7_mipi_csi_parent_data),
	},
};

static struct clk_regmap t7_mipi_csi_phy_div0 = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_MIPI_CSI_PHY_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mipi_csi_phy_div0",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_mipi_csi_phy_sel0.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_mipi_csi_phy0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_MIPI_CSI_PHY_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mipi_csi_phy0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_mipi_csi_phy_div0.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_mipi_csi_phy_sel1 = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_MIPI_CSI_PHY_CLK_CTRL,
		.mask = 0x7,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mipi_csi_phy_sel1",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t7_mipi_csi_parent_data,
		.num_parents = ARRAY_SIZE(t7_mipi_csi_parent_data),
	},
};

static struct clk_regmap t7_mipi_csi_phy_div1 = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_MIPI_CSI_PHY_CLK_CTRL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mipi_csi_phy_div1",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_mipi_csi_phy_sel1.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_mipi_csi_phy1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_MIPI_CSI_PHY_CLK_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mipi_csi_phy1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_mipi_csi_phy_div1.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_mipi_csi_phy_clk = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_MIPI_CSI_PHY_CLK_CTRL,
		.mask = 0x1,
		.shift = 31,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mipi_csi_phy_clk",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_mipi_csi_phy0.hw,
			&t7_mipi_csi_phy1.hw
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct clk_hw *t7_vpu_parent_hws[] = {
	&t7_fclk_div3.hw,
	&t7_fclk_div4.hw,
	&t7_fclk_div5.hw,
	&t7_fclk_div7.hw
};

static struct clk_regmap t7_vpu_0_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VPU_CLK_CTRL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vpu_0_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = t7_vpu_parent_hws,
		.num_parents = ARRAY_SIZE(t7_vpu_parent_hws),
	},
};

static struct clk_regmap t7_vpu_0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_VPU_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vpu_0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) { &t7_vpu_0_sel.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_vpu_0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VPU_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &t7_vpu_0_div.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap t7_vpu_1_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VPU_CLK_CTRL,
		.mask = 0x3,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vpu_1_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = t7_vpu_parent_hws,
		.num_parents = ARRAY_SIZE(t7_vpu_parent_hws),
	},
};

static struct clk_regmap t7_vpu_1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_VPU_CLK_CTRL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vpu_1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) { &t7_vpu_1_sel.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_vpu_1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VPU_CLK_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &t7_vpu_1_div.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap t7_vpu = {
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
			&t7_vpu_0.hw,
			&t7_vpu_1.hw
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_NO_REPARENT,
	},
};

static const struct clk_hw *vpu_clkb_tmp_parent_hws[] = {
	&t7_vpu.hw,
	&t7_fclk_div4.hw,
	&t7_fclk_div5.hw,
	&t7_fclk_div7.hw
};

static struct clk_regmap t7_vpu_clkb_tmp_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VPU_CLKB_CTRL,
		.mask = 0x3,
		.shift = 20,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_clkb_tmp_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = vpu_clkb_tmp_parent_hws,
		.num_parents = ARRAY_SIZE(vpu_clkb_tmp_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap t7_vpu_clkb_tmp_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_VPU_CLKB_CTRL,
		.shift = 16,
		.width = 4,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_clkb_tmp_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_vpu_clkb_tmp_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_vpu_clkb_tmp = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VPU_CLKB_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vpu_clkb_tmp",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_vpu_clkb_tmp_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_vpu_clkb_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_VPU_CLKB_CTRL,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_clkb_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_vpu_clkb_tmp.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_vpu_clkb = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VPU_CLKB_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vpu_clkb",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_vpu_clkb_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static const struct clk_hw *vpu_clkc_parent_hws[] = {
	&t7_fclk_div4.hw,
	&t7_fclk_div3.hw,
	&t7_fclk_div5.hw,
	&t7_fclk_div7.hw
};

static struct clk_regmap t7_vpu_clkc_p0_mux  = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VPU_CLKC_CTRL,
		.mask = 0x3,
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

static struct clk_regmap t7_vpu_clkc_p0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_VPU_CLKC_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_clkc_p0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_vpu_clkc_p0_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_vpu_clkc_p0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VPU_CLKC_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vpu_clkc_p0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_vpu_clkc_p0_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_vpu_clkc_p1_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VPU_CLKC_CTRL,
		.mask = 0x3,
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

static struct clk_regmap t7_vpu_clkc_p1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_VPU_CLKC_CTRL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_clkc_p1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_vpu_clkc_p1_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_vpu_clkc_p1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VPU_CLKC_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vpu_clkc_p1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_vpu_clkc_p1_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_vpu_clkc_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VPU_CLKC_CTRL,
		.mask = 0x1,
		.shift = 31,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_clkc_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_vpu_clkc_p0.hw,
			&t7_vpu_clkc_p1.hw
		},
		.num_parents = 2,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static u32 t7_vapb_table[] = { 0, 1, 2, 3, 7};
static const struct clk_hw *t7_vapb_parent_hws[] = {
	&t7_fclk_div4.hw,
	&t7_fclk_div3.hw,
	&t7_fclk_div5.hw,
	&t7_fclk_div7.hw,
	&t7_fclk_div2p5.hw
};

static struct clk_regmap t7_vapb_0_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VAPBCLK_CTRL,
		.mask = 0x7,
		.shift = 9,
		.table = t7_vapb_table
	},
	.hw.init = &(struct clk_init_data){
		.name = "vapb_0_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = t7_vapb_parent_hws,
		.num_parents = ARRAY_SIZE(t7_vapb_parent_hws),
	},
};

static struct clk_regmap t7_vapb_0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_VAPBCLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vapb_0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_vapb_0_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_vapb_0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VAPBCLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vapb_0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_vapb_0_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap t7_vapb_1_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VAPBCLK_CTRL,
		.mask = 0x7,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vapb_1_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = t7_vapb_parent_hws,
		.num_parents = ARRAY_SIZE(t7_vapb_parent_hws),
		.flags = CLK_SET_RATE_NO_REPARENT,
	},
};

static struct clk_regmap t7_vapb_1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_VAPBCLK_CTRL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vapb_1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_vapb_1_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_vapb_1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VAPBCLK_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vapb_1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_vapb_1_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap t7_vapb = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VAPBCLK_CTRL,
		.mask = 1,
		.shift = 31,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vapb_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_vapb_0.hw,
			&t7_vapb_1.hw
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_NO_REPARENT | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_gdcclk_0_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_GDC_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
		.table = t7_vapb_table
	},
	.hw.init = &(struct clk_init_data){
		.name = "gdcclk_0_sel",
		.ops = &clk_regmap_mux_ops,
		/* Share parent with vapb clk */
		.parent_hws = t7_vapb_parent_hws,
		.num_parents = ARRAY_SIZE(t7_vapb_parent_hws),
	},
};

static struct clk_regmap t7_gdcclk_0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_GDC_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "gdcclk_0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_gdcclk_0_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_gdcclk_0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_GDC_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "gdcclk_0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &t7_gdcclk_0_div.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap t7_gdcclk_1_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_GDC_CLK_CTRL,
		.mask = 0x7,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data){
		.name = "gdcclk_1_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = t7_vapb_parent_hws,
		.num_parents = ARRAY_SIZE(t7_vapb_parent_hws),
		.flags = CLK_SET_RATE_NO_REPARENT,
	},
};

static struct clk_regmap t7_gdcclk_1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_GDC_CLK_CTRL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "gdcclk_1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_gdcclk_1_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_gdcclk_1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_GDC_CLK_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "gdcclk_1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_gdcclk_1_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap t7_gdcclk = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_GDC_CLK_CTRL,
		.mask = 1,
		.shift = 31,
	},
	.hw.init = &(struct clk_init_data){
		.name = "gdcclk_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_gdcclk_0.hw,
			&t7_gdcclk_1.hw
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_NO_REPARENT | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_gdc_clk = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_GDC_CLK_CTRL,
		.bit_idx = 30,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "gdc_clk",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_gdcclk.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap t7_dewarpclk_0_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_AMLGDC_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
		.table = t7_vapb_table
	},
	.hw.init = &(struct clk_init_data){
		.name = "dewarpclk_0_sel",
		.ops = &clk_regmap_mux_ops,
		/* Share parent with vapb clk */
		.parent_hws = t7_vapb_parent_hws,
		.num_parents = ARRAY_SIZE(t7_vapb_parent_hws),
	},
};

static struct clk_regmap t7_dewarpclk_0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_AMLGDC_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "dewarpclk_0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_dewarpclk_0_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_dewarpclk_0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_AMLGDC_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "dewarpclk_0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &t7_dewarpclk_0_div.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap t7_dewarpclk_1_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_AMLGDC_CLK_CTRL,
		.mask = 0x7,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data){
		.name = "dewarpclk_1_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = t7_vapb_parent_hws,
		.num_parents = ARRAY_SIZE(t7_vapb_parent_hws),
		.flags = CLK_SET_RATE_NO_REPARENT,
	},
};

static struct clk_regmap t7_dewarpclk_1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_AMLGDC_CLK_CTRL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "dewarpclk_1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_dewarpclk_1_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_dewarpclk_1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_AMLGDC_CLK_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "dewarpclk_1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_dewarpclk_1_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap t7_dewarpclk = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_AMLGDC_CLK_CTRL,
		.mask = 1,
		.shift = 31,
	},
	.hw.init = &(struct clk_init_data){
		.name = "dewarpclk_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_dewarpclk_0.hw,
			&t7_dewarpclk_1.hw
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_NO_REPARENT | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_dewarp_clk = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_AMLGDC_CLK_CTRL,
		.bit_idx = 30,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "dewarp_clk",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_dewarpclk.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static u32 t7_anakin_table[] = { 0, 1, 2, 3, 7};
static const struct clk_hw *t7_anakin_parent_hws[] = {
	&t7_fclk_div4.hw,
	&t7_fclk_div3.hw,
	&t7_fclk_div5.hw,
	&t7_fclk_div2.hw,
	&t7_fclk_div2p5.hw
};

static struct clk_regmap t7_anakin_0_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_ANAKIN_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
		.table = t7_anakin_table
	},
	.hw.init = &(struct clk_init_data){
		.name = "anakin_0_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = t7_anakin_parent_hws,
		.num_parents = ARRAY_SIZE(t7_anakin_parent_hws),
	},
};

static struct clk_regmap t7_anakin_0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_ANAKIN_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "anakin_0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_anakin_0_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_anakin_0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_ANAKIN_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "anakin_0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &t7_anakin_0_div.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_GATE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_anakin_1_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_ANAKIN_CLK_CTRL,
		.mask = 0x7,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data){
		.name = "anakin_1_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = t7_anakin_parent_hws,
		.num_parents = ARRAY_SIZE(t7_anakin_parent_hws),
	},
};

static struct clk_regmap t7_anakin_1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_ANAKIN_CLK_CTRL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "anakin_1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_anakin_1_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_anakin_1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_ANAKIN_CLK_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "anakin_1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_anakin_1_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_GATE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_anakin = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_ANAKIN_CLK_CTRL,
		.mask = 1,
		.shift = 31,
	},
	.hw.init = &(struct clk_init_data){
		.name = "anakin_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_anakin_0.hw,
			&t7_anakin_1.hw
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t7_anakin_clk = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_ANAKIN_CLK_CTRL,
		.bit_idx = 30,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "anakin_clk",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_anakin.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t7_ge2d_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VAPBCLK_CTRL,
		.bit_idx = 30,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "ge2d_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &t7_vapb.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/*cts_hdcp22_esmclk*/

/*cts_hdcp22_skpclk*/

/* cts_vdin_meas_clk */
static const struct clk_parent_data t7_vdin_parent_hws[] = {
	{ .fw_name = "xtal", },
	{ .hw = &t7_fclk_div4.hw },
	{ .hw = &t7_fclk_div3.hw },
	{ .hw = &t7_fclk_div5.hw },
	{ .hw = &t7_vid_pll.hw }
};

static struct clk_regmap t7_vdin_meas_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VDIN_MEAS_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vdin_meas_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t7_vdin_parent_hws,
		.num_parents = ARRAY_SIZE(t7_vdin_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap t7_vdin_meas_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_VDIN_MEAS_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vdin_meas_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_vdin_meas_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_vdin_meas_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VDIN_MEAS_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vdin_meas_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_vdin_meas_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data t7_sd_emmc_clk0_parent_data[] = {
	{ .fw_name = "xtal", },
	{ .hw = &t7_fclk_div2.hw },
	{ .hw = &t7_fclk_div3.hw },
	{ .hw = &t7_hifi_pll.hw },
	{ .hw = &t7_fclk_div2p5.hw },
	{ .hw = &t7_mpll2.hw },
	{ .hw = &t7_mpll3.hw },
	{ .hw = &t7_gp0_pll.hw }
};

static struct clk_regmap t7_sd_emmc_c_clk0_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_NAND_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_c_clk0_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t7_sd_emmc_clk0_parent_data,
		.num_parents = ARRAY_SIZE(t7_sd_emmc_clk0_parent_data),
		.flags = CLK_GET_RATE_NOCACHE
	},
};

static struct clk_regmap t7_sd_emmc_c_clk0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_NAND_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_c_clk0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_sd_emmc_c_clk0_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t7_sd_emmc_c_clk0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_NAND_CLK_CTRL,
		.bit_idx = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sd_emmc_c_clk0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_sd_emmc_c_clk0_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t7_sd_emmc_a_clk0_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_SD_EMMC_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_a_clk0_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t7_sd_emmc_clk0_parent_data,
		.num_parents = ARRAY_SIZE(t7_sd_emmc_clk0_parent_data),
		.flags = CLK_GET_RATE_NOCACHE
	},
};

static struct clk_regmap t7_sd_emmc_a_clk0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_SD_EMMC_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_a_clk0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_sd_emmc_a_clk0_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_sd_emmc_a_clk0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_SD_EMMC_CLK_CTRL,
		.bit_idx = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sd_emmc_a_clk0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_sd_emmc_a_clk0_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_sd_emmc_b_clk0_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_SD_EMMC_CLK_CTRL,
		.mask = 0x7,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_b_clk0_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t7_sd_emmc_clk0_parent_data,
		.num_parents = ARRAY_SIZE(t7_sd_emmc_clk0_parent_data),
		.flags = CLK_GET_RATE_NOCACHE
	},
};

static struct clk_regmap t7_sd_emmc_b_clk0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_SD_EMMC_CLK_CTRL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_b_clk0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_sd_emmc_b_clk0_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t7_sd_emmc_b_clk0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_SD_EMMC_CLK_CTRL,
		.bit_idx = 23,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sd_emmc_b_clk0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_sd_emmc_b_clk0_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE |	CLK_SET_RATE_PARENT
	},
};

/*cts_cdac_clk*/

static const struct clk_parent_data t7_spicc_parent_hws[] = {
	{ .fw_name = "xtal", },
	{ .hw = &t7_sys_clk.hw },
	{ .hw = &t7_fclk_div4.hw },
	{ .hw = &t7_fclk_div3.hw },
	{ .hw = &t7_fclk_div2.hw },
	{ .hw = &t7_fclk_div5.hw },
	{ .hw = &t7_fclk_div7.hw },
	{ .hw = &t7_gp1_pll.hw }
};

static struct clk_regmap t7_spicc0_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_SPICC_CLK_CTRL,
		.mask = 0x7,
		.shift = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spicc0_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t7_spicc_parent_hws,
		.num_parents = ARRAY_SIZE(t7_spicc_parent_hws),
	},
};

static struct clk_regmap t7_spicc0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_SPICC_CLK_CTRL,
		.shift = 0,
		.width = 6,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spicc0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_spicc0_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_spicc0_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_SPICC_CLK_CTRL,
		.bit_idx = 6,
	},
	.hw.init = &(struct clk_init_data){
		.name = "spicc0_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_spicc0_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_spicc1_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_SPICC_CLK_CTRL,
		.mask = 0x7,
		.shift = 23,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spicc1_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t7_spicc_parent_hws,
		.num_parents = ARRAY_SIZE(t7_spicc_parent_hws),
	},
};

static struct clk_regmap t7_spicc1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_SPICC_CLK_CTRL,
		.shift = 16,
		.width = 6,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spicc1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_spicc1_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_spicc1_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_SPICC_CLK_CTRL,
		.bit_idx = 22,
	},
	.hw.init = &(struct clk_init_data){
		.name = "spicc1_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_spicc1_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_spicc2_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_SPICC_CLK_CTRL1,
		.mask = 0x7,
		.shift = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spicc2_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t7_spicc_parent_hws,
		.num_parents = ARRAY_SIZE(t7_spicc_parent_hws),
	},
};

static struct clk_regmap t7_spicc2_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_SPICC_CLK_CTRL1,
		.shift = 0,
		.width = 6,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spicc2_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_spicc2_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_spicc2_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_SPICC_CLK_CTRL1,
		.bit_idx = 6,
	},
	.hw.init = &(struct clk_init_data){
		.name = "spicc2_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_spicc2_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_spicc3_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_SPICC_CLK_CTRL1,
		.mask = 0x7,
		.shift = 23,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spicc3_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t7_spicc_parent_hws,
		.num_parents = ARRAY_SIZE(t7_spicc_parent_hws),
	},
};

static struct clk_regmap t7_spicc3_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_SPICC_CLK_CTRL1,
		.shift = 16,
		.width = 6,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spicc3_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_spicc3_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_spicc3_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_SPICC_CLK_CTRL1,
		.bit_idx = 22,
	},
	.hw.init = &(struct clk_init_data){
		.name = "spicc3_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_spicc3_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_spicc4_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_SPICC_CLK_CTRL2,
		.mask = 0x7,
		.shift = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spicc4_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t7_spicc_parent_hws,
		.num_parents = ARRAY_SIZE(t7_spicc_parent_hws),
	},
};

static struct clk_regmap t7_spicc4_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_SPICC_CLK_CTRL2,
		.shift = 0,
		.width = 6,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spicc4_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_spicc4_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_spicc4_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_SPICC_CLK_CTRL2,
		.bit_idx = 6,
	},
	.hw.init = &(struct clk_init_data){
		.name = "spicc4_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_spicc4_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_spicc5_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_SPICC_CLK_CTRL2,
		.mask = 0x7,
		.shift = 23,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spicc5_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t7_spicc_parent_hws,
		.num_parents = ARRAY_SIZE(t7_spicc_parent_hws),
	},
};

static struct clk_regmap t7_spicc5_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_SPICC_CLK_CTRL2,
		.shift = 16,
		.width = 6,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spicc5_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_spicc5_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_spicc5_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_SPICC_CLK_CTRL2,
		.bit_idx = 22,
	},
	.hw.init = &(struct clk_init_data){
		.name = "spicc5_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_spicc5_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/*cts_bt656*/

static const struct clk_parent_data t7_pwm_parent_data[] = {
	{ .fw_name = "xtal", },
	{ .hw = &t7_vid_pll.hw },
	{ .hw = &t7_fclk_div4.hw },
	{ .hw = &t7_fclk_div3.hw }
};

static struct clk_regmap t7_pwm_a_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_AB_CTRL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_a_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t7_pwm_parent_data,
		.num_parents = ARRAY_SIZE(t7_pwm_parent_data),
		.flags = CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap t7_pwm_a_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_AB_CTRL,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_a_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_pwm_a_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap t7_pwm_a_gate = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_AB_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_a_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_pwm_a_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap t7_pwm_b_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_AB_CTRL,
		.mask = 0x3,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_b_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t7_pwm_parent_data,
		.num_parents = ARRAY_SIZE(t7_pwm_parent_data),
		.flags = CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap t7_pwm_b_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_AB_CTRL,
		.shift = 16,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_b_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_pwm_b_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap t7_pwm_b_gate = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_AB_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_b_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_pwm_b_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap t7_pwm_c_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_CD_CTRL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_c_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t7_pwm_parent_data,
		.num_parents = ARRAY_SIZE(t7_pwm_parent_data),
		.flags = CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap t7_pwm_c_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_CD_CTRL,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_c_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_pwm_c_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap t7_pwm_c_gate = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_CD_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_c_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_pwm_c_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap t7_pwm_d_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_CD_CTRL,
		.mask = 0x3,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_d_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t7_pwm_parent_data,
		.num_parents = ARRAY_SIZE(t7_pwm_parent_data),
		.flags = CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap t7_pwm_d_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_CD_CTRL,
		.shift = 16,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_d_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_pwm_d_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap t7_pwm_d_gate = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_CD_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_d_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_pwm_d_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap t7_pwm_e_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_EF_CTRL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_e_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t7_pwm_parent_data,
		.num_parents = ARRAY_SIZE(t7_pwm_parent_data),
		.flags = CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap t7_pwm_e_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_EF_CTRL,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_e_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_pwm_e_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap t7_pwm_e_gate = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_EF_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_e_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_pwm_e_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap t7_pwm_f_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_EF_CTRL,
		.mask = 0x3,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_f_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t7_pwm_parent_data,
		.num_parents = ARRAY_SIZE(t7_pwm_parent_data),
		.flags = CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap t7_pwm_f_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_EF_CTRL,
		.shift = 16,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_f_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_pwm_f_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap t7_pwm_f_gate = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_EF_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_f_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_pwm_f_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap t7_pwm_ao_a_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_AO_AB_CTRL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_ao_a_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t7_pwm_parent_data,
		.num_parents = ARRAY_SIZE(t7_pwm_parent_data),
	},
};

static struct clk_regmap t7_pwm_ao_a_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_AO_AB_CTRL,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_ao_a_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_pwm_ao_a_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_pwm_ao_a_gate = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_AO_AB_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_ao_a_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_pwm_ao_a_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap t7_pwm_ao_b_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_AO_AB_CTRL,
		.mask = 0x3,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_ao_b_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t7_pwm_parent_data,
		.num_parents = ARRAY_SIZE(t7_pwm_parent_data),
	},
};

static struct clk_regmap t7_pwm_ao_b_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_AO_AB_CTRL,
		.shift = 16,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_ao_b_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_pwm_ao_b_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_pwm_ao_b_gate = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_AO_AB_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_ao_b_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_pwm_ao_b_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap t7_pwm_ao_c_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_AO_CD_CTRL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_ao_c_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t7_pwm_parent_data,
		.num_parents = ARRAY_SIZE(t7_pwm_parent_data),
	},
};

static struct clk_regmap t7_pwm_ao_c_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_AO_CD_CTRL,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_ao_c_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_pwm_ao_c_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t7_pwm_ao_c_gate = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_AO_CD_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_ao_c_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_pwm_ao_c_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap t7_pwm_ao_d_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_AO_CD_CTRL,
		.mask = 0x3,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_ao_d_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t7_pwm_parent_data,
		.num_parents = ARRAY_SIZE(t7_pwm_parent_data),
	},
};

static struct clk_regmap t7_pwm_ao_d_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_AO_CD_CTRL,
		.shift = 16,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_ao_d_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_pwm_ao_d_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t7_pwm_ao_d_gate = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_AO_CD_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_ao_d_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_pwm_ao_d_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap t7_pwm_ao_e_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_AO_EF_CTRL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_ao_e_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t7_pwm_parent_data,
		.num_parents = ARRAY_SIZE(t7_pwm_parent_data),
	},
};

static struct clk_regmap t7_pwm_ao_e_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_AO_EF_CTRL,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_ao_e_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_pwm_ao_e_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t7_pwm_ao_e_gate = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_AO_EF_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_ao_e_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_pwm_ao_e_div.hw
		},
		.num_parents = 1,
		/*The clock feeds the GPU,it should be always on*/
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap t7_pwm_ao_f_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_AO_EF_CTRL,
		.mask = 0x3,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_ao_f_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t7_pwm_parent_data,
		.num_parents = ARRAY_SIZE(t7_pwm_parent_data),
	},
};

static struct clk_regmap t7_pwm_ao_f_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_AO_EF_CTRL,
		.shift = 16,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_ao_f_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_pwm_ao_f_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t7_pwm_ao_f_gate = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_AO_EF_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_ao_f_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_pwm_ao_f_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap t7_pwm_ao_g_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_AO_GH_CTRL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_ao_g_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t7_pwm_parent_data,
		.num_parents = ARRAY_SIZE(t7_pwm_parent_data),
	},
};

static struct clk_regmap t7_pwm_ao_g_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_AO_GH_CTRL,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_ao_g_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_pwm_ao_g_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t7_pwm_ao_g_gate = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_AO_GH_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_ao_g_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_pwm_ao_g_div.hw
		},
		.num_parents = 1,
		/*This clock feeds the DDR,it should be always on.*/
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap t7_pwm_ao_h_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_AO_GH_CTRL,
		.mask = 0x3,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_ao_h_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t7_pwm_parent_data,
		.num_parents = ARRAY_SIZE(t7_pwm_parent_data),
	},
};

static struct clk_regmap t7_pwm_ao_h_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_AO_GH_CTRL,
		.shift = 16,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_ao_h_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_pwm_ao_h_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t7_pwm_ao_h_gate = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_AO_GH_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_ao_h_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_pwm_ao_h_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static u32 t7_dsi_meas_table[] = { 0, 1, 2, 3, 6, 7};

static const struct clk_parent_data t7_dsi_meas_parent_data[] = {
	{ .fw_name = "xtal", },
	{ .hw = &t7_fclk_div4.hw },
	{ .hw = &t7_fclk_div3.hw },
	{ .hw = &t7_fclk_div5.hw },
	{ .hw = &t7_fclk_div2.hw },
	{ .hw = &t7_fclk_div7.hw }
};

static struct clk_regmap t7_dsi_a_meas_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_MIPI_DSI_MEAS_CLK_CTRL,
		.mask = 0x3,
		.shift = 9,
		.table = t7_dsi_meas_table
	},
	.hw.init = &(struct clk_init_data){
		.name = "dsi_a_meas_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t7_dsi_meas_parent_data,
		.num_parents = ARRAY_SIZE(t7_dsi_meas_parent_data)
	},
};

static struct clk_regmap t7_dsi_a_meas_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_MIPI_DSI_MEAS_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "dsi_a_meas_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_dsi_a_meas_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_dsi_a_meas_gate = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_MIPI_DSI_MEAS_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "dsi_a_meas_clk",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_dsi_a_meas_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_dsi_b_meas_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_MIPI_DSI_MEAS_CLK_CTRL,
		.mask = 0x3,
		.shift = 21,
		.table = t7_dsi_meas_table
	},
	.hw.init = &(struct clk_init_data){
		.name = "dsi_b_meas_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t7_dsi_meas_parent_data,
		.num_parents = ARRAY_SIZE(t7_dsi_meas_parent_data)
	},
};

static struct clk_regmap t7_dsi_b_meas_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_MIPI_DSI_MEAS_CLK_CTRL,
		.shift = 12,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "dsi_b_meas_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_dsi_b_meas_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_dsi_b_meas_gate = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_MIPI_DSI_MEAS_CLK_CTRL,
		.bit_idx = 20,
	},
	.hw.init = &(struct clk_init_data){
		.name = "dsi_b_meas_clk",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_dsi_b_meas_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static u32 t7_dsi_phy_table[] = { 4, 5, 6, 7};
static const struct clk_hw *t7_dsi_phy_parent_hws[] = {
	&t7_fclk_div2.hw,
	&t7_fclk_div2p5.hw,
	&t7_fclk_div3.hw,
	&t7_fclk_div7.hw
};

static struct clk_regmap t7_dsi0_phy_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_MIPIDSI_PHY_CLK_CTRL,
		.mask = 0x3,
		.shift = 12,
		.table = t7_dsi_phy_table
	},
	.hw.init = &(struct clk_init_data){
		.name = "dsi0_phy_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = t7_dsi_phy_parent_hws,
		.num_parents = ARRAY_SIZE(t7_dsi_phy_parent_hws)
	},
};

static struct clk_regmap t7_dsi0_phy_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_MIPIDSI_PHY_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "dsi0_phy_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_dsi0_phy_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t7_dsi0_phy_gate = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_MIPIDSI_PHY_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "dsi0_phy_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_dsi0_phy_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t7_dsi1_phy_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_MIPIDSI_PHY_CLK_CTRL,
		.mask = 0x3,
		.shift = 25,
		.table = t7_dsi_phy_table
	},
	.hw.init = &(struct clk_init_data){
		.name = "dsi1_phy_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = t7_dsi_phy_parent_hws,
		.num_parents = ARRAY_SIZE(t7_dsi_phy_parent_hws)
	},
};

static struct clk_regmap t7_dsi1_phy_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_MIPIDSI_PHY_CLK_CTRL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "dsi1_phy_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_dsi1_phy_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t7_dsi1_phy_gate = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_MIPIDSI_PHY_CLK_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "dsi1_phy_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_dsi1_phy_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t7_eth_rmii_sel = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_ETH_CLK_CTRL,
		.mask = 0x1,
		.shift = 9,
		.table = t7_dsi_phy_table
	},
	.hw.init = &(struct clk_init_data){
		.name = "eth_rmii_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_fclk_div2.hw,
			&t7_gp1_pll.hw
		},
		.num_parents = 2
	},
};

static struct clk_regmap t7_eth_rmii_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_ETH_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "eth_rmii_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_eth_rmii_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t7_eth_rmii = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_ETH_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "eth_rmii",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_eth_rmii_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static struct clk_fixed_factor t7_eth_div8 = {
	.mult = 1,
	.div = 8,
	.hw.init = &(struct clk_init_data){
		.name = "eth_div8",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) { &t7_fclk_div2.hw },
		.num_parents = 1,
	},
};

static struct clk_regmap t7_eth_125m = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_ETH_CLK_CTRL,
		.bit_idx = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "eth_125m",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_eth_div8.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap t7_saradc_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_SAR_CLK_CTRL0,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "saradc_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = (const struct clk_parent_data []) {
			{ .fw_name = "xtal", },
			{ .hw = &t7_sys_clk.hw },
		},
		.num_parents = 2,
	},
};

static struct clk_regmap t7_saradc_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_SAR_CLK_CTRL0,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "saradc_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_saradc_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_saradc_gate = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_SAR_CLK_CTRL0,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "saradc_clk",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_saradc_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/* gen clk */
static u32 t7_gen_clk_mux_table[] = { 0, 5, 6, 7, 19, 21, 22,
				23, 24, 25, 26, 27, 28 };

static const char * const t7_gen_clk_parent_names[] = {
	"xtal", "gp0_pll", "gp1_pll", "hifi_pll", "fclk_div2", "fclk_div3",
	"fclk_div4", "fclk_div5", "fclk_div7", "mpll0", "mpll1",
	"mpll2", "mpll3"
};

static struct clk_regmap t7_gen_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_GEN_CLK_CTRL,
		.mask = 0x1f,
		.shift = 12,
		.table = t7_gen_clk_mux_table,
	},
	.hw.init = &(struct clk_init_data){
		.name = "gen_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_names = t7_gen_clk_parent_names,
		.num_parents = ARRAY_SIZE(t7_gen_clk_parent_names),
	},
};

static struct clk_regmap t7_gen_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_GEN_CLK_CTRL,
		.shift = 0,
		.width = 11,
	},
	.hw.init = &(struct clk_init_data){
		.name = "gen_div",
		.ops = &clk_regmap_divider_ops,
		.parent_names = (const char *[]){ "gen_sel" },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_gen = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_GEN_CLK_CTRL,
		.bit_idx = 11,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "gen",
		.ops = &clk_regmap_gate_ops,
		.parent_names = (const char *[]){ "gen_div" },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_mclk_0_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = ANACTRL_MCLK_PLL_CNTL4,
		.mask = 0x3,
		.shift = 4,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mclk_0_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = (const struct clk_parent_data []) {
			{ .hw = &t7_mclk_pll.hw },
			{ .fw_name = "xtal", },
			{ .hw = &t7_mpll_50m.hw },
		},
		.num_parents = 3,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_fixed_factor t7_mclk_0_div2 = {
	.mult = 1,
	.div = 2,
	.hw.init = &(struct clk_init_data){
		.name = "mclk_0_div2",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) { &t7_mclk_0_sel.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_mclk_0_pre = {
	.data = &(struct clk_regmap_gate_data){
		.offset = ANACTRL_MCLK_PLL_CNTL4,
		.bit_idx = 2,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mclk_0_pre",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_mclk_0_div2.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_mclk_0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = ANACTRL_MCLK_PLL_CNTL4,
		.bit_idx = 0,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mclk_0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_mclk_0_pre.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_mclk_1_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = ANACTRL_MCLK_PLL_CNTL4,
		.mask = 0x3,
		.shift = 12,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mclk_1_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = (const struct clk_parent_data []) {
			{ .hw = &t7_mclk_pll.hw },
			{ .fw_name = "xtal", },
			{ .hw = &t7_mpll_50m.hw },
		},
		.num_parents = 3,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_fixed_factor t7_mclk_1_div2 = {
	.mult = 1,
	.div = 2,
	.hw.init = &(struct clk_init_data){
		.name = "mclk_1_div2",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) { &t7_mclk_1_sel.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_mclk_1_pre = {
	.data = &(struct clk_regmap_gate_data){
		.offset = ANACTRL_MCLK_PLL_CNTL4,
		.bit_idx = 10,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mclk_1_pre",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_mclk_1_div2.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t7_mclk_1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = ANACTRL_MCLK_PLL_CNTL4,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mclk_1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_mclk_1_pre.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

#define MESON_T7_SYS_GATE(_name, _reg, _bit)				\
struct clk_regmap _name = {						\
	.data = &(struct clk_regmap_gate_data) {			\
		.offset = (_reg),					\
		.bit_idx = (_bit),					\
	},								\
	.hw.init = &(struct clk_init_data) {				\
		.name = #_name,						\
		.ops = &clk_regmap_gate_ops,				\
		.parent_hws = (const struct clk_hw *[]) {		\
			&t7_sys_clk.hw					\
		},							\
		.num_parents = 1,					\
		.flags = CLK_IGNORE_UNUSED,				\
	},								\
}

/*CLKCTRL_SYS_CLK_EN0_REG0*/
static MESON_T7_SYS_GATE(t7_ddr,		CLKCTRL_SYS_CLK_EN0_REG0, 0);
static MESON_T7_SYS_GATE(t7_dos,		CLKCTRL_SYS_CLK_EN0_REG0, 1);
static MESON_T7_SYS_GATE(t7_mipi_dsi_a,	CLKCTRL_SYS_CLK_EN0_REG0, 2);
static MESON_T7_SYS_GATE(t7_mipi_dsi_b,	CLKCTRL_SYS_CLK_EN0_REG0, 3);
static MESON_T7_SYS_GATE(t7_ethphy,		CLKCTRL_SYS_CLK_EN0_REG0, 4);
static MESON_T7_SYS_GATE(t7_mali,		CLKCTRL_SYS_CLK_EN0_REG0, 6);
static MESON_T7_SYS_GATE(t7_aocpu,		CLKCTRL_SYS_CLK_EN0_REG0, 13);
static MESON_T7_SYS_GATE(t7_aucpu,		CLKCTRL_SYS_CLK_EN0_REG0, 14);
static MESON_T7_SYS_GATE(t7_cec,		CLKCTRL_SYS_CLK_EN0_REG0, 16);
static MESON_T7_SYS_GATE(t7_gdc,		CLKCTRL_SYS_CLK_EN0_REG0, 17);
static MESON_T7_SYS_GATE(t7_deswarp,		CLKCTRL_SYS_CLK_EN0_REG0, 18);
static MESON_T7_SYS_GATE(t7_ampipe_nand,	CLKCTRL_SYS_CLK_EN0_REG0, 19);
static MESON_T7_SYS_GATE(t7_ampipe_eth,	CLKCTRL_SYS_CLK_EN0_REG0, 20);
static MESON_T7_SYS_GATE(t7_am2axi0,		CLKCTRL_SYS_CLK_EN0_REG0, 21);
static MESON_T7_SYS_GATE(t7_am2axi1,		CLKCTRL_SYS_CLK_EN0_REG0, 22);
static MESON_T7_SYS_GATE(t7_am2axi2,		CLKCTRL_SYS_CLK_EN0_REG0, 23);
static MESON_T7_SYS_GATE(t7_sdemmca,		CLKCTRL_SYS_CLK_EN0_REG0, 24);
static MESON_T7_SYS_GATE(t7_sdemmcb,		CLKCTRL_SYS_CLK_EN0_REG0, 25);
static MESON_T7_SYS_GATE(t7_sdemmcc,		CLKCTRL_SYS_CLK_EN0_REG0, 26);
static MESON_T7_SYS_GATE(t7_smartcard,		CLKCTRL_SYS_CLK_EN0_REG0, 27);
static MESON_T7_SYS_GATE(t7_acodec,		CLKCTRL_SYS_CLK_EN0_REG0, 28);
static MESON_T7_SYS_GATE(t7_spifc,		CLKCTRL_SYS_CLK_EN0_REG0, 29);
static MESON_T7_SYS_GATE(t7_msr_clk,		CLKCTRL_SYS_CLK_EN0_REG0, 30);
static MESON_T7_SYS_GATE(t7_ir_ctrl,		CLKCTRL_SYS_CLK_EN0_REG0, 31);

/*CLKCTRL_SYS_CLK_EN0_REG1*/
static MESON_T7_SYS_GATE(t7_audio,		CLKCTRL_SYS_CLK_EN0_REG1, 0);
static MESON_T7_SYS_GATE(t7_eth,		CLKCTRL_SYS_CLK_EN0_REG1, 3);
static MESON_T7_SYS_GATE(t7_uart_a,		CLKCTRL_SYS_CLK_EN0_REG1, 5);
static MESON_T7_SYS_GATE(t7_uart_b,		CLKCTRL_SYS_CLK_EN0_REG1, 6);
static MESON_T7_SYS_GATE(t7_uart_c,		CLKCTRL_SYS_CLK_EN0_REG1, 7);
static MESON_T7_SYS_GATE(t7_uart_d,		CLKCTRL_SYS_CLK_EN0_REG1, 8);
static MESON_T7_SYS_GATE(t7_uart_e,		CLKCTRL_SYS_CLK_EN0_REG1, 9);
static MESON_T7_SYS_GATE(t7_uart_f,		CLKCTRL_SYS_CLK_EN0_REG1, 10);
static MESON_T7_SYS_GATE(t7_aififo,		CLKCTRL_SYS_CLK_EN0_REG1, 11);
static MESON_T7_SYS_GATE(t7_spicc2,		CLKCTRL_SYS_CLK_EN0_REG1, 12);
static MESON_T7_SYS_GATE(t7_spicc3,		CLKCTRL_SYS_CLK_EN0_REG1, 13);
static MESON_T7_SYS_GATE(t7_spicc4,		CLKCTRL_SYS_CLK_EN0_REG1, 14);
static MESON_T7_SYS_GATE(t7_ts_a73,		CLKCTRL_SYS_CLK_EN0_REG1, 15);
static MESON_T7_SYS_GATE(t7_ts_a53,		CLKCTRL_SYS_CLK_EN0_REG1, 16);
static MESON_T7_SYS_GATE(t7_spicc5,		CLKCTRL_SYS_CLK_EN0_REG1, 17);
static MESON_T7_SYS_GATE(t7_g2d,		CLKCTRL_SYS_CLK_EN0_REG1, 20);
static MESON_T7_SYS_GATE(t7_spicc0,		CLKCTRL_SYS_CLK_EN0_REG1, 21);
static MESON_T7_SYS_GATE(t7_spicc1,		CLKCTRL_SYS_CLK_EN0_REG1, 22);
static MESON_T7_SYS_GATE(t7_pcie,		CLKCTRL_SYS_CLK_EN0_REG1, 24);
static MESON_T7_SYS_GATE(t7_usb,		CLKCTRL_SYS_CLK_EN0_REG1, 26);
static MESON_T7_SYS_GATE(t7_pcie_phy,		CLKCTRL_SYS_CLK_EN0_REG1, 27);
static MESON_T7_SYS_GATE(t7_i2c_ao_a,		CLKCTRL_SYS_CLK_EN0_REG1, 28);
static MESON_T7_SYS_GATE(t7_i2c_ao_b,		CLKCTRL_SYS_CLK_EN0_REG1, 29);
static MESON_T7_SYS_GATE(t7_i2c_m_a,		CLKCTRL_SYS_CLK_EN0_REG1, 30);
static MESON_T7_SYS_GATE(t7_i2c_m_b,		CLKCTRL_SYS_CLK_EN0_REG1, 31);

/*CLKCTRL_SYS_CLK_EN0_REG2*/
static MESON_T7_SYS_GATE(t7_i2c_m_c,		CLKCTRL_SYS_CLK_EN0_REG2, 0);
static MESON_T7_SYS_GATE(t7_i2c_m_d,		CLKCTRL_SYS_CLK_EN0_REG2, 1);
static MESON_T7_SYS_GATE(t7_i2c_m_e,		CLKCTRL_SYS_CLK_EN0_REG2, 2);
static MESON_T7_SYS_GATE(t7_i2c_m_f,		CLKCTRL_SYS_CLK_EN0_REG2, 3);
static MESON_T7_SYS_GATE(t7_hdmitx_apb,	CLKCTRL_SYS_CLK_EN0_REG2, 4);
static MESON_T7_SYS_GATE(t7_i2c_s_a,		CLKCTRL_SYS_CLK_EN0_REG2, 5);
static MESON_T7_SYS_GATE(t7_hdmirx_pclk,	CLKCTRL_SYS_CLK_EN0_REG2, 8);
static MESON_T7_SYS_GATE(t7_mmc_apb,		CLKCTRL_SYS_CLK_EN0_REG2, 11);
static MESON_T7_SYS_GATE(t7_mipi_isp_pclk,	CLKCTRL_SYS_CLK_EN0_REG2, 17);
static MESON_T7_SYS_GATE(t7_rsa,		CLKCTRL_SYS_CLK_EN0_REG2, 18);
static MESON_T7_SYS_GATE(t7_pclk_sys_cpu_apb,	CLKCTRL_SYS_CLK_EN0_REG2, 19);
static MESON_T7_SYS_GATE(t7_a73pclk_cpu_apb,	CLKCTRL_SYS_CLK_EN0_REG2, 20);
static MESON_T7_SYS_GATE(t7_dspa,		CLKCTRL_SYS_CLK_EN0_REG2, 21);
static MESON_T7_SYS_GATE(t7_dspb,		CLKCTRL_SYS_CLK_EN0_REG2, 22);
static MESON_T7_SYS_GATE(t7_vpu_intr,		CLKCTRL_SYS_CLK_EN0_REG2, 25);
static MESON_T7_SYS_GATE(t7_sar_adc,		CLKCTRL_SYS_CLK_EN0_REG2, 28);
static MESON_T7_SYS_GATE(t7_gic,		CLKCTRL_SYS_CLK_EN0_REG2, 30);
static MESON_T7_SYS_GATE(t7_ts_gpu,		CLKCTRL_SYS_CLK_EN0_REG2, 31);

/*CLKCTRL_SYS_CLK_EN0_REG3*/
static MESON_T7_SYS_GATE(t7_ts_nna,		CLKCTRL_SYS_CLK_EN0_REG3, 0);
static MESON_T7_SYS_GATE(t7_ts_vpu,		CLKCTRL_SYS_CLK_EN0_REG3, 1);
static MESON_T7_SYS_GATE(t7_ts_hevc,		CLKCTRL_SYS_CLK_EN0_REG3, 2);
static MESON_T7_SYS_GATE(t7_pwm_ao_ab,		CLKCTRL_SYS_CLK_EN0_REG3, 3);
static MESON_T7_SYS_GATE(t7_pwm_ao_cd,		CLKCTRL_SYS_CLK_EN0_REG3, 4);
static MESON_T7_SYS_GATE(t7_pwm_ao_ef,		CLKCTRL_SYS_CLK_EN0_REG3, 5);
static MESON_T7_SYS_GATE(t7_pwm_ao_gh,		CLKCTRL_SYS_CLK_EN0_REG3, 6);
static MESON_T7_SYS_GATE(t7_pwm_ab,		CLKCTRL_SYS_CLK_EN0_REG3, 7);
static MESON_T7_SYS_GATE(t7_pwm_cd,		CLKCTRL_SYS_CLK_EN0_REG3, 8);
static MESON_T7_SYS_GATE(t7_pwm_ef,		CLKCTRL_SYS_CLK_EN0_REG3, 9);

/* Array of all clocks provided by this provider */
static struct clk_hw_onecell_data t7_hw_onecell_data = {
	.hws = {
		[CLKID_FIXED_PLL_DCO]		= &t7_fixed_pll_dco.hw,
		[CLKID_FIXED_PLL]		= &t7_fixed_pll.hw,
		[CLKID_SYS_PLL_DCO]		= &t7_sys_pll_dco.hw,
		[CLKID_SYS_PLL]			= &t7_sys_pll.hw,
		[CLKID_SYS1_PLL_DCO]		= &t7_sys1_pll_dco.hw,
		[CLKID_SYS1_PLL]		= &t7_sys1_pll.hw,
		[CLKID_FCLK_DIV2_DIV]		= &t7_fclk_div2_div.hw,
		[CLKID_FCLK_DIV2]		= &t7_fclk_div2.hw,
		[CLKID_FCLK_DIV3_DIV]		= &t7_fclk_div3_div.hw,
		[CLKID_FCLK_DIV3]		= &t7_fclk_div3.hw,
		[CLKID_FCLK_DIV4_DIV]		= &t7_fclk_div4_div.hw,
		[CLKID_FCLK_DIV4]		= &t7_fclk_div4.hw,
		[CLKID_FCLK_DIV5_DIV]		= &t7_fclk_div5_div.hw,
		[CLKID_FCLK_DIV5]		= &t7_fclk_div5.hw,
		[CLKID_FCLK_DIV7_DIV]		= &t7_fclk_div7_div.hw,
		[CLKID_FCLK_DIV7]		= &t7_fclk_div7.hw,
		[CLKID_FCLK_DIV2P5_DIV]		= &t7_fclk_div2p5_div.hw,
		[CLKID_FCLK_DIV2P5]		= &t7_fclk_div2p5.hw,
		[CLKID_GP0_PLL_DCO]		= &t7_gp0_pll_dco.hw,
		[CLKID_GP0_PLL]			= &t7_gp0_pll.hw,
		[CLKID_GP1_PLL_DCO]		= &t7_gp1_pll_dco.hw,
		[CLKID_GP1_PLL]			= &t7_gp1_pll.hw,
		[CLKID_MCLK_PLL_DCO]		= &t7_mclk_pll_dco.hw,
		[CLKID_MCLK_PRE]		= &t7_mclk_pre_od.hw,
		[CLKID_MCLK_PLL]		= &t7_mclk_pll.hw,
		[CLKID_CPU_DYN_CLK]		= &t7_cpu_dyn_clk.hw,
		[CLKID_CPU_CLK]			= &t7_cpu_clk.hw,
		[CLKID_A73_DYN_CLK]		= &t7_a73_dyn_clk.hw,
		[CLKID_A73_CLK]			= &t7_a73_clk.hw,
/*
 *		[CLKID_CPU1_CLK]		= &t7_cpu1_clk.hw,
 *		[CLKID_CPU2_CLK]		= &t7_cpu2_clk.hw,
 *		[CLKID_CPU3_CLK]		= &t7_cpu3_clk.hw,
 */
		[CLKID_HIFI_PLL_DCO]		= &t7_hifi_pll_dco.hw,
		[CLKID_HIFI_PLL]		= &t7_hifi_pll.hw,
		[CLKID_PCIE_PLL_DCO]		= &t7_pcie_pll_dco.hw,
		[CLKID_PCIE_PLL_DCO_DIV2]	= &t7_pcie_pll_dco_div2.hw,
		[CLKID_PCIE_PLL_OD]		= &t7_pcie_pll_od.hw,
		[CLKID_PCIE_PLL]		= &t7_pcie_pll.hw,
		[CLKID_PCIE_HCSL]		= &t7_pcie_hcsl.hw,
		[CLKID_HDMI_PLL_DCO]		= &t7_hdmi_pll_dco.hw,
		[CLKID_HDMI_PLL_OD]		= &t7_hdmi_pll_od.hw,
		[CLKID_HDMI_PLL]		= &t7_hdmi_pll.hw,
		[CLKID_MPLL_50M_DIV]		= &t7_mpll_50m_div.hw,
		[CLKID_MPLL_50M]		= &t7_mpll_50m.hw,
		[CLKID_MPLL_PREDIV]		= &t7_mpll_prediv.hw,
		[CLKID_MPLL0_DIV]		= &t7_mpll0_div.hw,
		[CLKID_MPLL0]			= &t7_mpll0.hw,
		[CLKID_MPLL1_DIV]		= &t7_mpll1_div.hw,
		[CLKID_MPLL1]			= &t7_mpll1.hw,
		[CLKID_MPLL2_DIV]		= &t7_mpll2_div.hw,
		[CLKID_MPLL2]			= &t7_mpll2.hw,
		[CLKID_MPLL3_DIV]		= &t7_mpll3_div.hw,
		[CLKID_MPLL3]			= &t7_mpll3.hw,
		[CLKID_RTC_32K_CLKIN]		= &t7_rtc_32k_clkin.hw,
		[CLKID_RTC_32K_DIV]		= &t7_rtc_32k_div.hw,
		[CLKID_RTC_32K_XATL]		= &t7_rtc_32k_xtal.hw,
		[CLKID_RTC_32K_MUX]		= &t7_rtc_32k_sel.hw,
		[CLKID_RTC_CLK]			= &t7_rtc_clk.hw,
		[CLKID_SYS_CLK_B_MUX]		= &t7_sysclk_b_sel.hw,
		[CLKID_SYS_CLK_B_DIV]		= &t7_sysclk_b_div.hw,
		[CLKID_SYS_CLK_B_GATE]		= &t7_sysclk_b.hw,
		[CLKID_SYS_CLK_A_MUX]		= &t7_sysclk_a_sel.hw,
		[CLKID_SYS_CLK_A_DIV]		= &t7_sysclk_a_div.hw,
		[CLKID_SYS_CLK_A_GATE]		= &t7_sysclk_a.hw,
		[CLKID_SYS_CLK]			= &t7_sys_clk.hw,
		[CLKID_CECA_32K_CLKIN]		= &t7_ceca_32k_clkin.hw,
		[CLKID_CECA_32K_DIV]		= &t7_ceca_32k_div.hw,
		[CLKID_CECA_32K_MUX_PRE]	= &t7_ceca_32k_sel_pre.hw,
		[CLKID_CECA_32K_MUX]		= &t7_ceca_32k_sel.hw,
		[CLKID_CECA_32K_CLKOUT]		= &t7_ceca_32k_clkout.hw,
		[CLKID_CECB_32K_CLKIN]		= &t7_cecb_32k_clkin.hw,
		[CLKID_CECB_32K_DIV]		= &t7_cecb_32k_div.hw,
		[CLKID_CECB_32K_MUX_PRE]	= &t7_cecb_32k_sel_pre.hw,
		[CLKID_CECB_32K_MUX]		= &t7_cecb_32k_sel.hw,
		[CLKID_CECB_32K_CLKOUT]		= &t7_cecb_32k_clkout.hw,
		[CLKID_SC_CLK_MUX]		= &t7_sc_clk_mux.hw,
		[CLKID_SC_CLK_DIV]		= &t7_sc_clk_div.hw,
		[CLKID_SC_CLK_GATE]		= &t7_sc_clk_gate.hw,
		[CLKID_DSPA_CLK_B_MUX]		= &t7_dspa_b_mux.hw,
		[CLKID_DSPA_CLK_B_DIV]		= &t7_dspa_b_div.hw,
		[CLKID_DSPA_CLK_B_GATE]		= &t7_dspa_b_gate.hw,
		[CLKID_DSPA_CLK_A_MUX]		= &t7_dspa_a_mux.hw,
		[CLKID_DSPA_CLK_A_DIV]		= &t7_dspa_a_div.hw,
		[CLKID_DSPA_CLK_A_GATE]		= &t7_dspa_a_gate.hw,
		[CLKID_DSPA_CLK]		= &t7_dspa_mux.hw,
		[CLKID_DSPB_CLK_B_MUX]		= &t7_dspb_b_mux.hw,
		[CLKID_DSPB_CLK_B_DIV]		= &t7_dspb_b_div.hw,
		[CLKID_DSPB_CLK_B_GATE]		= &t7_dspb_b_gate.hw,
		[CLKID_DSPB_CLK_A_MUX]		= &t7_dspb_a_mux.hw,
		[CLKID_DSPB_CLK_A_DIV]		= &t7_dspb_a_div.hw,
		[CLKID_DSPB_CLK_A_GATE]		= &t7_dspb_a_gate.hw,
		[CLKID_DSPB_CLK]		= &t7_dspb_mux.hw,
		[CLKID_24M_CLK_GATE]		= &t7_24M_clk_gate.hw,
		[CLKID_12M_CLK_DIV]		= &t7_12M_clk_div.hw,
		[CLKID_12M_CLK_GATE]		= &t7_12M_clk_gate.hw,
		[CLKID_25M_CLK_DIV]		= &t7_25M_clk_div.hw,
		[CLKID_25M_CLK_GATE]		= &t7_25M_clk_gate.hw,
		[CLKID_VID_PLL]			= &t7_vid_pll_div.hw,
		[CLKID_VID_PLL_MUX]		= &t7_vid_pll_sel.hw,
		[CLKID_VID_PLL]			= &t7_vid_pll.hw,
		[CLKID_VCLK_MUX]		= &t7_vclk_sel.hw,
		[CLKID_VCLK2_MUX]		= &t7_vclk2_sel.hw,
		[CLKID_VCLK_INPUT]		= &t7_vclk_input.hw,
		[CLKID_VCLK2_INPUT]		= &t7_vclk2_input.hw,
		[CLKID_VCLK_DIV]		= &t7_vclk_div.hw,
		[CLKID_VCLK2_DIV]		= &t7_vclk2_div.hw,
		[CLKID_VCLK]			= &t7_vclk.hw,
		[CLKID_VCLK2]			= &t7_vclk2.hw,
		[CLKID_VCLK_DIV1]		= &t7_vclk_div1.hw,
		[CLKID_VCLK_DIV2_EN]		= &t7_vclk_div2_en.hw,
		[CLKID_VCLK_DIV4_EN]		= &t7_vclk_div4_en.hw,
		[CLKID_VCLK_DIV6_EN]		= &t7_vclk_div6_en.hw,
		[CLKID_VCLK_DIV12_EN]		= &t7_vclk_div12_en.hw,
		[CLKID_VCLK2_DIV1]		= &t7_vclk2_div1.hw,
		[CLKID_VCLK2_DIV2_EN]		= &t7_vclk2_div2_en.hw,
		[CLKID_VCLK2_DIV4_EN]		= &t7_vclk2_div4_en.hw,
		[CLKID_VCLK2_DIV6_EN]		= &t7_vclk2_div6_en.hw,
		[CLKID_VCLK2_DIV12_EN]		= &t7_vclk2_div12_en.hw,
		[CLKID_VCLK_DIV2]		= &t7_vclk_div2.hw,
		[CLKID_VCLK_DIV4]		= &t7_vclk_div4.hw,
		[CLKID_VCLK_DIV6]		= &t7_vclk_div6.hw,
		[CLKID_VCLK_DIV12]		= &t7_vclk_div12.hw,
		[CLKID_VCLK2_DIV2]		= &t7_vclk2_div2.hw,
		[CLKID_VCLK2_DIV4]		= &t7_vclk2_div4.hw,
		[CLKID_VCLK2_DIV6]		= &t7_vclk2_div6.hw,
		[CLKID_VCLK2_DIV12]		= &t7_vclk2_div12.hw,
		[CLKID_CTS_ENCI_MUX]		= &t7_cts_enci_sel.hw,
		[CLKID_CTS_ENCP_MUX]		= &t7_cts_encp_sel.hw,
		[CLKID_CTS_VDAC_MUX]		= &t7_cts_vdac_sel.hw,
		[CLKID_HDMI_TX_MUX]		= &t7_hdmi_tx_sel.hw,
		[CLKID_CTS_ENCI]		= &t7_cts_enci.hw,
		[CLKID_CTS_ENCP]		= &t7_cts_encp.hw,
		[CLKID_CTS_VDAC]		= &t7_cts_vdac.hw,
		[CLKID_HDMI_TX]			= &t7_hdmi_tx.hw,
		[CLKID_HDMITX_SYS_MUX]		= &t7_hdmitx_sys_sel.hw,
		[CLKID_HDMITX_SYS_DIV]		= &t7_hdmitx_sys_div.hw,
		[CLKID_HDMITX_SYS]		= &t7_hdmitx_sys.hw,
		[CLKID_HDMITX_PRIF_MUX]		= &t7_hdmitx_prif_sel.hw,
		[CLKID_HDMITX_PRIF_DIV]		= &t7_hdmitx_prif_div.hw,
		[CLKID_HDMITX_PRIF]		= &t7_hdmitx_prif.hw,
		[CLKID_HDMITX_200M_MUX]		= &t7_hdmitx_200m_sel.hw,
		[CLKID_HDMITX_200M_DIV]		= &t7_hdmitx_200m_div.hw,
		[CLKID_HDMITX_200M]		= &t7_hdmitx_200m.hw,
		[CLKID_HDMITX_AUD_MUX]		= &t7_hdmitx_aud_sel.hw,
		[CLKID_HDMITX_AUD_DIV]		= &t7_hdmitx_aud_div.hw,
		[CLKID_HDMITX_AUD]		= &t7_hdmitx_aud.hw,
		[CLKID_HDMIRX_5M_MUX]		= &t7_hdmirx_5m_sel.hw,
		[CLKID_HDMIRX_5M_DIV]		= &t7_hdmirx_5m_div.hw,
		[CLKID_HDMIRX_5M]		= &t7_hdmirx_5m.hw,
		[CLKID_HDMIRX_2M_MUX]		= &t7_hdmirx_2m_sel.hw,
		[CLKID_HDMIRX_2M_DIV]		= &t7_hdmirx_2m_div.hw,
		[CLKID_HDMIRX_2M]		= &t7_hdmirx_2m.hw,
		[CLKID_HDMIRX_CFG_MUX]		= &t7_hdmirx_cfg_sel.hw,
		[CLKID_HDMIRX_CFG_DIV]		= &t7_hdmirx_cfg_div.hw,
		[CLKID_HDMIRX_CFG]		= &t7_hdmirx_cfg.hw,
		[CLKID_HDMIRX_HDCP_MUX]		= &t7_hdmirx_hdcp_sel.hw,
		[CLKID_HDMIRX_HDCP_DIV]		= &t7_hdmirx_hdcp_div.hw,
		[CLKID_HDMIRX_HDCP]		= &t7_hdmirx_hdcp.hw,
		[CLKID_HDMIRX_AUD_PLL_MUX]	= &t7_hdmirx_aud_pll_sel.hw,
		[CLKID_HDMIRX_AUD_PLL_DIV]	= &t7_hdmirx_aud_pll_div.hw,
		[CLKID_HDMIRX_AUD_PLL]		= &t7_hdmirx_aud_pll.hw,
		[CLKID_HDMIRX_ACR_MUX]		= &t7_hdmirx_acr_sel.hw,
		[CLKID_HDMIRX_ACR_DIV]		= &t7_hdmirx_acr_div.hw,
		[CLKID_HDMIRX_ACR]		= &t7_hdmirx_acr.hw,
		[CLKID_HDMIRX_METER_MUX]	= &t7_hdmirx_meter_sel.hw,
		[CLKID_HDMIRX_METER_DIV]	= &t7_hdmirx_meter_div.hw,
		[CLKID_HDMIRX_METER]		= &t7_hdmirx_meter.hw,
		[CLKID_MCLK_0_SEL]		= &t7_mclk_0_sel.hw,
		[CLKID_MCLK_0_DIV2]		= &t7_mclk_0_div2.hw,
		[CLKID_MCLK_0_PRE]		= &t7_mclk_0_pre.hw,
		[CLKID_MCLK_0]			= &t7_mclk_0.hw,
		[CLKID_MCLK_1_SEL]		= &t7_mclk_1_sel.hw,
		[CLKID_MCLK_1_DIV2]		= &t7_mclk_1_div2.hw,
		[CLKID_MCLK_1_PRE]		= &t7_mclk_1_pre.hw,
		[CLKID_MCLK_1]			= &t7_mclk_1.hw,
		[CLKID_TS_CLK_DIV]		= &t7_ts_clk_div.hw,
		[CLKID_TS_CLK_GATE]		= &t7_ts_clk_gate.hw,
		[CLKID_MALI_0_SEL]		= &t7_mali_0_sel.hw,
		[CLKID_MALI_0_DIV]		= &t7_mali_0_div.hw,
		[CLKID_MALI_0]			= &t7_mali_0.hw,
		[CLKID_MALI_1_SEL]		= &t7_mali_1_sel.hw,
		[CLKID_MALI_1_DIV]		= &t7_mali_1_div.hw,
		[CLKID_MALI_1]			= &t7_mali_1.hw,
		[CLKID_MALI_MUX]		= &t7_mali_mux.hw,
		[CLKID_VDEC_P0_MUX]		= &t7_vdec_p0_mux.hw,
		[CLKID_VDEC_P0_DIV]		= &t7_vdec_p0_div.hw,
		[CLKID_VDEC_P0]			= &t7_vdec_p0.hw,
		[CLKID_VDEC_P1_MUX]		= &t7_vdec_p1_mux.hw,
		[CLKID_VDEC_P1_DIV]		= &t7_vdec_p1_div.hw,
		[CLKID_VDEC_P1]			= &t7_vdec_p1.hw,
		[CLKID_VDEC_MUX]		= &t7_vdec_mux.hw,
		[CLKID_HCODEC_P0_MUX]		= &t7_hcodec_p0_mux.hw,
		[CLKID_HCODEC_P0_DIV]		= &t7_hcodec_p0_div.hw,
		[CLKID_HCODEC_P0]		= &t7_hcodec_p0.hw,
		[CLKID_HCODEC_P1_MUX]		= &t7_hcodec_p1_mux.hw,
		[CLKID_HCODEC_P1_DIV]		= &t7_hcodec_p1_div.hw,
		[CLKID_HCODEC_P1]		= &t7_hcodec_p1.hw,
		[CLKID_HCODEC_MUX]		= &t7_hcodec_mux.hw,
		[CLKID_HEVCB_P0_MUX]		= &t7_hevcb_p0_mux.hw,
		[CLKID_HEVCB_P0_DIV]		= &t7_hevcb_p0_div.hw,
		[CLKID_HEVCB_P0]		= &t7_hevcb_p0.hw,
		[CLKID_HEVCB_P1_MUX]		= &t7_hevcb_p1_mux.hw,
		[CLKID_HEVCB_P1_DIV]		= &t7_hevcb_p1_div.hw,
		[CLKID_HEVCB_P1]		= &t7_hevcb_p1.hw,
		[CLKID_HEVCB_MUX]		= &t7_hevcb_mux.hw,
		[CLKID_HEVCF_P0_MUX]		= &t7_hevcf_p0_mux.hw,
		[CLKID_HEVCF_P0_DIV]		= &t7_hevcf_p0_div.hw,
		[CLKID_HEVCF_P0]		= &t7_hevcf_p0.hw,
		[CLKID_HEVCF_P1_MUX]		= &t7_hevcf_p1_mux.hw,
		[CLKID_HEVCF_P1_DIV]		= &t7_hevcf_p1_div.hw,
		[CLKID_HEVCF_P1]		= &t7_hevcf_p1.hw,
		[CLKID_HEVCF_MUX]		= &t7_hevcf_mux.hw,
		[CLKID_WAVE_A_MUX]		= &t7_wave_a_sel.hw,
		[CLKID_WAVE_A_DIV]		= &t7_wave_a_div.hw,
		[CLKID_WAVE_A_GATE]		= &t7_wave_aclk.hw,
		[CLKID_WAVE_B_MUX]		= &t7_wave_b_sel.hw,
		[CLKID_WAVE_B_DIV]		= &t7_wave_b_div.hw,
		[CLKID_WAVE_B_GATE]		= &t7_wave_bclk.hw,
		[CLKID_WAVE_C_MUX]		= &t7_wave_c_sel.hw,
		[CLKID_WAVE_C_DIV]		= &t7_wave_c_div.hw,
		[CLKID_WAVE_C_GATE]		= &t7_wave_cclk.hw,
		[CLKID_MIPI_ISP_MUX]		= &t7_mipi_isp_sel.hw,
		[CLKID_MIPI_ISP_DIV]		= &t7_mipi_isp_div.hw,
		[CLKID_MIPI_ISP]		= &t7_mipi_isp.hw,
		[CLKID_MIPI_CSI_PHY_SEL0]	= &t7_mipi_csi_phy_sel0.hw,
		[CLKID_MIPI_CSI_PHY_DIV0]	= &t7_mipi_csi_phy_div0.hw,
		[CLKID_MIPI_CSI_PHY0]		= &t7_mipi_csi_phy0.hw,
		[CLKID_MIPI_CSI_PHY_SEL1]	= &t7_mipi_csi_phy_sel1.hw,
		[CLKID_MIPI_CSI_PHY_DIV1]	= &t7_mipi_csi_phy_div1.hw,
		[CLKID_MIPI_CSI_PHY1]		= &t7_mipi_csi_phy1.hw,
		[CLKID_MIPI_CSI_PHY_CLK]	= &t7_mipi_csi_phy_clk.hw,
		[CLKID_VPU_0_MUX]		= &t7_vpu_0_sel.hw,
		[CLKID_VPU_0_DIV]		= &t7_vpu_0_div.hw,
		[CLKID_VPU_0]			= &t7_vpu_0.hw,
		[CLKID_VPU_1_MUX]		= &t7_vpu_1_sel.hw,
		[CLKID_VPU_1_DIV]		= &t7_vpu_1_div.hw,
		[CLKID_VPU_1]			= &t7_vpu_1.hw,
		[CLKID_VPU]			= &t7_vpu.hw,
		[CLKID_VPU_CLKB_TMP_MUX]	= &t7_vpu_clkb_tmp_mux.hw,
		[CLKID_VPU_CLKB_TMP_DIV]	= &t7_vpu_clkb_tmp_div.hw,
		[CLKID_VPU_CLKB_TMP]		= &t7_vpu_clkb_tmp.hw,
		[CLKID_VPU_CLKB_DIV]		= &t7_vpu_clkb_div.hw,
		[CLKID_VPU_CLKB]		= &t7_vpu_clkb.hw,
		[CLKID_VPU_CLKC_P0_MUX]		= &t7_vpu_clkc_p0_mux.hw,
		[CLKID_VPU_CLKC_P0_DIV]		= &t7_vpu_clkc_p0_div.hw,
		[CLKID_VPU_CLKC_P0]		= &t7_vpu_clkc_p0.hw,
		[CLKID_VPU_CLKC_P1_MUX]		= &t7_vpu_clkc_p1_mux.hw,
		[CLKID_VPU_CLKC_P1_DIV]		= &t7_vpu_clkc_p1_div.hw,
		[CLKID_VPU_CLKC_P1]		= &t7_vpu_clkc_p1.hw,
		[CLKID_VPU_CLKC_MUX]		= &t7_vpu_clkc_mux.hw,
		[CLKID_VAPB_0_MUX]		= &t7_vapb_0_sel.hw,
		[CLKID_VAPB_0_DIV]		= &t7_vapb_0_div.hw,
		[CLKID_VAPB_0]			= &t7_vapb_0.hw,
		[CLKID_VAPB_1_MUX]		= &t7_vapb_1_sel.hw,
		[CLKID_VAPB_1_DIV]		= &t7_vapb_1_div.hw,
		[CLKID_VAPB_1]			= &t7_vapb_1.hw,
		[CLKID_VAPB]			= &t7_vapb.hw,
		[CLKID_GDCCLK_0_MUX]		= &t7_gdcclk_0_sel.hw,
		[CLKID_GDCCLK_0_DIV]		= &t7_gdcclk_0_div.hw,
		[CLKID_GDCCLK_0]		= &t7_gdcclk_0.hw,
		[CLKID_GDCCLK_1_MUX]		= &t7_gdcclk_1_sel.hw,
		[CLKID_GDCCLK_1_DIV]		= &t7_gdcclk_1_div.hw,
		[CLKID_GDCCLK_1]		= &t7_gdcclk_1.hw,
		[CLKID_GDCCLK]			= &t7_gdcclk.hw,
		[CLKID_GDC_CLK]			= &t7_gdc_clk.hw,
		[CLKID_DEWARPCLK_0_MUX]		= &t7_dewarpclk_0_sel.hw,
		[CLKID_DEWARPCLK_0_DIV]		= &t7_dewarpclk_0_div.hw,
		[CLKID_DEWARPCLK_0]		= &t7_dewarpclk_0.hw,
		[CLKID_DEWARPCLK_1_MUX]		= &t7_dewarpclk_1_sel.hw,
		[CLKID_DEWARPCLK_1_DIV]		= &t7_dewarpclk_1_div.hw,
		[CLKID_DEWARPCLK_1]		= &t7_dewarpclk_1.hw,
		[CLKID_DEWARPCLK]		= &t7_dewarpclk.hw,
		[CLKID_DEWARP_CLK]		= &t7_dewarp_clk.hw,
		[CLKID_ANAKIN_0_MUX]		= &t7_anakin_0_sel.hw,
		[CLKID_ANAKIN_0_DIV]		= &t7_anakin_0_div.hw,
		[CLKID_ANAKIN_0]		= &t7_anakin_0.hw,
		[CLKID_ANAKIN_1_MUX]		= &t7_anakin_1_sel.hw,
		[CLKID_ANAKIN_1_DIV]		= &t7_anakin_1_div.hw,
		[CLKID_ANAKIN_1]		= &t7_anakin_1.hw,
		[CLKID_ANAKIN]			= &t7_anakin.hw,
		[CLKID_ANAKIN_CLK]		= &t7_anakin_clk.hw,
		[CLKID_GE2D]			= &t7_ge2d_gate.hw,
		[CLKID_VDIN_MEAS_MUX]		= &t7_vdin_meas_mux.hw,
		[CLKID_VDIN_MEAS_DIV]		= &t7_vdin_meas_div.hw,
		[CLKID_VDIN_MEAS_GATE]		= &t7_vdin_meas_gate.hw,
		[CLKID_VID_LOCK_DIV]		= &t7_vid_lock_div.hw,
		[CLKID_VID_LOCK]		= &t7_vid_lock_clk.hw,
		[CLKID_PWM_A_MUX]		= &t7_pwm_a_mux.hw,
		[CLKID_PWM_A_DIV]		= &t7_pwm_a_div.hw,
		[CLKID_PWM_A_GATE]		= &t7_pwm_a_gate.hw,
		[CLKID_PWM_B_MUX]		= &t7_pwm_b_mux.hw,
		[CLKID_PWM_B_DIV]		= &t7_pwm_b_div.hw,
		[CLKID_PWM_B_GATE]		= &t7_pwm_b_gate.hw,
		[CLKID_PWM_C_MUX]		= &t7_pwm_c_mux.hw,
		[CLKID_PWM_C_DIV]		= &t7_pwm_c_div.hw,
		[CLKID_PWM_C_GATE]		= &t7_pwm_c_gate.hw,
		[CLKID_PWM_D_MUX]		= &t7_pwm_d_mux.hw,
		[CLKID_PWM_D_DIV]		= &t7_pwm_d_div.hw,
		[CLKID_PWM_D_GATE]		= &t7_pwm_d_gate.hw,
		[CLKID_PWM_E_MUX]		= &t7_pwm_e_mux.hw,
		[CLKID_PWM_E_DIV]		= &t7_pwm_e_div.hw,
		[CLKID_PWM_E_GATE]		= &t7_pwm_e_gate.hw,
		[CLKID_PWM_F_MUX]		= &t7_pwm_f_mux.hw,
		[CLKID_PWM_F_DIV]		= &t7_pwm_f_div.hw,
		[CLKID_PWM_F_GATE]		= &t7_pwm_f_gate.hw,
		[CLKID_PWM_AO_A_MUX]		= &t7_pwm_ao_a_mux.hw,
		[CLKID_PWM_AO_A_DIV]		= &t7_pwm_ao_a_div.hw,
		[CLKID_PWM_AO_A_GATE]		= &t7_pwm_ao_a_gate.hw,
		[CLKID_PWM_AO_B_MUX]		= &t7_pwm_ao_b_mux.hw,
		[CLKID_PWM_AO_B_DIV]		= &t7_pwm_ao_b_div.hw,
		[CLKID_PWM_AO_B_GATE]		= &t7_pwm_ao_b_gate.hw,
		[CLKID_PWM_AO_C_MUX]		= &t7_pwm_ao_c_mux.hw,
		[CLKID_PWM_AO_C_DIV]		= &t7_pwm_ao_c_div.hw,
		[CLKID_PWM_AO_C_GATE]		= &t7_pwm_ao_c_gate.hw,
		[CLKID_PWM_AO_D_MUX]		= &t7_pwm_ao_d_mux.hw,
		[CLKID_PWM_AO_D_DIV]		= &t7_pwm_ao_d_div.hw,
		[CLKID_PWM_AO_D_GATE]		= &t7_pwm_ao_d_gate.hw,
		[CLKID_PWM_AO_E_MUX]		= &t7_pwm_ao_e_mux.hw,
		[CLKID_PWM_AO_E_DIV]		= &t7_pwm_ao_e_div.hw,
		[CLKID_PWM_AO_E_GATE]		= &t7_pwm_ao_e_gate.hw,
		[CLKID_PWM_AO_F_MUX]		= &t7_pwm_ao_f_mux.hw,
		[CLKID_PWM_AO_F_DIV]		= &t7_pwm_ao_f_div.hw,
		[CLKID_PWM_AO_F_GATE]		= &t7_pwm_ao_f_gate.hw,
		[CLKID_PWM_AO_G_MUX]		= &t7_pwm_ao_g_mux.hw,
		[CLKID_PWM_AO_G_DIV]		= &t7_pwm_ao_g_div.hw,
		[CLKID_PWM_AO_G_GATE]		= &t7_pwm_ao_g_gate.hw,
		[CLKID_PWM_AO_H_MUX]		= &t7_pwm_ao_h_mux.hw,
		[CLKID_PWM_AO_H_DIV]		= &t7_pwm_ao_h_div.hw,
		[CLKID_PWM_AO_H_GATE]		= &t7_pwm_ao_h_gate.hw,
		[CLKID_SPICC0_MUX]		= &t7_spicc0_mux.hw,
		[CLKID_SPICC0_DIV]		= &t7_spicc0_div.hw,
		[CLKID_SPICC0_GATE]		= &t7_spicc0_gate.hw,
		[CLKID_SPICC1_MUX]		= &t7_spicc1_mux.hw,
		[CLKID_SPICC1_DIV]		= &t7_spicc1_div.hw,
		[CLKID_SPICC1_GATE]		= &t7_spicc1_gate.hw,
		[CLKID_SPICC2_MUX]		= &t7_spicc2_mux.hw,
		[CLKID_SPICC2_DIV]		= &t7_spicc2_div.hw,
		[CLKID_SPICC2_GATE]		= &t7_spicc2_gate.hw,
		[CLKID_SPICC3_MUX]		= &t7_spicc3_mux.hw,
		[CLKID_SPICC3_DIV]		= &t7_spicc3_div.hw,
		[CLKID_SPICC3_GATE]		= &t7_spicc3_gate.hw,
		[CLKID_SPICC4_MUX]		= &t7_spicc4_mux.hw,
		[CLKID_SPICC4_DIV]		= &t7_spicc4_div.hw,
		[CLKID_SPICC4_GATE]		= &t7_spicc4_gate.hw,
		[CLKID_SPICC5_MUX]		= &t7_spicc5_mux.hw,
		[CLKID_SPICC5_DIV]		= &t7_spicc5_div.hw,
		[CLKID_SPICC5_GATE]		= &t7_spicc5_gate.hw,
		[CLKID_SD_EMMC_C_CLK_MUX]	= &t7_sd_emmc_c_clk0_sel.hw,
		[CLKID_SD_EMMC_C_CLK_DIV]	= &t7_sd_emmc_c_clk0_div.hw,
		[CLKID_SD_EMMC_C_CLK]		= &t7_sd_emmc_c_clk0.hw,
		[CLKID_SD_EMMC_A_CLK_MUX]	= &t7_sd_emmc_a_clk0_sel.hw,
		[CLKID_SD_EMMC_A_CLK_DIV]	= &t7_sd_emmc_a_clk0_div.hw,
		[CLKID_SD_EMMC_A_CLK]		= &t7_sd_emmc_a_clk0.hw,
		[CLKID_SD_EMMC_B_CLK_MUX]	= &t7_sd_emmc_b_clk0_sel.hw,
		[CLKID_SD_EMMC_B_CLK_DIV]	= &t7_sd_emmc_b_clk0_div.hw,
		[CLKID_SD_EMMC_B_CLK]		= &t7_sd_emmc_b_clk0.hw,
		[CLKID_DSI_A_MEAS_MUX]		= &t7_dsi_a_meas_mux.hw,
		[CLKID_DSI_A_MEAS_DIV]		= &t7_dsi_a_meas_div.hw,
		[CLKID_DSI_A_MEAS_GATE]		= &t7_dsi_a_meas_gate.hw,
		[CLKID_DSI_B_MEAS_MUX]		= &t7_dsi_b_meas_mux.hw,
		[CLKID_DSI_B_MEAS_DIV]		= &t7_dsi_b_meas_div.hw,
		[CLKID_DSI_B_MEAS_GATE]		= &t7_dsi_b_meas_gate.hw,
		[CLKID_DSI0_PHY_MUX]		= &t7_dsi0_phy_mux.hw,
		[CLKID_DSI0_PHY_DIV]		= &t7_dsi0_phy_div.hw,
		[CLKID_DSI0_PHY_GATE]		= &t7_dsi0_phy_gate.hw,
		[CLKID_DSI1_PHY_MUX]		= &t7_dsi1_phy_mux.hw,
		[CLKID_DSI1_PHY_DIV]		= &t7_dsi1_phy_div.hw,
		[CLKID_DSI1_PHY_GATE]		= &t7_dsi1_phy_gate.hw,
		[CLKID_ETH_RMII_SEL]		= &t7_eth_rmii_sel.hw,
		[CLKID_ETH_RMII_DIV]		= &t7_eth_rmii_div.hw,
		[CLKID_ETH_RMII]		= &t7_eth_rmii.hw,
		[CLKID_ETH_DIV8]		= &t7_eth_div8.hw,
		[CLKID_ETH_125M]		= &t7_eth_125m.hw,
		[CLKID_SARADC_MUX]		= &t7_saradc_mux.hw,
		[CLKID_SARADC_DIV]		= &t7_saradc_div.hw,
		[CLKID_SARADC_GATE]		= &t7_saradc_gate.hw,
		[CLKID_GEN_MUX]			= &t7_gen_sel.hw,
		[CLKID_GEN_DIV]			= &t7_gen_div.hw,
		[CLKID_GEN_GATE]		= &t7_gen.hw,

		[CLKID_DDR]			= &t7_ddr.hw,
		[CLKID_DOS]			= &t7_dos.hw,
		[CLKID_MIPI_DSI_A]		= &t7_mipi_dsi_a.hw,
		[CLKID_MIPI_DSI_B]		= &t7_mipi_dsi_b.hw,
		[CLKID_ETHPHY]			= &t7_ethphy.hw,
		[CLKID_MALI]			= &t7_mali.hw,
		[CLKID_AOCPU]			= &t7_aocpu.hw,
		[CLKID_AUCPU]			= &t7_aucpu.hw,
		[CLKID_CEC]			= &t7_cec.hw,
		[CLKID_GDC]			= &t7_gdc.hw,
		[CLKID_DESWARP]			= &t7_deswarp.hw,
		[CLKID_AMPIPE_NAND]		= &t7_ampipe_nand.hw,
		[CLKID_AMPIPE_ETH]		= &t7_ampipe_eth.hw,
		[CLKID_AM2AXI0]			= &t7_am2axi0.hw,
		[CLKID_AM2AXI1]			= &t7_am2axi1.hw,
		[CLKID_AM2AXI2]			= &t7_am2axi2.hw,
		[CLKID_SD_EMMC_A]		= &t7_sdemmca.hw,
		[CLKID_SD_EMMC_B]		= &t7_sdemmcb.hw,
		[CLKID_SD_EMMC_C]		= &t7_sdemmcc.hw,
		[CLKID_SMARTCARD]		= &t7_smartcard.hw,
		[CLKID_ACODEC]			= &t7_acodec.hw,
		[CLKID_SPIFC]			= &t7_spifc.hw,
		[CLKID_MSR_CLK]			= &t7_msr_clk.hw,
		[CLKID_IR_CTRL]			= &t7_ir_ctrl.hw,

		[CLKID_AUDIO]			= &t7_audio.hw,
		[CLKID_ETH]			= &t7_eth.hw,
		[CLKID_UART_A]			= &t7_uart_a.hw,
		[CLKID_UART_B]			= &t7_uart_b.hw,
		[CLKID_UART_C]			= &t7_uart_c.hw,
		[CLKID_UART_D]			= &t7_uart_d.hw,
		[CLKID_UART_E]			= &t7_uart_e.hw,
		[CLKID_UART_F]			= &t7_uart_f.hw,
		[CLKID_AIFIFO]			= &t7_aififo.hw,
		[CLKID_SPICC2]			= &t7_spicc2.hw,
		[CLKID_SPICC3]			= &t7_spicc3.hw,
		[CLKID_SPICC4]			= &t7_spicc4.hw,
		[CLKID_TS_A73]			= &t7_ts_a73.hw,
		[CLKID_TS_A53]			= &t7_ts_a53.hw,
		[CLKID_SPICC5]			= &t7_spicc5.hw,
		[CLKID_G2D]			= &t7_g2d.hw,
		[CLKID_SPICC0]			= &t7_spicc0.hw,
		[CLKID_SPICC1]			= &t7_spicc1.hw,
		[CLKID_PCIE]			= &t7_pcie.hw,
		[CLKID_USB]			= &t7_usb.hw,
		[CLKID_PCIE_PHY]		= &t7_pcie_phy.hw,
		[CLKID_I2C_AO_A]		= &t7_i2c_ao_a.hw,
		[CLKID_I2C_AO_B]		= &t7_i2c_ao_b.hw,
		[CLKID_I2C_M_A]			= &t7_i2c_m_a.hw,
		[CLKID_I2C_M_B]			= &t7_i2c_m_b.hw,

		[CLKID_I2C_M_C]			= &t7_i2c_m_c.hw,
		[CLKID_I2C_M_D]			= &t7_i2c_m_d.hw,
		[CLKID_I2C_M_E]			= &t7_i2c_m_e.hw,
		[CLKID_I2C_M_F]			= &t7_i2c_m_f.hw,
		[CLKID_HDMITX_APB]		= &t7_hdmitx_apb.hw,
		[CLKID_I2C_S_A]			= &t7_i2c_s_a.hw,
		[CLKID_HDMIRX_PCLK]		= &t7_hdmirx_pclk.hw,
		[CLKID_MMC_APB]			= &t7_mmc_apb.hw,
		[CLKID_MIPI_ISP_PCLK]		= &t7_mipi_isp_pclk.hw,
		[CLKID_RSA]			= &t7_rsa.hw,
		[CLKID_PCLK_SYS_CPU_APB]	= &t7_pclk_sys_cpu_apb.hw,
		[CLKID_A73PCLK_CPU_APB]		= &t7_a73pclk_cpu_apb.hw,
		[CLKID_DSPA]			= &t7_dspa.hw,
		[CLKID_DSPB]			= &t7_dspb.hw,
		[CLKID_VPU_INTR]		= &t7_vpu_intr.hw,
		[CLKID_SAR_ADC]			= &t7_sar_adc.hw,
		[CLKID_GIC]			= &t7_gic.hw,
		[CLKID_TS_GPU]			= &t7_ts_gpu.hw,
		[CLKID_TS_NNA]			= &t7_ts_nna.hw,
		[CLKID_TS_VPU]			= &t7_ts_vpu.hw,
		[CLKID_TS_HEVC]			= &t7_ts_hevc.hw,
		[CLKID_PWM_AB]			= &t7_pwm_ab.hw,
		[CLKID_PWM_CD]			= &t7_pwm_cd.hw,
		[CLKID_PWM_EF]			= &t7_pwm_ef.hw,
		[CLKID_PWM_AO_AB]		= &t7_pwm_ao_ab.hw,
		[CLKID_PWM_AO_CD]		= &t7_pwm_ao_cd.hw,
		[CLKID_PWM_AO_EF]		= &t7_pwm_ao_ef.hw,
		[CLKID_PWM_AO_GH]		= &t7_pwm_ao_gh.hw,

		[NR_CLKS]			= NULL
	},
	.num = NR_CLKS,
};

/* Convenience table to populate regmap in .probe */
static struct clk_regmap *const t7_clk_regmaps[] = {
	&t7_rtc_32k_clkin,
	&t7_rtc_32k_div,
	&t7_rtc_32k_xtal,
	&t7_rtc_32k_sel,
	&t7_rtc_clk,
	&t7_sysclk_b_sel,
	&t7_sysclk_b_div,
	&t7_sysclk_b,
	&t7_sysclk_a_sel,
	&t7_sysclk_a_div,
	&t7_sysclk_a,
	&t7_sys_clk,
	&t7_ceca_32k_clkin,
	&t7_ceca_32k_div,
	&t7_ceca_32k_sel_pre,
	&t7_ceca_32k_sel,
	&t7_ceca_32k_clkout,
	&t7_cecb_32k_clkin,
	&t7_cecb_32k_div,
	&t7_cecb_32k_sel_pre,
	&t7_cecb_32k_sel,
	&t7_cecb_32k_clkout,
	&t7_sc_clk_mux,
	&t7_sc_clk_div,
	&t7_sc_clk_gate,
	&t7_dspa_a_mux,
	&t7_dspa_a_div,
	&t7_dspa_a_gate,
	&t7_dspa_b_mux,
	&t7_dspa_b_div,
	&t7_dspa_b_gate,
	&t7_dspa_mux,
	&t7_dspb_a_mux,
	&t7_dspb_a_div,
	&t7_dspb_a_gate,
	&t7_dspb_b_mux,
	&t7_dspb_b_div,
	&t7_dspb_b_gate,
	&t7_dspb_mux,
	&t7_24M_clk_gate,
	&t7_12M_clk_gate,
	&t7_25M_clk_div,
	&t7_25M_clk_gate,
	&t7_vid_pll_div,
	&t7_vid_pll_sel,
	&t7_vid_pll,
	&t7_vclk_sel,
	&t7_vclk2_sel,
	&t7_vclk_input,
	&t7_vclk2_input,
	&t7_vclk_div,
	&t7_vclk2_div,
	&t7_vclk,
	&t7_vclk2,
	&t7_vclk_div1,
	&t7_vclk_div2_en,
	&t7_vclk_div4_en,
	&t7_vclk_div6_en,
	&t7_vclk_div12_en,
	&t7_vclk2_div1,
	&t7_vclk2_div2_en,
	&t7_vclk2_div4_en,
	&t7_vclk2_div6_en,
	&t7_vclk2_div12_en,
	&t7_cts_enci_sel,
	&t7_cts_encp_sel,
	&t7_cts_vdac_sel,
	&t7_hdmi_tx_sel,
	&t7_cts_enci,
	&t7_cts_encp,
	&t7_cts_vdac,
	&t7_hdmi_tx,
	&t7_hdmitx_sys_sel,
	&t7_hdmitx_sys_div,
	&t7_hdmitx_sys,
	&t7_hdmitx_prif_sel,
	&t7_hdmitx_prif_div,
	&t7_hdmitx_prif,
	&t7_hdmitx_200m_sel,
	&t7_hdmitx_200m_div,
	&t7_hdmitx_200m,
	&t7_hdmitx_aud_sel,
	&t7_hdmitx_aud_div,
	&t7_hdmitx_aud,
	&t7_hdmirx_5m_sel,
	&t7_hdmirx_5m_div,
	&t7_hdmirx_5m,
	&t7_hdmirx_2m_sel,
	&t7_hdmirx_2m_div,
	&t7_hdmirx_2m,
	&t7_hdmirx_cfg_sel,
	&t7_hdmirx_cfg_div,
	&t7_hdmirx_cfg,
	&t7_hdmirx_hdcp_sel,
	&t7_hdmirx_hdcp_div,
	&t7_hdmirx_hdcp,
	&t7_hdmirx_aud_pll_sel,
	&t7_hdmirx_aud_pll_div,
	&t7_hdmirx_aud_pll,
	&t7_hdmirx_acr_sel,
	&t7_hdmirx_acr_div,
	&t7_hdmirx_acr,
	&t7_hdmirx_meter_sel,
	&t7_hdmirx_meter_div,
	&t7_hdmirx_meter,
	&t7_ts_clk_div,
	&t7_ts_clk_gate,
	&t7_mali_0_sel,
	&t7_mali_0_div,
	&t7_mali_0,
	&t7_mali_1_sel,
	&t7_mali_1_div,
	&t7_mali_1,
	&t7_mali_mux,
	&t7_vdec_p0_mux,
	&t7_vdec_p0_div,
	&t7_vdec_p0,
	&t7_vdec_p1_mux,
	&t7_vdec_p1_div,
	&t7_vdec_p1,
	&t7_vdec_mux,
	&t7_hcodec_p0_mux,
	&t7_hcodec_p0_div,
	&t7_hcodec_p0,
	&t7_hcodec_p1_mux,
	&t7_hcodec_p1_div,
	&t7_hcodec_p1,
	&t7_hcodec_mux,
	&t7_hevcb_p0_mux,
	&t7_hevcb_p0_div,
	&t7_hevcb_p0,
	&t7_hevcb_p1_mux,
	&t7_hevcb_p1_div,
	&t7_hevcb_p1,
	&t7_hevcb_mux,
	&t7_hevcf_p0_mux,
	&t7_hevcf_p0_div,
	&t7_hevcf_p0,
	&t7_hevcf_p1_mux,
	&t7_hevcf_p1_div,
	&t7_hevcf_p1,
	&t7_hevcf_mux,
	&t7_wave_a_sel,
	&t7_wave_a_div,
	&t7_wave_aclk,
	&t7_wave_b_sel,
	&t7_wave_b_div,
	&t7_wave_bclk,
	&t7_wave_c_sel,
	&t7_wave_c_div,
	&t7_wave_cclk,
	&t7_mipi_isp_sel,
	&t7_mipi_isp_div,
	&t7_mipi_isp,
	&t7_mipi_csi_phy_sel0,
	&t7_mipi_csi_phy_div0,
	&t7_mipi_csi_phy0,
	&t7_mipi_csi_phy_sel1,
	&t7_mipi_csi_phy_div1,
	&t7_mipi_csi_phy1,
	&t7_mipi_csi_phy_clk,
	&t7_vpu_0_sel,
	&t7_vpu_0_div,
	&t7_vpu_0,
	&t7_vpu_1_sel,
	&t7_vpu_1_div,
	&t7_vpu_1,
	&t7_vpu,
	&t7_vpu_clkb_tmp_mux,
	&t7_vpu_clkb_tmp_div,
	&t7_vpu_clkb_tmp,
	&t7_vpu_clkb_div,
	&t7_vpu_clkb,
	&t7_vpu_clkc_p0_mux,
	&t7_vpu_clkc_p0_div,
	&t7_vpu_clkc_p0,
	&t7_vpu_clkc_p1_mux,
	&t7_vpu_clkc_p1_div,
	&t7_vpu_clkc_p1,
	&t7_vpu_clkc_mux,
	&t7_vapb_0_sel,
	&t7_vapb_0_div,
	&t7_vapb_0,
	&t7_vapb_1_sel,
	&t7_vapb_1_div,
	&t7_vapb_1,
	&t7_vapb,
	&t7_gdcclk_0_sel,
	&t7_gdcclk_0_div,
	&t7_gdcclk_0,
	&t7_gdcclk_1_sel,
	&t7_gdcclk_1_div,
	&t7_gdcclk_1,
	&t7_gdcclk,
	&t7_gdc_clk,
	&t7_dewarpclk_0_sel,
	&t7_dewarpclk_0_div,
	&t7_dewarpclk_0,
	&t7_dewarpclk_1_sel,
	&t7_dewarpclk_1_div,
	&t7_dewarpclk_1,
	&t7_dewarpclk,
	&t7_dewarp_clk,
	&t7_anakin_0_sel,
	&t7_anakin_0_div,
	&t7_anakin_0,
	&t7_anakin_1_sel,
	&t7_anakin_1_div,
	&t7_anakin_1,
	&t7_anakin,
	&t7_anakin_clk,
	&t7_ge2d_gate,
	&t7_vdin_meas_mux,
	&t7_vdin_meas_div,
	&t7_vdin_meas_gate,
	&t7_vid_lock_div,
	&t7_vid_lock_clk,
	&t7_pwm_a_mux,
	&t7_pwm_a_div,
	&t7_pwm_a_gate,
	&t7_pwm_b_mux,
	&t7_pwm_b_div,
	&t7_pwm_b_gate,
	&t7_pwm_c_mux,
	&t7_pwm_c_div,
	&t7_pwm_c_gate,
	&t7_pwm_d_mux,
	&t7_pwm_d_div,
	&t7_pwm_d_gate,
	&t7_pwm_e_mux,
	&t7_pwm_e_div,
	&t7_pwm_e_gate,
	&t7_pwm_f_mux,
	&t7_pwm_f_div,
	&t7_pwm_f_gate,
	&t7_pwm_ao_a_mux,
	&t7_pwm_ao_a_div,
	&t7_pwm_ao_a_gate,
	&t7_pwm_ao_b_mux,
	&t7_pwm_ao_b_div,
	&t7_pwm_ao_b_gate,
	&t7_pwm_ao_c_mux,
	&t7_pwm_ao_c_div,
	&t7_pwm_ao_c_gate,
	&t7_pwm_ao_d_mux,
	&t7_pwm_ao_d_div,
	&t7_pwm_ao_d_gate,
	&t7_pwm_ao_e_mux,
	&t7_pwm_ao_e_div,
	&t7_pwm_ao_e_gate,
	&t7_pwm_ao_f_mux,
	&t7_pwm_ao_f_div,
	&t7_pwm_ao_f_gate,
	&t7_pwm_ao_g_mux,
	&t7_pwm_ao_g_div,
	&t7_pwm_ao_g_gate,
	&t7_pwm_ao_h_mux,
	&t7_pwm_ao_h_div,
	&t7_pwm_ao_h_gate,
	&t7_spicc0_mux,
	&t7_spicc0_div,
	&t7_spicc0_gate,
	&t7_spicc1_mux,
	&t7_spicc1_div,
	&t7_spicc1_gate,
	&t7_spicc2_mux,
	&t7_spicc2_div,
	&t7_spicc2_gate,
	&t7_spicc3_mux,
	&t7_spicc3_div,
	&t7_spicc3_gate,
	&t7_spicc4_mux,
	&t7_spicc4_div,
	&t7_spicc4_gate,
	&t7_spicc5_mux,
	&t7_spicc5_div,
	&t7_spicc5_gate,
	&t7_sd_emmc_c_clk0_sel,
	&t7_sd_emmc_c_clk0_div,
	&t7_sd_emmc_c_clk0,
	&t7_sd_emmc_a_clk0_sel,
	&t7_sd_emmc_a_clk0_div,
	&t7_sd_emmc_a_clk0,
	&t7_sd_emmc_b_clk0_sel,
	&t7_sd_emmc_b_clk0_div,
	&t7_sd_emmc_b_clk0,
	&t7_eth_rmii_sel,
	&t7_eth_rmii_div,
	&t7_eth_rmii,
	&t7_eth_125m,
	&t7_dsi_a_meas_mux,
	&t7_dsi_a_meas_div,
	&t7_dsi_a_meas_gate,
	&t7_dsi_b_meas_mux,
	&t7_dsi_b_meas_div,
	&t7_dsi_b_meas_gate,
	&t7_dsi0_phy_mux,
	&t7_dsi0_phy_div,
	&t7_dsi0_phy_gate,
	&t7_dsi1_phy_mux,
	&t7_dsi1_phy_div,
	&t7_dsi1_phy_gate,
	&t7_saradc_mux,
	&t7_saradc_div,
	&t7_saradc_gate,
	&t7_gen_sel,
	&t7_gen_div,
	&t7_gen,

	&t7_ddr,
	&t7_dos,
	&t7_mipi_dsi_a,
	&t7_mipi_dsi_b,
	&t7_ethphy,
	&t7_mali,
	&t7_aocpu,
	&t7_aucpu,
	&t7_cec,
	&t7_gdc,
	&t7_deswarp,
	&t7_ampipe_nand,
	&t7_ampipe_eth,
	&t7_am2axi0,
	&t7_am2axi1,
	&t7_am2axi2,
	&t7_sdemmca,
	&t7_sdemmcb,
	&t7_sdemmcc,
	&t7_smartcard,
	&t7_acodec,
	&t7_spifc,
	&t7_msr_clk,
	&t7_ir_ctrl,
	&t7_audio,
	&t7_eth,
	&t7_uart_a,
	&t7_uart_b,
	&t7_uart_c,
	&t7_uart_d,
	&t7_uart_e,
	&t7_uart_f,
	&t7_aififo,
	&t7_spicc2,
	&t7_spicc3,
	&t7_spicc4,
	&t7_ts_a73,
	&t7_ts_a53,
	&t7_spicc5,
	&t7_g2d,
	&t7_spicc0,
	&t7_spicc1,
	&t7_pcie,
	&t7_usb,
	&t7_pcie_phy,
	&t7_i2c_ao_a,
	&t7_i2c_ao_b,
	&t7_i2c_m_a,
	&t7_i2c_m_b,
	&t7_i2c_m_c,
	&t7_i2c_m_d,
	&t7_i2c_m_e,
	&t7_i2c_m_f,
	&t7_hdmitx_apb,
	&t7_i2c_s_a,
	&t7_hdmirx_pclk,
	&t7_mmc_apb,
	&t7_mipi_isp_pclk,
	&t7_rsa,
	&t7_pclk_sys_cpu_apb,
	&t7_a73pclk_cpu_apb,
	&t7_dspa,
	&t7_dspb,
	&t7_vpu_intr,
	&t7_sar_adc,
	&t7_gic,
	&t7_ts_gpu,
	&t7_ts_nna,
	&t7_ts_vpu,
	&t7_ts_hevc,
	&t7_pwm_ab,
	&t7_pwm_cd,
	&t7_pwm_ef,
	&t7_pwm_ao_ab,
	&t7_pwm_ao_cd,
	&t7_pwm_ao_ef,
	&t7_pwm_ao_gh,
};

static struct clk_regmap *const t7_cpu_clk_regmaps[] = {
	&t7_cpu_dyn_clk,
	&t7_cpu_clk,
	&t7_a73_dyn_clk,
	&t7_a73_clk,
/*
 *	&t7_cpu1_clk,
 *	&t7_cpu2_clk,
 *	&t7_cpu3_clk
 */
};

static struct clk_regmap *const t7_pll_clk_regmaps[] = {
	&t7_fixed_pll_dco,
	&t7_fixed_pll,
	&t7_sys_pll_dco,
	&t7_sys_pll,
	&t7_sys1_pll_dco,
	&t7_sys1_pll,
	&t7_fclk_div2,
	&t7_fclk_div3,
	&t7_fclk_div4,
	&t7_fclk_div5,
	&t7_fclk_div7,
	&t7_fclk_div2p5,
	&t7_gp0_pll_dco,
	&t7_gp0_pll,
	&t7_gp1_pll_dco,
	&t7_gp1_pll,
	&t7_mclk_pll_dco,
	&t7_mclk_pre_od,
	&t7_mclk_pll,
	&t7_hifi_pll_dco,
	&t7_hifi_pll,
	&t7_pcie_pll_dco,
	&t7_pcie_pll_od,
	&t7_pcie_hcsl,
	&t7_hdmi_pll_dco,
	&t7_hdmi_pll_od,
	&t7_hdmi_pll,
	&t7_mpll_50m,
	&t7_mpll0_div,
	&t7_mpll0,
	&t7_mpll1_div,
	&t7_mpll1,
	&t7_mpll2_div,
	&t7_mpll2,
	&t7_mpll3_div,
	&t7_mpll3,
	&t7_mclk_0_sel,
	&t7_mclk_0_pre,
	&t7_mclk_0,
	&t7_mclk_1_sel,
	&t7_mclk_1_pre,
	&t7_mclk_1
};

static int meson_t7_dvfs_setup(struct platform_device *pdev)
{
	int ret;

	/* Setup cluster 0 clock notifier for sys_pll */
	ret = clk_notifier_register(t7_sys_pll.hw.clk,
				    &t7_sys_pll_nb_data.nb);
	if (ret) {
		dev_err(&pdev->dev, "failed to register sys_pll notifier\n");
		return ret;
	}
	/* Setup cluster 1 clock notifier for sys1_pll */
	ret = clk_notifier_register(t7_sys1_pll.hw.clk,
				    &t7_sys1_pll_nb_data.nb);
	if (ret) {
		dev_err(&pdev->dev, "failed to register sys1_pll notifier\n");
		return ret;
	}

	return 0;
}

static struct regmap_config clkc_regmap_config = {
	.reg_bits       = 32,
	.val_bits       = 32,
	.reg_stride     = 4,
};

static struct regmap *t7_regmap_resource(struct device *dev, char *name)
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

static int meson_t7_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct regmap *basic_map;
	struct regmap *pll_map;
	struct regmap *cpu_clk_map;
	int ret, i;

	/* Get regmap for different clock area */
	basic_map = t7_regmap_resource(dev, "basic");
	if (IS_ERR(basic_map)) {
		dev_err(dev, "basic clk registers not found\n");
		return PTR_ERR(basic_map);
	}

	pll_map = t7_regmap_resource(dev, "pll");
	if (IS_ERR(pll_map)) {
		dev_err(dev, "pll clk registers not found\n");
		return PTR_ERR(pll_map);
	}

	cpu_clk_map = t7_regmap_resource(dev, "cpu_clk");
	if (IS_ERR(cpu_clk_map)) {
		dev_err(dev, "cpu clk registers not found\n");
		return PTR_ERR(cpu_clk_map);
	}

	/* Populate regmap for the regmap backed clocks */
	for (i = 0; i < ARRAY_SIZE(t7_clk_regmaps); i++)
		t7_clk_regmaps[i]->map = basic_map;

	for (i = 0; i < ARRAY_SIZE(t7_cpu_clk_regmaps); i++)
		t7_cpu_clk_regmaps[i]->map = cpu_clk_map;

	for (i = 0; i < ARRAY_SIZE(t7_pll_clk_regmaps); i++)
		t7_pll_clk_regmaps[i]->map = pll_map;
	regmap_write(pll_map, ANACTRL_MPLL_CTRL0, 0x00000543);

	for (i = 0; i < t7_hw_onecell_data.num; i++) {
		/* array might be sparse */
		if (!t7_hw_onecell_data.hws[i])
			continue;

		ret = devm_clk_hw_register(dev, t7_hw_onecell_data.hws[i]);
		if (ret) {
			dev_err(dev, "Clock registration failed\n");
			return ret;
		}
	}

	meson_t7_dvfs_setup(pdev);

	return devm_of_clk_add_hw_provider(dev, of_clk_hw_onecell_get,
					   &t7_hw_onecell_data);
}

static const struct of_device_id clkc_match_table[] = {
	{
		.compatible = "amlogic,t7-clkc"
	},
	{}
};

static struct platform_driver t7_driver = {
	.probe		= meson_t7_probe,
	.driver		= {
		.name	= "t7-clkc",
		.of_match_table = clkc_match_table,
	},
};

#ifndef CONFIG_AMLOGIC_MODIFY
builtin_platform_driver(t7_driver);
#else
#ifndef MODULE
static int t7_clkc_init(void)
{
	return platform_driver_register(&t7_driver);
}
arch_initcall_sync(t7_clkc_init);
#else
int __init meson_t7_clkc_init(void)
{
	return platform_driver_register(&t7_driver);
}
#endif
#endif

MODULE_LICENSE("GPL v2");
