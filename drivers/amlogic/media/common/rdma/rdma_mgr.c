// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#define pr_fmt(fmt) "rdma: " fmt

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
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/clk.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_fdt.h>

#include <linux/amlogic/media/utils/vdec_reg.h>
#include <linux/amlogic/media/rdma/rdma_mgr.h>
#include <linux/amlogic/media/vpu/vpu.h>
#include "rdma.h"

#define DRIVER_NAME "amlogic-rdma"
#define MODULE_NAME "amlogic-rdma"
#define DEVICE_NAME "rdma"
#define CLASS_NAME  "rdma-class"

#define pr_dbg(fmt, args...)   pr_info("RDMA: " fmt, ## args)
#define pr_error(fmt, args...) pr_err("RDMA: " fmt, ## args)

#define rdma_io_read(addr)      readl(addr)
#define rdma_io_write(addr, val) writel((val), addr)

/* #define SKIP_OSD_CHANNEL */

int rdma_mgr_irq_request;
int rdma_reset_tigger_flag;

static int debug_flag;
/* burst size 0=16; 1=24; 2=32; 3=48.*/
static int ctrl_ahb_rd_burst_size = 3;
static int ctrl_ahb_wr_burst_size = 3;
static int rdma_watchdog = 20;
static int reset_count;
static int rdma_watchdog_count;
static int rdma_force_reset = -1;
static u16 trace_reg;

#define RDMA_NUM        8
#define RDMA_TABLE_SIZE (8 * (PAGE_SIZE))

struct rdma_regadr_s {
	u32 rdma_ahb_start_addr;
	u32 rdma_ahb_end_addr;
	u32 trigger_mask_reg;
	u32 trigger_mask_reg_bitpos;
	u32 addr_inc_reg;
	u32 addr_inc_reg_bitpos;
	u32 rw_flag_reg;
	u32 rw_flag_reg_bitpos;
	u32 clear_irq_bitpos;
	u32 irq_status_bitpos;
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
	u32 rdma_table_phy_addr;
	int rdma_item_count;
	int rdma_write_count;
	unsigned char keep_buf;
	unsigned char used;
	int prev_trigger_type;
};

struct rdma_device_info {
	const char *device_name;
	struct platform_device *rdma_dev;
	struct rdma_instance_s rdma_ins[RDMA_NUM];
};

static struct rdma_device_data_s rdma_meson_dev;
static DEFINE_SPINLOCK(rdma_lock);

static struct rdma_device_info rdma_info;
static struct vpu_dev_s *rdma_vpu_dev;
static struct rdma_regadr_s rdma_regadr[RDMA_NUM] = {
	{RDMA_AHB_START_ADDR_MAN,
		RDMA_AHB_END_ADDR_MAN,
		0, 0,
		RDMA_ACCESS_MAN, 1,
		RDMA_ACCESS_MAN, 2,
		24, 24
	},
	{RDMA_AHB_START_ADDR_1,
		RDMA_AHB_END_ADDR_1,
		RDMA_ACCESS_AUTO,  8,
		RDMA_ACCESS_AUTO,  1,
		RDMA_ACCESS_AUTO,  5,
		25, 25
	},
	{RDMA_AHB_START_ADDR_2,
			RDMA_AHB_END_ADDR_2,
			RDMA_ACCESS_AUTO,  16,
			RDMA_ACCESS_AUTO,  2,
			RDMA_ACCESS_AUTO,  6,
			26, 26
	},
	{RDMA_AHB_START_ADDR_3,
		RDMA_AHB_END_ADDR_3,
		RDMA_ACCESS_AUTO,  24,
		RDMA_ACCESS_AUTO,  3,
		RDMA_ACCESS_AUTO,  7,
		27, 27
	},
	{RDMA_AHB_START_ADDR_4,
			RDMA_AHB_END_ADDR_4,
			RDMA_ACCESS_AUTO3, 0,
			RDMA_ACCESS_AUTO2, 0,
			RDMA_ACCESS_AUTO2, 4,
			28, 28
	},
	{RDMA_AHB_START_ADDR_5,
		RDMA_AHB_END_ADDR_5,
		RDMA_ACCESS_AUTO3, 8,
		RDMA_ACCESS_AUTO2, 1,
		RDMA_ACCESS_AUTO2, 5,
		29, 29
	},
	{RDMA_AHB_START_ADDR_6,
		RDMA_AHB_END_ADDR_6,
		RDMA_ACCESS_AUTO3, 16,
		RDMA_ACCESS_AUTO2, 2,
		RDMA_ACCESS_AUTO2, 6,
		30, 30
	},
	{RDMA_AHB_START_ADDR_7,
		RDMA_AHB_END_ADDR_7,
		RDMA_ACCESS_AUTO3, 24,
		RDMA_ACCESS_AUTO2, 3,
		RDMA_ACCESS_AUTO2, 7,
		31, 31
	}
};

