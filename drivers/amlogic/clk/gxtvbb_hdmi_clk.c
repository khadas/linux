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

#include <dt-bindings/clock/gxbb.h>
#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/clk-provider.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/delay.h>
#include "clk.h"
#include "clk-pll.h"
#include <linux/amlogic/cpu_version.h>
#undef pr_fmt
#define pr_fmt(fmt) "gxbb_hdmi_clk: " fmt

#undef DEBUG
/* #define DEBUG */
#ifndef DEBUG
#undef pr_info
#define pr_info(fmt, ...)
#endif
#define to_hdmi_clk(_hw) container_of(_hw, struct hdmi_clock, hw)
static void __iomem *hiu_base;

static void hdmi_update_bits(unsigned int reg, unsigned int mask,
					unsigned int val)
{
	unsigned int tmp, orig;
	orig = readl(hiu_base + reg);
	tmp = orig & ~mask;
	tmp |= val & mask;
	writel(tmp, hiu_base + reg);
}

#undef	HHI_HDMI_PLL_CNTL
#undef	HHI_HDMI_PLL_CNTL2
#undef	HHI_HDMI_PLL_CNTL3
#undef	HHI_HDMI_PLL_CNTL4
#undef	HHI_HDMI_PLL_CNTL5
#undef	HHI_HDMI_PLL_CNTL6
#undef	HHI_VID_PLL_CLK_DIV
#undef	HHI_VID_CLK_CNTL
#undef	HHI_VID_CLK_CNTL2
#undef	HHI_VID_CLK_DIV
#undef	HHI_HDMI_CLK_CNTL

#define	HHI_HDMI_PLL_CNTL	(0xc8 << 2)
#define	HHI_HDMI_PLL_CNTL2	(0xc9 << 2)
#define	HHI_HDMI_PLL_CNTL3	(0xca << 2)
#define	HHI_HDMI_PLL_CNTL4	(0xcb << 2)
#define	HHI_HDMI_PLL_CNTL5	(0xcc << 2)
#define	HHI_HDMI_PLL_CNTL6	(0xcd << 2)
#define	HHI_VID_PLL_CLK_DIV	(0x68 << 2)
#define	HHI_VID_CLK_CNTL	(0x5f << 2)
#define	HHI_VID_CLK_CNTL2	(0x65 << 2)
#define	HHI_VID_CLK_DIV		(0x59 << 2)
#define	HHI_HDMI_CLK_CNTL	(0x73 << 2)
#define DIV_1    0
#define DIV_2      1
#define DIV_3      2
#define DIV_3p5    3
#define DIV_3p75   4
#define DIV_4      5
#define DIV_5      6
#define DIV_6      7
#define DIV_6p25   8
#define DIV_7      9
#define DIV_7p5    10
#define DIV_12     11
#define DIV_14     12
#define DIV_15     13

#define HPLL_FVCO_RATE(_rate, _m, _n, _od1, _od2) \
{							\
		.rate	=	(_rate),				\
		.m    = (_m),           \
		.n    = (_n),           \
		.od   = (_od1),          \
		.od2 = (_od2), \
}
#define VID_CLK(_rate, _prate, _mux1, _mux2, _div, _od3)   \
{                         \
	.rate = _rate, \
	.prate = _prate, \
	.mux1 = _mux1, \
	.mux2 = _mux2, \
	.div = _div, \
	.od3 = _od3, \
}
struct vid_clk_table {
	unsigned int rate;
	unsigned int prate;
	unsigned int mux1;
	unsigned int mux2;
	unsigned int div;
	unsigned int od3;
};

static struct aml_pll_conf hpll_pll_phy_conf = {
	.od_mask = 0x3,
	.od_shift = 16,
	.od2_mask = 0x3,
	.od2_shift = 22,
	.n_mask = 0x1f,
	.n_shift = 9,
	.m_mask = 0x1ff,
	.m_shift = 0,
};

