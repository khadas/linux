// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2022 MediaTek Inc.
 */

#include <dt-bindings/clock/mediatek,mt8365-clk.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/delay.h>
#include <linux/mfd/syscon.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include "clk-gate.h"
#include "clk-mtk.h"
#include "clk-mux.h"
#include "clk-pll.h"

static DEFINE_SPINLOCK(mt8365_clk_lock);

static const struct mtk_fixed_clk top_fixed_clks[] = {
	FIXED_CLK(CLK_TOP_I2S0_BCK, "i2s0_bck", NULL, 26000000),
	FIXED_CLK(CLK_TOP_DSI0_LNTC_DSICK, "dsi0_lntc_dsick", "clk26m",
		  75000000),
	FIXED_CLK(CLK_TOP_VPLL_DPIX, "vpll_dpix", "clk26m", 75000000),
	FIXED_CLK(CLK_TOP_LVDSTX_CLKDIG_CTS, "lvdstx_dig_cts", "clk26m",
		  52500000),
};

static const struct mtk_fixed_factor top_divs[] = {
	FACTOR(CLK_TOP_SYS_26M_D2, "sys_26m_d2", "clk26m", 1, 2),
	FACTOR(CLK_TOP_SYSPLL_D2, "syspll_d2", "mainpll", 1, 2),
	FACTOR(CLK_TOP_SYSPLL1_D2, "syspll1_d2", "mainpll", 1, 4),
	FACTOR(CLK_TOP_SYSPLL1_D4, "syspll1_d4", "mainpll", 1, 8),
	FACTOR(CLK_TOP_SYSPLL1_D8, "syspll1_d8", "mainpll", 1, 16),
	FACTOR(CLK_TOP_SYSPLL1_D16, "syspll1_d16", "mainpll", 1, 32),
	FACTOR(CLK_TOP_SYSPLL_D3, "syspll_d3", "mainpll", 1, 3),
	FACTOR(CLK_TOP_SYSPLL2_D2, "syspll2_d2", "mainpll", 1, 6),
	FACTOR(CLK_TOP_SYSPLL2_D4, "syspll2_d4", "mainpll", 1, 12),
	FACTOR(CLK_TOP_SYSPLL2_D8, "syspll2_d8", "mainpll", 1, 24),
	FACTOR(CLK_TOP_SYSPLL_D5, "syspll_d5", "mainpll", 1, 5),
	FACTOR(CLK_TOP_SYSPLL3_D2, "syspll3_d2", "mainpll", 1, 10),
	FACTOR(CLK_TOP_SYSPLL3_D4, "syspll3_d4", "mainpll", 1, 20),
	FACTOR(CLK_TOP_SYSPLL_D7, "syspll_d7", "mainpll", 1, 7),
	FACTOR(CLK_TOP_SYSPLL4_D2, "syspll4_d2", "mainpll", 1, 14),
	FACTOR(CLK_TOP_SYSPLL4_D4, "syspll4_d4", "mainpll", 1, 28),
	FACTOR(CLK_TOP_UNIVPLL, "univpll", "univ_en", 1, 2),
	FACTOR(CLK_TOP_UNIVPLL_D2, "univpll_d2", "univpll", 1, 2),
	FACTOR(CLK_TOP_UNIVPLL1_D2, "univpll1_d2", "univpll", 1, 4),
	FACTOR(CLK_TOP_UNIVPLL1_D4, "univpll1_d4", "univpll", 1, 8),
	FACTOR(CLK_TOP_UNIVPLL_D3, "univpll_d3", "univpll", 1, 3),
	FACTOR(CLK_TOP_UNIVPLL2_D2, "univpll2_d2", "univpll", 1, 6),
	FACTOR(CLK_TOP_UNIVPLL2_D4, "univpll2_d4", "univpll", 1, 12),
	FACTOR(CLK_TOP_UNIVPLL2_D8, "univpll2_d8", "univpll", 1, 24),
	FACTOR(CLK_TOP_UNIVPLL2_D32, "univpll2_d32", "univpll", 1, 96),
	FACTOR(CLK_TOP_UNIVPLL_D5, "univpll_d5", "univpll", 1, 5),
	FACTOR(CLK_TOP_UNIVPLL3_D2, "univpll3_d2", "univpll", 1, 10),
	FACTOR(CLK_TOP_UNIVPLL3_D4, "univpll3_d4", "univpll", 1, 20),
	FACTOR(CLK_TOP_MMPLL, "mmpll_ck", "mmpll", 1, 1),
	FACTOR(CLK_TOP_MMPLL_D2, "mmpll_d2", "mmpll", 1, 2),
	FACTOR(CLK_TOP_MFGPLL, "mfgpll_ck", "mfgpll", 1, 1),
	FACTOR(CLK_TOP_LVDSPLL_D2, "lvdspll_d2", "lvdspll", 1, 2),
	FACTOR(CLK_TOP_LVDSPLL_D4, "lvdspll_d4", "lvdspll", 1, 4),
	FACTOR(CLK_TOP_LVDSPLL_D8, "lvdspll_d8", "lvdspll", 1, 8),
	FACTOR(CLK_TOP_LVDSPLL_D16, "lvdspll_d16", "lvdspll", 1, 16),
	FACTOR(CLK_TOP_USB20_192M, "usb20_192m_ck", "usb20_en", 1, 13),
	FACTOR(CLK_TOP_USB20_192M_D4, "usb20_192m_d4", "usb20_192m_ck", 1, 4),
	FACTOR(CLK_TOP_USB20_192M_D8, "usb20_192m_d8", "usb20_192m_ck", 1, 8),
	FACTOR(CLK_TOP_USB20_192M_D16, "usb20_192m_d16", "usb20_192m_ck",
	       1, 16),
	FACTOR(CLK_TOP_USB20_192M_D32, "usb20_192m_d32", "usb20_192m_ck",
	       1, 32),
	FACTOR(CLK_TOP_APLL1, "apll1_ck", "apll1", 1, 1),
	FACTOR(CLK_TOP_APLL1_D2, "apll1_d2", "apll1_ck", 1, 2),
	FACTOR(CLK_TOP_APLL1_D4, "apll1_d4", "apll1_ck", 1, 4),
	FACTOR(CLK_TOP_APLL1_D8, "apll1_d8", "apll1_ck", 1, 8),
	FACTOR(CLK_TOP_APLL2, "apll2_ck", "apll2", 1, 1),
	FACTOR(CLK_TOP_APLL2_D2, "apll2_d2", "apll2_ck", 1, 2),
	FACTOR(CLK_TOP_APLL2_D4, "apll2_d4", "apll2_ck", 1, 4),
	FACTOR(CLK_TOP_APLL2_D8, "apll2_d8", "apll2_ck", 1, 8),
	FACTOR(CLK_TOP_MSDCPLL, "msdcpll_ck", "msdcpll", 1, 1),
	FACTOR(CLK_TOP_MSDCPLL_D2, "msdcpll_d2", "msdcpll", 1, 2),
	FACTOR(CLK_TOP_DSPPLL, "dsppll_ck", "dsppll", 1, 1),
	FACTOR(CLK_TOP_DSPPLL_D2, "dsppll_d2", "dsppll", 1, 2),
	FACTOR(CLK_TOP_DSPPLL_D4, "dsppll_d4", "dsppll", 1, 4),
	FACTOR(CLK_TOP_DSPPLL_D8, "dsppll_d8", "dsppll", 1, 8),
	FACTOR(CLK_TOP_APUPLL, "apupll_ck", "apupll", 1, 1),
	FACTOR(CLK_TOP_CLK26M_D52, "clk26m_d52", "clk26m", 1, 52),
};

