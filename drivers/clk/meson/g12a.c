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
#include "g12a.h"
#include "clkcs_init.h"

static DEFINE_SPINLOCK(meson_clk_lock);

static struct clk_regmap g12a_fixed_pll_dco = {
	.data = &(struct meson_clk_pll_data){
		.en = {
			.reg_off = HHI_FIX_PLL_CNTL0,
			.shift   = 28,
			.width   = 1,
		},
		.m = {
			.reg_off = HHI_FIX_PLL_CNTL0,
			.shift   = 0,
			.width   = 8,
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

static struct clk_regmap g12a_fixed_pll = {
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
			&g12a_fixed_pll_dco.hw
		},
		.num_parents = 1,
		/*
		 * This clock won't ever change at runtime so
		 * CLK_SET_RATE_PARENT is not required
		 */
	},
};

static const struct pll_mult_range g12a_sys_pll_mult_range = {
#ifdef CONFIG_AMLOGIC_MODIFY
	.min = 125,
#else
	.min = 128,
#endif
	.max = 250,
};

static const struct clk_ops meson_pll_clk_no_ops = {};

/*
 * the sys pll DCO value should be 3G~6G,
 * otherwise the sys pll can not lock.
 * od is for 32 bit.
 */

#ifdef CONFIG_ARM
static const struct pll_params_table g12a_sys_pll_params_table[] = {
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

static struct clk_regmap g12a_sys_pll_dco = {
	.data = &(struct meson_clk_pll_data){
		.en = {
			.reg_off = HHI_SYS_PLL_CNTL0,
			.shift   = 28,
			.width   = 1,
		},
		.m = {
			.reg_off = HHI_SYS_PLL_CNTL0,
			.shift   = 0,
			.width   = 8,
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
		.table = g12a_sys_pll_params_table,
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
		.range = &g12a_sys_pll_mult_range,
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
 *    it will be a lot of mass because of unit differences.
 *
 * Keep Consistent with 64bit, creat a Virtual clock for sys pll
 */
static struct clk_regmap g12a_sys_pll = {
	.hw.init = &(struct clk_init_data){
		.name = "sys_pll",
		.ops = &meson_pll_clk_no_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_sys_pll_dco.hw
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
static struct clk_regmap g12a_sys_pll = {
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
			&g12a_sys_pll_dco.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};
#endif

static struct clk_regmap g12b_sys1_pll_dco = {
	.data = &(struct meson_clk_pll_data){
		.en = {
			.reg_off = HHI_SYS1_PLL_CNTL0,
			.shift   = 28,
			.width   = 1,
		},
		.m = {
			.reg_off = HHI_SYS1_PLL_CNTL0,
			.shift   = 0,
			.width   = 8,
		},
		.n = {
			.reg_off = HHI_SYS1_PLL_CNTL0,
			.shift   = 10,
			.width   = 5,
		},
		.l = {
			.reg_off = HHI_SYS1_PLL_CNTL0,
			.shift   = 31,
			.width   = 1,
		},
		.rst = {
			.reg_off = HHI_SYS1_PLL_CNTL0,
			.shift   = 29,
			.width   = 1,
		},
		.range = &g12a_sys_pll_mult_range,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sys1_pll_dco",
		.ops = &meson_clk_pll_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "xtal",
		},
		.num_parents = 1,
		/* This clock feeds the CPU, avoid disabling it */
		.flags = CLK_IS_CRITICAL,
	},
};

static struct clk_regmap g12b_sys1_pll = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_SYS1_PLL_CNTL0,
		.shift = 16,
		.width = 3,
		.flags = CLK_DIVIDER_POWER_OF_TWO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sys1_pll",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12b_sys1_pll_dco.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap g12a_sys_pll_div16_en = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_SYS_CPU_CLK_CNTL1,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sys_pll_div16_en",
		.ops = &clk_regmap_gate_ro_ops,
		.parent_hws = (const struct clk_hw *[]) { &g12a_sys_pll.hw },
		.num_parents = 1,
		/*
		 * This clock is used to debug the sys_pll range
		 * Linux should not change it at runtime
		 */
	},
};

static struct clk_regmap g12b_sys1_pll_div16_en = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_SYS_CPUB_CLK_CNTL1,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sys1_pll_div16_en",
		.ops = &clk_regmap_gate_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12b_sys1_pll.hw
		},
		.num_parents = 1,
		/*
		 * This clock is used to debug the sys_pll range
		 * Linux should not change it at runtime
		 */
	},
};

static struct clk_fixed_factor g12a_sys_pll_div16 = {
	.mult = 1,
	.div = 16,
	.hw.init = &(struct clk_init_data){
		.name = "sys_pll_div16",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_sys_pll_div16_en.hw
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor g12b_sys1_pll_div16 = {
	.mult = 1,
	.div = 16,
	.hw.init = &(struct clk_init_data){
		.name = "sys1_pll_div16",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12b_sys1_pll_div16_en.hw
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor g12a_fclk_div2_div = {
	.mult = 1,
	.div = 2,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div2_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) { &g12a_fixed_pll.hw },
		.num_parents = 1,
	},
};

static struct clk_regmap g12a_fclk_div2 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_FIX_PLL_CNTL1,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div2",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_fclk_div2_div.hw
		},
		.num_parents = 1,
		/*
		 * This clock feeds on CPU clock, it should be set
		 * by the platform to operate correctly.
		 * Similar to fclk_div3, it seems that this clock is used by
		 * the resident firmware and is required by the platform to
		 * operate correctly.
		 * Until the following condition are met, we need this clock to
		 * be marked as critical:
		 * a) Mark the clock used by a firmware resource, if possible
		 * b) CCF has a clock hand-off mechanism to make the sure the
		 *    clock stays on until the proper driver comes along
		 */
		.flags = CLK_IS_CRITICAL,
	},
};

static struct clk_fixed_factor g12a_fclk_div3_div = {
	.mult = 1,
	.div = 3,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div3_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) { &g12a_fixed_pll.hw },
		.num_parents = 1,
	},
};

static struct clk_regmap g12a_fclk_div3 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_FIX_PLL_CNTL1,
		.bit_idx = 20,
	},
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div3",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_fclk_div3_div.hw
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

/* Datasheet names this field as "premux0" */
static struct clk_regmap g12a_cpu_clk_premux0 = {
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
			{ .hw = &g12a_fclk_div2.hw },
			{ .hw = &g12a_fclk_div3.hw },
		},
		.num_parents = 3,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/* Datasheet names this field as "premux1" */
static struct clk_regmap g12a_cpu_clk_premux1 = {
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
			{ .hw = &g12a_fclk_div2.hw },
			{ .hw = &g12a_fclk_div3.hw },
		},
		.num_parents = 3,
		/* This sub-tree is used a parking clock */
		.flags = CLK_SET_RATE_NO_REPARENT
	},
};

/* Datasheet names this field as "mux0_divn_tcnt" */
static struct clk_regmap g12a_cpu_clk_mux0_div = {
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
			&g12a_cpu_clk_premux0.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/* Datasheet names this field as "postmux0" */
static struct clk_regmap g12a_cpu_clk_postmux0 = {
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
			&g12a_cpu_clk_premux0.hw,
			&g12a_cpu_clk_mux0_div.hw,
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/* Datasheet names this field as "Mux1_divn_tcnt" */
static struct clk_regmap g12a_cpu_clk_mux1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_SYS_CPU_CLK_CNTL0,
		.shift = 20,
		.width = 6,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cpu_clk_dyn1_div",
#ifdef CONFIG_AMLOGIC_MODIFY
		.ops = &clk_regmap_divider_ops,
#else
		.ops = &clk_regmap_divider_ro_ops,
#endif
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_cpu_clk_premux1.hw
		},
		.num_parents = 1,
	},
};

/* Datasheet names this field as "postmux1" */
static struct clk_regmap g12a_cpu_clk_postmux1 = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_SYS_CPU_CLK_CNTL0,
		.mask = 0x1,
		.shift = 18,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cpu_clk_dyn1",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_cpu_clk_premux1.hw,
			&g12a_cpu_clk_mux1_div.hw,
		},
		.num_parents = 2,
		/* This sub-tree is used a parking clock */
		.flags = CLK_SET_RATE_NO_REPARENT,
	},
};

/* Datasheet names this field as "Final_dyn_mux_sel" */
static struct clk_regmap g12a_cpu_clk_dyn = {
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
			&g12a_cpu_clk_postmux0.hw,
			&g12a_cpu_clk_postmux1.hw,
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/* Datasheet names this field as "Final_mux_sel" */
static struct clk_regmap g12a_cpu_clk = {
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
			&g12a_cpu_clk_dyn.hw,
			&g12a_sys_pll.hw,
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/* Datasheet names this field as "Final_mux_sel" */
static struct clk_regmap g12b_cpu_clk = {
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
			&g12a_cpu_clk_dyn.hw,
			&g12b_sys1_pll.hw
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/* Datasheet names this field as "premux0" */
static struct clk_regmap g12b_cpub_clk_premux0 = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_SYS_CPUB_CLK_CNTL,
		.mask = 0x3,
		.shift = 0,
		.flags = CLK_MUX_ROUND_CLOSEST,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cpub_clk_dyn0_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = (const struct clk_parent_data []) {
			{ .fw_name = "xtal", },
			{ .hw = &g12a_fclk_div2.hw },
			{ .hw = &g12a_fclk_div3.hw },
		},
		.num_parents = 3,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/* Datasheet names this field as "mux0_divn_tcnt" */
static struct clk_regmap g12b_cpub_clk_mux0_div = {
	.data = &(struct meson_clk_cpu_dyndiv_data){
		.div = {
			.reg_off = HHI_SYS_CPUB_CLK_CNTL,
			.shift = 4,
			.width = 6,
		},
		.dyn = {
			.reg_off = HHI_SYS_CPUB_CLK_CNTL,
			.shift = 26,
			.width = 1,
		},
	},
	.hw.init = &(struct clk_init_data){
		.name = "cpub_clk_dyn0_div",
		.ops = &meson_clk_cpu_dyndiv_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12b_cpub_clk_premux0.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/* Datasheet names this field as "postmux0" */
static struct clk_regmap g12b_cpub_clk_postmux0 = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_SYS_CPUB_CLK_CNTL,
		.mask = 0x1,
		.shift = 2,
		.flags = CLK_MUX_ROUND_CLOSEST,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cpub_clk_dyn0",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12b_cpub_clk_premux0.hw,
			&g12b_cpub_clk_mux0_div.hw
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/* Datasheet names this field as "premux1" */
static struct clk_regmap g12b_cpub_clk_premux1 = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_SYS_CPUB_CLK_CNTL,
		.mask = 0x3,
		.shift = 16,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cpub_clk_dyn1_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = (const struct clk_parent_data []) {
			{ .fw_name = "xtal", },
			{ .hw = &g12a_fclk_div2.hw },
			{ .hw = &g12a_fclk_div3.hw },
		},
		.num_parents = 3,
		/* This sub-tree is used a parking clock */
		.flags = CLK_SET_RATE_NO_REPARENT,
	},
};

/* Datasheet names this field as "Mux1_divn_tcnt" */
static struct clk_regmap g12b_cpub_clk_mux1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_SYS_CPUB_CLK_CNTL,
		.shift = 20,
		.width = 6,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cpub_clk_dyn1_div",
#ifdef CONFIG_AMLOGIC_MODIFY
		.ops = &clk_regmap_divider_ops,
#else
		.ops = &clk_regmap_divider_ro_ops,
#endif
		.parent_hws = (const struct clk_hw *[]) {
			&g12b_cpub_clk_premux1.hw
		},
		.num_parents = 1,
	},
};

/* Datasheet names this field as "postmux1" */
static struct clk_regmap g12b_cpub_clk_postmux1 = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_SYS_CPUB_CLK_CNTL,
		.mask = 0x1,
		.shift = 18,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cpub_clk_dyn1",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12b_cpub_clk_premux1.hw,
			&g12b_cpub_clk_mux1_div.hw
		},
		.num_parents = 2,
		/* This sub-tree is used a parking clock */
		.flags = CLK_SET_RATE_NO_REPARENT,
	},
};

/* Datasheet names this field as "Final_dyn_mux_sel" */
static struct clk_regmap g12b_cpub_clk_dyn = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_SYS_CPUB_CLK_CNTL,
		.mask = 0x1,
		.shift = 10,
		.flags = CLK_MUX_ROUND_CLOSEST,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cpub_clk_dyn",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12b_cpub_clk_postmux0.hw,
			&g12b_cpub_clk_postmux1.hw
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/* Datasheet names this field as "Final_mux_sel" */
static struct clk_regmap g12b_cpub_clk = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_SYS_CPUB_CLK_CNTL,
		.mask = 0x1,
		.shift = 11,
		.flags = CLK_MUX_ROUND_CLOSEST,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cpub_clk",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12b_cpub_clk_dyn.hw,
			&g12a_sys_pll.hw
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap sm1_gp1_pll;

/* Datasheet names this field as "premux0" */
static struct clk_regmap sm1_dsu_clk_premux0 = {
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
			{ .hw = &g12a_fclk_div2.hw },
			{ .hw = &g12a_fclk_div3.hw },
			{ .hw = &sm1_gp1_pll.hw },
		},
		.num_parents = 4,
		.flags = CLK_SET_RATE_PARENT | CLK_SET_RATE_NO_REPARENT,
	},
};

/* Datasheet names this field as "premux1" */
static struct clk_regmap sm1_dsu_clk_premux1 = {
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
			{ .hw = &g12a_fclk_div2.hw },
			{ .hw = &g12a_fclk_div3.hw },
			{ .hw = &sm1_gp1_pll.hw },
		},
		.num_parents = 4,
		.flags = CLK_SET_RATE_PARENT | CLK_SET_RATE_NO_REPARENT,
	},
};

/* Datasheet names this field as "Mux0_divn_tcnt" */
static struct clk_regmap sm1_dsu_clk_mux0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_SYS_CPU_CLK_CNTL5,
		.shift = 4,
		.width = 6,
	},
	.hw.init = &(struct clk_init_data){
		.name = "dsu_clk_dyn0_div",
#ifdef CONFIG_AMLOGIC_MODIFY
		.ops = &clk_regmap_divider_ops,
#else
		.ops = &clk_regmap_divider_ro_ops,
#endif
		.parent_hws = (const struct clk_hw *[]) {
			&sm1_dsu_clk_premux0.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/* Datasheet names this field as "postmux0" */
static struct clk_regmap sm1_dsu_clk_postmux0 = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_SYS_CPU_CLK_CNTL5,
		.mask = 0x1,
		.shift = 2,
	},
	.hw.init = &(struct clk_init_data){
		.name = "dsu_clk_dyn0",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sm1_dsu_clk_premux0.hw,
			&sm1_dsu_clk_mux0_div.hw,
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/* Datasheet names this field as "Mux1_divn_tcnt" */
static struct clk_regmap sm1_dsu_clk_mux1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_SYS_CPU_CLK_CNTL5,
		.shift = 20,
		.width = 6,
	},
	.hw.init = &(struct clk_init_data){
		.name = "dsu_clk_dyn1_div",
#ifdef CONFIG_AMLOGIC_MODIFY
		.ops = &clk_regmap_divider_ops,
#else
		.ops = &clk_regmap_divider_ro_ops,
#endif
		.parent_hws = (const struct clk_hw *[]) {
			&sm1_dsu_clk_premux1.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/* Datasheet names this field as "postmux1" */
static struct clk_regmap sm1_dsu_clk_postmux1 = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_SYS_CPU_CLK_CNTL5,
		.mask = 0x1,
		.shift = 18,
	},
	.hw.init = &(struct clk_init_data){
		.name = "dsu_clk_dyn1",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sm1_dsu_clk_premux1.hw,
			&sm1_dsu_clk_mux1_div.hw,
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/* Datasheet names this field as "Final_dyn_mux_sel" */
static struct clk_regmap sm1_dsu_clk_dyn = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_SYS_CPU_CLK_CNTL5,
		.mask = 0x1,
		.shift = 10,
	},
	.hw.init = &(struct clk_init_data){
		.name = "dsu_clk_dyn",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sm1_dsu_clk_postmux0.hw,
			&sm1_dsu_clk_postmux1.hw,
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT | CLK_SET_RATE_NO_REPARENT,
	},
};

/* Datasheet names this field as "Final_mux_sel" */
static struct clk_regmap sm1_dsu_final_clk = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_SYS_CPU_CLK_CNTL5,
		.mask = 0x1,
		.shift = 11,
	},
	.hw.init = &(struct clk_init_data){
		.name = "dsu_clk_final",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sm1_dsu_clk_dyn.hw,
			&g12a_sys_pll.hw,
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT | CLK_SET_RATE_NO_REPARENT,
	},
};

/* Datasheet names this field as "Cpu_clk_sync_mux_sel" bit 0 */
static struct clk_regmap sm1_cpu1_clk = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_SYS_CPU_CLK_CNTL6,
		.mask = 0x1,
		.shift = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cpu1_clk",
		.ops = &clk_regmap_mux_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_cpu_clk.hw,
			/* This CPU also have a dedicated clock tree */
		},
		.num_parents = 1,
	},
};

/* Datasheet names this field as "Cpu_clk_sync_mux_sel" bit 1 */
static struct clk_regmap sm1_cpu2_clk = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_SYS_CPU_CLK_CNTL6,
		.mask = 0x1,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cpu2_clk",
		.ops = &clk_regmap_mux_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_cpu_clk.hw,
			/* This CPU also have a dedicated clock tree */
		},
		.num_parents = 1,
	},
};

/* Datasheet names this field as "Cpu_clk_sync_mux_sel" bit 2 */
static struct clk_regmap sm1_cpu3_clk = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_SYS_CPU_CLK_CNTL6,
		.mask = 0x1,
		.shift = 26,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cpu3_clk",
		.ops = &clk_regmap_mux_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_cpu_clk.hw,
			/* This CPU also have a dedicated clock tree */
		},
		.num_parents = 1,
	},
};

/* Datasheet names this field as "Cpu_clk_sync_mux_sel" bit 4 */
static struct clk_regmap sm1_dsu_clk = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_SYS_CPU_CLK_CNTL6,
		.mask = 0x1,
		.shift = 27,
	},
	.hw.init = &(struct clk_init_data){
		.name = "dsu_clk",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_cpu_clk.hw,
			&sm1_dsu_final_clk.hw,
		},
		.num_parents = 2,
	},
};

struct g12a_cpu_clk_postmux_nb_data {
	struct notifier_block nb;
	struct clk_hw *fclk_div2;
	struct clk_hw *cpu_clk_dyn;
	struct clk_hw *cpu_clk_postmux0;
	struct clk_hw *cpu_clk_postmux1;
	struct clk_hw *cpu_clk_premux1;
};

static int g12a_cpu_clk_postmux_notifier_cb(struct notifier_block *nb,
					    unsigned long event, void *data)
{
	struct g12a_cpu_clk_postmux_nb_data *nb_data =
		container_of(nb, struct g12a_cpu_clk_postmux_nb_data, nb);

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

static struct g12a_cpu_clk_postmux_nb_data g12a_cpu_clk_postmux0_nb_data = {
	.cpu_clk_dyn = &g12a_cpu_clk_dyn.hw,
	.cpu_clk_postmux0 = &g12a_cpu_clk_postmux0.hw,
	.cpu_clk_postmux1 = &g12a_cpu_clk_postmux1.hw,
	.cpu_clk_premux1 = &g12a_cpu_clk_premux1.hw,
	.nb.notifier_call = g12a_cpu_clk_postmux_notifier_cb,
};

static struct g12a_cpu_clk_postmux_nb_data g12b_cpub_clk_postmux0_nb_data = {
	.cpu_clk_dyn = &g12b_cpub_clk_dyn.hw,
	.cpu_clk_postmux0 = &g12b_cpub_clk_postmux0.hw,
	.cpu_clk_postmux1 = &g12b_cpub_clk_postmux1.hw,
	.cpu_clk_premux1 = &g12b_cpub_clk_premux1.hw,
	.nb.notifier_call = g12a_cpu_clk_postmux_notifier_cb,
};

struct g12a_sys_pll_nb_data {
	struct notifier_block nb;
	struct clk_hw *sys_pll;
	struct clk_hw *cpu_clk;
	struct clk_hw *cpu_clk_dyn;
};

static int g12a_sys_pll_notifier_cb(struct notifier_block *nb,
				    unsigned long event, void *data)
{
	struct g12a_sys_pll_nb_data *nb_data =
		container_of(nb, struct g12a_sys_pll_nb_data, nb);

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

static struct g12a_sys_pll_nb_data g12a_sys_pll_nb_data = {
	.sys_pll = &g12a_sys_pll.hw,
	.cpu_clk = &g12a_cpu_clk.hw,
	.cpu_clk_dyn = &g12a_cpu_clk_dyn.hw,
	.nb.notifier_call = g12a_sys_pll_notifier_cb,
};

/* G12B first CPU cluster uses sys1_pll */
static struct g12a_sys_pll_nb_data g12b_cpu_clk_sys1_pll_nb_data = {
	.sys_pll = &g12b_sys1_pll.hw,
	.cpu_clk = &g12b_cpu_clk.hw,
	.cpu_clk_dyn = &g12a_cpu_clk_dyn.hw,
	.nb.notifier_call = g12a_sys_pll_notifier_cb,
};

/* G12B second CPU cluster uses sys_pll */
static struct g12a_sys_pll_nb_data g12b_cpub_clk_sys_pll_nb_data = {
	.sys_pll = &g12a_sys_pll.hw,
	.cpu_clk = &g12b_cpub_clk.hw,
	.cpu_clk_dyn = &g12b_cpub_clk_dyn.hw,
	.nb.notifier_call = g12a_sys_pll_notifier_cb,
};

/* sm1 dsu notify */
struct sm1_dsu_clk_postmux_nb_data {
	struct notifier_block nb;
	struct clk_hw *dsu_clk_dyn;
	struct clk_hw *dsu_clk_postmux0;
	struct clk_hw *dsu_clk_postmux1;
};

static int sm1_dsu_clk_postmux_notifier_cb(struct notifier_block *nb,
					   unsigned long event, void *data)
{
	struct sm1_dsu_clk_postmux_nb_data *nb_data =
		container_of(nb, struct sm1_dsu_clk_postmux_nb_data, nb);
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

static struct sm1_dsu_clk_postmux_nb_data sm1_dsu_clk_postmux0_nb_data = {
	.dsu_clk_dyn = &sm1_dsu_clk_dyn.hw,
	.dsu_clk_postmux0 = &sm1_dsu_clk_postmux0.hw,
	.dsu_clk_postmux1 = &sm1_dsu_clk_postmux1.hw,
	.nb.notifier_call = sm1_dsu_clk_postmux_notifier_cb,
};

static struct clk_regmap g12a_cpu_clk_div16_en = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_SYS_CPU_CLK_CNTL1,
		.bit_idx = 1,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "cpu_clk_div16_en",
		.ops = &clk_regmap_gate_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_cpu_clk.hw
		},
		.num_parents = 1,
		/*
		 * This clock is used to debug the cpu_clk range
		 * Linux should not change it at runtime
		 */
	},
};

static struct clk_regmap g12b_cpub_clk_div16_en = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_SYS_CPUB_CLK_CNTL1,
		.bit_idx = 1,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "cpub_clk_div16_en",
		.ops = &clk_regmap_gate_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12b_cpub_clk.hw
		},
		.num_parents = 1,
		/*
		 * This clock is used to debug the cpu_clk range
		 * Linux should not change it at runtime
		 */
	},
};

static struct clk_fixed_factor g12a_cpu_clk_div16 = {
	.mult = 1,
	.div = 16,
	.hw.init = &(struct clk_init_data){
		.name = "cpu_clk_div16",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_cpu_clk_div16_en.hw
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor g12b_cpub_clk_div16 = {
	.mult = 1,
	.div = 16,
	.hw.init = &(struct clk_init_data){
		.name = "cpub_clk_div16",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12b_cpub_clk_div16_en.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap g12a_cpu_clk_apb_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_SYS_CPU_CLK_CNTL1,
		.shift = 3,
		.width = 3,
		.flags = CLK_DIVIDER_POWER_OF_TWO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cpu_clk_apb_div",
		.ops = &clk_regmap_divider_ro_ops,
		.parent_hws = (const struct clk_hw *[]) { &g12a_cpu_clk.hw },
		.num_parents = 1,
	},
};

static struct clk_regmap g12a_cpu_clk_apb = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_SYS_CPU_CLK_CNTL1,
		.bit_idx = 1,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "cpu_clk_apb",
		.ops = &clk_regmap_gate_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_cpu_clk_apb_div.hw
		},
		.num_parents = 1,
		/*
		 * This clock is set by the ROM monitor code,
		 * Linux should not change it at runtime
		 */
	},
};

static struct clk_regmap g12a_cpu_clk_atb_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_SYS_CPU_CLK_CNTL1,
		.shift = 6,
		.width = 3,
		.flags = CLK_DIVIDER_POWER_OF_TWO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cpu_clk_atb_div",
		.ops = &clk_regmap_divider_ro_ops,
		.parent_hws = (const struct clk_hw *[]) { &g12a_cpu_clk.hw },
		.num_parents = 1,
	},
};

static struct clk_regmap g12a_cpu_clk_atb = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_SYS_CPU_CLK_CNTL1,
		.bit_idx = 17,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "cpu_clk_atb",
		.ops = &clk_regmap_gate_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_cpu_clk_atb_div.hw
		},
		.num_parents = 1,
		/*
		 * This clock is set by the ROM monitor code,
		 * Linux should not change it at runtime
		 */
	},
};

static struct clk_regmap g12a_cpu_clk_axi_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_SYS_CPU_CLK_CNTL1,
		.shift = 9,
		.width = 3,
		.flags = CLK_DIVIDER_POWER_OF_TWO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cpu_clk_axi_div",
		.ops = &clk_regmap_divider_ro_ops,
		.parent_hws = (const struct clk_hw *[]) { &g12a_cpu_clk.hw },
		.num_parents = 1,
	},
};

static struct clk_regmap g12a_cpu_clk_axi = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_SYS_CPU_CLK_CNTL1,
		.bit_idx = 18,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "cpu_clk_axi",
		.ops = &clk_regmap_gate_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_cpu_clk_axi_div.hw
		},
		.num_parents = 1,
		/*
		 * This clock is set by the ROM monitor code,
		 * Linux should not change it at runtime
		 */
	},
};

static struct clk_regmap g12a_cpu_clk_trace_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_SYS_CPU_CLK_CNTL1,
		.shift = 20,
		.width = 3,
		.flags = CLK_DIVIDER_POWER_OF_TWO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cpu_clk_trace_div",
		.ops = &clk_regmap_divider_ro_ops,
		.parent_data = &(const struct clk_parent_data) {
			/*
			 * Note:
			 * G12A and G12B have different cpu_clks (with
			 * different struct clk_hw). We fallback to the global
			 * naming string mechanism so cpu_clk_trace_div picks
			 * up the appropriate one.
			 */
			.name = "cpu_clk",
			.index = -1,
		},
		.num_parents = 1,
	},
};

