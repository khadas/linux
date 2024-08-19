// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2024 Rockchip Electronics Co. Ltd.
 *
 * Author: Cai Wenzhong <cwz@rock-chips.com>
 */

#include <linux/kernel.h>
#include <linux/iopoll.h>

#include "rkx12x_reg.h"
#include "rkx12x_txphy.h"
#include "rkx12x_compact.h"

#define TXPHY_TIMING_ADJUST_API		1 /* 1: enable txphy timing adjust api */

#define PSEC_PER_SEC		1000000000000LL

/* mipi txphy base */
#define MIPI_TX0_PHY_BASE	RKX12X_MIPI_LVDS_TX_PHY0_BASE
#define MIPI_TX1_PHY_BASE	RKX12X_MIPI_LVDS_TX_PHY1_BASE

#define MIPI_TXPHY_DEV(id)	((id) == 0 ? (MIPI_TX0_PHY_BASE) : (MIPI_TX1_PHY_BASE))

/* mipi txphy register */
#define CLANE_PARA0		0x0000
#define CLANE_PARA1		0x0004
#define T_INITTIME_C(x)		UPDATE(x, 31, 0)

#define CLANE_PARA2		0x0008
#define T_CLKPREPARE_C(x)	UPDATE(x, 23, 16)
#define T_CLKZERO_C(x)		UPDATE(x, 15, 8)
#define T_CLKPRE_C(x)		UPDATE(x, 7, 0)

#define CLANE_PARA3		0x000c
#define T_CLKPOST_C_MASK	GENMASK(23, 16)
#define T_CLKPOST_C(x)		UPDATE(x, 23, 16)
#define T_CLKTRAIL_C_MASK	GENMASK(15, 8)
#define T_CLKTRAIL_C(x)		UPDATE(x, 15, 8)
#define T_HSEXIT_C_MASK		GENMASK(7, 0)
#define T_HSEXIT_C(x)		UPDATE(x, 7, 0)

#define DLANE0_PARA0		0x0010
#define DLANE1_PARA0		0x0024
#define DLANE2_PARA0		0x0034
#define DLANE3_PARA0		0x0044
#define T_RST2ENLPTX_D(x)	UPDATE(x, 15, 0)

#define DLANE0_PARA1		0x0014
#define DLANE1_PARA1		0x0028
#define DLANE2_PARA1		0x0038
#define DLANE3_PARA1		0x0048
#define T_INITTIME_D(x)		UPDATE(x, 31, 0)

#define DLANE0_PARA2		0x0018
#define DLANE1_PARA2		0x002c
#define DLANE2_PARA2		0x003c
#define DLANE3_PARA2		0x004c
#define T_HSPREPARE_D(x)	UPDATE(x, 31, 24)
#define T_HSZERO_D(x)		UPDATE(x, 23, 16)
#define T_HSTRAIL_D(x)		UPDATE(x, 15, 8)
#define T_HSEXIT_D(x)		UPDATE(x, 7, 0)

#define DLANE0_PARA3		0x001c
#define DLANE1_PARA3		0x0030
#define DLANE2_PARA3		0x0040
#define DLANE3_PARA3		0x0050
#define T_WAKEUP_D(x)		UPDATE(x, 31, 0)

#define DLANE0_PARA4		0x0020
#define T_TAGO_D0(x)		UPDATE(x, 23, 16)
#define T_TASURE_D0(x)		UPDATE(x, 15, 8)
#define T_TAGET_D0(x)		UPDATE(x, 7, 0)

#define COMMON_PARA0		0x0054
#define T_LPX(x)		UPDATE(x, 7, 0)
#define CTRL_PARA0		0x0058
#define PWON_SEL		BIT(3)
#define PWON_DSI		BIT(1)
#define SU_IDDQ_EN		BIT(0)

#define PLL_CTRL_PARA0		0x005c
#define PLL_LOCK		BIT(27)
#define RATE_MASK		GENMASK(26, 24)
#define RATE(x)			UPDATE(x, 26, 24)
#define REFCLK_DIV_MASK		GENMASK(23, 19)
#define REFCLK_DIV(x)		UPDATE(x, 23, 19)
#define PLL_DIV_MASK		GENMASK(18, 4)
#define PLL_DIV(x)		UPDATE(x, 18, 4)
#define DSI_PIXELCLK_DIV(x)	UPDATE(x, 3, 0)

