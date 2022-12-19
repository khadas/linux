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

#ifndef __AML_CSIPHY_HW_H__
#define __AML_CSIPHY_HW_H__

#ifdef T7C_CHIP
#define MPPI_DPHY_BASE              0x1000
#define MPPI_HOST_BASE              0x0
#define MPPI_PROC_BASE              (0xc0 << 2)

#define MIPI_CSI_2M_PHY2_CNTL1      (0 << 2)
#define MIPI_CSI_2M_PHY2_CNTL2      (1 << 2)
#define MIPI_CSI_2M_PHY2_CNTL3      (2 << 2)

#define MIPI_CSI_2M_PHY2_CNTL4      (4 << 2)
#define MIPI_CSI_2M_PHY2_CNTL5      (5 << 2)
#define MIPI_CSI_2M_PHY2_CNTL6      (6 << 2)
#else
#define MIPI_BASE_ADDR              0xff918000
#define MIPI_DPHY_ADDR              0xff919000
#define MPPI_DPHY_BASE              0x0000
#define MPPI_HOST_BASE              0x1000
#define MPPI_PROC_BASE              0x0

#define MIPI_ISP_TOP_CTRL0          (0x0000 << 2)
#define MIPI_ISP_TOP_CTRL1          (0x0001 << 2)
#define MIPI_CSI_2M_PHY2_CNTL0      (0x0010 << 2)
#define MIPI_CSI_2M_PHY2_CNTL1      (0x0011 << 2)
#define MIPI_CSI_2M_PHY2_CNTL2      (0x0012 << 2)
#define MIPI_CSI_2M_PHY2_CNTL3      (0x0013 << 2)
#define MIPI_CSI_5M_PHY0_CNTL0      (0x0030 << 2)
#define MIPI_CSI_5M_PHY0_CNTL1      (0x0031 << 2)
#define MIPI_CSI_5M_PHY1_CNTL0      (0x0040 << 2)
#define MIPI_CSI_5M_PHY1_CNTL1      (0x0041 << 2)
#define MIPI_CSI_TOF_PHY3_CNTL0     (0x0050 << 2)
#define MIPI_CSI_TOF_PHY3_CNTL1     (0x0051 << 2)
#endif

#define MIPI_PHY_CTRL	            0x00
#define MIPI_PHY_CLK_LANE_CTRL	    0x04
#define MIPI_PHY_DATA_LANE_CTRL	    0x08
#define MIPI_PHY_DATA_LANE_CTRL1    0x0C
#define MIPI_PHY_TCLK_MISS	    0x10
#define MIPI_PHY_TCLK_SETTLE	    0x14
#define MIPI_PHY_THS_EXIT	    0x18
#define MIPI_PHY_THS_SKIP	    0x1C
#define MIPI_PHY_THS_SETTLE	    0x20
#define MIPI_PHY_TINIT	            0x24
#define MIPI_PHY_TULPS_C	    0x28
#define MIPI_PHY_TULPS_S	    0x2C
#define MIPI_PHY_TMBIAS	            0x30
#define MIPI_PHY_TLP_EN_W           0x34
#define MIPI_PHY_TLPOK	            0x38
#define MIPI_PHY_TWD_INIT	    0x3C
#define MIPI_PHY_TWD_HS		    0x40
#define MIPI_PHY_AN_CTRL0	    0x44
#define MIPI_PHY_AN_CTRL1	    0x48
#define MIPI_PHY_AN_CTRL2	    0x4C
#define MIPI_PHY_CLK_LANE_STS	    0x50
#define MIPI_PHY_DATA_LANE0_STS	    0x54
#define MIPI_PHY_DATA_LANE1_STS	    0x58
#define MIPI_PHY_DATA_LANE2_STS	    0x5C
#define MIPI_PHY_DATA_LANE3_STS	    0x60
#define MIPI_PHY_INT_STS	    0x6C
//#define MIPI_PHY_MUX_CTRL0          (0x61 << 2)
//#define MIPI_PHY_MUX_CTRL1	    (0x62 << 2)
#define MIPI_PHY_MUX_CTRL0	    (0xa1 << 2)
#define MIPI_PHY_MUX_CTRL1	    (0xa2 << 2)

#define CSI2_HOST_VERSION           0x000
#define CSI2_HOST_N_LANES           0x004
#define CSI2_HOST_PHY_SHUTDOWNZ     0x008
#define CSI2_HOST_DPHY_RSTZ         0x00c
#define CSI2_HOST_CSI2_RESETN       0x010
#define CSI2_HOST_PHY_STATE         0x014
#define CSI2_HOST_DATA_IDS_1        0x018
#define CSI2_HOST_DATA_IDS_2        0x01c
#define CSI2_HOST_ERR1              0x020
#define CSI2_HOST_ERR2              0x024
#define CSI2_HOST_MASK1             0x028
#define CSI2_HOST_MASK2             0x02c
#define CSI2_HOST_PHY_TST_CTRL0     0x030
#define CSI2_HOST_PHY_TST_CTRL1     0x034

#define MIPI_PROC_TOP_CTRL0         (0x00<<2)
#define MIPI_PROC_TOP_CTRL1         (0x01<<2)

enum {
	DPHY_MD = 0,
	HOST_MD,
	PROC_MD,
	APHY_MD
};

#endif /* __AML_P1_CSIPHY_HW_H__ */