static struct rdma_regadr_s rdma_regadr_tl1[RDMA_NUM] = {
	{RDMA_AHB_START_ADDR_MAN,
		RDMA_AHB_END_ADDR_MAN,
		0, 0,
		RDMA_ACCESS_MAN, 1,
		RDMA_ACCESS_MAN, 2,
		24, 24
	},
	{RDMA_AHB_START_ADDR_1,
		RDMA_AHB_END_ADDR_1,
		RDMA_AUTO_SRC1_SEL,  0,
		RDMA_ACCESS_AUTO,  1,
		RDMA_ACCESS_AUTO,  5,
		25, 25
	},
	{RDMA_AHB_START_ADDR_2,
			RDMA_AHB_END_ADDR_2,
			RDMA_AUTO_SRC2_SEL,  0,
			RDMA_ACCESS_AUTO,  2,
			RDMA_ACCESS_AUTO,  6,
			26, 26
	},
	{RDMA_AHB_START_ADDR_3,
		RDMA_AHB_END_ADDR_3,
		RDMA_AUTO_SRC3_SEL,  0,
		RDMA_ACCESS_AUTO,  3,
		RDMA_ACCESS_AUTO,  7,
		27, 27
	},
	{RDMA_AHB_START_ADDR_4,
			RDMA_AHB_END_ADDR_4,
			RDMA_AUTO_SRC4_SEL, 0,
			RDMA_ACCESS_AUTO2, 0,
			RDMA_ACCESS_AUTO2, 4,
			28, 28
	},
	{RDMA_AHB_START_ADDR_5,
		RDMA_AHB_END_ADDR_5,
		RDMA_AUTO_SRC5_SEL, 0,
		RDMA_ACCESS_AUTO2, 1,
		RDMA_ACCESS_AUTO2, 5,
		29, 29
	},
	{RDMA_AHB_START_ADDR_6,
		RDMA_AHB_END_ADDR_6,
		RDMA_AUTO_SRC6_SEL, 0,
		RDMA_ACCESS_AUTO2, 2,
		RDMA_ACCESS_AUTO2, 6,
		30, 30
	},
	{RDMA_AHB_START_ADDR_7,
		RDMA_AHB_END_ADDR_7,
		RDMA_AUTO_SRC7_SEL, 0,
		RDMA_ACCESS_AUTO2, 3,
		RDMA_ACCESS_AUTO2, 7,
		31, 31
	}
};

int rdma_register(struct rdma_op_s *rdma_op, void *op_arg, int table_size)
{
	int i;
	unsigned long flags;
	struct rdma_device_info *info = &rdma_info;
	dma_addr_t dma_handle;

	spin_lock_irqsave(&rdma_lock, flags);
	for (i = 1; i < RDMA_NUM; i++) {
		/* 0 is reserved for RDMA MANUAL */
		if (!info->rdma_ins[i].op &&
		    info->rdma_ins[i].used == 0) {
			info->rdma_ins[i].op = rdma_op;
			break;
		}
	}
	spin_unlock_irqrestore(&rdma_lock, flags);
	if (i < RDMA_NUM) {
		info->rdma_ins[i].not_process = 0;
		info->rdma_ins[i].op_arg = op_arg;
		info->rdma_ins[i].rdma_item_count = 0;
		info->rdma_ins[i].rdma_write_count = 0;
		if (info->rdma_ins[i].rdma_table_size == 0) {
			info->rdma_ins[i].rdma_table_addr =
				dma_alloc_coherent
				(&info->rdma_dev->dev, table_size,
				&dma_handle, GFP_KERNEL);
			info->rdma_ins[i].rdma_table_phy_addr =
				(u32)(dma_handle);
			info->rdma_ins[i].reg_buf =
				kmalloc(table_size, GFP_KERNEL);
			pr_info("%s, rdma_table_addr %lx phy: %x reg_buf %lx\n",
				__func__,
				(unsigned long)
				info->rdma_ins[i].rdma_table_addr,
				info->rdma_ins[i].rdma_table_phy_addr,
				(unsigned long)info->rdma_ins[i].reg_buf);
			info->rdma_ins[i].rdma_table_size = table_size;
		}

		if (!info->rdma_ins[i].rdma_table_addr ||
		    !info->rdma_ins[i].reg_buf) {
			if (!info->rdma_ins[i].keep_buf) {
				kfree(info->rdma_ins[i].reg_buf);
				info->rdma_ins[i].reg_buf = NULL;
			}
			if (info->rdma_ins[i].rdma_table_addr) {
				dma_free_coherent
				(&info->rdma_dev->dev,
				 table_size,
				 info->rdma_ins[i].rdma_table_addr,
				 (dma_addr_t)
				 info->rdma_ins[i].rdma_table_phy_addr);
				 info->rdma_ins[i].rdma_table_addr = NULL;
			}
			info->rdma_ins[i].rdma_table_size  = 0;
			info->rdma_ins[i].op = NULL;
			i = -1;
			pr_info("%s: memory allocate fail\n",
				__func__);
		} else {
			pr_info("%s success, handle %d table_size %d\n",
				__func__, i, table_size);
		}
		return i;
	}
	return -1;
}
EXPORT_SYMBOL(rdma_register);

