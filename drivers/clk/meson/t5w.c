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
#include "t5w.h"
#include "clkcs_init.h"
#include <dt-bindings/clock/t5w-clkc.h>
#include "clk-secure.h"

static DEFINE_SPINLOCK(meson_clk_lock);

static const struct clk_ops meson_pll_clk_no_ops = {};

/*
 * the sys pll DCO value should be 3G~6G,
 * otherwise the sys pll can not lock.
 * od is for 32 bit.
 */

#ifdef CONFIG_ARM
static const struct pll_params_table t5w_sys_pll_params_table[] = {
	PLL_PARAMS(100, 1, 1), /*DCO=2400M OD=1200M*/
	PLL_PARAMS(116, 1, 1), /*DCO=2784 OD=1392M*/
	PLL_PARAMS(126, 1, 1), /*DCO=3024 OD=1512M*/
	PLL_PARAMS(67, 1, 0), /*DCO=1608M OD=1608MM*/
	PLL_PARAMS(71, 1, 0), /*DCO=1704MM OD=1704M*/
	PLL_PARAMS(75, 1, 0), /*DCO=1800M OD=1800M*/
	PLL_PARAMS(79, 1, 0), /*DCO=1896M OD=1896M*/
	PLL_PARAMS(159, 2, 0), /*DCO=1908M OD=1908M*/
	PLL_PARAMS(80, 1, 0), /*DCO=1920M OD=1920M*/
	PLL_PARAMS(83, 1, 0), /*DCO=1992M OD=1992M*/
	{ /* sentinel */ }
};
#else
static const struct pll_params_table t5w_sys_pll_params_table[] = {
	PLL_PARAMS(100, 1), /*DCO=2400M OD=1200M*/
	PLL_PARAMS(116, 1), /*DCO=2784 OD=1392M*/
	PLL_PARAMS(126, 1), /*DCO=3024 OD=1512M*/
	PLL_PARAMS(67, 1), /*DCO=1608M OD=1608MM*/
	PLL_PARAMS(71, 1), /*DCO=1704MM OD=1704M*/
	PLL_PARAMS(75, 1), /*DCO=1800M OD=1800M*/
	PLL_PARAMS(79, 1), /*DCO=1896M OD=1896M*/
	PLL_PARAMS(159, 2), /*DCO=1908M OD=1908M*/
	PLL_PARAMS(80, 1), /*DCO=1920M OD=1920M*/
	PLL_PARAMS(83, 1), /*DCO=1992M OD=1992M*/
	{ /* sentinel */ }
};
#endif

static struct clk_regmap t5w_sys_pll_dco = {
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
			.shift   = 16,
			.width   = 5,
		},
#ifdef CONFIG_ARM
		/* od for 32bit */
		.od = {
			.reg_off = HHI_SYS_PLL_CNTL0,
			.shift	 = 12,
			.width	 = 3,
		},
#endif
		.table = t5w_sys_pll_params_table,
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
		/*
		 * This clock feeds the CPU, avoid disabling it
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
static struct clk_regmap t5w_sys_pll = {
	.hw.init = &(struct clk_init_data){
		.name = "sys_pll",
		.ops = &meson_pll_clk_no_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_sys_pll_dco.hw
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
static struct clk_regmap t5w_sys_pll = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_SYS_PLL_CNTL0,
		.shift = 12,
		.width = 3,
		.flags = CLK_DIVIDER_POWER_OF_TWO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sys_pll",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_sys_pll_dco.hw
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
#endif

static struct clk_regmap t5w_fixed_pll_dco = {
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
		/*
		 * This clock feeds the sysytem, avoid disabling it
		 * Register has the risk of being directly operated
		 */
		.flags = CLK_IS_CRITICAL | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap t5w_fixed_pll = {
	.data = &(struct clk_regmap_div_data) {
		.offset = HHI_FIX_PLL_CNTL0,
		.shift = 16,
		.width = 2,
		.flags = CLK_DIVIDER_POWER_OF_TWO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "fixed_pll",
		.ops = &clk_regmap_divider_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_fixed_pll_dco.hw
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

static struct clk_fixed_factor t5w_fclk_div2_div = {
	.mult = 1,
	.div = 2,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div2_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) { &t5w_fixed_pll.hw },
		.num_parents = 1,
	},
};

static struct clk_regmap t5w_fclk_div2 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_FIX_PLL_CNTL1,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div2",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_fclk_div2_div.hw
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

static struct clk_fixed_factor t5w_fclk_div3_div = {
	.mult = 1,
	.div = 3,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div3_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) { &t5w_fixed_pll.hw },
		.num_parents = 1,
	},
};

static struct clk_regmap t5w_fclk_div3 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_FIX_PLL_CNTL1,
		.bit_idx = 20,
	},
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div3",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_fclk_div3_div.hw
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

static struct clk_fixed_factor t5w_fclk_div4_div = {
	.mult = 1,
	.div = 4,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div4_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) { &t5w_fixed_pll.hw },
		.num_parents = 1,
	},
};

static struct clk_regmap t5w_fclk_div4 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_FIX_PLL_CNTL1,
		.bit_idx = 21,
	},
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div4",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_fclk_div4_div.hw
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

static struct clk_fixed_factor t5w_fclk_div5_div = {
	.mult = 1,
	.div = 5,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div5_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) { &t5w_fixed_pll.hw },
		.num_parents = 1,
	},
};

static struct clk_regmap t5w_fclk_div5 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_FIX_PLL_CNTL1,
		.bit_idx = 22,
	},
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div5",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_fclk_div5_div.hw
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

static struct clk_fixed_factor t5w_fclk_div7_div = {
	.mult = 1,
	.div = 7,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div7_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) { &t5w_fixed_pll.hw },
		.num_parents = 1,
	},
};

static struct clk_regmap t5w_fclk_div7 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_FIX_PLL_CNTL1,
		.bit_idx = 23,
	},
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div7",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_fclk_div7_div.hw
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

static struct clk_fixed_factor t5w_fclk_div2p5_div = {
	.mult = 2,
	.div = 5,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div2p5_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_fixed_pll.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap t5w_fclk_div2p5 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_FIX_PLL_CNTL1,
		.bit_idx = 25,
	},
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div2p5",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_fclk_div2p5_div.hw
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

#ifdef CONFIG_ARM
static const struct pll_params_table t5w_gp0_pll_table[] = {
	PLL_PARAMS(141, 1, 2), /* DCO = 3384M OD = 2 PLL = 846M */
	PLL_PARAMS(132, 1, 2), /* DCO = 3168M OD = 2 PLL = 792M */
	PLL_PARAMS(248, 1, 3), /* DCO = 5952M OD = 3 PLL = 744M */
	PLL_PARAMS(128, 1, 2), /* DCO = 3072M OD = 2 PLL = 768M */
	PLL_PARAMS(192, 1, 2), /* DCO = 4608M OD = 4 PLL = 1152M */
	{ /* sentinel */  }
};
#else
static const struct pll_params_table t5w_gp0_pll_table[] = {
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
static const struct reg_sequence t5w_gp0_init_regs[] = {
	{ .reg = HHI_GP0_PLL_CNTL1,	.def = 0x00000000 },
	{ .reg = HHI_GP0_PLL_CNTL2,	.def = 0x00000180 },
	{ .reg = HHI_GP0_PLL_CNTL3,	.def = 0x4a681c00 },
	{ .reg = HHI_GP0_PLL_CNTL4,	.def = 0x88770290 },
	{ .reg = HHI_GP0_PLL_CNTL5,	.def = 0x3927200a },
	{ .reg = HHI_GP0_PLL_CNTL6,	.def = 0x56540000 }
};

static struct clk_regmap t5w_gp0_pll_dco = {
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
		/* for 32bit */
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
		.table = t5w_gp0_pll_table,
		.init_regs = t5w_gp0_init_regs,
		.init_count = ARRAY_SIZE(t5w_gp0_init_regs),
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
static struct clk_regmap t5w_gp0_pll = {
	.hw.init = &(struct clk_init_data){
		.name = "gp0_pll",
		.ops = &meson_pll_clk_no_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_gp0_pll_dco.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};
#else
static struct clk_regmap t5w_gp0_pll = {
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
			&t5w_gp0_pll_dco.hw
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
static const struct pll_params_table t5w_gp1_pll_table[] = {
	PLL_PARAMS(100, 1, 1), /*DCO=2400M OD=1200M*/
	PLL_PARAMS(116, 1, 1), /*DCO=2784M OD=1392M*/
	PLL_PARAMS(117, 1, 1), /*DCO=2808M OD=1404M*/
	PLL_PARAMS(125, 1, 1), /*DCO=3000M OD=1500M*/
	{ /* sentinel */  }
};
#else
static const struct pll_params_table t5w_gp1_pll_table[] = {
	/*
	 *  The DCO range of gp1pll on T5W is 1.6G - 3.2G
	 */
	PLL_PARAMS(100, 1), /*DCO=2400M OD=DCO/2=1200M*/
	PLL_PARAMS(116, 1), /*DCO=2784M OD=DCO/2=1392M*/
	PLL_PARAMS(117, 1), /*DCO=2808M OD=1404M */
	PLL_PARAMS(125, 1), /*DCO=3000M OD=DCO/2=1500M*/
	{ /* sentinel */  }
};
#endif

/*
 * Internal gp1 pll emulation configuration parameters
 */
static const struct reg_sequence t5w_gp1_init_regs[] = {
	//{ .reg = HHI_GP1_PLL_CNTL0,	.def = 0x00011000 },
	{ .reg = HHI_GP1_PLL_CNTL1,	.def = 0x1420500f },
	{ .reg = HHI_GP1_PLL_CNTL2,	.def = 0x00023001 },
	{ .reg = HHI_GP1_PLL_CNTL3,	.def = 0x0 },
};

static struct clk_regmap t5w_gp1_pll_dco = {
	.data = &(struct meson_clk_pll_data){
		.en = {
			.reg_off = HHI_GP1_PLL_CNTL0,
			.shift   = 28,
			.width   = 1,
		},
		.n = {
			.reg_off = HHI_GP1_PLL_CNTL0,
			.shift   = 16,
			.width   = 5,
		},
		.m = {
			.reg_off = HHI_GP1_PLL_CNTL0,
			.shift   = 0,
			.width   = 8,
		},
#ifdef CONFIG_ARM
		.od = {
			.reg_off = HHI_GP1_PLL_CNTL0,
			.shift   = 12,
			.width   = 3,
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
		.table = t5w_gp1_pll_table,
		/* GP1 has been opened under the bootloader,
		 * do not repeat the operation here, otherwise gp1
		 * may be closed
		 */
		.init_regs = t5w_gp1_init_regs,
		.init_count = ARRAY_SIZE(t5w_gp1_init_regs),
	},
	.hw.init = &(struct clk_init_data){
		.name = "gp1_pll_dco",
		.ops = &meson_clk_pll_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "xtal",
		},
		.num_parents = 1,
		/*
		 * Register has the risk of being directly operated
		 */
		.flags = CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED,
	},
};

#ifdef CONFIG_ARM
static struct clk_regmap t5w_gp1_pll = {
	.hw.init = &(struct clk_init_data){
		.name = "gp1_pll",
		.ops = &meson_pll_clk_no_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_gp1_pll_dco.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};
#else
static struct clk_regmap t5w_gp1_pll = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_GP1_PLL_CNTL0,
		.shift = 12,
		.width = 3,
		.flags = CLK_DIVIDER_POWER_OF_TWO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "gp1_pll",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_gp1_pll_dco.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
	},
};
#endif

#ifdef CONFIG_ARM
static const struct pll_params_table t5w_hifi_pll_table[] = {
	PLL_PARAMS(150, 1, 1), /* DCO = 1806.336M OD = 1 */
	{ /* sentinel */  }
};
#else
static const struct pll_params_table t5w_hifi_pll_table[] = {
	PLL_PARAMS(150, 1), /* DCO = 1806.336M */
	{ /* sentinel */  }
};
#endif

/*
 * Internal hifi pll emulation configuration parameters
 */
static const struct reg_sequence t5w_hifi_init_regs[] = {
	{ .reg = HHI_HIFI_PLL_CNTL1,	.def = 0x00010e56 },
	{ .reg = HHI_HIFI_PLL_CNTL2,	.def = 0x00000000 },
	{ .reg = HHI_HIFI_PLL_CNTL3,	.def = 0x6a285c00 },
	{ .reg = HHI_HIFI_PLL_CNTL4,	.def = 0x65771290 },
	{ .reg = HHI_HIFI_PLL_CNTL5,	.def = 0x3927200a },
	{ .reg = HHI_HIFI_PLL_CNTL6,	.def = 0x56540000 }
};

static struct clk_regmap t5w_hifi_pll_dco = {
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
		.table = t5w_hifi_pll_table,
		.init_regs = t5w_hifi_init_regs,
		.init_count = ARRAY_SIZE(t5w_hifi_init_regs),
		.flags = CLK_MESON_PLL_ROUND_CLOSEST,
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

static struct clk_regmap t5w_hifi_pll = {
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
			&t5w_hifi_pll_dco.hw
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

static struct clk_fixed_factor t5w_mpll_50m_div = {
	.mult = 1,
	.div = 80,
	.hw.init = &(struct clk_init_data){
		.name = "mpll_50m_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_fixed_pll_dco.hw
		},
		.num_parents = 1,
	},
};

static const struct clk_parent_data t5w_mpll_50m_sel[] = {
	{ .fw_name = "xtal", },
	{ .hw = &t5w_mpll_50m_div.hw }
};

static struct clk_regmap t5w_mpll_50m = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_FIX_PLL_CNTL3,
		.mask = 0x1,
		.shift = 5,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpll_50m",
		.ops = &clk_regmap_mux_ro_ops,
		.parent_data = t5w_mpll_50m_sel,
		.num_parents = ARRAY_SIZE(t5w_mpll_50m_sel),
	},
};

static struct clk_fixed_factor t5w_mpll_prediv = {
	.mult = 1,
	.div = 2,
	.hw.init = &(struct clk_init_data){
		.name = "mpll_prediv",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_fixed_pll_dco.hw
		},
		.num_parents = 1,
	},
};

static const struct reg_sequence t5w_mpll0_init_regs[] = {
	{ .reg = HHI_MPLL_CNTL2, .def = 0x40000033 }
};

static struct clk_regmap t5w_mpll0_div = {
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
		.init_regs = t5w_mpll0_init_regs,
		.init_count = ARRAY_SIZE(t5w_mpll0_init_regs),
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpll0_div",
		.ops = &meson_clk_mpll_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_mpll_prediv.hw
		},
		.num_parents = 1,
		/*
		 * Register has the risk of being directly operated.
		 */
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap t5w_mpll0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_MPLL_CNTL1,
		.bit_idx = 31,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpll0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &t5w_mpll0_div.hw },
		.num_parents = 1,
		/*
		 * mpll0 is directly used in other modules, and the
		 * parent rate needs to be modified
		 * Register has the risk of being directly operated.
		 */
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
	},
};

