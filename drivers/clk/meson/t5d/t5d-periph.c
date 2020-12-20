// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/clk-provider.h>
#include <linux/init.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/module.h>
#include "../clk-regmap.h"
#include "../meson-eeclk.h"
#include "t5d-periph.h"
#include "../clkcs_init.h"

static u32 mux_table_clk81[]	= { 6, 5, 7 };
static const struct clk_parent_data clk81_parent_data[] = {
	{ .fw_name = "fclk_div3", },
	{ .fw_name = "fclk_div4", },
	{ .fw_name = "fclk_div5", },
};

static struct clk_regmap t5d_mpeg_clk_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_MPEG_CLK_CNTL,
		.mask = 0x7,
		.shift = 12,
		.table = mux_table_clk81,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpeg_clk_sel",
		.ops = &clk_regmap_mux_ro_ops,
		.parent_data = clk81_parent_data,
		.num_parents = ARRAY_SIZE(clk81_parent_data),
	},
};

static struct clk_regmap t5d_mpeg_clk_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_MPEG_CLK_CNTL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpeg_clk_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_mpeg_clk_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t5d_clk81 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_MPEG_CLK_CNTL,
		.bit_idx = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "clk81",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_mpeg_clk_div.hw
		},
		.num_parents = 1,
		.flags = (CLK_SET_RATE_PARENT | CLK_IS_CRITICAL),
	},
};

static struct clk_regmap t5d_ts_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_TS_CLK_CNTL,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "ts_div",
		.ops = &clk_regmap_divider_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "xtal",
		},
		.num_parents = 1,
	},
};

static struct clk_regmap t5d_ts = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_TS_CLK_CNTL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "ts",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_ts_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

const char *t5d_gpu_parent_names[] = { "xtal", "gp0_pll", "hifi_pll",
	"fclk_div2p5", "fclk_div3", "fclk_div4", "fclk_div5", "fclk_div7"};

static struct clk_regmap t5d_gpu_p0_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_MALI_CLK_CNTL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "gpu_p0_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_names = t5d_gpu_parent_names,
		.num_parents = 8,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap t5d_gpu_p0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_MALI_CLK_CNTL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "gpu_p0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_gpu_p0_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t5d_gpu_p0_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_MALI_CLK_CNTL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "gpu_p0_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_gpu_p0_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t5d_gpu_p1_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_MALI_CLK_CNTL,
		.mask = 0x7,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data){
		.name = "gpu_p1_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_names = t5d_gpu_parent_names,
		.num_parents = 8,
	},
};

static struct clk_regmap t5d_gpu_p1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_MALI_CLK_CNTL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "gpu_p1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_gpu_p1_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t5d_gpu_p1_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_MALI_CLK_CNTL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "gpu_p1_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_gpu_p1_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t5d_gpu_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_MALI_CLK_CNTL,
		.mask = 0x1,
		.shift = 31,
	},
	.hw.init = &(struct clk_init_data){
		.name = "gpu_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_gpu_p0_gate.hw,
			&t5d_gpu_p1_gate.hw
		},
		.num_parents = 2,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_UNGATE
	},
};

static const char * const t5d_vpu_parent_names[] = { "fclk_div3",
	"fclk_div4", "fclk_div5", "fclk_div7", "null", "null",
	"null",  "null"};

static struct clk_regmap t5d_vpu_p0_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VPU_CLK_CNTL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vpu_p0_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_names = t5d_vpu_parent_names,
		.num_parents = 8,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap t5d_vpu_p0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VPU_CLK_CNTL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vpu_p0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_vpu_p0_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t5d_vpu_p0_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VPU_CLK_CNTL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_p0_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_vpu_p0_div.hw
		},
		.num_parents = 1,
		/* delete CLK_IGNORE_UNUSED in real chip */
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t5d_vpu_p1_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VPU_CLK_CNTL,
		.mask = 0x7,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vpu_p1_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_names = t5d_vpu_parent_names,
		.num_parents = 8,
	},
};

static struct clk_regmap t5d_vpu_p1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VPU_CLK_CNTL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vpu_p1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_vpu_p1_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t5d_vpu_p1_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VPU_CLK_CNTL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_p1_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_vpu_p1_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t5d_vpu_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VPU_CLK_CNTL,
		.mask = 0x1,
		.shift = 31,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vpu_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_vpu_p0_gate.hw,
			&t5d_vpu_p1_gate.hw
		},
		.num_parents = 2,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

/* vpu clkc clock, different with vpu clock */
static const char * const t5d_vpu_clkc_parent_names[] = { "fclk_div4",
	"fclk_div3", "fclk_div5", "fclk_div7", "null", "null",
	"null",  "null"};

