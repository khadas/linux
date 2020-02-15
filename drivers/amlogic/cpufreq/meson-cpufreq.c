// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt
#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/device.h>
#include <linux/export.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/pm_opp.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/cpumask.h>
#include <linux/mutex.h>
#include <linux/of_platform.h>
#include <linux/topology.h>
#include <linux/regulator/consumer.h>
#include <linux/delay.h>
#include <linux/regulator/driver.h>
#include "../../regulator/internal.h"
#include <linux/amlogic/scpi_protocol.h>
#include "meson-cpufreq.h"
#include <linux/energy_model.h>
#include <linux/cpu_cooling.h>
#include <linux/thermal.h>
#include <linux/arm-smccc.h>

#define CREATE_TRACE_POINTS
/*whether use different tables or not*/
static bool cpufreq_voltage_set_skip;
static unsigned int get_cpufreq_table_index(u64 function_id,
					    u64 arg0, u64 arg1, u64 arg2)
{
	struct arm_smccc_res res;

	arm_smccc_smc((unsigned long)function_id,
		      (unsigned long)arg0,
		      (unsigned long)arg1,
		      (unsigned long)arg2,
		      0, 0, 0, 0, &res);
	return res.a0;
}

static unsigned int meson_cpufreq_get_rate(unsigned int cpu)
{
	u32 cur_cluster = topology_physical_package_id(cpu);
	u32 rate = clk_get_rate(clk[cur_cluster]) / 1000;

	pr_debug("%s: cpu: %d, cluster: %d, freq: %u\n",
		 __func__, cpu, cur_cluster, rate);

	return rate;
}

static unsigned int meson_cpufreq_set_rate(struct cpufreq_policy *policy,
					   u32 cur_cluster, u32 rate)
{
	struct clk  *low_freq_clk_p, *high_freq_clk_p;
	struct meson_cpufreq_driver_data *cpufreq_data;
	u32 new_rate;
	int ret, cpu = 0;

	cpu = policy->cpu;
	cpufreq_data = policy->driver_data;
	high_freq_clk_p = cpufreq_data->high_freq_clk_p;
	low_freq_clk_p = cpufreq_data->low_freq_clk_p;

	mutex_lock(&cluster_lock[cur_cluster]);
	new_rate = rate;

	pr_debug("%s: cpu: %d, new cluster: %d, freq: %d\n",
		 __func__, cpu, cur_cluster, new_rate);

	if (new_rate > mid_rate) {
		/*
		 * do not enable sys pll clock, it will
		 * enable when call clk_set_rate, otherwise it
		 * will wait lock for sys pll for long time, it run
		 * afer spin_lock_irqsave.it could not get the lock
		 * for a long time.
		 */
		ret = clk_set_parent(clk[cur_cluster], low_freq_clk_p);
		if (ret) {
			pr_err("%s: error in setting low_freq_clk_p as parent\n",
			       __func__);
			mutex_unlock(&cluster_lock[cur_cluster]);
			return ret;
		}
		ret = clk_set_rate(high_freq_clk_p, new_rate * 1000);
		if (ret) {
			pr_err("%s: error in setting high_freq_clk_p rate!\n",
			       __func__);
			mutex_unlock(&cluster_lock[cur_cluster]);
			return ret;
		}

		ret =  clk_prepare_enable(high_freq_clk_p);
		if (ret)
			pr_err("enable high freq clk failed!\n");
		ret = clk_set_parent(clk[cur_cluster], high_freq_clk_p);
		if (ret) {
			pr_err("%s: error in setting high_freq_clk_p as parent\n",
			       __func__);
		}

	} else {
		ret = clk_set_rate(low_freq_clk_p, new_rate * 1000);
		if (ret) {
			pr_err("%s: error in setting low_freq_clk_p rate!\n",
			       __func__);
			mutex_unlock(&cluster_lock[cur_cluster]);
			return ret;
		}

		ret = clk_set_parent(clk[cur_cluster], low_freq_clk_p);
		if (ret) {
			pr_err("%s: error in setting low_freq_clk_p rate!\n",
			       __func__);
			mutex_unlock(&cluster_lock[cur_cluster]);
			return ret;
		}

		if (__clk_is_enabled(high_freq_clk_p) &&
		    __clk_get_enable_count(high_freq_clk_p) != 0)
			clk_disable_unprepare(high_freq_clk_p);
	}

	if (!ret) {
		/*
		 * FIXME: clk_set_rate hasn't returned an error here however it
		 * may be that clk_change_rate failed due to hardware or
		 * firmware issues and wasn't able to report that due to the
		 * current design of the clk core layer. To work around this
		 * problem we will read back the clock rate and check it is
		 * correct. This needs to be removed once clk core is fixed.
		 */
		if (abs(clk_get_rate(clk[cur_cluster]) - new_rate * 1000)
				> gap_rate) {
			mutex_unlock(&cluster_lock[cur_cluster]);
			ret = -EIO;
		}
	}

	if (WARN_ON(ret)) {
		pr_err("clk_set_rate failed: %d, new cluster: %d\n", ret,
		       cur_cluster);
		mutex_unlock(&cluster_lock[cur_cluster]);
		return ret;
	}

	mutex_unlock(&cluster_lock[cur_cluster]);
	return 0;
}

