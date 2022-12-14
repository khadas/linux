/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 BayLibre, SAS.
 * Author: Jerome Brunet <jbrunet@baylibre.com>
 */

#ifndef __MESON_CLK_PLL_H
#define __MESON_CLK_PLL_H

#include <linux/clk-provider.h>
#include <linux/regmap.h>
#include "parm.h"

struct pll_mult_range {
	unsigned int	min;
	unsigned int	max;
};

#if defined CONFIG_AMLOGIC_MODIFY && defined CONFIG_ARM
struct pll_params_table {
	u16		m;
	u16		n;
	u16		od;
};

#define PLL_PARAMS(_m, _n, _od)						\
	{								\
		.m		= (_m),					\
		.n		= (_n),					\
		.od		= (_od),				\
	}
#else
struct pll_params_table {
	unsigned int	m;
	unsigned int	n;
};

#define PLL_PARAMS(_m, _n)						\
	{								\
		.m		= (_m),					\
		.n		= (_n),					\
	}
#endif

#define CLK_MESON_PLL_ROUND_CLOSEST	BIT(0)

struct meson_clk_pll_data {
	struct parm en;
	struct parm m;
	struct parm n;
	struct parm frac;
	struct parm l;
	struct parm rst;
	/* for 32bit dco overflow */
	struct parm od;
	/*for pcie*/
	struct parm pcie_hcsl;
	struct parm pcie_exen;
	const struct reg_sequence *init_regs;
	unsigned int init_count;
	const struct pll_params_table *table;
	const struct pll_mult_range *range;
	unsigned int smc_id;
	u8 flags;
	u8 secid_disable;
	u8 secid;
	u8 new_frac;
	bool ignore_init;
};

extern const struct clk_ops meson_clk_pll_ro_ops;
extern const struct clk_ops meson_clk_axg_pll_ro_ops;
extern const struct clk_ops meson_clk_pll_ops;
extern const struct clk_ops meson_clk_pcie_pll_ops;
extern const struct clk_ops meson_secure_pll_v2_ops;

#endif /* __MESON_CLK_PLL_H */
