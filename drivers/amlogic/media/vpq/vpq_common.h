/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __VPQ_COMMON_H__
#define __VPQ_COMMON_H__

//#include <linux/amlogic/media/vpq/vpq_cmd.h>
#include <linux/amlogic/vpq_cmd.h>
#include "vpq_table_type.h"

#define SATBYY_CURVE_LENGTH          (9)
#define SATBYY_CURVE_NUM             (1)
#define LUMABYHUE_CURVE_LENGTH       (32)
#define LUMABYHUE_CURVE_NUM          (1)
#define SATBYHS_CURVE_LENGTH         (32)
#define SATBYHS_CURVE_NUM            (3)
#define HUEBYHUE_CURVE_LENGTH        (32)
#define HUEBYHUE_CURVE_NUM           (1)
#define HUEBYHY_CURVE_LENGTH         (32)
#define HUEBYHY_CURVE_NUM            (5)
#define HUEBYHS_CURVE_LENGTH         (32)
#define HUEBYHS_CURVE_NUM            (5)
#define SATBYHY_CURVE_LENGTH         (32)
#define SATBYHY_CURVE_NUM            (5)
#define CM_CUSTOM_CURVE_LENGTH       (32)

enum vpq_pict_mode_item_e {
	VPQ_ITEM_BRIGHTNESS = 0,
	VPQ_ITEM_CONTRAST,
	VPQ_ITEM_SATURATION,
	VPQ_ITEM_HUE,
	VPQ_ITEM_SHARPNESS,
	VPQ_ITEM_DYNAMIC_CONTRAST,
	VPQ_ITEM_LOCAL_CONTRAST,
	VPQ_ITEM_BLACK_STRETCH,
	VPQ_ITEM_BLUE_STRETCH,
	VPQ_ITEM_CHROMA_CORING,
	VPQ_ITEM_NR,
	VPQ_ITEM_DEBLOCK,
	VPQ_ITEM_DEMOSQUITO,
	VPQ_ITEM_MCDI,
	VPQ_ITEM_TMO,
	VPQ_ITEM_SMOOTHPLUS,
	VPQ_ITEM_COLOR_BASE,
	VPQ_ITEM_AIPQ,
	VPQ_ITEM_AISR,
	VPQ_ITEM_MAX,
};

struct vpq_pict_mode_item_data_s {
	int data[VPQ_ITEM_MAX];
};

struct vpq_gamma_tabel_s {
	unsigned int data[VPQ_GAMMA_TABLE_LEN];
};

struct vpq_tcon_gamma_table_s {
	unsigned short data[VPQ_GAMMA_TABLE_LEN];
};

enum vpq_dump_type_e {
	VPQ_DUMP_VADJ1 = 0,
	VPQ_DUMP_GAMMA,
	VPQ_DUMP_WB,
	VPQ_DUMP_DNLP,
	VPQ_DUMP_LC,
	VPQ_DUMP_CM,
	VPQ_DUMP_BLE,
	VPQ_DUMP_BLS,
	VPQ_DUMP_TMO,
	VPQ_DUMP_AIPQ,
	VPQ_DUMP_AISR,
	VPQ_DUMP_CCORING,
};

#endif //__VPQ_COMMON_H__
