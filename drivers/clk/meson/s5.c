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

#include "clk-pll.h"
#include "clk-regmap.h"
#include "clk-cpu-dyndiv.h"
#include "vid-pll-div.h"
#include "clk-dualdiv.h"
#include "s5.h"
#include "clkcs_init.h"
#include <dt-bindings/clock/s5-clkc.h>
#include "clk-secure.h"

static const struct clk_ops meson_pll_clk_no_ops = {};

/*
 * the sys pll DCO value should be 3G~6G,
 * otherwise the sys pll can not lock.
 * od is for 32 bit.
 */

#ifdef CONFIG_ARM
static const struct pll_params_table s5_sys_pll_params_table[] = {
	PLL_PARAMS(100, 1, 1), /*DCO=2400M OD=1200M*/
	PLL_PARAMS(116, 1, 1), /*DCO=2784 OD=1392M*/
	PLL_PARAMS(126, 1, 1), /*DCO=3024 OD=1512M*/
	PLL_PARAMS(67, 1, 0), /*DCO=1608M OD=1608MM*/
	PLL_PARAMS(71, 1, 0), /*DCO=1704MM OD=1704M*/
	PLL_PARAMS(75, 1, 0), /*DCO=1800M OD=1800M*/
	PLL_PARAMS(79, 1, 0), /*DCO=1896M OD=1896M*/
	PLL_PARAMS(84, 1, 0), /*DCO=2016M OD=2016M*/
	{ /* sentinel */ }
};
#else
static const struct pll_params_table s5_sys_pll_params_table[] = {
	PLL_PARAMS(100, 1), /*DCO=2400M OD=1200M*/
	PLL_PARAMS(116, 1), /*DCO=2784 OD=1392M*/
	PLL_PARAMS(126, 1), /*DCO=3024 OD=1512M*/
	PLL_PARAMS(67, 1), /*DCO=1608M OD=1608MM*/
	PLL_PARAMS(71, 1), /*DCO=1704MM OD=1704M*/
	PLL_PARAMS(75, 1), /*DCO=1800M OD=1800M*/
	PLL_PARAMS(79, 1), /*DCO=1896M OD=1896M*/
	PLL_PARAMS(84, 1), /*DCO=2016M OD=2016M*/
	{ /* sentinel */ }
};
#endif

static struct clk_regmap s5_sys_pll_dco = {
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
		.table = s5_sys_pll_params_table,
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
		.flags = CLK_GET_RATE_NOCACHE | CLK_IS_CRITICAL,
	},
};

#ifdef CONFIG_ARM
static const struct pll_params_table s5_sys1_pll_params_table[] = {
	PLL_PARAMS(100, 1, 1), /*DCO=2400M OD=1200M*/
	PLL_PARAMS(116, 1, 1), /*DCO=2784 OD=1392M*/
	PLL_PARAMS(126, 1, 1), /*DCO=3024 OD=1512M*/
	PLL_PARAMS(67, 1, 0), /*DCO=1608M OD=1608MM*/
	PLL_PARAMS(71, 1, 0), /*DCO=1704MM OD=1704M*/
	PLL_PARAMS(75, 1, 0), /*DCO=1800M OD=1800M*/
	PLL_PARAMS(79, 1, 0), /*DCO=1896M OD=1896M*/
	PLL_PARAMS(84, 1, 0), /*DCO=2016M OD=2016M*/
	PLL_PARAMS(88, 1, 0), /*DCO=2112M OD=2112M*/
	{ /* sentinel */ }
};
#else
static const struct pll_params_table s5_sys1_pll_params_table[] = {
	PLL_PARAMS(100, 1), /*DCO=2400M OD=1200M*/
	PLL_PARAMS(116, 1), /*DCO=2784 OD=1392M*/
	PLL_PARAMS(126, 1), /*DCO=3024 OD=1512M*/
	PLL_PARAMS(67, 1), /*DCO=1608M OD=1608MM*/
	PLL_PARAMS(71, 1), /*DCO=1704MM OD=1704M*/
	PLL_PARAMS(75, 1), /*DCO=1800M OD=1800M*/
	PLL_PARAMS(79, 1), /*DCO=1896M OD=1896M*/
	PLL_PARAMS(84, 1), /*DCO=2016M OD=2016M*/
	PLL_PARAMS(88, 1), /*DCO=2112M OD=2112M*/
	{ /* sentinel */ }
};
#endif

static struct clk_regmap s5_sys1_pll_dco = {
	.data = &(struct meson_clk_pll_data) {
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
		.table = s5_sys1_pll_params_table,
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
		.flags = CLK_GET_RATE_NOCACHE | CLK_IS_CRITICAL,
	},
};

static const struct reg_sequence s5_sys2pll_init_regs[] = {
	{ .reg = CLKCTRL_SYS2PLL_CTRL0,	.def = 0x20011045 },
	{ .reg = CLKCTRL_SYS2PLL_CTRL0,	.def = 0x30011045 },
	{ .reg = CLKCTRL_SYS2PLL_CTRL1,	.def = 0x3420500f },
	{ .reg = CLKCTRL_SYS2PLL_CTRL2,	.def = 0x00023041 },
	{ .reg = CLKCTRL_SYS2PLL_CTRL3,	.def = 0x0, .delay_us = 20 },
	{ .reg = CLKCTRL_SYS2PLL_CTRL0,	.def = 0x10011045, .delay_us = 20 },
	{ .reg = CLKCTRL_SYS2PLL_CTRL2,	.def = 0x00023001, .delay_us = 50 }
};

#ifdef CONFIG_ARM
static const struct pll_params_table s5_sys2_pll_params_table[] = {
	PLL_PARAMS(69, 1, 1), /*DCO=1656M OD=828M*/
	{ /* sentinel */ }
};
#else
static const struct pll_params_table s5_sys2_pll_params_table[] = {
	PLL_PARAMS(69, 1), /* DCO=1656M */
	{ /* sentinel */ }
};
#endif

static struct clk_regmap s5_sys2_pll_dco = {
	.data = &(struct meson_clk_pll_data) {
		.en = {
			.reg_off = CLKCTRL_SYS2PLL_CTRL0,
			.shift   = 28,
			.width   = 1,
		},
		.m = {
			.reg_off = CLKCTRL_SYS2PLL_CTRL0,
			.shift   = 0,
			.width   = 8,
		},
		.n = {
			.reg_off = CLKCTRL_SYS2PLL_CTRL0,
			.shift   = 16,
			.width   = 5,
		},
#ifdef CONFIG_ARM
		/* od for 32bit */
		.od = {
			.reg_off = CLKCTRL_SYS2PLL_CTRL0,
			.shift	 = 12,
			.width	 = 3,
		},
#endif
		.l = {
			.reg_off = CLKCTRL_SYS2PLL_STS,
			.shift   = 31,
			.width   = 1,
		},
		.rst = {
			.reg_off = CLKCTRL_SYS2PLL_CTRL0,
			.shift   = 29,
			.width   = 1,
		},
		.table = s5_sys2_pll_params_table,
		.init_regs = s5_sys2pll_init_regs,
		.init_count = ARRAY_SIZE(s5_sys2pll_init_regs),
	},
	.hw.init = &(struct clk_init_data){
		.name = "sys2_pll_dco",
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
static struct clk_regmap s5_sys_pll = {
	.hw.init = &(struct clk_init_data){
		.name = "sys_pll",
		.ops = &meson_pll_clk_no_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_sys_pll_dco.hw
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

static struct clk_regmap s5_sys1_pll = {
	.hw.init = &(struct clk_init_data){
		.name = "sys1_pll",
		.ops = &meson_pll_clk_no_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_sys1_pll_dco.hw
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

static struct clk_regmap s5_sys2_pll = {
	.hw.init = &(struct clk_init_data){
		.name = "sys2_pll",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_sys2_pll_dco.hw
		},
		.num_parents = 1,
		/*
		 * sys1 pll may be initialized in the bootloader
		 * CLK_IGNORE_UNUSED is needed to prevent the system
		 * hang up which will be called by clk_disable_unused
		 */
		.flags = CLK_SET_RATE_PARENT,
	},
};
#else
const static struct clk_div_table s5_sys_pll_od_tab[] = {
	{0,		1},  /* reg = 0; div = 1 */
	{1,		2},  /* reg = 1; div = 2 */
	{2,		4},  /* reg = 2; div = 4 */
	{3,		8},  /* reg = 3; div = 8 */
	{4,		16},  /* reg = 4; div = 16 */
	{ /* sentinel */ }
};

static struct clk_regmap s5_sys_pll = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_SYS0PLL_CTRL0,
		.shift = 12,
		.width = 3,
		.table = s5_sys_pll_od_tab,  /* max_div = 16 */
		.flags = CLK_DIVIDER_ALLOW_ZERO,
		.smc_id = SECURE_PLL_CLK,
		.secid = SECID_SYS0_PLL_OD
	},
	.hw.init = &(struct clk_init_data){
		.name = "sys_pll",
		.ops = &clk_regmap_secure_v2_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_sys_pll_dco.hw
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

static struct clk_regmap s5_sys1_pll = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_SYS1PLL_CTRL0,
		.shift = 12,
		.width = 3,
		.table = s5_sys_pll_od_tab,  /* max_div = 16 */
		.flags = CLK_DIVIDER_POWER_OF_TWO | CLK_DIVIDER_ALLOW_ZERO,
		.smc_id = SECURE_PLL_CLK,
		.secid = SECID_SYS1_PLL_OD
	},
	.hw.init = &(struct clk_init_data){
		.name = "sys1_pll",
		.ops = &clk_regmap_secure_v2_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_sys1_pll_dco.hw
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

static struct clk_regmap s5_sys2_pll = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_SYS2PLL_CTRL0,
		.shift = 12,
		.width = 3,
		.table = s5_sys_pll_od_tab,  /* max_div = 16 */
		.flags = CLK_DIVIDER_POWER_OF_TWO | CLK_DIVIDER_ALLOW_ZERO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sys2_pll",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_sys2_pll_dco.hw
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

static struct clk_regmap s5_fixed_pll_dco = {
	.data = &(struct meson_clk_pll_data){
		.en = {
			.reg_off = CLKCTRL_FIXPLL_CTRL0,
			.shift   = 28,
			.width   = 1,
		},
		.m = {
			.reg_off = CLKCTRL_FIXPLL_CTRL0,
			.shift   = 0,
			.width   = 8,
		},
		.n = {
			.reg_off = CLKCTRL_FIXPLL_CTRL0,
			.shift   = 16,
			.width   = 5,
		},
#ifdef CONFIG_ARM
		/* od for 32bit */
		.od = {
			.reg_off = CLKCTRL_FIXPLL_CTRL0,
			.shift	 = 12,
			.width	 = 3,
		},
#endif
		.l = {
			.reg_off = CLKCTRL_FIXPLL_STS,
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

#ifdef CONFIG_ARM
static struct clk_regmap s5_fixed_pll = {
	.hw.init = &(struct clk_init_data){
		.name = "fixed_pll",
		.ops = &meson_pll_clk_no_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_fixed_pll_dco.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};
#else
static struct clk_regmap s5_fixed_pll = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_FIXPLL_CTRL0,
		.shift = 12,
		.width = 3,
		.flags = CLK_DIVIDER_POWER_OF_TWO | CLK_DIVIDER_ALLOW_ZERO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "fixed_pll",
		.ops = &clk_regmap_divider_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_fixed_pll_dco.hw
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
#endif

static struct clk_fixed_factor s5_fclk_div2_div = {
	.mult = 1,
	.div = 2,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div2_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) { &s5_fixed_pll.hw },
		.num_parents = 1,
	},
};

static struct clk_regmap s5_fclk_div2 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_FIXPLL_CTRL3,
		.bit_idx = 20,
	},
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div2",
		.ops = &clk_regmap_gate_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_fclk_div2_div.hw
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

static struct clk_fixed_factor s5_fclk_div3_div = {
	.mult = 1,
	.div = 3,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div3_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) { &s5_fixed_pll.hw },
		.num_parents = 1,
	},
};

static struct clk_regmap s5_fclk_div3 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_FIXPLL_CTRL3,
		.bit_idx = 16,
	},
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div3",
		.ops = &clk_regmap_gate_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_fclk_div3_div.hw
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

static struct clk_fixed_factor s5_fclk_div4_div = {
	.mult = 1,
	.div = 4,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div4_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) { &s5_fixed_pll.hw },
		.num_parents = 1,
	},
};

static struct clk_regmap s5_fclk_div4 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_FIXPLL_CTRL3,
		.bit_idx = 17,
	},
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div4",
		.ops = &clk_regmap_gate_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_fclk_div4_div.hw
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

static struct clk_fixed_factor s5_fclk_div5_div = {
	.mult = 1,
	.div = 5,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div5_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) { &s5_fixed_pll.hw },
		.num_parents = 1,
	},
};

static struct clk_regmap s5_fclk_div5 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_FIXPLL_CTRL3,
		.bit_idx = 18,
	},
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div5",
		.ops = &clk_regmap_gate_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_fclk_div5_div.hw
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

static struct clk_fixed_factor s5_fclk_div7_div = {
	.mult = 1,
	.div = 7,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div7_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) { &s5_fixed_pll.hw },
		.num_parents = 1,
	},
};

static struct clk_regmap s5_fclk_div7 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_FIXPLL_CTRL3,
		.bit_idx = 19,
	},
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div7",
		.ops = &clk_regmap_gate_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_fclk_div7_div.hw
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

