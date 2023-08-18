// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/major.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/reset.h>
#include <linux/compiler.h>
#include <linux/arm-smccc.h>

#include <linux/amlogic/media/video_sink/video.h>
#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/media/vout/hdmi_tx21/enc_clk_config.h>
#include <linux/amlogic/media/vout/hdmi_tx21/hdmi_info_global.h>
#include <linux/amlogic/media/vout/hdmi_tx21/hdmi_tx_module.h>
#include <linux/amlogic/media/vout/hdmi_tx21/hdmi_tx_ddc.h>
#include <linux/arm-smccc.h>
#include "common.h"
#include "../hdmi_tx_ext.h"
#include <linux/amlogic/clk_measure.h>

#define MESON_CPU_ID_T7 0
static void hdmi_phy_suspend(void);
static void hdmi_phy_wakeup(struct hdmitx_dev *hdev);
static void hdmitx_set_phy(struct hdmitx_dev *hdev);
static void hdmitx_set_hw(struct hdmitx_dev *hdev);
static void hdmitx_csc_config(u8 input_color_format,
			      u8 output_color_format,
			      u8 color_depth);
static int hdmitx_hdmi_dvi_config(struct hdmitx_dev *hdev,
				  u32 dvi_mode);

static int hdmitx_set_dispmode(struct hdmitx_dev *hdev);
static int hdmitx_set_audmode(struct hdmitx_dev *hdev,
			      struct hdmitx_audpara *audio_param);
static void hdmitx_debug(struct hdmitx_dev *hdev, const char *buf);
static void hdmitx_debug_bist(struct hdmitx_dev *hdev, u32 num);
static void hdmitx_uninit(struct hdmitx_dev *hdev);
static int hdmitx_cntl(struct hdmitx_dev *hdev, u32 cmd,
		       u32 argv);
static int hdmitx_cntl_ddc(struct hdmitx_dev *hdev, u32 cmd,
			   unsigned long argv);
static int hdmitx_get_state(struct hdmitx_dev *hdev, u32 cmd,
			    u32 argv);
static int hdmitx_cntl_config(struct hdmitx_dev *hdev, u32 cmd,
			      u32 argv);
static int hdmitx_cntl_misc(struct hdmitx_dev *hdev, u32 cmd,
			    u32  argv);
static enum hdmi_vic get_vic_from_pkt(void);
static enum frl_rate_enum get_current_frl_rate(void);

#define EDID_RAM_ADDR_SIZE	 (8)

/* HSYNC polarity: active high */
#define HSYNC_POLARITY	 1
/* VSYNC polarity: active high */
#define VSYNC_POLARITY	 1
/* Pixel format: 0=RGB444; 1=YCbCr444; 2=Rsrv; 3=YCbCr422. */
#define TX_INPUT_COLOR_FORMAT   HDMI_COLORSPACE_YUV444
/* Pixel range: 0=16-235/240; 1=16-240; 2=1-254; 3=0-255. */
#define TX_INPUT_COLOR_RANGE	0
/* Pixel bit width: 4=24-bit; 5=30-bit; 6=36-bit; 7=48-bit. */
#define TX_COLOR_DEPTH		 COLORDEPTH_24B

int hdmitx21_hpd_hw_op(enum hpd_op cmd)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	switch (hdev->data->chip_type) {
	case MESON_CPU_ID_S5:
		return !!(hd21_read_reg(PADCTRL_GPIOH_I) & (1 << 2));
	case MESON_CPU_ID_T7:
	default:
		return !!(hd21_read_reg(PADCTRL_GPIOW_I) & (1 << 15));
	}
	return 0;
}
EXPORT_SYMBOL(hdmitx21_hpd_hw_op);

int read21_hpd_gpio(void)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	switch (hdev->data->chip_type) {
	case MESON_CPU_ID_T7:
	default:
		return 1;
	}
	return 0;
}
EXPORT_SYMBOL(read21_hpd_gpio);

int hdmitx21_ddc_hw_op(enum ddc_op cmd)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	switch (hdev->data->chip_type) {
	case MESON_CPU_ID_T7:
	default:
		return 1;
	}
	return 0;
}
EXPORT_SYMBOL(hdmitx21_ddc_hw_op);

static void config_avmute(u32 val)
{
	pr_debug(HW "avmute set to %d\n", val);
	switch (val) {
	/* This code is required for proper generation of GCP's in ES1 */
	case SET_AVMUTE:
		hdmitx21_set_reg_bits(TPI_SC_IVCTX, 0, 7, 1);
		hdmitx21_set_reg_bits(TPI_SC_IVCTX, 0, 3, 1);
		hdmitx21_set_reg_bits(TPI_SC_IVCTX, 1, 3, 1);
		break;
	case CLR_AVMUTE:
		hdmitx21_set_reg_bits(TPI_SC_IVCTX, 0, 7, 1);
		hdmitx21_set_reg_bits(TPI_SC_IVCTX, 0, 3, 1);
		hdmitx21_set_reg_bits(TPI_SC_IVCTX, 1, 7, 1);
		break;
	case OFF_AVMUTE:
	default:
		break;
	}
}

void hdmitx21_set_avi_vic(enum hdmi_vic vic)
{
	hdmi_avi_infoframe_config(CONF_AVI_VIC, vic & 0xff);
}

static int read_avmute(void)
{
	return 0;
}

static void config_video_mapping(enum hdmi_colorspace cs,
				 enum hdmi_color_depth cd)
{
	u32 val = 0;

	pr_info("config: cs = %d cd = %d\n", cs, cd);
	switch (cs) {
	case HDMI_COLORSPACE_RGB:
		switch (cd) {
		case COLORDEPTH_24B:
			val = 0x1;
			break;
		case COLORDEPTH_30B:
			val = 0x3;
			break;
		case COLORDEPTH_36B:
			val = 0x5;
			break;
		case COLORDEPTH_48B:
			val = 0x7;
			break;
		default:
			break;
		}
		break;
	case HDMI_COLORSPACE_YUV444:
	case HDMI_COLORSPACE_YUV420:
		switch (cd) {
		case COLORDEPTH_24B:
			val = 0x9;
			break;
		case COLORDEPTH_30B:
			val = 0xb;
			break;
		case COLORDEPTH_36B:
			val = 0xd;
			break;
		case COLORDEPTH_48B:
			val = 0xf;
			break;
		default:
			break;
		}
		break;
	case HDMI_COLORSPACE_YUV422:
		switch (cd) {
		case COLORDEPTH_24B:
			val = 0x16;
			break;
		case COLORDEPTH_30B:
			val = 0x14;
			break;
		case COLORDEPTH_36B:
			val = 0x12;
			break;
		case COLORDEPTH_48B:
			pr_info("y422 no 48bits mode\n");
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}
	switch (cd) {
	case COLORDEPTH_24B:
		val = 0x4;
		break;
	case COLORDEPTH_30B:
		val = 0x5;
		break;
	case COLORDEPTH_36B:
		val = 0x6;
		break;
	case COLORDEPTH_48B:
		val = 0x7;
		break;
	default:
		break;
	}
	switch (cd) {
	case COLORDEPTH_30B:
	case COLORDEPTH_36B:
	case COLORDEPTH_48B:
		break;
	case COLORDEPTH_24B:
		break;
	default:
		break;
	}
}

/* reset HDMITX APB & TX */
void hdmitx21_sys_reset(void)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	switch (hdev->data->chip_type) {
	case MESON_CPU_ID_T7:
		hdmitx21_sys_reset_t7();
		break;
	case MESON_CPU_ID_S5:
		hdmitx21_sys_reset_s5();
		break;
	default:
		break;
	}
}

bool hdmitx21_uboot_already_display(struct hdmitx_dev *hdev)
{
	if (hdev->pxp_mode)
		return 0;

	if (hd21_read_reg(ANACTRL_HDMIPHY_CTRL0))
		return 1;
	return 0;
}

static enum hdmi_color_depth _get_colordepth(void)
{
	u32 data;
	u8 val;
	enum hdmi_color_depth depth = COLORDEPTH_24B;

	data = hdmitx21_rd_reg(P2T_CTRL_IVCTX);
	if (data & (1 << 7)) {
		val = data & 0x3;
		switch (val) {
		case 1:
			depth = COLORDEPTH_30B;
			break;
		case 2:
			depth = COLORDEPTH_36B;
			break;
		case 3:
			depth = COLORDEPTH_48B;
			break;
		case 0:
		default:
			depth = COLORDEPTH_24B;
			break;
		}
	}

	return depth;
}

int get_extended_colorimetry_from_avi(struct hdmitx_dev *hdev)
{
	int ret;
	u8 body[32] = {0};
	union hdmi_infoframe *infoframe = &hdev->infoframes.avi;
	struct hdmi_avi_infoframe *avi = &infoframe->avi;

	ret = hdmi_avi_infoframe_get(hdev, body);
	if (ret == -1 || ret == 0)
		return -1;
	ret = hdmi_infoframe_unpack(infoframe, body, sizeof(body));
	if (ret < 0) {
		pr_info("hdmitx21: parsing avi failed %d\n", ret);
	} else {
		/* colorimetry 3: extended */
		if (avi->colorimetry == HDMI_COLORIMETRY_EXTENDED)
			ret = avi->extended_colorimetry;
		else
			ret = -1;
	}
	return ret;
}

static enum hdmi_vic _get_vic_from_vsif(struct hdmitx_dev *hdev)
{
	int ret;
	u8 body[32] = {0};
	enum hdmi_vic hdmi4k_vic = HDMI_0_UNKNOWN;
	union hdmi_infoframe *infoframe = &hdev->infoframes.vend;
	struct hdmi_vendor_infoframe *vendor = &infoframe->vendor.hdmi;

	ret = hdmi_vend_infoframe_get(hdev, body);
	if (ret == -1 || ret == 0)
		return hdmi4k_vic;
	ret = hdmi_infoframe_unpack(infoframe, body, sizeof(body));
	if (ret < 0) {
		pr_info("hdmitx21: parsing VEND failed %d\n", ret);
	} else {
		switch (vendor->vic) {
		case 1:
			hdmi4k_vic = HDMI_95_3840x2160p30_16x9;
			break;
		case 2:
			hdmi4k_vic = HDMI_94_3840x2160p25_16x9;
			break;
		case 3:
			hdmi4k_vic = HDMI_93_3840x2160p24_16x9;
			break;
		case 4:
			hdmi4k_vic = HDMI_98_4096x2160p24_256x135;
			break;
		default:
			break;
		}
	}
	return hdmi4k_vic;
}

static void hdmi_hwp_init(struct hdmitx_dev *hdev, u8 reset)
{
	u32 data32;

	if (hdev->data->chip_type >= MESON_CPU_ID_S5) {
		hd21_set_reg_bits(CLKCTRL_VID_CLK0_CTRL, 7, 0, 3);
		hd21_set_reg_bits(CLKCTRL_ENC_HDMI_CLK_CTRL, 1, 20, 1);
		hd21_set_reg_bits(CLKCTRL_ENC_HDMI_CLK_CTRL, 1, 12, 1);
		hd21_set_reg_bits(CLKCTRL_ENC_HDMI_CLK_CTRL, 1, 4, 1);
		hd21_set_reg_bits(CLKCTRL_HDMI_CLK_CTRL, 7, 8, 3);
		hd21_set_reg_bits(CLKCTRL_HTX_CLK_CTRL1, 1, 24, 1);
	}

	pr_info("%s%d\n", __func__, __LINE__);
	if (!reset && hdmitx21_uboot_already_display(hdev)) {
		int ret;
		u8 body[32] = {0};
		union hdmi_infoframe *infoframe = &hdev->infoframes.avi;
		struct hdmi_avi_infoframe *avi = &infoframe->avi;
		const struct hdmi_timing *tp;
		const char *name;
		enum hdmi_vic vic = HDMI_0_UNKNOWN;

		hdev->ready = 1;
		ret = hdmitx_infoframe_rawget(HDMI_INFOFRAME_TYPE_AVI, body);
		if (ret == -1 || ret == 0) {
			pr_info("hdmitx21: AVI not enabled %d\n", ret);
			return;
		}
		ret = hdmi_infoframe_unpack(infoframe, body, sizeof(body));
		if (ret < 0) {
			pr_info("hdmitx21: parsing AVI failed %d\n", ret);
		} else {
			if (!hdev->para)
				return;
			hdev->frl_rate = get_current_frl_rate();
			if (hdev->para) {
				hdev->para->cs = avi->colorspace;
				hdev->para->cd = _get_colordepth();
				if (hdev->para->cs == HDMI_COLORSPACE_YUV422)
					hdev->para->cd = COLORDEPTH_36B;
				hdmitx21_fmt_attr(hdev);
				vic = avi->video_code;
				if (vic == HDMI_0_UNKNOWN)
					vic = _get_vic_from_vsif(hdev);
				hdev->cur_VIC = vic;
				tp = hdmitx21_gettiming_from_vic(vic);
				if (tp) {
					name = tp->sname ? tp->sname : tp->name;
					hdev->para = hdmitx21_get_fmtpara(name,
						hdev->fmt_attr);
				}
			} else {
				pr_info("hdmitx21: failed to get para\n");
				hdev->para->cs = HDMI_COLORSPACE_YUV444;
				hdev->para->cd = COLORDEPTH_24B;
			}
			pr_info("hdmitx21: parsing AVI CS%d CD%d VIC%d AVI_VIC%d\n",
				avi->colorspace, hdev->para->cd, hdev->cur_VIC, avi->video_code);
		}
		return;
	}

	if (hdev->data->chip_type == MESON_CPU_ID_T7) {
		// --------------------------------------------------------
		// Program core_pin_mux to enable HDMI pins
		// --------------------------------------------------------
		// [23:20] GPIOW_13_SEL=1 for hdmitx_sda
		hd21_set_reg_bits(PADCTRL_PIN_MUX_REGN, 1, 20, 4);
		// [27:24] GPIOW_14_SEL=1 for hdmitx_scl
		hd21_set_reg_bits(PADCTRL_PIN_MUX_REGN, 1, 24, 4);
		// [31:28] GPIOW_15_SEL=1 for hdmitx_hpd
		hd21_set_reg_bits(PADCTRL_PIN_MUX_REGN, 1, 28, 4);
	}
	hdmitx21_set_default_clk();    // set MPEG, audio and default video
	// [8]      hdcp_topology_err
	// [7]      rxsense_fall
	// [6]      rxsense_rise
	// [5]      err_i2c_timeout_pls
	// [4]      hs_intr
	// [3]      core_aon_intr_rise
	// [2]      hpd_fall
	// [1]      hpd_rise
	// [0]      core_pwd_intr_rise
	hdmitx21_wr_reg(HDMITX_TOP_INTR_STAT_CLR, 0x000001ff);

	// Enable internal pixclk, tmds_clk, spdif_clk, i2s_clk, cecclk
	// [   31] free_clk_en
	// [   13] aud_mclk_sel: 0=Use i2s_mclk; 1=Use spdif_clk. For ACR.
	// [   12] i2s_ws_inv
	// [   11] i2s_clk_inv
	// [    9] tmds_clk_inv
	// [    8] pixel_clk_inv
	// [    3] i2s_clk_enable
	// [    2] tmds_clk_enable
	// [    1] tmds_clk_enable
	// [ 0] pixel_clk_enable
	data32 = 0;
	data32 |= (0 << 31);
	data32 |= ((1 - 0) << 13);
	data32 |= (0 << 12);
	data32 |= (0 << 11);
	data32 |= (0 << 9);
	data32 |= (0 << 8);
	data32 |= (1 << 3);
	data32 |= (1 << 2);
	data32 |= (1 << 1);
	data32 |= (1 << 0);
	hdmitx21_wr_reg(HDMITX_TOP_CLK_CNTL,  data32);
	data32 = 0;
	data32 |= (1 << 8);  // [  8] hdcp_topology_err
	data32 |= (1 << 7);  // [  7] rxsense_fall
	data32 |= (1 << 6);  // [  6] rxsense_rise
	data32 |= (1 << 5);  // [  5] err_i2c_timeout_pls
	data32 |= (1 << 4);  // [  4] hs_intr
	data32 |= (1 << 3);  // [  3] core_aon_intr_rise
	data32 |= (1 << 2);  // [  2] hpd_fall_intr
	data32 |= (1 << 1);  // [  1] hpd_rise_intr
	data32 |= (1 << 0);  // [ 0] core_pwd_intr_rise
	hdmitx21_set_bit(HDMITX_TOP_INTR_MASKN, BIT(2) | BIT(1), 1);

	//--------------------------------------------------------------------------
	// Configure E-DDC interface
	//--------------------------------------------------------------------------
	data32 = 0;
	data32 |= (1 << 24); // [26:24] infilter_ddc_intern_clk_divide
	data32 |= (0 << 16); // [23:16] infilter_ddc_sample_clk_divide
	hdmitx21_wr_reg(hdev->data->chip_type == MESON_CPU_ID_T7 ?
		HDMITX_T7_TOP_INFILTER : HDMITX_S5_TOP_INFILTER, data32);
	hdmitx21_set_reg_bits(AON_CYP_CTL_IVCTX, 2, 0, 2);
	hdmitx21_set_reg_bits(PCLK2TMDS_MISC0_IVCTX, 0, 0, 2); /* Original DE generation logic */
	/*Control signals for repeat count */
	hdmitx21_set_reg_bits(HBLANK_REKEY_CONTROL_IVCTX, 1, 6, 1);
}

int hdmitx21_uboot_audio_en(void)
{
	u32 data;

	data = hdmitx21_rd_reg(AUD_EN_IVCTX);
	pr_info("%s[%d] data = 0x%x\n", __func__, __LINE__, data);
	if (data & 1)
		return 1;
	return 0;
}

void hdmitx21_meson_init(struct hdmitx_dev *hdev)
{
	pr_info("%s%d\n", __func__, __LINE__);
	hdev->hwop.setdispmode = hdmitx_set_dispmode;
	hdev->hwop.setaudmode = hdmitx_set_audmode;
	hdev->hwop.debugfun = hdmitx_debug;
	hdev->hwop.debug_bist = hdmitx_debug_bist;
	hdev->hwop.uninit = hdmitx_uninit;
	hdev->hwop.cntl = hdmitx_cntl;	/* todo */
	hdev->hwop.cntlddc = hdmitx_cntl_ddc;
	hdev->hwop.getstate = hdmitx_get_state;
	hdev->hwop.cntlpacket = hdmitx_cntl;
	hdev->hwop.cntlconfig = hdmitx_cntl_config;
	hdev->hwop.cntlmisc = hdmitx_cntl_misc;
	hdmi_hwp_init(hdev, 0);
	hdmitx21_debugfs_init();
	hdev->hwop.cntlmisc(hdev, MISC_AVMUTE_OP, CLR_AVMUTE);
}

void phy_hpll_off(void)
{
	hdmi_phy_suspend();
}

static void hdmitx_phy_pre_init(struct hdmitx_dev *hdev)
{
	if (!hdev || !hdev->data)
		return;

	/* only need for s5 or later */
	if (hdev->data->chip_type <= MESON_CPU_ID_T7)
		return;
	hdmitx_s5_phy_pre_init(hdev);
}

