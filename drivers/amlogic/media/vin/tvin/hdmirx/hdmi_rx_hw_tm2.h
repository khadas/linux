/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef _HDMI_RX_TM2_H
#define _HDMI_RX_TM2_H

/* tm2 HIU apll register */
#define TM2_HHI_HDMIRX_APLL_CNTL0               (0xd2 << 2)
#define TM2_HHI_HDMIRX_APLL_CNTL1               (0xd3 << 2)
#define TM2_HHI_HDMIRX_APLL_CNTL2               (0xd4 << 2)
#define TM2_HHI_HDMIRX_APLL_CNTL3               (0xd5 << 2)
#define TM2_HHI_HDMIRX_APLL_CNTL4               (0xd6 << 2)

/* tm2 HIU PHY register */
#define TM2_HHI_HDMIRX_PHY_MISC_CNTL0           (0xd7 << 2)
#define MISCI_COMMON_RST                        _BIT(10)
#define TM2_HHI_HDMIRX_PHY_MISC_CNTL1           (0xd8 << 2)
#define TM2_HHI_HDMIRX_PHY_MISC_CNTL2           (0xe0 << 2)
#define TM2_HHI_HDMIRX_PHY_MISC_CNTL3           (0xe1 << 2)
#define TM2_HHI_HDMIRX_PHY_DCHA_CNTL0           (0xe2 << 2)
#define TM2_HHI_HDMIRX_PHY_DCHA_CNTL1           (0xe3 << 2)
	#define TM2_DFE_OFSETCAL_START          _BIT(27)
	#define TM2_DFE_TAP1_EN                 _BIT(17)
	#define TM2_DEF_SUM_RS_TRIM             MSK(3, 12)
	/*[4:5] in trim,[6:7] im trim*/
	#define TM2_DFE_SUM_TRIM                MSK(4, 4)
#define TM2_HHI_HDMIRX_PHY_DCHA_CNTL2           (0xe4 << 2)
	#define TM2_DFE_VADC_EN                 _BIT(21)
#define TM2_HHI_HDMIRX_PHY_DCHA_CNTL3           (0xc5 << 2)/*for revB*/
#define TM2_HHI_HDMIRX_PHY_DCHD_CNTL0           (0xe5 << 2)
	#define TM2_CDR_LKDET_EN                _BIT(28)
	/*bit'24:eq rst bit'25:cdr rst*/
	#define TM2_CDR_EQ_RSTB                 MSK(2, 24)
	/*0:manual 1:c only 2:r only 3:both rc*/
	#define TM2_EQ_ADP_MODE                 MSK(2, 10)
	#define TM2_EQ_ADP_STG                  MSK(2, 8)
#define TM2_HHI_HDMIRX_PHY_DCHD_CNTL1          (0xe6 << 2)
	/*tm2b*/
	#define TM2_OFST_CAL_START              _BIT(31)
	#define TM2_EQ_BYP_VAL                  MSK(5, 12)
#define TM2_HHI_HDMIRX_PHY_DCHD_CNTL2           (0xe7 << 2)
	#define TM2_DFE_DBG_STL                 MSK(3, 28)
	#define	TM2_DFE_EN                      _BIT(27)
	#define TM2_DFE_RSTB                    _BIT(26)
	#define TM2_TAP1_BYP_EN                 _BIT(19)
#define TM2_HHI_HDMIRX_PHY_DCHD_CNTL3          (0xc6 << 2)/*for revB*/
	#define TM2_DBG_STS_SEL                MSK(2, 30)
#define TM2_HHI_HDMIRX_PHY_DCHD_STAT           (0xef << 2)

/*--------------------------function declare------------------*/
/* TM2 */
void aml_phy_short_bist_tm2(void);
void aml_phy_init_tm2(void);
void dump_aml_phy_sts_tm2(void);
bool aml_get_tmds_valid_tm2(void);
void aml_phy_power_off_tm2(void);
void aml_phy_switch_port_tm2(void);

/*function declare end*/

#endif /*_HDMI_RX_TM2_H*/

