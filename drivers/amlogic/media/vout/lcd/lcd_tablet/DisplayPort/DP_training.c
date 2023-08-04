// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 *
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#include <linux/types.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#ifdef CONFIG_AMLOGIC_VPU
#include <linux/amlogic/media/vpu/vpu.h>
#endif
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include "DP_tx.h"
#include "../../lcd_reg.h"
#include "../../lcd_common.h"

static void dptx_training_status_print(struct aml_lcd_drv_s *pdrv)
{
	unsigned char aux_data[3];
	int ret;

	ret = dptx_aux_read(pdrv, DPCD_LANE0_1_STATUS, 3, aux_data);
	if (ret == 0) {
		LCDPR("[%d]: %s: aux_data [0]=0x%x, [1]=0x%x, [2]=0x%x\n",
			pdrv->index, __func__, aux_data[0], aux_data[1], aux_data[2]);
	}
}

/* @return:
 * 0: link rate reduced
 * 1: link rate already lowest RBR (1.62G)
 * 2: link rate not cover resolution (eDP)
 */
static int dptx_link_rate_config_reduce(struct aml_lcd_drv_s *pdrv)
{
	unsigned char link_rate, lane_cnt;
	// unsigned int bit_rate;
	int ret;

	link_rate = pdrv->config.control.edp_cfg.link_rate;
	lane_cnt = pdrv->config.control.edp_cfg.lane_count;
	switch (link_rate) {
	case DP_LINK_RATE_HBR2:
		link_rate = DP_LINK_RATE_HBR;
		break;
	case DP_LINK_RATE_HBR:
		link_rate = DP_LINK_RATE_RBR;
		break;
	case DP_LINK_RATE_RBR:
	default:
		LCDERR("%s: already lowest link rate\n", __func__);
		return 1;
	}

	//DP policy: flexable resolution
	//eDP policy: fixed resolution, valid BW required
	if (pdrv->config.basic.lcd_type == LCD_EDP) {
		ret = dptx_band_width_check(link_rate, lane_cnt,
			pdrv->config.timing.lcd_clk, pdrv->config.basic.lcd_bits * 3);
		if (ret) {
			LCDERR("%s: band width check failed\n", __func__);
			return 2;
		}
	}

	LCDPR("%s: link_rate: 0x%x, lane_cnt: %d\n", __func__, link_rate, lane_cnt);
	pdrv->config.control.edp_cfg.link_rate = link_rate;
	pdrv->config.control.edp_cfg.link_rate_update = 1;

	return 0;
}

/* @brief:
 * 1. read DPCD_ADJUST_REQUEST_LANE0_1 2_3
 * 2. store to edp_cfg: [adj_req_preem, adj_req_vswing]
 * 3. compare between [curr*, adj_reg*], set phy_update tag
 */