static void set_phy_by_mode(u32 mode)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();
	u32 tmds_clk = 0;

	switch (hdev->data->chip_type) {
	case MESON_CPU_ID_T7:
		set21_phy_by_mode_t7(mode);
		break;
	case MESON_CPU_ID_S5:
		tmds_clk = hdev->para->tmds_clk;
		pr_info("%s[%d] tmds_clk %d\n", __func__, __LINE__, tmds_clk);
		hdmitx_set_s5_phypara(hdev->frl_rate, tmds_clk);
		break;
	default:
		pr_info("%s: Not match chip ID\n", __func__);
		break;
	}
}

static void hdmitx_set_phy(struct hdmitx_dev *hdev)
{
	u32 phy_addr = 0;

	if (!hdev)
		return;

	phy_addr = ANACTRL_HDMIPHY_CTRL0;
	hd21_write_reg(phy_addr, 0x0);

	phy_addr = ANACTRL_HDMIPHY_CTRL1;

/* P_HHI_HDMI_PHY_CNTL1 bit[1]: enable clock	bit[0]: soft reset */
#define RESET_HDMI_PHY() \
do { \
	hd21_set_reg_bits(phy_addr, 0xf, 0, 4); \
	mdelay(2); \
	hd21_set_reg_bits(phy_addr, 0xe, 0, 4); \
	mdelay(2); \
} while (0)

	hd21_set_reg_bits(phy_addr, 0x0390, 16, 16);
	hd21_set_reg_bits(phy_addr, 0x1, 17, 1);
	hd21_set_reg_bits(phy_addr, 0x0, 17, 1);
	hd21_set_reg_bits(phy_addr, 0x0, 0, 4);
	msleep(20);
	RESET_HDMI_PHY();
	RESET_HDMI_PHY();
	RESET_HDMI_PHY();
#undef RESET_HDMI_PHY

	if (hdev->para->tmds_clk > 340000)
		set_phy_by_mode(HDMI_PHYPARA_6G);
	else if (hdev->para->tmds_clk > 290000)
		set_phy_by_mode(HDMI_PHYPARA_3G);
	else
		set_phy_by_mode(HDMI_PHYPARA_DEF);
}

/* vid_pll_clk for master clk
 * RA bit[18:16] source 0: vid_pll0_clk
 * RG[7:0] XD0
 * RA[2:0] DIV4/2/1_EN
 */
static void set_vid_clk_div(u32 div)
{
	hd21_set_reg_bits(CLKCTRL_VID_CLK0_CTRL, 0, 16, 3);
	hd21_set_reg_bits(CLKCTRL_VID_CLK0_DIV, div - 1, 0, 8);
	hd21_set_reg_bits(CLKCTRL_VID_CLK0_CTRL, 7, 0, 3);
}

/* RD bit[5]:pixel clk gate enable */
static void set_hdmi_tx_pixel_div(u32 div)
{
	hd21_set_reg_bits(CLKCTRL_VID_CLK0_CTRL2, 1, 5, 1);
}

/* RG [27:24] ENCP_CLK_SEL no need for S5? S5 use RE[15:12] as div
 * RA bit[19]: CLK_EN0
 * todo
 */
static void set_encp_div(u32 div)
{
	hd21_set_reg_bits(CLKCTRL_VID_CLK0_DIV, div, 24, 4);
	hd21_set_reg_bits(CLKCTRL_VID_CLK0_CTRL, 1, 19, 1);
}

/* RD bit[2] ENCP CLK GATE, but for clk tree of S5,
 * RD bit[3] is for gate of ENC, not bit[2].
 * todo
 */
static void hdmitx_enable_encp_clk(void)
{
	hd21_set_reg_bits(CLKCTRL_VID_CLK0_CTRL2, 1, 2, 1);
}

static void set_hdmitx_fe_clk(void)
{
	u32 tmp = 0;
	u32 vid_clk_cntl2;
	u32 vid_clk_div;
	u32 hdmi_clk_cntl;

	/* RD bit[9]: Gclk_hdmi_tx_fe_clk */
	vid_clk_cntl2 = CLKCTRL_VID_CLK0_CTRL2;
	/* RG [27:24] ENCP_CLK_SEL no need for S5? */
	vid_clk_div = CLKCTRL_VID_CLK0_DIV;
	/* register wrong for S5, CLKCTRL_ENC0_HDMI_CLK_CTRL?
	 * CLKCTRL_HDMI_CLK_CTRL bit[23:20] reserved in S5
	 */
	hdmi_clk_cntl = CLKCTRL_HDMI_CLK_CTRL;

	hd21_set_reg_bits(vid_clk_cntl2, 1, 9, 1);
	/* divider same as encp */
	tmp = (hd21_read_reg(vid_clk_div) >> 24) & 0xf;
	hd21_set_reg_bits(hdmi_clk_cntl, tmp, 20, 4);
}

static void _hdmitx21_set_clk(void)
{
	set_vid_clk_div(1);
	set_hdmi_tx_pixel_div(1);
	set_encp_div(1);
	hdmitx_enable_encp_clk();
	set_hdmitx_fe_clk();
}

void enable_crt_video_encl2(u32 enable, u32 in_sel)
{
	//encl_clk_sel:hi_viid_clk_div[15:12]
	hd21_set_reg_bits(CLKCTRL_VIID_CLK2_DIV, in_sel, 12, 4);
	if (in_sel <= 4) { //V1
	//#if (SDF_CORNER == 0 || SDF_CORNER == 2)    //ss_corner
	//      hd21_set_reg_bits(CLKCTRL_VID_CLK_CTRL, 1, 16, 3);  //sel div4 : 500M
	//#endif
		hd21_set_reg_bits(CLKCTRL_VID_CLK2_CTRL, 3, 0, 2);
	} else {
		hd21_set_reg_bits(CLKCTRL_VIID_CLK2_CTRL, 1, in_sel - 8, 1);
	}
	//gclk_encl_clk:hi_vid_clk_cntl2[3]
	hd21_set_reg_bits(CLKCTRL_VID_CLK2_CTRL2, enable, 3, 1); /* cts_enc2_clk */
}

//Enable CLK_ENCL
void enable_crt_video_encl(u32 enable, u32 in_sel)
{
	/* RE bit[15:12] encl_clk_sel, 0: vid_pll0_clk */
	//encl_clk_sel:hi_viid_clk_div[15:12]
	hd21_set_reg_bits(CLKCTRL_VIID_CLK0_DIV, in_sel,  12, 4);
	if (in_sel <= 4) { //V1
		//#if (SDF_CORNER == 0 || SDF_CORNER == 2)    //ss_corner
		//      hd21_set_reg_bits(CLKCTRL_VID_CLK_CTRL, 1, 16, 3);  //sel div4 : 500M
		//#endif
		/* RA bit[1:0] div2/1 */
		hd21_set_reg_bits(CLKCTRL_VID_CLK0_CTRL, 1, in_sel, 1);
	} else {
		hd21_set_reg_bits(CLKCTRL_VIID_CLK0_CTRL, 1, in_sel - 8, 1);
	}
	/* RD bit[3]: ENCL gated clock */
	//gclk_encl_clk:hi_vid_clk_cntl2[3]
	hd21_set_reg_bits(CLKCTRL_VID_CLK0_CTRL2, enable, 3, 1);
}

//Enable CLK_ENCP, no use for S5, already configure previously
void enable_crt_video_encp(u32 enable, u32 in_sel)
{
	enable_crt_video_encl(enable, in_sel);
}

/* check the h_total with depth
 * for example, VIC4, 720p60hz
 * htotal will be 1650/8bit, 2062.5/10bit, 2475/12bit under tmds
 * htotal will be 825/8bit, 1031.25/10bit, 1237.5/12bit under frl
 * which will has the fraction.
 * Under such case, the GCP phase will be dynamic value
 */
static bool is_deep_htotal_frac(bool frl_mode, u32 h_total,
	enum hdmi_colorspace cs, enum hdmi_color_depth cd)
{
	if (frl_mode) {
		if (cs == HDMI_COLORSPACE_YUV420) {
			if (cd == COLORDEPTH_24B) {
				if (h_total % 4)
					return 1;
			} else if (cd == COLORDEPTH_30B) {
				if (h_total * 5 % 16)
					return 1;
			} else if (cd == COLORDEPTH_36B) {
				if (h_total * 3 % 8)
					return 1;
			}
		} else if (cs == HDMI_COLORSPACE_YUV444 || cs == HDMI_COLORSPACE_RGB) {
			if (cd == COLORDEPTH_24B) {
				if (h_total % 2)
					return 1;
			} else if (cd == COLORDEPTH_30B) {
				if (h_total * 5 % 8)
					return 1;
			} else if (cd == COLORDEPTH_36B) {
				if (h_total * 3 % 4)
					return 1;
			}
		} else if (cs == HDMI_COLORSPACE_YUV422) {
			if (h_total % 2)
				return 1;
		}
	} else {
		if (cs == HDMI_COLORSPACE_YUV420) {
			if (cd == COLORDEPTH_24B) {
				if (h_total % 2)
					return 1;
			} else if (cd == COLORDEPTH_30B) {
				if (h_total * 5 % 8)
					return 1;
			} else if (cd == COLORDEPTH_36B) {
				if (h_total * 3 % 4)
					return 1;
			}
		} else if (cs == HDMI_COLORSPACE_YUV444 || cs == HDMI_COLORSPACE_RGB) {
			if (cd == COLORDEPTH_24B) {
				return 0;
			} else if (cd == COLORDEPTH_30B) {
				if (h_total * 5 % 4)
					return 1;
			} else if (cd == COLORDEPTH_36B) {
				if (h_total * 3 % 2)
					return 1;
			}
		} else if (cs == HDMI_COLORSPACE_YUV422) {
			return 0;
		}
	}
	return 0;
}

static bool is_deep_phase_unstable(enum hdmi_colorspace cs, enum hdmi_color_depth cd)
{
	u8 gcp_cur_st = (hdmitx21_rd_reg(GCP_CUR_STAT_IVCTX) >> 5) & 0x3;

	pr_info("%s[%d] gcp_cur_st %d\n", __func__, __LINE__, gcp_cur_st);
	if (cs == HDMI_COLORSPACE_YUV422) {
		if (gcp_cur_st != 0)
			return 1;
	} else {
		if (cd == COLORDEPTH_36B) {
			if (gcp_cur_st != 0x2)
				return 1;
		} else {
			if (gcp_cur_st)
				return 1;
		}
	}

	return 0;
}

//Enable HDMI_TX_PIXEL_CLK
//Note: when in_sel == 15, select tcon_clko
void enable_crt_video_hdmi(u32 enable, u32 in_sel, u8 enc_sel)
{
	u32 data32;
	u32 addr_vid_clk02;
	u32 addr_viid_clk02;
	u32 addr_vid_clk022;
	struct hdmitx_dev *hdev = get_hdmitx21_device();
	struct hdmi_format_para *para = hdev->para;

	addr_vid_clk02 = (enc_sel == 0) ? CLKCTRL_VID_CLK0_CTRL : CLKCTRL_VID_CLK2_CTRL;
	addr_viid_clk02 = (enc_sel == 0) ? CLKCTRL_VIID_CLK0_CTRL : CLKCTRL_VIID_CLK2_CTRL;
	addr_vid_clk022 = (enc_sel == 0) ? CLKCTRL_VID_CLK0_CTRL2 : CLKCTRL_VID_CLK2_CTRL2;

	if (in_sel <= 4) { //V1
		if (in_sel == 1)
			// If 420 mode, need to turn on div1_clk for hdmi_tx_fe_clk
			// For hdmi_tx_fe_clk and hdmi_tx_pnx_clk
			hd21_set_reg_bits(addr_vid_clk02, 3, 0, 2);
	} else if (in_sel <= 9) { //V2
		// For hdmi_tx_pixel_clk
		hd21_set_reg_bits(addr_viid_clk02, 1, in_sel - 8, 1);
	}

	/* RD bit[10,9,5] for pnx/fe/pixel clk gate */
	// Enable hdmi_tx_pnx_clk
	hd21_set_reg_bits(addr_vid_clk022, enable, 10, 1);
	// Enable hdmi_tx_fe_clk
	hd21_set_reg_bits(addr_vid_clk022, enable, 9, 1);
	// Enable hdmi_tx_pixel_clk
	hd21_set_reg_bits(addr_vid_clk022, enable, 5, 1);

	/* RF */
	// [22:21] clk_sl: 0=enc0_hdmi_tx_pnx_clk, 1=enc2_hdmi_tx_pnx_clk.
	// [   20] clk_en for hdmi_tx_pnx_clk
	// [19:16] clk_div for hdmi_tx_pnx_clk
	// [14:13] clk_sl: 0=enc0_hdmi_tx_fe_clk, 1=enc2_hdmi_tx_fe_clk.
	// [   12] clk_en for hdmi_tx_fe_clk
	// [11: 8] clk_div for hdmi_tx_fe_clk
	// [ 6: 5] clk_sl: 0=enc0_hdmi_tx_pixel_clk, 1=enc2_hdmi_tx_pixel_clk.
	// [    4] clk_en for hdmi_tx_pixel_clk
	// [ 3: 0] clk_div for hdmi_tx_pixel_clk

	/* this bit13 of CLKCTRL_ENC_HDMI_CLK_CTRL will select source from
	 * enc0_hdmi_tx_fe_clk, while it need to select source from gp2
	 */
	data32 = 0;
	data32 = (enc_sel << 21) |
		 (0 << 20) |
		 (0 << 16) |
		 (enc_sel << 13) |
		 (0 << 12) |
		 (0 << 8) |
		 (enc_sel << 5) |
		 (0 << 4) |
		 (0 << 0);
	if (hdev->data->chip_type >= MESON_CPU_ID_S5 &&
	    (para->cs == HDMI_COLORSPACE_YUV420 && !hdev->frl_rate)) {
		data32 |= (1 << 0); /* pixel_clk DIV */
		data32 |= (1 << 8); /* fe_clk DIV */
		data32 |= (1 << 16); /* pnx_clk DIV */
	}

	hd21_write_reg(CLKCTRL_ENC_HDMI_CLK_CTRL, data32);
	hd21_set_reg_bits(CLKCTRL_ENC_HDMI_CLK_CTRL, 1, 20, 1);
	hd21_set_reg_bits(CLKCTRL_ENC_HDMI_CLK_CTRL, 1, 12, 1);
	hd21_set_reg_bits(CLKCTRL_ENC_HDMI_CLK_CTRL, 1, 4, 1);
}   // enable_crt_video_hdmi

//Enable CLK_ENCP
void enable_crt_video_encp2(u32 enable, u32 in_sel)
{
	enable_crt_video_encl2(enable, in_sel);
}

/* master_clk selects source from vid_pll0_clk or vid_pll1_clk(fpll_pixel_clk)
 * under FRL mode, no matter DSC or non-DSC, select fpll_pixel_clk
 * it's used for pixel/pnx/enc/fe clk under frl & non-DSC mode;
 * and used for pixel/pnx clk under frl & DSC mode (enc_clk mux to v2_master_clk
 * and fe_clk mux to GP2_pll, refer to hdmitx_mux_gp2_pll);
 */
static void hdmitx_mux_vid_pll_clk(struct hdmitx_dev *hdev)
{
	/* RA bit[18:16] vid_pll_clk source: 0 vid_pll0_clk, 4 vid_pll1_clk */
	hd21_set_reg_bits(CLKCTRL_VID_CLK0_CTRL, hdev->frl_rate ? 4 : 0, 16, 3);
}

/* In the DSC mode, the v2_master_clk will choose gp2_pll_clk */
/* this mux is for enc_clk & fe_clk
 * note enc_clk need go through CRT_VIDEO, while fe_clk can
 * directly mux to gp2_pll
 */
static void hdmitx_mux_gp2_pll(struct hdmitx_dev *hdev)
{
	/* RB bit[18:16] V2_cntl_clk_in_sel
	 * bit[19] V2_cntl_clk_en0, bit[0]:V2_cntl_div1_en
	 * select gp2_pll_clk->v2_master_clk->enc_clk div1
	 */
	hd21_set_reg_bits(CLKCTRL_VIID_CLK0_CTRL, 1, 16, 3);
	hd21_set_reg_bits(CLKCTRL_VIID_CLK0_CTRL, 1, 19, 1);
	hd21_set_reg_bits(CLKCTRL_VIID_CLK0_CTRL, 1, 0, 1);

	/* RE bit[15:12] enc_clk mux to v2_master_clk
	 * bit[7:0] V2_cntl_xd0
	 * bit[16] V2_cntl_clk_div_en
	 */
	hd21_set_reg_bits(CLKCTRL_VIID_CLK0_DIV, 8, 12, 4);
	hd21_set_reg_bits(CLKCTRL_VIID_CLK0_DIV, 0, 0, 8);
	hd21_set_reg_bits(CLKCTRL_VIID_CLK0_DIV, 1, 16, 1);

	/* RD bit[3] ENC gate enable */
	hd21_set_reg_bits(CLKCTRL_VID_CLK0_CTRL2, 1, 3, 1);

	/* RF fe_clk mux to gp2_pll_clk */
	hd21_set_reg_bits(CLKCTRL_ENC_HDMI_CLK_CTRL, 1, 13, 2); /* mux */
	hd21_set_reg_bits(CLKCTRL_ENC_HDMI_CLK_CTRL, 1, 12, 1); /* en */
	hd21_set_reg_bits(CLKCTRL_ENC_HDMI_CLK_CTRL, 0, 8, 4); /* div */
}

static void set_hdmitx_enc_idx(unsigned int val)
{
	struct arm_smccc_res res;

	arm_smccc_smc(HDCPTX_IOOPR, CONF_ENC_IDX, 1, !!val, 0, 0, 0, 0, &res);
}

static int dfm_type = -1;
module_param(dfm_type, int, 0644);
MODULE_PARM_DESC(dfm_type, "for dfm debug");

/* set to 1 only for cvtem packet test */
static int emp_dbg_en;

void hdmitx_soft_reset(u32 bits_nr)
{
	pr_info("%s[%d]\n", __func__, __LINE__);
	if (bits_nr & BIT(0)) {
		/* 18or10to20 fifos Software reset */
		hdmitx21_reset_reg_bit(PWD_SRST_IVCTX, 2);
	}
	if (bits_nr & BIT(1)) {
		/* Software Reset. Reset all internal logic */
		hdmitx21_reset_reg_bit(PWD_SRST_IVCTX, 0);
	}
	if (bits_nr & BIT(2)) {
		/* reset for the cipher engine */
		hdmitx21_reset_reg_bit(HDCP_CTRL_IVCTX, 2);
	}
	if (bits_nr & BIT(3)) {
		/* HW TPI State Machine Reset */
		hdmitx21_reset_reg_bit(AON_CYP_CTL_IVCTX, 3);
	}
	if (bits_nr & BIT(4)) {
		/* Software Reset for hdcp2x logic only */
		hdmitx21_reset_reg_bit(HDCP2X_TX_SRST_IVCTX, 5);
	}
	if (bits_nr & BIT(5)) {
		/* PCLK to TCLK Video FIFO Software reset */
		hdmitx21_reset_reg_bit(PWD_SRST_IVCTX, 1);
	}
}

