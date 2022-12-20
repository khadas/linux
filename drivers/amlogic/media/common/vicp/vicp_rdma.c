// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifdef CONFIG_AMLOGIC_MEDIA_CODEC_MM
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#endif
#include "vicp_rdma.h"
#include "vicp_reg.h"
#include <linux/amlogic/media/vicp/vicp.h>

struct rdma_buf_type_t rdma_buf[MAX_INPUTSOURCE_COUNT];
u32 rdma_buf_choice;

// --------------------------------------------------------
// vicp_rdma initial function
// --------------------------------------------------------
void vicp_rdma_reset(void)
{
	vicp_reg_set_bits(VID_CMPR_DMA_RESET, 1, 0, 1);
}

void vicp_rdma_init(struct rdma_buf_type_t *rdma_buf)
{
	vicp_print(VICP_RDMA, "%s: cmd_buf: start_addr: 0x%llx, len: %lld.\n",
		__func__, rdma_buf->cmd_buf_start_addr, rdma_buf->cmd_buf_len);
	vicp_print(VICP_RDMA, "%s: load_buf: start_addr: 0x%llx, len: %lld.\n",
		__func__, rdma_buf->load_buf_start_addr, rdma_buf->load_buf_len);
	vicp_reg_set_bits(VID_CMPR_DMA_CBUF_BADDR_LSB,
		((rdma_buf->cmd_buf_start_addr >> 4) >> 0) & 0xffffffff, 0, 32);
	vicp_reg_set_bits(VID_CMPR_DMA_CBUF_BADDR_MSB,
		((rdma_buf->cmd_buf_start_addr >> 4) >> 32) & 0xffffffff, 0, 32);
	vicp_reg_set_bits(VID_CMPR_DMA_CBUF_LENGTH, rdma_buf->cmd_buf_len, 0, 32);
	vicp_reg_set_bits(VID_CMPR_DMA_LBUF_BADDR_LSB,
		((rdma_buf->load_buf_start_addr >> 4) >> 0) & 0xffffffff, 0, 32);
	vicp_reg_set_bits(VID_CMPR_DMA_LBUF_BADDR_MSB,
		((rdma_buf->load_buf_start_addr >> 4) >> 32) & 0xffffffff, 0, 32);
	vicp_reg_set_bits(VID_CMPR_DMA_LBUF_LENGTH, rdma_buf->load_buf_len, 0, 32);
}

