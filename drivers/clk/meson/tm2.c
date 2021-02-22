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
#include "tm2.h"
#include "clkcs_init.h"

static DEFINE_SPINLOCK(meson_clk_lock);

static struct clk_regmap tm2_fixed_pll_dco = {
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

static struct clk_regmap tm2_fixed_pll = {
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
			&tm2_fixed_pll_dco.hw
		},
		.num_parents = 1,
		/*
		 * This clock won't ever change at runtime so
		 * CLK_SET_RATE_PARENT is not required
		 */
	},
};

static const struct pll_mult_range tm2_sys_pll_mult_range = {
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
static const struct pll_params_table tm2_sys_pll_params_table[] = {
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

static struct clk_regmap tm2_sys_pll_dco = {
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
		.table = tm2_sys_pll_params_table,
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
		.range = &tm2_sys_pll_mult_range,
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
static struct clk_regmap tm2_sys_pll = {
	.hw.init = &(struct clk_init_data){
		.name = "sys_pll",
		.ops = &meson_pll_clk_no_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_sys_pll_dco.hw
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
static struct clk_regmap tm2_sys_pll = {
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
			&tm2_sys_pll_dco.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};
#endif

static struct clk_regmap tm2_sys_pll_div16_en = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_SYS_CPU_CLK_CNTL1,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sys_pll_div16_en",
		.ops = &clk_regmap_gate_ro_ops,
		.parent_hws = (const struct clk_hw *[]) { &tm2_sys_pll.hw },
		.num_parents = 1,
		/*
		 * This clock is used to debug the sys_pll range
		 * Linux should not change it at runtime
		 */
	},
};

static struct clk_fixed_factor tm2_sys_pll_div16 = {
	.mult = 1,
	.div = 16,
	.hw.init = &(struct clk_init_data){
		.name = "sys_pll_div16",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_sys_pll_div16_en.hw
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor tm2_fclk_div2_div = {
	.mult = 1,
	.div = 2,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div2_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) { &tm2_fixed_pll.hw },
		.num_parents = 1,
	},
};

static struct clk_regmap tm2_fclk_div2 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_FIX_PLL_CNTL1,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div2",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_fclk_div2_div.hw
		},
		.num_parents = 1,
		/*
		 * This clock feeds on CPU clock, it should be set
		 * by the platform to operate correctly.
		 */
		.flags = CLK_IS_CRITICAL,
	},
};

static struct clk_fixed_factor tm2_fclk_div3_div = {
	.mult = 1,
	.div = 3,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div3_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) { &tm2_fixed_pll.hw },
		.num_parents = 1,
	},
};

static struct clk_regmap tm2_fclk_div3 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_FIX_PLL_CNTL1,
		.bit_idx = 20,
	},
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div3",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_fclk_div3_div.hw
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

static struct clk_fixed_factor tm2_fclk_div4_div = {
	.mult = 1,
	.div = 4,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div4_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) { &tm2_fixed_pll.hw },
		.num_parents = 1,
	},
};

static struct clk_regmap tm2_fclk_div4 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_FIX_PLL_CNTL1,
		.bit_idx = 21,
	},
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div4",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_fclk_div4_div.hw
		},
		.num_parents = 1,
		/*
		 * This clock feeds on GPU, it should be set
		 * by the platform to operate correctly.
		 */
		.flags = CLK_IS_CRITICAL,
	},
};

static struct clk_fixed_factor tm2_fclk_div5_div = {
	.mult = 1,
	.div = 5,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div5_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) { &tm2_fixed_pll.hw },
		.num_parents = 1,
	},
};

static struct clk_regmap tm2_fclk_div5 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_FIX_PLL_CNTL1,
		.bit_idx = 22,
	},
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div5",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_fclk_div5_div.hw
		},
		.num_parents = 1,
		/*
		 * This clock feeds on GPU, it should be set
		 * by the platform to operate correctly.
		 */
		.flags = CLK_IS_CRITICAL,
	},
};

static struct clk_fixed_factor tm2_fclk_div7_div = {
	.mult = 1,
	.div = 7,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div7_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) { &tm2_fixed_pll.hw },
		.num_parents = 1,
	},
};

static struct clk_regmap tm2_fclk_div7 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_FIX_PLL_CNTL1,
		.bit_idx = 23,
	},
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div7",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_fclk_div7_div.hw
		},
		.num_parents = 1,
		/*
		 * This clock feeds on GPU, it should be set
		 * by the platform to operate correctly.
		 */
		.flags = CLK_IS_CRITICAL,
	},
};

static struct clk_fixed_factor tm2_fclk_div2p5_div = {
	.mult = 1,
	.div = 5,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div2p5_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_fixed_pll_dco.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap tm2_fclk_div2p5 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_FIX_PLL_CNTL1,
		.bit_idx = 25,
	},
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div2p5",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_fclk_div2p5_div.hw
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
static const struct pll_params_table tm2_gp0_pll_table[] = {
	PLL_PARAMS(141, 1, 2), /* DCO = 3384M OD = 2 PLL = 846M */
	PLL_PARAMS(132, 1, 2), /* DCO = 3168M OD = 2 PLL = 792M */
	PLL_PARAMS(248, 1, 3), /* DCO = 5952M OD = 3 PLL = 744M */
	{ /* sentinel */  },
};
#else
static const struct pll_params_table tm2_gp0_pll_table[] = {
	PLL_PARAMS(141, 1), /* DCO = 3384M OD = 2 PLL = 846M*/
	PLL_PARAMS(132, 1), /* DCO = 3168M OD = 2 PLL = 792M */
	PLL_PARAMS(248, 1), /* DCO = 5952M OD = 3 PLL = 744M */
	{0, 0},
};
#endif

/*
 * Internal gp0 pll emulation configuration parameters
 */
static const struct reg_sequence tm2_gp0_init_regs[] = {
	{ .reg = HHI_GP0_PLL_CNTL1,	.def = 0x00000000 },
	{ .reg = HHI_GP0_PLL_CNTL2,	.def = 0x00000000 },
	{ .reg = HHI_GP0_PLL_CNTL3,	.def = 0x48681c00 },
	{ .reg = HHI_GP0_PLL_CNTL4,	.def = 0x33771290 },
	{ .reg = HHI_GP0_PLL_CNTL5,	.def = 0x39272000 },
	{ .reg = HHI_GP0_PLL_CNTL6,	.def = 0x56540000 },
};

static struct clk_regmap tm2_gp0_pll_dco = {
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
		.table = tm2_gp0_pll_table,
		.init_regs = tm2_gp0_init_regs,
		.init_count = ARRAY_SIZE(tm2_gp0_init_regs),
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
static struct clk_regmap tm2_gp0_pll = {
	.hw.init = &(struct clk_init_data){
		.name = "gp0_pll",
		.ops = &meson_pll_clk_no_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_gp0_pll_dco.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};
#else
static struct clk_regmap tm2_gp0_pll = {
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
			&tm2_gp0_pll_dco.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};
#endif

static const struct pll_mult_range tm2_gp1_pll_mult_range = {
	.min = 128,
	.max = 250,
};

static struct clk_regmap tm2_gp1_pll_dco = {
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
	},
	.hw.init = &(struct clk_init_data){
		.name = "gp1_pll_dco",
		.ops = &meson_clk_pll_ro_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "xtal",
		},
		.num_parents = 1,
		/* This clock feeds the DSU, avoid disabling it */
		.flags = CLK_IS_CRITICAL,
	},
};

static struct clk_regmap tm2_gp1_pll = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_GP1_PLL_CNTL0,
		.shift = 16,
		.width = 3,
		.flags = (CLK_DIVIDER_POWER_OF_TWO |
			  CLK_DIVIDER_ROUND_CLOSEST),
	},
	.hw.init = &(struct clk_init_data){
		.name = "gp1_pll",
		.ops = &clk_regmap_divider_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_gp1_pll_dco.hw
		},
		.num_parents = 1,
	},
};

/*
 * Internal hifi pll emulation configuration parameters
 */
static const struct reg_sequence tm2_hifi_init_regs[] = {
	{ .reg = HHI_HIFI_PLL_CNTL1,	.def = 0x00000000 },
	{ .reg = HHI_HIFI_PLL_CNTL2,	.def = 0x00000000 },
	{ .reg = HHI_HIFI_PLL_CNTL3,	.def = 0x6a285c00 },
	{ .reg = HHI_HIFI_PLL_CNTL4,	.def = 0x65771290 },
	{ .reg = HHI_HIFI_PLL_CNTL5,	.def = 0x39272000 },
	{ .reg = HHI_HIFI_PLL_CNTL6,	.def = 0x56540000 },
};