static int dptx_training_phy_adj_req_process(struct aml_lcd_drv_s *pdrv)
{
	struct edp_config_s *edp_cfg = &pdrv->config.control.edp_cfg;
	unsigned char adj_req_lane[2], i;

	// dptx_dpcd_read(pdrv, adj_req_lane, DPCD_ADJUST_REQUEST_LANE0_1, 2);
	dptx_aux_read(pdrv, DPCD_ADJUST_REQUEST_LANE0_1, 2, adj_req_lane);
	/* parse DPCD adjust request */
	edp_cfg->adj_req_vswing[0] = (adj_req_lane[0] & 0x03);
	edp_cfg->adj_req_vswing[1] = ((adj_req_lane[0] >> 4) & 0x03);
	edp_cfg->adj_req_vswing[2] = (adj_req_lane[1] & 0x03);
	edp_cfg->adj_req_vswing[3] = ((adj_req_lane[1] >> 4) & 0x03);
	edp_cfg->adj_req_preem[0]  = ((adj_req_lane[0] >> 2) & 0x03);
	edp_cfg->adj_req_preem[1]  = ((adj_req_lane[0] >> 6) & 0x03);
	edp_cfg->adj_req_preem[2]  = ((adj_req_lane[1] >> 2) & 0x03);
	edp_cfg->adj_req_preem[3]  = ((adj_req_lane[1] >> 6) & 0x03);

	//determine vswing & preem change on any lane
	edp_cfg->phy_update = 0;
	for (i = 0; i < 4; i++) {
		edp_cfg->phy_update |=
			(edp_cfg->adj_req_vswing[i] != edp_cfg->curr_vswing[i]) << (i * 2);
		edp_cfg->phy_update |=
			(edp_cfg->adj_req_preem[i] != edp_cfg->curr_preem[i]) << (i * 2 + 1);
	}
	LCDPR("[%d]: %s: phy update: 0x%02x\n", pdrv->index, __func__, edp_cfg->phy_update);

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL && edp_cfg->phy_update) {
		pr_info(" ----- 0:Vsw/Pre - 1:Vsw/Pre - 2:Vsw/Pre - 3:Vsw/Pre --\n"
			" adj_req:  %d %d   |   %d %d   |   %d %d   |   %d %d\n"
			" current:  %d %d   |   %d %d   |   %d %d   |   %d %d\n",
			edp_cfg->adj_req_vswing[0], edp_cfg->adj_req_preem[0],
			edp_cfg->adj_req_vswing[1], edp_cfg->adj_req_preem[1],
			edp_cfg->adj_req_vswing[2], edp_cfg->adj_req_preem[2],
			edp_cfg->adj_req_vswing[3], edp_cfg->adj_req_preem[3],
			edp_cfg->curr_vswing[0], edp_cfg->curr_preem[0],
			edp_cfg->curr_vswing[1], edp_cfg->curr_preem[1],
			edp_cfg->curr_vswing[2], edp_cfg->curr_preem[2],
			edp_cfg->curr_vswing[3], edp_cfg->curr_preem[3]);
	}
	return 0;
}

/* @brief
 * 1. Setup lcd phy
 * 2. Setup DP ip reg
 * 3. transmit phy setting over aux
 * 4. update edp_cfg: [curr_preem, curr_vswing, phy_update]
 */
