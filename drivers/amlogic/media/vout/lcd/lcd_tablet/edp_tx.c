// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 *
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#include <linux/types.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include "../lcd_reg.h"
#include "../lcd_clk_config.h"
#include "../lcd_common.h"

#define EDP_TX_AUX_REQ_TIMEOUT   1000
#define EDP_TX_AUX_REQ_INTERVAL  1
#define EDP_AUX_RETRY_CNT        5
#define EDP_AUX_TIMEOUT          1000
#define EDP_AUX_INTERVAL         200
static int dptx_aux_write(struct aml_lcd_drv_s *pdrv, unsigned int addr,
			  unsigned int len, unsigned char *buf)
{
	unsigned int data, i, state;
	unsigned int retry_cnt = 0, timeout = 0;

dptx_aux_write_retry:
	timeout = 0;
	while (timeout++ < EDP_TX_AUX_REQ_TIMEOUT) {
		state = dptx_reg_getb(pdrv, EDP_TX_AUX_STATE, 1, 1);
		if (state == 0)
			break;
		lcd_delay_us(EDP_TX_AUX_REQ_INTERVAL);
	};

	dptx_reg_write(pdrv, EDP_TX_AUX_ADDRESS, addr);
	for (i = 0; i < len; i++)
		dptx_reg_write(pdrv, EDP_TX_AUX_WRITE_FIFO, buf[i]);

	dptx_reg_write(pdrv, EDP_TX_AUX_COMMAND, (0x800 | ((len - 1) & 0xf)));

	timeout = 0;
	while (timeout++ < EDP_AUX_TIMEOUT) {
		lcd_delay_us(EDP_AUX_INTERVAL);
		data = dptx_reg_read(pdrv, EDP_TX_AUX_TRANSFER_STATUS);
		if (data & (1 << 0)) {
			state = dptx_reg_read(pdrv, EDP_TX_AUX_REPLY_CODE);
			if (state == 0)
				return 0;
			if (state == 1) {
				LCDPR("[%d]: edp aux write addr 0x%x NACK!\n",
				      pdrv->index, addr);
				return -1;
			}
			if (state == 2) {
				LCDPR("[%d]: edp aux write addr 0x%x Defer!\n",
				      pdrv->index, addr);
			}
			break;
		}

		if (data & (1 << 3)) {
			LCDPR("[%d]: edp aux write addr 0x%x Error!\n",
			      pdrv->index, addr);
			break;
		}
	}

	if (retry_cnt++ < EDP_AUX_RETRY_CNT) {
		lcd_delay_us(EDP_AUX_INTERVAL);
		LCDPR("[%d]: edp aux write addr 0x%x timeout, retry %d\n",
		      pdrv->index, addr, retry_cnt);
		goto dptx_aux_write_retry;
	}

	LCDPR("[%d]: edp aux write addr 0x%x failed\n", pdrv->index, addr);
	return -1;
}

static int dptx_aux_read(struct aml_lcd_drv_s *pdrv, unsigned int addr,
			 unsigned int len, unsigned char *buf)
{
	unsigned int data, i, state;
	unsigned int retry_cnt = 0, timeout = 0;

dptx_aux_read_retry:
	timeout = 0;
	while (timeout++ < EDP_TX_AUX_REQ_TIMEOUT) {
		state = dptx_reg_getb(pdrv, EDP_TX_AUX_STATE, 1, 1);
		if (state == 0)
			break;
		lcd_delay_us(EDP_TX_AUX_REQ_INTERVAL);
	};

	dptx_reg_write(pdrv, EDP_TX_AUX_ADDRESS, addr);
	dptx_reg_write(pdrv, EDP_TX_AUX_COMMAND, (0x900 | ((len - 1) & 0xf)));

	timeout = 0;
	while (timeout++ < EDP_AUX_TIMEOUT) {
		lcd_delay_us(EDP_AUX_INTERVAL);
		data = dptx_reg_read(pdrv, EDP_TX_AUX_TRANSFER_STATUS);
		if (data & (1 << 0)) {
			state = dptx_reg_read(pdrv, EDP_TX_AUX_REPLY_CODE);
			if (state == 0)
				goto dptx_aux_read_succeed;
			if (state == 1) {
				LCDPR("[%d]: edp aux read addr 0x%x NACK!\n",
				      pdrv->index, addr);
				return -1;
			}
			if (state == 2) {
				LCDPR("[%d]: edp aux read addr 0x%x Defer!\n",
				      pdrv->index, addr);
			}
			break;
		}

		if (data & (1 << 3)) {
			LCDPR("[%d]: edp aux read addr 0x%x Error!\n", pdrv->index, addr);
			break;
		}
	}

	if (retry_cnt++ < EDP_AUX_RETRY_CNT) {
		lcd_delay_us(EDP_AUX_INTERVAL);
		LCDPR("[%d]: edp aux read addr 0x%x timeout, retry %d\n",
		      pdrv->index, addr, retry_cnt);
		goto dptx_aux_read_retry;
	}

	LCDPR("[%d]: edp aux read addr 0x%x failed\n", pdrv->index, addr);
	return -1;

dptx_aux_read_succeed:
	for (i = 0; i < len; i++)
		buf[i] = (unsigned char)(dptx_reg_read(pdrv, EDP_TX_AUX_REPLY_DATA));

	return 0;
}

