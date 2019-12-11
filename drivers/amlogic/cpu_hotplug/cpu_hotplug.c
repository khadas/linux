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

static int find_idlest_cpumask_cpu(unsigned int cluster)
{
	unsigned int load, min_load = UINT_MAX;
	int least_loaded_cpu = -1;
	int shallowest_cpu = -1;
	int cpu;
	struct rq *rq = NULL;

	for_each_cpu_and(cpu, &hpg.cpumask[cluster], cpu_online_mask) {
		if (cpu == cpumask_first(&hpg.cpumask[cluster]))
			continue;

		if (available_idle_cpu(cpu)) {
			shallowest_cpu = cpu;
		} else if (shallowest_cpu == -1) {
			rq = cpu_rq(cpu);
			load = rq->nr_running;
			if (load < min_load) {
				min_load = load;
				least_loaded_cpu = cpu;
			}
		}
	}

	return shallowest_cpu != -1 ? shallowest_cpu : least_loaded_cpu;
}

static void set_hotplug_online(unsigned int cluster)
{
	unsigned int cpu, online;
	int ret;

	for_each_cpu(cpu, &hpg.cpumask[cluster]) {
		if (cpu_online(cpu))
			continue;

		online = cpu_num_online(cluster);
		if (online >= hpg.max_num[cluster])
			break;

		ret = device_online(get_cpu_device(cpu));
	}
}

static void set_hotplug_offline(unsigned int cluster)
{
	unsigned int target, online, min, max;
	int ret;

	online = cpu_num_online(cluster);
	min = hpg.min_num[cluster];
	max = hpg.max_num[cluster];

	while (online > min && (online > max)) {
		target = find_idlest_cpumask_cpu(cluster);
		if (target == -1)
			break;
		ret = device_offline(get_cpu_device(target));
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

static int __init cpu_hotplug_init(void)
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

static void __exit cpu_hotplug_exit(void)
{
}

MODULE_DESCRIPTION("amlogic cpu hotplug");

module_init(cpu_hotplug_init);
module_exit(cpu_hotplug_exit);
