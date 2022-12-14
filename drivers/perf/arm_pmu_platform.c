// SPDX-License-Identifier: GPL-2.0
/*
 * platform_device probing code for ARM performance counters.
 *
 * Copyright (C) 2009 picoChip Designs, Ltd., Jamie Iles
 * Copyright (C) 2010 ARM Ltd., Will Deacon <will.deacon@arm.com>
 */
#define pr_fmt(fmt) "hw perfevents: " fmt

#include <linux/bug.h>
#include <linux/cpumask.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/irq.h>
#include <linux/irqdesc.h>
#include <linux/kconfig.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/percpu.h>
#include <linux/perf/arm_pmu.h>
#include <linux/platform_device.h>
#include <linux/printk.h>
#include <linux/smp.h>

static int probe_current_pmu(struct arm_pmu *pmu,
			     const struct pmu_probe_info *info)
{
	int cpu = get_cpu();
	unsigned int cpuid = read_cpuid_id();
	int ret = -ENODEV;

	pr_info("probing PMU on CPU %d\n", cpu);

	for (; info->init != NULL; info++) {
		if ((cpuid & info->mask) != info->cpuid)
			continue;
		ret = info->init(pmu);
		break;
	}

	put_cpu();
	return ret;
}

static int pmu_parse_percpu_irq(struct arm_pmu *pmu, int irq)
{
	int cpu, ret;
	struct pmu_hw_events __percpu *hw_events = pmu->hw_events;

	ret = irq_get_percpu_devid_partition(irq, &pmu->supported_cpus);
	if (ret)
		return ret;

	for_each_cpu(cpu, &pmu->supported_cpus)
		per_cpu(hw_events->irq, cpu) = irq;

	return 0;
}

static bool pmu_has_irq_affinity(struct device_node *node)
{
	return !!of_find_property(node, "interrupt-affinity", NULL);
}

static int pmu_parse_irq_affinity(struct device_node *node, int i)
{
	struct device_node *dn;
	int cpu;

	/*
	 * If we don't have an interrupt-affinity property, we guess irq
	 * affinity matches our logical CPU order, as we used to assume.
	 * This is fragile, so we'll warn in pmu_parse_irqs().
	 */
	if (!pmu_has_irq_affinity(node))
		return i;

	dn = of_parse_phandle(node, "interrupt-affinity", i);
	if (!dn) {
		pr_warn("failed to parse interrupt-affinity[%d] for %pOFn\n",
			i, node);
		return -EINVAL;
	}

	cpu = of_cpu_node_to_id(dn);
	if (cpu < 0) {
		pr_warn("failed to find logical CPU for %pOFn\n", dn);
		cpu = nr_cpu_ids;
	}

	of_node_put(dn);

	return cpu;
}

static int pmu_parse_irqs(struct arm_pmu *pmu)
{
	int i = 0, num_irqs;
	struct platform_device *pdev = pmu->plat_device;
	struct pmu_hw_events __percpu *hw_events = pmu->hw_events;

	num_irqs = platform_irq_count(pdev);
	if (num_irqs < 0) {
		pr_err("unable to count PMU IRQs\n");
		return num_irqs;
	}

	/*
	 * In this case we have no idea which CPUs are covered by the PMU.
	 * To match our prior behaviour, we assume all CPUs in this case.
	 */
	if (num_irqs == 0) {
		pr_warn("no irqs for PMU, sampling events not supported\n");
		pmu->pmu.capabilities |= PERF_PMU_CAP_NO_INTERRUPT;
		cpumask_setall(&pmu->supported_cpus);
		return 0;
	}

	if (num_irqs == 1) {
		int irq = platform_get_irq(pdev, 0);
		if (irq && irq_is_percpu_devid(irq))
			return pmu_parse_percpu_irq(pmu, irq);
	}

	if (nr_cpu_ids != 1 && !pmu_has_irq_affinity(pdev->dev.of_node)) {
		pr_warn("no interrupt-affinity property for %pOF, guessing.\n",
			pdev->dev.of_node);
	}

	for (i = 0; i < num_irqs; i++) {
		int cpu, irq;

		irq = platform_get_irq(pdev, i);
		if (WARN_ON(irq <= 0))
			continue;

		if (irq_is_percpu_devid(irq)) {
			pr_warn("multiple PPIs or mismatched SPI/PPI detected\n");
			return -EINVAL;
		}

		cpu = pmu_parse_irq_affinity(pdev->dev.of_node, i);
		if (cpu < 0)
			return cpu;
		if (cpu >= nr_cpu_ids)
			continue;

		if (per_cpu(hw_events->irq, cpu)) {
			pr_warn("multiple PMU IRQs for the same CPU detected\n");
			return -EINVAL;
		}

		per_cpu(hw_events->irq, cpu) = irq;
		cpumask_set_cpu(cpu, &pmu->supported_cpus);
	}

	return 0;
}

