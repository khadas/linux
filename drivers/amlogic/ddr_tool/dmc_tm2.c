// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * drivers/amlogic/ddr_tool/dmc_tm2.c
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
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
#include <linux/sched/clock.h>
#include <linux/amlogic/page_trace.h>
#include "ddr_port.h"
#include "dmc_monitor.h"

#define DMC_PROT0_RANGE		((0x0030  << 2))
#define DMC_PROT0_CTRL		((0x0031  << 2))
#define DMC_PROT0_CTRL1		((0x0032  << 2))

#define DMC_PROT1_RANGE		((0x0033  << 2))
#define DMC_PROT1_CTRL		((0x0034  << 2))
#define DMC_PROT1_CTRL1		((0x0035  << 2))

#define DMC_PROT_VIO_0		((0x0036  << 2))
#define DMC_PROT_VIO_1		((0x0037  << 2))

#define DMC_PROT_VIO_2		((0x0038  << 2))
#define DMC_PROT_VIO_3		((0x0039  << 2))

#define DMC_PROT_IRQ_CTRL	((0x003a  << 2))
#define DMC_IRQ_STS		((0x003b  << 2))

#define DMC_SEC_STATUS		((0x00f2  << 2))
#define DMC_VIO_ADDR0		((0x00f3  << 2))
#define DMC_VIO_ADDR1		((0x00f4  << 2))
#define DMC_VIO_ADDR2		((0x00f5  << 2))
#define DMC_VIO_ADDR3		((0x00f6  << 2))

#define DMC_SEC_STATUS_SC2	((0x00fb  << 2))
#define DMC_VIO_ADDR0_SC2	((0x00fc  << 2))
#define DMC_VIO_ADDR1_SC2	((0x00fd  << 2))
#define DMC_VIO_ADDR2_SC2	((0x00fe  << 2))
#define DMC_VIO_ADDR3_SC2	((0x00ff  << 2))

#define DMC_VIO_PROT0		BIT(19)
#define DMC_VIO_PROT1		BIT(20)

static size_t tm2_dmc_dump_reg(char *buf)
{
	size_t sz = 0, i;
	unsigned long val;
	void *io = dmc_mon->mon_comm[0].io_mem;

	for (i = 0; i < 2; i++) {
		val = dmc_prot_rw(io, DMC_PROT0_RANGE + (i * 12), 0, DMC_READ);
		sz += sprintf(buf + sz, "DMC_PROT%zu_RANGE:%lx\n", i, val);
		val = dmc_prot_rw(io, DMC_PROT0_CTRL + (i * 12), 0, DMC_READ);
		sz += sprintf(buf + sz, "DMC_PROT%zu_CTRL:%lx\n", i, val);
		val = dmc_prot_rw(io, DMC_PROT0_CTRL1 + (i * 12), 0, DMC_READ);
		sz += sprintf(buf + sz, "DMC_PROT%zu_CTRL1:%lx\n", i, val);
	}
	for (i = 0; i < 4; i++) {
		val = dmc_prot_rw(io, DMC_PROT_VIO_0 + (i << 2), 0, DMC_READ);
		sz += sprintf(buf + sz, "DMC_PROT_VIO_%zu:%lx\n", i, val);
	}
	val = dmc_prot_rw(io, DMC_PROT_IRQ_CTRL, 0, DMC_READ);
	sz += sprintf(buf + sz, "DMC_PROT_IRQ_CTRL:%lx\n", val);
	val = dmc_prot_rw(io, DMC_IRQ_STS, 0, DMC_READ);
	sz += sprintf(buf + sz, "DMC_IRQ_STS:%lx\n", val);

	return sz;
}

