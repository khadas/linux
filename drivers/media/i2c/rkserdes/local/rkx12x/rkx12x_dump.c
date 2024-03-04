// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2024 Rockchip Electronics Co. Ltd.
 *
 * Author: Cai Wenzhong <cwz@rock-chips.com>
 */

#include "rkx12x_drv.h"
#include "rkx12x_dump.h"
#include "rkx12x_reg.h"

#define rkx12x_print(fmt, ...)				\
({							\
	pr_info("[RKX12X] " fmt, ##__VA_ARGS__);	\
})

static void rkx12x_grf_reg_dump(struct rkx12x *rkx12x)
{
	struct i2c_client *client = rkx12x->client;
	u32 reg = 0;
	u32 offset = 0;
	u32 val = 0, bits_val = 0;

	/* DES_GRF_SOC_CON3 */
	reg = DES_GRF_SOC_CON3;
	offset = reg - RKX12X_DES_GRF_BASE;
	rkx12x->i2c_reg_read(client, reg, &val);
	rkx12x_print("DES_GRF_SOC_CON3(0x%08x) = 0x%08x\n", offset, val);
	if (val & BIT(0))
		rkx12x_print("    cif dclk invert\n");
	else
		rkx12x_print("    cif dclk bypass\n");

	bits_val = (val & GENMASK(8, 1)) >> 1;
	rkx12x_print("    cif dclk delay line number = %d\n", bits_val);

	if (val & BIT(9))
		rkx12x_print("    cif dclk bypass enable\n");
	else
		rkx12x_print("    cif dclk use clock delayline\n");

	/* DES_GRF_SOC_CON4 */
	reg = DES_GRF_SOC_CON4;
	offset = reg - RKX12X_DES_GRF_BASE;
	rkx12x->i2c_reg_read(client, reg, &val);
	rkx12x_print("DES_GRF_SOC_CON4(0x%08x) = 0x%08x\n", offset, val);
	if (val & BIT(10))
		rkx12x_print("    PMA0 enable\n");
	if (val & BIT(11))
		rkx12x_print("    PMA1 enable\n");

	/* DES_GRF_SOC_CON5 */
	reg = DES_GRF_SOC_CON5;
	offset = reg - RKX12X_DES_GRF_BASE;
	rkx12x->i2c_reg_read(client, reg, &val);
	rkx12x_print("DES_GRF_SOC_CON5(0x%08x) = 0x%08x\n", offset, val);
	if (val & BIT(8))
		rkx12x_print("    MIPI PHY0: CSI\n");
	else
		rkx12x_print("    MIPI PHY0: DSI\n");

	/* DES_GRF_IRQ_EN */
	reg = DES_GRF_IRQ_EN;
	offset = reg - RKX12X_DES_GRF_BASE;
	rkx12x->i2c_reg_read(client, reg, &val);
	rkx12x_print("DES_GRF_IRQ_EN(0x%08x) = 0x%08x\n", offset, val);

	/* DES_GRF_SOC_STATUS0 */
	reg = DES_GRF_SOC_STATUS0;
	offset = reg - RKX12X_DES_GRF_BASE;
	rkx12x->i2c_reg_read(client, reg, &val);
	rkx12x_print("DES_GRF_SOC_STATUS0(0x%08x) = 0x%08x\n", offset, val);
	if (val & DES_PCS0_READY)
		rkx12x_print("    des pcs0 ready\n");
	if (val & DES_PCS1_READY)
		rkx12x_print("    des pcs1 ready\n");
}

static void rkx12x_mipi_grf_reg_dump(struct rkx12x *rkx12x, u8 mipi_phy_id)
{
	struct i2c_client *client = rkx12x->client;
	u32 mipi_grf_base = 0;
	u32 reg = 0;
	u32 offset = 0;
	u32 val = 0, bits_val = 0;
	u32 i = 0;

	mipi_grf_base = mipi_phy_id ? RKX12X_GRF_MIPI1_BASE : RKX12X_GRF_MIPI0_BASE;

	/* GRF_MIPI_TX_CON0 */
	offset = 0x0000;
	reg = mipi_grf_base + offset;
	rkx12x->i2c_reg_read(client, reg, &val);
	rkx12x_print("GRF_MIPI_TX_CON0(0x%08x) = 0x%08x\n", offset, val);
	bits_val = (val & GENMASK(2, 1)) >> 1;
	rkx12x_print("    LVDS post divider = %d\n", bits_val);
	bits_val = (val & GENMASK(4, 3)) >> 3;
	if (bits_val == 0)
		rkx12x_print("    PHY mode: GPIO\n");
	else if (bits_val == 1)
		rkx12x_print("    PHY mode: LVDS\n");
	else if (bits_val == 2)
		rkx12x_print("    PHY mode: MIPI\n");
	else if (bits_val == 3)
		rkx12x_print("    PHY mode: mini-LVDS\n");
	if (val & BIT(5))
		rkx12x_print("    IDDQ test mode enable\n");

	bits_val = (val & GENMASK(9, 6)) >> 6;
	rkx12x_print("    refclk_div = %d\n", bits_val);

	if (val & BIT(10))
		rkx12x_print("    D-PHY Shut Down\n");

	/* GRF_MIPI_TX_CON1 */
	offset = 0x0004;
	reg = mipi_grf_base + offset;
	rkx12x->i2c_reg_read(client, reg, &val);
	rkx12x_print("GRF_MIPI_TX_CON1(0x%08x) = 0x%08x\n", offset, val);
	bits_val = (val & GENMASK(14, 0)) >> 0;
	rkx12x_print("    pll_div = %d\n", bits_val);
	if (val & BIT(15))
		rkx12x_print("    PLL Power On\n");
	else
		rkx12x_print("    PLL Power Off\n");

	/* GRF_MIPI_TX_CON2 */
	offset = 0x0008;
	reg = mipi_grf_base + offset;
	rkx12x->i2c_reg_read(client, reg, &val);
	rkx12x_print("GRF_MIPI_TX_CON2(0x%08x) = 0x%08x\n", offset, val);
	bits_val = (val & GENMASK(15, 0)) >> 0;
	rkx12x_print("    pll_ctrl_low = %d\n", bits_val);

	/* GRF_MIPI_TX_CON3 */
	offset = 0x000c;
	reg = mipi_grf_base + offset;
	rkx12x->i2c_reg_read(client, reg, &val);
	rkx12x_print("GRF_MIPI_TX_CON3(0x%08x) = 0x%08x\n", offset, val);
	bits_val = (val & GENMASK(15, 0)) >> 0;
	rkx12x_print("    pll_ctrl_high = %d\n", bits_val);

	/* GRF_MIPI_TX_CON4 */
	offset = 0x0010;
	reg = mipi_grf_base + offset;
	rkx12x->i2c_reg_read(client, reg, &val);
	rkx12x_print("GRF_MIPI_TX_CON4(0x%08x) = 0x%08x\n", offset, val);
	bits_val = (val & GENMASK(3, 0)) >> 0;
	rkx12x_print("    data lane0 output voltage = %d\n", bits_val);
	bits_val = (val & GENMASK(7, 4)) >> 4;
	rkx12x_print("    data lane1 output voltage = %d\n", bits_val);
	bits_val = (val & GENMASK(11, 8)) >> 8;
	rkx12x_print("    data lane2 output voltage = %d\n", bits_val);
	bits_val = (val & GENMASK(15, 12)) >> 12;
	rkx12x_print("    data lane3 output voltage = %d\n", bits_val);

	/* GRF_MIPI_TX_CON5 */
	offset = 0x0014;
	reg = mipi_grf_base + offset;
	rkx12x->i2c_reg_read(client, reg, &val);
	rkx12x_print("GRF_MIPI_TX_CON5(0x%08x) = 0x%08x\n", offset, val);
	bits_val = (val & GENMASK(2, 0)) >> 0;
	rkx12x_print("    data lane0 parallel data bus width = %d\n", bits_val);
	bits_val = (val & GENMASK(5, 3)) >> 3;
	rkx12x_print("    data lane1 parallel data bus width = %d\n", bits_val);
	bits_val = (val & GENMASK(8, 6)) >> 6;
	rkx12x_print("    data lane2 parallel data bus width = %d\n", bits_val);
	bits_val = (val & GENMASK(11, 9)) >> 9;
	rkx12x_print("    data lane3 parallel data bus width = %d\n", bits_val);
	bits_val = (val & GENMASK(14, 12)) >> 0;
	rkx12x_print("    clock lane parallel data bus width = %d\n", bits_val);

	/* GRF_MIPI_TX_CON6 - GRF_MIPI_TX_CON9 */
	for (i = 0; i < 4; i++) {
		offset = 0x0018 + 4 * i;
		reg = mipi_grf_base + offset;
		rkx12x->i2c_reg_read(client, reg, &val);
		rkx12x_print("GRF_MIPI_TX_CON%d(0x%08x) = 0x%08x\n", 6 + i, offset, val);
		bits_val = (val & GENMASK(15, 0)) >> 0;
		rkx12x_print("    data lane%d control low = %d\n", i, bits_val);
	}

	/* GRF_MIPI_TX_CON10 */
	offset = 0x0028;
	reg = mipi_grf_base + offset;
	rkx12x->i2c_reg_read(client, reg, &val);
	rkx12x_print("GRF_MIPI_TX_CON10(0x%08x) = 0x%08x\n", offset, val);
		bits_val = (val & GENMASK(15, 0)) >> 0;
		rkx12x_print("    clock lane control low = %d\n", bits_val);

	/* GRF_MIPI_TX_CON11 */
	offset = 0x002c;
	reg = mipi_grf_base + offset;
	rkx12x->i2c_reg_read(client, reg, &val);
	rkx12x_print("GRF_MIPI_TX_CON11(0x%08x) = 0x%08x\n", offset, val);
	bits_val = (val & GENMASK(3, 0)) >> 0;
	rkx12x_print("    data lane0 control high = %d\n", bits_val);
	bits_val = (val & GENMASK(7, 4)) >> 4;
	rkx12x_print("    data lane1 control high = %d\n", bits_val);
	bits_val = (val & GENMASK(11, 8)) >> 8;
	rkx12x_print("    data lane2 control high = %d\n", bits_val);
	bits_val = (val & GENMASK(15, 12)) >> 12;
	rkx12x_print("    data lane3 control high = %d\n", bits_val);

	/* GRF_MIPI_TX_CON14 */
	offset = 0x0038;
	reg = mipi_grf_base + offset;
	rkx12x->i2c_reg_read(client, reg, &val);
	rkx12x_print("GRF_MIPI_TX_CON14(0x%08x) = 0x%08x\n", offset, val);
	if (val & BIT(10))
		rkx12x_print("    data lane0 Power down\n");
	if (val & BIT(11))
		rkx12x_print("    data lane1 Power down\n");
	if (val & BIT(12))
		rkx12x_print("    data lane2 Power down\n");
	if (val & BIT(13))
		rkx12x_print("    data lane3 Power down\n");
	if (val & BIT(14))
		rkx12x_print("    clock lane Power down\n");

	/* GRF_MIPI_SOC_STATUS */
	offset = 0x0580;
	reg = mipi_grf_base + offset;
	rkx12x->i2c_reg_read(client, reg, &val);
	rkx12x_print("GRF_MIPI_SOC_STATUS(0x%08x) = 0x%08x\n", offset, val);
	if (val & BIT(0))
		rkx12x_print("    PHY PLL locked\n");
	else
		rkx12x_print("    PHY PLL unlocked\n");
}

static void rkx12x_mipitxphy_reg_dump(struct rkx12x *rkx12x, u8 mipi_phy_id)
{
	struct i2c_client *client = rkx12x->client;
	u32 mipi_phy_base = 0;
	u32 reg = 0;
	u32 offset = 0;
	u32 val = 0, bits_val = 0;
	u32 i = 0;

	mipi_phy_base = mipi_phy_id ? RKX12X_MIPI_LVDS_TX_PHY1_BASE : RKX12X_MIPI_LVDS_TX_PHY0_BASE;

	/* LVDSMIPITX_CLANE_PARA0 */
	offset = 0x0000;
	reg = mipi_phy_base + offset;
	rkx12x->i2c_reg_read(client, reg, &val);
	rkx12x_print("LVDSMIPITX_CLANE_PARA0(0x%08x) = 0x%08x\n", offset, val);
	bits_val = (val & GENMASK(15, 0)) >> 0;
	rkx12x_print("    t_rst2enlptx_c = %d\n", bits_val);

	/* LVDSMIPITX_CLANE_PARA1 */
	offset = 0x0004;
	reg = mipi_phy_base + offset;
	rkx12x->i2c_reg_read(client, reg, &val);
	rkx12x_print("LVDSMIPITX_CLANE_PARA1(0x%08x) = 0x%08x\n", offset, val);
	bits_val = (val & GENMASK(15, 0)) >> 0;
	rkx12x_print("    t_inittime_c = %d\n", bits_val);

	/* LVDSMIPITX_CLANE_PARA2 */
	offset = 0x0008;
	reg = mipi_phy_base + offset;
	rkx12x->i2c_reg_read(client, reg, &val);
	rkx12x_print("LVDSMIPITX_CLANE_PARA2(0x%08x) = 0x%08x\n", offset, val);
	bits_val = (val & GENMASK(7, 0)) >> 0;
	rkx12x_print("    t_clkpre_c = %d\n", bits_val);
	bits_val = (val & GENMASK(15, 8)) >> 8;
	rkx12x_print("    t_clkzero_c = %d\n", bits_val);
	bits_val = (val & GENMASK(23, 16)) >> 16;
	rkx12x_print("    t_clkprepare_c = %d\n", bits_val);

	/* LVDSMIPITX_CLANE_PARA3 */
	offset = 0x000c;
	reg = mipi_phy_base + offset;
	rkx12x->i2c_reg_read(client, reg, &val);
	rkx12x_print("LVDSMIPITX_CLANE_PARA3(0x%08x) = 0x%08x\n", offset, val);
	bits_val = (val & GENMASK(7, 0)) >> 0;
	rkx12x_print("    t_hsexit_c = %d\n", bits_val);
	bits_val = (val & GENMASK(15, 8)) >> 8;
	rkx12x_print("    t_clktrail_c = %d\n", bits_val);
	bits_val = (val & GENMASK(23, 16)) >> 16;
	rkx12x_print("    t_clkpost_c = %d\n", bits_val);

	/* LVDSMIPITX_DLANE0_PARA0 - LVDSMIPITX_DLANE0_PARA4 */
	offset = 0x0010;
	reg = mipi_phy_base + offset;
	rkx12x->i2c_reg_read(client, reg, &val);
	rkx12x_print("LVDSMIPITX_DLANE0_PARA0(0x%08x) = 0x%08x\n", offset, val);
	bits_val = (val & GENMASK(15, 0)) >> 0;
	rkx12x_print("    t_rst2enlptx_d0 = %d\n", bits_val);

	offset = 0x0014;
	reg = mipi_phy_base + offset;
	rkx12x->i2c_reg_read(client, reg, &val);
	rkx12x_print("LVDSMIPITX_DLANE0_PARA1(0x%08x) = 0x%08x\n", offset, val);
	bits_val = (val & GENMASK(15, 0)) >> 0;
	rkx12x_print("    t_inittime_d0 = %d\n", bits_val);

	offset = 0x0018;
	reg = mipi_phy_base + offset;
	rkx12x->i2c_reg_read(client, reg, &val);
	rkx12x_print("LVDSMIPITX_DLANE0_PARA2(0x%08x) = 0x%08x\n", offset, val);
	bits_val = (val & GENMASK(7, 0)) >> 0;
	rkx12x_print("    t_hsexit_d0 = %d\n", bits_val);
	bits_val = (val & GENMASK(15, 8)) >> 8;
	rkx12x_print("    t_hstrail_d0 = %d\n", bits_val);
	bits_val = (val & GENMASK(23, 16)) >> 16;
	rkx12x_print("    t_hszero_d0 = %d\n", bits_val);
	bits_val = (val & GENMASK(31, 24)) >> 24;
	rkx12x_print("    t_hsprepare_d0 = %d\n", bits_val);

	offset = 0x001c;
	reg = mipi_phy_base + offset;
	rkx12x->i2c_reg_read(client, reg, &val);
	rkx12x_print("LVDSMIPITX_DLANE0_PARA3(0x%08x) = 0x%08x\n", offset, val);
	bits_val = (val & GENMASK(31, 0)) >> 0;
	rkx12x_print("    t_wakeup_d0 = %d\n", bits_val);

	offset = 0x0020;
	reg = mipi_phy_base + offset;
	rkx12x->i2c_reg_read(client, reg, &val);
	rkx12x_print("LVDSMIPITX_DLANE0_PARA4(0x%08x) = 0x%08x\n", offset, val);
	bits_val = (val & GENMASK(7, 0)) >> 0;
	rkx12x_print("    t_taget_d0 = %d\n", bits_val);
	bits_val = (val & GENMASK(15, 8)) >> 8;
	rkx12x_print("    t_tasure_d0 = %d\n", bits_val);
	bits_val = (val & GENMASK(23, 16)) >> 16;
	rkx12x_print("    t_tago_d0 = %d\n", bits_val);

	/* LVDSMIPITX_DLANE1_PARA0 - LVDSMIPITX_DLANE1_PARA3 */
	/* LVDSMIPITX_DLANE2_PARA0 - LVDSMIPITX_DLANE2_PARA3 */
	/* LVDSMIPITX_DLANE3_PARA0 - LVDSMIPITX_DLANE3_PARA3 */
	for (i = 1; i < 4; i++) {
		offset = 0x0024 + 16 * i;
		reg = mipi_phy_base + offset;
		rkx12x->i2c_reg_read(client, reg, &val);
		rkx12x_print("LVDSMIPITX_DLANE%d_PARA0(0x%08x) = 0x%08x\n", i, offset, val);
		bits_val = (val & GENMASK(15, 0)) >> 0;
		rkx12x_print("    t_rst2enlptx_d%d = %d\n", i, bits_val);

		offset = 0x0028 + 16 * i;
		reg = mipi_phy_base + offset;
		rkx12x->i2c_reg_read(client, reg, &val);
		rkx12x_print("LVDSMIPITX_DLANE%d_PARA1(0x%08x) = 0x%08x\n", i, offset, val);
		bits_val = (val & GENMASK(15, 0)) >> 0;
		rkx12x_print("    t_inittime_d%d = %d\n", i, bits_val);

		offset = 0x002c + 16 * i;
		reg = mipi_phy_base + offset;
		rkx12x->i2c_reg_read(client, reg, &val);
		rkx12x_print("LVDSMIPITX_DLANE%d_PARA2(0x%08x) = 0x%08x\n", i, offset, val);
		bits_val = (val & GENMASK(7, 0)) >> 0;
		rkx12x_print("    t_hsexit_d%d = %d\n", i, bits_val);
		bits_val = (val & GENMASK(15, 8)) >> 8;
		rkx12x_print("    t_hstrail_d%d = %d\n", i, bits_val);
		bits_val = (val & GENMASK(23, 16)) >> 16;
		rkx12x_print("    t_hszero_d%d = %d\n", i, bits_val);
		bits_val = (val & GENMASK(31, 24)) >> 24;
		rkx12x_print("    t_hsprepare_d%d = %d\n", i, bits_val);

		offset = 0x0030 + 16 * i;
		reg = mipi_phy_base + offset;
		rkx12x->i2c_reg_read(client, reg, &val);
		rkx12x_print("LVDSMIPITX_DLANE%d_PARA3(0x%08x) = 0x%08x\n", i, offset, val);
		bits_val = (val & GENMASK(31, 0)) >> 0;
		rkx12x_print("    t_wakeup_d%d = %d\n", i, bits_val);
	}

	/* LVDSMIPITX_COMMON_PARA0 */
	offset = 0x0054;
	reg = mipi_phy_base + offset;
	rkx12x->i2c_reg_read(client, reg, &val);
	rkx12x_print("LVDSMIPITX_COMMON_PARA0(0x%08x) = 0x%08x\n", offset, val);
	bits_val = (val & GENMASK(7, 0)) >> 0;
	rkx12x_print("    t_lpx = %d\n", bits_val);

	/* LVDSMIPITX_CTRL_PARA0 */
	offset = 0x0058;
	reg = mipi_phy_base + offset;
	rkx12x->i2c_reg_read(client, reg, &val);
	rkx12x_print("LVDSMIPITX_CTRL_PARA0(0x%08x) = 0x%08x\n", offset, val);
	if (val & BIT(3)) {
		rkx12x_print("    PMA Power-on: DPHY PPI interface\n");
	} else {
		rkx12x_print("    PMA Power-on: software\n");

		if (val & BIT(0))
			rkx12x_print("    su_iddq_en\n");
		if (val & BIT(1))
			rkx12x_print("    pwon_dsi\n");
		if (val & BIT(2))
			rkx12x_print("    pwon_pll\n");
	}
	if (val & BIT(4))
		rkx12x_print("    Lane-0: LP-CD enable\n");
	if (val & BIT(5))
		rkx12x_print("    Lane-0: LP-RX enable\n");
	if (val & BIT(6))
		rkx12x_print("    Lane-0: ULP-RX enable\n");

	if (val & BIT(7))
		rkx12x_print("    reference voltage is ready\n");
	else
		rkx12x_print("    reference voltage is not ready\n");

	/* LVDSMIPITX_PLL_CTRL_PARA0 */
	offset = 0x005c;
	reg = mipi_phy_base + offset;
	rkx12x->i2c_reg_read(client, reg, &val);
	rkx12x_print("LVDSMIPITX_PLL_CTRL_PARA0(0x%08x) = 0x%08x\n", offset, val);
	bits_val = (val & GENMASK(3, 0)) >> 0;
	rkx12x_print("    dsi_pixelclk_div = %d\n", bits_val);
	bits_val = (val & GENMASK(18, 4)) >> 4;
	rkx12x_print("    pll_div = %d\n", bits_val);
	bits_val = (val & GENMASK(23, 19)) >> 19;
	rkx12x_print("    refclk_div = %d\n", bits_val);
	bits_val = (val & GENMASK(26, 24)) >> 24;
	rkx12x_print("    rate = %d\n", bits_val);
	if (val & BIT(27))
		rkx12x_print("    PLL is locked\n");
	else
		rkx12x_print("    PLL is not locked\n");

	/* LVDSMIPITX_PLL_CTRL_PARA1 */
	offset = 0x0060;
	reg = mipi_phy_base + offset;
	rkx12x->i2c_reg_read(client, reg, &val);
	rkx12x_print("LVDSMIPITX_PLL_CTRL_PARA1(0x%08x) = 0x%08x\n", offset, val);

	/* LVDSMIPITX_RCAL_CTRL */
	offset = 0x0064;
	reg = mipi_phy_base + offset;
	rkx12x->i2c_reg_read(client, reg, &val);
	rkx12x_print("LVDSMIPITX_RCAL_CTRL(0x%08x) = 0x%08x\n", offset, val);
	if (val & BIT(0))
		rkx12x_print("    rcal_done\n");
	bits_val = (val & GENMASK(8, 1)) >> 1;
	rkx12x_print("    rcal_ctrl = %d\n", bits_val);
	if (val & BIT(13)) {
		rkx12x_print("    HS-TX Rout trimming: enable\n");
	} else {
		rkx12x_print("    HS-TX Rout trimming: disable\n");

		bits_val = (val & GENMASK(12, 9)) >> 9;
		rkx12x_print("    rcal_trim = %d\n", bits_val);
	}

	/* LVDSMIPITX_TRIM_PARA */
	offset = 0x0068;
	reg = mipi_phy_base + offset;
	rkx12x->i2c_reg_read(client, reg, &val);
	rkx12x_print("LVDSMIPITX_TRIM_PARA(0x%08x) = 0x%08x\n", offset, val);
}

static void rkx12x_csi_tx_reg_dump(struct rkx12x *rkx12x, u8 csi_tx_id)
{
	struct i2c_client *client = rkx12x->client;
	u32 csi_tx_base = 0;
	u32 reg = 0;
	u32 offset = 0;
	u32 val = 0, bits_val = 0;

	csi_tx_base = csi_tx_id ? RKX12X_CSI_TX1_BASE : RKX12X_CSI_TX0_BASE;

	/* CSITX_CONFIG_DONE */
	offset = 0x0000;
	reg = csi_tx_base + offset;
	rkx12x->i2c_reg_read(client, reg, &val);
	rkx12x_print("CSITX_CONFIG_DONE(0x%08x) = 0x%08x\n", offset, val);
	if (val & BIT(0))
		rkx12x_print("    config done\n");

	if (val & BIT(8))
		rkx12x_print("    config done when frm_end_tx\n");
	else
		rkx12x_print("    config done when frm_end_rx\n");

	/* CSITX_CSITX_EN */
	offset = 0x0004;
	reg = csi_tx_base + offset;
	rkx12x->i2c_reg_read(client, reg, &val);
	rkx12x_print("CSITX_CSITX_EN(0x%08x) = 0x%08x\n", offset, val);

	/* CSITX_CSITX_VERSION */
	offset = 0x0008;
	reg = csi_tx_base + offset;
	rkx12x->i2c_reg_read(client, reg, &val);
	rkx12x_print("CSITX_CSITX_VERSION(0x%08x) = 0x%08x\n", offset, val);

	/* CSITX_SYS_CTRL0_IMD */
	offset = 0x0010;
	reg = csi_tx_base + offset;
	rkx12x->i2c_reg_read(client, reg, &val);
	rkx12x_print("CSITX_SYS_CTRL0_IMD(0x%08x) = 0x%08x\n", offset, val);
	if (val & BIT(0))
		rkx12x_print("    soft reset enable\n");
	else
		rkx12x_print("    soft reset disable\n");

	/* CSITX_SYS_CTRL1 */
	offset = 0x0014;
	reg = csi_tx_base + offset;
	rkx12x->i2c_reg_read(client, reg, &val);
	rkx12x_print("CSITX_SYS_CTRL1(0x%08x) = 0x%08x\n", offset, val);
	if (val & BIT(0))
		rkx12x_print("    select idi\n");
	else
		rkx12x_print("    select vop\n");

	/* CSITX_SYS_CTRL2 */
	offset = 0x0018;
	reg = csi_tx_base + offset;
	rkx12x->i2c_reg_read(client, reg, &val);
	rkx12x_print("CSITX_SYS_CTRL2(0x%08x) = 0x%08x\n", offset, val);
	if (val & BIT(0))
		rkx12x_print("    vsync enable: send vsync timing short packet\n");
	else
		rkx12x_print("    vsync disable\n");

	if (val & BIT(1))
		rkx12x_print("    hsync enable: send hsync timing short packet\n");
	else
		rkx12x_print("    hsync disable\n");

	if (val & BIT(4))
		rkx12x_print("    idi_whole_frm enable\n");
	else
		rkx12x_print("    idi_whole_frm disable\n");

	/* CSITX_SYS_CTRL3_IMD */
	offset = 0x001c;
	reg = csi_tx_base + offset;
	rkx12x->i2c_reg_read(client, reg, &val);
	rkx12x_print("CSITX_SYS_CTRL3_IMD(0x%08x) = 0x%08x\n", offset, val);
	if (val & BIT(0)) {
		rkx12x_print("    non_continous_mode\n");
	} else {
		rkx12x_print("    continous_mode\n");

		if (val & BIT(4))
			rkx12x_print("    cont_mode_clk_set\n");
		if (val & BIT(8))
			rkx12x_print("    cont_mode_clk_clr\n");
	}

	/* CSITX_BYPASS_CTRL */
	offset = 0x0060;
	reg = csi_tx_base + offset;
	rkx12x->i2c_reg_read(client, reg, &val);
	rkx12x_print("CSITX_BYPASS_CTRL(0x%08x) = 0x%08x\n", offset, val);
	if (val & BIT(0)) {
		rkx12x_print("    bypass path enable\n");

		bits_val = (val & GENMASK(7, 4)) >> 4;
		rkx12x_print("    bypass camera format = %d\n", bits_val);
	} else {
		rkx12x_print("    bypass path disable\n");
	}

	if (val & BIT(1)) {
		rkx12x_print("    bypass datatype userdefine enable\n");

		bits_val = (val & GENMASK(13, 8)) >> 8;
		rkx12x_print("    bypass datatype userdefine = %d\n", bits_val);
	} else {
		rkx12x_print("    bypass datatype userdefine disable\n");
	}

	if (val & BIT(2)) {
		rkx12x_print("    bypass virtual num userdefine enable\n");

		bits_val = (val & GENMASK(15, 14)) >> 14;
		rkx12x_print("    bypass virtual num userdefine = %d\n", bits_val);
	} else {
		rkx12x_print("    bypass virtual num userdefine disable\n");
	}

	if (val & BIT(3)) {
		rkx12x_print("    bypass word count userdefine enable\n");

		bits_val = (val & GENMASK(31, 16)) >> 16;
		rkx12x_print("    bypass word count userdefine = %d\n", bits_val);
	} else {
		rkx12x_print("    bypass word count userdefine disable\n");
	}

	/* CSITX_BYPASS_PKT_CTRL */
	offset = 0x0064;
	reg = csi_tx_base + offset;
	rkx12x->i2c_reg_read(client, reg, &val);
	rkx12x_print("CSITX_BYPASS_PKT_CTRL(0x%08x) = 0x%08x\n", offset, val);
	if (val & BIT(4)) {
		rkx12x_print("    bypass line padding enable\n");

		bits_val = (val & GENMASK(7, 5)) >> 5;
		rkx12x_print("    line padding number = %d\n", bits_val);
	}
	if (val & BIT(8)) {
		rkx12x_print("    bypass packet padding enable\n");

		bits_val = (val & GENMASK(31, 16)) >> 16;
		rkx12x_print("    byte number of one packet = %d\n", bits_val);
	}

	/* CSITX_CSITX_STATUS0 */
	offset = 0x0070;
	reg = csi_tx_base + offset;
	rkx12x->i2c_reg_read(client, reg, &val);
	rkx12x_print("CSITX_CSITX_STATUS0(0x%08x) = 0x%08x\n", offset, val);
	if (val & BIT(0))
		rkx12x_print("    csitx idle\n");
	if (val & BIT(1))
		rkx12x_print("    header fifo empty\n");
	if (val & BIT(2))
		rkx12x_print("    payload lb empty\n");
	if (val & BIT(3))
		rkx12x_print("    payload lb valid\n");
	if (val & BIT(4))
		rkx12x_print("    tx fifo empty\n");
	if (val & BIT(5))
		rkx12x_print("    packetiser idle\n");
	if (val & BIT(6))
		rkx12x_print("    lane splitter idle\n");
	if (val & BIT(4))
		rkx12x_print("    tx fifo empty\n");

	/* CSITX_CSITX_STATUS1 */
	offset = 0x0074;
	reg = csi_tx_base + offset;
	rkx12x->i2c_reg_read(client, reg, &val);
	rkx12x_print("CSITX_CSITX_STATUS1(0x%08x) = 0x%08x\n", offset, val);

	/* CSITX_CSITX_STATUS2 */
	offset = 0x0078;
	reg = csi_tx_base + offset;
	rkx12x->i2c_reg_read(client, reg, &val);
	rkx12x_print("CSITX_CSITX_STATUS2(0x%08x) = 0x%08x\n", offset, val);
	bits_val = (val & GENMASK(15, 0)) >> 0;
	rkx12x_print("    dphy scanline = %d\n", bits_val);
	bits_val = (val & GENMASK(31, 16)) >> 16;
	rkx12x_print("    cphy scanline = %d\n", bits_val);

	/* CSITX_LINE_FLAG_NUM */
	offset = 0x007c;
	reg = csi_tx_base + offset;
	rkx12x->i2c_reg_read(client, reg, &val);
	rkx12x_print("CSITX_LINE_FLAG_NUM(0x%08x) = 0x%08x\n", offset, val);
	bits_val = (val & GENMASK(15, 0)) >> 0;
	rkx12x_print("    line num flag0 = %d\n", bits_val);
	bits_val = (val & GENMASK(31, 16)) >> 16;
	rkx12x_print("    line num flag1 = %d\n", bits_val);

	/* CSITX_INTR_EN_IMD */
	offset = 0x0080;
	reg = csi_tx_base + offset;
	rkx12x->i2c_reg_read(client, reg, &val);
	rkx12x_print("CSITX_INTR_EN_IMD(0x%08x) = 0x%08x\n", offset, val);

	/* CSITX_INTR_STATUS */
	offset = 0x0088;
	reg = csi_tx_base + offset;
	rkx12x->i2c_reg_read(client, reg, &val);
	rkx12x_print("CSITX_INTR_STATUS(0x%08x) = 0x%08x\n", offset, val);
	if (val & BIT(0))
		rkx12x_print("    intr_sts_frm_st_rx\n");
	if (val & BIT(1))
		rkx12x_print("    intr_sts_line_end_rx\n");
	if (val & BIT(2))
		rkx12x_print("    intr_sts_frm_end_rx\n");
	if (val & BIT(3))
		rkx12x_print("    intr_sts_frm_st_tx\n");
	if (val & BIT(4))
		rkx12x_print("    intr_sts_frm_end_tx\n");
	if (val & BIT(5))
		rkx12x_print("    intr_sts_line_end_tx\n");
	if (val & BIT(6))
		rkx12x_print("    intr_sts_line_flag0\n");
	if (val & BIT(7))
		rkx12x_print("    intr_sts_line_flag1\n");
	if (val & BIT(8))
		rkx12x_print("    intr_sts_stopstate\n");
	if (val & BIT(9))
		rkx12x_print("    intr_sts_pll_lock\n");
	if (val & BIT(10))
		rkx12x_print("    intr_sts_csitx_idle\n");

	/* CSITX_INTR_RAW_STATUS */
	offset = 0x008c;
	reg = csi_tx_base + offset;
	rkx12x->i2c_reg_read(client, reg, &val);
	rkx12x_print("CSITX_INTR_RAW_STATUS(0x%08x) = 0x%08x\n", offset, val);
	if (val & BIT(0))
		rkx12x_print("    intr_frm_st_rx\n");
	if (val & BIT(1))
		rkx12x_print("    intr_line_end_rx\n");
	if (val & BIT(2))
		rkx12x_print("    intr_frm_end_rx\n");
	if (val & BIT(3))
		rkx12x_print("    intr_frm_st_tx\n");
	if (val & BIT(4))
		rkx12x_print("    intr_frm_end_tx\n");
	if (val & BIT(5))
		rkx12x_print("    intr_line_end_tx\n");
	if (val & BIT(6))
		rkx12x_print("    intr_line_flag0\n");
	if (val & BIT(7))
		rkx12x_print("    intr_line_flag1\n");
	if (val & BIT(8))
		rkx12x_print("    intr_stopstate\n");
	if (val & BIT(9))
		rkx12x_print("    intr_pll_lock\n");
	if (val & BIT(10))
		rkx12x_print("    intr_csitx_idle\n");

	/* CSITX_ERR_INTR_EN_IMD */
	offset = 0x0090;
	reg = csi_tx_base + offset;
	rkx12x->i2c_reg_read(client, reg, &val);
	rkx12x_print("CSITX_ERR_INTR_EN_IMD(0x%08x) = 0x%08x\n", offset, val);

	/* CSITX_ERR_INTR_CLR_IMD */
	offset = 0x0094;
	reg = csi_tx_base + offset;
	rkx12x->i2c_reg_read(client, reg, &val);
	rkx12x_print("CSITX_ERR_INTR_CLR_IMD(0x%08x) = 0x%08x\n", offset, val);

	/* CSITX_ERR_INTR_STATUS_IMD */
	offset = 0x0098;
	reg = csi_tx_base + offset;
	rkx12x->i2c_reg_read(client, reg, &val);
	rkx12x_print("CSITX_ERR_INTR_STATUS_IMD(0x%08x) = 0x%08x\n", offset, val);

	/* CSITX_ERR_INTR_RAW_STATUS_IMD */
	offset = 0x009c;
	reg = csi_tx_base + offset;
	rkx12x->i2c_reg_read(client, reg, &val);
	rkx12x_print("CSITX_ERR_INTR_RAW_STATUS_IMD(0x%08x) = 0x%08x\n", offset, val);
}

static void rkx12x_dvp_tx_reg_dump(struct rkx12x *rkx12x)
{
	struct i2c_client *client = rkx12x->client;
	u32 dvp_tx_base = RKX12X_DVP_TX_BASE;
	u32 reg = 0;
	u32 offset = 0;
	u32 val = 0, bits_val = 0;

	/* DVP_TX_CTRL0 */
	offset = 0x0100;
	reg = dvp_tx_base + offset;
	rkx12x->i2c_reg_read(client, reg, &val);
	rkx12x_print("DVP_TX_CTRL0(0x%08x) = 0x%08x\n", offset, val);
	if (val & BIT(0))
		rkx12x_print("    dvp tx enable\n");
	else
		rkx12x_print("    dvp tx disable\n");
	bits_val = (val & GENMASK(31, 8)) >> 8;
	rkx12x_print("    Vsync signal width(unit: pclk) = %d\n", bits_val);

	/* DVP_TX_CTRL1 */
	offset = 0x0104;
	reg = dvp_tx_base + offset;
	rkx12x->i2c_reg_read(client, reg, &val);
	rkx12x_print("DVP_TX_CTRL1(0x%08x) = 0x%08x\n", offset, val);
	bits_val = (val & GENMASK(13, 0)) >> 0;
	rkx12x_print("    Width = %d\n", bits_val);
	bits_val = (val & GENMASK(29, 16)) >> 16;
	rkx12x_print("    Height = %d\n", bits_val);

	/* DVP_TX_INTEN */
	offset = 0x0174;
	reg = dvp_tx_base + offset;
	rkx12x->i2c_reg_read(client, reg, &val);
	rkx12x_print("DVP_TX_INTEN(0x%08x) = 0x%08x\n", offset, val);

	/* DVP_TX_INTSTAT */
	offset = 0x0178;
	reg = dvp_tx_base + offset;
	rkx12x->i2c_reg_read(client, reg, &val);
	rkx12x_print("DVP_TX_INTSTAT(0x%08x) = 0x%08x\n", offset, val);
	if (val & BIT(0))
		rkx12x_print("    frame_start_intst\n");
	if (val & BIT(8))
		rkx12x_print("    frame_end_intst\n");

	if (val & BIT(19))
		rkx12x_print("    fifo_overflow_intst\n");
	if (val & BIT(20))
		rkx12x_print("    fifo_empty_intst\n");

	if (val & BIT(24))
		rkx12x_print("    size_error_intst\n");

	/* DVP_TX_SIZE_NUM */
	offset = 0x01c0;
	reg = dvp_tx_base + offset;
	rkx12x->i2c_reg_read(client, reg, &val);
	rkx12x_print("DVP_TX_SIZE_NUM(0x%08x) = 0x%08x\n", offset, val);
	bits_val = (val & GENMASK(13, 0)) >> 0;
	rkx12x_print("    pixel number in one line = %d\n", bits_val);
	bits_val = (val & GENMASK(29, 16)) >> 16;
	rkx12x_print("    line number in one frame = %d\n", bits_val);
}

static void rkx12x_linkrx_dump_engine_cfg(u32 val)
{
	u32 bits_val = 0;

	if (val & BIT(0))
		rkx12x_print("    linkrx enable\n");
	else
		rkx12x_print("    linkrx disable\n");

	if (val & BIT(4))
		rkx12x_print("    lane0 enable\n");
	else
		rkx12x_print("    lane0 disable\n");

	if (val & BIT(5))
		rkx12x_print("    lane1 enable\n");
	else
		rkx12x_print("    lane1 disable\n");

	bits_val = (val & GENMASK(13, 12)) >> 12;
	rkx12x_print("    lane0 data width: %d\n", 8 * (bits_val + 1));

	bits_val = (val & GENMASK(15, 14)) >> 14;
	rkx12x_print("    lane1 data width: %d\n", 8 * (bits_val + 1));

	if (val & BIT(20)) {
		rkx12x_print("    video timing recover engine0 enable\n");

		if (val & BIT(21))
			rkx12x_print("    engine0: use two lane send data\n");
		else
			rkx12x_print("    engine0: use one lane send data\n");
	}

	if (val & BIT(22)) {
		rkx12x_print("    video timing recover engine1 enable\n");

		if (val & BIT(23))
			rkx12x_print("    engine1: use two lane send data\n");
		else
			rkx12x_print("    engine1: use one lane send data\n");
	}

	if (val & BIT(28))
		rkx12x_print("    video frequency auto modify enable\n");
	else
		rkx12x_print("    video frequency auto modify disable\n");

	if (val & BIT(29))
		rkx12x_print("    dual lvds: exchange lvds0 and lvds1 data\n");

	bits_val = (val & GENMASK(31, 30)) >> 30;
	if (bits_val == 0)
		rkx12x_print("    train clock select engine0 clock\n");
	else if (bits_val == 1)
		rkx12x_print("    train clock select engine1 clock\n");
	else
		rkx12x_print("    train clock select i2s sclk\n");
}

static void rkx12x_linkrx_dump_fifo_status(u32 val)
{
	/* cmd fifo occur overflow */
	if (val & BIT(0))
		rkx12x_print("    cmd fifo0 overflow\n");
	if (val & BIT(1))
		rkx12x_print("    cmd fifo1 overflow\n");
	if (val & BIT(2))
		rkx12x_print("    cmd fifo2 overflow\n");
	if (val & BIT(3))
		rkx12x_print("    cmd fifo3 overflow\n");

	/* video order fifo occur overflow */
	if (val & BIT(4))
		rkx12x_print("    video order fifo0 overflow\n");
	if (val & BIT(5))
		rkx12x_print("    video order fifo1 overflow\n");
	if (val & BIT(6))
		rkx12x_print("    video order fifo2 overflow\n");
	if (val & BIT(7))
		rkx12x_print("    video order fifo3 overflow\n");

	/* video data fifo0 occur overflow */
	if (val & BIT(8))
		rkx12x_print("    video data fifo0 overflow\n");
	if (val & BIT(9))
		rkx12x_print("    video data fifo1 overflow\n");
	if (val & BIT(10))
		rkx12x_print("    video data fifo2 overflow\n");
	if (val & BIT(11))
		rkx12x_print("    video data fifo3 overflow\n");

	/* audio order fifo overflow */
	if (val & BIT(12))
		rkx12x_print("    audio order fifo overflow\n");
	/* audio data  fifo overflow */
	if (val & BIT(13))
		rkx12x_print("    audio data fifo overflow\n");

	/* engine0 data order miss match */
	if (val & BIT(14))
		rkx12x_print("    engine0 data order miss match\n");
	/* engine1 data order miss match */
	if (val & BIT(15))
		rkx12x_print("    engine1 data order miss match\n");

	/* cmd fifo occur underrun */
	if (val & BIT(16))
		rkx12x_print("    cmd fifo0 underrun\n");
	if (val & BIT(17))
		rkx12x_print("    cmd fifo1 underrun\n");
	if (val & BIT(18))
		rkx12x_print("    cmd fifo2 underrun\n");
	if (val & BIT(19))
		rkx12x_print("    cmd fifo3 underrun\n");

	/* video order fifo occur underrun */
	if (val & BIT(20))
		rkx12x_print("    video order fifo0 underrun\n");
	if (val & BIT(21))
		rkx12x_print("    video order fifo1 underrun\n");
	if (val & BIT(22))
		rkx12x_print("    video order fifo2 underrun\n");
	if (val & BIT(23))
		rkx12x_print("    video order fifo3 underrun\n");

	/* video data fifo0 occur underrun */
	if (val & BIT(24))
		rkx12x_print("    video data fifo0 underrun\n");
	if (val & BIT(25))
		rkx12x_print("    video data fifo1 underrun\n");
	if (val & BIT(26))
		rkx12x_print("    video data fifo2 underrun\n");
	if (val & BIT(27))
		rkx12x_print("    video data fifo3 underrun\n");

	/* audio order fifo underrun */
	if (val & BIT(28))
		rkx12x_print("    audio order fifo underrun\n");
	/* audio data fifo underrun */
	if (val & BIT(29))
		rkx12x_print("    audio data fifo underrun\n");
}

static void rkx12x_linkrx_dump_source_id_cfg(u32 val)
{
	u32 bits_val = 0;

	if (val & BIT(0)) {
		rkx12x_print("    lane0 device id use the pkt hdr info\n");
	} else {
		rkx12x_print("    lane0 device id use the lane0_dsource_id_reg\n");

		bits_val = (val & GENMASK(3, 2)) >> 2;
		rkx12x_print("    lane0 device source id reg: device_id = %d\n", bits_val);

		bits_val = (val & BIT(1)) >> 1;
		rkx12x_print("    lane0 device source id reg: lane_id = %d\n", bits_val);
	}

	if (val & BIT(4)) {
		rkx12x_print("    lane1 device id use the pkt hdr info\n");
	} else {
		rkx12x_print("    lane1 device id use the lane1_dsource_id_reg\n");

		bits_val = (val & GENMASK(7, 6)) >> 6;
		rkx12x_print("    lane1 device source id reg: device_id = %d\n", bits_val);

		bits_val = (val & BIT(5)) >> 5;
		rkx12x_print("    lane1 device source id reg: lane_id = %d\n", bits_val);
	}

	bits_val = (val & GENMASK(19, 17)) >> 17;
	if (val & BIT(16)) {
		rkx12x_print("    engine0 interface: display type\n");
		if (bits_val == 0)
			rkx12x_print("    engine0 output: dsi\n");
		else if (bits_val == 1)
			rkx12x_print("    engine0 output: dual_lvds\n");
		else if (bits_val == 2)
			rkx12x_print("    engine0 output: lvds0\n");
		else if (bits_val == 3)
			rkx12x_print("    engine0 output: lvds1\n");
		else if (bits_val == 5)
			rkx12x_print("    engine0 output: rgb\n");
	} else {
		rkx12x_print("    engine0 output interface: camera type\n");
		if (bits_val == 0)
			rkx12x_print("    engine0 output: csi\n");
		else if (bits_val == 1)
			rkx12x_print("    engine0 output: lvds\n");
		else if (bits_val == 2)
			rkx12x_print("    engine0 output: dvp\n");
	}

	bits_val = (val & GENMASK(23, 21)) >> 21;
	if (val & BIT(20)) {
		rkx12x_print("    engine1 output interface: display type\n");
		if (bits_val == 0)
			rkx12x_print("    engine1 output: dsi\n");
		else if (bits_val == 1)
			rkx12x_print("    engine1 output: dual_lvds\n");
		else if (bits_val == 2)
			rkx12x_print("    engine1 output: lvds0\n");
		else if (bits_val == 3)
			rkx12x_print("    engine1 output: lvds1\n");
		else if (bits_val == 5)
			rkx12x_print("    engine1 output: rgb\n");
	} else {
		rkx12x_print("    engine1 output interface: camera type\n");
		if (bits_val == 0)
			rkx12x_print("    engine1 output: csi\n");
		else if (bits_val == 1)
			rkx12x_print("    engine1 output: lvds\n");
		else if (bits_val == 2)
			rkx12x_print("    engine1 output: dvp\n");
	}
}

static void rkx12x_linkrx_reg_dump(struct rkx12x *rkx12x)
{
	struct i2c_client *client = rkx12x->client;
	u32 reg = 0;
	u32 offset = 0;
	u32 val = 0;

	/* link ready status */
	reg = DES_GRF_SOC_STATUS0;
	rkx12x->i2c_reg_read(client, reg, &val);
	rkx12x_print("DES_GRF_SOC_STATUS0(0x%08x) = 0x%08x\n", reg, val);
	if (val & DES_PCS0_READY)
		rkx12x_print("    des pcs0 ready\n");
	if (val & DES_PCS1_READY)
		rkx12x_print("    des pcs1 ready\n");

	/* PMA enable */
	reg = DES_GRF_SOC_CON4;
	rkx12x->i2c_reg_read(client, reg, &val);
	rkx12x_print("DES_GRF_SOC_CON4(0x%08x) = 0x%08x\n", reg, val);
	if (val & BIT(10))
		rkx12x_print("    PMA0 enable\n");
	if (val & BIT(11))
		rkx12x_print("    PMA1 enable\n");

	/* DES_RKLINK_LANE_ENGINE_CFG */
	offset = 0x0000;
	reg = RKX12X_DES_RKLINK_BASE + offset;
	rkx12x->i2c_reg_read(client, reg, &val);
	rkx12x_print("DES_RKLINK_LANE_ENGINE_CFG(0x%08x) = 0x%08x\n", offset, val);
	rkx12x_linkrx_dump_engine_cfg(val);

	/* RKLINK_DES_LANE_ENGINE_DST */
	offset = 0x0004;
	reg = RKX12X_DES_RKLINK_BASE + offset;
	rkx12x->i2c_reg_read(client, reg, &val);
	rkx12x_print("RKLINK_DES_LANE_ENGINE_DST(0x%08x) = 0x%08x\n", offset, val);

	/* DES_RKLINK_DATA_ID_CFG */
	offset = 0x0008;
	reg = RKX12X_DES_RKLINK_BASE + offset;
	rkx12x->i2c_reg_read(client, reg, &val);
	rkx12x_print("DES_RKLINK_DATA_ID_CFG(0x%08x) = 0x%08x\n", offset, val);
	rkx12x_linkrx_dump_source_id_cfg(val);

	/* DES_RKLINK_ORDER_ID_CFG */
	offset = 0x000C;
	reg = RKX12X_DES_RKLINK_BASE + offset;
	rkx12x->i2c_reg_read(client, reg, &val);
	rkx12x_print("DES_RKLINK_ORDER_ID_CFG(0x%08x) = 0x%08x\n", offset, val);

	/* DES_RKLINK_PKT_LOSE_NUM_L0 */
	offset = 0x0010;
	reg = RKX12X_DES_RKLINK_BASE + offset;
	rkx12x->i2c_reg_read(client, reg, &val);
	rkx12x_print("DES_RKLINK_PKT_LOSE_NUM_L0(0x%08x) = 0x%08x\n", offset, val);

	/* DES_RKLINK_PKT_LOSE_NUM_L1 */
	offset = 0x0014;
	reg = RKX12X_DES_RKLINK_BASE + offset;
	rkx12x->i2c_reg_read(client, reg, &val);
	rkx12x_print("DES_RKLINK_PKT_LOSE_NUM_L1(0x%08x) = 0x%08x\n", offset, val);

	/* DES_RKLINK_ECC_CRC_RESULT */
	offset = 0x0020;
	reg = RKX12X_DES_RKLINK_BASE + offset;
	rkx12x->i2c_reg_read(client, reg, &val);
	rkx12x_print("DES_RKLINK_ECC_CRC_RESULT(0x%08x) = 0x%08x\n", offset, val);

	/* DES_RKLINK_SOURCE_ID_CFG */
	offset = 0x0024;
	reg = RKX12X_DES_RKLINK_BASE + offset;
	rkx12x->i2c_reg_read(client, reg, &val);
	rkx12x_print("DES_RKLINK_SOURCE_ID_CFG(0x%08x) = 0x%08x\n", offset, val);

	/* DES_RKLINK_REC01_PKT_LENGTH */
	offset = 0x0028;
	reg = RKX12X_DES_RKLINK_BASE + offset;
	rkx12x->i2c_reg_read(client, reg, &val);
	rkx12x_print("DES_RKLINK_REC01_PKT_LENGTH(0x%08x) = 0x%08x\n", offset, val);

	/* DES_RKLINK_REC01_ENGINE_DELAY */
	offset = 0x0030;
	reg = RKX12X_DES_RKLINK_BASE + offset;
	rkx12x->i2c_reg_read(client, reg, &val);
	rkx12x_print("DES_RKLINK_REC01_ENGINE_DELAY(0x%08x) = 0x%08x\n", offset, val);

	/* DES_RKLINK_REC_PATCH_CFG */
	offset = 0x0050;
	reg = RKX12X_DES_RKLINK_BASE + offset;
	rkx12x->i2c_reg_read(client, reg, &val);
	rkx12x_print("DES_RKLINK_REC_PATCH_CFG(0x%08x) = 0x%08x\n", offset, val);

	/* DES_RKLINK_FIFO_STATUS */
	offset = 0x0084;
	reg = RKX12X_DES_RKLINK_BASE + offset;
	rkx12x->i2c_reg_read(client, reg, &val);
	rkx12x_print("DES_RKLINK_FIFO_STATUS(0x%08x) = 0x%08x\n", offset, val);
	rkx12x_linkrx_dump_fifo_status(val);

	/* DES_RKLINK_SINK_IRQ_EN */
	offset = 0x0088;
	reg = RKX12X_DES_RKLINK_BASE + offset;
	rkx12x->i2c_reg_read(client, reg, &val);
	rkx12x_print("DES_RKLINK_SINK_IRQ_EN(0x%08x) = 0x%08x\n", offset, val);

	/* DES_RKLINK_TRAIN_CNT_ADDR */
	offset = 0x008c;
	reg = RKX12X_DES_RKLINK_BASE + offset;
	rkx12x->i2c_reg_read(client, reg, &val);
	rkx12x_print("DES_RKLINK_TRAIN_CNT_ADDR(0x%08x) = 0x%08x\n", offset, val);

	/* DES_RKLINK_STOP_CFG */
	offset = 0x008c;
	reg = RKX12X_DES_RKLINK_BASE + offset;
	rkx12x->i2c_reg_read(client, reg, &val);
	rkx12x_print("DES_RKLINK_STOP_CFG(0x%08x) = 0x%08x\n", offset, val);
	if (val & BIT(4))
		rkx12x_print("    audio status: stop\n");
	else
		rkx12x_print("    audio status: normal\n");
	if (val & BIT(1))
		rkx12x_print("    video engine 1 status: stop\n");
	else
		rkx12x_print("    video engine 1 status: normal\n");
	if (val & BIT(0))
		rkx12x_print("    video engine 0 status: stop\n");
	else
		rkx12x_print("    video engine 0 status: normal\n");
}

int rkx12x_module_dump(struct rkx12x *rkx12x, u32 module_id)
{
	rkx12x_print("module dump: module_id = %d\n", module_id);

	switch (module_id) {
	case RKX12X_DUMP_GRF:
		rkx12x_grf_reg_dump(rkx12x);
		break;
	case RKX12X_DUMP_LINKRX:
		rkx12x_linkrx_reg_dump(rkx12x);
		break;
	case RKX12X_DUMP_GRF_MIPI0:
		rkx12x_mipi_grf_reg_dump(rkx12x, 0);
		break;
	case RKX12X_DUMP_GRF_MIPI1:
		rkx12x_mipi_grf_reg_dump(rkx12x, 1);
		break;
	case RKX12X_DUMP_TXPHY0:
		rkx12x_mipitxphy_reg_dump(rkx12x, 0);
		break;
	case RKX12X_DUMP_TXPHY1:
		rkx12x_mipitxphy_reg_dump(rkx12x, 1);
		break;
	case RKX12X_DUMP_CSITX0:
		rkx12x_csi_tx_reg_dump(rkx12x, 0);
		break;
	case RKX12X_DUMP_CSITX1:
		rkx12x_csi_tx_reg_dump(rkx12x, 1);
		break;
	case RKX12X_DUMP_DVPTX:
		rkx12x_dvp_tx_reg_dump(rkx12x);
		break;
	default:
		rkx12x_print("module id = %d is not support\n", module_id);
		return -1;
	}

	return 0;
}
EXPORT_SYMBOL(rkx12x_module_dump);
