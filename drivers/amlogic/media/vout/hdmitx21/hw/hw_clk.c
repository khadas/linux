// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/printk.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/amlogic/media/vout/hdmi_tx21/hdmi_tx_module.h>
#include "common.h"

#ifdef CONFIG_AMLOGIC_VPU
#include <linux/amlogic/media/vpu/vpu.h>
#endif

/* local frac_rate flag */
static u32 frac_rate;
static void set_crt_video_enc(u32 vidx, u32 in_sel, u32 div_n);
static void set_crt_video_enc2(u32 vidx, u32 in_sel, u32 div_n);
/*
 * HDMITX Clock configuration
 */
static inline int check_div(u32 div)
{
	if (div == -1)
		return -1;
	switch (div) {
	case 1:
		div = 0;
		break;
	case 2:
		div = 1;
		break;
	case 4:
		div = 2;
		break;
	case 6:
		div = 3;
		break;
	case 12:
		div = 4;
		break;
	default:
		break;
	}
	return div;
}

static void hdmitx_disable_encp_clk(struct hdmitx_dev *hdev)
{
	//hd21_set_reg_bits(CLKCTRL_VID_CLK_CTRL2, 0, 2, 1);
}

static void hdmitx_enable_encp_clk(struct hdmitx_dev *hdev)
{
	//hd21_set_reg_bits(CLKCTRL_VID_CLK_CTRL2, 1, 2, 1);
}

static void hdmitx_disable_enci_clk(struct hdmitx_dev *hdev)
{
	//hd21_set_reg_bits(CLKCTRL_VID_CLK_CTRL2, 0, 0, 1);

	if (hdev->hdmitx_clk_tree.venci_top_gate)
		clk_disable_unprepare(hdev->hdmitx_clk_tree.venci_top_gate);

	if (hdev->hdmitx_clk_tree.venci_0_gate)
		clk_disable_unprepare(hdev->hdmitx_clk_tree.venci_0_gate);

	if (hdev->hdmitx_clk_tree.venci_1_gate)
		clk_disable_unprepare(hdev->hdmitx_clk_tree.venci_1_gate);
}

static void hdmitx_disable_tx_pixel_clk(struct hdmitx_dev *hdev)
{
	//hd21_set_reg_bits(CLKCTRL_VID_CLK_CTRL2, 0, 5, 1);
}

void hdmitx21_set_audioclk(u8 hdmitx_aud_clk_div)
{
	u32 data32;

	if (hdmitx_aud_clk_div == 0)
		hdmitx_aud_clk_div = 1;
	// Enable hdmitx_aud_clk
	// [10: 9] clk_sel for cts_hdmitx_aud_clk: 2=fclk_div3
	// [    8] clk_en for cts_hdmitx_aud_clk
	// [ 6: 0] clk_div for cts_hdmitx_aud_clk: fclk_div3/aud_clk_div
	data32 = 0;
	data32 |= (2 << 9);
	data32 |= (0 << 8);
	data32 |= ((hdmitx_aud_clk_div - 1 + 1 + 16) << 0);	//500->511
	hd21_set_reg_bits(CLKCTRL_HTX_CLK_CTRL1, data32, 0, 12);
	// [    8] clk_en for cts_hdmitx_aud_clk
	hd21_set_reg_bits(CLKCTRL_HTX_CLK_CTRL1, 1, 8, 1);
}

