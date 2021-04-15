/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 * Author: Yue Wang <yue.wang@amlogic.com>
 */

#ifndef __AMLOGIC_PCI_H__
#define __AMLOGIC_PCI_H__

/* PCIe Port Logic registers */
#define PLR_OFFSET			0x700
#define PCIE_PORT_LINK_CTRL_OFF	(PLR_OFFSET + 0x10)
#define PCIE_GEN2_CTRL_OFF	(PLR_OFFSET + 0x10c)

#define TYPE1_HDR_OFFSET	0X0
#define PCIE_BASE_ADDR0		(TYPE1_HDR_OFFSET + 0X10)
#define PCIE_BASE_ADDR1		(TYPE1_HDR_OFFSET + 0x14)

#define PCIE_CFG0			0x0

#define APP_LTSSM_ENABLE	(1<<7)
#define PCIE_CFG_STATUS12	0x30
#define PCIE_CFG_STATUS17	0x44

/* PCIe Port Logic registers */
#define PLR_OFFSET			0x700
#define PCIE_PHY_DEBUG_R1		(PLR_OFFSET + 0x2c)
#define PCIE_PHY_DEBUG_R1_LINK_UP	(0x1 << 4)
#define PCIE_PHY_DEBUG_R1_LINK_IN_TRAINING	(0x1 << 29)

#define    WAIT_LINKUP_TIMEOUT         2000

enum pcie_data_rate {
	PCIE_GEN1,
	PCIE_GEN2,
	PCIE_GEN3,
	PCIE_GEN4
};

#define PCIE_CAP_OFFSET	0x70
#define PCIE_DEV_CTRL_DEV_STUS	(PCIE_CAP_OFFSET + 0x08)

union phy_r0 {
	/** raw register data */
	uint32_t d32;
	/** register bits */
	struct {
		unsigned phy_test_powerdown:1;
		unsigned phy_ref_use_pad:1;
		unsigned pipe_port_sel:2;
		unsigned pcs_common_clocks:1;
		unsigned reserved:27;
	} b;
};

union phy_r1 {
	/** raw register data */
	uint32_t d32;
	/** register bits */
	struct {
		unsigned phy_tx1_term_offset:5;
		unsigned phy_tx0_term_offset:5;
		unsigned phy_rx1_eq:3;
		unsigned phy_rx0_eq:3;
		unsigned phy_los_level:5;
		unsigned phy_los_bias:3;
		unsigned phy_ref_clkdiv2:1;
		unsigned phy_mpll_multiplier:7;
	} b;
};

union phy_r2 {
	/** raw register data */
	uint32_t d32;
	/** register bits */
	struct {
		unsigned pcs_tx_deemph_gen2_6db:6;
		unsigned pcs_tx_deemph_gen2_3p5db:6;
		unsigned pcs_tx_deemph_gen1:6;
		unsigned phy_tx_vboost_lvl:3;
		unsigned reserved:11;
	} b;
};

union phy_r3 {
	/** raw register data */
	uint32_t d32;
	/** register bits */
	struct {
		unsigned pcs_tx_swing_low:7;
		unsigned pcs_tx_swing_full:7;
		unsigned reserved:18;
	} b;
};

union phy_r4 {
	/** raw register data */
	uint32_t d32;
	/** register bits */
	struct {
		unsigned phy_cr_write:1;
		unsigned phy_cr_read:1;
		unsigned phy_cr_data_in:16;
		unsigned phy_cr_cap_data:1;
		unsigned phy_cr_cap_addr:1;
		unsigned reserved:12;
	} b;
};

union phy_r5 {
	/** raw register data */
	uint32_t d32;
	/** register bits */
	struct {
		unsigned phy_cr_data_out:16;
		unsigned phy_cr_ack:1;
		unsigned phy_bs_out:1;
		unsigned reserved:14;
	} b;
};

union phy_r6 {
	/** raw register data */
	uint32_t d32;
	/** register bits */
	struct {
		unsigned phy_bs_update_dr:1;
		unsigned phy_bs_shift_dr:1;
		unsigned phy_bs_preload:1;
		unsigned phy_bs_invert:1;
		unsigned phy_bs_init:1;
		unsigned phy_bs_in:1;
		unsigned phy_bs_highz:1;
		unsigned phy_bs_extest_ac:1;
		unsigned phy_bs_extest:1;
		unsigned phy_bs_clk:1;
		unsigned phy_bs_clamp:1;
		unsigned phy_bs_capture_dr:1;
		unsigned phy_acjt_level:5;
		unsigned reserved:15;
	} b;
};

union pcie_phy_m31_r0 {
	/** raw register data */
	u32 d32;
	/** register bits */
	struct {
		unsigned PHY_SSCG_ON:1;
		unsigned PHY_CTLE_OFF:1;
		unsigned REFPAD_EXT_100M_EN:1;
		unsigned CLKPADEN:1;
		unsigned U3_SSRX_SEL:1;
		unsigned U3_SSTX_SEL:1;
		unsigned PCIE_XTLSEL:1;
		unsigned USB_XTLSEL:3;
		unsigned U3_HOST_PHY:1;
		unsigned USB_CLKSEL:1;
		unsigned PCIE_CLKSEL:2;
		unsigned OSCOUTEN:1;
		unsigned PLL_EN:1;
		unsigned LPM_ALIVE:1;
		unsigned PHY_SEL:2;
		unsigned p_datawidth:1;
		unsigned u_datawidth:1;
		unsigned p_rx_termination:1;
		unsigned FSLSSERIALMODE:1;
		unsigned TX_SE0:1;
		unsigned TX_DAT:1;
		unsigned TX_ENABLE_N:1;
		unsigned TXBITSTUFFENABLEH:1;
		unsigned TXBITSTUFFENABLE:1;
		unsigned UTMI_VCONTROLLOADM:1;
		unsigned RESERVED:3;
	} b;
};

