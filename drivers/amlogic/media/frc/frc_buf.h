/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */
#ifndef __FRC_BUF_H__
#define __FRC_BUF_H__

#define ALIGN_64	64
#define ALIGN_1k	0x400
#define ALIGN_4K	0x1000

#define FRC_BUF_RELEASE_NUM    12

extern int frc_in_hsize;
extern int frc_in_vsize;
extern int frc_dbg_en;

int frc_buf_alloc(struct frc_dev_s *frc_data);
int frc_buf_release(struct frc_dev_s *devp);
int frc_buf_calculate(struct frc_dev_s *frc_data);
int frc_buf_config(struct frc_dev_s *frc_data);
void frc_dump_memory_info(struct frc_dev_s *devp);
int frc_buf_distribute(struct frc_dev_s *devp);
int frc_buf_distribute_new(struct frc_dev_s *devp);
void frc_buf_dump_memory_addr_info(struct frc_dev_s *devp);
void frc_buf_dump_memory_size_info(struct frc_dev_s *devp);
int frc_buf_mapping_tab_init(struct frc_dev_s *devp);
void frc_buf_dump_link_tab(struct frc_dev_s *devp, u32 mode);
void frc_dump_buf_reg(void);
void frc_mem_dynamic_proc(struct work_struct *work);

#endif