void hdmitx21_set_default_clk(void)
{
	u32 data32;

	// Enable clk81_hdmitx_pclk
	hd21_set_reg_bits(CLKCTRL_SYS_CLK_EN0_REG2, 1, 4, 1);

	// Enable fixed hdmitx_sys_clk
	data32 = 0;
	data32 |= (3 << 9); // [10: 9] clk_sel for cts_hdmitx_sys_clk: 3=fclk_div5
	data32 |= (0 << 8); // [    8] clk_en for cts_hdmitx_sys_clk
	data32 |= (1 << 0); // [ 6: 0] clk_div for cts_hdmitx_sys_clk: fclk_dvi5/2=400/2=200M
	hd21_write_reg(CLKCTRL_HDMI_CLK_CTRL, data32);
	data32 |= (1 << 8); // [    8] clk_en for cts_hdmitx_sys_clk
	hd21_write_reg(CLKCTRL_HDMI_CLK_CTRL, data32);

	// Enable fixed hdmitx_prif_clk, hdmitx_200m_clk
	data32 = 0;
	data32 |= (3 << 25); // [26:25] clk_sel for cts_hdmitx_200m_clk: 3=fclk_div5
	data32 |= (0 << 24); // [   24] clk_en for cts_hdmitx_200m_clk
	data32 |= (1 << 16); // [22:16] clk_div for cts_hdmitx_200m_clk: fclk_dvi5/16=400/16=25M
	data32 |= (3 << 9); // [10: 9] clk_sel for cts_hdmitx_prif_clk: 3=fclk_div5
	data32 |= (0 << 8); // [    8] clk_en for cts_hdmitx_prif_clk
	data32 |= (1 << 0); // [ 6: 0] clk_div for cts_hdmitx_prif_clk: fclk_dvi5/2=400/2=200M
	hd21_write_reg(CLKCTRL_HTX_CLK_CTRL0, data32);
	data32 |= (1 << 24); // [   24] clk_en for cts_hdmitx_200m_clk
	data32 |= (1 << 8); // [    8] clk_en for cts_hdmitx_prif_clk
	hd21_write_reg(CLKCTRL_HTX_CLK_CTRL0, data32);

	//hd21_set_reg_bits(CLKCTRL_VID_CLK0_CTRL, 0, 0, 5);

	// wire    wr_enable = control[3];
	// wire    fifo_enable = control[2];
	// assign  phy_clk_en = control[1];
	hd21_set_reg_bits(ANACTRL_HDMIPHY_CTRL1, 1, 1, 1); // Enable tmds_clk
	hd21_set_reg_bits(ANACTRL_HDMIPHY_CTRL1, 1, 2, 1); // Enable the decoupling FIFO
	// Enable enable the write/read decoupling state machine
	hd21_set_reg_bits(ANACTRL_HDMIPHY_CTRL1, 1, 3, 1);
	// Bring HDMITX MEM output of power down
	hd21_set_reg_bits(PWRCTRL_MEM_PD11, 0, 8, 8);
	// Bring out of reset
	hdmitx21_wr_reg(HDMITX_TOP_SW_RESET, 0);
	// Test after initial out of reset, cannot write to IP register, unless enable access
	hdmitx21_wr_reg(INTR3_MASK_IVCTX, 0xff);
	hdmitx21_wr_reg(HDMITX_TOP_SEC_SCRATCH, 1);
}

void hdmitx21_set_cts_hdcp22_clk(struct hdmitx_dev *hdev)
{
	//hd21_write_reg(CLKCTRL_HDCP22_CLK_CTRL, 0x01000100);
}

void hdmitx21_set_hdcp_pclk(struct hdmitx_dev *hdev)
{
	/* top hdcp pixel clock */
	hd21_set_reg_bits(CLKCTRL_SYS_CLK_EN0_REG2, 1, 3, 1);
}

static void set_hpll_clk_out(u32 clk)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	pr_info("config HPLL = %d frac_rate = %d\n", clk, frac_rate);

	switch (hdev->data->chip_type) {
	case MESON_CPU_ID_T7:
		set21_t7_hpll_clk_out(frac_rate, clk);
		break;
	case MESON_CPU_ID_S5:
		pr_info("%s%d***config s5 hpll***\n", __func__, __LINE__);
		switch (hdev->frl_rate) {
		case FRL_NONE: /* for legacy modes */
			set21_s5_hpll_clk_out(frac_rate, clk);
			break;
		case FRL_3G3L:
		case FRL_6G3L:
		case FRL_6G4L:
		case FRL_12G4L:
			set21_s5_hpll_clk_out(frac_rate, 6000000);
			break;
		case FRL_8G4L:
			set21_s5_hpll_clk_out(frac_rate, 4000000);
			break;
		case FRL_10G4L:
			set21_s5_hpll_clk_out(frac_rate, 5000000);
			break;
		default:
			pr_info("not support rate: %d\n", hdev->frl_rate);
			break;
		}
		break;
	default:
		pr_info("%s%dNot match chip ID\n", __func__, __LINE__);
		break;
	}

	pr_info("config HPLL done\n");
}

