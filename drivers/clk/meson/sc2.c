// SPDX-License-Identifier: GPL-2.0+
/*
 * Amlogic Meson-SC2 Clock Controller Driver
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
#include "sc2.h"
#include "clkcs_init.h"
#include <dt-bindings/clock/amlogic,sc2-clkc.h>
#include "clk-secure.h"

static DEFINE_SPINLOCK(meson_clk_lock);

static struct clk_regmap sc2_fixed_pll_dco = {
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

static struct clk_regmap sc2_fixed_pll = {
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
			&sc2_fixed_pll_dco.hw
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
static const struct pll_params_table sc2_sys_pll_params_table[] = {
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
static const struct pll_params_table sc2_sys_pll_params_table[] = {
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

static struct clk_regmap sc2_sys_pll_dco = {
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
		.table = sc2_sys_pll_params_table,
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
	},
	.hw.init = &(struct clk_init_data){
		.name = "sys_pll_dco",
		.ops = &meson_secure_clk_pll_ops,
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
static struct clk_regmap sc2_sys_pll = {
	.hw.init = &(struct clk_init_data){
		.name = "sys_pll",
		.ops = &meson_pll_clk_no_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_sys_pll_dco.hw
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
static struct clk_regmap sc2_sys_pll = {
	.data = &(struct clk_regmap_div_data){
		.offset = ANACTRL_SYSPLL_CTRL0,
		.shift = 16,
		.width = 3,
		.flags = CLK_DIVIDER_POWER_OF_TWO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sys_pll",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_sys_pll_dco.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};
#endif

static struct clk_fixed_factor sc2_fclk_div2_div = {
	.mult = 1,
	.div = 2,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div2_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) { &sc2_fixed_pll.hw },
		.num_parents = 1,
	},
};

static struct clk_regmap sc2_fclk_div2 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = ANACTRL_FIXPLL_CTRL1,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div2",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_fclk_div2_div.hw
		},
		.num_parents = 1,
		.flags = CLK_IS_CRITICAL,
	},
};

static struct clk_fixed_factor sc2_fclk_div3_div = {
	.mult = 1,
	.div = 3,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div3_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) { &sc2_fixed_pll.hw },
		.num_parents = 1,
	},
};

static struct clk_regmap sc2_fclk_div3 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = ANACTRL_FIXPLL_CTRL1,
		.bit_idx = 20,
	},
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div3",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_fclk_div3_div.hw
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

static struct clk_fixed_factor sc2_fclk_div4_div = {
	.mult = 1,
	.div = 4,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div4_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) { &sc2_fixed_pll.hw },
		.num_parents = 1,
	},
};

static struct clk_regmap sc2_fclk_div4 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = ANACTRL_FIXPLL_CTRL1,
		.bit_idx = 21,
	},
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div4",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_fclk_div4_div.hw
		},
		.num_parents = 1,
		.flags = CLK_IS_CRITICAL,
	},
};

static struct clk_fixed_factor sc2_fclk_div5_div = {
	.mult = 1,
	.div = 5,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div5_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) { &sc2_fixed_pll.hw },
		.num_parents = 1,
	},
};

static struct clk_regmap sc2_fclk_div5 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = ANACTRL_FIXPLL_CTRL1,
		.bit_idx = 22,
	},
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div5",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_fclk_div5_div.hw
		},
		.num_parents = 1,
		.flags = CLK_IS_CRITICAL,
	},
};

static struct clk_fixed_factor sc2_fclk_div7_div = {
	.mult = 1,
	.div = 7,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div7_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) { &sc2_fixed_pll.hw },
		.num_parents = 1,
	},
};

static struct clk_regmap sc2_fclk_div7 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = ANACTRL_FIXPLL_CTRL1,
		.bit_idx = 23,
	},
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div7",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_fclk_div7_div.hw
		},
		.num_parents = 1,
		.flags = CLK_IS_CRITICAL,
	},
};

static struct clk_fixed_factor sc2_fclk_div2p5_div = {
	.mult = 2,
	.div = 5,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div2p5_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_fixed_pll.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap sc2_fclk_div2p5 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = ANACTRL_FIXPLL_CTRL1,
		.bit_idx = 25,
	},
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div2p5",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_fclk_div2p5_div.hw
		},
		.num_parents = 1,
		.flags = CLK_IS_CRITICAL,
	},
};

#ifdef CONFIG_ARM
static const struct pll_params_table sc2_gp0_pll_table[] = {
	PLL_PARAMS(141, 1, 2), /* DCO = 3384M OD = 2 PLL = 846M */
	PLL_PARAMS(132, 1, 2), /* DCO = 3168M OD = 2 PLL = 792M */
	PLL_PARAMS(248, 1, 3), /* DCO = 5952M OD = 3 PLL = 744M */
	{ /* sentinel */  }
};
#else
static const struct pll_params_table sc2_gp0_pll_table[] = {
	PLL_PARAMS(141, 1), /* DCO = 3384M OD = 2 PLL = 846M */
	PLL_PARAMS(132, 1), /* DCO = 3168M OD = 2 PLL = 792M */
	PLL_PARAMS(248, 1), /* DCO = 5952M OD = 3 PLL = 744M */
	{ /* sentinel */  }
};
#endif

/*
 * Internal gp0 pll emulation configuration parameters
 */
static const struct reg_sequence sc2_gp0_init_regs[] = {
	{ .reg = ANACTRL_GP0PLL_CTRL1,	.def = 0x00000000 },
	{ .reg = ANACTRL_GP0PLL_CTRL2,	.def = 0x00000000 },
	{ .reg = ANACTRL_GP0PLL_CTRL3,	.def = 0x48681c00 },
	{ .reg = ANACTRL_GP0PLL_CTRL4,	.def = 0x88770290 },
	{ .reg = ANACTRL_GP0PLL_CTRL5,	.def = 0x39272000 },
	{ .reg = ANACTRL_GP0PLL_CTRL6,	.def = 0x56540000 }
};

static struct clk_regmap sc2_gp0_pll_dco = {
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
		.table = sc2_gp0_pll_table,
		.init_regs = sc2_gp0_init_regs,
		.init_count = ARRAY_SIZE(sc2_gp0_init_regs),
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
static struct clk_regmap sc2_gp0_pll = {
	.hw.init = &(struct clk_init_data){
		.name = "gp0_pll",
		.ops = &meson_pll_clk_no_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_gp0_pll_dco.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};
#else
static struct clk_regmap sc2_gp0_pll = {
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
			&sc2_gp0_pll_dco.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
	},
};
#endif

#ifdef CONFIG_ARM
static const struct pll_params_table sc2_gp1_pll_table[] = {
	PLL_PARAMS(200, 1, 2), /*DCO=4800M OD=1200M*/
	PLL_PARAMS(125, 1, 1), /*DCO=3000M OD=1500M*/
	{ /* sentinel */  }
};
#else
static const struct pll_params_table sc2_gp1_pll_table[] = {
	PLL_PARAMS(200, 1), /*DCO=4800M OD=1200M*/
	PLL_PARAMS(125, 1), /*DCO=3000M OD=1500M*/
	{ /* sentinel */  }
};
#endif

static struct clk_regmap sc2_gp1_pll_dco = {
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
		.table = sc2_gp1_pll_table,
	},
	.hw.init = &(struct clk_init_data){
		.name = "gp1_pll_dco",
		.ops = &meson_secure_clk_pll_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "xtal",
		},
		.num_parents = 1,
		/* This clock feeds the DSU, avoid disabling it */
		.flags = CLK_GET_RATE_NOCACHE | CLK_IS_CRITICAL,
	},
};

static struct clk_regmap sc2_gp1_pll = {
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
			&sc2_gp1_pll_dco.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

/*cpu_clk*/
/* Datasheet names this field as "premux0" */
static struct clk_regmap sc2_cpu_clk_premux0 = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CPUCTRL_SYS_CPU_CLK_CTRL,
		.mask = 0x3,
		.shift = 0,
		.flags = CLK_MUX_ROUND_CLOSEST,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cpu_clk_dyn0_sel",
		.ops = &clk_regmap_secure_mux_ops,
		.parent_data = (const struct clk_parent_data []) {
			{ .fw_name = "xtal", },
			{ .hw = &sc2_fclk_div2.hw },
			{ .hw = &sc2_fclk_div3.hw },
		},
		.num_parents = 3,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/* Datasheet names this field as "premux1" */
static struct clk_regmap sc2_cpu_clk_premux1 = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CPUCTRL_SYS_CPU_CLK_CTRL,
		.mask = 0x3,
		.shift = 16,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cpu_clk_dyn1_sel",
		.ops = &clk_regmap_secure_mux_ops,
		.parent_data = (const struct clk_parent_data []) {
			{ .fw_name = "xtal", },
			{ .hw = &sc2_fclk_div2.hw },
			{ .hw = &sc2_fclk_div3.hw },
		},
		.num_parents = 3,
		/* This sub-tree is used a parking clock */
		.flags = CLK_SET_RATE_NO_REPARENT
	},
};

/* Datasheet names this field as "mux0_divn_tcnt" */
static struct clk_regmap sc2_cpu_clk_mux0_div = {
	.data = &(struct meson_clk_cpu_dyndiv_data){
		.div = {
			.reg_off = CPUCTRL_SYS_CPU_CLK_CTRL,
			.shift = 4,
			.width = 6,
		},
		.dyn = {
			.reg_off = CPUCTRL_SYS_CPU_CLK_CTRL,
			.shift = 26,
			.width = 1,
		},
	},
	.hw.init = &(struct clk_init_data){
		.name = "cpu_clk_dyn0_div",
		.ops = &meson_secure_clk_cpu_dyndiv_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_cpu_clk_premux0.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/* Datasheet names this field as "postmux0" */
static struct clk_regmap sc2_cpu_clk_postmux0 = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CPUCTRL_SYS_CPU_CLK_CTRL,
		.mask = 0x1,
		.shift = 2,
		.flags = CLK_MUX_ROUND_CLOSEST,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cpu_clk_dyn0",
		.ops = &clk_regmap_secure_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_cpu_clk_premux0.hw,
			&sc2_cpu_clk_mux0_div.hw,
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/* Datasheet names this field as "Mux1_divn_tcnt" */
static struct clk_regmap sc2_cpu_clk_mux1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CPUCTRL_SYS_CPU_CLK_CTRL,
		.shift = 20,
		.width = 6,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cpu_clk_dyn1_div",
		.ops = &clk_regmap_secure_divider_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_cpu_clk_premux1.hw
		},
		.num_parents = 1,
	},
};

/* Datasheet names this field as "postmux1" */
static struct clk_regmap sc2_cpu_clk_postmux1 = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CPUCTRL_SYS_CPU_CLK_CTRL,
		.mask = 0x1,
		.shift = 18,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cpu_clk_dyn1",
		.ops = &clk_regmap_secure_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_cpu_clk_premux1.hw,
			&sc2_cpu_clk_mux1_div.hw,
		},
		.num_parents = 2,
		/* This sub-tree is used a parking clock */
		.flags = CLK_SET_RATE_NO_REPARENT,
	},
};

/* Datasheet names this field as "Final_dyn_mux_sel" */
static struct clk_regmap sc2_cpu_clk_dyn = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CPUCTRL_SYS_CPU_CLK_CTRL,
		.mask = 0x1,
		.shift = 10,
		.flags = CLK_MUX_ROUND_CLOSEST,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cpu_clk_dyn",
		.ops = &clk_regmap_secure_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_cpu_clk_postmux0.hw,
			&sc2_cpu_clk_postmux1.hw,
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/* Datasheet names this field as "Final_mux_sel" */
static struct clk_regmap sc2_cpu_clk = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CPUCTRL_SYS_CPU_CLK_CTRL,
		.mask = 0x1,
		.shift = 11,
		.flags = CLK_MUX_ROUND_CLOSEST,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cpu_clk",
		.ops = &clk_regmap_secure_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_cpu_clk_dyn.hw,
			&sc2_sys_pll.hw,
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/*dsu_clk*/
/* Datasheet names this field as "Final_mux_sel" */
/* Datasheet names this field as "premux0" */
static struct clk_regmap sc2_dsu_clk_premux0 = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CPUCTRL_SYS_CPU_CLK_CTRL5,
		.mask = 0x3,
		.shift = 0,
	},
	.hw.init = &(struct clk_init_data){
		.name = "dsu_clk_dyn0_sel",
		.ops = &clk_regmap_secure_mux_ops,
		.parent_data = (const struct clk_parent_data []) {
			{ .fw_name = "xtal", },
			{ .hw = &sc2_fclk_div2.hw },
			{ .hw = &sc2_fclk_div3.hw },
			{ .hw = &sc2_gp1_pll.hw },
		},
		.num_parents = 4,
		.flags = CLK_SET_RATE_PARENT | CLK_SET_RATE_NO_REPARENT,
	},
};

/* Datasheet names this field as "premux1" */
static struct clk_regmap sc2_dsu_clk_premux1 = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CPUCTRL_SYS_CPU_CLK_CTRL5,
		.mask = 0x3,
		.shift = 16,
	},
	.hw.init = &(struct clk_init_data){
		.name = "dsu_clk_dyn1_sel",
		.ops = &clk_regmap_secure_mux_ops,
		.parent_data = (const struct clk_parent_data []) {
			{ .fw_name = "xtal", },
			{ .hw = &sc2_fclk_div2.hw },
			{ .hw = &sc2_fclk_div3.hw },
			{ .hw = &sc2_gp1_pll.hw },
		},
		.num_parents = 4,
		.flags = CLK_SET_RATE_PARENT | CLK_SET_RATE_NO_REPARENT,
	},
};

/* Datasheet names this field as "Mux0_divn_tcnt" */
static struct clk_regmap sc2_dsu_clk_mux0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CPUCTRL_SYS_CPU_CLK_CTRL5,
		.shift = 4,
		.width = 6,
	},
	.hw.init = &(struct clk_init_data){
		.name = "dsu_clk_dyn0_div",
		.ops = &clk_regmap_secure_divider_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_dsu_clk_premux0.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/* Datasheet names this field as "postmux0" */
static struct clk_regmap sc2_dsu_clk_postmux0 = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CPUCTRL_SYS_CPU_CLK_CTRL5,
		.mask = 0x1,
		.shift = 2,
	},
	.hw.init = &(struct clk_init_data){
		.name = "dsu_clk_dyn0",
		.ops = &clk_regmap_secure_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_dsu_clk_premux0.hw,
			&sc2_dsu_clk_mux0_div.hw,
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/* Datasheet names this field as "Mux1_divn_tcnt" */
static struct clk_regmap sc2_dsu_clk_mux1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CPUCTRL_SYS_CPU_CLK_CTRL5,
		.shift = 20,
		.width = 6,
	},
	.hw.init = &(struct clk_init_data){
		.name = "dsu_clk_dyn1_div",
		.ops = &clk_regmap_secure_divider_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_dsu_clk_premux1.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/* Datasheet names this field as "postmux1" */
static struct clk_regmap sc2_dsu_clk_postmux1 = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CPUCTRL_SYS_CPU_CLK_CTRL5,
		.mask = 0x1,
		.shift = 18,
	},
	.hw.init = &(struct clk_init_data){
		.name = "dsu_clk_dyn1",
		.ops = &clk_regmap_secure_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_dsu_clk_premux1.hw,
			&sc2_dsu_clk_mux1_div.hw,
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/* Datasheet names this field as "Final_dyn_mux_sel" */
static struct clk_regmap sc2_dsu_clk_dyn = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CPUCTRL_SYS_CPU_CLK_CTRL5,
		.mask = 0x1,
		.shift = 10,
	},
	.hw.init = &(struct clk_init_data){
		.name = "dsu_clk_dyn",
		.ops = &clk_regmap_secure_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_dsu_clk_postmux0.hw,
			&sc2_dsu_clk_postmux1.hw,
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT | CLK_SET_RATE_NO_REPARENT,
	},
};