static struct clk_regmap tm2_hifi_pll_dco = {
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
		.range = &tm2_gp1_pll_mult_range,
		.init_regs = tm2_hifi_init_regs,
		.init_count = ARRAY_SIZE(tm2_hifi_init_regs),
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

static struct clk_regmap tm2_hifi_pll = {
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
			&tm2_hifi_pll_dco.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/*
 * The Meson G12A PCIE PLL is fined tuned to deliver a very precise
 * 100MHz reference clock for the PCIe Analog PHY, and thus requires
 * a strict register sequence to enable the PLL.
 */
static const struct reg_sequence tm2_pcie_pll_init_regs[] = {
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
static const struct pll_params_table tm2_pcie_pll_table[] = {
	PLL_PARAMS(200, 1),
	{0, 0},
};
#else
static const struct pll_params_table tm2_pcie_pll_table[] = {
	PLL_PARAMS(200, 1, 0),
	{0, 0, 0},
};
#endif

static struct clk_regmap tm2_pcie_pll_dco = {
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
		.table = tm2_pcie_pll_table,
		.init_regs = tm2_pcie_pll_init_regs,
		.init_count = ARRAY_SIZE(tm2_pcie_pll_init_regs),
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

static struct clk_fixed_factor tm2_pcie_pll_dco_div2 = {
	.mult = 1,
	.div = 2,/* pcie dco internal div by 2 */
	.hw.init = &(struct clk_init_data){
		.name = "pcie_pll_dco_div2",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_pcie_pll_dco.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tm2_pcie_pll_od = {
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
			&tm2_pcie_pll_dco_div2.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_fixed_factor tm2_pcie_pll = {
	.mult = 1,
	.div = 2, /* pcie pll internal div by 2*/
	.hw.init = &(struct clk_init_data){
		.name = "pcie_pll",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_pcie_pll_od.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/*
 * pcie pll--- pre GATE---pcie0
 *               |
 *                ---pcie1
 */
static struct clk_regmap tm2_pcie_pre_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_PCIE_PLL_CNTL1,
		.bit_idx = 29,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pcie_pre_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_pcie_pll.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tm2_pcie_hcsl0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_PCIE_PLL_CNTL5,
		.bit_idx = 3,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pcie_hcsl0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_pcie_pre_gate.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tm2_pcie_hcsl1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_PCIE_PLL_CNTL1,
		.bit_idx = 28,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pcie_hcsl1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_pcie_pre_gate.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_fixed_factor tm2_mpll_50m_div = {
	.mult = 1,
	.div = 80,
	.hw.init = &(struct clk_init_data){
		.name = "mpll_50m_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_fixed_pll_dco.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap tm2_mpll_50m = {
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
			{ .hw = &tm2_mpll_50m_div.hw },
		},
		.num_parents = 2,
	},
};

static struct clk_fixed_factor tm2_mpll_prediv = {
	.mult = 1,
	.div = 2,
	.hw.init = &(struct clk_init_data){
		.name = "mpll_prediv",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_fixed_pll_dco.hw
		},
		.num_parents = 1,
	},
};

/*
 * put it in init regs, in kernel 4.9 it is dealing in set rate
 */
static const struct reg_sequence tm2_mpll0_init_regs[] = {
	{ .reg = HHI_MPLL_CNTL0,	.def = 0x00000543 },
	{ .reg = HHI_MPLL_CNTL2,	.def = 0x40000033 },
};

static struct clk_regmap tm2_mpll0_div = {
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
		.init_regs = tm2_mpll0_init_regs,
		.init_count = ARRAY_SIZE(tm2_mpll0_init_regs),
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpll0_div",
		.ops = &meson_clk_mpll_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_mpll_prediv.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap tm2_mpll0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_MPLL_CNTL1,
		.bit_idx = 31,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpll0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &tm2_mpll0_div.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct reg_sequence tm2_mpll1_init_regs[] = {
	{ .reg = HHI_MPLL_CNTL4,	.def = 0x40000033 },
};

static struct clk_regmap tm2_mpll1_div = {
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
		.init_regs = tm2_mpll1_init_regs,
		.init_count = ARRAY_SIZE(tm2_mpll1_init_regs),
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpll1_div",
		.ops = &meson_clk_mpll_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_mpll_prediv.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap tm2_mpll1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_MPLL_CNTL3,
		.bit_idx = 31,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpll1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &tm2_mpll1_div.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct reg_sequence tm2_mpll2_init_regs[] = {
	{ .reg = HHI_MPLL_CNTL6,	.def = 0x40000033 },
};

static struct clk_regmap tm2_mpll2_div = {
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
		.init_regs = tm2_mpll2_init_regs,
		.init_count = ARRAY_SIZE(tm2_mpll2_init_regs),
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpll2_div",
		.ops = &meson_clk_mpll_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_mpll_prediv.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap tm2_mpll2 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_MPLL_CNTL5,
		.bit_idx = 31,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpll2",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &tm2_mpll2_div.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct reg_sequence tm2_mpll3_init_regs[] = {
	{ .reg = HHI_MPLL_CNTL8,	.def = 0x40000033 },
};

static struct clk_regmap tm2_mpll3_div = {
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
		.init_regs = tm2_mpll3_init_regs,
		.init_count = ARRAY_SIZE(tm2_mpll3_init_regs),
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpll3_div",
		.ops = &meson_clk_mpll_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_mpll_prediv.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap tm2_mpll3 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_MPLL_CNTL7,
		.bit_idx = 31,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpll3",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &tm2_mpll3_div.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static u32 mux_table_clk81[]	= { 0, 2, 3, 4, 5, 6, 7 };
static const struct clk_parent_data clk81_parent_data[] = {
	{ .fw_name = "xtal", },
	{ .hw = &tm2_fclk_div7.hw },
	{ .hw = &tm2_mpll1.hw },
	{ .hw = &tm2_mpll2.hw },
	{ .hw = &tm2_fclk_div4.hw },
	{ .hw = &tm2_fclk_div3.hw },
	{ .hw = &tm2_fclk_div5.hw },
};

static struct clk_regmap tm2_mpeg_clk_sel = {
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

static struct clk_regmap tm2_mpeg_clk_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_MPEG_CLK_CNTL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpeg_clk_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_mpeg_clk_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tm2_clk81 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_MPEG_CLK_CNTL,
		.bit_idx = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "clk81",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_mpeg_clk_div.hw
		},
		.num_parents = 1,
		.flags = (CLK_SET_RATE_PARENT | CLK_IS_CRITICAL),
	},
};

static const struct clk_parent_data tm2_sd_emmc_clk0_parent_data[] = {
	{ .fw_name = "xtal", },
	{ .hw = &tm2_fclk_div2.hw },
	{ .hw = &tm2_fclk_div3.hw },
	{ .hw = &tm2_fclk_div5.hw },
	{ .hw = &tm2_fclk_div2p5.hw },
	/*
	 * Following these parent clocks, we should also have had mpll2, mpll3
	 * and gp0_pll but these clocks are too precious to be used here. All
	 * the necessary rates for MMC and NAND operation can be acheived using
	 * tm2_ee_core or fclk_div clocks
	 */
};

/* SDcard clock */
static struct clk_regmap tm2_sd_emmc_b_clk0_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_SD_EMMC_CLK_CNTL,
		.mask = 0x7,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_b_clk0_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = tm2_sd_emmc_clk0_parent_data,
		.num_parents = ARRAY_SIZE(tm2_sd_emmc_clk0_parent_data),
	},
};

static struct clk_regmap tm2_sd_emmc_b_clk0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_SD_EMMC_CLK_CNTL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_b_clk0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_sd_emmc_b_clk0_sel.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap tm2_sd_emmc_b_clk0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_SD_EMMC_CLK_CNTL,
		.bit_idx = 23,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sd_emmc_b_clk0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_sd_emmc_b_clk0_div.hw
		},
		.num_parents = 1,
	},
};

/* EMMC/NAND clock */
static struct clk_regmap tm2_sd_emmc_c_clk0_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_NAND_CLK_CNTL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_c_clk0_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = tm2_sd_emmc_clk0_parent_data,
		.num_parents = ARRAY_SIZE(tm2_sd_emmc_clk0_parent_data),
	},
};

static struct clk_regmap tm2_sd_emmc_c_clk0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_NAND_CLK_CNTL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_c_clk0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_sd_emmc_c_clk0_sel.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap tm2_sd_emmc_c_clk0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_NAND_CLK_CNTL,
		.bit_idx = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sd_emmc_c_clk0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_sd_emmc_c_clk0_div.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap tm2_hdmi_pll_dco = {
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

static struct clk_regmap tm2_hdmi_pll_od = {
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
			&tm2_hdmi_pll_dco.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tm2_hdmi_pll_od2 = {
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
			&tm2_hdmi_pll_od.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tm2_hdmi_pll = {
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
			&tm2_hdmi_pll_od2.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

/* Video Clocks */
static struct clk_regmap tm2_vid_pll_div = {
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
		.parent_hws = (const struct clk_hw *[]) { &tm2_hdmi_pll.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
	},
};

static const struct clk_hw *tm2_vid_pll_parent_hws[] = {
	&tm2_vid_pll_div.hw,
	&tm2_hdmi_pll.hw,
};

static struct clk_regmap tm2_vid_pll_sel = {
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
		.parent_hws = tm2_vid_pll_parent_hws,
		.num_parents = ARRAY_SIZE(tm2_vid_pll_parent_hws),
		.flags = CLK_SET_RATE_NO_REPARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap tm2_vid_pll = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VID_PLL_CLK_DIV,
		.bit_idx = 19,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vid_pll",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_vid_pll_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

/* VPU Clock */

static const struct clk_hw *tm2_vpu_parent_hws[] = {
	&tm2_fclk_div3.hw,
	&tm2_fclk_div4.hw,
	&tm2_fclk_div5.hw,
	&tm2_fclk_div7.hw,
	&tm2_mpll1.hw,
	&tm2_vid_pll.hw,
	&tm2_hifi_pll.hw,
	&tm2_gp0_pll.hw,
};

static struct clk_regmap tm2_vpu_0_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VPU_CLK_CNTL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vpu_0_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = tm2_vpu_parent_hws,
		.num_parents = ARRAY_SIZE(tm2_vpu_parent_hws),
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tm2_vpu_0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VPU_CLK_CNTL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vpu_0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) { &tm2_vpu_0_sel.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tm2_vpu_0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VPU_CLK_CNTL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &tm2_vpu_0_div.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap tm2_vpu_1_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VPU_CLK_CNTL,
		.mask = 0x7,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vpu_1_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = tm2_vpu_parent_hws,
		.num_parents = ARRAY_SIZE(tm2_vpu_parent_hws),
		.flags = CLK_SET_RATE_NO_REPARENT,
	},
};

static struct clk_regmap tm2_vpu_1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VPU_CLK_CNTL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vpu_1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) { &tm2_vpu_1_sel.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tm2_vpu_1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VPU_CLK_CNTL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &tm2_vpu_1_div.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap tm2_vpu = {
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
			&tm2_vpu_0.hw,
			&tm2_vpu_1.hw,
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_NO_REPARENT,
	},
};

/* VDEC clocks */

static const struct clk_hw *tm2_vdec_parent_hws[] = {
	&tm2_fclk_div2p5.hw,
	&tm2_fclk_div3.hw,
	&tm2_fclk_div4.hw,
	&tm2_fclk_div5.hw,
	&tm2_fclk_div7.hw,
	&tm2_hifi_pll.hw,
	&tm2_gp0_pll.hw,
};

static struct clk_regmap tm2_vdec_1_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VDEC_CLK_CNTL,
		.mask = 0x7,
		.shift = 9,
		.flags = CLK_MUX_ROUND_CLOSEST,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vdec_1_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = tm2_vdec_parent_hws,
		.num_parents = ARRAY_SIZE(tm2_vdec_parent_hws),
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tm2_vdec_1_div = {
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
			&tm2_vdec_1_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tm2_vdec_1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VDEC_CLK_CNTL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vdec_1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_vdec_1_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tm2_vdec_hevcf_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VDEC2_CLK_CNTL,
		.mask = 0x7,
		.shift = 9,
		.flags = CLK_MUX_ROUND_CLOSEST,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vdec_hevcf_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = tm2_vdec_parent_hws,
		.num_parents = ARRAY_SIZE(tm2_vdec_parent_hws),
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tm2_vdec_hevcf_div = {
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
			&tm2_vdec_hevcf_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tm2_vdec_hevcf = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VDEC2_CLK_CNTL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vdec_hevcf",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_vdec_hevcf_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tm2_vdec_hevc_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VDEC2_CLK_CNTL,
		.mask = 0x7,
		.shift = 25,
		.flags = CLK_MUX_ROUND_CLOSEST,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vdec_hevc_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = tm2_vdec_parent_hws,
		.num_parents = ARRAY_SIZE(tm2_vdec_parent_hws),
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tm2_vdec_hevc_div = {
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
			&tm2_vdec_hevc_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tm2_vdec_hevc = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VDEC2_CLK_CNTL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vdec_hevc",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_vdec_hevc_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/* VAPB Clock */

static const struct clk_hw *tm2_vapb_parent_hws[] = {
	&tm2_fclk_div4.hw,
	&tm2_fclk_div3.hw,
	&tm2_fclk_div5.hw,
	&tm2_fclk_div7.hw,
	&tm2_mpll1.hw,
	&tm2_vid_pll.hw,
	&tm2_mpll2.hw,
	&tm2_fclk_div2p5.hw,
};

static struct clk_regmap tm2_vapb_0_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VAPBCLK_CNTL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vapb_0_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = tm2_vapb_parent_hws,
		.num_parents = ARRAY_SIZE(tm2_vapb_parent_hws),
		.flags = CLK_SET_RATE_NO_REPARENT,
	},
};

static struct clk_regmap tm2_vapb_0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VAPBCLK_CNTL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vapb_0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_vapb_0_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tm2_vapb_0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VAPBCLK_CNTL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vapb_0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_vapb_0_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap tm2_vapb_1_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VAPBCLK_CNTL,
		.mask = 0x3,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vapb_1_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = tm2_vapb_parent_hws,
		.num_parents = ARRAY_SIZE(tm2_vapb_parent_hws),
		.flags = CLK_SET_RATE_NO_REPARENT,
	},
};

static struct clk_regmap tm2_vapb_1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VAPBCLK_CNTL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vapb_1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_vapb_1_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tm2_vapb_1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VAPBCLK_CNTL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vapb_1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_vapb_1_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap tm2_vapb_sel = {
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
			&tm2_vapb_0.hw,
			&tm2_vapb_1.hw,
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_NO_REPARENT | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tm2_vapb = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VAPBCLK_CNTL,
		.bit_idx = 30,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vapb",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &tm2_vapb_sel.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static const struct clk_hw *tm2_vclk_parent_hws[] = {
	&tm2_vid_pll.hw,
	&tm2_gp0_pll.hw,
	&tm2_hifi_pll.hw,
	&tm2_mpll1.hw,
	&tm2_fclk_div3.hw,
	&tm2_fclk_div4.hw,
	&tm2_fclk_div5.hw,
	&tm2_fclk_div7.hw,
};

static struct clk_regmap tm2_vclk_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VID_CLK_CNTL,
		.mask = 0x7,
		.shift = 16,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vclk_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = tm2_vclk_parent_hws,
		.num_parents = ARRAY_SIZE(tm2_vclk_parent_hws),
		.flags = CLK_SET_RATE_NO_REPARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap tm2_vclk2_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VIID_CLK_CNTL,
		.mask = 0x7,
		.shift = 16,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vclk2_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = tm2_vclk_parent_hws,
		.num_parents = ARRAY_SIZE(tm2_vclk_parent_hws),
		.flags = CLK_SET_RATE_NO_REPARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap tm2_vclk_input = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VID_CLK_DIV,
		.bit_idx = 16,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk_input",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &tm2_vclk_sel.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap tm2_vclk2_input = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VIID_CLK_DIV,
		.bit_idx = 16,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk2_input",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &tm2_vclk2_sel.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap tm2_vclk_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VID_CLK_DIV,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vclk_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_vclk_input.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap tm2_vclk2_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VIID_CLK_DIV,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vclk2_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_vclk2_input.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap tm2_vclk = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VID_CLK_CNTL,
		.bit_idx = 19,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &tm2_vclk_div.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap tm2_vclk2 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VIID_CLK_CNTL,
		.bit_idx = 19,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk2",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &tm2_vclk2_div.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap tm2_vclk_div1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VID_CLK_CNTL,
		.bit_idx = 0,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk_div1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &tm2_vclk.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap tm2_vclk_div2_en = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VID_CLK_CNTL,
		.bit_idx = 1,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk_div2_en",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &tm2_vclk.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap tm2_vclk_div4_en = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VID_CLK_CNTL,
		.bit_idx = 2,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk_div4_en",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &tm2_vclk.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap tm2_vclk_div6_en = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VID_CLK_CNTL,
		.bit_idx = 3,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk_div6_en",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &tm2_vclk.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap tm2_vclk_div12_en = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VID_CLK_CNTL,
		.bit_idx = 4,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk_div12_en",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &tm2_vclk.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap tm2_vclk2_div1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VIID_CLK_CNTL,
		.bit_idx = 0,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk2_div1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &tm2_vclk2.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap tm2_vclk2_div2_en = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VIID_CLK_CNTL,
		.bit_idx = 1,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk2_div2_en",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &tm2_vclk2.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap tm2_vclk2_div4_en = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VIID_CLK_CNTL,
		.bit_idx = 2,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk2_div4_en",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &tm2_vclk2.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap tm2_vclk2_div6_en = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VIID_CLK_CNTL,
		.bit_idx = 3,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk2_div6_en",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &tm2_vclk2.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap tm2_vclk2_div12_en = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VIID_CLK_CNTL,
		.bit_idx = 4,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk2_div12_en",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &tm2_vclk2.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_fixed_factor tm2_vclk_div2 = {
	.mult = 1,
	.div = 2,
	.hw.init = &(struct clk_init_data){
		.name = "vclk_div2",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_vclk_div2_en.hw
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor tm2_vclk_div4 = {
	.mult = 1,
	.div = 4,
	.hw.init = &(struct clk_init_data){
		.name = "vclk_div4",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_vclk_div4_en.hw
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor tm2_vclk_div6 = {
	.mult = 1,
	.div = 6,
	.hw.init = &(struct clk_init_data){
		.name = "vclk_div6",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_vclk_div6_en.hw
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor tm2_vclk_div12 = {
	.mult = 1,
	.div = 12,
	.hw.init = &(struct clk_init_data){
		.name = "vclk_div12",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_vclk_div12_en.hw
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor tm2_vclk2_div2 = {
	.mult = 1,
	.div = 2,
	.hw.init = &(struct clk_init_data){
		.name = "vclk2_div2",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_vclk2_div2_en.hw
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor tm2_vclk2_div4 = {
	.mult = 1,
	.div = 4,
	.hw.init = &(struct clk_init_data){
		.name = "vclk2_div4",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_vclk2_div4_en.hw
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor tm2_vclk2_div6 = {
	.mult = 1,
	.div = 6,
	.hw.init = &(struct clk_init_data){
		.name = "vclk2_div6",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_vclk2_div6_en.hw
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor tm2_vclk2_div12 = {
	.mult = 1,
	.div = 12,
	.hw.init = &(struct clk_init_data){
		.name = "vclk2_div12",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_vclk2_div12_en.hw
		},
		.num_parents = 1,
	},
};

static u32 mux_table_cts_sel[] = { 0, 1, 2, 3, 4, 8, 9, 10, 11, 12 };
static const struct clk_hw *tm2_cts_parent_hws[] = {
	&tm2_vclk_div1.hw,
	&tm2_vclk_div2.hw,
	&tm2_vclk_div4.hw,
	&tm2_vclk_div6.hw,
	&tm2_vclk_div12.hw,
	&tm2_vclk2_div1.hw,
	&tm2_vclk2_div2.hw,
	&tm2_vclk2_div4.hw,
	&tm2_vclk2_div6.hw,
	&tm2_vclk2_div12.hw,
};

static struct clk_regmap tm2_cts_enci_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VID_CLK_DIV,
		.mask = 0xf,
		.shift = 28,
		.table = mux_table_cts_sel,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cts_enci_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = tm2_cts_parent_hws,
		.num_parents = ARRAY_SIZE(tm2_cts_parent_hws),
		.flags = CLK_SET_RATE_NO_REPARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap tm2_cts_encp_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VID_CLK_DIV,
		.mask = 0xf,
		.shift = 20,
		.table = mux_table_cts_sel,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cts_encp_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = tm2_cts_parent_hws,
		.num_parents = ARRAY_SIZE(tm2_cts_parent_hws),
		.flags = CLK_SET_RATE_NO_REPARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap tm2_cts_vdac_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VIID_CLK_DIV,
		.mask = 0xf,
		.shift = 28,
		.table = mux_table_cts_sel,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cts_vdac_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = tm2_cts_parent_hws,
		.num_parents = ARRAY_SIZE(tm2_cts_parent_hws),
		.flags = CLK_SET_RATE_NO_REPARENT | CLK_GET_RATE_NOCACHE,
	},
};

/* TOFIX: add support for cts_tcon */
static u32 mux_table_hdmi_tx_sel[] = { 0, 1, 2, 3, 4, 8, 9, 10, 11, 12 };
static const struct clk_hw *tm2_cts_hdmi_tx_parent_hws[] = {
	&tm2_vclk_div1.hw,
	&tm2_vclk_div2.hw,
	&tm2_vclk_div4.hw,
	&tm2_vclk_div6.hw,
	&tm2_vclk_div12.hw,
	&tm2_vclk2_div1.hw,
	&tm2_vclk2_div2.hw,
	&tm2_vclk2_div4.hw,
	&tm2_vclk2_div6.hw,
	&tm2_vclk2_div12.hw,
};

static struct clk_regmap tm2_hdmi_tx_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_HDMI_CLK_CNTL,
		.mask = 0xf,
		.shift = 16,
		.table = mux_table_hdmi_tx_sel,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmi_tx_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = tm2_cts_hdmi_tx_parent_hws,
		.num_parents = ARRAY_SIZE(tm2_cts_hdmi_tx_parent_hws),
		.flags = CLK_SET_RATE_NO_REPARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap tm2_cts_enci = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VID_CLK_CNTL2,
		.bit_idx = 0,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "cts_enci",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_cts_enci_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap tm2_cts_encp = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VID_CLK_CNTL2,
		.bit_idx = 2,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "cts_encp",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_cts_encp_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap tm2_cts_vdac = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VID_CLK_CNTL2,
		.bit_idx = 4,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "cts_vdac",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_cts_vdac_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap tm2_hdmi_tx = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VID_CLK_CNTL2,
		.bit_idx = 5,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmi_tx",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_hdmi_tx_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

/* HDMI Clocks */

static const struct clk_parent_data tm2_hdmi_parent_data[] = {
	{ .fw_name = "xtal", },
	{ .hw = &tm2_fclk_div4.hw },
	{ .hw = &tm2_fclk_div3.hw },
	{ .hw = &tm2_fclk_div5.hw },
};

static struct clk_regmap tm2_hdmi_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_HDMI_CLK_CNTL,
		.mask = 0x3,
		.shift = 9,
		.flags = CLK_MUX_ROUND_CLOSEST,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmi_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = tm2_hdmi_parent_data,
		.num_parents = ARRAY_SIZE(tm2_hdmi_parent_data),
		.flags = CLK_SET_RATE_NO_REPARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap tm2_hdmi_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_HDMI_CLK_CNTL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmi_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) { &tm2_hdmi_sel.hw },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap tm2_hdmi = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_HDMI_CLK_CNTL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmi",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &tm2_hdmi_div.hw },
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

static const struct clk_parent_data tm2_mali_0_1_parent_data[] = {
	{ .fw_name = "xtal", },
	{ .hw = &tm2_gp0_pll.hw },
	{ .hw = &tm2_fclk_div2p5.hw },
	{ .hw = &tm2_fclk_div3.hw },
	{ .hw = &tm2_fclk_div4.hw },
	{ .hw = &tm2_fclk_div5.hw },
	{ .hw = &tm2_fclk_div7.hw },
};

static struct clk_regmap tm2_mali_0_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_MALI_CLK_CNTL,
		.mask = 0x7,
		.shift = 9,
		.table = mux_table_mali,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mali_0_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = tm2_mali_0_1_parent_data,
		.num_parents = ARRAY_SIZE(tm2_mali_0_1_parent_data),
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tm2_mali_0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_MALI_CLK_CNTL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mali_0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_mali_0_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tm2_mali_0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_MALI_CLK_CNTL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mali_0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_mali_0_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_GATE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tm2_mali_1_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_MALI_CLK_CNTL,
		.mask = 0x7,
		.shift = 25,
		.table = mux_table_mali,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mali_1_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = tm2_mali_0_1_parent_data,
		.num_parents = ARRAY_SIZE(tm2_mali_0_1_parent_data),
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tm2_mali_1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_MALI_CLK_CNTL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mali_1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_mali_1_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tm2_mali_1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_MALI_CLK_CNTL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mali_1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_mali_1_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_GATE | CLK_SET_RATE_PARENT,
	},
};

static const struct clk_hw *tm2_mali_parent_hws[] = {
	&tm2_mali_0.hw,
	&tm2_mali_1.hw,
};

static struct clk_regmap tm2_mali = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_MALI_CLK_CNTL,
		.mask = 1,
		.shift = 31,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mali",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = tm2_mali_parent_hws,
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tm2_ts_div = {
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

static struct clk_regmap tm2_ts = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_TS_CLK_CNTL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "ts",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_ts_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/*
 * media clocks
 */

/* cts_vdec_clk */
static const struct clk_parent_data tm2_dec_parent_hws[] = {
	{ .hw = &tm2_fclk_div2p5.hw },
	{ .hw = &tm2_fclk_div3.hw },
	{ .hw = &tm2_fclk_div4.hw },
	{ .hw = &tm2_fclk_div5.hw },
	{ .hw = &tm2_fclk_div7.hw },
	{ .hw = &tm2_hifi_pll.hw },
	{ .hw = &tm2_gp0_pll.hw },
	{ .fw_name = "xtal", },
};

static struct clk_regmap tm2_vdec_p1_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VDEC3_CLK_CNTL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vdec_p1_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = tm2_dec_parent_hws,
		.num_parents = ARRAY_SIZE(tm2_dec_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap tm2_vdec_p1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VDEC3_CLK_CNTL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vdec_p1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_vdec_p1_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tm2_vdec_p1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VDEC3_CLK_CNTL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vdec_p1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_vdec_p1_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

/* cts_hcodec_clk */
static struct clk_regmap tm2_hcodec_p0_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VDEC_CLK_CNTL,
		.mask = 0x7,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hcodec_p0_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = tm2_dec_parent_hws,
		.num_parents = ARRAY_SIZE(tm2_dec_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap tm2_hcodec_p0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VDEC_CLK_CNTL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hcodec_p0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_hcodec_p0_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tm2_hcodec_p0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VDEC_CLK_CNTL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hcodec_p0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_hcodec_p0_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tm2_hcodec_p1_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VDEC3_CLK_CNTL,
		.mask = 0x7,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hcodec_p1_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = tm2_dec_parent_hws,
		.num_parents = ARRAY_SIZE(tm2_dec_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap tm2_hcodec_p1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VDEC3_CLK_CNTL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hcodec_p1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_hcodec_p1_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tm2_hcodec_p1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VDEC3_CLK_CNTL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hcodec_p1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_hcodec_p1_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data tm2_hcodec_mux_parent_hws[] = {
	{ .hw = &tm2_hcodec_p0.hw },
	{ .hw = &tm2_hcodec_p1.hw },
};

static struct clk_regmap tm2_hcodec_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VDEC3_CLK_CNTL,
		.mask = 0x1,
		.shift = 31,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hcodec_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = tm2_hcodec_mux_parent_hws,
		.num_parents = ARRAY_SIZE(tm2_hcodec_mux_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

/* cts_hevcb_clk */

static struct clk_regmap tm2_hevc_p1_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VDEC4_CLK_CNTL,
		.mask = 0x7,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hevc_p1_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = tm2_dec_parent_hws,
		.num_parents = ARRAY_SIZE(tm2_dec_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap tm2_hevc_p1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VDEC4_CLK_CNTL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hevc_p1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_hevc_p1_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tm2_hevc_p1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VDEC4_CLK_CNTL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hevc_p1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_hevc_p1_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

/* cts_hevcf_clk */

static struct clk_regmap tm2_hevcf_p1_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VDEC4_CLK_CNTL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hevcf_p1_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = tm2_dec_parent_hws,
		.num_parents = ARRAY_SIZE(tm2_dec_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap tm2_hevcf_p1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VDEC4_CLK_CNTL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hevcf_p1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_hevcf_p1_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tm2_hevcf_p1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VDEC4_CLK_CNTL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hevcf_p1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_hevcf_p1_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data vpu_parent_hws[] = {
	{ .hw = &tm2_fclk_div4.hw },
	{ .hw = &tm2_fclk_div3.hw },
	{ .hw = &tm2_fclk_div5.hw },
	{ .hw = &tm2_fclk_div7.hw },
};

static struct clk_regmap tm2_vpu_clkc_p0_mux  = {
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

static struct clk_regmap tm2_vpu_clkc_p0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VPU_CLKC_CNTL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_clkc_p0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_vpu_clkc_p0_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tm2_vpu_clkc_p0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VPU_CLKC_CNTL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vpu_clkc_p0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_vpu_clkc_p0_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tm2_vpu_clkc_p1_mux = {
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

static struct clk_regmap tm2_vpu_clkc_p1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VPU_CLKC_CNTL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_clkc_p1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_vpu_clkc_p1_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tm2_vpu_clkc_p1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VPU_CLKC_CNTL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vpu_clkc_p1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_vpu_clkc_p1_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data tm2_vpu_mux_parent_hws[] = {
	{ .hw = &tm2_vpu_clkc_p0.hw },
	{ .hw = &tm2_vpu_clkc_p1.hw },
};

static struct clk_regmap tm2_vpu_clkc_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VPU_CLKC_CNTL,
		.mask = 0x1,
		.shift = 31,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_clkc_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = tm2_vpu_mux_parent_hws,
		.num_parents = ARRAY_SIZE(tm2_vpu_mux_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data tm2_meas_parent_hws[] = {
	{ .fw_name = "xtal", },
	{ .hw = &tm2_fclk_div4.hw },
	{ .hw = &tm2_fclk_div3.hw },
	{ .hw = &tm2_fclk_div5.hw },
	{ .hw = &tm2_vid_pll.hw },
	{ .hw = &tm2_gp0_pll.hw },
	{ .hw = &tm2_fclk_div2.hw },
	{ .hw = &tm2_fclk_div7.hw },
};

static struct clk_regmap tm2_dsi_meas_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VDIN_MEAS_CLK_CNTL,
		.mask = 0x7,
		.shift = 21,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "dsi_meas_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = tm2_meas_parent_hws,
		.num_parents = ARRAY_SIZE(tm2_meas_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap tm2_dsi_meas_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VDIN_MEAS_CLK_CNTL,
		.shift = 12,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "dsi_meas_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_dsi_meas_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tm2_dsi_meas = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VDIN_MEAS_CLK_CNTL,
		.bit_idx = 20,
	},
	.hw.init = &(struct clk_init_data){
		.name = "dsi_meas",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_dsi_meas_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data tm2_vdin_parent_hws[] = {
	{ .fw_name = "xtal", },
	{ .hw = &tm2_fclk_div4.hw },
	{ .hw = &tm2_fclk_div3.hw },
	{ .hw = &tm2_fclk_div5.hw },
	{ .hw = &tm2_vid_pll.hw }
};

static struct clk_regmap tm2_vdin_meas_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VDIN_MEAS_CLK_CNTL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vdin_meas_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = tm2_vdin_parent_hws,
		.num_parents = ARRAY_SIZE(tm2_vdin_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap tm2_vdin_meas_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VDIN_MEAS_CLK_CNTL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vdin_meas_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_vdin_meas_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tm2_vdin_meas = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VDIN_MEAS_CLK_CNTL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vdin_meas",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_vdin_meas_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data tm2_vdec_mux_parent_hws[] = {
	{ .hw = &tm2_vdec_1.hw },
	{ .hw = &tm2_vdec_p1.hw },
};

static struct clk_regmap tm2_vdec_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VDEC3_CLK_CNTL,
		.mask = 0x1,
		.shift = 15,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vdec_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = tm2_vdec_mux_parent_hws,
		.num_parents = ARRAY_SIZE(tm2_vdec_mux_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data tm2_hevc_mux_parent_hws[] = {
	{ .hw = &tm2_vdec_hevc.hw },
	{ .hw = &tm2_hevc_p1.hw },
};

static struct clk_regmap tm2_hevc_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VDEC4_CLK_CNTL,
		.mask = 0x1,
		.shift = 31,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hevc_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = tm2_hevc_mux_parent_hws,
		.num_parents = ARRAY_SIZE(tm2_hevc_mux_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data tm2_hevcf_mux_parent_hws[] = {
	{ .hw = &tm2_vdec_hevcf.hw },
	{ .hw = &tm2_hevcf_p1.hw },
};

static struct clk_regmap tm2_hevcf_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VDEC4_CLK_CNTL,
		.mask = 0x1,
		.shift = 15,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hevcf_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = tm2_hevcf_mux_parent_hws,
		.num_parents = ARRAY_SIZE(tm2_hevcf_mux_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data vpu_clkb_tmp_parent_hws[] = {
	{ .hw = &tm2_vpu.hw },
	{ .hw = &tm2_fclk_div4.hw },
	{ .hw = &tm2_fclk_div5.hw },
	{ .hw = &tm2_fclk_div7.hw },
};

static struct clk_regmap tm2_vpu_clkb_tmp_mux = {
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

static struct clk_regmap tm2_vpu_clkb_tmp_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VPU_CLKB_CNTL,
		.shift = 16,
		.width = 4,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_clkb_tmp_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_vpu_clkb_tmp_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tm2_vpu_clkb_tmp = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VPU_CLKB_CNTL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vpu_clkb_tmp",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_vpu_clkb_tmp_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tm2_vpu_clkb_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VPU_CLKB_CNTL,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_clkb_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_vpu_clkb_tmp.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tm2_vpu_clkb = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VPU_CLKB_CNTL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vpu_clkb",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_vpu_clkb_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

/* cpu and dsu */
/* Datasheet names this field as "premux0" */
static struct clk_regmap tm2_cpu_clk_premux0 = {
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
			{ .hw = &tm2_fclk_div2.hw },
			{ .hw = &tm2_fclk_div3.hw },
		},
		.num_parents = 3,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/* Datasheet names this field as "premux1" */
static struct clk_regmap tm2_cpu_clk_premux1 = {
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
			{ .hw = &tm2_fclk_div2.hw },
			{ .hw = &tm2_fclk_div3.hw },
		},
		.num_parents = 3,
		/* This sub-tree is used a parking clock */
		.flags = CLK_SET_RATE_NO_REPARENT
	},
};

/* Datasheet names this field as "mux0_divn_tcnt" */
static struct clk_regmap tm2_cpu_clk_mux0_div = {
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
			&tm2_cpu_clk_premux0.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/* Datasheet names this field as "postmux0" */
static struct clk_regmap tm2_cpu_clk_postmux0 = {
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
			&tm2_cpu_clk_premux0.hw,
			&tm2_cpu_clk_mux0_div.hw,
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/* Datasheet names this field as "Mux1_divn_tcnt" */
static struct clk_regmap tm2_cpu_clk_mux1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_SYS_CPU_CLK_CNTL0,
		.shift = 20,
		.width = 6,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cpu_clk_dyn1_div",
		.ops = &clk_regmap_divider_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_cpu_clk_premux1.hw
		},
		.num_parents = 1,
	},
};

/* Datasheet names this field as "postmux1" */
static struct clk_regmap tm2_cpu_clk_postmux1 = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_SYS_CPU_CLK_CNTL0,
		.mask = 0x1,
		.shift = 18,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cpu_clk_dyn1",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_cpu_clk_premux1.hw,
			&tm2_cpu_clk_mux1_div.hw,
		},
		.num_parents = 2,
		/* This sub-tree is used a parking clock */
		.flags = CLK_SET_RATE_NO_REPARENT,
	},
};

/* Datasheet names this field as "Final_dyn_mux_sel" */
static struct clk_regmap tm2_cpu_clk_dyn = {
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
			&tm2_cpu_clk_postmux0.hw,
			&tm2_cpu_clk_postmux1.hw,
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/* Datasheet names this field as "Final_mux_sel" */
static struct clk_regmap tm2_cpu_clk = {
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
			&tm2_cpu_clk_dyn.hw,
			&tm2_sys_pll.hw,
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/* Datasheet names this field as "premux0" */
static struct clk_regmap tm2_dsu_clk_premux0 = {
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
			{ .hw = &tm2_fclk_div2.hw },
			{ .hw = &tm2_fclk_div3.hw },
			{ .hw = &tm2_gp1_pll.hw },
		},
		.num_parents = 4,
		.flags = CLK_SET_RATE_PARENT | CLK_SET_RATE_NO_REPARENT,
	},
};

/* Datasheet names this field as "premux1" */
static struct clk_regmap tm2_dsu_clk_premux1 = {
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
			{ .hw = &tm2_fclk_div2.hw },
			{ .hw = &tm2_fclk_div3.hw },
			{ .hw = &tm2_gp1_pll.hw },
		},
		.num_parents = 4,
		.flags = CLK_SET_RATE_PARENT | CLK_SET_RATE_NO_REPARENT,
	},
};

/* Datasheet names this field as "Mux0_divn_tcnt" */
static struct clk_regmap tm2_dsu_clk_mux0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_SYS_CPU_CLK_CNTL5,
		.shift = 4,
		.width = 6,
	},
	.hw.init = &(struct clk_init_data){
		.name = "dsu_clk_dyn0_div",
		.ops = &clk_regmap_divider_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_dsu_clk_premux0.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/* Datasheet names this field as "postmux0" */
static struct clk_regmap tm2_dsu_clk_postmux0 = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_SYS_CPU_CLK_CNTL5,
		.mask = 0x1,
		.shift = 2,
	},
	.hw.init = &(struct clk_init_data){
		.name = "dsu_clk_dyn0",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_dsu_clk_premux0.hw,
			&tm2_dsu_clk_mux0_div.hw,
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/* Datasheet names this field as "Mux1_divn_tcnt" */
static struct clk_regmap tm2_dsu_clk_mux1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_SYS_CPU_CLK_CNTL5,
		.shift = 20,
		.width = 6,
	},
	.hw.init = &(struct clk_init_data){
		.name = "dsu_clk_dyn1_div",
		.ops = &clk_regmap_divider_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_dsu_clk_premux1.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/* Datasheet names this field as "postmux1" */
static struct clk_regmap tm2_dsu_clk_postmux1 = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_SYS_CPU_CLK_CNTL5,
		.mask = 0x1,
		.shift = 18,
	},
	.hw.init = &(struct clk_init_data){
		.name = "dsu_clk_dyn1",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_dsu_clk_premux1.hw,
			&tm2_dsu_clk_mux1_div.hw,
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/* Datasheet names this field as "Final_dyn_mux_sel" */
static struct clk_regmap tm2_dsu_clk_dyn = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_SYS_CPU_CLK_CNTL5,
		.mask = 0x1,
		.shift = 10,
	},
	.hw.init = &(struct clk_init_data){
		.name = "dsu_clk_dyn",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_dsu_clk_postmux0.hw,
			&tm2_dsu_clk_postmux1.hw,
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT | CLK_SET_RATE_NO_REPARENT,
	},
};

/* Datasheet names this field as "Final_mux_sel" */
static struct clk_regmap tm2_dsu_final_clk = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_SYS_CPU_CLK_CNTL5,
		.mask = 0x1,
		.shift = 11,
	},
	.hw.init = &(struct clk_init_data){
		.name = "dsu_clk_final",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_dsu_clk_dyn.hw,
			&tm2_sys_pll.hw,
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT | CLK_SET_RATE_NO_REPARENT,
	},
};

static struct clk_regmap tm2_dsu_clk = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_SYS_CPU_CLK_CNTL6,
		.mask = 0x1,
		.shift = 27,
	},
	.hw.init = &(struct clk_init_data){
		.name = "dsu_clk",
		.ops = &clk_regmap_mux_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_cpu_clk.hw,
			&tm2_dsu_final_clk.hw,
		},
		.num_parents = 2,
	},
};

static int tm2_cpu_clk_mux_notifier_cb(struct notifier_block *nb,
				       unsigned long event, void *data)
{
	if (event == POST_RATE_CHANGE || event == PRE_RATE_CHANGE) {
		/* Wait for clock propagation before/after changing the mux */
		udelay(100);
		return NOTIFY_OK;
	}

	return NOTIFY_DONE;
}

static struct notifier_block tm2_cpu_clk_mux_nb = {
	.notifier_call = tm2_cpu_clk_mux_notifier_cb,
};

struct tm2_cpu_clk_postmux_nb_data {
	struct notifier_block nb;
	struct clk_hw *fclk_div2;
	struct clk_hw *cpu_clk_dyn;
	struct clk_hw *cpu_clk_postmux0;
	struct clk_hw *cpu_clk_postmux1;
	struct clk_hw *cpu_clk_premux1;
};

static int tm2_cpu_clk_postmux_notifier_cb(struct notifier_block *nb,
					   unsigned long event, void *data)
{
	struct tm2_cpu_clk_postmux_nb_data *nb_data =
		container_of(nb, struct tm2_cpu_clk_postmux_nb_data, nb);

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

static struct tm2_cpu_clk_postmux_nb_data tm2_cpu_clk_postmux0_nb_data = {
	.cpu_clk_dyn = &tm2_cpu_clk_dyn.hw,
	.cpu_clk_postmux0 = &tm2_cpu_clk_postmux0.hw,
	.cpu_clk_postmux1 = &tm2_cpu_clk_postmux1.hw,
	.cpu_clk_premux1 = &tm2_cpu_clk_premux1.hw,
	.nb.notifier_call = tm2_cpu_clk_postmux_notifier_cb,
};

struct tm2_sys_pll_nb_data {
	struct notifier_block nb;
	struct clk_hw *sys_pll;
	struct clk_hw *cpu_clk;
	struct clk_hw *cpu_clk_dyn;
};

static int tm2_sys_pll_notifier_cb(struct notifier_block *nb,
				   unsigned long event, void *data)
{
	struct tm2_sys_pll_nb_data *nb_data =
		container_of(nb, struct tm2_sys_pll_nb_data, nb);

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

static struct tm2_sys_pll_nb_data tm2_sys_pll_nb_data = {
	.sys_pll = &tm2_sys_pll.hw,
	.cpu_clk = &tm2_cpu_clk.hw,
	.cpu_clk_dyn = &tm2_cpu_clk_dyn.hw,
	.nb.notifier_call = tm2_sys_pll_notifier_cb,
};

/* tm2 dsu notify */
struct tm2_dsu_clk_postmux_nb_data {
	struct notifier_block nb;
	struct clk_hw *dsu_clk_dyn;
	struct clk_hw *dsu_clk_postmux0;
	struct clk_hw *dsu_clk_postmux1;
};

static int tm2_dsu_clk_postmux_notifier_cb(struct notifier_block *nb,
					   unsigned long event, void *data)
{
	struct tm2_dsu_clk_postmux_nb_data *nb_data =
		container_of(nb, struct tm2_dsu_clk_postmux_nb_data, nb);
	int ret = 0;

	switch (event) {
	case PRE_RATE_CHANGE:
		ret = clk_hw_set_parent(nb_data->dsu_clk_dyn,
					nb_data->dsu_clk_postmux1);
		if (ret)
			return notifier_from_errno(ret);
		udelay(100);
		return NOTIFY_OK;
	case POST_RATE_CHANGE:
		ret = clk_hw_set_parent(nb_data->dsu_clk_dyn,
					nb_data->dsu_clk_postmux0);
		if (ret)
			return notifier_from_errno(ret);
		udelay(100);
		return NOTIFY_OK;
	default:
		return NOTIFY_DONE;
	}
}

static struct tm2_dsu_clk_postmux_nb_data tm2_dsu_clk_postmux0_nb_data = {
	.dsu_clk_dyn = &tm2_dsu_clk_dyn.hw,
	.dsu_clk_postmux0 = &tm2_dsu_clk_postmux0.hw,
	.dsu_clk_postmux1 = &tm2_dsu_clk_postmux1.hw,
	.nb.notifier_call = tm2_dsu_clk_postmux_notifier_cb,
};

static struct clk_regmap tm2_cpu_clk_div16_en = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_SYS_CPU_CLK_CNTL1,
		.bit_idx = 1,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "cpu_clk_div16_en",
		.ops = &clk_regmap_gate_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_cpu_clk.hw
		},
		.num_parents = 1,
		/*
		 * This clock is used to debug the cpu_clk range
		 * Linux should not change it at runtime
		 */
	},
};

static struct clk_fixed_factor tm2_cpu_clk_div16 = {
	.mult = 1,
	.div = 16,
	.hw.init = &(struct clk_init_data){
		.name = "cpu_clk_div16",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_cpu_clk_div16_en.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap tm2_cpu_clk_apb_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_SYS_CPU_CLK_CNTL1,
		.shift = 3,
		.width = 3,
		.flags = CLK_DIVIDER_POWER_OF_TWO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cpu_clk_apb_div",
		.ops = &clk_regmap_divider_ro_ops,
		.parent_hws = (const struct clk_hw *[]) { &tm2_cpu_clk.hw },
		.num_parents = 1,
	},
};

static struct clk_regmap tm2_cpu_clk_apb = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_SYS_CPU_CLK_CNTL1,
		.bit_idx = 1,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "cpu_clk_apb",
		.ops = &clk_regmap_gate_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_cpu_clk_apb_div.hw
		},
		.num_parents = 1,
		/*
		 * This clock is set by the ROM monitor code,
		 * Linux should not change it at runtime
		 */
	},
};

static struct clk_regmap tm2_cpu_clk_atb_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_SYS_CPU_CLK_CNTL1,
		.shift = 6,
		.width = 3,
		.flags = CLK_DIVIDER_POWER_OF_TWO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cpu_clk_atb_div",
		.ops = &clk_regmap_divider_ro_ops,
		.parent_hws = (const struct clk_hw *[]) { &tm2_cpu_clk.hw },
		.num_parents = 1,
	},
};

static struct clk_regmap tm2_cpu_clk_atb = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_SYS_CPU_CLK_CNTL1,
		.bit_idx = 17,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "cpu_clk_atb",
		.ops = &clk_regmap_gate_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_cpu_clk_atb_div.hw
		},
		.num_parents = 1,
		/*
		 * This clock is set by the ROM monitor code,
		 * Linux should not change it at runtime
		 */
	},
};