/* HERE MUST BE BIT OPERATION!!! */
static void set_hpll_sspll(enum hdmi_vic vic)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	switch (hdev->data->chip_type) {
	case MESON_CPU_ID_T7:
		set21_hpll_sspll_t7(vic);
		break;
	case MESON_CPU_ID_S5:
		set21_hpll_sspll_s5(vic);
		break;
	default:
		pr_info("%s%dNot match chip ID\n", __func__, __LINE__);
		break;
	}
}

static void set_hpll_od0(u32 div)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	switch (hdev->data->chip_type) {
	case MESON_CPU_ID_T7:
		set21_hpll_od0_t7(div);
		break;
	case MESON_CPU_ID_S5:
		set21_txpll_3_od0_s5(div);
		break;
	default:
		pr_info("%s%dNot match chip ID\n", __func__, __LINE__);
		break;
	}
}

static void set_hpll_od1(u32 div)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	switch (hdev->data->chip_type) {
	case MESON_CPU_ID_T7:
		set21_hpll_od1_t7(div);
		break;
	case MESON_CPU_ID_S5:
		set21_txpll_3_od1_s5(div);
		break;
	default:
		pr_info("%s%dNot match chip ID\n", __func__, __LINE__);
		break;
	}
}

static void set_hpll_od2(u32 div)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	switch (hdev->data->chip_type) {
	case MESON_CPU_ID_T7:
		set21_hpll_od2_t7(div);
		break;
	case MESON_CPU_ID_S5:
		set21_txpll_3_od2_s5(div);
		break;
	default:
		pr_info("%s%dNot match chip ID\n", __func__, __LINE__);
		break;
	}
}

static void set_hpll_od3(u32 div)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	switch (hdev->data->chip_type) {
	case MESON_CPU_ID_T7:
		//set21_hpll_od3_t7(div);
		break;
	case MESON_CPU_ID_S5:
		set21_txpll_4_od_s5(div);
		break;
	default:
		pr_info("%s%dNot match chip ID\n", __func__, __LINE__);
		break;
	}
}

/* --------------------------------------------------
 *              clocks_set_vid_clk_div
 * --------------------------------------------------
 * wire            clk_final_en    = control[19];
 * wire            clk_div1        = control[18];
 * wire    [1:0]   clk_sel         = control[17:16];
 * wire            set_preset      = control[15];
 * wire    [14:0]  shift_preset    = control[14:0];
 */
