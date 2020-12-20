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
#include "../clk-mpll.h"
#include "../clk-pll.h"
#include "../clk-regmap.h"
#include "../clk-cpu-dyndiv.h"
#include "../vid-pll-div.h"
#include "../meson-eeclk.h"
#include "t5d.h"
#include "../clkcs_init.h"

static DEFINE_SPINLOCK(meson_clk_lock);

static struct clk_regmap t5d_fixed_pll_dco = {
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
	},
};

static struct clk_regmap t5d_fixed_pll = {
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
			&t5d_fixed_pll_dco.hw
		},
		.num_parents = 1,
		/*
		 * This clock won't ever change at runtime so
		 * CLK_SET_RATE_PARENT is not required
		 */
	},
};

static const struct pll_mult_range t5d_sys_pll_mult_range = {
	.min = 128,
	.max = 250,
};

static const struct clk_ops meson_pll_clk_no_ops = {};

/*
 * the sys pll DCO value should be 3G~6G,
 * otherwise the sys pll can not lock.
 * od is for 32 bit.
 */

#ifdef CONFIG_ARM
static const struct pll_params_table t5d_sys_pll_params_table[] = {
	PLL_PARAMS(168, 1, 2), /*DCO=4032M OD=1008M*/
	PLL_PARAMS(184, 1, 2), /*DCO=4416M OD=1104M*/
	PLL_PARAMS(200, 1, 2), /*DCO=4800M OD=1200M*/
	PLL_PARAMS(216, 1, 2), /*DCO=5184M OD=1296M*/
	PLL_PARAMS(233, 1, 2), /*DCO=5592M OD=1398M*/
	PLL_PARAMS(249, 1, 2), /*DCO=5976M OD=1494M*/
	PLL_PARAMS(126, 1, 1), /*DCO=3024M OD=1512M*/
	PLL_PARAMS(134, 1, 1), /*DCO=3216M OD=1608M*/
	PLL_PARAMS(142, 1, 1), /*DCO=3408M OD=1704M*/
	PLL_PARAMS(150, 1, 1), /*DCO=3600M OD=1800M*/
	PLL_PARAMS(158, 1, 1), /*DCO=3792M OD=1896M*/
	PLL_PARAMS(159, 1, 1), /*DCO=3816M OD=1908*/
	PLL_PARAMS(160, 1, 1), /*DCO=3840M OD=1920M*/
	PLL_PARAMS(168, 1, 1), /*DCO=4032M OD=2016M*/
	{ /* sentinel */ },
};
#endif

static struct clk_regmap t5d_sys_pll_dco = {
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
			.width	 = 3,
		},
		.table = t5d_sys_pll_params_table,
#endif
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
		.range = &t5d_sys_pll_mult_range,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sys_pll_dco",
		.ops = &meson_clk_pll_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "xtal",
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
 *    it will be a lot of mass because of unit deferentces.
 *
 * Keep Consistent with 64bit, creat a Virtual clock for sys pll
 */
static struct clk_regmap t5d_sys_pll = {
	.hw.init = &(struct clk_init_data){
		.name = "sys_pll",
		.ops = &meson_pll_clk_no_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_sys_pll_dco.hw
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
static struct clk_regmap t5d_sys_pll = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_SYS_PLL_CNTL0,
		.shift = 16,
		.width = 3,
		.flags = CLK_DIVIDER_POWER_OF_TWO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sys_pll",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_sys_pll_dco.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};
#endif

static struct clk_fixed_factor t5d_fclk_div2_div = {
	.mult = 1,
	.div = 2,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div2_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) { &t5d_fixed_pll.hw },
		.num_parents = 1,
	},
};

static struct clk_regmap t5d_fclk_div2 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_FIX_PLL_CNTL1,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div2",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_fclk_div2_div.hw
		},
		.num_parents = 1,
		/*
		 * This clock feeds on CPU clock, it should be set
		 * by the platform to operate correctly.
		 */
		.flags = CLK_IS_CRITICAL,
	},
};

static struct clk_fixed_factor t5d_fclk_div3_div = {
	.mult = 1,
	.div = 3,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div3_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) { &t5d_fixed_pll.hw },
		.num_parents = 1,
	},
};

static struct clk_regmap t5d_fclk_div3 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_FIX_PLL_CNTL1,
		.bit_idx = 20,
	},
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div3",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_fclk_div3_div.hw
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

static struct clk_fixed_factor t5d_fclk_div4_div = {
	.mult = 1,
	.div = 4,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div4_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) { &t5d_fixed_pll.hw },
		.num_parents = 1,
	},
};

static struct clk_regmap t5d_fclk_div4 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_FIX_PLL_CNTL1,
		.bit_idx = 21,
	},
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div4",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_fclk_div4_div.hw
		},
		.num_parents = 1,
		/*
		 * This clock feeds on GPU, it should be set
		 * by the platform to operate correctly.
		 */
		.flags = CLK_IS_CRITICAL,
	},
};

static struct clk_fixed_factor t5d_fclk_div5_div = {
	.mult = 1,
	.div = 5,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div5_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) { &t5d_fixed_pll.hw },
		.num_parents = 1,
	},
};

static struct clk_regmap t5d_fclk_div5 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_FIX_PLL_CNTL1,
		.bit_idx = 22,
	},
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div5",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_fclk_div5_div.hw
		},
		.num_parents = 1,
		/*
		 * This clock feeds on GPU, it should be set
		 * by the platform to operate correctly.
		 */
		.flags = CLK_IS_CRITICAL,
	},
};

static struct clk_fixed_factor t5d_fclk_div7_div = {
	.mult = 1,
	.div = 7,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div7_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) { &t5d_fixed_pll.hw },
		.num_parents = 1,
	},
};