static void dptx_link_fast_training(struct aml_lcd_drv_s *pdrv)
{
	int index = pdrv->index;
	unsigned char p_data = 0;
	int ret;

	/* disable scrambling */
	dptx_reg_write(pdrv, EDP_TX_SCRAMBLING_DISABLE, 0x1);

	/* set training pattern 1 */
	dptx_reg_write(pdrv, EDP_TX_TRAINING_PATTERN_SET, 0x1);
	p_data = 0x21;
	ret = dptx_aux_write(pdrv, EDP_DPCD_TRAINING_PATTERN_SET, 1, &p_data);
	if (ret)
		LCDERR("[%d]: edp training pattern 1 failed.....\n", index);
	lcd_delay_us(10);

	/* set training pattern 2 */
	dptx_reg_write(pdrv, EDP_TX_TRAINING_PATTERN_SET, 0x2);
	p_data = 0x22;
	ret = dptx_aux_write(pdrv, EDP_DPCD_TRAINING_PATTERN_SET, 1, &p_data);
	if (ret)
		LCDERR("[%d]: edp training pattern 2 failed.....\n", index);
	lcd_delay_us(10);

	/* set training pattern 3 */
	dptx_reg_write(pdrv, EDP_TX_TRAINING_PATTERN_SET, 0x3);
	p_data = 0x23;
	ret = dptx_aux_write(pdrv, EDP_DPCD_TRAINING_PATTERN_SET, 1, &p_data);
	if (ret)
		LCDERR("[%d]: edp training pattern 3 failed.....\n", index);
	lcd_delay_us(10);

	/* disable the training pattern */
	p_data = 0x20;
	ret = dptx_aux_write(pdrv, EDP_DPCD_TRAINING_PATTERN_SET, 1, &p_data);
	if (ret)
		LCDERR("[%d]: edp training pattern off failed.....\n", index);
	dptx_reg_write(pdrv, EDP_TX_TRAINING_PATTERN_SET, 0x0);
}

void dptx_dpcd_dump(struct aml_lcd_drv_s *pdrv)
{
	int index = pdrv->index;
	unsigned char p_data[12];
	int ret, i;

	if (index > 1) {
		LCDERR("[%d]: %s: invalid drv_index\n", index, __func__);
		return;
	}

	memset(p_data, 0, 12);
	LCDPR("[%d]: edp DPCD link status:\n", index);
	ret = dptx_aux_read(pdrv, 0x100, 8, p_data);
	if (ret == 0) {
		for (i = 0; i < 8; i++)
			pr_info("0x%04x: 0x%02x\n", (0x100 + i), p_data[i]);
		pr_info("\n");
	}

	memset(p_data, 0, 12);
	LCDPR("[%d]: edp DPCD training status:\n", index);
	ret = dptx_aux_read(pdrv, 0x200, 12, p_data);
	if (ret == 0) {
		for (i = 0; i < 12; i++)
			pr_info("0x%04x: 0x%02x\n", (0x200 + i), p_data[i]);
		pr_info("\n");
	}
}

