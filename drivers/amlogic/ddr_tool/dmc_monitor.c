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
#include <linux/amlogic/page_trace.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/arm-smccc.h>
#include <linux/highmem.h>
#include "dmc_monitor.h"
#include "ddr_port.h"

struct dmc_monitor *dmc_mon;

static unsigned long init_dev_mask;
static unsigned long init_start_addr;
static unsigned long init_end_addr;

static int early_dmc_param(char *buf)
{
	unsigned long s_addr, e_addr, mask;
	/*
	 * Patten:  dmc_montiro=[start_addr],[end_addr],[mask]
	 * Example: dmc_monitor=0x00000000,0x20000000,0x7fce
	 */
	if (!buf)
		return -EINVAL;

	if (sscanf(buf, "%lx,%lx,%lx", &s_addr, &e_addr, &mask) != 3)
		return -EINVAL;

	init_start_addr = s_addr;
	init_end_addr   = e_addr;
	init_dev_mask   = mask;

	pr_info("%s, buf:%s, %lx-%lx, %lx\n",
		__func__, buf, s_addr, e_addr, mask);

	return 0;
}

#ifdef MODULE
static char *dmc_param = "";

static int set_dmc_param(const char *val, const struct kernel_param *kp)
{
	param_set_charp(val, kp);

	return early_dmc_param(dmc_param);
}

static const struct kernel_param_ops dmc_param_ops = {
	.set = set_dmc_param,
	.get = param_get_charp,
};

module_param_cb(dmc_monitor, &dmc_param_ops, &dmc_param, 0644);
MODULE_PARM_DESC(jtag, "dmc_monitor");
#else
early_param("dmc_monitor", early_dmc_param);
#endif


void show_violation_mem(unsigned long addr)
{
	struct page *page;
	unsigned long *p, *q;

	if (!pfn_valid(__phys_to_pfn(addr)))
		return;

	page = phys_to_page(addr);
	p = kmap_atomic(page);
	if (!p)
		return;

	q = p + ((addr & (PAGE_SIZE - 1)) / sizeof(*p));
	pr_emerg(DMC_TAG "[%08lx]:%016lx, f:%8lx, m:%p, a:%ps\n",
		 (unsigned long)q, *q, page->flags & 0xffffffff,
		 page->mapping,
		 (void *)get_page_trace(page));
	kunmap_atomic(p);
}

unsigned long dmc_prot_rw(void  __iomem *base,
			  unsigned long off, unsigned long value, int rw)
{
	if (base) {
		if (rw == DMC_WRITE) {
			writel(value, base + off);
			return 0;
		} else {
			return readl(base + off);
		}
	} else {
		return dmc_rw(off + dmc_mon->io_base, value, rw);
	}
}

static inline int dual_dmc(struct dmc_monitor *mon)
{
	return mon->configs & DUAL_DMC;
}

static int dev_name_to_id(const char *dev_name)
{
	int i, len;

	for (i = 0; i < dmc_mon->port_num; i++) {
		if (dmc_mon->port[i].port_id >= PORT_MAJOR &&
		    !dual_dmc(dmc_mon))
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
	int i;

	for (i = 0; i < dmc_mon->port_num; i++) {
		if (dmc_mon->port[i].port_id == id)
			return dmc_mon->port[i].port_name;
	}
	return NULL;
}

char *to_sub_ports(int mid, int sid, char *id_str)
{
	int i;

	if (dual_dmc(dmc_mon))	/* not supported */
		return NULL;

	/* 7 is device port id */
	if (mid == 7) {
		for (i = 0; i < dmc_mon->port_num; i++) {
			if (dmc_mon->port[i].port_id == sid + PORT_MAJOR)
				return dmc_mon->port[i].port_name;
		}
	}
	sprintf(id_str, "%2d", sid);

	return id_str;
}

unsigned int get_all_dev_mask(void)
{
	unsigned int ret = 0;
	int i;

	if (dual_dmc(dmc_mon))	/* not supported */
		return 0;

	for (i = 0; i < PORT_MAJOR; i++) {
		if (dmc_mon->port[i].port_id >= PORT_MAJOR)
			break;
		ret |= (1 << dmc_mon->port[i].port_id);
	}
	return ret;
}

static unsigned int get_other_dev_mask(void)
{
	unsigned int ret = 0;
	int i;

	if (dual_dmc(dmc_mon))	/* not supported */
		return 0;

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
		    strstr(dmc_mon->port[i].port_name, "DEVICE"))
			continue;

		ret |= (1 << dmc_mon->port[i].port_id);
	}
	return ret;
}

