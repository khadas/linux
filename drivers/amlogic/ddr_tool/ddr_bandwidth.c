// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/sysfs.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/sched/clock.h>
#include "ddr_bandwidth.h"
#include "dmc.h"

static struct ddr_bandwidth *aml_db;

static void cal_ddr_usage(struct ddr_bandwidth *db, struct ddr_grant *dg)
{
	u64 mul, mbw; /* avoid overflow */
	unsigned long i, cnt, freq = 0, flags;

	if (db->mode == MODE_AUTODETECT) { /* ignore mali bandwidth */
		static int count;
		unsigned int grant = dg->all_grant;

		if (db->mali_port[0] >= 0)
			grant -= dg->channel_grant[0];
		if (db->mali_port[1] >= 0)
			grant -= dg->channel_grant[1];
		if (grant > db->threshold) {
			if (count >= 2) {
				if (db->busy == 0) {
					db->busy = 1;
					schedule_work(&db->work_bandwidth);
				}
			} else {
				count++;
			}
		} else if (count > 0) {
			if (count >= 2) {
				db->busy = 0;
				schedule_work(&db->work_bandwidth);
			}
			count = 0;
		}
		return;
	}
	if (db->ops && db->ops->get_freq)
		freq = db->ops->get_freq(db);
	mul  = dg->all_grant;
	mul *= 10000ULL;
	do_div(mul, db->bytes_per_cycle);
	cnt  = db->clock_count;
	do_div(mul, cnt);
	db->cur_sample.total_usage = mul;
	if (freq) {
		/* calculate in KB */
		mbw  = (u64)freq * db->bytes_per_cycle;
		mbw /= 1024;	/* theoretic max bandwidth */
		mul  = dg->all_grant;
		mul *= freq;
		mul /= 1024;
		do_div(mul, cnt);
		if (mul >= mbw) {
			/* sample may overflow if irq tick changed, ignore it */
			pr_emerg("%s, bandwidth:%lld large than max :%lld\n",
				 __func__, mul, mbw);
			return;
		}
		db->cur_sample.total_bandwidth = mul;
		db->cur_sample.tick = sched_clock();
		for (i = 0; i < db->channels; i++) {
			mul  = dg->channel_grant[i];
			mul *= freq;
			mul /= 1024;
			do_div(mul, cnt);
			db->cur_sample.bandwidth[i] = mul;
		}
	}

	if (db->stat_flag)  /* stop update usage stat if flag set */
		return;

	spin_lock_irqsave(&aml_db->lock, flags);
	/* update max sample */
	if (db->cur_sample.total_bandwidth > db->max_sample.total_bandwidth)
		memcpy(&db->max_sample, &db->cur_sample,
		       sizeof(struct ddr_bandwidth_sample));

	/* update usage statistics */
	db->usage_stat[db->cur_sample.total_usage / 1000]++;

	/* collect for average bandwidth calculate */
	db->avg.avg_bandwidth += db->cur_sample.total_bandwidth;
	db->avg.avg_usage     += db->cur_sample.total_usage;
	for (i = 0; i < db->channels; i++) {
		db->avg.avg_port[i]  += db->cur_sample.bandwidth[i];
		if (db->cur_sample.bandwidth[i] > db->avg.max_bandwidth[i])
			db->avg.max_bandwidth[i] = db->cur_sample.bandwidth[i];
	}
	db->avg.sample_count++;
	spin_unlock_irqrestore(&aml_db->lock, flags);
}

static irqreturn_t dmc_irq_handler(int irq, void *dev_instance)
{
	struct ddr_bandwidth *db;
	struct ddr_grant dg = {0};

	db = (struct ddr_bandwidth *)dev_instance;
	if (db->ops && db->ops->handle_irq) {
		if (!db->ops->handle_irq(db, &dg))
			cal_ddr_usage(db, &dg);
	}
	return IRQ_HANDLED;
}

unsigned int aml_get_ddr_usage(void)
{
	unsigned int ret = 0;

	if (aml_db)
		ret = aml_db->cur_sample.total_usage;

	return ret;
}
EXPORT_SYMBOL(aml_get_ddr_usage);

static char *find_port_name(int id)
{
	int i;

	if (!aml_db->real_ports || !aml_db->port_desc)
		return NULL;

	for (i = 0; i < aml_db->real_ports; i++) {
		if (aml_db->port_desc[i].port_id == id)
			return aml_db->port_desc[i].port_name;
	}
	return NULL;
}