static int hdmitx_set_dispmode(struct hdmitx_dev *hdev)
{
	struct hdmi_format_para *para = hdev->para;
	u32 data32;
	enum hdmi_vic vic = para->timing.vic;
	struct vinfo_s *vinfo = NULL;

	fifo_flow_enable_intrs(0);
	/* gp2 setting has been set for fe/enc for dsc*/
	hdmitx21_set_clk(hdev);

	hdmitx_phy_pre_init(hdev);
	if (hdev->data->chip_type == MESON_CPU_ID_T7)
		_hdmitx21_set_clk();
	/* [92]cts_htx_tmds clk config */
	hdmitx_set_clkdiv(hdev);
	hdmitx_mux_vid_pll_clk(hdev);

	if (hdev->enc_idx == 2) {
		set_hdmitx_enc_idx(2);
		hd21_set_reg_bits(VPU_DISP_VIU2_CTRL, 1, 29, 1);
		//hd21_set_reg_bits(VPU_VIU_VENC_MUX_CTRL, 2, 2, 2);
	}
	/* dsc program step8.1: Make sure VENC timing gen is disabled. */
	hdmitx21_venc_en(0, 0);
	hdmitx21_venc_en(0, 1);
	hd21_set_reg_bits(VPU_HDMI_SETTING, 0, (hdev->enc_idx == 0) ? 0 : 1, 1);
	/* dsc program step8.3: Program VPU/HDMI setting. */
	/* below crt_video_encp* is ENC for T7 */
	if (hdev->data->chip_type == MESON_CPU_ID_T7) {
		if (hdev->enc_idx == 0)
			enable_crt_video_encp(1, 0);
		else
			enable_crt_video_encp2(1, 0);
	}
	/* set divider for pixel/pnx/fe clk and gate */
	/* it will override above set for enc/fe for DSC */
	enable_crt_video_hdmi(1,
			      (TX_INPUT_COLOR_FORMAT == HDMI_COLORSPACE_YUV420) ? 1 : 0,
			      hdev->enc_idx);
	if (hdev->dsc_en)
		hdmitx_mux_gp2_pll(hdev);

	// configure GCP
	/* for 8bit depth or y422: non-merge gcp mode + clr_avmute,
	 * for dc mode: merge gcp mode + clr_avmute
	 */
	if (para->cs == HDMI_COLORSPACE_YUV422 ||
		para->cd == COLORDEPTH_24B ||
		hdev->dsc_en) {
		hdmitx21_set_reg_bits(GCP_CNTL_IVCTX, 0, 0, 1);
		/* hdmi_gcppkt_manual_set(1); */
	} else {
		/* hdmi_gcppkt_manual_set(0); */
		hdmitx21_set_reg_bits(GCP_CNTL_IVCTX, 1, 0, 1);
	}
	// --------------------------------------------------------
	// Enable viu vsync interrupt, enable hdmitx interrupt, enable htx_hdcp22 interrupt
	// --------------------------------------------------------
	hd21_write_reg(VPU_VENC_CTRL, 1);
	if (hdev->enc_idx == 2) {
		// Enable VENC2 to HDMITX path
		hd21_set_reg_bits(SYSCTRL_VPU_SECURE_REG0, 1, 16, 1);
	}

	// --------------------------------------------------------
	// Set TV encoder for HDMI
	// --------------------------------------------------------
	pr_info("configure venc\n");
	//              enc_index   output_type enable)
	// only 480i / 576i use the ENCI
	if (para->timing.pi_mode == 0 &&
	    (para->timing.v_active == 480 || para->timing.v_active == 576)) {
		hd21_write_reg(VPU_VENC_CTRL, 0); // sel enci timming
		set_tv_enci_new(hdev, hdev->enc_idx, vic, 1);
	} else {
		hd21_write_reg(VPU_VENC_CTRL, 1); // sel encp timming
		set_tv_encp_new(hdev, hdev->enc_idx, vic, 1);
	}

	// --------------------------------------------------------
	// Configure video format timing for HDMI:
	// Based on the corresponding settings in set_tv_enc.c, calculate
	// the register values to meet the timing requirements defined in CEA-861-D
	// --------------------------------------------------------
	pr_info("configure hdmitx video format timing\n");

	// [ 1: 0] hdmi_vid_fmt. 0=444; 1=convert to 422; 2=convert to 420.
	// [ 3: 2] chroma_dnsmp_h. 0=use pixel 0; 1=use pixel 1; 2=use average.
	// [    4] dith_en. 1=enable dithering before HDMI TX input.
	// [    5] hdmi_dith_md: random noise selector.
	// [ 9: 6] hdmi_dith10_cntl.
	// [   10] hdmi_round_en. 1= enable 12-b rounded to 10-b.
	// [   11] tunnel_en
	// [21:12] hdmi_dith_new
	// [23:22] chroma_dnsmp_v. 0=use line 0; 1=use line 1; 2=use average.
	// [27:24] pix_repeat
	data32 = 0;
	data32 = (((para->cs == HDMI_COLORSPACE_YUV420) ? 2 :
		  (para->cs == HDMI_COLORSPACE_YUV422) ? 1 : 0) << 0) |
		  (2 << 2) |
		  (0 << 4) |
		  (0 << 5) |
		  (0 << 6) |
		  (((para->cd == COLORDEPTH_24B) ? 1 : 0) << 10) |
		  (0 << 11) |
		  (0 << 12) |
		  (2 << 22) |
		  (0 << 24);
	if (hdev->data->chip_type >= MESON_CPU_ID_S5 &&
	    (hdev->frl_rate && para->cs == HDMI_COLORSPACE_YUV420))
		data32 |= 3 << 0; // 3:420 dual port
	hd21_write_reg(VPU_HDMI_FMT_CTRL, data32);

	// [    2] inv_hsync_b
	// [    3] inv_vsync_b
	// [    4] hdmi_dith_en_b. For 10-b to 8-b.
	// [    5] hdmi_dith_md_b. For 10-b to 8-b.
	// [ 9: 6] hdmi_dith10_b. For 10-b to 8-b.
	// [   10] hdmi_round_en_b. For 10-b to 8-b.
	// [21:12] hdmi_dith_new_b. For 10-b to 8-b.
	data32 = 0;
	data32 = (0 << 2) |
		(0 << 3) |
		(0 << 4) |
		(0 << 5) |
		(0 << 6) |
		(((para->cd == COLORDEPTH_24B) ? 1 : 0) << 10) |
		(0 << 12);
	if (para->cd == COLORDEPTH_24B && hdmitx21_dv_en() == 0)
		data32 |= (1 << 4);
	hd21_write_reg(VPU_HDMI_DITH_CNTL, data32);
	hdmitx21_dither_config(hdev);
	if (hdev->data->chip_type >= MESON_CPU_ID_S5)
		hdmi_hwp_init(hdev, 1);

	// Set this timer very small on purpose, to test the new function
	hdmitx21_wr_reg(HDMITX_TOP_I2C_BUSY_CNT_MAX,  30);

	data32 = 0;
	data32 |= (1 << 31); // [   31] cntl_hdcp14_min_size_v_en
	data32 |= (240 << 16); // [28:16] cntl_hdcp14_min_size_v
	data32 |= (1 << 15); // [   15] cntl_hdcp14_min_size_h_en
	data32 |= (640 << 0);  // [13: 0] cntl_hdcp14_min_size_h
	hdmitx21_wr_reg(HDMITX_TOP_HDCP14_MIN_SIZE, data32);

	data32 = 0;
	data32 |= (1 << 31); // [   31] cntl_hdcp22_min_size_v_en
	data32 |= (1080 << 16); // [28:16] cntl_hdcp22_min_size_v
	data32 |= (1 << 15); // [   15] cntl_hdcp22_min_size_h_en
	data32 |= (1920 << 0);  // [13: 0] cntl_hdcp22_min_size_h
	hdmitx21_wr_reg(HDMITX_TOP_HDCP22_MIN_SIZE, data32);

	hdev->cur_VIC = hdev->para->timing.vic;

	hdmitx_soft_reset(BIT(1) | BIT(2) | BIT(3) | BIT(4));
	//[4] reg_bypass_video_path
	// For non-DSC, set to bit4 as 0
	if (hdev->dsc_en)
		hdmitx21_set_reg_bits(PCLK2TMDS_MISC1_IVCTX, 1, 4, 1);
	else
		hdmitx21_set_reg_bits(PCLK2TMDS_MISC1_IVCTX, 0, 4, 1);

	if (hdev->data->chip_type == MESON_CPU_ID_S5) {
		/* block hsync, this is need to enable when in DSC mode */
		if (hdev->dsc_en)
			hdmitx21_set_reg_bits(H21TXSB_CTRL_1_IVCTX, 1, 2, 1);
		else
			hdmitx21_set_reg_bits(H21TXSB_CTRL_1_IVCTX, 0, 2, 1);
	}
	hdmitx_set_hw(hdev);

	/* dsc program step8.4: Configure VENC timing gen to be slave mode
	 * (receive hs/vs sync signal from DSC encoder timing gen)
	 */
	if (hdev->dsc_en)
		hd21_set_reg_bits(ENCP_VIDEO_SYNC_MODE, 1, 4, 1);
	else
		hd21_set_reg_bits(ENCP_VIDEO_SYNC_MODE, 0, 4, 1);

	/* move enable venc to the end */
	/* if (para->timing.pi_mode == 0 && */
		/* (para->timing.v_active == 480 || para->timing.v_active == 576)) */
		/* hdmitx21_venc_en(1, 0); */
	/* else */
		/* hdmitx21_venc_en(1, 1); */

	// [    0] src_sel_enci
	// [    1] src_sel_encp
	// [    2] inv_hsync. 1=Invert Hsync polarity.
	// [    3] inv_vsync. 1=Invert Vsync polarity.
	// [    4] inv_dvi_clk. 1=Invert clock to external DVI,
	//         (clock invertion exists at internal HDMI).
	// YUV420. Output Y1Y0C to hdmitx.
	// YUV444/422/RGB. Output CrYCb, CY0, or RGB to hdmitx.
	// [ 7: 5] comp_map_post. Data from vfmt is CrYCb(444), CY0(422), CY0Y1(420) or RGB,
	//         map the data to desired format before go to hdmitx:
	// 0=output {2, 1,0};
	// 1=output {1,0,2};
	// 2=output {1,2,0};
	// 3=output {0,2,1};
	// 4=output {0, 1,2};
	// 5=output {2.0,1};
	// 6,7=Rsrv.
	// [11: 8] wr_rate_pre. 0=A write every clk1; 1=A write every 2 clk1; ...;
	//                      15=A write every 16 clk1.
	// [15:12] rd_rate_pre. 0=A read every clk2; 1=A read every 2 clk2; ...;
	//                      15=A read every 16 clk2.
	// RGB. Output RGB to vfmt.
	// YUV. Output CrYCb to vfmt.
	// [18:16] comp_map_pre. Data from VENC is YCbCr or RGB, map data to desired
	//                       format before go to vfmt:
	// 0=output YCbCr(RGB);
	// 1=output CbCrY(GBR);
	// 2=output CbYCr(GRB);
	// 3=output CrYCb(BRG);
	// 4=output CrCbY(BGR);
	// 5=output YCrCb(RBG);
	// 6,7=Rsrv.
	// [23:20] wr_rate_post. 0=A write every clk1; 1=A write every 2 clk1; ...;
	//                       15=A write every 16 clk1.
	// [27:24] rd_rate_post. 0=A read every clk2; 1=A read every 2 clk2; ...;
	//                       15=A read every 16 clk2.
	data32 = 0;
	data32 = (0 << 0) |
		 (0 << 1) |
		 (para->timing.h_pol << 2) |
		 (para->timing.v_pol << 3) |
		 (0 << 4) |
		 (((para->cs == HDMI_COLORSPACE_YUV420) ? 4 : 0) << 5) |
		 (0 << 8) |
		 (0 << 12) |
		 (((TX_INPUT_COLOR_FORMAT == HDMI_COLORSPACE_RGB) ? 0 : 3) << 16) |
		 (((para->cs == HDMI_COLORSPACE_YUV420) ? 1 : 0) << 20) |
		 (0 << 24) |
		 (0 << 28);
	if (hdev->data->chip_type >= MESON_CPU_ID_S5 && hdev->frl_rate) {
		if (para->cs == HDMI_COLORSPACE_YUV420)
			data32 |= 2 << 28;
		else
			data32 |= 1 << 28;
	}
	hd21_write_reg(VPU_HDMI_SETTING, data32);
	if (hdev->data->chip_type >= MESON_CPU_ID_S5 && hdev->para->cs == HDMI_COLORSPACE_RGB)
		hd21_set_reg_bits(VPU_HDMI_SETTING, 4, 16, 3);
	// [    1] src_sel_encp: Enable ENCI or ENCP output to HDMI
	/* move enable de-couple fifo to the end */
	/* hd21_set_reg_bits(VPU_HDMI_SETTING, 1, (hdev->enc_idx == 0) ? 0 : 1, 1); */
	if (hdev->data->chip_type >= MESON_CPU_ID_S5) {
		if (hdev->frl_rate) {
			hd21_set_reg_bits(VPU_HDMI_SETTING, 0, 20, 4);
			hd21_set_reg_bits(VPU_HDMI_SETTING, 0, 24, 4);
			if (hdev->para->cs != HDMI_COLORSPACE_YUV420)
				hd21_set_reg_bits(VPU_HDMI_SETTING, 1, 1, 1);
		}
		hd21_set_reg_bits(VPU_HDMI_SETTING, 0, 16, 3);	//hard code
		if (!hdev->frl_rate && hdev->para->cs == HDMI_COLORSPACE_YUV420) {
			hd21_set_reg_bits(VPU_HDMI_SETTING, 0, 20, 8);
			hd21_set_reg_bits(VPU_HDMI_SETTING, 1, 8, 8);
		}
	}
	if (hdev->dsc_en) {
		/* for dsc y420/y444, no need comp_map_post */
		hd21_set_reg_bits(VPU_HDMI_SETTING, 0, 5, 3);
		if (hdev->para->cs == HDMI_COLORSPACE_YUV422)
			hd21_set_reg_bits(VPU_HDMI_SETTING, 5, 5, 3);
		/* no up_sample and data split */
		hd21_set_reg_bits(ENCP_VIDEO_MODE_ADV, 0, 0, 3);
		hd21_set_reg_bits(VPU_HDMI_SETTING, 0, 1, 1);
		/* 4ppc to dsc module */
		hd21_set_reg_bits(VPU_HDMI_SETTING, 2, 28, 2);
	}
	/* todo: move to main.c set_disp_mode_auto */
	vinfo = get_current_vinfo();
	if (vinfo) {
		vinfo->cur_enc_ppc = 1;
		if (get_current_frl_rate())
			vinfo->cur_enc_ppc = 4;
		if (hdev->dsc_en) {
			if (hdev->para->cs == HDMI_COLORSPACE_RGB)
				vinfo->vpp_post_out_color_fmt = 1;
			else
				vinfo->vpp_post_out_color_fmt = 0;
		} else {
			vinfo->vpp_post_out_color_fmt = 0;
		}
		pr_info("vpp_post_out_color_fmt: %d\n", vinfo->vpp_post_out_color_fmt);
	}

	/* recommend flow: dsc mux->dsc configure/dsc_enc_en/dsc_tmg_en->venc_enable */
	hd21_set_reg_bits(VPU_HDMI_SETTING, !!hdev->dsc_en, 31, 1);
#ifdef CONFIG_AMLOGIC_DSC
	if (hdev->dsc_en) {
		hdmitx_dsc_cvtem_pkt_send(&hdev->dsc_data.pps_data, &hdev->para->timing);
		/* dsc program step8.5: Program DSC settings. */
		/* dsc program step8.6: Enable DSC encoder timing gen 'reg_tmg_en' */
		aml_dsc_enable(true);
	}

	if (emp_dbg_en) {
		/* only for cvtem packet debug test */
		hdmitx_get_dsc_data(&hdev->dsc_data);
		hdmitx_dsc_cvtem_pkt_send(&hdev->dsc_data.pps_data, &hdev->para->timing);
	}
#endif

	/* enable venc */
	hd21_set_reg_bits(VPU_HDMI_SETTING, 1, (hdev->enc_idx == 0) ? 0 : 1, 1);
	usleep_range(1000, 1005);
	if (para->timing.pi_mode == 0 &&
	    (para->timing.v_active == 480 || para->timing.v_active == 576))
		hdmitx21_venc_en(1, 0);
	else
		hdmitx21_venc_en(1, 1);

	/* check the deep color phase */
	{
		enum hdmi_colorspace cs = hdev->para->cs;
		enum hdmi_color_depth cd = hdev->para->cd;
		unsigned int h_total = para->timing.h_total;
		bool h_unstable = 0;
		int loop = 20;

		h_unstable = is_deep_htotal_frac(0, h_total, cs, cd);
		pr_info("%s[%d] frl_rate %d htotal %d cs %d cd %d h_unstable %d\n",
			__func__, __LINE__, get_current_frl_rate(), h_total, cs, cd, h_unstable);
		if (!h_unstable) {
			while (loop--) {
				hdmitx21_set_reg_bits(INTR2_SW_TPI_IVCTX, 0, 1, 1);
				mdelay(1);
				hdmitx21_poll_reg(INTR2_SW_TPI_IVCTX, 1 << 1, ~(1 << 1), HZ / 100);
				if (is_deep_phase_unstable(cs, cd)) {
					/* reset pfifo */
					hdmitx21_set_reg_bits(PWD_SRST_IVCTX, 1, 1, 1);
					hdmitx21_set_reg_bits(PWD_SRST_IVCTX, 0, 1, 1);
					continue;
				} else {
					break;
				}
			}
		}
	}

	hdmitx_set_phy(hdev);
	if (hdev->data->chip_type == MESON_CPU_ID_S5) {
		hdmitx_dfm_cfg(0, 0);

		if (hdev->frl_rate) {
			u32 tri_bytes_per_line = 0;
			bool ret = 0;
			int hc_active = 0;

			if (hdev->dsc_en) {
#ifdef CONFIG_AMLOGIC_DSC
				hc_active = dsc_get_hc_active_by_mode(hdev->dsc_data.dsc_mode);
				/* the pixel_clk is for dual pixel, need double when calculate */
				ret = frl_check_full_bw(HDMI_COLORSPACE_YUV444, COLORDEPTH_24B,
					hdev->dsc_data.cts_hdmi_tx_pixel_clk / 1000 * 2,
					hc_active, hdev->frl_rate, &tri_bytes_per_line);
#endif
			} else {
				ret = frl_check_full_bw(hdev->para->cs, hdev->para->cd,
					hdev->para->timing.pixel_freq, hdev->para->timing.h_active,
					hdev->frl_rate, &tri_bytes_per_line);
			}
			/* manual mode */
			if (dfm_type == 1) {
				hdmitx_dfm_cfg(1, tri_bytes_per_line);
			} else if (dfm_type == 2) {
				hdmitx_dfm_cfg(2, 0);
			} else if (dfm_type == 0) {
				hdmitx_dfm_cfg(0, 0);
			} else {
				if (ret)
					hdmitx_dfm_cfg(1, tri_bytes_per_line);
				else
					hdmitx_dfm_cfg(2, 0);
			}
			pr_info("%s hc_active: %d, need full_bw: %d, tri_bytes_per_line: %d, dfm_type: %d\n",
				__func__, hc_active, ret, tri_bytes_per_line, dfm_type);

		} else {
			fifo_flow_enable_intrs(1);
		}
		if (hdev->rxcap.max_frl_rate && hdev->frl_rate)
			frl_tx_training_handler(hdev);
	}
	return 0;
}

