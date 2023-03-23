/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __TDM_MATCH_TABLE_H__
#define __TDM_MATCH_TABLE_H__

#include "tdm_gain_version.h"

/* For OE function V1:
 * OE is set by EE_AUDIO_TDMOUT_A_CTRL0 & EE_AUDIO_TDMOUT_A_CTRL1
 */
#define OE_FUNCTION_V1 1
/* For OE function V2:
 * OE is set by EE_AUDIO_TDMOUT_A_CTRL2
 */
#define OE_FUNCTION_V2 2

struct src_table {
	char name[32];
	unsigned int val;
};

struct tdm_chipinfo {
	/* device id */
	unsigned int id;

	/* lane max count */
	unsigned int lane_cnt;

	/* no eco, sclk_ws_inv for out */
	bool sclk_ws_inv;

	/* output en (oe) for pinmux */
	unsigned int oe_fn;
	/* same source */
	bool same_src_fn;


	/* ACODEC_ADC function */
	bool adc_fn;

	/* mclk pad offset */
	bool mclkpad_no_offset;

	/* offset config for SW_RESET in reg.h */
	int reset_reg_offset;

	/* async fifo */
	bool async_fifo;

	/* from tm2_revb */
	bool separate_tohdmitx_en;

	/* only for A113D */
	bool reset_tdmin;

	struct src_table *tdmin_srcs;
	bool slot_num_en;
	bool chnum_en;
	int  gain_ver;
	bool use_arb;
	bool use_vadtop;
	bool regulator;
};

#define SRC_TDMIN_A     "tdmin_a"
#define SRC_TDMIN_B     "tdmin_b"
#define SRC_TDMIN_C     "tdmin_c"
#define SRC_TDMIN_D     "tdmin_d"

#define SRC_TDMIND_A    "tdmind_a"
#define SRC_TDMIND_B    "tdmind_b"
#define SRC_TDMIND_C    "tdmind_c"
#define SRC_HDMIRX      "hdmirx"
#define SRC_ACODEC      "acodec_adc"
#define SRC_TDMOUT_A    "tdmout_a"
#define SRC_TDMOUT_B    "tdmout_b"
#define SRC_TDMOUT_C    "tdmout_c"
#define SRC_TDMOUT_D    "tdmout_d"
#define TDMIN_SRC_CONFIG(_name, _val) \
{	.name = (_name), .val = (_val)}

struct src_table tdmin_srcs_v1[] = {
	TDMIN_SRC_CONFIG(SRC_TDMIN_A, 0),
	TDMIN_SRC_CONFIG(SRC_TDMIN_B, 1),
	TDMIN_SRC_CONFIG(SRC_TDMIN_C, 2),
	TDMIN_SRC_CONFIG(SRC_TDMIND_A, 3),
	TDMIN_SRC_CONFIG(SRC_TDMIND_B, 4),
	TDMIN_SRC_CONFIG(SRC_TDMIND_C, 5),
	TDMIN_SRC_CONFIG(SRC_HDMIRX, 6),
	TDMIN_SRC_CONFIG(SRC_ACODEC, 7),
	TDMIN_SRC_CONFIG(SRC_TDMOUT_A, 13),
	TDMIN_SRC_CONFIG(SRC_TDMOUT_B, 14),
	TDMIN_SRC_CONFIG(SRC_TDMOUT_C, 15),
	{ /* sentinel */ }
};

/* t5 afterwards */
struct src_table tdmin_srcs_v2[] = {
	TDMIN_SRC_CONFIG(SRC_TDMIN_A, 0),
	TDMIN_SRC_CONFIG(SRC_TDMIN_B, 1),
	TDMIN_SRC_CONFIG(SRC_TDMIN_C, 2),
	TDMIN_SRC_CONFIG(SRC_HDMIRX, 4),
	TDMIN_SRC_CONFIG(SRC_ACODEC, 5),
	TDMIN_SRC_CONFIG(SRC_TDMOUT_A, 12),
	TDMIN_SRC_CONFIG(SRC_TDMOUT_B, 13),
	TDMIN_SRC_CONFIG(SRC_TDMOUT_C, 14),
	{ /* sentinel */ }
};

