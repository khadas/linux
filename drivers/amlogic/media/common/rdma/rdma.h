/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef RDMA_VSYNC_H_
#define RDMA_VSYNC_H_
enum {
	VSYNC_RDMA,
	LINE_N_INT_RDMA,
};

void vsync_rdma_config(void);
void vsync_rdma_config_pre(void);
bool is_vsync_rdma_enable(void);
void start_rdma(void);
void enable_rdma_log(int flag);
void enable_rdma(int enable_flag);
int rdma_watchdog_setting(int flag);
int rdma_init2(void);
struct rdma_op_s *get_rdma_ops(int rdma_type);
void set_rdma_handle(int rdma_type, int handle);

int rdma_init(void);
void rdma_exit(void);

#endif
