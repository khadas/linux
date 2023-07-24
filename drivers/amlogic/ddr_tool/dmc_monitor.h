/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __DMC_MONITOR_H__
#define __DMC_MONITOR_H__

#include "ddr_port.h"
#define PROTECT_READ		BIT(0)
#define PROTECT_WRITE		BIT(1)

#define DMC_MON_RW		0x8200004A

#define DMC_READ_VIOLATION	BIT(0)
#define DMC_WRITE_VIOLATION	BIT(1)

/*
 * Address is aligned to 64 KB
 */
#define DMC_ADDR_SIZE		(0x10000)
#define DMC_TAG			"DMC VIOLATION"

/* for configs */
#define DMC_DEVICE_8BIT		BIT(0)

/* for debug */
#define DMC_DEBUG_WRITE		BIT(0)
#define DMC_DEBUG_READ		BIT(1)
#define DMC_DEBUG_CMA		BIT(2)
#define DMC_DEBUG_SAME		BIT(3)
#define DMC_DEBUG_INCLUDE	BIT(4)
#define DMC_DEBUG_TRACE		BIT(5)
#define DMC_DEBUG_SUSPEND	BIT(6)
#define DMC_DEBUG_SERROR	BIT(7)

struct dmc_monitor;

struct dmc_mon_ops {
	void (*handle_irq)(struct dmc_monitor *mon, void *data);
	int  (*set_monitor)(struct dmc_monitor *mon);
	void (*disable)(struct dmc_monitor *mon);
	size_t (*dump_reg)(char *buf);
	int (*reg_control)(char *input, char control, char *output);
};

struct black_dev_list {
	unsigned int num;
	unsigned char device[10][MAX_NAME];
};

struct dmc_monitor {
	void __iomem  *io_mem1;		/* For dmc 1 */
	void __iomem  *io_mem2;		/* For dmc 2 */
	void __iomem  *io_mem3;		/* For dmc 3 */
	void __iomem  *io_mem4;		/* For dmc 4 */
	unsigned long  io_base;		/* For secure world access */
	unsigned long  addr_start;	/* monitor start address */
	unsigned long  addr_end;	/* monitor end address */
	u64            device;		/* monitor device mask */
	u32            mon_number;	/* monitor number */
	u32             debug;		/* monitor debug */
	unsigned short port_num;	/* how many devices? */
	unsigned short vpu_port_num;	/* vpu sub number */
	unsigned char  chip;		/* chip ID */
	unsigned char  configs;		/* config for dmc */
	unsigned long  last_addr;
	unsigned long  last_status;
	unsigned int   last_trace;
	struct ddr_port_desc *port;
	struct vpu_sub_desc *vpu_port;
	struct dmc_mon_ops   *ops;
};

void dmc_monitor_disable(void);

/*
 * start: physical start address, aligned to 64KB
 * end: physical end address, aligned to 64KB
 * dev_mask: device bit to set
 * en: 0: close monitor, 1: enable monitor
 */
int dmc_set_monitor(unsigned long start, unsigned long end,
		    unsigned long dev_mask, int en);

/*
 * start: physical start address, aligned to 64KB
 * end: physical end address, aligned to 64KB
 * port_name: name of port to set, see ddr_port_desc for each chip in
 *            drivers/amlogic/ddr_tool/ddr_port_desc.c
 * en: 0: close monitor, 1: enable monitor
 */
int dmc_set_monitor_by_name(unsigned long start, unsigned long end,
			    const char *port_name, int en);

unsigned int get_all_dev_mask(void);

/*
 * Following functions are internal used only
 */
unsigned long dmc_prot_rw(void __iomem *base, unsigned long addr,
			  unsigned long value, int rw);

char *to_ports(int id);
char *to_sub_ports_name(int mid, int sid, char rw);
int dmc_violation_ignore(char *title, unsigned long addr, unsigned long status,
				int port, int subport, char rw);
void show_violation_mem_printk(char *title, unsigned long addr, unsigned long status,
				int port, int sub_port, char rw);
void show_violation_mem_trace_event(unsigned long addr, unsigned long status,
				    int port, int sub_port, char rw);

extern struct dmc_monitor *dmc_mon;
#ifdef CONFIG_AMLOGIC_DMC_MONITOR_GX
extern struct dmc_mon_ops gx_dmc_mon_ops;
#endif
#ifdef CONFIG_AMLOGIC_DMC_MONITOR_G12
extern struct dmc_mon_ops g12_dmc_mon_ops;
#endif
#ifdef CONFIG_AMLOGIC_DMC_MONITOR_C1
extern struct dmc_mon_ops c1_dmc_mon_ops;
#endif
#ifdef CONFIG_AMLOGIC_DMC_MONITOR_TM2
extern struct dmc_mon_ops tm2_dmc_mon_ops;
#endif
#ifdef CONFIG_AMLOGIC_DMC_MONITOR_T7
extern struct dmc_mon_ops t7_dmc_mon_ops;
#endif
#ifdef CONFIG_AMLOGIC_DMC_MONITOR_S4
extern struct dmc_mon_ops s4_dmc_mon_ops;
#endif
#ifdef CONFIG_AMLOGIC_DMC_MONITOR_S5
extern struct dmc_mon_ops s5_dmc_mon_ops;
#endif
#ifdef CONFIG_AMLOGIC_DMC_MONITOR_A4
extern struct dmc_mon_ops a4_dmc_mon_ops;
#endif

#ifdef CONFIG_AMLOGIC_DMC_MONITOR
int __init dmc_monitor_init(void);
void dmc_monitor_exit(void);
#else
static int dmc_monitor_init(void)
{
	return 0;
}

void dmc_monitor_exit(void)
{
}
#endif

#ifdef CONFIG_AMLOGIC_USER_FAULT
void set_dump_dmc_func(void *f);
#else
void __weak set_dump_dmc_func(void *f) {}
#endif /* CONFIG_AMLOGIC_USER_FAULT */

#endif /* __DMC_MONITOR_H__ */