static struct amlogic_pll_rate_table hpll_phy_tbl[] = {
	HPLL_FVCO_RATE(5940000, 0x7b, 0x1, 0, 0),
	HPLL_FVCO_RATE(3450000, 0x47, 0x1, 0, 0),
	HPLL_FVCO_RATE(2970000, 0x3d, 0x1, 0, 0),
	HPLL_FVCO_RATE(1485000, 0x3d, 0x1, 1, 0),
	HPLL_FVCO_RATE(742500, 0x3d, 0x1, 2, 0),
	HPLL_FVCO_RATE(270000, 0x5a, 0x1, 2, 2),
};
static struct vid_clk_table vid_clk_tbl[] = {
	VID_CLK(594000, 5940000, 0, 1, DIV_5, 1),
	VID_CLK(594000, 2970000, 0, 1, DIV_5, 0),
	VID_CLK(345000, 3450000, 0, 1, DIV_5, 1),
	VID_CLK(297000, 2970000, 0, 1, DIV_5, 1),
	VID_CLK(148500, 1485000, 0, 1, DIV_5, 1),
	VID_CLK(148500, 742500, 0, 1, DIV_5, 0),
	VID_CLK(54000,  270000, 0, 1, DIV_5, 0),
};

/*
*	return value in Khz
*/
static unsigned long hpll_clk_recal(struct clk_hw *hw,
					unsigned long parent_rate)
{
	size_t val, m, n, od1, od2, rate, frac, fvco;
	fvco = parent_rate * 2;
	val = readl(hiu_base + HHI_HDMI_PLL_CNTL);
	m = (val >> hpll_pll_phy_conf.m_shift) & hpll_pll_phy_conf.m_mask;
	n =  (val >> hpll_pll_phy_conf.n_shift) & hpll_pll_phy_conf.n_mask;
	frac = readl(hiu_base + HHI_HDMI_PLL_CNTL2)  & 0xfff;
	fvco = fvco / 1000;
	fvco = (fvco * m + fvco*frac/(0x3ff+1)) / n;
	pr_info("fvco = %zd\n", fvco);
	val = readl(hiu_base + HHI_HDMI_PLL_CNTL2);
	od1 = (val >> hpll_pll_phy_conf.od_shift) & hpll_pll_phy_conf.od_mask;
	od2 = (val >> hpll_pll_phy_conf.od2_shift) & hpll_pll_phy_conf.od2_mask;
	rate = fvco / ((1 << od1) * (1 << od2));
	pr_info("od1=%zd,od2=%zd\n", od1, od2);
	return rate;
}
static long hpll_clk_round(struct clk_hw *hw, unsigned long drate,
					unsigned long *prate)
{
	size_t i;
	/* Assumming rate_table is in descending order */
	for (i = 0; i < ARRAY_SIZE(hpll_phy_tbl); i++) {
		if (drate >= hpll_phy_tbl[i].rate)
			return hpll_phy_tbl[i].rate;
	}
	/* return minimum supported value */
	return hpll_phy_tbl[i - 1].rate;
}
static void set_pll(struct amlogic_pll_rate_table *rate_tbl)
{
	size_t  m , n , od, od2, val;

	m = rate_tbl->m;
	n = rate_tbl->n;
	od = rate_tbl->od;
	od2 = rate_tbl->od2;
	pr_info("m =%zx,n=%zd,od=%zd,od2=%zd\n", m, n, od, od2);
	/* set m n*/
	val = readl(hiu_base + HHI_HDMI_PLL_CNTL);
	val = val & (~(hpll_pll_phy_conf.m_mask << hpll_pll_phy_conf.m_shift));
	val = val | (m << hpll_pll_phy_conf.m_shift);
	writel(val, hiu_base + HHI_HDMI_PLL_CNTL);


	val = readl(hiu_base + HHI_HDMI_PLL_CNTL);
	val = val & (~(hpll_pll_phy_conf.n_mask << hpll_pll_phy_conf.n_shift));
	val = val | (n << hpll_pll_phy_conf.n_shift);
	writel(val, hiu_base + HHI_HDMI_PLL_CNTL);

	/*set od od2*/

	val = readl(hiu_base + HHI_HDMI_PLL_CNTL2);
	val = val &
		(~(hpll_pll_phy_conf.od_mask << hpll_pll_phy_conf.od_shift));
	val = val | (od << hpll_pll_phy_conf.od_shift);
	writel(val, hiu_base + HHI_HDMI_PLL_CNTL2);


	val = readl(hiu_base + HHI_HDMI_PLL_CNTL2);
	val = val &
		(~(hpll_pll_phy_conf.od2_mask << hpll_pll_phy_conf.od2_shift));
	val = val | (od2 << hpll_pll_phy_conf.od2_shift);
	writel(val, hiu_base + HHI_HDMI_PLL_CNTL2);


	/*reset pll */
	val = readl(hiu_base + HHI_HDMI_PLL_CNTL);
	val = val & (~(1 << 28));
	writel(val, hiu_base + HHI_HDMI_PLL_CNTL);
	mdelay(10);

	if (!(readl(hiu_base + HHI_HDMI_PLL_CNTL) >> 31))
		pr_info("hdmi pll can't lock\n");

}
static int	hpll_clk_set(struct clk_hw *hw, unsigned long drate,
				    unsigned long prate)
{
	size_t i = 0;
	struct amlogic_pll_rate_table *rate_tbl;
	for (i = 0; i < ARRAY_SIZE(hpll_phy_tbl); i++) {
		if (drate >= hpll_phy_tbl[i].rate) {
			rate_tbl = &hpll_phy_tbl[i];
			break;
		}
	}
	if (i == ARRAY_SIZE(hpll_phy_tbl))
		rate_tbl = &hpll_phy_tbl[i];

	pr_info("%s,drate = %ld,found %ld\n", __func__, drate, rate_tbl->rate);
	switch (drate) {
	case 5940000:
		writel(0x5800027b, hiu_base + HHI_HDMI_PLL_CNTL);
		writel(0x00000000, hiu_base + HHI_HDMI_PLL_CNTL2);
		hdmi_update_bits(HHI_HDMI_PLL_CNTL2, 0xffff, 0x4300);
		writel(0x135c5091, hiu_base + HHI_HDMI_PLL_CNTL3);
		writel(0x801da72c, hiu_base + HHI_HDMI_PLL_CNTL4);
		writel(0x71486980, hiu_base + HHI_HDMI_PLL_CNTL5);
		writel(0x000002e55, hiu_base + HHI_HDMI_PLL_CNTL6);
		set_pll(rate_tbl);
		pr_info("hpll reg: 0x%x\n",
			readl(hiu_base + HHI_HDMI_PLL_CNTL));
		break;
	case 3450000:
		writel(0x58000247, hiu_base + HHI_HDMI_PLL_CNTL);
		writel(0x00000000, hiu_base + HHI_HDMI_PLL_CNTL2);
		hdmi_update_bits(HHI_HDMI_PLL_CNTL2, 0xffff, 0x4300);
		writel(0x0d5c5091, hiu_base + HHI_HDMI_PLL_CNTL3);
		writel(0x801da72c, hiu_base + HHI_HDMI_PLL_CNTL4);
		writel(0x71486980, hiu_base + HHI_HDMI_PLL_CNTL5);
		writel(0x000002e55, hiu_base + HHI_HDMI_PLL_CNTL6);
		set_pll(rate_tbl);
		pr_info("hpll reg: 0x%x\n",
			readl(hiu_base + HHI_HDMI_PLL_CNTL));
		break;
	case 2970000:
	case 1485000:
	case  742500:
		writel(0x5800023d, hiu_base + HHI_HDMI_PLL_CNTL);
		writel(0x00000000, hiu_base + HHI_HDMI_PLL_CNTL2);
		hdmi_update_bits(HHI_HDMI_PLL_CNTL2, 0xffff, 0x4380);
		writel(0x0d5c5091, hiu_base + HHI_HDMI_PLL_CNTL3);
		writel(0x801da72c, hiu_base + HHI_HDMI_PLL_CNTL4);
		writel(0x71486980, hiu_base + HHI_HDMI_PLL_CNTL5);
		writel(0x000002e55, hiu_base + HHI_HDMI_PLL_CNTL6);
		set_pll(rate_tbl);
		pr_info("hpll reg: 0x%x\n",
			readl(hiu_base + HHI_HDMI_PLL_CNTL));
		break;
	case 270000:
		writel(0x5800025a, hiu_base + HHI_HDMI_PLL_CNTL);
		writel(0x00000000, hiu_base + HHI_HDMI_PLL_CNTL2);
		writel(0x0d5c5091, hiu_base + HHI_HDMI_PLL_CNTL3);
		writel(0x801da72c, hiu_base + HHI_HDMI_PLL_CNTL4);
		writel(0x71486980, hiu_base + HHI_HDMI_PLL_CNTL5);
		writel(0x000002e55, hiu_base + HHI_HDMI_PLL_CNTL6);
		set_pll(rate_tbl);
		pr_info("hpll reg: 0x%x\n",
			readl(hiu_base + HHI_HDMI_PLL_CNTL));
		break;
	default:
		pr_info("wrong drate %ld\n", drate);
		break;
	}
	return 0;
}