/* t7 afterwards */
struct src_table tdmin_srcs_v3[] = {
	TDMIN_SRC_CONFIG(SRC_TDMIN_A, 0),
	TDMIN_SRC_CONFIG(SRC_TDMIN_B, 1),
	TDMIN_SRC_CONFIG(SRC_TDMIN_C, 2),
	TDMIN_SRC_CONFIG(SRC_TDMIN_D, 3),
	TDMIN_SRC_CONFIG(SRC_HDMIRX, 6),
	TDMIN_SRC_CONFIG(SRC_ACODEC, 7),
	TDMIN_SRC_CONFIG(SRC_TDMOUT_A, 12),
	TDMIN_SRC_CONFIG(SRC_TDMOUT_B, 13),
	TDMIN_SRC_CONFIG(SRC_TDMOUT_C, 14),
	TDMIN_SRC_CONFIG(SRC_TDMOUT_D, 15),
	{ /* sentinel */ }
};

struct tdm_chipinfo g12a_tdma_chipinfo = {
	.id          = TDM_A,
	.sclk_ws_inv = true,
	.oe_fn       = true,
	.same_src_fn = true,
	.mclkpad_no_offset = true,
	.tdmin_srcs = &tdmin_srcs_v1[0],
	.chnum_en = true,
	.gain_ver = GAIN_VER1,
	.use_arb = true,
};

struct tdm_chipinfo g12a_tdmb_chipinfo = {
	.id          = TDM_B,
	.sclk_ws_inv = true,
	.oe_fn       = OE_FUNCTION_V1,
	.same_src_fn = true,
	.mclkpad_no_offset = true,
	.tdmin_srcs = &tdmin_srcs_v1[0],
	.chnum_en = true,
	.gain_ver = GAIN_VER1,
	.use_arb = true,
};

struct tdm_chipinfo g12a_tdmc_chipinfo = {
	.id          = TDM_C,
	.sclk_ws_inv = true,
	.oe_fn       = OE_FUNCTION_V1,
	.same_src_fn = true,
	.mclkpad_no_offset = true,
	.tdmin_srcs = &tdmin_srcs_v1[0],
	.chnum_en = true,
	.gain_ver = GAIN_VER1,
	.use_arb = true,
};

#ifndef CONFIG_AMLOGIC_REMOVE_OLD
struct tdm_chipinfo tl1_tdma_chipinfo = {
	.id          = TDM_A,
	.sclk_ws_inv = true,
	.oe_fn       = OE_FUNCTION_V1,
	.same_src_fn = true,
	.adc_fn      = true,
	.reset_reg_offset = 1,
	.async_fifo  = true,
	.tdmin_srcs = &tdmin_srcs_v1[0],
	.chnum_en = true,
	.gain_ver = GAIN_VER1,
	.use_arb = true,
};

struct tdm_chipinfo tl1_tdmb_chipinfo = {
	.id          = TDM_B,
	.sclk_ws_inv = true,
	.oe_fn       = OE_FUNCTION_V1,
	.same_src_fn = true,
	.adc_fn      = true,
	.reset_reg_offset = 1,
	.async_fifo  = true,
	.tdmin_srcs = &tdmin_srcs_v1[0],
	.chnum_en = true,
	.gain_ver = GAIN_VER1,
	.use_arb = true,
};

struct tdm_chipinfo tl1_tdmc_chipinfo = {
	.id          = TDM_C,
	.sclk_ws_inv = true,
	.oe_fn       = OE_FUNCTION_V1,
	.same_src_fn = true,
	.adc_fn      = true,
	.reset_reg_offset = 1,
	.async_fifo  = true,
	.tdmin_srcs = &tdmin_srcs_v1[0],
	.chnum_en = true,
	.gain_ver = GAIN_VER1,
	.use_arb = true,
};
#endif