static int armpmu_request_irqs(struct arm_pmu *armpmu)
{
	struct pmu_hw_events __percpu *hw_events = armpmu->hw_events;
	int cpu, err = 0;

	for_each_cpu(cpu, &armpmu->supported_cpus) {
		int irq = per_cpu(hw_events->irq, cpu);
		if (!irq)
			continue;

		err = armpmu_request_irq(irq, cpu);
		if (err)
			break;
	}

	return err;
}

static void armpmu_free_irqs(struct arm_pmu *armpmu)
{
	int cpu;
	struct pmu_hw_events __percpu *hw_events = armpmu->hw_events;

	for_each_cpu(cpu, &armpmu->supported_cpus) {
		int irq = per_cpu(hw_events->irq, cpu);

		armpmu_free_irq(irq, cpu);
	}
}

#ifdef CONFIG_AMLOGIC_MODIFY
#include <linux/of_address.h>
#include <linux/delay.h>

struct amlpmu_context amlpmu_ctx;

static void amlpmu_fix_setup_affinity(int irq)
{
	int cluster_index = 0;
	int cpu;
	int affinity_cpu = -1;
	struct amlpmu_cpuinfo *ci = NULL;
	struct amlpmu_context *ctx = &amlpmu_ctx;
	s64 latest_next_stamp = S64_MAX;

	if (irq == ctx->irqs[0]) {
		cluster_index = 0;
	} else if (ctx->clusterb_enabled && irq == ctx->irqs[1]) {
		cluster_index = 1;
	} else {
		pr_err("%s() bad irq = %d\n", __func__, irq);
		return;
	}

	/*
	 * find latest next_predicted_stamp cpu for affinity cpu
	 * if no cpu have predict time, select first cpu of cpumask
	 * todo:
	 * - if a cpu predict failed for continuous N times,
	 *   try add some punishment.
	 * - if no cpu have predicted time, try recently most used cpu
	 *   for affinity
	 * - try to keep and promote prediction accuracy
	 */
	for_each_cpu_and(cpu,
			 &ctx->cpumasks[cluster_index],
			 cpu_possible_mask) {
		ci = per_cpu_ptr(ctx->cpuinfo, cpu);
		//pr_info("cpu = %d, ci->next_predicted_stamp = %lld\n",
		//	cpu, ci->next_predicted_stamp);
		if (ci->next_predicted_stamp &&
		    ci->next_predicted_stamp < latest_next_stamp) {
			latest_next_stamp = ci->next_predicted_stamp;
			affinity_cpu = cpu;
		}
	}

	if (affinity_cpu == -1) {
		affinity_cpu = cpumask_first(&ctx->cpumasks[cluster_index]);
		pr_debug("used first cpu: %d, cluster: 0x%lx\n",
			 affinity_cpu,
			 *cpumask_bits(&ctx->cpumasks[cluster_index]));
	} else {
		pr_debug("find affinity cpu: %d, next_predicted_stamp: %lld\n",
			 affinity_cpu,
			 latest_next_stamp);
	}

	if (irq_set_affinity(irq, cpumask_of(affinity_cpu)))
		pr_err("irq_set_affinity() failed irq: %d, affinity_cpu: %d\n",
		       irq,
		       affinity_cpu);
}

/*
 * on pmu interrupt generated cpu, @irq_num is valid
 * on other cpus(called by AML_PMU_IPI), @irq_num is -1
 */
