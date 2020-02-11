/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __TDM_MATCH_TABLE_H__
#define __TDM_MATCH_TABLE_H__

/* For OE function V1:
 * OE is set by EE_AUDIO_TDMOUT_A_CTRL0 & EE_AUDIO_TDMOUT_A_CTRL1
 */
#define OE_FUNCTION_V1 1
/* For OE function V2:
 * OE is set by EE_AUDIO_TDMOUT_A_CTRL2
 */
#define OE_FUNCTION_V2 2

struct tdm_chipinfo {
	/* device id */
	unsigned int id;

	/* lane max count */
	unsigned int lane_cnt;

	/* no eco, sclk_ws_inv for out */
	bool sclk_ws_inv;

	/* output en (oe) for pinmux */
	unsigned int oe_fn;

	/* clk pad */
	bool no_mclkpad_ctrl;

	/* same source */
	bool same_src_fn;

	/* same source, spdif re-enable */
	bool same_src_spdif_reen;

	/* ACODEC_ADC function */
	bool adc_fn;

	/* mclk pad offset */
	bool mclkpad_no_offset;

	/* offset config for SW_RESET in reg.h */
	int reset_reg_offset;

	/* async fifo */
	bool async_fifo;
};

struct tdm_chipinfo sm1_tdma_chipinfo = {
	.id          = TDM_A,
	.sclk_ws_inv = true,
	.oe_fn       = OE_FUNCTION_V2,
	.same_src_fn = true,
	.lane_cnt    = LANE_MAX0,
	.reset_reg_offset = 1,
	.async_fifo  = true,
};

struct tdm_chipinfo sm1_tdmb_chipinfo = {
	.id          = TDM_B,
	.sclk_ws_inv = true,
	.oe_fn       = OE_FUNCTION_V2,
	.same_src_fn = true,
	.lane_cnt    = LANE_MAX3,
	.reset_reg_offset = 1,
	.async_fifo  = true,
};

struct tdm_chipinfo sm1_tdmc_chipinfo = {
	.id          = TDM_C,
	.sclk_ws_inv = true,
	.oe_fn       = OE_FUNCTION_V2,
	.same_src_fn = true,
	.lane_cnt    = LANE_MAX1,
	.reset_reg_offset = 1,
	.async_fifo  = true,
};

struct tdm_chipinfo tm2_tdma_chipinfo = {
	.id          = TDM_A,
	.sclk_ws_inv = true,
	.oe_fn       = OE_FUNCTION_V2,
	.same_src_fn = true,
	.adc_fn      = true,
	.lane_cnt    = LANE_MAX3,
	.reset_reg_offset = 1,
	.async_fifo  = true,
};

struct tdm_chipinfo tm2_tdmb_chipinfo = {
	.id          = TDM_B,
	.sclk_ws_inv = true,
	.oe_fn       = OE_FUNCTION_V2,
	.same_src_fn = true,
	.adc_fn      = true,
	.lane_cnt    = LANE_MAX1,
	.reset_reg_offset = 1,
	.async_fifo  = true,
};

struct tdm_chipinfo tm2_tdmc_chipinfo = {
	.id          = TDM_C,
	.sclk_ws_inv = true,
	.oe_fn       = OE_FUNCTION_V2,
	.same_src_fn = true,
	.adc_fn      = true,
	.lane_cnt    = LANE_MAX1,
	.reset_reg_offset = 1,
	.async_fifo  = true,
};

static const struct of_device_id aml_tdm_device_id[] = {
	{
		.compatible = "amlogic, sm1-snd-tdma",
		.data       = &sm1_tdma_chipinfo,
	},
	{
		.compatible = "amlogic, sm1-snd-tdmb",
		.data       = &sm1_tdmb_chipinfo,
	},
	{
		.compatible = "amlogic, sm1-snd-tdmc",
		.data       = &sm1_tdmc_chipinfo,
	},
	{
		.compatible = "amlogic, tm2-snd-tdma",
		.data       = &tm2_tdma_chipinfo,
	},
	{
		.compatible = "amlogic, tm2-snd-tdmb",
		.data       = &tm2_tdmb_chipinfo,
	},
	{
		.compatible = "amlogic, tm2-snd-tdmc",
		.data       = &tm2_tdmc_chipinfo,
	},
	{},
};
MODULE_DEVICE_TABLE(of, aml_tdm_device_id);

#endif
