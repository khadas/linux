/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __VPQ_MODULE_TIMING_H__
#define __VPQ_MODULE_TIMING_H__

#include "../vpq_common.h"

extern struct PQ_TABLE_PARAM pq_table_param;

enum vpq_module_sig_info_item_e {
VPQ_MODULE_SIG_INFO_SOURCE = 0,
VPQ_MODULE_SIG_INFO_HEIGHT,
VPQ_MODULE_SIG_INFO_SCAN_MODE,
VPQ_MODULE_SIG_INFO_HDR_TYPE,
VPQ_MODULE_SIG_INFO_MAX,
};

int vpq_module_timing_table_index(enum pq_index_table_index_e module_index);

#endif //__VPQ_MODULE_TIMING_H__