static const struct reg_sequence t5w_mpll1_init_regs[] = {
	{ .reg = HHI_MPLL_CNTL4, .def = 0x40000033 }
};

static struct clk_regmap t5w_mpll1_div = {
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
		.init_regs = t5w_mpll1_init_regs,
		.init_count = ARRAY_SIZE(t5w_mpll1_init_regs),
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpll1_div",
		.ops = &meson_clk_mpll_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_mpll_prediv.hw
		},
		.num_parents = 1,
		/*
		 * Register has the risk of being directly operated.
		 */
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap t5w_mpll1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_MPLL_CNTL3,
		.bit_idx = 31,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpll1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &t5w_mpll1_div.hw },
		.num_parents = 1,
		/*
		 * mpll1 is directly used in other modules, and the
		 * parent rate needs to be modified
		 * Register has the risk of being directly operated.
		 */
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
	},
};

static const struct reg_sequence t5w_mpll2_init_regs[] = {
	{ .reg = HHI_MPLL_CNTL6, .def = 0x40000033 }
};

static struct clk_regmap t5w_mpll2_div = {
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
		.init_regs = t5w_mpll2_init_regs,
		.init_count = ARRAY_SIZE(t5w_mpll2_init_regs),
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpll2_div",
		.ops = &meson_clk_mpll_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_mpll_prediv.hw
		},
		/*
		 * Register has the risk of being directly operated.
		 */
		.num_parents = 1,
	},
};

static struct clk_regmap t5w_mpll2 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_MPLL_CNTL5,
		.bit_idx = 31,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpll2",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &t5w_mpll2_div.hw },
		.num_parents = 1,
		/*
		 * mpll2 is directly used in other modules, and the
		 * parent rate needs to be modified
		 * Register has the risk of being directly operated.
		 */
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
	},
};

static const struct reg_sequence t5w_mpll3_init_regs[] = {
	{ .reg = HHI_MPLL_CNTL8, .def = 0x40000033 }
};

static struct clk_regmap t5w_mpll3_div = {
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
		.init_regs = t5w_mpll3_init_regs,
		.init_count = ARRAY_SIZE(t5w_mpll3_init_regs),
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpll3_div",
		.ops = &meson_clk_mpll_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_mpll_prediv.hw
		},
		/*
		 * Register has the risk of being directly operated.
		 */
		.num_parents = 1,
	},
};

static struct clk_regmap t5w_mpll3 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_MPLL_CNTL7,
		.bit_idx = 31,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpll3",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &t5w_mpll3_div.hw },
		.num_parents = 1,
		/*
		 * mpll3 is directly used in other modules, and the
		 * parent rate needs to be modified
		 * Register has the risk of being directly operated.
		 */
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
	},
};

/* Datasheet names this field as "premux0" */
static struct clk_regmap t5w_cpu_clk_premux0 = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_SYS_CPU_CLK_CNTL,
		.mask = 0x3,
		.shift = 0,
		.flags = CLK_MUX_ROUND_CLOSEST,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cpu_clk_dyn0_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = (const struct clk_parent_data []) {
			{ .fw_name = "xtal", },
			{ .hw = &t5w_fclk_div2.hw },
			{ .hw = &t5w_fclk_div3.hw },
		},
		.num_parents = 3,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/* Datasheet names this field as "premux1" */
static struct clk_regmap t5w_cpu_clk_premux1 = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_SYS_CPU_CLK_CNTL,
		.mask = 0x3,
		.shift = 16,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cpu_clk_dyn1_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = (const struct clk_parent_data []) {
			{ .fw_name = "xtal", },
			{ .hw = &t5w_fclk_div2.hw },
			{ .hw = &t5w_fclk_div3.hw },
		},
		.num_parents = 3,
		/* This sub-tree is used a parking clock */
		.flags = CLK_SET_RATE_NO_REPARENT
	},
};

/* Datasheet names this field as "mux0_divn_tcnt" */
static struct clk_regmap t5w_cpu_clk_mux0_div = {
	.data = &(struct meson_clk_cpu_dyndiv_data){
		.div = {
			.reg_off = HHI_SYS_CPU_CLK_CNTL,
			.shift = 4,
			.width = 6,
		},
		.dyn = {
			.reg_off = HHI_SYS_CPU_CLK_CNTL,
			.shift = 26,
			.width = 1,
		},
	},
	.hw.init = &(struct clk_init_data){
		.name = "cpu_clk_dyn0_div",
		.ops = &meson_clk_cpu_dyndiv_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_cpu_clk_premux0.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/* Datasheet names this field as "postmux0" */
static struct clk_regmap t5w_cpu_clk_postmux0 = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_SYS_CPU_CLK_CNTL,
		.mask = 0x1,
		.shift = 2,
		.flags = CLK_MUX_ROUND_CLOSEST,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cpu_clk_dyn0",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_cpu_clk_premux0.hw,
			&t5w_cpu_clk_mux0_div.hw,
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/* Datasheet names this field as "Mux1_divn_tcnt" */
static struct clk_regmap t5w_cpu_clk_mux1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_SYS_CPU_CLK_CNTL,
		.shift = 20,
		.width = 6,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cpu_clk_dyn1_div",
		.ops = &clk_regmap_divider_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_cpu_clk_premux1.hw
		},
		.num_parents = 1,
	},
};

/* Datasheet names this field as "postmux1" */
static struct clk_regmap t5w_cpu_clk_postmux1 = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_SYS_CPU_CLK_CNTL,
		.mask = 0x1,
		.shift = 18,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cpu_clk_dyn1",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_cpu_clk_premux1.hw,
			&t5w_cpu_clk_mux1_div.hw,
		},
		.num_parents = 2,
		/* This sub-tree is used a parking clock */
		.flags = CLK_SET_RATE_NO_REPARENT,
	},
};

/* Datasheet names this field as "Final_dyn_mux_sel" */
static struct clk_regmap t5w_cpu_clk_dyn = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_SYS_CPU_CLK_CNTL,
		.mask = 0x1,
		.shift = 10,
		.flags = CLK_MUX_ROUND_CLOSEST,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cpu_clk_dyn",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_cpu_clk_postmux0.hw,
			&t5w_cpu_clk_postmux1.hw,
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/* Datasheet names this field as "premux0" */
static struct clk_regmap t5w_dsu_clk_premux0 = {
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
			{ .hw = &t5w_fclk_div2.hw },
			{ .hw = &t5w_fclk_div3.hw },
			{ .hw = &t5w_gp1_pll.hw },
		},
		.num_parents = 4,
		.flags = CLK_SET_RATE_PARENT | CLK_SET_RATE_NO_REPARENT,
	},
};

/* Datasheet names this field as "premux1" */
static struct clk_regmap t5w_dsu_clk_premux1 = {
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
			{ .hw = &t5w_fclk_div2.hw },
			{ .hw = &t5w_fclk_div3.hw },
			{ .hw = &t5w_gp1_pll.hw },
		},
		.num_parents = 4,
		.flags = CLK_SET_RATE_PARENT | CLK_SET_RATE_NO_REPARENT,
	},
};

/* Datasheet names this field as "Mux0_divn_tcnt" */
static struct clk_regmap t5w_dsu_clk_mux0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_SYS_CPU_CLK_CNTL5,
		.shift = 4,
		.width = 6,
	},
	.hw.init = &(struct clk_init_data){
		.name = "dsu_clk_dyn0_div",
		.ops = &clk_regmap_divider_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_dsu_clk_premux0.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/* Datasheet names this field as "postmux0" */
static struct clk_regmap t5w_dsu_clk_postmux0 = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_SYS_CPU_CLK_CNTL5,
		.mask = 0x1,
		.shift = 2,
	},
	.hw.init = &(struct clk_init_data){
		.name = "dsu_clk_dyn0",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_dsu_clk_premux0.hw,
			&t5w_dsu_clk_mux0_div.hw,
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/* Datasheet names this field as "Mux1_divn_tcnt" */
static struct clk_regmap t5w_dsu_clk_mux1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_SYS_CPU_CLK_CNTL5,
		.shift = 20,
		.width = 6,
	},
	.hw.init = &(struct clk_init_data){
		.name = "dsu_clk_dyn1_div",
		.ops = &clk_regmap_divider_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_dsu_clk_premux1.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/* Datasheet names this field as "postmux1" */
static struct clk_regmap t5w_dsu_clk_postmux1 = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_SYS_CPU_CLK_CNTL5,
		.mask = 0x1,
		.shift = 18,
	},
	.hw.init = &(struct clk_init_data){
		.name = "dsu_clk_dyn1",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_dsu_clk_premux1.hw,
			&t5w_dsu_clk_mux1_div.hw,
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/* Datasheet names this field as "Final_dyn_mux_sel" */
static struct clk_regmap t5w_dsu_clk_dyn = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_SYS_CPU_CLK_CNTL5,
		.mask = 0x1,
		.shift = 10,
	},
	.hw.init = &(struct clk_init_data){
		.name = "dsu_clk_dyn",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_dsu_clk_postmux0.hw,
			&t5w_dsu_clk_postmux1.hw,
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT | CLK_SET_RATE_NO_REPARENT,
	},
};

/* Datasheet names this field as "Final_mux_sel" */
static struct clk_regmap t5w_dsu_final_clk = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_SYS_CPU_CLK_CNTL5,
		.mask = 0x1,
		.shift = 11,
	},
	.hw.init = &(struct clk_init_data){
		.name = "dsu_clk_final",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_dsu_clk_dyn.hw,
			&t5w_sys_pll.hw,
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT | CLK_SET_RATE_NO_REPARENT,
	},
};

/* Datasheet names this field as "Final_mux_sel" */
static struct clk_regmap t5w_cpu_clk = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_SYS_CPU_CLK_CNTL,
		.mask = 0x1,
		.shift = 11,
		.flags = CLK_MUX_ROUND_CLOSEST,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cpu_clk",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_cpu_clk_dyn.hw,
			&t5w_sys_pll.hw,
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t5w_dsu_clk = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_SYS_CPU_CLK_CNTL6,
		.mask = 0x1,
		.shift = 27,
	},
	.hw.init = &(struct clk_init_data){
		.name = "dsu_clk",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_cpu_clk.hw,
			&t5w_dsu_final_clk.hw,
		},
		.num_parents = 2,
	},
};

struct t5w_cpu_clk_postmux_nb_data {
	struct notifier_block nb;
	struct clk_hw *fclk_div2;
	struct clk_hw *cpu_clk_dyn;
	struct clk_hw *cpu_clk_postmux0;
	struct clk_hw *cpu_clk_postmux1;
	struct clk_hw *cpu_clk_premux1;
};

static int t5w_cpu_clk_postmux_notifier_cb(struct notifier_block *nb,
					   unsigned long event, void *data)
{
	struct t5w_cpu_clk_postmux_nb_data *nb_data =
		container_of(nb, struct t5w_cpu_clk_postmux_nb_data, nb);

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
		 * Now, cpu_clk is fclk_div2 in the current path :
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

static struct t5w_cpu_clk_postmux_nb_data t5w_cpu_clk_postmux0_nb_data = {
	.cpu_clk_dyn = &t5w_cpu_clk_dyn.hw,
	.cpu_clk_postmux0 = &t5w_cpu_clk_postmux0.hw,
	.cpu_clk_postmux1 = &t5w_cpu_clk_postmux1.hw,
	.cpu_clk_premux1 = &t5w_cpu_clk_premux1.hw,
	.nb.notifier_call = t5w_cpu_clk_postmux_notifier_cb,
};

struct t5w_sys_pll_nb_data {
	struct notifier_block nb;
	struct clk_hw *sys_pll;
	struct clk_hw *cpu_clk;
	struct clk_hw *cpu_dyn_clk;
};

static int t5w_sys_pll_notifier_cb(struct notifier_block *nb,
				   unsigned long event, void *data)
{
	struct t5w_sys_pll_nb_data *nb_data =
		container_of(nb, struct t5w_sys_pll_nb_data, nb);

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

static struct t5w_sys_pll_nb_data t5w_sys_pll_nb_data = {
	.sys_pll = &t5w_sys_pll.hw,
	.cpu_clk = &t5w_cpu_clk.hw,
	.cpu_dyn_clk = &t5w_cpu_clk_dyn.hw,
	.nb.notifier_call = t5w_sys_pll_notifier_cb,
};

static u32 mux_table_clk81[]	= { 6, 5, 7 };

static const struct clk_hw *clk81_parent_hws[] = {
	&t5w_fclk_div3.hw,
	&t5w_fclk_div4.hw,
	&t5w_fclk_div5.hw,
};

static struct clk_regmap t5w_mpeg_clk_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_MPEG_CLK_CNTL,
		.mask = 0x7,
		.shift = 12,
		.table = mux_table_clk81,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpeg_clk_sel",
		.ops = &clk_regmap_mux_ro_ops,
		.parent_hws = clk81_parent_hws,
		.num_parents = ARRAY_SIZE(clk81_parent_hws),
	},
};

