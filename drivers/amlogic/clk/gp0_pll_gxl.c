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
#define	HHI_GP0_PLL_CNTL1			(0x16 << 2)
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
	GPLL0_RATE(192000000, 64, 1, 3),
	GPLL0_RATE(216000000, 72, 1, 3),
	GPLL0_RATE(240000000, 80, 1, 3),
	GPLL0_RATE(264000000, 88, 1, 3),
	GPLL0_RATE(288000000, 96, 1, 3),
	GPLL0_RATE(312000000, 104, 1, 3),
	GPLL0_RATE(336000000, 112, 1, 3),
	GPLL0_RATE(360000000, 120, 1, 3),
	GPLL0_RATE(384000000, 64, 1, 2),
	GPLL0_RATE(408000000, 68, 1, 2),
	GPLL0_RATE(432000000, 72, 1, 2),
	GPLL0_RATE(456000000, 76, 1, 2),
	GPLL0_RATE(480000000, 80, 1, 2),
	GPLL0_RATE(504000000, 84, 1, 2),
	GPLL0_RATE(528000000, 88, 1, 2),
	GPLL0_RATE(552000000, 92, 1, 2),
	GPLL0_RATE(576000000, 96, 1, 2),
	GPLL0_RATE(600000000, 100, 1, 2),
	GPLL0_RATE(624000000, 104, 1, 2),
	GPLL0_RATE(648000000, 108, 1, 2),
	GPLL0_RATE(672000000, 112, 1, 2),
	GPLL0_RATE(696000000, 116, 1, 2),
	GPLL0_RATE(720000000, 120, 1, 2),
	GPLL0_RATE(744000000, 124, 1, 2),
	GPLL0_RATE(768000000, 64, 1, 1),
	GPLL0_RATE(792000000, 66, 1, 1),
	GPLL0_RATE(816000000, 68, 1, 1),
	GPLL0_RATE(840000000, 70, 1, 1),
	GPLL0_RATE(864000000, 72, 1, 1),
	GPLL0_RATE(888000000, 74, 1, 1),
	GPLL0_RATE(912000000, 76, 1, 1),
	GPLL0_RATE(936000000, 78, 1, 1),
	GPLL0_RATE(960000000, 80, 1, 1),
	GPLL0_RATE(984000000, 82, 1, 1),
	GPLL0_RATE(1008000000, 84, 1, 1),
	GPLL0_RATE(1032000000, 86, 1, 1),
	GPLL0_RATE(1056000000, 88, 1, 1),
	GPLL0_RATE(1080000000, 90, 1, 1),
	GPLL0_RATE(1104000000, 92, 1, 1),
	GPLL0_RATE(1128000000, 94, 1, 1),
	GPLL0_RATE(1152000000, 96, 1, 1),
	GPLL0_RATE(1176000000, 98, 1, 1),
	GPLL0_RATE(1200000000, 100, 1, 1),
	GPLL0_RATE(1224000000, 102, 1, 1),
	GPLL0_RATE(1248000000, 104, 1, 1),
	GPLL0_RATE(1272000000, 106, 1, 1),
	GPLL0_RATE(1296000000, 108, 1, 1),
	GPLL0_RATE(1320000000, 110, 1, 1),
	GPLL0_RATE(1344000000, 112, 1, 1),
	GPLL0_RATE(1368000000, 114, 1, 1),
	GPLL0_RATE(1392000000, 116, 1, 1),
	GPLL0_RATE(1416000000, 118, 1, 1),
	GPLL0_RATE(1440000000, 120, 1, 1),
	GPLL0_RATE(1464000000, 122, 1, 1),
	GPLL0_RATE(1488000000, 124, 1, 1),
	GPLL0_RATE(1512000000, 63, 1, 0),
	GPLL0_RATE(1536000000, 64, 1, 0),
	GPLL0_RATE(1560000000, 65, 1, 0),
	GPLL0_RATE(1584000000, 66, 1, 0),
	GPLL0_RATE(1608000000, 67, 1, 0),
	GPLL0_RATE(1632000000, 68, 1, 0),
	GPLL0_RATE(1656000000, 69, 1, 0),
	GPLL0_RATE(1680000000, 70, 1, 0),
	GPLL0_RATE(1704000000, 71, 1, 0),
	GPLL0_RATE(1728000000, 72, 1, 0),
	GPLL0_RATE(1752000000, 73, 1, 0),
	GPLL0_RATE(1776000000, 74, 1, 0),
	GPLL0_RATE(1800000000, 75, 1, 0),
	GPLL0_RATE(1824000000, 76, 1, 0),
	GPLL0_RATE(1848000000, 77, 1, 0),
	GPLL0_RATE(1872000000, 78, 1, 0),
	GPLL0_RATE(1896000000, 79, 1, 0),
	GPLL0_RATE(1920000000, 80, 1, 0),
	GPLL0_RATE(1944000000, 81, 1, 0),
	GPLL0_RATE(1968000000, 82, 1, 0),
	GPLL0_RATE(1992000000, 83, 1, 0),
	GPLL0_RATE(2016000000, 84, 1, 0),
	GPLL0_RATE(2040000000, 85, 1, 0),
	GPLL0_RATE(2064000000, 86, 1, 0),
	GPLL0_RATE(2088000000, 87, 1, 0),
	GPLL0_RATE(2112000000, 88, 1, 0),
	GPLL0_RATE(2136000000, 89, 1, 0),
	GPLL0_RATE(2160000000, 90, 1, 0),
	GPLL0_RATE(2184000000, 91, 1, 0),
	GPLL0_RATE(2208000000, 92, 1, 0),
	GPLL0_RATE(2232000000, 93, 1, 0),
	GPLL0_RATE(2256000000, 94, 1, 0),
	GPLL0_RATE(2280000000, 95, 1, 0),
	GPLL0_RATE(2304000000, 96, 1, 0),
	GPLL0_RATE(2328000000, 97, 1, 0),
	GPLL0_RATE(2352000000, 98, 1, 0),
	GPLL0_RATE(2376000000, 99, 1, 0),
	GPLL0_RATE(2400000000, 100, 1, 0),
	GPLL0_RATE(2424000000, 101, 1, 0),
	GPLL0_RATE(2448000000, 102, 1, 0),
	GPLL0_RATE(2472000000, 103, 1, 0),
	GPLL0_RATE(2496000000, 104, 1, 0),
	GPLL0_RATE(2520000000, 105, 1, 0),
	GPLL0_RATE(2544000000, 106, 1, 0),
	GPLL0_RATE(2568000000, 107, 1, 0),
	GPLL0_RATE(2592000000, 108, 1, 0),
	GPLL0_RATE(2616000000, 109, 1, 0),
	GPLL0_RATE(2640000000, 110, 1, 0),
	GPLL0_RATE(2664000000, 111, 1, 0),
	GPLL0_RATE(2688000000, 112, 1, 0),
	GPLL0_RATE(2712000000, 113, 1, 0),
	GPLL0_RATE(2736000000, 114, 1, 0),
	GPLL0_RATE(2760000000, 115, 1, 0),
	GPLL0_RATE(2784000000, 116, 1, 0),
	GPLL0_RATE(2808000000, 117, 1, 0),
	GPLL0_RATE(2832000000, 118, 1, 0),
	GPLL0_RATE(2856000000, 119, 1, 0),
	GPLL0_RATE(2880000000, 120, 1, 0),
	GPLL0_RATE(2904000000, 121, 1, 0),
	GPLL0_RATE(2928000000, 122, 1, 0),
	GPLL0_RATE(2952000000, 123, 1, 0),
	GPLL0_RATE(2976000000, 124, 1, 0),
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

	writel(0x40010250, hiu_base + HHI_GP0_PLL_CNTL);
	writel(0xc084a000, hiu_base + HHI_GP0_PLL_CNTL1);
	writel(0xb75020be, hiu_base + HHI_GP0_PLL_CNTL2);
	writel(0x0a59a288, hiu_base + HHI_GP0_PLL_CNTL3);
	writel(0xc000004d, hiu_base + HHI_GP0_PLL_CNTL4);
	writel(0x00078000, hiu_base + HHI_GP0_PLL_CNTL5);

	gp0_update_bits(HHI_GP0_PLL_CNTL, m_mask, m<<m_sft);
	gp0_update_bits(HHI_GP0_PLL_CNTL, n_mask, n<<n_sft);
	gp0_update_bits(HHI_GP0_PLL_CNTL, od_mask, od<<od_sft);
	gp0_update_bits(HHI_GP0_PLL_CNTL, 1<<29, 1 << 29);
	/* pr_info("drate = %ld, HHI_GP0_PLL_CNTL = %x\n", */
			/* drate,readl(hiu_base + HHI_GP0_PLL_CNTL)); */
	mdelay(1);
	gp0_update_bits(HHI_GP0_PLL_CNTL, 1<<29, 0);
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
void __init gp0_clk_gxl_init(void __iomem *reg_base, unsigned int id)
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
