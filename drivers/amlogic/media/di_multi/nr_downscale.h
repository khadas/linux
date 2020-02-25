/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _NR_DS_H
#define _NR_DS_H

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

void dim_nr_ds_buf_init(unsigned int cma_flag, unsigned long mem_start,
			struct device *dev);
void dim_nr_ds_buf_uninit(unsigned int cma_flag, struct device *dev);
void dim_nr_ds_init(unsigned int width, unsigned int height);
void dim_nr_ds_mif_config(void);
void dim_nr_ds_hw_ctrl(bool enable);
void dim_nr_ds_irq(void);
void dim_get_nr_ds_buf(unsigned long *addr, unsigned long *size);
void dim_dump_nrds_reg(unsigned int base_addr);
#endif