static struct clk_regmap g12a_cpu_clk_trace = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_SYS_CPU_CLK_CNTL1,
		.bit_idx = 23,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "cpu_clk_trace",
		.ops = &clk_regmap_gate_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_cpu_clk_trace_div.hw
		},
		.num_parents = 1,
		/*
		 * This clock is set by the ROM monitor code,
		 * Linux should not change it at runtime
		 */
	},
};

static struct clk_fixed_factor g12b_cpub_clk_div2 = {
	.mult = 1,
	.div = 2,
	.hw.init = &(struct clk_init_data){
		.name = "cpub_clk_div2",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12b_cpub_clk.hw
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor g12b_cpub_clk_div3 = {
	.mult = 1,
	.div = 3,
	.hw.init = &(struct clk_init_data){
		.name = "cpub_clk_div3",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12b_cpub_clk.hw
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor g12b_cpub_clk_div4 = {
	.mult = 1,
	.div = 4,
	.hw.init = &(struct clk_init_data){
		.name = "cpub_clk_div4",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12b_cpub_clk.hw
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor g12b_cpub_clk_div5 = {
	.mult = 1,
	.div = 5,
	.hw.init = &(struct clk_init_data){
		.name = "cpub_clk_div5",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12b_cpub_clk.hw
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor g12b_cpub_clk_div6 = {
	.mult = 1,
	.div = 6,
	.hw.init = &(struct clk_init_data){
		.name = "cpub_clk_div6",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12b_cpub_clk.hw
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor g12b_cpub_clk_div7 = {
	.mult = 1,
	.div = 7,
	.hw.init = &(struct clk_init_data){
		.name = "cpub_clk_div7",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12b_cpub_clk.hw
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor g12b_cpub_clk_div8 = {
	.mult = 1,
	.div = 8,
	.hw.init = &(struct clk_init_data){
		.name = "cpub_clk_div8",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12b_cpub_clk.hw
		},
		.num_parents = 1,
	},
};

static u32 mux_table_cpub[] = { 1, 2, 3, 4, 5, 6, 7 };
static struct clk_regmap g12b_cpub_clk_apb_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_SYS_CPUB_CLK_CNTL1,
		.mask = 7,
		.shift = 3,
		.table = mux_table_cpub,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cpub_clk_apb_sel",
		.ops = &clk_regmap_mux_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12b_cpub_clk_div2.hw,
			&g12b_cpub_clk_div3.hw,
			&g12b_cpub_clk_div4.hw,
			&g12b_cpub_clk_div5.hw,
			&g12b_cpub_clk_div6.hw,
			&g12b_cpub_clk_div7.hw,
			&g12b_cpub_clk_div8.hw
		},
		.num_parents = 7,
	},
};

static struct clk_regmap g12b_cpub_clk_apb = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_SYS_CPUB_CLK_CNTL1,
		.bit_idx = 16,
		.flags = CLK_GATE_SET_TO_DISABLE,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "cpub_clk_apb",
		.ops = &clk_regmap_gate_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12b_cpub_clk_apb_sel.hw
		},
		.num_parents = 1,
		/*
		 * This clock is set by the ROM monitor code,
		 * Linux should not change it at runtime
		 */
	},
};

static struct clk_regmap g12b_cpub_clk_atb_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_SYS_CPUB_CLK_CNTL1,
		.mask = 7,
		.shift = 6,
		.table = mux_table_cpub,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cpub_clk_atb_sel",
		.ops = &clk_regmap_mux_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12b_cpub_clk_div2.hw,
			&g12b_cpub_clk_div3.hw,
			&g12b_cpub_clk_div4.hw,
			&g12b_cpub_clk_div5.hw,
			&g12b_cpub_clk_div6.hw,
			&g12b_cpub_clk_div7.hw,
			&g12b_cpub_clk_div8.hw
		},
		.num_parents = 7,
	},
};

static struct clk_regmap g12b_cpub_clk_atb = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_SYS_CPUB_CLK_CNTL1,
		.bit_idx = 17,
		.flags = CLK_GATE_SET_TO_DISABLE,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "cpub_clk_atb",
		.ops = &clk_regmap_gate_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12b_cpub_clk_atb_sel.hw
		},
		.num_parents = 1,
		/*
		 * This clock is set by the ROM monitor code,
		 * Linux should not change it at runtime
		 */
	},
};

static struct clk_regmap g12b_cpub_clk_axi_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_SYS_CPUB_CLK_CNTL1,
		.mask = 7,
		.shift = 9,
		.table = mux_table_cpub,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cpub_clk_axi_sel",
		.ops = &clk_regmap_mux_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12b_cpub_clk_div2.hw,
			&g12b_cpub_clk_div3.hw,
			&g12b_cpub_clk_div4.hw,
			&g12b_cpub_clk_div5.hw,
			&g12b_cpub_clk_div6.hw,
			&g12b_cpub_clk_div7.hw,
			&g12b_cpub_clk_div8.hw
		},
		.num_parents = 7,
	},
};

static struct clk_regmap g12b_cpub_clk_axi = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_SYS_CPUB_CLK_CNTL1,
		.bit_idx = 18,
		.flags = CLK_GATE_SET_TO_DISABLE,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "cpub_clk_axi",
		.ops = &clk_regmap_gate_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12b_cpub_clk_axi_sel.hw
		},
		.num_parents = 1,
		/*
		 * This clock is set by the ROM monitor code,
		 * Linux should not change it at runtime
		 */
	},
};

static struct clk_regmap g12b_cpub_clk_trace_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_SYS_CPUB_CLK_CNTL1,
		.mask = 7,
		.shift = 20,
		.table = mux_table_cpub,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cpub_clk_trace_sel",
		.ops = &clk_regmap_mux_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12b_cpub_clk_div2.hw,
			&g12b_cpub_clk_div3.hw,
			&g12b_cpub_clk_div4.hw,
			&g12b_cpub_clk_div5.hw,
			&g12b_cpub_clk_div6.hw,
			&g12b_cpub_clk_div7.hw,
			&g12b_cpub_clk_div8.hw
		},
		.num_parents = 7,
	},
};

static struct clk_regmap g12b_cpub_clk_trace = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_SYS_CPUB_CLK_CNTL1,
		.bit_idx = 23,
		.flags = CLK_GATE_SET_TO_DISABLE,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "cpub_clk_trace",
		.ops = &clk_regmap_gate_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12b_cpub_clk_trace_sel.hw
		},
		.num_parents = 1,
		/*
		 * This clock is set by the ROM monitor code,
		 * Linux should not change it at runtime
		 */
	},
};

#ifdef CONFIG_ARM
static const struct pll_params_table g12a_gp0_pll_table[] = {
	PLL_PARAMS(141, 1, 2), /* DCO = 3384M OD = 2 PLL = 846M */
	PLL_PARAMS(132, 1, 2), /* DCO = 3168M OD = 2 PLL = 792M */
	PLL_PARAMS(248, 1, 3), /* DCO = 5952M OD = 3 PLL = 744M */
	{ /* sentinel */  },
};
#else
static const struct pll_params_table g12a_gp0_pll_table[] = {
	PLL_PARAMS(141, 1), /* DCO = 3384M OD = 2 PLL = 846M */
	PLL_PARAMS(132, 1), /* DCO = 3168M OD = 2 PLL = 792M */
	PLL_PARAMS(248, 1), /* DCO = 5952M OD = 3 PLL = 744M */
	{ /* sentinel */  },
};
#endif

/*
 * Internal gp0 pll emulation configuration parameters
 */
static const struct reg_sequence g12a_gp0_init_regs[] = {
	{ .reg = HHI_GP0_PLL_CNTL1,	.def = 0x00000000 },
	{ .reg = HHI_GP0_PLL_CNTL2,	.def = 0x00000000 },
	{ .reg = HHI_GP0_PLL_CNTL3,	.def = 0x48681c00 },
	{ .reg = HHI_GP0_PLL_CNTL4,	.def = 0x33771290 },
	{ .reg = HHI_GP0_PLL_CNTL5,	.def = 0x39272000 },
	{ .reg = HHI_GP0_PLL_CNTL6,	.def = 0x56540000 },
};

static struct clk_regmap g12a_gp0_pll_dco = {
	.data = &(struct meson_clk_pll_data){
		.en = {
			.reg_off = HHI_GP0_PLL_CNTL0,
			.shift   = 28,
			.width   = 1,
		},
		.m = {
			.reg_off = HHI_GP0_PLL_CNTL0,
			.shift   = 0,
			.width   = 8,
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
			.width   = 17,
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
		.table = g12a_gp0_pll_table,
		.init_regs = g12a_gp0_init_regs,
		.init_count = ARRAY_SIZE(g12a_gp0_init_regs),
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
static struct clk_regmap g12a_gp0_pll = {
	.hw.init = &(struct clk_init_data){
		.name = "gp0_pll",
		.ops = &meson_pll_clk_no_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_gp0_pll_dco.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};
#else
static struct clk_regmap g12a_gp0_pll = {
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
			&g12a_gp0_pll_dco.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};
#endif

#ifdef CONFIG_ARM
static const struct pll_params_table sm1_gp1_pll_table[] = {
	PLL_PARAMS(200, 1, 2), /*DCO=4800M OD=1200M*/
	PLL_PARAMS(125, 1, 1), /*DCO=3000M OD=1500M*/
	{ /* sentinel */  },
};
#else
static const struct pll_params_table sm1_gp1_pll_table[] = {
	PLL_PARAMS(200, 1), /*DCO=4800M OD=1200M*/
	PLL_PARAMS(125, 1), /*DCO=3000M OD=1500M*/
	{ /* sentinel */  },
};
#endif

/*
 * Internal gp1 pll emulation configuration parameters
 */
static const struct reg_sequence sm1_gp1_init_regs[] = {
	{ .reg = HHI_GP1_PLL_CNTL1,	.def = 0x00000000 },
	{ .reg = HHI_GP1_PLL_CNTL2,	.def = 0x00000000 },
	{ .reg = HHI_GP1_PLL_CNTL3,	.def = 0x48681c00 },
	{ .reg = HHI_GP1_PLL_CNTL4,	.def = 0x33771290 },
	{ .reg = HHI_GP1_PLL_CNTL5,	.def = 0x39272000 },
	{ .reg = HHI_GP1_PLL_CNTL6,	.def = 0x56540000 }
};

static struct clk_regmap sm1_gp1_pll_dco = {
	.data = &(struct meson_clk_pll_data){
		.en = {
			.reg_off = HHI_GP1_PLL_CNTL0,
			.shift   = 28,
			.width   = 1,
		},
		.m = {
			.reg_off = HHI_GP1_PLL_CNTL0,
			.shift   = 0,
			.width   = 8,
		},
		.n = {
			.reg_off = HHI_GP1_PLL_CNTL0,
			.shift   = 10,
			.width   = 5,
		},
#ifdef CONFIG_ARM
		/* for 32bit */
		.od = {
			.reg_off = HHI_GP1_PLL_CNTL0,
			.shift	 = 16,
			.width	 = 3,
		},
#endif
		.frac = {
			.reg_off = HHI_GP1_PLL_CNTL1,
			.shift   = 0,
			.width   = 19,
		},
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
		.table = sm1_gp1_pll_table,
		/*.init_regs = sm1_gp1_init_regs,
		 *.init_count = ARRAY_SIZE(sm1_gp1_init_regs),
		 */
	},
	.hw.init = &(struct clk_init_data){
		.name = "gp1_pll_dco",
		.ops = &meson_clk_pll_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "xtal",
		},
		.num_parents = 1,
		/* This clock feeds the DSU, avoid disabling it */
		.flags = CLK_IS_CRITICAL | CLK_GET_RATE_NOCACHE,
	},
};

#ifdef CONFIG_ARM
static struct clk_regmap sm1_gp1_pll = {
	.hw.init = &(struct clk_init_data){
		.name = "gp1_pll",
		.ops = &meson_pll_clk_no_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sm1_gp1_pll_dco.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};
#else
static struct clk_regmap sm1_gp1_pll = {
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
			&sm1_gp1_pll_dco.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};
#endif

static const struct pll_mult_range g12a_hifi_pll_mult_range = {
	.min = 125,
	.max = 250,
};

/*
 * Internal hifi pll emulation configuration parameters
 */
static const struct reg_sequence g12a_hifi_init_regs[] = {
	{ .reg = HHI_HIFI_PLL_CNTL0,	.def = 0x08010496 },
	{ .reg = HHI_HIFI_PLL_CNTL0,	.def = 0x38010496 },
	{ .reg = HHI_HIFI_PLL_CNTL1,	.def = 0x00010e56 },
	{ .reg = HHI_HIFI_PLL_CNTL2,	.def = 0x00000000 },
	{ .reg = HHI_HIFI_PLL_CNTL3,	.def = 0x6a285c00 },
	{ .reg = HHI_HIFI_PLL_CNTL4,	.def = 0x65771290 },
	{ .reg = HHI_HIFI_PLL_CNTL5,	.def = 0x39272000 },
	{ .reg = HHI_HIFI_PLL_CNTL6,	.def = 0x56540000 },
	{ .reg = HHI_HIFI_PLL_CNTL0,	.def = 0x18010496 },
};

static struct clk_regmap g12a_hifi_pll_dco = {
	.data = &(struct meson_clk_pll_data){
		.en = {
			.reg_off = HHI_HIFI_PLL_CNTL0,
			.shift   = 28,
			.width   = 1,
		},
		.m = {
			.reg_off = HHI_HIFI_PLL_CNTL0,
			.shift   = 0,
			.width   = 8,
		},
		.n = {
			.reg_off = HHI_HIFI_PLL_CNTL0,
			.shift   = 10,
			.width   = 1,  /* keep always n == 1 */
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
		.range = &g12a_hifi_pll_mult_range,
		.init_regs = g12a_hifi_init_regs,
		.init_count = ARRAY_SIZE(g12a_hifi_init_regs),
		.flags = CLK_MESON_PLL_ROUND_CLOSEST,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hifi_pll_dco",
		.ops = &meson_clk_pll_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "xtal",
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap g12a_hifi_pll = {
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
			&g12a_hifi_pll_dco.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT |
			CLK_GET_RATE_NOCACHE,
	},
};

/*
 * The Meson G12A PCIE PLL is fined tuned to deliver a very precise
 * 100MHz reference clock for the PCIe Analog PHY, and thus requires
 * a strict register sequence to enable the PLL.
 */
static const struct reg_sequence g12a_pcie_pll_init_regs[] = {
	{ .reg = HHI_PCIE_PLL_CNTL0,	.def = 0x20090496 },
	{ .reg = HHI_PCIE_PLL_CNTL0,	.def = 0x30090496 },
	{ .reg = HHI_PCIE_PLL_CNTL1,	.def = 0x00000000 },
	{ .reg = HHI_PCIE_PLL_CNTL2,	.def = 0x00001100 },
	{ .reg = HHI_PCIE_PLL_CNTL3,	.def = 0x10058e00 },
	{ .reg = HHI_PCIE_PLL_CNTL4,	.def = 0x000100c0 },
	{ .reg = HHI_PCIE_PLL_CNTL5,	.def = 0x68000040 },
	{ .reg = HHI_PCIE_PLL_CNTL5,	.def = 0x68000060, .delay_us = 20 },
	{ .reg = HHI_PCIE_PLL_CNTL4,	.def = 0x008100c0, .delay_us = 10 },
	{ .reg = HHI_PCIE_PLL_CNTL0,	.def = 0x34090496 },
	{ .reg = HHI_PCIE_PLL_CNTL0,	.def = 0x14090496, .delay_us = 10 },
	{ .reg = HHI_PCIE_PLL_CNTL2,	.def = 0x00001000 },
};

#ifdef CONFIG_ARM64
/* Keep a single entry table for recalc/round_rate() ops */
static const struct pll_params_table g12a_pcie_pll_table[] = {
	PLL_PARAMS(150, 1),
	{0, 0},
};
#else
static const struct pll_params_table g12a_pcie_pll_table[] = {
	PLL_PARAMS(150, 1, 0),
	{0, 0, 0},
};
#endif

static struct clk_regmap g12a_pcie_pll_dco = {
	.data = &(struct meson_clk_pll_data){
		.en = {
			.reg_off = HHI_PCIE_PLL_CNTL0,
			.shift   = 28,
			.width   = 1,
		},
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
		.frac = {
			.reg_off = HHI_PCIE_PLL_CNTL1,
			.shift   = 0,
			.width   = 12,
		},
		.l = {
			.reg_off = HHI_PCIE_PLL_CNTL0,
			.shift   = 31,
			.width   = 1,
		},
		.rst = {
			.reg_off = HHI_PCIE_PLL_CNTL0,
			.shift   = 29,
			.width   = 1,
		},
		.table = g12a_pcie_pll_table,
		.init_regs = g12a_pcie_pll_init_regs,
		.init_count = ARRAY_SIZE(g12a_pcie_pll_init_regs),
	},
	.hw.init = &(struct clk_init_data){
		.name = "pcie_pll_dco",
		.ops = &meson_clk_pcie_pll_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "xtal",
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor g12a_pcie_pll_dco_div2 = {
	.mult = 1,
	.div = 2,
	.hw.init = &(struct clk_init_data){
		.name = "pcie_pll_dco_div2",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_pcie_pll_dco.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap g12a_pcie_pll_od = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_PCIE_PLL_CNTL0,
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
			&g12a_pcie_pll_dco_div2.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_fixed_factor g12a_pcie_pll = {
	.mult = 1,
	.div = 2,
	.hw.init = &(struct clk_init_data){
		.name = "pcie_pll_pll",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_pcie_pll_od.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap g12a_pcie_bgp = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_PCIE_PLL_CNTL5,
		.bit_idx = 27,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pcie_bgp",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &g12a_pcie_pll.hw },
		.num_parents = 1,
		.flags = CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap g12a_pcie_hcsl = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_PCIE_PLL_CNTL5,
		.bit_idx = 3,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pcie_hcsl",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &g12a_pcie_bgp.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap g12a_hdmi_pll_dco = {
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

static struct clk_regmap g12a_hdmi_pll_od = {
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
			&g12a_hdmi_pll_dco.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap g12a_hdmi_pll_od2 = {
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
			&g12a_hdmi_pll_od.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap g12a_hdmi_pll = {
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
			&g12a_hdmi_pll_od2.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_fixed_factor g12a_fclk_div4_div = {
	.mult = 1,
	.div = 4,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div4_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) { &g12a_fixed_pll.hw },
		.num_parents = 1,
	},
};

static struct clk_regmap g12a_fclk_div4 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_FIX_PLL_CNTL1,
		.bit_idx = 21,
	},
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div4",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_fclk_div4_div.hw
		},
		.num_parents = 1,
		/*
		 * This clock feeds on GPU, it should be set
		 * by the platform to operate correctly.
		 */
		.flags = CLK_IS_CRITICAL,
	},
};

static struct clk_fixed_factor g12a_fclk_div5_div = {
	.mult = 1,
	.div = 5,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div5_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) { &g12a_fixed_pll.hw },
		.num_parents = 1,
	},
};

static struct clk_regmap g12a_fclk_div5 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_FIX_PLL_CNTL1,
		.bit_idx = 22,
	},
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div5",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_fclk_div5_div.hw
		},
		.num_parents = 1,
		/*
		 * This clock feeds on GPU, it should be set
		 * by the platform to operate correctly.
		 */
		.flags = CLK_IS_CRITICAL,
	},
};

static struct clk_fixed_factor g12a_fclk_div7_div = {
	.mult = 1,
	.div = 7,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div7_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) { &g12a_fixed_pll.hw },
		.num_parents = 1,
	},
};

static struct clk_regmap g12a_fclk_div7 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_FIX_PLL_CNTL1,
		.bit_idx = 23,
	},
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div7",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_fclk_div7_div.hw
		},
		.num_parents = 1,
		/*
		 * This clock feeds on GPU, it should be set
		 * by the platform to operate correctly.
		 */
		.flags = CLK_IS_CRITICAL,
	},
};

static struct clk_fixed_factor g12a_fclk_div2p5_div = {
	.mult = 2,
	.div = 5,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div2p5_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_fixed_pll.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap g12a_fclk_div2p5 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_FIX_PLL_CNTL1,
		.bit_idx = 25,
	},
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div2p5",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_fclk_div2p5_div.hw
		},
		.num_parents = 1,
		/*
		 * This clock feeds on GPU, it should be set
		 * by the platform to operate correctly.
		 */
		.flags = CLK_IS_CRITICAL,
	},
};

static struct clk_fixed_factor g12a_mpll_50m_div = {
	.mult = 1,
	.div = 80,
	.hw.init = &(struct clk_init_data){
		.name = "mpll_50m_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_fixed_pll_dco.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap g12a_mpll_50m = {
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
			{ .hw = &g12a_mpll_50m_div.hw },
		},
		.num_parents = 2,
	},
};

static struct clk_fixed_factor g12a_mpll_prediv = {
	.mult = 1,
	.div = 2,
	.hw.init = &(struct clk_init_data){
		.name = "mpll_prediv",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_fixed_pll_dco.hw
		},
		.num_parents = 1,
	},
};

static const struct reg_sequence g12a_mpll0_init_regs[] = {
	{ .reg = HHI_MPLL_CNTL2,	.def = 0x40000033 },
};

static struct clk_regmap g12a_mpll0_div = {
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
		.init_regs = g12a_mpll0_init_regs,
		.init_count = ARRAY_SIZE(g12a_mpll0_init_regs),
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpll0_div",
		.ops = &meson_clk_mpll_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_mpll_prediv.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap g12a_mpll0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_MPLL_CNTL1,
		.bit_idx = 31,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpll0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &g12a_mpll0_div.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct reg_sequence g12a_mpll1_init_regs[] = {
	{ .reg = HHI_MPLL_CNTL4,	.def = 0x40000033 },
};

static struct clk_regmap g12a_mpll1_div = {
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
		.init_regs = g12a_mpll1_init_regs,
		.init_count = ARRAY_SIZE(g12a_mpll1_init_regs),
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpll1_div",
		.ops = &meson_clk_mpll_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_mpll_prediv.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap g12a_mpll1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_MPLL_CNTL3,
		.bit_idx = 31,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpll1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &g12a_mpll1_div.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct reg_sequence g12a_mpll2_init_regs[] = {
	{ .reg = HHI_MPLL_CNTL6,	.def = 0x40000033 },
};

static struct clk_regmap g12a_mpll2_div = {
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
		.init_regs = g12a_mpll2_init_regs,
		.init_count = ARRAY_SIZE(g12a_mpll2_init_regs),
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpll2_div",
		.ops = &meson_clk_mpll_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_mpll_prediv.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap g12a_mpll2 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_MPLL_CNTL5,
		.bit_idx = 31,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpll2",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &g12a_mpll2_div.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct reg_sequence g12a_mpll3_init_regs[] = {
	{ .reg = HHI_MPLL_CNTL8,	.def = 0x40000033 },
};

static struct clk_regmap g12a_mpll3_div = {
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
		.init_regs = g12a_mpll3_init_regs,
		.init_count = ARRAY_SIZE(g12a_mpll3_init_regs),
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpll3_div",
		.ops = &meson_clk_mpll_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_mpll_prediv.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap g12a_mpll3 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_MPLL_CNTL7,
		.bit_idx = 31,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpll3",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &g12a_mpll3_div.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static u32 mux_table_clk81[]	= { 0, 2, 3, 4, 5, 6, 7 };
static const struct clk_parent_data clk81_parent_data[] = {
	{ .fw_name = "xtal", },
	{ .hw = &g12a_fclk_div7.hw },
	{ .hw = &g12a_mpll1.hw },
	{ .hw = &g12a_mpll2.hw },
	{ .hw = &g12a_fclk_div4.hw },
	{ .hw = &g12a_fclk_div3.hw },
	{ .hw = &g12a_fclk_div5.hw },
};

static struct clk_regmap g12a_mpeg_clk_sel = {
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

static struct clk_regmap g12a_mpeg_clk_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_MPEG_CLK_CNTL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpeg_clk_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_mpeg_clk_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap g12a_clk81 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_MPEG_CLK_CNTL,
		.bit_idx = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "clk81",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_mpeg_clk_div.hw
		},
		.num_parents = 1,
		.flags = (CLK_SET_RATE_PARENT | CLK_IS_CRITICAL),
	},
};

static const struct clk_parent_data g12a_sd_emmc_clk0_parent_data[] = {
	{ .fw_name = "xtal", },
	{ .hw = &g12a_fclk_div2.hw },
	{ .hw = &g12a_fclk_div3.hw },
	{ .hw = &g12a_fclk_div5.hw },
	{ .hw = &g12a_fclk_div2p5.hw },
	/*
	 * Following these parent clocks, we should also have had mpll2, mpll3
	 * and gp0_pll but these clocks are too precious to be used here. All
	 * the necessary rates for MMC and NAND operation can be achieved using
	 * g12a_ee_core or fclk_div clocks
	 */
};

/* SDIO clock */
static struct clk_regmap g12a_sd_emmc_a_clk0_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_SD_EMMC_CLK_CNTL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_a_clk0_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = g12a_sd_emmc_clk0_parent_data,
		.num_parents = ARRAY_SIZE(g12a_sd_emmc_clk0_parent_data),
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap g12a_sd_emmc_a_clk0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_SD_EMMC_CLK_CNTL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_a_clk0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_sd_emmc_a_clk0_sel.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap g12a_sd_emmc_a_clk0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_SD_EMMC_CLK_CNTL,
		.bit_idx = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sd_emmc_a_clk0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_sd_emmc_a_clk0_div.hw
		},
		.num_parents = 1,
	},
};

/* SDcard clock */
static struct clk_regmap g12a_sd_emmc_b_clk0_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_SD_EMMC_CLK_CNTL,
		.mask = 0x7,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_b_clk0_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = g12a_sd_emmc_clk0_parent_data,
		.num_parents = ARRAY_SIZE(g12a_sd_emmc_clk0_parent_data),
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap g12a_sd_emmc_b_clk0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_SD_EMMC_CLK_CNTL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_b_clk0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_sd_emmc_b_clk0_sel.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap g12a_sd_emmc_b_clk0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_SD_EMMC_CLK_CNTL,
		.bit_idx = 23,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sd_emmc_b_clk0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_sd_emmc_b_clk0_div.hw
		},
		.num_parents = 1,
	},
};