static struct clk_regmap tm2_cpu_clk_axi_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_SYS_CPU_CLK_CNTL1,
		.shift = 9,
		.width = 3,
		.flags = CLK_DIVIDER_POWER_OF_TWO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cpu_clk_axi_div",
		.ops = &clk_regmap_divider_ro_ops,
		.parent_hws = (const struct clk_hw *[]) { &tm2_cpu_clk.hw },
		.num_parents = 1,
	},
};

static struct clk_regmap tm2_cpu_clk_axi = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_SYS_CPU_CLK_CNTL1,
		.bit_idx = 18,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "cpu_clk_axi",
		.ops = &clk_regmap_gate_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_cpu_clk_axi_div.hw
		},
		.num_parents = 1,
		/*
		 * This clock is set by the ROM monitor code,
		 * Linux should not change it at runtime
		 */
	},
};

static struct clk_regmap tm2_cpu_clk_trace_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_SYS_CPU_CLK_CNTL1,
		.shift = 20,
		.width = 3,
		.flags = CLK_DIVIDER_POWER_OF_TWO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cpu_clk_trace_div",
		.ops = &clk_regmap_divider_ro_ops,
		.parent_hws = (const struct clk_hw *[]) { &tm2_cpu_clk.hw },
		.num_parents = 1,
	},
};