static int dual_dmc(struct ddr_bandwidth *db)
{
	if (db && (db->soc_feature & DUAL_DMC))
		return 1;
	return 0;
}

static void clear_bandwidth_statistics(void)
{
	unsigned long flags;

	/* clear flag and start statistics */
	spin_lock_irqsave(&aml_db->lock, flags);
	memset(&aml_db->max_sample, 0, sizeof(struct ddr_bandwidth_sample));
	memset(aml_db->usage_stat, 0, 10 * sizeof(int));
	memset(&aml_db->avg, 0, sizeof(struct ddr_avg_bandwidth));
	spin_unlock_irqrestore(&aml_db->lock, flags);
}

static int format_port(char *buf, u64 port_mask)
{
	u64 t;
	int i, size = 0;
	char *name, dev;

	if (!port_mask)
		return 0;

	if (dual_dmc(aml_db)) {
		for (i = 0; i < 3; i++) {
			dev = port_mask & 0xff;
			port_mask >>= 8;
			if (!dev)
				continue;
			name = find_port_name(dev);
			if (!name)
				continue;
			size += sprintf(buf + size, "      %s\n", name);
		}
	} else {
		for (i = 0; i < sizeof(u64) * 8; i++) {
			t = 1ULL << i;
			if (port_mask & t) {
				name = find_port_name(i);
				if (!name)
					continue;
				size += sprintf(buf + size, "      %s\n", name);
			}
		}
	}
	return size;
}

static ssize_t port_show(struct class *cla,
			 struct class_attribute *attr, char *buf)
{
	int s = 0, i;

	if (aml_db->ops && !aml_db->ops->config_range) {
		for (i = 0; i < aml_db->channels; i++) {
			s += sprintf(buf + s, "ch %d:%16llx: ports:\n",
					i, aml_db->port[i]);
			s += format_port(buf + s, aml_db->port[i]);
		}
		return s;
	}
	for (i = 0; i < aml_db->channels; i++) {
		s += sprintf(buf + s, "ch %d:%16llx, [%08lx-%08lx], ports:\n",
				i, aml_db->port[i],
				aml_db->range[i].start,
				aml_db->range[i].end);
		s += format_port(buf + s, aml_db->port[i]);
	}
	return s;
}

static int dual_dmc_port_set(struct ddr_bandwidth *db, int port, int ch)
{
	int i, t;
	u64 cur;

	cur = db->port[ch];
	for (i = 0; i < 3; i++) {
		t = cur & 0xff;
		cur >>= 8;
		if (!t)
			break;
	}
	if (i >= 3) {
		pr_err("each channel only support 3 ports\n");
		return -ERANGE;
	}
	port &= 0xff;
	db->port[ch] = (db->port[ch] | (port << (i * 8)));
	return 0;
}

static ssize_t port_store(struct class *cla,
			  struct class_attribute *attr,
			  const char *buf, size_t count)
{
	int ch = 0, port = 0, paras = -1;
	unsigned long start = 0xffffffff, end;

	paras = sscanf(buf, "%d:%d %lx-%lx", &ch, &port, &start, &end);
	if (paras < 4) {
		paras = sscanf(buf, "%d:%d", &ch, &port);
		if (paras < 2) {
			pr_info("invalid input:%s\n", buf);
			return count;
		}
	}
	if (ch >= MAX_CHANNEL || ch < 0 ||
	    (ch && aml_db->cpu_type < DMC_TYPE_GXTVBB) ||
	    port > MAX_PORTS) {
		pr_info("invalid channel %d or port %d\n", ch, port);
		return count;
	}

	if (aml_db->ops && aml_db->ops->config_port) {
		if (dual_dmc(aml_db)) {
			if (port < 0) /* clear port set */
				aml_db->port[ch] = 0;
			else
				if (dual_dmc_port_set(aml_db, port, ch))
					return -ERANGE;
			aml_db->ops->config_port(aml_db, ch, aml_db->port[ch]);
		} else {
			if (port < 0) /* clear port set */
				aml_db->port[ch] = 0;
			else
				aml_db->port[ch] |= 1ULL << port;
			aml_db->ops->config_port(aml_db, ch, port);
		}
	}

	if (paras == 4 && aml_db->ops &&
		aml_db->ops->config_range && start != 0xffffffffffffffffULL) {
		aml_db->ops->config_range(aml_db, ch, start, end);
		aml_db->range[ch].start = start;
		aml_db->range[ch].end   = end;
	}

	return count;
}
static CLASS_ATTR_RW(port);

