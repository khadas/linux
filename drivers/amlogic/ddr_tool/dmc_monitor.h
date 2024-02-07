/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __DMC_MONITOR_H__
#define __DMC_MONITOR_H__

#include <linux/kallsyms.h>
#include <linux/amlogic/page_trace.h>
#include "ddr_port.h"

#define PAGEFLAGS_MASK		((1UL << NR_PAGEFLAGS) - 1)

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
#define DMC_NUM_MAX		4
#define DMC_FILTER_MAX		10

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
#define DMC_DEBUG_IRQ_THREAD	BIT(8)

struct dmc_monitor;
struct dmc_mon_ops {
	int (*handle_irq)(struct dmc_monitor *mon, void *data, char clear);
	int  (*set_monitor)(struct dmc_monitor *mon);
	void (*disable)(struct dmc_monitor *mon);
	size_t (*dump_reg)(char *buf);
	int (*reg_control)(char *input, char control, char *output);
	void (*vio_to_port)(void *data, unsigned long *vio_bit);
};

struct dmc_filter {
	unsigned int num;
	unsigned char name[DMC_FILTER_MAX][KSYM_SYMBOL_LEN];
};

union port_type {
	char *name;
	char id[4];
};

struct dmc_mon_comm {
	/* dmc reg remap */
	unsigned long io_base;
	void __iomem  *io_mem;
	/* irq handle save info*/
	int irq;
	char rw;
	union port_type port;
	union port_type sub;
	unsigned long addr;
	unsigned long status;
	struct page_trace trace;
	unsigned long page_flags;
	unsigned long long time;
	unsigned long last_addr;
	unsigned long last_status;
	unsigned long last_trace;
	unsigned long long last_time;
	unsigned long long sys_run_time;
	u64 irq_run_time;
	struct task_struct *irq_thread_task;
};

struct dmc_monitor {
	unsigned long        sec_base;			/* For secure world access */
	u8                   mon_number;		/* monitor number */
	unsigned short       port_num;			/* how many devices */
	unsigned short       vpu_port_num;		/* vpu sub number */
	unsigned char        chip;			/* chip ID */
	unsigned char        configs;			/* feature for dmc */
	u32                  debug;			/* monitor debug */
	u64                  device;
	unsigned long        addr_start;
	unsigned long        addr_end;
	struct dmc_mon_comm  mon_comm[DMC_NUM_MAX];	/* monitor common info */
	struct dmc_filter    filter;
	struct ddr_port_desc *port;
	struct vpu_sub_desc  *vpu_port;
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
unsigned long dmc_prot_rw(void  __iomem *base, long off, unsigned long value, int rw);

char *to_ports(int id);
char *to_sub_ports_name(int mid, int sid, char rw);
int dmc_violation_ignore(char *title, void *data, unsigned long vio_bit);
void show_violation_mem_printk(char *title, void *data);
void show_violation_mem_trace_event(char *title, void *data);
void dmc_irq_sleep(void *data);
void dmc_output_violation(struct dmc_monitor *mon, void *data);

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
