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

#define MIN_HTXPLL_VCO 3000000 /* Min 3GHz */
#define MAX_HTXPLL_VCO 6000000 /* Max 6GHz */
#define MIN_FPLL_VCO 1600000 /* Min 1.6GHz */
#define MAX_FPLL_VCO 3200000 /* Max 3.2GHz */
#define MIN_GP2PLL_VCO 1600000 /* Min 1.6GHz */
#define MAX_GP2PLL_VCO 3200000 /* Max 3.2GHz */

/* local frac_rate flag */
static u32 frac_rate;
static void set_crt_video_enc(u32 vidx, u32 in_sel, u32 div_n);
static void set_crt_video_enc2(u32 vidx, u32 in_sel, u32 div_n);
static void hdmitx_check_frac_rate(struct hdmitx_dev *hdev);
static int likely_frac_rate_mode(char *m);

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

/* --------------------------------------------------
 *             set_tmds_vid_clk_div
 * --------------------------------------------------
 * wire            clk_final_en    = control[19];
 * wire            clk_div1        = control[18];
 * wire    [1:0]   clk_sel         = control[17:16];
 * wire            set_preset      = control[15];
 * wire    [14:0]  shift_preset    = control[14:0];
 * div_src: 0 means divide the hdmi_tmds_out2 to tmds_clk
 *          1 means divide the hdmi_tmds_out2 to vid_pll0_clk
 */