static struct clk_ops hpll_ops = {
	.set_rate = hpll_clk_set,
	.recalc_rate = hpll_clk_recal,
	.round_rate = hpll_clk_round,
};

#define HPLL(_id, _name, _pname, _flags, _ops)				\
{								\
		.id		= _id,					\
		.name		= _name,				\
		.parent_name	= _pname,				\
		.flags		= _flags|CLK_GET_RATE_NOCACHE,		\
		.ops = _ops, \
}

static struct amlogic_pll_clock hdmi_plls[]  = {

	HPLL(CLK_HDMITX_PHY, "hdmi_pll_phy", "xtal", 0,
		&hpll_ops),
};


#define mux1_shift 8
#define mux2_shift 16
#define mux2_mask 0x3
#define div_shift 0
#define div_mask 0x7fff
#define od3_shift 18
#define od3_mask 0x3

static size_t cal_rate(unsigned int div, unsigned long parent_rate)
{
	size_t rate = -1;
	switch (div) {
	case 0x739c:
		rate = parent_rate/5;
	default:
		break;
	}
	return rate;
}
static unsigned long vid_pll_clk_recal(struct clk_hw *hw,
					unsigned long parent_rate)
{
	size_t val = 0, od3, rate;
	val = (readl(hiu_base + HHI_VID_PLL_CLK_DIV) >> 18) & 0x1;
	od3 = (readl(hiu_base + HHI_HDMI_PLL_CNTL2) >> 18) & 0x3;
	if (val)
		rate = parent_rate >> od3;
	else{
		size_t mux, div;
		mux = (readl(hiu_base + HHI_VID_PLL_CLK_DIV)>>16) & 0x3;
		div =  (readl(hiu_base + HHI_VID_PLL_CLK_DIV)>>0) & 0x7fff;
		pr_info("%s,mux=%zd,div=%zx\n", __func__, mux, div);
		if (mux == 0)
			rate = cal_rate(div, parent_rate);
		else if (mux == 1)
			rate = cal_rate(div, parent_rate);
		else if (mux == 2)
			rate = cal_rate(div, parent_rate);
		else
			rate = cal_rate(div, parent_rate);
	}
	rate = rate >> od3;
	pr_info("%s ,parent_rate =%ld rate = %zd\n", __func__, parent_rate,
		rate);
	return rate;
}
static long vid_pll_clk_round(struct clk_hw *hw, unsigned long drate,
					unsigned long *prate)
{
	int i;
	unsigned long rate;
	/* Assumming rate_table is in descending order */
	for (i = 0; i < ARRAY_SIZE(vid_clk_tbl); i++) {
		if (drate == vid_clk_tbl[i].rate) {
			rate = vid_clk_tbl[i].rate;
			pr_info("%s,i=%d\n", __func__, i);
			break;
		}
	}

	/* return minimum supported value */
	if (i == ARRAY_SIZE(vid_clk_tbl))
		rate = vid_clk_tbl[i - 1].rate;
	return rate;
}
static int	vid_pll_clk_set(struct clk_hw *hw, unsigned long drate,
				    unsigned long prate)
{
	size_t shift_val = 0;
	size_t shift_sel = 0;
	size_t div_sel, od3;
	int i;
	/* Assumming rate_table is in descending order */
	for (i = 0; i < ARRAY_SIZE(vid_clk_tbl); i++) {
		if (drate == vid_clk_tbl[i].rate &&
				prate == vid_clk_tbl[i].prate) {
			div_sel = vid_clk_tbl[i].div;
			od3 = vid_clk_tbl[i].od3;
			break;
		}
	}
	if (i >= ARRAY_SIZE(vid_clk_tbl)) {
		div_sel = vid_clk_tbl[i - 1].div;
		od3 = vid_clk_tbl[i - 1].od3;
	}

	pr_info("%s,drate =%ld,index =%d\n", __func__, drate, i);
	/* Disable the output clock */
	hdmi_update_bits(HHI_VID_PLL_CLK_DIV, 1<<15, 0);
	hdmi_update_bits(HHI_VID_PLL_CLK_DIV, 1<<19, 0);

	switch (div_sel) {
	case DIV_1:
		shift_val = 0xFFFF;
		shift_sel = 0;
		break;
	case DIV_2:
		shift_val = 0x0aaa;
		shift_sel = 0;
		break;
	case DIV_3:
		shift_val = 0x0db6;
		shift_sel = 0;
		break;
	case DIV_3p5:
		shift_val = 0x36cc;
		shift_sel = 1;
		break;
	case DIV_3p75:
		shift_val = 0x6666;
		shift_sel = 2;
		break;
	case DIV_4:
		shift_val = 0x0ccc;
		shift_sel = 0;
		break;
	case DIV_5:
		shift_val = 0x739c;
		shift_sel = 2;
		break;
	case DIV_6:
		shift_val = 0x0e38;
		shift_sel = 0;
		break;
	case DIV_6p25:
		shift_val = 0x0000;
		shift_sel = 3;
		break;
	case DIV_7:
		shift_val = 0x3c78;
		shift_sel = 1;
		break;
	case DIV_7p5:
		shift_val = 0x78f0;
		shift_sel = 2;
		break;
	case DIV_12:
		shift_val = 0x0fc0;
		shift_sel = 0;
		break;
	case DIV_14:
		shift_val = 0x3f80;
		shift_sel = 1;
		break;
	case DIV_15:
		shift_val = 0x7f80;
		shift_sel = 2;
		break;
	default:
		pr_info("invalid vid pll div\n");
		break;
	}

	if (shift_val == 0xffff)	/* if divide by 1 */
		hdmi_update_bits(HHI_VID_PLL_CLK_DIV, 1<<18, 1<<18);
	else {
		hdmi_update_bits(HHI_VID_PLL_CLK_DIV, 1<<18, 0);
		hdmi_update_bits(HHI_VID_PLL_CLK_DIV, 0x3<<16 ,
						shift_sel << 16);
		hdmi_update_bits(HHI_VID_PLL_CLK_DIV, 0x1<<15 , 0x1 << 15);
		hdmi_update_bits(HHI_VID_PLL_CLK_DIV, 0x7fff<<0 ,
						shift_val << 0);
		/* Set the selector low */
		hdmi_update_bits(HHI_VID_PLL_CLK_DIV, 0x1<<15, 0);
	}
	hdmi_update_bits(HHI_HDMI_PLL_CNTL2, 0x3<<18, od3 << 18);
	/* Enable the final output clock */
	hdmi_update_bits(HHI_VID_PLL_CLK_DIV, 0x1<<19, 0x1<<19);
	return 0;
}
static int	vid_pll_clk_prepare(struct clk_hw *hw)
{
	return 0;
}