static const char * const axi_parents[] = {
	"clk26m",
	"syspll_d7",
	"syspll1_d4",
	"syspll3_d2"
};

static const char * const mem_parents[] = {
	"clk26m",
	"mmpll_ck",
	"syspll_d3",
	"syspll1_d2"
};

static const char * const mm_parents[] = {
	"clk26m",
	"mmpll_ck",
	"syspll1_d2",
	"syspll_d5",
	"syspll1_d4",
	"univpll_d5",
	"univpll1_d2",
	"mmpll_d2"
};

static const char * const scp_parents[] = {
	"clk26m",
	"syspll4_d2",
	"univpll2_d2",
	"syspll1_d2",
	"univpll1_d2",
	"syspll_d3",
	"univpll_d3"
};

static const char * const mfg_parents[] = {
	"clk26m",
	"mfgpll_ck",
	"syspll_d3",
	"univpll_d3"
};

static const char * const atb_parents[] = {
	"clk26m",
	"syspll1_d4",
	"syspll1_d2"
};

static const char * const camtg_parents[] = {
	"clk26m",
	"usb20_192m_d8",
	"univpll2_d8",
	"usb20_192m_d4",
	"univpll2_d32",
	"usb20_192m_d16",
	"usb20_192m_d32"
};

static const char * const uart_parents[] = {
	"clk26m",
	"univpll2_d8"
};

static const char * const spi_parents[] = {
	"clk26m",
	"univpll2_d2",
	"univpll2_d4",
	"univpll2_d8"
};

static const char * const msdc50_0_hc_parents[] = {
	"clk26m",
	"syspll1_d2",
	"univpll1_d4",
	"syspll2_d2"
};

static const char * const msdc50_0_parents[] = {
	"clk26m",
	"msdcpll_ck",
	"univpll1_d2",
	"syspll1_d2",
	"univpll_d5",
	"syspll2_d2",
	"univpll1_d4",
	"syspll4_d2"
};

static const char * const msdc50_2_parents[] = {
	"clk26m",
	"msdcpll_ck",
	"univpll_d3",
	"univpll1_d2",
	"syspll1_d2",
	"univpll2_d2",
	"syspll2_d2",
	"univpll1_d4"
};

static const char * const msdc30_1_parents[] = {
	"clk26m",
	"msdcpll_d2",
	"univpll2_d2",
	"syspll2_d2",
	"univpll1_d4",
	"syspll1_d4",
	"syspll2_d4",
	"univpll2_d8"
};

static const char * const audio_parents[] = {
	"clk26m",
	"syspll3_d4",
	"syspll4_d4",
	"syspll1_d16"
};

static const char * const aud_intbus_parents[] = {
	"clk26m",
	"syspll1_d4",
	"syspll4_d2"
};

static const char * const aud_1_parents[] = {
	"clk26m",
	"apll1_ck"
};

static const char * const aud_2_parents[] = {
	"clk26m",
	"apll2_ck"
};

static const char * const aud_engen1_parents[] = {
	"clk26m",
	"apll1_d2",
	"apll1_d4",
	"apll1_d8"
};

static const char * const aud_engen2_parents[] = {
	"clk26m",
	"apll2_d2",
	"apll2_d4",
	"apll2_d8"
};

static const char * const aud_spdif_parents[] = {
	"clk26m",
	"univpll_d2"
};

static const char * const disp_pwm_parents[] = {
	"clk26m",
	"univpll2_d4"
};

static const char * const dxcc_parents[] = {
	"clk26m",
	"syspll1_d2",
	"syspll1_d4",
	"syspll1_d8"
};

static const char * const ssusb_sys_parents[] = {
	"clk26m",
	"univpll3_d4",
	"univpll2_d4",
	"univpll3_d2"
};

static const char * const spm_parents[] = {
	"clk26m",
	"syspll1_d8"
};

static const char * const i2c_parents[] = {
	"clk26m",
	"univpll3_d4",
	"univpll3_d2",
	"syspll1_d8",
	"syspll2_d8"
};

static const char * const pwm_parents[] = {
	"clk26m",
	"univpll3_d4",
	"syspll1_d8"
};

static const char * const senif_parents[] = {
	"clk26m",
	"univpll1_d4",
	"univpll1_d2",
	"univpll2_d2"
};

static const char * const aes_fde_parents[] = {
	"clk26m",
	"msdcpll_ck",
	"univpll_d3",
	"univpll2_d2",
	"univpll1_d2",
	"syspll1_d2"
};

static const char * const dpi0_parents[] = {
	"clk26m",
	"lvdspll_d2",
	"lvdspll_d4",
	"lvdspll_d8",
	"lvdspll_d16"
};

static const char * const dsp_parents[] = {
	"clk26m",
	"sys_26m_d2",
	"dsppll_ck",
	"dsppll_d2",
	"dsppll_d4",
	"dsppll_d8"
};

static const char * const nfi2x_parents[] = {
	"clk26m",
	"syspll2_d2",
	"syspll_d7",
	"syspll_d3",
	"syspll2_d4",
	"msdcpll_d2",
	"univpll1_d2",
	"univpll_d5"
};

static const char * const nfiecc_parents[] = {
	"clk26m",
	"syspll4_d2",
	"univpll2_d4",
	"syspll_d7",
	"univpll1_d2",
	"syspll1_d2",
	"univpll2_d2",
	"syspll_d5"
};

static const char * const ecc_parents[] = {
	"clk26m",
	"univpll2_d2",
	"univpll1_d2",
	"univpll_d3",
	"syspll_d2"
};

static const char * const eth_parents[] = {
	"clk26m",
	"univpll2_d8",
	"syspll4_d4",
	"syspll1_d8",
	"syspll4_d2"
};

static const char * const gcpu_parents[] = {
	"clk26m",
	"univpll_d3",
	"univpll2_d2",
	"syspll_d3",
	"syspll2_d2"
};

static const char * const gcpu_cpm_parents[] = {
	"clk26m",
	"univpll2_d2",
	"syspll2_d2"
};

static const char * const apu_parents[] = {
	"clk26m",
	"univpll_d2",
	"apupll_ck",
	"mmpll_ck",
	"syspll_d3",
	"univpll1_d2",
	"syspll1_d2",
	"syspll1_d4"
};