enum hdmi_tf_type hdmitx21_get_cur_hdr_st(void)
{
	int ret;
	u8 body[31] = {0};
	enum hdmi_tf_type mytype = HDMI_NONE;
	enum hdmi_eotf type = HDMI_EOTF_TRADITIONAL_GAMMA_SDR;
	union hdmi_infoframe info;
	struct hdmi_drm_infoframe *drm = &info.drm;

	ret = hdmitx_infoframe_rawget(HDMI_INFOFRAME_TYPE_DRM, body);
	if (ret == -1 || ret == 0) {
		type = HDMI_EOTF_TRADITIONAL_GAMMA_SDR;
	} else {
		ret = hdmi_infoframe_unpack(&info, body, sizeof(body));
		if (ret == 0)
			type = drm->eotf;
	}
	if (type == HDMI_EOTF_SMPTE_ST2084)
		mytype = HDMI_HDR_SMPTE_2084;
	if (type == HDMI_EOTF_BT_2100_HLG)
		mytype = HDMI_HDR_HLG;

	return mytype;
}

/* used for DV VSIF/HDR10+ check
 * note: DV_VSIF/HDR10+_VSIF/HDMI14_VSIF and HF_VSIF use
 * different VSIF per DV CTS spec, use ifdb_present and
 * additional_vsif_num to decide the position of HF_VSIF
 */
static bool hdmitx_vsif_en(u8 *body)
{
	int ret;
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	if (hdev->rxcap.ifdb_present && hdev->rxcap.additional_vsif_num >= 1)
		ret = hdmitx_infoframe_rawget(HDMI_INFOFRAME_TYPE_VENDOR, body);
	else if (!hdev->rxcap.ifdb_present)
		ret = hdmitx_infoframe_rawget(HDMI_INFOFRAME_TYPE_VENDOR2, body);
	else
		ret = hdmitx_infoframe_rawget(HDMI_INFOFRAME_TYPE_VENDOR, body);
	if (ret == -1 || ret == 0)
		return 0;
	else
		return 1;
}

enum hdmi_tf_type hdmitx21_get_cur_dv_st(void)
{
	int ret;
	u8 body[31] = {0};
	enum hdmi_tf_type type = HDMI_NONE;
	union hdmi_infoframe info;
	/* struct hdmi_vendor_infoframe *vend = (struct hdmi_vendor_infoframe *)&info; */
	struct hdmi_avi_infoframe *avi = (struct hdmi_avi_infoframe *)&info;
	unsigned int ieee_code = 0;
	unsigned int size = 0;
	enum hdmi_colorspace cs = 0;

	if (!hdmitx_vsif_en(body))
		return type;

	/* ret = hdmi_infoframe_unpack(&info, body, sizeof(body)); */
	/* if (ret) */
		/* return type; */
	/* ieee_code = vend->oui; */
	/* size = vend->length; */

	/* check Packet type */
	if (body[0] != 0x81)
		return type;

	size = body[2];
	ieee_code = body[4] | (body[5] << 8) | (body[6] << 16);
	/* dolby_vision_signal == body[7] & 0x3 */

	ret = hdmitx_infoframe_rawget(HDMI_INFOFRAME_TYPE_AVI, body);
	if (ret <= 0)
		return type;
	ret = hdmi_infoframe_unpack(&info, body, sizeof(body));
	if (ret)
		return type;
	cs = avi->colorspace;

	if ((ieee_code == HDMI_IEEE_OUI && size == 0x18) ||
	    (ieee_code == DOVI_IEEEOUI && size == 0x1b)) {
		if (cs == HDMI_COLORSPACE_YUV422) /* Y422 */
			type = HDMI_DV_VSIF_LL;
		if (cs == HDMI_COLORSPACE_RGB) /* RGB */
			type = HDMI_DV_VSIF_STD;
	}
	return type;
}

enum hdmi_tf_type hdmitx21_get_cur_hdr10p_st(void)
{
	/* int ret; */
	unsigned int ieee_code = 0;
	u8 body[31] = {0};
	enum hdmi_tf_type type = HDMI_NONE;
	/* union hdmi_infoframe info; */
	/* struct hdmi_vendor_infoframe *vend = (struct hdmi_vendor_infoframe *)&info; */

	if (!hdmitx_vsif_en(body))
		return type;

	/* ret = hdmi_infoframe_unpack(&info, body, sizeof(body)); */
	/* if (ret) */
		/* return type; */
	/* ieee_code = vend->oui; */

	/* check Packet type */
	if (body[0] != 0x81)
		return type;

	ieee_code = body[4] | (body[5] << 8) | (body[6] << 16);
	if (ieee_code == HDR10PLUS_IEEEOUI)
		type = HDMI_HDR10P_DV_VSIF;

	return type;
}

unsigned int hdmitx21_get_vender_infoframe_ieee(void)
{
	unsigned int ieee_code = 0;
	u8 body[31] = {0};

	if (!hdmitx_vsif_en(body))
		return 0;
	/* check Packet type */
	if (body[0] != 0x81)
		return 0;

	ieee_code = body[4] | (body[5] << 8) | (body[6] << 16);
	return ieee_code;
}

bool hdmitx21_hdr_en(void)
{
	return (hdmitx21_get_cur_hdr_st() & HDMI_HDR_TYPE) == HDMI_HDR_TYPE;
}

bool hdmitx21_dv_en(void)
{
	return (hdmitx21_get_cur_dv_st() & HDMI_DV_TYPE) == HDMI_DV_TYPE;
}

bool hdmitx21_hdr10p_en(void)
{
	return (hdmitx21_get_cur_hdr10p_st() & HDMI_HDR10P_TYPE) == HDMI_HDR10P_TYPE;
}

#define GET_OUTCHN_NO(a)	(((a) >> 4) & 0xf)
#define GET_OUTCHN_MSK(a)	((a) & 0xf)

static void set_aud_info_pkt(struct hdmitx_dev *hdev,
	struct hdmitx_audpara *audio_param)
{
	struct hdmi_audio_infoframe *info = &hdev->infoframes.aud.audio;

	hdmi_audio_infoframe_init(info);
	info->channels = audio_param->channel_num + 1;
	if (GET_OUTCHN_NO(hdev->aud_output_ch))
		info->channels = GET_OUTCHN_NO(hdev->aud_output_ch);
	info->channel_allocation = 0;
	/* Refer to Stream Header */
	info->coding_type = 0;
	info->sample_frequency = 0;
	info->sample_size = 0;

	/* Refer to Audio Coding Type (CT) field in Data Byte 1 */
	/* info->coding_type_ext = 0; */
	/* not defined */
	/* info->downmix_inhibit = 0; */
	/* info->level_shift_value = 0; */

	switch (audio_param->type) {
	case CT_MAT:
	case CT_DTS_HD_MA:
		/* CC: 8ch, copy from hdmitx20 */
		/* info->channels = 7 + 1; */
		info->channel_allocation = 0x13;
		break;
	case CT_PCM:
		/* Refer to CEA861-D P90, only even channels */
		switch (audio_param->channel_num + 1) {
		case 2:
			info->channel_allocation = 0;
			break;
		case 4:
			info->channel_allocation = 0x3;
			break;
		case 6:
			info->channel_allocation = 0xb;
			break;
		case 8:
			info->channel_allocation = 0x13;
			break;
		default:
			break;
		}
		switch (GET_OUTCHN_NO(hdev->aud_output_ch)) {
		case 2:
			info->channel_allocation = 0x00;
			break;
		case 4:
			info->channel_allocation =  0x03;
			break;
		case 6:
			info->channel_allocation = 0x0b;
			break;
		case 8:
			info->channel_allocation = 0x13;
			break;
		default:
			break;
		}
		break;
	case CT_DTS:
	case CT_DTS_HD:
	default:
		/* CC: 2ch, copy from hdmitx20 */
		/* info->channels = 1 + 1; */
		info->channel_allocation = 0;
		break;
	}
	hdmi_audio_infoframe_set(info);
}

u32 hdmi21_get_frl_aud_n_paras(enum hdmi_audio_fs fs,
			       u32 frl_rate)
{
	u32 base32k_n = 6048;
	u32 base44k1_n = 5292;
	u32 base48k_n = 5760;

	switch (frl_rate) {
	case 1:
		base32k_n = 6048;
		base44k1_n = 5292;
		base48k_n = 5760;
		break;
	case 2:
	case 3:
		base32k_n = 4032;
		base44k1_n = 5292;
		base48k_n = 6048;
		break;
	case 4:
		base32k_n = 4032;
		base44k1_n = 3969;
		base48k_n = 6048;
		break;
	case 5:
		base32k_n = 3456;
		base44k1_n = 3969;
		base48k_n = 5184;
		break;
	case 6:
		base32k_n = 3072;
		base44k1_n = 3969;
		base48k_n = 4752;
		break;
	default:
		break;
	}
	switch (fs) {
	case FS_32K:
		return base32k_n * 1;
	case FS_44K1:
		return base44k1_n * 1;
	case FS_88K2:
		return base44k1_n * 2;
	case FS_176K4:
		return base44k1_n * 4;
	case FS_48K:
		return base48k_n * 1;
	case FS_96K:
		return base48k_n * 2;
	case FS_192K:
		return base48k_n * 4;
	default:
		return base48k_n;
	}
}

static void set_aud_acr_pkt(struct hdmitx_dev *hdev,
			    struct hdmitx_audpara *audio_param)
{
	u32 aud_n_para;
	u32 char_rate = 0;
	struct hdmi_format_para *para;

	if (!hdev)
		return;

	para = hdev->para;
	if (!para)
		return;

	/* if current mode is 59.94hz not 60hz, char_rate will shift down 0.1% */
	char_rate = hdev->para->timing.pixel_freq;
	if (para && para->timing.name && likely_frac_rate_mode(para->timing.name))
		if (hdev->frac_rate_policy)
			char_rate = char_rate * 1000 / 1001;

	if (hdev->para->cs == HDMI_COLORSPACE_YUV422)
		aud_n_para = hdmi21_get_aud_n_paras(audio_param->sample_rate,
						  COLORDEPTH_24B, char_rate);
	else if (hdev->para->cs == HDMI_COLORSPACE_YUV420)
		aud_n_para = hdmi21_get_aud_n_paras(audio_param->sample_rate,
						  hdev->para->cd, char_rate / 2);
	else
		aud_n_para = hdmi21_get_aud_n_paras(audio_param->sample_rate,
						  hdev->para->cd, char_rate);
	if (hdev->frl_rate)
		aud_n_para = hdmi21_get_frl_aud_n_paras(audio_param->sample_rate, hdev->frl_rate);
	hdmitx21_set_reg_bits(ACR_CTS_CLK_DIV_IVCTX, hdev->frl_rate ? 1 : 0, 4, 1);
	/* N must multiples 4 for DD+ */
	switch (audio_param->type) {
	case CT_DOLBY_D:
		aud_n_para *= 4;
		break;
	default:
		break;
	}
	pr_info(HW "aud_n_para = %d\n", aud_n_para);
	hdmitx21_wr_reg(ACR_CTRL_IVCTX, 0x02);
	hdmitx21_wr_reg(N_SVAL1_IVCTX, (aud_n_para >> 0) & 0xff); //N_SVAL1
	hdmitx21_wr_reg(N_SVAL2_IVCTX, (aud_n_para >> 8) & 0xff); //N_SVAL2
	hdmitx21_wr_reg(N_SVAL3_IVCTX, (aud_n_para >> 16) & 0xff); //N_SVAL3
}

/* flag: 0 means mute */
static void audio_mute_op(bool flag)
{
	if (flag == 0) {
		hdmitx21_wr_reg(AUD_EN_IVCTX, 0);
		hdmitx21_set_reg_bits(AUDP_TXCTRL_IVCTX, 1, 7, 1);
		hdmitx21_set_reg_bits(TPI_AUD_CONFIG_IVCTX, 1, 4, 1);
	} else {
		hdmitx21_wr_reg(AUD_EN_IVCTX, 3);
		hdmitx21_set_reg_bits(AUDP_TXCTRL_IVCTX, 0, 7, 1);
		hdmitx21_set_reg_bits(TPI_AUD_CONFIG_IVCTX, 0, 4, 1);
	}
}

static int hdmitx_set_audmode(struct hdmitx_dev *hdev,
			      struct hdmitx_audpara *audio_param)
{
	u32 data32;
	bool hbr_audio = false;
	u8 div_n = 1;
	u32 sample_rate_k;
	u8 i2s_line_mask = 0;
	u8 hdmitx_aud_clk_div = 18;

	if (!hdev)
		return 0;
	if (!audio_param)
		return 0;
	pr_info(HW "set audio\n");

	/* if hdev->aud_output_ch is true, select I2S as 8ch in, 2ch out */
	if (hdev->aud_output_ch) {
		audio_param->aud_src_if = 1;
		pr_info("hdmitx aud_output_ch 0x%x\n", hdev->aud_output_ch);
	}

	hdmitx21_set_reg_bits(AIP_RST_IVCTX, 1, 0, 1);
	if (audio_param->type == CT_MAT || audio_param->type == CT_DTS_HD_MA) {
		hbr_audio = true;
		if (audio_param->aud_src_if != AUD_SRC_IF_I2S)
			pr_info("warning: hbr with non-i2s\n");
		//div_n = 4;
	}
	if (audio_param->type == CT_DOLBY_D)
		div_n = 4;

	sample_rate_k = aud_sr_idx_to_val(audio_param->sample_rate);
	//pr_info("sample_rate = %d\n", sample_rate_k);
	//pr_info("div_n = %d\n", div_n);
	/* audio asynchronous sample clock, for spdif */
	hdmitx_aud_clk_div = 2000000 / 3 / 6  / 128 / sample_rate_k / div_n;
	pr_info("clk_div = %d\n", hdmitx_aud_clk_div);
	//if (audio_param->sample_rate == FS_32K)
		//hdmitx_aud_clk_div = 26;
	//else if (audio_param->sample_rate == FS_48K)
		//hdmitx_aud_clk_div = 18;
	//else if (audio_param->sample_rate == FS_192K)
		//hdmitx_aud_clk_div = 4;
	//else
		//pr_info("Error:no audio clk setting for sample rate: %d\n",
			//audio_param->sample_rate);
	hdmitx21_set_audioclk(hdmitx_aud_clk_div);
	//audio_mute_op(hdev->tx_aud_cfg);
	//pr_info("audio_param->type = %d\n", audio_param->type);
	pr_info("audio_param->channel_num = %d\n", audio_param->channel_num);
	/* I2S: hbr and lpcm 2~8ch
	 * spdif: lpcm 2ch, aml_ac3/aml_eac3/aml_dts
	 */
	if (audio_param->aud_src_if == AUD_SRC_IF_I2S)
		hdev->tx_aud_src = 1;
	else
		hdev->tx_aud_src = 0;

	hdmitx21_set_reg_bits(SPDIF_SSMPL2_IVCTX, 0, 5, 1);

	/* if hdev->aud_output_ch is true, select I2S as 8ch in, 2ch out */
	//if (hdev->aud_output_ch)
		//hdev->tx_aud_src = 1;
	/* aud_mclk_sel: Select to use which clock for ACR measurement.
	 * 0= Use i2s_mclk; 1=Use spdif_clk.
	 */
	hdmitx21_set_reg_bits(HDMITX_TOP_CLK_CNTL, 1 - hdev->tx_aud_src, 13, 1);

	pr_info(HW "hdmitx tx_aud_src = %d\n", hdev->tx_aud_src);

	// config I2S
	//---------------
	//some common register config,why config this value ?? TODO
	hdmitx21_wr_reg(AIP_HDMI2MHL_IVCTX, 0x00); //AIP
	hdmitx21_wr_reg(PKT_FILTER_0_IVCTX, 0x02); //PKT FILTER
	hdmitx21_wr_reg(ASRC_IVCTX, 0x00); //ASRC
	hdmitx21_wr_reg(VP_INPUT_SYNC_ADJUST_CONFIG_IVCTX, 0x01); //vp__

	data32 = 0;
	if (hbr_audio) {
		/* hbr no layout, see hdmi1.4 spec table 5-28 */
		data32 = (0 << 1);
	} else if (hdev->tx_aud_src == 1) {
		unsigned char mask = GET_OUTCHN_MSK(hdev->aud_output_ch);

		/* multi-channel lpcm use layout 1 */
		if (audio_param->type == CT_PCM && audio_param->channel_num >= 2)
			data32 = (1 << 1);
		else
			data32 = (0 << 1);
		if (mask) {
			if (mask == 8 || mask == 4 || mask == 2 || mask == 1)
				data32 = (0 << 1);
			else
				data32 = (1 << 1);
		}
	} else {
		data32 = (0 << 1);
	}
	//AUDP_TXCTRL : [1] layout; [7] aud_mute_en
	hdmitx21_wr_reg(AUDP_TXCTRL_IVCTX, data32 & 0xff);

	set_aud_acr_pkt(hdev, audio_param);
	//FREQ 00:mclk=128*Fs;01:mclk=256*Fs;10:mclk=384*Fs;11:mclk=512*Fs;...
	hdmitx21_wr_reg(FREQ_SVAL_IVCTX, 0);

	// [7:6] reg_tpi_spdif_sample_size: 0=Refer to stream header; 1=16-bit; 2=20-bit; 3=24-bit
	// [  4] reg_tpi_aud_mute
	data32 = 0;
	data32 |= (3 << 6);
	data32 |= (0 << 4);
	hdmitx21_wr_reg(TPI_AUD_CONFIG_IVCTX, data32);

	/* for i2s: 2~8ch lpcm, hbr */
	if (hdev->tx_aud_src == 1) {
		unsigned char mask = GET_OUTCHN_MSK(hdev->aud_output_ch);
		int i = 0;
		int j = 0;
		const unsigned char map[] = {
			[1] = 0,
			[2] = 1,
			[4] = 2,
			[8] = 3,
		};

		hdmitx21_wr_reg(I2S_IN_MAP_IVCTX, 0xE4); //I2S_IN_MAP
		if (mask) {
			for (j = 0; j < 4; j++) {
				if (mask & (1 << j)) {
					hdmitx21_set_reg_bits(I2S_IN_MAP_IVCTX,
						map[1 << j], i << 1, 2);
					i++;
				}
			}
		}

		hdmitx21_wr_reg(I2S_IN_CTRL_IVCTX, 0x20); //I2S_IN_CTRL [5] reg_cbit_order TODO
		hdmitx21_wr_reg(I2S_IN_SIZE_IVCTX, 0x0b); //I2S_IN_SIZE
		/* channel status: for i2s hbr/pcm
		 * actually audio module only notify 4 bytes
		 */
		hdmitx21_wr_reg(I2S_CHST0_IVCTX, audio_param->status[0]); //I2S_CHST0
		hdmitx21_wr_reg(I2S_CHST1_IVCTX, audio_param->status[1]); //I2S_CHST1
		hdmitx21_wr_reg(I2S_CHST2_IVCTX, audio_param->status[2]); //I2S_CHST2
		hdmitx21_wr_reg(I2S_CHST3_IVCTX, audio_param->status[3]); //I2S_CHST3
		hdmitx21_wr_reg(I2S_CHST4_IVCTX, audio_param->status[4]); //I2S_CHST4
		/* hardcode: test that it works well for i2s pcm 2~8ch */
		/* hdmitx21_wr_reg(I2S_CHST0_IVCTX, 0x15); //I2S_CHST0 */
		/* hdmitx21_wr_reg(I2S_CHST1_IVCTX, 0x55); //I2S_CHST1 */
		/* hdmitx21_wr_reg(I2S_CHST2_IVCTX, 0xfa); //I2S_CHST2 */
		/* hdmitx21_wr_reg(I2S_CHST3_IVCTX, 0x32); //I2S_CHST3 */
		/* hdmitx21_wr_reg(I2S_CHST4_IVCTX, 0x2b); //I2S_CHST4 */
	}
	data32 = 0;
	data32 |= (0 << 6); //[  6] i2s2dsd_en
	data32 |= (0 << 0); //[5:0] aud_err_thresh
	hdmitx21_wr_reg(SPDIF_ERTH_IVCTX, data32);

