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

	for (i = 0; i < 5; i++) {
		rtn = of_address_to_resource(node, i, &rs);
		if (rtn != 0) {
			pr_err("%s:Error idx %d get mipi reg resource\n", __func__, i);
			continue;
		} else {
			pr_info("%s: rs idx %d info: name: %s\n", __func__,
				i, rs.name);

			if (strcmp(rs.name, "csi2_phy0") == 0) {
				t_mipi->csi2_phy0_reg = rs;
				t_mipi->csi2_phy0 =
					ioremap_nocache(t_mipi->csi2_phy0_reg.start, resource_size(&t_mipi->csi2_phy0_reg));
			} else if (strcmp(rs.name, "csi2_phy1") == 0) {
				t_mipi->csi2_phy1_reg = rs;
				t_mipi->csi2_phy1 =
					ioremap_nocache(t_mipi->csi2_phy1_reg.start, resource_size(&t_mipi->csi2_phy1_reg));
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
			pr_info("%s: rs idx %d info: irq %d\n", __func__,
				i, irq);

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
		t_mipi->aphy_ctrl3_cfg = 0x02; /*A: 0x02, B: 0xc002*/

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
		pr_info("%s:Success to get link device: %s\n", __func__,
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
 *	=======================MIPI PHY INTERFACE====================
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

static inline void mipi_aphy_reg_wr(int addr, uint32_t val)
{
	void __iomem *reg_addr = g_mipi->aphy + addr;

	__raw_writel(val, reg_addr);
}


static int am_mipi_phy_init(void *info)
{
	uint32_t data32 = 0x80000000;
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

	pr_err("%s:Settle:0x%08x\n", __func__, settle);

	if (m_info->ui_val <= 1)
		mipi_aphy_reg_wr(HI_CSI_PHY_CNTL0, 0x0b440585);
	else
		mipi_aphy_reg_wr(HI_CSI_PHY_CNTL0, 0x0b440581);

	mipi_aphy_reg_wr(HI_CSI_PHY_CNTL1, 0x803f0000);
	mipi_aphy_reg_wr(HI_CSI_PHY_CNTL3, g_mipi->aphy_ctrl3_cfg);

	//mipi_phy_reg_wr(MIPI_PHY_CTRL, data32);   //soft reset bit
	//mipi_phy_reg_wr(MIPI_PHY_CTRL,   0);   //release soft reset bit
	mipi_phy_reg_wr(MIPI_PHY_CLK_LANE_CTRL ,0x3d8); //3d8:continue mode
	mipi_phy_reg_wr(MIPI_PHY_TCLK_MISS ,0x9);  // clck miss = 50 ns --(x< 60 ns)
	mipi_phy_reg_wr(MIPI_PHY_TCLK_SETTLE ,0x1f);  // clck settle = 160 ns --(95ns< x < 300 ns)
	mipi_phy_reg_wr(MIPI_PHY_THS_EXIT ,0x1f);   // hs exit = 160 ns --(x>100ns)
	mipi_phy_reg_wr(MIPI_PHY_THS_SKIP ,0xa);   // hs skip = 55 ns --(40ns<x<55ns+4*UI)
	mipi_phy_reg_wr(MIPI_PHY_THS_SETTLE , settle);//settle);   // hs settle = 160 ns --(85 ns + 6*UI<x<145 ns + 10*UI)
	mipi_phy_reg_wr(MIPI_PHY_TINIT ,0x4e20);  // >100us
	mipi_phy_reg_wr(MIPI_PHY_TMBIAS ,0x100);
	mipi_phy_reg_wr(MIPI_PHY_TULPS_C ,0x1000);
	mipi_phy_reg_wr(MIPI_PHY_TULPS_S ,0x100);
	mipi_phy_reg_wr(MIPI_PHY_TLP_EN_W ,0x0c);
	mipi_phy_reg_wr(MIPI_PHY_TLPOK ,0x100);
	mipi_phy_reg_wr(MIPI_PHY_TWD_INIT ,0x400000);
	mipi_phy_reg_wr(MIPI_PHY_TWD_HS ,0x400000);
	mipi_phy_reg_wr(MIPI_PHY_DATA_LANE_CTRL , 0x0);
	mipi_phy_reg_wr(MIPI_PHY_DATA_LANE_CTRL1 , 0x3 | (0x1f << 2 ) | (0x3 << 7));     // enable data lanes pipe line and hs sync bit err.
	mipi_phy_reg_wr(MIPI_PHY_MUX_CTRL0 ,0x00000123);
	mipi_phy_reg_wr(MIPI_PHY_MUX_CTRL1 ,0x00000123);

	if (m_info->fte1_flag) {
		mipi_phy1_reg_wr(MIPI_PHY_CLK_LANE_CTRL ,0x3d8); //3d8:continue mode
		mipi_phy1_reg_wr(MIPI_PHY_TCLK_MISS ,0x9);  // clck miss = 50 ns --(x< 60 ns)
		mipi_phy1_reg_wr(MIPI_PHY_TCLK_SETTLE ,0x1f);  // clck settle = 160 ns --(95ns< x < 300 ns)
		mipi_phy1_reg_wr(MIPI_PHY_THS_EXIT ,0x1f);   // hs exit = 160 ns --(x>100ns)
		mipi_phy1_reg_wr(MIPI_PHY_THS_SKIP ,0xa);   // hs skip = 55 ns --(40ns<x<55ns+4*UI)
		mipi_phy1_reg_wr(MIPI_PHY_THS_SETTLE , settle);//settle);   // hs settle = 160 ns --(85 ns + 6*UI<x<145 ns + 10*UI)
		mipi_phy1_reg_wr(MIPI_PHY_TINIT ,0x4e20);  // >100us
		mipi_phy1_reg_wr(MIPI_PHY_TMBIAS ,0x100);
		mipi_phy1_reg_wr(MIPI_PHY_TULPS_C ,0x1000);
		mipi_phy1_reg_wr(MIPI_PHY_TULPS_S ,0x100);
		mipi_phy1_reg_wr(MIPI_PHY_TLP_EN_W ,0x0c);
		mipi_phy1_reg_wr(MIPI_PHY_TLPOK ,0x100);
		mipi_phy1_reg_wr(MIPI_PHY_TWD_INIT ,0x400000);
		mipi_phy1_reg_wr(MIPI_PHY_TWD_HS ,0x400000);
		mipi_phy1_reg_wr(MIPI_PHY_DATA_LANE_CTRL , 0x0);
		mipi_phy1_reg_wr(MIPI_PHY_DATA_LANE_CTRL1 , 0x3 | (0x1f << 2 ) | (0x3 << 7));     // enable data lanes pipe line and hs sync bit err.
		mipi_phy1_reg_wr(MIPI_PHY_MUX_CTRL0 ,g_mipi->dphy0_ctrl0_cfg);
		mipi_phy1_reg_wr(MIPI_PHY_MUX_CTRL1 ,g_mipi->dphy0_ctrl1_cfg);
	}

	//mipi_phy_reg_wr(MIPI_PHY_AN_CTRL0,0xa3a9); //MIPI_COMMON<15:0>=<1010,0011,1010,1001>
	//mipi_phy_reg_wr(MIPI_PHY_AN_CTRL1,0xcf25); //MIPI_CHCTL1<15:0>=<1100,1111,0010,0101>
	//mipi_phy_reg_wr(MIPI_PHY_AN_CTRL2,0x0667); //MIPI_CHCTL2<15:0>=<0000,0110,0110,0111>
	data32 = ((~(m_info->channel)) & 0xf) | (0 << 4); //enable lanes digital clock
	data32 |= ((0x10 | m_info->channel) << 5); //mipi_chpu  to analog
	mipi_phy_reg_wr(MIPI_PHY_CTRL, 0);

	if (m_info->fte1_flag) {
		mipi_phy1_reg_wr(MIPI_PHY_CTRL, 0);
	}

	return 0;
}

static void am_mipi_phy_reset(void)
{
	uint32_t data32;
	data32 = 0x1f; //disable lanes digital clock
	data32 |= 0x1 << 31; //soft reset bit
	mipi_phy_reg_wr(MIPI_PHY_CTRL, data32);
	mipi_phy1_reg_wr(MIPI_PHY_CTRL, data32);

}

/*
 *	=======================MIPI CSI INTERFACE====================
 */
static inline void mipi_csi_reg_wr(int addr, uint32_t val)
{
	void __iomem *reg_addr = g_mipi->csi0_host + addr;

	__raw_writel(val, reg_addr);
}

static inline void mipi_csi_reg_rd(int addr, uint32_t *val)
{
	uint32_t data = 0;
	void __iomem *reg_addr = g_mipi->csi0_host + addr;

	data = __raw_readl(reg_addr);

	*val = data;
}

static inline void mipi_csi1_reg_wr(int addr, uint32_t val)
{
	void __iomem *reg_addr = g_mipi->csi1_host + addr;

	__raw_writel(val, reg_addr);
}

static inline void mipi_csi1_reg_rd(int addr, uint32_t *val)
{
	uint32_t data = 0;
	void __iomem *reg_addr = g_mipi->csi1_host + addr;

	data = __raw_readl(reg_addr);

	*val = data;
}

int am_mipi_csi_clk_disable(void)
{
	return 0;
}

static int am_mipi_csi_init(void *info)
{
	struct am_mipi_info *m_info = NULL;

	if (info == NULL) {
		pr_err("%s:Error input param\n", __func__);
		return -1;
	}

	m_info = info;

	mipi_csi_reg_rd(MIPI_CSI_VERSION, &m_info->csi_version);
	pr_info("%s:csi version 0x%x\n", __func__, m_info->csi_version);

	mipi_csi_reg_wr(MIPI_CSI_CSI2_RESETN, 0); // csi2 reset
	mipi_csi_reg_wr(MIPI_CSI_CSI2_RESETN, 0xffffffff); // release csi2 reset
	mipi_csi_reg_wr(MIPI_CSI_DPHY_RSTZ, 0xffffffff); // release DPHY reset
	mipi_csi_reg_wr(MIPI_CSI_N_LANES, (m_info->lanes - 1) & 3);  //set lanes
	mipi_csi_reg_wr(MIPI_CSI_PHY_SHUTDOWNZ, 0xffffffff); // enable power

	if (m_info->fte1_flag) {
		mipi_csi1_reg_wr(MIPI_CSI_CSI2_RESETN, 0); // csi2 reset
		mipi_csi1_reg_wr(MIPI_CSI_CSI2_RESETN, 0xffffffff); // release csi2 reset
		mipi_csi1_reg_wr(MIPI_CSI_DPHY_RSTZ, 0xffffffff); // release DPHY reset
		mipi_csi1_reg_wr(MIPI_CSI_N_LANES, (m_info->lanes - 1) & 3);  //set lanes
		mipi_csi1_reg_wr(MIPI_CSI_PHY_SHUTDOWNZ, 0xffffffff); // enable power
	}

	return 0;
}

static void am_mipi_csi_set_lanes(int lanes)
{
	mipi_csi_reg_wr(MIPI_CSI_N_LANES, (lanes - 1) & 0x3);

	mipi_csi1_reg_wr(MIPI_CSI_N_LANES, (lanes - 1) & 0x3);
}

static void am_mipi_csi_reset(void)
{
	mipi_csi_reg_wr(MIPI_CSI_PHY_SHUTDOWNZ, 0); // enable power
	mipi_csi_reg_wr(MIPI_CSI_DPHY_RSTZ, 0); // release DPHY reset
	mipi_csi_reg_wr(MIPI_CSI_CSI2_RESETN, 0); // csi2 reset

	mipi_csi1_reg_wr(MIPI_CSI_PHY_SHUTDOWNZ, 0); // enable power
	mipi_csi1_reg_wr(MIPI_CSI_DPHY_RSTZ, 0); // release DPHY reset
	mipi_csi1_reg_wr(MIPI_CSI_CSI2_RESETN, 0); // csi2 reset
}

int am_mipi_csi_clk_enable(void)
{
	return 0;
}

/*
 *	=======================MIPI MODULE INTERFACE====================
 */
int am_mipi_init(void *info)
{
	int rtn = -1;

	if (info == NULL) {
		pr_err("%s:Error input param\n", __func__);
		return -1;
	}

	rtn = am_mipi_phy_init(info);
	if (rtn != 0) {
		pr_err("%s:Error mipi phy init\n", __func__);
		return -1;
	}

	rtn = am_mipi_csi_init(info);
	if (rtn != 0) {
		pr_err("%s:Error mipi csi init\n", __func__);
		return -1;
	}

	pr_info("%s:Success mipi init\n", __func__);

	return 0;
}

void am_mipi_set_lanes(int lanes)
{
	am_mipi_csi_set_lanes(lanes);
}

void am_mipi_deinit(void)
{
	am_mipi_phy_reset();
	am_mipi_csi_reset();

	pr_info("%s:Success mipi deinit\n", __func__);
}

