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
#include <linux/of_device.h>
#include <linux/irqreturn.h>
#include <linux/module.h>
#include <linux/mm.h>

#include <linux/cpu.h>
#include <linux/smp.h>
#include <linux/kallsyms.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/arm-smccc.h>
#include <linux/highmem.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_reserved_mem.h>
#include "dmc_monitor.h"
#include "ddr_port.h"
#include <linux/amlogic/gki_module.h>
#include <asm/module.h>
#include <linux/mmzone.h>
#include <linux/amlogic/dmc_dev_access.h>
#include <linux/sched/cputime.h>
#include <linux/sched/clock.h>
#include <linux/string.h>

#define CREATE_TRACE_POINTS
#include "dmc_trace.h"

// #define DEBUG
#define DMC_VERSION		"1.3.1"

#define IRQ_CHECK		0
#define IRQ_CLEAR		1

#define DMC_RATELIMIT_BURST	30
#define dmc_pr_crit(fmt, addr, val, status, port, subport, page_flags, buddy, slab, lru, trace, time, rw, title, rs)	\
({															\
	if (__ratelimit(rs))												\
		pr_crit(fmt, addr, val, status, port, subport, page_flags, buddy, slab, lru, trace, time, rw, title);	\
})

struct dmc_monitor *dmc_mon;

static unsigned long init_dev_mask;
static unsigned long init_start_addr;
static unsigned long init_end_addr;
static unsigned long init_dmc_debug;

static bool dmc_reserved_flag;
static int dmc_original_debug;

/* irq thread enabled */
static unsigned char init_dmc_irq_thread_en = 1;
/* ns, irq check time, default is 5000000*/
static unsigned long init_dmc_irq_check_ns = 5000000;
/* default 5ms check once, if thread run over ratio(default 32/128), thread will be sleep */
static unsigned char init_dmc_irq_ratio = 32;
/* sleep time(us) when irq too much */
static unsigned long init_dmc_irq_usleep = 500;
/* thread irq recheck time */
static unsigned long init_thread_recheck_ns;

static int early_dmc_param(char *buf)
{
	unsigned long s_addr, e_addr, mask, debug = 0;
	/*
	 * Patten:  dmc_monitor=[start_addr],[end_addr],[mask]
	 * Example: dmc_monitor=0x00000000,0x20000000,0x7fce
	 * debug: cat /sys/class/dmc_monitor/debug
	 */
	if (!buf)
		return -EINVAL;

	if (sscanf(buf, "%lx,%lx,%lx,%lx", &s_addr, &e_addr, &mask, &debug) != 4) {
		if (sscanf(buf, "%lx,%lx,%lx", &s_addr, &e_addr, &mask) != 3)
			return -EINVAL;
	}

	init_start_addr = s_addr;
	init_end_addr   = e_addr;
	init_dev_mask   = mask;
	init_dmc_debug = debug;

	pr_info("%s, buf:%s, %lx-%lx, %lx, %lx\n",
		__func__, buf, s_addr, e_addr, mask, debug);

	return 0;
}
__setup("dmc_monitor=", early_dmc_param);

/* only used to dmc filter set on uboot cmdline */
static char dmc_filter_early_buf[1024];
static int early_dmc_filter(char *buf)
{
	/*
	 * set dev name or symbol will be ignore print when access
	 * Patten:  dmc_filter=[dev_name],[or symbol_name]
	 * Example: dmc_monitor=USB,ETH,__dma_direct_alloc_pages
	 * debug: cat /sys/class/dmc_monitor/filter
	 */
	snprintf(dmc_filter_early_buf, 1024, "%s", buf);
	pr_info("dmc_filter, buf:%s\n", buf);

	return 0;
}
__setup("dmc_filter=", early_dmc_filter);

static int early_dmc_irq_thread(char *buf)
{
	int ret;
	unsigned char irq_thread_en = 0, irq_ratio = 0;
	unsigned long dmc_irq_check_time = 0, dmc_irq_usleep = 0;
	unsigned long thread_recheck_ns = 0;

	ret = sscanf(buf, "%hhd,%ld,%hhd,%ld, %ld",
		     &irq_thread_en, &dmc_irq_check_time, &irq_ratio,
		     &dmc_irq_usleep, &thread_recheck_ns);
	if (ret < 0)
		return -EINVAL;

	init_dmc_irq_thread_en = irq_thread_en;

	if (dmc_irq_check_time)
		init_dmc_irq_check_ns = dmc_irq_check_time;

	if (irq_ratio)
		init_dmc_irq_ratio = irq_ratio;

	if (dmc_irq_usleep)
		init_dmc_irq_usleep = dmc_irq_usleep;

	if (thread_recheck_ns)
		init_thread_recheck_ns = thread_recheck_ns;

	pr_info("dmc_irq_thread, en:%d, check time:%ld, ratio:%d, usleep_time:%ld, thread recheck:%ld\n",
			init_dmc_irq_thread_en, init_dmc_irq_check_ns,
			init_dmc_irq_ratio, init_dmc_irq_usleep, init_thread_recheck_ns);

	return 0;
}
__setup("dmc_irq_thread=", early_dmc_irq_thread);

static int set_dmc_filter(unsigned char *buf)
{
	int i = 0;
	static const char c[] = ",";
	char *buffer, *p;

	buffer = kstrdup(buf, GFP_KERNEL);
	if (!buffer)
		return -ENOMEM;

	do {
		p = strsep(&buffer, c);
		if (p) {
			if (dmc_mon->filter.num >= DMC_FILTER_MAX) {
				pr_warn("dmc_filter set over max:%d!\n", DMC_FILTER_MAX);
				return 0;
			}

			for (i = 0; i < dmc_mon->filter.num; i++) {
				if (!strcmp(p, dmc_mon->filter.name[i]))
					return 0;
			}
			sprintf(dmc_mon->filter.name[i], "%s", p);
			dmc_mon->filter.num++;
		}
	} while (p);

	kfree(buffer);

	return 0;
}

unsigned long read_violation_mem(unsigned long addr, char rw)
{
	struct page *page;
	unsigned long *p, *q;
	unsigned long val;

	if (!pfn_valid(__phys_to_pfn(addr)))
		return 0xdeaddead;

	if (rw == 'r')
		return 0x0;

	page = phys_to_page(addr);
	p = kmap_atomic(page);
	if (!p)
		return 0xdeaddead;

	q = p + ((addr & (PAGE_SIZE - 1)) / sizeof(*p));
	val = *q;

	kunmap_atomic(p);
	return val;
}

