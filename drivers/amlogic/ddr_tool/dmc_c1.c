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
	char rw = 'n';
	int port, subport;
	unsigned long irqreg;
	unsigned long addr = 0, status = 0;

	char title[10];

	irqreg = dmc_prot_rw(NULL, DMC_SEC_STATUS, 0, DMC_READ);
	if (irqreg & DMC_WRITE_VIOLATION) {
		status = dmc_prot_rw(NULL, DMC_VIO_ADDR1, 0, DMC_READ);
		addr = dmc_prot_rw(NULL, DMC_VIO_ADDR0, 0, DMC_READ);
		rw = 'w';
	} else if (irqreg & DMC_READ_VIOLATION) {
		status = dmc_prot_rw(NULL, DMC_VIO_ADDR3 + (1 << 2), 0, DMC_READ);
		addr = dmc_prot_rw(NULL, DMC_VIO_ADDR2, 0, DMC_READ);
		rw = 'r';
	}

	/* clear irq */
	dmc_prot_rw(NULL, DMC_SEC_STATUS, irqreg, DMC_WRITE);

	if (!(status & (PROT0_VIOLATION | PROT1_VIOLATION)))
		return;

	if (addr > mon->addr_end)
		return;

	port = status & 0x1f;
	subport = (status >> 9) & 0x7;
	if (port == 16)
		port = subport + 9;

	if (dmc_violation_ignore(title, addr, status | PROT0_VIOLATION | PROT1_VIOLATION,
				 port, subport, rw))
		return;

#if IS_ENABLED(CONFIG_EVENT_TRACING)
	if (mon->debug & DMC_DEBUG_TRACE) {
		show_violation_mem_trace_event(addr, status, port, subport, rw);
		return;
	}
#endif
	show_violation_mem_printk(title, addr, status, port, subport, rw);
}

static void c1_dmc_mon_irq(struct dmc_monitor *mon, void *data)
{
	check_violation(mon, data);
}

static int c1_dmc_mon_set(struct dmc_monitor *mon)
{
	unsigned long value, add;
	unsigned int wb;

	wb = mon->addr_start & 0x01;
	add = mon->addr_start & PAGE_MASK;

	dmc_prot_rw(NULL, DMC_SEC_STATUS, 0x3, DMC_WRITE);

	dmc_prot_rw(NULL, DMC_PROT0_STA, add, DMC_WRITE);
	add = mon->addr_end & PAGE_MASK;
	dmc_prot_rw(NULL, DMC_PROT0_EDA, add, DMC_WRITE);

	dmc_prot_rw(NULL, DMC_PROT0_CTRL, mon->device, DMC_WRITE);

	value = wb << 25;
	if (dmc_mon->debug & DMC_DEBUG_WRITE)
		value |= (1 << 24);
	/* if set, will be crash when read access */
	if (dmc_mon->debug & DMC_DEBUG_READ)
		value |= (1 << 26);
	dmc_prot_rw(NULL, DMC_PROT0_CTRL1, value, DMC_WRITE);

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

static int c1_reg_analysis(char *input, char *output)
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
		port = vio_reg3 & 0x1f;
		subport = (vio_reg3 >> 9) & 0x7;
		rw = 'r';

		count += sprintf(output + count, "DMC READ:");
		count += sprintf(output + count, "addr=%09lx port=%s sub=%s\n",
				 addr, to_ports(port), to_sub_ports_name(port, subport, rw));
	}

	if (status & 0x2) { /* write */
		addr = vio_reg0;
		port = vio_reg1 & 0x1f;
		subport = (vio_reg1 >> 9) & 0x7;
		rw = 'w';
		count += sprintf(output + count, "DMC WRITE:");
		count += sprintf(output + count, "addr=%09lx port=%s sub=%s\n",
				 addr, to_ports(port), to_sub_ports_name(port, subport, rw));
	}
	return count;
}

static int c1_dmc_reg_control(char *input, char control, char *output)
{
	int count = 0, i;
	unsigned long val;

	switch (control) {
	case 'a':	/* analysis vio reg */
		count = c1_reg_analysis(input, output);
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

struct dmc_mon_ops c1_dmc_mon_ops = {
	.handle_irq = c1_dmc_mon_irq,
	.set_monitor = c1_dmc_mon_set,
	.disable    = c1_dmc_mon_disable,
	.dump_reg   = c1_dmc_dump_reg,
	.reg_control   = c1_dmc_reg_control,
};
