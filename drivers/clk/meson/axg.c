// SPDX-License-Identifier: GPL-2.0+
/*
 * AmLogic Meson-AXG Clock Controller Driver
 *
 * Copyright (c) 2016 Baylibre SAS.
 * Author: Michael Turquette <mturquette@baylibre.com>
 *
 * Copyright (c) 2017 Amlogic, inc.
 * Author: Qiufang Dai <qiufang.dai@amlogic.com>
 */

#include <linux/clk-provider.h>
#include <linux/init.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/clk.h>

#include "clk-regmap.h"
#include "clk-pll.h"
#include "clk-mpll.h"
#include "axg.h"
#include "meson-eeclk.h"

static DEFINE_SPINLOCK(meson_clk_lock);

static struct clk_regmap axg_fixed_pll_dco = {
	.data = &(struct meson_clk_pll_data){
		.en = {
			.reg_off = HHI_MPLL_CNTL,
			.shift   = 30,
			.width   = 1,
		},
		.m = {
			.reg_off = HHI_MPLL_CNTL,
			.shift   = 0,
			.width   = 9,
		},
		.n = {
			.reg_off = HHI_MPLL_CNTL,
			.shift   = 9,
			.width   = 5,
		},
		.frac = {
			.reg_off = HHI_MPLL_CNTL2,
			.shift   = 0,
			.width   = 12,
		},
		.l = {
			.reg_off = HHI_MPLL_CNTL,
			.shift   = 31,
			.width   = 1,
		},
		.rst = {
			.reg_off = HHI_MPLL_CNTL,
			.shift   = 29,
			.width   = 1,
		},
	},
	.hw.init = &(struct clk_init_data){
		.name = "fixed_pll_dco",
		.ops = &meson_clk_axg_pll_ro_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "xtal",
		},
		.num_parents = 1,
	},
};

static struct clk_regmap axg_fixed_pll = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_MPLL_CNTL,
		.shift = 16,
		.width = 2,
		.flags = CLK_DIVIDER_POWER_OF_TWO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "fixed_pll",
		.ops = &clk_regmap_divider_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&axg_fixed_pll_dco.hw
		},
		.num_parents = 1,
		/*
		 * This clock won't ever change at runtime so
		 * CLK_SET_RATE_PARENT is not required
		 */
	},
};

static const struct pll_params_table axg_sys_pll_params_table[] = {
	PLL_PARAMS(67, 1), /*DCO=1608M OD=1608MM*/
	PLL_PARAMS(71, 1), /*DCO=1704MM OD=1704M*/
	PLL_PARAMS(75, 1), /*DCO=1800M OD=1800M*/
	PLL_PARAMS(126, 1), /*DCO=3024 OD=1512M*/
	PLL_PARAMS(116, 1), /*DCO=2784 OD=1392M*/
	PLL_PARAMS(118, 1), /*DCO=2832M OD=1416M*/
	PLL_PARAMS(100, 1), /*DCO=2400M OD=1200M*/
	PLL_PARAMS(108, 1), /*DCO=2592M OD=1296M*/
	PLL_PARAMS(79, 1), /*DCO=1896M OD=1896M*/
	PLL_PARAMS(80, 1), /*DCO=1920M OD=1920M*/
	PLL_PARAMS(84, 1), /*DCO=2016M OD=2016M*/
	PLL_PARAMS(92, 1), /*DCO=2208M OD=2208M*/
	{ /* sentinel */ }
};

static struct clk_regmap axg_sys_pll_dco = {
	.data = &(struct meson_clk_pll_data){
		.en = {
			.reg_off = HHI_SYS_PLL_CNTL,
			.shift   = 30,
			.width   = 1,
		},
		.m = {
			.reg_off = HHI_SYS_PLL_CNTL,
			.shift   = 0,
			.width   = 9,
		},
		.n = {
			.reg_off = HHI_SYS_PLL_CNTL,
			.shift   = 9,
			.width   = 5,
		},
		.l = {
			.reg_off = HHI_SYS_PLL_CNTL,
			.shift   = 31,
			.width   = 1,
		},
		.rst = {
			.reg_off = HHI_SYS_PLL_CNTL,
			.shift   = 29,
			.width   = 1,
		},
		.table = axg_sys_pll_params_table,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sys_pll_dco",
		.ops = &meson_clk_pll_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "xtal",
		},
		.num_parents = 1,
		.flags = CLK_IGNORE_UNUSED
	},
};

static struct clk_regmap axg_sys_pll = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_SYS_PLL_CNTL,
		.shift = 16,
		.width = 2,
		.flags = CLK_DIVIDER_POWER_OF_TWO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sys_pll",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&axg_sys_pll_dco.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct pll_params_table axg_gp0_pll_params_table[] = {
	PLL_PARAMS(40, 1),
	PLL_PARAMS(41, 1),
	PLL_PARAMS(42, 1),
	PLL_PARAMS(43, 1),
	PLL_PARAMS(44, 1),
	PLL_PARAMS(45, 1),
	PLL_PARAMS(46, 1),
	PLL_PARAMS(47, 1),
	PLL_PARAMS(48, 1),
	PLL_PARAMS(49, 1),
	PLL_PARAMS(50, 1),
	PLL_PARAMS(51, 1),
	PLL_PARAMS(52, 1),
	PLL_PARAMS(53, 1),
	PLL_PARAMS(54, 1),
	PLL_PARAMS(55, 1),
	PLL_PARAMS(56, 1),
	PLL_PARAMS(57, 1),
	PLL_PARAMS(58, 1),
	PLL_PARAMS(59, 1),
	PLL_PARAMS(60, 1),
	PLL_PARAMS(61, 1),
	PLL_PARAMS(62, 1),
	PLL_PARAMS(63, 1),
	PLL_PARAMS(64, 1),
	PLL_PARAMS(65, 1),
	PLL_PARAMS(66, 1),
	PLL_PARAMS(67, 1),
	PLL_PARAMS(68, 1),
	{ /* sentinel */ },
};

static const struct reg_sequence axg_gp0_init_regs[] = {
	{ .reg = HHI_GP0_PLL_CNTL1,	.def = 0xc084b000 },
	{ .reg = HHI_GP0_PLL_CNTL2,	.def = 0xb75020be },
	{ .reg = HHI_GP0_PLL_CNTL3,	.def = 0x0a59a288 },
	{ .reg = HHI_GP0_PLL_CNTL4,	.def = 0xc000004d },
	{ .reg = HHI_GP0_PLL_CNTL5,	.def = 0x00078000 },
};