static int meson_regulator_set_volate(struct regulator *regulator, int old_uv,
				      int new_uv, int tol_uv)
{
	int cur, to, vol_cnt = 0;
	int ret = 0;
	int temp_uv = 0;
	struct regulator_dev *rdev = regulator->rdev;

	if (cpufreq_voltage_set_skip)
		return ret;
	cur = regulator_map_voltage_iterate(rdev, old_uv, old_uv + tol_uv);
	to = regulator_map_voltage_iterate(rdev, new_uv, new_uv + tol_uv);
	vol_cnt = regulator_count_voltages(regulator);
	pr_debug("%s:old_uv:%d,cur:%d----->new_uv:%d,to:%d,vol_cnt=%d\n",
		 __func__, old_uv, cur, new_uv, to, vol_cnt);

	if (to >= vol_cnt)
		to = vol_cnt - 1;

	if (cur < 0 || cur >= vol_cnt) {
		temp_uv = regulator_list_voltage(regulator, to);
		ret = regulator_set_voltage_tol(regulator, temp_uv, temp_uv
						+ tol_uv);
		usleep_range(190, 200);
		return ret;
	}

	while (cur != to) {
		/* adjust to target voltage step by step */
		if (cur < to) {
			if (cur < to - 3)
				cur += 3;
			else
				cur = to;
		} else {
			if (cur > to + 3)
				cur -= 3;
			else
				cur = to;
		}
		temp_uv = regulator_list_voltage(regulator, cur);
		ret = regulator_set_voltage_tol(regulator, temp_uv,
						temp_uv + tol_uv);

		pr_debug("%s:temp_uv:%d, cur:%d, change_cur_uv:%d\n", __func__,
			 temp_uv, cur, regulator_get_voltage(regulator));
		usleep_range(190, 200);
	}
	return ret;
}

static int meson_regulator_get_volate(struct regulator *regulator)
{
	int ret = 0;

	if (cpufreq_voltage_set_skip)
		return ret;
	ret = regulator_get_voltage(regulator);

	return ret;
}