static int dptx_training_phy_update_apply(struct aml_lcd_drv_s *pdrv)
{
	struct edp_config_s *_cfg = &pdrv->config.control.edp_cfg;
	struct phy_config_s *phy = &pdrv->config.phy_cfg;
	unsigned char aux_data[4];
	int max_vsw = 0, max_pre = 0;
	int i, ret;

	if (_cfg->phy_update == 0)
		return 0;

	for (i = 0; i < 4; i++) {
		max_vsw = max_vsw > _cfg->adj_req_vswing[i] ? max_vsw : _cfg->adj_req_vswing[i];
		max_pre = max_pre > _cfg->adj_req_preem[i] ? max_pre : _cfg->adj_req_preem[i];
	}

	phy->vswing_level = dptx_vswing_std_to_phy(max_vsw);
	phy->preem_level = dptx_preem_std_to_phy(max_pre);

	lcd_phy_set(pdrv, 1);

	dptx_reg_write(pdrv, EDP_TX_PHY_VOLTAGE_DIFF_LANE_0, max_vsw);
	dptx_reg_write(pdrv, EDP_TX_PHY_VOLTAGE_DIFF_LANE_1, max_vsw);
	dptx_reg_write(pdrv, EDP_TX_PHY_VOLTAGE_DIFF_LANE_2, max_vsw);
	dptx_reg_write(pdrv, EDP_TX_PHY_VOLTAGE_DIFF_LANE_3, max_vsw);
	dptx_reg_write(pdrv, EDP_TX_PHY_PRE_EMPHASIS_LANE_0, max_pre);
	dptx_reg_write(pdrv, EDP_TX_PHY_PRE_EMPHASIS_LANE_1, max_pre);
	dptx_reg_write(pdrv, EDP_TX_PHY_PRE_EMPHASIS_LANE_2, max_pre);
	dptx_reg_write(pdrv, EDP_TX_PHY_PRE_EMPHASIS_LANE_3, max_pre);

	aux_data[0] = to_DPCD_LANESET(max_vsw, max_pre);
	aux_data[1] = to_DPCD_LANESET(max_vsw, max_pre);
	aux_data[2] = to_DPCD_LANESET(max_vsw, max_pre);
	aux_data[3] = to_DPCD_LANESET(max_vsw, max_pre);

	//set edp phy config !!!!!!!!!! support multiple lane IN phy
	//phy->vswing_level = dptx_vswing_std_to_phy(_cfg->adj_req_vswing[0]);
	//phy->preem_level = dptx_preem_std_to_phy(_cfg->adj_req_preem[0]);

	//dptx_reg_write(pdrv, EDP_TX_PHY_VOLTAGE_DIFF_LANE_0, _cfg->adj_req_vswing[0]);
	//dptx_reg_write(pdrv, EDP_TX_PHY_VOLTAGE_DIFF_LANE_1, _cfg->adj_req_vswing[1]);
	//dptx_reg_write(pdrv, EDP_TX_PHY_VOLTAGE_DIFF_LANE_2, _cfg->adj_req_vswing[2]);
	//dptx_reg_write(pdrv, EDP_TX_PHY_VOLTAGE_DIFF_LANE_3, _cfg->adj_req_vswing[3]);
	//dptx_reg_write(pdrv, EDP_TX_PHY_PRE_EMPHASIS_LANE_0, _cfg->adj_req_preem[0]);
	//dptx_reg_write(pdrv, EDP_TX_PHY_PRE_EMPHASIS_LANE_1, _cfg->adj_req_preem[1]);
	//dptx_reg_write(pdrv, EDP_TX_PHY_PRE_EMPHASIS_LANE_2, _cfg->adj_req_preem[2]);
	//dptx_reg_write(pdrv, EDP_TX_PHY_PRE_EMPHASIS_LANE_3, _cfg->adj_req_preem[3]);

	//aux_data[0] = to_DPCD_LANESET(_cfg->adj_req_vswing[0], _cfg->adj_req_preem[0]);
	//aux_data[1] = to_DPCD_LANESET(_cfg->adj_req_vswing[1], _cfg->adj_req_preem[1]);
	//aux_data[2] = to_DPCD_LANESET(_cfg->adj_req_vswing[2], _cfg->adj_req_preem[2]);
	//aux_data[3] = to_DPCD_LANESET(_cfg->adj_req_vswing[3], _cfg->adj_req_preem[3]);

	ret = dptx_aux_write(pdrv, DPCD_TRAINING_LANE0_SET, 4, aux_data);
	if (ret)
		LCDERR("[%d]: %s: write DPCD_TRAINING_LANE0_SET failed\n", pdrv->index, __func__);

	for (i = 0; i < 4; i++) {
		_cfg->curr_vswing[i] = max_vsw;
		_cfg->curr_preem[i] = max_pre;
		//_cfg->curr_vswing[i] = _cfg->adj_req_vswing[0];
		//_cfg->curr_preem[i] = _cfg->adj_req_preem[0];
	}

	_cfg->phy_update = 0;
	return ret;
}

/* *************** full link training ****************** */
static int dptx_set_training_pattern(struct aml_lcd_drv_s *pdrv, unsigned char pattern)
{
	unsigned char p_data = 0;
	int ret;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s: pattern %u\n", pdrv->index, __func__, pattern);
	//disable scrambling for any active test pattern
	//! EDP_SCRAMBLING NOT TURN ON EVEN NOT TRAINING
	// if (pattern && pattern <= 3)
	p_data = (1 << 5) | pattern;

	dptx_reg_write(pdrv, EDP_TX_TRAINING_PATTERN_SET, pattern);
	ret = dptx_aux_write(pdrv, DPCD_TRAINING_PATTERN_SET, 1, &p_data);
	if (ret)
		LCDERR("[%d]: write DPCD_TRAINING_PATTERN_SET: %d failed\n", pdrv->index, pattern);

	return ret;
}

