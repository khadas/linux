#include <linux/printk.h>
#include "common.h"
#include "mach_reg.h"
#include "mach_reg_gxtvbb.h"

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
		hd_set_reg_bits(P_PAD_PULL_UP_REG1, 0, 25, 1);
		break;
	case HPD_INIT_SET_FILTER:
		hdmitx_wr_reg(HDMITX_TOP_HPD_FILTER,
			((0xa << 12) | (0xa0 << 0)));
		break;
	case HPD_IS_HPD_MUXED:
		ret = !!(hd_read_reg(P_PERIPHS_PIN_MUX_6) & (1 << 31));
		break;
	case HPD_MUX_HPD:
		hd_set_reg_bits(P_PREG_PAD_GPIO1_EN_N, 1, 20, 1);
		hd_set_reg_bits(P_PERIPHS_PIN_MUX_6, 1, 31, 1);
		break;
	case HPD_UNMUX_HPD:
		hd_set_reg_bits(P_PERIPHS_PIN_MUX_6, 0, 31, 1);
		hd_set_reg_bits(P_PREG_PAD_GPIO1_EN_N, 1, 20, 1);
		break;
	case HPD_READ_HPD_GPIO:
		ret = !!(hd_read_reg(P_PREG_PAD_GPIO1_I) & (1 << 20));
		break;
	default:
		pr_info("error hpd cmd %d\n", cmd);
		break;
	}
	return ret;
}

int read_hpd_gpio_gxl(void)
{
	return !!(hd_read_reg(P_PREG_PAD_GPIO1_I) & (1 << 20));
}

int hdmitx_ddc_hw_op_gxl(enum ddc_op cmd)
{
	int ret = 0;

	switch (cmd) {
	case DDC_INIT_DISABLE_PULL_UP_DN:
		hd_set_reg_bits(P_PAD_PULL_UP_EN_REG1, 0, 23, 2);
		hd_set_reg_bits(P_PAD_PULL_UP_REG1, 0, 23, 2);
		break;
	case DDC_MUX_DDC:
		hd_set_reg_bits(P_PREG_PAD_GPIO1_EN_N, 3, 21, 2);
		hd_set_reg_bits(P_PERIPHS_PIN_MUX_6, 3, 29, 2);
		break;
	case DDC_UNMUX_DDC:
		hd_set_reg_bits(P_PREG_PAD_GPIO1_EN_N, 3, 21, 2);
		hd_set_reg_bits(P_PERIPHS_PIN_MUX_6, 0, 29, 2);
		break;
	default:
		pr_info("error ddc cmd %d\n", cmd);
	}
	return ret;
}
