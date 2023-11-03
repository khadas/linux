// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 Rockchip Electronics Co. Ltd.
 *
 * Author: Guochun Huang <hero.huang@rock-chips.com>
 */

#include "rk628.h"
#include "rk628_cru.h"
#include "rk628_config.h"
#include "panel.h"

int rk628_rgb_parse(struct rk628 *rk628, struct device_node *rgb_np)
{
	return rk628_panel_info_get(rk628, rgb_np);
}

void rk628_rgb_decoder_enable(struct rk628 *rk628)
{
		/* config sw_input_mode RGB */
	rk628_i2c_update_bits(rk628, GRF_SYSTEM_CON0, SW_INPUT_MODE_MASK,
			      SW_INPUT_MODE(INPUT_MODE_RGB));

	/* pinctrl for vop pin */
	rk628_i2c_write(rk628, GRF_GPIO2AB_SEL_CON, 0xffffffff);
	rk628_i2c_write(rk628, GRF_GPIO2C_SEL_CON, 0xffff5555);
	rk628_i2c_write(rk628, GRF_GPIO3AB_SEL_CON, 0x10b010b);

	/* rk628: modify IO drive strength for RGB */
	rk628_i2c_write(rk628, GRF_GPIO2A_D0_CON, 0xffff1111);
	rk628_i2c_write(rk628, GRF_GPIO2A_D1_CON, 0xffff1111);
	rk628_i2c_write(rk628, GRF_GPIO2B_D0_CON, 0xffff1111);
	rk628_i2c_write(rk628, GRF_GPIO2B_D1_CON, 0xffff1111);
	rk628_i2c_write(rk628, GRF_GPIO2C_D0_CON, 0xffff1111);
	rk628_i2c_write(rk628, GRF_GPIO2C_D1_CON, 0xffff1111);
	rk628_i2c_write(rk628, GRF_GPIO3A_D0_CON, 0xffff1011);
	rk628_i2c_write(rk628, GRF_GPIO3B_D_CON, 0x10001);
}

void rk628_rgb_encoder_enable(struct rk628 *rk628)
{
	rk628_i2c_update_bits(rk628, GRF_SYSTEM_CON0,
			      SW_BT_DATA_OEN_MASK | SW_OUTPUT_RGB_MODE_MASK,
			      SW_OUTPUT_RGB_MODE(OUTPUT_MODE_RGB >> 3));
	rk628_i2c_update_bits(rk628, GRF_POST_PROC_CON, SW_DCLK_OUT_INV_EN,
			      SW_DCLK_OUT_INV_EN);

	/* pinctrl for vop pin */
	rk628_i2c_write(rk628, GRF_GPIO2AB_SEL_CON, 0xffffffff);
	rk628_i2c_write(rk628, GRF_GPIO2C_SEL_CON, 0xffff5555);
	rk628_i2c_write(rk628, GRF_GPIO3AB_SEL_CON, 0x10b010b);

	/* rk628: modify IO drive strength for RGB */
	rk628_i2c_write(rk628, GRF_GPIO2A_D0_CON, 0xffff7777);
	rk628_i2c_write(rk628, GRF_GPIO2A_D1_CON, 0xffff7777);
	rk628_i2c_write(rk628, GRF_GPIO2B_D0_CON, 0xffff7777);
	rk628_i2c_write(rk628, GRF_GPIO2B_D1_CON, 0xffff7777);
	rk628_i2c_write(rk628, GRF_GPIO2C_D0_CON, 0xffff7777);
	rk628_i2c_write(rk628, GRF_GPIO2C_D1_CON, 0xffff7777);
	rk628_i2c_write(rk628, GRF_GPIO3A_D0_CON, 0xffff7077);
	rk628_i2c_write(rk628, GRF_GPIO3B_D_CON, 0xf0007);
}

void rk628_rgb_encoder_disable(struct rk628 *rk628)
{
}


void rk628_rgb_rx_enable(struct rk628 *rk628)
{

	rk628_rgb_decoder_enable(rk628);

}

void rk628_rgb_tx_enable(struct rk628 *rk628)
{
	rk628_rgb_encoder_enable(rk628);

	rk628_panel_prepare(rk628);
	rk628_panel_enable(rk628);
}

void rk628_rgb_tx_disable(struct rk628 *rk628)
{
	rk628_panel_disable(rk628);
	rk628_panel_unprepare(rk628);

	rk628_rgb_encoder_disable(rk628);
}