static struct clk_ops vid_clk_ops = {
	.prepare = vid_pll_clk_prepare,
	.set_rate = vid_pll_clk_set,
	.recalc_rate = vid_pll_clk_recal,
	.round_rate = vid_pll_clk_round,
};
struct cts_encx_table {
	unsigned int rate;
	unsigned int prate;
	unsigned int final_div;
	unsigned int cts_encx_div;
};

struct hdmi_clock {
	struct clk_hw	hw;
	char *name;
	const char *parent_name;
	struct clk_ops  *ops;
	unsigned long flags;
	struct cts_encx_table *encx_tbl;
	unsigned int encx_tbl_cnt;
	unsigned int encx_shift;
	unsigned int encx_mask;
	unsigned int ctrl_offset;
	void __iomem *ctrl_reg;
	u32 id;
};
#define  HDMI_CLK_TBL(_name, _pname, _ops, _flags, _id) \
{  \
	.name = _name,  \
	.parent_name = _pname,  \
	.ops = _ops,  \
	.flags = _flags,  \
	.id = _id, \
}
#define  HDMI_ENC_CLK_TBL(_name, _pname, _ops, _flags, \
			  _encx_tbl, _encx_shift, _ctrl_offset, _id) \
{  \
	.name = _name,  \
	.parent_name = _pname,  \
	.ops = _ops,  \
	.flags = _flags,  \
	.encx_tbl = _encx_tbl, \
	.encx_tbl_cnt = ARRAY_SIZE(_encx_tbl), \
	.encx_shift = _encx_shift, \
	.encx_mask = 0xf, \
	.ctrl_offset = _ctrl_offset, \
	.id = _id, \
}

