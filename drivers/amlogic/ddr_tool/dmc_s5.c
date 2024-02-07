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
#include <linux/sched/clock.h>
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

#define DMC_VIO_PROT0		BIT(22)
#define DMC_VIO_PROT1		BIT(23)

#define DMC_SEC_STATUS			(0x051a << 2)
#define DMC_VIO_ADDR0			(0x051b << 2)
#define DMC_VIO_ADDR1			(0x051c << 2)
#define DMC_VIO_ADDR2			(0x051d << 2)
#define DMC_VIO_ADDR3			(0x051e << 2)

static size_t s5_dmc_dump_reg(char *buf)
{
	int sz = 0, i, j;
	unsigned long val, base;
	void *io;

	for (j = 0; j < dmc_mon->mon_number; j++) {
		io = dmc_mon->mon_comm[j].io_mem;
		base = dmc_mon->mon_comm[j].io_base;

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

		val = dmc_rw(base + DMC_SEC_STATUS, 0, DMC_READ);
		sz += sprintf(buf + sz, "DMC_SEC_STATUS:%lx\n", val);

		for (i = 0; i < 4; i++) {
			val = dmc_rw(base + DMC_VIO_ADDR0 + (i << 2), 0, DMC_READ);
			sz += sprintf(buf + sz, "DMC_VIO_ADDR%d:%lx\n", i, val);
		}
	}
	return sz;
}

static int check_violation(struct dmc_monitor *mon, void *data)
{
	int ret = -1;
	unsigned long irqreg;
	struct page *page;
	struct page_trace *trace;
	struct dmc_mon_comm *mon_comm = (struct dmc_mon_comm *)data;

	/* irq write */
	irqreg = dmc_prot_rw(mon_comm->io_mem, DMC_PROT_IRQ_CTRL_STS, 0, DMC_READ);

	if (irqreg & DMC_WRITE_VIOLATION) {
		/* combine address */
		mon_comm->time = sched_clock();
		mon_comm->status = dmc_prot_rw(mon_comm->io_mem, DMC_PROT_VIO_1, 0, DMC_READ);
		mon_comm->addr  = (mon_comm->status >> 20) & 0x03;
		mon_comm->addr  = (mon_comm->addr << 32ULL);
		mon_comm->addr |= dmc_prot_rw(mon_comm->io_mem, DMC_PROT_VIO_0, 0, DMC_READ);
		mon_comm->rw = 'w';
		page = phys_to_page(mon_comm->addr);
		trace = find_page_base(page);
		if (trace)
			mon_comm->trace = *trace;
		else
			mon_comm->trace.ip_data = IP_INVALID;
		mon_comm->page_flags = page->flags & PAGEFLAGS_MASK;
		ret = 0;
	} else if (irqreg & DMC_READ_VIOLATION) {
		/* irq read */
		mon_comm->time = sched_clock();
		mon_comm->status = dmc_prot_rw(mon_comm->io_mem, DMC_PROT_VIO_3, 0, DMC_READ);
		/* combine address */
		mon_comm->addr  = (mon_comm->status >> 20) & 0x03;
		mon_comm->addr  = (mon_comm->addr << 32ULL);
		mon_comm->addr |= dmc_prot_rw(mon_comm->io_mem, DMC_PROT_VIO_2, 0, DMC_READ);
		mon_comm->rw = 'r';
		page = phys_to_page(mon_comm->addr);
		trace = find_page_base(page);
		if (trace)
			mon_comm->trace = *trace;
		else
			mon_comm->trace.ip_data = IP_INVALID;
		mon_comm->page_flags = page->flags & PAGEFLAGS_MASK;
		ret = 0;
	}
	return ret;
}

static int s5_dmc_mon_irq(struct dmc_monitor *mon, void *data, char clear)
{
	unsigned long irqreg;
	struct dmc_mon_comm *mon_comm = (struct dmc_mon_comm *)data;

	if (clear) {
		/* clear irq */
		irqreg = dmc_prot_rw(mon_comm->io_mem, DMC_PROT_IRQ_CTRL_STS, 0, DMC_READ);
		if (dmc_mon->debug & DMC_DEBUG_SUSPEND)
			irqreg &= ~0x04;
		else
			irqreg |= 0x04;		/* en */
		dmc_prot_rw(mon_comm->io_mem, DMC_PROT_IRQ_CTRL_STS, irqreg, DMC_WRITE);
	} else {
		return check_violation(mon, data);
	}

	return 0;
}

