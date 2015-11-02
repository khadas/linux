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

#include <dt-bindings/clock/meson8.h>
#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/clk-provider.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/delay.h>
#include "clk.h"
#include "clk-pll.h"

static void __iomem *reg_base_cbus;

#define to_hdmi_clk(_hw) container_of(_hw, struct hdmi_clock, hw)

#define CLK_GATE_BASE 0x100B
#define CLK_GATE_AO_BASE 0xc8100000
#define OFFSET(x) (((x) - CLK_GATE_BASE)*4) /*(x -0x100B)*4 */

#define AO_RTI_GEN_CTNL_REG0 0xc8100040
#define OFFSET_AO(x) (((x) - CLK_GATE_AO_BASE)*4)


#define HPLL_FVCO_RATE(_rate, _m, _n, _od) \
{							\
		.rate	=	(_rate),				\
		.m    = (_m),           \
		.n    = (_n),           \
		.od   = (_od),          \
}

static  struct amlogic_pll_rate_table hpll_lvds_tbl[] = {
	HPLL_FVCO_RATE(2160000000UL, 0x2d, 0x1, 0),
	HPLL_FVCO_RATE(1485000000, 0x3d, 0x1, 1),
	HPLL_FVCO_RATE(1296000000, 0x36, 0x1, 0),
};
struct aml_pll_conf hpll_pll_lvds_conf = {
	.od_mask = 0x3,
	.od_shift = 16,
	.n_mask = 0x1f,
	.n_shift = 10,
	.m_mask = 0x1ff,
	.m_shift = 0,
};
static struct aml_pll_conf hpll_pll_phy_conf = {
	.od_mask = 0x3,
	.od_shift = 18,
	.n_mask = 0x1f,
	.n_shift = 10,
	.m_mask = 0x1ff,
	.m_shift = 0,
};
static  struct amlogic_pll_rate_table hpll_phy_tbl[] = {
	/*fvco 2970Mhz*/
	HPLL_FVCO_RATE(2970000000UL, 0x3d, 0x1, 0),
	HPLL_FVCO_RATE(1485000000, 0x3d, 0x1, 1),

	HPLL_FVCO_RATE(1080000000, 0x2d, 0x1, 1),
	HPLL_FVCO_RATE(742500000, 0x3d, 0x1, 2),

	HPLL_FVCO_RATE(540000000, 0x2d, 0x1, 2),

	HPLL_FVCO_RATE(324000000, 0x36, 0x1, 2),
	HPLL_FVCO_RATE(270000000, 0x2d, 0x1, 3),

};
static  unsigned long pll_lvds_recalc_rate(struct amlogic_pll_clock *aml_pll, unsigned long parent_rate)
{
	u32 od, M, N, pll_con, frac, multi2;
	u32 fvco = parent_rate;
	pll_con = readl(aml_pll->pll_ctrl->con_reg);

	od = (pll_con >> aml_pll->pll_conf->od_shift) & aml_pll->pll_conf->od_mask;
	M = (pll_con >> aml_pll->pll_conf->m_shift) & aml_pll->pll_conf->m_mask;
	N = (pll_con >> aml_pll->pll_conf->n_shift) & aml_pll->pll_conf->n_mask;
	frac = readl(reg_base_cbus+OFFSET(HHI_VID_PLL_CNTL2)) & 0xfff;
	multi2 = (readl(reg_base_cbus+OFFSET(HHI_VID_PLL_CNTL4)) >> 24) & 0x1;
	fvco = fvco / 1000;
	fvco = (fvco * M + fvco*frac/(0xfff+1)) / N;
	fvco = (fvco >> od) << multi2;
	fvco = fvco * 1000;
	return fvco;
}
static  unsigned long pll_hdmi_recalc_rate(struct amlogic_pll_clock *aml_pll, unsigned long parent_rate)
{
	u32 od, M, N, pll_con, frac, multi2;
	u32 fvco = parent_rate;
	pll_con = readl(aml_pll->pll_ctrl->con_reg);

	od = (pll_con >> aml_pll->pll_conf->od_shift) & aml_pll->pll_conf->od_mask;
	M = (pll_con >> aml_pll->pll_conf->m_shift) & aml_pll->pll_conf->m_mask;
	N = (pll_con >> aml_pll->pll_conf->n_shift) & aml_pll->pll_conf->n_mask;
	frac = readl(reg_base_cbus+OFFSET(HHI_VID_PLL_CNTL2))  & 0xfff;
	multi2 = (readl(reg_base_cbus+OFFSET(HHI_VID_PLL_CNTL4)) >> 24) & 0x1;
	fvco = (fvco * M + fvco*frac/(0xfff+1)) / N;
	fvco = (fvco >> od) << multi2;
	return fvco;
}

