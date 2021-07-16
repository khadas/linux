/**
 * Copyright (C) ARM Limited 2010-2016. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef GATOR_H_
#define GATOR_H_

#include <linux/version.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/list.h>

#ifdef CONFIG_PERF_EVENTS
#define PERF_EVENTS 1
#else
#define PERF_EVENTS 0
#endif

#ifdef CONFIG_HW_PERF_EVENTS
#define HW_PERF_EVENTS 1
#else
#define HW_PERF_EVENTS 0
#endif

#ifdef __arm__
#define __arm_flag__ 1
#else
#define __arm_flag__ 0
#endif

#ifdef __aarch64__
#define __aarch64_flag__ 1
#else
#define __aarch64_flag__ 0
#endif

#ifdef CONFIG_CPU_FREQ
#define CPU_FREQ 1
#else
#define CPU_FREQ 0
#endif

#define GATOR_PERF_PMU_SUPPORT  (PERF_EVENTS && (!(__arm_flag__ || __aarch64_flag__) || HW_PERF_EVENTS))
#define GATOR_CPU_FREQ_SUPPORT  CPU_FREQ
#ifdef CONFIG_BL_SWITCHER
#define GATOR_IKS_SUPPORT 1
#else
#define GATOR_IKS_SUPPORT 0
#endif

/* cpu ids */
#define CORTEX_A5    0x41c05
#define CORTEX_A9    0x41c09
#define OTHER        0xfffff

#define MAXSIZE_CORE_NAME 32

struct gator_cpu {
	struct list_head list;
	unsigned long cpuid;
	unsigned long pmnc_counters;
	/* Human readable name */
	char core_name[MAXSIZE_CORE_NAME];
	/* gatorfs event and Perf PMU name */
	char pmnc_name[MAXSIZE_CORE_NAME];
	/* compatible from Documentation/devicetree/bindings/arm/cpus.txt */
	char dt_name[MAXSIZE_CORE_NAME];
};

/* clusters */
#define GATOR_CLUSTER_COUNT 4

extern const struct gator_cpu *gator_clusters[GATOR_CLUSTER_COUNT];
extern int gator_clusterids[NR_CPUS];
extern int gator_cluster_count;

/* gpu enums */
#define MALI_4xx     1
#define MALI_MIDGARD_OR_BIFROST 2

/******************************************************************************
 * Filesystem
 ******************************************************************************/
struct dentry *gatorfs_mkdir(struct super_block *sb, struct dentry *root,
			char const *name);

int gatorfs_create_ulong(struct super_block *sb, struct dentry *root,
			char const *name, unsigned long *val);

int gatorfs_create_ro_ulong(struct super_block *sb, struct dentry *root,
			char const *name, unsigned long *val);

/******************************************************************************
 * Tracepoints
 ******************************************************************************/
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 4, 0)
#   error Kernels prior to 3.4 not supported. DS-5 v5.21 and earlier supported 2.6.32 and later.
#elif LINUX_VERSION_CODE < KERNEL_VERSION(3, 15, 0)
#   define GATOR_DEFINE_PROBE(probe_name, proto) \
		static void probe_##probe_name(void *data, PARAMS(proto))
#   define GATOR_REGISTER_TRACE(probe_name) \
        register_trace_##probe_name(probe_##probe_name, NULL)
#   define GATOR_UNREGISTER_TRACE(probe_name) \
        unregister_trace_##probe_name(probe_##probe_name, NULL)
#else
#   define GATOR_DEFINE_PROBE(probe_name, proto) \
        extern struct tracepoint *gator_tracepoint_##probe_name; \
        static void probe_##probe_name(void *data, PARAMS(proto))
#   define GATOR_REGISTER_TRACE(probe_name) \
        ((gator_tracepoint_##probe_name == NULL) || tracepoint_probe_register(gator_tracepoint_##probe_name, probe_##probe_name, NULL))
#   define GATOR_UNREGISTER_TRACE(probe_name) \
        tracepoint_probe_unregister(gator_tracepoint_##probe_name, probe_##probe_name, NULL)
#endif

/******************************************************************************
 * Events
 ******************************************************************************/
struct gator_interface {
    const char *const name;
    /* Complementary function to init */
    void (*shutdown)(void);
    int (*create_files)(struct super_block *sb, struct dentry *root);
    int (*start)(void);
    /* Complementary function to start */
    void (*stop)(void);
    int (*online)(int **buffer, bool migrate);
    int (*offline)(int **buffer, bool migrate);
    /* called in process context but may not be running on core 'cpu' */
    void (*online_dispatch)(int cpu, bool migrate);
    /* called in process context but may not be running on core 'cpu' */
    void (*offline_dispatch)(int cpu, bool migrate);
    int (*read)(int **buffer, bool sched_switch);
    int (*read64)(long long **buffer, bool sched_switch);
    int (*read_proc)(long long **buffer, struct task_struct *);
    struct list_head list;
};

u64 gator_get_time(void);
int gator_events_install(struct gator_interface *interface);
int gator_events_get_key(void);
u32 gator_cpuid(void);

void gator_marshal_activity_switch(int core, int key, int activity, int pid);

#if !GATOR_IKS_SUPPORT

#define get_physical_cpu() smp_processor_id()
#define lcpu_to_pcpu(lcpu) lcpu
#define pcpu_to_lcpu(pcpu) pcpu

#else

#define get_physical_cpu() lcpu_to_pcpu(get_logical_cpu())
int lcpu_to_pcpu(const int lcpu);
int pcpu_to_lcpu(const int pcpu);

#endif

#define get_logical_cpu() smp_processor_id()
#define on_primary_core() (get_logical_cpu() == 0)

#endif /* GATOR_H_ */