void rdma_unregister(int i)
{
	unsigned long flags;
	struct rdma_device_info *info = &rdma_info;

	pr_info("%s(%d)\r\n", __func__, i);
	if (i > 0 && i < RDMA_NUM && info->rdma_ins[i].op) {
		/*rdma_clear(i);*/
		info->rdma_ins[i].op_arg = NULL;
		if (!info->rdma_ins[i].keep_buf) {
			kfree(info->rdma_ins[i].reg_buf);
			info->rdma_ins[i].reg_buf = NULL;
		}
		if (info->rdma_ins[i].rdma_table_addr) {
			dma_free_coherent
			(&info->rdma_dev->dev,
			 info->rdma_ins[i].rdma_table_size,
			 info->rdma_ins[i].rdma_table_addr,
			 (dma_addr_t)
			 info->rdma_ins[i].rdma_table_phy_addr);
			 info->rdma_ins[i].rdma_table_addr = NULL;
		}
		info->rdma_ins[i].rdma_table_size = 0;
		spin_lock_irqsave(&rdma_lock, flags);
		info->rdma_ins[i].op = NULL;
		spin_unlock_irqrestore(&rdma_lock, flags);
	}
}
EXPORT_SYMBOL(rdma_unregister);
static void rdma_reset(unsigned char external_reset)
{
	if (debug_flag & 4)
		pr_info("%s(%d)\n",
			__func__, external_reset);

	if (external_reset) {
		WRITE_MPEG_REG(RESET4_REGISTER,
			       (1 << 5));
	} else {
		WRITE_VCBUS_REG(RDMA_CTRL, (0x1 << 1));
		WRITE_VCBUS_REG(RDMA_CTRL, (0x1 << 1));
		WRITE_VCBUS_REG
		(RDMA_CTRL,
		 (ctrl_ahb_wr_burst_size << 4) |
		 (ctrl_ahb_rd_burst_size << 2) |
		 (0x0 << 1));
	}
	reset_count++;
}

static int rdma_isr_count;
irqreturn_t rdma_mgr_isr(int irq, void *dev_id)
{
	struct rdma_device_info *info = &rdma_info;
	int retry_count = 0;
	u32 rdma_status;
	int i;

	if (debug_flag & 0x10)
		return IRQ_HANDLED;
	rdma_isr_count++;
QUERY:
	retry_count++;
	rdma_status = READ_VCBUS_REG(RDMA_STATUS);
	if ((debug_flag & 4) && ((rdma_isr_count % 30) == 0))
		pr_info("%s: %x\r\n", __func__, rdma_status);
	for (i = 0; i < RDMA_NUM; i++) {
		struct rdma_instance_s *ins = &info->rdma_ins[i];

		if (ins->not_process)
			continue;
		if (!(rdma_status & (1 << (i + 24))))
			continue;
		/*bypass osd rdma done case */
#ifdef SKIP_OSD_CHANNEL
		if (i == 3)
			continue;
#endif
		if (rdma_status & (1 << ins->rdma_regadr->irq_status_bitpos)) {
			if (debug_flag & 2)
				pr_info("%s: process %d\r\n", __func__, i);

			if (ins->op && ins->op->irq_cb)
				ins->op->irq_cb(ins->op->arg);

			WRITE_VCBUS_REG
				(RDMA_CTRL,
				(1 << ins->rdma_regadr->clear_irq_bitpos));
		}
	}
	rdma_status = READ_VCBUS_REG(RDMA_STATUS);
#ifdef SKIP_OSD_CHANNEL
	if ((rdma_status & 0xf7000000) && retry_count < 100)
		goto QUERY;
#else
	if ((rdma_status & 0xff000000) && retry_count < 100)
		goto QUERY;
#endif
	return IRQ_HANDLED;
}

/*
 *	trigger_type:
 *		0, stop,
 *		0x1~0xff, interrupt input trigger mode
 *		0x100, RDMA_TRIGGER_MANUAL
 *		> 0x100, debug mode
 *
 *	return:
 *		-1, fail
 *		0, rdma table is empty, will not have rdma irq
 *		1, success
 */