static void rk628_bt1120_decoder_timing_cfg(struct rk628 *rk628)
{
	u32 src_hsync_len, src_hback_porch, src_hfront_porch, src_hactive;
	u32 src_vsync_len, src_vback_porch, src_vfront_porch, src_vactive;
	u32 dsp_htotal, dsp_hs_end, dsp_hact_st;
	u32 dsp_vtotal, dsp_vs_end, dsp_vact_st;
	u32 dsp_hbor_st, dsp_vbor_st;
	u16 bor_left = 0, bor_up = 0;
	struct rk628_display_mode *src = &rk628->src_mode;

	src_hactive = src->hdisplay;
	src_hsync_len = src->hsync_end - src->hsync_start;
	src_hback_porch = src->htotal - src->hsync_end;
	src_hfront_porch = src->hsync_start - src->hdisplay;
	src_vsync_len = src->vsync_end - src->vsync_start;
	src_vback_porch = src->vtotal - src->vsync_end;
	src_vfront_porch = src->vsync_start - src->vdisplay;
	src_vactive = src->vdisplay;

	dsp_htotal = src_hsync_len + src_hback_porch +
		     src_hactive + src_hfront_porch;
	dsp_vtotal = src_vsync_len + src_vback_porch +
		     src_vactive + src_vfront_porch;
	dsp_hs_end = src_hsync_len;
	dsp_vs_end = src_vsync_len;
	dsp_hbor_st = src_hsync_len + src_hback_porch;
	dsp_vbor_st = src_vsync_len + src_vback_porch;
	dsp_hact_st = dsp_hbor_st + bor_left;
	dsp_vact_st = dsp_vbor_st + bor_up;

	rk628_i2c_write(rk628, GRF_BT1120_TIMING_CTRL0, BT1120_DSP_HS_END(dsp_hs_end) |
			BT1120_DSP_HTOTAL(dsp_htotal));
	rk628_i2c_write(rk628, GRF_BT1120_TIMING_CTRL1, BT1120_DSP_HACT_ST(dsp_hact_st));
	rk628_i2c_write(rk628, GRF_BT1120_TIMING_CTRL2, BT1120_DSP_VS_END(dsp_vs_end) |
			BT1120_DSP_VTOTAL(dsp_vtotal));
	rk628_i2c_write(rk628, GRF_BT1120_TIMING_CTRL3, BT1120_DSP_VACT_ST(dsp_vact_st));
}

void rk628_bt1120_decoder_enable(struct rk628 *rk628)
{
	struct rk628_display_mode *mode = rk628_display_get_src_mode(rk628);

	rk628_set_input_bus_format(rk628, BUS_FMT_YUV420);
	/* pinctrl for vop pin */
	rk628_i2c_write(rk628, GRF_GPIO2AB_SEL_CON, 0xffffffff);
	rk628_i2c_write(rk628, GRF_GPIO2C_SEL_CON, 0xffff5555);
	rk628_i2c_write(rk628, GRF_GPIO3AB_SEL_CON, 0x10b010b);

	/* rk628: modify IO drive strength for RGB */
	rk628_i2c_write(rk628, GRF_GPIO2A_D0_CON, 0xffff1111);
	rk628_i2c_write(rk628, GRF_GPIO2A_D1_CON, 0xffff1111);
	rk628_i2c_write(rk628, GRF_GPIO2B_D0_CON, 0xffff1111);
	rk628_i2c_write(rk628, GRF_GPIO2B_D1_CON, 0xffff1111);
	rk628_i2c_write(rk628, GRF_GPIO2C_D0_CON, 0xffff1111);
	rk628_i2c_write(rk628, GRF_GPIO2C_D1_CON, 0xffff1111);
	rk628_i2c_write(rk628, GRF_GPIO3A_D0_CON, 0xffff1011);
	rk628_i2c_write(rk628, GRF_GPIO3B_D_CON, 0xf0001);

	/* config sw_input_mode bt1120 */
	rk628_i2c_update_bits(rk628, GRF_SYSTEM_CON0, SW_INPUT_MODE_MASK,
			      SW_INPUT_MODE(INPUT_MODE_BT1120));

	if (rk628->version == RK628F_VERSION)
		rk628_bt1120_decoder_timing_cfg(rk628);

	/* operation resetn_bt1120dec */
	rk628_i2c_write(rk628, CRU_SOFTRST_CON00, 0x10001000);
	rk628_i2c_write(rk628, CRU_SOFTRST_CON00, 0x10000000);

	rk628_cru_clk_set_rate(rk628, CGU_BT1120DEC, mode->clock * 1000);

#ifdef BT1120_DUAL_EDGE
	rk628_i2c_update_bits(rk628, GRF_RGB_DEC_CON0,
			      DEC_DUALEDGE_EN, DEC_DUALEDGE_EN);
	rk628_i2c_write(rk628, GRF_BT1120_DCLK_DELAY_CON0, 0x10000000);
	rk628_i2c_write(rk628, GRF_BT1120_DCLK_DELAY_CON1, 0);
#endif

	rk628_i2c_update_bits(rk628, GRF_RGB_DEC_CON1, SW_SET_X_MASK,
			      SW_SET_X(mode->hdisplay));
	rk628_i2c_update_bits(rk628, GRF_RGB_DEC_CON2, SW_SET_Y_MASK,
			      SW_SET_Y(mode->vdisplay));

	rk628_i2c_update_bits(rk628, GRF_SYSTEM_CON0,
			      SW_BT_DATA_OEN_MASK | SW_INPUT_MODE_MASK,
			      SW_BT_DATA_OEN | SW_INPUT_MODE(INPUT_MODE_BT1120));
	rk628_i2c_write(rk628, GRF_CSC_CTRL_CON, SW_Y2R_EN(1));
	rk628_i2c_update_bits(rk628, GRF_RGB_DEC_CON0,
			      SW_CAP_EN_PSYNC | SW_CAP_EN_ASYNC | SW_PROGRESS_EN,
			      SW_CAP_EN_PSYNC | SW_CAP_EN_ASYNC | SW_PROGRESS_EN);
}

