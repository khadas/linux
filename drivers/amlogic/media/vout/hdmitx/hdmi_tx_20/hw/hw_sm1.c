// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/printk.h>
#include <linux/amlogic/media/vout/hdmi_tx/hdmi_tx_module.h>
#include "common.h"
#include "mach_reg.h"

void set_phy_by_mode_sm1(unsigned int mode)
{
	switch (mode) {
	case HDMI_PHYPARA_6G: /* 5.94Gbps */
	case HDMI_PHYPARA_4p5G: /* 4.5Gbps*/
	case HDMI_PHYPARA_3p7G: /* 3.7Gbps */
		hd_write_reg(P_HHI_HDMI_PHY_CNTL5, 0x0000080b);
		hd_write_reg(P_HHI_HDMI_PHY_CNTL0, 0x37eb65c4);
		hd_write_reg(P_HHI_HDMI_PHY_CNTL3, 0x2ab0ff3b);
		break;
	case HDMI_PHYPARA_3G: /* 2.97Gbps */
		hd_write_reg(P_HHI_HDMI_PHY_CNTL5, 0x00000003);
		hd_write_reg(P_HHI_HDMI_PHY_CNTL0, 0x33eb42a2);
		hd_write_reg(P_HHI_HDMI_PHY_CNTL3, 0x2ab0ff3b);
		break;
	case HDMI_PHYPARA_270M: /* SD format, 480p/576p, 270Mbps */
	case HDMI_PHYPARA_DEF: /* less than 2.97G */
	default:
		hd_write_reg(P_HHI_HDMI_PHY_CNTL5, 0x00000003);
		hd_write_reg(P_HHI_HDMI_PHY_CNTL0, 0x33eb4252);
		hd_write_reg(P_HHI_HDMI_PHY_CNTL3, 0x2ab0ff3b);
		break;
	}
}

