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

#define pr_fmt(fmt) "AM_MIPI: " fmt


#include "system_am_mipi.h"
#include "system_am_adap.h"
#include <linux/of_address.h>
#include <linux/of_irq.h>


#define AM_MIPI_NAME "amlogic, phy-csi"

extern struct am_mipi *g_mipi;

/*
 *    =======================MIPI PHY INTERFACE====================
*/
static inline void mipi_phy2_reg_wr(int addr, uint32_t val)
{
    void __iomem *reg_addr = g_mipi->csi2_phy2 + addr;

    __raw_writel(val, reg_addr);
}

static inline void mipi_phy2_reg_rd(int addr, uint32_t *val)
{
    uint32_t data = 0;
    void __iomem *reg_addr = g_mipi->csi2_phy2 + addr;

    data = __raw_readl(reg_addr);

    *val = data;
}

uint32_t mipi_phy2_reg_rd_ext(int addr)
{
    uint32_t data = 0;
    void __iomem *reg_addr = g_mipi->csi2_phy2 + addr;

    data = __raw_readl(reg_addr);

    return data;
}

static inline void mipi_aphy_reg_wr(int addr, uint32_t val)
{
    void __iomem *reg_addr = g_mipi->aphy + addr;

    __raw_writel(val, reg_addr);
}

/*
 *    =======================MIPI HOST INTERFACE====================
 */
static inline void mipi_host2_reg_wr(int addr, uint32_t val)
{
    void __iomem *reg_addr = g_mipi->csi2_host + addr;

    __raw_writel(val, reg_addr);
}

static inline void mipi_host2_reg_rd(int addr, uint32_t *val)
{
    uint32_t data = 0;
    void __iomem *reg_addr = g_mipi->csi2_host + addr;

    data = __raw_readl(reg_addr);

    *val = data;
}

uint32_t mipi_csi2_host_reg_rd_ext(int addr)
{
    uint32_t data = 0;
    void __iomem *reg_addr = g_mipi->csi2_host + addr;

    data = __raw_readl(reg_addr);

    return data;
}

static int am_mipi_aphy1_dphy2_init(void *info)
{
    uint32_t cycle_time = 5;//5 ns
    uint32_t settle = 0;
    struct am_mipi_info *m_info = NULL;

    if (info == NULL) {
        pr_err("%s:Error input param\n", __func__);
        return -1;
    }

    m_info = info;

    settle = (85 + 145 + (16 * m_info->ui_val))/2;
    settle = settle/cycle_time;

    //aphy
    ///mipi_aphy_reg_wr(MIPI_CSI_PHY_CNTL0, 0x3f425c00);
    //mipi_aphy_reg_wr(MIPI_CSI_PHY_CNTL1, 0x333a0000);
    mipi_aphy_reg_wr(MIPI_CSI_PHY_CNTL4, 0x3f425c00);
    if (m_info->lanes == 4)
        mipi_aphy_reg_wr(MIPI_CSI_PHY_CNTL5, 0x33a0000);
    else
        mipi_aphy_reg_wr(MIPI_CSI_PHY_CNTL5, 0x333a0000);

    //mipi_aphy_reg_wr(MIPI_CSI_PHY_CNTL2, 0x3800000);
    if (m_info->lanes == 2)
        mipi_aphy_reg_wr(MIPI_CSI_PHY_CNTL6, 0x3800000);
    //mipi_aphy_reg_wr(MIPI_CSI_PHY_CNTL3, 0x301);
    pr_err("init aphy 1 success.");

    //phy2
    mipi_phy2_reg_wr(MIPI_PHY_CLK_LANE_CTRL ,0x3d8);//0x58
    mipi_phy2_reg_wr(MIPI_PHY_TCLK_MISS ,0x9);
    mipi_phy2_reg_wr(MIPI_PHY_TCLK_SETTLE, 0x1f);
    mipi_phy2_reg_wr(MIPI_PHY_THS_EXIT ,0x08);   // hs exit = 160 ns --(x>100ns)
    mipi_phy2_reg_wr(MIPI_PHY_THS_SKIP ,0xa);   // hs skip = 55 ns --(40ns<x<55ns+4*UI)
    mipi_phy2_reg_wr(MIPI_PHY_THS_SETTLE ,settle);   //85ns ~145ns. 8
    mipi_phy2_reg_wr(MIPI_PHY_TINIT ,0x4e20);  // >100us
    mipi_phy2_reg_wr(MIPI_PHY_TMBIAS ,0x100);
    mipi_phy2_reg_wr(MIPI_PHY_TULPS_C ,0x1000);
    mipi_phy2_reg_wr(MIPI_PHY_TULPS_S ,0x100);
    mipi_phy2_reg_wr(MIPI_PHY_TLP_EN_W ,0x0c);
    mipi_phy2_reg_wr(MIPI_PHY_TLPOK ,0x100);
    mipi_phy2_reg_wr(MIPI_PHY_TWD_INIT ,0x400000);
    mipi_phy2_reg_wr(MIPI_PHY_TWD_HS ,0x400000);
    mipi_phy2_reg_wr(MIPI_PHY_DATA_LANE_CTRL , 0x0);
    mipi_phy2_reg_wr(MIPI_PHY_DATA_LANE_CTRL1 , 0x3 | (0x1f << 2 ) | (0x3 << 7));      // enable data lanes pipe line and hs sync bit err.
    if (m_info->lanes == 2) {
        mipi_phy2_reg_wr(MIPI_PHY_MUX_CTRL0 , 0x000001ff);
        mipi_phy2_reg_wr(MIPI_PHY_MUX_CTRL1 , 0x000001ff);
    } else {
        mipi_phy2_reg_wr(MIPI_PHY_MUX_CTRL0 , 0x00000123);    //config input mux
        mipi_phy2_reg_wr(MIPI_PHY_MUX_CTRL1 , 0x00000123);
    }

    mipi_phy2_reg_wr(MIPI_PHY_CTRL, 0);          //  (0 << 9) | (((~chan) & 0xf ) << 5) | 0 << 4 | ((~chan) & 0xf) );

    pr_err("init dphy2 success.");
    return 0;
}

