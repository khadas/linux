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

struct am_mipi *g_mipi = NULL;

int am_mipi_parse_dt(struct device_node *node)
{
    int rtn = -1;
    int i = 0;
    int irq = -1;
    struct am_mipi *t_mipi = NULL;
    struct resource rs;

    if (node == NULL) {
        pr_err("%s:Error input param\n", __func__);
        return -1;
    }

    if (g_mipi) {
        pr_err("%s:am mipi ready register\n", __func__);
        return -1;
    }

    rtn = of_device_is_compatible(node, AM_MIPI_NAME);
    if (rtn == 0) {
        pr_err("%s:Error match compatible\n", __func__);
        return -1;
    }

    t_mipi = kzalloc(sizeof(*t_mipi), GFP_KERNEL);
    if (t_mipi == NULL) {
        pr_err("%s:Failed to alloc adapter\n", __func__);
        return -1;
    }

    t_mipi->of_node = node;

    for (i = 0; i < 9; i++) {
        rtn = of_address_to_resource(node, i, &rs);
        if (rtn != 0) {
            pr_err("%s:Error idx %d get mipi reg resource\n", __func__, i);
            continue;
        } else {
            pr_err("%s: rs idx %d info: name: %s\n", __func__, i, rs.name);

            if (strcmp(rs.name, "csi2_phy0") == 0) {
                t_mipi->csi2_phy0_reg = rs;
                t_mipi->csi2_phy0 =
                    ioremap_nocache(t_mipi->csi2_phy0_reg.start, resource_size(&t_mipi->csi2_phy0_reg));
            } else if (strcmp(rs.name, "csi2_phy1") == 0) {
                t_mipi->csi2_phy1_reg = rs;
                t_mipi->csi2_phy1 =
                    ioremap_nocache(t_mipi->csi2_phy1_reg.start, resource_size(&t_mipi->csi2_phy1_reg));
            } else if (strcmp(rs.name, "csi2_phy2") == 0) {
                t_mipi->csi2_phy2_reg = rs;
                t_mipi->csi2_phy2 =
                    ioremap_nocache(t_mipi->csi2_phy2_reg.start, resource_size(&t_mipi->csi2_phy2_reg));
            } else if (strcmp(rs.name, "csi2_phy3") == 0) {
                t_mipi->csi2_phy3_reg = rs;
                t_mipi->csi2_phy3 =
                    ioremap_nocache(t_mipi->csi2_phy3_reg.start, resource_size(&t_mipi->csi2_phy3_reg));
            } else if (strcmp(rs.name, "aphy_reg") == 0){
                t_mipi->aphy_reg = rs;
                t_mipi->aphy =
                    ioremap_nocache(t_mipi->aphy_reg.start, resource_size(&t_mipi->aphy_reg));
            } else if (strcmp(rs.name, "csi0_host") == 0) {
                t_mipi->csi0_host_reg = rs;
                t_mipi->csi0_host =
                    ioremap_nocache(t_mipi->csi0_host_reg.start, resource_size(&t_mipi->csi0_host_reg));
            } else if (strcmp(rs.name, "csi1_host") == 0) {
                t_mipi->csi1_host_reg = rs;
                t_mipi->csi1_host =
                    ioremap_nocache(t_mipi->csi1_host_reg.start, resource_size(&t_mipi->csi1_host_reg));
            } else if (strcmp(rs.name, "csi2_host") == 0) {
                t_mipi->csi2_host_reg = rs;
                t_mipi->csi2_host =
                    ioremap_nocache(t_mipi->csi2_host_reg.start, resource_size(&t_mipi->csi2_host_reg));
            } else if (strcmp(rs.name, "csi3_host") == 0) {
                t_mipi->csi3_host_reg = rs;
                t_mipi->csi3_host =
                    ioremap_nocache(t_mipi->csi3_host_reg.start, resource_size(&t_mipi->csi3_host_reg));
            } else{
                pr_err("%s:Error match address\n", __func__);
            }
        }
    }

    for (i = 0; i < 2; i++) {
        irq = irq_of_parse_and_map(node, i);
        if (irq <= 0) {
            pr_err("%s: Error idx %d get dev irq resource\n", __func__, i);
            continue;
        } else {
            //pr_info("%s: rs idx %d info: irq %d\n", __func__,    i, irq);

            if (i == 0) {
                t_mipi->phy_irq = irq;
            } else if (i == 1) {
                t_mipi->csi_irq = irq;
            } else {
                pr_err("%s:Error match irq\n", __func__);
            }
        }
    }

    rtn = of_property_read_u32(node, "aphy-ctrl3-cfg", &t_mipi->aphy_ctrl3_cfg);
    if (rtn != 0)
#if PLATFORM_C305X
        t_mipi->aphy_ctrl3_cfg = 0x301;
#else
        t_mipi->aphy_ctrl3_cfg = 0x02; /*A: 0x02, B: 0xc002*/
#endif
    rtn = of_property_read_u32(node, "dphy0-ctrl0-cfg", &t_mipi->dphy0_ctrl0_cfg);
    if (rtn != 0)
        t_mipi->dphy0_ctrl0_cfg = 0x123; /*A: 0x123, B: 0x123ff*/

    rtn = of_property_read_u32(node, "dphy0-ctrl1-cfg", &t_mipi->dphy0_ctrl1_cfg);
    if (rtn != 0)
        t_mipi->dphy0_ctrl1_cfg = 0x123; /*A: 0x123, B: 0x1ff01*/

    t_mipi->link_node = of_parse_phandle(node, "link-device", 0);

    if (t_mipi->link_node == NULL) {
        pr_err("%s:Failed to get link device\n", __func__);
    } else {
        pr_err("%s:Success to get link device: %s\n", __func__,
                    t_mipi->link_node->name);
        am_adap_parse_dt(t_mipi->link_node);
    }

    g_mipi = t_mipi;

    return 0;
}