static struct clk_regmap t5d_fclk_div7 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_FIX_PLL_CNTL1,
		.bit_idx = 23,
	},
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div7",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_fclk_div7_div.hw
		},
		.num_parents = 1,
		/*
		 * This clock feeds on GPU, it should be set
		 * by the platform to operate correctly.
		 */
		.flags = CLK_IS_CRITICAL,
	},
};

static struct clk_fixed_factor t5d_fclk_div2p5_div = {
	.mult = 1,
	.div = 5,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div2p5_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_fixed_pll_dco.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap t5d_fclk_div2p5 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_FIX_PLL_CNTL1,
		.bit_idx = 25,
	},
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div2p5",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_fclk_div2p5_div.hw
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
static const struct pll_params_table t5d_gp0_pll_table[] = {
	PLL_PARAMS(141, 1, 2), /* DCO = 3384M OD = 2 PLL = 846M */
	PLL_PARAMS(130, 1, 2), /* DCO = 3120M OD = 2 PLL = 780M */
	PLL_PARAMS(132, 1, 2), /* DCO = 3168M OD = 2 PLL = 792M */
	PLL_PARAMS(248, 1, 3), /* DCO = 5952M OD = 3 PLL = 744M */
	{ /* sentinel */  },
};
#else
static const struct pll_params_table t5d_gp0_pll_table[] = {
	PLL_PARAMS(141, 1), /* DCO = 3384M OD = 2 PLL = 846M*/
	PLL_PARAMS(130, 1), /* DCO = 3120M OD = 2 PLL = 780M */
	PLL_PARAMS(132, 1), /* DCO = 3168M OD = 2 PLL = 792M */
	PLL_PARAMS(248, 1), /* DCO = 5952M OD = 3 PLL = 744M */
	{0, 0},
};
#endif

/*
 * Internal gp0 pll emulation configuration parameters
 */
static const struct reg_sequence t5d_gp0_init_regs[] = {
	{ .reg = HHI_GP0_PLL_CNTL1,	.def = 0x00000000 },
	{ .reg = HHI_GP0_PLL_CNTL2,	.def = 0x00000000 },
	{ .reg = HHI_GP0_PLL_CNTL3,	.def = 0x48681c00 },
	{ .reg = HHI_GP0_PLL_CNTL4,	.def = 0x33771290 },
	{ .reg = HHI_GP0_PLL_CNTL5,	.def = 0x39272000 },
	{ .reg = HHI_GP0_PLL_CNTL6,	.def = 0x56540000 },
};