struct aml_pll_co_ctrl hpll_co_ctrl[] = {
	PLL_CONF(0x10ca, 0x4b525012),
};

struct aml_pll_ctrl hpll_ctrl = {
	.con_reg_offset = OFFSET(0x10c8),
	.lock_reg_offset = OFFSET(0x10c8),
};
#define h_delay()       \
    do {                \
		int i = 1000;   \
		while (i--);    \
    } while (0)

#define WAIT_FOR_PLL_LOCKED(reg)                        \
    do {                                                \
	unsigned int st = 0, cnt = 10;                  \
	while (cnt--) {                                 \
	    h_delay();                   \
	    st = !!(readl_relaxed(reg) & (1 << 31));   \
	    if (st) {                                    \
		break;                                  \
	    }                                           \
	    else {  /* reset pll */                     \
		aml_set_reg32_bits(reg, 0x3, 29, 2);    \
		aml_set_reg32_bits(reg, 0x2, 29, 2);    \
	    }                                           \
	}                                               \
	if (cnt < 9)                                     \
	    pr_debug(KERN_CRIT "pll[0x%p] reset %d times\n", reg, 9 - cnt);\
    } while (0);
static void set_hpll_ctrl(struct amlogic_pll_clock *aml_pll,
	struct amlogic_pll_rate_table *rate)
{
	struct aml_pll_conf *pll_conf = aml_pll->pll_conf;
	u32 val;
	val = readl(reg_base_cbus+OFFSET(HHI_VID_PLL_CNTL));
	val = (val & (~(pll_conf->od_mask << pll_conf->od_shift))) | (rate->od << pll_conf->od_shift);
	val = (val & (~(pll_conf->m_mask << pll_conf->m_shift))) | (rate->m << pll_conf->m_shift);
	val = (val & (~(pll_conf->n_mask << pll_conf->n_shift))) | (rate->n << pll_conf->n_shift);
	writel(val, reg_base_cbus+OFFSET(HHI_VID_PLL_CNTL));
	/* do pll reset*/
	val = val | (0x1<<29);
	writel(val, reg_base_cbus+OFFSET(HHI_VID_PLL_CNTL));
	val = val & (~(0x1 << 29));
	writel(val, reg_base_cbus+OFFSET(HHI_VID_PLL_CNTL));
}
static int waite_hpll_lock(struct amlogic_pll_clock *aml_pll,
	struct amlogic_pll_rate_table *rate)
{
	u32 clk = (rate->rate<<rate->od)/1000;
	/*lock*/