#define  CTS_XXX_TBL(_rate, _prate, _final_div, _xx_div) \
{  \
	.rate = _rate,  \
	.prate = _prate,  \
	.final_div = _final_div,  \
	.cts_encx_div = _xx_div,  \
}

static struct cts_encx_table cts_encp_tbl[] = {
	CTS_XXX_TBL(594000, 594000, 1, 1),
	CTS_XXX_TBL(297000, 297000, 1, 1),
	CTS_XXX_TBL(148500, 148500, 1, 1),
	CTS_XXX_TBL(108000, 432000, 4, 1),
	CTS_XXX_TBL(74250, 148500, 1, 2),
	CTS_XXX_TBL(54000, 540000, 1, 1),
};

static struct cts_encx_table cts_enci_tbl[] = {
	CTS_XXX_TBL(54000, 432000, 4, 2),
	CTS_XXX_TBL(27000, 54000, 1, 2),
	CTS_XXX_TBL(27000, 216000, 4, 2),
};
static struct cts_encx_table cts_pixel_tbl[] = {
	CTS_XXX_TBL(594000, 594000, 1, 1),
	CTS_XXX_TBL(297000, 594000, 1, 2),
	CTS_XXX_TBL(297000, 297000, 1, 1),
	CTS_XXX_TBL(148500, 148500, 1, 1),
	CTS_XXX_TBL(108000, 216000, 4, 1),
	CTS_XXX_TBL(74250, 148500, 1, 2),
	CTS_XXX_TBL(27000, 54000, 1, 2),
	CTS_XXX_TBL(27000, 216000, 4, 2),
};

