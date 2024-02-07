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

#define DMC_PROT0_RANGE		((0x00a0  << 2))
#define DMC_PROT0_CTRL		((0x00a1  << 2))
#define DMC_PROT1_RANGE		((0x00a2  << 2))
#define DMC_PROT1_CTRL		((0x00a3  << 2))
#define DMC_SEC_STATUS		((0x00b8  << 2))
#define DMC_VIO_ADDR0		((0x00b9  << 2))
#define DMC_VIO_ADDR1		((0x00ba  << 2))
#define DMC_VIO_ADDR2		((0x00bb  << 2))
#define DMC_VIO_ADDR3		((0x00bc  << 2))

static size_t g12_dmc_dump_reg(char *buf)
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
	for (i = 0; i < 4; i++) {
		val = dmc_prot_rw(NULL, DMC_VIO_ADDR0 + (i << 2), 0, DMC_READ);
		sz += sprintf(buf + sz, "DMC_VIO_ADDR%zu:%lx\n", i, val);
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

	irqreg = dmc_prot_rw(NULL, DMC_SEC_STATUS, 0, DMC_READ);
	if (irqreg & DMC_WRITE_VIOLATION) {
		mon_comm->time = sched_clock();
		mon_comm->status = dmc_prot_rw(NULL, DMC_VIO_ADDR1, 0, DMC_READ);
		mon_comm->addr = dmc_prot_rw(NULL, DMC_VIO_ADDR0, 0, DMC_READ);
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
		mon_comm->status = dmc_prot_rw(NULL, DMC_VIO_ADDR3 + (1 << 2), 0, DMC_READ);
		mon_comm->addr = dmc_prot_rw(NULL, DMC_VIO_ADDR2, 0, DMC_READ);
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

static int g12_dmc_mon_irq(struct dmc_monitor *mon, void *data, char clear)
{
	if (clear)
		/* clear irq */
		dmc_prot_rw(NULL, DMC_SEC_STATUS, 0x3, DMC_WRITE);
	else
		return check_violation(mon, data);

	return 0;
}

static void g12_dmc_vio_to_port(void *data, unsigned long *vio_bit)
{
	int port = 0, subport = 0;
	struct dmc_mon_comm *mon_comm = (struct dmc_mon_comm *)data;

	switch (dmc_mon->chip) {
	case DMC_TYPE_G12A:
		*vio_bit = BIT(22) | BIT(21);
		port = (mon_comm->status >> 10) & 0x1f;
		subport = (mon_comm->status >> 6) & 0xf;
		break;
	case DMC_TYPE_G12B:
		*vio_bit = BIT(25) | BIT(24);
		port = (mon_comm->status >> 13) & 0x1f;
		subport = (mon_comm->status >> 6) & 0xf;
		break;
	case DMC_TYPE_SM1:
	case DMC_TYPE_TL1:
	case DMC_TYPE_TM2:
		*vio_bit = BIT(23) | BIT(22);
		port = (mon_comm->status >> 11) & 0x1f;
		subport = (mon_comm->status >> 6) & 0xf;
		break;
	default:
		break;
	}

	mon_comm->port.name = to_ports(port);
	if (!mon_comm->port.name)
		sprintf(mon_comm->port.id, "%d", port);

	mon_comm->sub.name = to_sub_ports_name(port, subport, mon_comm->rw);
	if (!mon_comm->sub.name)
		sprintf(mon_comm->sub.id, "%d", subport);
}

static int g12_dmc_mon_set(struct dmc_monitor *mon)
{
	unsigned long value, end;
	unsigned int wb;

	/* aligned to 64KB */
	wb = mon->addr_start & 0x01;
	end = ALIGN_DOWN(mon->addr_end, DMC_ADDR_SIZE);

	dmc_prot_rw(NULL, DMC_SEC_STATUS, 0x3, DMC_WRITE);

	value = (mon->addr_start >> 16) | ((end >> 16) << 16);
	dmc_prot_rw(NULL, DMC_PROT0_RANGE, value, DMC_WRITE);

	value = (wb << 25) | mon->device;
	if (dmc_mon->debug & DMC_DEBUG_WRITE)
		value |= (1 << 24);
	/* if set, will be crash when read access */
	if (dmc_mon->debug & DMC_DEBUG_READ)
		value |= (1 << 26);
	dmc_prot_rw(NULL, DMC_PROT0_CTRL, value, DMC_WRITE);

	pr_emerg("range:%08lx - %08lx, device:%llx\n",
		 mon->addr_start, mon->addr_end, mon->device);
	return 0;
}

void g12_dmc_mon_disable(struct dmc_monitor *mon)
{
	dmc_prot_rw(NULL, DMC_PROT0_RANGE, 0, DMC_WRITE);
	dmc_prot_rw(NULL, DMC_PROT0_CTRL, 0, DMC_WRITE);
	mon->device     = 0;
	mon->addr_start = 0;
	mon->addr_end   = 0;
}

static int g12_reg_analysis(char *input, char *output)
{
	unsigned long status, vio_reg0, vio_reg1, vio_reg2, vio_reg3;
	int port, subport, count = 0;
	unsigned long addr;
	char rw = 'n', offset;

	if (sscanf(input, "%lx %lx %lx %lx %lx",
			 &status, &vio_reg0, &vio_reg1,
			 &vio_reg2, &vio_reg3) != 5) {
		pr_emerg("%s parma input error, buf:%s\n", __func__, input);
		return 0;
	}

	switch (dmc_mon->chip) {
	case DMC_TYPE_G12B:
		/* bit fix for G12B */
		offset = 13;
		break;
	case DMC_TYPE_SM1:
	case DMC_TYPE_TL1:
	case DMC_TYPE_TM2:
		/* bit fix for SM1/TL1/TM2 */
		offset = 11;
		break;
	default: /* G12A */
		offset = 10;
		break;
	}

	if (status & 0x1) { /* read */
		addr = vio_reg2;
		port =  (vio_reg3 >> offset) & 0x1f;
		subport = (vio_reg3 >> 6) & 0xf;
		rw = 'r';

		count += sprintf(output + count, "DMC READ:");
		count += sprintf(output + count, "addr=%09lx port=%s sub=%s\n",
				 addr, to_ports(port), to_sub_ports_name(port, subport, rw));
	}

	if (status & 0x2) { /* write */
		addr = vio_reg0;
		port =  (vio_reg1 >> offset) & 0x1f;
		subport = (vio_reg1 >> 6) & 0xf;
		rw = 'w';
		count += sprintf(output + count, "DMC WRITE:");
		count += sprintf(output + count, "addr=%09lx port=%s sub=%s\n",
				 addr, to_ports(port), to_sub_ports_name(port, subport, rw));
	}
	return count;
}

static int g12_dmc_reg_control(char *input, char control, char *output)
{
	int count = 0, i;
	unsigned long val;

	switch (control) {
	case 'a':	/* analysis sec vio reg */
		count = g12_reg_analysis(input, output);
		break;
	case 'c':	/* clear sec statue reg */
		dmc_prot_rw(NULL, DMC_SEC_STATUS, 0x3, DMC_WRITE);
		break;
	case 'd':	/* dump sec vio reg */
		count += sprintf(output + count, "DMC SEC INFO:\n");
		val = dmc_prot_rw(NULL, DMC_SEC_STATUS, 0, DMC_READ);
		count += sprintf(output + count, "DMC_SEC_STATUS:%lx\n", val);
		for (i = 0; i < 4; i++) {
			val = dmc_prot_rw(NULL, DMC_VIO_ADDR0 + (i << 2), 0, DMC_READ);
			count += sprintf(output + count, "DMC_VIO_ADDR%d:%lx\n", i, val);
		}
		break;
	default:
		break;
	}

	return count;
}

struct dmc_mon_ops g12_dmc_mon_ops = {
	.handle_irq  = g12_dmc_mon_irq,
	.vio_to_port = g12_dmc_vio_to_port,
	.set_monitor = g12_dmc_mon_set,
	.disable     = g12_dmc_mon_disable,
	.dump_reg    = g12_dmc_dump_reg,
	.reg_control = g12_dmc_reg_control,
};