/* Datasheet names this field as "Final_mux_sel" */
static struct clk_regmap sc2_dsu_final_clk = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CPUCTRL_SYS_CPU_CLK_CTRL5,
		.mask = 0x1,
		.shift = 11,
	},
	.hw.init = &(struct clk_init_data){
		.name = "dsu_clk_final",
		.ops = &clk_regmap_secure_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_dsu_clk_dyn.hw,
			&sc2_sys_pll.hw,
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT | CLK_SET_RATE_NO_REPARENT,
	},
};

/* Datasheet names this field as "Cpu_clk_sync_mux_sel" bit 0 */
static struct clk_regmap sc2_cpu1_clk = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CPUCTRL_SYS_CPU_CLK_CTRL6,
		.mask = 0x1,
		.shift = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cpu1_clk",
		.ops = &clk_regmap_secure_mux_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_cpu_clk.hw,
			/* This CPU also have a dedicated clock tree */
		},
		.num_parents = 1,
	},
};

/* Datasheet names this field as "Cpu_clk_sync_mux_sel" bit 1 */
static struct clk_regmap sc2_cpu2_clk = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CPUCTRL_SYS_CPU_CLK_CTRL6,
		.mask = 0x1,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cpu2_clk",
		.ops = &clk_regmap_secure_mux_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_cpu_clk.hw,
			/* This CPU also have a dedicated clock tree */
		},
		.num_parents = 1,
	},
};

/* Datasheet names this field as "Cpu_clk_sync_mux_sel" bit 2 */
static struct clk_regmap sc2_cpu3_clk = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CPUCTRL_SYS_CPU_CLK_CTRL6,
		.mask = 0x1,
		.shift = 26,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cpu3_clk",
		.ops = &clk_regmap_secure_mux_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_cpu_clk.hw,
			/* This CPU also have a dedicated clock tree */
		},
		.num_parents = 1,
	},
};

/* Datasheet names this field as "Cpu_clk_sync_mux_sel" bit 4 */
static struct clk_regmap sc2_dsu_clk = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CPUCTRL_SYS_CPU_CLK_CTRL6,
		.mask = 0x1,
		.shift = 27,
	},
	.hw.init = &(struct clk_init_data){
		.name = "dsu_clk",
		.ops = &clk_regmap_secure_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_cpu_clk.hw,
			&sc2_dsu_final_clk.hw,
		},
		.num_parents = 2,
	},
};

static int sc2_cpu_clk_mux_notifier_cb(struct notifier_block *nb,
				       unsigned long event, void *data)
{
	if (event == POST_RATE_CHANGE || event == PRE_RATE_CHANGE) {
		/* Wait for clock propagation before/after changing the mux */
		udelay(100);
		return NOTIFY_OK;
	}

	return NOTIFY_DONE;
}

static struct notifier_block sc2_cpu_clk_mux_nb = {
	.notifier_call = sc2_cpu_clk_mux_notifier_cb,
};

struct sc2_cpu_clk_postmux_nb_data {
	struct notifier_block nb;
	struct clk_hw *xtal;
	struct clk_hw *cpu_clk_dyn;
	struct clk_hw *cpu_clk_postmux0;
	struct clk_hw *cpu_clk_postmux1;
	struct clk_hw *cpu_clk_premux1;
};