static struct clk_regmap axg_gp0_pll_dco = {
	.data = &(struct meson_clk_pll_data){
		.en = {
			.reg_off = HHI_GP0_PLL_CNTL,
			.shift   = 30,
			.width   = 1,
		},
		.m = {
			.reg_off = HHI_GP0_PLL_CNTL,
			.shift   = 0,
			.width   = 9,
		},
		.n = {
			.reg_off = HHI_GP0_PLL_CNTL,
			.shift   = 9,
			.width   = 5,
		},
		.frac = {
			.reg_off = HHI_GP0_PLL_CNTL1,
			.shift   = 0,
			.width   = 12,
		},
		.l = {
			.reg_off = HHI_GP0_PLL_CNTL,
			.shift   = 31,
			.width   = 1,
		},
		.rst = {
			.reg_off = HHI_GP0_PLL_CNTL,
			.shift   = 29,
			.width   = 1,
		},
		.table = axg_gp0_pll_params_table,
		.init_regs = axg_gp0_init_regs,
		.init_count = ARRAY_SIZE(axg_gp0_init_regs),
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

static struct clk_regmap axg_gp0_pll = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_GP0_PLL_CNTL,
		.shift = 16,
		.width = 2,
		.flags = CLK_DIVIDER_POWER_OF_TWO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "gp0_pll",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&axg_gp0_pll_dco.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct pll_params_table axg_hifi_pll_params_table[] = {
	PLL_PARAMS(75, 1), /* DCO = 1800M */
	PLL_PARAMS(81, 1), /* DCO = 1944M */
	{ /* sentinel */ },
};

static const struct reg_sequence axg_hifi_init_regs[] = {
	{ .reg = HHI_HIFI_PLL_CNTL1,	.def = 0xc084b000 },
	{ .reg = HHI_HIFI_PLL_CNTL2,	.def = 0xb75020be },
	{ .reg = HHI_HIFI_PLL_CNTL3,	.def = 0x0a6a3a88 },
	{ .reg = HHI_HIFI_PLL_CNTL4,	.def = 0xc000004d },
	{ .reg = HHI_HIFI_PLL_CNTL5,	.def = 0x000581eb },
};

static struct clk_regmap axg_hifi_pll_dco = {
	.data = &(struct meson_clk_pll_data){
		.en = {
			.reg_off = HHI_HIFI_PLL_CNTL,
			.shift   = 30,
			.width   = 1,
		},
		.m = {
			.reg_off = HHI_HIFI_PLL_CNTL,
			.shift   = 0,
			.width   = 9,
		},
		.n = {
			.reg_off = HHI_HIFI_PLL_CNTL,
			.shift   = 9,
			.width   = 5,
		},
		.frac = {
			.reg_off = HHI_HIFI_PLL_CNTL5,
			.shift   = 0,
			.width   = 15,
		},
		.l = {
			.reg_off = HHI_HIFI_PLL_CNTL,
			.shift   = 31,
			.width   = 1,
		},
		.rst = {
			.reg_off = HHI_HIFI_PLL_CNTL,
			.shift   = 29,
			.width   = 1,
		},
		.table = axg_hifi_pll_params_table,
		.init_regs = axg_hifi_init_regs,
		.init_count = ARRAY_SIZE(axg_hifi_init_regs),
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

static struct clk_regmap axg_hifi_pll = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_HIFI_PLL_CNTL,
		.shift = 16,
		.width = 2,
		.flags = CLK_DIVIDER_POWER_OF_TWO | CLK_DIVIDER_ROUND_CLOSEST,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hifi_pll",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&axg_hifi_pll_dco.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_fixed_factor axg_fclk_div2_div = {
	.mult = 1,
	.div = 2,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div2_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) { &axg_fixed_pll.hw },
		.num_parents = 1,
	},
};

static struct clk_regmap axg_fclk_div2 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_MPLL_CNTL6,
		.bit_idx = 27,
	},
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div2",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&axg_fclk_div2_div.hw
		},
		.num_parents = 1,
		.flags = CLK_IS_CRITICAL,
	},
};

static struct clk_fixed_factor axg_fclk_div3_div = {
	.mult = 1,
	.div = 3,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div3_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) { &axg_fixed_pll.hw },
		.num_parents = 1,
	},
};

static struct clk_regmap axg_fclk_div3 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_MPLL_CNTL6,
		.bit_idx = 28,
	},
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div3",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&axg_fclk_div3_div.hw
		},
		.num_parents = 1,
		/*
		 * FIXME:
		 * This clock, as fdiv2, is used by the SCPI FW and is required
		 * by the platform to operate correctly.
		 * Until the following condition are met, we need this clock to
		 * be marked as critical:
		 * a) The SCPI generic driver claims and enable all the clocks
		 *    it needs
		 * b) CCF has a clock hand-off mechanism to make the sure the
		 *    clock stays on until the proper driver comes along
		 */
		.flags = CLK_IS_CRITICAL,
	},
};

static struct clk_fixed_factor axg_fclk_div4_div = {
	.mult = 1,
	.div = 4,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div4_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) { &axg_fixed_pll.hw },
		.num_parents = 1,
	},
};

static struct clk_regmap axg_fclk_div4 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_MPLL_CNTL6,
		.bit_idx = 29,
	},
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div4",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&axg_fclk_div4_div.hw
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor axg_fclk_div5_div = {
	.mult = 1,
	.div = 5,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div5_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) { &axg_fixed_pll.hw },
		.num_parents = 1,
	},
};