void am_mipi_deinit_parse_dt(void)
{
    struct am_mipi *t_mipi = NULL;

    t_mipi = g_mipi;

    if (t_mipi == NULL) {
        pr_err("Error input param\n");
        return;
    }

    am_adap_deinit_parse_dt();

    if (t_mipi->csi3_host != NULL) {
        iounmap(t_mipi->csi3_host);
        t_mipi->csi3_host = NULL;
    }

    if (t_mipi->csi2_host != NULL) {
        iounmap(t_mipi->csi2_host);
        t_mipi->csi2_host = NULL;
    }

    if (t_mipi->csi1_host != NULL) {
        iounmap(t_mipi->csi1_host);
        t_mipi->csi1_host = NULL;
    }

    if (t_mipi->csi0_host != NULL) {
        iounmap(t_mipi->csi0_host);
        t_mipi->csi0_host = NULL;
    }

    if (t_mipi->aphy != NULL) {
        iounmap(t_mipi->aphy);
        t_mipi->aphy = NULL;
    }

    if (t_mipi->csi2_phy3 != NULL) {
        iounmap(t_mipi->csi2_phy3);
        t_mipi->csi2_phy3 = NULL;
    }

    if (t_mipi->csi2_phy2 != NULL) {
        iounmap(t_mipi->csi2_phy2);
        t_mipi->csi2_phy2 = NULL;
    }

    if (t_mipi->csi2_phy1 != NULL) {
        iounmap(t_mipi->csi2_phy1);
        t_mipi->csi2_phy1 = NULL;
    }

    if (t_mipi->csi2_phy0 != NULL) {
        iounmap(t_mipi->csi2_phy0);
        t_mipi->csi2_phy0 = NULL;
    }

    kfree(t_mipi);
    t_mipi = NULL;
    g_mipi = NULL;

    pr_info("Success deinit parse mipi module\n");
}

/*
 *    =======================MIPI PHY INTERFACE====================
 */
static inline void mipi_phy_reg_wr(int addr, uint32_t val)
{
    void __iomem *reg_addr = g_mipi->csi2_phy0 + addr;

    __raw_writel(val, reg_addr);
}

static inline void mipi_phy_reg_rd(int addr, uint32_t *val)
{
    uint32_t data = 0;
    void __iomem *reg_addr = g_mipi->csi2_phy0 + addr;

    data = __raw_readl(reg_addr);

    *val = data;
}