static void s5_dmc_vio_to_port(void *data, unsigned long *vio_bit)
{
	int port = 0, subport = 0;
	struct dmc_mon_comm *mon_comm = (struct dmc_mon_comm *)data;

	*vio_bit = DMC_VIO_PROT1 | DMC_VIO_PROT0;
	port = mon_comm->status & 0xff;
	subport = (mon_comm->status >> 16) & 0x7;

	mon_comm->port.name = to_ports(port);
	if (!mon_comm->port.name)
		sprintf(mon_comm->port.id, "%d", port);

	mon_comm->sub.name = to_sub_ports_name(port, subport, mon_comm->rw);
	if (!mon_comm->sub.name)
		sprintf(mon_comm->sub.id, "%d", subport);
}

static int s5_dmc_mon_set(struct dmc_monitor *mon)
{
	unsigned long start, end, val;
	void *io;
	int i;
	unsigned int dev1, dev2;

	if (!dmc_mon->device)
		return 0;

	start = mon->addr_start >> PAGE_SHIFT;
	end   = ALIGN_DOWN(mon->addr_end, PAGE_SIZE);
	end   = end >> PAGE_SHIFT;
	dev1  = mon->device & 0xffffffff;
	dev2  = mon->device >> 32;

	for (i = 0; i < dmc_mon->mon_number; i++) {
		io = dmc_mon->mon_comm[i].io_mem;
		dmc_prot_rw(io, DMC_PROT_IRQ_CTRL_STS, 0x3, DMC_WRITE);
	}

	for (i = 0; i < dmc_mon->mon_number; i++) {
		io = dmc_mon->mon_comm[i].io_mem;

		if (dev1) {
			dmc_prot_rw(io, DMC_PROT0_STA, start, DMC_WRITE);
			dmc_prot_rw(io, DMC_PROT0_EDA, end, DMC_WRITE);
		} else {
			dmc_prot_rw(io, DMC_PROT0_STA, 0, DMC_WRITE);
			dmc_prot_rw(io, DMC_PROT0_EDA, 0, DMC_WRITE);
		}

		/* when set exclude, PROT1 can not be used */
		if (dev2 && (dmc_mon->debug & DMC_DEBUG_INCLUDE)) {
			dmc_prot_rw(io, DMC_PROT1_STA, start, DMC_WRITE);
			dmc_prot_rw(io, DMC_PROT1_EDA, end, DMC_WRITE);
		} else {
			dmc_prot_rw(io, DMC_PROT1_STA, 0UL, DMC_WRITE);
			dmc_prot_rw(io, DMC_PROT1_EDA, 0UL, DMC_WRITE);
		}

		val = 0xf;
		if (dmc_mon->debug & DMC_DEBUG_WRITE)
			val |= (1 << 27);

		if (dmc_mon->debug & DMC_DEBUG_READ)
			val |= (1 << 26);

		if (dmc_mon->debug & DMC_DEBUG_INCLUDE)
			val |= (1 << 8);
		else
			val &= ~(1 << 8);

		if (dev1) {
			dmc_prot_rw(io, DMC_PROT0_CTRL,  val,  DMC_WRITE);
			dmc_prot_rw(io, DMC_PROT0_CTRL1, dev1, DMC_WRITE);
		} else {
			dmc_prot_rw(io, DMC_PROT0_CTRL,  0UL,  DMC_WRITE);
			dmc_prot_rw(io, DMC_PROT0_CTRL1, 0UL, DMC_WRITE);
		}

		/* when set exclude, PROT1 can not be used */
		if (dev2 && (dmc_mon->debug & DMC_DEBUG_INCLUDE)) {
			dmc_prot_rw(io, DMC_PROT1_CTRL,  val,  DMC_WRITE);
			dmc_prot_rw(io, DMC_PROT1_CTRL1, dev2, DMC_WRITE);
		} else {
			dmc_prot_rw(io, DMC_PROT1_CTRL,  0UL,  DMC_WRITE);
			dmc_prot_rw(io, DMC_PROT1_CTRL1, 0UL, DMC_WRITE);
		}

		if (dmc_mon->debug & DMC_DEBUG_SUSPEND)
			val = 0X3;
		else
			val = 0X7;
		dmc_prot_rw(io, DMC_PROT_IRQ_CTRL_STS, val, DMC_WRITE);
	}

	pr_emerg("range:%08lx - %08lx, device:%16llx\n",
		 mon->addr_start, mon->addr_end, mon->device);
	return 0;
}