// Lane status register constants
#define BIT_DPCD_LANE_02_STATUS_CLK_REC_DONE  0
#define BIT_DPCD_LANE_02_STATUS_CHAN_EQ_DONE  1
#define BIT_DPCD_LANE_02_STATUS_SYM_LOCK_DONE 2
#define BIT_DPCD_LANE_13_STATUS_CLK_REC_DONE  4
#define BIT_DPCD_LANE_13_STATUS_CHAN_EQ_DONE  5
#define BIT_DPCD_LANE_13_STATUS_SYM_LOCK_DONE 6
#define BIT_DPCD_LANE_ALIGNMENT_DONE          0

static int dptx_check_channel_equalization(struct aml_lcd_drv_s *pdrv)
{
	unsigned int cr_done, channel_eq_done, symbol_lock_done, lane_align_done;
	unsigned char clk_rec[4], chan_eq[4], sym_lock[4], aux_data[3];
	unsigned char lane_cnt;
	int ret;

	lane_cnt = pdrv->config.control.edp_cfg.lane_count;

	ret = dptx_aux_read(pdrv, DPCD_LANE0_1_STATUS, 3, aux_data);
	if (ret) {
		LCDERR("[%d]: %s: aux read DPCD_LANE0_1_STATUS failed\n", pdrv->index, __func__);
		return -1;
	}

	clk_rec[0] = ((aux_data[0] >> BIT_DPCD_LANE_02_STATUS_CLK_REC_DONE) & 0x01);
	clk_rec[1] = ((aux_data[0] >> BIT_DPCD_LANE_13_STATUS_CLK_REC_DONE) & 0x01);
	clk_rec[2] = ((aux_data[1] >> BIT_DPCD_LANE_02_STATUS_CLK_REC_DONE) & 0x01);
	clk_rec[3] = ((aux_data[1] >> BIT_DPCD_LANE_13_STATUS_CLK_REC_DONE) & 0x01);
	chan_eq[0] = ((aux_data[0] >> BIT_DPCD_LANE_02_STATUS_CHAN_EQ_DONE) & 0x01);
	chan_eq[1] = ((aux_data[0] >> BIT_DPCD_LANE_13_STATUS_CLK_REC_DONE) & 0x01);
	chan_eq[2] = ((aux_data[1] >> BIT_DPCD_LANE_02_STATUS_CHAN_EQ_DONE) & 0x01);
	chan_eq[3] = ((aux_data[1] >> BIT_DPCD_LANE_13_STATUS_CLK_REC_DONE) & 0x01);
	sym_lock[0] = ((aux_data[0] >> BIT_DPCD_LANE_02_STATUS_SYM_LOCK_DONE) & 0x01);
	sym_lock[1] = ((aux_data[0] >> BIT_DPCD_LANE_13_STATUS_SYM_LOCK_DONE) & 0x01);
	sym_lock[2] = ((aux_data[1] >> BIT_DPCD_LANE_02_STATUS_SYM_LOCK_DONE) & 0x01);
	sym_lock[3] = ((aux_data[1] >> BIT_DPCD_LANE_13_STATUS_SYM_LOCK_DONE) & 0x01);

	cr_done = clk_rec[0] + clk_rec[1] + clk_rec[2] + clk_rec[3];
	channel_eq_done = chan_eq[0] + chan_eq[1] + chan_eq[2] + chan_eq[3];
	symbol_lock_done = sym_lock[0] + sym_lock[1] + sym_lock[2] + sym_lock[3];

	lane_align_done = ((aux_data[2] >> BIT_DPCD_LANE_ALIGNMENT_DONE) & 0x01);

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s:\n"
			" - Clock Recovery: [%d, %d, %d, %d]\n"
			" - Channel EQ    : [%d, %d, %d, %d]\n"
			" - Symbol Lock   : [%d, %d, %d, %d]\n"
			" - lane_align    : %d\n", pdrv->index, __func__,
			clk_rec[0], clk_rec[1], clk_rec[2], clk_rec[3],
			chan_eq[0], chan_eq[1], chan_eq[2], chan_eq[3],
			sym_lock[0], sym_lock[1], sym_lock[2], sym_lock[3],
			lane_align_done);

	if (cr_done != lane_cnt)
		return 1;
	if (channel_eq_done == lane_cnt && symbol_lock_done == lane_cnt && lane_align_done == 1)
		return 0;

	return 2;
}

