/*
 * drivers/amlogic/clk/mpll_clk.h
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

#ifndef __MPLL_CLK_H
#define __MPLL_CLK_H

struct mpll_clk {
	struct clk_hw		hw;
	void __iomem	*con_reg;
	void __iomem	*con_reg2;
	void __iomem	*con_reg3;
	unsigned int sdm_in_shift;
	unsigned int sdm_in_width;
	unsigned int sdm_in;
	unsigned int n_in_shift;
	unsigned int n_in_width;
	unsigned int n_in;
	unsigned int SSEN_shift;
	int misc_offset;
};

struct mpll_clk_tab {
	unsigned int		offset;
	unsigned int		offset2;
	unsigned int		offset3;
	const char		*name;
	const char		**parent_names;
	unsigned	int		id;
	unsigned int		flags;
	int misc_offset;
};
#define MPLL(n, p, o, o2, i, f) \
{	\
	.name = n, \
	.parent_names = p, \
	.offset = o, \
	.offset2 = o2, \
	.offset3 = 0, \
	.id = i, \
	.flags = f, \
	.misc_offset = -1, \
}
#define MPLL_MISC(n, p, o, o2, o3, i, f, m) \
{	\
	.name = n, \
	.parent_names = p, \
	.offset = o, \
	.offset2 = o2, \
	.offset3 = o3, \
	.id = i, \
	.flags = f, \
	.misc_offset = m, \
}
extern  void  mpll_clk_init(void __iomem *base, struct mpll_clk_tab *tab,
							unsigned int cnt);
#endif