static struct clk_regmap tm2_cpu_clk_trace = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_SYS_CPU_CLK_CNTL1,
		.bit_idx = 23,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "cpu_clk_trace",
		.ops = &clk_regmap_gate_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_cpu_clk_trace_div.hw
		},
		.num_parents = 1,
		/*
		 * This clock is set by the ROM monitor code,
		 * Linux should not change it at runtime
		 */
	},
};

static struct clk_regmap tm2_spicc_0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_GCLK_MPEG0,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spicc_0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_clk81.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap tm2_spicc_1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_GCLK_MPEG0,
		.bit_idx = 14,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spicc_1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_clk81.hw
		},
		.num_parents = 1,
	},
};

static const struct clk_parent_data tm2_spicc_parent_hws[] = {
	{ .fw_name = "xtal", },
	{ .hw = &tm2_clk81.hw },
	{ .hw = &tm2_fclk_div4.hw },
	{ .hw = &tm2_fclk_div3.hw },
	{ .hw = &tm2_fclk_div2.hw },
	{ .hw = &tm2_fclk_div5.hw },
	{ .hw = &tm2_fclk_div7.hw },
	{ .hw = &tm2_gp0_pll.hw },
};

static struct clk_regmap tm2_spicc0_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_SPICC_CLK_CNTL,
		.mask = 0x7,
		.shift = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spicc0_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = tm2_spicc_parent_hws,
		.num_parents = ARRAY_SIZE(tm2_spicc_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap tm2_spicc0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_SPICC_CLK_CNTL,
		.shift = 0,
		.width = 6,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spicc0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_spicc0_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tm2_spicc0_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_SPICC_CLK_CNTL,
		.bit_idx = 6,
	},
	.hw.init = &(struct clk_init_data){
		.name = "spicc0_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_spicc0_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tm2_spicc1_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_SPICC_CLK_CNTL,
		.mask = 0x7,
		.shift = 23,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spicc1_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = tm2_spicc_parent_hws,
		.num_parents = ARRAY_SIZE(tm2_spicc_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap tm2_spicc1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_SPICC_CLK_CNTL,
		.shift = 16,
		.width = 6,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spicc1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_spicc1_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tm2_spicc1_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_SPICC_CLK_CNTL,
		.bit_idx = 22,
	},
	.hw.init = &(struct clk_init_data){
		.name = "spicc1_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_spicc1_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data tm2_dsp_parent_hws[] = {
	{ .hw = &tm2_fclk_div2.hw },
	{ .hw = &tm2_fclk_div3.hw },
	{ .hw = &tm2_fclk_div5.hw },
	{ .hw = &tm2_fclk_div7.hw },
	{ .fw_name = "xtal", },
	{ .hw = &tm2_gp0_pll.hw },
	{ .hw = &tm2_gp1_pll.hw },
	{ .hw = &tm2_hifi_pll.hw },
};

static struct clk_regmap tm2_dspa_a_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_DSP_CLK_CNTL,
		.mask = 0x7,
		.shift = 4,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "dspa_a_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = tm2_dsp_parent_hws,
		.num_parents = ARRAY_SIZE(tm2_dsp_parent_hws),
	},
};

static struct clk_regmap tm2_dspa_a_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_DSP_CLK_CNTL,
		.shift = 0,
		.width = 4,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "dspa_a_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_dspa_a_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tm2_dspa_a_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_DSP_CLK_CNTL,
		.bit_idx = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "dspa_a_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_dspa_a_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tm2_dspa_b_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_DSP_CLK_CNTL,
		.mask = 0x7,
		.shift = 12,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "dspa_b_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = tm2_dsp_parent_hws,
		.num_parents = ARRAY_SIZE(tm2_dsp_parent_hws),
	},
};

