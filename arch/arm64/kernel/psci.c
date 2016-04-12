/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Copyright (C) 2013 ARM Limited
 *
 * Author: Will Deacon <will.deacon@arm.com>
 */

#define pr_fmt(fmt) "psci: " fmt

#include <linux/init.h>
#include <linux/of.h>
#include <linux/smp.h>
#include <linux/slab.h>

#include <asm/compiler.h>
#include <asm/cpu_ops.h>
#include <asm/errno.h>
#include <asm/psci.h>
#include <asm/smp_plat.h>
#include <asm/suspend.h>
#include <asm/system_misc.h>

#define PSCI_POWER_STATE_TYPE_STANDBY		0
#define PSCI_POWER_STATE_TYPE_POWER_DOWN	1

struct psci_power_state {
	u16	id;
	u8	type;
	u8	affinity_level;
};

struct psci_operations {
	int (*cpu_suspend)(struct psci_power_state state,
			   unsigned long entry_point);
	int (*cpu_off)(struct psci_power_state state);
	int (*cpu_on)(unsigned long cpuid, unsigned long entry_point);
	int (*migrate)(unsigned long cpuid);
};

static struct psci_operations psci_ops;

static int (*invoke_psci_fn)(u64, u64, u64, u64);

enum psci_function {
	PSCI_FN_CPU_SUSPEND,
	PSCI_FN_CPU_ON,
	PSCI_FN_CPU_OFF,
	PSCI_FN_MIGRATE,
	PSCI_FN_MAX,
};

static DEFINE_PER_CPU_READ_MOSTLY(struct psci_power_state *,
				psci_power_state);

static u32 psci_function_id[PSCI_FN_MAX];

#define PSCI_RET_SUCCESS		0
#define PSCI_RET_EOPNOTSUPP		-1
#define PSCI_RET_EINVAL			-2
#define PSCI_RET_EPERM			-3

static int psci_to_linux_errno(int errno)
{
	switch (errno) {
	case PSCI_RET_SUCCESS:
		return 0;
	case PSCI_RET_EOPNOTSUPP:
		return -EOPNOTSUPP;
	case PSCI_RET_EINVAL:
		return -EINVAL;
	case PSCI_RET_EPERM:
		return -EPERM;
	};

	return -EINVAL;
}

#define PSCI_POWER_STATE_ID_MASK	0xffff
#define PSCI_POWER_STATE_ID_SHIFT	0
#define PSCI_POWER_STATE_TYPE_MASK	0x1
#define PSCI_POWER_STATE_TYPE_SHIFT	16
#define PSCI_POWER_STATE_AFFL_MASK	0x3
#define PSCI_POWER_STATE_AFFL_SHIFT	24

static u32 psci_power_state_pack(struct psci_power_state state)
{
	return	((state.id & PSCI_POWER_STATE_ID_MASK)
			<< PSCI_POWER_STATE_ID_SHIFT)	|
		((state.type & PSCI_POWER_STATE_TYPE_MASK)
			<< PSCI_POWER_STATE_TYPE_SHIFT)	|
		((state.affinity_level & PSCI_POWER_STATE_AFFL_MASK)
			<< PSCI_POWER_STATE_AFFL_SHIFT);
}

static void psci_power_state_unpack(u32 power_state,
					struct psci_power_state *state)
{
	state->id = ((power_state >> PSCI_POWER_STATE_ID_SHIFT)
						& PSCI_POWER_STATE_ID_MASK);
	state->type = ((power_state >> PSCI_POWER_STATE_TYPE_SHIFT)
						& PSCI_POWER_STATE_TYPE_MASK);
	state->affinity_level = ((power_state >> PSCI_POWER_STATE_AFFL_SHIFT)
						& PSCI_POWER_STATE_AFFL_MASK);
}