static ssize_t busy_show(struct class *cla,
			 struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", aml_db->busy);
}
static CLASS_ATTR_RO(busy);

static ssize_t threshold_show(struct class *cla,
			      struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n",
			aml_db->threshold / aml_db->bytes_per_cycle
			/ (aml_db->clock_count / 10000));
}

static ssize_t threshold_store(struct class *cla,
			       struct class_attribute *attr,
			       const char *buf, size_t count)
{
	long val = 0;

	if (kstrtoul(buf, 10, &val)) {
		pr_info("invalid input:%s\n", buf);
		return 0;
	}

	if (val > 10000)
		val = 10000;
	aml_db->threshold = val * aml_db->bytes_per_cycle
			* (aml_db->clock_count / 10000);
	return count;
}
static CLASS_ATTR_RW(threshold);

static ssize_t mode_show(struct class *cla,
			 struct class_attribute *attr, char *buf)
{
	if (aml_db->mode == MODE_DISABLE)
		return sprintf(buf, "0: disable\n");
	else if (aml_db->mode == MODE_ENABLE)
		return sprintf(buf, "1: enable\n");
	else if (aml_db->mode == MODE_AUTODETECT)
		return sprintf(buf, "2: auto detect\n");
	return sprintf(buf, "\n");
}

static ssize_t mode_store(struct class *cla,
			  struct class_attribute *attr,
			  const char *buf, size_t count)
{
	long val = 0;

	if (kstrtoul(buf, 10, &val)) {
		pr_info("invalid input:%s\n", buf);
		return 0;
	}
	if (val > MODE_AUTODETECT || val < MODE_DISABLE)
		val = MODE_AUTODETECT;

	if (aml_db->mode == MODE_DISABLE && val != MODE_DISABLE) {
		int r = request_irq(aml_db->irq_num, dmc_irq_handler,
				IRQF_SHARED, "ddr_bandwidth", (void *)aml_db);
		if (r < 0) {
			pr_info("ddrbandwidth request irq failed\n");
			return count;
		}

		if (aml_db->ops->init)
			aml_db->ops->init(aml_db);
	} else if ((aml_db->mode != MODE_DISABLE) && (val == MODE_DISABLE)) {
		free_irq(aml_db->irq_num, (void *)aml_db);
		aml_db->cur_sample.total_usage = 0;
		aml_db->cur_sample.total_bandwidth = 0;
		aml_db->busy = 0;
		clear_bandwidth_statistics();
	}
	if (val == MODE_AUTODETECT && aml_db->ops &&
	    aml_db->ops->config_port && !dual_dmc(aml_db)) {
		if (aml_db->mali_port[0] >= 0) {
			aml_db->ops->config_port(aml_db, 0, aml_db->mali_port[0]);
			aml_db->port[0] = (1ULL << aml_db->mali_port[0]);
		}
		if (aml_db->mali_port[1] >= 0) {
			aml_db->ops->config_port(aml_db, 1, aml_db->mali_port[1]);
			aml_db->port[1] = (1ULL << aml_db->mali_port[1]);
		}
	}
	aml_db->mode = val;

	return count;
}
static CLASS_ATTR_RW(mode);

static ssize_t irq_clock_show(struct class *cla,
			      struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", aml_db->clock_count);
}

static ssize_t irq_clock_store(struct class *cla,
			       struct class_attribute *attr,
			       const char *buf, size_t count)
{
	long val = 0;

	if (kstrtoul(buf, 10, &val)) {
		pr_info("invalid input:%s\n", buf);
		return 0;
	}
	aml_db->threshold /= (aml_db->clock_count / 10000);
	aml_db->threshold *= (val / 10000);
	aml_db->clock_count = val;
	if (aml_db->ops && aml_db->ops->init)
		aml_db->ops->init(aml_db);
	return count;
}
static CLASS_ATTR_RW(irq_clock);

static ssize_t usage_stat_store(struct class *cla,
				struct class_attribute *attr,
				const char *buf, size_t count)
{
	int d = -1;

	if (kstrtoint(buf, 10, &d))
		return count;

	aml_db->stat_flag = d;
	if (d)
		return count;

	clear_bandwidth_statistics();
	return count;
}

static ssize_t usage_stat_show(struct class *cla,
			       struct class_attribute *attr, char *buf)
{
	size_t s = 0;
	int percent, rem, i;
	unsigned long long tick;
	unsigned long total_count = 0;
	struct ddr_avg_bandwidth tmp;
#define MAX_PREFIX "MAX bandwidth: %8d KB/s, usage: %2d.%02d%%"
#define AVG_PREFIX "AVG bandwidth: %8lld KB/s, usage: %2d.%02d%%"

