// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#undef pr_fmt
#define pr_fmt(fmt) "a1_audio_clkc: " fmt

#include <dt-bindings/clock/amlogic,a1-audio-clk.h>

#include "audio_clks.h"
#include "../regs.h"

static const char *const mclk_parent_names[] = {
	"xtal_dds", "fclk_div2", "fclk_div3", "hifi_pll", "xtal"};

static const char *const audioclk_parent_names[] = {
	"mclk_a", "mclk_b", "mclk_c", "mclk_d",
	"i_slv_sclk_a", "i_slv_sclk_b", "i_slv_sclk_c",
	"i_slv_sclk_d", "i_slv_sclk_e", "i_slv_sclk_f", "i_slv_sclk_g",
	"i_slv_sclk_h", "i_slv_sclk_i", "i_slv_sclk_j"};

CLOCK_GATE(audio_ddr_arb, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN0), 0, sys_clk);
CLOCK_GATE(audio_tdmina, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN0), 1, sys_clk);
CLOCK_GATE(audio_tdminb, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN0), 2, sys_clk);
CLOCK_GATE(audio_tdminlb, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN0), 3, sys_clk);
CLOCK_GATE
	(audio_loopbacka,
	AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN0),
	4, sys_clk);
CLOCK_GATE(audio_tdmouta, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN0), 5, sys_clk);
CLOCK_GATE(audio_tdmoutb, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN0), 6, sys_clk);
CLOCK_GATE(audio_frddra, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN0), 7, sys_clk);
CLOCK_GATE(audio_frddrb, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN0), 8, sys_clk);
CLOCK_GATE(audio_toddra, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN0), 9, sys_clk);
CLOCK_GATE(audio_toddrb, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN0), 10, sys_clk);
CLOCK_GATE(audio_spdifin, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN0), 11, sys_clk);
CLOCK_GATE
	(audio_resamplea,
	AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN0),
	12, sys_clk);
CLOCK_GATE(audio_eqdrc, AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN0), 13, sys_clk);
CLOCK_GATE
	(audio_audiolocker,
	AUD_ADDR_OFFSET(EE_AUDIO_CLK_GATE_EN0),
	14, sys_clk);

CLOCK_GATE(audio2_ddr_arb, AUD_ADDR_OFFSET(EE_AUDIO2_CLK_GATE_EN0), 0, sys_clk);
CLOCK_GATE(audio2_pdm, AUD_ADDR_OFFSET(EE_AUDIO2_CLK_GATE_EN0), 1, sys_clk);
CLOCK_GATE
	(audio2_tdmin_vad,
	AUD_ADDR_OFFSET(EE_AUDIO2_CLK_GATE_EN0),
	2, sys_clk);
CLOCK_GATE
	(audio2_toddr_vad,
	AUD_ADDR_OFFSET(EE_AUDIO2_CLK_GATE_EN0),
	3, sys_clk);
CLOCK_GATE(audio2_tovad, AUD_ADDR_OFFSET(EE_AUDIO2_CLK_GATE_EN0), 4, sys_clk);
CLOCK_GATE
	(audio2_toaudiotop,
	AUD_ADDR_OFFSET(EE_AUDIO2_CLK_GATE_EN0),
	7, sys_clk);

static struct clk_gate *a1_audio_clk_gates[] = {
	&audio_ddr_arb,
	&audio_tdmina,
	&audio_tdminb,
	&audio_tdminlb,
	&audio_loopbacka,
	&audio_tdmouta,
	&audio_tdmoutb,
	&audio_frddra,
	&audio_frddrb,
	&audio_toddra,
	&audio_toddrb,
	&audio_spdifin,
	&audio_resamplea,
	&audio_eqdrc,
	&audio_audiolocker,

};

static struct clk_gate *a1_audio2_clk_gates[] = {
	&audio2_ddr_arb,
	&audio2_pdm,
	&audio2_tdmin_vad,
	&audio2_toddr_vad,
	&audio2_tovad,
	&audio2_toaudiotop,
};

