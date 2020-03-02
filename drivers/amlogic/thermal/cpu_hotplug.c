// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include <linux/amlogic/cpu_hotplug.h>
#include <../kernel/sched/sched.h>

#define CPU_HOTPLUG_NONE	0
#define CPU_HOTPLUG_PLUG	1
#define CPU_HOTPLUG_UNPLUG	2
#define MAX_CLUSTRS		2

struct cpu_hotplug_s {
	int clusters;
	unsigned int flags[MAX_CLUSTRS];
	unsigned int max_num[MAX_CLUSTRS];
	cpumask_t cpumask[MAX_CLUSTRS];
	unsigned int cpunum[MAX_CLUSTRS];
	unsigned int min_num[MAX_CLUSTRS];
	struct work_struct work;
	struct mutex mutex; /* define a mutex */
};

static struct cpu_hotplug_s hpg;
struct rq;

static int cpu_hotplug_cpumask_init(void)
{
	int cpu, cluster;

	hpg.clusters = 0;
	for (cluster = 0; cluster < MAX_CLUSTRS; cluster++) {
		hpg.cpunum[cluster] = 0;
		cpumask_clear(&hpg.cpumask[cluster]);
	}

	for_each_possible_cpu(cpu) {
		cluster = topology_physical_package_id(cpu);
		if (cluster < 0)
			continue;
		cpumask_set_cpu(cpu, &hpg.cpumask[cluster]);
		if (hpg.clusters < cluster + 1)
			hpg.clusters = cluster + 1;
		hpg.cpunum[cluster]++;
	}

	for (cluster = 0; cluster < MAX_CLUSTRS; cluster++) {
		hpg.max_num[cluster] = hpg.cpunum[cluster];
		hpg.flags[cluster] = CPU_HOTPLUG_NONE;
		hpg.min_num[cluster] = 1;
	}

	return 0;
}

static int cpu_num_online(int cluster)
{
	cpumask_t mask;

	if (cluster >= hpg.clusters)
		return 0;
	cpumask_and(&mask, &hpg.cpumask[cluster], cpu_online_mask);
	return cpumask_weight(&mask);
}

void cpufreq_set_max_cpu_num(unsigned int num, unsigned int cluster)
{
	unsigned int cpu_online;

	if (!num || cluster > hpg.clusters) {
		dev_err(NULL, " %s <:%d %d>\n", __func__, num, cluster);
		return;
	}

	mutex_lock(&hpg.mutex);
	if (num > hpg.cpunum[cluster])
		num = hpg.cpunum[cluster];

	if (hpg.max_num[cluster] == num) {
		mutex_unlock(&hpg.mutex);
		return;
	}

	cpu_online = cpu_num_online(cluster);
	hpg.max_num[cluster] = num;
	if (num < cpu_online) {
		hpg.flags[cluster] = CPU_HOTPLUG_UNPLUG;
		schedule_work(&hpg.work);
	} else if (num > cpu_online) {
		hpg.flags[cluster] = CPU_HOTPLUG_PLUG;
		schedule_work(&hpg.work);
	}

	mutex_unlock(&hpg.mutex);
}
EXPORT_SYMBOL(cpufreq_set_max_cpu_num);

static int find_online_cpu(unsigned int cluster)
{
	int cpu;

	for_each_cpu_and(cpu, &hpg.cpumask[cluster], cpu_online_mask) {
		if (cpu == cpumask_first(&hpg.cpumask[cluster]))
			continue;

		return cpu;
	}

	return -1;
}

static void set_hotplug_online(unsigned int cluster)
{
	unsigned int cpu, online;
	struct device *dev;

	for_each_cpu(cpu, &hpg.cpumask[cluster]) {
		if (cpu_online(cpu))
			continue;

		online = cpu_num_online(cluster);
		if (online >= hpg.max_num[cluster])
			break;

		dev = get_cpu_device(cpu);
		cpu_up(cpu);
		kobject_uevent(&dev->kobj, KOBJ_ONLINE);
		dev->offline = false;
	}
}

static void set_hotplug_offline(unsigned int cluster)
{
	unsigned int target, online, min, max;
	struct device *dev;

	online = cpu_num_online(cluster);
	min = hpg.min_num[cluster];
	max = hpg.max_num[cluster];

	while (online > min && (online > max)) {
		target = find_online_cpu(cluster);
		if (target == -1)
			break;

		dev = get_cpu_device(target);
		cpu_down(target);
		kobject_uevent(&dev->kobj, KOBJ_OFFLINE);
		dev->offline = true;

		online = cpu_num_online(cluster);
		min = hpg.min_num[cluster];
		max = hpg.max_num[cluster];
	}
}

static void __ref hotplug_work(struct work_struct *work)
{
	unsigned int cluster, flg;

	mutex_lock(&hpg.mutex);
	for (cluster = 0; cluster < hpg.clusters; cluster++) {
		if (!hpg.flags[cluster])
			continue;

		flg = hpg.flags[cluster];
		hpg.flags[cluster] = 0;

		switch (flg) {
		case CPU_HOTPLUG_PLUG:
			set_hotplug_online(cluster);
			break;
		case CPU_HOTPLUG_UNPLUG:
			set_hotplug_offline(cluster);
			break;
		}
	}

	mutex_unlock(&hpg.mutex);
}

static ssize_t show_hotplug_max_cpus(struct kobject *kobj,
				     struct kobj_attribute *attr, char *buf)
{
	unsigned int max = 0;
	unsigned int c = 0;

	for (c = 0; c < hpg.clusters; c++)
		max |= hpg.max_num[c] << (c * 8);
	return sprintf(buf, "0x%04x\n", max);
}

static ssize_t store_hotplug_max_cpus(struct kobject *kobj,
				      struct kobj_attribute *attr,
				      const char *buf, size_t count)
{
	unsigned int input;
	unsigned int max;
	unsigned int c = 0;
	int ret;

	ret = kstrtouint(buf, 0, &input);
	if (ret) {
		dev_err(NULL, " %s <%d>\n", __func__, ret);
		return ret;
	}

	for (c = 0; c < hpg.clusters; c++) {
		max = input & 0xff;
		if (max)
			cpufreq_set_max_cpu_num(max, c);
		input = input >> 8;
	}

	return count;
}

define_one_global_rw(hotplug_max_cpus);

int cpu_hotplug_init(void)
{
	int err;

	mutex_init(&hpg.mutex);
	cpu_hotplug_cpumask_init();
	INIT_WORK(&hpg.work, hotplug_work);

	err = sysfs_create_file(&cpu_subsys.dev_root->kobj,
				&hotplug_max_cpus.attr);
	if (err) {
		dev_err(NULL, " %s <%d>\n", __func__, err);
		return err;
	}

	return 0;
}
