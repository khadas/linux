/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * drivers/amlogic/media/di_multi/nr_downscale.h
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

#ifndef _NR_DS_H
#define _NR_DS_H

#include "deinterlace.h"

#ifdef DIM_TB_DETECT
#define DIM_TB_BUFFER_MAX_SIZE 16
#define DIM_TB_DETECT_W 128
#define DIM_TB_DETECT_H 96
#define DIM_TB_ALGO_COUNT 6

struct dim_tb_buf_s {
	ulong vaddr;
	ulong paddr;
};

enum dim_tb_status {
	di_tb_idle,
	di_tb_running,
	di_tb_done,
};

void write_file(void);

void dim_tb_status(void);

struct vframe_tb_s {
	enum vframe_source_type_e source_type;
	u32 width;
	u32 height;
	u32 type;
	u32 omx_index;
};

int dim_tb_buffer_init(unsigned int ch);

int dim_tb_buffer_uninit(unsigned int ch);

void dim_tb_reg_init(struct vframe_tb_s *vfm, unsigned int on, unsigned int ch);

int dim_tb_detect(struct vframe_tb_s *vf, int data1, unsigned int ch);

void dim_tb_function(struct vframe_tb_s *vf, int data1, unsigned int ch);

void dim_ds_detect(struct vframe_tb_s *vf, int data1, unsigned int ch);

int dim_tb_task_process(struct vframe_tb_s *vf, u32 data, unsigned int ch);

void dim_tb_ext_cmd(struct vframe_s *vf, int data1, unsigned int ch,
		unsigned int cmd);
#endif
void dim_dump_tb(unsigned int data, unsigned int ch);

#define NR_DS_WIDTH	128
#define NR_DS_HEIGHT	96
#define NR_DS_BUF_SIZE	(96 << 7)
#define NR_DS_BUF_NUM	6
#define NR_DS_MEM_SIZE (NR_DS_BUF_SIZE * NR_DS_BUF_NUM)
#define NR_DS_PAGE_NUM (NR_DS_MEM_SIZE >> PAGE_SHIFT)

struct nr_ds_s {
	unsigned int field_num;
	unsigned long nrds_addr;
	struct page	*nrds_pages;
	unsigned int canvas_idx;
	unsigned char cur_buf_idx;
	unsigned long buf[NR_DS_BUF_NUM];
};

void dim_nr_ds_hw_init(unsigned int width, unsigned int height,
		       unsigned int ch);
void dim_nr_ds_mif_config(void);
void dim_nr_ds_hw_ctrl(bool enable);
void dim_nr_ds_irq(void);
void dim_get_nr_ds_buf(unsigned long *addr, unsigned long *size);
void dim_dump_nrds_reg(unsigned int base_addr);
#endif
