/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 BayLibre, SAS.
 * Author: Neil Armstrong <narmstrong@baylibre.com>
 */

#ifndef __MESON_CLK_CPU_DYNDIV_H
#define __MESON_CLK_CPU_DYNDIV_H

#include <linux/clk-provider.h>
#include "parm.h"

struct meson_clk_cpu_dyndiv_data {
	struct parm div;
	struct parm dyn;
};

struct cpu_dyn_table {
	unsigned long rate;
	u16	dyn_pre_mux;
	u16	dyn_post_mux;
	u16	dyn_div;
};

struct meson_sec_cpu_dyn_data {
	const struct cpu_dyn_table *table;
	u8 table_cnt;
	u8 secid_dyn_rd;
	u8 secid_dyn;
};

#define CPU_LOW_PARAMS(_rate, _dyn_pre_mux, _dyn_post_mux, _dyn_div)	\
	{								\
		.rate = (_rate),					\
		.dyn_pre_mux = (_dyn_pre_mux),				\
		.dyn_post_mux = (_dyn_post_mux),			\
		.dyn_div = (_dyn_div)					\
	}

extern const struct clk_ops meson_clk_cpu_dyndiv_ops;
extern const struct clk_ops meson_sec_cpu_dyn_ops;
extern const struct clk_ops meson_sec_sys_clk_ops;

#endif /* __MESON_CLK_CPU_DYNDIV_H */
