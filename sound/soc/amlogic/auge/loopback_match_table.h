/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include "loopback_hw.h"
#include "regs.h"

#define LOOPBACKA    0
#define LOOPBACKB    1

struct mux_conf lb_srcs_v1[] = {
	AUDIO_SRC_CONFIG("tdmin_a", 0, EE_AUDIO_LB_A_CTRL0, 0, 0x7),
	AUDIO_SRC_CONFIG("tdmin_b", 1, EE_AUDIO_LB_A_CTRL0, 0, 0x7),
	AUDIO_SRC_CONFIG("tdmin_c", 2, EE_AUDIO_LB_A_CTRL0, 0, 0x7),
	AUDIO_SRC_CONFIG("spdifin", 3, EE_AUDIO_LB_A_CTRL0, 0, 0x7),
	AUDIO_SRC_CONFIG("pdmin", 4, EE_AUDIO_LB_A_CTRL0, 0, 0x7),
	{ /* sentinel */ }
};

struct mux_conf lb_srcs_v2[] = {
	AUDIO_SRC_CONFIG("tdmin_a", 0, EE_AUDIO_LB_A_CTRL2, 20, 0x1f),
	AUDIO_SRC_CONFIG("tdmin_b", 1, EE_AUDIO_LB_A_CTRL2, 20, 0x1f),
	AUDIO_SRC_CONFIG("tdmin_c", 2, EE_AUDIO_LB_A_CTRL2, 20, 0x1f),
	AUDIO_SRC_CONFIG("spdifin", 3, EE_AUDIO_LB_A_CTRL2, 20, 0x1f),
	AUDIO_SRC_CONFIG("pdmin", 4, EE_AUDIO_LB_A_CTRL2, 20, 0x1f),
	AUDIO_SRC_CONFIG("fratv", 5, EE_AUDIO_LB_A_CTRL2, 20, 0x1f),
	AUDIO_SRC_CONFIG("tdmin_lb", 6, EE_AUDIO_LB_A_CTRL2, 20, 0x1f),
	AUDIO_SRC_CONFIG("loopback_a", 7, EE_AUDIO_LB_A_CTRL2, 20, 0x1f),
	AUDIO_SRC_CONFIG("frhdmirx", 8, EE_AUDIO_LB_A_CTRL2, 20, 0x1f),
	AUDIO_SRC_CONFIG("loopback_b", 9, EE_AUDIO_LB_A_CTRL2, 20, 0x1f),
	AUDIO_SRC_CONFIG("spdifin_lb", 10, EE_AUDIO_LB_A_CTRL2, 20, 0x1f),
	AUDIO_SRC_CONFIG("earc_rx_dmac", 11, EE_AUDIO_LB_A_CTRL2, 20, 0x1f),
	AUDIO_SRC_CONFIG("frhdmirx_pao", 12, EE_AUDIO_LB_A_CTRL2, 20, 0x1f),
	AUDIO_SRC_CONFIG("resample_a", 13, EE_AUDIO_LB_A_CTRL2, 20, 0x1f),
	AUDIO_SRC_CONFIG("resample_b", 14, EE_AUDIO_LB_A_CTRL2, 20, 0x1f),
	AUDIO_SRC_CONFIG("vad", 15, EE_AUDIO_LB_A_CTRL2, 20, 0x1f),
	AUDIO_SRC_CONFIG("pdmin_b", 16, EE_AUDIO_LB_A_CTRL2, 20, 0x1f),
	AUDIO_SRC_CONFIG("tdminb_lb", 17, EE_AUDIO_LB_A_CTRL2, 20, 0x1f),
	AUDIO_SRC_CONFIG("tdmin_d", 18, EE_AUDIO_LB_A_CTRL2, 20, 0x1f),
	{ /* sentinel */ }
};

struct mux_conf lb_srcs_v3[] = {
	AUDIO_SRC_CONFIG("tdmin_a", 0, EE_AUDIO_LB_A_CTRL2, 20, 0x1f),
	AUDIO_SRC_CONFIG("spdifin", 3, EE_AUDIO_LB_A_CTRL2, 20, 0x1f),
	AUDIO_SRC_CONFIG("tdmin_lb", 6, EE_AUDIO_LB_A_CTRL2, 20, 0x1f),
	AUDIO_SRC_CONFIG("loopback_a", 7, EE_AUDIO_LB_A_CTRL2, 20, 0x1f),
	AUDIO_SRC_CONFIG("resample_a", 13, EE_AUDIO_LB_A_CTRL2, 20, 0x1f),
	AUDIO_SRC_CONFIG("resample_b", 13, EE_AUDIO_LB_A_CTRL2, 20, 0x1f),
	AUDIO_SRC_CONFIG("vad", 29, EE_AUDIO_LB_A_CTRL2, 20, 0x1f),
	AUDIO_SRC_CONFIG("tdmin_c", 30, EE_AUDIO_LB_A_CTRL2, 20, 0x1f),
	AUDIO_SRC_CONFIG("pdmin", 31, EE_AUDIO_LB_A_CTRL2, 20, 0x1f),
	{ /* sentinel */ }
};

