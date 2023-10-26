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
#include "../../lcd_reg.h"
#include "../../lcd_common.h"

struct DP_dev_support_s source_support_T7 = {
	.link_rate      = 1UL << DP_LINK_RATE_HBR |
			  1UL << DP_LINK_RATE_RBR,
	.line_cnt       = 4,
	.enhanced_frame = 1,
	.TPS_support    = 0x01,
	.down_spread    = 1,
	.coding_support = 0x01,
	.DACP_support   = 0x4,
	.v_active   = 2160,
	.h_active   = 3840,
	.frame_rate = 144,
};

u16 DP_training_rd_interval[5] = {400, 4000, 8000, 12000, 16000};

char *edp_ver_str[5] = {"eDP 1.1", "eDP 1.2", "eDP 1.3", "eDP 1.4a", "eDP 1.4b"};

//ANACTRL phy value(0~0xf) to DP standard value(2bit)
enum DPCD_level_e dptx_vswing_phy_to_std(unsigned int phy_vswing)
{
	enum DPCD_level_e std_vswing_level;

	if (phy_vswing <= 0x3)
		std_vswing_level = VAL_DP_std_LEVEL_0;
	else if (phy_vswing <= 0x7)
		std_vswing_level = VAL_DP_std_LEVEL_1;
	else if (phy_vswing <= 0xb)
		std_vswing_level = VAL_DP_std_LEVEL_2;
	else if (phy_vswing <= 0xf)
		std_vswing_level = VAL_DP_std_LEVEL_3;
	else
		std_vswing_level = VAL_DP_std_LEVEL_0;

	return std_vswing_level;
}

enum DPCD_level_e dptx_preem_phy_to_std(unsigned int phy_preem)
{
	enum DPCD_level_e std_preem_level;

	if (phy_preem <= 0x3)
		std_preem_level = VAL_DP_std_LEVEL_0;
	else if (phy_preem <= 0x7)
		std_preem_level = VAL_DP_std_LEVEL_1;
	else if (phy_preem <= 0xb)
		std_preem_level = VAL_DP_std_LEVEL_2;
	else if (phy_preem <= 0xf)
		std_preem_level = VAL_DP_std_LEVEL_3;
	else
		std_preem_level = VAL_DP_std_LEVEL_0;

	return std_preem_level;
}

//DPCD value (2bit) to ANACTRL phy value(0~0xf)
unsigned char dptx_vswing_std_to_phy(enum DPCD_level_e tx_vswing)
{
	unsigned char value;

	switch (tx_vswing) {
	case VAL_DP_std_LEVEL_3:
		value = 0xf;
		break;
	case VAL_DP_std_LEVEL_2:
		value = 0xb;
		break;
	case VAL_DP_std_LEVEL_1:
		value = 0x7;
		break;
	case VAL_DP_std_LEVEL_0:
	default:
		value = 0x3;
		break;
	}

	return value;
}

unsigned char dptx_preem_std_to_phy(enum DPCD_level_e tx_preem)
{
	unsigned char value;

	switch (tx_preem) {
	case VAL_DP_std_LEVEL_3:
		value = 0xf;
		break;
	case VAL_DP_std_LEVEL_2:
		value = 0xb;
		break;
	case VAL_DP_std_LEVEL_1:
		value = 0x7;
		break;
	case VAL_DP_std_LEVEL_0:
	default:
		value = 0x3;
		break;
	}

	return value;
}

/* @brief: func to transform 2 bit val to DPCD TRAINING_LANEx_SET
 * @return:
 * Bits 1:0 = VOLTAGE SWING SET
 * Bit 2 = MAX_SWING_REACHED
 * Bit 4:3 = PRE-EMPHASIS_SET
 * Bit 5 = MAX_PRE-EMPHASIS_REACHED
 */
unsigned char to_DPCD_LANESET(enum DPCD_level_e vswing, enum DPCD_level_e preem)
{
	unsigned int reg_val, rx_vswing, rx_preem;

	rx_vswing = vswing | (vswing == VAL_DP_std_LEVEL_3) << 2;
	rx_preem = preem | (preem == VAL_DP_std_LEVEL_3) << 2;

	reg_val = (rx_preem << 3) | rx_vswing;

	return reg_val;
}
