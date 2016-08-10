/*
 * drivers/amlogic/display/osd/osd_rdma.c
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

/* Linux Headers */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/string.h>
#include <linux/io.h>
#include <linux/ctype.h>
#include <linux/poll.h>
#include <linux/clk.h>
#include <linux/slab.h>
#include <linux/dma-contiguous.h>
#include <linux/dma-mapping.h>
#include <linux/mm.h>

/* Local Headers */
#include <linux/amlogic/cpu_version.h>
#include "osd.h"
#include "osd_io.h"
#include "osd_reg.h"
#include "osd_rdma.h"
#include "osd_hw.h"

#define RDMA_TABLE_INTERNAL_COUNT 512

static DEFINE_SPINLOCK(rdma_lock);
static struct rdma_table_item *rdma_table;
static struct rdma_table_item *rdma_table_internal;
static struct device *osd_rdma_dev;
static struct page *table_pages;
static void *osd_rdma_table_virt;
static dma_addr_t osd_rdma_table_phy;
static u32 table_paddr;
static void *table_vaddr;
static u32 rdma_enable;
static u32 item_count;
static u32 item_count_internal;
static u32 rdma_debug;
static bool osd_rdma_init_flag;
static int ctrl_ahb_rd_burst_size = 3;
static int ctrl_ahb_wr_burst_size = 3;
#define OSD_RDMA_UPDATE_RETRY_COUNT 100
static unsigned int debug_rdma_status;
static unsigned int rdma_irq_count;
static unsigned int rdma_lost_count;
static unsigned int dump_reg_trigger;
static unsigned int second_rdma_irq;

static int osd_rdma_init(void);

static inline void osd_rdma_mem_cpy(struct rdma_table_item *dst,
					struct rdma_table_item *src, u32 len)
{
	asm volatile(
		"	stp x5, x6, [sp, #-16]!\n"
		"	cmp %2,#8\n"
		"	bne 1f\n"
		"	ldr x5, [%0]\n"
		"	str x5, [%1]\n"
		"	b 2f\n"
		"1:     ldp x5, x6, [%0]\n"
		"	stp x5, x6, [%1]\n"
		"2:     nop\n"
		"	ldp x5, x6, [sp], #16\n"
		:
		: "r" (src), "r" (dst), "r" (len)
		: "x5", "x6");
}

/*once init, will update config in every osd rdma interrupt*/
int osd_rdma_update_config(char is_init)
{
	static u32 config;

	if (is_init) {
		config  = 0;
		config |= 1                         << 7;   /* [31: 6] Rsrv.*/
		config |= 1                         << 6;   /* [31: 6] Rsrv.*/
		config |= ctrl_ahb_wr_burst_size    <<
			4;
		/* [ 5: 4] ctrl_ahb_wr_burst_size. 0=16; 1=24; 2=32; 3=48.*/
		config |= ctrl_ahb_rd_burst_size    <<
			2;
		/* [ 3: 2] ctrl_ahb_rd_burst_size. 0=16; 1=24; 2=32; 3=48.*/
		config |= 0                         << 1;
		/* [    1] ctrl_sw_reset.*/
		config |= 0                         << 0;
		/* [    0] ctrl_free_clk_enable.*/
		osd_reg_write(RDMA_CTRL, config);
	} else {
		osd_reg_write(RDMA_CTRL, (1<<27)|config);
	}
	return 0;

}
EXPORT_SYMBOL(osd_rdma_update_config);