void show_violation_mem_printk(char *title, void *data)
{
	struct dmc_mon_comm *mon_comm = (struct dmc_mon_comm *)data;

	static DEFINE_RATELIMIT_STATE(dmc_rs, HZ, DMC_RATELIMIT_BURST);

	dmc_pr_crit(DMC_TAG " addr=%09lx val=%016lx s=%08lx port=%s sub=%s f:%08lx bd:%d sb:%d lru:%d a:%ps t:%lld rw:%c%s\n",
		mon_comm->addr, read_violation_mem(mon_comm->addr, mon_comm->rw),
		mon_comm->status,
		virt_addr_valid(mon_comm->port.name) ? mon_comm->port.name : mon_comm->port.id,
		virt_addr_valid(mon_comm->sub.name) ? mon_comm->sub.name : mon_comm->sub.id,
		mon_comm->page_flags,
		mon_comm->page_flags & PAGE_FLAGS_CHECK_AT_FREE ? 1 : 0,
		test_bit(PG_slab, &mon_comm->page_flags),
		test_bit(PG_lru, &mon_comm->page_flags),
		(void *)unpack_ip(&mon_comm->trace),
		mon_comm->time, mon_comm->rw, title, &dmc_rs);
}

void show_violation_mem_trace_event(char *title, void *data)
{
	struct dmc_mon_comm *mon_comm = (struct dmc_mon_comm *)data;
	char *port = virt_addr_valid(mon_comm->port.name) ? mon_comm->port.name : mon_comm->port.id;
	char *sub = virt_addr_valid(mon_comm->sub.name) ? mon_comm->sub.name : mon_comm->sub.id;

	trace_dmc_violation(title, mon_comm->addr,
				mon_comm->status, port, sub,
				mon_comm->rw,
				unpack_ip(&mon_comm->trace),
				mon_comm->page_flags,
				mon_comm->time);
}

unsigned long dmc_prot_rw(void  __iomem *base,
			  long off, unsigned long value, int rw)
{
	if (base) {
		if (rw == DMC_WRITE) {
			writel(value, base + off);
			return 0;
		} else {
			return readl(base + off);
		}
	} else {
		return dmc_rw(off + dmc_mon->sec_base, value, rw);
	}
}

static inline int dmc_dev_is_byte(struct dmc_monitor *mon)
{
	return mon->configs & DMC_DEVICE_8BIT;
}

static int dev_name_to_id(const char *dev_name)
{
	int i, len;

	for (i = 0; i < dmc_mon->port_num; i++) {
		if (dmc_mon->port[i].port_id >= PORT_MAJOR &&
		    !dmc_dev_is_byte(dmc_mon))
			return -1;
		len = strlen(dmc_mon->port[i].port_name);
		if (!strncmp(dmc_mon->port[i].port_name, dev_name, len))
			break;
	}
	if (i >= dmc_mon->port_num)
		return -1;
	return dmc_mon->port[i].port_id;
}

char *to_ports(int id)
{
	char *name;
	int i;

	for (i = 0; i < dmc_mon->port_num; i++) {
		if (dmc_mon->port[i].port_id == id) {
			name =  dmc_mon->port[i].port_name;
			return name;
		}
	}
	return NULL;
}

char *to_sub_ports_name(int mid, int sid, char rw)
{
	char *name = NULL;
	int i, s_port = 0;

	if (strstr(to_ports(mid), "VPU")) {
		name = vpu_to_sub_port(to_ports(mid), rw, sid, NULL);
	} else if (strstr(to_ports(mid), "DEVICE")) {
		if (strstr(to_ports(mid), "DEVICE1"))
			s_port = sid + PORT_MAJOR + 8;
		else
			s_port = sid + PORT_MAJOR;

		for (i = 0; i < dmc_mon->port_num; i++) {
			if (dmc_mon->port[i].port_id == s_port) {
				name =  dmc_mon->port[i].port_name;
				break;
			}
		}
	}

	return name;
}

unsigned int get_all_dev_mask(void)
{
	unsigned int ret = 0;
	int i;

	if (dmc_dev_is_byte(dmc_mon))	/* not supported */
		return 0;

	for (i = 0; i < PORT_MAJOR; i++) {
		if (dmc_mon->port[i].port_id >= PORT_MAJOR)
			break;
		ret |= (1ULL << dmc_mon->port[i].port_id);
	}
	return ret;
}

static unsigned int get_other_dev_mask(void)
{
	unsigned int ret = 0;
	int i;


	for (i = 0; i < PORT_MAJOR; i++) {
		if (dmc_mon->port[i].port_id >= PORT_MAJOR)
			break;

		/*
		 * we don't want id with arm mali and device
		 * because these devices can access all ddr range
		 * and generate value-less report
		 */
		if (strstr(dmc_mon->port[i].port_name, "ARM")  ||
		    strstr(dmc_mon->port[i].port_name, "MALI") ||
		    strstr(dmc_mon->port[i].port_name, "DEVICE") ||
		    strstr(dmc_mon->port[i].port_name, "USB") ||
		    strstr(dmc_mon->port[i].port_name, "ETH") ||
		    strstr(dmc_mon->port[i].port_name, "EMMC"))
			continue;

		ret |= (1 << dmc_mon->port[i].port_id);
	}
	return ret;
}

static void output_violation(struct dmc_monitor *mon, void *data, char *title)
{
	unsigned long vio_bit = 0;
	struct dmc_mon_comm *mon_comm = (struct dmc_mon_comm *)data;

	/* get port subport and vio_bit */
	if (dmc_mon->ops && dmc_mon->ops->vio_to_port)
		dmc_mon->ops->vio_to_port(data, &vio_bit);

	if (!(mon_comm->status & vio_bit))
		goto irq_finish;

	if (mon_comm->addr > mon->addr_end)
		goto irq_finish;

	/* ignore violation */
	if (dmc_violation_ignore(title, data, vio_bit))
		goto irq_finish;

	/* violation print */
#if IS_ENABLED(CONFIG_EVENT_TRACING)
	if (mon->debug & DMC_DEBUG_TRACE) {
		show_violation_mem_trace_event(title, data);
		goto irq_finish;
	}
#endif
	show_violation_mem_printk(title, data);

	/* sleep if the irq too much */
irq_finish:
	if (mon->debug & DMC_DEBUG_IRQ_THREAD)
		dmc_irq_sleep(data);
}

