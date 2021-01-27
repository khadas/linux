/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __ENC_CLK_CONFIG_H__
#define __ENC_CLK_CONFIG_H__

enum Viu_Type {
	VIU_ENCL = 0, VIU_ENCI, VIU_ENCP, VIU_ENCT,
};

int set_viu_path(u32 viu_channel_sel,
		 enum Viu_Type viu_type_sel);
void set_enci_clk(u32 clk);
void set_encp_clk(u32 clk);
/* extern void set_vmode_clk(vmode_t mode); */

struct enc_clk_val {
	enum vmode_e mode;
	u32 hpll_clk_out;
	u32 hpll_hdmi_od;
	u32 hpll_lvds_od;
	u32 viu_path;
	enum Viu_Type viu_type;
	u32 vid_pll_div;
	u32 clk_final_div;
	u32 hdmi_tx_pixel_div;
	u32 encp_div;
	u32 enci_div;
	u32 enct_div;
	u32 encl_div;
	u32 vdac0_div;
	u32 vdac1_div;
	u32 unused; /* prevent compile error */
};

#endif