static const char * const mbist_diag_parents[] = {
	"clk26m",
	"syspll4_d4",
	"univpll2_d8"
};

static const char * const apll_i2s0_parents[] = {
	"aud_1_sel",
	"aud_2_sel"
};

static struct mtk_composite top_misc_mux_gates[] = {
	/* CLK_CFG_11 */
	MUX_GATE(CLK_TOP_MBIST_DIAG_SEL, "mbist_diag_sel", mbist_diag_parents,
		 0x0ec, 0, 2, 7),
};

struct mt8365_clk_audio_mux {
	int id;
	const char *name;
	u8 shift;
};

static struct mt8365_clk_audio_mux top_misc_muxes[] = {
	{ CLK_TOP_APLL_I2S0_SEL, "apll_i2s0_sel", 11},
	{ CLK_TOP_APLL_I2S1_SEL, "apll_i2s1_sel", 12},
	{ CLK_TOP_APLL_I2S2_SEL, "apll_i2s2_sel", 13},
	{ CLK_TOP_APLL_I2S3_SEL, "apll_i2s3_sel", 14},
	{ CLK_TOP_APLL_TDMOUT_SEL, "apll_tdmout_sel", 15},
	{ CLK_TOP_APLL_TDMIN_SEL, "apll_tdmin_sel", 16},
	{ CLK_TOP_APLL_SPDIF_SEL, "apll_spdif_sel", 17},
};

#define CLK_CFG_UPDATE 0x004
#define CLK_CFG_UPDATE1 0x008

static const struct mtk_mux top_muxes[] = {
	/* CLK_CFG_0 */
	MUX_GATE_CLR_SET_UPD_FLAGS(CLK_TOP_AXI_SEL, "axi_sel", axi_parents,
				   0x040, 0x044, 0x048, 0, 2, 7, CLK_CFG_UPDATE,
				   0, CLK_IS_CRITICAL),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MEM_SEL, "mem_sel", mem_parents, 0x040,
			     0x044, 0x048, 8, 2, 15, CLK_CFG_UPDATE, 1),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MM_SEL, "mm_sel", mm_parents, 0x040, 0x044,
			     0x048, 16, 3, 23, CLK_CFG_UPDATE, 2),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SCP_SEL, "scp_sel", scp_parents, 0x040,
			     0x044, 0x048, 24, 3, 31, CLK_CFG_UPDATE, 3),
	/* CLK_CFG_1 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MFG_SEL, "mfg_sel", mfg_parents, 0x050,
			     0x054, 0x058, 0, 2, 7, CLK_CFG_UPDATE, 4),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_ATB_SEL, "atb_sel", atb_parents, 0x050,
			     0x054, 0x058, 8, 2, 15, CLK_CFG_UPDATE, 5),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_CAMTG_SEL, "camtg_sel", camtg_parents,
			     0x050, 0x054, 0x058, 16, 3, 23, CLK_CFG_UPDATE, 6),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_CAMTG1_SEL, "camtg1_sel", camtg_parents,
			     0x050, 0x054, 0x058, 24, 3, 31, CLK_CFG_UPDATE, 7),
	/* CLK_CFG_2 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_UART_SEL, "uart_sel", uart_parents, 0x060,
			     0x064, 0x068, 0, 1, 7, CLK_CFG_UPDATE, 8),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SPI_SEL, "spi_sel", spi_parents, 0x060,
			     0x064, 0x068, 8, 2, 15, CLK_CFG_UPDATE, 9),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MSDC50_0_HC_SEL, "msdc50_0_hc_sel",
			     msdc50_0_hc_parents, 0x060, 0x064, 0x068, 16, 2,
			     23, CLK_CFG_UPDATE, 10),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MSDC2_2_HC_SEL, "msdc2_2_hc_sel",
			     msdc50_0_hc_parents, 0x060, 0x064, 0x068, 24, 2,
			     31, CLK_CFG_UPDATE, 11),
	/* CLK_CFG_3 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MSDC50_0_SEL, "msdc50_0_sel",
			     msdc50_0_parents, 0x070, 0x074, 0x078, 0, 3, 7,
			     CLK_CFG_UPDATE, 12),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MSDC50_2_SEL, "msdc50_2_sel",
			     msdc50_2_parents, 0x070, 0x074, 0x078, 8, 3, 15,
			     CLK_CFG_UPDATE, 13),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MSDC30_1_SEL, "msdc30_1_sel",
			     msdc30_1_parents, 0x070, 0x074, 0x078, 16, 3, 23,
			     CLK_CFG_UPDATE, 14),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AUDIO_SEL, "audio_sel", audio_parents,
			     0x070, 0x074, 0x078, 24, 2, 31, CLK_CFG_UPDATE,
			     15),
	/* CLK_CFG_4 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AUD_INTBUS_SEL, "aud_intbus_sel",
			     aud_intbus_parents, 0x080, 0x084, 0x088, 0, 2, 7,
			     CLK_CFG_UPDATE, 16),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AUD_1_SEL, "aud_1_sel", aud_1_parents,
			     0x080, 0x084, 0x088, 8, 1, 15, CLK_CFG_UPDATE, 17),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AUD_2_SEL, "aud_2_sel", aud_2_parents,
			     0x080, 0x084, 0x088, 16, 1, 23, CLK_CFG_UPDATE,
			     18),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AUD_ENGEN1_SEL, "aud_engen1_sel",
			     aud_engen1_parents, 0x080, 0x084, 0x088, 24, 2, 31,
			     CLK_CFG_UPDATE, 19),
	/* CLK_CFG_5 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AUD_ENGEN2_SEL, "aud_engen2_sel",
			     aud_engen2_parents, 0x090, 0x094, 0x098, 0, 2, 7,
			     CLK_CFG_UPDATE, 20),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AUD_SPDIF_SEL, "aud_spdif_sel",
			     aud_spdif_parents, 0x090, 0x094, 0x098, 8, 1, 15,
			     CLK_CFG_UPDATE, 21),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_DISP_PWM_SEL, "disp_pwm_sel",
			     disp_pwm_parents, 0x090, 0x094, 0x098, 16, 2, 23,
			     CLK_CFG_UPDATE, 22),
	/* CLK_CFG_6 */
	MUX_GATE_CLR_SET_UPD_FLAGS(CLK_TOP_DXCC_SEL, "dxcc_sel", dxcc_parents,
				   0x0a0, 0x0a4, 0x0a8, 0, 2, 7, CLK_CFG_UPDATE,
				   24, CLK_IS_CRITICAL),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SSUSB_SYS_SEL, "ssusb_sys_sel",
			     ssusb_sys_parents, 0x0a0, 0x0a4, 0x0a8, 8, 2, 15,
			     CLK_CFG_UPDATE, 25),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SSUSB_XHCI_SEL, "ssusb_xhci_sel",
			     ssusb_sys_parents, 0x0a0, 0x0a4, 0x0a8, 16, 2, 23,
			     CLK_CFG_UPDATE, 26),
	MUX_GATE_CLR_SET_UPD_FLAGS(CLK_TOP_SPM_SEL, "spm_sel", spm_parents,
				   0x0a0, 0x0a4, 0x0a8, 24, 1, 31,
				   CLK_CFG_UPDATE, 27, CLK_IS_CRITICAL),
	/* CLK_CFG_7 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_I2C_SEL, "i2c_sel", i2c_parents, 0x0b0,
			     0x0b4, 0x0b8, 0, 3, 7, CLK_CFG_UPDATE, 28),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_PWM_SEL, "pwm_sel", pwm_parents, 0x0b0,
			     0x0b4, 0x0b8, 8, 2, 15, CLK_CFG_UPDATE, 29),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SENIF_SEL, "senif_sel", senif_parents,
			     0x0b0, 0x0b4, 0x0b8, 16, 2, 23, CLK_CFG_UPDATE,
			     30),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AES_FDE_SEL, "aes_fde_sel",
			     aes_fde_parents, 0x0b0, 0x0b4, 0x0b8, 24, 3, 31,
			     CLK_CFG_UPDATE, 31),
	/* CLK_CFG_8 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_CAMTM_SEL, "camtm_sel", senif_parents,
			     0x0c0, 0x0c4, 0x0c8, 0, 2, 7, CLK_CFG_UPDATE1, 0),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_DPI0_SEL, "dpi0_sel", dpi0_parents, 0x0c0,
			     0x0c4, 0x0c8, 8, 3, 15, CLK_CFG_UPDATE1, 1),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_DPI1_SEL, "dpi1_sel", dpi0_parents, 0x0c0,
			     0x0c4, 0x0c8, 16, 3, 23, CLK_CFG_UPDATE1, 2),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_DSP_SEL, "dsp_sel", dsp_parents, 0x0c0,
			     0x0c4, 0x0c8, 24, 3, 31, CLK_CFG_UPDATE1, 3),
	/* CLK_CFG_9 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_NFI2X_SEL, "nfi2x_sel", nfi2x_parents,
			     0x0d0, 0x0d4, 0x0d8, 0, 3, 7, CLK_CFG_UPDATE1, 4),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_NFIECC_SEL, "nfiecc_sel", nfiecc_parents,
			     0x0d0, 0x0d4, 0x0d8, 8, 3, 15, CLK_CFG_UPDATE1, 5),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_ECC_SEL, "ecc_sel", ecc_parents, 0x0d0,
			     0x0d4, 0x0d8, 16, 3, 23, CLK_CFG_UPDATE1, 6),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_ETH_SEL, "eth_sel", eth_parents, 0x0d0,
			     0x0d4, 0x0d8, 24, 3, 31, CLK_CFG_UPDATE1, 7),
	/* CLK_CFG_10 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_GCPU_SEL, "gcpu_sel", gcpu_parents, 0x0e0,
			     0x0e4, 0x0e8, 0, 3, 7, CLK_CFG_UPDATE1, 8),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_GCPU_CPM_SEL, "gcpu_cpm_sel",
			     gcpu_cpm_parents, 0x0e0, 0x0e4, 0x0e8, 8, 2, 15,
			     CLK_CFG_UPDATE1, 9),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_APU_SEL, "apu_sel", apu_parents, 0x0e0,
			     0x0e4, 0x0e8, 16, 3, 23, CLK_CFG_UPDATE1, 10),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_APU_IF_SEL, "apu_if_sel", apu_parents,
			     0x0e0, 0x0e4, 0x0e8, 24, 3, 31, CLK_CFG_UPDATE1,
			     11),
};

static const char * const mcu_bus_parents[] = {
	"clk26m",
	"armpll",
	"mainpll",
	"univpll_d2"
};

static struct mtk_composite mcu_muxes[] = {
	/* bus_pll_divider_cfg */
	MUX_GATE_FLAGS(CLK_MCU_BUS_SEL, "mcu_bus_sel", mcu_bus_parents, 0x7C0,
		       9, 2, -1, CLK_SET_RATE_PARENT | CLK_IS_CRITICAL),
};