struct mux_conf tdmin_lb_srcs_v1[] = {
	AUDIO_SRC_CONFIG("tdmout_a", 0, EE_AUDIO_TDMIN_LB_CTRL, 20, 0xf),
	AUDIO_SRC_CONFIG("tdmout_b", 1, EE_AUDIO_TDMIN_LB_CTRL, 20, 0xf),
	AUDIO_SRC_CONFIG("tdmout_c", 2, EE_AUDIO_TDMIN_LB_CTRL, 20, 0xf),
	AUDIO_SRC_CONFIG("tdmin_a", 3, EE_AUDIO_TDMIN_LB_CTRL, 20, 0xf),
	AUDIO_SRC_CONFIG("tdmin_b", 4, EE_AUDIO_TDMIN_LB_CTRL, 20, 0xf),
	AUDIO_SRC_CONFIG("tdmin_c", 5, EE_AUDIO_TDMIN_LB_CTRL, 20, 0xf),
	AUDIO_SRC_CONFIG("tdmind_a", 6, EE_AUDIO_TDMIN_LB_CTRL, 20, 0xf),
	AUDIO_SRC_CONFIG("tdmind_b", 7, EE_AUDIO_TDMIN_LB_CTRL, 20, 0xf),
	AUDIO_SRC_CONFIG("tdmind_c", 8, EE_AUDIO_TDMIN_LB_CTRL, 20, 0xf),
	AUDIO_SRC_CONFIG("hdmirx", 9, EE_AUDIO_TDMIN_LB_CTRL, 20, 0xf),
	AUDIO_SRC_CONFIG("acodec_adc", 10, EE_AUDIO_TDMIN_LB_CTRL, 20, 0xf),
	{ /* sentinel */ }
};

struct mux_conf tdmin_lb_srcs_v2[] = {
	AUDIO_SRC_CONFIG("tdmin_a", 0, EE_AUDIO_TDMIN_LB_CTRL, 20, 0xf),
	AUDIO_SRC_CONFIG("tdmin_b", 1, EE_AUDIO_TDMIN_LB_CTRL, 20, 0xf),
	AUDIO_SRC_CONFIG("tdmin_c", 2, EE_AUDIO_TDMIN_LB_CTRL, 20, 0xf),
	AUDIO_SRC_CONFIG("tdmin_d", 3, EE_AUDIO_TDMIN_LB_CTRL, 20, 0xf),
	AUDIO_SRC_CONFIG("tdmout_a", 12, EE_AUDIO_TDMIN_LB_CTRL, 20, 0xf),
	AUDIO_SRC_CONFIG("tdmout_b", 13, EE_AUDIO_TDMIN_LB_CTRL, 20, 0xf),
	AUDIO_SRC_CONFIG("tdmout_c", 14, EE_AUDIO_TDMIN_LB_CTRL, 20, 0xf),
	AUDIO_SRC_CONFIG("tdmout_d", 15, EE_AUDIO_TDMIN_LB_CTRL, 20, 0xf),
	{ /* sentinel */ }
};
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

	/* srcs config to make reg compatible */
	struct mux_conf *srcs;
	struct mux_conf *tdmin_lb_srcs;
	/* from t5 chip, Lb src sel: EE_AUDIO_LB_A_CTRL3 [24:20]
	 * before t5 chips, Lb src sel: EE_AUDIO_LB_A_CTRL1 [0]
	 */
	bool multi_bits_lbsrcs;
	bool use_resamplea;
};

static struct loopback_chipinfo g12a_loopbacka_chipinfo = {
	.id = LOOPBACKA,
	.chnum_en = true,
	.srcs = &lb_srcs_v1[0],
	.tdmin_lb_srcs = &tdmin_lb_srcs_v1[0],
};

#ifndef CONFIG_AMLOGIC_REMOVE_OLD
static struct loopback_chipinfo tl1_loopbacka_chipinfo = {
	.id = LOOPBACKA,
	.ch_ctrl = true,
	.chnum_en = true,
	.srcs = &lb_srcs_v1[0],
	.tdmin_lb_srcs = &tdmin_lb_srcs_v1[0],
};

static struct loopback_chipinfo tl1_loopbackb_chipinfo = {
	.id = LOOPBACKB,
	.ch_ctrl = true,
	.chnum_en = true,
	.srcs = &lb_srcs_v1[0],
	.tdmin_lb_srcs = &tdmin_lb_srcs_v1[0],
};
#endif

static struct loopback_chipinfo sm1_loopbacka_chipinfo = {
	.id = LOOPBACKA,
	.ch_ctrl = true,
	.chnum_en = true,
	.srcs = &lb_srcs_v1[0],
	.tdmin_lb_srcs = &tdmin_lb_srcs_v1[0],
};

static struct loopback_chipinfo sm1_loopbackb_chipinfo = {
	.id = LOOPBACKB,
	.ch_ctrl = true,
	.chnum_en = true,
	.srcs = &lb_srcs_v1[0],
	.tdmin_lb_srcs = &tdmin_lb_srcs_v1[0],
};