union pcie_phy_m31_r1 {
	/** raw register data */
	u32 d32;
	/** register bits */
	struct {
		unsigned U31_INTERNALLOOPBACK:1;
		unsigned BIST_TXDEMPH_SEL:2;
		unsigned LS_EN:1;
		unsigned HS_BIST_MODE:1;
		unsigned LFPSRX_EN:1;
		unsigned VCONTROL:6;
		unsigned debug_sel:7;
		unsigned RESERVED:13;
	} b;
};

union pcie_phy_m31_r2 {
	/** raw register data */
	u32 d32;
	/** register bits */
	struct {
		unsigned CC_RP_0D9_EN:1;
		unsigned CC_RP_1D5_EN:2;
		unsigned CC_RP_3D0_EN:1;
		unsigned CC_HOST_EN:1;
		unsigned CC_EN:1;
		unsigned bist_bypass_sel:1;
		unsigned RESERVED:25;
	} b;
};

union pcie_phy_m31_r3 {
	/** raw register data */
	u32 d32;
	/** register bits */
	struct {
		unsigned phy_ref_clk_ctrl:2;
		unsigned clean_st:1;
		unsigned bist_bypass_cfg:29;
	} b;
};

union pcie_phy_m31_r4 {
	/** raw register data */
	u32 d32;
	/** register bits */
	struct {
		unsigned bypass_sel:16;
		unsigned bypass_cfg:16;
	} b;
};

union pcie_phy_m31_r5 {
	/** raw register data */
	u32 d32;
	/** register bits */
	struct {
		unsigned REFCLK_OFF_REQ_qre:1;
		unsigned PCLK_OFF_P1_REQ_pre:1;
		unsigned SQ_OFF_P1_REQ_pre:1;
		unsigned TX_OFF_P1_REQ_pre:1;
		unsigned PMCLK_EN_pre:1;
		unsigned REFCLK_OFF_ST:1;
		unsigned PCLK_OFF_P1_ST:1;
		unsigned SQ_OFF_P1_ST:1;
		unsigned TX_OFF_P1_ST:1;
		unsigned CAL_FB_VAL_L0_pre:1;
		unsigned pm_st:4;
		unsigned BIST_OK:1;
		unsigned BIST_OK_U2:1;
		unsigned pcie_rx_elecidle:1;
		unsigned linestate:2;
		unsigned usb_rx_elecidle:1;
		unsigned RESERVED:12;
	} b;
};

union pcie_phy_m31_r6 {
	/** raw register data */
	u32 d32;
	/** register bits */
	struct {
		unsigned CAL_PARAM_OUT_sync_low:32;
	} b;
};

union pcie_phy_m31_r7 {
	/** raw register data */
	u32 d32;
	/** register bits */
	struct {
		unsigned CAL_PARAM_OUT_sync_middle:32;
	} b;
};

union pcie_phy_m31_r8 {
	/** raw register data */
	u32 d32;
	/** register bits */
	struct {
		unsigned CAL_PARAM_OUT_sync_high:14;
		unsigned RESERVED:18;
	} b;
};

union pcie_phy_m31_r9 {
	/** raw register data */
	u32 d32;
	/** register bits */
	struct {
		unsigned CAL_FB_IN_L0_pre_low:32;
	} b;
};

union pcie_phy_m31_r10 {
	/** raw register data */
	u32 d32;
	/** register bits */
	struct {
		unsigned CAL_FB_IN_L0_pre_middle:32;
	} b;
};

union pcie_phy_m31_r11 {
	/** raw register data */
	u32 d32;
	/** register bits */
	struct {
		unsigned CAL_FB_IN_L0_pre_high:14;
		unsigned RESERVED:18;
	} b;
};

union pcie_phy_m31_r12 {
	/** raw register data */
	u32 d32;
	/** register bits */
	struct {
		unsigned bypass_cal_fb_low:32;
	} b;
};

union pcie_phy_m31_r13 {
	/** raw register data */
	u32 d32;
	/** register bits */
	struct {
		unsigned bypass_cal_fb_middle:32;
	} b;
};

union pcie_phy_m31_r14 {
	/** raw register data */
	u32 d32;
	/** register bits */
	struct {
		unsigned bypass_cal_fb_high:14;
		unsigned RESERVED:18;
	} b;
};

union pcie_phy_m31_r15 {
	/** raw register data */
	u32 d32;
	/** register bits */
	struct {
		unsigned m31phy_debug_out:32;
	} b;
};

struct pcie_phy_aml_regs {
	void __iomem    *pcie_phy_r[16];
};

struct pcie_phy {
	u32 power_state;
	u32 device_attch;
	u32 reset_state;
	void __iomem		*phy_base;	/* DT 1st resource */
	void __iomem		*reset_base;/* DT 3nd resource */
	u32 pcie_ctrl_sleep_shift;
	u32 pcie_hhi_mem_pd_mask;
	u32 pcie_ctrl_iso_shift;
	u32 pcie_hhi_mem_pd_shift;
	u32 phy_type;
};
#endif
