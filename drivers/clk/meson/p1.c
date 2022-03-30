// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
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
#include "p1.h"
#include "clkcs_init.h"
#include <dt-bindings/clock/p1-clkc.h>
#include "clk-secure.h"

static DEFINE_SPINLOCK(meson_clk_lock);

static const struct clk_ops meson_pll_clk_no_ops = {};

/*
 * the sys pll DCO value should be 3G~6G,
 * otherwise the sys pll can not lock.
 * od is for 32 bit.
 */

#ifdef CONFIG_ARM
static const struct pll_params_table p1_sys_pll_params_table[] = {
	PLL_PARAMS(100, 1, 1), /*DCO=2400M OD=1200M*/
	PLL_PARAMS(116, 1, 1), /*DCO=2784 OD=1392M*/
	PLL_PARAMS(126, 1, 1), /*DCO=3024 OD=1512M*/
	PLL_PARAMS(67, 1, 0), /*DCO=1608M OD=1608MM*/
	PLL_PARAMS(71, 1, 0), /*DCO=1704MM OD=1704M*/
	PLL_PARAMS(75, 1, 0), /*DCO=1800M OD=1800M*/
	PLL_PARAMS(79, 1, 0), /*DCO=1896M OD=1896M*/
	PLL_PARAMS(83, 1, 0), /*DCO=1992M OD=1992M*/
	{ /* sentinel */ }
};
#else
static const struct pll_params_table p1_sys_pll_params_table[] = {
	PLL_PARAMS(100, 1), /*DCO=2400M OD=1200M*/
	PLL_PARAMS(116, 1), /*DCO=2784 OD=1392M*/
	PLL_PARAMS(126, 1), /*DCO=3024 OD=1512M*/
	PLL_PARAMS(67, 1), /*DCO=1608M OD=1608MM*/
	PLL_PARAMS(71, 1), /*DCO=1704MM OD=1704M*/
	PLL_PARAMS(75, 1), /*DCO=1800M OD=1800M*/
	PLL_PARAMS(79, 1), /*DCO=1896M OD=1896M*/
	PLL_PARAMS(83, 1), /*DCO=1992M OD=1992M*/
	{ /* sentinel */ }
};
#endif

static struct clk_regmap p1_sys_pll_dco = {
	.data = &(struct meson_clk_pll_data){
		.en = {
			.reg_off = CLKCTRL_SYS0PLL_CTRL0,
			.shift   = 28,
			.width   = 1,
		},
		.m = {
			.reg_off = CLKCTRL_SYS0PLL_CTRL0,
			.shift   = 0,
			.width   = 8,
		},
		.n = {
			.reg_off = CLKCTRL_SYS0PLL_CTRL0,
			.shift   = 16,
			.width   = 5,
		},
#ifdef CONFIG_ARM
		/* od for 32bit */
		.od = {
			.reg_off = CLKCTRL_SYS0PLL_CTRL0,
			.shift	 = 12,
			.width	 = 3,
		},
#endif
		.table = p1_sys_pll_params_table,
		.l = {
			.reg_off = CLKCTRL_SYS0PLL_STS,
			.shift   = 31,
			.width   = 1,
		},
		.rst = {
			.reg_off = CLKCTRL_SYS0PLL_CTRL0,
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
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "xtal",
		},
		.num_parents = 1,
		/*
		 * This clock feeds the CPU, avoid disabling it
		 * Register has the risk of being directly operated
		 */
		.flags = CLK_IS_CRITICAL | CLK_GET_RATE_NOCACHE,
	},
};

#ifdef CONFIG_ARM
static const struct pll_params_table p1_sys1_pll_params_table[] = {
	PLL_PARAMS(100, 1, 1), /*DCO=2400M OD=1200M*/
	PLL_PARAMS(116, 1, 1), /*DCO=2784 OD=1392M*/
	PLL_PARAMS(126, 1, 1), /*DCO=3024 OD=1512M*/
	PLL_PARAMS(67, 1, 0), /*DCO=1608M OD=1608MM*/
	PLL_PARAMS(71, 1, 0), /*DCO=1704MM OD=1704M*/
	PLL_PARAMS(75, 1, 0), /*DCO=1800M OD=1800M*/
	PLL_PARAMS(79, 1, 0), /*DCO=1896M OD=1896M*/
	PLL_PARAMS(81, 1, 0), /*DCO=1944M OD=1944M*/
	{ /* sentinel */ }
};
#else
static const struct pll_params_table p1_sys1_pll_params_table[] = {
	PLL_PARAMS(100, 1), /*DCO=2400M OD=1200M*/
	PLL_PARAMS(116, 1), /*DCO=2784 OD=1392M*/
	PLL_PARAMS(126, 1), /*DCO=3024 OD=1512M*/
	PLL_PARAMS(67, 1), /*DCO=1608M OD=1608MM*/
	PLL_PARAMS(71, 1), /*DCO=1704MM OD=1704M*/
	PLL_PARAMS(75, 1), /*DCO=1800M OD=1800M*/
	PLL_PARAMS(79, 1), /*DCO=1896M OD=1896M*/
	PLL_PARAMS(81, 1), /*DCO=1944M OD=1944M*/
	{ /* sentinel */ }
};
#endif

static struct clk_regmap p1_sys1_pll_dco = {
	.data = &(struct meson_clk_pll_data){
		.en = {
			.reg_off = CLKCTRL_SYS1PLL_CTRL0,
			.shift   = 28,
			.width   = 1,
		},
		.m = {
			.reg_off = CLKCTRL_SYS1PLL_CTRL0,
			.shift   = 0,
			.width   = 8,
		},
		.n = {
			.reg_off = CLKCTRL_SYS1PLL_CTRL0,
			.shift   = 16,
			.width   = 5,
		},
#ifdef CONFIG_ARM
		/* od for 32bit */
		.od = {
			.reg_off = CLKCTRL_SYS1PLL_CTRL0,
			.shift	 = 12,
			.width	 = 3,
		},
#endif
		.table = p1_sys1_pll_params_table,
		.l = {
			.reg_off = CLKCTRL_SYS1PLL_STS,
			.shift   = 31,
			.width   = 1,
		},
		.rst = {
			.reg_off = CLKCTRL_SYS1PLL_CTRL0,
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
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "xtal",
		},
		.num_parents = 1,
		/*
		 * Register has the risk of being directly operated
		 */
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
 *    it will be a lot of mass because of unit deferentces.
 *
 * Keep Consistent with 64bit, creat a Virtual clock for sys pll
 */
static struct clk_regmap p1_sys_pll = {
	.hw.init = &(struct clk_init_data){
		.name = "sys_pll",
		.ops = &meson_pll_clk_no_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_sys_pll_dco.hw
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

static struct clk_regmap p1_sys1_pll = {
	.hw.init = &(struct clk_init_data){
		.name = "sys1_pll",
		.ops = &meson_pll_clk_no_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_sys1_pll_dco.hw
		},
		.num_parents = 1,
		/*
		 * sys1 pll may be initialized in the bootloader
		 * CLK_IGNORE_UNUSED is needed to prevent the system
		 * hang up which will be called by clk_disable_unused
		 */
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};
#else
static struct clk_regmap p1_sys_pll = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_SYS0PLL_CTRL0,
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
			&p1_sys_pll_dco.hw
		},
		.num_parents = 1,
		/*
		 * sys pll is used for cpu frequency, the parent
		 * rate needs to be modified
		 * Register has the risk of being directly operated
		 */
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap p1_sys1_pll = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_SYS1PLL_CTRL0,
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
			&p1_sys1_pll_dco.hw
		},
		.num_parents = 1,
		/*
		 * sys pll is used for other module, the parent
		 * rate needs to be modified
		 * Register has the risk of being directly operated
		 */
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
	},
};
#endif

static struct clk_regmap p1_fixed_pll_dco = {
	.data = &(struct meson_clk_pll_data){
		.en = {
			.reg_off = CLKCTRL_FIXPLL_CTRL0,
			.shift   = 28,
			.width   = 1,
		},
		.m = {
			.reg_off = CLKCTRL_FIXPLL_CTRL0,
			.shift   = 0,
			.width   = 9,
		},
		.n = {
			.reg_off = CLKCTRL_FIXPLL_CTRL0,
			.shift   = 10,
			.width   = 5,
		},
		.frac = {
			.reg_off = CLKCTRL_FIXPLL_CTRL1,
			.shift   = 0,
			.width   = 19,
		},
		.l = {
			.reg_off = CLKCTRL_FIXPLL_CTRL0,
			.shift   = 31,
			.width   = 1,
		},
		.rst = {
			.reg_off = CLKCTRL_FIXPLL_CTRL0,
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
		/*
		 * This clock feeds the sysytem, avoid disabling it
		 * Register has the risk of being directly operated
		 */
		.flags = CLK_IS_CRITICAL | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap p1_fixed_pll = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_FIXPLL_CTRL0,
		.shift = 16,
		.width = 2,
		.flags = CLK_DIVIDER_POWER_OF_TWO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "fixed_pll",
		.ops = &clk_regmap_divider_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_fixed_pll_dco.hw
		},
		.num_parents = 1,
		/*
		 * This clock won't ever change at runtime so
		 * CLK_SET_RATE_PARENT is not required
		 * Never close , Register may be rewritten
		 */
		.flags = CLK_IS_CRITICAL | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_fixed_factor p1_fclk_div2_div = {
	.mult = 1,
	.div = 2,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div2_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) { &p1_fixed_pll.hw },
		.num_parents = 1,
	},
};

static struct clk_regmap p1_fclk_div2 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_FIXPLL_CTRL1,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div2",
		.ops = &clk_regmap_gate_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_fclk_div2_div.hw
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
	},
};

static struct clk_fixed_factor p1_fclk_div3_div = {
	.mult = 1,
	.div = 3,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div3_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) { &p1_fixed_pll.hw },
		.num_parents = 1,
	},
};

static struct clk_regmap p1_fclk_div3 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_FIXPLL_CTRL1,
		.bit_idx = 20,
	},
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div3",
		.ops = &clk_regmap_gate_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_fclk_div3_div.hw
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
	},
};

static struct clk_fixed_factor p1_fclk_div4_div = {
	.mult = 1,
	.div = 4,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div4_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) { &p1_fixed_pll.hw },
		.num_parents = 1,
	},
};

static struct clk_regmap p1_fclk_div4 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_FIXPLL_CTRL1,
		.bit_idx = 21,
	},
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div4",
		.ops = &clk_regmap_gate_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_fclk_div4_div.hw
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
	},
};

static struct clk_fixed_factor p1_fclk_div5_div = {
	.mult = 1,
	.div = 5,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div5_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) { &p1_fixed_pll.hw },
		.num_parents = 1,
	},
};

static struct clk_regmap p1_fclk_div5 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_FIXPLL_CTRL1,
		.bit_idx = 22,
	},
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div5",
		.ops = &clk_regmap_gate_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_fclk_div5_div.hw
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
	},
};

static struct clk_fixed_factor p1_fclk_div7_div = {
	.mult = 1,
	.div = 7,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div7_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) { &p1_fixed_pll.hw },
		.num_parents = 1,
	},
};

static struct clk_regmap p1_fclk_div7 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_FIXPLL_CTRL1,
		.bit_idx = 23,
	},
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div7",
		.ops = &clk_regmap_gate_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_fclk_div7_div.hw
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
	},
};

static struct clk_fixed_factor p1_fclk_div2p5_div = {
	.mult = 2,
	.div = 5,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div2p5_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_fixed_pll.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap p1_fclk_div2p5 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_FIXPLL_CTRL1,
		.bit_idx = 25,
	},
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div2p5",
		.ops = &clk_regmap_gate_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_fclk_div2p5_div.hw
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
	},
};

#ifdef CONFIG_ARM
static const struct pll_params_table p1_gp0_pll_table[] = {
	PLL_PARAMS(141, 1, 2), /* DCO = 3384M OD = 2 PLL = 846M */
	PLL_PARAMS(128, 1, 2), /* DCO = 3072M OD = 2 PLL = 768M */
	PLL_PARAMS(132, 1, 2), /* DCO = 3168M OD = 2 PLL = 792M */
	PLL_PARAMS(248, 1, 3), /* DCO = 5952M OD = 3 PLL = 744M */
	PLL_PARAMS(192, 1, 2), /* DCO = 4608M OD = 4 PLL = 1152M */
	{ /* sentinel */  }
};
#else
static const struct pll_params_table p1_gp0_pll_table[] = {
	PLL_PARAMS(141, 1), /* DCO = 3384M OD = 2 PLL = 846M */
	PLL_PARAMS(132, 1), /* DCO = 3168M OD = 2 PLL = 792M */
	PLL_PARAMS(128, 1), /* DCO = 3072M OD = 2 PLL = 768M */
	PLL_PARAMS(248, 1), /* DCO = 5952M OD = 3 PLL = 744M */
	PLL_PARAMS(192, 1), /* DCO = 4608M OD = 4 PLL = 1152M */
	{ /* sentinel */  }
};
#endif

/*
 * Internal gp0 pll emulation configuration parameters
 */
static const struct reg_sequence p1_gp0_init_regs[] = {
	{ .reg = CLKCTRL_GP0PLL_CTRL1,	.def = 0x00000000 },
	{ .reg = CLKCTRL_GP0PLL_CTRL2,	.def = 0x00000000 },
	{ .reg = CLKCTRL_GP0PLL_CTRL3,	.def = 0x48681c00 },
	{ .reg = CLKCTRL_GP0PLL_CTRL4,	.def = 0x88770290 },
	{ .reg = CLKCTRL_GP0PLL_CTRL5,	.def = 0x3927200a },
	{ .reg = CLKCTRL_GP0PLL_CTRL6,	.def = 0x56540000 }
};

static struct clk_regmap p1_gp0_pll_dco = {
	.data = &(struct meson_clk_pll_data){
		.en = {
			.reg_off = CLKCTRL_GP0PLL_CTRL0,
			.shift   = 28,
			.width   = 1,
		},
		.m = {
			.reg_off = CLKCTRL_GP0PLL_CTRL0,
			.shift   = 0,
			.width   = 9,
		},
		.n = {
			.reg_off = CLKCTRL_GP0PLL_CTRL0,
			.shift   = 10,
			.width   = 5,
		},
#ifdef CONFIG_ARM
		/* for 32bit */
		.od = {
			.reg_off = CLKCTRL_GP0PLL_CTRL0,
			.shift	 = 16,
			.width	 = 3,
		},
#endif
		.frac = {
			.reg_off = CLKCTRL_GP0PLL_CTRL1,
			.shift   = 0,
			.width   = 19,
		},
		.l = {
			.reg_off = CLKCTRL_GP0PLL_CTRL0,
			.shift   = 31,
			.width   = 1,
		},
		.rst = {
			.reg_off = CLKCTRL_GP0PLL_CTRL0,
			.shift   = 29,
			.width   = 1,
		},
		.table = p1_gp0_pll_table,
		.init_regs = p1_gp0_init_regs,
		.init_count = ARRAY_SIZE(p1_gp0_init_regs),
	},
	.hw.init = &(struct clk_init_data){
		.name = "gp0_pll_dco",
		.ops = &meson_clk_pll_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "xtal",
		},
		.num_parents = 1,
		/*
		 * Register has the risk of being directly operated
		 */
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

#ifdef CONFIG_ARM
static struct clk_regmap p1_gp0_pll = {
	.hw.init = &(struct clk_init_data){
		.name = "gp0_pll",
		.ops = &meson_pll_clk_no_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_gp0_pll_dco.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};
#else
static struct clk_regmap p1_gp0_pll = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_GP0PLL_CTRL0,
		.shift = 16,
		.width = 3,
		.flags = (CLK_DIVIDER_POWER_OF_TWO |
			  CLK_DIVIDER_ROUND_CLOSEST),
	},
	.hw.init = &(struct clk_init_data){
		.name = "gp0_pll",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_gp0_pll_dco.hw
		},
		.num_parents = 1,
		/*
		 * gpo pll is directly used in other modules, and the
		 * parent rate needs to be modified
		 * Register has the risk of being directly operated.
		 */
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
	},
};
#endif

#ifdef CONFIG_ARM
static const struct pll_params_table p1_gp1_pll_table[] = {
	PLL_PARAMS(100, 1, 1), /*DCO=2400M OD=1200M*/
	PLL_PARAMS(125, 1, 1), /*DCO=3000M OD=1500M*/
	{ /* sentinel */  }
};
#else
static const struct pll_params_table p1_gp1_pll_table[] = {
	PLL_PARAMS(100, 1), /*DCO=2400M OD=1200M*/
	PLL_PARAMS(125, 1), /*DCO=3000M OD=1500M*/
	{ /* sentinel */  }
};
#endif
/* TODO: need check */
static struct clk_regmap p1_gp1_pll_dco = {
	.data = &(struct meson_clk_pll_data){
		.en = {
			.reg_off = CLKCTRL_GP1PLL_CTRL0,
			.shift   = 28,
			.width   = 1,
		},
		.m = {
			.reg_off = CLKCTRL_GP1PLL_CTRL0,
			.shift   = 0,
			.width   = 8,
		},
		.n = {
			.reg_off = CLKCTRL_GP1PLL_CTRL0,
			.shift   = 16,
			.width   = 5,
		},
		.l = {
			.reg_off = CLKCTRL_GP1PLL_STS,
			.shift   = 31,
			.width   = 1,
		},
		.rst = {
			.reg_off = CLKCTRL_GP1PLL_CTRL0,
			.shift   = 29,
			.width   = 1,
		},
		.table = p1_gp1_pll_table,
		.smc_id = SECURE_PLL_CLK,
		.secid_disable = SECID_GP1_DCO_PLL_DIS,
		.secid = SECID_GP1_DCO_PLL,
	},
	.hw.init = &(struct clk_init_data){
		.name = "gp1_pll_dco",
		.ops = &meson_secure_pll_v2_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "xtal",
		},
		.num_parents = 1,
		/*
		 * Register has the risk of being directly operated
		 */
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap p1_gp1_pll = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_GP1PLL_CTRL0,
		.shift = 12,
		.width = 3,
		.flags = CLK_DIVIDER_POWER_OF_TWO,
		.smc_id = SECURE_PLL_CLK,
		.secid = SECID_GP1_PLL_OD
	},
	.hw.init = &(struct clk_init_data){
		.name = "gp1_pll",
		.ops = &clk_regmap_secure_v2_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_gp1_pll_dco.hw
		},
		.num_parents = 1,
		/*
		 * gp1 pll is directly used in other modules, and the
		 * parent rate needs to be modified
		 * Register has the risk of being directly operated.
		 */
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
	},
};

#ifdef CONFIG_ARM
static const struct pll_params_table p1_fdle_pll_table[] = {
	PLL_PARAMS(100, 1, 1, /*DCO=2400M OD=1200M*/
	PLL_PARAMS(125, 1, 1), /*DCO=3000M OD=1500M*/
	{ /* sentinel */  }
};
#else
static const struct pll_params_table p1_fdle_pll_table[] = {
	PLL_PARAMS(100, 1), /*DCO=2400M OD=1200M*/
	PLL_PARAMS(125, 1), /*DCO=3000M OD=1500M*/
	{ /* sentinel */  }
};
#endif

static const struct reg_sequence p1_fdle_pll_init_regs[] = {
	{ .reg = CLKCTRL_FDLEPLL_CTRL0, .def = 0x20011086 },
	{ .reg = CLKCTRL_FDLEPLL_CTRL0, .def = 0x30011086 },
	{ .reg = CLKCTRL_FDLEPLL_CTRL1, .def = 0x1420500f },
	{ .reg = CLKCTRL_FDLEPLL_CTRL2, .def = 0x00023041 },
	{ .reg = CLKCTRL_FDLEPLL_CTRL3, .def = 0x0, .delay_us = 50 },
	{ .reg = CLKCTRL_FDLEPLL_CTRL0, .def = 0x10011086, .delay_us = 20 },
	{ .reg = CLKCTRL_FDLEPLL_CTRL2, .def = 0x00023001, .delay_us = 50 }
};

static struct clk_regmap p1_fdle_pll_dco = {
	.data = &(struct meson_clk_pll_data){
		.en = {
			.reg_off = CLKCTRL_FDLEPLL_CTRL0,
			.shift   = 28,
			.width   = 1,
		},
		.m = {
			.reg_off = CLKCTRL_FDLEPLL_CTRL0,
			.shift   = 0,
			.width   = 8,
		},
		.n = {
			.reg_off = CLKCTRL_FDLEPLL_CTRL0,
			.shift   = 16,
			.width   = 5,
		},
		.l = {
			.reg_off = CLKCTRL_FDLEPLL_STS,
			.shift   = 31,
			.width   = 1,
		},
		.rst = {
			.reg_off = CLKCTRL_FDLEPLL_CTRL0,
			.shift   = 29,
			.width   = 1,
		},
		.table = p1_fdle_pll_table,
		.init_regs = p1_fdle_pll_init_regs,
		.init_count = ARRAY_SIZE(p1_fdle_pll_init_regs),
	},
	.hw.init = &(struct clk_init_data){
		.name = "fdle_pll_dco",
		.ops = &meson_clk_pll_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "xtal",
		},
		.num_parents = 1,
		/*
		 * Register has the risk of being directly operated
		 */
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap p1_fdle_pll = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_FDLEPLL_CTRL0,
		.shift = 12,
		.width = 3,
		.flags = (CLK_DIVIDER_POWER_OF_TWO |
			  CLK_DIVIDER_ROUND_CLOSEST),
	},
	.hw.init = &(struct clk_init_data){
		.name = "fdle_pll",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_fdle_pll_dco.hw
		},
		.num_parents = 1,
		/*
		 * fdle pll is directly used in other modules, and the
		 * parent rate needs to be modified
		 * Register has the risk of being directly operated.
		 */
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
	},
};