int rdma_config(int handle, int trigger_type)
{
	int ret = 0;
	unsigned long flags;
	struct rdma_device_info *info = &rdma_info;
	struct rdma_instance_s *ins = &info->rdma_ins[handle];
	bool auto_start = false;

	if (handle == 0 || handle >= RDMA_NUM) {
		pr_info
		("%s error, (handle == %d) not allowed\n",
		 __func__, handle);
		return -1;
	}

	spin_lock_irqsave(&rdma_lock, flags);
	if (!ins->op) {
		spin_unlock_irqrestore(&rdma_lock, flags);

		pr_info("%s: handle (%d) not register\n",
			__func__, handle);
		return -1;
	}

	if (trigger_type & RDMA_AUTO_START_MASK)
		auto_start = true;

	trigger_type &= ~RDMA_AUTO_START_MASK;
	if (auto_start) {
		WRITE_VCBUS_REG_BITS
		(ins->rdma_regadr->trigger_mask_reg,
		 0,
		 ins->rdma_regadr->trigger_mask_reg_bitpos,
		 rdma_meson_dev.trigger_mask_len);

		WRITE_VCBUS_REG_BITS
		(ins->rdma_regadr->addr_inc_reg,
		 0,
		 ins->rdma_regadr->addr_inc_reg_bitpos,
		 1);
		WRITE_VCBUS_REG_BITS
		(ins->rdma_regadr->rw_flag_reg,
		 1,
		 ins->rdma_regadr->rw_flag_reg_bitpos,
		 1);
		WRITE_VCBUS_REG_BITS
		(ins->rdma_regadr->trigger_mask_reg,
		 trigger_type,
		 ins->rdma_regadr->trigger_mask_reg_bitpos,
		 rdma_meson_dev.trigger_mask_len);
		ret = 1;
		ins->rdma_write_count = 0;
	} else if (ins->rdma_item_count <= 0 || trigger_type == 0) {
		if (trigger_type == RDMA_TRIGGER_MANUAL)
			WRITE_VCBUS_REG
			(RDMA_ACCESS_MAN,
			 READ_VCBUS_REG(RDMA_ACCESS_MAN) & (~1));
			if (debug_flag & 2) {
				pr_info("%s: trigger_type %d : %d\r\n",
					__func__,
					trigger_type,
					ins->rdma_item_count);
			}
		WRITE_VCBUS_REG_BITS
		(ins->rdma_regadr->trigger_mask_reg,
		 0, ins->rdma_regadr->trigger_mask_reg_bitpos,
		 rdma_meson_dev.trigger_mask_len);
		ins->rdma_write_count = 0;
		ret = 0;
	} else {
		memcpy(ins->rdma_table_addr, ins->reg_buf,
		       ins->rdma_item_count * 2 * sizeof(u32));

		if (trigger_type > 0 && trigger_type <= RDMA_TRIGGER_MANUAL) {
			ins->rdma_write_count = ins->rdma_item_count;
			ins->prev_trigger_type = trigger_type;
			if (trigger_type == RDMA_TRIGGER_MANUAL) {
				/*manual RDMA */
				struct rdma_instance_s *man_ins =
					&info->rdma_ins[0];

				WRITE_VCBUS_REG
				(RDMA_ACCESS_MAN,
				 READ_VCBUS_REG(RDMA_ACCESS_MAN) & (~1));
				WRITE_VCBUS_REG
				(man_ins->rdma_regadr->rdma_ahb_start_addr,
				 ins->rdma_table_phy_addr);
				WRITE_VCBUS_REG
				(man_ins->rdma_regadr->rdma_ahb_end_addr,
				ins->rdma_table_phy_addr
				+ ins->rdma_item_count * 8 - 1);

				WRITE_VCBUS_REG_BITS
				(man_ins->rdma_regadr->addr_inc_reg,
				 0,
				 man_ins->rdma_regadr->addr_inc_reg_bitpos,
				 1);
				WRITE_VCBUS_REG_BITS
				(man_ins->rdma_regadr->rw_flag_reg,
				 1,
				 man_ins->rdma_regadr->rw_flag_reg_bitpos,
				 1);
				/* Manual-start RDMA*/
				WRITE_VCBUS_REG
				(RDMA_ACCESS_MAN,
				 READ_VCBUS_REG(RDMA_ACCESS_MAN) | 1);

				if (debug_flag & 2)
					pr_info("%s: manual config %d:\r\n",
						__func__, ins->rdma_item_count);
			} else {
				/* interrupt input trigger RDMA */
				if (debug_flag & 2)
					pr_info("%s: case 3 : %d:\r\n",
						__func__, ins->rdma_item_count);
				WRITE_VCBUS_REG_BITS
				(ins->rdma_regadr->trigger_mask_reg,
				 0,
				 ins->rdma_regadr->trigger_mask_reg_bitpos,
				 rdma_meson_dev.trigger_mask_len);

				WRITE_VCBUS_REG
				(ins->rdma_regadr->rdma_ahb_start_addr,
				 ins->rdma_table_phy_addr);
				WRITE_VCBUS_REG
				(ins->rdma_regadr->rdma_ahb_end_addr,
				 ins->rdma_table_phy_addr
				 + ins->rdma_item_count * 8 - 1);

				WRITE_VCBUS_REG_BITS
				(ins->rdma_regadr->addr_inc_reg,
				 0,
				 ins->rdma_regadr->addr_inc_reg_bitpos,
				 1);
				WRITE_VCBUS_REG_BITS
				(ins->rdma_regadr->rw_flag_reg,
				 1,
				 ins->rdma_regadr->rw_flag_reg_bitpos,
				 1);
				WRITE_VCBUS_REG_BITS
				(ins->rdma_regadr->trigger_mask_reg,
				 trigger_type,
				 ins->rdma_regadr->trigger_mask_reg_bitpos,
				 rdma_meson_dev.trigger_mask_len);
			}
		} else if (trigger_type == 0x101) {	/* debug mode */
			int i;

			for (i = 0; i < ins->rdma_item_count; i++) {
				WRITE_VCBUS_REG
					(ins->rdma_table_addr[i << 1],
					 ins->rdma_table_addr[(i << 1) + 1]);
				if (debug_flag & 1)
					pr_info("WR(%x)<=%x\n",
						ins->rdma_table_addr[i << 1],
						ins->rdma_table_addr
						[(i << 1) + 1]);
			}
			ins->rdma_write_count = 0;
		} else if (trigger_type == 0x102) { /* debug mode */
			int i;

			for (i = 0; i < ins->rdma_item_count; i++) {
				WRITE_VCBUS_REG(ins->reg_buf[i << 1],
						ins->reg_buf[(i << 1) + 1]);
				if (debug_flag & 1)
					pr_info("WR(%x)<=%x\n",
						ins->reg_buf[i << 1],
						ins->reg_buf[(i << 1) + 1]);
			}
			ins->rdma_write_count = 0;
		}
		ret = 1;
	}
	ins->rdma_item_count = 0;
	spin_unlock_irqrestore(&rdma_lock, flags);

	if (debug_flag & 2)
		pr_info("%s: (%d 0x%x) ret %d\r\n",
			__func__, handle, trigger_type, ret);

	return ret;
}
EXPORT_SYMBOL(rdma_config);

