// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include "hdmi_tx_debug_reg.h"

const static unsigned int regs_hdmitx[] = {
	HDMITX_TOP_SW_RESET,
	HDMITX_TOP_CLK_CNTL,
	HDMITX_TOP_INTR_MASKN,
	HDMITX_TOP_INTR_STAT,
	HDMITX_TOP_INTR_STAT_CLR,
	HDMITX_TOP_BIST_CNTL,
	HDMITX_TOP_TMDS_CLK_PTTN_01,
	HDMITX_TOP_TMDS_CLK_PTTN_23,
	HDMITX_TOP_TMDS_CLK_PTTN_CNTL,
	HDMITX_TOP_I2C_BUSY_CNT_MAX,
	HDMITX_TOP_I2C_BUSY_CNT_STAT,
	HDMITX_TOP_HDCP22_BSOD,
	HDMITX_TOP_DDC_CNTL,
	HDMITX_TOP_DISABLE_NULL,
	HDMITX_TOP_HDCP14_UNENCRYPT,
	HDMITX_TOP_MISC_CNTL,
	HDMITX_DWC_TX_INVID0,
	HDMITX_DWC_VP_PR_CD,
	HDMITX_DWC_VP_REMAP,
	HDMITX_DWC_VP_CONF,
	HDMITX_DWC_FC_INVIDCONF,
	HDMITX_DWC_FC_INHACTV0,
	HDMITX_DWC_FC_INHACTV1,
	HDMITX_DWC_FC_INHBLANK0,
	HDMITX_DWC_FC_INHBLANK1,
	HDMITX_DWC_FC_INVACTV0,
	HDMITX_DWC_FC_INVACTV1,
	HDMITX_DWC_FC_INVBLANK,
	HDMITX_DWC_FC_HSYNCINDELAY0,
	HDMITX_DWC_FC_HSYNCINDELAY1,
	HDMITX_DWC_FC_HSYNCINWIDTH0,
	HDMITX_DWC_FC_HSYNCINWIDTH1,
	HDMITX_DWC_FC_VSYNCINDELAY,
	HDMITX_DWC_FC_VSYNCINWIDTH,
	HDMITX_DWC_FC_GCP,
	HDMITX_DWC_FC_AVICONF0,
	HDMITX_DWC_FC_AVIVID,
	HDMITX_DWC_FC_VSDIEEEID0,
	HDMITX_DWC_FC_VSDSIZE,
	HDMITX_DWC_FC_VSDIEEEID1,
	HDMITX_DWC_FC_VSDIEEEID2,
	HDMITX_DWC_FC_AUDSCONF,
	HDMITX_DWC_FC_AUDSSTAT,
	HDMITX_DWC_FC_AUDSV,
	HDMITX_DWC_FC_AUDSU,
	HDMITX_DWC_FC_DATAUTO0,
	HDMITX_DWC_FC_DATAUTO1,
	HDMITX_DWC_FC_DATAUTO2,
	HDMITX_DWC_FC_DATMAN,
	HDMITX_DWC_FC_DATAUTO3,
	HDMITX_DWC_FC_PACKET_TX_EN,
	HDMITX_DWC_FC_DRM_HB01,
	HDMITX_DWC_FC_DRM_HB02,
	HDMITX_DWC_FC_DRM_PB00,
	HDMITX_DWC_FC_DRM_PB01,
	HDMITX_DWC_FC_DRM_PB02,
	HDMITX_DWC_FC_DRM_PB03,
	HDMITX_DWC_FC_DRM_PB04,
	HDMITX_DWC_FC_DRM_PB05,
	HDMITX_DWC_AUD_CONF0,
	HDMITX_DWC_AUD_CONF1,
	HDMITX_DWC_AUD_CONF2,
	HDMITX_DWC_AUD_N1,
	HDMITX_DWC_AUD_N2,
	HDMITX_DWC_AUD_N3,
	HDMITX_DWC_AUD_CTS1,
	HDMITX_DWC_AUD_CTS2,
	HDMITX_DWC_AUD_CTS3,
	HDMITX_DWC_AUD_INPUTCLKFS,
	HDMITX_DWC_AUD_SPDIF0,
	HDMITX_DWC_AUD_SPDIF1,
	HDMITX_DWC_MC_CLKDIS,
	HDMITX_DWC_MC_CLKDIS_SC2,
	HDMITX_DWC_MC_SWRSTZREQ,
	HDMITX_DWC_MC_LOCKONCLOCK,
	HDMITX_DWC_CSC_CFG,
	HDMITX_DWC_CSC_SCALE,
	HDMITX_DWC_CSC_COEF_A1_MSB,
	HDMITX_DWC_CSC_COEF_A1_LSB,
	HDMITX_DWC_A_HDCPCFG0,
	HDMITX_DWC_A_HDCPCFG1,
	HDMITX_DWC_A_HDCPOBS0,
	HDMITX_DWC_A_HDCPOBS1,
	HDMITX_DWC_A_HDCPOBS2,
	HDMITX_DWC_A_HDCPOBS3,
	HDMITX_DWC_A_APIINTCLR,
	HDMITX_DWC_A_APIINTSTAT,
	HDMITX_DWC_A_APIINTMSK,
	HDMITX_DWC_A_VIDPOLCFG,
	HDMITX_DWC_HDCPREG_BKSV0,
	HDMITX_DWC_HDCPREG_BKSV1,
	HDMITX_DWC_HDCPREG_BKSV2,
	HDMITX_DWC_HDCPREG_BKSV3,
	HDMITX_DWC_HDCPREG_BKSV4,
	HDMITX_DWC_HDCP22REG_CTRL,
	HDMITX_DWC_HDCP22REG_CTRL1,
	HDMITX_DWC_HDCP22REG_STS,
	HDMITX_DWC_HDCP22REG_STAT,
	REGS_END,
};