static int encx_clk_prepare(struct clk_hw *hw)
{
	hdmi_update_bits(HHI_VID_CLK_CNTL, 1<<19, 1<<19);
	hdmi_update_bits(HHI_VID_CLK_CNTL, 0x3<<16, 0);
	hdmi_update_bits(HHI_VID_CLK_CNTL, 0x3<<0, 0x3<<0);
	hdmi_update_bits(HHI_VID_CLK_CNTL2, 0x1f<<0, 0x1f<<0);
	return 0;
}
static int encx_clk_set(struct clk_hw *hw, unsigned long drate,
				    unsigned long prate)
{
	struct hdmi_clock *hdmi_clk = to_hdmi_clk(hw);
	struct cts_encx_table *enc_tbl = NULL;
	int i, val = 0;
	for (i = 0; i < hdmi_clk->encx_tbl_cnt; i++) {
		if (drate == hdmi_clk->encx_tbl[i].rate &&
			prate == hdmi_clk->encx_tbl[i].prate) {
			enc_tbl = &hdmi_clk->encx_tbl[i];
			break;
		}
	}
	if (i == hdmi_clk->encx_tbl_cnt)
		enc_tbl = &hdmi_clk->encx_tbl[i-1];
	pr_info("hdmi_clk %s\n", hdmi_clk->name);
	pr_info(" %s i = %d, prate =%ld,drate =%ld\n", __func__, i, prate,
		drate);
	hdmi_update_bits(HHI_VID_CLK_DIV, 0xff << 0, (enc_tbl->final_div-1)<<0);

	val = readl(hdmi_clk->ctrl_reg);
	val = val & (~(hdmi_clk->encx_mask << hdmi_clk->encx_shift));
	val = val | ((enc_tbl->cts_encx_div-1) << hdmi_clk->encx_shift);
	writel(val, hdmi_clk->ctrl_reg);

	return 0;
}
static unsigned long encx_clk_recal(struct clk_hw *hw,
					unsigned long parent_rate)
{
	struct hdmi_clock *hdmi_clk = to_hdmi_clk(hw);
	unsigned final_div, encx_div, drate;
	final_div = (readl(hiu_base+HHI_VID_CLK_DIV) & 0xff) + 1;
	encx_div = ((readl(hdmi_clk->ctrl_reg)>>hdmi_clk->encx_shift)
						& hdmi_clk->encx_mask);
	drate = parent_rate / final_div;
	pr_info("%s,parent_rate = %ld,final_div =%d,encx_div=%d\n",
				__func__, parent_rate, final_div, encx_div);
	if (encx_div == 0)
		drate = drate / 1;
	else if (encx_div == 1)
		drate = drate / 2;
	else if (encx_div == 2)
		drate = drate / 4;
	else if (encx_div == 3)
		drate = drate / 6;
	else if (encx_div == 6)
		drate = drate / 12;
	pr_info("%s,drate =%d\n", __func__, drate);
	return drate;
}
static long encx_clk_round(struct clk_hw *hw, unsigned long drate,
					unsigned long *prate)
{
	struct hdmi_clock *hdmi_clk = to_hdmi_clk(hw);
	struct cts_encx_table *enc_tbl = NULL;
	int i;
	for (i = 0; i < hdmi_clk->encx_tbl_cnt; i++) {
		if (drate == hdmi_clk->encx_tbl[i].rate) {
			enc_tbl = &hdmi_clk->encx_tbl[i];
			break;
		}
	}
	if (i == hdmi_clk->encx_tbl_cnt)
		enc_tbl = &hdmi_clk->encx_tbl[i-1];
	pr_info("%s,enc_tbl->rate =%d,prate =%ld\n",
				__func__, enc_tbl->rate, *prate);
	return enc_tbl->rate;
}