/* Set clock frequency */
static int meson_cpufreq_set_target(struct cpufreq_policy *policy,
				    unsigned int index)
{
	struct dev_pm_opp *opp;
	u32 cpu, cur_cluster;
	unsigned long freq_new, freq_old;
	unsigned int volt_new = 0, volt_old = 0, volt_tol = 0;
	struct meson_cpufreq_driver_data *cpufreq_data;
	struct device *cpu_dev;
	struct regulator *cpu_reg;
	int ret = 0;

	if (!policy) {
		pr_err("invalid policy, returning\n");
		return -ENODEV;
	}

	cpu = policy->cpu;
	cpufreq_data = policy->driver_data;
	cpu_dev = cpufreq_data->cpu_dev;
	cpu_reg = cpufreq_data->reg;
	cur_cluster = topology_physical_package_id(cpu);

	pr_debug("setting target for cpu %d, index =%d\n", policy->cpu, index);

	freq_new = freq_table[cur_cluster][index].frequency * 1000;

	if (!IS_ERR(cpu_reg)) {
		opp = dev_pm_opp_find_freq_ceil(cpu_dev, &freq_new);
		if (IS_ERR(opp)) {
			pr_err("failed to find OPP for %lu Khz\n",
			       freq_new / 1000);
			return PTR_ERR(opp);
		}
		volt_new = dev_pm_opp_get_voltage(opp);
		volt_old = meson_regulator_get_volate(cpu_reg);
		volt_tol = volt_new * cpufreq_data->volt_tol / 100;
		pr_debug("Found OPP: %lu kHz, %u, tolerance: %u\n",
			 freq_new / 1000, volt_new, volt_tol);
	}
	freq_old = clk_get_rate(clk[cur_cluster]);

	pr_debug("Scalling from %lu MHz, %u mV,cur_cluster_id:%u, --> %lu MHz, %u mV,new_cluster_id:%u\n",
		 freq_old / 1000000, (volt_old > 0) ? volt_old / 1000 : -1,
		 cur_cluster,
		 freq_new / 1000000, volt_new ? volt_new / 1000 : -1,
		 cur_cluster);

	/*cpufreq up,change voltage before frequency*/
	if (freq_new > freq_old) {
		ret = meson_regulator_set_volate(cpu_reg, volt_old,
						 volt_new, volt_tol);
		if (ret) {
			pr_err("failed to scale voltage %u %u up: %d\n",
			       volt_new, volt_tol, ret);
			return ret;
		}
	}

	if (cur_cluster == 0) {
		freqs.old = freq_old / 1000;
		freqs.new = freq_new / 1000;
		cpufreq_freq_transition_begin(policy, &freqs);
	}
	/*scale clock frequency*/
	ret = meson_cpufreq_set_rate(policy, cur_cluster,
				     freq_new / 1000);
	if (ret) {
		pr_err("failed to set clock %luMhz rate: %d\n",
		       freq_new / 1000000, ret);
		if (volt_old > 0 && freq_new > freq_old) {
			pr_debug("scaling to old voltage %u\n", volt_old);
			meson_regulator_set_volate(cpu_reg, volt_old, volt_old,
						   volt_tol);
		}
		return ret;
	}

	if (cur_cluster == 0)
		cpufreq_freq_transition_end(policy, &freqs, ret);
	/*cpufreq down,change voltage after frequency*/
	if (freq_new < freq_old) {
		ret = meson_regulator_set_volate(cpu_reg, volt_old,
						 volt_new, volt_tol);
		if (ret) {
			pr_err("failed to scale volt %u %u down: %d\n",
			       volt_new, volt_tol, ret);
			if (cur_cluster == 0) {
				freqs.old = freq_new / 1000;
				freqs.new = freq_old / 1000;
				cpufreq_freq_transition_begin(policy, &freqs);
			}
			meson_cpufreq_set_rate(policy, cur_cluster,
					       freq_old / 1000);
			if (cur_cluster == 0)
				cpufreq_freq_transition_end(policy,
							    &freqs, ret);
		}
	}

	freq_new = clk_get_rate(clk[cur_cluster]) / 1000000;
	pr_debug("After transition, new lk rate %luMhz, volt %dmV\n",
		 freq_new, meson_regulator_get_volate(cpu_reg) / 1000);
	return ret;
}

/* get the maximum frequency in the cpufreq_frequency_table */
static inline u32 get_table_max(struct cpufreq_frequency_table *table)
{
	struct cpufreq_frequency_table *pos;
	u32 max_freq = 0;

	cpufreq_for_each_entry(pos, table)
		if (pos->frequency > max_freq)
			max_freq = pos->frequency;
	return max_freq;
}