static void clocks_set_vid_clk_div_for_hdmi(int div_sel)
{
	int shift_val = 0;
	int shift_sel = 0;
	u32 reg_vid_pll = CLKCTRL_HDMI_VID_PLL_CLK_DIV;
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	pr_info("%s[%d] div = %d\n", __func__, __LINE__, div_sel);

	/* Disable the output clock */
	if (hdev->data->chip_type == MESON_CPU_ID_S5) {
		pr_info("%s[%d] s5 hadr code \n", __func__, __LINE__);
		hd21_write_reg(reg_vid_pll, 0);
		hd21_write_reg(reg_vid_pll, 0x1000000);
		hd21_write_reg(reg_vid_pll, 0x1020000);
		hd21_write_reg(reg_vid_pll, 0x1028000);
		hd21_write_reg(reg_vid_pll, 0x102f39c);
		hd21_write_reg(reg_vid_pll, 0x102739c);
		hd21_write_reg(reg_vid_pll, 0x10a739c);
		hd21_write_reg(CLKCTRL_HDMI_PLL_TMDS_CLK_DIV, 0x010af39c);	//hard code
		hd21_write_reg(CLKCTRL_HDMI_PLL_TMDS_CLK_DIV, 0x010a739c);
		return;
	}
	hd21_set_reg_bits(reg_vid_pll, 0, 18, 2);
	hd21_set_reg_bits(reg_vid_pll, 0, 15, 1);
	if (hdev->enc_idx == 2)
		hd21_set_reg_bits(reg_vid_pll, 1, 25, 1); /* vid_pll2_clk_sel_hdmi */
	else
		hd21_set_reg_bits(reg_vid_pll, 1, 24, 1); /* vid_pll2_clk_sel_hdmi */

	switch (div_sel) {
	case VID_PLL_DIV_1:
		shift_val = 0xFFFF;
		shift_sel = 0;
		break;
	case VID_PLL_DIV_2:
		shift_val = 0x0aaa;
		shift_sel = 0;
		break;
	case VID_PLL_DIV_3:
		shift_val = 0x0db6;
		shift_sel = 0;
		break;
	case VID_PLL_DIV_3p5:
		shift_val = 0x36cc;
		shift_sel = 1;
		break;
	case VID_PLL_DIV_3p75:
		shift_val = 0x6666;
		shift_sel = 2;
		break;
	case VID_PLL_DIV_4:
		shift_val = 0x0ccc;
		shift_sel = 0;
		break;
	case VID_PLL_DIV_5:
		shift_val = 0x739c;
		shift_sel = 2;
		break;
	case VID_PLL_DIV_6:
		shift_val = 0x0e38;
		shift_sel = 0;
		break;
	case VID_PLL_DIV_6p25:
		shift_val = 0x0000;
		shift_sel = 3;
		break;
	case VID_PLL_DIV_7:
		shift_val = 0x3c78;
		shift_sel = 1;
		break;
	case VID_PLL_DIV_7p5:
		shift_val = 0x78f0;
		shift_sel = 2;
		break;
	case VID_PLL_DIV_12:
		shift_val = 0x0fc0;
		shift_sel = 0;
		break;
	case VID_PLL_DIV_14:
		shift_val = 0x3f80;
		shift_sel = 1;
		break;
	case VID_PLL_DIV_15:
		shift_val = 0x7f80;
		shift_sel = 2;
		break;
	case VID_PLL_DIV_2p5:
		shift_val = 0x5294;
		shift_sel = 2;
		break;
	case VID_PLL_DIV_3p25:
		shift_val = 0x66cc;
		shift_sel = 2;
		break;
	default:
		pr_info("Error: clocks_set_vid_clk_div:  Invalid parameter\n");
		break;
	}

	if (shift_val == 0xffff) {      /* if divide by 1 */
		hd21_set_reg_bits(reg_vid_pll, 1, 18, 1);
	} else {
		hd21_set_reg_bits(reg_vid_pll, 0, 18, 1);
		hd21_set_reg_bits(reg_vid_pll, 0, 16, 2);
		hd21_set_reg_bits(reg_vid_pll, 0, 15, 1);
		hd21_set_reg_bits(reg_vid_pll, 0, 0, 15);

		hd21_set_reg_bits(reg_vid_pll, shift_sel, 16, 2);
		hd21_set_reg_bits(reg_vid_pll, 1, 15, 1);
		hd21_set_reg_bits(reg_vid_pll, shift_val, 0, 15);
		hd21_set_reg_bits(reg_vid_pll, 0, 15, 1);
	}
	/* Enable the final output clock */
	hd21_set_reg_bits(reg_vid_pll, 1, 19, 1);
	hd21_write_reg(CLKCTRL_HDMI_PLL_TMDS_CLK_DIV, 0x010a739c);	//hard code
}

