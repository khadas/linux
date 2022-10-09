/*
*
* SPDX-License-Identifier: GPL-2.0
*
* Copyright (C) 2011-2018 ARM or its affiliates
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

#include <linux/io.h>
#include <linux/delay.h>

#include "acamera_logger.h"
#include "system_apb_dma.h"


static void __iomem *m_base = NULL;

uint32_t dma_read_reg(uint32_t reg)
{
	return readl(m_base + reg);
}


static void dma_write(void __iomem *base, uint32_t reg, uint32_t val)
{
	writel(val, base + reg);
}

static uint32_t dma_read(void __iomem *base, uint32_t reg)
{
	return readl(base + reg);
}

static void dma_reset(void)
{
	uint32_t val = 0;
	void __iomem *base = m_base;

	val = dma_read(base, ISP_DMA_CTL0);
	val |= ((1 << 27) | (1 << 26) | (1 << 25) | (1 << 24));
	dma_write(base, ISP_DMA_CTL0, val);

	udelay(500);

	val = dma_read(base, ISP_DMA_CTL0);
	val &= ~((1 << 27) | (1 << 26) | (1 << 25) | (1 << 24));
	dma_write(base, ISP_DMA_CTL0, val);
}

static void dma_enable(void)
{
	uint32_t val = 0;
	void __iomem *base = m_base;

	val = dma_read(base, ISP_DMA_CTL0);
	val |= (1 << 31);
	dma_write(base, ISP_DMA_CTL0, val);
}

static void dma_disable(void)
{
	uint32_t val = 0;
	void __iomem *base = m_base;

	val = dma_read(base, ISP_DMA_CTL0);
	val &= ~(1 << 31);
	dma_write(base, ISP_DMA_CTL0, val);
}


void dma_cfg_src3_enable(void)
{
	uint32_t val = 0;
	void __iomem *base = m_base;

	val = dma_read(base, ISP_DMA_SRC3_CTL);
	val |= (1 << 2) | (1 << 0);
	dma_write(base, ISP_DMA_SRC3_CTL, val);
}

void dma_cfg_src3_disable(void)
{
	uint32_t val = 0;
	void __iomem *base = m_base;

	val = dma_read(base, ISP_DMA_SRC3_CTL);
	val &= ~(1 << 0);
	dma_write(base, ISP_DMA_SRC3_CTL, val);
}

void dma_cfg_src3_ping(struct apb_dma_cfg *cfg)
{
	uint32_t val = 0;
	void __iomem *base = m_base;

	dma_write(base, ISP_DMA_SRC3_PING_CMD_ADDR0, cfg->cmd_addr);
	dma_write(base, ISP_DMA_SRC3_PING_DST_ADDR0, cfg->dst_addr);

	val |= (cfg->cnt - 1);
	val |= (cfg->dir << 30);
	val |= (1 << 31);
	dma_write(base, ISP_DMA_SRC3_PING_TASK0, val);

	val = dma_read(base, ISP_DMA_SRC3_CTL);
	val &= ~(1 << 3);
	dma_write(base, ISP_DMA_SRC3_CTL, val);
}

void dma_cfg_src3_pong(struct apb_dma_cfg *cfg)
{
	uint32_t val = 0;
	void __iomem *base = m_base;

	dma_write(base, ISP_DMA_SRC3_PONG_CMD_ADDR0, cfg->cmd_addr);
	dma_write(base, ISP_DMA_SRC3_PONG_DST_ADDR0, cfg->dst_addr);

	val |= (cfg->cnt - 1);
	val |= (cfg->dir << 30);
	val |= (1 << 31);
	dma_write(base, ISP_DMA_SRC3_PONG_TASK0, val);

	val = dma_read(base, ISP_DMA_SRC3_CTL);
	val |= (1 << 3);
	dma_write(base, ISP_DMA_SRC3_CTL, val);
}

int dma_src3_get_current_state()
{
	uint32_t val = 0;
	void __iomem *base = m_base;

	val = dma_read(base, ISP_DMA_STAT1);
	return (val & (1 << 3));
}

uint32_t dma_is_working()
{
	uint32_t val = 0;
	void __iomem *base = m_base;

	val = dma_read(base, ISP_DMA_STAT0);
	return (val & (7 << 28));
}

void dma_src3_set_force_pong()
{
	uint32_t val = 0;
	void __iomem *base = m_base;

	val = dma_read(base, ISP_DMA_SRC3_CTL);
	val |= (1 << 3) | (1 << 2);
	dma_write(base, ISP_DMA_SRC3_CTL, val);
}


void dma_cfg_src4_enable(void)
{
	uint32_t val = 0;
	void __iomem *base = m_base;

	val = dma_read(base, ISP_DMA_SRC4_CTL);
	val |= (1 << 2) | (1 << 0);
	dma_write(base, ISP_DMA_SRC4_CTL, val);
}

void dma_cfg_src4_disable(void)
{
	uint32_t val = 0;
	void __iomem *base = m_base;

	val = dma_read(base, ISP_DMA_SRC4_CTL);
	val &= ~(1 << 0);
	dma_write(base, ISP_DMA_SRC4_CTL, val);
}

void dma_cfg_src4_ping(struct apb_dma_cfg *cfg)
{
	uint32_t val = 0;
	void __iomem *base = m_base;

	dma_write(base, ISP_DMA_SRC4_PING_CMD_ADDR0, cfg->cmd_addr);
	dma_write(base, ISP_DMA_SRC4_PING_DST_ADDR0, cfg->dst_addr);

	val |= (cfg->cnt - 1);
	val |= (cfg->dir << 30);
	val |= (1 << 31);
	dma_write(base, ISP_DMA_SRC4_PING_TASK0, val);

	val = dma_read(base, ISP_DMA_SRC4_CTL);
	val &= ~(1 << 3);
	dma_write(base, ISP_DMA_SRC4_CTL, val);
}

void dma_cfg_src4_pong(struct apb_dma_cfg *cfg)
{
	uint32_t val = 0;
	void __iomem *base = m_base;

	dma_write(base, ISP_DMA_SRC4_PONG_CMD_ADDR0, cfg->cmd_addr);
	dma_write(base, ISP_DMA_SRC4_PONG_DST_ADDR0, cfg->dst_addr);

	val |= (cfg->cnt - 1);
	val |= (cfg->dir << 30);
	val |= (1 << 31);
	dma_write(base, ISP_DMA_SRC4_PONG_TASK0, val);

	val = dma_read(base, ISP_DMA_SRC4_CTL);
	val |= (1 << 3);
	dma_write(base, ISP_DMA_SRC4_CTL, val);
}

void dma_cfg_src5_enable(void)
{
	uint32_t val = 0;
	void __iomem *base = m_base;

	val = dma_read(base, ISP_DMA_SRC5_CTL);
	val |= (1 << 2) | (1 << 0);
	dma_write(base, ISP_DMA_SRC5_CTL, val);
}

void dma_cfg_src5_disable(void)
{
	uint32_t val = 0;
	void __iomem *base = m_base;

	val = dma_read(base, ISP_DMA_SRC5_CTL);
	val &= ~(1 << 0);
	dma_write(base, ISP_DMA_SRC5_CTL, val);
}

void dma_cfg_src5_ping(struct apb_dma_cfg *cfg)
{
	uint32_t val = 0;
	void __iomem *base = m_base;

	dma_write(base, ISP_DMA_SRC5_PING_CMD_ADDR0, cfg->cmd_addr);
	dma_write(base, ISP_DMA_SRC5_PING_DST_ADDR0, cfg->dst_addr);

	val |= (cfg->cnt - 1);
	val |= (cfg->dir << 30);
	val |= (1 << 31);
	dma_write(base, ISP_DMA_SRC5_PING_TASK0, val);

	val = dma_read(base, ISP_DMA_SRC5_CTL);
	val &= ~(1 << 3);
	dma_write(base, ISP_DMA_SRC5_CTL, val);
}

void dma_cfg_src5_pong(struct apb_dma_cfg *cfg)
{
	uint32_t val = 0;
	void __iomem *base = m_base;

	dma_write(base, ISP_DMA_SRC5_PONG_CMD_ADDR0, cfg->cmd_addr);
	dma_write(base, ISP_DMA_SRC5_PONG_DST_ADDR0, cfg->dst_addr);

	val |= (cfg->cnt - 1);
	val |= (cfg->dir << 30);
	val |= (1 << 31);
	dma_write(base, ISP_DMA_SRC5_PONG_TASK0, val);

	val = dma_read(base, ISP_DMA_SRC5_CTL);
	val |= (1 << 3);
	dma_write(base, ISP_DMA_SRC5_CTL, val);
}

void dma_cfg_src6_enable(void)
{
	uint32_t val = 0;
	void __iomem *base = m_base;

	val = dma_read(base, ISP_DMA_SRC6_CTL);
	val |= (1 << 2) | (1 << 0);
	dma_write(base, ISP_DMA_SRC6_CTL, val);

	LOG(LOG_CRIT, "src6 enable");
}

void dma_cfg_src6_disable(void)
{
	uint32_t val = 0;
	void __iomem *base = m_base;

	val = dma_read(base, ISP_DMA_SRC6_CTL);
	val &= ~(1 << 0);
	dma_write(base, ISP_DMA_SRC6_CTL, val);

	LOG(LOG_CRIT, "src6 disable");
}

void dma_cfg_src6_ping(struct apb_dma_cfg *cfg)
{
	uint32_t val = 0;
	void __iomem *base = m_base;

	dma_write(base, ISP_DMA_SRC6_PING_CMD_ADDR0, cfg->cmd_addr);
	dma_write(base, ISP_DMA_SRC6_PING_DST_ADDR0, cfg->dst_addr);

	val |= (cfg->cnt - 1);
	val |= (cfg->dir << 30);
	val |= (1 << 31);
	dma_write(base, ISP_DMA_SRC6_PING_TASK0, val);

	val = dma_read(base, ISP_DMA_SRC6_CTL);
	val &= ~(1 << 3);
	dma_write(base, ISP_DMA_SRC6_CTL, val);
}

void dma_cfg_src6_pong(struct apb_dma_cfg *cfg)
{
	uint32_t val = 0;
	void __iomem *base = m_base;

	dma_write(base, ISP_DMA_SRC6_PONG_CMD_ADDR0, cfg->cmd_addr);
	dma_write(base, ISP_DMA_SRC6_PONG_DST_ADDR0, cfg->dst_addr);

	val |= (cfg->cnt - 1);
	val |= (cfg->dir << 30);
	val |= (1 << 31);
	dma_write(base, ISP_DMA_SRC6_PONG_TASK0, val);

	val = dma_read(base, ISP_DMA_SRC6_CTL);
	val |= (1 << 3);
	dma_write(base, ISP_DMA_SRC6_CTL, val);
}

int apb_dma_init(void)
{
	void __iomem *base = NULL;

	base = ioremap_nocache(APB_DMA_BASE_ADDR, 1024);
	if (!base) {
		LOG(LOG_ERR, "Error to ioremap");
		return -1;
	}

	m_base = base;

	dma_reset();

	dma_write(base, ISP_DMA_AXI_CTL, 0xf);

	dma_enable();

	return 0;
}

void apb_dma_deinit(void)
{
	void __iomem *base = m_base;

	dma_disable();

	if (base)
		iounmap(base);

	m_base = NULL;
}