/* Array of all clocks provided by this provider */
static struct clk_hw *a1_audio_clk_hws[] = {
	[CLKID_AUDIO_GATE_DDR_ARB]     = &audio_ddr_arb.hw,
	[CLKID_AUDIO_GATE_TDMINA]      = &audio_tdmina.hw,
	[CLKID_AUDIO_GATE_TDMINB]      = &audio_tdminb.hw,
	[CLKID_AUDIO_GATE_TDMINLB]     = &audio_tdminlb.hw,
	[CLKID_AUDIO_GATE_LOOPBACK]    = &audio_loopbacka.hw,
	[CLKID_AUDIO_GATE_TDMOUTA]     = &audio_tdmouta.hw,
	[CLKID_AUDIO_GATE_TDMOUTB]     = &audio_tdmoutb.hw,
	[CLKID_AUDIO_GATE_FRDDRA]      = &audio_frddra.hw,
	[CLKID_AUDIO_GATE_FRDDRB]      = &audio_frddrb.hw,
	[CLKID_AUDIO_GATE_TODDRA]      = &audio_toddra.hw,
	[CLKID_AUDIO_GATE_TODDRB]      = &audio_toddrb.hw,
	[CLKID_AUDIO_GATE_SPDIFIN]     = &audio_spdifin.hw,
	[CLKID_AUDIO_GATE_RESAMPLEA]   = &audio_resamplea.hw,
	[CLKID_AUDIO_GATE_EQDRC]       = &audio_eqdrc.hw,
	[CLKID_AUDIO_GATE_LOCKER]      = &audio_audiolocker.hw,

	[CLKID_AUDIO2_GATE_DDR_ARB]    = &audio2_ddr_arb.hw,
	[CLKID_AUDIO2_GATE_PDM]        = &audio2_pdm.hw,
	[CLKID_AUDIO2_GATE_TDMIN_VAD]  = &audio2_tdmin_vad.hw,
	[CLKID_AUDIO2_GATE_TODDR_VAD]  = &audio2_toddr_vad.hw,
	[CLKID_AUDIO2_GATE_TOVAD]      = &audio2_tovad.hw,
	[CLKID_AUDIO2_GATE_TOAUDIOTOP] = &audio2_toaudiotop.hw,
};

static int a1_clk_gates_init(struct clk **clks, void __iomem *iobase)
{
	int clkid;

	if (ARRAY_SIZE(a1_audio_clk_gates) != CLKID_AUDIO_GATE_MAX) {
		pr_err("check clk gates number\n");
		return -EINVAL;
	}

	for (clkid = 0; clkid < CLKID_AUDIO_GATE_MAX; clkid++) {
		a1_audio_clk_gates[clkid]->reg = iobase;
		clks[clkid] = clk_register(NULL, a1_audio_clk_hws[clkid]);
		WARN_ON(IS_ERR_OR_NULL(clks[clkid]));
	}

	return 0;
}

static int a1_clk2_gates_init(struct clk **clks, void __iomem *iobase)
{
	int clkid;

	if (ARRAY_SIZE(a1_audio2_clk_gates)
		!= CLKID_AUDIO2_GATE_MAX - CLKID_AUDIO_GATE_MAX) {
		pr_err("check clk2 gates number\n");
		return -EINVAL;
	}

	for (clkid = CLKID_AUDIO_GATE_MAX;
		clkid < CLKID_AUDIO2_GATE_MAX; clkid++) {
		a1_audio2_clk_gates[clkid - CLKID_AUDIO_GATE_MAX]->reg = iobase;
		clks[clkid] = clk_register(NULL, a1_audio_clk_hws[clkid]);
		WARN_ON(IS_ERR_OR_NULL(clks[clkid]));
	}

	return 0;
}

/* mclk_a */
CLOCK_COM_MUX(mclk_a, AUD_ADDR_OFFSET(EE_AUDIO_MCLK_A_CTRL(1)), 0x7, 24);
CLOCK_COM_DIV(mclk_a, AUD_ADDR_OFFSET(EE_AUDIO_MCLK_A_CTRL(1)), 0, 16);
CLOCK_COM_GATE(mclk_a, AUD_ADDR_OFFSET(EE_AUDIO_MCLK_A_CTRL(1)), 31);
/* mclk_b */
CLOCK_COM_MUX(mclk_b, AUD_ADDR_OFFSET(EE_AUDIO_MCLK_B_CTRL(1)), 0x7, 24);
CLOCK_COM_DIV(mclk_b, AUD_ADDR_OFFSET(EE_AUDIO_MCLK_B_CTRL(1)), 0, 16);
CLOCK_COM_GATE(mclk_b, AUD_ADDR_OFFSET(EE_AUDIO_MCLK_B_CTRL(1)), 31);
/* mclk_c */
CLOCK_COM_MUX(mclk_c, AUD_ADDR_OFFSET(EE_AUDIO_MCLK_C_CTRL(1)), 0x7, 24);
CLOCK_COM_DIV(mclk_c, AUD_ADDR_OFFSET(EE_AUDIO_MCLK_C_CTRL(1)), 0, 16);
CLOCK_COM_GATE(mclk_c, AUD_ADDR_OFFSET(EE_AUDIO_MCLK_C_CTRL(1)), 31);
/* mclk_d */
CLOCK_COM_MUX(mclk_d, AUD_ADDR_OFFSET(EE_AUDIO_MCLK_D_CTRL(1)), 0x7, 24);
CLOCK_COM_DIV(mclk_d, AUD_ADDR_OFFSET(EE_AUDIO_MCLK_D_CTRL(1)), 0, 16);
CLOCK_COM_GATE(mclk_d, AUD_ADDR_OFFSET(EE_AUDIO_MCLK_D_CTRL(1)), 31);
/* spdifin */
CLOCK_COM_MUX(spdifin, AUD_ADDR_OFFSET(EE_AUDIO_CLK_SPDIFIN_CTRL), 0x7, 24);
CLOCK_COM_DIV(spdifin, AUD_ADDR_OFFSET(EE_AUDIO_CLK_SPDIFIN_CTRL), 0, 8);
CLOCK_COM_GATE(spdifin, AUD_ADDR_OFFSET(EE_AUDIO_CLK_SPDIFIN_CTRL), 31);
/* audio resample_a */
CLOCK_COM_MUX(resample_a,
	      AUD_ADDR_OFFSET(EE_AUDIO_CLK_RESAMPLEA_CTRL), 0xf, 24);
