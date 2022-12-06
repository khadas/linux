/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __MESON_CPUFREQ_H
#define __MESON_CPUFREQ_H
/* Currently we support only two clusters */
#define MAX_CLUSTERS	2

/*core power supply*/
#define CORE_SUPPLY "cpu"
#define DSU_SUPPLY  "dsu"

/* Core Clocks */
#define CORE_CLK	"core_clk"
#define LOW_FREQ_CLK_PARENT	"low_freq_clk_parent"
#define HIGH_FREQ_CLK_PARENT	"high_freq_clk_parent"
#define DSU_CLK		"dsu_clk"
#define DSU_PRE_PARENT "dsu_pre_parent"
#define DSU_PRE_PARENT2 "dsu_pre_parent2"

static struct clk *clk[MAX_CLUSTERS];
static bool reg_use_buck[MAX_CLUSTERS];
static struct cpufreq_frequency_table *freq_table[MAX_CLUSTERS];
static struct cpumask cluster_cpus[MAX_CLUSTERS];
static struct thermal_cooling_device *cooldev[MAX_CLUSTERS];

/* Default voltage_tolerance */
#define DEF_VOLT_TOL		0
#define CLK_DIV		12
#define EFUSE_CPUFREQ_MIN 1500
/*mid rate for set parent,Khz*/
static unsigned int mid_rate = (1000 * 1000);
static unsigned int gap_rate = (10 * 1000 * 1000);
static struct cpufreq_freqs freqs;

/*
 * DSU_LOW_RATE:cpu clk less than DSU_LOW_RATE(1.2G)
 * dsu clk switch to cpu clk
 * DSU_HIGH_RATE:cpu clk between 1.2G to DSU_HIGH_RATE (1.8G)
 * dsu clk set to DSU_LOW_RATE(1.2G)
 * CPU_CMP_RATE: cpu clk greater than CPU_CMP_RATE(1.8G)
 * dsu clk set to DSU_HIGH_RATE(1.5G)
 */

#define DSU_LOW_RATE (1200 * 1000)
#define DSU_HIGH_RATE (1500 * 1000)
#define CPU_CMP_RATE (1800 * 1000)
unsigned int low_dsu_rate;

/*whether use different tables or not*/
bool cpufreq_tables_supply[MAX_CLUSTERS];
#define GET_DVFS_TABLE_INDEX           0x82000088

struct meson_cpufreq_driver_data {
	struct device *cpu_dev;
	struct regulator *reg;
	struct regulator *reg_dsu;
	bool reg_external_used;
	u32 *dsu_opp_table;
	bool dsu_clock_shared;
	struct cpufreq_policy *policy;
	/* voltage tolerance in percentage */
	unsigned int volt_tol;
	unsigned int cpuvolt_last_set, dsuvolt_last_set;
	struct clk *high_freq_clk_p;
	struct clk *low_freq_clk_p;
	struct clk *clk_dsu;
	struct clk *clk_dsu_pre;
	struct clk *clk_dsu_pre2;
	int clusterid;
	struct proc_dir_entry *parent_proc;
};

enum dsu_clock_power_voters {
	LITTLE_VOTER,
	BIG_VOTER,
	VOTER_NUM
};

static struct mutex cluster_lock[MAX_CLUSTERS];
static unsigned int meson_cpufreq_get_rate(unsigned int cpu);
static unsigned int meson_cpufreq_set_rate(struct cpufreq_policy *policy,
					   u32 cur_cluster, u32 rate);
static int meson_regulator_set_volate(struct regulator *regulator, u32 cluster_id,
	int old_uv, int new_uv, int tol_uv);
int get_cpufreq_tables_efuse(u32 cur_cluster);
int choose_cpufreq_tables_index(const struct device_node *np, u32 cur_cluster);
#endif /* __MESON_CPUFREQ_H */
