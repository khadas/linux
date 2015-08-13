/*
 * drivers/amlogic/clk/clk-sys.c
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

#undef pr_fmt
#define pr_fmt(fmt) "clk_sys: " fmt

/*cpu registers*/
#define  OFFSET(x) (x << 2)
#define HHI_SYS_CPU_CLK_CNTL1 OFFSET(0x57)
#define HHI_SYS_CPU_CLK_CNTL   OFFSET(0x67)
#define HHI_SYS_PLL_CNTL		OFFSET(0xc0)
#define HHI_SYS_PLL_CNTL2	OFFSET(0xc1)
#define HHI_SYS_PLL_CNTL3		OFFSET(0xc2)
#define HHI_SYS_PLL_CNTL4	OFFSET(0xc3)
#define HHI_SYS_PLL_CNTL5	OFFSET(0xc4)
#define HHI_DPLL_TOP_0		OFFSET(0xc6)
#define HHI_DPLL_TOP_1		OFFSET(0xc7)
#define HHI_MPLL_CNTL6		OFFSET(0xa5)


#define to_clk_pll(_hw) container_of(_hw, struct amlogic_clk_pll, hw)
#define to_clk(_hw) container_of(_hw, struct clk, hw)
#define to_aml_pll(_hw) container_of(_hw, struct amlogic_pll_clock, hw)
static int max_freq;