struct tdm_chipinfo sm1_tdma_chipinfo = {
	.id          = TDM_A,
	.sclk_ws_inv = true,
	.oe_fn       = OE_FUNCTION_V2,
	.same_src_fn = true,
	.lane_cnt    = LANE_MAX0,
	.reset_reg_offset = 1,
	.async_fifo  = true,
	.tdmin_srcs = &tdmin_srcs_v1[0],
	.chnum_en = true,
	.gain_ver = GAIN_VER2,
	.use_arb = true,
};

struct tdm_chipinfo sm1_tdmb_chipinfo = {
	.id          = TDM_B,
	.sclk_ws_inv = true,
	.oe_fn       = OE_FUNCTION_V2,
	.same_src_fn = true,
	.lane_cnt    = LANE_MAX3,
	.reset_reg_offset = 1,
	.async_fifo  = true,
	.tdmin_srcs = &tdmin_srcs_v1[0],
	.chnum_en = true,
	.gain_ver = GAIN_VER2,
	.use_arb = true,
};

struct tdm_chipinfo sm1_tdmc_chipinfo = {
	.id          = TDM_C,
	.sclk_ws_inv = true,
	.oe_fn       = OE_FUNCTION_V2,
	.same_src_fn = true,
	.lane_cnt    = LANE_MAX1,
	.reset_reg_offset = 1,
	.async_fifo  = true,
	.tdmin_srcs = &tdmin_srcs_v1[0],
	.chnum_en = true,
	.gain_ver = GAIN_VER2,
	.use_arb = true,
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
	.tdmin_srcs = &tdmin_srcs_v1[0],
	.chnum_en = true,
	.gain_ver = GAIN_VER2,
	.use_arb = true,
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
	.tdmin_srcs = &tdmin_srcs_v1[0],
	.chnum_en = true,
	.gain_ver = GAIN_VER2,
	.use_arb = true,
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
	.tdmin_srcs = &tdmin_srcs_v1[0],
	.chnum_en = true,
	.gain_ver = GAIN_VER2,
	.use_arb = true,
};

struct tdm_chipinfo tm2_revb_tdma_chipinfo = {
	.id          = TDM_A,
	.sclk_ws_inv = true,
	.oe_fn       = OE_FUNCTION_V2,
	.same_src_fn = true,
	.adc_fn      = true,
	.lane_cnt    = LANE_MAX3,
	.reset_reg_offset = 1,
	.async_fifo  = true,
	.separate_tohdmitx_en = true,
	.tdmin_srcs = &tdmin_srcs_v1[0],
	.slot_num_en = true,
	.chnum_en = false,
	.gain_ver = GAIN_VER3,
	.use_arb = true,
};

struct tdm_chipinfo tm2_revb_tdmb_chipinfo = {
	.id          = TDM_B,
	.sclk_ws_inv = true,
	.oe_fn       = OE_FUNCTION_V2,
	.same_src_fn = true,
	.adc_fn      = true,
	.lane_cnt    = LANE_MAX1,
	.reset_reg_offset = 1,
	.async_fifo  = true,
	.separate_tohdmitx_en = true,
	.tdmin_srcs = &tdmin_srcs_v1[0],
	.slot_num_en = true,
	.chnum_en = false,
	.gain_ver = GAIN_VER3,
	.use_arb = true,
};

struct tdm_chipinfo tm2_revb_tdmc_chipinfo = {
	.id          = TDM_C,
	.sclk_ws_inv = true,
	.oe_fn       = OE_FUNCTION_V2,
	.same_src_fn = true,
	.adc_fn      = true,
	.lane_cnt    = LANE_MAX1,
	.reset_reg_offset = 1,
	.async_fifo  = true,
	.separate_tohdmitx_en = true,
	.tdmin_srcs = &tdmin_srcs_v1[0],
	.slot_num_en = true,
	.chnum_en = false,
	.gain_ver = GAIN_VER3,
	.use_arb = true,
};

struct tdm_chipinfo t5_tdma_chipinfo = {
	.id          = TDM_A,
	.sclk_ws_inv = true,
	.oe_fn       = OE_FUNCTION_V2,
	.same_src_fn = true,
	.adc_fn      = true,
	.lane_cnt    = LANE_MAX1,
	.reset_reg_offset = 1,
	.async_fifo  = true,
	.separate_tohdmitx_en = true,
	.tdmin_srcs = &tdmin_srcs_v2[0],
	.slot_num_en = true,
	.chnum_en = false,
	.gain_ver = GAIN_VER3,
	.use_arb = true,
};

