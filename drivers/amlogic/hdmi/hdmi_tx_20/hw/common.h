
#ifndef __HDMITX_HW_COMMON_H__
#define __HDMITX_HW_COMMON_H__

#include <linux/amlogic/hdmi_tx/hdmi_tx_ddc.h>
#include <linux/amlogic/hdmi_tx/hdmi_common.h>
#include "mach_reg.h"
#include "hdmi_tx_reg.h"
/*
 * HDMITX HPD HW related operations
 */
enum hpd_op {
	HPD_INIT_DISABLE_PULLUP,
	HPD_INIT_SET_FILTER,
	HPD_IS_HPD_MUXED,
	HPD_MUX_HPD,
	HPD_UNMUX_HPD,
	HPD_READ_HPD_GPIO,
};

int hdmitx_hpd_hw_op_gxbb(enum hpd_op cmd);
int read_hpd_gpio_gxbb(void);
int hdmitx_ddc_hw_op_gxbb(enum ddc_op cmd);

int hdmitx_hpd_hw_op_gxtvbb(enum hpd_op cmd);
int read_hpd_gpio_gxtvbb(void);
int hdmitx_ddc_hw_op_gxtvbb(enum ddc_op cmd);

int hdmitx_hpd_hw_op_gxl(enum hpd_op cmd);
int read_hpd_gpio_gxl(void);
int hdmitx_ddc_hw_op_gxl(enum ddc_op cmd);
void set_gxl_hpll_clk_out(unsigned frac_rate, unsigned clk);
void set_hpll_sspll_gxl(enum hdmi_vic vic);
void set_hpll_od1_gxl(unsigned div);
void set_hpll_od2_gxl(unsigned div);
void set_hpll_od3_gxl(unsigned div);

#endif
