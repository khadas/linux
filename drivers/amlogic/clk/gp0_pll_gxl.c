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
#define	HHI_GP0_PLL_CNTL			(0x10 << 2)
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
	GPLL0_RATE(756000000, 63, 1, 1),
	GPLL0_RATE(768000000, 64, 1, 1),
	GPLL0_RATE(780000000, 65, 1, 1),
	GPLL0_RATE(792000000, 66, 1, 1),
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
	int times = 0;
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

	do {
		gp0_update_bits(HHI_GP0_PLL_CNTL, 1<<29, 1 << 29);
		udelay(10);
		gp0_update_bits(HHI_GP0_PLL_CNTL, 1<<29, 0);
		mdelay(1);
		times++;
		udelay(10);
		if (times > 100) {
			pr_info("gp0 pll can't lock\n");
			pr_info("cntl=%x\n",
				readl(hiu_base + HHI_GP0_PLL_CNTL));
			pr_info("cntl1=%x\n",
				readl(hiu_base + HHI_GP0_PLL_CNTL1));
			pr_info("cntl2=%x\n",
				readl(hiu_base + HHI_GP0_PLL_CNTL2));
			pr_info("cntl3=%x\n",
				readl(hiu_base + HHI_GP0_PLL_CNTL3));
			pr_info("cntl4=%x\n",
				readl(hiu_base + HHI_GP0_PLL_CNTL4));
			pr_info("cntl5=%x\n",
				readl(hiu_base + HHI_GP0_PLL_CNTL5));
			break;
		}
	} while (!((readl(hiu_base + HHI_GP0_PLL_CNTL) >> 31) & 0x1));

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
