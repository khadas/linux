// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 *
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#include <linux/types.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#ifdef CONFIG_AMLOGIC_VPU
#include <linux/amlogic/media/vpu/vpu.h>
#endif
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include "DP_tx.h"
#include "../lcd_tablet.h"
#include "../../lcd_reg.h"
#include "../../lcd_common.h"

#define EDP_HPD_TIMEOUT     500
#define EDP_READY_AFTER_HPD 150

void edp_tx_init(struct aml_lcd_drv_s *pdrv)
{
	unsigned char auxdata[2];
	unsigned int offset;
	int i, ret;
	struct edp_config_s *edp_cfg = &pdrv->config.control.edp_cfg;
	struct dptx_EDID_s edp_edid1;
	struct dptx_detail_timing_s *tm;

	if (!pdrv)
		return;
	if (pdrv->index > 1) {
		LCDERR("[%d]: %s: invalid drv_index\n", pdrv->index, __func__);
		return;
	}

	offset = pdrv->data->offset_venc_data[pdrv->index];

	lcd_vcbus_write(ENCL_VIDEO_EN + offset, 0);

	dptx_reset(pdrv);

	dptx_wait_phy_ready(pdrv);
	usleep_range(1000, 2000);

	dptx_reg_write(pdrv, EDP_TX_TRANSMITTER_OUTPUT_ENABLE, 0x1);
	dptx_reg_write(pdrv, EDP_TX_AUX_INTERRUPT_MASK, 0xf);	//turn off interrupt

	i = 0;
	while (i++ < EDP_HPD_TIMEOUT) {
		edp_cfg->HPD_level = dptx_reg_getb(pdrv, EDP_TX_AUX_STATE, 0, 1);
		if (edp_cfg->HPD_level)
			break;
		usleep_range(2000, 5000);
	}
	LCDPR("[%d]: eDP HPD state: %d, i=%d\n", pdrv->index, edp_cfg->HPD_level, i);
	if (edp_cfg->HPD_level == 0)
		return;

	mdelay(EDP_READY_AFTER_HPD);

	ret = DPCD_capability_detect(pdrv); //! before timing and training
	if (ret) {
		LCDERR("[%d]: DP DPCD_capability_detect ERROR\n", pdrv->index);
		return;
	}

	edp_cfg->lane_count = edp_cfg->max_lane_count;
	edp_cfg->link_rate = edp_cfg->max_link_rate;

	dptx_set_lane_config(pdrv);
	dptx_set_phy_config(pdrv, 1);

	// Power up link
	auxdata[0] = 0x1;
	ret = dptx_aux_write(pdrv, DPCD_SET_POWER, 1, auxdata);
	if (ret) {
		LCDERR("[%d]: eDP sink power up link failed.....\n", pdrv->index);
		return;
	}
	msleep(30);

	dptx_reg_write(pdrv, EDP_TX_MAIN_STREAM_ENABLE, 0x0);

	dptx_link_training(pdrv);

	if (edp_cfg->edid_en) {
		dptx_EDID_probe(pdrv, &edp_edid1);
		dptx_manage_timing(pdrv, &edp_edid1);
		tm = dptx_get_optimum_timing(pdrv);
		if (tm) {
			dptx_timing_update(pdrv, tm);
			dptx_timing_apply(pdrv);
		}
	}

	dptx_set_ContentProtection(pdrv);

	dptx_set_msa(pdrv);

	dptx_fast_link_training(pdrv);

	lcd_vcbus_write(ENCL_VIDEO_EN + offset, 1);
	dptx_reg_write(pdrv, EDP_TX_FORCE_SCRAMBLER_RESET, 0x1);
	dptx_reg_write(pdrv, EDP_TX_MAIN_STREAM_ENABLE, 0x1);

	LCDPR("[%d]: eDP enable main stream video\n", pdrv->index);
}

static void edp_tx_disable(struct aml_lcd_drv_s *pdrv)
{
	unsigned char auxdata;
	int ret;

	dptx_clear_timing(pdrv);
	// Power down link
	auxdata = 0x2;
	ret = dptx_aux_write(pdrv, DPCD_SET_POWER, 1, &auxdata);
	if (ret)
		LCDERR("[%d]: edp sink power down link failed.....\n", pdrv->index);

	dptx_reg_write(pdrv, EDP_TX_MAIN_STREAM_ENABLE, 0x0);
	LCDPR("[%d]: edp disable main stream video\n", pdrv->index);

	// disable the transmitter
	dptx_reg_write(pdrv, EDP_TX_TRANSMITTER_OUTPUT_ENABLE, 0x0);
}

static void edp_power_init(int index)
{
#ifdef CONFIG_SECURE_POWER_CONTROL
//#define PM_EDP0          48
//#define PM_EDP1          49
	if (index)
		pwr_ctrl_psci_smc(PM_EDP1, 1);
	else
		pwr_ctrl_psci_smc(PM_EDP0, 1);
	LCDPR("[%d]: edp power domain on\n", index);
#endif
}

void edp_tx_ctrl(struct aml_lcd_drv_s *pdrv, int flag)
{
	if (flag) {
		edp_power_init(pdrv->index);
		edp_tx_init(pdrv);
	} else {
		edp_tx_disable(pdrv);
	}
}
