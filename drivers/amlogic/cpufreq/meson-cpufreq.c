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
#include "../../opp/opp.h"
#include <linux/amlogic/scpi_protocol.h>
#include "meson-cpufreq.h"
#include <linux/energy_model.h>
#include <linux/cpu_cooling.h>
#include <linux/thermal.h>
#include <linux/arm-smccc.h>
#include <linux/proc_fs.h>

#define CREATE_TRACE_POINTS
/*whether use different tables or not*/
static bool cpufreq_voltage_set_skip;
static DEFINE_MUTEX(cpufreq_target_lock);
struct proc_dir_entry *cpufreq_proc;
static int opp_table_index[MAX_CLUSTERS];
static u32 dsu_voltage_vote_result[VOTER_NUM];
static u32 dsu_freq_vote_result[VOTER_NUM];
#define MAX(x1, x2) ({ \
	typeof(x1) _x1 = x1; \
	typeof(x2) _x2 = x2; \
	(_x1 > _x2 ? _x1 : _x2); })
#ifdef CONFIG_AMLOGIC_MODIFY
static unsigned int freqmax0;
module_param(freqmax0, uint, 0644);
static unsigned int freqmax1;
module_param(freqmax1, uint, 0644);
#endif

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
	struct cpufreq_policy policy;
	struct meson_cpufreq_driver_data *cpufreq_data;
	u32 cur_cluster;
	u32 rate = 0;

	if (!cpufreq_get_policy(&policy, cpu)) {
		cpufreq_data = policy.driver_data;
		cur_cluster = cpufreq_data->clusterid;
		rate = clk_get_rate(clk[cur_cluster]) / 1000;
		pr_debug("%s: cpu: %d, cluster: %d, freq: %u\n",
		 __func__, cpu, cur_cluster, rate);
	}

	return rate;
}

static int meson_set_shared_dsu_rate(struct clk *dsu,
	struct clk *dyn_parent, struct clk *gp1_parent)
{
	int ret = 0;
	struct clk *dsu_parent;
	unsigned int dsu_set_rate = MAX(dsu_freq_vote_result[LITTLE_VOTER],
		dsu_freq_vote_result[BIG_VOTER]);

	dsu_parent = dsu_set_rate > low_dsu_rate ? gp1_parent : dyn_parent;
	pr_debug("[%s %d][%s]dsu_set_rate=%uMHz begin\n",
		__func__, __LINE__, __clk_get_name(dsu_parent), dsu_set_rate / 1000);
	ret = clk_set_rate(dsu_parent, dsu_set_rate * 1000);
	if (ret) {
		pr_err("%s: dsu parent clk set %u MHz failed, ret = %d!\n",
		       __func__, dsu_set_rate, ret);
		return ret;
	}
	ret = clk_prepare_enable(dsu_parent);
	if (ret) {
		pr_err("%s: dsu parent enable failed, ret = %d\n", __func__, ret);
		return ret;
	}
	ret = clk_set_parent(dsu, dsu_parent);
	if (ret) {
		pr_err("%s: dsu set parent failed,ret = %d\n",
		       __func__, ret);
		return ret;
	}
	pr_debug("%s:dsu parent clk set rate succeed!\n", __func__);
	return ret;
}

static int meson_dsufreq_adjust(struct meson_cpufreq_driver_data *cpufreq_data,
				struct cpufreq_freqs *freq, unsigned int state)
{
	struct cpufreq_policy *policy = cpufreq_data->policy;
	struct clk *dsu_clk = cpufreq_data->clk_dsu;
	struct clk *dsu_cpu_parent =  policy->clk;
	struct clk *dsu_pre_parent = cpufreq_data->clk_dsu_pre;
	struct clk *dsu_pre_parent2 = cpufreq_data->clk_dsu_pre2;
	int ret = 0;
	unsigned int dsu_set_rate;

	if (!dsu_clk || !dsu_cpu_parent || !dsu_pre_parent)
		return 0;