static struct clk_regmap t5d_vpu_clkc_p0_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VPU_CLKC_CNTL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vpu_clkc_p0_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_names = t5d_vpu_clkc_parent_names,
		.num_parents = 8,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap t5d_vpu_clkc_p0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VPU_CLKC_CNTL,
		.shift = 0,
		.width = 7,
		.flags = CLK_DIVIDER_ALLOW_ZERO
	},
	.hw.init = &(struct clk_init_data){
		.name = "vpu_clkc_p0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_vpu_clkc_p0_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t5d_vpu_clkc_p0_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VPU_CLKC_CNTL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_clkc_p0_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_vpu_clkc_p0_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t5d_vpu_clkc_p1_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VPU_CLKC_CNTL,
		.mask = 0x7,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vpu_clkc_p1_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_names = t5d_vpu_clkc_parent_names,
		.num_parents = 8,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap t5d_vpu_clkc_p1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VPU_CLKC_CNTL,
		.shift = 16,
		.width = 7,
		.flags = CLK_DIVIDER_ALLOW_ZERO
	},
	.hw.init = &(struct clk_init_data){
		.name = "vpu_clkc_p1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_vpu_clkc_p1_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t5d_vpu_clkc_p1_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VPU_CLKC_CNTL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_clkc_p1_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_vpu_clkc_p1_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t5d_vpu_clkc_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VPU_CLKC_CNTL,
		.mask = 0x1,
		.shift = 31,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vpu_clkc_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_vpu_clkc_p0_gate.hw,
			&t5d_vpu_clkc_p1_gate.hw
		},
		.num_parents = 2,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static const char * const t5d_adc_extclk_in_parent_names[] = { "xtal",
	"fclk_div4", "fclk_div3", "fclk_div5",
	"fclk_div7", "mpll2", "gp0_pll", "hifi_pll" };

static struct clk_regmap t5d_adc_extclk_in_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_DEMOD_CLK_CNTL,
		.mask = 0x7,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data){
		.name = "adc_extclk_in_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_names = t5d_adc_extclk_in_parent_names,
		.num_parents = ARRAY_SIZE(t5d_adc_extclk_in_parent_names),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap t5d_adc_extclk_in_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_DEMOD_CLK_CNTL,
		.shift = 16,
		.width = 7,
		.flags = CLK_DIVIDER_ALLOW_ZERO
	},
	.hw.init = &(struct clk_init_data){
		.name = "adc_extclk_in_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_adc_extclk_in_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t5d_adc_extclk_in = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_DEMOD_CLK_CNTL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "adc_extclk_in",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_adc_extclk_in_div.hw
		},
		.num_parents = 1,
		/* delete CLK_IGNORE_UNUSED in real chip */
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static const char * const t5d_demod_parent_names[] = { "xtal",
	"fclk_div7", "fclk_div4", "adc_extclk_in" };

static struct clk_regmap t5d_demod_core_clk_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_DEMOD_CLK_CNTL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "demod_core_clk_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_names = t5d_demod_parent_names,
		.num_parents = 4,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap t5d_demod_core_clk_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_DEMOD_CLK_CNTL,
		.shift = 0,
		.width = 7,
		.flags = CLK_DIVIDER_ALLOW_ZERO
	},
	.hw.init = &(struct clk_init_data){
		.name = "demod_core_clk_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_demod_core_clk_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t5d_demod_core_clk = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_DEMOD_CLK_CNTL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "demod_core_clk",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_demod_core_clk_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT
	},
};

const char *t5d_dec_parent_names[] = { "fclk_div2p5", "fclk_div3",
	"fclk_div4", "fclk_div5", "fclk_div7", "hifi_pll", "gp0_pll", "xtal"};

static struct clk_regmap t5d_vdec_p0_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VDEC_CLK_CNTL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vdec_p0_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_names = t5d_dec_parent_names,
		.num_parents = 8,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap t5d_vdec_p0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VDEC_CLK_CNTL,
		.shift = 0,
		.width = 7,
		.flags = CLK_DIVIDER_ALLOW_ZERO
	},
	.hw.init = &(struct clk_init_data){
		.name = "vdec_p0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_vdec_p0_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t5d_vdec_p0_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VDEC_CLK_CNTL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vdec_p0_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_vdec_p0_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t5d_hevcf_p0_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VDEC2_CLK_CNTL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hevcf_p0_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_names = t5d_dec_parent_names,
		.num_parents = 8,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap t5d_hevcf_p0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VDEC2_CLK_CNTL,
		.shift = 0,
		.width = 7,
		.flags = CLK_DIVIDER_ALLOW_ZERO
	},
	.hw.init = &(struct clk_init_data){
		.name = "hevcf_p0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_hevcf_p0_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t5d_hevcf_p0_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VDEC2_CLK_CNTL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hevcf_p0_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_hevcf_p0_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t5d_vdec_p1_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VDEC3_CLK_CNTL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vdec_p1_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_names = t5d_dec_parent_names,
		.num_parents = 8,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap t5d_vdec_p1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VDEC3_CLK_CNTL,
		.shift = 0,
		.width = 7,
		.flags = CLK_DIVIDER_ALLOW_ZERO
	},
	.hw.init = &(struct clk_init_data){
		.name = "vdec_p1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_vdec_p1_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t5d_vdec_p1_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VDEC3_CLK_CNTL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vdec_p1_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_vdec_p1_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t5d_vdec_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VDEC3_CLK_CNTL,
		.mask = 0x1,
		.shift = 15,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vdec_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_vdec_p0_gate.hw,
			&t5d_vdec_p1_gate.hw
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap t5d_hevcf_p1_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VDEC4_CLK_CNTL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hevcf_p1_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_names = t5d_dec_parent_names,
		.num_parents = 8,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap t5d_hevcf_p1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VDEC4_CLK_CNTL,
		.shift = 0,
		.width = 7,
		.flags = CLK_DIVIDER_ALLOW_ZERO
	},
	.hw.init = &(struct clk_init_data){
		.name = "hevcf_p1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_hevcf_p1_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t5d_hevcf_p1_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VDEC4_CLK_CNTL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hevcf_p1_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_hevcf_p1_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t5d_hevcf_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VDEC4_CLK_CNTL,
		.mask = 0x1,
		.shift = 15,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hevcf_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_hevcf_p0_gate.hw,
			&t5d_hevcf_p0_gate.hw
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
	},
};

