/*
 * drivers/amlogic/clk/clk-pll.c
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


#include <linux/errno.h>
#include <linux/hrtimer.h>
#include <linux/delay.h>
#include <linux/clk-private.h>
#include "clk.h"
#include "clk-pll.h"

#define to_clk_pll(_hw) container_of(_hw, struct amlogic_clk_pll, hw)
#define to_clk(_hw) container_of(_hw, struct clk, hw)
#define to_aml_pll(_hw) container_of(_hw, struct amlogic_pll_clock, hw)

static struct amlogic_pll_rate_table *aml_get_pll_settings(
		struct amlogic_pll_clock *pll, unsigned long rate)
{
	struct amlogic_pll_rate_table  *rate_table = pll->rate_table;
	int i;

	for (i = 0; i < pll->rate_table_cnt; i++) {
		if (rate == rate_table[i].rate)
			return &rate_table[i];
	}

	return NULL;
}

static long amlogic_pll_round_rate(struct clk_hw *hw,
	    unsigned long drate, unsigned long *prate)
{
	struct amlogic_clk_pll *pll = to_clk_pll(hw);
	const struct amlogic_pll_rate_table *rate_table = pll->rate_table;
	int i;
	if (drate < rate_table[0].rate)
		return rate_table[0].rate;

	/* Assumming rate_table is in descending order */
	for (i = 1; i < pll->rate_count; i++) {
		if (drate <= rate_table[i].rate)
			return rate_table[i-1].rate;
	}

	/* return maximum supported value */
	return rate_table[i-1].rate;
}
static long aml_pll_round_rate(struct clk_hw *hw,
	    unsigned long drate, unsigned long *prate)
{
	struct amlogic_pll_clock *aml_pll = to_aml_pll(hw);
	const struct amlogic_pll_rate_table *rate_table = aml_pll->rate_table;
	int i;
	/* Assumming rate_table is in descending order */
	for (i = 0; i < aml_pll->rate_table_cnt; i++) {
		if (drate >= rate_table[i].rate)
			return rate_table[i].rate;
	}
	*prate = clk_get_parent(hw->clk)->rate;
	/* return minimum supported value */
	return rate_table[i - 1].rate;
}

/*
 * PLL1500 Clock Type
 */
static unsigned long amlogic_pll1500_recalc_rate(struct clk_hw *hw,
		unsigned long parent_rate)
{
	pr_debug("%s %s %d\n", __FILE__, __func__, __LINE__);
	pr_debug("To Do!!!!\n");
	return 1;
}

static int amlogic_pll1500_set_rate(struct clk_hw *hw, unsigned long drate,
		unsigned long prate)
{
	pr_debug("%s %s %d\n", __FILE__, __func__, __LINE__);
	pr_debug("To Do!!!!\n");
	return 1;
}

static const struct clk_ops amlogic_pll1500_clk_ops = {
	.recalc_rate = amlogic_pll1500_recalc_rate,
	.round_rate = amlogic_pll_round_rate,
	.set_rate = amlogic_pll1500_set_rate,
};

static const struct clk_ops amlogic_pll1500_clk_min_ops = {
	.recalc_rate = amlogic_pll1500_recalc_rate,
};



/*
 * PLL2550 Clock Type
 */
#define PLL2550_OD_SHIFT 0xf
#define PLL2550_OD_MASK 0x3
#define PLL2550_M_SHIFT 0x0
#define PLL2550_M_MASK 0x1ff
#define PLL2550_N_SHIFT 0x9
#define PLL2550_N_MASK 0x1f

static unsigned long amlogic_pll2550_recalc_rate(struct clk_hw *hw,
		unsigned long parent_rate)
{
	struct amlogic_clk_pll *pll = to_clk_pll(hw);
	u32 od, M, N, pll_con;
	u32 fvco = parent_rate;

	pll_con = readl(pll->con_reg);

	od = (pll_con >> PLL2550_OD_SHIFT) & PLL2550_OD_MASK;
	M = (pll_con >> PLL2550_M_SHIFT) & PLL2550_M_MASK;
	N = (pll_con >> PLL2550_N_SHIFT) & PLL2550_N_MASK;
	/*pr_debug("od: %d  M: %d  N  %d\n", od, M, N);*/

	fvco /= 1000000;
	fvco = fvco * M / N;
	fvco = (fvco >> od);
	fvco *= 1000000;
 /* pr_debug("%s: fvco is %u\n", __func__, fvco);*/
	return (unsigned long)fvco;
}