static int sc2_cpu_clk_postmux_notifier_cb(struct notifier_block *nb,
					   unsigned long event, void *data)
{
	struct sc2_cpu_clk_postmux_nb_data *nb_data =
		container_of(nb, struct sc2_cpu_clk_postmux_nb_data, nb);

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

static struct sc2_cpu_clk_postmux_nb_data sc2_cpu_clk_postmux0_nb_data = {
	.cpu_clk_dyn = &sc2_cpu_clk_dyn.hw,
	.cpu_clk_postmux0 = &sc2_cpu_clk_postmux0.hw,
	.cpu_clk_postmux1 = &sc2_cpu_clk_postmux1.hw,
	.cpu_clk_premux1 = &sc2_cpu_clk_premux1.hw,
	.nb.notifier_call = sc2_cpu_clk_postmux_notifier_cb,
};

struct sc2_sys_pll_nb_data {
	struct notifier_block nb;
	struct clk_hw *sys_pll;
	struct clk_hw *cpu_clk;
	struct clk_hw *cpu_clk_dyn;
};

static int sc2_sys_pll_notifier_cb(struct notifier_block *nb,
				   unsigned long event, void *data)
{
	struct sc2_sys_pll_nb_data *nb_data =
		container_of(nb, struct sc2_sys_pll_nb_data, nb);

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

static struct sc2_sys_pll_nb_data sc2_sys_pll_nb_data = {
	.sys_pll = &sc2_sys_pll.hw,
	.cpu_clk = &sc2_cpu_clk.hw,
	.cpu_clk_dyn = &sc2_cpu_clk_dyn.hw,
	.nb.notifier_call = sc2_sys_pll_notifier_cb,
};

/* sc2 dsu notify */
struct sc2_dsu_clk_postmux_nb_data {
	struct notifier_block nb;
	struct clk_hw *dsu_clk_dyn;
	struct clk_hw *dsu_clk_postmux0;
	struct clk_hw *dsu_clk_postmux1;
};

static int sc2_dsu_clk_postmux_notifier_cb(struct notifier_block *nb,
					   unsigned long event, void *data)
{
	struct sc2_dsu_clk_postmux_nb_data *nb_data =
		container_of(nb, struct sc2_dsu_clk_postmux_nb_data, nb);
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

static struct sc2_dsu_clk_postmux_nb_data sc2_dsu_clk_postmux0_nb_data = {
	.dsu_clk_dyn = &sc2_dsu_clk_dyn.hw,
	.dsu_clk_postmux0 = &sc2_dsu_clk_postmux0.hw,
	.dsu_clk_postmux1 = &sc2_dsu_clk_postmux1.hw,
	.nb.notifier_call = sc2_dsu_clk_postmux_notifier_cb,
};

#ifdef CONFIG_ARM
static const struct pll_params_table sc2_hifi_pll_table[] = {
	PLL_PARAMS(150, 1, 1), /* DCO = 1806.336M OD = 1 */
	{ /* sentinel */  }
};
#else
static const struct pll_params_table sc2_hifi_pll_table[] = {
	PLL_PARAMS(150, 1), /* DCO = 1806.336M */
	{ /* sentinel */  }
};
#endif

/*
 * Internal hifi pll emulation configuration parameters
 */
static const struct reg_sequence sc2_hifi_init_regs[] = {
	{ .reg = ANACTRL_HIFIPLL_CTRL1,	.def = 0x00010e56 },
	{ .reg = ANACTRL_HIFIPLL_CTRL2,	.def = 0x00000000 },
	{ .reg = ANACTRL_HIFIPLL_CTRL3,	.def = 0x6a285c00 },
	{ .reg = ANACTRL_HIFIPLL_CTRL4,	.def = 0x65771290 },
	{ .reg = ANACTRL_HIFIPLL_CTRL5,	.def = 0x39272000 },
	{ .reg = ANACTRL_HIFIPLL_CTRL6,	.def = 0x56540000 }
};

static struct clk_regmap sc2_hifi_pll_dco = {
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
		.table = sc2_hifi_pll_table,
		.init_regs = sc2_hifi_init_regs,
		.init_count = ARRAY_SIZE(sc2_hifi_init_regs),
		.flags = CLK_MESON_PLL_ROUND_CLOSEST,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hifi_pll_dco",
		.ops = &meson_clk_pll_ops,
		.parent_names = (const char *[]){ "xtal" },
		.num_parents = 1,
	},
};

static struct clk_regmap sc2_hifi_pll = {
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
			&sc2_hifi_pll_dco.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
	},
};

/*
 * The Meson sc2 PCIE PLL is fined tuned to deliver a very precise
 * 100MHz reference clock for the PCIe Analog PHY, and thus requires
 * a strict register sequence to enable the PLL.
 */
static const struct reg_sequence sc2_pcie_pll_init_regs[] = {
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
static const struct pll_params_table sc2_pcie_pll_table[] = {
	PLL_PARAMS(200, 1),
	{0, 0}
};
#else
static const struct pll_params_table sc2_pcie_pll_table[] = {
	PLL_PARAMS(200, 1, 0),
	{0, 0, 0}
};
#endif

static struct clk_regmap sc2_pcie_pll_dco = {
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
		.table = sc2_pcie_pll_table,
		.init_regs = sc2_pcie_pll_init_regs,
		.init_count = ARRAY_SIZE(sc2_pcie_pll_init_regs),
	},
	.hw.init = &(struct clk_init_data){
		.name = "pcie_pll_dco",
		.ops = &meson_clk_pcie_pll_ops,
		.parent_names = (const char *[]){ "xtal" },
		.num_parents = 1,
	},
};

static struct clk_fixed_factor sc2_pcie_pll_dco_div2 = {
	.mult = 1,
	.div = 2,
	.hw.init = &(struct clk_init_data){
		.name = "pcie_pll_dco_div2",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_pcie_pll_dco.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap sc2_pcie_pll_od = {
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
			&sc2_pcie_pll_dco_div2.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_fixed_factor sc2_pcie_pll = {
	.mult = 1,
	.div = 2,
	.hw.init = &(struct clk_init_data){
		.name = "pcie_pll",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_pcie_pll_od.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap sc2_pcie_hcsl = {
	.data = &(struct clk_regmap_gate_data){
		.offset = ANACTRL_PCIEPLL_CTRL5,
		.bit_idx = 3,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pcie_hcsl",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &sc2_pcie_pll.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap sc2_hdmi_pll_dco = {
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

static struct clk_regmap sc2_hdmi_pll_od = {
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
			&sc2_hdmi_pll_dco.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap sc2_hdmi_pll = {
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
			&sc2_hdmi_pll_od.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_fixed_factor sc2_mpll_50m_div = {
	.mult = 1,
	.div = 80,
	.hw.init = &(struct clk_init_data){
		.name = "mpll_50m_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_fixed_pll_dco.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap sc2_mpll_50m = {
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
			{ .hw = &sc2_mpll_50m_div.hw },
		},
		.num_parents = 2,
	},
};

static struct clk_fixed_factor sc2_mpll_prediv = {
	.mult = 1,
	.div = 2,
	.hw.init = &(struct clk_init_data){
		.name = "mpll_prediv",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_fixed_pll_dco.hw
		},
		.num_parents = 1,
	},
};

static const struct reg_sequence sc2_mpll0_init_regs[] = {
	{ .reg = ANACTRL_MPLL_CTRL2, .def = 0x40000033 }
};

static struct clk_regmap sc2_mpll0_div = {
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
		.init_regs = sc2_mpll0_init_regs,
		.init_count = ARRAY_SIZE(sc2_mpll0_init_regs),
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpll0_div",
		.ops = &meson_clk_mpll_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_mpll_prediv.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap sc2_mpll0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = ANACTRL_MPLL_CTRL1,
		.bit_idx = 31,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpll0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &sc2_mpll0_div.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct reg_sequence sc2_mpll1_init_regs[] = {
	{ .reg = ANACTRL_MPLL_CTRL4,	.def = 0x40000033 }
};

static struct clk_regmap sc2_mpll1_div = {
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
		.init_regs = sc2_mpll1_init_regs,
		.init_count = ARRAY_SIZE(sc2_mpll1_init_regs),
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpll1_div",
		.ops = &meson_clk_mpll_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_mpll_prediv.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap sc2_mpll1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = ANACTRL_MPLL_CTRL3,
		.bit_idx = 31,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpll1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &sc2_mpll1_div.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct reg_sequence sc2_mpll2_init_regs[] = {
	{ .reg = ANACTRL_MPLL_CTRL6, .def = 0x40000033 }
};

static struct clk_regmap sc2_mpll2_div = {
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
		.init_regs = sc2_mpll2_init_regs,
		.init_count = ARRAY_SIZE(sc2_mpll2_init_regs),
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpll2_div",
		.ops = &meson_clk_mpll_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_mpll_prediv.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap sc2_mpll2 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = ANACTRL_MPLL_CTRL5,
		.bit_idx = 31,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpll2",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &sc2_mpll2_div.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct reg_sequence sc2_mpll3_init_regs[] = {
	{ .reg = ANACTRL_MPLL_CTRL8, .def = 0x40000033 }
};

static struct clk_regmap sc2_mpll3_div = {
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
		.init_regs = sc2_mpll3_init_regs,
		.init_count = ARRAY_SIZE(sc2_mpll3_init_regs),
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpll3_div",
		.ops = &meson_clk_mpll_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_mpll_prediv.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap sc2_mpll3 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = ANACTRL_MPLL_CTRL7,
		.bit_idx = 31,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpll3",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &sc2_mpll3_div.hw },
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
static struct clk_regmap sc2_rtc_32k_clkin = {
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

static const struct meson_clk_dualdiv_param sc2_32k_div_table[] = {
	{
		.dual	= 1,
		.n1	= 733,
		.m1	= 8,
		.n2	= 732,
		.m2	= 11,
	},
	{}
};

static struct clk_regmap sc2_rtc_32k_div = {
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
		.table = sc2_32k_div_table,
	},
	.hw.init = &(struct clk_init_data){
		.name = "rtc_32k_div",
		.ops = &meson_clk_dualdiv_ops,
		.parent_names = (const char *[]){ "rtc_32k_clkin" },
		.num_parents = 1,
	},
};

static struct clk_regmap sc2_rtc_32k_xtal = {
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

static struct clk_regmap sc2_rtc_32k_sel = {
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

static struct clk_regmap sc2_rtc_clk = {
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
static u32 mux_table_sys_ab_clk_sel[] = { 0, 1, 2, 3, 4, 6, 7 };
static const char * const sys_ab_clk_parent_names[] = {
	"xtal", "fclk_div2", "fclk_div3", "fclk_div4",
	"fclk_div5", "axi_clk_frcpu", "fclk_div7", "rtc_clk"
};

static struct clk_regmap sc2_sysclk_b_sel = {
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

static struct clk_regmap sc2_sysclk_b_div = {
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

static struct clk_regmap sc2_sysclk_b = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_SYS_CLK_CTRL0,
		.bit_idx = 29,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sysclk_b",
		.ops = &clk_regmap_gate_ro_ops,
		.parent_names = (const char *[]){ "sysclk_b_div" },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED
			 | CLK_IS_CRITICAL,
	},
};

static struct clk_regmap sc2_sysclk_a_sel = {
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

static struct clk_regmap sc2_sysclk_a_div = {
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

static struct clk_regmap sc2_sysclk_a = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_SYS_CLK_CTRL0,
		.bit_idx = 13,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sysclk_a",
		.ops = &clk_regmap_gate_ro_ops,
		.parent_names = (const char *[]){ "sysclk_a_div" },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED
			 | CLK_IS_CRITICAL,
	},
};

static const char * const sys_clk_parent_names[] = {
	"sysclk_a", "sysclk_b"};

static struct clk_regmap sc2_sys_clk = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_SYS_CLK_CTRL0,
		.mask = 0x1,
		.shift = 31,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sys_clk",
		.ops = &clk_regmap_mux_ro_ops,
		.parent_names = sys_clk_parent_names,
		.num_parents = ARRAY_SIZE(sys_clk_parent_names),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

/*axi clk*/

/*ceca_clk*/
static struct clk_regmap sc2_ceca_32k_clkin = {
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

static struct clk_regmap sc2_ceca_32k_div = {
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
		.table = sc2_32k_div_table,
	},
	.hw.init = &(struct clk_init_data){
		.name = "ceca_32k_div",
		.ops = &meson_clk_dualdiv_ops,
		.parent_names = (const char *[]){ "ceca_32k_clkin" },
		.num_parents = 1,
	},
};

static struct clk_regmap sc2_ceca_32k_sel_pre = {
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

static struct clk_regmap sc2_ceca_32k_sel = {
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

static struct clk_regmap sc2_ceca_32k_clkout = {
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
static struct clk_regmap sc2_cecb_32k_clkin = {
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

static struct clk_regmap sc2_cecb_32k_div = {
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
		.table = sc2_32k_div_table,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cecb_32k_div",
		.ops = &meson_clk_dualdiv_ops,
		.parent_names = (const char *[]){ "cecb_32k_clkin" },
		.num_parents = 1,
	},
};

static struct clk_regmap sc2_cecb_32k_sel_pre = {
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

static struct clk_regmap sc2_cecb_32k_sel = {
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

static struct clk_regmap sc2_cecb_32k_clkout = {
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

/*cts_sc_clk*/
static const struct clk_parent_data sc2_sc_parent_data[] = {
	{ .hw = &sc2_fclk_div4.hw },
	{ .hw = &sc2_fclk_div3.hw },
	{ .hw = &sc2_fclk_div5.hw },
	{ .fw_name = "xtal", }
};

static struct clk_regmap sc2_sc_clk_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_SC_CLK_CTRL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sc_clk_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = sc2_sc_parent_data,
		.num_parents = ARRAY_SIZE(sc2_sc_parent_data),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap sc2_sc_clk_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_SC_CLK_CTRL,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sc_clk_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_sc_clk_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap sc2_sc_clk_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_SC_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sc_clk_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_sc_clk_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

/*rama_clk*/

/*dspa_clk*/
static const struct clk_parent_data sc2_dsp_parent_hws[] = {
	{ .fw_name = "xtal", },
	{ .hw = &sc2_fclk_div2p5.hw },
	{ .hw = &sc2_fclk_div3.hw },
	{ .hw = &sc2_fclk_div5.hw },
	{ .hw = &sc2_hifi_pll.hw },
	{ .hw = &sc2_fclk_div4.hw },
	{ .hw = &sc2_fclk_div7.hw },
	{ .hw = &sc2_rtc_clk.hw }
};

static struct clk_regmap sc2_dspa_a_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_DSPA_CLK_CTRL0,
		.mask = 0x7,
		.shift = 10,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "dspa_a_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = sc2_dsp_parent_hws,
		.num_parents = ARRAY_SIZE(sc2_dsp_parent_hws),
	},
};

static struct clk_regmap sc2_dspa_a_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_DSPA_CLK_CTRL0,
		.shift = 0,
		.width = 10,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "dspa_a_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_dspa_a_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap sc2_dspa_a_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_DSPA_CLK_CTRL0,
		.bit_idx = 13,
	},
	.hw.init = &(struct clk_init_data){
		.name = "dspa_a_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_dspa_a_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap sc2_dspa_b_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_DSPA_CLK_CTRL0,
		.mask = 0x7,
		.shift = 26,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "dspa_b_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = sc2_dsp_parent_hws,
		.num_parents = ARRAY_SIZE(sc2_dsp_parent_hws),
	},
};

static struct clk_regmap sc2_dspa_b_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_DSPA_CLK_CTRL0,
		.shift = 16,
		.width = 10,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "dspa_b_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_dspa_b_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap sc2_dspa_b_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_DSPA_CLK_CTRL0,
		.bit_idx = 29,
	},
	.hw.init = &(struct clk_init_data){
		.name = "dspa_b_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_dspa_b_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap sc2_dspa_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_DSPA_CLK_CTRL0,
		.mask = 0x1,
		.shift = 15,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "dspa_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_dspa_a_gate.hw,
			&sc2_dspa_b_gate.hw,
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/*12_24M clk*/
static struct clk_regmap sc2_24M_clk_gate = {
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

static struct clk_fixed_factor sc2_12M_clk_div = {
	.mult = 1,
	.div = 2,
	.hw.init = &(struct clk_init_data){
		.name = "24m_div2",
		.ops = &clk_fixed_factor_ops,
		.parent_names = (const char *[]){ "24m" },
		.num_parents = 1,
	},
};

static struct clk_regmap sc2_12M_clk_gate = {
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

static struct clk_regmap sc2_25M_clk_div = {
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

static struct clk_regmap sc2_25M_clk_gate = {
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
static struct clk_regmap sc2_vid_pll_div = {
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
		.parent_hws = (const struct clk_hw *[]) { &sc2_hdmi_pll.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
	},
};

static const struct clk_hw *sc2_vid_pll_parent_hws[] = {
	&sc2_vid_pll_div.hw,
	&sc2_hdmi_pll.hw
};

static struct clk_regmap sc2_vid_pll_sel = {
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
		.parent_hws = sc2_vid_pll_parent_hws,
		.num_parents = ARRAY_SIZE(sc2_vid_pll_parent_hws),
		.flags = CLK_SET_RATE_NO_REPARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap sc2_vid_pll = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VID_PLL_CLK_DIV,
		.bit_idx = 19,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vid_pll",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_vid_pll_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static const struct clk_hw *sc2_vclk_parent_hws[] = {
	&sc2_vid_pll.hw,
	&sc2_gp0_pll.hw,
	&sc2_hifi_pll.hw,
	&sc2_mpll1.hw,
	&sc2_fclk_div3.hw,
	&sc2_fclk_div4.hw,
	&sc2_fclk_div5.hw,
	&sc2_fclk_div7.hw
};

static struct clk_regmap sc2_vclk_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VID_CLK_CTRL,
		.mask = 0x7,
		.shift = 16,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vclk_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = sc2_vclk_parent_hws,
		.num_parents = ARRAY_SIZE(sc2_vclk_parent_hws),
		.flags = CLK_SET_RATE_NO_REPARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap sc2_vclk2_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VIID_CLK_CTRL,
		.mask = 0x7,
		.shift = 16,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vclk2_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = sc2_vclk_parent_hws,
		.num_parents = ARRAY_SIZE(sc2_vclk_parent_hws),
		.flags = CLK_SET_RATE_NO_REPARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap sc2_vclk_input = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VID_CLK_DIV,
		.bit_idx = 16,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk_input",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &sc2_vclk_sel.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap sc2_vclk2_input = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VIID_CLK_DIV,
		.bit_idx = 16,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk2_input",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &sc2_vclk2_sel.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap sc2_vclk_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_VID_CLK_DIV,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vclk_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_vclk_input.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap sc2_vclk2_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_VIID_CLK_DIV,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vclk2_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_vclk2_input.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap sc2_vclk = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VID_CLK_CTRL,
		.bit_idx = 19,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &sc2_vclk_div.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap sc2_vclk2 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VIID_CLK_CTRL,
		.bit_idx = 19,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk2",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &sc2_vclk2_div.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap sc2_vclk_div1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VID_CLK_CTRL,
		.bit_idx = 0,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk_div1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &sc2_vclk.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap sc2_vclk_div2_en = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VID_CLK_CTRL,
		.bit_idx = 1,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk_div2_en",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &sc2_vclk.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap sc2_vclk_div4_en = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VID_CLK_CTRL,
		.bit_idx = 2,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk_div4_en",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &sc2_vclk.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap sc2_vclk_div6_en = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VID_CLK_CTRL,
		.bit_idx = 3,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk_div6_en",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &sc2_vclk.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap sc2_vclk_div12_en = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VID_CLK_CTRL,
		.bit_idx = 4,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk_div12_en",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &sc2_vclk.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap sc2_vclk2_div1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VIID_CLK_CTRL,
		.bit_idx = 0,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk2_div1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &sc2_vclk2.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap sc2_vclk2_div2_en = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VIID_CLK_CTRL,
		.bit_idx = 1,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk2_div2_en",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &sc2_vclk2.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap sc2_vclk2_div4_en = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VIID_CLK_CTRL,
		.bit_idx = 2,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk2_div4_en",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &sc2_vclk2.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap sc2_vclk2_div6_en = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VIID_CLK_CTRL,
		.bit_idx = 3,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk2_div6_en",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &sc2_vclk2.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap sc2_vclk2_div12_en = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VIID_CLK_CTRL,
		.bit_idx = 4,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk2_div12_en",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &sc2_vclk2.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_fixed_factor sc2_vclk_div2 = {
	.mult = 1,
	.div = 2,
	.hw.init = &(struct clk_init_data){
		.name = "vclk_div2",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_vclk_div2_en.hw
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor sc2_vclk_div4 = {
	.mult = 1,
	.div = 4,
	.hw.init = &(struct clk_init_data){
		.name = "vclk_div4",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_vclk_div4_en.hw
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor sc2_vclk_div6 = {
	.mult = 1,
	.div = 6,
	.hw.init = &(struct clk_init_data){
		.name = "vclk_div6",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_vclk_div6_en.hw
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor sc2_vclk_div12 = {
	.mult = 1,
	.div = 12,
	.hw.init = &(struct clk_init_data){
		.name = "vclk_div12",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_vclk_div12_en.hw
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor sc2_vclk2_div2 = {
	.mult = 1,
	.div = 2,
	.hw.init = &(struct clk_init_data){
		.name = "vclk2_div2",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_vclk2_div2_en.hw
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor sc2_vclk2_div4 = {
	.mult = 1,
	.div = 4,
	.hw.init = &(struct clk_init_data){
		.name = "vclk2_div4",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_vclk2_div4_en.hw
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor sc2_vclk2_div6 = {
	.mult = 1,
	.div = 6,
	.hw.init = &(struct clk_init_data){
		.name = "vclk2_div6",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_vclk2_div6_en.hw
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor sc2_vclk2_div12 = {
	.mult = 1,
	.div = 12,
	.hw.init = &(struct clk_init_data){
		.name = "vclk2_div12",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_vclk2_div12_en.hw
		},
		.num_parents = 1,
	},
};

static u32 mux_table_cts_sel[] = { 0, 1, 2, 3, 4, 8, 9, 10, 11, 12 };
static const struct clk_hw *sc2_cts_parent_hws[] = {
	&sc2_vclk_div1.hw,
	&sc2_vclk_div2.hw,
	&sc2_vclk_div4.hw,
	&sc2_vclk_div6.hw,
	&sc2_vclk_div12.hw,
	&sc2_vclk2_div1.hw,
	&sc2_vclk2_div2.hw,
	&sc2_vclk2_div4.hw,
	&sc2_vclk2_div6.hw,
	&sc2_vclk2_div12.hw
};

static struct clk_regmap sc2_cts_enci_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VID_CLK_DIV,
		.mask = 0xf,
		.shift = 28,
		.table = mux_table_cts_sel,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cts_enci_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = sc2_cts_parent_hws,
		.num_parents = ARRAY_SIZE(sc2_cts_parent_hws),
		.flags = CLK_SET_RATE_NO_REPARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap sc2_cts_encp_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VID_CLK_DIV,
		.mask = 0xf,
		.shift = 20,
		.table = mux_table_cts_sel,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cts_encp_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = sc2_cts_parent_hws,
		.num_parents = ARRAY_SIZE(sc2_cts_parent_hws),
		.flags = CLK_SET_RATE_NO_REPARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap sc2_cts_vdac_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VIID_CLK_DIV,
		.mask = 0xf,
		.shift = 28,
		.table = mux_table_cts_sel,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cts_vdac_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = sc2_cts_parent_hws,
		.num_parents = ARRAY_SIZE(sc2_cts_parent_hws),
		.flags = CLK_SET_RATE_NO_REPARENT | CLK_GET_RATE_NOCACHE,
	},
};

/* TOFIX: add support for cts_tcon */
static u32 mux_table_hdmi_tx_sel[] = { 0, 1, 2, 3, 4, 8, 9, 10, 11, 12 };
static const struct clk_hw *sc2_cts_hdmi_tx_parent_hws[] = {
	&sc2_vclk_div1.hw,
	&sc2_vclk_div2.hw,
	&sc2_vclk_div4.hw,
	&sc2_vclk_div6.hw,
	&sc2_vclk_div12.hw,
	&sc2_vclk2_div1.hw,
	&sc2_vclk2_div2.hw,
	&sc2_vclk2_div4.hw,
	&sc2_vclk2_div6.hw,
	&sc2_vclk2_div12.hw
};

static struct clk_regmap sc2_hdmi_tx_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_HDMI_CLK_CTRL,
		.mask = 0xf,
		.shift = 16,
		.table = mux_table_hdmi_tx_sel,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmi_tx_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = sc2_cts_hdmi_tx_parent_hws,
		.num_parents = ARRAY_SIZE(sc2_cts_hdmi_tx_parent_hws),
		.flags = CLK_SET_RATE_NO_REPARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap sc2_cts_enci = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VID_CLK_CTRL2,
		.bit_idx = 0,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "cts_enci",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_cts_enci_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap sc2_cts_encp = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VID_CLK_CTRL2,
		.bit_idx = 2,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "cts_encp",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_cts_encp_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap sc2_cts_vdac = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VID_CLK_CTRL2,
		.bit_idx = 4,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "cts_vdac",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_cts_vdac_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap sc2_hdmi_tx = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VID_CLK_CTRL2,
		.bit_idx = 5,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmi_tx",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_hdmi_tx_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

/*hdmi_clk*/
static const struct clk_parent_data sc2_hdmi_parent_data[] = {
	{ .fw_name = "xtal", },
	{ .hw = &sc2_fclk_div4.hw },
	{ .hw = &sc2_fclk_div3.hw },
	{ .hw = &sc2_fclk_div5.hw }
};

static struct clk_regmap sc2_hdmi_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_HDMI_CLK_CTRL,
		.mask = 0x3,
		.shift = 9,
		.flags = CLK_MUX_ROUND_CLOSEST,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmi_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = sc2_hdmi_parent_data,
		.num_parents = ARRAY_SIZE(sc2_hdmi_parent_data),
		.flags = CLK_SET_RATE_NO_REPARENT | CLK_GET_RATE_NOCACHE |
			 CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap sc2_hdmi_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_HDMI_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmi_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) { &sc2_hdmi_sel.hw },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT |
			 CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap sc2_hdmi = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_HDMI_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmi",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &sc2_hdmi_div.hw },
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT |
			 CLK_IGNORE_UNUSED,
	},
};

/*vid_lock_clk*/

/*eth_clk :125M*/

/*ts_clk*/
static struct clk_regmap sc2_ts_clk_div = {
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

static struct clk_regmap sc2_ts_clk_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_TS_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "ts_clk_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_ts_clk_div.hw
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

static const struct clk_parent_data sc2_mali_0_1_parent_data[] = {
	{ .fw_name = "xtal", },
	{ .hw = &sc2_gp0_pll.hw },
	{ .hw = &sc2_fclk_div2p5.hw },
	{ .hw = &sc2_fclk_div3.hw },
	{ .hw = &sc2_fclk_div4.hw },
	{ .hw = &sc2_fclk_div5.hw },
	{ .hw = &sc2_fclk_div7.hw }
};

static struct clk_regmap sc2_mali_0_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_MALI_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
		.table = mux_table_mali,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mali_0_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = sc2_mali_0_1_parent_data,
		.num_parents = ARRAY_SIZE(sc2_mali_0_1_parent_data),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap sc2_mali_0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_MALI_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mali_0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_mali_0_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap sc2_mali_0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_MALI_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mali_0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_mali_0_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_GATE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap sc2_mali_1_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_MALI_CLK_CTRL,
		.mask = 0x7,
		.shift = 25,
		.table = mux_table_mali,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mali_1_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = sc2_mali_0_1_parent_data,
		.num_parents = ARRAY_SIZE(sc2_mali_0_1_parent_data),
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap sc2_mali_1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_MALI_CLK_CTRL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mali_1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_mali_1_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap sc2_mali_1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_MALI_CLK_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mali_1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_mali_1_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_GATE | CLK_SET_RATE_PARENT,
	},
};

static const struct clk_hw *sc2_mali_parent_hws[] = {
	&sc2_mali_0.hw,
	&sc2_mali_1.hw
};

static struct clk_regmap sc2_mali_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_MALI_CLK_CTRL,
		.mask = 1,
		.shift = 31,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mali",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = sc2_mali_parent_hws,
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/* cts_vdec_clk */
static const struct clk_parent_data sc2_dec_parent_hws[] = {
	{ .hw = &sc2_fclk_div2p5.hw },
	{ .hw = &sc2_fclk_div3.hw },
	{ .hw = &sc2_fclk_div4.hw },
	{ .hw = &sc2_fclk_div5.hw },
	{ .hw = &sc2_fclk_div7.hw },
	{ .hw = &sc2_hifi_pll.hw },
	{ .hw = &sc2_gp0_pll.hw },
	{ .fw_name = "xtal", }
};

static struct clk_regmap sc2_vdec_p0_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VDEC_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vdec_p0_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = sc2_dec_parent_hws,
		.num_parents = ARRAY_SIZE(sc2_dec_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap sc2_vdec_p0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_VDEC_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vdec_p0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_vdec_p0_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap sc2_vdec_p0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VDEC_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vdec_p0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_vdec_p0_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap sc2_vdec_p1_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VDEC3_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vdec_p1_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = sc2_dec_parent_hws,
		.num_parents = ARRAY_SIZE(sc2_dec_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap sc2_vdec_p1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_VDEC3_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vdec_p1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_vdec_p1_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap sc2_vdec_p1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VDEC3_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vdec_p1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_vdec_p1_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data sc2_vdec_mux_parent_hws[] = {
	{.hw = &sc2_vdec_p0.hw,},
	{.hw = &sc2_vdec_p1.hw,}
};

static struct clk_regmap sc2_vdec_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VDEC3_CLK_CTRL,
		.mask = 0x1,
		.shift = 15,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vdec_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = sc2_vdec_mux_parent_hws,
		.num_parents = ARRAY_SIZE(sc2_vdec_mux_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

/* cts_hcodec_clk */
static struct clk_regmap sc2_hcodec_p0_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VDEC_CLK_CTRL,
		.mask = 0x7,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hcodec_p0_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = sc2_dec_parent_hws,
		.num_parents = ARRAY_SIZE(sc2_dec_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap sc2_hcodec_p0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_VDEC_CLK_CTRL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hcodec_p0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_hcodec_p0_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap sc2_hcodec_p0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VDEC_CLK_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hcodec_p0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_hcodec_p0_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap sc2_hcodec_p1_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VDEC3_CLK_CTRL,
		.mask = 0x7,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hcodec_p1_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = sc2_dec_parent_hws,
		.num_parents = ARRAY_SIZE(sc2_dec_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap sc2_hcodec_p1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_VDEC3_CLK_CTRL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hcodec_p1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_hcodec_p1_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap sc2_hcodec_p1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VDEC3_CLK_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hcodec_p1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_hcodec_p1_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data sc2_hcodec_mux_parent_hws[] = {
	{ .hw = &sc2_hcodec_p0.hw },
	{ .hw = &sc2_hcodec_p1.hw }
};

static struct clk_regmap sc2_hcodec_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VDEC3_CLK_CTRL,
		.mask = 0x1,
		.shift = 31,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hcodec_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = sc2_hcodec_mux_parent_hws,
		.num_parents = ARRAY_SIZE(sc2_hcodec_mux_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

/* cts_hevcb_clk */
static u32 mux_table_vdec[] = { 0, 1, 2, 3, 4};

static const struct clk_hw *sc2_vdec_parent_hws[] = {
	&sc2_fclk_div2p5.hw,
	&sc2_fclk_div3.hw,
	&sc2_fclk_div4.hw,
	&sc2_fclk_div5.hw,
	&sc2_fclk_div7.hw
};

static struct clk_regmap sc2_hevcb_p0_mux = {
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
		.parent_hws = sc2_vdec_parent_hws,
		.num_parents = ARRAY_SIZE(sc2_vdec_parent_hws),
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap sc2_hevcb_p0_div = {
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
			&sc2_hevcb_p0_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap sc2_hevcb_p0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VDEC2_CLK_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hevcb_p0_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_hevcb_p0_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap sc2_hevcb_p1_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VDEC4_CLK_CTRL,
		.mask = 0x7,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hevcb_p1_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = sc2_dec_parent_hws,
		.num_parents = ARRAY_SIZE(sc2_dec_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap sc2_hevcb_p1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_VDEC4_CLK_CTRL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hevc_p1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_hevcb_p1_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap sc2_hevcb_p1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VDEC4_CLK_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hevcb_p1_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_hevcb_p1_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data sc2_hevcb_mux_parent_hws[] = {
	{ .hw = &sc2_hevcb_p0.hw },
	{ .hw = &sc2_hevcb_p1.hw }
};

static struct clk_regmap sc2_hevcb_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VDEC4_CLK_CTRL,
		.mask = 0x1,
		.shift = 31,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hevcb_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = sc2_hevcb_mux_parent_hws,
		.num_parents = ARRAY_SIZE(sc2_hevcb_mux_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

/* cts_hevcf_clk */
static struct clk_regmap sc2_hevcf_p0_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VDEC2_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hevcf_p0_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = sc2_dec_parent_hws,
		.num_parents = ARRAY_SIZE(sc2_dec_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap sc2_hevcf_p0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_VDEC2_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hevcf_p0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_hevcf_p0_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap sc2_hevcf_p0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VDEC2_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hevcf_p0_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_hevcf_p0_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap sc2_hevcf_p1_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VDEC4_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hevcf_p1_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = sc2_dec_parent_hws,
		.num_parents = ARRAY_SIZE(sc2_dec_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap sc2_hevcf_p1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_VDEC4_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hevcf_p1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_hevcf_p1_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap sc2_hevcf_p1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VDEC4_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hevcf_p1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_hevcf_p1_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data sc2_hevcf_mux_parent_hws[] = {
	{ .hw = &sc2_hevcf_p0.hw },
	{ .hw = &sc2_hevcf_p1.hw }
};

static struct clk_regmap sc2_hevcf_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VDEC4_CLK_CTRL,
		.mask = 0x1,
		.shift = 15,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hevcf_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = sc2_hevcf_mux_parent_hws,
		.num_parents = ARRAY_SIZE(sc2_hevcf_mux_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

/*cts_wave420l_a/b/c_clk*/
static const struct clk_parent_data sc2_wave_parent_data[] = {
	{ .fw_name = "xtal", },
	{ .hw = &sc2_fclk_div4.hw },
	{ .hw = &sc2_fclk_div3.hw },
	{ .hw = &sc2_fclk_div5.hw },
	{ .hw = &sc2_fclk_div7.hw },
	{ .hw = &sc2_mpll2.hw },
	{ .hw = &sc2_mpll3.hw },
	{ .hw = &sc2_gp0_pll.hw }
};

static struct clk_regmap sc2_wave_a_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_WAVE420L_CLK_CTRL2,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "wave_a_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = sc2_wave_parent_data,
		.num_parents = ARRAY_SIZE(sc2_wave_parent_data),
	},
};

static struct clk_regmap sc2_wave_a_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_WAVE420L_CLK_CTRL2,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "wave_a_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_wave_a_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap sc2_wave_aclk = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_WAVE420L_CLK_CTRL2,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "wave_aclk",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_wave_a_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap sc2_wave_b_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_WAVE420L_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "wave_b_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = sc2_wave_parent_data,
		.num_parents = ARRAY_SIZE(sc2_wave_parent_data),
	},
};

static struct clk_regmap sc2_wave_b_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_WAVE420L_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "wave_b_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_wave_b_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap sc2_wave_bclk = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_WAVE420L_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "wave_bclk",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_wave_b_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap sc2_wave_c_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_WAVE420L_CLK_CTRL,
		.mask = 0x7,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data){
		.name = "wave_c_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = sc2_wave_parent_data,
		.num_parents = ARRAY_SIZE(sc2_wave_parent_data),
	},
};

static struct clk_regmap sc2_wave_c_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_WAVE420L_CLK_CTRL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "wave_c_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_wave_c_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap sc2_wave_cclk = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_WAVE420L_CLK_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "wave_cclk",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_wave_c_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/* cts_vpu_clk */
static const struct clk_hw *sc2_vpu_parent_hws[] = {
	&sc2_fclk_div3.hw,
	&sc2_fclk_div4.hw,
	&sc2_fclk_div5.hw,
	&sc2_fclk_div7.hw,
	&sc2_mpll1.hw,
	&sc2_vid_pll.hw,
	&sc2_hifi_pll.hw,
	&sc2_gp0_pll.hw
};

static struct clk_regmap sc2_vpu_0_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VPU_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vpu_0_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = sc2_vpu_parent_hws,
		.num_parents = ARRAY_SIZE(sc2_vpu_parent_hws),
	},
};

static struct clk_regmap sc2_vpu_0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_VPU_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vpu_0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) { &sc2_vpu_0_sel.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap sc2_vpu_0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VPU_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &sc2_vpu_0_div.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap sc2_vpu_1_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VPU_CLK_CTRL,
		.mask = 0x7,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vpu_1_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = sc2_vpu_parent_hws,
		.num_parents = ARRAY_SIZE(sc2_vpu_parent_hws),
	},
};

static struct clk_regmap sc2_vpu_1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_VPU_CLK_CTRL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vpu_1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) { &sc2_vpu_1_sel.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap sc2_vpu_1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VPU_CLK_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &sc2_vpu_1_div.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap sc2_vpu = {
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
			&sc2_vpu_0.hw,
			&sc2_vpu_1.hw,
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_NO_REPARENT,
	},
};

/*cts_vpu_clkb*/
static const struct clk_parent_data vpu_clkb_tmp_parent_hws[] = {
	{ .hw = &sc2_vpu.hw },
	{ .hw = &sc2_fclk_div4.hw },
	{ .hw = &sc2_fclk_div5.hw },
	{ .hw = &sc2_fclk_div7.hw }
};

static struct clk_regmap sc2_vpu_clkb_tmp_mux = {
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

static struct clk_regmap sc2_vpu_clkb_tmp_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_VPU_CLKB_CTRL,
		.shift = 16,
		.width = 4,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_clkb_tmp_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_vpu_clkb_tmp_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap sc2_vpu_clkb_tmp = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VPU_CLKB_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vpu_clkb_tmp",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_vpu_clkb_tmp_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap sc2_vpu_clkb_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_VPU_CLKB_CTRL,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_clkb_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_vpu_clkb_tmp.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap sc2_vpu_clkb = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VPU_CLKB_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vpu_clkb",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_vpu_clkb_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

/* cts_vpu_clkc */
static const char * const vpu_clkc_parent_names[] = { "fclk_div4",
	"fclk_div3", "fclk_div5", "fclk_div7", "mpll1", "vid_pll",
	"mpll2",  "gp0_pll"};

static struct clk_regmap sc2_vpu_clkc_p0_mux  = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VPU_CLKC_CTRL,
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

static struct clk_regmap sc2_vpu_clkc_p0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_VPU_CLKC_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_clkc_p0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_vpu_clkc_p0_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap sc2_vpu_clkc_p0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VPU_CLKC_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vpu_clkc_p0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_vpu_clkc_p0_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap sc2_vpu_clkc_p1_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VPU_CLKC_CTRL,
		.mask = 0x7,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_clkc_p1_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_names = vpu_clkc_parent_names,
		.num_parents = ARRAY_SIZE(vpu_clkc_parent_names),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap sc2_vpu_clkc_p1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_VPU_CLKC_CTRL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_clkc_p1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_vpu_clkc_p1_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap sc2_vpu_clkc_p1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VPU_CLKC_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vpu_clkc_p1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_vpu_clkc_p1_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data sc2_vpu_mux_parent_hws[] = {
	{ .hw = &sc2_vpu_clkc_p0.hw },
	{ .hw = &sc2_vpu_clkc_p1.hw }
};

static struct clk_regmap sc2_vpu_clkc_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VPU_CLKC_CTRL,
		.mask = 0x1,
		.shift = 31,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_clkc_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = sc2_vpu_mux_parent_hws,
		.num_parents = ARRAY_SIZE(sc2_vpu_mux_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

/*cts_vapb_clk*/
static const struct clk_hw *sc2_vapb_parent_hws[] = {
	&sc2_fclk_div4.hw,
	&sc2_fclk_div3.hw,
	&sc2_fclk_div5.hw,
	&sc2_fclk_div7.hw,
	&sc2_mpll1.hw,
	&sc2_vid_pll.hw,
	&sc2_mpll2.hw,
	&sc2_fclk_div2p5.hw
};

static struct clk_regmap sc2_vapb_0_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VAPBCLK_CTRL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vapb_0_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = sc2_vapb_parent_hws,
		.num_parents = ARRAY_SIZE(sc2_vapb_parent_hws),
	},
};

static struct clk_regmap sc2_vapb_0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_VAPBCLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vapb_0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_vapb_0_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap sc2_vapb_0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VAPBCLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vapb_0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_vapb_0_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap sc2_vapb_1_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VAPBCLK_CTRL,
		.mask = 0x7,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vapb_1_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = sc2_vapb_parent_hws,
		.num_parents = ARRAY_SIZE(sc2_vapb_parent_hws),
		.flags = CLK_SET_RATE_NO_REPARENT,
	},
};

static struct clk_regmap sc2_vapb_1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_VAPBCLK_CTRL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vapb_1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_vapb_1_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap sc2_vapb_1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VAPBCLK_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vapb_1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_vapb_1_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap sc2_vapb = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VAPBCLK_CTRL,
		.mask = 1,
		.shift = 31,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vapb_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_vapb_0.hw,
			&sc2_vapb_1.hw,
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_NO_REPARENT | CLK_SET_RATE_PARENT,
	},
};

/*cts_ge2d_clk*/
static struct clk_regmap sc2_ge2d_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VAPBCLK_CTRL,
		.bit_idx = 30,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "ge2d_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &sc2_vapb.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/*cts_hdcp22_esmclk*/

/*cts_hdcp22_skpclk*/

/* cts_vdin_meas_clk */
static const struct clk_parent_data sc2_vdin_parent_hws[] = {
	{ .fw_name = "xtal", },
	{ .hw = &sc2_fclk_div4.hw },
	{ .hw = &sc2_fclk_div3.hw },
	{ .hw = &sc2_fclk_div5.hw },
	{ .hw = &sc2_vid_pll.hw }
};

static struct clk_regmap sc2_vdin_meas_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VDIN_MEAS_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vdin_meas_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = sc2_vdin_parent_hws,
		.num_parents = ARRAY_SIZE(sc2_vdin_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap sc2_vdin_meas_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_VDIN_MEAS_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vdin_meas_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_vdin_meas_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap sc2_vdin_meas_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VDIN_MEAS_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vdin_meas_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_vdin_meas_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

/*cts_nand_clk*/
static const struct clk_parent_data sc2_sd_emmc_clk0_parent_data[] = {
	{ .fw_name = "xtal", },
	{ .hw = &sc2_fclk_div2.hw },
	{ .hw = &sc2_fclk_div3.hw },
	{ .hw = &sc2_hifi_pll.hw },
	{ .hw = &sc2_fclk_div2p5.hw },
	{ .hw = &sc2_mpll2.hw },
	{ .hw = &sc2_mpll3.hw },
	{ .hw = &sc2_gp0_pll.hw }
};

static struct clk_regmap sc2_sd_emmc_c_clk0_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_NAND_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_c_clk0_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = sc2_sd_emmc_clk0_parent_data,
		.num_parents = ARRAY_SIZE(sc2_sd_emmc_clk0_parent_data),
		.flags = CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED |
			CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap sc2_sd_emmc_c_clk0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_NAND_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_c_clk0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_sd_emmc_c_clk0_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED |
			CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap sc2_sd_emmc_c_clk0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_NAND_CLK_CTRL,
		.bit_idx = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sd_emmc_c_clk0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_sd_emmc_c_clk0_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED |
			CLK_SET_RATE_PARENT,
	},
};

/*cts_sd_emmc_a/b_clk*/
static struct clk_regmap sc2_sd_emmc_a_clk0_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_SD_EMMC_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_a_clk0_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = sc2_sd_emmc_clk0_parent_data,
		.num_parents = ARRAY_SIZE(sc2_sd_emmc_clk0_parent_data),
		.flags = CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED |
			CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap sc2_sd_emmc_a_clk0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_SD_EMMC_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_a_clk0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_sd_emmc_a_clk0_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED |
			CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap sc2_sd_emmc_a_clk0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_SD_EMMC_CLK_CTRL,
		.bit_idx = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sd_emmc_a_clk0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_sd_emmc_a_clk0_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED |
			CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap sc2_sd_emmc_b_clk0_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_SD_EMMC_CLK_CTRL,
		.mask = 0x7,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_b_clk0_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = sc2_sd_emmc_clk0_parent_data,
		.num_parents = ARRAY_SIZE(sc2_sd_emmc_clk0_parent_data),
		.flags = CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED |
			CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap sc2_sd_emmc_b_clk0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_SD_EMMC_CLK_CTRL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_b_clk0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_sd_emmc_b_clk0_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED |
			CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap sc2_sd_emmc_b_clk0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_SD_EMMC_CLK_CTRL,
		.bit_idx = 23,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sd_emmc_b_clk0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_sd_emmc_b_clk0_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED |
			CLK_SET_RATE_PARENT,
	},
};

/*cts_cdac_clk*/

/*cts_spicc_0/1_clk*/
static const struct clk_parent_data sc2_spicc_parent_hws[] = {
	{ .fw_name = "xtal", },
	{ .hw = &sc2_sys_clk.hw },
	{ .hw = &sc2_fclk_div4.hw },
	{ .hw = &sc2_fclk_div3.hw },
	{ .hw = &sc2_fclk_div2.hw },
	{ .hw = &sc2_fclk_div5.hw },
	{ .hw = &sc2_fclk_div7.hw },
	{ .hw = &sc2_gp0_pll.hw }
};

static struct clk_regmap sc2_spicc0_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_SPICC_CLK_CTRL,
		.mask = 0x7,
		.shift = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spicc0_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = sc2_spicc_parent_hws,
		.num_parents = ARRAY_SIZE(sc2_spicc_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap sc2_spicc0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_SPICC_CLK_CTRL,
		.shift = 0,
		.width = 6,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spicc0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_spicc0_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap sc2_spicc0_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_SPICC_CLK_CTRL,
		.bit_idx = 6,
	},
	.hw.init = &(struct clk_init_data){
		.name = "spicc0_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_spicc0_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap sc2_spicc1_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_SPICC_CLK_CTRL,
		.mask = 0x7,
		.shift = 23,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spicc1_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = sc2_spicc_parent_hws,
		.num_parents = ARRAY_SIZE(sc2_spicc_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap sc2_spicc1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_SPICC_CLK_CTRL,
		.shift = 16,
		.width = 6,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spicc1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_spicc1_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap sc2_spicc1_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_SPICC_CLK_CTRL,
		.bit_idx = 22,
	},
	.hw.init = &(struct clk_init_data){
		.name = "spicc1_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_spicc1_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

/*cts_bt656*/

/*cts_pwm_*_clk*/
static const struct clk_parent_data sc2_pwm_parent_data[] = {
	{ .fw_name = "xtal", },
	{ .hw = &sc2_vid_pll.hw },
	{ .hw = &sc2_fclk_div4.hw },
	{ .hw = &sc2_fclk_div3.hw }
};

static struct clk_regmap sc2_pwm_a_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_AB_CTRL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_a_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = sc2_pwm_parent_data,
		.num_parents = ARRAY_SIZE(sc2_pwm_parent_data),
		.flags = CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap sc2_pwm_a_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_AB_CTRL,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_a_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_pwm_a_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap sc2_pwm_a_gate = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_AB_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_a_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_pwm_a_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap sc2_pwm_b_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_AB_CTRL,
		.mask = 0x3,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_b_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = sc2_pwm_parent_data,
		.num_parents = ARRAY_SIZE(sc2_pwm_parent_data),
		.flags = CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap sc2_pwm_b_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_AB_CTRL,
		.shift = 16,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_b_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_pwm_b_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap sc2_pwm_b_gate = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_AB_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_b_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_pwm_b_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap sc2_pwm_c_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_CD_CTRL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_c_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = sc2_pwm_parent_data,
		.num_parents = ARRAY_SIZE(sc2_pwm_parent_data),
		.flags = CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap sc2_pwm_c_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_CD_CTRL,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_c_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_pwm_c_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap sc2_pwm_c_gate = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_CD_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_c_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_pwm_c_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap sc2_pwm_d_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_CD_CTRL,
		.mask = 0x3,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_d_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = sc2_pwm_parent_data,
		.num_parents = ARRAY_SIZE(sc2_pwm_parent_data),
		.flags = CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap sc2_pwm_d_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_CD_CTRL,
		.shift = 16,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_d_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_pwm_d_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap sc2_pwm_d_gate = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_CD_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_d_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_pwm_d_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap sc2_pwm_e_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_EF_CTRL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_e_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = sc2_pwm_parent_data,
		.num_parents = ARRAY_SIZE(sc2_pwm_parent_data),
		.flags = CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap sc2_pwm_e_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_EF_CTRL,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_e_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_pwm_e_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap sc2_pwm_e_gate = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_EF_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_e_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_pwm_e_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap sc2_pwm_f_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_EF_CTRL,
		.mask = 0x3,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_f_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = sc2_pwm_parent_data,
		.num_parents = ARRAY_SIZE(sc2_pwm_parent_data),
		.flags = CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap sc2_pwm_f_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_EF_CTRL,
		.shift = 16,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_f_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_pwm_f_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap sc2_pwm_f_gate = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_EF_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_f_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_pwm_f_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap sc2_pwm_g_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_GH_CTRL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_g_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = sc2_pwm_parent_data,
		.num_parents = ARRAY_SIZE(sc2_pwm_parent_data),
		.flags = CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap sc2_pwm_g_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_GH_CTRL,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_g_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_pwm_g_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap sc2_pwm_g_gate = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_GH_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_g_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_pwm_g_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap sc2_pwm_h_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_GH_CTRL,
		.mask = 0x3,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_h_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = sc2_pwm_parent_data,
		.num_parents = ARRAY_SIZE(sc2_pwm_parent_data),
		.flags = CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap sc2_pwm_h_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_GH_CTRL,
		.shift = 16,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_h_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_pwm_h_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap sc2_pwm_h_gate = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_GH_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_h_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_pwm_h_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap sc2_pwm_i_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_IJ_CTRL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_i_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = sc2_pwm_parent_data,
		.num_parents = ARRAY_SIZE(sc2_pwm_parent_data),
		.flags = CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap sc2_pwm_i_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_IJ_CTRL,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_i_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_pwm_i_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap sc2_pwm_i_gate = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_IJ_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_i_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_pwm_i_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap sc2_pwm_j_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_IJ_CTRL,
		.mask = 0x3,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_j_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = sc2_pwm_parent_data,
		.num_parents = ARRAY_SIZE(sc2_pwm_parent_data),
		.flags = CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap sc2_pwm_j_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_IJ_CTRL,
		.shift = 16,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_j_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_pwm_h_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap sc2_pwm_j_gate = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_IJ_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_j_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_pwm_j_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

/*cts_sar_adc_clk*/
static struct clk_regmap sc2_saradc_mux = {
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
			{ .hw = &sc2_sys_clk.hw },
		},
		.num_parents = 2,
	},
};

static struct clk_regmap sc2_saradc_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_SAR_CLK_CTRL,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "saradc_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_saradc_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap sc2_saradc_gate = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_SAR_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "saradc_clk",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_saradc_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

/* gen clk */
static u32 sc2_gen_clk_mux_table[] = { 0, 5, 6, 7, 19, 21, 22,
				23, 24, 25, 26, 27, 28 };

static const char * const sc2_gen_clk_parent_names[] = {
	"xtal", "gp0_pll", "gp1_pll", "hifi_pll", "fclk_div2", "fclk_div3",
	"fclk_div4", "fclk_div5", "fclk_div7", "mpll0", "mpll1",
	"mpll2", "mpll3"
};

static struct clk_regmap sc2_gen_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_GEN_CLK_CTRL,
		.mask = 0x1f,
		.shift = 12,
		.table = sc2_gen_clk_mux_table,
	},
	.hw.init = &(struct clk_init_data){
		.name = "gen_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_names = sc2_gen_clk_parent_names,
		.num_parents = ARRAY_SIZE(sc2_gen_clk_parent_names),
	},
};

static struct clk_regmap sc2_gen_div = {
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

static struct clk_regmap sc2_gen = {
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
			&sc2_sys_clk.hw					\
		},							\
		.num_parents = 1,					\
		.flags = CLK_IGNORE_UNUSED,				\
	},								\
}

/*CLKCTRL_SYS_CLK_EN0_REG0*/
static MESON_SC2_SYS_GATE(sc2_ddr, CLKCTRL_SYS_CLK_EN0_REG0, 0);
static MESON_SC2_SYS_GATE(sc2_dos, CLKCTRL_SYS_CLK_EN0_REG0, 1);
static MESON_SC2_SYS_GATE(sc2_ethphy, CLKCTRL_SYS_CLK_EN0_REG0, 4);
static MESON_SC2_SYS_GATE(sc2_mali, CLKCTRL_SYS_CLK_EN0_REG0, 6);
static MESON_SC2_SYS_GATE(sc2_aocpu, CLKCTRL_SYS_CLK_EN0_REG0, 13);
static MESON_SC2_SYS_GATE(sc2_aucpu, CLKCTRL_SYS_CLK_EN0_REG0, 14);
static MESON_SC2_SYS_GATE(sc2_cec, CLKCTRL_SYS_CLK_EN0_REG0, 16);
static MESON_SC2_SYS_GATE(sc2_sdemmca, CLKCTRL_SYS_CLK_EN0_REG0, 24);
static MESON_SC2_SYS_GATE(sc2_sdemmcb, CLKCTRL_SYS_CLK_EN0_REG0, 25);
static MESON_SC2_SYS_GATE(sc2_nand, CLKCTRL_SYS_CLK_EN0_REG0, 26);
static MESON_SC2_SYS_GATE(sc2_smartcard, CLKCTRL_SYS_CLK_EN0_REG0, 27);
static MESON_SC2_SYS_GATE(sc2_acodec, CLKCTRL_SYS_CLK_EN0_REG0, 28);
static MESON_SC2_SYS_GATE(sc2_spifc, CLKCTRL_SYS_CLK_EN0_REG0, 29);
static MESON_SC2_SYS_GATE(sc2_msr_clk, CLKCTRL_SYS_CLK_EN0_REG0, 30);
static MESON_SC2_SYS_GATE(sc2_ir_ctrl, CLKCTRL_SYS_CLK_EN0_REG0, 31);

/*CLKCTRL_SYS_CLK_EN0_REG1*/
static MESON_SC2_SYS_GATE(sc2_audio, CLKCTRL_SYS_CLK_EN0_REG1, 0);
static MESON_SC2_SYS_GATE(sc2_eth, CLKCTRL_SYS_CLK_EN0_REG1, 3);
static MESON_SC2_SYS_GATE(sc2_uart_a, CLKCTRL_SYS_CLK_EN0_REG1, 5);
static MESON_SC2_SYS_GATE(sc2_uart_b, CLKCTRL_SYS_CLK_EN0_REG1, 6);
static MESON_SC2_SYS_GATE(sc2_uart_c, CLKCTRL_SYS_CLK_EN0_REG1, 7);
static MESON_SC2_SYS_GATE(sc2_uart_d, CLKCTRL_SYS_CLK_EN0_REG1, 8);
static MESON_SC2_SYS_GATE(sc2_uart_e, CLKCTRL_SYS_CLK_EN0_REG1, 9);
static MESON_SC2_SYS_GATE(sc2_aififo, CLKCTRL_SYS_CLK_EN0_REG1, 11);
static MESON_SC2_SYS_GATE(sc2_ts_ddr, CLKCTRL_SYS_CLK_EN0_REG1, 15);
static MESON_SC2_SYS_GATE(sc2_ts_pll, CLKCTRL_SYS_CLK_EN0_REG1, 16);
static MESON_SC2_SYS_GATE(sc2_g2d, CLKCTRL_SYS_CLK_EN0_REG1, 20);
static MESON_SC2_SYS_GATE(sc2_spicc0, CLKCTRL_SYS_CLK_EN0_REG1, 21);
static MESON_SC2_SYS_GATE(sc2_spicc1, CLKCTRL_SYS_CLK_EN0_REG1, 22);
static MESON_SC2_SYS_GATE(sc2_pcie, CLKCTRL_SYS_CLK_EN0_REG1, 24);
static MESON_SC2_SYS_GATE(sc2_usb, CLKCTRL_SYS_CLK_EN0_REG1, 26);
static MESON_SC2_SYS_GATE(sc2_pcie_phy, CLKCTRL_SYS_CLK_EN0_REG1, 27);
static MESON_SC2_SYS_GATE(sc2_i2c_m_a, CLKCTRL_SYS_CLK_EN0_REG1, 30);
static MESON_SC2_SYS_GATE(sc2_i2c_m_b, CLKCTRL_SYS_CLK_EN0_REG1, 31);

/*CLKCTRL_SYS_CLK_EN0_REG2*/
static MESON_SC2_SYS_GATE(sc2_i2c_m_c, CLKCTRL_SYS_CLK_EN0_REG2, 0);
static MESON_SC2_SYS_GATE(sc2_i2c_m_d, CLKCTRL_SYS_CLK_EN0_REG2, 1);
static MESON_SC2_SYS_GATE(sc2_i2c_m_e, CLKCTRL_SYS_CLK_EN0_REG2, 2);
static MESON_SC2_SYS_GATE(sc2_i2c_m_f, CLKCTRL_SYS_CLK_EN0_REG2, 3);
static MESON_SC2_SYS_GATE(sc2_hdmitx_apb, CLKCTRL_SYS_CLK_EN0_REG2, 4);
static MESON_SC2_SYS_GATE(sc2_i2c_s_a, CLKCTRL_SYS_CLK_EN0_REG2, 5);
static MESON_SC2_SYS_GATE(sc2_usb1_to_ddr, CLKCTRL_SYS_CLK_EN0_REG2, 8);
static MESON_SC2_SYS_GATE(sc2_hdcp22, CLKCTRL_SYS_CLK_EN0_REG2, 10);
static MESON_SC2_SYS_GATE(sc2_mmc_apb, CLKCTRL_SYS_CLK_EN0_REG2, 11);
static MESON_SC2_SYS_GATE(sc2_rsa, CLKCTRL_SYS_CLK_EN0_REG2, 18);
static MESON_SC2_SYS_GATE(sc2_cpu_debug, CLKCTRL_SYS_CLK_EN0_REG2, 19);
static MESON_SC2_SYS_GATE(sc2_dspa, CLKCTRL_SYS_CLK_EN0_REG2, 21);
static MESON_SC2_SYS_GATE(sc2_vpu_intr, CLKCTRL_SYS_CLK_EN0_REG2, 25);
static MESON_SC2_SYS_GATE(sc2_sar_adc, CLKCTRL_SYS_CLK_EN0_REG2, 28);
static MESON_SC2_SYS_GATE(sc2_gic, CLKCTRL_SYS_CLK_EN0_REG2, 30);

/*CLKCTRL_SYS_CLK_EN0_REG3*/
static MESON_SC2_SYS_GATE(sc2_pwm_ab, CLKCTRL_SYS_CLK_EN0_REG3, 7);
static MESON_SC2_SYS_GATE(sc2_pwm_cd, CLKCTRL_SYS_CLK_EN0_REG3, 8);
static MESON_SC2_SYS_GATE(sc2_pwm_ef, CLKCTRL_SYS_CLK_EN0_REG3, 9);
static MESON_SC2_SYS_GATE(sc2_pwm_gh, CLKCTRL_SYS_CLK_EN0_REG3, 10);
static MESON_SC2_SYS_GATE(sc2_pwm_ij, CLKCTRL_SYS_CLK_EN0_REG3, 11);

/* Array of all clocks provided by this provider */
static struct clk_hw_onecell_data sc2_hw_onecell_data = {
	.hws = {
		[CLKID_FIXED_PLL_DCO]		= &sc2_fixed_pll_dco.hw,
		[CLKID_FIXED_PLL]		= &sc2_fixed_pll.hw,
		[CLKID_SYS_PLL_DCO]		= &sc2_sys_pll_dco.hw,
		[CLKID_SYS_PLL]			= &sc2_sys_pll.hw,
		[CLKID_FCLK_DIV2_DIV]		= &sc2_fclk_div2_div.hw,
		[CLKID_FCLK_DIV2]		= &sc2_fclk_div2.hw,
		[CLKID_FCLK_DIV3_DIV]		= &sc2_fclk_div3_div.hw,
		[CLKID_FCLK_DIV3]		= &sc2_fclk_div3.hw,
		[CLKID_FCLK_DIV4_DIV]		= &sc2_fclk_div4_div.hw,
		[CLKID_FCLK_DIV4]		= &sc2_fclk_div4.hw,
		[CLKID_FCLK_DIV5_DIV]		= &sc2_fclk_div5_div.hw,
		[CLKID_FCLK_DIV5]		= &sc2_fclk_div5.hw,
		[CLKID_FCLK_DIV7_DIV]		= &sc2_fclk_div7_div.hw,
		[CLKID_FCLK_DIV7]		= &sc2_fclk_div7.hw,
		[CLKID_FCLK_DIV2P5_DIV]		= &sc2_fclk_div2p5_div.hw,
		[CLKID_FCLK_DIV2P5]		= &sc2_fclk_div2p5.hw,

		[CLKID_GP0_PLL_DCO]		= &sc2_gp0_pll_dco.hw,
		[CLKID_GP0_PLL]			= &sc2_gp0_pll.hw,
		[CLKID_GP1_PLL_DCO]		= &sc2_gp1_pll_dco.hw,
		[CLKID_GP1_PLL]			= &sc2_gp1_pll.hw,

		[CLKID_CPU_CLK_DYN0_SEL]	= &sc2_cpu_clk_premux0.hw,
		[CLKID_CPU_CLK_DYN0_DIV]	= &sc2_cpu_clk_mux0_div.hw,
		[CLKID_CPU_CLK_DYN0]		= &sc2_cpu_clk_postmux0.hw,
		[CLKID_CPU_CLK_DYN1_SEL]	= &sc2_cpu_clk_premux1.hw,
		[CLKID_CPU_CLK_DYN1_DIV]	= &sc2_cpu_clk_mux1_div.hw,
		[CLKID_CPU_CLK_DYN1]		= &sc2_cpu_clk_postmux1.hw,
		[CLKID_CPU_CLK_DYN]		= &sc2_cpu_clk_dyn.hw,
		[CLKID_CPU_CLK]			= &sc2_cpu_clk.hw,
		[CLKID_DSU_CLK_DYN0_SEL]	= &sc2_dsu_clk_premux0.hw,
		[CLKID_DSU_CLK_DYN0_DIV]	= &sc2_dsu_clk_mux0_div.hw,
		[CLKID_DSU_CLK_DYN0]		= &sc2_dsu_clk_postmux0.hw,
		[CLKID_DSU_CLK_DYN1_SEL]	= &sc2_dsu_clk_premux1.hw,
		[CLKID_DSU_CLK_DYN1_DIV]	= &sc2_dsu_clk_mux1_div.hw,
		[CLKID_DSU_CLK_DYN1]		= &sc2_dsu_clk_postmux1.hw,
		[CLKID_DSU_CLK_DYN]		= &sc2_dsu_clk_dyn.hw,
		[CLKID_DSU_CLK_FINAL]		= &sc2_dsu_final_clk.hw,
		[CLKID_DSU_CLK]			= &sc2_dsu_clk.hw,
		[CLKID_CPU1_CLK]		= &sc2_cpu1_clk.hw,
		[CLKID_CPU2_CLK]		= &sc2_cpu2_clk.hw,
		[CLKID_CPU3_CLK]		= &sc2_cpu3_clk.hw,

		[CLKID_HIFI_PLL_DCO]		= &sc2_hifi_pll_dco.hw,
		[CLKID_HIFI_PLL]		= &sc2_hifi_pll.hw,
		[CLKID_PCIE_PLL_DCO]		= &sc2_pcie_pll_dco.hw,
		[CLKID_PCIE_PLL_DCO_DIV2]	= &sc2_pcie_pll_dco_div2.hw,
		[CLKID_PCIE_PLL_OD]		= &sc2_pcie_pll_od.hw,
		[CLKID_PCIE_PLL]		= &sc2_pcie_pll.hw,
		[CLKID_PCIE_HCSL]		= &sc2_pcie_hcsl.hw,
		[CLKID_HDMI_PLL_DCO]		= &sc2_hdmi_pll_dco.hw,
		[CLKID_HDMI_PLL_OD]		= &sc2_hdmi_pll_od.hw,
		[CLKID_HDMI_PLL]		= &sc2_hdmi_pll.hw,
		[CLKID_MPLL_50M_DIV]		= &sc2_mpll_50m_div.hw,
		[CLKID_MPLL_50M]		= &sc2_mpll_50m.hw,
		[CLKID_MPLL_PREDIV]		= &sc2_mpll_prediv.hw,
		[CLKID_MPLL0_DIV]		= &sc2_mpll0_div.hw,
		[CLKID_MPLL0]			= &sc2_mpll0.hw,
		[CLKID_MPLL1_DIV]		= &sc2_mpll1_div.hw,
		[CLKID_MPLL1]			= &sc2_mpll1.hw,
		[CLKID_MPLL2_DIV]		= &sc2_mpll2_div.hw,
		[CLKID_MPLL2]			= &sc2_mpll2.hw,
		[CLKID_MPLL3_DIV]		= &sc2_mpll3_div.hw,
		[CLKID_MPLL3]			= &sc2_mpll3.hw,

		[CLKID_RTC_32K_CLKIN]		= &sc2_rtc_32k_clkin.hw,
		[CLKID_RTC_32K_DIV]		= &sc2_rtc_32k_div.hw,
		[CLKID_RTC_32K_XATL]		= &sc2_rtc_32k_xtal.hw,
		[CLKID_RTC_32K_MUX]		= &sc2_rtc_32k_sel.hw,
		[CLKID_RTC_CLK]			= &sc2_rtc_clk.hw,

		[CLKID_SYS_CLK_B_MUX]		= &sc2_sysclk_b_sel.hw,
		[CLKID_SYS_CLK_B_DIV]		= &sc2_sysclk_b_div.hw,
		[CLKID_SYS_CLK_B_GATE]		= &sc2_sysclk_b.hw,
		[CLKID_SYS_CLK_A_MUX]		= &sc2_sysclk_a_sel.hw,
		[CLKID_SYS_CLK_A_DIV]		= &sc2_sysclk_a_div.hw,
		[CLKID_SYS_CLK_A_GATE]		= &sc2_sysclk_a.hw,
		[CLKID_SYS_CLK]			= &sc2_sys_clk.hw,

		[CLKID_CECA_32K_CLKIN]		= &sc2_ceca_32k_clkin.hw,
		[CLKID_CECA_32K_DIV]		= &sc2_ceca_32k_div.hw,
		[CLKID_CECA_32K_MUX_PRE]	= &sc2_ceca_32k_sel_pre.hw,
		[CLKID_CECA_32K_MUX]		= &sc2_ceca_32k_sel.hw,
		[CLKID_CECA_32K_CLKOUT]		= &sc2_ceca_32k_clkout.hw,

		[CLKID_CECB_32K_CLKIN]		= &sc2_cecb_32k_clkin.hw,
		[CLKID_CECB_32K_DIV]		= &sc2_cecb_32k_div.hw,
		[CLKID_CECB_32K_MUX_PRE]	= &sc2_cecb_32k_sel_pre.hw,
		[CLKID_CECB_32K_MUX]		= &sc2_cecb_32k_sel.hw,
		[CLKID_CECB_32K_CLKOUT]		= &sc2_cecb_32k_clkout.hw,

		[CLKID_SC_CLK_MUX]		= &sc2_sc_clk_mux.hw,
		[CLKID_SC_CLK_DIV]		= &sc2_sc_clk_div.hw,
		[CLKID_SC_CLK_GATE]		= &sc2_sc_clk_gate.hw,

		[CLKID_DSPA_CLK_B_MUX]		= &sc2_dspa_b_mux.hw,
		[CLKID_DSPA_CLK_B_DIV]		= &sc2_dspa_b_div.hw,
		[CLKID_DSPA_CLK_B_GATE]		= &sc2_dspa_b_gate.hw,
		[CLKID_DSPA_CLK_A_MUX]		= &sc2_dspa_a_mux.hw,
		[CLKID_DSPA_CLK_A_DIV]		= &sc2_dspa_a_div.hw,
		[CLKID_DSPA_CLK_A_GATE]		= &sc2_dspa_a_gate.hw,
		[CLKID_DSPA_CLK]		= &sc2_dspa_mux.hw,

		[CLKID_24M_CLK_GATE]		= &sc2_24M_clk_gate.hw,
		[CLKID_12M_CLK_DIV]		= &sc2_12M_clk_div.hw,
		[CLKID_12M_CLK_GATE]		= &sc2_12M_clk_gate.hw,
		[CLKID_25M_CLK_DIV]		= &sc2_25M_clk_div.hw,
		[CLKID_25M_CLK_GATE]		= &sc2_25M_clk_gate.hw,

		[CLKID_VID_PLL]			= &sc2_vid_pll_div.hw,
		[CLKID_VID_PLL_MUX]		= &sc2_vid_pll_sel.hw,
		[CLKID_VID_PLL]			= &sc2_vid_pll.hw,
		[CLKID_VCLK_MUX]		= &sc2_vclk_sel.hw,
		[CLKID_VCLK2_MUX]		= &sc2_vclk2_sel.hw,
		[CLKID_VCLK_INPUT]		= &sc2_vclk_input.hw,
		[CLKID_VCLK2_INPUT]		= &sc2_vclk2_input.hw,
		[CLKID_VCLK_DIV]		= &sc2_vclk_div.hw,
		[CLKID_VCLK2_DIV]		= &sc2_vclk2_div.hw,
		[CLKID_VCLK]			= &sc2_vclk.hw,
		[CLKID_VCLK2]			= &sc2_vclk2.hw,
		[CLKID_VCLK_DIV1]		= &sc2_vclk_div1.hw,
		[CLKID_VCLK_DIV2_EN]		= &sc2_vclk_div2_en.hw,
		[CLKID_VCLK_DIV4_EN]		= &sc2_vclk_div4_en.hw,
		[CLKID_VCLK_DIV6_EN]		= &sc2_vclk_div6_en.hw,
		[CLKID_VCLK_DIV12_EN]		= &sc2_vclk_div12_en.hw,
		[CLKID_VCLK2_DIV1]		= &sc2_vclk2_div1.hw,
		[CLKID_VCLK2_DIV2_EN]		= &sc2_vclk2_div2_en.hw,
		[CLKID_VCLK2_DIV4_EN]		= &sc2_vclk2_div4_en.hw,
		[CLKID_VCLK2_DIV6_EN]		= &sc2_vclk2_div6_en.hw,
		[CLKID_VCLK2_DIV12_EN]		= &sc2_vclk2_div12_en.hw,
		[CLKID_VCLK_DIV2]		= &sc2_vclk_div2.hw,
		[CLKID_VCLK_DIV4]		= &sc2_vclk_div4.hw,
		[CLKID_VCLK_DIV6]		= &sc2_vclk_div6.hw,
		[CLKID_VCLK_DIV12]		= &sc2_vclk_div12.hw,
		[CLKID_VCLK2_DIV2]		= &sc2_vclk2_div2.hw,
		[CLKID_VCLK2_DIV4]		= &sc2_vclk2_div4.hw,
		[CLKID_VCLK2_DIV6]		= &sc2_vclk2_div6.hw,
		[CLKID_VCLK2_DIV12]		= &sc2_vclk2_div12.hw,
		[CLKID_CTS_ENCI_MUX]		= &sc2_cts_enci_sel.hw,
		[CLKID_CTS_ENCP_MUX]		= &sc2_cts_encp_sel.hw,
		[CLKID_CTS_VDAC_MUX]		= &sc2_cts_vdac_sel.hw,
		[CLKID_HDMI_TX_MUX]		= &sc2_hdmi_tx_sel.hw,
		[CLKID_CTS_ENCI]		= &sc2_cts_enci.hw,
		[CLKID_CTS_ENCP]		= &sc2_cts_encp.hw,
		[CLKID_CTS_VDAC]		= &sc2_cts_vdac.hw,
		[CLKID_HDMI_TX]			= &sc2_hdmi_tx.hw,
		[CLKID_HDMI_MUX]		= &sc2_hdmi_sel.hw,
		[CLKID_HDMI_DIV]		= &sc2_hdmi_div.hw,
		[CLKID_HDMI]			= &sc2_hdmi.hw,

		[CLKID_TS_CLK_DIV]		= &sc2_ts_clk_div.hw,
		[CLKID_TS_CLK_GATE]		= &sc2_ts_clk_gate.hw,

		[CLKID_MALI_0_SEL]		= &sc2_mali_0_sel.hw,
		[CLKID_MALI_0_DIV]		= &sc2_mali_0_div.hw,
		[CLKID_MALI_0]			= &sc2_mali_0.hw,
		[CLKID_MALI_1_SEL]		= &sc2_mali_1_sel.hw,
		[CLKID_MALI_1_DIV]		= &sc2_mali_1_div.hw,
		[CLKID_MALI_1]			= &sc2_mali_1.hw,
		[CLKID_MALI]			= &sc2_mali_mux.hw,

		[CLKID_VDEC_P0_MUX]		= &sc2_vdec_p0_mux.hw,
		[CLKID_VDEC_P0_DIV]		= &sc2_vdec_p0_div.hw,
		[CLKID_VDEC_P0]			= &sc2_vdec_p0.hw,
		[CLKID_VDEC_P1_MUX]		= &sc2_vdec_p1_mux.hw,
		[CLKID_VDEC_P1_DIV]		= &sc2_vdec_p1_div.hw,
		[CLKID_VDEC_P1]			= &sc2_vdec_p1.hw,
		[CLKID_VDEC_MUX]		= &sc2_vdec_mux.hw,

		[CLKID_HCODEC_P0_MUX]		= &sc2_hcodec_p0_mux.hw,
		[CLKID_HCODEC_P0_DIV]		= &sc2_hcodec_p0_div.hw,
		[CLKID_HCODEC_P0]		= &sc2_hcodec_p0.hw,
		[CLKID_HCODEC_P1_MUX]		= &sc2_hcodec_p1_mux.hw,
		[CLKID_HCODEC_P1_DIV]		= &sc2_hcodec_p1_div.hw,
		[CLKID_HCODEC_P1]		= &sc2_hcodec_p1.hw,
		[CLKID_HCODEC_MUX]		= &sc2_hcodec_mux.hw,

		[CLKID_HEVCB_P0_MUX]		= &sc2_hevcb_p0_mux.hw,
		[CLKID_HEVCB_P0_DIV]		= &sc2_hevcb_p0_div.hw,
		[CLKID_HEVCB_P0]		= &sc2_hevcb_p0.hw,
		[CLKID_HEVCB_P1_MUX]		= &sc2_hevcb_p1_mux.hw,
		[CLKID_HEVCB_P1_DIV]		= &sc2_hevcb_p1_div.hw,
		[CLKID_HEVCB_P1]		= &sc2_hevcb_p1.hw,
		[CLKID_HEVCB_MUX]		= &sc2_hevcb_mux.hw,

		[CLKID_HEVCF_P0_MUX]		= &sc2_hevcf_p0_mux.hw,
		[CLKID_HEVCF_P0_DIV]		= &sc2_hevcf_p0_div.hw,
		[CLKID_HEVCF_P0]		= &sc2_hevcf_p0.hw,
		[CLKID_HEVCF_P1_MUX]		= &sc2_hevcf_p1_mux.hw,
		[CLKID_HEVCF_P1_DIV]		= &sc2_hevcf_p1_div.hw,
		[CLKID_HEVCF_P1]		= &sc2_hevcf_p1.hw,
		[CLKID_HEVCF_MUX]		= &sc2_hevcf_mux.hw,

		[CLKID_WAVE_A_MUX]		= &sc2_wave_a_sel.hw,
		[CLKID_WAVE_A_DIV]		= &sc2_wave_a_div.hw,
		[CLKID_WAVE_A_GATE]		= &sc2_wave_aclk.hw,
		[CLKID_WAVE_B_MUX]		= &sc2_wave_b_sel.hw,
		[CLKID_WAVE_B_DIV]		= &sc2_wave_b_div.hw,
		[CLKID_WAVE_B_GATE]		= &sc2_wave_bclk.hw,
		[CLKID_WAVE_C_MUX]		= &sc2_wave_c_sel.hw,
		[CLKID_WAVE_C_DIV]		= &sc2_wave_c_div.hw,
		[CLKID_WAVE_C_GATE]		= &sc2_wave_cclk.hw,

		[CLKID_VPU_0_MUX]		= &sc2_vpu_0_sel.hw,
		[CLKID_VPU_0_DIV]		= &sc2_vpu_0_div.hw,
		[CLKID_VPU_0]			= &sc2_vpu_0.hw,
		[CLKID_VPU_1_MUX]		= &sc2_vpu_1_sel.hw,
		[CLKID_VPU_1_DIV]		= &sc2_vpu_1_div.hw,
		[CLKID_VPU_1]			= &sc2_vpu_1.hw,
		[CLKID_VPU]			= &sc2_vpu.hw,

		[CLKID_VPU_CLKB_TMP_MUX]	= &sc2_vpu_clkb_tmp_mux.hw,
		[CLKID_VPU_CLKB_TMP_DIV]	= &sc2_vpu_clkb_tmp_div.hw,
		[CLKID_VPU_CLKB_TMP]		= &sc2_vpu_clkb_tmp.hw,
		[CLKID_VPU_CLKB_DIV]		= &sc2_vpu_clkb_div.hw,
		[CLKID_VPU_CLKB]		= &sc2_vpu_clkb.hw,
		[CLKID_VPU_CLKC_P0_MUX]		= &sc2_vpu_clkc_p0_mux.hw,
		[CLKID_VPU_CLKC_P0_DIV]		= &sc2_vpu_clkc_p0_div.hw,
		[CLKID_VPU_CLKC_P0]		= &sc2_vpu_clkc_p0.hw,
		[CLKID_VPU_CLKC_P1_MUX]		= &sc2_vpu_clkc_p1_mux.hw,
		[CLKID_VPU_CLKC_P1_DIV]		= &sc2_vpu_clkc_p1_div.hw,
		[CLKID_VPU_CLKC_P1]		= &sc2_vpu_clkc_p1.hw,
		[CLKID_VPU_CLKC_MUX]		= &sc2_vpu_clkc_mux.hw,

		[CLKID_VAPB_0_MUX]		= &sc2_vapb_0_sel.hw,
		[CLKID_VAPB_0_DIV]		= &sc2_vapb_0_div.hw,
		[CLKID_VAPB_0]			= &sc2_vapb_0.hw,
		[CLKID_VAPB_1_MUX]		= &sc2_vapb_1_sel.hw,
		[CLKID_VAPB_1_DIV]		= &sc2_vapb_1_div.hw,
		[CLKID_VAPB_1]			= &sc2_vapb_1.hw,
		[CLKID_VAPB]			= &sc2_vapb.hw,

		[CLKID_GE2D]			= &sc2_ge2d_gate.hw,

		[CLKID_VDIN_MEAS_MUX]		= &sc2_vdin_meas_mux.hw,
		[CLKID_VDIN_MEAS_DIV]		= &sc2_vdin_meas_div.hw,
		[CLKID_VDIN_MEAS_GATE]		= &sc2_vdin_meas_gate.hw,

		[CLKID_SD_EMMC_C_CLK_MUX]	= &sc2_sd_emmc_c_clk0_sel.hw,
		[CLKID_SD_EMMC_C_CLK_DIV]	= &sc2_sd_emmc_c_clk0_div.hw,
		[CLKID_SD_EMMC_C_CLK]		= &sc2_sd_emmc_c_clk0.hw,

		[CLKID_SD_EMMC_A_CLK_MUX]	= &sc2_sd_emmc_a_clk0_sel.hw,
		[CLKID_SD_EMMC_A_CLK_DIV]	= &sc2_sd_emmc_a_clk0_div.hw,
		[CLKID_SD_EMMC_A_CLK]		= &sc2_sd_emmc_a_clk0.hw,
		[CLKID_SD_EMMC_B_CLK_MUX]	= &sc2_sd_emmc_b_clk0_sel.hw,
		[CLKID_SD_EMMC_B_CLK_DIV]	= &sc2_sd_emmc_b_clk0_div.hw,
		[CLKID_SD_EMMC_B_CLK]		= &sc2_sd_emmc_b_clk0.hw,

		[CLKID_SPICC0_MUX]		= &sc2_spicc0_mux.hw,
		[CLKID_SPICC0_DIV]		= &sc2_spicc0_div.hw,
		[CLKID_SPICC0_GATE]		= &sc2_spicc0_gate.hw,
		[CLKID_SPICC1_MUX]		= &sc2_spicc1_mux.hw,
		[CLKID_SPICC1_DIV]		= &sc2_spicc1_div.hw,
		[CLKID_SPICC1_GATE]		= &sc2_spicc1_gate.hw,

		[CLKID_PWM_A_MUX]		= &sc2_pwm_a_mux.hw,
		[CLKID_PWM_A_DIV]		= &sc2_pwm_a_div.hw,
		[CLKID_PWM_A_GATE]		= &sc2_pwm_a_gate.hw,
		[CLKID_PWM_B_MUX]		= &sc2_pwm_b_mux.hw,
		[CLKID_PWM_B_DIV]		= &sc2_pwm_b_div.hw,
		[CLKID_PWM_B_GATE]		= &sc2_pwm_b_gate.hw,
		[CLKID_PWM_C_MUX]		= &sc2_pwm_c_mux.hw,
		[CLKID_PWM_C_DIV]		= &sc2_pwm_c_div.hw,
		[CLKID_PWM_C_GATE]		= &sc2_pwm_c_gate.hw,
		[CLKID_PWM_D_MUX]		= &sc2_pwm_d_mux.hw,
		[CLKID_PWM_D_DIV]		= &sc2_pwm_d_div.hw,
		[CLKID_PWM_D_GATE]		= &sc2_pwm_d_gate.hw,
		[CLKID_PWM_E_MUX]		= &sc2_pwm_e_mux.hw,
		[CLKID_PWM_E_DIV]		= &sc2_pwm_e_div.hw,
		[CLKID_PWM_E_GATE]		= &sc2_pwm_e_gate.hw,
		[CLKID_PWM_F_MUX]		= &sc2_pwm_f_mux.hw,
		[CLKID_PWM_F_DIV]		= &sc2_pwm_f_div.hw,
		[CLKID_PWM_F_GATE]		= &sc2_pwm_f_gate.hw,
		[CLKID_PWM_G_MUX]		= &sc2_pwm_g_mux.hw,
		[CLKID_PWM_G_DIV]		= &sc2_pwm_g_div.hw,
		[CLKID_PWM_G_GATE]		= &sc2_pwm_g_gate.hw,
		[CLKID_PWM_H_MUX]		= &sc2_pwm_h_mux.hw,
		[CLKID_PWM_H_DIV]		= &sc2_pwm_h_div.hw,
		[CLKID_PWM_H_GATE]		= &sc2_pwm_h_gate.hw,
		[CLKID_PWM_I_MUX]		= &sc2_pwm_i_mux.hw,
		[CLKID_PWM_I_DIV]		= &sc2_pwm_i_div.hw,
		[CLKID_PWM_I_GATE]		= &sc2_pwm_i_gate.hw,
		[CLKID_PWM_J_MUX]		= &sc2_pwm_j_mux.hw,
		[CLKID_PWM_J_DIV]		= &sc2_pwm_j_div.hw,
		[CLKID_PWM_J_GATE]		= &sc2_pwm_j_gate.hw,

		[CLKID_SARADC_MUX]		= &sc2_saradc_mux.hw,
		[CLKID_SARADC_DIV]		= &sc2_saradc_div.hw,
		[CLKID_SARADC_GATE]		= &sc2_saradc_gate.hw,

		[CLKID_GEN_MUX]			= &sc2_gen_sel.hw,
		[CLKID_GEN_DIV]			= &sc2_gen_div.hw,
		[CLKID_GEN_GATE]		= &sc2_gen.hw,

		[CLKID_DDR]			= &sc2_ddr.hw,
		[CLKID_DOS]			= &sc2_dos.hw,
		[CLKID_ETHPHY]			= &sc2_ethphy.hw,
		[CLKID_MALI_GATE]		= &sc2_mali.hw,
		[CLKID_AOCPU]			= &sc2_aocpu.hw,
		[CLKID_AUCPU]			= &sc2_aucpu.hw,
		[CLKID_CEC]			= &sc2_cec.hw,
		[CLKID_SD_EMMC_A]		= &sc2_sdemmca.hw,
		[CLKID_SD_EMMC_B]		= &sc2_sdemmcb.hw,
		[CLKID_NAND]			= &sc2_nand.hw,
		[CLKID_SMARTCARD]		= &sc2_smartcard.hw,
		[CLKID_ACODEC]			= &sc2_acodec.hw,
		[CLKID_SPIFC]			= &sc2_spifc.hw,
		[CLKID_MSR_CLK]			= &sc2_msr_clk.hw,
		[CLKID_IR_CTRL]			= &sc2_ir_ctrl.hw,

		[CLKID_AUDIO]			= &sc2_audio.hw,
		[CLKID_ETH]			= &sc2_eth.hw,
		[CLKID_UART_A]			= &sc2_uart_a.hw,
		[CLKID_UART_B]			= &sc2_uart_b.hw,
		[CLKID_UART_C]			= &sc2_uart_c.hw,
		[CLKID_UART_D]			= &sc2_uart_d.hw,
		[CLKID_UART_E]			= &sc2_uart_e.hw,
		[CLKID_AIFIFO]			= &sc2_aififo.hw,
		[CLKID_TS_DDR]			= &sc2_ts_ddr.hw,
		[CLKID_TS_PLL]			= &sc2_ts_pll.hw,
		[CLKID_G2D]			= &sc2_g2d.hw,
		[CLKID_SPICC0]			= &sc2_spicc0.hw,
		[CLKID_SPICC1]			= &sc2_spicc1.hw,
		[CLKID_PCIE]			= &sc2_pcie.hw,
		[CLKID_USB]			= &sc2_usb.hw,
		[CLKID_PCIE_PHY]		= &sc2_pcie_phy.hw,
		[CLKID_I2C_M_A]			= &sc2_i2c_m_a.hw,
		[CLKID_I2C_M_B]			= &sc2_i2c_m_b.hw,
		[CLKID_I2C_M_C]			= &sc2_i2c_m_c.hw,
		[CLKID_I2C_M_D]			= &sc2_i2c_m_d.hw,
		[CLKID_I2C_M_E]			= &sc2_i2c_m_e.hw,
		[CLKID_I2C_M_F]			= &sc2_i2c_m_f.hw,
		[CLKID_HDMITX_APB]		= &sc2_hdmitx_apb.hw,
		[CLKID_I2C_S_A]			= &sc2_i2c_s_a.hw,
		[CLKID_USB1_TO_DDR]		= &sc2_usb1_to_ddr.hw,
		[CLKID_HDCP22]			= &sc2_hdcp22.hw,
		[CLKID_MMC_APB]			= &sc2_mmc_apb.hw,
		[CLKID_RSA]			= &sc2_rsa.hw,
		[CLKID_CPU_DEBUG]		= &sc2_cpu_debug.hw,
		[CLKID_DSPA]			= &sc2_dspa.hw,
		[CLKID_VPU_INTR]		= &sc2_vpu_intr.hw,
		[CLKID_SAR_ADC]			= &sc2_sar_adc.hw,
		[CLKID_GIC]			= &sc2_gic.hw,
		[CLKID_PWM_AB]			= &sc2_pwm_ab.hw,
		[CLKID_PWM_CD]			= &sc2_pwm_cd.hw,
		[CLKID_PWM_EF]			= &sc2_pwm_ef.hw,
		[CLKID_PWM_GH]			= &sc2_pwm_gh.hw,
		[CLKID_PWM_IJ]			= &sc2_pwm_ij.hw,

		[NR_CLKS]			= NULL
	},
	.num = NR_CLKS,
};

/* Convenience table to populate regmap in .probe */
static struct clk_regmap *const sc2_clk_regmaps[] = {
	&sc2_rtc_32k_clkin,
	&sc2_rtc_32k_div,
	&sc2_rtc_32k_xtal,
	&sc2_rtc_32k_sel,
	&sc2_rtc_clk,

	&sc2_sysclk_b_sel,
	&sc2_sysclk_b_div,
	&sc2_sysclk_b,
	&sc2_sysclk_a_sel,
	&sc2_sysclk_a_div,
	&sc2_sysclk_a,
	&sc2_sys_clk,

	&sc2_ceca_32k_clkin,
	&sc2_ceca_32k_div,
	&sc2_ceca_32k_sel_pre,
	&sc2_ceca_32k_sel,
	&sc2_ceca_32k_clkout,
	&sc2_cecb_32k_clkin,
	&sc2_cecb_32k_div,
	&sc2_cecb_32k_sel_pre,
	&sc2_cecb_32k_sel,
	&sc2_cecb_32k_clkout,

	&sc2_sc_clk_mux,
	&sc2_sc_clk_div,
	&sc2_sc_clk_gate,

	&sc2_dspa_a_mux,
	&sc2_dspa_a_div,
	&sc2_dspa_a_gate,
	&sc2_dspa_b_mux,
	&sc2_dspa_b_div,
	&sc2_dspa_b_gate,
	&sc2_dspa_mux,

	&sc2_24M_clk_gate,
	&sc2_12M_clk_gate,
	&sc2_25M_clk_div,
	&sc2_25M_clk_gate,
	&sc2_vid_pll_div,
	&sc2_vid_pll_sel,
	&sc2_vid_pll,
	&sc2_vclk_sel,
	&sc2_vclk2_sel,
	&sc2_vclk_input,
	&sc2_vclk2_input,
	&sc2_vclk_div,
	&sc2_vclk2_div,
	&sc2_vclk,
	&sc2_vclk2,
	&sc2_vclk_div1,
	&sc2_vclk_div2_en,
	&sc2_vclk_div4_en,
	&sc2_vclk_div6_en,
	&sc2_vclk_div12_en,
	&sc2_vclk2_div1,
	&sc2_vclk2_div2_en,
	&sc2_vclk2_div4_en,
	&sc2_vclk2_div6_en,
	&sc2_vclk2_div12_en,
	&sc2_cts_enci_sel,
	&sc2_cts_encp_sel,
	&sc2_cts_vdac_sel,
	&sc2_hdmi_tx_sel,
	&sc2_cts_enci,
	&sc2_cts_encp,
	&sc2_cts_vdac,
	&sc2_hdmi_tx,

	&sc2_hdmi_sel,
	&sc2_hdmi_div,
	&sc2_hdmi,
	&sc2_ts_clk_div,
	&sc2_ts_clk_gate,

	&sc2_mali_0_sel,
	&sc2_mali_0_div,
	&sc2_mali_0,
	&sc2_mali_1_sel,
	&sc2_mali_1_div,
	&sc2_mali_1,
	&sc2_mali_mux,

	&sc2_vdec_p0_mux,
	&sc2_vdec_p0_div,
	&sc2_vdec_p0,
	&sc2_vdec_p1_mux,
	&sc2_vdec_p1_div,
	&sc2_vdec_p1,
	&sc2_vdec_mux,

	&sc2_hcodec_p0_mux,
	&sc2_hcodec_p0_div,
	&sc2_hcodec_p0,
	&sc2_hcodec_p1_mux,
	&sc2_hcodec_p1_div,
	&sc2_hcodec_p1,
	&sc2_hcodec_mux,

	&sc2_hevcb_p0_mux,
	&sc2_hevcb_p0_div,
	&sc2_hevcb_p0,
	&sc2_hevcb_p1_mux,
	&sc2_hevcb_p1_div,
	&sc2_hevcb_p1,
	&sc2_hevcb_mux,

	&sc2_hevcf_p0_mux,
	&sc2_hevcf_p0_div,
	&sc2_hevcf_p0,
	&sc2_hevcf_p1_mux,
	&sc2_hevcf_p1_div,
	&sc2_hevcf_p1,
	&sc2_hevcf_mux,

	&sc2_wave_a_sel,
	&sc2_wave_a_div,
	&sc2_wave_aclk,
	&sc2_wave_b_sel,
	&sc2_wave_b_div,
	&sc2_wave_bclk,
	&sc2_wave_c_sel,
	&sc2_wave_c_div,
	&sc2_wave_cclk,

	&sc2_vpu_0_sel,
	&sc2_vpu_0_div,
	&sc2_vpu_0,
	&sc2_vpu_1_sel,
	&sc2_vpu_1_div,
	&sc2_vpu_1,
	&sc2_vpu,
	&sc2_vpu_clkb_tmp_mux,
	&sc2_vpu_clkb_tmp_div,
	&sc2_vpu_clkb_tmp,
	&sc2_vpu_clkb_div,
	&sc2_vpu_clkb,
	&sc2_vpu_clkc_p0_mux,
	&sc2_vpu_clkc_p0_div,
	&sc2_vpu_clkc_p0,
	&sc2_vpu_clkc_p1_mux,
	&sc2_vpu_clkc_p1_div,
	&sc2_vpu_clkc_p1,
	&sc2_vpu_clkc_mux,

	&sc2_vapb_0_sel,
	&sc2_vapb_0_div,
	&sc2_vapb_0,
	&sc2_vapb_1_sel,
	&sc2_vapb_1_div,
	&sc2_vapb_1,
	&sc2_vapb,

	&sc2_ge2d_gate,

	&sc2_vdin_meas_mux,
	&sc2_vdin_meas_div,
	&sc2_vdin_meas_gate,

	&sc2_sd_emmc_c_clk0_sel,
	&sc2_sd_emmc_c_clk0_div,
	&sc2_sd_emmc_c_clk0,
	&sc2_sd_emmc_a_clk0_sel,
	&sc2_sd_emmc_a_clk0_div,
	&sc2_sd_emmc_a_clk0,
	&sc2_sd_emmc_b_clk0_sel,
	&sc2_sd_emmc_b_clk0_div,
	&sc2_sd_emmc_b_clk0,

	&sc2_spicc0_mux,
	&sc2_spicc0_div,
	&sc2_spicc0_gate,
	&sc2_spicc1_mux,
	&sc2_spicc1_div,
	&sc2_spicc1_gate,

	&sc2_pwm_a_mux,
	&sc2_pwm_a_div,
	&sc2_pwm_a_gate,
	&sc2_pwm_b_mux,
	&sc2_pwm_b_div,
	&sc2_pwm_b_gate,
	&sc2_pwm_c_mux,
	&sc2_pwm_c_div,
	&sc2_pwm_c_gate,
	&sc2_pwm_d_mux,
	&sc2_pwm_d_div,
	&sc2_pwm_d_gate,
	&sc2_pwm_e_mux,
	&sc2_pwm_e_div,
	&sc2_pwm_e_gate,
	&sc2_pwm_f_mux,
	&sc2_pwm_f_div,
	&sc2_pwm_f_gate,
	&sc2_pwm_g_mux,
	&sc2_pwm_g_div,
	&sc2_pwm_g_gate,
	&sc2_pwm_h_mux,
	&sc2_pwm_h_div,
	&sc2_pwm_h_gate,
	&sc2_pwm_i_mux,
	&sc2_pwm_i_div,
	&sc2_pwm_i_gate,
	&sc2_pwm_j_mux,
	&sc2_pwm_j_div,
	&sc2_pwm_j_gate,

	&sc2_saradc_mux,
	&sc2_saradc_div,
	&sc2_saradc_gate,

	&sc2_gen_sel,
	&sc2_gen_div,
	&sc2_gen,

	&sc2_ddr,
	&sc2_dos,
	&sc2_ethphy,
	&sc2_mali,
	&sc2_aocpu,
	&sc2_aucpu,
	&sc2_cec,
	&sc2_sdemmca,
	&sc2_sdemmcb,
	&sc2_nand,
	&sc2_smartcard,
	&sc2_acodec,
	&sc2_spifc,
	&sc2_msr_clk,
	&sc2_ir_ctrl,
	&sc2_audio,
	&sc2_eth,
	&sc2_uart_a,
	&sc2_uart_b,
	&sc2_uart_c,
	&sc2_uart_d,
	&sc2_uart_e,
	&sc2_aififo,
	&sc2_ts_ddr,
	&sc2_ts_pll,
	&sc2_g2d,
	&sc2_spicc0,
	&sc2_spicc1,
	&sc2_pcie,
	&sc2_usb,
	&sc2_pcie_phy,
	&sc2_i2c_m_a,
	&sc2_i2c_m_b,
	&sc2_i2c_m_c,
	&sc2_i2c_m_d,
	&sc2_i2c_m_e,
	&sc2_i2c_m_f,
	&sc2_hdmitx_apb,
	&sc2_i2c_s_a,
	&sc2_usb1_to_ddr,
	&sc2_hdcp22,
	&sc2_mmc_apb,
	&sc2_rsa,
	&sc2_cpu_debug,
	&sc2_dspa,
	&sc2_vpu_intr,
	&sc2_sar_adc,
	&sc2_gic,
	&sc2_pwm_ab,
	&sc2_pwm_cd,
	&sc2_pwm_ef,
	&sc2_pwm_gh,
	&sc2_pwm_ij
};

static struct clk_regmap *const sc2_cpu_clk_regmaps[] = {
	&sc2_cpu_clk_premux0,
	&sc2_cpu_clk_mux0_div,
	&sc2_cpu_clk_postmux0,
	&sc2_cpu_clk_premux1,
	&sc2_cpu_clk_mux1_div,
	&sc2_cpu_clk_postmux1,
	&sc2_cpu_clk_dyn,
	&sc2_cpu_clk,

	&sc2_dsu_clk_premux0,
	&sc2_dsu_clk_premux1,
	&sc2_dsu_clk_mux0_div,
	&sc2_dsu_clk_postmux0,
	&sc2_dsu_clk_mux1_div,
	&sc2_dsu_clk_postmux1,
	&sc2_dsu_clk_dyn,
	&sc2_dsu_final_clk,
	&sc2_dsu_clk,

	&sc2_cpu1_clk,
	&sc2_cpu2_clk,
	&sc2_cpu3_clk
};

static struct clk_regmap *const sc2_pll_clk_regmaps[] = {
	&sc2_fixed_pll_dco,
	&sc2_fixed_pll,
	&sc2_sys_pll_dco,
	&sc2_sys_pll,
	&sc2_fclk_div2,
	&sc2_fclk_div3,
	&sc2_fclk_div4,
	&sc2_fclk_div5,
	&sc2_fclk_div7,
	&sc2_fclk_div2p5,
	&sc2_gp0_pll_dco,
	&sc2_gp0_pll,
	&sc2_gp1_pll_dco,
	&sc2_gp1_pll,

	&sc2_hifi_pll_dco,
	&sc2_hifi_pll,
	&sc2_pcie_pll_dco,
	&sc2_pcie_pll_od,
	&sc2_pcie_hcsl,
	&sc2_hdmi_pll_dco,
	&sc2_hdmi_pll_od,
	&sc2_hdmi_pll,
	&sc2_mpll_50m,
	&sc2_mpll0_div,
	&sc2_mpll0,
	&sc2_mpll1_div,
	&sc2_mpll1,
	&sc2_mpll2_div,
	&sc2_mpll2,
	&sc2_mpll3_div,
	&sc2_mpll3
};

static int meson_sc2_dvfs_setup_common(struct platform_device *pdev,
				       struct clk_hw **hws)
{
	struct clk *notifier_clk;
	struct clk_hw *xtal;
	int ret;

	xtal = clk_hw_get_parent_by_index(hws[CLKID_CPU_CLK_DYN1_SEL], 0);

	/* Setup clock notifier for cpu_clk_postmux0 */
	sc2_cpu_clk_postmux0_nb_data.xtal = xtal;
	notifier_clk = sc2_cpu_clk_postmux0.hw.clk;
	ret = clk_notifier_register(notifier_clk,
				    &sc2_cpu_clk_postmux0_nb_data.nb);
	if (ret) {
		dev_err(&pdev->dev, "failed to register the cpu_clk_postmux0 notifier\n");
		return ret;
	}

	/* Setup clock notifier for cpu_clk_dyn mux */
	notifier_clk = sc2_cpu_clk_dyn.hw.clk;
	ret = clk_notifier_register(notifier_clk, &sc2_cpu_clk_mux_nb);
	if (ret) {
		dev_err(&pdev->dev, "failed to register the cpu_clk_dyn notifier\n");
		return ret;
	}

	return 0;
}

static int meson_sc2_dvfs_setup(struct platform_device *pdev)
{
	struct clk_hw **hws = sc2_hw_onecell_data.hws;
	int ret;

	ret = meson_sc2_dvfs_setup_common(pdev, hws);
	if (ret)
		return ret;

	/* Setup clock notifier for cpu_clk mux */
	ret = clk_notifier_register(sc2_cpu_clk.hw.clk, &sc2_cpu_clk_mux_nb);
	if (ret) {
		dev_err(&pdev->dev, "failed to register cpu_clk notifier\n");
		return ret;
	}

	/* Setup clock notifier for sys_pll */
	ret = clk_notifier_register(sc2_sys_pll.hw.clk,
				    &sc2_sys_pll_nb_data.nb);
	if (ret) {
		dev_err(&pdev->dev, "failed to register sys_pll notifier\n");
		return ret;
	}

	/* Setup clock notifier for dsu */
	/* set sc2_dsu_clk_premux1 parent to fclk_div2 1G */
	ret = clk_set_parent(sc2_dsu_clk_premux1.hw.clk,
			     sc2_fclk_div2.hw.clk);
	if (ret < 0) {
		pr_err("%s: failed to set dsu parent\n", __func__);
		return ret;
	}

	ret = clk_notifier_register(sc2_dsu_clk_postmux0.hw.clk,
				    &sc2_dsu_clk_postmux0_nb_data.nb);
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

static struct regmap *sc2_regmap_resource(struct device *dev, char *name)
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

static int meson_sc2_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct regmap *basic_map;
	struct regmap *pll_map;
	struct regmap *cpu_clk_map;
	int ret, i;

	/* Get regmap for different clock area */
	basic_map = sc2_regmap_resource(dev, "basic");
	if (IS_ERR(basic_map)) {
		dev_err(dev, "basic clk registers not found\n");
		return PTR_ERR(basic_map);
	}

	pll_map = sc2_regmap_resource(dev, "pll");
	if (IS_ERR(pll_map)) {
		dev_err(dev, "pll clk registers not found\n");
		return PTR_ERR(pll_map);
	}

	cpu_clk_map = sc2_regmap_resource(dev, "cpu_clk");
	if (IS_ERR(cpu_clk_map)) {
		dev_err(dev, "cpu clk registers not found\n");
		return PTR_ERR(cpu_clk_map);
	}

	/* Populate regmap for the regmap backed clocks */
	for (i = 0; i < ARRAY_SIZE(sc2_clk_regmaps); i++)
		sc2_clk_regmaps[i]->map = basic_map;

	for (i = 0; i < ARRAY_SIZE(sc2_cpu_clk_regmaps); i++)
		sc2_cpu_clk_regmaps[i]->map = cpu_clk_map;

	for (i = 0; i < ARRAY_SIZE(sc2_pll_clk_regmaps); i++)
		sc2_pll_clk_regmaps[i]->map = pll_map;
	regmap_write(pll_map, ANACTRL_MPLL_CTRL0, 0x00000543);

	for (i = 0; i < sc2_hw_onecell_data.num; i++) {
		/* array might be sparse */
		if (!sc2_hw_onecell_data.hws[i])
			continue;
		/*
		 * dev_err(dev, "register %d  %s\n",i,
		 *	sc2_hw_onecell_data.hws[i]->init->name);
		 */
		ret = devm_clk_hw_register(dev, sc2_hw_onecell_data.hws[i]);
		if (ret) {
			dev_err(dev, "Clock registration failed\n");
			return ret;
		}
	}

	meson_sc2_dvfs_setup(pdev);

	return devm_of_clk_add_hw_provider(dev, of_clk_hw_onecell_get,
					   &sc2_hw_onecell_data);
}

static const struct of_device_id clkc_match_table[] = {
	{
		.compatible = "amlogic,sc2-clkc"
	},
	{}
};

static struct platform_driver sc2_driver = {
	.probe		= meson_sc2_probe,
	.driver		= {
		.name	= "sc2-clkc",
		.of_match_table = clkc_match_table,
	},
};

#ifndef CONFIG_AMLOGIC_MODIFY
builtin_platform_driver(sc2_driver);
#else
#ifndef MODULE
static int sc2_clkc_init(void)
{
	return platform_driver_register(&sc2_driver);
}
arch_initcall_sync(sc2_clkc_init);
#else
int __init meson_sc2_clkc_init(void)
{
	return platform_driver_register(&sc2_driver);
}
#endif
#endif

MODULE_LICENSE("GPL v2");