static struct clk_regmap t5w_mpeg_clk_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_MPEG_CLK_CNTL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpeg_clk_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_mpeg_clk_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t5w_clk81 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_MPEG_CLK_CNTL,
		.bit_idx = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "clk81",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_mpeg_clk_div.hw
		},
		.num_parents = 1,
		.flags = (CLK_SET_RATE_PARENT | CLK_IS_CRITICAL),
	},
};

/* cts_vdec_clk */
static const struct clk_parent_data t5w_dec_parent_hws[] = {
	{ .hw = &t5w_fclk_div2p5.hw },
	{ .hw = &t5w_fclk_div3.hw },
	{ .hw = &t5w_fclk_div4.hw },
	{ .hw = &t5w_fclk_div5.hw },
	{ .hw = &t5w_fclk_div7.hw },
	{ .hw = &t5w_hifi_pll.hw },
	{ .hw = &t5w_gp0_pll.hw },
	{ .fw_name = "xtal", }
};

static struct clk_regmap t5w_vdec_0_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VDEC_CLK_CNTL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vdec_0_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t5w_dec_parent_hws,
		.num_parents = ARRAY_SIZE(t5w_dec_parent_hws),
	},
};

static struct clk_regmap t5w_vdec_0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VDEC_CLK_CNTL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vdec_0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_vdec_0_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t5w_vdec_0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VDEC_CLK_CNTL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vdec_0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_vdec_0_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t5w_vdec_1_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VDEC3_CLK_CNTL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vdec_1_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t5w_dec_parent_hws,
		.num_parents = ARRAY_SIZE(t5w_dec_parent_hws),
	},
};

static struct clk_regmap t5w_vdec_1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VDEC3_CLK_CNTL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vdec_1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_vdec_1_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t5w_vdec_1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VDEC3_CLK_CNTL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vdec_1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_vdec_1_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t5w_vdec = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VDEC3_CLK_CNTL,
		.mask = 0x1,
		.shift = 15,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vdec",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_vdec_0.hw,
			&t5w_vdec_1.hw
		},
		.num_parents = 2,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t5w_hevcf_0_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VDEC2_CLK_CNTL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hevcf_0_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t5w_dec_parent_hws,
		.num_parents = ARRAY_SIZE(t5w_dec_parent_hws),
	},
};

static struct clk_regmap t5w_hevcf_0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VDEC2_CLK_CNTL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hevcf_0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_hevcf_0_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t5w_hevcf_0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VDEC2_CLK_CNTL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hevcf_0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_hevcf_0_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t5w_hevcf_1_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VDEC4_CLK_CNTL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hevcf_1_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t5w_dec_parent_hws,
		.num_parents = ARRAY_SIZE(t5w_dec_parent_hws),
	},
};

static struct clk_regmap t5w_hevcf_1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VDEC4_CLK_CNTL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hevcf_1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_hevcf_1_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t5w_hevcf_1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VDEC4_CLK_CNTL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hevcf_1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_hevcf_1_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t5w_hevcf = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VDEC4_CLK_CNTL,
		.mask = 0x1,
		.shift = 15,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hevcf",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_hevcf_0.hw,
			&t5w_hevcf_1.hw
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static u32 mux_table_ge2d_sel[] = {0, 1, 2, 3, 4, 6, 7};
static const struct clk_hw *t5w_ge2d_parent_hws[] = {
	&t5w_fclk_div4.hw,
	&t5w_fclk_div3.hw,
	&t5w_fclk_div5.hw,
	&t5w_fclk_div7.hw,
	&t5w_mpll1.hw,
	&t5w_mpll2.hw,
	&t5w_fclk_div2p5.hw
};

static struct clk_regmap t5w_ge2d_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_GE2DCLK_CNTL,
		.mask = 0x7,
		.shift = 9,
		.table = mux_table_ge2d_sel
	},
	.hw.init = &(struct clk_init_data) {
		.name = "ge2d_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = t5w_ge2d_parent_hws,
		.num_parents = ARRAY_SIZE(t5w_ge2d_parent_hws),
	},
};

static struct clk_regmap t5w_ge2d_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_GE2DCLK_CNTL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "ge2d_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_ge2d_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t5w_ge2d = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_GE2DCLK_CNTL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "ge2d",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_ge2d_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t5w_vapb_0_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VAPBCLK_CNTL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vapb_0_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = t5w_ge2d_parent_hws,
		.num_parents = ARRAY_SIZE(t5w_ge2d_parent_hws),
	},
};

static struct clk_regmap t5w_vapb_0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VAPBCLK_CNTL,
		.shift = 0,
		.width = 7,
		.flags = CLK_DIVIDER_ALLOW_ZERO
	},
	.hw.init = &(struct clk_init_data){
		.name = "vapb_0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_vapb_0_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t5w_vapb_0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VAPBCLK_CNTL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vapb_0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_vapb_0_div.hw
		},
		.num_parents = 1,
		/*
		 * vapb clk is used for display module, vpu driver is a KO,
		 * It is too late to enable to clk again.
		 * add CLK_IGNORE_UNUSED to avoid display abnormal
		 */
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED
	},
};

static struct clk_regmap t5w_vapb_1_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VAPBCLK_CNTL,
		.mask = 0x7,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vapb_1_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = t5w_ge2d_parent_hws,
		.num_parents = ARRAY_SIZE(t5w_ge2d_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap t5w_vapb_1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VAPBCLK_CNTL,
		.shift = 16,
		.width = 7,
		.flags = CLK_DIVIDER_ALLOW_ZERO
	},
	.hw.init = &(struct clk_init_data){
		.name = "vapb_1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_vapb_1_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t5w_vapb_1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VAPBCLK_CNTL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vapb_1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_vapb_1_div.hw
		},
		.num_parents = 1,
		/*
		 * vapb clk is used for display module, vpu driver is a KO,
		 * It is too late to enable to clk again.
		 * add CLK_IGNORE_UNUSED to avoid display abnormal
		 */
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED
	},
};

static struct clk_regmap t5w_vapb = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VAPBCLK_CNTL,
		.mask = 0x1,
		.shift = 31,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vapb_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_vapb_0.hw,
			&t5w_vapb_1.hw
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
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
static u32 mux_table_mali[] = { 0, 3, 4, 5, 6, 7};

static const struct clk_parent_data t5w_mali_0_1_parent_data[] = {
	{ .fw_name = "xtal", },
	{ .hw = &t5w_fclk_div2p5.hw },
	{ .hw = &t5w_fclk_div3.hw },
	{ .hw = &t5w_fclk_div4.hw },
	{ .hw = &t5w_fclk_div5.hw },
	{ .hw = &t5w_fclk_div7.hw },
};

static struct clk_regmap t5w_mali_0_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_MALI_CLK_CNTL,
		.mask = 0x7,
		.shift = 9,
		.table = mux_table_mali,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mali_0_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t5w_mali_0_1_parent_data,
		.num_parents = ARRAY_SIZE(t5w_mali_0_1_parent_data),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap t5w_mali_0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_MALI_CLK_CNTL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mali_0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_mali_0_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t5w_mali_0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_MALI_CLK_CNTL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mali_0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_mali_0_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_GATE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t5w_mali_1_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_MALI_CLK_CNTL,
		.mask = 0x7,
		.shift = 25,
		.table = mux_table_mali,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mali_1_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t5w_mali_0_1_parent_data,
		.num_parents = ARRAY_SIZE(t5w_mali_0_1_parent_data),
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t5w_mali_1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_MALI_CLK_CNTL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mali_1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_mali_1_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t5w_mali_1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_MALI_CLK_CNTL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mali_1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_mali_1_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_GATE | CLK_SET_RATE_PARENT,
	},
};

static const struct clk_hw *t5w_mali_parent_hws[] = {
	&t5w_mali_0.hw,
	&t5w_mali_1.hw
};

static struct clk_regmap t5w_mali = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_MALI_CLK_CNTL,
		.mask = 1,
		.shift = 31,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mali",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = t5w_mali_parent_hws,
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/*ts clk*/
static struct clk_regmap t5w_ts_clk_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_TS_CLK_CNTL,
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

static struct clk_regmap t5w_ts_clk = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_TS_CLK_CNTL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "ts_clk",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_ts_clk_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static const struct clk_parent_data t5w_sd_emmc_parent_data[] = {
	{ .fw_name = "xtal", },
	{ .hw = &t5w_fclk_div2.hw },
	{ .hw = &t5w_fclk_div3.hw },
	{ .hw = &t5w_fclk_div5.hw },
	{ .hw = &t5w_fclk_div7.hw },
	{ .hw = &t5w_mpll2.hw },
	{ .hw = &t5w_mpll3.hw },
	{ .hw = &t5w_gp0_pll.hw }
};

static struct clk_regmap t5w_sd_emmc_c_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_NAND_CLK_CNTL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_c_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t5w_sd_emmc_parent_data,
		.num_parents = ARRAY_SIZE(t5w_sd_emmc_parent_data),
		.flags = CLK_GET_RATE_NOCACHE
	},
};

static struct clk_regmap t5w_sd_emmc_c_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_NAND_CLK_CNTL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_c_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_sd_emmc_c_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t5w_sd_emmc_c = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_NAND_CLK_CNTL,
		.bit_idx = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sd_emmc_c",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_sd_emmc_c_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t5w_sd_emmc_b_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_SD_EMMC_CLK_CNTL,
		.mask = 0x7,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_b_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t5w_sd_emmc_parent_data,
		.num_parents = ARRAY_SIZE(t5w_sd_emmc_parent_data),
		.flags = CLK_GET_RATE_NOCACHE
	},
};

static struct clk_regmap t5w_sd_emmc_b_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_SD_EMMC_CLK_CNTL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_b_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_sd_emmc_b_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t5w_sd_emmc_b = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_SD_EMMC_CLK_CNTL,
		.bit_idx = 23,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sd_emmc_b",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_sd_emmc_b_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE |	CLK_SET_RATE_PARENT
	},
};

/* cts_vdin_meas_clk */
static u32 t5w_vdin_meas_table[] = {0, 1, 2, 3};
static const struct clk_parent_data t5w_vdin_parent_hws[] = {
	{ .fw_name = "xtal", },
	{ .hw = &t5w_fclk_div4.hw },
	{ .hw = &t5w_fclk_div3.hw },
	{ .hw = &t5w_fclk_div5.hw },
};

static struct clk_regmap t5w_vdin_meas_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VDIN_MEAS_CLK_CNTL,
		.mask = 0x7,
		.shift = 9,
		.table = t5w_vdin_meas_table,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vdin_meas_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t5w_vdin_parent_hws,
		.num_parents = ARRAY_SIZE(t5w_vdin_parent_hws),
	},
};

static struct clk_regmap t5w_vdin_meas_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VDIN_MEAS_CLK_CNTL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vdin_meas_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_vdin_meas_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t5w_vdin_meas = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VDIN_MEAS_CLK_CNTL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vdin_meas",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_vdin_meas_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t5w_vid_lock_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VID_LOCK_CLK_CNTL,
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

static struct clk_regmap t5w_vid_lock_clk  = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VID_LOCK_CLK_CNTL,
		.bit_idx = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vid_lock_clk",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_vid_lock_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static const struct clk_hw *t5w_vpu_parent_hws[] = {
	&t5w_fclk_div3.hw,
	&t5w_fclk_div4.hw,
	&t5w_fclk_div5.hw,
	&t5w_fclk_div7.hw
};

static struct clk_regmap t5w_vpu_0_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VPU_CLK_CTRL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vpu_0_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = t5w_vpu_parent_hws,
		.num_parents = ARRAY_SIZE(t5w_vpu_parent_hws),
	},
};

static struct clk_regmap t5w_vpu_0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VPU_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vpu_0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) { &t5w_vpu_0_sel.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t5w_vpu_0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VPU_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &t5w_vpu_0_div.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap t5w_vpu_1_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VPU_CLK_CTRL,
		.mask = 0x3,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vpu_1_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = t5w_vpu_parent_hws,
		.num_parents = ARRAY_SIZE(t5w_vpu_parent_hws),
	},
};

static struct clk_regmap t5w_vpu_1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VPU_CLK_CTRL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vpu_1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) { &t5w_vpu_1_sel.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t5w_vpu_1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VPU_CLK_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &t5w_vpu_1_div.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap t5w_vpu = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VPU_CLK_CTRL,
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
			&t5w_vpu_0.hw,
			&t5w_vpu_1.hw
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_NO_REPARENT,
	},
};

static const struct clk_hw *vpu_clkb_tmp_parent_hws[] = {
	&t5w_vpu.hw,
	&t5w_fclk_div4.hw,
	&t5w_fclk_div5.hw,
	&t5w_fclk_div7.hw
};