const static unsigned int regs_clkctrl_sc2[] = {
	P_CLKCTRL_SYS_CLK_EN0_REG2,
	P_CLKCTRL_VID_CLK_CTRL,
	P_CLKCTRL_VID_CLK_CTRL2,
	P_CLKCTRL_VID_CLK_DIV,
	P_CLKCTRL_HDMI_CLK_CTRL,
	P_CLKCTRL_VID_PLL_CLK_DIV,
	P_CLKCTRL_HDCP22_CLK_CTRL,
	REGS_END,
};

const static unsigned int regs_clkctrl_hhi[] = {
	P_HHI_VIID_CLK_DIV,
	P_HHI_VIID_CLK_CNTL,
	P_HHI_VIID_DIVIDER_CNTL,
	P_HHI_VID_CLK_DIV,
	P_HHI_VID_CLK_CNTL,
	P_HHI_VID_CLK_CNTL2,
	P_HHI_VID_DIVIDER_CNTL,
	P_HHI_VID_PLL_CLK_DIV,
	P_HHI_VPU_CLK_CNTL,
	P_HHI_OTHER_PLL_CNTL,
	P_HHI_HDMI_CLK_CNTL,
	P_HHI_HDMI_CLK_CNTL,
	P_HHI_HDCP22_CLK_CNTL,
	REGS_END,
};

const static unsigned int regs_anactrl_sc2[] = {
	P_ANACTRL_HDMIPLL_CTRL0,
	P_ANACTRL_HDMIPLL_CTRL1,
	P_ANACTRL_HDMIPLL_CTRL2,
	P_ANACTRL_HDMIPLL_CTRL3,
	P_ANACTRL_HDMIPLL_CTRL4,
	P_ANACTRL_HDMIPLL_CTRL5,
	P_ANACTRL_HDMIPLL_CTRL6,
	P_ANACTRL_HDMIPLL_STS,
	P_ANACTRL_HDMIPLL_VLOCK,
	P_ANACTRL_HDMIPHY_CTRL0,
	P_ANACTRL_HDMIPHY_CTRL1,
	P_ANACTRL_HDMIPHY_CTRL2,
	P_ANACTRL_HDMIPHY_CTRL3,
	P_ANACTRL_HDMIPHY_CTRL4,
	P_ANACTRL_HDMIPHY_CTRL5,
	P_ANACTRL_HDMIPHY_STS,
	REGS_END,
};

const static unsigned int regs_anactrl_hhi[] = {
	P_HHI_HDMI_PLL_CNTL,
	P_HHI_HDMI_PLL_CNTL2,
	P_HHI_HDMI_PLL_CNTL3,
	P_HHI_HDMI_PLL_CNTL4,
	P_HHI_HDMI_PLL_CNTL5,
	P_HHI_HDMI_PLL_CNTL6,
	P_HHI_HDMI_PLL_CNTL_I,
	P_HHI_HDMI_PLL_CNTL7,
	P_HHI_HDMI_PHY_CNTL0,
	P_HHI_HDMI_PHY_CNTL1,
	P_HHI_HDMI_PHY_CNTL2,
	P_HHI_HDMI_PHY_CNTL3,
	P_HHI_HDMI_PHY_CNTL4,
	P_HHI_HDMI_PHY_CNTL5,
	P_HHI_HDMI_PHY_STATUS,
	P_HHI_VID_LOCK_CLK_CNTL,
	REGS_END,
};

