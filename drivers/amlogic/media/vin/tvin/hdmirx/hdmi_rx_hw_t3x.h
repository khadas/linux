/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef _HDMI_RX_T3X_H
#define _HDMI_RX_T3X_H

/* T3X PHY register */
#define T3X_HDMIRX20PLL_CTRL0           (0x000 << 2)
#define T3X_HDMIRX20PLL_CTRL1           (0x001 << 2)
#define T3X_HDMIRX20PHY_DCHA_AFE        (0x002 << 2)
#define T3X_HDMIRX20PHY_DCHA_DFE        (0x003 << 2)
	#define T3X_SLICER_OFSTCAL_START _BIT(13)
#define T3X_HDMIRX20PHY_DCHD_CDR        (0x004 << 2)
	#define T3X_MUX_EYE_EN          _BIT(31)
	#define T3X_OFSET_CAL_START     _BIT(27)
	#define T3X_CDR_LKDET_EN        _BIT(14)
	#define T3X_CDR_RSTB            _BIT(13)
	#define T3X_CDR_FR_EN           _BIT(6)
	#define T3X_MUX_CDR_DBG_SEL     _BIT(19)
	#define T3X_CDR_OS_RATE         MSK(2, 8)
	#define T3X_MUX_DFE_OFST_EYE    MSK(3, 28)
#define T3X_HDMIRX20PHY_DCHD_EQ         (0x005 << 2)
	#define T3X_BYP_TAP0_EN         _BIT(30)
	#define T3X_BYP_TAP_EN          _BIT(19)
	#define T3X_DFE_HOLD_EN         _BIT(18)
	#define T3X_DFE_RSTB            _BIT(17)
	#define T3X_DFE_EN              _BIT(16)
	#define T3X_EQ_RSTB             _BIT(13)
	#define T3X_EQ_EN               _BIT(12)
	#define T3X_EN_BYP_EQ           _BIT(5)
	#define T3X_BYP_EQ              MSK(5, 0)
	#define T3X_EQ_MODE             MSK(2, 8)
	#define T3X_MUX_BLOCK_SEL       MSK(2, 22)
#define T3X_HDMIRX20PHY_DCHA_MISC1      (0x006 << 2)
	#define T3X_SQ_RSTN             _BIT(26)
	#define T3X_VCO_TMDS_EN         _BIT(20)
	#define T3X_RTERM_CNTL          MSK(4, 12)
#define T3X_HDMIRX20PHY_DCHA_MISC2      (0x007 << 2)
	#define T3X_PLL_CLK_SEL         _BIT(9)
#define T3X_HDMIRX20PHY_DCHD_STAT       (0x009 << 2)
#define T3X_HDMIRX_EARCTX_CNTL0         (0x040 << 2)
#define T3X_HDMIRX_EARCTX_CNTL1         (0x041 << 2)
#define T3X_HDMIRX_ARC_CNTL             (0x042 << 2)
#define T3X_HDMIRX_PHY_PROD_TEST0       (0x080 << 2)
#define T3X_HDMIRX_PHY_PROD_TEST1       (0x081 << 2)

#define T3X_RG_RX20PLL_0		0x100
#define T3X_RG_RX20PLL_1		0x104

/*--------------------------function declare------------------*/
/* T3X */
void aml_phy_init_t3x(void);
void aml_eq_eye_monitor_t3x(void);
void dump_reg_phy_t3x(void);
void dump_aml_phy_sts_t3x(void);
void aml_phy_short_bist_t3x(void);
bool aml_get_tmds_valid_t3x(void);
void aml_phy_power_off_t3x(void);
void aml_phy_switch_port_t3x(void);
void aml_phy_iq_skew_monitor_t3x(void);
void dump_vsi_reg_t3x(void);
unsigned int rx_sec_hdcp_cfg_t3x(void);
void rx_set_irq_t3x(bool en);
void rx_set_aud_output_t3x(u32 param);
void rx_sw_reset_t3x(int level);
void hdcp_init_t3x(void);
void aml_phy_get_trim_val_t3x(void);
/*function declare end*/

#endif /*_HDMI_RX_T3X_H*/