#ifdef CONFIG_ARM
static const struct pll_params_table p1_m4_pll_table[] = {
	PLL_PARAMS(100, 1, 1), /*DCO=4800M OD=1200M*/
	PLL_PARAMS(125, 1, 1), /*DCO=3000M OD=1500M*/
	{ /* sentinel */  }
};
#else
static const struct pll_params_table p1_m4_pll_table[] = {
	PLL_PARAMS(100, 1), /*DCO=4800M OD=1200M*/
	PLL_PARAMS(125, 1), /*DCO=3000M OD=1500M*/
	{ /* sentinel */  }
};
#endif

static const struct reg_sequence p1_m4_pll_init_regs[] = {
	{ .reg = CLKCTRL_M4PLL_CTRL0, .def = 0x20011086 },
	{ .reg = CLKCTRL_M4PLL_CTRL0, .def = 0x30011086 },
	{ .reg = CLKCTRL_M4PLL_CTRL1, .def = 0x1420500f },
	{ .reg = CLKCTRL_M4PLL_CTRL2, .def = 0x00023041 },
	{ .reg = CLKCTRL_M4PLL_CTRL3, .def = 0x0, .delay_us = 50 },
	{ .reg = CLKCTRL_M4PLL_CTRL0, .def = 0x10011086, .delay_us = 20 },
	{ .reg = CLKCTRL_M4PLL_CTRL2, .def = 0x00023001, .delay_us = 50 }
};

static struct clk_regmap p1_m4_pll_dco = {
	.data = &(struct meson_clk_pll_data){
		.en = {
			.reg_off = CLKCTRL_M4PLL_CTRL0,
			.shift   = 28,
			.width   = 1,
		},
		.m = {
			.reg_off = CLKCTRL_M4PLL_CTRL0,
			.shift   = 0,
			.width   = 8,
		},
		.n = {
			.reg_off = CLKCTRL_M4PLL_CTRL0,
			.shift   = 16,
			.width   = 5,
		},
		.l = {
			.reg_off = CLKCTRL_M4PLL_STS,
			.shift   = 31,
			.width   = 1,
		},
		.rst = {
			.reg_off = CLKCTRL_M4PLL_CTRL0,
			.shift   = 29,
			.width   = 1,
		},
		.table = p1_m4_pll_table,
		.init_regs = p1_m4_pll_init_regs,
		.init_count = ARRAY_SIZE(p1_m4_pll_init_regs),
	},
	.hw.init = &(struct clk_init_data){
		.name = "m4_pll_dco",
		.ops = &meson_clk_pll_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "xtal",
		},
		.num_parents = 1,
		/*
		 * Register has the risk of being directly operated
		 */
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap p1_m4_pll = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_M4PLL_CTRL0,
		.shift = 12,
		.width = 3,
		.flags = (CLK_DIVIDER_POWER_OF_TWO |
			  CLK_DIVIDER_ROUND_CLOSEST),
	},
	.hw.init = &(struct clk_init_data){
		.name = "m4_pll",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_m4_pll_dco.hw
		},
		.num_parents = 1,
		/*
		 * m4 pll is directly used in other modules, and the
		 * parent rate needs to be modified
		 * Register has the risk of being directly operated.
		 */
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
	},
};

#ifdef CONFIG_ARM
static const struct pll_params_table p1_gp2_pll_table[] = {
	PLL_PARAMS(200, 1, 2), /*DCO=4800M OD=1200M*/
	PLL_PARAMS(125, 1, 1), /*DCO=3000M OD=1500M*/
	{ /* sentinel */  }
};
#else
static const struct pll_params_table p1_gp2_pll_table[] = {
	PLL_PARAMS(200, 1), /*DCO=4800M OD=1200M*/
	PLL_PARAMS(125, 1), /*DCO=3000M OD=1500M*/
	{ /* sentinel */  }
};
#endif
/* TODO: need check */
static struct clk_regmap p1_gp2_pll_dco = {
	.data = &(struct meson_clk_pll_data){
		.en = {
			.reg_off = CLKCTRL_GP2PLL_CTRL0,
			.shift   = 28,
			.width   = 1,
		},
		.m = {
			.reg_off = CLKCTRL_GP2PLL_CTRL0,
			.shift   = 0,
			.width   = 8,
		},
		.n = {
			.reg_off = CLKCTRL_GP2PLL_CTRL0,
			.shift   = 16,
			.width   = 5,
		},
		.l = {
			.reg_off = CLKCTRL_GP2PLL_STS,
			.shift   = 31,
			.width   = 1,
		},
		.rst = {
			.reg_off = CLKCTRL_GP2PLL_CTRL0,
			.shift   = 29,
			.width   = 1,
		},
		.table = p1_gp2_pll_table,
	},
	.hw.init = &(struct clk_init_data){
		.name = "gp2_pll_dco",
		.ops = &meson_clk_pll_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "xtal",
		},
		.num_parents = 1,
		/*
		 * Register has the risk of being directly operated
		 */
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap p1_gp2_pll = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_GP2PLL_CTRL0,
		.shift = 12,
		.width = 3,
		.flags = (CLK_DIVIDER_POWER_OF_TWO |
			  CLK_DIVIDER_ROUND_CLOSEST),
	},
	.hw.init = &(struct clk_init_data){
		.name = "gp2_pll",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_gp2_pll_dco.hw
		},
		.num_parents = 1,
		/*
		 * gp2 pll is directly used in other modules, and the
		 * parent rate needs to be modified
		 * Register has the risk of being directly operated.
		 */
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
	},
};

#ifdef CONFIG_ARM
static const struct pll_params_table p1_nna_pll_table[] = {
	PLL_PARAMS(100, 1, 1), /*DCO=2400M OD=1200M*/
	PLL_PARAMS(125, 1, 1), /*DCO=3000M OD=1500M*/
	{ /* sentinel */  }
};
#else
static const struct pll_params_table p1_nna_pll_table[] = {
	PLL_PARAMS(100, 1), /*DCO=2400M OD=1200M*/
	PLL_PARAMS(125, 1), /*DCO=3000M OD=1500M*/
	{ /* sentinel */  }
};
#endif

static const struct reg_sequence p1_nna_pll_init_regs[] = {
	{ .reg = CLKCTRL_NNAPLL_CTRL0, .def = 0x20011086 },
	{ .reg = CLKCTRL_NNAPLL_CTRL0, .def = 0x30011086 },
	{ .reg = CLKCTRL_NNAPLL_CTRL1, .def = 0x1420500f },
	{ .reg = CLKCTRL_NNAPLL_CTRL2, .def = 0x00023041 },
	{ .reg = CLKCTRL_NNAPLL_CTRL3, .def = 0x0, .delay_us = 50 },
	{ .reg = CLKCTRL_NNAPLL_CTRL0, .def = 0x10011086, .delay_us = 20 },
	{ .reg = CLKCTRL_NNAPLL_CTRL2, .def = 0x00023001, .delay_us = 50 }
};

static struct clk_regmap p1_nna_pll_dco = {
	.data = &(struct meson_clk_pll_data){
		.en = {
			.reg_off = CLKCTRL_NNAPLL_CTRL0,
			.shift   = 28,
			.width   = 1,
		},
		.m = {
			.reg_off = CLKCTRL_NNAPLL_CTRL0,
			.shift   = 0,
			.width   = 8,
		},
		.n = {
			.reg_off = CLKCTRL_NNAPLL_CTRL0,
			.shift   = 16,
			.width   = 5,
		},
		.l = {
			.reg_off = CLKCTRL_NNAPLL_STS,
			.shift   = 31,
			.width   = 1,
		},
		.rst = {
			.reg_off = CLKCTRL_NNAPLL_CTRL0,
			.shift   = 29,
			.width   = 1,
		},
		.table = p1_nna_pll_table,
		.init_regs = p1_nna_pll_init_regs,
		.init_count = ARRAY_SIZE(p1_nna_pll_init_regs),
	},
	.hw.init = &(struct clk_init_data){
		.name = "nna_pll_dco",
		.ops = &meson_clk_pll_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "xtal",
		},
		.num_parents = 1,
		/*
		 * Register has the risk of being directly operated
		 */
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap p1_nna_pll = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_NNAPLL_CTRL0,
		.shift = 12,
		.width = 3,
		.flags = (CLK_DIVIDER_POWER_OF_TWO |
			  CLK_DIVIDER_ROUND_CLOSEST),
	},
	.hw.init = &(struct clk_init_data){
		.name = "nna_pll",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_nna_pll_dco.hw
		},
		.num_parents = 1,
		/*
		 * nna pll is directly used in other modules, and the
		 * parent rate needs to be modified
		 * Register has the risk of being directly operated.
		 */
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
	},
};

/* a55 cpu_clk, get the table from ucode */
static const struct cpu_dyn_table p1_cpu_dyn_table[] = {
	CPU_LOW_PARAMS(100000000, 1, 1, 9),
	CPU_LOW_PARAMS(250000000, 1, 1, 3),
	CPU_LOW_PARAMS(333333333, 2, 1, 1),
	CPU_LOW_PARAMS(500000000, 1, 1, 1),
	CPU_LOW_PARAMS(666666666, 2, 0, 0),
	CPU_LOW_PARAMS(1000000000, 1, 0, 0),
};

static const struct clk_parent_data p1_dyn_clk_sel[] __initconst = {
	{ .fw_name = "xtal", },
	{ .hw = &p1_fclk_div2.hw },
	{ .hw = &p1_fclk_div3.hw },
	{ .hw = &p1_fclk_div2p5.hw },
};

static struct clk_regmap p1_cpu_dyn_clk = {
	.data = &(struct meson_sec_cpu_dyn_data){
		.table = p1_cpu_dyn_table,
		.table_cnt = ARRAY_SIZE(p1_cpu_dyn_table),
		.secid_dyn_rd = SECID_CPU_CLK_RD,
		.secid_dyn = SECID_CPU_CLK_DYN,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cpu_dyn_clk",
		.ops = &meson_sec_cpu_dyn_ops,
		.parent_data = p1_dyn_clk_sel,
		.num_parents = 4,
	},
};

static struct clk_regmap p1_cpu_clk = {
	.data = &(struct clk_regmap_mux_data){
		.mask = 0x1,
		.shift = 11,
		.smc_id = SECURE_CPU_CLK,
		.secid = SECID_CPU_CLK_SEL,
		.secid_rd = SECID_CPU_CLK_RD
	},
	.hw.init = &(struct clk_init_data){
		.name = "cpu_clk",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_cpu_dyn_clk.hw,
			&p1_sys_pll.hw,
		},
		.num_parents = 2,
		/*
		 * This clock feeds the CPU, avoid disabling it
		 * Register has the risk of being directly operated
		 */
		.flags = CLK_SET_RATE_PARENT | CLK_IS_CRITICAL,
	},
};

static struct clk_regmap p1_a76_dyn_clk = {
	.data = &(struct meson_sec_cpu_dyn_data){
		.table = p1_cpu_dyn_table,
		.table_cnt = ARRAY_SIZE(p1_cpu_dyn_table),
		.secid_dyn_rd = SECID_A76_CLK_RD,
		.secid_dyn = SECID_A76_CLK_DYN,
	},
	.hw.init = &(struct clk_init_data){
		.name = "a76_dyn_clk",
		.ops = &meson_sec_cpu_dyn_ops,
		.parent_data = p1_dyn_clk_sel,
		.num_parents = 4,
	},
};

static struct clk_regmap p1_a76_clk = {
	.data = &(struct clk_regmap_mux_data){
		.mask = 0x1,
		.shift = 11,
		.smc_id = SECURE_CPU_CLK,
		.secid = SECID_A76_CLK_SEL,
		.secid_rd = SECID_A76_CLK_RD
	},
	.hw.init = &(struct clk_init_data){
		.name = "a76_clk",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_a76_dyn_clk.hw,
			&p1_sys1_pll.hw,
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_dsu_dyn_clk = {
	.data = &(struct meson_sec_cpu_dyn_data){
		.table = p1_cpu_dyn_table,
		.table_cnt = ARRAY_SIZE(p1_cpu_dyn_table),
		.secid_dyn_rd = SECID_DSU_CLK_RD,
		.secid_dyn = SECID_DSU_CLK_DYN,
	},
	.hw.init = &(struct clk_init_data){
		.name = "dsu_dyn_clk",
		.ops = &meson_sec_cpu_dyn_ops,
		.parent_data = p1_dyn_clk_sel,
		.num_parents = 4,
	},
};

static struct clk_regmap p1_dsu_clk = {
	.data = &(struct clk_regmap_mux_data){
		.mask = 0x1,
		.shift = 11,
		.smc_id = SECURE_CPU_CLK,
		.secid = SECID_DSU_CLK_SEL,
		.secid_rd = SECID_DSU_CLK_RD,
	},
	.hw.init = &(struct clk_init_data){
		.name = "dsu_clk",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_dsu_dyn_clk.hw,
			&p1_gp1_pll.hw,
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_dsu_final_clk = {
	.data = &(struct clk_regmap_mux_data){
		.mask = 0x1,
		.shift = 31,
		.smc_id = SECURE_CPU_CLK,
		.secid = SECID_DSU_FINAL_CLK_SEL,
		.secid_rd = SECID_DSU_FINAL_CLK_RD,
	},
	.hw.init = &(struct clk_init_data){
		.name = "dsu_final_clk",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_cpu_clk.hw,
			&p1_dsu_clk.hw,
		},
		.num_parents = 2,
	},
};

static struct clk_regmap p1_cpu6_clk = {
	.data = &(struct clk_regmap_mux_data){
		.mask = 0x1,
		.shift = 29,
		.smc_id = SECURE_CPU_CLK,
		.secid = SECID_CPU6_CLK_SEL,
		.secid_rd = SECID_CPU6_CLK_RD,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cpu6_clk",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_cpu_clk.hw,
			&p1_a76_clk.hw,
		},
		.num_parents = 2,
	},
};

static struct clk_regmap p1_cpu7_clk = {
	.data = &(struct clk_regmap_mux_data){
		.mask = 0x1,
		.shift = 30,
		.smc_id = SECURE_CPU_CLK,
		.secid = SECID_CPU7_CLK_SEL,
		.secid_rd = SECID_CPU7_CLK_RD,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cpu7_clk",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_cpu_clk.hw,
			&p1_a76_clk.hw,
		},
		.num_parents = 2,
	},
};

struct p1_sys_pll_nb_data {
	struct notifier_block nb;
	struct clk_hw *sys_pll;
	struct clk_hw *cpu_clk;
	struct clk_hw *cpu_dyn_clk;
};

static int p1_sys_pll_notifier_cb(struct notifier_block *nb,
				   unsigned long event, void *data)
{
	struct p1_sys_pll_nb_data *nb_data =
		container_of(nb, struct p1_sys_pll_nb_data, nb);

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

static struct p1_sys_pll_nb_data p1_sys_pll_nb_data = {
	.sys_pll = &p1_sys_pll.hw,
	.cpu_clk = &p1_cpu_clk.hw,
	.cpu_dyn_clk = &p1_cpu_dyn_clk.hw,
	.nb.notifier_call = p1_sys_pll_notifier_cb,
};

static struct p1_sys_pll_nb_data p1_sys1_pll_nb_data = {
	.sys_pll = &p1_sys1_pll.hw,
	.cpu_clk = &p1_a76_clk.hw,
	.cpu_dyn_clk = &p1_a76_dyn_clk.hw,
	.nb.notifier_call = p1_sys_pll_notifier_cb,
};

static struct p1_sys_pll_nb_data p1_gp1_pll_nb_data = {
	.sys_pll = &p1_gp1_pll.hw,
	.cpu_clk = &p1_dsu_clk.hw,
	.cpu_dyn_clk = &p1_dsu_dyn_clk.hw,
	.nb.notifier_call = p1_sys_pll_notifier_cb,
};

#ifdef CONFIG_ARM
static const struct pll_params_table p1_hifi_pll_table[] = {
	PLL_PARAMS(150, 1, 1), /* DCO = 1806.336M OD = 1 */
	{ /* sentinel */  }
};
#else
static const struct pll_params_table p1_hifi_pll_table[] = {
	PLL_PARAMS(150, 1), /* DCO = 1806.336M */
	{ /* sentinel */  }
};
#endif

/*
 * Internal hifi pll emulation configuration parameters
 */
static const struct reg_sequence p1_hifi_init_regs[] = {
	{ .reg = CLKCTRL_HIFIPLL_CTRL1,	.def = 0x00010e56 },
	{ .reg = CLKCTRL_HIFIPLL_CTRL2,	.def = 0x00000000 },
	{ .reg = CLKCTRL_HIFIPLL_CTRL3,	.def = 0x6a285c00 },
	{ .reg = CLKCTRL_HIFIPLL_CTRL4,	.def = 0x65771290 },
	{ .reg = CLKCTRL_HIFIPLL_CTRL5,	.def = 0x3927200a },
	{ .reg = CLKCTRL_HIFIPLL_CTRL6,	.def = 0x56540000 }
};

static struct clk_regmap p1_hifi_pll_dco = {
	.data = &(struct meson_clk_pll_data){
		.en = {
			.reg_off = CLKCTRL_HIFIPLL_CTRL0,
			.shift   = 28,
			.width   = 1,
		},
		.m = {
			.reg_off = CLKCTRL_HIFIPLL_CTRL0,
			.shift   = 0,
			.width   = 9,
		},
		.n = {
			.reg_off = CLKCTRL_HIFIPLL_CTRL0,
			.shift   = 10,
			.width   = 5,
		},
		.frac = {
			.reg_off = CLKCTRL_HIFIPLL_CTRL1,
			.shift   = 0,
			.width   = 19,
		},
		.l = {
			.reg_off = CLKCTRL_HIFIPLL_STS,
			.shift   = 31,
			.width   = 1,
		},
		.rst = {
			.reg_off = CLKCTRL_HIFIPLL_CTRL0,
			.shift   = 29,
			.width   = 1,
		},
		.table = p1_hifi_pll_table,
		.init_regs = p1_hifi_init_regs,
		.init_count = ARRAY_SIZE(p1_hifi_init_regs),
		//.flags = CLK_MESON_PLL_ROUND_CLOSEST,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hifi_pll_dco",
		.ops = &meson_clk_pll_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "xtal",
		},
		.num_parents = 1,
		/*
		 * Register has the risk of being directly operated
		 */
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap p1_hifi_pll = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_HIFIPLL_CTRL0,
		.shift = 16,
		.width = 3,
		.flags = CLK_DIVIDER_POWER_OF_TWO
			 // CLK_DIVIDER_ROUND_CLOSEST),
	},
	.hw.init = &(struct clk_init_data){
		.name = "hifi_pll",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_hifi_pll_dco.hw
		},
		.num_parents = 1,
		/*
		 * hifi pll is directly used in other modules, and the
		 * parent rate needs to be modified
		 * Register has the risk of being directly operated.
		 */
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
	},
};

/*
 * The Meson p1 PCIE PLL is fined tuned to deliver a very precise
 * 100MHz reference clock for the PCIe Analog PHY, and thus requires
 * a strict register sequence to enable the PLL.
 */
static const struct reg_sequence p1_pcie_pll_init_regs[] = {
	{ .reg = CLKCTRL_PCIEPLL_CTRL0,	.def = 0x200c04c8 },
	{ .reg = CLKCTRL_PCIEPLL_CTRL0,	.def = 0x300c04c8 },
	{ .reg = CLKCTRL_PCIEPLL_CTRL1,	.def = 0x30000000 },
	{ .reg = CLKCTRL_PCIEPLL_CTRL2,	.def = 0x00001100 },
	{ .reg = CLKCTRL_PCIEPLL_CTRL3,	.def = 0x10058e00 },
	{ .reg = CLKCTRL_PCIEPLL_CTRL4,	.def = 0x000100c0 },
	{ .reg = CLKCTRL_PCIEPLL_CTRL5,	.def = 0x68000048 },
	{ .reg = CLKCTRL_PCIEPLL_CTRL5,	.def = 0x68000068, .delay_us = 20 },
	{ .reg = CLKCTRL_PCIEPLL_CTRL4,	.def = 0x008100c0, .delay_us = 10 },
	{ .reg = CLKCTRL_PCIEPLL_CTRL0,	.def = 0x340c04c8 },
	{ .reg = CLKCTRL_PCIEPLL_CTRL0,	.def = 0x140c04c8, .delay_us = 10 },
	{ .reg = CLKCTRL_PCIEPLL_CTRL2,	.def = 0x00001000 }
};