static void dptx_set_msa(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_config_s *pconf = &pdrv->config;
	unsigned int hactive, vactive, htotal, vtotal, hsw, hbp, vsw, vbp;
	unsigned int bpc, data_per_lane, misc0_data, bit_depth, sync_mode;
	unsigned int m_vid; /*pclk/1000 */
	unsigned int n_vid; /*162000, 270000, 540000 */
	unsigned int ppc = 1; /* 1 pix per clock pix0 only */
	unsigned int cfmt = 0; /* RGB */

	hactive = pconf->basic.h_active;
	vactive = pconf->basic.v_active;
	htotal = pconf->basic.h_period;
	vtotal = pconf->basic.v_period;
	hsw = pconf->timing.hsync_width;
	hbp = pconf->timing.hsync_bp;
	vsw = pconf->timing.vsync_width;
	vbp = pconf->timing.vsync_bp;

	m_vid = pconf->timing.lcd_clk / 1000;
	switch (pconf->control.edp_cfg.link_rate) {
	case 1: /* 2.7G */
		n_vid = 270000;
		break;
	case 0: /* 1.62G */
	default:
		n_vid = 162000;
		break;
	}
	 /*6bit:0x0, 8bit:0x1, 10bit:0x2, 12bit:0x3 */
	switch (pconf->basic.lcd_bits) {
	case 6:
		bit_depth = 0x0;
		break;
	case 8:
		bit_depth = 0x1;
		break;
	case 10:
		bit_depth = 0x2;
		break;
	default:
		bit_depth = 0x7;
		break;
	}
	bpc = pconf->basic.lcd_bits; /* bits per color */
	sync_mode = pconf->control.edp_cfg.sync_clk_mode;
	data_per_lane = ((hactive * bpc * 3) + 15) / 16 - 1;

	/*bit[0] sync mode (1=sync 0=async) */
	misc0_data = (cfmt << 1) | (sync_mode << 0);
	misc0_data |= (bit_depth << 5);

	dptx_reg_write(pdrv, EDP_TX_MAIN_STREAM_HTOTAL, htotal);
	dptx_reg_write(pdrv, EDP_TX_MAIN_STREAM_VTOTAL, vtotal);
	dptx_reg_write(pdrv, EDP_TX_MAIN_STREAM_POLARITY, (0 << 1) | (0 << 0));
	dptx_reg_write(pdrv, EDP_TX_MAIN_STREAM_HSWIDTH, hsw);
	dptx_reg_write(pdrv, EDP_TX_MAIN_STREAM_VSWIDTH, vsw);
	dptx_reg_write(pdrv, EDP_TX_MAIN_STREAM_HRES, hactive);
	dptx_reg_write(pdrv, EDP_TX_MAIN_STREAM_VRES, vactive);
	dptx_reg_write(pdrv, EDP_TX_MAIN_STREAM_HSTART, (hsw + hbp));
	dptx_reg_write(pdrv, EDP_TX_MAIN_STREAM_VSTART, (vsw + vbp));
	dptx_reg_write(pdrv, EDP_TX_MAIN_STREAM_MISC0, misc0_data);
	dptx_reg_write(pdrv, EDP_TX_MAIN_STREAM_MISC1, 0x00000000);
	dptx_reg_write(pdrv, EDP_TX_MAIN_STREAM_M_VID, m_vid); /*unit: 1kHz */
	dptx_reg_write(pdrv, EDP_TX_MAIN_STREAM_N_VID, n_vid); /*unit: 10kHz */
	dptx_reg_write(pdrv, EDP_TX_MAIN_STREAM_TRANSFER_UNIT_SIZE, 32);
	dptx_reg_write(pdrv, EDP_TX_MAIN_STREAM_DATA_COUNT_PER_LANE,
		       data_per_lane);
	dptx_reg_write(pdrv, EDP_TX_MAIN_STREAM_USER_PIXEL_WIDTH, ppc);
}

static void dptx_reset(struct aml_lcd_drv_s *pdrv)
{
	unsigned int bit;

	if (pdrv->index)
		bit = 18;
	else
		bit = 17;

	lcd_reset_setb(pdrv, RESETCTRL_RESET1_MASK, 0, bit, 1);
	lcd_reset_setb(pdrv, RESETCTRL_RESET1_LEVEL, 0, bit, 1);
	lcd_delay_us(1);
	lcd_reset_setb(pdrv, RESETCTRL_RESET1_LEVEL, 1, bit, 1);
	lcd_delay_us(1);
}

static void dptx_phy_reset(struct aml_lcd_drv_s *pdrv)
{
	unsigned int bit;

	if (pdrv->index)
		bit = 20;
	else
		bit = 19;

	lcd_reset_setb(pdrv, RESETCTRL_RESET1_MASK, 0, bit, 1);
	lcd_reset_setb(pdrv, RESETCTRL_RESET1_LEVEL, 0, bit, 1);
	lcd_delay_us(1);
	lcd_reset_setb(pdrv, RESETCTRL_RESET1_LEVEL, 1, bit, 1);
	lcd_delay_us(1);
}