static inline void reset_rdma_table(void)
{
	unsigned long flags;
	u32 old_count;
	u32 end_addr;
	int i, j = 0;
	struct rdma_table_item reset_item[2] = {
		{
			.addr = OSD_RDMA_FLAG_REG,
			.val = OSD_RDMA_STATUS_MARK_TBL_RST,
		},
		{
			.addr = OSD_RDMA_FLAG_REG,
			.val = OSD_RDMA_STATUS_MARK_TBL_DONE,
		}
	};

	spin_lock_irqsave(&rdma_lock, flags);
	if (!OSD_RDMA_STATUS_IS_REJECT) {
		end_addr = osd_reg_read(END_ADDR) + 1;
		if (end_addr > table_paddr)
			old_count = (end_addr - table_paddr) >> 3;
		else
			old_count = 0;
		osd_reg_write(END_ADDR, table_paddr - 1);
		if (old_count < item_count) {
			for (i = 0; i < item_count - old_count; i++) {
				if (rdma_table[old_count + i].addr
					== OSD_RDMA_FLAG_REG)
					continue;
				osd_rdma_mem_cpy(
					&rdma_table[1 + j],
					&rdma_table[old_count + i], 8);
				j++;
			}
			item_count = j + 2;
		} else {
			item_count = 2;
		}
		osd_rdma_mem_cpy(rdma_table, &reset_item[0], 8);
		osd_rdma_mem_cpy(&rdma_table[item_count - 1],
			&reset_item[1], 8);
		osd_reg_write(END_ADDR,
			(table_paddr + item_count * 8 - 1));
	}
	spin_unlock_irqrestore(&rdma_lock, flags);
}

static int update_table_item(u32 addr, u32 val, u8 irq_mode)
{
	unsigned long flags;
	int retry_count = OSD_RDMA_UPDATE_RETRY_COUNT;
	struct rdma_table_item request_item;
	int reject1 = 0, reject2 = 0, ret = 0;
	u32 paddr;

	if (item_count > 500) {
		/* rdma table is full */
		pr_info("update_table_item overflow!\n");
		return -1;
	}
	/* pr_debug("%02dth, ctrl: 0x%x, status: 0x%x, auto:0x%x, flag:0x%x\n",
		item_count, osd_reg_read(RDMA_CTRL),
		osd_reg_read(RDMA_STATUS),
		osd_reg_read(RDMA_ACCESS_AUTO),
		osd_reg_read(OSD_RDMA_FLAG_REG)); */
retry:
	if (0 == (retry_count--)) {
		pr_debug("OSD RDMA stuck: 0x%x = 0x%x, status: 0x%x\n",
			addr, val, osd_reg_read(RDMA_STATUS));
		pr_debug("::retry count: %d-%d, count: %d, flag: 0x%x\n",
			reject1, reject2, item_count,
			osd_reg_read(OSD_RDMA_FLAG_REG));
		spin_lock_irqsave(&rdma_lock, flags);
		request_item.addr = OSD_RDMA_FLAG_REG;
		request_item.val = OSD_RDMA_STATUS_MARK_TBL_DONE;
		osd_rdma_mem_cpy(
			&rdma_table[item_count],
			&request_item, 8);
		request_item.addr = addr;
		request_item.val = val;
		osd_rdma_mem_cpy(
			&rdma_table[item_count - 1],
			&request_item, 8);
		item_count++;
		spin_unlock_irqrestore(&rdma_lock, flags);
		return -1;
	}

	if ((OSD_RDMA_STATUS_IS_REJECT) && (irq_mode)) {
		/* should not be here. Using the wrong write function
			or rdma isr is block */
		pr_info("update reg but rdma running, mode: %d\n",
			irq_mode);
		return -2;
	}

	if ((OSD_RDMA_STATUS_IS_REJECT) && (!irq_mode)) {
		/* should not be here. Using the wrong write function
			or rdma isr is block */
		reject1++;
		pr_debug("update reg but rdma running, mode: %d,",
			irq_mode);
		pr_debug("retry count:%d (%d), flag: 0x%x, status: 0x%x\n",
			retry_count, reject1,
			osd_reg_read(OSD_RDMA_FLAG_REG),
			osd_reg_read(RDMA_STATUS));
		goto retry;
	}

	/*atom_lock_start:*/
	spin_lock_irqsave(&rdma_lock, flags);
	request_item.addr = OSD_RDMA_FLAG_REG;
	request_item.val = OSD_RDMA_STATUS_MARK_TBL_DONE;
	osd_rdma_mem_cpy(&rdma_table[item_count], &request_item, 8);
	request_item.addr = addr;
	request_item.val = val;
	osd_rdma_mem_cpy(&rdma_table[item_count - 1], &request_item, 8);
	item_count++;
	paddr = table_paddr + item_count * 8 - 1;
	if (!OSD_RDMA_STATUS_IS_REJECT) {
		osd_reg_write(END_ADDR, paddr);
	} else if (!irq_mode) {
		reject2++;
		pr_debug("need update ---, but rdma running,");
		pr_debug("retry count:%d (%d), flag: 0x%x, status: 0x%x\n",
			retry_count, reject2,
			osd_reg_read(OSD_RDMA_FLAG_REG),
			osd_reg_read(RDMA_STATUS));
		item_count--;
		spin_unlock_irqrestore(&rdma_lock, flags);
		goto retry;
	} else
		ret = -3;
	/*atom_lock_end:*/
	spin_unlock_irqrestore(&rdma_lock, flags);
	return ret;
}

