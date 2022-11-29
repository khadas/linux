/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * drivers/amlogic/media/video_processor/common/vicp/vicp_rdma.h
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

#ifndef _VICP_PROCESS_RDMA_H_
#define _VICP_PROCESS_RDMA_H_

#include <linux/types.h>
#include "vicp_log.h"

#define VID_CMPR_APB_BASE	0xfe03e000

#define CMD_JMP  ((u64)8)
#define CMD_WINT ((u64)9)
#define CMD_CINT ((u64)10)
#define CMD_END  ((u64)11)
#define CMD_NOP  ((u64)12)
#define CMD_RD   ((u64)13)
#define CMD_WR   ((u64)14)

struct rdma_buf_type_t {
	u64 cmd_buf_start_addr;
	u64 cmd_buf_end_addr;
	u64 cmd_buf_len;
	u64 load_buf_start_addr;
	u64 load_buf_end_addr;
	u64 load_buf_len;
};

void vicp_rdma_buf_init(u32 buf_num, u64 cmd_buf_start_addr, u64 load_buf_start_addr);
void vicp_rdma_buf_load(u32 count);
void set_vicp_rdma_buf_choice(u32 buf_num);
struct rdma_buf_type_t *get_vicp_rdma_buf_choice(u32 buf_num);
struct rdma_buf_type_t *get_current_vicp_rdma_buf(void);
void vicp_rdma_errorflag_clear(void);
void vicp_rdma_errorflag_parser(void);
void vicp_rdma_cpsr_dump(void);
void vicp_rdma_buf_dump(u32 buf_count, u32 buf_num);
void vicp_rdma_init(struct rdma_buf_type_t *rdma_buf);
void vicp_rdma_reset(void);
void vicp_rdma_trigger(void);
void vicp_rdma_enable(int rdma_cfg_en, int rdma_lbuf_en, int rdma_test_mode);
void vicp_rdma_cbuf_ready(int buf_index);
struct rdma_buf_type_t *vicp_rdma_jmp(struct rdma_buf_type_t *last_rdma_buf,
	struct rdma_buf_type_t *rdma_buf, int rdma_lbuf_en);
struct rdma_buf_type_t *vicp_rdma_end(struct rdma_buf_type_t *rdma_buf);
struct rdma_buf_type_t *vicp_rdma_wr(struct rdma_buf_type_t *rdma_buf, u64 reg_addr,
	u64 reg_data, u64 start_bits, u64 bits_num);
struct rdma_buf_type_t *vicp_rdma_rd(struct rdma_buf_type_t *rdma_buf, u64 reg_addr);
struct rdma_buf_type_t *vicp_rdma_wint(struct rdma_buf_type_t *rdma_buf, u64 int_idx);
void vicp_rdma_release(struct rdma_buf_type_t *rdma_buf0, struct rdma_buf_type_t *rdma_buf1,
	int rdma_lbuf_en, int buf_index);

#endif //_VICP_RDMA_H_