static void am_mipi_phy2_reset(void)
{
    uint32_t data32;
    data32 = 0x1f; //disable lanes digital clock
    data32 |= 0x1 << 31; //soft reset bit
    mipi_phy2_reg_wr(MIPI_PHY_CTRL, data32);
}

static int am_mipi_host2_init(void *info)
{
    struct am_mipi_info *m_info = NULL;

    if (info == NULL) {
        pr_err("%s:Error input param\n", __func__);
        return -1;
    }

    m_info = info;

    mipi_host2_reg_rd(CSI2_HOST_VERSION, &m_info->csi_version);
    pr_info("%s:csi host version 0x%x\n", __func__, m_info->csi_version);

    mipi_host2_reg_wr(CSI2_HOST_CSI2_RESETN, 0); // csi2 reset
    mipi_host2_reg_wr(CSI2_HOST_CSI2_RESETN, 0xffffffff); // release csi2 reset
    mipi_host2_reg_wr(CSI2_HOST_DPHY_RSTZ, 0xffffffff); // release DPHY reset
    mipi_host2_reg_wr(CSI2_HOST_N_LANES, (m_info->lanes - 1) & 3);  //set lanes
    mipi_host2_reg_wr(CSI2_HOST_PHY_SHUTDOWNZ, 0xffffffff); // enable power

    return 0;
}

static void am_mipi_host2_reset(void)
{
    mipi_host2_reg_wr(CSI2_HOST_PHY_SHUTDOWNZ, 0); // enable power
    mipi_host2_reg_wr(CSI2_HOST_DPHY_RSTZ, 0); // release DPHY reset
    mipi_host2_reg_wr(CSI2_HOST_CSI2_RESETN, 0); // csi2 reset
}

/*
 *    =======================MIPI MODULE INTERFACE====================
 */
int am_mipi2_init(void *info)
{
    int rtn = -1;

    if (info == NULL) {
        pr_err("%s:Error input param\n", __func__);
        return -1;
    }

    am_mipi2_deinit();

    rtn = am_mipi_aphy1_dphy2_init(info);
    if (rtn != 0) {
        pr_err("%s:Error mipi phy init\n", __func__);
        return -1;
    }

    rtn = am_mipi_host2_init(info);
    if (rtn != 0) {
        pr_err("%s:Error mipi host init\n", __func__);
        return -1;
    }

    pr_info("%s:Success mipi init\n", __func__);

    return 0;
}