static inline int update_table_item_internal(u32 addr, u32 val)
{
	struct rdma_table_item request_item;
	if (item_count_internal > 500) {
		/* rdma table is full */
		pr_info("update_table_item_internal overflow!\n");
		return -1;
	}
	request_item.addr = addr;
	request_item.val = val;
	memcpy(
		&rdma_table_internal[item_count_internal],
		&request_item, 8);
	item_count_internal++;
	return 0;
}

static inline u32 read_reg_internal(u32 addr)
{
	int  i;
	u32 val = 0;
	if (rdma_enable) {
		for (i = (item_count - 1); i >= 0; i--) {
			if (addr == rdma_table[i].addr) {
				val = rdma_table[i].val;
				break;
			}
		}
		if (i >= 0)
			return val;
	}
	return osd_reg_read(addr);
}

static inline int wrtie_reg_internal(u32 addr, u32 val)
{
	struct rdma_table_item request_item;

	if (!rdma_enable) {
		osd_reg_write(addr, val);
		return 0;
	}

	if (item_count > 500) {
		/* rdma table is full */
		pr_info("wrtie_reg_internal overflow!\n");
		return -1;
	}
	/* TODO remove the Done write operation to save the time */
	request_item.addr = OSD_RDMA_FLAG_REG;
	request_item.val = OSD_RDMA_STATUS_MARK_TBL_DONE;
	memcpy(
		&rdma_table[item_count],
		&request_item, 8);
	request_item.addr = addr;
	request_item.val = val;
	memcpy(
		&rdma_table[item_count - 1],
		&request_item, 8);
	item_count++;
	return 0;
}

u32 VSYNCOSD_RD_MPEG_REG(u32 addr)
{
	int  i;
	u32 val = 0;
	unsigned long flags;

	if (rdma_enable) {
		spin_lock_irqsave(&rdma_lock, flags);
		for (i = (item_count - 1); i >= 0; i--) {
			if (addr == rdma_table[i].addr) {
				val = rdma_table[i].val;
				break;
			}
		}
		spin_unlock_irqrestore(&rdma_lock, flags);
		if (i >= 0)
			return val;
	}
	return osd_reg_read(addr);
}
EXPORT_SYMBOL(VSYNCOSD_RD_MPEG_REG);

int VSYNCOSD_WR_MPEG_REG(u32 addr, u32 val)
{
	int ret = 0;
	if (rdma_enable)
		ret = update_table_item(addr, val, 0);
	else
		osd_reg_write(addr, val);
	return ret;
}
EXPORT_SYMBOL(VSYNCOSD_WR_MPEG_REG);