static const char * const t5d_hdcp22_esm_parent_names[] = { "fclk_div7",
	"fclk_div4", "fclk_div3", "fclk_div5" };

static struct clk_regmap t5d_hdcp22_esm_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_HDCP22_CLK_CNTL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdcp22_esm_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_names = t5d_hdcp22_esm_parent_names,
		.num_parents = 4,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap t5d_hdcp22_esm_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_HDCP22_CLK_CNTL,
		.shift = 0,
		.width = 7,
		.flags = CLK_DIVIDER_ALLOW_ZERO
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdcp22_esm_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_hdcp22_esm_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t5d_hdcp22_esm_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_HDCP22_CLK_CNTL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdcp22_esm_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_hdcp22_esm_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT
	},
};

static const char * const t5d_hdcp22_skp_parent_names[] = { "xtal",
	"fclk_div4", "fclk_div3", "fclk_div5" };

static struct clk_regmap t5d_hdcp22_skp_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_HDCP22_CLK_CNTL,
		.mask = 0x3,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdcp22_skp_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_names = t5d_hdcp22_skp_parent_names,
		.num_parents = 4,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap t5d_hdcp22_skp_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_HDCP22_CLK_CNTL,
		.shift = 16,
		.width = 7,
		.flags = CLK_DIVIDER_ALLOW_ZERO
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdcp22_skp_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_hdcp22_skp_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t5d_hdcp22_skp_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_HDCP22_CLK_CNTL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdcp22_skp_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_hdcp22_skp_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT
	},
};

static const char * const t5d_vapb_parent_names[] = { "fclk_div4",
	"fclk_div3", "fclk_div5", "fclk_div7", "mpll1", "null",
	"t5d_mpll2",  "fclk_div2p5"};

static struct clk_regmap t5d_vapb_p0_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VAPBCLK_CNTL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vapb_p0_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_names = t5d_vapb_parent_names,
		.num_parents = 8,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap t5d_vapb_p0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VAPBCLK_CNTL,
		.shift = 0,
		.width = 7,
		.flags = CLK_DIVIDER_ALLOW_ZERO
	},
	.hw.init = &(struct clk_init_data){
		.name = "vapb_p0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_vapb_p0_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t5d_vapb_p0_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VAPBCLK_CNTL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vapb_p0_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_vapb_p0_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t5d_vapb_p1_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VAPBCLK_CNTL,
		.mask = 0x7,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vapb_p1_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_names = t5d_vapb_parent_names,
		.num_parents = 8,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap t5d_vapb_p1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VAPBCLK_CNTL,
		.shift = 16,
		.width = 7,
		.flags = CLK_DIVIDER_ALLOW_ZERO
	},
	.hw.init = &(struct clk_init_data){
		.name = "vapb_p1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_vapb_p1_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t5d_vapb_p1_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VAPBCLK_CNTL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vapb_p1_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_vapb_p1_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t5d_vapb_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VAPBCLK_CNTL,
		.mask = 0x1,
		.shift = 31,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vapb_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_vapb_p0_gate.hw,
			&t5d_vapb_p1_gate.hw
		},
		.num_parents = 2,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t5d_ge2d_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VAPBCLK_CNTL,
		.bit_idx = 30,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "ge2d_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_vapb_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const char * const t5d_hdmirx_parent_names[] = { "xtal",
	"fclk_div4", "fclk_div3", "fclk_div5" };

static struct clk_regmap t5d_hdmirx_cfg_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_HDMIRX_CLK_CNTL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmirx_cfg_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_names = t5d_hdmirx_parent_names,
		.num_parents = 4,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap t5d_hdmirx_cfg_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_HDMIRX_CLK_CNTL,
		.shift = 0,
		.width = 7,
		.flags = CLK_DIVIDER_ALLOW_ZERO
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmirx_cfg_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_hdmirx_cfg_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t5d_hdmirx_cfg_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_HDMIRX_CLK_CNTL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmirx_cfg_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_hdmirx_cfg_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t5d_hdmirx_modet_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_HDMIRX_CLK_CNTL,
		.mask = 0x3,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmirx_modet_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_names = t5d_hdmirx_parent_names,
		.num_parents = 4,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap t5d_hdmirx_modet_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_HDMIRX_CLK_CNTL,
		.shift = 16,
		.width = 7,
		.flags = CLK_DIVIDER_ALLOW_ZERO
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmirx_modet_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_hdmirx_modet_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t5d_hdmirx_modet_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_HDMIRX_CLK_CNTL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmirx_modet_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_hdmirx_modet_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT
	},
};