void dmc_output_violation(struct dmc_monitor *mon, void *data)
{
	char title[10];
	struct dmc_mon_comm *mon_comm = (struct dmc_mon_comm *)data;
	struct dmc_mon_comm mon_comm_tmp = *mon_comm;
	unsigned long long t;

	if (dmc_mon->ops && dmc_mon->ops->handle_irq) {
		/* clear violation and flush */
		dmc_mon->ops->handle_irq(mon, data, IRQ_CLEAR);

		t = sched_clock();
		do {
			/* check new violation happened */
			if (!dmc_mon->ops->handle_irq(mon, &mon_comm_tmp, IRQ_CHECK)) {
				/* print old violation info */
				output_violation(mon, data, title);
				mon_comm->time = mon_comm_tmp.time;
				mon_comm->addr = mon_comm_tmp.addr;
				mon_comm->status = mon_comm_tmp.status;
				mon_comm->trace = mon_comm_tmp.trace;
				mon_comm->page_flags = mon_comm_tmp.page_flags;
				mon_comm->rw = mon_comm_tmp.rw;
				sprintf(title, "%s", "_isr");
				break;
			}
		} while (sched_clock() - t < init_thread_recheck_ns);

		/* if not new violation, print old info, otherwise new info*/
		output_violation(mon, data, title);

		/* clear violation and flush */
		dmc_mon->ops->handle_irq(mon, data, IRQ_CLEAR);
	}
}

int dmc_violation_ignore(char *title, void *data, unsigned long vio_bit)
{
	int i, is_ignore = 0;
	struct dmc_mon_comm *mon_comm = (struct dmc_mon_comm *)data;
	unsigned long status = mon_comm->status | vio_bit;
	struct page_trace *trace = &mon_comm->trace;
#ifdef CONFIG_KALLSYMS
	char sym[KSYM_SYMBOL_LEN];
#endif

	/* if trace invalid , should not be ignore*/
	if (trace && trace->order == IP_INVALID)
		goto dmc_ignore;

	/* ignore cma driver pages */
	if (trace->migrate_type == MIGRATE_CMA) {
		if (dmc_mon->debug & DMC_DEBUG_CMA) {
			sprintf(title, "%s", "_cma");
		} else {
			is_ignore = 1;
			goto dmc_ignore;
		}
	}

	/* ignore violation on same page/same port */
	if ((mon_comm->addr & PAGE_MASK) == mon_comm->last_addr &&
	     status == mon_comm->last_status &&
	     trace->ip_data == mon_comm->last_trace) {
		if (dmc_mon->debug & DMC_DEBUG_SAME) {
			sprintf(title, "%s", "_same");
		} else {
			is_ignore = 1;
			goto dmc_ignore;
		}
	}

#ifdef CONFIG_KALLSYMS
	sprint_symbol_no_offset(sym, unpack_ip(&mon_comm->trace));
#endif

	/* ignore black dev or symbol filter */
	for (i = 0; i < dmc_mon->filter.num; i++) {
		if (virt_addr_valid(mon_comm->port.name)) {
			if (strstr(mon_comm->port.name, dmc_mon->filter.name[i])) {
				is_ignore = 1;
				goto dmc_ignore;
			}
		}
		if (virt_addr_valid(mon_comm->sub.name)) {
			if (strstr(mon_comm->sub.name, dmc_mon->filter.name[i])) {
				is_ignore = 1;
				goto dmc_ignore;
			}
		}
	#ifdef CONFIG_KALLSYMS
		if (strstr(sym, dmc_mon->filter.name[i]) == sym) {
			is_ignore = 1;
			goto dmc_ignore;
		}
	#endif
	}

dmc_ignore:
	mon_comm->last_trace = trace ? trace->ip_data : 0xdeaddead;
	mon_comm->last_addr = mon_comm->addr & PAGE_MASK;
	mon_comm->last_status = status;

	return is_ignore;
}

static void dmc_set_default(struct dmc_monitor *mon)
{
	char default_filter_dev[] = "USB,ETH,EMMC";
	char default_filter_sym[] = "__dma_direct_alloc_pages,codec_mm_page_alloc_all_locked";

	set_dmc_filter(default_filter_dev);
	set_dmc_filter(default_filter_sym);

	if (dmc_dev_is_byte(mon)) {
		switch (mon->chip) {
	#ifdef CONFIG_AMLOGIC_DMC_MONITOR_T7
		case DMC_TYPE_T7:
			mon->device = 0x02030405;
			mon->debug &= ~DMC_DEBUG_INCLUDE;
			break;
		case DMC_TYPE_T3:
			mon->device = 0x02041c11;
			mon->debug &= ~DMC_DEBUG_INCLUDE;
			break;
		case DMC_TYPE_P1:
			mon->device = 0x02324c52;
			mon->debug &= ~DMC_DEBUG_INCLUDE;
			break;
	#endif
	#ifdef CONFIG_AMLOGIC_DMC_MONITOR_C3
		case DMC_TYPE_C3:
			mon->device = 0x02495051;
			mon->debug &= ~DMC_DEBUG_INCLUDE;
			break;
	#endif
	#ifdef CONFIG_AMLOGIC_DMC_MONITOR_T5M
		case DMC_TYPE_T5M:
			mon->device = 0x0204253a;
			mon->debug &= ~DMC_DEBUG_INCLUDE;
			break;
	#endif
	#ifdef CONFIG_AMLOGIC_DMC_MONITOR_S5
		case DMC_TYPE_S5:
			mon->device = 0x02040809;
			mon->debug &= ~DMC_DEBUG_INCLUDE;
			break;
	#endif
		}
	} else {
		mon->device = get_other_dev_mask();
	}

	pr_emerg("set dmc default: device=%llx, debug=%x\n", mon->device, mon->debug);

	if (dmc_mon->addr_start < dmc_mon->addr_end && dmc_mon->ops &&
	     dmc_mon->ops->set_monitor)
		dmc_mon->ops->set_monitor(dmc_mon);
}

static void dmc_clear_reserved_memory(struct dmc_monitor *mon)
{
	if (dmc_reserved_flag) {
		mon->debug = dmc_original_debug;
		mon->device = 0;
		mon->addr_start = 0;
		mon->addr_end = 0;
		dmc_reserved_flag = 0;
	}
}