#ifdef CONFIG_ARM64
/* Keep a single entry table for recalc/round_rate() ops */
static const struct pll_params_table p1_pcie_pll_table[] = {
	PLL_PARAMS(150, 1),
	{0, 0}
};
#else
static const struct pll_params_table p1_pcie_pll_table[] = {
	PLL_PARAMS(150, 1, 0),
	{0, 0, 0}
};
#endif

static struct clk_regmap p1_pcie_pll_dco = {
	.data = &(struct meson_clk_pll_data){
		.en = {
			.reg_off = CLKCTRL_PCIEPLL_CTRL0,
			.shift   = 28,
			.width   = 1,
		},
		.m = {
			.reg_off = CLKCTRL_PCIEPLL_CTRL0,
			.shift   = 0,
			.width   = 8,
		},
		.n = {
			.reg_off = CLKCTRL_PCIEPLL_CTRL0,
			.shift   = 10,
			.width   = 5,
		},
		.frac = {
			.reg_off = CLKCTRL_PCIEPLL_CTRL1,
			.shift   = 0,
			.width   = 12,
		},
		.l = {
			.reg_off = CLKCTRL_PCIEPLL_STS,
			.shift   = 31,
			.width   = 1,
		},
		.rst = {
			.reg_off = CLKCTRL_PCIEPLL_CTRL0,
			.shift   = 29,
			.width   = 1,
		},
		.table = p1_pcie_pll_table,
		.init_regs = p1_pcie_pll_init_regs,
		.init_count = ARRAY_SIZE(p1_pcie_pll_init_regs),
	},
	.hw.init = &(struct clk_init_data){
		.name = "pcie_pll_dco",
		.ops = &meson_clk_pcie_pll_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "xtal",
		},
		.num_parents = 1,
		/*
		 * Register has the risk of being directly operated
		 */
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_fixed_factor p1_pcie_pll_dco_div2 = {
	.mult = 1,
	.div = 2,
	.hw.init = &(struct clk_init_data){
		.name = "pcie_pll_dco_div2",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_pcie_pll_dco.hw
		},
		.num_parents = 1,
		/*
		 * pcie pll is directly used in other modules, and the
		 * parent rate needs to be modified
		 * Register has the risk of being directly operated.
		 */
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap p1_pcie_pll_od = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_PCIEPLL_CTRL0,
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
			&p1_pcie_pll_dco_div2.hw
		},
		.num_parents = 1,
		/*
		 * pcie pll is directly used in other modules, and the
		 * parent rate needs to be modified
		 * Register has the risk of being directly operated.
		 */
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_fixed_factor p1_pcie_pll = {
	.mult = 1,
	.div = 2,
	.hw.init = &(struct clk_init_data){
		.name = "pcie_pll",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_pcie_pll_od.hw
		},
		.num_parents = 1,
		/*
		 * pcie pll is directly used in other modules, and the
		 * parent rate needs to be modified
		 * Register has the risk of being directly operated.
		 */
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap p1_pcie_hcsl = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_PCIEPLL_CTRL5,
		.bit_idx = 3,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pcie_hcsl",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &p1_pcie_pll.hw },
		.num_parents = 1,
	},
};

static struct clk_fixed_factor p1_mpll_50m_div = {
	.mult = 1,
	.div = 80,
	.hw.init = &(struct clk_init_data){
		.name = "mpll_50m_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_fixed_pll_dco.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap p1_mpll_50m = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_FIXPLL_CTRL3,
		.mask = 0x1,
		.shift = 5,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpll_50m",
		.ops = &clk_regmap_mux_ro_ops,
		.parent_data = (const struct clk_parent_data []) {
			{ .fw_name = "xtal", },
			{ .hw = &p1_mpll_50m_div.hw },
		},
		.num_parents = 2,
	},
};

#ifdef CONFIG_ARM
static const struct pll_params_table p1_mclk_pll_table[] = {
	PLL_PARAMS(99, 1, 1), /* DCO = 2376M OD = 1 PLL = 1152M */
	PLL_PARAMS(100, 1, 1), /* DCO = 2400M */
	{ /* sentinel */  }
};
#else
static const struct pll_params_table p1_mclk_pll_table[] = {
	PLL_PARAMS(99, 1), /* DCO = 2376M OD = 1 PLL = 1152M */
	PLL_PARAMS(100, 1), /* DCO = 2400M */
	{ /* sentinel */  }
};
#endif

static const struct reg_sequence p1_mclk_init_regs[] = {
	{ .reg = ANACTRL_MCLK_PLL_CNTL0, .def = 0x20010063 },
	{ .reg = ANACTRL_MCLK_PLL_CNTL0, .def = 0x30010063 },
	{ .reg = ANACTRL_MCLK_PLL_CNTL1, .def = 0x0480500f },
	{ .reg = ANACTRL_MCLK_PLL_CNTL2, .def = 0x00023041 },
	{ .reg = ANACTRL_MCLK_PLL_CNTL3, .def = 0x18000000 },
	{ .reg = ANACTRL_MCLK_PLL_CNTL4, .def = 0x01303003 },
	{ .reg = ANACTRL_MCLK_PLL_CNTL5, .def = 0x00000008 },
	{ .reg = ANACTRL_MCLK_PLL_CNTL6, .def = 0x01303003 },
	{ .reg = ANACTRL_MCLK_PLL_CNTL7, .def = 0x00000008 },
	{ .reg = ANACTRL_MCLK_PLL_CNTL8, .def = 0x01303003 },
	{ .reg = ANACTRL_MCLK_PLL_CNTL9, .def = 0x00000008 },
	{ .reg = ANACTRL_MCLK_PLL_CNTL10, .def = 0x01303003 },
	{ .reg = ANACTRL_MCLK_PLL_CNTL11, .def = 0x00000008 },
	{ .reg = ANACTRL_MCLK_PLL_CNTL12, .def = 0x01303003 },
	{ .reg = ANACTRL_MCLK_PLL_CNTL13, .def = 0x00000008, .delay_us = 20 },
	{ .reg = ANACTRL_MCLK_PLL_CNTL0, .def = 0x10010063, .delay_us = 20 },
	{ .reg = ANACTRL_MCLK_PLL_CNTL2, .def = 0x00023001, .delay_us = 20 }
};

static struct clk_regmap p1_mclk_pll_dco = {
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
		.l = {
			.reg_off = ANACTRL_MCLK_PLL_STS,
			.shift   = 31,
			.width   = 1,
		},
		.rst = {
			.reg_off = ANACTRL_MCLK_PLL_CNTL0,
			.shift   = 29,
			.width   = 1,
		},
		.table = p1_mclk_pll_table,
		.init_regs = p1_mclk_init_regs,
		.init_count = ARRAY_SIZE(p1_mclk_init_regs),
	},
	.hw.init = &(struct clk_init_data){
		.name = "mclk_pll_dco",
		.ops = &meson_clk_pll_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "xtal",
		},
		.num_parents = 1,
		.flags = CLK_IGNORE_UNUSED,
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

static struct clk_regmap p1_mclk_0_pre_od = {
	.data = &(struct clk_regmap_div_data){
		.offset = ANACTRL_MCLK_PLL_CNTL4,
		.shift = 24,
		.width = 3,
		.table = mclk_div,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mclk_0_pre_od",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_mclk_pll_dco.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_mclk_0_pll = {
	.data = &(struct clk_regmap_div_data){
		.offset = ANACTRL_MCLK_PLL_CNTL5,
		.shift = 0,
		.width = 5,
		.flags = CLK_DIVIDER_ONE_BASED |
			CLK_DIVIDER_ROUND_CLOSEST |
			CLK_DIVIDER_ALLOW_ZERO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mclk_0_pll",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_mclk_0_pre_od.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static u32 mclk_table[] = {0, 1, 2};

static struct clk_regmap p1_mclk_0_sel = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = ANACTRL_MCLK_PLL_CNTL4,
		.mask = 0x7,
		.shift = 4,
		.table = mclk_table,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mclk_0_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = (const struct clk_parent_data []) {
			{ .hw = &p1_mclk_0_pll.hw },
			{ .fw_name = "xtal", },
			{ .hw = &p1_mpll_50m_div.hw },
		},
		.num_parents = 3,
	},
};

static struct clk_regmap p1_mclk_0_div2 = {
	.data = &(struct clk_regmap_div_data){
		.offset = ANACTRL_MCLK_PLL_CNTL4,
		.shift = 2,
		.width = 1,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mclk_0_div2",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_mclk_0_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_mclk_0_div_en = {
	.data = &(struct clk_regmap_gate_data){
		.offset = ANACTRL_MCLK_PLL_CNTL4,
		.bit_idx = 1,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mclk_0_div_en",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &p1_mclk_0_div2.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap p1_mclk_0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = ANACTRL_MCLK_PLL_CNTL4,
		.bit_idx = 0,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mclk_0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_mclk_0_div_en.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

/*mclk1*/
static struct clk_regmap p1_mclk_1_pre_od = {
	.data = &(struct clk_regmap_div_data){
		.offset = ANACTRL_MCLK_PLL_CNTL6,
		.shift = 24,
		.width = 3,
		.table = mclk_div,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mclk_1_pre_od",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_mclk_pll_dco.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_mclk_1_pll = {
	.data = &(struct clk_regmap_div_data){
		.offset = ANACTRL_MCLK_PLL_CNTL7,
		.shift = 0,
		.width = 5,
		.flags = CLK_DIVIDER_ONE_BASED |
			CLK_DIVIDER_ROUND_CLOSEST |
			CLK_DIVIDER_ALLOW_ZERO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mclk_1_pll",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_mclk_1_pre_od.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_mclk_1_sel = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = ANACTRL_MCLK_PLL_CNTL6,
		.mask = 0x7,
		.shift = 4,
		.table = mclk_table,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mclk_1_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = (const struct clk_parent_data []) {
			{ .hw = &p1_mclk_1_pll.hw },
			{ .fw_name = "xtal", },
			{ .hw = &p1_mpll_50m_div.hw },
		},
		.num_parents = 3,
	},
};

static struct clk_regmap p1_mclk_1_div2 = {
	.data = &(struct clk_regmap_div_data){
		.offset = ANACTRL_MCLK_PLL_CNTL6,
		.shift = 2,
		.width = 1,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mclk_1_div2",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_mclk_1_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_mclk_1_div_en = {
	.data = &(struct clk_regmap_gate_data){
		.offset = ANACTRL_MCLK_PLL_CNTL6,
		.bit_idx = 1,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mclk_1_div_en",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_mclk_1_div2.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap p1_mclk_1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = ANACTRL_MCLK_PLL_CNTL6,
		.bit_idx = 0,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mclk_1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_mclk_1_div_en.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

/* mclk2 */
static struct clk_regmap p1_mclk_2_pre_od = {
	.data = &(struct clk_regmap_div_data){
		.offset = ANACTRL_MCLK_PLL_CNTL8,
		.shift = 24,
		.width = 3,
		.table = mclk_div,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mclk_2_pre_od",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_mclk_pll_dco.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_mclk_2_pll = {
	.data = &(struct clk_regmap_div_data){
		.offset = ANACTRL_MCLK_PLL_CNTL9,
		.shift = 0,
		.width = 5,
		.flags = CLK_DIVIDER_ONE_BASED |
			CLK_DIVIDER_ROUND_CLOSEST |
			CLK_DIVIDER_ALLOW_ZERO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mclk_2_pll",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_mclk_2_pre_od.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_mclk_2_sel = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = ANACTRL_MCLK_PLL_CNTL8,
		.mask = 0x7,
		.shift = 4,
		.table = mclk_table,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mclk_2_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = (const struct clk_parent_data []) {
			{ .hw = &p1_mclk_2_pll.hw },
			{ .fw_name = "xtal", },
			{ .hw = &p1_mpll_50m_div.hw },
		},
		.num_parents = 3,
	},
};

static struct clk_regmap p1_mclk_2_div2 = {
	.data = &(struct clk_regmap_div_data){
		.offset = ANACTRL_MCLK_PLL_CNTL8,
		.shift = 2,
		.width = 1,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mclk_2_div2",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_mclk_2_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_mclk_2_div_en = {
	.data = &(struct clk_regmap_gate_data){
		.offset = ANACTRL_MCLK_PLL_CNTL8,
		.bit_idx = 1,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mclk_2_div_en",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &p1_mclk_2_div2.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap p1_mclk_2 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = ANACTRL_MCLK_PLL_CNTL8,
		.bit_idx = 0,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mclk_2",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_mclk_2_div_en.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

/* mclk3 */
static struct clk_regmap p1_mclk_3_pre_od = {
	.data = &(struct clk_regmap_div_data){
		.offset = ANACTRL_MCLK_PLL_CNTL10,
		.shift = 24,
		.width = 3,
		.table = mclk_div,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mclk_3_pre_od",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_mclk_pll_dco.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_mclk_3_pll = {
	.data = &(struct clk_regmap_div_data){
		.offset = ANACTRL_MCLK_PLL_CNTL11,
		.shift = 0,
		.width = 5,
		.flags = CLK_DIVIDER_ONE_BASED |
			CLK_DIVIDER_ROUND_CLOSEST |
			CLK_DIVIDER_ALLOW_ZERO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mclk_3_pll",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_mclk_3_pre_od.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_mclk_3_sel = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = ANACTRL_MCLK_PLL_CNTL10,
		.mask = 0x7,
		.shift = 4,
		.table = mclk_table,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mclk_3_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = (const struct clk_parent_data []) {
			{ .hw = &p1_mclk_3_pll.hw },
			{ .fw_name = "xtal", },
			{ .hw = &p1_mpll_50m_div.hw },
		},
		.num_parents = 3,
	},
};

static struct clk_regmap p1_mclk_3_div2 = {
	.data = &(struct clk_regmap_div_data){
		.offset = ANACTRL_MCLK_PLL_CNTL10,
		.shift = 2,
		.width = 1,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mclk_3_div2",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_mclk_3_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_mclk_3_div_en = {
	.data = &(struct clk_regmap_gate_data){
		.offset = ANACTRL_MCLK_PLL_CNTL10,
		.bit_idx = 1,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mclk_3_div_en",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &p1_mclk_3_div2.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap p1_mclk_3 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = ANACTRL_MCLK_PLL_CNTL10,
		.bit_idx = 0,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mclk_3",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_mclk_3_div_en.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

/* mclk4 */
static struct clk_regmap p1_mclk_4_pre_od = {
	.data = &(struct clk_regmap_div_data){
		.offset = ANACTRL_MCLK_PLL_CNTL12,
		.shift = 24,
		.width = 3,
		.table = mclk_div,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mclk_4_pre_od",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_mclk_pll_dco.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_mclk_4_pll = {
	.data = &(struct clk_regmap_div_data){
		.offset = ANACTRL_MCLK_PLL_CNTL13,
		.shift = 0,
		.width = 5,
		.flags = CLK_DIVIDER_ONE_BASED |
			CLK_DIVIDER_ROUND_CLOSEST |
			CLK_DIVIDER_ALLOW_ZERO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mclk_4_pll",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_mclk_4_pre_od.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_mclk_4_sel = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = ANACTRL_MCLK_PLL_CNTL12,
		.mask = 0x7,
		.shift = 4,
		.table = mclk_table,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mclk_4_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = (const struct clk_parent_data []) {
			{ .hw = &p1_mclk_4_pll.hw },
			{ .fw_name = "xtal", },
			{ .hw = &p1_mpll_50m_div.hw },
		},
		.num_parents = 3,
	},
};

static struct clk_regmap p1_mclk_4_div2 = {
	.data = &(struct clk_regmap_div_data){
		.offset = ANACTRL_MCLK_PLL_CNTL12,
		.shift = 2,
		.width = 1,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mclk_4_div2",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_mclk_4_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_mclk_4_div_en = {
	.data = &(struct clk_regmap_gate_data){
		.offset = ANACTRL_MCLK_PLL_CNTL12,
		.bit_idx = 1,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mclk_4_div_en",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &p1_mclk_4_div2.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap p1_mclk_4 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = ANACTRL_MCLK_PLL_CNTL12,
		.bit_idx = 0,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mclk_4",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_mclk_4_div_en.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap p1_mclk_0_pll_div_en = {
	.data = &(struct clk_regmap_gate_data){
		.offset = ANACTRL_MCLK_PLL_CNTL4,
		.bit_idx = 2,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mclk_0_pll_div_en",
		.ops = &clk_regmap_gate_ops,
		.parent_data = (const struct clk_parent_data []) {
			{ .fw_name = "xtal", },
		},
		.num_parents = 1,
	},
};

static struct clk_regmap p1_mclk_1_pll_div_en = {
	.data = &(struct clk_regmap_gate_data){
		.offset = ANACTRL_MCLK_PLL_CNTL6,
		.bit_idx = 2,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mclk_1_pll_div_en",
		.ops = &clk_regmap_gate_ops,
		.parent_data = (const struct clk_parent_data []) {
			{ .fw_name = "xtal", },
		},
		.num_parents = 1,
	},
};

static struct clk_regmap p1_mclk_2_pll_div_en = {
	.data = &(struct clk_regmap_gate_data){
		.offset = ANACTRL_MCLK_PLL_CNTL8,
		.bit_idx = 2,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mclk_2_pll_div_en",
		.ops = &clk_regmap_gate_ops,
		.parent_data = (const struct clk_parent_data []) {
			{ .fw_name = "xtal", },
		},
		.num_parents = 1,
	},
};

static struct clk_regmap p1_mclk_3_pll_div_en = {
	.data = &(struct clk_regmap_gate_data){
		.offset = ANACTRL_MCLK_PLL_CNTL10,
		.bit_idx = 2,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mclk_3_pll_div_en",
		.ops = &clk_regmap_gate_ops,
		.parent_data = (const struct clk_parent_data []) {
			{ .fw_name = "xtal", },
		},
		.num_parents = 1,
	},
};

static struct clk_regmap p1_mclk_4_pll_div_en = {
	.data = &(struct clk_regmap_gate_data){
		.offset = ANACTRL_MCLK_PLL_CNTL12,
		.bit_idx = 2,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mclk_4_pll_div_en",
		.ops = &clk_regmap_gate_ops,
		.parent_data = (const struct clk_parent_data []) {
			{ .fw_name = "xtal", },
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor p1_mpll_prediv = {
	.mult = 1,
	.div = 2,
	.hw.init = &(struct clk_init_data){
		.name = "mpll_prediv",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_fixed_pll_dco.hw
		},
		.num_parents = 1,
	},
};

static const struct reg_sequence p1_mpll0_init_regs[] = {
	{ .reg = CLKCTRL_MPLL_CTRL2, .def = 0x40000033 }
};

static struct clk_regmap p1_mpll0_div = {
	.data = &(struct meson_clk_mpll_data){
		.sdm = {
			.reg_off = CLKCTRL_MPLL_CTRL1,
			.shift   = 0,
			.width   = 14,
		},
		.sdm_en = {
			.reg_off = CLKCTRL_MPLL_CTRL1,
			.shift   = 30,
			.width	 = 1,
		},
		.n2 = {
			.reg_off = CLKCTRL_MPLL_CTRL1,
			.shift   = 20,
			.width   = 9,
		},
		.ssen = {
			.reg_off = CLKCTRL_MPLL_CTRL1,
			.shift   = 29,
			.width	 = 1,
		},
		.lock = &meson_clk_lock,
		.init_regs = p1_mpll0_init_regs,
		.init_count = ARRAY_SIZE(p1_mpll0_init_regs),
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpll0_div",
		.ops = &meson_clk_mpll_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_mpll_prediv.hw
		},
		.num_parents = 1,
		/*
		 * Register has the risk of being directly operated.
		 */
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap p1_mpll0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_MPLL_CTRL1,
		.bit_idx = 31,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpll0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &p1_mpll0_div.hw },
		.num_parents = 1,
		/*
		 * mpll0 is directly used in other modules, and the
		 * parent rate needs to be modified
		 * Register has the risk of being directly operated.
		 */
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
	},
};

static const struct reg_sequence p1_mpll1_init_regs[] = {
	{ .reg = CLKCTRL_MPLL_CTRL4, .def = 0x40000033 }
};

static struct clk_regmap p1_mpll1_div = {
	.data = &(struct meson_clk_mpll_data){
		.sdm = {
			.reg_off = CLKCTRL_MPLL_CTRL3,
			.shift   = 0,
			.width   = 14,
		},
		.sdm_en = {
			.reg_off = CLKCTRL_MPLL_CTRL3,
			.shift   = 30,
			.width	 = 1,
		},
		.n2 = {
			.reg_off = CLKCTRL_MPLL_CTRL3,
			.shift   = 20,
			.width   = 9,
		},
		.ssen = {
			.reg_off = CLKCTRL_MPLL_CTRL3,
			.shift   = 29,
			.width	 = 1,
		},
		.lock = &meson_clk_lock,
		.init_regs = p1_mpll1_init_regs,
		.init_count = ARRAY_SIZE(p1_mpll1_init_regs),
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpll1_div",
		.ops = &meson_clk_mpll_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_mpll_prediv.hw
		},
		.num_parents = 1,
		/*
		 * Register has the risk of being directly operated.
		 */
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap p1_mpll1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_MPLL_CTRL3,
		.bit_idx = 31,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpll1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &p1_mpll1_div.hw },
		.num_parents = 1,
		/*
		 * mpll1 is directly used in other modules, and the
		 * parent rate needs to be modified
		 * Register has the risk of being directly operated.
		 */
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
	},
};

static const struct reg_sequence p1_mpll2_init_regs[] = {
	{ .reg = CLKCTRL_MPLL_CTRL6, .def = 0x40000033 }
};

static struct clk_regmap p1_mpll2_div = {
	.data = &(struct meson_clk_mpll_data){
		.sdm = {
			.reg_off = CLKCTRL_MPLL_CTRL5,
			.shift   = 0,
			.width   = 14,
		},
		.sdm_en = {
			.reg_off = CLKCTRL_MPLL_CTRL5,
			.shift   = 30,
			.width	 = 1,
		},
		.n2 = {
			.reg_off = CLKCTRL_MPLL_CTRL5,
			.shift   = 20,
			.width   = 9,
		},
		.ssen = {
			.reg_off = CLKCTRL_MPLL_CTRL5,
			.shift   = 29,
			.width	 = 1,
		},
		.lock = &meson_clk_lock,
		.init_regs = p1_mpll2_init_regs,
		.init_count = ARRAY_SIZE(p1_mpll2_init_regs),
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpll2_div",
		.ops = &meson_clk_mpll_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_mpll_prediv.hw
		},
		/*
		 * Register has the risk of being directly operated.
		 */
		.num_parents = 1,
	},
};

static struct clk_regmap p1_mpll2 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_MPLL_CTRL5,
		.bit_idx = 31,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpll2",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &p1_mpll2_div.hw },
		.num_parents = 1,
		/*
		 * mpll2 is directly used in other modules, and the
		 * parent rate needs to be modified
		 * Register has the risk of being directly operated.
		 */
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
	},
};

static const struct reg_sequence p1_mpll3_init_regs[] = {
	{ .reg = CLKCTRL_MPLL_CTRL8, .def = 0x40000033 }
};

static struct clk_regmap p1_mpll3_div = {
	.data = &(struct meson_clk_mpll_data){
		.sdm = {
			.reg_off = CLKCTRL_MPLL_CTRL7,
			.shift   = 0,
			.width   = 14,
		},
		.sdm_en = {
			.reg_off = CLKCTRL_MPLL_CTRL7,
			.shift   = 30,
			.width	 = 1,
		},
		.n2 = {
			.reg_off = CLKCTRL_MPLL_CTRL7,
			.shift   = 20,
			.width   = 9,
		},
		.ssen = {
			.reg_off = CLKCTRL_MPLL_CTRL7,
			.shift   = 29,
			.width	 = 1,
		},
		.lock = &meson_clk_lock,
		.init_regs = p1_mpll3_init_regs,
		.init_count = ARRAY_SIZE(p1_mpll3_init_regs),
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpll3_div",
		.ops = &meson_clk_mpll_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_mpll_prediv.hw
		},
		/*
		 * Register has the risk of being directly operated.
		 */
		.num_parents = 1,
	},
};

static struct clk_regmap p1_mpll3 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_MPLL_CTRL7,
		.bit_idx = 31,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpll3",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &p1_mpll3_div.hw },
		.num_parents = 1,
		/*
		 * mpll3 is directly used in other modules, and the
		 * parent rate needs to be modified
		 * Register has the risk of being directly operated.
		 */
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
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
static struct clk_regmap p1_rtc_32k_clkin = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_RTC_BY_OSCIN_CTRL0,
		.bit_idx = 31,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "rtc_32k_clkin",
		.ops = &clk_regmap_gate_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "xtal",
		},
		.num_parents = 1,
	},
};

static const struct meson_clk_dualdiv_param p1_32k_div_table[] = {
	{
		.dual	= 1,
		.n1	= 733,
		.m1	= 8,
		.n2	= 732,
		.m2	= 11,
	},
	{}
};

static struct clk_regmap p1_rtc_32k_div = {
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
		.table = p1_32k_div_table,
	},
	.hw.init = &(struct clk_init_data){
		.name = "rtc_32k_div",
		.ops = &meson_clk_dualdiv_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_rtc_32k_clkin.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap p1_rtc_32k_xtal = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_RTC_BY_OSCIN_CTRL1,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "rtc_32k_xtal",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_rtc_32k_clkin.hw
		},
		.num_parents = 1,
	},
};

/*
 * three parent for rtc clock out
 * pad is from where?
 */
static u32 rtc_32k_sel[] = {0, 1};

static struct clk_regmap p1_rtc_32k_sel = {
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
			&p1_rtc_32k_xtal.hw,
			&p1_rtc_32k_div.hw
		},
		.num_parents = 2,
		/*
		 * rtc 32k is directly used in other modules, and the
		 * parent rate needs to be modified
		 */
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_rtc_clk = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_RTC_BY_OSCIN_CTRL0,
		.bit_idx = 30,
	},
	.hw.init = &(struct clk_init_data){
		.name = "rtc_clk",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_rtc_32k_sel.hw,
		},
		.num_parents = 1,
		/*
		 * rtc 32k is directly used in other modules, and the
		 * parent rate needs to be modified
		 */
		.flags = CLK_SET_RATE_PARENT,
	},
};

/* sys clk = fclk_div4 / 3, and it is set in firmware */
static u32 mux_table_sys_clk_sel[] = { 0, 1, 2, 3, 4 };

static const struct clk_parent_data sys_clk_parent_data[] = {
	{ .fw_name = "xtal", },
	{ .hw = &p1_fclk_div2.hw },
	{ .hw = &p1_fclk_div3.hw },
	{ .hw = &p1_fclk_div4.hw },
	{ .hw = &p1_fclk_div5.hw },
};

static struct clk_regmap p1_sysclk_1_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_SYS_CLK_CTRL0,
		.mask = 0x7,
		.shift = 26,
		.table = mux_table_sys_clk_sel,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sysclk_1_sel",
		.ops = &clk_regmap_mux_ro_ops,
		.parent_data = sys_clk_parent_data,
		.num_parents = ARRAY_SIZE(sys_clk_parent_data),
	},
};

static struct clk_regmap p1_sysclk_1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_SYS_CLK_CTRL0,
		.shift = 16,
		.width = 10,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sysclk_1_div",
		.ops = &clk_regmap_divider_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_sysclk_1_sel.hw,
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap p1_sysclk_1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_SYS_CLK_CTRL0,
		.bit_idx = 29,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sysclk_1",
		.ops = &clk_regmap_gate_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_sysclk_1_div.hw,
		},
		.num_parents = 1,
	},
};

