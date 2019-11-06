/*
*
* SPDX-License-Identifier: GPL-2.0
*
* Copyright (C) 2018 Amlogic or its affiliates
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

#ifndef __SYSTEM_AM_MIPI_H__
#define __SYSTEM_AM_MIPI_H__

#include <linux/module.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/i2c.h>


#define HI_CSI_PHY_CNTL0	0x4C
#define HI_CSI_PHY_CNTL1	0x50
#define HI_CSI_PHY_CNTL2	0x54
#define HI_CSI_PHY_CNTL3	0x58

#define MIPI_PHY_CTRL	0x00
#define MIPI_PHY_CLK_LANE_CTRL	0x04
#define MIPI_PHY_DATA_LANE_CTRL	0x08
#define MIPI_PHY_DATA_LANE_CTRL1	0x0C
#define MIPI_PHY_TCLK_MISS	0x10
#define MIPI_PHY_TCLK_SETTLE	0x14
#define MIPI_PHY_THS_EXIT	0x18
#define MIPI_PHY_THS_SKIP	0x1C
#define MIPI_PHY_THS_SETTLE	0x20
#define MIPI_PHY_TINIT	0x24
#define MIPI_PHY_TULPS_C	0x28
#define MIPI_PHY_TULPS_S	0x2C
#define MIPI_PHY_TMBIAS		0x30
#define MIPI_PHY_TLP_EN_W	0x34
#define MIPI_PHY_TLPOK	0x38
#define MIPI_PHY_TWD_INIT	0x3C
#define MIPI_PHY_TWD_HS		0x40
#define MIPI_PHY_AN_CTRL0	0x44
#define MIPI_PHY_AN_CTRL1	0x48
#define MIPI_PHY_AN_CTRL2	0x4C
#define MIPI_PHY_CLK_LANE_STS	0x50
#define MIPI_PHY_DATA_LANE0_STS	0x54
#define MIPI_PHY_DATA_LANE1_STS	0x58
#define MIPI_PHY_DATA_LANE2_STS	0x5C
#define MIPI_PHY_DATA_LANE3_STS	0x60
#define MIPI_PHY_INT_STS	0x6C
#define MIPI_PHY_MUX_CTRL0  (0x61 << 2)
#define MIPI_PHY_MUX_CTRL1	(0x62 << 2)



#define MIPI_CSI_VERSION	0x000
#define MIPI_CSI_N_LANES	0x004
#define MIPI_CSI_PHY_SHUTDOWNZ 0x008
#define MIPI_CSI_DPHY_RSTZ	0x00C
#define MIPI_CSI_CSI2_RESETN	0x010
#define MIPI_CSI_PHY_STATE	0x014
#define MIPI_CSI_DATA_IDS_1	0x018
#define MIPI_CSI_DATA_IDS_2	0x01C

struct am_mipi {
	struct device_node *of_node;
	struct device_node *link_node;
	struct resource csi2_phy0_reg;
	struct resource csi2_phy1_reg;
	struct resource aphy_reg;
	struct resource csi0_host_reg;
	struct resource csi1_host_reg;
	void __iomem *csi2_phy0;
	void __iomem *csi2_phy1;
	void __iomem *aphy;
	void __iomem *csi0_host;
	void __iomem *csi1_host;
	int phy_irq;
	int csi_irq;
	uint32_t aphy_ctrl3_cfg;
	uint32_t dphy0_ctrl0_cfg;
	uint32_t dphy0_ctrl1_cfg;
};

typedef struct am_mipi_info {
	uint32_t ui_val;
	uint8_t channel;
	uint8_t lanes;

	uint32_t csi_version;
	uint32_t fte1_flag;
}am_mipi_info_t;

int am_mipi_parse_dt(struct device_node *node);
void am_mipi_deinit_parse_dt(void);
int am_mipi_init(void *info);
void am_mipi_set_lanes(int lanes);
void am_mipi_deinit(void);



#endif