static struct clk_fixed_factor s5_fclk_div2p5_div = {
	.mult = 2,
	.div = 5,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div2p5_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_fixed_pll.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap s5_fclk_div2p5 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_FIXPLL_CTRL3,
		.bit_idx = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div2p5",
		.ops = &clk_regmap_gate_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_fclk_div2p5_div.hw
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

static struct clk_fixed_factor s5_fclk_clk50m_div = {
	.mult = 1,
	.div = 40,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_clk50m_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_fixed_pll.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap s5_fclk_clk50m = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_FIXPLL_CTRL3,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "fclk_clk50m",
		.ops = &clk_regmap_gate_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_fclk_clk50m_div.hw
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
static const struct pll_params_table s5_gp0_pll_table[] = {
	PLL_PARAMS(70, 1, 1), /* DCO = 1680M OD = 2 PLL = 840M */
	PLL_PARAMS(132, 1, 2), /* DCO = 3168M OD = 4 PLL = 792M */
	PLL_PARAMS(128, 1, 2), /* DCO = 3072M OD = 4 PLL = 768M */
	PLL_PARAMS(124, 1, 2), /* DCO = 2976M OD = 4 PLL = 744M */
	PLL_PARAMS(96, 1, 1), /* DCO = 2304M OD = 2 PLL = 1152M */
	{ /* sentinel */  }
};
#else
static const struct pll_params_table s5_gp0_pll_table[] = {
	PLL_PARAMS(70, 1), /* DCO = 1680M OD = 2 PLL = 846M */
	PLL_PARAMS(132, 1), /* DCO = 3168M OD = 2 PLL = 792M */
	PLL_PARAMS(128, 1), /* DCO = 3072M OD = 2 PLL = 768M */
	PLL_PARAMS(124, 1), /* DCO = 2976M OD = 3 PLL = 744M */
	PLL_PARAMS(96, 1), /* DCO = 2304M OD = 4 PLL = 1152M */
	{ /* sentinel */  }
};
#endif

/*
 * Internal gp0 pll emulation configuration parameters
 */
static const struct reg_sequence s5_gp0_init_regs[] = {
	{ .reg = CLKCTRL_GP0PLL_CTRL0,	.def = 0x20011085 },
	{ .reg = CLKCTRL_GP0PLL_CTRL0,	.def = 0x30011085 },
	{ .reg = CLKCTRL_GP0PLL_CTRL1,	.def = 0x03a10000 },
	{ .reg = CLKCTRL_GP0PLL_CTRL2,	.def = 0x00040000 },
	{ .reg = CLKCTRL_GP0PLL_CTRL3,	.def = 0x0b0da000, .delay_us = 20 },
	{ .reg = CLKCTRL_GP0PLL_CTRL0,	.def = 0x10011085, .delay_us = 20 },
	{ .reg = CLKCTRL_GP0PLL_CTRL3,	.def = 0x0b0da200, .delay_us = 50 }
};

static struct clk_regmap s5_gp0_pll_dco = {
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
			.shift   = 16,
			.width   = 5,
		},
#ifdef CONFIG_ARM
		/* for 32bit */
		.od = {
			.reg_off = CLKCTRL_GP0PLL_CTRL0,
			.shift	 = 10,
			.width	 = 3,
		},
#endif
		.frac = {
			.reg_off = CLKCTRL_GP0PLL_CTRL1,
			.shift   = 0,
			.width   = 19,
		},
		.l = {
			.reg_off = CLKCTRL_GP0PLL_STS,
			.shift   = 31,
			.width   = 1,
		},
		.rst = {
			.reg_off = CLKCTRL_GP0PLL_CTRL0,
			.shift   = 29,
			.width   = 1,
		},
		.table = s5_gp0_pll_table,
		//.init_regs = s5_gp0_init_regs,
		//.init_count = ARRAY_SIZE(s5_gp0_init_regs),
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
static struct clk_regmap s5_gp0_pll = {
	.hw.init = &(struct clk_init_data){
		.name = "gp0_pll",
		.ops = &meson_pll_clk_no_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_gp0_pll_dco.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};
#else
const static struct clk_div_table s5_gp0_pll_od_tab[] = {
	{0,		1},  /* reg = 0; div = 1 */
	{1,		2},  /* reg = 1; div = 2 */
	{2,		4},  /* reg = 2; div = 4 */
	{3,		8},  /* reg = 3; div = 8 */
	{4,		16},  /* reg = 4; div = 16 */
	{ /* sentinel */ }
};

static struct clk_regmap s5_gp0_pll = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_GP0PLL_CTRL0,
		.shift = 10,
		.width = 3,
		.table = s5_gp0_pll_od_tab,
		.flags = (CLK_DIVIDER_ALLOW_ZERO | CLK_DIVIDER_ROUND_CLOSEST),
	},
	.hw.init = &(struct clk_init_data){
		.name = "gp0_pll",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_gp0_pll_dco.hw
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
static const struct pll_params_table s5_gp1_pll_table[] = {
	PLL_PARAMS(100, 1, 1), /*DCO=2400M OD=1200M*/
	PLL_PARAMS(116, 1, 1), /*DCO=2784 OD=1392M*/
	PLL_PARAMS(125, 1, 1), /*DCO=3000 OD=1500M*/
	PLL_PARAMS(67, 1, 0), /*DCO=1608M OD=1608MM*/
	PLL_PARAMS(71, 1, 0), /*DCO=1704MM OD=1704M*/
	PLL_PARAMS(75, 1, 0), /*DCO=1800M OD=1800M*/
	PLL_PARAMS(79, 1, 0), /*DCO=1896M OD=1896M*/
	PLL_PARAMS(81, 1, 0), /*DCO=1944M OD=1944M*/
	{ /* sentinel */  }
};
#else
static const struct pll_params_table s5_gp1_pll_table[] = {
	PLL_PARAMS(100, 1), /*DCO=2400M OD=1200M*/
	PLL_PARAMS(116, 1), /*DCO=2784 OD=1392M*/
	PLL_PARAMS(125, 1), /*DCO=3000 OD=1500M*/
	PLL_PARAMS(67, 1), /*DCO=1608M OD=1608MM*/
	PLL_PARAMS(71, 1), /*DCO=1704MM OD=1704M*/
	PLL_PARAMS(75, 1), /*DCO=1800M OD=1800M*/
	PLL_PARAMS(79, 1), /*DCO=1896M OD=1896M*/
	PLL_PARAMS(81, 1), /*DCO=1944M OD=1944M*/
	{ /* sentinel */  }
};
#endif

/* TODO: need check */
static struct clk_regmap s5_gp1_pll_dco = {
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
#ifdef CONFIG_ARM
		/* od for 32bit */
		.od = {
			.reg_off = CLKCTRL_GP1PLL_CTRL0,
			.shift	 = 12,
			.width	 = 3,
		},
#endif
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
		.table = s5_gp1_pll_table,
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
		.flags = CLK_GET_RATE_NOCACHE | CLK_IS_CRITICAL,
	},
};

#ifdef CONFIG_ARM
static struct clk_regmap s5_gp1_pll = {
	.hw.init = &(struct clk_init_data){
		.name = "gp1_pll",
		.ops = &meson_pll_clk_no_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_gp1_pll_dco.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};
#else
const static struct clk_div_table s5_gp1_pll_od_tab[] = {
	{0,		1},  /* reg = 0; div = 1 */
	{1,		2},  /* reg = 1; div = 2 */
	{2,		4},  /* reg = 2; div = 4 */
	{3,		8},  /* reg = 3; div = 8 */
	{4,		16},  /* reg = 4; div = 16 */
	{ /* sentinel */ }
};

static struct clk_regmap s5_gp1_pll = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_GP1PLL_CTRL0,
		.shift = 12,
		.width = 3,
		.table = s5_gp1_pll_od_tab,
		.flags = CLK_DIVIDER_ALLOW_ZERO,
		.smc_id = SECURE_PLL_CLK,
		.secid = SECID_GP1_PLL_OD
	},
	.hw.init = &(struct clk_init_data){
		.name = "gp1_pll",
		.ops = &clk_regmap_secure_v2_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_gp1_pll_dco.hw
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
#endif

#ifdef CONFIG_ARM
static const struct pll_params_table s5_gp2_pll_table[] = {
	PLL_PARAMS(70, 1, 1), /* DCO = 1680M OD = 2 PLL = 840M */
	PLL_PARAMS(132, 1, 2), /* DCO = 3168M OD = 4 PLL = 792M */
	PLL_PARAMS(128, 1, 2), /* DCO = 3072M OD = 4 PLL = 768M */
	PLL_PARAMS(124, 1, 2), /* DCO = 2976M OD = 4 PLL = 744M */
	PLL_PARAMS(96, 1, 1), /* DCO = 2304M OD = 2 PLL = 1152M */
	{ /* sentinel */  }
};
#else
static const struct pll_params_table s5_gp2_pll_table[] = {
	PLL_PARAMS(70, 1), /* DCO = 1680M OD = 2 PLL = 846M */
	PLL_PARAMS(132, 1), /* DCO = 3168M OD = 2 PLL = 792M */
	PLL_PARAMS(128, 1), /* DCO = 3072M OD = 2 PLL = 768M */
	PLL_PARAMS(124, 1), /* DCO = 2976M OD = 3 PLL = 744M */
	PLL_PARAMS(96, 1), /* DCO = 2304M OD = 4 PLL = 1152M */
	{ /* sentinel */  }
};
#endif

/*
 * Internal gp2 pll emulation configuration parameters
 */
static const struct reg_sequence s5_gp2_init_regs[] = {
	{ .reg = CLKCTRL_GP2PLL_CTRL0,	.def = 0x20011085 },
	{ .reg = CLKCTRL_GP2PLL_CTRL0,	.def = 0x30011085 },
	{ .reg = CLKCTRL_GP2PLL_CTRL1,	.def = 0x03a10000 },
	{ .reg = CLKCTRL_GP2PLL_CTRL2,	.def = 0x00040000 },
	{ .reg = CLKCTRL_GP2PLL_CTRL3,	.def = 0x0b0da000, .delay_us = 20 },
	{ .reg = CLKCTRL_GP2PLL_CTRL0,	.def = 0x10011085, .delay_us = 20 },
	{ .reg = CLKCTRL_GP2PLL_CTRL3,	.def = 0x0b0da200, .delay_us = 20 }
};

static struct clk_regmap s5_gp2_pll_dco = {
	.data = &(struct meson_clk_pll_data){
		.en = {
			.reg_off = CLKCTRL_GP2PLL_CTRL0,
			.shift   = 28,
			.width   = 1,
		},
		.m = {
			.reg_off = CLKCTRL_GP2PLL_CTRL0,
			.shift   = 0,
			.width   = 9,
		},
		.n = {
			.reg_off = CLKCTRL_GP2PLL_CTRL0,
			.shift   = 16,
			.width   = 5,
		},
#ifdef CONFIG_ARM
		/* for 32bit */
		.od = {
			.reg_off = CLKCTRL_GP2PLL_CTRL0,
			.shift	= 10,
			.width	= 3,
		},
#endif
		.frac = {
			.reg_off = CLKCTRL_GP2PLL_CTRL1,
			.shift   = 0,
			.width   = 19,
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
		.table = s5_gp2_pll_table,
		.init_regs = s5_gp2_init_regs,
		.init_count = ARRAY_SIZE(s5_gp2_init_regs),
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

#ifdef CONFIG_ARM
static struct clk_regmap s5_gp2_pll = {
	.hw.init = &(struct clk_init_data){
		.name = "gp2_pll",
		.ops = &meson_pll_clk_no_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_gp2_pll_dco.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};
#else
const static struct clk_div_table s5_gp2_pll_od_tab[] = {
	{0,		1},  /* reg = 0; div = 1 */
	{1,		2},  /* reg = 1; div = 2 */
	{2,		4},  /* reg = 2; div = 4 */
	{3,		8},  /* reg = 3; div = 8 */
	{4,		16},  /* reg = 4; div = 16 */
	{ /* sentinel */ }
};

static struct clk_regmap s5_gp2_pll = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_GP2PLL_CTRL0,
		.shift = 10,
		.width = 3,
		.table = s5_gp2_pll_od_tab,
		.flags = (CLK_DIVIDER_ALLOW_ZERO | CLK_DIVIDER_ROUND_CLOSEST),
	},
	.hw.init = &(struct clk_init_data){
		.name = "gp2_pll",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_gp2_pll_dco.hw
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
static const struct pll_params_table s5_fpll_table[] = {
	PLL_PARAMS(100, 1, 1, /*DCO=2400M OD=1200M*/
	PLL_PARAMS(125, 1, 1), /*DCO=3000M OD=1500M*/
	{ /* sentinel */  }
};
#else
static const struct pll_params_table s5_fpll_table[] = {
	PLL_PARAMS(100, 1), /*DCO=2400M OD=1200M*/
	PLL_PARAMS(125, 1), /*DCO=3000M OD=1500M*/
	{ /* sentinel */  }
};
#endif

static const struct reg_sequence s5_fpll_init_regs[] = {
	{ .reg = CLKCTRL_FPLL_CTRL0, .def = 0x20a12085 },
	{ .reg = CLKCTRL_FPLL_CTRL0, .def = 0x30a12085 },
	{ .reg = CLKCTRL_FPLL_CTRL1, .def = 0x03a10000 },
	{ .reg = CLKCTRL_FPLL_CTRL2, .def = 0x00040000 },
	{ .reg = CLKCTRL_FPLL_CTRL3, .def = 0x0b0da000, .delay_us = 20 },
	{ .reg = CLKCTRL_FPLL_CTRL0, .def = 0x10a12085, .delay_us = 20 },
	{ .reg = CLKCTRL_FPLL_CTRL3, .def = 0x0b0da200, .delay_us = 50 }
};

static struct clk_regmap s5_fpll_dco = {
	.data = &(struct meson_clk_pll_data){
		.en = {
			.reg_off = CLKCTRL_FPLL_CTRL0,
			.shift   = 28,
			.width   = 1,
		},
		.m = {
			.reg_off = CLKCTRL_FPLL_CTRL0,
			.shift   = 0,
			.width   = 9,
		},
		.frac = {
			.reg_off = CLKCTRL_FPLL_CTRL1,
			.shift   = 0,
			.width   = 19,
		},
		.n = {
			.reg_off = CLKCTRL_FPLL_CTRL0,
			.shift   = 16,
			.width   = 5,
		},
#ifdef CONFIG_ARM
		/* od for 32bit */
		.od = {
			.reg_off = CLKCTRL_FPLL_CTRL0,
			.shift	 = 21,
			.width	 = 2,
		},
#endif
		.l = {
			.reg_off = CLKCTRL_FPLL_STS,
			.shift   = 31,
			.width   = 1,
		},
		.rst = {
			.reg_off = CLKCTRL_FPLL_CTRL0,
			.shift   = 29,
			.width   = 1,
		},
		.table = s5_fpll_table,
		.init_regs = s5_fpll_init_regs,
		.init_count = ARRAY_SIZE(s5_fpll_init_regs),
	},
	.hw.init = &(struct clk_init_data){
		.name = "fpll_dco",
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
static struct clk_regmap s5_fpll_tmds_od = {
	.hw.init = &(struct clk_init_data){
		.name = "fpll_tmds_od",
		.ops = &meson_pll_clk_no_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_fpll_dco.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};
#else
static struct clk_regmap s5_fpll_tmds_od = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_FPLL_CTRL0,
		.shift = 21,
		.width = 2,
		.flags = (CLK_DIVIDER_POWER_OF_TWO | CLK_DIVIDER_ALLOW_ZERO |
			  CLK_DIVIDER_ROUND_CLOSEST),
	},
	.hw.init = &(struct clk_init_data){
		.name = "fpll_tmds_od",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_fpll_dco.hw
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
#endif

static struct clk_regmap s5_fpll_tmds_od1 = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_FPLL_CTRL0,
		.shift = 23,
		.width = 2,
		.flags = (CLK_DIVIDER_POWER_OF_TWO | CLK_DIVIDER_ALLOW_ZERO |
			  CLK_DIVIDER_ROUND_CLOSEST),
	},
	.hw.init = &(struct clk_init_data){
		.name = "fpll_tmds_od1",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_fpll_tmds_od.hw
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

static struct clk_fixed_factor s5_fpll_tmds = {
	.mult = 1,
	.div = 2,
	.hw.init = &(struct clk_init_data){
		.name = "fpll_tmds",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) { &s5_fpll_tmds_od1.hw },
		.num_parents = 1,
	},
};

#define CLK_MULTDIV_ALLOW_ZERO		BIT(0)
#define CLK_MULTDIV_ROUND_CLOSEST	BIT(1)

struct clk_mult_div_table {
	unsigned int	val;
	unsigned int	mult;
	unsigned int	div;
};

struct clk_regmap_mult_div_data {
	unsigned int	offset;
	u8		shift;
	u8		width;
	u8		flags;
	const struct clk_mult_div_table	*table;
};

static inline struct clk_regmap_mult_div_data *
clk_get_regmap_mult_div_data(struct clk_regmap *clk)
{
	return (struct clk_regmap_mult_div_data *)clk->data;
}

static unsigned long clk_regmap_mult_div_recalc_rate(struct clk_hw *hw,
						unsigned long prate)
{
	struct clk_regmap *clk = to_clk_regmap(hw);
	struct clk_regmap_mult_div_data *mult_div =
		clk_get_regmap_mult_div_data(clk);
	const struct clk_mult_div_table *table;
	unsigned int val;
	unsigned long rate = 0;
	int ret;

	ret = regmap_read(clk->map, mult_div->offset, &val);
	if (ret)
		/* Gives a hint that something is wrong */
		return 0;

	val >>= mult_div->shift;
	val &= clk_div_mask(mult_div->width);
	if (mult_div->table) {  /* now without defined tables are not supported */
		for (table = mult_div->table; table->div; table++) {
			if (val == table->val) {
				rate = prate * table->mult / table->div;
				break;
			}
		}
	}

	return rate;
}

static bool mult_div_is_best_rate(unsigned long rate, unsigned long new,
			   unsigned long best, unsigned long flags)
{
	if (flags & CLK_MULTDIV_ROUND_CLOSEST)
		return abs(rate - new) < abs(rate - best);

	return new >= rate && new < best;
}

static long clk_regmap_mult_div_round_rate(struct clk_hw *hw,
				unsigned long rate, unsigned long *prate)
{
	struct clk_regmap *clk = to_clk_regmap(hw);
	struct clk_regmap_mult_div_data *mult_div =
		clk_get_regmap_mult_div_data(clk);
	const struct clk_mult_div_table *table;
	unsigned long current_rate, best_rate = ~0;

	if (mult_div->table) {  /* now without defined tables are not supported */
		for (table = mult_div->table; table->div; table++) {
			current_rate = *prate * table->mult / table->div;
			if (mult_div_is_best_rate(rate, current_rate, best_rate,
				mult_div->flags))
				best_rate = current_rate;
		}
	}

	return (long)best_rate;
}

#define clk_mult_div_mask(width)	((1 << (width)) - 1)
static int clk_regmap_mult_div_set_rate(struct clk_hw *hw,
				unsigned long rate, unsigned long parent_rate)
{
	struct clk_regmap *clk = to_clk_regmap(hw);
	struct clk_regmap_mult_div_data *mult_div =
		clk_get_regmap_mult_div_data(clk);
	const struct clk_mult_div_table *table = NULL;
	unsigned long current_rate, best_rate = ~0;
	unsigned int val, best_val = 0;

	if (mult_div->table) {  /* now without defined tables are not supported */
		for (table = mult_div->table; table->div; table++) {
			current_rate = parent_rate * table->mult / table->div;
			if (mult_div_is_best_rate(rate, current_rate, best_rate,
				mult_div->flags)) {
				best_rate = current_rate;
				best_val = table->val;
			}
		}
	}

	if (best_rate == ~0)
		return -EINVAL;

	val = (unsigned int)best_val << mult_div->shift;
	return regmap_update_bits(clk->map, mult_div->offset,
			clk_div_mask(mult_div->width) << mult_div->shift, val);
};

const static struct clk_ops clk_regmap_mult_div_ops = {
	.recalc_rate = clk_regmap_mult_div_recalc_rate,
	.round_rate = clk_regmap_mult_div_round_rate,
	.set_rate = clk_regmap_mult_div_set_rate,
};

const static struct clk_mult_div_table s5_pixel_mult_div_table[] = {
	{0,	4,	4},  /* val = 0; fact = 4 / 4 = 1 */
	{1,	4,	5},  /* val = 1; fact = 4 / 5 = 0.8 */
	{2,	4,	6},  /* val = 2; fact = 4 / 6 = 0.67 */
	{3,	4,	7},  /* val = 3; fact = 4 / 7 = 0.57 */
	{4,	4,	8},  /* val = 4; fact = 4 / 8 = 0.5 */
	{ /* sentinel */ }
};

static struct clk_regmap s5_fpll_pixel = {
	.data = &(struct clk_regmap_mult_div_data){
		.offset = CLKCTRL_FPLL_CTRL0,
		.shift = 13,
		.width = 3,
		.table = s5_pixel_mult_div_table,
		.flags = (CLK_MULTDIV_ALLOW_ZERO | CLK_MULTDIV_ROUND_CLOSEST),
	},
	.hw.init = &(struct clk_init_data){
		.name = "fpll_pixel",
		.ops = &clk_regmap_mult_div_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_fpll_tmds.hw
		},
		.num_parents = 1,
		/*
		 * fdle pll is directly used in other modules, and the
		 * parent rate needs to be modified
		 * Register has the risk of being directly operated.
		 */
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

#ifdef CONFIG_ARM
static const struct pll_params_table s5_hifi1_pll_table[] = {
	PLL_PARAMS(67, 1, 4), /* DCO=1608M OD=100.5M */
	PLL_PARAMS(81, 1, 2), /* DCO=1944M OD=486M */
	{ /* sentinel */  }
};
#else
static const struct pll_params_table s5_hifi1_pll_table[] = {
	PLL_PARAMS(67, 1), /* DCO=1608M */
	PLL_PARAMS(81, 1), /* DCO=1944M */
	{ /* sentinel */  }
};
#endif

static const struct reg_sequence s5_hifi1_init_regs[] = {
	{ .reg = CLKCTRL_HIFI1PLL_CTRL0, .def = 0x20011085 },
	{ .reg = CLKCTRL_HIFI1PLL_CTRL0, .def = 0x30011085 },
	{ .reg = CLKCTRL_HIFI1PLL_CTRL1, .def = 0x03a10000 },
	{ .reg = CLKCTRL_HIFI1PLL_CTRL2, .def = 0x00040000 },
	{ .reg = CLKCTRL_HIFI1PLL_CTRL3, .def = 0x0b0da000, .delay_us = 20 },
	{ .reg = CLKCTRL_HIFI1PLL_CTRL0, .def = 0x10011085, .delay_us = 20 },
	{ .reg = CLKCTRL_HIFI1PLL_CTRL3, .def = 0x0b0da200, .delay_us = 50 }
};

static const struct pll_mult_range s5_hifi1_pll_m = {
	.min = 67,
	.max = 133,
};

static struct clk_regmap s5_hifi1_pll_dco = {
	.data = &(struct meson_clk_pll_data){
		.en = {
			.reg_off = CLKCTRL_HIFI1PLL_CTRL0,
			.shift   = 28,
			.width   = 1,
		},
		.m = {
			.reg_off = CLKCTRL_HIFI1PLL_CTRL0,
			.shift   = 0,
			.width   = 9,
		},
		.n = {
			.reg_off = CLKCTRL_HIFI1PLL_CTRL0,
			.shift   = 16,
			.width   = 1,
		},
#ifdef CONFIG_ARM
		/* od for 32bit */
		.od = {
			.reg_off = CLKCTRL_HIFI1PLL_CTRL0,
			.shift	 = 10,
			.width	 = 3,
		},
#endif
		.frac = {
			.reg_off = CLKCTRL_HIFI1PLL_CTRL1,
			.shift   = 0,
			.width   = 19,
		},
		.l = {
			.reg_off = CLKCTRL_HIFI1PLL_STS,
			.shift   = 31,
			.width   = 1,
		},
		.rst = {
			.reg_off = CLKCTRL_HIFI1PLL_CTRL0,
			.shift   = 29,
			.width   = 1,
		},
		.table = s5_hifi1_pll_table,
		.range = &s5_hifi1_pll_m,
		.init_regs = s5_hifi1_init_regs,
		.init_count = ARRAY_SIZE(s5_hifi1_init_regs),
	},
	.hw.init = &(struct clk_init_data){
		.name = "hifi1_pll_dco",
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
static struct clk_regmap s5_hifi1_pll = {
	.hw.init = &(struct clk_init_data){
		.name = "hifi1_pll",
		.ops = &meson_pll_clk_no_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_hifi1_pll_dco.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};
#else
const static struct clk_div_table s5_hifi1_pll_od_table[] = {
	{0,		1},  /* reg = 0; div = 1 */
	{1,		2},  /* reg = 1; div = 2 */
	{2,		4},  /* reg = 2; div = 4 */
	{3,		8},  /* reg = 3; div = 8 */
	{4,		16},  /* reg = 4; div = 16 */
	{ /* sentinel */ }
};

static struct clk_regmap s5_hifi1_pll = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_HIFI1PLL_CTRL0,
		.shift = 10,
		.width = 3,
		.table = s5_hifi1_pll_od_table,
		.flags = (CLK_DIVIDER_ALLOW_ZERO | CLK_DIVIDER_ROUND_CLOSEST),
	},
	.hw.init = &(struct clk_init_data){
		.name = "hifi1_pll",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_hifi1_pll_dco.hw
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
#endif

static const struct pll_mult_range s5_nna_pll_m = {
	.min = 67,
	.max = 133,
};

static const struct reg_sequence s5_nna_pll_init_regs[] = {
	{ .reg = CLKCTRL_NNAPLL_CTRL0, .def = 0x20010885 },
	{ .reg = CLKCTRL_NNAPLL_CTRL0, .def = 0x30010885 },
	{ .reg = CLKCTRL_NNAPLL_CTRL1, .def = 0x03a0aaaa },
	{ .reg = CLKCTRL_NNAPLL_CTRL2, .def = 0x00040000 },
	{ .reg = CLKCTRL_NNAPLL_CTRL3, .def = 0x0b0da000, .delay_us = 20 },
	{ .reg = CLKCTRL_NNAPLL_CTRL0, .def = 0x10010885, .delay_us = 20 },
	{ .reg = CLKCTRL_NNAPLL_CTRL3, .def = 0x0b0da200, .delay_us = 50 }
};

static struct clk_regmap s5_nna_pll_dco = {
	.data = &(struct meson_clk_pll_data){
		.en = {
			.reg_off = CLKCTRL_NNAPLL_CTRL0,
			.shift   = 28,
			.width   = 1,
		},
		.m = {
			.reg_off = CLKCTRL_NNAPLL_CTRL0,
			.shift   = 0,
			.width   = 9,
		},
#ifdef CONFIG_ARM
		/* od for 32bit */
		.od = {
			.reg_off = CLKCTRL_NNAPLL_CTRL0,
			.shift	 = 10,
			.width	 = 3,
		},
#endif
		.frac = {
			.reg_off = CLKCTRL_NNAPLL_CTRL1,
			.shift   = 0,
			.width   = 19,
		},
		.n = {
			.reg_off = CLKCTRL_NNAPLL_CTRL0,
			.shift   = 16,
			.width   = 1,
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
		.range = &s5_nna_pll_m,
		.init_regs = s5_nna_pll_init_regs,
		.init_count = ARRAY_SIZE(s5_nna_pll_init_regs),
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

#ifdef CONFIG_ARM
static struct clk_regmap s5_nna_pll = {
	.hw.init = &(struct clk_init_data){
		.name = "nna_pll",
		.ops = &meson_pll_clk_no_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_nna_pll_dco.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};
#else
const static struct clk_div_table s5_nna_pll_od_table[] = {
	{0,		1},  /* reg = 0; div = 1 */
	{1,		2},  /* reg = 1; div = 2 */
	{2,		4},  /* reg = 2; div = 4 */
	{3,		8},  /* reg = 3; div = 8 */
	{4,		16},  /* reg = 4; div = 16 */
	{ /* sentinel */ }
};

static struct clk_regmap s5_nna_pll = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_NNAPLL_CTRL0,
		.shift = 10,
		.width = 3,
		.table = s5_nna_pll_od_table,
		.flags = (CLK_DIVIDER_ALLOW_ZERO | CLK_DIVIDER_ROUND_CLOSEST),
	},
	.hw.init = &(struct clk_init_data){
		.name = "nna_pll",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_nna_pll_dco.hw
		},
		.num_parents = 1,
		/*
		 * nna pll is directly used in other modules, and the
		 * parent rate needs to be modified
		 * Register has the risk of being directly operated.
		 */
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};
#endif

static struct clk_regmap s5_nna_pll_audio = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_NNAPLL_CTRL0,
		.shift = 14,
		.width = 2,
		.flags = (CLK_DIVIDER_POWER_OF_TWO | CLK_DIVIDER_ALLOW_ZERO |
			  CLK_DIVIDER_ROUND_CLOSEST),
	},
	.hw.init = &(struct clk_init_data){
		.name = "nna_pll_audio",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_nna_pll_dco.hw
		},
		.num_parents = 1,
		/*
		 * nna pll is directly used in other modules, and the
		 * parent rate needs to be modified
		 * Register has the risk of being directly operated.
		 */
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

/* a55 cpu_clk, get the table from ucode */
static const struct cpu_dyn_table s5_cpu_dyn_table[] = {
	CPU_LOW_PARAMS(24000000, 0, 0, 0),
	CPU_LOW_PARAMS(100000000, 1, 1, 9),
	CPU_LOW_PARAMS(250000000, 1, 1, 3),
	CPU_LOW_PARAMS(333333333, 2, 1, 1),
	CPU_LOW_PARAMS(500000000, 1, 1, 1),
	CPU_LOW_PARAMS(666666666, 2, 0, 0),
	CPU_LOW_PARAMS(1000000000, 1, 0, 0),
};

static const struct clk_parent_data s5_dyn_clk_sel[] __initconst = {
	{ .fw_name = "xtal", },
	{ .hw = &s5_fclk_div2.hw },
	{ .hw = &s5_fclk_div3.hw },
	{ .hw = &s5_fclk_div2p5.hw },
};

static struct clk_regmap s5_cpu_dyn_clk = {
	.data = &(struct meson_sec_cpu_dyn_data){
		.table = s5_cpu_dyn_table,
		.table_cnt = ARRAY_SIZE(s5_cpu_dyn_table),
		.secid_dyn_rd = SECID_CPU_CLK_RD,
		.secid_dyn = SECID_CPU_CLK_DYN,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cpu_dyn_clk",
		.ops = &meson_sec_cpu_dyn_ops,
		.parent_data = s5_dyn_clk_sel,
		.num_parents = 4,
	},
};

static struct clk_regmap s5_cpu_clk = {
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
			&s5_cpu_dyn_clk.hw,
			&s5_sys_pll.hw,
		},
		.num_parents = 2,
		/*
		 * This clock feeds the CPU, avoid disabling it
		 * Register has the risk of being directly operated
		 */
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s5_a76_dyn_clk = {
	.data = &(struct meson_sec_cpu_dyn_data){
		.table = s5_cpu_dyn_table,
		.table_cnt = ARRAY_SIZE(s5_cpu_dyn_table),
		.secid_dyn_rd = SECID_A76_CLK_RD,
		.secid_dyn = SECID_A76_CLK_DYN,
	},
	.hw.init = &(struct clk_init_data){
		.name = "a76_dyn_clk",
		.ops = &meson_sec_cpu_dyn_ops,
		.parent_data = s5_dyn_clk_sel,
		.num_parents = 4,
	},
};

static struct clk_regmap s5_a76_clk = {
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
			&s5_a76_dyn_clk.hw,
			&s5_sys1_pll.hw,
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s5_dsu_dyn_clk = {
	.data = &(struct meson_sec_cpu_dyn_data){
		.table = s5_cpu_dyn_table,
		.table_cnt = ARRAY_SIZE(s5_cpu_dyn_table),
		.secid_dyn_rd = SECID_DSU_CLK_RD,
		.secid_dyn = SECID_DSU_CLK_DYN,
	},
	.hw.init = &(struct clk_init_data){
		.name = "dsu_dyn_clk",
		.ops = &meson_sec_cpu_dyn_ops,
		.parent_data = s5_dyn_clk_sel,
		.num_parents = 4,
	},
};

static struct clk_regmap s5_dsu_clk = {
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
			&s5_dsu_dyn_clk.hw,
			&s5_gp1_pll.hw,
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s5_dsu_final_clk = {
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
			&s5_cpu_clk.hw,
			&s5_dsu_clk.hw,
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s5_cpu4_clk = {
	.data = &(struct clk_regmap_mux_data){
		.mask = 0x1,
		.shift = 27,
		.smc_id = SECURE_CPU_CLK,
		.secid = SECID_CPU4_CLK_SEL,
		.secid_rd = SECID_CPU4_CLK_RD,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cpu4_clk",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_cpu_clk.hw,
			&s5_a76_clk.hw,
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

struct s5_sys_pll_nb_data {
	struct notifier_block nb;
	struct clk_hw *sys_pll;
	struct clk_hw *cpu_clk;
	struct clk_hw *cpu_dyn_clk;
};

static int s5_sys_pll_notifier_cb(struct notifier_block *nb,
				   unsigned long event, void *data)
{
	struct s5_sys_pll_nb_data *nb_data =
		container_of(nb, struct s5_sys_pll_nb_data, nb);

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

static struct s5_sys_pll_nb_data s5_sys_pll_nb_data = {
	.sys_pll = &s5_sys_pll.hw,
	.cpu_clk = &s5_cpu_clk.hw,
	.cpu_dyn_clk = &s5_cpu_dyn_clk.hw,
	.nb.notifier_call = s5_sys_pll_notifier_cb,
};

static struct s5_sys_pll_nb_data s5_sys1_pll_nb_data = {
	.sys_pll = &s5_sys1_pll.hw,
	.cpu_clk = &s5_a76_clk.hw,
	.cpu_dyn_clk = &s5_a76_dyn_clk.hw,
	.nb.notifier_call = s5_sys_pll_notifier_cb,
};

static struct s5_sys_pll_nb_data s5_gp1_pll_nb_data = {
	.sys_pll = &s5_gp1_pll.hw,
	.cpu_clk = &s5_dsu_clk.hw,
	.cpu_dyn_clk = &s5_dsu_dyn_clk.hw,
	.nb.notifier_call = s5_sys_pll_notifier_cb,
};

static const struct reg_sequence s5_hifi_init_regs[] = {
	{ .reg = CLKCTRL_HIFIPLL_CTRL0,	.def = 0x20011085 },
	{ .reg = CLKCTRL_HIFIPLL_CTRL0,	.def = 0x30011085 },
	{ .reg = CLKCTRL_HIFIPLL_CTRL1,	.def = 0x03a10000 },
	{ .reg = CLKCTRL_HIFIPLL_CTRL2,	.def = 0x00040000 },
	{ .reg = CLKCTRL_HIFIPLL_CTRL3,	.def = 0x0b0da000, .delay_us = 20 },
	{ .reg = CLKCTRL_HIFIPLL_CTRL0,	.def = 0x10011085, .delay_us = 20 },
	{ .reg = CLKCTRL_HIFIPLL_CTRL3,	.def = 0x0b0da200, .delay_us = 50 }
};

#ifdef CONFIG_ARM
static const struct pll_params_table s5_hifi_pll_table[] = {
	PLL_PARAMS(67, 1, 4), /* DCO=1608M OD=100.5M */
	PLL_PARAMS(81, 1, 2), /* DCO=1944M OD=486M */
	{ /* sentinel */  }
};
#else
static const struct pll_params_table s5_hifi_pll_table[] = {
	PLL_PARAMS(67, 1), /* DCO=1608M */
	PLL_PARAMS(81, 1), /* DCO=1944M */
	{ /* sentinel */  }
};
#endif

static const struct pll_mult_range s5_hifi_pll_m = {
	.min = 67,
	.max = 133,
};

/*
 * Internal hifi pll emulation configuration parameters
 */
static struct clk_regmap s5_hifi_pll_dco = {
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
			.shift   = 16,
			.width   = 1,
		},
		.frac = {
			.reg_off = CLKCTRL_HIFIPLL_CTRL1,
			.shift   = 0,
			.width   = 19,
		},
#ifdef CONFIG_ARM
		.od = {
			.reg_off = CLKCTRL_HIFIPLL_CTRL0,
			.shift	 = 10,
			.width	 = 3,
		},
#endif
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
		.table = s5_hifi_pll_table,
		.range = &s5_hifi_pll_m,
		.init_regs = s5_hifi_init_regs,
		.init_count = ARRAY_SIZE(s5_hifi_init_regs),
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

#ifdef CONFIG_ARM
static struct clk_regmap s5_hifi_pll = {
	.hw.init = &(struct clk_init_data){
		.name = "hifi_pll",
		.ops = &meson_pll_clk_no_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_hifi_pll_dco.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};
#else
const static struct clk_div_table s5_hifi_pll_od_table[] = {
	{0,		1},  /* reg = 0; div = 1 */
	{1,		2},  /* reg = 1; div = 2 */
	{2,		4},  /* reg = 2; div = 4 */
	{3,		8},  /* reg = 3; div = 8 */
	{4,		16},  /* reg = 4; div = 16 */
	{ /* sentinel */ }
};

static struct clk_regmap s5_hifi_pll = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_HIFIPLL_CTRL0,
		.shift = 10,
		.width = 3,
		.table = s5_hifi_pll_od_table,
		.flags = CLK_DIVIDER_ALLOW_ZERO | CLK_DIVIDER_ROUND_CLOSEST,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hifi_pll",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_hifi_pll_dco.hw
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
#endif

/*
 * The Meson s5 PCIE PLL is fined tuned to deliver a very precise
 * 100MHz reference clock for the PCIe Analog PHY, and thus requires
 * a strict register sequence to enable the PLL.
 */
static const struct reg_sequence s5_pcie_pll_init_regs[] = {
	{ .reg = ANACTRL_PCIEPLL_CTRL0,	.def = 0x200c04c8 },
	{ .reg = ANACTRL_PCIEPLL_CTRL0,	.def = 0x300c04c8 },
	{ .reg = ANACTRL_PCIEPLL_CTRL1,	.def = 0x30000000 },
	{ .reg = ANACTRL_PCIEPLL_CTRL2,	.def = 0x00001100 },
	{ .reg = ANACTRL_PCIEPLL_CTRL3,	.def = 0x10058e00 },
	{ .reg = ANACTRL_PCIEPLL_CTRL4,	.def = 0x000100c0 },
	{ .reg = ANACTRL_PCIEPLL_CTRL5,	.def = 0x68000048 },
	{ .reg = ANACTRL_PCIEPLL_CTRL5,	.def = 0x68000068, .delay_us = 20 },
	{ .reg = ANACTRL_PCIEPLL_CTRL4,	.def = 0x008100c0, .delay_us = 10 },
	{ .reg = ANACTRL_PCIEPLL_CTRL0,	.def = 0x340c04c8 },
	{ .reg = ANACTRL_PCIEPLL_CTRL0,	.def = 0x140c04c8, .delay_us = 10 },
	{ .reg = ANACTRL_PCIEPLL_CTRL2,	.def = 0x00001000 }
};

#ifdef CONFIG_ARM64
/* Keep a single entry table for recalc/round_rate() ops */
static const struct pll_params_table s5_pcie_pll_table[] = {
	PLL_PARAMS(150, 1),
	{0, 0}
};
#else
static const struct pll_params_table s5_pcie_pll_table[] = {
	PLL_PARAMS(150, 1, 0),
	{0, 0, 0}
};
#endif

static struct clk_regmap s5_pcie_pll_dco = {
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
			.reg_off = ANACTRL_PCIEPLL_STS,
			.shift   = 31,
			.width   = 1,
		},
		.rst = {
			.reg_off = ANACTRL_PCIEPLL_CTRL0,
			.shift   = 29,
			.width   = 1,
		},
		.table = s5_pcie_pll_table,
		.init_regs = s5_pcie_pll_init_regs,
		.init_count = ARRAY_SIZE(s5_pcie_pll_init_regs),
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

static struct clk_fixed_factor s5_pcie_pll_dco_div2 = {
	.mult = 1,
	.div = 2,
	.hw.init = &(struct clk_init_data){
		.name = "pcie_pll_dco_div2",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_pcie_pll_dco.hw
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

static struct clk_regmap s5_pcie_pll_od = {
	.data = &(struct clk_regmap_div_data){
		.offset = ANACTRL_PCIEPLL_CTRL0,
		.shift = 16,
		.width = 5,
		.flags = CLK_DIVIDER_ONE_BASED | CLK_DIVIDER_ALLOW_ZERO |
			CLK_DIVIDER_ROUND_CLOSEST,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pcie_pll_od",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_pcie_pll_dco_div2.hw
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

static struct clk_fixed_factor s5_pcie_pll = {
	.mult = 1,
	.div = 2,
	.hw.init = &(struct clk_init_data){
		.name = "pcie_pll",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_pcie_pll_od.hw
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

static struct clk_regmap s5_pcie_bgp = {
	.data = &(struct clk_regmap_gate_data){
		.offset = ANACTRL_PCIEPLL_CTRL5,
		.bit_idx = 27,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pcie_bgp",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &s5_pcie_pll.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap s5_pcie_hcsl_out = {
	.data = &(struct clk_regmap_gate_data){
		.offset = ANACTRL_PCIEPLL_CTRL5,
		.bit_idx = 3,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pcie_hcsl",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &s5_pcie_bgp.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/* pcie_clk_in from chip pad */
static struct clk_regmap s5_pcie_hcsl_pad = {
	.hw.init = &(struct clk_init_data){
			.name = "pcie_hcsl_pad",
			.ops = &meson_pll_clk_no_ops,
			.parent_hws = NULL,
			.num_parents = 0,
		},

};

static struct clk_regmap s5_pcie_hcsl_in_pad = {
	.data = &(struct clk_regmap_gate_data){
		.offset = ANACTRL_PCIEPLL_CTRL5,
		.bit_idx = 2,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pcie_hcsl_in_pad",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &s5_pcie_hcsl_pad.hw },
		.num_parents = 1,
	},
};

static struct clk_regmap s5_pcie_clk_in_sel = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = ANACTRL_PCIEPLL_CTRL5,
		.mask = 0x1,
		.shift = 2,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pcie_clk_in",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_pcie_pll.hw,
			&s5_pcie_hcsl_in_pad.hw
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct reg_sequence s5_pcie1_pll_init_regs[] = {
	{ .reg = ANACTRL_PCIEPLL_CTRL6,	.def = 0x200c04c8 },
	{ .reg = ANACTRL_PCIEPLL_CTRL6,	.def = 0x300c04c8 },
	{ .reg = ANACTRL_PCIEPLL_CTRL7,	.def = 0x30000000 },
	{ .reg = ANACTRL_PCIEPLL_CTRL8,	.def = 0x00001100 },
	{ .reg = ANACTRL_PCIEPLL_CTRL9,	.def = 0x10058e00 },
	{ .reg = ANACTRL_PCIEPLL_CTRL10, .def = 0x000100c0 },
	{ .reg = ANACTRL_PCIEPLL_CTRL11, .def = 0x68000048 },
	{ .reg = ANACTRL_PCIEPLL_CTRL11, .def = 0x68000068, .delay_us = 20 },
	{ .reg = ANACTRL_PCIEPLL_CTRL10, .def = 0x008100c0, .delay_us = 10 },
	{ .reg = ANACTRL_PCIEPLL_CTRL6,	.def = 0x340c04c8 },
	{ .reg = ANACTRL_PCIEPLL_CTRL6,	.def = 0x140c04c8, .delay_us = 10 },
	{ .reg = ANACTRL_PCIEPLL_CTRL8,	.def = 0x00001000 }
};

#ifdef CONFIG_ARM64
/* Keep a single entry table for recalc/round_rate() ops */
static const struct pll_params_table s5_pcie1_pll_table[] = {
	PLL_PARAMS(150, 1),
	{0, 0}
};
#else
static const struct pll_params_table s5_pcie1_pll_table[] = {
	PLL_PARAMS(150, 1, 0),
	{0, 0, 0}
};
#endif

static struct clk_regmap s5_pcie1_pll_dco = {
	.data = &(struct meson_clk_pll_data){
		.en = {
			.reg_off = ANACTRL_PCIEPLL_CTRL6,
			.shift   = 28,
			.width   = 1,
		},
		.m = {
			.reg_off = ANACTRL_PCIEPLL_CTRL6,
			.shift   = 0,
			.width   = 8,
		},
		.n = {
			.reg_off = ANACTRL_PCIEPLL_CTRL6,
			.shift   = 10,
			.width   = 5,
		},
		.frac = {
			.reg_off = ANACTRL_PCIEPLL_CTRL7,
			.shift   = 0,
			.width   = 12,
		},
		.l = {
			.reg_off = ANACTRL_PCIEPLL_STS1,
			.shift   = 31,
			.width   = 1,
		},
		.rst = {
			.reg_off = ANACTRL_PCIEPLL_CTRL6,
			.shift   = 29,
			.width   = 1,
		},
		.table = s5_pcie1_pll_table,
		.init_regs = s5_pcie1_pll_init_regs,
		.init_count = ARRAY_SIZE(s5_pcie1_pll_init_regs),
	},
	.hw.init = &(struct clk_init_data){
		.name = "pcie1_pll_dco",
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

static struct clk_fixed_factor s5_pcie1_pll_dco_div2 = {
	.mult = 1,
	.div = 2,
	.hw.init = &(struct clk_init_data){
		.name = "pcie1_pll_dco_div2",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_pcie1_pll_dco.hw
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

static struct clk_regmap s5_pcie1_pll_od = {
	.data = &(struct clk_regmap_div_data){
		.offset = ANACTRL_PCIEPLL_CTRL6,
		.shift = 16,
		.width = 5,
		.flags = CLK_DIVIDER_ONE_BASED | CLK_DIVIDER_ALLOW_ZERO |
			CLK_DIVIDER_ROUND_CLOSEST,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pcie1_pll_od",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_pcie1_pll_dco_div2.hw
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

static struct clk_fixed_factor s5_pcie1_pll = {
	.mult = 1,
	.div = 2,
	.hw.init = &(struct clk_init_data){
		.name = "pcie1_pll",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_pcie1_pll_od.hw
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

static struct clk_regmap s5_pcie1_bgp = {
	.data = &(struct clk_regmap_gate_data){
		.offset = ANACTRL_PCIEPLL_CTRL11,
		.bit_idx = 27,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pcie1_bgp",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &s5_pcie1_pll.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap s5_pcie1_hcsl_out = {
	.data = &(struct clk_regmap_gate_data){
		.offset = ANACTRL_PCIEPLL_CTRL11,
		.bit_idx = 3,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pcie1_hcsl",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &s5_pcie1_bgp.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/* pcie_clk_in from chip pad */
static struct clk_regmap s5_pcie1_hcsl_pad = {
	.hw.init = &(struct clk_init_data){
			.name = "pcie1_hcsl_pad",
			.ops = &meson_pll_clk_no_ops,
			.parent_hws = NULL,
			.num_parents = 0,
		},

};

static struct clk_regmap s5_pcie1_hcsl_in_pad = {
	.data = &(struct clk_regmap_gate_data){
		.offset = ANACTRL_PCIEPLL_CTRL11,
		.bit_idx = 2,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pcie1_hcsl_in_pad",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &s5_pcie1_hcsl_pad.hw },
		.num_parents = 1,
	},
};

static struct clk_regmap s5_pcie1_clk_in_sel = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = ANACTRL_PCIEPLL_CTRL11,
		.mask = 0x1,
		.shift = 2,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pcie1_clk_in",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_pcie1_pll.hw,
			&s5_pcie1_hcsl_in_pad.hw
		},
		.num_parents = 2,
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
 *	         output N1 and N2 in run.
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
static struct clk_regmap s5_rtc_32k_clkin = {
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

static const struct meson_clk_dualdiv_param s5_32k_div_table[] = {
	{
		.dual	= 1,
		.n1	= 733,
		.m1	= 8,
		.n2	= 732,
		.m2	= 11,
	},
	{}
};

static struct clk_regmap s5_rtc_32k_div = {
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
		.table = s5_32k_div_table,
	},
	.hw.init = &(struct clk_init_data){
		.name = "rtc_32k_div",
		.ops = &meson_clk_dualdiv_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_rtc_32k_clkin.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap s5_rtc_32k_xtal = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_RTC_BY_OSCIN_CTRL1,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "rtc_32k_xtal",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_rtc_32k_clkin.hw
		},
		.num_parents = 1,
	},
};

/*
 * three parent for rtc clock out
 * pad is from where?
 */
static u32 rtc_32k_sel[] = {0, 1};

static struct clk_regmap s5_rtc_32k_sel = {
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
			&s5_rtc_32k_xtal.hw,
			&s5_rtc_32k_div.hw
		},
		.num_parents = 2,
		/*
		 * rtc 32k is directly used in other modules, and the
		 * parent rate needs to be modified
		 */
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s5_rtc_clk = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_RTC_BY_OSCIN_CTRL0,
		.bit_idx = 30,
	},
	.hw.init = &(struct clk_init_data){
		.name = "rtc_clk",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_rtc_32k_sel.hw,
		},
		.num_parents = 1,
		/*
		 * rtc 32k is directly used in other modules, and the
		 * parent rate needs to be modified
		 */
		.flags = CLK_SET_RATE_PARENT,
	},
};

/*ceca_clk*/
static struct clk_regmap s5_ceca_32k_clkin = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_CECA_CTRL0,
		.bit_idx = 31,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "ceca_32k_clkin",
		.ops = &clk_regmap_gate_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "xtal",
		},
		.num_parents = 1,
	},
};

static struct clk_regmap s5_ceca_32k_div = {
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
		.table = s5_32k_div_table,
	},
	.hw.init = &(struct clk_init_data){
		.name = "ceca_32k_div",
		.ops = &meson_clk_dualdiv_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_ceca_32k_clkin.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap s5_ceca_32k_sel_pre = {
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
			&s5_ceca_32k_div.hw,
			&s5_ceca_32k_clkin.hw
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s5_ceca_32k_sel = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_CECA_CTRL1,
		.mask = 0x1,
		.shift = 31,
	},
	.hw.init = &(struct clk_init_data){
		.name = "ceca_32k_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_ceca_32k_sel_pre.hw,
			&s5_rtc_clk.hw
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s5_ceca_32k_clkout = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_CECA_CTRL0,
		.bit_idx = 30,
	},
	.hw.init = &(struct clk_init_data){
		.name = "ceca_32k_clkout",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_ceca_32k_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/*cecb_clk*/
static struct clk_regmap s5_cecb_32k_clkin = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_CECB_CTRL0,
		.bit_idx = 31,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "cecb_32k_clkin",
		.ops = &clk_regmap_gate_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "xtal",
		},
		.num_parents = 1,
	},
};

static struct clk_regmap s5_cecb_32k_div = {
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
		.table = s5_32k_div_table,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cecb_32k_div",
		.ops = &meson_clk_dualdiv_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_cecb_32k_clkin.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap s5_cecb_32k_sel_pre = {
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
			&s5_cecb_32k_div.hw,
			&s5_cecb_32k_clkin.hw
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s5_cecb_32k_sel = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_CECB_CTRL1,
		.mask = 0x1,
		.shift = 31,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cecb_32k_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_cecb_32k_sel_pre.hw,
			&s5_rtc_clk.hw
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s5_cecb_32k_clkout = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_CECB_CTRL0,
		.bit_idx = 30,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cecb_32k_clkout",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_cecb_32k_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/* sys clk = fclk_div4 / 3, and it is set in firmware */
static u32 mux_table_sys_clk_sel[] = { 0, 1, 2, 3, 4 };

static const struct clk_parent_data sys_clk_parent_data[] = {
	{ .fw_name = "xtal", },
	{ .hw = &s5_fclk_div2.hw },
	{ .hw = &s5_fclk_div3.hw },
	{ .hw = &s5_fclk_div4.hw },
	{ .hw = &s5_fclk_div5.hw },
};

static struct clk_regmap s5_sysclk_1_sel = {
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

static struct clk_regmap s5_sysclk_1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_SYS_CLK_CTRL0,
		.shift = 16,
		.width = 10,
		.flags = CLK_DIVIDER_ALLOW_ZERO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sysclk_1_div",
		.ops = &clk_regmap_divider_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_sysclk_1_sel.hw,
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap s5_sysclk_1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_SYS_CLK_CTRL0,
		.bit_idx = 29,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sysclk_1",
		.ops = &clk_regmap_gate_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_sysclk_1_div.hw,
		},
		.num_parents = 1,
	},
};

static struct clk_regmap s5_sysclk_0_sel = {
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

static struct clk_regmap s5_sysclk_0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_SYS_CLK_CTRL0,
		.shift = 0,
		.width = 10,
		.flags = CLK_DIVIDER_ALLOW_ZERO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sysclk_0_div",
		.ops = &clk_regmap_divider_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_sysclk_0_sel.hw,
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap s5_sysclk_0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_SYS_CLK_CTRL0,
		.bit_idx = 13,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sysclk_0",
		.ops = &clk_regmap_gate_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_sysclk_0_div.hw,
		},
		.num_parents = 1,
	},
};

static struct clk_regmap s5_sys_clk = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_SYS_CLK_CTRL0,
		.mask = 0x1,
		.shift = 15,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sys_clk",
		.ops = &clk_regmap_mux_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_sysclk_0.hw,
			&s5_sysclk_1.hw
		},
		.num_parents = 2,
	},
};

/* sys clk = fclk_div4 / 3, and it is set in firmware */
static u32 mux_table_axi_clk_sel[] = { 0, 1, 2, 3, 4, 6 };

static const struct clk_parent_data axi_clk_parent_data[] = {
	{ .fw_name = "xtal", },
	{ .hw = &s5_fclk_div2.hw },
	{ .hw = &s5_fclk_div3.hw },
	{ .hw = &s5_fclk_div4.hw },
	{ .hw = &s5_fclk_div5.hw },
	{ .hw = &s5_fclk_div2p5.hw },
};

static struct clk_regmap s5_axiclk_1_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_AXI_CLK_CTRL0,
		.mask = 0x7,
		.shift = 26,
		.table = mux_table_axi_clk_sel,
	},
	.hw.init = &(struct clk_init_data){
		.name = "axiclk_1_sel",
		.ops = &clk_regmap_mux_ro_ops,
		.parent_data = axi_clk_parent_data,
		.num_parents = ARRAY_SIZE(axi_clk_parent_data),
	},
};

static struct clk_regmap s5_axiclk_1_div = {
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
			&s5_axiclk_1_sel.hw,
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap s5_axiclk_1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_AXI_CLK_CTRL0,
		.bit_idx = 29,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "axiclk_1",
		.ops = &clk_regmap_gate_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_axiclk_1_div.hw,
		},
		.num_parents = 1,
	},
};

static struct clk_regmap s5_axiclk_0_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_AXI_CLK_CTRL0,
		.mask = 0x7,
		.shift = 10,
		.table = mux_table_axi_clk_sel,
	},
	.hw.init = &(struct clk_init_data){
		.name = "axiclk_0_sel",
		.ops = &clk_regmap_mux_ro_ops,
		.parent_data = axi_clk_parent_data,
		.num_parents = ARRAY_SIZE(axi_clk_parent_data),
	},
};

static struct clk_regmap s5_axiclk_0_div = {
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
			&s5_axiclk_0_sel.hw,
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap s5_axiclk_0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_AXI_CLK_CTRL0,
		.bit_idx = 13,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "axiclk_0",
		.ops = &clk_regmap_gate_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_axiclk_0_div.hw,
		},
		.num_parents = 1,
	},
};

static struct clk_regmap s5_axi_clk = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_AXI_CLK_CTRL0,
		.mask = 0x1,
		.shift = 15,
	},
	.hw.init = &(struct clk_init_data){
		.name = "axi_clk",
		.ops = &clk_regmap_mux_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_axiclk_0.hw,
			&s5_axiclk_1.hw
		},
		.num_parents = 2,
		.flags = CLK_IS_CRITICAL,
	},
};

/*12_24M clk*/
static struct clk_regmap s5_24m_clk_gate = {
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

static struct clk_fixed_factor s5_24m_div2 = {
	.mult = 1,
	.div = 2,
	.hw.init = &(struct clk_init_data){
		.name = "24m_div2",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_24m_clk_gate.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap s5_12m_clk = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_CLK12_24_CTRL,
		.bit_idx = 10,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "12m_clk",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_24m_div2.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap s5_25m_clk_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_CLK12_24_CTRL,
		.shift = 0,
		.width = 8,
		.flags = CLK_DIVIDER_ALLOW_ZERO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "25m_clk_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_fclk_div2.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap s5_25m_clk = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_CLK12_24_CTRL,
		.bit_idx = 12,
	},
	.hw.init = &(struct clk_init_data){
		.name = "25m_clk",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_25m_clk_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static u32 mux_table_vclk_sel[] = {1, 4, 5, 6, 7};
static const struct clk_hw *s5_vclk_parent_hws[] = {
	//&s5_vid_pll.hw, //TODO: Need to confirm vid pll with vlsi
	&s5_gp2_pll.hw,
	&s5_fpll_pixel.hw,
	&s5_fclk_div4.hw,
	&s5_fclk_div5.hw,
	&s5_fclk_div7.hw
};

static struct clk_regmap s5_vclk_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VID_CLK0_CTRL,
		.mask = 0x7,
		.shift = 16,
		.table = mux_table_vclk_sel,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vclk_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = s5_vclk_parent_hws,
		.num_parents = ARRAY_SIZE(s5_vclk_parent_hws),
		.flags = CLK_SET_RATE_NO_REPARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap s5_vclk2_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VIID_CLK0_CTRL,
		.mask = 0x7,
		.shift = 16,
		.table = mux_table_vclk_sel,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vclk2_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = s5_vclk_parent_hws,
		.num_parents = ARRAY_SIZE(s5_vclk_parent_hws),
		.flags = CLK_SET_RATE_NO_REPARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap s5_vclk_input = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VID_CLK0_DIV,
		.bit_idx = 16,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk_input",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &s5_vclk_sel.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap s5_vclk2_input = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VIID_CLK0_DIV,
		.bit_idx = 16,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk2_input",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &s5_vclk2_sel.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap s5_vclk_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_VID_CLK0_DIV,
		.shift = 0,
		.width = 8,
		.flags = CLK_DIVIDER_ALLOW_ZERO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vclk_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_vclk_input.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap s5_vclk2_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_VIID_CLK0_DIV,
		.shift = 0,
		.width = 8,
		.flags = CLK_DIVIDER_ALLOW_ZERO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vclk2_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_vclk2_input.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap s5_vclk = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VID_CLK0_CTRL,
		.bit_idx = 19,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &s5_vclk_div.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap s5_vclk2 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VIID_CLK0_CTRL,
		.bit_idx = 19,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk2",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &s5_vclk2_div.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap s5_vclk_div1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VID_CLK0_CTRL,
		.bit_idx = 0,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk_div1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &s5_vclk.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap s5_vclk_div2_en = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VID_CLK0_CTRL,
		.bit_idx = 1,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk_div2_en",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &s5_vclk.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap s5_vclk_div4_en = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VID_CLK0_CTRL,
		.bit_idx = 2,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk_div4_en",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &s5_vclk.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap s5_vclk_div6_en = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VID_CLK0_CTRL,
		.bit_idx = 3,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk_div6_en",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &s5_vclk.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap s5_vclk_div12_en = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VID_CLK0_CTRL,
		.bit_idx = 4,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk_div12_en",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &s5_vclk.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap s5_vclk2_div1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VIID_CLK0_CTRL,
		.bit_idx = 0,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk2_div1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &s5_vclk2.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap s5_vclk2_div2_en = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VIID_CLK0_CTRL,
		.bit_idx = 1,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk2_div2_en",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &s5_vclk2.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap s5_vclk2_div4_en = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VIID_CLK0_CTRL,
		.bit_idx = 2,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk2_div4_en",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &s5_vclk2.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap s5_vclk2_div6_en = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VIID_CLK0_CTRL,
		.bit_idx = 3,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk2_div6_en",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &s5_vclk2.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap s5_vclk2_div12_en = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VIID_CLK0_CTRL,
		.bit_idx = 4,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk2_div12_en",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &s5_vclk2.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_fixed_factor s5_vclk_div2 = {
	.mult = 1,
	.div = 2,
	.hw.init = &(struct clk_init_data){
		.name = "vclk_div2",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_vclk_div2_en.hw
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor s5_vclk_div4 = {
	.mult = 1,
	.div = 4,
	.hw.init = &(struct clk_init_data){
		.name = "vclk_div4",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_vclk_div4_en.hw
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor s5_vclk_div6 = {
	.mult = 1,
	.div = 6,
	.hw.init = &(struct clk_init_data){
		.name = "vclk_div6",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_vclk_div6_en.hw
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor s5_vclk_div12 = {
	.mult = 1,
	.div = 12,
	.hw.init = &(struct clk_init_data){
		.name = "vclk_div12",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_vclk_div12_en.hw
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor s5_vclk2_div2 = {
	.mult = 1,
	.div = 2,
	.hw.init = &(struct clk_init_data){
		.name = "vclk2_div2",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_vclk2_div2_en.hw
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor s5_vclk2_div4 = {
	.mult = 1,
	.div = 4,
	.hw.init = &(struct clk_init_data){
		.name = "vclk2_div4",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_vclk2_div4_en.hw
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor s5_vclk2_div6 = {
	.mult = 1,
	.div = 6,
	.hw.init = &(struct clk_init_data){
		.name = "vclk2_div6",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_vclk2_div6_en.hw
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor s5_vclk2_div12 = {
	.mult = 1,
	.div = 12,
	.hw.init = &(struct clk_init_data){
		.name = "vclk2_div12",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_vclk2_div12_en.hw
		},
		.num_parents = 1,
	},
};

static u32 mux_table_cts_sel[] = { 0, 1, 2, 3, 4, 8, 9, 10, 11, 12 };
static const struct clk_hw *s5_cts_parent_hws[] = {
	&s5_vclk_div1.hw,
	&s5_vclk_div2.hw,
	&s5_vclk_div4.hw,
	&s5_vclk_div6.hw,
	&s5_vclk_div12.hw,
	&s5_vclk2_div1.hw,
	&s5_vclk2_div2.hw,
	&s5_vclk2_div4.hw,
	&s5_vclk2_div6.hw,
	&s5_vclk2_div12.hw
};

static struct clk_regmap s5_cts_enci_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VID_CLK0_DIV,
		.mask = 0xf,
		.shift = 28,
		.table = mux_table_cts_sel,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cts_enci_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = s5_cts_parent_hws,
		.num_parents = ARRAY_SIZE(s5_cts_parent_hws),
		.flags = CLK_SET_RATE_NO_REPARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap s5_cts_enct_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VID_CLK0_DIV,
		.mask = 0xf,
		.shift = 20,
		.table = mux_table_cts_sel,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cts_enct_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = s5_cts_parent_hws,
		.num_parents = ARRAY_SIZE(s5_cts_parent_hws),
		.flags = CLK_SET_RATE_NO_REPARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap s5_cts_encp_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VID_CLK0_DIV,
		.mask = 0xf,
		.shift = 24,
		.table = mux_table_cts_sel,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cts_encp_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = s5_cts_parent_hws,
		.num_parents = ARRAY_SIZE(s5_cts_parent_hws),
		.flags = CLK_SET_RATE_NO_REPARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap s5_cts_encl_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VIID_CLK0_DIV,
		.mask = 0xf,
		.shift = 12,
		.table = mux_table_cts_sel,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cts_encl_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = s5_cts_parent_hws,
		.num_parents = ARRAY_SIZE(s5_cts_parent_hws),
		.flags = CLK_SET_RATE_NO_REPARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap s5_cts_vdac_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VIID_CLK0_DIV,
		.mask = 0xf,
		.shift = 28,
		.table = mux_table_cts_sel,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cts_vdac_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = s5_cts_parent_hws,
		.num_parents = ARRAY_SIZE(s5_cts_parent_hws),
		.flags = CLK_SET_RATE_NO_REPARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap s5_cts_enci = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VID_CLK0_CTRL2,
		.bit_idx = 0,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "cts_enci",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_cts_enci_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap s5_cts_enct = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VID_CLK0_CTRL2,
		.bit_idx = 1,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "cts_enct",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_cts_enct_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap s5_cts_encp = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VID_CLK0_CTRL2,
		.bit_idx = 2,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "cts_encp",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_cts_encp_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap s5_cts_encl = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VID_CLK0_CTRL2,
		.bit_idx = 3,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "cts_encl",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_cts_encl_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap s5_cts_vdac = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VID_CLK0_CTRL2,
		.bit_idx = 4,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "cts_vdac",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_cts_vdac_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static u32 s5_enc_hdmi_tx_mux_table[] = { 0, 1, 2, 3, 4, 8, 9, 10, 11, 12 };
static const struct clk_hw *s5_enc_hdmi_tx_parent_hws[] = {
	&s5_vclk_div1.hw,
	&s5_vclk_div2.hw,
	&s5_vclk_div4.hw,
	&s5_vclk_div6.hw,
	&s5_vclk_div12.hw,
	&s5_vclk2_div1.hw,
	&s5_vclk2_div2.hw,
	&s5_vclk2_div4.hw,
	&s5_vclk2_div6.hw,
	&s5_vclk2_div12.hw
};

static struct clk_regmap s5_enc_hdmi_tx_pnx_clk_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_ENC0_HDMI_CLK_CTRL,
		.mask = 0xf,
		.shift = 24,
		.table = s5_enc_hdmi_tx_mux_table,
	},
	.hw.init = &(struct clk_init_data){
		.name = "enc_hdmi_tx_pnx_clk_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = s5_enc_hdmi_tx_parent_hws,
		.num_parents = ARRAY_SIZE(s5_enc_hdmi_tx_parent_hws),
	},
};

static struct clk_regmap s5_enc_hdmi_tx_pnx_clk = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VID_CLK0_CTRL2,
		.bit_idx = 10,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "enc_hdmi_tx_pnx_clk",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_enc_hdmi_tx_pnx_clk_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s5_enc_hdmi_tx_fe_clk_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_ENC0_HDMI_CLK_CTRL,
		.mask = 0xf,
		.shift = 20,
		.table = s5_enc_hdmi_tx_mux_table,
	},
	.hw.init = &(struct clk_init_data){
		.name = "enc_hdmi_tx_fe_clk_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = s5_enc_hdmi_tx_parent_hws,
		.num_parents = ARRAY_SIZE(s5_enc_hdmi_tx_parent_hws),
	},
};

static struct clk_regmap s5_enc_hdmi_tx_fe_clk = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VID_CLK0_CTRL2,
		.bit_idx = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "enc_hdmi_tx_fe_clk",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_enc_hdmi_tx_fe_clk_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s5_enc_hdmi_tx_pixel_clk_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_ENC0_HDMI_CLK_CTRL,
		.mask = 0xf,
		.shift = 16,
		.table = s5_enc_hdmi_tx_mux_table,
	},
	.hw.init = &(struct clk_init_data){
		.name = "enc_hdmi_tx_pixel_clk_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = s5_enc_hdmi_tx_parent_hws,
		.num_parents = ARRAY_SIZE(s5_enc_hdmi_tx_parent_hws),
	},
};

static struct clk_regmap s5_enc_hdmi_tx_pixel_clk = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VID_CLK0_CTRL2,
		.bit_idx = 5,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "enc_hdmi_tx_pixel_clk",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_enc_hdmi_tx_pixel_clk_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static u32 s5_cts_hdmi_tx_pnx_clk_mux_table[] = { 0, 1 };
static const struct clk_hw *s5_hdmi_tx_pnx_clk_parent_hws[] = {
	&s5_enc_hdmi_tx_pnx_clk.hw,
	&s5_gp2_pll.hw,
};

static struct clk_regmap s5_hdmi_tx_pnx_clk_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_ENC_HDMI_CLK_CTRL,
		.mask = 0x3,
		.shift = 21,
		.table = s5_cts_hdmi_tx_pnx_clk_mux_table,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmi_tx_pnx_clk_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = s5_hdmi_tx_pnx_clk_parent_hws,
		.num_parents = ARRAY_SIZE(s5_hdmi_tx_pnx_clk_parent_hws),
	},
};

static u32 s5_cts_hdmi_tx_fe_clk_mux_table[] = { 0, 1 };
static const struct clk_hw *s5_hdmi_tx_fe_clk_parent_hws[] = {
	&s5_enc_hdmi_tx_fe_clk.hw,
	&s5_gp2_pll.hw,
};

static struct clk_regmap s5_hdmi_tx_fe_clk_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_ENC_HDMI_CLK_CTRL,
		.mask = 0x3,
		.shift = 13,
		.table = s5_cts_hdmi_tx_fe_clk_mux_table,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmi_tx_fe_clk_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = s5_hdmi_tx_fe_clk_parent_hws,
		.num_parents = ARRAY_SIZE(s5_hdmi_tx_fe_clk_parent_hws),
	},
};

static u32 s5_cts_hdmi_tx_pixel_clk_mux_table[] = { 0, 1 };
static const struct clk_hw *s5_hdmi_tx_pixel_clk_parent_hws[] = {
	&s5_enc_hdmi_tx_pixel_clk.hw,
	&s5_gp2_pll.hw,
};

static struct clk_regmap s5_hdmi_tx_pixel_clk_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_ENC_HDMI_CLK_CTRL,
		.mask = 0x3,
		.shift = 5,
		.table = s5_cts_hdmi_tx_pixel_clk_mux_table,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmi_tx_pixel_clk_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = s5_hdmi_tx_pixel_clk_parent_hws,
		.num_parents = ARRAY_SIZE(s5_hdmi_tx_pixel_clk_parent_hws),
	},
};

static struct clk_regmap s5_hdmi_tx_pnx_clk_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_ENC_HDMI_CLK_CTRL,
		.shift = 16,
		.width = 4,
		.flags = CLK_DIVIDER_ALLOW_ZERO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmi_tx_pnx_clk_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_hdmi_tx_pnx_clk_sel.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap s5_hdmi_tx_fe_clk_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_ENC_HDMI_CLK_CTRL,
		.shift = 8,
		.width = 4,
		.flags = CLK_DIVIDER_ALLOW_ZERO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmi_tx_fe_clk_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_hdmi_tx_fe_clk_sel.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap s5_hdmi_tx_pixel_clk_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_ENC_HDMI_CLK_CTRL,
		.shift = 0,
		.width = 4,
		.flags = CLK_DIVIDER_ALLOW_ZERO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmi_tx_pixel_clk_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_hdmi_tx_pixel_clk_sel.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap s5_hdmi_tx_pnx_clk = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_ENC_HDMI_CLK_CTRL,
		.bit_idx = 20,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmi_tx_pnx_clk",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_hdmi_tx_pnx_clk_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s5_hdmi_tx_fe_clk = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_ENC_HDMI_CLK_CTRL,
		.bit_idx = 12,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmi_tx_fe_clk",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_hdmi_tx_fe_clk_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s5_hdmi_tx_pixel_clk = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_ENC_HDMI_CLK_CTRL,
		.bit_idx = 4,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmi_tx_pixel_clk",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_hdmi_tx_pixel_clk_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static u32 s5_htx_tmds_table[] = { 1, 2 };
static const struct clk_hw *s5_htx_tmds_parent_hws[] = {
	&s5_fpll_tmds.hw,
	&s5_fclk_div3.hw,
};

static struct clk_regmap s5_htx_tmds_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_HTX_CLK_CTRL1,
		.mask = 0x7,
		.shift = 25,
		.table = s5_htx_tmds_table,
	},
	.hw.init = &(struct clk_init_data){
		.name = "htx_tmds_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = s5_htx_tmds_parent_hws,
		.num_parents = ARRAY_SIZE(s5_htx_tmds_parent_hws),
	},
};

static struct clk_regmap s5_htx_tmds_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_HTX_CLK_CTRL1,
		.shift = 16,
		.width = 7,
		.flags = CLK_DIVIDER_ALLOW_ZERO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "htx_tmds_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_htx_tmds_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s5_htx_tmds = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_HTX_CLK_CTRL1,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "htx_tmds",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_htx_tmds_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static u32 s5_vapb_table[] = { 0, 1, 2, 3};
static const struct clk_hw *s5_vapb_parent_hws[] = {
	&s5_fclk_div4.hw,
	&s5_fclk_div3.hw,
	&s5_fclk_div5.hw,
	&s5_fclk_div7.hw,
};

static struct clk_regmap s5_vapb_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VAPBCLK_CTRL,
		.mask = 0x7,
		.shift = 9,
		.table = s5_vapb_table,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vapb_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = s5_vapb_parent_hws,
		.num_parents = ARRAY_SIZE(s5_vapb_parent_hws),
	},
};

static struct clk_regmap s5_vapb_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_VAPBCLK_CTRL,
		.shift = 0,
		.width = 7,
		.flags = CLK_DIVIDER_ALLOW_ZERO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vapb_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_vapb_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s5_vapb = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VAPBCLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vapb",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_vapb_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static u32 s5_ge2d_table[] = { 0, 1, 2, 3, 5, 6 };

static const struct clk_parent_data s5_ge2d_clk_parent_data[] = {
	{ .fw_name = "xtal", },
	{ .hw = &s5_fclk_div2p5.hw },
	{ .hw = &s5_fclk_div3.hw },
	{ .hw = &s5_nna_pll.hw },
	{ .hw = &s5_fclk_div4.hw },
	{ .hw = &s5_sys2_pll.hw },
};

static struct clk_regmap s5_ge2d_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_GE2DCLK_CTRL,
		.mask = 0x7,
		.shift = 9,
		.table = s5_ge2d_table,
	},
	.hw.init = &(struct clk_init_data){
		.name = "ge2d_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = s5_ge2d_clk_parent_data,
		.num_parents = ARRAY_SIZE(s5_ge2d_clk_parent_data),
	},
};

static struct clk_regmap s5_ge2d_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_GE2DCLK_CTRL,
		.shift = 0,
		.width = 7,
		.flags = CLK_DIVIDER_ALLOW_ZERO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "ge2d_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_ge2d_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s5_ge2d = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_GE2DCLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "ge2d",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_ge2d_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static u32 s5_sd_emmc_table[] = { 0, 1, 2, 4, 6, 7 };
static const struct clk_parent_data s5_sd_emmc_parent_data[] = {
	{ .fw_name = "xtal", },
	{ .hw = &s5_fclk_div2.hw },
	{ .hw = &s5_fclk_div3.hw },
	{ .hw = &s5_fclk_div2p5.hw },
	{ .hw = &s5_hifi1_pll.hw },
	{ .hw = &s5_gp0_pll.hw }
};

static struct clk_regmap s5_sd_emmc_c_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_NAND_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
		.table = s5_sd_emmc_table,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_c_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = s5_sd_emmc_parent_data,
		.num_parents = ARRAY_SIZE(s5_sd_emmc_parent_data),
	},
};

static struct clk_regmap s5_sd_emmc_c_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_NAND_CLK_CTRL,
		.shift = 0,
		.width = 7,
		.flags = CLK_DIVIDER_ALLOW_ZERO,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_c_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_sd_emmc_c_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap s5_sd_emmc_c = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_NAND_CLK_CTRL,
		.bit_idx = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sd_emmc_c",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_sd_emmc_c_div.hw
		},
		.num_parents = 1,
		.flags = CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap s5_sd_emmc_a_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_SD_EMMC_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
		.table = s5_sd_emmc_table,
		.flags = CLK_MUX_ROUND_CLOSEST,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_a_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = s5_sd_emmc_parent_data,
		.num_parents = ARRAY_SIZE(s5_sd_emmc_parent_data),
	},
};

static struct clk_regmap s5_sd_emmc_a_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_SD_EMMC_CLK_CTRL,
		.shift = 0,
		.width = 7,
		.flags = CLK_DIVIDER_ALLOW_ZERO,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_a_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_sd_emmc_a_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap s5_sd_emmc_a = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_SD_EMMC_CLK_CTRL,
		.bit_idx = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sd_emmc_a",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_sd_emmc_a_div.hw
		},
		.num_parents = 1,
		.flags = CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap s5_sd_emmc_b_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_SD_EMMC_CLK_CTRL,
		.mask = 0x7,
		.shift = 25,
		.table = s5_sd_emmc_table,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_b_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = s5_sd_emmc_parent_data,
		.num_parents = ARRAY_SIZE(s5_sd_emmc_parent_data),
	},
};

static struct clk_regmap s5_sd_emmc_b_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_SD_EMMC_CLK_CTRL,
		.shift = 16,
		.width = 7,
		.flags = CLK_DIVIDER_ALLOW_ZERO,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_b_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_sd_emmc_b_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap s5_sd_emmc_b = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_SD_EMMC_CLK_CTRL,
		.bit_idx = 23,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sd_emmc_b",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_sd_emmc_b_div.hw
		},
		.num_parents = 1,
		.flags = CLK_IGNORE_UNUSED,
	},
};

static const struct clk_parent_data s5_spicc_parent_hws[] = {
	{ .fw_name = "xtal", },
	{ .hw = &s5_sys_clk.hw },
	{ .hw = &s5_fclk_div4.hw },
	{ .hw = &s5_fclk_div3.hw },
	{ .hw = &s5_fclk_div2.hw },
	{ .hw = &s5_fclk_div5.hw },
	{ .hw = &s5_fclk_div7.hw },
};

static struct clk_regmap s5_spicc0_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_SPICC_CLK_CTRL,
		.mask = 0x7,
		.shift = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spicc0_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = s5_spicc_parent_hws,
		.num_parents = ARRAY_SIZE(s5_spicc_parent_hws),
	},
};

static struct clk_regmap s5_spicc0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_SPICC_CLK_CTRL,
		.shift = 0,
		.width = 6,
		.flags = CLK_DIVIDER_ALLOW_ZERO,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spicc0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_spicc0_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s5_spicc0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_SPICC_CLK_CTRL,
		.bit_idx = 6,
	},
	.hw.init = &(struct clk_init_data){
		.name = "spicc0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_spicc0_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s5_spicc1_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_SPICC_CLK_CTRL,
		.mask = 0x7,
		.shift = 23,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spicc1_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = s5_spicc_parent_hws,
		.num_parents = ARRAY_SIZE(s5_spicc_parent_hws),
	},
};