static const char * const t5d_hdmirx_acr_parent_names[] = { "fclk_div4",
	"fclk_div3", "fclk_div5", "fclk_div7" };

static struct clk_regmap t5d_hdmirx_acr_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_HDMIRX_AUD_CLK_CNTL,
		.mask = 0x3,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmirx_acr_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_names = t5d_hdmirx_acr_parent_names,
		.num_parents = 4,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap t5d_hdmirx_acr_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_HDMIRX_AUD_CLK_CNTL,
		.shift = 16,
		.width = 7,
		.flags = CLK_DIVIDER_ALLOW_ZERO
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmirx_acr_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_hdmirx_acr_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t5d_hdmirx_acr_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_HDMIRX_AUD_CLK_CNTL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmirx_acr_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_hdmirx_acr_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t5d_hdmirx_meter_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_HDMIRX_METER_CLK_CNTL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmirx_meter_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_names = t5d_hdmirx_parent_names,
		.num_parents = 4,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap t5d_hdmirx_meter_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_HDMIRX_METER_CLK_CNTL,
		.shift = 0,
		.width = 7,
		.flags = CLK_DIVIDER_ALLOW_ZERO
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmirx_meter_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_hdmirx_meter_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t5d_hdmirx_meter_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_HDMIRX_METER_CLK_CNTL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmirx_meter_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_hdmirx_meter_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT
	},
};

static const char * const t5d_vdin_meas_parent_names[] = { "xtal", "fclk_div4",
	"fclk_div3", "fclk_div5" };

static struct clk_regmap t5d_vdin_meas_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VDIN_MEAS_CLK_CNTL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vdin_meas_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_names = t5d_vdin_meas_parent_names,
		.num_parents = 4,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap t5d_vdin_meas_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VDIN_MEAS_CLK_CNTL,
		.shift = 0,
		.width = 7,
		.flags = CLK_DIVIDER_ALLOW_ZERO
	},
	.hw.init = &(struct clk_init_data){
		.name = "vdin_meas_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_vdin_meas_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t5d_vdin_meas_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VDIN_MEAS_CLK_CNTL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vdin_meas_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_vdin_meas_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT
	},
};

static const char * const sd_emmc_parent_names[] = { "xtal", "fclk_div2",
	"fclk_div3", "fclk_div5", "fclk_div2p5", "mpll2", "mpll3", "gp0_pll" };

static struct clk_regmap t5d_sd_emmc_c_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_NAND_CLK_CNTL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sd_emmc_mux_c",
		.ops = &clk_regmap_mux_ops,
		.parent_names = sd_emmc_parent_names,
		.num_parents = 8,
		.flags = (CLK_GET_RATE_NOCACHE),
	},
};

static struct clk_regmap t5d_sd_emmc_c_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_NAND_CLK_CNTL,
		.shift = 0,
		.width = 7,
		.flags = CLK_DIVIDER_ALLOW_ZERO
	},
	.hw.init = &(struct clk_init_data){
		.name = "sd_emmc_div_c",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_sd_emmc_c_mux.hw
		},
		.num_parents = 1,
		//.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t5d_sd_emmc_c_gate  = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_NAND_CLK_CNTL,
		.bit_idx = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_gate_c",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_sd_emmc_c_div.hw
		},
		.num_parents = 1,
		//.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT
	},
};

static const char * const spicc_parent_names[] = { "xtal",
	"clk81", "fclk_div4", "fclk_div3", "fclk_div2", "fclk_div5",
	"fclk_div7"};

static struct clk_regmap t5d_spicc0_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_SPICC_CLK_CNTL,
		.mask = 0x7,
		.shift = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "spicc0_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_names = spicc_parent_names,
		.num_parents = 7,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap t5d_spicc0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_SPICC_CLK_CNTL,
		.shift = 0,
		.width = 6,
		.flags = CLK_DIVIDER_ALLOW_ZERO
	},
	.hw.init = &(struct clk_init_data){
		.name = "spicc0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_spicc0_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t5d_spicc0_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_SPICC_CLK_CNTL,
		.bit_idx = 6,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spicc0_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_spicc0_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t5d_vdac_clkc_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_CDAC_CLK_CNTL,
		.mask = 0x1,
		.shift = 16,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vdac_clkc_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_names = (const char *[]){ "xtal", "fclk_div5" },
		.num_parents = 2,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap t5d_vdac_clkc_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_CDAC_CLK_CNTL,
		.shift = 0,
		.width = 16,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vdac_clkc_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_vdac_clkc_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t5d_vdac_clkc_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_CDAC_CLK_CNTL,
		.bit_idx = 20,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vdac_clkc_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_vdac_clkc_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT
	},
};

/*
 * T5 : the last clk source is fclk_div7
 * T5D: the last clk source is fclk_div3.
 * And the last clk source is never used in T5, put fclk_div3 here for T5D
 */
static const char * const t5d_vpu_clkb_parent_names[] = { "vpu_mux", "fclk_div4",
				"fclk_div5", "fclk_div3" };

