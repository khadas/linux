/*
 * drivers/amlogic/clk/clk.h
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

#ifndef __AMLOGIC_CLK_H
#define __AMLOGIC_CLK_H

#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/io.h>
#include <linux/clk-provider.h>
#include <linux/clk-private.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include "clk-pll.h"

#define PNAME(x) static char const *x[] __initconst


/**
 * struct amlogic_clk_provider: information about clock provider
 * @reg_base: virtual address for the register base.
 * @clk_data: holds clock related data like clk* and number of clocks.
 * @lock: maintains exclusion bwtween callbacks for a given clock-provider.
 */
struct amlogic_clk_provider {
	void __iomem *reg_base;
	struct clk_onecell_data clk_data;
	spinlock_t lock;
};

/**
 * struct amlogic_fixed_rate_clock: information about fixed-rate clock
 * @id: platform specific id of the clock.
 * @name: name of this fixed-rate clock.
 * @parent_name: optional parent clock name.
 * @flags: optional fixed-rate clock flags.
 * @fixed-rate: fixed clock rate of this clock.
 */
struct amlogic_fixed_rate_clock {
	unsigned int		id;
	char			*name;
	const char		*parent_name;
	unsigned long		flags;
	unsigned long		fixed_rate;
};

#define FRATE(_id, cname, pname, f, frate)		\
	{						\
		.id		= _id,			\
		.name		= cname,		\
		.parent_name	= pname,		\
		.flags		= f,			\
		.fixed_rate	= frate,		\
	}

/*
 * struct amlogic_fixed_factor_clock: information about fixed-factor clock
 * @id: platform specific id of the clock.
 * @name: name of this fixed-factor clock.
 * @parent_name: parent clock name.
 * @mult: fixed multiplication factor.
 * @div: fixed division factor.
 * @flags: optional fixed-factor clock flags.
 */
struct amlogic_fixed_factor_clock {
	unsigned int		id;
	char			*name;
	const char		*parent_name;
	unsigned long		mult;
	unsigned long		div;
	unsigned long		flags;
};

#define FFACTOR(_id, cname, pname, m, d, f)		\
	{						\
		.id		= _id,			\
		.name		= cname,		\
		.parent_name	= pname,		\
		.mult		= m,			\
		.div		= d,			\
		.flags		= f,			\
	}

/**
 * struct amlogic_mux_clock: information about mux clock
 * @id: platform specific id of the clock.
 * @dev_name: name of the device to which this clock belongs.
 * @name: name of this mux clock.
 * @parent_names: array of pointer to parent clock names.
 * @num_parents: number of parents listed in @parent_names.
 * @flags: optional flags for basic clock.
 * @offset: offset of the register for configuring the mux.
 * @shift: starting bit location of the mux control bit-field in @reg.
 * @width: width of the mux control bit-field in @reg.
 * @mux_flags: flags for mux-type clock.
 * @alias: optional clock alias name to be assigned to this clock.
 */
struct amlogic_mux_clock {
	unsigned int		id;
	const char		*dev_name;
	const char		*name;
	const char		**parent_names;
	u8			num_parents;
	unsigned long		flags;
	unsigned long		offset;
	u8			shift;
	u8			width;
	u8			mux_flags;
	const char		*alias;
	unsigned int is_ao;
};

#define __MUX(_id, dname, cname, pnames, o, s, w, f, mf, a, is_aobus)	\
	{							\
		.id		= _id,				\
		.dev_name	= dname,			\
		.name		= cname,			\
		.parent_names	= pnames,			\
		.num_parents	= ARRAY_SIZE(pnames),		\
		.flags		= (f) | CLK_SET_RATE_NO_REPARENT, \
		.offset		= o,				\
		.shift		= s,				\
		.width		= w,				\
		.mux_flags	= mf,				\
		.alias		= a,				\
		.is_ao    = is_aobus,    \
	}

#define MUX(_id, cname, pnames, o, s, w, is_ao)			\
	__MUX(_id, NULL, cname, pnames, o, s, w, 0, 0, NULL, is_ao)

#define MUX_A(_id, cname, pnames, o, s, w, a, is_ao)			\
	__MUX(_id, NULL, cname, pnames, o, s, w, 0, 0, a, is_ao)

