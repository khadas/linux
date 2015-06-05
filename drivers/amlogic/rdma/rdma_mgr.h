#ifndef RDMA_MGR_H_
#define RDMA_MGR_H_

#include "../amports/vdec_reg.h"

struct rdma_op_s {
	void (*irq_cb)(void *arg);
	void *arg;
};

#define RDMA_TRIGGER_MANUAL	0x100
#define RDMA_TRIGGER_DEBUG1 0x101
#define RDMA_TRIGGER_DEBUG2 0x102

int rdma_register(struct rdma_op_s *rdma_op, void *op_arg, int table_size);

int rdma_config(int handle, int trigger_type);

u32 rdma_read_reg(int handle, u32 adr);

int rdma_write_reg(int handle, u32 adr, u32 val);

int rdma_write_reg_bits(int handle, u32 adr, u32 val, u32 start, u32 len);

int rdma_clear(int handle);

#endif