size_t dump_dmc_reg(char *buf)
{
	size_t sz = 0, i;
	unsigned char dev;
	u64 devices;

	if (dmc_mon->ops && dmc_mon->ops->dump_reg)
		sz += dmc_mon->ops->dump_reg(buf);
	sz += sprintf(buf + sz, "IO_BASE:%lx\n", dmc_mon->io_base);
	sz += sprintf(buf + sz, "RANGE:%lx - %lx\n",
		      dmc_mon->addr_start, dmc_mon->addr_end);
	sz += sprintf(buf + sz, "MONITOR DEVICE:\n");
	if (!dmc_mon->device)
		return sz;

	if (dual_dmc(dmc_mon)) {
		devices = dmc_mon->device;
		for (i = 0; i < sizeof(dmc_mon->device); i++) {
			dev = devices & 0xff;
			devices >>= 8ULL;
			if (dev)
				sz += sprintf(buf + sz, "    %s\n", to_ports(dev));
		}
	} else {
		for (i = 0; i < sizeof(dmc_mon->device) * 8; i++) {
			if (dmc_mon->device & (1 << i))
				sz += sprintf(buf + sz, "    %s\n", to_ports(i));
		}
	}

	return sz;
}

static irqreturn_t dmc_monitor_irq_handler(int irq, void *dev_instance)
{
	if (dmc_mon->ops && dmc_mon->ops->handle_irq)
		dmc_mon->ops->handle_irq(dmc_mon, dev_instance);

	return IRQ_HANDLED;
}

static void clear_irq_work(struct work_struct *work)
{
	/*
	 * DMC VIOLATION may happen very quickly and irq re-generated
	 * again before CPU leave IRQ mode, once this scenario happened,
	 * DMC protection would not generate IRQ again until we cleared
	 * it manually.
	 * Since no parameters used for irq handler, so we just call IRQ
	 * handler again to save code  size.
	 */
	dmc_monitor_irq_handler(0, NULL);
	schedule_delayed_work(&dmc_mon->work, HZ);
}

static int dmc_regulation_dev(unsigned long dev, int add)
{
	unsigned char *p, cur;
	int i, set;

	if (dual_dmc(dmc_mon)) {
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

	dmc_mon->addr_start = start;
	dmc_mon->addr_end   = end;
	dmc_regulation_dev(dev_mask, en);
	if (start < end && dmc_mon->ops && dmc_mon->ops->set_montor)
		return dmc_mon->ops->set_montor(dmc_mon);
	return -EINVAL;
}
EXPORT_SYMBOL(dmc_set_monitor);

int dmc_set_monitor_by_name(unsigned long start, unsigned long end,
			    const char *port_name, int en)
{
	long id;

	id = dev_name_to_id(port_name);
	if (id >= 0 && dual_dmc(dmc_mon))
		return dmc_set_monitor(start, end, id, en);
	else if (id < 0 || id >= BITS_PER_LONG)
		return -EINVAL;
	return dmc_set_monitor(start, end, 1UL << id, en);
}
EXPORT_SYMBOL(dmc_set_monitor_by_name);

void dmc_monitor_disable(void)
{
	if (dmc_mon->ops && dmc_mon->ops->disable)
		return dmc_mon->ops->disable(dmc_mon);
}
EXPORT_SYMBOL(dmc_monitor_disable);

static ssize_t range_show(struct class *cla,
			  struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%08lx - %08lx\n",
		       dmc_mon->addr_start, dmc_mon->addr_end);
}

static ssize_t range_store(struct class *cla,
			   struct class_attribute *attr,
			   const char *buf, size_t count)
{
	int ret;
	unsigned long start, end;

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

	if (!strncmp(buf, "none", 4)) {
		dmc_monitor_disable();
		return count;
	}

	if (dual_dmc(dmc_mon)) {
		if (!strncmp(buf, "exclude", 3)) {
			dmc_mon->configs &= ~POLICY_INCLUDE;
		} else {
			i = dev_name_to_id(buf);
			if (i < 0) {
				pr_info("bad device:%s\n", buf);
				return -EINVAL;
			}
			if (dmc_regulation_dev(i, 1))
				return -EINVAL;
		}
	} else {
		if (!strncmp(buf, "all", 3)) {
			dmc_mon->device = get_all_dev_mask();
		} else if (!strncmp(buf, "other", 5)) {
			dmc_mon->device = get_other_dev_mask();
		} else {
			i = dev_name_to_id(buf);
			if (i < 0) {
				pr_info("bad device:%s\n", buf);
				return -EINVAL;
			}
			dmc_regulation_dev(1 << i, 1);
		}
	}
	if (dmc_mon->addr_start < dmc_mon->addr_end && dmc_mon->ops &&
	     dmc_mon->ops->set_montor)
		dmc_mon->ops->set_montor(dmc_mon);

	return count;
}