	if (aml_db->mode != MODE_ENABLE)
		return sprintf(buf, "set mode to enable(1) first.\n");

	total_count = aml_db->avg.sample_count;
	if (!total_count)
		return sprintf(buf, "No sample, please wait...\n");

	/* show for max bandwidth */
	percent = aml_db->max_sample.total_usage / 100;
	rem     = aml_db->max_sample.total_usage % 100;
	tick    = aml_db->max_sample.tick;
	do_div(tick, 1000);
	s      += sprintf(buf + s, MAX_PREFIX ", tick:%lld us\n",
			  aml_db->max_sample.total_bandwidth,
			  percent, rem, tick);

	/* show for average bandwidth */
	memcpy(&tmp, &aml_db->avg, sizeof(tmp));
	do_div(tmp.avg_bandwidth, total_count);
	do_div(tmp.avg_usage,     total_count);
	for (i = 0; i < aml_db->channels; i++)
		do_div(tmp.avg_port[i], total_count);

	rem     = do_div(tmp.avg_usage, 100);
	percent = tmp.avg_usage,
	s      += sprintf(buf + s, AVG_PREFIX ", samples:%ld\n",
			  tmp.avg_bandwidth,
			  percent, rem, total_count);

	s += sprintf(buf + s, "\nbandwidth status for each channel\n");
	s += sprintf(buf + s, "ch,        %s, avg bw(KB/s), max bw(KB/s), %s\n",
		     "port mask", "bw@max sample(KB/s)");
	for (i = 0; i < aml_db->channels; i++) {
		s += sprintf(buf + s,
			     "%2d, %16llx,     %8lld,     %8ld,          %8d\n",
			     i, aml_db->port[i],
			     tmp.avg_port[i],
			     tmp.max_bandwidth[i],
			     aml_db->max_sample.bandwidth[i]);
	}

	/* show for usage statistics */
	s += sprintf(buf + s, "\nusage distribution:\n");
	s += sprintf(buf + s, "range,         count,  proportion\n");
	for (i = 0; i < 10; i++) {
		percent = aml_db->usage_stat[i] * 10000 / total_count;
		rem     = percent % 100;
		percent = percent / 100;
		s += sprintf(buf + s, "%2d%% ~ %3d%%: %8d,  %3d.%02d%%\n",
			     i * 10, (i + 1) * 10,
			     aml_db->usage_stat[i], percent, rem);
	}
	s += sprintf(buf + s, "\n");
	return s;
}
static CLASS_ATTR_RW(usage_stat);

static ssize_t bandwidth_show(struct class *cla,
			      struct class_attribute *attr, char *buf)
{
	size_t s = 0;
	int percent, rem, i;
	unsigned long long tick;
#define BANDWIDTH_PREFIX "Total bandwidth: %8d KB/s, usage: %2d.%02d%%\n"

	if (aml_db->mode != MODE_ENABLE)
		return sprintf(buf, "set mode to enable(1) first.\n");

	percent = aml_db->cur_sample.total_usage / 100;
	rem     = aml_db->cur_sample.total_usage % 100;
	tick    = aml_db->cur_sample.tick;
	do_div(tick, 1000);
	s      += sprintf(buf + s, BANDWIDTH_PREFIX,
			  aml_db->cur_sample.total_bandwidth,
			  percent, rem);

	for (i = 0; i < aml_db->channels; i++) {
		s += sprintf(buf + s, "ch:%d port:%16llx: %8d KB/s\n",
			     i, aml_db->port[i],
			     aml_db->cur_sample.bandwidth[i]);
	}
	return s;
}
static CLASS_ATTR_RO(bandwidth);

static ssize_t freq_show(struct class *cla,
			 struct class_attribute *attr, char *buf)
{
	unsigned long clk = 0;

	if (aml_db->ops && aml_db->ops->get_freq)
		clk = aml_db->ops->get_freq(aml_db);
	return sprintf(buf, "%ld MHz\n", clk / 1000000);
}
static CLASS_ATTR_RO(freq);