/* EMMC/NAND clock */
static struct clk_regmap g12a_sd_emmc_c_clk0_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_NAND_CLK_CNTL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_c_clk0_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = g12a_sd_emmc_clk0_parent_data,
		.num_parents = ARRAY_SIZE(g12a_sd_emmc_clk0_parent_data),
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap g12a_sd_emmc_c_clk0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_NAND_CLK_CNTL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_c_clk0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_sd_emmc_c_clk0_sel.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap g12a_sd_emmc_c_clk0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_NAND_CLK_CNTL,
		.bit_idx = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sd_emmc_c_clk0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_sd_emmc_c_clk0_div.hw
		},
		.num_parents = 1,
	},
};

/* Video Clocks */

static struct clk_regmap g12a_vid_pll_div = {
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
		.parent_hws = (const struct clk_hw *[]) { &g12a_hdmi_pll.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
	},
};

static const struct clk_hw *g12a_vid_pll_parent_hws[] = {
	&g12a_vid_pll_div.hw,
	&g12a_hdmi_pll.hw,
};

static struct clk_regmap g12a_vid_pll_sel = {
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
		.parent_hws = g12a_vid_pll_parent_hws,
		.num_parents = ARRAY_SIZE(g12a_vid_pll_parent_hws),
		.flags = CLK_SET_RATE_NO_REPARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap g12a_vid_pll = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VID_PLL_CLK_DIV,
		.bit_idx = 19,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vid_pll",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_vid_pll_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

/* VPU Clock */

static const struct clk_hw *g12a_vpu_parent_hws[] = {
	&g12a_fclk_div3.hw,
	&g12a_fclk_div4.hw,
	&g12a_fclk_div5.hw,
	&g12a_fclk_div7.hw,
	&g12a_mpll1.hw,
	&g12a_vid_pll.hw,
	&g12a_gp0_pll.hw,
};

static u32 mux_table_vpu[] = {0, 1, 2, 3, 4, 5, 7};

static struct clk_regmap g12a_vpu_0_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VPU_CLK_CNTL,
		.mask = 0x7,
		.shift = 9,
		.table = mux_table_vpu,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vpu_0_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = g12a_vpu_parent_hws,
		.num_parents = ARRAY_SIZE(g12a_vpu_parent_hws),
	},
};

static struct clk_regmap g12a_vpu_0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VPU_CLK_CNTL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vpu_0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) { &g12a_vpu_0_sel.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap g12a_vpu_0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VPU_CLK_CNTL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &g12a_vpu_0_div.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap g12a_vpu_1_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VPU_CLK_CNTL,
		.mask = 0x7,
		.shift = 25,
		.table = mux_table_vpu,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vpu_1_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = g12a_vpu_parent_hws,
		.num_parents = ARRAY_SIZE(g12a_vpu_parent_hws),
		.flags = CLK_SET_RATE_NO_REPARENT,
	},
};

static struct clk_regmap g12a_vpu_1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VPU_CLK_CNTL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vpu_1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) { &g12a_vpu_1_sel.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap g12a_vpu_1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VPU_CLK_CNTL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &g12a_vpu_1_div.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap g12a_vpu = {
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
			&g12a_vpu_0.hw,
			&g12a_vpu_1.hw,
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_NO_REPARENT,
	},
};

/* VDEC clocks */

static u32 mux_table_vdec[] = { 0, 1, 2, 3, 4};

static const struct clk_hw *g12a_vdec_parent_hws[] = {
	&g12a_fclk_div2p5.hw,
	&g12a_fclk_div3.hw,
	&g12a_fclk_div4.hw,
	&g12a_fclk_div5.hw,
	&g12a_fclk_div7.hw,
};

/*
 * Due to add CLK_SET_RATE_PARENT flag, when set vdec clock rate,
 * It will probably change hifi pll rate, avoid change hifi pll
 * and gp0 pll rate, using mux table instead.
 */
static struct clk_regmap g12a_vdec_1_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VDEC_CLK_CNTL,
		.mask = 0x7,
		.shift = 9,
		.flags = CLK_MUX_ROUND_CLOSEST,
		.table = mux_table_vdec,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vdec_1_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = g12a_vdec_parent_hws,
		.num_parents = ARRAY_SIZE(g12a_vdec_parent_hws),
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap g12a_vdec_1_div = {
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
			&g12a_vdec_1_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap g12a_vdec_1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VDEC_CLK_CNTL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vdec_1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_vdec_1_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap g12a_vdec_hevcf_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VDEC2_CLK_CNTL,
		.mask = 0x7,
		.shift = 9,
		.flags = CLK_MUX_ROUND_CLOSEST,
		.table = mux_table_vdec,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vdec_hevcf_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = g12a_vdec_parent_hws,
		.num_parents = ARRAY_SIZE(g12a_vdec_parent_hws),
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap g12a_vdec_hevcf_div = {
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
			&g12a_vdec_hevcf_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap g12a_vdec_hevcf = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VDEC2_CLK_CNTL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vdec_hevcf",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_vdec_hevcf_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap g12a_vdec_hevc_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VDEC2_CLK_CNTL,
		.mask = 0x7,
		.shift = 25,
		.flags = CLK_MUX_ROUND_CLOSEST,
		.table = mux_table_vdec,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vdec_hevc_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = g12a_vdec_parent_hws,
		.num_parents = ARRAY_SIZE(g12a_vdec_parent_hws),
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap g12a_vdec_hevc_div = {
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
			&g12a_vdec_hevc_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap g12a_vdec_hevc = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VDEC2_CLK_CNTL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vdec_hevc",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_vdec_hevc_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/* wave_aclk */
static const struct clk_parent_data g12a_wave_parent_data[] = {
	{ .fw_name = "xtal", },
	{ .hw = &g12a_fclk_div4.hw },
	{ .hw = &g12a_fclk_div3.hw },
	{ .hw = &g12a_fclk_div5.hw },
	{ .hw = &g12a_fclk_div7.hw },
	{ .hw = &g12a_mpll2.hw },
	{ .hw = &g12a_mpll3.hw },
	{ .hw = &g12a_gp0_pll.hw },
};

static struct clk_regmap g12a_wave_a_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_WAVE420L_CLK_CNTL2,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "wave_a_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = g12a_wave_parent_data,
		.num_parents = ARRAY_SIZE(g12a_wave_parent_data),
	},
};

static struct clk_regmap g12a_wave_a_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_WAVE420L_CLK_CNTL2,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "wave_a_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_wave_a_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap g12a_wave_aclk = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_WAVE420L_CLK_CNTL2,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "wave_aclk",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_wave_a_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap g12a_wave_b_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_WAVE420L_CLK_CNTL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "wave_b_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = g12a_wave_parent_data,
		.num_parents = ARRAY_SIZE(g12a_wave_parent_data),
	},
};

static struct clk_regmap g12a_wave_b_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_WAVE420L_CLK_CNTL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "wave_b_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_wave_b_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap g12a_wave_bclk = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_WAVE420L_CLK_CNTL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "wave_bclk",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_wave_b_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap g12a_wave_c_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_WAVE420L_CLK_CNTL,
		.mask = 0x7,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data){
		.name = "wave_c_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = g12a_wave_parent_data,
		.num_parents = ARRAY_SIZE(g12a_wave_parent_data),
	},
};

static struct clk_regmap g12a_wave_c_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_WAVE420L_CLK_CNTL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "wave_c_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_wave_c_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap g12a_wave_cclk = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_WAVE420L_CLK_CNTL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "wave_cclk",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_wave_c_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/* VAPB Clock */

static const struct clk_hw *g12a_vapb_parent_hws[] = {
	&g12a_fclk_div4.hw,
	&g12a_fclk_div3.hw,
	&g12a_fclk_div5.hw,
	&g12a_fclk_div7.hw,
	&g12a_mpll1.hw,
	&g12a_vid_pll.hw,
	&g12a_mpll2.hw,
	&g12a_fclk_div2p5.hw,
};

static struct clk_regmap g12a_vapb_0_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VAPBCLK_CNTL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vapb_0_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = g12a_vapb_parent_hws,
		.num_parents = ARRAY_SIZE(g12a_vapb_parent_hws),
		.flags = CLK_SET_RATE_NO_REPARENT,
	},
};

static struct clk_regmap g12a_vapb_0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VAPBCLK_CNTL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vapb_0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_vapb_0_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap g12a_vapb_0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VAPBCLK_CNTL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vapb_0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_vapb_0_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap g12a_vapb_1_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VAPBCLK_CNTL,
		.mask = 0x3,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vapb_1_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = g12a_vapb_parent_hws,
		.num_parents = ARRAY_SIZE(g12a_vapb_parent_hws),
		.flags = CLK_SET_RATE_NO_REPARENT,
	},
};

static struct clk_regmap g12a_vapb_1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VAPBCLK_CNTL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vapb_1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_vapb_1_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap g12a_vapb_1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VAPBCLK_CNTL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vapb_1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_vapb_1_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap g12a_vapb_sel = {
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
			&g12a_vapb_0.hw,
			&g12a_vapb_1.hw,
		},
		.num_parents = 2,
#ifdef CONFIG_AMLOGIC_MODIFY
		.flags = CLK_SET_RATE_PARENT | CLK_SET_RATE_NO_REPARENT,
#else
		.flags = CLK_SET_RATE_NO_REPARENT,
#endif
	},
};

#ifdef CONFIG_AMLOGIC_MODIFY
static struct clk_regmap g12a_ge2d_gate = {
#else
static struct clk_regmap g12a_vapb = {
#endif
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VAPBCLK_CNTL,
		.bit_idx = 30,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vapb",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &g12a_vapb_sel.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static const struct clk_hw *g12a_vclk_parent_hws[] = {
	&g12a_vid_pll.hw,
	&g12a_gp0_pll.hw,
	&g12a_hifi_pll.hw,
	&g12a_mpll1.hw,
	&g12a_fclk_div3.hw,
	&g12a_fclk_div4.hw,
	&g12a_fclk_div5.hw,
	&g12a_fclk_div7.hw,
};

static struct clk_regmap g12a_vclk_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VID_CLK_CNTL,
		.mask = 0x7,
		.shift = 16,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vclk_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = g12a_vclk_parent_hws,
		.num_parents = ARRAY_SIZE(g12a_vclk_parent_hws),
		.flags = CLK_SET_RATE_NO_REPARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap g12a_vclk2_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VIID_CLK_CNTL,
		.mask = 0x7,
		.shift = 16,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vclk2_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = g12a_vclk_parent_hws,
		.num_parents = ARRAY_SIZE(g12a_vclk_parent_hws),
		.flags = CLK_SET_RATE_NO_REPARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap g12a_vclk_input = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VID_CLK_DIV,
		.bit_idx = 16,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk_input",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &g12a_vclk_sel.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap g12a_vclk2_input = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VIID_CLK_DIV,
		.bit_idx = 16,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk2_input",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &g12a_vclk2_sel.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap g12a_vclk_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VID_CLK_DIV,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vclk_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_vclk_input.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap g12a_vclk2_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VIID_CLK_DIV,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vclk2_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_vclk2_input.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap g12a_vclk = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VID_CLK_CNTL,
		.bit_idx = 19,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &g12a_vclk_div.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap g12a_vclk2 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VIID_CLK_CNTL,
		.bit_idx = 19,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk2",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &g12a_vclk2_div.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap g12a_vclk_div1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VID_CLK_CNTL,
		.bit_idx = 0,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk_div1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &g12a_vclk.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap g12a_vclk_div2_en = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VID_CLK_CNTL,
		.bit_idx = 1,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk_div2_en",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &g12a_vclk.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap g12a_vclk_div4_en = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VID_CLK_CNTL,
		.bit_idx = 2,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk_div4_en",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &g12a_vclk.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap g12a_vclk_div6_en = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VID_CLK_CNTL,
		.bit_idx = 3,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk_div6_en",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &g12a_vclk.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap g12a_vclk_div12_en = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VID_CLK_CNTL,
		.bit_idx = 4,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk_div12_en",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &g12a_vclk.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap g12a_vclk2_div1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VIID_CLK_CNTL,
		.bit_idx = 0,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk2_div1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &g12a_vclk2.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap g12a_vclk2_div2_en = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VIID_CLK_CNTL,
		.bit_idx = 1,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk2_div2_en",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &g12a_vclk2.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap g12a_vclk2_div4_en = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VIID_CLK_CNTL,
		.bit_idx = 2,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk2_div4_en",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &g12a_vclk2.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap g12a_vclk2_div6_en = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VIID_CLK_CNTL,
		.bit_idx = 3,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk2_div6_en",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &g12a_vclk2.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap g12a_vclk2_div12_en = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VIID_CLK_CNTL,
		.bit_idx = 4,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk2_div12_en",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &g12a_vclk2.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_fixed_factor g12a_vclk_div2 = {
	.mult = 1,
	.div = 2,
	.hw.init = &(struct clk_init_data){
		.name = "vclk_div2",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_vclk_div2_en.hw
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor g12a_vclk_div4 = {
	.mult = 1,
	.div = 4,
	.hw.init = &(struct clk_init_data){
		.name = "vclk_div4",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_vclk_div4_en.hw
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor g12a_vclk_div6 = {
	.mult = 1,
	.div = 6,
	.hw.init = &(struct clk_init_data){
		.name = "vclk_div6",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_vclk_div6_en.hw
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor g12a_vclk_div12 = {
	.mult = 1,
	.div = 12,
	.hw.init = &(struct clk_init_data){
		.name = "vclk_div12",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_vclk_div12_en.hw
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor g12a_vclk2_div2 = {
	.mult = 1,
	.div = 2,
	.hw.init = &(struct clk_init_data){
		.name = "vclk2_div2",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_vclk2_div2_en.hw
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor g12a_vclk2_div4 = {
	.mult = 1,
	.div = 4,
	.hw.init = &(struct clk_init_data){
		.name = "vclk2_div4",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_vclk2_div4_en.hw
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor g12a_vclk2_div6 = {
	.mult = 1,
	.div = 6,
	.hw.init = &(struct clk_init_data){
		.name = "vclk2_div6",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_vclk2_div6_en.hw
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor g12a_vclk2_div12 = {
	.mult = 1,
	.div = 12,
	.hw.init = &(struct clk_init_data){
		.name = "vclk2_div12",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_vclk2_div12_en.hw
		},
		.num_parents = 1,
	},
};

static u32 mux_table_cts_sel[] = { 0, 1, 2, 3, 4, 8, 9, 10, 11, 12 };
static const struct clk_hw *g12a_cts_parent_hws[] = {
	&g12a_vclk_div1.hw,
	&g12a_vclk_div2.hw,
	&g12a_vclk_div4.hw,
	&g12a_vclk_div6.hw,
	&g12a_vclk_div12.hw,
	&g12a_vclk2_div1.hw,
	&g12a_vclk2_div2.hw,
	&g12a_vclk2_div4.hw,
	&g12a_vclk2_div6.hw,
	&g12a_vclk2_div12.hw,
};

static struct clk_regmap g12a_cts_enci_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VID_CLK_DIV,
		.mask = 0xf,
		.shift = 28,
		.table = mux_table_cts_sel,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cts_enci_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = g12a_cts_parent_hws,
		.num_parents = ARRAY_SIZE(g12a_cts_parent_hws),
		.flags = CLK_SET_RATE_NO_REPARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap g12a_cts_encp_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VID_CLK_DIV,
		.mask = 0xf,
		.shift = 20,
		.table = mux_table_cts_sel,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cts_encp_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = g12a_cts_parent_hws,
		.num_parents = ARRAY_SIZE(g12a_cts_parent_hws),
		.flags = CLK_SET_RATE_NO_REPARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap g12a_cts_vdac_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VIID_CLK_DIV,
		.mask = 0xf,
		.shift = 28,
		.table = mux_table_cts_sel,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cts_vdac_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = g12a_cts_parent_hws,
		.num_parents = ARRAY_SIZE(g12a_cts_parent_hws),
		.flags = CLK_SET_RATE_NO_REPARENT | CLK_GET_RATE_NOCACHE,
	},
};

/* TOFIX: add support for cts_tcon */
static u32 mux_table_hdmi_tx_sel[] = { 0, 1, 2, 3, 4, 8, 9, 10, 11, 12 };
static const struct clk_hw *g12a_cts_hdmi_tx_parent_hws[] = {
	&g12a_vclk_div1.hw,
	&g12a_vclk_div2.hw,
	&g12a_vclk_div4.hw,
	&g12a_vclk_div6.hw,
	&g12a_vclk_div12.hw,
	&g12a_vclk2_div1.hw,
	&g12a_vclk2_div2.hw,
	&g12a_vclk2_div4.hw,
	&g12a_vclk2_div6.hw,
	&g12a_vclk2_div12.hw,
};

static struct clk_regmap g12a_hdmi_tx_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_HDMI_CLK_CNTL,
		.mask = 0xf,
		.shift = 16,
		.table = mux_table_hdmi_tx_sel,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmi_tx_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = g12a_cts_hdmi_tx_parent_hws,
		.num_parents = ARRAY_SIZE(g12a_cts_hdmi_tx_parent_hws),
		.flags = CLK_SET_RATE_NO_REPARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap g12a_cts_enci = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VID_CLK_CNTL2,
		.bit_idx = 0,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "cts_enci",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_cts_enci_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap g12a_cts_encp = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VID_CLK_CNTL2,
		.bit_idx = 2,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "cts_encp",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_cts_encp_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap g12a_cts_vdac = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VID_CLK_CNTL2,
		.bit_idx = 4,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "cts_vdac",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_cts_vdac_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap g12a_hdmi_tx = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VID_CLK_CNTL2,
		.bit_idx = 5,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmi_tx",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_hdmi_tx_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

/* HDMI Clocks */

static const struct clk_parent_data g12a_hdmi_parent_data[] = {
	{ .fw_name = "xtal", },
	{ .hw = &g12a_fclk_div4.hw },
	{ .hw = &g12a_fclk_div3.hw },
	{ .hw = &g12a_fclk_div5.hw },
};

static struct clk_regmap g12a_hdmi_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_HDMI_CLK_CNTL,
		.mask = 0x3,
		.shift = 9,
		.flags = CLK_MUX_ROUND_CLOSEST,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmi_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = g12a_hdmi_parent_data,
		.num_parents = ARRAY_SIZE(g12a_hdmi_parent_data),
		.flags = CLK_SET_RATE_NO_REPARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap g12a_hdmi_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_HDMI_CLK_CNTL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmi_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) { &g12a_hdmi_sel.hw },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap g12a_hdmi = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_HDMI_CLK_CNTL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmi",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &g12a_hdmi_div.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

/*
 * The MALI IP is clocked by two identical clocks (mali_0 and mali_1)
 * muxed by a glitch-free switch on Meson8b and Meson8m2 and later.
 *
 * CLK_SET_RATE_PARENT is added for mali_0_sel clock
 * 1.gp0 pll only support the 846M, avoid other rate 500/400M from it
 * 2.hifi pll is used for other module, skip it, avoid some rate from it
 */
static u32 mux_table_mali[] = { 0, 1, 3, 4, 5, 6, 7 };

static const struct clk_parent_data g12a_mali_0_1_parent_data[] = {
	{ .fw_name = "xtal", },
	{ .hw = &g12a_gp0_pll.hw },
	{ .hw = &g12a_fclk_div2p5.hw },
	{ .hw = &g12a_fclk_div3.hw },
	{ .hw = &g12a_fclk_div4.hw },
	{ .hw = &g12a_fclk_div5.hw },
	{ .hw = &g12a_fclk_div7.hw },
};

static struct clk_regmap g12a_mali_0_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_MALI_CLK_CNTL,
		.mask = 0x7,
		.shift = 9,
		.table = mux_table_mali,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mali_0_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = g12a_mali_0_1_parent_data,
		.num_parents = ARRAY_SIZE(g12a_mali_0_1_parent_data),
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap g12a_mali_0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_MALI_CLK_CNTL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mali_0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_mali_0_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap g12a_mali_0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_MALI_CLK_CNTL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mali_0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_mali_0_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_GATE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap g12a_mali_1_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_MALI_CLK_CNTL,
		.mask = 0x7,
		.shift = 25,
		.table = mux_table_mali,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mali_1_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = g12a_mali_0_1_parent_data,
		.num_parents = ARRAY_SIZE(g12a_mali_0_1_parent_data),
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap g12a_mali_1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_MALI_CLK_CNTL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mali_1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_mali_1_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap g12a_mali_1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_MALI_CLK_CNTL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mali_1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_mali_1_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_GATE | CLK_SET_RATE_PARENT,
	},
};

static const struct clk_hw *g12a_mali_parent_hws[] = {
	&g12a_mali_0.hw,
	&g12a_mali_1.hw,
};

static struct clk_regmap g12a_mali = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_MALI_CLK_CNTL,
		.mask = 1,
		.shift = 31,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mali",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = g12a_mali_parent_hws,
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap g12a_ts_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_TS_CLK_CNTL,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "ts_div",
		.ops = &clk_regmap_divider_ro_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "xtal",
		},
		.num_parents = 1,
	},
};

static struct clk_regmap g12a_ts = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_TS_CLK_CNTL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "ts",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_ts_div.hw
		},
		.num_parents = 1,
	},
};

/*
 * media clocks
 */

/* cts_vdec_clk */
static const struct clk_parent_data g12a_dec_parent_hws[] = {
	{ .hw = &g12a_fclk_div2p5.hw },
	{ .hw = &g12a_fclk_div3.hw },
	{ .hw = &g12a_fclk_div4.hw },
	{ .hw = &g12a_fclk_div5.hw },
	{ .hw = &g12a_fclk_div7.hw },
	{ .hw = &g12a_hifi_pll.hw },
	{ .hw = &g12a_gp0_pll.hw },
	{ .fw_name = "xtal", },
};

static struct clk_regmap g12a_vdec_p1_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VDEC3_CLK_CNTL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vdec_p1_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = g12a_dec_parent_hws,
		.num_parents = ARRAY_SIZE(g12a_dec_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap g12a_vdec_p1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VDEC3_CLK_CNTL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vdec_p1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_vdec_p1_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap g12a_vdec_p1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VDEC3_CLK_CNTL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vdec_p1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_vdec_p1_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

/* cts_hcodec_clk */
static struct clk_regmap g12a_hcodec_p0_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VDEC_CLK_CNTL,
		.mask = 0x7,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hcodec_p0_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = g12a_dec_parent_hws,
		.num_parents = ARRAY_SIZE(g12a_dec_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap g12a_hcodec_p0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VDEC_CLK_CNTL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hcodec_p0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_hcodec_p0_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap g12a_hcodec_p0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VDEC_CLK_CNTL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hcodec_p0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_hcodec_p0_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap g12a_hcodec_p1_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VDEC3_CLK_CNTL,
		.mask = 0x7,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hcodec_p1_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = g12a_dec_parent_hws,
		.num_parents = ARRAY_SIZE(g12a_dec_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap g12a_hcodec_p1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VDEC3_CLK_CNTL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hcodec_p1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_hcodec_p1_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap g12a_hcodec_p1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VDEC3_CLK_CNTL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hcodec_p1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_hcodec_p1_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data g12a_hcodec_mux_parent_hws[] = {
	{ .hw = &g12a_hcodec_p0.hw },
	{ .hw = &g12a_hcodec_p1.hw },
};

static struct clk_regmap g12a_hcodec_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VDEC3_CLK_CNTL,
		.mask = 0x1,
		.shift = 31,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hcodec_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = g12a_hcodec_mux_parent_hws,
		.num_parents = ARRAY_SIZE(g12a_hcodec_mux_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

/* cts_hevcb_clk */

static struct clk_regmap g12a_hevc_p1_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VDEC4_CLK_CNTL,
		.mask = 0x7,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hevc_p1_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = g12a_dec_parent_hws,
		.num_parents = ARRAY_SIZE(g12a_dec_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap g12a_hevc_p1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VDEC4_CLK_CNTL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hevc_p1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_hevc_p1_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap g12a_hevc_p1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VDEC4_CLK_CNTL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hevc_p1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_hevc_p1_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

/* cts_hevcf_clk */

static struct clk_regmap g12a_hevcf_p1_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VDEC4_CLK_CNTL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hevcf_p1_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = g12a_dec_parent_hws,
		.num_parents = ARRAY_SIZE(g12a_dec_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap g12a_hevcf_p1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VDEC4_CLK_CNTL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hevcf_p1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_hevcf_p1_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap g12a_hevcf_p1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VDEC4_CLK_CNTL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hevcf_p1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_hevcf_p1_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

/* vpu_clkb_tmp */

/* cts_vpu_clkc */
static const char * const vpu_clkc_parent_names[] = { "fclk_div4",
	"fclk_div3", "fclk_div5", "fclk_div7", "mpll1", "vid_pll",
	"mpll2",  "gp0_pll"};

static struct clk_regmap g12a_vpu_clkc_p0_mux  = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VPU_CLKC_CNTL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_clkc_p0_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_names = vpu_clkc_parent_names,
		.num_parents = ARRAY_SIZE(vpu_clkc_parent_names),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap g12a_vpu_clkc_p0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VPU_CLKC_CNTL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_clkc_p0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_vpu_clkc_p0_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap g12a_vpu_clkc_p0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VPU_CLKC_CNTL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vpu_clkc_p0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_vpu_clkc_p0_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

/* cts_vpu_clk */

static const struct clk_parent_data vpu_parent_hws[] = {
	{ .hw = &g12a_fclk_div3.hw },
	{ .hw = &g12a_fclk_div4.hw },
	{ .hw = &g12a_fclk_div5.hw },
	{ .hw = &g12a_fclk_div7.hw },
	{ .fw_name = "mp1l0", },
	{ .hw = &g12a_vid_pll.hw },
	{ .hw = &g12a_hifi_pll.hw },
	{ .hw = &g12a_gp0_pll.hw },
};

static struct clk_regmap g12a_vpu_clkc_p1_mux = {
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

static struct clk_regmap g12a_vpu_clkc_p1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VPU_CLKC_CNTL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_clkc_p1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_vpu_clkc_p1_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap g12a_vpu_clkc_p1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VPU_CLKC_CNTL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vpu_clkc_p1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_vpu_clkc_p1_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data g12a_vpu_mux_parent_hws[] = {
	{ .hw = &g12a_vpu_clkc_p0.hw },
	{ .hw = &g12a_vpu_clkc_p1.hw },
};

static struct clk_regmap g12a_vpu_clkc_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VPU_CLKC_CNTL,
		.mask = 0x1,
		.shift = 31,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_clkc_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = g12a_vpu_mux_parent_hws,
		.num_parents = ARRAY_SIZE(g12a_vpu_mux_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data g12a_meas_parent_hws[] = {
	{ .fw_name = "xtal", },
	{ .hw = &g12a_fclk_div4.hw },
	{ .hw = &g12a_fclk_div3.hw },
	{ .hw = &g12a_fclk_div5.hw },
	{ .hw = &g12a_vid_pll.hw },
	{ .hw = &g12a_gp0_pll.hw },
	{ .hw = &g12a_fclk_div2.hw },
	{ .hw = &g12a_fclk_div7.hw },
};

static struct clk_regmap g12a_dsi_meas_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VDIN_MEAS_CLK_CNTL,
		.mask = 0x7,
		.shift = 21,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "dsi_meas_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = g12a_meas_parent_hws,
		.num_parents = ARRAY_SIZE(g12a_meas_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap g12a_dsi_meas_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VDIN_MEAS_CLK_CNTL,
		.shift = 12,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "dsi_meas_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_dsi_meas_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap g12a_dsi_meas = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VDIN_MEAS_CLK_CNTL,
		.bit_idx = 20,
	},
	.hw.init = &(struct clk_init_data){
		.name = "dsi_meas",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_dsi_meas_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

#ifdef CONFIG_AMLOGIC_MODIFY
/* cts_vdin_meas_clk */
static const struct clk_parent_data sm1_vdin_parent_hws[] = {
	{ .fw_name = "xtal", },
	{ .hw = &g12a_fclk_div4.hw },
	{ .hw = &g12a_fclk_div3.hw },
	{ .hw = &g12a_fclk_div5.hw },
	{ .hw = &g12a_vid_pll.hw },
	{ .hw = &g12a_gp0_pll.hw },
	{ .hw = &g12a_fclk_div2.hw },
	{ .hw = &g12a_fclk_div7.hw },
};

static struct clk_regmap sm1_vdin_meas_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VDIN_MEAS_CLK_CNTL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vdin_meas_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = sm1_vdin_parent_hws,
		.num_parents = ARRAY_SIZE(sm1_vdin_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap sm1_vdin_meas_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VDIN_MEAS_CLK_CNTL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vdin_meas_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sm1_vdin_meas_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap sm1_vdin_meas_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VDIN_MEAS_CLK_CNTL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vdin_meas_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sm1_vdin_meas_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data sm1_vipnanoq_parent_hws[] = {
	{ .fw_name = "xtal", },
	{ .hw = &g12a_gp0_pll.hw },
	{ .hw = &g12a_hifi_pll.hw },
	{ .hw = &g12a_fclk_div2p5.hw },
	{ .hw = &g12a_fclk_div3.hw },
	{ .hw = &g12a_fclk_div4.hw },
	{ .hw = &g12a_fclk_div5.hw },
	{ .hw = &g12a_fclk_div7.hw },
};

static struct clk_regmap sm1_vipnanoq_core_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VIPNANOQ_CLK_CNTL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vipnanoq_core_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = sm1_vipnanoq_parent_hws,
		.num_parents = ARRAY_SIZE(sm1_vipnanoq_parent_hws),
	},
};

static struct clk_regmap sm1_vipnanoq_core_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VIPNANOQ_CLK_CNTL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vipnanoq_core_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sm1_vipnanoq_core_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap sm1_vipnanoq_core_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VIPNANOQ_CLK_CNTL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vipnanoq_core_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sm1_vipnanoq_core_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap sm1_vipnanoq_axi_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VIPNANOQ_CLK_CNTL,
		.mask = 0x7,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vipnanoq_axi_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = sm1_vipnanoq_parent_hws,
		.num_parents = ARRAY_SIZE(sm1_vipnanoq_parent_hws),
	},
};

static struct clk_regmap sm1_vipnanoq_axi_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VIPNANOQ_CLK_CNTL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vipnanoq_axi_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sm1_vipnanoq_axi_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap sm1_vipnanoq_axi_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VIPNANOQ_CLK_CNTL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vipnanoq_axi_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sm1_vipnanoq_axi_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/* GPIO 25M */
static struct clk_regmap g12a_25m_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_XTAL_DIVN_CNTL,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "25m_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_fclk_div2.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap g12a_25m_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_XTAL_DIVN_CNTL,
		.bit_idx = 12,
	},
	.hw.init = &(struct clk_init_data){
		.name = "25m",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_25m_div.hw
		},
		.num_parents = 1,
		.flags = (CLK_SET_RATE_PARENT),
	},
};

/*
 * clk_12m_24m model
 *
 *          |------|     |-----| clk_12m_24m |-----|
 * xtal---->| gate |---->| div |------------>| pad |
 *          |------|     |-----|             |-----|
 */
static struct clk_regmap g12a_24m_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_XTAL_DIVN_CNTL,
		.bit_idx = 11,
	},
	.hw.init = &(struct clk_init_data){
		.name = "24m_clk_in",
		.ops = &clk_regmap_gate_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "xtal",
		},
		.num_parents = 1,
		.flags = CLK_IS_CRITICAL,
	},
};

static struct clk_regmap g12a_12m_24m_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_XTAL_DIVN_CNTL,
		.shift = 10,
		.width = 1,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "12m_24m_clk",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_24m_gate.hw
		},
		.num_parents = 1,
	},
};