static void set_vid_clk_div(struct hdmitx_dev *hdev, u32 div)
{
	hd21_set_reg_bits(CLKCTRL_VID_CLK0_CTRL, 0, 16, 3);
	hd21_set_reg_bits(CLKCTRL_VID_CLK0_DIV, div - 1, 0, 8);
	hd21_set_reg_bits(CLKCTRL_VID_CLK0_CTRL, 7, 0, 3);
}

static void set_hdmitx_enc_div(struct hdmitx_dev *hdev, u32 div)
{
	div = check_div(div);
	if (div == -1)
		return;
	hd21_set_reg_bits(CLKCTRL_VID_CLK0_DIV, div, 12, 4);
	hd21_set_reg_bits(CLKCTRL_VID_CLK0_CTRL2, 1, 3, 1);
}

static void set_hdmitx_fe_div(struct hdmitx_dev *hdev, u32 div)
{
	div = check_div(div);
	if (div == -1)
		return;
	hd21_set_reg_bits(CLKCTRL_ENC0_HDMI_CLK_CTRL, div, 20, 4);
	hd21_set_reg_bits(CLKCTRL_VID_CLK0_CTRL2, 1, 9, 1);
}

static void set_hdmitx_pnx_div(struct hdmitx_dev *hdev, u32 div)
{
	div = check_div(div);
	if (div == -1)
		return;
	hd21_set_reg_bits(CLKCTRL_ENC0_HDMI_CLK_CTRL, div, 24, 4);
	hd21_set_reg_bits(CLKCTRL_VID_CLK0_CTRL2, 1, 10, 1);
}

static void set_hdmitx_pixel_div(struct hdmitx_dev *hdev, u32 div)
{
	div = check_div(div);
	if (div == -1)
		return;
	hd21_set_reg_bits(CLKCTRL_ENC0_HDMI_CLK_CTRL, div, 16, 4);
	hd21_set_reg_bits(CLKCTRL_VID_CLK0_CTRL2, 1, 5, 1);
}

/* mode hpll_clk_out od1 od2(PHY) od3
 * vid_pll_div vid_clk_div hdmi_tx_pixel_div encp_div enci_div
 */
static struct hw_enc_clk_val_group setting_enc_clk_val_24[] = {
	{{HDMI_16_1920x1080p60_16x9,
	  HDMI_31_1920x1080p50_16x9,
	  HDMI_VIC_END},
		5940000, 8, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0,
			VID_PLL_DIV_5, VID_PLL_DIV_5},
	{{HDMI_196_7680x4320p30_16x9,
	  HDMI_199_7680x4320p60_16x9,
	  HDMI_VIC_END},
		5000000, 8, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0,
			VID_PLL_DIV_5, VID_PLL_DIV_5},
};

/* For 3D Frame Packing Clock Setting
 * mode hpll_clk_out od1 od2(PHY) od3
 * vid_pll_div vid_clk_div hdmi_tx_pixel_div encp_div enci_div
 */
static struct hw_enc_clk_val_group setting_3dfp_enc_clk_val[] = {
	{{HDMI_16_1920x1080p60_16x9,
	  HDMI_31_1920x1080p50_16x9,
	  HDMI_VIC_END},
		5940000, 2, 1, 2, VID_PLL_DIV_5, 1, 1, 1, 1, 1},
	{{HDMI_19_1280x720p50_16x9,
	  HDMI_4_1280x720p60_16x9,
	  HDMI_34_1920x1080p30_16x9,
	  HDMI_32_1920x1080p24_16x9,
	  HDMI_33_1920x1080p25_16x9,
	  HDMI_VIC_END},
		5940000, 2, 2, 2, VID_PLL_DIV_5, 1, 1, 1, 1, 1},
	/* NO 2160p mode*/
	{{HDMI_VIC_FAKE,
	  HDMI_VIC_END},
		3450000, 1, 2, 2, VID_PLL_DIV_5, 1, 1, 1, 1, 1},
};