static struct loopback_chipinfo tm2_loopbacka_chipinfo = {
	.id = LOOPBACKA,
	.ch_ctrl = true,
	.chnum_en = true,
	.srcs  = &lb_srcs_v1[0],
	.tdmin_lb_srcs = &tdmin_lb_srcs_v1[0],
};

static struct loopback_chipinfo tm2_loopbackb_chipinfo = {
	.id = LOOPBACKB,
	.ch_ctrl = true,
	.chnum_en = true,
	.srcs = &lb_srcs_v1[0],
	.tdmin_lb_srcs = &tdmin_lb_srcs_v1[0],
};

static struct loopback_chipinfo tm2_revb_loopbacka_chipinfo = {
	.id = LOOPBACKA,
	.ch_ctrl = true,
	.chnum_en = false,
	.srcs = &lb_srcs_v1[0],
	.tdmin_lb_srcs = &tdmin_lb_srcs_v1[0],
};

static struct loopback_chipinfo tm2_revb_loopbackb_chipinfo = {
	.id = LOOPBACKB,
	.ch_ctrl = true,
	.chnum_en = false,
	.srcs = &lb_srcs_v1[0],
	.tdmin_lb_srcs = &tdmin_lb_srcs_v1[0],
};

static struct loopback_chipinfo t5_loopbacka_chipinfo = {
	.id = LOOPBACKA,
	.ch_ctrl = true,
	.chnum_en = false,
	.srcs = &lb_srcs_v2[0],
	.tdmin_lb_srcs = &tdmin_lb_srcs_v2[0],
	.multi_bits_lbsrcs = true,
};

static struct loopback_chipinfo p1_loopbacka_chipinfo = {
	.id = LOOPBACKA,
	.ch_ctrl = true,
	.chnum_en = false,
	.srcs = &lb_srcs_v2[0],
	.tdmin_lb_srcs = &tdmin_lb_srcs_v2[0],
	.multi_bits_lbsrcs = true,
};

static struct loopback_chipinfo p1_loopbackb_chipinfo = {
	.id = LOOPBACKB,
	.ch_ctrl = true,
	.chnum_en = false,
	.srcs = &lb_srcs_v2[0],
	.tdmin_lb_srcs = &tdmin_lb_srcs_v2[0],
	.multi_bits_lbsrcs = true,
};

static struct loopback_chipinfo a5_loopbacka_chipinfo = {
	.id = LOOPBACKA,
	.ch_ctrl = true,
	.chnum_en = false,
	.srcs = &lb_srcs_v3[0],
	.tdmin_lb_srcs = &tdmin_lb_srcs_v2[0],
	.multi_bits_lbsrcs = true,
	.use_resamplea = true,
};

static const struct of_device_id loopback_device_id[] = {
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
	{
		.compatible = "amlogic, snd-loopback",
		.data = &g12a_loopbacka_chipinfo,
	},
	{
		.compatible = "amlogic, axg-loopback",
		.data = &g12a_loopbacka_chipinfo,
	},
	{
		.compatible = "amlogic, tl1-loopbacka",
		.data = &tl1_loopbacka_chipinfo,
	},
	{
		.compatible = "amlogic, tl1-loopbackb",
		.data = &tl1_loopbackb_chipinfo,
	},
#endif
	{
		.compatible = "amlogic, g12a-loopback",
		.data = &g12a_loopbacka_chipinfo,
	},
	{
		.compatible = "amlogic, sm1-loopbacka",
		.data = &sm1_loopbacka_chipinfo,
	},
	{
		.compatible = "amlogic, sm1-loopbackb",
		.data = &sm1_loopbackb_chipinfo,
	},
	{
		.compatible = "amlogic, tm2-loopbacka",
		.data = &tm2_loopbacka_chipinfo,
	},
	{
		.compatible = "amlogic, tm2-loopbackb",
		.data = &tm2_loopbackb_chipinfo,
	},
	{
		.compatible = "amlogic, tm2-revb-loopbacka",
		.data = &tm2_revb_loopbacka_chipinfo,
	},
	{
		.compatible = "amlogic, tm2-revb-loopbackb",
		.data = &tm2_revb_loopbackb_chipinfo,
	},
	{
		.compatible = "amlogic, t5-loopbacka",
		.data = &t5_loopbacka_chipinfo,
	},
	{
		.compatible = "amlogic, p1-loopbacka",
		.data = &p1_loopbacka_chipinfo,
	},
	{
		.compatible = "amlogic, p1-loopbackb",
		.data = &p1_loopbackb_chipinfo,
	},
	{
		.compatible = "amlogic, a5-loopbacka",
		.data = &a5_loopbacka_chipinfo,
	},
	{
		.compatible = "amlogic, axg-loopback",
		.data = &g12a_loopbacka_chipinfo,
	},

	{}
};
MODULE_DEVICE_TABLE(of, loopback_device_id);
