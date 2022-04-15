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

#include <linux/cpu.h>
#include <linux/smp.h>
#include <linux/kallsyms.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>
#include <linux/amlogic/page_trace.h>
#include "ddr_port.h"
#include "dmc_monitor.h"

#define DMC_PROT0_RANGE		((0x00d0  << 2))
#define DMC_PROT0_CTRL		((0x00d1  << 2))
#define DMC_PROT0_CTRL1		((0x00d2  << 2))

#define DMC_PROT1_RANGE		((0x00d3  << 2))
#define DMC_PROT1_CTRL		((0x00d4  << 2))
#define DMC_PROT1_CTRL1		((0x00d5  << 2))

#define DMC_PROT_VIO_0		((0x00d6  << 2))
#define DMC_PROT_VIO_1		((0x00d7  << 2))

#define DMC_PROT_VIO_2		((0x00d8  << 2))
#define DMC_PROT_VIO_3		((0x00d9  << 2))

#define DMC_PROT_IRQ_CTRL	((0x00da  << 2))
#define DMC_IRQ_STS		((0x00cf  << 2))

#define DMC_VIO_PROT_RANGE0     BIT(19)
#define DMC_VIO_PROT_RANGE1     BIT(20)

#define DMC_VIO_PROT_RANGE0_T5W     BIT(20)
#define DMC_VIO_PROT_RANGE1_T5W     BIT(21)

#define DMC_VIO_PROT_RANGE0_A5     BIT(20)
#define DMC_VIO_PROT_RANGE1_A5     BIT(21)

static size_t s4_dmc_dump_reg(char *buf)
{
	size_t sz = 0, i;
	unsigned long val;

	for (i = 0; i < 2; i++) {
		val = dmc_prot_rw(dmc_mon->io_mem1, DMC_PROT0_RANGE + (i * 12), 0, DMC_READ);
		sz += sprintf(buf + sz, "DMC_PROT%zu_RANGE:%lx\n", i, val);
		val = dmc_prot_rw(dmc_mon->io_mem1, DMC_PROT0_CTRL + (i * 12), 0, DMC_READ);
		sz += sprintf(buf + sz, "DMC_PROT%zu_CTRL:%lx\n", i, val);
		val = dmc_prot_rw(dmc_mon->io_mem1, DMC_PROT0_CTRL1 + (i * 12), 0, DMC_READ);
		sz += sprintf(buf + sz, "DMC_PROT%zu_CTRL1:%lx\n", i, val);
	}
	for (i = 0; i < 4; i++) {
		val = dmc_prot_rw(dmc_mon->io_mem1, DMC_PROT_VIO_0 + (i << 2), 0, DMC_READ);
		sz += sprintf(buf + sz, "DMC_PROT_VIO_%zu:%lx\n", i, val);
	}
	val = dmc_prot_rw(dmc_mon->io_mem1, DMC_PROT_IRQ_CTRL, 0, DMC_READ);
	sz += sprintf(buf + sz, "DMC_PROT_IRQ_CTRL:%lx\n", val);
	val = dmc_prot_rw(dmc_mon->io_mem1, DMC_IRQ_STS, 0, DMC_READ);
	sz += sprintf(buf + sz, "DMC_IRQ_STS:%lx\n", val);

	return sz;
}