static struct clk_regmap s5_spicc1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_SPICC_CLK_CTRL,
		.shift = 16,
		.width = 6,
		.flags = CLK_DIVIDER_ALLOW_ZERO,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spicc1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_spicc1_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s5_spicc1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_SPICC_CLK_CTRL,
		.bit_idx = 22,
	},
	.hw.init = &(struct clk_init_data){
		.name = "spicc1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_spicc1_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s5_spicc2_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_SPICC_CLK_CTRL1,
		.mask = 0x7,
		.shift = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spicc2_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = s5_spicc_parent_hws,
		.num_parents = ARRAY_SIZE(s5_spicc_parent_hws),
	},
};

static struct clk_regmap s5_spicc2_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_SPICC_CLK_CTRL1,
		.shift = 0,
		.width = 6,
		.flags = CLK_DIVIDER_ALLOW_ZERO,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spicc2_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_spicc2_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s5_spicc2 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_SPICC_CLK_CTRL1,
		.bit_idx = 6,
	},
	.hw.init = &(struct clk_init_data){
		.name = "spicc2",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_spicc2_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static u32 s5_pwm_table[] = { 0, 2, 3, 6};

static const struct clk_parent_data s5_pwm_parent_data[] = {
	{ .fw_name = "xtal", },
	{ .hw = &s5_fclk_div4.hw },
	{ .hw = &s5_fclk_div3.hw },
	{ .hw = &s5_fclk_div5.hw }
};

static struct clk_regmap s5_pwm_a_sel = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_AB_CTRL,
		.mask = 0x7,
		.shift = 9,
		.table = s5_pwm_table,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_a_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = s5_pwm_parent_data,
		.num_parents = ARRAY_SIZE(s5_pwm_parent_data),
	},
};

