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
#include <linux/arm-smccc.h>
#include <linux/amlogic/dmc_monitor.h>
#include <linux/amlogic/ddr_port.h>

static struct dmc_monitor *dmc_mon;

unsigned long dmc_rw(unsigned long addr, unsigned long value, int rw)
{
	struct arm_smccc_res smccc;

	arm_smccc_smc(DMC_MON_RW, addr + dmc_mon->io_base,
		      value, rw, 0, 0, 0, 0, &smccc);

	return smccc.a0;
}

static int dev_name_to_id(const char *dev_name)
{
	int i, len;

	for (i = 0; i < dmc_mon->port_num; i++) {
		if (dmc_mon->port[i].port_id >= PORT_MAJOR)
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

	if (dmc_mon->ops && dmc_mon->ops->dump_reg)
		sz += dmc_mon->ops->dump_reg(buf);
	sz += sprintf(buf + sz, "IO_BASE:%lx\n", dmc_mon->io_base);
	sz += sprintf(buf + sz, "RANGE:%lx - %lx\n",
		      dmc_mon->addr_start, dmc_mon->addr_end);
	sz += sprintf(buf + sz, "MONITOR DEVICE:\n");
	for (i = 0; i < sizeof(dmc_mon->device) * 8; i++) {
		if (dmc_mon->device & (1 << i))
			sz += sprintf(buf + sz, "    %s\n", to_ports(i));
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

int dmc_set_monitor(unsigned long start, unsigned long end,
		    unsigned long dev_mask, int en)
{
	if (!dmc_mon)
		return -EINVAL;

	dmc_mon->addr_start = start;
	dmc_mon->addr_end   = end;
	if (en)
		dmc_mon->device |= dev_mask;
	else
		dmc_mon->device &= ~(dev_mask);
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
	if (id < 0 || id >= BITS_PER_LONG)
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
		dmc_mon->device |= (1 << i);
	}
	dmc_set_monitor(dmc_mon->addr_start, dmc_mon->addr_end,
			dmc_mon->device, 1);

	return count;
}

static ssize_t device_show(struct class *cla,
			   struct class_attribute *attr, char *buf)
{
	int i, s = 0;

	s += sprintf(buf + s, "supported device:\n");
	for (i = 0; i < dmc_mon->port_num; i++) {
		if (dmc_mon->port[i].port_id >= PORT_MAJOR)
			break;
		s += sprintf(buf + s, "%2d:%s\n",
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
	case DMC_TYPE_TM2:
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

	irq = of_irq_get(node, 0);
	r = request_irq(irq, dmc_monitor_irq_handler,
			IRQF_SHARED, "dmc_monitor", pdev);
	if (r < 0) {
		pr_err("request irq failed:%d, r:%d\n", irq, r);
		dmc_mon = NULL;
		return -EINVAL;
	}
	r = class_register(&dmc_monitor_class);
	if (r) {
		pr_err("regist dmc_monitor_class failed\n");
		dmc_mon = NULL;
		return -EINVAL;
	}
	INIT_DELAYED_WORK(&dmc_mon->work, clear_irq_work);
	schedule_delayed_work(&dmc_mon->work, HZ);

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
static const struct of_device_id dmc_monitor_match[] __initconst = {
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

static int __init dmc_monitor_init(void)
{
#ifdef CONFIG_OF
	const struct of_device_id *match_id;

	match_id = dmc_monitor_match;
	dmc_monitor_driver.driver.of_match_table = match_id;
#endif

	return platform_driver_probe(&dmc_monitor_driver, dmc_monitor_probe);
}

static void __exit dmc_monitor_exit(void)
{
	platform_driver_unregister(&dmc_monitor_driver);
}

module_init(dmc_monitor_init);
module_exit(dmc_monitor_exit);
MODULE_DESCRIPTION("amlogic dmc monitor driver");
MODULE_LICENSE("GPL");