static int dptx_wait_phy_ready(struct aml_lcd_drv_s *pdrv)
{
	int index = pdrv->index;
	unsigned int data = 0;
	unsigned int done = 100;

	do {
		data = dptx_reg_read(pdrv, EDP_TX_PHY_STATUS);
		if (done < 20) {
			LCDPR("dptx%d wait phy ready: reg_val=0x%x, wait_count=%u\n",
			      index, data, (100 - done));
		}
		done--;
		lcd_delay_us(100);
	} while (((data & 0x7f) != 0x7f) && (done > 0));

	if ((data & 0x7f) == 0x7f)
		return 0;

	LCDERR("[%d]: edp tx phy error!\n", index);
	return -1;
}

#define EDP_HPD_TIMEOUT    1000
static void edp_tx_init(struct aml_lcd_drv_s *pdrv)
{
	unsigned int hpd_state = 0;
	unsigned char auxdata[2];
	unsigned int offset;
	int i, index, ret;

	index = pdrv->index;
	if (index > 1) {
		LCDERR("%s: invalid drv_index %d\n", __func__, index);
		return;
	}

	offset = pdrv->data->offset_venc_data[pdrv->index];

	dptx_phy_reset(pdrv);
	dptx_reset(pdrv);
	mdelay(2);

	lcd_vcbus_write(ENCL_VIDEO_EN + offset, 0);

	/* Set Aux channel clk-div: 24MHz */
	dptx_reg_write(pdrv, EDP_TX_AUX_CLOCK_DIVIDER, 24);

	/* Enable the transmitter */
	/* remove the reset on the PHY */
	dptx_reg_write(pdrv, EDP_TX_PHY_RESET, 0);
	dptx_wait_phy_ready(pdrv);
	mdelay(2);
	dptx_reg_write(pdrv, EDP_TX_TRANSMITTER_OUTPUT_ENABLE, 0x1);

	i = 0;
	while (i++ < EDP_HPD_TIMEOUT) {
		hpd_state = dptx_reg_getb(pdrv, EDP_TX_AUX_STATE, 0, 1);
		if (hpd_state)
			break;
		mdelay(2);
	}
	LCDPR("[%d]: edp HPD state: %d, i=%d\n", index, hpd_state, i);

	/* tx Link-rate and Lane_count */
	dptx_reg_write(pdrv, EDP_TX_LINK_BW_SET, 0x0a); /* Link-rate */
	dptx_reg_write(pdrv, EDP_TX_LINK_COUNT_SET, 0x02); /* Lanes */

	/* sink Link-rate and Lane_count */
	auxdata[0] = 0x0a; /* 2.7GHz //EDP_DPCD_LINK_BANDWIDTH_SET */
	auxdata[1] = 2;              /*EDP_DPCD_LANE_COUNT_SET */
	ret = dptx_aux_write(pdrv, EDP_DPCD_LINK_BANDWIDTH_SET, 2, auxdata);
	if (ret)
		LCDERR("[%d]: edp sink set lane rate & count failed.....\n", index);

	/* Power up link */
	auxdata[0] = 0x1;
	ret = dptx_aux_write(pdrv, EDP_DPCD_SET_POWER, 1, auxdata);
	if (ret)
		LCDERR("[%d]: edp sink power up link failed.....\n", index);

	dptx_link_fast_training(pdrv);
	/*dptx_dpcd_dump(pdrv); */

	dptx_set_msa(pdrv);
	lcd_vcbus_write(ENCL_VIDEO_EN + offset, 1);

	dptx_reg_write(pdrv, EDP_TX_MAIN_STREAM_ENABLE, 0x1);
	LCDPR("[%d]: edp enable main stream video\n", index);
}

static void edp_tx_disable(struct aml_lcd_drv_s *pdrv)
{
	unsigned char auxdata;
	int index, ret;

	index = pdrv->index;
	if (index > 1) {
		LCDERR("%s: invalid drv_index %d\n", __func__, index);
		return;
	}

	/* Power down link */
	auxdata = 0x2;
	ret = dptx_aux_write(pdrv, EDP_DPCD_SET_POWER, 1, &auxdata);
	if (ret)
		LCDERR("[%d]: edp sink power down link failed.....\n", index);

	dptx_reg_write(pdrv, EDP_TX_MAIN_STREAM_ENABLE, 0x0);
	LCDPR("[%d]: edp disable main stream video\n", index);

	/* disable the transmitter */
	dptx_reg_write(pdrv, EDP_TX_TRANSMITTER_OUTPUT_ENABLE, 0x0);
}

static void edp_power_init(int index)
{
#ifdef CONFIG_SECURE_POWER_CONTROL
/*#define PM_EDP0          48 */
/*#define PM_EDP1          49 */
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