static struct clk_regmap s5_pwm_a_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_AB_CTRL,
		.shift = 0,
		.width = 8,
		.flags = CLK_DIVIDER_ALLOW_ZERO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_a_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_pwm_a_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s5_pwm_a = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_AB_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_a",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_pwm_a_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap s5_pwm_b_sel = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_AB_CTRL,
		.mask = 0x7,
		.shift = 25,
		.table = s5_pwm_table,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_b_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = s5_pwm_parent_data,
		.num_parents = ARRAY_SIZE(s5_pwm_parent_data),
	},
};

static struct clk_regmap s5_pwm_b_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_AB_CTRL,
		.shift = 16,
		.width = 8,
		.flags = CLK_DIVIDER_ALLOW_ZERO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_b_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_pwm_b_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s5_pwm_b = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_AB_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_b",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_pwm_b_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap s5_pwm_c_sel = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_CD_CTRL,
		.mask = 0x7,
		.shift = 9,
		.table = s5_pwm_table,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_c_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = s5_pwm_parent_data,
		.num_parents = ARRAY_SIZE(s5_pwm_parent_data),
	},
};

static struct clk_regmap s5_pwm_c_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_CD_CTRL,
		.shift = 0,
		.width = 8,
		.flags = CLK_DIVIDER_ALLOW_ZERO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_c_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_pwm_c_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s5_pwm_c = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_CD_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_c",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_pwm_c_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap s5_pwm_d_sel = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_CD_CTRL,
		.mask = 0x7,
		.shift = 25,
		.table = s5_pwm_table,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_d_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = s5_pwm_parent_data,
		.num_parents = ARRAY_SIZE(s5_pwm_parent_data),
	},
};

static struct clk_regmap s5_pwm_d_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_CD_CTRL,
		.shift = 16,
		.width = 8,
		.flags = CLK_DIVIDER_ALLOW_ZERO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_d_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_pwm_d_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s5_pwm_d = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_CD_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_d",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_pwm_d_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap s5_pwm_e_sel = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_EF_CTRL,
		.mask = 0x7,
		.shift = 9,
		.table = s5_pwm_table,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_e_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = s5_pwm_parent_data,
		.num_parents = ARRAY_SIZE(s5_pwm_parent_data),
	},
};