void rk628_bt1120_encoder_enable(struct rk628 *rk628)
{
	u32 val = 0;

	rk628_set_output_bus_format(rk628, BUS_FMT_YUV420);
	/* pinctrl for vop pin */
	rk628_i2c_write(rk628, GRF_GPIO2AB_SEL_CON, 0xffffffff);
	rk628_i2c_write(rk628, GRF_GPIO2C_SEL_CON, 0xffff5555);
	rk628_i2c_write(rk628, GRF_GPIO3AB_SEL_CON, 0x10b010b);

	/* rk628: modify IO drive strength for RGB */
	rk628_i2c_write(rk628, GRF_GPIO2A_D0_CON, 0xffff1111);
	rk628_i2c_write(rk628, GRF_GPIO2A_D1_CON, 0xffff1111);
	rk628_i2c_write(rk628, GRF_GPIO2B_D0_CON, 0xffff1111);
	rk628_i2c_write(rk628, GRF_GPIO2B_D1_CON, 0xffff1111);
	rk628_i2c_write(rk628, GRF_GPIO2C_D0_CON, 0xffff1111);
	rk628_i2c_write(rk628, GRF_GPIO2C_D1_CON, 0xffff1111);
	rk628_i2c_write(rk628, GRF_GPIO3A_D0_CON, 0xffff1011);
	rk628_i2c_write(rk628, GRF_GPIO3B_D_CON, 0xf0001);

	rk628_i2c_update_bits(rk628, GRF_SYSTEM_CON0,
			      SW_BT_DATA_OEN_MASK | SW_OUTPUT_RGB_MODE_MASK,
			      SW_OUTPUT_RGB_MODE(OUTPUT_MODE_BT1120 >> 3));
	rk628_i2c_write(rk628, GRF_CSC_CTRL_CON, SW_R2Y_EN(1));
	rk628_i2c_update_bits(rk628, GRF_POST_PROC_CON,
			      SW_DCLK_OUT_INV_EN, SW_DCLK_OUT_INV_EN);

#ifdef BT1120_DUAL_EDGE
	val |= ENC_DUALEDGE_EN(1);
	rk628_i2c_write(rk628, GRF_BT1120_DCLK_DELAY_CON0, 0x10000000);
	rk628_i2c_write(rk628, GRF_BT1120_DCLK_DELAY_CON1, 0);
#endif
	val |= BT1120_UV_SWAP(1);
	rk628_i2c_write(rk628, GRF_RGB_ENC_CON, val);
}

void rk628_bt1120_rx_enable(struct rk628 *rk628)
{
	rk628_bt1120_decoder_enable(rk628);
}

void rk628_bt1120_tx_enable(struct rk628 *rk628)
{
	rk628_bt1120_encoder_enable(rk628);
}