#define DIV_ADJ_F(_id, _name, _parent, _reg, _shift, _width, _flags) {	\
		.id = _id,					\
		.name = _name,					\
		.parent_name = _parent,				\
		.div_reg = _reg,				\
		.div_shift = _shift,				\
		.div_width = _width,				\
		.clk_divider_flags = _flags,			\
}

static const struct mtk_clk_divider top_adj_divs[] = {
	DIV_ADJ_F(CLK_TOP_APLL12_CK_DIV0, "apll12_ck_div0", "apll_i2s0_sel",
		  0x324, 0, 8, CLK_DIVIDER_ROUND_CLOSEST),
	DIV_ADJ_F(CLK_TOP_APLL12_CK_DIV1, "apll12_ck_div1", "apll_i2s1_sel",
		  0x324, 8, 8, CLK_DIVIDER_ROUND_CLOSEST),
	DIV_ADJ_F(CLK_TOP_APLL12_CK_DIV2, "apll12_ck_div2", "apll_i2s2_sel",
		  0x324, 16, 8, CLK_DIVIDER_ROUND_CLOSEST),
	DIV_ADJ_F(CLK_TOP_APLL12_CK_DIV3, "apll12_ck_div3", "apll_i2s3_sel",
		  0x324, 24, 8, CLK_DIVIDER_ROUND_CLOSEST),
	DIV_ADJ_F(CLK_TOP_APLL12_CK_DIV6, "apll12_ck_div6", "apll_spdif_sel",
		  0x32c, 0, 8, CLK_DIVIDER_ROUND_CLOSEST),
};

struct mtk_simple_gate {
	int id;
	const char *name;
	const char *parent;
	u32 reg;
	u8 shift;
	unsigned long gate_flags;
};