static struct clk_regmap t5d_gp0_pll_dco = {
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
		.table = t5d_gp0_pll_table,
		.init_regs = t5d_gp0_init_regs,
		.init_count = ARRAY_SIZE(t5d_gp0_init_regs),
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
static struct clk_regmap t5d_gp0_pll = {
	.hw.init = &(struct clk_init_data){
		.name = "gp0_pll",
		.ops = &meson_pll_clk_no_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_gp0_pll_dco.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};
#else
static struct clk_regmap t5d_gp0_pll = {
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
			&t5d_gp0_pll_dco.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};
#endif

/*
 * Internal hifi pll emulation configuration parameters
 */
static const struct reg_sequence t5d_hifi_init_regs[] = {
	{ .reg = HHI_HIFI_PLL_CNTL1,	.def = 0x00000000 },
	{ .reg = HHI_HIFI_PLL_CNTL2,	.def = 0x00000000 },
	{ .reg = HHI_HIFI_PLL_CNTL3,	.def = 0x6a285c00 },
	{ .reg = HHI_HIFI_PLL_CNTL4,	.def = 0x65771290 },
	{ .reg = HHI_HIFI_PLL_CNTL5,	.def = 0x39272000 },
	{ .reg = HHI_HIFI_PLL_CNTL6,	.def = 0x56540000 },
};

static const struct pll_mult_range t5d_hifi_pll_mult_range = {
	.min = 128,
	.max = 250,
};

static struct clk_regmap t5d_hifi_pll_dco = {
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
		.range = &t5d_hifi_pll_mult_range,
		.init_regs = t5d_hifi_init_regs,
		.init_count = ARRAY_SIZE(t5d_hifi_init_regs),
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

static struct clk_regmap t5d_hifi_pll = {
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
			&t5d_hifi_pll_dco.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_fixed_factor t5d_mpll_50m_div = {
	.mult = 1,
	.div = 80,
	.hw.init = &(struct clk_init_data){
		.name = "mpll_50m_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_fixed_pll_dco.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap t5d_mpll_50m = {
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
			{ .hw = &t5d_mpll_50m_div.hw },
		},
		.num_parents = 2,
	},
};

static struct clk_fixed_factor t5d_mpll_prediv = {
	.mult = 1,
	.div = 2,
	.hw.init = &(struct clk_init_data){
		.name = "mpll_prediv",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_fixed_pll_dco.hw
		},
		.num_parents = 1,
	},
};

/*
 * put it in init regs, in kernel 4.9 it is dealing in set rate
 */
static const struct reg_sequence t5d_mpll0_init_regs[] = {
	{ .reg = HHI_MPLL_CNTL0,	.def = 0x00000543 },
	{ .reg = HHI_MPLL_CNTL2,	.def = 0x40000033 },
};

static struct clk_regmap t5d_mpll0_div = {
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
		.init_regs = t5d_mpll0_init_regs,
		.init_count = ARRAY_SIZE(t5d_mpll0_init_regs),
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpll0_div",
		.ops = &meson_clk_mpll_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_mpll_prediv.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap t5d_mpll0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_MPLL_CNTL1,
		.bit_idx = 31,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpll0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &t5d_mpll0_div.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct reg_sequence t5d_mpll1_init_regs[] = {
	{ .reg = HHI_MPLL_CNTL4,	.def = 0x40000033 },
};

static struct clk_regmap t5d_mpll1_div = {
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
		.init_regs = t5d_mpll1_init_regs,
		.init_count = ARRAY_SIZE(t5d_mpll1_init_regs),
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpll1_div",
		.ops = &meson_clk_mpll_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_mpll_prediv.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap t5d_mpll1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_MPLL_CNTL3,
		.bit_idx = 31,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpll1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &t5d_mpll1_div.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct reg_sequence t5d_mpll2_init_regs[] = {
	{ .reg = HHI_MPLL_CNTL6,	.def = 0x40000033 },
};

static struct clk_regmap t5d_mpll2_div = {
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
		.init_regs = t5d_mpll2_init_regs,
		.init_count = ARRAY_SIZE(t5d_mpll2_init_regs),
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpll2_div",
		.ops = &meson_clk_mpll_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_mpll_prediv.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap t5d_mpll2 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_MPLL_CNTL5,
		.bit_idx = 31,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpll2",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &t5d_mpll2_div.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct reg_sequence t5d_mpll3_init_regs[] = {
	{ .reg = HHI_MPLL_CNTL8,	.def = 0x40000033 },
};

static struct clk_regmap t5d_mpll3_div = {
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
		.init_regs = t5d_mpll3_init_regs,
		.init_count = ARRAY_SIZE(t5d_mpll3_init_regs),
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpll3_div",
		.ops = &meson_clk_mpll_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_mpll_prediv.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap t5d_mpll3 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_MPLL_CNTL7,
		.bit_idx = 31,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpll3",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &t5d_mpll3_div.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/* Datasheet names this field as "premux0" */
static struct clk_regmap t5d_cpu_clk_premux0 = {
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
			{ .hw = &t5d_fclk_div2.hw },
			{ .hw = &t5d_fclk_div3.hw },
		},
		.num_parents = 3,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/* Datasheet names this field as "premux1" */
static struct clk_regmap t5d_cpu_clk_premux1 = {
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
			{ .hw = &t5d_fclk_div2.hw },
			{ .hw = &t5d_fclk_div3.hw },
		},
		.num_parents = 3,
		/* This sub-tree is used a parking clock */
		.flags = CLK_SET_RATE_NO_REPARENT
	},
};

/* Datasheet names this field as "mux0_divn_tcnt" */
static struct clk_regmap t5d_cpu_clk_mux0_div = {
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
			&t5d_cpu_clk_premux0.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/* Datasheet names this field as "postmux0" */
static struct clk_regmap t5d_cpu_clk_postmux0 = {
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
			&t5d_cpu_clk_premux0.hw,
			&t5d_cpu_clk_mux0_div.hw,
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/* Datasheet names this field as "Mux1_divn_tcnt" */
static struct clk_regmap t5d_cpu_clk_mux1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_SYS_CPU_CLK_CNTL0,
		.shift = 20,
		.width = 6,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cpu_clk_dyn1_div",
		.ops = &clk_regmap_divider_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_cpu_clk_premux1.hw
		},
		.num_parents = 1,
	},
};

/* Datasheet names this field as "postmux1" */
static struct clk_regmap t5d_cpu_clk_postmux1 = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_SYS_CPU_CLK_CNTL0,
		.mask = 0x1,
		.shift = 18,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cpu_clk_dyn1",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_cpu_clk_premux1.hw,
			&t5d_cpu_clk_mux1_div.hw,
		},
		.num_parents = 2,
		/* This sub-tree is used a parking clock */
		.flags = CLK_SET_RATE_NO_REPARENT,
	},
};

