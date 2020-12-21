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
#include <dt-bindings/clock/t5d-aoclkc.h>
#include "../clk-regmap.h"
#include "../meson-eeclk.h"
#include "../clkcs_init.h"
#include "../clk-dualdiv.h"

#define AO_RTI_PWR_CNTL_REG0		0x10
#define AO_CLK_GATE0			0x4c
#define AO_CLK_GATE0_SP			0x50
#define AO_SAR_CLK			0x90
#define AO_CECB_CLK_CNTL_REG0		(0xa0 << 2)
#define AO_CECB_CLK_CNTL_REG1		(0xa1 << 2)

#define MESON_AO_GATE(_name, _reg, _bit)						\
struct clk_regmap _name = {                                             \
		.data = &(struct clk_regmap_gate_data){                         \
			.offset = (_reg),                                       \
			.bit_idx = (_bit),                                      \
		},                                                              \
		.hw.init = &(struct clk_init_data) {                            \
			.name = #_name,                                         \
			.ops = &clk_regmap_gate_ops,                            \
			.parent_names = (const char *[]){ "clk81" },		\
			.num_parents = 1,                                       \
			.flags = (CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED),     \
		},                                                              \
}

MESON_AO_GATE(t5d_ao_ahb_bus,		AO_CLK_GATE0, 0);
MESON_AO_GATE(t5d_ao_ir,		AO_CLK_GATE0, 1);
MESON_AO_GATE(t5d_ao_i2c_master,	AO_CLK_GATE0, 2);
MESON_AO_GATE(t5d_ao_i2c_slave,		AO_CLK_GATE0, 3);
MESON_AO_GATE(t5d_ao_uart1,		AO_CLK_GATE0, 4);
MESON_AO_GATE(t5d_ao_prod_i2c,		AO_CLK_GATE0, 5);
MESON_AO_GATE(t5d_ao_uart2,		AO_CLK_GATE0, 6);
MESON_AO_GATE(t5d_ao_ir_blaster,	AO_CLK_GATE0, 7);
MESON_AO_GATE(t5d_ao_sar_adc,		AO_CLK_GATE0, 8);

static struct clk_regmap t5d_aoclk81 = {
	.data = &(struct clk_regmap_mux_data){
		.offset = AO_RTI_PWR_CNTL_REG0,
		.mask = 0x1,
		.shift = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "aoclk81",
		.ops = &clk_regmap_mux_ops,
		.parent_names = (const char *[]){ "clk81", "ao_slow_clk" },
		.num_parents = 2,
	},
};

static struct clk_regmap t5d_saradc_mux = {
	.data = &(struct clk_regmap_mux_data){
		.offset = AO_SAR_CLK,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "saradc_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_names = (const char *[]){ "xtal", "aoclk81"},
		.num_parents = 2,
	},
};

static struct clk_regmap t5d_saradc_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = AO_SAR_CLK,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "saradc_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_saradc_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t5d_saradc_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = AO_SAR_CLK,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "saradc_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_saradc_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct meson_clk_dualdiv_param clk_32k_table[] = {
	{
		.dual	= 1,
		.n1	= 733,
		.m1	= 8,
		.n2	= 732,
		.m2	= 11,
	},
	{}
};

static struct clk_regmap t5d_cecb_32k_clkin = {
	.data = &(struct clk_regmap_gate_data){
		.offset = AO_CECB_CLK_CNTL_REG0,
		.bit_idx = 31,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "cecb_32k_clkin",
		.ops = &clk_regmap_gate_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "xtal",
		},
		.num_parents = 1,
	},
};