static struct clk_regmap tm2_dspa_b_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_DSP_CLK_CNTL,
		.shift = 8,
		.width = 4,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "dspa_b_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_dspa_b_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tm2_dspa_b_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_DSP_CLK_CNTL,
		.bit_idx = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "dspa_b_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_dspa_b_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tm2_dspa_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_DSP_CLK_CNTL,
		.mask = 0x1,
		.shift = 15,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "dspa_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_dspa_a_gate.hw,
			&tm2_dspa_b_gate.hw,
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tm2_dspb_a_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_DSP_CLK_CNTL,
		.mask = 0x7,
		.shift = 20,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "dspb_a_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = tm2_dsp_parent_hws,
		.num_parents = ARRAY_SIZE(tm2_dsp_parent_hws),
	},
};

static struct clk_regmap tm2_dspb_a_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_DSP_CLK_CNTL,
		.shift = 16,
		.width = 4,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "dspb_a_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_dspb_a_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tm2_dspb_a_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_DSP_CLK_CNTL,
		.bit_idx = 23,
	},
	.hw.init = &(struct clk_init_data){
		.name = "dspb_a_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_dspb_a_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tm2_dspb_b_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_DSP_CLK_CNTL,
		.mask = 0x7,
		.shift = 28,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "dspb_b_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = tm2_dsp_parent_hws,
		.num_parents = ARRAY_SIZE(tm2_dsp_parent_hws),
	},
};

static struct clk_regmap tm2_dspb_b_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_DSP_CLK_CNTL,
		.shift = 24,
		.width = 4,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "dspb_b_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_dspb_b_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tm2_dspb_b_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_DSP_CLK_CNTL,
		.bit_idx = 23,
	},
	.hw.init = &(struct clk_init_data){
		.name = "dspb_b_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_dspb_b_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tm2_dspb_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_DSP_CLK_CNTL,
		.mask = 0x1,
		.shift = 31,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "dspb_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_dspb_a_gate.hw,
			&tm2_dspb_b_gate.hw,
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data tm2_vipnanoq_parent_hws[] = {
	{ .fw_name = "xtal", },
	{ .hw = &tm2_gp0_pll.hw },
	{ .hw = &tm2_hifi_pll.hw },
	{ .hw = &tm2_fclk_div2p5.hw },
	{ .hw = &tm2_fclk_div3.hw },
	{ .hw = &tm2_fclk_div4.hw },
	{ .hw = &tm2_fclk_div5.hw },
	{ .hw = &tm2_fclk_div7.hw },
};

static struct clk_regmap tm2_vipnanoq_core_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VIPNANOQ_CLK_CNTL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vipnanoq_core_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = tm2_vipnanoq_parent_hws,
		.num_parents = ARRAY_SIZE(tm2_vipnanoq_parent_hws),
	},
};

static struct clk_regmap tm2_vipnanoq_core_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VIPNANOQ_CLK_CNTL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vipnanoq_core_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_vipnanoq_core_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tm2_vipnanoq_core_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VIPNANOQ_CLK_CNTL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vipnanoq_core_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_vipnanoq_core_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tm2_vipnanoq_axi_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VIPNANOQ_CLK_CNTL,
		.mask = 0x7,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vipnanoq_axi_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = tm2_vipnanoq_parent_hws,
		.num_parents = ARRAY_SIZE(tm2_vipnanoq_parent_hws),
	},
};

static struct clk_regmap tm2_vipnanoq_axi_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VIPNANOQ_CLK_CNTL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vipnanoq_axi_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_vipnanoq_axi_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tm2_vipnanoq_axi_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VIPNANOQ_CLK_CNTL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vipnanoq_axi_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_vipnanoq_axi_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data tm2_hdmirx_parent_hws[] = {
	{ .fw_name = "xtal", },
	{ .hw = &tm2_fclk_div4.hw },
	{ .hw = &tm2_fclk_div3.hw },
	{ .hw = &tm2_fclk_div5.hw },
};

static struct clk_regmap tm2_hdmirx_cfg_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_HDMIRX_CLK_CNTL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmirx_cfg_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = tm2_hdmirx_parent_hws,
		.num_parents = ARRAY_SIZE(tm2_hdmirx_parent_hws),
	},
};

static struct clk_regmap tm2_hdmirx_cfg_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_HDMIRX_CLK_CNTL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmirx_cfg_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_hdmirx_cfg_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tm2_hdmirx_cfg_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_HDMIRX_CLK_CNTL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmirx_cfg_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_hdmirx_cfg_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tm2_hdmirx_modet_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_HDMIRX_CLK_CNTL,
		.mask = 0x3,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmirx_modet_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = tm2_hdmirx_parent_hws,
		.num_parents = ARRAY_SIZE(tm2_hdmirx_parent_hws),
	},
};

static struct clk_regmap tm2_hdmirx_modet_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_HDMIRX_CLK_CNTL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmirx_modet_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_hdmirx_modet_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tm2_hdmirx_modet_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_HDMIRX_CLK_CNTL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmirx_modet_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_hdmirx_modet_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data tm2_hdmirx_ref_parent_hws[] = {
	{ .hw = &tm2_fclk_div4.hw },
	{ .hw = &tm2_fclk_div3.hw },
	{ .hw = &tm2_fclk_div5.hw },
	{ .hw = &tm2_fclk_div7.hw },
};

static struct clk_regmap tm2_hdmirx_acr_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_HDMIRX_AUD_CLK_CNTL,
		.mask = 0x3,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmirx_acr_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = tm2_hdmirx_ref_parent_hws,
		.num_parents = ARRAY_SIZE(tm2_hdmirx_ref_parent_hws),
	},
};

static struct clk_regmap tm2_hdmirx_acr_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_HDMIRX_AUD_CLK_CNTL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmirx_acr_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_hdmirx_acr_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tm2_hdmirx_acr_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_HDMIRX_AUD_CLK_CNTL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmirx_acr_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_hdmirx_acr_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data tm2_hdmirx_meter_parent_hws[] = {
	{ .fw_name = "xtal", },
	{ .hw = &tm2_fclk_div4.hw },
	{ .hw = &tm2_fclk_div3.hw },
	{ .hw = &tm2_fclk_div5.hw },
};

static struct clk_regmap tm2_hdmirx_meter_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_HDMIRX_METER_CLK_CNTL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmirx_meter_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = tm2_hdmirx_meter_parent_hws,
		.num_parents = ARRAY_SIZE(tm2_hdmirx_meter_parent_hws),
	},
};

static struct clk_regmap tm2_hdmirx_meter_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_HDMIRX_METER_CLK_CNTL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmirx_meter_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_hdmirx_meter_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tm2_hdmirx_meter_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_HDMIRX_METER_CLK_CNTL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmirx_meter_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_hdmirx_meter_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tm2_hdmirx_axi_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_HDMIRX_AXI_CLK_CNTL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmirx_axi_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = tm2_hdmirx_meter_parent_hws,
		.num_parents = ARRAY_SIZE(tm2_hdmirx_meter_parent_hws),
	},
};

static struct clk_regmap tm2_hdmirx_axi_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_HDMIRX_AXI_CLK_CNTL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmirx_axi_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_hdmirx_axi_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tm2_hdmirx_axi_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_HDMIRX_AXI_CLK_CNTL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmirx_axi_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_hdmirx_axi_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tm2_hdmirx_skp_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_HDCP22_CLK_CNTL,
		.mask = 0x3,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmirx_skp_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = tm2_hdmirx_meter_parent_hws,
		.num_parents = ARRAY_SIZE(tm2_hdmirx_meter_parent_hws),
	},
};

static struct clk_regmap tm2_hdmirx_skp_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_HDMIRX_METER_CLK_CNTL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmirx_skp_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_hdmirx_skp_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tm2_hdmirx_skp_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_HDMIRX_METER_CLK_CNTL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmirx_skp_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_hdmirx_skp_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data tm2_hdmirx_esm_parent_hws[] = {
	{ .hw = &tm2_fclk_div7.hw },
	{ .hw = &tm2_fclk_div4.hw },
	{ .hw = &tm2_fclk_div3.hw },
	{ .hw = &tm2_fclk_div5.hw },
};

static struct clk_regmap tm2_hdmirx_esm_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_HDCP22_CLK_CNTL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmirx_esm_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = tm2_hdmirx_esm_parent_hws,
		.num_parents = ARRAY_SIZE(tm2_hdmirx_esm_parent_hws),
	},
};

static struct clk_regmap tm2_hdmirx_esm_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_HDCP22_CLK_CNTL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmirx_esm_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_hdmirx_esm_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tm2_hdmirx_esm_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_HDCP22_CLK_CNTL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmirx_esm_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_hdmirx_esm_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

#define MESON_GATE(_name, _reg, _bit) \
	MESON_PCLK(_name, _reg, _bit, &tm2_clk81.hw)

#define MESON_GATE_RO(_name, _reg, _bit) \
	MESON_PCLK_RO(_name, _reg, _bit, &tm2_clk81.hw)

/* Everything Else (EE) domain gates */
static MESON_GATE(tm2_ddr, HHI_GCLK_MPEG0,          0);
static MESON_GATE(tm2_dos, HHI_GCLK_MPEG0,          1);
static MESON_GATE(tm2_eth_phy, HHI_GCLK_MPEG0,      4);
static MESON_GATE(tm2_isa, HHI_GCLK_MPEG0,          5);
static MESON_GATE(tm2_pl310, HHI_GCLK_MPEG0,        6);
static MESON_GATE(tm2_periphs, HHI_GCLK_MPEG0,      7);
static MESON_GATE(tm2_i2c, HHI_GCLK_MPEG0,          9);
static MESON_GATE(tm2_sana, HHI_GCLK_MPEG0,         10);
static MESON_GATE(tm2_smart_card, HHI_GCLK_MPEG0,   11);
static MESON_GATE(tm2_uart0, HHI_GCLK_MPEG0,        13);
static MESON_GATE(tm2_stream, HHI_GCLK_MPEG0,       15);
static MESON_GATE(tm2_async_fifo, HHI_GCLK_MPEG0,   16);
static MESON_GATE(tm2_tvfe, HHI_GCLK_MPEG0,         18);
static MESON_GATE(tm2_hiu_reg, HHI_GCLK_MPEG0,      19);
static MESON_GATE(tm2_hdmirx_pclk, HHI_GCLK_MPEG0,  21);
static MESON_GATE(tm2_atv_demod, HHI_GCLK_MPEG0,    22);
static MESON_GATE(tm2_assist_misc, HHI_GCLK_MPEG0,  23);
static MESON_GATE(tm2_emmc_b, HHI_GCLK_MPEG0,       25);
static MESON_GATE(tm2_emmc_c, HHI_GCLK_MPEG0,       26);
static MESON_GATE(tm2_adec, HHI_GCLK_MPEG0,         27);
static MESON_GATE(tm2_acodec, HHI_GCLK_MPEG0,       28);
static MESON_GATE(tm2_tcon, HHI_GCLK_MPEG0,         29);
static MESON_GATE(tm2_spi, HHI_GCLK_MPEG0,          30);
static MESON_GATE(tm2_dsp, HHI_GCLK_MPEG0,          31);

static MESON_GATE(tm2_audio, HHI_GCLK_MPEG1,        0);
static MESON_GATE(tm2_eth_core, HHI_GCLK_MPEG1,     3);
static MESON_GATE(tm2_demux, HHI_GCLK_MPEG1,        4);
static MESON_GATE(tm2_aififo, HHI_GCLK_MPEG1,       11);
static MESON_GATE(tm2_adc, HHI_GCLK_MPEG1,          13);
static MESON_GATE(tm2_uart1, HHI_GCLK_MPEG1,        16);
static MESON_GATE(tm2_g2d, HHI_GCLK_MPEG1,          20);
static MESON_GATE(tm2_reset, HHI_GCLK_MPEG1,        23);
static MESON_GATE(tm2_u_parser, HHI_GCLK_MPEG1,     25);
static MESON_GATE(tm2_usb_general, HHI_GCLK_MPEG1,  26);
static MESON_GATE(tm2_ahb_arb0, HHI_GCLK_MPEG1,     29);

static MESON_GATE(tm2_ahb_data_bus, HHI_GCLK_MPEG2, 1);
static MESON_GATE(tm2_ahb_ctrl_bus, HHI_GCLK_MPEG2, 2);
static MESON_GATE(tm2_bt656, HHI_GCLK_MPEG2,        6);
static MESON_GATE(tm2_usb1_to_ddr, HHI_GCLK_MPEG2,  8);
static MESON_GATE(tm2_mmc_pclk, HHI_GCLK_MPEG2,     11);
static MESON_GATE(tm2_uart2, HHI_GCLK_MPEG2,        15);
static MESON_GATE(tm2_clk81_ts, HHI_GCLK_MPEG2,           22);
static MESON_GATE(tm2_demod_comb, HHI_GCLK_MPEG2,   25);
static MESON_GATE(tm2_vpu_intr, HHI_GCLK_MPEG2,     28);
static MESON_GATE(tm2_gic, HHI_GCLK_MPEG2,          30);

static MESON_GATE(tm2_vclk2_venci0, HHI_GCLK_OTHER, 1);
static MESON_GATE(tm2_vclk2_venci1, HHI_GCLK_OTHER, 2);
static MESON_GATE(tm2_vclk2_vencp0, HHI_GCLK_OTHER, 3);
static MESON_GATE(tm2_vclk2_vencp1, HHI_GCLK_OTHER, 4);
static MESON_GATE(tm2_vclk2_venct0, HHI_GCLK_OTHER, 5);
static MESON_GATE(tm2_vclk2_venct1, HHI_GCLK_OTHER, 6);
static MESON_GATE(tm2_vclk2_other, HHI_GCLK_OTHER,  7);
static MESON_GATE(tm2_vclk2_enci, HHI_GCLK_OTHER,   8);
static MESON_GATE(tm2_vclk2_encp, HHI_GCLK_OTHER,   9);
static MESON_GATE(tm2_dac_clk, HHI_GCLK_OTHER,      10);
static MESON_GATE(tm2_enc480p, HHI_GCLK_OTHER,      20);
static MESON_GATE(tm2_rng1, HHI_GCLK_OTHER,         21);
static MESON_GATE(tm2_vclk2_enct, HHI_GCLK_OTHER,   22);
static MESON_GATE(tm2_vclk2_encl, HHI_GCLK_OTHER,   23);
static MESON_GATE(tm2_vclk2_venclmmc, HHI_GCLK_OTHER, 24);
static MESON_GATE(tm2_vclk2_vencl, HHI_GCLK_OTHER,  25);
static MESON_GATE(tm2_vclk2_other1, HHI_GCLK_OTHER, 26);

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

static MESON_GATE(tm2_dma,		HHI_GCLK_SP_MPEG, 0);
static MESON_GATE(tm2_efuse,		HHI_GCLK_SP_MPEG, 1);
static MESON_GATE(tm2_rom_boot,		HHI_GCLK_SP_MPEG, 2);
static MESON_GATE(tm2_reset_sec,	HHI_GCLK_SP_MPEG, 3);
static MESON_GATE(tm2_sec_ahb_apb3,	HHI_GCLK_SP_MPEG, 4);