int VSYNCOSD_WR_MPEG_REG_BITS(u32 addr, u32 val, u32 start, u32 len)
{
	unsigned long read_val;
	unsigned long write_val;
	int ret = 0;
	if (rdma_enable) {
		read_val = VSYNCOSD_RD_MPEG_REG(addr);
		write_val = (read_val & ~(((1L << (len)) - 1) << (start)))
			    | ((unsigned int)(val) << (start));
		ret = update_table_item(addr, write_val, 0);
	} else
		osd_reg_set_bits(addr, val, start, len);
	return ret;
}
EXPORT_SYMBOL(VSYNCOSD_WR_MPEG_REG_BITS);

int VSYNCOSD_SET_MPEG_REG_MASK(u32 addr, u32 _mask)
{
	unsigned long read_val;
	unsigned long write_val;
	int ret = 0;
	if (rdma_enable) {
		read_val = VSYNCOSD_RD_MPEG_REG(addr);
		write_val = read_val | _mask;
		ret = update_table_item(addr, write_val, 0);
	} else
		osd_reg_set_mask(addr, _mask);
	return ret;
}
EXPORT_SYMBOL(VSYNCOSD_SET_MPEG_REG_MASK);

int VSYNCOSD_CLR_MPEG_REG_MASK(u32 addr, u32 _mask)
{
	unsigned long read_val;
	unsigned long write_val;
	int ret = 0;
	if (rdma_enable) {
		read_val = VSYNCOSD_RD_MPEG_REG(addr);
		write_val = read_val & (~_mask);
		ret = update_table_item(addr, write_val, 0);
	} else
		osd_reg_clr_mask(addr, _mask);
	return ret;
}
EXPORT_SYMBOL(VSYNCOSD_CLR_MPEG_REG_MASK);

int VSYNCOSD_IRQ_WR_MPEG_REG(u32 addr, u32 val)
{
	int ret = 0;
	if (rdma_enable)
		ret = update_table_item(addr, val, 1);
	else
		osd_reg_write(addr, val);
	return ret;
}
EXPORT_SYMBOL(VSYNCOSD_IRQ_WR_MPEG_REG);

int VSYNCOSD_IRQ_WR_MPEG_REG_BITS(u32 addr, u32 val, u32 start, u32 len)
{
	unsigned long read_val;
	unsigned long write_val;
	int ret = 0;
	if (rdma_enable) {
		read_val = VSYNCOSD_RD_MPEG_REG(addr);
		write_val = (read_val & ~(((1L << (len)) - 1) << (start)))
			    | ((unsigned int)(val) << (start));
		ret = update_table_item(addr, write_val, 1);
	} else
		osd_reg_set_bits(addr, val, start, len);
	return ret;
}
EXPORT_SYMBOL(VSYNCOSD_IRQ_WR_MPEG_REG_BITS);

int VSYNCOSD_IRQ_SET_MPEG_REG_MASK(u32 addr, u32 _mask)
{
	unsigned long read_val;
	unsigned long write_val;
	int ret = 0;
	if (rdma_enable) {
		read_val = VSYNCOSD_RD_MPEG_REG(addr);
		write_val = read_val | _mask;
		ret = update_table_item(addr, write_val, 1);
	} else
		osd_reg_set_mask(addr, _mask);
	return ret;
}
EXPORT_SYMBOL(VSYNCOSD_IRQ_SET_MPEG_REG_MASK);

int VSYNCOSD_IRQ_CLR_MPEG_REG_MASK(u32 addr, u32 _mask)
{
	unsigned long read_val;
	unsigned long write_val;
	int ret = 0;
	if (rdma_enable) {
		read_val = VSYNCOSD_RD_MPEG_REG(addr);
		write_val = read_val & (~_mask);
		ret = update_table_item(addr, write_val, 1);
	} else
		osd_reg_clr_mask(addr, _mask);
	return ret;
}
EXPORT_SYMBOL(VSYNCOSD_IRQ_CLR_MPEG_REG_MASK);

