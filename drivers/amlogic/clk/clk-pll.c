/*
 * Copyright (c) 2014 Amlogic, Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This file contains the utility functions to register the pll clocks.
*/

#include <linux/errno.h>
#include <linux/hrtimer.h>
#include <linux/delay.h>
#include <linux/clk-private.h>
#include "clk.h"
#include "clk-pll.h"

#define to_clk_pll(_hw) container_of(_hw, struct amlogic_clk_pll, hw)
#define to_clk(_hw) container_of(_hw, struct clk, hw)

static struct amlogic_pll_rate_table *amlogic_get_pll_settings(
				struct amlogic_clk_pll *pll, unsigned long rate)
{
	struct amlogic_pll_rate_table  *rate_table = pll->rate_table;
	int i;

	for (i = 0; i < pll->rate_count; i++) {
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

	/* Assumming rate_table is in descending order */
	for (i = 0; i < pll->rate_count; i++) {
		if (drate >= rate_table[i].rate)
			return rate_table[i].rate;
	}

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
 * PLL2500 Clock Type
 */
#define PLL2500_OD_SHIFT 0x10
#define PLL2500_OD_MASK 0x3
#define PLL2500_M_SHIFT 0x0
#define PLL2500_M_MASK ((1<<9)-1)
#define PLL2500_N_SHIFT 0x9
#define PLL2500_N_MASK 0x1f

static void aml_pll2500_set_reg32_bits(uint32_t _reg, const uint32_t _value,
		const uint32_t _start, const uint32_t _len)
{
	writel(((readl((void *)_reg) & ~(((1L << (_len))-1) << (_start)))
		| ((unsigned)((_value)&((1L<<(_len))-1)) << (_start))),
		(void *)_reg);
}

static inline void udelay_scaled(unsigned long usecs, unsigned int oldMHz,
				 unsigned int newMHz)
{
	if (arm_delay_ops.ticks_per_jiffy)
		udelay(usecs);
	else
		udelay(usecs * newMHz / oldMHz);
}

static unsigned long amlogic_pll2500_recalc_rate(struct clk_hw *hw,
		unsigned long parent_rate)
{
	struct amlogic_clk_pll *pll = to_clk_pll(hw);
	u32 od, M, N, ext_div_n, pll_con;
	u32 fvco = parent_rate;

  /*using measure sys div3 to get sys pll clock. (25)*/
	pll_con = readl(pll->con_reg);
	ext_div_n = ((readl(pll->con_reg + ((HHI_SYS_CPU_CLK_CNTL1 -
		HHI_SYS_PLL_CNTL)*4))) >> 20) & 0x3ff;
	/*pr_debug("%s: pll_con is 0x%p\n", __func__, (void *)pll_con);
	pr_debug("HHI_SYS_CPU_CLK_CNTL1 : 0x%x\n",readl(pll->con_reg +
		((HHI_SYS_CPU_CLK_CNTL1 - HHI_SYS_PLL_CNTL)*4)));*/
	od = (pll_con >> PLL2500_OD_SHIFT) & PLL2500_OD_MASK;
	M = (pll_con >> PLL2500_M_SHIFT) & PLL2500_M_MASK;
	N = (pll_con >> PLL2500_N_SHIFT) & PLL2500_N_MASK;
	/*pr_debug("od: %d  M: %d  N  %d\n", od, M, N);*/
	fvco /= 1000000;
	fvco = fvco * M / N;
	fvco = fvco>>od;
	/*pr_debug("%s: HHI_SYS_CPU_CLK_CNTL : 0x%x\n",
	 __func__,readl(pll->con_reg
	 + ((HHI_SYS_CPU_CLK_CNTL - HHI_SYS_PLL_CNTL)*4)));*/
	if (ext_div_n)
		fvco /= (ext_div_n + 1)*2;
	fvco *= 1000000;
  /*pr_debug("%s: fvco is %u\n", __func__, fvco);*/
	return (unsigned long)fvco;
}

static int amlogic_pll2500_set_rate(struct clk_hw *hw, unsigned long drate,
		unsigned long prate)
{
	/* To Do */
	struct amlogic_clk_pll *pll = to_clk_pll(hw);
	struct amlogic_pll_rate_table *rate;
	u32 old_cntl, old_od, old_N, old_M, old_ext_div_n, new_cntl;
	u32 old_rate, tmp, loop;
	static int only_once;
	unsigned long irq_flags;

	old_cntl = readl(pll->con_reg);
	tmp = old_cntl;
	old_rate = clk_get_rate(__clk_lookup("sys_pll"));

	irq_flags = arch_local_irq_save();
	preempt_disable();

	/*CPU switch to xtal*/
	tmp = readl(pll->con_reg + (HHI_SYS_CPU_CLK_CNTL - HHI_SYS_PLL_CNTL)*4);
	writel((tmp&~(1<<7)), (pll->con_reg + (HHI_SYS_CPU_CLK_CNTL
		- HHI_SYS_PLL_CNTL)*4));
	/*pr_debug("HHI_SYS_CPU_CLK_CNTL is 0x%x\n", readl(pll->con_reg
		+ (HHI_SYS_CPU_CLK_CNTL - HHI_SYS_PLL_CNTL)*4));*/
	if (old_rate <= drate) {
		/* when increasing frequency, lpj has already been adjusted */
		udelay_scaled(10, drate / 1000000, 24 /*clk_get_rate_xtal*/);
	} else {
		/* when decreasing frequency, lpj has not yet been adjusted */
		udelay_scaled(10, old_rate / 1000000, 24 /*clk_get_rate_xtal*/);
	}

	/*pr_debug("%s: drate is %lu\n",__func__,drate);*/
	drate /= 1000000;
	if (drate < pll->rate_table[0].rate)
		drate = (pll->rate_table[0].rate) * 1000000;
	if (drate > pll->rate_table[pll->rate_count].rate)
		drate = (pll->rate_table[pll->rate_count].rate) * 1000000;

	/* Get required rate settings from table */
	rate = amlogic_get_pll_settings(pll, drate);
	if (!rate) {
		pr_err("%s: Invalid rate : %lu for pll clk %s\n", __func__,
			drate, __clk_get_name(hw->clk));
		return -EINVAL;
	}

	old_od = (old_cntl >> PLL2500_OD_SHIFT) & PLL2500_OD_MASK;
	old_M = (old_cntl >> PLL2500_M_SHIFT) & PLL2500_M_MASK;
	old_N = (old_cntl >> PLL2500_N_SHIFT) & PLL2500_N_MASK;
	old_ext_div_n = (readl(pll->con_reg + (HHI_SYS_CPU_CLK_CNTL1
		- HHI_SYS_PLL_CNTL)*4)>>20) & 0x3ff;
	pr_debug("%s: old M: %u old N: %u old od: %u old ext_div_n: %u\n",
	  __func__, old_M, old_N, old_od, old_ext_div_n);
	pr_debug("%s: new M: %u new N: %u new od: %u new ext_div_n: %u\n",
	  __func__, rate->m, rate->n, rate->od, rate->ext_div_n);

	if ((rate->m != old_M) || (rate->n != old_N) || (rate->od != old_od)
		|| (rate->ext_div_n != old_ext_div_n)) {
SETPLL:
		aml_pll2500_set_reg32_bits((uint32_t)(pll->con_reg
			+ (HHI_SYS_CPU_CLK_CNTL1 - HHI_SYS_PLL_CNTL)*4),
			rate->ext_div_n, 20, 10);

		if (rate->ext_div_n) {
			aml_pll2500_set_reg32_bits((uint32_t)(pll->con_reg
			+ (HHI_SYS_CPU_CLK_CNTL - HHI_SYS_PLL_CNTL)*4),
			3, 2, 2);
		} else {
			aml_pll2500_set_reg32_bits((uint32_t)(pll->con_reg
			+ (HHI_SYS_CPU_CLK_CNTL - HHI_SYS_PLL_CNTL)*4),
			0, 2, 2);
		}

		tmp = old_cntl;
	  tmp &= ~(0x33fff); /*M:bit0-8 N:bit9-13 OD:bit16-17*/
	  tmp |= (rate->m)<<PLL2500_M_SHIFT;
	  tmp |= (rate->n)<<PLL2500_N_SHIFT;
	  tmp |= (rate->od)<<PLL2500_OD_SHIFT;
	  tmp &= ~(1<<31); /*unlock*/
	  tmp &= ~(0xff<<20); /*SS_CLK SS_AMP*/

		writel(((tmp | (1<<29))), pll->con_reg);

		if (only_once == 99) {
			only_once = 1;
			writel(M8_SYS_PLL_CNTL_2, pll->con_reg + 1*4);
			writel(M8_SYS_PLL_CNTL_3, pll->con_reg + 2*4);
			writel(M8_SYS_PLL_CNTL_4, pll->con_reg + 3*4);
			writel(M8_SYS_PLL_CNTL_5, pll->con_reg + 4*4);
	  }

	  aml_pll2500_set_reg32_bits((uint32_t)(pll->con_reg + 1*4),
		rate->afc_dsel_bp_in, 12, 1);
	  aml_pll2500_set_reg32_bits((uint32_t)(pll->con_reg + 1*4),
		rate->afc_dsel_bp_en, 13, 1);

	  writel(tmp, pll->con_reg);

	  udelay_scaled(100, drate / 1000000, 24 /*clk_get_rate_xtal*/);

	  new_cntl = readl(pll->con_reg);

	  if ((new_cntl & (1<<31)) == 0) {
			if (loop++ >= 10) {
				loop = 0;
				pr_err("CPU freq: %ld MHz, syspll (%x) can't lock:\n",
					drate/1000000, new_cntl);
				pr_err("  [10c0..10c4]%08x, %08x, %08x, %08x,"
					"%08x: [10a5]%08x, [10c7]%08x\n",
					readl(pll->con_reg),
					readl(pll->con_reg + 1*4),
					readl(pll->con_reg + 2*4),
					readl(pll->con_reg + 3*4),
					readl(pll->con_reg + 4*4),
					readl(pll->con_reg + (HHI_MPLL_CNTL6
						- HHI_SYS_PLL_CNTL)*4),
					readl(pll->con_reg + (HHI_DPLL_TOP_1
						- HHI_SYS_PLL_CNTL)*4)
				);
		    if (!((readl(pll->con_reg + (HHI_SYS_CPU_CLK_CNTL
						- HHI_DPLL_TOP_1)*4)) & 0x2)) {
					pr_err("  SYS_TDC_CAL_DONE triggered, disable TDC_CAL_EN\n");
		      aml_pll2500_set_reg32_bits((uint32_t)(pll->con_reg + 3*4),
						 0, 10, 1);
					pr_err("  HHI_SYS_PLL_CNTL4: %08x\n",
						readl(pll->con_reg + 3*4));
				} else{
		      rate->afc_dsel_bp_in = !rate->afc_dsel_bp_in;
					pr_err("  INV afc_dsel_bp_in, new afc_dsel_bp_in=%08x\n",
						rate->afc_dsel_bp_in);
		      for (tmp = 0; tmp < pll->rate_count; tmp++) {
				    if (drate == pll->rate_table[tmp].rate)
					    /*write back afc_dsel_bp_in bit.*/
					    pll->rate_table[tmp].afc_dsel_bp_in
					      = rate->afc_dsel_bp_in;
					}
				}
				pr_err("  Try again!\n");
			}
			goto SETPLL;
		};
	} else{
		pr_debug("no change for sys pll!\n");
	}

  /*cpu switch to sys pll*/
	readl(pll->con_reg + (HHI_SYS_CPU_CLK_CNTL - HHI_SYS_PLL_CNTL)*4);
	writel((tmp&(1<<7)), (pll->con_reg + (HHI_SYS_CPU_CLK_CNTL
		- HHI_SYS_PLL_CNTL)*4));
	if (old_rate <= drate) {
			/*when increasing frequency,
			lpj has already been adjusted*/
			udelay(100);
	} else {
			/*when decreasing frequency,
			lpj has not yet been adjusted*/
			udelay_scaled(100, old_rate / 1000000, drate / 1000000);
	}
	preempt_enable();
	arch_local_irq_restore(irq_flags);
	pr_debug("cur rate is %lu\n", clk_get_rate(__clk_lookup("sys_pll")));

	return 0;
}

static const struct clk_ops amlogic_pll2500_clk_ops = {
	.recalc_rate = amlogic_pll2500_recalc_rate,
	.round_rate = amlogic_pll_round_rate,
	.set_rate = amlogic_pll2500_set_rate,
};

static const struct clk_ops amlogic_pll2500_clk_min_ops = {
	.recalc_rate = amlogic_pll2500_recalc_rate,
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

static unsigned long amlogic_pll3000_recalc_rate(struct clk_hw *hw,
		unsigned long parent_rate)
{
	pr_debug("%s %s %d\n", __FILE__, __func__, __LINE__);
	pr_debug("To Do!!!!\n");
	return 1;
}

static int amlogic_pll3000_set_rate(struct clk_hw *hw, unsigned long drate,
		unsigned long prate)
{
	pr_debug("%s %s %d\n", __FILE__, __func__, __LINE__);
	pr_debug("To Do!!!!\n");
	return 1;
}

static const struct clk_ops amlogic_pll3000_clk_ops = {
	.recalc_rate = amlogic_pll3000_recalc_rate,
	.round_rate = amlogic_pll_round_rate,
	.set_rate = amlogic_pll3000_set_rate,
};

static const struct clk_ops amlogic_pll3000_clk_min_ops = {
	.recalc_rate = amlogic_pll3000_recalc_rate,
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

	case pll_2500:
		if (!pll->rate_table)
			init.ops = &amlogic_pll2500_clk_min_ops;
		else
		  init.ops = &amlogic_pll2500_clk_ops;
		break;

	case pll_2550:
		if (!pll->rate_table)
			init.ops = &amlogic_pll2550_clk_min_ops;
		else
			init.ops = &amlogic_pll2550_clk_ops;
		break;

	case pll_3000:
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