static int amlogic_pll2550_set_rate(struct clk_hw *hw, unsigned long drate,
		unsigned long prate)
{

	struct amlogic_clk_pll *pll = to_clk_pll(hw);
	if (drate == 2000000000) {
		/*fixed pll = xtal * M(0:8) * OD_FB(4) /N(9:13) /OD(16:17)
		//M: 0~511  OD_FB:0~1 + 1, N:0~32 + 1 OD:0~3 + 1
		//recommend this pll is fixed as 2G. */
		unsigned long xtal = 24000000;

		unsigned cntl = readl(pll->con_reg);
		unsigned m = cntl&0x1FF;
		unsigned n = ((cntl>>9)&0x1F);
		unsigned od = ((cntl >> 16)&3) + 1;
		unsigned od_fb = ((readl(pll->con_reg + 4*4)>>4)&1) + 1;
		unsigned long rate;
		if (prate)
			xtal = prate;
		xtal /= 1000000;
		rate = xtal * m * od_fb;
		rate /= n;
		rate /= od;
		rate *= 1000000;
		if (drate != rate) {
			/*aml_set_reg32_mask(pll,(1<<29));*/
			writel(((readl(pll->con_reg)) | (1<<29)), pll->con_reg);
			writel(M8_MPLL_CNTL_2, pll->con_reg + 1*4);
			writel(M8_MPLL_CNTL_3, pll->con_reg + 2*4);
			writel(M8_MPLL_CNTL_4, pll->con_reg + 3*4);
			writel(M8_MPLL_CNTL_5, pll->con_reg + 4*4);
			writel(M8_MPLL_CNTL_6, pll->con_reg + 5*4);
			writel(M8_MPLL_CNTL_7, pll->con_reg + 6*4);
			writel(M8_MPLL_CNTL_8, pll->con_reg + 7*4);
			writel(M8_MPLL_CNTL_9, pll->con_reg + 8*4);
			writel(M8_MPLL_CNTL, pll->con_reg);
			/*M8_PLL_WAIT_FOR_LOCK(P_HHI_MPLL_CNTL);*/
			do {
				udelay(1000);
			} while ((readl(pll->con_reg)&0x80000000) == 0);
		}
	} else
		return -1;
	return 0;
}

static const struct clk_ops amlogic_pll2550_clk_ops = {
	.recalc_rate = amlogic_pll2550_recalc_rate,
	.round_rate = amlogic_pll_round_rate,
	.set_rate = amlogic_pll2550_set_rate,
};

static const struct clk_ops amlogic_pll2550_clk_min_ops = {
	.recalc_rate = amlogic_pll2550_recalc_rate,
};


/*
 * PLL3000 Clock Type
 */


static unsigned long aml_pll_recalc_rate(struct clk_hw *hw,
		unsigned long parent_rate)
{
	struct amlogic_pll_clock *aml_pll = to_aml_pll(hw);
	u32 od, M, N, pll_con;
	u32 fvco = parent_rate;
	pll_con = readl(aml_pll->pll_ctrl->con_reg);
	if (aml_pll->pll_recalc_rate)
		return aml_pll->pll_recalc_rate(aml_pll, parent_rate);
	od = (pll_con >> aml_pll->pll_conf->od_shift) &
			aml_pll->pll_conf->od_mask;
	M = (pll_con >> aml_pll->pll_conf->m_shift) &
			aml_pll->pll_conf->m_mask;
	N = (pll_con >> aml_pll->pll_conf->n_shift) &
			aml_pll->pll_conf->n_mask;
	pr_debug("od: %d  M: %d  N  %d\n", od, M, N);

	fvco /= 1000000;
	fvco = fvco * M / N;
	fvco = (fvco >> od);
	fvco *= 1000000;
	pr_debug("%s: fvco is %u\n", __func__, fvco);
	return (unsigned long)fvco;
}