static void hdmitx21_set_clk_(struct hdmitx_dev *hdev)
{
	int i = 0;
	int j = 0;
	struct hw_enc_clk_val_group *p_enc = NULL;
	enum hdmi_vic vic = hdev->para->timing.vic;
	enum hdmi_colorspace cs = hdev->para->cs;
	enum hdmi_color_depth cd = hdev->para->cd;
	struct hw_enc_clk_val_group tmp_clk = {0};

	//if (hdev->pxp_mode) /* skip VCO setting */
	//	return;

	/* YUV 422 always use 24B mode */
	if (cs == HDMI_COLORSPACE_YUV422)
		cd = COLORDEPTH_24B;
	if (hdev->flag_3dfp) {
		p_enc = &setting_3dfp_enc_clk_val[0];
		for (j = 0; j < sizeof(setting_3dfp_enc_clk_val)
			/ sizeof(struct hw_enc_clk_val_group); j++) {
			for (i = 0; ((i < GROUP_MAX) && (p_enc[j].group[i]
				!= HDMI_VIC_END)); i++) {
				if (vic == p_enc[j].group[i])
					goto next;
			}
		}
		if (j == sizeof(setting_3dfp_enc_clk_val)
			/ sizeof(struct hw_enc_clk_val_group)) {
			pr_info("Not find VIC = %d for hpll setting\n", vic);
			return;
		}
	} else if (cd == COLORDEPTH_24B) {
		pr_info("%s %d\n", __func__, __LINE__);
		p_enc = &setting_enc_clk_val_24[0];
		for (j = 0; j < sizeof(setting_enc_clk_val_24)
			/ sizeof(struct hw_enc_clk_val_group); j++) {
			for (i = 0; ((i < GROUP_MAX) && (p_enc[j].group[i]
				!= HDMI_VIC_END)); i++) {
				if (vic == p_enc[j].group[i])
					goto next;
			}
		}
		if (j == sizeof(setting_enc_clk_val_24)
			/ sizeof(struct hw_enc_clk_val_group)) {
			pr_info("Not find VIC = %d for hpll setting\n", vic);
			return;
		}
	}
next:
	pr_info("%s %d\n", __func__, __LINE__);
	memcpy(&tmp_clk, &p_enc[j], sizeof(struct hw_enc_clk_val_group));
	set_hpll_clk_out(tmp_clk.txpll_vco_clk);
	if (cd == COLORDEPTH_24B && hdev->sspll)
		set_hpll_sspll(vic);
	if (hdev->frl_rate) {
		set_frl_hpll_od(hdev->frl_rate);
	} else {
		/* for legacy modes */
		set_hpll_od0(tmp_clk.txpll_3_od0);
		set_hpll_od1(tmp_clk.txpll_3_od1);
		set_hpll_od2(tmp_clk.txpll_3_od2);
		set_hpll_od3(tmp_clk.txpll_4_od);
	}
	clocks_set_vid_clk_div_for_hdmi(tmp_clk.vid_pll_clk_div);
	pr_info("j = %d  vid_pll_clk_div = %d\n", j, tmp_clk.vid_pll_clk_div);
	set_vid_clk_div(hdev, 1);
	set_hdmitx_enc_div(hdev, 1);
	set_hdmitx_fe_div(hdev, 1);
	set_hdmitx_pnx_div(hdev, 1);
	set_hdmitx_pixel_div(hdev, 1);
	hdmitx_enable_encp_clk(hdev);

	//configure crt_video V1: in_sel=vid_pll_clk(0),div_n=xd)
	set_crt_video_enc(0, 0, 1);
	if (hdev->enc_idx == 2)
		set_crt_video_enc2(0, 0, 1);
}

static int likely_frac_rate_mode(char *m)
{
	if (strstr(m, "24hz") || strstr(m, "30hz") || strstr(m, "60hz") ||
	    strstr(m, "120hz") || strstr(m, "240hz"))
		return 1;
	else
		return 0;
}