static struct clk_regmap t5d_vpu_clkb_tmp_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_VPU_CLKB_CNTL,
		.mask = 0x3,
		.shift = 20,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vpu_clkb_tmp_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_names = t5d_vpu_clkb_parent_names,
		.num_parents = 4,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap t5d_vpu_clkb_tmp_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VPU_CLKB_CNTL,
		.shift = 16,
		.width = 4,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vpu_clkb_tmp_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_vpu_clkb_tmp_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t5d_vpu_clkb_tmp_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VPU_CLKB_CNTL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_clkb_tmp_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_vpu_clkb_tmp_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t5d_vpu_clkb_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VPU_CLKB_CNTL,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vpu_clkb_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_vpu_clkb_tmp_gate.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t5d_vpu_clkb_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VPU_CLKB_CNTL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_clkb_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_vpu_clkb_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static const char * const t5d_tcon_pll_clk_parent_names[] = { "xtal", "fclk_div5",
				"fclk_div4", "fclk_div3", "mpll2", "mpll3",
				"null", "null" };

static struct clk_regmap t5d_tcon_pll_clk_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_TCON_CLK_CNTL,
		.mask = 0x7,
		.shift = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "tcon_pll_clk_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_names = t5d_tcon_pll_clk_parent_names,
		.num_parents = 8,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap t5d_tcon_pll_clk_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_TCON_CLK_CNTL,
		.shift = 0,
		.width = 6,
	},
	.hw.init = &(struct clk_init_data){
		.name = "tcon_pll_clk_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_tcon_pll_clk_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t5d_tcon_pll_clk_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_TCON_CLK_CNTL,
		.bit_idx = 6,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "tcon_pll_clk_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_tcon_pll_clk_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t5d_vid_lock_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_VID_LOCK_CLK_CNTL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vid_lock_div",
		.ops = &clk_regmap_divider_ops,
		.parent_names = (const char *[]){ "xtal" },
		.num_parents = 1,
	},
};

static struct clk_regmap t5d_vid_lock_clk = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_VID_LOCK_CLK_CNTL,
		.bit_idx = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vid_lock_clk",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_vid_lock_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const char * const t5d_hdmi_axi_parent_names[] = { "xtal", "fclk_div4",
				"fclk_div3", "fclk_div5"};

static struct clk_regmap t5d_hdmi_axi_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_HDMI_AXI_CLK_CNTL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmi_axi_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_names = t5d_hdmi_axi_parent_names,
		.num_parents = 4,
	},
};

static struct clk_regmap t5d_hdmi_axi_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_HDMI_AXI_CLK_CNTL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmi_axi_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_hdmi_axi_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t5d_hdmi_axi_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_HDMI_AXI_CLK_CNTL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmi_axi_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_hdmi_axi_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT
	},
};

static const char * const t5d_demod_t2_parent_names[] = { "xtal",
	"fclk_div5", "fclk_div4", "adc_extclk_in" };

static struct clk_regmap t5d_demod_core_t2_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_DEMOD_CLK_CNTL1,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "demod_core_t2_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_names = t5d_demod_t2_parent_names,
		.num_parents = 4,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap t5d_demod_core_t2_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_DEMOD_CLK_CNTL1,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "demod_core_t2_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_demod_core_t2_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t5d_demod_core_t2 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_DEMOD_CLK_CNTL1,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "demod_core_t2",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_demod_core_t2_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT
	},
};

static const char * const t5d_tsin_parent_names[] = { "fclk_div2",
	"xtal", "fclk_div4", "fclk_div3", "fclk_div2p5",
	"fclk_div7"};

static struct clk_regmap t5d_tsin_deglich_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = HHI_TSIN_DEGLITCH_CLK_CNTL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "tsin_deglich_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_names = t5d_tsin_parent_names,
		.num_parents = ARRAY_SIZE(t5d_tsin_parent_names),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap t5d_tsin_deglich_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = HHI_TSIN_DEGLITCH_CLK_CNTL,
		.shift = 0,
		.width = 6,
	},
	.hw.init = &(struct clk_init_data){
		.name = "tsin_deglich_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_tsin_deglich_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t5d_tsin_deglich = {
	.data = &(struct clk_regmap_gate_data){
		.offset = HHI_TSIN_DEGLITCH_CLK_CNTL,
		.bit_idx = 6,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "tsin_deglich",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_tsin_deglich_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT
	},
};

