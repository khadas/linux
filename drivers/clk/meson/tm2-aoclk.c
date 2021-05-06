// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/clk-provider.h>
#include <linux/platform_device.h>
#include <linux/reset-controller.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include "meson-aoclk.h"
#include "tm2-aoclk.h"

#include "clk-regmap.h"
#include "clk-dualdiv.h"
#include "clkcs_init.h"

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
#define AXG_AO_GATE(_name, _reg, _bit)					\
static struct clk_regmap tm2_aoclkc_##_name = {				\
	.data = &(struct clk_regmap_gate_data) {			\
		.offset = (_reg),					\
		.bit_idx = (_bit),					\
	},								\
	.hw.init = &(struct clk_init_data) {				\
		.name =  "tm2_aoclkc_" #_name,				\
		.ops = &clk_regmap_gate_ops,				\
		.parent_data = &(const struct clk_parent_data) {	\
			.fw_name = "mpeg-clk",				\
		},							\
		.num_parents = 1,					\
		.flags = CLK_IGNORE_UNUSED,				\
	},								\
}

AXG_AO_GATE(ahb_bus,		AO_CLK_GATE0, 0);
AXG_AO_GATE(ir,			AO_CLK_GATE0, 1);
AXG_AO_GATE(i2c_master,		AO_CLK_GATE0, 2);
AXG_AO_GATE(i2c_slave,		AO_CLK_GATE0, 3);
AXG_AO_GATE(uart1,		AO_CLK_GATE0, 4);
AXG_AO_GATE(prod_i2c,		AO_CLK_GATE0, 5);
AXG_AO_GATE(uart2,		AO_CLK_GATE0, 6);
AXG_AO_GATE(ir_blaster,		AO_CLK_GATE0, 7);
AXG_AO_GATE(saradc,		AO_CLK_GATE0, 8);
AXG_AO_GATE(m3,			AO_CLK_GATE0_SP, 1);
AXG_AO_GATE(ahb_sram,		AO_CLK_GATE0_SP, 2);
AXG_AO_GATE(rti,		AO_CLK_GATE0_SP, 3);
AXG_AO_GATE(m4_fclk,		AO_CLK_GATE0_SP, 4);
AXG_AO_GATE(m4_hclk,		AO_CLK_GATE0_SP, 5);