static const struct mtk_simple_gate top_clk_gates[] = {
	{ CLK_TOP_CONN_32K, "conn_32k", "clk32k", 0x0, 10, CLK_GATE_SET_TO_DISABLE },
	{ CLK_TOP_CONN_26M, "conn_26m", "clk26m", 0x0, 11, CLK_GATE_SET_TO_DISABLE },
	{ CLK_TOP_DSP_32K, "dsp_32k", "clk32k", 0x0, 16, CLK_GATE_SET_TO_DISABLE },
	{ CLK_TOP_DSP_26M, "dsp_26m", "clk26m", 0x0, 17, CLK_GATE_SET_TO_DISABLE },
	{ CLK_TOP_USB20_48M_EN, "usb20_48m_en", "usb20_192m_d4", 0x104, 8, 0 },
	{ CLK_TOP_UNIVPLL_48M_EN, "univpll_48m_en", "usb20_192m_d4", 0x104, 9, 0 },
	{ CLK_TOP_LVDSTX_CLKDIG_EN, "lvdstx_dig_en", "lvdstx_dig_cts", 0x104, 20, 0 },
	{ CLK_TOP_VPLL_DPIX_EN, "vpll_dpix_en", "vpll_dpix", 0x104, 21, 0 },
	{ CLK_TOP_SSUSB_TOP_CK_EN, "ssusb_top_ck_en", NULL, 0x104, 22, 0 },
	{ CLK_TOP_SSUSB_PHY_CK_EN, "ssusb_phy_ck_en", NULL, 0x104, 23, 0 },
	{ CLK_TOP_AUD_I2S0_M, "aud_i2s0_m_ck", "apll12_ck_div0", 0x320, 0, 0 },
	{ CLK_TOP_AUD_I2S1_M, "aud_i2s1_m_ck", "apll12_ck_div1", 0x320, 1, 0 },
	{ CLK_TOP_AUD_I2S2_M, "aud_i2s2_m_ck", "apll12_ck_div2", 0x320, 2, 0 },
	{ CLK_TOP_AUD_I2S3_M, "aud_i2s3_m_ck", "apll12_ck_div3", 0x320, 3, 0 },
	{ CLK_TOP_AUD_TDMOUT_M, "aud_tdmout_m_ck", "apll12_ck_div4", 0x320, 4, 0 },
	{ CLK_TOP_AUD_TDMOUT_B, "aud_tdmout_b_ck", "apll12_ck_div4b", 0x320, 5, 0 },
	{ CLK_TOP_AUD_TDMIN_M, "aud_tdmin_m_ck", "apll12_ck_div5", 0x320, 6, 0 },
	{ CLK_TOP_AUD_TDMIN_B, "aud_tdmin_b_ck", "apll12_ck_div5b", 0x320, 7, 0 },
	{ CLK_TOP_AUD_SPDIF_M, "aud_spdif_m_ck", "apll12_ck_div6", 0x320, 8, 0 },
};

static const struct mtk_gate_regs ifr2_cg_regs = {
	.set_ofs = 0x80,
	.clr_ofs = 0x84,
	.sta_ofs = 0x90,
};

static const struct mtk_gate_regs ifr3_cg_regs = {
	.set_ofs = 0x88,
	.clr_ofs = 0x8c,
	.sta_ofs = 0x94,
};

static const struct mtk_gate_regs ifr4_cg_regs = {
	.set_ofs = 0xa4,
	.clr_ofs = 0xa8,
	.sta_ofs = 0xac,
};

static const struct mtk_gate_regs ifr5_cg_regs = {
	.set_ofs = 0xc0,
	.clr_ofs = 0xc4,
	.sta_ofs = 0xc8,
};

static const struct mtk_gate_regs ifr6_cg_regs = {
	.set_ofs = 0xd0,
	.clr_ofs = 0xd4,
	.sta_ofs = 0xd8,
};