void dmc_set_urgent(unsigned int port, unsigned int type)
{
	unsigned int val = 0, addr = 0;

	if (aml_db->cpu_type < DMC_TYPE_G12A) {
		unsigned int port_reg[16] = {
			DMC_AXI0_CHAN_CTRL, DMC_AXI1_CHAN_CTRL,
			DMC_AXI2_CHAN_CTRL, DMC_AXI3_CHAN_CTRL,
			DMC_AXI4_CHAN_CTRL, DMC_AXI5_CHAN_CTRL,
			DMC_AXI6_CHAN_CTRL, DMC_AXI7_CHAN_CTRL,
			DMC_AM0_CHAN_CTRL, DMC_AM1_CHAN_CTRL,
			DMC_AM2_CHAN_CTRL, DMC_AM3_CHAN_CTRL,
			DMC_AM4_CHAN_CTRL, DMC_AM5_CHAN_CTRL,
			DMC_AM6_CHAN_CTRL, DMC_AM7_CHAN_CTRL,};

		if (port >= 16)
			return;
		addr = port_reg[port];
	} else {
		unsigned int port_reg[24] = {
			DMC_AXI0_G12_CHAN_CTRL, DMC_AXI1_G12_CHAN_CTRL,
			DMC_AXI2_G12_CHAN_CTRL, DMC_AXI3_G12_CHAN_CTRL,
			DMC_AXI4_G12_CHAN_CTRL, DMC_AXI5_G12_CHAN_CTRL,
			DMC_AXI6_G12_CHAN_CTRL, DMC_AXI7_G12_CHAN_CTRL,
			DMC_AXI8_G12_CHAN_CTRL, DMC_AXI9_G12_CHAN_CTRL,
			DMC_AXI10_G12_CHAN_CTRL, DMC_AXI1_G12_CHAN_CTRL,
			DMC_AXI12_G12_CHAN_CTRL, 0, 0, 0,
			DMC_AM0_G12_CHAN_CTRL, DMC_AM1_G12_CHAN_CTRL,
			DMC_AM2_G12_CHAN_CTRL, DMC_AM3_G12_CHAN_CTRL,
			DMC_AM4_G12_CHAN_CTRL, DMC_AM5_G12_CHAN_CTRL,
			DMC_AM6_G12_CHAN_CTRL, DMC_AM7_G12_CHAN_CTRL,};

		if (port >= 24 || port_reg[port] == 0)
			return;
		addr = port_reg[port];
	}

	/**
	 *bit 18. force this channel all request to be super urgent request.
	 *bit 17. force this channel all request to be urgent request.
	 *bit 16. force this channel all request to be non urgent request.
	 */
	val = readl(aml_db->ddr_reg1 + addr);
	val &= (~(0x7 << 16));
	val |= ((type & 0x7) << 16);
	writel(val, aml_db->ddr_reg1 + addr);
}
EXPORT_SYMBOL(dmc_set_urgent);

static ssize_t urgent_show(struct class *cla,
			   struct class_attribute *attr, char *buf)
{
	int i, s = 0;

	if (dual_dmc(aml_db))
		return -EINVAL;

	if (!aml_db->real_ports || !aml_db->port_desc)
		return -EINVAL;

	s += sprintf(buf + s, "echo port val > /sys/class/aml_ddr/urgent\n"
			"val:\n\t1: non urgent;\n\t2: urgent;\n\t4: super urgent;\n"
			"port:   (hex integer)\n");
	for (i = 0; i < aml_db->real_ports; i++) {
		if (aml_db->port_desc[i].port_id >= 24)
			break;
		s += sprintf(buf + s, "\tbit%d: %s\n",
				aml_db->port_desc[i].port_id,
				aml_db->port_desc[i].port_name);
	}

	return s;
}

static ssize_t urgent_store(struct class *cla,
			    struct class_attribute *attr,
			    const char *buf, size_t count)
{
	unsigned int port = 0, val, i;

	if (sscanf(buf, "%x %d", &port, &val) != 2) {
		pr_info("invalid input:%s\n", buf);
		return -EINVAL;
	}
	for (i = 0; i < 24; i++) {
		if (port & 1)
			dmc_set_urgent(i, val);
		port >>= 1;
	}
	return count;
}
static CLASS_ATTR_RW(urgent);

#if DDR_BANDWIDTH_DEBUG
static ssize_t dump_reg_show(struct class *cla,
			     struct class_attribute *attr, char *buf)
{
	int s = 0;

	if (aml_db->ops && aml_db->ops->dump_reg)
		return aml_db->ops->dump_reg(aml_db, buf);

	return s;
}
static CLASS_ATTR_RO(dump_reg);
#endif

static ssize_t cpu_type_show(struct class *cla,
			     struct class_attribute *attr, char *buf)
{
	int cpu_type;