struct tdm_chipinfo t5_tdmb_chipinfo = {
	.id          = TDM_B,
	.sclk_ws_inv = true,
	.oe_fn       = OE_FUNCTION_V2,
	.same_src_fn = true,
	.adc_fn      = true,
	.lane_cnt    = LANE_MAX1,
	.reset_reg_offset = 1,
	.async_fifo  = true,
	.separate_tohdmitx_en = true,
	.tdmin_srcs = &tdmin_srcs_v2[0],
	.slot_num_en = true,
	.chnum_en = false,
	.gain_ver = GAIN_VER3,
	.use_arb = true,
};

struct tdm_chipinfo t5_tdmc_chipinfo = {
	.id          = TDM_C,
	.sclk_ws_inv = true,
	.oe_fn       = OE_FUNCTION_V2,
	.same_src_fn = true,
	.adc_fn      = true,
	.lane_cnt    = LANE_MAX1,
	.reset_reg_offset = 1,
	.async_fifo  = true,
	.separate_tohdmitx_en = true,
	.tdmin_srcs = &tdmin_srcs_v2[0],
	.slot_num_en = true,
	.chnum_en = false,
	.gain_ver = GAIN_VER3,
	.use_arb = true,
};

struct tdm_chipinfo t7_tdma_chipinfo = {
	.id          = TDM_A,
	.sclk_ws_inv = true,
	.oe_fn       = OE_FUNCTION_V2,
	.same_src_fn = true,
	.adc_fn      = true,
	.lane_cnt    = LANE_MAX1,
	.reset_reg_offset = 1,
	.async_fifo  = true,
	.separate_tohdmitx_en = true,
	.tdmin_srcs = &tdmin_srcs_v3[0],
	.slot_num_en = true,
	.chnum_en = false,
	.gain_ver = GAIN_VER3,
	.use_arb = true,
};

struct tdm_chipinfo t7_tdmb_chipinfo = {
	.id          = TDM_B,
	.sclk_ws_inv = true,
	.oe_fn       = OE_FUNCTION_V2,
	.same_src_fn = true,
	.adc_fn      = true,
	.lane_cnt    = LANE_MAX1,
	.reset_reg_offset = 1,
	.async_fifo  = true,
	.separate_tohdmitx_en = true,
	.tdmin_srcs = &tdmin_srcs_v3[0],
	.slot_num_en = true,
	.chnum_en = false,
	.gain_ver = GAIN_VER3,
	.use_arb = true,
};

struct tdm_chipinfo t7_tdmc_chipinfo = {
	.id          = TDM_C,
	.sclk_ws_inv = true,
	.oe_fn       = OE_FUNCTION_V2,
	.same_src_fn = true,
	.adc_fn      = true,
	.lane_cnt    = LANE_MAX1,
	.reset_reg_offset = 1,
	.async_fifo  = true,
	.separate_tohdmitx_en = true,
	.tdmin_srcs = &tdmin_srcs_v3[0],
	.slot_num_en = true,
	.chnum_en = false,
	.gain_ver = GAIN_VER3,
	.use_arb = true,
};

struct tdm_chipinfo t7_tdmd_chipinfo = {
	.id          = TDM_D,
	.sclk_ws_inv = true,
	.oe_fn       = OE_FUNCTION_V2,
	.same_src_fn = true,
	.adc_fn      = true,
	.lane_cnt    = LANE_MAX1,
	.reset_reg_offset = 1,
	.async_fifo  = true,
	.separate_tohdmitx_en = true,
	.tdmin_srcs = &tdmin_srcs_v3[0],
	.slot_num_en = true,
	.chnum_en = false,
	.gain_ver = GAIN_VER3,
	.use_arb = true,
};

