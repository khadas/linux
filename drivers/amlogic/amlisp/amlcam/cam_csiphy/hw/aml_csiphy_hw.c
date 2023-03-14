/*
*
* SPDX-License-Identifier: GPL-2.0
*
* Copyright (C) 2020 Amlogic or its affiliates
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; version 2.
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
* or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
* for more details.
* You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*
*/

#define pr_fmt(fmt)  "aml-csiphy:%s:%d: " fmt, __func__, __LINE__

#include <linux/io.h>
#include "../aml_csiphy.h"
#include "aml_csiphy_hw.h"

#define CYCLE_TIME 5

static int mipi_reg_write(void *c_dev, int idx, u32 addr, u32 val)
{
	int rtn = -1;
	void __iomem *base = NULL;
	struct csiphy_dev_t *csiphy_dev = c_dev;

	switch(idx) {
	case DPHY_MD:
		base = csiphy_dev->csi_dphy + MPPI_DPHY_BASE;
	break;
	case HOST_MD:
		base = csiphy_dev->csi_dphy + MPPI_HOST_BASE;
	break;
	case APHY_MD:
		base = csiphy_dev->csi_aphy;
	break;
	case PROC_MD:
		base = csiphy_dev->csi_dphy + MPPI_PROC_BASE;
	break;

	default:
		pr_err("Error input idx\n");
	return rtn;
	}

	writel(val, base + addr);

	return 0;
}

static int mipi_reg_read(void *c_dev, int idx, u32 addr, u32 *val)
{
	int rtn = -1;
	void __iomem *base = NULL;
	struct csiphy_dev_t *csiphy_dev = c_dev;

	switch (idx) {
	case DPHY_MD:
		base = csiphy_dev->csi_dphy + MPPI_DPHY_BASE;
	break;
	case HOST_MD:
		base = csiphy_dev->csi_dphy + MPPI_HOST_BASE;
	break;
	case APHY_MD:
		base = csiphy_dev->csi_aphy;
	break;
	case PROC_MD:
		base = csiphy_dev->csi_dphy + MPPI_PROC_BASE;
	break;

	default:
		pr_err("Error input idx\n");
	return rtn;
	}

	*val = readl(base + addr);

	return 0;

}


static int aphy_cfg(void *c_dev, int idx, int lanes, int bps)
{
	int module  = APHY_MD;

	if (idx < 2) {
		mipi_reg_write(c_dev, module, MIPI_CSI_2M_PHY2_CNTL1, 0x3f425c00);
		if (lanes == 4) {
			mipi_reg_write(c_dev, module, MIPI_CSI_2M_PHY2_CNTL2, 0x033a0000);
		} else {
			mipi_reg_write(c_dev, module, MIPI_CSI_2M_PHY2_CNTL2, 0x333a0000);
		}
		if (lanes <= 2) {
			mipi_reg_write(c_dev, module, MIPI_CSI_2M_PHY2_CNTL3, 0x3800000);
		}
	} else {
		mipi_reg_write(c_dev, module, MIPI_CSI_2M_PHY2_CNTL4, 0x3f425c00);
		if (lanes == 4) {
			mipi_reg_write(c_dev, module, MIPI_CSI_2M_PHY2_CNTL5, 0x033a0000);
		} else {
			mipi_reg_write(c_dev, module, MIPI_CSI_2M_PHY2_CNTL5, 0x333a0000);
		}
		if (lanes <= 2) {
			mipi_reg_write(c_dev, module, MIPI_CSI_2M_PHY2_CNTL6, 0x3800000);
		}
	}

	return 0;
}