	cpu_type = aml_db->cpu_type;
	return sprintf(buf, "%x\n", cpu_type);
}
static CLASS_ATTR_RO(cpu_type);

static ssize_t name_of_ports_show(struct class *cla,
				  struct class_attribute *attr, char *buf)
{
	int i, s = 0;

	if (!aml_db->real_ports || !aml_db->port_desc)
		return -EINVAL;

	for (i = 0; i < aml_db->real_ports; i++) {
		s += sprintf(buf + s, "%2d, %s\n",
			     aml_db->port_desc[i].port_id,
			     aml_db->port_desc[i].port_name);
	}

	return s;
}
static CLASS_ATTR_RO(name_of_ports);

static struct attribute *aml_ddr_tool_attrs[] = {
	&class_attr_port.attr,
	&class_attr_irq_clock.attr,
	&class_attr_urgent.attr,
	&class_attr_threshold.attr,
	&class_attr_mode.attr,
	&class_attr_busy.attr,
	&class_attr_bandwidth.attr,
	&class_attr_freq.attr,
	&class_attr_cpu_type.attr,
	&class_attr_usage_stat.attr,
	&class_attr_name_of_ports.attr,
#if DDR_BANDWIDTH_DEBUG
	&class_attr_dump_reg.attr,
#endif
	NULL
};
ATTRIBUTE_GROUPS(aml_ddr_tool);

static struct class aml_ddr_class = {
	.name = "aml_ddr",
	.class_groups = aml_ddr_tool_groups,
};

static int __init init_chip_config(int cpu, struct ddr_bandwidth *band)
{
	switch (cpu) {
#ifdef CONFIG_AMLOGIC_DDR_BANDWIDTH_A1
	case DMC_TYPE_A1:
		band->ops = &a1_ddr_bw_ops;
		band->channels = 2;
		band->mali_port[0] = -1; /* no mali */
		band->mali_port[1] = -1;
		band->bytes_per_cycle = 8;
		break;
#endif
#ifdef CONFIG_AMLOGIC_DDR_BANDWIDTH_GX
	case DMC_TYPE_GXBB:
	case DMC_TYPE_GXTVBB:
		band->ops = &gx_ddr_bw_ops;
		band->channels = 1;
		band->mali_port[0] = 2;
		band->mali_port[1] = -1;
		break;
#endif
#ifdef CONFIG_AMLOGIC_DDR_BANDWIDTH_GXL
	case DMC_TYPE_GXL:
	case DMC_TYPE_GXM:
	case DMC_TYPE_TXL:
	case DMC_TYPE_TXLX:
	case DMC_TYPE_AXG:
	case DMC_TYPE_GXLX:
	case DMC_TYPE_TXHD:
		band->ops = &gxl_ddr_bw_ops;
		band->channels = 4;
		if (cpu == DMC_TYPE_AXG) {
			band->mali_port[0] = -1; /* no mali */
			band->mali_port[1] = -1;
		} else if (cpu == DMC_TYPE_GXM) {
			band->mali_port[0] = 1; /* port1: mali */
			band->mali_port[1] = -1;
		} else {
			band->mali_port[0] = 1; /* port1: mali0 */
			band->mali_port[1] = 2; /* port2: mali1 */
		}
		break;
#endif
#ifdef CONFIG_AMLOGIC_DDR_BANDWIDTH_G12
	case DMC_TYPE_G12A:
	case DMC_TYPE_G12B:
	case DMC_TYPE_SM1:
	case DMC_TYPE_TL1:
	case DMC_TYPE_TM2:
	case DMC_TYPE_C1:
		band->ops = &g12_ddr_bw_ops;
		band->channels = 4;
		if (cpu == DMC_TYPE_C1) {
			band->mali_port[0] = -1; /* no mali */
			band->mali_port[1] = -1;
		} else {
			band->mali_port[0] = 1; /* port1: mali */
			band->mali_port[1] = -1;
		}
		break;
#endif
#ifdef CONFIG_AMLOGIC_DDR_BANDWIDTH_T7
	case DMC_TYPE_T7:
		band->ops            = &t7_ddr_bw_ops;
		band->channels     = 8;
		band->soc_feature |= DUAL_DMC;
		band->mali_port[0] = 3; /* port3: mali */
		band->mali_port[1] = 4;
		break;
#endif
#ifdef CONFIG_AMLOGIC_DDR_BANDWIDTH_T5
	case DMC_TYPE_T5:
	case DMC_TYPE_T5D:
		band->ops = &t5_ddr_bw_ops;
		aml_db->channels = 8;
		aml_db->mali_port[0] = 1; /* port1: mali */
		aml_db->mali_port[1] = -1;
		break;
#endif
#ifdef CONFIG_AMLOGIC_DDR_BANDWIDTH_S4
	case DMC_TYPE_S4:
		band->ops = &s4_ddr_bw_ops;
		aml_db->channels = 8;
		aml_db->mali_port[0] = 1; /* port1: mali */
		aml_db->mali_port[1] = -1;
		break;
#endif
	default:
		pr_err("%s, Can't find ops for chip:%x\n", __func__, cpu);
		return -1;
	}
	return 0;
}

