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

#define DMC_PROT0_RANGE		((0x00a0  << 2))
#define DMC_PROT0_CTRL		((0x00a1  << 2))
#define DMC_PROT1_RANGE		((0x00a2  << 2))
#define DMC_PROT1_CTRL		((0x00a3  << 2))
#define DMC_SEC_STATUS		((0x00b6  << 2))
#define DMC_VIO_ADDR0		((0x00b7  << 2))
#define DMC_VIO_ADDR1		((0x00b8  << 2))
#define DMC_VIO_ADDR2		((0x00b9  << 2))
#define DMC_VIO_ADDR3		((0x00ba  << 2))
#define DMC_VIO_ADDR4		((0x00bb  << 2))
#define DMC_VIO_ADDR5		((0x00bc  << 2))
#define DMC_VIO_ADDR6		((0x00bd  << 2))
#define DMC_VIO_ADDR7		((0x00be  << 2))

#define DMC_VIO_PROT_RANGE0	BIT(20)
#define DMC_VIO_PROT_RANGE1	BIT(21)

static size_t gx_dmc_dump_reg(char *buf)
{
	size_t sz = 0, i;
	unsigned long val;

	val = dmc_prot_rw(NULL, DMC_PROT0_RANGE, 0, DMC_READ);
	sz += sprintf(buf + sz, "DMC_PROT0_RANGE:%lx\n", val);
	val = dmc_prot_rw(NULL, DMC_PROT0_CTRL, 0, DMC_READ);
	sz += sprintf(buf + sz, "DMC_PROT0_CTRL:%lx\n", val);
	val = dmc_prot_rw(NULL, DMC_PROT1_RANGE, 0, DMC_READ);
	sz += sprintf(buf + sz, "DMC_PROT1_RANGE:%lx\n", val);
	val = dmc_prot_rw(NULL, DMC_PROT1_CTRL, 0, DMC_READ);
	sz += sprintf(buf + sz, "DMC_PROT1_CTRL:%lx\n", val);
	val = dmc_prot_rw(NULL, DMC_SEC_STATUS, 0, DMC_READ);
	sz += sprintf(buf + sz, "DMC_SEC_STATUS:%lx\n", val);
	for (i = 0; i < 8; i++) {
		val = dmc_prot_rw(NULL, DMC_VIO_ADDR0 + (i << 2), 0, DMC_READ);
		sz += sprintf(buf + sz, "DMC_VIO_ADDR%zu:%lx\n", i, val);
	}

	return sz;
}

static void check_violation(struct dmc_monitor *mon, void *data)
{
	int i, port, subport;
	unsigned long addr, status;
	char id_str[MAX_NAME];
	char title[10] = "";
	struct page *page;
	struct page_trace *trace;

	for (i = 1; i < 8; i += 2) {
		status = dmc_prot_rw(NULL, DMC_VIO_ADDR0 + (i << 2), 0, DMC_READ);
		if (!(status & DMC_VIO_PROT_RANGE0))
			continue;
		addr = dmc_prot_rw(NULL, DMC_VIO_ADDR0 + ((i - 1) << 2), 0,
				   DMC_READ);
		if (addr > mon->addr_end)
			continue;

		/* ignore violation on same page/same port */
		if ((addr & PAGE_MASK) == mon->last_addr &&
		    status == mon->last_status) {
			mon->same_page++;
			if (mon->debug & DMC_DEBUG_CMA)
				sprintf(title, "%s", "_SAME");
			else
				continue;
		}
		/* ignore cma driver pages */
		page = phys_to_page(addr);
		trace = find_page_base(page);
		if (trace && trace->migrate_type == MIGRATE_CMA) {
			if (mon->debug & DMC_DEBUG_CMA)
				sprintf(title, "%s", "_CMA");
			else
				continue;
		}

		port = (status >> 10) & 0xf;
		subport = (status >> 6) & 0xf;

		pr_emerg(DMC_TAG "%s, addr:%08lx, s:%08lx, ID:%s, sub:%s, c:%ld, d:%p\n",
			 title, addr, status, to_ports(port),
			 to_sub_ports(port, subport, id_str),
			 mon->same_page, data);
		show_violation_mem(addr);
		if (!port) /* dump stack for CPU write */
			dump_stack();

		mon->same_page   = 0;
		mon->last_addr   = addr & PAGE_MASK;
		mon->last_status = status;
	}
}

static void gx_dmc_mon_irq(struct dmc_monitor *mon, void *data)
{
	unsigned long value;

	value = dmc_prot_rw(NULL, DMC_SEC_STATUS, 0, DMC_READ);
	if (in_interrupt()) {
		if (value & DMC_WRITE_VIOLATION)
			check_violation(mon, data);

		/* check irq flags just after IRQ handler */
		mod_delayed_work(system_wq, &mon->work, 0);
	}
	/* clear irq */
	dmc_prot_rw(NULL, DMC_SEC_STATUS, value, DMC_WRITE);
}

static int gx_dmc_mon_set(struct dmc_monitor *mon)
{
	unsigned long value, end;

	/* aligned to 64KB */
	end = ALIGN(mon->addr_end, DMC_ADDR_SIZE);
	value = (mon->addr_start >> 16) | ((end >> 16) << 16);
	dmc_prot_rw(NULL, DMC_PROT0_RANGE, value, DMC_WRITE);

	value = (1 << 19) | mon->device;
	dmc_prot_rw(NULL, DMC_PROT0_CTRL, value, DMC_WRITE);

	pr_emerg("range:%08lx - %08lx, device:%llx\n",
		 mon->addr_start, mon->addr_end, mon->device);
	return 0;
}

void gx_dmc_mon_disable(struct dmc_monitor *mon)
{
	dmc_prot_rw(NULL, DMC_PROT0_RANGE, 0, DMC_WRITE);
	dmc_prot_rw(NULL, DMC_PROT0_CTRL, 0, DMC_WRITE);
	mon->device     = 0;
	mon->addr_start = 0;
	mon->addr_end   = 0;
}

struct dmc_mon_ops gx_dmc_mon_ops = {
	.handle_irq = gx_dmc_mon_irq,
	.set_montor = gx_dmc_mon_set,
	.disable    = gx_dmc_mon_disable,
	.dump_reg   = gx_dmc_dump_reg,
};