int rdma_clear(int handle)
{
	int ret = 0;
	unsigned long flags;
	struct rdma_device_info *info = &rdma_info;
	struct rdma_instance_s *ins = &info->rdma_ins[handle];

	spin_lock_irqsave(&rdma_lock, flags);
	if (handle <= 0 ||
	    handle >= RDMA_NUM ||
	    !ins->op) {
		spin_unlock_irqrestore(&rdma_lock, flags);
		pr_info("%s error, handle (%d) not register\n",
			__func__, handle);
		return -1;
	}
	WRITE_VCBUS_REG_BITS(ins->rdma_regadr->trigger_mask_reg,
			     0, ins->rdma_regadr->trigger_mask_reg_bitpos,
			     rdma_meson_dev.trigger_mask_len);
	ins->rdma_write_count = 0;
	spin_unlock_irqrestore(&rdma_lock, flags);
	return ret;
}
EXPORT_SYMBOL(rdma_clear);

u32 rdma_read_reg(int handle, u32 adr)
{
	int i;
	u32 *write_table;
	int match = 0;
	int read_from = 0;
	struct rdma_device_info *info = &rdma_info;
	struct rdma_instance_s *ins = &info->rdma_ins[handle];
	u32 read_val = READ_VCBUS_REG(adr);

	for (i = (ins->rdma_item_count - 1); i >= 0; i--) {
		if (ins->reg_buf[i << 1] == adr) {
			read_val = ins->reg_buf[(i << 1) + 1];
			match = 1;
			read_from = 1;
			break;
		}
	}
	if (!match) {
		write_table = ins->rdma_table_addr;
		for (i = (ins->rdma_write_count - 1);
			i >= 0; i--) {
			if (write_table[i << 1] == adr) {
				read_val =
					write_table[(i << 1) + 1];
				read_from = 2;
				break;
			}
		}
	}
	if (adr == trace_reg) {
		if (read_from == 2)
			pr_info("(%s) handle %d, %04x=0x%08x from write table(%d)\n",
				__func__,
				handle, adr,
				read_val,
				ins->rdma_write_count);
		else if (read_from == 1)
			pr_info("(%s) handle %d, %04x=0x%08x from item table(%d)\n",
				__func__,
				handle, adr,
				read_val,
				ins->rdma_item_count);
		else
			pr_info("(%s) handle %d, %04x=0x%08x from real reg\n",
				__func__,
				handle, adr,
				read_val);
	}
	return read_val;
}
EXPORT_SYMBOL(rdma_read_reg);

int rdma_watchdog_setting(int flag)
{
	int ret = 0;

	if (flag == 0)
		rdma_watchdog_count = 0;
	else
		rdma_watchdog_count++;

	if (debug_flag & 8) {
		rdma_force_reset = 1;
		debug_flag = 0;
	}
	if ((rdma_watchdog > 0 &&
	     rdma_watchdog_count > rdma_watchdog) ||
	     rdma_force_reset > 0) {
		pr_info("%s rdma reset: %d, force flag:%d\n",
			__func__,
			rdma_watchdog_count,
			rdma_force_reset);
		rdma_watchdog_count = 0;
		rdma_force_reset = 0;
		rdma_reset(1);
		rdma_reset_tigger_flag = 1;
		ret = 1;
	}
	return ret;
}
EXPORT_SYMBOL(rdma_watchdog_setting);

