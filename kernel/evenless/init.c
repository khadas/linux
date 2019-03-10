/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#include <linux/init.h>
#include <linux/module.h>
#include <evenless/init.h>
#include <evenless/sched.h>
#include <evenless/clock.h>
#include <evenless/timer.h>
#include <evenless/tick.h>
#include <evenless/memory.h>
#include <evenless/file.h>
#include <evenless/factory.h>
#include <evenless/control.h>
#define CREATE_TRACE_POINTS
#include <trace/events/evenless.h>

static char *oobcpus_arg;
module_param_named(oobcpus, oobcpus_arg, charp, 0444);

static char init_state_arg[16] = "enabled";
module_param_string(state, init_state_arg, sizeof(init_state_arg), 0444);

struct cpumask evl_oob_cpus;
EXPORT_SYMBOL_GPL(evl_oob_cpus);

DEFINE_PER_CPU(struct evl_machine_cpudata, evl_machine_cpudata);
EXPORT_PER_CPU_SYMBOL_GPL(evl_machine_cpudata);

#ifdef CONFIG_EVL_DEBUG
#define boot_debug_notice "[DEBUG]"
#else
#define boot_debug_notice ""
#endif

#ifdef CONFIG_ENABLE_DEFAULT_TRACERS
#define boot_trace_notice "[TRACE]"
#else
#define boot_trace_notice ""
#endif

#define boot_state_notice				\
	({						\
		evl_is_stopped() ? "[STOPPED]" : "";	\
	})

static struct {
	const char *label;
	enum evl_run_states state;
} init_states[] __initdata = {
	{ "disabled", EVL_STATE_DISABLED },
	{ "stopped", EVL_STATE_STOPPED },
	{ "enabled", EVL_STATE_WARMUP },
};

static void __init setup_init_state(void)
{
	static char warn_bad_state[] __initdata =
		EVL_WARNING "invalid init state '%s'\n";
	int n;

	for (n = 0; n < ARRAY_SIZE(init_states); n++)
		if (strcmp(init_states[n].label, init_state_arg) == 0) {
			set_evl_state(init_states[n].state);
			return;
		}

	printk(warn_bad_state, init_state_arg);
}

#ifdef CONFIG_EVL_DEBUG

void __init evl_warn_init(const char *fn, int level, int status)
{
	printk(EVL_ERR "FAILED: %s => [%d]\n", fn, status);
}

#endif

static __init int init_core(void)
{
	int ret;

	enable_oob_stage("EVL");

	ret = evl_init_memory();
	if (ret)
		goto cleanup_stage;

	ret = evl_early_init_factories();
	if (ret)
		goto cleanup_memory;

	ret = evl_clock_init();
	if (ret)
		goto cleanup_early_factories;

	ret = evl_init_sched();
	if (ret)
		goto cleanup_clock;

	/*
	 * If starting in stopped mode, do all initializations, but do
	 * not enable the core timer.
	 */
	if (evl_is_warming()) {
		ret = evl_enable_tick();
		if (ret)
			goto cleanup_sched;
		set_evl_state(EVL_STATE_RUNNING);
	}

	ret = dovetail_start();
	if (ret)
		goto cleanup_tick;

	/*
	 * Other factories can clone elements, which would allow users
	 * to issue Dovetail requests afterwards, so let's expose them
	 * last.
	 */
	ret = evl_late_init_factories();
	if (ret)
		goto cleanup_dovetail;

	return 0;

cleanup_dovetail:
	dovetail_stop();
cleanup_tick:
	if (evl_is_running())
		evl_disable_tick();
cleanup_sched:
	evl_cleanup_sched();
cleanup_clock:
	evl_clock_cleanup();
cleanup_early_factories:
	evl_early_cleanup_factories();
cleanup_memory:
	evl_cleanup_memory();
cleanup_stage:
	disable_oob_stage();
	set_evl_state(EVL_STATE_STOPPED);

	return ret;
}

static int __init evl_init(void)
{
	int ret;

	setup_init_state();

	if (!evl_is_enabled()) {
		printk(EVL_WARNING "disabled on kernel command line\n");
		return 0;
	}

	/*
	 * Set of CPUs the core knows about (>= set of CPUs running
	 * EVL threads).
	 */
	if (oobcpus_arg && *oobcpus_arg) {
		if (cpulist_parse(oobcpus_arg, &evl_oob_cpus)) {
			printk(EVL_WARNING "invalid set of OOB cpus\n");
			cpumask_copy(&evl_oob_cpus, cpu_online_mask);
		}
	} else
		cpumask_copy(&evl_oob_cpus, cpu_online_mask);

	/* Threads may run on any out-of-band CPU by default. */
	evl_cpu_affinity = evl_oob_cpus;

	ret = EVL_INIT_CALL(0, init_core());
	if (ret)
		goto fail;

	printk(EVL_INFO "core started %s%s%s\n",
		boot_debug_notice,
		boot_trace_notice,
		boot_state_notice);

	return 0;
fail:
	set_evl_state(EVL_STATE_DISABLED);

	printk(EVL_ERR "disabling.\n");

	return ret;
}
device_initcall(evl_init);