static struct clk_regmap s5_pwm_e_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_EF_CTRL,
		.shift = 0,
		.width = 8,
		.flags = CLK_DIVIDER_ALLOW_ZERO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_e_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_pwm_e_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s5_pwm_e = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_EF_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_e",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_pwm_e_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap s5_pwm_f_sel = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_EF_CTRL,
		.mask = 0x7,
		.shift = 25,
		.table = s5_pwm_table,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_f_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = s5_pwm_parent_data,
		.num_parents = ARRAY_SIZE(s5_pwm_parent_data),
	},
};

static struct clk_regmap s5_pwm_f_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_EF_CTRL,
		.shift = 16,
		.width = 8,
		.flags = CLK_DIVIDER_ALLOW_ZERO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_f_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_pwm_f_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s5_pwm_f = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_EF_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_f",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_pwm_f_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap s5_pwm_g_sel = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_GH_CTRL,
		.mask = 0x7,
		.shift = 9,
		.table = s5_pwm_table,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_g_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = s5_pwm_parent_data,
		.num_parents = ARRAY_SIZE(s5_pwm_parent_data),
	},
};

static struct clk_regmap s5_pwm_g_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_GH_CTRL,
		.shift = 0,
		.width = 8,
		.flags = CLK_DIVIDER_ALLOW_ZERO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_g_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_pwm_g_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s5_pwm_g = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_GH_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_g",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_pwm_g_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap s5_pwm_h_sel = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_GH_CTRL,
		.mask = 0x7,
		.shift = 25,
		.table = s5_pwm_table,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_h_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = s5_pwm_parent_data,
		.num_parents = ARRAY_SIZE(s5_pwm_parent_data),
	},
};

static struct clk_regmap s5_pwm_h_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_GH_CTRL,
		.shift = 16,
		.width = 8,
		.flags = CLK_DIVIDER_ALLOW_ZERO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_h_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_pwm_h_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s5_pwm_h = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_GH_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_h",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_pwm_h_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap s5_pwm_i_sel = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_IJ_CTRL,
		.mask = 0x7,
		.shift = 9,
		.table = s5_pwm_table,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_i_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = s5_pwm_parent_data,
		.num_parents = ARRAY_SIZE(s5_pwm_parent_data),
	},
};

static struct clk_regmap s5_pwm_i_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_IJ_CTRL,
		.shift = 0,
		.width = 8,
		.flags = CLK_DIVIDER_ALLOW_ZERO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_i_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_pwm_i_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s5_pwm_i = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_IJ_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_i",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_pwm_i_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap s5_pwm_j_sel = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_IJ_CTRL,
		.mask = 0x7,
		.shift = 25,
		.table = s5_pwm_table,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_j_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = s5_pwm_parent_data,
		.num_parents = ARRAY_SIZE(s5_pwm_parent_data),
	},
};

static struct clk_regmap s5_pwm_j_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_IJ_CTRL,
		.shift = 16,
		.width = 8,
		.flags = CLK_DIVIDER_ALLOW_ZERO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_j_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_pwm_j_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s5_pwm_j = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_IJ_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_j",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_pwm_j_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap s5_saradc_sel = {
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
			{ .hw = &s5_sys_clk.hw },
			{ .hw = &s5_fclk_div5.hw },
		},
		.num_parents = 3,
	},
};

static struct clk_regmap s5_saradc_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_SAR_CLK_CTRL0,
		.shift = 0,
		.width = 8,
		.flags = CLK_DIVIDER_ALLOW_ZERO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "saradc_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_saradc_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s5_saradc = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_SAR_CLK_CTRL0,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "saradc",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_saradc_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/* gen clk */
static u32 s5_gen_clk_mux_table[] = { 0, 1, 4, 5, 6, 7, 8, 9, 10, 11, 13, 14,
				19, 20, 21, 22, 23, 24 };

static const struct clk_parent_data s5_gen_clk_parent_data[] = {
	{ .fw_name = "xtal", },
	{ .hw = &s5_rtc_clk.hw },
	{ .hw = &s5_sys2_pll.hw },
	{ .hw = &s5_gp0_pll.hw },
	{ .hw = &s5_gp1_pll.hw },
	{ .hw = &s5_hifi_pll.hw },
	{ .hw = &s5_pcie_pll.hw },
	{ .hw = &s5_pcie1_pll.hw },
	{ .hw = &s5_gp2_pll.hw },
	{ .hw = &s5_fpll_tmds.hw },
	{ .hw = &s5_nna_pll.hw },
	{ .hw = &s5_hifi1_pll.hw },
	{ .hw = &s5_fclk_div2.hw },
	{ .hw = &s5_fclk_div2p5.hw },
	{ .hw = &s5_fclk_div3.hw },
	{ .hw = &s5_fclk_div4.hw },
	{ .hw = &s5_fclk_div5.hw },
	{ .hw = &s5_fclk_div7.hw },
};

static struct clk_regmap s5_gen_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_GEN_CLK_CTRL,
		.mask = 0x1f,
		.shift = 12,
		.table = s5_gen_clk_mux_table,
	},
	.hw.init = &(struct clk_init_data){
		.name = "gen_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = s5_gen_clk_parent_data,
		.num_parents = ARRAY_SIZE(s5_gen_clk_parent_data),
	},
};

static struct clk_regmap s5_gen_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_GEN_CLK_CTRL,
		.shift = 0,
		.width = 11,
		.flags = CLK_DIVIDER_ALLOW_ZERO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "gen_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_gen_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s5_gen = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_GEN_CLK_CTRL,
		.bit_idx = 11,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "gen",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_gen_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s5_eth_rmii_sel = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_ETH_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "eth_rmii_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_fclk_div2.hw,
			&s5_fclk_div4.hw,
		},
		.num_parents = 2,
	},
};

static struct clk_regmap s5_eth_rmii_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_ETH_CLK_CTRL,
		.shift = 0,
		.width = 7,
		.flags = CLK_DIVIDER_ALLOW_ZERO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "eth_rmii_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_eth_rmii_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap s5_eth_rmii = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_ETH_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "eth_rmii",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_eth_rmii_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static struct clk_fixed_factor s5_eth_div8 = {
	.mult = 1,
	.div = 8,
	.hw.init = &(struct clk_init_data){
		.name = "eth_div8",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) { &s5_fclk_div2.hw },
		.num_parents = 1,
	},
};

static struct clk_regmap s5_eth_125m = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_ETH_CLK_CTRL,
		.bit_idx = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "eth_125m",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_eth_div8.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap s5_ts_clk_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_TS_CLK_CTRL,
		.shift = 0,
		.width = 8,
		.flags = CLK_DIVIDER_ALLOW_ZERO,
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

static struct clk_regmap s5_ts_clk = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_TS_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "ts_clk",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_ts_clk_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static u32 s5_usb_clk_table[] = { 0, 1, 2, 3 };

static const struct clk_hw *s5_usb_clk_parent_hws[] = {
	&s5_fclk_div4.hw,
	&s5_fclk_div3.hw,
	&s5_fclk_div5.hw,
	&s5_fclk_div2.hw,
};

static struct clk_regmap s5_usb_250m_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_USB_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
		.table = s5_usb_clk_table,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "usb_250m_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = s5_usb_clk_parent_hws,
		.num_parents = ARRAY_SIZE(s5_usb_clk_parent_hws),
	},
};

static struct clk_regmap s5_usb_250m_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_USB_CLK_CTRL,
		.shift = 0,
		.width = 7,
		.flags = CLK_DIVIDER_ALLOW_ZERO,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "usb_250m_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_usb_250m_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s5_usb_250m = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_USB_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "usb_250m",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_usb_250m_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s5_pcie_400m_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_USB_CLK_CTRL,
		.mask = 0x7,
		.shift = 25,
		.table = s5_usb_clk_table,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pcie_400m_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = s5_usb_clk_parent_hws,
		.num_parents = ARRAY_SIZE(s5_usb_clk_parent_hws),
	},
};

static struct clk_regmap s5_pcie_400m_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_USB_CLK_CTRL,
		.shift = 16,
		.width = 7,
		.flags = CLK_DIVIDER_ALLOW_ZERO,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pcie_400m_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_pcie_400m_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s5_pcie_400m = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_USB_CLK_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pcie_400m",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_pcie_400m_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s5_pcie_clk_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_USB_CLK_CTRL1,
		.mask = 0x7,
		.shift = 9,
		.table = s5_usb_clk_table,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pcie_clk_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = s5_usb_clk_parent_hws,
		.num_parents = ARRAY_SIZE(s5_usb_clk_parent_hws),
	},
};

static struct clk_regmap s5_pcie_clk_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_USB_CLK_CTRL1,
		.shift = 0,
		.width = 7,
		.flags = CLK_DIVIDER_ALLOW_ZERO,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pcie_clk_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_pcie_clk_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s5_pcie_clk = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_USB_CLK_CTRL1,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pcie_clk",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_pcie_clk_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s5_pcie_tl_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_USB_CLK_CTRL1,
		.mask = 0x7,
		.shift = 25,
		.table = s5_usb_clk_table,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pcie_tl_clk_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = s5_usb_clk_parent_hws,
		.num_parents = ARRAY_SIZE(s5_usb_clk_parent_hws),
	},
};

static struct clk_regmap s5_pcie_tl_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_USB_CLK_CTRL1,
		.shift = 16,
		.width = 7,
		.flags = CLK_DIVIDER_ALLOW_ZERO,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pcie_tl_clk_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_pcie_tl_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s5_pcie_tl = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_USB_CLK_CTRL1,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pcie_tl_clk",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_pcie_tl_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static u32 s5_nna_table[] = { 0, 2, 3, 5 };

static const struct clk_parent_data s5_nna_clk_parent_data[] = {
	{ .fw_name = "xtal", },
	{ .hw = &s5_fclk_div3.hw },
	{ .hw = &s5_nna_pll.hw },
	{ .hw = &s5_fclk_div4.hw },
};

static struct clk_regmap s5_nna0_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_NNA_CLK_CTRL0,
		.mask = 0x7,
		.shift = 9,
		.table = s5_nna_table,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "nna0_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = s5_nna_clk_parent_data,
		.num_parents = ARRAY_SIZE(s5_nna_clk_parent_data),
	},
};

static struct clk_regmap s5_nna0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_NNA_CLK_CTRL0,
		.shift = 0,
		.width = 7,
		.flags = CLK_DIVIDER_ALLOW_ZERO,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "nna0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_nna0_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap s5_nna0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_NNA_CLK_CTRL0,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "nna0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_nna0_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s5_nna1_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_NNA_CLK_CTRL0,
		.mask = 0x7,
		.shift = 25,
		.table = s5_nna_table,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "nna1_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = s5_nna_clk_parent_data,
		.num_parents = ARRAY_SIZE(s5_nna_clk_parent_data),
	},
};

static struct clk_regmap s5_nna1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_NNA_CLK_CTRL0,
		.shift = 16,
		.width = 7,
		.flags = CLK_DIVIDER_ALLOW_ZERO,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "nna1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_nna1_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap s5_nna1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_NNA_CLK_CTRL0,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "nna1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_nna1_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s5_nna_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_NNA_CLK_CTRL0,
		.mask = 0x1,
		.shift = 31,
		.flags = CLK_MUX_ROUND_CLOSEST,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "nna_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = (const struct clk_parent_data []) {
			{ .hw = &s5_nna0.hw, },
			{ .hw = &s5_nna1.hw, }
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT | CLK_SET_RATE_NO_REPARENT,
	},
};

static struct clk_regmap s5_nna = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_NNA_CLK_CTRL0,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "nna",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_nna_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s5_cdac_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_CDAC_CLK_CTRL,
		.mask = 0x1,
		.shift = 16,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "cdac_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = (const struct clk_parent_data []) {
			{ .fw_name = "xtal", },
			{ .hw = &s5_fclk_div5.hw },
		},
		.num_parents = 2,
	},
};

static struct clk_regmap s5_cdac_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_CDAC_CLK_CTRL,
		.shift = 0,
		.width = 16,
		.flags = CLK_DIVIDER_ALLOW_ZERO,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "cdac_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_cdac_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s5_cdac = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_CDAC_CLK_CTRL,
		.bit_idx = 20,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cdac",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_cdac_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s5_sc_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_SC_CLK_CTRL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sc_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = (const struct clk_parent_data []) {
			{ .hw = &s5_fclk_div2.hw },
			{ .hw = &s5_fclk_div3.hw },
			{ .hw = &s5_fclk_div5.hw },
			{ .fw_name = "xtal", }
		},
		.num_parents = 4,
	},
};

static struct clk_regmap s5_sc_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_SC_CLK_CTRL,
		.shift = 0,
		.width = 8,
		.flags = CLK_DIVIDER_ALLOW_ZERO,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sc_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_sc_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s5_sc = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_SC_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sc",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_sc_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/* sys clk = fclk_div4 / 3, and it is set in firmware */
static u32 s5_mux_table_vpuclk_sel[] = { 0, 1, 2, 3 };

static const struct clk_parent_data s5_vpuclk_parent_data[] = {
	{ .hw = &s5_fclk_div3.hw },
	{ .hw = &s5_fclk_div4.hw },
	{ .hw = &s5_fclk_div5.hw },
	{ .hw = &s5_fclk_div7.hw },
};

static struct clk_regmap s5_vpuclk1_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VPU_CLK_CTRL,
		.mask = 0x7,
		.shift = 25,
		.table = s5_mux_table_vpuclk_sel,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vpuclk1_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = s5_vpuclk_parent_data,
		.num_parents = ARRAY_SIZE(s5_vpuclk_parent_data),
	},
};

static struct clk_regmap s5_vpuclk1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_VPU_CLK_CTRL,
		.shift = 16,
		.width = 7,
		.flags = CLK_DIVIDER_ALLOW_ZERO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vpuclk1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_vpuclk1_sel.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap s5_vpuclk1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VPU_CLK_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpuclk1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_vpuclk1_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap s5_vpuclk0_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VPU_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
		.table = s5_mux_table_vpuclk_sel,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vpuclk0_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = s5_vpuclk_parent_data,
		.num_parents = ARRAY_SIZE(s5_vpuclk_parent_data),
	},
};

static struct clk_regmap s5_vpuclk0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_VPU_CLK_CTRL,
		.shift = 0,
		.width = 7,
		.flags = CLK_DIVIDER_ALLOW_ZERO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vpuclk0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_vpuclk0_sel.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap s5_vpuclk0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VPU_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpuclk0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_vpuclk0_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap s5_vpuclk_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VPU_CLK_CTRL,
		.mask = 0x1,
		.shift = 31,
		.flags = CLK_MUX_ROUND_CLOSEST,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vpu_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_vpuclk0.hw,
			&s5_vpuclk1.hw
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT | CLK_SET_RATE_NO_REPARENT,
	},
};

static struct clk_regmap s5_vpuclk = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VPU_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_vpuclk_sel.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static u32 s5_mux_table_vpuclkb_tmp_sel[] = { 0, 1, 2, 3 };

static const struct clk_parent_data s5_vpuclkb_tmp_parent_data[] = {
	{ .hw = &s5_vpuclk.hw },
	{ .hw = &s5_fclk_div4.hw },
	{ .hw = &s5_fclk_div5.hw },
	{ .hw = &s5_fclk_div7.hw },
};

static struct clk_regmap s5_vpuclkb_tmp_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VPU_CLKB_CTRL,
		.mask = 0x3,
		.shift = 20,
		.table = s5_mux_table_vpuclkb_tmp_sel,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vpub_tmp_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = s5_vpuclkb_tmp_parent_data,
		.num_parents = ARRAY_SIZE(s5_vpuclkb_tmp_parent_data),
	},
};

static struct clk_regmap s5_vpuclkb_tmp_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_VPU_CLKB_CTRL,
		.shift = 16,
		.width = 4,
		.flags = CLK_DIVIDER_ALLOW_ZERO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vpub_tmp_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_vpuclkb_tmp_sel.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap s5_vpuclkb_tmp = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VPU_CLKB_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpub_tmp",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_vpuclkb_tmp_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap s5_vpuclkb_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_VPU_CLKB_CTRL,
		.shift = 0,
		.width = 8,
		.flags = CLK_DIVIDER_ALLOW_ZERO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vpub_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_vpuclkb_tmp.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap s5_vpuclkb = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VPU_CLKB_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpub",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_vpuclkb_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static u32 mux_table_vdin_meas_sel[] = { 0, 1, 2, 3 };

static const struct clk_parent_data vdin_meas_parent_data[] = {
	{ .fw_name = "xtal", },
	{ .hw = &s5_fclk_div4.hw },
	{ .hw = &s5_fclk_div3.hw },
	{ .hw = &s5_fclk_div5.hw },
};

static struct clk_regmap s5_vdin_meas_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VDIN_MEAS_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
		.table = mux_table_vdin_meas_sel,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vdin_meas_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = vdin_meas_parent_data,
		.num_parents = ARRAY_SIZE(vdin_meas_parent_data),
	},
};

static struct clk_regmap s5_vdin_meas_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_VDIN_MEAS_CLK_CTRL,
		.shift = 0,
		.width = 7,
		.flags = CLK_DIVIDER_ALLOW_ZERO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vdin_meas_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_vdin_meas_sel.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap s5_vdin_meas = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VDIN_MEAS_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vdin_meas",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_vdin_meas_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static u32 mux_table_vid_lock_sel[] = { 0, 3 };
static const struct clk_parent_data vid_lock_parent_data[] = {
	{ .fw_name = "xtal", },
	{ .hw = &s5_nna_pll.hw },
};

static struct clk_regmap s5_vid_lock_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VID_LOCK_CLK_CTRL,
		.mask = 0x3,
		.shift = 8,
		.table = mux_table_vid_lock_sel,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vid_lock_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = vid_lock_parent_data,
		.num_parents = ARRAY_SIZE(vid_lock_parent_data),
	},
};

static struct clk_regmap s5_vid_lock_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_VID_LOCK_CLK_CTRL,
		.shift = 0,
		.width = 7,
		.flags = CLK_DIVIDER_ALLOW_ZERO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vid_lock_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_vid_lock_sel.hw,
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap s5_vid_lock = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VID_LOCK_CLK_CTRL,
		.bit_idx = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vid_lock",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_vid_lock_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static u32 mux_table_cmpr_sel[] = { 0, 1, 2, 3, 4 };
static const struct clk_parent_data cmpr_parent_data[] = {
	{ .fw_name = "xtal", },
	{ .hw = &s5_fclk_div4.hw },
	{ .hw = &s5_fclk_div3.hw },
	{ .hw = &s5_fclk_div5.hw },
	{ .hw = &s5_fclk_div7.hw },
};

static struct clk_regmap s5_cmpr_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_CMPR_CLK_CTRL,
		.mask = 0x7,
		.shift = 25,
		.table = mux_table_cmpr_sel,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cmpr_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = cmpr_parent_data,
		.num_parents = ARRAY_SIZE(cmpr_parent_data),
	},
};

static struct clk_regmap s5_cmpr_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_CMPR_CLK_CTRL,
		.shift = 16,
		.width = 7,
		.flags = CLK_DIVIDER_ALLOW_ZERO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cmpr_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_cmpr_sel.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap s5_cmpr = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_CMPR_CLK_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "cmpr",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_cmpr_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static u32 mux_table_mali_sel[] = { 0, 1, 3, 4, 5, 6 };
static const struct clk_parent_data mali_parent_data[] = {
	{ .fw_name = "xtal", },
	{ .hw = &s5_sys2_pll.hw },
	{ .hw = &s5_fclk_div2p5.hw },
	{ .hw = &s5_fclk_div3.hw },
	{ .hw = &s5_fclk_div4.hw },
	{ .hw = &s5_fclk_div5.hw },
};

static struct clk_regmap s5_mali0_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_MALI_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
		.table = mux_table_mali_sel,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mali0_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = mali_parent_data,
		.num_parents = ARRAY_SIZE(mali_parent_data),
	},
};

static struct clk_regmap s5_mali0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_MALI_CLK_CTRL,
		.shift = 0,
		.width = 7,
		.flags = CLK_DIVIDER_ALLOW_ZERO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mali0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_mali0_sel.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap s5_mali0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_MALI_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mali0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_mali0_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s5_mali1_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_MALI_CLK_CTRL,
		.mask = 0x7,
		.shift = 25,
		.table = mux_table_mali_sel,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mali1_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = mali_parent_data,
		.num_parents = ARRAY_SIZE(mali_parent_data),
	},
};

static struct clk_regmap s5_mali1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_MALI_CLK_CTRL,
		.shift = 16,
		.width = 7,
		.flags = CLK_DIVIDER_ALLOW_ZERO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mali1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_mali1_sel.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap s5_mali1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_MALI_CLK_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mali1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_mali1_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s5_mali_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_MALI_CLK_CTRL,
		.mask = 0x1,
		.shift = 31,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mali_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_mali0.hw,
			&s5_mali1.hw,
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT | CLK_SET_RATE_NO_REPARENT,
	},
};

static struct clk_regmap s5_mali = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_MALI_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mali",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_mali_sel.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static u32 mux_table_dec_sel[] = { 0, 1, 2, 3, 4 };
static const struct clk_parent_data dec_parent_data[] = {
	{ .hw = &s5_fclk_div2p5.hw },
	{ .hw = &s5_fclk_div3.hw },
	{ .hw = &s5_fclk_div4.hw },
	{ .hw = &s5_fclk_div5.hw },
	{ .hw = &s5_fclk_div7.hw },
};

static struct clk_regmap s5_vdec0_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VDEC_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
		.table = mux_table_dec_sel,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vdec0_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = dec_parent_data,
		.num_parents = ARRAY_SIZE(dec_parent_data),
	},
};

static struct clk_regmap s5_vdec0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_VDEC_CLK_CTRL,
		.shift = 0,
		.width = 7,
		.flags = CLK_DIVIDER_ALLOW_ZERO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vdec0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_vdec0_sel.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap s5_vdec0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VDEC_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vdec0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_vdec0_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s5_vdec1_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VDEC3_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
		.table = mux_table_dec_sel,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vdec1_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = dec_parent_data,
		.num_parents = ARRAY_SIZE(dec_parent_data),
	},
};

static struct clk_regmap s5_vdec1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_VDEC3_CLK_CTRL,
		.shift = 0,
		.width = 7,
		.flags = CLK_DIVIDER_ALLOW_ZERO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vdec1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_vdec1_sel.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap s5_vdec1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VDEC3_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vdec1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_vdec1_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s5_vdec_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VDEC3_CLK_CTRL,
		.mask = 0x1,
		.shift = 15,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vdec_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_vdec0.hw,
			&s5_vdec1.hw,
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT | CLK_SET_RATE_NO_REPARENT,
	},
};

static struct clk_regmap s5_vdec = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VDEC_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vdec",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_vdec_sel.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s5_hcodec0_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VDEC_CLK_CTRL,
		.mask = 0x7,
		.shift = 25,
		.table = mux_table_dec_sel,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hcodec0_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = dec_parent_data,
		.num_parents = ARRAY_SIZE(dec_parent_data),
	},
};

static struct clk_regmap s5_hcodec0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_VDEC_CLK_CTRL,
		.shift = 16,
		.width = 7,
		.flags = CLK_DIVIDER_ALLOW_ZERO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hcodec0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_hcodec0_sel.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap s5_hcodec0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VDEC_CLK_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hcodec0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_hcodec0_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s5_hcodec1_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VDEC3_CLK_CTRL,
		.mask = 0x7,
		.shift = 25,
		.table = mux_table_dec_sel,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hcodec1_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = dec_parent_data,
		.num_parents = ARRAY_SIZE(dec_parent_data),
	},
};

static struct clk_regmap s5_hcodec1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_VDEC3_CLK_CTRL,
		.shift = 16,
		.width = 7,
		.flags = CLK_DIVIDER_ALLOW_ZERO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hcodec1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_hcodec1_sel.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap s5_hcodec1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VDEC3_CLK_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hcodec1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_hcodec1_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s5_hcodec_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VDEC3_CLK_CTRL,
		.mask = 0x1,
		.shift = 31,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hcodec_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_hcodec0.hw,
			&s5_hcodec1.hw,
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT | CLK_SET_RATE_NO_REPARENT,
	},
};