static struct clk_regmap tm2_aoclkc_cts_oscin = {
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

static const struct meson_clk_dualdiv_param tm2_aoclkc_32k_div_table[] = {
	{
		.dual	= 1,
		.n1	= 733,
		.m1	= 8,
		.n2	= 732,
		.m2	= 11,
	}, {}
};

/* 32k_by_oscin clock */

static struct clk_regmap tm2_aoclkc_32k_by_oscin_pre = {
	.data = &(struct clk_regmap_gate_data){
		.offset = AO_RTC_ALT_CLK_CNTL0,
		.bit_idx = 31,
	},
	.hw.init = &(struct clk_init_data){
		.name = "tm2_aoclkc_32k_by_oscin_pre",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_aoclkc_cts_oscin.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap tm2_aoclkc_32k_by_oscin_div = {
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
		.table = tm2_aoclkc_32k_div_table,
	},
	.hw.init = &(struct clk_init_data){
		.name = "tm2_aoclkc_32k_by_oscin_div",
		.ops = &meson_clk_dualdiv_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_aoclkc_32k_by_oscin_pre.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap tm2_aoclkc_32k_by_oscin_sel = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = AO_RTC_ALT_CLK_CNTL1,
		.mask = 0x1,
		.shift = 24,
		.flags = CLK_MUX_ROUND_CLOSEST,
	},
	.hw.init = &(struct clk_init_data){
		.name = "tm2_aoclkc_32k_by_oscin_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_aoclkc_32k_by_oscin_div.hw,
			&tm2_aoclkc_32k_by_oscin_pre.hw,
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tm2_aoclkc_32k_by_oscin = {
	.data = &(struct clk_regmap_gate_data){
		.offset = AO_RTC_ALT_CLK_CNTL0,
		.bit_idx = 30,
	},
	.hw.init = &(struct clk_init_data){
		.name = "tm2_aoclkc_32k_by_oscin",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_aoclkc_32k_by_oscin_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/* cec clock */

static struct clk_regmap tm2_aoclkc_cec_pre = {
	.data = &(struct clk_regmap_gate_data){
		.offset = AO_CEC_CLK_CNTL_REG0,
		.bit_idx = 31,
	},
	.hw.init = &(struct clk_init_data){
		.name = "tm2_aoclkc_cec_pre",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_aoclkc_cts_oscin.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap tm2_aoclkc_cec_div = {
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
		.table = tm2_aoclkc_32k_div_table,
	},
	.hw.init = &(struct clk_init_data){
		.name = "tm2_aoclkc_cec_div",
		.ops = &meson_clk_dualdiv_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_aoclkc_cec_pre.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap tm2_aoclkc_cec_sel = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = AO_CEC_CLK_CNTL_REG1,
		.mask = 0x1,
		.shift = 24,
		.flags = CLK_MUX_ROUND_CLOSEST,
	},
	.hw.init = &(struct clk_init_data){
		.name = "tm2_aoclkc_cec_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_aoclkc_cec_div.hw,
			&tm2_aoclkc_cec_pre.hw,
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tm2_aoclkc_cec = {
	.data = &(struct clk_regmap_gate_data){
		.offset = AO_CEC_CLK_CNTL_REG0,
		.bit_idx = 30,
	},
	.hw.init = &(struct clk_init_data){
		.name = "tm2_aoclkc_cec",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_aoclkc_cec_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tm2_aoclkc_cts_rtc_oscin = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = AO_RTI_PWR_CNTL_REG0,
		.mask = 0x1,
		.shift = 10,
		.flags = CLK_MUX_ROUND_CLOSEST,
	},
	.hw.init = &(struct clk_init_data){
		.name = "tm2_aoclkc_cts_rtc_oscin",
		.ops = &clk_regmap_mux_ops,
		.parent_data = (const struct clk_parent_data []) {
			{ .hw = &tm2_aoclkc_32k_by_oscin.hw },
			{ .fw_name = "ext-32k-0", },
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tm2_aoclkc_clk81 = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = AO_RTI_PWR_CNTL_REG0,
		.mask = 0x1,
		.shift = 8,
		.flags = CLK_MUX_ROUND_CLOSEST,
	},
	.hw.init = &(struct clk_init_data){
		.name = "tm2_aoclkc_clk81",
		.ops = &clk_regmap_mux_ro_ops,
		.parent_data = (const struct clk_parent_data []) {
			{ .fw_name = "mpeg-clk", },
			{ .hw = &tm2_aoclkc_cts_rtc_oscin.hw },
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tm2_aoclkc_saradc_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = AO_SAR_CLK,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "tm2_aoclkc_saradc_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = (const struct clk_parent_data []) {
			{ .fw_name = "xtal", },
			{ .hw = &tm2_aoclkc_clk81.hw },
		},
		.num_parents = 2,
	},
};

static struct clk_regmap tm2_aoclkc_saradc_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = AO_SAR_CLK,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "tm2_aoclkc_saradc_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_aoclkc_saradc_mux.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tm2_aoclkc_saradc_gate = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = AO_SAR_CLK,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "tm2_aoclkc_saradc_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tm2_aoclkc_saradc_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap *tm2_aoclkc_regmap[] __initdata = {
	&tm2_aoclkc_ahb_bus,
	&tm2_aoclkc_ir,
	&tm2_aoclkc_i2c_master,
	&tm2_aoclkc_i2c_slave,
	&tm2_aoclkc_uart1,
	&tm2_aoclkc_prod_i2c,
	&tm2_aoclkc_uart2,
	&tm2_aoclkc_ir_blaster,
	&tm2_aoclkc_saradc,
	&tm2_aoclkc_m3,
	&tm2_aoclkc_ahb_sram,
	&tm2_aoclkc_rti,
	&tm2_aoclkc_m4_fclk,
	&tm2_aoclkc_m4_hclk,
	&tm2_aoclkc_cts_oscin,
	&tm2_aoclkc_32k_by_oscin_pre,
	&tm2_aoclkc_32k_by_oscin_div,
	&tm2_aoclkc_32k_by_oscin_sel,
	&tm2_aoclkc_32k_by_oscin,
	&tm2_aoclkc_cec_pre,
	&tm2_aoclkc_cec_div,
	&tm2_aoclkc_cec_sel,
	&tm2_aoclkc_cec,
	&tm2_aoclkc_cts_rtc_oscin,
	&tm2_aoclkc_clk81,
	&tm2_aoclkc_saradc_mux,
	&tm2_aoclkc_saradc_div,
	&tm2_aoclkc_saradc_gate,
};

static const struct clk_hw_onecell_data tm2_aoclkc_onecell_data = {
	.hws = {
		[CLKID_AO_AHB]		= &tm2_aoclkc_ahb_bus.hw,
		[CLKID_AO_IR_IN]	= &tm2_aoclkc_ir.hw,
		[CLKID_AO_I2C_M0]	= &tm2_aoclkc_i2c_master.hw,
		[CLKID_AO_I2C_S0]	= &tm2_aoclkc_i2c_slave.hw,
		[CLKID_AO_UART]		= &tm2_aoclkc_uart1.hw,
		[CLKID_AO_PROD_I2C]	= &tm2_aoclkc_prod_i2c.hw,
		[CLKID_AO_UART2]	= &tm2_aoclkc_uart2.hw,
		[CLKID_AO_IR_OUT]	= &tm2_aoclkc_ir_blaster.hw,
		[CLKID_AO_SAR_ADC]	= &tm2_aoclkc_saradc.hw,
		[CLKID_AO_M3]		= &tm2_aoclkc_m3.hw,
		[CLKID_AO_AHB_SRAM]	= &tm2_aoclkc_ahb_sram.hw,
		[CLKID_AO_RTI]		= &tm2_aoclkc_rti.hw,
		[CLKID_AO_M4_FCLK]	= &tm2_aoclkc_m4_fclk.hw,
		[CLKID_AO_M4_HCLK]	= &tm2_aoclkc_m4_hclk.hw,
		[CLKID_AO_CLK81]	= &tm2_aoclkc_clk81.hw,
		[CLKID_AO_SAR_ADC_SEL]	= &tm2_aoclkc_saradc_mux.hw,
		[CLKID_AO_SAR_ADC_DIV]	= &tm2_aoclkc_saradc_div.hw,
		[CLKID_AO_SAR_ADC_CLK]	= &tm2_aoclkc_saradc_gate.hw,
		[CLKID_AO_CTS_OSCIN]	= &tm2_aoclkc_cts_oscin.hw,
		[CLKID_AO_32K_PRE]	= &tm2_aoclkc_32k_by_oscin_pre.hw,
		[CLKID_AO_32K_DIV]	= &tm2_aoclkc_32k_by_oscin_div.hw,
		[CLKID_AO_32K_SEL]	= &tm2_aoclkc_32k_by_oscin_sel.hw,
		[CLKID_AO_32K]		= &tm2_aoclkc_32k_by_oscin.hw,
		[CLKID_AO_CEC_PRE]	= &tm2_aoclkc_cec_pre.hw,
		[CLKID_AO_CEC_DIV]	= &tm2_aoclkc_cec_div.hw,
		[CLKID_AO_CEC_SEL]	= &tm2_aoclkc_cec_sel.hw,
		[CLKID_AO_CEC]		= &tm2_aoclkc_cec.hw,
		[CLKID_AO_CTS_RTC_OSCIN] = &tm2_aoclkc_cts_rtc_oscin.hw,
	},
	.num = NR_CLKS,
};

static const struct meson_aoclk_data tm2_aoclkc_aoclkc_data = {
	.num_clks	= ARRAY_SIZE(tm2_aoclkc_regmap),
	.clks		= tm2_aoclkc_regmap,
	.hw_data	= &tm2_aoclkc_onecell_data,
};

static const struct of_device_id tm2_aoclkc_aoclkc_match_table[] = {
	{
		.compatible	= "amlogic,meson-tm2-aoclkc",
		.data		= &tm2_aoclkc_aoclkc_data,
	},
	{ }
};

static struct platform_driver tm2_aoclkc_driver = {
	.probe		= meson_aoclkc_probe,
	.driver		= {
		.name	= "tm2-aoclkc",
		.of_match_table = tm2_aoclkc_aoclkc_match_table,
	},
};

#ifndef MODULE
static int tm2_aoclkc_init(void)
{
	return platform_driver_register(&tm2_aoclkc_driver);
}

arch_initcall_sync(tm2_aoclkc_init);
#else
int __init meson_tm2_aoclkc_init(void)
{
	return platform_driver_register(&tm2_aoclkc_driver);
}
#endif

MODULE_LICENSE("GPL v2");