void vicp_rdma_trigger(void)
{
	/*0:link mode 1: trigle mode*/
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
void vicp_rdma_buf_init(u32 buf_num, u64 cmd_buf_start_addr, u64 load_buf_start_addr)
{
	if (buf_num >= MAX_INPUTSOURCE_COUNT) {
		vicp_print(VICP_ERROR, "out of max input count.\n");
		return;
	}

	rdma_buf[buf_num].cmd_buf_start_addr = cmd_buf_start_addr;
	rdma_buf[buf_num].cmd_buf_len = 0;
	rdma_buf[buf_num].cmd_buf_end_addr = cmd_buf_start_addr;
	rdma_buf[buf_num].load_buf_start_addr = load_buf_start_addr;
	rdma_buf[buf_num].load_buf_len = 0;
	rdma_buf[buf_num].load_buf_end_addr = load_buf_start_addr;
}

void vicp_rdma_buf_load(u32 count)
{
	int i = 0;

	if (count > MAX_INPUTSOURCE_COUNT) {
		vicp_print(VICP_ERROR, "%s: invalid count.\n", __func__);
		return;
	}
	vicp_print(VICP_RDMA, "%s: count is %d.\n", __func__, count);
	for (i = 0; i < count; i++) {
		if (i == 0)
			vicp_rdma_init(&rdma_buf[0]);

		vicp_rdma_cbuf_ready(i);
	}
}

void set_vicp_rdma_buf_choice(u32 buf_num)
{
	if (buf_num >= MAX_INPUTSOURCE_COUNT) {
		vicp_print(VICP_ERROR, "%s: invalid number.\n", __func__);
		return;
	}
	vicp_print(VICP_RDMA, "%s: cur_buf_num:%d, new_buf_num:%d.\n",
		__func__, rdma_buf_choice, buf_num);
	rdma_buf_choice = buf_num;
}

struct rdma_buf_type_t *get_vicp_rdma_buf_choice(u32 buf_num)
{
	return &rdma_buf[buf_num];
}

struct rdma_buf_type_t *get_current_vicp_rdma_buf(void)
{
	return &rdma_buf[rdma_buf_choice];
}

void vicp_rdma_errorflag_clear(void)
{
	vicp_reg_set_bits(VID_CMPR_DMA_ERR_CLR, 0xff, 0, 8);
	return vicp_reg_set_bits(VID_CMPR_DMA_ERR_CLR, 0, 0, 8);
}

void vicp_rdma_errorflag_parser(void)
{
	u32 reg_val;

	reg_val = vicp_reg_get_bits(VID_CMPR_DMA_ERR_FLAG, 0, 8);
	pr_info("%s: errorflag is %d.\n", __func__, reg_val);

	if (reg_val & (1 << 7))
		pr_info("dma_err_mask.\n");
	else if (reg_val & (1 << 6))
		pr_info("err_cbuf_len.\n");
	else if (reg_val & (1 << 5))
		pr_info("err_lbuf_len.\n");
	else if (reg_val & (1 << 4))
		pr_info("dma_err_end.\n");
	else if (reg_val & (1 << 3))
		pr_info("dma_err_opc.\n");
	else if (reg_val & (1 << 2))
		pr_info("err_jump_cmd.\n");
	else if (reg_val & (1 << 1))
		pr_info("err_wint_sta.\n");
	else if (reg_val & (1 << 0))
		pr_info("no_jump_cmd.\n");
	else
		pr_info("no error.\n");
}

void vicp_rdma_cpsr_dump(void)
{
	int i = 0;

	for (i = 0; i < 0x1ff; i++)
		pr_info("[0x%04x] = 0x%08x\n", i, vicp_reg_read(i));
}

void vicp_rdma_buf_dump(u32 buf_count, u32 buf_num)
{
	int i = 0;
	u64 *data_addr;
	u64 *start_addr;
	u64 command;

	pr_info("cmd_buf[%d] length: %lld.\n", buf_num, rdma_buf[buf_num].cmd_buf_len);
	pr_info("##########cmd buf[%d] data##########\n", buf_num);
	start_addr = (u64 *)codec_mm_vmap(rdma_buf[buf_num].cmd_buf_start_addr, RDMA_CMD_BUF_LEN);
	for (i = 0; i < rdma_buf[buf_num].cmd_buf_len; i++) {
		data_addr = start_addr + i;
		command = (*data_addr >> 60) & 0xf;
		if (command == CMD_JMP) {
			pr_info("CMD_JMP: val:0x%16llx, buf_len:%05lld, start_addr:0x%05llx.\n",
				*data_addr,
				(*data_addr >> 34) & 0xffff,
				(*data_addr >> 0) & 0x3ffffffff);
		} else if (command == CMD_WINT) {
			pr_info("CMD_WINT: val:0x%16llx, interrupt_index:%05lld.\n",
				*data_addr,
				(*data_addr >> 0) & 0xff);
		} else if (command == CMD_CINT) {
			pr_info("CMD_CINT: val:0x%16llx.\n", *data_addr);
		} else if (command == CMD_END) {
			pr_info("CMD_END: val:0x%16llx, len: %05lld, addr:0x%05llx.\n",
				*data_addr,
				(*data_addr >> 34) & 0x3ffffff,
				(*data_addr >> 0) & 0x3ffffffff);
		} else if (command == CMD_NOP) {
			pr_info("CMD_NOP: val:0x%16llx.\n", *data_addr);
		} else if (command == CMD_RD) {
			pr_info("CMD_RD: val:0x%16llx, addr:0x%05llx.\n",
				*data_addr,
				(*data_addr >> 34) & 0xffff);
		} else if (command == CMD_WR) {
			pr_info("CMD_WR: val:0x%16llx, msb:%02lld, lsb:%02lld, reg:0x%04llx, data:0x%09llx.\n",
				*data_addr,
				(*data_addr >> 55) & 0x1f,
				(*data_addr >> 50) & 0x1f,
				(*data_addr >> 34) & 0xffff,
				(*data_addr >> 0) & 0x3ffffffff);
		} else {
			vicp_print(VICP_ERROR, "%s: invalid cmd.\n", __func__);
		}
	}

	if (rdma_buf[buf_num].cmd_buf_len != 0 && (buf_num + 1 < buf_count)) {
		data_addr = start_addr + rdma_buf[buf_num].cmd_buf_len;
		command = (*data_addr >> 60) & 0xf;
		if (command == CMD_JMP) {
			pr_info("CMD_JMP: val:0x%16llx, buf_len:%05lld, start_addr:0x%05llx.\n",
				*data_addr,
				(*data_addr >> 34) & 0xffff,
				(*data_addr >> 0) & 0x3ffffffff);
		} else {
			vicp_print(VICP_ERROR, "%s: invalid cmd.\n", __func__);
		}
	}

	pr_info("load_buf[%d] length: %lld.\n", buf_num, rdma_buf[buf_num].load_buf_len);
	pr_info("##########load buf[%d] data##########\n", buf_num);
	start_addr = (u64 *)codec_mm_vmap(rdma_buf[buf_num].load_buf_start_addr, RDMA_LOAD_BUF_LEN);
	for (i = 0; i < rdma_buf[buf_num].load_buf_len; i++) {
		data_addr = start_addr + i;
		command = (*data_addr >> 60) & 0xf;
		if (command == CMD_JMP) {
			pr_info("CMD_JMP: val:0x%16llx, buf_len:%05lld, start_addr:0x%05llx.\n",
				*data_addr,
				(*data_addr >> 34) & 0xffff,
				(*data_addr >> 0) & 0x3ffffffff);
		} else if (command == CMD_WINT) {
			pr_info("CMD_WINT: val:0x%16llx, interrupt_index:%5lld.\n",
				*data_addr,
				(*data_addr >> 0) & 0xff);
		} else if (command == CMD_CINT) {
			pr_info("CMD_CINT: val:0x%16llx.\n", *data_addr);
		} else if (command == CMD_END) {
			pr_info("CMD_END: val:0x%16llx, len: %5lld, addr:0x%5llx.\n",
				*data_addr,
				(*data_addr >> 34) & 0x3ffffff,
				(*data_addr >> 0) & 0x3ffffffff);
		} else if (command == CMD_NOP) {
			pr_info("CMD_NOP: val:0x%16llx.\n", *data_addr);
		} else if (command == CMD_RD) {
			pr_info("CMD_RD: val:0x%16llx, addr:0x%5llx.\n",
				*data_addr,
				(*data_addr >> 34) & 0xffff);
		} else if (command == CMD_WR) {
			pr_info("CMD_WR: val:0x%16llx, msb:%2lld, lsb:%2lld, reg:0x%4llx, data:0x%9llx.\n",
				*data_addr,
				(*data_addr >> 55) & 0x1f,
				(*data_addr >> 50) & 0x1f,
				(*data_addr >> 34) & 0xffff,
				(*data_addr >> 0) & 0x3ffffffff);
		} else {
			vicp_print(VICP_ERROR, "%s: invalid cmd.\n", __func__);
		}
	}

	if (rdma_buf[buf_num].load_buf_len != 0 && (buf_num + 1 < buf_count)) {
		data_addr = start_addr + rdma_buf[buf_num].load_buf_len;
		command = (*data_addr >> 60) & 0xf;
		if (command == CMD_JMP) {
			pr_info("CMD_JMP: val:0x%16llx, buf_len:%05lld, start_addr:0x%05llx.\n",
				*data_addr,
				(*data_addr >> 34) & 0xffff,
				(*data_addr >> 0) & 0x3ffffffff);
		} else {
			vicp_print(VICP_ERROR, "%s: invalid cmd.\n", __func__);
		}
	}
}

struct rdma_buf_type_t *vicp_rdma_jmp(struct rdma_buf_type_t *last_rdma_buf,
	struct rdma_buf_type_t *rdma_buf, int rdma_lbuf_en)
{
	u64 *data_addr = NULL;
	u64 wdata = 0;

	vicp_print(VICP_RDMA, "%s: last_buf_end:0x%llx, cur_buf_start:0x%llx, cur_buf_len:%lld.\n",
		__func__,
		last_rdma_buf->cmd_buf_end_addr,
		rdma_buf->cmd_buf_start_addr,
		rdma_buf->cmd_buf_len);

	wdata = (CMD_JMP << 60) |
		(rdma_buf->cmd_buf_len << 34) |
		(rdma_buf->cmd_buf_start_addr >> 4);

	data_addr = (u64 *)codec_mm_vmap(last_rdma_buf->cmd_buf_end_addr, 8);
	*data_addr = wdata;
	if (rdma_lbuf_en) {
		wdata = (CMD_JMP << 60) |
			((u64)1 << 54) |
			(rdma_buf->load_buf_len << 34) |
			(rdma_buf->load_buf_start_addr >> 4);
		data_addr = (u64 *)codec_mm_vmap(last_rdma_buf->cmd_buf_end_addr + 8, 8);
		*data_addr = wdata;
	}

	return rdma_buf;
}

struct rdma_buf_type_t *vicp_rdma_end(struct rdma_buf_type_t *rdma_buf)
{
	u64 wdata, nop_num;
	u64 *data_addr;
	int i;

	nop_num = 16 - ((rdma_buf->cmd_buf_end_addr + 8) % 16);
	for (i = 0; i < nop_num; i = i + 8) {
		wdata = CMD_NOP << 60;
		data_addr = (u64 *)codec_mm_vmap(rdma_buf->cmd_buf_end_addr, 8);
		*data_addr = wdata;
		rdma_buf->cmd_buf_end_addr += 8;
		rdma_buf->cmd_buf_len += 1;
	}

	wdata = CMD_END << 60 |
		(rdma_buf->cmd_buf_len + 1) << 34 |
		(rdma_buf->cmd_buf_start_addr >> 4);

	data_addr = (u64 *)codec_mm_vmap(rdma_buf->cmd_buf_end_addr, 8);
	*data_addr = wdata;
	rdma_buf->cmd_buf_end_addr += 8;
	rdma_buf->cmd_buf_len += 1;

	vicp_print(VICP_RDMA, "%s, rdma_buf_len is %lld.\n", __func__, rdma_buf->cmd_buf_len);

	return rdma_buf;
}

struct rdma_buf_type_t *vicp_rdma_wr(struct rdma_buf_type_t *rdma_buf, u64 reg_addr,
	u64 reg_data, u64 start_bits, u64 bits_num)
{
	u64 *data_addr;
	u64 wdata;

	reg_data = (reg_data & (((u64)1 << bits_num) - (u64)1)) << start_bits;
	wdata = CMD_WR << 60 |
		(start_bits + bits_num - 1) << 55 |
		start_bits << 50 |
		reg_addr << 34 |
		reg_data << 0;

	data_addr = (u64 *)codec_mm_vmap(rdma_buf->cmd_buf_end_addr, 8);
	*data_addr = wdata;
	rdma_buf->cmd_buf_end_addr += 8;
	rdma_buf->cmd_buf_len += 1;

	return rdma_buf;
}

struct rdma_buf_type_t *vicp_rdma_rd(struct rdma_buf_type_t *rdma_buf, u64 reg_addr)
{
	u64 *data_addr;
	u64 wdata;

	wdata = (CMD_RD << 60) |
		(reg_addr << 34) |
		(rdma_buf->load_buf_end_addr >> 4);

	data_addr = (u64 *)codec_mm_vmap(rdma_buf->cmd_buf_end_addr, 8);
	*data_addr = wdata;
	rdma_buf->cmd_buf_end_addr += 8;
	rdma_buf->cmd_buf_len += 1;
	rdma_buf->load_buf_end_addr += 8;
	rdma_buf->load_buf_len += 1;

	return rdma_buf;
}

struct rdma_buf_type_t *vicp_rdma_wint(struct rdma_buf_type_t *rdma_buf, u64 int_idx)
{
	u64 *data_addr;
	u64 wdata;

	wdata =  (CMD_WINT << 60 | int_idx);
	data_addr = (u64 *)codec_mm_vmap(rdma_buf->cmd_buf_end_addr, 8);
	*data_addr = wdata;
	rdma_buf->cmd_buf_end_addr += 8;
	rdma_buf->cmd_buf_len += 1;

	return rdma_buf;
}

void  vicp_rdma_release(struct rdma_buf_type_t *rdma_buf0, struct rdma_buf_type_t *rdma_buf1,
			int rdma_lbuf_en, int buf_index)
{
	vicp_print(VICP_RDMA, "%s: buf0:%p, buf1:%p, lbuf_en:%d, buf_index:%d.\n",
		__func__, rdma_buf0, rdma_buf1, rdma_lbuf_en, buf_index);

	if (rdma_buf0 == rdma_buf1)
		vicp_rdma_init(rdma_buf0);
	else
		vicp_rdma_jmp(rdma_buf0, rdma_buf1, rdma_lbuf_en);

	vicp_rdma_cbuf_ready(buf_index);
}