void am_mipi2_deinit(void)
{
    am_mipi_phy2_reset();
    am_mipi_host2_reset();

    pr_info("%s:Success mipi deinit\n", __func__);
}

//-----------------------------------------------------------------------------
static inline void mipi_phy3_reg_wr(int addr, uint32_t val)
{
    void __iomem *reg_addr = g_mipi->csi2_phy3 + addr;

    __raw_writel(val, reg_addr);
}

static inline void mipi_phy3_reg_rd(int addr, uint32_t *val)
{
    uint32_t data = 0;
    void __iomem *reg_addr = g_mipi->csi2_phy3 + addr;

    data = __raw_readl(reg_addr);

    *val = data;
}

uint32_t mipi_phy3_reg_rd_ext(int addr)
{
    uint32_t data = 0;
    void __iomem *reg_addr = g_mipi->csi2_phy3 + addr;

    data = __raw_readl(reg_addr);

    return data;
}

/*
 *    =======================MIPI HOST INTERFACE====================
 */
static inline void mipi_host3_reg_wr(int addr, uint32_t val)
{
    void __iomem *reg_addr = g_mipi->csi3_host + addr;

    __raw_writel(val, reg_addr);
}

static inline void mipi_host3_reg_rd(int addr, uint32_t *val)
{
    uint32_t data = 0;
    void __iomem *reg_addr = g_mipi->csi3_host + addr;

    data = __raw_readl(reg_addr);

    *val = data;
}

uint32_t mipi_csi3_host_reg_rd_ext(int addr)
{
    uint32_t data = 0;
    void __iomem *reg_addr = g_mipi->csi3_host + addr;

    data = __raw_readl(reg_addr);

    return data;
}

static int am_mipi_aphy1_dphy3_init(void *info)
{
    uint32_t cycle_time = 5;//5 ns
    uint32_t settle = 0;
    struct am_mipi_info *m_info = NULL;

    if (info == NULL) {
        pr_err("%s:Error input param\n", __func__);
        return -1;
    }

    m_info = info;

    settle = (85 + 145 + (16 * m_info->ui_val))/2;
    settle = settle/cycle_time;

    //aphy
    mipi_aphy_reg_wr(MIPI_CSI_PHY_CNTL4, 0x3f425c00);
    if (m_info->lanes == 4)
        mipi_aphy_reg_wr(MIPI_CSI_PHY_CNTL5, 0x33a0000);
    else
        mipi_aphy_reg_wr(MIPI_CSI_PHY_CNTL5, 0x333a0000);

    if (m_info->lanes == 2)
        mipi_aphy_reg_wr(MIPI_CSI_PHY_CNTL6, 0x3800000);

    pr_err("init aphy 1 success.");

    //phy3
    mipi_phy3_reg_wr(MIPI_PHY_CLK_LANE_CTRL ,0x3d8);//0x58
    mipi_phy3_reg_wr(MIPI_PHY_TCLK_MISS ,0x9);
    mipi_phy3_reg_wr(MIPI_PHY_TCLK_SETTLE, 0x1f);
    mipi_phy3_reg_wr(MIPI_PHY_THS_EXIT ,0x08);   // hs exit = 160 ns --(x>100ns)
    mipi_phy3_reg_wr(MIPI_PHY_THS_SKIP ,0xa);   // hs skip = 55 ns --(40ns<x<55ns+4*UI)
    mipi_phy3_reg_wr(MIPI_PHY_THS_SETTLE ,settle);   //85ns ~145ns. 8
    mipi_phy3_reg_wr(MIPI_PHY_TINIT ,0x4e20);  // >100us
    mipi_phy3_reg_wr(MIPI_PHY_TMBIAS ,0x100);
    mipi_phy3_reg_wr(MIPI_PHY_TULPS_C ,0x1000);
    mipi_phy3_reg_wr(MIPI_PHY_TULPS_S ,0x100);
    mipi_phy3_reg_wr(MIPI_PHY_TLP_EN_W ,0x0c);
    mipi_phy3_reg_wr(MIPI_PHY_TLPOK ,0x100);
    mipi_phy3_reg_wr(MIPI_PHY_TWD_INIT ,0x400000);
    mipi_phy3_reg_wr(MIPI_PHY_TWD_HS ,0x400000);
    mipi_phy3_reg_wr(MIPI_PHY_DATA_LANE_CTRL , 0x0);
    mipi_phy3_reg_wr(MIPI_PHY_DATA_LANE_CTRL1 , 0x3 | (0x1f << 2 ) | (0x3 << 7));      // enable data lanes pipe line and hs sync bit err.
    if (m_info->lanes == 2) {
        mipi_phy3_reg_wr(MIPI_PHY_MUX_CTRL0 , 0x123ff);    //config input mux
        mipi_phy3_reg_wr(MIPI_PHY_MUX_CTRL1 , 0x1ff01);
    } else {
        mipi_phy3_reg_wr(MIPI_PHY_MUX_CTRL0 , 0x00000123);    //config input mux
        mipi_phy3_reg_wr(MIPI_PHY_MUX_CTRL1 , 0x00000123);
    }

    mipi_phy3_reg_wr(MIPI_PHY_CTRL, 0);          //  (0 << 9) | (((~chan) & 0xf ) << 5) | 0 << 4 | ((~chan) & 0xf) );

    pr_err("init dphy3 success.");
    return 0;
}