#define PLL_CTRL_PARA1		0x0060
#define PLL_CTRL(x)		UPDATE(x, 31, 0)

#define RCAL_CTRL		0x0064
#define RCAL_EN			BIT(13)
#define RCAL_TRIM(x)		UPDATE(x, 12, 9)
#define RCAL_DONE		BIT(0)

#define TRIM_PARA		0x0068
#define HSTX_AMP_TRIM(x)	UPDATE(x, 13, 11)
#define LPTX_SR_TRIM(x)		UPDATE(x, 10, 8)
#define LPRX_VREF_TRIM(x)	UPDATE(x, 7, 4)
#define LPCD_VREF_TRIM(x)	UPDATE(x, 3, 0)

#define TEST_PARA0		0x006c
#define FSET_EN			BIT(3)

#define MISC_PARA		0x0074
#define CLK_MODE_MASK		BIT(11)
#define CLK_MODE_NON_CONTINUOUS	UPDATE(1, 11, 11)
#define CLK_MODE_CONTINUOUS	UPDATE(0, 11, 11)
#define LANE_NUM_MASK		GENMASK(6, 5)
#define LANE_NUM(x)		UPDATE(x, 6, 5)

#define CLANE_PARA4		0x0078
#define INTERFACE_PARA		0x007c
#define TXREADYESC_VLD(x)	UPDATE(x, 15, 8)
#define RXVALIDESC_VLD(x)	UPDATE(x, 7, 0)

/* mipi tx grf base */
#define MIPI_TX0_GRF_BASE	RKX12X_GRF_MIPI0_BASE
#define MIPI_TX1_GRF_BASE	RKX12X_GRF_MIPI1_BASE

#define MIPI_TXGRF_DEV(id)	((id) == 0 ? (MIPI_TX0_GRF_BASE) : (MIPI_TX1_GRF_BASE))

#define GRF_MIPITX_CON0		0x0000
#define PHY_SHUTDWN(x)		HIWORD_UPDATE(x, GENMASK(10, 10), 10)
#define PHY_MODE(x)		HIWORD_UPDATE(x, GENMASK(4, 3), 3)

#define GRF_MIPITX_CON1		0x0004
#define PLL_POWERON(x)		HIWORD_UPDATE(x, GENMASK(15, 15), 15)

#define GRF_MIPI_STATUS		0x0080
#define PHY_LOCK		BIT(0)

/*
 * Minimum D-PHY timings based on MIPI D-PHY specification. Derived
 * from the valid ranges specified in Section 6.9, Table 14, Page 41
 * of the D-PHY specification (v2.1).
 */
static void rkx12x_txphy_dphy_get_def_cfg(u64 data_rate, u8 lanes,
				struct cfg_opts_mipi_dphy *cfg)
{
	unsigned long long ui;

	ui = ALIGN(PSEC_PER_SEC, data_rate);
	do_div(ui, data_rate);

	cfg->clk_miss = 0;
	cfg->clk_post = 60000 + 52 * ui;
	cfg->clk_pre = 8000;
	cfg->clk_prepare = 38000;
	cfg->clk_settle = 95000;
	cfg->clk_term_en = 0;
	cfg->clk_trail = 60000;
	cfg->clk_zero = 262000;
	cfg->d_term_en = 0;
	cfg->eot = 0;
	cfg->hs_exit = 100000;
	cfg->hs_prepare = 40000 + 4 * ui;
	cfg->hs_zero = 105000 + 6 * ui;
	cfg->hs_settle = 85000 + 6 * ui;
	cfg->hs_skip = 40000;

	/*
	 * The MIPI D-PHY specification (Section 6.9, v1.2, Table 14, Page 40)
	 * contains this formula as:
	 *
	 *     T_HS-TRAIL = max(n * 8 * ui, 60 + n * 4 * ui)
	 *
	 * where n = 1 for forward-direction HS mode and n = 4 for reverse-
	 * direction HS mode. There's only one setting and this function does
	 * not parameterize on anything other that ui, so this code will
	 * assumes that reverse-direction HS mode is supported and uses n = 4.
	 */
	cfg->hs_trail = max(4 * 8 * ui, 60000 + 4 * 4 * ui);

