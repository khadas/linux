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

#ifndef __SYSTEM_APB_DMA_H__
#define __SYSTEM_APB_DMA_H__

#define APB_DMA_BASE_ADDR       0xfe3bbc00

#define ISP_DMA_AXI_CTL 	      (0x00 << 2)
#define ISP_DMA_CTL0 		      (0x01 << 2)
#define ISP_DMA_PENDING0              (0x02 << 2)
#define ISP_DMA_PENDING1              (0x03 << 2)
#define ISP_DMA_IRQ_MASK0             (0x04 << 2)
#define ISP_DMA_IRQ_MASK1             (0x05 << 2)

#define ISP_DMA_STAT0                 (0x10 << 2)
#define ISP_DMA_STAT1                 (0x11 << 2)

#define ISP_DMA_ARBIT_CNTL0           (0x20 << 2)
#define ISP_DMA_ARBIT_CNTL1           (0x21 << 2)
#define ISP_DMA_ARBIT_CNTL2           (0x22 << 2)
#define ISP_DMA_ARBIT_CNTL3           (0x23 << 2)

#define ISP_DMA_SRC3_CTL              (0x57 << 2)
#define ISP_DMA_SRC3_PING_CMD_ADDR0   (0x58 << 2)
#define ISP_DMA_SRC3_PING_DST_ADDR0   (0x59 << 2)
#define ISP_DMA_SRC3_PING_TASK0	      (0x5a << 2)
#define ISP_DMA_SRC3_PING_CMD_ADDR1   (0x5b << 2)
#define ISP_DMA_SRC3_PING_DST_ADDR1   (0x5c << 2)
#define ISP_DMA_SRC3_PING_TASK1	      (0x5d << 2)
#define ISP_DMA_SRC3_PONG_CMD_ADDR0   (0x5e << 2)
#define ISP_DMA_SRC3_PONG_DST_ADDR0   (0x5f << 2)
#define ISP_DMA_SRC3_PONG_TASK0	      (0x60 << 2)
#define ISP_DMA_SRC3_PONG_CMD_ADDR1   (0x61 << 2)
#define ISP_DMA_SRC3_PONG_DST_ADDR1   (0x62 << 2)
#define ISP_DMA_SRC3_PONG_TASK1	      (0x63 << 2)

#define ISP_DMA_SRC4_CTL              (0x64 << 2)
#define ISP_DMA_SRC4_PING_CMD_ADDR0   (0x65 << 2)
#define ISP_DMA_SRC4_PING_DST_ADDR0   (0x66 << 2)
#define ISP_DMA_SRC4_PING_TASK0	      (0x67 << 2)
#define ISP_DMA_SRC4_PING_CMD_ADDR1   (0x68 << 2)
#define ISP_DMA_SRC4_PING_DST_ADDR1   (0x69 << 2)
#define ISP_DMA_SRC4_PING_TASK1	      (0x6a << 2)
#define ISP_DMA_SRC4_PONG_CMD_ADDR0   (0x6b << 2)
#define ISP_DMA_SRC4_PONG_DST_ADDR0   (0x6c << 2)
#define ISP_DMA_SRC4_PONG_TASK0	      (0x6d << 2)
#define ISP_DMA_SRC4_PONG_CMD_ADDR1   (0x6e << 2)
#define ISP_DMA_SRC4_PONG_DST_ADDR1   (0x6f << 2)
#define ISP_DMA_SRC4_PONG_TASK1	      (0x70 << 2)

#define ISP_DMA_SRC5_CTL              (0x71 << 2)
#define ISP_DMA_SRC5_PING_CMD_ADDR0   (0x72 << 2)
#define ISP_DMA_SRC5_PING_DST_ADDR0   (0x73 << 2)
#define ISP_DMA_SRC5_PING_TASK0	      (0x74 << 2)
#define ISP_DMA_SRC5_PING_CMD_ADDR1   (0x75 << 2)
#define ISP_DMA_SRC5_PING_DST_ADDR1   (0x76 << 2)
#define ISP_DMA_SRC5_PING_TASK1	      (0x77 << 2)
#define ISP_DMA_SRC5_PONG_CMD_ADDR0   (0x78 << 2)
#define ISP_DMA_SRC5_PONG_DST_ADDR0   (0x79 << 2)
#define ISP_DMA_SRC5_PONG_TASK0	      (0x7a << 2)
#define ISP_DMA_SRC5_PONG_CMD_ADDR1   (0x7b << 2)
#define ISP_DMA_SRC5_PONG_DST_ADDR1   (0x7c << 2)
#define ISP_DMA_SRC5_PONG_TASK1	      (0x7d << 2)

#define ISP_DMA_SRC6_CTL              (0x7e << 2)
#define ISP_DMA_SRC6_PING_CMD_ADDR0   (0x7f << 2)
#define ISP_DMA_SRC6_PING_DST_ADDR0   (0x80 << 2)
#define ISP_DMA_SRC6_PING_TASK0	      (0x81 << 2)
#define ISP_DMA_SRC6_PING_CMD_ADDR1   (0x82 << 2)
#define ISP_DMA_SRC6_PING_DST_ADDR1   (0x83 << 2)
#define ISP_DMA_SRC6_PING_TASK1	      (0x84 << 2)
#define ISP_DMA_SRC6_PONG_CMD_ADDR0   (0x85 << 2)
#define ISP_DMA_SRC6_PONG_DST_ADDR0   (0x86 << 2)
#define ISP_DMA_SRC6_PONG_TASK0	      (0x87 << 2)
#define ISP_DMA_SRC6_PONG_CMD_ADDR1   (0x88 << 2)
#define ISP_DMA_SRC6_PONG_DST_ADDR1   (0x89 << 2)
#define ISP_DMA_SRC6_PONG_TASK1	      (0x8a << 2)

enum {
	APB_DMA_READ,
	APB_DMA_WRITE,
};

struct apb_dma_cmd {
	uint32_t val;
	uint32_t reg;
};

struct apb_dma_cfg {
	uint32_t cmd_addr;
	uint32_t dst_addr;
	uint32_t dir;
	uint32_t cnt;
};

int apb_dma_init(void);
void apb_dma_deinit(void);
void dma_cfg_src3_enable(void);
void dma_cfg_src3_disable(void);
void dma_cfg_src3_ping(struct apb_dma_cfg *cfg);
void dma_cfg_src3_pong(struct apb_dma_cfg *cfg);

void dma_cfg_src4_enable(void);
void dma_cfg_src4_disable(void);
void dma_cfg_src4_ping(struct apb_dma_cfg *cfg);
void dma_cfg_src4_pong(struct apb_dma_cfg *cfg);

void dma_cfg_src5_enable(void);
void dma_cfg_src5_disable(void);
void dma_cfg_src5_ping(struct apb_dma_cfg *cfg);
void dma_cfg_src5_pong(struct apb_dma_cfg *cfg);

void dma_cfg_src6_enable(void);
void dma_cfg_src6_disable(void);
void dma_cfg_src6_ping(struct apb_dma_cfg *cfg);
void dma_cfg_src6_pong(struct apb_dma_cfg *cfg);
int dma_src3_get_current_state(void);
void dma_src3_set_force_pong(void);
uint32_t dma_is_working(void);
uint32_t dma_read_reg(uint32_t reg);


#endif
