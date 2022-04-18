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

#define DMC_PROT0_STA		((0x00a3  << 2))
#define DMC_PROT0_EDA		((0x00a4  << 2))
#define DMC_PROT0_CTRL		((0x00a5  << 2))
#define DMC_PROT0_CTRL1		((0x00a6  << 2))

#define DMC_PROT1_STA		((0x00a7  << 2))
#define DMC_PROT1_EDA		((0x00a8  << 2))
#define DMC_PROT1_CTRL		((0x00a9  << 2))
#define DMC_PROT1_CTRL1		((0x00aa  << 2))

#define DMC_SEC_STATUS		((0x00b8  << 2))
#define DMC_VIO_ADDR0		((0x00b9  << 2))
#define DMC_VIO_ADDR1		((0x00ba  << 2))

#define DMC_VIO_ADDR2		((0x00bb  << 2))
#define DMC_VIO_ADDR3		((0x00bc  << 2))

#define PROT1_VIOLATION		BIT(23)
#define PROT0_VIOLATION		BIT(22)

static size_t c1_dmc_dump_reg(char *buf)
{
	size_t sz = 0, i;
	unsigned long val;

	val = dmc_prot_rw(NULL, DMC_PROT0_STA, 0, DMC_READ);
	sz += sprintf(buf + sz, "DMC_PROT0_STA:%lx\n", val);
	val = dmc_prot_rw(NULL, DMC_PROT0_EDA, 0, DMC_READ);
	sz += sprintf(buf + sz, "DMC_PROT0_EDA:%lx\n", val);
	val = dmc_prot_rw(NULL, DMC_PROT0_CTRL, 0, DMC_READ);
	sz += sprintf(buf + sz, "DMC_PROT0_CTRL:%lx\n", val);
	val = dmc_prot_rw(NULL, DMC_PROT0_CTRL1, 0, DMC_READ);
	sz += sprintf(buf + sz, "DMC_PROT0_CTRL1:%lx\n", val);

	val = dmc_prot_rw(NULL, DMC_PROT1_STA, 0, DMC_READ);
	sz += sprintf(buf + sz, "DMC_PROT1_STA:%lx\n", val);
	val = dmc_prot_rw(NULL, DMC_PROT1_EDA, 0, DMC_READ);
	sz += sprintf(buf + sz, "DMC_PROT1_EDA:%lx\n", val);
	val = dmc_prot_rw(NULL, DMC_PROT1_CTRL, 0, DMC_READ);
	sz += sprintf(buf + sz, "DMC_PROT1_CTRL:%lx\n", val);
	val = dmc_prot_rw(NULL, DMC_PROT1_CTRL1, 0, DMC_READ);
	sz += sprintf(buf + sz, "DMC_PROT1_CTRL1:%lx\n", val);

	val = dmc_prot_rw(NULL, DMC_SEC_STATUS, 0, DMC_READ);
	sz += sprintf(buf + sz, "DMC_SEC_STATUS:%lx\n", val);
	for (i = 0; i < 4; i++) {
		val = dmc_prot_rw(NULL, DMC_VIO_ADDR0 + (i << 2), 0, DMC_READ);
		sz += sprintf(buf + sz, "DMC_VIO_ADDR%zu:%lx\n", i, val);
	}

	return sz;
}

static void check_violation(struct dmc_monitor *mon, void *data)
{
	int i, port, subport;
	unsigned long addr, status;
	struct page *page;
	struct page_trace *trace;
	char title[10] = "";

	for (i = 1; i < 4; i += 2) {
		status = dmc_prot_rw(NULL, DMC_VIO_ADDR0 + (i << 2), 0, DMC_READ);
		if (!(status & PROT0_VIOLATION))
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

		port = status & 0x1f;
		subport = (status >> 9) & 0x7;
		if (port == 16)
			port = subport + 9;
		pr_emerg(DMC_TAG "%s, addr:%08lx, s:%08lx, ID:%s, sub:%d, c:%ld, d:%p\n",
			 title, addr, status, to_ports(port), subport,
			 mon->same_page, data);
		show_violation_mem(addr);

		mon->same_page   = 0;
		mon->last_addr   = addr & PAGE_MASK;
		mon->last_status = status;
	}
}

static void c1_dmc_mon_irq(struct dmc_monitor *mon, void *data)
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

static int c1_dmc_mon_set(struct dmc_monitor *mon)
{
	unsigned long add;

	add = mon->addr_start & PAGE_MASK;
	dmc_prot_rw(NULL, DMC_PROT0_STA, add, DMC_WRITE);
	add = mon->addr_end & PAGE_MASK;
	dmc_prot_rw(NULL, DMC_PROT0_EDA, add, DMC_WRITE);

	dmc_prot_rw(NULL, DMC_PROT0_CTRL, mon->device, DMC_WRITE);
	dmc_prot_rw(NULL, DMC_PROT0_CTRL1, 1 << 24, DMC_WRITE);

	pr_emerg("range:%08lx - %08lx, device:%llx\n",
		 mon->addr_start, mon->addr_end, mon->device);
	return 0;
}

void c1_dmc_mon_disable(struct dmc_monitor *mon)
{
	dmc_prot_rw(NULL, DMC_PROT0_STA,   0, DMC_WRITE);
	dmc_prot_rw(NULL, DMC_PROT0_EDA,   0, DMC_WRITE);
	dmc_prot_rw(NULL, DMC_PROT0_CTRL,  0, DMC_WRITE);
	dmc_prot_rw(NULL, DMC_PROT0_CTRL1, 0, DMC_WRITE);

	mon->device     = 0;
	mon->addr_start = 0;
	mon->addr_end   = 0;
}

struct dmc_mon_ops c1_dmc_mon_ops = {
	.handle_irq = c1_dmc_mon_irq,
	.set_montor = c1_dmc_mon_set,
	.disable    = c1_dmc_mon_disable,
	.dump_reg   = c1_dmc_dump_reg,
};