	/*
	 * Note that TINIT is considered a protocol-dependent parameter, and
	 * thus the exact requirements for TINIT,MASTER and TINIT,SLAVE (transmitter
	 * and receiver initialization Stop state lengths, respectively,) are defined
	 * by the protocol layer specification and are outside the scope of this document.
	 * However, the D-PHY specification does place a minimum bound on the lengths of
	 * TINIT,MASTER and TINIT,SLAVE, which each shall be no less than 100 ¦Ìs. A protocol
	 * layer specification using the D-PHY specification may specify any values greater
	 * than this limit, for example, TINIT,MASTER ¡Ý 1 ms and TINIT,SLAVE = 500 to 800 ¦Ìs
	 */
	cfg->init = 100; /* us */
	cfg->lpx = 50000;
	cfg->ta_get = 5 * cfg->lpx;
	cfg->ta_go = 4 * cfg->lpx;
	cfg->ta_sure = cfg->lpx;
	cfg->wakeup = 1000; /* us */

	cfg->hs_clk_rate = data_rate;
	cfg->lanes = lanes;
}

static int rkx12x_txphy_dphy_timing_init(struct rkx12x_txphy *txphy)
{
	struct i2c_client *client = txphy->client;
	struct device *dev = &client->dev;
	u32 txphy_base = MIPI_TXPHY_DEV(txphy->phy_id); // mipi txphy base
	const struct cfg_opts_mipi_dphy cfg = txphy->mipi_dphy_cfg;
	u32 byte_clk = DIV_ROUND_CLOSEST_ULL(txphy->data_rate, 8);
	u64 t_byte_clk = DIV_ROUND_CLOSEST_ULL(PSEC_PER_SEC, byte_clk);
	u32 esc_div = DIV_ROUND_UP(byte_clk, 20 * USEC_PER_SEC);
	u32 t_clkprepare, t_clkzero, t_clkpre, t_clkpost, t_clktrail;
	u32 t_init, t_hsexit, t_hsprepare, t_hszero, t_wakeup, t_hstrail;
	u32 t_tago, t_tasure, t_taget;
	int ret = 0;

	dev_info(dev, "TXPHY: byte_clk = %d, t_byte_clk = %lld\n", byte_clk, t_byte_clk);

	dev_info(dev, "TXPHY: esc_div = %d\n", esc_div);
	ret |= txphy->i2c_reg_write(client, txphy_base + INTERFACE_PARA,
				TXREADYESC_VLD(esc_div - 2) |
				RXVALIDESC_VLD(esc_div - 2));
	ret |= txphy->i2c_reg_write(client, txphy_base + COMMON_PARA0, esc_div);
	ret |= txphy->i2c_reg_update(client, txphy_base + TEST_PARA0, FSET_EN, FSET_EN);

	t_init = DIV_ROUND_UP((u64)cfg.init * (PSEC_PER_SEC / USEC_PER_SEC), t_byte_clk) - 1;
	dev_info(dev, "TXPHY: t_init = %d\n", t_init);
	ret |= txphy->i2c_reg_write(client, txphy_base + CLANE_PARA1, T_INITTIME_C(t_init));
	ret |= txphy->i2c_reg_write(client, txphy_base + DLANE0_PARA1, T_INITTIME_D(t_init));
	ret |= txphy->i2c_reg_write(client, txphy_base + DLANE1_PARA1, T_INITTIME_D(t_init));
	ret |= txphy->i2c_reg_write(client, txphy_base + DLANE2_PARA1, T_INITTIME_D(t_init));
	ret |= txphy->i2c_reg_write(client, txphy_base + DLANE3_PARA1, T_INITTIME_D(t_init));

	t_clkprepare = DIV_ROUND_UP(cfg.clk_prepare, t_byte_clk) - 1;
	t_clkzero = DIV_ROUND_UP(cfg.clk_zero, t_byte_clk) - 1;
	t_clkpre = DIV_ROUND_UP(cfg.clk_pre, t_byte_clk) - 1;
	dev_info(dev, "TXPHY: t_clkprepare = %d\n", t_clkprepare);
	dev_info(dev, "TXPHY: t_clkzero = %d\n", t_clkzero);
	dev_info(dev, "TXPHY: txphy t_clkpre = %d\n", t_clkpre);
	ret |= txphy->i2c_reg_write(client, txphy_base + CLANE_PARA2,
				T_CLKPREPARE_C(t_clkprepare) |
				T_CLKZERO_C(t_clkzero) | T_CLKPRE_C(t_clkpre));

	t_clkpost = DIV_ROUND_UP(cfg.clk_post, t_byte_clk) - 1;
	t_clktrail = DIV_ROUND_UP(cfg.clk_trail, t_byte_clk) - 1;
	t_hsexit = DIV_ROUND_UP(cfg.hs_exit, t_byte_clk) - 1;
	dev_info(dev, "TXPHY: t_clkpost = %d\n", t_clkpost);
	dev_info(dev, "TXPHY: t_clktrail = %d\n", t_clktrail);
	dev_info(dev, "TXPHY: t_hsexit = %d\n", t_hsexit);
	ret |= txphy->i2c_reg_write(client, txphy_base + CLANE_PARA3,
				T_CLKPOST_C(t_clkpost) |
				T_CLKTRAIL_C(t_clktrail) |
				T_HSEXIT_C(t_hsexit));

	t_hsprepare = DIV_ROUND_UP(cfg.hs_prepare, t_byte_clk) - 1;
	t_hszero = DIV_ROUND_UP(cfg.hs_zero, t_byte_clk) - 1;
	t_hstrail = DIV_ROUND_UP(cfg.hs_trail, t_byte_clk) - 1;
#if (TXPHY_TIMING_ADJUST_API == 0)
	dev_info(dev, "TXPHY: t_hsprepare = %d\n", t_hsprepare);
	dev_info(dev, "TXPHY: t_hszero = %d\n", t_hszero);
	dev_info(dev, "TXPHY: t_hstrail = %d\n", t_hstrail);
	ret |= txphy->i2c_reg_write(client, txphy_base + DLANE0_PARA2,
				T_HSPREPARE_D(t_hsprepare) |
				T_HSZERO_D(t_hszero) |
				T_HSTRAIL_D(t_hstrail) |
				T_HSEXIT_D(t_hsexit));

	ret |= txphy->i2c_reg_write(client, txphy_base + DLANE1_PARA2,
				T_HSPREPARE_D(t_hsprepare) |
				T_HSZERO_D(t_hszero) |
				T_HSTRAIL_D(t_hstrail) |
				T_HSEXIT_D(t_hsexit));

	ret |= txphy->i2c_reg_write(client, txphy_base + DLANE2_PARA2,
				T_HSPREPARE_D(t_hsprepare) |
				T_HSZERO_D(t_hszero) |
				T_HSTRAIL_D(t_hstrail) |
				T_HSEXIT_D(t_hsexit));

	ret |= txphy->i2c_reg_write(client, txphy_base + DLANE3_PARA2,
				T_HSPREPARE_D(t_hsprepare) |
				T_HSZERO_D(t_hszero) |
				T_HSTRAIL_D(t_hstrail) |
				T_HSEXIT_D(t_hsexit));
#else /* TXPHY_TIMING_ADJUST_API */
{
	u32 mask = 0, val = 0;
	u32 t_hstrail_dlane = 0;

	mask = 0;
	mask |= GENMASK(31, 24); // T_HSPREPARE_D
	mask |= GENMASK(23, 16); // T_HSZERO_D
	mask |= GENMASK(15, 8); // T_HSTRAIL_D
	mask |= GENMASK(7, 0); // T_HSEXIT_D

	val = 0;
	val |= T_HSPREPARE_D(t_hsprepare);
	val |= T_HSZERO_D(t_hszero);
	t_hstrail_dlane = txphy->mipi_timing.t_hstrail_dlane0;
	if (t_hstrail_dlane & RKX12X_MIPI_TIMING_EN) {
		dev_info(dev, "TXPHY: DLane0: t_hstrail adjust by setting\n");
		t_hstrail_dlane &= (~RKX12X_MIPI_TIMING_EN);
	} else {
		t_hstrail_dlane = t_hstrail;
	}
	val |= T_HSTRAIL_D(t_hstrail_dlane);
	val |= T_HSEXIT_D(t_hsexit);

	dev_info(dev, "TXPHY: DLane0: t_hsprepare = %d\n", t_hsprepare);
	dev_info(dev, "TXPHY: DLane0: t_hszero = %d\n", t_hszero);
	dev_info(dev, "TXPHY: DLane0: t_hstrail = %d\n", t_hstrail_dlane);

	ret |= txphy->i2c_reg_update(client, txphy_base + DLANE0_PARA2, mask, val);

	val = 0;
	val |= T_HSPREPARE_D(t_hsprepare);
	val |= T_HSZERO_D(t_hszero);
	t_hstrail_dlane = txphy->mipi_timing.t_hstrail_dlane1;
	if (t_hstrail_dlane & RKX12X_MIPI_TIMING_EN) {
		dev_info(dev, "TXPHY: DLane1: t_hstrail adjust by setting\n");
		t_hstrail_dlane &= (~RKX12X_MIPI_TIMING_EN);
	} else {
		t_hstrail_dlane = t_hstrail;
	}
	val |= T_HSTRAIL_D(t_hstrail_dlane);
	val |= T_HSEXIT_D(t_hsexit);

	dev_info(dev, "TXPHY: DLane1: t_hsprepare = %d\n", t_hsprepare);
	dev_info(dev, "TXPHY: DLane1: t_hszero = %d\n", t_hszero);
	dev_info(dev, "TXPHY: DLane1: t_hstrail = %d\n", t_hstrail_dlane);

	ret |= txphy->i2c_reg_update(client, txphy_base + DLANE1_PARA2, mask, val);

	val = 0;
	val |= T_HSPREPARE_D(t_hsprepare);
	val |= T_HSZERO_D(t_hszero);
	t_hstrail_dlane = txphy->mipi_timing.t_hstrail_dlane2;
	if (t_hstrail_dlane & RKX12X_MIPI_TIMING_EN) {
		dev_info(dev, "TXPHY: DLane2: t_hstrail adjust by setting\n");
		t_hstrail_dlane &= (~RKX12X_MIPI_TIMING_EN);
	} else {
		t_hstrail_dlane = t_hstrail;
	}
	val |= T_HSTRAIL_D(t_hstrail_dlane);
	val |= T_HSEXIT_D(t_hsexit);

	dev_info(dev, "TXPHY: DLane2: t_hsprepare = %d\n", t_hsprepare);
	dev_info(dev, "TXPHY: DLane2: t_hszero = %d\n", t_hszero);
	dev_info(dev, "TXPHY: DLane2: t_hstrail = %d\n", t_hstrail_dlane);

	ret |= txphy->i2c_reg_update(client, txphy_base + DLANE2_PARA2, mask, val);

	val = 0;
	val |= T_HSPREPARE_D(t_hsprepare);
	val |= T_HSZERO_D(t_hszero);
	t_hstrail_dlane = txphy->mipi_timing.t_hstrail_dlane3;
	if (t_hstrail_dlane & RKX12X_MIPI_TIMING_EN) {
		dev_info(dev, "TXPHY: DLane3: t_hstrail adjust by setting\n");
		t_hstrail_dlane &= (~RKX12X_MIPI_TIMING_EN);
	} else {
		t_hstrail_dlane = t_hstrail;
	}
	val |= T_HSTRAIL_D(t_hstrail_dlane);
	val |= T_HSEXIT_D(t_hsexit);

	dev_info(dev, "TXPHY: DLane3: t_hsprepare = %d\n", t_hsprepare);
	dev_info(dev, "TXPHY: DLane3: t_hszero = %d\n", t_hszero);
	dev_info(dev, "TXPHY: DLane3: t_hstrail = %d\n", t_hstrail_dlane);

	ret |= txphy->i2c_reg_update(client, txphy_base + DLANE3_PARA2, mask, val);
}
#endif /* TXPHY_TIMING_ADJUST_API */

	t_wakeup = DIV_ROUND_UP((u64)cfg.wakeup * (PSEC_PER_SEC / USEC_PER_SEC), t_byte_clk) - 1;
	dev_info(dev, "TXPHY: t_wakeup = %d\n", t_wakeup);
	ret |= txphy->i2c_reg_write(client, txphy_base + DLANE0_PARA3, T_WAKEUP_D(t_wakeup));
	ret |= txphy->i2c_reg_write(client, txphy_base + DLANE1_PARA3, T_WAKEUP_D(t_wakeup));
	ret |= txphy->i2c_reg_write(client, txphy_base + DLANE2_PARA3, T_WAKEUP_D(t_wakeup));
	ret |= txphy->i2c_reg_write(client, txphy_base + DLANE3_PARA3, T_WAKEUP_D(t_wakeup));
	ret |= txphy->i2c_reg_write(client, txphy_base + CLANE_PARA4, T_WAKEUP_D(t_wakeup));

	t_tago = DIV_ROUND_UP(cfg.ta_go, t_byte_clk) - 1;
	t_tasure = DIV_ROUND_UP(cfg.ta_sure, t_byte_clk) - 1;
	t_taget = DIV_ROUND_UP(cfg.ta_get, t_byte_clk) - 1;
	dev_info(dev, "TXPHY: t_tago = %d\n", t_tago);
	dev_info(dev, "TXPHY: t_tasure = %d\n", t_tasure);
	dev_info(dev, "TXPHY: t_taget = %d\n", t_taget);
	ret |= txphy->i2c_reg_write(client, txphy_base + DLANE0_PARA4,
				T_TAGO_D0(t_tago) |
				T_TASURE_D0(t_tasure) |
				T_TAGET_D0(t_taget));

	return ret;
}