static struct clk_regmap t5d_cecb_32k_div = {
	.data = &(struct meson_clk_dualdiv_data){
		.n1 = {
			.reg_off = AO_CECB_CLK_CNTL_REG0,
			.shift   = 0,
			.width   = 12,
		},
		.n2 = {
			.reg_off = AO_CECB_CLK_CNTL_REG0,
			.shift   = 12,
			.width   = 12,
		},
		.m1 = {
			.reg_off = AO_CECB_CLK_CNTL_REG1,
			.shift   = 0,
			.width   = 12,
		},
		.m2 = {
			.reg_off = AO_CECB_CLK_CNTL_REG1,
			.shift   = 12,
			.width   = 12,
		},
		.dual = {
			.reg_off = AO_CECB_CLK_CNTL_REG0,
			.shift   = 28,
			.width   = 1,
		},
		.table = clk_32k_table,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cecb_32k_div",
		.ops = &meson_clk_dualdiv_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_cecb_32k_clkin.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap t5d_cecb_32k_sel_pre = {
	.data = &(struct clk_regmap_mux_data){
		.offset = AO_CECB_CLK_CNTL_REG1,
		.mask = 0x1,
		.shift = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cecb_32k_sel_pre",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_cecb_32k_div.hw,
			&t5d_cecb_32k_clkin.hw
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t5d_cecb_32k_clkout = {
	.data = &(struct clk_regmap_gate_data){
		.offset = AO_CECB_CLK_CNTL_REG0,
		.bit_idx = 30,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "cecb_32k_clkout",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t5d_cecb_32k_sel_pre.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

/* Array of all clocks provided by this provider */
static struct clk_hw_onecell_data t5d_aoclkc_hw_onecell_data = {
	.hws = {
		[CLKID_AO_CLK81] =	&t5d_aoclk81.hw,
		[CLKID_SARADC_MUX] =	&t5d_saradc_mux.hw,
		[CLKID_SARADC_DIV] =	&t5d_saradc_div.hw,
		[CLKID_SARADC_GATE] =	&t5d_saradc_gate.hw,
		[CLKID_AO_AHB_BUS] =	&t5d_ao_ahb_bus.hw,
		[CLKID_AO_IR] =		&t5d_ao_ir.hw,
		[CLKID_AO_I2C_MASTER] =	&t5d_ao_i2c_master.hw,
		[CLKID_AO_I2C_SLAVE] =	&t5d_ao_i2c_slave.hw,
		[CLKID_AO_UART1] =	&t5d_ao_uart1.hw,
		[CLKID_AO_PROD_I2C] =	&t5d_ao_prod_i2c.hw,
		[CLKID_AO_UART2] =	&t5d_ao_uart2.hw,
		[CLKID_AO_IR_BLASTER] =	&t5d_ao_ir_blaster.hw,
		[CLKID_AO_SAR_ADC] =	&t5d_ao_sar_adc.hw,
		[CLKID_CECB_32K_CLKIN]	= &t5d_cecb_32k_clkin.hw,
		[CLKID_CECB_32K_DIV]	= &t5d_cecb_32k_div.hw,
		[CLKID_CECB_32K_MUX_PRE] = &t5d_cecb_32k_sel_pre.hw,
		[CLKID_CECB_32K_CLKOUT]	= &t5d_cecb_32k_clkout.hw,
		[NR_AOCLKS]		= NULL,
	},
	.num = NR_AOCLKS,
};

/* Convenience table to populate regmap in .probe */
static struct clk_regmap *const t5d_aoclkc_clk_regmaps[] = {
	&t5d_ao_ahb_bus,
	&t5d_ao_ir,
	&t5d_ao_i2c_master,
	&t5d_ao_i2c_slave,
	&t5d_ao_uart1,
	&t5d_ao_prod_i2c,
	&t5d_ao_uart2,
	&t5d_ao_ir_blaster,
	&t5d_ao_sar_adc,
	&t5d_aoclk81,
	&t5d_saradc_mux,
	&t5d_saradc_div,
	&t5d_saradc_gate,
	&t5d_cecb_32k_clkin,
	&t5d_cecb_32k_div,
	&t5d_cecb_32k_sel_pre,
	&t5d_cecb_32k_clkout
};

const struct meson_eeclkc_data t5d_aoclkc_data = {
	.regmap_clks = t5d_aoclkc_clk_regmaps,
	.regmap_clk_num = ARRAY_SIZE(t5d_aoclkc_clk_regmaps),
	.hw_onecell_data = &t5d_aoclkc_hw_onecell_data,
};

static const struct of_device_id clkc_match_table[] = {
	{
		.compatible = "amlogic,t5d-aoclkc",
		.data = &t5d_aoclkc_data
	},
	{}
};

static struct platform_driver t5d_aoclkc_driver = {
	.probe		= meson_eeclkc_probe,
	.driver		= {
		.name	= "t5d-aoclkc",
		.of_match_table = clkc_match_table,
	},
};

#ifndef MODULE
static int t5d_aoclkc_init(void)
{
	return platform_driver_register(&t5d_aoclkc_driver);
}

arch_initcall_sync(t5d_aoclkc_init);
#else
int __init meson_t5d_aoclkc_init(void)
{
	return platform_driver_register(&t5d_aoclkc_driver);
}
#endif

MODULE_LICENSE("GPL v2");