int rdma_write_reg(int handle, u32 adr, u32 val)
{
	struct rdma_device_info *info = &rdma_info;
	struct rdma_instance_s *ins = &info->rdma_ins[handle];

	if (ins->rdma_table_size == 0)
		return -1;

	if (debug_flag & 1)
		pr_info("rdma_write(%d) %d(%x)<=%x\n",
			handle, ins->rdma_item_count, adr, val);
	if (((ins->rdma_item_count << 1) + 1) <
		(ins->rdma_table_size / sizeof(u32))) {
		ins->reg_buf[ins->rdma_item_count << 1] = adr;
		ins->reg_buf[(ins->rdma_item_count << 1) + 1] = val;
		ins->rdma_item_count++;
	} else {
		int i;

		if (debug_flag & 4)
			pr_info("%s(%d, %x, %x ,%d) buf overflow\n",
				__func__, rdma_watchdog_count,
				handle, adr, val);
		for (i = 0; i < ins->rdma_item_count; i++)
			WRITE_VCBUS_REG(ins->reg_buf[i << 1],
					ins->reg_buf[(i << 1) + 1]);
		ins->rdma_item_count = 0;
		ins->rdma_write_count = 0;
		ins->reg_buf[ins->rdma_item_count << 1] = adr;
		ins->reg_buf[(ins->rdma_item_count << 1) + 1] = val;
		ins->rdma_item_count++;
	}
	if (adr == trace_reg)
		pr_info("(%s) handle %d, %04x=0x%08x (%d)\n",
			__func__,
			handle, adr,
			val,
			ins->rdma_item_count);
	return 0;
}
EXPORT_SYMBOL(rdma_write_reg);

int rdma_write_reg_bits(int handle, u32 adr, u32 val, u32 start, u32 len)
{
	int i;
	u32 *write_table;
	int match = 0;
	int read_from = 0;
	struct rdma_device_info *info = &rdma_info;
	struct rdma_instance_s *ins = &info->rdma_ins[handle];
	u32 read_val = READ_VCBUS_REG(adr);
	u32 write_val;

	if (ins->rdma_table_size == 0)
		return -1;

	for (i = (ins->rdma_item_count - 1); i >= 0; i--) {
		if (ins->reg_buf[i << 1] == adr) {
			read_val = ins->reg_buf[(i << 1) + 1];
			match = 1;
			read_from = 1;
			break;
		}
	}
	if (!match) {
		write_table = ins->rdma_table_addr;
		for (i = (ins->rdma_write_count - 1);
			i >= 0; i--) {
			if (write_table[i << 1] == adr) {
				read_val =
					write_table[(i << 1) + 1];
				read_from = 2;
				break;
			}
		}
	}
	write_val = (read_val & ~(((1L << (len)) - 1) << (start)))
		| ((unsigned int)(val) << (start));

	if (adr == trace_reg) {
		if (read_from == 2)
			pr_info("(%s) handle %d, %04x=0x%08x->0x%08x from write table(%d)\n",
				__func__,
				handle, adr,
				read_val,
				write_val,
				ins->rdma_write_count);
		else if (read_from == 1)
			pr_info("(%s) handle %d, %04x=0x%08x->0x%08x from item table(%d)\n",
				__func__,
				handle, adr,
				read_val,
				write_val,
				ins->rdma_item_count);
		else
			pr_info("(%s) handle %d, %04x=0x%08x->0x%08x from real reg\n",
				__func__,
				handle, adr,
				read_val,
				write_val);
	}
	if (match) {
		ins->reg_buf[(i << 1) + 1] = write_val;
		return 0;
	}
	if (debug_flag & 1)
		pr_info("rdma_write(%d) %d(%x)<=%x\n",
			handle, ins->rdma_item_count, adr, write_val);

	rdma_write_reg(handle, adr, write_val);
	return 0;
}
EXPORT_SYMBOL(rdma_write_reg_bits);

static struct rdma_device_data_s rdma_meson = {
	.cpu_type = CPU_NORMAL,
	.rdma_ver = RDMA_VER_1,
	.trigger_mask_len = 8,
};

static struct rdma_device_data_s rdma_g12b_reva = {
	.cpu_type = CPU_G12B_REVA,
	.rdma_ver = RDMA_VER_1,
	.trigger_mask_len = 8,
};

static struct rdma_device_data_s rdma_g12b_revb = {
	.cpu_type = CPU_G12B_REVB,
	.rdma_ver = RDMA_VER_1,
	.trigger_mask_len = 8,
};

static struct rdma_device_data_s rdma_tl1 = {
	.cpu_type = CPU_NORMAL,
	.rdma_ver = RDMA_VER_2,
	.trigger_mask_len = 16,
};

static const struct of_device_id rdma_dt_match[] = {
	{
		.compatible = "amlogic, meson, rdma",
		.data = &rdma_meson,
	},
	{
		.compatible = "amlogic, meson-g12b-reva, rdma",
		.data = &rdma_g12b_reva,
	},
	{
		.compatible = "amlogic, meson-g12b-revb, rdma",
		.data = &rdma_g12b_revb,
	},
	{
		.compatible = "amlogic, meson-tl1, rdma",
		.data = &rdma_tl1,
	},
	{},
};

u32 is_meson_g12b_revb(void)
{
	if (rdma_meson_dev.cpu_type == CPU_G12B_REVB)
		return 1;
	else
		return 0;
}

static ssize_t show_debug_flag(struct class *class,
			       struct class_attribute *attr,
			       char *buf)
{
	return snprintf(buf, 40, "%d\n", debug_flag);
}

static ssize_t store_debug_flag(struct class *class,
				struct class_attribute *attr,
				const char *buf, size_t count)
{
	int res = 0;
	int ret = 0;

	ret = kstrtoint(buf, 0, &res);
	pr_info("debug_flag: %d->%d\n", debug_flag, res);
	debug_flag = res;

	return count;
}