	//[7:4] I2S_EN SD0~SD3
	//[  3] DSD_EN
	//[  2] HBRA_EN
	//[  1] SPID_EN  Enable later in test.c, otherwise initial junk data will be sent
	//[ 0] PKT_EN
	data32 = 0;
	if (hdev->tx_aud_src == 1) {
		/* todo: other channel num(4/6ch) */
		if (audio_param->channel_num == 2 - 1) {
			i2s_line_mask = 1;
		} else if (audio_param->channel_num == 4 - 1) {
			/* SD0/1 */
			i2s_line_mask = 0x3;
		} else if (audio_param->channel_num == 6 - 1) {
			/* SD0/1/2 */
			i2s_line_mask = 0x7;
		} else if (audio_param->channel_num == 8 - 1) {
			/* SD0/1/2/3 */
			i2s_line_mask = 0xf;
		}
		if (GET_OUTCHN_MSK(hdev->aud_output_ch))
			i2s_line_mask = GET_OUTCHN_MSK(hdev->aud_output_ch);
		data32 |= (i2s_line_mask << 4);
		data32 |= (0 << 3);
		data32 |= (hbr_audio << 2);
		data32 |= (0 << 1);
		data32 |= (0 << 0);
		hdmitx21_wr_reg(AUD_MODE_IVCTX, data32);  //AUD_MODE
	} else {
		hdmitx21_wr_reg(AUD_MODE_IVCTX, 0x2);  //AUD_MODE
	}

	hdmitx21_wr_reg(AUD_EN_IVCTX, 0x03);           //AUD_EN

	set_aud_info_pkt(hdev, audio_param);
	hdmitx21_set_reg_bits(AIP_RST_IVCTX, 0, 0, 1);
	audio_mute_op(hdev->tx_aud_cfg);
	return 1;
}

static void hdmitx_uninit(struct hdmitx_dev *phdev)
{
	free_irq(phdev->irq_hpd, (void *)phdev);
	pr_info(HW "power off hdmi, unmux hpd\n");

	phy_hpll_off();
	hdmitx21_hpd_hw_op(HPD_UNMUX_HPD);
}

static void hw_reset_dbg(void)
{
}

static int hdmitx_cntl(struct hdmitx_dev *hdev, u32 cmd,
		       u32 argv)
{
	if (cmd == HDMITX_AVMUTE_CNTL) {
		return 0;
	} else if (cmd == HDMITX_EARLY_SUSPEND_RESUME_CNTL) {
		if (argv == HDMITX_EARLY_SUSPEND) {
			u32 pll_cntl = ANACTRL_HDMIPLL_CTRL0;

			hd21_set_reg_bits(pll_cntl, 1, 29, 1);
			usleep_range(49, 51);
			hd21_set_reg_bits(pll_cntl, 0, 28, 1);

			hdmi_phy_suspend();
		}
		if (argv == HDMITX_LATE_RESUME) {
			/* No need below, will be set at set_disp_mode_auto() */
			/* hd21_set_reg_bits(HHI_HDMI_PLL_CNTL, 1, 30, 1); */
			hw_reset_dbg();
			pr_info(HW "swrstzreq\n");
		}
		return 0;
	} else if (cmd == HDMITX_HWCMD_MUX_HPD_IF_PIN_HIGH) {
		/* turnon digital module if gpio is high */
		if (hdmitx21_hpd_hw_op(HPD_IS_HPD_MUXED) == 0) {
			if (hdmitx21_hpd_hw_op(HPD_READ_HPD_GPIO)) {
				msleep(500);
				if (hdmitx21_hpd_hw_op(HPD_READ_HPD_GPIO)) {
					pr_info(HPD "mux hpd\n");
					msleep(100);
					hdmitx21_hpd_hw_op(HPD_MUX_HPD);
				}
			}
		}
	} else if (cmd == HDMITX_HWCMD_MUX_HPD) {
		hdmitx21_hpd_hw_op(HPD_MUX_HPD);
/* For test only. */
	} else if (cmd == HDMITX_HWCMD_TURNOFF_HDMIHW) {
		int unmux_hpd_flag = argv;

		if (unmux_hpd_flag) {
			pr_info(HW "power off hdmi, unmux hpd\n");
			phy_hpll_off();
			hdmitx21_hpd_hw_op(HPD_UNMUX_HPD);
		} else {
			pr_info(HW "power off hdmi\n");
			phy_hpll_off();
		}
	}
	return 0;
}

#define DUMP_CVREG_SECTION(_start, _end) \
do { \
	typeof(_start) start = (_start); \
	typeof(_end) end = (_end); \
	if (start > end) { \
		pr_info("Error start = 0x%x > end = 0x%x\n", \
			((start & 0xffff) >> 2), ((end & 0xffff) >> 2)); \
		break; \
	} \
	pr_info("Start = 0x%x[0x%x]   End = 0x%x[0x%x]\n", \
		start, ((start & 0xffff) >> 2), end, ((end & 0xffff) >> 2)); \
	for (addr = start; addr < end + 1; addr += 4) {	\
		val = hd21_read_reg(addr); \
		if (val) \
			pr_info("0x%08x[0x%04x]: 0x%08x\n", addr, \
				((addr & 0xffff) >> 2), val); \
		} \
} while (0)

#define DUMP_HDMITXREG_SECTION(_start, _end) \
do { \
	typeof(_start) start = (_start); \
	typeof(_end) end = (_end); \
	if (start > end) \
		break; \
\
	for (addr = start; addr < end + 1; addr++) { \
		val = hdmitx21_rd_reg(addr); \
		if (val) \
			pr_info("[0x%08x]: 0x%08x\n", addr, val); \
	} \
} while (0)

static void hdmitx_dump_intr(void)
{
}

#undef pr_fmt
#define pr_fmt(fmt) "hdmitx: " fmt

static void hdmitx_debug(struct hdmitx_dev *hdev, const char *buf)
{
	char tmpbuf[128];
	int i = 0;
	int ret;
	unsigned long adr = 0;
	unsigned long value = 0;
	static enum hdmitx_event event_type = HDMITX_NONE_EVENT;
	struct vinfo_s *vinfo;

	if (!buf)
		return;

	while ((buf[i]) && (buf[i] != ',') && (buf[i] != ' ')) {
		tmpbuf[i] = buf[i];
		i++;
	}
	tmpbuf[i] = 0;

	if (strncmp(tmpbuf, "testhpll", 8) == 0) {
		ret = kstrtoul(tmpbuf + 8, 10, &value);
		hdev->cur_VIC = value;
		hdmitx21_set_clk(hdev);
		return;
	} else if (strncmp(tmpbuf, "frl", 3) == 0) {
		if (strncmp(tmpbuf + 3, "stop", 4) == 0) {
			frl_tx_stop(hdev);
			return;
		}
		hdev->frl_rate = tmpbuf[3] - '0';
		frl_tx_training_handler(hdev);
		return;
	} else if (strncmp(tmpbuf, "clkmsr", 6) == 0) {
		struct _hdmi_clkmsr {
			int idx;
			char *name;
		};
		const static struct _hdmi_clkmsr hdmiclkmsr[] = {
			{4, "fpll_tmds_clk"},
			{8, "fpll_pixel_clk"},
			{16, "vid_pll0_clk"},
			{27, "gp2_pll_clk"},
			{63, "cts_vpu_clk"},
			{64, "cts_vpu_clkb"},
			{66, "cts_vapbclk"},
			{68, "cts_hdmi_tx_pixel_clk"},
			{69, "cts_hdmi_tx_pnx_clk"},
			{70, "cts_hdmi_tx_fe_clk"},
			{71, "cts_hdmitx_aud_clk"},
			{72, "cts_hdmitx_200m_clk"},
			{73, "cts_hdmitx_prif_clk"},
			{74, "cts_hdmitx_sys_clk"},
			{75, "audio_tohdmitx_mclk"},
			{76, "audio_tohdmi_spdif_clk"},
			{79, "htx_aes_clk"},
			{82, "audio_tohdmitx_bclk"},
			{89, "htx_tmds20_clk"},
			{90, "htx_ch_clk"},
			{91, "htx_phy_clk1618"},
			{92, "cts_htx_tmds_clk"},
			{93, "hdmi_clk_todig"},
			{94, "hdmi_clk_out2"},
			{95, "htx_spll_ref_clk_in"},
		};
		if (tmpbuf[6] == 'p') {
			for (i = 0; i < ARRAY_SIZE(hdmiclkmsr); i++)
				pr_info("[%d] %d %s\n", hdmiclkmsr[i].idx,
					meson_clk_measure_with_precision(hdmiclkmsr[i].idx, 32),
					hdmiclkmsr[i].name);
			return;
		}
		for (i = 0; i < ARRAY_SIZE(hdmiclkmsr); i++)
			pr_info("[%d] %d %s\n", hdmiclkmsr[i].idx,
				meson_clk_measure(hdmiclkmsr[i].idx),
				hdmiclkmsr[i].name);
		return;
	} else if (strncmp(tmpbuf, "testedid", 8) == 0) {
		hdev->hwop.cntlddc(hdev, DDC_RESET_EDID, 0);
		hdev->hwop.cntlddc(hdev, DDC_EDID_READ_DATA, 0);
		return;
	} else if (strncmp(tmpbuf, "i2c_reactive", 11) == 0) {
		hdev->hwop.cntlmisc(hdev, MISC_I2C_REACTIVE, 0);
		return;
	} else if (strncmp(tmpbuf, "bist", 4) == 0) {
		if (!hdev->para)
			return;

		vinfo = &hdev->para->hdmitx_vinfo;
		if (strncmp(tmpbuf + 4, "off", 3) == 0) {
			if (vinfo->viu_mux == VIU_MUX_ENCI) {
				hd21_write_reg(ENCI_TST_EN, 0);
			} else {
				hd21_set_reg_bits(ENCP_VIDEO_MODE_ADV, 1, 3, 1);
				hd21_write_reg(VENC_VIDEO_TST_EN, 0);
			}
			hdev->bist_lock = 0;
			return;
		}
		hdev->bist_lock = 1;
		/* for 480i/576i mode */
		if (vinfo->viu_mux == VIU_MUX_ENCI) {
			/* nearly DE_BEGIN */
			hd21_write_reg(ENCI_TST_CLRBAR_STRT, 0x112);
			/* 1440 / 8 = 0xb4 */
			hd21_write_reg(ENCI_TST_CLRBAR_WIDTH, 0xb4);
			hd21_write_reg(ENCI_TST_Y, 0x200);
			hd21_write_reg(ENCI_TST_CB, 0x200);
			hd21_write_reg(ENCI_TST_CR, 0x200);
			hd21_write_reg(ENCI_TST_EN, 1);
			if (strncmp(tmpbuf + 4, "line", 4) == 0)
				hd21_write_reg(ENCI_TST_MDSEL, 2);
			else if (strncmp(tmpbuf + 4, "dot", 3) == 0)
				hd21_write_reg(ENCI_TST_MDSEL, 3);
			else
				hd21_write_reg(ENCI_TST_MDSEL, 1);
			return;
		}
		/* for encp including 1080i */
		hdmi_avi_infoframe_config(CONF_AVI_CS, hdev->para->cs);
		hd21_set_reg_bits(ENCP_VIDEO_MODE_ADV, 0, 3, 1);
		hd21_write_reg(VENC_VIDEO_TST_EN, 1);
		if (strncmp(tmpbuf + 4, "line", 4) == 0) {
			hd21_write_reg(VENC_VIDEO_TST_MDSEL, 2);
			return;
		}
		if (strncmp(tmpbuf + 4, "dot", 3) == 0) {
			hd21_write_reg(VENC_VIDEO_TST_MDSEL, 3);
			return;
		}
		if (strncmp(tmpbuf + 4, "start", 5) == 0) {
			ret = kstrtoul(tmpbuf + 9, 10, &value);
			hd21_write_reg(VENC_VIDEO_TST_CLRBAR_STRT, value);
			return;
		}
		if (strncmp(tmpbuf + 4, "shift", 5) == 0) {
			ret = kstrtoul(tmpbuf + 9, 10, &value);
			hd21_write_reg(VENC_VIDEO_TST_VDCNT_STSET, value);
			return;
		}
		if (strncmp(tmpbuf + 4, "auto", 4) == 0) {
			const struct hdmi_timing *t;

			if (!hdev->para)
				return;
			t = &hdev->para->timing;
			value = t->h_active;
			/* when FRL works, here will be half rate */
			if (hdev->frl_rate) {
				value /= 2;
				if (hdev->para->cs == HDMI_COLORSPACE_YUV420)
					value /= 2;
			}
			hd21_write_reg(VENC_VIDEO_TST_CLRBAR_STRT, 0x113);
			hd21_write_reg(VENC_VIDEO_TST_MDSEL, 1);
			hd21_write_reg(VENC_VIDEO_TST_CLRBAR_WIDTH, value / 8);
			return;
		}
		if ((strncmp(tmpbuf + 4, "X", 1) == 0) || (strncmp(tmpbuf + 4, "x", 1) == 0)) {
			const struct hdmi_timing *t;
			u32 width = 1920;
			u32 height = 1080;

			if (!hdev->para)
				return;
			if (hdev->data->chip_type < MESON_CPU_ID_S5) {
				pr_info("s5 or later support x pattern\n");
				return;
			}
			t = &hdev->para->timing;
			width = t->h_active;
			/* when FRL works, here will be half rate */
			if (hdev->frl_rate) {
				width /= 2;
				if (hdev->para->cs == HDMI_COLORSPACE_YUV420 ||
					hdev->dsc_en)
					width /= 2;
			}
			height = t->v_active;
			hd21_write_reg(VENC_VIDEO_TST_Y, 0x3ff);
			hd21_write_reg(VENC_VIDEO_TST_CB, 0x1);
			hd21_write_reg(VENC_VIDEO_TST_CR, 0x1);
			hd21_write_reg(VENC_VIDEO_TST_CLRBAR_STRT, height);
			hd21_write_reg(VENC_VIDEO_TST_CLRBAR_WIDTH, width);
			hd21_set_reg_bits(ENCP_VIDEO_MODE_ADV, 0, 3, 1);
			hd21_write_reg(VENC_VIDEO_TST_MDSEL, 4);
			hd21_write_reg(VENC_VIDEO_TST_EN, 1);
			return;
		}
		hd21_write_reg(VENC_VIDEO_TST_MDSEL, 1);
		value = 1920;
		ret = kstrtoul(tmpbuf + 4, 10, &value);
		hd21_write_reg(VENC_VIDEO_TST_CLRBAR_STRT, 0x113);
		hd21_write_reg(VENC_VIDEO_TST_CLRBAR_WIDTH, value / 8);
		return;
	} else if (strncmp(tmpbuf, "testaudio", 9) == 0) {
		hdmitx_set_audmode(hdev, NULL);
	} else if (strncmp(tmpbuf, "dumpintr", 8) == 0) {
		hdmitx_dump_intr();
	} else if (strncmp(tmpbuf, "chkfmt", 6) == 0) {
		check21_detail_fmt();
		return;
	} else if (strncmp(tmpbuf, "ss", 2) == 0) {
		pr_info("hdev->output_blank_flag: 0x%x\n",
			hdev->output_blank_flag);
		pr_info("hdev->hpd_state: 0x%x\n", hdev->hpd_state);
		pr_info("hdev->cur_VIC: 0x%x\n", hdev->cur_VIC);
	} else if (strncmp(tmpbuf, "hpd_lock", 8) == 0) {
		if (tmpbuf[8] == '1') {
			hdev->hpd_lock = 1;
			pr_info(HPD "hdmitx: lock hpd\n");
		} else {
			hdev->hpd_lock = 0;
			pr_info(HPD "hdmitx: unlock hpd\n");
		}
		return;
	} else if (strncmp(tmpbuf, "vic", 3) == 0) {
		pr_info(HW "hdmi vic count = %d\n", hdev->vic_count);
		if ((tmpbuf[3] >= '0') && (tmpbuf[3] <= '9')) {
			hdev->vic_count = tmpbuf[3] - '0';
			pr_info(HW "set hdmi vic count = %d\n",
				hdev->vic_count);
		}
	} else if (strncmp(tmpbuf, "dumpcecreg", 10) == 0) {
		u8 cec_val = 0;
		u32 cec_adr = 0;
		/* HDMI CEC Regs address range:0xc000~0xc01c;0xc080~0xc094 */
		for (cec_adr = 0xc000; cec_adr < 0xc01d; cec_adr++) {
			cec_val = hdmitx21_rd_reg(cec_adr);
			pr_info(CEC "HDMI CEC Regs[0x%x]: 0x%x\n",
				cec_adr, cec_val);
		}
		for (cec_adr = 0xc080; cec_adr < 0xc095; cec_adr++) {
			cec_val = hdmitx21_rd_reg(cec_adr);
			pr_info(CEC "HDMI CEC Regs[0x%x]: 0x%x\n",
				cec_adr, cec_val);
		}
		return;
	} else if (tmpbuf[0] == 'w') {
		unsigned long read_back = 0;

		ret = kstrtoul(tmpbuf + 2, 16, &adr);
		ret = kstrtoul(buf + i + 1, 16, &value);
		/* scdc register write */
		if (buf[1] == 's') {
			scdc21_wr_sink(adr, value);
			pr_info("scdc wr 0x%lx 0x%lx\n", adr, value);
			return;
		}
		if (buf[1] == 'h') {
			hdmitx21_wr_reg((unsigned int)adr, (unsigned int)value);
			read_back = hdmitx21_rd_reg(adr);
		}
		pr_info(HW "write %lx to %s reg[%lx]\n", value, "HDMI", adr);
		/* read back in order to check writing is OK or NG. */
		pr_info(HW "Read Back %s reg[%lx]=%lx\n", "HDMI",
			adr, read_back);
	} else if (tmpbuf[0] == 'r') {
		u8 val;
		ret = kstrtoul(tmpbuf + 2, 16, &adr);
		/* scdc register read */
		if (buf[1] == 's') {
			scdc21_rd_sink(adr, &val);
			pr_info("scdc rd 0x%lx 0x%x\n", adr, val);
			return;
		}
		if (buf[1] == 'h')
			value = hdmitx21_rd_reg(adr);
		pr_info(HW "%s reg[%lx]=%lx\n", "HDMI", adr, value);
	} else if (strncmp(tmpbuf, "prbs", 4) == 0) {
		u32 phy_cntl1 = ANACTRL_HDMIPHY_CTRL1;
		u32 phy_cntl4 = ANACTRL_HDMIPHY_CTRL4;
		u32 phy_status = ANACTRL_HDMIPHY_STS;

		switch (hdev->data->chip_type) {
		case MESON_CPU_ID_T7:
			phy_cntl1 = ANACTRL_HDMIPHY_CTRL1;
			phy_cntl4 = ANACTRL_HDMIPHY_CTRL4;
			phy_status = ANACTRL_HDMIPHY_STS;
			break;
		default:
			break;
		}
		/* test prbs */
		for (i = 0; i < 4; i++) {
			hd21_write_reg(phy_cntl1, 0x0390000f);
			hd21_write_reg(phy_cntl1, 0x0390000e);
			hd21_write_reg(phy_cntl1, 0x03904002);
			hd21_write_reg(phy_cntl4, 0x0001efff
				| (i << 20));
			hd21_write_reg(phy_cntl1, 0xef904002);
			mdelay(10);
			if (i > 0)
				pr_info("prbs D[%d]:%x\n", i - 1,
					hd21_read_reg(phy_status));
			else
				pr_info("prbs clk :%x\n",
					hd21_read_reg(phy_status));
		}
	} else if (strncmp(tmpbuf, "reauth_dbg", 10) == 0) {
		ret = kstrtoul(tmpbuf + 10, 10, &value);
		hdcp_reauth_dbg = value;
		pr_info("set hdcp_reauth_dbg :%lu\n", hdcp_reauth_dbg);
	} else if (strncmp(tmpbuf, "streamtype_dbg", 14) == 0) {
		ret = kstrtoul(tmpbuf + 14, 10, &value);
		streamtype_dbg = value;
		pr_info("set streamtype_dbg :%lu\n", streamtype_dbg);
	} else if (strncmp(tmpbuf, "en_fake_rcv_id", 14) == 0) {
		ret = kstrtoul(tmpbuf + 14, 10, &value);
		en_fake_rcv_id = value;
		pr_info("set en_fake_rcv_id :%lu\n", en_fake_rcv_id);
	} else if (strncmp(tmpbuf, "aud_mute", 8) == 0) {
		ret = kstrtoul(tmpbuf + 8, 10, &value);
		hdmitx21_ext_set_audio_output(value);
		pr_info("aud_mute :%lu\n", value);
	} else if (strncmp(tmpbuf, "avmute_frame", 12) == 0) {
		ret = kstrtoul(tmpbuf + 12, 10, &value);
		hdev->debug_param.avmute_frame = value;
		pr_info(HW "avmute_frame = %lu\n", value);
	} else if (strncmp(tmpbuf, "testtype", 8) == 0) {
		ret = kstrtoul(tmpbuf + 8, 10, &value);
		event_type = value;
		pr_info("test event type :%lu\n", value);
	} else if (strncmp(tmpbuf, "testevent", 9) == 0) {
		ret = kstrtoul(tmpbuf + 9, 10, &value);
		hdmitx21_set_uevent(event_type, value);
	} else if (strncmp(tmpbuf, "teststate", 9) == 0) {
		ret = kstrtoul(tmpbuf + 9, 10, &value);
		hdmitx21_set_uevent_state(event_type, value);
	} else if (strncmp(tmpbuf, "hdcp_timeout", 12) == 0) {
		ret = kstrtoul(tmpbuf + 12, 10, &value);
		hdev->up_hdcp_timeout_sec = value;
	} else if (strncmp(tmpbuf, "avmute_ms", 10) == 0) {
		ret = kstrtoul(tmpbuf + 10, 10, &value);
		avmute_ms = value;
		pr_info("set avmute_ms :%lu\n", avmute_ms);
	} else if (strncmp(tmpbuf, "vid_mute_ms", 10) == 0) {
		ret = kstrtoul(tmpbuf + 10, 10, &value);
		vid_mute_ms = value;
		pr_info("set vid_mute_ms :%lu\n", vid_mute_ms);
	} else if (strncmp(tmpbuf, "get_output_mute", 15) == 0) {
		pr_info("VPP output mute :%d\n", get_output_mute());
	} else if (strncmp(tmpbuf, "set_output_mute", 15) == 0) {
		ret = kstrtoul(tmpbuf + 15, 10, &value);
		set_output_mute(!!value);
		pr_info("set VPP output mute :%d\n", !!value);
	} else if (strncmp(tmpbuf, "hdcp_ver", 8) == 0) {
		pr_info("hdcp_22_capable :%d\n", is_rx_hdcp2ver());
	} else if (strncmp(tmpbuf, "emp_test", 8) == 0) {
		ret = kstrtoul(tmpbuf + 8, 10, &value);
		if (value == 0) {
			hdmitx_dsc_cvtem_pkt_disable();
		} else {
#ifdef CONFIG_AMLOGIC_DSC
			hdmitx_get_dsc_data(&hdev->dsc_data);
			hdmitx_dsc_cvtem_pkt_send(&hdev->dsc_data.pps_data, &hdev->para->timing);
#endif
		}
		pr_info("dsc emp test %s\n", value ? "enable" : "disable");
	} else if (strncmp(tmpbuf, "venc", 4) == 0) {
		ret = kstrtoul(tmpbuf + 4, 10, &value);
		pr_info("venc :%d\n", !!value);
		hd21_set_reg_bits(VPU_HDMI_SETTING, !!value, (hdev->enc_idx == 0) ? 0 : 1, 1);
		usleep_range(1000, 1005);
		hdmitx21_venc_en(!!value, 1);
	} else if (strncmp(tmpbuf, "dsc_mux", 7) == 0) {
		ret = kstrtoul(tmpbuf + 7, 10, &value);
		pr_info("dsc_mux :%d\n", !!value);
		hd21_set_reg_bits(VPU_HDMI_SETTING, !!value, 31, 1);
	} else if (strncmp(tmpbuf, "slice_num", 9) == 0) {
		ret = kstrtoul(tmpbuf + 9, 10, &value);
		pr_info("force vpp_post slice_num :%lu\n", value);
		vinfo = &hdev->para->hdmitx_vinfo;
		vinfo->cur_enc_ppc = value;
	} else if (strncmp(tmpbuf, "emp_dbg_en", 10) == 0) {
		ret = kstrtoul(tmpbuf + 10, 10, &value);
		pr_info("emp_dbg_en :%d\n", !!value);
		emp_dbg_en = !!value;
	} else if (strncmp(tmpbuf, "clk_reg", 7) == 0) {
		pr_info("RA: CLKCTRL_VID_CLK0_CTRL         0xfe0000c0: 0x%08x\n",
			hd21_read_reg(CLKCTRL_VID_CLK0_CTRL));
		pr_info("RB: CLKCTRL_VIID_CLK0_CTRL        0xfe0000d0: 0x%08x\n",
			hd21_read_reg(CLKCTRL_VIID_CLK0_CTRL));
		pr_info("RC: CLKCTRL_ENC0_HDMI_CLK_CTRL    0xfe0000d4: 0x%08x\n",
			hd21_read_reg(CLKCTRL_ENC0_HDMI_CLK_CTRL));
		pr_info("RD: CLKCTRL_VID_CLK0_CTRL2        0xfe0000c4: 0x%08x\n",
			hd21_read_reg(CLKCTRL_VID_CLK0_CTRL2));
		pr_info("RE: CLKCTRL_VIID_CLK0_DIV         0xfe0000cc: 0x%08x\n",
			hd21_read_reg(CLKCTRL_VIID_CLK0_DIV));
		pr_info("RF: CLKCTRL_ENC_HDMI_CLK_CTRL     0xfe0000dc: 0x%08x\n",
			hd21_read_reg(CLKCTRL_ENC_HDMI_CLK_CTRL));
		pr_info("RG: CLKCTRL_VID_CLK0_DIV          0xfe0000c8: 0x%08x\n",
			hd21_read_reg(CLKCTRL_VID_CLK0_DIV));
		pr_info("RH: CLKCTRL_HDMI_PLL_TMDS_CLK_DIV 0xfe000218: 0x%08x\n",
			hd21_read_reg(CLKCTRL_HDMI_PLL_TMDS_CLK_DIV));
		pr_info("RI: CLKCTRL_HDMI_VID_PLL_CLK_DIV  0xfe000204: 0x%08x\n",
			hd21_read_reg(CLKCTRL_HDMI_VID_PLL_CLK_DIV));
		pr_info("RJ: CLKCTRL_HTX_CLK_CTRL1         0xfe000120: 0x%08x\n",
			hd21_read_reg(CLKCTRL_HTX_CLK_CTRL1));
	}
}