		aml_write_reg32(reg_base_cbus+OFFSET(HHI_VID_PLL_CNTL2), 0x69c88000);
		aml_write_reg32(reg_base_cbus+OFFSET(HHI_VID_PLL_CNTL3), 0xca563823);
		aml_write_reg32(reg_base_cbus+OFFSET(HHI_VID_PLL_CNTL4), 0x40238100);
		aml_write_reg32(reg_base_cbus+OFFSET(HHI_VID_PLL_CNTL5), 0x00012286);
		aml_write_reg32(reg_base_cbus+OFFSET(HHI_VID2_PLL_CNTL2), 0x430a800);		/* internal LDO share with HPLL & VIID PLL */
		switch (clk) {
		case 2971000:		/* only for 4k mode */
			aml_write_reg32(reg_base_cbus+OFFSET(HHI_VID_PLL_CNTL2), 0x69c84000);
			aml_write_reg32(reg_base_cbus+OFFSET(HHI_VID_PLL_CNTL3), 0xce49c022);
			aml_write_reg32(reg_base_cbus+OFFSET(HHI_VID_PLL_CNTL4), 0x4123b100);
			aml_set_reg32_bits(reg_base_cbus+OFFSET(HHI_VID2_PLL_CNTL2), 1, 16, 1);
			aml_write_reg32(reg_base_cbus+OFFSET(HHI_VID_PLL_CNTL5), 0x00012385);
			set_hpll_ctrl(aml_pll, rate);
			/* aml_write_reg32(reg_base_cbus+OFFSET(HHI_VID_PLL_CNTL),  0x6002043d); */
			/* aml_write_reg32(reg_base_cbus+OFFSET(HHI_VID_PLL_CNTL),  0x4002043d); */
			WAIT_FOR_PLL_LOCKED(reg_base_cbus+OFFSET(HHI_VID_PLL_CNTL));
			h_delay();
			aml_write_reg32(reg_base_cbus+OFFSET(HHI_VID_PLL_CNTL5), 0x00016385);	/* optimise HPLL VCO 2.97GHz performance */
			aml_write_reg32(reg_base_cbus+OFFSET(HHI_VID_PLL_CNTL2), 0x69c84e00);

			break;
		case 2970000:		/* for 1080p/i 720p mode */
			aml_write_reg32(reg_base_cbus+OFFSET(HHI_VID_PLL_CNTL2), 0x69c84000);
			aml_write_reg32(reg_base_cbus+OFFSET(HHI_VID_PLL_CNTL3), 0x8a46c023);
			aml_write_reg32(reg_base_cbus+OFFSET(HHI_VID_PLL_CNTL4), 0x4123b100);
			aml_write_reg32(reg_base_cbus+OFFSET(HHI_VID_PLL_CNTL5), 0x00012385);
			set_hpll_ctrl(aml_pll, rate);
			/* aml_write_reg32(reg_base_cbus+OFFSET(HHI_VID_PLL_CNTL),  0x6002043d); */
			/* aml_write_reg32(reg_base_cbus+OFFSET(HHI_VID_PLL_CNTL),  0x4002043d); */
			WAIT_FOR_PLL_LOCKED(reg_base_cbus+OFFSET(HHI_VID_PLL_CNTL));
			h_delay();
			aml_write_reg32(reg_base_cbus+OFFSET(HHI_VID_PLL_CNTL5), 0x00016385);	/* optimise HPLL VCO 2.97GHz performance */
			aml_write_reg32(reg_base_cbus+OFFSET(HHI_VID_PLL_CNTL2), 0x69c84e00);
			break;
		case 2160000:
			aml_write_reg32(reg_base_cbus+OFFSET(HHI_VID_PLL_CNTL2), 0x69c84000);
			aml_write_reg32(reg_base_cbus+OFFSET(HHI_VID_PLL_CNTL3), 0x8a46c023);
			aml_write_reg32(reg_base_cbus+OFFSET(HHI_VID_PLL_CNTL4), 0x0123b100);
			aml_write_reg32(reg_base_cbus+OFFSET(HHI_VID_PLL_CNTL5), 0x12385);
			set_hpll_ctrl(aml_pll, rate);
			/* aml_write_reg32(reg_base_cbus+OFFSET(HHI_VID_PLL_CNTL),  0x6001042d); */
			/* aml_write_reg32(reg_base_cbus+OFFSET(HHI_VID_PLL_CNTL),  0x4001042d); */
			WAIT_FOR_PLL_LOCKED(reg_base_cbus+OFFSET(HHI_VID_PLL_CNTL));
			break;
		case 1080000:
			set_hpll_ctrl(aml_pll, rate);
			/* aml_write_reg32(reg_base_cbus+OFFSET(HHI_VID_PLL_CNTL),  0x6001042d); */
			/* aml_write_reg32(reg_base_cbus+OFFSET(HHI_VID_PLL_CNTL),  0x4001042d); */
			WAIT_FOR_PLL_LOCKED(reg_base_cbus+OFFSET(HHI_VID_PLL_CNTL));
			break;
		case 1296000:
			aml_write_reg32(reg_base_cbus+OFFSET(HHI_VID_PLL_CNTL2), 0x59c88000);
			aml_write_reg32(reg_base_cbus+OFFSET(HHI_VID_PLL_CNTL3), 0xca49b022);
			aml_write_reg32(reg_base_cbus+OFFSET(HHI_VID_PLL_CNTL4), 0x0023b100);
			aml_write_reg32(reg_base_cbus+OFFSET(HHI_VID_PLL_CNTL5), 0x00012385);
			set_hpll_ctrl(aml_pll, rate);
			/* aml_write_reg32(reg_base_cbus+OFFSET(HHI_VID_PLL_CNTL),  0x600c0436); */
			/* aml_write_reg32(reg_base_cbus+OFFSET(HHI_VID_PLL_CNTL),  0x400c0436); */
			aml_write_reg32(reg_base_cbus+OFFSET(HHI_VID_PLL_CNTL5), 0x00016385);
			WAIT_FOR_PLL_LOCKED(reg_base_cbus+OFFSET(HHI_VID_PLL_CNTL));
			break;
		default:
			printk("error hpll clk: %d\n", clk);
			break;
		}
		if (clk < 2970)
			aml_write_reg32(reg_base_cbus+OFFSET(HHI_VID_PLL_CNTL5), (readl_relaxed(reg_base_cbus+OFFSET(HHI_VID_PLL_CNTL5)) & (~(0xf << 12))) | (0x6 << 12));
	return 0;
}