int VSYNCOSD_EX_WR_MPEG_REG(u32 addr, u32 val)
{
	int ret = -1;
	if (rdma_enable && rdma_table_internal)
		ret = update_table_item_internal(addr, val);
	if (ret != 0) {
		ret = 0;
		if (rdma_enable)
			ret = update_table_item(addr, val, 1);
		else
			osd_reg_write(addr, val);
	}
	return ret;
}
EXPORT_SYMBOL(VSYNCOSD_EX_WR_MPEG_REG);

static int start_osd_rdma(char channel)
{
	char intr_bit = 8 * channel;
	char rw_bit = 4 + channel;
	char inc_bit = channel;
	u32 data32;
	char is_init = 1;

	osd_rdma_update_config(is_init);

	data32  = osd_reg_read(RDMA_ACCESS_AUTO);
	/*
	 * [23: 16] interrupt inputs enable mask for auto-start
	 * 1: vsync int bit 0
	 */
	data32 |= 0x1 << intr_bit;
	/* [    6] ctrl_cbus_write_1. 1=Register write; 0=Register read. */
	data32 |= 1 << rw_bit;
	/*
	 * [    2] ctrl_cbus_addr_incr_1.
	 * 1=Incremental register access; 0=Non-incremental.
	 */
	data32 &= ~(1 << inc_bit);
	osd_reg_write(RDMA_ACCESS_AUTO, data32);
	return 1;
}

static int stop_rdma(char channel)
{
	char intr_bit = 8 * channel;
	u32 data32 = 0x0;

	data32  = osd_reg_read(RDMA_ACCESS_AUTO);
	data32 &= ~(0x1 <<
		    intr_bit);
	/* [23: 16] interrupt inputs enable mask
	for auto-start 1: vsync int bit 0*/
	osd_reg_write(RDMA_ACCESS_AUTO, data32);
	return 0;
}


void osd_rdma_interrupt_done_clear(void)
{
	if (rdma_reset_tigger_flag) {
		pr_info("osd rdma restart!\n");
		rdma_reset_tigger_flag = 0;
		osd_rdma_enable(0);
		osd_rdma_enable(1);
	}
}
int read_rdma_table(void)
{
	int rdma_count = 0;
	int i, reg;

	if (rdma_debug) {
		for (rdma_count = 0; rdma_count < item_count + 1; rdma_count++)
			pr_info("rdma_table addr is 0x%x, value is 0x%x\n",
				rdma_table[rdma_count].addr,
				rdma_table[rdma_count].val);
		reg = 0x1100;
		pr_info("RDMA relative registers-----------------\n");
		for (i = 0 ; i < 24 ; i++)
			pr_info("[0x%x]== 0x%x\n", reg+i, osd_reg_read(reg+i));
	}
	return 0;
}
EXPORT_SYMBOL(read_rdma_table);

int osd_rdma_enable(u32 enable)
{
	int ret = 0;
	unsigned long flags;

	if (enable == rdma_enable)
		return 0;

	ret = osd_rdma_init();
	if (ret != 0)
		return -1;

	rdma_enable = enable;
	if (enable) {
		spin_lock_irqsave(&rdma_lock, flags);
		OSD_RDMA_STATUS_CLEAR_REJECT;
		osd_reg_write(START_ADDR, table_paddr);
		osd_reg_write(END_ADDR, table_paddr - 1);
		item_count = 0;
		spin_unlock_irqrestore(&rdma_lock, flags);
		reset_rdma_table();
		start_osd_rdma(OSD_RDMA_CHANNEL_INDEX);
	} else {
		stop_rdma(OSD_RDMA_CHANNEL_INDEX);
	}

	return 1;
}
EXPORT_SYMBOL(osd_rdma_enable);