static int rkx12x_txphy_dphy_pll_set(struct rkx12x_txphy *txphy)
{
	struct i2c_client *client = txphy->client;
	u32 txphy_base = MIPI_TXPHY_DEV(txphy->phy_id); // mipi txphy base
	u32 mask, val;
	int ret = 0;

	mask = RATE_MASK | REFCLK_DIV_MASK | PLL_DIV_MASK;
	val = RATE(txphy->mipi_pll.rate_factor) |
			REFCLK_DIV(txphy->mipi_pll.ref_div) |
			PLL_DIV(txphy->mipi_pll.fb_div);
	ret = txphy->i2c_reg_update(client, txphy_base + PLL_CTRL_PARA0, mask, val);

	return ret;
}

static u64 rkx12x_txphy_dphy_get_pll_cfg(struct rkx12x_mipi_pll *mipi_pll, u64 data_rate)
{
	const struct {
		unsigned long max_lane_mbps;
		u8 refclk_div;
		u8 post_factor;
	} hsfreqrange_table[] = {
		{  100, 0, 5 },
		{  200, 0, 4 },
		{  400, 0, 3 },
		{  800, 0, 2 },
		{ 1600, 0, 1 },
	};
	u64 ref_clk = 24 * USEC_PER_SEC;
	u64 fvco;
	u16 fb_div;
	u8 ref_div, post_div, index;

	for (index = 0; index < ARRAY_SIZE(hsfreqrange_table); index++)
		if (data_rate <= hsfreqrange_table[index].max_lane_mbps * USEC_PER_SEC)
			break;

	if (index == ARRAY_SIZE(hsfreqrange_table))
		--index;

	/*
	 * FVCO = Fckref * 8 * (PI_POST_DIV + PF_POST_DIV) / (R + 1)
	 * data_rate = FVCO / post_div;
	 */

	ref_div = hsfreqrange_table[index].refclk_div;
	post_div = 1 << hsfreqrange_table[index].post_factor;
	fvco = data_rate * post_div;
	fb_div = DIV_ROUND_CLOSEST_ULL((fvco * (ref_div + 1)) << 10, ref_clk * 8);

	data_rate = DIV_ROUND_CLOSEST_ULL(ref_clk * 8 * fb_div,
					(u64)((ref_div + 1) * post_div) << 10);

	mipi_pll->ref_div = ref_div;
	mipi_pll->fb_div = fb_div;
	mipi_pll->rate_factor = hsfreqrange_table[index].post_factor;

#if 0
	printk("index = %d, data rate = %lld\n", index, data_rate);
	printk("ref_div = %d, fb_div = %d, post_div = %d, rate_factor = %d\n",
			mipi_pll->ref_div, mipi_pll->fb_div,
			post_div, mipi_pll->rate_factor);
#endif

	return data_rate;
}