static struct clk_regmap axg_fclk_div5 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_MPLL_CNTL6,
		.bit_idx = 30,
	},
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div5",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&axg_fclk_div5_div.hw
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor axg_fclk_div7_div = {
	.mult = 1,
	.div = 7,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div7_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&axg_fixed_pll.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap axg_fclk_div7 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_MPLL_CNTL6,
		.bit_idx = 31,
	},
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div7",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&axg_fclk_div7_div.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap axg_mpll_prediv = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_MPLL_CNTL5,
		.shift = 12,
		.width = 1,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpll_prediv",
		.ops = &clk_regmap_divider_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&axg_fixed_pll.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap axg_mpll0_div = {
	.data = &(struct meson_clk_mpll_data){
		.sdm = {
			.reg_off = HHI_MPLL_CNTL7,
			.shift   = 0,
			.width   = 14,
		},
		.sdm_en = {
			.reg_off = HHI_MPLL_CNTL7,
			.shift   = 15,
			.width	 = 1,
		},
		.n2 = {
			.reg_off = HHI_MPLL_CNTL7,
			.shift   = 16,
			.width   = 9,
		},
		.misc = {
			.reg_off = HHI_PLL_TOP_MISC,
			.shift   = 0,
			.width	 = 1,
		},
		.lock = &meson_clk_lock,
		.flags = CLK_MESON_MPLL_ROUND_CLOSEST,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpll0_div",
		.ops = &meson_clk_mpll_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&axg_mpll_prediv.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap axg_mpll0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_MPLL_CNTL7,
		.bit_idx = 14,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpll0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&axg_mpll0_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap axg_mpll1_div = {
	.data = &(struct meson_clk_mpll_data){
		.sdm = {
			.reg_off = HHI_MPLL_CNTL8,
			.shift   = 0,
			.width   = 14,
		},
		.sdm_en = {
			.reg_off = HHI_MPLL_CNTL8,
			.shift   = 15,
			.width	 = 1,
		},
		.n2 = {
			.reg_off = HHI_MPLL_CNTL8,
			.shift   = 16,
			.width   = 9,
		},
		.misc = {
			.reg_off = HHI_PLL_TOP_MISC,
			.shift   = 1,
			.width	 = 1,
		},
		.lock = &meson_clk_lock,
		.flags = CLK_MESON_MPLL_ROUND_CLOSEST,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpll1_div",
		.ops = &meson_clk_mpll_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&axg_mpll_prediv.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap axg_mpll1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_MPLL_CNTL8,
		.bit_idx = 14,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpll1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&axg_mpll1_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap axg_mpll2_div = {
	.data = &(struct meson_clk_mpll_data){
		.sdm = {
			.reg_off = HHI_MPLL_CNTL9,
			.shift   = 0,
			.width   = 14,
		},
		.sdm_en = {
			.reg_off = HHI_MPLL_CNTL9,
			.shift   = 15,
			.width	 = 1,
		},
		.n2 = {
			.reg_off = HHI_MPLL_CNTL9,
			.shift   = 16,
			.width   = 9,
		},
		.ssen = {
			.reg_off = HHI_MPLL_CNTL,
			.shift   = 25,
			.width	 = 1,
		},
		.misc = {
			.reg_off = HHI_PLL_TOP_MISC,
			.shift   = 2,
			.width	 = 1,
		},
		.lock = &meson_clk_lock,
		.flags = CLK_MESON_MPLL_ROUND_CLOSEST,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpll2_div",
		.ops = &meson_clk_mpll_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&axg_mpll_prediv.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap axg_mpll2 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_MPLL_CNTL9,
		.bit_idx = 14,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpll2",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&axg_mpll2_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap axg_mpll3_div = {
	.data = &(struct meson_clk_mpll_data){
		.sdm = {
			.reg_off = HHI_MPLL3_CNTL0,
			.shift   = 12,
			.width   = 14,
		},
		.sdm_en = {
			.reg_off = HHI_MPLL3_CNTL0,
			.shift   = 11,
			.width	 = 1,
		},
		.n2 = {
			.reg_off = HHI_MPLL3_CNTL0,
			.shift   = 2,
			.width   = 9,
		},
		.misc = {
			.reg_off = HHI_PLL_TOP_MISC,
			.shift   = 3,
			.width	 = 1,
		},
		.lock = &meson_clk_lock,
		.flags = CLK_MESON_MPLL_ROUND_CLOSEST,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpll3_div",
		.ops = &meson_clk_mpll_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&axg_mpll_prediv.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap axg_mpll3 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_MPLL3_CNTL0,
		.bit_idx = 0,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpll3",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&axg_mpll3_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct pll_params_table axg_pcie_pll_params_table[] = {
	{
		.m = 200,
		.n = 3,
	},
	{ /* sentinel */ },
};

static const struct reg_sequence axg_pcie_init_regs[] = {
	{ .reg = HHI_PCIE_PLL_CNTL1,	.def = 0x0084a2aa },
	{ .reg = HHI_PCIE_PLL_CNTL2,	.def = 0xb75020be },
	{ .reg = HHI_PCIE_PLL_CNTL3,	.def = 0x0a47488e },
	{ .reg = HHI_PCIE_PLL_CNTL4,	.def = 0xc000004d },
	{ .reg = HHI_PCIE_PLL_CNTL5,	.def = 0x00078000 },
	{ .reg = HHI_PCIE_PLL_CNTL6,	.def = 0x002323c6 },
	{ .reg = HHI_PCIE_PLL_CNTL,     .def = 0x400106c8 },
};

static struct clk_regmap axg_pcie_pll_dco = {
	.data = &(struct meson_clk_pll_data){
		.en = {
			.reg_off = HHI_PCIE_PLL_CNTL,
			.shift   = 30,
			.width   = 1,
		},
		.m = {
			.reg_off = HHI_PCIE_PLL_CNTL,
			.shift   = 0,
			.width   = 9,
		},
		.n = {
			.reg_off = HHI_PCIE_PLL_CNTL,
			.shift   = 9,
			.width   = 5,
		},
		.frac = {
			.reg_off = HHI_PCIE_PLL_CNTL1,
			.shift   = 0,
			.width   = 12,
		},
		.l = {
			.reg_off = HHI_PCIE_PLL_CNTL,
			.shift   = 31,
			.width   = 1,
		},
		.rst = {
			.reg_off = HHI_PCIE_PLL_CNTL,
			.shift   = 29,
			.width   = 1,
		},
		.table = axg_pcie_pll_params_table,
		.init_regs = axg_pcie_init_regs,
		.init_count = ARRAY_SIZE(axg_pcie_init_regs),
	},
	.hw.init = &(struct clk_init_data){
		.name = "pcie_pll_dco",
		.ops = &meson_clk_pll_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "xtal",
		},
		.num_parents = 1,
	},
};

static struct clk_regmap axg_pcie_pll_od = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_PCIE_PLL_CNTL,
		.shift = 16,
		.width = 2,
		.flags = CLK_DIVIDER_POWER_OF_TWO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pcie_pll_od",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&axg_pcie_pll_dco.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap axg_pcie_pll = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_PCIE_PLL_CNTL6,
		.shift = 6,
		.width = 2,
		.flags = CLK_DIVIDER_POWER_OF_TWO,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pcie_pll",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&axg_pcie_pll_od.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap axg_pcie_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_PCIE_PLL_CNTL6,
		.mask = 0x1,
		.shift = 2,
		/* skip the parent mpll3, reserved for debug */
		.table = (u32[]){ 1 },
	},
	.hw.init = &(struct clk_init_data){
		.name = "pcie_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) { &axg_pcie_pll.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap axg_pcie_ref = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_PCIE_PLL_CNTL6,
		.mask = 0x1,
		.shift = 1,
		/* skip the parent 0, reserved for debug */
		.table = (u32[]){ 1 },
	},
	.hw.init = &(struct clk_init_data){
		.name = "pcie_ref",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) { &axg_pcie_mux.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap axg_pcie_cml_en0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_PCIE_PLL_CNTL6,
		.bit_idx = 4,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pcie_cml_en0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &axg_pcie_ref.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,

	},
};

static struct clk_regmap axg_pcie_cml_en1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_PCIE_PLL_CNTL6,
		.bit_idx = 3,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pcie_cml_en1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &axg_pcie_ref.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static u32 mux_table_clk81[]	= { 0, 2, 3, 4, 5, 6, 7 };
static const struct clk_parent_data clk81_parent_data[] = {
	{ .fw_name = "xtal", },
	{ .hw = &axg_fclk_div7.hw },
	{ .hw = &axg_mpll1.hw },
	{ .hw = &axg_mpll2.hw },
	{ .hw = &axg_fclk_div4.hw },
	{ .hw = &axg_fclk_div3.hw },
	{ .hw = &axg_fclk_div5.hw },
};

static struct clk_regmap axg_mpeg_clk_sel = {
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

static struct clk_regmap axg_mpeg_clk_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_MPEG_CLK_CNTL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpeg_clk_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&axg_mpeg_clk_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap axg_clk81 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_MPEG_CLK_CNTL,
		.bit_idx = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "clk81",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&axg_mpeg_clk_div.hw
		},
		.num_parents = 1,
		.flags = (CLK_SET_RATE_PARENT | CLK_IS_CRITICAL),
	},
};

static const struct clk_parent_data axg_sd_emmc_clk0_parent_data[] = {
	{ .fw_name = "xtal", },
	{ .hw = &axg_fclk_div2.hw },
	{ .hw = &axg_fclk_div3.hw },
	{ .hw = &axg_fclk_div5.hw },
	{ .hw = &axg_fclk_div7.hw },
	/*
	 * Following these parent clocks, we should also have had mpll2, mpll3
	 * and gp0_pll but these clocks are too precious to be used here. All
	 * the necessary rates for MMC and NAND operation can be achieved using
	 * xtal or fclk_div clocks
	 */
};

/* SDcard clock */
static struct clk_regmap axg_sd_emmc_b_clk0_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_SD_EMMC_CLK_CNTL,
		.mask = 0x7,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_b_clk0_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = axg_sd_emmc_clk0_parent_data,
		.num_parents = ARRAY_SIZE(axg_sd_emmc_clk0_parent_data),
	},
};