void s5_dmc_mon_disable(struct dmc_monitor *mon)
{
	void *io;
	int i;

	for (i = 0; i < dmc_mon->mon_number; i++) {
		io = dmc_mon->mon_comm[i].io_mem;
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

static int s5_reg_analysis(char *input, char *output)
{
	unsigned long status, vio_reg0, vio_reg1, vio_reg2, vio_reg3;
	int port, subport, count = 0;
	unsigned long addr;
	char rw = 'n';

	if (sscanf(input, "%lx %lx %lx %lx %lx",
			 &status, &vio_reg0, &vio_reg1,
			 &vio_reg2, &vio_reg3) != 5) {
		pr_emerg("%s parma input error, buf:%s\n", __func__, input);
		return 0;
	}

	if (status & 0x1) { /* read */
		rw = 'r';
		addr  = (vio_reg3 >> 20) & 0x03;
		addr  = (addr << 32ULL);
		addr |= vio_reg2;
		port = vio_reg3 & 0xff;
		subport = (vio_reg3 >> 16) & 0x7;
		count += sprintf(output + count, "DMC READ:");
		count += sprintf(output + count, "addr=%09lx port=%s sub=%s\n",
				 addr, to_ports(port), to_sub_ports_name(port, subport, rw));
	}

	if (status & 0x2) { /* write */
		rw = 'w';
		addr  = (vio_reg1 >> 20) & 0x03;
		addr  = (addr << 32ULL);
		addr |= vio_reg0;
		port = vio_reg1 & 0xff;
		subport = (vio_reg1 >> 16) & 0x7;
		count += sprintf(output + count, "DMC WRITE:");
		count += sprintf(output + count, "addr=%09lx port=%s sub=%s\n",
				 addr, to_ports(port), to_sub_ports_name(port, subport, rw));
	}
	return count;
}

static int s5_dmc_reg_control(char *input, char control, char *output)
{
	int s = 0, i, j;
	unsigned long val, base;

	switch (control) {
	case 'a':	/* analysis sec vio reg */
		s = s5_reg_analysis(input, output);
		break;
	case 'c':
	case 'd':
		for (j = 0; j < dmc_mon->mon_number; j++) {
			base = dmc_mon->mon_comm[j].io_base;

			switch (control) {
			case 'c':	/* clear sec statue reg */
				dmc_rw(base + DMC_SEC_STATUS, 0x3, DMC_WRITE);
				break;
			case 'd':	/* dump sec vio reg */
				s += sprintf(output + s, "DMC%d SEC INFO:\n", j);
				val = dmc_rw(base + DMC_SEC_STATUS, 0, DMC_READ);
				s += sprintf(output + s, "DMC_SEC_STATUS:%lx\n", val);
				for (i = 0; i < 4; i++) {
					val = dmc_rw(base + DMC_VIO_ADDR0 + (i << 2), 0, DMC_READ);
					s += sprintf(output + s, "DMC_VIO_ADDR%d:%lx\n", i, val);
				}
				break;
			}
		}
		break;
	default:
		break;
	}

	return s;
}

struct dmc_mon_ops s5_dmc_mon_ops = {
	.handle_irq  = s5_dmc_mon_irq,
	.vio_to_port = s5_dmc_vio_to_port,
	.set_monitor = s5_dmc_mon_set,
	.disable     = s5_dmc_mon_disable,
	.dump_reg    = s5_dmc_dump_reg,
	.reg_control = s5_dmc_reg_control,
};