CLOCK_COM_DIV(resample_a, AUD_ADDR_OFFSET(EE_AUDIO_CLK_RESAMPLEA_CTRL), 0, 8);
CLOCK_COM_GATE(resample_a, AUD_ADDR_OFFSET(EE_AUDIO_CLK_RESAMPLEA_CTRL), 31);
/* audio locker_out */
CLOCK_COM_MUX(locker_out, AUD_ADDR_OFFSET(EE_AUDIO_CLK_LOCKER_CTRL), 0xf, 24);
CLOCK_COM_DIV(locker_out, AUD_ADDR_OFFSET(EE_AUDIO_CLK_LOCKER_CTRL), 16, 8);
CLOCK_COM_GATE(locker_out, AUD_ADDR_OFFSET(EE_AUDIO_CLK_LOCKER_CTRL), 31);
/* audio locker_in */
CLOCK_COM_MUX(locker_in, AUD_ADDR_OFFSET(EE_AUDIO_CLK_LOCKER_CTRL), 0xf, 8);
CLOCK_COM_DIV(locker_in, AUD_ADDR_OFFSET(EE_AUDIO_CLK_LOCKER_CTRL), 0, 8);
CLOCK_COM_GATE(locker_in, AUD_ADDR_OFFSET(EE_AUDIO_CLK_LOCKER_CTRL), 15);
/* audio eqdrc  */
CLOCK_COM_MUX(eqdrc, AUD_ADDR_OFFSET(EE_AUDIO_CLK_EQDRC_CTRL0), 0x7, 24);
CLOCK_COM_DIV(eqdrc, AUD_ADDR_OFFSET(EE_AUDIO_CLK_EQDRC_CTRL0), 0, 16);
CLOCK_COM_GATE(eqdrc, AUD_ADDR_OFFSET(EE_AUDIO_CLK_EQDRC_CTRL0), 31);

/* mclk_vad */
CLOCK_COM_MUX(mclk_vad, AUD_ADDR_OFFSET(EE_AUDIO2_MCLK_VAD_CTRL), 0x7, 24);
CLOCK_COM_DIV(mclk_vad, AUD_ADDR_OFFSET(EE_AUDIO2_MCLK_VAD_CTRL), 0, 16);
CLOCK_COM_GATE(mclk_vad, AUD_ADDR_OFFSET(EE_AUDIO2_MCLK_VAD_CTRL), 31);

/* vad clk */
CLOCK_COM_MUX(vad, AUD_ADDR_OFFSET(EE_AUDIO2_CLK_VAD_CTRL), 0x7, 24);
CLOCK_COM_DIV(vad, AUD_ADDR_OFFSET(EE_AUDIO2_CLK_VAD_CTRL), 0, 16);
CLOCK_COM_GATE(vad, AUD_ADDR_OFFSET(EE_AUDIO2_CLK_VAD_CTRL), 31);

/* pdm dclk */
CLOCK_COM_MUX(pdmdclk, AUD_ADDR_OFFSET(EE_AUDIO2_CLK_PDMIN_CTRL0), 0x7, 24);
CLOCK_COM_DIV(pdmdclk, AUD_ADDR_OFFSET(EE_AUDIO2_CLK_PDMIN_CTRL0), 0, 16);
CLOCK_COM_GATE(pdmdclk, AUD_ADDR_OFFSET(EE_AUDIO2_CLK_PDMIN_CTRL0), 31);
/* pdmin1 */
CLOCK_COM_MUX(pdmsysclk, AUD_ADDR_OFFSET(EE_AUDIO2_CLK_PDMIN_CTRL1), 0x7, 24);
CLOCK_COM_DIV(pdmsysclk, AUD_ADDR_OFFSET(EE_AUDIO2_CLK_PDMIN_CTRL1), 0, 16);
CLOCK_COM_GATE(pdmsysclk, AUD_ADDR_OFFSET(EE_AUDIO2_CLK_PDMIN_CTRL1), 31);

