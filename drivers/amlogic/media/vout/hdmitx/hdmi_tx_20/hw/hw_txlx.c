// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/printk.h>
#include <linux/pinctrl/devinfo.h>
#include <linux/amlogic/media/vout/hdmi_tx/hdmi_tx_module.h>
#include "common.h"
#include "reg_ops.h"
#include "txlx_reg.h"

unsigned int hdmitx_get_format_txlx(void)
{
	return hd_read_reg(P_ISA_DEBUG_REG0);
}

void hdmitx_set_format_txlx(unsigned int val)
{
	return hd_write_reg(P_ISA_DEBUG_REG0, val);
}

/*
 * hdmitx apb reset
 * P_RESET0_REGISTER bit19 : hdmitx capb
 * P_RESET2_REGISTER bit15 : hdmi system reset
 * P_RESET2_REGISTER bit2 : hdmi tx
 */
void hdmitx_sys_reset_txlx(void)
{
	hd_set_reg_bits(P_RESET0_REGISTER, 1, 19, 1);
	hd_set_reg_bits(P_RESET2_REGISTER, 1, 15, 1);
	hd_set_reg_bits(P_RESET2_REGISTER, 1,  2, 1);
}

/*
 * NAME		PAD		PINMUX		GPIO
 * HPD		GPIOH_1		reg0[23]	GPIO1[21]
 * SCL		GPIOH_2		reg0[22]	GPIO1[22[
 * SDA		GPIOH_3		reg0[21]	GPIO1[23]
 */

int hdmitx_hpd_hw_op_txlx(enum hpd_op cmd)
{
	int ret = 0;

	struct hdmitx_dev *hdev = get_hdmitx_device();

	if (!(hdev->pdev)) {
		pr_info("exit for null device of hdmitx!\n");
		return -ENODEV;
	}

	if (!(hdev->pdev->pins)) {
		pr_info("exit for null pins of hdmitx device!\n");
		return -ENODEV;
	}

	if (!(hdev->pdev->pins->p)) {
		pr_info("exit for null pinctrl of hdmitx device pins!\n");
		return -ENODEV;
	}

	switch (cmd) {
	case HPD_INIT_DISABLE_PULLUP:
		break;
	case HPD_INIT_SET_FILTER:
		hdmitx_wr_reg(HDMITX_TOP_HPD_FILTER,
			      ((0xa << 12) | (0xa0 << 0)));
		break;
	case HPD_IS_HPD_MUXED:
		ret = 1;
		break;
	case HPD_MUX_HPD:
		pinctrl_select_state(hdev->pdev->pins->p,
				     hdev->pinctrl_default);
		break;
	case HPD_UNMUX_HPD:
		pinctrl_select_state(hdev->pdev->pins->p, hdev->pinctrl_i2c);
		break;
	case HPD_READ_HPD_GPIO:
		ret = hdmitx_rd_reg(HDMITX_DWC_PHY_STAT0) & (1 << 1);
		break;
	default:
		pr_err("error hpd cmd %d\n", cmd);
		break;
	}
	return ret;
}

int read_hpd_gpio_txlx(void)
{
	return hdmitx_rd_reg(HDMITX_DWC_PHY_STAT0) & (1 << 1);
}

int hdmitx_ddc_hw_op_txlx(enum ddc_op cmd)
{
	int ret = 0;
	struct hdmitx_dev *hdev = get_hdmitx_device();

	if (!(hdev->pdev)) {
		pr_info("exit for null device of hdmitx!\n");
		return -ENODEV;
	}

	if (!(hdev->pdev->pins)) {
		pr_info("exit for null pins of hdmitx device!\n");
		return -ENODEV;
	}

	if (!(hdev->pdev->pins->p)) {
		pr_info("exit for null pinctrl of hdmitx device pins!\n");
		return -ENODEV;
	}

	switch (cmd) {
	case DDC_INIT_DISABLE_PULL_UP_DN:
		break;
	case DDC_MUX_DDC:
		pinctrl_select_state(hdev->pdev->pins->p,
				     hdev->pinctrl_default);
		break;
	case DDC_UNMUX_DDC:
		pinctrl_select_state(hdev->pdev->pins->p, hdev->pinctrl_i2c);
		break;
	default:
		pr_err("error ddc cmd %d\n", cmd);
	}
	return ret;
}
