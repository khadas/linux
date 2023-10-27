/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __VPQ_MODULE_CM_H__
#define __VPQ_MODULE_CM_H__

enum color_adj_mode_9_e {
	EN_CAM9_PURPLE = 0,
	EN_CAM9_RED,
	EN_CAM9_SKIN,
	EN_CAM9_YELLOW,
	EN_CAM9_YELLOW_GREEN,
	EN_CAM9_GREEN,
	EN_CAM9_BLUE_GREEN,
	EN_CAM9_CYAN,
	EN_CAM9_BLUE,
	EN_CAM9_MAX,
};

enum color_adj_mode_14_e {
	EN_CAM14_BLUE_PURPLE = 0,
	EN_CAM14_PURPLE,
	EN_CAM14_PURPLE_RED,
	EN_CAM14_RED,
	EN_CAM14_SKIN_CHEEKS,
	EN_CAM14_SKIN_HAIR_CHEEKS,
	EN_CAM14_SKIN_YELLOW,
	EN_CAM14_YELLOW,
	EN_CAM14_YELLOW_GREEN,
	EN_CAM14_GREEN,
	EN_CAM14_GREEN_CYAN,
	EN_CAM14_CYAN,
	EN_CAM14_CYAN_BLUE,
	EN_CAM14_BLUE,
	EN_CAM14_MAX,
};

enum color_adj_type_e {
	EN_COLOR_ADJ_TYPE_9 = 0,
	EN_COLOR_ADJ_TYPE_14,
	EN_COLOR_ADJ_TYPE_MAX,
};

enum color_adj_curve_e {
	EN_COLOR_ADJ_CURVE_HUE_HS = 0,
	EN_COLOR_ADJ_CURVE_HUE,
	EN_COLOR_ADJ_CURVE_SAT,
	EN_COLOR_ADJ_CURVE_LUMA,
	EN_COLOR_ADJ_CURVE_MAX,
};

struct color_adj_param_s {
	enum color_adj_curve_e curve_type;
	enum color_adj_type_e color_type;
	enum color_adj_mode_9_e mode_9;
	enum color_adj_mode_14_e mode_14;
	int value;
	int lpf_en;
};

void color_adj_by_mode(struct color_adj_param_s param, int *curve_io);

#endif //__VPQ_MODULE_CM_H__

