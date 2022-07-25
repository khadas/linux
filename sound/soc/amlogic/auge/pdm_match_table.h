/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __PDM_MATCH_TABLE_H__
#define __PDM_MATCH_TABLE_H__

static struct pdm_chipinfo g12a_pdm_chipinfo = {
	.id              = PDM_A,
	.mute_fn         = true,
	.truncate_data   = false,
	.use_arb         = true,
};

#ifndef CONFIG_AMLOGIC_REMOVE_OLD
static struct pdm_chipinfo tl1_pdm_chipinfo = {
	.id              = PDM_A,
	.mute_fn         = true,
	.truncate_data   = false,
	.use_arb         = true,
};
#endif

static struct pdm_chipinfo sm1_pdm_chipinfo = {
	.id              = PDM_A,
	.mute_fn         = true,
	.truncate_data   = false,
	.train           = true,
	.train_version   = PDM_TRAIN_VERSION_V1,
	.use_arb         = true,
};

static struct pdm_chipinfo tm2_revb_pdm_chipinfo = {
	.id              = PDM_A,
	.mute_fn         = true,
	.truncate_data   = false,
	.train           = true,
	.train_version   = PDM_TRAIN_VERSION_V1,
	.use_arb         = true,
	.oscin_divide    = true,
};

static struct pdm_chipinfo sc2_pdm_chipinfo = {
	.id              = PDM_A,
	.mute_fn         = true,
	.truncate_data   = false,
	.train           = true,
	.train_version   = PDM_TRAIN_VERSION_V2,
	.use_arb         = true,
	.oscin_divide    = true,
};

static struct pdm_chipinfo p1_pdm_chipinfo_a = {
	.id              = PDM_A,
	.mute_fn         = true,
	.truncate_data   = true,
	.train           = true,
	.train_version   = PDM_TRAIN_VERSION_V1,
	.use_arb         = true,
	.oscin_divide    = true,
};

static struct pdm_chipinfo p1_pdm_chipinfo_b = {
	.id              = PDM_B,
	.mute_fn         = true,
	.truncate_data   = true,
	.train           = true,
	.train_version   = PDM_TRAIN_VERSION_V1,
	.use_arb         = true,
	.oscin_divide    = true,
};

static struct pdm_chipinfo a5_pdm_chipinfo = {
	.id              = PDM_A,
	.mute_fn         = true,
	.truncate_data   = true,
	.train           = true,
	.train_version   = PDM_TRAIN_VERSION_V1,
	.use_arb         = false,
	.vad_top         = true,
	.regulator       = true,
	.oscin_divide    = true,
};

static struct pdm_chipinfo axg_pdm_chipinfo = {
	.id              = PDM_A,
	.train_version   = PDM_TRAIN_VERSION_V1,
	.use_arb         = true,
};

static const struct of_device_id aml_pdm_device_id[] = {
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
	{
		.compatible = "amlogic, tl1-snd-pdm",
		.data       = &tl1_pdm_chipinfo,
	},
#endif
	{
		.compatible = "amlogic, g12a-snd-pdm",
		.data       = &g12a_pdm_chipinfo,
	},
	{
		.compatible = "amlogic, sm1-snd-pdm",
		.data		= &sm1_pdm_chipinfo,
	},
	{
		.compatible = "amlogic, tm2-revb-snd-pdm",
		.data		= &tm2_revb_pdm_chipinfo,
	},
	{
		.compatible = "amlogic, sc2-snd-pdm",
		.data		= &sc2_pdm_chipinfo,
	},
	{
		.compatible = "amlogic, p1-snd-pdm-a",
		.data       = &p1_pdm_chipinfo_a,
	},
	{
		.compatible = "amlogic, p1-snd-pdm-b",
		.data       = &p1_pdm_chipinfo_b,
	},
	{
		.compatible = "amlogic, a5-snd-pdm",
		.data       = &a5_pdm_chipinfo,
	},
	{
		.compatible = "amlogic, axg-snd-pdm",
		.data       = &axg_pdm_chipinfo,
	},

	{}
};
MODULE_DEVICE_TABLE(of, aml_pdm_device_id);
#endif