uint32_t mipi_phy_reg_rd_ext(int addr)
{
    uint32_t data = 0;
    void __iomem *reg_addr = g_mipi->csi2_phy0 + addr;

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
static inline void mipi_host_reg_wr(int addr, uint32_t val)
{
    void __iomem *reg_addr = g_mipi->csi0_host + addr;

    __raw_writel(val, reg_addr);
}

static inline void mipi_host_reg_rd(int addr, uint32_t *val)
{
    uint32_t data = 0;
    void __iomem *reg_addr = g_mipi->csi0_host + addr;

    data = __raw_readl(reg_addr);

    *val = data;
}

uint32_t mipi_host_reg_rd_ext(int addr)
{
    uint32_t data = 0;
    void __iomem *reg_addr = g_mipi->csi0_host + addr;

    data = __raw_readl(reg_addr);

    return data;
}

static int am_mipi_phy_init(void *info)
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
    mipi_aphy_reg_wr(MIPI_CSI_PHY_CNTL0, 0x3f425c00);
    if (m_info->lanes == 4)
        mipi_aphy_reg_wr(MIPI_CSI_PHY_CNTL1, 0x33a0000);
    else
        mipi_aphy_reg_wr(MIPI_CSI_PHY_CNTL1, 0x333a0000);

    if (m_info->lanes == 2)
        mipi_aphy_reg_wr(MIPI_CSI_PHY_CNTL2, 0x3800000);

    pr_err("init aphy success.");

    //phy0
    mipi_phy_reg_wr(MIPI_PHY_CLK_LANE_CTRL ,0x3d8);//0x58
    mipi_phy_reg_wr(MIPI_PHY_TCLK_MISS ,0x9);
    mipi_phy_reg_wr(MIPI_PHY_TCLK_SETTLE, 0x1f);
    mipi_phy_reg_wr(MIPI_PHY_THS_EXIT ,0x08);   // hs exit = 160 ns --(x>100ns)
    mipi_phy_reg_wr(MIPI_PHY_THS_SKIP ,0xa);   // hs skip = 55 ns --(40ns<x<55ns+4*UI)
    mipi_phy_reg_wr(MIPI_PHY_THS_SETTLE ,settle);   //85ns ~145ns.
    mipi_phy_reg_wr(MIPI_PHY_TINIT ,0x4e20);  // >100us
    mipi_phy_reg_wr(MIPI_PHY_TMBIAS ,0x100);
    mipi_phy_reg_wr(MIPI_PHY_TULPS_C ,0x1000);
    mipi_phy_reg_wr(MIPI_PHY_TULPS_S ,0x100);
    mipi_phy_reg_wr(MIPI_PHY_TLP_EN_W ,0x0c);
    mipi_phy_reg_wr(MIPI_PHY_TLPOK ,0x100);
    mipi_phy_reg_wr(MIPI_PHY_TWD_INIT ,0x400000);
    mipi_phy_reg_wr(MIPI_PHY_TWD_HS ,0x400000);
    mipi_phy_reg_wr(MIPI_PHY_DATA_LANE_CTRL , 0x0);
    mipi_phy_reg_wr(MIPI_PHY_DATA_LANE_CTRL1 , 0x3 | (0x1f << 2 ) | (0x3 << 7));      // enable data lanes pipe line and hs sync bit err.
    if (m_info->lanes == 2) {
        mipi_phy_reg_wr(MIPI_PHY_MUX_CTRL0 , 0x000001ff);
        mipi_phy_reg_wr(MIPI_PHY_MUX_CTRL1 , 0x000001ff);
    } else {
        mipi_phy_reg_wr(MIPI_PHY_MUX_CTRL0 , 0x00000123);    //config input mux
        mipi_phy_reg_wr(MIPI_PHY_MUX_CTRL1 , 0x00000123);
    }

    mipi_phy_reg_wr(MIPI_PHY_CTRL, 0);          //  (0 << 9) | (((~chan) & 0xf ) << 5) | 0 << 4 | ((~chan) & 0xf) );

    pr_err("init dphy success.");
    return 0;
}

static void am_mipi_phy_reset(void)
{
    uint32_t data32;
    data32 = 0x1f; //disable lanes digital clock
    data32 |= 0x1 << 31; //soft reset bit
    mipi_phy_reg_wr(MIPI_PHY_CTRL, data32);
}