static  struct amlogic_pll_rate_table syspll_tbl[] = {
	PLL_2500_RATE(24, 56, 1, 2, 6, 0, 0), /* fvco 1344, / 4, /14 */
	PLL_2500_RATE(48, 64, 1, 2, 3, 0, 0), /* fvco 1536, / 4, / 8 */
	PLL_2500_RATE(72, 72, 1, 2, 2, 1, 1), /* fvco 1728, / 4, / 6 */
	PLL_2500_RATE(96, 64, 1, 2, 1, 0, 0), /* fvco 1536, / 4, / 4 */
	PLL_2500_RATE(120, 80, 1, 2, 1, 1, 1), /* fvco 1920, / 4, / 4 */
	PLL_2500_RATE(144, 96, 1, 2, 1, 0, 0), /* fvco 2304, / 4, / 4 */
	PLL_2500_RATE(168, 56, 1, 1, 1, 0, 0), /* fvco 1344, / 2, / 4 */
	PLL_2500_RATE(192, 64, 1, 1, 1, 0, 0), /* fvco 1536, / 2, / 4 */
	PLL_2500_RATE(216, 72, 1, 1, 1, 1, 1), /* fvco 1728, / 2, / 4 */
	PLL_2500_RATE(240, 80, 1, 1, 1, 1, 1), /* fvco 1920, / 2, / 4 */
	PLL_2500_RATE(264, 88, 1, 1, 1, 0, 0), /* fvco 2112, / 2, / 4 */
	PLL_2500_RATE(288, 96, 1, 1, 1, 0, 0), /* fvco 2304, / 2, / 4 */
	PLL_2500_RATE(312, 52, 1, 2, 0, 0, 0), /* fvco 1248, / 4, / 1 */
	PLL_2500_RATE(336, 56, 1, 2, 0, 0, 0), /* fvco 1344, / 4, / 1 */
	PLL_2500_RATE(360, 60, 1, 2, 0, 0, 0), /* fvco 1440, / 4, / 1 */
	PLL_2500_RATE(384, 64, 1, 2, 0, 0, 0), /* fvco 1536, / 4, / 1 */
	PLL_2500_RATE(408, 68, 1, 2, 0, 1, 0), /* fvco 1632, / 4, / 1 */
	PLL_2500_RATE(432, 72, 1, 2, 0, 1, 1), /* fvco 1728, / 4, / 1 */
	PLL_2500_RATE(456, 76, 1, 2, 0, 1, 1), /* fvco 1824, / 4, / 1 */
	PLL_2500_RATE(480, 80, 1, 2, 0, 1, 1), /* fvco 1920, / 4, / 1 */
	PLL_2500_RATE(504, 84, 1, 2, 0, 1, 1), /* fvco 2016, / 4, / 1 */
	PLL_2500_RATE(528, 88, 1, 2, 0, 0, 0), /* fvco 2112, / 4, / 1 */
	PLL_2500_RATE(552, 92, 1, 2, 0, 0, 0), /* fvco 2208, / 4, / 1 */
	PLL_2500_RATE(576, 96, 1, 2, 0, 0, 0), /* fvco 2304, / 4, / 1 */
	PLL_2500_RATE(600, 50, 1, 1, 0, 0, 0), /* fvco 1200, / 2, / 1 */
	PLL_2500_RATE(624, 52, 1, 1, 0, 0, 0), /* fvco 1248, / 2, / 1 */
	PLL_2500_RATE(648, 54, 1, 1, 0, 0, 0), /* fvco 1296, / 2, / 1 */
	PLL_2500_RATE(672, 56, 1, 1, 0, 0, 0), /* fvco 1344, / 2, / 1 */
	PLL_2500_RATE(696, 58, 1, 1, 0, 0, 0), /* fvco 1392, / 2, / 1 */
	PLL_2500_RATE(720, 60, 1, 1, 0, 0, 0), /* fvco 1440, / 2, / 1 */
	PLL_2500_RATE(744, 62, 1, 1, 0, 0, 0), /* fvco 1488, / 2, / 1 */
	PLL_2500_RATE(768, 64, 1, 1, 0, 0, 0), /* fvco 1536, / 2, / 1 */
	PLL_2500_RATE(792, 66, 1, 1, 0, 0, 0), /* fvco 1584, / 2, / 1 */
	PLL_2500_RATE(816, 68, 1, 1, 0, 1, 0), /* fvco 1632, / 2, / 1 */
	PLL_2500_RATE(840, 70, 1, 1, 0, 1, 0), /* fvco 1680, / 2, / 1 */
	PLL_2500_RATE(864, 72, 1, 1, 0, 1, 1), /* fvco 1728, / 2, / 1 */
	PLL_2500_RATE(888, 74, 1, 1, 0, 1, 1), /* fvco 1776, / 2, / 1 */
	PLL_2500_RATE(912, 76, 1, 1, 0, 1, 1), /* fvco 1824, / 2, / 1 */
	PLL_2500_RATE(936, 78, 1, 1, 0, 1, 1), /* fvco 1872, / 2, / 1 */
	PLL_2500_RATE(960, 80, 1, 1, 0, 1, 1), /* fvco 1920, / 2, / 1 */
	PLL_2500_RATE(984, 82, 1, 1, 0, 1, 1), /* fvco 1968, / 2, / 1 */
	PLL_2500_RATE(1008, 84, 1, 1, 0, 1, 1), /* fvco 2016, / 2, / 1 */
	PLL_2500_RATE(1032, 86, 1, 1, 0, 1, 1), /* fvco 2064, / 2, / 1 */
	PLL_2500_RATE(1056, 88, 1, 1, 0, 0, 0), /* fvco 2112, / 2, / 1 */
	PLL_2500_RATE(1080, 90, 1, 1, 0, 0, 0), /* fvco 2160, / 2, / 1 */
	PLL_2500_RATE(1104, 92, 1, 1, 0, 0, 0), /* fvco 2208, / 2, / 1 */
	PLL_2500_RATE(1128, 94, 1, 1, 0, 0, 0), /* fvco 2256, / 2, / 1 */
	PLL_2500_RATE(1152, 96, 1, 1, 0, 0, 0), /* fvco 2304, / 2, / 1 */
	PLL_2500_RATE(1176, 98, 1, 1, 0, 0, 0), /* fvco 2352, / 2, / 1 */
	PLL_2500_RATE(1200, 50, 1, 0, 0, 0, 0), /* fvco 1200, / 1, / 1 */
	PLL_2500_RATE(1224, 51, 1, 0, 0, 0, 0), /* fvco 1224, / 1, / 1 */
	PLL_2500_RATE(1248, 52, 1, 0, 0, 0, 0), /* fvco 1248, / 1, / 1 */
	PLL_2500_RATE(1272, 53, 1, 0, 0, 0, 0), /* fvco 1272, / 1, / 1 */
	PLL_2500_RATE(1296, 54, 1, 0, 0, 0, 0), /* fvco 1296, / 1, / 1 */
	PLL_2500_RATE(1320, 55, 1, 0, 0, 0, 0), /* fvco 1320, / 1, / 1 */
	PLL_2500_RATE(1344, 56, 1, 0, 0, 0, 0), /* fvco 1344, / 1, / 1 */
	PLL_2500_RATE(1368, 57, 1, 0, 0, 0, 0), /* fvco 1368, / 1, / 1 */
	PLL_2500_RATE(1392, 58, 1, 0, 0, 0, 0), /* fvco 1392, / 1, / 1 */
	PLL_2500_RATE(1416, 59, 1, 0, 0, 0, 0), /* fvco 1416, / 1, / 1 */
	PLL_2500_RATE(1440, 60, 1, 0, 0, 0, 0), /* fvco 1440, / 1, / 1 */
	PLL_2500_RATE(1464, 61, 1, 0, 0, 0, 0), /* fvco 1464, / 1, / 1 */
	PLL_2500_RATE(1488, 62, 1, 0, 0, 0, 0), /* fvco 1488, / 1, / 1 */
	PLL_2500_RATE(1512, 63, 1, 0, 0, 0, 0), /* fvco 1512, / 1, / 1 */
	PLL_2500_RATE(1536, 64, 1, 0, 0, 0, 0), /* fvco 1536, / 1, / 1 */
	PLL_2500_RATE(1560, 65, 1, 0, 0, 0, 0), /* fvco 1560, / 1, / 1 */
	PLL_2500_RATE(1584, 66, 1, 0, 0, 0, 0), /* fvco 1584, / 1, / 1 */
	PLL_2500_RATE(1608, 67, 1, 0, 0, 1, 0), /* fvco 1608, / 1, / 1 */
	PLL_2500_RATE(1632, 68, 1, 0, 0, 1, 0), /* fvco 1632, / 1, / 1 */
	PLL_2500_RATE(1656, 68, 1, 0, 0, 1, 0), /* fvco 1656, / 1, / 1 */
	PLL_2500_RATE(1680, 68, 1, 0, 0, 1, 0), /* fvco 1680, / 1, / 1 */
	PLL_2500_RATE(1704, 68, 1, 0, 0, 1, 0), /* fvco 1704, / 1, / 1 */
	PLL_2500_RATE(1728, 69, 1, 0, 0, 1, 0), /* fvco 1728, / 1, / 1 */
	PLL_2500_RATE(1752, 69, 1, 0, 0, 1, 0), /* fvco 1752, / 1, / 1 */
	PLL_2500_RATE(1776, 69, 1, 0, 0, 1, 0), /* fvco 1776, / 1, / 1 */
	PLL_2500_RATE(1800, 69, 1, 0, 0, 1, 0), /* fvco 1800, / 1, / 1 */
	PLL_2500_RATE(1824, 70, 1, 0, 0, 1, 0), /* fvco 1824, / 1, / 1 */
	PLL_2500_RATE(1848, 70, 1, 0, 0, 1, 0), /* fvco 1848, / 1, / 1 */
	PLL_2500_RATE(1872, 70, 1, 0, 0, 1, 0), /* fvco 1872, / 1, / 1 */
	PLL_2500_RATE(1896, 70, 1, 0, 0, 1, 0), /* fvco 1896, / 1, / 1 */
	PLL_2500_RATE(1920, 71, 1, 0, 0, 1, 0), /* fvco 1920, / 1, / 1 */
	PLL_2500_RATE(1944, 71, 1, 0, 0, 1, 0), /* fvco 1944, / 1, / 1 */
	PLL_2500_RATE(1968, 71, 1, 0, 0, 1, 0), /* fvco 1968, / 1, / 1 */
	PLL_2500_RATE(1992, 71, 1, 0, 0, 1, 0), /* fvco 1992, / 1, / 1 */
	PLL_2500_RATE(2016, 72, 1, 0, 0, 1, 0), /* fvco 2016, / 1, / 1 */
	PLL_2500_RATE(2040, 72, 1, 0, 0, 1, 0), /* fvco 2040, / 1, / 1 */
	PLL_2500_RATE(2064, 72, 1, 0, 0, 1, 0), /* fvco 2064, / 1, / 1 */
	PLL_2500_RATE(2088, 72, 1, 0, 0, 1, 0), /* fvco 2088, / 1, / 1 */
	PLL_2500_RATE(2112, 73, 1, 0, 0, 0, 0), /* fvco 2112, / 1, / 1 */
};

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
	if (drate < rate_table[0].rate)
		return rate_table[0].rate;

	/* Assumming rate_table is in descending order */
	for (i = 1; i < pll->rate_count; i++) {
		if (drate < rate_table[i].rate)
			return rate_table[i-1].rate;
	}

	/* return maximum supported value */
	return rate_table[i-1].rate;
}