static void dmc_enabled_reserved_memory(struct platform_device *pdev, struct dmc_monitor *mon)
{
	struct device_node *node;
	struct reserved_mem *rmem;

	node = of_parse_phandle(pdev->dev.of_node, "memory-region", 0);
	if (!node)
		return;

	rmem = of_reserved_mem_lookup(node);
	if (!rmem) {
		pr_info("no such reserved mem of dmc_monitor\n");
		return;
	}

	if (!rmem->size)
		return;

	mon->addr_start = rmem->base;
	mon->addr_end = rmem->base + rmem->size - 1;

	dmc_original_debug = mon->debug;

	if (dmc_dev_is_byte(mon)) {
		mon->device = 0x0102;
		mon->debug &= ~DMC_DEBUG_INCLUDE;
	} else {
		mon->device = get_all_dev_mask();
		mon->device &= 0xfffffffffffffffe;
	}

	mon->debug &= ~DMC_DEBUG_TRACE;
	mon->debug |= DMC_DEBUG_CMA;
	mon->debug |= DMC_DEBUG_SAME;
	dmc_reserved_flag = 1;

	if (mon->addr_start < mon->addr_end && mon->ops && mon->ops->set_monitor) {
		pr_emerg("DMC DEBUG MEMORY ENABLED: range:%lx-%lx, device=%llx, debug=%x\n",
				mon->addr_start, mon->addr_end, mon->device, mon->debug);
		mon->ops->set_monitor(mon);
	}
}

size_t dump_dmc_reg(char *buf)
{
	size_t sz = 0, i;
	unsigned char dev;
	u64 devices;

	if (dmc_mon->ops && dmc_mon->ops->dump_reg)
		sz += dmc_mon->ops->dump_reg(buf);
	sz += sprintf(buf + sz, "SEC_BASE:0x%lx\n", dmc_mon->sec_base);
	sz += sprintf(buf + sz, "DMC_DEBUG:0x%x\n", dmc_mon->debug);
	sz += sprintf(buf + sz, "RANGE:0x%lx - 0x%lx\n",
		      dmc_mon->addr_start, dmc_mon->addr_end);
	sz += sprintf(buf + sz, "MONITOR DEVICE(%s):\n",
		dmc_mon->debug & DMC_DEBUG_INCLUDE ? "include" : "exclude");
	if (!dmc_mon->device)
		return sz;

	if (dmc_dev_is_byte(dmc_mon)) {
		devices = dmc_mon->device;
		for (i = 0; i < sizeof(dmc_mon->device); i++) {
			dev = devices & 0xff;
			devices >>= 8ULL;
			if (dev)
				sz += sprintf(buf + sz, "    %s\n", to_ports(dev));
		}
	} else {
		for (i = 0; i < sizeof(dmc_mon->device) * 8; i++) {
			if (dmc_mon->device & (1ULL << i))
				sz += sprintf(buf + sz, "    %s\n", to_ports(i));
		}
	}

	return sz;
}

static size_t str_end_char(const char *s, size_t count, int c)
{
	size_t len = count, offset = count;

	while (count--) {
		if (*s == (char)c)
			offset = len - count;
		if (*s++ == '\0') {
			offset = len - count;
			break;
		}
	}
	return offset;
}

static void serror_dump_dmc_reg(void)
{
	int len = 0, i = 0, offset = 0;
	static char buf[2048] = {0};

	if (dmc_mon->ops && dmc_mon->ops->reg_control) {
		len = dmc_mon->ops->reg_control(NULL, 'd', buf);
		while (i < len) {
			offset = str_end_char(buf + i, 512, '\n');
			if (offset > 1)
				pr_emerg("%.*s", offset, buf + i);
			i += offset;
		}
		pr_emerg("\n");
	}
}

static irqreturn_t __nocfi dmc_monitor_irq_handler(int irq, void *dev_instance)
{
	if (dmc_mon->ops && dmc_mon->ops->handle_irq)
		dmc_mon->ops->handle_irq(dmc_mon, dev_instance, IRQ_CHECK);

	if (dmc_mon->debug & DMC_DEBUG_IRQ_THREAD)
		return IRQ_WAKE_THREAD;

	dmc_output_violation(dmc_mon, dev_instance);

	return IRQ_HANDLED;
}

static irqreturn_t dmc_irq_thread(int irq, void *dev_instance)
{
	dmc_output_violation(dmc_mon, dev_instance);

	return IRQ_HANDLED;
}

void dmc_irq_sleep(void *data)
{
	struct dmc_mon_comm *mon_comm = (struct dmc_mon_comm *)data;
	unsigned long long sys_clock, task_time;
	u64 t_ns = 0;

	/* get system and thread runtime from before irq to current */
	sys_clock = sched_clock();

	/* old soc may creat a secure irq when irq set but irq thread not be init */
	if (!mon_comm->irq_thread_task)
		return;

	task_time = task_sched_runtime(mon_comm->irq_thread_task);

	if (sys_clock - mon_comm->sys_run_time >= init_dmc_irq_check_ns * 2) {
	/* If the time between two interruptions is more than 10ms, refresh and recalculate */
		mon_comm->sys_run_time = sys_clock;
		mon_comm->irq_run_time = task_time;
	} else if (sys_clock - mon_comm->sys_run_time >= init_dmc_irq_check_ns) {
		t_ns = task_time - mon_comm->irq_run_time;
		/* if system run than 5ms, and thread runtime than ratio, sleep */
		if (t_ns >= (init_dmc_irq_check_ns >> 7) * init_dmc_irq_ratio)
			usleep_range(init_dmc_irq_usleep, init_dmc_irq_usleep + 100);

		mon_comm->sys_run_time = sys_clock;
		mon_comm->irq_run_time = task_time;
	}
}

static int dmc_irq_set(struct device_node *node, int save_irq, int request)
{
	static int save_irq_num, request_num;
	int irq, r = 0, i, affinity_cpu = 1;
	struct irq_desc *desc;

	pr_info("%s: save_irq=%d, request=%d\n", __func__, save_irq, request);
	if (!save_irq && !save_irq_num)
		return -EINVAL;
	if (!save_irq && ((request && request_num) || (!request && !request_num)))
		return -EINVAL;

	for (i = 0; i < dmc_mon->mon_number; i++) {
		if (save_irq) {
			dmc_mon->mon_comm[i].irq = of_irq_get(node, i);
			save_irq_num++;
			if (!request)
				continue;
		}

		irq = dmc_mon->mon_comm[i].irq;
		if (request) {
			r = request_threaded_irq(irq,
						 dmc_monitor_irq_handler,
						 dmc_irq_thread,
						 IRQF_SHARED | IRQF_ONESHOT,
						 "dmc_monitor", &dmc_mon->mon_comm[i]);
			if (r < 0) {
				pr_err("request irq fail:%d, r:%d\n", irq, r);
				request_num = 0;
				dmc_mon = NULL;
				return -EINVAL;
			}

			if (affinity_cpu <= num_online_cpus()) {
				irq_set_affinity_hint(irq, get_cpu_mask(affinity_cpu));
				affinity_cpu++;
			} else {
				irq_set_affinity_hint(irq, get_cpu_mask(num_online_cpus()));
			}
			desc = irq_to_desc(irq);
			dmc_mon->mon_comm[i].irq_thread_task = desc->action->thread;
			request_num++;
		} else {
			free_irq(irq, &dmc_mon->mon_comm[i]);
			request_num--;
		}
	}
	return 0;
}