/* Array of all clocks provided by this provider */
static struct clk_hw_onecell_data tm2_hw_onecell_data = {
	.hws = {
		[CLKID_SYS_PLL]			= &tm2_sys_pll.hw,
		[CLKID_FIXED_PLL]		= &tm2_fixed_pll.hw,
		[CLKID_FCLK_DIV2]		= &tm2_fclk_div2.hw,
		[CLKID_FCLK_DIV3]		= &tm2_fclk_div3.hw,
		[CLKID_FCLK_DIV4]		= &tm2_fclk_div4.hw,
		[CLKID_FCLK_DIV5]		= &tm2_fclk_div5.hw,
		[CLKID_FCLK_DIV7]		= &tm2_fclk_div7.hw,
		[CLKID_FCLK_DIV2P5]		= &tm2_fclk_div2p5.hw,
		[CLKID_GP0_PLL]			= &tm2_gp0_pll.hw,
		[CLKID_GP1_PLL]			= &tm2_gp1_pll.hw,
		[CLKID_GP1_PLL_DCO]		= &tm2_gp1_pll_dco.hw,
		[CLKID_MPEG_SEL]		= &tm2_mpeg_clk_sel.hw,
		[CLKID_MPEG_DIV]		= &tm2_mpeg_clk_div.hw,
		[CLKID_CLK81]			= &tm2_clk81.hw,
		[CLKID_MPLL0]			= &tm2_mpll0.hw,
		[CLKID_MPLL1]			= &tm2_mpll1.hw,
		[CLKID_MPLL2]			= &tm2_mpll2.hw,
		[CLKID_MPLL3]			= &tm2_mpll3.hw,
		[CLKID_DDR]			= &tm2_ddr.hw, /*MPEG0 0*/
		[CLKID_DOS]			= &tm2_dos.hw,
		[CLKID_ETH_PHY]			= &tm2_eth_phy.hw,
		[CLKID_ISA]			= &tm2_isa.hw,
		[CLKID_PL310]			= &tm2_pl310.hw,
		[CLKID_PERIPHS]			= &tm2_periphs.hw,
		[CLKID_SPICC0]			= &tm2_spicc_0.hw,
		[CLKID_I2C]			= &tm2_i2c.hw,
		[CLKID_SANA]			= &tm2_sana.hw,
		[CLKID_SMART_CARD]		= &tm2_smart_card.hw,
		[CLKID_UART0]			= &tm2_uart0.hw,
		[CLKID_SPICC1]			= &tm2_spicc_1.hw,
		[CLKID_STREAM]			= &tm2_stream.hw,
		[CLKID_ASYNC_FIFO]		= &tm2_async_fifo.hw,
		[CLKID_TVFE]			= &tm2_tvfe.hw,
		[CLKID_HIU_REG]			= &tm2_hiu_reg.hw,
		[CLKID_HDMIRX_PCLK]		= &tm2_hdmirx_pclk.hw,
		[CLKID_ATV_DEMOD]		= &tm2_atv_demod.hw,
		[CLKID_ASSIST_MISC]		= &tm2_assist_misc.hw,
		[CLKID_SD_EMMC_B]		= &tm2_emmc_b.hw,
		[CLKID_SD_EMMC_C]		= &tm2_emmc_c.hw,
		[CLKID_ADEC]			= &tm2_adec.hw,
		[CLKID_ACODEC]			= &tm2_acodec.hw,
		[CLKID_TCON]			= &tm2_tcon.hw,
		[CLKID_SPI]			= &tm2_spi.hw,
		[CLKID_DSP]			= &tm2_dsp.hw,
		[CLKID_AUDIO]			= &tm2_audio.hw,
		[CLKID_ETH_CORE]		= &tm2_eth_core.hw,
		[CLKID_DEMUX]			= &tm2_demux.hw,
		[CLKID_AIFIFO]			= &tm2_aififo.hw,
		[CLKID_ADC]			= &tm2_adc.hw,
		[CLKID_UART1]			= &tm2_uart1.hw,
		[CLKID_G2D]			= &tm2_g2d.hw,
		[CLKID_RESET]			= &tm2_reset.hw,
		[CLKID_U_PARSER]		= &tm2_u_parser.hw,
		[CLKID_USB_GENERAL]		= &tm2_usb_general.hw,
		[CLKID_AHB_ARB0]		= &tm2_ahb_arb0.hw,
		[CLKID_AHB_DATA_BUS]		= &tm2_ahb_data_bus.hw,
		[CLKID_AHB_CTRL_BUS]		= &tm2_ahb_ctrl_bus.hw,
		[CLKID_BT656]			= &tm2_bt656.hw,
		[CLKID_USB1_TO_DDR]		= &tm2_usb1_to_ddr.hw,
		[CLKID_MMC_PCLK]		= &tm2_mmc_pclk.hw,
		[CLKID_UART2]			= &tm2_uart2.hw,
		[CLKID_CLK81_TS]		= &tm2_clk81_ts.hw,
		[CLKID_VPU_INTR]		= &tm2_vpu_intr.hw,
		[CLKID_DEMOD_COMB]		= &tm2_demod_comb.hw,
		[CLKID_GIC]			= &tm2_gic.hw,
		[CLKID_VCLK2_VENCI0]		= &tm2_vclk2_venci0.hw,
		[CLKID_VCLK2_VENCI1]		= &tm2_vclk2_venci1.hw,
		[CLKID_VCLK2_VENCP0]		= &tm2_vclk2_vencp0.hw,
		[CLKID_VCLK2_VENCP1]		= &tm2_vclk2_vencp1.hw,
		[CLKID_VCLK2_VENCT0]		= &tm2_vclk2_venct0.hw,
		[CLKID_VCLK2_VENCT1]		= &tm2_vclk2_venct1.hw,
		[CLKID_VCLK2_OTHER]		= &tm2_vclk2_other.hw,
		[CLKID_VCLK2_ENCI]		= &tm2_vclk2_enci.hw,
		[CLKID_VCLK2_ENCP]		= &tm2_vclk2_encp.hw,
		[CLKID_DAC_CLK]			= &tm2_dac_clk.hw,
		[CLKID_ENC480P]			= &tm2_enc480p.hw,
		[CLKID_RNG1]			= &tm2_rng1.hw,
		[CLKID_VCLK2_ENCT]		= &tm2_vclk2_enct.hw,
		[CLKID_VCLK2_ENCL]		= &tm2_vclk2_encl.hw,
		[CLKID_VCLK2_VENCLMMC]		= &tm2_vclk2_venclmmc.hw,
		[CLKID_VCLK2_VENCL]		= &tm2_vclk2_vencl.hw,
						  /*OTHER 26*/
		[CLKID_VCLK2_OTHER1]		= &tm2_vclk2_other1.hw,
		[CLKID_VIPNANOQ]		= &tm2_vipnanoq.hw,
		[CLKID_PCIE0]			= &tm2_pcie0.hw,
		[CLKID_PCIE1]			= &tm2_pcie1.hw,
		[CLKID_PCIE0PHY]		= &tm2_pcie0phy.hw,
		[CLKID_PCIE1PHY]		= &tm2_pcie1phy.hw,
		[CLKID_PARSER1]			= &tm2_parserl.hw,
		[CLKID_HDCP22_PCLK]		= &tm2_hdcp22_pclk.hw,
		[CLKID_HDMITX_PCLK]		= &tm2_hdmitx_pclk.hw,
		[CLKID_HDMITX_AXI_PCLK]		= &tm2_hdmirx_axi_pclk.hw,
		[CLKID_DSPB]			= &tm2_dspb.hw,
		[CLKID_DSPA]			= &tm2_dspa.hw,
		[CLKID_DMA]			= &tm2_dma.hw,
		[CLKID_EFUSE]			= &tm2_efuse.hw,
		[CLKID_ROM_BOOT]		= &tm2_rom_boot.hw,
		[CLKID_RESET_SEC]		= &tm2_reset_sec.hw,
		[CLKID_SEC_APH_APB3]		= &tm2_sec_ahb_apb3.hw,
		[CLKID_SD_EMMC_B_CLK0_SEL]	= &tm2_sd_emmc_b_clk0_sel.hw,
		[CLKID_SD_EMMC_B_CLK0_DIV]	= &tm2_sd_emmc_b_clk0_div.hw,
		[CLKID_SD_EMMC_B_CLK0]		= &tm2_sd_emmc_b_clk0.hw,
		[CLKID_SD_EMMC_C_CLK0_SEL]	= &tm2_sd_emmc_c_clk0_sel.hw,
		[CLKID_SD_EMMC_C_CLK0_DIV]	= &tm2_sd_emmc_c_clk0_div.hw,
		[CLKID_SD_EMMC_C_CLK0]		= &tm2_sd_emmc_c_clk0.hw,
		[CLKID_MPLL0_DIV]		= &tm2_mpll0_div.hw,
		[CLKID_MPLL1_DIV]		= &tm2_mpll1_div.hw,
		[CLKID_MPLL2_DIV]		= &tm2_mpll2_div.hw,
		[CLKID_MPLL3_DIV]		= &tm2_mpll3_div.hw,
		[CLKID_FCLK_DIV2_DIV]		= &tm2_fclk_div2_div.hw,
		[CLKID_FCLK_DIV3_DIV]		= &tm2_fclk_div3_div.hw,
		[CLKID_FCLK_DIV4_DIV]		= &tm2_fclk_div4_div.hw,
		[CLKID_FCLK_DIV5_DIV]		= &tm2_fclk_div5_div.hw,
		[CLKID_FCLK_DIV7_DIV]		= &tm2_fclk_div7_div.hw,
		[CLKID_FCLK_DIV2P5_DIV]		= &tm2_fclk_div2p5_div.hw,
		[CLKID_HIFI_PLL]		= &tm2_hifi_pll.hw,
		[CLKID_FIXED_PLL_DCO]		= &tm2_fixed_pll_dco.hw,
		[CLKID_SYS_PLL_DCO]		= &tm2_sys_pll_dco.hw,
		[CLKID_GP0_PLL_DCO]		= &tm2_gp0_pll_dco.hw,
		[CLKID_HIFI_PLL_DCO]		= &tm2_hifi_pll_dco.hw,
		[CLKID_MPLL_PREDIV]		= &tm2_mpll_prediv.hw,
		[CLKID_VPU_0_SEL]		= &tm2_vpu_0_sel.hw,
		[CLKID_VPU_0_DIV]		= &tm2_vpu_0_div.hw,
		[CLKID_VPU_0]			= &tm2_vpu_0.hw,
		[CLKID_VPU_1_SEL]		= &tm2_vpu_1_sel.hw,
		[CLKID_VPU_1_DIV]		= &tm2_vpu_1_div.hw,
		[CLKID_VPU_1]			= &tm2_vpu_1.hw,
		[CLKID_VPU]			= &tm2_vpu.hw,
		[CLKID_VAPB_0_SEL]		= &tm2_vapb_0_sel.hw,
		[CLKID_VAPB_0_DIV]		= &tm2_vapb_0_div.hw,
		[CLKID_VAPB_0]			= &tm2_vapb_0.hw,
		[CLKID_VAPB_1_SEL]		= &tm2_vapb_1_sel.hw,
		[CLKID_VAPB_1_DIV]		= &tm2_vapb_1_div.hw,
		[CLKID_VAPB_1]			= &tm2_vapb_1.hw,
		[CLKID_VAPB_SEL]		= &tm2_vapb_sel.hw,
		[CLKID_VAPB]			= &tm2_vapb.hw,
		[CLKID_HDMI_PLL_DCO]		= &tm2_hdmi_pll_dco.hw,
		[CLKID_HDMI_PLL_OD]		= &tm2_hdmi_pll_od.hw,
		[CLKID_HDMI_PLL_OD2]		= &tm2_hdmi_pll_od2.hw,
		[CLKID_HDMI_PLL]		= &tm2_hdmi_pll.hw,
		[CLKID_VID_PLL]			= &tm2_vid_pll_div.hw,
		[CLKID_VID_PLL_SEL]		= &tm2_vid_pll_sel.hw,
		[CLKID_VID_PLL_DIV]		= &tm2_vid_pll.hw,
		[CLKID_VCLK_SEL]		= &tm2_vclk_sel.hw,
		[CLKID_VCLK2_SEL]		= &tm2_vclk2_sel.hw,
		[CLKID_VCLK_INPUT]		= &tm2_vclk_input.hw,
		[CLKID_VCLK2_INPUT]		= &tm2_vclk2_input.hw,
		[CLKID_VCLK_DIV]		= &tm2_vclk_div.hw,
		[CLKID_VCLK2_DIV]		= &tm2_vclk2_div.hw,
		[CLKID_VCLK]			= &tm2_vclk.hw,
		[CLKID_VCLK2]			= &tm2_vclk2.hw,
		[CLKID_VCLK_DIV1]		= &tm2_vclk_div1.hw,
		[CLKID_VCLK_DIV2_EN]		= &tm2_vclk_div2_en.hw,
		[CLKID_VCLK_DIV4_EN]		= &tm2_vclk_div4_en.hw,
		[CLKID_VCLK_DIV6_EN]		= &tm2_vclk_div6_en.hw,
		[CLKID_VCLK_DIV12_EN]		= &tm2_vclk_div12_en.hw,
		[CLKID_VCLK2_DIV1]		= &tm2_vclk2_div1.hw,
		[CLKID_VCLK2_DIV2_EN]		= &tm2_vclk2_div2_en.hw,
		[CLKID_VCLK2_DIV4_EN]		= &tm2_vclk2_div4_en.hw,
		[CLKID_VCLK2_DIV6_EN]		= &tm2_vclk2_div6_en.hw,
		[CLKID_VCLK2_DIV12_EN]		= &tm2_vclk2_div12_en.hw,
		[CLKID_VCLK_DIV2]		= &tm2_vclk_div2.hw,
		[CLKID_VCLK_DIV4]		= &tm2_vclk_div4.hw,
		[CLKID_VCLK_DIV6]		= &tm2_vclk_div6.hw,
		[CLKID_VCLK_DIV12]		= &tm2_vclk_div12.hw,
		[CLKID_VCLK2_DIV2]		= &tm2_vclk2_div2.hw,
		[CLKID_VCLK2_DIV4]		= &tm2_vclk2_div4.hw,
		[CLKID_VCLK2_DIV6]		= &tm2_vclk2_div6.hw,
		[CLKID_VCLK2_DIV12]		= &tm2_vclk2_div12.hw,
		[CLKID_CTS_ENCI_SEL]		= &tm2_cts_enci_sel.hw,
		[CLKID_CTS_ENCP_SEL]		= &tm2_cts_encp_sel.hw,
		[CLKID_CTS_VDAC_SEL]		= &tm2_cts_vdac_sel.hw,
		[CLKID_HDMI_TX_SEL]		= &tm2_hdmi_tx_sel.hw,
		[CLKID_CTS_ENCI]		= &tm2_cts_enci.hw,
		[CLKID_CTS_ENCP]		= &tm2_cts_encp.hw,
		[CLKID_CTS_VDAC]		= &tm2_cts_vdac.hw,
		[CLKID_HDMI_TX]			= &tm2_hdmi_tx.hw,
		[CLKID_HDMI_SEL]		= &tm2_hdmi_sel.hw,
		[CLKID_HDMI_DIV]		= &tm2_hdmi_div.hw,
		[CLKID_HDMI]			= &tm2_hdmi.hw,
		[CLKID_MALI_0_SEL]		= &tm2_mali_0_sel.hw,
		[CLKID_MALI_0_DIV]		= &tm2_mali_0_div.hw,
		[CLKID_MALI_0]			= &tm2_mali_0.hw,
		[CLKID_MALI_1_SEL]		= &tm2_mali_1_sel.hw,
		[CLKID_MALI_1_DIV]		= &tm2_mali_1_div.hw,
		[CLKID_MALI_1]			= &tm2_mali_1.hw,
		[CLKID_MALI]			= &tm2_mali.hw,
		[CLKID_MPLL_50M_DIV]		= &tm2_mpll_50m_div.hw,
		[CLKID_MPLL_50M]		= &tm2_mpll_50m.hw,
		[CLKID_SYS_PLL_DIV16_EN]	= &tm2_sys_pll_div16_en.hw,
		[CLKID_SYS_PLL_DIV16]		= &tm2_sys_pll_div16.hw,
		[CLKID_CPU_CLK_DYN0_SEL]	= &tm2_cpu_clk_premux0.hw,
		[CLKID_CPU_CLK_DYN0_DIV]	= &tm2_cpu_clk_mux0_div.hw,
		[CLKID_CPU_CLK_DYN0]		= &tm2_cpu_clk_postmux0.hw,
		[CLKID_CPU_CLK_DYN1_SEL]	= &tm2_cpu_clk_premux1.hw,
		[CLKID_CPU_CLK_DYN1_DIV]	= &tm2_cpu_clk_mux1_div.hw,
		[CLKID_CPU_CLK_DYN1]		= &tm2_cpu_clk_postmux1.hw,
		[CLKID_CPU_CLK_DYN]		= &tm2_cpu_clk_dyn.hw,
		[CLKID_CPU_CLK]			= &tm2_cpu_clk.hw,
		[CLKID_CPU_CLK_DIV16_EN]	= &tm2_cpu_clk_div16_en.hw,
		[CLKID_CPU_CLK_DIV16]		= &tm2_cpu_clk_div16.hw,
		[CLKID_CPU_CLK_APB_DIV]		= &tm2_cpu_clk_apb_div.hw,
		[CLKID_CPU_CLK_APB]		= &tm2_cpu_clk_apb.hw,
		[CLKID_CPU_CLK_ATB_DIV]		= &tm2_cpu_clk_atb_div.hw,
		[CLKID_CPU_CLK_ATB]		= &tm2_cpu_clk_atb.hw,
		[CLKID_CPU_CLK_AXI_DIV]		= &tm2_cpu_clk_axi_div.hw,
		[CLKID_CPU_CLK_AXI]		= &tm2_cpu_clk_axi.hw,
		[CLKID_CPU_CLK_TRACE_DIV]	= &tm2_cpu_clk_trace_div.hw,
		[CLKID_CPU_CLK_TRACE]		= &tm2_cpu_clk_trace.hw,
		[CLKID_PCIE_PLL_DCO]		= &tm2_pcie_pll_dco.hw,
		[CLKID_PCIE_PLL_DCO_DIV2]	= &tm2_pcie_pll_dco_div2.hw,
		[CLKID_PCIE_PLL_OD]		= &tm2_pcie_pll_od.hw,
		[CLKID_PCIE_PLL]		= &tm2_pcie_pll.hw,
		[CLKID_VDEC_1_SEL]		= &tm2_vdec_1_sel.hw,
		[CLKID_VDEC_1_DIV]		= &tm2_vdec_1_div.hw,
		[CLKID_VDEC_1]			= &tm2_vdec_1.hw,
		[CLKID_VDEC_HEVC_SEL]		= &tm2_vdec_hevc_sel.hw,
		[CLKID_VDEC_HEVC_DIV]		= &tm2_vdec_hevc_div.hw,
		[CLKID_VDEC_HEVC]		= &tm2_vdec_hevc.hw,
		[CLKID_VDEC_HEVCF_SEL]		= &tm2_vdec_hevcf_sel.hw,
		[CLKID_VDEC_HEVCF_DIV]		= &tm2_vdec_hevcf_div.hw,
		[CLKID_VDEC_HEVCF]		= &tm2_vdec_hevcf.hw,
		[CLKID_TS_DIV]			= &tm2_ts_div.hw,
		[CLKID_TS]			= &tm2_ts.hw,
		[CLKID_PCIE_PRE_EN]		= &tm2_pcie_pre_gate.hw,
		[CLKID_PCIE_HCSL0]		= &tm2_pcie_hcsl0.hw,
		[CLKID_PCIE_HCSL1]		= &tm2_pcie_hcsl1.hw,
		[CLKID_DSI_MEAS_MUX]		= &tm2_dsi_meas_mux.hw,
		[CLKID_DSI_MEAS_DIV]		= &tm2_dsi_meas_div.hw,
		[CLKID_DSI_MEAS]		= &tm2_dsi_meas.hw,
		[CLKID_VDEC_P1_MUX]		= &tm2_vdec_p1_mux.hw,
		[CLKID_VDEC_P1_DIV]		= &tm2_vdec_p1_div.hw,
		[CLKID_VDEC_P1]			= &tm2_vdec_p1.hw,
		[CLKID_VDEC_MUX]		= &tm2_vdec_mux.hw,
		[CLKID_HCODEC_P0_MUX]		= &tm2_hcodec_p0_mux.hw,
		[CLKID_HCODEC_P0_DIV]		= &tm2_hcodec_p0_div.hw,
		[CLKID_HCODEC_P0]		= &tm2_hcodec_p0.hw,
		[CLKID_HCODEC_P1_MUX]		= &tm2_hcodec_p1_mux.hw,
		[CLKID_HCODEC_P1_DIV]		= &tm2_hcodec_p1_div.hw,
		[CLKID_HCODEC_P1]		= &tm2_hcodec_p1.hw,
		[CLKID_HCODEC_MUX]		= &tm2_hcodec_mux.hw,
		[CLKID_HEVC_P1_MUX]		= &tm2_hevc_p1_mux.hw,
		[CLKID_HEVC_P1_DIV]		= &tm2_hevc_p1_div.hw,
		[CLKID_HEVC_P1]			= &tm2_hevc_p1.hw,
		[CLKID_HEVC_MUX]		= &tm2_hevc_mux.hw,
		[CLKID_HEVCF_P1_MUX]		= &tm2_hevcf_p1_mux.hw,
		[CLKID_HEVCF_P1_DIV]		= &tm2_hevcf_p1_div.hw,
		[CLKID_HEVCF_P1]		= &tm2_hevcf_p1.hw,
		[CLKID_HEVCF_MUX]		= &tm2_hevcf_mux.hw,
		[CLKID_VPU_CLKB_TMP_MUX]	= &tm2_vpu_clkb_tmp_mux.hw,
		[CLKID_VPU_CLKB_TMP_DIV]	= &tm2_vpu_clkb_tmp_div.hw,
		[CLKID_VPU_CLKB_TMP]		= &tm2_vpu_clkb_tmp.hw,
		[CLKID_VPU_CLKB_DIV]		= &tm2_vpu_clkb_div.hw,
		[CLKID_VPU_CLKB]		= &tm2_vpu_clkb.hw,
		[CLKID_VPU_CLKC_P0_MUX]		= &tm2_vpu_clkc_p0_mux.hw,
		[CLKID_VPU_CLKC_P0_DIV]		= &tm2_vpu_clkc_p0_div.hw,
		[CLKID_VPU_CLKC_P0]		= &tm2_vpu_clkc_p0.hw,
		[CLKID_VPU_CLKC_P1_MUX]		= &tm2_vpu_clkc_p1_mux.hw,
		[CLKID_VPU_CLKC_P1_DIV]		= &tm2_vpu_clkc_p1_div.hw,
		[CLKID_VPU_CLKC_P1]		= &tm2_vpu_clkc_p1.hw,
		[CLKID_VPU_CLKC_MUX]		= &tm2_vpu_clkc_mux.hw,
		[CLKID_SPICC0_MUX]		= &tm2_spicc0_mux.hw,
		[CLKID_SPICC0_DIV]		= &tm2_spicc0_div.hw,
		[CLKID_SPICC0_GATE]		= &tm2_spicc0_gate.hw,
		[CLKID_SPICC1_MUX]		= &tm2_spicc1_mux.hw,
		[CLKID_SPICC1_DIV]		= &tm2_spicc1_div.hw,
		[CLKID_SPICC1_GATE]		= &tm2_spicc1_gate.hw,
		[CLKID_DSPA_A_MUX]		= &tm2_dspa_a_mux.hw,
		[CLKID_DSPA_A_DIV]		= &tm2_dspa_a_div.hw,
		[CLKID_DSPA_A_GATE]		= &tm2_dspa_a_gate.hw,
		[CLKID_DSPA_B_MUX]		= &tm2_dspa_b_mux.hw,
		[CLKID_DSPA_B_DIV]		= &tm2_dspa_b_div.hw,
		[CLKID_DSPA_B_GATE]		= &tm2_dspa_b_gate.hw,
		[CLKID_DSPA_MUX]		= &tm2_dspa_mux.hw,
		[CLKID_DSPB_A_MUX]		= &tm2_dspb_a_mux.hw,
		[CLKID_DSPB_A_DIV]		= &tm2_dspb_a_div.hw,
		[CLKID_DSPB_A_GATE]		= &tm2_dspb_a_gate.hw,
		[CLKID_DSPB_B_MUX]		= &tm2_dspb_b_mux.hw,
		[CLKID_DSPB_B_DIV]		= &tm2_dspb_b_div.hw,
		[CLKID_DSPB_B_GATE]		= &tm2_dspb_b_gate.hw,
		[CLKID_DSPB_MUX]		= &tm2_dspb_mux.hw,
		[CLKID_VIPNANOQ_CORE_MUX]	= &tm2_vipnanoq_core_mux.hw,
		[CLKID_VIPNANOQ_CORE_DIV]	= &tm2_vipnanoq_core_div.hw,
		[CLKID_VIPNANOQ_CORE_GATE]	= &tm2_vipnanoq_core_gate.hw,
		[CLKID_VIPNANOQ_AXI_MUX]	= &tm2_vipnanoq_axi_mux.hw,
		[CLKID_VIPNANOQ_AXI_DIV]	= &tm2_vipnanoq_axi_div.hw,
		[CLKID_VIPNANOQ_AXI_GATE]	= &tm2_vipnanoq_axi_gate.hw,
		[CLKID_DSU_CLK_DYN0_SEL]	= &tm2_dsu_clk_premux0.hw,
		[CLKID_DSU_CLK_DYN0_DIV]	= &tm2_dsu_clk_premux1.hw,
		[CLKID_DSU_CLK_DYN0]		= &tm2_dsu_clk_mux0_div.hw,
		[CLKID_DSU_CLK_DYN1_SEL]	= &tm2_dsu_clk_postmux0.hw,
		[CLKID_DSU_CLK_DYN1_DIV]	= &tm2_dsu_clk_mux1_div.hw,
		[CLKID_DSU_CLK_DYN1]		= &tm2_dsu_clk_postmux1.hw,
		[CLKID_DSU_CLK_DYN]		= &tm2_dsu_clk_dyn.hw,
		[CLKID_DSU_CLK_FINAL]		= &tm2_dsu_final_clk.hw,
		[CLKID_DSU_CLK]			= &tm2_dsu_clk.hw,
		[CLKID_VDIN_MEAS_MUX]		= &tm2_vdin_meas_mux.hw,
		[CLKID_VDIN_MEAS_DIV]		= &tm2_vdin_meas_div.hw,
		[CLKID_VDIN_MEAS]		= &tm2_vdin_meas.hw,
		[CLKID_HDMIRX_CFG_MUX]		= &tm2_hdmirx_cfg_mux.hw,
		[CLKID_HDMIRX_CFG_DIV]		= &tm2_hdmirx_cfg_div.hw,
		[CLKID_HDMIRX_CFG_GATE]		= &tm2_hdmirx_cfg_gate.hw,
		[CLKID_HDMIRX_MODET_MUX]	= &tm2_hdmirx_modet_mux.hw,
		[CLKID_HDMIRX_MODET_DIV]	= &tm2_hdmirx_modet_div.hw,
		[CLKID_HDMIRX_MODET_GATE]	= &tm2_hdmirx_modet_gate.hw,
		[CLKID_HDMIRX_ACR_MUX]		= &tm2_hdmirx_acr_mux.hw,
		[CLKID_HDMIRX_ACR_DIV]		= &tm2_hdmirx_acr_div.hw,
		[CLKID_HDMIRX_ACR_GATE]		= &tm2_hdmirx_acr_gate.hw,
		[CLKID_HDMIRX_METER_MUX]	= &tm2_hdmirx_meter_mux.hw,
		[CLKID_HDMIRX_METER_DIV]	= &tm2_hdmirx_meter_div.hw,
		[CLKID_HDMIRX_METER_GATE]	= &tm2_hdmirx_meter_gate.hw,
		[CLKID_HDMIRX_AXI_MUX]		= &tm2_hdmirx_axi_mux.hw,
		[CLKID_HDMIRX_AXI_DIV]		= &tm2_hdmirx_axi_div.hw,
		[CLKID_HDMIRX_AXI_GATE]		= &tm2_hdmirx_axi_gate.hw,
		[CLKID_HDMIRX_SKP_MUX]		= &tm2_hdmirx_skp_mux.hw,
		[CLKID_HDMIRX_SKP_DIV]		= &tm2_hdmirx_skp_div.hw,
		[CLKID_HDMIRX_SKP_GATE]		= &tm2_hdmirx_skp_gate.hw,
		[CLKID_HDMIRX_ESM_MUX]		= &tm2_hdmirx_esm_mux.hw,
		[CLKID_HDMIRX_ESM_DIV]		= &tm2_hdmirx_esm_div.hw,
		[CLKID_HDMIRX_ESM_GATE]		= &tm2_hdmirx_esm_gate.hw,
		[NR_CLKS]			= NULL,
	},
	.num = NR_CLKS,
};