static int dptx_run_channel_equalization_loop(struct aml_lcd_drv_s *pdrv)
{
	unsigned char tps_sel;
	int i = 0, ret = 0;

	if (pdrv->config.control.edp_cfg.TPS_support & BIT(1))
		tps_sel = 4;
	else if (pdrv->config.control.edp_cfg.TPS_support & BIT(0))
		tps_sel = 3;
	else
		tps_sel = 2;

	dptx_set_training_pattern(pdrv, tps_sel);

	//channel equalization & symbol lock loop
	while (i++ < DP_TRAINING_EQ_ATTEMPTS) {
		dptx_training_phy_update_apply(pdrv);

		usleep_range(DP_CHAN_EQ_TIMEOUT, 2 * DP_CHAN_EQ_TIMEOUT); //wait for channel eq
		ret = dptx_check_channel_equalization(pdrv);
		if (ret == 0) //success
			break;
		if (ret < 0) //aux error
			break;

		//if ((pdrv->config.control.edp_cfg.curr_vswing[0] & 0x03) != VAL_DP_std_LEVEL_3)
		//!!!!! judged by 4 lane !!!!!!!
		//five times?
		dptx_training_phy_adj_req_process(pdrv);
	}

	return ret;
}

static int dptx_check_clk_recovery(struct aml_lcd_drv_s *pdrv)
{
	unsigned char cr_done[4], aux_data[2];
	unsigned char lane_cnt, cr_all_done = 0;
	int ret;

	lane_cnt = pdrv->config.control.edp_cfg.lane_count;

	ret = dptx_aux_read(pdrv, DPCD_LANE0_1_STATUS, 2, aux_data);
	if (ret) { //clear cr_done flags on failure of AUX transaction
		LCDERR("[%d]: %s: aux read failed\n", pdrv->index, __func__);
		return -1;
	}

	cr_done[0] = ((aux_data[0] >> BIT_DPCD_LANE_02_STATUS_CLK_REC_DONE) & 0x01);
	cr_done[1] = ((aux_data[0] >> BIT_DPCD_LANE_13_STATUS_CLK_REC_DONE) & 0x01);
	cr_done[2] = ((aux_data[1] >> BIT_DPCD_LANE_02_STATUS_CLK_REC_DONE) & 0x01);
	cr_done[3] = ((aux_data[1] >> BIT_DPCD_LANE_13_STATUS_CLK_REC_DONE) & 0x01);

	cr_all_done = cr_done[0] + cr_done[1] + cr_done[2] + cr_done[3];

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s: CR result: [%d, %d, %d, %d]\n", pdrv->index, __func__,
			cr_done[0], cr_done[1], cr_done[2], cr_done[3]);

	if (cr_all_done == lane_cnt)
		return 0;

	return 1;
}

static int dptx_run_clk_recovery_loop(struct aml_lcd_drv_s *pdrv)
{
	int i = 0, ret = 0;

	dptx_set_training_pattern(pdrv, 1);

	while (i++ < DP_TRAINING_CR_ATTEMPTS) {
		dptx_training_phy_update_apply(pdrv);

		mdelay(DP_CLOCK_REC_TIMEOUT); //wait for clock recovery
		ret = dptx_check_clk_recovery(pdrv);
		if (ret == 0) //success
			break;
		if (ret < 0) //aux error
			break;

		// TODO: check for max vswing level
		// if ((pdrv->config.control.edp_cfg.preset_vswing_rx[0] & 0x07) != 0x07)
		// max Vod or same  5 times?
		dptx_training_phy_adj_req_process(pdrv);
	}

	return ret;
}