int choose_cpufreq_tables_index(const struct device_node *np, u32 cur_cluster)
{
	int ret = 0;

	cpufreq_tables_supply = of_property_read_bool(np,
						      "multi_tables_available");
	if (cpufreq_tables_supply) {
		/*choose appropriate cpufreq tables according efuse info*/
		ret = get_cpufreq_table_index(GET_DVFS_TABLE_INDEX,
					      cur_cluster, 0, 0);
		pr_info("%s:tables_index %u\n", __func__, ret);
	}

	return ret;
}

static int meson_cpufreq_transition_notifier(struct notifier_block *nb,
					     unsigned long val, void *data)
{
	struct cpufreq_freqs *freq = data;
	struct meson_cpufreq_driver_data *cpufreq_data =
					to_meson_dvfs_cpu_nb(nb);
	struct cpufreq_policy *policy = cpufreq_data->policy;
	struct clk *dsu_clk = cpufreq_data->clk_dsu;
	struct clk *dsu_cpu_parent =  policy->clk;
	struct clk *dsu_pre_parent = cpufreq_data->clk_dsu_pre;
	int ret = 0;
	unsigned int dsu_set_rate;

	if (!dsu_clk || !dsu_cpu_parent || !dsu_pre_parent)
		return 0;

	pr_debug("%s:event %ld,old_rate =%u,new_rate =%u!\n",
		 __func__, val, freq->old, freq->new);
	switch (val) {
	case CPUFREQ_PRECHANGE:
		if (freq->new > DSU_LOW_RATE) {
			pr_debug("%s:dsu clk switch parent to dsu pre!\n",
				 __func__);
			if (__clk_get_enable_count(dsu_pre_parent) == 0) {
				ret = clk_prepare_enable(dsu_pre_parent);
				if (ret) {
					pr_err("%s: CPU%d gp1 pll enable failed,ret = %d\n",
					       __func__, policy->cpu, ret);
					return ret;
				}
			}

			if (freq->new > CPU_CMP_RATE)
				dsu_set_rate = DSU_HIGH_RATE;
			else
				dsu_set_rate = DSU_LOW_RATE;

			clk_set_rate(dsu_pre_parent, dsu_set_rate * 1000);
			if (ret) {
				pr_err("%s: GP1 clk setting %u MHz failed, ret = %d!\n",
				       __func__, dsu_set_rate, ret);
				return ret;
			}
			pr_debug("%s:GP1 clk setting %u MHz!\n",
				 __func__, dsu_set_rate);

			ret = clk_set_parent(dsu_clk, dsu_pre_parent);
		}

		return ret;
	case CPUFREQ_POSTCHANGE:
		if (freq->new <= DSU_LOW_RATE) {
			pr_debug("%s:dsu clk switch parent to cpu!\n",
				 __func__);
			ret = clk_set_parent(dsu_clk, dsu_cpu_parent);
			if (__clk_get_enable_count(dsu_pre_parent) >= 1)
				clk_disable_unprepare(dsu_pre_parent);
		}

		return ret;
	default:
		return 0;
	}
	return 0;
}

static struct notifier_block meson_cpufreq_notifier_block = {
	.notifier_call = meson_cpufreq_transition_notifier,
};

static void get_cluster_cores(unsigned int cpuid, struct cpumask *dstp)
{
	int cpu;

	cpumask_clear(dstp);
	for_each_possible_cpu(cpu) {
		if (topology_physical_package_id(cpuid) ==
			topology_physical_package_id(cpu))
			cpumask_set_cpu(cpu, dstp);
	}
}