static struct clk_regmap p1_sysclk_0_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_SYS_CLK_CTRL0,
		.mask = 0x7,
		.shift = 10,
		.table = mux_table_sys_clk_sel,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sysclk_0_sel",
		.ops = &clk_regmap_mux_ro_ops,
		.parent_data = sys_clk_parent_data,
		.num_parents = ARRAY_SIZE(sys_clk_parent_data),
	},
};

static struct clk_regmap p1_sysclk_0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_SYS_CLK_CTRL0,
		.shift = 0,
		.width = 10,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sysclk_0_div",
		.ops = &clk_regmap_divider_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_sysclk_0_sel.hw,
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap p1_sysclk_0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_SYS_CLK_CTRL0,
		.bit_idx = 13,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sysclk_0",
		.ops = &clk_regmap_gate_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_sysclk_0_div.hw,
		},
		.num_parents = 1,
	},
};

static struct clk_regmap p1_sys_clk = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_SYS_CLK_CTRL0,
		.mask = 0x1,
		.shift = 15,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sys_clk",
		.ops = &clk_regmap_mux_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_sysclk_0.hw,
			&p1_sysclk_1.hw
		},
		.num_parents = 2,
	},
};

static u32 mux_table_dsp_clk_sel[] = { 0, 1, 2, 3, 4, 5, 7 };

static const struct clk_parent_data p1_dsp_parent_hws[] = {
	{ .fw_name = "xtal", },
	{ .hw = &p1_fclk_div2p5.hw },
	{ .hw = &p1_fclk_div3.hw },
	{ .hw = &p1_m4_pll.hw },
	{ .hw = &p1_hifi_pll.hw },
	{ .hw = &p1_fclk_div4.hw },
	//{ .hw = &p1_gp2_pll.hw },
	{ .hw = &p1_rtc_clk.hw }
};

static struct clk_regmap p1_dspa_0_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_DSPA_CLK_CTRL0,
		.mask = 0x7,
		.shift = 10,
		.table = mux_table_dsp_clk_sel
	},
	.hw.init = &(struct clk_init_data) {
		.name = "dspa_0_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = p1_dsp_parent_hws,
		.num_parents = ARRAY_SIZE(p1_dsp_parent_hws),
	},
};

static struct clk_regmap p1_dspa_0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_DSPA_CLK_CTRL0,
		.shift = 0,
		.width = 10,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "dspa_0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_dspa_0_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_dspa_0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_DSPA_CLK_CTRL0,
		.bit_idx = 13,
	},
	.hw.init = &(struct clk_init_data){
		.name = "dspa_0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_dspa_0_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_dspa_1_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_DSPA_CLK_CTRL0,
		.mask = 0x7,
		.shift = 26,
		.table = mux_table_dsp_clk_sel
	},
	.hw.init = &(struct clk_init_data) {
		.name = "dspa_1_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = p1_dsp_parent_hws,
		.num_parents = ARRAY_SIZE(p1_dsp_parent_hws),
	},
};

static struct clk_regmap p1_dspa_1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_DSPA_CLK_CTRL0,
		.shift = 16,
		.width = 10,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "dspa_1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_dspa_1_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_dspa_1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_DSPA_CLK_CTRL0,
		.bit_idx = 29,
	},
	.hw.init = &(struct clk_init_data){
		.name = "dspa_1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_dspa_1_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_dspa = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_DSPA_CLK_CTRL0,
		.mask = 0x1,
		.shift = 15,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "dspa",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_dspa_0.hw,
			&p1_dspa_1.hw,
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_dspb_0_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_DSPB_CLK_CTRL0,
		.mask = 0x7,
		.shift = 10,
		.table = mux_table_dsp_clk_sel
	},
	.hw.init = &(struct clk_init_data) {
		.name = "dspb_0_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = p1_dsp_parent_hws,
		.num_parents = ARRAY_SIZE(p1_dsp_parent_hws),
	},
};

static struct clk_regmap p1_dspb_0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_DSPB_CLK_CTRL0,
		.shift = 0,
		.width = 10,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "dspb_0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_dspb_0_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_dspb_0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_DSPB_CLK_CTRL0,
		.bit_idx = 13,
	},
	.hw.init = &(struct clk_init_data){
		.name = "dspb_0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_dspb_0_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_dspb_1_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_DSPB_CLK_CTRL0,
		.mask = 0x7,
		.shift = 26,
		.table = mux_table_dsp_clk_sel
	},
	.hw.init = &(struct clk_init_data) {
		.name = "dspb_1_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = p1_dsp_parent_hws,
		.num_parents = ARRAY_SIZE(p1_dsp_parent_hws),
	},
};

static struct clk_regmap p1_dspb_1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_DSPB_CLK_CTRL0,
		.shift = 16,
		.width = 10,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "dspb_1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_dspb_1_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_dspb_1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_DSPB_CLK_CTRL0,
		.bit_idx = 29,
	},
	.hw.init = &(struct clk_init_data){
		.name = "dspb_1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_dspb_1_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_dspb = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_DSPB_CLK_CTRL0,
		.mask = 0x1,
		.shift = 15,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "dspb",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_dspb_0.hw,
			&p1_dspb_1.hw,
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/*12_24M clk*/
static struct clk_regmap p1_24m_clk_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_CLK12_24_CTRL,
		.bit_idx = 11,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "24m_clk_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "xtal",
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor p1_24m_div2 = {
	.mult = 1,
	.div = 2,
	.hw.init = &(struct clk_init_data){
		.name = "24m_div2",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_24m_clk_gate.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap p1_12m_clk = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_CLK12_24_CTRL,
		.bit_idx = 10,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "12m_clk",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_24m_div2.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap p1_25m_clk_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_CLK12_24_CTRL,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "25m_clk_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_fclk_div2.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap p1_25m_clk = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_CLK12_24_CTRL,
		.bit_idx = 12,
	},
	.hw.init = &(struct clk_init_data){
		.name = "25m_clk",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_25m_clk_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static u32 p1_vapb_table[] = { 0, 1, 2, 3, 7};
static const struct clk_hw *p1_vapb_parent_hws[] = {
	&p1_fclk_div4.hw,
	&p1_fclk_div3.hw,
	&p1_fclk_div5.hw,
	&p1_fclk_div7.hw,
	&p1_fclk_div2p5.hw
};

static struct clk_regmap p1_vapb_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VAPBCLK_CTRL,
		.mask = 0x7,
		.shift = 9,
		.table = p1_vapb_table
	},
	.hw.init = &(struct clk_init_data){
		.name = "vapb_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = p1_vapb_parent_hws,
		.num_parents = ARRAY_SIZE(p1_vapb_parent_hws),
	},
};

static struct clk_regmap p1_vapb_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_VAPBCLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vapb_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_vapb_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_vapb = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VAPBCLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vapb",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_vapb_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static u32 p1_ge2d_table[] = { 0, 1, 2, 3, 5, 6 };

static const struct clk_parent_data p1_ge2d_clk_parent_data[] = {
	{ .fw_name = "xtal", },
	{ .hw = &p1_fclk_div2p5.hw },
	{ .hw = &p1_fclk_div3.hw },
	{ .hw = &p1_fdle_pll.hw },
	{ .hw = &p1_fclk_div4.hw },
	{ .hw = &p1_gp2_pll.hw },
};

static struct clk_regmap p1_ge2d_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_GE2DCLK_CTRL,
		.mask = 0x7,
		.shift = 9,
		.table = p1_ge2d_table
	},
	.hw.init = &(struct clk_init_data){
		.name = "ge2d_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = p1_ge2d_clk_parent_data,
		.num_parents = ARRAY_SIZE(p1_ge2d_clk_parent_data),
	},
};

static struct clk_regmap p1_ge2d_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_GE2DCLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "ge2d_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_ge2d_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_ge2d = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_GE2DCLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "ge2d",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_ge2d_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data p1_sd_emmc_parent_data[] = {
	{ .fw_name = "xtal", },
	{ .hw = &p1_fclk_div2.hw },
	{ .hw = &p1_fclk_div3.hw },
	{ .hw = &p1_hifi_pll.hw },
	{ .hw = &p1_fclk_div2p5.hw },
	{ .hw = &p1_mpll2.hw },
	{ .hw = &p1_mpll3.hw },
	{ .hw = &p1_gp0_pll.hw }
};

static struct clk_regmap p1_sd_emmc_c_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_NAND_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_c_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = p1_sd_emmc_parent_data,
		.num_parents = ARRAY_SIZE(p1_sd_emmc_parent_data),
	},
};

static struct clk_regmap p1_sd_emmc_c_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_NAND_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_c_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_sd_emmc_c_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap p1_sd_emmc_c = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_NAND_CLK_CTRL,
		.bit_idx = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sd_emmc_c",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_sd_emmc_c_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap p1_sd_emmc_a_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_SD_EMMC_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_a_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = p1_sd_emmc_parent_data,
		.num_parents = ARRAY_SIZE(p1_sd_emmc_parent_data),
	},
};

static struct clk_regmap p1_sd_emmc_a_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_SD_EMMC_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_a_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_sd_emmc_a_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_sd_emmc_a = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_SD_EMMC_CLK_CTRL,
		.bit_idx = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sd_emmc_a",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_sd_emmc_a_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data p1_spicc_parent_hws[] = {
	{ .fw_name = "xtal", },
	{ .hw = &p1_sys_clk.hw },
	{ .hw = &p1_fclk_div4.hw },
	{ .hw = &p1_fclk_div3.hw },
	{ .hw = &p1_fclk_div2.hw },
	{ .hw = &p1_fclk_div5.hw },
	{ .hw = &p1_fclk_div7.hw },
};

static struct clk_regmap p1_spicc0_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_SPICC_CLK_CTRL,
		.mask = 0x7,
		.shift = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spicc0_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = p1_spicc_parent_hws,
		.num_parents = ARRAY_SIZE(p1_spicc_parent_hws),
	},
};

static struct clk_regmap p1_spicc0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_SPICC_CLK_CTRL,
		.shift = 0,
		.width = 6,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spicc0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_spicc0_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_spicc0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_SPICC_CLK_CTRL,
		.bit_idx = 6,
	},
	.hw.init = &(struct clk_init_data){
		.name = "spicc0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_spicc0_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_spicc1_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_SPICC_CLK_CTRL,
		.mask = 0x7,
		.shift = 23,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spicc1_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = p1_spicc_parent_hws,
		.num_parents = ARRAY_SIZE(p1_spicc_parent_hws),
	},
};

static struct clk_regmap p1_spicc1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_SPICC_CLK_CTRL,
		.shift = 16,
		.width = 6,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spicc1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_spicc1_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_spicc1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_SPICC_CLK_CTRL,
		.bit_idx = 22,
	},
	.hw.init = &(struct clk_init_data){
		.name = "spicc1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_spicc1_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_spicc2_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_SPICC_CLK_CTRL1,
		.mask = 0x7,
		.shift = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spicc2_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = p1_spicc_parent_hws,
		.num_parents = ARRAY_SIZE(p1_spicc_parent_hws),
	},
};

static struct clk_regmap p1_spicc2_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_SPICC_CLK_CTRL1,
		.shift = 0,
		.width = 6,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spicc2_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_spicc2_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_spicc2 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_SPICC_CLK_CTRL1,
		.bit_idx = 6,
	},
	.hw.init = &(struct clk_init_data){
		.name = "spicc2",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_spicc2_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_spicc3_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_SPICC_CLK_CTRL1,
		.mask = 0x7,
		.shift = 23,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spicc3_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = p1_spicc_parent_hws,
		.num_parents = ARRAY_SIZE(p1_spicc_parent_hws),
	},
};

static struct clk_regmap p1_spicc3_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_SPICC_CLK_CTRL1,
		.shift = 16,
		.width = 6,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spicc3_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_spicc3_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_spicc3 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_SPICC_CLK_CTRL1,
		.bit_idx = 22,
	},
	.hw.init = &(struct clk_init_data){
		.name = "spicc3",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_spicc3_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_spicc4_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_SPICC_CLK_CTRL2,
		.mask = 0x7,
		.shift = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spicc4_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = p1_spicc_parent_hws,
		.num_parents = ARRAY_SIZE(p1_spicc_parent_hws),
	},
};

static struct clk_regmap p1_spicc4_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_SPICC_CLK_CTRL2,
		.shift = 0,
		.width = 6,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spicc4_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_spicc4_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_spicc4 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_SPICC_CLK_CTRL2,
		.bit_idx = 6,
	},
	.hw.init = &(struct clk_init_data){
		.name = "spicc4",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_spicc4_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_spicc5_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_SPICC_CLK_CTRL2,
		.mask = 0x7,
		.shift = 23,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spicc5_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = p1_spicc_parent_hws,
		.num_parents = ARRAY_SIZE(p1_spicc_parent_hws),
	},
};

static struct clk_regmap p1_spicc5_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_SPICC_CLK_CTRL2,
		.shift = 16,
		.width = 6,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spicc5_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_spicc5_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_spicc5 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_SPICC_CLK_CTRL2,
		.bit_idx = 22,
	},
	.hw.init = &(struct clk_init_data){
		.name = "spicc5",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_spicc5_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static u32 p1_pwm_table[] = { 0, 5, 6, 7};

static const struct clk_parent_data p1_pwm_parent_data[] = {
	{ .fw_name = "xtal", },
	{ .hw = &p1_mpll1.hw },
	{ .hw = &p1_mpll2.hw },
	{ .hw = &p1_fclk_div3.hw }
};

static struct clk_regmap p1_pwm_a_sel = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_AB_CTRL,
		.mask = 0x3,
		.shift = 9,
		.table = p1_pwm_table
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_a_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = p1_pwm_parent_data,
		.num_parents = ARRAY_SIZE(p1_pwm_parent_data),
	},
};

static struct clk_regmap p1_pwm_a_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_AB_CTRL,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_a_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_pwm_a_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_pwm_a = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_AB_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_a",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_pwm_a_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap p1_pwm_b_sel = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_AB_CTRL,
		.mask = 0x3,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_b_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = p1_pwm_parent_data,
		.num_parents = ARRAY_SIZE(p1_pwm_parent_data),
	},
};

static struct clk_regmap p1_pwm_b_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_AB_CTRL,
		.shift = 16,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_b_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_pwm_b_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_pwm_b = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_AB_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_b",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_pwm_b_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap p1_pwm_c_sel = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_CD_CTRL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_c_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = p1_pwm_parent_data,
		.num_parents = ARRAY_SIZE(p1_pwm_parent_data),
	},
};

static struct clk_regmap p1_pwm_c_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_CD_CTRL,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_c_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_pwm_c_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_pwm_c = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_CD_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_c",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_pwm_c_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap p1_pwm_d_sel = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_CD_CTRL,
		.mask = 0x3,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_d_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = p1_pwm_parent_data,
		.num_parents = ARRAY_SIZE(p1_pwm_parent_data),
	},
};