#define MUX_F(_id, cname, pnames, o, s, w, f, mf, is_ao)		\
	__MUX(_id, NULL, cname, pnames, o, s, w, f, mf, NULL, is_ao)

#define MUX_FA(_id, cname, pnames, o, s, w, f, mf, a, is_ao)		\
	__MUX(_id, NULL, cname, pnames, o, s, w, f, mf, a, is_ao)

/**
 * @id: platform specific id of the clock.
 * struct amlogic_div_clock: information about div clock
 * @dev_name: name of the device to which this clock belongs.
 * @name: name of this div clock.
 * @parent_name: name of the parent clock.
 * @flags: optional flags for basic clock.
 * @offset: offset of the register for configuring the div.
 * @shift: starting bit location of the div control bit-field in @reg.
 * @div_flags: flags for div-type clock.
 * @alias: optional clock alias name to be assigned to this clock.
 */
struct amlogic_div_clock {
	unsigned int		id;
	const char		*dev_name;
	const char		*name;
	const char		*parent_name;
	unsigned long		flags;
	unsigned long		offset;
	u8			shift;
	u8			width;
	u8			div_flags;
	const char		*alias;
	struct clk_div_table	*table;
	unsigned int is_ao;     /*1: ao bus, 0: cbus */
};

#define __DIV(_id, dname, cname, pname, o, s, w, f, df, a, t, is_aobus)	\
	{							\
		.id		= _id,				\
		.dev_name	= dname,			\
		.name		= cname,			\
		.parent_name	= pname,			\
		.flags		= (f) | CLK_SET_RATE_NO_REPARENT,	\
		.offset		= o,				\
		.shift		= s,				\
		.width		= w,				\
		.div_flags	= df,				\
		.alias		= a,				\
		.table		= t,				\
		.is_ao    = is_aobus,    \
	}

#define DIV(_id, cname, pname, o, s, w, is_ao)				\
	__DIV(_id, NULL, cname, pname, o, s, w, 0, 0, NULL, NULL, is_ao)

#define DIV_A(_id, cname, pname, o, s, w, a, is_ao)			\
	__DIV(_id, NULL, cname, pname, o, s, w, 0, 0, a, NULL, is_ao)

#define DIV_F(_id, cname, pname, o, s, w, f, df, is_ao)		\
	__DIV(_id, NULL, cname, pname, o, s, w, f, df, NULL, NULL, is_ao)

#define DIV_T(_id, cname, pname, o, s, w, t, is_ao)			\
	__DIV(_id, NULL, cname, pname, o, s, w, 0, 0, NULL, t, is_ao)

/**
 * struct amlogic_gate_clock: information about gate clock
 * @id: platform specific id of the clock.
 * @dev_name: name of the device to which this clock belongs.
 * @name: name of this gate clock.
 * @parent_name: name of the parent clock.
 * @flags: optional flags for basic clock.
 * @offset: offset of the register for configuring the gate.
 * @bit_idx: bit index of the gate control bit-field in @reg.
 * @gate_flags: flags for gate-type clock.
 * @alias: optional clock alias name to be assigned to this clock.
 */
struct amlogic_gate_clock {
	unsigned int		id;
	const char		*dev_name;
	const char		*name;
	const char		*parent_name;
	unsigned long		flags;
	unsigned long		offset;
	u8			bit_idx;
	u8			gate_flags;
	const char		*alias;
	unsigned int is_ao;     /*1: ao bus, 0: cbus */
};

#define __GATE(_id, dname, cname, pname, o, b, f, gf, a, is_aobus)	\
	{							\
		.id		= _id,				\
		.dev_name	= dname,			\
		.name		= cname,			\
		.parent_name	= pname,			\
		.flags		= f,				\
		.offset		= o,				\
		.bit_idx	= b,				\
		.gate_flags	= gf,				\
		.alias		= a,				\
		.is_ao    = is_aobus,    \
	}

#define GATE(_id, cname, pname, o, b, f, gf, is_ao)			\
	__GATE(_id, NULL, cname, pname, o, b, f, gf, NULL, is_ao)