static int psci_cpu_suspend(struct psci_power_state state,
			    unsigned long entry_point)
{
	int err;
	u32 fn, power_state;

	fn = psci_function_id[PSCI_FN_CPU_SUSPEND];
	power_state = psci_power_state_pack(state);
	err = invoke_psci_fn(fn, power_state, entry_point, 0);
	return psci_to_linux_errno(err);
}

static int psci_cpu_off(struct psci_power_state state)
{
	int err;
	u32 fn, power_state;

	fn = psci_function_id[PSCI_FN_CPU_OFF];
	power_state = psci_power_state_pack(state);
	err = invoke_psci_fn(fn, power_state, 0, 0);
	return psci_to_linux_errno(err);
}

static int psci_cpu_on(unsigned long cpuid, unsigned long entry_point)
{
	int err;
	u32 fn;

	fn = psci_function_id[PSCI_FN_CPU_ON];
	err = invoke_psci_fn(fn, cpuid, entry_point, 0);
	return psci_to_linux_errno(err);
}

static int psci_migrate(unsigned long cpuid)
{
	int err;
	u32 fn;

	fn = psci_function_id[PSCI_FN_MIGRATE];
	err = invoke_psci_fn(fn, cpuid, 0, 0);
	return psci_to_linux_errno(err);
}

static int __maybe_unused cpu_psci_cpu_init_idle(struct device_node *cpu_node,
						 unsigned int cpu)
{
	int i, ret, count = 0;
	struct psci_power_state *psci_states;
	struct device_node *state_node;

	/*
	 * If the PSCI cpu_suspend function hook has not been initialized
	 * idle states must not be enabled, so bail out
	 */
	if (!psci_ops.cpu_suspend)
		return -EOPNOTSUPP;

	/* Count idle states */
	while ((state_node = of_parse_phandle(cpu_node, "cpu-idle-states",
					      count))) {
		count++;
		of_node_put(state_node);
	}

	if (!count)
		return -ENODEV;

	psci_states = kcalloc(count, sizeof(*psci_states), GFP_KERNEL);
	if (!psci_states)
		return -ENOMEM;

	for (i = 0; i < count; i++) {
		u32 psci_power_state;

		state_node = of_parse_phandle(cpu_node, "cpu-idle-states", i);

		ret = of_property_read_u32(state_node,
					   "arm,psci-suspend-param",
					   &psci_power_state);
		if (ret) {
			pr_warn(" * %s missing arm,psci-suspend-param property\n",
				state_node->full_name);
			of_node_put(state_node);
			goto free_mem;
		}

		of_node_put(state_node);
		pr_debug("psci-power-state %#x index %d\n", psci_power_state,
							    i);
		psci_power_state_unpack(psci_power_state, &psci_states[i]);
	}
	/* Idle states parsed correctly, initialize per-cpu pointer */
	per_cpu(psci_power_state, cpu) = psci_states;
	return 0;

free_mem:
	kfree(psci_states);
	return ret;
}

static const struct of_device_id psci_of_match[] __initconst = {
	{ .compatible = "arm,psci",	},
	{},
};

int __init psci_init(void)
{
	struct device_node *np;
	const char *method;
	u32 id;
	int err = 0;

	np = of_find_matching_node(NULL, psci_of_match);
	if (!np)
		return -ENODEV;

	pr_info("probing function IDs from device-tree\n");

	if (of_property_read_string(np, "method", &method)) {
		pr_warn("missing \"method\" property\n");
		err = -ENXIO;
		goto out_put_node;
	}

	if (!strcmp("hvc", method)) {
		invoke_psci_fn = __invoke_psci_fn_hvc;
	} else if (!strcmp("smc", method)) {
		invoke_psci_fn = __invoke_psci_fn_smc;
	} else {
		pr_warn("invalid \"method\" property: %s\n", method);
		err = -EINVAL;
		goto out_put_node;
	}

	if (!of_property_read_u32(np, "cpu_suspend", &id)) {
		psci_function_id[PSCI_FN_CPU_SUSPEND] = id;
		psci_ops.cpu_suspend = psci_cpu_suspend;
	}

	if (!of_property_read_u32(np, "cpu_off", &id)) {
		psci_function_id[PSCI_FN_CPU_OFF] = id;
		psci_ops.cpu_off = psci_cpu_off;
	}

	if (!of_property_read_u32(np, "cpu_on", &id)) {
		psci_function_id[PSCI_FN_CPU_ON] = id;
		psci_ops.cpu_on = psci_cpu_on;
	}

	if (!of_property_read_u32(np, "migrate", &id)) {
		psci_function_id[PSCI_FN_MIGRATE] = id;
		psci_ops.migrate = psci_migrate;
	}

out_put_node:
	of_node_put(np);
	return err;
}