static ssize_t show_rdma_watchdog(struct class *class,
				  struct class_attribute *attr,
				  char *buf)
{
	return snprintf(buf, 40, "%d\n", rdma_watchdog);
}

static ssize_t store_rdma_watchdog(struct class *class,
				   struct class_attribute *attr,
				   const char *buf, size_t count)
{
	int res = 0;
	int ret = 0;

	ret = kstrtoint(buf, 0, &res);
	pr_info("rdma_watchdog: %d->%d\n", rdma_watchdog, res);
	rdma_watchdog = res;

	return count;
}

static ssize_t show_reset_count(struct class *class,
				struct class_attribute *attr,
				char *buf)
{
	return snprintf(buf, 40, "%d\n", reset_count);
}

static ssize_t store_reset_count(struct class *class,
				 struct class_attribute *attr,
				 const char *buf, size_t count)
{
	int res = 0;
	int ret = 0;

	ret = kstrtoint(buf, 0, &res);
	pr_info("reset_count: %d->%d\n", reset_count, res);
	reset_count = res;

	return count;
}

static ssize_t show_ctrl_ahb_rd_burst_size(struct class *class,
					   struct class_attribute *attr,
					   char *buf)
{
	return snprintf(buf, 40, "%d\n", ctrl_ahb_rd_burst_size);
}

static ssize_t store_ctrl_ahb_rd_burst_size(struct class *class,
					    struct class_attribute *attr,
					    const char *buf, size_t count)
{
	int res = 0;
	int ret = 0;

	ret = kstrtoint(buf, 0, &res);
	pr_info("ctrl_ahb_rd_burst_size: %d->%d\n",
		ctrl_ahb_rd_burst_size, res);
	ctrl_ahb_rd_burst_size = res;

	return count;
}

static ssize_t show_ctrl_ahb_wr_burst_size(struct class *class,
					   struct class_attribute *attr,
					   char *buf)
{
	return snprintf(buf, 40, "%d\n", ctrl_ahb_wr_burst_size);
}

static ssize_t store_ctrl_ahb_wr_burst_size(struct class *class,
					    struct class_attribute *attr,
					    const char *buf, size_t count)
{
	int res = 0;
	int ret = 0;

	ret = kstrtoint(buf, 0, &res);
	pr_info("ctrl_ahb_wr_burst_size: %d->%d\n",
		ctrl_ahb_wr_burst_size, res);
	ctrl_ahb_wr_burst_size = res;

	return count;
}

static ssize_t show_trace_reg(struct class *class,
			      struct class_attribute *attr,
			      char *buf)
{
	return snprintf(buf, 40, "%d\n", trace_reg);
}

static ssize_t store_trace_reg(struct class *class,
			       struct class_attribute *attr,
			       const char *buf, size_t count)
{
	int res = 0;
	int ret = 0;

	ret = kstrtoint(buf, 0, &res);
	pr_info("ctrl_ahb_wr_burst_size: %d->%d\n", trace_reg, res);
	trace_reg = res;

	return count;
}

static struct class_attribute rdma_mgr_attrs[] = {
	__ATTR(debug_flag, 0664,
	       show_debug_flag, store_debug_flag),
	__ATTR(rdma_watchdog, 0664,
	       show_rdma_watchdog, store_rdma_watchdog),
	__ATTR(reset_count, 0664,
	       show_reset_count, store_reset_count),
	__ATTR(ctrl_ahb_rd_burst_size, 0664,
	       show_ctrl_ahb_rd_burst_size, store_ctrl_ahb_rd_burst_size),
	__ATTR(ctrl_ahb_wr_burst_size, 0664,
	       show_ctrl_ahb_wr_burst_size, store_ctrl_ahb_wr_burst_size),
	__ATTR(trace_reg, 0664,
	       show_trace_reg, store_trace_reg),
};

static struct class *rdma_mgr_class;
static int create_rdma_mgr_class(void)
{
	int i;

	rdma_mgr_class = class_create(THIS_MODULE, "rdma_mgr");
	if (IS_ERR_OR_NULL(rdma_mgr_class)) {
		pr_err("create rdma_mgr_class failed\n");
		return -1;
	}

	for (i = 0; i < ARRAY_SIZE(rdma_mgr_attrs); i++) {
		if (class_create_file(rdma_mgr_class,
				      &rdma_mgr_attrs[i])) {
			pr_err("create rdma mgr attribute %s failed\n",
			       rdma_mgr_attrs[i].attr.name);
		}
	}
	return 0;
}

static int remove_rdma_mgr_class(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(rdma_mgr_attrs); i++)
		class_remove_file(rdma_mgr_class, &rdma_mgr_attrs[i]);

	class_destroy(rdma_mgr_class);
	rdma_mgr_class = NULL;
	return 0;
}