static struct clk_regmap s5_hcodec = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VDEC_CLK_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hcodec",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_hcodec_sel.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s5_hevc0_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VDEC2_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
		.table = mux_table_dec_sel,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hevc0_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = dec_parent_data,
		.num_parents = ARRAY_SIZE(dec_parent_data),
	},
};

static struct clk_regmap s5_hevc0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_VDEC2_CLK_CTRL,
		.shift = 0,
		.width = 7,
		.flags = CLK_DIVIDER_ALLOW_ZERO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hevc0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_hevc0_sel.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap s5_hevc0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VDEC2_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hevc0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_hevc0_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s5_hevc1_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VDEC4_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
		.table = mux_table_dec_sel,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hevc1_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = dec_parent_data,
		.num_parents = ARRAY_SIZE(dec_parent_data),
	},
};

static struct clk_regmap s5_hevc1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_VDEC4_CLK_CTRL,
		.shift = 0,
		.width = 7,
		.flags = CLK_DIVIDER_ALLOW_ZERO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hevc1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_hevc1_sel.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap s5_hevc1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VDEC4_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hevc1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_hevc1_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s5_hevc_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VDEC4_CLK_CTRL,
		.mask = 0x1,
		.shift = 15,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hevc_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_hevc0.hw,
			&s5_hevc1.hw,
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT | CLK_SET_RATE_NO_REPARENT,
	},
};

static struct clk_regmap s5_hevc = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VDEC2_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hevc",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_hevc_sel.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static u32 mux_table_vc9000e_sel[] = { 0, 1, 2, 3, 4 };
static const struct clk_parent_data vc9000e_parent_data[] = {
	{ .fw_name = "xtal", },
	{ .hw = &s5_fclk_div4.hw },
	{ .hw = &s5_fclk_div3.hw },
	{ .hw = &s5_fclk_div5.hw },
	{ .hw = &s5_fclk_div7.hw },
};

static struct clk_regmap s5_vc9000e_axi_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VC9000E_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
		.table = mux_table_vc9000e_sel,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vc9000e_axi_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = vc9000e_parent_data,
		.num_parents = ARRAY_SIZE(vc9000e_parent_data),
	},
};

static struct clk_regmap s5_vc9000e_axi_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_VC9000E_CLK_CTRL,
		.shift = 0,
		.width = 7,
		.flags = CLK_DIVIDER_ALLOW_ZERO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vc9000e_axi_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_vc9000e_axi_sel.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap s5_vc9000e_axi = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VC9000E_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vc9000e_axi",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_vc9000e_axi_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s5_vc9000e_core_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VC9000E_CLK_CTRL,
		.mask = 0x7,
		.shift = 25,
		.table = mux_table_vc9000e_sel,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vc9000e_core_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = vc9000e_parent_data,
		.num_parents = ARRAY_SIZE(vc9000e_parent_data),
	},
};

static struct clk_regmap s5_vc9000e_core_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_VC9000E_CLK_CTRL,
		.shift = 16,
		.width = 7,
		.flags = CLK_DIVIDER_ALLOW_ZERO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vc9000e_core_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_vc9000e_core_sel.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap s5_vc9000e_core = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VC9000E_CLK_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vc9000e_core",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_vc9000e_core_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static u32 mux_table_hdmitx_sel[] = { 0, 1, 2, 3 };
static const struct clk_parent_data hdmitx_parent_data[] = {
	{ .fw_name = "xtal", },
	{ .hw = &s5_fclk_div4.hw },
	{ .hw = &s5_fclk_div3.hw },
	{ .hw = &s5_fclk_div5.hw },
};

static struct clk_regmap s5_hdmitx_sys_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_HDMI_CLK_CTRL,
		.mask = 0x3,
		.shift = 9,
		.table = mux_table_hdmitx_sel,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmitx_sys_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = hdmitx_parent_data,
		.num_parents = ARRAY_SIZE(hdmitx_parent_data),
	},
};

static struct clk_regmap s5_hdmitx_sys_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_HDMI_CLK_CTRL,
		.shift = 0,
		.width = 7,
		.flags = CLK_DIVIDER_ALLOW_ZERO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmitx_sys_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_hdmitx_sys_sel.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap s5_hdmitx_sys = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_HDMI_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmitx_sys",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_hdmitx_sys_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap s5_hdmitx_prif_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_HTX_CLK_CTRL0,
		.mask = 0x3,
		.shift = 9,
		.table = mux_table_hdmitx_sel,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmitx_prif_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = hdmitx_parent_data,
		.num_parents = ARRAY_SIZE(hdmitx_parent_data),
	},
};

static struct clk_regmap s5_hdmitx_prif_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_HTX_CLK_CTRL0,
		.shift = 0,
		.width = 7,
		.flags = CLK_DIVIDER_ALLOW_ZERO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmitx_prif_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_hdmitx_prif_sel.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap s5_hdmitx_prif = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_HTX_CLK_CTRL0,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmitx_prif",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_hdmitx_prif_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap s5_hdmitx_200m_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_HTX_CLK_CTRL0,
		.mask = 0x3,
		.shift = 25,
		.table = mux_table_hdmitx_sel,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmitx_200m_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = hdmitx_parent_data,
		.num_parents = ARRAY_SIZE(hdmitx_parent_data),
	},
};

static struct clk_regmap s5_hdmitx_200m_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_HTX_CLK_CTRL0,
		.shift = 16,
		.width = 7,
		.flags = CLK_DIVIDER_ALLOW_ZERO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmitx_200m_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_hdmitx_200m_sel.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap s5_hdmitx_200m = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_HTX_CLK_CTRL0,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmitx_200m",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_hdmitx_200m_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap s5_hdmitx_aud_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_HTX_CLK_CTRL1,
		.mask = 0x3,
		.shift = 9,
		.table = mux_table_vc9000e_sel,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmitx_aud_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = hdmitx_parent_data,
		.num_parents = ARRAY_SIZE(hdmitx_parent_data),
	},
};

static struct clk_regmap s5_hdmitx_aud_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_HTX_CLK_CTRL1,
		.shift = 0,
		.width = 7,
		.flags = CLK_DIVIDER_ALLOW_ZERO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmitx_aud_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_hdmitx_aud_sel.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap s5_hdmitx_aud = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_HTX_CLK_CTRL1,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmitx_aud",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_hdmitx_aud_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

#define MESON_S5_SYS_GATE(_name, _reg, _bit)				\
struct clk_regmap s5_ ## _name = {						\
	.data = &(struct clk_regmap_gate_data) {			\
		.offset = (_reg),					\
		.bit_idx = (_bit),					\
	},								\
	.hw.init = &(struct clk_init_data) {				\
		.name = #_name,						\
		.ops = &clk_regmap_gate_ops,				\
		.parent_hws = (const struct clk_hw *[]) {		\
			&s5_sys_clk.hw					\
		},							\
		.num_parents = 1,					\
		.flags = CLK_IGNORE_UNUSED,				\
	},								\
}

/*CLKCTRL_SYS_CLK_EN0_REG0*/
static MESON_S5_SYS_GATE(sys_clk_ddr,		CLKCTRL_SYS_CLK_EN0_REG0, 0);
static MESON_S5_SYS_GATE(sys_clk_ethphy,		CLKCTRL_SYS_CLK_EN0_REG0, 4);
static MESON_S5_SYS_GATE(sys_clk_gpu,		CLKCTRL_SYS_CLK_EN0_REG0, 6);
static MESON_S5_SYS_GATE(sys_clk_aocpu,		CLKCTRL_SYS_CLK_EN0_REG0, 13);
static MESON_S5_SYS_GATE(sys_clk_aucpu,		CLKCTRL_SYS_CLK_EN0_REG0, 14);
static MESON_S5_SYS_GATE(sys_clk_dewarpc,		CLKCTRL_SYS_CLK_EN0_REG0, 16);
static MESON_S5_SYS_GATE(sys_clk_dewarpb,		CLKCTRL_SYS_CLK_EN0_REG0, 17);
static MESON_S5_SYS_GATE(sys_clk_dewarpa,		CLKCTRL_SYS_CLK_EN0_REG0, 18);
static MESON_S5_SYS_GATE(sys_clk_ampipe_nand,	CLKCTRL_SYS_CLK_EN0_REG0, 19);
static MESON_S5_SYS_GATE(sys_clk_ampipe_eth,		CLKCTRL_SYS_CLK_EN0_REG0, 20);
static MESON_S5_SYS_GATE(sys_clk_am2axi0,		CLKCTRL_SYS_CLK_EN0_REG0, 21);
static MESON_S5_SYS_GATE(sys_clk_sdemmca,		CLKCTRL_SYS_CLK_EN0_REG0, 24);
static MESON_S5_SYS_GATE(sys_clk_sdemmcb,		CLKCTRL_SYS_CLK_EN0_REG0, 25);
static MESON_S5_SYS_GATE(sys_clk_sdemmcc,		CLKCTRL_SYS_CLK_EN0_REG0, 26);
static MESON_S5_SYS_GATE(sys_clk_vc9000e,		CLKCTRL_SYS_CLK_EN0_REG0, 27);
static MESON_S5_SYS_GATE(sys_clk_acodec,		CLKCTRL_SYS_CLK_EN0_REG0, 28);
static MESON_S5_SYS_GATE(sys_clk_spifc,		CLKCTRL_SYS_CLK_EN0_REG0, 29);
static MESON_S5_SYS_GATE(sys_clk_msr_clk,		CLKCTRL_SYS_CLK_EN0_REG0, 30);
static MESON_S5_SYS_GATE(sys_clk_ir_ctrl,		CLKCTRL_SYS_CLK_EN0_REG0, 31);

/*CLKCTRL_SYS_CLK_EN0_REG1*/
static MESON_S5_SYS_GATE(sys_clk_audio,		CLKCTRL_SYS_CLK_EN0_REG1, 0);
static MESON_S5_SYS_GATE(sys_clk_dos,		CLKCTRL_SYS_CLK_EN0_REG1, 2);
static MESON_S5_SYS_GATE(sys_clk_eth,		CLKCTRL_SYS_CLK_EN0_REG1, 3);
static MESON_S5_SYS_GATE(sys_clk_uart_a,		CLKCTRL_SYS_CLK_EN0_REG1, 5);
static MESON_S5_SYS_GATE(sys_clk_uart_b,		CLKCTRL_SYS_CLK_EN0_REG1, 6);
static MESON_S5_SYS_GATE(sys_clk_uart_c,		CLKCTRL_SYS_CLK_EN0_REG1, 7);
static MESON_S5_SYS_GATE(sys_clk_uart_d,		CLKCTRL_SYS_CLK_EN0_REG1, 8);
static MESON_S5_SYS_GATE(sys_clk_uart_e,		CLKCTRL_SYS_CLK_EN0_REG1, 9);
static MESON_S5_SYS_GATE(sys_clk_uart_f,		CLKCTRL_SYS_CLK_EN0_REG1, 10);
static MESON_S5_SYS_GATE(sys_clk_spicc2,		CLKCTRL_SYS_CLK_EN0_REG1, 12);
static MESON_S5_SYS_GATE(sys_clk_ts_a55,		CLKCTRL_SYS_CLK_EN0_REG1, 16);
static MESON_S5_SYS_GATE(sys_clk_g2d,		CLKCTRL_SYS_CLK_EN0_REG1, 20);
static MESON_S5_SYS_GATE(sys_clk_spicc0,		CLKCTRL_SYS_CLK_EN0_REG1, 21);
static MESON_S5_SYS_GATE(sys_clk_spicc1,		CLKCTRL_SYS_CLK_EN0_REG1, 22);
static MESON_S5_SYS_GATE(sys_clk_smart_card,		CLKCTRL_SYS_CLK_EN0_REG1, 23);
static MESON_S5_SYS_GATE(sys_clk_pcie,		CLKCTRL_SYS_CLK_EN0_REG1, 24);
static MESON_S5_SYS_GATE(sys_clk_pciephy,		CLKCTRL_SYS_CLK_EN0_REG1, 25);
static MESON_S5_SYS_GATE(sys_clk_usb,		CLKCTRL_SYS_CLK_EN0_REG1, 26);
static MESON_S5_SYS_GATE(sys_clk_pcie_phy0,		CLKCTRL_SYS_CLK_EN0_REG1, 27);
static MESON_S5_SYS_GATE(sys_clk_pcie_phy1,		CLKCTRL_SYS_CLK_EN0_REG1, 28);
static MESON_S5_SYS_GATE(sys_clk_pcie_phy2,		CLKCTRL_SYS_CLK_EN0_REG1, 29);
static MESON_S5_SYS_GATE(sys_clk_i2c_m_a,		CLKCTRL_SYS_CLK_EN0_REG1, 30);
static MESON_S5_SYS_GATE(sys_clk_i2c_m_b,		CLKCTRL_SYS_CLK_EN0_REG1, 31);

/*CLKCTRL_SYS_CLK_EN0_REG2*/
static MESON_S5_SYS_GATE(sys_clk_i2c_m_c,		CLKCTRL_SYS_CLK_EN0_REG2, 0);
static MESON_S5_SYS_GATE(sys_clk_i2c_m_d,		CLKCTRL_SYS_CLK_EN0_REG2, 1);
static MESON_S5_SYS_GATE(sys_clk_i2c_m_e,		CLKCTRL_SYS_CLK_EN0_REG2, 2);
static MESON_S5_SYS_GATE(sys_clk_i2c_m_f,		CLKCTRL_SYS_CLK_EN0_REG2, 3);
static MESON_S5_SYS_GATE(sys_clk_i2c_s_a,		CLKCTRL_SYS_CLK_EN0_REG2, 8);
static MESON_S5_SYS_GATE(sys_clk_cmpr,		CLKCTRL_SYS_CLK_EN0_REG2, 10);
static MESON_S5_SYS_GATE(sys_clk_mmc_pclk,		CLKCTRL_SYS_CLK_EN0_REG2, 11);
static MESON_S5_SYS_GATE(sys_clk_hdmitx_pclk,	CLKCTRL_SYS_CLK_EN0_REG2, 16);
static MESON_S5_SYS_GATE(sys_clk_hdmi20_aes_clk,	CLKCTRL_SYS_CLK_EN0_REG2, 17);
static MESON_S5_SYS_GATE(sys_clk_pclk_sys_cpu_apb,	CLKCTRL_SYS_CLK_EN0_REG2, 19);
static MESON_S5_SYS_GATE(sys_clk_cec,		CLKCTRL_SYS_CLK_EN0_REG2, 23);
static MESON_S5_SYS_GATE(sys_clk_vpu_intr,		CLKCTRL_SYS_CLK_EN0_REG2, 25);
static MESON_S5_SYS_GATE(sys_clk_sar_adc,		CLKCTRL_SYS_CLK_EN0_REG2, 28);
static MESON_S5_SYS_GATE(sys_clk_gic,		CLKCTRL_SYS_CLK_EN0_REG2, 30);
static MESON_S5_SYS_GATE(sys_clk_ts_gpu,		CLKCTRL_SYS_CLK_EN0_REG2, 31);

/*CLKCTRL_SYS_CLK_EN0_REG3*/
static MESON_S5_SYS_GATE(sys_clk_ts_nna,		CLKCTRL_SYS_CLK_EN0_REG3, 0);
static MESON_S5_SYS_GATE(sys_clk_ts_vpu,		CLKCTRL_SYS_CLK_EN0_REG3, 1);
static MESON_S5_SYS_GATE(sys_clk_ts_dos,		CLKCTRL_SYS_CLK_EN0_REG3, 2);
static MESON_S5_SYS_GATE(sys_clk_pwm_ab,		CLKCTRL_SYS_CLK_EN0_REG3, 5);
static MESON_S5_SYS_GATE(sys_clk_pwm_cd,		CLKCTRL_SYS_CLK_EN0_REG3, 6);
static MESON_S5_SYS_GATE(sys_clk_pwm_ef,		CLKCTRL_SYS_CLK_EN0_REG3, 7);
static MESON_S5_SYS_GATE(sys_clk_pwm_gh,		CLKCTRL_SYS_CLK_EN0_REG3, 8);
static MESON_S5_SYS_GATE(sys_clk_pwm_ij,		CLKCTRL_SYS_CLK_EN0_REG3, 9);

