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
	PRE_VSYNC_RDMA = 3,
	EX_VSYNC_RDMA = 4,
	LINE_N_INT_RDMA = 1,
	VSYNC_RDMA_READ = 2, /* for read */
};

extern int has_multi_vpp;
void vpp1_vsync_rdma_register(void);
void vpp2_vsync_rdma_register(void);
void pre_vsync_rdma_register(void);
int vsync_rdma_config(void);
void vsync_rdma_config_pre(void);
int vsync_rdma_vpp1_config(void);
void vsync_rdma_vpp1_config_pre(void);
int vsync_rdma_vpp2_config(void);
void vsync_rdma_vpp2_config_pre(void);
int pre_vsync_rdma_config(void);
void pre_vsync_rdma_config_pre(void);
bool is_vsync_rdma_enable(void);
bool is_vsync_vpp1_rdma_enable(void);
bool is_vsync_vpp2_rdma_enable(void);
bool is_pre_vsync_rdma_enable(void);
void start_rdma(void);
void enable_rdma_log(int flag);
void enable_rdma(int enable_flag);
int rdma_watchdog_setting(int flag);
int rdma_init2(void);
struct rdma_op_s *get_rdma_ops(int rdma_type);
void set_rdma_handle(int rdma_type, int handle);
int get_rdma_handle(int rdma_type);
int set_vsync_rdma_id(u8 id);
int rdma_init(void);
void rdma_exit(void);
int get_ex_vsync_rdma_enable(void);
void set_ex_vsync_rdma_enable(int enable);
void set_force_rdma_config(void);
int is_in_vsync_isr(void);
#ifdef CONFIG_AMLOGIC_BL_LDIM
int is_in_ldim_vsync_isr(void);
#endif
extern int vsync_rdma_handle[5];
#endif
