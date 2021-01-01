// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/irqreturn.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/highmem.h>

#include <linux/cpu.h>
#include <linux/smp.h>
#include <linux/kallsyms.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>
#include <linux/amlogic/page_trace.h>
#include "ddr_port.h"
#include "dmc_monitor.h"

#define DMC_PROT0_STA			(0x00e0 << 2)
#define DMC_PROT0_EDA			(0x00e1 << 2)
#define DMC_PROT0_CTRL			(0x00e2 << 2)
#define DMC_PROT0_CTRL1			(0x00e3 << 2)

#define DMC_PROT1_STA			(0x00e4 << 2)
#define DMC_PROT1_EDA			(0x00e5 << 2)
#define DMC_PROT1_CTRL			(0x00e6 << 2)
#define DMC_PROT1_CTRL1			(0x00e7 << 2)

#define DMC_PROT_VIO_0			(0x00e8 << 2)
#define DMC_PROT_VIO_1			(0x00e9 << 2)
#define DMC_PROT_VIO_2			(0x00ea << 2)
#define DMC_PROT_VIO_3			(0x00eb << 2)

#define DMC_PROT_IRQ_CTRL_STS		(0x00ec << 2)

#define DMC_VIO_PROT_RANGE0	BIT(17)
#define DMC_VIO_PROT_RANGE1	BIT(18)

static size_t t7_dmc_dump_reg(char *buf)
{
	int sz = 0, i, j;
	unsigned long val;
	void *io;

	for (j = 0; j < 2; j++) {
		if (j)
			io = dmc_mon->io_mem2;
		else
			io = dmc_mon->io_mem1;
		sz += sprintf(buf + sz, "\nDMC%d:\n", j);

		val = dmc_prot_rw(io, DMC_PROT0_STA, 0, DMC_READ);
		sz += sprintf(buf + sz, "DMC_PROT0_STA:%lx\n", val);
		val = dmc_prot_rw(io, DMC_PROT0_EDA, 0, DMC_READ);
		sz += sprintf(buf + sz, "DMC_PROT0_EDA:%lx\n", val);
		val = dmc_prot_rw(io, DMC_PROT0_CTRL, 0, DMC_READ);
		sz += sprintf(buf + sz, "DMC_PROT0_CTRL:%lx\n", val);
		val = dmc_prot_rw(io, DMC_PROT0_CTRL1, 0, DMC_READ);
		sz += sprintf(buf + sz, "DMC_PROT0_CTRL1:%lx\n", val);

		val = dmc_prot_rw(io, DMC_PROT1_STA, 0, DMC_READ);
		sz += sprintf(buf + sz, "DMC_PROT1_STA:%lx\n", val);
		val = dmc_prot_rw(io, DMC_PROT1_EDA, 0, DMC_READ);
		sz += sprintf(buf + sz, "DMC_PROT1_EDA:%lx\n", val);
		val = dmc_prot_rw(io, DMC_PROT1_CTRL, 0, DMC_READ);
		sz += sprintf(buf + sz, "DMC_PROT1_CTRL:%lx\n", val);
		val = dmc_prot_rw(io, DMC_PROT1_CTRL1, 0, DMC_READ);
		sz += sprintf(buf + sz, "DMC_PROT1_CTRL1:%lx\n", val);

		val = dmc_prot_rw(io, DMC_PROT_IRQ_CTRL_STS, 0, DMC_READ);
		sz += sprintf(buf + sz, "DMC_PROT_IRQ_CTRL_STS:%lx\n", val);

		for (i = 0; i < 4; i++) {
			val = dmc_prot_rw(io, DMC_PROT_VIO_0 + (i << 2), 0, DMC_READ);
			sz += sprintf(buf + sz, "DMC_PROT_VIO_%d:%lx\n", i, val);
		}
	}
	return sz;
}

static void check_violation(struct dmc_monitor *mon, void *io)
{
	int port;
	unsigned long addr;
	unsigned long status;
	struct page *page;
	struct page_trace *trace;

	status = dmc_prot_rw(io, DMC_PROT_VIO_1, 0, DMC_READ);
	if (!(status & (DMC_VIO_PROT_RANGE0 | DMC_VIO_PROT_RANGE1)))
		return;

	/* combine address */
	addr  = (status >> 15) & 0x03;
	addr  = (addr << 32ULL);
	addr |= dmc_prot_rw(io, DMC_PROT_VIO_0, 0, DMC_READ);
	if (addr > mon->addr_end)
		return;

	/* ignore violation on same page/same port */
	if ((addr & PAGE_MASK) == mon->last_addr &&
	    status == mon->last_status) {
		mon->same_page++;
		return;
	}
	/* ignore cma driver pages */
	page = phys_to_page(addr);
	trace = find_page_base(page);
	if (trace && trace->migrate_type == MIGRATE_CMA)
		return;

	port = status & 0xff;
	pr_emerg(DMC_TAG ", addr:%08lx, s:%08lx, ID:%s, c:%ld, d:%p\n",
		 addr, status, to_ports(port),
		 mon->same_page, io);
	show_violation_mem(addr);
	mon->same_page   = 0;
	mon->last_addr   = addr & PAGE_MASK;
	mon->last_status = status;
}