/* Array of all clocks provided by this provider */
static struct clk_hw_onecell_data s5_hw_onecell_data = {
	.hws = {
		[CLKID_SYS_PLL_DCO]			= &s5_sys_pll_dco.hw,
		[CLKID_SYS_PLL]				= &s5_sys_pll.hw,
		[CLKID_SYS1_PLL_DCO]			= &s5_sys1_pll_dco.hw,
		[CLKID_SYS1_PLL]			= &s5_sys1_pll.hw,
		[CLKID_FIXED_PLL_DCO]			= &s5_fixed_pll_dco.hw,
		[CLKID_FIXED_PLL]			= &s5_fixed_pll.hw,
		[CLKID_FCLK_DIV2_DIV]			= &s5_fclk_div2_div.hw,
		[CLKID_FCLK_DIV2]			= &s5_fclk_div2.hw,
		[CLKID_FCLK_DIV3_DIV]			= &s5_fclk_div3_div.hw,
		[CLKID_FCLK_DIV3]			= &s5_fclk_div3.hw,
		[CLKID_FCLK_DIV4_DIV]			= &s5_fclk_div4_div.hw,
		[CLKID_FCLK_DIV4]			= &s5_fclk_div4.hw,
		[CLKID_FCLK_DIV5_DIV]			= &s5_fclk_div5_div.hw,
		[CLKID_FCLK_DIV5]			= &s5_fclk_div5.hw,
		[CLKID_FCLK_DIV7_DIV]			= &s5_fclk_div7_div.hw,
		[CLKID_FCLK_DIV7]			= &s5_fclk_div7.hw,
		[CLKID_FCLK_DIV2P5_DIV]			= &s5_fclk_div2p5_div.hw,
		[CLKID_FCLK_DIV2P5]			= &s5_fclk_div2p5.hw,
		[CLKID_FCLK_CLK50M_DIV]			= &s5_fclk_clk50m_div.hw,
		[CLKID_FCLK_CLK50M]			= &s5_fclk_clk50m.hw,
		[CLKID_GP0_PLL_DCO]			= &s5_gp0_pll_dco.hw,
		[CLKID_GP0_PLL]				= &s5_gp0_pll.hw,
		[CLKID_GP1_PLL_DCO]			= &s5_gp1_pll_dco.hw,
		[CLKID_GP1_PLL]				= &s5_gp1_pll.hw,
		[CLKID_CPU_DYN_CLK]			= &s5_cpu_dyn_clk.hw,
		[CLKID_CPU_CLK]				= &s5_cpu_clk.hw,
		[CLKID_HIFI_PLL_DCO]			= &s5_hifi_pll_dco.hw,
		[CLKID_HIFI_PLL]			= &s5_hifi_pll.hw,
		[CLKID_PCIE_PLL_DCO]			= &s5_pcie_pll_dco.hw,
		[CLKID_PCIE_PLL_DCO_DIV2]		= &s5_pcie_pll_dco_div2.hw,
		[CLKID_PCIE_PLL_OD]			= &s5_pcie_pll_od.hw,
		[CLKID_PCIE_PLL]			= &s5_pcie_pll.hw,
		[CLKID_PCIE_BGP]			= &s5_pcie_bgp.hw,
		[CLKID_PCIE_HCSL_OUT]		= &s5_pcie_hcsl_out.hw,
		[CLKID_PCIE_HCSL_PAD]		= &s5_pcie_hcsl_pad.hw,
		[CLKID_PCIE_HCSL_IN_PAD]		= &s5_pcie_hcsl_in_pad.hw,
		[CLKID_PCIE_CLK_IN]			= &s5_pcie_clk_in_sel.hw,
		[CLKID_PCIE1_PLL_DCO]			= &s5_pcie1_pll_dco.hw,
		[CLKID_PCIE1_PLL_DCO_DIV2]		= &s5_pcie1_pll_dco_div2.hw,
		[CLKID_PCIE1_PLL_OD]			= &s5_pcie1_pll_od.hw,
		[CLKID_PCIE1_PLL]			= &s5_pcie1_pll.hw,
		[CLKID_PCIE1_BGP]			= &s5_pcie1_bgp.hw,
		[CLKID_PCIE1_HCSL_OUT]		= &s5_pcie1_hcsl_out.hw,
		[CLKID_PCIE1_HCSL_PAD]		= &s5_pcie1_hcsl_pad.hw,
		[CLKID_PCIE1_HCSL_IN_PAD]		= &s5_pcie1_hcsl_in_pad.hw,
		[CLKID_PCIE1_CLK_IN]			= &s5_pcie1_clk_in_sel.hw,
		[CLKID_SYS2_PLL_DCO]			= &s5_sys2_pll_dco.hw,
		[CLKID_SYS2_PLL]			= &s5_sys2_pll.hw,
		[CLKID_A76_DYN_CLK]			= &s5_a76_dyn_clk.hw,
		[CLKID_A76_CLK]				= &s5_a76_clk.hw,
		[CLKID_DSU_DYN_CLK]			= &s5_dsu_dyn_clk.hw,
		[CLKID_DSU_CLK]				= &s5_dsu_clk.hw,
		[CLKID_DSU_FINAL_CLK]			= &s5_dsu_final_clk.hw,
		[CLKID_GP2_PLL_DCO]			= &s5_gp2_pll_dco.hw,
		[CLKID_GP2_PLL]				= &s5_gp2_pll.hw,
		[CLKID_NNA_PLL_DCO]			= &s5_nna_pll_dco.hw,
		[CLKID_NNA_PLL]				= &s5_nna_pll.hw,
		[CLKID_NNA_PLL_AUDIO]				= &s5_nna_pll_audio.hw,
		[CLKID_HIFI1_PLL_DCO]			= &s5_hifi1_pll_dco.hw,
		[CLKID_HIFI1_PLL]			= &s5_hifi1_pll.hw,
		[CLKID_FDLE_PLL_DCO]			= &s5_fpll_dco.hw,
		[CLKID_FDLE_PLL_OD]			= &s5_fpll_tmds_od.hw,
		[CLKID_FDLE_PLL_OD1]			= &s5_fpll_tmds_od1.hw,
		[CLKID_FDLE_PLL_TMDS]			= &s5_fpll_tmds.hw,
		[CLKID_FDLE_PLL_PIXEL]			= &s5_fpll_pixel.hw,
		[CLKID_CPU4_CLK]			= &s5_cpu4_clk.hw,
		[CLKID_RTC_32K_CLKIN]			= &s5_rtc_32k_clkin.hw,
		[CLKID_RTC_32K_DIV]			= &s5_rtc_32k_div.hw,
		[CLKID_RTC_32K_XATL]			= &s5_rtc_32k_xtal.hw,
		[CLKID_RTC_32K_SEL]			= &s5_rtc_32k_sel.hw,
		[CLKID_RTC_CLK]				= &s5_rtc_clk.hw,
		[CLKID_SYS_CLK_1_SEL]			= &s5_sysclk_1_sel.hw,
		[CLKID_SYS_CLK_1_DIV]			= &s5_sysclk_1_div.hw,
		[CLKID_SYS_CLK_1]			= &s5_sysclk_1.hw,
		[CLKID_SYS_CLK_0_SEL]			= &s5_sysclk_0_sel.hw,
		[CLKID_SYS_CLK_0_DIV]			= &s5_sysclk_0_div.hw,
		[CLKID_SYS_CLK_0]			= &s5_sysclk_0.hw,
		[CLKID_SYS_CLK]				= &s5_sys_clk.hw,
		[CLKID_CECA_32K_CLKIN]		= &s5_ceca_32k_clkin.hw,
		[CLKID_CECA_32K_DIV]		= &s5_ceca_32k_div.hw,
		[CLKID_CECA_32K_MUX_PRE]	= &s5_ceca_32k_sel_pre.hw,
		[CLKID_CECA_32K_MUX]		= &s5_ceca_32k_sel.hw,
		[CLKID_CECA_32K_CLKOUT]		= &s5_ceca_32k_clkout.hw,
		[CLKID_CECB_32K_CLKIN]		= &s5_cecb_32k_clkin.hw,
		[CLKID_CECB_32K_DIV]		= &s5_cecb_32k_div.hw,
		[CLKID_CECB_32K_MUX_PRE]	= &s5_cecb_32k_sel_pre.hw,
		[CLKID_CECB_32K_MUX]		= &s5_cecb_32k_sel.hw,
		[CLKID_CECB_32K_CLKOUT]		= &s5_cecb_32k_clkout.hw,
		[CLKID_AXICLK_1_SEL]			= &s5_axiclk_1_sel.hw,
		[CLKID_AXICLK_1_DIV]			= &s5_axiclk_1_div.hw,
		[CLKID_AXICLK_1]			= &s5_axiclk_1.hw,
		[CLKID_AXICLK_0_SEL]			= &s5_axiclk_0_sel.hw,
		[CLKID_AXICLK_0_DIV]			= &s5_axiclk_0_div.hw,
		[CLKID_AXICLK_0]			= &s5_axiclk_0.hw,
		[CLKID_AXICLK]				= &s5_axi_clk.hw,
		[CLKID_24M_CLK_GATE]			= &s5_24m_clk_gate.hw,
		[CLKID_24M_DIV2]			= &s5_24m_div2.hw,
		[CLKID_12M_CLK]				= &s5_12m_clk.hw,
		[CLKID_25M_CLK_DIV]			= &s5_25m_clk_div.hw,
		[CLKID_25M_CLK]				= &s5_25m_clk.hw,
		[CLKID_VCLK_SEL]			= &s5_vclk_sel.hw,
		[CLKID_VCLK2_SEL]			= &s5_vclk2_sel.hw,
		[CLKID_VCLK_INPUT]			= &s5_vclk_input.hw,
		[CLKID_VCLK2_INPUT]			= &s5_vclk2_input.hw,
		[CLKID_VCLK_DIV]			= &s5_vclk_div.hw,
		[CLKID_VCLK2_DIV]			= &s5_vclk2_div.hw,
		[CLKID_VCLK]				= &s5_vclk.hw,
		[CLKID_VCLK2]				= &s5_vclk2.hw,
		[CLKID_VCLK_DIV1]			= &s5_vclk_div1.hw,
		[CLKID_VCLK_DIV2_EN]			= &s5_vclk_div2_en.hw,
		[CLKID_VCLK_DIV4_EN]			= &s5_vclk_div4_en.hw,
		[CLKID_VCLK_DIV6_EN]			= &s5_vclk_div6_en.hw,
		[CLKID_VCLK_DIV12_EN]			= &s5_vclk_div12_en.hw,
		[CLKID_VCLK2_DIV1]			= &s5_vclk2_div1.hw,
		[CLKID_VCLK2_DIV2_EN]			= &s5_vclk2_div2_en.hw,
		[CLKID_VCLK2_DIV4_EN]			= &s5_vclk2_div4_en.hw,
		[CLKID_VCLK2_DIV6_EN]			= &s5_vclk2_div6_en.hw,
		[CLKID_VCLK2_DIV12_EN]			= &s5_vclk2_div12_en.hw,
		[CLKID_VCLK_DIV2]			= &s5_vclk_div2.hw,
		[CLKID_VCLK_DIV4]			= &s5_vclk_div4.hw,
		[CLKID_VCLK_DIV6]			= &s5_vclk_div6.hw,
		[CLKID_VCLK_DIV12]			= &s5_vclk_div12.hw,
		[CLKID_VCLK2_DIV2]			= &s5_vclk2_div2.hw,
		[CLKID_VCLK2_DIV4]			= &s5_vclk2_div4.hw,
		[CLKID_VCLK2_DIV6]			= &s5_vclk2_div6.hw,
		[CLKID_VCLK2_DIV12]			= &s5_vclk2_div12.hw,
		[CLKID_CTS_ENCI_SEL]        = &s5_cts_enci_sel.hw,
		[CLKID_CTS_ENCT_SEL]        = &s5_cts_enct_sel.hw,
		[CLKID_CTS_ENCP_SEL]        = &s5_cts_encp_sel.hw,
		[CLKID_CTS_ENCL_SEL]        = &s5_cts_encl_sel.hw,
		[CLKID_CTS_VDAC_SEL]        = &s5_cts_vdac_sel.hw,
		[CLKID_CTS_ENCI]            = &s5_cts_enci.hw,
		[CLKID_CTS_ENCT]            = &s5_cts_enct.hw,
		[CLKID_CTS_ENCP]            = &s5_cts_encp.hw,
		[CLKID_CTS_ENCL]            = &s5_cts_encl.hw,
		[CLKID_CTS_VDAC]            = &s5_cts_vdac.hw,
		[CLKID_SD_EMMC_C_CLK_SEL]		= &s5_sd_emmc_c_sel.hw,
		[CLKID_SD_EMMC_C_CLK_DIV]		= &s5_sd_emmc_c_div.hw,
		[CLKID_SD_EMMC_C_CLK]			= &s5_sd_emmc_c.hw,
		[CLKID_SD_EMMC_A_CLK_SEL]		= &s5_sd_emmc_a_sel.hw,
		[CLKID_SD_EMMC_A_CLK_DIV]		= &s5_sd_emmc_a_div.hw,
		[CLKID_SD_EMMC_A_CLK]			= &s5_sd_emmc_a.hw,
		[CLKID_SD_EMMC_B_CLK_SEL]		= &s5_sd_emmc_b_sel.hw,
		[CLKID_SD_EMMC_B_CLK_DIV]		= &s5_sd_emmc_b_div.hw,
		[CLKID_SD_EMMC_B_CLK]			= &s5_sd_emmc_b.hw,
		[CLKID_SPICC0_SEL]			= &s5_spicc0_sel.hw,
		[CLKID_SPICC0_DIV]			= &s5_spicc0_div.hw,
		[CLKID_SPICC0]				= &s5_spicc0.hw,
		[CLKID_SPICC1_SEL]			= &s5_spicc1_sel.hw,
		[CLKID_SPICC1_DIV]			= &s5_spicc1_div.hw,
		[CLKID_SPICC1]				= &s5_spicc1.hw,
		[CLKID_SPICC2_SEL]			= &s5_spicc2_sel.hw,
		[CLKID_SPICC2_DIV]			= &s5_spicc2_div.hw,
		[CLKID_SPICC2]				= &s5_spicc2.hw,
		[CLKID_PWM_A_SEL]			= &s5_pwm_a_sel.hw,
		[CLKID_PWM_A_DIV]			= &s5_pwm_a_div.hw,
		[CLKID_PWM_A]				= &s5_pwm_a.hw,
		[CLKID_PWM_B_SEL]			= &s5_pwm_b_sel.hw,
		[CLKID_PWM_B_DIV]			= &s5_pwm_b_div.hw,
		[CLKID_PWM_B]				= &s5_pwm_b.hw,
		[CLKID_PWM_C_SEL]			= &s5_pwm_c_sel.hw,
		[CLKID_PWM_C_DIV]			= &s5_pwm_c_div.hw,
		[CLKID_PWM_C]				= &s5_pwm_c.hw,
		[CLKID_PWM_D_SEL]			= &s5_pwm_d_sel.hw,
		[CLKID_PWM_D_DIV]			= &s5_pwm_d_div.hw,
		[CLKID_PWM_D]				= &s5_pwm_d.hw,
		[CLKID_PWM_E_SEL]			= &s5_pwm_e_sel.hw,
		[CLKID_PWM_E_DIV]			= &s5_pwm_e_div.hw,
		[CLKID_PWM_E]				= &s5_pwm_e.hw,
		[CLKID_PWM_F_SEL]			= &s5_pwm_f_sel.hw,
		[CLKID_PWM_F_DIV]			= &s5_pwm_f_div.hw,
		[CLKID_PWM_F]				= &s5_pwm_f.hw,
		[CLKID_PWM_G_SEL]			= &s5_pwm_g_sel.hw,
		[CLKID_PWM_G_DIV]			= &s5_pwm_g_div.hw,
		[CLKID_PWM_G]				= &s5_pwm_g.hw,
		[CLKID_PWM_H_SEL]			= &s5_pwm_h_sel.hw,
		[CLKID_PWM_H_DIV]			= &s5_pwm_h_div.hw,
		[CLKID_PWM_H]				= &s5_pwm_h.hw,
		[CLKID_PWM_I_SEL]			= &s5_pwm_i_sel.hw,
		[CLKID_PWM_I_DIV]			= &s5_pwm_i_div.hw,
		[CLKID_PWM_I]				= &s5_pwm_i.hw,
		[CLKID_PWM_J_SEL]			= &s5_pwm_j_sel.hw,
		[CLKID_PWM_J_DIV]			= &s5_pwm_j_div.hw,
		[CLKID_PWM_J]				= &s5_pwm_j.hw,
		[CLKID_SARADC_SEL]			= &s5_saradc_sel.hw,
		[CLKID_SARADC_DIV]			= &s5_saradc_div.hw,
		[CLKID_SARADC]				= &s5_saradc.hw,
		[CLKID_GEN_SEL]				= &s5_gen_sel.hw,
		[CLKID_GEN_DIV]				= &s5_gen_div.hw,
		[CLKID_GEN]				= &s5_gen.hw,
		[CLKID_ETH_RMII_SEL]			= &s5_eth_rmii_sel.hw,
		[CLKID_ETH_RMII_DIV]			= &s5_eth_rmii_div.hw,
		[CLKID_ETH_RMII]			= &s5_eth_rmii.hw,
		[CLKID_ETH_DIV8]			= &s5_eth_div8.hw,
		[CLKID_ETH_125M]			= &s5_eth_125m.hw,
		[CLKID_TS_CLK_DIV]			= &s5_ts_clk_div.hw,
		[CLKID_TS_CLK]				= &s5_ts_clk.hw,
		[CLKID_USB_250M_SEL]			= &s5_usb_250m_sel.hw,
		[CLKID_USB_250M_DIV]			= &s5_usb_250m_div.hw,
		[CLKID_USB_250M]			= &s5_usb_250m.hw,
		[CLKID_PCIE_400M_SEL]			= &s5_pcie_400m_sel.hw,
		[CLKID_PCIE_400M_DIV]			= &s5_pcie_400m_div.hw,
		[CLKID_PCIE_400M]			= &s5_pcie_400m.hw,
		[CLKID_PCIE_CLK_SEL]			= &s5_pcie_clk_sel.hw,
		[CLKID_PCIE_CLK_DIV]			= &s5_pcie_clk_div.hw,
		[CLKID_PCIE_CLK]			= &s5_pcie_clk.hw,
		[CLKID_PCIE_TL_CLK_SEL]			= &s5_pcie_tl_sel.hw,
		[CLKID_PCIE_TL_CLK_DIV]			= &s5_pcie_tl_div.hw,
		[CLKID_PCIE_TL_CLK]			= &s5_pcie_tl.hw,
		[CLKID_CDAC_SEL]			= &s5_cdac_sel.hw,
		[CLKID_CDAC_DIV]			= &s5_cdac_div.hw,
		[CLKID_CDAC]				= &s5_cdac.hw,
		[CLKID_SC_SEL]				= &s5_sc_sel.hw,
		[CLKID_SC_DIV]				= &s5_sc_div.hw,
		[CLKID_SC]				= &s5_sc.hw,
		[CLKID_VAPB_0_SEL]			= &s5_vapb_sel.hw,
		[CLKID_VAPB_0_DIV]			= &s5_vapb_div.hw,
		[CLKID_VAPB_0]				= &s5_vapb.hw,
		[CLKID_GE2D_SEL]			= &s5_ge2d_sel.hw,
		[CLKID_GE2D_DIV]			= &s5_ge2d_div.hw,
		[CLKID_GE2D]				= &s5_ge2d.hw,
		[CLKID_NNA_0_SEL]			= &s5_nna0_sel.hw,
		[CLKID_NNA_0_DIV]			= &s5_nna0_div.hw,
		[CLKID_NNA_0]				= &s5_nna0.hw,
		[CLKID_NNA_1_SEL]			= &s5_nna1_sel.hw,
		[CLKID_NNA_1_DIV]			= &s5_nna1_div.hw,
		[CLKID_NNA_1]				= &s5_nna1.hw,
		[CLKID_NNA_SEL]					= &s5_nna_sel.hw,
		[CLKID_NNA]					= &s5_nna.hw,
		[CLKID_VPU0_SEL]			= &s5_vpuclk0_sel.hw,
		[CLKID_VPU0_DIV]			= &s5_vpuclk0_div.hw,
		[CLKID_VPU0]				= &s5_vpuclk0.hw,
		[CLKID_VPU1_SEL]			= &s5_vpuclk1_sel.hw,
		[CLKID_VPU1_DIV]			= &s5_vpuclk1_div.hw,
		[CLKID_VPU1]				= &s5_vpuclk1.hw,
		[CLKID_VPU_SEL]				= &s5_vpuclk_sel.hw,
		[CLKID_VPU]					= &s5_vpuclk.hw,
		[CLKID_VPU_CLKB_TMP_SEL]		= &s5_vpuclkb_tmp_sel.hw,
		[CLKID_VPU_CLKB_TMP_DIV]		= &s5_vpuclkb_tmp_div.hw,
		[CLKID_VPU_CLKB_TMP]			= &s5_vpuclkb_tmp.hw,
		[CLKID_VPU_CLKB_DIV]			= &s5_vpuclkb_div.hw,
		[CLKID_VPU_CLKB]			= &s5_vpuclkb.hw,
		[CLKID_VIN_MEAS_SEL]			= &s5_vdin_meas_sel.hw,
		[CLKID_VIN_MEAS_DIV]			= &s5_vdin_meas_div.hw,
		[CLKID_VIN_MEAS]			= &s5_vdin_meas.hw,
		[CLKID_VID_LOCK_SEL]			= &s5_vid_lock_sel.hw,
		[CLKID_VID_LOCK_DIV]			= &s5_vid_lock_div.hw,
		[CLKID_VID_LOCK]			= &s5_vid_lock.hw,
		[CLKID_CMPR_SEL]			= &s5_cmpr_sel.hw,
		[CLKID_CMPR_DIV]			= &s5_cmpr_div.hw,
		[CLKID_CMPR]				= &s5_cmpr.hw,
		[CLKID_MALI0_SEL]			= &s5_mali0_sel.hw,
		[CLKID_MALI0_DIV]			= &s5_mali0_div.hw,
		[CLKID_MALI0]				= &s5_mali0.hw,
		[CLKID_MALI1_SEL]			= &s5_mali1_sel.hw,
		[CLKID_MALI1_DIV]			= &s5_mali1_div.hw,
		[CLKID_MALI1]				= &s5_mali1.hw,
		[CLKID_MALI_SEL]				= &s5_mali_sel.hw,
		[CLKID_MALI]				= &s5_mali.hw,
		[CLKID_VDEC0_SEL]			= &s5_vdec0_sel.hw,
		[CLKID_VDEC0_DIV]			= &s5_vdec0_div.hw,
		[CLKID_VDEC0]				= &s5_vdec0.hw,
		[CLKID_VDEC1_SEL]			= &s5_vdec1_sel.hw,
		[CLKID_VDEC1_DIV]			= &s5_vdec1_div.hw,
		[CLKID_VDEC1]				= &s5_vdec1.hw,
		[CLKID_VDEC_SEL]				= &s5_vdec_sel.hw,
		[CLKID_VDEC]				= &s5_vdec.hw,
		[CLKID_HCODEC0_SEL]			= &s5_hcodec0_sel.hw,
		[CLKID_HCODEC0_DIV]			= &s5_hcodec0_div.hw,
		[CLKID_HCODEC0]				= &s5_hcodec0.hw,
		[CLKID_HCODEC1_SEL]			= &s5_hcodec1_sel.hw,
		[CLKID_HCODEC1_DIV]			= &s5_hcodec1_div.hw,
		[CLKID_HCODEC1]				= &s5_hcodec1.hw,
		[CLKID_HCODEC_SEL]				= &s5_hcodec_sel.hw,
		[CLKID_HCODEC]				= &s5_hcodec.hw,
		[CLKID_HEVC0_SEL]			= &s5_hevc0_sel.hw,
		[CLKID_HEVC0_DIV]			= &s5_hevc0_div.hw,
		[CLKID_HEVC0]				= &s5_hevc0.hw,
		[CLKID_HEVC1_SEL]			= &s5_hevc1_sel.hw,
		[CLKID_HEVC1_DIV]			= &s5_hevc1_div.hw,
		[CLKID_HEVC1]				= &s5_hevc1.hw,
		[CLKID_HEVC_SEL]				= &s5_hevc_sel.hw,
		[CLKID_HEVC]				= &s5_hevc.hw,
		[CLKID_VC9000E_AXI_SEL]			= &s5_vc9000e_axi_sel.hw,
		[CLKID_VC9000E_AXI_DIV]			= &s5_vc9000e_axi_div.hw,
		[CLKID_VC9000E_AXI]			= &s5_vc9000e_axi.hw,
		[CLKID_VC9000E_CORE_SEL]		= &s5_vc9000e_core_sel.hw,
		[CLKID_VC9000E_CORE_DIV]		= &s5_vc9000e_core_div.hw,
		[CLKID_VC9000E_CORE]			= &s5_vc9000e_core.hw,
		[CLKID_HDMITX_SYS_SEL]			= &s5_hdmitx_sys_sel.hw,
		[CLKID_HDMITX_SYS_DIV]			= &s5_hdmitx_sys_div.hw,
		[CLKID_HDMITX_SYS]			= &s5_hdmitx_sys.hw,
		[CLKID_HDMITX_PRIF_SEL]			= &s5_hdmitx_prif_sel.hw,
		[CLKID_HDMITX_PRIF_DIV]			= &s5_hdmitx_prif_div.hw,
		[CLKID_HDMITX_PRIF]			= &s5_hdmitx_prif.hw,
		[CLKID_HDMITX_200M_SEL]			= &s5_hdmitx_200m_sel.hw,
		[CLKID_HDMITX_200M_DIV]			= &s5_hdmitx_200m_div.hw,
		[CLKID_HDMITX_200M]			= &s5_hdmitx_200m.hw,
		[CLKID_HDMITX_AUD_SEL]			= &s5_hdmitx_aud_sel.hw,
		[CLKID_HDMITX_AUD_DIV]			= &s5_hdmitx_aud_div.hw,
		[CLKID_HDMITX_AUD]			= &s5_hdmitx_aud.hw,
		[CLKID_ENC_HDMI_TX_PNX_SEL]		= &s5_enc_hdmi_tx_pnx_clk_sel.hw,
		[CLKID_ENC_HDMI_TX_PNX]			= &s5_enc_hdmi_tx_pnx_clk.hw,
		[CLKID_ENC_HDMI_TX_FE_SEL]		= &s5_enc_hdmi_tx_fe_clk_sel.hw,
		[CLKID_ENC_HDMI_TX_FE]			= &s5_enc_hdmi_tx_fe_clk.hw,
		[CLKID_ENC_HDMI_TX_PIXEL_SEL]		= &s5_enc_hdmi_tx_pixel_clk_sel.hw,
		[CLKID_ENC_HDMI_TX_PIXEL]		= &s5_enc_hdmi_tx_pixel_clk.hw,
		[CLKID_HDMI_TX_PNX_SEL]     = &s5_hdmi_tx_pnx_clk_sel.hw,
		[CLKID_HDMI_TX_FE_SEL]      = &s5_hdmi_tx_fe_clk_sel.hw,
		[CLKID_HDMI_TX_PIXEL_SEL]   = &s5_hdmi_tx_pixel_clk_sel.hw,
		[CLKID_HDMI_TX_PNX_DIV]     = &s5_hdmi_tx_pnx_clk_div.hw,
		[CLKID_HDMI_TX_FE_DIV]      = &s5_hdmi_tx_fe_clk_div.hw,
		[CLKID_HDMI_TX_PIXEL_DIV]   = &s5_hdmi_tx_pixel_clk_div.hw,
		[CLKID_HDMI_TX_PNX]         = &s5_hdmi_tx_pnx_clk.hw,
		[CLKID_HDMI_TX_FE]          = &s5_hdmi_tx_fe_clk.hw,
		[CLKID_HDMI_TX_PIXEL]       = &s5_hdmi_tx_pixel_clk.hw,
		[CLKID_HTX_TMDS_SEL]        = &s5_htx_tmds_sel.hw,
		[CLKID_HTX_TMDS_DIV]        = &s5_htx_tmds_div.hw,
		[CLKID_HTX_TMDS]            = &s5_htx_tmds.hw,
		[CLKID_SYS_CLK_DDR]			= &s5_sys_clk_ddr.hw,
		[CLKID_SYS_CLK_ETHPHY]			= &s5_sys_clk_ethphy.hw,
		[CLKID_SYS_CLK_GPU]			= &s5_sys_clk_gpu.hw,
		[CLKID_SYS_CLK_VC9000E]			= &s5_sys_clk_vc9000e.hw,
		[CLKID_SYS_CLK_AOCPU]			= &s5_sys_clk_aocpu.hw,
		[CLKID_SYS_CLK_AUCPU]			= &s5_sys_clk_aucpu.hw,
		[CLKID_SYS_CLK_DEWARPC]			= &s5_sys_clk_dewarpc.hw,
		[CLKID_SYS_CLK_DEWARPB]			= &s5_sys_clk_dewarpb.hw,
		[CLKID_SYS_CLK_DEWARPA]			= &s5_sys_clk_dewarpa.hw,
		[CLKID_SYS_CLK_AMPIPE_NAND]		= &s5_sys_clk_ampipe_nand.hw,
		[CLKID_SYS_CLK_AMPIPE_ETH]		= &s5_sys_clk_ampipe_eth.hw,
		[CLKID_SYS_CLK_AM2AXI0]			= &s5_sys_clk_am2axi0.hw,
		[CLKID_SYS_CLK_IR_CTRL]			= &s5_sys_clk_ir_ctrl.hw,
		[CLKID_SYS_CLK_SD_EMMC_B]		= &s5_sys_clk_sdemmcb.hw,
		[CLKID_SYS_CLK_SD_EMMC_A]		= &s5_sys_clk_sdemmca.hw,
		[CLKID_SYS_CLK_SD_EMMC_C]		= &s5_sys_clk_sdemmcc.hw,
		[CLKID_SYS_CLK_SPIFC]			= &s5_sys_clk_spifc.hw,
		[CLKID_SYS_CLK_MSR_CLK]			= &s5_sys_clk_msr_clk.hw,
		[CLKID_SYS_CLK_ACODEC]			= &s5_sys_clk_acodec.hw,
		[CLKID_SYS_CLK_AUDIO]			= &s5_sys_clk_audio.hw,
		[CLKID_SYS_CLK_ETH]			= &s5_sys_clk_eth.hw,
		[CLKID_SYS_CLK_UART_A]			= &s5_sys_clk_uart_a.hw,
		[CLKID_SYS_CLK_UART_B]			= &s5_sys_clk_uart_b.hw,
		[CLKID_SYS_CLK_UART_C]			= &s5_sys_clk_uart_c.hw,
		[CLKID_SYS_CLK_UART_D]			= &s5_sys_clk_uart_d.hw,
		[CLKID_SYS_CLK_UART_E]			= &s5_sys_clk_uart_e.hw,
		[CLKID_SYS_CLK_UART_F]			= &s5_sys_clk_uart_f.hw,
		[CLKID_SYS_CLK_DOS]			= &s5_sys_clk_dos.hw,
		[CLKID_SYS_CLK_SPICC2]			= &s5_sys_clk_spicc2.hw,
		[CLKID_SYS_CLK_TS_A55]			= &s5_sys_clk_ts_a55.hw,
		[CLKID_SYS_CLK_SMART_CARD]		= &s5_sys_clk_smart_card.hw,
		[CLKID_SYS_CLK_G2D]			= &s5_sys_clk_g2d.hw,
		[CLKID_SYS_CLK_SPICC0]			= &s5_sys_clk_spicc0.hw,
		[CLKID_SYS_CLK_SPICC1]			= &s5_sys_clk_spicc1.hw,
		[CLKID_SYS_CLK_PCIE]			= &s5_sys_clk_pcie.hw,
		[CLKID_SYS_CLK_PCIEPHY]			= &s5_sys_clk_pciephy.hw,
		[CLKID_SYS_CLK_USB]			= &s5_sys_clk_usb.hw,
		[CLKID_SYS_CLK_PCIE_PHY0]		= &s5_sys_clk_pcie_phy0.hw,
		[CLKID_SYS_CLK_PCIE_PHY1]		= &s5_sys_clk_pcie_phy1.hw,
		[CLKID_SYS_CLK_PCIE_PHY2]		= &s5_sys_clk_pcie_phy2.hw,
		[CLKID_SYS_CLK_I2C_M_A]			= &s5_sys_clk_i2c_m_a.hw,
		[CLKID_SYS_CLK_I2C_M_B]			= &s5_sys_clk_i2c_m_b.hw,
		[CLKID_SYS_CLK_I2C_M_C]			= &s5_sys_clk_i2c_m_c.hw,
		[CLKID_SYS_CLK_I2C_M_D]			= &s5_sys_clk_i2c_m_d.hw,
		[CLKID_SYS_CLK_I2C_M_E]			= &s5_sys_clk_i2c_m_e.hw,
		[CLKID_SYS_CLK_I2C_M_F]			= &s5_sys_clk_i2c_m_f.hw,
		[CLKID_SYS_CLK_TS_GPU]			= &s5_sys_clk_ts_gpu.hw,
		[CLKID_SYS_CLK_I2C_S_A]			= &s5_sys_clk_i2c_s_a.hw,
		[CLKID_SYS_CLK_CMPR]			= &s5_sys_clk_cmpr.hw,
		[CLKID_SYS_CLK_MMC_PCLK]		= &s5_sys_clk_mmc_pclk.hw,
		[CLKID_SYS_CLK_HDMITX_PCLK]		= &s5_sys_clk_hdmitx_pclk.hw,
		[CLKID_SYS_CLK_HDMI20_AES_CLK]		= &s5_sys_clk_hdmi20_aes_clk.hw,
		[CLKID_SYS_CLK_PCLK_SYS_CPU_APB]	= &s5_sys_clk_pclk_sys_cpu_apb.hw,
		[CLKID_SYS_CLK_CEC]			= &s5_sys_clk_cec.hw,
		[CLKID_SYS_CLK_VPU_INTR]		= &s5_sys_clk_vpu_intr.hw,
		[CLKID_SYS_CLK_SAR_ADC]			= &s5_sys_clk_sar_adc.hw,
		[CLKID_SYS_CLK_GIC]			= &s5_sys_clk_gic.hw,
		[CLKID_SYS_CLK_TS_NNA]			= &s5_sys_clk_ts_nna.hw,
		[CLKID_SYS_CLK_PWM_AB]			= &s5_sys_clk_pwm_ab.hw,
		[CLKID_SYS_CLK_PWM_CD]			= &s5_sys_clk_pwm_cd.hw,
		[CLKID_SYS_CLK_PWM_EF]			= &s5_sys_clk_pwm_ef.hw,
		[CLKID_SYS_CLK_PWM_GH]			= &s5_sys_clk_pwm_gh.hw,
		[CLKID_SYS_CLK_PWM_IJ]			= &s5_sys_clk_pwm_ij.hw,
		[CLKID_SYS_CLK_TS_VPU]			= &s5_sys_clk_ts_vpu.hw,
		[CLKID_SYS_CLK_TS_DOS]			= &s5_sys_clk_ts_dos.hw,
		[NR_CLKS]				= NULL
	},
	.num = NR_CLKS,
};