#define GATE_IFR2(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ifr2_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_IFR3(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ifr3_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_IFR4(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ifr4_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_IFR5(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ifr5_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_IFR6(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ifr6_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate ifr_clks[] = {
	/* IFR2 */
	GATE_IFR2(CLK_IFR_PMIC_TMR, "ifr_pmic_tmr", "clk26m", 0),
	GATE_IFR2(CLK_IFR_PMIC_AP, "ifr_pmic_ap", "clk26m", 1),
	GATE_IFR2(CLK_IFR_PMIC_MD, "ifr_pmic_md", "clk26m", 2),
	GATE_IFR2(CLK_IFR_PMIC_CONN, "ifr_pmic_conn", "clk26m", 3),
	GATE_IFR2(CLK_IFR_ICUSB, "ifr_icusb", "axi_sel", 8),
	GATE_IFR2(CLK_IFR_GCE, "ifr_gce", "axi_sel", 9),
	GATE_IFR2(CLK_IFR_THERM, "ifr_therm", "axi_sel", 10),
	GATE_IFR2(CLK_IFR_PWM_HCLK, "ifr_pwm_hclk", "axi_sel", 15),
	GATE_IFR2(CLK_IFR_PWM1, "ifr_pwm1", "pwm_sel", 16),
	GATE_IFR2(CLK_IFR_PWM2, "ifr_pwm2", "pwm_sel", 17),
	GATE_IFR2(CLK_IFR_PWM3, "ifr_pwm3", "pwm_sel", 18),
	GATE_IFR2(CLK_IFR_PWM4, "ifr_pwm4", "pwm_sel", 19),
	GATE_IFR2(CLK_IFR_PWM5, "ifr_pwm5", "pwm_sel", 20),
	GATE_IFR2(CLK_IFR_PWM, "ifr_pwm", "pwm_sel", 21),
	GATE_IFR2(CLK_IFR_UART0, "ifr_uart0", "uart_sel", 22),
	GATE_IFR2(CLK_IFR_UART1, "ifr_uart1", "uart_sel", 23),
	GATE_IFR2(CLK_IFR_UART2, "ifr_uart2", "uart_sel", 24),
	GATE_IFR2(CLK_IFR_DSP_UART, "ifr_dsp_uart", "uart_sel", 26),
	GATE_IFR2(CLK_IFR_GCE_26M, "ifr_gce_26m", "clk26m", 27),
	GATE_IFR2(CLK_IFR_CQ_DMA_FPC, "ifr_cq_dma_fpc", "axi_sel", 28),
	GATE_IFR2(CLK_IFR_BTIF, "ifr_btif", "axi_sel", 31),
	/* IFR3 */
	GATE_IFR3(CLK_IFR_SPI0, "ifr_spi0", "spi_sel", 1),
	GATE_IFR3(CLK_IFR_MSDC0_HCLK, "ifr_msdc0", "msdc50_0_hc_sel", 2),
	GATE_IFR3(CLK_IFR_MSDC2_HCLK, "ifr_msdc2", "msdc2_2_hc_sel", 3),
	GATE_IFR3(CLK_IFR_MSDC1_HCLK, "ifr_msdc1", "axi_sel", 4),
	GATE_IFR3(CLK_IFR_DVFSRC, "ifr_dvfsrc", "clk26m", 7),
	GATE_IFR3(CLK_IFR_GCPU, "ifr_gcpu", "axi_sel", 8),
	GATE_IFR3(CLK_IFR_TRNG, "ifr_trng", "axi_sel", 9),
	GATE_IFR3(CLK_IFR_AUXADC, "ifr_auxadc", "clk26m", 10),
	GATE_IFR3(CLK_IFR_AUXADC_MD, "ifr_auxadc_md", "clk26m", 14),
	GATE_IFR3(CLK_IFR_AP_DMA, "ifr_ap_dma", "axi_sel", 18),
	GATE_IFR3(CLK_IFR_DEBUGSYS, "ifr_debugsys", "axi_sel", 24),
	GATE_IFR3(CLK_IFR_AUDIO, "ifr_audio", "axi_sel", 25),
	/* IFR4 */
	GATE_IFR4(CLK_IFR_PWM_FBCLK6, "ifr_pwm_fbclk6", "pwm_sel", 0),
	GATE_IFR4(CLK_IFR_DISP_PWM, "ifr_disp_pwm", "disp_pwm_sel", 2),
	GATE_IFR4(CLK_IFR_AUD_26M_BK, "ifr_aud_26m_bk", "clk26m", 4),
	GATE_IFR4(CLK_IFR_CQ_DMA, "ifr_cq_dma", "axi_sel", 27),
	/* IFR5 */
	GATE_IFR5(CLK_IFR_MSDC0_SF, "ifr_msdc0_sf", "msdc50_0_sel", 0),
	GATE_IFR5(CLK_IFR_MSDC1_SF, "ifr_msdc1_sf", "msdc50_0_sel", 1),
	GATE_IFR5(CLK_IFR_MSDC2_SF, "ifr_msdc2_sf", "msdc50_0_sel", 2),
	GATE_IFR5(CLK_IFR_AP_MSDC0, "ifr_ap_msdc0", "msdc50_0_sel", 7),
	GATE_IFR5(CLK_IFR_MD_MSDC0, "ifr_md_msdc0", "msdc50_0_sel", 8),
	GATE_IFR5(CLK_IFR_MSDC0_SRC, "ifr_msdc0_src", "msdc50_0_sel", 9),
	GATE_IFR5(CLK_IFR_MSDC1_SRC, "ifr_msdc1_src", "msdc30_1_sel", 10),
	GATE_IFR5(CLK_IFR_MSDC2_SRC, "ifr_msdc2_src", "msdc50_2_sel", 11),
	GATE_IFR5(CLK_IFR_PWRAP_TMR, "ifr_pwrap_tmr", "clk26m", 12),
	GATE_IFR5(CLK_IFR_PWRAP_SPI, "ifr_pwrap_spi", "clk26m", 13),
	GATE_IFR5(CLK_IFR_PWRAP_SYS, "ifr_pwrap_sys", "clk26m", 14),
	GATE_IFR5(CLK_IFR_IRRX_26M, "ifr_irrx_26m", "clk26m", 22),
	GATE_IFR5(CLK_IFR_IRRX_32K, "ifr_irrx_32k", "clk32k", 23),
	GATE_IFR5(CLK_IFR_I2C0_AXI, "ifr_i2c0_axi", "i2c_sel", 24),
	GATE_IFR5(CLK_IFR_I2C1_AXI, "ifr_i2c1_axi", "i2c_sel", 25),
	GATE_IFR5(CLK_IFR_I2C2_AXI, "ifr_i2c2_axi", "i2c_sel", 26),
	GATE_IFR5(CLK_IFR_I2C3_AXI, "ifr_i2c3_axi", "i2c_sel", 27),
	GATE_IFR5(CLK_IFR_NIC_AXI, "ifr_nic_axi", "axi_sel", 28),
	GATE_IFR5(CLK_IFR_NIC_SLV_AXI, "ifr_nic_slv_axi", "axi_sel", 29),
	GATE_IFR5(CLK_IFR_APU_AXI, "ifr_apu_axi", "axi_sel", 30),
	/* IFR6 */
	GATE_IFR6(CLK_IFR_NFIECC, "ifr_nfiecc", "nfiecc_sel", 0),
	GATE_IFR6(CLK_IFR_NFI1X_BK, "ifr_nfi1x_bk", "nfi2x_sel", 1),
	GATE_IFR6(CLK_IFR_NFIECC_BK, "ifr_nfiecc_bk", "nfi2x_sel", 2),
	GATE_IFR6(CLK_IFR_NFI_BK, "ifr_nfi_bk", "axi_sel", 3),
	GATE_IFR6(CLK_IFR_MSDC2_AP_BK, "ifr_msdc2_ap_bk", "axi_sel", 4),
	GATE_IFR6(CLK_IFR_MSDC2_MD_BK, "ifr_msdc2_md_bk", "axi_sel", 5),
	GATE_IFR6(CLK_IFR_MSDC2_BK, "ifr_msdc2_bk", "axi_sel", 6),
	GATE_IFR6(CLK_IFR_SUSB_133_BK, "ifr_susb_133_bk", "axi_sel", 7),
	GATE_IFR6(CLK_IFR_SUSB_66_BK, "ifr_susb_66_bk", "axi_sel", 8),
	GATE_IFR6(CLK_IFR_SSUSB_SYS, "ifr_ssusb_sys", "ssusb_sys_sel", 9),
	GATE_IFR6(CLK_IFR_SSUSB_REF, "ifr_ssusb_ref", "ssusb_sys_sel", 10),
	GATE_IFR6(CLK_IFR_SSUSB_XHCI, "ifr_ssusb_xhci", "ssusb_xhci_sel", 11),
};

static const struct mtk_simple_gate peri_clks[] = {
	{ CLK_PERIAXI, "periaxi", "axi_sel", 0x20c, 31, 0 },
};

#define MT8365_PLL_FMAX		(3800UL * MHZ)
#define MT8365_PLL_FMIN		(1500UL * MHZ)
#define CON0_MT8365_RST_BAR	BIT(23)

#define PLL_B(_id, _name, _reg, _pwr_reg, _en_mask, _flags, _pcwbits,	\
		_pd_reg, _pd_shift, _tuner_reg, _tuner_en_reg,		\
		_tuner_en_bit,	_pcw_reg, _pcw_shift, _div_table,	\
		_rst_bar_mask, _pcw_chg_reg) {				\
		.id = _id,						\
		.name = _name,						\
		.reg = _reg,						\
		.pwr_reg = _pwr_reg,					\
		.en_mask = _en_mask,					\
		.flags = _flags,					\
		.rst_bar_mask = _rst_bar_mask,				\
		.fmax = MT8365_PLL_FMAX,				\
		.fmin = MT8365_PLL_FMIN,				\
		.pcwbits = _pcwbits,					\
		.pcwibits = 8,						\
		.pd_reg = _pd_reg,					\
		.pd_shift = _pd_shift,					\
		.tuner_reg = _tuner_reg,				\
		.tuner_en_reg = _tuner_en_reg,				\
		.tuner_en_bit = _tuner_en_bit,				\
		.pcw_reg = _pcw_reg,					\
		.pcw_shift = _pcw_shift,				\
		.pcw_chg_reg = _pcw_chg_reg,				\
		.div_table = _div_table,				\
	}

#define PLL(_id, _name, _reg, _pwr_reg, _en_mask, _flags, _pcwbits,	\
			_pd_reg, _pd_shift, _tuner_reg,			\
			_tuner_en_reg, _tuner_en_bit, _pcw_reg,		\
			_pcw_shift, _rst_bar_mask, _pcw_chg_reg)	\
		PLL_B(_id, _name, _reg, _pwr_reg, _en_mask, _flags,	\
			_pcwbits, _pd_reg, _pd_shift,			\
			_tuner_reg, _tuner_en_reg, _tuner_en_bit,	\
			_pcw_reg, _pcw_shift, NULL, _rst_bar_mask,	\
			_pcw_chg_reg)					\