static void get_ddr_external_bus_width(struct ddr_bandwidth *db,
				       unsigned long sec_base)
{
	unsigned long reg, base;

	if (db->cpu_type == DMC_TYPE_TM2)
		base = sec_base + (0x0100 << 2);
	else
		base = sec_base + (0x00da << 2);

	/* each axi cycle transfer 4 external bus */
	reg = dmc_rw(base, 0, DMC_READ);
	if (reg & BIT(16))	/* 16bit external bus width, 2 * 4 = 8*/
		db->bytes_per_cycle = 8;
	else			/* 32bit external bus width, 4 * 4 = 16 */
		db->bytes_per_cycle = 16;
	pr_info("bytes_per_cycle:%d, reg:%lx\n", db->bytes_per_cycle, reg);
}

/*
 * ddr_bandwidth_probe only executes before the init process starts
 * to run, so add __ref to indicate it is okay to call __init function
 * ddr_find_port_desc
 */
static int __init ddr_bandwidth_probe(struct platform_device *pdev)
{
	int r = 0;
#ifdef CONFIG_OF
	struct device_node *node = pdev->dev.of_node;
	/*struct pinctrl *p;*/
	struct resource *res;
	resource_size_t *base;
	unsigned int sec_base;
	int io_idx = 0;
#endif
	struct ddr_port_desc *desc = NULL;
	int pcnt;

	pr_info("%s, %d\n", __func__, __LINE__);
	aml_db = devm_kzalloc(&pdev->dev, sizeof(*aml_db), GFP_KERNEL);
	if (!aml_db)
		return -ENOMEM;

	aml_db->cpu_type = (unsigned long)of_device_get_match_data(&pdev->dev);
	pr_info("chip type:0x%x\n", aml_db->cpu_type);
	if (aml_db->cpu_type < DMC_TYPE_M8B) {
		pr_info("unsupport chip type:%d\n", aml_db->cpu_type);
		aml_db = NULL;
		return -EINVAL;
	}

	/* find and configure port description */
	pcnt = ddr_find_port_desc(aml_db->cpu_type, &desc);
	if (pcnt < 0) {
		pr_err("can't find port descriptor,cpu:%d\n", aml_db->cpu_type);
	} else {
		aml_db->port_desc = desc;
		aml_db->real_ports = pcnt;
	}

	if (init_chip_config(aml_db->cpu_type, aml_db)) {
		aml_db = NULL;
		return -EINVAL;
	}

#ifdef CONFIG_OF
	/* resource 0 for ddr register base */
	res = platform_get_resource(pdev, IORESOURCE_MEM, io_idx);
	if (res) {
		base = ioremap(res->start, res->end - res->start);
		aml_db->ddr_reg1 = (void *)base;
	} else {
		pr_err("can't get ddr reg base\n");
		aml_db = NULL;
		return -EINVAL;
	}
	io_idx++;

	if (dual_dmc(aml_db)) {
		/* next for ddr register base */
		res = platform_get_resource(pdev, IORESOURCE_MEM, io_idx);
		if (res) {
			base = ioremap(res->start, res->end - res->start);
			aml_db->ddr_reg2 = (void *)base;
			io_idx++;
		} else {
			pr_err("can't get ddr reg %d base\n", io_idx);
			aml_db = NULL;
			return -EINVAL;
		}
	}

	/* next for pll register base */
	res = platform_get_resource(pdev, IORESOURCE_MEM, io_idx);
	if (res) {
		base = ioremap(res->start, res->end - res->start);
		aml_db->pll_reg = (void *)base;
	} else {
		pr_err("can't get ddr reg %d base\n", io_idx);
		aml_db = NULL;
		return -EINVAL;
	}

	aml_db->irq_num = of_irq_get(node, 0);
	r = of_property_read_u32(node, "sec_base", &sec_base);
	if (r < 0) {
		aml_db->bytes_per_cycle = 16;
		pr_info("can't find sec_base, set bytes_per_cycle to 16\n");
	} else {
		get_ddr_external_bus_width(aml_db, sec_base);
	}
#endif
	spin_lock_init(&aml_db->lock);
	aml_db->clock_count = DEFAULT_CLK_CNT;
	aml_db->mode = MODE_DISABLE;
	aml_db->threshold = DEFAULT_THRESHOLD * aml_db->bytes_per_cycle *
			    (aml_db->clock_count / 10000);

	if (!aml_db->ops->config_port)
		return -EINVAL;

	r = class_register(&aml_ddr_class);
	if (r)
		pr_info("%s, class regist failed\n", __func__);

	return 0;
}