/* static int __devinit rdma_probe(struct platform_device *pdev) */
static int __init rdma_probe(struct platform_device *pdev)
{
	int i;
	u32 data32;
	int int_rdma;
	int handle;
	struct rdma_device_info *info = &rdma_info;

	int_rdma = platform_get_irq_byname(pdev, "rdma");
	if (int_rdma == -ENXIO) {
		dev_err(&pdev->dev, "cannot get rdma irq resource\n");
		return -ENODEV;
	}

	if (pdev->dev.of_node) {
		const struct of_device_id *match;
		struct rdma_device_data_s *rdma_meson;
		struct device_node	*of_node = pdev->dev.of_node;

		match = of_match_node(rdma_dt_match, of_node);
		if (match) {
			rdma_meson = (struct rdma_device_data_s *)match->data;
			if (rdma_meson) {
				memcpy(&rdma_meson_dev, rdma_meson,
				       sizeof(struct rdma_device_data_s));
			} else {
				pr_err("%s data NOT match\n", __func__);
				return -ENODEV;
			}
		} else {
			pr_err("%s NOT match\n", __func__);
			return -ENODEV;
		}
	} else {
		pr_err("dev %s NOT found\n", __func__);
		return -ENODEV;
	}
	pr_info("%s,cpu_type:%d, ver:%d, len:%d\n", __func__,
		rdma_meson_dev.cpu_type,
		rdma_meson_dev.rdma_ver, rdma_meson_dev.trigger_mask_len);

	rdma_vpu_dev = vpu_dev_register(VPU_RDMA, "rdma");
	vpu_dev_mem_power_on(rdma_vpu_dev);

	WRITE_VCBUS_REG(VPU_VDISP_ASYNC_HOLD_CTRL, 0x18101810);
	WRITE_VCBUS_REG(VPU_VPUARB2_ASYNC_HOLD_CTRL, 0x18101810);

	rdma_mgr_irq_request = 0;
	trace_reg = 0;

	for (i = 0; i < RDMA_NUM; i++) {
		info->rdma_ins[i].rdma_table_size = 0;
		if (rdma_meson_dev.rdma_ver == RDMA_VER_1)
			info->rdma_ins[i].rdma_regadr = &rdma_regadr[i];
		else if (rdma_meson_dev.rdma_ver == RDMA_VER_2)
			info->rdma_ins[i].rdma_regadr = &rdma_regadr_tl1[i];
		else
			info->rdma_ins[i].rdma_regadr = &rdma_regadr[i];
		info->rdma_ins[i].keep_buf = 1;
		/*do not change it in normal case */
		info->rdma_ins[i].used = 0;
		info->rdma_ins[i].prev_trigger_type = 0;
		info->rdma_ins[i].rdma_write_count = 0;
	}

	WRITE_MPEG_REG(RESET4_REGISTER, (1 << 5));

#ifdef SKIP_OSD_CHANNEL
	info->rdma_ins[3].used = 1; /* OSD driver uses this channel */
#endif

	if (request_irq(int_rdma, &rdma_mgr_isr,
			IRQF_SHARED, "rdma", (void *)"rdma")) {
		dev_err(&pdev->dev, "can't request irq for rdma\n");
		return -ENODEV;
	}

	rdma_mgr_irq_request = 1;
	data32  = 0;
	data32 |= 1 << 7; /* wrtie ddr urgent */
	data32 |= 1 << 6; /* read ddr urgent */
	data32 |= ctrl_ahb_wr_burst_size << 4;
	data32 |= ctrl_ahb_rd_burst_size << 2;
	data32 |= 0 << 1;
	data32 |= 0 << 0;
	WRITE_VCBUS_REG(RDMA_CTRL, data32);

	info->rdma_dev = pdev;

	handle = rdma_register(get_rdma_ops(VSYNC_RDMA),
			       NULL, RDMA_TABLE_SIZE);
	set_rdma_handle(VSYNC_RDMA, handle);
	if (is_meson_g12b_revb()) {
		pr_info("g12b revb!!!!\n");
		handle = rdma_register(get_rdma_ops(LINE_N_INT_RDMA),
				       NULL, RDMA_TABLE_SIZE);
		set_rdma_handle(LINE_N_INT_RDMA, handle);
	}

	create_rdma_mgr_class();

	rdma_init();
	return 0;
}

/* static int __devexit rdma_remove(struct platform_device *pdev) */
static int rdma_remove(struct platform_device *pdev)
{
	pr_error("RDMA driver removed.\n");
	remove_rdma_mgr_class();
	rdma_exit();

	vpu_dev_mem_power_down(rdma_vpu_dev);
	vpu_dev_unregister(rdma_vpu_dev);
	return 0;
}

static struct platform_driver rdma_driver = {
	.remove = rdma_remove,
	.driver = {
		.name = "amlogic-rdma",
		.of_match_table = rdma_dt_match,
	},
};

static int __init amrdma_init(void)
{
	return platform_driver_probe(&rdma_driver, rdma_probe);
}

static void __exit amrdma_exit(void)
{
	platform_driver_unregister(&rdma_driver);
}

subsys_initcall(amrdma_init);
module_exit(amrdma_exit);

MODULE_DESCRIPTION("AMLOGIC RDMA management driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Rain Zhang <rain.zhang@amlogic.com>");
