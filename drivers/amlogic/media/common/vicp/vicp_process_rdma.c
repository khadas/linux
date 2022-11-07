// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * drivers/amlogic/media/video_processor/common/vicp/vicp_process_rdma.c
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#include "vicp_process_rdma.h"
#include "vicp_reg.h"
// --------------------------------------------------------
// vicp_rdma initial function
// --------------------------------------------------------

void vicp_rdma_reset(void)
{
	vicp_reg_set_bits(VID_CMPR_DMA_RESET, 1, 0, 1);
}

void vicp_rdma_init(struct rdma_buf_type_t *rdma_buf)
{
	vicp_reg_set_bits(VID_CMPR_DMA_CBUF_BADDR_LSB,
		((rdma_buf->rdma_cbuf_st_address >> 4) >> 0) & 0xffffffff, 0, 32);
	vicp_reg_set_bits(VID_CMPR_DMA_CBUF_BADDR_MSB,
		((rdma_buf->rdma_cbuf_st_address >> 4) >> 32) & 0xffffffff, 0, 32);
	vicp_reg_set_bits(VID_CMPR_DMA_CBUF_LENGTH, rdma_buf->rdma_cbuf_length, 0, 32);
	vicp_reg_set_bits(VID_CMPR_DMA_LBUF_BADDR_LSB,
		((rdma_buf->rdma_lbuf_st_address >> 4) >> 0) & 0xffffffff, 0, 32);
	vicp_reg_set_bits(VID_CMPR_DMA_LBUF_BADDR_MSB,
		((rdma_buf->rdma_lbuf_st_address >> 4) >> 32) & 0xffffffff, 0, 32);
	vicp_reg_set_bits(VID_CMPR_DMA_LBUF_LENGTH, rdma_buf->rdma_lbuf_length, 0, 32);
}

void vicp_rdma_trigger(void)
{
	/*0:link mode 1: trigger mode*/
	vicp_reg_set_bits(VID_CMPR_DMA_MODE, 0, 0, 1);
	vicp_reg_set_bits(VID_CMPR_DMA_START, 1, 0, 1);
}

void vicp_rdma_enable(int rdma_cfg_en, int rdma_lbuf_en, int rdma_test_mode)
{
	vicp_reg_set_bits(VID_CMPR_DMA_ENABLE, rdma_cfg_en, 0, 1);
	vicp_reg_set_bits(VID_CMPR_DMA_LBUF_CTRL, rdma_lbuf_en, 0, 1);
	vicp_reg_set_bits(VID_CMPR_DMA_MODE, 1, 16, 1);/*open selt ro read back*/
	vicp_reg_set_bits(VID_CMPR_DMA_MODE, rdma_test_mode, 12, 1);
}

void vicp_rdma_cbuf_ready(int buf_index)
{
	vicp_reg_set_bits(VID_CMPR_DMA_CBUF_READY, buf_index, 4, 16);
	vicp_reg_set_bits(VID_CMPR_DMA_CBUF_READY, 1, 0, 1);
}

// --------------------------------------------------------
// All command
// --------------------------------------------------------

struct rdma_buf_type_t *vicp_rdma_jmp(struct rdma_buf_type_t *last_rdma_buf,
	struct rdma_buf_type_t *rdma_buf, int rdma_lbuf_en)
{
	u64 wdata = (CMD_JMP << 60) |
		(rdma_buf->rdma_cbuf_length << 34) |
		(rdma_buf->rdma_cbuf_st_address >> 4);
	vicp_reg_write_addr(last_rdma_buf->rdma_cbuf_end_address, wdata);
	if (rdma_lbuf_en) {
		wdata = (CMD_JMP << 60) |
			((u64)1 << 54) |
			(rdma_buf->rdma_lbuf_length << 34) |
			(rdma_buf->rdma_lbuf_st_address >> 4);

		vicp_reg_write_addr(last_rdma_buf->rdma_cbuf_end_address + 8, wdata);
	}

	return rdma_buf;
}

struct rdma_buf_type_t *vicp_rdma_end(struct rdma_buf_type_t *rdma_buf)
{
	u64 wdata, nop_num;
	int i;

	nop_num = 16 - ((rdma_buf->rdma_cbuf_end_address + 8) % 16);
	for (i = 0; i < nop_num; i = i + 8) {
		wdata = CMD_NOP << 60;
		vicp_reg_write_addr(rdma_buf->rdma_cbuf_end_address, wdata);
		rdma_buf->rdma_cbuf_end_address += 8;
		rdma_buf->rdma_cbuf_length += 1;
	}

	wdata = CMD_END << 60 |
		(rdma_buf->rdma_cbuf_length + 1) << 34 |
		(rdma_buf->rdma_cbuf_st_address >> 4);

	vicp_reg_write_addr(rdma_buf->rdma_cbuf_end_address, wdata);
	rdma_buf->rdma_cbuf_end_address += 8;
	rdma_buf->rdma_cbuf_length += 1;

	return rdma_buf;
}

struct rdma_buf_type_t *vicp_rdma_wr(struct rdma_buf_type_t *rdma_buf, u64 reg_addr,
			u64 reg_data, u64 start_bits, u64 bits_num)
{
	reg_addr = reg_addr >= VID_CMPR_APB_BASE ? reg_addr - VID_CMPR_APB_BASE : reg_addr;
	reg_addr = reg_addr >> 2;

	reg_data = (reg_data & (((u64)1 << bits_num) - (u64)1)) << start_bits;
	u64 wdata = CMD_WR << 60 |
		(start_bits + bits_num - 1) << 55 |
		start_bits << 50 |
		reg_addr << 34 |
		reg_data << 0;

	vicp_reg_write_addr(rdma_buf->rdma_cbuf_end_address, wdata);
	rdma_buf->rdma_cbuf_end_address += 8;
	rdma_buf->rdma_cbuf_length += 1;

	return rdma_buf;
}

struct rdma_buf_type_t *vicp_rdma_rd(struct rdma_buf_type_t *rdma_buf, u64 reg_addr)
{
	reg_addr = reg_addr >= VID_CMPR_APB_BASE ? reg_addr - VID_CMPR_APB_BASE : reg_addr;
	reg_addr = reg_addr >> 2;

	u64 wdata = (CMD_RD << 60) | (reg_addr << 34) | (rdma_buf->rdma_lbuf_end_address >> 4);

	vicp_reg_write_addr(rdma_buf->rdma_cbuf_end_address, wdata);
	rdma_buf->rdma_cbuf_end_address += 8;
	rdma_buf->rdma_cbuf_length += 1;
	rdma_buf->rdma_lbuf_end_address += 8;
	rdma_buf->rdma_lbuf_length += 1;

	return rdma_buf;
}

struct rdma_buf_type_t *vicp_rdma_wint(struct rdma_buf_type_t *rdma_buf, u64 int_idx)
{
	u64 wdata =  (CMD_WINT << 60 | int_idx);

	vicp_reg_write_addr(rdma_buf->rdma_cbuf_end_address, wdata);
	rdma_buf->rdma_cbuf_end_address += 8;
	rdma_buf->rdma_cbuf_length += 1;

	return rdma_buf;
}

void  vicp_rdma_release(struct rdma_buf_type_t *rdma_buf0, struct rdma_buf_type_t *rdma_buf1,
			int rdma_lbuf_en, int buf_index)
{
	if (rdma_buf0 == rdma_buf1)
		vicp_rdma_init(rdma_buf0);
	else
		vicp_rdma_jmp(rdma_buf0, rdma_buf1, rdma_lbuf_en);

	vicp_rdma_cbuf_ready(buf_index);
}