static int aml_pll_set_rate(struct clk_hw *hw, unsigned long drate,
		unsigned long prate)
{
	struct amlogic_pll_clock *aml_pll = to_aml_pll(hw);
	struct amlogic_pll_rate_table *rate;
	pr_info("drate=%lu,prate=%ld\n", drate, prate);
	rate = aml_get_pll_settings(aml_pll, drate);
	if (!rate) {
		pr_err("%s: Invalid rate : %lu for pll clk %s\n", __func__,
			drate, __clk_get_name(hw->clk));
		return -EINVAL;
	}

	aml_pll->waite_pll_lock(aml_pll, rate);
	return 1;
}

static const struct clk_ops amlogic_pll3000_clk_ops = {
	.recalc_rate = aml_pll_recalc_rate,
	.round_rate = aml_pll_round_rate,
	.set_rate = aml_pll_set_rate,
};

static const struct clk_ops amlogic_pll3000_clk_min_ops = {
	.recalc_rate = aml_pll_recalc_rate,
};

static void __init _amlogic_clk_register_pll(struct amlogic_pll_clock *pll_clk,
						void __iomem *base)
{
	struct amlogic_clk_pll *pll;
	struct clk *clk;
	struct clk_init_data init;
	int ret, len;

	pll = kzalloc(sizeof(*pll), GFP_KERNEL);
	if (!pll) {
		pr_err("%s: could not allocate pll clk %s\n",
			__func__, pll_clk->name);
		return;
	}

	init.name = pll_clk->name;
	init.flags = pll_clk->flags;
	init.parent_names = &pll_clk->parent_name;
	init.num_parents = 1;

	if (pll_clk->rate_table) {
		/* find count of rates in rate_table */
		for (len = 0; pll_clk->rate_table[len].rate != 0; )
			len++;

		pll->rate_count = len;
		pll->rate_table = kmemdup(pll_clk->rate_table,
					pll->rate_count *
					sizeof(struct amlogic_pll_rate_table),
					GFP_KERNEL);
		WARN(!pll->rate_table,
			"%s: could not allocate rate table for %s\n",
			__func__, pll_clk->name);
		if (!pll->rate_table)
			pr_debug("%s: could not allocate rate table for %s\n",
			__func__, pll_clk->name);
	}

	switch (pll_clk->type) {
	case pll_1500:
		if (!pll->rate_table)
			init.ops = &amlogic_pll1500_clk_min_ops;
		else
			init.ops = &amlogic_pll1500_clk_ops;
		break;

	case pll_2550:
		if (!pll->rate_table)
			init.ops = &amlogic_pll2550_clk_min_ops;
		else
			init.ops = &amlogic_pll2550_clk_ops;
		break;

	case pll_3000_lvds:
			pll_clk->hw.init = &init;
			pll_clk->pll_ctrl->con_reg =
				base+pll_clk->pll_ctrl->con_reg_offset;
			pll_clk->pll_ctrl->lock_reg =
				base+pll_clk->pll_ctrl->lock_reg_offset;
		if (!pll->rate_table)
			init.ops = &amlogic_pll3000_clk_min_ops;
		else
			init.ops = &amlogic_pll3000_clk_ops;
		break;

	default:
		pr_warn("%s: Unknown pll type for pll clk %s\n",
			__func__, pll_clk->name);
	}

	pll->hw.init = &init;
	pll->type = pll_clk->type;
	pll->con_reg = base + pll_clk->con_offset;

	if (pll_clk->type == pll_3000_lvds)
		clk = clk_register(NULL, &pll_clk->hw);
	else
		clk = clk_register(NULL, &pll->hw);
	if (IS_ERR(clk)) {
		pr_err("%s: failed to register pll clock %s : %ld\n",
			__func__, pll_clk->name, PTR_ERR(clk));
		kfree(pll);
		return;
	}

	amlogic_clk_add_lookup(clk, pll_clk->id);

	if (!pll_clk->alias)
		return;

	ret = clk_register_clkdev(clk, pll_clk->alias, pll_clk->dev_name);
	if (ret)
		pr_err("%s: failed to register lookup for %s : %d",
			__func__, pll_clk->name, ret);
}

void __init amlogic_clk_register_pll(struct amlogic_pll_clock *pll_list,
				unsigned int nr_pll, void __iomem *base)
{
	int cnt;

	for (cnt = 0; cnt < nr_pll; cnt++)
		_amlogic_clk_register_pll(&pll_list[cnt], base);
}
