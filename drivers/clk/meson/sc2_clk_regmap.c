// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2018 AMLOGIC.
 */

#include <linux/module.h>
#include <linux/arm-smccc.h>
#include "clk-regmap.h"
#include "clk-secure.h"
#include "clk-cpu-dyndiv.h"

static unsigned long clk_regmap_secure_div_recalc_rate(struct clk_hw *hw,
						       unsigned long prate)
{
	struct clk_regmap *clk = to_clk_regmap(hw);
	struct clk_regmap_div_data *div = clk_get_regmap_div_data(clk);
	struct arm_smccc_res res;
	unsigned int val;

	if (!strcmp(clk_hw_get_name(hw), "cpu_clk_dyn1_div"))
		arm_smccc_smc(CPUCLK_SECURE_RW, CLK_CPU_REG_RW,
			      0, 0, 0, 0, 0, 0, &res);
	else
		arm_smccc_smc(CPUCLK_SECURE_RW, CLK_DSU_REG_RW,
			      0, 0, 0, 0, 0, 0, &res);
	val = res.a0;

	val >>= div->shift;
	val &= clk_div_mask(div->width);
	return divider_recalc_rate(hw, prate, val, div->table, div->flags,
				   div->width);
}

static long clk_regmap_secure_div_round_rate(struct clk_hw *hw, unsigned long rate,
					     unsigned long *prate)
{
	struct clk_regmap *clk = to_clk_regmap(hw);
	struct clk_regmap_div_data *div = clk_get_regmap_div_data(clk);
	struct arm_smccc_res res;
	unsigned int val;

	/* if read only, just return current value */
	if (div->flags & CLK_DIVIDER_READ_ONLY) {
		if (!strcmp(clk_hw_get_name(hw), "cpu_clk_dyn1_div"))
			arm_smccc_smc(CPUCLK_SECURE_RW, CLK_CPU_REG_RW,
				      0, 0, 0, 0, 0, 0, &res);
		else
			arm_smccc_smc(CPUCLK_SECURE_RW, CLK_DSU_REG_RW,
				      0, 0, 0, 0, 0, 0, &res);
		val = res.a0;

		val >>= div->shift;
		val &= clk_div_mask(div->width);

		return divider_ro_round_rate(hw, rate, prate, div->table,
					     div->width, div->flags, val);
	}

	return divider_round_rate(hw, rate, prate, div->table, div->width,
				  div->flags);
}

static int clk_regmap_secure_div_set_rate(struct clk_hw *hw, unsigned long rate,
					  unsigned long parent_rate)
{
	struct clk_regmap *clk = to_clk_regmap(hw);
	struct clk_regmap_div_data *div = clk_get_regmap_div_data(clk);
	struct arm_smccc_res res;
	int ret;

	ret = divider_get_val(rate, parent_rate, div->table, div->width,
			      div->flags);
	if (ret < 0)
		return ret;

	if (!strcmp(clk_hw_get_name(hw), "cpu_clk_dyn1_div"))
		arm_smccc_smc(CPUCLK_SECURE_RW, CPU_DIVIDER_SET_RATE, ret,
			      div->width, div->shift, 0, 0, 0, &res);
	else
		arm_smccc_smc(CPUCLK_SECURE_RW, DSU_DIVIDER_SET_RATE, ret,
			      div->width, div->shift, 0, 0, 0, &res);
	return 0;
};

/* Would prefer clk_regmap_div_ro_ops but clashes with qcom */

const struct clk_ops clk_regmap_secure_divider_ops = {
	.recalc_rate = clk_regmap_secure_div_recalc_rate,
	.round_rate = clk_regmap_secure_div_round_rate,
	.set_rate = clk_regmap_secure_div_set_rate,
};
EXPORT_SYMBOL_GPL(clk_regmap_secure_divider_ops);

const struct clk_ops clk_regmap_secure_divider_ro_ops = {
	.recalc_rate = clk_regmap_secure_div_recalc_rate,
	.round_rate = clk_regmap_secure_div_round_rate,
};
EXPORT_SYMBOL_GPL(clk_regmap_secure_divider_ro_ops);

static u8 clk_regmap_secure_mux_get_parent(struct clk_hw *hw)
{
	struct clk_regmap *clk = to_clk_regmap(hw);
	struct clk_regmap_mux_data *mux = clk_get_regmap_mux_data(clk);
	struct arm_smccc_res res;
	unsigned int val;

	if (!strcmp(clk_hw_get_name(hw), "cpu_clk_dyn0_sel") ||
	    !strcmp(clk_hw_get_name(hw), "cpu_clk_dyn1_sel") ||
	    !strcmp(clk_hw_get_name(hw), "cpu_clk_dyn0") ||
	    !strcmp(clk_hw_get_name(hw), "cpu_clk_dyn1") ||
	    !strcmp(clk_hw_get_name(hw), "cpu_clk_dyn") ||
	    !strcmp(clk_hw_get_name(hw), "cpu_clk"))
		arm_smccc_smc(CPUCLK_SECURE_RW, CLK_CPU_REG_RW,
			      0, 0, 0, 0, 0, 0, &res);
	else if (!strcmp(clk_hw_get_name(hw), "cpu1_clk") ||
		 !strcmp(clk_hw_get_name(hw), "cpu2_clk") ||
		 !strcmp(clk_hw_get_name(hw), "cpu3_clk") ||
		 !strcmp(clk_hw_get_name(hw), "dsu_clk"))
		arm_smccc_smc(CPUCLK_SECURE_RW, CLK_CPU_DSU_REG_RW,
			      0, 0, 0, 0, 0, 0, &res);
	else
		arm_smccc_smc(CPUCLK_SECURE_RW, CLK_DSU_REG_RW,
			      0, 0, 0, 0, 0, 0, &res);

	val = res.a0;
	val >>= mux->shift;
	val &= mux->mask;
	return clk_mux_val_to_index(hw, mux->table, mux->flags, val);
}