static void __t7_dmc_mon_irq(struct dmc_monitor *mon, void *io)
{
	unsigned long value;

	value = dmc_prot_rw(io, DMC_PROT_IRQ_CTRL_STS, 0, DMC_READ);
	if (in_interrupt()) {
		if (value & DMC_WRITE_VIOLATION)
			check_violation(mon, io);

		/* check irq flags just after IRQ handler */
		mod_delayed_work(system_wq, &mon->work, 0);
	}
	/* clear irq */
	dmc_prot_rw(io, DMC_PROT_IRQ_CTRL_STS, value, DMC_WRITE);
}

static void t7_dmc_mon_irq(struct dmc_monitor *mon, void *io)
{
	int i;

	if (io) {
		__t7_dmc_mon_irq(mon, io);
	} else {
		for (i = 0; i < 2; i++) {
			io = i ? dmc_mon->io_mem2 : dmc_mon->io_mem1;
			__t7_dmc_mon_irq(mon, io);
		}
	}
}

static int t7_dmc_mon_set(struct dmc_monitor *mon)
{
	unsigned long start, end, val;
	void *io;
	int i;
	unsigned int dev1, dev2;

	if (!dmc_mon->device)
		return 0;

	start = mon->addr_start >> PAGE_SHIFT;
	end   = ALIGN(mon->addr_end, PAGE_SIZE);
	end   = end >> PAGE_SHIFT;
	dev1  = mon->device & 0xffffffff;
	dev2  = mon->device >> 32;
	for (i = 0; i < 2; i++) {
		io = i ? dmc_mon->io_mem2 : dmc_mon->io_mem1;
		dmc_prot_rw(io, DMC_PROT0_STA, start, DMC_WRITE);
		dmc_prot_rw(io, DMC_PROT0_EDA, end, DMC_WRITE);
		dmc_prot_rw(io, DMC_PROT1_STA, start, DMC_WRITE);
		dmc_prot_rw(io, DMC_PROT1_EDA, end, DMC_WRITE);
		val = (1 << 27) | 0xf;
		if (dmc_mon->configs & POLICY_INCLUDE)
			val |= (1 << 8);
		else
			val &= ~(1 << 8);

		dmc_prot_rw(io, DMC_PROT0_CTRL,  val,  DMC_WRITE);
		dmc_prot_rw(io, DMC_PROT0_CTRL1, dev1, DMC_WRITE);
		dmc_prot_rw(io, DMC_PROT1_CTRL,  val,  DMC_WRITE);
		dmc_prot_rw(io, DMC_PROT1_CTRL1, dev2, DMC_WRITE);
	}

	pr_emerg("range:%08lx - %08lx, device:%16llx\n",
		 mon->addr_start, mon->addr_end, mon->device);
	return 0;
}

void t7_dmc_mon_disable(struct dmc_monitor *mon)
{
	void *io;
	int i;

	for (i = 0; i < 2; i++) {
		io = i ? dmc_mon->io_mem2 : dmc_mon->io_mem1;
		dmc_prot_rw(io, DMC_PROT0_STA,   0, DMC_WRITE);
		dmc_prot_rw(io, DMC_PROT0_EDA,   0, DMC_WRITE);
		dmc_prot_rw(io, DMC_PROT1_STA,   0, DMC_WRITE);
		dmc_prot_rw(io, DMC_PROT1_EDA,   0, DMC_WRITE);
		dmc_prot_rw(io, DMC_PROT0_CTRL,  0, DMC_WRITE);
		dmc_prot_rw(io, DMC_PROT0_CTRL1, 0, DMC_WRITE);
		dmc_prot_rw(io, DMC_PROT1_CTRL,  0, DMC_WRITE);
		dmc_prot_rw(io, DMC_PROT1_CTRL1, 0, DMC_WRITE);
	}
	mon->addr_start = 0;
	mon->addr_end   = 0;
	mon->device     = 0;
}

struct dmc_mon_ops t7_dmc_mon_ops = {
	.handle_irq = t7_dmc_mon_irq,
	.set_montor = t7_dmc_mon_set,
	.disable    = t7_dmc_mon_disable,
	.dump_reg   = t7_dmc_dump_reg,
};