static int dmc_regulation_dev(unsigned long dev, int add)
{
	unsigned char *p, cur;
	int i, set;

	if (dmc_dev_is_byte(dmc_mon)) {
		/* dev is a set of 8 bit user id index */
		while (dev) {
			cur = dev & 0xff;
			set = 0;
			p   = (unsigned char *)&dmc_mon->device;
			for (i = 0; i < sizeof(dmc_mon->device); i++) {
				if (p[i] == cur) {	/* already set */
					if (add)
						set = 1;
					else		/* clear it */
						p[i] = 0;
					break;
				}

				if (p[i] == 0 && add) {	/* find empty one */
					p[i] = (dev & 0xff);
					set = 1;
					break;
				}
			}
			if (i == sizeof(dmc_mon->device) && !set && add) {
				pr_err("%s, monitor device full\n", __func__);
				return -EINVAL;
			}
			dev >>= 8;
		}
	} else {
		if (add) /* dev is bit mask */
			dmc_mon->device |= dev;
		else
			dmc_mon->device &= ~(dev);
	}
	return 0;
}

int dmc_set_monitor(unsigned long start, unsigned long end,
		    unsigned long dev_mask, int en)
{
	if (!dmc_mon)
		return -EINVAL;

	dmc_clear_reserved_memory(dmc_mon);
	dmc_mon->addr_start = start;
	dmc_mon->addr_end   = end;
	dmc_regulation_dev(dev_mask, en);
	if (start < end && dmc_mon->ops && dmc_mon->ops->set_monitor)
		return dmc_mon->ops->set_monitor(dmc_mon);
	return -EINVAL;
}
EXPORT_SYMBOL(dmc_set_monitor);

int dmc_set_monitor_by_name(unsigned long start, unsigned long end,
			    const char *port_name, int en)
{
	long id;

	id = dev_name_to_id(port_name);
	if (id >= 0 && dmc_dev_is_byte(dmc_mon))
		return dmc_set_monitor(start, end, id, en);
	else if (id < 0 || id >= BITS_PER_LONG)
		return -EINVAL;
	return dmc_set_monitor(start, end, 1UL << id, en);
}
EXPORT_SYMBOL(dmc_set_monitor_by_name);

void dmc_monitor_disable(void)
{
	memset(&dmc_mon->filter, 0, sizeof(dmc_mon->filter));

	if (dmc_mon->ops && dmc_mon->ops->disable)
		return dmc_mon->ops->disable(dmc_mon);
}
EXPORT_SYMBOL(dmc_monitor_disable);

static ssize_t range_show(struct class *cla,
			  struct class_attribute *attr, char *buf)
{
	if (!dmc_mon->ops) {
		pr_err("Can't find ops for chip:%x\n", dmc_mon->chip);
		return 0;
	}

	return sprintf(buf, "%08lx - %08lx\n",
		       dmc_mon->addr_start, dmc_mon->addr_end);
}

static ssize_t range_store(struct class *cla,
			   struct class_attribute *attr,
			   const char *buf, size_t count)
{
	int ret;
	unsigned long start, end;

	if (!dmc_mon->ops) {
		pr_err("Can't find ops for chip:%x\n", dmc_mon->chip);
		return count;
	}

	ret = sscanf(buf, "%lx %lx", &start, &end);
	if (ret != 2) {
		pr_info("%s, bad input:%s\n", __func__, buf);
		return count;
	}
	dmc_set_monitor(start, end, dmc_mon->device, 1);
	return count;
}
static CLASS_ATTR_RW(range);

static ssize_t device_store(struct class *cla,
			    struct class_attribute *attr,
			    const char *buf, size_t count)
{
	int i;

	if (!dmc_mon->ops) {
		pr_err("Can't find ops for chip:%x\n", dmc_mon->chip);
		return count;
	}

	dmc_clear_reserved_memory(dmc_mon);

	if (!strncmp(buf, "none", 4)) {
		dmc_monitor_disable();
		return count;
	}

	if (!strncmp(buf, "default", 7)) {
		dmc_set_default(dmc_mon);
		return count;
	}

	if (dmc_dev_is_byte(dmc_mon)) {
		i = dev_name_to_id(buf);
		if (i < 0) {
			pr_info("bad device:%s\n", buf);
			return -EINVAL;
		}
		if (dmc_regulation_dev(i, 1))
			return -EINVAL;
	} else {
		if (!strncmp(buf, "all", 3)) {
			dmc_mon->device = get_all_dev_mask();
		} else {
			i = dev_name_to_id(buf);
			if (i < 0) {
				pr_info("bad device:%s\n", buf);
				return -EINVAL;
			}
			dmc_regulation_dev(1UL << i, 1);
		}
	}
	if (dmc_mon->addr_start < dmc_mon->addr_end && dmc_mon->ops &&
	     dmc_mon->ops->set_monitor)
		dmc_mon->ops->set_monitor(dmc_mon);

	return count;
}

static ssize_t device_show(struct class *cla,
			   struct class_attribute *attr, char *buf)
{
	int i, s = 0;

	if (!dmc_mon->ops) {
		pr_err("Can't find ops for chip:%x\n", dmc_mon->chip);
		return 0;
	}

	s += sprintf(buf + s, "supported device:\n");
	for (i = 0; i < dmc_mon->port_num; i++) {
		if (dmc_mon->port[i].port_id >= PORT_MAJOR &&
		   !dmc_dev_is_byte(dmc_mon))
			break;
		s += sprintf(buf + s, "%2d : %s\n",
			dmc_mon->port[i].port_id,
			dmc_mon->port[i].port_name);
	}
	return s;
}
static CLASS_ATTR_RW(device);

static ssize_t dump_show(struct class *cla,
			 struct class_attribute *attr, char *buf)
{
	return dump_dmc_reg(buf);
}
static CLASS_ATTR_RO(dump);

