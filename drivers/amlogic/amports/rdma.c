/*
 * drivers/amlogic/amports/rdma.c
 *
 * Copyright (C) 2015 Amlogic, Inc. All rights reserved.
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

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/dma-mapping.h>

#include <linux/string.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/ctype.h>
#include <linux/amlogic/vout/vout_notify.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/clk.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include "rdma.h"
#include "vdec_reg.h"


#define Wr(adr, val) WRITE_VCBUS_REG(adr, val)
#define Rd(adr)    READ_VCBUS_REG(adr)
#define Wr_reg_bits(adr, val, start, len) \
			WRITE_VCBUS_REG_BITS(adr, val, start, len)


/*#define CONFIG_RDMA_IN_RDMAIRQ*/
/*#define CONFIG_RDMA_IN_TASK*/

#define RDMA_TABLE_SIZE                    (2 * (PAGE_SIZE))
#define RDMA2_TABLE_SIZE                   (PAGE_SIZE >> 3)

static u32 *rmda_table;

static u32 *rmda_table_addr_remap;
static u32 rmda_table_phy_addr;

static int rmda_item_count;


static int irq_count;

static int rdma_done_line_max;

static int enable;

static int enable_mask = 0x400ff;

static int pre_enable_;

static int debug_flag;

static int post_line_start;

static bool rdma_start;
static bool vsync_rdma_config_delay_flag;
/* burst size 0=16; 1=24; 2=32; 3=48.*/
static int ctrl_ahb_rd_burst_size = 3;
static int ctrl_ahb_wr_burst_size = 3;

static int rdma_isr_cfg_count;
static int vsync_cfg_count;
static int rdma_config(unsigned char type);

static struct semaphore  rdma_sema;
struct task_struct *rdma_task = NULL;
static unsigned rdma_config_flag;

static unsigned char rdma_start_flag;


static int rdma_task_handle(void *data)
{
	int ret = 0;
	while (1) {
		ret = down_interruptible(&rdma_sema);
		if (rdma_config_flag == 1) {
			rdma_config_flag = 0;
			rdma_config(1);
		}
		if (rdma_start_flag) {
			rdma_start_flag = 0;
			rdma_init2();
			rdma_start = true;
		}

	}

	return 0;

}