struct tdm_chipinfo p1_tdma_chipinfo = {
	.id          = TDM_A,
	.sclk_ws_inv = true,
	.oe_fn       = OE_FUNCTION_V2,
	.same_src_fn = false,
	.adc_fn      = false,
	.lane_cnt    = LANE_MAX1,
	.reset_reg_offset = 1,
	.async_fifo  = true,
	.separate_tohdmitx_en = true,
	.tdmin_srcs = &tdmin_srcs_v3[0],
	.slot_num_en = true,
	.chnum_en = false,
	.use_arb = true,
};

struct tdm_chipinfo p1_tdmb_chipinfo = {
	.id          = TDM_B,
	.sclk_ws_inv = true,
	.oe_fn       = OE_FUNCTION_V2,
	.same_src_fn = false,
	.adc_fn      = false,
	.lane_cnt    = LANE_MAX1,
	.reset_reg_offset = 1,
	.async_fifo  = true,
	.separate_tohdmitx_en = true,
	.tdmin_srcs = &tdmin_srcs_v3[0],
	.slot_num_en = true,
	.chnum_en = false,
	.use_arb = true,
};

struct tdm_chipinfo p1_tdmc_chipinfo = {
	.id          = TDM_C,
	.sclk_ws_inv = true,
	.oe_fn       = OE_FUNCTION_V2,
	.same_src_fn = false,
	.adc_fn      = false,
	.lane_cnt    = LANE_MAX1,
	.reset_reg_offset = 1,
	.async_fifo  = true,
	.separate_tohdmitx_en = true,
	.tdmin_srcs = &tdmin_srcs_v3[0],
	.slot_num_en = true,
	.chnum_en = false,
	.use_arb = true,
};

struct tdm_chipinfo p1_tdmd_chipinfo = {
	.id          = TDM_D,
	.sclk_ws_inv = true,
	.oe_fn       = OE_FUNCTION_V2,
	.same_src_fn = false,
	.adc_fn      = false,
	.lane_cnt    = LANE_MAX1,
	.reset_reg_offset = 1,
	.async_fifo  = true,
	.separate_tohdmitx_en = true,
	.tdmin_srcs = &tdmin_srcs_v3[0],
	.slot_num_en = true,
	.chnum_en = false,
	.use_arb = true,
};

struct tdm_chipinfo a5_tdma_chipinfo = {
	.id          = TDM_A,
	.sclk_ws_inv = true,
	.oe_fn       = OE_FUNCTION_V2,
	.same_src_fn = false,
	.adc_fn      = false,
	.lane_cnt    = LANE_MAX1,
	.reset_reg_offset = 1,
	.async_fifo  = true,
	.tdmin_srcs = &tdmin_srcs_v3[0],
	.slot_num_en = true,
	.chnum_en = false,
	.gain_ver = GAIN_VER3,
	.use_arb = false,
	.regulator = true,
};

struct tdm_chipinfo a5_tdmb_chipinfo = {
	.id          = TDM_B,
	.sclk_ws_inv = true,
	.oe_fn       = OE_FUNCTION_V2,
	.same_src_fn = false,
	.adc_fn      = false,
	.lane_cnt    = LANE_MAX3,
	.reset_reg_offset = 1,
	.async_fifo  = true,
	.tdmin_srcs = &tdmin_srcs_v3[0],
	.slot_num_en = true,
	.chnum_en = false,
	.gain_ver = GAIN_VER3,
	.use_arb = false,
	.regulator = true,
};

struct tdm_chipinfo a5_tdmc_chipinfo = {
	.id          = TDM_C,
	.sclk_ws_inv = true,
	.oe_fn       = OE_FUNCTION_V2,
	.same_src_fn = false,
	.adc_fn      = false,
	.lane_cnt    = LANE_MAX3,
	.reset_reg_offset = 1,
	.async_fifo  = true,
	.tdmin_srcs = &tdmin_srcs_v3[0],
	.slot_num_en = true,
	.chnum_en = false,
	.gain_ver = GAIN_VER3,
	.use_arb = false,
	.use_vadtop = true,
	.regulator = true,
};

struct tdm_chipinfo axg_tdma_chipinfo = {
	.id          = TDM_A,
	.reset_tdmin = true,
	.use_arb = true,
	.tdmin_srcs = &tdmin_srcs_v1[0],
};