static const struct mtk_pll_div_table armpll_div_table[] = {
	{ .div = 0, .freq = MT8365_PLL_FMAX },
	{ .div = 1, .freq = 1500 * MHZ },
	{ .div = 2, .freq = 750 * MHZ },
	{ .div = 3, .freq = 375 * MHZ },
	{ .div = 4, .freq = 182500000 },
	{ } /* sentinel */
};

static const struct mtk_pll_div_table mfgpll_div_table[] = {
	{ .div = 0, .freq = MT8365_PLL_FMAX },
	{ .div = 1, .freq = 1600 * MHZ },
	{ .div = 2, .freq = 800 * MHZ },
	{ .div = 3, .freq = 400 * MHZ },
	{ .div = 4, .freq = 200 * MHZ },
	{ } /* sentinel */
};

static const struct mtk_pll_div_table dsppll_div_table[] = {
	{ .div = 0, .freq = MT8365_PLL_FMAX },
	{ .div = 1, .freq = 1600 * MHZ },
	{ .div = 2, .freq = 600 * MHZ },
	{ .div = 3, .freq = 400 * MHZ },
	{ .div = 4, .freq = 200 * MHZ },
	{ } /* sentinel */
};

static const struct mtk_pll_data plls[] = {
	PLL_B(CLK_APMIXED_ARMPLL, "armpll", 0x030C, 0x0318, 0x00000001, PLL_AO,
	      22, 0x0310, 24, 0, 0, 0, 0x0310, 0, armpll_div_table, 0, 0),
	PLL(CLK_APMIXED_MAINPLL, "mainpll", 0x0228, 0x0234, 0xFF000001,
	    HAVE_RST_BAR, 22, 0x022C, 24, 0, 0, 0, 0x022C, 0,
	    CON0_MT8365_RST_BAR, 0),
	PLL(CLK_APMIXED_UNIVPLL, "univpll2", 0x0208, 0x0214, 0xFF000001,
	    HAVE_RST_BAR, 22, 0x020C, 24, 0, 0, 0, 0x020C, 0,
	    CON0_MT8365_RST_BAR, 0),
	PLL_B(CLK_APMIXED_MFGPLL, "mfgpll", 0x0218, 0x0224, 0x00000001, 0, 22,
	      0x021C, 24, 0, 0, 0, 0x021C, 0, mfgpll_div_table, 0, 0),
	PLL(CLK_APMIXED_MSDCPLL, "msdcpll", 0x0350, 0x035C, 0x00000001, 0, 22,
	    0x0354, 24, 0, 0, 0, 0x0354, 0, 0, 0),
	PLL(CLK_APMIXED_MMPLL, "mmpll", 0x0330, 0x033C, 0x00000001, 0, 22,
	    0x0334, 24, 0, 0, 0, 0x0334, 0, 0, 0),
	PLL(CLK_APMIXED_APLL1, "apll1", 0x031C, 0x032C, 0x00000001, 0, 32,
	    0x0320, 24, 0x0040, 0x000C, 0, 0x0324, 0, 0, 0x0320),
	PLL(CLK_APMIXED_APLL2, "apll2", 0x0360, 0x0370, 0x00000001, 0, 32,
	    0x0364, 24, 0x004C, 0x000C, 5, 0x0368, 0, 0, 0x0364),
	PLL(CLK_APMIXED_LVDSPLL, "lvdspll", 0x0374, 0x0380, 0x00000001, 0, 22,
	    0x0378, 24, 0, 0, 0, 0x0378, 0, 0, 0),
	PLL_B(CLK_APMIXED_DSPPLL, "dsppll", 0x0390, 0x039C, 0x00000001, 0, 22,
	      0x0394, 24, 0, 0, 0, 0x0394, 0, dsppll_div_table, 0, 0),
	PLL(CLK_APMIXED_APUPLL, "apupll", 0x03A0, 0x03AC, 0x00000001, 0, 22,
	    0x03A4, 24, 0, 0, 0, 0x03A4, 0, 0, 0),
};

static int clk_mt8365_apmixed_probe(struct platform_device *pdev)
{
	void __iomem *base;
	struct clk_hw_onecell_data *clk_data;
	struct device_node *node = pdev->dev.of_node;
	struct device *dev = &pdev->dev;
	struct clk_hw *hw;
	int ret;

	base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(base))
		return PTR_ERR(base);

	clk_data = mtk_devm_alloc_clk_data(dev, CLK_APMIXED_NR_CLK);
	if (!clk_data)
		return -ENOMEM;

	hw = devm_clk_hw_register_gate(dev, "univ_en", "univpll2", 0,
				       base + 0x204, 0, 0, NULL);
	if (IS_ERR(hw))
		return PTR_ERR(hw);
	clk_data->hws[CLK_APMIXED_UNIV_EN] = hw;

	hw = devm_clk_hw_register_gate(dev, "usb20_en", "univ_en", 0,
				       base + 0x204, 1, 0, NULL);
	if (IS_ERR(hw))
		return PTR_ERR(hw);
	clk_data->hws[CLK_APMIXED_USB20_EN] = hw;

	ret = mtk_clk_register_plls(node, plls, ARRAY_SIZE(plls), clk_data);
	if (ret)
		return ret;

	ret = of_clk_add_hw_provider(node, of_clk_hw_onecell_get, clk_data);
	if (ret)
		goto unregister_plls;

	return 0;

unregister_plls:
	mtk_clk_unregister_plls(plls, ARRAY_SIZE(plls), clk_data);

	return ret;
}

static int
clk_mt8365_register_mtk_simple_gates(struct device *dev, void __iomem *base,
				     struct clk_hw_onecell_data *clk_data,
				     const struct mtk_simple_gate *gates,
				     unsigned int num_gates)
{
	unsigned int i;

	for (i = 0; i != num_gates; ++i) {
		const struct mtk_simple_gate *gate = &gates[i];
		struct clk_hw *hw;

		hw = devm_clk_hw_register_gate(dev, gate->name, gate->parent, 0,
					       base + gate->reg, gate->shift,
					       gate->gate_flags, NULL);
		if (IS_ERR(hw))
			return PTR_ERR(hw);

		clk_data->hws[gate->id] = hw;
	}

	return 0;
}