/* Convenience table to populate regmap in .probe */
static struct clk_regmap *const tm2_clk_regmaps[] = {
	&tm2_clk81,
	&tm2_mpeg_clk_div,
	&tm2_mpeg_clk_sel,
	&tm2_sd_emmc_b_clk0,
	&tm2_sd_emmc_c_clk0,
	&tm2_sd_emmc_b_clk0_div,
	&tm2_sd_emmc_c_clk0_div,
	&tm2_sd_emmc_b_clk0_sel,
	&tm2_sd_emmc_c_clk0_sel,
	&tm2_mpll0,
	&tm2_mpll1,
	&tm2_mpll2,
	&tm2_mpll3,
	&tm2_mpll0_div,
	&tm2_mpll1_div,
	&tm2_mpll2_div,
	&tm2_mpll3_div,
	&tm2_fixed_pll,
	&tm2_sys_pll,
	&tm2_gp0_pll,
	&tm2_gp1_pll,
	&tm2_hifi_pll,
	&tm2_fixed_pll_dco,
	&tm2_sys_pll_dco,
	&tm2_gp0_pll_dco,
	&tm2_gp1_pll_dco,
	&tm2_hifi_pll_dco,
	&tm2_fclk_div2,
	&tm2_fclk_div3,
	&tm2_fclk_div4,
	&tm2_fclk_div5,
	&tm2_fclk_div7,
	&tm2_fclk_div2p5,
	&tm2_vpu_0_sel,
	&tm2_vpu_0_div,
	&tm2_vpu_0,
	&tm2_vpu_1_sel,
	&tm2_vpu_1_div,
	&tm2_vpu_1,
	&tm2_vpu,
	&tm2_vapb_0_sel,
	&tm2_vapb_0_div,
	&tm2_vapb_0,
	&tm2_vapb_1_sel,
	&tm2_vapb_1_div,
	&tm2_vapb_1,
	&tm2_vapb_sel,
	&tm2_vapb,
	&tm2_hdmi_pll_dco,
	&tm2_hdmi_pll_od,
	&tm2_hdmi_pll_od2,
	&tm2_hdmi_pll,
	&tm2_vid_pll_div,
	&tm2_vid_pll_sel,
	&tm2_vid_pll,
	&tm2_vclk_sel,
	&tm2_vclk2_sel,
	&tm2_vclk_input,
	&tm2_vclk2_input,
	&tm2_vclk_div,
	&tm2_vclk2_div,
	&tm2_vclk,
	&tm2_vclk2,
	&tm2_vclk_div1,
	&tm2_vclk_div2_en,
	&tm2_vclk_div4_en,
	&tm2_vclk_div6_en,
	&tm2_vclk_div12_en,
	&tm2_vclk2_div1,
	&tm2_vclk2_div2_en,
	&tm2_vclk2_div4_en,
	&tm2_vclk2_div6_en,
	&tm2_vclk2_div12_en,
	&tm2_cts_enci_sel,
	&tm2_cts_encp_sel,
	&tm2_cts_vdac_sel,
	&tm2_hdmi_tx_sel,
	&tm2_cts_enci,
	&tm2_cts_encp,
	&tm2_cts_vdac,
	&tm2_hdmi_tx,
	&tm2_hdmi_sel,
	&tm2_hdmi_div,
	&tm2_hdmi,
	&tm2_mali_0_sel,
	&tm2_mali_0_div,
	&tm2_mali_0,
	&tm2_mali_1_sel,
	&tm2_mali_1_div,
	&tm2_mali_1,
	&tm2_mali,
	&tm2_mpll_50m,
	&tm2_sys_pll_div16_en,
	&tm2_cpu_clk_premux0,
	&tm2_cpu_clk_mux0_div,
	&tm2_cpu_clk_postmux0,
	&tm2_cpu_clk_premux1,
	&tm2_cpu_clk_mux1_div,
	&tm2_cpu_clk_postmux1,
	&tm2_cpu_clk_dyn,
	&tm2_cpu_clk,
	&tm2_cpu_clk_div16_en,
	&tm2_cpu_clk_apb_div,
	&tm2_cpu_clk_apb,
	&tm2_cpu_clk_atb_div,
	&tm2_cpu_clk_atb,
	&tm2_cpu_clk_axi_div,
	&tm2_cpu_clk_axi,
	&tm2_cpu_clk_trace_div,
	&tm2_cpu_clk_trace,
	&tm2_pcie_pll_od,
	&tm2_pcie_pll_dco,
	&tm2_pcie_hcsl0,
	&tm2_pcie_hcsl1,
	&tm2_pcie_pre_gate,
	&tm2_vdec_1_sel,
	&tm2_vdec_1_div,
	&tm2_vdec_1,
	&tm2_vdec_hevc_sel,
	&tm2_vdec_hevc_div,
	&tm2_vdec_hevc,
	&tm2_vdec_hevcf_sel,
	&tm2_vdec_hevcf_div,
	&tm2_vdec_hevcf,
	&tm2_ts_div,
	&tm2_ts,
	&tm2_dsi_meas_mux,
	&tm2_dsi_meas_div,
	&tm2_dsi_meas,
	&tm2_vdin_meas_mux,
	&tm2_vdin_meas_div,
	&tm2_vdin_meas,
	&tm2_vdec_p1_mux,
	&tm2_vdec_p1_div,
	&tm2_vdec_p1,
	&tm2_vdec_mux,
	&tm2_hcodec_p0_mux,
	&tm2_hcodec_p0_div,
	&tm2_hcodec_p0,
	&tm2_hcodec_p1_mux,
	&tm2_hcodec_p1_div,
	&tm2_hcodec_p1,
	&tm2_hcodec_mux,
	&tm2_hevc_p1_mux,
	&tm2_hevc_p1_div,
	&tm2_hevc_p1,
	&tm2_hevc_mux,
	&tm2_hevcf_p1_mux,
	&tm2_hevcf_p1_div,
	&tm2_hevcf_p1,
	&tm2_hevcf_mux,
	&tm2_vpu_clkb_tmp_mux,
	&tm2_vpu_clkb_tmp_div,
	&tm2_vpu_clkb_tmp,
	&tm2_vpu_clkb_div,
	&tm2_vpu_clkb,
	&tm2_vpu_clkc_p0_mux,
	&tm2_vpu_clkc_p0_div,
	&tm2_vpu_clkc_p0,
	&tm2_vpu_clkc_p1_mux,
	&tm2_vpu_clkc_p1_div,
	&tm2_vpu_clkc_p1,
	&tm2_vpu_clkc_mux,
	&tm2_ddr,
	&tm2_dos,
	&tm2_eth_phy,
	&tm2_isa,
	&tm2_pl310,
	&tm2_periphs,
	&tm2_spicc_0,
	&tm2_i2c,
	&tm2_sana,
	&tm2_smart_card,
	&tm2_uart0,
	&tm2_spicc_1,
	&tm2_stream,
	&tm2_async_fifo,
	&tm2_tvfe,
	&tm2_hiu_reg,
	&tm2_hdmirx_pclk,
	&tm2_atv_demod,
	&tm2_assist_misc,
	&tm2_emmc_b,
	&tm2_emmc_c,
	&tm2_adec,
	&tm2_acodec,
	&tm2_tcon,
	&tm2_spi,
	&tm2_dsp,/*gate0 end*/
	&tm2_audio,
	&tm2_eth_core,
	&tm2_demux,
	&tm2_aififo,
	&tm2_adc,
	&tm2_uart1,
	&tm2_g2d,
	&tm2_reset,
	&tm2_u_parser,
	&tm2_usb_general,
	&tm2_ahb_arb0, /*gate1 end*/
	&tm2_ahb_data_bus,
	&tm2_ahb_ctrl_bus,
	&tm2_bt656,
	&tm2_usb1_to_ddr,
	&tm2_mmc_pclk,
	&tm2_uart2,
	&tm2_clk81_ts,
	&tm2_vpu_intr,
	&tm2_demod_comb,
	&tm2_gic,/*gate2 end*/
	&tm2_vclk2_venci0,
	&tm2_vclk2_venci1,
	&tm2_vclk2_vencp0,
	&tm2_vclk2_vencp1,
	&tm2_vclk2_venct0,
	&tm2_vclk2_venct1,
	&tm2_vclk2_other,
	&tm2_vclk2_enci,
	&tm2_vclk2_encp,
	&tm2_dac_clk,
	&tm2_enc480p,
	&tm2_rng1,
	&tm2_vclk2_enct,
	&tm2_vclk2_encl,
	&tm2_vclk2_venclmmc,
	&tm2_vclk2_vencl,
	&tm2_vclk2_other1,/* other end */
	&tm2_vipnanoq,
	&tm2_pcie1,
	&tm2_pcie0,
	&tm2_pcie1phy,
	&tm2_parserl,
	&tm2_hdcp22_pclk,
	&tm2_hdmitx_pclk,
	&tm2_pcie0phy,
	&tm2_hdmirx_axi_pclk,
	&tm2_dspb,
	&tm2_dspa,
	&tm2_dma,
	&tm2_efuse,
	&tm2_rom_boot,
	&tm2_reset_sec,
	&tm2_sec_ahb_apb3,
	&tm2_spicc0_mux,
	&tm2_spicc0_div,
	&tm2_spicc0_gate,
	&tm2_spicc1_mux,
	&tm2_spicc1_div,
	&tm2_spicc1_gate,
	&tm2_dspa_a_mux,
	&tm2_dspa_a_div,
	&tm2_dspa_a_gate,
	&tm2_dspa_b_mux,
	&tm2_dspa_b_div,
	&tm2_dspa_b_gate,
	&tm2_dspa_mux,
	&tm2_dspb_a_mux,
	&tm2_dspb_a_div,
	&tm2_dspb_a_gate,
	&tm2_dspb_b_mux,
	&tm2_dspb_b_div,
	&tm2_dspb_b_gate,
	&tm2_dspb_mux,
	&tm2_vipnanoq_core_mux,
	&tm2_vipnanoq_core_div,
	&tm2_vipnanoq_core_gate,
	&tm2_vipnanoq_axi_mux,
	&tm2_vipnanoq_axi_div,
	&tm2_vipnanoq_axi_gate,
	&tm2_dsu_clk_premux0,
	&tm2_dsu_clk_premux1,
	&tm2_dsu_clk_mux0_div,
	&tm2_dsu_clk_postmux0,
	&tm2_dsu_clk_mux1_div,
	&tm2_dsu_clk_postmux1,
	&tm2_dsu_clk_dyn,
	&tm2_dsu_final_clk,
	&tm2_dsu_clk,
	&tm2_hdmirx_cfg_mux,
	&tm2_hdmirx_cfg_div,
	&tm2_hdmirx_cfg_gate,
	&tm2_hdmirx_modet_mux,
	&tm2_hdmirx_modet_div,
	&tm2_hdmirx_modet_gate,
	&tm2_hdmirx_acr_mux,
	&tm2_hdmirx_acr_div,
	&tm2_hdmirx_acr_gate,
	&tm2_hdmirx_meter_mux,
	&tm2_hdmirx_meter_div,
	&tm2_hdmirx_meter_gate,
	&tm2_hdmirx_axi_mux,
	&tm2_hdmirx_axi_div,
	&tm2_hdmirx_axi_gate,
	&tm2_hdmirx_skp_mux,
	&tm2_hdmirx_skp_div,
	&tm2_hdmirx_skp_gate,
	&tm2_hdmirx_esm_mux,
	&tm2_hdmirx_esm_div,
	&tm2_hdmirx_esm_gate
};