static struct clk_ops encx_clk_ops = {
	.prepare = encx_clk_prepare,
	.set_rate = encx_clk_set,
	.recalc_rate = encx_clk_recal,
	.round_rate = encx_clk_round,
};

static struct hdmi_clock hdmi_clock_tbl[] = {
	HDMI_CLK_TBL("vid_clk", "hdmi_pll_phy", &vid_clk_ops,
					CLK_SET_RATE_NO_REPARENT, CLK_VID),
	HDMI_ENC_CLK_TBL("cts_encp_clk", "vid_clk",
			&encx_clk_ops, CLK_SET_RATE_NO_REPARENT,
			cts_encp_tbl, 24, HHI_VID_CLK_DIV, CLK_HDMITX_ENCP),
	HDMI_ENC_CLK_TBL("cts_enci_clk", "vid_clk",
			 &encx_clk_ops, CLK_SET_RATE_NO_REPARENT,
			 cts_enci_tbl, 28, HHI_VID_CLK_DIV, CLK_HDMITX_ENCI),
	HDMI_ENC_CLK_TBL("cts_pixel_clk", "vid_clk",
			 &encx_clk_ops, CLK_SET_RATE_NO_REPARENT,
		cts_pixel_tbl, 16, HHI_HDMI_CLK_CNTL, CLK_HDMITX_PIXEL),
};
static struct clk *hdmi_clk_register(struct hdmi_clock *hdmi_clk)
{
	struct clk_init_data init;
	struct clk *clk;
	int ret;
	init.name = hdmi_clk->name;
	init.ops = hdmi_clk->ops;
	init.flags = hdmi_clk->flags;
	init.parent_names = &hdmi_clk->parent_name;
	init.num_parents = 1;
	hdmi_clk->hw.init = &init;
	hdmi_clk->ctrl_reg = hiu_base+hdmi_clk->ctrl_offset;
	clk = clk_register(NULL, &hdmi_clk->hw);