static int am_mipi_host_init(void *info)
{
    struct am_mipi_info *m_info = NULL;

    if (info == NULL) {
        pr_err("%s:Error input param\n", __func__);
        return -1;
    }

    m_info = info;

    mipi_host_reg_rd(CSI2_HOST_VERSION, &m_info->csi_version);
    pr_info("%s:csi host version 0x%x\n", __func__, m_info->csi_version);

    mipi_host_reg_wr(CSI2_HOST_CSI2_RESETN, 0); // csi2 reset
    mipi_host_reg_wr(CSI2_HOST_CSI2_RESETN, 0xffffffff); // release csi2 reset
    mipi_host_reg_wr(CSI2_HOST_DPHY_RSTZ, 0xffffffff); // release DPHY reset
    mipi_host_reg_wr(CSI2_HOST_N_LANES, (m_info->lanes - 1) & 3);  //set lanes
    mipi_host_reg_wr(CSI2_HOST_PHY_SHUTDOWNZ, 0xffffffff); // enable power

    return 0;
}

static void am_mipi_host_reset(void)
{
    mipi_host_reg_wr(CSI2_HOST_PHY_SHUTDOWNZ, 0); // enable power
    mipi_host_reg_wr(CSI2_HOST_DPHY_RSTZ, 0); // release DPHY reset
    mipi_host_reg_wr(CSI2_HOST_CSI2_RESETN, 0); // csi2 reset
}

/*
 *    =======================MIPI MODULE INTERFACE====================
 */
int am_mipi_init(void *info)
{
    int rtn = -1;

    if (info == NULL) {
        pr_err("%s:Error input param\n", __func__);
        return -1;
    }

    am_mipi_deinit();

    rtn = am_mipi_phy_init(info);
    if (rtn != 0) {
        pr_err("%s:Error mipi phy init\n", __func__);
        return -1;
    }

    rtn = am_mipi_host_init(info);
    if (rtn != 0) {
        pr_err("%s:Error mipi host init\n", __func__);
        return -1;
    }

    pr_info("%s:Success mipi init\n", __func__);

    return 0;
}

void am_mipi_deinit(void)
{
    am_mipi_phy_reset();
    am_mipi_host_reset();

    pr_info("%s:Success mipi deinit\n", __func__);
}

//-----------------------------------------------------------------------------
static inline void mipi_phy1_reg_wr(int addr, uint32_t val)
{
    void __iomem *reg_addr = g_mipi->csi2_phy1 + addr;

    __raw_writel(val, reg_addr);
}

static inline void mipi_phy1_reg_rd(int addr, uint32_t *val)
{
    uint32_t data = 0;
    void __iomem *reg_addr = g_mipi->csi2_phy1 + addr;

    data = __raw_readl(reg_addr);

    *val = data;
}

static inline void mipi_host1_reg_wr(int addr, uint32_t val)
{
    void __iomem *reg_addr = g_mipi->csi1_host + addr;

    __raw_writel(val, reg_addr);
}

static inline void mipi_host1_reg_rd(int addr, uint32_t *val)
{
    uint32_t data = 0;
    void __iomem *reg_addr = g_mipi->csi1_host + addr;

    data = __raw_readl(reg_addr);

    *val = data;
}