static ssize_t device_show(struct class *cla,
			   struct class_attribute *attr, char *buf)
{
	int i, s = 0;

	s += sprintf(buf + s, "supported device:\n");
	for (i = 0; i < dmc_mon->port_num; i++) {
		if (dmc_mon->port[i].port_id >= PORT_MAJOR &&
		   !dual_dmc(dmc_mon))
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

static struct attribute *dmc_monitor_attrs[] = {
	&class_attr_range.attr,
	&class_attr_device.attr,
	&class_attr_dump.attr,
	NULL
};
ATTRIBUTE_GROUPS(dmc_monitor);

static struct class dmc_monitor_class = {
	.name = "dmc_monitor",
	.class_groups = dmc_monitor_groups,
};

static void __init get_dmc_ops(int chip, struct dmc_monitor *mon)
{
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

	case DMC_TYPE_T5:
	case DMC_TYPE_T5D:
		mon->ops = &tm2_dmc_mon_ops;
		break;
#endif
#ifdef CONFIG_AMLOGIC_DMC_MONITOR_T7
	case DMC_TYPE_T7:
		mon->ops = &t7_dmc_mon_ops;
		mon->configs |= DUAL_DMC;
		mon->configs |= POLICY_INCLUDE;
		break;
#endif
#ifdef CONFIG_AMLOGIC_DMC_MONITOR_S4
	case DMC_TYPE_S4:
		mon->ops = &s4_dmc_mon_ops;
		break;
#endif
	default:
		pr_err("%s, Can't find ops for chip:%x\n", __func__, chip);
		break;
	}
}

static int __init dmc_monitor_probe(struct platform_device *pdev)
{
	int r = 0, irq, ports;
	unsigned int tmp;
	struct device_node *node;
	struct ddr_port_desc *desc = NULL;
	struct resource *res;

	pr_info("%s\n", __func__);
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

	node = pdev->dev.of_node;
	r = of_property_read_u32(node, "reg_base", &tmp);
	if (r < 0) {
		pr_err("can't find iobase\n");
		dmc_mon = NULL;
		return -EINVAL;
	}

	dmc_mon->io_base = tmp;

	/* for register not in secure world */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res)
		dmc_mon->io_mem1 = ioremap(res->start, res->end - res->start);
	if (dual_dmc(dmc_mon)) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
		if (res)
			dmc_mon->io_mem2 = ioremap(res->start,
						   res->end - res->start);
	}

	irq = of_irq_get(node, 0);
	r = request_irq(irq, dmc_monitor_irq_handler,
			IRQF_SHARED, "dmc_monitor", dmc_mon->io_mem1);
	if (r < 0) {
		pr_err("request irq failed:%d, r:%d\n", irq, r);
		dmc_mon = NULL;
		return -EINVAL;
	}
	if (dual_dmc(dmc_mon)) {
		irq = of_irq_get(node, 1);
		r = request_irq(irq, dmc_monitor_irq_handler,
				IRQF_SHARED, "dmc_monitor", dmc_mon->io_mem2);
		if (r < 0) {
			pr_err("request irq failed:%d, r:%d\n", irq, r);
			dmc_mon = NULL;
			return -EINVAL;
		}
	}

	r = class_register(&dmc_monitor_class);
	if (r) {
		pr_err("regist dmc_monitor_class failed\n");
		dmc_mon = NULL;
		return -EINVAL;
	}
	INIT_DELAYED_WORK(&dmc_mon->work, clear_irq_work);
	schedule_delayed_work(&dmc_mon->work, HZ);

	if (init_dev_mask)
		dmc_set_monitor(init_start_addr,
				init_end_addr, init_dev_mask, 1);

	return 0;
}

static int dmc_monitor_remove(struct platform_device *pdev)
{
	cancel_delayed_work_sync(&dmc_mon->work);
	class_unregister(&dmc_monitor_class);
	dmc_mon = NULL;
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id dmc_monitor_match[] = {
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
		.compatible = "amlogic,dmc_monitor-tl1",
		.data = (void *)DMC_TYPE_TL1,
	},
	{
		.compatible = "amlogic,dmc_monitor-c1",
		.data = (void *)DMC_TYPE_C1,
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
		.compatible = "amlogic,dmc_monitor-t7",
		.data = (void *)DMC_TYPE_T7,
	},
	{
		.compatible = "amlogic,dmc_monitor-s4",
		.data = (void *)DMC_TYPE_S4,
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

