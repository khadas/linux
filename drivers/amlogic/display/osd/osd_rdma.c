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
#include "osd.h"
#include "osd_io.h"
#include "osd_reg.h"
#include "osd_rdma.h"

static DEFINE_SPINLOCK(rdma_lock);
static struct rdma_table_item_t *rdma_table;
static struct device *osd_rdma_dev;
static struct page *table_pages;
static void *osd_rdma_table_virt;
static dma_addr_t osd_rdma_table_phy;
static u32 table_paddr;
static void *table_vaddr;
static u32 rdma_enable;
static u32 item_count;
static u32 rdma_debug;
#define OSD_RDMA_UPDATE_RETRY_COUNT 100
static bool osd_rdma_init_flag;
static int ctrl_ahb_rd_burst_size = 3;
static int ctrl_ahb_wr_burst_size = 3;

static int osd_rdma_init(void);

static inline void reset_rdma_table(void)
{
	unsigned long flags;

	spin_lock_irqsave(&rdma_lock, flags);
	osd_reg_write(END_ADDR, table_paddr-1);
	rdma_table[0].addr = OSD_RDMA_FLAG_REG;
	rdma_table[0].val = OSD_RDMA_STATUS_MARK_TBL_RST;
	rdma_table[1].addr = OSD_RDMA_FLAG_REG;
	rdma_table[1].val = OSD_RDMA_STATUS_MARK_COMPLETE;
	item_count = 1;
	spin_unlock_irqrestore(&rdma_lock, flags);
}



static int update_table_item(u32 addr, u32 val)
{
	unsigned long flags;
	int retry_count = OSD_RDMA_UPDATE_RETRY_COUNT;

retry:
	/*in reject region then we wait for hw rdma operation complete.*/
	if (OSD_RDMA_STATUS_IS_REJECT && (retry_count > 0)) {
		retry_count--;
		osd_log_err("%s retry: %d", __func__, retry_count);
		goto retry;
	}
	if (!OSD_RDMA_STAUS_IS_DIRTY) {
		/*since last HW op,no new wirte request.
		rdma HW op will clear DIRTY flag.*/
		/*reset all pointer. set table start margin.*/
		reset_rdma_table();
	}
	/*atom_lock_start:*/
	/*set write op aotmic lock flag.*/
	OSD_RDMA_STAUS_MARK_DIRTY;
	spin_lock_irqsave(&rdma_lock, flags);
	item_count++;
	rdma_table[item_count].addr = OSD_RDMA_FLAG_REG;
	rdma_table[item_count].val = OSD_RDMA_STATUS_MARK_COMPLETE;
	osd_reg_write(END_ADDR, (table_paddr + item_count * 8 + 7));
	rdma_table[item_count - 1].addr = addr;
	rdma_table[item_count - 1].val = val;
	spin_unlock_irqrestore(&rdma_lock, flags);
	/*if dirty flag is cleared, then RDMA hw write and cpu
	sw write is racing.if reject flag is true,then hw RDMA hw write
	start when cpu write.*/
	/*atom_lock_end:*/
	if (!OSD_RDMA_STAUS_IS_DIRTY || OSD_RDMA_STATUS_IS_REJECT) {
		spin_lock_irqsave(&rdma_lock, flags);
		item_count--;
		spin_unlock_irqrestore(&rdma_lock, flags);
		goto retry;
	}
	read_rdma_table();
	return 0;
}
u32 VSYNCOSD_RD_MPEG_REG(u32 addr)
{
	int  i;
	if (rdma_enable) {
		for (i = (item_count - 1); i >= 0; i--) {
			if (addr == rdma_table[i].addr)
				return rdma_table[i].val;
		}
	}
	return osd_reg_read(addr);
}
EXPORT_SYMBOL(VSYNCOSD_RD_MPEG_REG);

int VSYNCOSD_WR_MPEG_REG(u32 addr, u32 val)
{
	if (rdma_enable)
		update_table_item(addr, val);
	else
		osd_reg_write(addr, val);
	return 0;
}
EXPORT_SYMBOL(VSYNCOSD_WR_MPEG_REG);

int VSYNCOSD_WR_MPEG_REG_BITS(u32 addr, u32 val, u32 start, u32 len)
{
	unsigned long read_val;
	unsigned long write_val;
	if (rdma_enable) {
		read_val = VSYNCOSD_RD_MPEG_REG(addr);
		write_val = (read_val & ~(((1L << (len)) - 1) << (start)))
			    | ((unsigned int)(val) << (start));
		update_table_item(addr, write_val);
	} else
		osd_reg_set_bits(addr, val, start, len);
	return 0;
}
EXPORT_SYMBOL(VSYNCOSD_WR_MPEG_REG_BITS);

