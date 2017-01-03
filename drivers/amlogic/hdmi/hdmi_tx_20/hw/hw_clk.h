
#ifndef __HW_ENC_CLK_CONFIG_H__
#define __HW_ENC_CLK_CONFIG_H__

#include <linux/amlogic/hdmi_tx/hdmi_common.h>
#include <linux/amlogic/hdmi_tx/hdmi_tx_module.h>
#include <linux/amlogic/vout/vinfo.h>

#define VID_PLL_DIV_1      0
#define VID_PLL_DIV_2      1
#define VID_PLL_DIV_3      2
#define VID_PLL_DIV_3p5    3
#define VID_PLL_DIV_3p75   4
#define VID_PLL_DIV_4      5
#define VID_PLL_DIV_5      6
#define VID_PLL_DIV_6      7
#define VID_PLL_DIV_6p25   8
#define VID_PLL_DIV_7      9
#define VID_PLL_DIV_7p5    10
#define VID_PLL_DIV_12     11
#define VID_PLL_DIV_14     12
#define VID_PLL_DIV_15     13
#define VID_PLL_DIV_2p5    14

#define GROUP_MAX	8
struct hw_enc_clk_val_group {
	enum hdmi_vic group[GROUP_MAX];
	unsigned hpll_clk_out; /* Unit: kHz */
	unsigned od1;
	unsigned od2; /* HDMI_CLK_TODIG */
	unsigned od3;
	unsigned vid_pll_div;
	unsigned vid_clk_div;
	unsigned hdmi_tx_pixel_div;
	unsigned encp_div;
	unsigned enci_div;
};

void hdmitx_set_clk(struct hdmitx_dev *hdev);

#endif