/*
 * PLL2500 Clock Type
 */
#define OD_SHIFT 0x10
#define OD_MASK 0x3
#define M_SHIFT 0x0
#define M_MASK ((1<<9)-1)
#define N_SHIFT 0x9
#define N_MASK 0x1f

static void set_reg32_bits(void __iomem *_reg,
				       const uint32_t _value,
		const uint32_t _start, const uint32_t _len)
{
	writel(((readl(_reg) & ~(((1L << (_len))-1) << (_start)))
		| ((unsigned)((_value)&((1L<<(_len))-1)) << (_start))),
		(void *)_reg);
}

static inline void udelay_scaled(unsigned long usecs, unsigned int oldMHz,
				 unsigned int newMHz)
{
		udelay(usecs);
}

static unsigned long amlogic_pll2500_recalc_rate(struct clk_hw *hw,
		unsigned long parent_rate)
{
	struct amlogic_clk_pll *pll = to_clk_pll(hw);
	u32 od, M, N, ext_div_n, pll_con;
	u32 fvco = parent_rate;
	u32 mux;
	u32 val;
  /*using measure sys div3 to get sys pll clock. (25)*/
	pll_con = readl(pll->con_reg + HHI_SYS_PLL_CNTL);
	val = readl(pll->con_reg + HHI_SYS_CPU_CLK_CNTL);
	mux = (val >> 2) & 0x3;
	ext_div_n = (readl(pll->con_reg + HHI_SYS_CPU_CLK_CNTL1) >> 20)
								& 0x3ff;
	od = (pll_con >> OD_SHIFT) & OD_MASK;
	M = (pll_con >> M_SHIFT) & M_MASK;
	N = (pll_con >> N_SHIFT) & N_MASK;

	fvco /= 1000000;
	fvco = fvco * M / N;
	fvco = fvco>>od;
	if (mux == 0)
		fvco /= 1;
	if (mux == 1)
		fvco /= 2;
	if (mux == 2)
		fvco /= 3;
	if (mux == 3 && ext_div_n)
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
	u32 old_rate, tmp, loop = 0;
	static int only_once;
	unsigned long irq_flags;
	if (drate > max_freq)
		drate = max_freq;

	old_cntl = readl(pll->con_reg + HHI_SYS_PLL_CNTL);
	tmp = old_cntl;
	old_rate = clk_get_rate(__clk_lookup("sys_pll"));
	if (drate == old_rate)
		return 0;

	irq_flags = arch_local_irq_save();
	preempt_disable();

	/*CPU switch to xtal*/
	tmp = readl(pll->con_reg + HHI_SYS_CPU_CLK_CNTL);
	writel((tmp&~(1<<7)), pll->con_reg + HHI_SYS_CPU_CLK_CNTL);
	/*pr_debug("HHI_SYS_CPU_CLK_CNTL is 0x%x\n", readl(pll->con_reg
		+ (HHI_SYS_CPU_CLK_CNTL - HHI_SYS_PLL_CNTL)*4));*/
	if (old_rate <= drate) {
		/* when increasing frequency, lpj has already been adjusted */
		udelay_scaled(10, drate / 1000000, 24 /*clk_get_rate_xtal*/);
	} else {
		/* when decreasing frequency, lpj has not yet been adjusted */
		udelay_scaled(10, old_rate / 1000000, 24 /*clk_get_rate_xtal*/);
	}

	if (drate < pll->rate_table[0].rate)
		drate = (pll->rate_table[0].rate);

	if (drate > pll->rate_table[pll->rate_count-1].rate)
		drate = (pll->rate_table[pll->rate_count-1].rate);


	/* Get required rate settings from table */
	rate = amlogic_get_pll_settings(pll, drate);
	if (!rate) {
		pr_err("%s: Invalid rate : %lu for pll clk %s\n", __func__,
			drate, __clk_get_name(hw->clk));
		return -EINVAL;
	}
	old_od = (old_cntl >> OD_SHIFT) & OD_MASK;
	old_M = (old_cntl >> M_SHIFT) & M_MASK;
	old_N = (old_cntl >> N_SHIFT) & N_MASK;
	old_ext_div_n = (readl(pll->con_reg + HHI_SYS_CPU_CLK_CNTL1)>>20)
								& 0x3ff;

	if ((rate->m != old_M) || (rate->n != old_N) || (rate->od != old_od)
		|| (rate->ext_div_n != old_ext_div_n)) {
SETPLL:
		set_reg32_bits((pll->con_reg + HHI_SYS_CPU_CLK_CNTL1),
			rate->ext_div_n, 20, 10);
		if (rate->ext_div_n) {
			set_reg32_bits((pll->con_reg + HHI_SYS_CPU_CLK_CNTL),
			3, 2, 2);
		} else {
			set_reg32_bits((pll->con_reg + HHI_SYS_CPU_CLK_CNTL),
			0, 2, 2);
		}

		tmp = old_cntl;
		tmp &= ~(0x33fff); /*M:bit0-8 N:bit9-13 OD:bit16-17*/
		tmp |= (rate->m)<<M_SHIFT;
		tmp |= (rate->n)<<N_SHIFT;
		tmp |= (rate->od)<<OD_SHIFT;
		tmp &= ~(1<<31); /*unlock*/
		tmp &= ~(0xff<<20); /*SS_CLK SS_AMP*/

		writel(((tmp | (1<<29))), (pll->con_reg + HHI_SYS_PLL_CNTL));


		if (only_once == 99) {
			only_once = 1;
			writel(M8_SYS_PLL_CNTL_2,
			       pll->con_reg + HHI_SYS_PLL_CNTL2);
			writel(M8_SYS_PLL_CNTL_3,
			       pll->con_reg + HHI_SYS_PLL_CNTL3);
			writel(M8_SYS_PLL_CNTL_4,
			       pll->con_reg + HHI_SYS_PLL_CNTL4);
			writel(M8_SYS_PLL_CNTL_5,
			       pll->con_reg + HHI_SYS_PLL_CNTL5);
		}

	  set_reg32_bits((pll->con_reg + HHI_SYS_PLL_CNTL2),
		rate->afc_dsel_bp_in, 12, 1);
	  set_reg32_bits((pll->con_reg + HHI_SYS_PLL_CNTL2),
		rate->afc_dsel_bp_en, 13, 1);


	  writel(tmp, (pll->con_reg + HHI_SYS_PLL_CNTL));

	  udelay_scaled(100, drate / 1000000, 24 /*clk_get_rate_xtal*/);

	  new_cntl = readl(pll->con_reg + HHI_SYS_PLL_CNTL);


	  if ((new_cntl & (1<<31)) == 0) {
		if (loop++ >= 10) {
			loop = 0;
			pr_err("CPU freq: %ld MHz, syspll (%x) can't lock:\n",
					drate/1000000, new_cntl);
			pr_err("  [10c0..10c4]%08x, %08x, %08x, %08x,",
				readl(pll->con_reg),
				readl(pll->con_reg + HHI_SYS_PLL_CNTL2),
				readl(pll->con_reg + HHI_SYS_PLL_CNTL3),
				readl(pll->con_reg + HHI_SYS_PLL_CNTL4));
			pr_err("%08x: [10a5]%08x, [10c7]%08x\n",
					readl(pll->con_reg + HHI_SYS_PLL_CNTL5),
					readl(pll->con_reg + HHI_MPLL_CNTL6),
					readl(pll->con_reg + HHI_DPLL_TOP_1));
		if (!((readl(pll->con_reg + HHI_DPLL_TOP_1)) & 0x2)) {
			pr_err("SYS_TDC_CAL_DONE triggered,dis TDC_CAL_EN\n");
			set_reg32_bits((pll->con_reg + HHI_SYS_PLL_CNTL4),
						 0, 10, 1);
			pr_err("  HHI_SYS_PLL_CNTL4: %08x\n",
				readl(pll->con_reg + HHI_SYS_PLL_CNTL4));
		} else{
			rate->afc_dsel_bp_in = !rate->afc_dsel_bp_in;
			pr_err("  INV afc_dsel_bp_in,new afc_dsel_bp_in=%08x\n",
						rate->afc_dsel_bp_in);
			for (tmp = 0; tmp < pll->rate_count; tmp++) {
				if (drate == (pll->rate_table[tmp].rate))
					/*write back afc_dsel_bp_in bit.*/
					pll->rate_table[tmp].afc_dsel_bp_in
						= rate->afc_dsel_bp_in;
				}
			}
				pr_err("  Try again!\n");
		}
			goto SETPLL;
		}
	} else{
		pr_debug("no change for sys pll!\n");
	}


  /*cpu switch to sys pll*/
	tmp = readl(pll->con_reg + HHI_SYS_CPU_CLK_CNTL);
	writel((tmp|(1<<7)), pll->con_reg + HHI_SYS_CPU_CLK_CNTL);


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


static void __init _amlogic_clk_register_pll(struct amlogic_pll_clock *pll_clk,
						void __iomem *base, int cnt)
{
	struct amlogic_clk_pll *pll;
	struct clk *clk;
	struct clk_init_data init;
	int ret;

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

	pll->rate_count = cnt;
	pll->rate_table = pll_clk->rate_table;

	if (!pll->rate_table)
		init.ops = &amlogic_pll2500_clk_min_ops;
	else
		  init.ops = &amlogic_pll2500_clk_ops;
	pll->hw.init = &init;
	pll->type = pll_clk->type;
	pll->con_reg = base;
	clk = clk_register(NULL, &pll->hw);
	if (IS_ERR(clk)) {
		pr_err("%s: failed to register pll clock %s : %ld\n",
			__func__, pll_clk->name, PTR_ERR(clk));
		kfree(pll);
		return;
	}

	amlogic_clk_add_lookup(clk, pll_clk->id);

	ret = clk_register_clkdev(clk, pll_clk->name, pll_clk->dev_name);
	if (ret)
		pr_err("%s: failed to register lookup for %s : %d",
			__func__, pll_clk->name, ret);

	pr_info("register PLL %s success done\n", pll_clk->name);
}

static struct amlogic_pll_clock sys_plls  = {
	.name = "sys_pll",
	.parent_name	= "xtal",
	.rate_table = syspll_tbl,
};

void __init sys_pll_init(void __iomem *base, struct device_node *np, u32 clk_id)
{

	int cnt = sizeof(syspll_tbl)/sizeof(syspll_tbl[0]);
	if (of_property_read_u32_index(np, "sys_max", 0, &max_freq))
		max_freq =	syspll_tbl[cnt-1].rate;
	sys_plls.id = clk_id;
	_amlogic_clk_register_pll(&sys_plls, base, cnt);
}