static int ddr_bandwidth_remove(struct platform_device *pdev)
{
	if (aml_db) {
		class_destroy(&aml_ddr_class);
		free_irq(aml_db->irq_num, aml_db);
		kfree(aml_db->port_desc);
		iounmap(aml_db->ddr_reg1);
		iounmap(aml_db->pll_reg);
		aml_db = NULL;
	}

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id aml_ddr_bandwidth_dt_match[] = {
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
	{
		.compatible = "amlogic,ddr-bandwidth-m8b",
		.data = (void *)DMC_TYPE_M8B,	/* chip id */
	},
	{
		.compatible = "amlogic,ddr-bandwidth-gx",
		.data = (void *)DMC_TYPE_GXBB,
	},
	{
		.compatible = "amlogic,ddr-bandwidth-gxl",
		.data = (void *)DMC_TYPE_GXL,
	},
	{
		.compatible = "amlogic,ddr-bandwidth-gxm",
		.data = (void *)DMC_TYPE_GXM,
	},
	{
		.compatible = "amlogic,ddr-bandwidth-gxlx",
		.data = (void *)DMC_TYPE_GXLX,
	},
	{
		.compatible = "amlogic,ddr-bandwidth-axg",
		.data = (void *)DMC_TYPE_AXG,
	},
	{
		.compatible = "amlogic,ddr-bandwidth-txl",
		.data = (void *)DMC_TYPE_TXL,
	},
	{
		.compatible = "amlogic,ddr-bandwidth-txlx",
		.data = (void *)DMC_TYPE_TXLX,
	},
	{
		.compatible = "amlogic,ddr-bandwidth-txhd",
		.data = (void *)DMC_TYPE_TXHD,
	},
	{
		.compatible = "amlogic,ddr-bandwidth-tl1",
		.data = (void *)DMC_TYPE_TL1,
	},
	{
		.compatible = "amlogic,ddr-bandwidth-a1",
		.data = (void *)DMC_TYPE_A1,
	},
	{
		.compatible = "amlogic,ddr-bandwidth-c1",
		.data = (void *)DMC_TYPE_C1,
	},
#endif
	{
		.compatible = "amlogic,ddr-bandwidth-g12a",
		.data = (void *)DMC_TYPE_G12A,
	},
	{
		.compatible = "amlogic,ddr-bandwidth-g12b",
		.data = (void *)DMC_TYPE_G12B,
	},
	{
		.compatible = "amlogic,ddr-bandwidth-tm2",
		.data = (void *)DMC_TYPE_TM2,
	},
	{
		.compatible = "amlogic,ddr-bandwidth-t5",
		.data = (void *)DMC_TYPE_T5,
	},
	{
		.compatible = "amlogic,ddr-bandwidth-t5d",
		.data = (void *)DMC_TYPE_T5D,
	},
	{
		.compatible = "amlogic,ddr-bandwidth-t7",
		.data = (void *)DMC_TYPE_T7,
	},
	{
		.compatible = "amlogic,ddr-bandwidth-s4",
		.data = (void *)DMC_TYPE_S4,
	},
	{}
};
#endif

static struct platform_driver ddr_bandwidth_driver = {
	.driver = {
		.name = "amlogic,ddr-bandwidth",
		.owner = THIS_MODULE,
	},
	.remove = ddr_bandwidth_remove,
};

int __init ddr_bandwidth_init(void)
{
#ifdef CONFIG_OF
	const struct of_device_id *match_id;
	match_id = aml_ddr_bandwidth_dt_match;
	ddr_bandwidth_driver.driver.of_match_table = match_id;
#endif

	platform_driver_probe(&ddr_bandwidth_driver,
			      ddr_bandwidth_probe);
	return 0;
}

void ddr_bandwidth_exit(void)
{
	platform_driver_unregister(&ddr_bandwidth_driver);
}