static ssize_t debug_store(struct class *cla,
			  struct class_attribute *attr,
			  const char *buf, size_t count)
{
	int val;
	char string[128];

	if (sscanf(buf, "%s %d", string, &val) != 2) {
		pr_info("invalid input:%s\n", buf);
		return count;
	}

	if (strstr(string, "write") == string) {
		if (val)
			dmc_mon->debug |= DMC_DEBUG_WRITE;
		else
			dmc_mon->debug &= ~DMC_DEBUG_WRITE;
	} else if (strstr(string, "read") == string) {
		if (val)
			dmc_mon->debug |= DMC_DEBUG_READ;
		else
			dmc_mon->debug &= ~DMC_DEBUG_READ;
	} else if (strstr(string, "cma") == string) {
		if (val)
			dmc_mon->debug |= DMC_DEBUG_CMA;
		else
			dmc_mon->debug &= ~DMC_DEBUG_CMA;
	} else if (strstr(string, "same") == string) {
		if (val)
			dmc_mon->debug |= DMC_DEBUG_SAME;
		else
			dmc_mon->debug &= ~DMC_DEBUG_SAME;
	} else if (strstr(string, "include") == string) {
		if (dmc_dev_is_byte(dmc_mon)) {
			if (val)
				dmc_mon->debug |= DMC_DEBUG_INCLUDE;
			else
				dmc_mon->debug &= ~DMC_DEBUG_INCLUDE;
		} else {
			pr_emerg("dmc not support include set\n");
		}
	} else if (strstr(string, "trace") == string) {
		if (val)
			dmc_mon->debug |= DMC_DEBUG_TRACE;
		else
			dmc_mon->debug &= ~DMC_DEBUG_TRACE;
	} else if (strstr(string, "suspend") == string) {
		if (val) {
			dmc_mon->debug |= DMC_DEBUG_SUSPEND;
			if (dmc_irq_set(NULL, 0, 0) < 0)
				pr_emerg("free dmc irq error\n");
		} else {
			dmc_mon->debug &= ~DMC_DEBUG_SUSPEND;
			if (dmc_irq_set(NULL, 0, 1) < 0)
				pr_emerg("request dmc irq error\n");
		}
	} else if (strstr(string, "serror") == string) {
		if (val) {
			dmc_mon->debug |= DMC_DEBUG_SERROR;
			if (dmc_mon->ops && dmc_mon->ops->reg_control)
				dmc_mon->ops->reg_control(NULL, 'c', NULL);
			serror_dump_dmc_reg();
		} else {
			dmc_mon->debug &= ~DMC_DEBUG_SERROR;
		}
	} else if (strstr(string, "irq") == string) {
		if (val)
			dmc_mon->debug |= DMC_DEBUG_IRQ_THREAD;
		else
			dmc_mon->debug &= ~DMC_DEBUG_IRQ_THREAD;
	} else {
		pr_info("invalid param name:%s\n", buf);
		return count;
	}

	dmc_set_monitor(dmc_mon->addr_start, dmc_mon->addr_end, dmc_mon->device, 1);

	return count;
}

static ssize_t debug_show(struct class *cla,
			 struct class_attribute *attr, char *buf)
{
	int s = 0;

	s += sprintf(buf + s, "dmc version: %s\n", DMC_VERSION);
	s += sprintf(buf + s, "debug value: 0x%02x\n", dmc_mon->debug);
	s += sprintf(buf + s, "write:   write monitor:%d\n",
			dmc_mon->debug & DMC_DEBUG_WRITE ? 1 : 0);
	s += sprintf(buf + s, "read:    read monitor :%d\n",
			dmc_mon->debug & DMC_DEBUG_READ ? 1 : 0);
	s += sprintf(buf + s, "cma:     cma access show :%d\n",
			dmc_mon->debug & DMC_DEBUG_CMA ? 1 : 0);
	s += sprintf(buf + s, "same:    same access show :%d\n",
			dmc_mon->debug & DMC_DEBUG_SAME ? 1 : 0);
	s += sprintf(buf + s, "include: include mode :%d\n",
			dmc_mon->debug & DMC_DEBUG_INCLUDE ? 1 : 0);
	s += sprintf(buf + s, "trace:   trace print :%d\n",
			dmc_mon->debug & DMC_DEBUG_TRACE ? 1 : 0);
	s += sprintf(buf + s, "suspend: suspend debug :%d\n",
			dmc_mon->debug & DMC_DEBUG_SUSPEND ? 1 : 0);
	s += sprintf(buf + s, "serror:  serror debug :%d\n",
			dmc_mon->debug & DMC_DEBUG_SERROR ? 1 : 0);
	s += sprintf(buf + s, "irq:     irq_thread :%d, time:%ld, ratio:%d, irq_usleep:%ld, recheck:%ld\n",
				dmc_mon->debug & DMC_DEBUG_IRQ_THREAD ? 1 : 0,
				init_dmc_irq_check_ns, init_dmc_irq_ratio,
				init_dmc_irq_usleep, init_thread_recheck_ns);
	s += sprintf(buf + s, "pr_limit: printk ratelimit :%d\n", DMC_RATELIMIT_BURST);

	return s;
}
static CLASS_ATTR_RW(debug);

#if IS_ENABLED(CONFIG_AMLOGIC_DMC_DEV_ACCESS)
static ssize_t dev_access_store(struct class *cla,
			  struct class_attribute *attr,
			  const char *buf, size_t count)
{
	int ret;
	unsigned long addr = 0, size = 0;
	int id = 0;

	ret = sscanf(buf, "%d %lx %lx", &id, &addr, &size);
	if (ret != 3) {
		pr_info("input param num should be 3 (id addr size)\n");
		return count;
	}
	dmc_dev_access(id, addr, size);

	return count;
}

static ssize_t dev_access_show(struct class *cla,
			 struct class_attribute *attr, char *buf)
{
	return show_dmc_notifier_list(buf);
}
static CLASS_ATTR_RW(dev_access);
#endif

static ssize_t cmdline_show(struct class *cla,
			 struct class_attribute *attr, char *buf)
{
	int count = 0;
	int i = 0;

	count += sprintf(buf + count,
			 "setenv initargs $initargs dmc_monitor=0x%lx,0x%lx,0x%llx,0x%x",
			 dmc_mon->addr_start, dmc_mon->addr_end, dmc_mon->device, dmc_mon->debug);
	if (dmc_mon->filter.num) {
		count += sprintf(buf + count, " dmc_filter=%s", dmc_mon->filter.name[0]);
		for (i = 1; i < dmc_mon->filter.num; i++)
			count += sprintf(buf + count, ",%s", dmc_mon->filter.name[i]);
	}
	count += sprintf(buf + count, ";saveenv;reset;\n");
	return count;
}
static CLASS_ATTR_RO(cmdline);

