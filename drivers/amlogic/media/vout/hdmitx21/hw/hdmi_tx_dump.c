// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/amlogic/media/vout/hdmi_tx21/hdmi_tx_module.h>

#include "common.h"

static void dump32(u32 start, u32 end)
{
	u32 value = 0;

	for (; start <= end; start += 4) {
		value = hd21_read_reg(start);
		if (value)
			pr_info("[0x%08x] 0x%08x\n", TO21_PHY_ADDR(start), value);
	}
}

static void dumptop(u32 start, u32 end)
{
	u32 value = 0;

	start = (((start) & 0xffff) | TOP_OFFSET_MASK);
	end = (((end) & 0xffff) | TOP_OFFSET_MASK);
	for (; start <= end; start += 4) {
		value = hdmitx21_rd_reg(start);
		if (value)
			pr_info("[0x%08x] 0x%02x\n", start, value);
	}
}

static void dumpcor(u32 start, u32 end)
{
	u32 value = 0;

	for (; start <= end; start++) {
		value = hdmitx21_rd_reg(start);
		if (value)
			pr_info("[0x%08x] 0x%02x\n", start, value);
	}
}

void dump_hdmitx_reg(void)
{
	// ((0x0000 << 2) + 0xfe008000) ~ ((0x00f3 << 2) + 0xfe008000)
	dump32(ANACTRL_SYS0PLL_CTRL0, ANACTRL_DIF_PHY_STS);
	// ((0x0001 << 2) + 0xfe000000) ~ ((0x0128 << 2) + 0xfe000000)
	dump32(CLKCTRL_OSCIN_CTRL, CLKCTRL_EFUSE_A73_CFG2);
	// ((0x0000 << 2) + 0xfe00c000) ~ ((0x027f << 2) + 0xfe00c000)
	dump32(PWRCTRL_PWR_ACK0, PWRCTRL_A73TOP_FSM_JUMP);
	// ((0x0000 << 2) + 0xfe09c000) ~ ((0x0027 << 2) + 0xfe09c000)
	// dump32(AOCPU_CPU_CTRL0, AOCPU_IRQ_SEL15);
	// ((0x1100 << 2) + 0xff000000) ~ ((0x16b6 << 2) + 0xff000000)
	// dump32(RDMA_AHB_START_ADDR_MAN, LCD2_GAMMA_ADDR_PORT0);
	// ((0x1b00 << 2) + 0xff000000) ~ ((0x1bea << 2) + 0xff000000)
	dump32(ENCI_VIDEO_MODE, ENCP_VRR_CTRL1);
	// ((0x1c00 << 2) + 0xff000000) ~((0x1cfc << 2) + 0xff000000)
	dump32(ENCI_DVI_HSO_BEGIN, VPU_VENCL_DITH_LUT_12);
	// ((0x2158 << 2) + 0xff000000) ~ ((0x2250 << 2) + 0xff000000)
	dump32(ENCP1_VFIFO2VD_CTL, ENCP1_VFIFO2VD_CTL2);
	// ((0x2358 << 2) + 0xff000000) ~ ((0x2450 << 2) + 0xff000000)
	dump32(ENCP2_VFIFO2VD_CTL, ENCP2_VFIFO2VD_CTL2);
	// ((0x2451 << 2) + 0xff000000) ~ ((0x24fc << 2) + 0xff000000)
	dump32(VENC2_DVI_SETTING_MORE, VPU2_VENCL_DITH_LUT_12);
	// ((0x2701 << 2) + 0xff000000) ~ ((0x24fc << 2) + 0xff000000)
	dump32(VPU_CRC_CTRL, VPUCTRL_REG_ADDR(0x27fd));
	// 0xfe300000 ~ 0xfe300000 + (0x041 << 2)
	dumptop(HDMITX_TOP_SW_RESET, HDMITX_TOP_SECURE_DATA);
	// 0x00000000 - 0x00000018
	dumpcor(INTR3_IVCTX, AON_CYP_CTL_IVCTX);
	// 0x00000100 - 0x00000130
	dumpcor(VND_IDL_IVCTX, TOP_INTR_IVCTX);
	// 0x00000200 - 0x000002d5
	dumpcor(DEBUG_MODE_EN_IVCTX, DROP_GEN_TYPE_5_IVCTX);
	// 0x00000300 - 0x0000031a
	dumpcor(TX_ZONE_CTL0_IVCTX, FIFO_10TO20_CTRL_IVCTX);
	// 0x00000330 - 0x00000334
	// dumpcor(MHLHDMITXTOP_INTR_IVCTX, EMSC_ADCTC_LD_SEL_IVCTX);
	// 0x00000400 - 0x000004a0
	dumpcor(BIST_RST_IVCTX, CR_BLACK_HIGH_IVCTX);
	// 0x00000607 - 0x00000777
	dumpcor(TPI_MISC_IVCTX, HT_LTP_ST_PHY_IVCTX);
	// 0x00000780 - 0x000007f7
	// dumpcor(HT_TOP_CTL_PHY_ANA_IVCTX, HT_LTP_ST_PHY_ANA_IVCTX);
	// 0x00000800 - 0x00000876
	dumpcor(CP2TX_CTRL_0_IVCTX, CP2TX_GP1_IVCTX);
	// 0x000008a0 - 0x000008d0
	dumpcor(HDCP2X_DEBUG_CTRL0_IVCTX, HDCP2X_DEBUG_STAT16_IVCTX);
	// 0x00000900 - 0x00000934
	dumpcor(SCRCTL_IVCTX, RSVD1_HDMI2_IVCTX);
	// 0x00000940 - 0x00000985
	// dumpcor(H21TXSB_CTRL_IVCTX, H21TX_SB_TOP_INS_DISP_CTRL_1_IVCTX);
	// 0x00000a00 - 0x00000a6e
	dumpcor(RSVD0_AIP_IVCTX, AUDIO_CLK_DIV_IVCTX);
	// 0x00000b00 - 0x00000be1
	dumpcor(VP_FEATURES_IVCTX, VP_EMBD_SYNC_ENC_CONFIG_IVCTX);
	// 0x00000d00 - 0x00000d3c
	dumpcor(VP_CMS_CSC0_FEATURES_IVCTX, VP_CMS_CSC0_MULTI_CSC_OUT_RCR_OFFSET_IVCTX);
	//0x00000d80 - 0x00000ddc
	dumpcor(VP_CMS_CSC1_FEATURES_IVCTX, VP_CMS_CSC1_PIXCAP_OUT_CR_IVCTX);
	// 0x00000f00 - 0x00000f26
	dumpcor(D_HDR_GEN_CTL_IVCTX, D_HDR_SPARE_9_IVCTX);
	// 0x00000f80 - 0x00000fa9
	dumpcor(DSC_PKT_GEN_CTL_IVCTX, DSC_PKT_SPARE_9_IVCTX);
	// 0x00001000 - 0x0000103c
	// dumpcor(ZONE_CTRL_0_IVCTX, ZONE_CTRL_10_IVCTX);
}