static int check_violation(struct dmc_monitor *mon, void *data)
{
	int ret = -1;
	unsigned long irqreg;
	struct page *page;
	struct page_trace *trace;
	struct dmc_mon_comm *mon_comm = (struct dmc_mon_comm *)data;
	void *io = mon_comm->io_mem;

	irqreg = dmc_prot_rw(io, DMC_IRQ_STS, 0, DMC_READ);
	if (irqreg & DMC_WRITE_VIOLATION) {
		mon_comm->time = sched_clock();
		mon_comm->status = dmc_prot_rw(io, DMC_PROT_VIO_1, 0, DMC_READ);
		mon_comm->addr = dmc_prot_rw(io, DMC_PROT_VIO_0, 0, DMC_READ);
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
		mon_comm->time = sched_clock();
		mon_comm->status = dmc_prot_rw(io, DMC_PROT_VIO_3 + (1 << 2), 0, DMC_READ);
		mon_comm->addr = dmc_prot_rw(io, DMC_PROT_VIO_2, 0, DMC_READ);
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

static int tm2_dmc_mon_irq(struct dmc_monitor *mon, void *data, char clear)
{
	unsigned long irqreg;
	struct dmc_mon_comm *mon_comm = (struct dmc_mon_comm *)data;

	if (clear) {
		/* clear irq */
		irqreg = dmc_prot_rw(mon_comm->io_mem, DMC_PROT_IRQ_CTRL, 0, DMC_READ);
		if (dmc_mon->debug & DMC_DEBUG_SUSPEND)
			irqreg &= ~0x04;
		else
			irqreg |= 0x04;		/* en */
		dmc_prot_rw(mon_comm->io_mem, DMC_PROT_IRQ_CTRL, irqreg, DMC_WRITE);
	} else {
		return check_violation(mon, data);
	}

	return 0;
}

static void tm2_dmc_vio_to_port(void *data, unsigned long *vio_bit)
{
	int port = 0, subport = 0;
	struct dmc_mon_comm *mon_comm = (struct dmc_mon_comm *)data;

	*vio_bit = DMC_VIO_PROT1 | DMC_VIO_PROT0;

	port = (mon_comm->status >> 11) & 0x1f;
	subport = (mon_comm->status >> 6) & 0xf;

	mon_comm->port.name = to_ports(port);
	if (!mon_comm->port.name)
		sprintf(mon_comm->port.id, "%d", port);

	mon_comm->sub.name = to_sub_ports_name(port, subport, mon_comm->rw);
	if (!mon_comm->sub.name)
		sprintf(mon_comm->sub.id, "%d", subport);
}

static int tm2_dmc_mon_set(struct dmc_monitor *mon)
{
	unsigned long value, end;
	unsigned int wb;
	void *io = dmc_mon->mon_comm[0].io_mem;

	/* aligned to 64KB */
	wb = mon->addr_start & 0x01;
	end = ALIGN_DOWN(mon->addr_end, DMC_ADDR_SIZE);
	value = (mon->addr_start >> 16) | ((end >> 16) << 16);

	dmc_prot_rw(io, DMC_PROT0_RANGE, value, DMC_WRITE);

	dmc_prot_rw(io, DMC_PROT0_CTRL, mon->device | 1 << 24, DMC_WRITE);

	value = (wb << 25) | 0xffff;
	if (dmc_mon->debug & DMC_DEBUG_WRITE)
		value |= (1 << 24);
	/* if set, will be crash when read access */
	if (dmc_mon->debug & DMC_DEBUG_READ)
		value |= (1 << 26);
	dmc_prot_rw(io, DMC_PROT0_CTRL1, value, DMC_WRITE);

	if (dmc_mon->debug & DMC_DEBUG_SUSPEND)
		value = 0X3;
	else
		value = 0X7;
	dmc_prot_rw(io, DMC_PROT_IRQ_CTRL, value, DMC_WRITE);

	pr_emerg("range:%08lx - %08lx, device:%llx\n",
		 mon->addr_start, mon->addr_end, mon->device);
	return 0;
}

void tm2_dmc_mon_disable(struct dmc_monitor *mon)
{
	void *io = dmc_mon->mon_comm[0].io_mem;

	dmc_prot_rw(io, DMC_PROT0_RANGE, 0, DMC_WRITE);
	dmc_prot_rw(io, DMC_PROT0_CTRL, 0, DMC_WRITE);
	dmc_prot_rw(io, DMC_PROT0_CTRL1, 0, DMC_WRITE);
	dmc_prot_rw(io, DMC_PROT_IRQ_CTRL, 0, DMC_WRITE);
	mon->device     = 0;
	mon->addr_start = 0;
	mon->addr_end   = 0;
}

static int tm2_reg_analysis(char *input, char *output)
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
		addr = vio_reg2;
		port = (vio_reg3 >> 11) & 0x1f;
		subport = (vio_reg3 >> 6) & 0xf;
		rw = 'r';

		count += sprintf(output + count, "DMC READ:");
		count += sprintf(output + count, "addr=%09lx port=%s sub=%s\n",
				 addr, to_ports(port), to_sub_ports_name(port, subport, rw));
	}

	if (status & 0x2) { /* write */
		addr = vio_reg0;
		port = (vio_reg1 >> 11) & 0x1f;
		subport = (vio_reg1 >> 6) & 0xf;
		rw = 'w';
		count += sprintf(output + count, "DMC WRITE:");
		count += sprintf(output + count, "addr=%09lx port=%s sub=%s\n",
				 addr, to_ports(port), to_sub_ports_name(port, subport, rw));
	}
	return count;
}

static int tm2_dmc_reg_control(char *input, char control, char *output)
{
	int count = 0, i;
	unsigned long val;

	switch (control) {
	case 'a':	/* analysis sec vio reg */
		count = tm2_reg_analysis(input, output);
		break;
	case 'c':	/* clear sec statue reg */
		if (dmc_mon->chip == DMC_TYPE_SC2)
			dmc_prot_rw(NULL, 0x1000 + DMC_SEC_STATUS_SC2, 0x3, DMC_WRITE);
		else
			dmc_prot_rw(NULL, DMC_SEC_STATUS, 0x3, DMC_WRITE);
		break;
	case 'd':	/* dump sec vio reg */
		count += sprintf(output + count, "DMC SEC INFO:\n");
		if (dmc_mon->chip == DMC_TYPE_SC2) {
			val = dmc_prot_rw(NULL, 0x1000 + DMC_SEC_STATUS_SC2, 0, DMC_READ);
			count += sprintf(output + count, "DMC_SEC_STATUS:%lx\n", val);
			for (i = 0; i < 4; i++) {
				val = dmc_prot_rw(NULL, 0x1000 + DMC_VIO_ADDR0_SC2 + (i << 2),
						  0, DMC_READ);
				count += sprintf(output + count, "DMC_VIO_ADDR%d:%lx\n", i, val);
			}
		} else {
			val = dmc_prot_rw(NULL, DMC_SEC_STATUS, 0, DMC_READ);
			count += sprintf(output + count, "DMC_SEC_STATUS:%lx\n", val);
			for (i = 0; i < 4; i++) {
				val = dmc_prot_rw(NULL, DMC_VIO_ADDR0 + (i << 2), 0, DMC_READ);
				count += sprintf(output + count, "DMC_VIO_ADDR%d:%lx\n", i, val);
			}
		}
		break;
	default:
		break;
	}

	return count;
}

struct dmc_mon_ops tm2_dmc_mon_ops = {
	.handle_irq  = tm2_dmc_mon_irq,
	.vio_to_port = tm2_dmc_vio_to_port,
	.set_monitor = tm2_dmc_mon_set,
	.disable     = tm2_dmc_mon_disable,
	.dump_reg    = tm2_dmc_dump_reg,
	.reg_control = tm2_dmc_reg_control,
};