static char *hdmitx_bist_str[] = {
	"0-None",        /* 0 */
	"1-Color Bar",   /* 1 */
	"2-Thin Line",   /* 2 */
	"3-Dot Grid",    /* 3 */
	"4-Gray",        /* 4 */
	"5-Red",         /* 5 */
	"6-Green",       /* 6 */
	"7-Blue",        /* 7 */
	"8-Black",       /* 8 */
};

static void hdmitx_debug_bist(struct hdmitx_dev *hdev, u32 num)
{
	u32 h_active, video_start;
	struct vinfo_s *vinfo;

	if (!hdev->para)
		return;
	vinfo = &hdev->para->hdmitx_vinfo;
	switch (num) {
	case 1:
	case 2:
	case 3:
		if (vinfo->viu_mux == VIU_MUX_ENCI) {
			hd21_write_reg(ENCI_TST_CLRBAR_STRT, 0x112);
			hd21_write_reg(ENCI_TST_CLRBAR_WIDTH, 0xb4);
			hd21_write_reg(ENCI_TST_MDSEL, num);
			hd21_write_reg(ENCI_TST_Y, 0x200);
			hd21_write_reg(ENCI_TST_CB, 0x200);
			hd21_write_reg(ENCI_TST_CR, 0x200);
			hd21_write_reg(ENCI_TST_EN, 1);
		} else {
			video_start = hd21_read_reg(ENCP_VIDEO_HAVON_BEGIN);
			h_active = vinfo->width;
			hd21_write_reg(VENC_VIDEO_TST_MDSEL, num);
			hd21_write_reg(VENC_VIDEO_TST_CLRBAR_STRT, video_start);
			hd21_write_reg(VENC_VIDEO_TST_CLRBAR_WIDTH,
				     h_active / 8);
			hd21_write_reg(VENC_VIDEO_TST_Y, 0x200);
			hd21_write_reg(VENC_VIDEO_TST_CB, 0x200);
			hd21_write_reg(VENC_VIDEO_TST_CR, 0x200);
			hd21_write_reg(VENC_VIDEO_TST_EN, 1);
			hd21_set_reg_bits(ENCP_VIDEO_MODE_ADV, 0, 3, 1);
		}
		pr_info("show bist pattern %d: %s\n",
			num, hdmitx_bist_str[num]);
		break;
	case 4:
		if (vinfo->viu_mux == VIU_MUX_ENCI) {
			hd21_write_reg(ENCI_TST_MDSEL, 0);
			hd21_write_reg(ENCI_TST_Y, 0x3ff);
			hd21_write_reg(ENCI_TST_CB, 0x200);
			hd21_write_reg(ENCI_TST_CR, 0x200);
			hd21_write_reg(ENCI_TST_EN, 1);
		} else {
			hd21_write_reg(VENC_VIDEO_TST_MDSEL, 0);
			hd21_write_reg(VENC_VIDEO_TST_Y, 0x3ff);
			hd21_write_reg(VENC_VIDEO_TST_CB, 0x200);
			hd21_write_reg(VENC_VIDEO_TST_CR, 0x200);
			hd21_write_reg(VENC_VIDEO_TST_EN, 1);
			hd21_set_reg_bits(ENCP_VIDEO_MODE_ADV, 0, 3, 1);
		}
		pr_info("show bist pattern %d: %s\n",
			num, hdmitx_bist_str[num]);
		break;
	case 5:
		if (vinfo->viu_mux == VIU_MUX_ENCI) {
			hd21_write_reg(ENCI_TST_MDSEL, 0);
			hd21_write_reg(ENCI_TST_Y, 0x200);
			hd21_write_reg(ENCI_TST_CB, 0x0);
			hd21_write_reg(ENCI_TST_CR, 0x3ff);
			hd21_write_reg(ENCI_TST_EN, 1);
		} else {
			hd21_write_reg(VENC_VIDEO_TST_MDSEL, 0);
			hd21_write_reg(VENC_VIDEO_TST_Y, 0x200);
			hd21_write_reg(VENC_VIDEO_TST_CB, 0x0);
			hd21_write_reg(VENC_VIDEO_TST_CR, 0x3ff);
			hd21_write_reg(VENC_VIDEO_TST_EN, 1);
			hd21_set_reg_bits(ENCP_VIDEO_MODE_ADV, 0, 3, 1);
		}
		pr_info("show bist pattern %d: %s\n",
			num, hdmitx_bist_str[num]);
		break;
	case 6:
		if (vinfo->viu_mux == VIU_MUX_ENCI) {
			hd21_write_reg(ENCI_TST_MDSEL, 0);
			hd21_write_reg(ENCI_TST_Y, 0x200);
			hd21_write_reg(ENCI_TST_CB, 0x0);
			hd21_write_reg(ENCI_TST_CR, 0x0);
			hd21_write_reg(ENCI_TST_EN, 1);
		} else {
			hd21_write_reg(VENC_VIDEO_TST_MDSEL, 0);
			hd21_write_reg(VENC_VIDEO_TST_Y, 0x200);
			hd21_write_reg(VENC_VIDEO_TST_CB, 0x0);
			hd21_write_reg(VENC_VIDEO_TST_CR, 0x0);
			hd21_write_reg(VENC_VIDEO_TST_EN, 1);
			hd21_set_reg_bits(ENCP_VIDEO_MODE_ADV, 0, 3, 1);
		}
		pr_info("show bist pattern %d: %s\n",
			num, hdmitx_bist_str[num]);
		break;
	case 7:
		if (vinfo->viu_mux == VIU_MUX_ENCI) {
			hd21_write_reg(ENCI_TST_MDSEL, 0);
			hd21_write_reg(ENCI_TST_Y, 0x200);
			hd21_write_reg(ENCI_TST_CB, 0x3ff);
			hd21_write_reg(ENCI_TST_CR, 0x0);
			hd21_write_reg(ENCI_TST_EN, 1);
		} else {
			hd21_write_reg(VENC_VIDEO_TST_MDSEL, 0);
			hd21_write_reg(VENC_VIDEO_TST_Y, 0x200);
			hd21_write_reg(VENC_VIDEO_TST_CB, 0x3ff);
			hd21_write_reg(VENC_VIDEO_TST_CR, 0x0);
			hd21_write_reg(VENC_VIDEO_TST_EN, 1);
			hd21_set_reg_bits(ENCP_VIDEO_MODE_ADV, 0, 3, 1);
		}
		pr_info("show bist pattern %d: %s\n",
			num, hdmitx_bist_str[num]);
		break;
	case 8:
		if (vinfo->viu_mux == VIU_MUX_ENCI) {
			hd21_write_reg(ENCI_TST_MDSEL, 0);
			hd21_write_reg(ENCI_TST_Y, 0x0);
			hd21_write_reg(ENCI_TST_CB, 0x200);
			hd21_write_reg(ENCI_TST_CR, 0x200);
			hd21_write_reg(ENCI_TST_EN, 1);
		} else {
			hd21_write_reg(VENC_VIDEO_TST_MDSEL, 0);
			hd21_write_reg(VENC_VIDEO_TST_Y, 0x0);
			hd21_write_reg(VENC_VIDEO_TST_CB, 0x200);
			hd21_write_reg(VENC_VIDEO_TST_CR, 0x200);
			hd21_write_reg(VENC_VIDEO_TST_EN, 1);
			hd21_set_reg_bits(ENCP_VIDEO_MODE_ADV, 0, 3, 1);
		}
		pr_info("show bist pattern %d: %s\n",
			num, hdmitx_bist_str[num]);
		break;
	case 0:
	default:
		/*hdev->bist_lock = 0;*/
		if (vinfo->viu_mux == VIU_MUX_ENCI) {
			hd21_write_reg(ENCI_TST_EN, 0);
		} else {
			hd21_set_reg_bits(ENCP_VIDEO_MODE_ADV, 1, 3, 1);
			hd21_write_reg(VENC_VIDEO_TST_EN, 0);
		}
		pr_info("disable bist pattern");
		break;
	}
}

static void hdmitx_getediddata(u8 *des, u8 *src)
{
	int i = 0;
	u32 blk = src[126] + 1;

	if (blk == 2)
		if (src[128 + 4] == 0xe2 && src[128 + 5] == 0x78)
			blk = src[128 + 6] + 1;
	if (blk > 8)
		blk = 8;

	for (i = 0; i < 128 * blk; i++)
		des[i] = src[i];
}

static void hdmitx_set_scdc_div40(u32 div40)
{
	u32 addr = 0x20;
	u32 data;

	if (div40)
		data = 0x3;
	else
		data = 0;
	hdmitx21_set_reg_bits(HDCP2X_CTL_0_IVCTX, 0, 0, 1);
	hdmitx21_wr_reg(LM_DDC_IVCTX, 0x80);
	hdmitx21_wr_reg(DDC_ADDR_IVCTX, 0xa8); //SCDC slave addr
	hdmitx21_wr_reg(DDC_OFFSET_IVCTX, addr & 0xff); //SCDC slave offset
	hdmitx21_wr_reg(DDC_DATA_AON_IVCTX, data & 0xff); //SCDC slave offset data to ddc fifo
	hdmitx21_wr_reg(DDC_DIN_CNT1_IVCTX, 0x01); //data length lo
	hdmitx21_wr_reg(DDC_DIN_CNT2_IVCTX, 0x00); //data length hi
	hdmitx21_wr_reg(DDC_CMD_IVCTX, 0x06); //DDC Write CMD
	hdmitx21_poll_reg(DDC_STATUS_IVCTX, 1 << 4, ~(1 << 4), HZ / 100); //i2c process
	hdmitx21_poll_reg(DDC_STATUS_IVCTX, 0 << 4, ~(1 << 4), HZ / 100); //i2c done
}

static void set_t7_top_div40(u32 div40)
{
	u32 data32;

	// Enable normal output to PHY
	if (div40) {
		data32 = 0;
		data32 |= (0 << 16); // [25:16] tmds_clk_pttn[19:10]
		data32 |= (0 << 0);  // [ 9: 0] tmds_clk_pttn[ 9: 0]
		hdmitx21_wr_reg(HDMITX_T7_TOP_TMDS_CLK_PTTN_01, data32);

		data32 = 0;
		data32 |= (0x3ff << 16); // [25:16] tmds_clk_pttn[39:30]
		data32 |= (0x3ff << 0);  // [ 9: 0] tmds_clk_pttn[29:20]
		hdmitx21_wr_reg(HDMITX_T7_TOP_TMDS_CLK_PTTN_23, data32);
	} else {
		hdmitx21_wr_reg(HDMITX_T7_TOP_TMDS_CLK_PTTN_01, 0x001f001f);
		hdmitx21_wr_reg(HDMITX_T7_TOP_TMDS_CLK_PTTN_23, 0x001f001f);
	}
	hdmitx21_wr_reg(HDMITX_T7_TOP_TMDS_CLK_PTTN_CNTL, 0x1);
	// [14:12] tmds_sel: 0=output 0; 1=output normal data;
	//                   2=output PRBS; 4=output shift pattn
	// [11: 8] shift_pttn
	// [ 4: 0] prbs_pttn
	data32 = 0;
	data32 |= (1 << 12);
	data32 |= (0 << 8);
	data32 |= (0 << 0);
	hdmitx21_wr_reg(HDMITX_TOP_BIST_CNTL, data32);
	if (div40)
		hdmitx21_wr_reg(HDMITX_T7_TOP_TMDS_CLK_PTTN_CNTL, 0x2);
}