int dptx_link_config_update(struct aml_lcd_drv_s *pdrv)
{
	if (pdrv->config.control.edp_cfg.link_rate_update == 0)
		return 0;

	lcd_clk_generate_parameter(pdrv);
	lcd_set_clk(pdrv);

	dptx_set_lane_config(pdrv);

	return 0;
}

static int dptx_run_training_loop(struct aml_lcd_drv_s *pdrv, unsigned int retry_cnt)
{
	unsigned int bit_rate_adaptive = 1, link_rate_retry_cnt = 0;
	unsigned int state = DP_TRAINING_CLOCK_REC;
	int ret;
	int i;

	for (i = 0; i < 4; i++)	{
		pdrv->config.control.edp_cfg.adj_req_vswing[i] = 0;
		pdrv->config.control.edp_cfg.adj_req_preem[i] = 0;
		pdrv->config.control.edp_cfg.curr_preem[i] = 0;
		pdrv->config.control.edp_cfg.curr_vswing[i] = 0;
	}

	//turn off scrambling for training
	dptx_reg_write(pdrv, EDP_TX_SCRAMBLING_DISABLE, 0x01);

	//enter training loop
	while (!(state == DP_TRAINING_SUCCESS || state == DP_TRAINING_FAILED)) {
		if (retry_cnt++ > 5) {
			state = DP_TRAINING_FAILED;
			break;
		}
		switch (state) {
		case DP_TRAINING_CLOCK_REC:
		/* *************************************
		 * the initial state preforms clock recovery.
		 * if successful, the training sequence moves on to symbol lock.
		 * if clock recovery fails, the training loop exits.
		 */
			if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
				LCDPR("[%d]: ----- CLOCK_REC @ %d -----", pdrv->index, retry_cnt);
			ret = dptx_run_clk_recovery_loop(pdrv);
			if (ret == 0) {
				state = DP_TRAINING_CHANNEL_EQ;
				break;
			} else if (ret < 0) {
				state = DP_TRAINING_FAILED;
				break;
			}
			LCDPR("[%d]: %s: CR failed\n", pdrv->index, __func__);
			if (bit_rate_adaptive && ret == 1)
				state = DP_TRAINING_ADJ_SPD_CR_FAIL;
			else
				state = DP_TRAINING_FAILED;
			break;
		case DP_TRAINING_CHANNEL_EQ:
		/* *************************************
		 * once clock recovery is complete, perform symbol lock and channel equalization.
		 * if this state fails, then we can adjust the link rate.
		 */
			if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
				LCDPR("[%d]: ----- CHANNEL_EQ @ %d -----", pdrv->index, retry_cnt);
			ret = dptx_run_channel_equalization_loop(pdrv);
			if (ret == 0) {
				state = DP_TRAINING_SUCCESS;
				break;
			}
			if (ret < 0) {
				state = DP_TRAINING_FAILED;
				break;
			}
			LCDPR("[%d]: %s: channel EQ failed\n", pdrv->index, __func__);
			if (bit_rate_adaptive && ret == 1)
				state = DP_TRAINING_ADJ_SPD_CR_FAIL_IN_EQ;
			else if (bit_rate_adaptive && ret == 2)
				state = DP_TRAINING_ADJ_SPD_EQ_FAIL_OVERTIME;
			else
				state = DP_TRAINING_FAILED;
			break;
		case DP_TRAINING_ADJ_SPD_CR_FAIL:
		case DP_TRAINING_ADJ_SPD_CR_FAIL_IN_EQ:
		case DP_TRAINING_ADJ_SPD_EQ_FAIL_OVERTIME:
		/* *************************************
		 * when allowed by the function parameter, adjust the link speed and
		 *   attempt to retrain the link starting with clock recovery.
		 * the state of the state variable should not be changed in this state
		 *   allowing a failure condition to report the proper state.
		 */
			if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
				LCDPR("[%d]: ----- ADJ_SPD @ %d -----", pdrv->index, retry_cnt);
			ret = dptx_link_rate_config_reduce(pdrv);
			dptx_link_config_update(pdrv);
			if (ret)
				link_rate_retry_cnt++;
			//dptx_phy_config_reinit(pdrv);
			if (link_rate_retry_cnt == DP_TRAINING_LINKRATE_ATTEMPTS) {
				state = DP_TRAINING_FAILED;
				break;
			}
			state = DP_TRAINING_CLOCK_REC;
		}
	}

	//training pattern off
	dptx_set_training_pattern(pdrv, 0);
	//turn on scrambling after training
	//! EDP_SCRAMBLING NOT TURN ON
	dptx_reg_write(pdrv, EDP_TX_SCRAMBLING_DISABLE, 0x01);

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		dptx_training_status_print(pdrv);
	if (state == DP_TRAINING_SUCCESS)
		return 0;

	return -1;
}