static struct clk_regmap axg_sd_emmc_b_clk0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_SD_EMMC_CLK_CNTL,
		.shift = 16,
		.width = 7,
		.flags = CLK_DIVIDER_ROUND_CLOSEST,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_b_clk0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&axg_sd_emmc_b_clk0_sel.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap axg_sd_emmc_b_clk0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_SD_EMMC_CLK_CNTL,
		.bit_idx = 23,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sd_emmc_b_clk0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&axg_sd_emmc_b_clk0_div.hw
		},
		.num_parents = 1,
	},
};

/* EMMC/NAND clock */
static struct clk_regmap axg_sd_emmc_c_clk0_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_NAND_CLK_CNTL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_c_clk0_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = axg_sd_emmc_clk0_parent_data,
		.num_parents = ARRAY_SIZE(axg_sd_emmc_clk0_parent_data),
	},
};

static struct clk_regmap axg_sd_emmc_c_clk0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_NAND_CLK_CNTL,
		.shift = 0,
		.width = 7,
		.flags = CLK_DIVIDER_ROUND_CLOSEST,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_c_clk0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&axg_sd_emmc_c_clk0_sel.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap axg_sd_emmc_c_clk0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_NAND_CLK_CNTL,
		.bit_idx = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sd_emmc_c_clk0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&axg_sd_emmc_c_clk0_div.hw
		},
		.num_parents = 1,
	},
};

static u32 mux_table_gen_clk[]	= { 0, 4, 5, 6, 7, 8,
				    9, 10, 11, 13, 14, };
static const struct clk_parent_data gen_clk_parent_data[] = {
	{ .fw_name = "xtal", },
	{ .hw = &axg_hifi_pll.hw },
	{ .hw = &axg_mpll0.hw },
	{ .hw = &axg_mpll1.hw },
	{ .hw = &axg_mpll2.hw },
	{ .hw = &axg_mpll3.hw },
	{ .hw = &axg_fclk_div4.hw },
	{ .hw = &axg_fclk_div3.hw },
	{ .hw = &axg_fclk_div5.hw },
	{ .hw = &axg_fclk_div7.hw },
	{ .hw = &axg_gp0_pll.hw },
};

static struct clk_regmap axg_gen_clk_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_GEN_CLK_CNTL,
		.mask = 0xf,
		.shift = 12,
		.table = mux_table_gen_clk,
	},
	.hw.init = &(struct clk_init_data){
		.name = "gen_clk_sel",
		.ops = &clk_regmap_mux_ops,
		/*
		 * bits 15:12 selects from 14 possible parents:
		 * xtal, [rtc_oscin_i], [sys_cpu_div16], [ddr_dpll_pt],
		 * hifi_pll, mpll0, mpll1, mpll2, mpll3, fdiv4,
		 * fdiv3, fdiv5, [cts_msr_clk], fdiv7, gp0_pll
		 */
		.parent_data = gen_clk_parent_data,
		.num_parents = ARRAY_SIZE(gen_clk_parent_data),
	},
};

static struct clk_regmap axg_gen_clk_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_GEN_CLK_CNTL,
		.shift = 0,
		.width = 11,
	},
	.hw.init = &(struct clk_init_data){
		.name = "gen_clk_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&axg_gen_clk_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap axg_gen_clk = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_GEN_CLK_CNTL,
		.bit_idx = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "gen_clk",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&axg_gen_clk_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap axg_cpu_clk_premux0 = {
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
			{ .hw = &axg_fclk_div2.hw },
			{ .hw = &axg_fclk_div3.hw },
		},
		.num_parents = 3,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static u32 mux_table_vpu_sel[] = { 0, 1, 2, 3 };

static const struct clk_hw *axg_vpu_parent_hws[] = {
	&axg_fclk_div4.hw,
	&axg_fclk_div3.hw,
	&axg_fclk_div5.hw,
	&axg_fclk_div7.hw,
};

static struct clk_regmap axg_vpu_0_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VPU_CLK_CNTL,
		.mask = 0x7,
		.shift = 9,
		.table = mux_table_vpu_sel
	},
	.hw.init = &(struct clk_init_data){
		.name = "vpu_0_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = axg_vpu_parent_hws,
		.num_parents = ARRAY_SIZE(axg_vpu_parent_hws),
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap axg_vpu_0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VPU_CLK_CNTL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vpu_0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) { &axg_vpu_0_sel.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap axg_vpu_0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VPU_CLK_CNTL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &axg_vpu_0_div.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap axg_vpu_1_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VPU_CLK_CNTL,
		.mask = 0x7,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vpu_1_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = axg_vpu_parent_hws,
		.num_parents = ARRAY_SIZE(axg_vpu_parent_hws),
		.flags = CLK_SET_RATE_NO_REPARENT,
	},
};

static struct clk_regmap axg_vpu_1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VPU_CLK_CNTL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vpu_1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) { &axg_vpu_1_sel.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap axg_vpu_1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VPU_CLK_CNTL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &axg_vpu_1_div.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap axg_vpu = {
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
			&axg_vpu_0.hw,
			&axg_vpu_1.hw,
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_NO_REPARENT,
	},
};

static struct clk_regmap axg_vapb_0_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VAPBCLK_CNTL,
		.mask = 0x3,
		.shift = 9,
		.table = mux_table_vpu_sel
	},
	.hw.init = &(struct clk_init_data){
		.name = "vapb_0_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = axg_vpu_parent_hws,
		.num_parents = ARRAY_SIZE(axg_vpu_parent_hws),
		.flags = CLK_SET_RATE_NO_REPARENT,
	},
};

static struct clk_regmap axg_vapb_0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VAPBCLK_CNTL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vapb_0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&axg_vapb_0_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap axg_vapb_0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VAPBCLK_CNTL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vapb_0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&axg_vapb_0_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap axg_vapb_1_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VAPBCLK_CNTL,
		.mask = 0x3,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vapb_1_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = axg_vpu_parent_hws,
		.num_parents = ARRAY_SIZE(axg_vpu_parent_hws),
		.flags = CLK_SET_RATE_NO_REPARENT,
	},
};