static void set_tmds_vid_clk_div(u8 div_src, u32 div_val)
{
	u32 div_reg;
	u32 shift_val = 0;
	u32 shift_sel = 0;

	div_reg = (div_src == 1) ? CLKCTRL_HDMI_VID_PLL_CLK_DIV : CLKCTRL_HDMI_PLL_TMDS_CLK_DIV;

	// Disable the output clock
	hd21_set_reg_bits(div_reg, 0, 15, 1);
	hd21_set_reg_bits(div_reg, 0, 19, 1);

	switch (div_val) {
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
	default:
		pr_err("%s[%d] invalid div %d\n", __func__, __LINE__, div_val);
	}

	if (shift_val == 0xffff) { // if divide by 1
		hd21_set_reg_bits(div_reg, 1, 18, 1);
	} else {
		hd21_set_reg_bits(div_reg, shift_val, 0, 15);
		hd21_set_reg_bits(div_reg, 1, 15, 1);
		hd21_set_reg_bits(div_reg, 0, 20, 1);
		hd21_set_reg_bits(div_reg, shift_sel, 16, 3);
		// Set the selector low
		hd21_set_reg_bits(div_reg, 0, 15, 1);
	}
	// Enable the final output clock
	hd21_set_reg_bits(div_reg, 1, 19, 1);
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
	{{HDMI_96_3840x2160p50_16x9,
	  HDMI_97_3840x2160p60_16x9,
	  HDMI_VIC_END},
		5940000, 2, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0,
			VID_PLL_DIV_5, VID_PLL_DIV_5},
	{{HDMI_196_7680x4320p30_16x9,
	  HDMI_198_7680x4320p50_16x9,
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

/* if vsync likes 24000, 30000, ... etc, return 1 */
static bool is_vsync_int(u32 clk)
{
	if (clk % 3000 == 0)
		return 1;
	return 0;
}

/* if vsync likes 59940, ... etc, return 1 */
static bool is_vsync_frac(u32 clk)
{
	clk += clk / 1000;
	if (is_vsync_int(clk) || is_vsync_int(clk + 1))
		return 1;
	return 0;
}

/* for varied hdmi basic modes, such as
 * vic/16, the vsync is 60, and may shift to 59.94
 * but vic/2, the vsync is 59.94, and may shift to 60
 * return values:
 *    0: no any shift
 *    1: shift down 0.1%
 *    2: shift up 0.1%
 */
static u32 check_clock_shift(enum hdmi_vic vic, u32 frac_policy)
{
	const struct hdmi_timing *timing = NULL;

	timing = hdmitx21_gettiming_from_vic(vic);
	if (!timing) {
		pr_err("%s[%d] not valid vic %d\n", __func__, __LINE__, vic);
		return 0;
	}

	/* only check such as 24hz, 30hz, 60hz, ... */
	if (!likely_frac_rate_mode(timing->name))
		return 0;

	if (is_vsync_int(timing->v_freq)) {
		if (frac_policy)
			return 1;
		else
			return 0;
	}
	if (is_vsync_frac(timing->v_freq)) {
		if (frac_policy)
			return 0;
		else
			return 2;
	}
	return 0;
}

static void set_hdmitx_s5_htx_pll(struct hdmitx_dev *hdev)
{
	enum hdmi_vic vic = HDMI_0_UNKNOWN;
	enum hdmi_colorspace cs = HDMI_COLORSPACE_YUV444;
	enum hdmi_color_depth cd = COLORDEPTH_24B;
	u32 base_pixel_clk = 25200;
	u32 htx_vco = 5940000;
	u32 div = 1;

	if (!hdev || !hdev->para)
		return;

	vic = hdev->para->timing.vic;
	cs = hdev->para->cs;
	cd = hdev->para->cd;
	if (vic == HDMI_0_UNKNOWN) {
		pr_err("%s[%d] not valid vic %d\n", __func__, __LINE__, vic);
		return;
	}

	base_pixel_clk = hdev->para->timing.pixel_freq;
	if (base_pixel_clk < 25175 || base_pixel_clk > 5940000) {
		pr_err("%s[%d] not valid pixel clock %d\n", __func__, __LINE__, base_pixel_clk);
		return;
	}

	/* For FRL modes */
	if (hdev->frl_rate != FRL_NONE) {
		pr_info("set hpll for frl_rate %d\n", hdev->frl_rate);
		switch (hdev->frl_rate) {
		case FRL_3G3L:
			set21_s5_htxpll_clk_out(6000000, 8);
			break;
		case FRL_6G3L:
			set21_s5_htxpll_clk_out(6000000, 4);
			break;
		case FRL_6G4L:
			set21_s5_htxpll_clk_out(6000000, 2);
			break;
		case FRL_12G4L:
			set21_s5_htxpll_clk_out(6000000, 1);
			break;
		case FRL_8G4L:
			set21_s5_htxpll_clk_out(4000000, 1);
			break;
		case FRL_10G4L:
			set21_s5_htxpll_clk_out(5000000, 1);
			break;
		default:
			pr_err("not support frl_rate: %d\n", hdev->frl_rate);
			break;
		}
		return;
	}

	pr_info("%s[%d] base_pixel_clk %d  cs %d  cd %d  frac_rate %d\n",
		__func__, __LINE__, base_pixel_clk, cs, cd, frac_rate);
	/* for legacy TMDS modes */
	if (cs != HDMI_COLORSPACE_YUV422) {
		switch (cd) {
		case COLORDEPTH_48B:
			base_pixel_clk = base_pixel_clk * 2;
			break;
		case COLORDEPTH_36B:
			base_pixel_clk = base_pixel_clk * 3 / 2;
			break;
		case COLORDEPTH_30B:
			base_pixel_clk = base_pixel_clk * 5 / 4;
			break;
		case COLORDEPTH_24B:
		default:
			base_pixel_clk = base_pixel_clk * 1;
			break;
		}
	}
	if (check_clock_shift(vic, frac_rate) == 1)
		base_pixel_clk = base_pixel_clk - base_pixel_clk / 1001;
	if (check_clock_shift(vic, frac_rate) == 2)
		base_pixel_clk = base_pixel_clk + base_pixel_clk / 1000;
	base_pixel_clk = base_pixel_clk * 10; /* for tmds modes, here should multi 10 */
	if (cs == HDMI_COLORSPACE_YUV420)
		base_pixel_clk /= 2;
	pr_info("%s[%d] calculate pixel_clk to %d\n", __func__, __LINE__, base_pixel_clk);
	if (base_pixel_clk > MAX_HTXPLL_VCO) {
		pr_err("%s[%d] base_pixel_clk %d over MAX_HTXPLL_VCO %d\n",
			__func__, __LINE__, base_pixel_clk, MAX_HTXPLL_VCO);
	}

	div = 1;
	/* the base pixel_clk range should be 250M ~ 5940M? */
	htx_vco = base_pixel_clk;
	do {
		if (htx_vco >= MIN_HTXPLL_VCO && htx_vco < MAX_HTXPLL_VCO)
			break;
		div *= 2;
		htx_vco *= 2;
	} while (div <= 32);

	/* the hdmi phy works under DUAL mode, and the div should be multiply 2 */
	div *= 2;

	set21_s5_htxpll_clk_out(htx_vco, div);
}

static void set_hdmitx_htx_pll(struct hdmitx_dev *hdev)
{
	int i = 0;
	int j = 0;
	struct hw_enc_clk_val_group *p_enc = NULL;
	enum hdmi_vic vic = hdev->para->timing.vic;
	enum hdmi_colorspace cs = hdev->para->cs;
	enum hdmi_color_depth cd = hdev->para->cd;
	u8 clk_div_val = VID_PLL_DIV_5;

	//if (hdev->pxp_mode) /* skip VCO setting */
	//	return;

	if (hdev->data->chip_type >= MESON_CPU_ID_S5) {
		set_hdmitx_s5_htx_pll(hdev);
		if (hdev->frl_rate)
			set_frl_hpll_od(hdev->frl_rate);
		if (cs != HDMI_COLORSPACE_YUV422) {
			if (cd == COLORDEPTH_36B)
				clk_div_val = VID_PLL_DIV_7p5;
			else if (cd == COLORDEPTH_30B)
				clk_div_val = VID_PLL_DIV_6p25;
			else
				clk_div_val = VID_PLL_DIV_5;
		}
		set_tmds_vid_clk_div(0, VID_PLL_DIV_5);
		set_tmds_vid_clk_div(1, clk_div_val);
		// set crt_vid_mux_div
		//[19] disable clk_div0
		hd21_set_reg_bits(CLKCTRL_VID_CLK0_CTRL, 0, 19, 1);
		// bit[18:16] crt_vid_mux_div source select
		hd21_set_reg_bits(CLKCTRL_VID_CLK0_CTRL, 0, 16, 3);
		hd21_set_reg_bits(CLKCTRL_VID_CLK0_DIV, 0, 0, 8);
		// bit[2:0] crt_vid_mux_div div1/2/4 enable
		hd21_set_reg_bits(CLKCTRL_VID_CLK0_CTRL, 7, 0, 3);
		// cts_enc_clk div and enable
		hd21_set_reg_bits(CLKCTRL_VIID_CLK0_DIV, 0, 12, 4);
		hd21_set_reg_bits(CLKCTRL_VID_CLK0_CTRL2, 1, 3, 1);
		// enc0_hdmi_tx_fe_clk div and enable
		hd21_set_reg_bits(CLKCTRL_ENC0_HDMI_CLK_CTRL, 0, 20, 4);
		hd21_set_reg_bits(CLKCTRL_VID_CLK0_CTRL2, 1, 9, 1);
		// enc0_hdmi_tx_pnx_clk div and enable
		hd21_set_reg_bits(CLKCTRL_ENC0_HDMI_CLK_CTRL, 0, 24, 4);
		hd21_set_reg_bits(CLKCTRL_VID_CLK0_CTRL2, 1, 10, 1);
		// enc0_hdmi_tx_pixel_clk div and enable
		hd21_set_reg_bits(CLKCTRL_ENC0_HDMI_CLK_CTRL, 0, 16, 4);
		hd21_set_reg_bits(CLKCTRL_VID_CLK0_CTRL2, 1, 5, 1);
		//[19] enable clk_div0
		hd21_set_reg_bits(CLKCTRL_VID_CLK0_CTRL, 1, 19, 1);
		return;
	}
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
	pr_info("TODO here\n");

	return;
	if (cd == COLORDEPTH_24B && hdev->sspll)
		set_hpll_sspll(vic);
	if (hdev->frl_rate) {
		set_frl_hpll_od(hdev->frl_rate);
	} else {
	}
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

static void hdmitx_set_fpll_without_dsc(struct hdmitx_dev *hdev)
{
	u32 fpll_vco = 2376000;
	u32 div = 1;
	u32 tmp_clk = 0;
	u32 pixel_od = 0;
	enum hdmi_vic vic = HDMI_0_UNKNOWN;

	if (!hdev && !hdev->para)
		return;

	vic = hdev->para->timing.vic;
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
			tmp_clk = tmp_clk * 3 / 2;
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
	if (check_clock_shift(vic, frac_rate) == 1) {
		fpll_vco = fpll_vco - fpll_vco / 1001;
		pr_info("fpll_vco %d down shift to %d\n", tmp_clk, fpll_vco);
	}
	if (check_clock_shift(vic, frac_rate) == 2) {
		fpll_vco = fpll_vco + fpll_vco / 1000;
		pr_info("fpll_vco %d up shift to %d\n", tmp_clk, fpll_vco);
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

static void hdmitx_set_fpll_with_dsc(struct hdmitx_dev *hdev)
{
	u32 fpll_vco = 2376000;
	u32 div = 1;
	u32 tmp_clk = 0;
	u32 pixel_od = 0;

	if (!hdev && !hdev->para)
		return;

	/* HARD CODE, FRL8G4L 4320p60 y420 8bit, HDMI 2.1 Spec, Page 281 */
	/* 594 / 4500 * (2380 + 116) */
	tmp_clk = 329472 * 2;
	/* TODO */
	fpll_vco = tmp_clk;
	if (fpll_vco > MAX_FPLL_VCO) {
		pr_info("hdmitx21: FPLL VCO over clock %d\n", fpll_vco);
		return;
	}
	if (0) { /* TODO */
		fpll_vco = fpll_vco - fpll_vco / 1001;
		pr_info("fpll_vco %d shift to %d\n", tmp_clk, fpll_vco);
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

void hdmitx_set_fpll(struct hdmitx_dev *hdev)
{
	if (hdev->dsc_en)
		hdmitx_set_fpll_with_dsc(hdev);
	else
		hdmitx_set_fpll_without_dsc(hdev);
}

void hdmitx_set_gp2pll(struct hdmitx_dev *hdev)
{
	u32 gp2pll_vco = 2376000;
	u32 div = 1;
	u32 tmp_clk = 0;

	if (!hdev && !hdev->para)
		return;

	tmp_clk = hdev->para->timing.pixel_freq;
	if (hdev->frl_rate)
		tmp_clk /= 2;
	switch (hdev->para->cs) {
	case HDMI_COLORSPACE_RGB:
	case HDMI_COLORSPACE_YUV444:
		if (hdev->para->cd == COLORDEPTH_30B)
			tmp_clk = tmp_clk * 5 / 4;
		if (hdev->para->cd == COLORDEPTH_36B)
			tmp_clk = tmp_clk * 3 / 2;
		if (hdev->para->cd == COLORDEPTH_48B)
			tmp_clk = tmp_clk * 2;
		break;
	case HDMI_COLORSPACE_YUV420:
		tmp_clk /= 2;
		if (hdev->para->cd == COLORDEPTH_30B)
			tmp_clk = tmp_clk * 5 / 4;
		if (hdev->para->cd == COLORDEPTH_36B)
			tmp_clk = tmp_clk * 3 / 2;
		if (hdev->para->cd == COLORDEPTH_48B)
			tmp_clk *= 1;
		break;
	case HDMI_COLORSPACE_YUV422:
	default:
		tmp_clk *= 1;
		break;
	}
	tmp_clk *= 2; /* here is a fixed DIV2 to tmds_clk */

	gp2pll_vco = tmp_clk;
	if (gp2pll_vco > MAX_FPLL_VCO) {
		pr_info("hdmitx21: GP2PLL VCO over clock %d\n", gp2pll_vco);
		return;
	}
	if (0) { /* TODO */
		gp2pll_vco = gp2pll_vco * 1000 / 1001;
		pr_info("gp2pll_vco %d shift to %d\n", tmp_clk, gp2pll_vco);
	}
	div = 1;
	do {
		if (gp2pll_vco >= MIN_GP2PLL_VCO && gp2pll_vco < MAX_GP2PLL_VCO)
			break;
		div *= 2;
		gp2pll_vco *= 2;
	} while (div <= 16);

	hdmitx_set_s5_gp2pll(gp2pll_vco, div);
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

	if (hdev->data->chip_type == MESON_CPU_ID_T7) {
		set21_t7_hpll_clk_out(frac_rate, 5940000); /* TODO, for t7 */
		return;
	}

	disable_hdmitx_s5_plls(hdev);
	/* typical 3 modes: legacy tmds, FRL w/o DSC, FRL w/ DSC */
	set_hdmitx_htx_pll(hdev);
	if (hdev->frl_rate) {
		hdmitx_set_fpll(hdev);
		if (hdev->dsc_en)
			hdmitx_set_gp2pll(hdev);
	}
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