static int dptx_check_link_status(struct aml_lcd_drv_s *pdrv)
{
	unsigned int status, link_ok = 0;
	unsigned char aux_data[3];
	int ret;

	ret = dptx_aux_read(pdrv, DPCD_LANE0_1_STATUS, 3, aux_data);
		//LANE status 01/23 & LANE_ALIGN_STATUS_UPDATED
	if (ret) {
		LCDERR("[%d]: %s: edp aux read failed.....\n", pdrv->index, __func__);
		return -1;
	}

	status = (((aux_data[2] & 0x01) << 16) | (aux_data[1] << 8) | aux_data[0]);

	/* 4lane: 0x17777; 2lane: 0x10077; 1lane: 0x10007 */
	link_ok = 0x7777 >> (4 - pdrv->config.control.edp_cfg.lane_count) * 4 | 0x10000;

	if ((status & link_ok) == link_ok)
		return 0;

	LCDPR("[%d]: %s: link training failed: 0x%x\n", pdrv->index, __func__, status);
	return -1;
}

int dptx_full_link_training(struct aml_lcd_drv_s *pdrv)
{
	unsigned int training_done = 0, retry_cnt = 0;
	int ret;
	unsigned char aux_data[4];

	if (!pdrv)
		return -1;

	LCDPR("[%d]: %s\n", pdrv->index, __func__);

	while (training_done == 0) {
		if (retry_cnt > DP_FULL_LINK_TRAINING_ATTEMPTS)
			break;
		ret = dptx_run_training_loop(pdrv, retry_cnt);
		if (ret == 0) {
			ret = dptx_check_link_status(pdrv);
			if (ret == 0) {
				training_done = 1;
				break;
			}
		}
		retry_cnt++;
	}

	ret = dptx_aux_read(pdrv, DPCD_TRAINING_SCORE_LANE0, 4, aux_data);
	LCDPR("[%d]: %s: result: [0x%02x, 0x%02x, 0x%02x, 0x%02x]\n", pdrv->index, __func__,
		aux_data[0], aux_data[1], aux_data[2], aux_data[3]);

	if (training_done) {
		LCDPR("[%d]: %s: ok\n", pdrv->index, __func__);
		return 0;
	}

	LCDPR("[%d]: %s: failed\n", pdrv->index, __func__);
	return -1;
}