struct tdm_chipinfo axg_tdmb_chipinfo = {
	.id          = TDM_B,
	.reset_tdmin = true,
	.use_arb = true,
	.tdmin_srcs = &tdmin_srcs_v1[0],
};

struct tdm_chipinfo axg_tdmc_chipinfo = {
	.id          = TDM_C,
	.reset_tdmin = true,
	.use_arb = true,
	.tdmin_srcs = &tdmin_srcs_v1[0],
};

static const struct of_device_id aml_tdm_device_id[] = {
	{
		.compatible = "amlogic, g12a-snd-tdma",
		.data       = &g12a_tdma_chipinfo,
	},
	{
		.compatible = "amlogic, g12a-snd-tdmb",
		.data       = &g12a_tdmb_chipinfo,
	},
	{
		.compatible = "amlogic, g12a-snd-tdmc",
		.data       = &g12a_tdmc_chipinfo,
	},
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
	{
		.compatible = "amlogic, tl1-snd-tdma",
		.data       = &tl1_tdma_chipinfo,
	},
	{
		.compatible = "amlogic, tl1-snd-tdmb",
		.data       = &tl1_tdmb_chipinfo,
	},
	{
		.compatible = "amlogic, tl1-snd-tdmc",
		.data       = &tl1_tdmc_chipinfo,
	},
#endif
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
	{
		.compatible = "amlogic, tm2-revb-snd-tdma",
		.data       = &tm2_revb_tdma_chipinfo,
	},
	{
		.compatible = "amlogic, tm2-revb-snd-tdmb",
		.data       = &tm2_revb_tdmb_chipinfo,
	},
	{
		.compatible = "amlogic, tm2-revb-snd-tdmc",
		.data       = &tm2_revb_tdmc_chipinfo,
	},
	{
		.compatible = "amlogic, t5-snd-tdma",
		.data       = &t5_tdma_chipinfo,
	},
	{
		.compatible = "amlogic, t5-snd-tdmb",
		.data       = &t5_tdmb_chipinfo,
	},
	{
		.compatible = "amlogic, t5-snd-tdmc",
		.data       = &t5_tdmc_chipinfo,
	},
	{
		.compatible = "amlogic, t7-snd-tdma",
		.data       = &t7_tdma_chipinfo,
	},
	{
		.compatible = "amlogic, t7-snd-tdmb",
		.data       = &t7_tdmb_chipinfo,
	},
	{
		.compatible = "amlogic, t7-snd-tdmc",
		.data       = &t7_tdmc_chipinfo,
	},
	{
		.compatible = "amlogic, t7-snd-tdmd",
		.data       = &t7_tdmd_chipinfo,
	},
	{
		.compatible = "amlogic, p1-snd-tdma",
		.data       = &p1_tdma_chipinfo,
	},
	{
		.compatible = "amlogic, p1-snd-tdmb",
		.data       = &p1_tdmb_chipinfo,
	},
	{
		.compatible = "amlogic, p1-snd-tdmc",
		.data       = &p1_tdmc_chipinfo,
	},
	{
		.compatible = "amlogic, p1-snd-tdmd",
		.data       = &p1_tdmd_chipinfo,
	},
	{
		.compatible = "amlogic, a5-snd-tdma",
		.data       = &a5_tdma_chipinfo,
	},
	{
		.compatible = "amlogic, a5-snd-tdmb",
		.data       = &a5_tdmb_chipinfo,
	},
		{
		.compatible = "amlogic, a5-snd-tdmc",
		.data       = &a5_tdmc_chipinfo,
	},
	{
		.compatible = "amlogic, axg-snd-tdma",
		.data       = &axg_tdma_chipinfo,
	},
	{
		.compatible = "amlogic, axg-snd-tdmb",
		.data       = &axg_tdmb_chipinfo,
	},
		{
		.compatible = "amlogic, axg-snd-tdmc",
		.data       = &axg_tdmc_chipinfo,
	},
	{}
};
MODULE_DEVICE_TABLE(of, aml_tdm_device_id);

#endif
