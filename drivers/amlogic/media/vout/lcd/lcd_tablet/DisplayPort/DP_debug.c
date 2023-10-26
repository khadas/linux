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
#include <linux/kernel.h>
#ifdef CONFIG_AMLOGIC_VPU
#include <linux/amlogic/media/vpu/vpu.h>
#endif
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include "DP_tx.h"
#include "../../lcd_reg.h"
#include "../../lcd_common.h"

struct reg_list_s {
	unsigned char type;
	unsigned short reg;
	char *name;
};

static char *DPCD_field[3] = {"Receiver Capability", "Link Configuration", "Link/Sink Status"};

static struct reg_list_s DPCD_reg_l[]  = {
	{0, DPCD_DPCD_REV,			"DPCD_REV"},
	{0, DPCD_MAX_LINK_RATE,			"MAX_LINK_RATE"},
	{0, DPCD_MAX_LANE_COUNT,		"MAX_LANE_COUNT"},
	{0, DPCD_MAX_DOWNSPREAD,		"MAX_DOWNSPREAD"},
	{0, DPCD_NORP,				"NORP"},
	{0, DPCD_DOWNSTREAMPORT_PRESENT,	"DOWNSTREAMPORT_PRESENT"},
	{0, DPCD_MAIN_LINK_CHANNEL_CODING,	"MAIN_LINK_CHANNEL_CODING"},
	{0, DPCD_NUM_DOWNSTREAM_PORTS,		"NUM_DOWNSTREAM_PORTS"},
	{0, DPCD_RECEIVE_PORT0_CAP_0,		"RECEIVE_PORT0_CAP_0"},
	{0, DPCD_RECEIVE_PORT0_CAP_1,		"RECEIVE_PORT0_CAP_1"},
	{0, DPCD_RECEIVE_PORT1_CAP_0,		"RECEIVE_PORT1_CAP_0"},
	{0, DPCD_RECEIVE_PORT1_CAP_1,		"RECEIVE_PORT1_CAP_1"},
	{0, DPCD_I2C_SPEED_CAP,			"I2C_SPEED_CAP"},
	{0, DPCD_eDP_CONFIGURATION_CAP,		"eDP_CONFIGURATION_CAP"},
	{0, DPCD_TRAINING_AUX_RD_INTERVAL,	"TRAINING_AUX_RD_INTERVAL"},
	{0, DPCD_FAUX_CAP,			"FAUX_CAP"},
	{0, DPCD_MSTM_CAP,			"MSTM_CAP"},
	{0, DPCD_NUMBER_OF_AUDIO_ENDPOINTS,	"NUMBER_OF_AUDIO_ENDPOINTS"},
	{0, DPCD_PSR_SUPPORT,			"PSR_SUPPORT"},
	{0, DPCD_PSR_CAPS,			"PSR_CAPS"},
	{0, DPCD_DETAILED_CAP_INFO_AVAILABLE,	"DETAILED_CAP_INFO_AVAILABLE"},

	{1, DPCD_LINK_BW_SET,			"LINK_BW_SET"},
	{1, DPCD_LANE_COUNT_SET,		"LANE_COUNT_SET"},
	{1, DPCD_TRAINING_PATTERN_SET,		"TRAINING_PATTERN_SET"},
	{1, DPCD_TRAINING_LANE0_SET,		"TRAINING_LANE0_SET"},
	{1, DPCD_TRAINING_LANE1_SET,		"TRAINING_LANE1_SET"},
	{1, DPCD_TRAINING_LANE2_SET,		"TRAINING_LANE2_SET"},
	{1, DPCD_TRAINING_LANE3_SET,		"TRAINING_LANE3_SET"},
	{1, DPCD_DOWNSPREAD_CONTROL,		"DOWNSPREAD_CONTROL"},
	{1, DPCD_MAIN_LINK_CODING_SET,		"MAIN_LINK_CODING_SET"},
	{1, DPCD_I2C_SPEED_CONTROL_STATUS_BIT_MAP, "I2C_SPEED_CONTROL_STATUS_BIT_MAP"},
	{1, DPCD_eDP_CONFIGURATION_SET,		"eDP_CONFIGURATION_SET"},
	{1, DPCD_LINK_QUAL_LANE0_SET,		"LINK_QUAL_LANE0_SET"},
	{1, DPCD_LINK_QUAL_LANE1_SET,		"LINK_QUAL_LANE1_SET"},
	{1, DPCD_LINK_QUAL_LANE2_SET,		"LINK_QUAL_LANE2_SET"},
	{1, DPCD_LINK_QUAL_LANE3_SET,		"LINK_QUAL_LANE3_SET"},
	{1, DPCD_TRAINING_LANE0_1_SET2,		"TRAINING_LANE0_1_SET2"},
	{1, DPCD_TRAINING_LANE2_3_SET2,		"TRAINING_LANE2_3_SET2"},

