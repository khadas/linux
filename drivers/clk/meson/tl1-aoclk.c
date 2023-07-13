// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/clk-provider.h>
#include <linux/platform_device.h>
#include <linux/reset-controller.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include "meson-aoclk.h"

#include "clk-regmap.h"
#include "clk-dualdiv.h"
#include "clkcs_init.h"
#include <dt-bindings/clock/tl1-aoclkc.h>

/*
 * AO Configuration Clock registers offsets
 * Register offsets from the data sheet must be multiplied by 4.
 */
#define AO_RTI_PWR_CNTL_REG0	0x10
#define AO_RTI_GEN_CNTL_REG0	0x40
#define AO_CLK_GATE0		0x4c
#define AO_CLK_GATE0_SP		0x50
#define AO_OSCIN_CNTL		0x58
#define AO_CEC_CLK_CNTL_REG0	0x74
#define AO_CEC_CLK_CNTL_REG1	0x78
#define AO_SAR_CLK		0x90
#define AO_RTC_ALT_CLK_CNTL0	0x94
#define AO_RTC_ALT_CLK_CNTL1	0x98

/*
 * Like every other peripheral clock gate in Amlogic Clock drivers,
 * we are using CLK_IGNORE_UNUSED here, so we keep the state of the
 * bootloader. The goal is to remove this flag at some point.
 * Actually removing it will require some extensive test to be done safely.
 */
#define MESON_AO_GATE(_name, _reg, _bit)					\
struct clk_regmap _name = {							\
		.data = &(struct clk_regmap_gate_data){				\
			.offset = (_reg),					\
			.bit_idx = (_bit),					\
		},								\
		.hw.init = &(struct clk_init_data) {				\
			.name = #_name,						\
			.ops = &clk_regmap_gate_ops,				\
			.parent_names = (const char *[]){ "clk81" },		\
			.num_parents = 1,					\
			.flags = CLK_IGNORE_UNUSED				\
		},								\
}

MESON_AO_GATE(tl1_aoclkc_ahb_bus,		AO_CLK_GATE0, 0);
MESON_AO_GATE(tl1_aoclkc_ir,			AO_CLK_GATE0, 1);
MESON_AO_GATE(tl1_aoclkc_i2c_master,		AO_CLK_GATE0, 2);
MESON_AO_GATE(tl1_aoclkc_i2c_slave,		AO_CLK_GATE0, 3);
MESON_AO_GATE(tl1_aoclkc_uart1,			AO_CLK_GATE0, 4);
MESON_AO_GATE(tl1_aoclkc_prod_i2c,		AO_CLK_GATE0, 5);
MESON_AO_GATE(tl1_aoclkc_uart2,			AO_CLK_GATE0, 6);
MESON_AO_GATE(tl1_aoclkc_ir_blaster,		AO_CLK_GATE0, 7);
MESON_AO_GATE(tl1_aoclkc_saradc,		AO_CLK_GATE0, 8);

static struct clk_regmap tl1_aoclkc_cts_oscin = {
	.data = &(struct clk_regmap_gate_data){
		.offset = AO_RTI_PWR_CNTL_REG0,
		.bit_idx = 14,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cts_oscin",
		.ops = &clk_regmap_gate_ro_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "xtal",
		},
		.num_parents = 1,
	},
};

static const struct meson_clk_dualdiv_param tl1_aoclkc_32k_div_table[] = {
	{
		.dual	= 1,
		.n1	= 733,
		.m1	= 8,
		.n2	= 732,
		.m2	= 11,
	}, {}
};

/* 32k_by_oscin clock */