static u32 mux_table_gen_clk[]	= { 0, 5, 7, 20, 21, 22, 23,
				   24, 25, 26, 27, 28 };
static const struct clk_parent_data g12a_gen_parent_hws[] = {
	{ .fw_name = "xtal", },
	{ .hw = &g12a_gp0_pll.hw },
	{ .hw = &g12a_hifi_pll.hw },
	{ .hw = &g12a_fclk_div2.hw },
	{ .hw = &g12a_fclk_div3.hw },
	{ .hw = &g12a_fclk_div4.hw },
	{ .hw = &g12a_fclk_div5.hw },
	{ .hw = &g12a_fclk_div7.hw },
	{ .hw = &g12a_mpll0.hw },
	{ .hw = &g12a_mpll1.hw },
	{ .hw = &g12a_mpll2.hw },
	{ .hw = &g12a_mpll3.hw },

};

static struct clk_regmap g12a_gen_clk_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_GEN_CLK_CNTL,
		.mask = 0x1f,
		.shift = 12,
		.table = mux_table_gen_clk,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "gen_clk_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = g12a_gen_parent_hws,
		.num_parents = ARRAY_SIZE(g12a_gen_parent_hws),
	},
};

static struct clk_regmap g12a_gen_clk_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_GEN_CLK_CNTL,
		.shift = 0,
		.width = 11,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "gen_clk_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_gen_clk_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap g12a_gen_clk = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_GEN_CLK_CNTL,
		.bit_idx = 11,
	},
	.hw.init = &(struct clk_init_data){
		.name = "gen_clk",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_gen_clk_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data sm1_media_parent_adapt_hws[] = {
	{ .fw_name = "xtal", },
	{ .hw = &g12a_fclk_div4.hw },
	{ .hw = &g12a_fclk_div3.hw },
	{ .hw = &g12a_fclk_div5.hw },
	{ .hw = &g12a_fclk_div7.hw },
	{ .hw = &g12a_mpll2.hw },
	{ .hw = &g12a_mpll3.hw },
	{ .hw = &g12a_gp0_pll.hw },
};

static struct clk_regmap sm1_csi_adapt_clk_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_CSI2_ADAPT_CLK_CNTL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "csi_adapt_clk_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = sm1_media_parent_adapt_hws,
		.num_parents = ARRAY_SIZE(sm1_media_parent_adapt_hws),
	},
};

static struct clk_regmap sm1_csi_adapt_clk_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_CSI2_ADAPT_CLK_CNTL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "csi_adapt_clk_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sm1_csi_adapt_clk_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap sm1_csi_adapt_clk_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_CSI2_ADAPT_CLK_CNTL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "csi_adapt_clk_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sm1_csi_adapt_clk_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data g12b_media_parent_hws[] = {
	{ .fw_name = "xtal", },
	{ .hw = &g12a_gp0_pll.hw },
	{ .hw = &g12a_hifi_pll.hw },
	{ .hw = &g12a_fclk_div2p5.hw },
	{ .hw = &g12a_fclk_div3.hw },
	{ .hw = &g12a_fclk_div4.hw },
	{ .hw = &g12a_fclk_div5.hw },
	{ .hw = &g12a_fclk_div7.hw },
};

static const struct clk_parent_data g12b_media_parent_mipi_hws[] = {
	{ .fw_name = "xtal", },
	{ .hw = &g12a_gp0_pll.hw },
	{ .hw = &g12a_mpll1.hw },
	{ .hw = &g12a_mpll2.hw },
	{ .hw = &g12a_fclk_div3.hw },
	{ .hw = &g12a_fclk_div4.hw },
	{ .hw = &g12a_fclk_div5.hw },
	{ .hw = &g12a_fclk_div7.hw },
};

static struct clk_regmap g12b_gdc_core_clk_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_APICALGDC_CNTL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "gdc_core_clk_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = g12b_media_parent_hws,
		.num_parents = ARRAY_SIZE(g12b_media_parent_hws),
	},
};

static struct clk_regmap g12b_gdc_core_clk_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_APICALGDC_CNTL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "gdc_core_clk_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12b_gdc_core_clk_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap g12b_gdc_core_clk_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_APICALGDC_CNTL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "gdc_core_clk_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12b_gdc_core_clk_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap g12b_gdc_axi_clk_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_APICALGDC_CNTL,
		.mask = 0x7,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data){
		.name = "gdc_axi_clk_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = g12b_media_parent_hws,
		.num_parents = ARRAY_SIZE(g12b_media_parent_hws),
	},
};

static struct clk_regmap g12b_gdc_axi_clk_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_APICALGDC_CNTL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "gdc_axi_clk_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12b_gdc_axi_clk_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap g12b_gdc_axi_clk_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_APICALGDC_CNTL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "gdc_axi_clk_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12b_gdc_axi_clk_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap g12b_mipi_isp_clk_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_MIPI_ISP_CLK_CNTL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mipi_isp_clk_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = g12b_media_parent_hws,
		.num_parents = ARRAY_SIZE(g12b_media_parent_hws),
	},
};

static struct clk_regmap g12b_mipi_isp_clk_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_MIPI_ISP_CLK_CNTL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mipi_isp_clk_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12b_mipi_isp_clk_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap g12b_mipi_isp_clk_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_MIPI_ISP_CLK_CNTL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mipi_isp_clk_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12b_mipi_isp_clk_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap g12b_mipi_csi_phy_clk0_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_MIPI_CSI_PHY_CLK_CNTL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mipi_csi_phy_clk0_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = g12b_media_parent_mipi_hws,
		.num_parents = ARRAY_SIZE(g12b_media_parent_mipi_hws),
	},
};

static struct clk_regmap g12b_mipi_csi_phy_clk0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_MIPI_CSI_PHY_CLK_CNTL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mipi_csi_phy_clk0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12b_mipi_csi_phy_clk0_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap g12b_mipi_csi_phy_clk0_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_MIPI_CSI_PHY_CLK_CNTL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mipi_csi_phy_clk0_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12b_mipi_csi_phy_clk0_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap g12b_mipi_csi_phy_clk1_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_MIPI_CSI_PHY_CLK_CNTL,
		.mask = 0x7,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mipi_csi_phy_clk1_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = g12b_media_parent_mipi_hws,
		.num_parents = ARRAY_SIZE(g12b_media_parent_mipi_hws),
	},
};

static struct clk_regmap g12b_mipi_csi_phy_clk1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_MIPI_CSI_PHY_CLK_CNTL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mipi_csi_phy_clk1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12b_mipi_csi_phy_clk1_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap g12b_mipi_csi_phy_clk1_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_MIPI_CSI_PHY_CLK_CNTL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mipi_csi_phy_clk1_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12b_mipi_csi_phy_clk1_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap g12b_mipi_sci_phy_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_MIPI_CSI_PHY_CLK_CNTL,
		.mask = 0x1,
		.shift = 31,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mipi_sci_phy_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12b_mipi_csi_phy_clk0_gate.hw,
			&g12b_mipi_csi_phy_clk1_gate.hw
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

#endif

static const struct clk_parent_data g12a_vdec_mux_parent_hws[] = {
	{ .hw = &g12a_vdec_1.hw },
	{ .hw = &g12a_vdec_p1.hw },
};

