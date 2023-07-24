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

#define DMC_SEC_STATUS		((0x00fa  << 2))
#define DMC_VIO_ADDR0		((0x00fb  << 2))
#define DMC_VIO_ADDR1		((0x00fc  << 2))
#define DMC_VIO_ADDR2		((0x00fd  << 2))
#define DMC_VIO_ADDR3		((0x00fe  << 2))

#define DMC_VIO_PROT_RANGE0     BIT(19)
#define DMC_VIO_PROT_RANGE1     BIT(20)

#define DMC_VIO_PROT_RANGE0_T5W     BIT(20)
#define DMC_VIO_PROT_RANGE1_T5W     BIT(21)

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
	char rw = 'n';
	int port, subport;
	unsigned long value, irqreg;
	unsigned long addr = 0, status = 0;
	char title[10];

	irqreg = dmc_prot_rw(dmc_mon->io_mem1, DMC_IRQ_STS, 0, DMC_READ);
	if (irqreg & DMC_WRITE_VIOLATION) {
		status = dmc_prot_rw(dmc_mon->io_mem1, DMC_PROT_VIO_1, 0, DMC_READ);
		addr = dmc_prot_rw(dmc_mon->io_mem1, DMC_PROT_VIO_0, 0, DMC_READ);
		rw = 'w';
	} else if (irqreg & DMC_READ_VIOLATION) {
		status = dmc_prot_rw(dmc_mon->io_mem1, DMC_PROT_VIO_3, 0, DMC_READ);
		addr = dmc_prot_rw(dmc_mon->io_mem1, DMC_PROT_VIO_2, 0, DMC_READ);
		rw = 'r';
	}

	/* clear irq */
	if (dmc_mon->debug & DMC_DEBUG_SUSPEND)
		irqreg &= ~0x04;
	else
		irqreg |= 0x04;		/* en */
	dmc_prot_rw(dmc_mon->io_mem1, DMC_PROT_IRQ_CTRL, irqreg, DMC_WRITE);

	switch (dmc_mon->chip) {
	case DMC_TYPE_T5W:
		value = (DMC_VIO_PROT_RANGE0_T5W |
					DMC_VIO_PROT_RANGE1_T5W);
		break;

	default:
		value = (DMC_VIO_PROT_RANGE0 | DMC_VIO_PROT_RANGE1);
		break;
	}

	if (!(status & value))
		return;

	if (addr > mon->addr_end)
		return;

	switch (dmc_mon->chip) {
	case DMC_TYPE_T5W:
		port = (status >> 9) & 0x1f;
		subport = (status >> 4) & 0xf;
		break;

	default:
		port = (status >> 11) & 0x1f;
		subport = (status >> 6) & 0xf;
		break;
	}

	if (dmc_violation_ignore(title, addr, status | value, port, subport, rw))
		return;

#if IS_ENABLED(CONFIG_EVENT_TRACING)
	if (mon->debug & DMC_DEBUG_TRACE) {
		show_violation_mem_trace_event(addr, status, port, subport, rw);
		return;
	}
#endif
	show_violation_mem_printk(title, addr, status, port, subport, rw);
}

static void s4_dmc_mon_irq(struct dmc_monitor *mon, void *data)
{

	check_violation(mon, data);

}

static int s4_dmc_mon_set(struct dmc_monitor *mon)
{
	unsigned long value, end;
	unsigned int wb;

	/* aligned to 64KB */
	wb = mon->addr_start & 0x01;
	end = ALIGN(mon->addr_end, DMC_ADDR_SIZE);
	value = (mon->addr_start >> 16) | ((end >> 16) << 16);
	dmc_prot_rw(dmc_mon->io_mem1, DMC_PROT0_RANGE, value, DMC_WRITE);

	dmc_prot_rw(dmc_mon->io_mem1, DMC_PROT0_CTRL, mon->device | 1 << 24, DMC_WRITE);

	value = (wb << 25) | 0xffff;
	if (dmc_mon->debug & DMC_DEBUG_WRITE)
		value |= (1 << 24);
	/* if set, will be crash when read access */
	if (dmc_mon->debug & DMC_DEBUG_READ)
		value |= (1 << 26);
	dmc_prot_rw(dmc_mon->io_mem1, DMC_PROT0_CTRL1, value, DMC_WRITE);

	if (dmc_mon->debug & DMC_DEBUG_SUSPEND)
		value = 0X3;
	else
		value = 0X7;
	dmc_prot_rw(dmc_mon->io_mem1, DMC_PROT_IRQ_CTRL, value, DMC_WRITE);

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

static int s4_reg_analysis(char *input, char *output)
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
		rw = 'r';
		switch (dmc_mon->chip) {
		case DMC_TYPE_T5W:
			port = (vio_reg3 >> 9) & 0x1f;
			subport = (vio_reg3 >> 4) & 0xf;
			break;

		default:
			port = (vio_reg3 >> 11) & 0x1f;
			subport = (vio_reg3 >> 6) & 0xf;
			break;
		}
		count += sprintf(output + count, "DMC READ:");
		count += sprintf(output + count, "addr=%09lx port=%s sub=%s\n",
				 addr, to_ports(port), to_sub_ports_name(port, subport, rw));
	}

	if (status & 0x2) { /* write */
		addr = vio_reg0;
		rw = 'w';
		switch (dmc_mon->chip) {
		case DMC_TYPE_T5W:
			port = (vio_reg1 >> 9) & 0x1f;
			subport = (vio_reg1 >> 4) & 0xf;
			break;

		default:
			port = (vio_reg1 >> 11) & 0x1f;
			subport = (vio_reg1 >> 6) & 0xf;
			break;
		}
		count += sprintf(output + count, "DMC WRITE:");
		count += sprintf(output + count, "addr=%09lx port=%s sub=%s\n",
				 addr, to_ports(port), to_sub_ports_name(port, subport, rw));
	}
	return count;
}

static int s4_dmc_reg_control(char *input, char control, char *output)
{
	int count = 0, i;
	unsigned long val;

	switch (control) {
	case 'a':	/* analysis sec vio reg */
		count = s4_reg_analysis(input, output);
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

struct dmc_mon_ops s4_dmc_mon_ops = {
	.handle_irq = s4_dmc_mon_irq,
	.set_monitor = s4_dmc_mon_set,
	.disable    = s4_dmc_mon_disable,
	.dump_reg   = s4_dmc_dump_reg,
	.reg_control   = s4_dmc_reg_control,
};