#define GATE_A(_id, cname, pname, o, b, f, gf, a, is_ao)		\
	__GATE(_id, NULL, cname, pname, o, b, f, gf, a, is_ao)

#define GATE_D(_id, dname, cname, pname, o, b, f, gf, is_ao)		\
	__GATE(_id, dname, cname, pname, o, b, f, gf, NULL, is_ao)

#define GATE_DA(_id, dname, cname, pname, o, b, f, gf, a, is_ao)	\
	__GATE(_id, dname, cname, pname, o, b, f, gf, a, is_ao)


/**
 * struct amlogic_clk_reg_dump: register dump of clock controller registers.
 * @offset: clock register offset from the controller base address.
 * @value: the value to be register at offset.
 */
struct amlogic_clk_reg_dump {
	u32	offset;
	u32	value;
};
enum amlogic_clk_branch_type {
	branch_composite,
};

#define COMPOSITE(_id, cname, pnames, f, mo, ms, mw, mf, dof, ds, dw,\
		  df, go, gs, gf)				\
	{							\
		.id		= _id,				\
		.branch_type	= branch_composite,		\
		.name		= cname,			\
		.parent_names	= pnames,			\
		.num_parents	= ARRAY_SIZE(pnames),		\
		.flags		= f,				\
		.mux_offset	= mo,				\
		.mux_shift	= ms,				\
		.mux_width	= mw,				\
		.mux_flags	= mf,				\
		.div_offset = dof,				\
		.div_shift	= ds,				\
		.div_width	= dw,				\
		.div_flags	= df,				\
		.gate_offset	= go,				\
		.gate_shift	= gs,				\
		.gate_flags	= gf,				\
	}

#define COMPOSITE_NOMUX(_id, cname, pname, f, mo, dof, ds, dw, df,	\
			go, gs, gf)				\
	{							\
		.id		= _id,				\
		.branch_type	= branch_composite,		\
		.name		= cname,			\
		.parent_names	= pname,	\
		.num_parents	= 1,				\
		.flags		= f,				\
		.mux_offset	= mo,				\
		.div_offset = dof,				\
		.div_shift	= ds,				\
		.div_width	= dw,				\
		.div_flags	= df,				\
		.gate_offset	= go,				\
		.gate_shift	= gs,				\
		.gate_flags	= gf,				\
	}

#define COMPOSITE_NOMUX_DIVTBL(_id, cname, pname, f, mo, df, ds, dw,\
			       dof, dt, go, gs, gf)		\
	{							\
		.id		= _id,				\
		.branch_type	= branch_composite,		\
		.name		= cname,			\
		.parent_names	= (const char *[]){ pname },	\
		.num_parents	= 1,				\
		.flags		= f,				\
		.mux_offset	= mo,				\
		.div_offset = dof,				\
		.div_shift	= ds,				\
		.div_width	= dw,				\
		.div_flags	= df,				\
		.div_table	= dt,				\
		.gate_offset	= go,				\
		.gate_shift	= gs,				\
		.gate_flags	= gf,				\
	}

#define COMPOSITE_NODIV(_id, cname, pnames, f, mo, ms, mw, mf,	\
			go, gs, gf)				\
	{							\
		.id		= _id,				\
		.branch_type	= branch_composite,		\
		.name		= cname,			\
		.parent_names	= pnames,			\
		.num_parents	= ARRAY_SIZE(pnames),		\
		.flags		= f,				\
		.mux_offset	= mo,				\
		.mux_shift	= ms,				\
		.mux_width	= mw,				\
		.mux_flags	= mf,				\
		.gate_offset	= go,				\
		.gate_shift	= gs,				\
		.gate_flags	= gf,				\
	}

#define COMPOSITE_NOGATE(_id, cname, pnames, f, mo, ms, mw, mf,	\
			 dof, ds, dw, df)				\
	{							\
		.id		= _id,				\
		.branch_type	= branch_composite,		\
		.name		= cname,			\
		.parent_names	= pnames,			\
		.num_parents	= ARRAY_SIZE(pnames),		\
		.flags		= f,				\
		.mux_offset	= mo,				\
		.mux_shift	= ms,				\
		.mux_width	= mw,				\
		.mux_flags	= mf,				\
		.div_offset = dof,				\
		.div_shift	= ds,				\
		.div_width	= dw,				\
		.div_flags	= df,				\
		.gate_offset	= -1,				\
	}

