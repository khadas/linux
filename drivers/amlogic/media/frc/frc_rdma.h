/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */
#include <linux/amlogic/media/frc/frc_reg.h>

#define RDMA_NUM        8
#define REG_TEST_NUM 32

#ifndef PAGE_SIZE
# define PAGE_SIZE 4096
#endif

struct rdma_op_s {
	void (*irq_cb)(void *arg);
	void *arg;
};

/*RDMA total memory size is 1M, phy:0x7cf90000~0x7d090000*/
#define FRC_RDMA_SIZE (256 * (PAGE_SIZE))

struct reg_test {
	u32 addr;
	u32 value;
};

struct frc_rdma_info {
	ulong rdma_table_phy_addr;
	u32 *rdma_table_addr;
	int rdma_table_size;
	u8 buf_status;
	int rdma_item_count;
	int rdma_write_count;
};

struct rdma_instance_s {
	int not_process;
	struct rdma_regadr_s *rdma_regadr;
	struct rdma_op_s *op;
	void *op_arg;
	int rdma_table_size;
	u32 *reg_buf;
	dma_addr_t dma_handle;
	u32 *rdma_table_addr;
	ulong rdma_table_phy_addr;
	int rdma_item_count;
	int rdma_write_count;
	unsigned char keep_buf;
	unsigned char used;
	int prev_trigger_type;
	int prev_read_count;
	int lock_flag;
	int irq_count;
	int rdma_config_count;
	int rdma_empty_config_count;
};

struct rdma_regadr_s {
	u32 rdma_ahb_start_addr;
	u32 rdma_ahb_start_addr_msb;
	u32 rdma_ahb_end_addr;
	u32 rdma_ahb_end_addr_msb;
	u32 trigger_mask_reg;
	u32 trigger_mask_reg_bitpos;
	u32 addr_inc_reg;
	u32 addr_inc_reg_bitpos;
	u32 rw_flag_reg;
	u32 rw_flag_reg_bitpos;
	u32 clear_irq_bitpos;
	u32 irq_status_bitpos;
};

extern int frc_test;

void frc_rdma_alloc_buf(void);
void frc_rdma_release_buf(void);
int frc_rdma_process(u32 val);
irqreturn_t frc_rdma_isr(int irq, void *dev_id);
void frc_rdma_table_config(u32 addr, u32 val);
int frc_rdma_config(int handle, u32 trigger_type);

int frc_rdma_init(void);
struct frc_rdma_info *frc_get_rdma_info(void);
int frc_auto_test(int val, int val2);
void frc_rdma_speed_test(int num);
void frc_rdma_reg_list(void);
int frc_rdma_test_write(u32 handle, u32 addr, u32 val, u32 start, u32 len);
int FRC_RDMA_VSYNC_WR_REG(u32 addr, u32 val);
int FRC_RDMA_VSYNC_WR_BITS(u32 addr, u32 val, u32 start, u32 len);
int FRC_RDMA_VSYNC_REG_UPDATE(u32 addr, u32 val, u32 mask);

int is_rdma_enable(void);
