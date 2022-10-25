/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef _HDMI_RX_T5M_H
#define _HDMI_RX_T5M_H

/* T5M PHY register */
#define T5M_HDMIRX20PLL_CTRL0			(0x000 << 2)
#define T5M_HDMIRX20PLL_CTRL1			(0x001 << 2)
#define T5M_HDMIRX20PHY_DCHA_AFE		(0x002 << 2)
	#define T5M_LEQ_HYPER_GAIN_CH0		_BIT(3)
	#define T5M_LEQ_HYPER_GAIN_CH1		_BIT(7)
	#define T5M_LEQ_HYPER_GAIN_CH2		_BIT(11)
#define T5M_HDMIRX20PHY_DCHA_DFE		(0x003 << 2)
	#define T5M_SLICER_OFSTCAL_START	_BIT(13)
#define T5M_HDMIRX20PHY_DCHD_CDR		(0x004 << 2)
	#define T5M_EHM_DBG_SEL			_BIT(31)
	#define T5M_OFSET_CAL_START		_BIT(27)
	#define T5M_CDR_LKDET_EN		_BIT(14)
	#define T5M_CDR_RSTB			_BIT(13)
	#define T5M_CDR_FR_EN			_BIT(6)
	#define T5M_MUX_CDR_DBG_SEL		_BIT(19)
	#define T5M_CDR_OS_RATE			MSK(2, 8)
	#define T5M_DFE_OFST_DBG_SEL		MSK(3, 28)
	#define T5M_ERROR_CNT			0X0
	#define T5M_SCAN_STATE			0X1
	#define T5M_POSITIVE_EYE_HEIGHT		0x2
	#define T5M_NEGATIVE_EYE_HEIGHT		0x3
	#define T5M_LEFT_EYE_WIDTH		0x4
	#define T5M_RIGHT_EYE_WIDTH		0x5
#define T5M_HDMIRX20PHY_DCHD_EQ			(0x005 << 2)
	#define T5M_BYP_TAP0_EN			_BIT(30)
	#define T5M_BYP_TAP_EN			_BIT(19)
	#define T5M_DFE_HOLD_EN			_BIT(18)
	#define T5M_DFE_RSTB			_BIT(17)
	#define T5M_DFE_EN			_BIT(16)
	#define T5M_EHM_SW_SCAN_EN		_BIT(15)
	#define T5M_EHM_HW_SCAN_EN		_BIT(14)
	#define T5M_EQ_RSTB			_BIT(13)
	#define T5M_EQ_EN			_BIT(12)
	#define T5M_EN_BYP_EQ			_BIT(5)
	#define T5M_BYP_EQ			MSK(5, 0)
	#define T5M_EQ_MODE			MSK(2, 8)
	#define T5M_STATUS_MUX_SEL		MSK(2, 22)
#define T5M_HDMIRX20PHY_DCHA_MISC1		(0x006 << 2)
	#define T5M_SQ_RSTN			_BIT(26)
	#define T5M_VCO_TMDS_EN			_BIT(20)
	#define T5M_RTERM_CNTL			MSK(4, 12)
#define T5M_HDMIRX20PHY_DCHA_MISC2		(0x007 << 2)
	#define T5M_TMDS_VALID_SEL		_BIT(10)
	#define T5M_PLL_CLK_SEL			_BIT(9)
#define T5M_HDMIRX20PHY_DCHD_STAT       (0x009 << 2)
#define T5M_HDMIRX_EARCTX_CNTL0         (0x040 << 2)
#define T5M_HDMIRX_EARCTX_CNTL1         (0x041 << 2)
#define T5M_HDMIRX_ARC_CNTL             (0x042 << 2)
#define T5M_HDMIRX_PHY_PROD_TEST0       (0x080 << 2)
#define T5M_HDMIRX_PHY_PROD_TEST1       (0x081 << 2)

#define T5M_RG_RX20PLL_0		0x100
#define T5M_RG_RX20PLL_1		0x104

/*--------------------------function declare------------------*/
/* T5m */
void aml_phy_init_t5m(void);
void aml_eq_eye_monitor_t5m(void);
void dump_reg_phy_t5m(void);
void dump_aml_phy_sts_t5m(void);
void aml_phy_short_bist_t5m(void);
bool aml_get_tmds_valid_t5m(void);
void aml_phy_power_off_t5m(void);
void aml_phy_switch_port_t5m(void);
void aml_phy_iq_skew_monitor_t5m(void);
void dump_vsi_reg_t5m(void);
unsigned int rx_sec_hdcp_cfg_t5m(void);
void rx_set_irq_t5m(bool en);
void rx_set_aud_output_t5m(u32 param);
void rx_sw_reset_t5m(int level);
void hdcp_init_t5m(void);
void aml_phy_get_trim_val_t5m(void);
/*function declare end*/

#endif /*_HDMI_RX_T5M_H*/