	{2, DPCD_SINK_COUNT,			"SINK_COUNT"},
	{2, DPCD_DEVICE_SERVICE_IRQ_VECTOR,	"DEVICE_SERVICE_IRQ_VECTOR"},
	{2, DPCD_LANE0_1_STATUS,		"LANE0_1_STATUS"},
	{2, DPCD_LANE2_3_STATUS,		"LANE2_3_STATUS"},
	{2, DPCD_LANE_ALIGN__STATUS_UPDATED,	"LANE_ALIGN__STATUS_UPDATED"},
	{2, DPCD_SINK_STATUS,			"SINK_STATUS"},
	{2, DPCD_ADJUST_REQUEST_LANE0_1,	"ADJUST_REQUEST_LANE0_1"},
	{2, DPCD_ADJUST_REQUEST_LANE2_3,	"ADJUST_REQUEST_LANE2_3"},
	{2, DPCD_TRAINING_SCORE_LANE0,		"TRAINING_SCORE_LANE0"},
	{2, DPCD_TRAINING_SCORE_LANE1,		"TRAINING_SCORE_LANE1"},
	{2, DPCD_TRAINING_SCORE_LANE2,		"TRAINING_SCORE_LANE2"},
	{2, DPCD_TRAINING_SCORE_LANE3,		"TRAINING_SCORE_LANE3"},
	{2, DPCD_SYMBOL_ERROR_COUNT_LANE0,	"SYMBOL_ERROR_COUNT_LANE0"},
	{2, DPCD_SYMBOL_ERROR_COUNT_LANE0_,	"SYMBOL_ERROR_COUNT_LANE0"},
	{2, DPCD_SYMBOL_ERROR_COUNT_LANE1,	"SYMBOL_ERROR_COUNT_LANE1"},
	{2, DPCD_SYMBOL_ERROR_COUNT_LANE1_,	"SYMBOL_ERROR_COUNT_LANE1"},
	{2, DPCD_SYMBOL_ERROR_COUNT_LANE2,	"SYMBOL_ERROR_COUNT_LANE2"},
	{2, DPCD_SYMBOL_ERROR_COUNT_LANE2_,	"SYMBOL_ERROR_COUNT_LANE2"},
	{2, DPCD_SYMBOL_ERROR_COUNT_LANE3,	"SYMBOL_ERROR_COUNT_LANE3"},
	{2, DPCD_SYMBOL_ERROR_COUNT_LANE3_,	"SYMBOL_ERROR_COUNT_LANE3"},

	{0xff, 0xff, ""},
};

void dptx_DPCD_dump(struct aml_lcd_drv_s *pdrv)
{
	unsigned char p_data = 0;
	int ret, i = 0, filed = -1;

	while (DPCD_reg_l[i].type != 0xff) {
		if (filed != DPCD_reg_l[i].type) {
			pr_info("DPCD %s field:\n", DPCD_field[DPCD_reg_l[i].type]);
			filed = DPCD_reg_l[i].type;
		}
		ret = dptx_aux_read(pdrv, DPCD_reg_l[i].reg, 1, &p_data);
		if (ret) {
			pr_info(" %-35s 0x%04x: rd fail\n", DPCD_reg_l[i].name, DPCD_reg_l[i].reg);
			i++;
			continue;
		}
		pr_info(" %-35s 0x%04x: 0x%02x\n", DPCD_reg_l[i].name, DPCD_reg_l[i].reg, p_data);
		i++;
	}
}