/* *************** fast link training ****************** */
int dptx_fast_link_training(struct aml_lcd_drv_s *pdrv)
{
	unsigned char p_data = 0;
	unsigned char aux_data[4];
	int ret = 0;

	if (!pdrv)
		return -1;

	// disable scrambling
	dptx_reg_write(pdrv, EDP_TX_SCRAMBLING_DISABLE, 0x1);

	// set training pattern 1
	dptx_reg_write(pdrv, EDP_TX_TRAINING_PATTERN_SET, 0x1);
	p_data = 0x21;
	ret = dptx_aux_write(pdrv, DPCD_TRAINING_PATTERN_SET, 1, &p_data);
	if (ret)
		LCDERR("[%d]: edp training pattern 1 failed.....\n", pdrv->index);
	usleep_range(1000, 1200);

	// set training pattern 2
	dptx_reg_write(pdrv, EDP_TX_TRAINING_PATTERN_SET, 0x2);
	p_data = 0x22;
	ret = dptx_aux_write(pdrv, DPCD_TRAINING_PATTERN_SET, 1, &p_data);
	if (ret & 0x02)
		LCDERR("[%d]: edp training pattern 2 failed.....\n", pdrv->index);
	usleep_range(1000, 1200);

	// set training pattern 3
	dptx_reg_write(pdrv, EDP_TX_TRAINING_PATTERN_SET, 0x3);
	p_data = 0x23;
	ret = dptx_aux_write(pdrv, DPCD_TRAINING_PATTERN_SET, 1, &p_data);
	if (ret)
		LCDERR("[%d]: edp training pattern 3 failed.....\n", pdrv->index);
	usleep_range(1000, 1200);

	// disable the training pattern
	p_data = 0x20;
	ret = dptx_aux_write(pdrv, DPCD_TRAINING_PATTERN_SET, 1, &p_data);
	if (ret & 0x04)
		LCDERR("[%d]: edp training pattern off failed.....\n", pdrv->index);

	dptx_reg_write(pdrv, EDP_TX_TRAINING_PATTERN_SET, 0x0);

	ret = dptx_aux_read(pdrv, DPCD_TRAINING_SCORE_LANE0, 4, aux_data);

	LCDPR("[%d]: %s result: [0x%02x, 0x%02x, 0x%02x, 0x%02x]\n", pdrv->index, __func__,
		aux_data[0], aux_data[1], aux_data[2], aux_data[3]);
	/* read fast train result */
	// return 1;

	return 0;
}

/* ******************* link training ********************* */

/* return:
 * -1: aux failed
 * 0: Requires AUX CH handshake
 * 1: Does not require AUX CH handshake
 */
static int dptx_sink_handshake_check(struct aml_lcd_drv_s *pdrv)
{
	unsigned char data;
	int ret;

	ret = dptx_aux_read(pdrv, DPCD_MAX_DOWNSPREAD, 1, &data);
	if (ret) {
		LCDPR("%s aux read DPCD_MAX_DOWNSPREAD failed\n", __func__);
		return -1;
	}
	data = (data >> 6) & 0x1;
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s: DPCD(0x%x)[6] = %d\n", pdrv->index, __func__,
			DPCD_MAX_DOWNSPREAD, data);
	return data;
}

int dptx_link_training(struct aml_lcd_drv_s *pdrv)
{
	int ret = -1;
	enum DP_training_mode_e train_mode;

	if (!pdrv)
		return -1;

	train_mode = pdrv->config.control.edp_cfg.training_mode & 0x0f;

	switch (train_mode) {
	case DP_NO_LINK_TRAINING:
		LCDPR("[%d]%s: NO link training required\n", pdrv->index, __func__);
		ret = 0;
		break;
	case DP_FAST_LINK_TRAINING:
		ret = dptx_fast_link_training(pdrv);
		break;
	case DP_FULL_LINK_TRAINING:
		ret = dptx_full_link_training(pdrv);
		break;
	case DP_LINK_TRAINING_AUTO:
	default:
		//if (pdrv->config.basic.lcd_type == LCD_DP) {
		//	ret = dptx_full_link_training(pdrv);
		//}
		ret = dptx_sink_handshake_check(pdrv);
		if (ret < 0)
			return -1;

		if (ret == 1) {
			ret = dptx_fast_link_training(pdrv);
			break;
		}
		ret = dptx_full_link_training(pdrv);
		break;
	}

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		dptx_DPCD_dump(pdrv);

	return ret;
}