static int a1_clks_init(struct clk **clks, void __iomem *iobase)
{
	IOMAP_COM_CLK(mclk_a, iobase);
	clks[CLKID_AUDIO_MCLK_A] = REGISTER_CLK_COM(mclk_a);
	WARN_ON(IS_ERR_OR_NULL(clks[CLKID_AUDIO_MCLK_A]));

	IOMAP_COM_CLK(mclk_b, iobase);
	clks[CLKID_AUDIO_MCLK_B] = REGISTER_CLK_COM(mclk_b);
	WARN_ON(IS_ERR_OR_NULL(clks[CLKID_AUDIO_MCLK_B]));

	IOMAP_COM_CLK(mclk_c, iobase);
	clks[CLKID_AUDIO_MCLK_C] = REGISTER_CLK_COM(mclk_c);
	WARN_ON(IS_ERR_OR_NULL(clks[CLKID_AUDIO_MCLK_C]));

	IOMAP_COM_CLK(mclk_d, iobase);
	clks[CLKID_AUDIO_MCLK_D] = REGISTER_CLK_COM(mclk_d);
	WARN_ON(IS_ERR_OR_NULL(clks[CLKID_AUDIO_MCLK_D]));

	IOMAP_COM_CLK(spdifin, iobase);
	clks[CLKID_AUDIO_SPDIFIN] = REGISTER_CLK_COM(spdifin);
	WARN_ON(IS_ERR_OR_NULL(clks[CLKID_AUDIO_SPDIFIN]));

	IOMAP_COM_CLK(resample_a, iobase);
	clks[CLKID_AUDIO_RESAMPLEA] = REGISTER_AUDIOCLK_COM(resample_a);
	WARN_ON(IS_ERR_OR_NULL(clks[CLKID_AUDIO_RESAMPLEA]));

	IOMAP_COM_CLK(locker_out, iobase);
	clks[CLKID_AUDIO_LOCKER_OUT] = REGISTER_AUDIOCLK_COM(locker_out);
	WARN_ON(IS_ERR_OR_NULL(clks[CLKID_AUDIO_LOCKER_OUT]));

	IOMAP_COM_CLK(locker_in, iobase);
	clks[CLKID_AUDIO_LOCKER_IN] = REGISTER_AUDIOCLK_COM(locker_in);
	WARN_ON(IS_ERR_OR_NULL(clks[CLKID_AUDIO_LOCKER_IN]));

	IOMAP_COM_CLK(eqdrc, iobase);
	clks[CLKID_AUDIO_EQDRC] = REGISTER_CLK_COM(eqdrc);
	WARN_ON(IS_ERR_OR_NULL(clks[CLKID_AUDIO_EQDRC]));

	return 0;
}

static int a1_clks2_init(struct clk **clks, void __iomem *iobase)
{
	IOMAP_COM_CLK(mclk_vad, iobase);
	clks[CLKID_AUDIO2_MCLK_VAD] = REGISTER_CLK_COM(mclk_vad);
	WARN_ON(IS_ERR_OR_NULL(clks[CLKID_AUDIO2_MCLK_VAD]));

	IOMAP_COM_CLK(vad, iobase);
	clks[CLKID_AUDIO2_VAD_CLK] = REGISTER_CLK_COM(vad);
	WARN_ON(IS_ERR_OR_NULL(clks[CLKID_AUDIO2_VAD_CLK]));

	IOMAP_COM_CLK(pdmdclk, iobase);
	clks[CLKID_AUDIO2_PDM_DCLK] = REGISTER_CLK_COM(pdmdclk);
	WARN_ON(IS_ERR_OR_NULL(clks[CLKID_AUDIO2_PDM_DCLK]));

	IOMAP_COM_CLK(pdmsysclk, iobase);
	clks[CLKID_AUDIO2_PDM_SYSCLK] = REGISTER_CLK_COM(pdmsysclk);
	WARN_ON(IS_ERR_OR_NULL(clks[CLKID_AUDIO2_PDM_SYSCLK]));

	return 0;
}

struct audio_clk_init a1_audio_clks_init = {
	.clk_num	= NUM_AUDIO_CLKS,
	.clk_gates	= a1_clk_gates_init,
	.clks		= a1_clks_init,
	.clk2_gates	= a1_clk2_gates_init,
	.clks2		= a1_clks2_init,
};