static void amlpmu_irq_fix(int irq_num)
{
	int cpu;
	int cur_cpu;
	int pmuirq_val;
	int cluster_index = 0;
	struct amlpmu_context *ctx = &amlpmu_ctx;

	cur_cpu = smp_processor_id();

	if (irq_num == ctx->irqs[0]) {
		cluster_index = 0;
	} else if (ctx->clusterb_enabled && irq_num == ctx->irqs[1]) {
		cluster_index = 1;
	} else {
		pr_err("%s() bad irq = %d\n", __func__, irq_num);
		return;
	}

	if (!cpumask_test_cpu(cur_cpu, &ctx->cpumasks[cluster_index])) {
		pr_warn("%s() cur_cpu %d not in cluster: 0x%lx\n",
			__func__,
			cur_cpu,
			*cpumask_bits(&ctx->cpumasks[cluster_index]));
	}

	pmuirq_val = readl(ctx->regs[cluster_index]);
	pmuirq_val &= 0xf;
	pmuirq_val <<= ctx->first_cpus[cluster_index];

	pr_debug("%s() val=0x%0x, first_cpu=%d, cluster=0x%lx\n",
		 __func__,
		 readl(ctx->regs[cluster_index]),
		 ctx->first_cpus[cluster_index],
		 *cpumask_bits(&ctx->cpumasks[cluster_index]));

	/*
	 * if pmuirq_val is zero means we can't get irq cpu info
	 * from the register(eg: gxm clusterb), so we have to select another
	 * cpu(next cpu) in cluster to try to handle this irq.
	 */
	if (!pmuirq_val) {
		int next_cpu = -1;

		for_each_cpu_and(cpu,
				&ctx->cpumasks[cluster_index],
				cpu_online_mask) {
			if (cpu > cur_cpu) {
				next_cpu = cpu;
				break;
			}
		}

		if (next_cpu == -1) {
			for_each_cpu_and(cpu,
					&ctx->cpumasks[cluster_index],
					cpu_online_mask) {
				if (cpu < cur_cpu) {
					next_cpu = cpu;
					break;
				}
			}
		}

		if (next_cpu != -1) {
			if (irq_set_affinity(irq_num, cpumask_of(cpu)))
				pr_err("irq_set_affin failed, irq=%d cpu=%d\n",
					irq_num,
					cpu);
		} else {
			pr_err("can't find nextcpu\n");
		}

		return;
	}

	/* fix irq from register info */
	for_each_cpu_and(cpu,
			 &ctx->cpumasks[cluster_index],
			 cpu_online_mask) {
		if (!(pmuirq_val & 1 << cpu))
			continue;

		if (cpu == cur_cpu)
			continue;

		pr_debug("fix pmu irq cpu=%d, pmuirq=0x%x\n", cpu, pmuirq_val);

		if (irq_set_affinity(irq_num, cpumask_of(cpu)))
			pr_err("irq_set_affinity() failed, irq=%d cpu=%d\n",
					irq_num,
					cpu);

		return;
	}

	return;
}

static void amlpmu_update_stats(int irq_num, int has_overflowed)
{
	int freq;
	int i;
	ktime_t stamp;
	unsigned long time = jiffies;
	struct amlpmu_cpuinfo *ci;
	struct amlpmu_context *ctx = &amlpmu_ctx;

	ci = this_cpu_ptr(ctx->cpuinfo);

	if (has_overflowed) {
		ci->valid_irq_cnt++;
		ci->valid_irq_time = time;

		stamp = ktime_get();
		ci->stamp_deltas[ci->valid_irq_cnt % MAX_DELTA_CNT] =
			stamp - ci->last_stamp;
		ci->last_stamp = stamp;

		/* update avg_delta if it's valid */
		ci->avg_delta = 0;
		for (i = 0; i < MAX_DELTA_CNT; i++)
			ci->avg_delta += ci->stamp_deltas[i];

		ci->avg_delta /= MAX_DELTA_CNT;
		for (i = 0; i < MAX_DELTA_CNT; i++) {
			if ((ci->stamp_deltas[i] > ci->avg_delta * 3 / 2) ||
			    (ci->stamp_deltas[i] < ci->avg_delta / 2)) {
				ci->avg_delta = 0;
				break;
			}
		}

		if (ci->avg_delta)
			ci->next_predicted_stamp =
				ci->last_stamp + ci->avg_delta;
		else
			ci->next_predicted_stamp = 0;

		pr_debug("irq_num = %d, valid_irq_cnt = %lu\n",
			 irq_num,
			 ci->valid_irq_cnt);
		pr_debug("cur_delta = %lld, avg_delta = %lld, next = %lld\n",
			 ci->stamp_deltas[ci->valid_irq_cnt % MAX_DELTA_CNT],
			 ci->avg_delta,
			 ci->next_predicted_stamp);
	}

	if (time_after(ci->valid_irq_time, ci->last_valid_irq_time + 2 * HZ)) {
		freq = ci->valid_irq_cnt - ci->last_valid_irq_cnt;
		freq *= HZ;
		freq /= (ci->valid_irq_time - ci->last_valid_irq_time);
		pr_info("######## valid_irq_cnt: %lu - %lu = %lu, freq = %d\n",
			ci->valid_irq_cnt,
			ci->last_valid_irq_cnt,
			ci->valid_irq_cnt - ci->last_valid_irq_cnt,
			freq);

		ci->last_valid_irq_cnt = ci->valid_irq_cnt;
		ci->last_valid_irq_time = ci->valid_irq_time;
	}
}