/* CPU initialization */
static int meson_cpufreq_init(struct cpufreq_policy *policy)
{
	u32 cur_cluster;
	struct device *cpu_dev;
	struct device_node *np;
	struct regulator *cpu_reg = NULL;
	struct meson_cpufreq_driver_data *cpufreq_data;
	struct clk *low_freq_clk_p, *high_freq_clk_p = NULL;
	struct clk *dsu_clk, *dsu_pre_parent;
	unsigned int transition_latency = CPUFREQ_ETERNAL;
	unsigned int volt_tol = 0;
	unsigned long freq_hz = 0;
	int cpu = 0, ret = 0, tables_index;
	int nr_opp, tmp_rate;

	if (!policy) {
		pr_err("invalid cpufreq_policy\n");
		return -ENODEV;
	}

	cur_cluster = topology_physical_package_id(policy->cpu);
	cpu = policy->cpu;
	cpu_dev = get_cpu_device(cpu);
	if (IS_ERR(cpu_dev)) {
		pr_err("%s: failed to get cpu%d device\n", __func__,
		       policy->cpu);
		return -ENODEV;
	}

	np = of_node_get(cpu_dev->of_node);
	if (!np) {
		pr_err("ERROR failed to find cpu%d node\n", cpu);
		return -ENOENT;
	}

	cpufreq_data = kmalloc(sizeof(*cpufreq_data), GFP_KERNEL);
	if (IS_ERR(cpufreq_data)) {
		pr_err("%s: failed to alloc cpufreq data!\n", __func__);
		ret = -ENOMEM;
		goto free_np;
	}

	clk[cur_cluster] = of_clk_get_by_name(np, CORE_CLK);
	if (IS_ERR(clk[cur_cluster])) {
		pr_err("failed to get cpu clock, %ld\n",
		       PTR_ERR(clk[cur_cluster]));
		ret = PTR_ERR(clk[cur_cluster]);
		goto free_mem;
	}

	low_freq_clk_p = of_clk_get_by_name(np, LOW_FREQ_CLK_PARENT);
	if (IS_ERR(low_freq_clk_p)) {
		pr_err("%s: Failed to get low parent for cpu: %d, cluster: %d\n",
		       __func__, cpu_dev->id, cur_cluster);
		ret = PTR_ERR(low_freq_clk_p);
		goto free_clk;
	}

	/*setting low_freq_clk_p to 1G,default 24M*/
	ret = clk_set_rate(low_freq_clk_p, mid_rate * 1000);
	if (ret) {
		pr_err("%s: error in setting low_freq_clk_p rate!\n",
		       __func__);
		goto free_clk;
	}

	high_freq_clk_p = of_clk_get_by_name(np, HIGH_FREQ_CLK_PARENT);
	if (IS_ERR(high_freq_clk_p)) {
		pr_err("%s: Failed to get high parent for cpu: %d,cluster: %d\n",
		       __func__, cpu_dev->id, cur_cluster);
		ret = PTR_ERR(high_freq_clk_p);
		goto free_clk;
	}

	dsu_clk = of_clk_get_by_name(np, DSU_CLK);
	if (IS_ERR(dsu_clk)) {
		dsu_clk = NULL;
		pr_info("%s: ignor dsu clk!\n", __func__);
	}

	dsu_pre_parent = of_clk_get_by_name(np, DSU_PRE_PARENT);
	if (IS_ERR(dsu_pre_parent)) {
		dsu_pre_parent = NULL;
		pr_info("%s: ignor dsu pre parent clk!\n", __func__);
	}

	cpufreq_voltage_set_skip = of_property_read_bool(np,
							 "cpufreq_voltage_set_skip");
	cpu_reg = devm_regulator_get(cpu_dev, CORE_SUPPLY);
	if (IS_ERR(cpu_reg)) {
		pr_err("%s:failed to get regulator, %ld\n", __func__,
		       PTR_ERR(cpu_reg));
		cpu_reg = NULL;
	}

	if (of_property_read_u32(np, "voltage-tolerance", &volt_tol))
		volt_tol = DEF_VOLT_TOL;
	pr_info("value of voltage_tolerance %u\n", volt_tol);

	if (!of_property_read_u32(np, "mid-rate",
				  &tmp_rate))
		mid_rate = tmp_rate;
	pr_info("value of mid_rate %u\n", mid_rate);

	get_cluster_cores(cpu, policy->cpus);

	tables_index = choose_cpufreq_tables_index(np, cur_cluster);
	ret = dev_pm_opp_of_add_table_indexed(cpu_dev, tables_index);
	if (ret) {
		pr_err("%s: init_opp_table failed, cpu: %d, cluster: %d, err: %d\n",
		       __func__, cpu_dev->id, cur_cluster, ret);
		goto free_reg;
	}

	ret = dev_pm_opp_init_cpufreq_table(cpu_dev, &freq_table[cur_cluster]);
	if (ret) {
		dev_err(cpu_dev, "%s: failed to init cpufreq table, cpu: %d, err: %d\n",
			__func__, cpu_dev->id, ret);
		goto free_reg;
	}

	policy->freq_table = freq_table[cur_cluster];
	ret = cpufreq_table_validate_and_sort(policy);
	if (ret) {
		dev_err(cpu_dev, "CPU %d, cluster: %d invalid freq table\n",
			policy->cpu, cur_cluster);
		goto free_opp_table;
	}

	if (of_property_read_u32(np, "clock-latency", &transition_latency))
		policy->cpuinfo.transition_latency = CPUFREQ_ETERNAL;
	if (cur_cluster == 0) {
		cpufreq_data->freq_transition = meson_cpufreq_notifier_block;

		ret = cpufreq_register_notifier(&cpufreq_data->freq_transition,
						CPUFREQ_TRANSITION_NOTIFIER);
		if (ret) {
			dev_err(cpu_dev, "failed to register cpufreq notifier!\n");
			goto fail_cpufreq_unregister;
		}
	}

	cpufreq_data->cpu_dev = cpu_dev;
	cpufreq_data->low_freq_clk_p = low_freq_clk_p;
	cpufreq_data->high_freq_clk_p = high_freq_clk_p;
	cpufreq_data->clk_dsu = dsu_clk;
	cpufreq_data->clk_dsu_pre = dsu_pre_parent;
	cpufreq_data->reg = cpu_reg;
	cpufreq_data->volt_tol = volt_tol;
	cpufreq_data->policy = policy;
	policy->driver_data = cpufreq_data;
	policy->clk = clk[cur_cluster];
	policy->cpuinfo.transition_latency = transition_latency;
	policy->suspend_freq = get_table_max(freq_table[cur_cluster]);
	policy->cur = clk_get_rate(clk[cur_cluster]) / 1000;

	/*
	 * if uboot default cpufreq larger than freq_table's max,
	 * it will set freq_table's max.
	 */
	if (policy->cur > policy->suspend_freq)
		freq_hz = policy->suspend_freq * 1000;
	else
		freq_hz =  policy->cur * 1000;

	nr_opp = dev_pm_opp_get_opp_count(cpu_dev);

	dev_info(cpu_dev, "%s: CPU %d initialized\n", __func__, policy->cpu);
	return ret;
fail_cpufreq_unregister:
	if (cur_cluster == 0)
		cpufreq_unregister_notifier(&cpufreq_data->freq_transition,
					    CPUFREQ_TRANSITION_NOTIFIER);
free_opp_table:
	if (policy->freq_table) {
		dev_pm_opp_free_cpufreq_table(cpu_dev,
					      &freq_table[cur_cluster]);
		dev_pm_opp_of_cpumask_remove_table(policy->related_cpus);
	}
free_reg:
	if (!IS_ERR(cpu_reg))
		devm_regulator_put(cpu_reg);
free_clk:
	if (!IS_ERR(clk[cur_cluster]))
		clk_put(clk[cur_cluster]);
	if (!IS_ERR(low_freq_clk_p))
		clk_put(low_freq_clk_p);
	if (!IS_ERR(high_freq_clk_p))
		clk_put(high_freq_clk_p);
free_mem:
	kfree(cpufreq_data);
free_np:
	if (np)
		of_node_put(np);
	return ret;
}

