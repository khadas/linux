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
#include "acamera_firmware_config.h"

//---------------- C1_phy_host_base -----------------
/*
csi2_phy0: 0xfe008000
csi2_phy1: 0xfe008400
aphy_reg:  0xfe007c00
csi0_host: 0xfe022000
csi1_host: 0xfe022400
*/

#define MIPI_PHY_CTRL                   (0x00 << 2)
#define MIPI_PHY_CLK_LANE_CTRL          (0x01 << 2)
#define MIPI_PHY_DATA_LANE_CTRL         (0x02 << 2)
#define MIPI_PHY_DATA_LANE_CTRL1        (0x03 << 2)
#define MIPI_PHY_TCLK_MISS              (0x04 << 2)
#define MIPI_PHY_TCLK_SETTLE            (0x05 << 2)
#define MIPI_PHY_THS_EXIT               (0x06 << 2)
#define MIPI_PHY_THS_SKIP               (0x07 << 2)
#define MIPI_PHY_THS_SETTLE             (0x08 << 2)
#define MIPI_PHY_TINIT                  (0x09 << 2)
#define MIPI_PHY_TULPS_C                (0x0a << 2)
#define MIPI_PHY_TULPS_S                (0x0b << 2)
#define MIPI_PHY_TMBIAS                 (0x0c << 2)
#define MIPI_PHY_TLP_EN_W               (0x0d << 2)
#define MIPI_PHY_TLPOK                  (0x0e << 2)
#define MIPI_PHY_TWD_INIT               (0x0f << 2)
#define MIPI_PHY_TWD_HS                 (0x10 << 2)
#define MIPI_PHY_AN_CTRL0               (0x11 << 2)
#define MIPI_PHY_AN_CTRL1               (0x12 << 2)
#define MIPI_PHY_AN_CTRL2               (0x13 << 2)
#define MIPI_PHY_CLK_LANE_STS           (0x14 << 2)
#define MIPI_PHY_DATA_LANE0_STS	        (0x15 << 2)
#define MIPI_PHY_DATA_LANE1_STS	        (0x16 << 2)
#define MIPI_PHY_DATA_LANE2_STS	        (0x17 << 2)
#define MIPI_PHY_DATA_LANE3_STS	        (0x18 << 2)
#define MIPI_PHY_ESC_CMD                (0x19 << 2)
#define MIPI_PHY_INT_CTRL               (0x1a << 2)
#define MIPI_PHY_INT_STS                (0x1b << 2)

#define MIPI_PHY_MUX_CTRL0              (0xa1 << 2)
#define MIPI_PHY_MUX_CTRL1              (0xa2 << 2)

#if PLATFORM_T7 == 1
#define MIPI_CSI2_PHY0_BASE_ADDR        0xFE3BFC00
#define MIPI_CSI2_PHY1_BASE_ADDR        0xFE3BF800
#define MIPI_CSI2_PHY2_BASE_ADDR        0xFE3BF400
#define MIPI_CSI2_PHY3_BASE_ADDR        0xFE3BF000

#define MIPI_CSI2_HOST0_BASE_ADDR       0xFE3BEC00
#define MIPI_CSI2_HOST1_BASE_ADDR       0xFE3BE800
#define MIPI_CSI2_HOST2_BASE_ADDR       0xFE3BE400
#define MIPI_CSI2_HOST3_BASE_ADDR       0xFE3BE000

#define MIPI_ISP_BASE_ADDR              0xFA000000

// MIPI-CSI2 analog registers
 #define MIPI_CSI_PHY_CNTL0            (0x0000  << 2)
 #define MIPI_CSI_PHY_CNTL1            (0x0001  << 2)
 #define MIPI_CSI_PHY_CNTL2            (0x0002  << 2)
 #define MIPI_CSI_PHY_CNTL3            (0x0003  << 2)
 #define MIPI_CSI_PHY_CNTL4            (0x0004  << 2)
 #define MIPI_CSI_PHY_CNTL5            (0x0005  << 2)
 #define MIPI_CSI_PHY_CNTL6			   (0x0006  << 2)
 #define MIPI_CSI_PHY_CNTL7			   (0x0007  << 2)

// MIPI-CSI2 host registers (internal offset)
#define CSI2_HOST_VERSION              0x000
#define CSI2_HOST_N_LANES              0x004
#define CSI2_HOST_PHY_SHUTDOWNZ        0x008
#define CSI2_HOST_DPHY_RSTZ            0x00c
#define CSI2_HOST_CSI2_RESETN          0x010
#define CSI2_HOST_PHY_STATE            0x014
#define CSI2_HOST_DATA_IDS_1           0x018
#define CSI2_HOST_DATA_IDS_2           0x01c
#define CSI2_HOST_ERR1                 0x020
#define CSI2_HOST_ERR2                 0x024
#define CSI2_HOST_MASK1                0x028
#define CSI2_HOST_MASK2                0x02c
#define CSI2_HOST_PHY_TST_CTRL0        0x030
#define CSI2_HOST_PHY_TST_CTRL1        0x034
#endif

struct am_mipi {
	struct device_node *of_node;
	struct device_node *link_node;
	struct resource csi2_phy0_reg;
	struct resource csi2_phy1_reg;
	struct resource csi2_phy2_reg;
	struct resource csi2_phy3_reg;
	struct resource aphy_reg;
	struct resource csi0_host_reg;
	struct resource csi1_host_reg;
	struct resource csi2_host_reg;
	struct resource csi3_host_reg;
	void __iomem *csi2_phy0;
	void __iomem *csi2_phy1;
	void __iomem *csi2_phy2;
	void __iomem *csi2_phy3;
	void __iomem *aphy;
	void __iomem *csi0_host;
	void __iomem *csi1_host;
	void __iomem *csi2_host;
	void __iomem *csi3_host;
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
uint32_t mipi_host_reg_rd_ext(int addr);
uint32_t mipi_phy_reg_rd_ext(int addr);

int am_mipi1_init(void *info);
void am_mipi1_deinit(void);
uint32_t mipi_csi1_host_reg_rd_ext(int addr);
uint32_t mipi_phy1_reg_rd_ext(int addr);

int am_mipi2_init(void *info);
void am_mipi2_deinit(void);
uint32_t mipi_csi2_host_reg_rd_ext(int addr);
uint32_t mipi_phy2_reg_rd_ext(int addr);

int am_mipi3_init(void *info);
void am_mipi3_deinit(void);
uint32_t mipi_csi3_host_reg_rd_ext(int addr);
uint32_t mipi_phy3_reg_rd_ext(int addr);

#endif