/* Convenience table to populate regmap in .probe */
static struct clk_regmap *const s5_clk_regmaps[] = {
	&s5_rtc_32k_clkin,
	&s5_rtc_32k_div,
	&s5_rtc_32k_xtal,
	&s5_rtc_32k_sel,
	&s5_rtc_clk,
	&s5_ceca_32k_clkin,
	&s5_ceca_32k_div,
	&s5_ceca_32k_sel_pre,
	&s5_ceca_32k_sel,
	&s5_ceca_32k_clkout,
	&s5_cecb_32k_clkin,
	&s5_cecb_32k_div,
	&s5_cecb_32k_sel_pre,
	&s5_cecb_32k_sel,
	&s5_cecb_32k_clkout,
	&s5_sysclk_1_sel,
	&s5_sysclk_1_div,
	&s5_sysclk_1,
	&s5_sysclk_0_sel,
	&s5_sysclk_0_div,
	&s5_sysclk_0,
	&s5_sys_clk,
	&s5_axiclk_0_sel,
	&s5_axiclk_0_div,
	&s5_axiclk_0,
	&s5_axiclk_1_sel,
	&s5_axiclk_1_div,
	&s5_axiclk_1,
	&s5_axi_clk,
	&s5_24m_clk_gate,
	&s5_12m_clk,
	&s5_25m_clk_div,
	&s5_25m_clk,
	&s5_vclk_sel,
	&s5_vclk2_sel,
	&s5_vclk_input,
	&s5_vclk2_input,
	&s5_vclk_div,
	&s5_vclk2_div,
	&s5_vclk,
	&s5_vclk2,
	&s5_vclk_div1,
	&s5_vclk_div2_en,
	&s5_vclk_div4_en,
	&s5_vclk_div6_en,
	&s5_vclk_div12_en,
	&s5_vclk2_div1,
	&s5_vclk2_div2_en,
	&s5_vclk2_div4_en,
	&s5_vclk2_div6_en,
	&s5_vclk2_div12_en,
	&s5_cts_enci_sel,
	&s5_cts_enct_sel,
	&s5_cts_encp_sel,
	&s5_cts_encl_sel,
	&s5_cts_vdac_sel,
	&s5_cts_enci,
	&s5_cts_enct,
	&s5_cts_encp,
	&s5_cts_encl,
	&s5_cts_vdac,
	&s5_vapb_sel,
	&s5_vapb_div,
	&s5_vapb,
	&s5_ge2d_sel,
	&s5_ge2d_div,
	&s5_ge2d,
	&s5_sd_emmc_c_sel,
	&s5_sd_emmc_c_div,
	&s5_sd_emmc_c,
	&s5_sd_emmc_a_sel,
	&s5_sd_emmc_a_div,
	&s5_sd_emmc_a,
	&s5_sd_emmc_b_sel,
	&s5_sd_emmc_b_div,
	&s5_sd_emmc_b,
	&s5_spicc0_sel,
	&s5_spicc0_div,
	&s5_spicc0,
	&s5_spicc1_sel,
	&s5_spicc1_div,
	&s5_spicc1,
	&s5_spicc2_sel,
	&s5_spicc2_div,
	&s5_spicc2,
	&s5_pwm_a_sel,
	&s5_pwm_a_div,
	&s5_pwm_a,
	&s5_pwm_b_sel,
	&s5_pwm_b_div,
	&s5_pwm_b,
	&s5_pwm_c_sel,
	&s5_pwm_c_div,
	&s5_pwm_c,
	&s5_pwm_d_sel,
	&s5_pwm_d_div,
	&s5_pwm_d,
	&s5_pwm_e_sel,
	&s5_pwm_e_div,
	&s5_pwm_e,
	&s5_pwm_f_sel,
	&s5_pwm_f_div,
	&s5_pwm_f,
	&s5_pwm_g_sel,
	&s5_pwm_g_div,
	&s5_pwm_g,
	&s5_pwm_h_sel,
	&s5_pwm_h_div,
	&s5_pwm_h,
	&s5_pwm_i_sel,
	&s5_pwm_i_div,
	&s5_pwm_i,
	&s5_pwm_j_sel,
	&s5_pwm_j_div,
	&s5_pwm_j,
	&s5_saradc_sel,
	&s5_saradc_div,
	&s5_saradc,
	&s5_gen_sel,
	&s5_gen_div,
	&s5_gen,
	&s5_eth_rmii_sel,
	&s5_eth_rmii_div,
	&s5_eth_rmii,
	&s5_eth_125m,
	&s5_ts_clk_div,
	&s5_ts_clk,
	&s5_usb_250m_sel,
	&s5_usb_250m_div,
	&s5_usb_250m,
	&s5_pcie_400m_sel,
	&s5_pcie_400m_div,
	&s5_pcie_400m,
	&s5_pcie_clk_sel,
	&s5_pcie_clk_div,
	&s5_pcie_clk,
	&s5_pcie_tl_sel,
	&s5_pcie_tl_div,
	&s5_pcie_tl,
	&s5_nna0_sel,
	&s5_nna0_div,
	&s5_nna0,
	&s5_nna1_sel,
	&s5_nna1_div,
	&s5_nna1,
	&s5_nna_sel,
	&s5_nna,
	&s5_cdac_sel,
	&s5_cdac_div,
	&s5_cdac,
	&s5_sc_sel,
	&s5_sc_div,
	&s5_sc,
	&s5_vpuclk0_sel,
	&s5_vpuclk0_div,
	&s5_vpuclk0,
	&s5_vpuclk1_sel,
	&s5_vpuclk1_div,
	&s5_vpuclk1,
	&s5_vpuclk_sel,
	&s5_vpuclk,
	&s5_vpuclkb_tmp_sel,
	&s5_vpuclkb_tmp_div,
	&s5_vpuclkb_tmp,
	&s5_vpuclkb_div,
	&s5_vpuclkb,
	&s5_vdin_meas_sel,
	&s5_vdin_meas_div,
	&s5_vdin_meas,
	&s5_cmpr_sel,
	&s5_cmpr_div,
	&s5_cmpr,
	&s5_vid_lock_sel,
	&s5_vid_lock_div,
	&s5_vid_lock,
	&s5_mali0_sel,
	&s5_mali0_div,
	&s5_mali0,
	&s5_mali1_sel,
	&s5_mali1_div,
	&s5_mali1,
	&s5_mali_sel,
	&s5_mali,
	&s5_vdec0_sel,
	&s5_vdec0_div,
	&s5_vdec0,
	&s5_vdec1_sel,
	&s5_vdec1_div,
	&s5_vdec1,
	&s5_vdec_sel,
	&s5_vdec,
	&s5_hcodec0_sel,
	&s5_hcodec0_div,
	&s5_hcodec0,
	&s5_hcodec1_sel,
	&s5_hcodec1_div,
	&s5_hcodec1,
	&s5_hcodec_sel,
	&s5_hcodec,
	&s5_hevc0_sel,
	&s5_hevc0_div,
	&s5_hevc0,
	&s5_hevc1_sel,
	&s5_hevc1_div,
	&s5_hevc1,
	&s5_hevc_sel,
	&s5_hevc,
	&s5_vc9000e_axi_sel,
	&s5_vc9000e_axi_div,
	&s5_vc9000e_axi,
	&s5_vc9000e_core_sel,
	&s5_vc9000e_core_div,
	&s5_vc9000e_core,
	&s5_hdmitx_sys_sel,
	&s5_hdmitx_sys_div,
	&s5_hdmitx_sys,
	&s5_hdmitx_prif_sel,
	&s5_hdmitx_prif_div,
	&s5_hdmitx_prif,
	&s5_hdmitx_200m_sel,
	&s5_hdmitx_200m_div,
	&s5_hdmitx_200m,
	&s5_hdmitx_aud_sel,
	&s5_hdmitx_aud_div,
	&s5_hdmitx_aud,
	&s5_enc_hdmi_tx_pnx_clk_sel,
	&s5_enc_hdmi_tx_pnx_clk,
	&s5_enc_hdmi_tx_fe_clk_sel,
	&s5_enc_hdmi_tx_fe_clk,
	&s5_enc_hdmi_tx_pixel_clk_sel,
	&s5_enc_hdmi_tx_pixel_clk,
	&s5_hdmi_tx_pnx_clk_sel,
	&s5_hdmi_tx_fe_clk_sel,
	&s5_hdmi_tx_pixel_clk_sel,
	&s5_hdmi_tx_pnx_clk_div,
	&s5_hdmi_tx_fe_clk_div,
	&s5_hdmi_tx_pixel_clk_div,
	&s5_hdmi_tx_pnx_clk,
	&s5_hdmi_tx_fe_clk,
	&s5_hdmi_tx_pixel_clk,
	&s5_htx_tmds_sel,
	&s5_htx_tmds_div,
	&s5_htx_tmds,
	&s5_sys_clk_ddr,
	&s5_sys_clk_ethphy,
	&s5_sys_clk_gpu,
	&s5_sys_clk_aocpu,
	&s5_sys_clk_aucpu,
	&s5_sys_clk_dewarpc,
	&s5_sys_clk_dewarpb,
	&s5_sys_clk_dewarpa,
	&s5_sys_clk_ampipe_nand,
	&s5_sys_clk_ampipe_eth,
	&s5_sys_clk_am2axi0,
	&s5_sys_clk_sdemmca,
	&s5_sys_clk_sdemmcb,
	&s5_sys_clk_sdemmcc,
	&s5_sys_clk_vc9000e,
	&s5_sys_clk_acodec,
	&s5_sys_clk_spifc,
	&s5_sys_clk_msr_clk,
	&s5_sys_clk_ir_ctrl,
	&s5_sys_clk_audio,
	&s5_sys_clk_dos,
	&s5_sys_clk_eth,
	&s5_sys_clk_uart_a,
	&s5_sys_clk_uart_b,
	&s5_sys_clk_uart_c,
	&s5_sys_clk_uart_d,
	&s5_sys_clk_uart_e,
	&s5_sys_clk_uart_f,
	&s5_sys_clk_spicc2,
	&s5_sys_clk_ts_a55,
	&s5_sys_clk_g2d,
	&s5_sys_clk_spicc0,
	&s5_sys_clk_spicc1,
	&s5_sys_clk_smart_card,
	&s5_sys_clk_pcie,
	&s5_sys_clk_pciephy,
	&s5_sys_clk_usb,
	&s5_sys_clk_pcie_phy0,
	&s5_sys_clk_pcie_phy1,
	&s5_sys_clk_pcie_phy2,
	&s5_sys_clk_i2c_m_a,
	&s5_sys_clk_i2c_m_b,
	&s5_sys_clk_i2c_m_c,
	&s5_sys_clk_i2c_m_d,
	&s5_sys_clk_i2c_m_e,
	&s5_sys_clk_i2c_m_f,
	&s5_sys_clk_i2c_s_a,
	&s5_sys_clk_cmpr,
	&s5_sys_clk_mmc_pclk,
	&s5_sys_clk_hdmitx_pclk,
	&s5_sys_clk_hdmi20_aes_clk,
	&s5_sys_clk_pclk_sys_cpu_apb,
	&s5_sys_clk_cec,
	&s5_sys_clk_vpu_intr,
	&s5_sys_clk_sar_adc,
	&s5_sys_clk_gic,
	&s5_sys_clk_ts_gpu,
	&s5_sys_clk_ts_nna,
	&s5_sys_clk_ts_vpu,
	&s5_sys_clk_ts_dos,
	&s5_sys_clk_pwm_ab,
	&s5_sys_clk_pwm_cd,
	&s5_sys_clk_pwm_ef,
	&s5_sys_clk_pwm_gh,
	&s5_sys_clk_pwm_ij,
	/*plls*/
	&s5_sys_pll_dco,
	&s5_sys_pll,
	&s5_sys1_pll_dco,
	&s5_sys2_pll,
	&s5_sys2_pll_dco,
	&s5_sys1_pll,
	&s5_fixed_pll_dco,
	&s5_fixed_pll,
	&s5_fclk_div2,
	&s5_fclk_div3,
	&s5_fclk_div4,
	&s5_fclk_div5,
	&s5_fclk_div7,
	&s5_fclk_div2p5,
	&s5_fclk_clk50m,
	&s5_gp0_pll_dco,
	&s5_gp0_pll,
	&s5_gp1_pll_dco,
	&s5_gp1_pll,
	&s5_gp2_pll_dco,
	&s5_gp2_pll,
	&s5_hifi_pll_dco,
	&s5_hifi_pll,
	&s5_fpll_dco,
	&s5_fpll_tmds_od,
	&s5_fpll_tmds_od1,
	&s5_fpll_pixel,
	&s5_nna_pll_dco,
	&s5_nna_pll,
	&s5_nna_pll_audio,
	&s5_hifi1_pll_dco,
	&s5_hifi1_pll,
};

static struct clk_regmap *const s5_pll_regmaps[] = {
	&s5_pcie_pll_dco,
	&s5_pcie_pll_od,
	&s5_pcie_bgp,
	&s5_pcie_hcsl_out,
	&s5_pcie_hcsl_in_pad,
	&s5_pcie_clk_in_sel,
	&s5_pcie1_pll_dco,
	&s5_pcie1_pll_od,
	&s5_pcie1_bgp,
	&s5_pcie1_hcsl_out,
	&s5_pcie1_hcsl_in_pad,
	&s5_pcie1_clk_in_sel,
};

static struct clk_regmap *const s5_cpu_clk_regmaps[] = {
	&s5_cpu_dyn_clk,
	&s5_cpu_clk,
	&s5_a76_dyn_clk,
	&s5_a76_clk,
	&s5_dsu_dyn_clk,
	&s5_dsu_clk,
	&s5_dsu_final_clk,
	&s5_cpu4_clk,
};

struct s5_dual_div_mux_nb_data {
	struct notifier_block nb;
	struct clk_hw *clk_mux0;
	struct clk_hw *clk_mux1;
	struct clk_hw *clk_mux;
};

static int s5_dual_div_mux_notifier_cb(struct notifier_block *nb,
				   unsigned long event, void *data)
{
	struct clk_notifier_data *cnd = (struct clk_notifier_data *)data;
	struct s5_dual_div_mux_nb_data *nb_data =
		container_of(nb, struct s5_dual_div_mux_nb_data, nb);

	switch (event) {
	case PRE_RATE_CHANGE:
		clk_set_rate(nb_data->clk_mux1->clk, cnd->new_rate);
		clk_prepare_enable(nb_data->clk_mux1->clk);
		clk_hw_set_parent(nb_data->clk_mux, nb_data->clk_mux1);

		udelay(5);

		return NOTIFY_OK;

	case POST_RATE_CHANGE:
		clk_hw_set_parent(nb_data->clk_mux, nb_data->clk_mux0);
		clk_disable_unprepare(nb_data->clk_mux1->clk);

		udelay(5);

		return NOTIFY_OK;

	default:
		return NOTIFY_DONE;
	}
}

static struct s5_dual_div_mux_nb_data s5_nna_clk_nb_data[] = {
	{
		.clk_mux0 = &s5_nna0.hw,
		.clk_mux1 = &s5_nna1.hw,
		.clk_mux = &s5_nna_sel.hw,
		.nb.notifier_call = s5_dual_div_mux_notifier_cb,
	},
	{
		.clk_mux0 = &s5_vpuclk0.hw,
		.clk_mux1 = &s5_vpuclk1.hw,
		.clk_mux = &s5_vpuclk_sel.hw,
		.nb.notifier_call = s5_dual_div_mux_notifier_cb,
	},
	{
		.clk_mux0 = &s5_mali0.hw,
		.clk_mux1 = &s5_mali1.hw,
		.clk_mux = &s5_mali_sel.hw,
		.nb.notifier_call = s5_dual_div_mux_notifier_cb,
	},
	{
		.clk_mux0 = &s5_vdec0.hw,
		.clk_mux1 = &s5_vdec1.hw,
		.clk_mux = &s5_vdec_sel.hw,
		.nb.notifier_call = s5_dual_div_mux_notifier_cb,
	},
	{
		.clk_mux0 = &s5_hcodec0.hw,
		.clk_mux1 = &s5_hcodec1.hw,
		.clk_mux = &s5_hcodec_sel.hw,
		.nb.notifier_call = s5_dual_div_mux_notifier_cb,
	},
	{
		.clk_mux0 = &s5_hevc0.hw,
		.clk_mux1 = &s5_hevc1.hw,
		.clk_mux = &s5_hevc_sel.hw,
		.nb.notifier_call = s5_dual_div_mux_notifier_cb,
	},
};

static int meson_s5_dvfs_setup(struct platform_device *pdev)
{
	int ret, i;

	/* Setup cluster 0 clock notifier for sys_pll */
	ret = clk_notifier_register(s5_sys_pll.hw.clk,
				    &s5_sys_pll_nb_data.nb);
	if (ret) {
		dev_err(&pdev->dev, "failed to register sys_pll notifier\n");
		return ret;
	}

	/* Setup cluster 1 clock notifier for sys1_pll */
	ret = clk_notifier_register(s5_sys1_pll.hw.clk,
				    &s5_sys1_pll_nb_data.nb);
	if (ret) {
		dev_err(&pdev->dev, "failed to register sys1_pll notifier\n");
		return ret;
	}
	/* Setup clock notifier for gp1_pll */
	ret = clk_notifier_register(s5_gp1_pll.hw.clk,
				    &s5_gp1_pll_nb_data.nb);
	if (ret) {
		dev_err(&pdev->dev, "failed to register gp1_pll notifier\n");
		return ret;
	}

	for (i = 0; i < ARRAY_SIZE(s5_nna_clk_nb_data); i++) {
		ret = clk_notifier_register(s5_nna_clk_nb_data[i].clk_mux0->clk,
				    &s5_nna_clk_nb_data[i].nb);
		if (ret) {
			dev_err(&pdev->dev, "failed to register dual_div_mux notifier\n");
			return ret;
		}

		clk_hw_set_parent(s5_nna_clk_nb_data[i].clk_mux, s5_nna_clk_nb_data[i].clk_mux0);
	}

	return 0;
}

static struct regmap_config clkc_regmap_config = {
	.reg_bits       = 32,
	.val_bits       = 32,
	.reg_stride     = 4,
};

static struct regmap *s5_regmap_resource(struct device *dev, char *name)
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

static int meson_s5_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct regmap *basic_map, *pll_map;
	struct regmap *cpu_clk_map;
	int ret, i;

	/* Get regmap for different clock area */
	basic_map = s5_regmap_resource(dev, "basic");
	if (IS_ERR(basic_map)) {
		dev_err(dev, "basic clk registers not found\n");
		return PTR_ERR(basic_map);
	}

	cpu_clk_map = s5_regmap_resource(dev, "cpu_clk");
	if (IS_ERR(cpu_clk_map)) {
		dev_err(dev, "cpu clk registers not found\n");
		return PTR_ERR(cpu_clk_map);
	}

	pll_map = s5_regmap_resource(dev, "pll");
	if (IS_ERR(pll_map)) {
		dev_err(dev, "pll clk registers not found\n");
		return PTR_ERR(pll_map);
	}

	/* Populate regmap for the regmap backed clocks */
	for (i = 0; i < ARRAY_SIZE(s5_clk_regmaps); i++)
		s5_clk_regmaps[i]->map = basic_map;

	for (i = 0; i < ARRAY_SIZE(s5_cpu_clk_regmaps); i++)
		s5_cpu_clk_regmaps[i]->map = cpu_clk_map;

	for (i = 0; i < ARRAY_SIZE(s5_pll_regmaps); i++)
		s5_pll_regmaps[i]->map = pll_map;

	regmap_write(basic_map, CLKCTRL_MPLL_CTRL0, 0x00000543);

	for (i = 0; i < s5_hw_onecell_data.num; i++) {
		/* array might be sparse */
		if (!s5_hw_onecell_data.hws[i])
			continue;

		ret = devm_clk_hw_register(dev, s5_hw_onecell_data.hws[i]);
		if (ret) {
			dev_err(dev, "Clock registration failed\n");
			return ret;
		}
	}

	meson_s5_dvfs_setup(pdev);

	return devm_of_clk_add_hw_provider(dev, of_clk_hw_onecell_get,
					   &s5_hw_onecell_data);
}

static const struct of_device_id clkc_match_table[] = {
	{
		.compatible = "amlogic,s5-clkc"
	},
	{}
};

static struct platform_driver s5_driver = {
	.probe		= meson_s5_probe,
	.driver		= {
		.name	= "s5-clkc",
		.of_match_table = clkc_match_table,
	},
};

#ifndef CONFIG_AMLOGIC_MODIFY
builtin_platform_driver(s5_driver);
#else
#ifndef MODULE
static int s5_clkc_init(void)
{
	return platform_driver_register(&s5_driver);
}
arch_initcall_sync(s5_clkc_init);
#else
int __init meson_s5_clkc_init(void)
{
	return platform_driver_register(&s5_driver);
}
#endif
#endif

MODULE_LICENSE("GPL v2");