static int rkx12x_txphy_csi_pll_init(struct rkx12x_txphy *txphy)
{
	int ret = 0;

	dev_info(&txphy->client->dev, "TXPHY: mipi pll init freq = %lld\n", txphy->clock_freq);

	/* mipi txphy pll calc by data rate */
	txphy->data_rate = rkx12x_txphy_dphy_get_pll_cfg(&txphy->mipi_pll, txphy->clock_freq * 2);

	/* get default config by data rate */
	rkx12x_txphy_dphy_get_def_cfg(txphy->data_rate, txphy->data_lanes, &txphy->mipi_dphy_cfg);

	/* mipi txphy time init */
	ret |= rkx12x_txphy_dphy_timing_init(txphy);

	/* mipi txphy pll set */
	ret |= rkx12x_txphy_dphy_pll_set(txphy);
	if (ret)
		return ret;

	return 0;
}

static int rkx12x_txphy_csi_init(struct rkx12x_txphy *txphy)
{
	struct i2c_client *client = txphy->client;
	struct device *dev = &client->dev;
	u32 grf_mipi_base = MIPI_TXGRF_DEV(txphy->phy_id);
	u32 mipi_txphy_base = MIPI_TXPHY_DEV(txphy->phy_id);
	u32 reg, mask = 0, val = 0;
	int ret = 0;

	/* select MIPI */
	dev_info(dev, "TXPHY: Select MIPI\n");
	reg = grf_mipi_base + GRF_MIPITX_CON0;
	val = PHY_MODE(2);
	ret |= txphy->i2c_reg_write(client, reg, val);

	/* mipi phy0 select: csi */
	/* note: mipi phy0 support csi and dsi, mipi phy1 only support csi */
	if (txphy->phy_id == RKX12X_TXPHY_ID0) {
		dev_info(dev, "TXPHY0: Select CSI\n");

		reg = DES_GRF_SOC_CON5;
		val = ((1 << 8) << 16) | (1 << 8); // bit8
		ret |= txphy->i2c_reg_write(client, reg, val);
	}

	/* data lane number config */
	reg = mipi_txphy_base + MISC_PARA;
	mask = LANE_NUM_MASK;
	val = LANE_NUM(txphy->data_lanes - 1);
	ret |= txphy->i2c_reg_update(client, reg, mask, val);

	/* non-continuous mode */
	if (txphy->clock_mode == RKX12X_TXPHY_CLK_NON_CONTINUOUS) {
		dev_info(dev, "TXPHY: non-continuous mode.\n");
		/*
		 * if mipi dphy set non-continuous, csi_tx also should be set.
		 */
		reg = mipi_txphy_base + MISC_PARA;
		mask = CLK_MODE_MASK;
		val = CLK_MODE_NON_CONTINUOUS;
		ret |= txphy->i2c_reg_update(client, reg, mask, val);
	}
	if (ret)
		return ret;

	return 0;
}