static int clk_mt8365_top_probe(struct platform_device *pdev)
{
	void __iomem *base;
	struct clk_hw_onecell_data *clk_data;
	struct device_node *node = pdev->dev.of_node;
	struct device *dev = &pdev->dev;
	int ret;
	int i;

	base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(base))
		return PTR_ERR(base);

	clk_data = mtk_alloc_clk_data(CLK_TOP_NR_CLK);
	if (!clk_data)
		return -ENOMEM;

	ret = mtk_clk_register_fixed_clks(top_fixed_clks,
					  ARRAY_SIZE(top_fixed_clks), clk_data);
	if (ret)
		goto free_clk_data;

	ret = mtk_clk_register_factors(top_divs, ARRAY_SIZE(top_divs),
				       clk_data);
	if (ret)
		goto unregister_fixed_clks;

	ret = mtk_clk_register_muxes(&pdev->dev, top_muxes,
				     ARRAY_SIZE(top_muxes), node,
				     &mt8365_clk_lock, clk_data);
	if (ret)
		goto unregister_factors;

	ret = mtk_clk_register_composites(&pdev->dev, top_misc_mux_gates,
					  ARRAY_SIZE(top_misc_mux_gates), base,
					  &mt8365_clk_lock, clk_data);
	if (ret)
		goto unregister_muxes;

	for (i = 0; i != ARRAY_SIZE(top_misc_muxes); ++i) {
		struct mt8365_clk_audio_mux *mux = &top_misc_muxes[i];
		struct clk_hw *hw;

		hw = devm_clk_hw_register_mux(dev, mux->name, apll_i2s0_parents,
					      ARRAY_SIZE(apll_i2s0_parents),
					      CLK_SET_RATE_PARENT, base + 0x320,
					      mux->shift, 1, 0, NULL);
		if (IS_ERR(hw)) {
			ret = PTR_ERR(hw);
			goto unregister_composites;
		}

		clk_data->hws[mux->id] = hw;
	}

	ret = mtk_clk_register_dividers(top_adj_divs, ARRAY_SIZE(top_adj_divs),
					base, &mt8365_clk_lock, clk_data);
	if (ret)
		goto unregister_composites;

	ret = clk_mt8365_register_mtk_simple_gates(dev, base, clk_data,
						   top_clk_gates,
						   ARRAY_SIZE(top_clk_gates));
	if (ret)
		goto unregister_dividers;

	ret = of_clk_add_hw_provider(node, of_clk_hw_onecell_get, clk_data);
	if (ret)
		goto unregister_dividers;

	return 0;
unregister_dividers:
	mtk_clk_unregister_dividers(top_adj_divs, ARRAY_SIZE(top_adj_divs),
				    clk_data);
unregister_composites:
	mtk_clk_unregister_composites(top_misc_mux_gates,
				      ARRAY_SIZE(top_misc_mux_gates), clk_data);
unregister_muxes:
	mtk_clk_unregister_muxes(top_muxes, ARRAY_SIZE(top_muxes), clk_data);
unregister_factors:
	mtk_clk_unregister_factors(top_divs, ARRAY_SIZE(top_divs), clk_data);
unregister_fixed_clks:
	mtk_clk_unregister_fixed_clks(top_fixed_clks,
				      ARRAY_SIZE(top_fixed_clks), clk_data);
free_clk_data:
	mtk_free_clk_data(clk_data);

	return ret;
}

static int clk_mt8365_infra_probe(struct platform_device *pdev)
{
	struct clk_hw_onecell_data *clk_data;
	struct device_node *node = pdev->dev.of_node;
	int ret;

	clk_data = mtk_alloc_clk_data(CLK_IFR_NR_CLK);
	if (!clk_data)
		return -ENOMEM;

	ret = mtk_clk_register_gates(&pdev->dev, node, ifr_clks,
				     ARRAY_SIZE(ifr_clks), clk_data);
	if (ret)
		goto free_clk_data;

	ret = of_clk_add_hw_provider(node, of_clk_hw_onecell_get, clk_data);
	if (ret)
		goto unregister_gates;

	return 0;

unregister_gates:
	mtk_clk_unregister_gates(ifr_clks, ARRAY_SIZE(ifr_clks), clk_data);
free_clk_data:
	mtk_free_clk_data(clk_data);

	return ret;
}

static int clk_mt8365_peri_probe(struct platform_device *pdev)
{
	void __iomem *base;
	struct clk_hw_onecell_data *clk_data;
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;
	int ret;

	base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(base))
		return PTR_ERR(base);

	clk_data = mtk_devm_alloc_clk_data(dev, CLK_PERI_NR_CLK);
	if (!clk_data)
		return -ENOMEM;

	ret = clk_mt8365_register_mtk_simple_gates(dev, base, clk_data,
						   peri_clks,
						   ARRAY_SIZE(peri_clks));
	if (ret)
		return ret;

	ret = of_clk_add_hw_provider(node, of_clk_hw_onecell_get, clk_data);

	return ret;
}

static int clk_mt8365_mcu_probe(struct platform_device *pdev)
{
	struct clk_hw_onecell_data *clk_data;
	struct device_node *node = pdev->dev.of_node;
	void __iomem *base;
	int ret;

	base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(base))
		return PTR_ERR(base);

	clk_data = mtk_alloc_clk_data(CLK_MCU_NR_CLK);
	if (!clk_data)
		return -ENOMEM;

	ret = mtk_clk_register_composites(&pdev->dev, mcu_muxes,
					  ARRAY_SIZE(mcu_muxes), base,
					  &mt8365_clk_lock, clk_data);
	if (ret)
		goto free_clk_data;

	ret = of_clk_add_hw_provider(node, of_clk_hw_onecell_get, clk_data);
	if (ret)
		goto unregister_composites;

	return 0;

unregister_composites:
	mtk_clk_unregister_composites(mcu_muxes, ARRAY_SIZE(mcu_muxes),
				      clk_data);
free_clk_data:
	mtk_free_clk_data(clk_data);

	return ret;
}

static const struct of_device_id of_match_clk_mt8365[] = {
	{
		.compatible = "mediatek,mt8365-apmixedsys",
		.data = clk_mt8365_apmixed_probe,
	}, {
		.compatible = "mediatek,mt8365-topckgen",
		.data = clk_mt8365_top_probe,
	}, {
		.compatible = "mediatek,mt8365-infracfg",
		.data = clk_mt8365_infra_probe,
	}, {
		.compatible = "mediatek,mt8365-pericfg",
		.data = clk_mt8365_peri_probe,
	}, {
		.compatible = "mediatek,mt8365-mcucfg",
		.data = clk_mt8365_mcu_probe,
	}, {
		/* sentinel */
	}
};

static int clk_mt8365_probe(struct platform_device *pdev)
{
	int (*clk_probe)(struct platform_device *pdev);
	int ret;

	clk_probe = of_device_get_match_data(&pdev->dev);
	if (!clk_probe)
		return -EINVAL;

	ret = clk_probe(pdev);
	if (ret)
		dev_err(&pdev->dev,
			"%s: could not register clock provider: %d\n",
			pdev->name, ret);

	return ret;
}

static struct platform_driver clk_mt8365_drv = {
	.probe = clk_mt8365_probe,
	.driver = {
		.name = "clk-mt8365",
		.of_match_table = of_match_clk_mt8365,
	},
};

static int __init clk_mt8365_init(void)
{
	return platform_driver_register(&clk_mt8365_drv);
}
arch_initcall(clk_mt8365_init);
MODULE_LICENSE("GPL");