static struct amlogic_pll_clock hdmi_plls[]  = {
	HDMI_PLL(pll_3000_lvds, CLK_HDMITX_PLL, "hdmi_pll_lvds", "xtal", 0,
		&hpll_ctrl, NULL, waite_hpll_lock, hpll_lvds_tbl,
		&hpll_pll_lvds_conf, hpll_co_ctrl, pll_lvds_recalc_rate),
	HDMI_PLL(pll_3000_lvds, CLK_HDMITX_PHY, "hdmi_pll_phy", "xtal", 0,
		&hpll_ctrl, NULL, waite_hpll_lock, hpll_phy_tbl,
		&hpll_pll_phy_conf, hpll_co_ctrl, pll_hdmi_recalc_rate),
};
struct vid_clk_table {
	unsigned int rate;
	unsigned int prate;
	unsigned int post_mux_val;
	unsigned int post_n_val;
	unsigned int pre_mux_val;
};
#define post_mux_shift 8
#define post_mux_mask 0x3
#define post_n_shift 12
#define post_n_mask 0x7
#define pre_mux_shift 4
#define pre_mux_mask 0x7
#define VID_CLK(_rate, _prate, _p_m_v, _p_n_v, _pre_m_v)   \
{                         \
	.rate = _rate, \
	.prate = _prate, \
	.post_mux_val = _p_m_v, \
	.post_n_val = _p_n_v, \
	.pre_mux_val = _pre_m_v, \
}
static struct vid_clk_table vid_clk_tbl[] = {
	VID_CLK(432000000, 2160000000UL, 0, 1, 4),
	VID_CLK(216000000, 1296000000, 0, 1, 5),
	VID_CLK(148500000, 1485000000, 1, 1, 4),
};
static unsigned long vid_pll_clk_recal(struct clk_hw *hw,
					unsigned long parent_rate)
{
	u32 val, post_mux, post_n, pre_mux, rate;
	val = readl(reg_base_cbus + OFFSET(HHI_VID_DIVIDER_CNTL));
	post_mux = (val >> post_mux_shift) & post_mux_mask;
	post_n = (val >> post_n_shift) & post_n_mask;
	pre_mux = (val >> pre_mux_shift) & pre_mux_mask;
	rate = parent_rate / (pre_mux+1);
	if (post_mux)
		rate = rate / (post_n+1);
	return rate;
}
static long vid_pll_clk_round(struct clk_hw *hw, unsigned long drate,
					unsigned long *prate)
{
	int i;
	unsigned long rate;
	/* Assumming rate_table is in descending order */
	for (i = 0; i < ARRAY_SIZE(vid_clk_tbl); i++) {
		if (drate >= vid_clk_tbl[i].rate) {
			rate = vid_clk_tbl[i].rate;
			*prate = vid_clk_tbl[i].prate;
			break;
		}
	}

	/* return minimum supported value */
	if (i == ARRAY_SIZE(vid_clk_tbl)) {
		rate = vid_clk_tbl[i - 1].rate;
		*prate = vid_clk_tbl[i-1].prate;
		}
	return rate;
}
static int	vid_pll_clk_set(struct clk_hw *hw, unsigned long drate,
				    unsigned long prate)
{
	int i;
	u32 val;
	struct vid_clk_table *vid_tabl = NULL;

	/* Assumming rate_table is in descending order */
	for (i = 0; i < ARRAY_SIZE(vid_clk_tbl); i++) {
		if (drate >= vid_clk_tbl[i].rate) {
			vid_tabl = &vid_clk_tbl[i];
			break;
		}
	}
	if (i == ARRAY_SIZE(vid_clk_tbl))
		vid_tabl = &vid_clk_tbl[i-1];
	/*need fix later*/
	val = readl(reg_base_cbus + OFFSET(HHI_VID_DIVIDER_CNTL));
	val = (val & (~(0x1<<16)));
	writel(val, (reg_base_cbus + OFFSET(HHI_VID_DIVIDER_CNTL)));

	val = readl(reg_base_cbus + OFFSET(HHI_VID_DIVIDER_CNTL));
	val = (val & (~(post_mux_mask << post_mux_shift))) | (vid_tabl->post_mux_val << post_mux_shift);
	val = (val & (~(post_n_mask << post_n_shift))) | (vid_tabl->post_n_val << post_n_shift);
	val = (val & (~(pre_mux_mask << pre_mux_shift))) | (vid_tabl->pre_mux_val << pre_mux_shift);
	writel(val, (reg_base_cbus + OFFSET(HHI_VID_DIVIDER_CNTL)));

    /* Soft Reset div_post/div_pre */
	aml_set_reg32_bits(reg_base_cbus + OFFSET(HHI_VID_DIVIDER_CNTL), 0, 0, 2);
	aml_set_reg32_bits(reg_base_cbus + OFFSET(HHI_VID_DIVIDER_CNTL), 1, 3, 1);
	aml_set_reg32_bits(reg_base_cbus + OFFSET(HHI_VID_DIVIDER_CNTL), 1, 7, 1);
	aml_set_reg32_bits(reg_base_cbus + OFFSET(HHI_VID_DIVIDER_CNTL), 3, 0, 2);
	aml_set_reg32_bits(reg_base_cbus + OFFSET(HHI_VID_DIVIDER_CNTL), 0, 3, 1);
	aml_set_reg32_bits(reg_base_cbus + OFFSET(HHI_VID_DIVIDER_CNTL), 0, 7, 1);

	val = readl(reg_base_cbus + OFFSET(HHI_VID_DIVIDER_CNTL));
	val = val | (0x1 << 16);
	writel(val, (reg_base_cbus + OFFSET(HHI_VID_DIVIDER_CNTL)));

	return 0;
}
static int	vid_pll_clk_prepare(struct clk_hw *hw)
{
	aml_set_reg32_bits(reg_base_cbus + OFFSET(HHI_VID_DIVIDER_CNTL), 0, 15, 1);
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
#define  HDMI_CLK_TBL(_name, _pname, _ops, _flags) \
{  \
	.name = _name,  \
	.parent_name = _pname,  \
	.ops = _ops,  \
	.flags = _flags,  \
}
#define  HDMI_ENC_CLK_TBL(_name, _pname, _ops, _flags, _encx_tbl, _encx_shift, _ctrl_offset, _id) \
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
	CTS_XXX_TBL(148500000, 148500000, 1, 1),
	CTS_XXX_TBL(108000000, 432000000, 4, 1),
	CTS_XXX_TBL(74250000, 148500000, 1, 2),
	CTS_XXX_TBL(54000000, 432000000, 4, 2),
};