#ifdef CONFIG_SMP

static int __init cpu_psci_cpu_init(struct device_node *dn, unsigned int cpu)
{
	return 0;
}

static int __init cpu_psci_cpu_prepare(unsigned int cpu)
{
	if (!psci_ops.cpu_on) {
		pr_err("no cpu_on method, not booting CPU%d\n", cpu);
		return -ENODEV;
	}

	return 0;
}

static int cpu_psci_cpu_boot(unsigned int cpu)
{
	int err = psci_ops.cpu_on(cpu_logical_map(cpu), __pa(secondary_entry));
	if (err)
		pr_err("psci: failed to boot CPU%d (%d)\n", cpu, err);

	return err;
}

#ifdef CONFIG_HOTPLUG_CPU
static int cpu_psci_cpu_disable(unsigned int cpu)
{
	/* Fail early if we don't have CPU_OFF support */
	if (!psci_ops.cpu_off)
		return -EOPNOTSUPP;
	return 0;
}

static void cpu_psci_cpu_die(unsigned int cpu)
{
	int ret;
	/*
	 * There are no known implementations of PSCI actually using the
	 * power state field, pass a sensible default for now.
	 */
	struct psci_power_state state = {
		.type = PSCI_POWER_STATE_TYPE_POWER_DOWN,
	};

	ret = psci_ops.cpu_off(state);

	pr_crit("psci: unable to power off CPU%u (%d)\n", cpu, ret);
}
#endif
static int psci_suspend_finisher(unsigned long index)
{
	 struct psci_power_state *state =
		__this_cpu_read(psci_power_state);

	struct psci_power_state  state_no_idle = {
		.id = 0,
		.type = 1,
		.affinity_level = 0,
	};

	if (!state)
		return psci_ops.cpu_suspend(state_no_idle,
				    virt_to_phys(cpu_resume));
	else
		return psci_ops.cpu_suspend(state[index - 1],
				    virt_to_phys(cpu_resume));
}

static int __maybe_unused cpu_psci_cpu_suspend(unsigned long index)
{
	int ret;
	/* struct psci_power_state *state =
	 * __this_cpu_read(psci_power_state); */
	/*
	 * idle state index 0 corresponds to wfi, should never be called
	 * from the cpu_suspend operations
	 */
	if (WARN_ON_ONCE(!index))
		return -EINVAL;

	/* if (state[index - 1].type == PSCI_POWER_STATE_TYPE_STANDBY) */
		/* ret = psci_ops.cpu_suspend(state[index - 1], 0); */
	/* else */
		ret = __cpu_suspend(index, psci_suspend_finisher);

	return ret;
}

const struct cpu_operations cpu_psci_ops = {
	.name		= "psci",
#ifdef CONFIG_CPU_IDLE
	.cpu_init_idle	= cpu_psci_cpu_init_idle,
#endif
	.cpu_init	= cpu_psci_cpu_init,
	.cpu_prepare	= cpu_psci_cpu_prepare,
	.cpu_boot	= cpu_psci_cpu_boot,
	.cpu_suspend	= cpu_psci_cpu_suspend,
#ifdef CONFIG_HOTPLUG_CPU
	.cpu_disable	= cpu_psci_cpu_disable,
	.cpu_die	= cpu_psci_cpu_die,
#endif
};

#endif