static struct clk_regmap g12a_vdec_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VDEC3_CLK_CNTL,
		.mask = 0x1,
		.shift = 15,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vdec_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = g12a_vdec_mux_parent_hws,
		.num_parents = ARRAY_SIZE(g12a_vdec_mux_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data g12a_hevc_mux_parent_hws[] = {
	{ .hw = &g12a_vdec_hevc.hw },
	{ .hw = &g12a_hevc_p1.hw },
};

static struct clk_regmap g12a_hevc_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VDEC4_CLK_CNTL,
		.mask = 0x1,
		.shift = 31,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hevc_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = g12a_hevc_mux_parent_hws,
		.num_parents = ARRAY_SIZE(g12a_hevc_mux_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data g12a_hevcf_mux_parent_hws[] = {
	{ .hw = &g12a_vdec_hevcf.hw },
	{ .hw = &g12a_hevcf_p1.hw },
};

static struct clk_regmap g12a_hevcf_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VDEC4_CLK_CNTL,
		.mask = 0x1,
		.shift = 15,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hevcf_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = g12a_hevcf_mux_parent_hws,
		.num_parents = ARRAY_SIZE(g12a_hevcf_mux_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data vpu_clkb_tmp_parent_hws[] = {
	{ .hw = &g12a_vpu.hw },
	{ .hw = &g12a_fclk_div4.hw },
	{ .hw = &g12a_fclk_div5.hw },
	{ .hw = &g12a_fclk_div7.hw },
};

static struct clk_regmap g12a_vpu_clkb_tmp_mux = {
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

static struct clk_regmap g12a_vpu_clkb_tmp_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VPU_CLKB_CNTL,
		.shift = 16,
		.width = 4,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_clkb_tmp_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_vpu_clkb_tmp_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap g12a_vpu_clkb_tmp = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VPU_CLKB_CNTL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vpu_clkb_tmp",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_vpu_clkb_tmp_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap g12a_vpu_clkb_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VPU_CLKB_CNTL,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_clkb_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_vpu_clkb_tmp.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap g12a_vpu_clkb = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VPU_CLKB_CNTL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vpu_clkb",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_vpu_clkb_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data g12a_spicc_parent_hws[] = {
	{ .fw_name = "xtal", },
	{ .hw = &g12a_clk81.hw },
	{ .hw = &g12a_fclk_div4.hw },
	{ .hw = &g12a_fclk_div3.hw },
	{ .hw = &g12a_fclk_div2.hw },
	{ .hw = &g12a_fclk_div5.hw },
	{ .hw = &g12a_fclk_div7.hw },
	{ .hw = &g12a_gp0_pll.hw },
};

static struct clk_regmap g12a_spicc0_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_SPICC_CLK_CNTL,
		.mask = 0x7,
		.shift = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spicc0_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = g12a_spicc_parent_hws,
		.num_parents = ARRAY_SIZE(g12a_spicc_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap g12a_spicc0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_SPICC_CLK_CNTL,
		.shift = 0,
		.width = 6,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spicc0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_spicc0_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap g12a_spicc0_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_SPICC_CLK_CNTL,
		.bit_idx = 6,
	},
	.hw.init = &(struct clk_init_data){
		.name = "spicc0_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_spicc0_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap g12a_spicc1_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_SPICC_CLK_CNTL,
		.mask = 0x7,
		.shift = 23,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spicc1_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = g12a_spicc_parent_hws,
		.num_parents = ARRAY_SIZE(g12a_spicc_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap g12a_spicc1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_SPICC_CLK_CNTL,
		.shift = 16,
		.width = 6,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spicc1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_spicc1_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap g12a_spicc1_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_SPICC_CLK_CNTL,
		.bit_idx = 22,
	},
	.hw.init = &(struct clk_init_data){
		.name = "spicc1_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_spicc1_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

/*cts_bt656*/
static const struct clk_parent_data g12a_bt656_parent_hws[] = {
	{ .hw = &g12a_fclk_div2.hw },
	{ .hw = &g12a_fclk_div3.hw },
	{ .hw = &g12a_fclk_div5.hw },
	{ .hw = &g12a_fclk_div7.hw },
};

static struct clk_regmap g12a_bt656_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_BT656_CLK_CNTL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "bt656_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = g12a_bt656_parent_hws,
		.num_parents = ARRAY_SIZE(g12a_bt656_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap g12a_bt656_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_BT656_CLK_CNTL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "bt656_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_bt656_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap g12a_bt656_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_BT656_CLK_CNTL,
		.bit_idx = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "bt656_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&g12a_bt656_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

#define MESON_GATE(_name, _reg, _bit) \
	MESON_PCLK(_name, _reg, _bit, &g12a_clk81.hw)

#define MESON_GATE_RO(_name, _reg, _bit) \
	MESON_PCLK_RO(_name, _reg, _bit, &g12a_clk81.hw)

/* Everything Else (EE) domain gates */
static MESON_GATE(g12a_ddr,			HHI_GCLK_MPEG0,	0);
static MESON_GATE(g12a_dos,			HHI_GCLK_MPEG0,	1);
static MESON_GATE(g12a_audio_locker,		HHI_GCLK_MPEG0,	2);
static MESON_GATE(g12a_mipi_dsi_host,		HHI_GCLK_MPEG0,	3);
static MESON_GATE(g12a_eth_phy,			HHI_GCLK_MPEG0,	4);
static MESON_GATE(g12a_isa,			HHI_GCLK_MPEG0,	5);
static MESON_GATE(g12a_pl301,			HHI_GCLK_MPEG0,	6);
static MESON_GATE(g12a_periphs,			HHI_GCLK_MPEG0,	7);
static MESON_GATE(g12a_spicc_0,			HHI_GCLK_MPEG0,	8);
static MESON_GATE(g12a_i2c,			HHI_GCLK_MPEG0,	9);
static MESON_GATE(g12a_sana,			HHI_GCLK_MPEG0,	10);
static MESON_GATE(g12a_sd,			HHI_GCLK_MPEG0,	11);
static MESON_GATE(g12a_rng0,			HHI_GCLK_MPEG0,	12);
static MESON_GATE(g12a_uart0,			HHI_GCLK_MPEG0,	13);
static MESON_GATE(g12a_spicc_1,			HHI_GCLK_MPEG0,	14);
static MESON_GATE(g12a_hiu_reg,			HHI_GCLK_MPEG0,	19);
static MESON_GATE(g12a_mipi_dsi_phy,		HHI_GCLK_MPEG0,	20);
static MESON_GATE(g12a_assist_misc,		HHI_GCLK_MPEG0,	23);
static MESON_GATE(g12a_emmc_a,			HHI_GCLK_MPEG0,	4);
static MESON_GATE(g12a_emmc_b,			HHI_GCLK_MPEG0,	25);
static MESON_GATE(g12a_emmc_c,			HHI_GCLK_MPEG0,	26);
static MESON_GATE(g12a_audio_codec,		HHI_GCLK_MPEG0,	28);

static MESON_GATE(g12a_audio,			HHI_GCLK_MPEG1,	0);
static MESON_GATE(g12a_eth_core,		HHI_GCLK_MPEG1,	3);
static MESON_GATE(g12a_demux,			HHI_GCLK_MPEG1,	4);
static MESON_GATE(g12a_audio_ififo,		HHI_GCLK_MPEG1,	11);
static MESON_GATE(g12a_adc,			HHI_GCLK_MPEG1,	13);
static MESON_GATE(g12a_uart1,			HHI_GCLK_MPEG1,	16);
#ifdef CONFIG_AMLOGIC_MODIFY
static MESON_GATE(g12b_csi_dig,			HHI_GCLK_MPEG1, 18);
static MESON_GATE(sm1_csi_dig,			HHI_GCLK_MPEG1, 18);
static MESON_GATE(g12b_vipnanoq,		HHI_GCLK_MPEG1, 19);
static MESON_GATE(sm1_nna,			HHI_GCLK_MPEG1, 19);
static MESON_GATE(g12a_ge2d,			HHI_GCLK_MPEG1,	20);
#else
static MESON_GATE(g12a_g2d,			HHI_GCLK_MPEG1,	20);
#endif
static MESON_GATE(g12a_reset,			HHI_GCLK_MPEG1,	23);
static MESON_GATE(g12a_pcie_comb,		HHI_GCLK_MPEG1,	24);
static MESON_GATE(g12a_parser,			HHI_GCLK_MPEG1,	25);
static MESON_GATE(g12a_usb_general,		HHI_GCLK_MPEG1,	26);
static MESON_GATE(g12a_pcie_phy,		HHI_GCLK_MPEG1,	27);
#ifdef CONFIG_AMLOGIC_MODIFY
static MESON_GATE(sm1_parser1,			HHI_GCLK_MPEG1, 28);
#endif
static MESON_GATE(g12a_ahb_arb0,		HHI_GCLK_MPEG1,	29);

static MESON_GATE(g12a_ahb_data_bus,		HHI_GCLK_MPEG2,	1);
static MESON_GATE(g12a_ahb_ctrl_bus,		HHI_GCLK_MPEG2,	2);
static MESON_GATE(g12a_htx_hdcp22,		HHI_GCLK_MPEG2,	3);
static MESON_GATE(g12a_htx_pclk,		HHI_GCLK_MPEG2,	4);
static MESON_GATE(g12a_bt656,			HHI_GCLK_MPEG2,	6);
static MESON_GATE(g12a_usb1_to_ddr,		HHI_GCLK_MPEG2,	8);
static MESON_GATE(g12a_mmc_pclk,		HHI_GCLK_MPEG2,	11);
static MESON_GATE(g12a_uart2,			HHI_GCLK_MPEG2,	15);
#ifdef CONFIG_AMLOGIC_MODIFY
static MESON_GATE(sm1_csi_host,			HHI_GCLK_MPEG2, 16);
static MESON_GATE(sm1_csi_adpat,		HHI_GCLK_MPEG2, 17);
static MESON_GATE(g12b_gdc,			HHI_GCLK_MPEG2, 16);
static MESON_GATE(g12b_mipi_isp,		HHI_GCLK_MPEG2, 17);
static MESON_GATE(sm1_temp_sensor,		HHI_GCLK_MPEG2, 22);
#endif
static MESON_GATE(g12a_vpu_intr,		HHI_GCLK_MPEG2,	25);
#ifdef CONFIG_AMLOGIC_MODIFY
static MESON_GATE(g12b_csi2_phy1,		HHI_GCLK_MPEG2, 28);
static MESON_GATE(g12b_csi2_phy0,		HHI_GCLK_MPEG2, 29);
static MESON_GATE(sm1_csi_phy,			HHI_GCLK_MPEG2, 29);
#endif
static MESON_GATE(g12a_gic,			HHI_GCLK_MPEG2,	30);

static MESON_GATE(g12a_vclk2_venci0,		HHI_GCLK_OTHER,	1);
static MESON_GATE(g12a_vclk2_venci1,		HHI_GCLK_OTHER,	2);
static MESON_GATE(g12a_vclk2_vencp0,		HHI_GCLK_OTHER,	3);
static MESON_GATE(g12a_vclk2_vencp1,		HHI_GCLK_OTHER,	4);
static MESON_GATE(g12a_vclk2_venct0,		HHI_GCLK_OTHER,	5);
static MESON_GATE(g12a_vclk2_venct1,		HHI_GCLK_OTHER,	6);
static MESON_GATE(g12a_vclk2_other,		HHI_GCLK_OTHER,	7);
static MESON_GATE(g12a_vclk2_enci,		HHI_GCLK_OTHER,	8);
static MESON_GATE(g12a_vclk2_encp,		HHI_GCLK_OTHER,	9);
static MESON_GATE(g12a_dac_clk,			HHI_GCLK_OTHER,	10);
static MESON_GATE(g12a_aoclk_gate,		HHI_GCLK_OTHER,	14);
static MESON_GATE(g12a_iec958_gate,		HHI_GCLK_OTHER,	16);
static MESON_GATE(g12a_enc480p,			HHI_GCLK_OTHER,	20);
static MESON_GATE(g12a_rng1,			HHI_GCLK_OTHER,	21);
static MESON_GATE(g12a_vclk2_enct,		HHI_GCLK_OTHER,	22);
static MESON_GATE(g12a_vclk2_encl,		HHI_GCLK_OTHER,	23);
static MESON_GATE(g12a_vclk2_venclmmc,		HHI_GCLK_OTHER,	24);
static MESON_GATE(g12a_vclk2_vencl,		HHI_GCLK_OTHER,	25);
static MESON_GATE(g12a_vclk2_other1,		HHI_GCLK_OTHER,	26);

static MESON_GATE_RO(g12a_dma,			HHI_GCLK_OTHER2, 0);
static MESON_GATE_RO(g12a_efuse,		HHI_GCLK_OTHER2, 1);
static MESON_GATE_RO(g12a_rom_boot,		HHI_GCLK_OTHER2, 2);
static MESON_GATE_RO(g12a_reset_sec,		HHI_GCLK_OTHER2, 3);
static MESON_GATE_RO(g12a_sec_ahb_apb3,		HHI_GCLK_OTHER2, 4);

/* Array of all clocks provided by this provider */
static struct clk_hw_onecell_data g12a_hw_onecell_data = {
	.hws = {
		[CLKID_SYS_PLL]			= &g12a_sys_pll.hw,
		[CLKID_FIXED_PLL]		= &g12a_fixed_pll.hw,
		[CLKID_FCLK_DIV2]		= &g12a_fclk_div2.hw,
		[CLKID_FCLK_DIV3]		= &g12a_fclk_div3.hw,
		[CLKID_FCLK_DIV4]		= &g12a_fclk_div4.hw,
		[CLKID_FCLK_DIV5]		= &g12a_fclk_div5.hw,
		[CLKID_FCLK_DIV7]		= &g12a_fclk_div7.hw,
		[CLKID_FCLK_DIV2P5]		= &g12a_fclk_div2p5.hw,
		[CLKID_GP0_PLL]			= &g12a_gp0_pll.hw,
		[CLKID_MPEG_SEL]		= &g12a_mpeg_clk_sel.hw,
		[CLKID_MPEG_DIV]		= &g12a_mpeg_clk_div.hw,
		[CLKID_CLK81]			= &g12a_clk81.hw,
		[CLKID_MPLL0]			= &g12a_mpll0.hw,
		[CLKID_MPLL1]			= &g12a_mpll1.hw,
		[CLKID_MPLL2]			= &g12a_mpll2.hw,
		[CLKID_MPLL3]			= &g12a_mpll3.hw,
		[CLKID_DDR]			= &g12a_ddr.hw,
		[CLKID_DOS]			= &g12a_dos.hw,
		[CLKID_AUDIO_LOCKER]		= &g12a_audio_locker.hw,
		[CLKID_MIPI_DSI_HOST]		= &g12a_mipi_dsi_host.hw,
		[CLKID_ETH_PHY]			= &g12a_eth_phy.hw,
		[CLKID_ISA]			= &g12a_isa.hw,
		[CLKID_PL301]			= &g12a_pl301.hw,
		[CLKID_PERIPHS]			= &g12a_periphs.hw,
		[CLKID_SPICC0]			= &g12a_spicc_0.hw,
		[CLKID_I2C]			= &g12a_i2c.hw,
		[CLKID_SANA]			= &g12a_sana.hw,
		[CLKID_SD]			= &g12a_sd.hw,
		[CLKID_RNG0]			= &g12a_rng0.hw,
		[CLKID_UART0]			= &g12a_uart0.hw,
		[CLKID_SPICC1]			= &g12a_spicc_1.hw,
		[CLKID_HIU_IFACE]		= &g12a_hiu_reg.hw,
		[CLKID_MIPI_DSI_PHY]		= &g12a_mipi_dsi_phy.hw,
		[CLKID_ASSIST_MISC]		= &g12a_assist_misc.hw,
		[CLKID_SD_EMMC_A]		= &g12a_emmc_a.hw,
		[CLKID_SD_EMMC_B]		= &g12a_emmc_b.hw,
		[CLKID_SD_EMMC_C]		= &g12a_emmc_c.hw,
		[CLKID_AUDIO_CODEC]		= &g12a_audio_codec.hw,
		[CLKID_AUDIO]			= &g12a_audio.hw,
		[CLKID_ETH]			= &g12a_eth_core.hw,
		[CLKID_DEMUX]			= &g12a_demux.hw,
		[CLKID_AUDIO_IFIFO]		= &g12a_audio_ififo.hw,
		[CLKID_ADC]			= &g12a_adc.hw,
		[CLKID_UART1]			= &g12a_uart1.hw,
#ifdef CONFIG_AMLOGIC_MODIFY
		[CLKID_GE2D]			= &g12a_ge2d.hw,
#else
		[CLKID_G2D]			= &g12a_g2d.hw,
#endif
		[CLKID_RESET]			= &g12a_reset.hw,
		[CLKID_PCIE_COMB]		= &g12a_pcie_comb.hw,
		[CLKID_PARSER]			= &g12a_parser.hw,
		[CLKID_USB]			= &g12a_usb_general.hw,
		[CLKID_PCIE_PHY]		= &g12a_pcie_phy.hw,
		[CLKID_AHB_ARB0]		= &g12a_ahb_arb0.hw,
		[CLKID_AHB_DATA_BUS]		= &g12a_ahb_data_bus.hw,
		[CLKID_AHB_CTRL_BUS]		= &g12a_ahb_ctrl_bus.hw,
		[CLKID_HTX_HDCP22]		= &g12a_htx_hdcp22.hw,
		[CLKID_HTX_PCLK]		= &g12a_htx_pclk.hw,
		[CLKID_BT656]			= &g12a_bt656.hw,
		[CLKID_USB1_DDR_BRIDGE]		= &g12a_usb1_to_ddr.hw,
		[CLKID_MMC_PCLK]		= &g12a_mmc_pclk.hw,
		[CLKID_UART2]			= &g12a_uart2.hw,
		[CLKID_VPU_INTR]		= &g12a_vpu_intr.hw,
		[CLKID_GIC]			= &g12a_gic.hw,
		[CLKID_SD_EMMC_A_CLK0_SEL]	= &g12a_sd_emmc_a_clk0_sel.hw,
		[CLKID_SD_EMMC_A_CLK0_DIV]	= &g12a_sd_emmc_a_clk0_div.hw,
		[CLKID_SD_EMMC_A_CLK0]		= &g12a_sd_emmc_a_clk0.hw,
		[CLKID_SD_EMMC_B_CLK0_SEL]	= &g12a_sd_emmc_b_clk0_sel.hw,
		[CLKID_SD_EMMC_B_CLK0_DIV]	= &g12a_sd_emmc_b_clk0_div.hw,
		[CLKID_SD_EMMC_B_CLK0]		= &g12a_sd_emmc_b_clk0.hw,
		[CLKID_SD_EMMC_C_CLK0_SEL]	= &g12a_sd_emmc_c_clk0_sel.hw,
		[CLKID_SD_EMMC_C_CLK0_DIV]	= &g12a_sd_emmc_c_clk0_div.hw,
		[CLKID_SD_EMMC_C_CLK0]		= &g12a_sd_emmc_c_clk0.hw,
		[CLKID_MPLL0_DIV]		= &g12a_mpll0_div.hw,
		[CLKID_MPLL1_DIV]		= &g12a_mpll1_div.hw,
		[CLKID_MPLL2_DIV]		= &g12a_mpll2_div.hw,
		[CLKID_MPLL3_DIV]		= &g12a_mpll3_div.hw,
		[CLKID_FCLK_DIV2_DIV]		= &g12a_fclk_div2_div.hw,
		[CLKID_FCLK_DIV3_DIV]		= &g12a_fclk_div3_div.hw,
		[CLKID_FCLK_DIV4_DIV]		= &g12a_fclk_div4_div.hw,
		[CLKID_FCLK_DIV5_DIV]		= &g12a_fclk_div5_div.hw,
		[CLKID_FCLK_DIV7_DIV]		= &g12a_fclk_div7_div.hw,
		[CLKID_FCLK_DIV2P5_DIV]		= &g12a_fclk_div2p5_div.hw,
		[CLKID_HIFI_PLL]		= &g12a_hifi_pll.hw,
		[CLKID_VCLK2_VENCI0]		= &g12a_vclk2_venci0.hw,
		[CLKID_VCLK2_VENCI1]		= &g12a_vclk2_venci1.hw,
		[CLKID_VCLK2_VENCP0]		= &g12a_vclk2_vencp0.hw,
		[CLKID_VCLK2_VENCP1]		= &g12a_vclk2_vencp1.hw,
		[CLKID_VCLK2_VENCT0]		= &g12a_vclk2_venct0.hw,
		[CLKID_VCLK2_VENCT1]		= &g12a_vclk2_venct1.hw,
		[CLKID_VCLK2_OTHER]		= &g12a_vclk2_other.hw,
		[CLKID_VCLK2_ENCI]		= &g12a_vclk2_enci.hw,
		[CLKID_VCLK2_ENCP]		= &g12a_vclk2_encp.hw,
		[CLKID_DAC_CLK]			= &g12a_dac_clk.hw,
		[CLKID_AOCLK]			= &g12a_aoclk_gate.hw,
		[CLKID_IEC958]			= &g12a_iec958_gate.hw,
		[CLKID_ENC480P]			= &g12a_enc480p.hw,
		[CLKID_RNG1]			= &g12a_rng1.hw,
		[CLKID_VCLK2_ENCT]		= &g12a_vclk2_enct.hw,
		[CLKID_VCLK2_ENCL]		= &g12a_vclk2_encl.hw,
		[CLKID_VCLK2_VENCLMMC]		= &g12a_vclk2_venclmmc.hw,
		[CLKID_VCLK2_VENCL]		= &g12a_vclk2_vencl.hw,
		[CLKID_VCLK2_OTHER1]		= &g12a_vclk2_other1.hw,
		[CLKID_FIXED_PLL_DCO]		= &g12a_fixed_pll_dco.hw,
		[CLKID_SYS_PLL_DCO]		= &g12a_sys_pll_dco.hw,
		[CLKID_GP0_PLL_DCO]		= &g12a_gp0_pll_dco.hw,
		[CLKID_HIFI_PLL_DCO]		= &g12a_hifi_pll_dco.hw,
		[CLKID_DMA]			= &g12a_dma.hw,
		[CLKID_EFUSE]			= &g12a_efuse.hw,
		[CLKID_ROM_BOOT]		= &g12a_rom_boot.hw,
		[CLKID_RESET_SEC]		= &g12a_reset_sec.hw,
		[CLKID_SEC_AHB_APB3]		= &g12a_sec_ahb_apb3.hw,
		[CLKID_MPLL_PREDIV]		= &g12a_mpll_prediv.hw,
		[CLKID_VPU_0_SEL]		= &g12a_vpu_0_sel.hw,
		[CLKID_VPU_0_DIV]		= &g12a_vpu_0_div.hw,
		[CLKID_VPU_0]			= &g12a_vpu_0.hw,
		[CLKID_VPU_1_SEL]		= &g12a_vpu_1_sel.hw,
		[CLKID_VPU_1_DIV]		= &g12a_vpu_1_div.hw,
		[CLKID_VPU_1]			= &g12a_vpu_1.hw,
		[CLKID_VPU]			= &g12a_vpu.hw,
		[CLKID_VAPB_0_SEL]		= &g12a_vapb_0_sel.hw,
		[CLKID_VAPB_0_DIV]		= &g12a_vapb_0_div.hw,
		[CLKID_VAPB_0]			= &g12a_vapb_0.hw,
		[CLKID_VAPB_1_SEL]		= &g12a_vapb_1_sel.hw,
		[CLKID_VAPB_1_DIV]		= &g12a_vapb_1_div.hw,
		[CLKID_VAPB_1]			= &g12a_vapb_1.hw,
		[CLKID_VAPB_SEL]		= &g12a_vapb_sel.hw,
#ifdef CONFIG_AMLOGIC_MODIFY
		[CLKID_GE2D_GATE]		= &g12a_ge2d_gate.hw,
#else
		[CLKID_VAPB]			= &g12a_vapb.hw,
#endif
		[CLKID_HDMI_PLL_DCO]		= &g12a_hdmi_pll_dco.hw,
		[CLKID_HDMI_PLL_OD]		= &g12a_hdmi_pll_od.hw,
		[CLKID_HDMI_PLL_OD2]		= &g12a_hdmi_pll_od2.hw,
		[CLKID_HDMI_PLL]		= &g12a_hdmi_pll.hw,
		[CLKID_VID_PLL]			= &g12a_vid_pll_div.hw,
		[CLKID_VID_PLL_SEL]		= &g12a_vid_pll_sel.hw,
		[CLKID_VID_PLL_DIV]		= &g12a_vid_pll.hw,
		[CLKID_VCLK_SEL]		= &g12a_vclk_sel.hw,
		[CLKID_VCLK2_SEL]		= &g12a_vclk2_sel.hw,
		[CLKID_VCLK_INPUT]		= &g12a_vclk_input.hw,
		[CLKID_VCLK2_INPUT]		= &g12a_vclk2_input.hw,
		[CLKID_VCLK_DIV]		= &g12a_vclk_div.hw,
		[CLKID_VCLK2_DIV]		= &g12a_vclk2_div.hw,
		[CLKID_VCLK]			= &g12a_vclk.hw,
		[CLKID_VCLK2]			= &g12a_vclk2.hw,
		[CLKID_VCLK_DIV1]		= &g12a_vclk_div1.hw,
		[CLKID_VCLK_DIV2_EN]		= &g12a_vclk_div2_en.hw,
		[CLKID_VCLK_DIV4_EN]		= &g12a_vclk_div4_en.hw,
		[CLKID_VCLK_DIV6_EN]		= &g12a_vclk_div6_en.hw,
		[CLKID_VCLK_DIV12_EN]		= &g12a_vclk_div12_en.hw,
		[CLKID_VCLK2_DIV1]		= &g12a_vclk2_div1.hw,
		[CLKID_VCLK2_DIV2_EN]		= &g12a_vclk2_div2_en.hw,
		[CLKID_VCLK2_DIV4_EN]		= &g12a_vclk2_div4_en.hw,
		[CLKID_VCLK2_DIV6_EN]		= &g12a_vclk2_div6_en.hw,
		[CLKID_VCLK2_DIV12_EN]		= &g12a_vclk2_div12_en.hw,
		[CLKID_VCLK_DIV2]		= &g12a_vclk_div2.hw,
		[CLKID_VCLK_DIV4]		= &g12a_vclk_div4.hw,
		[CLKID_VCLK_DIV6]		= &g12a_vclk_div6.hw,
		[CLKID_VCLK_DIV12]		= &g12a_vclk_div12.hw,
		[CLKID_VCLK2_DIV2]		= &g12a_vclk2_div2.hw,
		[CLKID_VCLK2_DIV4]		= &g12a_vclk2_div4.hw,
		[CLKID_VCLK2_DIV6]		= &g12a_vclk2_div6.hw,
		[CLKID_VCLK2_DIV12]		= &g12a_vclk2_div12.hw,
		[CLKID_CTS_ENCI_SEL]		= &g12a_cts_enci_sel.hw,
		[CLKID_CTS_ENCP_SEL]		= &g12a_cts_encp_sel.hw,
		[CLKID_CTS_VDAC_SEL]		= &g12a_cts_vdac_sel.hw,
		[CLKID_HDMI_TX_SEL]		= &g12a_hdmi_tx_sel.hw,
		[CLKID_CTS_ENCI]		= &g12a_cts_enci.hw,
		[CLKID_CTS_ENCP]		= &g12a_cts_encp.hw,
		[CLKID_CTS_VDAC]		= &g12a_cts_vdac.hw,
		[CLKID_HDMI_TX]			= &g12a_hdmi_tx.hw,
		[CLKID_HDMI_SEL]		= &g12a_hdmi_sel.hw,
		[CLKID_HDMI_DIV]		= &g12a_hdmi_div.hw,
		[CLKID_HDMI]			= &g12a_hdmi.hw,
		[CLKID_MALI_0_SEL]		= &g12a_mali_0_sel.hw,
		[CLKID_MALI_0_DIV]		= &g12a_mali_0_div.hw,
		[CLKID_MALI_0]			= &g12a_mali_0.hw,
		[CLKID_MALI_1_SEL]		= &g12a_mali_1_sel.hw,
		[CLKID_MALI_1_DIV]		= &g12a_mali_1_div.hw,
		[CLKID_MALI_1]			= &g12a_mali_1.hw,
		[CLKID_MALI]			= &g12a_mali.hw,
		[CLKID_MPLL_50M_DIV]		= &g12a_mpll_50m_div.hw,
		[CLKID_MPLL_50M]		= &g12a_mpll_50m.hw,
		[CLKID_SYS_PLL_DIV16_EN]	= &g12a_sys_pll_div16_en.hw,
		[CLKID_SYS_PLL_DIV16]		= &g12a_sys_pll_div16.hw,
		[CLKID_CPU_CLK_DYN0_SEL]	= &g12a_cpu_clk_premux0.hw,
		[CLKID_CPU_CLK_DYN0_DIV]	= &g12a_cpu_clk_mux0_div.hw,
		[CLKID_CPU_CLK_DYN0]		= &g12a_cpu_clk_postmux0.hw,
		[CLKID_CPU_CLK_DYN1_SEL]	= &g12a_cpu_clk_premux1.hw,
		[CLKID_CPU_CLK_DYN1_DIV]	= &g12a_cpu_clk_mux1_div.hw,
		[CLKID_CPU_CLK_DYN1]		= &g12a_cpu_clk_postmux1.hw,
		[CLKID_CPU_CLK_DYN]		= &g12a_cpu_clk_dyn.hw,
		[CLKID_CPU_CLK]			= &g12a_cpu_clk.hw,
		[CLKID_CPU_CLK_DIV16_EN]	= &g12a_cpu_clk_div16_en.hw,
		[CLKID_CPU_CLK_DIV16]		= &g12a_cpu_clk_div16.hw,
		[CLKID_CPU_CLK_APB_DIV]		= &g12a_cpu_clk_apb_div.hw,
		[CLKID_CPU_CLK_APB]		= &g12a_cpu_clk_apb.hw,
		[CLKID_CPU_CLK_ATB_DIV]		= &g12a_cpu_clk_atb_div.hw,
		[CLKID_CPU_CLK_ATB]		= &g12a_cpu_clk_atb.hw,
		[CLKID_CPU_CLK_AXI_DIV]		= &g12a_cpu_clk_axi_div.hw,
		[CLKID_CPU_CLK_AXI]		= &g12a_cpu_clk_axi.hw,
		[CLKID_CPU_CLK_TRACE_DIV]	= &g12a_cpu_clk_trace_div.hw,
		[CLKID_CPU_CLK_TRACE]		= &g12a_cpu_clk_trace.hw,
		[CLKID_PCIE_PLL_DCO]		= &g12a_pcie_pll_dco.hw,
		[CLKID_PCIE_PLL_DCO_DIV2]	= &g12a_pcie_pll_dco_div2.hw,
		[CLKID_PCIE_PLL_OD]		= &g12a_pcie_pll_od.hw,
		[CLKID_PCIE_PLL]		= &g12a_pcie_pll.hw,
		[CLKID_PCIE_BGP]		= &g12a_pcie_bgp.hw,
		[CLKID_PCIE_HCSL]		= &g12a_pcie_hcsl.hw,
		[CLKID_VDEC_1_SEL]		= &g12a_vdec_1_sel.hw,
		[CLKID_VDEC_1_DIV]		= &g12a_vdec_1_div.hw,
		[CLKID_VDEC_1]			= &g12a_vdec_1.hw,
		[CLKID_VDEC_HEVC_SEL]		= &g12a_vdec_hevc_sel.hw,
		[CLKID_VDEC_HEVC_DIV]		= &g12a_vdec_hevc_div.hw,
		[CLKID_VDEC_HEVC]		= &g12a_vdec_hevc.hw,
		[CLKID_VDEC_HEVCF_SEL]		= &g12a_vdec_hevcf_sel.hw,
		[CLKID_VDEC_HEVCF_DIV]		= &g12a_vdec_hevcf_div.hw,
		[CLKID_VDEC_HEVCF]		= &g12a_vdec_hevcf.hw,
		[CLKID_TS_DIV]			= &g12a_ts_div.hw,
		[CLKID_TS]			= &g12a_ts.hw,
		[CLKID_SPICC0_MUX]		= &g12a_spicc0_mux.hw,
		[CLKID_SPICC0_DIV]		= &g12a_spicc0_div.hw,
		[CLKID_SPICC0_GATE]		= &g12a_spicc0_gate.hw,
		[CLKID_SPICC1_MUX]		= &g12a_spicc1_mux.hw,
		[CLKID_SPICC1_DIV]		= &g12a_spicc1_div.hw,
		[CLKID_SPICC1_GATE]		= &g12a_spicc1_gate.hw,
		[CLKID_WAVE_A_SEL]		= &g12a_wave_a_sel.hw,
		[CLKID_WAVE_A_DIV]		= &g12a_wave_a_div.hw,
		[CLKID_WAVE_A_CLK]		= &g12a_wave_aclk.hw,
		[CLKID_WAVE_B_SEL]		= &g12a_wave_b_sel.hw,
		[CLKID_WAVE_B_DIV]		= &g12a_wave_b_div.hw,
		[CLKID_WAVE_B_CLK]		= &g12a_wave_bclk.hw,
		[CLKID_WAVE_C_SEL]		= &g12a_wave_c_sel.hw,
		[CLKID_WAVE_C_DIV]		= &g12a_wave_c_div.hw,
		[CLKID_WAVE_C_CLK]		= &g12a_wave_cclk.hw,
		[CLKID_BT656_MUX]		= &g12a_bt656_mux.hw,
		[CLKID_BT656_DIV]		= &g12a_bt656_div.hw,
		[CLKID_BT656_GATE]		= &g12a_bt656_gate.hw,
#ifdef CONFIG_AMLOGIC_MODIFY
		[CLKID_25M_CLK_DIV]		= &g12a_25m_div.hw,
		[CLKID_25M_CLK_GATE]		= &g12a_25m_gate.hw,
		[CLKID_24M_GATE]		= &g12a_24m_gate.hw,
		[CLKID_12M_24M]			= &g12a_12m_24m_div.hw,
		[CLKID_GEN_MUX]			= &g12a_gen_clk_sel.hw,
		[CLKID_GEN_DIV]			= &g12a_gen_clk_div.hw,
		[CLKID_GEN_CLK]			= &g12a_gen_clk.hw,
		[CLKID_DSI_MEAS_MUX]		= &g12a_dsi_meas_mux.hw,
		[CLKID_DSI_MEAS_DIV]		= &g12a_dsi_meas_div.hw,
		[CLKID_DSI_MEAS]		= &g12a_dsi_meas.hw,
		[CLKID_VDEC_P1_MUX]		= &g12a_vdec_p1_mux.hw,
		[CLKID_VDEC_P1_DIV]		= &g12a_vdec_p1_div.hw,
		[CLKID_VDEC_P1]			= &g12a_vdec_p1.hw,
		[CLKID_VDEC_MUX]		= &g12a_vdec_mux.hw,
		[CLKID_HCODEC_P0_MUX]		= &g12a_hcodec_p0_mux.hw,
		[CLKID_HCODEC_P0_DIV]		= &g12a_hcodec_p0_div.hw,
		[CLKID_HCODEC_P0]		= &g12a_hcodec_p0.hw,
		[CLKID_HCODEC_P1_MUX]		= &g12a_hcodec_p1_mux.hw,
		[CLKID_HCODEC_P1_DIV]		= &g12a_hcodec_p1_div.hw,
		[CLKID_HCODEC_P1]		= &g12a_hcodec_p1.hw,
		[CLKID_HCODEC_MUX]		= &g12a_hcodec_mux.hw,
		[CLKID_HEVC_P1_MUX]		= &g12a_hevc_p1_mux.hw,
		[CLKID_HEVC_P1_DIV]		= &g12a_hevc_p1_div.hw,
		[CLKID_HEVC_P1]			= &g12a_hevc_p1.hw,
		[CLKID_HEVC_MUX]		= &g12a_hevc_mux.hw,
		[CLKID_HEVCF_P1_MUX]		= &g12a_hevcf_p1_mux.hw,
		[CLKID_HEVCF_P1_DIV]		= &g12a_hevcf_p1_div.hw,
		[CLKID_HEVCF_P1]		= &g12a_hevcf_p1.hw,
		[CLKID_HEVCF_MUX]		= &g12a_hevcf_mux.hw,
		[CLKID_VPU_CLKB_TMP_MUX]	= &g12a_vpu_clkb_tmp_mux.hw,
		[CLKID_VPU_CLKB_TMP_DIV]	= &g12a_vpu_clkb_tmp_div.hw,
		[CLKID_VPU_CLKB_TMP]		= &g12a_vpu_clkb_tmp.hw,
		[CLKID_VPU_CLKB_DIV]		= &g12a_vpu_clkb_div.hw,
		[CLKID_VPU_CLKB]		= &g12a_vpu_clkb.hw,
		[CLKID_VPU_CLKC_P0_MUX]		= &g12a_vpu_clkc_p0_mux.hw,
		[CLKID_VPU_CLKC_P0_DIV]		= &g12a_vpu_clkc_p0_div.hw,
		[CLKID_VPU_CLKC_P0]		= &g12a_vpu_clkc_p0.hw,
		[CLKID_VPU_CLKC_P1_MUX]		= &g12a_vpu_clkc_p1_mux.hw,
		[CLKID_VPU_CLKC_P1_DIV]		= &g12a_vpu_clkc_p1_div.hw,
		[CLKID_VPU_CLKC_P1]		= &g12a_vpu_clkc_p1.hw,
		[CLKID_VPU_CLKC_MUX]		= &g12a_vpu_clkc_mux.hw,
#endif  /* CONFIG_AMLOGIC_MODIFY */
		[NR_CLKS]			= NULL,
	},
	.num = NR_CLKS,
};

static struct clk_hw_onecell_data g12b_hw_onecell_data = {
	.hws = {
		[CLKID_SYS_PLL]			= &g12a_sys_pll.hw,
		[CLKID_FIXED_PLL]		= &g12a_fixed_pll.hw,
		[CLKID_FCLK_DIV2]		= &g12a_fclk_div2.hw,
		[CLKID_FCLK_DIV3]		= &g12a_fclk_div3.hw,
		[CLKID_FCLK_DIV4]		= &g12a_fclk_div4.hw,
		[CLKID_FCLK_DIV5]		= &g12a_fclk_div5.hw,
		[CLKID_FCLK_DIV7]		= &g12a_fclk_div7.hw,
		[CLKID_FCLK_DIV2P5]		= &g12a_fclk_div2p5.hw,
		[CLKID_GP0_PLL]			= &g12a_gp0_pll.hw,
		[CLKID_MPEG_SEL]		= &g12a_mpeg_clk_sel.hw,
		[CLKID_MPEG_DIV]		= &g12a_mpeg_clk_div.hw,
		[CLKID_CLK81]			= &g12a_clk81.hw,
		[CLKID_MPLL0]			= &g12a_mpll0.hw,
		[CLKID_MPLL1]			= &g12a_mpll1.hw,
		[CLKID_MPLL2]			= &g12a_mpll2.hw,
		[CLKID_MPLL3]			= &g12a_mpll3.hw,
		[CLKID_DDR]			= &g12a_ddr.hw,
		[CLKID_DOS]			= &g12a_dos.hw,
		[CLKID_AUDIO_LOCKER]		= &g12a_audio_locker.hw,
		[CLKID_MIPI_DSI_HOST]		= &g12a_mipi_dsi_host.hw,
		[CLKID_ETH_PHY]			= &g12a_eth_phy.hw,
		[CLKID_ISA]			= &g12a_isa.hw,
		[CLKID_PL301]			= &g12a_pl301.hw,
		[CLKID_PERIPHS]			= &g12a_periphs.hw,
		[CLKID_SPICC0]			= &g12a_spicc_0.hw,
		[CLKID_I2C]			= &g12a_i2c.hw,
		[CLKID_SANA]			= &g12a_sana.hw,
		[CLKID_SD]			= &g12a_sd.hw,
		[CLKID_RNG0]			= &g12a_rng0.hw,
		[CLKID_UART0]			= &g12a_uart0.hw,
		[CLKID_SPICC1]			= &g12a_spicc_1.hw,
		[CLKID_HIU_IFACE]		= &g12a_hiu_reg.hw,
		[CLKID_MIPI_DSI_PHY]		= &g12a_mipi_dsi_phy.hw,
		[CLKID_ASSIST_MISC]		= &g12a_assist_misc.hw,
		[CLKID_SD_EMMC_A]		= &g12a_emmc_a.hw,
		[CLKID_SD_EMMC_B]		= &g12a_emmc_b.hw,
		[CLKID_SD_EMMC_C]		= &g12a_emmc_c.hw,
		[CLKID_AUDIO_CODEC]		= &g12a_audio_codec.hw,
		[CLKID_AUDIO]			= &g12a_audio.hw,
		[CLKID_ETH]			= &g12a_eth_core.hw,
		[CLKID_DEMUX]			= &g12a_demux.hw,
		[CLKID_AUDIO_IFIFO]		= &g12a_audio_ififo.hw,
		[CLKID_ADC]			= &g12a_adc.hw,
		[CLKID_UART1]			= &g12a_uart1.hw,
#ifdef CONFIG_AMLOGIC_MODIFY
		[CLKID_GE2D]			= &g12a_ge2d.hw,
#else
		[CLKID_G2D]			= &g12a_g2d.hw,
#endif
		[CLKID_RESET]			= &g12a_reset.hw,
		[CLKID_PCIE_COMB]		= &g12a_pcie_comb.hw,
		[CLKID_PARSER]			= &g12a_parser.hw,
		[CLKID_USB]			= &g12a_usb_general.hw,
		[CLKID_PCIE_PHY]		= &g12a_pcie_phy.hw,
		[CLKID_AHB_ARB0]		= &g12a_ahb_arb0.hw,
		[CLKID_AHB_DATA_BUS]		= &g12a_ahb_data_bus.hw,
		[CLKID_AHB_CTRL_BUS]		= &g12a_ahb_ctrl_bus.hw,
		[CLKID_HTX_HDCP22]		= &g12a_htx_hdcp22.hw,
		[CLKID_HTX_PCLK]		= &g12a_htx_pclk.hw,
		[CLKID_BT656]			= &g12a_bt656.hw,
		[CLKID_USB1_DDR_BRIDGE]		= &g12a_usb1_to_ddr.hw,
		[CLKID_MMC_PCLK]		= &g12a_mmc_pclk.hw,
		[CLKID_UART2]			= &g12a_uart2.hw,
		[CLKID_VPU_INTR]		= &g12a_vpu_intr.hw,
		[CLKID_GIC]			= &g12a_gic.hw,
		[CLKID_SD_EMMC_A_CLK0_SEL]	= &g12a_sd_emmc_a_clk0_sel.hw,
		[CLKID_SD_EMMC_A_CLK0_DIV]	= &g12a_sd_emmc_a_clk0_div.hw,
		[CLKID_SD_EMMC_A_CLK0]		= &g12a_sd_emmc_a_clk0.hw,
		[CLKID_SD_EMMC_B_CLK0_SEL]	= &g12a_sd_emmc_b_clk0_sel.hw,
		[CLKID_SD_EMMC_B_CLK0_DIV]	= &g12a_sd_emmc_b_clk0_div.hw,
		[CLKID_SD_EMMC_B_CLK0]		= &g12a_sd_emmc_b_clk0.hw,
		[CLKID_SD_EMMC_C_CLK0_SEL]	= &g12a_sd_emmc_c_clk0_sel.hw,
		[CLKID_SD_EMMC_C_CLK0_DIV]	= &g12a_sd_emmc_c_clk0_div.hw,
		[CLKID_SD_EMMC_C_CLK0]		= &g12a_sd_emmc_c_clk0.hw,
		[CLKID_MPLL0_DIV]		= &g12a_mpll0_div.hw,
		[CLKID_MPLL1_DIV]		= &g12a_mpll1_div.hw,
		[CLKID_MPLL2_DIV]		= &g12a_mpll2_div.hw,
		[CLKID_MPLL3_DIV]		= &g12a_mpll3_div.hw,
		[CLKID_FCLK_DIV2_DIV]		= &g12a_fclk_div2_div.hw,
		[CLKID_FCLK_DIV3_DIV]		= &g12a_fclk_div3_div.hw,
		[CLKID_FCLK_DIV4_DIV]		= &g12a_fclk_div4_div.hw,
		[CLKID_FCLK_DIV5_DIV]		= &g12a_fclk_div5_div.hw,
		[CLKID_FCLK_DIV7_DIV]		= &g12a_fclk_div7_div.hw,
		[CLKID_FCLK_DIV2P5_DIV]		= &g12a_fclk_div2p5_div.hw,
		[CLKID_HIFI_PLL]		= &g12a_hifi_pll.hw,
		[CLKID_VCLK2_VENCI0]		= &g12a_vclk2_venci0.hw,
		[CLKID_VCLK2_VENCI1]		= &g12a_vclk2_venci1.hw,
		[CLKID_VCLK2_VENCP0]		= &g12a_vclk2_vencp0.hw,
		[CLKID_VCLK2_VENCP1]		= &g12a_vclk2_vencp1.hw,
		[CLKID_VCLK2_VENCT0]		= &g12a_vclk2_venct0.hw,
		[CLKID_VCLK2_VENCT1]		= &g12a_vclk2_venct1.hw,
		[CLKID_VCLK2_OTHER]		= &g12a_vclk2_other.hw,
		[CLKID_VCLK2_ENCI]		= &g12a_vclk2_enci.hw,
		[CLKID_VCLK2_ENCP]		= &g12a_vclk2_encp.hw,
		[CLKID_DAC_CLK]			= &g12a_dac_clk.hw,
		[CLKID_AOCLK]			= &g12a_aoclk_gate.hw,
		[CLKID_IEC958]			= &g12a_iec958_gate.hw,
		[CLKID_ENC480P]			= &g12a_enc480p.hw,
		[CLKID_RNG1]			= &g12a_rng1.hw,
		[CLKID_VCLK2_ENCT]		= &g12a_vclk2_enct.hw,
		[CLKID_VCLK2_ENCL]		= &g12a_vclk2_encl.hw,
		[CLKID_VCLK2_VENCLMMC]		= &g12a_vclk2_venclmmc.hw,
		[CLKID_VCLK2_VENCL]		= &g12a_vclk2_vencl.hw,
		[CLKID_VCLK2_OTHER1]		= &g12a_vclk2_other1.hw,
		[CLKID_FIXED_PLL_DCO]		= &g12a_fixed_pll_dco.hw,
		[CLKID_SYS_PLL_DCO]		= &g12a_sys_pll_dco.hw,
		[CLKID_GP0_PLL_DCO]		= &g12a_gp0_pll_dco.hw,
		[CLKID_HIFI_PLL_DCO]		= &g12a_hifi_pll_dco.hw,
		[CLKID_DMA]			= &g12a_dma.hw,
		[CLKID_EFUSE]			= &g12a_efuse.hw,
		[CLKID_ROM_BOOT]		= &g12a_rom_boot.hw,
		[CLKID_RESET_SEC]		= &g12a_reset_sec.hw,
		[CLKID_SEC_AHB_APB3]		= &g12a_sec_ahb_apb3.hw,
		[CLKID_MPLL_PREDIV]		= &g12a_mpll_prediv.hw,
		[CLKID_VPU_0_SEL]		= &g12a_vpu_0_sel.hw,
		[CLKID_VPU_0_DIV]		= &g12a_vpu_0_div.hw,
		[CLKID_VPU_0]			= &g12a_vpu_0.hw,
		[CLKID_VPU_1_SEL]		= &g12a_vpu_1_sel.hw,
		[CLKID_VPU_1_DIV]		= &g12a_vpu_1_div.hw,
		[CLKID_VPU_1]			= &g12a_vpu_1.hw,
		[CLKID_VPU]			= &g12a_vpu.hw,
		[CLKID_VAPB_0_SEL]		= &g12a_vapb_0_sel.hw,
		[CLKID_VAPB_0_DIV]		= &g12a_vapb_0_div.hw,
		[CLKID_VAPB_0]			= &g12a_vapb_0.hw,
		[CLKID_VAPB_1_SEL]		= &g12a_vapb_1_sel.hw,
		[CLKID_VAPB_1_DIV]		= &g12a_vapb_1_div.hw,
		[CLKID_VAPB_1]			= &g12a_vapb_1.hw,
		[CLKID_VAPB_SEL]		= &g12a_vapb_sel.hw,
#ifdef CONFIG_AMLOGIC_MODIFY
		[CLKID_GE2D_GATE]		= &g12a_ge2d_gate.hw,
#else
		[CLKID_VAPB]			= &g12a_vapb.hw,
#endif
		[CLKID_HDMI_PLL_DCO]		= &g12a_hdmi_pll_dco.hw,
		[CLKID_HDMI_PLL_OD]		= &g12a_hdmi_pll_od.hw,
		[CLKID_HDMI_PLL_OD2]		= &g12a_hdmi_pll_od2.hw,
		[CLKID_HDMI_PLL]		= &g12a_hdmi_pll.hw,
		[CLKID_VID_PLL]			= &g12a_vid_pll_div.hw,
		[CLKID_VID_PLL_SEL]		= &g12a_vid_pll_sel.hw,
		[CLKID_VID_PLL_DIV]		= &g12a_vid_pll.hw,
		[CLKID_VCLK_SEL]		= &g12a_vclk_sel.hw,
		[CLKID_VCLK2_SEL]		= &g12a_vclk2_sel.hw,
		[CLKID_VCLK_INPUT]		= &g12a_vclk_input.hw,
		[CLKID_VCLK2_INPUT]		= &g12a_vclk2_input.hw,
		[CLKID_VCLK_DIV]		= &g12a_vclk_div.hw,
		[CLKID_VCLK2_DIV]		= &g12a_vclk2_div.hw,
		[CLKID_VCLK]			= &g12a_vclk.hw,
		[CLKID_VCLK2]			= &g12a_vclk2.hw,
		[CLKID_VCLK_DIV1]		= &g12a_vclk_div1.hw,
		[CLKID_VCLK_DIV2_EN]		= &g12a_vclk_div2_en.hw,
		[CLKID_VCLK_DIV4_EN]		= &g12a_vclk_div4_en.hw,
		[CLKID_VCLK_DIV6_EN]		= &g12a_vclk_div6_en.hw,
		[CLKID_VCLK_DIV12_EN]		= &g12a_vclk_div12_en.hw,
		[CLKID_VCLK2_DIV1]		= &g12a_vclk2_div1.hw,
		[CLKID_VCLK2_DIV2_EN]		= &g12a_vclk2_div2_en.hw,
		[CLKID_VCLK2_DIV4_EN]		= &g12a_vclk2_div4_en.hw,
		[CLKID_VCLK2_DIV6_EN]		= &g12a_vclk2_div6_en.hw,
		[CLKID_VCLK2_DIV12_EN]		= &g12a_vclk2_div12_en.hw,
		[CLKID_VCLK_DIV2]		= &g12a_vclk_div2.hw,
		[CLKID_VCLK_DIV4]		= &g12a_vclk_div4.hw,
		[CLKID_VCLK_DIV6]		= &g12a_vclk_div6.hw,
		[CLKID_VCLK_DIV12]		= &g12a_vclk_div12.hw,
		[CLKID_VCLK2_DIV2]		= &g12a_vclk2_div2.hw,
		[CLKID_VCLK2_DIV4]		= &g12a_vclk2_div4.hw,
		[CLKID_VCLK2_DIV6]		= &g12a_vclk2_div6.hw,
		[CLKID_VCLK2_DIV12]		= &g12a_vclk2_div12.hw,
		[CLKID_CTS_ENCI_SEL]		= &g12a_cts_enci_sel.hw,
		[CLKID_CTS_ENCP_SEL]		= &g12a_cts_encp_sel.hw,
		[CLKID_CTS_VDAC_SEL]		= &g12a_cts_vdac_sel.hw,
		[CLKID_HDMI_TX_SEL]		= &g12a_hdmi_tx_sel.hw,
		[CLKID_CTS_ENCI]		= &g12a_cts_enci.hw,
		[CLKID_CTS_ENCP]		= &g12a_cts_encp.hw,
		[CLKID_CTS_VDAC]		= &g12a_cts_vdac.hw,
		[CLKID_HDMI_TX]			= &g12a_hdmi_tx.hw,
		[CLKID_HDMI_SEL]		= &g12a_hdmi_sel.hw,
		[CLKID_HDMI_DIV]		= &g12a_hdmi_div.hw,
		[CLKID_HDMI]			= &g12a_hdmi.hw,
		[CLKID_MALI_0_SEL]		= &g12a_mali_0_sel.hw,
		[CLKID_MALI_0_DIV]		= &g12a_mali_0_div.hw,
		[CLKID_MALI_0]			= &g12a_mali_0.hw,
		[CLKID_MALI_1_SEL]		= &g12a_mali_1_sel.hw,
		[CLKID_MALI_1_DIV]		= &g12a_mali_1_div.hw,
		[CLKID_MALI_1]			= &g12a_mali_1.hw,
		[CLKID_MALI]			= &g12a_mali.hw,
		[CLKID_MPLL_50M_DIV]		= &g12a_mpll_50m_div.hw,
		[CLKID_MPLL_50M]		= &g12a_mpll_50m.hw,
		[CLKID_SYS_PLL_DIV16_EN]	= &g12a_sys_pll_div16_en.hw,
		[CLKID_SYS_PLL_DIV16]		= &g12a_sys_pll_div16.hw,
		[CLKID_CPU_CLK_DYN0_SEL]	= &g12a_cpu_clk_premux0.hw,
		[CLKID_CPU_CLK_DYN0_DIV]	= &g12a_cpu_clk_mux0_div.hw,
		[CLKID_CPU_CLK_DYN0]		= &g12a_cpu_clk_postmux0.hw,
		[CLKID_CPU_CLK_DYN1_SEL]	= &g12a_cpu_clk_premux1.hw,
		[CLKID_CPU_CLK_DYN1_DIV]	= &g12a_cpu_clk_mux1_div.hw,
		[CLKID_CPU_CLK_DYN1]		= &g12a_cpu_clk_postmux1.hw,
		[CLKID_CPU_CLK_DYN]		= &g12a_cpu_clk_dyn.hw,
		[CLKID_CPU_CLK]			= &g12b_cpu_clk.hw,
		[CLKID_CPU_CLK_DIV16_EN]	= &g12a_cpu_clk_div16_en.hw,
		[CLKID_CPU_CLK_DIV16]		= &g12a_cpu_clk_div16.hw,
		[CLKID_CPU_CLK_APB_DIV]		= &g12a_cpu_clk_apb_div.hw,
		[CLKID_CPU_CLK_APB]		= &g12a_cpu_clk_apb.hw,
		[CLKID_CPU_CLK_ATB_DIV]		= &g12a_cpu_clk_atb_div.hw,
		[CLKID_CPU_CLK_ATB]		= &g12a_cpu_clk_atb.hw,
		[CLKID_CPU_CLK_AXI_DIV]		= &g12a_cpu_clk_axi_div.hw,
		[CLKID_CPU_CLK_AXI]		= &g12a_cpu_clk_axi.hw,
		[CLKID_CPU_CLK_TRACE_DIV]	= &g12a_cpu_clk_trace_div.hw,
		[CLKID_CPU_CLK_TRACE]		= &g12a_cpu_clk_trace.hw,
		[CLKID_PCIE_PLL_DCO]		= &g12a_pcie_pll_dco.hw,
		[CLKID_PCIE_PLL_DCO_DIV2]	= &g12a_pcie_pll_dco_div2.hw,
		[CLKID_PCIE_PLL_OD]		= &g12a_pcie_pll_od.hw,
		[CLKID_PCIE_PLL]		= &g12a_pcie_pll.hw,
		[CLKID_PCIE_BGP]		= &g12a_pcie_bgp.hw,
		[CLKID_PCIE_HCSL]               = &g12a_pcie_hcsl.hw,
		[CLKID_VDEC_1_SEL]		= &g12a_vdec_1_sel.hw,
		[CLKID_VDEC_1_DIV]		= &g12a_vdec_1_div.hw,
		[CLKID_VDEC_1]			= &g12a_vdec_1.hw,
		[CLKID_VDEC_HEVC_SEL]		= &g12a_vdec_hevc_sel.hw,
		[CLKID_VDEC_HEVC_DIV]		= &g12a_vdec_hevc_div.hw,
		[CLKID_VDEC_HEVC]		= &g12a_vdec_hevc.hw,
		[CLKID_VDEC_HEVCF_SEL]		= &g12a_vdec_hevcf_sel.hw,
		[CLKID_VDEC_HEVCF_DIV]		= &g12a_vdec_hevcf_div.hw,
		[CLKID_VDEC_HEVCF]		= &g12a_vdec_hevcf.hw,
		[CLKID_TS_DIV]			= &g12a_ts_div.hw,
		[CLKID_TS]			= &g12a_ts.hw,
		[CLKID_SYS1_PLL_DCO]		= &g12b_sys1_pll_dco.hw,
		[CLKID_SYS1_PLL]		= &g12b_sys1_pll.hw,
		[CLKID_SYS1_PLL_DIV16_EN]	= &g12b_sys1_pll_div16_en.hw,
		[CLKID_SYS1_PLL_DIV16]		= &g12b_sys1_pll_div16.hw,
		[CLKID_CPUB_CLK_DYN0_SEL]	= &g12b_cpub_clk_premux0.hw,
		[CLKID_CPUB_CLK_DYN0_DIV]	= &g12b_cpub_clk_mux0_div.hw,
		[CLKID_CPUB_CLK_DYN0]		= &g12b_cpub_clk_postmux0.hw,
		[CLKID_CPUB_CLK_DYN1_SEL]	= &g12b_cpub_clk_premux1.hw,
		[CLKID_CPUB_CLK_DYN1_DIV]	= &g12b_cpub_clk_mux1_div.hw,
		[CLKID_CPUB_CLK_DYN1]		= &g12b_cpub_clk_postmux1.hw,
		[CLKID_CPUB_CLK_DYN]		= &g12b_cpub_clk_dyn.hw,
		[CLKID_CPUB_CLK]		= &g12b_cpub_clk.hw,
		[CLKID_CPUB_CLK_DIV16_EN]	= &g12b_cpub_clk_div16_en.hw,
		[CLKID_CPUB_CLK_DIV16]		= &g12b_cpub_clk_div16.hw,
		[CLKID_CPUB_CLK_DIV2]		= &g12b_cpub_clk_div2.hw,
		[CLKID_CPUB_CLK_DIV3]		= &g12b_cpub_clk_div3.hw,
		[CLKID_CPUB_CLK_DIV4]		= &g12b_cpub_clk_div4.hw,
		[CLKID_CPUB_CLK_DIV5]		= &g12b_cpub_clk_div5.hw,
		[CLKID_CPUB_CLK_DIV6]		= &g12b_cpub_clk_div6.hw,
		[CLKID_CPUB_CLK_DIV7]		= &g12b_cpub_clk_div7.hw,
		[CLKID_CPUB_CLK_DIV8]		= &g12b_cpub_clk_div8.hw,
		[CLKID_CPUB_CLK_APB_SEL]	= &g12b_cpub_clk_apb_sel.hw,
		[CLKID_CPUB_CLK_APB]		= &g12b_cpub_clk_apb.hw,
		[CLKID_CPUB_CLK_ATB_SEL]	= &g12b_cpub_clk_atb_sel.hw,
		[CLKID_CPUB_CLK_ATB]		= &g12b_cpub_clk_atb.hw,
		[CLKID_CPUB_CLK_AXI_SEL]	= &g12b_cpub_clk_axi_sel.hw,
		[CLKID_CPUB_CLK_AXI]		= &g12b_cpub_clk_axi.hw,
		[CLKID_CPUB_CLK_TRACE_SEL]	= &g12b_cpub_clk_trace_sel.hw,
		[CLKID_CPUB_CLK_TRACE]		= &g12b_cpub_clk_trace.hw,
		[CLKID_SPICC0_MUX]		= &g12a_spicc0_mux.hw,
		[CLKID_SPICC0_DIV]		= &g12a_spicc0_div.hw,
		[CLKID_SPICC0_GATE]		= &g12a_spicc0_gate.hw,
		[CLKID_SPICC1_MUX]		= &g12a_spicc1_mux.hw,
		[CLKID_SPICC1_DIV]		= &g12a_spicc1_div.hw,
		[CLKID_SPICC1_GATE]		= &g12a_spicc1_gate.hw,
		[CLKID_WAVE_A_SEL]		= &g12a_wave_a_sel.hw,
		[CLKID_WAVE_A_DIV]		= &g12a_wave_a_div.hw,
		[CLKID_WAVE_A_CLK]		= &g12a_wave_aclk.hw,
		[CLKID_WAVE_B_SEL]		= &g12a_wave_b_sel.hw,
		[CLKID_WAVE_B_DIV]		= &g12a_wave_b_div.hw,
		[CLKID_WAVE_B_CLK]		= &g12a_wave_bclk.hw,
		[CLKID_WAVE_C_SEL]		= &g12a_wave_c_sel.hw,
		[CLKID_WAVE_C_DIV]		= &g12a_wave_c_div.hw,
		[CLKID_WAVE_C_CLK]		= &g12a_wave_cclk.hw,
		[CLKID_BT656_MUX]		= &g12a_bt656_mux.hw,
		[CLKID_BT656_DIV]		= &g12a_bt656_div.hw,
		[CLKID_BT656_GATE]		= &g12a_bt656_gate.hw,
#ifdef CONFIG_AMLOGIC_MODIFY
		[CLKID_25M_CLK_DIV]		= &g12a_25m_div.hw,
		[CLKID_25M_CLK_GATE]		= &g12a_25m_gate.hw,
		[CLKID_24M_GATE]		= &g12a_24m_gate.hw,
		[CLKID_12M_24M]			= &g12a_12m_24m_div.hw,
		[CLKID_GEN_MUX]			= &g12a_gen_clk_sel.hw,
		[CLKID_GEN_DIV]			= &g12a_gen_clk_div.hw,
		[CLKID_GEN_CLK]			= &g12a_gen_clk.hw,
		[CLKID_DSI_MEAS_MUX]		= &g12a_dsi_meas_mux.hw,
		[CLKID_DSI_MEAS_DIV]		= &g12a_dsi_meas_div.hw,
		[CLKID_DSI_MEAS]		= &g12a_dsi_meas.hw,
		[CLKID_VDEC_P1_MUX]		= &g12a_vdec_p1_mux.hw,
		[CLKID_VDEC_P1_DIV]		= &g12a_vdec_p1_div.hw,
		[CLKID_VDEC_P1]			= &g12a_vdec_p1.hw,
		[CLKID_VDEC_MUX]		= &g12a_vdec_mux.hw,
		[CLKID_HCODEC_P0_MUX]		= &g12a_hcodec_p0_mux.hw,
		[CLKID_HCODEC_P0_DIV]		= &g12a_hcodec_p0_div.hw,
		[CLKID_HCODEC_P0]		= &g12a_hcodec_p0.hw,
		[CLKID_HCODEC_P1_MUX]		= &g12a_hcodec_p1_mux.hw,
		[CLKID_HCODEC_P1_DIV]		= &g12a_hcodec_p1_div.hw,
		[CLKID_HCODEC_P1]		= &g12a_hcodec_p1.hw,
		[CLKID_HCODEC_MUX]		= &g12a_hcodec_mux.hw,
		[CLKID_HEVC_P1_MUX]		= &g12a_hevc_p1_mux.hw,
		[CLKID_HEVC_P1_DIV]		= &g12a_hevc_p1_div.hw,
		[CLKID_HEVC_P1]			= &g12a_hevc_p1.hw,
		[CLKID_HEVC_MUX]		= &g12a_hevc_mux.hw,
		[CLKID_HEVCF_P1_MUX]		= &g12a_hevcf_p1_mux.hw,
		[CLKID_HEVCF_P1_DIV]		= &g12a_hevcf_p1_div.hw,
		[CLKID_HEVCF_P1]		= &g12a_hevcf_p1.hw,
		[CLKID_HEVCF_MUX]		= &g12a_hevcf_mux.hw,
		[CLKID_VPU_CLKB_TMP_MUX]	= &g12a_vpu_clkb_tmp_mux.hw,
		[CLKID_VPU_CLKB_TMP_DIV]	= &g12a_vpu_clkb_tmp_div.hw,
		[CLKID_VPU_CLKB_TMP]		= &g12a_vpu_clkb_tmp.hw,
		[CLKID_VPU_CLKB_DIV]		= &g12a_vpu_clkb_div.hw,
		[CLKID_VPU_CLKB]		= &g12a_vpu_clkb.hw,
		[CLKID_VPU_CLKC_P0_MUX]		= &g12a_vpu_clkc_p0_mux.hw,
		[CLKID_VPU_CLKC_P0_DIV]		= &g12a_vpu_clkc_p0_div.hw,
		[CLKID_VPU_CLKC_P0]		= &g12a_vpu_clkc_p0.hw,
		[CLKID_VPU_CLKC_P1_MUX]		= &g12a_vpu_clkc_p1_mux.hw,
		[CLKID_VPU_CLKC_P1_DIV]		= &g12a_vpu_clkc_p1_div.hw,
		[CLKID_VPU_CLKC_P1]		= &g12a_vpu_clkc_p1.hw,
		[CLKID_VPU_CLKC_MUX]		= &g12a_vpu_clkc_mux.hw,
		[CLKID_VIPNANOQ_CORE_MUX]	= &sm1_vipnanoq_core_mux.hw,
		[CLKID_VIPNANOQ_CORE_DIV]	= &sm1_vipnanoq_core_div.hw,
		[CLKID_VIPNANOQ_CORE_GATE]	= &sm1_vipnanoq_core_gate.hw,
		[CLKID_VIPNANOQ_AXI_MUX]	= &sm1_vipnanoq_axi_mux.hw,
		[CLKID_VIPNANOQ_AXI_DIV]	= &sm1_vipnanoq_axi_div.hw,
		[CLKID_VIPNANOQ_AXI_GATE]	= &sm1_vipnanoq_axi_gate.hw,
		[CLKID_GDC_CORE_CLK_MUX]	= &g12b_gdc_core_clk_mux.hw,
		[CLKID_GDC_CORE_CLK_DIV]	= &g12b_gdc_core_clk_div.hw,
		[CLKID_GDC_CORE_CLK]		= &g12b_gdc_core_clk_gate.hw,
		[CLKID_GDC_AXI_CLK_MUX]		= &g12b_gdc_axi_clk_mux.hw,
		[CLKID_GDC_AXI_CLK_DIV]		= &g12b_gdc_axi_clk_div.hw,
		[CLKID_GDC_AXI_CLK]		= &g12b_gdc_axi_clk_gate.hw,
		[CLKID_MIPI_ISP_CLK_MUX]	= &g12b_mipi_isp_clk_mux.hw,
		[CLKID_MIPI_ISP_CLK_DIV]	= &g12b_mipi_isp_clk_div.hw,
		[CLKID_MIPI_ISP_CLK]		= &g12b_mipi_isp_clk_gate.hw,
		[CLKID_MIPI_CSI_PHY_CLK0_MUX]	= &g12b_mipi_csi_phy_clk0_mux.hw,
		[CLKID_MIPI_CSI_PHY_CLK0_DIV]	= &g12b_mipi_csi_phy_clk0_div.hw,
		[CLKID_MIPI_CSI_PHY_CLK0]	= &g12b_mipi_csi_phy_clk0_gate.hw,
		[CLKID_MIPI_CSI_PHY_CLK1_MUX]	= &g12b_mipi_csi_phy_clk1_mux.hw,
		[CLKID_MIPI_CSI_PHY_CLK1_DIV]	= &g12b_mipi_csi_phy_clk1_div.hw,
		[CLKID_MIPI_CSI_PHY_CLK1]	= &g12b_mipi_csi_phy_clk1_gate.hw,
		[CLKID_MIPI_CSI_PHY_CLK]	= &g12b_mipi_sci_phy_mux.hw,
		[CLKID_CSI_DIG]			= &g12b_csi_dig.hw,
		[CLKID_VIPNANOQ]		= &g12b_vipnanoq.hw,
		[CLKID_GDC]			= &g12b_gdc.hw,
		[CLKID_MIPI_ISP]		= &g12b_mipi_isp.hw,
		[CLKID_CSI2_PHY1]		= &g12b_csi2_phy1.hw,
		[CLKID_CSI2_PHY0]		= &g12b_csi2_phy0.hw,
#endif  /* CONFIG_AMLOGIC_MODIFY */
		[NR_CLKS]			= NULL,
	},
	.num = NR_CLKS,
};

static struct clk_hw_onecell_data sm1_hw_onecell_data = {
	.hws = {
		[CLKID_SYS_PLL]			= &g12a_sys_pll.hw,
		[CLKID_FIXED_PLL]		= &g12a_fixed_pll.hw,
		[CLKID_FCLK_DIV2]		= &g12a_fclk_div2.hw,
		[CLKID_FCLK_DIV3]		= &g12a_fclk_div3.hw,
		[CLKID_FCLK_DIV4]		= &g12a_fclk_div4.hw,
		[CLKID_FCLK_DIV5]		= &g12a_fclk_div5.hw,
		[CLKID_FCLK_DIV7]		= &g12a_fclk_div7.hw,
		[CLKID_FCLK_DIV2P5]		= &g12a_fclk_div2p5.hw,
		[CLKID_GP0_PLL]			= &g12a_gp0_pll.hw,
		[CLKID_MPEG_SEL]		= &g12a_mpeg_clk_sel.hw,
		[CLKID_MPEG_DIV]		= &g12a_mpeg_clk_div.hw,
		[CLKID_CLK81]			= &g12a_clk81.hw,
		[CLKID_MPLL0]			= &g12a_mpll0.hw,
		[CLKID_MPLL1]			= &g12a_mpll1.hw,
		[CLKID_MPLL2]			= &g12a_mpll2.hw,
		[CLKID_MPLL3]			= &g12a_mpll3.hw,
		[CLKID_DDR]			= &g12a_ddr.hw,
		[CLKID_DOS]			= &g12a_dos.hw,
		[CLKID_AUDIO_LOCKER]		= &g12a_audio_locker.hw,
		[CLKID_MIPI_DSI_HOST]		= &g12a_mipi_dsi_host.hw,
		[CLKID_ETH_PHY]			= &g12a_eth_phy.hw,
		[CLKID_ISA]			= &g12a_isa.hw,
		[CLKID_PL301]			= &g12a_pl301.hw,
		[CLKID_PERIPHS]			= &g12a_periphs.hw,
		[CLKID_SPICC0]			= &g12a_spicc_0.hw,
		[CLKID_I2C]			= &g12a_i2c.hw,
		[CLKID_SANA]			= &g12a_sana.hw,
		[CLKID_SD]			= &g12a_sd.hw,
		[CLKID_RNG0]			= &g12a_rng0.hw,
		[CLKID_UART0]			= &g12a_uart0.hw,
		[CLKID_SPICC1]			= &g12a_spicc_1.hw,
		[CLKID_HIU_IFACE]		= &g12a_hiu_reg.hw,
		[CLKID_MIPI_DSI_PHY]		= &g12a_mipi_dsi_phy.hw,
		[CLKID_ASSIST_MISC]		= &g12a_assist_misc.hw,
		[CLKID_SD_EMMC_A]		= &g12a_emmc_a.hw,
		[CLKID_SD_EMMC_B]		= &g12a_emmc_b.hw,
		[CLKID_SD_EMMC_C]		= &g12a_emmc_c.hw,
		[CLKID_AUDIO_CODEC]		= &g12a_audio_codec.hw,
		[CLKID_AUDIO]			= &g12a_audio.hw,
		[CLKID_ETH]			= &g12a_eth_core.hw,
		[CLKID_DEMUX]			= &g12a_demux.hw,
		[CLKID_AUDIO_IFIFO]		= &g12a_audio_ififo.hw,
		[CLKID_ADC]			= &g12a_adc.hw,
		[CLKID_UART1]			= &g12a_uart1.hw,
#ifdef CONFIG_AMLOGIC_MODIFY
		[CLKID_GE2D]			= &g12a_ge2d.hw,
#else
		[CLKID_G2D]			= &g12a_g2d.hw,
#endif
		[CLKID_RESET]			= &g12a_reset.hw,
		[CLKID_PCIE_COMB]		= &g12a_pcie_comb.hw,
		[CLKID_PARSER]			= &g12a_parser.hw,
		[CLKID_USB]			= &g12a_usb_general.hw,
		[CLKID_PCIE_PHY]		= &g12a_pcie_phy.hw,
		[CLKID_AHB_ARB0]		= &g12a_ahb_arb0.hw,
		[CLKID_AHB_DATA_BUS]		= &g12a_ahb_data_bus.hw,
		[CLKID_AHB_CTRL_BUS]		= &g12a_ahb_ctrl_bus.hw,
		[CLKID_HTX_HDCP22]		= &g12a_htx_hdcp22.hw,
		[CLKID_HTX_PCLK]		= &g12a_htx_pclk.hw,
		[CLKID_BT656]			= &g12a_bt656.hw,
		[CLKID_USB1_DDR_BRIDGE]		= &g12a_usb1_to_ddr.hw,
		[CLKID_MMC_PCLK]		= &g12a_mmc_pclk.hw,
		[CLKID_UART2]			= &g12a_uart2.hw,
		[CLKID_VPU_INTR]		= &g12a_vpu_intr.hw,
		[CLKID_GIC]			= &g12a_gic.hw,
		[CLKID_SD_EMMC_A_CLK0_SEL]	= &g12a_sd_emmc_a_clk0_sel.hw,
		[CLKID_SD_EMMC_A_CLK0_DIV]	= &g12a_sd_emmc_a_clk0_div.hw,
		[CLKID_SD_EMMC_A_CLK0]		= &g12a_sd_emmc_a_clk0.hw,
		[CLKID_SD_EMMC_B_CLK0_SEL]	= &g12a_sd_emmc_b_clk0_sel.hw,
		[CLKID_SD_EMMC_B_CLK0_DIV]	= &g12a_sd_emmc_b_clk0_div.hw,
		[CLKID_SD_EMMC_B_CLK0]		= &g12a_sd_emmc_b_clk0.hw,
		[CLKID_SD_EMMC_C_CLK0_SEL]	= &g12a_sd_emmc_c_clk0_sel.hw,
		[CLKID_SD_EMMC_C_CLK0_DIV]	= &g12a_sd_emmc_c_clk0_div.hw,
		[CLKID_SD_EMMC_C_CLK0]		= &g12a_sd_emmc_c_clk0.hw,
		[CLKID_MPLL0_DIV]		= &g12a_mpll0_div.hw,
		[CLKID_MPLL1_DIV]		= &g12a_mpll1_div.hw,
		[CLKID_MPLL2_DIV]		= &g12a_mpll2_div.hw,
		[CLKID_MPLL3_DIV]		= &g12a_mpll3_div.hw,
		[CLKID_FCLK_DIV2_DIV]		= &g12a_fclk_div2_div.hw,
		[CLKID_FCLK_DIV3_DIV]		= &g12a_fclk_div3_div.hw,
		[CLKID_FCLK_DIV4_DIV]		= &g12a_fclk_div4_div.hw,
		[CLKID_FCLK_DIV5_DIV]		= &g12a_fclk_div5_div.hw,
		[CLKID_FCLK_DIV7_DIV]		= &g12a_fclk_div7_div.hw,
		[CLKID_FCLK_DIV2P5_DIV]		= &g12a_fclk_div2p5_div.hw,
		[CLKID_HIFI_PLL]		= &g12a_hifi_pll.hw,
		[CLKID_VCLK2_VENCI0]		= &g12a_vclk2_venci0.hw,
		[CLKID_VCLK2_VENCI1]		= &g12a_vclk2_venci1.hw,
		[CLKID_VCLK2_VENCP0]		= &g12a_vclk2_vencp0.hw,
		[CLKID_VCLK2_VENCP1]		= &g12a_vclk2_vencp1.hw,
		[CLKID_VCLK2_VENCT0]		= &g12a_vclk2_venct0.hw,
		[CLKID_VCLK2_VENCT1]		= &g12a_vclk2_venct1.hw,
		[CLKID_VCLK2_OTHER]		= &g12a_vclk2_other.hw,
		[CLKID_VCLK2_ENCI]		= &g12a_vclk2_enci.hw,
		[CLKID_VCLK2_ENCP]		= &g12a_vclk2_encp.hw,
		[CLKID_DAC_CLK]			= &g12a_dac_clk.hw,
		[CLKID_AOCLK]			= &g12a_aoclk_gate.hw,
		[CLKID_IEC958]			= &g12a_iec958_gate.hw,
		[CLKID_ENC480P]			= &g12a_enc480p.hw,
		[CLKID_RNG1]			= &g12a_rng1.hw,
		[CLKID_VCLK2_ENCT]		= &g12a_vclk2_enct.hw,
		[CLKID_VCLK2_ENCL]		= &g12a_vclk2_encl.hw,
		[CLKID_VCLK2_VENCLMMC]		= &g12a_vclk2_venclmmc.hw,
		[CLKID_VCLK2_VENCL]		= &g12a_vclk2_vencl.hw,
		[CLKID_VCLK2_OTHER1]		= &g12a_vclk2_other1.hw,
		[CLKID_FIXED_PLL_DCO]		= &g12a_fixed_pll_dco.hw,
		[CLKID_SYS_PLL_DCO]		= &g12a_sys_pll_dco.hw,
		[CLKID_GP0_PLL_DCO]		= &g12a_gp0_pll_dco.hw,
		[CLKID_HIFI_PLL_DCO]		= &g12a_hifi_pll_dco.hw,
		[CLKID_DMA]			= &g12a_dma.hw,
		[CLKID_EFUSE]			= &g12a_efuse.hw,
		[CLKID_ROM_BOOT]		= &g12a_rom_boot.hw,
		[CLKID_RESET_SEC]		= &g12a_reset_sec.hw,
		[CLKID_SEC_AHB_APB3]		= &g12a_sec_ahb_apb3.hw,
		[CLKID_MPLL_PREDIV]		= &g12a_mpll_prediv.hw,
		[CLKID_VPU_0_SEL]		= &g12a_vpu_0_sel.hw,
		[CLKID_VPU_0_DIV]		= &g12a_vpu_0_div.hw,
		[CLKID_VPU_0]			= &g12a_vpu_0.hw,
		[CLKID_VPU_1_SEL]		= &g12a_vpu_1_sel.hw,
		[CLKID_VPU_1_DIV]		= &g12a_vpu_1_div.hw,
		[CLKID_VPU_1]			= &g12a_vpu_1.hw,
		[CLKID_VPU]			= &g12a_vpu.hw,
		[CLKID_VAPB_0_SEL]		= &g12a_vapb_0_sel.hw,
		[CLKID_VAPB_0_DIV]		= &g12a_vapb_0_div.hw,
		[CLKID_VAPB_0]			= &g12a_vapb_0.hw,
		[CLKID_VAPB_1_SEL]		= &g12a_vapb_1_sel.hw,
		[CLKID_VAPB_1_DIV]		= &g12a_vapb_1_div.hw,
		[CLKID_VAPB_1]			= &g12a_vapb_1.hw,
		[CLKID_VAPB_SEL]		= &g12a_vapb_sel.hw,
#ifdef CONFIG_AMLOGIC_MODIFY
		[CLKID_GE2D_GATE]		= &g12a_ge2d_gate.hw,
#else
		[CLKID_VAPB]			= &g12a_vapb.hw,
#endif
		[CLKID_HDMI_PLL_DCO]		= &g12a_hdmi_pll_dco.hw,
		[CLKID_HDMI_PLL_OD]		= &g12a_hdmi_pll_od.hw,
		[CLKID_HDMI_PLL_OD2]		= &g12a_hdmi_pll_od2.hw,
		[CLKID_HDMI_PLL]		= &g12a_hdmi_pll.hw,
		[CLKID_VID_PLL]			= &g12a_vid_pll_div.hw,
		[CLKID_VID_PLL_SEL]		= &g12a_vid_pll_sel.hw,
		[CLKID_VID_PLL_DIV]		= &g12a_vid_pll.hw,
		[CLKID_VCLK_SEL]		= &g12a_vclk_sel.hw,
		[CLKID_VCLK2_SEL]		= &g12a_vclk2_sel.hw,
		[CLKID_VCLK_INPUT]		= &g12a_vclk_input.hw,
		[CLKID_VCLK2_INPUT]		= &g12a_vclk2_input.hw,
		[CLKID_VCLK_DIV]		= &g12a_vclk_div.hw,
		[CLKID_VCLK2_DIV]		= &g12a_vclk2_div.hw,
		[CLKID_VCLK]			= &g12a_vclk.hw,
		[CLKID_VCLK2]			= &g12a_vclk2.hw,
		[CLKID_VCLK_DIV1]		= &g12a_vclk_div1.hw,
		[CLKID_VCLK_DIV2_EN]		= &g12a_vclk_div2_en.hw,
		[CLKID_VCLK_DIV4_EN]		= &g12a_vclk_div4_en.hw,
		[CLKID_VCLK_DIV6_EN]		= &g12a_vclk_div6_en.hw,
		[CLKID_VCLK_DIV12_EN]		= &g12a_vclk_div12_en.hw,
		[CLKID_VCLK2_DIV1]		= &g12a_vclk2_div1.hw,
		[CLKID_VCLK2_DIV2_EN]		= &g12a_vclk2_div2_en.hw,
		[CLKID_VCLK2_DIV4_EN]		= &g12a_vclk2_div4_en.hw,
		[CLKID_VCLK2_DIV6_EN]		= &g12a_vclk2_div6_en.hw,
		[CLKID_VCLK2_DIV12_EN]		= &g12a_vclk2_div12_en.hw,
		[CLKID_VCLK_DIV2]		= &g12a_vclk_div2.hw,
		[CLKID_VCLK_DIV4]		= &g12a_vclk_div4.hw,
		[CLKID_VCLK_DIV6]		= &g12a_vclk_div6.hw,
		[CLKID_VCLK_DIV12]		= &g12a_vclk_div12.hw,
		[CLKID_VCLK2_DIV2]		= &g12a_vclk2_div2.hw,
		[CLKID_VCLK2_DIV4]		= &g12a_vclk2_div4.hw,
		[CLKID_VCLK2_DIV6]		= &g12a_vclk2_div6.hw,
		[CLKID_VCLK2_DIV12]		= &g12a_vclk2_div12.hw,
		[CLKID_CTS_ENCI_SEL]		= &g12a_cts_enci_sel.hw,
		[CLKID_CTS_ENCP_SEL]		= &g12a_cts_encp_sel.hw,
		[CLKID_CTS_VDAC_SEL]		= &g12a_cts_vdac_sel.hw,
		[CLKID_HDMI_TX_SEL]		= &g12a_hdmi_tx_sel.hw,
		[CLKID_CTS_ENCI]		= &g12a_cts_enci.hw,
		[CLKID_CTS_ENCP]		= &g12a_cts_encp.hw,
		[CLKID_CTS_VDAC]		= &g12a_cts_vdac.hw,
		[CLKID_HDMI_TX]			= &g12a_hdmi_tx.hw,
		[CLKID_HDMI_SEL]		= &g12a_hdmi_sel.hw,
		[CLKID_HDMI_DIV]		= &g12a_hdmi_div.hw,
		[CLKID_HDMI]			= &g12a_hdmi.hw,
		[CLKID_MALI_0_SEL]		= &g12a_mali_0_sel.hw,
		[CLKID_MALI_0_DIV]		= &g12a_mali_0_div.hw,
		[CLKID_MALI_0]			= &g12a_mali_0.hw,
		[CLKID_MALI_1_SEL]		= &g12a_mali_1_sel.hw,
		[CLKID_MALI_1_DIV]		= &g12a_mali_1_div.hw,
		[CLKID_MALI_1]			= &g12a_mali_1.hw,
		[CLKID_MALI]			= &g12a_mali.hw,
		[CLKID_MPLL_50M_DIV]		= &g12a_mpll_50m_div.hw,
		[CLKID_MPLL_50M]		= &g12a_mpll_50m.hw,
		[CLKID_SYS_PLL_DIV16_EN]	= &g12a_sys_pll_div16_en.hw,
		[CLKID_SYS_PLL_DIV16]		= &g12a_sys_pll_div16.hw,
		[CLKID_CPU_CLK_DYN0_SEL]	= &g12a_cpu_clk_premux0.hw,
		[CLKID_CPU_CLK_DYN0_DIV]	= &g12a_cpu_clk_mux0_div.hw,
		[CLKID_CPU_CLK_DYN0]		= &g12a_cpu_clk_postmux0.hw,
		[CLKID_CPU_CLK_DYN1_SEL]	= &g12a_cpu_clk_premux1.hw,
		[CLKID_CPU_CLK_DYN1_DIV]	= &g12a_cpu_clk_mux1_div.hw,
		[CLKID_CPU_CLK_DYN1]		= &g12a_cpu_clk_postmux1.hw,
		[CLKID_CPU_CLK_DYN]		= &g12a_cpu_clk_dyn.hw,
		[CLKID_CPU_CLK]			= &g12a_cpu_clk.hw,
		[CLKID_CPU_CLK_DIV16_EN]	= &g12a_cpu_clk_div16_en.hw,
		[CLKID_CPU_CLK_DIV16]		= &g12a_cpu_clk_div16.hw,
		[CLKID_CPU_CLK_APB_DIV]		= &g12a_cpu_clk_apb_div.hw,
		[CLKID_CPU_CLK_APB]		= &g12a_cpu_clk_apb.hw,
		[CLKID_CPU_CLK_ATB_DIV]		= &g12a_cpu_clk_atb_div.hw,
		[CLKID_CPU_CLK_ATB]		= &g12a_cpu_clk_atb.hw,
		[CLKID_CPU_CLK_AXI_DIV]		= &g12a_cpu_clk_axi_div.hw,
		[CLKID_CPU_CLK_AXI]		= &g12a_cpu_clk_axi.hw,
		[CLKID_CPU_CLK_TRACE_DIV]	= &g12a_cpu_clk_trace_div.hw,
		[CLKID_CPU_CLK_TRACE]		= &g12a_cpu_clk_trace.hw,
		[CLKID_PCIE_PLL_DCO]		= &g12a_pcie_pll_dco.hw,
		[CLKID_PCIE_PLL_DCO_DIV2]	= &g12a_pcie_pll_dco_div2.hw,
		[CLKID_PCIE_PLL_OD]		= &g12a_pcie_pll_od.hw,
		[CLKID_PCIE_PLL]		= &g12a_pcie_pll.hw,
		[CLKID_PCIE_BGP]		= &g12a_pcie_bgp.hw,
		[CLKID_PCIE_HCSL]               = &g12a_pcie_hcsl.hw,
		[CLKID_VDEC_1_SEL]		= &g12a_vdec_1_sel.hw,
		[CLKID_VDEC_1_DIV]		= &g12a_vdec_1_div.hw,
		[CLKID_VDEC_1]			= &g12a_vdec_1.hw,
		[CLKID_VDEC_HEVC_SEL]		= &g12a_vdec_hevc_sel.hw,
		[CLKID_VDEC_HEVC_DIV]		= &g12a_vdec_hevc_div.hw,
		[CLKID_VDEC_HEVC]		= &g12a_vdec_hevc.hw,
		[CLKID_VDEC_HEVCF_SEL]		= &g12a_vdec_hevcf_sel.hw,
		[CLKID_VDEC_HEVCF_DIV]		= &g12a_vdec_hevcf_div.hw,
		[CLKID_VDEC_HEVCF]		= &g12a_vdec_hevcf.hw,
		[CLKID_TS_DIV]			= &g12a_ts_div.hw,
		[CLKID_TS]			= &g12a_ts.hw,
		[CLKID_GP1_PLL_DCO]		= &sm1_gp1_pll_dco.hw,
		[CLKID_GP1_PLL]			= &sm1_gp1_pll.hw,
		[CLKID_DSU_CLK_DYN0_SEL]	= &sm1_dsu_clk_premux0.hw,
		[CLKID_DSU_CLK_DYN0_DIV]	= &sm1_dsu_clk_premux1.hw,
		[CLKID_DSU_CLK_DYN0]		= &sm1_dsu_clk_mux0_div.hw,
		[CLKID_DSU_CLK_DYN1_SEL]	= &sm1_dsu_clk_postmux0.hw,
		[CLKID_DSU_CLK_DYN1_DIV]	= &sm1_dsu_clk_mux1_div.hw,
		[CLKID_DSU_CLK_DYN1]		= &sm1_dsu_clk_postmux1.hw,
		[CLKID_DSU_CLK_DYN]		= &sm1_dsu_clk_dyn.hw,
		[CLKID_DSU_CLK_FINAL]		= &sm1_dsu_final_clk.hw,
		[CLKID_DSU_CLK]			= &sm1_dsu_clk.hw,
		[CLKID_CPU1_CLK]		= &sm1_cpu1_clk.hw,
		[CLKID_CPU2_CLK]		= &sm1_cpu2_clk.hw,
		[CLKID_CPU3_CLK]		= &sm1_cpu3_clk.hw,
		[CLKID_DSI_MEAS_MUX]		= &g12a_dsi_meas_mux.hw,
		[CLKID_DSI_MEAS_DIV]		= &g12a_dsi_meas_div.hw,
		[CLKID_DSI_MEAS]		= &g12a_dsi_meas.hw,
		[CLKID_VDEC_P1_MUX]		= &g12a_vdec_p1_mux.hw,
		[CLKID_VDEC_P1_DIV]		= &g12a_vdec_p1_div.hw,
		[CLKID_VDEC_P1]			= &g12a_vdec_p1.hw,
		[CLKID_VDEC_MUX]		= &g12a_vdec_mux.hw,
		[CLKID_HCODEC_P0_MUX]		= &g12a_hcodec_p0_mux.hw,
		[CLKID_HCODEC_P0_DIV]		= &g12a_hcodec_p0_div.hw,
		[CLKID_HCODEC_P0]		= &g12a_hcodec_p0.hw,
		[CLKID_HCODEC_P1_MUX]		= &g12a_hcodec_p1_mux.hw,
		[CLKID_HCODEC_P1_DIV]		= &g12a_hcodec_p1_div.hw,
		[CLKID_HCODEC_P1]		= &g12a_hcodec_p1.hw,
		[CLKID_HCODEC_MUX]		= &g12a_hcodec_mux.hw,
		[CLKID_HEVC_P1_MUX]		= &g12a_hevc_p1_mux.hw,
		[CLKID_HEVC_P1_DIV]		= &g12a_hevc_p1_div.hw,
		[CLKID_HEVC_P1]			= &g12a_hevc_p1.hw,
		[CLKID_HEVC_MUX]		= &g12a_hevc_mux.hw,
		[CLKID_HEVCF_P1_MUX]		= &g12a_hevcf_p1_mux.hw,
		[CLKID_HEVCF_P1_DIV]		= &g12a_hevcf_p1_div.hw,
		[CLKID_HEVCF_P1]		= &g12a_hevcf_p1.hw,
		[CLKID_HEVCF_MUX]		= &g12a_hevcf_mux.hw,
		[CLKID_VPU_CLKB_TMP_MUX]	= &g12a_vpu_clkb_tmp_mux.hw,
		[CLKID_VPU_CLKB_TMP_DIV]	= &g12a_vpu_clkb_tmp_div.hw,
		[CLKID_VPU_CLKB_TMP]		= &g12a_vpu_clkb_tmp.hw,
		[CLKID_VPU_CLKB_DIV]		= &g12a_vpu_clkb_div.hw,
		[CLKID_VPU_CLKB]		= &g12a_vpu_clkb.hw,
		[CLKID_VPU_CLKC_P0_MUX]		= &g12a_vpu_clkc_p0_mux.hw,
		[CLKID_VPU_CLKC_P0_DIV]		= &g12a_vpu_clkc_p0_div.hw,
		[CLKID_VPU_CLKC_P0]		= &g12a_vpu_clkc_p0.hw,
		[CLKID_VPU_CLKC_P1_MUX]		= &g12a_vpu_clkc_p1_mux.hw,
		[CLKID_VPU_CLKC_P1_DIV]		= &g12a_vpu_clkc_p1_div.hw,
		[CLKID_VPU_CLKC_P1]		= &g12a_vpu_clkc_p1.hw,
		[CLKID_VPU_CLKC_MUX]		= &g12a_vpu_clkc_mux.hw,
		[CLKID_SPICC0_MUX]		= &g12a_spicc0_mux.hw,
		[CLKID_SPICC0_DIV]		= &g12a_spicc0_div.hw,
		[CLKID_SPICC0_GATE]		= &g12a_spicc0_gate.hw,
		[CLKID_SPICC1_MUX]		= &g12a_spicc1_mux.hw,
		[CLKID_SPICC1_DIV]		= &g12a_spicc1_div.hw,
		[CLKID_SPICC1_GATE]		= &g12a_spicc1_gate.hw,
		[CLKID_WAVE_A_SEL]		= &g12a_wave_a_sel.hw,
		[CLKID_WAVE_A_DIV]		= &g12a_wave_a_div.hw,
		[CLKID_WAVE_A_CLK]		= &g12a_wave_aclk.hw,
		[CLKID_WAVE_B_SEL]		= &g12a_wave_b_sel.hw,
		[CLKID_WAVE_B_DIV]		= &g12a_wave_b_div.hw,
		[CLKID_WAVE_B_CLK]		= &g12a_wave_bclk.hw,
		[CLKID_WAVE_C_SEL]		= &g12a_wave_c_sel.hw,
		[CLKID_WAVE_C_DIV]		= &g12a_wave_c_div.hw,
		[CLKID_WAVE_C_CLK]		= &g12a_wave_cclk.hw,
		[CLKID_BT656_MUX]		= &g12a_bt656_mux.hw,
		[CLKID_BT656_DIV]		= &g12a_bt656_div.hw,
		[CLKID_BT656_GATE]		= &g12a_bt656_gate.hw,
#ifdef CONFIG_AMLOGIC_MODIFY
		[CLKID_DSI_MEAS_MUX]		= &g12a_dsi_meas_mux.hw,
		[CLKID_DSI_MEAS_DIV]		= &g12a_dsi_meas_div.hw,
		[CLKID_DSI_MEAS]		= &g12a_dsi_meas.hw,
		[CLKID_VDEC_P1_MUX]		= &g12a_vdec_p1_mux.hw,
		[CLKID_VDEC_P1_DIV]		= &g12a_vdec_p1_div.hw,
		[CLKID_VDEC_P1]			= &g12a_vdec_p1.hw,
		[CLKID_VDEC_MUX]		= &g12a_vdec_mux.hw,
		[CLKID_HCODEC_P0_MUX]		= &g12a_hcodec_p0_mux.hw,
		[CLKID_HCODEC_P0_DIV]		= &g12a_hcodec_p0_div.hw,
		[CLKID_HCODEC_P0]		= &g12a_hcodec_p0.hw,
		[CLKID_HCODEC_P1_MUX]		= &g12a_hcodec_p1_mux.hw,
		[CLKID_HCODEC_P1_DIV]		= &g12a_hcodec_p1_div.hw,
		[CLKID_HCODEC_P1]		= &g12a_hcodec_p1.hw,
		[CLKID_HCODEC_MUX]		= &g12a_hcodec_mux.hw,
		[CLKID_HEVC_P1_MUX]		= &g12a_hevc_p1_mux.hw,
		[CLKID_HEVC_P1_DIV]		= &g12a_hevc_p1_div.hw,
		[CLKID_HEVC_P1]			= &g12a_hevc_p1.hw,
		[CLKID_HEVC_MUX]		= &g12a_hevc_mux.hw,
		[CLKID_HEVCF_P1_MUX]		= &g12a_hevcf_p1_mux.hw,
		[CLKID_HEVCF_P1_DIV]		= &g12a_hevcf_p1_div.hw,
		[CLKID_HEVCF_P1]		= &g12a_hevcf_p1.hw,
		[CLKID_HEVCF_MUX]		= &g12a_hevcf_mux.hw,
		[CLKID_VPU_CLKB_TMP_MUX]	= &g12a_vpu_clkb_tmp_mux.hw,
		[CLKID_VPU_CLKB_TMP_DIV]	= &g12a_vpu_clkb_tmp_div.hw,
		[CLKID_VPU_CLKB_TMP]		= &g12a_vpu_clkb_tmp.hw,
		[CLKID_VPU_CLKB_DIV]		= &g12a_vpu_clkb_div.hw,
		[CLKID_VPU_CLKB]		= &g12a_vpu_clkb.hw,
		[CLKID_VPU_CLKC_P0_MUX]		= &g12a_vpu_clkc_p0_mux.hw,
		[CLKID_VPU_CLKC_P0_DIV]		= &g12a_vpu_clkc_p0_div.hw,
		[CLKID_VPU_CLKC_P0]		= &g12a_vpu_clkc_p0.hw,
		[CLKID_VPU_CLKC_P1_MUX]		= &g12a_vpu_clkc_p1_mux.hw,
		[CLKID_VPU_CLKC_P1_DIV]		= &g12a_vpu_clkc_p1_div.hw,
		[CLKID_VPU_CLKC_P1]		= &g12a_vpu_clkc_p1.hw,
		[CLKID_VPU_CLKC_MUX]		= &g12a_vpu_clkc_mux.hw,
		[CLKID_VDIN_MEAS_MUX]		= &sm1_vdin_meas_mux.hw,
		[CLKID_VDIN_MEAS_DIV]		= &sm1_vdin_meas_div.hw,
		[CLKID_VDIN_MEAS_GATE]		= &sm1_vdin_meas_gate.hw,
		[CLKID_VIPNANOQ_CORE_MUX]	= &sm1_vipnanoq_core_mux.hw,
		[CLKID_VIPNANOQ_CORE_DIV]	= &sm1_vipnanoq_core_div.hw,
		[CLKID_VIPNANOQ_CORE_GATE]	= &sm1_vipnanoq_core_gate.hw,
		[CLKID_VIPNANOQ_AXI_MUX]	= &sm1_vipnanoq_axi_mux.hw,
		[CLKID_VIPNANOQ_AXI_DIV]	= &sm1_vipnanoq_axi_div.hw,
		[CLKID_VIPNANOQ_AXI_GATE]	= &sm1_vipnanoq_axi_gate.hw,
		[CLKID_CSI_ADAPT_CLK_MUX]	= &sm1_csi_adapt_clk_mux.hw,
		[CLKID_CSI_ADAPT_CLK_DIV]	= &sm1_csi_adapt_clk_div.hw,
		[CLKID_CSI_ADAPT_CLK]		= &sm1_csi_adapt_clk_gate.hw,
		[CLKID_CSI_PHY]			= &sm1_csi_phy.hw,
		[CLKID_TEMP_SENSOR]		= &sm1_temp_sensor.hw,
		[CLKID_CSI_ADAPT]		= &sm1_csi_adpat.hw,
		[CLKID_CSI_HOST]		= &sm1_csi_host.hw,
		[CLKID_PARSER1]			= &sm1_parser1.hw,
		[CLKID_NNA]			= &sm1_nna.hw,
		[CLKID_CSI_DIG]			= &sm1_csi_dig.hw,
		[CLKID_MIPI_CSI_PHY_CLK0_MUX]	= &g12b_mipi_csi_phy_clk0_mux.hw,
		[CLKID_MIPI_CSI_PHY_CLK0_DIV]	= &g12b_mipi_csi_phy_clk0_div.hw,
		[CLKID_MIPI_CSI_PHY_CLK0]	= &g12b_mipi_csi_phy_clk0_gate.hw,
		[CLKID_25M_CLK_DIV]		= &g12a_25m_div.hw,
		[CLKID_25M_CLK_GATE]		= &g12a_25m_gate.hw,
		[CLKID_24M_GATE]		= &g12a_24m_gate.hw,
		[CLKID_12M_24M]			= &g12a_12m_24m_div.hw,
		[CLKID_GEN_MUX]			= &g12a_gen_clk_sel.hw,
		[CLKID_GEN_DIV]			= &g12a_gen_clk_div.hw,
		[CLKID_GEN_CLK]			= &g12a_gen_clk.hw,
#endif
		[NR_CLKS]			= NULL,
	},
	.num = NR_CLKS,
};

/* Convenience table to populate regmap in .probe */
static struct clk_regmap *const g12a_clk_regmaps[] __initconst = {
	&g12a_clk81,
	&g12a_dos,
	&g12a_ddr,
	&g12a_audio_locker,
	&g12a_mipi_dsi_host,
	&g12a_eth_phy,
	&g12a_isa,
	&g12a_pl301,
	&g12a_periphs,
	&g12a_spicc_0,
	&g12a_i2c,
	&g12a_sana,
	&g12a_sd,
	&g12a_rng0,
	&g12a_uart0,
	&g12a_spicc_1,
	&g12a_hiu_reg,
	&g12a_mipi_dsi_phy,
	&g12a_assist_misc,
	&g12a_emmc_a,
	&g12a_emmc_b,
	&g12a_emmc_c,
	&g12a_audio_codec,
	&g12a_audio,
	&g12a_eth_core,
	&g12a_demux,
	&g12a_audio_ififo,
	&g12a_adc,
	&g12a_uart1,
#ifdef CONFIG_AMLOGIC_MODIFY
	&g12a_ge2d,
#else
	&g12a_g2d,
#endif
	&g12a_reset,
	&g12a_pcie_comb,
	&g12a_parser,
	&g12a_usb_general,
	&g12a_pcie_phy,
	&g12a_ahb_arb0,
	&g12a_ahb_data_bus,
	&g12a_ahb_ctrl_bus,
	&g12a_htx_hdcp22,
	&g12a_htx_pclk,
	&g12a_bt656,
	&g12a_usb1_to_ddr,
	&g12a_mmc_pclk,
	&g12a_uart2,
	&g12a_vpu_intr,
	&g12a_gic,
	&g12a_sd_emmc_a_clk0,
	&g12a_sd_emmc_b_clk0,
	&g12a_sd_emmc_c_clk0,
	&g12a_mpeg_clk_div,
	&g12a_sd_emmc_a_clk0_div,
	&g12a_sd_emmc_b_clk0_div,
	&g12a_sd_emmc_c_clk0_div,
	&g12a_mpeg_clk_sel,
	&g12a_sd_emmc_a_clk0_sel,
	&g12a_sd_emmc_b_clk0_sel,
	&g12a_sd_emmc_c_clk0_sel,
	&g12a_mpll0,
	&g12a_mpll1,
	&g12a_mpll2,
	&g12a_mpll3,
	&g12a_mpll0_div,
	&g12a_mpll1_div,
	&g12a_mpll2_div,
	&g12a_mpll3_div,
	&g12a_fixed_pll,
	&g12a_sys_pll,
	&g12a_gp0_pll,
	&g12a_hifi_pll,
	&g12a_vclk2_venci0,
	&g12a_vclk2_venci1,
	&g12a_vclk2_vencp0,
	&g12a_vclk2_vencp1,
	&g12a_vclk2_venct0,
	&g12a_vclk2_venct1,
	&g12a_vclk2_other,
	&g12a_vclk2_enci,
	&g12a_vclk2_encp,
	&g12a_dac_clk,
	&g12a_aoclk_gate,
	&g12a_iec958_gate,
	&g12a_enc480p,
	&g12a_rng1,
	&g12a_vclk2_enct,
	&g12a_vclk2_encl,
	&g12a_vclk2_venclmmc,
	&g12a_vclk2_vencl,
	&g12a_vclk2_other1,
	&g12a_fixed_pll_dco,
	&g12a_sys_pll_dco,
	&g12a_gp0_pll_dco,
	&g12a_hifi_pll_dco,
	&g12a_fclk_div2,
	&g12a_fclk_div3,
	&g12a_fclk_div4,
	&g12a_fclk_div5,
	&g12a_fclk_div7,
	&g12a_fclk_div2p5,
	&g12a_dma,
	&g12a_efuse,
	&g12a_rom_boot,
	&g12a_reset_sec,
	&g12a_sec_ahb_apb3,
	&g12a_vpu_0_sel,
	&g12a_vpu_0_div,
	&g12a_vpu_0,
	&g12a_vpu_1_sel,
	&g12a_vpu_1_div,
	&g12a_vpu_1,
	&g12a_vpu,
	&g12a_vapb_0_sel,
	&g12a_vapb_0_div,
	&g12a_vapb_0,
	&g12a_vapb_1_sel,
	&g12a_vapb_1_div,
	&g12a_vapb_1,
	&g12a_vapb_sel,
#ifdef CONFIG_AMLOGIC_MODIFY
	&g12a_ge2d_gate,
#else
	&g12a_vapb,
#endif
	&g12a_hdmi_pll_dco,
	&g12a_hdmi_pll_od,
	&g12a_hdmi_pll_od2,
	&g12a_hdmi_pll,
	&g12a_vid_pll_div,
	&g12a_vid_pll_sel,
	&g12a_vid_pll,
	&g12a_vclk_sel,
	&g12a_vclk2_sel,
	&g12a_vclk_input,
	&g12a_vclk2_input,
	&g12a_vclk_div,
	&g12a_vclk2_div,
	&g12a_vclk,
	&g12a_vclk2,
	&g12a_vclk_div1,
	&g12a_vclk_div2_en,
	&g12a_vclk_div4_en,
	&g12a_vclk_div6_en,
	&g12a_vclk_div12_en,
	&g12a_vclk2_div1,
	&g12a_vclk2_div2_en,
	&g12a_vclk2_div4_en,
	&g12a_vclk2_div6_en,
	&g12a_vclk2_div12_en,
	&g12a_cts_enci_sel,
	&g12a_cts_encp_sel,
	&g12a_cts_vdac_sel,
	&g12a_hdmi_tx_sel,
	&g12a_cts_enci,
	&g12a_cts_encp,
	&g12a_cts_vdac,
	&g12a_hdmi_tx,
	&g12a_hdmi_sel,
	&g12a_hdmi_div,
	&g12a_hdmi,
	&g12a_mali_0_sel,
	&g12a_mali_0_div,
	&g12a_mali_0,
	&g12a_mali_1_sel,
	&g12a_mali_1_div,
	&g12a_mali_1,
	&g12a_mali,
	&g12a_mpll_50m,
	&g12a_sys_pll_div16_en,
	&g12a_cpu_clk_premux0,
	&g12a_cpu_clk_mux0_div,
	&g12a_cpu_clk_postmux0,
	&g12a_cpu_clk_premux1,
	&g12a_cpu_clk_mux1_div,
	&g12a_cpu_clk_postmux1,
	&g12a_cpu_clk_dyn,
	&g12a_cpu_clk,
	&g12a_cpu_clk_div16_en,
	&g12a_cpu_clk_apb_div,
	&g12a_cpu_clk_apb,
	&g12a_cpu_clk_atb_div,
	&g12a_cpu_clk_atb,
	&g12a_cpu_clk_axi_div,
	&g12a_cpu_clk_axi,
	&g12a_cpu_clk_trace_div,
	&g12a_cpu_clk_trace,
	&g12a_pcie_pll_od,
	&g12a_pcie_pll_dco,
	&g12a_pcie_bgp,
	&g12a_pcie_hcsl,
	&g12a_vdec_1_sel,
	&g12a_vdec_1_div,
	&g12a_vdec_1,
	&g12a_vdec_hevc_sel,
	&g12a_vdec_hevc_div,
	&g12a_vdec_hevc,
	&g12a_vdec_hevcf_sel,
	&g12a_vdec_hevcf_div,
	&g12a_vdec_hevcf,
	&g12a_ts_div,
	&g12a_ts,
	&g12b_cpu_clk,
	&g12b_sys1_pll_dco,
	&g12b_sys1_pll,
	&g12b_sys1_pll_div16_en,
	&g12b_cpub_clk_premux0,
	&g12b_cpub_clk_mux0_div,
	&g12b_cpub_clk_postmux0,
	&g12b_cpub_clk_premux1,
	&g12b_cpub_clk_mux1_div,
	&g12b_cpub_clk_postmux1,
	&g12b_cpub_clk_dyn,
	&g12b_cpub_clk,
	&g12b_cpub_clk_div16_en,
	&g12b_cpub_clk_apb_sel,
	&g12b_cpub_clk_apb,
	&g12b_cpub_clk_atb_sel,
	&g12b_cpub_clk_atb,
	&g12b_cpub_clk_axi_sel,
	&g12b_cpub_clk_axi,
	&g12b_cpub_clk_trace_sel,
	&g12b_cpub_clk_trace,
	&sm1_gp1_pll_dco,
	&sm1_gp1_pll,
	&sm1_dsu_clk_premux0,
	&sm1_dsu_clk_premux1,
	&sm1_dsu_clk_mux0_div,
	&sm1_dsu_clk_postmux0,
	&sm1_dsu_clk_mux1_div,
	&sm1_dsu_clk_postmux1,
	&sm1_dsu_clk_dyn,
	&sm1_dsu_final_clk,
	&sm1_dsu_clk,
	&sm1_cpu1_clk,
	&sm1_cpu2_clk,
	&sm1_cpu3_clk,
	&g12a_dsi_meas_mux,
	&g12a_dsi_meas_div,
	&g12a_dsi_meas,
	&g12a_vdec_p1_mux,
	&g12a_vdec_p1_div,
	&g12a_vdec_p1,
	&g12a_vdec_mux,
	&g12a_hcodec_p0_mux,
	&g12a_hcodec_p0_div,
	&g12a_hcodec_p0,
	&g12a_hcodec_p1_mux,
	&g12a_hcodec_p1_div,
	&g12a_hcodec_p1,
	&g12a_hcodec_mux,
	&g12a_hevc_p1_mux,
	&g12a_hevc_p1_div,
	&g12a_hevc_p1,
	&g12a_hevc_mux,
	&g12a_hevcf_p1_mux,
	&g12a_hevcf_p1_div,
	&g12a_hevcf_p1,
	&g12a_hevcf_mux,
	&g12a_vpu_clkb_tmp_mux,
	&g12a_vpu_clkb_tmp_div,
	&g12a_vpu_clkb_tmp,
	&g12a_vpu_clkb_div,
	&g12a_vpu_clkb,
	&g12a_vpu_clkc_p0_mux,
	&g12a_vpu_clkc_p0_div,
	&g12a_vpu_clkc_p0,
	&g12a_vpu_clkc_p1_mux,
	&g12a_vpu_clkc_p1_div,
	&g12a_vpu_clkc_p1,
	&g12a_vpu_clkc_mux,
	&g12a_wave_a_sel,
	&g12a_wave_a_div,
	&g12a_wave_aclk,
	&g12a_wave_b_sel,
	&g12a_wave_b_div,
	&g12a_wave_bclk,
	&g12a_wave_c_sel,
	&g12a_wave_c_div,
	&g12a_wave_cclk,
#ifdef CONFIG_AMLOGIC_MODIFY
	&g12a_uart2,
	&g12a_spicc0_mux,
	&g12a_spicc0_div,
	&g12a_spicc0_gate,
	&g12a_spicc1_mux,
	&g12a_spicc1_div,
	&g12a_spicc1_gate,
	&sm1_vdin_meas_mux,
	&sm1_vdin_meas_div,
	&sm1_vdin_meas_gate,
	&g12a_bt656_mux,
	&g12a_bt656_div,
	&g12a_bt656_gate,
	&sm1_vipnanoq_core_mux,
	&sm1_vipnanoq_core_div,
	&sm1_vipnanoq_core_gate,
	&sm1_vipnanoq_axi_mux,
	&sm1_vipnanoq_axi_div,
	&sm1_vipnanoq_axi_gate,
	&g12a_gen_clk_sel,
	&g12a_gen_clk_div,
	&g12a_gen_clk,
	&g12a_25m_div,
	&g12a_25m_gate,
	&g12a_24m_gate,
	&g12a_12m_24m_div,
	&g12b_gdc_core_clk_mux,
	&g12b_gdc_core_clk_div,
	&g12b_gdc_core_clk_gate,
	&g12b_gdc_axi_clk_mux,
	&g12b_gdc_axi_clk_div,
	&g12b_gdc_axi_clk_gate,
	&g12b_mipi_isp_clk_mux,
	&g12b_mipi_isp_clk_div,
	&g12b_mipi_isp_clk_gate,
	&g12b_mipi_csi_phy_clk0_mux,
	&g12b_mipi_csi_phy_clk0_div,
	&g12b_mipi_csi_phy_clk0_gate,
	&g12b_mipi_csi_phy_clk1_mux,
	&g12b_mipi_csi_phy_clk1_div,
	&g12b_mipi_csi_phy_clk1_gate,
	&g12b_mipi_sci_phy_mux,
	&sm1_csi_adapt_clk_mux,
	&sm1_csi_adapt_clk_div,
	&sm1_csi_adapt_clk_gate,
	&g12b_csi_dig,
	&g12b_vipnanoq,
	&g12b_gdc,
	&g12b_mipi_isp,
	&g12b_csi2_phy1,
	&g12b_csi2_phy0,
	&sm1_csi_phy,
	&sm1_temp_sensor,
	&sm1_csi_adpat,
	&sm1_csi_host,
	&sm1_parser1,
	&sm1_nna,
	&sm1_csi_dig
#endif
};

static const struct reg_sequence g12a_init_regs[] = {
	{ .reg = HHI_MPLL_CNTL0,	.def = 0x00000543 },
};

static int meson_g12a_dvfs_setup_common(struct platform_device *pdev,
					struct clk_hw **hws)
{
	struct clk *notifier_clk;
	struct clk_hw *fclk_div2;
	int ret;

	fclk_div2 = clk_hw_get_parent_by_index(hws[CLKID_CPU_CLK_DYN1_SEL], 1);

	/* Setup clock notifier for cpu_clk_postmux0 */
	g12a_cpu_clk_postmux0_nb_data.fclk_div2 = fclk_div2;
	notifier_clk = g12a_cpu_clk_postmux0.hw.clk;
	ret = clk_notifier_register(notifier_clk,
				    &g12a_cpu_clk_postmux0_nb_data.nb);
	if (ret) {
		dev_err(&pdev->dev, "failed to register the cpu_clk_postmux0 notifier\n");
		return ret;
	}

	return 0;
}

static int meson_g12b_dvfs_setup(struct platform_device *pdev)
{
	struct clk_hw **hws = g12b_hw_onecell_data.hws;
	struct clk *notifier_clk;
	struct clk_hw *fclk_div2;
	int ret;

	ret = meson_g12a_dvfs_setup_common(pdev, hws);
	if (ret)
		return ret;

	fclk_div2 = clk_hw_get_parent_by_index(hws[CLKID_CPU_CLK_DYN1_SEL], 1);

	/* Setup clock notifier for sys1_pll */
	notifier_clk = g12b_sys1_pll.hw.clk;
	ret = clk_notifier_register(notifier_clk,
				    &g12b_cpu_clk_sys1_pll_nb_data.nb);
	if (ret) {
		dev_err(&pdev->dev, "failed to register the sys1_pll notifier\n");
		return ret;
	}

	/* Add notifiers for the second CPU cluster */

	/* Setup clock notifier for cpub_clk_postmux0 */
	g12b_cpub_clk_postmux0_nb_data.fclk_div2 = fclk_div2;
	notifier_clk = g12b_cpub_clk_postmux0.hw.clk;
	ret = clk_notifier_register(notifier_clk,
				    &g12b_cpub_clk_postmux0_nb_data.nb);
	if (ret) {
		dev_err(&pdev->dev, "failed to register the cpub_clk_postmux0 notifier\n");
		return ret;
	}

	/* Setup clock notifier for sys_pll */
	notifier_clk = g12a_sys_pll.hw.clk;
	ret = clk_notifier_register(notifier_clk,
				    &g12b_cpub_clk_sys_pll_nb_data.nb);
	if (ret) {
		dev_err(&pdev->dev, "failed to register the sys_pll notifier\n");
		return ret;
	}

	return 0;
}

static int meson_g12a_dvfs_setup(struct platform_device *pdev)
{
	struct clk_hw **hws = g12a_hw_onecell_data.hws;
	struct clk *notifier_clk;
	int ret;

	ret = meson_g12a_dvfs_setup_common(pdev, hws);
	if (ret)
		return ret;

	/* Setup clock notifier for sys_pll */
	notifier_clk = g12a_sys_pll.hw.clk;
	ret = clk_notifier_register(notifier_clk, &g12a_sys_pll_nb_data.nb);
	if (ret) {
		dev_err(&pdev->dev, "failed to register the sys_pll notifier\n");
		return ret;
	}

	return 0;
}

static int meson_sm1_dvfs_setup(struct platform_device *pdev)
{
	struct clk_hw **hws = sm1_hw_onecell_data.hws;
	int ret;

	ret = meson_g12a_dvfs_setup_common(pdev, hws);
	if (ret)
		return ret;

	/* Setup clock notifier for sys_pll */
	ret = clk_notifier_register(g12a_sys_pll.hw.clk,
				    &g12a_sys_pll_nb_data.nb);
	if (ret) {
		dev_err(&pdev->dev, "failed to register sys_pll notifier\n");
		return ret;
	}

	/* Setup clock notifier for dsu */
	/* set sm1_dsu_clk_premux1 parent to fclk_div2 1G */
	ret = clk_set_parent(sm1_dsu_clk_premux1.hw.clk,
			     g12a_fclk_div2.hw.clk);
	if (ret < 0) {
		pr_err("%s: failed to set dsu parent\n", __func__);
		return ret;
	}

	ret = clk_notifier_register(sm1_dsu_clk_postmux0.hw.clk,
				    &sm1_dsu_clk_postmux0_nb_data.nb);
	if (ret) {
		dev_err(&pdev->dev, "failed to register dsu notifier\n");
		return ret;
	}

	return 0;
}

struct meson_g12a_data {
	const struct meson_eeclkc_data eeclkc_data;
	int (*dvfs_setup)(struct platform_device *pdev);
};

static int meson_g12a_probe(struct platform_device *pdev)
{
	const struct meson_eeclkc_data *eeclkc_data;
	const struct meson_g12a_data *g12a_data;
	struct meson_clk_pll_data *hifi_pll_data;
	int ret;

	eeclkc_data = of_device_get_match_data(&pdev->dev);
	if (!eeclkc_data)
		return -EINVAL;

	/*
	 * HACK: The G12A, G12B, and SM1 share the same driver, and SM1's
	 * hifi_pll_dco has been initialized in the bootloader, and the kernel
	 * does not need to repeat the configuration.
	 */
	if (eeclkc_data->hw_onecell_data == &sm1_hw_onecell_data) {
		hifi_pll_data = (struct meson_clk_pll_data *)g12a_hifi_pll_dco.data;
		hifi_pll_data->flags |= CLK_MESON_PLL_IGNORE_INIT;
	}

	ret = meson_eeclkc_probe(pdev);
	if (ret)
		return ret;

	g12a_data = container_of(eeclkc_data, struct meson_g12a_data,
				 eeclkc_data);

	if (g12a_data->dvfs_setup)
		return g12a_data->dvfs_setup(pdev);

	return 0;
}

static const struct meson_g12a_data g12a_clkc_data = {
	.eeclkc_data = {
		.regmap_clks = g12a_clk_regmaps,
		.regmap_clk_num = ARRAY_SIZE(g12a_clk_regmaps),
		.hw_onecell_data = &g12a_hw_onecell_data,
		.init_regs = g12a_init_regs,
		.init_count = ARRAY_SIZE(g12a_init_regs),
	},
	.dvfs_setup = meson_g12a_dvfs_setup,
};

static const struct meson_g12a_data g12b_clkc_data = {
	.eeclkc_data = {
		.regmap_clks = g12a_clk_regmaps,
		.regmap_clk_num = ARRAY_SIZE(g12a_clk_regmaps),
		.hw_onecell_data = &g12b_hw_onecell_data,
	},
	.dvfs_setup = meson_g12b_dvfs_setup,
};

static const struct meson_g12a_data sm1_clkc_data = {
	.eeclkc_data = {
		.regmap_clks = g12a_clk_regmaps,
		.regmap_clk_num = ARRAY_SIZE(g12a_clk_regmaps),
		.hw_onecell_data = &sm1_hw_onecell_data,
	},
	.dvfs_setup = meson_sm1_dvfs_setup,
};

static const struct of_device_id clkc_match_table[] = {
	{
		.compatible = "amlogic,g12a-clkc",
		.data = &g12a_clkc_data.eeclkc_data
	},
	{
		.compatible = "amlogic,g12b-clkc",
		.data = &g12b_clkc_data.eeclkc_data
	},
	{
		.compatible = "amlogic,sm1-clkc",
		.data = &sm1_clkc_data.eeclkc_data
	},
	{}
};

static struct platform_driver g12a_driver = {
	.probe		= meson_g12a_probe,
	.driver		= {
		.name	= "g12a-clkc",
		.of_match_table = clkc_match_table,
	},
};

#ifndef CONFIG_AMLOGIC_MODIFY
builtin_platform_driver(g12a_driver);
#else
#ifndef MODULE
static int g12a_clkc_init(void)
{
	return platform_driver_register(&g12a_driver);
}
arch_initcall_sync(g12a_clkc_init);
#else
int __init meson_g12a_clkc_init(void)
{
	return platform_driver_register(&g12a_driver);
}
#endif
#endif

MODULE_LICENSE("GPL v2");