static struct cts_encx_table cts_enci_tbl[] = {
	CTS_XXX_TBL(54000000, 432000000, 4, 2),
	CTS_XXX_TBL(27000000, 432000000, 4, 1),
};
static struct cts_encx_table cts_pixel_tbl[] = {
	CTS_XXX_TBL(148500000, 148500000, 1, 1),
	CTS_XXX_TBL(108000000, 216000000, 4, 1),
	CTS_XXX_TBL(74250000, 148500000, 1, 2),
	CTS_XXX_TBL(54000000, 432000000, 4, 2),
	CTS_XXX_TBL(27000000, 216000000, 4, 2),
};

static int	encx_clk_prepare(struct clk_hw *hw)
{
	aml_set_reg32_bits(reg_base_cbus + OFFSET(HHI_VID_CLK_CNTL), 1, 19, 1);
	aml_set_reg32_bits(reg_base_cbus + OFFSET(HHI_VID_CLK_CNTL), 0, 16, 3);
	aml_set_reg32_bits(reg_base_cbus + OFFSET(HHI_VID_CLK_CNTL), 7, 0, 3);
	return 0;
}
static int	encx_clk_set(struct clk_hw *hw, unsigned long drate,
				    unsigned long prate)
{
	struct hdmi_clock *hdmi_clk = to_hdmi_clk(hw);
	struct cts_encx_table *enc_tbl = NULL;
	int i;
	u32 val;
	for (i = 0; i < hdmi_clk->encx_tbl_cnt; i++) {
		if (drate >= hdmi_clk->encx_tbl[i].rate) {
			enc_tbl = &hdmi_clk->encx_tbl[i];
			break;
		}
	}
	if (i == hdmi_clk->encx_tbl_cnt)
		enc_tbl = &hdmi_clk->encx_tbl[i-1];
	val = readl(reg_base_cbus+OFFSET(HHI_VID_CLK_DIV));
	val = (val & (~(0xff))) | (enc_tbl->final_div-1);
	writel(val, (reg_base_cbus+OFFSET(HHI_VID_CLK_DIV)));

	val = readl(hdmi_clk->ctrl_reg);
	val = (val & (~(hdmi_clk->encx_mask << hdmi_clk->encx_shift))) | ((enc_tbl->cts_encx_div-1) << hdmi_clk->encx_shift);
	writel(val, hdmi_clk->ctrl_reg);
	return 0;
}
static unsigned long encx_clk_recal(struct clk_hw *hw,
					unsigned long parent_rate)
{
	struct hdmi_clock *hdmi_clk = to_hdmi_clk(hw);
	unsigned final_div, encx_div, drate;
	final_div = (readl(reg_base_cbus+OFFSET(HHI_VID_CLK_DIV)) & 0xff) + 1;
	encx_div = ((readl(hdmi_clk->ctrl_reg)>>hdmi_clk->encx_shift) & hdmi_clk->encx_mask) + 1;
	drate = parent_rate / final_div;
	drate = drate  / encx_div;
	return drate;
}
static long encx_clk_round(struct clk_hw *hw, unsigned long drate,
					unsigned long *prate)
{
	struct hdmi_clock *hdmi_clk = to_hdmi_clk(hw);
	struct cts_encx_table *enc_tbl = NULL;
	int i;
	for (i = 0; i < hdmi_clk->encx_tbl_cnt; i++) {
		if (drate >= hdmi_clk->encx_tbl[i].rate) {
			enc_tbl = &hdmi_clk->encx_tbl[i];
			break;
		}
	}
	if (i == hdmi_clk->encx_tbl_cnt)
		enc_tbl = &hdmi_clk->encx_tbl[i-1];
	*prate = enc_tbl->prate;
	return enc_tbl->rate;
}

