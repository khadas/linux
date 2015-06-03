/*
 * drivers/amlogic/clk/clk.c
 *
 * Copyright (C) 2015 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
*/

#include "clk.h"

static DEFINE_SPINLOCK(lock);
static struct clk **clk_table;
static void __iomem *reg_base;
static void __iomem *reg_base_ao;
#ifdef CONFIG_OF
static struct clk_onecell_data clk_data;
#endif

/* add a clock instance to the clock lookup table used for dt based lookup */
void amlogic_clk_add_lookup(struct clk *clk, unsigned int id)
{
	if (clk_table && id)
		clk_table[id] = clk;
}

/**
 * Register a clock branch.
 * Most clock branches have a form like
 *
 * src1 --|--\
 *        |M |--[div]-[gate]-
 * src2 --|--/
 *
 * sometimes without one of those components.
 */
static struct clk *amlogic_clk_register_branch(const char *name,
		const char **parent_names, u8 num_parents, void __iomem *base,
		int mux_offset, u8 mux_shift, u8 mux_width, u8 mux_flags,
		int div_offset, u8 div_shift, u8 div_width, u8 div_flags,
		struct clk_div_table *div_table, int gate_offset,
		u8 gate_shift, u8 gate_flags, unsigned long flags,
		spinlock_t *lock)
{
	struct clk *clk;
	struct clk_mux *mux = NULL;
	struct clk_gate *gate = NULL;
	struct clk_divider *div = NULL;
	const struct clk_ops *mux_ops = NULL, *div_ops = NULL,
			     *gate_ops = NULL;

	if (num_parents > 1) {
		mux = kzalloc(sizeof(*mux), GFP_KERNEL);
		if (!mux)
			return ERR_PTR(-ENOMEM);

		mux->reg = base + mux_offset;
		mux->shift = mux_shift;
		mux->mask = BIT(mux_width) - 1;
		mux->flags = mux_flags;
		mux->lock = lock;
		mux_ops = (mux_flags & CLK_MUX_READ_ONLY) ? &clk_mux_ro_ops
							: &clk_mux_rw_ops;
	}

	if (gate_offset >= 0) {
		gate = kzalloc(sizeof(*gate), GFP_KERNEL);
		if (!gate)
			return ERR_PTR(-ENOMEM);

		gate->flags = gate_flags;
		gate->reg = base + gate_offset;
		gate->bit_idx = gate_shift;
		gate->lock = lock;
		gate_ops = &clk_gate_ops;
	}

	if (div_width > 0) {
		div = kzalloc(sizeof(*div), GFP_KERNEL);
		if (!div)
			return ERR_PTR(-ENOMEM);

		div->flags = div_flags;
		div->reg = base + div_offset;
		div->shift = div_shift;
		div->width = div_width;
		div->lock = lock;
		div->table = div_table;
		div_ops = &clk_divider_ops;
	}

	clk = clk_register_composite(NULL, name, parent_names, num_parents,
				     mux ? &mux->hw : NULL, mux_ops,
				     div ? &div->hw : NULL, div_ops,
				     gate ? &gate->hw : NULL, gate_ops,
				     flags);

	return clk;
}
void __init amlogic_clk_register_branches(
				      struct amlogic_clk_branch *list,
				      unsigned int nr_clk)
{
	struct clk *clk = NULL;
	unsigned int idx;
	unsigned long flags;
	int ret;
	for (idx = 0; idx < nr_clk; idx++, list++) {
		flags = list->flags;

		/* catch simple muxes */
		switch (list->branch_type) {
		case branch_composite:
			clk = amlogic_clk_register_branch(list->name,
				list->parent_names, list->num_parents,
				reg_base, list->mux_offset, list->mux_shift,
				list->mux_width, list->mux_flags,
				list->div_offset, list->div_shift, list->div_width,
				list->div_flags, list->div_table,
				list->gate_offset, list->gate_shift,
				list->gate_flags, flags, &lock);
			break;
		}

		/* none of the cases above matched */
		if (!clk) {
			pr_err("%s: unknown clock type %d\n",
			       __func__, list->branch_type);
			continue;
		}

		if (IS_ERR(clk)) {
			pr_err("%s: failed to register clock %s: %ld\n",
			       __func__, list->name, PTR_ERR(clk));
			continue;
		}

		amlogic_clk_add_lookup(clk, list->id);

		if (list->name) {
			ret = clk_register_clkdev(clk, list->name,
						NULL);
			if (ret)
				pr_err("%s: failed to register lookup %s\n",
						__func__, list->name);
		}
	}
}

/* register a list of fixed clocks */
void __init amlogic_clk_register_fixed_rate(
		struct amlogic_fixed_rate_clock *list, unsigned int nr_clk)
{
	struct clk *clk;
	unsigned int idx, ret;

	for (idx = 0; idx < nr_clk; idx++, list++) {
		clk = clk_register_fixed_rate(NULL, list->name,
			list->parent_name, list->flags, list->fixed_rate);
		if (IS_ERR(clk)) {
			pr_err("%s: failed to register clock %s\n", __func__,
				list->name);
			continue;
		}

		amlogic_clk_add_lookup(clk, list->id);

		/*
		 * Unconditionally add a clock lookup for the fixed rate clocks.
		 * There are not many of these on any of Amlogic platforms.
		 */
		ret = clk_register_clkdev(clk, list->name, NULL);
		if (ret)
			pr_err("%s: failed to register clock lookup for %s",
				__func__, list->name);
	}
}

