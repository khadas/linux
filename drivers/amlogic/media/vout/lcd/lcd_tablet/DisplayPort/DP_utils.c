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
	.DPCD_rev       = 0xff,
	.link_rate      = DP_LINK_RATE_HBR,
	.line_cnt       = 4,
	.enhanced_frame = 1,
	.TPS3           = 1,
	.TPS4           = 0,
	.down_spread    = 1,

	.v_active       = 2160,
	.h_active       = 3840,
	.frame_rate     = 144,
};

enum DP_link_rate_val_e DP_link_rate_to_val[8] = {
	DP_LINK_RATE_VAL_RBR,
	DP_LINK_RATE_VAL_HBR,
	DP_LINK_RATE_VAL_HBR2,
	DP_LINK_RATE_VAL_HBR3,
	DP_LINK_RATE_VAL_UBR10,
	DP_LINK_RATE_VAL_UBR13_5,
	DP_LINK_RATE_VAL_UBR20,
	DP_LINK_RATE_VAL_INVALID
};

enum DP_link_rate_e DP_val_to_link_rate(enum DP_link_rate_val_e lkr_val)
{
	if (lkr_val == DP_LINK_RATE_VAL_RBR)
		return DP_LINK_RATE_RBR;
	if (lkr_val == DP_LINK_RATE_VAL_HBR)
		return DP_LINK_RATE_HBR;
	if (lkr_val == DP_LINK_RATE_VAL_HBR2)
		return DP_LINK_RATE_HBR2;
	if (lkr_val == DP_LINK_RATE_VAL_HBR3)
		return DP_LINK_RATE_HBR3;
	if (lkr_val == DP_LINK_RATE_VAL_UBR10)
		return DP_LINK_RATE_UBR10;
	if (lkr_val == DP_LINK_RATE_VAL_UBR13_5)
		return DP_LINK_RATE_UBR13_5;
	if (lkr_val == DP_LINK_RATE_VAL_UBR20)
		return DP_LINK_RATE_UBR20;

	return DP_LINK_RATE_INVALID;
}

char *DP_link_rate_str[8] = {
	"RBR (1.64 GHz)",
	"HBR (2.7 GHz)",
	"HBR2 (5.4 GHz)",
	"HBR3 (8.1 GHz)",
	"UBR10 (10 GHz)",
	"UBR13_5 (13.5 GHz)",
	"UBR20 (20 GHz)",
	"invalid"
};

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