int rkx12x_txphy_csi_enable(struct rkx12x_txphy *txphy, u64 hs_clk_freq)
{
	struct i2c_client *client = txphy->client;
	struct device *dev = &client->dev;
	u32 grf_mipi_base = MIPI_TXGRF_DEV(txphy->phy_id);
	u32 mipi_txphy_base = MIPI_TXPHY_DEV(txphy->phy_id);
	u32 reg, val = 0;
	int ret = 0;

	if (hs_clk_freq == 0) {
		dev_info(dev, "TXPHY: clock freq error\n");
		return -EINVAL;
	}

	txphy->clock_freq = hs_clk_freq;

	ret = rkx12x_txphy_csi_pll_init(txphy);
	if (ret)
		return ret;

	/* DPHY power on */
	dev_info(dev, "TXPHY: DPHY PowerOn\n");
	reg = grf_mipi_base + GRF_MIPITX_CON0;
	val = PHY_SHUTDWN(1);
	ret |= txphy->i2c_reg_write(client, reg, val);

	/* PLL power on */
	dev_info(dev, "TXPHY: PLL PowerOn\n");
	reg = grf_mipi_base + GRF_MIPITX_CON1;
	val = PLL_POWERON(1);
	ret |= txphy->i2c_reg_write(client, reg, val);
	if (ret)
		return ret;

	ret = read_poll_timeout(txphy->i2c_reg_read, ret,
				!(ret < 0) && (val & PLL_LOCK),
				0, MSEC_PER_SEC, false, client,
				mipi_txphy_base + PLL_CTRL_PARA0,
				&val);
	if (ret < 0)
		dev_err(dev, "TXPHY: PLL is not locked\n");
	else
		dev_info(dev, "TXPHY: PLL is locked\n");

	return ret;
}

