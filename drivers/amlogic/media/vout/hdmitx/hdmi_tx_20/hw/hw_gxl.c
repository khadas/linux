// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/printk.h>
#include "common.h"
#include "mach_reg.h"

/*
 * From GXL chips, registers names changes.
 * Added new HHI_HDMI_PLL_CNTL1 but addressed as 0xc9.
 * It's different with HHI_HDMI_PLL_CNTL2 0xc9 in previouse chips.
 */
#ifdef P_HHI_HDMI_PLL_CNTL2
#undef P_HHI_HDMI_PLL_CNTL2
#endif
#ifdef P_HHI_HDMI_PLL_CNTL3
#undef P_HHI_HDMI_PLL_CNTL3
#endif
#ifdef P_HHI_HDMI_PLL_CNTL4
#undef P_HHI_HDMI_PLL_CNTL4
#endif
#ifdef P_HHI_HDMI_PLL_CNTL5
#undef P_HHI_HDMI_PLL_CNTL5
#endif
#ifdef P_HHI_HDMI_PLL_CNTL6
#undef P_HHI_HDMI_PLL_CNTL6
#endif

#define P_HHI_HDMI_PLL_CNTL1 HHI_REG_ADDR(0xc9)
#define P_HHI_HDMI_PLL_CNTL2 HHI_REG_ADDR(0xca)
#define P_HHI_HDMI_PLL_CNTL3 HHI_REG_ADDR(0xcb)
#define P_HHI_HDMI_PLL_CNTL4 HHI_REG_ADDR(0xcc)
#define P_HHI_HDMI_PLL_CNTL5 HHI_REG_ADDR(0xcd)

/*
 * NAME		PAD		PINMUX		GPIO
 * HPD		GPIOH_0		reg6[31]	GPIO1[20]
 * SCL		GPIOH_2		reg6[29]	GPIO1[22[
 * SDA		GPIOH_1		reg6[30]	GPIO1[21]
 */

int hdmitx_hpd_hw_op_gxl(enum hpd_op cmd)
{
	int ret = 0;

	switch (cmd) {
	case HPD_INIT_DISABLE_PULLUP:
		break;
	case HPD_INIT_SET_FILTER:
		hdmitx_wr_reg(HDMITX_TOP_HPD_FILTER,
			      ((0xa << 12) | (0xa0 << 0)));
		break;
	case HPD_IS_HPD_MUXED:
		break;
	case HPD_MUX_HPD:
		break;
	case HPD_UNMUX_HPD:
		break;
	case HPD_READ_HPD_GPIO:
		break;
	default:
		pr_info("error hpd cmd %d\n", cmd);
		break;
	}
	return ret;
}

int read_hpd_gpio_gxl(void)
{
	return 1;
}

int hdmitx_ddc_hw_op_gxl(enum ddc_op cmd)
{
	int ret = 0;

	switch (cmd) {
	case DDC_INIT_DISABLE_PULL_UP_DN:
		break;
	case DDC_MUX_DDC:
		break;
	case DDC_UNMUX_DDC:
		break;
	default:
		pr_info("error ddc cmd %d\n", cmd);
	}
	return ret;
}

void set_gxl_hpll_clk_out(unsigned int frac_rate, unsigned int clk)
{
	/* TODO */
}

void set_hpll_sspll_gxl(enum hdmi_vic vic)
{
	switch (vic) {
	case HDMI_1920x1080p60_16x9:
	case HDMI_1920x1080p50_16x9:
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL2, 0x68b48c4, 0, 30);
		break;
	case HDMI_1280x720p60_16x9:
	case HDMI_1280x720p50_16x9:
	case HDMI_1920x1080i60_16x9:
	case HDMI_1920x1080i50_16x9:
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL2, 0x64348c4, 0, 30);
		break;
	default:
		break;
	}
}

void set_hpll_od1_gxl(unsigned int div)
{
	switch (div) {
	case 1:
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL2, 0, 21, 2);
		break;
	case 2:
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL2, 1, 21, 2);
		break;
	case 4:
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL2, 2, 21, 2);
		break;
	default:
		pr_info("Err %s[%d]\n", __func__, __LINE__);
		break;
	}
}

void set_hpll_od2_gxl(unsigned int div)
{
	switch (div) {
	case 1:
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL2, 0, 23, 2);
		break;
	case 2:
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL2, 1, 23, 2);
		break;
	case 4:
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL2, 2, 23, 2);
		break;
	default:
		pr_info("Err %s[%d]\n", __func__, __LINE__);
		break;
	}
}

void set_hpll_od3_gxl(unsigned int div)
{
	switch (div) {
	case 1:
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL2, 0, 19, 2);
		break;
	case 2:
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL2, 1, 19, 2);
		break;
	case 4:
		hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL2, 2, 19, 2);
		break;
	default:
		pr_info("Err %s[%d]\n", __func__, __LINE__);
		break;
	}
}

void set_phy_by_mode_gxl(unsigned int mode)
{
	switch (mode) {
	case HDMI_PHYPARA_6G: /* 5.94Gbps, 3.7125Gbsp */
	case HDMI_PHYPARA_4p5G:
	case HDMI_PHYPARA_3p7G:
		hd_write_reg(P_HHI_HDMI_PHY_CNTL0, 0x333d3282);
		hd_write_reg(P_HHI_HDMI_PHY_CNTL3, 0x2136315b);
		break;
	case HDMI_PHYPARA_3G: /* 2.97Gbps */
		hd_write_reg(P_HHI_HDMI_PHY_CNTL0, 0x33303382);
		hd_write_reg(P_HHI_HDMI_PHY_CNTL3, 0x2036315b);
		break;
	case HDMI_PHYPARA_DEF: /* 1.485Gbps */
		hd_write_reg(P_HHI_HDMI_PHY_CNTL0, 0x33303042);
		hd_write_reg(P_HHI_HDMI_PHY_CNTL3, 0x2016315b);
		break;
	case HDMI_PHYPARA_270M:
	default: /* 742.5Mbps, and below */
		hd_write_reg(P_HHI_HDMI_PHY_CNTL0, 0x33604132);
		hd_write_reg(P_HHI_HDMI_PHY_CNTL3, 0x0016315b);
		break;
	}
}