int VSYNCOSD_SET_MPEG_REG_MASK(u32 addr, u32 _mask)
{
	unsigned long read_val;
	unsigned long write_val;
	if (rdma_enable) {
		read_val = VSYNCOSD_RD_MPEG_REG(addr);
		write_val = read_val | _mask;
		update_table_item(addr, write_val);
	} else
		osd_reg_set_mask(addr, _mask);
	return 0;
}
EXPORT_SYMBOL(VSYNCOSD_SET_MPEG_REG_MASK);

int VSYNCOSD_CLR_MPEG_REG_MASK(u32 addr, u32 _mask)
{
	unsigned long read_val;
	unsigned long write_val;
	if (rdma_enable) {
		read_val = VSYNCOSD_RD_MPEG_REG(addr);
		write_val = read_val & (~_mask);
		update_table_item(addr, write_val);
	} else
		osd_reg_clr_mask(addr, _mask);
	return 0;
}
EXPORT_SYMBOL(VSYNCOSD_CLR_MPEG_REG_MASK);

static int start_osd_rdma(char channel)
{
	char intr_bit = 8 * channel;
	char rw_bit = 4 + channel;
	char inc_bit = channel;
	u32 data32;
	data32  = 0;
	data32 |= 0 << 6; /* [31: 6] Rsrv. */
	/* [ 5: 4] ctrl_ahb_wr_burst_size. 0=16; 1=24; 2=32; 3=48. */
	/* [ 5: 4] ctrl_ahb_wr_burst_size. 0=16; 1=32; 2=48; 3=64. */
	data32 |= ctrl_ahb_wr_burst_size << 4;
	/* [ 3: 2] ctrl_ahb_rd_burst_size. 0=16; 1=24; 2=32; 3=48. */
	/* [ 3: 2] ctrl_ahb_rd_burst_size. 0=16; 1=32; 2=48; 3=64. */
	data32 |= ctrl_ahb_rd_burst_size << 2;
	data32 |= 0 << 1;   /* [    1] ctrl_sw_reset. */
	data32 |= 0 << 0;   /* [    0] ctrl_free_clk_enable. */
	osd_reg_write(RDMA_CTRL, data32);
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

int reset_rdma(void)
{
	/*reset mechanism , to clear rdma status.*/
	if (OSD_RDMA_STAUS_IS_DONE) { /*check if it is OSD rdma completed.*/
		OSD_RDMA_STAUS_CLEAR_DONE;
		/*check if no cpu write request since the latest hw rdma op.*/
		if (!OSD_RDMA_STAUS_IS_DIRTY) {
			/*since last HW op,no new wirte request.
			rdma HW op will clear DIRTY flag.*/
			/*reset all pointer. set table start margin.*/
			pr_debug("reset from isr\n");
			reset_rdma_table();
		}
	}
	return 0;
}
EXPORT_SYMBOL(reset_rdma);

int osd_rdma_enable(u32 enable)
{
	int ret = 0;

	if (enable == rdma_enable)
		return 0;

	ret = osd_rdma_init();
	if (ret != 0)
		return -1;

	rdma_enable = enable;
	if (enable) {
		reset_rdma_table();
		osd_reg_write(START_ADDR, table_paddr);
		OSD_RDMA_STATUS_CLEAR_ALL;
		start_osd_rdma(OSD_RDMA_CHANNEL_INDEX);
	} else
		stop_rdma(OSD_RDMA_CHANNEL_INDEX);

	return 0;
}
EXPORT_SYMBOL(osd_rdma_enable);

static void osd_rdma_release(struct device *dev)
{
	kfree(dev);
	osd_rdma_dev = NULL;
}

static irqreturn_t osd_rdma_isr(int irq, void *dev_id)
{
	reset_rdma();

	osd_update_scan_mode();
	osd_update_3d_mode();
	osd_update_vsync_hit();

	osd_reg_write(RDMA_CTRL, 1 << (24+OSD_RDMA_CHANNEL_INDEX));

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
	table_vaddr = osd_rdma_table_virt;
	table_paddr = osd_rdma_table_phy;
	osd_log_info("%s: rmda_table p=0x%x,op=0x%lx , v=0x%p\n", __func__,
			table_paddr, (long unsigned int)osd_rdma_table_phy,
		     table_vaddr);
	rdma_table = (struct rdma_table_item_t *)table_vaddr;
	if (NULL == rdma_table) {
		osd_log_err("%s: failed to remap rmda_table_addr\n", __func__);
		goto error2;
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
	return -1;
}

MODULE_PARM_DESC(item_count, "\n item_count\n");
module_param(item_count, uint, 0664);

MODULE_PARM_DESC(table_paddr, "\n table_paddr\n");
module_param(table_paddr, uint, 0664);

MODULE_PARM_DESC(rdma_debug, "\n rdma_debug\n");
module_param(rdma_debug, uint, 0664);