static void check_violation(struct dmc_monitor *mon, void *data)
{
	int i, port, subport;
	unsigned long addr, status, value;
	char id_str[MAX_NAME];
	struct page *page;
	struct page_trace *trace;

	for (i = 1; i < 4; i += 2) {
		status = dmc_prot_rw(dmc_mon->io_mem1, DMC_PROT_VIO_0 + (i << 2), 0, DMC_READ);

		switch (dmc_mon->chip) {
		case DMC_TYPE_T5W:
			value = (DMC_VIO_PROT_RANGE0_T5W |
						DMC_VIO_PROT_RANGE1_T5W);
			break;
		case DMC_TYPE_A5:
			value = (DMC_VIO_PROT_RANGE0_A5 | DMC_VIO_PROT_RANGE1_A5);
			break;

		default:
			value = (DMC_VIO_PROT_RANGE0 | DMC_VIO_PROT_RANGE1);
			break;
		}

		if (!(status & value))
			continue;

		addr = dmc_prot_rw(dmc_mon->io_mem1, DMC_PROT_VIO_0 + ((i - 1) << 2), 0,
				   DMC_READ);
		if (addr > mon->addr_end)
			continue;

		/* ignore violation on same page/same port */
		if ((addr & PAGE_MASK) == mon->last_addr &&
		    status == mon->last_status) {
			mon->same_page++;
			continue;
		}
		/* ignore cma driver pages */
		page = phys_to_page(addr);
		trace = find_page_base(page);
		if (trace && trace->migrate_type == MIGRATE_CMA)
			continue;

		switch (dmc_mon->chip) {
		case DMC_TYPE_T5W:
			port = (status >> 9) & 0x1f;
			subport = (status >> 4) & 0xf;
			value = (port == 10 && (subport == 6 || subport == 2));
			break;
		case DMC_TYPE_A5:
			port = (status >> 9) & 0x07;
			subport = (status >> 4) & 0xf;
			value = (port == 6 && subport == 2);
			break;

		default:
			port = (status >> 11) & 0x1f;
			subport = (status >> 6) & 0xf;
			value = (port == 7 && (subport == 11 || subport == 4));
			break;
		}

		/* ignore sd_emmc in device */
		if (value)
			continue;

		pr_emerg(DMC_TAG ", addr:%08lx, s:%08lx, ID:%s, sub:%s, c:%ld, d:%p\n",
			 addr, status, to_ports(port),
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

static void s4_dmc_mon_irq(struct dmc_monitor *mon, void *data)
{
	unsigned long value;

	value = dmc_prot_rw(dmc_mon->io_mem1, DMC_IRQ_STS, 0, DMC_READ);
	if (in_interrupt()) {
		if (value & DMC_WRITE_VIOLATION)
			check_violation(mon, data);

		/* check irq flags just after IRQ handler */
		mod_delayed_work(system_wq, &mon->work, 0);
	}
	/* clear irq */
	value &= 0x03;		/* irq flags */
	value |= 0x04;		/* en */
	dmc_prot_rw(dmc_mon->io_mem1, DMC_PROT_IRQ_CTRL, value, DMC_WRITE);
}

static int s4_dmc_mon_set(struct dmc_monitor *mon)
{
	unsigned long value, end;

	/* aligned to 64KB */
	end = ALIGN(mon->addr_end, DMC_ADDR_SIZE);
	value = (mon->addr_start >> 16) | ((end >> 16) << 16);
	dmc_prot_rw(dmc_mon->io_mem1, DMC_PROT0_RANGE, value, DMC_WRITE);

	dmc_prot_rw(dmc_mon->io_mem1, DMC_PROT0_CTRL, mon->device | 1 << 24, DMC_WRITE);

	value = (1 << 24 | 0xffff);

	dmc_prot_rw(dmc_mon->io_mem1, DMC_PROT0_CTRL1, value, DMC_WRITE);

	dmc_prot_rw(dmc_mon->io_mem1, DMC_PROT_IRQ_CTRL, 0x06, DMC_WRITE);

	pr_emerg("range:%08lx - %08lx, device:%llx\n",
		 mon->addr_start, mon->addr_end, mon->device);
	return 0;
}

void s4_dmc_mon_disable(struct dmc_monitor *mon)
{
	dmc_prot_rw(dmc_mon->io_mem1, DMC_PROT0_RANGE, 0, DMC_WRITE);
	dmc_prot_rw(dmc_mon->io_mem1, DMC_PROT0_CTRL, 0, DMC_WRITE);
	dmc_prot_rw(dmc_mon->io_mem1, DMC_PROT0_CTRL1, 0, DMC_WRITE);
	dmc_prot_rw(dmc_mon->io_mem1, DMC_PROT_IRQ_CTRL, 0, DMC_WRITE);
	mon->device     = 0;
	mon->addr_start = 0;
	mon->addr_end   = 0;
}

struct dmc_mon_ops s4_dmc_mon_ops = {
	.handle_irq = s4_dmc_mon_irq,
	.set_montor = s4_dmc_mon_set,
	.disable    = s4_dmc_mon_disable,
	.dump_reg   = s4_dmc_dump_reg,
};
