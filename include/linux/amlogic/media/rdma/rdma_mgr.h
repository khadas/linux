/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef RDMA_MGR_H_
#define RDMA_MGR_H_

struct rdma_op_s {
	void (*irq_cb)(void *arg);
	void *arg;
};

#define RDMA_TRIGGER_VSYNC_INPUT 0x1
#define RDMA_TRIGGER_LINE_INPUT  BIT(8)
#define RDMA_TRIGGER_VPP1_VSYNC_INPUT BIT(9)
#define RDMA_TRIGGER_VPP2_VSYNC_INPUT BIT(19)
#define RDMA_TRIGGER_PRE_VSYNC_INPUT  BIT(24)
#define RDMA_TRIGGER_MANUAL      BIT(28)
#define RDMA_TRIGGER_DEBUG1      BIT(29)
#define RDMA_TRIGGER_DEBUG2      BIT(30)
#define RDMA_AUTO_START_MASK     0x80000000
#define RDMA_TRIGGER_OMIT_LOCK   0x100000

/* rdma write: bit[30] = 0
 * rdma read:  bit[30] = 1
 */
#define RDMA_READ_MASK 0x40000000

enum rdma_ver_e {
	RDMA_VER_1,
	RDMA_VER_2,
	RDMA_VER_3,
	RDMA_VER_4,
};

enum cpu_ver_e {
	CPU_NORMAL,
	CPU_G12B_REVA,
	CPU_G12B_REVB,
	CPU_TL1,
	CPU_SC2,
	CPU_T7,
};

struct rdma_device_data_s {
	enum cpu_ver_e cpu_type;
	enum rdma_ver_e rdma_ver;
	u32 trigger_mask_len;
};

u32 is_meson_g12b_revb(void);
/*
 *	rdma_read_reg(), rdma_write_reg(), rdma_clear() can only be called
 *	after rdma_register() is called and
 *	before rdma_unregister() is called
 */
int rdma_register(struct rdma_op_s *rdma_op, void *op_arg, int table_size);

/*
 *	if keep_buf is 0, rdma_unregister can only be called in its irq_cb.
 *	in normal case, keep_buf is 1, so rdma_unregister can be called anywhere
 */
void rdma_unregister(int i);

int rdma_config(int handle, u32 trigger_type);

u32 rdma_read_reg(int handle, u32 adr);

int rdma_write_reg(int handle, u32 adr, u32 val);

int rdma_write_reg_bits(int handle, u32 adr, u32 val, u32 start, u32 len);

int rdma_clear(int handle);

s32 rdma_add_read_reg(int handle, u32 adr);

u32 *rdma_get_read_back_addr(int handle);

int rdma_buffer_unlock(int handle);
int rdma_buffer_lock(int handle);
#endif
