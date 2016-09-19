/*
 * drivers/amlogic/clk/clk_mpll_clock.c
 *
 * Copyright (C) 2015 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
*/

#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/string.h>
#include <linux/log2.h>
#include "mpll_clk.h"
#include "clk.h"

#undef pr_fmt
#define pr_fmt(fmt) "gxbb_mpll_clk: " fmt

#define sdm_mask(d)		((1 << (d->sdm_in_width)) - 1)
#define n_mask(d)			((1 << (d->n_in_width)) - 1)

#define to_clk_pll(_hw) container_of(_hw, struct mpll_clk, hw)
#define MAX_RATE	500000000
#define MIN_RATE	5000000
#define N2_MAX		127
#define N2_MIN		4
#define SDM_MAX	16384
#define ERROR		10000000
#define SDM_EN      15
#define EN_DDS      14

static int mpll_enable(struct clk_hw *hw)
{
	struct mpll_clk *mpll = to_clk_pll(hw);
	unsigned int val;

	if (strncmp(hw->clk->name, "mpll_clk_out0", 13) == 0) {
		val = readl(mpll->con_reg2);
		val |= 1 <<  mpll->SSEN_shift;
		writel(val, mpll->con_reg2);
	}

	val = readl(mpll->con_reg);
	val = val | (1 << SDM_EN) | (1 << EN_DDS);
	writel(val, mpll->con_reg);
	return 0;
}

static void mpll_disable(struct clk_hw *hw)
{
	struct mpll_clk *mpll = to_clk_pll(hw);
	unsigned int val;
	if (strncmp(hw->clk->name, "mpll_clk_out0", 13) == 0) {
		val = readl(mpll->con_reg2);
		val &= ~(1 <<  mpll->SSEN_shift);
		writel(val, mpll->con_reg2);
	}

	val = readl(mpll->con_reg);
	val = val & (~((1 << SDM_EN) | (1 << EN_DDS)));
	writel(val, mpll->con_reg);
}

static unsigned long mpll_recalc_rate(struct clk_hw *hw,
		unsigned long prate)
{
	struct mpll_clk *mpll = to_clk_pll(hw);
	unsigned long rate = 0;
	unsigned int val, sdm, n2;

	val = readl(mpll->con_reg);
	sdm = (val >> mpll->sdm_in_shift) & sdm_mask(mpll);
	n2 = (val >> mpll->n_in_shift) & n_mask(mpll);

	rate = (prate * SDM_MAX)/((SDM_MAX*mpll->n_in)+mpll->sdm_in);

	return rate;
}
static long mpll_round_rate(struct clk_hw *hw,
	    unsigned long drate, unsigned long *prate)
{
	unsigned int rate = 0;
	rate = drate;
	if (drate < MIN_RATE)
		rate = MIN_RATE;
	if (drate > MAX_RATE)
		rate = MAX_RATE;
	return rate;
}

static int mpll_set_rate(struct clk_hw *hw, unsigned long drate,
		unsigned long prate)
{
	struct mpll_clk *mpll = to_clk_pll(hw);
	u64 divider;
	unsigned long frac;
	uint val;
	divider = prate;
	frac = do_div(divider, drate);
	val = DIV_ROUND_UP(frac * SDM_MAX, drate);

	pr_debug("divider=%llu,frac=%lu,SDMval=%d\n",
						divider, frac, val);
	mpll->n_in = divider;
	mpll->sdm_in = val;

	val = readl(mpll->con_reg);
	val &= ~(sdm_mask(mpll) << mpll->sdm_in_shift);
	val |= mpll->sdm_in <<  mpll->sdm_in_shift;
	val &=  ~(n_mask(mpll) << mpll->n_in_shift);
	val |= mpll->n_in <<  mpll->n_in_shift;
	writel(val, mpll->con_reg);
	pr_debug("readl con_reg=%x\n", readl(mpll->con_reg));
	return 0;
}

static const struct clk_ops mpll_clk_ops = {
	.enable = mpll_enable,
	.disable = mpll_disable,
	.recalc_rate = mpll_recalc_rate,
	.round_rate = mpll_round_rate,
	.set_rate = mpll_set_rate,
};

void __init mpll_clk_register(void __iomem *base, struct mpll_clk_tab *tab)
{

	struct mpll_clk *pll;
	struct clk *clk;
	struct clk_init_data init;
	int ret;

	pll = kzalloc(sizeof(*pll), GFP_KERNEL);
	if (!pll) {
		pr_err("%s: could not allocate pll clk %s\n",
			__func__, tab->name);
		return;
	}

	pll->sdm_in_shift = 0;
	pll->sdm_in_width = 14;
	pll->n_in_shift = 16;
	pll->n_in_width = 9;
	pll->SSEN_shift = 25;

	init.name = tab->name;
	init.flags = tab->flags;
	init.parent_names = tab->parent_names;
	init.num_parents = 1;
	init.ops = &mpll_clk_ops;

	pll->hw.init = &init;
	pll->con_reg = base + tab->offset;
	pll->con_reg2 = base + tab->offset2;
	clk = clk_register(NULL, &pll->hw);
	if (IS_ERR(clk)) {
		pr_err("%s: failed to register pll clock %s : %ld\n",
			__func__, tab->name, PTR_ERR(clk));
		kfree(pll);
		return;
	}

	amlogic_clk_add_lookup(clk, tab->id);

	ret = clk_register_clkdev(clk, tab->name, NULL);
	if (ret)
		pr_err("%s: failed to register lookup for %s : %d",
			__func__, tab->name, ret);

	pr_info("register %s success done\n", tab->name);

}

void __init mpll_clk_init(void __iomem *base, struct mpll_clk_tab *tab,
			  unsigned int cnt)
{
	int i = 0;
	for (i = 0; i < cnt; i++, tab++)
		mpll_clk_register(base, tab);
}
