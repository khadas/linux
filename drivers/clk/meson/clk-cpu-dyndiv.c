// SPDX-License-Identifier: (GPL-2.0 OR MIT)
/*
 * Copyright (c) 2019 BayLibre, SAS.
 * Author: Neil Armstrong <narmstrong@baylibre.com>
 */

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/arm-smccc.h>
#include "clk-regmap.h"
#include "clk-cpu-dyndiv.h"

#define CPU_DYN_SEL_MASK	BIT(10)
#define SYS_CLK_SEL_MASK	BIT(15)
#define SECURE_CPU_CLK		0x82000099

static inline struct meson_clk_cpu_dyndiv_data *
meson_clk_cpu_dyndiv_data(struct clk_regmap *clk)
{
	return (struct meson_clk_cpu_dyndiv_data *)clk->data;
}

static inline struct meson_sec_cpu_dyn_data *
meson_sec_cpu_dyn_data(struct clk_regmap *clk)
{
	return (struct meson_sec_cpu_dyn_data *)clk->data;
}

static unsigned long meson_clk_cpu_dyndiv_recalc_rate(struct clk_hw *hw,
						      unsigned long prate)
{
	struct clk_regmap *clk = to_clk_regmap(hw);
	struct meson_clk_cpu_dyndiv_data *data = meson_clk_cpu_dyndiv_data(clk);

	return divider_recalc_rate(hw, prate,
				   meson_parm_read(clk->map, &data->div),
				   NULL, 0, data->div.width);
}

static long meson_clk_cpu_dyndiv_round_rate(struct clk_hw *hw,
					    unsigned long rate,
					    unsigned long *prate)
{
	struct clk_regmap *clk = to_clk_regmap(hw);
	struct meson_clk_cpu_dyndiv_data *data = meson_clk_cpu_dyndiv_data(clk);

	return divider_round_rate(hw, rate, prate, NULL, data->div.width, 0);
}

static int meson_clk_cpu_dyndiv_set_rate(struct clk_hw *hw, unsigned long rate,
					  unsigned long parent_rate)
{
	struct clk_regmap *clk = to_clk_regmap(hw);
	struct meson_clk_cpu_dyndiv_data *data = meson_clk_cpu_dyndiv_data(clk);
	unsigned int val;
	int ret;

	ret = divider_get_val(rate, parent_rate, NULL, data->div.width, 0);
	if (ret < 0)
		return ret;

	val = (unsigned int)ret << data->div.shift;

	/* Write the SYS_CPU_DYN_ENABLE bit before changing the divider */
	meson_parm_write(clk->map, &data->dyn, 1);

	/* Update the divider while removing the SYS_CPU_DYN_ENABLE bit */
	return regmap_update_bits(clk->map, data->div.reg_off,
				  SETPMASK(data->div.width, data->div.shift) |
				  SETPMASK(data->dyn.width, data->dyn.shift),
				  val);
};

const struct clk_ops meson_clk_cpu_dyndiv_ops = {
	.recalc_rate = meson_clk_cpu_dyndiv_recalc_rate,
	.round_rate = meson_clk_cpu_dyndiv_round_rate,
	.set_rate = meson_clk_cpu_dyndiv_set_rate,
};
EXPORT_SYMBOL_GPL(meson_clk_cpu_dyndiv_ops);

static unsigned long meson_sec_cpu_dyn_recalc_rate(struct clk_hw *hw,
						      unsigned long prate)
{
	struct clk_regmap *clk = to_clk_regmap(hw);
	struct meson_sec_cpu_dyn_data *data = meson_sec_cpu_dyn_data(clk);
	unsigned int val, pindex, div;
	unsigned long rate, nprate = 0;
	struct arm_smccc_res res;

	/* get cpu register value */
	arm_smccc_smc(SECURE_CPU_CLK, data->secid_dyn_rd,
			0, 0, 0, 0, 0, 0, &res);
	val = res.a0;

	/* Confirm Now cpu is on final0 or final1 , bit10 = 0 or 1 */
	if (val & CPU_DYN_SEL_MASK) {
		pindex = (val >> 16) & 0x3;
		nprate = clk_hw_get_rate(clk_hw_get_parent_by_index(hw, pindex));
		if ((val >> 18) & 0x1) {
			div = (val >> 20) & 0x3f;
			rate = DIV_ROUND_UP_ULL((u64)nprate, div + 1);
		} else {
			rate = nprate;
		}
	} else {
		/* Get parent rate */
		pindex = val & 0x3;
		nprate = clk_hw_get_rate(clk_hw_get_parent_by_index(hw, pindex));
		if ((val >> 2) & 0x1) {
			div = (val >> 4) & 0x3f;
			rate = DIV_ROUND_UP_ULL((u64)nprate, div + 1);
		} else {
			rate = nprate;
		}
	}

	return rate;
}

/* find the best rate near to target rate */
static long meson_sec_cpu_dyn_round_rate(struct clk_hw *hw,
					    unsigned long rate,
					    unsigned long *prate)
{
	struct clk_regmap *clk = to_clk_regmap(hw);
	struct meson_sec_cpu_dyn_data *data = meson_sec_cpu_dyn_data(clk);
	struct cpu_dyn_table *table = (struct cpu_dyn_table *)data->table;
	unsigned long min, max;
	unsigned int i, cnt = data->table_cnt;

	min = table[0].rate;
	max = table[cnt - 1].rate;

	if (rate < min)
		return min;

	if (rate > max)
		return max;

	for (i = 0; i < data->table_cnt; i++) {
		if (rate <= table[i].rate)
			return table[i].rate;
	}

	return min;
}