int osd_rdma_reset_and_flush(u32 reset_bit)
{
	unsigned long read_val;
	unsigned long write_val;
	unsigned long flags;
	u32 osd_backup[OSD_REG_BACKUP_COUNT];
	u32 afbc_backup[OSD_AFBC_REG_BACKUP_COUNT];
	struct rdma_table_item request_item;
	int i, ret = 0;
	spin_lock_irqsave(&rdma_lock, flags);
	i = 0;
	while ((reset_bit & 1)
		&& (i < OSD_REG_BACKUP_COUNT)) {
		osd_backup[i] = read_reg_internal(
			osd_reg_backup[i]);
		i++;
	}
	i = 0;
	while ((reset_bit & 0x80000000)
		&& (i < OSD_AFBC_REG_BACKUP_COUNT)) {
		afbc_backup[i] = read_reg_internal(
			osd_afbc_reg_backup[i]);
		i++;
	}
	read_val = read_reg_internal(VIU_SW_RESET);
	write_val = read_val | reset_bit;
	wrtie_reg_internal(VIU_SW_RESET, write_val);

	read_val = read_reg_internal(VIU_SW_RESET);
	write_val = read_val & (~reset_bit);
	wrtie_reg_internal(VIU_SW_RESET, write_val);

	i = 0;
	while ((reset_bit & 1)
		&& (i < OSD_REG_BACKUP_COUNT)) {
		wrtie_reg_internal(
			osd_reg_backup[i], osd_backup[i]);
		i++;
	}
	i = 0;
	while ((reset_bit & 0x80000000)
		&& (i < OSD_AFBC_REG_BACKUP_COUNT)) {
		if (osd_afbc_reg_backup[i]
			== OSD1_AFBCD_ENABLE)
			wrtie_reg_internal(
				osd_afbc_reg_backup[i],
				afbc_backup[i] | 0x100);
		else
			wrtie_reg_internal(
				osd_afbc_reg_backup[i],
				afbc_backup[i]);
		i++;
	}
	if (item_count + item_count_internal < 500) {
		request_item.addr = OSD_RDMA_FLAG_REG;
		request_item.val = OSD_RDMA_STATUS_MARK_TBL_DONE;
		memcpy(
			&rdma_table[item_count + item_count_internal - 1],
			&request_item, 8);
		memcpy(
			&rdma_table[item_count - 1],
			&rdma_table_internal[0],
			8 * item_count_internal);
		item_count = item_count + item_count_internal;
		osd_reg_write(END_ADDR, (table_paddr + item_count * 8 - 1));
	} else {
		pr_info("osd_rdma_reset_and_flush item overflow %d + %d\n",
			item_count, item_count_internal);
		osd_reg_write(END_ADDR, (table_paddr + item_count * 8 - 1));
		ret = -1;
	}
	if (dump_reg_trigger > 0) {
		for (i = 0; i < item_count; i++)
			pr_info("dump rdma reg[%d]:0x%x, data:0x%x\n",
				i, rdma_table[i].addr,
				rdma_table[i].val);
		dump_reg_trigger--;
	}
	spin_unlock_irqrestore(&rdma_lock, flags);
	return ret;
}
EXPORT_SYMBOL(osd_rdma_reset_and_flush);

static void osd_rdma_release(struct device *dev)
{
	kfree(dev);
	osd_rdma_dev = NULL;
	kfree(rdma_table_internal);
	rdma_table_internal = NULL;
}

static irqreturn_t osd_rdma_isr(int irq, void *dev_id)
{
	u32 rdma_status;
	rdma_status = osd_reg_read(RDMA_STATUS);
	debug_rdma_status = rdma_status;
	if (rdma_status & (1 << (24+OSD_RDMA_CHANNEL_INDEX))) {
		OSD_RDMA_STATUS_CLEAR_REJECT;
		reset_rdma_table();
		item_count_internal = 0;
		osd_update_scan_mode();
		osd_update_3d_mode();
		osd_update_vsync_hit();
		osd_hw_reset();
		rdma_irq_count++;
		{
			/*This is a memory barrier*/
			wmb();
		}
		osd_reg_write(RDMA_CTRL, 1 << (24+OSD_RDMA_CHANNEL_INDEX));
	} else
		rdma_lost_count++;
	rdma_status = osd_reg_read(RDMA_STATUS);
	if (rdma_status & 0xf7000000) {
		if (!second_rdma_irq)
			pr_info("osd rdma irq as first function call, status: 0x%x\n",
				rdma_status);
		pr_info("osd rdma miss done isr, status: 0x%x\n", rdma_status);
		osd_reg_write(RDMA_CTRL, rdma_status & 0xf7000000);
	}
	return IRQ_HANDLED;
}

