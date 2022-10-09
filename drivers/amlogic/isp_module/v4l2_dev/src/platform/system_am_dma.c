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

#include <asm/types.h>
#include <asm/io.h>
#include <linux/delay.h>
#include "acamera_types.h"
#include "acamera_logger.h"
#include "system_dma.h"
#include "system_stdlib.h"
#include "system_hw_io.h"

#if FW_USE_AML_DMA

#include "system_am_dma.h"

static void *p_hw_dma_base = NULL;
static int m_dma_init = 0;

void dma_write(uint32_t val, unsigned long addr)
{
	iowrite32(val, p_hw_dma_base + addr);
}

uint32_t dma_read(unsigned long addr)
{
	return ioread32( p_hw_dma_base + addr );
}

void am_dma_init(void)
{
    uint32_t val = 0;

    if (m_dma_init ++)
        return;

    p_hw_dma_base = ioremap(ISP_DMA_BASE_ADDR, 0x100);

    val = dma_read(ISP_DMA_BUS_CTL0);
    val = val | (3 << 0);
    dma_write(val, ISP_DMA_BUS_CTL0);

    val = dma_read(ISP_DMA_CTL);
    val = val | (1 << 7) | (1 << 4) | (0 << 0);
    dma_write(val, ISP_DMA_CTL);
}

void am_dma_deinit(void)
{
    m_dma_init = 0;
    iounmap( p_hw_dma_base );
}

void am_dma_cfg_src(uint32_t num, unsigned long src_addr)
{
    if (num > 7 || src_addr == 0) {
        LOG(LOG_ERR, "Error input param");
        return;
    }

    switch (num) {
    case 0:
        dma_write(src_addr, ISP_DMA_SRC_ADDR0);
        break;
    case 1:
        dma_write(src_addr, ISP_DMA_SRC_ADDR1);
        break;
    case 2:
        dma_write(src_addr, ISP_DMA_SRC_ADDR2);
        break;
    case 3:
        dma_write(src_addr, ISP_DMA_SRC_ADDR3);
        break;
    case 4:
        dma_write(src_addr, ISP_DMA_SRC_ADDR4);
        break;
    case 5:
        dma_write(src_addr, ISP_DMA_SRC_ADDR5);
        break;
    case 6:
        dma_write(src_addr, ISP_DMA_SRC_ADDR6);
        break;
    case 7:
        dma_write(src_addr, ISP_DMA_SRC_ADDR7);
        break;
    default:
        LOG(LOG_ERR, "Error dma channel number");
        break;
	}
}

void am_dma_cfg_dst(uint32_t num, unsigned long dst_addr)
{
    if (num > 7 || dst_addr == 0) {
        LOG(LOG_ERR, "Error input param");
        return;
    }

    switch (num) {
    case 0:
        dma_write(dst_addr, ISP_DMA_DST_ADDR0);
        break;
    case 1:
        dma_write(dst_addr, ISP_DMA_DST_ADDR1);
        break;
    case 2:
        dma_write(dst_addr, ISP_DMA_DST_ADDR2);
        break;
    case 3:
        dma_write(dst_addr, ISP_DMA_DST_ADDR3);
        break;
    case 4:
        dma_write(dst_addr, ISP_DMA_DST_ADDR4);
        break;
    case 5:
        dma_write(dst_addr, ISP_DMA_DST_ADDR5);
        break;
    case 6:
        dma_write(dst_addr, ISP_DMA_DST_ADDR6);
        break;
    case 7:
        dma_write(dst_addr, ISP_DMA_DST_ADDR7);
        break;
    default:
        LOG(LOG_ERR, "Error dma channel number");
        break;
    }
}

void am_dma_cfg_dir_cnt(uint32_t num, uint32_t dir, uint32_t cnt)
{
    uint32_t val = 0;

    if (num > 7 || dir > 1 || cnt > 0xffff) {
        LOG(LOG_ERR, "Error input param");
        return;
    }

    val = val | (dir << 31) | (cnt - 1) << 0;

    switch (num) {
    case 0:
        dma_write(val, ISP_DMA_CTL_TASK0);
        break;
    case 1:
        dma_write(val, ISP_DMA_CTL_TASK1);
        break;
    case 2:
        dma_write(val, ISP_DMA_CTL_TASK2);
        break;
    case 3:
        dma_write(val, ISP_DMA_CTL_TASK3);
        break;
    case 4:
        dma_write(val, ISP_DMA_CTL_TASK4);
        break;
    case 5:
        dma_write(val, ISP_DMA_CTL_TASK5);
        break;
    case 6:
        dma_write(val, ISP_DMA_CTL_TASK6);
        break;
    case 7:
        dma_write(val, ISP_DMA_CTL_TASK7);
        break;
    default:
        LOG(LOG_ERR, "Error dma channel number");
        break;
    }
}

void am_dma_start(void)
{
    uint32_t val = 0;

    val = dma_read(ISP_DMA_CTL);
    val = val | (1 << 31);
    dma_write(val, ISP_DMA_CTL);
}

void am_dma_done_check(void)
{
    uint32_t val = 0;

    while (1) {
        val = dma_read(ISP_DMA_PENDING);
        if ( val & (1 << 0) ) {
            break;
        }
        mdelay(1);
    }

    dma_write(val, ISP_DMA_PENDING);
}
#endif