static struct clk_regmap axg_vapb_1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VAPBCLK_CNTL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vapb_1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&axg_vapb_1_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap axg_vapb_1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VAPBCLK_CNTL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vapb_1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&axg_vapb_1_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap axg_vapb_sel = {
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
			&axg_vapb_0.hw,
			&axg_vapb_1.hw,
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_NO_REPARENT | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap axg_vapb = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VAPBCLK_CNTL,
		.bit_idx = 30,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vapb",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &axg_vapb_sel.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static u32 mux_table_means_sel[] = { 0, 1, 2, 3, 6, 7 };

static const struct clk_parent_data axg_meas_parent_hws[] = {
	{ .fw_name = "xtal", },
	{ .hw = &axg_fclk_div4.hw },
	{ .hw = &axg_fclk_div3.hw },
	{ .hw = &axg_fclk_div5.hw },
	{ .hw = &axg_fclk_div2.hw },
	{ .hw = &axg_fclk_div7.hw },
};

static struct clk_regmap axg_dsi_meas_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VDIN_MEAS_CLK_CNTL,
		.mask = 0x7,
		.shift = 21,
		.table = mux_table_means_sel
	},
	.hw.init = &(struct clk_init_data) {
		.name = "dsi_meas_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = axg_meas_parent_hws,
		.num_parents = ARRAY_SIZE(axg_meas_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap axg_dsi_meas_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VDIN_MEAS_CLK_CNTL,
		.shift = 12,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "dsi_meas_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&axg_dsi_meas_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap axg_dsi_meas = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VDIN_MEAS_CLK_CNTL,
		.bit_idx = 20,
	},
	.hw.init = &(struct clk_init_data){
		.name = "dsi_meas",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&axg_dsi_meas_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap axg_spicc_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_SPICC_HCLK_CNTL,
		.mask = 0x3,
		.shift = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spicc_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = axg_vpu_parent_hws,
		.num_parents = ARRAY_SIZE(axg_vpu_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap axg_spicc_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_SPICC_HCLK_CNTL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spicc_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&axg_spicc_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap axg_spicc = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_SPICC_HCLK_CNTL,
		.bit_idx = 15,
	},
	.hw.init = &(struct clk_init_data){
		.name = "spicc",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&axg_spicc_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap axg_cpu_clk_premux1 = {
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
			{ .hw = &axg_fclk_div2.hw },
			{ .hw = &axg_fclk_div3.hw },
		},
		.num_parents = 3,
		/* This sub-tree is used a parking clock */
		.flags = CLK_SET_RATE_NO_REPARENT
	},
};

static struct clk_regmap axg_cpu_clk_mux0_div = {
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
			&axg_cpu_clk_premux0.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap axg_cpu_clk_postmux0 = {
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
			&axg_cpu_clk_premux0.hw,
			&axg_cpu_clk_mux0_div.hw,
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap axg_cpu_clk_mux1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_SYS_CPU_CLK_CNTL0,
		.shift = 20,
		.width = 6,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cpu_clk_dyn1_div",
		.ops = &clk_regmap_divider_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&axg_cpu_clk_premux1.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap axg_cpu_clk_postmux1 = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_SYS_CPU_CLK_CNTL0,
		.mask = 0x1,
		.shift = 18,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cpu_clk_dyn1",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&axg_cpu_clk_premux1.hw,
			&axg_cpu_clk_mux1_div.hw,
		},
		.num_parents = 2,
		/* This sub-tree is used a parking clock */
		.flags = CLK_SET_RATE_NO_REPARENT,
	},
};