static void am_mipi_phy3_reset(void)
{
    uint32_t data32;
    data32 = 0x1f; //disable lanes digital clock
    data32 |= 0x1 << 31; //soft reset bit
    mipi_phy3_reg_wr(MIPI_PHY_CTRL, data32);
}

static int am_mipi_host3_init(void *info)
{
    struct am_mipi_info *m_info = NULL;

    if (info == NULL) {
        pr_err("%s:Error input param\n", __func__);
        return -1;
    }

    m_info = info;

    mipi_host3_reg_rd(CSI2_HOST_VERSION, &m_info->csi_version);
    pr_info("%s:csi host version 0x%x\n", __func__, m_info->csi_version);

    mipi_host3_reg_wr(CSI2_HOST_CSI2_RESETN, 0); // csi2 reset
    mipi_host3_reg_wr(CSI2_HOST_CSI2_RESETN, 0xffffffff); // release csi2 reset
    mipi_host3_reg_wr(CSI2_HOST_DPHY_RSTZ, 0xffffffff); // release DPHY reset
    mipi_host3_reg_wr(CSI2_HOST_N_LANES, (m_info->lanes - 1) & 3);  //set lanes
    mipi_host3_reg_wr(CSI2_HOST_PHY_SHUTDOWNZ, 0xffffffff); // enable power

    return 0;
}

static void am_mipi_host3_reset(void)
{
    mipi_host3_reg_wr(CSI2_HOST_PHY_SHUTDOWNZ, 0); // enable power
    mipi_host3_reg_wr(CSI2_HOST_DPHY_RSTZ, 0); // release DPHY reset
    mipi_host3_reg_wr(CSI2_HOST_CSI2_RESETN, 0); // csi2 reset
}

/*
 *    =======================MIPI MODULE INTERFACE====================
 */
int am_mipi3_init(void *info)
{
    int rtn = -1;

    if (info == NULL) {
        pr_err("%s:Error input param\n", __func__);
        return -1;
    }

    am_mipi3_deinit();

    rtn = am_mipi_aphy1_dphy3_init(info);
    if (rtn != 0) {
        pr_err("%s:Error mipi3 phy init\n", __func__);
        return -1;
    }

    rtn = am_mipi_host3_init(info);
    if (rtn != 0) {
        pr_err("%s:Error mipi3 host init\n", __func__);
        return -1;
    }

    pr_info("%s:Success mipi3 init\n", __func__);

    return 0;
}

void am_mipi3_deinit(void)
{
    am_mipi_phy3_reset();
    am_mipi_host3_reset();

    pr_info("%s:Success mipi3 deinit\n", __func__);
}

