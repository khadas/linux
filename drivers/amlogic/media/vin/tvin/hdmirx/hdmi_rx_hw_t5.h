/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef _HDMI_RX_T5_H
#define _HDMI_RX_T5_H

/* T5 HIU apll register */
#define T5_HHI_RX_APLL_CNTL0                    (0x0 << 2)/*0x0*/
#define T5_HHI_RX_APLL_CNTL1                    (0x1 << 2)/*0x4*/
#define T5_HHI_RX_APLL_CNTL2                    (0x2 << 2)/*0x8*/
#define T5_HHI_RX_APLL_CNTL3                    (0x3 << 2)/*0xc*/
#define T5_HHI_RX_APLL_CNTL4                    (0x4 << 2)/*0x10*/

/* T5 HIU PHY register */
#define T5_HHI_RX_PHY_MISC_CNTL0                (0x5 << 2)/*0x14*/
	#define T5_SQO_GATE                     _BIT(30)
	#define T5_PLL_SRC_SEL                  _BIT(29)
#define T5_HHI_RX_PHY_MISC_CNTL1                (0x6 << 2)/*0x18*/
#define T5_HHI_RX_PHY_MISC_CNTL2                (0x7 << 2)/*0x1c*/
#define T5_HHI_RX_PHY_MISC_CNTL3                (0x8 << 2)/*0x20*/
#define T5_HHI_RX_PHY_DCHA_CNTL0                (0x9 << 2)/*0x24*/
	#define T5_HYPER_GAIN                   MSK(3, 12)
#define T5_HHI_RX_PHY_DCHA_CNTL1                (0xa << 2)/*0x28*/
	#define T5_DFE_OFSETCAL_START           _BIT(27)
#define T5_HHI_RX_PHY_DCHA_CNTL2                (0xb << 2)/*0x2c*/
	#define T5_EYE_MONITOR_EN1              _BIT(27)/*The same as dchd_eye[19]*/
	#define T5_AFE_EN                       _BIT(17)
#define T5_HHI_RX_PHY_DCHA_CNTL3                (0xc << 2)/*0x30*/
#define T5_HHI_RX_PHY_DCHD_CNTL0                (0xd << 2)/*0x34*/
	#define T5_CDR_MODE                     _BIT(31)
	#define T5_CDR_FR_EN                    _BIT(30)
	#define T5_EQ_EN                        _BIT(29)
	#define T5_CDR_LKDET_EN                 _BIT(28)
	#define T5_CDR_RST                      _BIT(25)
	#define T5_CDR_PH_DIV                   MSK(3, 0)
	#define T5_CDR_EQ_RSTB                  MSK(2, 24)
	#define T5_EQ_ADP_MODE                  MSK(2, 10)
#define T5_HHI_RX_PHY_DCHD_CNTL1                (0xe << 2)/*0x38*/
	#define T5_OFST_CAL_START               _BIT(31)
	#define T5_IQ_OFST_SIGN                 _BIT(27)
	#define T5_EQ_BYP_VAL2                  MSK(5, 17)
	#define T5_EQ_BYP_VAL1                  MSK(5, 12)
#define T5_HHI_RX_PHY_DCHD_CNTL2                (0xf << 2)/*0x3c*/
	#define T5_DFE_HOLD                     _BIT(31)
	#define	T5_DFE_EN                       _BIT(27)
	#define T5_DFE_RST                      _BIT(26)
	#define T5_EYE_STATUS                   MSK(3, 28)
	#define T5_DFE_DBG_STL                  MSK(3, 28)
	#define T5_ERROR_CNT                    0x0
	#define T5_SCAN_STATE                   0x1
	#define T5_POSITIVE_EYE_HEIGHT          0x2
	#define T5_NEGATIVE_EYE_HEIGHT          0x3
	#define T5_LEFT_EYE_WIDTH               0x4
	#define T5_RIGHT_EYE_WIDTH              0x5
#define T5_HHI_RX_PHY_DCHD_CNTL3               (0x10 << 2)/*0x40*/
	#define T5_DBG_STS_SEL                 MSK(2, 30)
#define T5_HHI_RX_PHY_DCHD_CNTL4               (0x11 << 2)/*0x44*/
	#define T5_EYE_MONITOR_EN              _BIT(19)
	#define T5_EYE_STATUS_EN               _BIT(18)
	#define T5_EYE_EN_HW_SCAN              _BIT(16)
#define T5_HHI_RX_PHY_MISC_STAT               (0x12 << 2)/*0x48*/
#define T5_HHI_RX_PHY_DCHD_STAT               (0x13 << 2)/*0x4c*/

/*--------------------------function declare------------------*/
/* T5 */
void dump_reg_phy_t5(void);
void aml_phy_init_t5(void);
void dump_aml_phy_sts_t5(void);
void aml_eq_eye_monitor_t5(void);
void aml_phy_offset_cal_t5(void);
void aml_phy_short_bist_t5(void);
void aml_phy_iq_skew_monitor_t5(void);
bool aml_get_tmds_valid_t5(void);
void aml_phy_power_off_t5(void);
void aml_phy_switch_port_t5(void);
void aml_phy_get_trim_val_t5(void);

/*function declare end*/

#endif /*_HDMI_RX_T5_H*/