static int meson_cpufreq_exit(struct cpufreq_policy *policy)
{
	struct device *cpu_dev;
	struct meson_cpufreq_driver_data *cpufreq_data;
	int cur_cluster = topology_physical_package_id(policy->cpu);

	cpufreq_data = policy->driver_data;
	if (!cpufreq_data)
		return 0;

	cpufreq_cooling_unregister(cpufreq_data->cdev);

	cpu_dev = get_cpu_device(policy->cpu);
	if (!cpu_dev) {
		pr_err("%s: failed to get cpu%d device\n", __func__,
		       policy->cpu);
		return -ENODEV;
	}
	if (cur_cluster == 0)
		cpufreq_unregister_notifier(&cpufreq_data->freq_transition,
					    CPUFREQ_TRANSITION_NOTIFIER);

	if (policy->freq_table) {
		dev_pm_opp_free_cpufreq_table(cpu_dev,
					      &freq_table[cur_cluster]);
		dev_pm_opp_of_cpumask_remove_table(policy->related_cpus);
	}
	dev_dbg(cpu_dev, "%s: Exited, cpu: %d\n", __func__, policy->cpu);
	kfree(cpufreq_data);

	return 0;
}

static int meson_cpufreq_suspend(struct cpufreq_policy *policy)
{
	return cpufreq_generic_suspend(policy);
}