static struct clk_regmap p1_pwm_d_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_CD_CTRL,
		.shift = 16,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_d_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_pwm_d_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_pwm_d = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_CD_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_d",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_pwm_d_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap p1_pwm_e_sel = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_EF_CTRL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_e_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = p1_pwm_parent_data,
		.num_parents = ARRAY_SIZE(p1_pwm_parent_data),
	},
};

static struct clk_regmap p1_pwm_e_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_EF_CTRL,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_e_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_pwm_e_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_pwm_e = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_EF_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_e",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_pwm_e_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap p1_pwm_f_sel = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_EF_CTRL,
		.mask = 0x3,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_f_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = p1_pwm_parent_data,
		.num_parents = ARRAY_SIZE(p1_pwm_parent_data),
	},
};

static struct clk_regmap p1_pwm_f_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_EF_CTRL,
		.shift = 16,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_f_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_pwm_f_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_pwm_f = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_EF_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_f",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_pwm_f_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap p1_pwm_g_sel = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_GH_CTRL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_g_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = p1_pwm_parent_data,
		.num_parents = ARRAY_SIZE(p1_pwm_parent_data),
	},
};

static struct clk_regmap p1_pwm_g_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_GH_CTRL,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_g_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_pwm_g_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_pwm_g = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_GH_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_g",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_pwm_g_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap p1_pwm_h_sel = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_GH_CTRL,
		.mask = 0x3,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_h_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = p1_pwm_parent_data,
		.num_parents = ARRAY_SIZE(p1_pwm_parent_data),
	},
};

static struct clk_regmap p1_pwm_h_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_GH_CTRL,
		.shift = 16,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_h_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_pwm_h_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_pwm_h = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_GH_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_h",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_pwm_h_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap p1_pwm_i_sel = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_IJ_CTRL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_i_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = p1_pwm_parent_data,
		.num_parents = ARRAY_SIZE(p1_pwm_parent_data),
	},
};

static struct clk_regmap p1_pwm_i_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_IJ_CTRL,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_i_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_pwm_i_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_pwm_i = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_IJ_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_i",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_pwm_i_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap p1_pwm_j_sel = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_IJ_CTRL,
		.mask = 0x3,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_j_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = p1_pwm_parent_data,
		.num_parents = ARRAY_SIZE(p1_pwm_parent_data),
	},
};

static struct clk_regmap p1_pwm_j_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_IJ_CTRL,
		.shift = 16,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_j_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_pwm_j_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_pwm_j = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_IJ_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_j",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_pwm_j_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap p1_saradc_sel = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_SAR_CLK_CTRL0,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "saradc_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = (const struct clk_parent_data []) {
			{ .fw_name = "xtal", },
			{ .hw = &p1_sys_clk.hw },
			{ .hw = &p1_fclk_div5.hw },
		},
		.num_parents = 3,
	},
};

static struct clk_regmap p1_saradc_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_SAR_CLK_CTRL0,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "saradc_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_saradc_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_saradc = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_SAR_CLK_CTRL0,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "saradc",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_saradc_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/* gen clk */
static u32 p1_gen_clk_mux_table[] = { 0, 5, 6, 7, 19, 21, 22,
				23, 24, 25, 26, 27, 28 };

static const struct clk_parent_data p1_gen_clk_parent_data[] = {
	{ .fw_name = "xtal", },
	{ .hw = &p1_gp0_pll.hw },
	{ .hw = &p1_sys1_pll.hw },
	{ .hw = &p1_hifi_pll.hw },
	{ .hw = &p1_fclk_div2.hw },
	{ .hw = &p1_fclk_div3.hw },
	{ .hw = &p1_fclk_div4.hw },
	{ .hw = &p1_fclk_div5.hw },
	{ .hw = &p1_fclk_div7.hw },
	{ .hw = &p1_mpll0.hw },
	{ .hw = &p1_mpll1.hw },
	{ .hw = &p1_mpll2.hw },
	{ .hw = &p1_mpll3.hw },
};

static struct clk_regmap p1_gen_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_GEN_CLK_CTRL,
		.mask = 0x1f,
		.shift = 12,
		.table = p1_gen_clk_mux_table,
	},
	.hw.init = &(struct clk_init_data){
		.name = "gen_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = p1_gen_clk_parent_data,
		.num_parents = ARRAY_SIZE(p1_gen_clk_parent_data),
	},
};

static struct clk_regmap p1_gen_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_GEN_CLK_CTRL,
		.shift = 0,
		.width = 11,
	},
	.hw.init = &(struct clk_init_data){
		.name = "gen_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_gen_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_gen = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_GEN_CLK_CTRL,
		.bit_idx = 11,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "gen",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_gen_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_eth_rmii_sel = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_ETH_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "eth_rmii_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_fclk_div2.hw,
			&p1_fclk_div4.hw,
		//	&p1_gp2_pll.hw
		},
		.num_parents = 3
	},
};

static struct clk_regmap p1_eth_rmii_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_ETH_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "eth_rmii_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_eth_rmii_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap p1_eth_rmii = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_ETH_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "eth_rmii",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_eth_rmii_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static struct clk_fixed_factor p1_eth_div8 = {
	.mult = 1,
	.div = 8,
	.hw.init = &(struct clk_init_data){
		.name = "eth_div8",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) { &p1_fclk_div2.hw },
		.num_parents = 1,
	},
};

static struct clk_regmap p1_eth_125m = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_ETH_CLK_CTRL,
		.bit_idx = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "eth_125m",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_eth_div8.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap p1_ts_clk_div = {
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

static struct clk_regmap p1_ts_clk = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_TS_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "ts_clk",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_ts_clk_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static u32 p1_usb_clk_table[] = { 0, 1, 2, 3 };

static const struct clk_hw *p1_usb_clk_parent_hws[] = {
	&p1_fclk_div4.hw,
	&p1_fclk_div3.hw,
	&p1_fclk_div5.hw,
	&p1_fclk_div2.hw,
};

static struct clk_regmap p1_usb_250m_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_USB_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
		.table = p1_usb_clk_table
	},
	.hw.init = &(struct clk_init_data) {
		.name = "usb_250m_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = p1_usb_clk_parent_hws,
		.num_parents = ARRAY_SIZE(p1_usb_clk_parent_hws),
	},
};

static struct clk_regmap p1_usb_250m_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_USB_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "usb_250m_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_usb_250m_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_usb_250m = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_USB_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "usb_250m",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_usb_250m_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_pcie_400m_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_USB_CLK_CTRL,
		.mask = 0x7,
		.shift = 25,
		.table = p1_usb_clk_table
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pcie_400m_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = p1_usb_clk_parent_hws,
		.num_parents = ARRAY_SIZE(p1_usb_clk_parent_hws),
	},
};

static struct clk_regmap p1_pcie_400m_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_USB_CLK_CTRL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pcie_400m_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_pcie_400m_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_pcie_400m = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_USB_CLK_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pcie_400m",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_pcie_400m_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_pcie_clk_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_USB_CLK_CTRL1,
		.mask = 0x7,
		.shift = 9,
		.table = p1_usb_clk_table
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pcie_clk_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = p1_usb_clk_parent_hws,
		.num_parents = ARRAY_SIZE(p1_usb_clk_parent_hws),
	},
};

static struct clk_regmap p1_pcie_clk_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_USB_CLK_CTRL1,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pcie_clk_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_pcie_clk_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_pcie_clk = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_USB_CLK_CTRL1,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pcie_clk",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_pcie_clk_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_pcie_tl_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_USB_CLK_CTRL1,
		.mask = 0x7,
		.shift = 25,
		.table = p1_usb_clk_table
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pcie_tl_clk_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = p1_usb_clk_parent_hws,
		.num_parents = ARRAY_SIZE(p1_usb_clk_parent_hws),
	},
};

static struct clk_regmap p1_pcie_tl_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_USB_CLK_CTRL1,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pcie_tl_clk_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_pcie_tl_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_pcie_tl = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_USB_CLK_CTRL1,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pcie_tl_clk",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_pcie_tl_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static u32 p1_nna_table[] = { 0, 1, 2, 3, 5, 6 };

static const struct clk_parent_data p1_nna_clk_parent_data[] = {
	{ .fw_name = "xtal", },
	{ .hw = &p1_fclk_div2p5.hw },
	{ .hw = &p1_fclk_div3.hw },
	{ .hw = &p1_nna_pll.hw },
	{ .hw = &p1_fclk_div4.hw },
	{ .hw = &p1_gp2_pll.hw },
};

static struct clk_regmap p1_nna0_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_NNA_CLK_CTRL0,
		.mask = 0x7,
		.shift = 9,
		.table = p1_nna_table
	},
	.hw.init = &(struct clk_init_data) {
		.name = "nna0_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = p1_nna_clk_parent_data,
		.num_parents = ARRAY_SIZE(p1_nna_clk_parent_data),
	},
};

static struct clk_regmap p1_nna0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_NNA_CLK_CTRL0,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "nna0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_nna0_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_nna0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_NNA_CLK_CTRL0,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "nna0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_nna0_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_nna1_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_NNA_CLK_CTRL0,
		.mask = 0x7,
		.shift = 25,
		.table = p1_nna_table
	},
	.hw.init = &(struct clk_init_data) {
		.name = "nna1_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = p1_nna_clk_parent_data,
		.num_parents = ARRAY_SIZE(p1_nna_clk_parent_data),
	},
};

static struct clk_regmap p1_nna1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_NNA_CLK_CTRL0,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "nna1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_nna1_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_nna1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_NNA_CLK_CTRL0,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "nna1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_nna1_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_nna2_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_NNA_CLK_CTRL1,
		.mask = 0x7,
		.shift = 9,
		.table = p1_nna_table
	},
	.hw.init = &(struct clk_init_data) {
		.name = "nna2_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = p1_nna_clk_parent_data,
		.num_parents = ARRAY_SIZE(p1_nna_clk_parent_data),
	},
};

static struct clk_regmap p1_nna2_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_NNA_CLK_CTRL1,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "nna2_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_nna2_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_nna2 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_NNA_CLK_CTRL1,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "nna2",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_nna2_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_nna3_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_NNA_CLK_CTRL1,
		.mask = 0x7,
		.shift = 25,
		.table = p1_nna_table
	},
	.hw.init = &(struct clk_init_data) {
		.name = "nna3_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = p1_nna_clk_parent_data,
		.num_parents = ARRAY_SIZE(p1_nna_clk_parent_data),
	},
};

static struct clk_regmap p1_nna3_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_NNA_CLK_CTRL1,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "nna3_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_nna3_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_nna3 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_NNA_CLK_CTRL1,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "nna3",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_nna3_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_nna4_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_NNA_CLK_CTRL2,
		.mask = 0x7,
		.shift = 9,
		.table = p1_nna_table
	},
	.hw.init = &(struct clk_init_data) {
		.name = "nna4_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = p1_nna_clk_parent_data,
		.num_parents = ARRAY_SIZE(p1_nna_clk_parent_data),
	},
};

static struct clk_regmap p1_nna4_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_NNA_CLK_CTRL2,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "nna4_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_nna4_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_nna4 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_NNA_CLK_CTRL2,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "nna4",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_nna4_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_nna5_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_NNA_CLK_CTRL2,
		.mask = 0x7,
		.shift = 25,
		.table = p1_nna_table
	},
	.hw.init = &(struct clk_init_data) {
		.name = "nna5_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = p1_nna_clk_parent_data,
		.num_parents = ARRAY_SIZE(p1_nna_clk_parent_data),
	},
};

static struct clk_regmap p1_nna5_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_NNA_CLK_CTRL2,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "nna5_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_nna5_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_nna5 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_NNA_CLK_CTRL2,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "nna5",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_nna5_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct clk_hw *p1_isp_parent_hws[] = {
	&p1_fclk_div2p5.hw,
	&p1_fclk_div3.hw,
	&p1_fclk_div4.hw,
	&p1_fclk_div5.hw,
	&p1_fdle_pll.hw,
	&p1_m4_pll.hw,
	&p1_gp2_pll.hw,
	&p1_nna_pll.hw,
};

static struct clk_regmap p1_isp0_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_ISP0_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "isp0_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = p1_isp_parent_hws,
		.num_parents = ARRAY_SIZE(p1_isp_parent_hws),
	},
};

static struct clk_regmap p1_isp0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_ISP0_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "isp0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_isp0_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_isp0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_ISP0_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "isp0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_isp0_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_isp1_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_ISP1_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "isp1_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = p1_isp_parent_hws,
		.num_parents = ARRAY_SIZE(p1_isp_parent_hws),
	},
};

static struct clk_regmap p1_isp1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_ISP1_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "isp1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_isp1_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_isp1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_ISP1_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "isp1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_isp1_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_isp2_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_ISP2_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "isp2_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = p1_isp_parent_hws,
		.num_parents = ARRAY_SIZE(p1_isp_parent_hws),
	},
};

static struct clk_regmap p1_isp2_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_ISP2_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "isp2_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_isp2_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_isp2 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_ISP2_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "isp2",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_isp2_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_isp3_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_ISP3_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "isp3_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = p1_isp_parent_hws,
		.num_parents = ARRAY_SIZE(p1_isp_parent_hws),
	},
};

static struct clk_regmap p1_isp3_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_ISP3_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "isp3_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_isp3_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_isp3 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_ISP3_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "isp3",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_isp3_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_isp4_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_ISP4_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "isp4_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = p1_isp_parent_hws,
		.num_parents = ARRAY_SIZE(p1_isp_parent_hws),
	},
};

static struct clk_regmap p1_isp4_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_ISP4_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "isp4_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_isp4_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_isp4 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_ISP4_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "isp4",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_isp4_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_csiphy0_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_ISP0_CLK_CTRL,
		.mask = 0x7,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "csiphy0_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = p1_isp_parent_hws,
		.num_parents = ARRAY_SIZE(p1_isp_parent_hws),
	},
};

static struct clk_regmap p1_csiphy0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_ISP0_CLK_CTRL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "csiphy0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_csiphy0_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_csiphy0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_ISP0_CLK_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "csiphy0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_csiphy0_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_csiphy1_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_ISP1_CLK_CTRL,
		.mask = 0x7,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "csiphy1_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = p1_isp_parent_hws,
		.num_parents = ARRAY_SIZE(p1_isp_parent_hws),
	},
};

static struct clk_regmap p1_csiphy1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_ISP1_CLK_CTRL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "csiphy1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_csiphy1_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_csiphy1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_ISP1_CLK_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "csiphy1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_csiphy1_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_csiphy2_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_ISP2_CLK_CTRL,
		.mask = 0x7,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "csiphy2_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = p1_isp_parent_hws,
		.num_parents = ARRAY_SIZE(p1_isp_parent_hws),
	},
};

static struct clk_regmap p1_csiphy2_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_ISP2_CLK_CTRL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "csiphy2_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_csiphy2_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_csiphy2 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_ISP2_CLK_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "csiphy2",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_csiphy2_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_csiphy3_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_ISP3_CLK_CTRL,
		.mask = 0x7,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "csiphy3_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = p1_isp_parent_hws,
		.num_parents = ARRAY_SIZE(p1_isp_parent_hws),
	},
};

static struct clk_regmap p1_csiphy3_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_ISP3_CLK_CTRL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "csiphy3_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_csiphy3_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_csiphy3 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_ISP3_CLK_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "csiphy3",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_csiphy3_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_csiphy4_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_ISP4_CLK_CTRL,
		.mask = 0x7,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "csiphy4_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = p1_isp_parent_hws,
		.num_parents = ARRAY_SIZE(p1_isp_parent_hws),
	},
};

static struct clk_regmap p1_csiphy4_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_ISP4_CLK_CTRL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "csiphy4_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_csiphy4_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_csiphy4 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_ISP4_CLK_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "csiphy4",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_csiphy4_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_mopa_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_MOPCLK_CTRL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mopa_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = p1_isp_parent_hws,
		.num_parents = ARRAY_SIZE(p1_isp_parent_hws),
	},
};

static struct clk_regmap p1_mopa_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_MOPCLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mopa_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_mopa_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_mopa = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_MOPCLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mopa",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_mopa_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_mopb_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_MOPCLK_CTRL,
		.mask = 0x7,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mopb_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = p1_isp_parent_hws,
		.num_parents = ARRAY_SIZE(p1_isp_parent_hws),
	},
};

static struct clk_regmap p1_mopb_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_MOPCLK_CTRL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mopb_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_mopb_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_mopb = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_MOPCLK_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mopb",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_mopb_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_depa_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_DEPCLK_CTRL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "depa_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = p1_isp_parent_hws,
		.num_parents = ARRAY_SIZE(p1_isp_parent_hws),
	},
};

static struct clk_regmap p1_depa_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_DEPCLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "depa_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_depa_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_depa = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_DEPCLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "depa",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_depa_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_depb_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_DEPCLK_CTRL,
		.mask = 0x7,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "depb_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = p1_isp_parent_hws,
		.num_parents = ARRAY_SIZE(p1_isp_parent_hws),
	},
};

static struct clk_regmap p1_depb_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_DEPCLK_CTRL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "depb_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_depb_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_depb = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_DEPCLK_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "depb",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_depb_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_vfe_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VFECLK_CTRL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vfe_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = p1_isp_parent_hws,
		.num_parents = ARRAY_SIZE(p1_isp_parent_hws),
	},
};

static struct clk_regmap p1_vfe_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_VFECLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vfe_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_vfe_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_vfe = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VFECLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vfe",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_vfe_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_dewarpa_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_DEWARPA_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "dewarpa_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = p1_isp_parent_hws,
		.num_parents = ARRAY_SIZE(p1_isp_parent_hws),
	},
};

static struct clk_regmap p1_dewarpa_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_DEWARPA_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "dewarpa_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_dewarpa_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_dewarpa = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_DEWARPA_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "dewarpa",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_dewarpa_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_dewarpb_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_DEWARPB_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "dewarpb_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = p1_isp_parent_hws,
		.num_parents = ARRAY_SIZE(p1_isp_parent_hws),
	},
};

static struct clk_regmap p1_dewarpb_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_DEWARPB_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "dewarpb_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_dewarpb_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_dewarpb = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_DEWARPB_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "dewarpb",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_dewarpb_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_dewarpc_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_DEWARPC_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "dewarpc_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = p1_isp_parent_hws,
		.num_parents = ARRAY_SIZE(p1_isp_parent_hws),
	},
};

static struct clk_regmap p1_dewarpc_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_DEWARPC_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "dewarpc_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_dewarpc_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_dewarpc = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_DEWARPC_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "dewarpc",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_dewarpc_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data p1_m4_parent_hws[] = {
	{ .fw_name = "xtal", },
	{ .hw = &p1_fclk_div2p5.hw },
	{ .hw = &p1_fclk_div3.hw },
	{ .hw = &p1_m4_pll.hw },
	{ .hw = &p1_hifi_pll.hw },
	{ .hw = &p1_fclk_div4.hw },
	{ .hw = &p1_gp2_pll.hw },
	{ .hw = &p1_rtc_clk.hw }
};

static struct clk_regmap p1_m4_clk_0_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_M4_CLK_CTRL,
		.mask = 0x7,
		.shift = 10,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "m4_clk_0_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = p1_m4_parent_hws,
		.num_parents = ARRAY_SIZE(p1_m4_parent_hws),
	},
};

static struct clk_regmap p1_m4_clk_0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_M4_CLK_CTRL,
		.shift = 0,
		.width = 10,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "m4_clk_0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_m4_clk_0_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_m4_clk_0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_M4_CLK_CTRL,
		.bit_idx = 13,
	},
	.hw.init = &(struct clk_init_data){
		.name = "m4_clk_0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_m4_clk_0_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_m4_clk_1_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_M4_CLK_CTRL,
		.mask = 0x7,
		.shift = 26,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "m4_clk_1_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = p1_m4_parent_hws,
		.num_parents = ARRAY_SIZE(p1_m4_parent_hws),
	},
};