void amlpmu_handle_irq(struct arm_pmu *cpu_pmu, int irq_num, int has_overflowed)
{
		int cpu;
		struct amlpmu_cpuinfo *ci;
		struct amlpmu_context *ctx = &amlpmu_ctx;

		ci = this_cpu_ptr(ctx->cpuinfo);
		ci->irq_num = irq_num;
		cpu = smp_processor_id();

		pr_debug("%s() irq_num = %d, overflowed = %d\n",
			 __func__,
			 irq_num,
			 has_overflowed);

		/*
		 * if current cpu is not overflowed, it's possible some other
		 * cpus caused the pmu interrupt.
		 * so if current cpu is interrupt generated cpu(irq_num != -1),
		 * call aml_pmu_fix() try to fix it.
		 */
		if (!has_overflowed)
			amlpmu_irq_fix(irq_num);

		/*
		 * valid_irq status
		 * avg_delta time account to predict next interrupt time
		 */
		amlpmu_update_stats(irq_num, has_overflowed);

		if (has_overflowed)
			amlpmu_fix_setup_affinity(irq_num);
}

static int amlpmu_init(struct platform_device *pdev, struct arm_pmu *pmu)
{
	int cpu;
	int ret = 0;
	int irq;
	u32 cpumasks[MAX_CLUSTER_NR] = {0};
	struct amlpmu_context *ctx = &amlpmu_ctx;
	struct amlpmu_cpuinfo *ci;
	int cluster_nr = 1;

	memset(ctx, 0, sizeof(*ctx));

	/* each cpu has it's own pmu interrupt */
	if (of_property_read_bool(pdev->dev.of_node, "private-interrupts")) {
		ctx->private_interrupts = 1;
		return 0;
	}

	ctx->cpuinfo = __alloc_percpu_gfp(sizeof(struct amlpmu_cpuinfo),
					  SMP_CACHE_BYTES,
					  GFP_KERNEL | __GFP_ZERO);
	if (!ctx->cpuinfo) {
		pr_err("alloc percpu failed\n");
		ret = -ENOMEM;
		goto free;
	}

	for_each_possible_cpu(cpu) {
		ci = per_cpu_ptr(ctx->cpuinfo, cpu);
		ci->last_valid_irq_time = INITIAL_JIFFIES;
		ci->last_fix_irq_time = INITIAL_JIFFIES;
		ci->last_empty_irq_time = INITIAL_JIFFIES;
	}

	ctx->pmu = pmu;

	if (of_property_read_bool(pdev->dev.of_node, "clusterb-enabled")) {
		ctx->clusterb_enabled = 1;
		cluster_nr = MAX_CLUSTER_NR;
	}

	pr_info("clusterb_enabled = %d\n", ctx->clusterb_enabled);

	ret = of_property_read_u32_array(pdev->dev.of_node,
					 "cpumasks",
					 cpumasks,
					 cluster_nr);
	if (ret) {
		pr_err("read prop cpumasks failed, ret = %d\n", ret);
		ret = -EINVAL;
		goto free;
	}
	pr_info("cpumasks 0x%0x, 0x%0x\n", cpumasks[0], cpumasks[1]);

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		pr_err("get clusterA irq failed, %d\n", irq);
		ret = -EINVAL;
		goto free;
	}
	ctx->irqs[0] = irq;
	pr_info("cluster A irq = %d\n", irq);

	ctx->regs[0] = of_iomap(pdev->dev.of_node, 0);
	if (IS_ERR(ctx->regs[0])) {
		pr_err("of_iomap() clusterA failed, base = %p\n", ctx->regs[0]);
		ret = PTR_ERR(ctx->regs[0]);
		goto free;
	}

	cpumask_clear(&ctx->cpumasks[0]);
	memcpy(cpumask_bits(&ctx->cpumasks[0]),
	       &cpumasks[0],
	       sizeof(cpumasks[0]));
	if (!cpumask_intersects(&ctx->cpumasks[0], cpu_possible_mask)) {
		pr_err("bad cpumasks[0] 0x%x\n",  cpumasks[0]);
		ret = -EINVAL;
		goto free;
	}
	ctx->first_cpus[0] = cpumask_first(&ctx->cpumasks[0]);

	for_each_cpu(cpu, &ctx->cpumasks[0]) {
		cpumask_set_cpu(cpu, &pmu->supported_cpus);
	}

	amlpmu_fix_setup_affinity(ctx->irqs[0]);

	if (!ctx->clusterb_enabled)
		return 0;

	irq = platform_get_irq(pdev, 1);
	if (irq < 0) {
		pr_err("get clusterB irq failed, %d\n", irq);
		ret = -EINVAL;
		goto free;
	}
	ctx->irqs[1] = irq;
	pr_info("cluster B irq = %d\n", irq);

	ctx->regs[1] = of_iomap(pdev->dev.of_node, 1);
	if (IS_ERR(ctx->regs[1])) {
		pr_err("of_iomap() clusterA failed, base = %p\n", ctx->regs[1]);
		ret = PTR_ERR(ctx->regs[1]);
		goto free;
	}

	cpumask_clear(&ctx->cpumasks[1]);
	memcpy(cpumask_bits(&ctx->cpumasks[1]),
	       &cpumasks[1],
	       sizeof(cpumasks[1]));
	if (!cpumask_intersects(&ctx->cpumasks[1], cpu_possible_mask)) {
		pr_err("bad cpumasks[1] 0x%x\n",  cpumasks[1]);
		ret = -EINVAL;
		goto free;
	} else if (cpumask_intersects(&ctx->cpumasks[0], &ctx->cpumasks[1])) {
		pr_err("cpumasks intersect 0x%x : 0x%x\n",
		       cpumasks[0],
		       cpumasks[1]);
		ret = -EINVAL;
		goto free;
	}
	ctx->first_cpus[1] = cpumask_first(&ctx->cpumasks[1]);

	for_each_cpu(cpu, &ctx->cpumasks[1]) {
		cpumask_set_cpu(cpu, &pmu->supported_cpus);
	}

	amlpmu_fix_setup_affinity(ctx->irqs[1]);

	return 0;

