/*
 * Amlogic Display Module Driver
 *
 * Copyright (C) 2015 Amlogic, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *
 */


#ifndef __ENC_CLK_CONFIG_H__
#define __ENC_CLK_CONFIG_H__

enum viu_type_e {
	VIU_ENCL = 0,
	VIU_ENCI,
	VIU_ENCP,
	VIU_ENCT,
};

struct enc_clk_val_s {
	enum vmode_e mode;
	unsigned hpll_clk_out;
	unsigned hpll_hdmi_od;
	/* #if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON8 */
	unsigned hpll_lvds_od;
	/* #endif */
	unsigned viu_path;
	enum viu_type_e viu_type;
	unsigned vid_pll_div;
	unsigned clk_final_div;
	unsigned hdmi_tx_pixel_div;
	unsigned encp_div;
	unsigned enci_div;
	unsigned enct_div;
	unsigned encl_div;
	unsigned vdac0_div;
	unsigned vdac1_div;
	unsigned unused;    /* prevent compile error */
};

extern void set_enci_clk(unsigned clk);
extern void set_encp_clk(unsigned clk);
extern void set_vmode_clk(enum vmode_e mode);

#endif