int rkx12x_txphy_csi_disable(struct rkx12x_txphy *txphy)
{
	struct i2c_client *client = txphy->client;
	struct device *dev = &client->dev;
	u32 grf_mipi_base = MIPI_TXGRF_DEV(txphy->phy_id);
	int ret = 0;

	dev_info(dev, "TXPHY: PLL PowerDown\n");
	ret |= txphy->i2c_reg_write(client,
			grf_mipi_base + GRF_MIPITX_CON1, PLL_POWERON(0));

	dev_info(dev, "TXPHY: DPHY PowerDown\n");
	ret |= txphy->i2c_reg_write(client,
			grf_mipi_base + GRF_MIPITX_CON0, PHY_SHUTDWN(0));

	return ret;
}

static int rkx12x_txphy_dvp_init(struct rkx12x_txphy *txphy)
{
	struct i2c_client *client = txphy->client;
	struct device *dev = &client->dev;
	u32 grf_mipi_base = RKX12X_GRF_MIPI0_BASE; /* dvp mode fixed txphy0 */
	u32 reg, val = 0;
	int ret = 0;

	/* select GPIO */
	dev_info(dev, "TXPHY: Select GPIO\n");
	reg = grf_mipi_base + GRF_MIPITX_CON0;
	val = PHY_MODE(0);
	ret |= txphy->i2c_reg_write(client, reg, val);

	/* txphy ie */
	reg = DES_GRF_GPIO1A_IE;
	val = 0xffff0000;
	ret |= txphy->i2c_reg_write(client, reg, val);

	reg = DES_GRF_GPIO1B_IE;
	val = 0xffff0000;
	ret |= txphy->i2c_reg_write(client, reg, val);

	/* txphy oe */
	reg = RKX12X_GPIO1_TX_BASE + 0x08;
	val = 0xffffffff;
	ret |= txphy->i2c_reg_write(client, reg, val);

	reg = RKX12X_GPIO1_TX_BASE + 0x0c;
	val = 0xffffffff;
	ret |= txphy->i2c_reg_write(client, reg, val);

	return ret;
}

int rkx12x_txphy_init(struct rkx12x_txphy *txphy)
{
	int ret = 0;

	if ((txphy == NULL) || (txphy->client == NULL)
			|| (txphy->i2c_reg_read == NULL)
			|| (txphy->i2c_reg_write == NULL)
			|| (txphy->i2c_reg_update == NULL))
		return -EINVAL;

	dev_info(&txphy->client->dev, "=== rkx12x txphy init ===\n");

	switch (txphy->phy_mode) {
	case RKX12X_TXPHY_MODE_MIPI:
		// txphy csi init
		ret = rkx12x_txphy_csi_init(txphy);
		break;
	case RKX12X_TXPHY_MODE_GPIO:
		// txphy dvp init
		ret = rkx12x_txphy_dvp_init(txphy);
		break;
	default:
		dev_info(&txphy->client->dev, "txphy phy mode = %d error\n",
			txphy->phy_mode);
		ret = -1;
		break;
	}

	return ret;
}
