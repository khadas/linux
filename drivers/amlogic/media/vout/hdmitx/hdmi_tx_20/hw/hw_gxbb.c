// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/printk.h>
#include "common.h"
#include "mach_reg.h"

/*
 * NAME		PAD		PINMUX		GPIO
 * HPD		GPIOH_0		reg1[26]	GPIO1[16]
 * SCL		GPIOH_2		reg1[24]	GPIO1[18[
 * SDA		GPIOH_1		reg7[25]	GPIO1[17]
 */

int hdmitx_hpd_hw_op_gxbb(enum hpd_op cmd)
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

int read_hpd_gpio_gxbb(void)
{
	return 0;
}

int hdmitx_ddc_hw_op_gxbb(enum ddc_op cmd)
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