void rdma_table_prepare_write(u32 reg_adr, u32 val)
{
	if (((rmda_item_count << 1) + 1) < (RDMA_TABLE_SIZE / sizeof(u32))) {
		rmda_table[rmda_item_count << 1] = reg_adr;
		rmda_table[(rmda_item_count << 1) + 1] = val;
		rmda_item_count++;
	}	else {
		int i;
		for (i = 0; i < rmda_item_count; i++)
			Wr(rmda_table[i << 1], rmda_table[(i << 1) + 1]);
		rmda_item_count = 0;
		rmda_table[rmda_item_count << 1] = reg_adr;
		rmda_table[(rmda_item_count << 1) + 1] = val;
		rmda_item_count++;
	}
}
EXPORT_SYMBOL(rdma_table_prepare_write);
static int rdma_config(unsigned char type)
{
	u32 data32;
	if (rmda_item_count > 0) {
#if (defined RDMA_CHECK_REG) && (defined RDMA_CHECK_BIT)
		u32 check_val = Rd(RDMA_CHECK_REG);
		rdma_table_prepare_write(RDMA_CHECK_REG,
			check_val | (1 << RDMA_CHECK_BIT));
#endif
		memcpy(rmda_table_addr_remap, rmda_table,
			rmda_item_count * 2 * sizeof(u32));
#ifdef RDMA_CHECK_PRE
		memcpy(rmda_table_pre, rmda_table,
			rmda_item_count * 2 * sizeof(u32));
#endif
	} else {
#ifdef CONFIG_RDMA_IN_TASK
		rdma_config_flag = 2;
#endif
	}
#ifdef RDMA_CHECK_PRE
	rmda_item_count_pre = rmda_item_count;
#endif
	data32  = 0;
	data32 |= 1 << 6;
	data32 |= ctrl_ahb_wr_burst_size << 4;
	data32 |= ctrl_ahb_rd_burst_size << 2;
	data32 |= 0 << 1;
	data32 |= 0 << 0;
	Wr(RDMA_CTRL, data32);

	if (type == 0) { /*manual RDMA*/
		Wr(RDMA_AHB_START_ADDR_MAN, rmda_table_phy_addr);
		Wr(RDMA_AHB_END_ADDR_MAN,   rmda_table_phy_addr
			+ rmda_item_count * 8 - 1);

		data32 = 0;
		data32 |= 0 << 3;
		data32 |= 1 << 2;
		data32 |= 0 << 1;
		data32 |= 0 << 0;
		Wr(RDMA_ACCESS_MAN, data32);
/* Manual-start RDMA*/
		if (rmda_item_count > 0)
			Wr(RDMA_ACCESS_MAN, Rd(RDMA_ACCESS_MAN) | 1);
	} else if (type == 1) { /*vsync trigger RDMA*/
		Wr(RDMA_AHB_START_ADDR_1, rmda_table_phy_addr);
		Wr(RDMA_AHB_END_ADDR_1,
			rmda_table_phy_addr + rmda_item_count * 8 - 1);

		data32 = Rd(RDMA_ACCESS_AUTO);
		if (rmda_item_count > 0) {
			data32 |= 0x1 << 8;
			data32 |= 1 << 5;
			data32 |= 0 << 1;
			Wr(RDMA_ACCESS_AUTO, data32);
		} else {
			data32 &= 0xffffedd;
			Wr(RDMA_ACCESS_AUTO, data32);
		}
	} else if (type == 2) {
		int i;
		for (i = 0; i < rmda_item_count; i++) {
			Wr(rmda_table_addr_remap[i << 1],
				rmda_table_addr_remap[(i << 1) + 1]);
			if (debug_flag & 1)
				pr_info("VSYNC_WR(%x)<=%x\n",
					rmda_table_addr_remap[i << 1],
					rmda_table_addr_remap[(i << 1) + 1]);
		}
	} else if (type == 3) {
		int i;
		for (i = 0; i < rmda_item_count; i++) {
			Wr(rmda_table[i << 1], rmda_table[(i << 1) + 1]);
			if (debug_flag & 1)
				pr_info("VSYNC_WR(%x)<=%x\n",
					rmda_table[i << 1],
					rmda_table[(i << 1) + 1]);
		}
	}
	rmda_item_count = 0;
	return 0;
}

void vsync_rdma_config(void)
{
	u32 data32;
	int enable_ = ((enable & enable_mask) | (enable_mask >> 8)) & 0xff;

	if (pre_enable_ != enable_) {
		if (((enable_mask >> 17) & 0x1) == 0) {
			data32  = Rd(RDMA_ACCESS_AUTO);
			data32 &= 0xffffedd;
			Wr(RDMA_ACCESS_MAN, 0);
			Wr(RDMA_ACCESS_AUTO, data32);
		}
		vsync_rdma_config_delay_flag = false;
	}

	if (enable_ == 1) {
#ifdef CONFIG_RDMA_IN_TASK
		if ((rdma_config_flag == 2) || (pre_enable_ != enable)) {
			rdma_config_flag = 1;
			up(&rdma_sema);
		}

#elif (defined CONFIG_RDMA_IN_RDMAIRQ)
		if (pre_enable_ != enable_)
			rdma_config(1);
#else
		rdma_config(1);
		vsync_cfg_count++;
#endif
	} else if (enable_ == 2)
		rdma_config(0); /*manually in cur vsync*/
	else if (enable_ == 3)
		;
	else if (enable_ == 4)
		rdma_config(2); /*for debug*/
	else if (enable_ == 5)
		rdma_config(3); /*for debug*/
	else if (enable_ == 6)
		;

	pre_enable_ = enable_;
}
EXPORT_SYMBOL(vsync_rdma_config);

void vsync_rdma_config_pre(void)
{
	int enable_ = ((enable&enable_mask)|(enable_mask>>8))&0xff;
	if (enable_ == 3)/*manually in next vsync*/
		rdma_config(0);
	else if (enable_ == 6)
		rdma_config(2);
}
EXPORT_SYMBOL(vsync_rdma_config_pre);