static struct clk_regmap tl1_aoclkc_32k_by_oscin_pre = {
	.data = &(struct clk_regmap_gate_data){
		.offset = AO_RTC_ALT_CLK_CNTL0,
		.bit_idx = 31,
	},
	.hw.init = &(struct clk_init_data){
		.name = "tl1_aoclkc_32k_by_oscin_pre",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_aoclkc_cts_oscin.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap tl1_aoclkc_32k_by_oscin_div = {
	.data = &(struct meson_clk_dualdiv_data){
		.n1 = {
			.reg_off = AO_RTC_ALT_CLK_CNTL0,
			.shift   = 0,
			.width   = 12,
		},
		.n2 = {
			.reg_off = AO_RTC_ALT_CLK_CNTL0,
			.shift   = 12,
			.width   = 12,
		},
		.m1 = {
			.reg_off = AO_RTC_ALT_CLK_CNTL1,
			.shift   = 0,
			.width   = 12,
		},
		.m2 = {
			.reg_off = AO_RTC_ALT_CLK_CNTL1,
			.shift   = 12,
			.width   = 12,
		},
		.dual = {
			.reg_off = AO_RTC_ALT_CLK_CNTL0,
			.shift   = 28,
			.width   = 1,
		},
		.table = tl1_aoclkc_32k_div_table,
	},
	.hw.init = &(struct clk_init_data){
		.name = "tl1_aoclkc_32k_by_oscin_div",
		.ops = &meson_clk_dualdiv_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_aoclkc_32k_by_oscin_pre.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap tl1_aoclkc_32k_by_oscin_sel = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = AO_RTC_ALT_CLK_CNTL1,
		.mask = 0x1,
		.shift = 24,
		.flags = CLK_MUX_ROUND_CLOSEST,
	},
	.hw.init = &(struct clk_init_data){
		.name = "tl1_aoclkc_32k_by_oscin_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_aoclkc_32k_by_oscin_div.hw,
			&tl1_aoclkc_32k_by_oscin_pre.hw,
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tl1_aoclkc_32k_by_oscin = {
	.data = &(struct clk_regmap_gate_data){
		.offset = AO_RTC_ALT_CLK_CNTL0,
		.bit_idx = 30,
	},
	.hw.init = &(struct clk_init_data){
		.name = "tl1_aoclkc_32k_by_oscin",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_aoclkc_32k_by_oscin_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/* cec clock */

static struct clk_regmap tl1_aoclkc_cec_pre = {
	.data = &(struct clk_regmap_gate_data){
		.offset = AO_CEC_CLK_CNTL_REG0,
		.bit_idx = 31,
	},
	.hw.init = &(struct clk_init_data){
		.name = "tl1_aoclkc_cec_pre",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_aoclkc_cts_oscin.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap tl1_aoclkc_cec_div = {
	.data = &(struct meson_clk_dualdiv_data){
		.n1 = {
			.reg_off = AO_CEC_CLK_CNTL_REG0,
			.shift   = 0,
			.width   = 12,
		},
		.n2 = {
			.reg_off = AO_CEC_CLK_CNTL_REG0,
			.shift   = 12,
			.width   = 12,
		},
		.m1 = {
			.reg_off = AO_CEC_CLK_CNTL_REG1,
			.shift   = 0,
			.width   = 12,
		},
		.m2 = {
			.reg_off = AO_CEC_CLK_CNTL_REG1,
			.shift   = 12,
			.width   = 12,
		},
		.dual = {
			.reg_off = AO_CEC_CLK_CNTL_REG0,
			.shift   = 28,
			.width   = 1,
		},
		.table = tl1_aoclkc_32k_div_table,
	},
	.hw.init = &(struct clk_init_data){
		.name = "tl1_aoclkc_cec_div",
		.ops = &meson_clk_dualdiv_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_aoclkc_cec_pre.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap tl1_aoclkc_cec_sel = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = AO_CEC_CLK_CNTL_REG1,
		.mask = 0x1,
		.shift = 24,
		.flags = CLK_MUX_ROUND_CLOSEST,
	},
	.hw.init = &(struct clk_init_data){
		.name = "tl1_aoclkc_cec_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_aoclkc_cec_div.hw,
			&tl1_aoclkc_cec_pre.hw,
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tl1_aoclkc_cec = {
	.data = &(struct clk_regmap_gate_data){
		.offset = AO_CEC_CLK_CNTL_REG0,
		.bit_idx = 30,
	},
	.hw.init = &(struct clk_init_data){
		.name = "tl1_aoclkc_cec",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_aoclkc_cec_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tl1_aoclkc_cts_rtc_oscin = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = AO_RTI_PWR_CNTL_REG0,
		.mask = 0x1,
		.shift = 10,
		.flags = CLK_MUX_ROUND_CLOSEST,
	},
	.hw.init = &(struct clk_init_data){
		.name = "tl1_aoclkc_cts_rtc_oscin",
		.ops = &clk_regmap_mux_ops,
		.parent_data = (const struct clk_parent_data []) {
			{ .hw = &tl1_aoclkc_32k_by_oscin.hw },
			{ .fw_name = "ext-32k-0", },
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tl1_aoclkc_clk81 = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = AO_RTI_PWR_CNTL_REG0,
		.mask = 0x1,
		.shift = 8,
		.flags = CLK_MUX_ROUND_CLOSEST,
	},
	.hw.init = &(struct clk_init_data){
		.name = "tl1_aoclkc_clk81",
		.ops = &clk_regmap_mux_ro_ops,
		.parent_data = (const struct clk_parent_data []) {
			{ .fw_name = "mpeg-clk", },
			{ .hw = &tl1_aoclkc_cts_rtc_oscin.hw },
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tl1_aoclkc_saradc_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = AO_SAR_CLK,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "tl1_aoclkc_saradc_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = (const struct clk_parent_data []) {
			{ .fw_name = "xtal", },
			{ .hw = &tl1_aoclkc_clk81.hw },
		},
		.num_parents = 2,
	},
};

static struct clk_regmap tl1_aoclkc_saradc_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = AO_SAR_CLK,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "tl1_aoclkc_saradc_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_aoclkc_saradc_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tl1_aoclkc_saradc_gate = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = AO_SAR_CLK,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "tl1_aoclkc_saradc_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tl1_aoclkc_saradc_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap *tl1_aoclkc_regmap[] __initdata = {
	&tl1_aoclkc_ahb_bus,
	&tl1_aoclkc_ir,
	&tl1_aoclkc_i2c_master,
	&tl1_aoclkc_i2c_slave,
	&tl1_aoclkc_uart1,
	&tl1_aoclkc_prod_i2c,
	&tl1_aoclkc_uart2,
	&tl1_aoclkc_ir_blaster,
	&tl1_aoclkc_saradc,
	&tl1_aoclkc_cts_oscin,
	&tl1_aoclkc_32k_by_oscin_pre,
	&tl1_aoclkc_32k_by_oscin_div,
	&tl1_aoclkc_32k_by_oscin_sel,
	&tl1_aoclkc_32k_by_oscin,
	&tl1_aoclkc_cec_pre,
	&tl1_aoclkc_cec_div,
	&tl1_aoclkc_cec_sel,
	&tl1_aoclkc_cec,
	&tl1_aoclkc_cts_rtc_oscin,
	&tl1_aoclkc_clk81,
	&tl1_aoclkc_saradc_mux,
	&tl1_aoclkc_saradc_div,
	&tl1_aoclkc_saradc_gate,
};

static const struct clk_hw_onecell_data tl1_aoclkc_onecell_data = {
	.hws = {
		[CLKID_AO_AHB]		= &tl1_aoclkc_ahb_bus.hw,
		[CLKID_AO_IR_IN]	= &tl1_aoclkc_ir.hw,
		[CLKID_AO_I2C_M0]	= &tl1_aoclkc_i2c_master.hw,
		[CLKID_AO_I2C_S0]	= &tl1_aoclkc_i2c_slave.hw,
		[CLKID_AO_UART]		= &tl1_aoclkc_uart1.hw,
		[CLKID_AO_PROD_I2C]	= &tl1_aoclkc_prod_i2c.hw,
		[CLKID_AO_UART2]	= &tl1_aoclkc_uart2.hw,
		[CLKID_AO_IR_OUT]	= &tl1_aoclkc_ir_blaster.hw,
		[CLKID_AO_SAR_ADC]	= &tl1_aoclkc_saradc.hw,
		[CLKID_AO_CLK81]	= &tl1_aoclkc_clk81.hw,
		[CLKID_AO_SAR_ADC_SEL]	= &tl1_aoclkc_saradc_mux.hw,
		[CLKID_AO_SAR_ADC_DIV]	= &tl1_aoclkc_saradc_div.hw,
		[CLKID_AO_SAR_ADC_CLK]	= &tl1_aoclkc_saradc_gate.hw,
		[CLKID_AO_CTS_OSCIN]	= &tl1_aoclkc_cts_oscin.hw,
		[CLKID_AO_32K_PRE]	= &tl1_aoclkc_32k_by_oscin_pre.hw,
		[CLKID_AO_32K_DIV]	= &tl1_aoclkc_32k_by_oscin_div.hw,
		[CLKID_AO_32K_SEL]	= &tl1_aoclkc_32k_by_oscin_sel.hw,
		[CLKID_AO_32K]		= &tl1_aoclkc_32k_by_oscin.hw,
		[CLKID_AO_CEC_PRE]	= &tl1_aoclkc_cec_pre.hw,
		[CLKID_AO_CEC_DIV]	= &tl1_aoclkc_cec_div.hw,
		[CLKID_AO_CEC_SEL]	= &tl1_aoclkc_cec_sel.hw,
		[CLKID_AO_CEC]		= &tl1_aoclkc_cec.hw,
		[CLKID_AO_CTS_RTC_OSCIN] = &tl1_aoclkc_cts_rtc_oscin.hw,
	},
	.num = NR_CLKS,
};

static const struct meson_aoclk_data tl1_aoclkc_aoclkc_data = {
	.num_clks	= ARRAY_SIZE(tl1_aoclkc_regmap),
	.clks		= tl1_aoclkc_regmap,
	.hw_data	= &tl1_aoclkc_onecell_data,
};

static const struct of_device_id tl1_aoclkc_aoclkc_match_table[] = {
	{
		.compatible	= "amlogic,meson-tl1-aoclkc",
		.data		= &tl1_aoclkc_aoclkc_data,
	},
	{ }
};

static struct platform_driver tl1_aoclkc_driver = {
	.probe		= meson_aoclkc_probe,
	.driver		= {
		.name	= "tl1-aoclkc",
		.of_match_table = tl1_aoclkc_aoclkc_match_table,
	},
};

#ifndef MODULE
static int tl1_aoclkc_init(void)
{
	return platform_driver_register(&tl1_aoclkc_driver);
}

arch_initcall_sync(tl1_aoclkc_init);
#else
int __init meson_tl1_aoclkc_init(void)
{
	return platform_driver_register(&tl1_aoclkc_driver);
}
#endif

MODULE_LICENSE("GPL v2");