static void set_s5_top_div40(u32 div40, u32 frl_mode)
{
	u32 data32;

	// Enable normal output to PHY
	if (div40) {
		data32 = 0;
		data32 |= (0 << 16); // [25:16] tmds_clk_pttn[19:10]
		data32 |= (0 << 0);  // [ 9: 0] tmds_clk_pttn[ 9: 0]
		hdmitx21_wr_reg(HDMITX_S5_TOP_TMDS_CLK_PTTN_01, data32);

		data32 = 0;
		data32 |= (0x3ff << 16); // [25:16] tmds_clk_pttn[39:30]
		data32 |= (0x3ff << 0);  // [ 9: 0] tmds_clk_pttn[29:20]
		hdmitx21_wr_reg(HDMITX_S5_TOP_TMDS_CLK_PTTN_23, data32);
	} else {
		hdmitx21_wr_reg(HDMITX_S5_TOP_TMDS_CLK_PTTN_01, 0x001f001f);
		hdmitx21_wr_reg(HDMITX_S5_TOP_TMDS_CLK_PTTN_23, 0x001f001f);
	}
	hdmitx21_wr_reg(HDMITX_S5_TOP_TMDS_CLK_PTTN_CNTL, 0x1);
	// [18:16] tmds_sel: 0=output 0; 1=output normal data;
	//                   2=output PRBS; 4=output shift pattn
	// [11: 8] shift_pttn
	// [ 4: 0] prbs_pttn
	data32 = 0;
	data32 |= (1 << 16);
	data32 |= (0 << 8);
	data32 |= (0 << 0);
	hdmitx21_wr_reg(HDMITX_TOP_BIST_CNTL, data32);

	hdmitx21_set_reg_bits(HDMITX_TOP_BIST_CNTL, frl_mode ? 1 : 0, 19, 1);

	if (div40)
		hdmitx21_wr_reg(HDMITX_S5_TOP_TMDS_CLK_PTTN_CNTL, 0x2);
}

static void set_top_div40(u32 div40)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	if (hdev->data->chip_type == MESON_CPU_ID_T7)
		set_t7_top_div40(div40);
	else
		set_s5_top_div40(div40, hdev->frl_rate);
}

static void hdmitx_set_div40(u32 div40)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	if (div40)
		hdmitx_set_scdc_div40(1);
	else if (hdev->rxcap.scdc_present ||
		hdev->pre_tmds_clk_div40)
		hdmitx_set_scdc_div40(0);
	else
		pr_info("warn: SCDC not present, should not send 1:10\n");
	set_top_div40(div40);
	hdmitx21_wr_reg(SCRCTL_IVCTX, (1 << 5) | !!div40);
	hdev->pre_tmds_clk_div40 = !!div40;
}

static int hdmitx_cntl_ddc(struct hdmitx_dev *hdev, u32 cmd,
			   unsigned long argv)
{
	if ((cmd & CMD_DDC_OFFSET) != CMD_DDC_OFFSET) {
		pr_err(HW "ddc: invalid cmd 0x%x\n", cmd);
		return -1;
	}

	switch (cmd) {
	case DDC_RESET_EDID:
		memset(hdev->tmp_edid_buf, 0, ARRAY_SIZE(hdev->tmp_edid_buf));
		break;
	case DDC_EDID_READ_DATA:
		/* if running on pxp, then skip EDID reading */
		if (hdev->pxp_mode)
			return 0;
		hdmitx21_read_edid(hdev->tmp_edid_buf);
		break;
	case DDC_EDID_GET_DATA:
		if (argv == 0)
			hdmitx_getediddata(hdev->EDID_buf, hdev->tmp_edid_buf);
		else
			hdmitx_getediddata(hdev->EDID_buf1, hdev->tmp_edid_buf);
		break;
	case DDC_GLITCH_FILTER_RESET:
		hdmitx21_set_reg_bits(HDMITX_TOP_SW_RESET, 1, 6, 1);
		/*keep resetting DDC for some time*/
		usleep_range(1000, 2000);
		hdmitx21_set_reg_bits(HDMITX_TOP_SW_RESET, 0, 6, 1);
		/*wait recover for resetting DDC*/
		usleep_range(1000, 2000);
		break;
	case DDC_PIN_MUX_OP:
		if (argv == PIN_MUX)
			hdmitx21_ddc_hw_op(DDC_MUX_DDC);
		if (argv == PIN_UNMUX)
			hdmitx21_ddc_hw_op(DDC_UNMUX_DDC);
		break;
	case DDC_EDID_CLEAR_RAM:
		break;
	case DDC_SCDC_DIV40_SCRAMB:
		if (argv == 1) {
			hdmitx_set_scdc_div40(1);
		} else if (argv == 0) {
			if (hdev->rxcap.scdc_present)
				hdmitx_set_scdc_div40(0);
			else
				pr_info("warn2: SCDC not present, should not send 1:10\n");
		} else {
			/* force send 1:10 tmds bit clk ratio, for echo 2 > div40 */
			hdmitx_set_scdc_div40(0);
		}
		hdev->div40 = (argv == 1);
		break;
	case DDC_HDCP_SET_TOPO_INFO:
		set_hdcp2_topo(!!argv);
		break;
	default:
		break;
	}
	return 1;
}

static int hdmitx_hdmi_dvi_config(struct hdmitx_dev *hdev,
				  u32 dvi_mode)
{
	if (dvi_mode == 1)
		hdmitx_csc_config(TX_INPUT_COLOR_FORMAT, HDMI_COLORSPACE_RGB, hdev->para->cd);

	return 0;
}

static int hdmitx_get_hdmi_dvi_config(struct hdmitx_dev *hdev)
{
	/* TODO */
	return HDMI_MODE;
}

static int hdmitx_cntl_config(struct hdmitx_dev *hdev, u32 cmd,
			      u32 argv)
{
	int ret = 0;

	if ((cmd & CMD_CONF_OFFSET) != CMD_CONF_OFFSET) {
		pr_err(HW "config: invalid cmd 0x%x\n", cmd);
		return -1;
	}

	switch (cmd) {
	case CONF_HDMI_DVI_MODE:
		hdmitx_hdmi_dvi_config(hdev, (argv == DVI_MODE) ? 1 : 0);
		break;
	case CONF_GET_HDMI_DVI_MODE:
		ret = hdmitx_get_hdmi_dvi_config(hdev);
		break;
	case CONF_AUDIO_MUTE_OP:
		audio_mute_op(argv == AUDIO_MUTE ? 0 : 1);
		break;
	case CONF_VIDEO_MUTE_OP:
		if (argv == VIDEO_MUTE) {
			if (hdev->data->chip_type == MESON_CPU_ID_S5) {
				hd21_set_reg_bits(ENCP_VIDEO_MODE_ADV, 0, 3, 1);
				hd21_write_reg(VENC_VIDEO_TST_EN, 1);
				hd21_write_reg(VENC_VIDEO_TST_MDSEL, 0);
				hd21_write_reg(VENC_VIDEO_TST_Y, 0x0);
				hd21_write_reg(VENC_VIDEO_TST_CB, 0x200);
				hd21_write_reg(VENC_VIDEO_TST_CR, 0x200);
			} else {
				set_output_mute(true);
			}
		}
		if (argv == VIDEO_UNMUTE) {
			if (hdev->data->chip_type == MESON_CPU_ID_S5) {
				hd21_set_reg_bits(ENCP_VIDEO_MODE_ADV, 1, 3, 1);
				hd21_write_reg(VENC_VIDEO_TST_EN, 0);
			} else {
				set_output_mute(false);
			}
		}
		break;
	case CONF_CLR_AVI_PACKET:
		break;
	case CONF_CLR_VSDB_PACKET:
		break;
	case CONF_VIDEO_MAPPING:
		config_video_mapping(hdev->para->cs, hdev->para->cd);
		break;
	case CONF_CLR_AUDINFO_PACKET:
		break;
	case CONF_ASPECT_RATIO:
		pr_info("%s argv = %d\n", __func__, argv);
		hdmi_avi_infoframe_config(CONF_AVI_VIC, argv >> 2);
		hdmi_avi_infoframe_config(CONF_AVI_AR, argv & 0x3);
		break;
	case CONF_AVI_BT2020:
		break;
	case CONF_CLR_DV_VS10_SIG:
		break;
	case CONF_CT_MODE:
		hdmi_avi_infoframe_config(CONF_AVI_CT_TYPE, argv);
		break;
	case CONF_EMP_NUMBER:
		break;
	case CONF_EMP_PHY_ADDR:
		break;
	default:
		break;
	}

	return ret;
}

static enum frl_rate_enum get_current_frl_rate(void)
{
	u8 rate = hdmitx21_rd_reg(FRL_LINK_RATE_CONFIG_IVCTX) & 0xf;

	if (rate >= FRL_RATE_MAX)
		rate = FRL_NONE;

	return rate;
}

static int hdmitx_tmds_rxsense(void)
{
	int ret = 0;
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	switch (hdev->data->chip_type) {
	case MESON_CPU_ID_T7:
		hd21_set_reg_bits(ANACTRL_HDMIPHY_CTRL0, 1, 16, 1);
		hd21_set_reg_bits(ANACTRL_HDMIPHY_CTRL3, 1, 23, 1);
		hd21_set_reg_bits(ANACTRL_HDMIPHY_CTRL3, 0, 24, 1);
		hd21_set_reg_bits(ANACTRL_HDMIPHY_CTRL3, 3, 20, 3);
		ret = hd21_read_reg(ANACTRL_HDMIPHY_CTRL2) & 0x1;
		return ret;
	case MESON_CPU_ID_S5:
		hd21_set_reg_bits(ANACTRL_HDMIPHY_CTRL0, 1, 19, 1);
		ret = !!(hd21_read_reg(ANACTRL_HDMIPHY_CTRL2) & 0xf);
		return ret;
	default:
		break;
	}
	if (!(hdev->hwop.cntlmisc(hdev, MISC_HPD_GPI_ST, 0)))
		return 0;
	return ret;
}

/*Check from SCDC Status_Flags_0/1 */
/* 0 means TMDS ok */
static int hdmitx_tmds_cedst(struct hdmitx_dev *hdev)
{
	return scdc21_status_flags(hdev);
}

static int hdmitx_cntl_misc(struct hdmitx_dev *hdev, u32 cmd,
			    u32 argv)
{
	if ((cmd & CMD_MISC_OFFSET) != CMD_MISC_OFFSET) {
		pr_err(HW "misc: w: invalid cmd 0x%x\n", cmd);
		return -1;
	}

	switch (cmd) {
	case MISC_GET_FRL_MODE:
		return (int)get_current_frl_rate();
	case MISC_CLK_DIV_RST:
		hdmitx21_s5_clk_div_rst(argv);
		break;
	case MISC_HPD_MUX_OP:
		if (argv == PIN_MUX)
			argv = HPD_MUX_HPD;
		else
			argv = HPD_UNMUX_HPD;
		return hdmitx21_hpd_hw_op(argv);
	case MISC_HPD_GPI_ST:
		return hdmitx21_hpd_hw_op(HPD_READ_HPD_GPIO);
	case MISC_TRIGGER_HPD:
		if (argv == 1)
			hdmitx21_wr_reg(HDMITX_TOP_INTR_STAT, 1 << 1);
		else
			hdmitx21_wr_reg(HDMITX_TOP_INTR_STAT, 1 << 2);
		return 0;
		break;
	case MISC_TMDS_PHY_OP:
		if (argv == TMDS_PHY_ENABLE) {
			hdmitx_phy_pre_init(hdev);
			hdmi_phy_wakeup(hdev); /* TODO */
			fifo_flow_enable_intrs(1);
		}
		if (argv == TMDS_PHY_DISABLE) {
			/* as did in echo -1 > hdcp_mode in hdmitx20 */
			fifo_flow_enable_intrs(0);
			hdmitx21_disable_hdcp(hdev);
			hdmi_phy_suspend();
		}
		break;
	case MISC_TMDS_RXSENSE:
		return hdmitx_tmds_rxsense();
	case MISC_TMDS_CEDST:
		return hdmitx_tmds_cedst(hdev);
	case MISC_VIID_IS_USING:
		break;
	case MISC_AVMUTE_OP:
		config_avmute(argv);
		break;
	case MISC_READ_AVMUTE_OP:
		return read_avmute();
	case MISC_I2C_RESET:
		hdmitx21_set_reg_bits(HDMITX_TOP_SW_RESET, 1, 9, 1);
		usleep_range(1000, 2000);
		hdmitx21_set_reg_bits(HDMITX_TOP_SW_RESET, 0, 9, 1);
		usleep_range(1000, 2000);
		break;
	case MISC_HDMI_CLKS_CTRL:
		hdmitx_clks_gate_ctrl(!!argv);
		break;
	case MISC_I2C_REACTIVE:
		break;
	default:
		break;
	}
	return 1;
}

static enum hdmi_vic get_vic_from_pkt(void)
{
	enum hdmi_vic vic = HDMI_0_UNKNOWN;

	vic = hdmitx21_rd_reg(TPI_AVI_BYTE4_IVCTX) & 0x7f;

	return vic;
}

static int hdmitx_get_state(struct hdmitx_dev *hdev, u32 cmd,
			    u32 argv)
{
	if ((cmd & CMD_STAT_OFFSET) != CMD_STAT_OFFSET) {
		pr_err(HW "state: invalid cmd 0x%x\n", cmd);
		return -1;
	}

	switch (cmd) {
	case STAT_VIDEO_VIC:
		return (int)get_vic_from_pkt();
	case STAT_VIDEO_CLK:
		break;
	case STAT_HDR_TYPE:
		return 0;
	default:
		break;
	}
	return 0;
}

static void hdmi_phy_suspend(void)
{
	u32 phy_cntl0 = ANACTRL_HDMIPHY_CTRL0;
	u32 phy_cntl3 = ANACTRL_HDMIPHY_CTRL3;
	u32 phy_cntl5 = ANACTRL_HDMIPHY_CTRL5;
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	if (hdev->data->chip_type == MESON_CPU_ID_T7)
		phy_cntl5 = ANACTRL_HDMIPHY_CTRL5;
	else
		phy_cntl5 = ANACTRL_HDMIPHY_CTRL6;
	/* When HDMI PHY is turned off, the arc power
	 * supply module still need to work, otherwise
	 * hdmi arc disables, no audio during HEACT 5-18
	 */
	if (!hdev->suspend_flag && hdev->arc_rx_en)
		hd21_write_reg(phy_cntl0, 0x10000);
	else
		hd21_write_reg(phy_cntl0, 0x0);
	/* keep PHY_CNTL3 bit[1:0] as 0b11,
	 * otherwise may cause HDCP22 boot failed
	 */
	hd21_write_reg(phy_cntl3, 0x3);
	hd21_write_reg(phy_cntl5, 0x800);
}

static void hdmi_phy_wakeup(struct hdmitx_dev *hdev)
{
	hdmitx_set_phy(hdev);
}

#define COLORDEPTH_24B            4
#define HDMI_COLOR_DEPTH_30B            5
#define HDMI_COLOR_DEPTH_36B            6
#define HDMI_COLOR_DEPTH_48B            7

#define HDMI_COLOR_FORMAT_RGB 0
#define HDMI_COLORSPACE_YUV422           1
#define HDMI_COLOR_FORMAT_444           2
#define HDMI_COLORSPACE_YUV420           3

#define HDMI_COLOR_RANGE_LIM 0
#define HDMI_COLOR_RANGE_FUL            1

#define HDMI_AUDIO_PACKET_SMP 0x02
#define HDMI_AUDIO_PACKET_1BT 0x07
#define HDMI_AUDIO_PACKET_DST 0x08
#define HDMI_AUDIO_PACKET_HBR 0x09
#define HDMI_AUDIO_PACKET_MUL 0x0e

/*
 * color_depth: Pixel bit width: 4=24-bit; 5=30-bit; 6=36-bit; 7=48-bit.
 * input_color_format: 0=RGB444; 1=YCbCr422; 2=YCbCr444; 3=YCbCr420.
 * input_color_range: 0=limited; 1=full.
 * output_color_format: 0=RGB444; 1=YCbCr422; 2=YCbCr444; 3=YCbCr420
 */