static int osd_rdma_init(void)
{
	int ret = -1;
	if (osd_rdma_init_flag)
		return 0;

	osd_rdma_dev = kzalloc(sizeof(struct device), GFP_KERNEL);
	if (!osd_rdma_dev) {
		osd_log_err("osd rdma init error!\n");
		return -1;
	}
	osd_rdma_dev->release = osd_rdma_release;
	dev_set_name(osd_rdma_dev, "osd-rdma-dev");
	dev_set_drvdata(osd_rdma_dev, osd_rdma_dev);
	ret = device_register(osd_rdma_dev);
	if (ret) {
		osd_log_err("register rdma dev error\n");
		goto error2;
	}
	table_pages = dma_alloc_from_contiguous(osd_rdma_dev, 1, 4);
	osd_rdma_table_virt = dma_alloc_coherent(osd_rdma_dev, PAGE_SIZE,
					&osd_rdma_table_phy, GFP_KERNEL);

	if (!osd_rdma_table_virt) {
		osd_log_err("osd rdma dma alloc failed!\n");
		goto error2;
	}

	rdma_table_internal = kzalloc(
		RDMA_TABLE_INTERNAL_COUNT * sizeof(struct rdma_table_item),
		GFP_KERNEL);

	if (!rdma_table_internal) {
		osd_log_err("osd rdma dma alloc failed2!\n");
		goto error2;
	}
	item_count_internal = 0;
	second_rdma_irq = 0;
	dump_reg_trigger = 0;
	table_vaddr = osd_rdma_table_virt;
	table_paddr = osd_rdma_table_phy;
	osd_log_info("%s: rdma_table p=0x%x,op=0x%lx , v=0x%p\n", __func__,
		table_paddr, (long unsigned int)osd_rdma_table_phy,
		table_vaddr);
	rdma_table = (struct rdma_table_item *)table_vaddr;
	if (NULL == rdma_table) {
		osd_log_err("%s: failed to remap rmda_table_addr\n", __func__);
		goto error2;
	}

	if (rdma_mgr_irq_request) {
		second_rdma_irq = 1;
		pr_info("osd rdma request irq as second interrput function!\n");
	}
	if (request_irq(INT_RDMA, &osd_rdma_isr,
		IRQF_SHARED, "osd_rdma", (void *)"osd_rdma")) {
		osd_log_err("can't request irq for rdma\n");
		goto error2;
	}

	osd_rdma_init_flag = true;
	return 0;

error2:
	kfree(osd_rdma_dev);
	osd_rdma_dev = NULL;
	kfree(rdma_table_internal);
	rdma_table_internal = NULL;
	return -1;
}

MODULE_PARM_DESC(item_count, "\n item_count\n");
module_param(item_count, uint, 0664);

MODULE_PARM_DESC(table_paddr, "\n table_paddr\n");
module_param(table_paddr, uint, 0664);

MODULE_PARM_DESC(rdma_debug, "\n rdma_debug\n");
module_param(rdma_debug, uint, 0664);

MODULE_PARM_DESC(debug_rdma_status, "\n debug_rdma_status\n");
module_param(debug_rdma_status, uint, 0664);

MODULE_PARM_DESC(rdma_irq_count, "\n rdma_irq_count\n");
module_param(rdma_irq_count, uint, 0664);

MODULE_PARM_DESC(rdma_lost_count, "\n rdma_lost_count\n");
module_param(rdma_lost_count, uint, 0664);

MODULE_PARM_DESC(dump_reg_trigger, "\n dump_reg_trigger\n");
module_param(dump_reg_trigger, uint, 0664);