void dptx_EDID_dump(struct aml_lcd_drv_s *pdrv)
{
	struct dptx_EDID_s edp_edid1;
	unsigned int lcd_debug_print_flag_old = lcd_debug_print_flag;

	lcd_debug_print_flag = LCD_DBG_PR_NORMAL;
	dptx_EDID_probe(pdrv, &edp_edid1);
	lcd_debug_print_flag = lcd_debug_print_flag_old;
}

static void dptx_EDID_timing_probe(struct aml_lcd_drv_s *pdrv)
{
	struct dptx_EDID_s edp_edid1;

	dptx_EDID_probe(pdrv, &edp_edid1);
	dptx_clear_timing(pdrv);
	dptx_manage_timing(pdrv, &edp_edid1);
}

static void dptx_EDID_timing_select(struct aml_lcd_drv_s *pdrv, int idx)
{
	struct dptx_detail_timing_s *tm = NULL;

	tm = dptx_get_timing(pdrv, idx);
	if (!tm) {
		LCDERR("[%d] %s %dth timing not available", pdrv->index, __func__, idx);
		return;
	}
	dptx_timing_update(pdrv, tm);
	dptx_timing_apply(pdrv);
}

static void dptx_scramble_ctrl(struct aml_lcd_drv_s *pdrv, int en)
{
	dptx_aux_write_single(pdrv, DPCD_TRAINING_PATTERN_SET, en ? 0x00 : 0x20);
	dptx_reg_write(pdrv, EDP_TX_SCRAMBLING_DISABLE, en ? 0x00 : 0x01);
	dptx_reg_write(pdrv, EDP_TX_FORCE_SCRAMBLER_RESET, 0x1);
}

int edp_debug_test(struct aml_lcd_drv_s *pdrv, char *str, int num)
{
	int ret;

	LCDPR("[%d]: %s: %s %d\n", pdrv->index, __func__, str, num);

	if (strcmp(str, "dpcd") == 0) {
		dptx_DPCD_dump(pdrv);
	} else if (strcmp(str, "edid") == 0) {
		dptx_EDID_dump(pdrv);
	} else if (strcmp(str, "fast") == 0) {
		dptx_fast_link_training(pdrv);
	} else if (strcmp(str, "full") == 0) {
		dptx_full_link_training(pdrv);
	} else if (strcmp(str, "timing") == 0) {
		if (num >= 0)
			dptx_EDID_timing_select(pdrv, num);
		else
			dptx_EDID_timing_probe(pdrv);
	} else if (strcmp(str, "main_stream") == 0) {
		if (num)
			dptx_reg_write(pdrv, EDP_TX_MAIN_STREAM_ENABLE, 0x1);
		else
			dptx_reg_write(pdrv, EDP_TX_MAIN_STREAM_ENABLE, 0x0);
	} else if (strcmp(str, "scramble") == 0) {
		dptx_scramble_ctrl(pdrv, num);
	} else if (strcmp(str, "reset") == 0) {
		if (num == 1) { //combo phy
			dptx_reset_part(pdrv, 0);
			LCDPR("reset combo dphy\n");
		} else if (num == 2) { //edp pipeline
			dptx_reset_part(pdrv, 1);
			LCDPR("reset edp pipeline\n");
		} else if (num == 3) { //edp ctrl
			dptx_reset_part(pdrv, 2);
			LCDPR("reset edp ctrl\n");
		}
	} else if (strcmp(str, "power") == 0) {
		if (num == 0) {
			ret = dptx_aux_write_single(pdrv, DPCD_SET_POWER, 0x2);
			if (ret)
				LCDERR("[%d]: edp sink power down link failed.\n", pdrv->index);
		} else {
			ret = dptx_aux_write_single(pdrv, DPCD_SET_POWER, 0x1);
			if (ret)
				LCDERR("[%d]: edp sink power up link failed.\n", pdrv->index);
		}
	} else {
		return -1;
	}
	return 0;
}