/* Array of all clocks provided by this provider */
static struct clk_hw_onecell_data t5d_periph_hw_onecell_data = {
	.hws = {
		[CLKID_MPEG_SEL]	= &t5d_mpeg_clk_sel.hw,
		[CLKID_MPEG_DIV]	= &t5d_mpeg_clk_div.hw,
		[CLKID_CLK81]		= &t5d_clk81.hw,
		[CLKID_GPU_P0_MUX]	= &t5d_gpu_p0_mux.hw,
		[CLKID_GPU_P0_DIV]	= &t5d_gpu_p0_div.hw,
		[CLKID_GPU_P0_GATE]	= &t5d_gpu_p0_gate.hw,
		[CLKID_GPU_P1_MUX]	= &t5d_gpu_p1_mux.hw,
		[CLKID_GPU_P1_DIV]	= &t5d_gpu_p1_div.hw,
		[CLKID_GPU_P1_GATE]	= &t5d_gpu_p1_gate.hw,
		[CLKID_GPU_MUX]		= &t5d_gpu_mux.hw,
		[CLKID_VPU_P0_MUX]	= &t5d_vpu_p0_mux.hw,
		[CLKID_VPU_P0_DIV]	= &t5d_vpu_p0_div.hw,
		[CLKID_VPU_P0_GATE]	= &t5d_vpu_p0_gate.hw,
		[CLKID_VPU_P1_MUX]	= &t5d_vpu_p1_mux.hw,
		[CLKID_VPU_P1_DIV]	= &t5d_vpu_p1_div.hw,
		[CLKID_VPU_P1_GATE]	= &t5d_vpu_p1_gate.hw,
		[CLKID_VPU_MUX]		= &t5d_vpu_mux.hw,
		[CLKID_VPU_CLKC_P0_MUX]	= &t5d_vpu_clkc_p0_mux.hw,
		[CLKID_VPU_CLKC_P0_DIV]	= &t5d_vpu_clkc_p0_div.hw,
		[CLKID_VPU_CLKC_P0_GATE]	= &t5d_vpu_clkc_p0_gate.hw,
		[CLKID_VPU_CLKC_P1_MUX]	= &t5d_vpu_clkc_p1_mux.hw,
		[CLKID_VPU_CLKC_P1_DIV]	= &t5d_vpu_clkc_p1_div.hw,
		[CLKID_VPU_CLKC_P1_GATE]	= &t5d_vpu_clkc_p1_gate.hw,
		[CLKID_VPU_CLKC_MUX]	= &t5d_vpu_clkc_mux.hw,
		[CLKID_ADC_EXTCLK_IN_MUX]	= &t5d_adc_extclk_in_mux.hw,
		[CLKID_ADC_EXTCLK_IN_DIV]	= &t5d_adc_extclk_in_div.hw,
		[CLKID_ADC_EXTCLK_IN]		= &t5d_adc_extclk_in.hw,
		[CLKID_DEMOOD_CORE_CLK_MUX]	= &t5d_demod_core_clk_mux.hw,
		[CLKID_DEMOOD_CORE_CLK_DIV]	= &t5d_demod_core_clk_div.hw,
		[CLKID_DEMOOD_CORE_CLK]		= &t5d_demod_core_clk.hw,
		[CLKID_VDEC_P0_MUX]		= &t5d_vdec_p0_mux.hw,
		[CLKID_VDEC_P0_DIV]		= &t5d_vdec_p0_div.hw,
		[CLKID_VDEC_P0_GATE]		= &t5d_vdec_p0_gate.hw,
		[CLKID_HEVCF_P0_MUX]		= &t5d_hevcf_p0_mux.hw,
		[CLKID_HEVCF_P0_DIV]		= &t5d_hevcf_p0_div.hw,
		[CLKID_HEVCF_P0_GATE]		= &t5d_hevcf_p0_gate.hw,
		[CLKID_VDEC_P1_MUX]		= &t5d_vdec_p1_mux.hw,
		[CLKID_VDEC_P1_DIV]		= &t5d_vdec_p1_div.hw,
		[CLKID_VDEC_P1_GATE]		= &t5d_vdec_p1_gate.hw,
		[CLKID_VDEC_MUX]		= &t5d_vdec_mux.hw,
		[CLKID_HEVCF_P1_MUX]		= &t5d_hevcf_p1_mux.hw,
		[CLKID_HEVCF_P1_DIV]		= &t5d_hevcf_p1_div.hw,
		[CLKID_HEVCF_P1_GATE]		= &t5d_hevcf_p1_gate.hw,
		[CLKID_HEVCF_MUX]		= &t5d_hevcf_mux.hw,
		[CLKID_HDCP22_ESM_MUX]		= &t5d_hdcp22_esm_mux.hw,
		[CLKID_HDCP22_SKP_MUX]		= &t5d_hdcp22_skp_mux.hw,
		[CLKID_HDCP22_ESM_DIV]		= &t5d_hdcp22_esm_div.hw,
		[CLKID_HDCP22_SKP_DIV]		= &t5d_hdcp22_skp_div.hw,
		[CLKID_HDCP22_ESM_GATE]		= &t5d_hdcp22_esm_gate.hw,
		[CLKID_HDCP22_SKP_GATE]		= &t5d_hdcp22_skp_gate.hw,
		[CLKID_VAPB_P0_MUX]		= &t5d_vapb_p0_mux.hw,
		[CLKID_VAPB_P1_MUX]		= &t5d_vapb_p1_mux.hw,
		[CLKID_VAPB_P0_DIV]		= &t5d_vapb_p0_div.hw,
		[CLKID_VAPB_P1_DIV]		= &t5d_vapb_p1_div.hw,
		[CLKID_VAPB_P0_GATE]		= &t5d_vapb_p0_gate.hw,
		[CLKID_VAPB_P1_GATE]		= &t5d_vapb_p1_gate.hw,
		[CLKID_VAPB_MUX]		= &t5d_vapb_mux.hw,
		[CLKID_HDMIRX_CFG_MUX]		= &t5d_hdmirx_cfg_mux.hw,
		[CLKID_HDMIRX_MODET_MUX]	= &t5d_hdmirx_modet_mux.hw,
		[CLKID_HDMIRX_CFG_DIV]		= &t5d_hdmirx_cfg_div.hw,
		[CLKID_HDMIRX_MODET_DIV]	= &t5d_hdmirx_modet_div.hw,
		[CLKID_HDMIRX_CFG_GATE]		= &t5d_hdmirx_cfg_gate.hw,
		[CLKID_HDMIRX_MODET_GATE]	= &t5d_hdmirx_modet_gate.hw,
		[CLKID_HDMIRX_ACR_MUX]		= &t5d_hdmirx_acr_mux.hw,
		[CLKID_HDMIRX_ACR_DIV]		= &t5d_hdmirx_acr_div.hw,
		[CLKID_HDMIRX_ACR_GATE]		= &t5d_hdmirx_acr_gate.hw,
		[CLKID_HDMIRX_METER_MUX]	= &t5d_hdmirx_meter_mux.hw,
		[CLKID_HDMIRX_METER_DIV]	= &t5d_hdmirx_meter_div.hw,
		[CLKID_HDMIRX_METER_GATE]	= &t5d_hdmirx_meter_gate.hw,
		[CLKID_VDIN_MEAS_MUX]		= &t5d_vdin_meas_mux.hw,
		[CLKID_VDIN_MEAS_DIV]		= &t5d_vdin_meas_div.hw,
		[CLKID_VDIN_MEAS_GATE]		= &t5d_vdin_meas_gate.hw,
		[CLKID_SD_EMMC_C_MUX]		= &t5d_sd_emmc_c_mux.hw,
		[CLKID_SD_EMMC_C_DIV]		= &t5d_sd_emmc_c_div.hw,
		[CLKID_SD_EMMC_C_GATE]		= &t5d_sd_emmc_c_gate.hw,
		[CLKID_SPICC0_MUX]		= &t5d_spicc0_mux.hw,
		[CLKID_SPICC0_DIV]		= &t5d_spicc0_div.hw,
		[CLKID_SPICC0_GATE]		= &t5d_spicc0_gate.hw,
		[CLKID_VDAC_CLKC_MUX]		= &t5d_vdac_clkc_mux.hw,
		[CLKID_VDAC_CLKC_DIV]		= &t5d_vdac_clkc_div.hw,
		[CLKID_VDAC_CLKC_GATE]		= &t5d_vdac_clkc_gate.hw,
		[CLKID_GE2D_GATE]               = &t5d_ge2d_gate.hw,
		[CLKID_VPU_CLKB_TMP_MUX]	= &t5d_vpu_clkb_tmp_mux.hw,
		[CLKID_VPU_CLKB_TMP_DIV]	= &t5d_vpu_clkb_tmp_div.hw,
		[CLKID_VPU_CLKB_TMP_GATE]	= &t5d_vpu_clkb_tmp_gate.hw,
		[CLKID_VPU_CLKB_DIV]		= &t5d_vpu_clkb_div.hw,
		[CLKID_VPU_CLKB_GATE]		= &t5d_vpu_clkb_gate.hw,
		[CLKID_TCON_PLL_CLK_MUX]	= &t5d_tcon_pll_clk_mux.hw,
		[CLKID_TCON_PLL_CLK_DIV]	= &t5d_tcon_pll_clk_div.hw,
		[CLKID_TCON_PLL_CLK_GATE]	= &t5d_tcon_pll_clk_gate.hw,
		[CLKID_VID_LOCK_DIV]		= &t5d_vid_lock_div.hw,
		[CLKID_VID_LOCK_CLK]		= &t5d_vid_lock_clk.hw,
		[CLKID_HDMI_AXI_MUX]		= &t5d_hdmi_axi_mux.hw,
		[CLKID_HDMI_AXI_DIV]		= &t5d_hdmi_axi_div.hw,
		[CLKID_HDMI_AXI_GATE]		= &t5d_hdmi_axi_gate.hw,
		[CLKID_DEMOD_T2_MUX]		= &t5d_demod_core_t2_mux.hw,
		[CLKID_DEMOD_T2_DIV]		= &t5d_demod_core_t2_div.hw,
		[CLKID_DEMOD_T2_GATE]		= &t5d_demod_core_t2.hw,
		[CLKID_TSIN_DEGLICH_MUX]	= &t5d_tsin_deglich_mux.hw,
		[CLKID_TSIN_DEGLICH_DIV]	= &t5d_tsin_deglich_div.hw,
		[CLKID_TSIN_DEGLICH_GATE]	= &t5d_tsin_deglich.hw,
		[NR_PER_CLKS]		= NULL,
	},
	.num = NR_PER_CLKS,
};