struct amlogic_clk_branch {
	unsigned int			id;
	enum amlogic_clk_branch_type	branch_type;
	const char			*name;
	const char			**parent_names;
	u8				num_parents;
	unsigned long			flags;
	int				mux_offset;
	u8				mux_shift;
	u8				mux_width;
	u8				mux_flags;
	int				div_offset;
	u8				div_shift;
	u8				div_width;
	u8				div_flags;
	struct clk_div_table		*div_table;
	int				gate_offset;
	u8				gate_shift;
	u8				gate_flags;
};


#define __PLL(_typ, _id, _dname, _name, _pname, _flags, _con,	\
		_rtable, _alias)					\
	{								\
		.id		= _id,					\
		.type		= _typ,					\
		.dev_name	= _dname,				\
		.name		= _name,				\
		.parent_name	= _pname,				\
		.flags		= CLK_GET_RATE_NOCACHE,			\
		.con_offset	= _con,					\
		.rate_table	= _rtable,				\
		.alias		= _alias,				\
	}

#define PLL(_typ, _id, _name, _pname, _con, _rtable)	\
	__PLL(_typ, _id, NULL, _name, _pname, CLK_GET_RATE_NOCACHE,	\
		_con, _rtable, _name)

#define PLL_A(_typ, _id, _name, _pname, _con, _alias, _rtable) \
	__PLL(_typ, _id, NULL, _name, _pname, CLK_GET_RATE_NOCACHE,	\
		_con, _rtable, _alias)

extern void __init amlogic_clk_init(struct device_node *np, void __iomem *base,
		void __iomem *base_ao, unsigned long nr_clks,
		unsigned long *rdump, unsigned long nr_rdump,
		unsigned long *soc_rdump, unsigned long nr_soc_rdump);

extern void __init amlogic_clk_of_register_fixed_ext(
		struct amlogic_fixed_rate_clock *fixed_rate_clk,
		unsigned int nr_fixed_rate_clk,
		struct of_device_id *clk_matches);

extern void amlogic_clk_add_lookup(struct clk *clk, unsigned int id);

extern void __init amlogic_clk_register_fixed_rate(
		struct amlogic_fixed_rate_clock *clk_list, unsigned int nr_clk);
extern void __init amlogic_clk_register_fixed_factor(
		struct amlogic_fixed_factor_clock *list, unsigned int nr_clk);
extern void __init amlogic_clk_register_mux(
		struct amlogic_mux_clock *clk_list, unsigned int nr_clk);
extern void __init amlogic_clk_register_div(
		struct amlogic_div_clock *clk_list, unsigned int nr_clk);
extern void __init amlogic_clk_register_gate(
		struct amlogic_gate_clock *clk_list, unsigned int nr_clk);

extern void __init amlogic_clk_register_pll(struct amlogic_pll_clock *pll_list,
				unsigned int nr_pll, void __iomem *base);
extern void __init sys_pll_init(void __iomem *base,
				struct device_node *np, u32 clk_id);
void amlogic_clk_register_branches(struct amlogic_clk_branch *clk_list,
				    unsigned int nr_clk);
extern void __init gp0_clk_init(void __iomem *reg_base, unsigned int id);
extern void __init gp0_clk_gxl_init(void __iomem *reg_base, unsigned int id);
extern unsigned long _get_rate(const char *clk_name);
#ifdef CONFIG_RESET_CONTROLLER
void meson_register_rstc(struct device_node *np, unsigned int num_regs,
			 void __iomem *ao_base, void __iomem *ot_base,
			 unsigned int ao_off_id, u8 flags);
#else
static inline void meson_register_rstc(struct device_node *np,
				       unsigned int num_regs,
				       void __iomem *ao_base,
				       void __iomem *ot_base,
				       unsigned int ao_off_id, u8 flags)
{
}
#endif
#endif /* __AMLOGIC_CLK_H */