static int dphy_cfg(void *c_dev, int idx, int lanes, u32 bps)
{
	u32 ui_val = 0;
	u32 settle = 0;
	int module = DPHY_MD;
	struct csiphy_dev_t *csiphy_dev = c_dev;

	ui_val = 1000 / bps;
	if ((1000 % bps) != 0)
        	ui_val += 1;

	settle = (85 + 145 + (16 * ui_val)) / 2;
	settle = settle / 5;

	//mipi_reg_write(c_dev, module, MIPI_PHY_CTRL, 0x80000000);//soft reset bit
	//mipi_reg_write(c_dev, module, MIPI_PHY_CTRL, 0);//release soft reset bit
	if (csiphy_dev->clock_mode) {
		mipi_reg_write(c_dev, module, MIPI_PHY_CLK_LANE_CTRL, 0x58);//MIPI_PHY_CLK_LANE_CTRL
	} else {
		mipi_reg_write(c_dev, module, MIPI_PHY_CLK_LANE_CTRL, 0x3d8);//3d8:continue mode
	}
	mipi_reg_write(c_dev, module, MIPI_PHY_TCLK_MISS, 0x9);// clck miss = 50 ns --(x< 60 ns)
	mipi_reg_write(c_dev, module, MIPI_PHY_TCLK_SETTLE, 0x1f);// clck settle = 160 ns --(95ns< x < 300 ns)
	mipi_reg_write(c_dev, module, MIPI_PHY_THS_EXIT, 0x8);// hs exit = 160 ns --(x>100ns)
	mipi_reg_write(c_dev, module, MIPI_PHY_THS_SKIP, 0xa);// hs skip = 55 ns --(40ns<x<55ns+4*UI)
	mipi_reg_write(c_dev, module, MIPI_PHY_THS_SETTLE, settle);// hs settle = 160 ns --(85 ns + 6*UI<x<145 ns + 10*UI)
	mipi_reg_write(c_dev, module, MIPI_PHY_TINIT, 0x4e20);// >100us
	mipi_reg_write(c_dev, module, MIPI_PHY_TMBIAS,0x100);
	mipi_reg_write(c_dev, module, MIPI_PHY_TULPS_C,0x1000);
	mipi_reg_write(c_dev, module, MIPI_PHY_TULPS_S,0x100);
	mipi_reg_write(c_dev, module, MIPI_PHY_TLP_EN_W, 0x0c);
	mipi_reg_write(c_dev, module, MIPI_PHY_TLPOK, 0x100);
	mipi_reg_write(c_dev, module, MIPI_PHY_TWD_INIT, 0x400000);
	mipi_reg_write(c_dev, module, MIPI_PHY_TWD_HS, 0x400000);
	mipi_reg_write(c_dev, module, MIPI_PHY_DATA_LANE_CTRL, 0x0);
	mipi_reg_write(c_dev, module, MIPI_PHY_DATA_LANE_CTRL1, 0x3 | (0x1f << 2 ) | (0x3 << 7));//446mbps 5ff, 223mbps 1ff    // enable data lanes pipe line and hs sync bit err.
	if (lanes <= 2) {
		if ((idx & 0x01) == 0) {
			mipi_reg_write(c_dev, module, MIPI_PHY_MUX_CTRL0, 0x000001ff);
			mipi_reg_write(c_dev, module, MIPI_PHY_MUX_CTRL1, 0x000201ff);
		} else {
			mipi_reg_write(c_dev, module, MIPI_PHY_MUX_CTRL0, 0x123ff);
			mipi_reg_write(c_dev, module, MIPI_PHY_MUX_CTRL1, 0x1ff01);
		}
	} else {
		mipi_reg_write(c_dev, module, MIPI_PHY_MUX_CTRL0, 0x00000123);
		mipi_reg_write(c_dev, module, MIPI_PHY_MUX_CTRL1, 0x00020123);
	}
	//mipi_reg_write(c_dev, module, MIPI_PHY_DATA_LANE_CTRL1	, 0x1fd);
	mipi_reg_write(c_dev, module, MIPI_PHY_CTRL, 0);

	return 0;
}

static int host_cfg(void *c_dev, int idx, u32 lanes)
{
	int module = HOST_MD;
	mipi_reg_write(c_dev, module, CSI2_HOST_CSI2_RESETN, 0);
	mipi_reg_write(c_dev, module, CSI2_HOST_CSI2_RESETN, 0xFFFFFFFF);
	mipi_reg_write(c_dev, module,  CSI2_HOST_N_LANES    , lanes);
	mipi_reg_write(c_dev, module,  CSI2_HOST_MASK1      , 0xFFFFFFFF);
	return 0;
}

static void mipi_reset(void *c_dev, int idx)
{
	u32 data = 0;
	int module = DPHY_MD;

	data = 0x1f; //disable lanes digital clock
	data |= 0x1 << 31; //soft reset bit
	mipi_reg_write(c_dev, module, MIPI_PHY_CTRL, data);
}

static u32 csiphy_hw_version(void *c_dev, int idx)
{
	u32 version = 0xc0ff;

	return version;
}

static void csiphy_hw_reset(void *c_dev, int idx)
{
	struct csiphy_dev_t *csiphy_dev = c_dev;

	mipi_reset(c_dev, idx);

	pr_info("CSIPHY%u: hw reset\n", csiphy_dev->index);

	return;
}

static int csiphy_hw_start(void *c_dev, int idx, int lanes, s64 link_freq)
{
	u32 bps = 0;
	u32 freq = 0;
	struct csiphy_dev_t *csiphy_dev = c_dev;

	freq = (u32)link_freq;
	bps = freq / 1000000;

	aphy_cfg(c_dev, idx, lanes, bps);
	dphy_cfg(c_dev, idx, lanes, bps);
	host_cfg(c_dev, idx, lanes - 1);
	pr_info("CSIPHY%u: hw start\n", csiphy_dev->index);

	return 0;
}

static void csiphy_hw_stop(void *c_dev, int idx)
{
	struct csiphy_dev_t *csiphy_dev = c_dev;

	mipi_reset(c_dev, idx);

	pr_info("CSIPHY%u: hw stop\n", csiphy_dev->index);

	return;
}

const struct csiphy_dev_ops csiphy_dev_hw_ops = {
	.hw_reset = csiphy_hw_reset,
	.hw_version = csiphy_hw_version,
	.hw_start = csiphy_hw_start,
	.hw_stop = csiphy_hw_stop,
};