static const struct reg_sequence tm2_init_regs[] = {
	{ .reg = HHI_MPLL_CNTL0,	.def = 0x00000543 },
};

static int meson_tm2_dvfs_setup_common(struct platform_device *pdev,
					struct clk_hw **hws)
{
	struct clk *notifier_clk;
	int ret;

	/* Setup clock notifier for cpu_clk_postmux0 */
	tm2_cpu_clk_postmux0_nb_data.fclk_div2 = &tm2_fclk_div2.hw;
	notifier_clk = tm2_cpu_clk_postmux0.hw.clk;
	ret = clk_notifier_register(notifier_clk,
				    &tm2_cpu_clk_postmux0_nb_data.nb);
	if (ret) {
		dev_err(&pdev->dev, "failed to register the cpu_clk_postmux0 notifier\n");
		return ret;
	}

	/* Setup clock notifier for cpu_clk_dyn mux */
	notifier_clk = tm2_cpu_clk_dyn.hw.clk;
	ret = clk_notifier_register(notifier_clk, &tm2_cpu_clk_mux_nb);
	if (ret) {
		dev_err(&pdev->dev, "failed to register the cpu_clk_dyn notifier\n");
		return ret;
	}

	/* Setup clock notifier for dsu */
	/* set tm2_dsu_clk_premux1 parent to fclk_div2 1G */
	ret = clk_set_parent(tm2_dsu_clk_premux1.hw.clk,
			     tm2_fclk_div2.hw.clk);
	if (ret < 0) {
		pr_err("%s: failed to set dsu parent\n", __func__);
		return ret;
	}

	ret = clk_notifier_register(tm2_dsu_clk_postmux0.hw.clk,
				    &tm2_dsu_clk_postmux0_nb_data.nb);
	if (ret) {
		dev_err(&pdev->dev, "failed to register dsu notifier\n");
		return ret;
	}

	return 0;
}

static int meson_tm2_dvfs_setup(struct platform_device *pdev)
{
	struct clk_hw **hws = tm2_hw_onecell_data.hws;
	struct clk *notifier_clk;
	int ret;

	ret = meson_tm2_dvfs_setup_common(pdev, hws);
	if (ret)
		return ret;

	/* Setup clock notifier for cpu_clk mux */
	notifier_clk = tm2_cpu_clk.hw.clk;
	ret = clk_notifier_register(notifier_clk, &tm2_cpu_clk_mux_nb);
	if (ret) {
		dev_err(&pdev->dev, "failed to register the cpu_clk notifier\n");
		return ret;
	}

	/* Setup clock notifier for sys_pll */
	notifier_clk = tm2_sys_pll.hw.clk;
	ret = clk_notifier_register(notifier_clk, &tm2_sys_pll_nb_data.nb);
	if (ret) {
		dev_err(&pdev->dev, "failed to register the sys_pll notifier\n");
		return ret;
	}

	return 0;
}

struct meson_tm2_data {
	const struct meson_eeclkc_data eeclkc_data;
	int (*dvfs_setup)(struct platform_device *pdev);
};

static int meson_tm2_probe(struct platform_device *pdev)
{
	const struct meson_eeclkc_data *eeclkc_data;
	const struct meson_tm2_data *tm2_data;
	int ret;

	eeclkc_data = of_device_get_match_data(&pdev->dev);
	if (!eeclkc_data)
		return -EINVAL;

	ret = meson_eeclkc_probe(pdev);
	if (ret)
		return ret;

	tm2_data = container_of(eeclkc_data, struct meson_tm2_data,
				 eeclkc_data);

	if (tm2_data->dvfs_setup)
		return tm2_data->dvfs_setup(pdev);

	return 0;
}

static const struct meson_tm2_data tm2_clkc_data = {
	.eeclkc_data = {
		.regmap_clks = tm2_clk_regmaps,
		.regmap_clk_num = ARRAY_SIZE(tm2_clk_regmaps),
		.hw_onecell_data = &tm2_hw_onecell_data,
		.init_regs = tm2_init_regs,
		.init_count = ARRAY_SIZE(tm2_init_regs),
	},
	.dvfs_setup = meson_tm2_dvfs_setup,
};

static const struct of_device_id clkc_match_table[] = {
	{
		.compatible = "amlogic,tm2-clkc",
		.data = &tm2_clkc_data.eeclkc_data
	},
	{}
};

static struct platform_driver tm2_driver = {
	.probe		= meson_tm2_probe,
	.driver		= {
		.name	= "tm2-clkc",
		.of_match_table = clkc_match_table,
	},
};

#ifndef MODULE
static int tm2_clkc_init(void)
{
	return platform_driver_register(&tm2_driver);
}

arch_initcall_sync(tm2_clkc_init);
#else
int __init meson_tm2_clkc_init(void)
{
	return platform_driver_register(&tm2_driver);
}
#endif

MODULE_LICENSE("GPL v2");
