/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef _HDMI_RX_T7_H
#define _HDMI_RX_T7_H

/* T7 HIU apll register */
#define T7_HHI_RX_APLL_CNTL0                    (0x0 << 2)/*0x0*/
#define T7_HHI_RX_APLL_CNTL1                    (0x1 << 2)/*0x4*/
#define T7_HHI_RX_APLL_CNTL2                    (0x2 << 2)/*0x8*/
#define T7_HHI_RX_APLL_CNTL3                    (0x3 << 2)/*0xc*/
#define T7_HHI_RX_APLL_CNTL4                    (0x4 << 2)/*0x10*/

/* T7 HIU PHY register */
#define T7_HHI_RX_PHY_MISC_CNTL0                (0x5 << 2)/*0x14*/
	#define T7_SQO_GATE                     _BIT(30)
	#define T7_PLL_SRC_SEL                  _BIT(29)
#define T7_HHI_RX_PHY_MISC_CNTL1                (0x6 << 2)/*0x18*/
#define T7_HHI_RX_PHY_MISC_CNTL2                (0x7 << 2)/*0x1c*/
#define T7_HHI_RX_PHY_MISC_CNTL3                (0x8 << 2)/*0x20*/
#define T7_HHI_RX_PHY_DCHA_CNTL0                (0x9 << 2)/*0x24*/
	#define T7_LEQ_BUF_GAIN                 MSK(3, 20)
	#define T7_LEQ_POLE                     MSK(3, 16)
	#define T7_HYPER_GAIN                   MSK(3, 12)
#define T7_HHI_RX_PHY_DCHA_CNTL1                (0xa << 2)/*0x28*/
	#define T7_DFE_OFSETCAL_START           _BIT(27)

#define T7_HHI_RX_PHY_DCHA_CNTL2                (0xb << 2)/*0x2c*/
	#define T7_EYE_MONITOR_EN1              _BIT(27)/*The same as dchd_eye[19]*/
	#define T7_AFE_EN                       _BIT(17)
#define T7_HHI_RX_PHY_DCHA_CNTL3                (0xc << 2)/*0x30*/
#define T7_HHI_RX_PHY_DCHD_CNTL0                (0xd << 2)/*0x34*/
	#define T7_CDR_MODE                     _BIT(31)
	#define T7_CDR_FR_EN                    _BIT(30)
	#define T7_EQ_EN                        _BIT(29)
	#define T7_CDR_LKDET_EN                 _BIT(28)
	#define	T7_DFE_EN                       _BIT(27)
	#define T7_CDR_RST                      _BIT(25)
	#define T7_EQ_ADP_MODE                  MSK(2, 10)
	#define T7_CDR_PH_DIV                   MSK(3, 0)
	#define T7_CDR_EQ_RSTB                  MSK(2, 24)
#define T7_HHI_RX_PHY_DCHD_CNTL1                (0xe << 2)/*0x38*/
	#define T7_OFST_CAL_START               _BIT(31)
	#define T7_IQ_OFST_SIGN                 _BIT(27)
	#define IQ_OFST_VAL                     MSK(5, 22)
	#define T7_EQ_BYP_VAL2                  MSK(5, 17)
	#define T7_EQ_BYP_VAL1                  MSK(5, 12)
#define T7_HHI_RX_PHY_DCHD_CNTL2                (0xf << 2)/*0x3c*/
	#define T7_DFE_HOLD                     _BIT(31)
	#define T7_DFE_RST                      _BIT(26)
	#define T7_EYE_STATUS                   MSK(3, 28)
	#define T7_DFE_DBG_STL                  MSK(3, 28)
	#define T7_ERROR_CNT                    0x0
	#define T7_SCAN_STATE                   0x1
	#define T7_POSITIVE_EYE_HEIGHT          0x2
	#define T7_NEGATIVE_EYE_HEIGHT          0x3
	#define T7_LEFT_EYE_WIDTH               0x4
	#define T7_RIGHT_EYE_WIDTH              0x5
#define T7_HHI_RX_PHY_DCHD_CNTL3                (0x10 << 2)/*0x40*/
	#define T7_DBG_STS_SEL                  MSK(2, 30)
#define T7_HHI_RX_PHY_DCHD_CNTL4                (0x11 << 2)/*0x44*/
	#define T7_EYE_MONITOR_EN               _BIT(19)
	#define T7_EYE_STATUS_EN                _BIT(18)
	#define T7_EYE_EN_HW_SCAN               _BIT(16)
#define T7_HHI_RX_PHY_MISC_STAT                 (0x12 << 2)/*0x48*/
#define T7_HHI_RX_PHY_DCHD_STAT                 (0x13 << 2)/*0x4c*/

/*--------------------------function declare------------------*/
/* T7 */
u8 rx_get_stream_manage_info(void);
void rpt_update_hdcp1x(struct hdcp_topo_s *topo);
void rpt_update_hdcp2x(struct hdcp_topo_s *topo);
void hdcp_init_t7(void);
void dump_reg_phy_t7(void);
void aml_phy_init_t7(void);
void dump_aml_phy_sts_t7(void);
void aml_eq_eye_monitor_t7(void);
void aml_phy_offset_cal_t7(void);
void aml_phy_short_bist_t7(void);
void aml_phy_iq_skew_monitor_t7(void);
bool aml_get_tmds_valid_t7(void);
void aml_phy_power_off_t7(void);
void aml_phy_switch_port_t7(void);
unsigned int rx_sec_hdcp_cfg_t7(void);
void dump_vsi_reg_t7(void);
void rx_set_irq_t7(bool en);
void rx_set_aud_output_t7(u32 param);
void rx_sw_reset_t7(int level);
void aml_phy_get_trim_val_t7(void);

/*function declare end*/

#endif /*_HDMI_RX_T7_H*/