static void config_hdmi21_tx(struct hdmitx_dev *hdev)
{
	struct hdmi_format_para *para = hdev->para;
	u8 color_depth = COLORDEPTH_24B; // Pixel bit width: 4=24-bit; 5=30-bit; 6=36-bit; 7=48-bit.
	// Pixel format: 0=RGB444; 1=YCbCr422; 2=YCbCr444; 3=YCbCr420.
	u8 input_color_format = HDMI_COLORSPACE_YUV444;
	u8 input_color_range = COLORRANGE_LIM; // Pixel range: 0=limited; 1=full.
	// Pixel format: 0=RGB444; 1=YCbCr422; 2=YCbCr444; 3=YCbCr420.
	u8 output_color_format = HDMI_COLORSPACE_YUV444;
	u8 output_color_range = COLORRANGE_LIM; // Pixel range: 0=limited; 1=full.
	u32 active_pixels = 1920; // Number of active pixels per line
	u32 active_lines = 1080; // Number of active lines per field
	// 0=I2S 2-channel; 1=I2S 4 x 2-channel; 2=channel 0/1, 4/5 valid.
	// 2=audio sample packet; 7=one bit audio; 8=DST audio packet; 9=HBR audio packet.
	u32 data32;
	u8 data8;
	u8 csc_en;
	u8 dp_color_depth = 0;

	if (para->tmds_clk > 340000) {
		para->scrambler_en = 1;
		para->tmds_clk_div40 = 1;
	} else {
		para->scrambler_en = 0;
		para->tmds_clk_div40 = 0;
	}
	color_depth = para->cd;
	output_color_format = para->cs;
	active_pixels = para->timing.h_active;
	active_lines = para->timing.v_active;
	dp_color_depth = (output_color_format == HDMI_COLORSPACE_YUV422) ?
				COLORDEPTH_24B : color_depth;

	if (hdev->dsc_en)
		dp_color_depth = COLORDEPTH_24B;
	pr_info("configure hdmitx21\n");
	hdmitx21_wr_reg(HDMITX_TOP_SW_RESET, 0);
	hdmitx_set_div40(para->tmds_clk_div40);
	hdev->div40 = para->tmds_clk_div40;

	//--------------------------------------------------------------------------
	// Glitch-filter HPD and RxSense
	//--------------------------------------------------------------------------
	// [31:28] rxsense_glitch_width: Filter out glitch width <= hpd_glitch_width
	// [27:16] rxsense_valid_width: Filter out signal stable width <= hpd_valid_width*1024
	// [15:12] hpd_glitch_width: Filter out glitch width <= hpd_glitch_width
	// [11: 0] hpd_valid_width: Filter out signal stable width <= hpd_valid_width*1024
	data32 = 0;
	data32 |= (8 << 28);
	data32 |= (0 << 16);
	data32 |= (7 << 12);
	data32 |= (0 << 0);
	hdmitx21_wr_reg(HDMITX_TOP_HPD_FILTER,    data32);

	//-------------
	//config video
	//-------------
	data8 = 0;
	data8 |= (dp_color_depth & 0x03); // [1:0]color depth. 00:8bpp;01:10bpp;10:12bpp;11:16bpp
	data8 |= (((dp_color_depth != 4) ? 1 : 0) << 7);  // [7]  deep color enable bit
	data8 |= (hdev->frl_rate ? 1 : 0) << 3;
	data8 |= (hdev->frl_rate ? 1 : 0) << 4;
	hdmitx21_wr_reg(P2T_CTRL_IVCTX, data8);
	data32 = 0;
	data32 |= (1 << 5);  // [  5] reg_hdmi2_on
	data32 |= (para->scrambler_en & 0x01 << 0);  // [ 0] scrambler_en.
	hdmitx21_wr_reg(SCRCTL_IVCTX, data32 & 0xff);
	hdmitx21_set_reg_bits(FRL_LINK_RATE_CONFIG_IVCTX, hdev->frl_rate, 0, 4);

	hdmitx21_wr_reg(SW_RST_IVCTX, 0); // default value
	hdmitx21_wr_reg(CLK_DIV_CNTRL_IVCTX, hdev->frl_rate ? 0 : 1);
	//hdmitx21_wr_reg(H21TXSB_PKT_PRD_IVCTX, 0x1);
	//hdmitx21_wr_reg(HOST_CTRL2_IVCTX, 0x80); //INT active high
	hdmitx21_wr_reg(CLKPWD_IVCTX, 0xf4);
	hdmitx21_wr_reg(SOC_FUNC_SEL_IVCTX, 0x01);
	//hdmitx21_wr_reg(SYS_MISC_IVCTX, 0x00); config same with default
	//hdmitx21_wr_reg(DIPT_CNTL_IVCTX, 0x06); config same with default
	hdmitx21_wr_reg(TEST_TXCTRL_IVCTX, 0x02); //[1] enable hdmi
	//hdmitx21_wr_reg(TX_ZONE_CTL4_IVCTX, 0x04); config same with default
	hdmitx21_wr_reg(CLKRATIO_IVCTX, 0x8a);

	//---------------
	//config vp core
	//---------------
	csc_en = (input_color_format != output_color_format ||
		  input_color_range  != output_color_range) ? 1 : 0;
	//some common register are configuration here TODO why config this value
	hdmitx21_wr_reg(VP_CMS_CSC0_MULTI_CSC_CONFIG_IVCTX, 0x00);
	hdmitx21_wr_reg((VP_CMS_CSC0_MULTI_CSC_CONFIG_IVCTX + 1), 0x08);
	hdmitx21_wr_reg(VP_CMS_CSC1_MULTI_CSC_CONFIG_IVCTX, 0x00);
	hdmitx21_wr_reg((VP_CMS_CSC1_MULTI_CSC_CONFIG_IVCTX + 1), 0x08);
	if (output_color_format == HDMI_COLORSPACE_RGB) {
		hdmitx21_wr_reg(VP_CMS_CSC0_MULTI_CSC_CONFIG_IVCTX, 0x65);
		hdmitx21_wr_reg((VP_CMS_CSC0_MULTI_CSC_CONFIG_IVCTX + 1), 0x08);
	}

	// [5:4] disable_lsbs_cr / [3:2] disable_lsbs_cb / [1:0] disable_lsbs_y
	// 0=12bit; 1=10bit(disable 2-LSB), 2=8bit(disable 4-LSB), 3=6bit(disable 6-LSB)
	data8 = 0;
	data8 |= ((6 - color_depth) << 4);
	data8 |= ((6 - color_depth) << 2);
	data8 |= ((6 - color_depth) << 0);
	hdmitx21_wr_reg(VP_INPUT_MASK_IVCTX, data8);

	// [11: 9] select_cr: 0=y; 1=cb; 2=Cr; 3={cr[11:4],cb[7:4]};
	//                    4={cr[3:0],y[3:0],cb[3:0]};
	//                    5={y[3:0],cr[3:0],cb[3:0]};
	//                    6={cb[3:0],y[3:0],cr[3:0]};
	//                    7={y[3:0],cb[3:0],cr[3:0]}.
	// [ 8: 6] select_cb: 0=y; 1=cb; 2=Cr; 3={cr[11:4],cb[7:4]};
	//                    4={cr[3:0],y[3:0],cb[3:0]};
	//                    5={y[3:0],cr[3:0],cb[3:0]};
	//                    6={cb[3:0],y[3:0],cr[3:0]};
	//                    7={y[3:0],cb[3:0],cr[3:0]}.
	// [ 5: 3] select_y : 0=y; 1=cb; 2=Cr; 3={y[11:4],cb[7:4]};
	//                    4={cb[3:0],cr[3:0],y[3:0]};
	//                    5={cr[3:0],cb[3:0],y[3:0]};
	//                    6={y[3:0],cb[3:0],cr[3:0]};
	//                    7={y[3:0],cr[3:0],cb[3:0]}.
	// [    2] reverse_cr
	// [    1] reverse_cb
	// [ 0] reverse_y
	//Output 422
	if (para->cs == HDMI_COLORSPACE_YUV422) {
		data32 = 0;
		data32 |= (2 << 9);
		data32 |= (4 << 6);
		data32 |= (0 << 3);
		data32 |= (0 << 2);
		data32 |= (0 << 1);
		data32 |= (0 << 0);

		// [5:4] disable_lsbs_cr / [3:2] disable_lsbs_cb / [1:0] disable_lsbs_y
		// 0=12bit; 1=10bit(disable 2-LSB),
		// 2=8bit(disable 4-LSB), 3=6bit(disable 6-LSB)
		data8 = 0;
		data8 |= (2 << 4);
		data8 |= (2 << 2);
		data8 |= (2 << 0);
	} else {
		data32 = 0;
		data32 |= (2 << 9);
		data32 |= (1 << 6);
		data32 |= (0 << 3);
		data32 |= (0 << 2);
		data32 |= (0 << 1);
		data32 |= (0 << 0);

		// [5:4] disable_lsbs_cr / [3:2] disable_lsbs_cb / [1:0] disable_lsbs_y
		// 0=12bit; 1=10bit(disable 2-LSB),
		// 2=8bit(disable 4-LSB), 3=6bit(disable 6-LSB)
		data8 = 0;
		data8 |= (0 << 4);
	}
	//mapping for yuv422 12bit
	hdmitx21_wr_reg(VP_OUTPUT_MAPPING_IVCTX, data32 & 0xff);
	hdmitx21_wr_reg(VP_OUTPUT_MAPPING_IVCTX + 1, (data32 >> 8) & 0xff);
	hdmitx21_wr_reg(VP_OUTPUT_MASK_IVCTX, data8);

	//---------------
	// config I2S
	//---------------
	hdmitx21_set_audio(hdev, &hdev->cur_audio_param);

	//---------------
	// config Packet
	//---------------
	hdmitx21_wr_reg(VTEM_CTRL_IVCTX, 0x04); //[2] reg_vtem_ctrl

	//drm,emp pacekt
	hdmitx21_wr_reg(HDMITX_TOP_HS_INTR_CNTL, 0x010); //set TX hs_int h_cnt
	//tx_program_drm_emp(int_ext,active_lines,blank_lines,128);

	//--------------------------------------------------------------------------
	// Configure HDCP
	//--------------------------------------------------------------------------

	data32 = 0;
	data32 |= (0xef << 22); // [29:20] channel2 override
	data32 |= (0xcd << 12); // [19:10] channel1 override
	data32 |= (0xab << 2);  // [ 9: 0] channel0 override
	hdmitx21_wr_reg(HDMITX_TOP_SEC_VIDEO_OVR, data32);

	hdmitx21_wr_reg(HDMITX_TOP_HDCP14_MIN_SIZE, 0);
	hdmitx21_wr_reg(HDMITX_TOP_HDCP22_MIN_SIZE, 0);

	data32 = 0;
	data32 |= (active_lines << 16); // [30:16] cntl_vactive
	data32 |= (active_pixels << 0);  // [14: 0] cntl_hactive
	hdmitx21_wr_reg(HDMITX_TOP_HV_ACTIVE, data32);
	hdmitx21_set_reg_bits(PWD_SRST_IVCTX, 3, 1, 2);
	hdmitx21_set_reg_bits(PWD_SRST_IVCTX, 0, 1, 2);
	if (hdev->rxcap.ieeeoui == HDMI_IEEE_OUI)
		hdmitx21_set_reg_bits(TPI_SC_IVCTX, 1, 0, 1);
	else
		hdmitx21_set_reg_bits(TPI_SC_IVCTX, 0, 0, 1);
} /* config_hdmi21_tx */

static void hdmitx_csc_config(u8 input_color_format,
			      u8 output_color_format,
			      u8 color_depth)
{
	u8 conv_en;
	u8 csc_scale;
	u8 rgb_ycc_indicator;
	unsigned long csc_coeff_a1, csc_coeff_a2, csc_coeff_a3, csc_coeff_a4;
	unsigned long csc_coeff_b1, csc_coeff_b2, csc_coeff_b3, csc_coeff_b4;
	unsigned long csc_coeff_c1, csc_coeff_c2, csc_coeff_c3, csc_coeff_c4;
	unsigned long data32;

	conv_en = (((input_color_format  == HDMI_COLORSPACE_RGB) ||
		(output_color_format == HDMI_COLORSPACE_RGB)) &&
		(input_color_format  != output_color_format)) ? 1 : 0;

	if (conv_en) {
		if (output_color_format == HDMI_COLORSPACE_RGB) {
			csc_coeff_a1 = 0x2000;
			csc_coeff_a2 = 0x6926;
			csc_coeff_a3 = 0x74fd;
			csc_coeff_a4 = (color_depth == COLORDEPTH_24B) ?
				0x010e :
			(color_depth == COLORDEPTH_30B) ? 0x043b :
			(color_depth == COLORDEPTH_36B) ? 0x10ee :
			(color_depth == COLORDEPTH_48B) ? 0x10ee : 0x010e;
		csc_coeff_b1 = 0x2000;
		csc_coeff_b2 = 0x2cdd;
		csc_coeff_b3 = 0x0000;
		csc_coeff_b4 = (color_depth == COLORDEPTH_24B) ? 0x7e9a :
			(color_depth == COLORDEPTH_30B) ? 0x7a65 :
			(color_depth == COLORDEPTH_36B) ? 0x6992 :
			(color_depth == COLORDEPTH_48B) ? 0x6992 : 0x7e9a;
		csc_coeff_c1 = 0x2000;
		csc_coeff_c2 = 0x0000;
		csc_coeff_c3 = 0x38b4;
		csc_coeff_c4 = (color_depth == COLORDEPTH_24B) ? 0x7e3b :
			(color_depth == COLORDEPTH_30B) ? 0x78ea :
			(color_depth == COLORDEPTH_36B) ? 0x63a6 :
			(color_depth == COLORDEPTH_48B) ? 0x63a6 : 0x7e3b;
		csc_scale = 1;
	} else { /* input_color_format == HDMI_COLORSPACE_RGB */
		csc_coeff_a1 = 0x2591;
		csc_coeff_a2 = 0x1322;
		csc_coeff_a3 = 0x074b;
		csc_coeff_a4 = 0x0000;
		csc_coeff_b1 = 0x6535;
		csc_coeff_b2 = 0x2000;
		csc_coeff_b3 = 0x7acc;
		csc_coeff_b4 = (color_depth == COLORDEPTH_24B) ? 0x0200 :
			(color_depth == COLORDEPTH_30B) ? 0x0800 :
			(color_depth == COLORDEPTH_36B) ? 0x2000 :
			(color_depth == COLORDEPTH_48B) ? 0x2000 : 0x0200;
		csc_coeff_c1 = 0x6acd;
		csc_coeff_c2 = 0x7534;
		csc_coeff_c3 = 0x2000;
		csc_coeff_c4 = (color_depth == COLORDEPTH_24B) ? 0x0200 :
			(color_depth == COLORDEPTH_30B) ? 0x0800 :
			(color_depth == COLORDEPTH_36B) ? 0x2000 :
			(color_depth == COLORDEPTH_48B) ? 0x2000 : 0x0200;
		csc_scale = 0;
	}
	} else {
		csc_coeff_a1 = 0x2000;
		csc_coeff_a2 = 0x0000;
		csc_coeff_a3 = 0x0000;
		csc_coeff_a4 = 0x0000;
		csc_coeff_b1 = 0x0000;
		csc_coeff_b2 = 0x2000;
		csc_coeff_b3 = 0x0000;
		csc_coeff_b4 = 0x0000;
		csc_coeff_c1 = 0x0000;
		csc_coeff_c2 = 0x0000;
		csc_coeff_c3 = 0x2000;
		csc_coeff_c4 = 0x0000;
		csc_scale = 1;
	}

	data32 = 0;
	data32 |= (color_depth << 4);  /* [7:4] csc_color_depth */
	data32 |= (csc_scale << 0);  /* [1:0] cscscale */

	/* set csc in video path */
	/* set rgb_ycc indicator */
	switch (output_color_format) {
	case HDMI_COLORSPACE_RGB:
		rgb_ycc_indicator = 0x0;
		break;
	case HDMI_COLORSPACE_YUV422:
		rgb_ycc_indicator = 0x1;
		break;
	case HDMI_COLORSPACE_YUV420:
		rgb_ycc_indicator = 0x3;
		break;
	case HDMI_COLORSPACE_YUV444:
	default:
		rgb_ycc_indicator = 0x2;
		break;
	}
}   /* hdmitx_csc_config */

static void hdmitx_set_hw(struct hdmitx_dev *hdev)
{
	pr_info(HW " config hdmitx IP vic = %d cd:%d cs: %d\n",
		hdev->para->timing.vic, hdev->para->cd, hdev->para->cs);

	config_hdmi21_tx(hdev);
}

/* For the ENCP_VIDEO_MAX_LNCNT, it will always start as 0
 * when set this register, here will minus 1
 * and get the value, here will plus 1
 */
void hdmitx_vrr_set_maxlncnt(u32 max_lcnt)
{
	/* max_lcnt can't be 0 for VRR */
	if (!max_lcnt)
		return;

	hd21_write_reg(ENCP_VIDEO_MAX_LNCNT, max_lcnt - 1);
}

u32 hdmitx_vrr_get_maxlncnt(void)
{
	return hd21_read_reg(ENCP_VIDEO_MAX_LNCNT) + 1;
}

int hdmitx21_read_phy_status(void)
{
	int phy_value = 0;

	phy_value = !!(hd21_read_reg(ANACTRL_HDMIPHY_CTRL0) & 0xffff);

	return phy_value;
}

void hdmitx21_dither_config(struct hdmitx_dev *hdev)
{
	struct hdmi_format_para *para = hdev->para;

	if (para->cd == COLORDEPTH_24B && hdmitx21_dv_en() == 0)
		hd21_set_reg_bits(VPU_HDMI_DITH_CNTL, 1, 4, 1);
	else
		hd21_set_reg_bits(VPU_HDMI_DITH_CNTL, 0, 4, 1);
}

/* for emds pkt cuva */
void hdmitx_dhdr_send(u8 *body, int max_size)
{
	u32 data;
	int i;
	int active_lines;
	int blank_lines;
	int hdr_emp_num;
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	if (!body) {
		hdmitx21_wr_reg(D_HDR_INSERT_CTRL_IVCTX, 0);
		return;
	}

	active_lines = hdev->para->timing.v_active;
	blank_lines = hdev->para->timing.v_blank;

	hdr_emp_num = (3 - 1) * 28 + 21;	//emds total send 3 packet as one dhdr
	//  step1: hdr timing
	hdmitx21_wr_reg(D_HDR_VB_LE_IVCTX, (blank_lines & 0xff)); //reg_vb_le default 0x20
	hdmitx21_wr_reg(D_HDR_SPARE_3_IVCTX, 0x2); //[1] reg_fapa_fsm_proper_move

	//  step2: prepare packet data
	hdmitx21_wr_reg(D_HDR_INSERT_PAYLOAD_1_IVCTX, hdr_emp_num >> 8); //payload[15:8] pb5 msb
	hdmitx21_wr_reg(D_HDR_INSERT_PAYLOAD_0_IVCTX, hdr_emp_num & 0xff); //payload[7:0] pb6 lsb

	//setting send ml_ds data  payload =0
	hdmitx21_wr_reg(D_HDR_GEN_CTL_IVCTX, 0xb1); //[0] reg_source_en [6:5] fapa ctrl
	//[0] reg_send_end_data_set [3] reg_send_pkt_cont (send data set for each frame if it is 1)
	hdmitx21_wr_reg(D_HDR_INSERT_CTL_1_IVCTX, 0x8);
	data  = 0;
	data |= (0 << 6); //[7 : 6] rsvd
	data |= (0 << 5); //[    5] reg_set_new_in_ml_ds_pkt
	data |= (0 << 4); //[    4] reg_set_new_in_middle
	data |= (0 << 3); //[    3] reg_set_new_always
	data |= (1 << 2); //[    2] reg_to_err_ctrl
	data |= (0 << 1); //[    1] feg_fapa1_supprot. =0:FAPA start only after second active line.
	data |= (1 << 0); //[    0] reg_set_end_for_ml_ds
	hdmitx21_wr_reg(D_HDR_INSERT_CTL_2_IVCTX, data);

	hdmitx21_wr_reg(D_HDR_ACT_DE_LO_IVCTX, (active_lines & 0xff)); //reg_act_de_lsb
	hdmitx21_wr_reg(D_HDR_ACT_DE_HI_IVCTX, (active_lines >> 8)); //reg_act_de_msb

	//  step3: packet header and content
	hdmitx21_wr_reg(D_HDR_EM_HB0_IVCTX, 0x7f); // HB0 header

	hdmitx21_wr_reg(D_HDR_EM_PB0_IVCTX, 0x00); // pb0 [7] new; [6] end
	hdmitx21_wr_reg(D_HDR_EM_PB2_IVCTX, 0x02); // pb2 ID
	hdmitx21_wr_reg(D_HDR_EM_PB3_IVCTX, 0x00); // pb3
	hdmitx21_wr_reg(D_HDR_EM_PB4_IVCTX, 0x00); // pb4

	hdmitx21_wr_reg(D_HDR_INSERT_CTRL_IVCTX, 0x1);	//[0]reg_pkt_gen; [1]reg_pkt_gen_en
	hdmitx21_wr_reg(D_HDR_MEM_WADDR_RST_IVCTX, 0x1);
	hdmitx21_wr_reg(D_HDR_MEM_WADDR_RST_IVCTX, 0x1);
	hdmitx21_wr_reg(D_HDR_MEM_WADDR_RST_IVCTX, 0x0);
	hdmitx21_wr_reg(D_HDR_MEM_WADDR_RST_IVCTX, 0x0);
	for (i = 0; i < hdr_emp_num && i < max_size; i++) {
		data = *body++;
		hdmitx21_wr_reg(D_HDR_MEM_WDATA_IVCTX, data);
	}
	hdmitx21_wr_reg(D_HDR_INSERT_CTRL_IVCTX, 0x3);
}