static struct clk_regmap t5w_vpu_clkb_tmp_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VPU_CLKB_CTRL,
		.mask = 0x3,
		.shift = 20,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_clkb_tmp_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = vpu_clkb_tmp_parent_hws,
		.num_parents = ARRAY_SIZE(vpu_clkb_tmp_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap t5w_vpu_clkb_tmp_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VPU_CLKB_CTRL,
		.shift = 16,
		.width = 4,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_clkb_tmp_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_vpu_clkb_tmp_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t5w_vpu_clkb_tmp = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VPU_CLKB_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vpu_clkb_tmp",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_vpu_clkb_tmp_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t5w_vpu_clkb_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VPU_CLKB_CTRL,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_clkb_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_vpu_clkb_tmp.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t5w_vpu_clkb = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VPU_CLKB_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vpu_clkb",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_vpu_clkb_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static const struct clk_hw *vpu_clkc_parent_hws[] = {
	&t5w_fclk_div4.hw,
	&t5w_fclk_div3.hw,
	&t5w_fclk_div5.hw,
	&t5w_fclk_div7.hw
};

static struct clk_regmap t5w_vpu_clkc_0_sel  = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VPU_CLKC_CTRL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_clkc_0_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = vpu_clkc_parent_hws,
		.num_parents = ARRAY_SIZE(vpu_clkc_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap t5w_vpu_clkc_0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VPU_CLKC_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_clkc_0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_vpu_clkc_0_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t5w_vpu_clkc_0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VPU_CLKC_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vpu_clkc_0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_vpu_clkc_0_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t5w_vpu_clkc_1_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VPU_CLKC_CTRL,
		.mask = 0x3,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_clkc_1_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = vpu_clkc_parent_hws,
		.num_parents = ARRAY_SIZE(vpu_clkc_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap t5w_vpu_clkc_1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VPU_CLKC_CTRL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_clkc_1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_vpu_clkc_1_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t5w_vpu_clkc_1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VPU_CLKC_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vpu_clkc_1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_vpu_clkc_1_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t5w_vpu_clkc = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VPU_CLKC_CTRL,
		.mask = 0x1,
		.shift = 31,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_clkc",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_vpu_clkc_0.hw,
			&t5w_vpu_clkc_1.hw
		},
		.num_parents = 2,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

/*cts tcon pll clk*/
static const struct clk_parent_data t5w_tcon_pll_parent_data[] = {
	{ .fw_name = "xtal", },
	{ .hw = &t5w_fclk_div5.hw },
	{ .hw = &t5w_fclk_div4.hw },
	{ .hw = &t5w_fclk_div3.hw },
	{ .hw = &t5w_mpll2.hw },
	{ .hw = &t5w_mpll3.hw },
};

static struct clk_regmap t5w_cts_tcon_pll_clk_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_TCON_CLK_CNTL,
		.mask = 0x7,
		.shift = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "cts_tcon_pll_clk_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t5w_tcon_pll_parent_data,
		.num_parents = ARRAY_SIZE(t5w_tcon_pll_parent_data),
	},
};

static struct clk_regmap t5w_cts_tcon_pll_clk_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_TCON_CLK_CNTL,
		.shift = 0,
		.width = 6,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "cts_tcon_pll_clk_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_cts_tcon_pll_clk_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t5w_cts_tcon_pll_clk = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_TCON_CLK_CNTL,
		.bit_idx = 6,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cts_tcon_pll_clk",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_cts_tcon_pll_clk_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

/*adc extclk in clkl*/
static const struct clk_parent_data t5w_adc_extclk_parent_data[] = {
	{ .fw_name = "xtal", },
	{ .hw = &t5w_fclk_div4.hw },
	{ .hw = &t5w_fclk_div3.hw },
	{ .hw = &t5w_fclk_div5.hw },
	{ .hw = &t5w_fclk_div7.hw },
	{ .hw = &t5w_mpll2.hw },
	{ .hw = &t5w_gp0_pll.hw },
	{ .hw = &t5w_hifi_pll.hw }
};

static struct clk_regmap t5w_adc_extclk_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_DEMOD_CLK_CNTL,
		.mask = 0x7,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "adc_extclk_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t5w_adc_extclk_parent_data,
		.num_parents = ARRAY_SIZE(t5w_adc_extclk_parent_data),
	},
};

static struct clk_regmap t5w_adc_extclk_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_DEMOD_CLK_CNTL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "adc_extclk_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_adc_extclk_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t5w_adc_extclk = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_DEMOD_CLK_CNTL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "adc_extclk",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_adc_extclk_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

/*cts demod core*/
static const struct clk_parent_data t5w_demod_parent_data[] = {
	{ .fw_name = "xtal", },
	{ .hw = &t5w_fclk_div7.hw },
	{ .hw = &t5w_fclk_div4.hw },
	//todo: need add adc_dpll_intclk
};

static struct clk_regmap t5w_cts_demod_core_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_DEMOD_CLK_CNTL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "cts_demod_core_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t5w_demod_parent_data,
		.num_parents = ARRAY_SIZE(t5w_demod_parent_data),
	},
};

static struct clk_regmap t5w_cts_demod_core_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_DEMOD_CLK_CNTL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "cts_demod_core_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_cts_demod_core_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t5w_cts_demod_core = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_DEMOD_CLK_CNTL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cts_demod_core",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_cts_demod_core_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

/*cts demod core t2 clk*/
static struct clk_regmap t5w_cts_demod_core_t2_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_DEMOD_CLK_CNTL1,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "cts_demod_core_t2_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t5w_demod_parent_data,
		.num_parents = ARRAY_SIZE(t5w_demod_parent_data),
	},
};

static struct clk_regmap t5w_cts_demod_core_t2_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_DEMOD_CLK_CNTL1,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "cts_demod_core_t2_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_cts_demod_core_t2_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t5w_cts_demod_core_t2 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_DEMOD_CLK_CNTL1,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cts_demod_core_t2",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_cts_demod_core_t2_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t5w_demod_32k_clkin = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_DEMOD_32K_CNTL0,
		.bit_idx = 31,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "demod_32k_clkin",
		.ops = &clk_regmap_gate_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "xtal",
		},
		.num_parents = 1,
	},
};

static const struct meson_clk_dualdiv_param t5w_demod_32k_div_table[] = {
	{
		.dual	= 0,
		.n1	= 733,
	},
	{}
};

static struct clk_regmap t5w_demod_32k_div = {
	.data = &(struct meson_clk_dualdiv_data){
		.n1 = {
			.reg_off = HHI_DEMOD_32K_CNTL0,
			.shift   = 0,
			.width   = 12,
		},
		.n2 = {
			.reg_off = HHI_DEMOD_32K_CNTL0,
			.shift   = 12,
			.width   = 12,
		},
		.m1 = {
			.reg_off = HHI_DEMOD_32K_CNTL1,
			.shift   = 0,
			.width   = 12,
		},
		.m2 = {
			.reg_off = HHI_DEMOD_32K_CNTL1,
			.shift   = 12,
			.width   = 12,
		},
		.dual = {
			.reg_off = HHI_DEMOD_32K_CNTL0,
			.shift   = 28,
			.width   = 1,
		},
		.table = t5w_demod_32k_div_table,
	},
	.hw.init = &(struct clk_init_data){
		.name = "demod_32k_div",
		.ops = &meson_clk_dualdiv_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_demod_32k_clkin.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap t5w_demod_32k = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_DEMOD_32K_CNTL0,
		.bit_idx = 30,
	},
	.hw.init = &(struct clk_init_data){
		.name = "demod_32k",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_demod_32k_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data t5w_hdmirx_sys_parent_data[] = {
	{ .fw_name = "xtal", },
	{ .hw = &t5w_fclk_div4.hw },
	{ .hw = &t5w_fclk_div3.hw },
	{ .hw = &t5w_fclk_div5.hw }
};

static struct clk_regmap t5w_hdmirx_5m_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_HRX_CLK_CTRL0,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmirx_5m_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t5w_hdmirx_sys_parent_data,
		.num_parents = ARRAY_SIZE(t5w_hdmirx_sys_parent_data),
	},
};

static struct clk_regmap t5w_hdmirx_5m_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_HRX_CLK_CTRL0,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmirx_5m_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_hdmirx_5m_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t5w_hdmirx_5m  = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_HRX_CLK_CTRL0,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmirx_5m",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_hdmirx_5m_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t5w_hdmirx_2m_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_HRX_CLK_CTRL0,
		.mask = 0x3,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmirx_2m_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t5w_hdmirx_sys_parent_data,
		.num_parents = ARRAY_SIZE(t5w_hdmirx_sys_parent_data),
	},
};

static struct clk_regmap t5w_hdmirx_2m_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_HRX_CLK_CTRL0,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmirx_2m_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_hdmirx_2m_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t5w_hdmirx_2m = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_HRX_CLK_CTRL0,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmirx_2m",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_hdmirx_2m_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t5w_hdmirx_cfg_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_HRX_CLK_CTRL1,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmirx_cfg_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t5w_hdmirx_sys_parent_data,
		.num_parents = ARRAY_SIZE(t5w_hdmirx_sys_parent_data),
	},
};

static struct clk_regmap t5w_hdmirx_cfg_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_HRX_CLK_CTRL1,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmirx_cfg_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_hdmirx_cfg_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t5w_hdmirx_cfg  = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_HRX_CLK_CTRL1,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmirx_cfg",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_hdmirx_cfg_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t5w_hdmirx_hdcp_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_HRX_CLK_CTRL1,
		.mask = 0x3,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmirx_hdcp_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t5w_hdmirx_sys_parent_data,
		.num_parents = ARRAY_SIZE(t5w_hdmirx_sys_parent_data),
	},
};

static struct clk_regmap t5w_hdmirx_hdcp_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_HRX_CLK_CTRL1,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmirx_hdcp_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_hdmirx_hdcp_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t5w_hdmirx_hdcp = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_HRX_CLK_CTRL1,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmirx_hdcp",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_hdmirx_hdcp_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t5w_hdmirx_aud_pll_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_HRX_CLK_CTRL2,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmirx_aud_pll_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t5w_hdmirx_sys_parent_data,
		.num_parents = ARRAY_SIZE(t5w_hdmirx_sys_parent_data),
	},
};

static struct clk_regmap t5w_hdmirx_aud_pll_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_HRX_CLK_CTRL2,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmirx_aud_pll_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_hdmirx_aud_pll_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t5w_hdmirx_aud_pll  = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_HRX_CLK_CTRL2,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmirx_aud_pll",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_hdmirx_aud_pll_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t5w_hdmirx_acr_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_HRX_CLK_CTRL2,
		.mask = 0x3,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmirx_acr_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t5w_hdmirx_sys_parent_data,
		.num_parents = ARRAY_SIZE(t5w_hdmirx_sys_parent_data),
	},
};

static struct clk_regmap t5w_hdmirx_acr_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_HRX_CLK_CTRL2,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmirx_acr_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_hdmirx_acr_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t5w_hdmirx_acr = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_HRX_CLK_CTRL2,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmirx_acr",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_hdmirx_acr_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t5w_hdmirx_meter_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_HRX_CLK_CTRL3,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmirx_meter_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t5w_hdmirx_sys_parent_data,
		.num_parents = ARRAY_SIZE(t5w_hdmirx_sys_parent_data),
	},
};

static struct clk_regmap t5w_hdmirx_meter_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_HRX_CLK_CTRL3,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmirx_meter_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_hdmirx_meter_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t5w_hdmirx_meter  = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_HRX_CLK_CTRL3,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmirx_meter",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_hdmirx_meter_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

/*cts_cdac_clk*/
static const struct clk_parent_data t5w_cdac_parent_data[] = {
	{ .fw_name = "xtal", },
	{ .hw = &t5w_fclk_div5.hw },
};

static struct clk_regmap t5w_cdac_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_CDAC_CLK_CNTL,
		.mask = 0x3,
		.shift = 16,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "cdac_clk_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t5w_cdac_parent_data,
		.num_parents = ARRAY_SIZE(t5w_cdac_parent_data),
		.flags = CLK_GET_RATE_NOCACHE
	},
};

static struct clk_regmap t5w_cdac_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_CDAC_CLK_CNTL,
		.shift = 0,
		.width = 16,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "cdac_clk_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_cdac_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t5w_cdac = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_CDAC_CLK_CNTL,
		.bit_idx = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cdac",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_cdac_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE |	CLK_SET_RATE_PARENT
	},
};

/*eth clk*/
static u32 t5w_eth_rmii_table[] = { 0 };
static struct clk_regmap t5w_eth_rmii_sel = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = HHI_ETH_CLK_CNTL,
		.mask = 0x7,
		.shift = 9,
		.table = t5w_eth_rmii_table
	},
	.hw.init = &(struct clk_init_data){
		.name = "eth_rmii_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_fclk_div2.hw,
		},
		.num_parents = 1
	},
};

static struct clk_regmap t5w_eth_rmii_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = HHI_ETH_CLK_CNTL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "eth_rmii_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_eth_rmii_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t5w_eth_rmii = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = HHI_ETH_CLK_CNTL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "eth_rmii",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_eth_rmii_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t5w_eth_125m = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = HHI_ETH_CLK_CNTL,
		.bit_idx = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "eth_125m",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_fclk_div2.hw
		},
		.num_parents = 1,
	},
};