static int clk_regmap__secure_mux_set_parent(struct clk_hw *hw, u8 index)
{
	struct clk_regmap *clk = to_clk_regmap(hw);
	struct clk_regmap_mux_data *mux = clk_get_regmap_mux_data(clk);
	unsigned int val = clk_mux_index_to_val(mux->table, mux->flags, index);
	struct arm_smccc_res res;

	if (!strcmp(clk_hw_get_name(hw), "cpu_clk_dyn0_sel") ||
	    !strcmp(clk_hw_get_name(hw), "cpu_clk_dyn1_sel") ||
	    !strcmp(clk_hw_get_name(hw), "cpu_clk_dyn0") ||
	    !strcmp(clk_hw_get_name(hw), "cpu_clk_dyn1") ||
	    !strcmp(clk_hw_get_name(hw), "cpu_clk_dyn") ||
	    !strcmp(clk_hw_get_name(hw), "cpu_clk"))
		arm_smccc_smc(CPUCLK_SECURE_RW, SET_CPU0_MUX_PARENT,
			      val, mux->mask, mux->shift, 0, 0, 0, &res);
	else if (!strcmp(clk_hw_get_name(hw), "cpu1_clk") ||
		 !strcmp(clk_hw_get_name(hw), "cpu2_clk") ||
		 !strcmp(clk_hw_get_name(hw), "cpu3_clk") ||
		 !strcmp(clk_hw_get_name(hw), "dsu_clk"))
		arm_smccc_smc(CPUCLK_SECURE_RW, SET_CPU123_DSU_MUX_PARENT,
			      val, mux->mask, mux->shift, 0, 0, 0, &res);
	else
		arm_smccc_smc(CPUCLK_SECURE_RW, SET_DSU_PRE_MUX_PARENT,
			      val, mux->mask, mux->shift, 0, 0, 0, &res);

	return 0;
}

static int clk_regmap_secure_mux_determine_rate(struct clk_hw *hw,
						struct clk_rate_request *req)
{
	struct clk_regmap *clk = to_clk_regmap(hw);
	struct clk_regmap_mux_data *mux = clk_get_regmap_mux_data(clk);

	return clk_mux_determine_rate_flags(hw, req, mux->flags);
}

const struct clk_ops clk_regmap_secure_mux_ops = {
	.get_parent = clk_regmap_secure_mux_get_parent,
	.set_parent = clk_regmap__secure_mux_set_parent,
	.determine_rate = clk_regmap_secure_mux_determine_rate,
};
EXPORT_SYMBOL_GPL(clk_regmap_secure_mux_ops);

const struct clk_ops clk_regmap_secure_mux_ro_ops = {
	.get_parent = clk_regmap_secure_mux_get_parent,
};
EXPORT_SYMBOL_GPL(clk_regmap_secure_mux_ro_ops);

static inline struct meson_clk_cpu_dyndiv_data *
meson_clk_cpu_dyndiv_data(struct clk_regmap *clk)
{
	return (struct meson_clk_cpu_dyndiv_data *)clk->data;
}

static unsigned long
meson_secure_clk_cpu_dyndiv_recalc_rate(struct clk_hw *hw,
					unsigned long prate)
{
	struct clk_regmap *clk = to_clk_regmap(hw);
	struct meson_clk_cpu_dyndiv_data *data = meson_clk_cpu_dyndiv_data(clk);
	struct arm_smccc_res res;
	unsigned int val;

	arm_smccc_smc(CPUCLK_SECURE_RW, CLK_CPU_REG_RW,
		      0, 0, 0, 0, 0, 0, &res);
	val = res.a0;

	val >>= data->div.shift;
	val &= clk_div_mask(data->div.width);
	return divider_recalc_rate(hw, prate, val,
				   NULL, 0, data->div.width);
}

static long meson_secure_clk_cpu_dyndiv_round_rate(struct clk_hw *hw,
						   unsigned long rate,
						   unsigned long *prate)
{
	struct clk_regmap *clk = to_clk_regmap(hw);
	struct meson_clk_cpu_dyndiv_data *data = meson_clk_cpu_dyndiv_data(clk);

	return divider_round_rate(hw, rate, prate, NULL, data->div.width, 0);
}

static int meson_secure_clk_cpu_dyndiv_set_rate(struct clk_hw *hw,
						unsigned long rate,
						unsigned long parent_rate)
{
	struct clk_regmap *clk = to_clk_regmap(hw);
	struct meson_clk_cpu_dyndiv_data *data = meson_clk_cpu_dyndiv_data(clk);
	struct arm_smccc_res res;
	int ret;

	ret = divider_get_val(rate, parent_rate, NULL, data->div.width, 0);
	if (ret < 0)
		return ret;

	arm_smccc_smc(CPUCLK_SECURE_RW, CPU_DIVIDER_SET_RATE, ret,
		      data->div.width, data->div.shift, 0, 0, 0, &res);
	return 0;
};

const struct clk_ops meson_secure_clk_cpu_dyndiv_ops = {
	.recalc_rate = meson_secure_clk_cpu_dyndiv_recalc_rate,
	.round_rate = meson_secure_clk_cpu_dyndiv_round_rate,
	.set_rate = meson_secure_clk_cpu_dyndiv_set_rate,
};
EXPORT_SYMBOL_GPL(meson_secure_clk_cpu_dyndiv_ops);

MODULE_DESCRIPTION("Amlogic regmap backed clock driver");
MODULE_LICENSE("GPL v2");