/* Datasheet names this field as "Final_dyn_mux_sel" */
static struct clk_regmap t5d_cpu_clk_dyn = {
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
			&t5d_cpu_clk_postmux0.hw,
			&t5d_cpu_clk_postmux1.hw,
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/* Datasheet names this field as "Final_mux_sel" */
static struct clk_regmap t5d_cpu_clk = {
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
			&t5d_cpu_clk_dyn.hw,
			&t5d_sys_pll.hw,
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static int t5d_cpu_clk_mux_notifier_cb(struct notifier_block *nb,
				       unsigned long event, void *data)
{
	if (event == POST_RATE_CHANGE || event == PRE_RATE_CHANGE) {
		/* Wait for clock propagation before/after changing the mux */
		udelay(100);
		return NOTIFY_OK;
	}

	return NOTIFY_DONE;
}

static struct notifier_block t5d_cpu_clk_mux_nb = {
	.notifier_call = t5d_cpu_clk_mux_notifier_cb,
};

struct t5d_cpu_clk_postmux_nb_data {
	struct notifier_block nb;
	struct clk_hw *xtal;
	struct clk_hw *cpu_clk_dyn;
	struct clk_hw *cpu_clk_postmux0;
	struct clk_hw *cpu_clk_postmux1;
	struct clk_hw *cpu_clk_premux1;
};

static int t5d_cpu_clk_postmux_notifier_cb(struct notifier_block *nb,
					   unsigned long event, void *data)
{
	struct t5d_cpu_clk_postmux_nb_data *nb_data =
		container_of(nb, struct t5d_cpu_clk_postmux_nb_data, nb);

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

		/* Setup cpu_clk_premux1 to xtal */
		clk_hw_set_parent(nb_data->cpu_clk_premux1,
				  nb_data->xtal);

		/* Setup cpu_clk_postmux1 to bypass divider */
		clk_hw_set_parent(nb_data->cpu_clk_postmux1,
				  nb_data->cpu_clk_premux1);

		/* Switch to parking clk on cpu_clk_postmux1 */
		clk_hw_set_parent(nb_data->cpu_clk_dyn,
				  nb_data->cpu_clk_postmux1);

		/*
		 * Now, cpu_clk is 24MHz in the current path :
		 * cpu_clk
		 *    \- cpu_clk_dyn
		 *          \- cpu_clk_postmux1
		 *                \- cpu_clk_premux1
		 *                      \- xtal
		 */

		udelay(100);

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

		udelay(100);

		return NOTIFY_OK;

	default:
		return NOTIFY_DONE;
	}
}

static struct t5d_cpu_clk_postmux_nb_data t5d_cpu_clk_postmux0_nb_data = {
	.cpu_clk_dyn = &t5d_cpu_clk_dyn.hw,
	.cpu_clk_postmux0 = &t5d_cpu_clk_postmux0.hw,
	.cpu_clk_postmux1 = &t5d_cpu_clk_postmux1.hw,
	.cpu_clk_premux1 = &t5d_cpu_clk_premux1.hw,
	.nb.notifier_call = t5d_cpu_clk_postmux_notifier_cb,
};

struct t5d_sys_pll_nb_data {
	struct notifier_block nb;
	struct clk_hw *sys_pll;
	struct clk_hw *cpu_clk;
	struct clk_hw *cpu_clk_dyn;
};

static int t5d_sys_pll_notifier_cb(struct notifier_block *nb,
				   unsigned long event, void *data)
{
	struct t5d_sys_pll_nb_data *nb_data =
		container_of(nb, struct t5d_sys_pll_nb_data, nb);

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

static struct t5d_sys_pll_nb_data t5d_sys_pll_nb_data = {
	.sys_pll = &t5d_sys_pll.hw,
	.cpu_clk = &t5d_cpu_clk.hw,
	.cpu_clk_dyn = &t5d_cpu_clk_dyn.hw,
	.nb.notifier_call = t5d_sys_pll_notifier_cb,
};

#define MESON_GATE(_name, _reg, _bit)						\
struct clk_regmap _name = {                                             \
		.data = &(struct clk_regmap_gate_data){                         \
			.offset = (_reg),                                       \
			.bit_idx = (_bit),                                      \
		},                                                              \
		.hw.init = &(struct clk_init_data) {                            \
			.name = #_name,                                         \
			.ops = &clk_regmap_gate_ops,                            \
			.parent_names = (const char *[]){ "clk81" },		\
			.num_parents = 1,                                       \
			.flags = (CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED),     \
		},                                                              \
}

static MESON_GATE(t5d_clk81_ddr,		HHI_GCLK_MPEG0, 0);
static MESON_GATE(t5d_clk81_dos,		HHI_GCLK_MPEG0, 1);
static MESON_GATE(t5d_clk81_ethphy,	HHI_GCLK_MPEG0, 4);
static MESON_GATE(t5d_clk81_isa,		HHI_GCLK_MPEG0, 5);
static MESON_GATE(t5d_clk81_pl310,	HHI_GCLK_MPEG0, 6);
static MESON_GATE(t5d_clk81_periphs,	HHI_GCLK_MPEG0, 7);
static MESON_GATE(t5d_clk81_spicc0,	HHI_GCLK_MPEG0, 8);
static MESON_GATE(t5d_clk81_i2c,		HHI_GCLK_MPEG0, 9);
static MESON_GATE(t5d_clk81_sana,		HHI_GCLK_MPEG0, 10);
static MESON_GATE(t5d_clk81_uart0,	HHI_GCLK_MPEG0, 13);
static MESON_GATE(t5d_clk81_stream,	HHI_GCLK_MPEG0, 15);
static MESON_GATE(t5d_clk81_async_fifo,	HHI_GCLK_MPEG0, 16);
static MESON_GATE(t5d_clk81_tvfe,		HHI_GCLK_MPEG0, 18);
static MESON_GATE(t5d_clk81_hiu_reg,	HHI_GCLK_MPEG0, 19);
static MESON_GATE(t5d_clk81_hdmirx_pclk,	HHI_GCLK_MPEG0, 21);
static MESON_GATE(t5d_clk81_atv_demod,	HHI_GCLK_MPEG0, 22);
static MESON_GATE(t5d_clk81_assist_misc,	HHI_GCLK_MPEG0, 23);
static MESON_GATE(t5d_clk81_pwr_ctrl,	HHI_GCLK_MPEG0, 24);
static MESON_GATE(t5d_clk81_emmc_c,	HHI_GCLK_MPEG0, 26);
static MESON_GATE(t5d_clk81_adec,		HHI_GCLK_MPEG0, 27);
static MESON_GATE(t5d_clk81_acodec,	HHI_GCLK_MPEG0, 28);
static MESON_GATE(t5d_clk81_tcon,		HHI_GCLK_MPEG0, 29);
static MESON_GATE(t5d_clk81_spi,		HHI_GCLK_MPEG0, 30);

static MESON_GATE(t5d_clk81_audio,	HHI_GCLK_MPEG1, 0);
static MESON_GATE(t5d_clk81_eth,		HHI_GCLK_MPEG1, 3);
static MESON_GATE(t5d_clk81_top_demux,	HHI_GCLK_MPEG1, 4);
static MESON_GATE(t5d_clk81_clk_rst,	HHI_GCLK_MPEG1, 5);
static MESON_GATE(t5d_clk81_aififo,	HHI_GCLK_MPEG1, 11);
static MESON_GATE(t5d_clk81_uart1,	HHI_GCLK_MPEG1, 16);
static MESON_GATE(t5d_clk81_g2d,		HHI_GCLK_MPEG1, 20);
static MESON_GATE(t5d_clk81_reset,	HHI_GCLK_MPEG1, 23);
static MESON_GATE(t5d_clk81_parser0,	HHI_GCLK_MPEG1, 25);
static MESON_GATE(t5d_clk81_usb_gene,	HHI_GCLK_MPEG1, 26);
static MESON_GATE(t5d_clk81_parser1,	HHI_GCLK_MPEG1, 28);
static MESON_GATE(t5d_clk81_ahb_arb0,	HHI_GCLK_MPEG1, 29);

static MESON_GATE(t5d_clk81_ahb_data,	HHI_GCLK_MPEG2, 1);
static MESON_GATE(t5d_clk81_ahb_ctrl,	HHI_GCLK_MPEG2, 2);
static MESON_GATE(t5d_clk81_usb1_to_ddr,	HHI_GCLK_MPEG2, 8);
static MESON_GATE(t5d_clk81_mmc_pclk,	HHI_GCLK_MPEG2, 11);
static MESON_GATE(t5d_clk81_hdmirx_axi,	HHI_GCLK_MPEG2, 12);
static MESON_GATE(t5d_clk81_hdcp22_pclk,	HHI_GCLK_MPEG2, 13);
static MESON_GATE(t5d_clk81_uart2,	HHI_GCLK_MPEG2, 15);
static MESON_GATE(t5d_clk81_ts,		HHI_GCLK_MPEG2, 22);
static MESON_GATE(t5d_clk81_vpu_int,	HHI_GCLK_MPEG2, 25);
static MESON_GATE(t5d_clk81_demod_com,	HHI_GCLK_MPEG2, 28);
static MESON_GATE(t5d_clk81_gic,		HHI_GCLK_MPEG2, 30);

static MESON_GATE(t5d_clk81_vclk2_venci0,	HHI_GCLK_OTHER, 1);
static MESON_GATE(t5d_clk81_vclk2_venci1,	HHI_GCLK_OTHER, 2);
static MESON_GATE(t5d_clk81_vclk2_vencp0,	HHI_GCLK_OTHER, 3);
static MESON_GATE(t5d_clk81_vclk2_vencp1,	HHI_GCLK_OTHER, 4);
static MESON_GATE(t5d_clk81_vclk2_venct0,	HHI_GCLK_OTHER, 5);
static MESON_GATE(t5d_clk81_vclk2_venct1,	HHI_GCLK_OTHER, 6);
static MESON_GATE(t5d_clk81_vclk2_other,	HHI_GCLK_OTHER, 7);
static MESON_GATE(t5d_clk81_vclk2_enci,	HHI_GCLK_OTHER, 8);
static MESON_GATE(t5d_clk81_vclk2_encp,	HHI_GCLK_OTHER, 9);
static MESON_GATE(t5d_clk81_dac_clk,	HHI_GCLK_OTHER, 10);
static MESON_GATE(t5d_clk81_enc480p,	HHI_GCLK_OTHER, 20);
static MESON_GATE(t5d_clk81_ramdom,	HHI_GCLK_OTHER, 21);
static MESON_GATE(t5d_clk81_vclk2_enct,	HHI_GCLK_OTHER, 22);
static MESON_GATE(t5d_clk81_vclk2_encl,	HHI_GCLK_OTHER, 23);
static MESON_GATE(t5d_clk81_vclk2_venclmmc,	HHI_GCLK_OTHER, 24);
static MESON_GATE(t5d_clk81_vclk2_vencl,	HHI_GCLK_OTHER, 25);
static MESON_GATE(t5d_clk81_vclk2_other1,	HHI_GCLK_OTHER, 26);

static MESON_GATE(t5d_clk81_dma,		HHI_GCLK_AO, 0);
static MESON_GATE(t5d_clk81_efuse,	HHI_GCLK_AO, 1);
static MESON_GATE(t5d_clk81_rom_boot,	HHI_GCLK_AO, 2);
static MESON_GATE(t5d_clk81_reset_sec,	HHI_GCLK_AO, 3);
static MESON_GATE(t5d_clk81_sec_ahb,	HHI_GCLK_AO, 4);
static MESON_GATE(t5d_clk81_rsa,		HHI_GCLK_AO, 5);

/* Array of all clocks provided by this provider */
static struct clk_hw_onecell_data t5d_hw_onecell_data = {
	.hws = {
		[CLKID_FIXED_PLL_DCO]	= &t5d_fixed_pll_dco.hw,
		[CLKID_SYS_PLL_DCO]	= &t5d_sys_pll_dco.hw,
		[CLKID_GP0_PLL_DCO]	= &t5d_gp0_pll_dco.hw,
		[CLKID_HIFI_PLL_DCO]	= &t5d_hifi_pll_dco.hw,
		[CLKID_FIXED_PLL]	= &t5d_fixed_pll.hw,
		[CLKID_SYS_PLL]		= &t5d_sys_pll.hw,
		[CLKID_GP0_PLL]		= &t5d_gp0_pll.hw,
		[CLKID_HIFI_PLL]	= &t5d_hifi_pll.hw,
		[CLKID_FCLK_DIV2_DIV]	= &t5d_fclk_div2_div.hw,
		[CLKID_FCLK_DIV3_DIV]	= &t5d_fclk_div3_div.hw,
		[CLKID_FCLK_DIV4_DIV]	= &t5d_fclk_div4_div.hw,
		[CLKID_FCLK_DIV5_DIV]	= &t5d_fclk_div5_div.hw,
		[CLKID_FCLK_DIV7_DIV]	= &t5d_fclk_div7_div.hw,
		[CLKID_FCLK_DIV2P5_DIV]	= &t5d_fclk_div2p5_div.hw,
		[CLKID_FCLK_DIV2]	= &t5d_fclk_div2.hw,
		[CLKID_FCLK_DIV3]	= &t5d_fclk_div3.hw,
		[CLKID_FCLK_DIV4]	= &t5d_fclk_div4.hw,
		[CLKID_FCLK_DIV5]	= &t5d_fclk_div5.hw,
		[CLKID_FCLK_DIV7]	= &t5d_fclk_div7.hw,
		[CLKID_FCLK_DIV2P5]	= &t5d_fclk_div2p5.hw,
		[CLKID_PRE_MPLL]	= &t5d_mpll_prediv.hw,
		[CLKID_MPLL0_DIV]       = &t5d_mpll0_div.hw,
		[CLKID_MPLL1_DIV]       = &t5d_mpll1_div.hw,
		[CLKID_MPLL2_DIV]       = &t5d_mpll2_div.hw,
		[CLKID_MPLL3_DIV]       = &t5d_mpll3_div.hw,
		[CLKID_MPLL0]           = &t5d_mpll0.hw,
		[CLKID_MPLL1]           = &t5d_mpll1.hw,
		[CLKID_MPLL2]           = &t5d_mpll2.hw,
		[CLKID_MPLL3]           = &t5d_mpll3.hw,
		[CLKID_CPU_CLK_DYN0_SEL]	= &t5d_cpu_clk_premux0.hw,
		[CLKID_CPU_CLK_DYN0_DIV]	= &t5d_cpu_clk_mux0_div.hw,
		[CLKID_CPU_CLK_DYN0]		= &t5d_cpu_clk_postmux0.hw,
		[CLKID_CPU_CLK_DYN1_SEL]	= &t5d_cpu_clk_premux1.hw,
		[CLKID_CPU_CLK_DYN1_DIV]	= &t5d_cpu_clk_mux1_div.hw,
		[CLKID_CPU_CLK_DYN1]		= &t5d_cpu_clk_postmux1.hw,
		[CLKID_CPU_CLK_DYN]		= &t5d_cpu_clk_dyn.hw,
		[CLKID_CPU_CLK]			= &t5d_cpu_clk.hw,
		[CLKID_MPLL_50M_DIV]	= &t5d_mpll_50m_div.hw,
		[CLKID_MPLL_50M]	= &t5d_mpll_50m.hw,
		[CLKID_DDR]		= &t5d_clk81_ddr.hw,
		[CLKID_DOS]		= &t5d_clk81_dos.hw,
		[CLKID_ETH_PHY]		= &t5d_clk81_ethphy.hw,
		[CLKID_ISA]		= &t5d_clk81_isa.hw,
		[CLKID_PL310]		= &t5d_clk81_pl310.hw,
		[CLKID_PERIPHS]		= &t5d_clk81_periphs.hw,
		[CLKID_SPICC0]		= &t5d_clk81_spicc0.hw,
		[CLKID_I2C]		= &t5d_clk81_i2c.hw,
		[CLKID_SANA]		= &t5d_clk81_sana.hw,
		[CLKID_UART0]		= &t5d_clk81_uart0.hw,
		[CLKID_STREAM]		= &t5d_clk81_stream.hw,
		[CLKID_ASYNC_FIFO]	= &t5d_clk81_async_fifo.hw,
		[CLKID_TVFE]		= &t5d_clk81_tvfe.hw,
		[CLKID_HIU_REG]		= &t5d_clk81_hiu_reg.hw,
		[CLKID_HDMIRX_PCLK]	= &t5d_clk81_hdmirx_pclk.hw,
		[CLKID_ATV_DEMOD]	= &t5d_clk81_atv_demod.hw,
		[CLKID_ASSIST_MISC]	= &t5d_clk81_assist_misc.hw,
		[CLKID_PWR_CTRL]	= &t5d_clk81_pwr_ctrl.hw,
		[CLKID_SD_EMMC_C]	= &t5d_clk81_emmc_c.hw,
		[CLKID_ADEC]		= &t5d_clk81_adec.hw,
		[CLKID_ACODEC]		= &t5d_clk81_acodec.hw,
		[CLKID_TCON]		= &t5d_clk81_tcon.hw,
		[CLKID_SPI]		= &t5d_clk81_spi.hw,
		[CLKID_AUDIO]		= &t5d_clk81_audio.hw,
		[CLKID_ETH_CORE]	= &t5d_clk81_eth.hw,
		[CLKID_DEMUX]		= &t5d_clk81_top_demux.hw,
		[CLKID_CLK_RST]		= &t5d_clk81_clk_rst.hw,
		[CLKID_AIFIFO]		= &t5d_clk81_aififo.hw,
		[CLKID_UART1]		= &t5d_clk81_uart1.hw,
		[CLKID_G2D]		= &t5d_clk81_g2d.hw,
		[CLKID_RESET]		= &t5d_clk81_reset.hw,
		[CLKID_PARSER0]		= &t5d_clk81_parser0.hw,
		[CLKID_USB_GENERAL]	= &t5d_clk81_usb_gene.hw,
		[CLKID_PARSER1]		= &t5d_clk81_parser1.hw,
		[CLKID_AHB_ARB0]	= &t5d_clk81_ahb_arb0.hw,
		[CLKID_AHB_DATA_BUS]	= &t5d_clk81_ahb_data.hw,
		[CLKID_AHB_CTRL_BUS]	= &t5d_clk81_ahb_ctrl.hw,
		[CLKID_USB1_TO_DDR]	= &t5d_clk81_usb1_to_ddr.hw,
		[CLKID_MMC_PCLK]	= &t5d_clk81_mmc_pclk.hw,
		[CLKID_HDMIRX_AXI]	= &t5d_clk81_hdmirx_axi.hw,
		[CLKID_HDCP22_PCLK]	= &t5d_clk81_hdcp22_pclk.hw,
		[CLKID_UART2]		= &t5d_clk81_uart2.hw,
		[CLKID_CLK81_TS]	= &t5d_clk81_ts.hw,
		[CLKID_VPU_INTR]	= &t5d_clk81_vpu_int.hw,
		[CLKID_DEMOD_COMB]	= &t5d_clk81_demod_com.hw,
		[CLKID_GIC]		= &t5d_clk81_gic.hw,
		[CLKID_VCLK2_VENCI0]	= &t5d_clk81_vclk2_venci0.hw,
		[CLKID_VCLK2_VENCI1]	= &t5d_clk81_vclk2_venci1.hw,
		[CLKID_VCLK2_VENCP0]	= &t5d_clk81_vclk2_vencp0.hw,
		[CLKID_VCLK2_VENCP1]	= &t5d_clk81_vclk2_vencp1.hw,
		[CLKID_VCLK2_VENCT0]	= &t5d_clk81_vclk2_venct0.hw,
		[CLKID_VCLK2_VENCT1]	= &t5d_clk81_vclk2_venct1.hw,
		[CLKID_VCLK2_OTHER]	= &t5d_clk81_vclk2_other.hw,
		[CLKID_VCLK2_ENCI]	= &t5d_clk81_vclk2_enci.hw,
		[CLKID_VCLK2_ENCP]	= &t5d_clk81_vclk2_encp.hw,
		[CLKID_DAC_CLK]		= &t5d_clk81_dac_clk.hw,
		[CLKID_ENC480P]		= &t5d_clk81_enc480p.hw,
		[CLKID_RAMDOM]		= &t5d_clk81_ramdom.hw,
		[CLKID_VCLK2_ENCT]	= &t5d_clk81_vclk2_enct.hw,
		[CLKID_VCLK2_ENCL]	= &t5d_clk81_vclk2_encl.hw,
		[CLKID_VCLK2_VENCLMMC]	= &t5d_clk81_vclk2_venclmmc.hw,
		[CLKID_VCLK2_VENCL]	= &t5d_clk81_vclk2_vencl.hw,
		[CLKID_VCLK2_OTHER1]	= &t5d_clk81_vclk2_other1.hw,
		[CLKID_DMA]		= &t5d_clk81_dma.hw,
		[CLKID_EFUSE]		= &t5d_clk81_efuse.hw,
		[CLKID_ROM_BOOT]	= &t5d_clk81_rom_boot.hw,
		[CLKID_RESET_SEC]	= &t5d_clk81_reset_sec.hw,
		[CLKID_SEC_AHB]		= &t5d_clk81_sec_ahb.hw,
		[CLKID_RSA]		= &t5d_clk81_rsa.hw,/* clk81 gate over */
		[NR_CLKS]		= NULL,
	},
	.num = NR_CLKS,
};

/* Convenience table to populate regmap in .probe */
static struct clk_regmap *const t5d_clk_regmaps[] = {
	&t5d_fixed_pll,
	&t5d_sys_pll,
	&t5d_gp0_pll,
	&t5d_hifi_pll,
	&t5d_fixed_pll_dco,
	&t5d_sys_pll_dco,
	&t5d_gp0_pll_dco,
	&t5d_hifi_pll_dco,
	&t5d_fclk_div2,
	&t5d_fclk_div3,
	&t5d_fclk_div4,
	&t5d_fclk_div5,
	&t5d_fclk_div7,
	&t5d_fclk_div2p5,
	&t5d_mpll0_div,
	&t5d_mpll1_div,
	&t5d_mpll2_div,
	&t5d_mpll3_div,
	&t5d_mpll0,
	&t5d_mpll1,
	&t5d_mpll2,
	&t5d_mpll3,
	&t5d_clk81_ddr,
	&t5d_clk81_dos,
	&t5d_clk81_ethphy,
	&t5d_clk81_isa,
	&t5d_clk81_pl310,
	&t5d_clk81_periphs,
	&t5d_clk81_spicc0,
	&t5d_clk81_i2c,
	&t5d_clk81_sana,
	&t5d_clk81_uart0,
	&t5d_clk81_stream,
	&t5d_clk81_async_fifo,
	&t5d_clk81_tvfe,
	&t5d_clk81_hiu_reg,
	&t5d_clk81_hdmirx_pclk,
	&t5d_clk81_atv_demod,
	&t5d_clk81_assist_misc,
	&t5d_clk81_pwr_ctrl,
	&t5d_clk81_emmc_c,
	&t5d_clk81_adec,
	&t5d_clk81_acodec,
	&t5d_clk81_tcon,
	&t5d_clk81_spi,
	&t5d_clk81_audio,
	&t5d_clk81_eth,
	&t5d_clk81_top_demux,
	&t5d_clk81_clk_rst,
	&t5d_clk81_aififo,
	&t5d_clk81_uart1,
	&t5d_clk81_g2d,
	&t5d_clk81_reset,
	&t5d_clk81_parser0,
	&t5d_clk81_usb_gene,
	&t5d_clk81_parser1,
	&t5d_clk81_ahb_arb0,
	&t5d_clk81_ahb_data,
	&t5d_clk81_ahb_ctrl,
	&t5d_clk81_usb1_to_ddr,
	&t5d_clk81_mmc_pclk,
	&t5d_clk81_hdmirx_axi,
	&t5d_clk81_hdcp22_pclk,
	&t5d_clk81_uart2,
	&t5d_clk81_ts,
	&t5d_clk81_vpu_int,
	&t5d_clk81_demod_com,
	&t5d_clk81_gic,
	&t5d_clk81_vclk2_venci0,
	&t5d_clk81_vclk2_venci1,
	&t5d_clk81_vclk2_vencp0,
	&t5d_clk81_vclk2_vencp1,
	&t5d_clk81_vclk2_venct0,
	&t5d_clk81_vclk2_venct1,
	&t5d_clk81_vclk2_other,
	&t5d_clk81_vclk2_enci,
	&t5d_clk81_vclk2_encp,
	&t5d_clk81_dac_clk,
	&t5d_clk81_enc480p,
	&t5d_clk81_ramdom,
	&t5d_clk81_vclk2_enct,
	&t5d_clk81_vclk2_encl,
	&t5d_clk81_vclk2_venclmmc,
	&t5d_clk81_vclk2_vencl,
	&t5d_clk81_vclk2_other1,
	&t5d_clk81_dma,
	&t5d_clk81_efuse,
	&t5d_clk81_rom_boot,
	&t5d_clk81_reset_sec,
	&t5d_clk81_sec_ahb,
	&t5d_clk81_rsa,/* clk81 gate over */
	/*cpu regmap*/
	&t5d_cpu_clk_premux0,
	&t5d_cpu_clk_mux0_div,
	&t5d_cpu_clk_postmux0,
	&t5d_cpu_clk_premux1,
	&t5d_cpu_clk_mux1_div,
	&t5d_cpu_clk_postmux1,
	&t5d_cpu_clk_dyn,
	&t5d_cpu_clk,
	&t5d_mpll_50m,
};

static const struct reg_sequence t5d_init_regs[] = {
	{ .reg = HHI_MPLL_CNTL0,	.def = 0x00000543 },
};

static int meson_t5d_dvfs_setup_common(struct platform_device *pdev,
					struct clk_hw **hws)
{
	struct clk *notifier_clk;
	struct clk_hw *xtal;
	int ret;

	xtal = clk_hw_get_parent_by_index(hws[CLKID_CPU_CLK_DYN1_SEL], 0);

	/* Setup clock notifier for cpu_clk_postmux0 */
	t5d_cpu_clk_postmux0_nb_data.xtal = xtal;
	notifier_clk = t5d_cpu_clk_postmux0.hw.clk;
	ret = clk_notifier_register(notifier_clk,
				    &t5d_cpu_clk_postmux0_nb_data.nb);
	if (ret) {
		dev_err(&pdev->dev, "failed to register the cpu_clk_postmux0 notifier\n");
		return ret;
	}

	/* Setup clock notifier for cpu_clk_dyn mux */
	notifier_clk = t5d_cpu_clk_dyn.hw.clk;
	ret = clk_notifier_register(notifier_clk, &t5d_cpu_clk_mux_nb);
	if (ret) {
		dev_err(&pdev->dev, "failed to register the cpu_clk_dyn notifier\n");
		return ret;
	}

	return 0;
}

static int meson_t5d_dvfs_setup(struct platform_device *pdev)
{
	struct clk_hw **hws = t5d_hw_onecell_data.hws;
	struct clk *notifier_clk;
	int ret;

	ret = meson_t5d_dvfs_setup_common(pdev, hws);
	if (ret)
		return ret;

	/* Setup clock notifier for cpu_clk mux */
	notifier_clk = t5d_cpu_clk.hw.clk;
	ret = clk_notifier_register(notifier_clk, &t5d_cpu_clk_mux_nb);
	if (ret) {
		dev_err(&pdev->dev, "failed to register the cpu_clk notifier\n");
		return ret;
	}

	/* Setup clock notifier for sys_pll */
	notifier_clk = t5d_sys_pll.hw.clk;
	ret = clk_notifier_register(notifier_clk, &t5d_sys_pll_nb_data.nb);
	if (ret) {
		dev_err(&pdev->dev, "failed to register the sys_pll notifier\n");
		return ret;
	}

	return 0;
}

struct meson_t5d_data {
	const struct meson_eeclkc_data eeclkc_data;
	int (*dvfs_setup)(struct platform_device *pdev);
};

static int meson_t5d_probe(struct platform_device *pdev)
{
	const struct meson_eeclkc_data *eeclkc_data;
	const struct meson_t5d_data *t5d_data;
	int ret;

	eeclkc_data = of_device_get_match_data(&pdev->dev);
	if (!eeclkc_data)
		return -EINVAL;

	ret = meson_eeclkc_probe(pdev);
	if (ret)
		return ret;

	t5d_data = container_of(eeclkc_data, struct meson_t5d_data,
				 eeclkc_data);

	if (t5d_data->dvfs_setup)
		return t5d_data->dvfs_setup(pdev);

	return 0;
}

static const struct meson_t5d_data t5d_clkc_data = {
	.eeclkc_data = {
		.regmap_clks = t5d_clk_regmaps,
		.regmap_clk_num = ARRAY_SIZE(t5d_clk_regmaps),
		.hw_onecell_data = &t5d_hw_onecell_data,
		.init_regs = t5d_init_regs,
		.init_count = ARRAY_SIZE(t5d_init_regs),
	},
	.dvfs_setup = meson_t5d_dvfs_setup,
};

static const struct of_device_id clkc_match_table[] = {
	{
		.compatible = "amlogic,t5d-clkc",
		.data = &t5d_clkc_data.eeclkc_data
	},
	{}
};

static struct platform_driver t5d_driver = {
	.probe		= meson_t5d_probe,
	.driver		= {
		.name	= "t5d-clkc",
		.of_match_table = clkc_match_table,
	},
};

#ifndef MODULE
static int t5d_clkc_init(void)
{
	return platform_driver_register(&t5d_driver);
}

arch_initcall_sync(t5d_clkc_init);
#else
int __init meson_t5d_clkc_init(void)
{
	return platform_driver_register(&t5d_driver);
}
#endif

MODULE_LICENSE("GPL v2");
