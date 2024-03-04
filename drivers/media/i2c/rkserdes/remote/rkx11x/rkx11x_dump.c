// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2024 Rockchip Electronics Co. Ltd.
 *
 * Author: Cai Wenzhong <cwz@rock-chips.com>
 */

#include "rkx11x_drv.h"
#include "rkx11x_dump.h"
#include "rkx11x_reg.h"

#define rkx11x_print(fmt, ...)				\
({							\
	pr_info("[RKX11X] " fmt, ##__VA_ARGS__);	\
})

static void rkx11x_grf_reg_dump(struct rkx11x *rkx11x)
{
	struct i2c_client *client = rkx11x->client;
	u32 reg = 0;
	u32 offset = 0;
	u32 val = 0, bits_val = 0;

	/* SER_GRF_SOC_CON3 */
	reg = SER_GRF_SOC_CON3;
	offset = reg - RKX11X_SER_GRF_BASE;
	rkx11x->i2c_reg_read(client, reg, &val);
	rkx11x_print("SER_GRF_SOC_CON3(0x%08x) = 0x%08x\n", offset, val);

	if (val & BIT(0)) {
		rkx11x_print("    vicap clock delayline enable\n");
		bits_val = (val & GENMASK(7, 1)) >> 1;
		rkx11x_print("    vicap clock delayline number = %d\n", bits_val);
	} else {
		rkx11x_print("    vicap clock delayline disable\n");
	}

	if (val & BIT(8))
		rkx11x_print("    vicap clock invert\n");
	else
		rkx11x_print("    vicap clock bypass\n");

	if (val & BIT(9))
		rkx11x_print("    Dual-edge sampling for DVP signals\n");
	else
		rkx11x_print("    Single-edge sampling for DVP signals\n");

	if (val & BIT(14))
		rkx11x_print("    link command source: pcs1\n");
	else
		rkx11x_print("    link command source: pcs0\n");

	if (val & BIT(15))
		rkx11x_print("    audio source: pcs1\n");
	else
		rkx11x_print("    audio source: pcs0\n");

	/* SER_GRF_SOC_CON7 */
	reg = SER_GRF_SOC_CON7;
	offset = reg - RKX11X_SER_GRF_BASE;
	rkx11x->i2c_reg_read(client, reg, &val);
	rkx11x_print("SER_GRF_SOC_CON7(0x%08x) = 0x%08x\n", offset, val);
	if (val & BIT(8))
		rkx11x_print("    PMA0 enable\n");
	else
		rkx11x_print("    PMA0 disable\n");
	if (val & BIT(9))
		rkx11x_print("    PMA1 enable\n");
	else
		rkx11x_print("    PMA1 disable\n");

	if (val & BIT(15))
		rkx11x_print("    PMA1 control PMA_PLL\n");
	else
		rkx11x_print("    PMA0 control PMA_PLL\n");

	/* SER_GRF_IRQ_EN */
	reg = RKX11X_SER_GRF_BASE + 0x0140;
	offset = reg - RKX11X_SER_GRF_BASE;
	rkx11x->i2c_reg_read(client, reg, &val);
	rkx11x_print("SER_GRF_IRQ_EN(0x%08x) = 0x%08x\n", offset, val);

	/* SER_GRF_SOC_STATUS0 */
	reg = SER_GRF_SOC_STATUS0;
	offset = reg - RKX11X_SER_GRF_BASE;
	rkx11x->i2c_reg_read(client, reg, &val);
	rkx11x_print("SER_GRF_SOC_STATUS0(0x%08x) = 0x%08x\n", offset, val);
	if (val & SER_PCS1_READY)
		rkx11x_print("    ser pcs1 ready\n");
	if (val & SER_PCS0_READY)
		rkx11x_print("    ser pcs0 ready\n");
}

static void rkx11x_mipi_grf_reg_dump(struct rkx11x *rkx11x, u8 mipi_phy_id)
{
	struct i2c_client *client = rkx11x->client;
	u32 mipi_grf_base = 0;
	u32 reg = 0;
	u32 offset = 0;
	u32 val = 0, bits_val = 0;

	mipi_grf_base = mipi_phy_id ? RKX11X_GRF_MIPI1_BASE : RKX11X_GRF_MIPI0_BASE;

	/* GRF_MIPI_RX_CON0 */
	offset = 0x0000;
	reg = mipi_grf_base + offset;
	rkx11x->i2c_reg_read(client, reg, &val);
	rkx11x_print("GRF_MIPI_RX_CON0(0x%08x) = 0x%08x\n", offset, val);
	bits_val = (val & GENMASK(1, 0)) >> 0;
	if (bits_val == 0)
		rkx11x_print("    phy mode: gpio\n");
	else if (bits_val == 1)
		rkx11x_print("    phy mode: lvds display\n");
	else if (bits_val == 2)
		rkx11x_print("    phy mode: mipi\n");
	else
		rkx11x_print("    phy mode: lvds camera\n");

	if (val & BIT(2))
		rkx11x_print("    mipi data lane0 enable\n");
	if (val & BIT(3))
		rkx11x_print("    mipi data lane1 enable\n");
	if (val & BIT(4))
		rkx11x_print("    mipi data lane2 enable\n");
	if (val & BIT(5))
		rkx11x_print("    mipi data lane3 enable\n");

	bits_val = (val & GENMASK(15, 12)) >> 12;
	rkx11x_print("    Terminal impedance regulation control signal = %d\n", bits_val);

	/* GRF_MIPI_SOC_STATUS */
	offset = 0x0580;
	reg = mipi_grf_base + offset;
	rkx11x->i2c_reg_read(client, reg, &val);
	rkx11x_print("GRF_MIPI_SOC_STATUS(0x%08x) = 0x%08x\n", offset, val);
	if (val & BIT(24))
		rkx11x_print("    mipi clock lane locked indication signal\n");
	else
		rkx11x_print("    mipi clock lane unlocked indication signal\n");
	bits_val = (val & GENMASK(3, 0)) >> 0;
	rkx11x_print("    mipi data lane0 ctlo status = %d\n", bits_val);
	bits_val = (val & GENMASK(7, 4)) >> 4;
	rkx11x_print("    mipi data lane1 ctlo status = %d\n", bits_val);
	bits_val = (val & GENMASK(11, 8)) >> 8;
	rkx11x_print("    mipi data lane2 ctlo status = %d\n", bits_val);
	bits_val = (val & GENMASK(15, 12)) >> 12;
	rkx11x_print("    mipi data lane3 ctlo status = %d\n", bits_val);
	bits_val = (val & GENMASK(23, 16)) >> 16;
	rkx11x_print("    mipi clock lane ctlo status = %d\n", bits_val);
}

static void rkx11x_mipirxphy_reg_dump(struct rkx11x *rkx11x, u8 mipi_phy_id)
{
	struct i2c_client *client = rkx11x->client;
	u32 mipi_phy_base = 0;
	u32 reg = 0;
	u32 offset = 0;
	u32 val = 0, bits_val = 0;
	u32 i = 0;

	mipi_phy_base = mipi_phy_id ? RKX11X_MIPI_LVDS_RX_PHY1_BASE : RKX11X_MIPI_LVDS_RX_PHY0_BASE;

	/* LVDSMIPIRX_SOFT_RST */
	offset = 0x0000;
	reg = mipi_phy_base + offset;
	rkx11x->i2c_reg_read(client, reg, &val);
	rkx11x_print("LVDSMIPIRX_SOFT_RST(0x%08x) = 0x%08x\n", offset, val);
	if (val & BIT(1))
		rkx11x_print("    soft reset: clk_hs domain\n");
	if (val & BIT(0))
		rkx11x_print("    soft reset: clk_cfg domain\n");

	/* LVDSMIPIRX_PHY_RCAL */
	offset = 0x0004;
	reg = mipi_phy_base + offset;
	rkx11x->i2c_reg_read(client, reg, &val);
	rkx11x_print("LVDSMIPIRX_PHY_RCAL(0x%08x) = 0x%08x\n", offset, val);
	if (val & BIT(0)) {
		rkx11x_print("    rcal_trim: enable\n");
	} else {
		rkx11x_print("    rcal_trim: disable, using  default value\n");

		bits_val = (val & GENMASK(4, 1)) >> 1;
		rkx11x_print("    Default value of HS-RX terminal = 0x%04x\n", bits_val);
	}
	bits_val = (val & GENMASK(12, 5)) >> 5;
	rkx11x_print("    rcal ctrl = %d\n", bits_val);
	bits_val = (val & GENMASK(16, 13)) >> 13;
	rkx11x_print("    rcal out = %d\n", bits_val);
	bits_val = (val & BIT(17)) >> 17;
	rkx11x_print("    rcal done = %d\n", bits_val);

	/* LVDSMIPIRX_ULP_RX_EN */
	offset = 0x0008;
	reg = mipi_phy_base + offset;
	rkx11x->i2c_reg_read(client, reg, &val);
	rkx11x_print("LVDSMIPIRX_ULP_RX_EN(0x%08x) = 0x%08x\n", offset, val);
	if (val & BIT(0))
		rkx11x_print("    Clock Lane LP-RX receiver enable\n");
	if (val & BIT(1))
		rkx11x_print("    Data Lane0 LP-RX receiver enable\n");
	if (val & BIT(2))
		rkx11x_print("    Data Lane1 LP-RX receiver enable\n");
	if (val & BIT(3))
		rkx11x_print("    Data Lane2 LP-RX receiver enable\n");
	if (val & BIT(4))
		rkx11x_print("    Data Lane3 LP-RX receiver enable\n");
	if (val & BIT(5))
		rkx11x_print("    Clock Lane ULP-RX receiver enable\n");
	if (val & BIT(6))
		rkx11x_print("    Data Lane0 ULP-RX receiver enable\n");
	if (val & BIT(7))
		rkx11x_print("    Data Lane1 ULP-RX receiver enable\n");
	if (val & BIT(8))
		rkx11x_print("    Data Lane2 ULP-RX receiver enable\n");
	if (val & BIT(9))
		rkx11x_print("    Data Lane3 ULP-RX receiver enable\n");

	/* LVDSMIPIRX_VOFFCAL_OUT */
	offset = 0x000c;
	reg = mipi_phy_base + offset;
	rkx11x->i2c_reg_read(client, reg, &val);
	rkx11x_print("LVDSMIPIRX_VOFFCAL_OUT(0x%08x) = 0x%08x\n", offset, val);
	if (val & BIT(5))
		rkx11x_print("    Data Lane3 HS-RX DC-Offset auto-calibration done\n");
	bits_val = (val & GENMASK(4, 0)) >> 0;
	rkx11x_print("    Data Lane3 HS-RX DC-Offset auto-calibration results = %d\n", bits_val);

	if (val & BIT(11))
		rkx11x_print("    Data Lane2 HS-RX DC-Offset auto-calibration done\n");
	bits_val = (val & GENMASK(10, 6)) >> 6;
	rkx11x_print("    Data Lane2 HS-RX DC-Offset auto-calibration results = %d\n", bits_val);

	if (val & BIT(17))
		rkx11x_print("    Data Lane1 HS-RX DC-Offset auto-calibration done\n");
	bits_val = (val & GENMASK(16, 12)) >> 12;
	rkx11x_print("    Data Lane1 HS-RX DC-Offset auto-calibration results = %d\n", bits_val);

	if (val & BIT(23))
		rkx11x_print("    Data Lane0 HS-RX DC-Offset auto-calibration done\n");
	bits_val = (val & GENMASK(22, 18)) >> 18;
	rkx11x_print("    Data Lane0 HS-RX DC-Offset auto-calibration results = %d\n", bits_val);

	if (val & BIT(29))
		rkx11x_print("    Clock Lane HS-RX DC-Offset auto-calibration done\n");
	bits_val = (val & GENMASK(28, 24)) >> 24;
	rkx11x_print("    Clock Lane HS-RX DC-Offset auto-calibration results = %d\n", bits_val);

	/* LVDSMIPIRX_CSI_CTL01 */
	offset = 0x0010;
	reg = mipi_phy_base + offset;
	rkx11x->i2c_reg_read(client, reg, &val);
	rkx11x_print("LVDSMIPIRX_CSI_CTL01(0x%08x) = 0x%08x\n", offset, val);

	/* LVDSMIPIRX_CSI_CTL23 */
	offset = 0x0014;
	reg = mipi_phy_base + offset;
	rkx11x->i2c_reg_read(client, reg, &val);
	rkx11x_print("LVDSMIPIRX_CSI_CTL23(0x%08x) = 0x%08x\n", offset, val);

	/* LVDSMIPIRX_CSI_CTL4 */
	offset = 0x0018;
	reg = mipi_phy_base + offset;
	rkx11x->i2c_reg_read(client, reg, &val);
	rkx11x_print("LVDSMIPIRX_CSI_CTL4(0x%08x) = 0x%08x\n", offset, val);

	/* LVDSMIPIRX_CSI_VINIT */
	offset = 0x001c;
	reg = mipi_phy_base + offset;
	rkx11x->i2c_reg_read(client, reg, &val);
	rkx11x_print("LVDSMIPIRX_CSI_VINIT(0x%08x) = 0x%08x\n", offset, val);
	bits_val = (val & GENMASK(3, 0)) >> 0;
	rkx11x_print("    data lane0 lprx_vinit = %d\n", bits_val);
	bits_val = (val & GENMASK(7, 4)) >> 4;
	rkx11x_print("    data lane1 lprx_vinit = %d\n", bits_val);
	bits_val = (val & GENMASK(11, 8)) >> 8;
	rkx11x_print("    data lane2 lprx_vinit = %d\n", bits_val);
	bits_val = (val & GENMASK(15, 12)) >> 12;
	rkx11x_print("    data lane3 lprx_vinit = %d\n", bits_val);
	bits_val = (val & GENMASK(19, 16)) >> 16;
	rkx11x_print("    clock lane lprx_vinit = %d\n", bits_val);
	bits_val = (val & GENMASK(23, 20)) >> 20;
	rkx11x_print("    lprx_vref_trim = %d\n", bits_val);

	/* LVDSMIPIRX_CLANE_PARA */
	offset = 0x0020;
	reg = mipi_phy_base + offset;
	rkx11x->i2c_reg_read(client, reg, &val);
	rkx11x_print("LVDSMIPIRX_CLANE_PARA(0x%08x) = 0x%08x\n", offset, val);
	bits_val = (val & GENMASK(7, 0)) >> 0;
	rkx11x_print("    t_clk_settle = %d\n", bits_val);
	bits_val = (val & GENMASK(15, 8)) >> 8;
	rkx11x_print("    t_clk_term_en = %d\n", bits_val);

	/* LVDSMIPIRX_T_HS_TERMEN */
	offset = 0x0024;
	reg = mipi_phy_base + offset;
	rkx11x->i2c_reg_read(client, reg, &val);
	rkx11x_print("LVDSMIPIRX_T_HS_TERMEN(0x%08x) = 0x%08x\n", offset, val);
	bits_val = (val & GENMASK(7, 0)) >> 0;
	rkx11x_print("    t_d0_termen = %d\n", bits_val);
	bits_val = (val & GENMASK(15, 8)) >> 8;
	rkx11x_print("    t_d1_termen = %d\n", bits_val);
	bits_val = (val & GENMASK(23, 16)) >> 16;
	rkx11x_print("    t_d2_termen = %d\n", bits_val);
	bits_val = (val & GENMASK(31, 24)) >> 24;
	rkx11x_print("    t_d3_termen = %d\n", bits_val);

	/* LVDSMIPIRX_T_HS_SETTLE */
	offset = 0x0028;
	reg = mipi_phy_base + offset;
	rkx11x->i2c_reg_read(client, reg, &val);
	rkx11x_print("LVDSMIPIRX_T_HS_SETTLE(0x%08x) = 0x%08x\n", offset, val);
	bits_val = (val & GENMASK(7, 0)) >> 0;
	rkx11x_print("    t_d0_settle = %d\n", bits_val);
	bits_val = (val & GENMASK(15, 8)) >> 8;
	rkx11x_print("    t_d1_settle = %d\n", bits_val);
	bits_val = (val & GENMASK(23, 16)) >> 16;
	rkx11x_print("    t_d2_settle = %d\n", bits_val);
	bits_val = (val & GENMASK(31, 24)) >> 24;
	rkx11x_print("    t_d3_settle = %d\n", bits_val);

	/* LVDSMIPIRX_T_CLANE_INIT */
	offset = 0x0030;
	reg = mipi_phy_base + offset;
	rkx11x->i2c_reg_read(client, reg, &val);
	rkx11x_print("LVDSMIPIRX_T_CLANE_INIT(0x%08x) = 0x%08x\n", offset, val);
	bits_val = (val & GENMASK(23, 0)) >> 0;
	rkx11x_print("    t_clk_init = %d\n", bits_val);

	/* LVDSMIPIRX_T_LANE3_INIT - LVDSMIPIRX_T_LANE0_INIT */
	for (i = 0; i < 4; i++) {
		offset = 0x0034 + 4 * i;
		reg = mipi_phy_base + offset;
		rkx11x->i2c_reg_read(client, reg, &val);
		rkx11x_print("LVDSMIPIRX_T_LANE%d_INIT(0x%08x) = 0x%08x\n", i, offset, val);
		bits_val = (val & GENMASK(23, 0)) >> 0;
		rkx11x_print("    t_d%d_init = %d\n", i, bits_val);
	}

	/* LVDSMIPIRX_TLPX_CTRL */
	offset = 0x0044;
	reg = mipi_phy_base + offset;
	rkx11x->i2c_reg_read(client, reg, &val);
	rkx11x_print("LVDSMIPIRX_TLPX_CTRL(0x%08x) = 0x%08x\n", offset, val);
	if (val & BIT(8))
		rkx11x_print("    tlpx check enable\n");
	bits_val = (val & GENMASK(7, 0)) >> 0;
	rkx11x_print("    tlpx = %d\n", bits_val);

	/* LVDSMIPIRX_NE_SWAP */
	offset = 0x0048;
	reg = mipi_phy_base + offset;
	rkx11x->i2c_reg_read(client, reg, &val);
	rkx11x_print("LVDSMIPIRX_NE_SWAP(0x%08x) = 0x%08x\n", offset, val);

	/* LVDSMIPIRX_MISC INFO */
	offset = 0x0048;
	reg = mipi_phy_base + offset;
	rkx11x->i2c_reg_read(client, reg, &val);
	rkx11x_print("LVDSMIPIRX_MISC INFO(0x%08x) = 0x%08x\n", offset, val);
	if (val & BIT(0))
		rkx11x_print("    long sotsync enable\n");
	if (val & BIT(1))
		rkx11x_print("    ulps_lp10_sel enable\n");
}

static void rkx11x_csi2host_reg_dump(struct rkx11x *rkx11x, u8 csi2host_id)
{
	struct i2c_client *client = rkx11x->client;
	u32 csi2host_base = 0;
	u32 reg = 0;
	u32 offset = 0;
	u32 val = 0, bits_val = 0;
	u32 i = 0;

	csi2host_base = csi2host_id ? RKX11X_CSI2HOST1_BASE : RKX11X_CSI2HOST0_BASE;

	/* CSI2HOST_VERSION */
	offset = 0x0000;
	reg = csi2host_base + offset;
	rkx11x->i2c_reg_read(client, reg, &val);
	rkx11x_print("CSI2HOST_VERSION(0x%08x) = 0x%08x\n", offset, val);

	/* CSI2HOST_N_LANES */
	offset = 0x0004;
	reg = csi2host_base + offset;
	rkx11x->i2c_reg_read(client, reg, &val);
	rkx11x_print("CSI2HOST_N_LANES(0x%08x) = 0x%08x\n", offset, val);
	bits_val = (val & GENMASK(1, 0)) >> 0;
	rkx11x_print("    mipi data lane = %d\n", bits_val + 1);

	/* CSI2HOST_PHY_STATE */
	offset = 0x0014;
	reg = csi2host_base + offset;
	rkx11x->i2c_reg_read(client, reg, &val);
	rkx11x_print("CSI2HOST_PHY_STATE(0x%08x) = 0x%08x\n", offset, val);
	if (val & BIT(0))
		rkx11x_print("    data lane0 module has entered the Ultra Low Power mode\n");
	if (val & BIT(1))
		rkx11x_print("    data lane1 module has entered the Ultra Low Power mode\n");
	if (val & BIT(2))
		rkx11x_print("    data lane2 module has entered the Ultra Low Power mode\n");
	if (val & BIT(3))
		rkx11x_print("    data lane3 module has entered the Ultra Low Power mode\n");

	if (val & BIT(4))
		rkx11x_print("    data lane0 in Stop state\n");
	if (val & BIT(5))
		rkx11x_print("    data lane1 in Stop state\n");
	if (val & BIT(6))
		rkx11x_print("    data lane2 in Stop state\n");
	if (val & BIT(7))
		rkx11x_print("    data lane3 in Stop state\n");

	if (val & BIT(8))
		rkx11x_print("    clock lane is actively receiving a DDR clock\n");

	if (val & BIT(9))
		rkx11x_print("    clock lane module has entered the Ultra Low Power state\n");
	if (val & BIT(10))
		rkx11x_print("    clock lane in Stop state\n");

	if (val & BIT(11))
		rkx11x_print("    data lane0: high-speed receive active\n");
	if (val & BIT(12))
		rkx11x_print("    data lane1: high-speed receive active\n");
	if (val & BIT(13))
		rkx11x_print("    data lane2: high-speed receive active\n");
	if (val & BIT(14))
		rkx11x_print("    data lane3: high-speed receive active\n");

	/* CSI2HOST_ERR1 */
	offset = 0x0020;
	reg = csi2host_base + offset;
	rkx11x->i2c_reg_read(client, reg, &val);
	rkx11x_print("CSI2HOST_ERR1(0x%08x) = 0x%08x\n", offset, val);

	/* CSI2HOST_ERR2 */
	offset = 0x0024;
	reg = csi2host_base + offset;
	rkx11x->i2c_reg_read(client, reg, &val);
	rkx11x_print("CSI2HOST_ERR2(0x%08x) = 0x%08x\n", offset, val);

	/* CSI2HOST_MSK1 */
	offset = 0x0028;
	reg = csi2host_base + offset;
	rkx11x->i2c_reg_read(client, reg, &val);
	rkx11x_print("CSI2HOST_MSK1(0x%08x) = 0x%08x\n", offset, val);

	/* CSI2HOST_MSK2 */
	offset = 0x002c;
	reg = csi2host_base + offset;
	rkx11x->i2c_reg_read(client, reg, &val);
	rkx11x_print("CSI2HOST_MSK2(0x%08x) = 0x%08x\n", offset, val);

	/* CSI2HOST_CONTROL */
	offset = 0x0040;
	reg = csi2host_base + offset;
	rkx11x->i2c_reg_read(client, reg, &val);
	rkx11x_print("CSI2HOST_CONTROL(0x%08x) = 0x%08x\n", offset, val);
	if (val & BIT(0))
		rkx11x_print("    cphy enable\n");
	else
		rkx11x_print("    dphy enable\n");

	if (val & BIT(4))
		rkx11x_print("    dsi enable\n");
	else
		rkx11x_print("    csi2 enable\n");

	if (val & BIT(5))
		rkx11x_print("    Old rtl code for dphy adapter\n");
	else
		rkx11x_print("    New rtl code for dphy adapter\n");

	bits_val = (val & GENMASK(13, 8)) >> 8;
	rkx11x_print("    datatype of frame start = %d\n", bits_val);
	bits_val = (val & GENMASK(19, 14)) >> 14;
	rkx11x_print("    datatype of frame end = %d\n", bits_val);
	bits_val = (val & GENMASK(25, 20)) >> 20;
	rkx11x_print("    datatype of line start = %d\n", bits_val);
	bits_val = (val & GENMASK(31, 26)) >> 26;
	rkx11x_print("    datatype of line end = %d\n", bits_val);

	/* CSI2HOST_HEADER_0 - CSI2HOST_HEADER_3 */
	for (i = 0; i < 4; i++) {
		offset = 0x0050 + 4 * i;
		reg = csi2host_base + offset;
		rkx11x->i2c_reg_read(client, reg, &val);
		rkx11x_print("CSI2HOST_HEADER_%d(0x%08x) = 0x%08x\n", i, offset, val);
		bits_val = (val & GENMASK(5, 0)) >> 0;
		rkx11x_print("    datatype = 0x%x\n", bits_val);
		bits_val = (val & GENMASK(7, 6)) >> 6;
		rkx11x_print("    vc = %d\n", bits_val);
		bits_val = (val & GENMASK(23, 8)) >> 8;
		rkx11x_print("    word count = %d\n", bits_val);
		bits_val = (val & GENMASK(29, 24)) >> 24;
		rkx11x_print("    ecc = %d\n", bits_val);
	}
}

static void rkx11x_vicap_dvp_rx_reg_dump(struct rkx11x *rkx11x)
{
	struct i2c_client *client = rkx11x->client;
	u32 vicap_base = RKX11X_VICAP_BASE;
	u32 reg = 0;
	u32 offset = 0;
	u32 val = 0, bits_val = 0;

	/* VICAP_DVP_CTRL */
	offset = 0x0010;
	reg = vicap_base + offset;
	rkx11x->i2c_reg_read(client, reg, &val);
	rkx11x_print("VICAP_DVP_CTRL(0x%08x) = 0x%08x\n", offset, val);
	if (val & BIT(0))
		rkx11x_print("    dvp enable\n");
	else
		rkx11x_print("    dvp disable\n");

	/* VICAP_DVP_INTEN */
	offset = 0x0014;
	reg = vicap_base + offset;
	rkx11x->i2c_reg_read(client, reg, &val);
	rkx11x_print("VICAP_DVP_INTEN(0x%08x) = 0x%08x\n", offset, val);

	/* VICAP_DVP_INTSTAT */
	offset = 0x0018;
	reg = vicap_base + offset;
	rkx11x->i2c_reg_read(client, reg, &val);
	rkx11x_print("VICAP_DVP_INTSTAT(0x%08x) = 0x%08x\n", offset, val);
	if (val & BIT(0))
		rkx11x_print("    frame start interrupt\n");
	if (val & BIT(8))
		rkx11x_print("    frame end interrupt\n");

	/* VICAP_DVP_FORMAT */
	offset = 0x001c;
	reg = vicap_base + offset;
	rkx11x->i2c_reg_read(client, reg, &val);
	rkx11x_print("VICAP_DVP_FORMAT(0x%08x) = 0x%08x\n", offset, val);
	if (val & BIT(0))
		rkx11x_print("    vsync: high active\n");
	else
		rkx11x_print("    vsync: low active\n");
	if (val & BIT(1))
		rkx11x_print("    href: high active\n");
	else
		rkx11x_print("    href: low active\n");

	bits_val = (val & GENMASK(4, 2)) >> 2;
	rkx11x_print("    input mode = %d\n", bits_val);
	bits_val = (val & GENMASK(6, 5)) >> 5;
	rkx11x_print("    input yuv order = %d\n", bits_val);
	bits_val = (val & GENMASK(8, 7)) >> 7;
	rkx11x_print("    input raw width = %d\n", bits_val);

	/* VICAP_DVP_PIX_NUM_ID0 */
	offset = 0x0084;
	reg = vicap_base + offset;
	rkx11x->i2c_reg_read(client, reg, &val);
	rkx11x_print("VICAP_DVP_PIX_NUM_ID0(0x%08x) = 0x%08x\n", offset, val);
	bits_val = (val & GENMASK(13, 0)) >> 0;
	rkx11x_print("    id0 Y pixel number in one line = %d\n", bits_val);

	/* VICAP_DVP_LINE_NUM_ID0 */
	offset = 0x0088;
	reg = vicap_base + offset;
	rkx11x->i2c_reg_read(client, reg, &val);
	rkx11x_print("VICAP_DVP_LINE_NUM_ID0(0x%08x) = 0x%08x\n", offset, val);
	bits_val = (val & GENMASK(13, 0)) >> 0;
	rkx11x_print("    id0 Y line number in one frame = %d\n", bits_val);
}

static void vicap_mipi_id_reg_dump(struct rkx11x *rkx11x, u32 vicap_base, u32 id)
{
	struct i2c_client *client = rkx11x->client;
	u32 reg = 0;
	u32 offset = 0;
	u32 val = 0, bits_val = 0;

	/* ID CTRL0 */
	offset = 0x0100 + 8 * id;
	reg = vicap_base +  + offset;
	rkx11x->i2c_reg_read(client, reg, &val);
	rkx11x_print("VICAP_MIPI_ID%d_CTRL0(0x%08x) = 0x%08x\n", id, offset, val);
	if (val & BIT(0)) {
		rkx11x_print("    id%d: capture enable\n", id);

		bits_val = (val & GENMASK(3, 1)) >> 1;
		rkx11x_print("    id%d: parse type = 0x%x\n", id, bits_val);
		bits_val = (val & GENMASK(9, 8)) >> 8;
		rkx11x_print("    id%d: virtual channel = %d\n", id, bits_val);
		bits_val = (val & GENMASK(15, 10)) >> 10;
		rkx11x_print("    id%d: data type = 0x%x\n", id, bits_val);

		/* ID CTRL1 */
		offset = 0x0104 + 8 * id;
		reg = vicap_base +  + offset;
		rkx11x->i2c_reg_read(client, reg, &val);
		rkx11x_print("VICAP_MIPI_ID%d_CTRL1(0x%08x) = 0x%08x\n", id, offset, val);
		bits_val = (val & GENMASK(13, 0)) >> 0;
		rkx11x_print("    id%d: width = %d\n", id, bits_val);
		bits_val = (val & GENMASK(29, 16)) >> 16;
		rkx11x_print("    id%d: height = %d\n", id, bits_val);

		/* VC FRAME_NUM */
		offset = 0x0019c + 4 * id;
		reg = vicap_base + offset;
		rkx11x->i2c_reg_read(client, reg, &val);
		rkx11x_print("VICAP_MIPI_FRAME_NUM_VC%d(0x%08x) = 0x%08x\n", id, offset, val);
		bits_val = (val & GENMASK(15, 0)) >> 0;
		rkx11x_print("    vc%d: frame number of frame start = %d\n", id, bits_val);
		bits_val = (val & GENMASK(31, 16)) >> 16;
		rkx11x_print("    vc%d: frame number of frame end = %d\n", id, bits_val);

		/* ID SIZE_NUM */
		offset = 0x001c0 + 4 * id;
		reg = vicap_base + offset;
		rkx11x->i2c_reg_read(client, reg, &val);
		rkx11x_print("VICAP_MIPI_LVDS_SIZE_NUM_ID%d(0x%08x) = 0x%08x\n", id, offset, val);
		bits_val = (val & GENMASK(13, 0)) >> 0;
		rkx11x_print("    id%d: pixel number in one line = %d\n", id, bits_val);
		bits_val = (val & GENMASK(29, 16)) >> 16;
		rkx11x_print("    id%d: line number in one frame = %d\n", id, bits_val);
	} else {
		rkx11x_print("    id%d: capture disable\n", id);
	}
}

static void rkx11x_vicap_csi_reg_dump(struct rkx11x *rkx11x)
{
	struct i2c_client *client = rkx11x->client;
	u32 vicap_base = RKX11X_VICAP_BASE;
	u32 reg = 0;
	u32 offset = 0;
	u32 val = 0;
	u32 i = 0;

	/* VICAP_MIPI_ID0_CTRL0 - VICAP_MIPI_ID3_CTRL0 */
	/* VICAP_MIPI_ID0_CTRL1 - VICAP_MIPI_ID3_CTRL1 */
	/* VICAP_MIPI_FRAME_NUM_VC0 - VICAP_MIPI_FRAME_NUM_VC3 */
	/* VICAP_MIPI_LVDS_SIZE_NUM_ID0 - VICAP_MIPI_LVDS_SIZE_NUM_ID3 */
	for (i = 0; i < 4; i++)
		vicap_mipi_id_reg_dump(rkx11x, vicap_base, i);

	/* VICAP_MIPI_LVDS_INTEN */
	offset = 0x00174;
	reg = vicap_base + offset;
	rkx11x->i2c_reg_read(client, reg, &val);
	rkx11x_print("VICAP_MIPI_LVDS_INTEN(0x%08x) = 0x%08x\n", offset, val);

	/* VICAP_MIPI_LVDS_INTSTAT */
	offset = 0x00178;
	reg = vicap_base + offset;
	rkx11x->i2c_reg_read(client, reg, &val);
	rkx11x_print("VICAP_MIPI_LVDS_INTSTAT(0x%08x) = 0x%08x\n", offset, val);
	if (val & BIT(0))
		rkx11x_print("    id0 frame start\n");
	if (val & BIT(2))
		rkx11x_print("    id1 frame start\n");
	if (val & BIT(4))
		rkx11x_print("    id2 frame start\n");
	if (val & BIT(6))
		rkx11x_print("    id3 frame start\n");

	if (val & BIT(8))
		rkx11x_print("    id0 frame end\n");
	if (val & BIT(10))
		rkx11x_print("    id1 frame end\n");
	if (val & BIT(12))
		rkx11x_print("    id2 frame end\n");
	if (val & BIT(14))
		rkx11x_print("    id3 frame end\n");

	if (val & BIT(19))
		rkx11x_print("    fifo overflow\n");
}

static void rkx11x_linktx_dump_serdes_ctrl(u32 val)
{
	u32 bits_val = 0;

	if (val & BIT(0))
		rkx11x_print("    serdes enable\n");
	else
		rkx11x_print("    serdes disable\n");

	if (val & BIT(1))
		rkx11x_print("    two lane work\n");
	else
		rkx11x_print("    one lane work\n");

	bits_val = (val & GENMASK(3, 2)) >> 2;
	rkx11x_print("    serdes data width: %d\n", 8 * (bits_val + 1));

	bits_val = (val & GENMASK(5, 4)) >> 4;
	rkx11x_print("    channel 0 device id: %d\n", bits_val);

	bits_val = (val & GENMASK(7, 6)) >> 6;
	rkx11x_print("    channel 1 device id: %d\n", bits_val);

	if (val & BIT(8))
		rkx11x_print("    channel 1 enable\n");
	else
		rkx11x_print("    channel 1 disable\n");

	if (val & BIT(9))
		rkx11x_print("    replication enable\n");

	if (val & BIT(10))
		rkx11x_print("    lane exchange enable\n");

	if (val & BIT(11))
		rkx11x_print("    video enable\n");
	else
		rkx11x_print("    video disable\n");

	if (val & BIT(12))
		rkx11x_print("    display type\n");
	else
		rkx11x_print("    camera type\n");

	bits_val = (val & GENMASK(14, 13)) >> 13;
	if (bits_val == 0)
		rkx11x_print("    train clock select engine0 pixel clock\n");
	else if (bits_val == 1)
		rkx11x_print("    train clock select engine1 pixel clock\n");
	else
		rkx11x_print("    train clock select i2s sclk\n");

	if (val & BIT(16))
		rkx11x_print("    video channel 0 status: stop\n");
	else
		rkx11x_print("    video channel 0 status: normal\n");

	if (val & BIT(17))
		rkx11x_print("    video channel 1 status: stop\n");
	else
		rkx11x_print("    video channel 1 status: normal\n");

	if (val & BIT(18))
		rkx11x_print("    audio status: stop\n");
	else
		rkx11x_print("    audio status: normal\n");

	if (val & BIT(30))
		rkx11x_print("    video channel 0: enable\n");
	else
		rkx11x_print("    video channel 0: disable\n");

	if (val & BIT(31))
		rkx11x_print("    video channel 1: enable\n");
	else
		rkx11x_print("    video channel 1: disable\n");
}

static void rkx11x_linktx_dump_video_ctrl(u32 val, u32 cam_display_sel)
{
	u32 bits_val = 0;

	bits_val = (val & GENMASK(2, 0)) >> 0;
	if (cam_display_sel) {
		rkx11x_print("    input interface: display type\n");
		if (bits_val == 0)
			rkx11x_print("    input: dsi\n");
		else if (bits_val == 1)
			rkx11x_print("    input: dual_lvds\n");
		else if (bits_val == 2)
			rkx11x_print("    input: lvds0\n");
		else if (bits_val == 3)
			rkx11x_print("    input: lvds1\n");
		else if (bits_val == 5)
			rkx11x_print("    input: rgb\n");
	} else {
		rkx11x_print("    input interface: camera type\n");
		if (bits_val == 0)
			rkx11x_print("    input: csi\n");
		else if (bits_val == 1)
			rkx11x_print("    input: lvds\n");
		else if (bits_val == 2)
			rkx11x_print("    input: dvp\n");
	}

	if (val & BIT(3))
		rkx11x_print("    pixel vsync sel\n");

	bits_val = (val & GENMASK(13, 4)) >> 4;
	rkx11x_print("    dual lvds cycle diff num = %d\n", bits_val);

	bits_val = (val & GENMASK(29, 16)) >> 16;
	rkx11x_print("    video packet length = %d\n", bits_val);
}

static void rkx11x_linktx_dump_fifo_status(u32 val)
{
	if (val & BIT(0))
		rkx11x_print("    dsi ch0 fifo overflow\n");
	if (val & BIT(1))
		rkx11x_print("    dsi ch1 fifo overflow\n");

	if (val & BIT(2))
		rkx11x_print("    lvds0 fifo overflow\n");
	if (val & BIT(3))
		rkx11x_print("    lvds1 fifo overflow\n");

	if (val & BIT(4))
		rkx11x_print("    audio data fifo overflow\n");

	if (val & BIT(5))
		rkx11x_print("    ch0 data fifo overflow\n");
	if (val & BIT(6))
		rkx11x_print("    ch1 data fifo overflow\n");

	if (val & BIT(7))
		rkx11x_print("    ch0 cmd fifo overflow\n");
	if (val & BIT(8))
		rkx11x_print("    ch1 cmd fifo overflow\n");

	if (val & BIT(16))
		rkx11x_print("    dsi ch0 fifo underrun\n");
	if (val & BIT(17))
		rkx11x_print("    dsi ch1 fifo underrun\n");

	if (val & BIT(18))
		rkx11x_print("    lvds0 fifo underrun\n");
	if (val & BIT(19))
		rkx11x_print("    lvds1 fifo underrun\n");

	if (val & BIT(20))
		rkx11x_print("    audio data fifo underrun\n");

	if (val & BIT(21))
		rkx11x_print("    ch0 data fifo underrun\n");
	if (val & BIT(22))
		rkx11x_print("    ch1 data fifo underrun\n");

	if (val & BIT(23))
		rkx11x_print("    ch0 cmd fifo underrun\n");
	if (val & BIT(24))
		rkx11x_print("    ch1 cmd fifo underrun\n");
}

static void rkx11x_linktx_reg_dump(struct rkx11x *rkx11x)
{
	struct i2c_client *client = rkx11x->client;
	u32 reg = 0;
	u32 offset = 0;
	u32 val = 0;
	u32 cam_display_sel = 0;

	/* link ready status */
	reg = SER_GRF_SOC_STATUS0;
	rkx11x->i2c_reg_read(client, reg, &val);
	rkx11x_print("SER_GRF_SOC_STATUS0(0x%08x) = 0x%08x\n", reg, val);
	if (val & SER_PCS1_READY)
		rkx11x_print("    ser pcs1 ready\n");
	if (val & SER_PCS0_READY)
		rkx11x_print("    ser pcs0 ready\n");

	/* PMA status */
	reg = SER_GRF_SOC_CON7;
	rkx11x->i2c_reg_read(client, reg, &val);
	rkx11x_print("SER_GRF_SOC_CON7(0x%08x) = 0x%08x\n", reg, val);
	if (val & BIT(8))
		rkx11x_print("    PMA0 enable\n");
	else
		rkx11x_print("    PMA0 disable\n");
	if (val & BIT(9))
		rkx11x_print("    PMA1 enable\n");
	else
		rkx11x_print("    PMA1 disable\n");
	if (val & BIT(15))
		rkx11x_print("    PMA1 control PMA_PLL\n");
	else
		rkx11x_print("    PMA0 control PMA_PLL\n");

	/* SER_RKLINK_SERDES_CTRL */
	offset = 0x0000;
	reg = RKX11X_SER_RKLINK_BASE + offset;
	rkx11x->i2c_reg_read(client, reg, &val);
	rkx11x_print("SER_RKLINK_SERDES_CTRL(0x%08x) = 0x%08x\n", offset, val);
	cam_display_sel = val & BIT(12);
	rkx11x_linktx_dump_serdes_ctrl(val);

	/* RKLINK_TX_VIDEO_CTRL */
	offset = 0x0004;
	reg = RKX11X_SER_RKLINK_BASE + offset;
	rkx11x->i2c_reg_read(client, reg, &val);
	rkx11x_print("RKLINK_TX_VIDEO_CTRL(0x%08x) = 0x%08x\n", offset, val);
	rkx11x_linktx_dump_video_ctrl(val, cam_display_sel);

	/* RKLINK_TX_FIFO_STATUS */
	offset = 0x003C;
	reg = RKX11X_SER_RKLINK_BASE + offset;
	rkx11x->i2c_reg_read(client, reg, &val);
	rkx11x_print("RKLINK_TX_FIFO_STATUS(0x%08x) = 0x%08x\n", offset, val);
	rkx11x_linktx_dump_fifo_status(val);

	/* RKLINK_TX_SOURCE_IRQ_EN */
	offset = 0x0040;
	reg = RKX11X_SER_RKLINK_BASE + offset;
	rkx11x->i2c_reg_read(client, reg, &val);
	rkx11x_print("RKLINK_TX_SOURCE_IRQ_EN(0x%08x) = 0x%08x\n", offset, val);
}

int rkx11x_module_dump(struct rkx11x *rkx11x, u32 module_id)
{
	rkx11x_print("module dump: module_id = %d\n", module_id);

	switch (module_id) {
	case RKX11X_DUMP_GRF:
		rkx11x_grf_reg_dump(rkx11x);
		break;
	case RKX11X_DUMP_LINKTX:
		rkx11x_linktx_reg_dump(rkx11x);
		break;
	case RKX11X_DUMP_GRF_MIPI0:
		rkx11x_mipi_grf_reg_dump(rkx11x, 0);
		break;
	case RKX11X_DUMP_GRF_MIPI1:
		rkx11x_mipi_grf_reg_dump(rkx11x, 1);
		break;
	case RKX11X_DUMP_RXPHY0:
		rkx11x_mipirxphy_reg_dump(rkx11x, 0);
		break;
	case RKX11X_DUMP_RXPHY1:
		rkx11x_mipirxphy_reg_dump(rkx11x, 1);
		break;
	case RKX11X_DUMP_CSI2HOST0:
		rkx11x_csi2host_reg_dump(rkx11x, 0);
		break;
	case RKX11X_DUMP_CSI2HOST1:
		rkx11x_csi2host_reg_dump(rkx11x, 1);
		break;
	case RKX11X_DUMP_VICAP_CSI:
		rkx11x_vicap_csi_reg_dump(rkx11x);
		break;
	case RKX11X_DUMP_VICAP_DVP:
		rkx11x_vicap_dvp_rx_reg_dump(rkx11x);
		break;
	default:
		rkx11x_print("module id = %d is not support\n", module_id);
		return -1;
	}

	return 0;
}
EXPORT_SYMBOL(rkx11x_module_dump);
