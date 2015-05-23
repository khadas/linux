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
	unsigned int sdm_in_shift;
	unsigned int sdm_in_width;
	unsigned int sdm_in;
	unsigned int n_in_shift;
	unsigned int n_in_width;
	unsigned int n_in;
	unsigned int SSEN_shift;
};

struct mpll_clk_tab {
	unsigned int		offset;
	unsigned int		offset2;
	const char		*name;
	const char		**parent_names;
	unsigned	int		id;
	unsigned int		flags;
};
#define MPLL(n, p, o, o2, i, f) \
{	\
	.name = n, \
	.parent_names = p, \
	.offset = o, \
	.offset2 = o2, \
	.id = i, \
	.flags = f, \
}
extern  void  mpll_clk_init(void __iomem *base, struct mpll_clk_tab *tab,
							unsigned int cnt);
#endif