/*spicc clk*/
static u32 t5w_spicc_mux_table[] = { 0, 1, 2, 3, 4, 5, 6};
static const struct clk_parent_data t5w_spicc_parent_hws[] = {
	{ .fw_name = "xtal", },
	{ .hw = &t5w_clk81.hw },
	{ .hw = &t5w_fclk_div4.hw },
	{ .hw = &t5w_fclk_div3.hw },
	{ .hw = &t5w_fclk_div2.hw },
	{ .hw = &t5w_fclk_div5.hw },
	{ .hw = &t5w_fclk_div7.hw },
};

static struct clk_regmap t5w_spicc0_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_SPICC_CLK_CNTL,
		.mask = 0x7,
		.shift = 7,
		.table = t5w_spicc_mux_table
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spicc0_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t5w_spicc_parent_hws,
		.num_parents = ARRAY_SIZE(t5w_spicc_parent_hws),
	},
};

static struct clk_regmap t5w_spicc0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_SPICC_CLK_CNTL,
		.shift = 0,
		.width = 6,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spicc0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_spicc0_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t5w_spicc0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_SPICC_CLK_CNTL,
		.bit_idx = 6,
	},
	.hw.init = &(struct clk_init_data){
		.name = "spicc0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_spicc0_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t5w_spicc1_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_SPICC_CLK_CNTL,
		.mask = 0x7,
		.shift = 23,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spicc1_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t5w_spicc_parent_hws,
		.num_parents = ARRAY_SIZE(t5w_spicc_parent_hws),
	},
};

static struct clk_regmap t5w_spicc1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_SPICC_CLK_CNTL,
		.shift = 16,
		.width = 6,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spicc1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_spicc1_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t5w_spicc1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_SPICC_CLK_CNTL,
		.bit_idx = 22,
	},
	.hw.init = &(struct clk_init_data){
		.name = "spicc1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_spicc1_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t5w_spicc2_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_SPICC_CLK_CNTL1,
		.mask = 0x7,
		.shift = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spicc2_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t5w_spicc_parent_hws,
		.num_parents = ARRAY_SIZE(t5w_spicc_parent_hws),
	},
};

static struct clk_regmap t5w_spicc2_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_SPICC_CLK_CNTL1,
		.shift = 0,
		.width = 6,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spicc2_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_spicc2_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t5w_spicc2 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_SPICC_CLK_CNTL1,
		.bit_idx = 6,
	},
	.hw.init = &(struct clk_init_data){
		.name = "spicc2",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_spicc2_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static u32 t5w_tvfe_mux_table[] = {0, 1, 2, 3};
static const struct clk_hw *t5w_tvfe_parent_hws[] = {
	&t5w_fclk_div3.hw,
	&t5w_fclk_div4.hw,
	&t5w_fclk_div5.hw,
	&t5w_fclk_div7.hw,
};

static struct clk_regmap t5w_tvfe_clk_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_TVFECLK_CNTL,
		.mask = 0x7,
		.shift = 7,
		.table = t5w_tvfe_mux_table
	},
	.hw.init = &(struct clk_init_data) {
		.name = "tvfe_clk_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = t5w_tvfe_parent_hws,
		.num_parents = ARRAY_SIZE(t5w_spicc_parent_hws),
	},
};

static struct clk_regmap t5w_tvfe_clk_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_TVFECLK_CNTL,
		.shift = 0,
		.width = 6,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "tvfe_clk_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_tvfe_clk_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t5w_tvfe_clk = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_TVFECLK_CNTL,
		.bit_idx = 6,
	},
	.hw.init = &(struct clk_init_data){
		.name = "tvfe_clk",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_tvfe_clk_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static u32 t5w_tsin_deglich_mux_table[] = {0, 2, 3, 4, 5, 6};
static const struct clk_hw *t5w_tsin_parent_parent_hws[] = {
	&t5w_fclk_div2.hw,
	&t5w_fclk_div4.hw,
	&t5w_fclk_div3.hw,
	&t5w_fclk_div2p5.hw,
	&t5w_fclk_div5.hw,
	&t5w_fclk_div7.hw,
};

static struct clk_regmap t5w_tsin_deglich_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_TSIN_DEGLITCH_CLK_CNTL,
		.mask = 0x7,
		.shift = 7,
		.table = t5w_tsin_deglich_mux_table
	},
	.hw.init = &(struct clk_init_data) {
		.name = "tsin_deglich_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = t5w_tsin_parent_parent_hws,
		.num_parents = ARRAY_SIZE(t5w_tsin_parent_parent_hws),
	},
};