#define MIN_FPLL_VCO 1600000 /* Min 1.6GHz */
#define MAX_FPLL_VCO 3200000 /* Max 3.2GHz */

void hdmitx_set_fpll(struct hdmitx_dev *hdev)
{
	u32 fpll_vco = 2376000;
	u32 div = 1;
	u32 tmp_clk = 0;
	u32 pixel_od = 0;

	if (!hdev && !hdev->para)
		return;

	tmp_clk = hdev->para->timing.pixel_freq;
	if (hdev->frl_rate)
		tmp_clk /= 2;
	switch (hdev->para->cs) {
	case HDMI_COLORSPACE_RGB:
	case HDMI_COLORSPACE_YUV444:
		if (hdev->para->cd == COLORDEPTH_30B) {
			tmp_clk = tmp_clk * 5 / 4;
			pixel_od = 1;
		}
		if (hdev->para->cd == COLORDEPTH_36B) {
			tmp_clk = tmp_clk * 3 / 2;
			pixel_od = 2;
		}
		if (hdev->para->cd == COLORDEPTH_48B) {
			tmp_clk = tmp_clk * 2;
			pixel_od = 4;
		}
		break;
	case HDMI_COLORSPACE_YUV420:
		tmp_clk /= 2;
		if (hdev->para->cd == COLORDEPTH_30B) {
			tmp_clk = tmp_clk * 5 / 4;
			pixel_od = 1;
		}
		if (hdev->para->cd == COLORDEPTH_36B) {
			tmp_clk = tmp_clk * 3 / 4;
			pixel_od = 2;
		}
		if (hdev->para->cd == COLORDEPTH_48B) {
			tmp_clk *= 1;
			pixel_od = 4;
		}
		break;
	case HDMI_COLORSPACE_YUV422:
	default:
		tmp_clk *= 1;
		pixel_od = 0;
		break;
	}
	tmp_clk *= 2; /* here is a fixed DIV2 to tmds_clk */

	fpll_vco = tmp_clk;
	if (fpll_vco > MAX_FPLL_VCO) {
		pr_info("hdmitx21: FPLL VCO over clock %d\n", fpll_vco);
		return;
	}
	div = 1;
	do {
		if (fpll_vco >= MIN_FPLL_VCO && fpll_vco < MAX_FPLL_VCO)
			break;
		div *= 2;
		fpll_vco *= 2;
	} while (div <= 64);

	hdmitx_set_s5_fpll(fpll_vco, div, pixel_od);
}

void hdmitx_set_clkdiv(struct hdmitx_dev *hdev)
{
	hdmitx_set_s5_clkdiv(hdev);
}

static void hdmitx_check_frac_rate(struct hdmitx_dev *hdev)
{
	struct hdmi_format_para *para = hdev->para;

	frac_rate = hdev->frac_rate_policy;
	if (para && para->timing.name && likely_frac_rate_mode(para->timing.name)) {
		;
	} else {
		pr_info("this mode doesn't have frac_rate\n");
		frac_rate = 0;
	}

	pr_info("frac_rate = %d\n", hdev->frac_rate_policy);
}

void hdmitx21_set_clk(struct hdmitx_dev *hdev)
{
	hdmitx_check_frac_rate(hdev);
	hdmitx21_set_clk_(hdev);
}

void hdmitx21_disable_clk(struct hdmitx_dev *hdev)
{
	/* cts_encp/enci_clk */
	if (hdev->para->hdmitx_vinfo.viu_mux == VIU_MUX_ENCI)
		hdmitx_disable_enci_clk(hdev);
	else
		hdmitx_disable_encp_clk(hdev);

	/* hdmi_tx_pixel_clk */
	hdmitx_disable_tx_pixel_clk(hdev);
}