free:
	if (ctx->cpuinfo)
		free_percpu(ctx->cpuinfo);

	if (ctx->regs[0])
		iounmap(ctx->regs[0]);

	if (ctx->regs[1])
		iounmap(ctx->regs[1]);

	return ret;
}

#endif

int arm_pmu_device_probe(struct platform_device *pdev,
			 const struct of_device_id *of_table,
			 const struct pmu_probe_info *probe_table)
{
	const struct of_device_id *of_id;
	armpmu_init_fn init_fn;
	struct device_node *node = pdev->dev.of_node;
	struct arm_pmu *pmu;
	int ret = -ENODEV;

	pmu = armpmu_alloc();
	if (!pmu)
		return -ENOMEM;

	pmu->plat_device = pdev;

	ret = pmu_parse_irqs(pmu);
	if (ret)
		goto out_free;

	if (node && (of_id = of_match_node(of_table, pdev->dev.of_node))) {
		init_fn = of_id->data;

		pmu->secure_access = of_property_read_bool(pdev->dev.of_node,
							   "secure-reg-access");

		/* arm64 systems boot only as non-secure */
		if (IS_ENABLED(CONFIG_ARM64) && pmu->secure_access) {
			pr_warn("ignoring \"secure-reg-access\" property for arm64\n");
			pmu->secure_access = false;
		}

		ret = init_fn(pmu);
	} else if (probe_table) {
		cpumask_setall(&pmu->supported_cpus);
		ret = probe_current_pmu(pmu, probe_table);
	}

	if (ret) {
		pr_info("%pOF: failed to probe PMU!\n", node);
		goto out_free;
	}

	ret = armpmu_request_irqs(pmu);
	if (ret)
		goto out_free_irqs;

	ret = armpmu_register(pmu);
	if (ret)
		goto out_free_irqs;

#ifdef CONFIG_AMLOGIC_MODIFY
		if (amlpmu_init(pdev, pmu)) {
			pr_err("amlpmu_init() failed\n");
			return 1;
		}
#endif

	return 0;

out_free_irqs:
	armpmu_free_irqs(pmu);
out_free:
	pr_info("%pOF: failed to register PMU devices!\n", node);
	armpmu_free(pmu);
	return ret;
}