irqreturn_t rdma_isr(int irq, void *dev_id)
{
#if (defined RDMA_CHECK_REG) && (defined RDMA_CHECK_BIT)
	u32 check_val = Rd(RDMA_CHECK_REG);
	if (((check_val >> RDMA_CHECK_BIT) & 0x1) == 0)
		return IRQ_HANDLED;
	Wr(RDMA_CHECK_REG, Rd(RDMA_CHECK_REG) & (~(1 << RDMA_CHECK_BIT)));
#endif

#ifdef CONFIG_RDMA_IN_TASK
	if (rmda_item_count > 0) {
		rdma_config_flag = 1;
		up(&rdma_sema);
	} else
		rdma_config_flag = 2;

#elif (defined CONFIG_RDMA_IN_RDMAIRQ)
	int enable_ = ((enable&enable_mask) | (enable_mask >> 8)) & 0xff;
	if (enable_ == 1)
		rdma_config(1); /*triggered by next vsync*/
	irq_count++;
#else
	int enc_line;
	u32 data32;

	irq_count++;
	if (post_line_start) {
		while (((Rd(ENCL_INFO_READ) >> 16) & 0x1fff) < post_line_start)
			;
	}

	if (vsync_rdma_config_delay_flag) {
		data32  = Rd(RDMA_ACCESS_AUTO);
		data32 &= 0xffffedd;
		Wr(RDMA_ACCESS_MAN, 0);
		Wr(RDMA_ACCESS_AUTO, data32);
		rdma_config(1);
		vsync_rdma_config_delay_flag = false;
	}

	switch (Rd(VPU_VIU_VENC_MUX_CTRL)&0x3) {
	case 0:
		enc_line = (Rd(ENCL_INFO_READ) >> 16) & 0x1fff;
		break;
	case 1:
		enc_line = (Rd(ENCI_INFO_READ) >> 16) & 0x1fff;
		break;
	case 2:
		enc_line = (Rd(ENCP_INFO_READ) >> 16) & 0x1fff;
		break;
	case 3:
		enc_line = (Rd(ENCT_INFO_READ) >> 16) & 0x1fff;
		break;
	}
	if (enc_line > rdma_done_line_max)
		rdma_done_line_max = enc_line;
#endif
	return IRQ_HANDLED;
}

struct device *amports_get_dma_device(void);

int rdma_init2(void)
{
	dma_addr_t dma_handle;
	rmda_table_addr_remap =
		dma_alloc_coherent(amports_get_dma_device(), RDMA_TABLE_SIZE,
		&dma_handle, GFP_KERNEL);
	rmda_table_phy_addr = (u32)(dma_handle);
	pr_info("%s, rmda_table_addr %p rdma_table_addr_phy %x\n",
		__func__, rmda_table_addr_remap, rmda_table_phy_addr);
	rmda_table = kmalloc(RDMA_TABLE_SIZE, GFP_KERNEL);

#if (defined CONFIG_RDMA_IN_RDMAIRQ) || (defined CONFIG_RDMA_IN_TASK)
	if (request_irq(INT_RDMA, &rdma_isr,
		IRQF_SHARED, "rdma",
		(void *)"rdma"))
		return -1;

#endif
	return 0;
}


MODULE_PARM_DESC(enable, "\n enable\n");
module_param(enable, uint, 0664);

MODULE_PARM_DESC(enable_mask, "\n enable_mask\n");
module_param(enable_mask, uint, 0664);

MODULE_PARM_DESC(irq_count, "\n irq_count\n");
module_param(irq_count, uint, 0664);

MODULE_PARM_DESC(post_line_start, "\n post_line_start\n");
module_param(post_line_start, uint, 0664);

MODULE_PARM_DESC(rdma_done_line_max, "\n rdma_done_line_max\n");
module_param(rdma_done_line_max, uint, 0664);

MODULE_PARM_DESC(ctrl_ahb_rd_burst_size, "\n ctrl_ahb_rd_burst_size\n");
module_param(ctrl_ahb_rd_burst_size, uint, 0664);

MODULE_PARM_DESC(ctrl_ahb_wr_burst_size, "\n ctrl_ahb_wr_burst_size\n");
module_param(ctrl_ahb_wr_burst_size, uint, 0664);

MODULE_PARM_DESC(debug_flag, "\n debug_flag\n");
module_param(debug_flag, uint, 0664);

MODULE_PARM_DESC(rdma_isr_cfg_count, "\n rdma_isr_cfg_count\n");
module_param(rdma_isr_cfg_count, uint, 0664);

MODULE_PARM_DESC(vsync_cfg_count, "\n vsync_cfg_count\n");
module_param(vsync_cfg_count, uint, 0664);

