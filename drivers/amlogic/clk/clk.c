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
	if (clk_table && id) {
		clk_table[id] = clk;
	  /*pr_debug("%s: add a clock: clk_table[%d] is %s\n",
		__func__, id, clk_table[id]->name);*/
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
		if (list->alias) {
			ret = clk_register_clkdev(clk, list->alias,
						list->dev_name);
			if (ret)
				pr_err("%s: failed to register lookup %s\n",
						__func__, list->alias);
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
		if (list->alias) {
			ret = clk_register_clkdev(clk, list->alias,
						list->dev_name);
			if (ret)
				pr_err("%s: failed to register lookup %s\n",
						__func__, list->alias);
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
		if (list->alias) {
			ret = clk_register_clkdev(clk, list->alias,
							list->dev_name);
			if (ret)
				pr_err("%s: failed to register lookup %s\n",
					__func__, list->alias);
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
	const struct of_device_id *match;
	struct device_node *np;
	u32 freq;

	for_each_matching_node_and_match(np, clk_matches, &match) {
		of_property_read_u32(np, "clock-frequency", &freq);
	if (of_property_read_u32(np, "clock-frequency", &freq))
			continue;
		fixed_rate_clk[(u32)match->data].fixed_rate = freq;
	}

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