static int meson_cpufreq_resume(struct cpufreq_policy *policy)
{
	return cpufreq_generic_suspend(policy);
}

static void meson_cpufreq_ready(struct cpufreq_policy *policy)
{
	struct meson_cpufreq_driver_data *cpufreq_data = policy->driver_data;

	cpufreq_data->cdev = of_cpufreq_cooling_register(policy);
}

static struct cpufreq_driver meson_cpufreq_driver = {
	.name			= "arm-big-little",
	.flags			= CPUFREQ_STICKY |
					CPUFREQ_HAVE_GOVERNOR_PER_POLICY |
					CPUFREQ_NEED_INITIAL_FREQ_CHECK |
					CPUFREQ_ASYNC_NOTIFICATION,
	.verify			= cpufreq_generic_frequency_table_verify,
	.target_index	= meson_cpufreq_set_target,
	.get			= meson_cpufreq_get_rate,
	.init			= meson_cpufreq_init,
	.exit			= meson_cpufreq_exit,
	.attr			= cpufreq_generic_attr,
	.suspend		= meson_cpufreq_suspend,
	.resume			= meson_cpufreq_resume,
	.ready			= meson_cpufreq_ready,
};

static int meson_cpufreq_probe(struct platform_device *pdev)
{
	struct device *cpu_dev;
	struct device_node *np;
	unsigned int cpu = 0;
	int ret, i;

	for (i = 0; i < MAX_CLUSTERS; i++)
		mutex_init(&cluster_lock[i]);

	cpu_dev = get_cpu_device(cpu);
	if (!cpu_dev) {
		pr_err("failed to get cpu%d device\n", cpu);
		return -ENODEV;
	}

	np = of_node_get(cpu_dev->of_node);
	if (!np) {
		pr_err("failed to find cpu node\n");
		of_node_put(np);
		return -ENODEV;
	}

	ret = cpufreq_register_driver(&meson_cpufreq_driver);
	if (ret) {
		pr_err("%s: Failed registering platform driver, err: %d\n",
				__func__, ret);
	}

	return ret;
}

static int meson_cpufreq_remove(struct platform_device *pdev)
{
	return cpufreq_unregister_driver(&meson_cpufreq_driver);
}

static const struct of_device_id amlogic_cpufreq_meson_dt_match[] = {
	{	.compatible = "amlogic, cpufreq-meson",
	},
	{},
};

static struct platform_driver meson_cpufreq_platdrv = {
	.driver = {
		.name	= "cpufreq-meson",
		.owner  = THIS_MODULE,
		.of_match_table = amlogic_cpufreq_meson_dt_match,
	},
	.probe		= meson_cpufreq_probe,
	.remove		= meson_cpufreq_remove,
};
module_platform_driver(meson_cpufreq_platdrv);

MODULE_AUTHOR("Amlogic cpufreq driver owner");
MODULE_DESCRIPTION("Generic ARM big LITTLE cpufreq driver via DTS");
MODULE_LICENSE("GPL v2");
