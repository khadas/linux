/*
 * Amlogic OSD RDMA driver
 *
 * Copyright (C) 2015 Amlogic, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Author: Amlogic Platform-BJ <platform.bj@amlogic.com>
 *
 */

/* Linux Headers */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/string.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/ctype.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/clk.h>
#include <linux/slab.h>
#include <linux/dma-contiguous.h>
#include <linux/dma-mapping.h>

/* Amlogic Headers */
#include <linux/amlogic/vout/vout_notify.h>

/* Local Headers */
#include "osd_reg.h"
#include "osd_io.h"
#include "osd_log.h"
#include "osd_rdma.h"

struct rdma_table_item_s {
	u32 addr;
	u32 val;
};

#define TABLE_SIZE PAGE_SIZE
#define MAX_TABLE_ITEM (TABLE_SIZE/sizeof(struct rdma_table_item_s))
#define RDMA_CHANNEL_INDEX 3  /* auto  1,2,3   manual is 0 */
#define START_ADDR (RDMA_AHB_START_ADDR_MAN+(RDMA_CHANNEL_INDEX<<3))
#define END_ADDR (RDMA_AHB_END_ADDR_MAN+(RDMA_CHANNEL_INDEX<<3))

#define Wr(adr, val) osd_reg_write(adr, val)
#define Rd(adr) osd_reg_read(adr)
#define Wr_reg_bits(adr, val, start, len) osd_reg_set_bits(adr, val, start, len)
#define Wr_set_reg_bits_mask(adr, _mask) osd_reg_set_mask(adr, _mask)
#define Wr_clr_reg_bits_mask(adr, _mask) osd_reg_clr_mask(adr, _mask)


static struct rdma_table_item_s *rdma_table;
static struct device *osd_rdma_dev;
static struct page *table_pages;
static u32 table_paddr;
static void *table_vaddr;
static u32 rdma_enable;
static u32 item_count;
static u32 rdma_debug;

static bool osd_rdma_init_flag;
static int ctrl_ahb_rd_burst_size = 3;
static int ctrl_ahb_wr_burst_size = 3;

static int osd_rdma_init(void);

static int update_table_item(u32 addr, u32 val)
{
	if (item_count > (MAX_TABLE_ITEM - 1))
		return -1;
	/* new comer,then add it . */
	rdma_table[item_count].addr = addr;
	rdma_table[item_count].val = val;
	item_count++;
	osd_reg_write(END_ADDR, (table_paddr + item_count * 8 - 1));
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
	return Rd(addr);
}
EXPORT_SYMBOL(VSYNCOSD_RD_MPEG_REG);

int VSYNCOSD_WR_MPEG_REG(u32 addr, u32 val)
{
	if (rdma_enable)
		update_table_item(addr, val);
	else
		Wr(addr, val);
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
		Wr_reg_bits(addr, val, start, len);
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
		Wr_set_reg_bits_mask(addr, _mask);
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
		Wr_clr_reg_bits_mask(addr, _mask);
	return 0;
}
EXPORT_SYMBOL(VSYNCOSD_CLR_MPEG_REG_MASK);

static int osd_rdma_start(char channel)
{
	char intr_bit = 8 * channel;
	char rw_bit = 4 + channel;
	char inc_bit = channel;
	u32 data32;
	data32  = 0;
	data32 |= 0 << 6; /* [31: 6] Rsrv. */
	/* [ 5: 4] ctrl_ahb_wr_burst_size. 0=16; 1=24; 2=32; 3=48. */
	data32 |= ctrl_ahb_wr_burst_size << 4;
	/* [ 3: 2] ctrl_ahb_rd_burst_size. 0=16; 1=24; 2=32; 3=48. */
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
	/*
	 * [23: 16] interrupt inputs enable mask for auto-start
	 * 1: vsync int bit 0
	 */
	data32 &= ~(0x1 << intr_bit);
	osd_reg_write(RDMA_ACCESS_AUTO, data32);
	return 0;
}

int osd_rdma_read_table(void)
{
	int rdma_count = 0;
	if (rdma_debug) {
		for (rdma_count = 0; rdma_count < item_count; rdma_count++)
			osd_log_info("rdma_table addr is 0x%x, value is 0x%x\n",
				rdma_table[rdma_count].addr,
				rdma_table[rdma_count].val);
	}
	return 0;
}
EXPORT_SYMBOL(osd_rdma_read_table);

int osd_rdma_reset(void)
{
	item_count = 0;
	memset(rdma_table, 0x0, TABLE_SIZE);
	osd_reg_write(END_ADDR, (table_paddr + item_count * 8 - 1));
	return 0;
}
EXPORT_SYMBOL(osd_rdma_reset);

int osd_rdma_enable(u32 enable)
{
	int ret = 0;
	if (!osd_rdma_init_flag) {
		ret = osd_rdma_init();
		if (ret) {
			osd_log_err("osd rdma init failed.\n");
			return -1;
		}
	}
	if (enable == rdma_enable)
		return 0;
	rdma_enable = enable;
	if (enable) {
		osd_reg_write(START_ADDR, table_paddr);
		/* enable then start it. */
		osd_rdma_reset();
		osd_rdma_start(RDMA_CHANNEL_INDEX);
	} else
		stop_rdma(RDMA_CHANNEL_INDEX);
	return 1;
}
EXPORT_SYMBOL(osd_rdma_enable);

static void osd_rdma_release(struct device *dev)
{
	kfree(dev);
	osd_rdma_dev = NULL;
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
	if (!table_pages) {
		osd_log_err("osd rdma dma alloc failed!\n");
		goto error2;
	}
	table_paddr = page_to_phys(table_pages);
	table_vaddr = phys_to_virt(table_paddr);
	osd_log_info("%s: rmda_table p=0x%x, v=0x%p\n", __func__, table_paddr,
		     table_vaddr);
	rdma_table = (struct rdma_table_item_s *)table_vaddr;
	if (NULL == rdma_table) {
		osd_log_err("%s: failed to remap rmda_table_addr\n", __func__);
		return -1;
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