static ssize_t filter_store(struct class *cla,
			  struct class_attribute *attr,
			  const char *buf, size_t count)
{
	if (!strncmp(buf, "none", 4))
		memset(&dmc_mon->filter, 0, sizeof(dmc_mon->filter));
	else
		set_dmc_filter((char *)buf);

	return count;
}

static ssize_t filter_show(struct class *cla,
			 struct class_attribute *attr, char *buf)
{
	unsigned int count = 0;
	int i;

	count += sprintf(buf + count, "filter list:\n");
	for (i = 0; i < dmc_mon->filter.num; i++)
		count += sprintf(buf + count, "%s\n", dmc_mon->filter.name[i]);

	return count;
}
static CLASS_ATTR_RW(filter);

static ssize_t reg_analysis_store(struct class *cla,
			  struct class_attribute *attr,
			  const char *buf, size_t count)
{
	char info[1024];
	int ret = 0;

	if (dmc_mon->ops && dmc_mon->ops->reg_control)
		ret = dmc_mon->ops->reg_control((char *)buf, 'a', info);
	if (ret)
		pr_emerg("%s", info);

	return count;
}
static CLASS_ATTR_WO(reg_analysis);

static struct attribute *dmc_monitor_attrs[] = {
	&class_attr_range.attr,
	&class_attr_device.attr,
	&class_attr_dump.attr,
	&class_attr_debug.attr,
	&class_attr_cmdline.attr,
	&class_attr_filter.attr,
	&class_attr_reg_analysis.attr,
#if IS_ENABLED(CONFIG_AMLOGIC_DMC_DEV_ACCESS)
	&class_attr_dev_access.attr,
#endif
	NULL
};
ATTRIBUTE_GROUPS(dmc_monitor);

static struct class dmc_monitor_class = {
	.name = "dmc_monitor",
	.class_groups = dmc_monitor_groups,
};

static void __init get_dmc_ops(int chip, struct dmc_monitor *mon)
{
	/* set default parameters */
	mon->mon_number = 1;
	mon->debug |= DMC_DEBUG_INCLUDE;
	mon->debug |= DMC_DEBUG_WRITE;
	mon->debug &= ~DMC_DEBUG_TRACE;

	switch (chip) {
#ifdef CONFIG_AMLOGIC_DMC_MONITOR_G12
	case DMC_TYPE_G12A:
	case DMC_TYPE_G12B:
	case DMC_TYPE_SM1:
	case DMC_TYPE_TL1:
		mon->ops = &g12_dmc_mon_ops;
		break;
#endif
#ifdef CONFIG_AMLOGIC_DMC_MONITOR_C1
	case DMC_TYPE_C1:
		mon->ops = &c1_dmc_mon_ops;
		break;
#endif
#ifdef CONFIG_AMLOGIC_DMC_MONITOR_GX
	case DMC_TYPE_GXBB:
	case DMC_TYPE_GXTVBB:
	case DMC_TYPE_GXL:
	case DMC_TYPE_GXM:
	case DMC_TYPE_TXL:
	case DMC_TYPE_TXLX:
	case DMC_TYPE_AXG:
	case DMC_TYPE_GXLX:
	case DMC_TYPE_TXHD:
		mon->ops = &gx_dmc_mon_ops;
		break;
#endif
#ifdef CONFIG_AMLOGIC_DMC_MONITOR_TM2
	case DMC_TYPE_TM2:
		if (is_meson_rev_b())
			mon->ops = &tm2_dmc_mon_ops;
		else
		#ifdef CONFIG_AMLOGIC_DMC_MONITOR_G12
			mon->ops = &g12_dmc_mon_ops;
		#else
			#error need support for revA
		#endif
		break;

	case DMC_TYPE_SC2:
		mon->ops = &tm2_dmc_mon_ops;
		break;

	case DMC_TYPE_T5:
	case DMC_TYPE_T5D:
		mon->ops = &tm2_dmc_mon_ops;
		break;
#endif
#ifdef CONFIG_AMLOGIC_DMC_MONITOR_T7
	case DMC_TYPE_T7:
	case DMC_TYPE_T3:
		mon->ops = &t7_dmc_mon_ops;
		mon->configs |= DMC_DEVICE_8BIT;
		mon->mon_number = 2;
		break;
	case DMC_TYPE_P1:
		mon->ops = &t7_dmc_mon_ops;
		mon->configs |= DMC_DEVICE_8BIT;
		mon->mon_number = 4;
		break;
#endif
#ifdef CONFIG_AMLOGIC_DMC_MONITOR_S4
	case DMC_TYPE_S4:
	case DMC_TYPE_T5W:
	case DMC_TYPE_A5:
		mon->ops = &s4_dmc_mon_ops;
		break;
#endif
#ifdef CONFIG_AMLOGIC_DMC_MONITOR_S5
	case DMC_TYPE_S5:
		mon->ops = &s5_dmc_mon_ops;
		mon->configs |= DMC_DEVICE_8BIT;
		mon->mon_number = 4;
		break;
#endif
#ifdef CONFIG_AMLOGIC_DMC_MONITOR_A4
	case DMC_TYPE_A4:
		mon->ops = &a4_dmc_mon_ops;
		mon->configs |= DMC_DEVICE_8BIT;
		mon->mon_number = 1;
		break;
#endif
	default:
		pr_err("%s, Can't find ops for chip:%x\n", __func__, chip);
		break;
	}
}

