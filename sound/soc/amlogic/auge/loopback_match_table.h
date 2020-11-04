/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#define LOOPBACKA    0
#define LOOPBACKB    1

struct loopback_chipinfo {
	unsigned int id;

	/* reset all modules, after g12a
	 * it has reset bits control for modules
	 */
	bool is_reset_all;

	/* channel and mask switch to ctrl2, ctrl3 from tl1
	 * for datain, 0: channel and mask is controlled by ctrl0
	 *             1: channel and mask is controlled by ctrl2
	 * for datalb, 0: channel and mask is controlled by ctrl1
	 *             1: channel and mask is controlled by ctrl3
	 */
	bool ch_ctrl;

	/* EE_AUDIO_LB_A_CTRL0 bit 27, chnum_en
	 * from tm2 revb, no chnum_en
	 */
	bool chnum_en;
};

static struct loopback_chipinfo g12a_loopbacka_chipinfo = {
	.id      = LOOPBACKA,
	.chnum_en = true,
};

static struct loopback_chipinfo tl1_loopbacka_chipinfo = {
	.id      = LOOPBACKA,
	.ch_ctrl = true,
	.chnum_en = true,
};

static struct loopback_chipinfo tl1_loopbackb_chipinfo = {
	.id      = LOOPBACKB,
	.ch_ctrl = true,
	.chnum_en = true,
};

static struct loopback_chipinfo sm1_loopbacka_chipinfo = {
	.id      = LOOPBACKA,
	.ch_ctrl = true,
	.chnum_en = true,
};

static struct loopback_chipinfo sm1_loopbackb_chipinfo = {
	.id      = LOOPBACKB,
	.ch_ctrl = true,
	.chnum_en = true,
};

static struct loopback_chipinfo tm2_loopbacka_chipinfo = {
	.id      = LOOPBACKA,
	.ch_ctrl = true,
	.chnum_en = true,
};

static struct loopback_chipinfo tm2_loopbackb_chipinfo = {
	.id      = LOOPBACKB,
	.ch_ctrl = true,
	.chnum_en = true,
};

static struct loopback_chipinfo tm2_revb_loopbacka_chipinfo = {
	.id      = LOOPBACKA,
	.ch_ctrl = true,
	.chnum_en = false,
};

static struct loopback_chipinfo tm2_revb_loopbackb_chipinfo = {
	.id      = LOOPBACKB,
	.ch_ctrl = true,
	.chnum_en = false,
};

static const struct of_device_id loopback_device_id[] = {
	{
		.compatible = "amlogic, snd-loopback",
	},
	{
		.compatible = "amlogic, axg-loopback",
	},
	{
		.compatible = "amlogic, g12a-loopback",
		.data       = &g12a_loopbacka_chipinfo,
	},
	{
		.compatible = "amlogic, tl1-loopbacka",
		.data		= &tl1_loopbacka_chipinfo,
	},
	{
		.compatible = "amlogic, tl1-loopbackb",
		.data		= &tl1_loopbackb_chipinfo,
	},
	{
		.compatible = "amlogic, sm1-loopbacka",
		.data		= &sm1_loopbacka_chipinfo,
	},
	{
		.compatible = "amlogic, sm1-loopbackb",
		.data		= &sm1_loopbackb_chipinfo,
	},
	{
		.compatible = "amlogic, tm2-loopbacka",
		.data		= &tm2_loopbacka_chipinfo,
	},
	{
		.compatible = "amlogic, tm2-loopbackb",
		.data		= &tm2_loopbackb_chipinfo,
	},
	{
		.compatible = "amlogic, tm2-revb-loopbacka",
		.data		= &tm2_revb_loopbacka_chipinfo,
	},
	{
		.compatible = "amlogic, tm2-revb-loopbackb",
		.data		= &tm2_revb_loopbackb_chipinfo,
	},
	{}
};
MODULE_DEVICE_TABLE(of, loopback_device_id);