const static unsigned int regs_enc[] = {
	P_ENCP_VIDEO_MODE_ADV,
	P_VENC_VIDEO_TST_EN,
	P_ENCP_VIDEO_EN,
	P_ENCP_VIDEO_MAX_PXCNT,
	P_ENCP_VIDEO_MAX_LNCNT,
	P_ENCP_DVI_HSO_BEGIN,
	P_ENCP_DVI_HSO_END,
	P_ENCP_DVI_VSO_BLINE_EVN,
	P_ENCP_DVI_VSO_BLINE_ODD,
	P_ENCP_DVI_VSO_ELINE_EVN,
	P_ENCP_DVI_VSO_ELINE_ODD,
	P_ENCP_DVI_VSO_BEGIN_EVN,
	P_ENCP_DVI_VSO_BEGIN_ODD,
	P_ENCP_DVI_VSO_END_EVN,
	P_ENCP_DVI_VSO_END_ODD,
	P_ENCP_DE_H_BEGIN,
	P_ENCP_DE_H_END,
	P_ENCP_DE_V_BEGIN_EVEN,
	P_ENCP_DE_V_END_EVEN,
	P_ENCP_DE_V_BEGIN_ODD,
	P_ENCP_DE_V_END_ODD,
	P_VPP_POSTBLEND_H_SIZE,
	P_VPU_HDMI_SETTING,
	P_VPU_HDMI_DATA_OVR,
	P_VPU_HDMI_FMT_CTRL,
	P_VPU_HDMI_DITH_CNTL,
	REGS_END,
};

const static unsigned int read_hdmitx_regs(const unsigned int add)
{
	return hdmitx_rd_reg(add);
}

const static unsigned int get_hdmitx_addr(const unsigned int add)
{
	return add;
}

const struct hdmitx_dbg_reg_s dbg_regs_hdmitx = {
	.rd_reg_func = read_hdmitx_regs,
	.get_reg_paddr = get_hdmitx_addr,
	.name = "hdmitx regs",
	.reg = regs_hdmitx,
};

const static unsigned int read_non_hdmitx_reg(const unsigned int add)
{
	return hd_read_reg(add);
}

const static unsigned int get_non_hdmitx_addr(const unsigned int add)
{
	return TO_PHY_ADDR(add);
}

const struct hdmitx_dbg_reg_s dbg_regs_clkctrl_hhi = {
	.rd_reg_func = read_non_hdmitx_reg,
	.get_reg_paddr = get_non_hdmitx_addr,
	.name = "clkctrl regs",
	.reg = regs_clkctrl_hhi,
};

const struct hdmitx_dbg_reg_s dbg_regs_clkctrl_sc2 = {
	.rd_reg_func = read_non_hdmitx_reg,
	.get_reg_paddr = get_non_hdmitx_addr,
	.name = "clkctrl regs",
	.reg = regs_clkctrl_sc2,
};

const struct hdmitx_dbg_reg_s dbg_regs_anactrl_hhi = {
	.rd_reg_func = read_non_hdmitx_reg,
	.get_reg_paddr = get_non_hdmitx_addr,
	.name = "anactrl regs",
	.reg = regs_anactrl_hhi,
};

const struct hdmitx_dbg_reg_s dbg_regs_anactrl_sc2 = {
	.rd_reg_func = read_non_hdmitx_reg,
	.get_reg_paddr = get_non_hdmitx_addr,
	.name = "anactrl regs",
	.reg = regs_anactrl_sc2,
};

const struct hdmitx_dbg_reg_s dbg_regs_enc = {
	.rd_reg_func = read_non_hdmitx_reg,
	.get_reg_paddr = get_non_hdmitx_addr,
	.name = "enc regs",
	.reg = regs_enc,
};

const struct hdmitx_dbg_reg_s *array_hhi[] = {
	&dbg_regs_hdmitx,
	&dbg_regs_clkctrl_hhi,
	&dbg_regs_anactrl_hhi,
	&dbg_regs_enc,
	NULL,
};

const struct hdmitx_dbg_reg_s *array_sc2[] = {
	&dbg_regs_hdmitx,
	&dbg_regs_clkctrl_sc2,
	&dbg_regs_anactrl_sc2,
	&dbg_regs_enc,
	NULL,
};

const struct hdmitx_dbg_reg_s **hdmitx_get_dbg_regs(enum amhdmitx_chip_e type)
{
	if (type == MESON_CPU_ID_SC2)
		return array_sc2;
	else
		return array_hhi;
}