//===============================================================
//  CRT_VIDEO SETTING FUNCTIONS
//===============================================================
static void set_crt_video_enc(u32 vidx, u32 in_sel, u32 div_n)
//input :
//vidx      : 0:V1; 1:V2;     there have 2 parallel set clock generator: V1 and V2
//in_sel     : 0:vid_pll_clk;  1:fclk_div4; 2:flck_div3; 3:fclk_div5;
//            4:vid_pll2_clk; 5:fclk_div7; 6:vid_pll2_clk;
//div_n      : clock divider for enci_clk/encp_clk/encl_clk/vda_clk/hdmi_tx_pixel_clk;
{
	if (vidx == 0) { //V1
		hd21_set_reg_bits(CLKCTRL_VID_CLK0_CTRL, 0, 19, 1); //[19] -disable clk_div0
		udelay(2);
		// [18:16] - cntl_clk_in_sel
		hd21_set_reg_bits(CLKCTRL_VID_CLK0_CTRL, in_sel, 16, 3);
		hd21_set_reg_bits(CLKCTRL_VID_CLK0_DIV, div_n - 1, 0, 8); // [7:0]   - cntl_xd0
		udelay(5);
		hd21_set_reg_bits(CLKCTRL_VID_CLK0_CTRL, 1, 19, 1); //[19] -enable clk_div0
	} else { //V2
		hd21_set_reg_bits(CLKCTRL_VIID_CLK0_CTRL, 0, 19, 1); //[19] -disable clk_div0
		udelay(2);
		// [18:16] - cntl_clk_in_sel
		hd21_set_reg_bits(CLKCTRL_VIID_CLK0_CTRL, in_sel, 16, 3);
		hd21_set_reg_bits(CLKCTRL_VIID_CLK0_DIV, div_n - 1, 0, 8); // [7:0]   - cntl_xd0
		udelay(5);
		hd21_set_reg_bits(CLKCTRL_VIID_CLK0_CTRL, 1, 19, 1); //[19] -enable clk_div0
	}
}

//===============================================================
//  CRT_VIDEO SETTING FUNCTIONS
//===============================================================
static void set_crt_video_enc2(u32 vidx, u32 in_sel, u32 div_n)
//input :
//vidx      : 0:V1; 1:V2;     there have 2 parallel set clock generator: V1 and V2
//in_sel     : 0:vid_pll_clk;  1:fclk_div4; 2:flck_div3; 3:fclk_div5;
//            4:vid_pll2_clk; 5:fclk_div7; 6:vid_pll2_clk;
//div_n      : clock divider for enci_clk/encp_clk/encl_clk/vda_clk/hdmi_tx_pixel_clk;
{
	if (vidx == 0) { //V1
		hd21_set_reg_bits(CLKCTRL_VID_CLK2_CTRL, 0, 19, 1); //[19] -disable clk_div0
		udelay(2);
		// [18:16] - cntl_clk_in_sel
		hd21_set_reg_bits(CLKCTRL_VID_CLK2_CTRL, in_sel, 16, 3);
		hd21_set_reg_bits(CLKCTRL_VID_CLK2_DIV, (div_n - 1), 0, 8); // [7:0]   - cntl_xd0
		udelay(5);
		hd21_set_reg_bits(CLKCTRL_VID_CLK2_CTRL, 1, 19, 1); //[19] -enable clk_div0
	} else { //V2
		hd21_set_reg_bits(CLKCTRL_VIID_CLK2_CTRL, 0, 19, 1); //[19] -disable clk_div0
		udelay(2);
		// [18:16] - cntl_clk_in_sel
		hd21_set_reg_bits(CLKCTRL_VIID_CLK2_CTRL, in_sel, 16, 3);
		// [7:0]   - cntl_xd0
		hd21_set_reg_bits(CLKCTRL_VIID_CLK2_DIV, (div_n - 1), 0, 8);
		udelay(5);
		hd21_set_reg_bits(CLKCTRL_VIID_CLK2_CTRL, 1, 19, 1); //[19] -enable clk_div0
	}
}
