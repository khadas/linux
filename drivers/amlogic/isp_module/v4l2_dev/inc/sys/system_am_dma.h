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

#ifndef __SYSTEM_AM_DMA_H__
#define __SYSTEM_AM_DMA_H__

#define ISP_DMA_BASE_ADDR       0xfe3b3400

#define ISP_DMA_BUS_CTL0        0x00
#define ISP_DMA_CTL             0x04
#define ISP_DMA_SRC_ADDR0       0x08
#define ISP_DMA_DST_ADDR0       0x0c
#define ISP_DMA_CTL_TASK0       0x10
#define ISP_DMA_SRC_ADDR1       0x14
#define ISP_DMA_DST_ADDR1       0x18
#define ISP_DMA_CTL_TASK1       0x1c
#define ISP_DMA_SRC_ADDR2       0x20
#define ISP_DMA_DST_ADDR2       0x24
#define ISP_DMA_CTL_TASK2       0x28
#define ISP_DMA_SRC_ADDR3       0x2c
#define ISP_DMA_DST_ADDR3       0x30
#define ISP_DMA_CTL_TASK3       0x34
#define ISP_DMA_SRC_ADDR4       0x38
#define ISP_DMA_DST_ADDR4       0x3c
#define ISP_DMA_CTL_TASK4       0x40
#define ISP_DMA_SRC_ADDR5       0x44
#define ISP_DMA_DST_ADDR5       0x48
#define ISP_DMA_CTL_TASK5       0x4c
#define ISP_DMA_SRC_ADDR6       0x50
#define ISP_DMA_DST_ADDR6       0x54
#define ISP_DMA_CTL_TASK6       0x58
#define ISP_DMA_SRC_ADDR7       0x5c
#define ISP_DMA_DST_ADDR7       0x60
#define ISP_DMA_CTL_TASK7       0x64

#define ISP_DMA_ST0             0x68
#define ISP_DMA_ST1             0x6c
#define ISP_DMA_ST2             0x70
#define ISP_DMA_ST3             0x74
#define ISP_DMA_ST4             0x78
#define ISP_DMA_ST5             0x7c
#define ISP_DMA_ST6             0x80

#define ISP_DMA_PENDING 	0x8c
#define ISP_DMA_IRQ_MASK 	0x90

extern void am_dma_init(void);
extern void am_dma_deinit(void);
extern void am_dma_cfg_src(uint32_t num, unsigned long src_addr);
extern void am_dma_cfg_dst(uint32_t num, unsigned long dst_addr);
extern void am_dma_cfg_dir_cnt(uint32_t num, uint32_t dir, uint32_t cnt);
extern void am_dma_start(void);
extern void am_dma_done_check(void);

#endif