static int meson_sec_cpu_dyn_set_rate(struct clk_hw *hw, unsigned long rate,
					  unsigned long parent_rate)
{
	struct clk_regmap *clk = to_clk_regmap(hw);
	struct meson_sec_cpu_dyn_data *data = meson_sec_cpu_dyn_data(clk);
	struct cpu_dyn_table *table = (struct cpu_dyn_table *)data->table;
	struct arm_smccc_res res;
	unsigned int nrate, i;

	for (i = 0; i < data->table_cnt; i++) {
		if (rate == table[i].rate) {
			table = &table[i];
			break;
		}
	}
	nrate = table->rate;

	/* For set more than 1G, need to set additional parent frequency */
	if (!strcmp(clk_hw_get_name(hw), "dsu_dyn_clk") && nrate > 1000000000) {
		if (clk_get_rate(hw->clk) > 1000000000) {
			/* switch dsu to fix div2 */
			arm_smccc_smc(SECURE_CPU_CLK, data->secid_dyn,
				1, 0, 0, 0, 0, 0, &res);
			/* udelay(50); */
		}
		clk_set_rate(clk_hw_get_parent_by_index(hw, 3)->clk, nrate);
	}
	arm_smccc_smc(SECURE_CPU_CLK, data->secid_dyn,
			table->dyn_pre_mux, table->dyn_post_mux, table->dyn_div,
			0, 0, 0, &res);
	return 0;
}

static u8 meson_sec_cpu_dyn_get_parent(struct clk_hw *hw)
{
	struct clk_regmap *clk = to_clk_regmap(hw);
	struct meson_sec_cpu_dyn_data *data = meson_sec_cpu_dyn_data(clk);
	u32 pre_shift, val;
	struct arm_smccc_res res;

	arm_smccc_smc(SECURE_CPU_CLK, data->secid_dyn_rd,
			0, 0, 0, 0, 0, 0, &res);
	val = res.a0;

	if (val & CPU_DYN_SEL_MASK)
		pre_shift = 16;
	else
		pre_shift = 0;

	val = val >> pre_shift;
	val &= 0x3;
	if (val >= clk_hw_get_num_parents(hw))
		return -EINVAL;

	return val;
}

/*recalc the rate in kernel, And set the cpu fixed clk as one level clk
 * due to everything is doing in bl31
 */
const struct clk_ops meson_sec_cpu_dyn_ops = {
	.recalc_rate = meson_sec_cpu_dyn_recalc_rate,
	.round_rate = meson_sec_cpu_dyn_round_rate,
	.set_rate = meson_sec_cpu_dyn_set_rate,
	.get_parent = meson_sec_cpu_dyn_get_parent
};
EXPORT_SYMBOL_GPL(meson_sec_cpu_dyn_ops);

static u8 meson_sec_sys_clk_get_parent(struct clk_hw *hw)
{
	struct clk_regmap *clk = to_clk_regmap(hw);
	struct meson_sec_cpu_dyn_data *data = meson_sec_cpu_dyn_data(clk);
	u32 pre_shift, val;
	struct arm_smccc_res res;

	arm_smccc_smc(SECURE_CPU_CLK, data->secid_dyn_rd,
			0, 0, 0, 0, 0, 0, &res);
	val = res.a0;

	if (val & SYS_CLK_SEL_MASK)
		pre_shift = 26;
	else
		pre_shift = 10;

	val = val >> pre_shift;
	val &= 0x7;
	if (val >= clk_hw_get_num_parents(hw))
		return -EINVAL;

	return val;
}

static unsigned long meson_sec_sys_clk_recalc_rate(struct clk_hw *hw,
						      unsigned long prate)
{
	struct clk_regmap *clk = to_clk_regmap(hw);
	struct meson_sec_cpu_dyn_data *data = meson_sec_cpu_dyn_data(clk);
	unsigned int val, pindex, div;
	unsigned long nprate = 0;
	struct arm_smccc_res res;

	/* get cpu register value */
	arm_smccc_smc(SECURE_CPU_CLK, data->secid_dyn_rd,
			0, 0, 0, 0, 0, 0, &res);
	val = res.a0;

	/* Confirm Now cpu is on final0 or final1 , bit10 = 0 or 1 */
	if (val & SYS_CLK_SEL_MASK) {
		pindex = (val >> 26) & 0x7;
		nprate = clk_hw_get_rate(clk_hw_get_parent_by_index(hw, pindex));
		div = (val >> 16) & 0x3ff;
	} else {
		/* Get parent rate */
		pindex = (val >> 10) & 0x7;
		nprate = clk_hw_get_rate(clk_hw_get_parent_by_index(hw, pindex));
		div = (val >> 0) & 0x3ff;
	}

	return DIV_ROUND_UP_ULL((u64)nprate, div + 1);
}

const struct clk_ops meson_sec_sys_clk_ops = {
	.recalc_rate = meson_sec_sys_clk_recalc_rate,
	.round_rate = meson_sec_cpu_dyn_round_rate,
	.set_rate = meson_sec_cpu_dyn_set_rate,
	.get_parent = meson_sec_sys_clk_get_parent
};
EXPORT_SYMBOL_GPL(meson_sec_sys_clk_ops);

MODULE_DESCRIPTION("Amlogic CPU Dynamic Clock divider");
MODULE_AUTHOR("Neil Armstrong <narmstrong@baylibre.com>");
MODULE_LICENSE("GPL v2");
