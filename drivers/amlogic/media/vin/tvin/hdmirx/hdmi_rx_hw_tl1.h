/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef _HDMI_RX_TL1_H
#define _HDMI_RX_TL1_H

/* tl1 HIU apll register */
#define TL1_HHI_HDMIRX_APLL_CNTL0               (0xd2 << 2)
#define TL1_HHI_HDMIRX_APLL_CNTL1               (0xd3 << 2)
#define TL1_HHI_HDMIRX_APLL_CNTL2               (0xd4 << 2)
#define TL1_HHI_HDMIRX_APLL_CNTL3               (0xd5 << 2)
#define TL1_HHI_HDMIRX_APLL_CNTL4               (0xd6 << 2)

/* tl1 HIU PHY register */
#define TL1_HHI_HDMIRX_PHY_MISC_CNTL0           (0xd7 << 2)
#define TL1_HHI_HDMIRX_PHY_MISC_CNTL1           (0xd8 << 2)
#define TL1_MISCI_MANUAL_MODE                    _BIT(22)
#define TL1_HHI_HDMIRX_PHY_MISC_CNTL2           (0xe0 << 2)
#define TL1_HHI_HDMIRX_PHY_MISC_CNTL3           (0xe1 << 2)
#define TL1_HHI_HDMIRX_PHY_DCHA_CNTL0           (0xe2 << 2)
#define TL1_HHI_HDMIRX_PHY_DCHA_CNTL1           (0xe3 << 2)
	/*[4:5] in trim,[6:7] im trim*/
#define TL1_HHI_HDMIRX_PHY_DCHA_CNTL2           (0xe4 << 2)
#define TL1_HHI_HDMIRX_PHY_DCHA_CNTL3           (0xc5 << 2)/*for revB*/
#define TL1_HHI_HDMIRX_PHY_DCHD_CNTL0           (0xe5 << 2)
	/*bit'24:eq rst bit'25:cdr rst*/
	/*0:manual 1:c only 2:r only 3:both rc*/
#define TL1_HHI_HDMIRX_PHY_DCHD_CNTL1           (0xe6 << 2)
#define TL1_HHI_HDMIRX_PHY_DCHD_CNTL2           (0xe7 << 2)
#define TL1_HHI_HDMIRX_PHY_DCHD_CNTL3           (0xc6 << 2)/*for revB*/
#define TL1_HHI_HDMIRX_PHY_ARC_CNTL             (0xe8 << 2)
#define TL1_HHI_HDMIRX_EARCTX_CNTL0             (0x69 << 2)
#define TL1_HHI_HDMIRX_EARCTX_CNTL1             (0x6a << 2)
#define TL1_HHI_HDMIRX_PHY_MISC_STAT            (0xee << 2)
#define TL1_HHI_HDMIRX_PHY_DCHD_STAT            (0xef << 2)

/*--------------------------function declare------------------*/
/* Tl1 */
void aml_phy_init_tl1(void);
void dump_aml_phy_sts_tl1(void);
void aml_phy_short_bist_tl1(void);
bool aml_get_tmds_valid_tl1(void);
void aml_phy_power_off_tl1(void);
void aml_phy_switch_port_tl1(void);

/*function declare end*/

#endif /*_HDMI_RX_TL1_H*/