/* Convenience table to populate regmap in .probe */
static struct clk_regmap *const t5d_periph_clk_regmaps[] = {
	&t5d_mpeg_clk_sel,
	&t5d_mpeg_clk_div,
	&t5d_clk81,
	&t5d_ts_div,
	&t5d_ts,
	&t5d_gpu_p0_mux,
	&t5d_gpu_p1_mux,
	&t5d_gpu_mux,
	&t5d_vpu_p0_mux,
	&t5d_vpu_p1_mux,
	&t5d_vpu_mux,
	&t5d_vpu_clkc_p0_mux,
	&t5d_vpu_clkc_p1_mux,
	&t5d_vpu_clkc_mux,
	&t5d_adc_extclk_in_mux,
	&t5d_demod_core_clk_mux,
	&t5d_vdec_p0_mux,
	&t5d_hevcf_p0_mux,
	&t5d_vdec_p1_mux,
	&t5d_vdec_mux,
	&t5d_hevcf_p1_mux,
	&t5d_hevcf_mux,
	&t5d_hdcp22_esm_mux,
	&t5d_hdcp22_skp_mux,
	&t5d_vapb_p0_mux,
	&t5d_vapb_p1_mux,
	&t5d_vapb_mux,
	&t5d_hdmirx_cfg_mux,
	&t5d_hdmirx_modet_mux,
	&t5d_hdmirx_acr_mux,
	&t5d_hdmirx_meter_mux,
	&t5d_vdin_meas_mux,
	&t5d_sd_emmc_c_mux,
	&t5d_spicc0_mux,
	&t5d_vdac_clkc_mux,
	&t5d_vpu_clkb_tmp_mux,
	&t5d_tcon_pll_clk_mux,
	&t5d_hdmi_axi_mux,
	&t5d_demod_core_t2_mux,
	&t5d_tsin_deglich_mux,
	&t5d_gpu_p0_div,
	&t5d_gpu_p1_div,
	&t5d_vpu_p0_div,
	&t5d_vpu_p1_div,
	&t5d_vpu_clkc_p0_div,
	&t5d_vpu_clkc_p1_div,
	&t5d_adc_extclk_in_div,
	&t5d_demod_core_clk_div,
	&t5d_vdec_p0_div,
	&t5d_hevcf_p0_div,
	&t5d_vdec_p1_div,
	&t5d_hevcf_p1_div,
	&t5d_hdcp22_esm_div,
	&t5d_hdcp22_skp_div,
	&t5d_vapb_p0_div,
	&t5d_vapb_p1_div,
	&t5d_hdmirx_cfg_div,
	&t5d_hdmirx_modet_div,
	&t5d_hdmirx_acr_div,
	&t5d_hdmirx_meter_div,
	&t5d_vdin_meas_div,
	&t5d_sd_emmc_c_div,
	&t5d_spicc0_div,
	&t5d_vdac_clkc_div,
	&t5d_vpu_clkb_tmp_div,
	&t5d_vpu_clkb_div,
	&t5d_tcon_pll_clk_div,
	&t5d_vid_lock_div,
	&t5d_hdmi_axi_div,
	&t5d_demod_core_t2_div,
	&t5d_tsin_deglich_div,
	&t5d_gpu_p0_gate,
	&t5d_gpu_p1_gate,
	&t5d_vpu_p0_gate,
	&t5d_vpu_p1_gate,
	&t5d_vpu_clkc_p0_gate,
	&t5d_vpu_clkc_p1_gate,
	&t5d_adc_extclk_in,
	&t5d_demod_core_clk,
	&t5d_vdec_p0_gate,
	&t5d_hevcf_p0_gate,
	&t5d_vdec_p1_gate,
	&t5d_hevcf_p1_gate,
	&t5d_hdcp22_esm_gate,
	&t5d_hdcp22_skp_gate,
	&t5d_vapb_p0_gate,
	&t5d_vapb_p1_gate,
	&t5d_hdmirx_cfg_gate,
	&t5d_hdmirx_modet_gate,
	&t5d_hdmirx_acr_gate,
	&t5d_hdmirx_meter_gate,
	&t5d_vdin_meas_gate,
	&t5d_sd_emmc_c_gate,
	&t5d_spicc0_gate,
	&t5d_vdac_clkc_gate,
	&t5d_ge2d_gate,
	&t5d_vpu_clkb_tmp_gate,
	&t5d_vpu_clkb_gate,
	&t5d_tcon_pll_clk_gate,
	&t5d_vid_lock_clk,
	&t5d_hdmi_axi_gate,
	&t5d_demod_core_t2,
	&t5d_tsin_deglich,
};

static int meson_t5d_periph_probe(struct platform_device *pdev)
{
	int ret;

	ret = meson_eeclkc_probe(pdev);
	if (ret)
		return ret;

	return 0;
}

const struct meson_eeclkc_data t5d_periph_data = {
	.regmap_clks = t5d_periph_clk_regmaps,
	.regmap_clk_num = ARRAY_SIZE(t5d_periph_clk_regmaps),
	.hw_onecell_data = &t5d_periph_hw_onecell_data,
};

static const struct of_device_id clkc_match_table[] = {
	{
		.compatible = "amlogic,t5d-periph-clkc",
		.data = &t5d_periph_data
	},
	{}
};

static struct platform_driver t5d_periph_driver = {
	.probe		= meson_t5d_periph_probe,
	.driver		= {
		.name	= "t5d-periph-clkc",
		.of_match_table = clkc_match_table,
	},
};

#ifndef MODULE
static int t5d_periph_clkc_init(void)
{
	return platform_driver_register(&t5d_periph_driver);
}

arch_initcall_sync(t5d_periph_clkc_init);
#else
int __init meson_t5d_periph_clkc_init(void)
{
	return platform_driver_register(&t5d_periph_driver);
}
#endif

MODULE_LICENSE("GPL v2");