static int __init dmc_monitor_probe(struct platform_device *pdev)
{
	int r = 0, ports, vpu_ports, i;
	unsigned int tmp;
	struct device_node *node;
	struct ddr_port_desc *desc = NULL;
	struct vpu_sub_desc *vpu_desc = NULL;
	struct resource *res;

	pr_info("dmc version:%s\n", DMC_VERSION);
	dmc_mon = devm_kzalloc(&pdev->dev, sizeof(*dmc_mon), GFP_KERNEL);
	if (!dmc_mon)
		return -ENOMEM;

	tmp = (unsigned long)of_device_get_match_data(&pdev->dev);
	pr_info("%s, chip type:%d\n", __func__, tmp);
	ports = ddr_find_port_desc(tmp, &desc);
	if (ports < 0) {
		pr_err("can't get port desc\n");
		dmc_mon = NULL;
		return -EINVAL;
	}
	dmc_mon->chip = tmp;
	dmc_mon->port_num = ports;
	dmc_mon->port = desc;
	get_dmc_ops(dmc_mon->chip, dmc_mon);

	vpu_ports = dmc_find_port_sub(tmp, &vpu_desc);
	if (vpu_ports < 0) {
		dmc_mon->vpu_port = NULL;
		dmc_mon->vpu_port_num = 0;
	}
	dmc_mon->vpu_port_num = vpu_ports;
	dmc_mon->vpu_port = vpu_desc;

	node = pdev->dev.of_node;
	r = of_property_read_u32(node, "reg_base", &tmp);
	if (r < 0) {
		pr_err("can't find iobase\n");
		dmc_mon = NULL;
		return -EINVAL;
	}

	dmc_mon->sec_base = tmp;

	/* for register not in secure world */
	for (i = 0; i < dmc_mon->mon_number; i++) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		if (res) {
			dmc_mon->mon_comm[i].io_base = res->start;
			dmc_mon->mon_comm[i].io_mem = ioremap(res->start, res->end - res->start);
		}
	}

	r = class_register(&dmc_monitor_class);
	if (r) {
		pr_err("regist dmc_monitor_class failed\n");
		dmc_mon = NULL;
		return -EINVAL;
	}

	if (init_dmc_debug)
		dmc_mon->debug = init_dmc_debug;

	if (init_dmc_irq_thread_en)
		dmc_mon->debug |= DMC_DEBUG_IRQ_THREAD;

	if (strlen(dmc_filter_early_buf))
		set_dmc_filter(dmc_filter_early_buf);

	dmc_enabled_reserved_memory(pdev, dmc_mon);

	if (init_dev_mask) {
		dmc_set_monitor(init_start_addr,
				init_end_addr, init_dev_mask, 1);
	}

	set_dump_dmc_func(serror_dump_dmc_reg);

	if (dmc_mon->debug & DMC_DEBUG_SUSPEND) {
		if (dmc_irq_set(node, 1, 0) < 0)
			pr_emerg("get dmc irq failed\n");
	} else {
		if (dmc_irq_set(node, 1, 1) < 0)
			pr_emerg("request dmc irq failed\n");
	}
	if (dmc_mon->debug & DMC_DEBUG_SERROR) {
		if (dmc_mon->ops && dmc_mon->ops->reg_control)
			dmc_mon->ops->reg_control(NULL, 'c', NULL);
		serror_dump_dmc_reg();
	}

	return 0;
}

static int dmc_monitor_remove(struct platform_device *pdev)
{
	class_unregister(&dmc_monitor_class);
	dmc_mon = NULL;
	set_dump_dmc_func(NULL);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id dmc_monitor_match[] = {
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
	{
		.compatible = "amlogic,dmc_monitor-gxl",
		.data = (void *)DMC_TYPE_GXL,	/* chip id */
	},
	{
		.compatible = "amlogic,dmc_monitor-gxm",
		.data = (void *)DMC_TYPE_GXM,
	},
	{
		.compatible = "amlogic,dmc_monitor-gxlx",
		.data = (void *)DMC_TYPE_GXLX,
	},
	{
		.compatible = "amlogic,dmc_monitor-txl",
		.data = (void *)DMC_TYPE_TXL,
	},
	{
		.compatible = "amlogic,dmc_monitor-txlx",
		.data = (void *)DMC_TYPE_TXLX,
	},
	{
		.compatible = "amlogic,dmc_monitor-txhd",
		.data = (void *)DMC_TYPE_TXHD,
	},
	{
		.compatible = "amlogic,dmc_monitor-c1",
		.data = (void *)DMC_TYPE_C1,
	},
#endif
	{
		.compatible = "amlogic,dmc_monitor-axg",
		.data = (void *)DMC_TYPE_AXG,
	},
	{
		.compatible = "amlogic,dmc_monitor-g12a",
		.data = (void *)DMC_TYPE_G12A,
	},
	{
		.compatible = "amlogic,dmc_monitor-sm1",
		.data = (void *)DMC_TYPE_SM1,
	},
	{
		.compatible = "amlogic,dmc_monitor-g12b",
		.data = (void *)DMC_TYPE_G12B,
	},
	{
		.compatible = "amlogic,dmc_monitor-tm2",
		.data = (void *)DMC_TYPE_TM2,
	},
	{
		.compatible = "amlogic,dmc_monitor-t5",
		.data = (void *)DMC_TYPE_T5,
	},
	{
		.compatible = "amlogic,dmc_monitor-t5d",
		.data = (void *)DMC_TYPE_T5D,
	},
	{
		.compatible = "amlogic,dmc_monitor-t5w",
		.data = (void *)DMC_TYPE_T5W,
	},
	{
		.compatible = "amlogic,dmc_monitor-t7",
		.data = (void *)DMC_TYPE_T7,
	},
	{
		.compatible = "amlogic,dmc_monitor-t3",
		.data = (void *)DMC_TYPE_T3,
	},
	{
		.compatible = "amlogic,dmc_monitor-p1",
		.data = (void *)DMC_TYPE_P1,
	},
	{
		.compatible = "amlogic,dmc_monitor-s4",
		.data = (void *)DMC_TYPE_S4,
	},
	{
		.compatible = "amlogic,dmc_monitor-sc2",
		.data = (void *)DMC_TYPE_SC2,
	},
	{
		.compatible = "amlogic,dmc_monitor-a5",
		.data = (void *)DMC_TYPE_A5,
	},
	{
		.compatible = "amlogic,dmc_monitor-s5",
		.data = (void *)DMC_TYPE_S5,
	},
	{
		.compatible = "amlogic,dmc_monitor-a4",
		.data = (void *)DMC_TYPE_A4,
	},
	{
		.compatible = "amlogic,dmc_monitor-tl1",
		.data = (void *)DMC_TYPE_TL1,
	},
	{}
};
#endif

static struct platform_driver dmc_monitor_driver = {
	.driver = {
		.name  = "dmc_monitor",
		.owner = THIS_MODULE,
	},
	.remove = dmc_monitor_remove,
};

int __init dmc_monitor_init(void)
{
#ifdef CONFIG_OF
	const struct of_device_id *match_id;

	match_id = dmc_monitor_match;
	dmc_monitor_driver.driver.of_match_table = match_id;
#endif

	platform_driver_probe(&dmc_monitor_driver, dmc_monitor_probe);
	return 0;
}

void dmc_monitor_exit(void)
{
	platform_driver_unregister(&dmc_monitor_driver);
}