	if (IS_ERR(clk))
		return ERR_PTR(-1);

	ret = clk_register_clkdev(clk, hdmi_clk->name,
					NULL);
	if (ret)
		pr_err("%s: failed to register lookup %s\n",
			__func__, hdmi_clk->name);
	amlogic_clk_add_lookup(clk, hdmi_clk->id);
	return clk;
}
/* register gxbb clocks */
static struct clk *hdmi_pll_register(struct amlogic_pll_clock *hdmi_clk)
{
	struct clk_init_data init;
	struct clk *clk;
	int ret;
	init.name = hdmi_clk->name;
	init.ops = hdmi_clk->ops;
	init.flags = hdmi_clk->flags;
	init.parent_names = &hdmi_clk->parent_name;
	init.num_parents = 1;
	hdmi_clk->hw.init = &init;
	clk = clk_register(NULL, &hdmi_clk->hw);

	if (IS_ERR(clk))
		return ERR_PTR(-1);

	ret = clk_register_clkdev(clk, hdmi_clk->name,
					NULL);
	if (ret)
		pr_err("%s: failed to register lookup %s\n",
			__func__, hdmi_clk->name);
	amlogic_clk_add_lookup(clk, hdmi_clk->id);
	return clk;
}

void __init gxtvbb_hdmi_clk_init(void __iomem *reg_base)
{
	int i = 0;
	hiu_base = reg_base;
	for (i = 0; i < ARRAY_SIZE(hdmi_plls); i++)
		hdmi_pll_register(&hdmi_plls[i]);
	for (i = 0; i < ARRAY_SIZE(hdmi_clock_tbl); i++)
		hdmi_clk_register(&hdmi_clock_tbl[i]);

#if 0
	{
		struct clk *clk;
		pr_info("#####\n");
		clk = __clk_lookup("hdmi_pll_phy");
		clk_set_rate(clk, 742500);
		pr_info("get  rate = %ld\n", clk_get_rate(clk));
		pr_info("#####\n");
		clk = __clk_lookup("vid_clk");
		clk_set_rate(clk, 148500);

		pr_info("get  rate = %ld\n", clk_get_rate(clk));
		pr_info("#####\n");
		clk = __clk_lookup("cts_encp_clk");
		clk_set_rate(clk, 148500);

		pr_info("get  rate = %ld\n", clk_get_rate(clk));
		pr_info("#####\n");
		clk = __clk_lookup("cts_pixel_clk");
		clk_set_rate(clk, 74250);

		pr_info("get  rate = %ld\n", clk_get_rate(clk));
		pr_info("#####\n");

		pr_info("@@@@@@@@@@@@@@@\n");

		clk = __clk_lookup("hdmi_pll_phy");
		clk_set_rate(clk, 1485000);

		pr_info("get  rate = %ld\n", clk_get_rate(clk));

		pr_info("@@@@@@@@@@@@@@@\n");
		clk = __clk_lookup("vid_clk");
		clk_set_rate(clk, 148500);

		pr_info("get  rate = %ld\n", clk_get_rate(clk));
		pr_info("@@@@@@@@@@@@@@@\n");

		clk = __clk_lookup("cts_encp_clk");
		clk_set_rate(clk, 148500);
		pr_info("get  rate = %ld\n", clk_get_rate(clk));
		pr_info("@@@@@@@@@@@@@@@\n");

		clk = __clk_lookup("cts_pixel_clk");
		clk_set_rate(clk, 148500);

		pr_info("get  rate = %ld\n", clk_get_rate(clk));
		pr_info("#####\n");

	}
#endif
	pr_info("gxbb hdmi clock init complete\n");
}