u32 VSYNC_RD_MPEG_REG(u32 adr)
{
	int i;
	int enable_ = ((enable&enable_mask) | (enable_mask >> 8)) & 0xff;
	u32 read_val = Rd(adr);
	if ((enable_ != 0) && rdma_start) {
		for (i = (rmda_item_count - 1) ; i >= 0; i--) {
			if (rmda_table[i << 1] == adr) {
				read_val = rmda_table[(i << 1) + 1];
				break;
			}
		}
	}
	return read_val;
}
EXPORT_SYMBOL(VSYNC_RD_MPEG_REG);

int VSYNC_WR_MPEG_REG(u32 adr, u32 val)
{
	int enable_ = ((enable & enable_mask) | (enable_mask >> 8)) & 0xff;
	if ((enable_ != 0) && rdma_start) {
		if (debug_flag & 1)
			pr_info("RDMA_WR %d(%x)<=%x\n",
				rmda_item_count, adr, val);
		rdma_table_prepare_write(adr, val);
	} else {
		Wr(adr, val);
		if (debug_flag & 1)
			pr_info("VSYNC_WR(%x)<=%x\n", adr, val);
	}
	return 0;
}
EXPORT_SYMBOL(VSYNC_WR_MPEG_REG);

int VSYNC_WR_MPEG_REG_BITS(u32 adr, u32 val, u32 start, u32 len)
{
	int i;
	int enable_ = ((enable & enable_mask) | (enable_mask >> 8)) & 0xff;
	if ((enable_ != 0) && rdma_start) {
		u32 read_val = Rd(adr);
		u32 write_val;
		for (i = (rmda_item_count - 1) ; i >= 0; i--) {
			if (rmda_table[i<<1] == adr) {
				read_val = rmda_table[(i<<1)+1];
				break;
			}
		}
		write_val = (read_val & ~(((1L<<(len))-1)<<(start)))
			|((unsigned int)(val) << (start));
		if (debug_flag & 1)
			pr_info("RDMA_WR %d(%x)<=%x\n",
				rmda_item_count, adr, write_val);

		rdma_table_prepare_write(adr, write_val);
	}	else {
		u32 read_val = Rd(adr);
		u32 write_val = (read_val & ~(((1L<<(len))-1)<<(start)))
			|((unsigned int)(val) << (start));
		Wr(adr, write_val);
		if (debug_flag & 1)
			pr_info("VSYNC_WR(%x)<=%x\n", adr, write_val);
	}
	return 0;
}
EXPORT_SYMBOL(VSYNC_WR_MPEG_REG_BITS);

u32 RDMA_READ_REG(u32 adr)
{
	u32 read_val = 0xffffffff;
	return read_val;
}
EXPORT_SYMBOL(RDMA_READ_REG);

int RDMA_SET_READ(u32 adr)
{
	return 0;
}
EXPORT_SYMBOL(RDMA_SET_READ);

bool is_vsync_rdma_enable(void)
{
	int enable_ = ((enable & enable_mask) | (enable_mask >> 8)) & 0xff;
	return (enable_ != 0) && (((enable_mask >> 19) & 0x1) == 0);
}
EXPORT_SYMBOL(is_vsync_rdma_enable);

void start_rdma(void)
{
		if (!rdma_start) {
			rdma_start_flag = 1;
			up(&rdma_sema);
		}
}
EXPORT_SYMBOL(start_rdma);

void enable_rdma_log(int flag)
{
	if (flag)
		debug_flag |= 0x1;
	else
		debug_flag &= (~0x1);
}
EXPORT_SYMBOL(enable_rdma_log);

void enable_rdma(int enable_flag)
{
	enable = enable_flag;
}
EXPORT_SYMBOL(enable_rdma);

static int  __init rdma_init(void)
{
		WRITE_VCBUS_REG(VPU_VDISP_ASYNC_HOLD_CTRL, 0x18101810);
		WRITE_VCBUS_REG(VPU_VPUARB2_ASYNC_HOLD_CTRL, 0x18101810);

	enable = 1;

#if (defined RDMA_CHECK_REG) && (defined RDMA_CHECK_BIT)
	Wr(RDMA_CHECK_REG, Rd(RDMA_CHECK_REG)&(~(1 << RDMA_CHECK_BIT)));
#endif

#if 1
/*def CONFIG_RDMA_IN_TASK*/
	sema_init(&rdma_sema, 1);
	kthread_run(rdma_task_handle, NULL, "kthread_h265");
#endif
	return 0;
}



module_init(rdma_init);