	pr_debug("%s:event %u,old_rate =%u,new_rate =%u!\n",
		 __func__, state, freq->old, freq->new);
	switch (state) {
	case CPUFREQ_PRECHANGE:
		if (freq->new > low_dsu_rate) {
			if (cpufreq_data->dsu_clock_shared) {
				meson_set_shared_dsu_rate(dsu_clk, dsu_pre_parent, dsu_pre_parent2);
			} else {
				pr_debug("%s:dsu clk switch parent to dsu pre!\n",
					 __func__);
				if (!__clk_is_enabled(dsu_pre_parent)) {
					ret = clk_prepare_enable(dsu_pre_parent);
					if (ret) {
						pr_err("%s: CPU%d gp1 pll enable failed,ret = %d\n",
						       __func__, policy->cpu,
						       ret);
						return ret;
					}
				}

				if (freq->new > CPU_CMP_RATE)
					dsu_set_rate = DSU_HIGH_RATE;
				else
					dsu_set_rate = low_dsu_rate;

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
		}
		break;
	case CPUFREQ_POSTCHANGE:
		if (freq->new <= low_dsu_rate) {
			if (cpufreq_data->dsu_clock_shared) {
				meson_set_shared_dsu_rate(dsu_clk, dsu_pre_parent, dsu_pre_parent2);
			} else {
				pr_debug("%s:dsu clk switch parent to cpu!\n",
					 __func__);
				ret = clk_set_parent(dsu_clk, dsu_cpu_parent);
				if (__clk_is_enabled(dsu_pre_parent))
					clk_disable_unprepare(dsu_pre_parent);
			}
		}

		break;
	default:
		break;
	}

	return ret;
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

		if (__clk_is_enabled(high_freq_clk_p))
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

static int meson_regulator_set_volate(struct regulator *regulator, u32 cluster_id,
	int old_uv, int new_uv, int tol_uv)
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
	pr_debug("%s[cluster%d set %s]:old_uv:%d,cur:%d----->new_uv:%d,to:%d,vol_cnt=%d\n",
		 __func__, cluster_id, regulator->supply_name,
		 old_uv, cur, new_uv, to, vol_cnt);

	if (to >= vol_cnt)
		to = vol_cnt - 1;

	if (cur < 0 || cur >= vol_cnt || reg_use_buck[cluster_id]) {
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

static u32 meson_get_suggested_dsu_volt(u32 *dsu_opp_table,
	struct cpufreq_freqs *freq, unsigned int volt_new)
{
	if (freq->new > CPU_CMP_RATE)
		return dsu_opp_table[3];
	else if (freq->new > low_dsu_rate)
		return dsu_opp_table[1];
	else
		return volt_new;
}

static u32 meson_get_suggested_dsu_freq(u32 *dsu_opp_table, struct cpufreq_freqs *freq)
{
	if (freq->new > CPU_CMP_RATE)
		return dsu_opp_table[2];
	else if (freq->new > low_dsu_rate)
		return dsu_opp_table[0];
	else
		return freq->new;
}

static void meson_dsuvolt_adjust(struct meson_cpufreq_driver_data *cpufreq_data)
{
	struct regulator *dsu_reg = cpufreq_data->reg_dsu;
	u32 old_dsu_vol;

	if (cpufreq_voltage_set_skip || !dsu_reg)
		return;
	old_dsu_vol = cpufreq_data->dsuvolt_last_set;
	meson_regulator_set_volate(dsu_reg,
		cpufreq_data->clusterid, old_dsu_vol,
		dsu_voltage_vote_result[cpufreq_data->clusterid], 0);
	cpufreq_data->dsuvolt_last_set = dsu_voltage_vote_result[cpufreq_data->clusterid];
}

static void meson_dsu_volt_freq_vote(struct meson_cpufreq_driver_data *cpufreq_data,
	struct cpufreq_freqs *freq, unsigned int volt_new)
{
	if (cpufreq_data->reg_external_used)
		dsu_voltage_vote_result[cpufreq_data->clusterid] = volt_new;
	else if (cpufreq_data->reg_dsu && cpufreq_data->dsu_opp_table)
		dsu_voltage_vote_result[cpufreq_data->clusterid] =
		meson_get_suggested_dsu_volt(cpufreq_data->dsu_opp_table, freq, volt_new);
	if (cpufreq_data->dsu_clock_shared && cpufreq_data->dsu_opp_table)
		dsu_freq_vote_result[cpufreq_data->clusterid] =
		meson_get_suggested_dsu_freq(cpufreq_data->dsu_opp_table, freq);
}

/* Set clock frequency */
static int meson_cpufreq_set_target(struct cpufreq_policy *policy,
				    unsigned int index)
{
	struct dev_pm_opp *opp;
	u32 cpu, cur_cluster;
	unsigned long freq_new, freq_old, dsu_freq;
	unsigned int volt_new = 0, volt_old = 0, volt_tol = 0;
	struct meson_cpufreq_driver_data *cpufreq_data;
	struct device *cpu_dev;
	struct regulator *cpu_reg;
	int ret = 0;

	if (!policy) {
		pr_err("invalid policy, returning\n");
		return -ENODEV;
	}

	mutex_lock(&cpufreq_target_lock);
	cpu = policy->cpu;
	cpufreq_data = policy->driver_data;
	cpu_dev = cpufreq_data->cpu_dev;
	cpu_reg = cpufreq_data->reg;
	cur_cluster = cpufreq_data->clusterid;

	pr_debug("setting target for cpu %d, index =%d\n", policy->cpu, index);

	freq_new = freq_table[cur_cluster][index].frequency * 1000;

	if (!IS_ERR(cpu_reg)) {
		opp = dev_pm_opp_find_freq_ceil(cpu_dev, &freq_new);
		if (IS_ERR(opp)) {
			mutex_unlock(&cpufreq_target_lock);
			pr_err("failed to find OPP for %lu Khz\n",
			       freq_new / 1000);
			return PTR_ERR(opp);
		}
		volt_new = dev_pm_opp_get_voltage(opp);
		dev_pm_opp_put(opp);
		if (cpufreq_data->reg_external_used)
			volt_old = cpufreq_data->cpuvolt_last_set;
		else
			volt_old = meson_regulator_get_volate(cpu_reg);
		volt_tol = volt_new * cpufreq_data->volt_tol / 100;
		pr_debug("Found OPP: %lu kHz, %u, tolerance: %u\n",
			 freq_new / 1000, volt_new, volt_tol);
	}
	freq_old = clk_get_rate(clk[cur_cluster]);

	freqs.old = freq_old / 1000;
	freqs.new = freq_new / 1000;

	meson_dsu_volt_freq_vote(cpufreq_data, &freqs, volt_new);

	pr_debug("[cluster%d:%d]scaling from %lu MHz, %u mV --> %lu MHz, %u mV[%u %u][%u %u]\n",
		 cur_cluster, reg_use_buck[cur_cluster],
		 freq_old / 1000000, (volt_old > 0) ? volt_old / 1000 : -1,
		 freq_new / 1000000, volt_new ? volt_new / 1000 : -1,
		 dsu_voltage_vote_result[LITTLE_VOTER], dsu_voltage_vote_result[BIG_VOTER],
		 dsu_freq_vote_result[LITTLE_VOTER], dsu_freq_vote_result[BIG_VOTER]);

	/*cpufreq up,change voltage before frequency*/
	if (freq_new > freq_old) {
		ret = meson_regulator_set_volate(cpu_reg, cur_cluster, volt_old,
						 volt_new, volt_tol);
		if (ret) {
			mutex_unlock(&cpufreq_target_lock);
			pr_err("failed to scale voltage %u %u up: %d\n",
			       volt_new, volt_tol, ret);
			return ret;
		}
		meson_dsuvolt_adjust(cpufreq_data);
	}

	meson_dsufreq_adjust(cpufreq_data, &freqs, CPUFREQ_PRECHANGE);
	/*scale clock frequency*/
	ret = meson_cpufreq_set_rate(policy, cur_cluster,
				     freq_new / 1000);
	if (ret) {
		pr_err("failed to set clock %luMhz rate: %d\n",
		       freq_new / 1000000, ret);
		if (volt_old > 0 && freq_new > freq_old) {
			pr_debug("scaling to old voltage %u\n", volt_old);
			freqs.old = freq_new / 1000;
			freqs.new = freq_old / 1000;
			meson_dsu_volt_freq_vote(cpufreq_data, &freqs, volt_old);
			if (cpufreq_data->dsu_clock_shared) {
				meson_dsufreq_adjust(cpufreq_data, &freqs, CPUFREQ_PRECHANGE);
				meson_dsufreq_adjust(cpufreq_data, &freqs, CPUFREQ_POSTCHANGE);
			}
			meson_regulator_set_volate(cpu_reg, cur_cluster, volt_old, volt_old,
						   volt_tol);
			meson_dsuvolt_adjust(cpufreq_data);
		}
		mutex_unlock(&cpufreq_target_lock);
		return ret;
	}

	meson_dsufreq_adjust(cpufreq_data, &freqs, CPUFREQ_POSTCHANGE);
	/*cpufreq down,change voltage after frequency*/
	if (freq_new < freq_old) {
		ret = meson_regulator_set_volate(cpu_reg, cur_cluster, volt_old,
						 volt_new, volt_tol);
		if (ret) {
			pr_err("failed to scale volt %u %u down: %d\n",
			       volt_new, volt_tol, ret);
			return ret;
		}
		meson_dsuvolt_adjust(cpufreq_data);
	}
	cpufreq_data->cpuvolt_last_set = volt_new;
	freq_new = clk_get_rate(clk[cur_cluster]) / 1000000;
	dsu_freq = clk_get_rate(cpufreq_data->clk_dsu) / 1000000;

	pr_debug("After transition, new lk rate %luMhz, volt %dmV[dsu %luMhz]\n",
		 freq_new, meson_regulator_get_volate(cpu_reg) / 1000, dsu_freq);
	mutex_unlock(&cpufreq_target_lock);
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

	cpufreq_tables_supply[cur_cluster] = of_property_read_bool(np,
						      "multi_tables_available");
	if (cpufreq_tables_supply[cur_cluster]) {
		/*choose appropriate cpufreq tables according efuse info*/
		ret = get_cpufreq_table_index(GET_DVFS_TABLE_INDEX,
					      cur_cluster, 0, 0);
		pr_info("%s:clusterid: %u tables_index %u\n", __func__, cur_cluster, ret);
	}
	opp_table_index[cur_cluster] = ret;
	return ret;
}

static void get_cluster_cores(unsigned int cpuid, struct cpumask *dstp, u32 *cur_cluster)
{
	int cpu, i;

	cpumask_clear(dstp);
	for (i = 0; i < MAX_CLUSTERS; i++) {
		if (cpumask_test_cpu(cpuid, &cluster_cpus[i])) {
			*cur_cluster = i;
			cpumask_copy(dstp, &cluster_cpus[i]);
			return;
		}
	}

	for_each_possible_cpu(cpu) {
		//coverity [overrun-local]
		if (topology_physical_package_id(cpuid) ==
			topology_physical_package_id(cpu))
			cpumask_set_cpu(cpu, dstp);
	}
}

static int
aml_cpufreq_frequency_table_cpuinfo(struct cpufreq_policy *policy,
				    struct cpufreq_frequency_table *table)
{
	struct cpufreq_frequency_table *pos;
	unsigned int min_freq = ~0;
	unsigned int max_freq = 0;
	unsigned int freq;

	cpufreq_for_each_valid_entry(pos, table) {
		freq = pos->frequency;

		if (!cpufreq_boost_enabled() &&
		    (pos->flags & CPUFREQ_BOOST_FREQ))
			continue;

		pr_debug("table entry %u: %u kHz\n", (int)(pos - table), freq);
		if (freq < min_freq)
			min_freq = freq;
		if (freq > max_freq)
			max_freq = freq;
	}

	policy->min = min_freq;
	policy->cpuinfo.min_freq = min_freq;
	policy->max = max_freq;
	policy->cpuinfo.max_freq = max_freq;

	if (policy->min == ~0)
		return -EINVAL;
	else
		return 0;
}

static int set_freq_table_sorted(struct cpufreq_policy *policy)
{
	struct cpufreq_frequency_table *pos, *table = policy->freq_table;
	struct cpufreq_frequency_table *prev = NULL;
	int ascending = 0;

	policy->freq_table_sorted = CPUFREQ_TABLE_UNSORTED;

	cpufreq_for_each_valid_entry(pos, table) {
		if (!prev) {
			prev = pos;
			continue;
		}

		if (pos->frequency == prev->frequency) {
			pr_warn("Duplicate freq-table entries: %u\n",
				pos->frequency);
			return -EINVAL;
		}

		/* Frequency increased from prev to pos */
		if (pos->frequency > prev->frequency) {
			/* But frequency was decreasing earlier */
			if (ascending < 0) {
				pr_debug("Freq table is unsorted\n");
				return 0;
			}

			ascending++;
		} else {
			/* Frequency decreased from prev to pos */

			/* But frequency was increasing earlier */
			if (ascending > 0) {
				pr_debug("Freq table is unsorted\n");
				return 0;
			}

			ascending--;
		}

		prev = pos;
	}

	if (ascending > 0)
		policy->freq_table_sorted = CPUFREQ_TABLE_SORTED_ASCENDING;
	else
		policy->freq_table_sorted = CPUFREQ_TABLE_SORTED_DESCENDING;

	pr_debug("Freq table is sorted in %s order\n",
		 ascending > 0 ? "ascending" : "descending");

	return 0;
}

static int aml_cpufreq_table_validate_and_sort(struct cpufreq_policy *policy)
{
	int ret;

	if (!policy->freq_table)
		return 0;

	ret = aml_cpufreq_frequency_table_cpuinfo(policy, policy->freq_table);
	if (ret)
		return ret;

	return set_freq_table_sorted(policy);
}

static int opptable_show(struct seq_file *m, void *v)
{
	struct cpufreq_policy *policy = m->private;
	struct meson_cpufreq_driver_data *driver_data = policy->driver_data;
	struct device *cpu_dev = driver_data->cpu_dev;
	struct opp_table *opp_table = dev_pm_opp_get_opp_table(cpu_dev);
	struct dev_pm_opp *opp;
	int clusterid = driver_data->clusterid;

	seq_printf(m, "pdvfs_en: %d\n", cpufreq_tables_supply[clusterid]);
	seq_printf(m, "table_index: %d\n", opp_table_index[clusterid]);
	if (opp_table) {
		seq_puts(m, "opp table:\n");
		mutex_lock(&opp_table->lock);
		list_for_each_entry(opp, &opp_table->opp_list, node) {
			if (!opp->available)
				continue;
			seq_printf(m, "%lu %lu\n", opp->rate, opp->supplies[0].u_volt);
		}
		dev_pm_opp_put_opp_table(opp_table);
		mutex_unlock(&opp_table->lock);
	}
	return 0;
}

static int maxfreq_show(struct seq_file *m, void *v)
{
	return 0;
}

static int get_index_from_freq(struct cpufreq_frequency_table *table, int freq)
{
	struct cpufreq_frequency_table *pos;
	int i = 0;

	cpufreq_for_each_valid_entry(pos, table) {
		if (freq > pos->frequency)
			i++;
		else
			break;
	}
	return i;
}

static ssize_t  meson_maxfreq_write(struct file *file, const char __user *userbuf,
	size_t count, loff_t *ppos)
{
	struct cpufreq_policy *policy = PDE_DATA(file_inode(file));
	struct meson_cpufreq_driver_data *driver_data = policy->driver_data;
	char buf[10] = {0};
	int maxfreq, index;

	count = min_t(size_t, count, sizeof(buf) - 1);

	if (copy_from_user(buf, userbuf, count))
		return -EFAULT;

	//ret = sscanf(buf, "%d", &maxfreq);
	if (kstrtoint(buf, 0, &maxfreq))
		return -EINVAL;

	index = get_index_from_freq(freq_table[driver_data->clusterid], maxfreq);
	meson_cpufreq_set_target(policy, index);

	return count;
}

static int meson_opptable_open(struct inode *inode, struct file *file)
{
	return single_open(file, opptable_show, PDE_DATA(inode));
}

static int meson_maxfreq_open(struct inode *inode, struct file *file)
{
	return single_open(file, maxfreq_show, PDE_DATA(inode));
}

static const struct file_operations meson_opptable_fops = {
	.open = meson_opptable_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static const struct file_operations meson_maxfreq_fops = {
	.open = meson_maxfreq_open,
	.read = seq_read,
	.write = meson_maxfreq_write,
	.llseek = seq_lseek,
	.release = single_release,
};

static void create_meson_cpufreq_proc_files(struct cpufreq_policy *policy)
{
	char policy_name[10] = {0};
	struct meson_cpufreq_driver_data *data = policy->driver_data;

	snprintf(policy_name, sizeof(policy_name), "policy%u",
		cpumask_first(policy->related_cpus));
	data->parent_proc = proc_mkdir(policy_name, cpufreq_proc);
	if (data->parent_proc) {
		proc_create_data("opp-table", 0444, data->parent_proc,
			&meson_opptable_fops, policy);
		proc_create_data("scaling_max_freq", 0444, data->parent_proc,
			&meson_maxfreq_fops, policy);
	}
}

static void destroy_meson_cpufreq_proc_files(struct cpufreq_policy *policy)
{
	char policy_name[10] = {0};
	struct meson_cpufreq_driver_data *data = policy->driver_data;

	snprintf(policy_name, sizeof(policy_name), "policy%u",
		cpumask_first(policy->related_cpus));
	remove_proc_entry("opp-table", data->parent_proc);
	remove_proc_entry("scaling_max_freq", data->parent_proc);
	remove_proc_entry(policy_name, cpufreq_proc);
}

static void show_dsu_opp_table(u32 *table, int clusterid)
{
	pr_debug("[cluster%d dsu_opp_table][%u %u %u %u]\n",
		clusterid, table[0], table[1], table[2], table[3]);
}

static void meson_set_policy_volt_range(struct meson_cpufreq_driver_data *data)
{
	struct device *cpu_dev = data->cpu_dev;
	struct opp_table *opp_table = dev_pm_opp_get_opp_table(cpu_dev);
	struct dev_pm_opp *opp;
	unsigned long max = 0, min = -1;
	int oldvolt_cpu, oldvolt_dsu;

	if (opp_table) {
		mutex_lock(&opp_table->lock);
		list_for_each_entry(opp, &opp_table->opp_list, node) {
			if (!opp->available)
				continue;
			if (opp->supplies[0].u_volt > max)
				max = opp->supplies[0].u_volt;
			if (opp->supplies[0].u_volt < min)
				min = opp->supplies[0].u_volt;
		}
		dev_pm_opp_put_opp_table(opp_table);
		mutex_unlock(&opp_table->lock);
	}

	/*set cpu regulator voltage to a valid value in init callback*/
	oldvolt_cpu = meson_regulator_get_volate(data->reg);
	if (oldvolt_cpu > max)
		oldvolt_cpu = max;
	else if (oldvolt_cpu < min)
		oldvolt_cpu = min;
	regulator_set_voltage_tol(data->reg, oldvolt_cpu, oldvolt_cpu);
	data->cpuvolt_last_set = oldvolt_cpu;

	if (data->reg_dsu) {
		oldvolt_dsu = meson_regulator_get_volate(data->reg_dsu);
		if (oldvolt_dsu > data->dsu_opp_table[3])
			oldvolt_dsu = data->dsu_opp_table[3];
		else if (oldvolt_dsu < min)
			oldvolt_dsu = min;
		/*set regulator voltage to a valid value in init callback*/
		regulator_set_voltage_tol(data->reg_dsu, oldvolt_dsu, oldvolt_dsu);
		data->dsuvolt_last_set = oldvolt_dsu;
	}
}

/* CPU initialization */
static int meson_cpufreq_init(struct cpufreq_policy *policy)
{
	u32 cur_cluster;
	struct device *cpu_dev;
	struct device_node *np;
	struct regulator *cpu_reg = NULL, *dsu_reg = NULL;
	struct meson_cpufreq_driver_data *cpufreq_data;
	struct clk *low_freq_clk_p, *high_freq_clk_p = NULL;
	struct clk *dsu_clk, *dsu_pre_parent, *dsu_pre_parent2;
	unsigned int transition_latency = CPUFREQ_ETERNAL;
	unsigned int volt_tol = 0;
	unsigned long freq_hz = 0;
	int cpu = 0, ret = 0, tables_index;
	int nr_opp, tmp_rate;
	u32 *dsu_opp_table;
	bool cpu_supply_external_used;
	bool dsu_clock_shared;

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

	get_cluster_cores(cpu, policy->cpus, &cur_cluster);

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
	dsu_pre_parent2 = of_clk_get_by_name(np, DSU_PRE_PARENT2);
	if (IS_ERR(dsu_pre_parent2)) {
		dsu_pre_parent2 = NULL;
		pr_debug("%s: ignor dsu pre parent2 clk!\n", __func__);
	}

	cpufreq_voltage_set_skip = of_property_read_bool(np,
							 "cpufreq_voltage_set_skip");
	reg_use_buck[cur_cluster] = of_property_read_bool(np,
		"cpu_reg_use_buck");
	dsu_opp_table = kmalloc(sizeof(*dsu_opp_table) * 4, GFP_KERNEL);
	if (IS_ERR(dsu_opp_table)) {
		pr_err("failed to alloc dsu_opp_table.\n");
		dsu_opp_table = NULL;
	} else {
		if (of_property_read_u32_array(np, "dsu-opp-table", &dsu_opp_table[0], 4)) {
			kfree(dsu_opp_table);
			dsu_opp_table = NULL;
		} else {
			show_dsu_opp_table(dsu_opp_table, cur_cluster);
		}
	}
	cpu_reg = devm_regulator_get(cpu_dev, CORE_SUPPLY);
	if (IS_ERR(cpu_reg)) {
		pr_err("%s:failed to get regulator, %ld\n", __func__,
		       PTR_ERR(cpu_reg));
		cpu_reg = NULL;
		goto free_dsutable;
	}

	dsu_reg = regulator_get_optional(cpu_dev, DSU_SUPPLY);
	if (IS_ERR(dsu_reg))
		dsu_reg = NULL;

	cpu_supply_external_used = of_property_read_bool(np, "cpu_supply_external_used");
	dsu_clock_shared = of_property_read_bool(np, "dsu_clock_shared");
	pr_debug("[%s %d][%d %d]\n", __func__, __LINE__, cpu_supply_external_used, dsu_clock_shared);

	if (of_property_read_u32(np, "voltage-tolerance", &volt_tol))
		volt_tol = DEF_VOLT_TOL;
	pr_debug("value of voltage_tolerance %u\n", volt_tol);

	if (!of_property_read_u32(np, "mid-rate",
				  &tmp_rate))
		mid_rate = tmp_rate;
	if (of_property_read_u32(np, "dsu-low-rate",
				 &low_dsu_rate))
		low_dsu_rate = DSU_LOW_RATE;
	pr_debug("value of low_dsu_rate %u\n", low_dsu_rate);

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
	ret = aml_cpufreq_table_validate_and_sort(policy);
	if (ret) {
		dev_err(cpu_dev, "CPU %d, cluster: %d invalid freq table\n",
			policy->cpu, cur_cluster);
		goto free_opp_table;
	}

	if (of_property_read_u32(np, "clock-latency", &transition_latency))
		policy->cpuinfo.transition_latency = CPUFREQ_ETERNAL;
	if (of_property_read_u32(np, "suspend-freq", &policy->suspend_freq))
		policy->suspend_freq = get_table_max(freq_table[cur_cluster]);
	else
		policy->suspend_freq = cpufreq_driver_resolve_freq(policy, policy->suspend_freq);

	cpufreq_data->clusterid = cur_cluster;
	cpufreq_data->cpu_dev = cpu_dev;
	cpufreq_data->low_freq_clk_p = low_freq_clk_p;
	cpufreq_data->high_freq_clk_p = high_freq_clk_p;
	cpufreq_data->clk_dsu = dsu_clk;
	cpufreq_data->clk_dsu_pre = dsu_pre_parent;
	cpufreq_data->clk_dsu_pre2 = dsu_pre_parent2;
	cpufreq_data->reg = cpu_reg;
	cpufreq_data->reg_dsu = dsu_reg;
	cpufreq_data->dsu_opp_table = dsu_opp_table;
	cpufreq_data->reg_external_used = cpu_supply_external_used;
	cpufreq_data->dsu_clock_shared = dsu_clock_shared;
	cpufreq_data->clusterid = cur_cluster;
	cpufreq_data->volt_tol = volt_tol;
	cpufreq_data->policy = policy;
	policy->driver_data = cpufreq_data;
	policy->clk = clk[cur_cluster];
	policy->cpuinfo.transition_latency = transition_latency;
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
	dev_pm_opp_of_register_em(policy->cpus);
	meson_set_policy_volt_range(cpufreq_data);

	dev_info(cpu_dev, "%s: CPU %d initialized\n", __func__, policy->cpu);
	return ret;
free_opp_table:
	if (policy->freq_table) {
		dev_pm_opp_free_cpufreq_table(cpu_dev,
					      &freq_table[cur_cluster]);
		dev_pm_opp_of_cpumask_remove_table(policy->related_cpus);
	}
free_reg:
	if (!IS_ERR(cpu_reg))
		devm_regulator_put(cpu_reg);
	if (!IS_ERR(dsu_reg))
		regulator_put(dsu_reg);
free_dsutable:
	kfree(dsu_opp_table);
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
	int cur_cluster;

	cpufreq_data = policy->driver_data;
	if (!cpufreq_data)
		return 0;
	cur_cluster = cpufreq_data->clusterid;
	dsu_voltage_vote_result[cpufreq_data->clusterid] = 0;
	dsu_freq_vote_result[cpufreq_data->clusterid] = 0;
	destroy_meson_cpufreq_proc_files(policy);

	cpu_dev = get_cpu_device(policy->cpu);
	if (!cpu_dev) {
		pr_err("%s: failed to get cpu%d device\n", __func__,
		       policy->cpu);
		return -ENODEV;
	}

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
	if (policy && policy->cdev)
		dev_set_uevent_suppress(&policy->cdev->device, true);

	return cpufreq_generic_suspend(policy);
}

static int meson_cpufreq_resume(struct cpufreq_policy *policy)
{
	if (policy && policy->cdev)
		dev_set_uevent_suppress(&policy->cdev->device, false);

	return cpufreq_generic_suspend(policy);
}

static void meson_cpufreq_ready(struct cpufreq_policy *policy)
{
	struct meson_cpufreq_driver_data *cpufreq_data = policy->driver_data;
#ifdef CONFIG_AMLOGIC_MODIFY
	unsigned int maxfreq = cpufreq_data->clusterid ? freqmax1 : freqmax0;
#endif

	if (!cooldev[cpufreq_data->clusterid])
		cooldev[cpufreq_data->clusterid] = of_cpufreq_cooling_register(policy);
	create_meson_cpufreq_proc_files(policy);

#ifdef CONFIG_AMLOGIC_MODIFY
	if (maxfreq && maxfreq >= policy->min && maxfreq < policy->max) {
		pr_info("policy%d: set policy->max to %u from bootargs\n", policy->cpu, maxfreq);
		if (freq_qos_update_request(policy->max_freq_req, maxfreq) < 0)
			pr_err("freq_qos_update_request failed\n");
	}
#endif
}

static struct cpufreq_driver meson_cpufreq_driver = {
	.name			= "arm-big-little",
	.flags			= CPUFREQ_STICKY |
					CPUFREQ_HAVE_GOVERNOR_PER_POLICY |
					CPUFREQ_NEED_INITIAL_FREQ_CHECK,
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

static void meson_get_cluster_cores(void)
{
	struct device *cpu_dev;
	struct device_node *np;
	int cpu = 0, cluster_id = 0, i;
	u32 core_num, *cores = NULL;

	for (i = 0; i < MAX_CLUSTERS; i++)
		cpumask_clear(&cluster_cpus[i]);
	for_each_possible_cpu(cpu) {
		if (!cpumask_test_cpu(cpu, &cluster_cpus[cluster_id])) {
			cpu_dev = get_cpu_device(cpu);
			if (!cpu_dev)
				return;
			np = of_node_get(cpu_dev->of_node);
			if (!np)
				return;
			if (of_property_read_u32(np, "dvfs_sibling_core_num", &core_num)) {
				continue;
			} else {
				cores = kmalloc_array(core_num, sizeof(*cores), GFP_KERNEL);
				if (!cores)
					return;
				if (!of_property_read_u32_array(np, "dvfs_sibling_cores",
							&cores[0], core_num)) {
					for (i = 0; i < core_num; i++) {
						pr_debug("[%s %d]cpu%d->cluster%d\n",
							__func__, __LINE__, cores[i], cluster_id);
						cpumask_set_cpu(cores[i],
								&cluster_cpus[cluster_id]);
					}
				}
			}
		}
		if (cores && cpu >= cores[core_num - 1]) {
			cluster_id++;
			kfree(cores);
			cores = NULL;
		}
	}
}

static int meson_cpufreq_probe(struct platform_device *pdev)
{
	struct device *cpu_dev;
	struct device_node *np;
	struct regulator *cpu_reg;
	struct clk *cpu_clk;
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

	meson_get_cluster_cores();
	cpu_clk = of_clk_get_by_name(np, CORE_CLK);
	ret = PTR_ERR_OR_ZERO(cpu_clk);
	if (ret) {
		if (ret == -EPROBE_DEFER)
			pr_debug("cpu clock not ready, retry!\n");
		else
			pr_debug("failed to get clock:%d!\n", ret);
		return ret;
	}

	cpu_reg = devm_regulator_get(cpu_dev, CORE_SUPPLY);
	ret = PTR_ERR_OR_ZERO(cpu_reg);
	if (ret) {
		if (ret == -EPROBE_DEFER)
			pr_debug("cpu regulator not ready, retry!\n");
		else
			pr_debug("failed to get cpu regulator:%d!\n", ret);
		return ret;
	}
	cpufreq_proc = proc_mkdir("meson_cpufreq", NULL);
	if (!cpufreq_proc)
		pr_err("%s: failed to create meson_cpufreq proc entry\n", __func__);

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