static struct clk_regmap axg_cpu_clk_dyn = {
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
			&axg_cpu_clk_postmux0.hw,
			&axg_cpu_clk_postmux1.hw,
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap axg_cpu_clk = {
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
			&axg_cpu_clk_dyn.hw,
			&axg_sys_pll.hw,
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

struct axg_cpu_clk_postmux_nb_data {
	struct notifier_block nb;
	struct clk_hw *fclk_div2;
	struct clk_hw *cpu_clk_dyn;
	struct clk_hw *cpu_clk_postmux0;
	struct clk_hw *cpu_clk_postmux1;
	struct clk_hw *cpu_clk_premux1;
};

static int axg_cpu_clk_postmux_notifier_cb(struct notifier_block *nb,
					   unsigned long event, void *data)
{
	struct axg_cpu_clk_postmux_nb_data *nb_data =
		container_of(nb, struct axg_cpu_clk_postmux_nb_data, nb);

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

		udelay(10);

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

		udelay(10);

		return NOTIFY_OK;

	default:
		return NOTIFY_DONE;
	}
}

static struct axg_cpu_clk_postmux_nb_data axg_cpu_clk_postmux0_nb_data = {
	.cpu_clk_dyn = &axg_cpu_clk_dyn.hw,
	.cpu_clk_postmux0 = &axg_cpu_clk_postmux0.hw,
	.cpu_clk_postmux1 = &axg_cpu_clk_postmux1.hw,
	.cpu_clk_premux1 = &axg_cpu_clk_premux1.hw,
	.nb.notifier_call = axg_cpu_clk_postmux_notifier_cb,
};

struct axg_sys_pll_nb_data {
	struct notifier_block nb;
	struct clk_hw *sys_pll;
	struct clk_hw *cpu_clk;
	struct clk_hw *cpu_clk_dyn;
};

static int axg_sys_pll_notifier_cb(struct notifier_block *nb,
				   unsigned long event, void *data)
{
	struct axg_sys_pll_nb_data *nb_data =
		container_of(nb, struct axg_sys_pll_nb_data, nb);

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

		udelay(10);

		return NOTIFY_OK;

	case POST_RATE_CHANGE:
		/*
		 * The sys_pll has ben updated, now switch back cpu_clk to
		 * sys_pll
		 */

		/* Configure cpu_clk to use sys_pll */
		clk_hw_set_parent(nb_data->cpu_clk,
				  nb_data->sys_pll);

		udelay(10);

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

static struct axg_sys_pll_nb_data axg_sys_pll_nb_data = {
	.sys_pll = &axg_sys_pll.hw,
	.cpu_clk = &axg_cpu_clk.hw,
	.cpu_clk_dyn = &axg_cpu_clk_dyn.hw,
	.nb.notifier_call = axg_sys_pll_notifier_cb,
};

#define MESON_GATE(_name, _reg, _bit) \
	MESON_PCLK(_name, _reg, _bit, &axg_clk81.hw)

/* Everything Else (EE) domain gates */
static MESON_GATE(axg_clk81_ddr, HHI_GCLK_MPEG0, 0);
static MESON_GATE(axg_clk81_audio_locker, HHI_GCLK_MPEG0, 2);
static MESON_GATE(axg_clk81_mipi_dsi_host, HHI_GCLK_MPEG0, 3);
static MESON_GATE(axg_clk81_isa, HHI_GCLK_MPEG0, 5);
static MESON_GATE(axg_clk81_pl301, HHI_GCLK_MPEG0, 6);
static MESON_GATE(axg_clk81_periphs, HHI_GCLK_MPEG0, 7);
static MESON_GATE(axg_clk81_spicc_0, HHI_GCLK_MPEG0, 8);
static MESON_GATE(axg_clk81_i2c, HHI_GCLK_MPEG0, 9);
static MESON_GATE(axg_clk81_rng0, HHI_GCLK_MPEG0, 12);
static MESON_GATE(axg_clk81_uart0, HHI_GCLK_MPEG0, 13);
static MESON_GATE(axg_clk81_mipi_dsi_phy, HHI_GCLK_MPEG0, 14);
static MESON_GATE(axg_clk81_spicc_1, HHI_GCLK_MPEG0, 15);
static MESON_GATE(axg_clk81_pcie_a, HHI_GCLK_MPEG0, 16);
static MESON_GATE(axg_clk81_pcie_b, HHI_GCLK_MPEG0, 17);
static MESON_GATE(axg_clk81_hiu_reg, HHI_GCLK_MPEG0, 19);
static MESON_GATE(axg_clk81_assist_misc, HHI_GCLK_MPEG0, 23);
static MESON_GATE(axg_clk81_emmc_b, HHI_GCLK_MPEG0, 25);
static MESON_GATE(axg_clk81_emmc_c, HHI_GCLK_MPEG0, 26);
static MESON_GATE(axg_clk81_dma, HHI_GCLK_MPEG0, 27);
static MESON_GATE(axg_clk81_spi, HHI_GCLK_MPEG0, 30);

static MESON_GATE(axg_clk81_audio, HHI_GCLK_MPEG1, 0);
static MESON_GATE(axg_clk81_eth_core, HHI_GCLK_MPEG1, 3);
static MESON_GATE(axg_clk81_uart1, HHI_GCLK_MPEG1, 16);
static MESON_GATE(axg_clk81_g2d, HHI_GCLK_MPEG1, 20);
static MESON_GATE(axg_clk81_usb0, HHI_GCLK_MPEG1, 21);
static MESON_GATE(axg_clk81_usb1, HHI_GCLK_MPEG1, 22);
static MESON_GATE(axg_clk81_reset, HHI_GCLK_MPEG1, 23);
static MESON_GATE(axg_clk81_usb_general, HHI_GCLK_MPEG1, 26);
static MESON_GATE(axg_clk81_ahb_arb0, HHI_GCLK_MPEG1, 29);
static MESON_GATE(axg_clk81_efuse, HHI_GCLK_MPEG1, 30);
static MESON_GATE(axg_clk81_boot_rom, HHI_GCLK_MPEG1, 31);

static MESON_GATE(axg_clk81_ahb_data_bus, HHI_GCLK_MPEG2, 1);
static MESON_GATE(axg_clk81_ahb_ctrl_bus, HHI_GCLK_MPEG2, 2);
static MESON_GATE(axg_clk81_usb1_to_ddr, HHI_GCLK_MPEG2, 8);
static MESON_GATE(axg_clk81_usb0_to_ddr, HHI_GCLK_MPEG2, 9);
static MESON_GATE(axg_clk81_mmc_pclk, HHI_GCLK_MPEG2, 11);
static MESON_GATE(axg_clk81_vpu_intr, HHI_GCLK_MPEG2, 25);
static MESON_GATE(axg_clk81_sec_ahb_ahb3_bridge, HHI_GCLK_MPEG2, 26);
static MESON_GATE(axg_clk81_gic, HHI_GCLK_MPEG2, 30);
static MESON_GATE(axg_clk81_mipi_enable, HHI_MIPI_CNTL0, 29);
static MESON_GATE(axg_clk81_mipi_bandgap, HHI_MIPI_CNTL0, 26);

/* Always On (AO) domain gates */

static MESON_GATE(axg_ao_media_cpu, HHI_GCLK_AO, 0);
static MESON_GATE(axg_ao_ahb_sram, HHI_GCLK_AO, 1);
static MESON_GATE(axg_ao_ahb_bus, HHI_GCLK_AO, 2);
static MESON_GATE(axg_ao_iface, HHI_GCLK_AO, 3);
static MESON_GATE(axg_ao_i2c, HHI_GCLK_AO, 4);

/* Array of all clocks provided by this provider */

static struct clk_hw_onecell_data axg_hw_onecell_data = {
	.hws = {
		[CLKID_SYS_PLL]			= &axg_sys_pll.hw,
		[CLKID_FIXED_PLL]		= &axg_fixed_pll.hw,
		[CLKID_FCLK_DIV2]		= &axg_fclk_div2.hw,
		[CLKID_FCLK_DIV3]		= &axg_fclk_div3.hw,
		[CLKID_FCLK_DIV4]		= &axg_fclk_div4.hw,
		[CLKID_FCLK_DIV5]		= &axg_fclk_div5.hw,
		[CLKID_FCLK_DIV7]		= &axg_fclk_div7.hw,
		[CLKID_GP0_PLL]			= &axg_gp0_pll.hw,
		[CLKID_MPEG_SEL]		= &axg_mpeg_clk_sel.hw,
		[CLKID_MPEG_DIV]		= &axg_mpeg_clk_div.hw,
		[CLKID_CLK81]			= &axg_clk81.hw,
		[CLKID_MPLL0]			= &axg_mpll0.hw,
		[CLKID_MPLL1]			= &axg_mpll1.hw,
		[CLKID_MPLL2]			= &axg_mpll2.hw,
		[CLKID_MPLL3]			= &axg_mpll3.hw,
		[CLKID_CLK81_DDR]		= &axg_clk81_ddr.hw,
		[CLKID_CLK81_AUDIO_LOCKER]	= &axg_clk81_audio_locker.hw,
		[CLKID_CLK81_MIPI_DSI_HOST]	= &axg_clk81_mipi_dsi_host.hw,
		[CLKID_CLK81_ISA]		= &axg_clk81_isa.hw,
		[CLKID_CLK81_PL301]		= &axg_clk81_pl301.hw,
		[CLKID_CLK81_PERIPHS]		= &axg_clk81_periphs.hw,
		[CLKID_CLK81_SPICC0]		= &axg_clk81_spicc_0.hw,
		[CLKID_CLK81_I2C]		= &axg_clk81_i2c.hw,
		[CLKID_CLK81_RNG0]		= &axg_clk81_rng0.hw,
		[CLKID_CLK81_UART0]		= &axg_clk81_uart0.hw,
		[CLKID_CLK81_MIPI_DSI_PHY]	= &axg_clk81_mipi_dsi_phy.hw,
		[CLKID_CLK81_SPICC1]		= &axg_clk81_spicc_1.hw,
		[CLKID_CLK81_PCIE_A]		= &axg_clk81_pcie_a.hw,
		[CLKID_CLK81_PCIE_B]		= &axg_clk81_pcie_b.hw,
		[CLKID_CLK81_HIU_IFACE]		= &axg_clk81_hiu_reg.hw,
		[CLKID_CLK81_ASSIST_MISC]	= &axg_clk81_assist_misc.hw,
		[CLKID_CLK81_SD_EMMC_B]		= &axg_clk81_emmc_b.hw,
		[CLKID_CLK81_SD_EMMC_C]		= &axg_clk81_emmc_c.hw,
		[CLKID_CLK81_DMA]		= &axg_clk81_dma.hw,
		[CLKID_CLK81_SPI]		= &axg_clk81_spi.hw,
		[CLKID_CLK81_AUDIO]		= &axg_clk81_audio.hw,
		[CLKID_CLK81_ETH]		= &axg_clk81_eth_core.hw,
		[CLKID_CLK81_UART1]		= &axg_clk81_uart1.hw,
		[CLKID_CLK81_G2D]		= &axg_clk81_g2d.hw,
		[CLKID_CLK81_USB0]		= &axg_clk81_usb0.hw,
		[CLKID_CLK81_USB1]		= &axg_clk81_usb1.hw,
		[CLKID_CLK81_RESET]		= &axg_clk81_reset.hw,
		[CLKID_CLK81_USB]		= &axg_clk81_usb_general.hw,
		[CLKID_CLK81_AHB_ARB0]		= &axg_clk81_ahb_arb0.hw,
		[CLKID_CLK81_EFUSE]		= &axg_clk81_efuse.hw,
		[CLKID_CLK81_BOOT_ROM]		= &axg_clk81_boot_rom.hw,
		[CLKID_CLK81_AHB_DATA_BUS]	= &axg_clk81_ahb_data_bus.hw,
		[CLKID_CLK81_AHB_CTRL_BUS]	= &axg_clk81_ahb_ctrl_bus.hw,
		[CLKID_CLK81_USB1_DDR_BRIDGE]	= &axg_clk81_usb1_to_ddr.hw,
		[CLKID_CLK81_USB0_DDR_BRIDGE]	= &axg_clk81_usb0_to_ddr.hw,
		[CLKID_CLK81_MMC_PCLK]		= &axg_clk81_mmc_pclk.hw,
		[CLKID_CLK81_VPU_INTR]		= &axg_clk81_vpu_intr.hw,
		[CLKID_CLK81_SEC_AHB_AHB3_BRIDGE]	= &axg_clk81_sec_ahb_ahb3_bridge.hw,
		[CLKID_CLK81_GIC]		= &axg_clk81_gic.hw,
		[CLKID_CLK81_MIPI_ENABLE]	= &axg_clk81_mipi_enable.hw,
		[CLKID_CLK81_MIPI_BANDGAP]	= &axg_clk81_mipi_bandgap.hw,
		[CLKID_AO_MEDIA_CPU]		= &axg_ao_media_cpu.hw,
		[CLKID_AO_AHB_SRAM]		= &axg_ao_ahb_sram.hw,
		[CLKID_AO_AHB_BUS]		= &axg_ao_ahb_bus.hw,
		[CLKID_AO_IFACE]		= &axg_ao_iface.hw,
		[CLKID_AO_I2C]			= &axg_ao_i2c.hw,
		[CLKID_SD_EMMC_B_CLK0_SEL]	= &axg_sd_emmc_b_clk0_sel.hw,
		[CLKID_SD_EMMC_B_CLK0_DIV]	= &axg_sd_emmc_b_clk0_div.hw,
		[CLKID_SD_EMMC_B_CLK0]		= &axg_sd_emmc_b_clk0.hw,
		[CLKID_SD_EMMC_C_CLK0_SEL]	= &axg_sd_emmc_c_clk0_sel.hw,
		[CLKID_SD_EMMC_C_CLK0_DIV]	= &axg_sd_emmc_c_clk0_div.hw,
		[CLKID_SD_EMMC_C_CLK0]		= &axg_sd_emmc_c_clk0.hw,
		[CLKID_MPLL0_DIV]		= &axg_mpll0_div.hw,
		[CLKID_MPLL1_DIV]		= &axg_mpll1_div.hw,
		[CLKID_MPLL2_DIV]		= &axg_mpll2_div.hw,
		[CLKID_MPLL3_DIV]		= &axg_mpll3_div.hw,
		[CLKID_HIFI_PLL]		= &axg_hifi_pll.hw,
		[CLKID_MPLL_PREDIV]		= &axg_mpll_prediv.hw,
		[CLKID_FCLK_DIV2_DIV]		= &axg_fclk_div2_div.hw,
		[CLKID_FCLK_DIV3_DIV]		= &axg_fclk_div3_div.hw,
		[CLKID_FCLK_DIV4_DIV]		= &axg_fclk_div4_div.hw,
		[CLKID_FCLK_DIV5_DIV]		= &axg_fclk_div5_div.hw,
		[CLKID_FCLK_DIV7_DIV]		= &axg_fclk_div7_div.hw,
		[CLKID_PCIE_PLL]		= &axg_pcie_pll.hw,
		[CLKID_PCIE_MUX]		= &axg_pcie_mux.hw,
		[CLKID_PCIE_REF]		= &axg_pcie_ref.hw,
		[CLKID_PCIE_CML_EN0]		= &axg_pcie_cml_en0.hw,
		[CLKID_PCIE_CML_EN1]		= &axg_pcie_cml_en1.hw,
		[CLKID_GEN_CLK_SEL]		= &axg_gen_clk_sel.hw,
		[CLKID_GEN_CLK_DIV]		= &axg_gen_clk_div.hw,
		[CLKID_GEN_CLK]			= &axg_gen_clk.hw,
		[CLKID_SYS_PLL_DCO]		= &axg_sys_pll_dco.hw,
		[CLKID_FIXED_PLL_DCO]		= &axg_fixed_pll_dco.hw,
		[CLKID_GP0_PLL_DCO]		= &axg_gp0_pll_dco.hw,
		[CLKID_HIFI_PLL_DCO]		= &axg_hifi_pll_dco.hw,
		[CLKID_PCIE_PLL_DCO]		= &axg_pcie_pll_dco.hw,
		[CLKID_PCIE_PLL_OD]		= &axg_pcie_pll_od.hw,
		[CLKID_CPU_CLK_DYN0_SEL]	= &axg_cpu_clk_premux0.hw,
		[CLKID_CPU_CLK_DYN0_DIV]	= &axg_cpu_clk_mux0_div.hw,
		[CLKID_CPU_CLK_DYN0]		= &axg_cpu_clk_postmux0.hw,
		[CLKID_CPU_CLK_DYN1_SEL]	= &axg_cpu_clk_premux1.hw,
		[CLKID_CPU_CLK_DYN1_DIV]	= &axg_cpu_clk_mux1_div.hw,
		[CLKID_CPU_CLK_DYN1]		= &axg_cpu_clk_postmux1.hw,
		[CLKID_CPU_CLK_DYN]		= &axg_cpu_clk_dyn.hw,
		[CLKID_CPU_CLK]			= &axg_cpu_clk.hw,
		[CLKID_VPU_0_SEL]		= &axg_vpu_0_sel.hw,
		[CLKID_VPU_0_DIV]		= &axg_vpu_0_div.hw,
		[CLKID_VPU_0]			= &axg_vpu_0.hw,
		[CLKID_VPU_1_SEL]		= &axg_vpu_1_sel.hw,
		[CLKID_VPU_1_DIV]		= &axg_vpu_1_div.hw,
		[CLKID_VPU_1]			= &axg_vpu_1.hw,
		[CLKID_VPU]			= &axg_vpu.hw,
		[CLKID_VAPB_0_SEL]		= &axg_vapb_0_sel.hw,
		[CLKID_VAPB_0_DIV]		= &axg_vapb_0_div.hw,
		[CLKID_VAPB_0]			= &axg_vapb_0.hw,
		[CLKID_VAPB_1_SEL]		= &axg_vapb_1_sel.hw,
		[CLKID_VAPB_1_DIV]		= &axg_vapb_1_div.hw,
		[CLKID_VAPB_1]			= &axg_vapb_1.hw,
		[CLKID_VAPB_SEL]		= &axg_vapb_sel.hw,
		[CLKID_VAPB]			= &axg_vapb.hw,
		[CLKID_SPICC_MUX]		= &axg_spicc_mux.hw,
		[CLKID_SPICC_DIV]		= &axg_spicc_div.hw,
		[CLKID_SPICC]			= &axg_spicc.hw,
		[CLKID_DSI_MEAS_MUX]		= &axg_dsi_meas_mux.hw,
		[CLKID_DSI_MEAS_DIV]		= &axg_dsi_meas_div.hw,
		[CLKID_DSI_MEAS]		= &axg_dsi_meas.hw,
		[NR_CLKS]			= NULL,
	},
	.num = NR_CLKS,
};

/* Convenience table to populate regmap in .probe */
static struct clk_regmap *const axg_clk_regmaps[] = {
	&axg_clk81,
	&axg_clk81_ddr,
	&axg_clk81_audio_locker,
	&axg_clk81_mipi_dsi_host,
	&axg_clk81_isa,
	&axg_clk81_pl301,
	&axg_clk81_periphs,
	&axg_clk81_spicc_0,
	&axg_clk81_i2c,
	&axg_clk81_rng0,
	&axg_clk81_uart0,
	&axg_clk81_mipi_dsi_phy,
	&axg_clk81_spicc_1,
	&axg_clk81_pcie_a,
	&axg_clk81_pcie_b,
	&axg_clk81_hiu_reg,
	&axg_clk81_assist_misc,
	&axg_clk81_emmc_b,
	&axg_clk81_emmc_c,
	&axg_clk81_dma,
	&axg_clk81_spi,
	&axg_clk81_audio,
	&axg_clk81_eth_core,
	&axg_clk81_uart1,
	&axg_clk81_g2d,
	&axg_clk81_usb0,
	&axg_clk81_usb1,
	&axg_clk81_reset,
	&axg_clk81_usb_general,
	&axg_clk81_ahb_arb0,
	&axg_clk81_efuse,
	&axg_clk81_boot_rom,
	&axg_clk81_ahb_data_bus,
	&axg_clk81_ahb_ctrl_bus,
	&axg_clk81_usb1_to_ddr,
	&axg_clk81_usb0_to_ddr,
	&axg_clk81_mmc_pclk,
	&axg_clk81_vpu_intr,
	&axg_clk81_sec_ahb_ahb3_bridge,
	&axg_clk81_gic,
	&axg_clk81_mipi_enable,
	&axg_clk81_mipi_bandgap,
	&axg_ao_media_cpu,
	&axg_ao_ahb_sram,
	&axg_ao_ahb_bus,
	&axg_ao_iface,
	&axg_ao_i2c,
	&axg_sd_emmc_b_clk0,
	&axg_sd_emmc_c_clk0,
	&axg_mpeg_clk_div,
	&axg_sd_emmc_b_clk0_div,
	&axg_sd_emmc_c_clk0_div,
	&axg_mpeg_clk_sel,
	&axg_sd_emmc_b_clk0_sel,
	&axg_sd_emmc_c_clk0_sel,
	&axg_mpll0,
	&axg_mpll1,
	&axg_mpll2,
	&axg_mpll3,
	&axg_mpll0_div,
	&axg_mpll1_div,
	&axg_mpll2_div,
	&axg_mpll3_div,
	&axg_fixed_pll,
	&axg_sys_pll,
	&axg_gp0_pll,
	&axg_hifi_pll,
	&axg_mpll_prediv,
	&axg_fclk_div2,
	&axg_fclk_div3,
	&axg_fclk_div4,
	&axg_fclk_div5,
	&axg_fclk_div7,
	&axg_pcie_pll_dco,
	&axg_pcie_pll_od,
	&axg_pcie_pll,
	&axg_pcie_mux,
	&axg_pcie_ref,
	&axg_pcie_cml_en0,
	&axg_pcie_cml_en1,
	&axg_gen_clk_sel,
	&axg_gen_clk_div,
	&axg_gen_clk,
	&axg_fixed_pll_dco,
	&axg_sys_pll_dco,
	&axg_gp0_pll_dco,
	&axg_hifi_pll_dco,
	&axg_pcie_pll_dco,
	&axg_pcie_pll_od,
	&axg_cpu_clk_premux0,
	&axg_cpu_clk_mux0_div,
	&axg_cpu_clk_postmux0,
	&axg_cpu_clk_premux1,
	&axg_cpu_clk_mux1_div,
	&axg_cpu_clk_postmux1,
	&axg_cpu_clk_dyn,
	&axg_cpu_clk,
	&axg_vpu_0_sel,
	&axg_vpu_0_div,
	&axg_vpu_0,
	&axg_vpu_1_sel,
	&axg_vpu_1_div,
	&axg_vpu_1,
	&axg_vpu,
	&axg_vapb_0_sel,
	&axg_vapb_0_div,
	&axg_vapb_0,
	&axg_vapb_1_sel,
	&axg_vapb_1_div,
	&axg_vapb_1,
	&axg_vapb_sel,
	&axg_vapb,
	&axg_spicc_mux,
	&axg_spicc_div,
	&axg_spicc,
	&axg_dsi_meas_mux,
	&axg_dsi_meas_div,
	&axg_dsi_meas
};

static int meson_axg_dvfs_setup(struct platform_device *pdev)
{
	struct clk *notifier_clk;
	int ret;

	/* Setup clock notifier for cpu_clk_postmux0 */
	axg_cpu_clk_postmux0_nb_data.fclk_div2 = &axg_fclk_div2.hw;
	notifier_clk = axg_cpu_clk_postmux0.hw.clk;
	ret = clk_notifier_register(notifier_clk,
				    &axg_cpu_clk_postmux0_nb_data.nb);
	if (ret) {
		dev_err(&pdev->dev, "failed to register the cpu_clk_postmux0 notifier\n");
		return ret;
	}

	/* Setup clock notifier for sys_pll */
	notifier_clk = axg_sys_pll.hw.clk;
	ret = clk_notifier_register(notifier_clk, &axg_sys_pll_nb_data.nb);
	if (ret) {
		dev_err(&pdev->dev, "failed to register the sys_pll notifier\n");
		return ret;
	}

	return 0;
}

struct meson_axg_data {
	const struct meson_eeclkc_data eeclkc_data;
	int (*dvfs_setup)(struct platform_device *pdev);
};

static int meson_axg_probe(struct platform_device *pdev)
{
	const struct meson_eeclkc_data *eeclkc_data;
	const struct meson_axg_data *axg_data;
	int ret;

	eeclkc_data = of_device_get_match_data(&pdev->dev);
	if (!eeclkc_data)
		return -EINVAL;

	ret = meson_eeclkc_probe(pdev);
	if (ret)
		return ret;

	axg_data = container_of(eeclkc_data, struct meson_axg_data,
				 eeclkc_data);

	if (axg_data->dvfs_setup)
		return axg_data->dvfs_setup(pdev);

	return 0;
}

static const struct meson_axg_data axg_clkc_data = {
	.eeclkc_data = {
		.regmap_clks = axg_clk_regmaps,
		.regmap_clk_num = ARRAY_SIZE(axg_clk_regmaps),
		.hw_onecell_data = &axg_hw_onecell_data,
		//.init_regs = axg_init_regs,
		//.init_count = ARRAY_SIZE(axg_init_regs),
	},
	.dvfs_setup = meson_axg_dvfs_setup,
};

static const struct of_device_id clkc_match_table[] = {
	{ .compatible = "amlogic,axg-clkc", .data = &axg_clkc_data.eeclkc_data },
	{}
};

static struct platform_driver axg_driver = {
	.probe		= meson_axg_probe,
	.driver		= {
		.name	= "axg-clkc",
		.of_match_table = clkc_match_table,
	},
};

#ifndef CONFIG_AMLOGIC_MODIFY
builtin_platform_driver(axg_driver);
#else
static int axg_clkc_init(void)
{
	return platform_driver_register(&axg_driver);
}
arch_initcall_sync(axg_clkc_init);
#endif
