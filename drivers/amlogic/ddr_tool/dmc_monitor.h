/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __DMC_MONITOR_H__
#define __DMC_MONITOR_H__

#define PROTECT_READ		BIT(0)
#define PROTECT_WRITE		BIT(1)

#define DMC_MON_RW		0x8200004A

#define DMC_WRITE_VIOLATION	BIT(1)

/*
 * Address is aligned to 64 KB
 */
#define DMC_ADDR_SIZE		(0x10000)
#define DMC_TAG			"DMC VIOLATION"

/* for T7 */
#define POLICY_INCLUDE		BIT(1)

struct dmc_monitor;
struct dmc_mon_ops {
	void (*handle_irq)(struct dmc_monitor *mon, void *data);
	int  (*set_montor)(struct dmc_monitor *mon);
	void (*disable)(struct dmc_monitor *mon);
	size_t (*dump_reg)(char *buf);
};

struct dmc_monitor {
	void __iomem  *io_mem1;		/* For dmc 1 */
	void __iomem  *io_mem2;		/* For dmc 2 */
	unsigned long  io_base;		/* For secure world access */
	unsigned long  addr_start;	/* monitor start address */
	unsigned long  addr_end;	/* monitor end address */
	u64            device;		/* monitor device mask */
	unsigned short port_num;	/* how many devices? */
	unsigned char  chip;		/* chip ID */
	unsigned char  configs;		/* config for dmc */
	unsigned long  last_addr;
	unsigned long  same_page;
	unsigned long  last_status;
	struct ddr_port_desc *port;
	struct dmc_mon_ops   *ops;
	struct delayed_work work;
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

size_t dump_dmc_reg(char *buf);

char *to_ports(int id);
char *to_sub_ports(int mid, int sid, char *id_str);
void show_violation_mem(unsigned long addr);

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

#endif /* __DMC_MONITOR_H__ */
