/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef RDMA_VSYNC_H_
#define RDMA_VSYNC_H_
enum {
	VSYNC_RDMA = 0,      /* for write */
	VSYNC_RDMA_VPP1 = 1,
	VSYNC_RDMA_VPP2 = 2,
	LINE_N_INT_RDMA = 1,
	VSYNC_RDMA_READ = 2, /* for read */
};

extern int has_multi_vpp;
void vsync_rdma_config(void);
void vsync_rdma_config_pre(void);
void vsync_rdma_vpp1_config(void);
void vsync_rdma_vpp1_config_pre(void);
void vsync_rdma_vpp2_config(void);
void vsync_rdma_vpp2_config_pre(void);
bool is_vsync_rdma_enable(void);
void start_rdma(void);
void enable_rdma_log(int flag);
void enable_rdma(int enable_flag);
int rdma_watchdog_setting(int flag);
int rdma_init2(void);
struct rdma_op_s *get_rdma_ops(int rdma_type);
void set_rdma_handle(int rdma_type, int handle);
int get_rdma_handle(int rdma_type);

int rdma_init(void);
void rdma_exit(void);

#endif