static struct clk_ops encx_clk_ops = {
	.prepare = encx_clk_prepare,
	.set_rate = encx_clk_set,
	.recalc_rate = encx_clk_recal,
	.round_rate = encx_clk_round,
};

static struct hdmi_clock hdmi_clock_tbl[] = {
	HDMI_CLK_TBL("vid_clk", "hdmi_pll_lvds", &vid_clk_ops, CLK_SET_RATE_PARENT),
	HDMI_ENC_CLK_TBL("cts_encp_clk", "vid_clk", &encx_clk_ops, CLK_SET_RATE_PARENT, cts_encp_tbl, 24, HHI_VID_CLK_DIV, CLK_HDMITX_ENCP),
	HDMI_ENC_CLK_TBL("cts_enci_clk", "vid_clk", &encx_clk_ops, CLK_SET_RATE_PARENT, cts_enci_tbl, 28, HHI_VID_CLK_DIV, CLK_HDMITX_ENCI),
	HDMI_ENC_CLK_TBL("cts_pixel_clk", "vid_clk", &encx_clk_ops, CLK_SET_RATE_PARENT, cts_pixel_tbl, 16, HHI_HDMI_CLK_CNTL, CLK_HDMITX_PIXEL),
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
	hdmi_clk->ctrl_reg = reg_base_cbus+OFFSET(hdmi_clk->ctrl_offset);
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
/* register meson8 clocks */
void __init hdmi_clk_init(void __iomem *reg_base)
{
	int i = 0;
	reg_base_cbus = reg_base;
	amlogic_clk_register_pll(hdmi_plls,
	    ARRAY_SIZE(hdmi_plls), reg_base);
	for (i = 0; i < ARRAY_SIZE(hdmi_clock_tbl); i++)
		hdmi_clk_register(&hdmi_clock_tbl[i]);

#if 0
	{
		struct clk *clk;
		clk = __clk_lookup("cts_encp_clk");
		printk("#####\n");
		clk_set_rate(clk, 148500000);
		printk("#####\n");
		clk_set_rate(clk,  74250000);
		printk("#####\n");


	}
#endif
	pr_info("hdmi clock init complete\n");
}