static struct clk_regmap t5w_tsin_deglich_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_TSIN_DEGLITCH_CLK_CNTL,
		.shift = 0,
		.width = 6,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "tsin_deglich_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_tsin_deglich_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t5w_tsin_deglich = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_TSIN_DEGLITCH_CLK_CNTL,
		.bit_idx = 6,
	},
	.hw.init = &(struct clk_init_data){
		.name = "tsin_deglich",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5w_tsin_deglich_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

struct t5w_dsu_clk_postmux_nb_data {
	struct notifier_block nb;
	struct clk_hw *dsu_clk_dyn;
	struct clk_hw *dsu_clk_postmux0;
	struct clk_hw *dsu_clk_postmux1;
};

static int t5w_dsu_clk_postmux_notifier_cb(struct notifier_block *nb,
					   unsigned long event, void *data)
{
	struct t5w_dsu_clk_postmux_nb_data *nb_data =
		container_of(nb, struct t5w_dsu_clk_postmux_nb_data, nb);
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

static struct t5w_dsu_clk_postmux_nb_data t5w_dsu_clk_postmux0_nb_data = {
	.dsu_clk_dyn = &t5w_dsu_clk_dyn.hw,
	.dsu_clk_postmux0 = &t5w_dsu_clk_postmux0.hw,
	.dsu_clk_postmux1 = &t5w_dsu_clk_postmux1.hw,
	.nb.notifier_call = t5w_dsu_clk_postmux_notifier_cb,
};

#define MESON_T5W_SYS_GATE(_name, _reg, _bit)				\
struct clk_regmap _name = {						\
	.data = &(struct clk_regmap_gate_data) {			\
		.offset = (_reg),					\
		.bit_idx = (_bit),					\
	},								\
	.hw.init = &(struct clk_init_data) {				\
		.name = #_name,						\
		.ops = &clk_regmap_gate_ops,				\
		.parent_hws = (const struct clk_hw *[]) {		\
			&t5w_clk81.hw					\
		},							\
		.num_parents = 1,					\
		.flags = CLK_IGNORE_UNUSED,				\
	},								\
}

static MESON_T5W_SYS_GATE(t5w_clk81_ddr,		HHI_GCLK_MPEG0, 0);
static MESON_T5W_SYS_GATE(t5w_clk81_dos,		HHI_GCLK_MPEG0, 1);
static MESON_T5W_SYS_GATE(t5w_clk81_ethphy,		HHI_GCLK_MPEG0, 4);
static MESON_T5W_SYS_GATE(t5w_clk81_isa,		HHI_GCLK_MPEG0, 5);
static MESON_T5W_SYS_GATE(t5w_clk81_pl310,		HHI_GCLK_MPEG0, 6);
static MESON_T5W_SYS_GATE(t5w_clk81_periphs,		HHI_GCLK_MPEG0, 7);
static MESON_T5W_SYS_GATE(t5w_clk81_spicc0,		HHI_GCLK_MPEG0, 8);
static MESON_T5W_SYS_GATE(t5w_clk81_i2c,		HHI_GCLK_MPEG0, 9);
static MESON_T5W_SYS_GATE(t5w_clk81_sana,		HHI_GCLK_MPEG0, 10);
static MESON_T5W_SYS_GATE(t5w_clk81_smartcard,		HHI_GCLK_MPEG0, 11);
static MESON_T5W_SYS_GATE(t5w_clk81_spicc2,		HHI_GCLK_MPEG0, 12);
static MESON_T5W_SYS_GATE(t5w_clk81_uart0,		HHI_GCLK_MPEG0, 13);
static MESON_T5W_SYS_GATE(t5w_clk81_spicc1,		HHI_GCLK_MPEG0, 14);
static MESON_T5W_SYS_GATE(t5w_clk81_stream,		HHI_GCLK_MPEG0, 15);
static MESON_T5W_SYS_GATE(t5w_clk81_async_fifo,		HHI_GCLK_MPEG0, 16);
static MESON_T5W_SYS_GATE(t5w_clk81_tvfe,		HHI_GCLK_MPEG0, 18);
static MESON_T5W_SYS_GATE(t5w_clk81_hiu_reg,		HHI_GCLK_MPEG0, 19);
static MESON_T5W_SYS_GATE(t5w_clk81_hdmi20_aes,		HHI_GCLK_MPEG0, 20);
static MESON_T5W_SYS_GATE(t5w_clk81_hdmirx_pclk,	HHI_GCLK_MPEG0, 21);
static MESON_T5W_SYS_GATE(t5w_clk81_atv_demod,		HHI_GCLK_MPEG0, 22);
static MESON_T5W_SYS_GATE(t5w_clk81_assist_misc,	HHI_GCLK_MPEG0, 23);
static MESON_T5W_SYS_GATE(t5w_clk81_pwr_ctrl,		HHI_GCLK_MPEG0, 24);
static MESON_T5W_SYS_GATE(t5w_clk81_emmc_b,		HHI_GCLK_MPEG0, 25);
static MESON_T5W_SYS_GATE(t5w_clk81_emmc_c,		HHI_GCLK_MPEG0, 26);
static MESON_T5W_SYS_GATE(t5w_clk81_adec,		HHI_GCLK_MPEG0, 27);
static MESON_T5W_SYS_GATE(t5w_clk81_acodec,		HHI_GCLK_MPEG0, 28);
static MESON_T5W_SYS_GATE(t5w_clk81_tcon,		HHI_GCLK_MPEG0, 29);
static MESON_T5W_SYS_GATE(t5w_clk81_spi,		HHI_GCLK_MPEG0, 30);

static MESON_T5W_SYS_GATE(t5w_clk81_audio,		HHI_GCLK_MPEG1, 0);
static MESON_T5W_SYS_GATE(t5w_clk81_eth,		HHI_GCLK_MPEG1, 3);
static MESON_T5W_SYS_GATE(t5w_clk81_clk_rst,		HHI_GCLK_MPEG1, 5);
static MESON_T5W_SYS_GATE(t5w_clk81_aififo,		HHI_GCLK_MPEG1, 11);
static MESON_T5W_SYS_GATE(t5w_clk81_uart1,		HHI_GCLK_MPEG1, 16);
static MESON_T5W_SYS_GATE(t5w_clk81_g2d,		HHI_GCLK_MPEG1, 20);
static MESON_T5W_SYS_GATE(t5w_clk81_reset,		HHI_GCLK_MPEG1, 23);
static MESON_T5W_SYS_GATE(t5w_clk81_usb_gene,		HHI_GCLK_MPEG1, 26);
static MESON_T5W_SYS_GATE(t5w_clk81_ahb_arb0,		HHI_GCLK_MPEG1, 29);

static MESON_T5W_SYS_GATE(t5w_clk81_ahb_data,		HHI_GCLK_MPEG2, 1);
static MESON_T5W_SYS_GATE(t5w_clk81_ahb_ctrl,		HHI_GCLK_MPEG2, 2);
static MESON_T5W_SYS_GATE(t5w_clk81_usb1_to_ddr,	HHI_GCLK_MPEG2, 8);
static MESON_T5W_SYS_GATE(t5w_clk81_mmc_pclk,		HHI_GCLK_MPEG2, 11);
static MESON_T5W_SYS_GATE(t5w_clk81_hdmirx_axi,		HHI_GCLK_MPEG2, 12);
static MESON_T5W_SYS_GATE(t5w_clk81_hdcp22_pclk,	HHI_GCLK_MPEG2, 13);
static MESON_T5W_SYS_GATE(t5w_clk81_uart2,		HHI_GCLK_MPEG2, 15);
static MESON_T5W_SYS_GATE(t5w_clk81_ciplus,		HHI_GCLK_MPEG2, 21);
static MESON_T5W_SYS_GATE(t5w_clk81_ts,			HHI_GCLK_MPEG2, 22);
static MESON_T5W_SYS_GATE(t5w_clk81_vpu_int,		HHI_GCLK_MPEG2, 25);
static MESON_T5W_SYS_GATE(t5w_clk81_demod_com,		HHI_GCLK_MPEG2, 28);
static MESON_T5W_SYS_GATE(t5w_clk81_mute,		HHI_GCLK_MPEG2, 29);
static MESON_T5W_SYS_GATE(t5w_clk81_gic,		HHI_GCLK_MPEG2, 30);
static MESON_T5W_SYS_GATE(t5w_clk81_aucpu,		HHI_GCLK_MPEG2, 31);

static MESON_T5W_SYS_GATE(t5w_clk81_vclk2_venci0,	HHI_GCLK_OTHER, 1);
static MESON_T5W_SYS_GATE(t5w_clk81_vclk2_venci1,	HHI_GCLK_OTHER, 2);
static MESON_T5W_SYS_GATE(t5w_clk81_vclk2_vencp0,	HHI_GCLK_OTHER, 3);
static MESON_T5W_SYS_GATE(t5w_clk81_vclk2_vencp1,	HHI_GCLK_OTHER, 4);
static MESON_T5W_SYS_GATE(t5w_clk81_vclk2_venct0,	HHI_GCLK_OTHER, 5);
static MESON_T5W_SYS_GATE(t5w_clk81_vclk2_venct1,	HHI_GCLK_OTHER, 6);
static MESON_T5W_SYS_GATE(t5w_clk81_vclk2_other,	HHI_GCLK_OTHER, 7);
static MESON_T5W_SYS_GATE(t5w_clk81_vclk2_enci,		HHI_GCLK_OTHER, 8);
static MESON_T5W_SYS_GATE(t5w_clk81_vclk2_encp,		HHI_GCLK_OTHER, 9);
static MESON_T5W_SYS_GATE(t5w_clk81_dac_clk,		HHI_GCLK_OTHER, 10);
static MESON_T5W_SYS_GATE(t5w_clk81_enc480p,		HHI_GCLK_OTHER, 20);
static MESON_T5W_SYS_GATE(t5w_clk81_ramdom,		HHI_GCLK_OTHER, 21);
static MESON_T5W_SYS_GATE(t5w_clk81_vclk2_enct,		HHI_GCLK_OTHER, 22);
static MESON_T5W_SYS_GATE(t5w_clk81_vclk2_encl,		HHI_GCLK_OTHER, 23);
static MESON_T5W_SYS_GATE(t5w_clk81_vclk2_venclmmc,	HHI_GCLK_OTHER, 24);
static MESON_T5W_SYS_GATE(t5w_clk81_vclk2_vencl,	HHI_GCLK_OTHER, 25);
static MESON_T5W_SYS_GATE(t5w_clk81_vclk2_other1,	HHI_GCLK_OTHER, 26);

static MESON_T5W_SYS_GATE(t5w_clk81_dma,		HHI_GCLK_SP_MPEG, 0);
static MESON_T5W_SYS_GATE(t5w_clk81_efuse,		HHI_GCLK_SP_MPEG, 1);
static MESON_T5W_SYS_GATE(t5w_clk81_rom_boot,		HHI_GCLK_SP_MPEG, 2);
static MESON_T5W_SYS_GATE(t5w_clk81_reset_sec,		HHI_GCLK_SP_MPEG, 3);
static MESON_T5W_SYS_GATE(t5w_clk81_sec_ahb,		HHI_GCLK_SP_MPEG, 4);
static MESON_T5W_SYS_GATE(t5w_clk81_rsa,		HHI_GCLK_SP_MPEG, 5);

/* Array of all clocks provided by this provider */
static struct clk_hw_onecell_data t5w_hw_onecell_data = {
	.hws = {
		[CLKID_FIXED_PLL_DCO]		= &t5w_fixed_pll_dco.hw,
		[CLKID_SYS_PLL_DCO]		= &t5w_sys_pll_dco.hw,
		[CLKID_GP0_PLL_DCO]		= &t5w_gp0_pll_dco.hw,
		[CLKID_GP1_PLL_DCO]		= &t5w_gp1_pll_dco.hw,
		[CLKID_HIFI_PLL_DCO]		= &t5w_hifi_pll_dco.hw,
		[CLKID_FIXED_PLL]		= &t5w_fixed_pll.hw,
		[CLKID_SYS_PLL]			= &t5w_sys_pll.hw,
		[CLKID_GP0_PLL]			= &t5w_gp0_pll.hw,
		[CLKID_GP1_PLL]			= &t5w_gp1_pll.hw,
		[CLKID_HIFI_PLL]		= &t5w_hifi_pll.hw,
		[CLKID_FCLK_DIV2_DIV]		= &t5w_fclk_div2_div.hw,
		[CLKID_FCLK_DIV3_DIV]		= &t5w_fclk_div3_div.hw,
		[CLKID_FCLK_DIV4_DIV]		= &t5w_fclk_div4_div.hw,
		[CLKID_FCLK_DIV5_DIV]		= &t5w_fclk_div5_div.hw,
		[CLKID_FCLK_DIV7_DIV]		= &t5w_fclk_div7_div.hw,
		[CLKID_FCLK_DIV2P5_DIV]		= &t5w_fclk_div2p5_div.hw,
		[CLKID_FCLK_DIV2]		= &t5w_fclk_div2.hw,
		[CLKID_FCLK_DIV3]		= &t5w_fclk_div3.hw,
		[CLKID_FCLK_DIV4]		= &t5w_fclk_div4.hw,
		[CLKID_FCLK_DIV5]		= &t5w_fclk_div5.hw,
		[CLKID_FCLK_DIV7]		= &t5w_fclk_div7.hw,
		[CLKID_FCLK_DIV2P5]		= &t5w_fclk_div2p5.hw,
		[CLKID_PRE_MPLL]		= &t5w_mpll_prediv.hw,
		[CLKID_MPLL0_DIV]		= &t5w_mpll0_div.hw,
		[CLKID_MPLL1_DIV]		= &t5w_mpll1_div.hw,
		[CLKID_MPLL2_DIV]		= &t5w_mpll2_div.hw,
		[CLKID_MPLL3_DIV]		= &t5w_mpll3_div.hw,
		[CLKID_MPLL0]			= &t5w_mpll0.hw,
		[CLKID_MPLL1]			= &t5w_mpll1.hw,
		[CLKID_MPLL2]			= &t5w_mpll2.hw,
		[CLKID_MPLL3]			= &t5w_mpll3.hw,
		[CLKID_MPLL_50M_DIV]		= &t5w_mpll_50m_div.hw,
		[CLKID_MPLL_50M]		= &t5w_mpll_50m.hw,
		[CLKID_CPU_CLK_DYN0_SEL]	= &t5w_cpu_clk_premux0.hw,
		[CLKID_CPU_CLK_DYN0_DIV]	= &t5w_cpu_clk_mux0_div.hw,
		[CLKID_CPU_CLK_DYN0]		= &t5w_cpu_clk_postmux0.hw,
		[CLKID_CPU_CLK_DYN1_SEL]	= &t5w_cpu_clk_premux1.hw,
		[CLKID_CPU_CLK_DYN1_DIV]	= &t5w_cpu_clk_mux1_div.hw,
		[CLKID_CPU_CLK_DYN1]		= &t5w_cpu_clk_postmux1.hw,
		[CLKID_CPU_CLK_DYN]		= &t5w_cpu_clk_dyn.hw,
		[CLKID_CPU_CLK]			= &t5w_cpu_clk.hw,
		[CLKID_DSU_CLK_DYN0_SEL]	= &t5w_dsu_clk_premux0.hw,
		[CLKID_DSU_CLK_DYN0_DIV]	= &t5w_dsu_clk_premux1.hw,
		[CLKID_DSU_CLK_DYN0]		= &t5w_dsu_clk_mux0_div.hw,
		[CLKID_DSU_CLK_DYN1_SEL]	= &t5w_dsu_clk_postmux0.hw,
		[CLKID_DSU_CLK_DYN1_DIV]	= &t5w_dsu_clk_mux1_div.hw,
		[CLKID_DSU_CLK_DYN1]		= &t5w_dsu_clk_postmux1.hw,
		[CLKID_DSU_CLK_DYN]		= &t5w_dsu_clk_dyn.hw,
		[CLKID_DSU_CLK_FINAL]		= &t5w_dsu_final_clk.hw,
		[CLKID_DSU_CLK]			= &t5w_dsu_clk.hw,
		[CLKID_CLK81_DDR]		= &t5w_clk81_ddr.hw,
		[CLKID_CLK81_DOS]		= &t5w_clk81_dos.hw,
		[CLKID_CLK81_ETH_PHY]		= &t5w_clk81_ethphy.hw,
		[CLKID_CLK81_ISA]		= &t5w_clk81_isa.hw,
		[CLKID_CLK81_PL310]		= &t5w_clk81_pl310.hw,
		[CLKID_CLK81_PERIPHS]		= &t5w_clk81_periphs.hw,
		[CLKID_CLK81_SPICC0]		= &t5w_clk81_spicc0.hw,
		[CLKID_CLK81_I2C]		= &t5w_clk81_i2c.hw,
		[CLKID_CLK81_SANA]		= &t5w_clk81_sana.hw,
		[CLKID_CLK81_SMARTCARD]		= &t5w_clk81_smartcard.hw,
		[CLKID_CLK81_SPICC1]		= &t5w_clk81_spicc1.hw,
		[CLKID_CLK81_SPICC2]		= &t5w_clk81_spicc2.hw,
		[CLKID_CLK81_UART0]		= &t5w_clk81_uart0.hw,
		[CLKID_CLK81_STREAM]		= &t5w_clk81_stream.hw,
		[CLKID_CLK81_ASYNC_FIFO]	= &t5w_clk81_async_fifo.hw,
		[CLKID_CLK81_TVFE]		= &t5w_clk81_tvfe.hw,
		[CLKID_CLK81_HIU_REG]		= &t5w_clk81_hiu_reg.hw,
		[CLKID_CLK81_HDMIRX_PCLK]	= &t5w_clk81_hdmirx_pclk.hw,
		[CLKID_CLK81_ATV_DEMOD]		= &t5w_clk81_atv_demod.hw,
		[CLKID_CLK81_ASSIST_MISC]	= &t5w_clk81_assist_misc.hw,
		[CLKID_CLK81_PWR_CTRL]		= &t5w_clk81_pwr_ctrl.hw,
		[CLKID_CLK81_SD_EMMC_C]		= &t5w_clk81_emmc_c.hw,
		[CLKID_CLK81_ADEC]		= &t5w_clk81_adec.hw,
		[CLKID_CLK81_ACODEC]		= &t5w_clk81_acodec.hw,
		[CLKID_CLK81_TCON]		= &t5w_clk81_tcon.hw,
		[CLKID_CLK81_SPI]		= &t5w_clk81_spi.hw,
		[CLKID_CLK81_AUDIO]		= &t5w_clk81_audio.hw,
		[CLKID_CLK81_ETH_CORE]		= &t5w_clk81_eth.hw,
		[CLKID_CLK81_CLK_RST]		= &t5w_clk81_clk_rst.hw,
		[CLKID_CLK81_AIFIFO]		= &t5w_clk81_aififo.hw,
		[CLKID_CLK81_UART1]		= &t5w_clk81_uart1.hw,
		[CLKID_CLK81_G2D]		= &t5w_clk81_g2d.hw,
		[CLKID_CLK81_RESET]		= &t5w_clk81_reset.hw,
		[CLKID_CLK81_USB_GENERAL]	= &t5w_clk81_usb_gene.hw,
		[CLKID_CLK81_AHB_ARB0]		= &t5w_clk81_ahb_arb0.hw,
		[CLKID_CLK81_AHB_DATA_BUS]	= &t5w_clk81_ahb_data.hw,
		[CLKID_CLK81_AHB_CTRL_BUS]	= &t5w_clk81_ahb_ctrl.hw,
		[CLKID_CLK81_USB1_TO_DDR]	= &t5w_clk81_usb1_to_ddr.hw,
		[CLKID_CLK81_MMC_PCLK]		= &t5w_clk81_mmc_pclk.hw,
		[CLKID_CLK81_HDMIRX_AXI]	= &t5w_clk81_hdmirx_axi.hw,
		[CLKID_CLK81_HDCP22_PCLK]	= &t5w_clk81_hdcp22_pclk.hw,
		[CLKID_CLK81_UART2]		= &t5w_clk81_uart2.hw,
		[CLKID_CLK81_CLK81_TS]		= &t5w_clk81_ts.hw,
		[CLKID_CLK81_VPU_INTR]		= &t5w_clk81_vpu_int.hw,
		[CLKID_CLK81_DEMOD_COMB]	= &t5w_clk81_demod_com.hw,
		[CLKID_CLK81_GIC]		= &t5w_clk81_gic.hw,
		[CLKID_CLK81_VCLK2_VENCI0]	= &t5w_clk81_vclk2_venci0.hw,
		[CLKID_CLK81_VCLK2_VENCI1]	= &t5w_clk81_vclk2_venci1.hw,
		[CLKID_CLK81_VCLK2_VENCP0]	= &t5w_clk81_vclk2_vencp0.hw,
		[CLKID_CLK81_VCLK2_VENCP1]	= &t5w_clk81_vclk2_vencp1.hw,
		[CLKID_CLK81_VCLK2_VENCT0]	= &t5w_clk81_vclk2_venct0.hw,
		[CLKID_CLK81_VCLK2_VENCT1]	= &t5w_clk81_vclk2_venct1.hw,
		[CLKID_CLK81_VCLK2_OTHER]	= &t5w_clk81_vclk2_other.hw,
		[CLKID_CLK81_VCLK2_ENCI]	= &t5w_clk81_vclk2_enci.hw,
		[CLKID_CLK81_VCLK2_ENCP]	= &t5w_clk81_vclk2_encp.hw,
		[CLKID_CLK81_DAC_CLK]		= &t5w_clk81_dac_clk.hw,
		[CLKID_CLK81_ENC480P]		= &t5w_clk81_enc480p.hw,
		[CLKID_CLK81_RAMDOM]		= &t5w_clk81_ramdom.hw,
		[CLKID_CLK81_VCLK2_ENCT]	= &t5w_clk81_vclk2_enct.hw,
		[CLKID_CLK81_VCLK2_ENCL]	= &t5w_clk81_vclk2_encl.hw,
		[CLKID_CLK81_VCLK2_VENCLMMC]	= &t5w_clk81_vclk2_venclmmc.hw,
		[CLKID_CLK81_VCLK2_VENCL]	= &t5w_clk81_vclk2_vencl.hw,
		[CLKID_CLK81_VCLK2_OTHER1]	= &t5w_clk81_vclk2_other1.hw,
		[CLKID_CLK81_DMA]		= &t5w_clk81_dma.hw,
		[CLKID_CLK81_EFUSE]		= &t5w_clk81_efuse.hw,
		[CLKID_CLK81_ROM_BOOT]		= &t5w_clk81_rom_boot.hw,
		[CLKID_CLK81_RESET_SEC]		= &t5w_clk81_reset_sec.hw,
		[CLKID_CLK81_SEC_AHB]		= &t5w_clk81_sec_ahb.hw,
		[CLKID_CLK81_RSA]		= &t5w_clk81_rsa.hw,
						/* clk81 gate over */
		[CLKID_MPEG_SEL]		= &t5w_mpeg_clk_sel.hw,
		[CLKID_MPEG_DIV]		= &t5w_mpeg_clk_div.hw,
		[CLKID_CLK81]			= &t5w_clk81.hw,
		[CLKID_VDEC_0_SEL]		= &t5w_vdec_0_sel.hw,
		[CLKID_VDEC_0_DIV]		= &t5w_vdec_0_div.hw,
		[CLKID_VDEC_0]			= &t5w_vdec_0.hw,
		[CLKID_VDEC_1_SEL]		= &t5w_vdec_1_sel.hw,
		[CLKID_VDEC_1_DIV]		= &t5w_vdec_1_div.hw,
		[CLKID_VDEC_1]			= &t5w_vdec_1.hw,
		[CLKID_VDEC]			= &t5w_vdec.hw,
		[CLKID_HEVCF_0_SEL]		= &t5w_hevcf_0_sel.hw,
		[CLKID_HEVCF_0_DIV]		= &t5w_hevcf_0_div.hw,
		[CLKID_HEVCF_0]			= &t5w_hevcf_0.hw,
		[CLKID_HEVCF_1_SEL]		= &t5w_hevcf_1_sel.hw,
		[CLKID_HEVCF_1_DIV]		= &t5w_hevcf_1_div.hw,
		[CLKID_HEVCF_1]			= &t5w_hevcf_1.hw,
		[CLKID_HEVCF]			= &t5w_hevcf.hw,
		[CLKID_GE2D_SEL]		= &t5w_ge2d_sel.hw,
		[CLKID_GE2D_DIV]		= &t5w_ge2d_div.hw,
		[CLKID_GE2D]			= &t5w_ge2d.hw,
		[CLKID_VAPB_0_SEL]		= &t5w_vapb_0_sel.hw,
		[CLKID_VAPB_0_DIV]		= &t5w_vapb_0_div.hw,
		[CLKID_VAPB_0]			= &t5w_vapb_0.hw,
		[CLKID_VAPB_1_SEL]		= &t5w_vapb_1_sel.hw,
		[CLKID_VAPB_1_DIV]		= &t5w_vapb_1_div.hw,
		[CLKID_VAPB_1]			= &t5w_vapb_1.hw,
		[CLKID_VAPB]			= &t5w_vapb.hw,
		[CLKID_MALI_0_SEL]		= &t5w_mali_0_sel.hw,
		[CLKID_MALI_0_DIV]		= &t5w_mali_0_div.hw,
		[CLKID_MALI_0]			= &t5w_mali_0.hw,
		[CLKID_MALI_1_SEL]		= &t5w_mali_1_sel.hw,
		[CLKID_MALI_1_DIV]		= &t5w_mali_1_div.hw,
		[CLKID_MALI_1]			= &t5w_mali_1.hw,
		[CLKID_MALI]			= &t5w_mali.hw,
		[CLKID_TS_CLK_DIV]		= &t5w_ts_clk_div.hw,
		[CLKID_TS_CLK]			= &t5w_ts_clk.hw,
		[CLKID_SD_EMMC_C_SEL]		= &t5w_sd_emmc_c_sel.hw,
		[CLKID_SD_EMMC_C_DIV]		= &t5w_sd_emmc_c_div.hw,
		[CLKID_SD_EMMC_C]		= &t5w_sd_emmc_c.hw,
		[CLKID_SD_EMMC_B_SEL]		= &t5w_sd_emmc_b_sel.hw,
		[CLKID_SD_EMMC_B_DIV]		= &t5w_sd_emmc_b_div.hw,
		[CLKID_SD_EMMC_B]		= &t5w_sd_emmc_b.hw,
		[CLKID_VDIN_MEAS_SEL]		= &t5w_vdin_meas_sel.hw,
		[CLKID_VDIN_MEAS_DIV]		= &t5w_vdin_meas_div.hw,
		[CLKID_VDIN_MEAS]		= &t5w_vdin_meas.hw,
		[CLKID_VID_LOCK_DIV]		= &t5w_vid_lock_div.hw,
		[CLKID_VID_LOCK]		= &t5w_vid_lock_clk.hw,
		[CLKID_VPU_0_SEL]		= &t5w_vpu_0_sel.hw,
		[CLKID_VPU_0_DIV]		= &t5w_vpu_0_div.hw,
		[CLKID_VPU_0]			= &t5w_vpu_0.hw,
		[CLKID_VPU_1_SEL]		= &t5w_vpu_1_sel.hw,
		[CLKID_VPU_1_DIV]		= &t5w_vpu_1_div.hw,
		[CLKID_VPU_1]			= &t5w_vpu_1.hw,
		[CLKID_VPU]			= &t5w_vpu.hw,
		[CLKID_VPU_CLKB_TMP_SEL]	= &t5w_vpu_clkb_tmp_sel.hw,
		[CLKID_VPU_CLKB_TMP_DIV]	= &t5w_vpu_clkb_tmp_div.hw,
		[CLKID_VPU_CLKB_TMP]		= &t5w_vpu_clkb_tmp.hw,
		[CLKID_VPU_CLKB_DIV]		= &t5w_vpu_clkb_div.hw,
		[CLKID_VPU_CLKB]		= &t5w_vpu_clkb.hw,
		[CLKID_VPU_CLKC_0_SEL]		= &t5w_vpu_clkc_0_sel.hw,
		[CLKID_VPU_CLKC_0_DIV]		= &t5w_vpu_clkc_0_div.hw,
		[CLKID_VPU_CLKC_0]		= &t5w_vpu_clkc_0.hw,
		[CLKID_VPU_CLKC_1_SEL]		= &t5w_vpu_clkc_1_sel.hw,
		[CLKID_VPU_CLKC_1_DIV]		= &t5w_vpu_clkc_1_div.hw,
		[CLKID_VPU_CLKC_1]		= &t5w_vpu_clkc_1.hw,
		[CLKID_VPU_CLKC]		= &t5w_vpu_clkc.hw,
		[CLKID_CTS_TCON_PLL_CLK_SEL]	= &t5w_cts_tcon_pll_clk_sel.hw,
		[CLKID_CTS_TCON_PLL_CLK_DIV]	= &t5w_cts_tcon_pll_clk_div.hw,
		[CLKID_CTS_TCON_PLL_CLK]	= &t5w_cts_tcon_pll_clk.hw,
		[CLKID_ADC_EXTCLK_SEL]		= &t5w_adc_extclk_sel.hw,
		[CLKID_ADC_EXTCLK_DIV]		= &t5w_adc_extclk_div.hw,
		[CLKID_ADC_EXTCLK]		= &t5w_adc_extclk.hw,
		[CLKID_CTS_DEMOD_CORE_SEL]	= &t5w_cts_demod_core_sel.hw,
		[CLKID_CTS_DEMOD_CORE_DIV]	= &t5w_cts_demod_core_div.hw,
		[CLKID_CTS_DEMOD_CORE]		= &t5w_cts_demod_core.hw,
		[CLKID_CTS_DEMOD_CORE_T2_SEL]	= &t5w_cts_demod_core_t2_sel.hw,
		[CLKID_CTS_DEMOD_CORE_T2_DIV]	= &t5w_cts_demod_core_t2_div.hw,
		[CLKID_CTS_DEMOD_CORE_T2]	= &t5w_cts_demod_core_t2.hw,
		[CLKID_DEMOD_32K_CLKIN]		= &t5w_demod_32k_clkin.hw,
		[CLKID_DEMOD_32K_DIV]		= &t5w_demod_32k_div.hw,
		[CLKID_DEMOD_32K]		= &t5w_demod_32k.hw,
		[CLKID_HDMIRX_5M_SEL]		= &t5w_hdmirx_5m_sel.hw,
		[CLKID_HDMIRX_5M_DIV]		= &t5w_hdmirx_5m_div.hw,
		[CLKID_HDMIRX_5M]		= &t5w_hdmirx_5m.hw,
		[CLKID_HDMIRX_2M_SEL]		= &t5w_hdmirx_2m_sel.hw,
		[CLKID_HDMIRX_2M_DIV]		= &t5w_hdmirx_2m_div.hw,
		[CLKID_HDMIRX_2M]		= &t5w_hdmirx_2m.hw,
		[CLKID_HDMIRX_CFG_SEL]		= &t5w_hdmirx_cfg_sel.hw,
		[CLKID_HDMIRX_CFG_DIV]		= &t5w_hdmirx_cfg_div.hw,
		[CLKID_HDMIRX_CFG]		= &t5w_hdmirx_cfg.hw,
		[CLKID_HDMIRX_HDCP_SEL]		= &t5w_hdmirx_hdcp_sel.hw,
		[CLKID_HDMIRX_HDCP_DIV]		= &t5w_hdmirx_hdcp_div.hw,
		[CLKID_HDMIRX_HDCP]		= &t5w_hdmirx_hdcp.hw,
		[CLKID_HDMIRX_AUD_PLL_SEL]	= &t5w_hdmirx_aud_pll_sel.hw,
		[CLKID_HDMIRX_AUD_PLL_DIV]	= &t5w_hdmirx_aud_pll_div.hw,
		[CLKID_HDMIRX_AUD_PLL]		= &t5w_hdmirx_aud_pll.hw,
		[CLKID_HDMIRX_ACR_SEL]		= &t5w_hdmirx_acr_sel.hw,
		[CLKID_HDMIRX_ACR_DIV]		= &t5w_hdmirx_acr_div.hw,
		[CLKID_HDMIRX_ACR]		= &t5w_hdmirx_acr.hw,
		[CLKID_HDMIRX_METER_SEL]	= &t5w_hdmirx_meter_sel.hw,
		[CLKID_HDMIRX_METER_DIV]	= &t5w_hdmirx_meter_div.hw,
		[CLKID_HDMIRX_METER]		= &t5w_hdmirx_meter.hw,
		[CLKID_CDAC_CLK_SEL]		= &t5w_cdac_sel.hw,
		[CLKID_CDAC_CLK_DIV]		= &t5w_cdac_div.hw,
		[CLKID_CDAC_CLK]		= &t5w_cdac.hw,
		[CLKID_ETH_RMII_SEL]		= &t5w_eth_rmii_sel.hw,
		[CLKID_ETH_RMII_DIV]		= &t5w_eth_rmii_div.hw,
		[CLKID_ETH_RMII]		= &t5w_eth_rmii.hw,
		[CLKID_ETH_125M]		= &t5w_eth_125m.hw,
		[CLKID_SPICC0_SEL]		= &t5w_spicc0_sel.hw,
		[CLKID_SPICC0_DIV]		= &t5w_spicc0_div.hw,
		[CLKID_SPICC0]			= &t5w_spicc0.hw,
		[CLKID_SPICC1_SEL]		= &t5w_spicc1_sel.hw,
		[CLKID_SPICC1_DIV]		= &t5w_spicc1_div.hw,
		[CLKID_SPICC1]			= &t5w_spicc1.hw,
		[CLKID_SPICC2_SEL]		= &t5w_spicc2_sel.hw,
		[CLKID_SPICC2_DIV]		= &t5w_spicc2_div.hw,
		[CLKID_SPICC2]			= &t5w_spicc2.hw,
		[CLKID_TVFE_CLK_SEL]		= &t5w_tvfe_clk_sel.hw,
		[CLKID_TVFE_CLK_DIV]		= &t5w_tvfe_clk_div.hw,
		[CLKID_TVFE_CLK]		= &t5w_tvfe_clk.hw,
		[CLKID_TSIN_DEGLICH_CLK_SEL]	= &t5w_tsin_deglich_sel.hw,
		[CLKID_TSIN_DEGLICH_CLK_DIV]	= &t5w_tsin_deglich_div.hw,
		[CLKID_TSIN_DEGLICH_CLK]	= &t5w_tsin_deglich.hw,
		[NR_CLKS]			= NULL
	},
	.num = NR_CLKS,
};

static struct clk_regmap *const t5w_clk_regmaps[] __initconst = {
	&t5w_fixed_pll_dco,
	&t5w_sys_pll_dco,
	&t5w_gp0_pll_dco,
	&t5w_gp1_pll_dco,
	&t5w_hifi_pll_dco,
	&t5w_fixed_pll,
	&t5w_sys_pll,
	&t5w_gp0_pll,
	&t5w_gp1_pll,
	&t5w_hifi_pll,
	&t5w_fclk_div2,
	&t5w_fclk_div3,
	&t5w_fclk_div4,
	&t5w_fclk_div5,
	&t5w_fclk_div7,
	&t5w_fclk_div2p5,
	&t5w_mpll0_div,
	&t5w_mpll1_div,
	&t5w_mpll2_div,
	&t5w_mpll3_div,
	&t5w_mpll0,
	&t5w_mpll1,
	&t5w_mpll2,
	&t5w_mpll3,
	&t5w_mpll_50m,
	&t5w_mpeg_clk_sel,
	&t5w_mpeg_clk_div,
	&t5w_clk81,
	&t5w_vdec_0_sel,
	&t5w_vdec_0_div,
	&t5w_vdec_0,
	&t5w_vdec_1_sel,
	&t5w_vdec_1_div,
	&t5w_vdec_1,
	&t5w_vdec,
	&t5w_hevcf_0_sel,
	&t5w_hevcf_0_div,
	&t5w_hevcf_0,
	&t5w_hevcf_1_sel,
	&t5w_hevcf_1_div,
	&t5w_hevcf_1,
	&t5w_hevcf,
	&t5w_ge2d_sel,
	&t5w_ge2d_div,
	&t5w_ge2d,
	&t5w_vapb_0_sel,
	&t5w_vapb_0_div,
	&t5w_vapb_0,
	&t5w_vapb_1_sel,
	&t5w_vapb_1_div,
	&t5w_vapb_1,
	&t5w_vapb,
	&t5w_mali_0_sel,
	&t5w_mali_0_div,
	&t5w_mali_0,
	&t5w_mali_1_sel,
	&t5w_mali_1_div,
	&t5w_mali_1,
	&t5w_mali,
	&t5w_ts_clk_div,
	&t5w_ts_clk,
	&t5w_sd_emmc_c_sel,
	&t5w_sd_emmc_c_div,
	&t5w_sd_emmc_c,
	&t5w_sd_emmc_b_sel,
	&t5w_sd_emmc_b_div,
	&t5w_sd_emmc_b,
	&t5w_vdin_meas_sel,
	&t5w_vdin_meas_div,
	&t5w_vdin_meas,
	&t5w_vid_lock_div,
	&t5w_vid_lock_clk,
	&t5w_vpu_0_sel,
	&t5w_vpu_0_div,
	&t5w_vpu_0,
	&t5w_vpu_1_sel,
	&t5w_vpu_1_div,
	&t5w_vpu_1,
	&t5w_vpu,
	&t5w_vpu_clkb_tmp_sel,
	&t5w_vpu_clkb_tmp_div,
	&t5w_vpu_clkb_tmp,
	&t5w_vpu_clkb_div,
	&t5w_vpu_clkb,
	&t5w_vpu_clkc_0_sel,
	&t5w_vpu_clkc_0_div,
	&t5w_vpu_clkc_0,
	&t5w_vpu_clkc_1_sel,
	&t5w_vpu_clkc_1_div,
	&t5w_vpu_clkc_1,
	&t5w_vpu_clkc,
	&t5w_cts_tcon_pll_clk_sel,
	&t5w_cts_tcon_pll_clk_div,
	&t5w_cts_tcon_pll_clk,
	&t5w_adc_extclk_sel,
	&t5w_adc_extclk_div,
	&t5w_adc_extclk,
	&t5w_cts_demod_core_sel,
	&t5w_cts_demod_core_div,
	&t5w_cts_demod_core,
	&t5w_cts_demod_core_t2_sel,
	&t5w_cts_demod_core_t2_div,
	&t5w_cts_demod_core_t2,
	&t5w_demod_32k_clkin,
	&t5w_demod_32k_div,
	&t5w_demod_32k,
	&t5w_hdmirx_5m_sel,
	&t5w_hdmirx_5m_div,
	&t5w_hdmirx_5m,
	&t5w_hdmirx_2m_sel,
	&t5w_hdmirx_2m_div,
	&t5w_hdmirx_2m,
	&t5w_hdmirx_cfg_sel,
	&t5w_hdmirx_cfg_div,
	&t5w_hdmirx_cfg,
	&t5w_hdmirx_hdcp_sel,
	&t5w_hdmirx_hdcp_div,
	&t5w_hdmirx_hdcp,
	&t5w_hdmirx_aud_pll_sel,
	&t5w_hdmirx_aud_pll_div,
	&t5w_hdmirx_aud_pll,
	&t5w_hdmirx_acr_sel,
	&t5w_hdmirx_acr_div,
	&t5w_hdmirx_acr,
	&t5w_hdmirx_meter_sel,
	&t5w_hdmirx_meter_div,
	&t5w_hdmirx_meter,
	&t5w_cdac_sel,
	&t5w_cdac_div,
	&t5w_cdac,
	&t5w_eth_rmii_sel,
	&t5w_eth_rmii_div,
	&t5w_eth_rmii,
	&t5w_eth_125m,
	&t5w_spicc0_sel,
	&t5w_spicc0_div,
	&t5w_spicc0,
	&t5w_spicc1_sel,
	&t5w_spicc1_div,
	&t5w_spicc1,
	&t5w_spicc2_sel,
	&t5w_spicc2_div,
	&t5w_spicc2,
	&t5w_tvfe_clk_sel,
	&t5w_tvfe_clk_div,
	&t5w_tvfe_clk,
	&t5w_tsin_deglich_sel,
	&t5w_tsin_deglich_div,
	&t5w_tsin_deglich,
};

static struct clk_regmap *const t5w_cpu_clk_regmaps[] __initconst = {
	&t5w_cpu_clk_premux0,
	&t5w_cpu_clk_mux0_div,
	&t5w_cpu_clk_postmux0,
	&t5w_cpu_clk_premux1,
	&t5w_cpu_clk_mux1_div,
	&t5w_cpu_clk_postmux1,
	&t5w_cpu_clk_dyn,
	&t5w_cpu_clk,
	&t5w_dsu_clk_premux0,
	&t5w_dsu_clk_premux1,
	&t5w_dsu_clk_mux0_div,
	&t5w_dsu_clk_postmux0,
	&t5w_dsu_clk_mux1_div,
	&t5w_dsu_clk_postmux1,
	&t5w_dsu_clk_dyn,
	&t5w_dsu_final_clk,
	&t5w_dsu_clk,
	&t5w_clk81_ddr,
	&t5w_clk81_dos,
	&t5w_clk81_ethphy,
	&t5w_clk81_isa,
	&t5w_clk81_pl310,
	&t5w_clk81_periphs,
	&t5w_clk81_spicc0,
	&t5w_clk81_spicc1,
	&t5w_clk81_spicc2,
	&t5w_clk81_i2c,
	&t5w_clk81_sana,
	&t5w_clk81_smartcard,
	&t5w_clk81_uart0,
	&t5w_clk81_stream,
	&t5w_clk81_async_fifo,
	&t5w_clk81_tvfe,
	&t5w_clk81_hiu_reg,
	&t5w_clk81_hdmi20_aes,
	&t5w_clk81_hdmirx_pclk,
	&t5w_clk81_atv_demod,
	&t5w_clk81_assist_misc,
	&t5w_clk81_pwr_ctrl,
	&t5w_clk81_emmc_b,
	&t5w_clk81_emmc_c,
	&t5w_clk81_adec,
	&t5w_clk81_acodec,
	&t5w_clk81_tcon,
	&t5w_clk81_spi,
	&t5w_clk81_audio,
	&t5w_clk81_eth,
	&t5w_clk81_clk_rst,
	&t5w_clk81_aififo,
	&t5w_clk81_uart1,
	&t5w_clk81_g2d,
	&t5w_clk81_reset,
	&t5w_clk81_usb_gene,
	&t5w_clk81_ahb_arb0,
	&t5w_clk81_ahb_data,
	&t5w_clk81_ahb_ctrl,
	&t5w_clk81_usb1_to_ddr,
	&t5w_clk81_mmc_pclk,
	&t5w_clk81_hdmirx_axi,
	&t5w_clk81_hdcp22_pclk,
	&t5w_clk81_uart2,
	&t5w_clk81_ciplus,
	&t5w_clk81_ts,
	&t5w_clk81_vpu_int,
	&t5w_clk81_demod_com,
	&t5w_clk81_mute,
	&t5w_clk81_gic,
	&t5w_clk81_aucpu,
	&t5w_clk81_vclk2_venci0,
	&t5w_clk81_vclk2_venci1,
	&t5w_clk81_vclk2_vencp0,
	&t5w_clk81_vclk2_vencp1,
	&t5w_clk81_vclk2_venct0,
	&t5w_clk81_vclk2_venct1,
	&t5w_clk81_vclk2_other,
	&t5w_clk81_vclk2_enci,
	&t5w_clk81_vclk2_encp,
	&t5w_clk81_dac_clk,
	&t5w_clk81_enc480p,
	&t5w_clk81_ramdom,
	&t5w_clk81_vclk2_enct,
	&t5w_clk81_vclk2_encl,
	&t5w_clk81_vclk2_venclmmc,
	&t5w_clk81_vclk2_vencl,
	&t5w_clk81_vclk2_other1,
	&t5w_clk81_dma,
	&t5w_clk81_efuse,
	&t5w_clk81_rom_boot,
	&t5w_clk81_reset_sec,
	&t5w_clk81_sec_ahb,
	&t5w_clk81_rsa,
};

static int meson_t5w_dvfs_setup(struct platform_device *pdev)
{
	int ret;
	struct clk *notifier_clk;
	struct clk_hw *fclk_div2;
	struct clk_hw **hws = t5w_hw_onecell_data.hws;

	fclk_div2 = clk_hw_get_parent_by_index(hws[CLKID_CPU_CLK_DYN1_SEL], 1);

	/* Setup clock notifier for cpu_clk_postmux0 */
	t5w_cpu_clk_postmux0_nb_data.fclk_div2 = fclk_div2;
	notifier_clk = t5w_cpu_clk_postmux0.hw.clk;
	ret = clk_notifier_register(notifier_clk,
				    &t5w_cpu_clk_postmux0_nb_data.nb);
	if (ret) {
		dev_err(&pdev->dev, "failed to register the cpu_clk_postmux0 notifier\n");
		return ret;
	}

	/* Setup cluster 0 clock notifier for sys_pll */
	ret = clk_notifier_register(t5w_sys_pll.hw.clk,
				    &t5w_sys_pll_nb_data.nb);
	if (ret) {
		dev_err(&pdev->dev, "failed to register sys_pll notifier\n");
		return ret;
	}
	ret = clk_set_parent(t5w_dsu_clk_premux1.hw.clk,
			     t5w_fclk_div2.hw.clk);
	if (ret < 0) {
		pr_err("%s: failed to set dsu premux1 parent\n", __func__);
		return ret;
	}

	/* set gp1 to 1.2G */
	ret = clk_set_rate(t5w_gp1_pll.hw.clk, 1200000000);
	if (ret < 0) {
		pr_err("%s: failed to init gp1 1.2G\n", __func__);
		return ret;
	}
	clk_prepare_enable(t5w_gp1_pll.hw.clk);
	/* Switch dsu to gp1 */
	ret = clk_set_parent(t5w_dsu_clk_premux0.hw.clk,
			     t5w_gp1_pll.hw.clk);
	if (ret < 0) {
		pr_err("%s: failed to set dsu premux0 parent\n", __func__);
		return ret;
	}
	/* Switch dsu to dsu final */
	ret = clk_set_parent(t5w_dsu_clk.hw.clk,
			     t5w_dsu_final_clk.hw.clk);
	if (ret < 0) {
		pr_err("%s: failed to set dsu parent\n", __func__);
		return ret;
	}
	ret = clk_notifier_register(t5w_dsu_clk_postmux0.hw.clk,
				    &t5w_dsu_clk_postmux0_nb_data.nb);
	if (ret) {
		dev_err(&pdev->dev, "failed to register dsu notifier\n");
		return ret;
	}

	return 0;
}

static struct regmap_config clkc_regmap_config = {
	.reg_bits       = 32,
	.val_bits       = 32,
	.reg_stride     = 4,
};

static struct regmap *t5w_regmap_resource(struct device *dev, char *name)
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

static int __ref meson_t5w_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct regmap *basic_map;
	struct regmap *cpu_map;
	int ret, i;

	/* Get regmap for different clock area */
	basic_map = t5w_regmap_resource(dev, "basic");
	if (IS_ERR(basic_map)) {
		dev_err(dev, "basic clk registers not found\n");
		return PTR_ERR(basic_map);
	}

	cpu_map = t5w_regmap_resource(dev, "cpu");
	if (IS_ERR(cpu_map)) {
		dev_err(dev, "pll clk registers not found\n");
		return PTR_ERR(cpu_map);
	}

	/* Populate regmap for the regmap backed clocks */
	for (i = 0; i < ARRAY_SIZE(t5w_clk_regmaps); i++)
		t5w_clk_regmaps[i]->map = basic_map;

	for (i = 0; i < ARRAY_SIZE(t5w_cpu_clk_regmaps); i++)
		t5w_cpu_clk_regmaps[i]->map = cpu_map;

	regmap_write(basic_map, HHI_MPLL_CNTL0, 0x00000543);

	for (i = 0; i < t5w_hw_onecell_data.num; i++) {
		/* array might be sparse */
		if (!t5w_hw_onecell_data.hws[i])
			continue;

		ret = devm_clk_hw_register(dev, t5w_hw_onecell_data.hws[i]);
		if (ret) {
			dev_err(dev, "Clock registration failed\n");
			return ret;
		}
	}

	meson_t5w_dvfs_setup(pdev);

	return devm_of_clk_add_hw_provider(dev, of_clk_hw_onecell_get,
					   &t5w_hw_onecell_data);

	return 0;
}

static const struct of_device_id clkc_match_table[] = {
	{
		.compatible = "amlogic,t5w-clkc"
	},
	{}
};

static struct platform_driver t5w_driver = {
	.probe		= meson_t5w_probe,
	.driver		= {
		.name	= "t5w-clkc",
		.of_match_table = clkc_match_table,
	},
};

#ifndef CONFIG_AMLOGIC_MODIFY
builtin_platform_driver(t5w_driver);
#else
#ifndef MODULE
static int meson_t5w_clkc_init(void)
{
	return platform_driver_register(&t5w_driver);
}
arch_initcall_sync(meson_t5w_clkc_init);
#else
int __init meson_t5w_clkc_init(void)
{
	return platform_driver_register(&t5w_driver);
}
#endif
#endif

MODULE_LICENSE("GPL v2");