static int am_mipi_dphy1_init(void *info)
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
    mipi_aphy_reg_wr(MIPI_CSI_PHY_CNTL0, 0x3f425c00);
    if (m_info->lanes == 4)
        mipi_aphy_reg_wr(MIPI_CSI_PHY_CNTL1, 0x33a0000);
    else
        mipi_aphy_reg_wr(MIPI_CSI_PHY_CNTL1, 0x333a0000);

    if (m_info->lanes == 2)
        mipi_aphy_reg_wr(MIPI_CSI_PHY_CNTL2, 0x3800000);

    pr_err("init aphy 0 success.");

    //phy1
    mipi_phy1_reg_wr(MIPI_PHY_CLK_LANE_CTRL ,0x3d8);//0x58
    mipi_phy1_reg_wr(MIPI_PHY_TCLK_MISS ,0x9);
    mipi_phy1_reg_wr(MIPI_PHY_TCLK_SETTLE, 0x1f);
    mipi_phy1_reg_wr(MIPI_PHY_THS_EXIT ,0x04);    // hs exit = 160 ns --(x>100ns)
    mipi_phy1_reg_wr(MIPI_PHY_THS_SKIP ,0xa);   // hs skip = 55 ns --(40ns<x<55ns+4*UI)
    mipi_phy1_reg_wr(MIPI_PHY_THS_SETTLE ,settle);    //85ns ~145ns.
    mipi_phy1_reg_wr(MIPI_PHY_TINIT ,0x4e20);  // >100us
    mipi_phy1_reg_wr(MIPI_PHY_TMBIAS ,0x100);
    mipi_phy1_reg_wr(MIPI_PHY_TULPS_C ,0x1000);
    mipi_phy1_reg_wr(MIPI_PHY_TULPS_S ,0x100);
    mipi_phy1_reg_wr(MIPI_PHY_TLP_EN_W ,0x0c);
    mipi_phy1_reg_wr(MIPI_PHY_TLPOK ,0x100);
    mipi_phy1_reg_wr(MIPI_PHY_TWD_INIT ,0x400000);
    mipi_phy1_reg_wr(MIPI_PHY_TWD_HS ,0x400000);
    mipi_phy1_reg_wr(MIPI_PHY_DATA_LANE_CTRL , 0x0);
    mipi_phy1_reg_wr(MIPI_PHY_DATA_LANE_CTRL1 , 0x3 | (0x1f << 2 ) | (0x3 << 7));      // enable data lanes pipe line and hs sync bit err.
    if (m_info->lanes == 2) {
        mipi_phy1_reg_wr(MIPI_PHY_MUX_CTRL0 , 0x123ff);      //config input mux
        mipi_phy1_reg_wr(MIPI_PHY_MUX_CTRL1 , 0x1ff01);
    } else {
        mipi_phy1_reg_wr(MIPI_PHY_MUX_CTRL0 , 0x00000123);    //config input mux
        mipi_phy1_reg_wr(MIPI_PHY_MUX_CTRL1 , 0x00000123);
    }

    mipi_phy1_reg_wr(MIPI_PHY_CTRL, 0);          //  (0 << 9) | (((~chan) & 0xf ) << 5) | 0 << 4 | ((~chan) & 0xf) );

    pr_err("init dphy1 success.");
    return 0;
}

static void am_mipi_phy1_reset(void)
{
    uint32_t data32;
    data32 = 0x1f; //disable lanes digital clock
    data32 |= 0x1 << 31; //soft reset bit
    mipi_phy1_reg_wr(MIPI_PHY_CTRL, data32);
}

static int am_mipi_host1_init(void *info)
{
    struct am_mipi_info *m_info = NULL;

    if (info == NULL) {
        pr_err("%s:Error input param\n", __func__);
        return -1;
    }

    m_info = info;

    mipi_host1_reg_rd(CSI2_HOST_VERSION, &m_info->csi_version);
    pr_info("%s:csi host version 0x%x\n", __func__, m_info->csi_version);

    mipi_host1_reg_wr(CSI2_HOST_CSI2_RESETN, 0); // csi2 reset
    mipi_host1_reg_wr(CSI2_HOST_CSI2_RESETN, 0xffffffff); // release csi2 reset
    mipi_host1_reg_wr(CSI2_HOST_DPHY_RSTZ, 0xffffffff); // release DPHY reset
    mipi_host1_reg_wr(CSI2_HOST_N_LANES, (m_info->lanes - 1) & 3);  //set lanes
    mipi_host1_reg_wr(CSI2_HOST_PHY_SHUTDOWNZ, 0xffffffff); // enable power

    return 0;
}

static void am_mipi_host1_reset(void)
{
    mipi_host1_reg_wr(CSI2_HOST_PHY_SHUTDOWNZ, 0); // enable power
    mipi_host1_reg_wr(CSI2_HOST_DPHY_RSTZ, 0); // release DPHY reset
    mipi_host1_reg_wr(CSI2_HOST_CSI2_RESETN, 0); // csi2 reset
}

int am_mipi1_init(void *info)
{
    int rtn = -1;

    if (info == NULL) {
        pr_err("%s:Error input param\n", __func__);
        return -1;
    }

    am_mipi1_deinit();

    rtn = am_mipi_dphy1_init(info);
    if (rtn != 0) {
        pr_err("%s:Error mipi phy init\n", __func__);
        return -1;
    }

    rtn = am_mipi_host1_init(info);
    if (rtn != 0) {
        pr_err("%s:Error mipi host init\n", __func__);
        return -1;
    }

    pr_info("%s:Success mipi init\n", __func__);

    return 0;
}

void am_mipi1_deinit(void)
{
    am_mipi_phy1_reset();
    am_mipi_host1_reset();

    pr_info("%s:Success mipi deinit\n", __func__);
}

