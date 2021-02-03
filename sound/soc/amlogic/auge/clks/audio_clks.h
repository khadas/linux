/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __AML_AUDIO_CLKS_H__
#define __AML_AUDIO_CLKS_H__

#include <linux/device.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/of_address.h>

#include <linux/clk-provider.h>

#define CLOCK_GATE(_name, _reg, _bit, _pname) \
	static struct clk_gate _name = { \
		.reg = (void __iomem *)(_reg), \
		.bit_idx = (_bit), \
		.lock = &aclk_lock, \
		.hw.init = &(struct clk_init_data) { \
			.name = #_name, \
			.ops = &clk_gate_ops, \
			.parent_names = (const char *[]){ #_pname }, \
			.num_parents = 1, \
			.flags = (CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED), \
		}, \
	}

#define CLOCK_COM_MUX(_name, _reg, _mask, _shift) \
	static struct clk_mux _name##_mux = { \
		.reg = (void __iomem *)(_reg), \
		.mask = (_mask), \
		.shift = (_shift), \
		.lock = &aclk_lock, \
	}
#define CLOCK_COM_DIV(_name, _reg, _shift, _width) \
	static struct clk_divider _name##_div = { \
		.reg = (void __iomem *)(_reg), \
		.shift = (_shift), \
		.width = (_width), \
		.lock = &aclk_lock, \
	}
#define CLOCK_COM_GATE(_name, _reg, _bit) \
	static struct clk_gate _name##_gate = { \
		.reg = (void __iomem *)(_reg), \
		.bit_idx = (_bit), \
		.lock = &aclk_lock, \
	}

#define IOMAP_COM_CLK(_name, _iobase) \
	do { \
		_name##_mux.reg += (unsigned long)(iobase); \
		_name##_div.reg += (unsigned long)(iobase); \
		_name##_gate.reg += (unsigned long)(iobase); \
	} while (0)

#define REGISTER_CLK_COM(_name) \
	clk_register_composite(NULL, #_name, \
			       mclk_parent_names, \
			       ARRAY_SIZE(mclk_parent_names), \
			       &_name##_mux.hw, &clk_mux_ops, \
			       &_name##_div.hw, &clk_divider_ops, \
			       &_name##_gate.hw, &clk_gate_ops, \
			       CLK_SET_RATE_NO_REPARENT)

#define REGISTER_AUDIOCLK_COM(_name) \
	clk_register_composite(NULL, #_name, \
			       audioclk_parent_names, \
			       ARRAY_SIZE(audioclk_parent_names), \
			       &_name##_mux.hw, &clk_mux_ops, \
			       &_name##_div.hw, &clk_divider_ops, \
			       &_name##_gate.hw, &clk_gate_ops, \
			       CLK_SET_RATE_NO_REPARENT)

#define REGISTER_CLK_COM_PARENTS(_name, pnames) \
	clk_register_composite(NULL, #_name, \
			       pnames##_parent_names, \
			       ARRAY_SIZE(pnames##_parent_names), \
			       &_name##_mux.hw, &clk_mux_ops, \
			       &_name##_div.hw, &clk_divider_ops, \
			       &_name##_gate.hw, &clk_gate_ops, \
			       CLK_SET_RATE_NO_REPARENT)

struct audio_clk_init {
	int clk_num;
	int (*clk_gates)(struct clk **clks, void __iomem *iobase);
	int (*clks)(struct clk **clks, void __iomem *iobase);
	int (*clk2_gates)(struct clk **clks, void __iomem *iobase);
	int (*clks2)(struct clk **clks, void __iomem *iobase);
};

extern struct audio_clk_init sm1_audio_clks_init;
extern struct audio_clk_init sc2_audio_clks_init;
extern struct audio_clk_init tm2_audio_clks_init;
extern struct audio_clk_init a1_audio_clks_init;
extern struct audio_clk_init t5_audio_clks_init;
extern struct audio_clk_init t7_audio_clks_init;
extern spinlock_t aclk_lock;

struct clk_chipinfo {
	/* force clock source as oscin(24M) */
	bool force_oscin_fn;
	/*
	 * Sclk_ws_inv for tdm; if not, echo for axg tdm
	 */
	bool sclk_ws_inv;
};
#endif