/* register a list of fixed factor clocks */
void __init amlogic_clk_register_fixed_factor(
		struct amlogic_fixed_factor_clock *list, unsigned int nr_clk)
{
	struct clk *clk;
	unsigned int idx;

	for (idx = 0; idx < nr_clk; idx++, list++) {
		clk = clk_register_fixed_factor(NULL, list->name,
			list->parent_name, list->flags, list->mult, list->div);
		if (IS_ERR(clk)) {
			pr_err("%s: failed to register clock %s\n", __func__,
				list->name);
			continue;
		}

		amlogic_clk_add_lookup(clk, list->id);
	}
}

/* register a list of mux clocks */
void __init amlogic_clk_register_mux(struct amlogic_mux_clock *list,
				unsigned int nr_clk)
{
	struct clk *clk;
	unsigned int idx, ret;
	void __iomem *base;

	for (idx = 0; idx < nr_clk; idx++, list++) {
		if (list->is_ao)
			base = reg_base_ao;   /*ao bus*/
		else
			base = reg_base;   /* cbus */

		clk = clk_register_mux(NULL, list->name, list->parent_names,
			list->num_parents, list->flags, base + list->offset,
			list->shift, list->width, list->mux_flags, &lock);
		if (IS_ERR(clk)) {
			pr_err("%s: failed to register clock %s\n", __func__,
				list->name);
			continue;
		}

		amlogic_clk_add_lookup(clk, list->id);

		/* register a clock lookup only if a clock alias is specified */
		if (list->name) {
			ret = clk_register_clkdev(clk, list->name,
						list->dev_name);
			if (ret)
				pr_err("%s: failed to register lookup %s\n",
						__func__, list->name);
		}
	}
}

/* register a list of div clocks */
void __init amlogic_clk_register_div(struct amlogic_div_clock *list,
				unsigned int nr_clk)
{
	struct clk *clk;
	unsigned int idx, ret;
	void __iomem *base;

	for (idx = 0; idx < nr_clk; idx++, list++) {
		if (list->is_ao)
			base = reg_base_ao;   /*ao bus*/
		else
			base = reg_base;   /* cbus */

		if (list->table)
			clk = clk_register_divider_table(NULL, list->name,
				list->parent_name, list->flags,
				base + list->offset, list->shift,
				list->width, list->div_flags,
				list->table, &lock);
		else
			clk = clk_register_divider(NULL, list->name,
				list->parent_name, list->flags,
				base + list->offset, list->shift,
				list->width, list->div_flags, &lock);
		if (IS_ERR(clk)) {
			pr_err("%s: failed to register clock %s\n", __func__,
				list->name);
			continue;
		}

		amlogic_clk_add_lookup(clk, list->id);

		/* register a clock lookup only if a clock alias is specified */
		if (list->name) {
			ret = clk_register_clkdev(clk, list->name,
						list->dev_name);
			if (ret)
				pr_err("%s: failed to register lookup %s\n",
						__func__, list->name);
		}
	}
}


/* register a list of gate clocks */
void __init amlogic_clk_register_gate(struct amlogic_gate_clock *list,
				unsigned int nr_clk)
{
	struct clk *clk;
	unsigned int idx, ret;
	void __iomem *base;

	for (idx = 0; idx < nr_clk; idx++, list++) {
		if (list->is_ao)
			base = reg_base_ao;   /*ao bus*/
		else
			base = reg_base;   /* cbus */

		clk = clk_register_gate(NULL, list->name, list->parent_name,
				list->flags, base + list->offset,
				list->bit_idx, list->gate_flags, &lock);

		if (IS_ERR(clk)) {
			pr_err("%s: failed to register clock %s\n", __func__,
				list->name);
			continue;
		}

		/* register a clock lookup only if a clock alias is specified */
		if (list->name) {
			ret = clk_register_clkdev(clk, list->name,
							list->dev_name);
			if (ret)
				pr_err("%s: failed to register lookup %s\n",
					__func__, list->name);
		}

		amlogic_clk_add_lookup(clk, list->id);
	}
}



/* setup the essentials required to support clock lookup using ccf */
void __init amlogic_clk_init(struct device_node *np, void __iomem *base,
		void __iomem *base_ao, unsigned long nr_clks,
		unsigned long *rdump, unsigned long nr_rdump,
		unsigned long *soc_rdump, unsigned long nr_soc_rdump)
{
	reg_base = base;
	reg_base_ao = base_ao;

	clk_table = kzalloc(sizeof(struct clk *) * nr_clks, GFP_KERNEL);
	if (!clk_table)
		panic("%s: could not allocate clock lookup table\n", __func__);

	if (!np)
		return;

#ifdef CONFIG_OF
	clk_data.clks = clk_table;
	clk_data.clk_num = nr_clks;
	of_clk_add_provider(np, of_clk_src_onecell_get, &clk_data);
#endif

}

/*
 * obtain the clock speed of all external fixed clock sources from device
 * tree and register it
 */
#ifdef CONFIG_OF
void __init amlogic_clk_of_register_fixed_ext(
			struct amlogic_fixed_rate_clock *fixed_rate_clk,
			unsigned int nr_fixed_rate_clk,
			struct of_device_id *clk_matches)
{

	amlogic_clk_register_fixed_rate(fixed_rate_clk, nr_fixed_rate_clk);
}
#endif

/* utility function to get the rate of a specified clock */
unsigned long _get_rate(const char *clk_name)
{
	struct clk *clk;

	clk = __clk_lookup(clk_name);
	if (!clk) {
		pr_err("%s: could not find clock %s\n", __func__, clk_name);
		return 0;
	}

	return clk_get_rate(clk);
}
