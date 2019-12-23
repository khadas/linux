/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __ENC_CLK_CONFIG_H__
#define __ENC_CLK_CONFIG_H__

enum Viu_Type {
	VIU_ENCL = 0, VIU_ENCI, VIU_ENCP, VIU_ENCT,
};

int set_viu_path(unsigned int viu_channel_sel,
		 enum Viu_Type viu_type_sel);
void set_enci_clk(unsigned int clk);
void set_encp_clk(unsigned int clk);
/* extern void set_vmode_clk(vmode_t mode); */

struct enc_clk_val {
	enum vmode_e mode;
	unsigned int hpll_clk_out;
	unsigned int hpll_hdmi_od;
	unsigned int hpll_lvds_od;
	unsigned int viu_path;
	enum Viu_Type viu_type;
	unsigned int vid_pll_div;
	unsigned int clk_final_div;
	unsigned int hdmi_tx_pixel_div;
	unsigned int encp_div;
	unsigned int enci_div;
	unsigned int enct_div;
	unsigned int encl_div;
	unsigned int vdac0_div;
	unsigned int vdac1_div;
	unsigned int unused; /* prevent compile error */
};

#endif
