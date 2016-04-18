/*
 * drivers/amlogic/clk/hdmi-clk.c
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

#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/clk-provider.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/delay.h>
#include "clk.h"
#include "clk-pll.h"
#define	HHI_GP0_PLL_CNTL				(0x10 << 2)
#define	HHI_GP0_PLL_CNTL2			(0x11 << 2)
#define	HHI_GP0_PLL_CNTL3			(0x12 << 2)
#define	HHI_GP0_PLL_CNTL4			(0x13 << 2)
#define	HHI_GP0_PLL_CNTL5			(0x14 << 2)



static void __iomem *hiu_base;
struct gpll_rate_table {
	unsigned long rate;
	unsigned int m;
	unsigned int n;
	unsigned int od;
};
#define od_mask (0x3<<16)
#define m_mask (0x1ff << 0)
#define n_mask (0x1f << 9)
#define od_sft  16
#define m_sft 0
#define n_sft  9

static void gp0_update_bits(size_t reg, size_t mask, unsigned int val)
{
	unsigned int tmp, orig;
	orig = readl(hiu_base + reg);
	tmp = orig & ~mask;
	tmp |= val & mask;
	writel(tmp, hiu_base + reg);
}

#define GPLL0_RATE(_rate, _m, _n, _od) \
{							\
		.rate	=	(_rate),				\
		.m    = (_m),           \
		.n    = (_n),           \
		.od   = (_od),          \
}

static  struct gpll_rate_table gpll0_tbl[] = {
	GPLL0_RATE(96000000, 32, 1, 3),
	GPLL0_RATE(99000000, 33, 1, 3),
	GPLL0_RATE(102000000, 34, 1, 3),
	GPLL0_RATE(105000000, 35, 1, 3),
	GPLL0_RATE(108000000, 36, 1, 3),
	GPLL0_RATE(111000000, 37, 1, 3),
	GPLL0_RATE(114000000, 38, 1, 3),
	GPLL0_RATE(117000000, 39, 1, 3),
	GPLL0_RATE(120000000, 40, 1, 3),
	GPLL0_RATE(123000000, 41, 1, 3),
	GPLL0_RATE(126000000, 42, 1, 3),
	GPLL0_RATE(129000000, 43, 1, 3),
	GPLL0_RATE(132000000, 44, 1, 3),
	GPLL0_RATE(135000000, 45, 1, 3),
	GPLL0_RATE(138000000, 46, 1, 3),
	GPLL0_RATE(141000000, 47, 1, 3),
	GPLL0_RATE(144000000, 48, 1, 3),
	GPLL0_RATE(147000000, 49, 1, 3),
	GPLL0_RATE(150000000, 50, 1, 3),
	GPLL0_RATE(153000000, 51, 1, 3),
	GPLL0_RATE(156000000, 52, 1, 3),
	GPLL0_RATE(159000000, 53, 1, 3),
	GPLL0_RATE(162000000, 54, 1, 3),
	GPLL0_RATE(165000000, 55, 1, 3),
	GPLL0_RATE(168000000, 56, 1, 3),
	GPLL0_RATE(171000000, 57, 1, 3),
	GPLL0_RATE(174000000, 58, 1, 3),
	GPLL0_RATE(177000000, 59, 1, 3),
	GPLL0_RATE(180000000, 60, 1, 3),
	GPLL0_RATE(183000000, 61, 1, 3),
	GPLL0_RATE(186000000, 62, 1, 3),
	GPLL0_RATE(192000000, 32, 1, 2),
	GPLL0_RATE(198000000, 33, 1, 2),
	GPLL0_RATE(204000000, 34, 1, 2),
	GPLL0_RATE(210000000, 35, 1, 2),
	GPLL0_RATE(216000000, 36, 1, 2),
	GPLL0_RATE(222000000, 37, 1, 2),
	GPLL0_RATE(228000000, 38, 1, 2),
	GPLL0_RATE(234000000, 39, 1, 2),
	GPLL0_RATE(240000000, 40, 1, 2),
	GPLL0_RATE(246000000, 41, 1, 2),
	GPLL0_RATE(252000000, 42, 1, 2),
	GPLL0_RATE(258000000, 43, 1, 2),
	GPLL0_RATE(264000000, 44, 1, 2),
	GPLL0_RATE(270000000, 45, 1, 2),
	GPLL0_RATE(276000000, 46, 1, 2),
	GPLL0_RATE(282000000, 47, 1, 2),
	GPLL0_RATE(288000000, 48, 1, 2),
	GPLL0_RATE(294000000, 49, 1, 2),
	GPLL0_RATE(300000000, 50, 1, 2),
	GPLL0_RATE(306000000, 51, 1, 2),
	GPLL0_RATE(312000000, 52, 1, 2),
	GPLL0_RATE(318000000, 53, 1, 2),
	GPLL0_RATE(324000000, 54, 1, 2),
	GPLL0_RATE(330000000, 55, 1, 2),
	GPLL0_RATE(336000000, 56, 1, 2),
	GPLL0_RATE(342000000, 57, 1, 2),
	GPLL0_RATE(348000000, 58, 1, 2),
	GPLL0_RATE(354000000, 59, 1, 2),
	GPLL0_RATE(360000000, 60, 1, 2),
	GPLL0_RATE(366000000, 61, 1, 2),
	GPLL0_RATE(372000000, 62, 1, 2),
	GPLL0_RATE(384000000, 32, 1, 1),
	GPLL0_RATE(396000000, 33, 1, 1),
	GPLL0_RATE(408000000, 34, 1, 1),
	GPLL0_RATE(420000000, 35, 1, 1),
	GPLL0_RATE(432000000, 36, 1, 1),
	GPLL0_RATE(444000000, 37, 1, 1),
	GPLL0_RATE(456000000, 38, 1, 1),
	GPLL0_RATE(468000000, 39, 1, 1),
	GPLL0_RATE(480000000, 40, 1, 1),
	GPLL0_RATE(492000000, 41, 1, 1),
	GPLL0_RATE(504000000, 42, 1, 1),
	GPLL0_RATE(516000000, 43, 1, 1),
	GPLL0_RATE(528000000, 44, 1, 1),
	GPLL0_RATE(540000000, 45, 1, 1),
	GPLL0_RATE(552000000, 46, 1, 1),
	GPLL0_RATE(564000000, 47, 1, 1),
	GPLL0_RATE(576000000, 48, 1, 1),
	GPLL0_RATE(588000000, 49, 1, 1),
	GPLL0_RATE(600000000, 50, 1, 1),
	GPLL0_RATE(612000000, 51, 1, 1),
	GPLL0_RATE(624000000, 52, 1, 1),
	GPLL0_RATE(636000000, 53, 1, 1),
	GPLL0_RATE(648000000, 54, 1, 1),
	GPLL0_RATE(660000000, 55, 1, 1),
	GPLL0_RATE(672000000, 56, 1, 1),
	GPLL0_RATE(684000000, 57, 1, 1),
	GPLL0_RATE(696000000, 58, 1, 1),
	GPLL0_RATE(708000000, 59, 1, 1),
	GPLL0_RATE(720000000, 60, 1, 1),
	GPLL0_RATE(732000000, 61, 1, 1),
	GPLL0_RATE(744000000, 62, 1, 1),
	GPLL0_RATE(768000000, 32, 1, 0),
	GPLL0_RATE(792000000, 33, 1, 0),
	GPLL0_RATE(816000000, 34, 1, 0),
	GPLL0_RATE(840000000, 35, 1, 0),
	GPLL0_RATE(864000000, 36, 1, 0),
	GPLL0_RATE(888000000, 37, 1, 0),
	GPLL0_RATE(912000000, 38, 1, 0),
	GPLL0_RATE(936000000, 39, 1, 0),
	GPLL0_RATE(960000000, 40, 1, 0),
	GPLL0_RATE(984000000, 41, 1, 0),
	GPLL0_RATE(1008000000, 42, 1, 0),
	GPLL0_RATE(1032000000, 43, 1, 0),
	GPLL0_RATE(1056000000, 44, 1, 0),
	GPLL0_RATE(1080000000, 45, 1, 0),
	GPLL0_RATE(1104000000, 46, 1, 0),
	GPLL0_RATE(1128000000, 47, 1, 0),
	GPLL0_RATE(1152000000, 48, 1, 0),
	GPLL0_RATE(1176000000, 49, 1, 0),
	GPLL0_RATE(1200000000, 50, 1, 0),
	GPLL0_RATE(1224000000, 51, 1, 0),
	GPLL0_RATE(1248000000, 52, 1, 0),
	GPLL0_RATE(1272000000, 53, 1, 0),
	GPLL0_RATE(1296000000, 54, 1, 0),
	GPLL0_RATE(1320000000, 55, 1, 0),
	GPLL0_RATE(1344000000, 56, 1, 0),
	GPLL0_RATE(1368000000, 57, 1, 0),
	GPLL0_RATE(1392000000, 58, 1, 0),
	GPLL0_RATE(1416000000, 59, 1, 0),
	GPLL0_RATE(1440000000, 60, 1, 0),
	GPLL0_RATE(1464000000, 61, 1, 0),
	GPLL0_RATE(1488000000, 62, 1, 0),
};

static unsigned long gpll_clk_recal(struct clk_hw *hw,
					unsigned long parent_rate)
{
	size_t od, m, n, val;
	unsigned long rate;
	val = readl(hiu_base + HHI_GP0_PLL_CNTL);
	od = val & od_mask;
	m = val & m_mask;
	n = val & n_mask;
	rate = (parent_rate * m/n) >> od;
	return rate;
}
static long gpll_clk_round(struct clk_hw *hw, unsigned long drate,
					unsigned long *prate)
{
	int i;
	unsigned long rate;
	/* Assumming rate_table is in descending order */
	for (i = 0; i < ARRAY_SIZE(gpll0_tbl); i++) {
		if (drate <= gpll0_tbl[i].rate) {
			rate = gpll0_tbl[i].rate;
			/* pr_info("drate =%ld,rate=%ld\n",drate,rate); */
			break;
		}
	}

	/* return minimum supported value */
	if (i == ARRAY_SIZE(gpll0_tbl))
		rate = gpll0_tbl[i - 1].rate;

	return rate;
}
static int	gpll_clk_set(struct clk_hw *hw, unsigned long drate,
				    unsigned long prate)
{

	int i;
	size_t m, n, od;
	struct gpll_rate_table *tbl;
	/* Assumming rate_table is in descending order */
	for (i = 0; i < ARRAY_SIZE(gpll0_tbl); i++) {
		if (drate <= gpll0_tbl[i].rate) {
			tbl = &gpll0_tbl[i];
			break;
		}
	}

	if (i == ARRAY_SIZE(gpll0_tbl))
		tbl = &gpll0_tbl[i];

	m = tbl->m;
	n = tbl->n;
	od = tbl->od;

	writel(0x6a000228, hiu_base + HHI_GP0_PLL_CNTL);
	writel(0x69c80000, hiu_base + HHI_GP0_PLL_CNTL2);
	writel(0x0a5590c4, hiu_base + HHI_GP0_PLL_CNTL3);
	writel(0x0000500d, hiu_base + HHI_GP0_PLL_CNTL4);

	gp0_update_bits(HHI_GP0_PLL_CNTL, m_mask, m<<m_sft);
	gp0_update_bits(HHI_GP0_PLL_CNTL, n_mask, n<<n_sft);
	gp0_update_bits(HHI_GP0_PLL_CNTL, od_mask, od<<od_sft);
	gp0_update_bits(HHI_GP0_PLL_CNTL, 1<<29, 0);
	/* pr_info("drate = %ld, HHI_GP0_PLL_CNTL = %x\n", */
			/* drate,readl(hiu_base + HHI_GP0_PLL_CNTL)); */
	mdelay(1);
	if (!((readl(hiu_base + HHI_GP0_PLL_CNTL) >> 31) & 0x1))
		pr_info(" can't lock gp0 pll\n");
	return 0;
}
static struct clk_ops gp0_ops = {
	.set_rate = gpll_clk_set,
	.recalc_rate = gpll_clk_recal,
	.round_rate = gpll_clk_round,
};
struct gpll_clock {
	struct clk_hw	hw;
	char *name;
	const char *parent_name;
	struct clk_ops  *ops;
	unsigned long flags;
};
static struct gpll_clock gpll_clk = {
	.name = "gp0_pll",
	.parent_name = "xtal",
	.ops = &gp0_ops,
	.flags = 0,
};
void __init gp0_clk_init(void __iomem *reg_base, unsigned int id)
{
	struct clk_init_data init;
	struct clk *clk;
	int ret;
	hiu_base = reg_base;
	init.name = gpll_clk.name;
	init.ops = gpll_clk.ops;
	init.flags = gpll_clk.flags;
	init.parent_names = &gpll_clk.parent_name;
	init.num_parents = 1;
	gpll_clk.hw.init = &init;
	clk = clk_register(NULL, &gpll_clk.hw);

	if (IS_ERR(clk))
		pr_err("%s: failed to register clk %s\n",
			__func__, gpll_clk.name);

	ret = clk_register_clkdev(clk, gpll_clk.name,
					NULL);
	if (ret)
		pr_err("%s: failed to register lookup %s\n",
			__func__, gpll_clk.name);
	amlogic_clk_add_lookup(clk, id);
#if 0
	{

		struct clk *clk;
		clk = __clk_lookup("gp0_pll");
		pr_info("#####\n");
		clk_set_rate(clk, 792000000);
		pr_info("#####\n");

	}
#endif
}