static struct clk_regmap p1_m4_clk_1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_M4_CLK_CTRL,
		.shift = 16,
		.width = 10,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "m4_clk_1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_m4_clk_1_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_m4_clk_1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_M4_CLK_CTRL,
		.bit_idx = 29,
	},
	.hw.init = &(struct clk_init_data){
		.name = "m4_clk_1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_m4_clk_1_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap p1_m4_clk = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_M4_CLK_CTRL,
		.mask = 0x1,
		.shift = 15,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "m4_clk",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&p1_m4_clk_0.hw,
			&p1_m4_clk_1.hw,
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

#define MESON_P1_SYS_GATE(_name, _reg, _bit)				\
struct clk_regmap _name = {						\
	.data = &(struct clk_regmap_gate_data) {			\
		.offset = (_reg),					\
		.bit_idx = (_bit),					\
	},								\
	.hw.init = &(struct clk_init_data) {				\
		.name = #_name,						\
		.ops = &clk_regmap_gate_ops,				\
		.parent_hws = (const struct clk_hw *[]) {		\
			&p1_sys_clk.hw					\
		},							\
		.num_parents = 1,					\
		.flags = CLK_IGNORE_UNUSED,				\
	},								\
}

/*CLKCTRL_SYS_CLK_EN0_REG0*/
static MESON_P1_SYS_GATE(p1_sys_clk_ddr,		CLKCTRL_SYS_CLK_EN0_REG0, 0);
static MESON_P1_SYS_GATE(p1_sys_clk_ethphy,		CLKCTRL_SYS_CLK_EN0_REG0, 4);
static MESON_P1_SYS_GATE(p1_sys_clk_m4,			CLKCTRL_SYS_CLK_EN0_REG0, 6);
static MESON_P1_SYS_GATE(p1_sys_clk_glb,		CLKCTRL_SYS_CLK_EN0_REG0, 7);
static MESON_P1_SYS_GATE(p1_sys_clk_aocpu,		CLKCTRL_SYS_CLK_EN0_REG0, 13);
static MESON_P1_SYS_GATE(p1_sys_clk_aucpu,		CLKCTRL_SYS_CLK_EN0_REG0, 14);
static MESON_P1_SYS_GATE(p1_sys_clk_dewarpc,		CLKCTRL_SYS_CLK_EN0_REG0, 16);
static MESON_P1_SYS_GATE(p1_sys_clk_dewarpb,		CLKCTRL_SYS_CLK_EN0_REG0, 17);
static MESON_P1_SYS_GATE(p1_sys_clk_dewarpa,		CLKCTRL_SYS_CLK_EN0_REG0, 18);
static MESON_P1_SYS_GATE(p1_sys_clk_ampipe_nand,	CLKCTRL_SYS_CLK_EN0_REG0, 19);
static MESON_P1_SYS_GATE(p1_sys_clk_ampipe_eth,		CLKCTRL_SYS_CLK_EN0_REG0, 20);
static MESON_P1_SYS_GATE(p1_sys_clk_am2axi0,		CLKCTRL_SYS_CLK_EN0_REG0, 21);
static MESON_P1_SYS_GATE(p1_sys_clk_am2axi1,		CLKCTRL_SYS_CLK_EN0_REG0, 22);
static MESON_P1_SYS_GATE(p1_sys_clk_am2axi2,		CLKCTRL_SYS_CLK_EN0_REG0, 23);
static MESON_P1_SYS_GATE(p1_sys_clk_sdemmca,		CLKCTRL_SYS_CLK_EN0_REG0, 25);
static MESON_P1_SYS_GATE(p1_sys_clk_sdemmcc,		CLKCTRL_SYS_CLK_EN0_REG0, 26);
static MESON_P1_SYS_GATE(p1_sys_clk_spifc,		CLKCTRL_SYS_CLK_EN0_REG0, 29);
static MESON_P1_SYS_GATE(p1_sys_clk_msr_clk,		CLKCTRL_SYS_CLK_EN0_REG0, 30);

/*CLKCTRL_SYS_CLK_EN0_REG1*/
static MESON_P1_SYS_GATE(p1_sys_clk_audio,		CLKCTRL_SYS_CLK_EN0_REG1, 0);
static MESON_P1_SYS_GATE(p1_sys_clk_eth,		CLKCTRL_SYS_CLK_EN0_REG1, 3);
static MESON_P1_SYS_GATE(p1_sys_clk_uart_a,		CLKCTRL_SYS_CLK_EN0_REG1, 5);
static MESON_P1_SYS_GATE(p1_sys_clk_uart_b,		CLKCTRL_SYS_CLK_EN0_REG1, 6);
static MESON_P1_SYS_GATE(p1_sys_clk_uart_c,		CLKCTRL_SYS_CLK_EN0_REG1, 7);
static MESON_P1_SYS_GATE(p1_sys_clk_uart_d,		CLKCTRL_SYS_CLK_EN0_REG1, 8);
static MESON_P1_SYS_GATE(p1_sys_clk_uart_e,		CLKCTRL_SYS_CLK_EN0_REG1, 9);
static MESON_P1_SYS_GATE(p1_sys_clk_uart_f,		CLKCTRL_SYS_CLK_EN0_REG1, 10);
static MESON_P1_SYS_GATE(p1_sys_clk_aififo,		CLKCTRL_SYS_CLK_EN0_REG1, 11);
static MESON_P1_SYS_GATE(p1_sys_clk_spicc2,		CLKCTRL_SYS_CLK_EN0_REG1, 12);
static MESON_P1_SYS_GATE(p1_sys_clk_spicc3,		CLKCTRL_SYS_CLK_EN0_REG1, 13);
static MESON_P1_SYS_GATE(p1_sys_clk_spicc4,		CLKCTRL_SYS_CLK_EN0_REG1, 14);
static MESON_P1_SYS_GATE(p1_sys_clk_ts_a76,		CLKCTRL_SYS_CLK_EN0_REG1, 15);
static MESON_P1_SYS_GATE(p1_sys_clk_ts_a55,		CLKCTRL_SYS_CLK_EN0_REG1, 16);
static MESON_P1_SYS_GATE(p1_sys_clk_spicc5,		CLKCTRL_SYS_CLK_EN0_REG1, 17);
static MESON_P1_SYS_GATE(p1_sys_clk_ts_ddr_0,		CLKCTRL_SYS_CLK_EN0_REG1, 18);
static MESON_P1_SYS_GATE(p1_sys_clk_ts_ddr_1,		CLKCTRL_SYS_CLK_EN0_REG1, 19);
static MESON_P1_SYS_GATE(p1_sys_clk_g2d,		CLKCTRL_SYS_CLK_EN0_REG1, 20);
static MESON_P1_SYS_GATE(p1_sys_clk_spicc0,		CLKCTRL_SYS_CLK_EN0_REG1, 21);
static MESON_P1_SYS_GATE(p1_sys_clk_spicc1,		CLKCTRL_SYS_CLK_EN0_REG1, 22);
static MESON_P1_SYS_GATE(p1_sys_clk_pcie,		CLKCTRL_SYS_CLK_EN0_REG1, 24);
static MESON_P1_SYS_GATE(p1_sys_clk_pciephy,		CLKCTRL_SYS_CLK_EN0_REG1, 25);
static MESON_P1_SYS_GATE(p1_sys_clk_usb,		CLKCTRL_SYS_CLK_EN0_REG1, 26);
static MESON_P1_SYS_GATE(p1_sys_clk_pcie_phy0,		CLKCTRL_SYS_CLK_EN0_REG1, 27);
static MESON_P1_SYS_GATE(p1_sys_clk_pcie_phy1,		CLKCTRL_SYS_CLK_EN0_REG1, 28);
static MESON_P1_SYS_GATE(p1_sys_clk_pcie_phy2,		CLKCTRL_SYS_CLK_EN0_REG1, 29);
static MESON_P1_SYS_GATE(p1_sys_clk_i2c_m_a,		CLKCTRL_SYS_CLK_EN0_REG1, 30);
static MESON_P1_SYS_GATE(p1_sys_clk_i2c_m_b,		CLKCTRL_SYS_CLK_EN0_REG1, 31);

/*CLKCTRL_SYS_CLK_EN0_REG2*/
static MESON_P1_SYS_GATE(p1_sys_clk_i2c_m_c,		CLKCTRL_SYS_CLK_EN0_REG2, 0);
static MESON_P1_SYS_GATE(p1_sys_clk_i2c_m_d,		CLKCTRL_SYS_CLK_EN0_REG2, 1);
static MESON_P1_SYS_GATE(p1_sys_clk_i2c_m_e,		CLKCTRL_SYS_CLK_EN0_REG2, 2);
static MESON_P1_SYS_GATE(p1_sys_clk_i2c_m_f,		CLKCTRL_SYS_CLK_EN0_REG2, 3);
static MESON_P1_SYS_GATE(p1_sys_clk_i2c_m_g,		CLKCTRL_SYS_CLK_EN0_REG2, 4);
static MESON_P1_SYS_GATE(p1_sys_clk_i2c_m_h,		CLKCTRL_SYS_CLK_EN0_REG2, 5);
static MESON_P1_SYS_GATE(p1_sys_clk_i2c_m_i,		CLKCTRL_SYS_CLK_EN0_REG2, 6);
static MESON_P1_SYS_GATE(p1_sys_clk_i2c_m_j,		CLKCTRL_SYS_CLK_EN0_REG2, 7);
static MESON_P1_SYS_GATE(p1_sys_clk_i2c_s_a,		CLKCTRL_SYS_CLK_EN0_REG2, 8);
static MESON_P1_SYS_GATE(p1_sys_clk_i2c_m_k,		CLKCTRL_SYS_CLK_EN0_REG2, 9);
static MESON_P1_SYS_GATE(p1_sys_clk_mmc_pclk,		CLKCTRL_SYS_CLK_EN0_REG2, 11);
static MESON_P1_SYS_GATE(p1_sys_clk_isp_pclk,		CLKCTRL_SYS_CLK_EN0_REG2, 17);
static MESON_P1_SYS_GATE(p1_sys_clk_rsa,		CLKCTRL_SYS_CLK_EN0_REG2, 18);
static MESON_P1_SYS_GATE(p1_sys_clk_pclk_sys_cpu_apb,	CLKCTRL_SYS_CLK_EN0_REG2, 19);
static MESON_P1_SYS_GATE(p1_sys_clk_dspa,		CLKCTRL_SYS_CLK_EN0_REG2, 21);
static MESON_P1_SYS_GATE(p1_sys_clk_dspb,		CLKCTRL_SYS_CLK_EN0_REG2, 22);
static MESON_P1_SYS_GATE(p1_sys_clk_sar_adc,		CLKCTRL_SYS_CLK_EN0_REG2, 28);
static MESON_P1_SYS_GATE(p1_sys_clk_gic,		CLKCTRL_SYS_CLK_EN0_REG2, 30);

/*CLKCTRL_SYS_CLK_EN0_REG3*/
static MESON_P1_SYS_GATE(p1_sys_clk_ts_nna,		CLKCTRL_SYS_CLK_EN0_REG3, 0);
static MESON_P1_SYS_GATE(p1_sys_clk_pwm_ab,		CLKCTRL_SYS_CLK_EN0_REG3, 5);
static MESON_P1_SYS_GATE(p1_sys_clk_pwm_cd,		CLKCTRL_SYS_CLK_EN0_REG3, 6);
static MESON_P1_SYS_GATE(p1_sys_clk_pwm_ef,		CLKCTRL_SYS_CLK_EN0_REG3, 7);
static MESON_P1_SYS_GATE(p1_sys_clk_pwm_gh,		CLKCTRL_SYS_CLK_EN0_REG3, 8);
static MESON_P1_SYS_GATE(p1_sys_clk_pwm_ij,		CLKCTRL_SYS_CLK_EN0_REG3, 9);
static MESON_P1_SYS_GATE(p1_sys_clk_depa,		CLKCTRL_SYS_CLK_EN0_REG3, 10);
static MESON_P1_SYS_GATE(p1_sys_clk_depb,		CLKCTRL_SYS_CLK_EN0_REG3, 11);
static MESON_P1_SYS_GATE(p1_sys_clk_mopa,		CLKCTRL_SYS_CLK_EN0_REG3, 12);
static MESON_P1_SYS_GATE(p1_sys_clk_mopb,		CLKCTRL_SYS_CLK_EN0_REG3, 13);
static MESON_P1_SYS_GATE(p1_sys_clk_vfe,		CLKCTRL_SYS_CLK_EN0_REG3, 14);
static MESON_P1_SYS_GATE(p1_sys_clk_i2c_s_b,		CLKCTRL_SYS_CLK_EN0_REG3, 15);
static MESON_P1_SYS_GATE(p1_sys_clk_i2c_s_c,		CLKCTRL_SYS_CLK_EN0_REG3, 16);
static MESON_P1_SYS_GATE(p1_sys_clk_i2c_s_d,		CLKCTRL_SYS_CLK_EN0_REG3, 17);
static MESON_P1_SYS_GATE(p1_sys_clk_i2c_s_e,		CLKCTRL_SYS_CLK_EN0_REG3, 18);
static MESON_P1_SYS_GATE(p1_sys_clk_i2c_s_f,		CLKCTRL_SYS_CLK_EN0_REG3, 19);
static MESON_P1_SYS_GATE(p1_sys_clk_i2c_s_g,		CLKCTRL_SYS_CLK_EN0_REG3, 20);
static MESON_P1_SYS_GATE(p1_sys_clk_i2c_s_h,		CLKCTRL_SYS_CLK_EN0_REG3, 21);
static MESON_P1_SYS_GATE(p1_sys_clk_i2c_s_i,		CLKCTRL_SYS_CLK_EN0_REG3, 22);
static MESON_P1_SYS_GATE(p1_sys_clk_i2c_s_j,		CLKCTRL_SYS_CLK_EN0_REG3, 23);
static MESON_P1_SYS_GATE(p1_sys_clk_i2c_s_k,		CLKCTRL_SYS_CLK_EN0_REG3, 24);

/* Array of all clocks provided by this provider */
static struct clk_hw_onecell_data p1_hw_onecell_data = {
	.hws = {
		[CLKID_SYS_PLL_DCO]			= &p1_sys_pll_dco.hw,
		[CLKID_SYS_PLL]				= &p1_sys_pll.hw,
		[CLKID_SYS1_PLL_DCO]			= &p1_sys1_pll_dco.hw,
		[CLKID_SYS1_PLL]			= &p1_sys1_pll.hw,
		[CLKID_FIXED_PLL_DCO]			= &p1_fixed_pll_dco.hw,
		[CLKID_FIXED_PLL]			= &p1_fixed_pll.hw,
		[CLKID_FCLK_DIV2_DIV]			= &p1_fclk_div2_div.hw,
		[CLKID_FCLK_DIV2]			= &p1_fclk_div2.hw,
		[CLKID_FCLK_DIV3_DIV]			= &p1_fclk_div3_div.hw,
		[CLKID_FCLK_DIV3]			= &p1_fclk_div3.hw,
		[CLKID_FCLK_DIV4_DIV]			= &p1_fclk_div4_div.hw,
		[CLKID_FCLK_DIV4]			= &p1_fclk_div4.hw,
		[CLKID_FCLK_DIV5_DIV]			= &p1_fclk_div5_div.hw,
		[CLKID_FCLK_DIV5]			= &p1_fclk_div5.hw,
		[CLKID_FCLK_DIV7_DIV]			= &p1_fclk_div7_div.hw,
		[CLKID_FCLK_DIV7]			= &p1_fclk_div7.hw,
		[CLKID_FCLK_DIV2P5_DIV]			= &p1_fclk_div2p5_div.hw,
		[CLKID_FCLK_DIV2P5]			= &p1_fclk_div2p5.hw,
		[CLKID_GP0_PLL_DCO]			= &p1_gp0_pll_dco.hw,
		[CLKID_GP0_PLL]				= &p1_gp0_pll.hw,
		[CLKID_GP1_PLL_DCO]			= &p1_gp1_pll_dco.hw,
		[CLKID_GP1_PLL]				= &p1_gp1_pll.hw,
		[CLKID_CPU_DYN_CLK]			= &p1_cpu_dyn_clk.hw,
		[CLKID_CPU_CLK]				= &p1_cpu_clk.hw,
		[CLKID_A76_DYN_CLK]			= &p1_a76_dyn_clk.hw,
		[CLKID_A76_CLK]				= &p1_a76_clk.hw,
		[CLKID_DSU_DYN_CLK]			= &p1_dsu_dyn_clk.hw,
		[CLKID_DSU_CLK]				= &p1_dsu_clk.hw,
		[CLKID_DSU_FINAL_CLK]			= &p1_dsu_final_clk.hw,
		[CLKID_CPU6_CLK]			= &p1_cpu6_clk.hw,
		[CLKID_CPU7_CLK]			= &p1_cpu7_clk.hw,
		[CLKID_HIFI_PLL_DCO]			= &p1_hifi_pll_dco.hw,
		[CLKID_HIFI_PLL]			= &p1_hifi_pll.hw,
		[CLKID_PCIE_PLL_DCO]			= &p1_pcie_pll_dco.hw,
		[CLKID_PCIE_PLL_DCO_DIV2]		= &p1_pcie_pll_dco_div2.hw,
		[CLKID_PCIE_PLL_OD]			= &p1_pcie_pll_od.hw,
		[CLKID_PCIE_PLL]			= &p1_pcie_pll.hw,
		[CLKID_PCIE_HCSL]			= &p1_pcie_hcsl.hw,
		[CLKID_MCLK_PLL_DCO]			= &p1_mclk_pll_dco.hw,
		[CLKID_MCLK_0_PRE_OD]			= &p1_mclk_0_pre_od.hw,
		[CLKID_MCLK_0_PLL]			= &p1_mclk_0_pll.hw,
		[CLKID_MCLK_0_SEL]			= &p1_mclk_0_sel.hw,
		[CLKID_MCLK_0_DIV2]			= &p1_mclk_0_div2.hw,
		[CLKID_MCLK_0_DIV_EN]			= &p1_mclk_0_div_en.hw,
		[CLKID_MCLK_0]				= &p1_mclk_0.hw,
		[CLKID_MCLK_1_PRE_OD]			= &p1_mclk_1_pre_od.hw,
		[CLKID_MCLK_1_PLL]			= &p1_mclk_1_pll.hw,
		[CLKID_MCLK_1_SEL]			= &p1_mclk_1_sel.hw,
		[CLKID_MCLK_1_DIV2]			= &p1_mclk_1_div2.hw,
		[CLKID_MCLK_1_DIV_EN]			= &p1_mclk_1_div_en.hw,
		[CLKID_MCLK_1]				= &p1_mclk_1.hw,
		[CLKID_MCLK_2_PRE_OD]			= &p1_mclk_2_pre_od.hw,
		[CLKID_MCLK_2_PLL]			= &p1_mclk_2_pll.hw,
		[CLKID_MCLK_2_SEL]			= &p1_mclk_2_sel.hw,
		[CLKID_MCLK_2_DIV2]			= &p1_mclk_2_div2.hw,
		[CLKID_MCLK_2_DIV_EN]			= &p1_mclk_2_div_en.hw,
		[CLKID_MCLK_2]				= &p1_mclk_2.hw,
		[CLKID_MCLK_3_PRE_OD]			= &p1_mclk_3_pre_od.hw,
		[CLKID_MCLK_3_PLL]			= &p1_mclk_3_pll.hw,
		[CLKID_MCLK_3_SEL]			= &p1_mclk_3_sel.hw,
		[CLKID_MCLK_3_DIV2]			= &p1_mclk_3_div2.hw,
		[CLKID_MCLK_3_DIV_EN]			= &p1_mclk_3_div_en.hw,
		[CLKID_MCLK_3]				= &p1_mclk_3.hw,
		[CLKID_MCLK_4_PRE_OD]			= &p1_mclk_4_pre_od.hw,
		[CLKID_MCLK_4_PLL]			= &p1_mclk_4_pll.hw,
		[CLKID_MCLK_4_SEL]			= &p1_mclk_4_sel.hw,
		[CLKID_MCLK_4_DIV2]			= &p1_mclk_4_div2.hw,
		[CLKID_MCLK_4_DIV_EN]			= &p1_mclk_4_div_en.hw,
		[CLKID_MCLK_4]				= &p1_mclk_4.hw,
		[CLKID_MCLK_0_PLL_DIV_EN]	= &p1_mclk_0_pll_div_en.hw,
		[CLKID_MCLK_1_PLL_DIV_EN]	= &p1_mclk_1_pll_div_en.hw,
		[CLKID_MCLK_2_PLL_DIV_EN]	= &p1_mclk_2_pll_div_en.hw,
		[CLKID_MCLK_3_PLL_DIV_EN]	= &p1_mclk_3_pll_div_en.hw,
		[CLKID_MCLK_4_PLL_DIV_EN]	= &p1_mclk_4_pll_div_en.hw,
		[CLKID_MPLL_50M_DIV]			= &p1_mpll_50m_div.hw,
		[CLKID_MPLL_50M]			= &p1_mpll_50m.hw,
		[CLKID_MPLL_PREDIV]			= &p1_mpll_prediv.hw,
		[CLKID_MPLL0_DIV]			= &p1_mpll0_div.hw,
		[CLKID_MPLL0]				= &p1_mpll0.hw,
		[CLKID_MPLL1_DIV]			= &p1_mpll1_div.hw,
		[CLKID_MPLL1]				= &p1_mpll1.hw,
		[CLKID_MPLL2_DIV]			= &p1_mpll2_div.hw,
		[CLKID_MPLL2]				= &p1_mpll2.hw,
		[CLKID_MPLL3_DIV]			= &p1_mpll3_div.hw,
		[CLKID_MPLL3]				= &p1_mpll3.hw,
		[CLKID_GP2_PLL_DCO]			= &p1_gp2_pll_dco.hw,
		[CLKID_GP2_PLL]				= &p1_gp2_pll.hw,
		[CLKID_NNA_PLL_DCO]			= &p1_nna_pll_dco.hw,
		[CLKID_NNA_PLL]				= &p1_nna_pll.hw,
		[CLKID_M4_PLL_DCO]			= &p1_m4_pll_dco.hw,
		[CLKID_M4_PLL]				= &p1_m4_pll.hw,
		[CLKID_FDLE_PLL_DCO]			= &p1_fdle_pll_dco.hw,
		[CLKID_FDLE_PLL]			= &p1_fdle_pll.hw,
		[CLKID_RTC_32K_CLKIN]			= &p1_rtc_32k_clkin.hw,
		[CLKID_RTC_32K_DIV]			= &p1_rtc_32k_div.hw,
		[CLKID_RTC_32K_XATL]			= &p1_rtc_32k_xtal.hw,
		[CLKID_RTC_32K_SEL]			= &p1_rtc_32k_sel.hw,
		[CLKID_RTC_CLK]				= &p1_rtc_clk.hw,
		[CLKID_SYS_CLK_1_SEL]			= &p1_sysclk_1_sel.hw,
		[CLKID_SYS_CLK_1_DIV]			= &p1_sysclk_1_div.hw,
		[CLKID_SYS_CLK_1]			= &p1_sysclk_1.hw,
		[CLKID_SYS_CLK_0_SEL]			= &p1_sysclk_0_sel.hw,
		[CLKID_SYS_CLK_0_DIV]			= &p1_sysclk_0_div.hw,
		[CLKID_SYS_CLK_0]			= &p1_sysclk_0.hw,
		[CLKID_SYS_CLK]				= &p1_sys_clk.hw,
		[CLKID_DSPA_1_SEL]			= &p1_dspa_1_sel.hw,
		[CLKID_DSPA_1_DIV]			= &p1_dspa_1_div.hw,
		[CLKID_DSPA_1]				= &p1_dspa_1.hw,
		[CLKID_DSPA_0_SEL]			= &p1_dspa_0_sel.hw,
		[CLKID_DSPA_0_DIV]			= &p1_dspa_0_div.hw,
		[CLKID_DSPA_0]				= &p1_dspa_0.hw,
		[CLKID_DSPA]				= &p1_dspa.hw,
		[CLKID_DSPB_1_SEL]			= &p1_dspb_1_sel.hw,
		[CLKID_DSPB_1_DIV]			= &p1_dspb_1_div.hw,
		[CLKID_DSPB_1]				= &p1_dspb_1.hw,
		[CLKID_DSPB_0_SEL]			= &p1_dspb_0_sel.hw,
		[CLKID_DSPB_0_DIV]			= &p1_dspb_0_div.hw,
		[CLKID_DSPB_0]				= &p1_dspb_0.hw,
		[CLKID_DSPB]				= &p1_dspb.hw,
		[CLKID_24M_CLK_GATE]			= &p1_24m_clk_gate.hw,
		[CLKID_24M_DIV2]			= &p1_24m_div2.hw,
		[CLKID_12M_CLK]				= &p1_12m_clk.hw,
		[CLKID_25M_CLK_DIV]			= &p1_25m_clk_div.hw,
		[CLKID_25M_CLK]				= &p1_25m_clk.hw,
		[CLKID_VAPB_0_SEL]			= &p1_vapb_sel.hw,
		[CLKID_VAPB_0_DIV]			= &p1_vapb_div.hw,
		[CLKID_VAPB_0]				= &p1_vapb.hw,
		[CLKID_GE2D_SEL]			= &p1_ge2d_sel.hw,
		[CLKID_GE2D_DIV]			= &p1_ge2d_div.hw,
		[CLKID_GE2D]				= &p1_ge2d.hw,
		[CLKID_SD_EMMC_C_CLK_SEL]		= &p1_sd_emmc_c_sel.hw,
		[CLKID_SD_EMMC_C_CLK_DIV]		= &p1_sd_emmc_c_div.hw,
		[CLKID_SD_EMMC_C_CLK]			= &p1_sd_emmc_c.hw,
		[CLKID_SD_EMMC_A_CLK_SEL]		= &p1_sd_emmc_a_sel.hw,
		[CLKID_SD_EMMC_A_CLK_DIV]		= &p1_sd_emmc_a_div.hw,
		[CLKID_SD_EMMC_A_CLK]			= &p1_sd_emmc_a.hw,
		[CLKID_SPICC0_SEL]			= &p1_spicc0_sel.hw,
		[CLKID_SPICC0_DIV]			= &p1_spicc0_div.hw,
		[CLKID_SPICC0]				= &p1_spicc0.hw,
		[CLKID_SPICC1_SEL]			= &p1_spicc1_sel.hw,
		[CLKID_SPICC1_DIV]			= &p1_spicc1_div.hw,
		[CLKID_SPICC1]				= &p1_spicc1.hw,
		[CLKID_SPICC2_SEL]			= &p1_spicc2_sel.hw,
		[CLKID_SPICC2_DIV]			= &p1_spicc2_div.hw,
		[CLKID_SPICC2]				= &p1_spicc2.hw,
		[CLKID_SPICC3_SEL]			= &p1_spicc3_sel.hw,
		[CLKID_SPICC3_DIV]			= &p1_spicc3_div.hw,
		[CLKID_SPICC3]				= &p1_spicc3.hw,
		[CLKID_SPICC4_SEL]			= &p1_spicc4_sel.hw,
		[CLKID_SPICC4_DIV]			= &p1_spicc4_div.hw,
		[CLKID_SPICC4]				= &p1_spicc4.hw,
		[CLKID_SPICC5_SEL]			= &p1_spicc5_sel.hw,
		[CLKID_SPICC5_DIV]			= &p1_spicc5_div.hw,
		[CLKID_SPICC5]				= &p1_spicc5.hw,
		[CLKID_PWM_A_SEL]			= &p1_pwm_a_sel.hw,
		[CLKID_PWM_A_DIV]			= &p1_pwm_a_div.hw,
		[CLKID_PWM_A]				= &p1_pwm_a.hw,
		[CLKID_PWM_B_SEL]			= &p1_pwm_b_sel.hw,
		[CLKID_PWM_B_DIV]			= &p1_pwm_b_div.hw,
		[CLKID_PWM_B]				= &p1_pwm_b.hw,
		[CLKID_PWM_C_SEL]			= &p1_pwm_c_sel.hw,
		[CLKID_PWM_C_DIV]			= &p1_pwm_c_div.hw,
		[CLKID_PWM_C]				= &p1_pwm_c.hw,
		[CLKID_PWM_D_SEL]			= &p1_pwm_d_sel.hw,
		[CLKID_PWM_D_DIV]			= &p1_pwm_d_div.hw,
		[CLKID_PWM_D]				= &p1_pwm_d.hw,
		[CLKID_PWM_E_SEL]			= &p1_pwm_e_sel.hw,
		[CLKID_PWM_E_DIV]			= &p1_pwm_e_div.hw,
		[CLKID_PWM_E]				= &p1_pwm_e.hw,
		[CLKID_PWM_F_SEL]			= &p1_pwm_f_sel.hw,
		[CLKID_PWM_F_DIV]			= &p1_pwm_f_div.hw,
		[CLKID_PWM_F]				= &p1_pwm_f.hw,
		[CLKID_PWM_G_SEL]			= &p1_pwm_g_sel.hw,
		[CLKID_PWM_G_DIV]			= &p1_pwm_g_div.hw,
		[CLKID_PWM_G]				= &p1_pwm_g.hw,
		[CLKID_PWM_H_SEL]			= &p1_pwm_h_sel.hw,
		[CLKID_PWM_H_DIV]			= &p1_pwm_h_div.hw,
		[CLKID_PWM_H]				= &p1_pwm_h.hw,
		[CLKID_PWM_I_SEL]			= &p1_pwm_i_sel.hw,
		[CLKID_PWM_I_DIV]			= &p1_pwm_i_div.hw,
		[CLKID_PWM_I]				= &p1_pwm_i.hw,
		[CLKID_PWM_J_SEL]			= &p1_pwm_j_sel.hw,
		[CLKID_PWM_J_DIV]			= &p1_pwm_j_div.hw,
		[CLKID_PWM_J]				= &p1_pwm_j.hw,
		[CLKID_SARADC_SEL]			= &p1_saradc_sel.hw,
		[CLKID_SARADC_DIV]			= &p1_saradc_div.hw,
		[CLKID_SARADC]				= &p1_saradc.hw,
		[CLKID_GEN_SEL]				= &p1_gen_sel.hw,
		[CLKID_GEN_DIV]				= &p1_gen_div.hw,
		[CLKID_GEN]				= &p1_gen.hw,
		[CLKID_ETH_RMII_SEL]			= &p1_eth_rmii_sel.hw,
		[CLKID_ETH_RMII_DIV]			= &p1_eth_rmii_div.hw,
		[CLKID_ETH_RMII]			= &p1_eth_rmii.hw,
		[CLKID_ETH_DIV8]			= &p1_eth_div8.hw,
		[CLKID_ETH_125M]			= &p1_eth_125m.hw,
		[CLKID_TS_CLK_DIV]			= &p1_ts_clk_div.hw,
		[CLKID_TS_CLK]				= &p1_ts_clk.hw,
		[CLKID_USB_250M_SEL]			= &p1_usb_250m_sel.hw,
		[CLKID_USB_250M_DIV]			= &p1_usb_250m_div.hw,
		[CLKID_USB_250M]			= &p1_usb_250m.hw,
		[CLKID_PCIE_400M_SEL]			= &p1_pcie_400m_sel.hw,
		[CLKID_PCIE_400M_DIV]			= &p1_pcie_400m_div.hw,
		[CLKID_PCIE_400M]			= &p1_pcie_400m.hw,
		[CLKID_PCIE_CLK_SEL]			= &p1_pcie_clk_sel.hw,
		[CLKID_PCIE_CLK_DIV]			= &p1_pcie_clk_div.hw,
		[CLKID_PCIE_CLK]			= &p1_pcie_clk.hw,
		[CLKID_PCIE_TL_CLK_SEL]			= &p1_pcie_tl_sel.hw,
		[CLKID_PCIE_TL_CLK_DIV]			= &p1_pcie_tl_div.hw,
		[CLKID_PCIE_TL_CLK]			= &p1_pcie_tl.hw,
		[CLKID_NNA0_SEL]			= &p1_nna0_sel.hw,
		[CLKID_NNA0_DIV]			= &p1_nna0_div.hw,
		[CLKID_NNA0]				= &p1_nna0.hw,
		[CLKID_NNA1_SEL]			= &p1_nna1_sel.hw,
		[CLKID_NNA1_DIV]			= &p1_nna1_div.hw,
		[CLKID_NNA1]				= &p1_nna1.hw,
		[CLKID_NNA2_SEL]			= &p1_nna2_sel.hw,
		[CLKID_NNA2_DIV]			= &p1_nna2_div.hw,
		[CLKID_NNA2]				= &p1_nna2.hw,
		[CLKID_NNA3_SEL]			= &p1_nna3_sel.hw,
		[CLKID_NNA3_DIV]			= &p1_nna3_div.hw,
		[CLKID_NNA3]				= &p1_nna3.hw,
		[CLKID_NNA4_SEL]			= &p1_nna4_sel.hw,
		[CLKID_NNA4_DIV]			= &p1_nna4_div.hw,
		[CLKID_NNA4]				= &p1_nna4.hw,
		[CLKID_NNA5_SEL]			= &p1_nna5_sel.hw,
		[CLKID_NNA5_DIV]			= &p1_nna5_div.hw,
		[CLKID_NNA5]				= &p1_nna5.hw,
		[CLKID_ISP0_SEL]			= &p1_isp0_sel.hw,
		[CLKID_ISP0_DIV]			= &p1_isp0_div.hw,
		[CLKID_ISP0]				= &p1_isp0.hw,
		[CLKID_ISP1_SEL]			= &p1_isp1_sel.hw,
		[CLKID_ISP1_DIV]			= &p1_isp1_div.hw,
		[CLKID_ISP1]				= &p1_isp1.hw,
		[CLKID_ISP2_SEL]			= &p1_isp2_sel.hw,
		[CLKID_ISP2_DIV]			= &p1_isp2_div.hw,
		[CLKID_ISP2]				= &p1_isp2.hw,
		[CLKID_ISP3_SEL]			= &p1_isp3_sel.hw,
		[CLKID_ISP3_DIV]			= &p1_isp3_div.hw,
		[CLKID_ISP3]				= &p1_isp3.hw,
		[CLKID_ISP4_SEL]			= &p1_isp4_sel.hw,
		[CLKID_ISP4_DIV]			= &p1_isp4_div.hw,
		[CLKID_ISP4]				= &p1_isp4.hw,
		[CLKID_CSIPHY0_SEL]			= &p1_csiphy0_sel.hw,
		[CLKID_CSIPHY0_DIV]			= &p1_csiphy0_div.hw,
		[CLKID_CSIPHY0]				= &p1_csiphy0.hw,
		[CLKID_CSIPHY1_SEL]			= &p1_csiphy1_sel.hw,
		[CLKID_CSIPHY1_DIV]			= &p1_csiphy1_div.hw,
		[CLKID_CSIPHY1]				= &p1_csiphy1.hw,
		[CLKID_CSIPHY2_SEL]			= &p1_csiphy2_sel.hw,
		[CLKID_CSIPHY2_DIV]			= &p1_csiphy2_div.hw,
		[CLKID_CSIPHY2]				= &p1_csiphy2.hw,
		[CLKID_CSIPHY3_SEL]			= &p1_csiphy3_sel.hw,
		[CLKID_CSIPHY3_DIV]			= &p1_csiphy3_div.hw,
		[CLKID_CSIPHY3]				= &p1_csiphy3.hw,
		[CLKID_CSIPHY4_SEL]			= &p1_csiphy4_sel.hw,
		[CLKID_CSIPHY4_DIV]			= &p1_csiphy4_div.hw,
		[CLKID_CSIPHY4]				= &p1_csiphy4.hw,
		[CLKID_MOPA_SEL]			= &p1_mopa_sel.hw,
		[CLKID_MOPA_DIV]			= &p1_mopa_div.hw,
		[CLKID_MOPA]				= &p1_mopa.hw,
		[CLKID_MOPB_SEL]			= &p1_mopb_sel.hw,
		[CLKID_MOPB_DIV]			= &p1_mopb_div.hw,
		[CLKID_MOPB]				= &p1_mopb.hw,
		[CLKID_DEPA_SEL]			= &p1_depa_sel.hw,
		[CLKID_DEPA_DIV]			= &p1_depa_div.hw,
		[CLKID_DEPA]				= &p1_depa.hw,
		[CLKID_DEPB_SEL]			= &p1_depb_sel.hw,
		[CLKID_DEPB_DIV]			= &p1_depb_div.hw,
		[CLKID_DEPB]				= &p1_depb.hw,
		[CLKID_VFE_SEL]				= &p1_vfe_sel.hw,
		[CLKID_VFE_DIV]				= &p1_vfe_div.hw,
		[CLKID_VFE]				= &p1_vfe.hw,
		[CLKID_DEWARPA_SEL]			= &p1_dewarpa_sel.hw,
		[CLKID_DEWARPA_DIV]			= &p1_dewarpa_div.hw,
		[CLKID_DEWARPA]				= &p1_dewarpa.hw,
		[CLKID_DEWARPB_SEL]			= &p1_dewarpb_sel.hw,
		[CLKID_DEWARPB_DIV]			= &p1_dewarpb_div.hw,
		[CLKID_DEWARPB]				= &p1_dewarpb.hw,
		[CLKID_DEWARPC_SEL]			= &p1_dewarpc_sel.hw,
		[CLKID_DEWARPC_DIV]			= &p1_dewarpc_div.hw,
		[CLKID_DEWARPC]				= &p1_dewarpc.hw,
		[CLKID_M4_CLK_0_SEL]			= &p1_m4_clk_0_sel.hw,
		[CLKID_M4_CLK_0_DIV]			= &p1_m4_clk_0_div.hw,
		[CLKID_M4_CLK_0]			= &p1_m4_clk_0.hw,
		[CLKID_M4_CLK_1_SEL]			= &p1_m4_clk_1_sel.hw,
		[CLKID_M4_CLK_1_DIV]			= &p1_m4_clk_1_div.hw,
		[CLKID_M4_CLK_1]			= &p1_m4_clk_1.hw,
		[CLKID_M4_CLK]				= &p1_m4_clk.hw,
		[CLKID_SYS_CLK_DDR]			= &p1_sys_clk_ddr.hw,
		[CLKID_SYS_CLK_ETHPHY]			= &p1_sys_clk_ethphy.hw,
		[CLKID_SYS_CLK_M4]			= &p1_sys_clk_m4.hw,
		[CLKID_SYS_CLK_GLB]			= &p1_sys_clk_glb.hw,
		[CLKID_SYS_CLK_AOCPU]			= &p1_sys_clk_aocpu.hw,
		[CLKID_SYS_CLK_AUCPU]			= &p1_sys_clk_aucpu.hw,
		[CLKID_SYS_CLK_DEWARPC]			= &p1_sys_clk_dewarpc.hw,
		[CLKID_SYS_CLK_DEWARPB]			= &p1_sys_clk_dewarpb.hw,
		[CLKID_SYS_CLK_DEWARPA]			= &p1_sys_clk_dewarpa.hw,
		[CLKID_SYS_CLK_AMPIPE_NAND]		= &p1_sys_clk_ampipe_nand.hw,
		[CLKID_SYS_CLK_AMPIPE_ETH]		= &p1_sys_clk_ampipe_eth.hw,
		[CLKID_SYS_CLK_AM2AXI0]			= &p1_sys_clk_am2axi0.hw,
		[CLKID_SYS_CLK_AM2AXI1]			= &p1_sys_clk_am2axi1.hw,
		[CLKID_SYS_CLK_AM2AXI2]			= &p1_sys_clk_am2axi2.hw,
		[CLKID_SYS_CLK_SD_EMMC_A]		= &p1_sys_clk_sdemmca.hw,
		[CLKID_SYS_CLK_SD_EMMC_C]		= &p1_sys_clk_sdemmcc.hw,
		[CLKID_SYS_CLK_SPIFC]			= &p1_sys_clk_spifc.hw,
		[CLKID_SYS_CLK_MSR_CLK]			= &p1_sys_clk_msr_clk.hw,
		[CLKID_SYS_CLK_AUDIO]			= &p1_sys_clk_audio.hw,
		[CLKID_SYS_CLK_ETH]			= &p1_sys_clk_eth.hw,
		[CLKID_SYS_CLK_UART_A]			= &p1_sys_clk_uart_a.hw,
		[CLKID_SYS_CLK_UART_B]			= &p1_sys_clk_uart_b.hw,
		[CLKID_SYS_CLK_UART_C]			= &p1_sys_clk_uart_c.hw,
		[CLKID_SYS_CLK_UART_D]			= &p1_sys_clk_uart_d.hw,
		[CLKID_SYS_CLK_UART_E]			= &p1_sys_clk_uart_e.hw,
		[CLKID_SYS_CLK_UART_F]			= &p1_sys_clk_uart_f.hw,
		[CLKID_SYS_CLK_AIFIFO]			= &p1_sys_clk_aififo.hw,
		[CLKID_SYS_CLK_SPICC2]			= &p1_sys_clk_spicc2.hw,
		[CLKID_SYS_CLK_SPICC3]			= &p1_sys_clk_spicc3.hw,
		[CLKID_SYS_CLK_SPICC4]			= &p1_sys_clk_spicc4.hw,
		[CLKID_SYS_CLK_TS_A76]			= &p1_sys_clk_ts_a76.hw,
		[CLKID_SYS_CLK_TS_A55]			= &p1_sys_clk_ts_a55.hw,
		[CLKID_SYS_CLK_SPICC5]			= &p1_sys_clk_spicc5.hw,
		[CLKID_SYS_CLK_TS_DDR_0]		= &p1_sys_clk_ts_ddr_0.hw,
		[CLKID_SYS_CLK_TS_DDR_1]		= &p1_sys_clk_ts_ddr_1.hw,
		[CLKID_SYS_CLK_G2D]			= &p1_sys_clk_g2d.hw,
		[CLKID_SYS_CLK_SPICC0]			= &p1_sys_clk_spicc0.hw,
		[CLKID_SYS_CLK_SPICC1]			= &p1_sys_clk_spicc1.hw,
		[CLKID_SYS_CLK_PCIE]			= &p1_sys_clk_pcie.hw,
		[CLKID_SYS_CLK_PCIEPHY]			= &p1_sys_clk_pciephy.hw,
		[CLKID_SYS_CLK_USB]			= &p1_sys_clk_usb.hw,
		[CLKID_SYS_CLK_PCIE_PHY0]		= &p1_sys_clk_pcie_phy0.hw,
		[CLKID_SYS_CLK_PCIE_PHY1]		= &p1_sys_clk_pcie_phy1.hw,
		[CLKID_SYS_CLK_PCIE_PHY2]		= &p1_sys_clk_pcie_phy2.hw,
		[CLKID_SYS_CLK_I2C_M_A]			= &p1_sys_clk_i2c_m_a.hw,
		[CLKID_SYS_CLK_I2C_M_B]			= &p1_sys_clk_i2c_m_b.hw,
		[CLKID_SYS_CLK_I2C_M_C]			= &p1_sys_clk_i2c_m_c.hw,
		[CLKID_SYS_CLK_I2C_M_D]			= &p1_sys_clk_i2c_m_d.hw,
		[CLKID_SYS_CLK_I2C_M_E]			= &p1_sys_clk_i2c_m_e.hw,
		[CLKID_SYS_CLK_I2C_M_F]			= &p1_sys_clk_i2c_m_f.hw,
		[CLKID_SYS_CLK_I2C_M_G]			= &p1_sys_clk_i2c_m_g.hw,
		[CLKID_SYS_CLK_I2C_M_H]			= &p1_sys_clk_i2c_m_h.hw,
		[CLKID_SYS_CLK_I2C_M_I]			= &p1_sys_clk_i2c_m_i.hw,
		[CLKID_SYS_CLK_I2C_M_J]			= &p1_sys_clk_i2c_m_j.hw,
		[CLKID_SYS_CLK_I2C_S_A]			= &p1_sys_clk_i2c_s_a.hw,
		[CLKID_SYS_CLK_I2C_M_K]			= &p1_sys_clk_i2c_m_k.hw,
		[CLKID_SYS_CLK_MMC_PCLK]		= &p1_sys_clk_mmc_pclk.hw,
		[CLKID_SYS_CLK_ISP_PCLK]		= &p1_sys_clk_isp_pclk.hw,
		[CLKID_SYS_CLK_RSA]			= &p1_sys_clk_rsa.hw,
		[CLKID_SYS_CLK_PCLK_SYS_CPU_APB]	= &p1_sys_clk_pclk_sys_cpu_apb.hw,
		[CLKID_SYS_CLK_DSPA]			= &p1_sys_clk_dspa.hw,
		[CLKID_SYS_CLK_DSPB]			= &p1_sys_clk_dspb.hw,
		[CLKID_SYS_CLK_SAR_ADC]			= &p1_sys_clk_sar_adc.hw,
		[CLKID_SYS_CLK_GIC]			= &p1_sys_clk_gic.hw,
		[CLKID_SYS_CLK_TS_NNA]			= &p1_sys_clk_ts_nna.hw,
		[CLKID_SYS_CLK_PWM_AB]			= &p1_sys_clk_pwm_ab.hw,
		[CLKID_SYS_CLK_PWM_CD]			= &p1_sys_clk_pwm_cd.hw,
		[CLKID_SYS_CLK_PWM_EF]			= &p1_sys_clk_pwm_ef.hw,
		[CLKID_SYS_CLK_PWM_GH]			= &p1_sys_clk_pwm_gh.hw,
		[CLKID_SYS_CLK_PWM_IJ]			= &p1_sys_clk_pwm_ij.hw,
		[CLKID_SYS_CLK_DEPA]			= &p1_sys_clk_depa.hw,
		[CLKID_SYS_CLK_DEPB]			= &p1_sys_clk_depb.hw,
		[CLKID_SYS_CLK_MOPA]			= &p1_sys_clk_mopa.hw,
		[CLKID_SYS_CLK_MOPB]			= &p1_sys_clk_mopb.hw,
		[CLKID_SYS_CLK_VFE]			= &p1_sys_clk_vfe.hw,
		[CLKID_SYS_CLK_I2C_S_B]			= &p1_sys_clk_i2c_s_b.hw,
		[CLKID_SYS_CLK_I2C_S_C]			= &p1_sys_clk_i2c_s_c.hw,
		[CLKID_SYS_CLK_I2C_S_D]			= &p1_sys_clk_i2c_s_d.hw,
		[CLKID_SYS_CLK_I2C_S_E]			= &p1_sys_clk_i2c_s_e.hw,
		[CLKID_SYS_CLK_I2C_S_F]			= &p1_sys_clk_i2c_s_f.hw,
		[CLKID_SYS_CLK_I2C_S_G]			= &p1_sys_clk_i2c_s_g.hw,
		[CLKID_SYS_CLK_I2C_S_H]			= &p1_sys_clk_i2c_s_h.hw,
		[CLKID_SYS_CLK_I2C_S_I]			= &p1_sys_clk_i2c_s_i.hw,
		[CLKID_SYS_CLK_I2C_S_J]			= &p1_sys_clk_i2c_s_j.hw,
		[CLKID_SYS_CLK_I2C_S_K]			= &p1_sys_clk_i2c_s_k.hw,
		[NR_CLKS]				= NULL
	},
	.num = NR_CLKS,
};

/* Convenience table to populate regmap in .probe */
static struct clk_regmap *const p1_clk_regmaps[] = {
	&p1_rtc_32k_clkin,
	&p1_rtc_32k_div,
	&p1_rtc_32k_xtal,
	&p1_rtc_32k_sel,
	&p1_rtc_clk,
	&p1_sysclk_1_sel,
	&p1_sysclk_1_div,
	&p1_sysclk_1,
	&p1_sysclk_0_sel,
	&p1_sysclk_0_div,
	&p1_sysclk_0,
	&p1_sys_clk,
	&p1_dspa_0_sel,
	&p1_dspa_0_div,
	&p1_dspa_0,
	&p1_dspa_1_sel,
	&p1_dspa_1_div,
	&p1_dspa_1,
	&p1_dspa,
	&p1_dspb_0_sel,
	&p1_dspb_0_div,
	&p1_dspb_0,
	&p1_dspb_1_sel,
	&p1_dspb_1_div,
	&p1_dspb_1,
	&p1_dspb,
	&p1_24m_clk_gate,
	&p1_12m_clk,
	&p1_25m_clk_div,
	&p1_25m_clk,
	&p1_vapb_sel,
	&p1_vapb_div,
	&p1_vapb,
	&p1_ge2d_sel,
	&p1_ge2d_div,
	&p1_ge2d,
	&p1_sd_emmc_c_sel,
	&p1_sd_emmc_c_div,
	&p1_sd_emmc_c,
	&p1_sd_emmc_a_sel,
	&p1_sd_emmc_a_div,
	&p1_sd_emmc_a,
	&p1_spicc0_sel,
	&p1_spicc0_div,
	&p1_spicc0,
	&p1_spicc1_sel,
	&p1_spicc1_div,
	&p1_spicc1,
	&p1_spicc2_sel,
	&p1_spicc2_div,
	&p1_spicc2,
	&p1_spicc3_sel,
	&p1_spicc3_div,
	&p1_spicc3,
	&p1_spicc4_sel,
	&p1_spicc4_div,
	&p1_spicc4,
	&p1_spicc5_sel,
	&p1_spicc5_div,
	&p1_spicc5,
	&p1_pwm_a_sel,
	&p1_pwm_a_div,
	&p1_pwm_a,
	&p1_pwm_b_sel,
	&p1_pwm_b_div,
	&p1_pwm_b,
	&p1_pwm_c_sel,
	&p1_pwm_c_div,
	&p1_pwm_c,
	&p1_pwm_d_sel,
	&p1_pwm_d_div,
	&p1_pwm_d,
	&p1_pwm_e_sel,
	&p1_pwm_e_div,
	&p1_pwm_e,
	&p1_pwm_f_sel,
	&p1_pwm_f_div,
	&p1_pwm_f,
	&p1_pwm_g_sel,
	&p1_pwm_g_div,
	&p1_pwm_g,
	&p1_pwm_h_sel,
	&p1_pwm_h_div,
	&p1_pwm_h,
	&p1_pwm_i_sel,
	&p1_pwm_i_div,
	&p1_pwm_i,
	&p1_pwm_j_sel,
	&p1_pwm_j_div,
	&p1_pwm_j,
	&p1_saradc_sel,
	&p1_saradc_div,
	&p1_saradc,
	&p1_gen_sel,
	&p1_gen_div,
	&p1_gen,
	&p1_eth_rmii_sel,
	&p1_eth_rmii_div,
	&p1_eth_rmii,
	&p1_eth_125m,
	&p1_ts_clk_div,
	&p1_ts_clk,
	&p1_usb_250m_sel,
	&p1_usb_250m_div,
	&p1_usb_250m,
	&p1_pcie_400m_sel,
	&p1_pcie_400m_div,
	&p1_pcie_400m,
	&p1_pcie_clk_sel,
	&p1_pcie_clk_div,
	&p1_pcie_clk,
	&p1_pcie_tl_sel,
	&p1_pcie_tl_div,
	&p1_pcie_tl,
	&p1_nna0_sel,
	&p1_nna0_div,
	&p1_nna0,
	&p1_nna1_sel,
	&p1_nna1_div,
	&p1_nna1,
	&p1_nna2_sel,
	&p1_nna2_div,
	&p1_nna2,
	&p1_nna3_sel,
	&p1_nna3_div,
	&p1_nna3,
	&p1_nna4_sel,
	&p1_nna4_div,
	&p1_nna4,
	&p1_nna5_sel,
	&p1_nna5_div,
	&p1_nna5,
	&p1_isp0_sel,
	&p1_isp0_div,
	&p1_isp0,
	&p1_isp1_sel,
	&p1_isp1_div,
	&p1_isp1,
	&p1_isp2_sel,
	&p1_isp2_div,
	&p1_isp2,
	&p1_isp3_sel,
	&p1_isp3_div,
	&p1_isp3,
	&p1_isp4_sel,
	&p1_isp4_div,
	&p1_isp4,
	&p1_csiphy0_sel,
	&p1_csiphy0_div,
	&p1_csiphy0,
	&p1_csiphy1_sel,
	&p1_csiphy1_div,
	&p1_csiphy1,
	&p1_csiphy2_sel,
	&p1_csiphy2_div,
	&p1_csiphy2,
	&p1_csiphy3_sel,
	&p1_csiphy3_div,
	&p1_csiphy3,
	&p1_csiphy4_sel,
	&p1_csiphy4_div,
	&p1_csiphy4,
	&p1_mopa_sel,
	&p1_mopa_div,
	&p1_mopa,
	&p1_mopb_sel,
	&p1_mopb_div,
	&p1_mopb,
	&p1_depa_sel,
	&p1_depa_div,
	&p1_depa,
	&p1_depb_sel,
	&p1_depb_div,
	&p1_depb,
	&p1_vfe_sel,
	&p1_vfe_div,
	&p1_vfe,
	&p1_dewarpa_sel,
	&p1_dewarpa_div,
	&p1_dewarpa,
	&p1_dewarpb_sel,
	&p1_dewarpb_div,
	&p1_dewarpb,
	&p1_dewarpc_sel,
	&p1_dewarpc_div,
	&p1_dewarpc,
	&p1_m4_clk_0_sel,
	&p1_m4_clk_0_div,
	&p1_m4_clk_0,
	&p1_m4_clk_1_sel,
	&p1_m4_clk_1_div,
	&p1_m4_clk_1,
	&p1_m4_clk,
	&p1_sys_clk_ddr,
	&p1_sys_clk_ethphy,
	&p1_sys_clk_m4,
	&p1_sys_clk_glb,
	&p1_sys_clk_aocpu,
	&p1_sys_clk_aucpu,
	&p1_sys_clk_dewarpc,
	&p1_sys_clk_dewarpb,
	&p1_sys_clk_dewarpa,
	&p1_sys_clk_ampipe_nand,
	&p1_sys_clk_ampipe_eth,
	&p1_sys_clk_am2axi0,
	&p1_sys_clk_am2axi1,
	&p1_sys_clk_am2axi2,
	&p1_sys_clk_sdemmca,
	&p1_sys_clk_sdemmcc,
	&p1_sys_clk_spifc,
	&p1_sys_clk_msr_clk,
	&p1_sys_clk_audio,
	&p1_sys_clk_eth,
	&p1_sys_clk_uart_a,
	&p1_sys_clk_uart_b,
	&p1_sys_clk_uart_c,
	&p1_sys_clk_uart_d,
	&p1_sys_clk_uart_e,
	&p1_sys_clk_uart_f,
	&p1_sys_clk_aififo,
	&p1_sys_clk_spicc2,
	&p1_sys_clk_spicc3,
	&p1_sys_clk_spicc4,
	&p1_sys_clk_ts_a76,
	&p1_sys_clk_ts_a55,
	&p1_sys_clk_spicc5,
	&p1_sys_clk_ts_ddr_0,
	&p1_sys_clk_ts_ddr_1,
	&p1_sys_clk_g2d,
	&p1_sys_clk_spicc0,
	&p1_sys_clk_spicc1,
	&p1_sys_clk_pcie,
	&p1_sys_clk_pciephy,
	&p1_sys_clk_usb,
	&p1_sys_clk_pcie_phy0,
	&p1_sys_clk_pcie_phy1,
	&p1_sys_clk_pcie_phy2,
	&p1_sys_clk_i2c_m_a,
	&p1_sys_clk_i2c_m_b,
	&p1_sys_clk_i2c_m_c,
	&p1_sys_clk_i2c_m_d,
	&p1_sys_clk_i2c_m_e,
	&p1_sys_clk_i2c_m_f,
	&p1_sys_clk_i2c_m_g,
	&p1_sys_clk_i2c_m_h,
	&p1_sys_clk_i2c_m_i,
	&p1_sys_clk_i2c_m_j,
	&p1_sys_clk_i2c_s_a,
	&p1_sys_clk_i2c_m_k,
	&p1_sys_clk_mmc_pclk,
	&p1_sys_clk_isp_pclk,
	&p1_sys_clk_rsa,
	&p1_sys_clk_pclk_sys_cpu_apb,
	&p1_sys_clk_dspa,
	&p1_sys_clk_dspb,
	&p1_sys_clk_sar_adc,
	&p1_sys_clk_gic,
	&p1_sys_clk_ts_nna,
	&p1_sys_clk_pwm_ab,
	&p1_sys_clk_pwm_cd,
	&p1_sys_clk_pwm_ef,
	&p1_sys_clk_pwm_gh,
	&p1_sys_clk_pwm_ij,
	&p1_sys_clk_depa,
	&p1_sys_clk_depb,
	&p1_sys_clk_mopa,
	&p1_sys_clk_mopb,
	&p1_sys_clk_vfe,
	&p1_sys_clk_i2c_s_b,
	&p1_sys_clk_i2c_s_c,
	&p1_sys_clk_i2c_s_d,
	&p1_sys_clk_i2c_s_e,
	&p1_sys_clk_i2c_s_f,
	&p1_sys_clk_i2c_s_g,
	&p1_sys_clk_i2c_s_h,
	&p1_sys_clk_i2c_s_i,
	&p1_sys_clk_i2c_s_j,
	&p1_sys_clk_i2c_s_k,
	/*plls*/
	&p1_sys_pll_dco,
	&p1_sys_pll,
	&p1_sys1_pll_dco,
	&p1_sys1_pll,
	&p1_fixed_pll_dco,
	&p1_fixed_pll,
	&p1_fclk_div2,
	&p1_fclk_div3,
	&p1_fclk_div4,
	&p1_fclk_div5,
	&p1_fclk_div7,
	&p1_fclk_div2p5,
	&p1_gp0_pll_dco,
	&p1_gp0_pll,
	&p1_gp1_pll_dco,
	&p1_gp1_pll,
	&p1_gp2_pll_dco,
	&p1_gp2_pll,
	&p1_hifi_pll_dco,
	&p1_hifi_pll,
	&p1_fdle_pll_dco,
	&p1_fdle_pll,
	&p1_nna_pll_dco,
	&p1_nna_pll,
	&p1_m4_pll_dco,
	&p1_m4_pll,
	&p1_mpll_50m,
	&p1_mpll0_div,
	&p1_mpll0,
	&p1_mpll1_div,
	&p1_mpll1,
	&p1_mpll2_div,
	&p1_mpll2,
	&p1_mpll3_div,
	&p1_mpll3
};

static struct clk_regmap *const p1_pll_regmaps[] = {
	&p1_pcie_pll_dco,
	&p1_pcie_pll_od,
	&p1_pcie_hcsl,
	&p1_mclk_pll_dco,
	&p1_mclk_0_pre_od,
	&p1_mclk_0_pll,
	&p1_mclk_0_sel,
	&p1_mclk_0_div2,
	&p1_mclk_0_div_en,
	&p1_mclk_0,
	&p1_mclk_1_pre_od,
	&p1_mclk_1_pll,
	&p1_mclk_1_sel,
	&p1_mclk_1_div2,
	&p1_mclk_1_div_en,
	&p1_mclk_1,
	&p1_mclk_2_pre_od,
	&p1_mclk_2_pll,
	&p1_mclk_2_sel,
	&p1_mclk_2_div2,
	&p1_mclk_2_div_en,
	&p1_mclk_2,
	&p1_mclk_3_pre_od,
	&p1_mclk_3_pll,
	&p1_mclk_3_sel,
	&p1_mclk_3_div2,
	&p1_mclk_3_div_en,
	&p1_mclk_3,
	&p1_mclk_4_pre_od,
	&p1_mclk_4_pll,
	&p1_mclk_4_sel,
	&p1_mclk_4_div2,
	&p1_mclk_4_div_en,
	&p1_mclk_4,
	&p1_mclk_0_pll_div_en,
	&p1_mclk_1_pll_div_en,
	&p1_mclk_2_pll_div_en,
	&p1_mclk_3_pll_div_en,
	&p1_mclk_4_pll_div_en,
};

static struct clk_regmap *const p1_cpu_clk_regmaps[] = {
	&p1_cpu_dyn_clk,
	&p1_cpu_clk,
	&p1_a76_dyn_clk,
	&p1_a76_clk,
	&p1_dsu_dyn_clk,
	&p1_dsu_clk,
	&p1_dsu_final_clk,
	&p1_cpu6_clk,
	&p1_cpu7_clk
};

static int meson_p1_dvfs_setup(struct platform_device *pdev)
{
	int ret;

	/* Setup cluster 0 clock notifier for sys_pll */
	ret = clk_notifier_register(p1_sys_pll.hw.clk,
				    &p1_sys_pll_nb_data.nb);
	if (ret) {
		dev_err(&pdev->dev, "failed to register sys_pll notifier\n");
		return ret;
	}

	/* Setup cluster 1 clock notifier for sys1_pll */
	ret = clk_notifier_register(p1_sys1_pll.hw.clk,
				    &p1_sys1_pll_nb_data.nb);
	if (ret) {
		dev_err(&pdev->dev, "failed to register sys1_pll notifier\n");
		return ret;
	}
	/* Setup clock notifier for gp1_pll */
	ret = clk_notifier_register(p1_gp1_pll.hw.clk,
				    &p1_gp1_pll_nb_data.nb);
	if (ret) {
		dev_err(&pdev->dev, "failed to register sys1_pll notifier\n");
		return ret;
	}

	/* keep dsu final at dsu clock */
	clk_set_rate(p1_dsu_clk.hw.clk, 1000000000);
	clk_hw_set_parent(&p1_dsu_final_clk.hw, &p1_dsu_clk.hw);

	return 0;
}

static struct regmap_config clkc_regmap_config = {
	.reg_bits       = 32,
	.val_bits       = 32,
	.reg_stride     = 4,
};

static struct regmap *p1_regmap_resource(struct device *dev, char *name)
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

static int meson_p1_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct regmap *basic_map, *pll_map;
	struct regmap *cpu_clk_map;
	int ret, i;

	/* Get regmap for different clock area */
	basic_map = p1_regmap_resource(dev, "basic");
	if (IS_ERR(basic_map)) {
		dev_err(dev, "basic clk registers not found\n");
		return PTR_ERR(basic_map);
	}

	cpu_clk_map = p1_regmap_resource(dev, "cpu_clk");
	if (IS_ERR(cpu_clk_map)) {
		dev_err(dev, "cpu clk registers not found\n");
		return PTR_ERR(cpu_clk_map);
	}

	pll_map = p1_regmap_resource(dev, "pll");
	if (IS_ERR(basic_map)) {
		dev_err(dev, "pll clk registers not found\n");
		return PTR_ERR(pll_map);
	}

	/* Populate regmap for the regmap backed clocks */
	for (i = 0; i < ARRAY_SIZE(p1_clk_regmaps); i++)
		p1_clk_regmaps[i]->map = basic_map;

	for (i = 0; i < ARRAY_SIZE(p1_cpu_clk_regmaps); i++)
		p1_cpu_clk_regmaps[i]->map = cpu_clk_map;

	for (i = 0; i < ARRAY_SIZE(p1_pll_regmaps); i++)
		p1_pll_regmaps[i]->map = pll_map;

	regmap_write(basic_map, CLKCTRL_MPLL_CTRL0, 0x00000543);

	for (i = 0; i < p1_hw_onecell_data.num; i++) {
		/* array might be sparse */
		if (!p1_hw_onecell_data.hws[i])
			continue;

		ret = devm_clk_hw_register(dev, p1_hw_onecell_data.hws[i]);
		if (ret) {
			dev_err(dev, "Clock registration failed\n");
			return ret;
		}
	}

	meson_p1_dvfs_setup(pdev);

	return devm_of_clk_add_hw_provider(dev, of_clk_hw_onecell_get,
					   &p1_hw_onecell_data);
}

static const struct of_device_id clkc_match_table[] = {
	{
		.compatible = "amlogic,p1-clkc"
	},
	{}
};

static struct platform_driver p1_driver = {
	.probe		= meson_p1_probe,
	.driver		= {
		.name	= "p1-clkc",
		.of_match_table = clkc_match_table,
	},
};

#ifndef CONFIG_AMLOGIC_MODIFY
builtin_platform_driver(p1_driver);
#else
#ifndef MODULE
static int p1_clkc_init(void)
{
	return platform_driver_register(&p1_driver);
}
arch_initcall_sync(p1_clkc_init);
#else
int __init meson_p1_clkc_init(void)
{
	return platform_driver_register(&p1_driver);
}
#endif
#endif

MODULE_LICENSE("GPL v2");
